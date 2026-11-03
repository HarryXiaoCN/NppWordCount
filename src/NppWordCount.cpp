// NppWordCount.cpp
// Notepad++ 插件：在底部状态栏实时显示当前文档字数（含标点 / 不含标点）。
//
// 构建：见项目根目录 build.bat（MSVC x64，需 /utf-8）
// 安装：<Notepad++>\plugins\NppWordCount\NppWordCount.dll
//
// 注意：DLL 文件名必须与下面导出的插件函数名 NppWordCount 一致，
//       否则 Notepad++ 不会认为加载成功。
//
// 实现要点：
//   1) 给状态栏控件新增一个独立分段（位于“长度/行数”右侧），不覆盖
//      Notepad++ 原有的“长度/行数”字段。
//      通过 SetWindowSubclass 拦截 SB_SETPARTS / SB_SETTEXT：
//        - NPP 设置分段时，在 DOC_SIZE 之后插入一个字数分段；
//        - NPP 更新原有分段时，把索引 >= 2 的往后挪一位，避免写错位置；
//        - 调整分段后按新索引恢复原有文本，避免闪烁/错位。
//   2) 监听 SCN_MODIFIED / NPP 事件，把“重新统计”Post 到插件自己的隐藏窗口，
//      避免在 Scintilla 通知回调里重入 Scintilla，也不会和 NPP 的状态栏刷新打架。

#include <windows.h>
#include <commctrl.h>

#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

#include "NppPluginApi.h"
#include "WordCountCore.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

static NppData g_nppData = {};
static std::wstring g_statusText;
static HWND g_lastSci = nullptr;
static int g_lastLen = -1;
static bool g_dirty = true;
static bool g_shuttingDown = false;
static DWORD g_lastCalcTick = 0;

// 插件自己的隐藏窗口：把刷新排到当前消息之后，避免重入 Scintilla/状态栏刷新
static HWND g_hiddenWnd = nullptr;
static WNDPROC g_prevWndProc = nullptr;
static bool g_refreshPosted = false;
static const UINT WM_APP_REFRESH = WM_APP + 1;
static const UINT_PTR RETRY_TIMER_ID = 1;

// ---------- 状态栏新增分段 ----------

static const UINT_PTR STATUS_SUBCLASS_ID = 0x4E50575A; // 'NPWZ'
static const int kExtraPartIndex = 2; // 插在 DOC_SIZE(索引 1) 之后

static HWND g_statusBar = nullptr;
static std::vector<int> g_nppParts; // NPP 原始分段右边界
static bool g_extraInserted = false;
static int g_extraWidth = 0;
static bool g_inSetParts = false;
static bool g_bypassIndexMapping = false;
static std::wstring g_extraText;

static const wchar_t *kEmptyExtraText = L"字数: 0  |  不含标点: 0";

// 回退方案：直接覆盖 NPP 的“文档大小”字段（仅在状态栏分段创建失败时使用）
static void SetStatusTextFallback(const wchar_t *text)
{
    if (g_nppData._nppHandle == nullptr || !IsWindow(g_nppData._nppHandle))
        return;

    SendMessageW(g_nppData._nppHandle, NPPM_SETSTATUSBAR,
                 static_cast<WPARAM>(STATUSBAR_DOC_SIZE),
                 reinterpret_cast<LPARAM>(text));
}

static int MeasureExtraWidth(HWND hwnd, const wchar_t *text)
{
    HDC hdc = GetDC(hwnd);
    if (hdc == nullptr)
        return 200;

    HFONT font = reinterpret_cast<HFONT>(
        SendMessageW(hwnd, WM_GETFONT, 0, 0));
    HGDIOBJ oldFont = nullptr;
    if (font != nullptr)
        oldFont = SelectObject(hdc, font);

    SIZE size = {};
    GetTextExtentPoint32W(hdc, text, static_cast<int>(std::wcslen(text)),
                          &size);

    if (oldFont != nullptr)
        SelectObject(hdc, oldFont);
    ReleaseDC(hwnd, hdc);
    return size.cx + 24;
}

