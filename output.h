// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

bool IsConsole(HANDLE h);

void SetUtf8Output(bool utf8);
void SetRedirectedStdOut(bool redirected);
bool IsRedirectedStdOut();
bool IsAsciiLineCharMode();

bool SetUseEscapeCodes(const WCHAR* s);
bool CanUseEscapeCodes(HANDLE hout);
void SetGracefulExit();

bool SetPagination(bool paginate);
DWORD GetConsoleColsRows(HANDLE hout);

enum PaginationAction { pageFull, pageHalf, pageOneLine, pageAbort };
PaginationAction PageBreak(HANDLE hin, HANDLE hout);

void ExpandTabs(const WCHAR* s, StrW& out, unsigned max_width=0);
void OutputConsole(HANDLE h, const WCHAR* p, unsigned len=unsigned(-1), const WCHAR* color=nullptr);

void PrintfV(const WCHAR* format, va_list args);
void Printf(const WCHAR* format, ...);

