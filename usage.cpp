// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "usage.h"

#include <vector>
#include <map>
#include <algorithm>

const char c_usage[] = "%s -? for help.";

enum FlagSection
{
    USAGE,
    DISPLAY,
    FILTER,
    FIELD,
    FORMAT,
    MAX
};

struct FlagUsageInfo
{
    FlagSection         section;
    const char*         flag;
    const char*         desc;
};

static const FlagUsageInfo c_usage_args =
{
    USAGE,
    "[drive:][path][filename]",
    "Specifies drive, directory, and/or files to list.  Prefix the filename part with :: to use a regular expression.  "
    "For more information about regular expressions use '-?\030regex'.\n"
};

static const FlagUsageInfo c_usage_info[] =
{
    // USAGE -----------------------------------------------------------------
    { USAGE,    "-?, --help",               "Display this help text.\n" },
    { USAGE,    "-? alphabetical",          "Display this help text in alphabetical order.\n" },
    { USAGE,    "-? colors",                "Display help text on color coding the file list.\n" },
    { USAGE,    "-? colorsamples",          "Display samples of the supported color codes.\n" },
    { USAGE,    "-? defaultcolors",         "Print the default DIRX_COLORS string.\n" },
    { USAGE,    "-? icons",                 "Display help text on file icons and Nerd Fonts.\n" },
    { USAGE,    "-? pictures",              "Display help text on format pictures.\n" },
    { USAGE,    "-? printallicons",         "Print a list of all icons.\n" },
    { USAGE,    "-? regex",                 "Display help text on regular expression syntax.\n" },
    { USAGE,    "-V, --version",            "Display version information.\n" },

    // DISPLAY OPTIONS -------------------------------------------------------
    { DISPLAY,  "-1",                       "Display one column per line.\n" },
    { DISPLAY,  "-2",                       "Display two columns per line (more in wide consoles).\n" },
    { DISPLAY,  "-4",                       "Display four columns per line (more in wide consoles).\n" },
    { DISPLAY,  "-a, --all",                "Display all files (include hidden and system files).\n" },
    { DISPLAY,  "-b, --bare",               "Bare mode; only display names, no header/detail/etc.\n" },
    { DISPLAY,  "-B, --almost-bare",        "Selects default options for a concise almost-bare view.  Displays the name "
                                            "field in one column per line, hides . and .. directories, shows a mini "
                                            "directory header above file lists, quashes volume/header/summary output, "
                                            "disables filename justify, shows long file names, and shows '-' as size "
                                            "for dirs.  If other flags are used to add more fields per file entry, then "
                                            "field widths are fitted to their contents.\n" },
    { DISPLAY,  "-c, --color",              "Display with colors (use '-?\030colors' for more info).\n" },
    { DISPLAY,  "-g, --git",                "List each file's git status.\n" },
    { DISPLAY,  "-gg, --git-repos",         "List status of git repo roots and each file's git status (or "
                                            "--git-repos-no-status to omit file status).\n" },
    { DISPLAY,  "-G, --grid",               "Synonym for --wide.\n" },
    { DISPLAY,  "-i, --icons[=WHEN]",       "Display file icons (use '-?\030icons' for more info).\n"
                                            "  always, auto, never (default)\n" },
    { DISPLAY,  "-k, --color-scale[=FIELD]",
                                            "Highlight levels of certain fields distinctly.\n"
                                            "  all, age, size, none (default)\n" },
    { DISPLAY,  "-l, --long",               "Long mode; display one file per line, plus attributes.\n" },
    { DISPLAY,  "-n, --normal",             "Force normal list format even on FAT volumes.\n" },
    { DISPLAY,  "-Q, --quash[=TYPES]",      "Quash types of output.  Use -Q by itself as a synonym for -Q+v+h+s.\n"
                                            "  v  Suppress the volume information\n"
                                            "  h  Suppress the header\n"
                                            "  s  Suppress the summary\n"
                                            "  -  Prefix to suppress next type (the default)\n"
                                            "  +  Prefix to un-suppress next type\n" },
    { DISPLAY,  "-p, --paginate",           "Pause after each screen full of information.\n" },
    { DISPLAY,  "-R",                       "Synonym for --recurse.\n" },
    { DISPLAY,  "-s, --recurse",            "Subdirectories; recursively display files in specified "
                                            "directory and all subdirectories.\n" },
    { DISPLAY,  "-u, --usage",              "Display directory size usage data.\n" },
    { DISPLAY,  "-v, --vertical",           "Sort columns vertically.\n" },
    { DISPLAY,  "    --horizontal",         "Sort columns horizontally (the default).\n" },
    { DISPLAY,  "-w, --wide",               "Wide mode; show as many columns as fit.\n" },
    { DISPLAY,  "-z, --fat",                "Force FAT list format even on non-FAT volumes.\n" },
    { DISPLAY,  "--color-scale-mode=MODE",  "Mode for --color-scale (use '-?\030colors' for more info).\n"
                                            "  fixed, gradient (default)\n" },
    { DISPLAY,  "--hyperlinks",             "Display entries as hyperlinks.\n" },
    { DISPLAY,  "--tree",                   "Tree mode; recursively display files and directories in a tree layout.\n" },

    // FILTERING AND SORTING OPTIONS -----------------------------------------
    { FILTER,   "-a[...]",                  "Display files with the specified attributes.  If attributes are combined, "
                                            "all attributes must match (-arhs only lists files with all three "
                                            "attributes set).  The - prefix excludes files with that attribute (-arh-s "
                                            "lists files that are read-only and hidden and not system).  The + prefix "
                                            "includes files that have any of the + attributes set (-ar+h+s lists files "
                                            "that are read-only and are hidden or system).\n"
                                            "  r  Read-only files            e  Encrypted files\n"
                                            "  h  Hidden files               t  Temporary files\n"
                                            "  s  System files               p  Sparse files\n"
                                            "  a  Ready for archiving        c  Compressed files\n"
                                            "  d  Directories                o  Offline files\n"
                                            "  i  Not content indexed files\n"
                                            "  j  Reparse points (mnemonic for junction)\n"
                                            "  l  Reparse points (mnemonic for link)\n"
                                            "  +  Prefix meaning any\n"
                                            "  -  Prefix meaning not\n" },
    { FILTER,   "-A, --almost-all",         "Display all files, except hide . and .. directories.\n" },
    { FILTER,   "-h",                       "Hide . and .. directories.\n" },
    { FILTER,   "-I, --ignore-glob=GLOB",   "Glob patterns of files to ignore; the syntax is the same as in "
                                            ".gitignore.  The / is used as the directory separator.  An optional ! "
                                            "prefix negates a pattern; any matching file excluded by a previous pattern "
                                            "will be included again.  Multiple patterns may be specified separated by a "
                                            "; or | character.\n" },
    { FILTER,   "-L, --levels=DEPTH",       "Limit the depth of recursion with -s.\n" },
    { FILTER,   "-o[...]",                  "Sort the list by the specified options:\n"
                                            "  n  Name [and extension if 'e' omitted] (alphabetic)\n"
                                            "  e  Extension (alphabetic)\n"
                                            "  g  Group directories first\n"
                                            "  d  Date/time (oldest first)\n"
                                            "  s  Size (smallest first)\n"
                                            "  c  Compression ratio\n"
                                            "  a  ASCII order for digits (sort \"10\" before \"2\")\n"
                                            "  u  Unsorted\n"
                                            "  r  Reverse order for all options\n"
                                            "  -  Prefix to reverse order\n" },
    { FILTER,   "-X, --skip=TYPES",         "Skip types during -s.  Use -X by itself as a synonym for -X+d+j+r.\n"
                                            "  d  Skip hidden directories (when used with -s)\n"
                                            "  j  Skip junctions (when used with -s)\n"
                                            "  r  Skip files with no alternate data streams\n"
                                            "  -  Prefix to skip next type (this is the default)\n"
                                            "  +  Prefix to un-skip next type\n" },
    { FILTER,   "--digit-sort",             "Sort in ASCII order for digits (\"10\" before \"2\").\n" },
    { FILTER,   "--git-ignore",             "Ignore files mentioned in .gitignore files.\n" },
    { FILTER,   "--hide-dot-files",         "Hide file and directory names starting with '.' or '_'.  Using -a "
                                            "overrides this and shows them anyway.\n" },
    { FILTER,   "--numeric-sort",           "Sort in numeric order (\"2\" before \"10\") (default).\n" },
    { FILTER,   "--reverse",                "Reverse the selected sort order.\n" },
    { FILTER,   "--string-sort",            "Sort punctuation as symbols.\n" },
    { FILTER,   "--word-sort",              "Sort punctuation as part of the word (default).\n" },

    // FIELD OPTIONS ---------------------------------------------------------
    { FIELD,    "-C, --ratio",              "List the compression ratio.\n" },
    { FIELD,    "-q, --owner",              "List the owner of the file.\n" },
    { FIELD,    "-r, --streams",            "List alternate data streams of the file.\n" },
    { FIELD,    "-S, --size",               "List the file size even in multple column formats.\n" },
    { FIELD,    "-S[acf], --size=acf",      "Which size field to display or use for sorting:\n"
                                            "  a  Allocation size\n"
                                            "  c  Compressed size\n"
                                            "  f  File size (default)\n" },
    { FIELD,    "-t, --attributes",         "List the file attributes (use the flag twice to list all attributes, e.g. "
                                            "-tt).\n" },
    { FIELD,    "-T, --time",               "List the file time even in multiple column formats.\n" },
    { FIELD,    "-T[acw], --time=acw",      "Which time field to display or use for sorting:\n"
                                            "  a  Access time\n"
                                            "  c  Creation time\n"
                                            "  w  Write time (default)\n" },
    { FIELD,    "-x, --short-names",        "Show 8.3 short file names.\n" },

    // FORMATTING OPTIONS ----------------------------------------------------
    { FORMAT,   "-,",                       "Show the thousand separator in sizes (the default).\n" },
    { FORMAT,   "-f[...]",                  "Use the specified format picture.  You can greatly customize how the list "
                                            "is displayed (use '-?\030pictures' for more info).\n" },
    { FORMAT,   "-F, --full-paths",         "Show full file paths in the file name column.\n" },
    { FORMAT,   "-j",                       "Justify file names in FAT list format.\n" },
    { FORMAT,   "-J",                       "Justify file names in non-FAT list formats.\n" },
    { FORMAT,   "--justify[=WHEN]",         "Justify file names, in which list formats.  If WHEN is omitted, 'always' "
                                            "is assumed.\n"
                                            "  always, fat, normal, never (default)\n" },
    { FORMAT,   "-SS",                      "Show long file sizes (implies -S).  Note that some list formats limit the "
                                            "file size width.\n" },
    { FORMAT,   "-TT",                      "Show long dates and times (implies -T).  Note that some list formats limit "
                                            "the date and time width.\n" },
    { FORMAT,   "-W, --width=COLS",         "Override the screen width.\n" },
    { FORMAT,   "-Y",                       "Abbreviate dates and times (implies -T).\n" },
    { FORMAT,   "-Z",                       "Abbreviate file sizes as 1K, 15M, etc (implies -S).\n" },
    { FORMAT,   "--bare-relative",          "When listing subdirectories recursively, print paths relative to the "
                                            "specified patterns instead of expanding them to fully qualified paths "
                                            "(implies --bare).\n" },
    { FORMAT,   "--classify",               "Print '\\' by dir names and '@' by symlink names.\n" },
    { FORMAT,   "--compact",                "Use compact time format (short for --time and --time-style=compact).\n" },
    { FORMAT,   "--escape-codes[=WHEN]",    "For colors and hyperlinks in modern terminals.\n"
                                            "  always, auto (default), never\n" },
    { FORMAT,   "--fit-columns",            "Fit more columns in -w mode by compacting column widths to fit their "
                                            "content (this is the default; use --no-fit-columns to disable it).\n" },
    { FORMAT,   "--lower",                  "Show file names using lower case.\n" },
    { FORMAT,   "--mini-bytes",             "Show bytes in the mini size format when less than 1000.\n" },
    { FORMAT,   "--mini-decimal",           "Always show one decimal place in the mini size format.\n" },
    { FORMAT,   "--mini-header",            "Show a mini header of just the directory name above each directory listing "
                                            "(if more than one directory).\n" },
    { FORMAT,   "--more-colors=LIST",       "Add color rules in the same format as the DIRX_COLORS environment variable "
                                            "(use '-?\030colors' for more info).\n" },
    { FORMAT,   "--nerd-fonts=VER",         "Select which Nerd Fonts version to use (see '-?\030colors' for more info).\n" },
    { FORMAT,   "--nix",                    "Selects default options that are similar to Unix and Linux systems.  Hides "
                                            "files starting with '.', skips recursing into hidden directories, sorts "
                                            "vertically, displays the file list in wide mode, fits field widths to "
                                            "their contents, selects 'compact' time style, shows a mini directory "
                                            "header above file lists, quashes volume/header/summary output, disables "
                                            "filename justify, shows long file names, shows '-' as size for dirs, and "
                                            "suppresses thousands separators.\n" },
    { FORMAT,   "--pad-icons=SPACES",       "Number of spaces to print after an icon.\n" },
    { FORMAT,   "--relative",               "Use relative time format (short for --time and --time-style=relative).\n" },
    { FORMAT,   "--size-style=STYLE",       "Which size format to use for display by default when not overridden by "
                                            "other format options:\n"
                                            "  mini, short, normal (default)\n" },
    { FORMAT,   "--time-style=STYLE",       "Which time format to use for display by default when not overridden by "
                                            "other format options:\n"
                                            "  locale (default), mini, compact, short, normal, full, iso, long-iso, relative\n" },
    { FORMAT,   "--truncate-char=HEX",      "Set the truncation character for file names that don't fit in the allotted "
                                            "space.  Specify a Unicode character value in hexadecimal (e.g. 2192 is a "
                                            "right-pointing arrow and 25b8 is a right-pointing triangle).  Or specify "
                                            "002e to use .. (two periods).\n" },
    { FORMAT,   "--utf8",                   "When output is redirected, produce UTF8 output instead of using the system "
                                            "codepage.\n" },
};

