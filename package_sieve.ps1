param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$Version = "1.0.0",
    [string]$BuildDirectory = "build"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$artifactRoot = Join-Path $root "$BuildDirectory\Source\Sieve_artefacts\$Configuration"
$vst3Artifact = Join-Path $artifactRoot "VST3\Sieve.vst3"
$standaloneArtifact = Join-Path $artifactRoot "Standalone\Sieve.exe"
$distRoot = Join-Path $root "dist"
$packageName = "Sieve-v$Version-windows-x64"
$packageDir = Join-Path $distRoot $packageName
$archive = Join-Path $distRoot "$packageName.zip"
$hashFile = "$archive.sha256"

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

Copy-Item -LiteralPath $vst3Artifact -Destination $packageDir -Recurse
New-Item -ItemType Directory -Path (Join-Path $packageDir "Standalone") -Force | Out-Null
Copy-Item -LiteralPath $standaloneArtifact -Destination (Join-Path $packageDir "Standalone")
Copy-Item -LiteralPath (Join-Path $root "Install.bat") -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $root "install.ps1") -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $root "README.md") -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $root "LICENSE.md") -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $root "THIRD_PARTY_LICENSES.md") -Destination $packageDir

$moduleInfo = Get-ChildItem -LiteralPath (Join-Path $packageDir "Sieve.vst3") -Filter "moduleinfo.json" -Recurse
if (-not $moduleInfo) { throw "moduleinfo.json is missing from the VST3 bundle" }

Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $archive
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant()
"$hash  $(Split-Path -Leaf $archive)" | Set-Content -LiteralPath $hashFile -NoNewline
Write-Output "Package: $archive"
Write-Output "SHA-256: $hashFile"
