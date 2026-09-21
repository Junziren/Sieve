param(
    [string]$Destination = "",
    [switch]$Force
)

# Downloads the official Microsoft runtimes that the Windows installer needs so
# the shipped ZIP can be installed on a machine without network access.

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $root "dependencies"
}

$sources = @(
    @{
        File = "MicrosoftEdgeWebView2RuntimeInstallerX64.exe"
        Url  = "https://go.microsoft.com/fwlink/?linkid=2124701"
        Note = "Microsoft Edge WebView2 Evergreen Standalone Installer (x64)"
    },
    @{
        File = "vc_redist.x64.exe"
        Url  = "https://aka.ms/vs/17/release/vc_redist.x64.exe"
        Note = "Microsoft Visual C++ 2015-2022 Redistributable (x64)"
    }
)

function Assert-MicrosoftSignature([string]$Path) {
    $signature = Get-AuthenticodeSignature -LiteralPath $Path
    if ($signature.Status -ne "Valid") {
        throw "Invalid Authenticode signature on $(Split-Path -Leaf $Path): $($signature.Status)"
    }

    $subject = $signature.SignerCertificate.Subject
    if ($subject -notmatch "O=Microsoft Corporation") {
        throw "Unexpected signer for $(Split-Path -Leaf $Path): $subject"
    }

    return $subject
}

New-Item -ItemType Directory -Path $Destination -Force | Out-Null
$manifest = @()

foreach ($source in $sources) {
    $target = Join-Path $Destination $source.File
    $needsDownload = $Force -or -not (Test-Path -LiteralPath $target -PathType Leaf)

    if (-not $needsDownload) {
        try {
            Assert-MicrosoftSignature $target | Out-Null
        } catch {
            Write-Host "Cached $($source.File) is not usable, downloading again." -ForegroundColor Yellow
            $needsDownload = $true
        }
    }

    if ($needsDownload) {
        Write-Host "Downloading $($source.Note)..." -ForegroundColor Cyan
        $partial = "$target.download"
        if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial -Force }
        Invoke-WebRequest -Uri $source.Url -OutFile $partial -UseBasicParsing -TimeoutSec 1800
        Move-Item -LiteralPath $partial -Destination $target -Force
    }

    $signer = Assert-MicrosoftSignature $target
    $item = Get-Item -LiteralPath $target
    $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()

    $manifest += [pscustomobject]@{
        File          = $source.File
        Note          = $source.Note
        Version       = $item.VersionInfo.ProductVersion
        SizeBytes     = $item.Length
        SHA256        = $hash
        Signer        = $signer
    }

    Write-Host ("  {0}  {1:N0} bytes  {2}" -f $source.File, $item.Length, $hash)
}

$manifestPath = Join-Path $Destination "dependencies.json"
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host "Windows dependencies ready in $Destination" -ForegroundColor Green