static const char c_usage_prolog[] =
"Displays a list of file and subdirectories in a directory.\n"
"\n"
"%s [options] [drive:][path][filename]\n"
"\n"
;

static const char c_usage_epilog[] =
"\n"
"Long options that can be used without an argument also accept a 'no-' prefix to disable them.  For example, the "
"--fit-columns option is enabled by default, and using --no-fit-columns disables it.\n"
"\n"
"Environment variables:\n"
"\n"
"Set DIRXCMD to preset options to use by default.  To override any on/off option, add a hyphen (-k- overrides -k).  Or "
"for long options insert \"no-\" (--no-attributes overrides --attributes).  For interop compatibility with the CMD DIR "
"command, slash options may also be used, for example /w.  Unlike the CMD DIR command, use /w- instead of /-w to "
"override the slash option.\n"
"\n"
"Set DIRX_COLORS to specify colors to use in the file list display.  Use '-?\030colors' for more info on color coding rules.\n"
;

static unsigned s_flag_col_width = 24;
static void AppendFlagUsage(StrA& u, const FlagUsageInfo& info, bool skip_leading_spaces=false)
{
    const char* flag = info.flag;
    if (skip_leading_spaces)
    {
        while (*flag == ' ')
            ++flag;
    }

    unsigned flag_len = 2 + unsigned(strlen(flag));
    u.Append("  ");
    u.Append(flag);
    if (flag_len + 2 > s_flag_col_width)
    {
        u.Append("\n");
        flag_len = 0;
    }
    u.AppendSpaces(s_flag_col_width - flag_len);
    u.Append("\032");

    const char* p = info.desc;
    while (*p)
    {
        const char* const end = strchr(p, '\n');
        size_t len = end ? (end - p) : strlen(p);
        u.Append(p, len);
        u.Append("\n");
        len += !!end;
        p += len;
        if (*p)
            u.AppendSpaces(s_flag_col_width);
    }
}

