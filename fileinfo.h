// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <windows.h>
#include "flags.h"
#include "str.h"

struct DirFormatSettings;

class FileInfo
{
public:
                        FileInfo() {}
                        ~FileInfo() {}

    void                Init(const WCHAR* dir, DWORD granularity, const WIN32_FIND_DATA* pfd, const DirFormatSettings& settings);

    DWORD               GetAttributes() const { return m_dwAttr; }
    const FILETIME&     GetAccessTime() const { return m_ftAccess; }
    const FILETIME&     GetCreatedTime() const { return m_ftCreated; }
    const FILETIME&     GetModifiedTime() const { return m_ftModified; }
    const FILETIME&     GetFileTime(const WhichTimeStamp timestamp) const;
    const unsigned __int64& GetFileSize(const WhichFileSize filesize = FILESIZE_FILESIZE) const;
    float               GetCompressionRatio() const;
    const StrW&         GetFileName(FormatFlags flags) const;
    const StrW&         GetLongName() const { return m_long; }
    const StrW&         GetOwner() const { return m_owner; }

    bool                IsPseudoDirectory() const;
    bool                IsReparseTag() const;
    bool                IsSymLink() const;

    void                SetAltDataStreams() const { m_has_alt_data_streams = true; }
    bool                HasAltDataStreams() const { return m_has_alt_data_streams; }

private:
    DWORD               m_dwAttr;
    FILETIME            m_ftAccess;
    FILETIME            m_ftCreated;
    FILETIME            m_ftModified;
    ULARGE_INTEGER      m_ulAllocation;
    ULARGE_INTEGER      m_ulCompressed;
    ULARGE_INTEGER      m_ulFile;
    DWORD               m_dwReserved0;
    StrW                m_long;
    StrW                m_short;
    StrW                m_owner;
    mutable bool        m_has_alt_data_streams;

#ifdef DEBUG
    bool                m_fGotCompressedSize;
#endif
};

bool IsPseudoDirectory(const WCHAR* dir);

