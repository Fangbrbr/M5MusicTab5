/**
 * @file FS.h
 * @brief Arduino fs::File / fs::FS 的最小替代（shim 层）
 *
 * 以 stdio(VFS) 为后端实现上游 SF2Sampler 用到的文件 API，
 * 支持普通文件读写定位与目录遍历（openNextFile）。
 * 通过 include 路径阴影替换 Arduino FS.h，vendor 文件零修改。
 *
 * Trap: SeekMode 与 File/FS 的 using 声明需暴露在全局作用域——
 * Arduino FS.h 即如此（using fs::File; using fs::FS;），上游代码
 * 大量非限定使用。
 */

#ifndef SHIM_FS_H
#define SHIM_FS_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <dirent.h>
#include "WString.h"

#define FILE_READ  "r"
#define FILE_WRITE "w"

enum SeekMode {
    SeekSet = 0,
    SeekCur = 1,
    SeekEnd = 2,
};

namespace fs {

class File {
public:
    File() = default;

    size_t read(uint8_t *buf, size_t len);
    size_t readBytes(char *buf, size_t len);
    size_t readBytes(uint8_t *buf, size_t len);
    bool seek(uint32_t pos, SeekMode mode = SeekSet);
    uint32_t position();
    uint32_t size();
    int available();
    void close();
    explicit operator bool() const;

    /* 目录遍历：FS::open 打开目录后逐个取出其中的普通文件 */
    bool isDirectory() const;
    File openNextFile();
    const char *name() const;

private:
    FILE *m_file = nullptr;
    DIR  *m_dir = nullptr;
    char  m_dir_path[192];   /* isDirectory 时保存目录路径，openNextFile 拼接用 */
    char  m_name[256];       /* openNextFile 结果或目录项名 */
    uint32_t m_size = 0;

    friend class FS;
};

class FS {
public:
    File open(const char *path, const char *mode = FILE_READ);
    File open(const String &path, const char *mode = FILE_READ) { return open(path.c_str(), mode); }
    bool exists(const char *path);
};

} // namespace fs

using fs::File;
using fs::FS;

#endif /* SHIM_FS_H */
