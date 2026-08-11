# 编译环境配置与构建指南

## 1. 问题背景

本项目基于 **ESP-IDF v5.4.4** + **ESP32-P4**（RISC-V），开发环境为 **Windows**。由于以下原因，常规构建命令在 Git Bash / AI Shell 中**无法直接运行**：

| 问题 | 现象 | 根因 |
|:---|:---|:---|
| MSYS 检测拦截 | `MSys/Mingw is no longer supported` | ESP-IDF Python 脚本检测 `MSYSTEM`/`MSYS` 环境变量，拒绝在 MSYS/MinGW 中运行 |
| 环境变量缺失 | `riscv32-esp-elf-gcc not found` | Shell 会话未继承 Windows 的 IDF_PATH、PATH，缺少工具链 |
| `cmd /c` 交互模式 | 命令打开 cmd 提示符但不执行 | Git Bash 中 `cmd /c` 的传参方式与交互式 Shell 冲突 |

---

## 2. 正确构建方案（已验证）

### 2.1 方案一：PowerShell 脚本（推荐，已验证通过）

创建并执行 PowerShell 脚本，显式设置所有环境变量，绕过 MSYS 检测：

```powershell
# 保存为 build.ps1，然后在 Git Bash 中执行：powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1

$env:MSYSTEM = ''          # 必须清空，否则 ESP-IDF 脚本检测到 MSYS 后拒绝运行
$env:MSYS = ''             # 同上
$env:ESP_IDF_VERSION = '5.4'  # 避免 Kconfig 中 `option env="ESP_IDF_VERSION"` 展开为空
$env:IDF_PATH = 'S:\Espressif\frameworks\esp-idf-v5.4.4'

# PATH 顺序很重要：Python venv > ninja > cmake > 工具链
$env:PATH = 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts;' +
            'S:\Espressif\tools\ninja\1.12.1;' +
            'S:\Espressif\tools\cmake\3.30.2\bin;' +
            'S:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;' +
            $env:PATH

# 首次构建或切换分区表后需要 fullclean
# & 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' 'S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py' fullclean

# 标准构建
& 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' 'S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py' build
```

**Git Bash 中执行方式**：
```bash
# 直接调用 PowerShell 执行脚本
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1

# 或者把脚本内容通过 here-document 写到临时文件再执行
cat > /tmp/build.ps1 << 'EOF'
...脚本内容...
EOF
powershell -NoProfile -ExecutionPolicy Bypass -File /tmp/build.ps1
```

### 2.2 方案二：直接调用 ninja（增量构建，CMake 已配置后）

如果 `build/build.ninja` 已存在（即 CMake 配置已完成），可直接用 ninja 增量构建，避免重复的 CMake 检测：

```powershell
# 同样需要先清空 MSYS 变量
$env:MSYSTEM = ''
$env:MSYS = ''
$env:ESP_IDF_VERSION = '5.4'
$env:IDF_PATH = 'S:\Espressif\frameworks\esp-idf-v5.4.4'
$env:PATH = 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts;' +
            'S:\Espressif\tools\ninja\1.12.1;' +
            'S:\Espressif\tools\cmake\3.30.2\bin;' +
            'S:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;' +
            $env:PATH

# 直接 ninja 增量构建（跳过 CMake 重新配置，速度快 3~5 倍）
& 'S:\Espressif\tools\ninja\1.12.1\ninja.exe' -C 'F:\Desktop\01-ActiveProjects\TAB5_Music_Pad\build'
```

> ⚠️ **注意**：切换分区表、修改 sdkconfig、新增/删除组件后，ninja 会自动触发 CMake 重新配置。如果 CMake 配置失败，请先运行方案一的 `idf.py build`。

> ⚠️ **新增/删除组件后必须完整重建**：由于项目使用 `EXTRA_COMPONENT_DIRS` 管理自定义组件，添加或移除 `components/` 下的模块后，CMake 缓存不会自动感知，建议执行 `idf.py fullclean` 后重新构建。

---

## 提交纪律（强制）

**每次构建成功后必须立即提交 Git。**

1. 构建通过（零错误，可忽略警告除外）后，AI Agent 必须立即执行 `git add -A` 与 `git commit`，不得等待用户催促。
2. 提交信息使用中文，简洁描述本次变更与验证结果。
3. `components/engine_gui/` 下的 EEZ 工程文件、生成代码、字体/图片资源一旦变更，必须与代码一起提交，不得视为临时文件跳过。
4. 不主动执行 `git push`，仅在用户明确要求时推送。
5. 禁止 `git reset`、`git rebase`、`git checkout -f`、`git clean -fd` 等会丢失/改写历史的操作，除非用户明确确认。

---

## 3. 关键环境变量速查

| 变量 | 正确值 | 用途 |
|:---|:---|:---|
| `IDF_PATH` | `S:\Espressif\frameworks\esp-idf-v5.4.4` | ESP-IDF 框架根目录 |
| `MSYSTEM` | **必须为空字符串** | 清空以避免 MSYS 检测拦截 |
| `MSYS` | **必须为空字符串** | 同上 |
| Python exe | `S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe` | IDF 专用 Python venv |
| Ninja | `S:\Espressif\tools\ninja\1.12.1\ninja.exe` | 构建系统 |
| CMake | `S:\Espressif\tools\cmake\3.30.2\bin\cmake.exe` | 配置系统 |
| GCC | `S:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-gcc.exe` | RISC-V 编译器 |

---

## 4. 常见问题与排查

