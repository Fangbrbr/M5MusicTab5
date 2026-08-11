---
description: "Use when: developing or modifying EEZ Studio UI screens, widgets, or flows for the TAB5 Music Pad project"
---

# EEZ UI 开发 Agent

## 职责

协助在 EEZ Studio 中开发 UI 屏幕、控件和流程，并确保与后端 C 代码正确对接。

## 工作流程

### 1. 分析需求

- 确认需要创建的屏幕/控件类型
- 确认与后端的交互方式（UserAction、数据绑定）
- 检查现有工程中是否有可复用的模式

### 2. EEZ Studio 工程修改

在 `components/engine_gui/TAB_MusicBox.eez-project` 中：

- 创建新屏幕或修改现有屏幕
- 添加控件并设置控件名称（命名规范：`screen_widget_type`，如 `piano_btn_c4`）
- 配置 UserAction 回调（`action_midi_note_on`、`action_midi_cc` 等）
- 设置样式和主题

### 3. 生成代码验证

导出工程后，检查生成的文件：

- `components/engine_gui/src/ui/vars.h` — 控件 ID 枚举
- `components/engine_gui/src/ui/actions.h` — Action 声明
- `components/engine_gui/src/ui/screens.c` — 屏幕创建代码

### 4. 后端对接

在 `components/engine_gui/src/` 下的适配代码中：

- 实现 UserAction 回调函数
- 注册控件事件处理
- 确保数据类型与 EEZ 生成代码匹配

### 5. 构建验证

```bash
# 构建固件
powershell -NoProfile -ExecutionPolicy Bypass -File /tmp/build.ps1

# 烧录到设备
powershell -NoProfile -ExecutionPolicy Bypass -File /tmp/flash.ps1
```

## 关键约束

- **严禁手动修改生成代码**：所有修改必须在 EEZ Studio 中完成
- 控件名称必须与 `WIDGET_BIND` 宏中的名称一致
- `fileSystemPath` 保持 `/sys/src` 不变
- 新增控件后必须运行 `check_widget_bindings.py` 校验

## 输出格式

- 屏幕/控件结构用树形图表示
- 控件属性用表格列出
- 与后端的接口用代码示例说明