static std::vector<std::wstring> ReadAllPartTexts(HWND hStatus, int count)
{
    std::vector<std::wstring> texts;
    if (hStatus == nullptr || count <= 0)
        return texts;

    texts.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        wchar_t buffer[512] = {};
        SendMessageW(hStatus, SB_GETTEXTW, static_cast<WPARAM>(i),
                     reinterpret_cast<LPARAM>(buffer));
        texts.emplace_back(buffer);
    }
    return texts;
}

// 根据 NPP 原始分段，重新构造 [doc_type, doc_size, 字数, cur_pos, ...]
static bool ApplyExtraLayout()
{
    if (g_statusBar == nullptr || g_nppParts.size() < 2)
        return false;

    int width = g_extraWidth;
    if (width < 40)
        return false;

    // 给“Normal text file”之类的文档类型留出空间
    const int minPart0 = 110;
    if (g_nppParts[0] - width < minPart0)
        width = g_nppParts[0] - minPart0;
    if (width < 40)
        return false;

    // 记录调整前的分段文本（用于按新索引恢复）
    const int oldCount = static_cast<int>(
        SendMessageW(g_statusBar, SB_GETPARTS, 0, 0));
    const std::vector<std::wstring> oldTexts =
        ReadAllPartTexts(g_statusBar, oldCount);
    const bool wasInserted =
        (oldCount == static_cast<int>(g_nppParts.size()) + 1);

    std::vector<int> parts;
    parts.reserve(g_nppParts.size() + 1);
    parts.push_back(g_nppParts[0] - width);
    parts.push_back(g_nppParts[1] - width);
    parts.push_back(g_nppParts[1]);
    for (std::size_t i = 2; i < g_nppParts.size(); ++i)
        parts.push_back(g_nppParts[i]);

    g_inSetParts = true;
    SendMessageW(g_statusBar, SB_SETPARTS,
                 static_cast<WPARAM>(parts.size()),
                 reinterpret_cast<LPARAM>(parts.data()));
    g_inSetParts = false;

    g_bypassIndexMapping = true;
    for (std::size_t i = 0; i < oldTexts.size(); ++i)
    {
        int target = 0;
        if (wasInserted)
        {
            target = static_cast<int>(i); // 已经是 7 段，索引不变
        }
        else
        {
            // 6 段 -> 7 段：0、1 不动，2 以后整体后移一位
            target = (i <= 1) ? static_cast<int>(i)
                              : static_cast<int>(i) + 1;
        }
        SendMessageW(g_statusBar, SB_SETTEXTW,
                     static_cast<WPARAM>(target),
                     reinterpret_cast<LPARAM>(oldTexts[i].c_str()));
    }

    const wchar_t *extra =
        g_extraText.empty() ? kEmptyExtraText : g_extraText.c_str();
    SendMessageW(g_statusBar, SB_SETTEXTW,
                 static_cast<WPARAM>(kExtraPartIndex),
                 reinterpret_cast<LPARAM>(extra));
    g_bypassIndexMapping = false;

    g_extraInserted = true;
    return true;
}

// 更新“字数”分段的文本；返回 false 表示分段没能建立（调用方需回退）
static bool SetExtraText(const wchar_t *text)
{
    if (g_statusBar == nullptr || !IsWindow(g_statusBar))
        return false;

    g_extraText = text;

    const int measured = MeasureExtraWidth(g_statusBar, text);
    const int diff = (measured > g_extraWidth)
                         ? (measured - g_extraWidth)
                         : (g_extraWidth - measured);
    if (!g_extraInserted || diff > 8)
    {
        g_extraWidth = measured;
        if (!ApplyExtraLayout())
            return false;
    }

    g_bypassIndexMapping = true;
    SendMessageW(g_statusBar, SB_SETTEXTW,
                 static_cast<WPARAM>(kExtraPartIndex),
                 reinterpret_cast<LPARAM>(text));
    g_bypassIndexMapping = false;
    return true;
}

