---
description: "Create a new component following project conventions"
---

# 创建新组件

按照项目规范创建一个新的 ESP-IDF 组件。

## 步骤

1. **确定组件类型与层级**
   - Engine（纯算法/协议层，零硬件依赖）
   - Service（直接调用 BSP/API）
   - App（业务应用，通过 MIDI 总线通信）
   - Task（胶水层，禁止包含业务逻辑）

2. **创建目录结构**
   ```
   components/<module_name>/
   ├── CMakeLists.txt
   ├── include/
   │   └── <module_name>.h
   └── <module_name>.c
   ```

3. **编写 CMakeLists.txt**
   ```cmake
   idf_component_register(
       SRCS "<module_name>.c"
       INCLUDE_DIRS "include"
       REQUIRES <依赖组件列表>
   )
   ```

4. **编写头文件**
   - 头文件保护宏：`#ifndef MODULE_NAME_H`
   - `extern "C"` 包裹（C++ 兼容）
   - 公共类型定义与函数声明

5. **编写源文件**
   - `static const char *TAG = "module_name"`
   - 按规范顺序组织代码

6. **构建验证**
   - 运行 `idf.py fullclean` + `idf.py build`
   - 确保零错误

## 注意事项

- 模块前缀严格统一：`engine_` / `service_` / `app_` / `task_`
- 遵循层间依赖规则，禁止跨越层级直接调用
- 新增组件后必须 `idf.py fullclean` 重建
