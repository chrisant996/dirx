// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "filesys.h"
#include "fileinfo.h"
#include "patterns.h"
#include "flags.h"

inline void AppendToTail(DirPattern*& head, DirPattern*& tail, DirPattern* p)
{
    assert(!!head == !!tail);
    if (!head)
        head = p;
    else
        tail->m_next = p;
    tail = p;
}

inline bool IsDriveOnly(const WCHAR* p)
{
    p += IsExtendedPath(p);

    // If it does not have any path character, is 2 chars long and has a
    // ':' it must be a drive.
    return (p[0] &&
            p[0] != '\\' &&
            p[0] != '/' &&
            p[1] == ':' &&
            !p[2]);
}

inline bool IsStrange(const WCHAR* p)
{
    // GetFullPathName converts "con" into the strange "\\.\con" and etc.
    return (p[0] == '\\' &&
            p[1] == '\\' &&
            p[2] == '.' &&
            (!p[3] || p[3] == '\\'));
}

const WCHAR* FindExtension(const WCHAR* file)
{
    const WCHAR* ext = 0;

    for (const WCHAR* p = file; *p; p++)
    {
        if (*p == ' ' || *p == '\t' || *p == '\\' || *p == '/')
            ext = 0;
        else if (*p == '.')
            ext = p;
    }

    return ext;
}

const WCHAR* FindName(const WCHAR* file)
{
    const WCHAR* name = file;

    for (const WCHAR* p = file; *p; p++)
    {
        if (*p == '\\' || *p == '/')
            name = p + 1;
    }

    return name;
}

static void AdjustSlashes(StrW& s, const WCHAR*& regex)
{
    bool beginning = true;
    for (const WCHAR* walk = s.Text(); *walk; walk++)
    {
        if (beginning && walk[0] == ':' && walk[1] == ':')
        {
            regex = walk;
            return;
        }
        if (*walk == '/')
        {
            s.SetAt(walk, '\\');
            beginning = true;
        }
        else if (*walk == '\\')
        {
            beginning = true;
        }
        else if (beginning)
        {
            beginning = false;
        }
    }
}

static void AdjustPattern(StrW& pattern, const WCHAR* regex, bool isFAT, StrW& dir, bool& implicit, Error& e)
{
    assert(pattern.Length());
    assert(!dir.Length());

    // If the pattern is a directory, append * as the wildcard.

    implicit = false;
    if (!regex)
    {
        bool is_dir = (pattern.Length() && pattern.Text()[pattern.Length() - 1] == '\\');
        if (!is_dir)
        {
            const DWORD dwAttr = GetFileAttributes(pattern.Text());
            is_dir = (dwAttr != 0xffffffff && (dwAttr & FILE_ATTRIBUTE_DIRECTORY));
        }

        // is_dir may have just been changed, so we need to test it again.
        if (is_dir)
        {
            if (IsDriveOnly(pattern.Text()))
                GetCwd(pattern, *pattern.Text());
            EnsureTrailingSlash(pattern);
            pattern.Append('*');
            implicit = true;
        }
    }

    // The goal is to generally produce the same error messages as CMD DIR
    // does, but that's very dependent on quirks of what CMD does, and in what
    // order.  Reverse engineering that is time consuming, but I think this
    // mostly produces the same kinds of error messages given the same inputs.
    // (Can't do that here when a regular expression is used, though.)

    // REVIEW:  This tries to use IsStrange() to detect devices and pipes by
    // relying on GetFullPathName to expand devices and pipes into the "\\.\"
    // namespace.  But it might need to use more a direct check for devices
    // and pipes.

    const bool was_strange = IsStrange(pattern.Text());
    if (!regex)
    {
        OverrideErrorMode(SEM_FAILCRITICALERRORS);

        WCHAR* file_part;
        StrW full;
        full.ReserveMaxPath();
        const DWORD len = GetFullPathName(pattern.Text(), full.Capacity(), full.Reserve(), &file_part);

        if (!len)
        {
            e.Sys();
            return;
        }
        else if (len >= full.Capacity())
        {
            e.Sys(ERROR_FILENAME_EXCED_RANGE);
            return;
        }

        pattern.Set(full);
    }

    // Separate the path and file portions, to be processed separately.

    const WCHAR* strip = regex ? regex : FindName(pattern.Text());

    dir.Set(pattern);
    dir.SetLength(strip - pattern.Text());
    if (strip != pattern.Text())
        pattern.Set(strip);

    // If the file system is FAT then we have to be explicit about where "*"
    // is specified in a name.
    //      - ".foo"  -->  prepend "*"  -->  "*.foo"
    //      - "*"     -->  append ".*"  -->  "*.*"

    if (isFAT)
    {
        if (*pattern.Text() == '.')
        {
            assert(!IsPseudoDirectory(pattern.Text()));
            if (!IsPseudoDirectory(pattern.Text()))
            {
                StrW tmp;
                tmp.Set(pattern);
                pattern.Set(L"*");
                pattern.Append(tmp);
            }
        }
        else if (pattern.Equal(L"*"))
        {
            pattern.Append(L".*");
        }
    }

    // Expand the path.

    if (!dir.Length())
    {
        GetCwd(dir);
    }
    else if (regex)
    {
        OverrideErrorMode(SEM_FAILCRITICALERRORS);

        // When using a regex GetFullPathName can only be used on the dir name
        // part, so wait until after splitting the dir and file name parts.
        WCHAR* file_part;
        StrW full;
        full.ReserveMaxPath();
        DWORD len = GetFullPathName(dir.Text(), full.Capacity(), full.Reserve(), &file_part);

        if (len)
            len += isFAT ? 4 : 2; // Backslash and star (dot star).

        if (!len)
        {
            e.Sys();
            return;
        }
        else if (len >= full.Capacity())
        {
            e.Sys(ERROR_FILENAME_EXCED_RANGE);
            return;
        }

        dir.Set(full);
    }
    StripTrailingSlashes(dir);

    if ((was_strange || !IsStrange(dir.Text())) &&
        !IsExtendedPath(dir.Text()) &&
        GetFileAttributes(dir.Text()) == 0xffffffff)
    {
        e.Sys();
        return;
    }

    assert(dir.Length());
    assert(pattern.Length());
}

