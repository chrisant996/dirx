// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "scan.h"
#include "fileinfo.h"
#include "flags.h"
#include "str.h"
#include "fields.h"

#include <vector>

struct SubDir;

struct DirContext
{
                        DirContext(FormatFlags flags, const std::shared_ptr<PictureFormatter>& picture) : flags(flags), picture(picture) {}
    StrW                dir;
    StrW                dir_rel;                // dir, but relative to the original pattern.
    FormatFlags         flags;
    std::shared_ptr<PictureFormatter> picture;
    std::shared_ptr<const RepoStatus> repo;
};

class OutputOperation
{
public:
                        OutputOperation() = default;
    virtual             ~OutputOperation() = default;
    virtual void        Render(HANDLE h, const DirContext* dir) = 0;
};

class DirEntryFormatter : public DirScanCallbacks
{
    typedef DirScanCallbacks base;

public:
                        DirEntryFormatter();
                        ~DirEntryFormatter();

    void                Initialize(unsigned cColumns, FormatFlags flags,
                                   WhichTimeStamp whichtimestamp=TIMESTAMP_MODIFIED,
                                   WhichFileSize whichfilesize=FILESIZE_FILESIZE,
                                   DWORD dwAttrIncludeAny=0, DWORD dwAttrMatch=0, DWORD dwAttrExcludeAny=0,
                                   const WCHAR* picture=nullptr);
    void                SetFitColumnsToContents(bool fit) { m_picture_template->SetFitColumnsToContents(fit); }

    DirFormatSettings&  Settings() override { return m_settings; }

    bool                OnVolumeBegin(const WCHAR* dir, Error& e) override;
    void                OnPatterns(bool grouped) override;
    void                OnScanFiles(const WCHAR* dir, const WCHAR* pattern, bool implicit, bool root_pass) override;
    void                OnDirectoryBegin(const WCHAR* dir, const WCHAR* dir_rel, const std::shared_ptr<const RepoStatus>& repo) override;
    void                OnFile(const WCHAR* dir, const WIN32_FIND_DATA* pfd) override;
    void                OnDirectoryEnd(const WCHAR* dir, bool next_dir_is_different) override;
    void                OnPatternEnd(const DirPattern* pattern) override;
    void                OnVolumeEnd(const WCHAR* dir) override;
    void                Finalize() override;
    void                ReportError(Error& e) override;
    void                AddSubDir(const StrW& dir, const StrW& dir_rel, unsigned depth, const std::shared_ptr<const GlobPatterns>& git_ignore, const std::shared_ptr<const RepoStatus>& repo) override;
    void                SortSubDirs() override;
    bool                NextSubDir(StrW& dir, StrW& dir_rel, unsigned& depth, std::shared_ptr<const GlobPatterns>& git_ignore, std::shared_ptr<const RepoStatus>& repo) override;
    unsigned            CountFiles() const override { return m_cFiles; }
    unsigned            CountDirs() const override { return m_cDirs; }
    bool                IsOnlyRootSubDir() const override;
    bool                IsRootSubDir() const override;

    bool                IsNewRootGroup(const WCHAR* dir) const;
    void                UpdateRootGroup(const WCHAR* dir);

private:
    bool                IsDelayedRender() const { return m_delayed_render; }
    void                Render(OutputOperation* o);

private:
    HANDLE              m_hout = 0;
    DirFormatSettings   m_settings;
    std::shared_ptr<PictureFormatter> m_picture_template;
    StrW                m_sColor;
    bool                m_fImmediate = true;
    bool                m_delayed_render = false;

    bool                m_line_break_before_volume = false;
    bool                m_line_break_before_miniheader = false;

    unsigned            m_cFiles = 0;
    unsigned            m_cDirs = 0;
    unsigned __int64    m_cbTotal = 0;
    unsigned __int64    m_cbAllocated = 0;
    unsigned __int64    m_cbCompressed = 0;
    unsigned            m_longest_file_width = 0;
    unsigned            m_longest_dir_width = 0;
    DWORD               m_granularity = 0;

    unsigned            m_cFilesTotal = 0;
    unsigned __int64    m_cbTotalTotal = 0;
    unsigned __int64    m_cbAllocatedTotal = 0;
    unsigned __int64    m_cbCompressedTotal = 0;

    std::vector<std::unique_ptr<FileInfo>> m_files;
    std::vector<SubDir> m_subdirs;
    StrW                m_root;
    StrW                m_root_group;
    bool                m_implicit = false;
    bool                m_root_pass = false;
    bool                m_grouped_patterns = false;
    unsigned            m_count_usage_dirs = 0;

    std::shared_ptr<DirContext> m_dir;

    std::vector<std::unique_ptr<OutputOperation>> m_outputs;
};

std::shared_ptr<const RepoStatus> FindRepo(const WCHAR* dir);
void AppendTreeLines(StrW& s, const FormatFlags flags);
bool IsTreeRoot();
