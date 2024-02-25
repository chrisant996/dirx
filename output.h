// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

void SetUtf8Output(bool utf8);
bool SetUseEscapeCodes(const WCHAR* s);
bool CanUseEscapeCodes(HANDLE hout);

bool SetPagination(bool paginate);
DWORD GetConsoleColsRows(HANDLE hout);

enum PaginationAction { pageFull, pageHalf, pageOneLine, pageAbort };
PaginationAction PageBreak(HANDLE hin, HANDLE hout);

void OutputConsole(HANDLE h, const WCHAR* p, unsigned len=unsigned(-1), const WCHAR* color=nullptr);

