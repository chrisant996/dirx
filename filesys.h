// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "str.h"

void HideDotFiles(int hide_dot_files);

bool IsPseudoDirectory(const WCHAR* dir);
unsigned IsExtendedPath(const WCHAR* p);
void GetCwd(StrW& dir, WCHAR chDrive='\0');
bool GetDrive(const WCHAR* pattern, StrW& drive, Error& e);
bool IsFATDrive(const WCHAR* path, Error& e);
bool IsHidden(const WIN32_FIND_DATA& fd);
bool IsHiddenName(const WCHAR* p);
bool IsDir(const WCHAR* p);
bool IsTraversableReparse(const WIN32_FIND_DATA& fd);

bool EnsureFileStreamFunctions();
HANDLE __FindFirstStreamW(LPCWSTR lpFileName, STREAM_INFO_LEVELS InfoLevel, LPVOID lpFindStreamData, DWORD dwFlags);
bool __FindNextStreamW(HANDLE hFindStream, LPVOID lpFindStreamData);
HANDLE __FindFirstFile(const StrW& s, bool short_names, WIN32_FIND_DATA* pfd);

#define SYMLINK_FLAG_RELATIVE   1
#define SYMLINK_FLAG_OBJECTID   2

typedef struct _REPARSE_DATA_BUFFER {
    ULONG  ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union {
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG Flags;
            WCHAR PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
            UCHAR  DataBuffer[1];
        } GenericReparseBuffer;
    } DUMMYUNIONNAME;
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

struct OverrideErrorMode
{
    OverrideErrorMode(UINT mode) : m_old(SetErrorMode(mode)) {}
    ~OverrideErrorMode() { SetErrorMode(m_old); }
private:
    const UINT m_old;
};

#ifndef _S_IFLNK
#define _S_IFLNK        (0x0800)
#endif
#ifndef S_IFLNK
#define S_IFLNK         _S_IFLNK
#endif

#ifndef S_ISDIR
#define S_ISDIR(m)      (((m)&S_IFMT) == S_IFDIR)   // Directory.
#endif

#ifndef S_ISREG
#define S_ISREG(m)      (((m)&S_IFMT) == S_IFREG)   // File.
#endif

#ifndef S_ISLNK
#define S_ISLNK(m)      (((m)&S_IFLNK) == S_IFLNK)  // Symlink.
#endif

