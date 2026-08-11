/**
 * @file SD_MMC.h
 * @brief Arduino SD_MMC 单例的替代声明（shim 层）
 *
 * 单例为 stdio(VFS) 后端，定义于 shim.cpp。实际 SD 卡路径（/sdcard）
 * 由 service_sd 挂载，经 VFS 直接可达。
 */

#ifndef SHIM_SD_MMC_H
#define SHIM_SD_MMC_H

#include "FS.h"

extern fs::FS SD_MMC;

#endif /* SHIM_SD_MMC_H */
