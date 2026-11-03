# NppWordCount

Notepad++ 小说字数统计插件：在底部状态栏**新增一个独立字段**，实时显示当前文档的字数（含标点 / 不含标点）。

```
Normal text file | 长度: 3,351 行数: 65 | 字数: 1079 | 不含标点: 938 | 行: 65 列: 1 ...
```

原来的“长度/行数”等状态栏信息完全保留，不会被覆盖。

- 作者：HarryXiaoCN
- 仓库：https://github.com/HarryXiaoCN/NppWordCount
- 许可证：MIT

## 下载

- 直接下载插件 DLL：[Releases](https://github.com/HarryXiaoCN/NppWordCount/releases/latest)
- 或从源码构建，见下文“从源码构建”。

## 功能

- 状态栏实时显示两个字数字段：
  - `字数`：包含标点
  - `不含标点`：排除中英文标点符号
- 输入、粘贴、撤销、打开/切换文件时自动更新
- 兼容 UTF-8（含无 BOM）和 ANSI/GBK 编码的小说文件
- 插件菜单：
  - **刷新字数统计**：立即重新统计
  - **关于 / 统计规则**：查看统计口径

## 统计口径

- **字数**：不含空格、制表符、换行、全角空格等空白字符的字符数；
  汉字、英文字母、数字、标点、符号都算 1 个字符。
- **不含标点**：在“字数”基础上再排除中英文标点/符号
  （如 `，。！？；：“”‘’（）《》【】「」…—·` 以及英文 `, . ! ? ; : " ' ( )` 等）。
- 空白字符不计入任何数字。
- emoji、生僻字（补充平面字符）按 1 个字符计。
- 统计范围：**当前文档全文**，不是选区。

示例：一个 UTF-8 无 BOM 的章节文件（约 3.3 KB）→ 字数 1079，不含标点 938。

## 安装

把 `NppWordCount.dll` 放到：

```
<Notepad++ 安装目录>\plugins\NppWordCount\NppWordCount.dll
```

安装/更新后如果 Notepad++ 正在运行，请**重启 Notepad++** 使插件生效。

也可以右键 `install.bat` →“以管理员身份运行”（默认安装到 `C:\Program Files\Notepad++`）。

手动编译后，`build\NppWordCount.dll` 就是插件本体。

## 从源码构建

需要 Visual Studio 2022（或 Build Tools，安装“使用 C++ 的桌面开发”）。

```bat
build.bat
```

会生成：

- `build\NppWordCount.dll`：插件本体
- `build\test_core.exe`：统计核心单元测试（38 个用例）
- `build\wc_file.exe`：命令行字数核对工具

然后以管理员身份运行 `install.bat`。

### 手动编译（可选）

```bat
cl /nologo /utf-8 /std:c++17 /EHsc /O2 /MT /W4 /DUNICODE /D_UNICODE ^
   /Fo:build\ /LD src\NppWordCount.cpp src\WordCountCore.cpp ^
   /link /OUT:build\NppWordCount.dll /IMPLIB:build\NppWordCount.lib /DLL
```

注意：源码是 UTF-8 编码，必须加 `/utf-8`（或把源文件保存为“UTF-8 带 BOM”），
否则中文界面文字会乱码。

### 命令行核对某章字数

```bat
build\wc_file.exe "C:\path\to\chapter.txt"
```

输出：

```
encoding : UTF-8
with punct.    : 1079
without punct. : 938
```

## 项目结构

```
src\NppWordCount.cpp   插件入口、状态栏新增字段、事件处理
src\WordCountCore.cpp  字数统计核心（UTF-8 / UTF-16 / ANSI）
src\WordCountCore.h
src\NppPluginApi.h     最小化的 Notepad++/Scintilla 接口定义
src\NppWordCount.def   导出表（32 位构建时使用）
tests\test_core.cpp    单元测试
tests\wc_file.cpp      命令行核对工具
build.bat              编译 + 单元测试
install.bat            安装到 Notepad++（需要管理员权限）
```

## 实现说明

Notepad++ 官方 API 只允许设置已有的 6 个状态栏字段，不能新增字段。
本插件直接对状态栏控件（标准 `msctls_statusbar32`）做子类化：

1. 拦截 Notepad++ 的 `SB_SETPARTS`，在“长度/行数”字段后插入一个字数分段；
2. 拦截 Notepad++ 的 `SB_SETTEXT`，把原有分段的索引往后映射一位，
   这样 Notepad++ 更新“长度/行数”“行/列”等字段时不会写错位置；
3. 在插件自己的隐藏窗口中统计并更新字数，避免在 Scintilla 通知回调里
   重入 Scintilla，也不会和 Notepad++ 的状态栏刷新互相覆盖。

大文档（> 2 MB）统计做了 200ms 节流，避免连续输入时卡顿。

## 卸载

删除目录：

```
<Notepad++ 安装目录>\plugins\NppWordCount
```

重启 Notepad++ 即可。

## 备注

- 仅支持 64 位 Notepad++（当前主流版本）。
- 如果 Notepad++ 升级后状态栏结构发生变化，新增字段可能失效，需要重新适配。
- 本插件只读取文档内容做统计，不会修改、保存你的文件。
- 统计规则如需调整（例如是否把数字/英文算作字数、是否排除空白），
  修改 `src\WordCountCore.cpp` 中的 `IsWhitespace` / `IsPunctuation` 即可。

## License

MIT License，见 [LICENSE](LICENSE)。
