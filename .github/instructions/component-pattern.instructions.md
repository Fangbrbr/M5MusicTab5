---
applyTo: ["components/*/CMakeLists.txt", "components/*/include/**", "components/*/*.c", "components/*/*.cpp"]
description: "Component creation and structure conventions for the TAB5 Music Pad project"
---

# 组件开发规范

## 目录结构

每个组件位于 `components/<module_name>/`，结构如下：

```
components/<module_name>/
├── CMakeLists.txt          # 组件构建配置
├── include/                # 公共头文件
│   └── <module_name>.h
├── <module_name>.c         # 实现（单文件模式）
└── <module_name>_extra.c   # 实现（多文件模式，可选）
```

## 命名约定

- 模块前缀严格统一：`engine_` / `service_` / `app_` / `task_` / `app_manager`
- 公共 API 使用 snake_case
- 头文件保护宏：全大写模块名，如 `APP_MANAGER_H`

## 头文件规范

```c
#ifndef MODULE_NAME_H
#define MODULE_NAME_H

#include <stdint.h>
#include <stdbool.h>
// 其他包含

#ifdef __cplusplus
extern "C" {
#endif

// 类型定义
// 公共函数声明

#ifdef __cplusplus
}
#endif

#endif /* MODULE_NAME_H */
```

## 源文件结构（按顺序）

1. `#include` 语句
2. `static const char *TAG = "module_name"`（日志标签）
3. `#define` 宏定义
4. `static` 变量
5. `static` 函数前向声明
6. 全局函数实现
7. `static` 函数实现

## CMakeLists.txt 模板

```cmake
idf_component_register(
    SRCS "<module_name>.c"
    INCLUDE_DIRS "include"
    REQUIRES <依赖组件列表>
)
```

## 层间依赖规则

| 层级 | 允许依赖 |
|:---|:---|
| Engine | 无（纯算法/协议层） |
| Service | BSP、其他 Service |
| App | Engine、AppManager |
| Task | Service、Engine、AppManager（纯胶水调用） |

**禁止跨越层级直接调用**：App 不得直接调用 Service/BSP，Engine 不得直接调用 Service/BSP。

## 日志规范

每个 `.c` / `.cpp` 文件顶部定义：

```c
static const char *TAG = "module_name";
```

使用 `ESP_LOGI` / `ESP_LOGW` / `ESP_LOGE` / `ESP_LOGD` 输出日志。

## 错误处理

- 业务代码避免 `ESP_ERROR_CHECK`，应显式检查返回值并返回 `esp_err_t` / `bool`
- 所有可能失败的操作（内存分配、队列、文件、硬件调用）必须判断返回值
