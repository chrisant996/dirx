// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set noet ts=8 sw=4 cino={0s:

#include "pch.h"
#include "usage.h"

const char c_usage[] = "%s -? for help.";

// --nix

const char c_long_usage[] =
"Displays a list of file and subdirectories in a directory.\n"
"\n"
"%s [options] [drive:][path][filename]\n"
"\n"
"  [drive:][path][filename]\n"
"			Specifies drive, directory, and/or files to list.\n"
"			Prefix the filename part with :: to use a regular\n"
"			expression.  For more information about regular\n"
"			expressions use '-? regex'.\n"
"\n"
"  -?, --help		Display this help text.\n"
"  -? colors		Display help text on color coding the file list.\n"
"  -? colorsamples	Display samples of the supported color codes.\n"
"  -? icons		Display help text on file icons and Nerd Fonts.\n"
"  -? pictures		Display help text on format pictures.\n"
"  -? regex		Display help text on regular expression syntax.\n"
"  -V, --version		Display version information.\n"
"\n"
"DISPLAY OPTIONS:\n"
"  -1			Display one column per line.\n"
"  -2			Display two columns per line (more in wide consoles).\n"
"  -4			Display four columns per line (more in wide consoles).\n"
"  -a, --all		Display all files (include hidden and system files).\n"
"  -b, --bare		Bare mode; only display names, no header/detail/etc.\n"
"  -c, --color		Display with colors (use '-? colors' for more info).\n"
"  -g, --git		List each file's git status.\n"
"  -gg, --git-repos	List status of git repo roots and each file's git\n"
"			status (or --git-repos-no-status to omit file status).\n"
"  -G, --grid		Synonym for --wide.\n"
"  -i, --icons[=WHEN]	Display file icons (use '-? icons' for more info).\n"
"			  always, auto, never (default)\n"
"  -k, --color-scale[=FIELD]\n"
"			Highlight levels of certain fields distinctly.\n"
"			  all, age, size, none (default)\n"
"  -l, --long		Long mode; display one file per line, plus attributes.\n"
"  -n, --normal		Force normal list format even on FAT volumes.\n"
"  -Q, --quash[=TYPES]	Quash types of output.  Use -Q by itself as a synonym\n"
"			for -Q+v+h+s.\n"
"			  v  Suppress the volume information\n"
"			  h  Suppress the header\n"
"			  s  Suppress the summary\n"
"			  -  Prefix to suppress next type (the default)\n"
"			  +  Prefix to un-suppress next type\n"
"  -p, --paginate	Pause after each screen full of information.\n"
"  -R			Synonym for --recurse.\n"
"  -s, --recurse		Subdirectories; recursively display files in specified\n"
"			directory and all subdirectories.\n"
"  -u, --usage		Display directory size usage data.\n"
"  -v, --vertical	Sort columns vertically.\n"
"      --horizontal	Sort columns horizontally (the default).\n"
"  -w, --wide		Wide mode; show as many columns as fit.\n"
"  -z, --fat		Force FAT list format even on non-FAT volumes.\n"
"  --color-scale-mode=MODE\n"
"			Mode for --color-scale (use '-? colors' for more info).\n"
"			  fixed, gradient (default)\n"
"  --hyperlinks		Display entries as hyperlinks.\n"
#ifdef DEBUG
"  --print-all-icons	Print a list of all icons (DEBUG ONLY).\n"
#endif
"\n"
"FILTERING AND SORTING OPTIONS:\n"
"  -a[...]		Display files with the specified attributes.  If\n"
"			attributes are combined, all attributes must match\n"
"			(-arhs only lists files with all three attributes set).\n"
"			The - prefix excludes files with that attribute (-arh-s\n"
"			lists files that are read-only and hidden and not\n"
"			system).  The + prefix includes files that have any of\n"
"			the + attributes set (-ar+h+s lists files that are\n"
"			read-only and are hidden or system).\n"
"			  r  Read-only files            e  Encrypted files\n"
"			  h  Hidden files               t  Temporary files\n"
"			  s  System files               p  Sparse files\n"
"			  a  Ready for archiving        c  Compressed files\n"
"			  d  Directories                o  Offline files\n"
"			  i  Not content indexed files\n"
"			  j  Reparse points (mnemonic for junction)\n"
"			  l  Reparse points (mnemonic for link)\n"
"			  +  Prefix meaning any\n"
"			  -  Prefix meaning not\n"
"  -A, --almost-all	Display all files, except hide . and .. directories.\n"
"  -h			Hide . and .. directories.\n"
"  -I, --ignore-glob=GLOB\n"
"			Glob patterns of files to ignore; the syntax is the\n"
"			same as in .gitignore.  The / is used as the directory\n"
"			separator.  An optional ! prefix negates a pattern; any\n"
"			matching file excluded by a previous pattern will be\n"
"			included again.  Multiple patterns may be specified\n"
"			separated by a ; or | character.\n"
"  -L, --levels=DEPTH	Limit the depth of recursion with -s.\n"
"  -o[...]		Sort the list by the specified options:\n"
"			  n  Name [and extension if 'e' omitted] (alphabetic)\n"
"			  e  Extension (alphabetic)\n"
"			  g  Group directories first\n"
"			  d  Date/time (oldest first)\n"
"			  s  Size (smallest first)\n"
"			  c  Compression ratio\n"
"			  a  Simple ASCII order (sort \"10\" before \"2\")\n"
"			  u  Unsorted\n"
"			  r  Reverse order for all options\n"
"			  -  Prefix to reverse order\n"
"  -X, --skip=TYPES	Skip types during -s.  Use -X by itself as a synonym\n"
"			for -X+d+j+r.\n"
"			  d  Skip hidden directories (when used with -s)\n"
"			  j  Skip junctions (when used with -s)\n"
"			  r  Skip files with no alternate data streams\n"
"			  -  Prefix to skip next type (this is the default)\n"
"			  +  Prefix to un-skip next type\n"
"  --git-ignore		Ignore files mentioned in .gitignore files.\n"
"  --hide-dot-files	Hide file and directory names starting with '.' or '_'.\n"
"			Using -a overrides this and shows them anyway.\n"
"  --reverse		Reverse the selected sort order.\n"
"  --string-sort		Sort punctuation as symbols.\n"
"  --word-sort		Sort punctuation as part of the word (default).\n"
"\n"
"FIELD OPTIONS:\n"
"  -C, --ratio		List the compression ratio.\n"
"  -q, --owner		List the owner of the file.\n"
"  -r, --streams		List alternate data streams of the file.\n"
"  -S, --size		List the file size even in multple column formats.\n"
"  -S[acf], --size=acf	Which size field to display or use for sorting:\n"
"			  a  Allocation size\n"
"			  c  Compressed size\n"
"			  f  File size (default)\n"
"  -t, --attributes	List the file attributes (use the flag twice to list\n"
"			all attributes, e.g. -tt).\n"
"  -T, --time		List the file time even in multiple column formats.\n"
"  -T[acw], --time=acw	Which time field to display or use for sorting:\n"
"			  a  Access time\n"
"			  c  Creation time\n"
"			  w  Write time (default)\n"
"  -x, --short-names	Show 8.3 short file names.\n"
"\n"
"FORMATTING OPTIONS:\n"
"  -,			Show the thousand separator in sizes (the default).\n"
"  -f[...]		Use the specified format picture.  You can greatly\n"
"			customize how the list is displayed (use '-? pictures'\n"
"			for more info).\n"
"  -F, --full-paths	Show full file paths in the file name column.\n"
"  -j			Justify file names in FAT list format.\n"
"  -J			Justify file names in non-FAT list formats.\n"
"  --justify[=WHEN]	Justify file names, in which list formats.  If WHEN is\n"
"			omitted, 'always' is assumed.\n"
"			  always, fat, normal, never (default)\n"
"  -SS			Show long file sizes (implies -S).  Note that some list\n"
"			formats limit the file size width.\n"
"  -TT			Show long dates and times (implies -T).  Note that some\n"
"			list formats limit the date and time width.\n"
"  -W, --width=COLS	Override the screen width.\n"
"  -Y			Abbreviate dates and times (implies -T).\n"
"  -Z			Abbreviate file sizes as 1K, 15M, etc (implies -S).\n"
"  --bare-relative	When listing subdirectories recursively, print paths\n"
"			relative to the specified patterns instead of expanding\n"
"			them to fully qualified paths.\n"
"  --classify		Print '\\' by dir names and '@' by symlink names.\n"
"  --compact		Use compact time format (short for --time and\n"
"			--time-style=compact).\n"
"  --escape-codes[=WHEN]\n"
"			For colors and hyperlinks in modern terminals.\n"
"			  always, auto (default), never\n"
"  --fit-columns		Fit more columns in -w mode by compacting column widths\n"
"			to fit their content (this is the default; use\n"
"			--no-fit-columns to disable it).\n"
"  --lower		Show file names using lower case.\n"
"  --mini-bytes		Show bytes in the mini size format when less than 1000.\n"
"  --mini-header		Show a mini header of just the directory name above\n"
"			each directory listing (if more than one directory).\n"
"  --more-colors=LIST	Add color rules in the same format as the DIRX_COLORS\n"
"			environment variable (use '-? colors' for more info).\n"
"  --nerd-fonts=VER	Select which Nerd Fonts version to use (see '-? colors'\n"
"			for more info).\n"
"  --nix		Selects default options that are similar to Unix and\n"
"			Linux systems.  Hides files starting with '.', skips\n"
"			recursing into hidden directories, sorts vertically,\n"
"			displays the file list in wide mode, selects 'compact'\n"
"			time style, shows a mini directory header above file\n"
"			lists, quashes volume/header/summary output, disables\n"
"			filename justify, shows long file names, shows '-' as\n"
"			size for dirs, and suppresses thousands separators.\n"
"  --pad-icons=SPACES	Number of spaces to print after an icon.\n"
"  --relative		Use relative time format (short for --time and\n"
"			--time-style=relative).\n"
"  --size-style=STYLE	Which size format to use for display by default when\n"
"			not overridden by other format options:\n"
"			  mini, short, normal (default)\n"
"  --time-style=STYLE	Which time format to use for display by default when\n"
"			not overridden by other format options:\n"
"			  locale (default), mini, compact, short, normal,\n"
"			  full, iso, long-iso, relative\n"
"  --truncate-char=HEX	Set the truncation character for file names that don't\n"
"			fit in the allotted space.  Specify a Unicode character\n"
"			value in hexadecimal (e.g. 2192 is a right-pointing\n"
"			arrow and 25b8 is a right-pointing triangle).  Or\n"
"			specify 002e to use .. (two periods).\n"
"  --utf8		When output is redirected, produce UTF8 output instead\n"
"			of using the system codepage.\n"
"\n"
"Long options that can be used without an argument also accept a 'no-' prefix\n"
"to disable them.  For example, the --fit-columns option is enabled by default,\n"
"and using --no-fit-columns disables it.\n"
"\n"
"Environment variables:\n"
"\n"
"Set DIRXCMD to preset options to use by default.  To override any on/off\n"
"option, add a hyphen (-k- overrides -k).  Or for long options insert \"no-\"\n"
"(--no-attributes overrides --attributes).  For interop compatibility with the\n"
"CMD DIR command, slash options may also be used, for example /w.  Unlike the\n"
"CMD DIR command, use /w- instead of /-w to override the slash option.\n"
"\n"
"Set DIRX_COLORS to specify colors to use in the file list display.  Use\n"
"'-? colors' for more info on color coding rules.\n"
;

