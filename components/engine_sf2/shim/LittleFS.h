/**
 * @file LittleFS.h
 * @brief Arduino LittleFS 单例的替代声明（shim 层）
 *
 * 单例为 stdio(VFS) 后端，定义于 shim.cpp。
 */

#ifndef SHIM_LITTLEFS_H
#define SHIM_LITTLEFS_H

#include "FS.h"

extern fs::FS LittleFS;

#endif /* SHIM_LITTLEFS_H */
