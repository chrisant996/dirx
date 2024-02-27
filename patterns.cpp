// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "filesys.h"
#include "fileinfo.h"
#include "patterns.h"
#include "handle.h"
#include "flags.h"
#include "git.h"

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

static void AdjustPattern(DirPattern* p, const WCHAR* regex, Error& e)
{
    StrW& pattern = p->m_patterns[0];
    StrW& dir = p->m_dir;
    bool& implicit = p->m_implicit;

    assert(pattern.Length());
    assert(!dir.Length());

    bool dir_rel_finished = false;

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
            {
                GetCwd(pattern, *pattern.Text());
                dir_rel_finished = true;
            }
            EnsureTrailingSlash(pattern);
            if (!dir_rel_finished)
            {
                EnsureTrailingSlash(p->m_dir_rel);
                dir_rel_finished = true;
            }
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

    if (!dir_rel_finished)
    {
        strip = FindName(p->m_dir_rel.Text());
        p->m_dir_rel.SetLength(strip - p->m_dir_rel.Text());
    }

    // If the file system is FAT then we have to be explicit about where "*"
    // is specified in a name.
    //      - ".foo"  -->  prepend "*"  -->  "*.foo"
    //      - "*"     -->  append ".*"  -->  "*.*"

    if (p->m_isFAT)
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
            len += p->m_isFAT ? 4 : 2; // Backslash and star (dot star).

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

static const WCHAR* FindGlobEnd(const WCHAR* p)
{
    for (;;)
    {
        if (!*p || *p == ';' || *p == '|')
            return p;
        if (*p == '\\' && p[1])
            ++p;
        ++p;
    }
}

static GlobPatterns MakeGlobs(const WCHAR* dir, const WCHAR* ignore_globs)
{
    StrW glob;
    GlobPatterns globs;
    globs.SetRoot(dir);

    if (ignore_globs && *ignore_globs)
    {
        while (true)
        {
            const WCHAR* end = FindGlobEnd(ignore_globs);
            while (*ignore_globs == ' ')
                ++ignore_globs;
            if (ignore_globs == end || !*ignore_globs)
                break;

            glob.Set(ignore_globs, end - ignore_globs);
            glob.TrimRight();

            globs.Append(glob.Text());

            ignore_globs = end;
            assert(!*ignore_globs || *ignore_globs == '|' || *ignore_globs == ';');
            if (*ignore_globs == '|' || *ignore_globs == ';')
                ++ignore_globs;
        }
    }
    return globs;
}

DirPattern* MakePatterns(int argc, const WCHAR** argv, const DirFormatSettings& settings, const WCHAR* ignore_globs, Error& e)
{
    DirPattern* patterns = nullptr;
    DirPattern* tail = nullptr;

    while (argc)
    {
        DirPattern* const p = new DirPattern;
        p->m_patterns.emplace_back();
        p->m_patterns.back().Set(argv[0]);
        p->m_dir_rel.Set(argv[0]);
        AppendToTail(patterns, tail, p);
        argc--;
        argv++;
    }

    if (!patterns)
    {
        DirPattern* const p = new DirPattern;
        p->m_patterns.emplace_back();
        GetCwd(p->m_patterns.back());
        assert(p->m_dir_rel.Empty());
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

        assert(p->m_patterns.size() == 1);
        AdjustPattern(p, regex, e);
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

    for (DirPattern* p = patterns; p; p = p->m_next)
    {
        p->m_ignore.emplace_back(std::move(MakeGlobs(p->m_dir.Text(), ignore_globs)));

        if (settings.IsSet(FMT_GITIGNORE))
            p->AddGitIgnore(p->m_dir.Text());
        if (settings.IsSet(FMT_GIT|FMT_GITREPOS))
            p->m_repo = GitStatus(p->m_dir.Text(), true, true);
    }

    return patterns;
}

void GlobPatterns::GlobPattern::Set(const WCHAR* p, size_t len)
{
    if (len == size_t(-1))
        len = wcslen(p);
    m_pattern.Set(p, len);
    Trim(m_pattern);
    p = m_pattern.Text();

    if (p[0] == '!')
        ++p;

    m_top_level = (p[0] == '/');
    m_any_level = true;
    m_multi_star_prefix_len = 0;

    // fnmatch() says "**/foo" matches "x/foo" but not "foo", so "**/" prefix
    // requires an extra call to fnmatch() with the "**/" prefix stripped.
    while (p[m_multi_star_prefix_len] == '*')
        m_multi_star_prefix_len++;
    if (p[m_multi_star_prefix_len] == '/')
        ++m_multi_star_prefix_len;
    if (m_multi_star_prefix_len < 3)
        m_multi_star_prefix_len = 0;

    // "**/foo" needs to use FNM_LEADING_DIR, so skip past the "**/" prefix so
    // m_anyLevel becomes true.
    p += m_multi_star_prefix_len;
    while (p[0])
    {
        // This triggers on "foo[!/]bar" BECAUSE empirical testing shows
        // gitignore does as well!  Since git accomplishes the "match at any
        // level" behavior through pre-processing, for compatibility the same
        // approach is used here (which certainly simplifies things).
        if (p[0] == '/' && p[1])
        {
            m_any_level = false;
            break;
        }

        ++p;
    }

    assert(!(m_top_level && m_any_level));
    assert(!(m_top_level && m_multi_star_prefix_len));
}

int GlobPatterns::GlobPattern::Flags() const
{
    return m_any_level ? WM_LEADING_DIR : 0;
}

GlobPatterns::GlobPatterns(int flags)
: m_flags(flags)
{
}

GlobPatterns::~GlobPatterns()
{
}

void GlobPatterns::SetRoot(const WCHAR* root)
{
#ifdef DEBUG
    m_root_initialized = true;
#endif
    m_root.Set(root);
}

bool GlobPatterns::IsApplicable(const WCHAR* dir) const
{
    if (wcsncmp(dir, m_root.Text(), m_root.Length()) != 0)
        return false;
    if (!IsPathSeparator(dir[m_root.Length()]))
        return false;
    return true;
}

bool GlobPatterns::IsMatch(const WCHAR* dir, const WCHAR* file) const
{
    bool result = false;
    StrW full;

    assert(dir);
    assert(wcsncmp(dir, m_root.Text(), m_root.Length()) == 0);
    if (wcsncmp(dir, m_root.Text(), m_root.Length()) != 0)
        return false;

    dir += m_root.Length();
    while (IsPathSeparator(*dir))
        ++dir;

    PathJoin(full, dir, file);

    for (const auto& pat : m_patterns)
    {
        const WCHAR* pattern = pat.Pattern().Text();

        if (!*pattern || *pattern == '#')
            continue;

        const bool negate = (*pattern == '!');
        if (negate)
            ++pattern;

        // Optimization:  if it's a match so far, only test negations; if it's
        // not a match so far, don't test negations.
        if (result == !negate)
            continue;

        const int flags = pat.Flags() | m_flags;

        // First try the pattern as is.  And if index_to_filename > 0 then
        // also try the pattern against the filename part, as long as the
        // pattern doesn't start with "/" or "*/"
        bool match = false;
        if (pat.IsTopLevel())
        {
            // If it's a top level pattern (starts with a slash) then only
            // match the beginning.
            match = (wildmatch(pattern + 1, full.Text(), flags) == 0);
        }
        else if (pat.IsAnyLevel())
        {
            // Patterns without a slash at the beginning or middle can match
            // any directory level.  The "**/" prefix is superfluous when
            // IsAnyLevel(), so skip past it if present so its slash doesn't
            // trigger a false mismatch (e.g. "**/foo" vs "foo").
            //
            // The implementation makes an optimization that relies on
            // matching while traversing the directory structure (vs matching
            // against a flat list of fully qualified file paths):  It only
            // matches against the last filename component, and traversing the
            // directory structure ends up making that have the same effect as
            // comparing to every filename component without needing to waste
            // computation time on redundant comparisons.
            match = (wildmatch(pattern + pat.MultiStarPrefix(), file, flags|WM_PATHNAME) == 0);
        }
        else
        {
            // Do normal matching.
            match = (wildmatch(pattern, full.Text(), flags) == 0);
        }

        if (match)
        {
            result = !negate;
            if (!m_any_negations)
                break;
        }
    }

    if (!result)
        return false;

    return true;
}

void GlobPatterns::SetPattern(size_t index, const WCHAR* p)
{
    GlobPattern& pat = m_patterns[index];
    const bool was_negate = pat.Pattern().Text()[0] == '!';
    const bool is_negate = p[0] == '!';

    pat.Set(p);

    UpdateAnyNegations(is_negate, was_negate);
}

void GlobPatterns::SwapWithNext(size_t index)
{
    if (index + 1 < m_patterns.size())
        std::swap(m_patterns[index], m_patterns[index + 1]);
}

void GlobPatterns::Insert(size_t index, const WCHAR* p)
{
    assert(m_root_initialized);

    if (index > m_patterns.size())
        index = m_patterns.size();

    StrW s;

    while (*p)
    {
        const WCHAR* next = p;
        while (*next)
        {
            if (next[0] == ';')
                break;
            s.Append(next[0]);
            if (next[0] == '\\' && next[1])
            {
                s.Append(next[1]);
                ++next;
            }
            ++next;
        }

        s.Set(p, next - p);

        GlobPattern pat;
        pat.Set(s.Text());
        m_patterns.insert(m_patterns.begin() + (index++), std::move(pat));
        s.Clear();

        p = next;
        if (*p == ';')
            ++p;
    }
}

bool GlobPatterns::Load(HANDLE h)
{
    assert(m_patterns.empty());

    DWORD bytes;
    DWORD size = GetFileSize(h, nullptr);
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(size);
    if (!ReadFile(h, buffer.get(), size, &bytes, 0 ) || bytes != size)
        return false;

    StrW line;
    const char* walk = static_cast<char*>(buffer.get());
    const char* end = static_cast<char*>(buffer.get()) + bytes;
    while (walk < end)
    {
        const char* start = walk;
        while (walk < end && *walk != '\r' && *walk != '\n')
            ++walk;

        const int needed = MultiByteToWideChar(CP_UTF8, 0, start, int(walk - start), nullptr, 0);
        if (needed > 0)
        {
            WCHAR* out = line.Reserve(needed + 1);
            MultiByteToWideChar(CP_UTF8, 0, start, int(walk - start), out, needed);
            out[needed] = '\0';

            GlobPattern glob;
            glob.Set(out, needed);
            m_patterns.emplace_back(std::move(glob));
        }

        if (*walk == '\r')
            ++walk;
        if (*walk == '\n')
            ++walk;
    }

    return true;
}

void GlobPatterns::Trim(StrW& s)
{
    const WCHAR* end = s.Text();

    for (const WCHAR* p = end; *p; ++p)
    {
        if (*p == ' ')
            continue;
        if (*p == '\\')
        {
            ++p;
            if (!*p)
            {
                end = p;
                break;
            }
        }
        end = p + 1;
    }

    s.SetEnd(end);
}

void GlobPatterns::UpdateAnyNegations(bool is_negate, bool was_negate)
{
    if (is_negate)
    {
        m_any_negations = true;
    }
    else if (was_negate && m_any_negations)
    {
        bool any = false;
        for (const auto& pat : m_patterns)
        {
            if (pat.Pattern().Text()[0] == '!')
            {
                any = true;
                break;
            }
        }
        m_any_negations = any;
    }
}

bool DirPattern::IsIgnore(const WCHAR* dir, const WCHAR* file) const
{
    if (!m_ignore.size())
        return false;
    for (const auto& ignore : m_ignore)
        if (ignore.IsMatch(dir, file))
            return true;
    return false;
}

void DirPattern::AddGitIgnore(const WCHAR* dir)
{
    StrW file(dir);
    EnsureTrailingSlash(file);
    file.Append(L".gitignore");
    SHFile h = CreateFile(file.Text(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, 0);
    if (!h.Empty())
    {
        GlobPatterns globs;
        if (globs.Load(h))
            m_ignore.emplace_back(std::move(globs));
    }
}
