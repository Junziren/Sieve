param(
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vst3Source = Join-Path $scriptDir "Sieve.vst3"
$standaloneSource = Join-Path $scriptDir "Standalone\Sieve.exe"
$vst3Destination = Join-Path ${env:ProgramFiles} "Common Files\VST3\Sieve.vst3"
$standaloneDirectory = Join-Path ${env:ProgramFiles} "Sieve"
$standaloneDestination = Join-Path $standaloneDirectory "Sieve.exe"

function Stop-Install([string]$Message) {
    throw $Message
}

function Get-RegistryVersion([string[]]$Paths) {
    foreach ($path in $Paths) {
        try {
            $value = Get-ItemProperty -LiteralPath $path -ErrorAction Stop
            if ($value.pv -or $value.Version) {
                return ($value.pv, $value.Version | Where-Object { $_ } | Select-Object -First 1)
            }
        } catch {
        }
    }
    return $null
}

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $DryRun -and -not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Stop-Install "Administrator privileges are required. Run Install.bat."
}

if (-not (Test-Path -LiteralPath $vst3Source -PathType Container)) {
    Stop-Install "Sieve.vst3 is missing from the installer package: $vst3Source"
}

$pluginBinary = Join-Path $vst3Source "Contents\x86_64-win\Sieve.vst3"
$moduleInfo = Get-ChildItem -LiteralPath (Join-Path $vst3Source "Contents") -Filter "moduleinfo.json" -Recurse -File
if (-not (Test-Path -LiteralPath $pluginBinary -PathType Leaf)) {
    Stop-Install "The Sieve VST3 binary is missing from the bundle."
}
if (-not $moduleInfo) {
    Stop-Install "moduleinfo.json is missing from the Sieve VST3 bundle."
}

$webView2Paths = @(
    "HKLM:\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
    "HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
    "HKCU:\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
)
$vcRuntimePaths = @(
    "HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64",
    "HKLM:\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64"
)

$webView2Version = Get-RegistryVersion $webView2Paths
if (-not $webView2Version) {
    Stop-Install "Microsoft Edge WebView2 Runtime is required. Install it, then run Install.bat again."
}

$vcRuntimeVersion = $null
foreach ($path in $vcRuntimePaths) {
    try {
        $value = Get-ItemProperty -LiteralPath $path -ErrorAction Stop
        if ($value.Installed -eq 1) {
            $vcRuntimeVersion = $value.Version
            break
        }
    } catch {
    }
}
if (-not $vcRuntimeVersion) {
    Stop-Install "Microsoft Visual C++ 2015-2022 x64 Runtime is required. Install it, then run Install.bat again."
}

Write-Host "Sieve installer checks passed." -ForegroundColor Green
Write-Host "  WebView2 Runtime: $webView2Version"
Write-Host "  VC++ x64 Runtime: $vcRuntimeVersion"
Write-Host "  VST3 destination:  $vst3Destination"
Write-Host "  Standalone target:  $standaloneDestination"

if ($DryRun) {
    Write-Host "Dry run complete. No files were changed." -ForegroundColor Cyan
    exit 0
}

if (Test-Path -LiteralPath $vst3Destination) {
    Remove-Item -LiteralPath $vst3Destination -Recurse -Force
}
New-Item -ItemType Directory -Path (Split-Path -Parent $vst3Destination) -Force | Out-Null
Copy-Item -LiteralPath $vst3Source -Destination $vst3Destination -Recurse -Force

if (Test-Path -LiteralPath $standaloneSource -PathType Leaf) {
    New-Item -ItemType Directory -Path $standaloneDirectory -Force | Out-Null
    Copy-Item -LiteralPath $standaloneSource -Destination $standaloneDestination -Force
}

if (-not (Test-Path -LiteralPath (Join-Path $vst3Destination "Contents\x86_64-win\Sieve.vst3") -PathType Leaf)) {
    Stop-Install "The VST3 copy did not verify after installation."
}
if (-not (Test-Path -LiteralPath (Join-Path $vst3Destination "Contents\Resources\moduleinfo.json") -PathType Leaf)) {
    Stop-Install "moduleinfo.json did not verify after installation."
}
if ((Test-Path -LiteralPath $standaloneSource -PathType Leaf) -and
    -not (Test-Path -LiteralPath $standaloneDestination -PathType Leaf)) {
    Stop-Install "The Standalone copy did not verify after installation."
}

Write-Host "Sieve installed successfully." -ForegroundColor Green
