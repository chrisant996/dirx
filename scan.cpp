// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "scan.h"
#include "flags.h"
#include "filesys.h"
#include "patterns.h"

#include <regex>

/*
 * RegExpHelper.
 */

class RegExpHelper
{
public:
    RegExpHelper() {}
    ~RegExpHelper() { delete m_regex; }

    bool                Init(const WCHAR* pattern, Error& e);
    bool                Match(const WCHAR* s);

    bool                IsRegex() const { return !!m_regex; }

private:
    std::regex_constants::syntax_option_type m_syntax;
    std::wregex*        m_regex = nullptr;
};

/**
 * Initialize the regexp helper class.  Patterns beginning with :: are treated
 * as a regular expression.
 *
 * @return bool         Indicates success/failure, but does NOT indicate
 *                      whether the pattern was a regular expression.
 */
bool RegExpHelper::Init(const WCHAR* pattern, Error& e)
{
    assert(!m_regex);

    if (pattern[0] != ':' || pattern[1] != ':')
        return true;

    std::regex_constants::syntax_option_type syntax = std::regex_constants::ECMAScript;
    syntax |= std::regex_constants::icase;
    syntax |= std::regex_constants::optimize;

    try
    {
        m_regex = new std::wregex(pattern + 2, syntax);
    }
    catch (std::regex_error ex)
    {
        StrW s;
        s.SetA(ex.what());
        e.Set(s.Text());
        return false;
    }

    return true;
}

bool RegExpHelper::Match(const WCHAR* s)
{
    assert(m_regex);

    std::wcmatch matches;
    try
    {
        std::regex_search(s, s + wcslen(s), matches, *m_regex, std::regex_constants::match_default);
    }
    catch (std::regex_error ex)
    {
        return false;
    }

    return !!matches.size();
}

/*
 * Scan directories and files.
 */

static bool ScanFiles(DirScanCallbacks& callbacks, const WCHAR* dir, unsigned depth, const DirPattern* pattern, const bool top, unsigned limit_depth, Error& e)
{
    if (wcslen(dir) >= MaxPath())
    {
        e.Set(L"The directory name %1 is too long.") << dir;
        return false;
    }

    const bool usage = callbacks.Settings().IsSet(FMT_USAGE);
    assert(implies(usage, pattern->m_patterns.size() <= 1));

    callbacks.OnPatterns(pattern->m_patterns.size() > 1);

    bool any_files_found = false;
    bool any_headers_displayed = false;
    bool call_OnDirectoryEnd = false;
    const bool subdirs = callbacks.Settings().IsSet(FMT_SUBDIRECTORIES);
    bool displayed_header = false;
    for (size_t ii = 0; ii < pattern->m_patterns.size(); ii++)
    {
        RegExpHelper reh;
        if (!reh.Init(pattern->m_patterns[ii].Text(), e))
            return false;

        StrW s;
        s.Set(dir);
        EnsureTrailingSlash(s);
        if (usage || reh.IsRegex())
            s.Append(callbacks.Settings().IsSet(FMT_FAT) ? L"*.*" : L"*");
        else
            s.Append(pattern->m_patterns[ii]);

        const bool implicit = pattern->m_implicit;
        WIN32_FIND_DATA fd;
        SHFind shFind;

        callbacks.OnScanFiles(dir, pattern->m_patterns[ii].Text(), implicit, top);

        if (usage && !ii && (implicit || !callbacks.IsRootSubDir()))
        {
            callbacks.OnDirectoryBegin(dir);
            displayed_header = true;
            any_headers_displayed = true;
        }

        if (!usage ||
            (!ii && (implicit || !callbacks.IsRootSubDir())))
        {
            shFind = __FindFirstFile(s, callbacks.Settings().m_need_short_filenames, &fd);
            if (shFind.Empty())
            {
                const DWORD dwErr = GetLastError();
                if (dwErr == ERROR_FILE_NOT_FOUND ||
                    (dwErr == ERROR_ACCESS_DENIED && subdirs && !callbacks.IsOnlyRootSubDir()))
                {
                }
                else
                {
                    e.Set(dwErr);
                    return false;
                }
            }
            else
            {
                do
                {
                    if (fd.dwFileAttributes & callbacks.Settings().m_dwAttrExcludeAny)
                        continue;
                    if (callbacks.Settings().m_dwAttrIncludeAny && !(fd.dwFileAttributes & callbacks.Settings().m_dwAttrIncludeAny))
                        continue;
                    if (callbacks.Settings().m_dwAttrMatch && (fd.dwFileAttributes & callbacks.Settings().m_dwAttrMatch) != callbacks.Settings().m_dwAttrMatch)
                        continue;
                    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && callbacks.Settings().IsSet(FMT_HIDEPSEUDODIRS) && IsPseudoDirectory(fd.cFileName))
                        continue;
                    if (IsHiddenName(fd.cFileName))
                        continue;

                    if (reh.IsRegex() && !reh.Match(fd.cFileName))
                        continue;

                    if (!displayed_header)
                    {
                        callbacks.OnDirectoryBegin(dir);
                        displayed_header = true;
                        any_headers_displayed = true;
                    }

                    callbacks.OnFile(dir, &fd);
                    any_files_found = true;
                }
                while (FindNextFile(shFind, &fd));

                const DWORD dwErr = GetLastError();
                if (dwErr && dwErr != ERROR_NO_MORE_FILES)
                {
                    e.Set(dwErr);
                    return false;
                }
            }
        }

        const bool filter_dirs = (usage &&
                                  !implicit &&
                                  callbacks.IsRootSubDir());
        if ((subdirs && !ii) || filter_dirs)
        {
            const WCHAR* strip;

            strip = FindName(s.Text());
            s.SetEnd(strip);
            if (filter_dirs && !reh.IsRegex())
            {
                s.Append(pattern->m_patterns[ii]);
            }
            else
            {
                if (IsFATDrive(s.Text(), e))
                    s.Append(L"*.");
                s.Append('*');
            }

            shFind.Close();
            shFind = __FindFirstFile(s, callbacks.Settings().m_need_short_filenames, &fd);
            if (shFind.Empty())
            {
                const DWORD dwErr = GetLastError();
                if (dwErr == ERROR_FILE_NOT_FOUND ||
                    (dwErr == ERROR_ACCESS_DENIED && subdirs))
                {
                }
                else
                {
                    e.Set(dwErr);
                    return false;
                }
            }
            else
            {
                const unsigned new_depth = limit_depth ? depth + 1 : 0;
                do
                {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                        continue;
                    if (limit_depth && new_depth > limit_depth)
                        continue;
                    if (IsHidden(fd) && callbacks.Settings().IsSet(FMT_SKIPHIDDENDIRS))
                        continue;
                    if (IsTraversableReparse(fd) && callbacks.Settings().IsSet(FMT_SKIPJUNCTIONS))
                        continue;
                    if (IsPseudoDirectory(fd.cFileName))
                        continue;

                    if (filter_dirs && reh.IsRegex() && !reh.Match(fd.cFileName))
                        continue;

                    strip = FindName(s.Text());
                    s.SetEnd(strip);
                    s.Append(fd.cFileName);
                    callbacks.AddSubDir(s, new_depth);
                }
                while (FindNextFile(shFind, &fd));

                callbacks.SortSubDirs();

                const DWORD dwErr = GetLastError();
                if (dwErr && dwErr != ERROR_NO_MORE_FILES)
                {
                    e.Set(dwErr);
                    return false;
                }
            }
        }
    }

    call_OnDirectoryEnd = (any_headers_displayed && (any_files_found || usage));
    if (call_OnDirectoryEnd)
        callbacks.OnDirectoryEnd(true);

    return any_files_found;
}

