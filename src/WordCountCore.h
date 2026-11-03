// WordCountCore.h
// 纯文本字数统计核心（与 Notepad++ 无关，便于单元测试）。
#pragma once
#ifndef NPP_WORDCOUNT_CORE_H
#define NPP_WORDCOUNT_CORE_H

#include <cstddef>

namespace wc
{

struct CountResult
{
    // 含标点字数：非空白、非控制字符的字符数（标点、数字、字母、汉字都算）
    long long withPunctuation = 0;
    // 不含标点字数：在上面基础上，再排除中英文标点/符号
    long long withoutPunctuation = 0;
};

// 统计 UTF-8 文本（非法字节会被跳过，不会崩溃）
CountResult CountUtf8(const char *data, std::size_t len);

// 统计 UTF-16 文本（wchar_t，Windows 上为 16 位；正确处理代理对）
CountResult CountWide(const wchar_t *data, std::size_t len);

// 统计 ANSI / 多字节文本（codepage 传 0 时使用系统默认代码页）
CountResult CountAnsi(const char *data, std::size_t len, unsigned int codepage);

// 是否为空白（空格、制表符、换行、全角空格等；空白一律不计入字数）
bool IsWhitespace(unsigned int cp);

// 是否为标点/符号（用于“不含标点”统计）
bool IsPunctuation(unsigned int cp);

} // namespace wc

#endif // NPP_WORDCOUNT_CORE_H
