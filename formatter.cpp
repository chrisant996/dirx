// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "formatter.h"
#include "sorting.h"
#include "patterns.h"
#include "filesys.h"
#include "colors.h"
#include "icons.h"
#include "output.h"
#include "ecma48.h"
#include "wcwidth.h"

#include <algorithm>
#include <memory>

//static WCHAR s_chTruncated = L'\x2192'; // Right arrow character.
//static WCHAR s_chTruncated = L'\x25b8'; // Black Right-Pointing Small Triangle character.
static WCHAR s_chTruncated = L'\x2026'; // Horizontal Ellipsis character.
static bool s_use_icons = false;
static BYTE s_icon_padding = 1;
static unsigned s_icon_width = 0;
static bool s_scale_size = false;
static bool s_scale_time = false;
static bool s_gradient = false;

static const WCHAR c_hyperlink[] = L"\x1b]8;;";
static const WCHAR c_BEL[] = L"\a";

// #define ASSERT_WIDTH

#if defined(DEBUG) && defined(ASSERT_WIDTH)
#define assert_width        assert
#else
#define assert_width(expr)  do {} while (0)
#endif

void SetTruncationCharacter(WCHAR ch)
{
    s_chTruncated = ch;
}

WCHAR GetTruncationCharacter()
{
    return s_chTruncated;
}

void SetUseIcons(bool use_icons)
{
    s_use_icons = use_icons;
    s_icon_width = s_use_icons ? 1 + s_icon_padding : 0;
}

void SetPadIcons(unsigned spaces)
{
    s_icon_padding = BYTE(clamp<unsigned>(spaces, 1, 4));
    s_icon_width = s_use_icons ? 1 + s_icon_padding : 0;
}

bool SetColorScale(const WCHAR* s)
{
    if (!s)
        return false;
    if (!_wcsicmp(s, L"") || !_wcsicmp(s, L"all"))
        s_scale_size = s_scale_time = true;
    else if (!_wcsicmp(s, L"none"))
        s_scale_size = s_scale_time = false;
    else if (!_wcsicmp(s, L"size"))
        s_scale_size = true, s_scale_time = false;
    else if (!_wcsicmp(s, L"time") || !_wcsicmp(s, L"date") || !_wcsicmp(s, L"age"))
        s_scale_size = false, s_scale_time = true;
    else
        return false;
    return true;
}

bool IsScalingSize()
{
    return s_scale_size;
}

bool SetColorScaleMode(const WCHAR* s)
{
    if (!s)
        return false;
    else if (!_wcsicmp(s, L"fixed"))
        s_gradient = false;
    else if (!_wcsicmp(s, L"gradient"))
        s_gradient = true;
    else
        return false;
    return true;
}

static void SelectFileTime(const FileInfo* const pfi, const WhichTimeStamp timestamp, SYSTEMTIME* const psystime)
{
    FILETIME ft;

    FileTimeToLocalFileTime(&pfi->GetFileTime(timestamp), &ft);
    FileTimeToSystemTime(&ft, psystime);
}

static const WCHAR* SelectColor(const FileInfo* const pfi, const DWORD flags)
{
    if (!(flags & FMT_DISABLECOLORS))
    {
        const WCHAR* color = LookupColor(pfi);
        if (color)
            return color;
    }
    return nullptr;
}

static LCID s_lcid = 0;
static unsigned s_locale_date_time_len = 0;
static WCHAR s_locale_date[80];
static WCHAR s_locale_time[80];
static WCHAR s_decimal[2];
static WCHAR s_thousand[2];

bool InitLocale()
{
    WCHAR tmp[80];

    // NOTE: Set a breakpoint on GetLocaleInfo in CMD.  Observe in the
    // assembly code that before it calls GetLocaleInfo, it calls
    // GetUserDefaultLCID and then tests for certain languages and ... uses
    // English instead.  I don't understand why it's doing that, but since I'm
    // trying to behave similarly I guess I'll do the same?
    s_lcid = GetUserDefaultLCID();
    if ((PRIMARYLANGID(s_lcid) == LANG_ARABIC) ||
        (PRIMARYLANGID(s_lcid) == LANG_FARSI) ||
        (PRIMARYLANGID(s_lcid) == LANG_HEBREW) ||
        (PRIMARYLANGID(s_lcid) == LANG_HINDI) ||
        (PRIMARYLANGID(s_lcid) == LANG_TAMIL) ||
        (PRIMARYLANGID(s_lcid) == LANG_THAI))
        s_lcid = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), SORT_DEFAULT); // 0x409

    if (!GetLocaleInfo(s_lcid, LOCALE_SDECIMAL, s_decimal, _countof(s_decimal)))
        wcscpy_s(s_decimal, L".");
    if (!GetLocaleInfo(s_lcid, LOCALE_STHOUSAND, s_thousand, _countof(s_thousand)))
        wcscpy_s(s_thousand, L",");

    // Get the locale-dependent short date and time formats.
    // (It looks like a loop, but it's just so 'break' can short circuit out.)
    while (true)
    {
        // First the date format...
        if (!GetLocaleInfo(s_lcid, LOCALE_SSHORTDATE, tmp, _countof(tmp)))
        {
            if (!GetLocaleInfo(s_lcid, LOCALE_IDATE, tmp, _countof(tmp)))
                break;

            if (tmp[0] == '0')
                wcscpy_s(tmp, L"MM/dd/yy");
            else if (tmp[0] == '1')
                wcscpy_s(tmp, L"dd/MM/yy");
            else if (tmp[0] == '2')
                wcscpy_s(tmp, L"yy/MM/dd");
            else
                break;
        }

        // Massage the locale date format so it's fixed width.
        StrW s;
        bool quoted = false;
        for (const WCHAR* pch = tmp; *pch; pch++)
        {
            if (*pch == '\'')
            {
                quoted = !quoted;
                s.Append(*pch);
            }
            else if (quoted)
            {
                s.Append(*pch);
            }
            else
            {
                unsigned c = 0;
                const WCHAR* pchCount = pch;
                while (*pchCount && *pchCount == *pch)
                {
                    s.Append(*pch);
                    pchCount++;
                    c++;
                }
                if (*pch == 'd' || *pch == 'M')
                {
                    if (c == 1)
                        s.Append(*pch);
                    else if (c == 4)
                        s.SetLength(s.Length() - 1);
                }
                pch = pchCount - 1;
            }
        }

        if (s.Length() >= _countof(s_locale_date))
            break;
        wcscpy_s(s_locale_date, s.Text());

        // Next the time format...
        if (!GetLocaleInfo(s_lcid, LOCALE_SSHORTTIME, tmp, _countof(tmp)))
            wcscpy_s(tmp, L"hh:mm tt");

        // Massage the locale time format so it's fixed width.
        s.Clear();
        quoted = false;
        for (const WCHAR* pch = tmp; *pch; pch++)
        {
            if (*pch == '\'')
            {
                quoted = !quoted;
                s.Append(*pch);
            }
            else if (quoted)
            {
                s.Append(*pch);
            }
            else if (*pch == 'h' || *pch == 'H' || *pch == 'm')
            {
                unsigned c = 0;
                const WCHAR* pchCount = pch;
                while (*pchCount && *pchCount == *pch)
                {
                    s.Append(*pch);
                    pchCount++;
                    c++;
                }
                if (c == 1)
                    s.Append(*pch);
                pch = pchCount - 1;
            }
            else
            {
                s.Append(*pch);
            }
        }

        if (s.Length() >= _countof(s_locale_time))
            break;
        wcscpy_s(s_locale_time, s.Text());

        s_locale_date_time_len = unsigned(wcslen(s_locale_date) + 2 + wcslen(s_locale_time));
        break;
    }

    return s_locale_date_time_len > 0;
}

static const AttrChar c_attr_chars[] =
{
    { FILE_ATTRIBUTE_READONLY, 'r' },
    { FILE_ATTRIBUTE_HIDDEN, 'h' },
    { FILE_ATTRIBUTE_SYSTEM, 's' },
    { FILE_ATTRIBUTE_ARCHIVE, 'a' },
    { FILE_ATTRIBUTE_DIRECTORY, 'd' },
    { FILE_ATTRIBUTE_ENCRYPTED, 'e' },
    { FILE_ATTRIBUTE_NORMAL, 'n' },
    { FILE_ATTRIBUTE_TEMPORARY, 't' },
    { FILE_ATTRIBUTE_SPARSE_FILE, 'p' },
    { FILE_ATTRIBUTE_COMPRESSED, 'c' },
    { FILE_ATTRIBUTE_OFFLINE, 'o' },
    { FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, 'i' },
    { FILE_ATTRIBUTE_REPARSE_POINT, 'j' },
    { FILE_ATTRIBUTE_REPARSE_POINT, 'l' },
    //{ FILE_ATTRIBUTE_DEVICE, 'v' },
};
static const WCHAR c_attr_mask_default[] = L"rhsadentpcoij";

DWORD ParseAttribute(const WCHAR ch)
{
    for (unsigned ii = _countof(c_attr_chars); ii--;)
    {
        if (ch == c_attr_chars[ii].ch)
            return c_attr_chars[ii].dwAttr;
    }
    return 0;
}

static void FormatAttributes(StrW& s, const DWORD dwAttr, const WCHAR chNotSet = '_')
{
    for (unsigned ii = 0; ii < _countof(c_attr_chars); ii++)
        s.Append((dwAttr & c_attr_chars[ii].dwAttr) ? c_attr_chars[ii].ch : chNotSet);
}

static void FormatAttributes(StrW& s, const DWORD dwAttr, const std::vector<AttrChar>* vec, WCHAR chNotSet = '_')
{
    if (!chNotSet)
        chNotSet = '_';

    for (unsigned ii = 0; ii < vec->size(); ii++)
    {
        const AttrChar* const pAttr = &*(vec->cbegin() + ii);
        s.Append((dwAttr & pAttr->dwAttr) ? pAttr->ch : chNotSet);
    }
}

