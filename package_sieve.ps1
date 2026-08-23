param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$Version = "1.0.0",
    [string]$BuildDirectory = "build"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$artifact = Join-Path $root "$BuildDirectory\Source\Sieve_artefacts\$Configuration\VST3\Sieve.vst3"
$distRoot = Join-Path $root "dist"
$packageName = "Sieve-v$Version-windows-x64"
$packageDir = Join-Path $distRoot $packageName
$archive = Join-Path $distRoot "$packageName.zip"

if (-not (Test-Path -LiteralPath $artifact)) {
    throw "VST3 artifact not found: $artifact"
}

if (Test-Path -LiteralPath $packageDir) { Remove-Item -LiteralPath $packageDir -Recurse -Force }
if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
New-Item -ItemType Directory -Path $packageDir -Force | Out-Null

Copy-Item -LiteralPath $artifact -Destination $packageDir -Recurse
Copy-Item -LiteralPath (Join-Path $root "README.md") -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $root "LICENSE.md") -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $root "THIRD_PARTY_LICENSES.md") -Destination $packageDir

$moduleInfo = Get-ChildItem -LiteralPath (Join-Path $packageDir "Sieve.vst3") -Filter "moduleinfo.json" -Recurse
if (-not $moduleInfo) { throw "moduleinfo.json is missing from the VST3 bundle" }

Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $archive
Write-Output "Package: $archive"