static int CmpFlagChar(char a, char b)
{
    static bool s_init = true;
    static int s_order[256];
    if (s_init)
    {
        static char c_ordered[] =
        "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
        "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
        ", "    // So "-?," sorts before "-? foo".
        "!\"#$%&'()*+./"
        "0123456789"
        ":;<=>?@"
        "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ"
        "[\\]^_`"
        "{|}~"
        "-"
        "\x7f"
        ;
        static_assert(sizeof(c_ordered) == _countof(s_order) / 2 + 1, "wrong number of characters in c_ordered");
        for (int i = 0; i < sizeof(c_ordered) - 1; ++i)
            s_order[c_ordered[i]] = i;
        for (int i = 128; i < 256; ++i)
            s_order[i] = i;
        s_init = false;
    }
    return (s_order[a] - s_order[b]);
}

static bool CmpFlagName(size_t a, size_t b)
{
    const auto& a_info = c_usage_info[a];
    const auto& b_info = c_usage_info[b];
    const char* a_str = a_info.flag;
    const char* b_str = b_info.flag;

#ifndef KEEP_ASSOCIATED_TOGETHER
    while (*a_str == ' ') ++a_str;
    while (*b_str == ' ') ++b_str;
#endif

    const int a_long = (a_str[0] && a_str[1] == '-');
    const int b_long = (b_str[0] && b_str[1] == '-');

    int n = a_long - b_long;
    if (n)
        return (n < 0);

    while (true)
    {
        const unsigned a_c = *a_str;
        const unsigned b_c = *b_str;
        n = CmpFlagChar(a_c, b_c);
        if (n)
            return (n < 0);
        if (!a_c || !b_c)
        {
            assert(!a_c && !b_c);
            return false;
        }
        ++a_str;
        ++b_str;
    }
}

