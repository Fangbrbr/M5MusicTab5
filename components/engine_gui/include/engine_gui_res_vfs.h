/**
 * @file engine_gui_res_vfs.h
 * @brief UI 资源 VFS 透明重定向层
 *
 * 将 EEZ 工程使用的统一路径 `/sys/src/xxx` 在运行时透明重定向：
 *   - SD 卡已挂载且存在对应文件 → `/sdcard/sys/src/xxx`
 *   - 否则 → `/sys_int/src/xxx`（SPIFFS 内置资源）
 *
 * 这样 EEZ 生成代码可以继续硬编码 `/sys/src/xxx`，无需根据存储来源修改。
 */

#ifndef ENGINE_GUI_RES_VFS_H
#define ENGINE_GUI_RES_VFS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册 `/sys/src` 资源重定向 VFS
 * @return ESP_OK 或错误码
 */
esp_err_t engine_gui_res_vfs_register(void);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_GUI_RES_VFS_H */
