$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$openocdDir = Join-Path $env:USERPROFILE '.platformio\packages\tool-openocd-esp32'
$imagePath = Join-Path $projectDir 'firmware\zephyr.bin'
if (-not (Test-Path -LiteralPath $imagePath)) { throw 'Build firmware first.' }
# Require the verified factory backup before replacing flash.
$backupDir = Join-Path $projectDir 'backups'
foreach ($name in 'original-flash.bin', 'original-uicr.bin') {
    if (-not (Test-Path -LiteralPath (Join-Path $backupDir $name))) {
        throw "Missing factory backup: $name"
    }
}
$imageSize = (Get-Item -LiteralPath $imagePath).Length
if ($imageSize -lt 8 -or $imageSize -gt 196608) { throw 'Invalid firmware size.' }
$argsCommon = @('-s', "$openocdDir\share\openocd\scripts",
    '-f', 'interface/stlink-dap.cfg', '-c', 'transport select dapdirect_swd',
    '-f', 'target/nrf52.cfg', '-c', 'adapter speed 1000',
    '-c', 'gdb_port disabled', '-c', 'tcl_port disabled', '-c', 'telnet_port disabled')
Push-Location $projectDir
try {
    & "$openocdDir\bin\openocd.exe" @argsCommon -c 'init' -c 'reset halt' `
        -c 'flash write_image erase firmware/zephyr.bin 0 bin' `
        -c "dump_image firmware/readback.bin 0 $imageSize" -c 'shutdown'
    if ($LASTEXITCODE -ne 0) { throw 'Flashing/readback failed; target has not been started.' }
    $expected = (Get-FileHash -LiteralPath $imagePath).Hash
    $actual = (Get-FileHash -LiteralPath "$projectDir\firmware\readback.bin").Hash
    if ($expected -ne $actual) { throw 'Firmware readback differs; target has not been started.' }
    Write-Host "Flash readback SHA256 OK: $actual"
    & "$openocdDir\bin\openocd.exe" @argsCommon -c 'init' -c 'reset run' -c 'shutdown'
    if ($LASTEXITCODE -ne 0) { throw 'Starting firmware failed.' }
} finally { Pop-Location }