DirPattern* MakePatterns(int argc, const WCHAR** argv, const DirFormatSettings& settings, Error& e)
{
    DirPattern* patterns = nullptr;
    DirPattern* tail = nullptr;

    while (argc)
    {
        DirPattern* const p = new DirPattern;
        p->m_patterns.emplace_back();
        p->m_patterns.back().Set(argv[0]);
        AppendToTail(patterns, tail, p);
        argc--;
        argv++;
    }

    if (!patterns)
    {
        DirPattern* const p = new DirPattern;
        p->m_patterns.emplace_back();
        GetCwd(p->m_patterns.back());
        AppendToTail(patterns, tail, p);
    }

    StrW tmp;
    DirPattern* cur = 0;
    for (DirPattern* p = patterns; p; p = p->m_next)
    {
        const WCHAR* regex = 0;
        AdjustSlashes(p->m_patterns[0], regex);

        // Check the file system, and adjust wildcards and options
        // appropriately.

        tmp.Set(p->m_patterns[0].Text(), regex ? regex - p->m_patterns[0].Text() : p->m_patterns[0].Length());
        if (!tmp.Length())
            GetCwd(tmp);
        p->m_isFAT = IsFATDrive(tmp.Text(), e);
        if (e.Test())
            return nullptr;

        AdjustPattern(p->m_patterns[0], regex, p->m_isFAT, p->m_dir, p->m_implicit, e);
        if (e.Test())
            return nullptr;

        if (p->m_isFAT && settings.m_whichtimestamp != TIMESTAMP_MODIFIED)
        {
            e.Set(L"FAT volumes do not store file Access or Creation times.");
            return nullptr;
        }

        // Group together adjacent patterns in the same directory, so we only
        // emit the directory header/footer once for the group of them.

        if (cur && cur->m_dir.EqualI(p->m_dir))
        {
            assert(p == cur->m_next);
            assert(p->m_patterns.size() == 1);
            cur->m_patterns.emplace_back();
            cur->m_patterns.back() = std::move(p->m_patterns[0]);
            cur->m_next = p->m_next;
            delete p;
            p = cur;
        }
        else
        {
            cur = p;
        }
    }

    return patterns;
}

