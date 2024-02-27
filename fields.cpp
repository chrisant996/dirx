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
static bool s_forced_icons_always = false;
static BYTE s_icon_padding = 1;
static unsigned s_icon_width = 0;
static bool s_mini_bytes = false;
static ColorScaleFields s_scale_fields = SCALE_NONE;
static bool s_gradient = true;
static WCHAR s_size_style = 0;
static WCHAR s_time_style = 0;
constexpr unsigned c_max_branch_name = 10;

static const WCHAR c_hyperlink[] = L"\x1b]8;;";
static const WCHAR c_BEL[] = L"\a";

/*
 * Configuration functions.
 */

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

    const WCHAR* orig = s;

    if (s[0] == '$')
        ++s;
    else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;

    WCHAR ch = 0;
    WORD w;

    bool any_digits = false;
    if (*s && ParseHexDigit(*s, &w))
    {
        any_digits = true;
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

    if (*s == 'h' || *s == 'H')
        ++s;
    if (*s)
    {
        Error e;
        e.Set(L"Invalid hexadecimal character code '%1'.") << orig;
        e.Report();
        return;
    }

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

bool SetUseIcons(const WCHAR* s, bool unless_always)
{
    if (!s)
        return false;
    if (unless_always && s_forced_icons_always)
        return false;
    if (!_wcsicmp(s, L"") || !_wcsicmp(s, L"auto"))
    {
        DWORD dwMode;
        s_use_icons = !!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &dwMode);
        s_forced_icons_always = false;
    }
    else if (!_wcsicmp(s, L"always"))
    {
        s_use_icons = true;
        s_forced_icons_always = true;
    }
    else if (!_wcsicmp(s, L"never") || !_wcsicmp(s, L"-"))
    {
        s_use_icons = false;
        s_forced_icons_always = false;
    }
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

unsigned GetIconWidth()
{
    return s_icon_width;
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
        s_scale_fields = ~SCALE_NONE;
    else if (!_wcsicmp(s, L"none"))
        s_scale_fields = SCALE_NONE;
    else if (!_wcsicmp(s, L"size"))
        s_scale_fields = SCALE_SIZE;
    else if (!_wcsicmp(s, L"time") || !_wcsicmp(s, L"date") || !_wcsicmp(s, L"age"))
        s_scale_fields = SCALE_TIME;
    else
        return false;
    return true;
}

ColorScaleFields GetColorScaleFields()
{
    return s_scale_fields;
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

bool IsGradientColorScaleMode()
{
    return s_gradient;
}

bool SetDefaultSizeStyle(const WCHAR* size_style)
{
    if (!size_style)
        return false;

    static const WCHAR* c_size_styles[] =
    {
        L"mmini",
        L"sshort",
        L"nnormal",
    };

    for (auto s : c_size_styles)
        if ((size_style[0] == s[0] && !size_style[1]) || wcsicmp(s + 1, size_style) == 0)
        {
            s_size_style = s[0];
            return true;
        }

    return false;
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

void ClearDefaultTimeStyleIf(const WCHAR* time_style)
{
    const auto old_time_style = s_time_style;
    if (SetDefaultTimeStyle(time_style))
    {
        if (s_time_style == old_time_style)
            s_time_style = 0;
        else
            s_time_style = old_time_style;
    }
}

/*
 * Formatter functions.
 */

static void SelectFileTime(const FileInfo* const pfi, const WhichTimeStamp timestamp, SYSTEMTIME* const psystime)
{
    FILETIME ft;

    FileTimeToLocalFileTime(&pfi->GetFileTime(timestamp), &ft);
    FileTimeToSystemTime(&ft, psystime);
}

const WCHAR* SelectColor(const FileInfo* const pfi, const FormatFlags flags, const WCHAR* dir, bool ignore_target_color)
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
                s.AppendColorElseNormal(color);
                prev_color = color;
            }
        }
        s.Append(bit ? pAttr->ch : chNotSet);
    }

    s.AppendNormalIf(prev_color);
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

