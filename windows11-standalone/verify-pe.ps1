# Verify the generated binary format; this does NOT execute the GUI or test Win11 behavior.
param([string]$Executable)
if (-not $Executable) { $Executable = Join-Path $PSScriptRoot 'TClock11Standalone.exe' }
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "EXE not found: $Executable"
}
$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Executable).Path)
if ($bytes.Length -lt 512) { throw 'EXE too small to contain a PE header' }
if ($bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) { throw 'Missing DOS MZ header' }
$pe = [BitConverter]::ToInt32($bytes, 0x3C)
if ($pe -lt 0x40 -or $pe -gt $bytes.Length - 96) { throw 'Invalid PE offset' }
if ($bytes[$pe] -ne 0x50 -or $bytes[$pe+1] -ne 0x45 -or $bytes[$pe+2] -ne 0 -or $bytes[$pe+3] -ne 0) {
    throw 'Invalid PE signature'
}
$machine = [BitConverter]::ToUInt16($bytes, $pe + 4)
$optional = $pe + 24
$magic = [BitConverter]::ToUInt16($bytes, $optional)
$subsystem = [BitConverter]::ToUInt16($bytes, $optional + 68)
if ($machine -ne 0x8664) { throw ('Unexpected machine: 0x{0:X4} (want AMD64)' -f $machine) }
if ($magic -ne 0x20B) { throw ('Unexpected optional header: 0x{0:X4} (want PE32+)' -f $magic) }
if ($subsystem -ne 2) { throw "Unexpected subsystem: $subsystem (want WINDOWS_GUI=2)" }
Write-Host ("PASS: x64 PE32+ GUI executable; size={0} bytes" -f $bytes.Length)
