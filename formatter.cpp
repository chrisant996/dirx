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
#include "columns.h"

#include <algorithm>
#include <memory>

//static WCHAR s_chTruncated = L'\x2192'; // Right arrow character.
//static WCHAR s_chTruncated = L'\x25b8'; // Black Right-Pointing Small Triangle character.
static WCHAR s_chTruncated = L'\x2026'; // Horizontal Ellipsis character.
static bool s_can_autofit = true;
static bool s_use_icons = false;
static BYTE s_icon_padding = 1;
static unsigned s_icon_width = 0;
static bool s_mini_bytes = false;
static bool s_scale_size = false;
static bool s_scale_time = false;
static bool s_gradient = true;
static WCHAR s_time_style = 0;
constexpr unsigned c_max_branch_name = 10;

static RepoMap s_repo_map;

static const WCHAR c_hyperlink[] = L"\x1b]8;;";
static const WCHAR c_BEL[] = L"\a";

// #define ASSERT_WIDTH

#if defined(DEBUG) && defined(ASSERT_WIDTH)
#define assert_width        assert
#else
#define assert_width(expr)  do {} while (0)
#endif

static bool ParseHexDigit(WCHAR ch, WORD* digit)
{
    if (ch >= '0' && ch <= '9')
    {
        *digit = ch - '0';
        return true;
    }

    ch = _toupper(ch);
    if (ch >= 'A' && ch <= 'F')
    {
        *digit = ch + 10 - 'A';
        return true;
    }

    return false;
}

void SetTruncationCharacterInHex(const WCHAR* s)
{
    SkipColonOrEqual(s);

    if (s[0] == '$')
        ++s;
    else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;

    WCHAR ch = 0;
    WORD w;

    if (*s && ParseHexDigit(*s, &w))
    {
        ch <<= 4;
        ch |= w;
        ++s;
        if (*s && ParseHexDigit(*s, &w))
        {
            ch <<= 4;
            ch |= w;
            ++s;
#ifdef UNICODE
            if (*s && ParseHexDigit(*s, &w))
            {
                ch <<= 4;
                ch |= w;
                ++s;
                if (*s && ParseHexDigit(*s, &w))
                {
                    ch <<= 4;
                    ch |= w;
                    ++s;
                }
            }
#endif
        }
    }

    if (!*s)
        s_chTruncated = ch;
}

WCHAR GetTruncationCharacter()
{
    return s_chTruncated;
}

void SetCanAutoFit(bool can_autofit)
{
    s_can_autofit = can_autofit;
}

bool SetUseIcons(const WCHAR* s)
{
    if (!s)
        return false;
    if (!_wcsicmp(s, L"") || !_wcsicmp(s, L"auto"))
    {
        DWORD dwMode;
        s_use_icons = !!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &dwMode);
    }
    else if (!_wcsicmp(s, L"always"))
        s_use_icons = true;
    else if (!_wcsicmp(s, L"never") || !_wcsicmp(s, L"-"))
        s_use_icons = false;
    else
        return false;
    s_icon_width = s_use_icons ? 1 + s_icon_padding : 0;
    return true;
}

void SetPadIcons(unsigned spaces)
{
    s_icon_padding = BYTE(clamp<unsigned>(spaces, 1, 4));
    s_icon_width = s_use_icons ? 1 + s_icon_padding : 0;
}

unsigned GetPadIcons()
{
    return s_icon_padding;
}

void SetMiniBytes(bool mini_bytes)
{
    s_mini_bytes = mini_bytes;
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

bool SetDefaultTimeStyle(const WCHAR* time_style)
{
    if (!time_style)
        return false;

    static const WCHAR* c_time_styles[] =
    {
        L"mmini",
        L"iiso",
        L"pcompact",
        L"sshort",
        L"nnormal",
        L"olong-iso",
        L"xfull",
#if 0
        L"ufull-iso",
#endif
        L"rrelative",
        L"llocale",
    };

    for (auto s : c_time_styles)
        if ((time_style[0] == s[0] && !time_style[1]) || wcsicmp(s + 1, time_style) == 0)
        {
            s_time_style = s[0];
            return true;
        }

    return false;
}

static void SelectFileTime(const FileInfo* const pfi, const WhichTimeStamp timestamp, SYSTEMTIME* const psystime)
{
    FILETIME ft;

    FileTimeToLocalFileTime(&pfi->GetFileTime(timestamp), &ft);
    FileTimeToSystemTime(&ft, psystime);
}

static const WCHAR* SelectColor(const FileInfo* const pfi, const FormatFlags flags, const WCHAR* dir, bool ignore_target_color=false)
{
    if (flags & FMT_COLORS)
    {
        const WCHAR* color = LookupColor(pfi, dir, ignore_target_color);
        if (color)
            return color;
    }
    return nullptr;
}

static LCID s_lcid = 0;
static unsigned s_locale_date_time_len = 0;
static WCHAR s_locale_date[80];
static WCHAR s_locale_time[80];
static WCHAR s_locale_monthname[12][10];
static unsigned s_locale_monthname_len[12];
static unsigned s_locale_monthname_longest_len = 1;
static WCHAR s_decimal[2];
static WCHAR s_thousand[2];

void InitLocale()
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

    static const struct { LCTYPE lst; const WCHAR* dflt; } c_monthname_lookup[] =
    {
        { LOCALE_SABBREVMONTHNAME1,  L"Jan" },
        { LOCALE_SABBREVMONTHNAME2,  L"Feb" },
        { LOCALE_SABBREVMONTHNAME3,  L"Mar" },
        { LOCALE_SABBREVMONTHNAME4,  L"Apr" },
        { LOCALE_SABBREVMONTHNAME5,  L"May" },
        { LOCALE_SABBREVMONTHNAME6,  L"Jun" },
        { LOCALE_SABBREVMONTHNAME7,  L"Jul" },
        { LOCALE_SABBREVMONTHNAME8,  L"Aug" },
        { LOCALE_SABBREVMONTHNAME9,  L"Sep" },
        { LOCALE_SABBREVMONTHNAME10, L"Oct" },
        { LOCALE_SABBREVMONTHNAME11, L"Nov" },
        { LOCALE_SABBREVMONTHNAME12, L"Dec" },
    };
    static_assert(_countof(c_monthname_lookup) == 12, "wrong number of month strings");
    static_assert(_countof(c_monthname_lookup) == _countof(s_locale_monthname), "wrong number of month strings");
    for (unsigned i = _countof(c_monthname_lookup); i--;)
    {
        if (!GetLocaleInfo(s_lcid, c_monthname_lookup[i].lst, s_locale_monthname[i], _countof(s_locale_monthname[i])))
            wcscpy_s(s_locale_monthname[i], c_monthname_lookup[i].dflt);
        s_locale_monthname_len[i] = __wcswidth(s_locale_monthname[i]);
        s_locale_monthname_longest_len = clamp<unsigned>(s_locale_monthname_len[i], s_locale_monthname_longest_len, 9);
    }

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
}

