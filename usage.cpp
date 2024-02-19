// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set noet ts=8 sw=4 cino={0s:

#include "pch.h"
#include "usage.h"

const char c_usage[] = "%s -? for help.";

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
"  -? icons		Display help text on file icons and Nerd Fonts.\n"
"  -? pictures		Display help text on format pictures.\n"
"  -? regex		Display help text on regular expression syntax.\n"
"  -V, --version	Display version information.\n"
"\n"
"DISPLAY OPTIONS:\n"
"  -1			Display one column per line.\n"
"  -2			Display two columns per line (more in wide consoles).\n"
"  -4			Display four columns per line (more in wide consoles).\n"
"  -a, --all		Display all files (include hidden and system files).\n"
"  -b, --bare		Bare mode; only display names, no header/detail/etc.\n"
"  -c, --color		Display with colors (use '-? colors' for more info).\n"
"  -i, --icons[=WHEN]	Display file icons (use '-? icons' for more info).\n"
"			  always, auto, never (default)\n"
"  -n, --normal		Force normal list format even on FAT volumes.\n"
"  -Q..., --quash=...	Quash types of output:\n"
"			  v  Suppress the volume information\n"
"			  h  Suppress the header\n"
"			  s  Suppress the summary\n"
"			  -  Prefix to suppress next type (the default)\n"
"			  +  Prefix to un-suppress next type\n"
"  -p, --paginate	Pause after each screen full of information.\n"
"  -s, --recurse		Subdirectories; recursively display files in specified\n"
"			directory and all subdirectories.\n"
"  -u, --usage		Display directory size usage data.\n"
"  -v, --vertical	Sort columns vertically.\n"
"      --horizontal	Sort columns horizontally (the default).\n"
"  -w, --wide		Wide mode; show as many columns as fit.\n"
"  -z, --fat		Force FAT list format even on non-FAT volumes.\n"
"  -k, --color-scale[=FIELD]\n"
"			Highlight levels of certain fields distinctly.\n"
"			  all, age, size, none (default)\n"
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
"  -h			Hide . and .. directories.\n"
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
"  -X..., --skip=...	Skip types during -s:\n"
"			  d  Skip hidden directories (when used with -s)\n"
"			  j  Skip junctions (when used with -s)\n"
"			  r  Skip files with no alternate data streams\n"
"			  -  Prefix to skip next type (this is the default)\n"
"			  +  Prefix to un-skip next type\n"
"  --hide-dot-files	Hide file and directory names starting with '.' or '_'.\n"
"			Using -a overrides this and shows them anyway.\n"
"  --string-sort		Sort punctuation as symbols.\n"
"  --word-sort		Sort punctuation as part of the word (default).\n"
"\n"
"FIELD OPTIONS:\n"
"  -C, --ratio		List the compression ratio.\n"
"  -q, --owner		List the owner of the file.\n"
"  -r, --streams	List alternate data streams of the file.\n"
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
"  -l, --lower		Show file names using lower case.\n"
"  -SS			Show long file sizes (implies -S).  Note that some list\n"
"			formats limit the file size width.\n"
"  -TT			Show long dates and times (implies -T).  Note that some\n"
"			list formats limit the date and time width.\n"
"  -W, --width=COLS	Override the screen width.\n"
"  -Y			Abbreviate dates and times (implies -T).\n"
"  -Z			Abbreviate file sizes as 1K, 15M, etc (implies -S).\n"
"  --classify		Print '\\' by dir names and '@' by symlink names.\n"
"  --compact-columns	Fit more columns in -w mode by compacting column widths\n"
"			to fit their content (this is the default; use\n"
"			--no-compact-columns to disable it).\n"
"  --escape-codes[=WHEN]\n"
"			For colors and hyperlinks in modern terminals.\n"
"			  always, auto (default), never\n"
"  --more-colors=LIST	Add color rules in the same format as the DIRX_COLORS\n"
"			environment variable (use '-? colors' for more info).\n"
"  --nerd-fonts=VER	Select which Nerd Fonts version to use (see '-? colors'\n"
"			for more info).\n"
"  --truncate-char=HEX	Set the truncation character for file names that don't\n"
"			fit in the allotted space.  Specify a Unicode character\n"
"			value in hexadecimal (e.g. -E2192 is a right-pointing\n"
"			arrow and -E25b8 is a right-pointing triangle).\n"
"  --pad-icons=SPACES	Select how many spaces to print after an icon.  The\n"
"			default is 1 space.  Some terminals or fonts may need\n"
"			a different number of spaces for icons to look good.\n"
"\n"
"Options may be preset in the DIRXCMD environment variable.  Override preset\n"
"options by following any on/off option with - (hyphen), for example -h-.\n"
"\n"
"Long options that can be used without an argument also accept a 'no-' prefix\n"
"to disable them.  For example, the --compact-columns option is enabled by\n"
"default, and using --no-compact-columns disables it.\n"
"\n"
"Specify color coding rules in the DIRX_COLORS environment variable, or set it\n"
"to * to use some predefined rules.  Use '-? colors' for more info.\n"
;

