// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "filesys.h"
#include "output.h"

static int s_hide_dot_files = 0;

// -1 = Disable hiding dot files; show dot files, and ignore HideDotFiles(0) and HideDotFiles(1).
//  0 = Show dot files.
//  1 = Hide dot files.
//  2 = Enable hiding dot files; hide dot files, and respect HideDotFiles(0) and HideDotFiles(1).
void HideDotFiles(int hide_dot_files)
{
    if (s_hide_dot_files >= 0 || hide_dot_files >= 2)
        s_hide_dot_files = hide_dot_files;
}

void DebugPrintHideDotFilesMode()
{
    if (g_debug)
        Printf(L"debug: hide_dot_files=%d\n", s_hide_dot_files);
}

bool IsPseudoDirectory(const WCHAR* dir)
{
    if (dir[0] != '.')
        return false;
    if (!dir[1])
        return true;
    if (dir[1] != '.')
        return false;
    if (!dir[2])
        return true;
    return false;
}

unsigned IsExtendedPath(const WCHAR* p)
{
    if (p[0] == '\\' &&
        p[1] == '\\' &&
        p[2] == '?' &&
        p[3] == '\\')
        return 4;
    return 0;
}

void GetCwd(StrW& dir, WCHAR chDrive)
{
    dir.Clear();

    // If no drive specified, get the current working directory.
    if (!chDrive)
    {
        StrW tmp;
        tmp.ReserveMaxPath();
        if (GetCurrentDirectory(tmp.Capacity(), tmp.Reserve()))
            dir.Set(tmp);
        return;
    }

    // Get the specified drive's cwd from the environment table.
    WCHAR name[4] = L"=C:";
    name[1] = WCHAR(towupper(chDrive));

    StrW value;
    value.ReserveMaxPath();
    if (GetEnvironmentVariable(name, value.Reserve(), value.Capacity()) && !value.Empty())
    {
        dir.Set(value);
        return;
    }

    // Otherwise assume root.
    dir.Printf(L"%c:\\", _totupper(chDrive));
    return;
}

bool GetDrive(const WCHAR* pattern, StrW& drive, Error& e)
{
    drive.Clear();

    if (!pattern || !*pattern)
        return false;

    bool unc = false;
    StrW extended;

    // Advance past \\?\ or \\?\UNC\.
    unsigned extended_len = IsExtendedPath(pattern);
    if (extended_len)
    {
        extended.Set(pattern, extended_len);
        pattern += extended_len;
        if (wcsnicmp(pattern, L"UNC\\", 4) == 0)
        {
            unc = true;
            extended.Append(pattern, 4);
            pattern += 4;
        }

        if (!*pattern)
            return false;
    }

    // For UNC paths, return the \\server\share as the drive.
    if (unc || (pattern[0] == '\\' &&
                pattern[1] == '\\'))
    {
        // Find end of \\server part.
        const WCHAR* tmp = wcschr(pattern + (unc ? 0 : 2), '\\');
        if (!tmp)
            return false;

        // Find end of \\server\share part.
        tmp = wcschr(tmp + 1, '\\');
        const size_t len = tmp ? tmp - pattern : wcslen(pattern);
        if (len > MaxPath())
        {
            e.Sys(ERROR_FILENAME_EXCED_RANGE);
            return false;
        }

        extended.Append(pattern, len);
        drive = std::move(extended);
        return true;
    }

    // Use drive letter from pattern, if present.
    if (pattern[0] &&
        pattern[1] == ':')
    {
        drive.Set(pattern, 2);
        drive.ToUpper();
        return true;
    }

    // Otherwise use drive letter from cwd.
    GetCwd(drive);
    if (drive.Length())
    {
        drive.SetLength(1);
        drive.Append(':');
    }
    return true;
}

bool IsFATDrive(const WCHAR* path, Error& e)
{
    StrW drive;
    DWORD cbComponentMax;
    WCHAR name[MAX_PATH + 1];

    if (!GetDrive(path, drive, e))
        return false;

    EnsureTrailingSlash(drive);

    if (!GetVolumeInformation(drive.Text(), 0, 0, 0, &cbComponentMax, 0, name, _countof(name)))
    {
        // Ignore ERROR_DIR_NOT_ROOT; consider SUBST drives as not FAT.
        const DWORD dwErr = GetLastError();
        if (dwErr != ERROR_DIR_NOT_ROOT)
            e.Sys(dwErr);
        return false;
    }

    return (!wcsicmp(name, L"FAT") && cbComponentMax == 12); // 12 == 8.3
}

