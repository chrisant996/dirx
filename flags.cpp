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

void FlipFlag(FormatFlags& flags, FormatFlags flag, bool& enable, bool default_enable)
{
    if (enable)
        flags |= flag;
    else
        flags &= ~flag;
    enable = default_enable;
}

void FailFlag(WCHAR ch, const WCHAR* value, WCHAR short_opt, const LongOption<WCHAR>* long_opt, Error& e)
{
    WCHAR tmp[2] = { ch, '\0' };
    if (long_opt)
    {
        e.Set(L"Unrecognized flag '%1' in '--%1=%2'.") << tmp << long_opt->name << value;
    }
    else
    {
        WCHAR short_name[2] = { short_opt, '\0' };
        e.Set(L"Unrecognized flag '%1' in '-%2%3'.") << tmp << short_name, value;
    }
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