const char c_help_colors[] =
"TBD.\n"
// TODO: Rewrite help since the implementation totally changed.
// TODO: Mention %NO_COLOR% and https://no-color.org/.
#if 0
"Set the DIRX_COLORS environment variable to control how files and directories\n"
"are colored.  The format is:\n"
"\n"
"  condition: color; condition: color; ...\n"
"\n"
"Any number of conditions can be given; use semicolons to separate them and\n"
"build a list of many coloring rules.  The first matching condition is used.\n"
"\n"
"Conditions are a combination of attributes or file extensions.  If any \n"
"attribute or file extension matches, then the condition is true.  Combine\n"
"attributes and file extensions using the .and., .or., .xor., and .not.\n"
"operators to create more complex rules (operators are evaluated from left to\n"
"right, parentheses are not supported, and Boolean short circuiting applies).\n"
"\n"
"Attributes can be any of the following (synonyms in brackets):\n"
"  archive			notindexed\n"
"  compressed			offline\n"
"  directory [dirs]		readonly [rdonly]\n"
"  encrypted			sparse\n"
"  hidden			system\n"
"  junction			temporary\n"
"  normal (files with no attributes set)\n"
"\n"
"File extensions can include ? and * wildcards, and can also include character\n"
"sets enclosed in [], for example \"ab[xyz]\" matches abx, aby, and abz.  Sets\n"
"can include ranges, for example \"[a-z]\" matches any letter.  If the first\n"
"character in the set is ! the set excludes the listed characters, for example\n"
"\"[!0-9]\" matches anything except digits.  The ? character in a set matches any\n"
"character, but only if there's a character to match:  for example \"ab[?]\" will\n"
"match abc, but will not match ab.\n"
"\n"
"Colors are 'foregroundcolor' or 'foregroundcolor on backgroundcolor'.\n"
"The color names are black, blue, green, cyan, red, magenta, yellow, and\n"
"white, and can be prefixed with bright (all colors can be abbreviated as\n"
"their first three letters).  If the background color is omitted, then only\n"
"the foreground color is applied.  This can be useful if you use command\n"
"windows with different background colors; the background color is inherited\n"
"from the console window.\n"
"\n"
"Examples:\n"
"\n"
"  system hidden:bright red on black\n"
"		Colors files with the system or hidden attributes as bright\n"
"		red on black.\n"
"\n"
"  directory:bri yel on bla; tmp temporary:cya on bla\n"
"		Colors files with the directory attribute as bright yellow on\n"
"		black, and files with the temporary attribute or tmp file\n"
"		extension as cyan on black.\n"
"\n"
"  exe bat cmd com pl vbs:bri gre\n"
"		Colors files with the exe, bat, cmd, com, pl, or vbs file\n"
"		extensions as bright green.\n"
"\n"
"Or set FSCOLORDIR to * to use the following set of predefined color rules:\n"
"\n"
"  hidden system:bri red on bla; \n"
"  directory:bri yel on bla; \n"
"  readonly .and. exe com bat btm cmd pif pl lnk vbs:bri gre on bla; \n"
"  exe com bat btm cmd pif pl lnk vbs:bri whi on bla; \n"
"  dpk:bri cya on bla; \n"
"  zip:cya on bla; \n"
"  arc arj lzh rar zoo:mag on bla; \n"
"  readonly:gre on bla\n"
"\n"
"TakeCommand/4NT compatibility:  If the FSCOLORDIR environment variable is not\n"
"set then COLORDIR is checked.  If COLORDIR is also not set but 4DOS is set to\n"
"point at a directory, then COLORDIR is read from a tcmd.ini or 4nt.ini file\n"
"in that directory.  Regular expressions are not yet supported in the coloring\n"
"rules.  Visit http://www.jpsoft.com for info on TakeCommand/4NT.\n"
#endif
;

