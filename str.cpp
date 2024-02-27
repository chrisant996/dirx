// Copyright (c) 2024 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include <stdio.h>
#include <stdarg.h>
#include "str.h"
#include "wcwidth.h"

int __vsnprintf(char* buffer, size_t len, const char* format, va_list args)
{
    return _vsnprintf_s(buffer, len, _TRUNCATE, format, args);
}

int __vsnprintf(WCHAR* const buffer, size_t const len, const WCHAR* const format, va_list args)
{
    return _vsnwprintf_s(buffer, len, _TRUNCATE, format, args);
}

char Str<char>::s_empty[1] = "";
WCHAR Str<WCHAR>::s_empty[1] = L"";

const char Str<char>::c_spaces[33] = "                                ";
const WCHAR Str<WCHAR>::c_spaces[33] = L"                                ";

void StrA::SetW(const WCHAR* p, size_t len)
{
    Clear();

    if (int(len) < 0)
        len = StrLen(p);

    const size_t needed = WideCharToMultiByte(CP_ACP, 0, p, int(len), 0, 0, 0, 0);
    int used = WideCharToMultiByte(CP_ACP, 0, p, int(len), Reserve(needed + 1), int(needed), 0, 0);

    assert(unsigned(used) < Capacity());
    m_length = used;
    m_p[used] = '\0';
}

void StrW::SetA(const char* p, size_t len)
{
    Clear();

    if (int(len) < 0)
        len = StrLen(p);

    const size_t needed = MultiByteToWideChar(CP_ACP, 0, p, int(len), 0, 0);
    int used = MultiByteToWideChar(CP_ACP, 0, p, int(len), Reserve(needed + 1), int(needed));

    assert(unsigned(used) < Capacity());
    m_length = used;
    m_p[used] = '\0';
}

WCHAR* CopyStr(const WCHAR* p)
{
    if (!p)
        return nullptr;

    size_t len = wcslen(p) + 1;
    size_t bytes = len * sizeof(*p);
    WCHAR* out = (WCHAR*)malloc(bytes);
    memcpy(out, p, bytes);
    return out;
}

void StripTrailingSlashes(StrW& s)
{
    while (s.Length() && IsPathSeparator(s.Text()[s.Length() - 1]))
        s.SetLength(s.Length() - 1);
}

void EnsureTrailingSlash(StrW& s)
{
    if (s.Length())
    {
        const WCHAR last = s.Text()[s.Length() - 1];
        if (last == '\\')
            return;
        if (last == '/')
            s.SetLength(s.Length() - 1);
        s.Append('\\');
    }
}

void PathJoin(StrW& out, const WCHAR* dir, const WCHAR* file)
{
    out.Set(dir);
    if (*dir)
        EnsureTrailingSlash(out);
    out.Append(file);
}

void PathJoin(StrW& out, const WCHAR* dir, const StrW& file)
{
    out.Set(dir);
    if (*dir)
        EnsureTrailingSlash(out);
    out.Append(file);
}

unsigned TruncateWcwidth(StrW& s, const unsigned truncate_width, const WCHAR truncation_char)
{
    const unsigned truncation_char_width = ((truncation_char == '.') ? 2 :
                                            (truncation_char == 0) ? 0 :
                                            __wcwidth(truncation_char));

    if (truncation_char_width > truncate_width)
    {
        s.Clear();
        return 0;
    }

    const WCHAR* truncate = s.Text();
    unsigned width = 0;

    for (const WCHAR* p = s.Text(); *p; ++p)
    {
        if (width + truncation_char_width <= truncate_width)
            truncate = p;

        char32_t ch = *p;

        // Decode surrogate pair.
        if ((ch & 0xFC00) == 0xD800)
        {
            WCHAR trail = p[1];
            if ((trail & 0xFC00) == 0xDC00)
            {
                ch = 0x10000 + (ch - 0xD800) * 0x400 + (trail - 0xDC00);
                ++p;
            }
        }

        int w = __wcwidth(ch);
        w = (w < 0) ? 1 : w;

        if (width + w > truncate_width)
        {
            s.SetEnd(truncate);
            if (truncation_char)
            {
                s.Append(truncation_char);
                if (truncation_char == '.')
                    s.Append(truncation_char);
            }
            return width + truncation_char_width;
        }

        width += w;
    }

    return width;
}

bool SortCase::operator()(const WCHAR* a, const WCHAR* b) const noexcept
{
    return wcscmp(a, b) < 0;
}

bool SortCaseless::operator()(const WCHAR* a, const WCHAR* b) const noexcept
{
    return _wcsicmp(a, b) < 0;
}

bool EqualCase::operator()(const WCHAR* a, const WCHAR* b) const noexcept
{
    return wcscmp(a, b) == 0;
}

bool EqualCaseless::operator()(const WCHAR* a, const WCHAR* b) const noexcept
{
    return _wcsicmp(a, b) == 0;
}

_NODISCARD size_t HashCase::operator()(const WCHAR* key) const noexcept
{
    size_t hash = 0;
    for (; WCHAR ch = *key; ++key)
    {
        hash *= 3;
        hash += (ch & 0xff);
        if (BYTE(ch) != ch)
        {
            hash *= 3;
            hash += (ch >> 8);
        }
    }
    return hash;
}

_NODISCARD size_t HashCaseless::operator()(const WCHAR* key) const noexcept
{
    size_t hash = 0;
    for (; WCHAR ch = *key; ++key)
    {
        if (iswupper(ch))
            ch = towlower(ch);
        hash *= 3;
        hash += (ch & 0xff);
        if (BYTE(ch) != ch)
        {
            hash *= 3;
            hash += (ch >> 8);
        }
    }
    return hash;
}