StrA MakeUsageString(bool alphabetical, unsigned flag_col_width)
{
    s_flag_col_width = flag_col_width;

    StrA u;
    u.Append(c_usage_prolog);
    AppendFlagUsage(u, c_usage_args);
    u.Append("\n");

    if (alphabetical)
    {
#ifdef KEEP_ASSOCIATED_TOGETHER
        std::map<size_t, size_t> associated_indices;
        const bool skip_leading_spaces = false;
#else
        const bool skip_leading_spaces = true;
#endif

        u.Append("OPTIONS:\n");

        size_t i = 0;
        std::vector<size_t> usage_indices;
        for (const auto& info : c_usage_info)
        {
            usage_indices.emplace_back(i);
#ifdef KEEP_ASSOCIATED_TOGETHER
            if (info.flag[0] == ' ')
                associated_indices[i - 1] = i;
#endif
            ++i;
        }

        std::sort(usage_indices.begin(), usage_indices.end(), CmpFlagName);

        for (size_t k = 0; k < usage_indices.size(); ++k)
        {
            const size_t i = usage_indices[k];
            const auto& info = c_usage_info[i];

#ifdef KEEP_ASSOCIATED_TOGETHER
            if (info.flag[0] == ' ')
                continue;
#endif

            AppendFlagUsage(u, info, skip_leading_spaces);

#ifdef KEEP_ASSOCIATED_TOGETHER
            for (size_t j = i;;)
            {
                const auto& assoc = associated_indices.find(j);
                if (assoc == associated_indices.end())
                    break;
                j = assoc->second;
                AppendFlagUsage(u, c_usage_info[j]);
            }
#endif
        }
    }
    else
    {
        FlagSection section = USAGE;
        for (const auto& info : c_usage_info)
        {
            if (section != info.section)
            {
                section = info.section;
                switch (section)
                {
                case DISPLAY:   u.Append("\nDISPLAY OPTIONS:\n"); break;
                case FILTER:    u.Append("\nFILTERING AND SORTING OPTIONS:\n"); break;
                case FIELD:     u.Append("\nFIELD OPTIONS:\n"); break;
                case FORMAT:    u.Append("\nFORMATTING OPTIONS:\n"); break;
                }
            }

            AppendFlagUsage(u, info);
        }
    }

    u.Append(c_usage_epilog);
    return u.Text();
}

