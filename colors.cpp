// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// https://github.com/eza-community/eza
// Formula for adjusting luminance copied from eza.
// Many color type keys (e.g. "im" "do" etc) copied from eza.
// Many file extensions copied from eza.

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "colors.h"
#include "fileinfo.h"
#include "filesys.h"
#include "patterns.h"
#include "sorting.h"
#include "fields.h"
#include "output.h"
#include "wildmatch/wildmatch.h"

#include <math.h>
#include <unordered_map>
#include <cmath>

extern const WCHAR c_norm[] = L"\x1b[m";
extern const WCHAR c_error[] = L"1;91";

static double s_min_luminance = 0.4;
static DWORD s_attrs_for_colors = ~0;

static const WCHAR c_DIRX_COLORS[] = L"DIRX_COLORS";
static const WCHAR c_default_colors[] =
    // L"hi=91:"
    // L"sy=91:"
    L"di=1;33:"
    L"ln=1;34:"
    // L"ro ex=1;32:"
    L"*.patch=1;36:"
    L"*.diff=1;36:"
    L"*.dpk=1;36:"
    L"*.zip=36:"
    L"co=35:"
    L"ex=1:"
    // L"ro=32:"
    L"xx=90:"
    // L"nt=38;2;239;65;54:ng=38;2;252;176;64:nm=38;2;240;230;50:nk=38;2;142;198;64:nb=38;2;1;148;68:"
    // L"da=94:"
    // L"da=0:"
    L"lp=36:"
    L"su=1;35:sf=1;35:ur=32:"
    L"or=31:"
    L"ga=32:gm=34:gd=31:gv=33:gt=35:gi=90:gc=31:"
    L"Gm=32:Go=33:Gc=32:Gd=1;33:"
    ;

enum ColorIndex : unsigned short
{
    ciZERO,                     // Not applicable.

    // File attributes.
    ciArchiveAttribute,         // The ARCHIVE file attribute.
    ciCompressedAttribute,      // The COMPRESSED file attribute.
    ciDirectory,
    ciEncrypted,
    ciFile,
    ciHidden,
    ciLink,
    ciNotContentIndexed,
    ciOffline,
    ciReadonly,
    ciSparse,
    ciSystem,
    ciTemporaryAttribute,       // The TEMPORARY file attribute.

    // Sizes.
    ciSizeB,
    ciSizeK,
    ciSizeM,
    ciSizeG,
    ciSizeT,
    ciSizeUnitB,
    ciSizeUnitK,
    ciSizeUnitM,
    ciSizeUnitG,
    ciSizeUnitT,

    // Attribute letters.
    ciAttributeLetterReadonly,  // r
    ciAttributeLetterHidden,    // h
    ciAttributeLetterSystem,    // s
    ciAttributeLetterLink,      // j
                                // a --> eza directly uses ciFile
                                // d --> eza directly uses ciDirectory

    // Fields.
    ciSize,
    ciSizeUnit,
    ciTime,
    ciCompressionField,
    ciOwnerField,

    // Git.
    ciGitNew,
    ciGitModified,
    ciGitDeleted,
    ciGitRenamed,
    ciGitTypeChanged,
    ciGitIgnored,
    ciGitConflicted,
    ciGitMainBranch,
    ciGitOtherBranch,
    ciGitClean,
    ciGitDirty,

    // File extension groups.
    ciCompressedArchive,        // A compressed archive (for example, a .zip file).
    ciDocument,
    ciExecutable,
    ciImage,
    ciVideo,
    ciMusic,
    ciLossless,
    ciBuild,
    ciCrypto,
    ciSourceCode,
    ciCompiled,
    ciTemporaryExtension,       // Temporary file extensions.

    // Other.
    ciOrphan,
    ciCompressed,
    ciTemporary,
    ciLinkPath,
    ciPunctuation,
    ciMiniHeader,

    ciCOUNT,

    ciALLSIZES,                 // Pseudo-value to trigger setting ciSizeB, ciSizeK, etc.
    ciALLUNITS,                 // Pseudo-value to trigger setting ciSizeUnitK, ciSizeUnitM, etc.
};

enum ColorFlag : DWORD
{
    CFLAG_ZERO                  = 0,

    CFLAG_COMPRESSED_ATTRIBUTE  = 0x00000001,
    CFLAG_COMPRESSED_ARCHIVE    = 0x00000002,
    CFLAG_TEMPORARY_ATTRIBUTE   = 0x00000004,
    CFLAG_TEMPORARY_EXTENSION   = 0x00000008,

    CFLAG_EXECUTABLE            = 0x00000010,
    CFLAG_DOCUMENT              = 0x00000020,
    CFLAG_IMAGE                 = 0x00000040,
    CFLAG_VIDEO                 = 0x00000080,
    CFLAG_MUSIC                 = 0x00000100,
    CFLAG_LOSSLESS              = 0x00000200,

    CFLAG_CRYPTO                = 0x00001000,

    CFLAG_SOURCE_CODE           = 0x00010000,
    CFLAG_BUILD                 = 0x00020000,
    CFLAG_COMPILED              = 0x00040000,

    CFLAG_NOT_A_TYPE            = 0x80000000,   // For non-type colors (e.g. "da" for the date field).
};
DEFINE_ENUM_FLAG_OPERATORS(ColorFlag);

struct ExtensionInfo
{
    ColorFlag           flags;
    const WCHAR*        icon;
};

struct KeyInfo
{
    ColorFlag           flags;
    ColorIndex          ci;
};

static std::unordered_map<const WCHAR*, ExtensionInfo, HashCaseless, EqualCaseless> s_extensions;
static std::unordered_map<const WCHAR*, ExtensionInfo, HashCaseless, EqualCaseless> s_filenames;
static std::unordered_map<const WCHAR*, KeyInfo, HashCase, EqualCase> s_key_to_info;
static ColorIndex s_color_fallback[ciCOUNT];