bool IsHidden(const WIN32_FIND_DATA& fd)
{
    return ((fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ||
            IsHiddenName(fd.cFileName));
}

bool IsHiddenName(const WCHAR* p)
{
    return ((s_hide_dot_files > 0) &&
            (p[0] == '.' || p[0] == '_'));
}

FileType GetFileType(const WCHAR* p)
{
    WIN32_FIND_DATA fd;
    SHFind h = FindFirstFile(p, &fd);
    if (h.Empty())
        return FileType::Invalid;
    if (fd.dwFileAttributes == DWORD(-1))
        return FileType::Invalid;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DEVICE)
        return FileType::Device;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return FileType::Dir;
    return FileType::File;
}

bool IsTraversableReparse(const WIN32_FIND_DATA& fd)
{
    return ((fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
            (IsReparseTagNameSurrogate(fd.dwReserved0) || fd.dwReserved0 == IO_REPARSE_TAG_DFS));
}

struct DelayLoadKernel32
{
    union
    {
        FARPROC proc[4];
        struct
        {
            HANDLE (WINAPI* FindFirstStreamW)(LPCWSTR lpFileName, STREAM_INFO_LEVELS InfoLevel, LPVOID lpFindStreamData, DWORD dwFlags);
            BOOL (APIENTRY* FindNextStreamW)(HANDLE hFindStream, LPVOID lpFindStreamData);
            HANDLE (WINAPI* FindFirstFileExW)(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags);
        };
    };

    DelayLoadKernel32() { ZeroMemory(this, sizeof(*this)); }

    bool EnsureLoaded()
    {
        if (!m_fTried)
        {
            HMODULE hLib = LoadLibrary(L"kernel32.dll");
            if (hLib)
            {
                proc[0] = GetProcAddress(hLib, "FindFirstStreamW");
                proc[1] = GetProcAddress(hLib, "FindNextStreamW");
                proc[3] = GetProcAddress(hLib, "FindFirstFileExW");
                m_hLib = hLib;
            }
            m_fTried = true;
        }
        return !!m_hLib;
    }

private:
    bool m_fTried;
    HMODULE m_hLib;
};

DelayLoadKernel32 s_kernel32;

bool EnsureFileStreamFunctions()
{
    return (s_kernel32.EnsureLoaded() &&
            s_kernel32.FindFirstStreamW &&
            s_kernel32.FindNextStreamW);
}

HANDLE __FindFirstStreamW(LPCWSTR lpFileName, STREAM_INFO_LEVELS InfoLevel, LPVOID lpFindStreamData, DWORD dwFlags)
{
    if (s_kernel32.FindFirstStreamW)
        return s_kernel32.FindFirstStreamW(lpFileName, InfoLevel, lpFindStreamData, dwFlags);
    return 0;
}

bool __FindNextStreamW(HANDLE hFindStream, LPVOID lpFindStreamData)
{
    if (s_kernel32.FindNextStreamW)
        return s_kernel32.FindNextStreamW(hFindStream, lpFindStreamData);
    return false;
}

HANDLE __FindFirstFile(const StrW& s, bool short_names, WIN32_FIND_DATA* pfd)
{
    if (s_kernel32.FindFirstFileExW)
    {
        const FINDEX_INFO_LEVELS FindExInfoBasic = FINDEX_INFO_LEVELS(FindExInfoStandard + 1);
        #define FIND_FIRST_EX_CASE_SENSITIVE    0x00000001
        #define FIND_FIRST_EX_LARGE_FETCH       0x00000002
        return s_kernel32.FindFirstFileExW(s.Text(),
                                           short_names ? FindExInfoStandard : FindExInfoBasic,
                                           pfd,
                                           FindExSearchNameMatch,
                                           NULL,
                                           FIND_FIRST_EX_LARGE_FETCH);
    }
    return FindFirstFile(s.Text(), pfd);
}