static WCHAR GetEffectiveFilenameFieldStyle(const DirFormatSettings& settings, WCHAR chStyle)
{
    if (!chStyle)
    {
        if (settings.IsSet(FMT_FAT))
            chStyle = 'f';
    }

    return chStyle;
}

static unsigned GetFilenameFieldWidth(const DirFormatSettings& settings, const FieldInfo* field, unsigned max_file_width, unsigned max_dir_width)
{
    assert(field->m_field == FLD_FILENAME);

    switch (GetEffectiveFilenameFieldStyle(settings, field->m_chStyle))
    {
    case 'f':       return s_icon_width + 12;
    }

    if (field->m_cchWidth)
        return field->m_cchWidth;

    unsigned width = std::max<unsigned>(max_file_width, max_dir_width);
    if (width)
        width += s_icon_width;
    return width;
}

static void JustifyFilename(StrW& s, const StrW& name, unsigned max_name_width, unsigned max_ext_width)
{
    assert(*name.Text() != '.');
    assert(max_name_width);
    assert(max_ext_width);

    const unsigned orig_len = s.Length();

    unsigned name_width = __wcswidth(name.Text());
    unsigned ext_width = 0;
    const WCHAR* ext = FindExtension(name.Text());

    if (ext)
    {
        ext_width = __wcswidth(ext);
        name_width -= ext_width;
        assert(*ext == '.');
        ext++;
        ext_width--;
    }

    if (!ext_width)
    {
        const unsigned combined_width = max_name_width + 1 + max_ext_width;
        if (name.Length() <= combined_width)
            s.Append(name);
        else
        {
            StrW tmp;
            tmp.Set(name);
            TruncateWcwidth(tmp, combined_width, GetTruncationCharacter());
            s.Append(tmp);
        }
    }
    else
    {
        StrW tmp;
        tmp.Set(name);
        TruncateWcwidth(tmp, max_name_width, 0);
        tmp.Append(name_width > max_name_width ? '.' : ' ');
        s.Append(tmp);
        if (ext_width > max_ext_width)
        {
            tmp.Clear();
            tmp.Set(ext);
            TruncateWcwidth(tmp, max_ext_width, GetTruncationCharacter());
            s.Append(tmp);
        }
        else
            s.Append(ext);
    }

    assert(max_name_width + 1 + max_ext_width >= __wcswidth(s.Text() + orig_len));
    s.AppendSpaces(max_name_width + 1 + max_ext_width - __wcswidth(s.Text() + orig_len));
}

static void FormatFilename(StrW& s, const FileInfo* pfi, DWORD flags, unsigned max_width=0, const WCHAR* dir=nullptr, const WCHAR* color=nullptr)
{
    StrW tmp;
    const StrW& name = pfi->GetFileName(flags);
    WCHAR classify = 0;

    if (*name.Text() == '.')
        flags &= ~(FMT_JUSTIFY_FAT|FMT_JUSTIFY_NONFAT);

    if (s_use_icons)
    {
        DWORD attr = pfi->GetAttributes();
        const WCHAR* icon = LookupIcon(name.Text(), attr);
        const WCHAR* icon_color = GetIconColor(color);
        if (icon_color)
            s.Printf(L"\x1b[%sm", icon_color);
        s.Append(icon ? icon : L" ");
        if (icon_color)
            s.Append(c_norm);
        s.AppendSpaces(s_icon_padding);
        if (max_width)
            max_width -= s_icon_width;
    }

    if (color)
        s.Printf(L"\x1b[%sm", color);

    const bool hyperlinks = ((flags & FMT_HYPERLINKS) && dir);
    if (hyperlinks)
    {
        s.Append(c_hyperlink);
        s.Append(L"file://");
        s.Append(dir);
        EnsureTrailingSlash(s);
        s.Append(name.Text());
        s.Append(c_BEL);
    }

    if (flags & FMT_FAT)
    {
        assert(!(flags & FMT_DIRBRACKETS));
        assert(!(flags & FMT_CLASSIFY));

        if (flags & FMT_JUSTIFY_FAT)
        {
            JustifyFilename(tmp, name, 8, 3);
        }
        else
        {
            unsigned name_width = __wcswidth(name.Text());
            tmp.Set(name);
            if (name_width > 12)
                name_width = TruncateWcwidth(tmp, 12, GetTruncationCharacter());
            tmp.AppendSpaces(12 - name_width);
        }

        if (flags & FMT_LOWERCASE)
            tmp.ToLower();

        assert(__wcswidth(tmp.Text()) == 12);

        s.Append(tmp);
    }
    else
    {
        const WCHAR* p = name.Text();
        const bool show_brackets = (pfi->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY) && (flags & FMT_DIRBRACKETS);
        unsigned name_width = 0;

        if (max_width)
        {
            if ((flags & FMT_JUSTIFY_NONFAT) && !show_brackets && max_width >= 2 + 1 + 3)
            {
                JustifyFilename(tmp, name, max_width - 4, 3);
                p = tmp.Text();

                assert(__wcswidth(p) == max_width);
                name_width = max_width;
            }
            else
            {
                const unsigned truncate_width = max_width - (show_brackets ? 2 : 0);
                name_width = __wcswidth(name.Text());
                if (name_width > truncate_width)
                {
                    if (truncate_width)
                    {
                        if (!tmp.Length())
                            tmp.Set(p);
                        name_width = TruncateWcwidth(tmp, truncate_width, GetTruncationCharacter());
                        p = tmp.Text();
                    }
                }
            }
        }

        if (max_width && tmp.Empty())
            name_width = __wcswidth(name.Text());

        if (flags & FMT_LOWERCASE)
        {
            if (!tmp.Length())
                tmp.Set(name);
            tmp.ToLower();
            p = tmp.Text();
        }

        if (show_brackets)
        {
            s.Printf(L"[%s]", p);
            if (max_width)
                name_width += 2;
        }
        else
        {
            if ((flags & FMT_FULLNAME) && dir)
            {
                const unsigned orig_len = s.Length();
                s.Append(dir);
                EnsureTrailingSlash(s);
                if (max_width)
                    name_width += __wcswidth(s.Text() + orig_len);
            }
            s.Append(p);

            if (flags & FMT_CLASSIFY)
            {
                if (pfi->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY)
                    classify = L'\\';
                else if (pfi->IsSymLink())
                    classify = L'@';
            }
        }

        if (max_width)
            s.AppendSpaces(max_width - name_width);
    }

    if (classify || hyperlinks)
    {
        unsigned spaces = 0;
        unsigned len = s.Length();
        while (len && s.Text()[len - 1] == ' ')
        {
            --len;
            ++spaces;
        }
        s.SetLength(len);
        if (hyperlinks)
        {
            s.Append(c_hyperlink);
            s.Append(c_BEL);
        }
        if (classify)
        {
            assert(spaces);
            --spaces;
            s.Append(&classify, 1);
        }
        s.AppendSpaces(spaces);
    }

    if (color)
        s.Append(c_norm);
}

static void FormatReparsePoint(StrW& s, const FileInfo* const pfi, const DWORD flags, const WCHAR* const dir)
{
    assert(dir);

    if (!pfi->IsReparseTag())
        return;

    StrW full;
    full.Set(dir);
    EnsureTrailingSlash(full);
    full.Append(pfi->GetLongName());

    SHFile shFile = CreateFile(full.Text(), FILE_READ_EA, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (shFile.Empty())
    {
        s.Append(L" [..]");
    }
    else
    {
        StrW tmp;
        DWORD dwReturned;
        BYTE buffer[MAXIMUM_REPARSE_DATA_BUFFER_SIZE + sizeof(WCHAR)];
        PREPARSE_DATA_BUFFER reparse_data = reinterpret_cast<PREPARSE_DATA_BUFFER>(buffer);
        if (DeviceIoControl(shFile, FSCTL_GET_REPARSE_POINT, 0, 0, buffer, sizeof(buffer), &dwReturned, 0))
        {
            switch (reparse_data->ReparseTag)
            {
            case IO_REPARSE_TAG_MOUNT_POINT:
                if (reparse_data->MountPointReparseBuffer.PrintNameLength)
                    tmp.SetW(reparse_data->MountPointReparseBuffer.PathBuffer + reparse_data->MountPointReparseBuffer.PrintNameOffset / sizeof(WCHAR),
                             reparse_data->MountPointReparseBuffer.PrintNameLength / sizeof(WCHAR));
                else
                    tmp.SetW(reparse_data->MountPointReparseBuffer.PathBuffer + reparse_data->MountPointReparseBuffer.SubstituteNameOffset / sizeof(WCHAR),
                             reparse_data->MountPointReparseBuffer.SubstituteNameLength / sizeof(WCHAR));
                break;
            case IO_REPARSE_TAG_SYMLINK:
                if (reparse_data->SymbolicLinkReparseBuffer.PrintNameLength)
                    tmp.SetW(reparse_data->SymbolicLinkReparseBuffer.PathBuffer + reparse_data->SymbolicLinkReparseBuffer.PrintNameOffset / sizeof(WCHAR),
                             reparse_data->SymbolicLinkReparseBuffer.PrintNameLength / sizeof(WCHAR));
                else
                    tmp.SetW(reparse_data->SymbolicLinkReparseBuffer.PathBuffer + reparse_data->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR),
                             reparse_data->SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(WCHAR));
                break;
            }
        }
        s.Append(L" [");
        if (tmp.Length())
        {
            const WCHAR* name = FindName(tmp.Text());
            const DWORD attr = pfi->GetAttributes();
            const int mode = (attr & FILE_ATTRIBUTE_DIRECTORY) ? S_IFDIR : S_IFREG;
            const WCHAR* color = (flags & FMT_DISABLECOLORS) ? nullptr : LookupColor(name, attr, mode);
            const WCHAR* path_color = GetColorByKey(L"lp");
            if (!path_color && !color)
                s.Append(tmp);
            else
            {
                if (!path_color)
                    path_color = color;
                if (path_color)
                    s.Printf(L"\x1b[%sm", path_color);
                s.Append(tmp.Text(), name - tmp.Text());
                if (color)
                    s.Printf(L"\x1b[0;%sm", color);
                else
                    s.Append(c_norm);
                s.Append(name);
                if (color)
                    s.Append(c_norm);
            }
        }
        else
            s.Append(L"...");
        s.Append(L"]");
    }
}