static void InitColorMaps()
{
    ZeroMemory(&s_color_fallback, sizeof(s_color_fallback));
    s_color_fallback[ciCompressedAttribute] = ciCompressed;
    s_color_fallback[ciCompressedArchive] = ciCompressed;
    s_color_fallback[ciTemporaryAttribute] = ciTemporary;
    s_color_fallback[ciTemporaryExtension] = ciTemporary;
    s_color_fallback[ciAttributeLetterReadonly] = ciReadonly;
    s_color_fallback[ciAttributeLetterHidden] = ciHidden;
    s_color_fallback[ciAttributeLetterSystem] = ciSystem;
    s_color_fallback[ciAttributeLetterLink] = ciLink;
    // NOTE, there is intentionally no fallback for these fields:
    //  - ciCompressionField
    //  - ciOwnerField
    //  - ciSize
    //      "cF=" falls back to the file entry's color.
    //      "cF=0" uses no color.
    //      "cF=44" uses the specified color.
    //  - ciSizeB, ciSizeK, ciSizeM, ciSizeG, ciSizeT
    //      This allows different coloring for color scale.

    static const struct
    {
        const WCHAR* key;
        ExtensionInfo info;
    }
    c_extensions[] =
    {
        { L"bat",       { CFLAG_EXECUTABLE } },
        { L"cmd",       { CFLAG_EXECUTABLE } },
        { L"com",       { CFLAG_EXECUTABLE } },
        { L"exe",       { CFLAG_EXECUTABLE } },

        { L"djvu",      { CFLAG_DOCUMENT } },
        { L"doc",       { CFLAG_DOCUMENT } },
        { L"docx",      { CFLAG_DOCUMENT } },
        { L"eml",       { CFLAG_DOCUMENT } },
        { L"fotd",      { CFLAG_DOCUMENT } },
        { L"gdoc",      { CFLAG_DOCUMENT } },
        { L"key",       { CFLAG_DOCUMENT } },
        { L"keynote",   { CFLAG_DOCUMENT } },
        { L"md",        { CFLAG_DOCUMENT } },
        { L"numbers",   { CFLAG_DOCUMENT } },
        { L"odp",       { CFLAG_DOCUMENT } },
        { L"ods",       { CFLAG_DOCUMENT } },
        { L"odt",       { CFLAG_DOCUMENT } },
        { L"pages",     { CFLAG_DOCUMENT } },
        { L"pdf",       { CFLAG_DOCUMENT } },
        { L"ppt",       { CFLAG_DOCUMENT } },
        { L"pptx",      { CFLAG_DOCUMENT } },
        { L"rtf",       { CFLAG_DOCUMENT } },
        { L"xls",       { CFLAG_DOCUMENT } },
        { L"xlsm",      { CFLAG_DOCUMENT } },
        { L"xlsx",      { CFLAG_DOCUMENT } },

        { L"arw",       { CFLAG_IMAGE } },
        { L"avif",      { CFLAG_IMAGE } },
        { L"bmp",       { CFLAG_IMAGE } },
        { L"cbr",       { CFLAG_IMAGE } },
        { L"cbz",       { CFLAG_IMAGE } },
        { L"cr2",       { CFLAG_IMAGE } },
        { L"dvi",       { CFLAG_IMAGE } },
        { L"eps",       { CFLAG_IMAGE } },
        { L"gif",       { CFLAG_IMAGE } },
        { L"heic",      { CFLAG_IMAGE } },
        { L"heif",      { CFLAG_IMAGE } },
        { L"ico",       { CFLAG_IMAGE } },
        { L"j2c",       { CFLAG_IMAGE } },
        { L"j2k",       { CFLAG_IMAGE } },
        { L"jfi",       { CFLAG_IMAGE } },
        { L"jfif",      { CFLAG_IMAGE } },
        { L"jif",       { CFLAG_IMAGE } },
        { L"jp2",       { CFLAG_IMAGE } },
        { L"jpe",       { CFLAG_IMAGE } },
        { L"jpeg",      { CFLAG_IMAGE } },
        { L"jpf",       { CFLAG_IMAGE } },
        { L"jpg",       { CFLAG_IMAGE } },
        { L"jpx",       { CFLAG_IMAGE } },
        { L"jxl",       { CFLAG_IMAGE } },
        { L"nef",       { CFLAG_IMAGE } },
        { L"orf",       { CFLAG_IMAGE } },
        { L"pbm",       { CFLAG_IMAGE } },
        { L"pgm",       { CFLAG_IMAGE } },
        { L"png",       { CFLAG_IMAGE } },
        { L"pnm",       { CFLAG_IMAGE } },
        { L"ppm",       { CFLAG_IMAGE } },
        { L"ps",        { CFLAG_IMAGE } },
        { L"psd",       { CFLAG_IMAGE } },
        { L"pxm",       { CFLAG_IMAGE } },
        { L"raw",       { CFLAG_IMAGE } },
        { L"qoi",       { CFLAG_IMAGE } },
        { L"stl",       { CFLAG_IMAGE } },
        { L"svg",       { CFLAG_IMAGE } },
        { L"tif",       { CFLAG_IMAGE } },
        { L"tiff",      { CFLAG_IMAGE } },
        { L"webp",      { CFLAG_IMAGE } },
        { L"xcf",       { CFLAG_IMAGE } },
        { L"xpm",       { CFLAG_IMAGE } },

        { L"avi",       { CFLAG_VIDEO } },
        { L"flv",       { CFLAG_VIDEO } },
        { L"h264",      { CFLAG_VIDEO } },
        { L"heics",     { CFLAG_VIDEO } },
        { L"m2ts",      { CFLAG_VIDEO } },
        { L"m2v",       { CFLAG_VIDEO } },
        { L"m4v",       { CFLAG_VIDEO } },
        { L"mkv",       { CFLAG_VIDEO } },
        { L"mov",       { CFLAG_VIDEO } },
        { L"mp4",       { CFLAG_VIDEO } },
        { L"mpeg",      { CFLAG_VIDEO } },
        { L"mpg",       { CFLAG_VIDEO } },
        { L"ogm",       { CFLAG_VIDEO } },
        { L"ogv",       { CFLAG_VIDEO } },
        { L"video",     { CFLAG_VIDEO } },
        { L"vob",       { CFLAG_VIDEO } },
        { L"webm",      { CFLAG_VIDEO } },
        { L"wmv",       { CFLAG_VIDEO } },

        { L"aac",       { CFLAG_MUSIC } },
        { L"m4a",       { CFLAG_MUSIC } },
        { L"mka",       { CFLAG_MUSIC } },
        { L"mp2",       { CFLAG_MUSIC } },
        { L"mp3",       { CFLAG_MUSIC } },
        { L"ogg",       { CFLAG_MUSIC } },
        { L"opus",      { CFLAG_MUSIC } },
        { L"wma",       { CFLAG_MUSIC } },
        { L"aif",       { CFLAG_MUSIC|CFLAG_LOSSLESS } },
        { L"aifc",      { CFLAG_MUSIC|CFLAG_LOSSLESS } },
        { L"aiff",      { CFLAG_MUSIC|CFLAG_LOSSLESS } },
        { L"alac",      { CFLAG_MUSIC|CFLAG_LOSSLESS } },
        { L"ape",       { CFLAG_MUSIC|CFLAG_LOSSLESS } },
        { L"flac",      { CFLAG_MUSIC|CFLAG_LOSSLESS } },
        { L"pcm",       { CFLAG_MUSIC|CFLAG_LOSSLESS } },
        { L"wav",       { CFLAG_MUSIC|CFLAG_LOSSLESS } },
        { L"wv",        { CFLAG_MUSIC|CFLAG_LOSSLESS } },

        { L"7z",        { CFLAG_COMPRESSED_ARCHIVE } }, // 7-Zip
        { L"ar",        { CFLAG_COMPRESSED_ARCHIVE } },
        { L"arj",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"br",        { CFLAG_COMPRESSED_ARCHIVE } }, // Brotli
        { L"bz",        { CFLAG_COMPRESSED_ARCHIVE } }, // bzip
        { L"bz2",       { CFLAG_COMPRESSED_ARCHIVE } }, // bzip2
        { L"bz3",       { CFLAG_COMPRESSED_ARCHIVE } }, // bzip3
        { L"cab",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"cpio",      { CFLAG_COMPRESSED_ARCHIVE } },
        { L"deb",       { CFLAG_COMPRESSED_ARCHIVE } }, // Debian
        { L"dmg",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"gz",        { CFLAG_COMPRESSED_ARCHIVE } }, // gzip
        { L"iso",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"lz",        { CFLAG_COMPRESSED_ARCHIVE } },
        { L"lz4",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"lzh",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"lzma",      { CFLAG_COMPRESSED_ARCHIVE } },
        { L"lzo",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"phar",      { CFLAG_COMPRESSED_ARCHIVE } }, // PHP PHAR
        { L"qcow",      { CFLAG_COMPRESSED_ARCHIVE } },
        { L"qcow2",     { CFLAG_COMPRESSED_ARCHIVE } },
        { L"rar",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"rpm",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"tar",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"taz",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"tbz",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"tbz2",      { CFLAG_COMPRESSED_ARCHIVE } },
        { L"tc",        { CFLAG_COMPRESSED_ARCHIVE } },
        { L"tgz",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"tlz",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"txz",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"tz",        { CFLAG_COMPRESSED_ARCHIVE } },
        { L"xz",        { CFLAG_COMPRESSED_ARCHIVE } },
        { L"vdi",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"vhd",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"vhdx",      { CFLAG_COMPRESSED_ARCHIVE } },
        { L"vmdk",      { CFLAG_COMPRESSED_ARCHIVE } },
        { L"z",         { CFLAG_COMPRESSED_ARCHIVE } },
        { L"zip",       { CFLAG_COMPRESSED_ARCHIVE } },
        { L"zst",       { CFLAG_COMPRESSED_ARCHIVE } }, // Zstandard

        { L"ninja",         { CFLAG_BUILD } },
        { L"mak",           { CFLAG_BUILD } },
        { L"sln",           { CFLAG_BUILD } },

        { L"applescript",   { CFLAG_SOURCE_CODE } }, // Apple script
        { L"as",            { CFLAG_SOURCE_CODE } }, // Action script
        { L"asa",           { CFLAG_SOURCE_CODE } }, // asp
        { L"asm",           { CFLAG_SOURCE_CODE } }, // assembly
        { L"awk",           { CFLAG_SOURCE_CODE } }, // awk
        { L"c",             { CFLAG_SOURCE_CODE } }, // C/C++
        { L"c++" ,          { CFLAG_SOURCE_CODE } }, // C/C++
        { L"cabal",         { CFLAG_SOURCE_CODE } }, // Cabal
        { L"cc",            { CFLAG_SOURCE_CODE } }, // C/C++
        { L"clj",           { CFLAG_SOURCE_CODE } }, // Clojure
        { L"cp",            { CFLAG_SOURCE_CODE } }, // C/C++ Xcode
        { L"cpp",           { CFLAG_SOURCE_CODE } }, // C/C++
        { L"cr",            { CFLAG_SOURCE_CODE } }, // Crystal
        { L"cs",            { CFLAG_SOURCE_CODE } }, // C#
        { L"css",           { CFLAG_SOURCE_CODE } }, // css
        { L"csx",           { CFLAG_SOURCE_CODE } }, // C#
        { L"cu",            { CFLAG_SOURCE_CODE } }, // CUDA
        { L"cxx",           { CFLAG_SOURCE_CODE } }, // C/C++
        { L"cypher",        { CFLAG_SOURCE_CODE } }, // Cypher (query language)
        { L"d",             { CFLAG_SOURCE_CODE } }, // D
        { L"dart",          { CFLAG_SOURCE_CODE } }, // Dart
        { L"di",            { CFLAG_SOURCE_CODE } }, // D
        { L"dpr",           { CFLAG_SOURCE_CODE } }, // Delphi Pascal
        { L"el",            { CFLAG_SOURCE_CODE } }, // Lisp
        { L"elm",           { CFLAG_SOURCE_CODE } }, // Elm
        { L"erl",           { CFLAG_SOURCE_CODE } }, // Erlang
        { L"ex",            { CFLAG_SOURCE_CODE } }, // Elixir
        { L"exs",           { CFLAG_SOURCE_CODE } }, // Elixir
        { L"fs",            { CFLAG_SOURCE_CODE } }, // F#
        { L"fsh",           { CFLAG_SOURCE_CODE } }, // Fragment shader
        { L"fsi",           { CFLAG_SOURCE_CODE } }, // F#
        { L"fsx",           { CFLAG_SOURCE_CODE } }, // F#
        { L"go",            { CFLAG_SOURCE_CODE } }, // Go
        { L"gradle",        { CFLAG_SOURCE_CODE } }, // Gradle
        { L"groovy",        { CFLAG_SOURCE_CODE } }, // Groovy
        { L"gvy",           { CFLAG_SOURCE_CODE } }, // Groovy
        { L"h",             { CFLAG_SOURCE_CODE } }, // C/C++ header
        { L"h++" ,          { CFLAG_SOURCE_CODE } }, // C/C++ header
        { L"hpp",           { CFLAG_SOURCE_CODE } }, // C/C++ header
        { L"hs",            { CFLAG_SOURCE_CODE } }, // Haskell
        { L"htc",           { CFLAG_SOURCE_CODE } }, // JavaScript
        { L"hxx",           { CFLAG_SOURCE_CODE } }, // C/C++ header
        { L"inc",           { CFLAG_SOURCE_CODE } },
        { L"inl",           { CFLAG_SOURCE_CODE } }, // C/C++ Microsoft
        { L"ipynb",         { CFLAG_SOURCE_CODE } }, // Jupyter Notebook
        { L"java",          { CFLAG_SOURCE_CODE } }, // Java
        { L"jl",            { CFLAG_SOURCE_CODE } }, // Julia
        { L"js",            { CFLAG_SOURCE_CODE } }, // JavaScript
        { L"jsx",           { CFLAG_SOURCE_CODE } }, // React
        { L"kt",            { CFLAG_SOURCE_CODE } }, // Kotlin
        { L"kts",           { CFLAG_SOURCE_CODE } }, // Kotlin
        { L"kusto",         { CFLAG_SOURCE_CODE } }, // Kusto (query language)
        { L"less",          { CFLAG_SOURCE_CODE } }, // less
        { L"lhs",           { CFLAG_SOURCE_CODE } }, // Haskell
        { L"lisp",          { CFLAG_SOURCE_CODE } }, // Lisp
        { L"ltx",           { CFLAG_SOURCE_CODE } }, // LaTeX
        { L"lua",           { CFLAG_SOURCE_CODE } }, // Lua
        { L"m",             { CFLAG_SOURCE_CODE } }, // Matlab
        { L"malloy",        { CFLAG_SOURCE_CODE } }, // Malloy (query language)
        { L"matlab",        { CFLAG_SOURCE_CODE } }, // Matlab
        { L"ml",            { CFLAG_SOURCE_CODE } }, // OCaml
        { L"mli",           { CFLAG_SOURCE_CODE } }, // OCaml
        { L"mn",            { CFLAG_SOURCE_CODE } }, // Matlab
        { L"nb",            { CFLAG_SOURCE_CODE } }, // Mathematica
        { L"p",             { CFLAG_SOURCE_CODE } }, // Pascal
        { L"pas",           { CFLAG_SOURCE_CODE } }, // Pascal
        { L"php",           { CFLAG_SOURCE_CODE } }, // PHP
        { L"pl",            { CFLAG_SOURCE_CODE } }, // Perl
        { L"pm",            { CFLAG_SOURCE_CODE } }, // Perl
        { L"pod",           { CFLAG_SOURCE_CODE } }, // Perl
        { L"pp",            { CFLAG_SOURCE_CODE } }, // Puppet
        { L"prql",          { CFLAG_SOURCE_CODE } }, // PRQL
        { L"ps1",           { CFLAG_SOURCE_CODE } }, // PowerShell
        { L"psd1",          { CFLAG_SOURCE_CODE } }, // PowerShell
        { L"psm1",          { CFLAG_SOURCE_CODE } }, // PowerShell
        { L"purs",          { CFLAG_SOURCE_CODE } }, // PureScript
        { L"py",            { CFLAG_SOURCE_CODE } }, // Python
        { L"r",             { CFLAG_SOURCE_CODE } }, // R
        { L"rb",            { CFLAG_SOURCE_CODE } }, // Ruby
        { L"rs",            { CFLAG_SOURCE_CODE } }, // Rust
        { L"rq",            { CFLAG_SOURCE_CODE } }, // SPARQL (query language)
        { L"sass",          { CFLAG_SOURCE_CODE } }, // Sass
        { L"scala",         { CFLAG_SOURCE_CODE } }, // Scala
        { L"scss",          { CFLAG_SOURCE_CODE } }, // Sass
        { L"sql",           { CFLAG_SOURCE_CODE } }, // SQL
        { L"swift",         { CFLAG_SOURCE_CODE } }, // Swift
        { L"tcl",           { CFLAG_SOURCE_CODE } }, // TCL
        { L"tex",           { CFLAG_SOURCE_CODE } }, // LaTeX
        { L"ts",            { CFLAG_SOURCE_CODE } }, // TypeScript
        { L"v",             { CFLAG_SOURCE_CODE } }, // V
        { L"vb",            { CFLAG_SOURCE_CODE } }, // Visual Basic
        { L"vsh",           { CFLAG_SOURCE_CODE } }, // Vertex shader
        { L"zig",           { CFLAG_SOURCE_CODE } }, // Zig

        { L"a",             { CFLAG_COMPILED } }, // Unix static library
        { L"bundle",        { CFLAG_COMPILED } }, // macOS application bundle
        { L"class",         { CFLAG_COMPILED } }, // Java class file
        { L"cma",           { CFLAG_COMPILED } }, // OCaml bytecode library
        { L"cmi",           { CFLAG_COMPILED } }, // OCaml interface
        { L"cmo",           { CFLAG_COMPILED } }, // OCaml bytecode object
        { L"cmx",           { CFLAG_COMPILED } }, // OCaml bytecode object for inlining
        { L"dll",           { CFLAG_COMPILED } }, // Windows dynamic link library
        { L"dylib",         { CFLAG_COMPILED } }, // Mach-O dynamic library
        { L"elc",           { CFLAG_COMPILED } }, // Emacs compiled lisp
        { L"ko",            { CFLAG_COMPILED } }, // Linux kernel module
        { L"lib",           { CFLAG_COMPILED } }, // Windows static library
        { L"o",             { CFLAG_COMPILED } }, // Compiled object file
        { L"obj",           { CFLAG_COMPILED } }, // Compiled object file
        { L"pyc",           { CFLAG_COMPILED } }, // Python compiled code
        { L"pyd",           { CFLAG_COMPILED } }, // Python dynamic module
        { L"pyo",           { CFLAG_COMPILED } }, // Python optimized code
        { L"so",            { CFLAG_COMPILED } }, // Unix shared library
        { L"zwc",           { CFLAG_COMPILED } }, // zsh compiled file

        { L"asc",           { CFLAG_CRYPTO } }, // GnuPG ASCII armored file
        { L"cer",           { CFLAG_CRYPTO } },
        { L"cert",          { CFLAG_CRYPTO } },
        { L"crt",           { CFLAG_CRYPTO } },
        { L"csr",           { CFLAG_CRYPTO } }, // PKCS#10 Certificate Signing Request
        { L"gpg",           { CFLAG_CRYPTO } }, // GnuPG encrypted file
        { L"kbx",           { CFLAG_CRYPTO } }, // GnuPG keybox
        { L"md5",           { CFLAG_CRYPTO } }, // MD5 checksum
        { L"p12",           { CFLAG_CRYPTO } }, // PKCS#12 certificate (Netscape)
        { L"pem",           { CFLAG_CRYPTO } }, // Privacy-Enhanced Mail certificate
        { L"pfx",           { CFLAG_CRYPTO } }, // PKCS#12 certificate (Microsoft)
        { L"pgp",           { CFLAG_CRYPTO } }, // PGP security key
        { L"pub",           { CFLAG_CRYPTO } }, // Public key
        { L"sha1",          { CFLAG_CRYPTO } }, // SHA-1 hash
        { L"sha224",        { CFLAG_CRYPTO } }, // SHA-224 hash
        { L"sha256",        { CFLAG_CRYPTO } }, // SHA-256 hash
        { L"sha384",        { CFLAG_CRYPTO } }, // SHA-384 hash
        { L"sha512",        { CFLAG_CRYPTO } }, // SHA-512 hash
        { L"sig",           { CFLAG_CRYPTO } }, // GnuPG signed file
        { L"signature",     { CFLAG_CRYPTO } }, // e-Filing Digital Signature File (India)

        { L"bak",           { CFLAG_TEMPORARY_EXTENSION } },
        { L"bk",            { CFLAG_TEMPORARY_EXTENSION } },
        { L"bkp",           { CFLAG_TEMPORARY_EXTENSION } },
        { L"crdownload",    { CFLAG_TEMPORARY_EXTENSION } },
        { L"download",      { CFLAG_TEMPORARY_EXTENSION } },
        { L"fdmdownload",   { CFLAG_TEMPORARY_EXTENSION } },
        { L"part",          { CFLAG_TEMPORARY_EXTENSION } },
        { L"swn",           { CFLAG_TEMPORARY_EXTENSION } },
        { L"swo",           { CFLAG_TEMPORARY_EXTENSION } },
        { L"swp",           { CFLAG_TEMPORARY_EXTENSION } },
        { L"tmp",           { CFLAG_TEMPORARY_EXTENSION } },

        // More.
        { L"dlg",           { CFLAG_SOURCE_CODE } }, // MSVC dialog template file
        { L"idl",           { CFLAG_SOURCE_CODE } }, // MSVC interface definition language
        { L"mpe",           { CFLAG_VIDEO } },
        { L"odl",           { CFLAG_SOURCE_CODE } }, // MSVC object definition language
        { L"pch",           { CFLAG_COMPILED } }, // MSVC precompiled header
        { L"pdb",           { CFLAG_COMPILED } }, // MSVC symbol file
        { L"rc",            { CFLAG_SOURCE_CODE } }, // MSVC resource compiler file
        { L"vcxproj",       { CFLAG_BUILD } },
        { L"zoo",           { CFLAG_COMPRESSED_ARCHIVE } },
    };

    std::unordered_map<const WCHAR*, ExtensionInfo, HashCaseless, EqualCaseless> extensions;
    extensions.reserve(512);
    for (const auto& entry : c_extensions)
        extensions.emplace(entry.key, entry.info);

    StrW tmp;

    const WCHAR* pathext = _wgetenv(L"PATHEXT");
    if (pathext)
    {
        const auto& exe_iter = extensions.find(L"exe");
        const ExtensionInfo& exe_info = exe_iter->second;

        while (*pathext)
        {
            while (*pathext == ' ')
                ++pathext;
            const WCHAR* end = wcschr(pathext, ';');
            if (end)
                tmp.Set(pathext, unsigned(end - pathext));
            else
                tmp.Set(pathext);
            tmp.TrimRight();

            const WCHAR* key = tmp.Text();
            if (*key == '.')
            {
                ++key;
                auto& iter = extensions.find(key);
                if (iter != extensions.end())
                    iter->second.flags |= CFLAG_EXECUTABLE;
                else
                {
                    tmp.Detach();
                    extensions.emplace(key, exe_info);
                }
            }

            if (!end)
                break;
            pathext = end + 1;
        }
    }

    s_extensions = std::move(extensions);

    s_filenames = {
        { L"Brewfile", { CFLAG_BUILD } },
        { L"bsconfig.json", { CFLAG_BUILD } },
        { L"BUILD", { CFLAG_BUILD } },
        { L"BUILD.bazel", { CFLAG_BUILD } },
        { L"build.gradle", { CFLAG_BUILD } },
        { L"build.sbt", { CFLAG_BUILD } },
        { L"build.xml", { CFLAG_BUILD } },
        { L"Cargo.toml", { CFLAG_BUILD } },
        { L"CMakeLists.txt", { CFLAG_BUILD } },
        { L"composer.json", { CFLAG_BUILD } },
        { L"configure", { CFLAG_BUILD } },
        { L"Containerfile", { CFLAG_BUILD } },
        { L"Dockerfile", { CFLAG_BUILD } },
        { L"Earthfile", { CFLAG_BUILD } },
        { L"flake.nix", { CFLAG_BUILD } },
        { L"Gemfile", { CFLAG_BUILD } },
        { L"GNUmakefile", { CFLAG_BUILD } },
        { L"Gruntfile.coffee", { CFLAG_BUILD } },
        { L"Gruntfile.js", { CFLAG_BUILD } },
        { L"jamfile", { CFLAG_BUILD } },
        { L"jamrules", { CFLAG_BUILD } },
        { L"jsconfig.json", { CFLAG_BUILD } },
        { L"Justfile", { CFLAG_BUILD } },
        { L"justfile", { CFLAG_BUILD } },
        { L"Makefile", { CFLAG_BUILD } },
        { L"makefile", { CFLAG_BUILD } },
        { L"meson.build", { CFLAG_BUILD } },
        { L"mix.exs", { CFLAG_BUILD } },
        { L"package.json", { CFLAG_BUILD } },
        { L"Pipfile", { CFLAG_BUILD } },
        { L"PKGBUILD", { CFLAG_BUILD } },
        { L"Podfile", { CFLAG_BUILD } },
        { L"pom.xml", { CFLAG_BUILD } },
        { L"premake5.lua", { CFLAG_BUILD } },
        { L"Procfile", { CFLAG_BUILD } },
        { L"pyproject.toml", { CFLAG_BUILD } },
        { L"Rakefile", { CFLAG_BUILD } },
        { L"RoboFile.php", { CFLAG_BUILD } },
        { L"SConstruct", { CFLAG_BUILD } },
        { L"tsconfig.json", { CFLAG_BUILD } },
        { L"Vagrantfile", { CFLAG_BUILD } },
        { L"webpack.config.cjs", { CFLAG_BUILD } },
        { L"webpack.config.js", { CFLAG_BUILD } },
        { L"WORKSPACE", { CFLAG_BUILD } },

        { L"id_dsa", { CFLAG_CRYPTO } },
        { L"id_ecdsa", { CFLAG_CRYPTO } },
        { L"id_ecdsa_sk", { CFLAG_CRYPTO } },
        { L"id_ed25519", { CFLAG_CRYPTO } },
        { L"id_ed25519_sk", { CFLAG_CRYPTO } },
        { L"id_rsa", { CFLAG_CRYPTO } },
    };

    s_key_to_info = {
        { L"sn", { CFLAG_NOT_A_TYPE, ciALLSIZES } },    // the numbers of a file’s size (sets nb, nk, nm, ng and nt)
        { L"nb", { CFLAG_NOT_A_TYPE, ciSizeB } },       // the numbers of a file’s size if it is lower than 1 KB/Kib
        { L"nk", { CFLAG_NOT_A_TYPE, ciSizeK } },       // the numbers of a file’s size if it is between 1 KB/KiB and 1 MB/MiB
        { L"nm", { CFLAG_NOT_A_TYPE, ciSizeM } },       // the numbers of a file’s size if it is between 1 MB/MiB and 1 GB/GiB
        { L"ng", { CFLAG_NOT_A_TYPE, ciSizeG } },       // the numbers of a file’s size if it is between 1 GB/GiB and 1 TB/TiB
        { L"nt", { CFLAG_NOT_A_TYPE, ciSizeT } },       // the numbers of a file’s size if it is 1 TB/TiB or higher
        { L"sb", { CFLAG_NOT_A_TYPE, ciALLUNITS } },    // the units of a file’s size (sets ub, uk, um, ug and ut)
        { L"ub", { CFLAG_NOT_A_TYPE, ciSizeUnitB } },   // the units of a file’s size if it is lower than 1 KB/Kib
        { L"uk", { CFLAG_NOT_A_TYPE, ciSizeUnitK } },   // the units of a file’s size if it is between 1 KB/KiB and 1 MB/MiB
        { L"um", { CFLAG_NOT_A_TYPE, ciSizeUnitM } },   // the units of a file’s size if it is between 1 MB/MiB and 1 GB/GiB
        { L"ug", { CFLAG_NOT_A_TYPE, ciSizeUnitG } },   // the units of a file’s size if it is between 1 GB/GiB and 1 TB/TiB
        { L"ut", { CFLAG_NOT_A_TYPE, ciSizeUnitT } },   // the units of a file’s size if it is 1 TB/TiB or higher
        // NOTE: eza documents "ub", but the library it uses to render size
        // units never prints any unit name for bytes, so it's impossible for
        // that color to ever actually get used.

        { L"da", { CFLAG_NOT_A_TYPE, ciTime } },        // a file date

        { L"ga", { CFLAG_NOT_A_TYPE, ciGitNew } },
        { L"gm", { CFLAG_NOT_A_TYPE, ciGitModified } },
        { L"gd", { CFLAG_NOT_A_TYPE, ciGitDeleted } },
        { L"gv", { CFLAG_NOT_A_TYPE, ciGitRenamed } },
        { L"gt", { CFLAG_NOT_A_TYPE, ciGitTypeChanged } },
        { L"gi", { CFLAG_NOT_A_TYPE, ciGitIgnored } },
        { L"gc", { CFLAG_NOT_A_TYPE, ciGitConflicted } },

        { L"Gm", { CFLAG_NOT_A_TYPE, ciGitMainBranch } },
        { L"Go", { CFLAG_NOT_A_TYPE, ciGitOtherBranch } },
        { L"Gc", { CFLAG_NOT_A_TYPE, ciGitClean } },
        { L"Gd", { CFLAG_NOT_A_TYPE, ciGitDirty } },

        { L"lp", { CFLAG_NOT_A_TYPE, ciLinkPath } },
        { L"or", { CFLAG_NOT_A_TYPE, ciOrphan } },
        // bO : the overlay style for broken symlink paths

        { L"ex", { CFLAG_EXECUTABLE, ciExecutable } },
        { L"do", { CFLAG_DOCUMENT, ciDocument } },
        { L"im", { CFLAG_IMAGE, ciImage } },
        { L"vi", { CFLAG_VIDEO, ciVideo } },
        { L"mu", { CFLAG_MUSIC|CFLAG_LOSSLESS, ciMusic } },
        { L"lo", { CFLAG_LOSSLESS, ciLossless } },
        { L"co", { CFLAG_COMPRESSED_ATTRIBUTE|CFLAG_COMPRESSED_ARCHIVE, ciCompressed } },
        { L"tm", { CFLAG_TEMPORARY_ATTRIBUTE|CFLAG_TEMPORARY_EXTENSION, ciTemporary } },
        { L"cm", { CFLAG_COMPILED, ciCompiled } },
        { L"bu", { CFLAG_BUILD, ciBuild } },
        { L"sc", { CFLAG_SOURCE_CODE, ciSourceCode } },
        { L"cr", { CFLAG_CRYPTO, ciCrypto } },

        { L"xx", { CFLAG_NOT_A_TYPE, ciPunctuation } },

        //---- ADDED BY DIRX -------------------------------------------------
        { L"hM", { CFLAG_NOT_A_TYPE, ciMiniHeader } }, // the mini header for a directory ("c:\foo:")
        { L"cF", { CFLAG_NOT_A_TYPE, ciCompressionField } }, // the compression ratio field
        { L"oF", { CFLAG_NOT_A_TYPE, ciOwnerField } },  // the owner field
        // { L"cT", { CFLAG_COMPRESSED_ATTRIBUTE, ciCompressedAttribute } },
        { L"cR", { CFLAG_COMPRESSED_ARCHIVE, ciCompressedArchive } },
        // { L"tT", { CFLAG_TEMPORARY_ATTRIBUTE, ciTemporaryAttribute } },
        { L"tX", { CFLAG_TEMPORARY_EXTENSION, ciTemporaryExtension } },

        //---- EZA REPURPOSED THESE TYPES ON WINDOWS AS FOLLOWS --------------
        { L"ur", { CFLAG_NOT_A_TYPE, ciAttributeLetterReadonly } },
        { L"su", { CFLAG_NOT_A_TYPE, ciAttributeLetterHidden } },
        { L"sf", { CFLAG_NOT_A_TYPE, ciAttributeLetterSystem } },
        { L"pi", { CFLAG_NOT_A_TYPE, ciAttributeLetterLink } },

        //---- IGNORE FOR LS_COLORS COMPATIBILITY ----------------------------
        { L"so", { CFLAG_ZERO, ciZERO } },
        { L"bd", { CFLAG_ZERO, ciZERO } },
        { L"cd", { CFLAG_ZERO, ciZERO } },

        //---- NOT APPLICABLE ------------------------------------------------
        // hd : the header row of a table
        // cc : an escaped character in a filename
        // cr : a regular file that is related to cryptography (ex: key or certificate)
    };
}