const char c_help_colors[] =
"Set the DIRX_COLORS environment variable to control how files and directories\n"
"are colored.  The format is:\n"
"\n"
"  condition=color: condition=color: ...\n"
"\n"
"Any number of pairs of conditions and colors can be given.  Use colons to\n"
"separate them and build a list of many coloring rules.\n"
"\n"
"The list can start with the special keyword \"reset\" to clear all default\n"
"color rules and any rules that may have been provided in the LS_COLORS\n"
"environment variable.\n"
"\n"
"Examples:\n"
"  sc=93:bu=93;4:cm=38;5;172:\n"
"        Source code files in yellow, build system files in yellow with\n"
"        underline, compiled files in orange.\n"
"  ro=32:ex=1:ro ex=1;31:ln=target:\n"
"        Read-only files in green, executable files in bright white, files\n"
"        that are both read-only and executable in bright green, and symlinks\n"
"        colored by the type of the target of the symlink.\n"
"\n"
"CONDITION syntax:\n"
"\n"
"Each condition can specify two-letter types, or patterns to match.\n"
"If more than one are specified then all of them must match, and rules with\n"
"multiple types and/or patterns are evaluated in the order they are listed.\n"
"\n"
"Negation:\n"
"  not     negates the next type or pattern (not hi means not hidden)\n"
"  !       negates the next type or pattern (!hi means not hidden)\n"
"\n"
"File attribute names:\n"
"  ar  archive             hi  hidden              ro  read-only\n"
"  cT  compressed attr     ln  symlink             SP  sparse\n"
"  di  directory           NI  not indexed         sy  system\n"
"  en  encrypted           of  offline             tT  temporary attr\n"
"  fi  file                or  orphaned symlink\n"
"\n"
"  NOTES: Setting ro by itself applies only to files, not directories.\n"
"         Setting ln=target or ln=: colors symlinks by the type of the target\n"
"         of the symlink.\n"
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
"  xx  punctuation (for example _ in the attributes field)\n"
"  hM  the directory name with --mini-header\n"
"  ur  the read-only attribute letter in the attributes field\n"
"  su  the hidden attribute letter in the attributes field\n"
"  sf  the system attribute letter in the attributes field\n"
"  pi  the junction attribute letter in the attributes field\n"
"  lp  the symlink path when showing symlink targets\n"
"\n"
"  NOTE:  the color element names above cannot be combined with types or\n"
"         patterns; they're for setting general colors, and don't affect\n"
"         how file names are colored.\n"
"\n"
"Anything else is interpreted as a pattern to match against the file name.\n"
"The patterns use the fnmatch glob syntax, which is also used by .gitignore.\n"
"\n"
"The most common use is to match a file extension, for example \"*.txt\" matches\n"
"files whose name ends with \".txt\".\n"
"\n"
"Patterns can include ? and * wildcards, and can also include character sets\n"
"enclosed in [].  For example \"ab[xyz]\" matches abx, aby, and abz.  A set can\n"
"include ranges, for example \"[a-z]\" matches any letter.  If the first\n"
"character in the set is ! or ^ the set excludes the listed characters, for\n"
"example \"[!0-9]\" matches anything except digits.  The ? character in a set\n"
"matches any character but only if there's a character to match, for example\n"
"\"ab[?]\" matches abc, but not ab.  Character sets can use [:class:] to\n"
"specify a class of characters; 'class' can be one of alnum, alpha, blank,\n"
"cntrl, digit, graph, lower, print, punct, space, xdigit, upper.\n"
"\n"
"Anything quoted is treated as a pattern, for example \"ro\" refers to a file\n"
"named \"ro\" instead of read-only files.\n"
"\n"
"A pattern by itself with no types applies only to files, not directories.\n"
"To specify a pattern that matches anything, combine di and the pattern.\n"
"\n"
"COLOR syntax:\n"
"\n"
"Colors are the SGR parameter to ANSI color escape codes.\n"
"https://en.wikipedia.org/wiki/ANSI_escape_code\n"
"\n"
"The following color parameters are allowed.  Note that the color parameters\n"
"are just sent to the terminal, and it is the terminal's responsibility to draw\n"
"them properly.  Some parameters might not work in all terminal programs.\n"
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
"  38;2;n;n;n    specify a 24-bit color where each n is from 0 to 255,\n"
"                and the order is red;green;blue.\n"
"  38;5;n        specify an 8-bit color where n is from 0 to 255.\n"
"\n"
"Run \"%s -? colorsamples\" to display a chart of the 8-bit color codes and\n"
"the available styles.\n"
"\n"
"Environment variables:\n"
"\n"
"Set NO_COLOR to any non-empty value to completely disable colors.\n"
"See https://no-color.org/ for more info.\n"
"\n"
"Set DIRX_COLOR_SCALE to any value accepted by the --color-scale flag to\n"
"control the default behavior.  If it's not set, then EZA_COLOR_SCALE and\n"
"EXA_COLOR_SCALE are also checked.\n"
"\n"
"Set DIRX_COLOR_SCALE_MODE to any value accepted by the --color-scale-mode flag\n"
"to control the default behavior.  If it's not set, then EZA_COLOR_SCALE_MODE\n"
"and EXA_COLOR_SCALE_MODE are also checked.\n"
"\n"
"Set DIRX_MIN_LUMINANCE to a value from -100 to 100 to control the range of\n"
"intensity decay in the gradient color scale mode.  If it's not set, then\n"
"EZA_MIN_LUMINANCE and EXA_MIN_LUMINANCE are also checked.\n"
"\n"
"If the DIRX_COLORS environment variable is not set, then LS_COLORS is also\n"
"checked.  DIRX enhancements are ignored when parsing LS_COLORS.)\n"
;

