param(
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vst3Source = Join-Path $scriptDir "Sieve.vst3"
$standaloneSource = Join-Path $scriptDir "Standalone\Sieve.exe"
$dependenciesDir = Join-Path $scriptDir "Dependencies"
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

function Install-BundledDependency([string]$Path, [string[]]$Arguments, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Stop-Install "$Label is missing and the installer package does not contain it: $Path"
    }

    $signature = Get-AuthenticodeSignature -LiteralPath $Path
    if ($signature.Status -ne "Valid" -or
        $signature.SignerCertificate.Subject -notmatch "O=Microsoft Corporation") {
        Stop-Install "Refusing to run the bundled $Label installer: signature is not a valid Microsoft signature."
    }

    Write-Host "Installing $Label (offline, bundled)..." -ForegroundColor Cyan
    $process = Start-Process -FilePath $Path -ArgumentList $Arguments -Wait -PassThru
    # 0 = success, 1641 = success/reboot initiated, 3010 = success/reboot required
    if ($process.ExitCode -notin @(0, 1641, 3010)) {
        Stop-Install "The bundled $Label installer failed with exit code $($process.ExitCode)."
    }
}

function Wait-ForDetection([scriptblock]$Probe, [int]$Attempts = 12, [int]$DelayMilliseconds = 1500) {
    for ($attempt = 0; $attempt -lt $Attempts; $attempt++) {
        $value = & $Probe
        if ($value) { return $value }
        Start-Sleep -Milliseconds $DelayMilliseconds
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

$probeWebView2 = { Get-RegistryVersion $webView2Paths }
$probeVcRuntime = {
    foreach ($path in $vcRuntimePaths) {
        try {
            $value = Get-ItemProperty -LiteralPath $path -ErrorAction Stop
            if ($value.Installed -eq 1) { return $value.Version }
        } catch {
        }
    }
    return $null
}

$webView2Installer = Join-Path $dependenciesDir "MicrosoftEdgeWebView2RuntimeInstallerX64.exe"
$vcRuntimeInstaller = Join-Path $dependenciesDir "vc_redist.x64.exe"

$webView2Version = & $probeWebView2
$vcRuntimeVersion = & $probeVcRuntime

if ($DryRun) {
    $webView2Plan = if ($webView2Version) {
        "already installed ($webView2Version)"
    } elseif (Test-Path -LiteralPath $webView2Installer -PathType Leaf) {
        "would install from Dependencies\$(Split-Path -Leaf $webView2Installer)"
    } else {
        "MISSING and not bundled"
    }
    $vcRuntimePlan = if ($vcRuntimeVersion) {
        "already installed ($vcRuntimeVersion)"
    } elseif (Test-Path -LiteralPath $vcRuntimeInstaller -PathType Leaf) {
        "would install from Dependencies\$(Split-Path -Leaf $vcRuntimeInstaller)"
    } else {
        "MISSING and not bundled"
    }

    Write-Host "Sieve installer dry run." -ForegroundColor Green
    Write-Host "  WebView2 Runtime: $webView2Plan"
    Write-Host "  VC++ x64 Runtime: $vcRuntimePlan"
    Write-Host "  VST3 destination:  $vst3Destination"
    Write-Host "  Standalone target:  $standaloneDestination"
    Write-Host "Dry run complete. No files were changed." -ForegroundColor Cyan
    exit 0
}

if (-not $webView2Version) {
    Install-BundledDependency $webView2Installer @("/silent", "/install") "Microsoft Edge WebView2 Runtime"
    $webView2Version = Wait-ForDetection $probeWebView2
    if (-not $webView2Version) {
        Stop-Install "Microsoft Edge WebView2 Runtime still could not be detected after installation. Reboot and run Install.bat again."
    }
}

if (-not $vcRuntimeVersion) {
    Install-BundledDependency $vcRuntimeInstaller @("/install", "/quiet", "/norestart") "Microsoft Visual C++ 2015-2022 x64 Runtime"
    $vcRuntimeVersion = Wait-ForDetection $probeVcRuntime
    if (-not $vcRuntimeVersion) {
        Stop-Install "Microsoft Visual C++ 2015-2022 x64 Runtime still could not be detected after installation. Reboot and run Install.bat again."
    }
}

Write-Host "Sieve installer checks passed." -ForegroundColor Green
Write-Host "  WebView2 Runtime: $webView2Version"
Write-Host "  VC++ x64 Runtime: $vcRuntimeVersion"
Write-Host "  VST3 destination:  $vst3Destination"
Write-Host "  Standalone target:  $standaloneDestination"

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