const char c_help_colors[] =
"Set the DIRX_COLORS environment variable to control how files and directories are colored.  The format is:\n"
"\n"
"  condition=color: condition=color: ...\n"
"\n"
"Any number of pairs of conditions and colors can be given.  Use colons to separate them and build a list of many "
"coloring rules.\n"
"\n"
"The list can start with the special keyword \"reset\" to clear all default color rules and any rules that may have "
"been provided in the LS_COLORS environment variable.\n"
"\n"
"Examples:\n"
"  sc=93:bu=93;4:cm=38;5;172:\n"
"          Source code files in yellow, build system files in yellow with underline, compiled files in orange.\n"
"  ro=32:ex=1:ro ex=1;31:ln=target:\n"
"          Read-only files in green, executable files in bright white, files that are both read-only and executable in "
                "bright green, and symlinks colored by the type of the target of the symlink.\n"
"\n"
"CONDITION syntax:\n"
"\n"
"Each condition can specify two-letter types, or patterns to match.  If more than one are specified then all of them "
"must match, and rules with multiple types and/or patterns are evaluated in the order they are listed.\n"
"\n"
"Negation:\n"
"  not     \032Negates the next type or pattern (not hi means not hidden)\n"
"  !       \032Negates the next type or pattern (!hi means not hidden)\n"
"\n"
"\001"
"File attribute names:\n"
"  ar  archive             hi  hidden              ro  read-only\n"
"  cT  compressed attr     ln  symlink             SP  sparse\n"
"  di  directory           NI  not indexed         sy  system\n"
"  en  encrypted           of  offline             tT  temporary attr\n"
"  fi  file                or  orphaned symlink\n"
"\002"
"\n"
"  NOTE:   \032Setting ro by itself applies only to files, not directories.\n"
"          Setting ln=target or ln=: colors symlinks by the type of the target of the symlink.\n"
"          Setting hi=50%% colors hidden files and directories at 50%% luminance of whatever color the file or "
                "directory would have had.  Specify any percentage between 30%% and 90%%.  This can only be used with the "
                "\"hi\" attribute name.\n"
"\n"
"File groups:\n"
"  ex  executable          mu  music               cm  compiled\n"
"  do  document            lo  lossless music      bu  build\n"
"  im  image               cR  compressed type     sc  source code\n"
"  vi  video               tX  temporary type      cr  crypto\n"
"\n"
"Special groups:\n"
"  co  compressed (includes both cR and cT)\n"
"  tm  temporary (includes both tX and tT)\n"
"\n"
"Color element names:\n"
"  sn  file size (sets nb, nk, nm, ng and nt)\n"
"  nb  file size if less than 1 KB\n"
"  nk  file size if between 1 KB and 1 MB\n"
"  nm  file size if between 1 MB and 1 GB\n"
"  ng  file size if between 1 GB and 1 TB\n"
"  nt  file size if 1 TB or greater\n"
"  sb  units of a file’s size (sets ub, uk, um, ug and ut)\n"
"  ub  units of a file’s size if less than 1 KB\n"
"  uk  units of a file’s size if between 1 KB and 1 MB\n"
"  um  units of a file’s size if between 1 MB and 1 GB\n"
"  ug  units of a file’s size if between 1 GB and 1 TB\n"
"  ut  units of a file’s size if 1 TB or greater\n"
"  da  file date and time\n"
"  cF  file compression ratio\n"
"  oF  file owner\n"
"  ga  new file in git\n"
"  gm  modified file in git\n"
"  gd  deleted file in git\n"
"  gv  renamed file in git\n"
"  gt  type change in git\n"
"  gi  ignored file in git\n"
"  gc  unmerged file in git (conflicted)\n"
"  Gm  git repo main branch name\n"
"  Go  git repo other branch name\n"
"  Gc  git repo is clean\n"
"  Gd  git repo is dirty\n"
"  GO  overlay style for dirty git repo\n"
"  xx  punctuation (for example _ in the attributes field)\n"
"  hM  the directory name with --mini-header\n"
"  ur  the read-only attribute letter in the attributes field\n"
"  su  the hidden attribute letter in the attributes field\n"
"  sf  the system attribute letter in the attributes field\n"
"  pi  the junction attribute letter in the attributes field\n"
"  lp  the symlink path when showing symlink targets\n"
"  bO  overlay style for broken symlink paths when showing symlink targets\n"
"\n"
"  NOTE:   \032The color element names above cannot be combined with types or patterns; they're for setting general "
          "colors, and don't affect how file names are colored.\n"