static void FormatSizeForReading(StrW& s, unsigned __int64 cbSize, unsigned field_width, const DirFormatSettings& settings)
{
    WCHAR tmp[100];
    WCHAR* out = tmp + _countof(tmp) - 1;
    unsigned cDigits = 0;

    *out = '\0';

    do
    {
        const WCHAR chDigit = '0' + (cbSize % 10);

        if (settings.IsSet(FMT_SEPARATETHOUSANDS))
        {
            // CMD DIR does not check GetLocaleInfo(LOCALE_SGROUPING).
            if (cDigits && !(cDigits % 3))
            {
                out--;
                *out = s_thousand[0];
            }
        }

        out--;
        *out = chDigit;

        cbSize /= 10;
        cDigits++;
    }
    while (cbSize);

    s.Printf(L"%*s", field_width, out);
}

static WCHAR GetEffectiveSizeFieldStyle(const DirFormatSettings& settings, WCHAR chStyle)
{
    if (!chStyle)
    {
        if (settings.IsSet(FMT_MINISIZE))
            chStyle = 'm';
        else if (!settings.IsSet(FMT_FULLSIZE))
            chStyle = 's';
    }

    return chStyle;
}

static unsigned GetSizeFieldWidthByStyle(const DirFormatSettings& settings, WCHAR chStyle)
{
    switch (GetEffectiveSizeFieldStyle(settings, chStyle))
    {
    case 'm':       return 4;           // "9.9M"
    case 's':       return 9;           // "123456789M"
    default:        return 16;          // ",123,456,789,123"
    }
}

static void FormatSize(StrW& s, unsigned __int64 cbSize, const WhichFileSize* which, const DirFormatSettings& settings, WCHAR chStyle, const WCHAR* color=nullptr, const WCHAR* fallback_color=nullptr)
{
    // FUTURE: CMD shows size for FILE_ATTRIBUTE_OFFLINE files in parentheses
    // to indicate it could take a while to retrieve them.

    chStyle = GetEffectiveSizeFieldStyle(settings, chStyle);

#ifdef DEBUG
    const unsigned orig_len = s.Length();
#endif

    if (!color && which)
        color = GetSizeColor(cbSize);
    if (!color)
        color = fallback_color;
    if (s_gradient && s_scale_size && which)
    {
        const WCHAR* gradient = ApplyGradient(color ? color : L"", cbSize, settings.m_min_size[*which], settings.m_max_size[*which]);
        if (gradient)
            color = gradient;
    }
    if (color)
        s.Printf(L"\x1b[%sm", color);

    switch (chStyle)
    {
    case 'm':
        {
            static const WCHAR c_size_chars[] = { 'K', 'K', 'M', 'G', 'T' };
            static_assert(_countof(c_size_chars) == 5, "size mismatch");

            double dSize = double(cbSize);
            unsigned iChSize = 0;

            while (dSize > 999 && iChSize + 1 < _countof(c_size_chars))
            {
                dSize /= 1024;
                iChSize++;
            }

            if (iChSize == 2 && dSize + 0.05 < 10.0)
            {
                dSize += 0.05;
                cbSize = static_cast<unsigned __int64>(dSize * 10);
                s.Printf(L"%I64u.%I64u%c", cbSize / 10, cbSize % 10, c_size_chars[iChSize]);
            }
            else
            {
                dSize += 0.5;
                cbSize = static_cast<unsigned __int64>(dSize);
                // Special case:  show 1..999 bytes as "1K", 0 bytes as "0K".
                if (!iChSize && cbSize)
                {
                    cbSize = 1;
                    iChSize++;
                }
                s.Printf(L"%3I64u%c", cbSize, c_size_chars[iChSize]);
            }
        }
        break;

    case 's':
        {
            // If size fits in 8 digits, report it as is.

            if (cbSize < 100000000)
            {
                s.Printf(L"%8I64u ", cbSize);
                break;
            }

            // Otherwise try to show fractional Megabytes or Terabytes.

            WCHAR chSize = 'M';
            double dSize = double(cbSize) / (1024 * 1024);

            if (dSize + 0.05 >= 1000000)
            {
                chSize = 'T';
                dSize /= 1024 * 1024;
            }

            dSize += 0.05;
            cbSize = static_cast<unsigned __int64>(dSize * 10);

            s.Printf(L"%6I64u.%1I64u%c", cbSize / 10, cbSize % 10, chSize);
        }
        break;

    default:
        FormatSizeForReading(s, cbSize, GetSizeFieldWidthByStyle(settings, chStyle), settings);
        break;
    }

    if (color)
        s.Append(c_norm);

#ifdef DEBUG
    assert_width(cell_count(s.Text() + orig_len) == GetSizeFieldWidthByStyle(settings, chStyle));
#endif
}

static const WCHAR* GetSizeTag(const FileInfo* const pfi, WCHAR chStyle)
{
    static const WCHAR* const c_rgszSizeTags[] =
    {
        L"  <JUNCTION>",    L"  <JUNCT>",   L" <J>",
        L"  <SYMLINKD>",    L"  <LINKD>",   L" <L>",
        L"  <SYMLINK>",     L"  <LINK>",    L" <L>",
        L"  <DIR>",         L"  <DIR>",     L" <D>",
    };
    static_assert(_countof(c_rgszSizeTags) == 4 * 3, "size mismatch");

    const unsigned iWidth = (chStyle == 'm' ? 2 :
                             chStyle == 's' ? 1 : 0);

    if (pfi->GetAttributes() & (FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))
    {
        if (pfi->IsReparseTag() &&
            !(pfi->GetAttributes() & FILE_ATTRIBUTE_OFFLINE))
        {
            if (!pfi->IsSymLink())
                return c_rgszSizeTags[0 * 3 + iWidth];
            else if (pfi->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY)
                return c_rgszSizeTags[1 * 3 + iWidth];
            else
                return c_rgszSizeTags[2 * 3 + iWidth];
        }
        else if (pfi->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY)
        {
            return c_rgszSizeTags[3 * 3 + iWidth];
        }
    }

    return 0;
}

static WhichFileSize WhichFileSizeByField(const DirFormatSettings& settings, WCHAR chField)
{
    switch (chField)
    {
    case 'a':           return FILESIZE_ALLOCATION;
    case 'c':           return FILESIZE_COMPRESSED;
    case 'f':           return FILESIZE_FILESIZE;
    default:            return settings.m_whichfilesize;
    }
}

static void FormatFileSize(StrW& s, const FileInfo* pfi, const DirFormatSettings& settings, WCHAR chStyle=0, WCHAR chField=0, const WCHAR* fallback_color=nullptr)
{
    chStyle = GetEffectiveSizeFieldStyle(settings, chStyle);
    const WCHAR* const tag = GetSizeTag(pfi, chStyle);

#ifdef DEBUG
    const unsigned orig_len = s.Length();
#endif

    if (tag)
    {
        if (fallback_color)
            s.Printf(L"\x1b[%sm", fallback_color);

        const unsigned cchField = GetSizeFieldWidthByStyle(settings, chStyle);
        s.Printf(L"%-*s", cchField, tag);

        if (fallback_color)
            s.Append(c_norm);
    }
    else
    {
        const WhichFileSize which = WhichFileSizeByField(settings, chField);
        FormatSize(s, pfi->GetFileSize(which), &which, settings, chStyle, nullptr, fallback_color);
    }

#ifdef DEBUG
    assert_width(cell_count(s.Text() + orig_len) == GetSizeFieldWidthByStyle(settings, chStyle));
#endif
}

static void FormatLocaleDateTime(StrW& s, const SYSTEMTIME* psystime)
{
    WCHAR tmp[128];

    if (GetDateFormat(s_lcid, 0, psystime, s_locale_date, tmp, _countof(tmp)))
        s.Append(tmp);
    s.Append(L"  ");
    if (GetTimeFormat(s_lcid, 0, psystime, s_locale_time, tmp, _countof(tmp)))
        s.Append(tmp);
}

static WCHAR GetEffectiveTimeFieldStyle(const DirFormatSettings& settings, WCHAR chStyle)
{
    if (!chStyle)
    {
        if (settings.IsSet(FMT_FAT))
            chStyle = 's';
        else if (settings.IsSet(FMT_FULLTIME))
            chStyle = 'x';
        else if (settings.IsSet(FMT_MINIDATE))
            chStyle = 'm';
        else if (settings.IsSet(FMT_LOCALEDATETIME) && s_locale_date_time_len)
            chStyle = 'l';
    }

    return chStyle;
}

static unsigned GetTimeFieldWidthByStyle(const DirFormatSettings& settings, WCHAR chStyle)
{
    switch (GetEffectiveTimeFieldStyle(settings, chStyle))
    {
    case 'l':           assert(s_locale_date_time_len); return s_locale_date_time_len;
    case 'x':           return 24;      // "YYYY/MM/DD  HH:mm:ss.mmm"
    case 's':           return 15;      // "MM/DD/YY  HH:mm"
    case 'm':           return 11;      // "MM/DD HH:mm"  or  "MM/DD  YYYY"
    default:            return 17;      // "MM/DD/YYYY  HH:mm"
    }
}

