// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "scan.h"
#include "fileinfo.h"
#include "flags.h"
#include "str.h"

#include <vector>

struct AttrChar
{
    DWORD dwAttr;
    WCHAR ch;
};

struct FieldInfo
{
    FieldType           m_field;
    WCHAR               m_chSubField;
    WCHAR               m_chStyle;
    unsigned            m_cchWidth;
    unsigned            m_ichInsert;
    std::vector<AttrChar>* m_masks;
    bool                m_auto_width;
};

class PictureFormatter
{
public:
    PictureFormatter(const DirFormatSettings& settings);

    void ParsePicture(const WCHAR* picture);

    void SetMaxFileDirWidth(unsigned max_file_width, unsigned max_dir_width);
    unsigned GetMaxWidth(unsigned fit_in_width=0, bool recalc_auto_width_fields=false);
    unsigned GetMinWidth(const FileInfo* pfi) const;
    bool CanAutoFitWidth() const;

    void Format(StrW& s, const FileInfo* pfi, const WCHAR* dir=nullptr, const WIN32_FIND_STREAM_DATA* pfsd=nullptr, bool one_per_line=true) const;

    bool IsFilenameWidthNeeded() const { return m_need_filename_width; }
    bool IsCompressedSizeNeeded() const { return m_need_compressed_size; }
    bool AreShortFilenamesNeeded() const { return m_need_short_filenames; }
    bool HasDate() const { return m_fHasDate; }

private:
    const DirFormatSettings& m_settings;
    StrW                m_picture;
    std::vector<FieldInfo> m_fields;
    unsigned            m_max_file_width = 0;
    unsigned            m_max_dir_width = 0;
    bool                m_need_filename_width = false;
    bool                m_need_compressed_size = false;
    bool                m_need_short_filenames = false;
    bool                m_fHasDate = false;
};

class DirEntryFormatter : public DirScanCallbacks
{
    typedef DirScanCallbacks base;

public:
                        DirEntryFormatter();
                        ~DirEntryFormatter();

    void                Initialize(unsigned cColumns, DWORD flags, WhichTimeStamp whichtimestamp=TIMESTAMP_MODIFIED, WhichFileSize whichfilesize=FILESIZE_FILESIZE, DWORD dwAttrIncludeAny=0, DWORD dwAttrMatch=0, DWORD dwAttrExcludeAny=0, const WCHAR* disable_options=nullptr, const WCHAR* picture=nullptr);

    void                DisplayOne(const FileInfo* pfi);

    DirFormatSettings&  Settings() override { return m_settings; }

    bool                OnVolumeBegin(const WCHAR* dir, Error& e) override;
    void                OnScanFiles(const WCHAR* dir, const WCHAR* pattern, bool implicit, bool root_pass) override;
    void                OnDirectoryBegin(const WCHAR* dir) override;
    void                OnFile(const WCHAR* dir, const WIN32_FIND_DATA* pfd) override;
    void                OnFileNotFound() override;
    void                OnDirectoryEnd(bool next_dir_is_different) override;
    void                OnVolumeEnd(const WCHAR* dir) override;
    void                AddSubDir(const StrW& dir) override;
    void                SortSubDirs() override;
    bool                NextSubDir(StrW& dir) override;
    unsigned            CountFiles() const override { return m_cFiles; }
    unsigned            CountDirs() const override { return m_cDirs; }
    bool                IsOnlyRootSubDir() const override;
    bool                IsRootSubDir() const override;

    bool                IsNewRootGroup(const WCHAR* dir) const;
    void                UpdateRootGroup(const WCHAR* dir);

private:
    HANDLE              m_hout = 0;
    DirFormatSettings   m_settings;
    PictureFormatter    m_picture;
    StrW                m_sColor;

    bool                m_line_break_before_volume = false;

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

    std::vector<FileInfo> m_files;
    std::vector<StrW>   m_subdirs;
    StrW                m_root;
    StrW                m_root_group;
    bool                m_implicit = false;
    bool                m_root_pass = false;
    unsigned            m_count_usage_dirs = 0;

    StrW                m_dir;
};

void InitLocale();
void SetCanAutoFit(bool can_autofit);
void SetConsoleWidth(unsigned long width);
bool SetUseIcons(const WCHAR* s);
void SetPadIcons(unsigned spaces);
unsigned GetPadIcons();
bool SetColorScale(const WCHAR* s);
bool IsScalingSize();
bool SetColorScaleMode(const WCHAR* s);
void SetTruncationCharacter(WCHAR ch);
WCHAR GetTruncationCharacter();
DWORD ParseAttribute(const WCHAR ch);