"\n"
"Anything else is interpreted as a pattern to match against the file name.  The patterns use the fnmatch glob syntax, "
"which is also used by .gitignore.\n"
"\n"
"The most common use is to match a file extension, for example \"*.txt\" matches files whose name ends with \".txt\".\n"
"\n"
"Patterns can include ? and * wildcards, and can also include character sets enclosed in [].  For example \"ab[xyz]\" "
"matches abx, aby, and abz.  A set can include ranges, for example \"[a-z]\" matches any letter.  If the first "
"character in the set is ! or ^ the set excludes the listed characters, for example \"[!0-9]\" matches anything except "
"digits.  The ? character in a set matches any character but only if there's a character to match, for example "
"\"ab[?]\" matches abc, but not ab.  Character sets can use [:class:] to specify a class of characters; 'class' can be "
"one of alnum, alpha, blank, cntrl, digit, graph, lower, print, punct, space, xdigit, upper.\n"
"\n"
"Anything quoted is treated as a pattern, for example \"ro\" refers to a file named \"ro\" instead of read-only files.\n"
"\n"
"A pattern by itself with no types applies only to files, not directories.  To specify a pattern that matches anything, "
"combine di and the pattern.\n"
"\n"
"COLOR syntax:\n"
"\n"
"Colors are the SGR parameter to ANSI color escape codes.\n"
"https://en.wikipedia.org/wiki/ANSI_escape_code\n"
"\n"
"The following color parameters are allowed.  Note that the color parameters are just sent to the terminal, and it is "
"the terminal's responsibility to draw them properly.  Some parameters might not work in all terminal programs.\n"
"\n"
"Styles:\n"
"  1     bold (bright)           22    not bold and not faint\n"
"  2     faint\n"
"  3     italic                  23    not italic\n"
"  4     underline               24    not underline (or double underline)\n"
"  7     reverse                 27    not reverse\n"
"  9     strikethrough           29    not strikethrough\n"
"  21    double underline        \n"
"  53    overline (line above)   55    not overline\n"
"\n"
"Text colors -- or add 10 to make it a background color:\n"
"  39    default text color\n"
"  30    black                   90    bright black (dark gray)\n"
"  31    dark red                91    bright red\n"
"  32    dark green              92    bright green\n"
"  33    dark yellow             93    bright yellow\n"
"  34    dark blue               94    bright blue\n"
"  35    dark magenta            95    bright magenta\n"
"  36    dark cyan               96    bright cyan\n"
"  37    dark white (gray)       97    bright white\n"
"\n"
"Extended colors -- refer to the wikipedia link for details:\n"
"  38;2;n;n;n    \032Specify a 24-bit color where each n is from 0 to 255, and the order is red;green;blue.\n"
"  38;5;n        \032Specify an 8-bit color where n is from 0 to 255.\n"
"\n"
"Run \"%s -? colorsamples\" to display a chart of the 8-bit color codes and the available styles.\n"
"\n"
"Environment variables:\n"
"\n"
"Set NO_COLOR to any non-empty value to completely disable colors.\n"
"See https://no-color.org/ for more info.\n"
"\n"
"Set DIRX_COLOR_SCALE to any value accepted by the --color-scale flag to control the default behavior.  If it's not "
"set, then EZA_COLOR_SCALE and EXA_COLOR_SCALE are also checked.\n"
"\n"
"Set DIRX_COLOR_SCALE_MODE to any value accepted by the --color-scale-mode flag to control the default behavior.  If "
"it's not set, then EZA_COLOR_SCALE_MODE and EXA_COLOR_SCALE_MODE are also checked.\n"
"\n"
"Set DIRX_MIN_LUMINANCE to a value from -100 to 100 to control the range of intensity decay in the gradient color scale "
"mode.  If it's not set, then EZA_MIN_LUMINANCE and EXA_MIN_LUMINANCE are also checked.\n"
"\n"
"If the DIRX_COLORS environment variable is not set, then LS_COLORS is also checked.  DIRX enhancements are ignored "
"when parsing LS_COLORS.)\n"
"\n"
"Run '%s -? defaultcolors' to print the default DIRX_COLORS string.\n"
;

