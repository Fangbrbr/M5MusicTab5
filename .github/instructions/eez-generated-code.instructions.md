---
applyTo: ["components/engine_gui/src/ui/**"]
description: "Rules for EEZ Studio generated code: never modify directly, always fix in EEZ project"
---

# EEZ Studio 生成代码规则

## 核心原则

`components/engine_gui/src/ui/` 下的所有文件均由 EEZ Studio 工程 `components/engine_gui/TAB_MusicBox.eez-project` 自动生成，是**单一可信源**。

## 禁止操作

**严禁手动修改以下文件**（即使是为了修复编译错误）：

- `vars.h`、`vars.c`
- `ui.h`、`ui.c`
- `screens.h`、`screens.c`
- `styles.h`、`styles.c`
- `eez-flow.h`、`eez-flow.cpp`
- `images.h`、`images.c`
- `fonts.h`、`fonts.c`
- `actions.h`、`actions.c`

## 正确修复流程

1. 发现生成代码导致编译失败、类型缺失、枚举不完整时，**立即停止编码任务**
2. 向用户报告：具体错误信息、受影响文件路径、失败原因
3. 等待用户在 EEZ Studio 侧修正工程并重新导出
4. 或取得用户**明确书面授权**后，可做最小临时 patch，但必须在 commit message 中注明授权与修改范围

## 例外

在取得用户明确授权的前提下，可做最小、可记录的临时修复。
