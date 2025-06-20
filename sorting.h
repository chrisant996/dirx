// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <windows.h>
#include "fileinfo.h"

#include <memory>

struct SubDir;

extern WCHAR g_sort_order[16];

void SetSortOrder(const WCHAR* order, Error& e);
void SetDefaultNumericSort(bool numeric_sort);
void SetStringSort(bool string_sort);
void SetReverseSort(bool reverse);
bool IsReversedSort();

namespace Sorting
{
int CmpStrN(const WCHAR* p1, int len1, const WCHAR* p2, int len2);
int CmpStrNI(const WCHAR* p1, int len1, const WCHAR* p2, int len2);
inline int CmpStr(const WCHAR* p1, const WCHAR* p2) { return CmpStrN(p1, -1, p2, -1); }
inline int CmpStrI(const WCHAR* p1, const WCHAR* p2) { return CmpStrNI(p1, -1, p2, -1); }
};

bool CmpFileInfo(const std::unique_ptr<FileInfo>& fi1, const std::unique_ptr<FileInfo>& fi2);
bool CmpSubDirs(const std::unique_ptr<SubDir>& d1, const std::unique_ptr<SubDir>& d2);

