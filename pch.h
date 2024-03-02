// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma warning(disable: 4290)

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <new>
#include <tchar.h>
#include <assert.h>

extern int g_debug;
extern int g_nix_defaults;

#define implies(x, y)           (!(x) || (y))

template <typename T> T clamp(T value, T min, T max)
{
    value = value < min ? min : value;
    return value > max ? max : value;
}

#include "str.h"
#include "error.h"
#include "handle.h"
