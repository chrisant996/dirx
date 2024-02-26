// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "scan.h"
#include "fileinfo.h"
#include "flags.h"
#include "str.h"

#include <vector>

struct SubDir;
struct DirContext;

struct AttrChar
{
    WCHAR ch;
    DWORD dwAttr;
};

struct FieldInfo
{
    FieldType           m_field = FieldType(0);
    WCHAR               m_chSubField = 0;
    WCHAR               m_chStyle = 0;
    unsigned            m_cchWidth = 0;
    unsigned            m_ichInsert = 0;
    std::vector<AttrChar>* m_masks;
    bool                m_auto_filename_width = false;
    bool                m_auto_filesize_width = false;
};

class PictureFormatter
{
public:
    PictureFormatter(const DirFormatSettings& settings);

    void SetFitColumnsToContents(bool fit);
    void ParsePicture(const WCHAR* picture);

    void SetMaxFileDirWidth(unsigned max_file_width, unsigned max_dir_width);
    unsigned GetMaxWidth(unsigned fit_in_width=0, bool recalc_auto_width_fields=false);
    unsigned GetMinWidth(const FileInfo* pfi) const;
    bool CanAutoFitFilename() const;

    void SetDirContext(const std::shared_ptr<const DirContext>& dir);
    void OnFile(const FileInfo* pfi);

    void Format(StrW& s, const FileInfo* pfi, const FileInfo* stream=nullptr, bool one_per_line=true) const;

    bool IsImmediate() const { return m_immediate; }
    bool IsFilenameWidthNeeded() const { return m_need_filename_width; }
    bool IsCompressedSizeNeeded() const { return m_need_compressed_size; }
    bool AreShortFilenamesNeeded() const { return m_need_short_filenames; }
    bool HasDate() const { return m_has_date; }
    bool HasGit() const { return m_has_git; }

private:
    const DirFormatSettings& m_settings;
    std::shared_ptr<const DirContext> m_dir;

    StrW                m_orig_picture;
    StrW                m_picture;
    std::vector<FieldInfo> m_fields;
    unsigned            m_max_filepart_width = 0;
    unsigned            m_max_dirpart_width = 0;
    unsigned            m_max_branch_width = 0;
    std::vector<unsigned> m_max_filesize_width; // FUTURE: see notes in ParsePicture.
    unsigned            m_max_relative_width_which[TIMESTAMP_ARRAY_SIZE];
    unsigned            m_max_owner_width = 0;
    bool                m_immediate = true;
    bool                m_fit_columns_to_contents = false;
    bool                m_finished_initial_parse = false;
    bool                m_need_filename_width = false;
    bool                m_need_branch_width = false;
    bool                m_need_filesize_width = false;
    bool                m_need_owner_width = false;
    bool                m_need_relative_width = false;
    bool                m_need_relative_width_which[TIMESTAMP_ARRAY_SIZE];
    bool                m_need_compressed_size = false;
    bool                m_need_short_filenames = false;
    bool                m_any_repo_roots = false;
    bool                m_has_date = false;
    bool                m_has_git = false;
};

void InitLocale();
void SetCanAutoFit(bool can_autofit);
void SetConsoleWidth(unsigned long width);
void SetMiniBytes(bool mini_bytes);
void SetTruncationCharacterInHex(const WCHAR* s);
WCHAR GetTruncationCharacter();
bool SetDefaultSizeStyle(const WCHAR* size_style);
bool SetDefaultTimeStyle(const WCHAR* time_style);
void ClearDefaultTimeStyleIf(const WCHAR* time_style);
DWORD ParseAttribute(const WCHAR ch);

bool SetUseIcons(const WCHAR* s);
void SetPadIcons(unsigned spaces);
unsigned GetPadIcons();
unsigned GetIconWidth();

enum ColorScaleFields { SCALE_NONE = 0, SCALE_TIME = 1<<0, SCALE_SIZE = 1<<1, };
DEFINE_ENUM_FLAG_OPERATORS(ColorScaleFields);

bool SetColorScale(const WCHAR* s);
ColorScaleFields GetColorScaleFields();
bool SetColorScaleMode(const WCHAR* s);
bool IsGradientColorScaleMode();

const WCHAR* SelectColor(const FileInfo* const pfi, const FormatFlags flags, const WCHAR* dir, bool ignore_target_color=false);
unsigned GetSizeFieldWidthByStyle(const DirFormatSettings& settings, WCHAR chStyle);
void FormatSizeForReading(StrW& s, unsigned __int64 cbSize, unsigned field_width, const DirFormatSettings& settings);
void FormatSize(StrW& s, unsigned __int64 cbSize, const WhichFileSize* which, const DirFormatSettings& settings, WCHAR chStyle, unsigned max_width=0, const WCHAR* color=nullptr, const WCHAR* fallback_color=nullptr, bool nocolor=false);
void FormatCompressed(StrW& s, const unsigned __int64 cbCompressed, const unsigned __int64 cbFile, const DWORD dwAttr);
void FormatFilename(StrW& s, const FileInfo* pfi, FormatFlags flags, unsigned max_width=0, const WCHAR* dir=nullptr, const WCHAR* color=nullptr, bool show_reparse=false);