### Q1: 报错 `MSys/Mingw is no longer supported`
**原因**：`MSYSTEM` 或 `MSYS` 环境变量不为空。  
**解决**：在调用任何 ESP-IDF/Python 工具前，先执行 `$env:MSYSTEM = ''; $env:MSYS = ''`（PowerShell）或 `unset MSYSTEM MSYS`（bash，但 bash 中无法彻底清空 Windows 环境变量，因此推荐 PowerShell）。

### Q2: 报错 `Could not find idf5.4_py*_env`
**原因**：ESP-IDF 自动探测未找到 Python venv。  
**解决**：使用本指南的「方案一」，直接指定 Python 解释器完整路径。

### Q3: 报错 `riscv32-esp-elf-gcc not found`
**原因**：PATH 中缺少 RISC-V 工具链目录。  
**解决**：在 PATH 中添加 `S:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin`。

### Q4: `cmd /c` 打开交互式提示符但不执行命令
**原因**：Git Bash 的 `cmd /c` 传参方式与 Windows 命令解析冲突。  
**解决**：不要通过 `cmd /c` 调用构建，改用 PowerShell（`-File` 参数）或直接调用 `python.exe`。

### Q5: 修改 partitions.csv 后编译报错分区大小不匹配
**原因**：CMake 缓存了旧的分区表信息。  
**解决**：执行 `idf.py fullclean` 后重新构建（会清除 build/ 目录并重新生成所有配置）。

### Q6: 新增 components/ 下的模块后编译找不到组件
**原因**：CMake 缓存未感知 EXTRA_COMPONENT_DIRS 的变化。  
**解决**：执行 `idf.py fullclean` 后重新构建。

### Q7: 报错 `ESP_ROM_ELF_DIR environment variable is not defined`
**原因**：缺少 ESP_ROM ELF 目录环境变量（不影响固件功能，仅影响 GDB 调试脚本生成）。  
**解决**：可忽略（CMake Warning），或在脚本中添加 `$env:ESP_ROM_ELF_DIR = 'S:\Espressif\frameworks\esp-idf-v5.4.4\components\esp_rom\esp32p4'`。

---

## 5. Agent 构建命令模板（复制即用）

以下命令可直接在 Git Bash 中执行，**无需任何修改**（已按本项目实际路径配置）：

```bash
# 创建临时 PowerShell 构建脚本
cat > /tmp/build.ps1 << 'EOF'
$env:MSYSTEM = ''
$env:MSYS = ''
$env:ESP_IDF_VERSION = '5.4'
$env:IDF_PATH = 'S:\Espressif\frameworks\esp-idf-v5.4.4'
$env:PATH = 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts;S:\Espressif\tools\ninja\1.12.1;S:\Espressif\tools\cmake\3.30.2\bin;S:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;' + $env:PATH
& 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' 'S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py' build
EOF

# 执行构建
powershell -NoProfile -ExecutionPolicy Bypass -File /tmp/build.ps1
```

如需完整重新构建（fullclean + build）：

```bash
cat > /tmp/build.ps1 << 'EOF'
$env:MSYSTEM = ''
$env:MSYS = ''
$env:ESP_IDF_VERSION = '5.4'
$env:IDF_PATH = 'S:\Espressif\frameworks\esp-idf-v5.4.4'
$env:PATH = 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts;S:\Espressif\tools\ninja\1.12.1;S:\Espressif\tools\cmake\3.30.2\bin;S:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;' + $env:PATH
& 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' 'S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py' fullclean
& 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' 'S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py' build
EOF
powershell -NoProfile -ExecutionPolicy Bypass -File /tmp/build.ps1
```

---

## 6. 烧录固件（PowerShell）

在 `.vscode/settings.json` 中已配置默认串口 `idf.portWin` = `COM29`，因此烧录命令与构建命令环境相同，只需把 `build` 替换为 `flash`：

```bash
cat > /tmp/flash.ps1 << 'EOF'
$env:MSYSTEM = ''
$env:MSYS = ''
$env:ESP_IDF_VERSION = '5.4'
$env:IDF_PATH = 'S:\Espressif\frameworks\esp-idf-v5.4.4'
$env:PATH = 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts;S:\Espressif\tools\ninja\1.12.1;S:\Espressif\tools\cmake\3.30.2\bin;S:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;' + $env:PATH
& 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' 'S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py' -p COM29 flash
EOF
powershell -NoProfile -ExecutionPolicy Bypass -File /tmp/flash.ps1
```

如果设备未接 COM29，先用以下命令查看可用串口：

```powershell
Get-PnpDevice -Class Ports | Select-Object FriendlyName, Status
```

然后将脚本中的 `COM29` 替换为实际端口号再执行。

查看设备日志：

```bash
cat > /tmp/monitor.ps1 << 'EOF'
$env:MSYSTEM = ''
$env:MSYS = ''
$env:ESP_IDF_VERSION = '5.4'
$env:IDF_PATH = 'S:\Espressif\frameworks\esp-idf-v5.4.4'
$env:PATH = 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts;S:\Espressif\tools\ninja\1.12.1;S:\Espressif\tools\cmake\3.30.2\bin;S:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;' + $env:PATH
& 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' 'S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py' -p COM29 monitor
EOF
powershell -NoProfile -ExecutionPolicy Bypass -File /tmp/monitor.ps1
```

---

## 7. 与 AGENTS.md 的关系

- `AGENTS.md` 中「构建系统」章节已明确说明：**项目中不存在 `tools/agent_build.bat`**。
- **Agent 在执行编译验证时，应优先使用本指南的「方案一」或「方案二」。**
- 若新增/删除 `components/` 下的模块，必须先执行 `idf.py fullclean` 再构建，否则 CMake 不会识别 EXTRA_COMPONENT_DIRS 的变更。

---

**维护责任**：AI Agent / 开发者 | **最后更新**：2026-06-14