static const AttrChar c_attr_chars[] =
{
    { 'r', FILE_ATTRIBUTE_READONLY },
    { 'h', FILE_ATTRIBUTE_HIDDEN },
    { 's', FILE_ATTRIBUTE_SYSTEM },
    { 'a', FILE_ATTRIBUTE_ARCHIVE },
    { 'd', FILE_ATTRIBUTE_DIRECTORY },
    { 'e', FILE_ATTRIBUTE_ENCRYPTED },
    { 'n', FILE_ATTRIBUTE_NORMAL },
    { 't', FILE_ATTRIBUTE_TEMPORARY },
    { 'p', FILE_ATTRIBUTE_SPARSE_FILE },
    { 'c', FILE_ATTRIBUTE_COMPRESSED },
    { 'o', FILE_ATTRIBUTE_OFFLINE },
    { 'i', FILE_ATTRIBUTE_NOT_CONTENT_INDEXED },
    { 'j', FILE_ATTRIBUTE_REPARSE_POINT },
    { 'l', FILE_ATTRIBUTE_REPARSE_POINT },
    //{ 'v', FILE_ATTRIBUTE_DEVICE },
};
static const WCHAR c_attr_mask_default[] = L"darhsj";
static const WCHAR c_attr_mask_all[] = L"darhsjceotpni";

DWORD ParseAttribute(const WCHAR ch)
{
    for (unsigned ii = _countof(c_attr_chars); ii--;)
    {
        if (ch == c_attr_chars[ii].ch)
            return c_attr_chars[ii].dwAttr;
    }
    return 0;
}

static void FormatAttributes(StrW& s, const DWORD dwAttr, const std::vector<AttrChar>* vec, WCHAR chNotSet, bool use_color)
{
    if (!chNotSet)
        chNotSet = '_';

    const WCHAR* prev_color = nullptr;

    for (unsigned ii = 0; ii < vec->size(); ii++)
    {
        const AttrChar* const pAttr = &*(vec->cbegin() + ii);
        const DWORD bit = (dwAttr & pAttr->dwAttr);
        if (use_color)
        {
            const WCHAR* color = GetAttrLetterColor(bit);
            if (color != prev_color)
            {
                if (color)
                    s.Printf(L"\x1b[0;%sm", color);
                else
                    s.Append(c_norm);
                prev_color = color;
            }
        }
        s.Append(bit ? pAttr->ch : chNotSet);
    }

    if (prev_color)
        s.Append(c_norm);
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

    unsigned name_len = name.Length();
    unsigned name_width = __wcswidth(name.Text());
    unsigned ext_width = 0;
    const WCHAR* ext = FindExtension(name.Text());

    if (ext)
    {
        ext_width = __wcswidth(ext);
        name_width -= ext_width;
        name_len = unsigned(ext - name.Text());
        assert(*ext == '.');
        ext++;
        ext_width--;
    }

    if (!ext_width)
    {
        const unsigned combined_width = max_name_width + 1 + max_ext_width;
        if (name_width <= combined_width)
        {
            s.Append(name);
        }
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
        tmp.Set(name.Text(), name_len);
        TruncateWcwidth(tmp, max_name_width, 0);
        tmp.AppendSpaces(max_name_width - name_width);
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
        {
            s.Append(ext);
        }
    }

    assert(max_name_width + 1 + max_ext_width >= __wcswidth(s.Text() + orig_len));
    s.AppendSpaces(max_name_width + 1 + max_ext_width - __wcswidth(s.Text() + orig_len));
}

static void FormatFilename(StrW& s, const FileInfo* pfi, FormatFlags flags, unsigned max_width=0, const WCHAR* dir=nullptr, const WCHAR* color=nullptr, bool show_reparse=false)
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
            s.Printf(L"\x1b[0;%sm", icon_color);
        s.Append(icon ? icon : L" ");
        if (icon_color)
            s.Append(c_norm);
        s.AppendSpaces(s_icon_padding);
        if (max_width)
            max_width -= s_icon_width;
    }

    if (color)
        s.Printf(L"\x1b[0;%sm", color);

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

        if (flags & FMT_LOWERCASE)
        {
            if (!tmp.Length())
                tmp.Set(name);
            tmp.ToLower();
            p = tmp.Text();
            if (max_width)
                name_width = __wcswidth(p);
        }
        else
        {
            if (max_width && tmp.Empty())
                name_width = __wcswidth(p);
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
                if (!show_reparse && pfi->IsReparseTag() && !UseLinkTargetColor())
                    classify = L'@';
                else if (pfi->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY)
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
            if (max_width)
            {
                assert(spaces);
                if (spaces) // Let the output be misaligned instead of crashing.
                    --spaces;
            }
            s.Append(&classify, 1);
        }
        s.AppendSpaces(spaces);
    }

    if (color)
        s.Append(c_norm);
}