static WCHAR* s_color_strings[ciCOUNT];

static ColorIndex ColorIndexFromColorFlag(ColorFlag flags)
{
    static const struct
    {
        ColorIndex ci;
        ColorFlag flag;
    }
    c_priority_order[] =
    {
        { ciCompressedAttribute, CFLAG_COMPRESSED_ATTRIBUTE },
        { ciTemporaryAttribute, CFLAG_TEMPORARY_ATTRIBUTE },
        { ciExecutable, CFLAG_EXECUTABLE },
        { ciDocument, CFLAG_DOCUMENT },
        { ciImage, CFLAG_IMAGE },
        { ciVideo, CFLAG_VIDEO },
        { ciLossless, CFLAG_LOSSLESS },
        { ciMusic, CFLAG_MUSIC },
        { ciSourceCode, CFLAG_SOURCE_CODE },
        { ciCompiled, CFLAG_COMPILED },
        { ciBuild, CFLAG_BUILD },
        { ciCompressedArchive, CFLAG_COMPRESSED_ARCHIVE },
        { ciTemporaryExtension, CFLAG_TEMPORARY_EXTENSION },
    };

    if (flags)
        for (const auto& element : c_priority_order)
        {
            if ((element.flag & flags) == flags && s_color_strings[element.ci])
                return element.ci;
        }

    return ciFile;
}