// 状态栏子类化：插入分段 + 映射 NPP 原有分段索引
static LRESULT CALLBACK StatusBarProc(HWND hwnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam, UINT_PTR /*uIdSubclass*/,
                                      DWORD_PTR /*dwRefData*/)
{
    switch (msg)
    {
    case SB_SETPARTS:
    {
        if (g_inSetParts)
            break; // 是我们自己发出的调整消息，直接转发

        const int count = static_cast<int>(wParam);
        const int *arr = reinterpret_cast<const int *>(lParam);
        if (count < 2 || arr == nullptr)
            break;

        // 记录 NPP 的原始分段；之后 NPP 仍会按这些索引发 SB_SETTEXT
        g_nppParts.assign(arr, arr + count);
        g_extraInserted = false;

        const wchar_t *text =
            g_extraText.empty() ? kEmptyExtraText : g_extraText.c_str();
        g_extraWidth = MeasureExtraWidth(hwnd, text);

        if (!ApplyExtraLayout())
        {
            // 窗口太窄，放不下新分段：保持 NPP 原样
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }
        return 0;
    }

    case SB_SETTEXTA:
    case SB_SETTEXTW:
    {
        if (g_bypassIndexMapping || !g_extraInserted)
            break;

        const int index = static_cast<int>(wParam & 0xFF);
        if (index >= kExtraPartIndex)
        {
            const WPARAM flags = wParam & ~static_cast<WPARAM>(0xFF);
            const WPARAM newIndex =
                static_cast<WPARAM>(index + 1) & 0xFF;
            return DefSubclassProc(hwnd, msg, newIndex | flags, lParam);
        }
        break;
    }

    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, StatusBarProc, STATUS_SUBCLASS_ID);
        if (g_statusBar == hwnd)
            g_statusBar = nullptr;
        break;

    default:
        break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static BOOL CALLBACK EnumStatusBarProc(HWND hwnd, LPARAM lp)
{
    wchar_t cls[64] = {};
    if (GetClassNameW(hwnd, cls, 64) == 0 ||
        std::wcscmp(cls, L"msctls_statusbar32") != 0)
        return TRUE;

    if (!IsWindowVisible(hwnd))
        return TRUE;

    HWND *best = reinterpret_cast<HWND *>(lp);
    if (*best == nullptr)
    {
        *best = hwnd;
        return TRUE;
    }

    RECT rc = {}, bestRc = {};
    GetWindowRect(hwnd, &rc);
    GetWindowRect(*best, &bestRc);
    if ((rc.right - rc.left) > (bestRc.right - bestRc.left))
        *best = hwnd;
    return TRUE;
}

static void EnsureStatusBar(HWND hNpp)
{
    if (g_statusBar != nullptr && IsWindow(g_statusBar))
        return;

    g_statusBar = nullptr;
    HWND hStatus = nullptr;
    EnumChildWindows(hNpp, EnumStatusBarProc,
                     reinterpret_cast<LPARAM>(&hStatus));
    if (hStatus == nullptr)
        return;

    if (!SetWindowSubclass(hStatus, StatusBarProc, STATUS_SUBCLASS_ID, 0))
        return;

    g_statusBar = hStatus;

    g_nppParts.clear();
    const int count = static_cast<int>(
        SendMessageW(hStatus, SB_GETPARTS, 0, 0));
    if (count >= 2)
    {
        std::vector<int> parts(static_cast<std::size_t>(count), 0);
        if (SendMessageW(hStatus, SB_GETPARTS,
                         static_cast<WPARAM>(count),
                         reinterpret_cast<LPARAM>(parts.data())) != 0)
            g_nppParts = parts;
    }

    g_extraInserted = false;
    g_extraWidth = 0;
}

// ---------- 统计与刷新 ----------

static void RefreshStatusBar(bool force)
{
    if (g_shuttingDown || g_nppData._nppHandle == nullptr ||
        !IsWindow(g_nppData._nppHandle))
        return;

    int which = 0;
    SendMessageW(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0,
                 reinterpret_cast<LPARAM>(&which));
    HWND hSci = (which == 1) ? g_nppData._scintillaSecondHandle
                             : g_nppData._scintillaMainHandle;
    if (hSci == nullptr || !IsWindow(hSci))
        return;

    const int len = static_cast<int>(SendMessageW(hSci, SCI_GETLENGTH, 0, 0));

    const bool needCalc =
        force || g_dirty || hSci != g_lastSci || len != g_lastLen;
    if (needCalc)
    {
        // 超大文档节流：200ms 内最多统计一次
        if (!force && len > 2 * 1024 * 1024)
        {
            const DWORD now = GetTickCount();
            if (!g_statusText.empty() && (now - g_lastCalcTick) < 200)
            {
                if (g_hiddenWnd != nullptr)
                    SetTimer(g_hiddenWnd, RETRY_TIMER_ID, 220, nullptr);
                return;
            }
        }

        wc::CountResult result;
        if (len > 0)
        {
            const int codepage = static_cast<int>(
                SendMessageW(hSci, SCI_GETCODEPAGE, 0, 0));
            const char *data = reinterpret_cast<const char *>(
                SendMessageW(hSci, SCI_GETCHARACTERPOINTER, 0, 0));
            if (data != nullptr)
            {
                if (codepage == SC_CP_UTF8)
                {
                    result =
                        wc::CountUtf8(data, static_cast<std::size_t>(len));
                }
                else
                {
                    result = wc::CountAnsi(
                        data, static_cast<std::size_t>(len),
                        static_cast<unsigned int>(codepage));
                }
            }
        }

        wchar_t buffer[128] = {};
        swprintf_s(buffer, L"字数: %lld  |  不含标点: %lld",
                   result.withPunctuation, result.withoutPunctuation);
        g_statusText = buffer;
        g_lastSci = hSci;
        g_lastLen = len;
        g_dirty = false;
        g_lastCalcTick = GetTickCount();
    }

    if (g_statusText.empty())
        return;

    EnsureStatusBar(g_nppData._nppHandle);
    const bool shown = (g_statusBar != nullptr)
                           ? SetExtraText(g_statusText.c_str())
                           : false;
    if (!shown)
        SetStatusTextFallback(g_statusText.c_str());
}

// ---------- 隐藏窗口（把刷新排到当前消息之后） ----------

static LRESULT CALLBACK HiddenWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam);