int ScanDir(DirScanCallbacks& callbacks, const DirPattern* patterns, unsigned limit_depth, Error& e)
{
    int rc = 0;

    // Find matching files, recursing into subdirectories as needed.

    StrW dir;
    StrW drive;
    StrW prev_drive;
    StrW prev_drive_dir; // OnVolumeBegin/OnVolumeEnd take a dir because they need to test for failure when converting the dir to a drive.
    bool in_volume = false;
    bool any_files_found = false;
    for (const DirPattern* p = patterns; p; p = p->m_next)
    {
        const FormatFlags flagsRestore = callbacks.Settings().m_flags;

        if (p->m_isFAT && !callbacks.Settings().IsSet(FMT_FORCENONFAT))
        {
            callbacks.Settings().m_flags |= FMT_FAT;
            callbacks.Settings().m_flags &= ~FMT_CLASSIFY;
        }

        dir.Set(p->m_dir);
        unsigned depth = p->m_depth;

        bool top = true;
        while (true)
        {
            GetDrive(dir.Text(), drive, e);
            if (e.Test())
                return 1;

            if (!prev_drive.EqualI(drive))
            {
                if (in_volume)
                {
                    if (any_files_found)
                    {
                        callbacks.OnVolumeEnd(prev_drive_dir.Text());
                        in_volume = false;
                    }
                    else if (!callbacks.Settings().IsSet(FMT_BARE) || callbacks.Settings().IsSet(FMT_USAGE))
                    {
                        callbacks.OnFileNotFound();
                    }
                }

                if (dir.Length()) // dir not drive: we're checking whether we're about to break out of the loop (drive can legitimately be empty).
                {
                    in_volume = true;
                    callbacks.OnVolumeBegin(dir.Text(), e);
                    if (e.Test())
                        return 1;
                }

                prev_drive.Set(drive);
                prev_drive_dir.Set(dir);
                any_files_found = false;
            }

            if (!dir.Length())
                break;

            if (ScanFiles(callbacks, dir.Text(), depth, p, top, limit_depth, e))
            {
                any_files_found = true;
                // REVIEW: Does rc=0 match CMD DIR behavior?
                rc = 0;
            }
            if (e.Test())
            {
                if (e.Code() == 0)
                {
                    // No code means it's a custom error message.
                    e.Report();
                    rc = 1;
                }
                else if (e.Code() == ERROR_ACCESS_DENIED)
                {
                    // CMD DIR reports access denied errors during traversal,
                    // so we will as well.
                    e.Report();
                }
                else if (!callbacks.CountDirs() && !callbacks.CountFiles())
                {
                    // CMD DIR always converts other errors into "file not
                    // found" if no dirs/files have been found.  The
                    // message gets reported by OnFileNotFound above.
                    e.Clear();
                    rc = 1;
                }
                else
                {
                    // REVIEW: Is this equivalent to CMD DIR behavior?
                    // TODO: Anything special needed for things like
                    // ERROR_BUFFER_OVERFLOW, ERROR_FILENAME_EXCED_RANGE,
                    // IsTraversableReparse(), or reparse points where
                    // IsTraversableReparse() returns false?
                    e.Clear();
                }
            }
            top = false;

            // We don't want to show the volume information for each
            // pattern on the command, we only want to show volume
            // information when we actually start looking at a different
            // volume.  So if there are no more subdirs but there are more
            // patterns, then we want to bail out of the loop right now to
            // avoid reseting prev_drive which would show the volume
            // information once per pattern.  But we don't want to bail
            // out if this is the last pattern, because then we'd
            // accidentally skip the summary information.

            if (!callbacks.NextSubDir(dir, depth) && p->m_next)
                break;
        }

        callbacks.Settings().m_flags = flagsRestore;
    }

    return rc;
}