static bool s_link_target_color = false;

bool UseLinkTargetColor()
{
    return s_link_target_color;
}

static void SetColorString(ColorIndex ci, WCHAR* color)
{
    assert(ci > ciZERO);
    assert(ci < ciCOUNT);
    if (ci > ciZERO && ci < ciCOUNT)
    {
        free(s_color_strings[ci]);
        s_color_strings[ci] = color;
    }
}

static const WCHAR* GetColorWithFallback(ColorIndex ci)
{
    assert(ci > ciZERO);
    assert(ci < ciCOUNT);
    const WCHAR* seq;
    if (ci > ciZERO && ci < ciCOUNT)
        seq = s_color_strings[ci];
    else
        seq = nullptr;

    if (!seq)
    {
        ci = s_color_fallback[ci];
        seq = s_color_strings[ci];
    }

    return seq;
}

// Returns the delimiter character that was encountered, if any.
// Returns -1 on error.
//
// The following backslash escape sequences are supported.  Readline thinks
// LS_COLORS supports a lot more, but it seems to me all the others are
// nonsensical and don't have real world use cases (ok, \040 or \x20 as
// alternate representations for SPACE, but until I see those used in the real
// world, I'm not going to add support for them).
//      \\      ->  BACKSLASH.
//      \"      ->  QUOTE.
//      \_      ->  SPACE.
//      \       ->  SPACE (BACKSLASH followed by SPACE).
//      \...    ->  Any other character following BACKSLASH is an error.
//
// Use / or \\ for path separators in wildmatch patterns (the same syntax as
// .gitignore uses).
//
// Spaces can be embedded three ways:
//      - Surround the string with quotes:      "abc def"
//      - Use the \_ escape sequence:           abc\_def
//      - Use \ followed by a space:            abc\ def
static int GetToken(const WCHAR*& in, StrW& out, const WCHAR* delims, Error& e)
{
    out.Clear();

    // Skip leading whitespace.
    while (*in == ' ' || *in == '\t')
        ++in;

    enum { STATE_TEXT, STATE_QUOTE, STATE_BACKSLASH } state = STATE_TEXT;

    const WCHAR* syntax = in;
    while (true)
    {
        const WCHAR* const ptr = in;
        const WCHAR c = *in;

        if (c)
            ++in;

        switch (state)
        {
        case STATE_TEXT:
            switch (c)
            {
            case 0:
            case ':':
                return c;
            case '\"':
                state = STATE_QUOTE;
                syntax = ptr;
                goto LAppend;
            case '\\':
                state = STATE_BACKSLASH;
                syntax = ptr;
                goto LAppend;
            default:
                for (const WCHAR* d = delims; *d; ++d)
                    if (*d == c)
                        return c;
LAppend:
                out.Append(ptr, unsigned(in - ptr));
                break;
            }
            break;

        case STATE_QUOTE:
            if (c == 0)
            {
                e.Set(L"Missing end quote: '%2'.") << syntax;
                return -1;
            }
            if (c == '\"')
                state = STATE_TEXT;
            goto LAppend;

        case STATE_BACKSLASH:
            switch (c)
            {
            case 0:
                out.Append('\\');
                return c;
            case '_':
            case ' ':
                out.Append('\\');
                out.Append(' ');
                break;
            case '\\':
            case '\"':
                out.Append('\\');
                out.Append(ptr, 1);
                break;
            default:
                {
                    WCHAR tmp[2] = { c };
                    e.Set(L"Syntax error: '\\%2' is not supported.") << tmp;
                    return -1;
                }
            }
            state = STATE_TEXT;
            break;
        }
    }
}