const char c_help_icons[] =
"To see icons, a Nerd Font is required; https://nerdfonts.com\n"
"By default, a Nerd Fonts v3 font is assumed.\n"
"\n"
"A few quick examples (without a Nerd Font these will look garbled):\n"
"\n"
"\001%s\002"
"\n"
"Icons are selected based on the name of the file or folder, or the extension of the file name.  The icon mappings are "
"hard-coded and aren't configurable at this time.\n"
"\n"
"Environment variables:\n"
"\n"
"Set DIRX_NERD_FONTS_VERSION=2 to select icons compatible with v2 fonts.\n"
"\n"
"Set DIRX_ICON_SPACING=n to specify the number of spaces to print after an icon.  The default is 1, but some terminals "
"or fonts may need a different number of spaces for icons to look good.  If DIRX_ICON_SPACING isn't set, then "
"EZA_ICON_SPACING and EXA_ICON_SPACING are checked as well.\n"
;

const WCHAR c_help_icons_examples[] =
L"   archive.zip        main.cpp\n"
L"   clink.lua          music.mp3\n"
L"   image.png          python.py\n"
L"   document.doc       README.md\n"
L"   Folder             script.cmd\n"
L"   LICENSE            todo.md\n"
;

const char c_help_pictures[] =
"Use the -f option to specify a custom format picture to list files.  Format picture field types and styles are case "
"sensitive:  field types are uppercase and field styles are lowercase.\n"
"\n"
"Examples:\n"
"\001"
"  \"F  Sm\"\t\tFilename  3.2M\n"
"  \"Ds  Sm Trh F18\"\t/04/09  13:45  123K r_ SomeVeryLongFilen%s\n"
"\002"
"\n"
"Format picture field types:\n"
"  F[#flx]\t\032Filename field.  By default this automatically chooses the field width and style based on the other options used.\n"
"\t\t  #\t\032A number indicating the width for the field.  If a file name is wider, it is truncated.  If a file name is narrower, it is padded with spaces to the right.\n"
"\t\t  f\tUse FAT name format.\n"
"\t\t  l\tShow the long file name.\n"
"\t\t  x\tShow the short file name.\n"
"  S[msacf]\t\032Size field.  By default this automatically chooses the field width and style based on the other options used.\n"
"\t\t  m\t\032Use mini format (use 4 characters, always abbreviate).\n"
"\t\t  s\t\032Use short format (use 9 characters, abbreviate when necessary).\n"
"\t\t  a\tShow the file allocation size.\n"
"\t\t  c\tShow the compressed file size.\n"
"\t\t  f\tShow the file data size.\n"
"  D[lmsxacw]\t\032Date and time field.  By default this automatically chooses the field width and style based on the other options used.\n"
"\t\t  l\t\032Use locale format (like CMD with extensions enabled).\n"
"\t\t  m\t\032Use mini format (11 characters with time for recent files or year for older files).\n"
"\t\t  i\t\032Use iso format (11 characters; excludes year).\n"
"\t\t  p\t\032Use compact format (12 characters with time for recent files or year for older files; includes month name).\n"
"\t\t  s\t\032Use short format (15 characters with 2 digit year).\n"
"\t\t  o\t\032Use long iso format (16 characters with 4 digit year).\n"
"\t\t  n\t\032Use normal format (17 characters with 4 digit year).\n"
"\t\t  x\t\032Use extended format (24 characters with 4 digit year, plus minutes and milliseconds).\n"
"\t\t  a\t\032Show the last access time.\n"
"\t\t  c\t\032Show the creation time.\n"
"\t\t  w\t\032Show the last write time.\n"
"  C[?]\t\t\032Compression ratio field.  By default this shows the field even if the corresponding option was not specified.\n"
"\t\t  ?\t\032Show only if the corresponding option was specified.\n"
"  O[?]\t\t\032Owner field.  By default this shows the field even if the corresponding option was not specified.\n"
"\t\t  ?\t\032Show only if the corresponding option was specified.\n"
"  X[?]\t\t\032Short name field.  By default this shows the field even if the corresponding option was not specified.\n"
"\t\t  ?\t\032Show only if the corresponding option was specified.\n"
"  T[...]\t\032Attributes field.  By default this shows the field even if the corresponding option was not specified, and shows all file attributes.  Control which attributes are listed as follows:\n"
"\001"
"\t\t  ?\tShow only if the corresponding option was specified.\n"
"\t\t  r\tRead-only                         e     Encrypted\n"
"\t\t  h\tHidden                            t     Temporary\n"
"\t\t  s\tSystem                            p     Sparse\n"
"\t\t  a\tReady for archiving               c     Compressed\n"
"\t\t  d\tDirectory                         o     Offline\n"
"\t\t  i\tNot content indexed\n"
"\t\t  j\tReparse point (mnemonic for junction)\n"
"\t\t  l\tReparse point (mnemonic for link)\n"
"\002"
"  G[?]\t\t\032Git file status field.  When in a git repo, this shows two letters corresponding to the git file status.  This field is omitted if git file status was not requested by --git or etc.\n"
"  R[?]\t\t\032Git repo field.  When listing a directory that is a git repo, this shows the status and branch name.  This field is omitted if git repo status was not requested by --git-repos or etc.\n"
"\n"
"Other characters in the format picture are output as-is.  To ensure a character is output as-is rather than being "
"treated as a field type (etc), use \\ to escape the character (e.g. \"\\F:F\" shows \"F:MyFile.doc\").\n"
"\n"
"For some field types ? suppresses the field if the corresponding option was not specified.  When a field is "
"suppressed, normally the field and the spaces immediately preceding it are omitted from the resulting format picture.  "
"For finer control over what part of the format picture is omitted, surround the text to be omitted using the [ and ] "
"characters.  Everything between the brackets will be shown or omitted as a unit, and the brackets themselves are not "
"shown (e.g. \"F [(C?) ]Sm\" shows \"File (12%%) 1.2M\" or \"File 1.2M\").\n"
"\n"
"The -F option fits the full path name into the filename field, if the field is at the end of the format picture.\n"
"\n"
"The -w option fits as many files as possible onto each line, using the custom format picture.  The -2 option is "
"incompatible with format pictures.\n"
"\n"
"The -r option forces a single column list, but fits the stream name into the filename field(s).  If the filename field "
"is at the end of the format picture then the entire stream names are shown; otherwise the stream names are truncated "
"to the width of the longest file name.\n"
;

