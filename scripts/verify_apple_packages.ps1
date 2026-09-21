param(
    [string]$Version = "1.0.0",
    [string]$Destination = ""
)

# Inspects the macOS release ZIPs without extracting them: Windows would turn
# the symlinks inside .app/.vst3 bundles into unusable files.

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $root "dist"
}

function Get-MachOArchitectures([byte[]]$Bytes) {
    # Match on raw bytes: PowerShell 5.1 compares large hex literals as signed
    # ints, which makes numeric magic checks unreliable.
    $magic = ($Bytes[0..3] | ForEach-Object { "{0:X2}" -f $_ }) -join ""
    $names = @{ "7" = "i386"; "16777223" = "x86_64"; "12" = "arm"; "16777228" = "arm64" }

    $fatBigEndian = @("CAFEBABE", "CAFEBABF")   # FAT_MAGIC / FAT_MAGIC_64
    $fatLittleEndian = @("BEBAFECA", "BFBAFECA")
    $thinLittleEndian = @("CFFAEDFE", "CEFAEDFE")

    $cpuName = {
        param([int64]$Cpu)
        if ($Cpu -lt 0) { $Cpu += 0x100000000 }
        if ($names.ContainsKey("$Cpu")) { return $names["$Cpu"] }
        return "cpu:$Cpu"
    }

    if ($fatBigEndian -contains $magic -or $fatLittleEndian -contains $magic) {
        $bigEndian = $fatBigEndian -contains $magic
        $count = if ($bigEndian) {
            ([uint32]$Bytes[4] -shl 24) -bor ([uint32]$Bytes[5] -shl 16) -bor ([uint32]$Bytes[6] -shl 8) -bor [uint32]$Bytes[7]
        } else {
            [BitConverter]::ToUInt32($Bytes, 4)
        }
        if ($count -gt 16) { return @("fat-entry-count-suspicious:$count") }

        $result = @()
        for ($i = 0; $i -lt $count; $i++) {
            $off = 8 + ($i * 20)
            $cpu = if ($bigEndian) {
                ([int]$Bytes[$off] -shl 24) -bor ([int]$Bytes[$off + 1] -shl 16) -bor ([int]$Bytes[$off + 2] -shl 8) -bor [int]$Bytes[$off + 3]
            } else {
                [BitConverter]::ToInt32($Bytes, $off)
            }
            $result += (& $cpuName $cpu)
        }
        return ($result | Sort-Object)
    }

    if ($thinLittleEndian -contains $magic) {
        return @((& $cpuName ([BitConverter]::ToInt32($Bytes, 4))))
    }

    return @("unknown-magic:$magic")
}

function Get-EntryBytes($Archive, [string]$EntryName) {
    $entry = $Archive.Entries | Where-Object { $_.FullName -eq $EntryName } | Select-Object -First 1
    if (-not $entry) { throw "Entry not found: $EntryName" }
    $stream = $entry.Open()
    $memory = New-Object System.IO.MemoryStream
    $stream.CopyTo($memory)
    $stream.Dispose()
    return $memory.ToArray()
}

# AUv3 only ships inside the arm64 standalone app; the other builds disable it.
$cases = @(
    @{ Arch = "arm64";     ExpectAppex = $true;  ExpectedArches = @{ "VST3" = "arm64"; "AU" = "arm64"; "Standalone" = "arm64" } },
    @{ Arch = "x86_64";    ExpectAppex = $false; ExpectedArches = @{ "VST3" = "x86_64"; "AU" = "x86_64"; "Standalone" = "x86_64" } },
    @{ Arch = "universal"; ExpectAppex = $false; ExpectedArches = @{ "VST3" = "arm64+x86_64"; "AU" = "arm64+x86_64"; "Standalone" = "arm64+x86_64" } }
)

$failed = $false

foreach ($case in $cases) {
    $arch = $case.Arch
    $zipPath = Join-Path $Destination "Sieve-v$Version-macos-$arch.zip"
    if (-not (Test-Path -LiteralPath $zipPath -PathType Leaf)) {
        throw "Missing package: $zipPath"
    }

    Write-Host ""
    Write-Host "=== $arch ===" -ForegroundColor Cyan

    $archive = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
    try {
        $prefix = "Sieve-v$Version-macos-$arch/"
        $binaries = @{
            "VST3"       = "VST3/Sieve.vst3/Contents/MacOS/Sieve"
            "AU"         = "AU/Sieve.component/Contents/MacOS/Sieve"
            "Standalone" = "Standalone/Sieve.app/Contents/MacOS/Sieve"
        }
        $appexEntry = "${prefix}Standalone/Sieve.app/Contents/PlugIns/Sieve.appex/Contents/MacOS/Sieve"

        foreach ($key in $binaries.Keys) {
            $name = $binaries[$key]
            $arches = (Get-MachOArchitectures (Get-EntryBytes $archive "$prefix$name")) -join "+"
            $expected = $case.ExpectedArches[$key]
            $mark = if ($arches -eq $expected) { "" } else { "  <-- expected $expected" }
            Write-Host ("  {0,-46} {1}{2}" -f $name, $arches, $mark)
            if ($arches -ne $expected) { $failed = $true }
        }

        $hasAppex = ($archive.Entries | Where-Object { $_.FullName -eq $appexEntry }).Count -gt 0
        if ($hasAppex) {
            $arches = (Get-MachOArchitectures (Get-EntryBytes $archive $appexEntry)) -join "+"
            Write-Host ("  {0,-46} {1}" -f "Sieve.appex", $arches)
        }
        Write-Host ("  entries: {0}" -f $archive.Entries.Count)

        if ($hasAppex -ne $case.ExpectAppex) {
            Write-Host "  FAIL: AUv3 extension presence does not match expectation" -ForegroundColor Red
            $failed = $true
        }
    } finally {
        $archive.Dispose()
    }
}

if ($failed) { exit 1 }
Write-Host ""
Write-Host "All Apple packages contain the expected bundles and architectures." -ForegroundColor Green
