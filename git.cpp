// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "git.h"
#include "filesys.h"
#include "output.h"

RepoStatus::~RepoStatus()
{
    for (const auto& s : status)
        free(const_cast<WCHAR*>(s.first));
}

static bool IsUncPath(const WCHAR* p, const WCHAR** past_unc)
{
    if (!IsPathSeparator(p[0]) || !IsPathSeparator(p[1]))
        return false;

    const WCHAR* const in = p;
    while (IsPathSeparator(*p))
        ++p;
    unsigned leading = unsigned(p - in);

    // Check for device namespace.
    if (p[0] == '.' && (!p[1] || IsPathSeparator(p[1])))
        return false;

    // Check for \\?\UNC\ namespace.
    if (leading == 2 && p[0] == '?' && IsPathSeparator(p[1]))
    {
        ++p;
        while (IsPathSeparator(*p))
            ++p;

        if (*p != 'U' && *p != 'u')
            return false;
        ++p;
        if (*p != 'N' && *p != 'n')
            return false;
        ++p;
        if (*p != 'C' && *p != 'c')
            return false;
        ++p;

        if (!IsPathSeparator(*p))
            return false;
        while (IsPathSeparator(*p))
            ++p;
    }

    if (past_unc)
    {
        // Skip server name.
        while (*p && !IsPathSeparator(*p))
            ++p;

        // Skip separator.
        while (IsPathSeparator(*p))
            ++p;

        // Skip share name.
        while (*p && !IsPathSeparator(*p))
            ++p;

        *past_unc = p;
    }
    return true;
}

static unsigned PastSsqs(const WCHAR* p)
{
    const WCHAR* const orig = p;
    if (!IsPathSeparator(*(p++)))
        return 0;
    if (!IsPathSeparator(*(p++)))
        return 0;
    if (*(p++) != '?')
        return 0;
    if (!IsPathSeparator(*(p++)))
        return 0;
    while (IsPathSeparator(*p))
        ++p;
    return unsigned(p - orig);
}

static const WCHAR* PastDrive(const WCHAR* p)
{
    if (!IsUncPath(p, &p))
    {
        p += PastSsqs(p);
        if (iswalpha(p[0]) && p[1] == ':')
            p += 2;
    }
    return p;
}

static bool PathToParent(StrW& out, StrW* child=nullptr)
{
    unsigned orig_len = out.Length();

    // Find end of drive or UNC root plus separator(s).
    unsigned start = unsigned(PastDrive(out.Text()) - out.Text());
    if (start && out.Text()[start - 1] == ':')
    {
        while (IsPathSeparator(out.Text()[start]))
            ++start;
    }

    // Trim separators at the end.
    unsigned end = out.Length();
    while (end > 0 && IsPathSeparator(out.Text()[end - 1]))
        --end;

    // Trim the last path component.
    unsigned child_end = end;
    while (end > 0 && !IsPathSeparator(out.Text()[end - 1]))
        --end;
    if (end < start)
        end = start;

    // Return the last path component.
    if (child)
        child->Set(out.Text() + end, child_end - end);

    // Trim trailing separators.
    while (end > start && IsPathSeparator(out.Text()[end - 1]))
        --end;

    out.SetLength(end);
    return (out.Length() != orig_len);
}

bool IsUnderRepo(const WCHAR* _dir, StrW& root)
{
    StrW dir(_dir);
    StrW git_dir(_dir);
    while (true)
    {
        git_dir.SetLength(dir.Length());
        EnsureTrailingSlash(git_dir);
        git_dir.Append(L".git");
        if (IsDir(git_dir.Text()))
        {
            root.Set(dir);
            return true;
        }

        if (!PathToParent(dir))
            return false;
    }
}

GitFileState CharToState(char ch)
{
    switch (ch)
    {
    case '?':   return GitFileState::NEW;
    case 'A':   return GitFileState::NEW;
    case 'M':   return GitFileState::MODIFIED;
    case 'T':   return GitFileState::TYPECHANGE;
    case 'D':   return GitFileState::DELETED;
    case 'R':   return GitFileState::RENAMED;
    case 'C':   return GitFileState::NEW;
    case '!':   return GitFileState::IGNORED;
    case 'U':   return GitFileState::UNMERGED;
    default:    return GitFileState::NONE;
    }

}

