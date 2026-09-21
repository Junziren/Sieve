param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$Version = "1.0.0",
    [string]$BuildDirectory = "build",
    [string]$DependenciesDirectory = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$artifactRoot = Join-Path $root "$BuildDirectory\Source\Sieve_artefacts\$Configuration"
$vst3Artifact = Join-Path $artifactRoot "VST3\Sieve.vst3"
$standaloneArtifact = Join-Path $artifactRoot "Standalone\Sieve.exe"
$distRoot = Join-Path $root "dist"
if ([string]::IsNullOrWhiteSpace($DependenciesDirectory)) {
    $DependenciesDirectory = Join-Path $root "dependencies"
}
$packageName = "Sieve-v$Version-windows-x64"
$packageDir = Join-Path $distRoot $packageName
$archive = Join-Path $distRoot "$packageName.zip"
$hashFile = "$archive.sha256"

# The Windows package must install on a machine with no network access, so the
# official Microsoft runtimes travel inside the ZIP.
$requiredDependencies = @(
    "MicrosoftEdgeWebView2RuntimeInstallerX64.exe",
    "vc_redist.x64.exe"
)

if (-not (Test-Path -LiteralPath $vst3Artifact)) {
    throw "VST3 artifact not found: $vst3Artifact"
}

if (-not (Test-Path -LiteralPath $standaloneArtifact)) {
    throw "Standalone artifact not found: $standaloneArtifact"
}

if (Test-Path -LiteralPath $packageDir) { Remove-Item -LiteralPath $packageDir -Recurse -Force }
if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
if (Test-Path -LiteralPath $hashFile) { Remove-Item -LiteralPath $hashFile -Force }
New-Item -ItemType Directory -Path $packageDir -Force | Out-Null

$missing = $requiredDependencies | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $DependenciesDirectory $_) -PathType Leaf)
}
if ($missing) {
    Write-Host "Fetching missing runtime dependencies..." -ForegroundColor Yellow
    & (Join-Path $root "scripts\fetch_windows_dependencies.ps1") -Destination $DependenciesDirectory
}

$dependencyManifest = @()
foreach ($name in $requiredDependencies) {
    $source = Join-Path $DependenciesDirectory $name
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required runtime dependency is missing: $source"
    }

    $signature = Get-AuthenticodeSignature -LiteralPath $source
    if ($signature.Status -ne "Valid" -or
        $signature.SignerCertificate.Subject -notmatch "O=Microsoft Corporation") {
        throw "Refusing to package $name : unexpected or invalid Microsoft signature."
    }

    $dependencyManifest += [pscustomobject]@{
        File     = $name
        Version  = (Get-Item -LiteralPath $source).VersionInfo.ProductVersion
        Size     = (Get-Item -LiteralPath $source).Length
        SHA256   = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
        Signer   = $signature.SignerCertificate.Subject
    }
}

Copy-Item -LiteralPath $vst3Artifact -Destination $packageDir -Recurse
New-Item -ItemType Directory -Path (Join-Path $packageDir "Standalone") -Force | Out-Null
Copy-Item -LiteralPath $standaloneArtifact -Destination (Join-Path $packageDir "Standalone")

$dependencyTarget = Join-Path $packageDir "Dependencies"
New-Item -ItemType Directory -Path $dependencyTarget -Force | Out-Null
foreach ($name in $requiredDependencies) {
    Copy-Item -LiteralPath (Join-Path $DependenciesDirectory $name) -Destination $dependencyTarget
}

Copy-Item -LiteralPath (Join-Path $root "Install.bat") -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $root "install.ps1") -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $root "README.md") -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $root "LICENSE.md") -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $root "THIRD_PARTY_LICENSES.md") -Destination $packageDir

$moduleInfo = Get-ChildItem -LiteralPath (Join-Path $packageDir "Sieve.vst3") -Filter "moduleinfo.json" -Recurse
if (-not $moduleInfo) { throw "moduleinfo.json is missing from the VST3 bundle" }

$dependencyManifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $dependencyTarget "dependencies.json") -Encoding UTF8

$vst3Binary = Join-Path $packageDir "Sieve.vst3\Contents\x86_64-win\Sieve.vst3"
$standaloneHash = (Get-FileHash -LiteralPath $standaloneArtifact -Algorithm SHA256).Hash.ToLowerInvariant()
$manifestLines = @(
    "Sieve $Version - Windows x64 installer package"
    "VST3 binary SHA-256:       $((Get-FileHash -LiteralPath $vst3Binary -Algorithm SHA256).Hash.ToLowerInvariant())"
    "Standalone binary SHA-256: $standaloneHash"
    ""
    "Bundled runtime dependencies:"
) + ($dependencyManifest | ForEach-Object {
    "  $($_.File)  version $($_.Version)  SHA-256 $($_.SHA256)"
})
$manifestLines | Set-Content -LiteralPath (Join-Path $packageDir "PACKAGE_MANIFEST.txt") -Encoding UTF8

Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $packageDir,
    $archive,
    [System.IO.Compression.CompressionLevel]::Optimal,
    $false)

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant()
"$hash  $(Split-Path -Leaf $archive)" | Set-Content -LiteralPath $hashFile -NoNewline
Write-Output "Package: $archive"
Write-Output "SHA-256: $hashFile"
Write-Output "Bundled dependencies:"
$dependencyManifest | ForEach-Object { Write-Output ("  {0} ({1:N0} bytes)" -f $_.File, $_.Size) }