static int GetConditionAndValue(const WCHAR*& in, StrW& condition, StrW& value, Error& e)
{
    const WCHAR* const orig_in = in;

    int end = GetToken(in, condition, L":=", e);
    if (end < 0)
        return -1;

    end = GetToken(in, value, L":", e);
    if (end < 0)
        return -1;

    if (wcschr(value.Text(), '='))
    {
        e.Set(L"Syntax error: value '%1' contains '=', is a ':' missing?") << value.Text();
        return -1;
    }

    condition.TrimRight();
    value.TrimRight();
    return !condition.Empty();
}

static bool GetSpacedToken(const WCHAR*& in, StrW& out, bool& quoted)
{
    out.Clear();
    quoted = false;

    const WCHAR* begin;
    for (begin = in; *begin && *begin == ' '; ++begin) {}

    if (*begin == '!')
    {
        out.Set(L"!");
        ++in;
        return true;
    }

    const WCHAR* end;
    for (end = begin; *end && *end != ' '; ++end)
    {
        if (*end == '\"')
        {
            const WCHAR* find;
            for (find = end; *find && *find != '\"'; ++find) {}
            end = find;
            quoted = true;
        }
        else if (*end == '\\')
        {
            if (end[1] == ' ' || end[1] == '_' || end[1] == '\\' || end[1] == '\"')
                ++end;
        }
    }

    if (quoted)
    {
        for (; begin < end; ++begin)
        {
            if (*begin != '\"')
                out.Append(begin, 1);
        }
    }
    else
    {
        out.Set(begin, end - begin);
    }

    in = end;

    return !out.Empty();
}

struct ColorPattern
{
    StrW m_pattern;                         // Wildmatch pattern to compare.
    bool m_not;                             // Use the inverse of whether it matches.
};

struct ColorRule
{
    DWORD m_attr = 0;                       // File attributes that must be set.
    DWORD m_not_attr = 0;                   // File attributes that must NOT be set.
    ColorFlag m_flags = CFLAG_ZERO;         // Color flags that must be set.
    ColorFlag m_not_flags = CFLAG_ZERO;     // Color flags that must NOT be set.
    std::vector<ColorPattern> m_patterns;   // Wildmatch patterns to match.
    StrW m_color;                           // The color to output when matched.
};

static std::vector<ColorRule> s_color_rules;

struct AttributeName
{
    const DWORD attr;
    const ColorIndex ci;
    int level;              // Support level: 0=ls+eza+dirx, 1=eza+dirx, 2=dirx.
    const WCHAR* key;       // The color lookup key.
};

static const AttributeName c_attributes[] =
{
    { FILE_ATTRIBUTE_ARCHIVE,               ciArchiveAttribute,     2,  L"ar" },
    { FILE_ATTRIBUTE_COMPRESSED,            ciCompressedAttribute,  2,  L"cT" },
    { FILE_ATTRIBUTE_DIRECTORY,             ciDirectory,            0,  L"di" },
    { FILE_ATTRIBUTE_ENCRYPTED,             ciEncrypted,            2,  L"en" },
    { FILE_ATTRIBUTE_NORMAL,                ciFile,                 0,  L"fi" },
    { FILE_ATTRIBUTE_HIDDEN,                ciHidden,               2,  L"hi" },
    { FILE_ATTRIBUTE_REPARSE_POINT,         ciLink,                 0,  L"ln" },
    { FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,   ciNotContentIndexed,    2,  L"NI" },
    { FILE_ATTRIBUTE_OFFLINE,               ciOffline,              2,  L"of" },
    { FILE_ATTRIBUTE_READONLY,              ciReadonly,             2,  L"ro" },
    { FILE_ATTRIBUTE_SPARSE_FILE,           ciSparse,               2,  L"SP" },
    { FILE_ATTRIBUTE_SYSTEM,                ciSystem,               2,  L"sy" },
    { FILE_ATTRIBUTE_TEMPORARY,             ciTemporaryAttribute,   2,  L"tT" },
};

