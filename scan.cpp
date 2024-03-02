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

static bool ScanFiles(DirScanCallbacks& callbacks, const WCHAR* dir, const WCHAR* dir_rel,
                      unsigned depth, const DirPattern* pattern,
                      const bool top, unsigned limit_depth,
                      const std::shared_ptr<const GlobPatterns>& git_ignore,
                      const std::shared_ptr<const RepoStatus>& repo,
                      Error& e)
{
    if (depth > limit_depth)
        return true;

    if (wcslen(dir) >= MaxPath())
    {
        e.Set(L"The directory name %1 is too long.") << dir;
        return false;
    }

    const bool usage = callbacks.Settings().IsSet(FMT_USAGE);
    assert(implies(usage, pattern->m_patterns.size() <= 1));

    callbacks.OnPatterns(pattern->m_patterns.size() > 1);

    StrW s2;
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
        StrW rel_parent;
        s.Set(dir);
        rel_parent.Set(dir_rel);
        EnsureTrailingSlash(s);
        if (rel_parent.Length() && rel_parent.Text()[rel_parent.Length() - 1] != ':')
            EnsureTrailingSlash(rel_parent);
        if (usage || reh.IsRegex())
            s.Append(callbacks.Settings().IsSet(FMT_FAT) ? L"*.*" : L"*");
        else
            s.Append(pattern->m_patterns[ii]);

        const bool implicit = pattern->m_implicit;
        WIN32_FIND_DATA fd;
        SHFind shFind;

        callbacks.OnScanFiles(dir, pattern->m_patterns[ii].Text(), implicit, top);

        if ((usage && !ii && (implicit || !callbacks.IsRootSubDir())) || callbacks.Settings().IsSet(FMT_TREE))
        {
            callbacks.OnDirectoryBegin(dir, dir_rel, repo);
            displayed_header = true;
            any_headers_displayed = true;
        }

        if (!usage ||
            (!ii && (implicit || !callbacks.IsRootSubDir())))
        {
            if (g_debug)
                wprintf(L"debug: scan '%s' for files\n", s.Text());

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
            else if (!(!limit_depth && !depth && callbacks.Settings().IsSet(FMT_TREE)))
            {
                do
                {
                    if (fd.dwFileAttributes & callbacks.Settings().m_dwAttrExcludeAny)
                        continue;
                    if (callbacks.Settings().m_dwAttrIncludeAny && !(fd.dwFileAttributes & callbacks.Settings().m_dwAttrIncludeAny))
                        continue;
                    if (callbacks.Settings().m_dwAttrMatch && (fd.dwFileAttributes & callbacks.Settings().m_dwAttrMatch) != callbacks.Settings().m_dwAttrMatch)
                        continue;
                    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                        ((callbacks.Settings().IsSet(FMT_HIDEPSEUDODIRS) && IsPseudoDirectory(fd.cFileName)) ||
                         (callbacks.Settings().IsSet(FMT_TREE))))
                        continue;
                    if (IsHiddenName(fd.cFileName))
                        continue;

                    if (reh.IsRegex() && !reh.Match(fd.cFileName))
                        continue;
                    if (pattern->IsIgnore(dir, fd.cFileName))
                        continue;
                    if (git_ignore && git_ignore.get()->IsMatch(dir, fd.cFileName))
                        continue;

                    if (!displayed_header)
                    {
                        callbacks.OnDirectoryBegin(dir, dir_rel, repo);
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
        if (((subdirs && !ii) || filter_dirs) && depth + 1 <= limit_depth)
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

            if (g_debug)
                wprintf(L"debug: scan '%s' for directories\n", s.Text());

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
                const unsigned new_depth = depth + 1;
                assert(new_depth <= limit_depth);
                do
                {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                        continue;
                    if (IsHidden(fd) && callbacks.Settings().IsSet(FMT_SKIPHIDDENDIRS))
                        continue;
                    if (IsTraversableReparse(fd) && callbacks.Settings().IsSet(FMT_SKIPJUNCTIONS))
                        continue;
                    if (IsPseudoDirectory(fd.cFileName))
                        continue;

                    if (filter_dirs && reh.IsRegex() && !reh.Match(fd.cFileName))
                        continue;
                    if (pattern->IsIgnore(dir, fd.cFileName))
                        continue;
                    if (git_ignore && git_ignore.get()->IsMatch(dir, fd.cFileName))
                        continue;

                    if (callbacks.Settings().IsSet(FMT_TREE))
                    {
                        if (!displayed_header)
                        {
                            callbacks.OnDirectoryBegin(dir, dir_rel, repo);
                            displayed_header = true;
                            any_headers_displayed = true;
                        }
                        callbacks.OnFile(dir, &fd);
                        any_files_found = true;
                    }

                    strip = FindName(s.Text());
                    s.SetEnd(strip);
                    s.Append(fd.cFileName);
                    s2.Set(rel_parent);
                    if (s2.Length() && s2.Text()[s2.Length() - 1] != ':')
                        EnsureTrailingSlash(s2);
                    s2.Append(fd.cFileName);
                    callbacks.AddSubDir(s, s2, new_depth, git_ignore, repo);
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
        callbacks.OnDirectoryEnd(dir, true);

    return any_files_found;
}

int ScanDir(DirScanCallbacks& callbacks, const DirPattern* patterns, unsigned limit_depth, Error& e)
{
    int rc = 0;

    // Find matching files, recursing into subdirectories as needed.

    StrW dir;
    StrW dir_rel;
    StrW drive;
    StrW prev_drive;
    StrW prev_drive_dir; // OnVolumeBegin/OnVolumeEnd take a dir because they need to test for failure when converting the dir to a drive.
    bool in_volume = false;
    bool any_files_found = false;
    for (const DirPattern* p = patterns; p; p = p->m_next)
    {
        std::shared_ptr<const GlobPatterns> git_ignore; // p->IsIgnore() internally handles the DirPattern's own git_ignore.
        std::shared_ptr<const RepoStatus> repo(p->m_repo);
        const FormatFlags flagsRestore = callbacks.Settings().m_flags;

        if (p->m_isFAT && !callbacks.Settings().IsSet(FMT_FORCENONFAT))
        {
            callbacks.Settings().m_flags |= FMT_FAT;
            callbacks.Settings().m_flags &= ~FMT_CLASSIFY;
        }

        dir.Set(p->m_dir);
        dir_rel.Set(p->m_dir_rel);
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
                    else if (!callbacks.Settings().IsSet(FMT_BARE|FMT_TREE) || callbacks.Settings().IsSet(FMT_USAGE))
                    {
                        // File Not Found is not a fatal error:  report it and continue.
                        e.Set(L"File Not Found");
                        callbacks.ReportError(e);
                        e.Clear();
                        rc = 1;
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

            if (ScanFiles(callbacks, dir.Text(), dir_rel.Text(), depth, p, top, limit_depth, git_ignore, repo, e))
            {
                any_files_found = true;
                rc = 0;
            }
            if (e.Test())
            {
                if (e.Code() == 0)
                {
                    // No code means it's a custom error message.
                    callbacks.ReportError(e);
                    e.Clear();
                    rc = 1;
                }
                else if (e.Code() == ERROR_ACCESS_DENIED)
                {
                    // CMD DIR reports access denied errors during traversal,
                    // so we will as well.
                    callbacks.ReportError(e);
                    e.Clear();
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

            if (!callbacks.NextSubDir(dir, dir_rel, depth, git_ignore, repo) && p->m_next)
                break;
        }

        callbacks.OnPatternEnd(p);

        callbacks.Settings().m_flags = flagsRestore;
    }

    return rc;
}