static WhichTimeStamp WhichTimeStampByField(const DirFormatSettings& settings, WCHAR chField)
{
    switch (chField)
    {
    case 'a':           return TIMESTAMP_ACCESS;
    case 'c':           return TIMESTAMP_CREATED;
    case 'w':           return TIMESTAMP_MODIFIED;
    default:            return settings.m_whichtimestamp;
    }
}

static void FormatTime(StrW& s, const FileInfo* pfi, const DirFormatSettings& settings, WCHAR chStyle=0, WCHAR chField=0, const WCHAR* fallback_color=nullptr)
{
    SYSTEMTIME systime;
    const WhichTimeStamp which = WhichTimeStampByField(settings, chField);

    SelectFileTime(pfi, which, &systime);

    chStyle = GetEffectiveTimeFieldStyle(settings, chStyle);

#ifdef DEBUG
    const unsigned orig_len = s.Length();
#endif

    const WCHAR* color = nullptr;
    if (!settings.IsSet(FMT_DISABLECOLORS))
    {
        color = GetColorByKey(L"da");
        if (!color)
            color = fallback_color;
        if (s_gradient && s_scale_time)
        {
            const WCHAR* gradient = ApplyGradient(color ? color : L"", FileTimeToULONGLONG(pfi->GetFileTime(which)), settings.m_min_time[which], settings.m_max_time[which]);
            if (gradient)
                color = gradient;
        }
        if (color)
            s.Printf(L"\x1b[%sm", color);
    }

    switch (chStyle)
    {
    case 'l':
        // Locale format.
        FormatLocaleDateTime(s, &systime);
        break;

    case 'x':
        // 24 characters.
        s.Printf(L"%04u/%02u/%02u  %2u:%02u:%02u%s%03u",
                 systime.wYear,
                 systime.wMonth,
                 systime.wDay,
                 systime.wHour,
                 systime.wMinute,
                 systime.wSecond,
                 s_decimal,
                 systime.wMilliseconds);
        break;

    case 's':
        // 15 characters.
        s.Printf(L"%2u/%02u/%02u  %2u:%02u",
                 systime.wMonth,
                 systime.wDay,
                 systime.wYear % 100,
                 systime.wHour,
                 systime.wMinute);
        break;

    case 'm':
        // 11 characters.
        {
            SYSTEMTIME systimeNow;
            GetLocalTime(&systimeNow);
            const unsigned iMonthFile = unsigned(systime.wYear) * 12 + systime.wMonth - 1;
            const unsigned iMonthNow = unsigned(systimeNow.wYear) * 12 + systimeNow.wMonth - 1;
            const bool fShowYear = (iMonthFile > iMonthNow || iMonthFile + 6 < iMonthNow);
            if (fShowYear)
            {
                //s.Printf(L"%2u/%02u/%04u ",
                s.Printf(L"%2u/%02u  %04u",
                //s.Printf(L"%2u/%02u--%04u",
                //s.Printf(L"%2u/%02u %04u*",
                         systime.wMonth,
                         systime.wDay,
                         systime.wYear);
            }
            else
            {
                s.Printf(L"%2u/%02u %2u:%02u",
                         systime.wMonth,
                         systime.wDay,
                         systime.wHour,
                         systime.wMinute);
            }
        }
        break;

    case 'n':
    default:
        // 17 characters.
        s.Printf(L"%2u/%02u/%04u  %2u:%02u",
                 systime.wMonth,
                 systime.wDay,
                 systime.wYear,
                 systime.wHour,
                 systime.wMinute);
        break;
    }

    if (color)
        s.Append(c_norm);

#ifdef DEBUG
    assert_width(cell_count(s.Text() + orig_len) == GetTimeFieldWidthByStyle(settings, chStyle));
#endif
}

static void FormatCompressed(StrW& s, const unsigned __int64 cbCompressed, const unsigned __int64 cbFile, const DWORD dwAttr)
{
    if (dwAttr & FILE_ATTRIBUTE_DIRECTORY)
    {
        s.Append(L"   ");
    }
    else
    {
        unsigned pct = 0;

        if ((dwAttr & FILE_ATTRIBUTE_COMPRESSED) && cbCompressed && cbFile)
        {
            unsigned __int64 cbDelta = cbFile - cbCompressed;
            float ratio = float(cbDelta) / float(cbFile);
            pct = unsigned(ratio * 100);
            if (pct > 99)
            {
                assert(false);
                pct = 99;
            }
        }

        s.Printf(L"%2u%%", pct);
    }
}

static void FormatCompressed(StrW& s, const FileInfo* pfi, WCHAR chField=0)
{
    if (pfi->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY)
    {
        s.Append(L"   ");
    }
    else
    {
        const WhichFileSize whichSmall = (chField == 'a') ? FILESIZE_FILESIZE : FILESIZE_COMPRESSED;
        const WhichFileSize whichLarge = (chField == 'a') ? FILESIZE_ALLOCATION : FILESIZE_FILESIZE;
        unsigned __int64 cbSmall = pfi->GetFileSize(whichSmall);
        unsigned __int64 cbLarge = pfi->GetFileSize(whichLarge);

        unsigned pct = 0;
        if (cbSmall && cbLarge)
        {
            unsigned __int64 cbDelta = cbLarge - cbSmall;
            float ratio = float(cbDelta) / float(cbLarge);
            pct = unsigned(ratio * 100);
            if (pct > 99)
                pct = 99;
        }

        s.Printf(L"%2u%%", pct);
    }
}

static void FormatOwner(StrW& s, const FileInfo* pfi)
{
    const WCHAR* owner = pfi->GetOwner().Text();
    unsigned width = __wcswidth(owner);
    s.Append(owner);
    s.AppendSpaces(22 - width);
}

/*
 * PictureFormatter.
 */

