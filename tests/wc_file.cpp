// wc_file.cpp
// 命令行核对工具：统计一个文件的字数（仅供开发/验证使用）。
// 用法: wc_file.exe <文件路径>
// 输出: 含标点字数 / 不含标点字数 / 判断出的编码
//
// 编译:
//   cl /utf-8 /std:c++17 /EHsc /O2 /MT tests\wc_file.cpp src\WordCountCore.cpp /Fe:build\wc_file.exe

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/WordCountCore.h"

static bool LooksLikeUtf8(const std::vector<unsigned char> &data)
{
    std::size_t i = 0;
    while (i < data.size())
    {
        const unsigned char b = data[i];
        if (b < 0x80)
        {
            ++i;
            continue;
        }
        int n = 0;
        if ((b & 0xE0) == 0xC0)
            n = 2;
        else if ((b & 0xF0) == 0xE0)
            n = 3;
        else if ((b & 0xF8) == 0xF0)
            n = 4;
        else
            return false;

        if (i + static_cast<std::size_t>(n) > data.size())
            return false;
        for (int k = 1; k < n; ++k)
        {
            if ((data[i + k] & 0xC0) != 0x80)
                return false;
        }
        i += static_cast<std::size_t>(n);
    }
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::printf("usage: wc_file.exe <file>\n");
        return 2;
    }

    FILE *fp = nullptr;
    if (fopen_s(&fp, argv[1], "rb") != 0 || fp == nullptr)
    {
        std::printf("cannot open file: %s\n", argv[1]);
        return 2;
    }

    std::fseek(fp, 0, SEEK_END);
    const long size = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (size < 0)
    {
        std::fclose(fp);
        return 2;
    }

    std::vector<unsigned char> data(static_cast<std::size_t>(size));
    if (size > 0)
        std::fread(data.data(), 1, static_cast<std::size_t>(size), fp);
    std::fclose(fp);

    wc::CountResult result;
    const char *encoding = "UTF-8";
    const char *view = reinterpret_cast<const char *>(data.data());
    std::size_t viewLen = data.size();

    if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE)
    {
        encoding = "UTF-16 LE";
        result = wc::CountWide(
            reinterpret_cast<const wchar_t *>(data.data() + 2),
            (data.size() - 2) / 2);
    }
    else if (data.size() >= 2 && data[0] == 0xFE && data[1] == 0xFF)
    {
        encoding = "UTF-16 BE";
        std::wstring wide;
        wide.reserve((data.size() - 2) / 2);
        for (std::size_t i = 2; i + 1 < data.size(); i += 2)
        {
            const wchar_t w = static_cast<wchar_t>(
                (static_cast<unsigned int>(data[i]) << 8) | data[i + 1]);
            wide.push_back(w);
        }
        result = wc::CountWide(wide.data(), wide.size());
    }
    else
    {
        std::size_t offset = 0;
        if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB &&
            data[2] == 0xBF)
            offset = 3; // BOM 交给统计逻辑忽略也可以，这里直接跳过
        view = reinterpret_cast<const char *>(data.data() + offset);
        viewLen = data.size() - offset;

        if (LooksLikeUtf8(data))
        {
            encoding = "UTF-8";
            result = wc::CountUtf8(view, viewLen);
        }
        else
        {
            encoding = "ANSI/GBK";
            result = wc::CountAnsi(view, viewLen, CP_ACP);
        }
    }

    std::printf("file     : %s\n", argv[1]);
    std::printf("encoding : %s\n", encoding);
    std::printf("with punct.    : %lld\n", result.withPunctuation);
    std::printf("without punct. : %lld\n", result.withoutPunctuation);
    return 0;
}
