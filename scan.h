// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <windows.h>
#include "flags.h"
#include "patterns.h"

#include <memory>

struct DirPattern;
class Error;

class DirScanCallbacks
{
public:
    virtual DirFormatSettings& Settings() = 0;

    virtual bool        OnVolumeBegin(const WCHAR* dir, Error& e) = 0;
    virtual void        OnPatterns(bool grouped) = 0;
    virtual void        OnScanFiles(const WCHAR* dir, bool implicit, bool top) = 0;
    virtual void        OnDirectoryBegin(const WCHAR* dir, const WCHAR* dir_rel, const std::shared_ptr<const RepoStatus>& repo) = 0;
    virtual void        OnFile(const WCHAR* dir, const WIN32_FIND_DATA* pfd) = 0;
    virtual void        OnDirectoryEnd(const WCHAR* dir, bool next_is_different) = 0;
    virtual void        OnPatternEnd(const DirPattern* pattern) = 0;
    virtual void        OnVolumeEnd(const WCHAR* dir) = 0;
    virtual void        Finalize() = 0;
    virtual void        ReportError(Error& e) = 0;
    virtual void        AddSubDir(const StrW& dir, const StrW& dir_rel, unsigned depth, const std::shared_ptr<const GlobPatterns>& git_ignore, const std::shared_ptr<const RepoStatus>& repo) = 0;
    virtual void        SortSubDirs() = 0;
    virtual bool        NextSubDir(StrW& dir, StrW& dir_rel, unsigned& depth, std::shared_ptr<const GlobPatterns>& git_ignorem, std::shared_ptr<const RepoStatus>& repo) = 0;
    virtual unsigned    CountFiles() const = 0;
    virtual unsigned    CountDirs() const = 0;
    virtual bool        IsOnlyRootSubDir() const = 0;
    virtual bool        IsRootSubDir() const = 0;
};

int ScanDir(DirScanCallbacks& callbacks, const DirPattern* patterns, unsigned limit_depth, Error& e);

