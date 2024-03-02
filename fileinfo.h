// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <windows.h>
#include "flags.h"
#include "str.h"

#include <memory>
#include <vector>

struct DirFormatSettings;

class FileInfo
{
public:
                        FileInfo() {}
                        ~FileInfo() { delete [] m_streams; }

    void                Init(const WCHAR* dir, DWORD granularity, const WIN32_FIND_DATA* pfd, const DirFormatSettings& settings);
    void                InitStream(const WIN32_FIND_STREAM_DATA& fsd);
    void                InitStreams(std::vector<std::unique_ptr<FileInfo>>& streams);

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
    const std::unique_ptr<FileInfo>* GetStreams() const { return m_streams; }

    bool                IsPseudoDirectory() const;
    bool                IsReparseTag() const;
    bool                IsSymLink() const;
    bool                IsBroken() const { return m_broken; }

    void                SetAltDataStreams() const { m_has_alt_data_streams = true; }
    bool                HasAltDataStreams() const { return m_has_alt_data_streams; }
    bool                IsAltDataStream() const { return m_is_alt_data_stream; }

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
    mutable bool        m_has_alt_data_streams = false;
    bool                m_is_alt_data_stream = false;
    bool                m_broken = false;
    std::unique_ptr<FileInfo>* m_streams = nullptr; // nullptr terminated, free with delete[].

#ifdef DEBUG
    bool                m_fGotCompressedSize;
#endif
};

bool IsPseudoDirectory(const WCHAR* dir);

