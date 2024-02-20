// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <windows.h>
#include "str.h"
#include <vector>

struct DirFormatSettings;
class Error;

struct SubDir
{
    StrW                dir;
    unsigned            depth;
};

struct DirPattern
{
    DirPattern() : m_implicit(false), m_next(nullptr) {}

    std::vector<StrW>   m_patterns;
    StrW                m_dir;
    bool                m_isFAT;
    bool                m_implicit;
    unsigned            m_depth = 0;

    DirPattern*         m_next;
};

const WCHAR* FindExtension(const WCHAR* file);
const WCHAR* FindName(const WCHAR* file);

DirPattern* MakePatterns(int argc, const WCHAR** argv, const DirFormatSettings& settings, Error& e);