static void EnsureHiddenWindow()
{
    if (g_hiddenWnd != nullptr || g_shuttingDown)
        return;

    HWND hwnd = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, nullptr, nullptr);
    if (hwnd == nullptr)
        return;

    SetLastError(0);
    const LONG_PTR old = SetWindowLongPtrW(
        hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HiddenWndProc));
    if (old == 0 && GetLastError() != 0)
    {
        DestroyWindow(hwnd);
        return;
    }

    g_prevWndProc = reinterpret_cast<WNDPROC>(old);
    g_hiddenWnd = hwnd;
}

static LRESULT CALLBACK HiddenWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam)
{
    if (msg == WM_APP_REFRESH)
    {
        g_refreshPosted = false;
        RefreshStatusBar(false);
        return 0;
    }

    if (msg == WM_TIMER && wParam == RETRY_TIMER_ID)
    {
        KillTimer(hwnd, RETRY_TIMER_ID);
        RefreshStatusBar(false);
        return 0;
    }

    if (g_prevWndProc != nullptr)
        return CallWindowProcW(g_prevWndProc, hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void PostRefresh()
{
    if (g_shuttingDown)
        return;

    EnsureHiddenWindow();
    if (g_hiddenWnd == nullptr)
    {
        RefreshStatusBar(false);
        return;
    }

    if (!g_refreshPosted)
    {
        g_refreshPosted = true;
        if (PostMessageW(g_hiddenWnd, WM_APP_REFRESH, 0, 0) == FALSE)
        {
            g_refreshPosted = false;
            RefreshStatusBar(false);
        }
    }
}

// ---------- 插件菜单命令 ----------

static void OnRefreshCommand()
{
    g_dirty = true;
    RefreshStatusBar(true);
}

static void OnAboutCommand()
{
    const wchar_t *text =
        L"《小说字数统计》\n\n"
        L"在底部状态栏实时显示当前文档字数：\n\n"
        L"· 状态栏会新增一个字数栏（位于“长度/行数”右侧），\n"
        L"  不会覆盖 Notepad++ 原有的长度、行数等信息。\n"
        L"· 字数：不含空格、制表符和换行的字符数\n"
        L"  （标点、数字、英文字母都算）。\n"
        L"· 不含标点：在上面的基础上再排除中英文标点符号。\n\n"
        L"统计范围：当前文档全文。\n"
        L"支持编码：UTF-8 与 ANSI(GBK)。";

    MessageBoxW(g_nppData._nppHandle, text, L"小说字数统计",
                MB_OK | MB_ICONINFORMATION);
}

static FuncItem g_funcItems[] = {
    {L"刷新字数统计", OnRefreshCommand, 0, false, nullptr},
    {L"关于 / 统计规则", OnAboutCommand, 0, false, nullptr},
};

// ---------- 插件导出函数 ----------

extern "C" __declspec(dllexport) const wchar_t *getName()
{
    return L"小说字数统计";
}

extern "C" __declspec(dllexport) FuncItem *getFuncsArray(int *nbF)
{
    if (nbF != nullptr)
        *nbF = static_cast<int>(sizeof(g_funcItems) / sizeof(g_funcItems[0]));
    return g_funcItems;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode)
{
    if (notifyCode == nullptr)
        return;

    switch (notifyCode->nmhdr.code)
    {
    case NPPN_READY:
    case NPPN_BUFFERACTIVATED:
    case NPPN_FILEOPENED:
    case NPPN_FILESAVED:
    case NPPN_LANGCHANGED:
    case NPPN_FILECLOSED:
    case NPPN_GLOBALMODIFIED: // “全部替换”等操作可能不触发 SCN_MODIFIED
        g_dirty = true;
        PostRefresh();
        break;

    case NPPN_SHUTDOWN:
        g_shuttingDown = true;
        if (g_hiddenWnd != nullptr)
        {
            DestroyWindow(g_hiddenWnd);
            g_hiddenWnd = nullptr;
        }
        if (g_statusBar != nullptr)
        {
            RemoveWindowSubclass(g_statusBar, StatusBarProc,
                                 STATUS_SUBCLASS_ID);
            g_statusBar = nullptr;
        }
        break;

    case SCN_MODIFIED:
        // 内容被修改（输入、粘贴、撤销等）
        g_dirty = true;
        PostRefresh();
        break;

    case SCN_UPDATEUI:
        // 只有内容变化需要重算；光标/选区/滚动不影响字数
        if (g_dirty)
            PostRefresh();
        break;

    default:
        break;
    }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT /*Message*/,
                                                     WPARAM /*wParam*/,
                                                     LPARAM /*lParam*/)
{
    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData)
{
    g_nppData = notpadPlusData;
}

extern "C" __declspec(dllexport) BOOL isUnicode()
{
    return TRUE;
}

// 与 DLL 文件名同名的插件函数（Notepad++ 要求必须存在）
extern "C" __declspec(dllexport) void NppWordCount()
{
}
