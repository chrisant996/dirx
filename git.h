// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <windows.h>
#include "str.h"

#include <map>
#include <memory>

enum class GitFileState : BYTE
{
    NONE,

    NEW,
    MODIFIED,
    DELETED,
    RENAMED,
    TYPECHANGE,
    IGNORED,
    UNMERGED,           // Conflicted.

    COUNT
};

struct FileStatus
{
    GitFileState        staged;
    GitFileState        working;
};

struct RepoStatus
{
                        ~RepoStatus();

    bool                repo = false;
    bool                main = true;    // Branch is main or master.
    bool                clean = true;
    StrW                branch;
    StrW                root;
    std::map<const WCHAR*, FileStatus, SortCase> status;
};

struct GitStatusSymbol
{
    WCHAR               symbol;
    WCHAR               color_key[3];
};

bool IsUnderRepo(const WCHAR* dir);
std::shared_ptr<RepoStatus> GitStatus(const WCHAR* dir, bool need_ignored, bool walk_up=false);
const GitStatusSymbol& GitSymbol(GitFileState state);

class RepoMap
{
public:
    void                Add(std::shared_ptr<const RepoStatus> repo);
    void                Remove(const WCHAR* dir);
    std::shared_ptr<const RepoStatus> Find(const WCHAR* dir) const;
private:
    std::map<const WCHAR*, std::shared_ptr<const RepoStatus>, SortCaseless> m_map;
};