const char c_help_icons[] =
"To see icons, a Nerd Font is required; https://nerdfonts.com\n"
"By default, a Nerd Fonts v3 font is assumed.\n"
"\n"
"A few quick examples (without a Nerd Font these will look garbled):\n"
"\n"
"%s"
"\n"
"Icons are selected based on the name of the file or folder, or the extension\n"
"of the file name.  The icon mappings are hard-coded and aren't configurable at\n"
"this time.\n"
"\n"
"Environment variables:\n"
"\n"
"Set DIRX_NERD_FONTS_VERSION=2 to select icons compatible with v2 fonts.\n"
"\n"
"Set DIRX_ICON_SPACING=n to specify the number of spaces to print after an\n"
"icon.  The default is 1, but some terminals or fonts may need a different\n"
"number of spaces for icons to look good.  If DIRX_ICON_SPACING isn't set,\n"
"then EZA_ICON_SPACING and EXA_ICON_SPACING are checked as well.\n"
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
"Use the -f option to specify a custom format picture to list files.  Format\n"
"picture field types and styles are case sensitive:  field types are uppercase\n"
"and field styles are lowercase.\n"
"\n"
"Examples:\n"
"  \"F  Sm\"		Filename  3.2M\n"
"  \"Ds  Sm Trh F18\"	10/04/09  13:45  123K r_ SomeVeryLongFilen%s\n"
"\n"
"Format picture field types:\n"
"  F[#flx]	Filename field.  By default this automatically chooses the\n"
"		field width and style based on the other options used.\n"
"		  #     A number indicating the width for the field.  If a\n"
"		        file name is wider, it is truncated.  If a file name\n"
"		        is narrower, it is padded with spaces to the right.\n"
"		  f     Use FAT name format.\n"
"		  l     Show the long file name.\n"
"		  x     Show the short file name.\n"
"  S[msacf]	Size field.  By default this automatically chooses the field\n"
"		width and style based on the other options used.\n"
"		  m     Use mini format (use 4 characters, always abbreviate).\n"
"		  s     Use short format (use 9 characters, abbreviate when\n"
"		        necessary).\n"
"		  a     Show the file allocation size.\n"
"		  c     Show the compressed file size.\n"
"		  f     Show the file data size.\n"
"  D[lmsxacw]	Date and time field.  By default this automatically chooses the\n"
"		field width and style based on the other options used.\n"
"		  l     Use locale format (like CMD with extensions enabled).\n"
"		  m     Use mini format (11 characters with time for recent\n"
"		        files or year for older files).\n"
"		  i     Use iso format (11 characters; excludes year).\n"
"		  p     Use compact format (12 characters with time for recent\n"
"		        files or year for older files; includes month name).\n"
"		  s     Use short format (15 characters with 2 digit year).\n"
"		  o     Use long iso format (16 characters with 4 digit year).\n"
"		  n     Use normal format (17 characters with 4 digit year).\n"
"		  x     Use extended format (24 characters with 4 digit year,\n"
"		        plus minutes and milliseconds).\n"
"		  a     Show the last access time.\n"
"		  c     Show the creation time.\n"
"		  w     Show the last write time.\n"
"  C[?]		Compression ratio field.  By default this shows the field even\n"
"		if the corresponding option was not specified.\n"
"		  ?     Show only if the corresponding option was specified.\n"
"  O[?]		Owner field.  By default this shows the field even if the\n"
"		corresponding option was not specified.\n"
"		  ?     Show only if the corresponding option was specified.\n"
"  X[?]		Short name field.  By default this shows the field even if the\n"
"		corresponding option was not specified.\n"
"		  ?     Show only if the corresponding option was specified.\n"
"  T[...]	Attributes field.  By default this shows the field even if the\n"
"		corresponding option was not specified, and shows all file\n"
"		attributes.  Control which attributes are listed as follows:\n"
"		  ?     Show only if the corresponding option was specified.\n"
"		  r     Read-only                         e     Encrypted\n"
"		  h     Hidden                            t     Temporary\n"
"		  s     System                            p     Sparse\n"
"		  a     Ready for archiving               c     Compressed\n"
"		  d     Directory                         o     Offline\n"
"		  i     Not content indexed\n"
"		  j     Reparse point (mnemonic for junction)\n"
"		  l     Reparse point (mnemonic for link)\n"
"  G[?]		Git file status field.  When in a git repo, this shows two\n"
"		letters corresponding to the git file status.  This field is\n"
"		omitted if git file status was not requested by --git or etc.\n"
"  R[?]		Git repo field.  When listing a directory that is a git repo,\n"
"		this shows the status and branch name.  This field is omitted\n"
"		if git repo status was not requested by --git-repos or etc.\n"
"\n"
"Other characters in the format picture are output as-is.  To ensure a\n"
"character is output as-is rather than being treated as a field type (etc),\n"
"use \\ to escape the character (e.g. \"\\F:F\" shows \"F:MyFile.doc\").\n"
"\n"
"For some field types ? suppresses the field if the corresponding option was\n"
"not specified.  When a field is suppressed, normally the field and the spaces\n"
"immediately preceding it are omitted from the resulting format picture.  For\n"
"finer control over what part of the format picture is omitted, surround the\n"
"text to be omitted using the [ and ] characters.  Everything between the\n"
"brackets will be shown or omitted as a unit, and the brackets themselves are\n"
"not shown (e.g. \"F [(C?) ]Sm\" shows \"File (12%%) 1.2M\" or \"File 1.2M\").\n"
"\n"
"The -F option fits the full path name into the filename field, if the field is\n"
"at the end of the format picture.\n"
"\n"
"The -w option fits as many files as possible onto each line, using the custom\n"
"format picture.  The -2 option is incompatible with format pictures.\n"
"\n"
"The -r option forces a single column list, but fits the stream name into the\n"
"filename field(s).  If the filename field is at the end of the format picture\n"
"then the entire stream names are shown; otherwise the stream names are\n"
"truncated to the width of the longest file name.\n"
;