static int ParseColorRule(const WCHAR* in, StrW& value, ColorRule& rule, int level, Error& e)
{
    unsigned num_attr = 0;
    int ci = -1;

    StrW token;
    StrW pseudo_type;
    bool ever_any = false;
    bool not = false;
    while (*in)
    {
        bool quoted = false;
        if (level >= 2)
            GetSpacedToken(in, token, quoted);
        else
            token = in, in += wcslen(in);
        if (token.Empty())
            continue;

        if (!pseudo_type.Empty())
        {
            e.Set(L"'%1' can only be set by itself.") << pseudo_type.Text();
            return -1;
        }

        if (level >= 2 && (token.EqualI(L"not") || token.Equal(L"!")))
        {
            not = true;
            continue;
        }

        bool found = false;
        if (!quoted)
        {
            for (const auto& a : c_attributes)
            {
                if (level >= a.level && token.Equal(a.key))
                {
                    ++num_attr;
                    ci = a.ci;
                    if (not)
                        rule.m_not_attr |= a.attr;
                    else
                        rule.m_attr |= a.attr;
                    found = true;
                    break;
                }
            }

#if 0
            if (!found && token.EqualI(L"any"))
            {
                rule.m_attr = 0;
                rule.m_not_attr = 0;
                rule.m_flags = CFLAG_ZERO;
                rule.m_not_flags = CFLAG_ZERO;
                num_attr = 0;
                ci = -1;
                ever_any = true;
                found = true;
            }
#endif

            if (!found && level >= 2)
            {
                const auto& info = s_key_to_info.find(token.Text());
                if (info != s_key_to_info.end())
                {
                    if (info->second.flags != CFLAG_ZERO)
                    {
                        ++num_attr;
                        ci = info->second.ci;
                        if (not)
                            rule.m_not_flags |= info->second.flags;
                        else
                            rule.m_flags |= info->second.flags;
                        found = true;

                        if (info->second.flags == CFLAG_NOT_A_TYPE)
                        {
                            if (not)
                            {
                                e.Set(L"Cannot negate '%1'.") << token.Text();
                                return -1;
                            }
                            pseudo_type.Set(token);
                        }
                    }
                }
            }
        }

        if (!found)
        {
            ColorPattern pat;
            pat.m_not = not;
            pat.m_pattern = std::move(token);
            rule.m_patterns.emplace_back(std::move(pat));
        }

        not = false;
    }

    // Readonly by itself implies NOT DIRECTORY so by itself it only applies
    // to files, but can be explicitly applied to dirs/links/etc.
    DWORD not_attr_mask = ~0;
    if (rule.m_attr == FILE_ATTRIBUTE_READONLY && rule.m_patterns.empty() && num_attr == 1)
    {
        rule.m_not_attr |= FILE_ATTRIBUTE_DIRECTORY;
        not_attr_mask &= ~FILE_ATTRIBUTE_DIRECTORY;
    }

    // A pattern with no attributes implies NOT DIRECTORY so it only applies
    // to files, but can be explicitly applied to dirs/links/etc.
    if (rule.m_patterns.size() && !rule.m_attr && !rule.m_not_attr && !ever_any)
        rule.m_not_attr |= FILE_ATTRIBUTE_DIRECTORY;

    if (num_attr == 1 && !(rule.m_not_attr & not_attr_mask) && !rule.m_not_flags && rule.m_patterns.empty())
    {
        if (ci == ciLink)
        {
            s_link_target_color = (value.EqualI(L"target") || value.Empty());
            if (s_link_target_color)
            {
                SetColorString(ciLink, nullptr);
                return 0;
            }
        }
        if (ci > ciZERO && ValidateColor(value.Text()) >= 0)
        {
            assert(rule.m_attr || rule.m_flags);
            WCHAR* color = value.Empty() ? nullptr : value.Detach();
            if (ci == ciALLSIZES)
            {
                SetColorString(ciSizeB, CopyStr(color));
                SetColorString(ciSizeK, CopyStr(color));
                SetColorString(ciSizeM, CopyStr(color));
                SetColorString(ciSizeG, CopyStr(color));
                SetColorString(ciSizeT, CopyStr(color));
                ci = ciSize;
            }
            else if (ci == ciALLUNITS)
            {
                SetColorString(ciSizeUnitB, CopyStr(color));
                SetColorString(ciSizeUnitK, CopyStr(color));
                SetColorString(ciSizeUnitM, CopyStr(color));
                SetColorString(ciSizeUnitG, CopyStr(color));
                SetColorString(ciSizeUnitT, CopyStr(color));
                ci = ciSizeUnit;
            }
            SetColorString(ColorIndex(ci), color);
        }
        return 0;
    }

    if (rule.m_attr || rule.m_not_attr || rule.m_patterns.size())
    {
        rule.m_color = std::move(value);
        return 1;
    }

    return 0;
}

static void ParseColors(const WCHAR* colors, const WCHAR* error_context, int level, Error& e)
{
    if (!colors || !*colors)
        return;

    if (wcscmp(colors, L"*") == 0)
        colors = c_default_colors;

    StrW token;
    StrW value;
    while (*colors)
    {
        const int ok = GetConditionAndValue(colors, token, value, e);
        if (ok < 0)
        {
            e.Set(L"Unparsable value for %1 string.") << error_context;
            return;
        }
        if (ok > 0)
        {
            ColorRule rule;
            int result = ParseColorRule(token.Text(), value, rule, level, e);
            if (result < 0)
                return;
            if (result > 0)
                s_color_rules.emplace_back(std::move(rule));
        }
    }
}

static bool StartsWithReset(const WCHAR*& p)
{
    if (p && *p)
    {
        Error e;
        StrW token;
        const WCHAR* in = p;
        GetToken(in, token, L":", e);
        if (token.EqualI(L"reset"))
        {
            p = in;
            return true;
        }
    }
    return false;
}

void ReportColorlessError(Error& e)
{
    if (e.Test())
    {
        StrW tmp;
        e.Format(tmp);
        tmp.Append(L"\n");
        // Use normal text color instead of error color.
        OutputConsole(GetStdHandle(STD_ERROR_HANDLE), tmp.Text(), tmp.Length());
        e.Clear();
    }
}

void InitColors(const WCHAR* custom)
{
    Error e;

    InitColorMaps();

    if (!StartsWithReset(custom))
    {
        const WCHAR* dirx_colors = _wgetenv(c_DIRX_COLORS);
        if (!StartsWithReset(dirx_colors))
        {
            ParseColors(L"*", L"Default Colors", 2, e);
            ReportColorlessError(e);
            ParseColors(_wgetenv(L"LS_COLORS"), L"LS_COLORS", 0, e);
            ReportColorlessError(e);
        }

        ParseColors(dirx_colors, c_DIRX_COLORS, 2, e);
        ReportColorlessError(e);
    }

    ParseColors(custom, L"--more-colors", 2, e);
    ReportColorlessError(e);

    const WCHAR* env = _wgetenv(L"DIRX_MIN_LUMINANCE");
    if (!env)
    {
        env = _wgetenv(L"EZA_MIN_LUMINANCE");
        if (!env)
            env = _wgetenv(L"EXA_MIN_LUMINANCE");
    }
    if (env)
    {
        const int x = clamp(_wtoi(env), -100, 100);
        s_min_luminance = double(x) / 100;
    }
}

void SetAttrsForColors(DWORD attrs_for_colors)
{
    s_attrs_for_colors = attrs_for_colors;
}

const WCHAR* LookupColor(const FileInfo* pfi, const WCHAR* dir, bool ignore_target_color)
{
    assert(dir); // Otherwise can't check for orphaned symlinks.

    DWORD attr = pfi->GetAttributes() & s_attrs_for_colors;
    const StrW& long_name = pfi->GetLongName();
    const WCHAR* name = long_name.Text();

    unsigned short mode = 0;
    if (attr & FILE_ATTRIBUTE_DIRECTORY)
        mode = S_IFDIR;
    else
        mode = S_IFREG;

    if (pfi->IsReparseTag())
    {
        if (!s_link_target_color && !ignore_target_color)
            mode |= S_IFLNK;

        if (dir) // Invalid...but don't crash if bug gets accidentally released.
        {
            StrW fullname;
            PathJoin(fullname, dir, long_name);

            struct _stat64 st;
            if (_wstat64(fullname.Text(), &st) < 0)
                mode &= ~(S_IFDIR|S_IFREG);
        }
    }

    return LookupColor(name, attr, mode);
}

