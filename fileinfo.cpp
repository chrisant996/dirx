// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "fileinfo.h"

#include <lmcons.h>

void FileInfo::Init(const WCHAR* dir, DWORD granularity, const WIN32_FIND_DATA* pfd, const DirFormatSettings& settings)
{
    StrW full;

    m_long.Set(pfd->cFileName);
    m_short.Set(pfd->cAlternateFileName);

    m_dwAttr = pfd->dwFileAttributes;
    m_ftAccess = pfd->ftLastAccessTime;
    m_ftCreated = pfd->ftCreationTime;
    m_ftModified = pfd->ftLastWriteTime;

    m_ulFile.LowPart = pfd->nFileSizeLow;
    m_ulFile.HighPart = pfd->nFileSizeHigh;

    const bool get_compressed_size = ((GetAttributes() & FILE_ATTRIBUTE_COMPRESSED) &&
                                      settings.m_need_compressed_size);

    if (get_compressed_size || settings.IsSet(FMT_SHOWOWNER))
        PathJoin(full, dir, m_long);

    if (get_compressed_size)
        m_ulCompressed.LowPart = GetCompressedFileSize(full.Text(), &m_ulCompressed.HighPart);
    else
        m_ulCompressed.QuadPart = 0;
#ifdef DEBUG
    m_fGotCompressedSize = get_compressed_size;
#endif

    if (granularity)
    {
        const ULONGLONG ull = m_ulCompressed.QuadPart ? m_ulCompressed.QuadPart : m_ulFile.QuadPart;
        m_ulAllocation.QuadPart = ull;
        m_ulAllocation.QuadPart += (granularity - (ull % granularity)) % granularity;
    }
    else
    {
        m_ulAllocation = m_ulFile;
    }

    if (settings.IsSet(FMT_SHOWOWNER))
    {
        const size_t cbSecurityDescriptor = 65536 * sizeof(WCHAR);
        BYTE* pbSecurityDescriptor = static_cast<BYTE*>(malloc(cbSecurityDescriptor));
        WCHAR name[UNLEN + 1];
        ULONG name_len = _countof(name);
        WCHAR domain[DNLEN + 1];
        ULONG domain_len = _countof(domain);
        DWORD needed;
        PSID pSID;
        BOOL owner_defaulted;
        SID_NAME_USE snu;

        if (!pbSecurityDescriptor ||
            !GetFileSecurity(full.Text(), OWNER_SECURITY_INFORMATION, pbSecurityDescriptor, cbSecurityDescriptor, &needed) ||
            !GetSecurityDescriptorOwner(pbSecurityDescriptor, &pSID, &owner_defaulted) ||
            !LookupAccountSid(0, pSID, name, &name_len, domain, &domain_len, &snu))
        {
            m_owner.Set(L"...");
        }
        else
        {
            m_owner.Set(domain);
            m_owner.Append('\\');
            m_owner.Append(name);
        }

        free(pbSecurityDescriptor);
    }

    if (GetAttributes() & FILE_ATTRIBUTE_REPARSE_POINT)
        m_dwReserved0 = pfd->dwReserved0;
    else
        m_dwReserved0 = 0;
}

void FileInfo::InitStream(const WIN32_FIND_STREAM_DATA& fsd)
{
    m_long.Set(fsd.cStreamName);
    m_ulFile.QuadPart = fsd.StreamSize.QuadPart;
    m_is_alt_data_stream = true;
}

void FileInfo::InitStreams(std::vector<std::unique_ptr<FileInfo>>& streams)
{
    assert(!m_streams);
    assert(!streams.empty());
    m_streams = new std::unique_ptr<FileInfo>[streams.size() + 1];
    for (size_t ii = 0; ii < streams.size(); ++ii)
    {
        assert(!m_streams[ii]);
        m_streams[ii] = std::move(streams[ii]);
    }
    assert(!m_streams[streams.size()]);
}

const FILETIME& FileInfo::GetFileTime(const WhichTimeStamp timestamp) const
{
    switch (timestamp)
    {
    case TIMESTAMP_ACCESS:  return m_ftAccess;
    case TIMESTAMP_CREATED: return m_ftCreated;
    default:                return m_ftModified;
    }
}

const unsigned __int64& FileInfo::GetFileSize(const WhichFileSize filesize) const
{
    switch (filesize)
    {
    case FILESIZE_ALLOCATION:
        return *reinterpret_cast<const unsigned __int64*>(&m_ulAllocation);
    case FILESIZE_COMPRESSED:
        assert(implies(GetAttributes() & FILE_ATTRIBUTE_COMPRESSED, m_fGotCompressedSize));
        if (m_ulCompressed.QuadPart)
            return *reinterpret_cast<const unsigned __int64*>(&m_ulCompressed);
        // fall through
    default:
        return *reinterpret_cast<const unsigned __int64*>(&m_ulFile);
    }
}

float FileInfo::GetCompressionRatio() const
{
    if (!m_ulFile.QuadPart || !(GetAttributes() & FILE_ATTRIBUTE_COMPRESSED))
        return 0.0;
    assert(m_fGotCompressedSize);
    unsigned __int64 cbDelta = m_ulFile.QuadPart - m_ulCompressed.QuadPart;
    return float(cbDelta) / float(m_ulFile.QuadPart);
}

const StrW& FileInfo::GetFileName(FormatFlags flags) const
{
    if ((flags & FMT_SHORTNAMES) && (m_short.Length() || (flags & FMT_ONLYSHORTNAMES)))
        return m_short;
    return m_long;
}

bool FileInfo::IsPseudoDirectory() const
{
    if (GetAttributes() & FILE_ATTRIBUTE_DIRECTORY)
        return ::IsPseudoDirectory(m_long.Text());
    return false;
}

bool FileInfo::IsReparseTag() const
{
    return ((GetAttributes() & FILE_ATTRIBUTE_REPARSE_POINT) &&
            (IsReparseTagNameSurrogate(m_dwReserved0) || m_dwReserved0 == IO_REPARSE_TAG_DFS));
}

bool FileInfo::IsSymLink() const
{
    return (IsReparseTag() && m_dwReserved0 == IO_REPARSE_TAG_SYMLINK);
}