static const char c_help_regex[] =
"Regular expressions specify a substring to find.  In addition, several special\n"
"operators perform advanced searches.\n"
"\n"
"To use a regular expression as a filename pattern, use :: followed by the\n"
"regular expression.  When specifying both a path and a regular expression, the\n"
":: must precede the regular expression, for example \"folder\\::regex\".  If the\n"
"regular expression contains spaces or characters with special meaning to the\n"
"shell (such as |, <, >) then enclose the expression in double quotes.\n"
"\n"
"Note that regular expressions are not natively supported by the OS, so all\n"
"filenames must be retrieved and compared with the regular expression.  This can\n"
"make using regular expressions much slower than using ? and * wildcards.\n"
"\n"
"The regular expression is the ECMAScript syntax described here:\n"
"https://learn.microsoft.com/en-us/cpp/standard-library/regular-expressions-cpp\n"
"\n"
"Here is a quick summary; for details about the full syntax available, refer to\n"
"the documentation link above.\n"
"\n"
"  Operator	Description\n"
"  ^		Matches the null string at the beginning of a line.\n"
"  $		Matches the null string at the end of a line.\n"
"  .		Matches any character.\n"
"  [set]		Matches any characters in the set.  The - character selects a\n"
"		range of characters.  The expression [a-z] matches any\n"
"		lowercase letter from a to z.  To select the ] character, put\n"
"		it at the beginning of the set.  To select the - character, put\n"
"		it at the end of the set.  To select the ^ character, put it\n"
"		anywhere except immediately after the opening bracket (which\n"
"		selects the complement of the set; see below).\n"
"  [^set]	Matches any characters not in the set (matches the complement\n"
"		of the set).  The syntax for a complement set is the same as\n"
"		the syntax for a normal set (above), except that it is preceded\n"
"		by a ^ character.\n"
"  [:class:]	Matches any character in the class (alnum, alpha, blank, cntrl,\n"
"		digit, graph, lower, print, punct, space, upper, xdigit).\n"
"  (abc)		Matches abc, and tags it as a sub-expression for retrieval.\n"
"  abc|def	Matches abc or def.\n"
"  x?		Matches 0 or 1 occurrences of the character x (or set, or\n"
"		tagged sub-expression).\n"
"  x+		Matches 1 or more occurrences of the character x (or set, or\n"
"		tagged sub-expression).  x+? uses non-greedy repetition.\n"
"  x*		Matches 0 or more occurrences of the character x (or set, or\n"
"		tagged sub-expression).  x*? uses non-greedy repitition.\n"
"  !x		Matches anything but x.\n"
"  \\b		Matches the null string at a word boundary (\\babc\\b matches\n"
"		' abc ' but not abcx or xabc or xabcx).\n"
"  \\B		Matches the null string not at a word boundary (\\Babc matches\n"
"		xabc but not ' abc').\n"
"  \\x		Some x characters have special meaning (refer to the full\n"
"		documentation).  For other characters, \\x literally matches the\n"
"		character x.  Some characters require \\x to match them,\n"
"		including ^, $, ., [, (, ), |, !, ?, +, *, and \\.\n"
;

