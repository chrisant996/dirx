// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

enum
{
    FMT_JUSTIFY_NONFAT          = 0x00000001,
    FMT_ATTRIBUTES              = 0x00000002,   // Prevents more than 1 column.
    FMT_FAT                     = 0x00000004,
    FMT_MINISIZE                = 0x00000008,   // Shows abbreviated (4 chars) size when more than 1 column.
    FMT_DISABLECOLORS           = 0x00000010,
    FMT_LOWERCASE               = 0x00000020,
    FMT_JUSTIFY_FAT             = 0x00000040,
    FMT_BARE                    = 0x00000080,   // Prevents more than 1 column, suppresses all columns other than name.
    FMT_DIRBRACKETS             = 0x00000100,
    FMT_WIDELISTIME             = 0x00000200,   // Include time in wide list format.
    FMT_FULLTIME                = 0x00000400,   // Include seconds and milliseconds, plus four digit year.
    FMT_FULLSIZE                = 0x00000800,   // Always show size in bytes (e.g. "1,234,567" not "1.2M").
    FMT_SHORTNAMES              = 0x00001000,   // Include short name (or in formats where only one name column is possible then show the short name instead of the long name).
    FMT_COMPRESSED              = 0x00002000,   // Include compression ratio.
    FMT_SEPARATETHOUSANDS       = 0x00004000,
    FMT_FORCENONFAT             = 0x00008000,   // Force normal list format even on FAT volumes.
    FMT_SORTVERTICAL            = 0x00010000,
    FMT_SUBDIRECTORIES          = 0x00020000,
    FMT_SHOWOWNER               = 0x00040000,   // Include file owner.
    FMT_ALTDATASTEAMS           = 0x00080000,
    FMT_REDIRECTED              = 0x00100000,
    FMT_ONLYALTDATASTREAMS      = 0x00200000,   // Skip files that do not have any alternate data streams (this also works with FMT_BARE, but in that case does not list the streams).
    FMT_HIDEDOTS                = 0x00400000,   // Hide the "." and ".." directories.
    FMT_AUTOSEPTHOUSANDS        = 0x00800000,   // If -, and -,- are not used then FMT_FULLSIZE|FMT_SEPARATETHOUSANDS are used iff the 1 column format is used.
    FMT_ONLYSHORTNAMES          = 0x01000000,   // Show the short name, even if it is blank.
    FMT_USAGE                   = 0x02000000,
    FMT_USAGEGROUPED            = 0x04000000,   // Show directory usage statistics grouped by top level directories.
    FMT_MINIDATE                = 0x08000000,   // Shows abbreviated (11 chars) time when more than 1 column.
    FMT_LOCALEDATETIME          = 0x10000000,   // Use current locale date and time format.
    FMT_FULLNAME                = 0x20000000,   // Show the fully qualified path in the file name column.
    FMT_HYPERLINKS              = 0x40000000,   // Add hyperlink escape codes for file and directory names.
    FMT_CLASSIFY                = 0x80000000,   // Append type symbol after file name (\ for dir, @ for symlink file).
};

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
    bool                IsSet(int flag) const { return !!(m_flags & flag); }
    bool                IsOptionDisabled(WCHAR ch) const { return !!wcschr(m_sDisableOptions.Text(), ch); }
    void                ClearMinMax();
    void                UpdateMinMaxTime(WhichTimeStamp which, const FILETIME& ft);
    void                UpdateMinMaxSize(WhichFileSize which, ULONGLONG size);

    DWORD               m_flags = 0;
    WhichTimeStamp      m_whichtimestamp = TIMESTAMP_MODIFIED;
    WhichFileSize       m_whichfilesize = FILESIZE_FILESIZE;
    unsigned            m_num_columns = 1;
    DWORD               m_dwAttrIncludeAny = 0;
    DWORD               m_dwAttrMatch = 0;
    DWORD               m_dwAttrExcludeAny = 0;
    StrW                m_sDisableOptions;
    bool                m_need_compressed_size = false;
    bool                m_need_short_filenames = false;

    ULONGLONG           m_min_time[TIMESTAMP_ARRAY_SIZE];
    ULONGLONG           m_max_time[TIMESTAMP_ARRAY_SIZE];
    ULONGLONG           m_min_size[FILESIZE_ARRAY_SIZE];
    ULONGLONG           m_max_size[FILESIZE_ARRAY_SIZE];
};

void SkipColonOrEqual(const WCHAR*& p);

extern const DirFormatSettings* g_settings;