const WCHAR* LookupColor(const WCHAR* name, DWORD attr, unsigned short mode)
{
    StrW tmp;
    if (attr & FILE_ATTRIBUTE_DIRECTORY)
    {
        const unsigned len = unsigned(wcslen(name));
        if (len && IsPathSeparator(name[len - 1]))
        {
            // It's safe to blindly strip trailing slashes because there's
            // no way to show a root directory in a directory listing.
            tmp.Set(name, len);
            StripTrailingSlashes(tmp);
            name = tmp.Text();
        }
    }

    // Identify flags for matching, and identify default color type.
    ColorIndex ci;
    ColorFlag flags = CFLAG_ZERO;
    if (!(mode & (S_IFDIR|S_IFREG)) && s_color_strings[ciOrphan])
    {
        ci = ciOrphan; // Dangling symlink.
        attr = 0;
    }
    else
    {
        if (S_ISLNK(mode) && !s_link_target_color)
        {
            assert(attr & FILE_ATTRIBUTE_REPARSE_POINT);
            ci = ciLink;
        }
        else if (S_ISREG(mode))
        {
            assert(!(attr & FILE_ATTRIBUTE_DIRECTORY));
            ci = ciFile;
            // Using FILE_ATTRIBUTE_NORMAL to mean "FILE" even though that's
            // not quite what it really means as far as the OS is concerned.
            attr |= FILE_ATTRIBUTE_NORMAL;

            const WCHAR* ext = FindExtension(name);
            if (ext && *ext == '.')
            {
                ++ext;
                const auto& iter = s_extensions.find(ext);
                if (iter != s_extensions.end())
                    flags = iter->second.flags;
            }
        }
        else if (S_ISDIR(mode))
        {
            assert(attr & FILE_ATTRIBUTE_DIRECTORY);
            ci = ciDirectory;
        }
        else
        {
            ci = ciOrphan;
            attr = 0;
        }

        if ((attr & FILE_ATTRIBUTE_TEMPORARY) && (s_color_strings[ciTemporaryAttribute] || s_color_strings[ciTemporary]))
            ci = ciTemporaryAttribute;
        else if ((attr & FILE_ATTRIBUTE_COMPRESSED) && (s_color_strings[ciCompressedAttribute] || s_color_strings[ciCompressed]))
            ci = ciCompressedAttribute;

        if ((attr & FILE_ATTRIBUTE_READONLY) && s_color_strings[ciReadonly])
            ci = ciReadonly;

        // Hidden takes precedence over Readonly, if no rules match.
        if ((attr & FILE_ATTRIBUTE_HIDDEN) && s_color_strings[ciHidden])
            ci = ciHidden;

        // Directory takes precedence over Readonly, if no rules match.
        if ((attr & FILE_ATTRIBUTE_DIRECTORY) && ci == ciReadonly)
            ci = ciDirectory;
    }

    // Look for a matching rule.  First match wins.
    const int bits = WM_CASEFOLD|WM_SLASHFOLD|WM_WILDSTAR;
    StrW no_trailing_sep;
    StrW only_name;
    for (const auto& rule : s_color_rules)
    {
        // Try to match attributes.
        if (rule.m_attr && (attr & rule.m_attr) != rule.m_attr)
            goto next_rule;
        if (rule.m_not_attr && (attr & rule.m_not_attr) != 0)
            goto next_rule;

        // Try to match flags.
        if (rule.m_flags && (flags & rule.m_flags) != rule.m_flags)
            goto next_rule;
        if (rule.m_not_flags && (flags & rule.m_not_flags) != rule.m_not_flags)
            goto next_rule;

        // Try to match patterns.
        for (const auto& pat : rule.m_patterns)
        {
            const WCHAR* n = name;

            if (attr & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (no_trailing_sep.Empty())
                {
                    no_trailing_sep.Set(name);
                    StripTrailingSlashes(no_trailing_sep);
                }
                n = no_trailing_sep.Text();
            }

            if (only_name.Empty())
                only_name.Set(FindName(n));
            n = only_name.Text();

            if (wildmatch(pat.m_pattern.Text(), n, bits) != (pat.m_not ? WM_NOMATCH : WM_MATCH))
                goto next_rule;
        }

        // Match found!
        return rule.m_color.Text();

next_rule:
        continue;
    }

    if (ci == ciFile)
    {
        if (_wcsnicmp(name, L"readme", 6) == 0 && s_color_strings[ciBuild])
            ci = ciBuild;
        else
            ci = ColorIndexFromColorFlag(flags);

        if (ci == ciFile)
        {
            const WCHAR* only_name = FindName(name);
            const auto& fninfo = s_filenames.find(only_name);
            if (fninfo != s_filenames.end())
                ci = ColorIndexFromColorFlag(fninfo->second.flags);

            if (ci == ciFile)
            {
                const WCHAR last = *only_name ? only_name[wcslen(only_name) - 1] : '\0';
                if (last == '~' || (last == '#' && *only_name == '#'))
                    ci = ciTemporaryExtension;
            }
        }
    }

    return GetColorWithFallback(ci);
}

const WCHAR* GetAttrLetterColor(DWORD attr)
{
    if (!attr)
        return GetColorByKey(L"xx");

    assert(!(attr & (attr - 1)));

    // Some of these attribute letter color mappings are different than the
    // mappings for file colors based on file attribute.  This makes it
    // possible to color the attribute letters independently from the file
    // names for the same attribute, or the fallback colors make it possible
    // to make the letters and file name colors identical if desired.
    ColorIndex ci = ciZERO;
    switch (attr)
    {
    case FILE_ATTRIBUTE_READONLY:               ci = ciAttributeLetterReadonly; break;
    case FILE_ATTRIBUTE_HIDDEN:                 ci = ciAttributeLetterHidden; break;
    case FILE_ATTRIBUTE_SYSTEM:                 ci = ciAttributeLetterSystem; break;
    case FILE_ATTRIBUTE_DIRECTORY:              ci = ciDirectory; break;
    case FILE_ATTRIBUTE_ARCHIVE:                ci = ciFile; break;
    case FILE_ATTRIBUTE_NORMAL:                 ci = ciFile; break;
    case FILE_ATTRIBUTE_TEMPORARY:              ci = ciTemporaryAttribute; break;
    case FILE_ATTRIBUTE_SPARSE_FILE:            ci = ciSparse; break;
    case FILE_ATTRIBUTE_REPARSE_POINT:          ci = ciAttributeLetterLink; break;
    case FILE_ATTRIBUTE_COMPRESSED:             ci = ciCompressedAttribute; break;
    case FILE_ATTRIBUTE_OFFLINE:                ci = ciOffline; break;
    case FILE_ATTRIBUTE_NOT_CONTENT_INDEXED:    ci = ciNotContentIndexed; break;
    case FILE_ATTRIBUTE_ENCRYPTED:              ci = ciEncrypted; break;

    // case FILE_ATTRIBUTE_DEVICE:
    // case FILE_ATTRIBUTE_INTEGRITY_STREAM:
    // case FILE_ATTRIBUTE_VIRTUAL:
    // case FILE_ATTRIBUTE_NO_SCRUB_DATA:
    // case FILE_ATTRIBUTE_EA:
    // case FILE_ATTRIBUTE_PINNED:
    // case FILE_ATTRIBUTE_UNPINNED:
    // case FILE_ATTRIBUTE_RECALL_ON_OPEN:
    // case FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS:
    }

    if (!ci)
        return nullptr;

    return GetColorWithFallback(ci);
}

const WCHAR* GetColorByKey(const WCHAR* key)
{
    if (key)
    {
        const auto& info = s_key_to_info.find(key);
        if (info != s_key_to_info.end())
            return GetColorWithFallback(info->second.ci);
    }
    return nullptr;
}

const WCHAR* GetSizeColor(ULONGLONG ull)
{
    ColorIndex ci;
    if (!(GetColorScaleFields() & SCALE_SIZE))
        ci = ciSize;
    else if (ull < 1024ull)
        ci = ciSizeB;
    else if (ull < 1024ull * 1024)
        ci = ciSizeK;
    else if (ull < 1024ull * 1024 * 1024)
        ci = ciSizeM;
    else if (ull < 1024ull * 1024 * 1024 * 1024)
        ci = ciSizeG;
    else
        ci = ciSizeT;
    return s_color_strings[ci];
}

const WCHAR* GetSizeUnitColor(ULONGLONG ull)
{
    ColorIndex ci;
    if (!(GetColorScaleFields() & SCALE_SIZE))
        ci = ciSizeUnit;
    else if (ull < 1024ull)
        ci = ciSizeUnitB;
    else if (ull < 1024ull * 1024)
        ci = ciSizeUnitK;
    else if (ull < 1024ull * 1024 * 1024)
        ci = ciSizeUnitM;
    else if (ull < 1024ull * 1024 * 1024 * 1024)
        ci = ciSizeUnitG;
    else
        ci = ciSizeUnitT;
    return s_color_strings[ci];
}

static COLORREF RgbFromColorTable(BYTE value)
{
    static CONSOLE_SCREEN_BUFFER_INFOEX s_infoex;
    static bool s_have = false;

    static const BYTE c_ansi_to_vga[] =
    {
        0,  4,  2,  6,  1,  5,  3,  7,
        8, 12, 10, 14,  9, 13, 11, 15,
    };

    if (!s_have)
    {
        COLORREF table[] =
        {
            // Windows Terminal doesn't implement GetConsoleScreenBufferInfoEx
            // yet, and returns a default table instead.  But it can return a
            // version of the default table with the R and B values swapped.
            RGB(0x0c, 0x0c, 0x0c),
            RGB(0xda, 0x37, 0x00),
            RGB(0x0e, 0xa1, 0x13),
            RGB(0xdd, 0x96, 0x3a),
            RGB(0x1f, 0x0f, 0xc5),
            RGB(0x98, 0x17, 0x88),
            RGB(0x00, 0x9c, 0xc1),
            RGB(0xcc, 0xcc, 0xcc),
            RGB(0x76, 0x76, 0x76),
            RGB(0xff, 0x78, 0x3b),
            RGB(0x0c, 0xc6, 0x16),
            RGB(0xd6, 0xd6, 0x61),
            RGB(0x56, 0x48, 0xe7),
            RGB(0x9e, 0x00, 0xb4),
            RGB(0xa5, 0xf1, 0xf9),
            RGB(0xf2, 0xf2, 0xf2),
        };

        s_infoex.cbSize = sizeof(s_infoex);
        if (!GetConsoleScreenBufferInfoEx(GetStdHandle(STD_OUTPUT_HANDLE), &s_infoex))
        {
            static_assert(sizeof(s_infoex.ColorTable) == sizeof(table), "table sizes do not match!");
            memcpy(s_infoex.ColorTable, table, sizeof(s_infoex.ColorTable));
            for (auto& rgb : s_infoex.ColorTable)
                rgb = RGB(GetBValue(rgb), GetGValue(rgb), GetRValue(rgb));
            s_infoex.wAttributes = 0x07;
        }
        else if (memcmp(s_infoex.ColorTable, table, sizeof(s_infoex.ColorTable)) == 0)
        {
            for (auto& rgb : s_infoex.ColorTable)
                rgb = RGB(GetBValue(rgb), GetGValue(rgb), GetRValue(rgb));
        }

        s_have = true;
    }

    if (value == 49)
        value = (s_infoex.wAttributes & 0xf0) >> 4;
    else if (value < 0 || value >= 16)
        value = s_infoex.wAttributes & 0x0f;
    else
        value = c_ansi_to_vga[value];
    return s_infoex.ColorTable[value];
}

