param([switch]$Pristine)
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$toolDir = Join-Path $env:USERPROFILE '.cache\ha-button'
$env:PATH = "$toolDir\venv\Scripts;$env:PATH"
$env:ZEPHYR_BASE = "$toolDir\zephyrproject\zephyr"
$env:ZEPHYR_SDK_INSTALL_DIR = "$toolDir\zephyr-sdk-0.17.4"
$env:ZEPHYR_TOOLCHAIN_VARIANT = 'zephyr'
# Zephyr does not support spaces in source/build paths. The junction keeps
# sources in the user's project while providing a short, space-free path.
$sourceDir = Join-Path $toolDir 'app'
if (-not (Test-Path -LiteralPath $sourceDir)) {
    New-Item -ItemType Junction -Path $sourceDir -Target $projectDir | Out-Null
}
if ((Get-Item -LiteralPath $sourceDir).Target -ne $projectDir) {
    throw "The build junction $sourceDir belongs to another project."
}
$buildDir = Join-Path $toolDir 'build'
$pristineMode = if ($Pristine) { 'always' } else { 'auto' }
Push-Location "$toolDir\zephyrproject"
try {
    & "$toolDir\venv\Scripts\west.exe" build -b ha21014/nrf52810 -d $buildDir -p $pristineMode $sourceDir -- '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    $outputDir = Join-Path $projectDir 'firmware'
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    foreach ($file in 'zephyr.hex', 'zephyr.bin', 'zephyr.elf', 'zephyr.map') {
        Copy-Item -LiteralPath "$buildDir\zephyr\$file" -Destination $outputDir -Force
    }
} finally { Pop-Location }
