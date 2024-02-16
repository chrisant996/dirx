// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "flags.h"

const DirFormatSettings* g_settings = nullptr;

void SkipColonOrEqual(const WCHAR*& p)
{
    if (*p == ':' || *p == '=')
        ++p;
}

void DirFormatSettings::ClearMinMax()
{
    memset(&m_min_time, 0xff, sizeof(m_min_time));
    memset(&m_max_time, 0x00, sizeof(m_max_time));
    memset(&m_min_size, 0xff, sizeof(m_min_size));
    memset(&m_max_size, 0x00, sizeof(m_max_size));
}

void DirFormatSettings::UpdateMinMaxTime(WhichTimeStamp which, const FILETIME& ft)
{
    ULONGLONG ull = FileTimeToULONGLONG(ft);
    if (m_min_time[which] > ull)
        m_min_time[which] = ull;
    if (m_max_time[which] < ull)
        m_max_time[which] = ull;
}

void DirFormatSettings::UpdateMinMaxSize(WhichFileSize which, ULONGLONG size)
{
    if (m_min_size[which] > size)
        m_min_size[which] = size;
    if (m_max_size[which] < size)
        m_max_size[which] = size;
}