void FormatFilename(StrW& s, const FileInfo* pfi, FormatFlags flags, unsigned max_width, const WCHAR* dir, const WCHAR* color, bool show_reparse)
{
    const StrW& name = pfi->GetFileName(flags);
    WCHAR classify = 0;

    if (*name.Text() == '.')
        flags &= ~(FMT_JUSTIFY_FAT|FMT_JUSTIFY_NONFAT);

    if (s_use_icons)
    {
        DWORD attr = pfi->GetAttributes();
        const WCHAR* icon = LookupIcon(name.Text(), attr);
        const WCHAR* icon_color = GetIconColor(color);
        s.AppendColor(icon_color);
        s.Append(icon ? icon : L" ");
        s.AppendNormalIf(icon_color);
        s.AppendSpaces(s_icon_padding);
        if (max_width)
            max_width -= s_icon_width;
    }

    s.AppendColor(color);

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

    StrW tmp;
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
            if (max_width && p != tmp.Text())
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
                if (*dir)
                {
                    s.Append(dir);
                    if (*dir && s.Text()[s.Length() - 1] != ':')
                        EnsureTrailingSlash(s);
                }
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

    const WCHAR* nolines = StripLineStyles(color);
    if (classify || hyperlinks || nolines != color)
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
        if (nolines != color)
            s.AppendColor(nolines);
        s.AppendSpaces(spaces);
    }

    s.AppendNormalIf(color);
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
        s.AppendColor(punct);
        s.Append(L"[..]");
        s.AppendNormalIf(punct);
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
            s.AppendColor(punct);
            s.Append(L"[");

            const WCHAR* name = FindName(tmp.Text());
            const DWORD attr = pfi->GetAttributes();
            const int mode = (attr & FILE_ATTRIBUTE_DIRECTORY) ? S_IFDIR : S_IFREG;
            const WCHAR* color = colors ? LookupColor(name, attr, mode) : nullptr;
            const WCHAR* path_color = colors ? GetColorByKey(L"lp") : nullptr;
            if (!path_color && !color)
            {
                s.AppendNormalIf(punct);
                s.Append(tmp);
            }
            else
            {
                if (!path_color)
                    path_color = color;
                s.AppendColorElseNormalIf(path_color, punct);

                s.Append(tmp.Text(), name - tmp.Text());

                s.AppendColorElseNormalIf(color, path_color);
                s.Append(name);
            }

            s.AppendColor(punct);
            s.Append(L"]");
            s.AppendNormalIf(punct);
        }
        else
        {
            s.AppendColor(punct);
            s.Append(L"[...]");
            s.AppendNormalIf(punct);
        }
    }
}

void FormatSizeForReading(StrW& s, unsigned __int64 cbSize, unsigned field_width, const DirFormatSettings& settings)
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
        if (s_size_style)
            chStyle = s_size_style;
        else if (settings.IsSet(FMT_MINISIZE))
            chStyle = 'm';
        else if (!settings.IsSet(FMT_FULLSIZE))
            chStyle = 's';
    }

    return chStyle;
}

unsigned GetSizeFieldWidthByStyle(const DirFormatSettings& settings, WCHAR chStyle)
{
    switch (GetEffectiveSizeFieldStyle(settings, chStyle))
    {
    case 'm':       return 4;           // "9.9M"
    case 's':       return 9;           // "123456789M"
    default:        return 16;          // ",123,456,789,123"
    }
}