std::shared_ptr<RepoStatus> GitStatus(const WCHAR* _dir, bool need_ignored, bool walk_up)
{
    std::shared_ptr<RepoStatus> status = std::make_shared<RepoStatus>();

    StrW root;
    StrW git_dir;
    if (walk_up)
    {
        if (!IsUnderRepo(_dir, root))
        {
failed:
            return status;
        }

        need_ignored = true;
        PathJoin(git_dir, root.Text(), L".git");
    }
    else
    {
        PathJoin(git_dir, _dir, L".git");
        if (!IsDir(git_dir.Text()))
            goto failed;

        root.Set(_dir);
    }

    if (g_debug)
        Printf(L"debug: git status in '%s'%s\n", root.Text(), need_ignored ? L", plus ignored files" : L"");

    // Prevent GVFS from printing progress gibberish even while output is
    // redirected to a pipe.
    SetEnvironmentVariable(L"GVFS_UNATTENDED", L"1");

    StrW command;
    command.Printf(L"2>nul git.exe --work-tree=\"%s\" --git-dir=\"%s\" status --porcelain --no-ahead-behind -unormal --branch %s",
                    root.Text(), git_dir.Text(), need_ignored ? L"--ignored" : L"");

    FILE* pipe = _wpopen(command.Text(), L"rt");
    if (!pipe)
        goto failed;

    const size_t buffer_size = 8192;
    char* buffer = new char[buffer_size];

    StrW wide;
    wide.Reserve(buffer_size);

    while (fgets(buffer, buffer_size, pipe))
    {
        char* eos = buffer + strlen(buffer);
        while (--eos >= buffer)
        {
            if (*eos != '\r' && *eos != '\n')
                break;
            *eos = '\0';
        }

        if (buffer[0] == '#' && buffer[1] == '#' && buffer[2] == ' ')
        {
            if (status->branch.Empty())
            {
                MultiByteToWideChar(CP_UTF8, 0, buffer + 3, -1, wide.Reserve(), wide.Capacity());
                wide.ResyncLength();
                if (wide.EqualI(L"HEAD (no branch)"))
                    wide.Set(L"HEAD");
                else if (wcsncmp(wide.Text(), L"No commits yet on ", 18) == 0)
                    wide.Clear();
                else
                {
                    const WCHAR* end = wcsstr(wide.Text(), L"...");
                    if (end)
                        wide.SetEnd(end);
                }
                status->branch.Set(wide.Text());
            }
            continue;
        }

        if (buffer[0] && buffer[1] && buffer[2] == ' ')
        {
            StrW name;
            FileStatus filestatus;
            filestatus.staged = CharToState(buffer[0]);
            filestatus.working = CharToState(buffer[1]);
            switch (filestatus.working)
            {
            case GitFileState::NEW:
                switch (filestatus.staged)
                {
                case GitFileState::NEW:
                    filestatus.staged = GitFileState::NONE;
                    break;
                }
                break;
            case GitFileState::IGNORED:
                switch (filestatus.staged)
                {
                case GitFileState::IGNORED:
                    filestatus.staged = GitFileState::NONE;
                    break;
                }
                break;
            }

            const char* parse = buffer + 3;
            if (filestatus.staged == GitFileState::RENAMED)
            {
                const char* arrow = strstr(parse, " -> ");
                if (arrow)
                    parse = arrow + 4;
            }

            const bool quoted = (*parse == '\"');
            if (quoted)
                ++parse;
            MultiByteToWideChar(CP_UTF8, 0, parse, -1, wide.Reserve(), wide.Capacity());
            name.Set(wide.Text());
            if (quoted)
            {
                const WCHAR* end = wcschr(name.Text(), '\"');
                if (!end)
                    continue;
                name.SetEnd(end);
            }
            else
            {
                const WCHAR* space = wcschr(name.Text(), ' ');
                if (space)
                    name.SetEnd(space);
            }

            StrW full;
            PathJoin(full, root.Text(), name);
            for (WCHAR* walk = full.Reserve(); *walk; ++walk)
                if (*walk == '/')
                    *walk = '\\';
            StripTrailingSlashes(full);

            status->status.emplace(full.Detach(), filestatus);
        }
    }

    _pclose(pipe);
    free(buffer);

    status->repo = true;
    status->clean = status->status.empty();
    status->main = status->branch.Equal(L"main") || status->branch.Equal(L"master");
    status->root.Set(root);

    // Finally (AFTER setting status->clean), add an implicit ignore entry for
    // the .git directory.
    FileStatus filestatus;
    filestatus.staged = GitFileState::NONE;
    filestatus.working = GitFileState::IGNORED;
    status->status.emplace(git_dir.Detach(), filestatus);

    return status;
}

void RepoMap::Add(std::shared_ptr<const RepoStatus> repo)
{
    if (repo)
        m_map.emplace(repo->root.Text(), repo);
}

void RepoMap::Remove(const WCHAR* dir)
{
    const auto& iter = m_map.find(dir);
    if (iter != m_map.end())
        m_map.erase(iter);
}

std::shared_ptr<const RepoStatus> RepoMap::Find(const WCHAR* dir) const
{
    const auto& iter = m_map.find(dir);
    if (iter == m_map.end())
        return nullptr;
    return iter->second;
}

