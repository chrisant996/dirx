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
#include "fields.h"
#include "scan.h"
#include "colors.h"
#include "samples.h"
#include "icons.h"
#include "usage.h"
#include "wcwidth.h"

#include <memory>

static const WCHAR c_opts[] = L"/:+?V,+1+2+4+a.Ab+Bc+C+f:F+g+G+h+i+I:j+J+k+l+L:n+o.p+q+Q.r+R+s+S.t+T.u+v+w+W:x+X.Y+z+Z+";
static const WCHAR c_DIRXCMD[] = L"DIRXCMD";

int g_debug = 0;
int g_nix_defaults = 0;             // By default, behave like CMD DIR.

static const WCHAR* get_env_prio(const WCHAR* a, const WCHAR* b=nullptr, const WCHAR* c=nullptr, const WCHAR** which=nullptr)
{
    const WCHAR* env = nullptr;
    if (a)
    {
        env = _wgetenv(a);
        if (env && which)
            *which = a;
    }
    if (!env && b)
    {
        env = _wgetenv(b);
        if (env && which)
            *which = b;
    }
    if (!env && c)
    {
        env = _wgetenv(c);
        if (env && which)
            *which = c;
    }
    return env;
}

int __cdecl _tmain(int argc, const WCHAR** argv)
{
    Error e;
    std::vector<FileInfo> rgFiles;
    StrW s;

    if (!IsConsole(GetStdHandle(STD_OUTPUT_HANDLE)))
        SetRedirectedStdOut(true);

    initialize_wcwidth();

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
        const WCHAR* which;
        if (!SetColorScale(env = get_env_prio(L"DIRX_COLOR_SCALE", L"EZA_COLOR_SCALE", L"EXA_COLOR_SCALE", &which)))
        {
            if (env)
            {
                e.Set(L"Unrecognized value '%1' in %%%s%%.") << env << which;
                ReportColorlessError(e);
            }
        }
        if (!SetColorScaleMode(env = get_env_prio(L"DIRX_COLOR_SCALE_MODE", L"EZA_COLOR_SCALE_MODE", L"EXA_COLOR_SCALE_MODE", &which)))
        {
            if (env)
            {
                e.Set(L"Unrecognized value '%1' in %%%s%%.") << env << which;
                ReportColorlessError(e);
            }
        }
        if (env = _wgetenv(L"DIRX_NERD_FONTS_VERSION"))
            SetNerdFontsVersion(wcstoul(env, nullptr, 10));
        if (env = get_env_prio(L"DIRX_ICON_SPACING", L"EZA_ICON_SPACING", L"EXA_ICON_SPACING", &which))
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
    int print_all_icons = 0;
    int utf8_stdout = 0;

    enum
    {
        LOI_UNIQUE_IDS              = 0x7FFF,
        LOI_ATTRIBUTES,
        LOI_NO_ATTRIBUTES,
        LOI_BARE_RELATIVE,
        LOI_NO_BARE_RELATIVE,
        LOI_NO_BARE,
        LOI_CLASSIFY,
        LOI_NO_CLASSIFY,
        LOI_NO_COLOR,
        LOI_COLOR_SCALE,
        LOI_NO_COLOR_SCALE,
        LOI_COLOR_SCALE_MODE,
        LOI_COMPACT_TIME,
        LOI_NO_COMPACT_TIME,
        LOI_DIGIT_SORT,
        LOI_ESCAPE_CODES,
        LOI_NO_FAT,
        LOI_FIT_COLUMNS,
        LOI_NO_FIT_COLUMNS,
        LOI_NO_FULL_PATHS,
        LOI_GIT,
        LOI_NO_GIT,
        LOI_GIT_IGNORE,
        LOI_NO_GIT_IGNORE,
        LOI_GIT_REPOS,
        LOI_GIT_REPOS_NO_STATUS,
        LOI_HIDE_DOT_FILES,
        LOI_NO_HIDE_DOT_FILES,
        LOI_HORIZONTAL,
        LOI_HYPERLINKS,
        LOI_NO_HYPERLINKS,
        LOI_ICONS,
        LOI_NO_ICONS,
        LOI_JUSTIFY,
        LOI_LEVELS,
        LOI_LOWER,
        LOI_NO_LOWER,
        LOI_MINI_BYTES,
        LOI_NO_MINI_BYTES,
        LOI_MINI_DECIMAL,
        LOI_NO_MINI_DECIMAL,
        LOI_MINI_HEADER,
        LOI_NO_MINI_HEADER,
        LOI_MORE_COLORS,
        LOI_NERD_FONTS_VER,
        LOI_NIX,
        LOI_NO_NIX,
        LOI_NO_NORMAL,
        LOI_NUMERIC_SORT,
        LOI_NO_OWNER,
        LOI_PAD_ICONS,
        LOI_NO_RATIO,
        LOI_RELATIVE,
        LOI_NO_RELATIVE,
        LOI_REVERSE,
        LOI_NO_REVERSE,
        LOI_NO_SHORT_NAMES,
        LOI_SIZE,
        LOI_NO_SIZE,
        LOI_SIZE_STYLE,
        LOI_NO_STREAMS,
        LOI_STRING_SORT,
        LOI_TIME,
        LOI_NO_TIME,
        LOI_TIME_STYLE,
        LOI_TREE,
        LOI_NO_TREE,
        LOI_TRUNCATE_CHAR,
        LOI_WORD_SORT,
    };

    static LongOption<WCHAR> long_opts[] =
    {
        { L"all",                   nullptr,            'a' },
        { L"almost-all",            nullptr,            'A' },
        { L"attributes",            nullptr,            LOI_ATTRIBUTES },
        { L"no-attributes",         nullptr,            LOI_NO_ATTRIBUTES },
        { L"bare",                  nullptr,            'b' },
        { L"no-bare",               nullptr,            LOI_NO_BARE },
        { L"almost-bare",           nullptr,            'B' },
        { L"bare-relative",         nullptr,            LOI_BARE_RELATIVE },
        { L"no-bare-relative",      nullptr,            LOI_NO_BARE_RELATIVE },
        { L"classify",              nullptr,            LOI_CLASSIFY },
        { L"no-classify",           nullptr,            LOI_NO_CLASSIFY },
        { L"color",                 nullptr,            'c' },
        { L"no-color",              nullptr,            LOI_NO_COLOR },
        { L"color-scale",           nullptr,            LOI_COLOR_SCALE,        LOHA_OPTIONAL },
        { L"no-color-scale",        nullptr,            LOI_NO_COLOR_SCALE },
        { L"color-scale-mode",      nullptr,            LOI_COLOR_SCALE_MODE,   LOHA_REQUIRED },
        { L"compact",               nullptr,            LOI_COMPACT_TIME },
        { L"no-compact",            nullptr,            LOI_NO_COMPACT_TIME },
        { L"debug",                 &g_debug,           1 },
        { L"no-debug",              &g_debug,           0 },
        { L"digit-sort",            nullptr,            LOI_DIGIT_SORT },
        { L"escape-codes",          nullptr,            LOI_ESCAPE_CODES,       LOHA_OPTIONAL },
        { L"fat",                   nullptr,            'z' },
        { L"no-fat",                nullptr,            LOI_NO_FAT },
        { L"fit-columns",           nullptr,            LOI_FIT_COLUMNS },
        { L"no-fit-columns",        nullptr,            LOI_NO_FIT_COLUMNS },
        { L"full-paths",            nullptr,            'F' },
        { L"no-full-paths",         nullptr,            LOI_NO_FULL_PATHS },
        { L"git",                   nullptr,            LOI_GIT },
        { L"no-git",                nullptr,            LOI_NO_GIT },
        { L"git-ignore",            nullptr,            LOI_GIT_IGNORE },
        { L"no-git-ignore",         nullptr,            LOI_NO_GIT_IGNORE },
        { L"git-repos",             nullptr,            LOI_GIT_REPOS },
        { L"git-repos-no-status",   nullptr,            LOI_GIT_REPOS_NO_STATUS },
        { L"grid",                  nullptr,            'G' },
        { L"no-grid",               nullptr,            '>' },
        { L"help",                  nullptr,            '?' },
        { L"hide-dot-files",        nullptr,            LOI_HIDE_DOT_FILES },
        { L"no-hide-dot-files",     nullptr,            LOI_NO_HIDE_DOT_FILES },
        { L"horizontal",            nullptr,            LOI_HORIZONTAL },
        { L"hyperlinks",            nullptr,            LOI_HYPERLINKS },
        { L"no-hyperlinks",         nullptr,            LOI_NO_HYPERLINKS },
        { L"icons",                 nullptr,            LOI_ICONS,              LOHA_OPTIONAL },
        { L"no-icons",              nullptr,            LOI_NO_ICONS },
        { L"ignore-glob",           nullptr,            'I',                    LOHA_REQUIRED },
        { L"justify",               nullptr,            LOI_JUSTIFY,            LOHA_OPTIONAL },
        { L"levels",                nullptr,            'L',                    LOHA_REQUIRED },
        { L"long",                  nullptr,            'l' },
        { L"no-long",               nullptr,            '<' },
        { L"lower",                 nullptr,            LOI_LOWER },
        { L"no-lower",              nullptr,            LOI_NO_LOWER },
        { L"mini-bytes",            nullptr,            LOI_MINI_BYTES },
        { L"no-mini-bytes",         nullptr,            LOI_NO_MINI_BYTES },
        { L"mini-decimal",          nullptr,            LOI_MINI_DECIMAL },
        { L"no-mini-decimal",       nullptr,            LOI_NO_MINI_DECIMAL },
        { L"mini-header",           nullptr,            LOI_MINI_HEADER },
        { L"no-mini-header",        nullptr,            LOI_NO_MINI_HEADER },
        { L"more-colors",           nullptr,            LOI_MORE_COLORS,        LOHA_REQUIRED },
        { L"nerd-fonts",            nullptr,            LOI_NERD_FONTS_VER,     LOHA_REQUIRED },
        { L"nix",                   nullptr,            LOI_NIX },
        { L"no-nix",                nullptr,            LOI_NO_NIX },
        { L"normal",                nullptr,            'n' },
        { L"no-normal",             nullptr,            LOI_NO_NORMAL },
        { L"numeric-sort",          nullptr,            LOI_NUMERIC_SORT },
        { L"owner",                 nullptr,            'q' },
        { L"no-owner",              nullptr,            LOI_NO_OWNER },
        { L"pad-icons",             nullptr,            LOI_PAD_ICONS,          LOHA_REQUIRED },
        { L"paginate",              nullptr,            'p' },
        { L"quash",                 nullptr,            'Q',                    LOHA_OPTIONAL },
        { L"ratio",                 nullptr,            'C' },
        { L"no-ratio",              nullptr,            LOI_NO_RATIO },
        { L"recurse",               nullptr,            's' },
        { L"relative",              nullptr,            LOI_RELATIVE },
        { L"no-relative",           nullptr,            LOI_NO_RELATIVE },
        { L"reverse",               nullptr,            LOI_REVERSE },
        { L"no-reverse",            nullptr,            LOI_NO_REVERSE },
        { L"short-names",           nullptr,            'x' },
        { L"no-short-names",        nullptr,            LOI_NO_SHORT_NAMES },
        { L"size",                  nullptr,            LOI_SIZE,               LOHA_OPTIONAL },
        { L"no-size",               nullptr,            LOI_NO_SIZE },
        { L"size-style",            nullptr,            LOI_SIZE_STYLE,         LOHA_REQUIRED },
        { L"skip",                  nullptr,            'X',                    LOHA_OPTIONAL },
        { L"streams",               nullptr,            'r' },
        { L"no-streams",            nullptr,            LOI_NO_STREAMS },
        { L"string-sort",           nullptr,            LOI_STRING_SORT },
        { L"time",                  nullptr,            LOI_TIME,               LOHA_OPTIONAL },
        { L"no-time",               nullptr,            LOI_NO_TIME },
        { L"time-style",            nullptr,            LOI_TIME_STYLE,         LOHA_REQUIRED },
        { L"tree",                  nullptr,            LOI_TREE },
        { L"no-tree",               nullptr,            LOI_NO_TREE },
        { L"truncate-char",         nullptr,            LOI_TRUNCATE_CHAR,      LOHA_REQUIRED },
        { L"usage",                 nullptr,            'u' },
        { L"utf8",                  &utf8_stdout,       1 },
        { L"no-utf8",               &utf8_stdout,       0 },
        { L"version",               nullptr,            'V' },
        { L"vertical",              nullptr,            'v' },
        { L"wide",                  nullptr,            'w' },
        { L"no-wide",               nullptr,            '>' },
        { L"width",                 nullptr,            'W',                    LOHA_REQUIRED },
        { L"word-sort",             nullptr,            LOI_WORD_SORT },
        { nullptr }
    };

    Options opts(99);
    MakeArgv argvEnv(_wgetenv(c_DIRXCMD));

    // First parse options in the DIRXCMD environment variable.
    if (!opts.Parse(argvEnv.Argc(), argvEnv.Argv(), c_opts, usage.Text(), OPT_NONE, long_opts))
    {
        fwprintf(stderr, L"In %%%s%%: %s", c_DIRXCMD, opts.ErrorString());
        SetGracefulExit();
        return 1;
    }

    WCHAR ch;
    const WCHAR* opt_value;
    unsigned num_dirxcmd_options = 0;
    while (opts.GetValue(num_dirxcmd_options, ch, opt_value))
        ++num_dirxcmd_options;

    // Then parse options from the command line.
    if (!opts.Parse(argc, argv, c_opts, usage.Text(), OPT_ANY|OPT_ANYWHERE|OPT_LONGABBR, long_opts))
    {
        fputws(opts.ErrorString(), stderr);
        SetGracefulExit();
        return 1;
    }

    assert(!argvEnv.Argc());

    SetUtf8Output(!!utf8_stdout);

    if (g_debug)
    {
        const WCHAR* dirxcmd = _wgetenv(c_DIRXCMD);
        if (dirxcmd)
            Printf(L"debug: DIRXCMD=%s\n", dirxcmd);
        const WCHAR* cmdline = GetCommandLineW();
        if (cmdline)
            Printf(L"debug: cmdline=%s\n", cmdline);
    }

    // Full usage text.

    if (opts['?'])
    {
        s.Clear();
        if (argv[0])
        {
            if (!wcsicmp(argv[0], L"colors"))
            {
                StrW tmp;
                tmp.SetA(c_help_colors);
                s.Printf(tmp.Text(), app.Text(), app.Text());
            }
            else if (!wcsicmp(argv[0], L"colorsamples"))
            {
                SetUseEscapeCodes(L"always");
                PrintColorSamples();
                SetGracefulExit();
                return 0;
            }
            else if (!wcsicmp(argv[0], L"defaultcolors"))
            {
                s.Printf(L"The default color string is:\n\n%s\n", GetDefaultColorString());
            }
            else if (!wcsicmp(argv[0], L"icons"))
            {
                StrW tmp;
                tmp.SetA(c_help_icons);
                s.Printf(tmp.Text(), c_help_icons_examples);
            }
            else if (!wcsicmp(argv[0], L"printallicons"))
            {
                print_all_icons = true;
            }
            else if (!wcsicmp(argv[0], L"pictures"))
            {
                StrW tmp;
                WCHAR trunc[2] = { GetTruncationCharacter(), '\0' };
                tmp.SetA(c_help_pictures);
                s.Printf(tmp.Text(), trunc);
            }
            else if (!wcsicmp(argv[0], L"regex"))
            {
                s.SetA(c_help_regex);
            }
        }
        if (!print_all_icons)
        {
            unsigned width = 80;
#ifdef AUTO_DYNAMIC_WIDTH_FOR_USAGE_TEXT
            width = LOWORD(GetConsoleColsRows(GetStdHandle(STD_OUTPUT_HANDLE)));
#endif
            const LongOption<WCHAR>* long_opt;
            for (unsigned ii = 0; opts.GetValue(ii, ch, opt_value, &long_opt); ii++)
            {
                if (ch == 'W')
                {
                    const unsigned c_min_usage_width = 64;
                    SkipColonOrEqual(opt_value);
                    width = max<unsigned>(c_min_usage_width, wcstoul(opt_value, nullptr, 10));
                }
            }
            SetConsoleWidth(width);
            if (s.Empty())
            {
                app.ToUpper();
                fmt.SetA(MakeUsageString(argv[0] && !wcsicmp(argv[0], L"alphabetical"), (width >= 88) ? 32 : 24));
                s.Printf(fmt.Text(), app.Text());
            }
            SetPagination(true);
            ExpandTabs(s.Text(), s);
            WrapText(s.Text(), s);
            OutputConsole(GetStdHandle(STD_OUTPUT_HANDLE), s.Text());
            SetGracefulExit();
            return 0;
        }
    }

    // Version information.

    if (opts['V'])
    {
        app.ToUpper();
        s.Clear();
        s.Printf(L"%s %hs, built %hs\nhttps://github.com/chrisant996/dirx\n", app.Text(), VERSION_STR, __DATE__);
        OutputConsole(GetStdHandle(STD_OUTPUT_HANDLE), s.Text());
        SetGracefulExit();
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
    unsigned limit_depth = -1;
    bool fresh_a_flag = true;
    bool used_A_flag = false;
    bool used_B_flag = false;
    bool auto_dir_brackets = true;
    const WCHAR* picture = 0;
    const LongOption<WCHAR>* long_opt;
    StrW ignore_globs;

    bool was_g = false;
    bool was_t = false;
    for (unsigned ii = 0; opts.GetValue(ii, ch, opt_value, &long_opt); ii++)
    {
        FormatFlags flagsON = FMT_NONE;
        FormatFlags flagsOFF = FMT_NONE;

        // Adjacent -g or -t have special meaning.
        {
            if (ii == num_dirxcmd_options)
                was_g = was_t = false;

            if (ch != 'g' || *opt_value != '+') was_g = false;
            else if (was_g)                     SetFlag(flags, FMT_GITREPOS);
            else                                was_g = true;

            if (ch != 't' || *opt_value != '+') was_t = false;
            else if (was_t)                     show_all_attributes = true;
            else                                was_t = true;
        }

        switch (ch)
        {
        case ',':       flagsON = FMT_SEPARATETHOUSANDS; flagsOFF = FMT_SEPARATETHOUSANDS|FMT_AUTOSEPTHOUSANDS; break;
        case 'b':       flagsON = FMT_BARE; break;
        case 'c':       flagsON = FMT_COLORS; break;
        case 'C':       flagsON = FMT_COMPRESSED; break;
        case 'F':       flagsON = FMT_FULLNAME|FMT_FORCENONFAT|FMT_HIDEPSEUDODIRS; break;
        case 'g':       flagsON = FMT_GIT; flagsOFF = FMT_GIT|FMT_GITREPOS; break;
        case 'h':       flagsON = FMT_HIDEPSEUDODIRS; used_A_flag = false; break;
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
                if (g_nix_defaults || used_A_flag)
                    ClearFlag(flags, FMT_HIDEPSEUDODIRS);
            }
            used_A_flag = false;
            SkipColonOrEqual(opt_value);
            if (wcscmp(opt_value, L"-") == 0)
            {
                fresh_a_flag = true;
                dwAttrIncludeAny = 0;
                dwAttrMatch = 0;
                dwAttrExcludeAny = g_nix_defaults ? FILE_ATTRIBUTE_HIDDEN : FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM;
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
        case 'A':
            fresh_a_flag = true;
            HideDotFiles(false);
            dwAttrIncludeAny = 0;
            dwAttrMatch = 0;
            dwAttrExcludeAny = 0;
            SetFlag(flags, FMT_HIDEPSEUDODIRS);
            used_A_flag = true;
            break;
        case 'B':
            used_B_flag = true;
            SetFlag(flags, FMT_FORCENONFAT|FMT_SORTVERTICAL|
                           FMT_NOVOLUMEINFO|FMT_NOHEADER|FMT_NOSUMMARY|
                           FMT_MINIHEADER|FMT_MAYBEMINIHEADER|
                           FMT_LONGNODATE|FMT_LONGNOSIZE|FMT_NODIRTAGINSIZE|
                           FMT_HIDEPSEUDODIRS|FMT_SKIPHIDDENDIRS);
            ClearFlag(flags, FMT_JUSTIFY_FAT|FMT_JUSTIFY_NONFAT);
            break;
        case 'I':
            SkipColonOrEqual(opt_value);
            if (wcscmp(opt_value, L"-") == 0)
                ignore_globs.Clear();
            else
            {
                if (!ignore_globs.Empty())
                    ignore_globs.Append(L"|");
                ignore_globs.Append(opt_value);
            }
            break;
        case 'L':
            SkipColonOrEqual(opt_value);
            if (*opt_value)
            {
                unsigned long n = wcstoul(opt_value, nullptr, 10);
                if (long(n) <= 0)
                    limit_depth = 0;
                else if (n >= 0xffffffff)
                    limit_depth = unsigned(-1);
                else
                    limit_depth = n;
            }
            break;
        case 'n':
            FlipFlag(flags, FMT_FORCENONFAT, (long_opt || *opt_value == '+'));
            continue;
        case 'Q':
            SkipColonOrEqual(opt_value);
            if (!*opt_value)
            {
                ClearFlag(flags, FMT_NOVOLUMEINFO|FMT_NOHEADER|FMT_NOSUMMARY);
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
                    case 'h':   FlipFlag(flags, FMT_NOHEADER, enable, true); break;
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
                ClearFlag(flags, FMT_SKIPHIDDENDIRS|FMT_SKIPJUNCTIONS|FMT_ONLYALTDATASTREAMS);
                continue;
            }
            else
            {
                bool enable = true;
                for (const WCHAR* walk = opt_value; *walk; ++walk)
                {
                    switch (*walk)
                    {
                    case '-':   enable = false; break;
                    case '+':   enable = true; break;
                    case 'd':   FlipFlag(flags, FMT_SKIPHIDDENDIRS, enable, true); break;
                    case 'j':   FlipFlag(flags, FMT_SKIPJUNCTIONS, enable, true); break;
                    case 'r':   FlipFlag(flags, FMT_ONLYALTDATASTREAMS, enable, true); break;
                    default:    FailFlag(*walk, opt_value, ch, long_opt, e); return e.Report();
                    }
                }
            }
            continue;
        case 'z':
            FlipFlag(flags, FMT_FAT, (long_opt || *opt_value == '+'));
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
            case LOI_COMPACT_TIME:
                SetFlag(flags, FMT_DATE);
                SetDefaultTimeStyle(L"compact");
                break;
            case LOI_NO_COMPACT_TIME:
                if (ClearDefaultTimeStyleIf(L"compact"))
                    SetFlag(flags, FMT_LONGNODATE);
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
                g_nix_defaults = true;
                HideDotFiles(true);
                SetFlag(flags, FMT_COLORS|FMT_NODIRTAGINSIZE|FMT_FORCENONFAT|FMT_HIDEPSEUDODIRS|FMT_SORTVERTICAL|FMT_SKIPHIDDENDIRS|FMT_NOVOLUMEINFO|FMT_NOHEADER|FMT_NOSUMMARY|FMT_MINIHEADER);
                ClearFlag(flags, FMT_JUSTIFY_FAT|FMT_JUSTIFY_NONFAT|FMT_SHORTNAMES|FMT_ONLYSHORTNAMES|FMT_FULLNAME|FMT_AUTOSEPTHOUSANDS|FMT_SEPARATETHOUSANDS);
                dwAttrExcludeAny &= ~FILE_ATTRIBUTE_SYSTEM;
                SetDefaultTimeStyle(L"compact");
                break;
            case LOI_NO_NIX:
                g_nix_defaults = false;
                HideDotFiles(false);
                SetFlag(flags, FMT_AUTOSEPTHOUSANDS);
                ClearFlag(flags, FMT_NODIRTAGINSIZE|FMT_HIDEPSEUDODIRS|FMT_SORTVERTICAL|FMT_FORCENONFAT|FMT_FAT|FMT_SKIPHIDDENDIRS|FMT_NOVOLUMEINFO|FMT_NOHEADER|FMT_NOSUMMARY|FMT_MINIHEADER);
                dwAttrExcludeAny |= FILE_ATTRIBUTE_SYSTEM;
                SetDefaultTimeStyle(L"locale");
                SetDefaultTimeStyle(L"locale");
                break;
            case LOI_RELATIVE:
                SetFlag(flags, FMT_DATE);
                SetDefaultTimeStyle(L"relative");
                break;
            case LOI_NO_RELATIVE:
                if (ClearDefaultTimeStyleIf(L"relative"))
                    SetFlag(flags, FMT_LONGNODATE);
                break;
            case LOI_SIZE_STYLE:
                if (!SetDefaultSizeStyle(opt_value))
                    goto unrecognized_long_opt_value;
                break;
            case LOI_TIME_STYLE:
                if (!SetDefaultTimeStyle(opt_value))
                    goto unrecognized_long_opt_value;
                break;
            case LOI_TREE:
                SetFlag(flags, FMT_TREE|FMT_SKIPHIDDENDIRS|FMT_SUBDIRECTORIES);
                break;
            case LOI_NO_TREE:
                if (flags & FMT_TREE)
                    ClearFlag(flags, (FMT_TREE|FMT_SKIPHIDDENDIRS|FMT_SUBDIRECTORIES));
                break;
            case LOI_TRUNCATE_CHAR:
                SetTruncationCharacterInHex(opt_value);
                break;

            case LOI_ATTRIBUTES:            flagsON = FMT_ATTRIBUTES; break;
            case LOI_NO_ATTRIBUTES:         flagsON = FMT_LONGNOATTRIBUTES; break;
            case LOI_NO_BARE:               flagsOFF = FMT_BARE; break;
            case LOI_BARE_RELATIVE:         flagsON = FMT_BARERELATIVE; break;
            case LOI_NO_BARE_RELATIVE:      flagsOFF = FMT_BARERELATIVE; break;
            case LOI_CLASSIFY:              flagsON = FMT_CLASSIFY; auto_dir_brackets = false; break;
            case LOI_NO_CLASSIFY:           flagsOFF = FMT_CLASSIFY; auto_dir_brackets = false; break;
            case LOI_NO_COLOR:              flagsOFF = FMT_COLORS; break;
            case LOI_NO_COLOR_SCALE:        SetColorScale(L"none"); break;
            case LOI_DIGIT_SORT:            SetDefaultNumericSort(false); break;
            case LOI_NO_FAT:                flagsOFF = FMT_FAT; break;
            case LOI_FIT_COLUMNS:           SetCanAutoFit(true); break;
            case LOI_NO_FIT_COLUMNS:        SetCanAutoFit(false); break;
            case LOI_NO_FULL_PATHS:         flagsOFF = FMT_FULLNAME|FMT_FORCENONFAT|FMT_HIDEPSEUDODIRS; break;
            case LOI_GIT:                   flagsON = FMT_GIT; break;
            case LOI_NO_GIT:                flagsOFF = FMT_GIT|FMT_GITREPOS; break;
            case LOI_GIT_IGNORE:            flagsON = FMT_GITIGNORE; break;
            case LOI_NO_GIT_IGNORE:         flagsOFF = FMT_GITIGNORE; break;
            case LOI_GIT_REPOS:             flagsON = FMT_GIT|FMT_GITREPOS; break;
            case LOI_GIT_REPOS_NO_STATUS:   flagsOFF = FMT_GITREPOS; break;
            case LOI_HIDE_DOT_FILES:        HideDotFiles(true); break;
            case LOI_NO_HIDE_DOT_FILES:     HideDotFiles(false); break;
            case LOI_HORIZONTAL:            flagsOFF = FMT_SORTVERTICAL; break;
            case LOI_HYPERLINKS:            flagsON = FMT_HYPERLINKS; break;
            case LOI_NO_HYPERLINKS:         flagsOFF = FMT_HYPERLINKS; break;
            case LOI_NO_ICONS:              SetUseIcons(L"never"); break;
            case LOI_LOWER:                 flagsON = FMT_LOWERCASE; break;
            case LOI_NO_LOWER:              flagsOFF = FMT_LOWERCASE; break;
            case LOI_MINI_BYTES:            SetMiniBytes(true); break;
            case LOI_NO_MINI_BYTES:         SetMiniBytes(false); break;
            case LOI_MINI_DECIMAL:          flagsON = FMT_MINIDECIMAL; break;
            case LOI_NO_MINI_DECIMAL:       flagsOFF = FMT_MINIDECIMAL; break;
            case LOI_MINI_HEADER:           flagsON = FMT_MINIHEADER; break;
            case LOI_NO_MINI_HEADER:        flagsOFF = FMT_MINIHEADER; break;
            case LOI_MORE_COLORS:           more_colors = opt_value; break;
            case LOI_NERD_FONTS_VER:        SetNerdFontsVersion(wcstoul(opt_value, nullptr, 10)); break;
            case LOI_NO_NORMAL:             flagsOFF = FMT_FORCENONFAT; break;
            case LOI_NUMERIC_SORT:          SetDefaultNumericSort(true); break;
            case LOI_NO_OWNER:              flagsOFF = FMT_SHOWOWNER; break;
            case LOI_PAD_ICONS:             SetPadIcons(wcstoul(opt_value, nullptr, 10)); break;
            case LOI_NO_RATIO:              flagsOFF = FMT_COMPRESSED; break;
            case LOI_REVERSE:               SetReverseSort(true); break;
            case LOI_NO_REVERSE:            SetReverseSort(false); break;
            case LOI_SIZE:                  flagsON = FMT_SIZE; break;
            case LOI_NO_SIZE:               flagsON = FMT_LONGNOSIZE; break;
            case LOI_NO_SHORT_NAMES:        flagsOFF = FMT_SHORTNAMES; break;
            case LOI_NO_STREAMS:            flagsOFF = FMT_ALTDATASTEAMS|FMT_FORCENONFAT; break;
            case LOI_STRING_SORT:           SetStringSort(true); break;
            case LOI_TIME:                  flagsON = FMT_DATE; break;
            case LOI_NO_TIME:               flagsON = FMT_LONGNODATE; break;
            case LOI_WORD_SORT:             SetStringSort(false); break;
            }
            break;
        }

        if (flagsON || flagsOFF)
        {
            if (!flagsOFF)
                flagsOFF = flagsON;

            if (*opt_value == '+' || (long_opt && flagsON))
                SetFlag(flags, flagsON);
            else if (*opt_value == '-' || (long_opt && flagsOFF))
                ClearFlag(flags, flagsOFF);
            else
                assert(false);

            // Explicitly choosing --mini-header makes it always appear,
            // regardless how many directory patterns are present.
            if ((flagsON|flagsOFF) & FMT_MINIHEADER)
                flags &= ~FMT_MAYBEMINIHEADER;
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
                ClearFlag(flags, FMT_SIZE|FMT_MINISIZE);
                filesize = FILESIZE_FILESIZE;
                continue;
            }
            SetFlag(flags, FMT_SIZE);
            if (wcscmp(opt_value, L"S") == 0)
            {
                SetFlag(flags, FMT_FULLSIZE);
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
                ClearFlag(flags, (FMT_DATE|FMT_MINIDATE));
                timestamp = TIMESTAMP_MODIFIED;
                continue;
            }
            SetFlag(flags, FMT_DATE);
            if (wcscmp(opt_value, L"T") == 0)
            {
                SetFlag(flags, FMT_FULLTIME);
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
                ClearFlag(flags, FMT_MINIDATE);
            else
                SetFlag(flags, FMT_MINIDATE|FMT_DATE);
            break;
        case 'Z':
            if (wcscmp(opt_value, L"-") == 0)
                ClearFlag(flags, FMT_MINISIZE);
            else
                SetFlag(flags, FMT_MINISIZE|FMT_SIZE);
            break;
        }
    }

    for (unsigned ii = 0; opt_value = opts.GetValue('o', ii); ii++)
    {
        SetSortOrder(opt_value, e);
        if (e.Test())
            return e.Report();
    }

    if (flags & FMT_TREE)
    {
        ClearFlag(flags, FMT_BARE|FMT_FULLNAME|FMT_FAT|FMT_JUSTIFY_FAT|FMT_JUSTIFY_NONFAT);
        if (!dwAttrIncludeAny && !dwAttrMatch && !dwAttrExcludeAny)
            ClearFlag(flags, FMT_SKIPHIDDENDIRS);
    }
    else if (flags & FMT_BARERELATIVE)
    {
        SetFlag(flags, FMT_BARE);
    }

    unsigned cColumns = 1;
    if (g_nix_defaults)
        cColumns = 0;

    // Need to always evaluate these, so that -l implies showing attributes.
    {
        bool long_attributes = false;
        for (unsigned ii = 0; opts.GetValue(ii, ch, opt_value); ii++)
        {
            switch (ch)
            {
            case 'l':
            case '<':
            case '1':
                if (*opt_value != '-' && ch != '<')
                {
                    cColumns = 1;
                    if (ch == 'l' && !(flags & FMT_LONGNOATTRIBUTES))
                        long_attributes = true;
                }
                else if (cColumns == 1)
                {
                    cColumns = 0;
                    long_attributes = false;
                }
                break;
            case '2':
                long_attributes = false;
                if (!picture)
                {
                    if (*opt_value != '-')
                        cColumns = 2;
                    else if (cColumns == 2)
                        cColumns = 1;
                }
                break;
            case '4':
                long_attributes = false;
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
            case '>':
                long_attributes = false;
                if (*opt_value != '-' && ch != '>')
                    cColumns = 0;
                else if (cColumns == 0)
                    cColumns = 1;
                break;
            }
        }

        if (long_attributes)
            SetFlag(flags, FMT_ATTRIBUTES);
        else if (flags & FMT_TREE)
        {
            if (!(flags & (FMT_DATE|FMT_MINIDATE)))
                SetFlag(flags, FMT_LONGNODATE);
            if (!(flags & (FMT_SIZE|FMT_MINISIZE)))
                SetFlag(flags, FMT_LONGNOSIZE);
        }
    }

    if (flags & (FMT_BARE|FMT_TREE|FMT_FULLNAME|FMT_FULLTIME|FMT_COMPRESSED|FMT_SHOWOWNER|FMT_ONLYALTDATASTREAMS|FMT_ALTDATASTEAMS))
        cColumns = 1;

    if (cColumns > 1 && (flags & FMT_ATTRIBUTES))
        cColumns = 1;

    if (flags & FMT_FORCENONFAT)
        ClearFlag(flags, FMT_FAT);
    if (auto_dir_brackets)
        FlipFlag(flags, FMT_DIRBRACKETS, (cColumns != 1 && !g_nix_defaults && !(flags & (FMT_FAT|FMT_ATTRIBUTES|FMT_MINISIZE|FMT_CLASSIFY))));
    if (!(flags & FMT_FAT))
        SetFlag(flags, FMT_FULLSIZE);
    if (flags & (FMT_BARE|FMT_TREE))
    {
        SetFlag(flags, FMT_HIDEPSEUDODIRS);
        ClearFlag(flags, FMT_ALTDATASTEAMS|FMT_JUSTIFY_FAT|FMT_JUSTIFY_NONFAT);
        if (flags & FMT_TREE)
            SetFlag(flags, FMT_NOVOLUMEINFO|FMT_NOSUMMARY);
        else
            SetUseIcons(L"never", true/*unless_always*/);
    }

    if (flags & FMT_SEPARATETHOUSANDS)
        SetFlag(flags, FMT_FULLSIZE);
    if (flags & FMT_AUTOSEPTHOUSANDS)
        SetFlag(flags, FMT_SEPARATETHOUSANDS);

    if (flags & FMT_USAGE)
    {
        cColumns = 1;
        if (flags & FMT_SUBDIRECTORIES)
            SetFlag(flags, FMT_USAGEGROUPED);
        ClearFlag(flags, FMT_COLORS|FMT_MINISIZE|FMT_LOWERCASE|FMT_FULLSIZE|FMT_COMPRESSED|
                         FMT_SEPARATETHOUSANDS|FMT_REDIRECTED|FMT_AUTOSEPTHOUSANDS|
                         FMT_USAGE|FMT_USAGEGROUPED|FMT_MINIDATE|FMT_MINIDECIMAL);
        SetFlag(flags, FMT_BARE|FMT_SUBDIRECTORIES|FMT_HIDEPSEUDODIRS);
        limit_depth = -1;
    }

    if (cColumns != 1)
        ClearFlag(flags, FMT_FULLSIZE|FMT_GITREPOS);
    if (cColumns > 2)
        ClearFlag(flags, FMT_GIT);

    if ((flags & FMT_ATTRIBUTES) && show_all_attributes)
        SetFlag(flags, FMT_ALLATTRIBUTES);

    // Initialize directory entry formatter.

    if (opts['p'])
        SetPagination(true);

    if (!CanUseEscapeCodes(GetStdHandle(STD_OUTPUT_HANDLE)))
    {
        ClearFlag(flags, FMT_COLORS);
        SetUseIcons(L"never", true/*unless_always*/);
        SetColorScale(L"none");
    }

    DirEntryFormatter def;
    def.SetFitColumnsToContents(g_nix_defaults || used_B_flag);
    def.Initialize(cColumns, flags, timestamp, filesize, dwAttrIncludeAny, dwAttrMatch, dwAttrExcludeAny, picture);

    if (g_debug)
    {
        if (limit_depth != unsigned(-1))
            Printf(L"debug: levels: %u\n", limit_depth);
    }

    if (def.Settings().IsSet(FMT_COLORS))
    {
        assert(!e.Test());
        InitColors(more_colors);
    }

    // Finally, print icons, if so requested.

    if (print_all_icons)
    {
        SetUseIcons(L"always");
        PrintAllIcons();
        SetGracefulExit();
        return 0;
    }

    // Determine path(s) to scan.

    DirPattern* patterns = MakePatterns(argc, argv, def.Settings(), ignore_globs.Text(), e);
    if (e.Test())
        return e.Report();

    if (g_debug)
    {
        StrW cwd;
        GetCwd(cwd);
        Printf(L"debug: cwd=%s\n", cwd.Text());

        int ii = 0;
        for (const auto* p = patterns; p; p = p->m_next, ++ii)
        {
            Printf(L"debug: pattern %u; dir '%s', fat %u, implicit %u, depth %u\n", ii, p->m_dir.Text(), p->m_isFAT, p->m_implicit, p->m_depth);
            if (def.Settings().IsSet(FMT_BARERELATIVE))
                Printf(L"debug: pattern %u; dir_rel '%s'\n", ii, p->m_dir_rel.Text());
            Printf(L"debug: pattern %u; patterns", ii);
            for (unsigned jj = 0; jj < p->m_patterns.size(); ++jj)
            {
                if (jj > 0)
                    Printf(L",");
                Printf(L" %s", p->m_patterns[jj].Text());
            }
            Printf(L"\n");
        }
    }

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
        (g_nix_defaults || def.Settings().IsSet(FMT_MAYBEMINIHEADER)) &&
        !def.Settings().IsSet(FMT_SUBDIRECTORIES) &&
        (!patterns || !patterns->m_next))
    {
        // Suppress the miniheader if there's only 0 or 1 directory patterns.
        def.Settings().m_flags &= ~FMT_MINIHEADER;
    }

    if (def.Settings().IsSet(FMT_BARERELATIVE) &&
        !def.Settings().IsSet(FMT_TREE))
    {
        // Eventually it would be nice to show the original input pattern.
        // But that's a little complicated, so for now use a simplified
        // approach:  Only show relative paths in bare mode if there were 0 or
        // 1 directory patterns and the current directory is a prefix.
        bool no = (patterns && patterns->m_next);
        if (!no)
        {
            StrW cwd;
            GetCwd(cwd);
            no = (wcsnicmp(cwd.Text(), patterns[0].m_dir.Text(), cwd.Length()) != 0);
        }
        if (no)
            def.Settings().m_flags &= ~FMT_BARERELATIVE;
    }

    const int rc = ScanDir(def, patterns, limit_depth, e);

    def.Finalize();

    if (e.Test())
        return e.Report();

    SetGracefulExit();
    return rc;
}