// TODO: write help text for icons.
const char c_help_icons[] =
"TBD.\n"
;

const char c_help_pictures[] =
"Use the -f option to specify a custom format picture to list files.  Format\n"
"picture field types and styles are case sensitive:  field types are uppercase\n"
"and field styles are lowercase.\n"
"\n"
"Examples:\n"
"  \"F  Sm\"		Filename  3.2M\n"
"  \"Ds  Sm Trh F18\"	10/04/09  13:45  123K r_ SomeVeryLongFilen%c\n"
"\n"
"Format picture field types:\n"
"  F[#flx]	Filename field.  By default this automatically chooses the\n"
"		field width and style based on the other options used.\n"
"		  #	A number indicating the width for the field.  If a\n"
"			file name is wider, it is truncated.  If a file name\n"
"			is narrower, it is padded with spaces to the right.\n"
"		  f	Use FAT name format.\n"
"		  l	Show the long file name.\n"
"		  x	Show the short file name.\n"
"  S[msacf]	Size field.  By default this automatically chooses the field\n"
"		width and style based on the other options used.\n"
"		  m	Use mini format (use 4 characters, always abbreviate).\n"
"		  s	Use short format (use 9 characters, abbreviate when\n"
"			necessary).\n"
"		  a	Show the file allocation size.\n"
"		  c	Show the compressed file size.\n"
"		  f	Show the file data size.\n"
"  D[lmsxacw]	Date and time field.  By default this automatically chooses the\n"
"		field width and style based on the other options used.\n"
"		  l	Use locale format (like CMD with extensions enabled).\n"
"		  m	Use mini format (11 characters with time for recent\n"
"			files or year for older files).\n"
"		  n	Use normal format (17 characters with four digit year).\n"
"		  s	Use short format (15 characters with two digit year).\n"
"		  x	Use extended format (24 characters with four digit\n"
"			year, minutes, and milliseconds).\n"
"		  a	Show the last access time.\n"
"		  c	Show the creation time.\n"
"		  w	Show the last write time.\n"
"  C[?]		Compression ratio field.  By default this shows the field even\n"
"		if the corresponding option was not specified.\n"
"		  ?	Show only if the corresponding option was specified.\n"
"  O[?]		Owner field.  By default this shows the field even if the\n"
"		corresponding option was not specified.\n"
"		  ?	Show only if the corresponding option was specified.\n"
"  X[?]		Short name field.  By default this shows the field even if the\n"
"		corresponding option was not specified.\n"
"		  ?	Show only if the corresponding option was specified.\n"
"  T[...]	Attributes field.  By default this shows the field even if the\n"
"		corresponding option was not specified, and shows all file\n"
"		attributes.  Control which attributes are listed as follows:\n"
"		  ?	Show only if the corresponding option was specified.\n"
"		  r	Read-only			  e	Encrypted\n"
"		  h	Hidden				  t	Temporary\n"
"		  s	System				  p	Sparse\n"
"		  a	Ready for archiving		  c	Compressed\n"
"		  d	Directory			  o	Offline\n"
"		  i	Not content indexed\n"
"		  j	Reparse point (mnemonic for junction)\n"
"		  l	Reparse point (mnemonic for link)\n"
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
"not shown (e.g. \"F [(C?) ]Sm\" shows \"File (12%) 1.2M\" or \"File 1.2M\").\n"
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
// TODO: Describe basics of ECMAScript regex and provide link to full docs.
#if 0
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
"  (abc)		Matches abc, and tags it as a sub-expression for retrieval.\n"
"  abc|def	Matches abc or def.\n"
"  x?		Matches 0 or 1 occurrences of the character x (or set, or\n"
"		tagged sub-expression).\n"
"  x+		Matches 1 or more occurrences of the character x (or set, or\n"
"		tagged sub-expression).\n"
"  x*		Matches 0 or more occurrences of the character x (or set, or\n"
"		tagged sub-expression).\n"
"  \\<		Matches the null string at the beginning of a word.\n"
"  \\>		Matches the null string at the end of a word.\n"
"  \\x		Literally matches the special character x.  Special characters\n"
"		are ^, $, ., [, (, ), |, ?, +, *, and \\.\n"
#endif
;

