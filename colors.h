// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <windows.h>

class Error;
class FileInfo;

int ValidateColor(const WCHAR* p, const WCHAR* end=nullptr);
int HasBackgroundColor(const WCHAR* p);
void ReportColorlessError(Error& e);

void InitColors(const WCHAR* custom);
bool UseLinkTargetColor();
void SetAttrsForColors(DWORD attrs_for_colors);
const WCHAR* LookupColor(const FileInfo* pfi, const WCHAR* dir, bool ignore_target_color=false);
const WCHAR* LookupColor(const WCHAR* name, DWORD attr, unsigned short mode);
const WCHAR* GetAttrLetterColor(DWORD attr);
const WCHAR* GetIconColor(const WCHAR* color);
const WCHAR* GetColorByKey(const WCHAR* key);
const WCHAR* GetSizeColor(ULONGLONG ull);
const WCHAR* GetSizeUnitColor(ULONGLONG ull);
const WCHAR* ApplyGradient(const WCHAR* color, ULONGLONG value, ULONGLONG min, ULONGLONG max);
const WCHAR* StripLineStyles(const WCHAR* color);

const WCHAR* GetDefaultColorString();

extern const WCHAR c_norm[];
extern const WCHAR c_error[];