static const char c_help_regex[] =
"Regular expressions specify a substring to find.  In addition, several special operators perform advanced searches.\n"
"\n"
"To use a regular expression as a filename pattern, use :: followed by the regular expression.  When specifying both a "
"path and a regular expression, the :: must precede the regular expression, for example \"folder\\::regex\".  If the "
"regular expression contains spaces or characters with special meaning to the shell (such as |, <, >) then enclose the "
"expression in double quotes.\n"
"\n"
"Note that regular expressions are not natively supported by the OS, so all filenames must be retrieved and compared "
"with the regular expression.  This can make using regular expressions much slower than using ? and * wildcards.\n"
"\n"
"The regular expression is the ECMAScript syntax described here:\n"
"https://learn.microsoft.com/en-us/cpp/standard-library/regular-expressions-cpp\n"
"\n"
"Here is a quick summary; for details about the full syntax available, refer to the documentation link above.\n"
"\n"
"  Operator\tDescription\n"
"  ^\t\t\032Matches the null string at the beginning of a line.\n"
"  $\t\t\032Matches the null string at the end of a line.\n"
"  .\t\t\032Matches any character.\n"
"  [set]\t\t\032Matches any characters in the set.  The - character selects a range of characters.  The expression "
                "[a-z] matches any lowercase letter from a to z.  To select the ] character, put it at the beginning of "
                "the set.  To select the - character, put it at the end of the set.  To select the ^ character, put it "
                "anywhere except immediately after the opening bracket (which selects the complement of the set; see "
                "below).\n"
"  [^set]\t\032Matches any characters not in the set (matches the complement of the set).  The syntax for a complement set "
                "is the same as the syntax for a normal set (above), except that it is preceded by a ^ character.\n"
"  [:class:]\t\032Matches any character in the class (alnum, alpha, blank, cntrl, digit, graph, lower, print, punct, space, "
                "upper, xdigit).\n"
"  (abc)\t\t\032Matches abc, and tags it as a sub-expression for retrieval.\n"
"  abc|def\t\032Matches abc or def.\n"
"  x?\t\t\032Matches 0 or 1 occurrences of the character x (or set, or tagged sub-expression).\n"
"  x+\t\t\032Matches 1 or more occurrences of the character x (or set, or tagged sub-expression).  x+? uses non-greedy repetition.\n"
"  x*\t\t\032Matches 0 or more occurrences of the character x (or set, or tagged sub-expression).  x*? uses non-greedy repitition.\n"
"  !x\t\t\032Matches anything but x.\n"
"  \\b\t\t\032Matches the null string at a word boundary (\\babc\\b matches '\030abc\030' but not abcx or xabc or xabcx).\n"
"  \\B\t\t\032Matches the null string not at a word boundary (\\Babc matches xabc but not '\030abc').\n"
"  \\x\t\t\032Some x characters have special meaning (refer to the full documentation).  For other characters, \\x "
                "literally matches the character x.  Some characters require \\x to match them, including ^, $, ., [, "
                "(, ), |, !, ?, +, *, and \\.\n"
;