void FormatSize(StrW& s, unsigned __int64 cbSize, const WhichFileSize* which, const DirFormatSettings& settings, WCHAR chStyle, unsigned max_width, const WCHAR* color, const WCHAR* fallback_color, bool nocolor)
{
    // FUTURE: CMD shows size for FILE_ATTRIBUTE_OFFLINE files in parentheses
    // to indicate it could take a while to retrieve them.

    chStyle = GetEffectiveSizeFieldStyle(settings, chStyle);

#ifdef DEBUG
    const unsigned orig_len = s.Length();
#endif

    nocolor = (nocolor || !settings.IsSet(FMT_COLORS));
    if (nocolor)
        color = nullptr;
    else
    {
        if (!color && which)
            color = GetSizeColor(cbSize);
        if (!color)
            color = fallback_color;
        if (s_gradient && (s_scale_fields & SCALE_SIZE) && which)
        {
            const WCHAR* gradient = ApplyGradient(color ? color : L"", cbSize, settings.m_min_size[*which], settings.m_max_size[*which]);
            if (gradient)
                color = gradient;
        }
    }

    const WCHAR* unit_color = nullptr;
    s.AppendColorNoLineStyles(color);

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

            unit_color = (!nocolor && !(s_gradient && (s_scale_fields & SCALE_SIZE)) && which) ? GetSizeUnitColor(cbSize) : nullptr;

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
                s.Printf(L"%I64u.%I64u", cbSize / 10, cbSize % 10);
            }
            else
            {
                dSize += 0.5;
                cbSize = static_cast<unsigned __int64>(dSize);
                if (!iChSize)
                {
                    if (s_mini_bytes && cbSize <= 999)
                    {
                        s.Printf(L"%*I64u", max_width ? max_width : 4, cbSize);
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
                assert(implies(max_width, max_width > 1));
                s.Printf(L"%*I64u", max_width ? max_width - 1 : 3, cbSize);
            }

            s.AppendColor(unit_color);
            s.Append(&c_size_chars[iChSize], 1);
        }
        break;

    case 's':
        {
            // If size fits in 8 digits, report it as is.

            if (cbSize < 100000000)
            {
                assert(implies(max_width, max_width > 1));
                s.Printf(L"%*I64u ", max_width ? max_width - 1 : 8, cbSize);
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

            assert(implies(max_width, max_width > 3));
            s.Printf(L"%*I64u.%1I64u%c", max_width ? max_width - 3 : 6, cbSize / 10, cbSize % 10, chSize);
        }
        break;

    default:
        FormatSizeForReading(s, cbSize, max_width ? max_width : GetSizeFieldWidthByStyle(settings, chStyle), settings);
        break;
    }

    s.AppendNormalIf(color || unit_color);
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

static void FormatFileSize(StrW& s, const FileInfo* pfi, const DirFormatSettings& settings, unsigned max_width, WCHAR chStyle=0, WCHAR chField=0, const WCHAR* fallback_color=nullptr, bool nocolor=false)
{
    chStyle = GetEffectiveSizeFieldStyle(settings, chStyle);
    const WCHAR* const tag = GetSizeTag(pfi, chStyle);

#ifdef DEBUG
    const unsigned orig_len = s.Length();
#endif

    if (tag)
    {
        if ((settings.IsSet(FMT_NODIRTAGINSIZE)) ||
            (settings.IsSet(FMT_COLORS) && (s_scale_fields & SCALE_SIZE)))
        {
            const unsigned trailing = (chStyle == 's');
            s.AppendSpaces(max_width - 1 - trailing);
            const WCHAR* color = (!nocolor && settings.IsSet(FMT_COLORS)) ? GetColorByKey(L"xx") : nullptr;
            s.AppendColor(color);
            s.Append(L"-");
            s.AppendNormalIf(color);
            s.AppendSpaces(trailing);
        }
        else
        {
            if (nocolor)
                fallback_color = nullptr;
            s.AppendColorNoLineStyles(fallback_color);
            s.Append(tag);
            s.AppendSpaces(max_width - unsigned(wcslen(tag)));
            s.AppendNormalIf(fallback_color);
        }
    }
    else
    {
        const WhichFileSize which = WhichFileSizeByField(settings, chField);
        FormatSize(s, pfi->GetFileSize(which), &which, settings, chStyle, max_width, nullptr, fallback_color, nocolor);
    }
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

struct UnitDefinition
{
    const WCHAR* mini_unit;
    const WCHAR* normal_unit;
    const WCHAR* normal_units;
};

static const UnitDefinition c_defs[] =
{
    { L"s ", L"second",  L"seconds" },
    { L"m ", L"minute",  L"minutes" },
    { L"hr", L"hour",    L"hours" },
    { L"dy", L"day",     L"days" },
    { L"wk", L"week",    L"weeks" },
    { L"mo", L"month",   L"months" },
    { L"yr", L"year",    L"years" },
    { L"C ", L"century", L"centuries" },
};

inline void PrintfRelative(StrW&s, unsigned unit_index, bool mini, ULONGLONG value)
{
    const unsigned n = unsigned(value);
    const UnitDefinition& def = c_defs[unit_index];
    const WCHAR* unit = mini ? def.mini_unit : (n == 1) ? def.normal_unit : def.normal_units;
    s.Printf(mini ? L"%3u %s" : L"%u %s", n, unit);
}

static void FormatRelativeTime(StrW& s, const FILETIME& ft, bool mini=false)
{
    const ULONGLONG now = FileTimeToULONGLONG(NowAsFileTime()) / 10000000;  // Seconds.
    const ULONGLONG then = FileTimeToULONGLONG(ft) / 10000000;              // Seconds.
    LONGLONG delta = now - then;

    if (delta < 1)
    {
        s.Append(mini ? L"   now" : L"now");
        return;
    }

    if (delta < 60)
    {
        PrintfRelative(s, 0, mini, delta);
        return;
    }

    delta /= 60;    // Minutes.

    if (delta < 60)
        PrintfRelative(s, 1, mini, delta);
    else if (delta < 24*60)
        PrintfRelative(s, 2, mini, delta / 60);
    else if (delta < 7*24*60)
        PrintfRelative(s, 3, mini, delta / (24*60));
    else if (delta < 31*24*60)
        PrintfRelative(s, 4, mini, delta / (7*24*60));
    else if (delta < 365*24*60)
        PrintfRelative(s, 5, mini, delta / ((365*24*60)/12));
    else if (delta < 100*365*24*60 + 24*24*60)
        PrintfRelative(s, 6, mini, delta / (365*24*60));
    else
        PrintfRelative(s, 7, mini, delta / (100*365*24*60 + 24*24*60));
}

static void FormatTime(StrW& s, const FileInfo* pfi, const DirFormatSettings& settings, const FieldInfo& field, const WCHAR* fallback_color=nullptr)
{
    SYSTEMTIME systime;
    const WhichTimeStamp which = WhichTimeStampByField(settings, field.m_chSubField);

    SelectFileTime(pfi, which, &systime);

    const WCHAR chStyle = GetEffectiveTimeFieldStyle(settings, field.m_chStyle);

#ifdef DEBUG
    const unsigned orig_len = s.Length();
#endif

    const WCHAR* color = nullptr;
    if (settings.IsSet(FMT_COLORS))
    {
        color = GetColorByKey(L"da");
        if (!color)
            color = fallback_color;
        if (s_gradient && (s_scale_fields & SCALE_TIME))
        {
            const WCHAR* gradient = ApplyGradient(color ? color : L"", FileTimeToULONGLONG(pfi->GetFileTime(which)), settings.m_min_time[which], settings.m_max_time[which]);
            if (gradient)
                color = gradient;
        }
        s.AppendColorNoLineStyles(color);
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
        {
            const unsigned len = s.Length();
            FormatRelativeTime(s, pfi->GetFileTime(which));
            s.AppendSpaces(field.m_cchWidth - (s.Length() - len));
        }
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
                s.Printf(L"%2u/%02u %02u:%02u",
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

    s.AppendNormalIf(color);
}

void FormatCompressed(StrW& s, const unsigned __int64 cbCompressed, const unsigned __int64 cbFile, const DWORD dwAttr)
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
    s.AppendColorNoLineStyles(color);

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

    s.AppendNormalIf(color);
}

static void FormatOwner(StrW& s, const FileInfo* pfi, const FormatFlags flags, unsigned max_width, const WCHAR* fallback_color)
{
    const WCHAR* owner = pfi->GetOwner().Text();
    unsigned width = __wcswidth(owner);

    const WCHAR* color = (flags & FMT_COLORS) ? GetColorByKey(L"oF") : fallback_color;
    s.AppendColorNoLineStyles(color);
    s.Append(owner);
    s.AppendSpaces(max_width - width);
    s.AppendNormalIf(color);
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

    s.AppendColor(color1);
    s.Append(&c_symbols[unsigned(staged)].symbol, 1);
    if (color1 != color2)
        s.AppendColor(color2);
    s.Append(&c_symbols[unsigned(working)].symbol, 1);
    assert(!!color1 == !!color2);
    s.AppendNormalIf(color1);
}

static void FormatGitRepo(StrW& s, const FileInfo* pfi, const WCHAR* dir, const FormatFlags flags, unsigned max_width)
{
    WCHAR status;
    StrW branch;
    unsigned branch_width;

    StrW full;
    PathJoin(full, dir, pfi->GetLongName());

    const WCHAR* color1 = nullptr;
    const WCHAR* color2 = nullptr;
    const auto repo = FindRepo(full.Text());
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

    s.AppendColor(color1);
    s.Append(&status, 1);
    s.AppendNormalIf(color1);

    s.AppendSpaces(1);

    s.AppendColor(color2);
    s.Append(branch);
    if (color2)
    {
        const WCHAR* pad_color = StripLineStyles(color2);
        if (pad_color != color2)
            s.AppendColor(pad_color);
    }
    s.AppendSpaces(max_width - 2 - branch_width);
    s.AppendNormalIf(color2);
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
    ZeroMemory(&m_max_relative_width_which, sizeof(m_max_relative_width_which));
    ZeroMemory(&m_need_relative_width_which, sizeof(m_need_relative_width_which));
}

void PictureFormatter::SetFitColumnsToContents(bool fit)
{
    assert(!m_finished_initial_parse);
    m_fit_columns_to_contents = fit;
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

    std::vector<FieldInfo> fields;
    m_fields.swap(fields);

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
                p->m_auto_filename_width = !p->m_cchWidth;
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
                p->m_chStyle = chStyle ? chStyle : s_size_style;
                p->m_cchWidth = (m_fit_columns_to_contents && !m_settings.IsSet(FMT_ALTDATASTEAMS)) ? 0 : GetSizeFieldWidthByStyle(m_settings, chStyle);
                if (!p->m_cchWidth)
                {
                    const WhichFileSize which = WhichFileSizeByField(m_settings, chSubField);
                    if (m_finished_initial_parse)
                    {
                        const FieldInfo& old_field = fields[m_fields.size() - 1];
                        assert(implies(m_finished_initial_parse, old_field.m_cchWidth));
                        p->m_cchWidth = old_field.m_cchWidth;
                    }
                    m_need_filesize_width = true;
                }
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
                p->m_chStyle = chStyle ? chStyle : s_time_style;
                p->m_cchWidth = GetTimeFieldWidthByStyle(m_settings, chStyle);
                if (!p->m_cchWidth)
                {
                    const WhichTimeStamp which = WhichTimeStampByField(m_settings, chSubField);
                    if (m_finished_initial_parse)
                    {
                        assert(GetEffectiveTimeFieldStyle(m_settings, chStyle) == 'r');
                        p->m_cchWidth = m_max_relative_width_which[which];
                    }
                    else
                    {
                        m_need_relative_width = true;
                        m_need_relative_width_which[which] = true;
                    }
                }
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
                p->m_cchWidth = m_fit_columns_to_contents ? 0 : 22;
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
            skip = (//picture[1] == '?' &&
                    (!m_settings.IsSet(FMT_GITREPOS) || (m_finished_initial_parse && !m_any_repo_roots)));
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
            skip = (//picture[1] == '?' &&
                    (!m_settings.IsSet(FMT_GIT) || (m_finished_initial_parse && (!m_dir->repo || !m_dir->repo->repo))));
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
        case FLD_FILESIZE:
            if (m_fit_columns_to_contents)
            {
                m_need_filesize_width = true;
                m_immediate = false;
            }
            break;
        case FLD_OWNER:
            if (m_fit_columns_to_contents)
            {
                m_need_owner_width = true;
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
    m_max_filepart_width = max_file_width;
    m_max_dirpart_width = max_dir_width;

    if (m_need_branch_width)
    {
        const unsigned width = 2 + clamp<unsigned>(m_max_branch_width, 1, c_max_branch_name);
        for (auto& field : m_fields)
            if (field.m_field == FLD_GITREPO)
                field.m_cchWidth = width;
    }

    if (m_need_relative_width)
    {
        for (auto& field : m_fields)
        {
            if (field.m_field == FLD_DATETIME && field.m_chStyle == 'r')
                field.m_cchWidth = m_max_relative_width_which[WhichTimeStampByField(m_settings, field.m_chSubField)];
        }
    }

#ifdef DEBUG
    if (m_need_filesize_width)
    {
        for (auto& field : m_fields)
        {
            if (field.m_field == FLD_FILESIZE)
                assert(field.m_cchWidth);
        }
    }
#endif

    if (m_need_owner_width)
    {
        for (auto& field : m_fields)
        {
            if (field.m_field == FLD_OWNER)
                field.m_cchWidth = m_max_owner_width;
        }
    }

    if (m_settings.IsSet(FMT_GIT|FMT_GITREPOS))
        ParsePicture(m_orig_picture.Text());
}

unsigned PictureFormatter::GetMaxWidth(unsigned fit_in_width, bool recalc_auto_width_fields)
{
    unsigned distribute = 0;

    assert(m_picture.Length() >= m_fields.size());

    unsigned width = unsigned(m_picture.Length() - m_fields.size());
    for (auto& field : m_fields)
    {
        if (recalc_auto_width_fields && field.m_auto_filename_width)
            field.m_cchWidth = 0;
        if (field.m_auto_filename_width)
        {
            assert(field.m_field == FLD_FILENAME);
            const unsigned filename_width = GetFilenameFieldWidth(m_settings, &field, 0, 0);
            width += filename_width;
            if (!filename_width)
            {
                // In a fixed-width column, whatever space remains is
                // distributed among however many filename fields are
                // present in the picture.
                distribute++;
            }
        }
        else
        {
            assert(field.m_cchWidth);
            width += field.m_cchWidth;
        }
    }

    if (distribute)
    {
        const unsigned max_file_width = s_icon_width + m_max_filepart_width;
        const unsigned max_dir_width = s_icon_width + m_max_dirpart_width + (m_settings.IsSet(FMT_DIRBRACKETS) ? 2 : 0);
        const unsigned max_entry_width = std::max<unsigned>(std::max<unsigned>(max_file_width, max_dir_width), unsigned(s_icon_width + 1));
        unsigned distribute_width = max_entry_width;
        if (fit_in_width)
        {
            distribute_width = distribute;
            if (width + distribute_width < fit_in_width)
                distribute_width = std::max<unsigned>((fit_in_width - width) / distribute, unsigned(1));
            distribute_width = std::min<unsigned>(distribute_width, max_entry_width);
        }
        for (auto& field : m_fields)
        {
            if (field.m_field == FLD_FILENAME)
            {
                field.m_cchWidth = distribute_width;
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
        if (m_fields[ii].m_auto_filename_width)
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

bool PictureFormatter::CanAutoFitFilename() const
{
    if (s_can_autofit &&
        !m_settings.IsSet(FMT_ALTDATASTEAMS|FMT_BARE|FMT_FAT|FMT_FULLNAME))
    {
        for (size_t ii = m_fields.size(); ii--;)
        {
            if (m_fields[ii].m_auto_filename_width)
                return true;
        }
    }
    return false;
}

void PictureFormatter::SetDirContext(const std::shared_ptr<const DirContext>& dir)
{
    assert(implies(m_dir, m_dir == dir));
    m_dir = dir;
}

void PictureFormatter::OnFile(const FileInfo* pfi)
{
    if (m_need_branch_width && m_max_branch_width < 10)
    {
        StrW full;
        PathJoin(full, m_dir->dir.Text(), pfi->GetLongName());

        const auto repo = FindRepo(full.Text());
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

    if (m_need_filesize_width)
    {
        StrW tmp;
        for (auto& field : m_fields)
        {
            if (field.m_field == FLD_FILESIZE)
            {
                tmp.Clear();
                FormatFileSize(tmp, pfi, m_settings, 0, field.m_chStyle, field.m_chSubField, nullptr, true);
                const WCHAR* p = tmp.Text();
                while (*p == ' ')
                    ++p;
                const unsigned width = __wcswidth(p);
                if (field.m_cchWidth < width)
                    field.m_cchWidth = width;

                for (auto stream = pfi->GetStreams(); stream && *stream; ++stream)
                {
                    tmp.Clear();
                    FormatFileSize(tmp, stream[0].get(), m_settings, 0, field.m_chStyle, field.m_chSubField, nullptr, true);
                    const WCHAR* p = tmp.Text();
                    while (*p == ' ')
                        ++p;
                    const unsigned width = __wcswidth(p);
                    if (field.m_cchWidth < width)
                        field.m_cchWidth = width;
                }
            }
        }
    }

    if (m_need_relative_width)
    {
        StrW tmp;
        for (unsigned ii = 0; ii < TIMESTAMP_ARRAY_SIZE; ++ii)
        {
            if (m_need_relative_width_which[ii])
            {
                FormatRelativeTime(tmp, pfi->GetFileTime(WhichTimeStamp(ii)));
                const unsigned width = __wcswidth(tmp.Text());
                if (m_max_relative_width_which[ii] < width)
                    m_max_relative_width_which[ii] = width;
            }
        }
    }

    if (m_need_owner_width)
    {
        const unsigned width = __wcswidth(pfi->GetOwner().Text());
        if (m_max_owner_width < width)
            m_max_owner_width = width;
    }
}

void PictureFormatter::Format(StrW& s, const FileInfo* pfi, const FileInfo* stream, bool one_per_line) const
{
    const unsigned max_file_width = m_max_filepart_width;
    const unsigned max_dir_width = m_max_dirpart_width + (m_settings.IsSet(FMT_DIRBRACKETS) ? 2 : 0);

    const WCHAR* dir = m_dir->dir.Text();
    const WCHAR* color = SelectColor(pfi, m_settings.m_flags, dir);

    // Format the fields.

    assert(!pfi->IsAltDataStream());
    assert(implies(stream, stream->IsAltDataStream()));

    StrW tmp;
    unsigned ichCopied = 0;
    for (size_t ii = 0; ii < m_fields.size(); ii++)
    {
        // Copy the constant picture text before the field.

        const FieldInfo& field = m_fields[ii];
        const unsigned ichCopyUpTo = field.m_ichInsert;
        s.Append(m_picture.Text() + ichCopied, ichCopyUpTo - ichCopied);
        ichCopied = ichCopyUpTo + 1;

        // Insert field value.

        if (stream)
        {
            switch (field.m_field)
            {
            case FLD_DATETIME:
            case FLD_COMPRESSION:
            case FLD_ATTRIBUTES:
            case FLD_OWNER:
            case FLD_SHORTNAME:
            case FLD_GITFILE:
            case FLD_GITREPO:
                // REVIEW: Color could be considered as mattering here because of background colors and columns.
                s.AppendSpaces(field.m_cchWidth);
                break;
            case FLD_FILESIZE:
                {
                    // FUTURE: color scale for stream sizes.
                    WhichFileSize which = FILESIZE_FILESIZE;
                    const WCHAR* size_color = m_settings.IsSet(FMT_COLORS) ? GetSizeColor(stream->GetFileSize()) : nullptr;
                    FormatSize(s, stream->GetFileSize(), &which, m_settings, field.m_chStyle, field.m_cchWidth, size_color ? size_color : color);
                }
                break;
            case FLD_FILENAME:
                {
                    // WARNING: Keep in sync with PictureFormatter::OnFile and
                    // its stream case for IsFilenameWidthNeeded.
                    tmp.Clear();
                    tmp.AppendSpaces(s_icon_width);
                    if (m_settings.IsSet(FMT_REDIRECTED))
                    {
                        if (m_settings.IsSet(FMT_FULLNAME))
                        {
                            tmp.Append(dir);
                            tmp.Append(L"\\");
                        }
                        tmp.Append(pfi->GetLongName());
                    }
                    else
                        tmp.AppendSpaces(2);
                    tmp.Append(stream->GetLongName());
                    if (m_settings.IsSet(FMT_LOWERCASE))
                        tmp.ToLower();

                    const bool fLast = (ii + 1 == m_fields.size() &&
                                        field.m_ichInsert + 1 == m_picture.Length());
                    unsigned width = GetFilenameFieldWidth(m_settings, &field, max_file_width, max_dir_width);

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
            switch (field.m_field)
            {
            case FLD_DATETIME:
                FormatTime(s, pfi, m_settings, field, color);
                break;
            case FLD_FILESIZE:
                FormatFileSize(s, pfi, m_settings, field.m_cchWidth, field.m_chStyle, field.m_chSubField, color);
                break;
            case FLD_COMPRESSION:
                FormatCompressed(s, pfi, m_settings.m_flags, color, field.m_chSubField);
                break;
            case FLD_ATTRIBUTES:
                FormatAttributes(s, pfi->GetAttributes(), field.m_masks, field.m_chStyle, m_settings.IsSet(FMT_COLORS));
                break;
            case FLD_OWNER:
                FormatOwner(s, pfi, m_settings.m_flags, field.m_cchWidth, color);
                break;
            case FLD_SHORTNAME:
                FormatFilename(s, pfi, m_settings.m_flags|FMT_SHORTNAMES|FMT_FAT|FMT_ONLYSHORTNAMES, 0, dir, color);
                break;
            case FLD_FILENAME:
                {
                    FormatFlags flags = m_settings.m_flags;
                    if (field.m_chSubField == 'x')
                        flags |= FMT_SHORTNAMES|FMT_FAT;
                    else
                        flags &= ~FMT_SHORTNAMES;
                    if (field.m_chStyle == 'f')
                        flags |= FMT_FAT;

                    const bool fLast = (ii + 1 == m_fields.size() &&
                                        field.m_ichInsert + 1 == m_picture.Length());
                    unsigned width = GetFilenameFieldWidth(m_settings, &field, max_file_width, max_dir_width);

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
                FormatGitFile(s, pfi, dir, m_settings.m_flags, m_dir->repo.get());
                break;
            case FLD_GITREPO:
                FormatGitRepo(s, pfi, dir, m_settings.m_flags, field.m_cchWidth);
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