static bool IsPictureOption(WCHAR ch)
{
    return((ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') ||
           (ch == '?') ||
           (ch == ':'));
}

static void SkipPictureOptions(const WCHAR*& format)
{
    assert(*format);
    while (IsPictureOption(format[1]))
        format++;
    assert(*format);
}

PictureFormatter::PictureFormatter(const DirFormatSettings& settings)
    : m_settings(settings)
{
}

void PictureFormatter::ParsePicture(const WCHAR* picture)
{
    assert(picture);
    assert(!m_picture.Length());

    bool skip = false;
    size_t count = 0;
    unsigned reset_on_skip_len = unsigned(-1);
    bool any_short_filename_fields = false;
    StrW mask;

    m_max_file_width = 0;
    m_max_dir_width = 0;
    m_need_filename_width = false;
    m_need_compressed_size = false;

    while (*picture)
    {
        WCHAR chStyle = 0;
        WCHAR chSubField = 0;

        switch (*picture)
        {
        case 'F':
            {
                unsigned len = 0;
                while (IsPictureOption(picture[1]))
                {
                    switch (picture[1])
                    {
                    case 'f':
                        chStyle = picture[1];
                        break;
                    case 'l':
                    case 'x':
                        chSubField = picture[1];
                        break;
                    default:
                        if (picture[1] >= '0' && picture[1] <= '9')
                        {
                            len = 0;
                            do
                            {
                                len *= 10;
                                len += picture[1] - '0';
                                picture++;
                            }
                            while (picture[1] >= '0' && picture[1] <= '9');
                            if (len < 2)
                                len = 2;
                            picture--;
                        }
                        break;
                    }
                    picture++;
                }
                m_fields.emplace_back();
                FieldInfo* const p = &m_fields.back();
                p->m_field = FLD_FILENAME;
                p->m_chSubField = chSubField;
                p->m_chStyle = chStyle;
                p->m_cchWidth = (chSubField == 'x' || chStyle == 'f') ? 12 : len;
                p->m_auto_width = !p->m_cchWidth;
                p->m_ichInsert = m_picture.Length();
                m_picture.Append('!');
            }
            break;
        case 'X':
            skip = (picture[1] == '?' && !m_settings.IsSet(FMT_SHORTNAMES));
            if (!skip)
            {
                any_short_filename_fields = true;
                m_fields.emplace_back();
                FieldInfo* const p = &m_fields.back();
                p->m_field = FLD_SHORTNAME;
                p->m_cchWidth = 12;
                p->m_ichInsert = m_picture.Length();
                m_picture.Append('!');
            }
            else if (reset_on_skip_len == unsigned(-1))
            {
                m_picture.TrimRight();
            }
            SkipPictureOptions(picture);
            break;
        case 'S':
            {
                while (IsPictureOption(picture[1]))
                {
                    switch (picture[1])
                    {
                    case 'm':
                    case 's':
                        chStyle = picture[1];
                        break;
                    case 'a':
                    case 'c':
                    case 'f':
                        chSubField = picture[1];
                        break;
                    }
                    picture++;
                }
                m_fields.emplace_back();
                FieldInfo* const p = &m_fields.back();
                p->m_field = FLD_FILESIZE;
                p->m_chSubField = chSubField;
                p->m_chStyle = chStyle;
                p->m_cchWidth = GetSizeFieldWidthByStyle(m_settings, chStyle);
                p->m_ichInsert = m_picture.Length();
                m_picture.Append('!');
                if (chSubField == 'c')
                    m_need_compressed_size = true;
            }
            break;
        case 'D':
            {
                while (IsPictureOption(picture[1]))
                {
                    switch (picture[1])
                    {
                    case 'l':
                    case 'm':
                    case 'n':
                    case 's':
                    case 'x':
                        chStyle = picture[1];
                        break;
                    case 'a':
                    case 'c':
                    case 'w':
                        chSubField = picture[1];
                        break;
                    }
                    picture++;
                }
                m_fields.emplace_back();
                FieldInfo* const p = &m_fields.back();
                p->m_field = FLD_DATETIME;
                p->m_chSubField = chSubField;
                p->m_chStyle = chStyle;
                p->m_cchWidth = GetTimeFieldWidthByStyle(m_settings, chStyle);
                p->m_ichInsert = m_picture.Length();
                m_picture.Append('!');
                m_fHasDate = true;
            }
            break;
        case 'C':
            skip = (picture[1] == '?' && !m_settings.IsSet(FMT_COMPRESSED));
            if (!skip)
            {
                while (IsPictureOption(picture[1]))
                {
                    switch (picture[1])
                    {
                    case 'a':
                    case 'c':
                        chSubField = picture[1];
                        break;
                    }
                    picture++;
                }
                m_fields.emplace_back();
                FieldInfo* const p = &m_fields.back();
                p->m_field = FLD_COMPRESSION;
                p->m_chSubField = chSubField;
                p->m_cchWidth = 3;
                p->m_ichInsert = m_picture.Length();
                m_picture.Append('!');
                if (!chSubField || chSubField == 'c')
                    m_need_compressed_size = true;
            }
            else if (reset_on_skip_len == unsigned(-1))
            {
                m_picture.TrimRight();
            }
            SkipPictureOptions(picture);
            break;
        case 'O':
            skip = (picture[1] == '?' && !m_settings.IsSet(FMT_SHOWOWNER));
            if (!skip)
            {
                m_fields.emplace_back();
                FieldInfo* const p = &m_fields.back();
                p->m_field = FLD_OWNER;
                p->m_cchWidth = 22;
                p->m_ichInsert = m_picture.Length();
                m_picture.Append('!');
            }
            else if (reset_on_skip_len == unsigned(-1))
            {
                m_picture.TrimRight();
            }
            SkipPictureOptions(picture);
            break;
        case 'T':
            skip = (picture[1] == '?' && !m_settings.IsSet(FMT_ATTRIBUTES));
            if (!skip)
            {
                mask.Clear();
                while (IsPictureOption(picture[1]))
                {
                    if (picture[1] == '?')
                    {
                        // Ignore.
                    }
                    else if (picture[1] == ':')
                    {
                        picture++;
                        if (!picture[1])
                            break;
                        chStyle = picture[1];
                    }
                    else
                    {
                        mask.Append(picture[1]);
                    }
                    picture++;
                }
                if (!mask.Length())
                    mask.Set(c_attr_mask_default);
                    m_fields.emplace_back();
                FieldInfo* const p = &m_fields.back();
                p->m_field = FLD_ATTRIBUTES;
                p->m_chStyle = chStyle;
                p->m_cchWidth = mask.Length();
                p->m_ichInsert = m_picture.Length();
                p->m_masks = new std::vector<AttrChar>;
                for (unsigned ii = 0; ii < mask.Length(); ii++)
                {
                    for (unsigned jj = _countof(c_attr_chars); jj--;)
                    {
                        if (_totlower(c_attr_chars[jj].ch) == _totlower(mask.Text()[ii]))
                        {
                            p->m_masks->emplace_back(c_attr_chars[jj]);
                            break;
                        }
                    }
                }
                m_picture.Append('!');
            }
            else if (reset_on_skip_len == unsigned(-1))
            {
                m_picture.TrimRight();
            }
            SkipPictureOptions(picture);
            break;

        case ' ':
            m_picture.Append(' ');
            break;
        case '[':
            skip = false;
            count = m_fields.size();
            reset_on_skip_len = m_picture.Length();
            break;
        case ']':
            if (skip && m_fields.size() - count <= 1 && reset_on_skip_len < m_picture.Length())
                m_picture.SetLength(reset_on_skip_len);
            skip = false;
            reset_on_skip_len = unsigned(-1);
            break;
        case '\\':
            m_picture.Append(picture[1]);
            picture++;
            break;

        default:
            m_picture.Append(*picture);
            break;
        }

        picture++;
    }

    // Post-processing to fill in implied subfields, etc.

    if (!any_short_filename_fields && m_settings.IsSet(FMT_SHORTNAMES))
    {
        for (size_t ii = m_fields.size(); ii--;)
        {
            if (m_fields[ii].m_field == FLD_FILENAME &&
                !m_fields[ii].m_chSubField)
            {
                m_fields[ii].m_chSubField = 'x';
            }
        }
    }

    for (size_t ii = m_fields.size(); ii--;)
    {
        if (m_fields[ii].m_field == FLD_FILENAME &&
            !m_fields[ii].m_cchWidth &&
            (ii + 1 < m_fields.size() ||
             m_fields[ii].m_ichInsert + 1 < m_picture.Length() ||
             m_settings.m_num_columns != 1))
        {
            m_need_filename_width = true;
            break;
        }
    }

    m_need_short_filenames = any_short_filename_fields || m_settings.IsSet(FMT_SHORTNAMES);
}

void PictureFormatter::SetMaxFileDirWidth(unsigned max_file_width, unsigned max_dir_width)
{
    m_max_file_width = max_file_width;
    m_max_dir_width = max_dir_width;
}

unsigned PictureFormatter::GetMaxWidth(unsigned fit_in_width, bool recalc_auto_width_fields)
{
    unsigned distribute = 0;

    assert(m_picture.Length() >= m_fields.size());

    unsigned width = unsigned(m_picture.Length() - m_fields.size());
    for (size_t ii = m_fields.size(); ii--;)
    {
        if (recalc_auto_width_fields && m_fields[ii].m_auto_width)
            m_fields[ii].m_cchWidth = 0;
        if (m_fields[ii].m_field == FLD_FILENAME)
        {
            const unsigned filename_width = GetFilenameFieldWidth(m_settings, &m_fields[ii], 0, 0);
            width += filename_width;
            if (!filename_width)
                distribute++;
        }
        else
        {
            assert(!m_fields[ii].m_auto_width);
            width += m_fields[ii].m_cchWidth;
        }
    }
    if (distribute)
    {
        const unsigned max_file_width = s_icon_width + m_max_file_width;
        const unsigned max_dir_width = s_icon_width + m_max_dir_width + (m_settings.IsSet(FMT_DIRBRACKETS) ? 2 : 0);
        const unsigned max_entry_width = std::max<unsigned>(std::max<unsigned>(max_file_width, max_dir_width), unsigned(s_icon_width + 1));
        unsigned distribute_width = max_entry_width;
        if (fit_in_width)
        {
            distribute_width = distribute;
            if (width + distribute_width < fit_in_width)
                distribute_width = std::max<unsigned>((fit_in_width - width) / distribute, unsigned(1));
            distribute_width = std::min<unsigned>(distribute_width, max_entry_width);
        }
        for (size_t ii = m_fields.size(); ii--;)
        {
            if (m_fields[ii].m_field == FLD_FILENAME)
            {
                m_fields[ii].m_cchWidth = distribute_width;
                width += distribute_width;
            }
        }

        assert(GetMaxWidth() == width);
    }
    return width;
}

void PictureFormatter::Format(StrW& s, const FileInfo* pfi, const WCHAR* dir, const WIN32_FIND_STREAM_DATA* pfsd, bool one_per_line) const
{
    assert(!s.Length() || s.Text()[s.Length() - 1] == '\n');

    const unsigned max_file_width = m_max_file_width;
    const unsigned max_dir_width = m_max_dir_width + (m_settings.IsSet(FMT_DIRBRACKETS) ? 2 : 0);

    const WCHAR* color = SelectColor(pfi, m_settings.m_flags);

    // Format the fields.

    StrW tmp;
    unsigned ichCopied = 0;
    for (size_t ii = 0; ii < m_fields.size(); ii++)
    {
        // Copy the constant picture text before the field.

        const unsigned ichCopyUpTo = m_fields[ii].m_ichInsert;
        s.Append(m_picture.Text() + ichCopied, ichCopyUpTo - ichCopied);
        ichCopied = ichCopyUpTo + 1;

        // Insert field value.

        if (pfsd)
        {
            switch (m_fields[ii].m_field)
            {
            case FLD_DATETIME:
            case FLD_COMPRESSION:
            case FLD_ATTRIBUTES:
            case FLD_OWNER:
            case FLD_SHORTNAME:
// TODO: Color matters here because of background colors and columns.
                s.AppendSpaces(m_fields[ii].m_cchWidth);
                break;
            case FLD_FILESIZE:
                {
                    const WCHAR* size_color = GetSizeColor(pfsd->StreamSize.QuadPart);
// TODO: min/max for stream sizes?
                    FormatSize(s, pfsd->StreamSize.QuadPart, nullptr, m_settings, GetEffectiveSizeFieldStyle(m_settings, m_fields[ii].m_chStyle), size_color ? size_color : color);
                }
                break;
            case FLD_FILENAME:
                {
                    if (m_settings.IsSet(FMT_REDIRECTED))
                    {
                        tmp.Clear();
                        if (m_settings.IsSet(FMT_FULLNAME))
                            tmp.Append(dir);
                        tmp.Append(pfi->GetLongName());
                    }
                    else
                        tmp.Set(L"  ");
                    tmp.Append(pfsd->cStreamName);
                    if (m_settings.IsSet(FMT_LOWERCASE))
                        tmp.ToLower();

                    const bool fLast = (ii + 1 == m_fields.size() &&
                                        m_fields[ii].m_ichInsert + 1 == m_picture.Length());
                    unsigned width = GetFilenameFieldWidth(m_settings, &m_fields[ii], max_file_width, max_dir_width);

                    if (fLast && (m_settings.IsSet(FMT_FULLNAME)))
                        width = 0;

                    if (width)
                    {
                        const unsigned used = TruncateWcwidth(tmp, width, GetTruncationCharacter());
                        tmp.AppendSpaces(width - used);
                    }

                    if (fLast)
                        tmp.TrimRight();

                    s.Append(tmp);
                }
                break;
            default:
                assert(false);
                break;
            }
        }
        else
        {
            switch (m_fields[ii].m_field)
            {
            case FLD_DATETIME:
                FormatTime(s, pfi, m_settings, m_fields[ii].m_chStyle, m_fields[ii].m_chSubField, color);
                break;
            case FLD_FILESIZE:
                FormatFileSize(s, pfi, m_settings, m_fields[ii].m_chStyle, m_fields[ii].m_chSubField, color);
                break;
            case FLD_COMPRESSION:
// TODO: Color?
                FormatCompressed(s, pfi, m_fields[ii].m_chSubField);
                break;
            case FLD_ATTRIBUTES:
// TODO: Color?
                FormatAttributes(s, pfi->GetAttributes(), m_fields[ii].m_masks, m_fields[ii].m_chStyle);
                break;
            case FLD_OWNER:
// TODO: Color?
                FormatOwner(s, pfi);
                break;
            case FLD_SHORTNAME:
                FormatFilename(s, pfi, m_settings.m_flags|FMT_SHORTNAMES|FMT_FAT|FMT_ONLYSHORTNAMES, 0, dir, color);
                break;
            case FLD_FILENAME:
                {
                    DWORD flags = m_settings.m_flags;
                    if (m_fields[ii].m_chSubField == 'x')
                        flags |= FMT_SHORTNAMES|FMT_FAT;
                    else
                        flags &= ~FMT_SHORTNAMES;
                    if (m_fields[ii].m_chStyle == 'f')
                        flags |= FMT_FAT;

                    const bool fLast = (ii + 1 == m_fields.size() &&
                                        m_fields[ii].m_ichInsert + 1 == m_picture.Length());
                    unsigned width = GetFilenameFieldWidth(m_settings, &m_fields[ii], max_file_width, max_dir_width);

                    if (!fLast)
                        flags &= ~FMT_FULLNAME;
                    if (flags & FMT_FULLNAME)
                        width = 0;

                    FormatFilename(s, pfi, flags, width, dir, color);
                    if (fLast)
                    {
                        s.TrimRight();
                        if (dir && one_per_line)
                            FormatReparsePoint(s, pfi, flags, dir);
                    }
                }
                break;
            default:
                assert(false);
                break;
            }
        }
    }

    // Append any trailing constant picture text.

    s.Append(m_picture.Text() + ichCopied, m_picture.Length() - ichCopied);
}

DirEntryFormatter::DirEntryFormatter()
    : m_picture(m_settings)
{
    g_settings = &m_settings;
}

DirEntryFormatter::~DirEntryFormatter()
{
    g_settings = nullptr;
}

void DirEntryFormatter::Initialize(unsigned num_columns, const DWORD flags, WhichTimeStamp whichtimestamp, WhichFileSize whichfilesize, DWORD dwAttrIncludeAny, DWORD dwAttrMatch, DWORD dwAttrExcludeAny, const WCHAR* disable_options, const WCHAR* picture)
{
    m_root_pass = false;
    m_count_usage_dirs = 0;

    m_hout = GetStdHandle(STD_OUTPUT_HANDLE);
    m_settings.m_num_columns = num_columns;
    m_settings.m_flags = flags;
    m_settings.m_whichtimestamp = whichtimestamp;
    m_settings.m_whichfilesize = whichfilesize;
    m_settings.m_dwAttrIncludeAny = dwAttrIncludeAny;
    m_settings.m_dwAttrMatch = dwAttrMatch;
    m_settings.m_dwAttrExcludeAny = dwAttrExcludeAny;
    if (disable_options)
        m_settings.m_sDisableOptions.Set(disable_options);
    m_settings.m_need_compressed_size = (whichfilesize == FILESIZE_COMPRESSED ||
                                         (flags & FMT_COMPRESSED) ||
                                         wcschr(g_sort_order, 'c'));
    m_settings.m_need_short_filenames = !!(flags & FMT_SHORTNAMES);

    DWORD dwMode;
    if (!GetConsoleMode(m_hout, &dwMode))
        m_settings.m_flags |= FMT_REDIRECTED;

    if (!CanUseEscapeCodes(m_hout))
    {
        m_settings.m_flags &= ~FMT_HYPERLINKS;
    }

    // Initialize the picture formatter.

    StrW sPic;

    if (!picture)
    {
        switch (m_settings.m_num_columns)
        {
        case 4:
            if (m_settings.IsSet(FMT_FAT) || m_settings.IsSet(FMT_MINISIZE))
                picture = L"F12 Sm";
            else
                picture = L"F17";
            break;
        case 2:
            if (m_settings.IsSet(FMT_FAT))
                picture = L"F Ss D";
            else
            {
                unsigned width = 38;
                StrW tmp;
                if (m_settings.IsSet(FMT_MINISIZE))
                {
                    tmp.Append(L" Sm");
                    width -= 1 + 4;
                }
                if (m_settings.IsSet(FMT_MINIDATE))
                {
                    if (tmp.Length())
                    {
                        tmp.Append(L" ");
                        width -= 1;
                    }
                    tmp.Append(L" Dm");
                    width -= 1 + 11;
                }
                else if (m_settings.IsSet(FMT_WIDELISTIME))
                {
                    if (tmp.Length())
                    {
                        tmp.Append(L" ");
                        width -= 1;
                    }
                    tmp.Append(L" Dn");
                    width -= 1 + 17;
                }
                sPic.Printf(L"F%u%s", width, tmp.Text());
                picture = sPic.Text();
            }
            break;
        case 1:
            if (m_settings.IsSet(FMT_FAT))
                picture = L"F Ss D  C?  T?";
            else
                picture = L"D  S  C?  T?  X?  O?  F";
            break;
        case 0:
            sPic.Set(L"F");
            if (m_settings.IsSet(FMT_FAT) ||
                m_settings.IsSet(FMT_MINISIZE))
                sPic.Append(L" Sm");
            if (m_settings.IsSet(FMT_MINIDATE))
                sPic.Append(L" Dm");
            else if (m_settings.IsSet(FMT_WIDELISTIME))
                sPic.Append(L" D");
            picture = sPic.Text();
            break;
        default:
            assert(false);
            break;
        }
    }

    m_picture.ParsePicture(picture);

    if (m_picture.IsCompressedSizeNeeded())
        m_settings.m_need_compressed_size = true;
    if (m_picture.AreShortFilenamesNeeded())
        m_settings.m_need_short_filenames = true;
}

bool DirEntryFormatter::IsOnlyRootSubDir() const
{
    return m_subdirs.empty() && IsRootSubDir();
}

bool DirEntryFormatter::IsRootSubDir() const
{
    return m_root_pass;
}

bool DirEntryFormatter::IsNewRootGroup(const WCHAR* dir) const
{
    // If the pattern is implicit, then it's only a new root group if we
    // don't yet have a root group.

    if (m_implicit)
        return !m_root_group.Length();

    // If we don't yet have a root group, then it's a new root group.

    if (!m_root_group.Length())
        return true;

    // If the root part is different, then it's a new root group.

    StrW tmp;
    tmp.Set(dir);
    EnsureTrailingSlash(tmp);
    if (tmp.Length() < m_root.Length())
        return true;
    tmp.SetLength(m_root.Length());
    if (!tmp.EqualI(m_root))
        return true;

    // At this point we know the root matches.  If the root matches but
    // the next level is different, then it's a new root group.

    tmp.Set(dir + m_root.Length());
    EnsureTrailingSlash(tmp);
    for (unsigned ii = 0; ii < tmp.Length(); ii++)
    {
        if (tmp.Text()[ii] == '\\')
        {
            tmp.SetLength(ii + 1);
            break;
        }
    }
    if (!tmp.EqualI(m_root_group.Text() + m_root.Length()))
        return true;

    // At this point we know the whole m_root_group portion matches, so
    // it's not a new root group.

    return false;
}

void DirEntryFormatter::UpdateRootGroup(const WCHAR* dir)
{
#ifdef DEBUG
    assert(m_root.Length());
    StrW tmp;
    tmp.Set(dir);
    EnsureTrailingSlash(tmp);
    assert(m_root.Length() <= tmp.Length());
    tmp.SetLength(m_root.Length());
    assert(tmp.EqualI(m_root));
#endif

    m_root_group.Set(dir);
    EnsureTrailingSlash(m_root_group);
    for (unsigned ii = m_root.Length(); ii < m_root_group.Length(); ii++)
    {
        if (m_root_group.Text()[ii] == '\\')
        {
            m_root_group.SetLength(ii + 1);
            break;
        }
    }
}

bool DirEntryFormatter::OnVolumeBegin(const WCHAR* dir, Error& e)
{
    StrW root;
    WCHAR volume_name[MAX_PATH];
    DWORD dwSerialNumber;
    const bool line_break_before_volume = m_line_break_before_volume;

    m_cFilesTotal = 0;
    m_cbTotalTotal = 0;
    m_cbAllocatedTotal = 0;
    m_cbCompressedTotal = 0;
    m_line_break_before_volume = true;

    if (Settings().IsOptionDisabled('v') ||
        Settings().IsOptionDisabled('h'))
        return false;

    if (Settings().IsSet(FMT_USAGE))
    {
        StrW s;
        if (line_break_before_volume)
            s.Append('\n');
        const unsigned size_field_width = GetSizeFieldWidthByStyle(Settings(), 0);
        s.Printf(L"%*.*s  %*.*s  %7s\n",
                 size_field_width, size_field_width, L"Used",
                 size_field_width, size_field_width, L"Allocated",
                 L"Files");
        OutputConsole(m_hout, s.Text(), s.Length());
        m_count_usage_dirs = 0;
        return true;
    }

    if (Settings().IsSet(FMT_BARE))
        return false;

    if (!GetDrive(dir, root, e))
    {
        e.Clear();
        return false;
    }

    EnsureTrailingSlash(root);
    if (!GetVolumeInformation(root.Text(), volume_name, _countof(volume_name), &dwSerialNumber, 0, 0, 0, 0))
    {
        const DWORD dwErr = GetLastError();
        if (dwErr != ERROR_DIR_NOT_ROOT)   // Don't fail on subst'd drives.
            e.Set(dwErr);
        return false;
    }

    if (*root.Text() == '\\')
        StripTrailingSlashes(root);
    else
        root.SetLength(1);

    StrW s;
    if (line_break_before_volume)
        s.Append('\n');
    if (volume_name[0])
        s.Printf(L" Volume in drive %s is %s\n", root.Text(), volume_name);
    else
        s.Printf(L" Volume in drive %s has no label.\n", root.Text());
    s.Printf(L" Volume Serial Number is %04X-%04X\n",
             HIWORD(dwSerialNumber), LOWORD(dwSerialNumber));
    OutputConsole(m_hout, s.Text(), s.Length());
    return true;
}

void DirEntryFormatter::OnScanFiles(const WCHAR* dir, const WCHAR* pattern, bool implicit, bool root_pass)
{
    m_implicit = implicit;
    m_root_pass = root_pass;
    if (m_root_pass)
    {
        m_root.Set(dir);
        EnsureTrailingSlash(m_root);
        m_root_group.Clear();
    }
}

void DirEntryFormatter::OnDirectoryBegin(const WCHAR* const dir)
{
    const bool fReset = (Settings().IsSet(FMT_USAGEGROUPED) ?
                         IsRootSubDir() || IsNewRootGroup(dir) :
                         !m_dir.EqualI(dir));
    if (fReset)
    {
        UpdateRootGroup(dir);
        Settings().ClearMinMax();
    }

    m_files.clear();
    m_longest_file_width = 0;
    m_longest_dir_width = 0;
    m_granularity = 0;

    DWORD dwSectorsPerCluster;
    DWORD dwBytesPerSector;
    DWORD dwFreeClusters;
    DWORD dwTotalClusters;
    if (GetDiskFreeSpace(dir, &dwSectorsPerCluster, &dwBytesPerSector, &dwFreeClusters, &dwTotalClusters))
        m_granularity = dwSectorsPerCluster * dwBytesPerSector;

    m_dir.Clear();
    if (Settings().IsSet(FMT_SHORTNAMES))
    {
        StrW short_name;
        short_name.ReserveMaxPath();
        const DWORD len = GetShortPathName(dir, short_name.Reserve(), short_name.Capacity());
        if (len && len < short_name.Capacity())
            m_dir.Set(short_name);
    }
    if (!m_dir.Length())
        m_dir.Set(dir);

    if (!Settings().IsSet(FMT_BARE) &&
        !Settings().IsOptionDisabled('h'))
    {
        StrW s;
        s.Printf(L"\n Directory of %s%s\n\n", dir, wcschr(dir, '\\') ? L"" : L"\\");
        OutputConsole(m_hout, s.Text(), s.Length());
    }
}

void DirEntryFormatter::DisplayOne(const FileInfo* const pfi)
{
    StrW s;

    if (m_settings.IsSet(FMT_BARE))
    {
        if (pfi->IsPseudoDirectory())
            return;

        if (m_settings.IsSet(FMT_SUBDIRECTORIES))
        {
            s.Set(m_dir);
            EnsureTrailingSlash(s);
        }
        FormatFilename(s, pfi, m_settings.m_flags, 0, m_dir.Text());
    }
    else
    {
        m_picture.Format(s, pfi, m_dir.Text());
    }

    if (m_settings.IsSet(FMT_ALTDATASTEAMS|FMT_ONLYALTDATASTREAMS))
    {
        assert(implies(m_settings.IsSet(FMT_ALTDATASTEAMS), !m_settings.IsSet(FMT_FAT|FMT_BARE)));

        StrW tmp;
        tmp.Set(m_dir);
        EnsureTrailingSlash(tmp);
        tmp.Append(pfi->GetLongName());

        StrW full;
        full.Set(tmp);

        bool fAnyAltDataStreams = false;
        WIN32_FIND_STREAM_DATA fsd;
        SHFind shFind = __FindFirstStreamW(full.Text(), FindStreamInfoStandard, &fsd, 0);
        if (!shFind.Empty())
        {
            do
            {
                if (!wcsicmp(fsd.cStreamName, L"::$DATA"))
                    continue;
                fAnyAltDataStreams = true;
                if (!m_settings.IsSet(FMT_ALTDATASTEAMS))
                    break;
                s.Append('\n');
                m_picture.Format(s, pfi, m_dir.Text(), &fsd);
            }
            while (__FindNextStreamW(shFind, &fsd));
        }

        if (fAnyAltDataStreams)
            pfi->SetAltDataStreams();
        else if (m_settings.IsSet(FMT_ONLYALTDATASTREAMS))
            return;
    }

    s.Append(L"\n");
    OutputConsole(m_hout, s.Text(), s.Length());
}

void DirEntryFormatter::OnFile(const WCHAR* const dir, const WIN32_FIND_DATA* const pfd)
{
    FileInfo* pfi;
    std::unique_ptr<FileInfo> spfi;
    const bool fUsage = Settings().IsSet(FMT_USAGE);
    const bool fImmediate = (!s_gradient &&
                             !*g_sort_order &&
                             Settings().m_num_columns == 1 &&
                             !m_picture.IsFilenameWidthNeeded());

    if (fImmediate || fUsage)
    {
        spfi = std::make_unique<FileInfo>();
        pfi = spfi.get();
    }
    else
    {
        m_files.emplace_back();
        pfi = &m_files.back();
    }

    pfi->Init(dir, m_granularity, pfd, Settings());

    if (fImmediate && !fUsage)
        DisplayOne(pfi);

    // Update statistics.

    if (Settings().IsSet(FMT_ONLYALTDATASTREAMS) && !pfi->HasAltDataStreams())
    {
        if (!fImmediate)
            m_files.erase(m_files.begin() + m_files.size() - 1);
    }
    else
    {
        const DWORD flags = Settings().m_flags;
        const unsigned num_columns = Settings().m_num_columns;

        if (pfi->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY)
        {
            m_cDirs++;
            if (!num_columns || m_picture.IsFilenameWidthNeeded())
            {
                unsigned name_width = __wcswidth(pfi->GetFileName(flags).Text());
                if ((flags & (FMT_CLASSIFY|FMT_DIRBRACKETS)) == FMT_CLASSIFY)
                    ++name_width;   // For appending '\' symbol.
                if (m_longest_dir_width < name_width)
                    m_longest_dir_width = name_width;
            }
        }
        else
        {
            m_cFiles++;
            m_cbTotal += pfi->GetFileSize();
            m_cbAllocated += pfi->GetFileSize(FILESIZE_ALLOCATION);
            if (Settings().IsSet(FMT_COMPRESSED))
                m_cbCompressed += pfi->GetFileSize(FILESIZE_COMPRESSED);
            if (!num_columns || m_picture.IsFilenameWidthNeeded())
            {
                unsigned name_width = __wcswidth(pfi->GetFileName(flags).Text());
                if ((flags & (FMT_CLASSIFY|FMT_FAT|FMT_JUSTIFY_NONFAT)) == FMT_CLASSIFY && pfi->IsSymLink())
                    ++name_width;   // For appending '@' symbol.
                if (m_longest_file_width < name_width)
                    m_longest_file_width = name_width;
                if (m_picture.IsFilenameWidthNeeded())
                {
                    const bool isFAT = Settings().IsSet(FMT_FAT);
                    if ((Settings().IsSet(FMT_JUSTIFY_FAT) && isFAT) ||
                        (Settings().IsSet(FMT_JUSTIFY_NONFAT) && !isFAT))
                    {
                        const WCHAR* name = pfi->GetFileName(flags).Text();
                        const WCHAR* ext = FindExtension(name);
                        unsigned noext_width = 0;
                        if (ext)
                            noext_width = __wcswidth(name, unsigned(ext - name));
                        if (!noext_width)
                            noext_width = name_width;
                        if (m_longest_file_width < noext_width + 4)
                            m_longest_file_width = noext_width + 4;
                    }
                }
            }
            if (s_gradient)
            {
                if (s_scale_size)
                {
                    for (WhichFileSize which = FILESIZE_ARRAY_SIZE; which = WhichFileSize(int(which) - 1);)
                        Settings().UpdateMinMaxSize(which, pfi->GetFileSize(which));
                }
                if (s_scale_time)
                {
                    for (WhichTimeStamp which = TIMESTAMP_ARRAY_SIZE; which = WhichTimeStamp(int(which) - 1);)
                        Settings().UpdateMinMaxTime(which, pfi->GetFileTime(which));
                }
            }
        }
    }
}

void DirEntryFormatter::OnFileNotFound()
{
    // File Not Found is not a fatal error:  report it and continue.
    Error e;
    e.Set(L"File Not Found");
    e.Report();
}

static void FormatTotalCount(StrW& s, unsigned c, const DirFormatSettings& settings)
{
    const bool fCompressed = settings.IsSet(FMT_COMPRESSED);
    const unsigned width = fCompressed ? 13 : 16;

    FormatSizeForReading(s, c, width, settings);
}

static void FormatFileTotals(StrW& s, unsigned cFiles, unsigned __int64 cbTotal, unsigned __int64 cbAllocated, unsigned __int64 cbCompressed, const DirFormatSettings& settings)
{
    const bool fCompressed = settings.IsSet(FMT_COMPRESSED);
    const unsigned width = fCompressed ? 13 : 15;

    FormatTotalCount(s, cFiles, settings);
    s.Append(L" File(s) ");
    FormatSizeForReading(s, cbTotal, 14, settings);
    s.Append(L" bytes");
    if (fCompressed && cbCompressed)
    {
        s.Append(L"  ");
        FormatCompressed(s, cbCompressed, cbTotal, FILE_ATTRIBUTE_COMPRESSED);
    }
    if (cbAllocated)
    {
        s.Append(' ');
        FormatSizeForReading(s, cbAllocated, width, settings);
        s.Append(L" bytes allocated");
    }
}

void DirEntryFormatter::OnDirectoryEnd(bool next_dir_is_different)
{
    bool do_end = next_dir_is_different;

    if (Settings().IsSet(FMT_USAGEGROUPED))
        do_end = (m_subdirs.empty() ||
                  IsNewRootGroup(m_subdirs[0].Text()));

    if (do_end)
    {
        m_cFilesTotal += m_cFiles;
        m_cbTotalTotal += m_cbTotal;
        m_cbAllocatedTotal += m_cbAllocated;
        m_cbCompressedTotal += m_cbCompressed;
    }

    // List any files not already listed by OnFile.

    if (!m_files.empty())
    {
        // Sort files.

        if (*g_sort_order)
            std::sort(m_files.begin(), m_files.end(), CmpFileInfo);

        // List files.

        m_picture.SetMaxFileDirWidth(m_longest_file_width, m_longest_dir_width);

        const unsigned num_columns = Settings().m_num_columns;
        switch (num_columns)
        {
        case 1:
            {
                for (size_t ii = 0; ii < m_files.size(); ii++)
                {
                    const FileInfo* const pfi = &m_files[ii];

                    DisplayOne(pfi);
                }
            }
            break;

        case 0:
        case 2:
        case 4:
            {
                assert(!Settings().IsSet(FMT_BARE));
                assert(!Settings().IsSet(FMT_COMPRESSED));
                assert(!Settings().IsSet(FMT_ATTRIBUTES));

                const bool isFAT = Settings().IsSet(FMT_FAT);
                unsigned console_width = LOWORD(GetConsoleColsRows(m_hout));
                if (!console_width)
                    console_width = 80;

                if (s_use_icons && isFAT)
                {
                    if ((num_columns == 2 && console_width <= (s_icon_width + 38) * 2 + 3) ||
                        (num_columns == 4 && console_width <= (s_icon_width + 17) * 4 + 3))
                    {
                        s_use_icons = false;
                        s_icon_width = 0;
                    }
                }

                StrW s;
                const unsigned max_per_file_width = m_picture.GetMaxWidth(Settings().IsSet(FMT_REDIRECTED) ? 0 : console_width - 1, true);
                const bool vertical = Settings().IsSet(FMT_SORTVERTICAL);
                const unsigned spacing = (num_columns != 0 || isFAT || m_picture.HasDate()) ? 3 : 2;
                const unsigned num_per_row = std::max<unsigned>((console_width + spacing - 1) / (max_per_file_width + spacing), unsigned(1));
                const unsigned num_rows = unsigned(m_files.size() + num_per_row - 1) / num_per_row;
                const unsigned num_add = vertical ? num_rows : 1;

                assert(implies(console_width >= 80, num_columns * (max_per_file_width + 3) < console_width + 3));

                for (unsigned ii = 0; ii < num_rows; ii++)
                {
                    unsigned iItem = vertical ? ii : ii * num_per_row;
                    for (unsigned jj = 0; jj < num_per_row && iItem < m_files.size(); jj++, iItem += num_add)
                    {
                        const FileInfo* pfi = &m_files[iItem];

                        if (jj)
                        {
                            const unsigned width = cell_count(s.Text());
                            assert_width(max_per_file_width >= width);
                            const unsigned spaces = max_per_file_width - width + spacing;
                            s.Clear();
                            s.AppendSpaces(spaces);
                            OutputConsole(m_hout, s.Text(), s.Length());
                        }

                        s.Clear();
                        m_picture.Format(s, pfi, m_dir.Text(), nullptr, false/*one_per_line*/);
                        OutputConsole(m_hout, s.Text(), s.Length());
                    }

                    s.Set(L"\n");
                    OutputConsole(m_hout, s.Text(), s.Length());
                }
            }
            break;

        default:
            assert(false);
            break;
        }
    }

    // Display summary.

    if (do_end)
    {
        if (Settings().IsSet(FMT_USAGE))
        {
            StrW s;
            StrW dir;
            FormatSize(s, m_cbTotal, nullptr, Settings(), 0);
            s.Append(L"  ");
            FormatSize(s, m_cbAllocated, nullptr, Settings(), 0);
            s.Printf(L"  %7u  ", CountFiles());
            dir.Set(Settings().IsSet(FMT_USAGEGROUPED) ? m_root_group : m_dir);
            StripTrailingSlashes(dir);
            if (Settings().IsSet(FMT_LOWERCASE))
                dir.ToLower();
            s.Append(dir);
            s.Append('\n');
            OutputConsole(m_hout, s.Text(), s.Length());
            m_count_usage_dirs++;
        }
        else if (!Settings().IsSet(FMT_BARE) &&
                 !Settings().IsOptionDisabled('s'))
        {
            StrW s;
            FormatFileTotals(s, CountFiles(), m_cbTotal, m_cbAllocated, m_cbCompressed, Settings());
            s.Append('\n');
            OutputConsole(m_hout, s.Text(), s.Length());
        }

        m_cFiles = 0;
        m_cbTotal = 0;
        m_cbAllocated = 0;
        m_cbCompressed = 0;
    }
}

void DirEntryFormatter::OnVolumeEnd(const WCHAR* dir)
{
    if (Settings().IsOptionDisabled('s'))
        return;

    Error e;
    StrW root;
    if (!GetDrive(dir, root, e))
        return;

    if (Settings().IsSet(FMT_USAGE))
    {
        if (m_count_usage_dirs > 1)
        {
            StrW s;

            FormatSize(s, m_cbTotalTotal, nullptr, Settings(), 0);
            s.Append(L"  ");
            FormatSize(s, m_cbAllocatedTotal, nullptr, Settings(), 0);
            s.Printf(L"  %7u  Total\n", m_cFilesTotal);

            ULARGE_INTEGER ulFreeToCaller;
            ULARGE_INTEGER ulTotalSize;
            ULARGE_INTEGER ulTotalFree;
            if (SHGetDiskFreeSpace(root.Text(), &ulFreeToCaller, &ulTotalSize, &ulTotalFree))
            {
                ULARGE_INTEGER ulTotalUsed;
                ulTotalUsed.QuadPart = ulTotalSize.QuadPart - ulTotalFree.QuadPart;

                FormatSizeForReading(s, *reinterpret_cast<unsigned __int64*>(&ulTotalUsed), 0, Settings());
                s.Append('/');
                FormatSizeForReading(s, *reinterpret_cast<unsigned __int64*>(&ulTotalSize), 0, Settings());

                double dInUse = double(ulTotalUsed.QuadPart) / double(ulTotalSize.QuadPart);
                s.Printf(L"  %.1f%% of disk in use\n", dInUse * 100);
            }

            OutputConsole(m_hout, s.Text(), s.Length());
        }
        return;
    }

    if (Settings().IsSet(FMT_BARE))
        return;

    // Display total files and sizes, if recursing into subdirectories.

    StrW s;

    if (Settings().IsSet(FMT_SUBDIRECTORIES))
    {
        s.Set(L"\n  ");
        if (!Settings().IsSet(FMT_COMPRESSED))
            s.Append(L"   ");
        s.Append(L"Total Files Listed:\n");
        FormatFileTotals(s, m_cFilesTotal, m_cbTotalTotal, m_cbAllocatedTotal, m_cbCompressedTotal, Settings());
        s.Append('\n');
        OutputConsole(m_hout, s.Text(), s.Length());
        s.Clear();
    }

    // Display count of directories, and bytes free on the volume.  For
    // similarity with CMD the count of directories includes the "." and
    // ".." pseudo-directories, even though it is a bit inaccurate.

    ULARGE_INTEGER ulFreeToCaller;
    ULARGE_INTEGER ulTotalSize;
    ULARGE_INTEGER ulTotalFree;

    FormatTotalCount(s, CountDirs(), Settings());
    s.Append(L" Dir(s) ");
    if (SHGetDiskFreeSpace(root.Text(), &ulFreeToCaller, &ulTotalSize, &ulTotalFree))
    {
        FormatSizeForReading(s, *reinterpret_cast<unsigned __int64*>(&ulFreeToCaller), 15, Settings());
        s.Printf(L" bytes free");
    }

    s.Append('\n');
    OutputConsole(m_hout, s.Text(), s.Length());
}

void DirEntryFormatter::AddSubDir(const StrW& dir)
{
    assert(Settings().IsSet(FMT_SUBDIRECTORIES));
    assert(!IsPseudoDirectory(dir.Text()));

    m_subdirs.emplace_back();
    StrW* subdir = &m_subdirs.back();
    subdir->Set(dir);
}

void DirEntryFormatter::SortSubDirs()
{
    if (!m_subdirs.empty())
        std::sort(m_subdirs.begin(), m_subdirs.end(), CmpSubDirs);
}

bool DirEntryFormatter::NextSubDir(StrW& dir)
{
    if (m_subdirs.empty())
    {
        dir.Clear();
        return false;
    }

    dir.Set(*m_subdirs.begin());
    m_subdirs.erase(m_subdirs.begin());
    return true;
}

