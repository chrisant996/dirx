// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <windows.h>

class Error;
class FileInfo;

int ValidateColor(const WCHAR* p, const WCHAR* end=nullptr);

void InitColors(const WCHAR* custom);
const WCHAR* LookupColor(const FileInfo* pfi);
const WCHAR* LookupColor(const WCHAR* name, DWORD attr, unsigned short mode);
const WCHAR* GetIconColor(const WCHAR* color);
const WCHAR* GetColorByKey(const WCHAR* key);
const WCHAR* GetSizeColor(ULONGLONG ull);
const WCHAR* ApplyGradient(const WCHAR* color, ULONGLONG value, ULONGLONG min, ULONGLONG max);

extern const WCHAR c_norm[];

