// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <windows.h>
#include "str.h"
#include "wildmatch/wildmatch.h"
#include <vector>
#include <memory>

struct DirFormatSettings;
class Error;

class GlobPatterns
{
    struct GlobPattern
    {
    public:
        void            Set(const WCHAR* p, size_t len=-1);

        const StrW&     Pattern() const { return m_pattern; }
        int             Flags() const;

        bool            IsTopLevel() const { return m_top_level; }
        bool            IsAnyLevel() const { return m_any_level; }
        size_t          MultiStarPrefix() const { return m_multi_star_prefix_len; }

    private:
        StrW            m_pattern;
        bool            m_top_level;    // Match relative to the start of the compared path.
        bool            m_any_level;    // m_pattern covers only one pathname component, so match with the filename part.
        BYTE            m_multi_star_prefix_len;    // If > 0 then m_pattern starts with two or more stars followed by a slash.
                                                    // (Omits any '!' negation character.)
    };

public:
                        GlobPatterns(int flags=WM_WILDSTAR|WM_SLASHFOLD|WM_CASEFOLD);
                        ~GlobPatterns();

    void                SetRoot(const WCHAR* root);
    bool                IsApplicable(const WCHAR* root) const;
    bool                IsMatch(const WCHAR* dir, const WCHAR* file) const;

    size_t              Count() const { return m_patterns.size(); }
    const StrW&         GetPattern(size_t index) const { return m_patterns[index].Pattern(); }
    void                SetPattern(size_t index, const WCHAR* p);
    void                SwapWithNext(size_t index);
    void                Insert(size_t index, const WCHAR* p);
    void                Append(const WCHAR* p) { Insert(-1, p); }

    bool                Load(HANDLE h);

    static void         Trim(StrW& s);

private:
    bool                TestForAnyLevel(const WCHAR* pattern);
    void                UpdateAnyNegations(bool is_negate, bool was_negate);

private:
    std::vector<GlobPattern> m_patterns;
    StrW                m_root;
    int                 m_flags;
    bool                m_any_negations = false;
#ifdef DEBUG
    bool                m_root_initialized = false;
#endif
};

struct SubDir
{
    StrW                dir;
    unsigned            depth;
    std::shared_ptr<GlobPatterns> git_ignore;
};

struct DirPattern
{
                        DirPattern() : m_implicit(false), m_next(nullptr) {}

    bool                IsIgnore(const WCHAR* dir, const WCHAR* file) const;
    void                AddGitIgnore(const WCHAR* dir);

    std::vector<StrW>   m_patterns;
    std::vector<GlobPatterns> m_ignore;
    StrW                m_dir;
    bool                m_isFAT;
    bool                m_implicit;
    unsigned            m_depth = 0;

    DirPattern*         m_next;
};

const WCHAR* FindExtension(const WCHAR* file);
const WCHAR* FindName(const WCHAR* file);

DirPattern* MakePatterns(int argc, const WCHAR** argv, const DirFormatSettings& settings, const WCHAR* ignore_globs, Error& e);

