<#
.SYNOPSIS
Upload the published HA Button firmware with ST-Link and OpenOCD (no build tools).
.EXAMPLE
.\tools\upload-firmware.ps1 -OpenOcdDir C:\Tools\OpenOCD
.EXAMPLE
.\tools\upload-firmware.ps1 -OpenOcdDir C:\Tools\OpenOCD -CheckOnly
#>
[CmdletBinding()]
param(
    [string]$OpenOcdDir,
    [ValidateRange(1, 1000)][int]$SpeedKhz = 100,
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$releaseDir = Join-Path $projectDir 'prebuilt'
$imagePath = Join-Path $releaseDir 'ha-button.bin'
$checksumPath = Join-Path $releaseDir 'SHA256SUMS.txt'
if (-not (Test-Path -LiteralPath $imagePath -PathType Leaf)) {
    throw 'Missing prebuilt/ha-button.bin. Download and extract the complete repository ZIP.'
}
$checksum = [IO.File]::ReadAllText($checksumPath).Trim()
if ($checksum -notmatch '\A([0-9a-fA-F]{64})  ha-button\.bin\z') {
    throw 'Invalid firmware checksum file.'
}
$expectedHash = $Matches[1]
if ((Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'Firmware checksum mismatch. Download the package again.'
}
$imageSize = (Get-Item -LiteralPath $imagePath).Length
if ($imageSize -lt 8 -or $imageSize -gt 196608) { throw 'Invalid nRF52810 image size.' }

if ($OpenOcdDir) {
    $openocd = Join-Path $OpenOcdDir 'bin/openocd.exe'
    $scripts = Join-Path $OpenOcdDir 'share/openocd/scripts'
    if (-not (Test-Path -LiteralPath $openocd -PathType Leaf)) { throw "OpenOCD not found: $openocd" }
    if (-not (Test-Path -LiteralPath (Join-Path $scripts 'target/nrf52.cfg'))) {
        throw 'OpenOCD installation is missing share/openocd/scripts/target/nrf52.cfg.'
    }
    $openocd = (Resolve-Path -LiteralPath $openocd).Path
    $common = @('-s', (Resolve-Path -LiteralPath $scripts).Path)
} else {
    $openocd = (Get-Command openocd.exe -CommandType Application -ErrorAction Stop).Source
    $common = @() # Use the installed OpenOCD script search path.
}
$common += @('-f', 'interface/stlink-dap.cfg', '-c', 'transport select dapdirect_swd',
    '-f', 'target/nrf52.cfg', '-c', "adapter speed $SpeedKhz",
    '-c', 'gdb_port disabled', '-c', 'tcl_port disabled', '-c', 'telnet_port disabled')
if ($CheckOnly) {
    # Parse the installed configuration without initializing the adapter.
    & $openocd @common -c shutdown
    if ($LASTEXITCODE -ne 0) { throw 'OpenOCD configuration check failed.' }
    Write-Host 'Firmware checksum and OpenOCD configuration OK. No device was accessed.'
    return
}
Write-Host 'Uploading HA Button (P0.30 LED, 8 ms). Keep SWD contacts stable until completion.'
# Relative Tcl paths avoid quoting problems when the extracted folder contains spaces.
Push-Location $releaseDir
try {
    & $openocd @common -c init -c 'reset halt' `
        -c 'set chip [mrw 0x10000100]; if {$chip != 0x52810} {error {Expected nRF52810; no flash write performed}}' `
        -c 'flash write_image erase ha-button.bin 0 bin' `
        -c 'verify_image ha-button.bin 0 bin' -c 'reset run' -c shutdown
    if ($LASTEXITCODE -ne 0) {
        throw 'Upload failed. Firmware may be incomplete; check SWD contacts and rerun. No automatic unlock or mass erase is performed.'
    }
    Write-Host 'Firmware uploaded, verified and started.'
} finally { Pop-Location }
