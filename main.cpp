// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// DIRX lists files, similar to the DIR command.

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"

#include <typeinfo>
#include <shellapi.h>

#include <winioctl.h>                   // For reparse point stuff...
#include <winnt.h>                      //  "

#include "version.h"
#include "argv.h"
#include "options.h"
#include "flags.h"
#include "fileinfo.h"
#include "output.h"
#include "filesys.h"
#include "sorting.h"
#include "patterns.h"
#include "formatter.h"
#include "scan.h"
#include "colors.h"
#include "icons.h"
#include "usage.h"

#include <memory>

static const WCHAR c_opts[] = L"/:+?V,+1+2+4+a.b+c+C+E.f:F+G+h+i+j+J+k+l+n+o.p+q+Q.r+R+s+S.t+T.u+v+w+W:x+X.Y+z+Z+";
static const WCHAR c_DIRXCMD[] = L"DIRXCMD";

static const WCHAR* get_env_prio(const WCHAR* a, const WCHAR* b=nullptr, const WCHAR* c=nullptr)
{
    const WCHAR* env = _wgetenv(a);
    if (!env && b)
        env = _wgetenv(b);
    if (!env && c)
        env = _wgetenv(c);
    return env;
}

int __cdecl _tmain(int argc, const WCHAR** argv)
{
    Error e;
    std::vector<FileInfo> rgFiles;
    StrW s;

    // Remember the app name, and generate the short usage text.

    StrW fmt;
    StrW app;
    StrW usage;
    app.Set(argc ? FindName(argv[0]) : L"DIRX");
    const WCHAR* const ext = FindExtension(app.Text());
    if (ext)
        app.SetEnd(ext);
    app.ToLower();
    fmt.SetA(c_usage);
    usage.Printf(fmt.Text(), app.Text());

    {
        const WCHAR* env;
        SetColorScale(get_env_prio(L"DIRX_COLOR_SCALE", L"EXA_COLOR_SCALE", L"EXA_COLOR_SCALE"));
        SetColorScaleMode(get_env_prio(L"DIRX_COLOR_SCALE_MODE", L"EZA_COLOR_SCALE_MODE", L"EXA_COLOR_SCALE_MODE"));
        if (env = _wgetenv(L"DIRX_NERD_FONTS_VERSION"))
            SetNerdFontsVersion(wcstoul(env, nullptr, 10));
        if (env = get_env_prio(L"DIRX_PAD_ICONS", L"EZA_ICON_SPACING", L"EXA_ICON_SPACING"))
            SetPadIcons(wcstoul(env, nullptr, 10));
    }

    // Skip past app name so we can parse command line options.

    if (argc)
    {
        argc--;
        argv++;
    }

    // Parse options -- this order ensures that command line options will
    // override DIRXCMD options:
    //  1.  Get the DIRXCMD environment variable, if it exists, and parse it
    //      into argc,argv format.
    //  2.  Parse options from DIRXCMD.
    //  3.  Parse the command line options.

    const WCHAR* more_colors = nullptr;
    bool show_all_attributes = false;
    int nix_defaults = 0;           // By default, behave like CMD DIR.
#ifdef DEBUG
    int print_all_icons = 0;
#endif

    enum
    {
        LOI_UNIQUE_IDS              = 0x7FFF,
        LOI_NO_ATTRIBUTES,
        LOI_CLASSIFY,
        LOI_NO_CLASSIFY,
        LOI_NO_COLOR,
        LOI_COLOR_SCALE,
        LOI_NO_COLOR_SCALE,
        LOI_COLOR_SCALE_MODE,
        LOI_COMPACT_COLUMNS,
        LOI_NO_COMPACT_COLUMNS,
        LOI_ESCAPE_CODES,
        LOI_NO_FAT,
        LOI_NO_FULL_PATHS,
        LOI_HIDE_DOT_FILES,
        LOI_NO_HIDE_DOT_FILES,
        LOI_HORIZONTAL,
        LOI_HYPERLINKS,
        LOI_NO_HYPERLINKS,
        LOI_ICONS,
        LOI_NO_ICONS,
        LOI_JUSTIFY,
        LOI_LOWER,
        LOI_NO_LOWER,
        LOI_NIX,
        LOI_NO_NIX,
        LOI_NO_NORMAL,
        LOI_MORE_COLORS,
        LOI_NERD_FONTS_VER,
        LOI_NO_OWNER,
        LOI_PAD_ICONS,
        LOI_NO_RATIO,
        LOI_NO_SHORT_NAMES,
        LOI_NO_SIZE,
        LOI_NO_STREAMS,
        LOI_STRING_SORT,
        LOI_NO_TIME,
        LOI_TRUNCATE_CHAR,
        LOI_WORD_SORT,
    };

    static LongOption<WCHAR> long_opts[] =
    {
        { L"all",                   nullptr,            'a' },
        { L"attributes",            nullptr,            't' },
        { L"no-attributes",         nullptr,            LOI_NO_ATTRIBUTES },
        { L"bare",                  nullptr,            'b' },
        { L"classify",              nullptr,            LOI_CLASSIFY },
        { L"no-classify",           nullptr,            LOI_NO_CLASSIFY },
        { L"color",                 nullptr,            'c' },
        { L"no-color",              nullptr,            LOI_NO_COLOR },
        { L"color-scale",           nullptr,            LOI_COLOR_SCALE,        LOHA_OPTIONAL },
        { L"no-color-scale",        nullptr,            LOI_NO_COLOR_SCALE },
        { L"color-scale-mode",      nullptr,            LOI_COLOR_SCALE_MODE,   LOHA_REQUIRED },
        { L"compact-columns",       nullptr,            LOI_COMPACT_COLUMNS },
        { L"no-compact-columns",    nullptr,            LOI_NO_COMPACT_COLUMNS },
        { L"escape-codes",          nullptr,            LOI_ESCAPE_CODES,       LOHA_OPTIONAL },
        { L"fat",                   nullptr,            'z' },
        { L"no-fat",                nullptr,            LOI_NO_FAT },
        { L"full-paths",            nullptr,            'F' },
        { L"no-full-paths",         nullptr,            LOI_NO_FULL_PATHS },
        { L"grid",                  nullptr,            'G' },
        { L"help",                  nullptr,            '?' },
        { L"hide-dot-files",        nullptr,            LOI_HIDE_DOT_FILES },
        { L"no-hide-dot-files",     nullptr,            LOI_NO_HIDE_DOT_FILES },
        { L"horizontal",            nullptr,            LOI_HORIZONTAL },
        { L"hyperlinks",            nullptr,            LOI_HYPERLINKS },
        { L"no-hyperlinks",         nullptr,            LOI_NO_HYPERLINKS },
        { L"icons",                 nullptr,            LOI_ICONS,              LOHA_OPTIONAL },
        { L"no-icons",              nullptr,            LOI_NO_ICONS },
        { L"justify",               nullptr,            LOI_JUSTIFY,            LOHA_OPTIONAL },
        { L"lower",                 nullptr,            LOI_LOWER },
        { L"no-lower",              nullptr,            LOI_NO_LOWER },
        { L"more-colors",           nullptr,            LOI_MORE_COLORS,        LOHA_REQUIRED },
        { L"nerd-fonts",            nullptr,            LOI_NERD_FONTS_VER,     LOHA_REQUIRED },
        { L"nix",                   nullptr,            LOI_NIX },
        { L"no-nix",                nullptr,            LOI_NO_NIX },
        { L"normal",                nullptr,            'n' },
        { L"no-normal",             nullptr,            LOI_NO_NORMAL },
        { L"owner",                 nullptr,            'q' },
        { L"no-owner",              nullptr,            LOI_NO_OWNER },
        { L"pad-icons",             nullptr,            LOI_PAD_ICONS,          LOHA_REQUIRED },
        { L"paginate",              nullptr,            'p' },
        { L"quash",                 nullptr,            'Q',                    LOHA_OPTIONAL },
        { L"ratio",                 nullptr,            'C' },
        { L"no-ratio",              nullptr,            LOI_NO_RATIO },
        { L"recurse",               nullptr,            's' },
        { L"short-names",           nullptr,            'x' },
        { L"no-short-names",        nullptr,            LOI_NO_SHORT_NAMES },
        { L"size",                  nullptr,            'S',                    LOHA_OPTIONAL },
        { L"no-size",               nullptr,            LOI_NO_SIZE },
        { L"skip",                  nullptr,            'X',                    LOHA_OPTIONAL },
        { L"streams",               nullptr,            'r' },
        { L"no-streams",            nullptr,            LOI_NO_STREAMS },
        { L"string-sort",           nullptr,            LOI_STRING_SORT },
        { L"time",                  nullptr,            'T',                    LOHA_OPTIONAL },
        { L"no-time",               nullptr,            LOI_NO_TIME },
        { L"truncate-char",         nullptr,            LOI_TRUNCATE_CHAR,      LOHA_REQUIRED },
        { L"usage",                 nullptr,            'u' },
        { L"version",               nullptr,            'V' },
        { L"vertical",              nullptr,            'v' },
        { L"wide",                  nullptr,            'w' },
        { L"width",                 nullptr,            'W',                    LOHA_REQUIRED },
        { L"word-sort",             nullptr,            LOI_WORD_SORT },
#ifdef DEBUG
        { L"print-all-icons",       &print_all_icons,   1 },
#endif
        { nullptr }
    };

    Options opts(99);
    MakeArgv argvEnv(_wgetenv(c_DIRXCMD));

    // First parse options in the DIRXCMD environment variable.
    if (!opts.Parse(argvEnv.Argc(), argvEnv.Argv(), c_opts, usage.Text(), OPT_NONE, long_opts))
    {
        fwprintf(stderr, L"In %%%s%%: %s", c_DIRXCMD, opts.ErrorString());
        return 1;
    }

    int t_count = 0;
    unsigned t_index = 0;
    for (; const WCHAR* opt_value = opts.GetValue('t', t_index); t_index++)
    {
        if (*opt_value == '-')
            t_count = 0;
        else
            ++t_count;
    }
    show_all_attributes = (t_count >= 2);

    // Then parse options from the command line.
    if (!opts.Parse(argc, argv, c_opts, usage.Text(), OPT_ANY|OPT_ANYWHERE|OPT_LONGABBR, long_opts))
    {
        fputws(opts.ErrorString(), stderr);
        return 1;
    }

    for (; const WCHAR* opt_value = opts.GetValue('t', t_index); t_index++)
    {
        if (*opt_value == '-')
            t_count = 0, show_all_attributes = false;
        else
            ++t_count;
    }
    show_all_attributes = show_all_attributes || (t_count >= 2);

    assert(!argvEnv.Argc());

    // Full usage text.

    if (opts['?'])
    {
        SetPagination(true);
        s.Clear();
        if (argv[0])
        {
            if (!wcsicmp(argv[0], L"colors"))
            {
                s.SetA(c_help_colors);
            }
            else if (!wcsicmp(argv[0], L"icons"))
            {
                s.SetA(c_help_icons);
            }
            else if (!wcsicmp(argv[0], L"pictures"))
            {
                StrW tmp;
                tmp.SetA(c_help_pictures);
                s.Printf(tmp.Text(), GetTruncationCharacter());
            }
            else if (!wcsicmp(argv[0], L"regex"))
            {
                s.SetA(c_help_regex);
            }
        }
        if (s.Empty())
        {
            app.ToUpper();
            fmt.SetA(c_long_usage);
            s.Printf(fmt.Text(), app.Text());
        }
        OutputConsole(GetStdHandle(STD_OUTPUT_HANDLE), s.Text());
        return 0;
    }

    // Version information.

    if (opts['V'])
    {
        app.ToUpper();
        s.Clear();
        s.Printf(L"%s %hs, built %hs\n", app.Text(), VERSION_STR, __DATE__);
        OutputConsole(GetStdHandle(STD_OUTPUT_HANDLE), s.Text());
        return 0;
    }

    // Interpret the options.

    InitLocale();

    FormatFlags flags = FMT_COLORS|FMT_AUTOSEPTHOUSANDS;
    WhichTimeStamp timestamp = TIMESTAMP_MODIFIED;
    WhichFileSize filesize = FILESIZE_FILESIZE;
    DWORD dwAttrIncludeAny = 0;
    DWORD dwAttrMatch = 0;
    DWORD dwAttrExcludeAny = FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM;
    bool fresh_a_flag = true;
    const WCHAR* picture = 0;
    const WCHAR* opt_value;
    const LongOption<WCHAR>* long_opt;
    WCHAR ch;

    for (unsigned ii = 0; opts.GetValue(ii, ch, opt_value, &long_opt); ii++)
    {
        FormatFlags flagsON = FMT_NONE;
        FormatFlags flagsOFF = FMT_NONE;

        switch (ch)
        {
        case ',':       flagsON = FMT_SEPARATETHOUSANDS; flagsOFF = FMT_SEPARATETHOUSANDS|FMT_AUTOSEPTHOUSANDS; break;
        case 'b':       flagsON = FMT_BARE; break;
        case 'c':       flagsON = FMT_COLORS; break;
        case 'C':       flagsON = FMT_COMPRESSED; break;
        case 'F':       flagsON = FMT_FULLNAME|FMT_FORCENONFAT|FMT_HIDEPSEUDODIRS; break;
        case 'h':       flagsON = FMT_HIDEPSEUDODIRS; break;
        case 'i':       SetUseIcons((*opt_value == '-') ? L"never" : L"auto"); continue;
        case 'j':       flagsON = FMT_JUSTIFY_FAT; break;
        case 'J':       flagsON = FMT_JUSTIFY_NONFAT; break;
        case 'k':       SetColorScale((*opt_value == '-') ? L"none" : L"all"); continue;
        case 'q':       flagsON = FMT_SHOWOWNER; break;
        case 'r':       flagsON = FMT_ALTDATASTEAMS|FMT_FORCENONFAT; break;
        case 'R':       flagsON = FMT_SUBDIRECTORIES; break;
        case ':':       flagsON = FMT_ALTDATASTEAMS|FMT_FORCENONFAT; break;
        case 's':       flagsON = FMT_SUBDIRECTORIES; break;
        case 't':       flagsON = FMT_ATTRIBUTES; break;
        case 'u':       flagsON = FMT_USAGE; break;
        case 'v':       flagsON = FMT_SORTVERTICAL; break;
        case 'x':       flagsON = FMT_SHORTNAMES; break;

        case 'a':
            if (fresh_a_flag)
            {
                fresh_a_flag = false;
                dwAttrIncludeAny = 0;
                dwAttrMatch = 0;
                dwAttrExcludeAny = 0;
            }
            SkipColonOrEqual(opt_value);
            if (wcscmp(opt_value, L"-") == 0)
            {
                fresh_a_flag = true;
                dwAttrIncludeAny = 0;
                dwAttrMatch = 0;
                dwAttrExcludeAny = FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM;
                HideDotFiles(true);
                continue;
            }
            else if (!*opt_value)
            {
                HideDotFiles(false);
                continue;
            }
            while (*opt_value)
            {
                DWORD* pdwAttr = &dwAttrMatch;
                if (*opt_value == '-')
                {
                    pdwAttr = &dwAttrExcludeAny;
                    opt_value++;
                }
                else if (*opt_value == '+')
                {
                    pdwAttr = &dwAttrIncludeAny;
                    opt_value++;
                }
                if (!*opt_value)
                    break;
                const DWORD dwAttr = ParseAttribute(*opt_value);
                if (!dwAttr)
                {
                    WCHAR sz[2];
                    sz[0] = *opt_value;
                    sz[1] = 0;
                    e.Set(L"Unrecognized attribute '%1' in '-a%2'.") << sz << opts['a'];
                    return e.Report();
                }
                *pdwAttr |= dwAttr;
                opt_value++;
            }
            break;
        case 'n':
            if (long_opt || *opt_value == '+')
            {
                flags |= FMT_FORCENONFAT;
                flags &= ~FMT_FAT;
            }
            else
            {
                flags &= ~FMT_FORCENONFAT;
            }
            continue;
        case 'Q':
            SkipColonOrEqual(opt_value);
            if (!*opt_value)
            {
                flags &= ~(FMT_NOVOLUMEINFO|FMT_NOHEADER|FMT_NOSUMMARY);
                continue;
            }
            else
            {
                bool enable = true;
                for (const WCHAR* walk = opt_value; *walk; ++walk)
                {
                    switch (*walk)
                    {
                    case '-':   enable = true; break;
                    case '+':   enable = false; break;
                    case 'v':   FlipFlag(flags, FMT_NOVOLUMEINFO, enable, true); break;
                    case 'h':   FlipFlag(flags, enable ? FMT_NOHEADER : FMT_NOHEADER|FMT_NOVOLUMEINFO, enable, true); break;
                    case 's':   FlipFlag(flags, FMT_NOSUMMARY, enable, true); break;
                    default:    FailFlag(*walk, opt_value, ch, long_opt, e); return e.Report();
                    }
                }
            }
            continue;
        case 'W':
            SkipColonOrEqual(opt_value);
            SetConsoleWidth(wcstoul(opt_value, nullptr, 10));
            continue;
        case 'X':
            SkipColonOrEqual(opt_value);
            if (!*opt_value)
            {
                flags &= ~(FMT_SKIPHIDDENDIRS|FMT_SKIPJUNCTIONS|FMT_ONLYALTDATASTREAMS);
                continue;
            }
            else
            {
                bool enable = false;
                for (const WCHAR* walk = opt_value; *walk; ++walk)
                {
                    switch (*walk)
                    {
                    case '-':   enable = false; break;
                    case '+':   enable = true; break;
                    case 'd':   FlipFlag(flags, FMT_SKIPHIDDENDIRS, enable, false); break;
                    case 'j':   FlipFlag(flags, FMT_SKIPJUNCTIONS, enable, false); break;
                    case 'r':   FlipFlag(flags, FMT_ONLYALTDATASTREAMS, enable, false); break;
                    default:    FailFlag(*walk, opt_value, ch, long_opt, e); return e.Report();
                    }
                }
            }
            continue;
        case 'z':
            if (long_opt || *opt_value == '+')
            {
                flags |= FMT_FAT;
                flags &= ~FMT_FORCENONFAT;
            }
            else
            {
                flags &= ~FMT_FAT;
            }
            continue;

        default:
            if (!long_opt)
                continue; // Other flags are handled separately further below.
            switch (long_opt->value)
            {
            case LOI_COLOR_SCALE:
                if (!SetColorScale(opt_value))
                {
unrecognized_long_opt_value:
                    e.Set(L"Unrecognized value '%1' for '--%2'.") << opt_value << long_opt->name;
                    return e.Report();
                }
                break;
            case LOI_COLOR_SCALE_MODE:
                if (!SetColorScaleMode(opt_value))
                    goto unrecognized_long_opt_value;
                break;
            case LOI_ESCAPE_CODES:
                if (!SetUseEscapeCodes(opt_value))
                    goto unrecognized_long_opt_value;
                break;
            case LOI_ICONS:
                if (!SetUseIcons(opt_value))
                    goto unrecognized_long_opt_value;
                break;
            case LOI_JUSTIFY:
                if (!opt_value) opt_value = L" ";
                else if (!_wcsicmp(opt_value, L"") || !_wcsicmp(opt_value, L"always"))
                    flagsON = FMT_JUSTIFY_FAT|FMT_JUSTIFY_NONFAT;
                else if (!_wcsicmp(opt_value, L"never"))
                    flagsOFF = FMT_JUSTIFY_FAT|FMT_JUSTIFY_NONFAT;
                else if (!_wcsicmp(opt_value, L"fat"))
                    flagsON = FMT_JUSTIFY_FAT, flagsOFF = FMT_JUSTIFY_NONFAT;
                else if (!_wcsicmp(opt_value, L"normal") || !_wcsicmp(opt_value, L"nonfat") || !_wcsicmp(opt_value, L"non-fat"))
                    flagsON = FMT_JUSTIFY_NONFAT, flagsOFF = FMT_JUSTIFY_FAT;
                else
                    goto unrecognized_long_opt_value;
                break;
            case LOI_NIX:
                nix_defaults = true;
                HideDotFiles(true);
                flags |= FMT_COLORS|FMT_FORCENONFAT|FMT_HIDEPSEUDODIRS|FMT_SORTVERTICAL|FMT_SKIPHIDDENDIRS|FMT_NOVOLUMEINFO|FMT_NOHEADER|FMT_NOSUMMARY|FMT_MINIHEADER;
                flags &= ~(FMT_JUSTIFY_FAT|FMT_JUSTIFY_NONFAT|FMT_FAT|FMT_SHORTNAMES|FMT_ONLYSHORTNAMES|FMT_FULLNAME);
                break;
            case LOI_NO_NIX:
                nix_defaults = false;
                HideDotFiles(false);
                flags &= ~(FMT_HIDEPSEUDODIRS|FMT_SORTVERTICAL|FMT_FORCENONFAT|FMT_FAT|FMT_SKIPHIDDENDIRS|FMT_NOVOLUMEINFO|FMT_NOHEADER|FMT_NOSUMMARY|FMT_MINIHEADER);
                break;
            case LOI_TRUNCATE_CHAR:
                SetTruncationCharacterInHex(opt_value);
                break;

            case LOI_NO_ATTRIBUTES:         flagsOFF = FMT_ATTRIBUTES; break;
            case LOI_CLASSIFY:              flagsON = FMT_CLASSIFY; break;
            case LOI_NO_CLASSIFY:           flagsOFF = FMT_CLASSIFY; break;
            case LOI_NO_COLOR:              flagsOFF = FMT_COLORS; break;
            case LOI_NO_COLOR_SCALE:        SetColorScale(L"none"); break;
            case LOI_COMPACT_COLUMNS:       SetCanAutoFit(true); break;
            case LOI_NO_COMPACT_COLUMNS:    SetCanAutoFit(false); break;
            case LOI_NO_FAT:                flagsOFF = FMT_FAT; break;
            case LOI_NO_FULL_PATHS:         flagsOFF = FMT_FULLNAME|FMT_FORCENONFAT|FMT_HIDEPSEUDODIRS; break;
            case LOI_HORIZONTAL:            flagsOFF = FMT_SORTVERTICAL; break;
            case LOI_HYPERLINKS:            flagsON = FMT_HYPERLINKS; break;
            case LOI_NO_HYPERLINKS:         flagsOFF = FMT_HYPERLINKS; break;
            case LOI_NO_ICONS:              SetUseIcons(L"never"); break;
            case LOI_LOWER:                 flagsON = FMT_LOWERCASE; break;
            case LOI_NO_LOWER:              flagsOFF = FMT_LOWERCASE; break;
            case LOI_NO_NORMAL:             flagsOFF = FMT_FORCENONFAT; break;
            case LOI_MORE_COLORS:           more_colors = opt_value; break;
            case LOI_NERD_FONTS_VER:        SetNerdFontsVersion(wcstoul(opt_value, nullptr, 10)); break;
            case LOI_NO_OWNER:              flagsOFF = FMT_SHOWOWNER; break;
            case LOI_PAD_ICONS:             SetPadIcons(wcstoul(opt_value, nullptr, 10)); break;
            case LOI_NO_RATIO:              flagsOFF = FMT_COMPRESSED; break;
            case LOI_NO_SHORT_NAMES:        flagsOFF = FMT_SHORTNAMES; break;
            case LOI_NO_SIZE:               flagsOFF = FMT_SIZE; break;
            case LOI_NO_STREAMS:            flagsOFF = FMT_ALTDATASTEAMS|FMT_FORCENONFAT; break;
            case LOI_STRING_SORT:           g_dwCmpStrFlags |= SORT_STRINGSORT; break;
            case LOI_NO_TIME:               flagsOFF = FMT_DATE; break;
            case LOI_WORD_SORT:             g_dwCmpStrFlags &= ~SORT_STRINGSORT; break;
            }
            break;
        }

        if (flagsON || flagsOFF)
        {
            if (!flagsOFF)
                flagsOFF = flagsON;

            if (*opt_value == '+' || (long_opt && flagsON))
                flags |= flagsON;
            else if (*opt_value == '-' || (long_opt && flagsOFF))
                flags &= ~flagsOFF;
            else
                assert(false);
        }
    }

    if (flags & FMT_USAGE)
    {
        dwAttrIncludeAny = 0;
        dwAttrMatch = 0;
        dwAttrExcludeAny = 0;
    }

    for (unsigned ii = 0; opt_value = opts.GetValue('f', ii); ii++)
    {
        SkipColonOrEqual(opt_value);
        if (wcscmp(opt_value, L"-") == 0)
        {
            picture = 0;
            continue;
        }
        picture = opt_value;
    }

    for (unsigned ii = 0; opts.GetValue(ii, ch, opt_value); ii++)
    {
        switch (ch)
        {
        case 'S':
            SkipColonOrEqual(opt_value);
            if (wcscmp(opt_value, L"-") == 0)
            {
                flags &= ~(FMT_SIZE|FMT_MINISIZE);
                filesize = FILESIZE_FILESIZE;
                continue;
            }
            flags |= FMT_SIZE;
            if (wcscmp(opt_value, L"S") == 0)
            {
                flags |= FMT_FULLSIZE;
                continue;
            }
            if (!wcscmp(opt_value, L"a"))
                filesize = FILESIZE_ALLOCATION;
            else if (!wcscmp(opt_value, L"c"))
                filesize = FILESIZE_COMPRESSED;
            else if (!wcscmp(opt_value, L"f"))
                filesize = FILESIZE_FILESIZE;
            else if (*opt_value)
            {
                e.Set(L"Unrecognized flag '-S%1'.") << opts['S'];
                return e.Report();
            }
            break;
        case 'T':
            SkipColonOrEqual(opt_value);
            if (wcscmp(opt_value, L"-") == 0)
            {
                flags &= ~(FMT_DATE|FMT_MINIDATE);
                timestamp = TIMESTAMP_MODIFIED;
                continue;
            }
            flags |= FMT_DATE;
            if (wcscmp(opt_value, L"T") == 0)
            {
                flags |= FMT_FULLTIME;
                continue;
            }
            if (!wcscmp(opt_value, L"a"))
                timestamp = TIMESTAMP_ACCESS;
            else if (!wcscmp(opt_value, L"c"))
                timestamp = TIMESTAMP_CREATED;
            else if (!wcscmp(opt_value, L"w") ||
                     !wcscmp(opt_value, L"m"))
                timestamp = TIMESTAMP_MODIFIED;
            else if (*opt_value)
            {
                e.Set(L"Unrecognized flag '-T%1'.") << opts['T'];
                return e.Report();
            }
            break;
        case 'Y':
            if (wcscmp(opt_value, L"-") == 0)
                flags &= ~FMT_MINIDATE;
            else
                flags |= FMT_MINIDATE;
            break;
        case 'Z':
            if (wcscmp(opt_value, L"-") == 0)
                flags &= ~FMT_MINISIZE;
            else
                flags |= FMT_MINISIZE;
            break;
        }
    }

    for (unsigned ii = 0; opt_value = opts.GetValue('o', ii); ii++)
    {
        SetSortOrder(opt_value, e);
        if (e.Test())
            return e.Report();
    }

    unsigned cColumns = 1;
    if (nix_defaults)
        cColumns = 0;

    if (!(flags & (FMT_BARE|FMT_ATTRIBUTES|FMT_FULLNAME|FMT_FULLTIME|FMT_COMPRESSED|FMT_FORCENONFAT|FMT_SHOWOWNER|FMT_ONLYALTDATASTREAMS|FMT_ALTDATASTEAMS)))
    {
        for (unsigned ii = 0; opts.GetValue(ii, ch, opt_value); ii++)
        {
            switch (ch)
            {
            case '1':
            case 'l':
                if (*opt_value != '-')
                    cColumns = 1;
                else if (cColumns == 1)
                    cColumns = 0;
                break;
            case '2':
                if (!picture)
                {
                    if (*opt_value != '-')
                        cColumns = 2;
                    else if (cColumns == 2)
                        cColumns = 1;
                }
                break;
            case '4':
                if (!picture)
                {
                    if (*opt_value != '-')
                        cColumns = 4;
                    else if (cColumns == 4)
                        cColumns = 1;
                }
                break;
            case 'G':
            case 'w':
                if (*opt_value != '-')
                    cColumns = 0;
                else if (cColumns == 0)
                    cColumns = 1;
                break;
            }
        }
    }

    if (flags & FMT_FORCENONFAT)
        flags &= ~FMT_FAT;
    if (cColumns != 1 && !nix_defaults && !(flags & (FMT_FAT|FMT_ATTRIBUTES|FMT_MINISIZE|FMT_CLASSIFY)))
        flags |= FMT_DIRBRACKETS;
    if (!(flags & FMT_FAT))
        flags |= FMT_FULLSIZE;
    if (flags & FMT_BARE)
    {
        flags &= ~(FMT_ALTDATASTEAMS|FMT_JUSTIFY_FAT|FMT_JUSTIFY_NONFAT);
        SetUseIcons(false);
    }

    if (flags & FMT_SEPARATETHOUSANDS)
        flags |= FMT_FULLSIZE;
    if (flags & FMT_AUTOSEPTHOUSANDS)
        flags |= FMT_SEPARATETHOUSANDS;

    if (flags & FMT_USAGE)
    {
        cColumns = 1;
        if (flags & FMT_SUBDIRECTORIES)
            flags |= FMT_USAGEGROUPED;
        flags &= (FMT_MINISIZE|FMT_LOWERCASE|FMT_FULLSIZE|FMT_COMPRESSED|
                  FMT_SEPARATETHOUSANDS|FMT_REDIRECTED|FMT_AUTOSEPTHOUSANDS|
                  FMT_USAGE|FMT_USAGEGROUPED|FMT_MINIDATE);
        flags |= FMT_BARE|FMT_SUBDIRECTORIES;
    }

    if (cColumns != 1)
        flags &= ~FMT_FULLSIZE;

    if ((flags & FMT_ATTRIBUTES) && show_all_attributes)
        flags |= FMT_ALLATTRIBUTES;

    // Initialize directory entry formatter.

    if (opts['p'])
        SetPagination(true);

    if (!CanUseEscapeCodes(GetStdHandle(STD_OUTPUT_HANDLE)))
    {
        flags &= ~FMT_COLORS;
        SetUseIcons(false);
        SetColorScale(L"none");
    }

    DirEntryFormatter def;
    def.Initialize(cColumns, flags, timestamp, filesize, dwAttrIncludeAny, dwAttrMatch, dwAttrExcludeAny, picture);

    if (def.Settings().IsSet(FMT_COLORS))
    {
        assert(!e.Test());
        InitColors(more_colors);
    }

#ifdef DEBUG
    if (print_all_icons)
    {
        PrintAllIcons();
        return 0;
    }
#endif

    // Determine path(s) to scan.

    DirPattern* patterns = MakePatterns(argc, argv, def.Settings(), e);
    if (e.Test())
        return e.Report();

    // Scan the file system and list the files.

    if (def.Settings().IsSet(FMT_ALTDATASTEAMS|FMT_ONLYALTDATASTREAMS))
    {
        if (!EnsureFileStreamFunctions())
        {
            e.Set(L"The operating system is unable to enumerate alternate data streams.");
            return e.Report();
        }
    }

    if (def.Settings().IsSet(FMT_MINIHEADER) &&
        !def.Settings().IsSet(FMT_SUBDIRECTORIES) &&
        (!patterns || !patterns->m_next))
        def.Settings().m_flags &= ~FMT_MINIHEADER;

    const int rc = ScanDir(def, patterns, e);

    if (e.Test())
        return e.Report();

    return rc;
}

