// NppPluginApi.h
// Notepad++ 插件接口的最小自包含定义（x64 / x86 通用）。
//
// 说明：
//   - 这里只声明本插件用得到的部分，避免依赖官方插件 SDK。
//   - 函数参数用 wchar_t 而不是 TCHAR，因此插件工程是否定义 UNICODE
//     都不会影响结构布局（Notepad++ 自身按 Unicode 构建）。
//   - SCNotification 只保留 nmhdr 字段：本插件只需要 nmhdr.code，
//     不依赖 Scintilla 版本之间 Sci_Position 等字段的差异，更稳妥。
#pragma once
#ifndef NPP_WORDCOUNT_PLUGIN_API_H
#define NPP_WORDCOUNT_PLUGIN_API_H

#include <windows.h>

// ---------- 插件接口结构 ----------

typedef void (__cdecl *PFUNCPLUGINCMD)();

struct ShortcutKey
{
    bool _isCtrl;
    bool _isAlt;
    bool _isShift;
    UCHAR _key;
};

struct FuncItem
{
    wchar_t _itemName[64];
    PFUNCPLUGINCMD _pFunc;
    int _cmdID;
    bool _init2Check;
    ShortcutKey *_pShKey;
};

struct NppData
{
    HWND _nppHandle;
    HWND _scintillaMainHandle;
    HWND _scintillaSecondHandle;
};

// ---------- Notepad++ 消息 ----------

// 注意：Notepad++ 的插件消息基址是 WM_USER + 1000（NPPMSG）
#define NPPMSG                    (WM_USER + 1000)

#define NPPM_GETCURRENTSCINTILLA  (NPPMSG + 4)
#define NPPM_SETSTATUSBAR         (NPPMSG + 24)
#define NPPM_GETMENUHANDLE        (NPPMSG + 25)
#define NPPPLUGINMENU             0
#define NPPMAINMENU               1

// 状态栏字段（NPPM_SETSTATUSBAR / NPPM_GETSTATUSBAR 的 wParam）
#define STATUSBAR_DOC_TYPE        0
#define STATUSBAR_DOC_SIZE        1
#define STATUSBAR_CUR_POS         2
#define STATUSBAR_EOF_FORMAT      3
#define STATUSBAR_UNICODE_TYPE    4
#define STATUSBAR_TYPING_MODE     5

// ---------- Notepad++ 通知码 ----------

#define NPPN_FIRST               1000
#define NPPN_READY               (NPPN_FIRST + 1)
#define NPPN_TBMODIFICATION      (NPPN_FIRST + 2)
#define NPPN_FILEBEFORECLOSE     (NPPN_FIRST + 3)
#define NPPN_FILEOPENED          (NPPN_FIRST + 4)
#define NPPN_FILECLOSED          (NPPN_FIRST + 5)
#define NPPN_FILEBEFOREOPEN      (NPPN_FIRST + 6)
#define NPPN_FILEBEFORESAVE      (NPPN_FIRST + 7)
#define NPPN_FILESAVED           (NPPN_FIRST + 8)
#define NPPN_SHUTDOWN            (NPPN_FIRST + 9)
#define NPPN_BUFFERACTIVATED     (NPPN_FIRST + 10)
#define NPPN_LANGCHANGED         (NPPN_FIRST + 11)
#define NPPN_WORDSTYLESUPDATED   (NPPN_FIRST + 12)
#define NPPN_SHORTCUTREMAPPED    (NPPN_FIRST + 13)
#define NPPN_FILEBEFORELOAD      (NPPN_FIRST + 14)
#define NPPN_FILELOADFAILED      (NPPN_FIRST + 15)
#define NPPN_READONLYCHANGED     (NPPN_FIRST + 16)
#define NPPN_GLOBALMODIFIED      (NPPN_FIRST + 30) // 8.6.4+：Replace All 不再触发 SCN_MODIFIED

// ---------- Scintilla 通知码 ----------

#define SCN_STYLENEEDED         2000
#define SCN_CHARADDED           2001
#define SCN_MODIFIED            2008
#define SCN_UPDATEUI            2007

// 本插件只需要 nmhdr.code（偏移固定），其余字段忽略。
typedef struct Sci_NotifyHeader
{
    void *hwndFrom;
    UINT_PTR idFrom;
    unsigned int code;
} Sci_NotifyHeader;

typedef struct SCNotification
{
    Sci_NotifyHeader nmhdr;
} SCNotification;

// ---------- Scintilla 消息 ----------

#define SCI_GETLENGTH            2006
#define SCI_GETTEXT              2182
#define SCI_SETTEXT              2181
#define SCI_APPENDTEXT           2282
#define SCI_GETCODEPAGE          2137
#define SCI_SETCODEPAGE          2037
#define SCI_GOTOPOS              2029
#define SCI_GETCHARACTERPOINTER  2520
#define SCI_SETSAVEPOINT         2027
#define SCI_GETMODIFY            2159
#define SC_CP_UTF8               65001

#endif // NPP_WORDCOUNT_PLUGIN_API_H
