// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma warning(disable: 4290)

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <new>
#include <tchar.h>
#include <uchar.h>
#include <assert.h>

extern int g_debug;
extern int g_nix_defaults;

#define implies(x, y)           (!(x) || (y))

template <typename T> T clamp(T value, T min, T max)
{
    value = value < min ? min : value;
    return value > max ? max : value;
}

typedef __int8 int8;
typedef unsigned __int8 uint8;
typedef __int32 int32;
typedef unsigned __int32 uint32;

#undef min
#undef max
template<class T> T min(T a, T b) { return (a <= b) ? a : b; }
template<class T> T max(T a, T b) { return (a >= b) ? a : b; }

#include "str.h"
#include "error.h"
#include "handle.h"
