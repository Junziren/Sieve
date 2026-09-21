param(
    [string]$RunId = "",
    [string]$Destination = "",
    [string]$Version = "1.0.0",
    [switch]$SkipVerification
)

# macOS bundles can only be produced on Apple toolchains, so "packaging for
# macOS locally" means pulling the finished archives from the Apple CI run and
# checking them here. Every archive is verified against its SHA-256 sidecar.

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $root "dist"
}
New-Item -ItemType Directory -Path $Destination -Force | Out-Null

if ([string]::IsNullOrWhiteSpace($RunId)) {
    Write-Host "Locating the latest successful Apple Release Build..." -ForegroundColor Cyan
    $runs = gh run list --workflow apple-release.yml --status success --limit 1 `
        --json databaseId,headSha,createdAt | ConvertFrom-Json
    if (-not $runs -or $runs.Count -eq 0) {
        throw "No successful Apple Release Build run was found."
    }
    $RunId = "$($runs[0].databaseId)"
    Write-Host ("  run {0}  commit {1}  {2}" -f $RunId, $runs[0].headSha.Substring(0, 7), $runs[0].createdAt)
}

$staging = Join-Path $env:TEMP ("sieve-apple-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $staging -Force | Out-Null

$architectures = @("arm64", "x86_64", "universal")
$downloaded = @()

foreach ($arch in $architectures) {
    $artifact = "Sieve-v$Version-macos-$arch"
    Write-Host "Downloading $artifact..." -ForegroundColor Cyan

    $ok = $false
    for ($attempt = 1; $attempt -le 5; $attempt++) {
        gh run download $RunId -n $artifact --dir $staging 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) { $ok = $true; break }
        Write-Host "  attempt $attempt failed, retrying..." -ForegroundColor Yellow
        Start-Sleep -Seconds 8
    }
    if (-not $ok) { throw "Could not download $artifact after 5 attempts." }

    $zip = Get-ChildItem -LiteralPath $staging -Recurse -Filter "$artifact.zip" | Select-Object -First 1
    $sidecar = Get-ChildItem -LiteralPath $staging -Recurse -Filter "$artifact.zip.sha256" | Select-Object -First 1
    if (-not $zip) { throw "$artifact.zip was not present in the downloaded artifact." }
    if (-not $sidecar) { throw "$artifact.zip.sha256 was not present in the downloaded artifact." }

    $expected = ((Get-Content -LiteralPath $sidecar.FullName) -split '\s+')[0].Trim()
    $actual = (Get-FileHash -LiteralPath $zip.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($expected -ne $actual) {
        throw "SHA-256 mismatch for $artifact.zip (expected $expected, got $actual)."
    }

    Move-Item -LiteralPath $zip.FullName -Destination (Join-Path $Destination $zip.Name) -Force
    Move-Item -LiteralPath $sidecar.FullName -Destination (Join-Path $Destination $sidecar.Name) -Force
    $downloaded += $zip.Name
    Write-Host ("  {0}  SHA-256 OK" -f $zip.Name) -ForegroundColor Green
}

if (-not $SkipVerification) {
    & (Join-Path $root "scripts\verify_apple_packages.ps1") -Version $Version -Destination $Destination
}

Write-Host ""
Write-Host "Apple packages in ${Destination}:" -ForegroundColor Green
$downloaded | ForEach-Object { Write-Host "  $_" }
