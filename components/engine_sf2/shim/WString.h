/**
 * @file WString.h
 * @brief Arduino String 的最小替代（shim 层）
 *
 * 仅覆盖上游 SF2Sampler 实际使用的 String API，基于 std::string 实现。
 * 通过 include 路径阴影供上游 <Arduino.h> 传递引入，vendor 文件零修改。
 */

#ifndef SHIM_WSTRING_H
#define SHIM_WSTRING_H

#include <string>
#include <cstring>
#include <cctype>

class String {
public:
    String() = default;
    String(const char *s) : m_str(s == nullptr ? "" : s) {}
    String(const char *s, size_t len) : m_str(s == nullptr ? "" : s, len) {}
    String(const std::string &s) : m_str(s) {}

    const char *c_str() const { return m_str.c_str(); }
    size_t length() const { return m_str.length(); }
    bool isEmpty() const { return m_str.empty(); }

    bool startsWith(const String &prefix) const
    {
        return m_str.compare(0, prefix.m_str.length(), prefix.m_str) == 0;
    }

    bool endsWith(const String &suffix) const
    {
        size_t n = suffix.m_str.length();
        return m_str.length() >= n && m_str.compare(m_str.length() - n, n, suffix.m_str) == 0;
    }

    void toLowerCase()
    {
        for (auto &c : m_str) {
            c = (char)tolower((unsigned char)c);
        }
    }

    String substring(size_t from) const
    {
        return from < m_str.length() ? String(m_str.substr(from)) : String();
    }

    String substring(size_t from, size_t to) const
    {
        if (from >= m_str.length()) {
            return String();
        }
        return String(m_str.substr(from, to - from));
    }

    int indexOf(char ch) const
    {
        size_t pos = m_str.find(ch);
        return pos == std::string::npos ? -1 : (int)pos;
    }

    int lastIndexOf(char ch) const
    {
        size_t pos = m_str.rfind(ch);
        return pos == std::string::npos ? -1 : (int)pos;
    }

    String operator+(const String &rhs) const { return String(m_str + rhs.m_str); }
    String &operator+=(const String &rhs) { m_str += rhs.m_str; return *this; }
    bool operator==(const String &rhs) const { return m_str == rhs.m_str; }
    bool operator!=(const String &rhs) const { return m_str != rhs.m_str; }

private:
    std::string m_str;
};

#endif /* SHIM_WSTRING_H */
