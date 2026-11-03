// WordCountCore.cpp
// 字数统计核心实现。
//
// 统计口径：
//   含标点字数  = 所有“非空白、非控制字符”的 Unicode 字符数量
//                 （汉字、英文字母、数字、标点、符号都算一个字符）
//   不含标点字数 = 在含标点字数的基础上，再排除标点/符号
//   空白（空格、制表符、换行、全角空格、不换行空格等）一律不计入。

#include "WordCountCore.h"

#include <windows.h>

#include <string>

namespace wc
{

namespace
{

// 控制字符：不计入任何字数
inline bool IsControl(unsigned int cp)
{
    return cp < 0x20 || (cp >= 0x7F && cp <= 0x9F);
}

// 把单个 Unicode 码点计入统计
inline void AddCodePoint(CountResult &result, unsigned int cp)
{
    if (IsWhitespace(cp) || IsControl(cp))
        return;

    ++result.withPunctuation;
    if (!IsPunctuation(cp))
        ++result.withoutPunctuation;
}

} // namespace

bool IsWhitespace(unsigned int cp)
{
    switch (cp)
    {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
    case 0x0B:
    case 0x0C:
    case 0x00A0: // 不换行空格
    case 0xFEFF: // BOM / 零宽不换行空格
        return true;
    default:
        break;
    }

    // Unicode 各类空格
    if (cp >= 0x2000 && cp <= 0x200A)
        return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F)
        return true;
    if (cp == 0x3000) // 全角空格
        return true;

    return false;
}

bool IsPunctuation(unsigned int cp)
{
    // ASCII 部分：字母、数字、空白、控制字符之外的可打印字符都是标点/符号
    if (cp < 0x80)
    {
        if (cp >= '0' && cp <= '9')
            return false;
        if (cp >= 'A' && cp <= 'Z')
            return false;
        if (cp >= 'a' && cp <= 'z')
            return false;
        if (cp < 0x20 || cp == 0x7F)
            return false;
        return true;
    }

    // 通用标点（引号、破折号、省略号、项目符号等）
    if (cp >= 0x2000 && cp <= 0x206F)
        return true;

    // CJK 符号和标点（、。〈〉《》「」『』【】〔〕等）
    if (cp >= 0x3000 && cp <= 0x303F)
        return true;

    // 竖排标点
    if (cp >= 0xFE10 && cp <= 0xFE19)
        return true;

    // CJK 兼容形式 / 小写变体
    if (cp >= 0xFE30 && cp <= 0xFE6F)
        return true;

    // 全角字符（FF01-FF5E 对应 ASCII 21-7E）
    if (cp >= 0xFF01 && cp <= 0xFF5E)
    {
        const unsigned int ascii = cp - 0xFEE0;
        if ((ascii >= '0' && ascii <= '9') ||
            (ascii >= 'A' && ascii <= 'Z') ||
            (ascii >= 'a' && ascii <= 'z'))
            return false;
        return true;
    }

    // 半角片假名标点
    if (cp >= 0xFF61 && cp <= 0xFF65)
        return true;

    // 全角符号（￥ ￡ ￢ 等）
    if (cp >= 0xFFE0 && cp <= 0xFFEE)
        return true;

    // 中文间隔号
    if (cp == 0x00B7)
        return true;

    // 拉丁补充区里的常见符号（¡ « » ¿ 等）
    if (cp >= 0x00A1 && cp <= 0x00BF)
    {
        if (cp == 0x00AA || cp == 0x00B5 || cp == 0x00BA) // ª µ º
            return false;
        if (cp == 0x00B2 || cp == 0x00B3 || cp == 0x00B9) // ² ³ ¹
            return false;
        if (cp == 0x00BC || cp == 0x00BD || cp == 0x00BE) // ¼ ½ ¾
            return false;
        return true;
    }

    // 乘号、除号
    if (cp == 0x00D7 || cp == 0x00F7)
        return true;

    // 片假名中点
    if (cp == 0x30FB)
        return true;

    return false;
}

CountResult CountUtf8(const char *data, std::size_t len)
{
    CountResult result;
    if (data == nullptr)
        return result;

    std::size_t i = 0;
    while (i < len)
    {
        const unsigned char b = static_cast<unsigned char>(data[i]);
        if (b < 0x80)
        {
            AddCodePoint(result, b);
            ++i;
            continue;
        }

        int n = 0;
        unsigned int cp = 0;
        if ((b & 0xE0) == 0xC0)
        {
            n = 2;
            cp = b & 0x1F;
        }
        else if ((b & 0xF0) == 0xE0)
        {
            n = 3;
            cp = b & 0x0F;
        }
        else if ((b & 0xF8) == 0xF0)
        {
            n = 4;
            cp = b & 0x07;
        }
        else
        {
            ++i; // 非法首字节
            continue;
        }

        if (i + static_cast<std::size_t>(n) > len)
        {
            ++i;
            continue;
        }

        bool ok = true;
        for (int k = 1; k < n; ++k)
        {
            const unsigned char cc = static_cast<unsigned char>(data[i + k]);
            if ((cc & 0xC0) != 0x80)
            {
                ok = false;
                break;
            }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok)
        {
            ++i;
            continue;
        }

        // 非法码点 / 超长编码 / 代理区，直接跳过
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) ||
            (n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) ||
            (n == 4 && cp < 0x10000))
        {
            i += static_cast<std::size_t>(n);
            continue;
        }

        AddCodePoint(result, cp);
        i += static_cast<std::size_t>(n);
    }

    return result;
}

CountResult CountWide(const wchar_t *data, std::size_t len)
{
    CountResult result;
    if (data == nullptr)
        return result;

    std::size_t i = 0;
    while (i < len)
    {
        const wchar_t ch = data[i];
        unsigned int cp = 0;
        std::size_t adv = 1;

        if (ch >= 0xD800 && ch <= 0xDBFF && (i + 1) < len)
        {
            const wchar_t low = data[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF)
            {
                cp = 0x10000u +
                     ((static_cast<unsigned int>(ch) - 0xD800u) << 10) +
                     (static_cast<unsigned int>(low) - 0xDC00u);
                adv = 2;
            }
            else
            {
                cp = 0xFFFD; // 孤立的高代理
            }
        }
        else if (ch >= 0xDC00 && ch <= 0xDFFF)
        {
            cp = 0xFFFD; // 孤立的低代理
        }
        else
        {
            cp = static_cast<unsigned int>(static_cast<unsigned short>(ch));
        }

        AddCodePoint(result, cp);
        i += adv;
    }

    return result;
}

CountResult CountAnsi(const char *data, std::size_t len, unsigned int codepage)
{
    CountResult result;
    if (data == nullptr || len == 0)
        return result;

    const UINT cp = (codepage != 0) ? static_cast<UINT>(codepage) : CP_ACP;

    const int need = MultiByteToWideChar(cp, 0, data, static_cast<int>(len),
                                         nullptr, 0);
    if (need > 0)
    {
        std::wstring wide(static_cast<std::size_t>(need), L'\0');
        const int written = MultiByteToWideChar(
            cp, 0, data, static_cast<int>(len), &wide[0], need);
        if (written > 0)
            return CountWide(wide.data(), static_cast<std::size_t>(written));
    }

    // 转换失败（异常代码页/超长数据）时的兜底：按单字节处理
    for (std::size_t i = 0; i < len; ++i)
        AddCodePoint(result, static_cast<unsigned char>(data[i]));
    return result;
}

} // namespace wc