static void FormatReparsePoint(StrW& s, const FileInfo* const pfi, const FormatFlags flags, const WCHAR* const dir)
{
    assert(dir);
    assert(pfi->IsReparseTag());

    StrW full;
    PathJoin(full, dir, pfi->GetLongName());

    const bool colors = !!(flags & FMT_COLORS);
    const WCHAR* punct = colors ? GetColorByKey(L"xx") : nullptr;

    SHFile shFile = CreateFile(full.Text(), FILE_READ_EA, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (shFile.Empty())
    {
        s.Append(L" ");
        if (punct)
            s.Printf(L"\x1b[0;%sm", punct);
        s.Append(L"[..]");
        if (punct)
            s.Append(c_norm);
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

        s.Append(L" ");

        if (tmp.Length())
        {
            if (punct)
                s.Printf(L"\x1b[0;%sm", punct);

            s.Append(L"[");

            const WCHAR* name = FindName(tmp.Text());
            const DWORD attr = pfi->GetAttributes();
            const int mode = (attr & FILE_ATTRIBUTE_DIRECTORY) ? S_IFDIR : S_IFREG;
            const WCHAR* color = colors ? LookupColor(name, attr, mode) : nullptr;
            const WCHAR* path_color = colors ? GetColorByKey(L"lp") : nullptr;
            if (!path_color && !color)
            {
                if (punct)
                    s.Append(c_norm);

                s.Append(tmp);
            }
            else
            {
                if (!path_color)
                    path_color = color;
                if (path_color)
                    s.Printf(L"\x1b[0;%sm", path_color);
                else if (punct)
                    s.Append(c_norm);

                s.Append(tmp.Text(), name - tmp.Text());

                if (color)
                    s.Printf(L"\x1b[0;%sm", color);
                else if (path_color)
                    s.Append(c_norm);

                s.Append(name);

            }

            if (punct)
                s.Printf(L"\x1b[0;%sm", punct);

            s.Append(L"]");

            if (punct)
                s.Append(c_norm);
        }
        else
        {
            if (punct)
                s.Printf(L"\x1b[0;%sm", punct);

            s.Append(L"[...]");

            if (punct)
                s.Append(c_norm);
        }
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

    if (settings.IsSet(FMT_COLORS))
    {
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
    }
    if (color)
        s.Printf(L"\x1b[0;%sm", StripLineStyles(color));

    switch (chStyle)
    {
    case 'm':
        {
//#define MORE_PRECISION
#ifdef MORE_PRECISION
            const unsigned iLoFrac = 2;
            const unsigned iHiFrac = 9;
            static const WCHAR c_size_chars[] = { 'b', 'K', 'M', 'G', 'T' };
#else
            const unsigned iLoFrac = 2;
            const unsigned iHiFrac = 2;
            static const WCHAR c_size_chars[] = { 'K', 'K', 'M', 'G', 'T' };
#endif
            static_assert(_countof(c_size_chars) == 5, "size mismatch");

            double dSize = double(cbSize);
            unsigned iChSize = 0;

            while (dSize > 999 && iChSize + 1 < _countof(c_size_chars))
            {
                dSize /= 1024;
                iChSize++;
            }

            if (iChSize >= iLoFrac && iChSize <= iHiFrac && dSize + 0.05 < 10.0)
            {
                dSize += 0.05;
                cbSize = static_cast<unsigned __int64>(dSize * 10);
                s.Printf(L"%I64u.%I64u%c", cbSize / 10, cbSize % 10, c_size_chars[iChSize]);
            }
            else
            {
                dSize += 0.5;
                cbSize = static_cast<unsigned __int64>(dSize);
                if (!iChSize)
                {
                    if (s_mini_bytes && cbSize <= 999)
                    {
                        s.Printf(L"%4I64u", cbSize);
                        // s.Printf(L"%3I64u%c", cbSize, c_size_chars[iChSize]);
                        // s.Printf(L"%I64u.%I64u%c", cbSize / 100, (cbSize / 10) % 10, c_size_chars[iChSize + 1]);
                        break;
                    }

                    // Special case:  show 1..999 bytes as "1K", 0 bytes as "0K".
                    if (cbSize)
                    {
                        cbSize = 1;
                        iChSize++;
                    }
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
        if ((settings.IsSet(FMT_NODIRTAGINSIZE)) ||
            (settings.IsSet(FMT_COLORS) && s_scale_size))
        {
            const unsigned trailing = (chStyle == 's');
            s.AppendSpaces(GetSizeFieldWidthByStyle(settings, chStyle) - 1 - trailing);
            if (settings.IsSet(FMT_COLORS))
                s.Printf(L"\x1b[0;%sm-%s", GetColorByKey(L"xx"), c_norm);
            else
                s.Append(L"-");
            s.AppendSpaces(trailing);
        }
        else
        {
            if (fallback_color)
                s.Printf(L"\x1b[0;%sm", StripLineStyles(fallback_color));

            const unsigned cchField = GetSizeFieldWidthByStyle(settings, chStyle);
            s.Printf(L"%-*s", cchField, tag);

            if (fallback_color)
                s.Append(c_norm);
        }
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
        if (s_time_style)
            chStyle = s_time_style;
        else if (settings.IsSet(FMT_FAT))
            chStyle = 's';
        else if (settings.IsSet(FMT_FULLTIME))
            chStyle = 'x';
        else if (settings.IsSet(FMT_MINIDATE))
            chStyle = 'm';
        else if (s_locale_date_time_len)
            chStyle = 'l';
    }

    return chStyle;
}

static unsigned GetTimeFieldWidthByStyle(const DirFormatSettings& settings, WCHAR chStyle)
{
    switch (GetEffectiveTimeFieldStyle(settings, chStyle))
    {
    case 'l':           assert(s_locale_date_time_len); return s_locale_date_time_len;
#if 0
    case 'u':           return 29;      // "YYYY-MM-DD HH:mm:ss.mmm -0800"
#endif
    case 'r':           return 0;       // Variable width.
    case 'x':           return 24;      // "YYYY/MM/DD  HH:mm:ss.mmm"
    case 'o':           return 16;      // "YYYY-MM-DD HH:mm"
    case 's':           return 15;      // "MM/DD/YY  HH:mm"
    case 'p':           return 12;      // "DD Mmm  YYYY"  or  "DD Mmm HH:mm"
    case 'i':           return 11;      // "MM-DD HH:mm"
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

static const SYSTEMTIME& NowAsLocalSystemTime()
{
    static const SYSTEMTIME now = [](){
        SYSTEMTIME systime;
        GetLocalTime(&systime);
        return systime;
    }();
    return now;
}

static const SYSTEMTIME& NowAsSystemTime()
{
    static const SYSTEMTIME now = [](){
        SYSTEMTIME systime;
        GetSystemTime(&systime);
        return systime;
    }();
    return now;
}

static const FILETIME& NowAsFileTime()
{
    static const FILETIME now = [](){
        FILETIME ft;
        SystemTimeToFileTime(&NowAsSystemTime(), &ft);
        return ft;
    }();
    return now;
}

static void FormatRelativeTime(StrW& s, const FILETIME& ft)
{
    const ULONGLONG now = FileTimeToULONGLONG(NowAsFileTime()) / 10000000;  // Seconds.
    const ULONGLONG then = FileTimeToULONGLONG(ft) / 10000000;              // Seconds.
    LONGLONG delta = now - then;

    if (delta < 1)
    {
        s.Append(L"   now");
        return;
    }

    if (delta < 60)
    {
        s.Printf(L"%3llu s ", delta);
    }

    delta /= 60;    // Minutes.

    if (delta < 60)
        s.Printf(L"%3llu m ", delta);
    else if (delta < 24*60)
        s.Printf(L"%3llu hr", delta / 60);
    else if (delta < 7*24*60)
        s.Printf(L"%3llu dy", delta / (24*60));
    else if (delta < 31*24*60)
        s.Printf(L"%3llu wk", delta / (7*24*60));
    else if (delta < 365*24*60)
        s.Printf(L"%3llu mo", delta / ((365*24*60)/12));
    else if (delta < 100*365*24*60 + 24*24*60)
        s.Printf(L"%3llu yr", delta / (365*24*60));
    else
        s.Printf(L"%3llu C ", delta / (100*365*24*60 + 24*24*60));
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
    if (settings.IsSet(FMT_COLORS))
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
            s.Printf(L"\x1b[0;%sm", StripLineStyles(color));
    }

    switch (chStyle)
    {
    case 'l':
        // Locale format.
        FormatLocaleDateTime(s, &systime);
        break;

    case 'p':
        // Compact format, 12 characters (depending on width of longest
        // abbreviated month name).
        {
            const SYSTEMTIME& systimeNow = NowAsLocalSystemTime();
            const unsigned iMonth = clamp<WORD>(systime.wMonth, 1, 12) - 1;
            const unsigned iMonthFile = unsigned(systime.wYear) * 12 + iMonth;
            const unsigned iMonthNow = unsigned(systimeNow.wYear) * 12 + systimeNow.wMonth - 1;
            const bool fShowYear = (iMonthFile > iMonthNow || iMonthFile + 6 < iMonthNow);
            s.Printf(s_locale_monthname[iMonth]);
            s.AppendSpaces(s_locale_monthname_longest_len - s_locale_monthname_len[iMonth]);
            s.Printf(L" %2u", systime.wDay);
            if (fShowYear)
                s.Printf(L"  %04u", systime.wYear);
            else
                s.Printf(L" %02u:%02u", systime.wHour, systime.wMinute);
        }
        break;

    case 'i':
        // iso format, 11 characters.
        s.Printf(L"%02u-%02u %2u:%02u",
                 systime.wMonth,
                 systime.wDay,
                 systime.wHour,
                 systime.wMinute);
        break;

    case 'o':
        // long-iso format, 16 characters.
        s.Printf(L"%04u-%02u-%02u %2u:%02u",
                 systime.wYear,
                 systime.wMonth,
                 systime.wDay,
                 systime.wHour,
                 systime.wMinute);
        break;

#if 0
    case 'u':
        // full-iso format, 29 characters.
        s.Printf(L"%04u-%02u-%02u %2u:%02u:%02u.%03u %c%02u%02u",
                 systime.wYear,
                 systime.wMonth,
                 systime.wDay,
                 systime.wHour,
                 systime.wMinute,
                 systime.wSecond,
                 systime.wMilliseconds,
                 '?',                   // TODO: - or + of time zone bias
                 0,                     // TODO: hours of time zone bias
                 0);                    // TODO: minutes of time zone bias
        break;
#endif

    case 'r':
        // Variable width.
        FormatRelativeTime(s, pfi->GetFileTime(which));
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
            const SYSTEMTIME& systimeNow = NowAsLocalSystemTime();
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

static void FormatCompressed(StrW& s, const FileInfo* pfi, const FormatFlags flags, const WCHAR* fallback_color, WCHAR chField=0)
{
    const WCHAR* color = (flags & FMT_COLORS) ? GetColorByKey(L"cF") : fallback_color;
    if (color)
        s.Printf(L"\x1b[0;%sm", StripLineStyles(color));

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

    if (color)
        s.Append(c_norm);
}

static void FormatOwner(StrW& s, const FileInfo* pfi, const FormatFlags flags, const WCHAR* fallback_color)
{
    const WCHAR* owner = pfi->GetOwner().Text();
    unsigned width = __wcswidth(owner);

    const WCHAR* color = (flags & FMT_COLORS) ? GetColorByKey(L"oF") : fallback_color;
    if (color)
        s.Printf(L"\x1b[0;%sm", StripLineStyles(color));

    s.Append(owner);
    s.AppendSpaces(22 - width);

    if (color)
        s.Append(c_norm);
}

static void FormatGitFile(StrW& s, const FileInfo* pfi, const WCHAR* dir, const FormatFlags flags, const RepoStatus* repo)
{
    GitFileState staged;
    GitFileState working;

    StrW full;
    PathJoin(full, dir, pfi->GetLongName());

    const auto& status = repo->status.find(full.Text());
    if (status == repo->status.end())
    {
        staged = GitFileState::NONE;
        working = GitFileState::NONE;
    }
    else
    {
        staged = status->second.staged;
        working = status->second.working;
    }

    static struct { WCHAR symbol; WCHAR color_key[3]; } c_symbols[] =
    {
        { '-', L"xx" },
        { 'N', L"ga" }, // GitFileState::NEW
        { 'M', L"gm" }, // GitFileState::MODIFIED
        { 'D', L"gd" }, // GitFileState::DELETED
        { 'R', L"gv" }, // GitFileState::RENAMED
        { 'T', L"gt" }, // GitFileState::TYPECHANGE
        { 'I', L"gi" }, // GitFileState::IGNORED
        { 'U', L"gc" }, // GitFileState::UNMERGED
    };
    static_assert(_countof(c_symbols) == unsigned(GitFileState::COUNT), "wrong number of GitFileState symbols");

    const WCHAR* color1;
    const WCHAR* color2;
    if (flags & FMT_COLORS)
    {
        color1 = GetColorByKey(c_symbols[unsigned(staged)].color_key);
        color2 = GetColorByKey(c_symbols[unsigned(working)].color_key);
        if (!color1)
            color1 = L"";
        if (!color2)
            color2 = L"";
    }
    else
    {
        color1 = nullptr;
        color2 = nullptr;
    }

    if (color1)
        s.Printf(L"\x1b[0;%sm", color1);
    s.Append(&c_symbols[unsigned(staged)].symbol, 1);
    if (color1 != color2)
        s.Printf(L"\x1b[0;%sm", color2);
    s.Append(&c_symbols[unsigned(working)].symbol, 1);
    if (color1)
        s.Append(c_norm);
}

static void FormatGitRepo(StrW& s, const FileInfo* pfi, const WCHAR* dir, const FormatFlags flags, unsigned max_width)
{
    WCHAR status;
    StrW branch;
    unsigned branch_width;

    StrW full;
    PathJoin(full, dir, pfi->GetLongName());

    const WCHAR* color1;
    const WCHAR* color2;
    const auto repo = s_repo_map.Find(full.Text());
    if (repo)
    {
        assert(repo->root.Equal(full.Text()));
        assert(repo->repo);
        status = repo->clean ? '|' : '+';
        branch.Set(repo->branch);
        branch_width = TruncateWcwidth(branch, max_width - 2, GetTruncationCharacter());
        if (flags & FMT_COLORS)
        {
            color1 = GetColorByKey(repo->clean ? L"Gc" : L"Gd");
            if (!color1)
                color1 = L"";
            color2 = GetColorByKey(branch.Empty() ? L"xx" : repo->main ? L"Gm" : L"Go");
            if (!color2)
                color2 = L"";
        }
        if (branch.Empty())
        {
            branch.Set(L"-");
            branch_width = 1;
        }
    }
    else
    {
        status = '-';
        branch.Set(L"-");
        branch_width = 1;
        if (flags & FMT_COLORS)
        {
            color1 = GetColorByKey(L"xx");
            if (!color1)
                color1 = L"";
            color2 = color1;
        }
    }

    if (color1)
        s.Printf(L"\x1b[0;%sm", color1);
    s.Append(&status, 1);
    if (color1)
        s.Append(c_norm);

    s.AppendSpaces(1);

    if (color2)
        s.Printf(L"\x1b[0;%sm", color2);
    s.Append(branch);
    if (color2)
    {
        const WCHAR* pad_color = StripLineStyles(color2);
        if (pad_color != color2)
            s.Printf(L"\x1b[0;%sm", pad_color);
    }
    s.AppendSpaces(max_width - 2 - branch_width);
    if (color2)
        s.Append(c_norm);
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
    assert(implies(m_finished_initial_parse, !m_orig_picture.Empty()));

    bool skip = false;
    size_t count = 0;
    unsigned reset_on_skip_len = unsigned(-1);
    bool any_short_filename_fields = false;
    StrW mask;

    if (!m_finished_initial_parse)
        m_orig_picture.Set(picture);

    m_has_date = false;
    m_has_git = false;
    m_picture.Clear();
    m_fields.clear();

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
                if (m_settings.IsSet(FMT_FULLSIZE) && m_settings.IsSet(FMT_FAT))
                    chStyle = 0;
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
                    case 'i':
                    case 'p':
                    case 's':
                    case 'o':
                    case 'n':
                    case 'x':
                    case 'r':
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
                m_has_date = true;
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
                    mask.Set(m_settings.IsSet(FMT_ALLATTRIBUTES) ? c_attr_mask_all : c_attr_mask_default);
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
        case 'R':
            skip = (picture[1] == '?' && (!m_settings.IsSet(FMT_GITREPOS) ||
                                          (m_finished_initial_parse && !m_any_repo_roots)));
            if (!skip)
            {
                m_fields.emplace_back();
                FieldInfo* const p = &m_fields.back();
                p->m_field = FLD_GITREPO;
                p->m_cchWidth = m_max_branch_width ? 2 + m_max_branch_width : 0;
                p->m_ichInsert = m_picture.Length();
                m_picture.Append('!');
            }
            else if (reset_on_skip_len == unsigned(-1))
            {
                m_picture.TrimRight();
            }
            SkipPictureOptions(picture);
            break;
        case 'G':
            skip = (picture[1] == '?' && (!m_settings.IsSet(FMT_GIT) ||
                                          (m_finished_initial_parse && (!m_repo || !m_repo->repo))));
            if (!skip)
            {
                m_fields.emplace_back();
                FieldInfo* const p = &m_fields.back();
                p->m_field = FLD_GITFILE;
                p->m_cchWidth = 2;
                p->m_ichInsert = m_picture.Length();
                m_picture.Append('!');
                m_has_git = true;
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
        const auto& field = m_fields[ii];
        if (field.m_cchWidth)
            continue;
        switch (field.m_field)
        {
        case FLD_FILENAME:
            if (ii + 1 < m_fields.size() ||
                field.m_ichInsert + 1 < m_picture.Length() ||
                m_settings.m_num_columns != 1)
            {
                m_need_filename_width = true;
                m_immediate = false;
            }
            break;
        case FLD_GITREPO:
            m_need_branch_width = true;
            m_immediate = false;
            break;
        case FLD_DATETIME:
            if (field.m_chStyle == 'r')
            {
                m_need_relative_width = true;
                m_immediate = false;
            }
            break;
        }
    }

    m_need_short_filenames = any_short_filename_fields || m_settings.IsSet(FMT_SHORTNAMES);
    m_finished_initial_parse = true;
}

void PictureFormatter::SetMaxFileDirWidth(unsigned max_file_width, unsigned max_dir_width)
{
    m_max_file_width = max_file_width;
    m_max_dir_width = max_dir_width;

    if (m_need_branch_width)
    {
        const unsigned width = 2 + clamp<unsigned>(m_max_branch_width, 1, c_max_branch_name);
        for (auto& field : m_fields)
            if (field.m_field == FLD_GITREPO)
                field.m_cchWidth = width;
    }

    if (m_need_relative_width)
    {
        const unsigned width = std::max<unsigned>(m_max_relative_width, 1);
        for (auto& field : m_fields)
            if (field.m_field == FLD_DATETIME && field.m_chStyle == 'r')
                field.m_cchWidth = width;
    }

    if (m_settings.IsSet(FMT_GIT|FMT_GITREPOS))
        ParsePicture(m_orig_picture.Text());
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

unsigned PictureFormatter::GetMinWidth(const FileInfo* pfi) const
{
    assert(m_picture.Length() >= m_fields.size());
    unsigned width = unsigned(m_picture.Length() - m_fields.size());

    StrW tmp;
    for (size_t ii = m_fields.size(); ii--;)
    {
        if (m_fields[ii].m_auto_width)
        {
            assert(m_fields[ii].m_field == FLD_FILENAME);
            const WCHAR* name = pfi->GetLongName().Text();

            if (m_settings.IsSet(FMT_LOWERCASE) && tmp.Empty())
            {
                tmp.Set(name);
                tmp.ToLower();
                name = tmp.Text();
            }

            if (m_settings.IsSet(FMT_DIRBRACKETS) && (pfi->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY))
                width += 2;
            else if (m_settings.IsSet(FMT_CLASSIFY) && ((pfi->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY) || (pfi->IsSymLink())))
                width += 1;

            width += s_icon_width + __wcswidth(name);
        }
        else
        {
            width += m_fields[ii].m_cchWidth;
        }
    }

    return width;
}

bool PictureFormatter::CanAutoFitWidth() const
{
    if (s_can_autofit &&
        !m_settings.IsSet(FMT_ALTDATASTEAMS|FMT_BARE|FMT_FAT|FMT_FULLNAME))
    {
        for (size_t ii = m_fields.size(); ii--;)
        {
            if (m_fields[ii].m_auto_width)
                return true;
        }
    }
    return false;
}

void PictureFormatter::SetDirContext(const WCHAR* dir, const std::shared_ptr<const RepoStatus>& repo)
{
    m_dir = dir;
    m_repo = repo;
    m_max_branch_width = 0;
    m_max_relative_width = 0;
}

inline void PictureFormatter::OnFile(const FileInfo* pfi)
{
    if (m_need_branch_width && m_max_branch_width < 10)
    {
        StrW full;
        PathJoin(full, m_dir, pfi->GetLongName());

        const auto repo = s_repo_map.Find(full.Text());
        if (repo && repo->repo)
        {
            const unsigned width = __wcswidth(repo->branch.Text());
            if (m_max_branch_width < width)
            {
                m_max_branch_width = width;
                if (m_max_branch_width >= 10)
                    m_max_branch_width = 10;
            }
            m_any_repo_roots = true;
        }
    }

    // TODO: m_need_relative_width
    m_max_relative_width = 6;
}

void PictureFormatter::Format(StrW& s, const FileInfo* pfi, const WIN32_FIND_STREAM_DATA* pfsd, bool one_per_line) const
{
    assert(!s.Length() || s.Text()[s.Length() - 1] == '\n');

    const unsigned max_file_width = m_max_file_width;
    const unsigned max_dir_width = m_max_dir_width + (m_settings.IsSet(FMT_DIRBRACKETS) ? 2 : 0);

    const WCHAR* dir = m_dir;
    const WCHAR* color = SelectColor(pfi, m_settings.m_flags, dir);

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
            case FLD_GITFILE:
            case FLD_GITREPO:
                // REVIEW: Color could be considered as mattering here because of background colors and columns.
                s.AppendSpaces(m_fields[ii].m_cchWidth);
                break;
            case FLD_FILESIZE:
                {
                    // FUTURE: color scale for stream sizes.
                    const WCHAR* size_color = m_settings.IsSet(FMT_COLORS) ? GetSizeColor(pfsd->StreamSize.QuadPart) : nullptr;
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
                FormatCompressed(s, pfi, m_settings.m_flags, color, m_fields[ii].m_chSubField);
                break;
            case FLD_ATTRIBUTES:
                FormatAttributes(s, pfi->GetAttributes(), m_fields[ii].m_masks, m_fields[ii].m_chStyle, m_settings.IsSet(FMT_COLORS));
                break;
            case FLD_OWNER:
                FormatOwner(s, pfi, m_settings.m_flags, color);
                break;
            case FLD_SHORTNAME:
                FormatFilename(s, pfi, m_settings.m_flags|FMT_SHORTNAMES|FMT_FAT|FMT_ONLYSHORTNAMES, 0, dir, color);
                break;
            case FLD_FILENAME:
                {
                    FormatFlags flags = m_settings.m_flags;
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

                    const bool show_reparse = (fLast && dir && one_per_line && pfi->IsReparseTag());
                    const WCHAR* field_color = color;
                    if (show_reparse && UseLinkTargetColor())
                        field_color = SelectColor(pfi, m_settings.m_flags, dir, true);

                    FormatFilename(s, pfi, flags, width, dir, field_color, show_reparse);
                    if (fLast)
                    {
                        s.TrimRight();
                        if (show_reparse)
                            FormatReparsePoint(s, pfi, flags, dir);
                    }
                }
                break;
            case FLD_GITFILE:
                FormatGitFile(s, pfi, dir, m_settings.m_flags, m_repo.get());
                break;
            case FLD_GITREPO:
                FormatGitRepo(s, pfi, dir, m_settings.m_flags, m_fields[ii].m_cchWidth);
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

void DirEntryFormatter::Initialize(unsigned num_columns, const FormatFlags flags, WhichTimeStamp whichtimestamp, WhichFileSize whichfilesize, DWORD dwAttrIncludeAny, DWORD dwAttrMatch, DWORD dwAttrExcludeAny, const WCHAR* picture)
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
            if (m_settings.IsSet(FMT_SIZE|FMT_MINISIZE|FMT_FAT))
                picture = L"F12 Sm";
            else
                picture = L"F17";
            break;
        case 2:
            if (m_settings.IsSet(FMT_FAT))
                picture = L"F Ss D G?";
            else
            {
                unsigned width = 38;
                StrW tmp;
                const bool size = m_settings.IsSet(FMT_SIZE|FMT_MINISIZE);
                const bool date = m_settings.IsSet(FMT_DATE|FMT_MINIDATE);
                if (size)
                {
                    if (date || m_settings.IsSet(FMT_MINISIZE))
                    {
                        tmp.Append(L" Sm");
                        width -= 1 + 4;
                    }
                    else
                    {
                        tmp.Append(L" Ss");
                        width -= 1 + 9;
                    }
                }
                if (date)
                {
                    if (size || m_settings.IsSet(FMT_MINIDATE))
                    {
                        if (tmp.Length())
                        {
                            tmp.Append(L" ");
                            width -= 1;
                        }
                        tmp.Append(L" Dm");
                        width -= 1 + 11;
                    }
                    else
                    {
                        if (tmp.Length())
                        {
                            tmp.Append(L" ");
                            width -= 1;
                        }
                        tmp.Append(L" Dn");
                        width -= 1 + 17;
                    }
                }
                sPic.Printf(L"F%u%s G?", width, tmp.Text());
                picture = sPic.Text();
            }
            break;
        case 1:
            if (m_settings.IsSet(FMT_FAT))
                picture = L"F Ss D  C?  T?  G?  R?";
            else
            {
                sPic.Clear();
                if (!m_settings.IsSet(FMT_LONGNODATE))
                    sPic.Append(L"D  ");
                if (!m_settings.IsSet(FMT_LONGNOSIZE))
                    sPic.Append(L"S  ");
                sPic.Append(L"C?  ");
                if (!m_settings.IsSet(FMT_LONGNOATTRIBUTES))
                    sPic.Append(L"T?  ");
                sPic.Append(L"X?  O?  G?  R?  F");
                picture = sPic.Text();
            }
            break;
        case 0:
            sPic.Set(L"F");
            if (m_settings.IsSet(FMT_SIZE|FMT_MINISIZE|FMT_FAT))
                sPic.Append(m_settings.IsSet(FMT_MINISIZE) ? L" Sm" : L" S");
            if (m_settings.IsSet(FMT_DATE|FMT_MINIDATE))
                sPic.Append(m_settings.IsSet(FMT_MINIDATE) ? L" Dm" : L" D");
            sPic.Append(L" G?");
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

    m_fImmediate = (!s_gradient &&
                    !*g_sort_order &&
                    Settings().m_num_columns == 1);
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

    if (Settings().IsSet(FMT_NOVOLUMEINFO))
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

    m_line_break_before_miniheader = true;
    return true;
}

void DirEntryFormatter::OnPatterns(bool grouped)
{
    m_grouped_patterns = grouped;
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

void DirEntryFormatter::OnDirectoryBegin(const WCHAR* const dir, const std::shared_ptr<const RepoStatus>& repo)
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

    m_picture.SetDirContext(dir, repo);

    if (!Settings().IsSet(FMT_BARE))
    {
        StrW s;
        if (Settings().IsSet(FMT_MINIHEADER))
        {
            if (m_line_break_before_miniheader)
                s.Append(L"\n");
            s.Printf(L"%s%s:\n", dir, wcschr(dir, '\\') ? L"" : L"\\");
        }
        else if (!Settings().IsSet(FMT_NOHEADER))
        {
            s.Printf(L"\n Directory of %s%s\n\n", dir, wcschr(dir, '\\') ? L"" : L"\\");
        }
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

        const FormatFlags flags = m_settings.m_flags | (m_settings.IsSet(FMT_SUBDIRECTORIES) ? FMT_FULLNAME : FMT_NONE);
        FormatFilename(s, pfi, flags, 0, m_dir.Text(), SelectColor(pfi, flags, m_dir.Text()));
    }
    else
    {
        m_picture.Format(s, pfi);
    }

    if (m_settings.IsSet(FMT_ALTDATASTEAMS|FMT_ONLYALTDATASTREAMS))
    {
        assert(implies(m_settings.IsSet(FMT_ALTDATASTEAMS), !m_settings.IsSet(FMT_FAT|FMT_BARE)));

        StrW tmp;
        PathJoin(tmp, m_dir.Text(), pfi->GetLongName());

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
                m_picture.Format(s, pfi, &fsd);
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
    const bool fImmediate = (m_fImmediate &&
                             m_picture.IsImmediate() &&
                             !m_grouped_patterns);

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

    if (Settings().IsSet(FMT_GIT|FMT_GITREPOS))
    {
        StrW full;
        PathJoin(full, dir, pfd->cFileName);
        auto repo = GitStatus(full.Text(), Settings().IsSet(FMT_SUBDIRECTORIES));
        s_repo_map.Add(repo);
    }

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
        const FormatFlags flags = Settings().m_flags;
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
            if (s_gradient && s_scale_size)
            {
                for (WhichFileSize which = FILESIZE_ARRAY_SIZE; which = WhichFileSize(int(which) - 1);)
                {
                    if (which != FILESIZE_COMPRESSED || Settings().m_need_compressed_size)
                        Settings().UpdateMinMaxSize(which, pfi->GetFileSize(which));
                }
            }
        }

        if (s_scale_time)
        {
            for (WhichTimeStamp which = TIMESTAMP_ARRAY_SIZE; which = WhichTimeStamp(int(which) - 1);)
                Settings().UpdateMinMaxTime(which, pfi->GetFileTime(which));
        }

        m_picture.OnFile(pfi);
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
                  IsNewRootGroup(m_subdirs[0].dir.Text()));

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

        bool clear_sort_order = false;
        if (m_grouped_patterns && !*g_sort_order)
        {
            clear_sort_order = true;
            wcscpy_s(g_sort_order, L"n");
        }

        if (*g_sort_order)
            std::sort(m_files.begin(), m_files.end(), CmpFileInfo);

        if (m_grouped_patterns && m_files.size())
        {
            // Remove duplicates.  By moving unique items into a new array.
            // This is (much) more efficient than modifying the array in situ,
            // because this doesn't insert or delete and thus doesn't shift
            // items.  It's easy to overlook that insert and delete are O(N)
            // operations.  But when inserting or deleting, the loop becomes
            // exponential instead of linear.

            // Move the first item, which by definition is unique.
            std::vector<FileInfo> files;
            files.emplace_back(std::move(m_files[0]));

            // Loop and move other unique items.
            for (size_t i = 1; i < m_files.size(); ++i)
            {
                auto& hare = m_files[i];
                if (!hare.GetLongName().Equal(files.back().GetLongName()))
                    files.emplace_back(std::move(hare));
            }

            // Swap the uniquely filtered array into place.
            m_files.swap(files);
        }

        if (clear_sort_order)
            *g_sort_order = '\0';

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
                const bool vertical = Settings().IsSet(FMT_SORTVERTICAL);
                const unsigned spacing = (num_columns != 0 || isFAT || m_picture.HasDate() ||
                                          (num_columns == 0 && m_picture.HasGit())) ? 3 : 2;

                ColumnWidths col_widths;
                std::vector<PictureFormatter> col_pictures;
                const bool autofit = m_picture.CanAutoFitWidth();
                if (!autofit)
                {
                    const unsigned max_per_file_width = m_picture.GetMaxWidth(console_width - 1, true);
                    assert(implies(console_width >= 80, num_columns * (max_per_file_width + 3) < console_width + 3));
                    for (unsigned num = std::max<unsigned>((console_width + spacing - 1) / (max_per_file_width + spacing), unsigned(1)); num--;)
                    {
                        col_widths.emplace_back(max_per_file_width);
                        col_pictures.emplace_back(m_picture);
                    }
                }
                else
                {
                    col_widths = CalculateColumns([this](size_t i){
                        return m_picture.GetMinWidth(&m_files[i]);
                    }, m_files.size(), vertical, spacing, console_width - 1);

                    for (unsigned i = 0; i < col_widths.size(); ++i)
                    {
                        col_pictures.emplace_back(m_picture);
                        col_pictures.back().GetMaxWidth(col_widths[i], true);
                    }
                }

                const unsigned num_per_row = std::max<unsigned>(1, unsigned(col_widths.size()));
                const unsigned num_rows = unsigned(m_files.size() + num_per_row - 1) / num_per_row;
                const unsigned num_add = vertical ? num_rows : 1;

                for (unsigned ii = 0; ii < num_rows; ii++)
                {
                    auto picture = col_pictures.begin();

                    unsigned iItem = vertical ? ii : ii * num_per_row;
                    for (unsigned jj = 0; jj < num_per_row && iItem < m_files.size(); jj++, iItem += num_add)
                    {
                        const FileInfo* pfi = &m_files[iItem];

                        if (jj)
                        {
                            const unsigned width = cell_count(s.Text());
                            const unsigned spaces = col_widths[jj - 1] - width + spacing;
                            s.Clear();
                            s.AppendSpaces(spaces);
                            OutputConsole(m_hout, s.Text(), s.Length());
                        }

                        s.Clear();
                        picture->Format(s, pfi, nullptr, false/*one_per_line*/);
                        OutputConsole(m_hout, s.Text(), s.Length());

                        ++picture;
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
        else if (!Settings().IsSet(FMT_BARE|FMT_NOSUMMARY))
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

    m_line_break_before_miniheader = true;
}

void DirEntryFormatter::OnVolumeEnd(const WCHAR* dir)
{
    if (Settings().IsSet(FMT_NOSUMMARY))
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

void DirEntryFormatter::AddSubDir(const StrW& dir, unsigned depth, const std::shared_ptr<const GlobPatterns>& git_ignore, const std::shared_ptr<const RepoStatus>& repo)
{
    assert(Settings().IsSet(FMT_SUBDIRECTORIES));
    assert(!IsPseudoDirectory(dir.Text()));

    SubDir subdir;
    subdir.dir.Set(dir);
    subdir.depth = depth;
    subdir.git_ignore = git_ignore;
    m_subdirs.emplace_back(std::move(subdir));

    if (Settings().IsSet(FMT_GITIGNORE))
    {
        StrW file(dir);
        EnsureTrailingSlash(file);
        file.Append(L".gitignore");
        SHFile h = CreateFile(file.Text(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, 0);
        if (!h.Empty())
        {
            std::shared_ptr<GlobPatterns> globs = std::make_shared<GlobPatterns>();
            if (globs && globs->Load(h))
                subdir.git_ignore = globs;
        }
    }

    if (Settings().IsSet(FMT_GIT|FMT_GITREPOS))
    {
        subdir.repo = s_repo_map.Find(dir.Text());
        if (!subdir.repo)
            subdir.repo = repo;
    }
}

void DirEntryFormatter::SortSubDirs()
{
    if (!m_subdirs.empty())
        std::sort(m_subdirs.begin(), m_subdirs.end(), CmpSubDirs);
}

bool DirEntryFormatter::NextSubDir(StrW& dir, unsigned& depth, std::shared_ptr<const GlobPatterns>& git_ignore, std::shared_ptr<const RepoStatus>& repo)
{
    if (m_subdirs.empty())
    {
        dir.Clear();
        depth = 0;
        git_ignore.reset();
        repo.reset();
        return false;
    }

    dir = std::move(m_subdirs[0].dir);
    depth = m_subdirs[0].depth;
    git_ignore = std::move(m_subdirs[0].git_ignore);
    repo = std::move(m_subdirs[0].repo);
    m_subdirs.erase(m_subdirs.begin());

    // The only thing that needs the map is FormatGitRepo(), to avoid running
    // "git status" twice for the same repo.  So, once traversal dives into a
    // directory, nothing will try to print that directory entry again, so the
    // map doesn't need to hold a reference anymore.  Removing it from the map
    // ensures the data structures can be freed once traversal through that
    // repo tree finishes.
    s_repo_map.Remove(dir.Text());

    return true;
}

