---
description: "Create a new Service component following project conventions"
---

# 创建新 Service

## 步骤

1. **创建目录结构**
   ```
   components/service_<name>/
   ├── CMakeLists.txt
   ├── include/
   │   └── service_<name>.h
   └── service_<name>.c
   ```

2. **编写头文件**
   ```c
   #ifndef SERVICE_<NAME>_H
   #define SERVICE_<NAME>_H

   #include <stdint.h>
   #include <stdbool.h>
   #include "esp_err.h"

   #ifdef __cplusplus
   extern "C" {
   endif

   esp_err_t service_<name>_init(void);
   esp_err_t service_<name>_process(void);
   // 其他公共 API

   #ifdef __cplusplus
   }
   #endif

   #endif /* SERVICE_<NAME>_H */
   ```

3. **编写源文件**
   ```c
   #include "service_<name>.h"
   #include "esp_log.h"

   static const char *TAG = "service_<name>";

   // 静态变量
   static bool s_initialized = false;

   esp_err_t service_<name>_init(void)
   {
       if (s_initialized) {
           return ESP_OK;
       }
       // 初始化代码
       s_initialized = true;
       return ESP_OK;
   }

   esp_err_t service_<name>_process(void)
   {
       // 每周期处理代码
       return ESP_OK;
   }
   ```

4. **编写 CMakeLists.txt**
   ```cmake
   idf_component_register(
       SRCS "service_<name>.c"
       INCLUDE_DIRS "include"
       REQUIRES <依赖组件列表>
   )
   ```

5. **集成到任务**
   在对应的 `task_*.c` 中调用 `service_<name>_process()`

6. **构建验证**
   - 运行 `idf.py fullclean` + `idf.py build`
   - 确保零错误

## 注意事项

- Service 直接调用 BSP/API
- 禁止在 Service 中写业务逻辑（业务逻辑属于 App）
- 初始化函数在 `main.c` 的 `app_main()` 中调用
- 处理函数在对应的 Task 中周期性调用
