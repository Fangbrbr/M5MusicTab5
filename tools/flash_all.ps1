# flash_all.ps1
# 一键构建 + 烧录固件与 SPIFFS 资源分区
#
# 用法（在 Git Bash / PowerShell 中）：
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash_all.ps1
#
# 默认串口 COM29；如需修改，传入参数：
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash_all.ps1 -Port COM30
#
# 说明：
# - 由于 `spiffs_create_partition_image(... FLASH_IN_PROJECT)` 已注册，
#   `idf.py flash` 会自动把 `build/storage.bin` 烧录到 `storage` 分区。
# - 资源文件由 `tools/prepare_spiffs.py` 在构建时自动同步到 `spiffs_image/src/`。

param(
    [string]$Port = "COM29",
    [switch]$Monitor
)

$ErrorActionPreference = "Stop"

# 清空 MSYS 环境变量，避免 ESP-IDF Python 脚本拒绝运行
$env:MSYSTEM = ""
$env:MSYS = ""
$env:ESP_IDF_VERSION = "5.4"
$env:IDF_PATH = "S:\Espressif\frameworks\esp-idf-v5.4.4"
$env:PATH = "S:\Espressif\python_env\idf5.4_py3.11_env\Scripts;" +
            "S:\Espressif\tools\ninja\1.12.1;" +
            "S:\Espressif\tools\cmake\3.30.2\bin;" +
            "S:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;" +
            $env:PATH

$python = "S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe"
$idf_py = "S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py"
$projectDir = Split-Path -Parent $PSScriptRoot
Set-Location $projectDir

Write-Host "=== Building TAB5_Music_Pad ===" -ForegroundColor Cyan
& $python $idf_py build
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

Write-Host "=== Flashing firmware + SPIFFS resources to $Port ===" -ForegroundColor Cyan
& $python $idf_py -p $Port flash
if ($LASTEXITCODE -ne 0) { throw "Flash failed" }

if ($Monitor) {
    Write-Host "=== Opening monitor ===" -ForegroundColor Cyan
    & $python $idf_py -p $Port monitor
}