static bool ParseNum(const WCHAR*& p, DWORD& num)
{
    num = 0;
    while (*p && *p != ';')
    {
        if (!iswdigit(*p))
            return false;
        num *= 10;
        num += *p - '0';
        if (num >= 256)
            return false;
        ++p;
    }
    return true;
}

static COLORREF RgbFromColor(const WCHAR* color, bool prefer_background=false)
{
    static const BYTE c_cube_series[] = { 0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff };

    unsigned format = 0;    // 5=8-bit, 2=24-bit, 0=30..37,39
    DWORD value = 39;
    bool bold = false;
    bool bg = false;

    bool start = true;
    int num = 0;
    for (const WCHAR* p = color; true; ++p)
    {
        if (!*p || *p == ';')
        {
            switch (num)
            {
            case 0:
                format = 0;
                value = 39;
                bold = false;
                break;
            case 1:
                bold = true;
                break;
            case 22:
                bold = false;
                break;
            case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
            case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
            case 39:
                if (!bg)
                {
                    format = 0;
                    value = num;
                }
                break;
            case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
            case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
            case 49:
                if (prefer_background)
                {
                    format = 0;
                    value = num;
                    bg = true;
                }
                break;
            }

            if (!*p)
                break;

            start = true;
            num = 0;
            continue;
        }

        if (start && ((wcsncmp(p, L"38;2;", 5) == 0) ||
                      (prefer_background && wcsncmp(p, L"48;2;", 5) == 0)))
        {
            bg = (*p == '4');
            p += 5;
            DWORD r, g, b;
            if (!ParseNum(p, r) || !*(p++))
                return 0xffffffff;
            if (!ParseNum(p, g) || !*(p++))
                return 0xffffffff;
            if (!ParseNum(p, b))
                return 0xffffffff;
            format = 2;
            value = RGB(BYTE(r), BYTE(g), BYTE(b));
            num = -1;
            --p; // Counteract the loop.
        }
        else if (start && ((wcsncmp(p, L"38;5;", 5) == 0) ||
                           (prefer_background && wcsncmp(p, L"48;2;", 5) == 0)))
        {
            bg = (*p == '4');
            p += 5;
            if (!ParseNum(p, value))
                return 0xffffffff;
            format = 5;
            num = -1;
            --p; // Counteract the loop.
        }
        else if (iswdigit(*p))
        {
            num *= 10;
            num += *p - '0';
        }
        else
            return 0xffffffff;

        start = false;
    }

    switch (format)
    {
    case 5:
        // 8-bit color.
        assert(value >= 0 && value <= 255);
        if (value <= 15)
            return RgbFromColorTable(BYTE(value));
        else if (value >= 232 && value <= 255)
        {
            const BYTE gray = 8 + ((BYTE(value) - 232) * 10);
            return RGB(gray, gray, gray);
        }
        else
        {
            value -= 16;
            const BYTE r = BYTE(value) / 36;
            value -= r * 36;
            const BYTE g = BYTE(value) / 6;
            value -= g * 6;
            const BYTE b = BYTE(value);
            return RGB(c_cube_series[r],
                       c_cube_series[g],
                       c_cube_series[b]);
        }
        break;

    case 2:
        // 24-bit color.
        return value;

    default:
        // 4-bit color.
        if (value >= 30 && value <= 37)
            return RgbFromColorTable(BYTE(value) - 30 + (bold && !bg ? 8 : 0));
        else if (value >= 90 && value <= 97)
            return RgbFromColorTable(BYTE(value) - 90 + 8);
        else if (value == 39 || value == 49)
            return RgbFromColorTable(BYTE(value));
        else if (value >= 40 && value <= 47)
            return RgbFromColorTable(BYTE(value) - 40 + (bold && !bg ? 8 : 0));
        else if (value >= 100 && value <= 107)
            return RgbFromColorTable(BYTE(value) - 100 + 8);
        assert(false);
        return 0xffffffff;
    }
}

const WCHAR* GetIconColor(const WCHAR* color)
{
    if (!color)
        return nullptr;

    COLORREF rgb = RgbFromColor(color, true);
    if (rgb == 0xffffffff)
        return color;

    static StrW s_color;
    s_color.Clear();
    s_color.Printf(L"38;2;%u;%u;%u", GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
    return s_color.Text();
}

namespace colorspace
{

// The Oklab code here is based on https://bottosson.github.io/posts/oklab, in
// the public domain (and also available under the MIT License).

struct Oklab
{
    Oklab() = default;
    Oklab(COLORREF cr) { from_rgb(cr); }

    void from_rgb(COLORREF cr);
    COLORREF to_rgb() const;

    float L = 0;
    float a = 0;
    float b = 0;

    inline static float rgb_to_linear(BYTE val)
    {
        float x = float(val) / 255.0f;
        return (x > 0.04045f) ? std::pow((x + 0.055f) / 1.055f, 2.4f) : (x / 12.92f);
    }

    inline static BYTE linear_to_rgb(float val)
    {
        float x = (val >= 0.0031308f) ? (1.055f * std::pow(val, 1.0f / 2.4f) - 0.055f) : (12.92f * val);
        return BYTE(clamp(int(x * 255), 0, 255));
    }
};

void Oklab::from_rgb(COLORREF cr)
{
    float _r = rgb_to_linear(GetRValue(cr));
    float _g = rgb_to_linear(GetGValue(cr));
    float _b = rgb_to_linear(GetBValue(cr));

    float l = 0.4122214708f * _r + 0.5363325363f * _g + 0.0514459929f * _b;
    float m = 0.2119034982f * _r + 0.6806995451f * _g + 0.1073969566f * _b;
    float s = 0.0883024619f * _r + 0.2817188376f * _g + 0.6299787005f * _b;

    l = std::cbrt(l);
    m = std::cbrt(m);
    s = std::cbrt(s);

    L = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    a = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    b = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
}

COLORREF Oklab::to_rgb() const
{
    float l = L + 0.3963377774f * a + 0.2158037573f * b;
    float m = L - 0.1055613458f * a - 0.0638541728f * b;
    float s = L - 0.0894841775f * a - 1.2914855480f * b;

    l = l * l * l;
    m = m * m * m;
    s = s * s * s;

    float _r = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    float _g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    float _b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;

    return RGB(linear_to_rgb(_r), linear_to_rgb(_g), linear_to_rgb(_b));
}

}; // namespace colorspace

const WCHAR* ApplyGradient(const WCHAR* color, ULONGLONG value, ULONGLONG min, ULONGLONG max)
{
    assert(color);

    COLORREF rgb = RgbFromColor(color);
    if (rgb == 0xffffffff || min > max)
        return color;

    // This formula for applying a gradient effect is borrowed from eza.
    // https://github.com/eza-community/eza/blob/626eb34df26376fc36758894424676ffa4363785/src/output/color_scale.rs#L201-L213
    {
        colorspace::Oklab oklab(rgb);

        double ratio = double(value - min) / double(max - min);
        if (std::isnan(ratio))
            ratio = 1.0;
        oklab.L = float(clamp(s_min_luminance + (1.0 - s_min_luminance) * exp(-4.0 * (1.0 - ratio)), 0.0, 1.0));

        rgb = oklab.to_rgb();
    }

    static StrW s_color;
    s_color.Set(color);
    if (*color)
        s_color.Append(';');
    s_color.Printf(L"38;2;%u;%u;%u", GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
    return s_color.Text();
}

const WCHAR* StripLineStyles(const WCHAR* color)
{
    if (!color)
        return color;

    static StrW s_tmp;
    s_tmp.Clear();

    const WCHAR* start = color;
    unsigned num = 0;
    int skip = 0;
    bool any_stripped = false;
    for (const WCHAR* p = color; true; ++p)
    {
        if (!*p || *p == ';')
        {
            bool strip = false;

            if (skip < 0)
            {
                if (num == 2)
                    skip = 3;
                else if (num == 5)
                    skip = 1;
                else
                    skip = 0;
            }
            else if (skip > 0)
            {
                --skip;
            }
            else
            {
                switch (num)
                {
                case 4:     // Underline.
                case 9:     // Strikethrough.
                case 21:    // Double underline.
                case 53:    // Overline.
                    strip = true;
                    break;
                case 38:
                case 48:
                case 58:
                    skip = -1;
                    break;
                }
            }

            if (strip)
                any_stripped = true;
            else
                s_tmp.Append(start, unsigned(p - start));

            if (!*p)
                break;

            start = p;
            num = 0;
            continue;
        }

        if (iswdigit(*p))
        {
            num *= 10;
            num += *p - '0';
        }
        else
            return L"";
    }

    return any_stripped ? s_tmp.Text() : color;
}

