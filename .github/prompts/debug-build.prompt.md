---
description: "Debug ESP-IDF build failures, CMake configuration issues, or compilation errors"
---

# 调试构建问题

## 常见构建错误排查

### MSYS 检测拦截

**现象**：`MSys/Mingw is no longer supported`

**解决**：确保 PowerShell 脚本中清空了环境变量：

```powershell
$env:MSYSTEM = ''
$env:MSYS = ''
```

### 工具链未找到

**现象**：`riscv32-esp-elf-gcc not found`

**解决**：检查 PATH 中是否包含工具链目录：

```powershell
$env:PATH = 'C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;' + $env:PATH
```

### 组件未找到

**现象**：`Could not find component 'xxx'`

**解决**：

1. 确认组件目录存在于 `components/` 下
2. 确认 `CMakeLists.txt` 中 `idf_component_register()` 格式正确
3. 运行 `idf.py fullclean` 后重新构建

### 分区表不匹配

**现象**：分区大小不匹配

**解决**：运行 `idf.py fullclean` 后重新构建

### 内存不足

**现象**：`region 'xxx' overflowed`

**解决**：

1. 检查分区表配置（`partitions.csv`）
2. 检查代码/数据大小
3. 考虑启用 PSRAM

## 构建命令

```bash
# 标准构建
cat > /tmp/build.ps1 << 'EOF'
$env:MSYSTEM = ''
$env:MSYS = ''
$env:ESP_IDF_VERSION = '5.5'
$env:IDF_PATH = 'S:\Espressif\.espressif\v5.5.5\esp-idf'
$env:ESP_ROM_ELF_DIR = 'C:\Espressif\tools\esp-rom-elfs\20241011'
$env:PATH = 'C:\Espressif\tools\python\v5.5.5\venv\Scripts;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;' + $env:PATH
& 'C:\Espressif\tools\python\v5.5.5\venv\Scripts\python.exe' 'S:\Espressif\.espressif\v5.5.5\esp-idf\tools\idf.py' build
EOF
powershell -NoProfile -ExecutionPolicy Bypass -File /tmp/build.ps1
```

## 输出格式

- 错误信息摘要
- 可能原因
- 修复步骤
- 验证结果
