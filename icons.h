// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <windows.h>

void SetNerdFontsVersion(unsigned ver=3);
const WCHAR* LookupIcon(const WCHAR* full, DWORD attr);

void PrintAllIcons();

