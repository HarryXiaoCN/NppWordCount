// test_core.cpp
// 字数统计核心的单元测试（不依赖 Notepad++）。
// 用 build.bat 编译运行，或用 cl 手动编译：
//   cl /utf-8 /std:c++17 /EHsc /O2 tests\test_core.cpp src\WordCountCore.cpp

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "../src/WordCountCore.h"

static int g_fail = 0;

static void Check(const char *name, wc::CountResult got,
                  long long expectedWith, long long expectedWithout)
{
    if (got.withPunctuation != expectedWith ||
        got.withoutPunctuation != expectedWithout)
    {
        ++g_fail;
        std::printf("[FAIL] %-38s got=(%lld, %lld) expected=(%lld, %lld)\n",
                    name, got.withPunctuation, got.withoutPunctuation,
                    expectedWith, expectedWithout);
    }
    else
    {
        std::printf("[ OK ] %s\n", name);
    }
}

static void CheckUtf8(const char *name, const char *text,
                      long long expectedWith, long long expectedWithout)
{
    Check(name, wc::CountUtf8(text, std::strlen(text)),
          expectedWith, expectedWithout);
}

static void CheckWide(const char *name, const wchar_t *text,
                      long long expectedWith, long long expectedWithout)
{
    Check(name, wc::CountWide(text, std::wcslen(text)),
          expectedWith, expectedWithout);
}

static void CheckAnsiGbk(const char *name, const wchar_t *text,
                         long long expectedWith, long long expectedWithout)
{
    const int need = WideCharToMultiByte(936, 0, text, -1, nullptr, 0,
                                         nullptr, nullptr);
    std::string gbk(need > 0 ? static_cast<std::size_t>(need) : 0, '\0');
    if (need > 0)
    {
        WideCharToMultiByte(936, 0, text, -1, &gbk[0], need, nullptr, nullptr);
        gbk.pop_back(); // 去掉结尾 NUL
    }
    Check(name, wc::CountAnsi(gbk.data(), gbk.size(), 936),
          expectedWith, expectedWithout);
}

int main()
{
    std::printf("=== UTF-8 用例 ===\n");
    CheckUtf8("空文本", "", 0, 0);
    CheckUtf8("中文句号", "你好，世界。", 6, 4);
    CheckUtf8("英文标点", "Hello, world!", 12, 10);
    CheckUtf8("纯空白", " \t\r\n", 0, 0);
    CheckUtf8("字母数字", "abc123", 6, 6);
    CheckUtf8("中文省略号", "……", 2, 0);
    CheckUtf8("中文破折号", "——", 2, 0);
    CheckUtf8("直角引号", "「你好」", 4, 2);
    CheckUtf8("全角括号", "（一）", 3, 1);
    CheckUtf8("全角数字", "１２３", 3, 3);
    CheckUtf8("全角逗号句号", "，。", 2, 0);
    CheckUtf8("章节标题", "第1章 你好，世界", 8, 7);
    CheckUtf8("emoji", "😀😀", 2, 2);
    CheckUtf8("间隔号", "a·b", 3, 2);
    CheckUtf8("破折号夹字母", "A—B", 3, 2);
    CheckUtf8("书名号", "《书名》", 4, 2);
    CheckUtf8("中英混排", "Hello 世界", 7, 7);
    CheckUtf8("百分号", "50%", 3, 2);
    CheckUtf8("小数点", "3.14", 4, 3);
    CheckUtf8("全角空格", "　", 0, 0);
    CheckUtf8("不换行空格", "a\xC2\xA0" "b", 2, 2);
    CheckUtf8("CRLF 两行", "第一行\r\n第二行", 6, 6);
    CheckUtf8("补充平面字符", u8"\U00020000", 1, 1);
    CheckUtf8("UTF-8 BOM", u8"\uFEFF" "A", 1, 1);
    CheckUtf8("非法 UTF-8 字节", "\xFF\xFE", 0, 0);
    CheckUtf8("单省略号", "…", 1, 0);
    CheckUtf8("全角波浪号", "～", 1, 0);
    CheckUtf8("半角波浪号", "~", 1, 0);
    CheckUtf8("连字符", "a-b", 3, 2);
    CheckUtf8("下划线", "a_b", 3, 2);

    std::printf("\n=== UTF-16 用例 ===\n");
    CheckWide("宽字符中文", L"你好，世界。", 6, 4);
    CheckWide("宽字符 emoji", L"😀", 1, 1);
    CheckWide("宽字符补充平面", L"\U00020000", 1, 1);
    CheckWide("宽字符纯空白", L" \t\r\n", 0, 0);

    std::printf("\n=== ANSI(GBK/936) 用例 ===\n");
    CheckAnsiGbk("GBK 中文", L"你好，世界。", 6, 4);
    CheckAnsiGbk("GBK 章节", L"第1章 你好，世界", 8, 7);
    CheckAnsiGbk("GBK 英文", L"Hello, world!", 12, 10);

    if (g_fail == 0)
    {
        std::printf("\n全部用例通过。\n");
        return 0;
    }

    std::printf("\n有 %d 个用例失败。\n", g_fail);
    return 1;
}
