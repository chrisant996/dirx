// Copyright (c) 2024 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#include <windows.h>

typedef int (*wcwidth_t)(char32_t ucs);

extern const wcwidth_t __wcwidth;
char32_t __decode(const WCHAR*& p, unsigned* len=nullptr);
unsigned __wcswidth(const WCHAR* p, unsigned len=-1);

