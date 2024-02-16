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

static const WCHAR c_opts[] = L"/:+?V,+1+2+4+a.A+b+c+E.f:F+h+j+J+l+L+M+n.o.p+q+r+s+S.t+T.u+v+w+x+Y+z+Z+";
static const WCHAR c_DIRXCMD[] = L"DIRXCMD";

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

/**
 * Can only return non-zero when !fRestore and ch == 'w'.
 */
static int HandleDisableOption(WCHAR ch, bool fRestore, DWORD& flags, Error& e)
{
    if (fRestore)
    {
        if (ch == 'r')
            flags &= ~FMT_ONLYALTDATASTREAMS;
        else if (ch == 'c')
            flags &= ~FMT_DISABLECOLORS;
        else if (ch == 'w')
            g_dwCmpStrFlags &= ~SORT_STRINGSORT;
    }
    else
    {
        if (ch == 'r')
            flags |= FMT_ONLYALTDATASTREAMS;
        else if (ch == 'c')
            flags |= FMT_DISABLECOLORS;
        else if (ch == 'w')
            g_dwCmpStrFlags |= SORT_STRINGSORT;
    }
    return 0;
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
        SetColorScale(_wgetenv(L"DIRX_COLOR_SCALE"));
        SetColorScaleMode(_wgetenv(L"DIRX_COLOR_SCALE_MODE"));
        if (env = _wgetenv(L"DIRX_NERD_FONTS_VERSION"))
            SetNerdFontsVersion(wcstoul(env, nullptr, 10));
        if (env = _wgetenv(L"DIRX_PAD_ICONS"))
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

    int hide_dot_files = 0;         // By default, behave like CMD DIR.

    enum
    {
        LOI_UNIQUE_IDS              = 0x7FFF,
        LOI_CLASSIFY,
        LOI_NO_CLASSIFY,
        LOI_COLOR_SCALE,
        LOI_NO_COLOR_SCALE,
        LOI_COLOR_SCALE_MODE,
        LOI_ESCAPE_CODES,
        LOI_HYPERLINKS,
        LOI_NO_HYPERLINKS,
        LOI_ICONS,
        LOI_NO_ICONS,
        LOI_NERD_FONTS_VER,
        LOI_PAD_ICONS,
    };

    static LongOption<WCHAR> long_opts[] =
    {
        { L"classify",              nullptr,            LOI_CLASSIFY },
        { L"no-classify",           nullptr,            LOI_NO_CLASSIFY },
        { L"color-scale",           nullptr,            LOI_COLOR_SCALE,        LOHA_OPTIONAL },
        { L"no-color-scale",        nullptr,            LOI_NO_COLOR_SCALE },
        { L"color-scale-mode",      nullptr,            LOI_COLOR_SCALE_MODE,   LOHA_REQUIRED },
        { L"escape-codes",          nullptr,            LOI_ESCAPE_CODES,       LOHA_OPTIONAL },
        { L"hide-dot-files",        &hide_dot_files,    1 },
        { L"no-hide-dot-files",     &hide_dot_files,    -1 },
        { L"hyperlinks",            nullptr,            LOI_HYPERLINKS },
        { L"no-hyperlinks",         nullptr,            LOI_NO_HYPERLINKS },
        { L"icons",                 nullptr,            LOI_ICONS },
        { L"no-icons",              nullptr,            LOI_NO_ICONS },
        { L"nerd-fonts-version",    nullptr,            LOI_NERD_FONTS_VER,     LOHA_REQUIRED },
        { L"pad-icons",             nullptr,            LOI_PAD_ICONS,          LOHA_REQUIRED },
        { nullptr }
    };

    MakeArgv argvEnv(_wgetenv(c_DIRXCMD));

    Options opts(99);
    if (!opts.Parse(argvEnv.Argc(), argvEnv.Argv(), c_opts, usage.Text(), OPT_NONE, long_opts))
    {
        fwprintf(stderr, L"In %%%s%%: %s", c_DIRXCMD, opts.ErrorString());
        return 1;
    }
    if (!opts.Parse(argc, argv, c_opts, usage.Text(), OPT_ANY|OPT_ANYWHERE|OPT_LONGABBR, long_opts))
    {
        fputws(opts.ErrorString(), stderr);
        return 1;
    }

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

    DWORD flags = FMT_AUTOSEPTHOUSANDS;
    if (InitLocale())
        flags |= FMT_LOCALEDATETIME;

    WhichTimeStamp timestamp = TIMESTAMP_MODIFIED;
    WhichFileSize filesize = FILESIZE_FILESIZE;
    DWORD dwAttrIncludeAny = 0;
    DWORD dwAttrMatch = 0;
    DWORD dwAttrExcludeAny = FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM;
    StrW sDisableOptions;
    const WCHAR* picture = 0;
    const WCHAR* opt_value;
    const LongOption<WCHAR>* long_opt;
    WCHAR ch;

    HideDotFiles(hide_dot_files);

    for (unsigned ii = 0; opts.GetValue(ii, ch, opt_value, &long_opt); ii++)
    {
        DWORD flagsON = 0;
        DWORD flagsOFF = 0;

        switch (ch)
        {
        case ',':       flagsON = FMT_SEPARATETHOUSANDS; flagsOFF = FMT_SEPARATETHOUSANDS|FMT_AUTOSEPTHOUSANDS; break;
        case 'b':       flagsON = FMT_BARE; break;
        case 'c':       flagsON = FMT_COMPRESSED; break;
        case 'F':       flagsON = FMT_FULLNAME|FMT_FORCENONFAT|FMT_HIDEDOTS; break;
        case 'h':       flagsON = FMT_HIDEDOTS; break;
        case 'j':       flagsON = FMT_JUSTIFY_FAT; break;
        case 'J':       flagsON = FMT_JUSTIFY_NONFAT; break;
        case 'l':       flagsON = FMT_LOWERCASE; break;
        case 'L':       flagsON = FMT_LOCALEDATETIME; break;
        case 'M':       flagsON = FMT_WIDELISTIME; break;
        case 'q':       flagsON = FMT_SHOWOWNER; break;
        case 'r':       flagsON = FMT_ALTDATASTEAMS|FMT_FORCENONFAT; break;
        case ':':       flagsON = FMT_ALTDATASTEAMS|FMT_FORCENONFAT; break;
        case 's':       flagsON = FMT_SUBDIRECTORIES; break;
        case 't':       flagsON = FMT_ATTRIBUTES; break;
        case 'u':       flagsON = FMT_USAGE; break;
        case 'v':       flagsON = FMT_SORTVERTICAL; break;
        case 'x':       flagsON = FMT_SHORTNAMES; break;
        case 'z':       flagsON = FMT_FAT; break;
        default:
            if (!long_opt)
                continue; // Other flags are handled separately further below.
            switch (long_opt->value)
            {
            case LOI_COLOR_SCALE:
                if (!SetColorScale(opt_value))
                {
                    e.Set(L"Unrecognized value '%1' for '--%2'.") << opt_value << long_opt->name;
                    return e.Report();
                }
                break;
            case LOI_COLOR_SCALE_MODE:
                if (!SetColorScaleMode(opt_value))
                {
                    e.Set(L"Unrecognized value '%1' for '--%2'.") << opt_value << long_opt->name;
                    return e.Report();
                }
                break;
            case LOI_ESCAPE_CODES:
                if (!SetUseEscapeCodes(opt_value))
                {
                    e.Set(L"Unrecognized value '%1' for '--%2'.") << opt_value << long_opt->name;
                    return e.Report();
                }
                break;
            case LOI_CLASSIFY:          flags |= FMT_CLASSIFY; break;
            case LOI_NO_CLASSIFY:       flags &= ~FMT_CLASSIFY; break;
            case LOI_NO_COLOR_SCALE:    SetColorScaleMode(L"none"); break;
            case LOI_HYPERLINKS:        flags |= FMT_HYPERLINKS; break;
            case LOI_NO_HYPERLINKS:     flags &= ~FMT_HYPERLINKS; break;
            case LOI_ICONS:             SetUseIcons(true); break;
            case LOI_NO_ICONS:          SetUseIcons(false); break;
            case LOI_NERD_FONTS_VER:    SetNerdFontsVersion(wcstoul(opt_value, nullptr, 10)); break;
            case LOI_PAD_ICONS:         SetPadIcons(wcstoul(opt_value, nullptr, 10)); break;
            }
            break;
        }

        if (!long_opt)
        {
            if (!flagsOFF)
                flagsOFF = flagsON;

            if (*opt_value == '+')
                flags |= flagsON;
            else if (*opt_value == '-')
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

    for (unsigned ii = 0; opt_value = opts.GetValue('a', ii); ii++)
    {
        if (!ii)
        {
            dwAttrIncludeAny = 0;
            dwAttrMatch = 0;
            dwAttrExcludeAny = 0;
        }
        SkipColonOrEqual(opt_value);
        if (wcscmp(opt_value, L"-") == 0)
        {
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
    }

    for (unsigned ii = 0; opt_value = opts.GetValue('E', ii); ii++)
    {
        SkipColonOrEqual(opt_value);
        if (opt_value[0] == '$')
            opt_value++;
        else if (opt_value[0] == '0' && _tolower(opt_value[1]) == 'x')
            opt_value += 2;

        ch = 0;
        WORD w;

        if (*opt_value && ParseHexDigit(*opt_value, &w))
        {
            ch <<= 4;
            ch |= w;
            opt_value++;
            if (*opt_value && ParseHexDigit(*opt_value, &w))
            {
                ch <<= 4;
                ch |= w;
                opt_value++;
#ifdef UNICODE
                if (*opt_value && ParseHexDigit(*opt_value, &w))
                {
                    ch <<= 4;
                    ch |= w;
                    opt_value++;
                    if (*opt_value && ParseHexDigit(*opt_value, &w))
                    {
                        ch <<= 4;
                        ch |= w;
                        opt_value++;
                    }
                }
#endif
            }
        }

        if (!*opt_value)
            SetTruncationCharacter(ch);
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
                flags &= ~FMT_FULLSIZE;
                filesize = FILESIZE_FILESIZE;
                continue;
            }
            flags &= ~FMT_MINISIZE;
            flags |= FMT_FULLSIZE;
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
                flags &= ~FMT_FULLTIME;
                timestamp = TIMESTAMP_MODIFIED;
                continue;
            }
            flags &= ~FMT_MINIDATE;
            flags |= FMT_FULLTIME;
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
            flags &= ~FMT_FULLTIME;
            flags |= FMT_MINIDATE;
            break;
        case 'Z':
            flags &= ~FMT_FULLSIZE;
            flags |= FMT_MINISIZE;
            break;
        }
    }

    for (unsigned ii = 0; opt_value = opts.GetValue('T', ii); ii++)
    {
    }

    for (unsigned ii = 0; opt_value = opts.GetValue('n', ii); ii++)
    {
        if (!*opt_value)
        {
            flags |= FMT_FORCENONFAT;
            continue;
        }
        SkipColonOrEqual(opt_value);
        if (wcscmp(opt_value, L"-") == 0)
        {
            for (size_t jj = 0; jj < sDisableOptions.Length(); jj++)
                HandleDisableOption(sDisableOptions.Text()[jj], true, flags, e);
            sDisableOptions.Clear();
            continue;
        }
        bool fPlus = false;
        for (; *opt_value; opt_value++)
        {
            const bool fWasPlus = fPlus;
            fPlus = (*opt_value == '+');
            if (!wcschr(L"-+vhdjsrcw", *opt_value))
            {
                e.Set(L"Unrecognized flag in '-n%1'.") << opts.GetValue('n', ii);
                return e.Report();
            }
            if (fWasPlus)
            {
                StrW tmp;
                for (size_t jj = 0; jj < sDisableOptions.Length(); jj++)
                {
                    if (sDisableOptions.Text()[jj] != *opt_value)
                    {
                        tmp.Append(sDisableOptions.Text()[jj]);
                        continue;
                    }
                    HandleDisableOption(*opt_value, true, flags, e);
                }
                sDisableOptions = std::move(tmp);
            }
            else
            {
                if (wcschr(sDisableOptions.Text(), *opt_value))
                    continue;
                if (HandleDisableOption(*opt_value, false, flags, e))
                    return e.Report();
                sDisableOptions.Append(*opt_value);
            }
        }
    }

    for (unsigned ii = 0; opt_value = opts.GetValue('o', ii); ii++)
    {
        SetSortOrder(opt_value, e);
        if (e.Test())
            return e.Report();
    }

    unsigned cColumns = 1;

    if (!(flags & (FMT_BARE|FMT_ATTRIBUTES|FMT_FULLNAME|FMT_FULLTIME|FMT_COMPRESSED|FMT_FORCENONFAT|FMT_SHOWOWNER|FMT_ONLYALTDATASTREAMS|FMT_ALTDATASTEAMS)))
    {
        for (unsigned ii = 0; opts.GetValue(ii, ch, opt_value); ii++)
        {
            switch (ch)
            {
            case '1':
                cColumns = 1;
                break;
            case '2':
                if (!picture)
                {
                    if (*opt_value == '-' && cColumns == 2)
                        cColumns = 1;
                    else
                        cColumns = 2;
                }
                break;
            case '4':
                if (!picture)
                {
                    if (*opt_value == '-' && cColumns == 4)
                        cColumns = 1;
                    else
                        cColumns = 4;
                }
                break;
            case 'w':
                if (*opt_value == '-' && cColumns == 0)
                    cColumns = 1;
                else
                    cColumns = 0;
                break;
            }
        }
    }

    if (flags & FMT_FORCENONFAT)
        flags &= ~FMT_FAT;
    if (cColumns != 1 && !(flags & (FMT_FAT|FMT_ATTRIBUTES|FMT_MINISIZE)))
        flags |= FMT_DIRBRACKETS;
    if (!(flags & FMT_FAT))
        flags |= FMT_FULLSIZE;
    if (flags & FMT_BARE)
        flags &= ~(FMT_ALTDATASTEAMS|FMT_JUSTIFY_FAT|FMT_JUSTIFY_NONFAT);

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

    if (flags & FMT_CLASSIFY)
        flags &= ~FMT_DIRBRACKETS;

    // Initialize directory entry formatter.

    if (opts['p'])
        SetPagination(true);

    if (!CanUseEscapeCodes(GetStdHandle(STD_OUTPUT_HANDLE)))
        flags |= FMT_DISABLECOLORS;

    DirEntryFormatter def;
    def.Initialize(cColumns, flags, timestamp, filesize, dwAttrIncludeAny, dwAttrMatch, dwAttrExcludeAny, sDisableOptions.Text(), picture);

    if (!def.Settings().IsSet(FMT_DISABLECOLORS))
    {
        assert(!e.Test());
        InitColors(nullptr);
    }

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

    const int rc = ScanDir(def, patterns, e);

    if (e.Test())
        return e.Report();

    return rc;
}
