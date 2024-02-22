// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "options.h"

enum FormatFlags : ULONGLONG
{
    FMT_NONE                    = 0x0000000000000000,

    FMT_COLORS                  = 0x0000000000000001,
    FMT_REDIRECTED              = 0x0000000000000002,
    FMT_BARE                    = 0x0000000000000004,   // Prevents more than 1 column, suppresses all columns other than name.
    FMT_SUBDIRECTORIES          = 0x0000000000000008,
    FMT_ATTRIBUTES              = 0x0000000000000010,   // Prevents more than 1 column.
    FMT_ALLATTRIBUTES           = 0x0000000000000020,   // Append type symbol after file name (\ for dir, @ for symlink file).
    FMT_FAT                     = 0x0000000000000040,
    FMT_FORCENONFAT             = 0x0000000000000080,   // Force normal list format even on FAT volumes.
    FMT_JUSTIFY_FAT             = 0x0000000000000100,
    FMT_JUSTIFY_NONFAT          = 0x0000000000000200,
    FMT_SHORTNAMES              = 0x0000000000000400,   // Include short name (or in formats where only one name column is possible then show the short name instead of the long name).
    FMT_ONLYSHORTNAMES          = 0x0000000000000800,   // Show the short name, even if it is blank.
    FMT_FULLNAME                = 0x0000000000001000,   // Show the fully qualified path in the file name column.
    FMT_HYPERLINKS              = 0x0000000000002000,   // Add hyperlink escape codes for file and directory names.
    FMT_LOWERCASE               = 0x0000000000004000,
    FMT_HIDEPSEUDODIRS          = 0x0000000000008000,   // Hide the "." and ".." directories.
    FMT_DIRBRACKETS             = 0x0000000000010000,
    FMT_CLASSIFY                = 0x0000000000020000,   // Append type symbol after file name (\ for dir, @ for symlink file).
    FMT_SIZE                    = 0x0000000000040000,   // Include size even in multi-column formats.
    FMT_MINISIZE                = 0x0000000000080000,   // Shows abbreviated (4 chars) size when more than 1 column.
    FMT_FULLSIZE                = 0x0000000000100000,   // Always show size in bytes (e.g. "1,234,567" not "1.2M").
    FMT_DATE                    = 0x0000000000200000,   // Include time even in multi-column formats.
    FMT_MINIDATE                = 0x0000000000400000,   // Shows abbreviated (11 chars) time when more than 1 column.
    FMT_FULLTIME                = 0x0000000000800000,   // Include seconds and milliseconds, plus four digit year.
    FMT_ALTDATASTEAMS           = 0x0000000001000000,
    FMT_ONLYALTDATASTREAMS      = 0x0000000002000000,   // Skip files that do not have any alternate data streams (this also works with FMT_BARE, but in that case does not list the streams).
    FMT_COMPRESSED              = 0x0000000004000000,   // Include compression ratio.
    FMT_SHOWOWNER               = 0x0000000008000000,   // Include file owner.
    FMT_SEPARATETHOUSANDS       = 0x0000000010000000,
    FMT_AUTOSEPTHOUSANDS        = 0x0000000020000000,   // If -, and -,- are not used then FMT_FULLSIZE|FMT_SEPARATETHOUSANDS are used iff the 1 column format is used.
    FMT_NODIRTAGINSIZE          = 0x0000000040000000,   // Show - instead of <D> or <J> in size column.
    //                          = 0x0000000080000000,
    FMT_SORTVERTICAL            = 0x0000000100000000,
    //                          = 0x0000000200000000,
    FMT_USAGE                   = 0x0000000400000000,
    FMT_USAGEGROUPED            = 0x0000000800000000,   // Show directory usage statistics grouped by top level directories.
    FMT_SKIPHIDDENDIRS          = 0x0000001000000000,
    FMT_SKIPJUNCTIONS           = 0x0000002000000000,
    FMT_NOVOLUMEINFO            = 0x0000004000000000,
    FMT_NOHEADER                = 0x0000008000000000,
    FMT_NOSUMMARY               = 0x0000010000000000,
    FMT_MINIHEADER              = 0x0000020000000000,   // Show single line header per directory.
    //                          = 0x0000040000000000,
    //                          = 0x0000080000000000,
    FMT_LONGNODATE              = 0x0000100000000000,   // Omit date in long mode.
    FMT_LONGNOSIZE              = 0x0000200000000000,   // Omit size in long mode.
    FMT_LONGNOATTRIBUTES        = 0x0000400000000000,   // Omit attributes in long mode.
};
DEFINE_ENUM_FLAG_OPERATORS(FormatFlags);

void FlipFlag(FormatFlags& flags, FormatFlags flag, bool& enable, bool default_enable);
void FailFlag(WCHAR ch, const WCHAR* value, WCHAR short_opt, const LongOption<WCHAR>* long_opt, Error& e);

enum FieldType
{
    FLD_DATETIME,               // Access, creation, write.
    FLD_FILESIZE,               // File size, allocation size, compressed size.
    FLD_COMPRESSION,            // Compression ratio.
    FLD_ATTRIBUTES,             // Attributes.
    FLD_OWNER,                  // Owner name.
    FLD_SHORTNAME,              // Short name, or empty if short name == file name.
    FLD_FILENAME,               // File name or short name, depending on flags/etc.
};

enum WhichTimeStamp
{
    TIMESTAMP_ACCESS,
    TIMESTAMP_CREATED,
    TIMESTAMP_MODIFIED,
    TIMESTAMP_ARRAY_SIZE
};

enum WhichFileSize
{
    FILESIZE_ALLOCATION,
    FILESIZE_COMPRESSED,
    FILESIZE_FILESIZE,
    FILESIZE_ARRAY_SIZE
};

inline ULONGLONG FileTimeToULONGLONG(const FILETIME& ft)
{
    return *reinterpret_cast<const ULONGLONG*>(&ft);
}

struct DirFormatSettings
{
    bool                IsSet(FormatFlags flag) const { return !!(m_flags & flag); }
    void                ClearMinMax();
    void                UpdateMinMaxTime(WhichTimeStamp which, const FILETIME& ft);
    void                UpdateMinMaxSize(WhichFileSize which, ULONGLONG size);

    FormatFlags         m_flags = FMT_NONE;
    WhichTimeStamp      m_whichtimestamp = TIMESTAMP_MODIFIED;
    WhichFileSize       m_whichfilesize = FILESIZE_FILESIZE;
    unsigned            m_num_columns = 1;
    DWORD               m_dwAttrIncludeAny = 0;
    DWORD               m_dwAttrMatch = 0;
    DWORD               m_dwAttrExcludeAny = 0;
    bool                m_need_compressed_size = false;
    bool                m_need_short_filenames = false;

    ULONGLONG           m_min_time[TIMESTAMP_ARRAY_SIZE];
    ULONGLONG           m_max_time[TIMESTAMP_ARRAY_SIZE];
    ULONGLONG           m_min_size[FILESIZE_ARRAY_SIZE];
    ULONGLONG           m_max_size[FILESIZE_ARRAY_SIZE];
};

void SkipColonOrEqual(const WCHAR*& p);

extern const DirFormatSettings* g_settings;

