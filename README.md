# DirX - the `dir` command, extended

Displays a list of file and subdirectories in a directory.

DirX is designed so that most command line options from the CMD `dir` command work the same, to make DirX mostly a drop-in replacement for `dir`.

## Usage

```
DIRX [options] [drive:][path][filename]
```

### Arguments

```
  [drive:][path][filename]
```

Specifies drive, directory, and/or files to list.  Use as many arguments as desired (or none, in which case the current directory is assumed).

Prefix the filename part with :: to use a [regular expression](#regular-expressions).

### Options

<table>
<tr><td><code>-?</code>, <code>--help</code></td><td>Display the help text.</td></tr>
<tr><td><code>-? colors</code></td><td>Display help text on color coding the file list.</td></tr>
<tr><td><code>-? colorsamples</code></td><td>Display samples of the supported color codes.</td></tr>
<tr><td><code>-? icons</code></td><td>Display help text on file icons and Nerd Fonts.</td></tr>
<tr><td><code>-? pictures</code></td><td>Display help text on format pictures.</td></tr>
<tr><td><code>-? regex</code></td><td>Display help text on regular expression syntax.</td></tr>
<tr><td><code>-V</code>, <code>--version</code></td><td>Display version information.</td></tr>
<tr><td><code>-TEST</code></td><td><table><tr><td>one</td><td>two</td></tr><tr><td>one</td><td>two</td></tr></table></td></tr>
</table>

#### Display Options

Flag | Description
-|-
`-1` | Display one column per line.
`-2` | Display two columns per line (more in wide consoles).
`-4` | Display four columns per line (more in wide consoles).
`-a`, `--all` | Display all files (include hidden and system files).
`-b`, `--bare` | Bare mode; only display names, no header/detail/etc.
`-c`, `--color` | Display with colors (use '-? colors' for more info).
`-g`, `--git` | List each file's git status.
`-gg`, `--git-repos` | List status of git repo roots and each file's git status (or --git-repos-no-status to omit file status).
`-i`, `--icons[=WHEN]` | Display file icons (use '-? icons' for more info).<br/>`always`, `auto`, `never` (default)
`-l`, `--long` | Long mode; display one file per line, plus attributes.
`-n`, `--normal` | Force normal list format even on FAT volumes.
`-Q`, `--quash[=TYPES]` | Quash types of output.  Use `-Q` by itself as a synonym for `-Q+v+h+s`.

"			  v  Suppress the volume information\n"
"			  h  Suppress the header\n"
"			  s  Suppress the summary\n"
"			  -  Prefix to suppress next type (the default)\n"
"			  +  Prefix to un-suppress next type\n"

Flag | Description
-|-
`-p`, `--paginate` | Pause after each screen full of information.
`-R` | Synonym for `--recurse`.
`-s`, `--recurse` | Subdirectories; recursively display files in specified directory and all subdirectories.
`-u`, `--usage` | Display directory size usage data.
`-v`, `--vertical` | Sort columns vertically.
`--horizontal` | Sort columns horizontally (the default).
`-w`, `--wide` | Wide mode; show as many columns as fit.
`-z`, `--fat` | Force FAT list format even on non-FAT volumes.
`-k`, `--color-scale[=FIELD]` | Highlight levels of certain fields distinctly.<br/>`all`, `age`, `size`, `none` (default)
`--color-scale-mode=MODE` | Mode for `--color-scale` (use `-? colors` for more info).<br/>`fixed`, `gradient` (default)
`--hyperlinks` | Display entries as hyperlinks.

#### Filtering and Sorting Options

"  -a[...]		Display files with the specified attributes.  If attributes are combined, all attributes must match (-arhs only lists files with all three attributes set).  The - prefix excludes files with that attribute (-arh-s lists files that are read-only and hidden and not system).  The + prefix includes files that have any of the + attributes set (-ar+h+s lists files that are read-only and are hidden or system).\n"
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
"  -I, --ignore-glob=GLOB\b"
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
"			for -X+d+j+r options.\n"
"			  d  Skip hidden directories (when used with -s)\n"
"			  j  Skip junctions (when used with -s)\n"
"			  r  Skip files with no alternate data streams\n"
"			  -  Prefix to skip next type (this is the default)\n"
"			  +  Prefix to un-skip next type\n"
"  --git-ignore		Ignore files mentioned in .gitignore files.\n"
"  --hide-dot-files	Hide file and directory names starting with '.' or '_'.\n"
"			Using -a overrides this and shows them anyway.\n"
"  --string-sort		Sort punctuation as symbols.\n"
"  --word-sort		Sort punctuation as part of the word (default).\n"

#### Field Options

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
"  --size-style=STYLE	Which size format to use for display by default when\n"
"			not overridden by other format options:\n"
"			  mini, short, normal (default)\n"
"  --time-style=STYLE	Which time format to use for display by default when\n"
"			not overridden by other format options:\n"
"			  locale (default), mini, compact, short, normal,\n"
"			  full, iso, long-iso, relative\n"

#### Formatting Options

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
"  --classify		Print '\\' by dir names and '@' by symlink names.\n"
"  --compact-columns	Fit more columns in -w mode by compacting column widths\n"
"			to fit their content (this is the default; use\n"
"			--no-compact-columns to disable it).\n"
"  --escape-codes[=WHEN]\n"
"			For colors and hyperlinks in modern terminals.\n"
"			  always, auto (default), never\n"
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
"  --truncate-char=HEX	Set the truncation character for file names that don't\n"
"			fit in the allotted space.  Specify a Unicode character\n"
"			value in hexadecimal (e.g. 2192 is a right-pointing\n"
"			arrow and 25b8 is a right-pointing triangle).  Or\n"
"			specify 002e to use .. (two periods).\n"
"  --utf8		When output is redirected, produce UTF8 output instead\n"
"			of using the system codepage.\n"

Long options that can be used without an argument also accept a `no-` prefix to disable them.  For example, the `--compact-columns` option is enabled by default, and using `--no-compact-columns` disables it.

#### Environment Variables

Set `DIRXCMD` to preset options to use by default.  To override any on/off option, add a hyphen (`-k-` overrides `-k`).  Or for long options insert `no-` (`--no-attributes` overrides `--attributes`).  For interop compatibility with the CMD `dir` command, slash options may also be used, for example `/w`.  Unlike the CMD `dir` command, use `/w-` instead of `/-w` to override the slash option.

Set `DIRX_COLORS` to specify colors to use in the file list display.  Use `-? colors` or see [Colors](#colors) for more info on color coding rules.

## Colors

Set the `DIRX_COLORS` environment variable to control how files and directories are colored.  The format is:

```
  condition=color: condition=color: ...
```

Any number of pairs of conditions and colors can be given.  Use colons to separate them and build a list of many coloring rules.

The list can start with the special keyword `reset` to clear all default color rules and any rules that may have been provided in the LS_COLORS environment variable.

#### Examples:

```
set DIRX_COLORS=sc=93:bu=93;4:cm=38;5;172
```
- Source code files in yellow.
- Build system files in yellow with underline.
- Compiled files in orange.

```
set DIRX_COLORS=ro=32:ex=1:ro ex=1;31:ln=target
```
- Read-only files in green.
- Executable files in bright white.
- Files that are both read-only and executable in bright green.
- Symlinks colored by the type of the target of the symlink.

### CONDITION syntax

Each condition can specify two-letter types, or patterns to match.  If more than one are specified then all of them must match, and rules with multiple types and/or patterns are evaluated in the order they are listed.

#### Negation:

- `not` negates the next type or pattern (`not hi` means not hidden).
- `!` negates the next type or pattern (`!hi` means not hidden).

#### File attribute names:

- `ar` Archive attribute (ready for archiving).
- `cT` Compressed attribute.
- `di` Directory.
- `en` Encrypted attribute.
- `fi` File.
- `hi` Hidden.
- `ln` Symlink.
- `NI` Not content indexed attribute.
- `of` Offline attribute.
- `or` Orphaned symlink.
- `ro` Read-only attribute.
- `SP` Sparse file.
- `sy` System attribute.
- `tT` Temporary attribute.

Setting `ro` by itself applies only to files, not directories.<br/>
Setting `ln=target` or `ln=:` colors symlinks by the type of the target of the symlink.

#### File groups:

- `ex` Executable files.
- `do` Document files.
- `im` Image files.
- `vi` Video files.
- `mu` Music files.
- `lo` Lossless music files.
- `cR` Compressed type files (like *.zip).
- `tX` Temporary type files (like *.tmp).
- `cm` Compiled files.
- `bu` Build files (like *.sln).
- `sc` Source code (like *.cpp).
- `cr` Cryptography files.

#### Special groups:

- `co` Compressed; includes both `cR` and `cT`.
- `tm` Temporary; includes both `tX` and `tT`.

#### Color element names:

These let you define colors of various elements of the output.

- `sn` File size (sets nb, nk, nm, ng and nt).
- `nb` File size if less than 1 KB.
- `nk` File size if between 1 KB and 1 MB.
- `nm` File size if between 1 MB and 1 GB.
- `ng` File size if between 1 GB and 1 TB.
- `nt` File size if 1 TB or greater.
- `sb` Units of a file’s size (sets ub, uk, um, ug and ut).
- `ub` Units of a file’s size if less than 1 KB.
- `uk` Units of a file’s size if between 1 KB and 1 MB.
- `um` Units of a file’s size if between 1 MB and 1 GB.
- `ug` Units of a file’s size if between 1 GB and 1 TB.
- `ut` Units of a file’s size if 1 TB or greater.
- `da` File date and time.
- `cF` File compression ratio.
- `oF` File owner.
- `ga` New file in git.
- `gm` Modified file in git.
- `gd` Deleted file in git.
- `gv` Renamed file in git.
- `gt` Type change in git.
- `gi` Ignored file in git.
- `gc` Unmerged file in git (conflicted).
- `Gm` Git repo main branch name.
- `Go` Git repo other branch name.
- `Gc` Git repo is clean.
- `Gd` Git repo is dirty.
- `xx` Punctuation (for example _ in the attributes field).
- `hM` The directory name with --mini-header.
- `ur` The read-only attribute letter in the attributes field.
- `su` The hidden attribute letter in the attributes field.
- `sf` The system attribute letter in the attributes field.
- `pi` The junction attribute letter in the attributes field.
- `lp` The symlink path when showing symlink targets.

The color element names above cannot be combined with types or patterns; they're for setting general colors, and don't affect how file names are colored.

#### Anything else:

Anything else in a condition is interpreted as a pattern to match against the file name.  The patterns use the `fnmatch` glob syntax, which is also used by [`.gitignore`](https://git-scm.com/docs/gitignore).

The most common use is to match a file extension, for example `*.txt` matches files whose name ends with `.txt`.


Patterns can include `?` and `*` wildcards, and can also include character sets enclosed in `[]`.  For example `ab[xyz]` matches `abx`, `aby`, and `abz`.  A set can include ranges, for example `[a-z]` matches any lowercase letter.  If the first character in the set is `!` or `^` the set excludes the listed characters, for example `[!0-9]` matches anything except digits.  The `?` character in a set matches any character but only if there's a character to match, for example `ab[?]` matches `abc`, but not `ab`.  Character sets can use `[:class:]` to specify a class of characters; _class_ can be one of `alnum`, `alpha`, `blank`, `cntrl`, `digit`, `graph`, `lower`, `print`, `punct`, `space`, `xdigit`, `upper`.

Anything quoted is treated as a pattern, for example `ro` refers to a file named `ro` instead of read-only files.

A pattern by itself with no types applies only to files, not directories.  To specify a pattern that matches anything, combine `di` and the pattern.

### COLOR syntax

Colors are the [SGR parameter](https://en.wikipedia.org/wiki/ANSI_escape_code#SGR) from [ANSI color escape codes](https://en.wikipedia.org/wiki/ANSI_escape_code).

The following color parameters are allowed.  Note that the color parameters are just sent to the terminal, and it is the terminal's responsibility to draw them properly.  Some parameters might not work in all terminal programs.

#### Styles:
Code | Description | &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; | Code | Description
-|-|-|-|-
`1` | bold (bright) | | `22` | not bold and not faint
`2` | faint
`3` | italic | | `23` | not italic
`4` | underline | | `24` | not underline (or double underline)
`7` | reverse | | `27` | not reverse
`9` | strikethrough | | `29` | not strikethrough
`21` | double underline
`53` | overline (line above) | | `55` | not overline

#### Text colors:
Code | Description | &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; | Code | Description
-|-|-|-|-
`39` | default text color
`30` | black | | `90` | bright black (dark gray)
`31` | dark red | | `91` | bright red
`32` | dark green | | `92` | bright green
`33` | dark yellow | | `93` | bright yellow
`34` | dark blue | | `94` | bright blue
`35` | dark magenta | | `95` | bright magenta
`36` | dark cyan | | `96` | bright cyan
`37` | dark white (gray) | | `97` | bright white

To make a background color, add 10 to the color values above.

#### Extended colors:

Code | Description
-|-
`38;2;n;n;n` | Specify a 24-bit color where each _n_ is from 0 to 255, and the order is _red_`;`_green_`;`_blue_.
`38;5;n` | Specify an 8-bit color where _n_ is from 0 to 255.  Refer to the wikipedia link above for details.

Run `dirx -? colorsamples` to display a chart of the 8-bit color codes and the available styles.

### Environment Variables

Set `NO_COLOR` to any non-empty value to completely disable colors.  See https://no-color.org/ for more info.

Set `DIRX_COLOR_SCALE` to any value accepted by the `--color-scale flag` to control the default behavior.  If it's not set, then `%EZA_COLOR_SCALE%` and `%EXA_COLOR_SCALE%` are also checked.

Set `DIRX_COLOR_SCALE_MODE` to any value accepted by the `--color-scale-mode` flag to control the default behavior.  If it's not set, then `%EZA_COLOR_SCALE_MODE%` and `%EXA_COLOR_SCALE_MODE%` are also checked.

Set `DIRX_MIN_LUMINANCE` to a value from -100 to 100 to control the range of intensity decay in the gradient color scale mode.  If it's not set, then `%EZA_MIN_LUMINANCE%` and `%EXA_MIN_LUMINANCE%` are also checked.

If the `DIRX_COLORS` environment variable is not set, then LS_COLORS is also checked.  DirX enhancements are ignored when parsing `%LS_COLORS%`.

## Icons

To see icons, a Nerd Font is required; visit https://nerdfonts.com for more info and to choose from many available fonts.

By default, a Nerd Fonts v3 font is assumed.

A few quick examples:

![image](iconsample.png)

Icons are selected based on the name of the file or folder, or the extension of the file name.  The icon mappings are hard-coded and aren't configurable at this time.

### Environment Variables

Set `DIRX_NERD_FONTS_VERSION=2` to select icons compatible with v2 Nerd Fonts.

Set `DIRX_ICON_SPACING=n` to specify the number of spaces to print after an icon.  _n_ can be from 1 to 4.  The default is 1, but some terminals or fonts may need a different number of spaces for icons to look good.  If it's not set, then `%EZA_ICON_SPACING%` and `%EXA_ICON_SPACING%` are checked as well.

## Format Pictures

Use the `-f` option to specify a custom format picture to list files.  Format picture field types and styles are case sensitive:  field types are uppercase and field styles are lowercase.

#### Examples:

Picture | Example
-|-
`"F  Sm"` | `Filename  3.2M`
`"Ds  Sm Trh F18"` | `10/04/09  13:45  123K r_ SomeVeryLongFilen…`

### Format picture field types

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
"		letters corresponding to the git file status.\n"
"  R[?]		Git repo field.  When listing a directory that is a git repo,\n"
"		this shows the status and branch name.\n"


Other characters in the format picture are output as-is.  To ensure a character is output as-is rather than being treated as a field type (etc), use `\` to escape the character (e.g. `"\F:F"` shows `F:MyFile.doc`).

For some field types `?` suppresses the field if the corresponding option was not specified.  When a field is suppressed, normally the field and the spaces immediately preceding it are omitted from the resulting format picture.  For finer control over what part of the format picture is omitted, surround the text to be omitted using the `[` and `]` characters.  Everything between the brackets will be shown or omitted as a unit, and the brackets themselves are not shown (e.g. `"F [(C?) ]Sm"` shows `File (12%) 1.2M` or `File 1.2M`).

The `-F` option fits the full path name into the filename field, if the field is at the end of the format picture.

The `-w` option fits as many files as possible onto each line, using the custom format picture.  The `-2` option is incompatible with format pictures (because it selects a specific format picture that's 38 characters wide).

The `-r` option forces a single column list, but fits the stream name into the filename field(s).  If the filename field is at the end of the format picture then the entire stream names are shown; otherwise the stream names are truncated to the width of the longest file name.

## Regular Expressions

Regular expressions specify a substring to find.  In addition, several special operators perform advanced searches.

To use a regular expression as a filename pattern, use `::` followed by the regular expression.  When specifying both a path and a regular expression, the `::` must precede the regular expression, for example `folder\\::regex`.  If the regular expression contains spaces or characters with special meaning to the shell (such as `|`, `<`, `>`) then enclose the expression in double quotes.

Note that regular expressions are not natively supported by the OS, so all filenames must be retrieved and compared with the regular expression.  This can make using regular expressions much slower than using `?` and `*` wildcards.

The regular expression is the [ECMAScript syntax](https://learn.microsoft.com/en-us/cpp/standard-library/regular-expressions-cpp).

Here is a quick summary; for details about the full syntax available, refer to the documentation link above.

Operator | Description
-|-
`^` | Matches the null string at the beginning of a line.
`$` | Matches the null string at the end of a line.
`.` | Matches any character.
`[set]` | Matches any characters in the set.  The `-` character selects a range of characters.  The expression `[a-z]` matches any lowercase letter from `a` to `z`.  To select the `]` character, put it at the beginning of the set.  To select the `-` character, put it at the end of the set.  To select the `^` character, put it anywhere except immediately after the opening bracket (which selects the complement of the set; see below)
`[^set]` |  any characters not in the set (matches the complement of the set).  The syntax for a complement set is the same as the syntax for a normal set (above), except that it is preceded by a `^` character.
`[:class:]` | Matches any character in the class (`alnum`, `alpha`, `blank`, `cntrl`, `digit`, `graph`, `lower`, `print`, `punct`, `space`, `upper`, `xdigit`).
`(abc)` | Matches `abc`, and tags it as a sub-expression for retrieval.
`abc\|def` | Matches `abc` or `def`.
`x?` | Matches 0 or 1 occurrences of the character `x` (or set, or tagged sub-expression).
`x+` | Matches 1 or more occurrences of the character `x` (or set, or tagged sub-expression).  `x+?` uses non-greedy repetition.
`x*` | Matches 0 or more occurrences of the character `x` (or set, or tagged sub-expression).  `x*?` uses non-greedy repitition.
`!x` | Matches anything but `x`.
`\b` | Matches the null string at a word boundary (`\babc\b` matches ` abc ` but not `abcx` or `xabc` or `xabcx`).
`\B` | Matches the null string not at a word boundary (`\Babc` matches `xabc` but not ` abc`).
`\x` | Some _x_ characters have special meaning (refer to the full documentation).  For other characters, `\x` literally matches the character _x_.  Some characters require `\x` to match them, including `^`, `$`, `.`, `[`, `(`, `)`, `\|`, `!`, `?`, `+`, `*`, and `\`.

## License

Copyright (c) 2024 by Christopher Antos
License: http://opensource.org/licenses/MIT

