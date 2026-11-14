/**
 * @file shim.cpp
 * @brief shim 层实现：fs::File / fs::FS 的 stdio(VFS) 后端与单例
 *
 * LittleFS / SD_MMC 两个 Arduino 单例在此统一为 VFS 后端；
 * 实际挂载由 service_sd / service_spiffs 完成，路径经 VFS 直读。
 */

#include "LittleFS.h"
#include "SD_MMC.h"
#include <string.h>
#include <sys/stat.h>
#include "esp_heap_caps.h"

fs::FS LittleFS;
fs::FS SD_MMC;

/* SF2 加载 IO 加速：newlib 默认 stdio 缓冲为 KB 级，3MB 采样数据逐样本
 * fread 在 SPIFFS 上耗时 10s+；换 32KB PSRAM 缓冲后接近 flash 直读速度。
 * Contract: 上游 SF2Parser 只有单个 file 成员，且 engine_sf2 加载全程持
 * s_sf2_mutex 串行——任一时刻至多一个打开文件，共享缓冲无并发冲突 */
#define SHIM_IO_BUF_SIZE  (32 * 1024)
static char *s_io_buf = NULL;   /* PSRAM，首次 open 分配并常驻 */

namespace fs {

size_t File::read(uint8_t *buf, size_t len)
{
    if (m_file == nullptr) {
        return 0;
    }
    return fread(buf, 1, len, m_file);
}

size_t File::readBytes(char *buf, size_t len)
{
    return read((uint8_t *)buf, len);
}

size_t File::readBytes(uint8_t *buf, size_t len)
{
    return read(buf, len);
}

bool File::seek(uint32_t pos, SeekMode mode)
{
    if (m_file == nullptr) {
        return false;
    }
    int whence = (mode == SeekCur) ? SEEK_CUR : (mode == SeekEnd) ? SEEK_END : SEEK_SET;
    return fseek(m_file, (long)pos, whence) == 0;
}

uint32_t File::position()
{
    if (m_file == nullptr) {
        return 0;
    }
    return (uint32_t)ftell(m_file);
}

uint32_t File::size()
{
    return m_size;
}

int File::available()
{
    if (m_file == nullptr) {
        return 0;
    }
    long pos = ftell(m_file);
    return (m_size > (uint32_t)pos) ? (int)(m_size - (uint32_t)pos) : 0;
}

void File::close()
{
    if (m_file != nullptr) {
        fclose(m_file);
        m_file = nullptr;
    }
    if (m_dir != nullptr) {
        closedir(m_dir);
        m_dir = nullptr;
    }
}

File::operator bool() const
{
    return m_file != nullptr || m_dir != nullptr;
}

bool File::isDirectory() const
{
    return m_dir != nullptr;
}

File File::openNextFile()
{
    File out;
    if (m_dir == nullptr) {
        return out;
    }

    struct dirent *entry;
    while ((entry = readdir(m_dir)) != nullptr) {
        if (entry->d_type != DT_REG) {
            continue;
        }
        char full[448];
        int n = snprintf(full, sizeof(full), "%s/%s", m_dir_path, entry->d_name);
        if (n < 0 || n >= (int)sizeof(full)) {
            continue;
        }
        out = FS().open(full, FILE_READ);
        if (out) {
            strncpy(out.m_name, entry->d_name, sizeof(out.m_name) - 1);
            out.m_name[sizeof(out.m_name) - 1] = '\0';
        }
        return out;
    }
    return out;
}

const char *File::name() const
{
    return m_name;
}

File FS::open(const char *path, const char *mode)
{
    File out;
    if (path == nullptr) {
        return out;
    }

    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        out.m_dir = opendir(path);
        if (out.m_dir != nullptr) {
            strncpy(out.m_dir_path, path, sizeof(out.m_dir_path) - 1);
            out.m_dir_path[sizeof(out.m_dir_path) - 1] = '\0';
        }
        return out;
    }

    out.m_file = fopen(path, mode);
    if (out.m_file != nullptr) {
        if (s_io_buf == NULL) {
            s_io_buf = (char *)heap_caps_malloc(SHIM_IO_BUF_SIZE,
                                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
        if (s_io_buf != NULL) {
            setvbuf(out.m_file, s_io_buf, _IOFBF, SHIM_IO_BUF_SIZE);
        }
        out.m_size = (stat(path, &st) == 0) ? (uint32_t)st.st_size : 0;
        const char *base = strrchr(path, '/');
        strncpy(out.m_name, base != nullptr ? base + 1 : path, sizeof(out.m_name) - 1);
        out.m_name[sizeof(out.m_name) - 1] = '\0';
    }
    return out;
}

bool FS::exists(const char *path)
{
    if (path == nullptr) {
        return false;
    }
    struct stat st;
    return stat(path, &st) == 0;
}

} // namespace fs
