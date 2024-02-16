// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// https://github.com/eza-community/eza
// Many icons copied from eza.

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "icons.h"
#include "filesys.h"
#include "patterns.h"

#include <unordered_map>

static unsigned s_nerd_font_version_index = 0;  // Reverse order:  0=v3, 1=v2.

enum class Icons
{
    AUDIO,
    BINARY,
    BOOK,
    CALENDAR,
    CLOCK,
    COMPRESSED,
    CONFIG,
    CSS3,
    DATABASE,
    DIFF,
    DISK_IMAGE,
    DOCKER,
    DOCUMENT,
    DOWNLOAD,
    EMACS,
    ESLINT,
    FILE,
    FILE_OUTLINE,
    FOLDER,
    FOLDER_CONFIG,
    FOLDER_GIT,
    FOLDER_GITHUB,
    FOLDER_HIDDEN,
    FOLDER_KEY,
    FOLDER_NPM,
    FOLDER_OPEN,
    FONT,
    GIST_SECRET,
    GIT,
    GRADLE,
    GRUNT,
    GULP,
    HTML5,
    IMAGE,
    INTELLIJ,
    JSON,
    KEY,
    KEYPASS,
    LANG_ASSEMBLY,
    LANG_C,
    LANG_CPP,
    LANG_CSHARP,
    LANG_D,
    LANG_ELIXIR,
    LANG_FORTRAN,
    LANG_FSHARP,
    LANG_GO,
    LANG_GROOVY,
    LANG_HASKELL,
    LANG_JAVA,
    LANG_JAVASCRIPT,
    LANG_KOTLIN,
    LANG_OCAML,
    LANG_PERL,
    LANG_PHP,
    LANG_PYTHON,
    LANG_R,
    LANG_RUBY,
    LANG_RUBYRAILS,
    LANG_RUST,
    LANG_SASS,
    LANG_STYLUS,
    LANG_TEX,
    LANG_TYPESCRIPT,
    LANG_V,
    LIBRARY,
    LICENSE,
    LOCK,
    MAKE,
    MARKDOWN,
    MUSTACHE,
    NODEJS,
    NPM,
    OS_ANDROID,
    OS_APPLE,
    OS_LINUX,
    OS_WINDOWS,
    OS_WINDOWS_CMD,
    PLAYLIST,
    POWERSHELL,
    PRIVATE_KEY,
    PUBLIC_KEY,
    RAZOR,
    REACT,
    README,
    SHEET,
    SHELL,
    SHELL_CMD,
    SHIELD_CHECK,
    SHIELD_KEY,
    SHIELD_LOCK,
    SIGNED_FILE,
    SLIDE,
    SUBLIME,
    SUBTITLE,
    TERRAFORM,
    TEXT,
    TYPST,
    UNITY,
    VECTOR,
    VIDEO,
    VIM,
    WRENCH,
    XML,
    YAML,
    YARN,

    // Directories.
    FOLDER_TRASH,
    FOLDER_CONTACTS,
    FOLDER_DESKTOP,
    FOLDER_DOWNLOADS,
    FOLDER_FAVORITES,
    FOLDER_HOME,
    FOLDER_MAIL,
    FOLDER_MOVIES,
    FOLDER_MUSIC,
    FOLDER_PICTURES,
    FOLDER_VIDEO,

    // Filenames.
    ATOM,
    GITLAB,
    SSH,
    EARTHFILE,
    HEROKU,
    JENKINS,
    PKGBUILD,
    MAVEN,
    PROCFILE,
    ROBOTS,
    VAGRANT,
    WEBPACK,

    // Extensions.
    ACF,
    AI,
    CLJ,
    CLJS,
    COFFEE,
    CR,
    CU,
    DART,
    DEB,
    DESKTOP,
    DRAWIO,
    EBUILD,
    EJS,
    ELM,
    EML,
    ENV,
    ERL,
    GFORM,
    GV,
    HAML,
    IPYNB,
    JL,
    LESS,
    LISP,
    LOG,
    LUA,
    MAGNET,
    MID,
    NINJA,
    NIX,
    ORG,
    OUT_EXT,
    PDF,
    PKG,
    PP,
    PSD,
    PURS,
    RDB,
    RPM,
    RSS,
    SCALA,
    SERVICE,
    SLN,
    SQLITE3,
    SVELTE,
    SWIFT,
    TORRENT,
    TWIG,
    VUE,
    ZIG,

    // More.
    INFO,
    HISTORY,

    __COUNT
};

struct NerdFontIcon
{
    const WCHAR* nfv[2];
};

const WCHAR* ____ = reinterpret_cast<const WCHAR*>(size_t(-1));

static const NerdFontIcon c_common_icons[] =
{
    { L"",      },     // AUDIO
    { L"", L"" },     // BINARY
    { L"",      },     // BOOK
    { L"", L"" },     // CALENDAR
    { L"",      },     // CLOCK
    { L"",      },     // COMPRESSED
    { L"",      },     // CONFIG
    { L"",      },     // CSS3
    { L"",      },     // DATABASE
    { L"",      },     // DIFF
    { L"",      },     // DISK_IMAGE
    { L"", L"" },     // DOCKER
    { L"",      },     // DOCUMENT
    { L"󰇚", L"" },     // DOWNLOAD
    { L"", ____ },     // EMACS
    { L"", ____ },     // ESLINT
    { L"", L"" },     // FILE
    { L"", L"" },     // FILE_OUTLINE
    { L"", L"" },     // FOLDER
    { L"",      },     // FOLDER_CONFIG
    { L"",      },     // FOLDER_GIT
    { L"",      },     // FOLDER_GITHUB
    { L"󱞞", ____ },     // FOLDER_HIDDEN
    { L"󰢬", ____ },     // FOLDER_KEY
    { L"",      },     // FOLDER_NPM
    { L"",      },     // FOLDER_OPEN
    { L"",      },     // FONT
    { L"", ____ },     // GIST_SECRET
    { L"",      },     // GIT
    { L"", ____ },     // GRADLE
    { L"",      },     // GRUNT
    { L"",      },     // GULP
    { L"",      },     // HTML5
    { L"",      },     // IMAGE
    { L"",      },     // INTELLIJ
    { L"",      },     // JSON
    { L"", L"" },     // KEY
    { L"",      },     // KEYPASS
    { L"", ____ },     // LANG_ASSEMBLY
#if 1
    { L"",      },     // LANG_C
    { L"",      },     // LANG_CPP
    { L"󰌛", L"" },     // LANG_CSHARP
#else
    { L"c", L"c" },     // LANG_C
    { L"󰙲", L"ﭱ" },     // LANG_CPP
    { L"󰌛", L"" },     // LANG_CSHARP
#endif
    { L"",      },     // LANG_D
    { L"",      },     // LANG_ELIXIR
    { L"󱈚", ____ },     // LANG_FORTRAN
    { L"",      },     // LANG_FSHARP
    { L"", ____ },     // LANG_GO
    { L"",      },     // LANG_GROOVY
    { L"",      },     // LANG_HASKELL
    { L"",      },     // LANG_JAVA
    { L"",      },     // LANG_JAVASCRIPT
    { L"", ____ },     // LANG_KOTLIN
    { L"", ____ },     // LANG_OCAML
    { L"", ____ },     // LANG_PERL
    { L"",      },     // LANG_PHP
    { L"",      },     // LANG_PYTHON
    { L"", L"ﳒ" },     // LANG_R
    { L"",      },     // LANG_RUBY
    { L"",      },     // LANG_RUBYRAILS
    { L"", L"" },     // LANG_RUST
    { L"",      },     // LANG_SASS
    { L"",      },     // LANG_STYLUS
    { L"", ____ },     // LANG_TEX
    { L"",      },     // LANG_TYPESCRIPT
    { L"", ____ },     // LANG_V
    { L"", L"" },     // LIBRARY
    { L"",      },     // LICENSE
    { L"",      },     // LOCK
    { L"", ____ },     // MAKE
    { L"",      },     // MARKDOWN
    { L"",      },     // MUSTACHE
    { L"",      },     // NODEJS
    { L"",      },     // NPM
    { L"",      },     // OS_ANDROID
    { L"",      },     // OS_APPLE
    { L"",      },     // OS_LINUX
    { L"",      },     // OS_WINDOWS
    { L"", L"" },     // OS_WINDOWS_CMD
    { L"󰲹", ____ },     // PLAYLIST
    { L"", L"" },     // POWERSHELL
    { L"󰌆", L"" },     // PRIVATE_KEY
    { L"󰷖", L"" },     // PUBLIC_KEY
    { L"",      },     // RAZOR
    { L"",      },     // REACT
    { L"󰂺", L"" },     // README
    { L"",      },     // SHEET
    { L"󱆃", L"#" },     // SHELL
    { L"",      },     // SHELL_CMD
    { L"󰕥", ____ },     // SHIELD_CHECK
    { L"󰯄", ____ },     // SHIELD_KEY
    { L"󰦝", ____ },     // SHIELD_LOCK
    { L"󱧃", ____ },     // SIGNED_FILE
    { L"",      },     // SLIDE
    { L"",      },     // SUBLIME
    { L"󰨖", ____ },     // SUBTITLE
    { L"󱁢", ____ },     // TERRAFORM
    { L"",      },     // TEXT
    { L"𝐭",      },     // TYPST
    { L"",      },     // UNITY
    { L"󰕙", L"縉" },     // VECTOR
    { L"",      },     // VIDEO
    { L"",      },     // VIM
    { L"",      },     // WRENCH
    { L"󰗀", L"" },     // XML
    { L"", L"!" },     // YAML
    { L"", ____ },     // YARN

    // Directories.
    { L"",      },     // FOLDER_TRASH
    { L"",      },     // FOLDER_CONTACTS
    { L"",      },     // FOLDER_DESKTOP
    { L"󰉍", L"" },     // FOLDER_DOWNLOADS
    { L"󰚝", L"ﮛ" },     // FOLDER_FAVORITES
    { L"󱂵", L"" },     // FOLDER_HOME
    { L"󰇰", L"" },     // FOLDER_MAIL
    { L"󰿎", L"" },     // FOLDER_MOVIES
    { L"󱍙", L"" },     // FOLDER_MUSIC
    { L"󰉏", L"" },     // FOLDER_PICTURES
    { L"",      },     // FOLDER_VIDEO

    // Filenames.
    { L"",      },     // ATOM
    { L"",      },     // GITLAB
    { L"󰣀", ____ },     // SSH
    { L"",      },     // EARTHFILE
    { L"",      },     // HEROKU
    { L"", ____ },     // JENKINS
    { L"",      },     // PKGBUILD
    { L"", ____ },     // MAVEN
    { L"",      },     // PROCFILE
    { L"󰚩", L"ﮧ" },     // ROBOTS
    { L"⍱",      },     // VAGRANT
    { L"󰜫", L"ﰩ" },     // WEBPACK

    // Extensions.
    { L"",      },     // ACF
    { L"",      },     // AI
    { L"",      },     // CLJ
    { L"",      },     // CLJS
    { L"",      },     // COFFEE
    { L"", ____ },     // CR
    { L"", ____ },     // CU
    { L"",      },     // DART
    { L"",      },     // DEB
    { L"", ____ },     // DESKTOP
    { L"", ____ },     // DRAWIO
    { L"",      },     // EBUILD
    { L"",      },     // EJS
    { L"",      },     // ELM
    { L"",      },     // EML
    { L"",      },     // ENV
    { L"",      },     // ERL
    { L"",      },     // GFORM
    { L"󱁉", ____ },     // GV
    { L"", ____ },     // HAML
    { L"", ____ },     // IPYNB
    { L"",      },     // JL
    { L"",      },     // LESS
    { L"󰅲", L"" },     // LISP
    { L"",      },     // LOG
    { L"",      },     // LUA
    { L"",      },     // MAGNET
    { L"󰣲", ____ },     // MID
    { L"󰝴", L"ﱲ" },     // NINJA
    { L"",      },     // NIX
    { L"", ____ },     // ORG
    { L"", ____ },     // OUT_EXT
    { L"",      },     // PDF
    { L"", L"" },     // PKG
    { L"", ____ },     // PP
    { L"",      },     // PSD
    { L"", ____ },     // PURS
    { L"",      },     // RDB
    { L"",      },     // RPM
    { L"",      },     // RSS
    { L"",      },     // SCALA
    { L"", ____ },     // SERVICE
    { L"",      },     // SLN
    { L"",      },     // SQLITE3
    { L"", ____ },     // SVELTE
    { L"",      },     // SWIFT
    { L"",      },     // TORRENT
    { L"",      },     // TWIG
    { L"󰡄", L"﵂" },     // VUE
    { L"", ____ },     // ZIG

    // More.
    { L"󰋼", L"" },     // INFO
    { L"", L"" },     // HISTORY
};

static_assert(size_t(Icons::__COUNT) == _countof(c_common_icons), "mismatched number of icons!");

static const WCHAR* GetIcon(Icons i)
{
    const WCHAR* icon;
    icon = c_common_icons[size_t(i)].nfv[s_nerd_font_version_index];
    if (!icon)
    {
        assert(s_nerd_font_version_index == 1);
        icon = c_common_icons[size_t(i)].nfv[0];
    }
    if (icon == ____)
        return nullptr;
    return icon;
}

struct IconMapping
{
    const WCHAR* name;
    Icons icon;
};

static const WCHAR* GetDirectoryIcon(const WCHAR* name)
{
    static const std::unordered_map<const WCHAR*, Icons, HashCase, EqualCase> c_directory_icons =
    {
        { L".config",               Icons::FOLDER_CONFIG },
        { L".git",                  Icons::FOLDER_GIT },
        { L".github",               Icons::FOLDER_GITHUB },
        { L".npm",                  Icons::FOLDER_NPM },
        { L".ssh",                  Icons::FOLDER_KEY },
        { L".Trash",                Icons::FOLDER_TRASH },
        { L"config",                Icons::FOLDER_CONFIG },
        { L"Contacts",              Icons::FOLDER_CONTACTS },
        { L"cron.d",                Icons::FOLDER_CONFIG },
        { L"cron.daily",            Icons::FOLDER_CONFIG },
        { L"cron.hourly",           Icons::FOLDER_CONFIG },
        { L"cron.monthly",          Icons::FOLDER_CONFIG },
        { L"cron.weekly",           Icons::FOLDER_CONFIG },
        { L"Desktop",               Icons::FOLDER_DESKTOP },
        { L"Downloads",             Icons::FOLDER_DOWNLOADS },
        { L"etc",                   Icons::FOLDER_CONFIG },
        { L"Favorites",             Icons::FOLDER_FAVORITES },
        { L"hidden",                Icons::FOLDER_HIDDEN },
        { L"home",                  Icons::FOLDER_HOME },
        { L"include",               Icons::FOLDER_CONFIG },
        { L"Mail",                  Icons::FOLDER_MAIL },
        { L"Movies",                Icons::FOLDER_MOVIES },
        { L"Music",                 Icons::FOLDER_MUSIC },
        { L"node_modules",          Icons::FOLDER_NPM },
        { L"npm_cache",             Icons::FOLDER_NPM },
        { L"pam.d",                 Icons::FOLDER_KEY },
        { L"Pictures",              Icons::FOLDER_PICTURES },
        { L"ssh",                   Icons::FOLDER_KEY },
        { L"sudoers.d",             Icons::FOLDER_KEY },
        { L"Videos",                Icons::FOLDER_VIDEO },
        { L"xbps.d",                Icons::FOLDER_CONFIG },
        { L"xorg.conf.d",           Icons::FOLDER_CONFIG },
    };

    const auto& iter = c_directory_icons.find(name);
    if (iter != c_directory_icons.end())
        return GetIcon(iter->second);

    return nullptr;
}

static const WCHAR* GetFilenameIcon(const WCHAR* name)
{
    static const std::unordered_map<const WCHAR*, Icons, HashCase, EqualCase> c_filename_icons =
    {
        { L".atom",                 Icons::ATOM },
        { L".bashrc",               Icons::SHELL },
        { L".bash_history",         Icons::SHELL },
        { L".bash_logout",          Icons::SHELL },
        { L".bash_profile",         Icons::SHELL },
        { L".CFUserTextEncoding",   Icons::OS_APPLE },
        { L".clang-format",         Icons::CONFIG },
        { L".cshrc",                Icons::SHELL },
        { L".DS_Store",             Icons::OS_APPLE },
        { L".emacs",                Icons::EMACS },
        { L".eslintrc.cjs",         Icons::ESLINT },
        { L".eslintrc.js",          Icons::ESLINT },
        { L".eslintrc.json",        Icons::ESLINT },
        { L".eslintrc.yaml",        Icons::ESLINT },
        { L".eslintrc.yml",         Icons::ESLINT },
        { L".gitattributes",        Icons::GIT },
        { L".gitconfig",            Icons::GIT },
        { L".gitignore",            Icons::GIT },
        { L".gitignore_global",     Icons::GIT },
        { L".gitlab-ci.yml",        Icons::GITLAB },
        { L".gitmodules",           Icons::GIT },
        { L".htaccess",             Icons::CONFIG },
        { L".htpasswd",             Icons::CONFIG },
        { L".idea",                 Icons::INTELLIJ },
        { L".ideavimrc",            Icons::VIM },
        { L".inputrc",              Icons::CONFIG },
        { L".kshrc",                Icons::SHELL },
        { L".login",                Icons::SHELL },
        { L".logout",               Icons::SHELL },
        { L".mailmap",              Icons::GIT },
        { L".node_repl_history",    Icons::NODEJS },
        { L".npmignore",            Icons::NPM },
        { L".npmrc",                Icons::NPM },
        { L".profile",              Icons::SHELL },
        { L".python_history",       Icons::LANG_PYTHON },
        { L".rustfmt.toml",         Icons::LANG_RUST },
        { L".rvm",                  Icons::LANG_RUBY },
        { L".rvmrc",                Icons::LANG_RUBY },
        { L".tcshrc",               Icons::SHELL },
        { L".viminfo",              Icons::VIM },
        { L".vimrc",                Icons::VIM },
        { L".Xauthority",           Icons::CONFIG },
        { L".xinitrc",              Icons::CONFIG },
        { L".Xresources",           Icons::CONFIG },
        { L".yarnrc",               Icons::YARN },
        { L".zlogin",               Icons::SHELL },
        { L".zlogout",              Icons::SHELL },
        { L".zprofile",             Icons::SHELL },
        { L".zshenv",               Icons::SHELL },
        { L".zshrc",                Icons::SHELL },
        { L".zsh_history",          Icons::SHELL },
        { L".zsh_sessions",         Icons::SHELL },
        { L"._DS_Store",            Icons::OS_APPLE },
        { L"a.out",                 Icons::SHELL_CMD },
        { L"authorized_keys",       Icons::SSH },
        { L"bashrc",                Icons::SHELL },
        { L"bspwmrc",               Icons::CONFIG },
        { L"build.gradle.kts",      Icons::GRADLE },
        { L"Cargo.lock",            Icons::LANG_RUST },
        { L"Cargo.toml",            Icons::LANG_RUST },
        { L"CMakeLists.txt",        Icons::MAKE },
        { L"composer.json",         Icons::LANG_PHP },
        { L"composer.lock",         Icons::LANG_PHP },
        { L"config",                Icons::CONFIG },
        { L"config.status",         Icons::CONFIG },
        { L"configure",             Icons::WRENCH },
        { L"configure.ac",          Icons::CONFIG },
        { L"configure.in",          Icons::CONFIG },
        { L"constraints.txt",       Icons::LANG_PYTHON },
        { L"COPYING",               Icons::LICENSE },
        { L"COPYRIGHT",             Icons::LICENSE },
        { L"crontab",               Icons::CONFIG },
        { L"crypttab",              Icons::CONFIG },
        { L"csh.cshrc",             Icons::SHELL },
        { L"csh.login",             Icons::SHELL },
        { L"csh.logout",            Icons::SHELL },
        { L"docker-compose.yml",    Icons::DOCKER },
        { L"Dockerfile",            Icons::DOCKER },
        { L"dune",                  Icons::LANG_OCAML },
        { L"dune-project",          Icons::WRENCH },
        { L"Earthfile",             Icons::EARTHFILE },
        { L"environment",           Icons::CONFIG },
        { L"GNUmakefile",           Icons::MAKE },
        { L"go.mod",                Icons::LANG_GO },
        { L"go.sum",                Icons::LANG_GO },
        { L"go.work",               Icons::LANG_GO },
        { L"gradle",                Icons::GRADLE },
        { L"gradle.properties",     Icons::GRADLE },
        { L"gradlew",               Icons::GRADLE },
        { L"gradlew.bat",           Icons::GRADLE },
        { L"group",                 Icons::LOCK },
        { L"gruntfile.coffee",      Icons::GRUNT },
        { L"gruntfile.js",          Icons::GRUNT },
        { L"gruntfile.ls",          Icons::GRUNT },
        { L"gshadow",               Icons::LOCK },
        { L"gulpfile.coffee",       Icons::GULP },
        { L"gulpfile.js",           Icons::GULP },
        { L"gulpfile.ls",           Icons::GULP },
        { L"heroku.yml",            Icons::HEROKU },
        { L"hostname",              Icons::CONFIG },
        { L"id_dsa",                Icons::PRIVATE_KEY },
        { L"id_ecdsa",              Icons::PRIVATE_KEY },
        { L"id_ecdsa_sk",           Icons::PRIVATE_KEY },
        { L"id_ed25519",            Icons::PRIVATE_KEY },
        { L"id_ed25519_sk",         Icons::PRIVATE_KEY },
        { L"id_rsa",                Icons::PRIVATE_KEY },
        { L"inputrc",               Icons::CONFIG },
        { L"Jenkinsfile",           Icons::JENKINS },
        { L"jsconfig.json",         Icons::LANG_JAVASCRIPT },
        { L"Justfile",              Icons::WRENCH },
        { L"known_hosts",           Icons::SSH },
        { L"LICENCE",               Icons::LICENSE },
        { L"LICENCE.md",            Icons::LICENSE },
        { L"LICENCE.txt",           Icons::LICENSE },
        { L"LICENSE",               Icons::LICENSE },
        { L"LICENSE-APACHE",        Icons::LICENSE },
        { L"LICENSE-MIT",           Icons::LICENSE },
        { L"LICENSE.md",            Icons::LICENSE },
        { L"LICENSE.txt",           Icons::LICENSE },
        { L"localized",             Icons::OS_APPLE },
        { L"localtime",             Icons::CLOCK },
        { L"Makefile",              Icons::MAKE },
        { L"makefile",              Icons::MAKE },
        { L"Makefile.ac",           Icons::MAKE },
        { L"Makefile.am",           Icons::MAKE },
        { L"Makefile.in",           Icons::MAKE },
        { L"MANIFEST",              Icons::LANG_PYTHON },
        { L"MANIFEST.in",           Icons::LANG_PYTHON },
        { L"npm-shrinkwrap.json",   Icons::NPM },
        { L"npmrc",                 Icons::NPM },
        { L"package-lock.json",     Icons::NPM },
        { L"package.json",          Icons::NPM },
        { L"passwd",                Icons::LOCK },
        { L"php.ini",               Icons::LANG_PHP },
        { L"PKGBUILD",              Icons::PKGBUILD },
        { L"pom.xml",               Icons::MAVEN },
        { L"Procfile",              Icons::PROCFILE },
        { L"profile",               Icons::SHELL },
        { L"pyproject.toml",        Icons::LANG_PYTHON },
        { L"Rakefile",              Icons::LANG_RUBY },
        { L"README",                Icons::README },
        { L"release.toml",          Icons::LANG_RUST },
        { L"requirements.txt",      Icons::LANG_PYTHON },
        { L"robots.txt",            Icons::ROBOTS },
        { L"rubydoc",               Icons::LANG_RUBYRAILS },
        { L"rvmrc",                 Icons::LANG_RUBY },
        { L"settings.gradle.kts",   Icons::GRADLE },
        { L"shadow",                Icons::LOCK },
        { L"shells",                Icons::CONFIG },
        { L"sudoers",               Icons::LOCK },
        { L"timezone",              Icons::CLOCK },
        { L"tsconfig.json",         Icons::LANG_TYPESCRIPT },
        { L"Vagrantfile",           Icons::VAGRANT },
        { L"webpack.config.js",     Icons::WEBPACK },
        { L"yarn.lock",             Icons::YARN },
        { L"zlogin",                Icons::SHELL },
        { L"zlogout",               Icons::SHELL },
        { L"zprofile",              Icons::SHELL },
        { L"zshenv",                Icons::SHELL },
        { L"zshrc",                 Icons::SHELL },

        // More.
        { L"CHANGES",               Icons::HISTORY },
        { L"CHANGES.md",            Icons::HISTORY },
        { L"CHANGES.txt",           Icons::HISTORY },
        { L"CHANGELOG",             Icons::HISTORY },
        { L"CHANGELOG.md",          Icons::HISTORY },
        { L"CHANGELOG.txt",         Icons::HISTORY },
    };

    const auto& iter = c_filename_icons.find(name);
    if (iter != c_filename_icons.end())
        return GetIcon(iter->second);

    return nullptr;
}

static const WCHAR* GetExtensionIcon(const WCHAR* ext)
{
    static const std::unordered_map<const WCHAR*, Icons, HashCase, EqualCase> c_extension_icons =
    {
        { L"7z",                     Icons::COMPRESSED },
        { L"a",                      Icons::OS_LINUX },
        { L"acc",                    Icons::AUDIO },
        { L"acf",                    Icons::ACF },
        { L"ai",                     Icons::AI },
        { L"aif",                    Icons::AUDIO },
        { L"aifc",                   Icons::AUDIO },
        { L"aiff",                   Icons::AUDIO },
        { L"alac",                   Icons::AUDIO },
        { L"android",                Icons::OS_ANDROID },
        { L"ape",                    Icons::AUDIO },
        { L"apk",                    Icons::OS_ANDROID },
        { L"apple",                  Icons::OS_APPLE },
        { L"ar",                     Icons::COMPRESSED },
        { L"arj",                    Icons::COMPRESSED },
        { L"arw",                    Icons::IMAGE },
        { L"asc",                    Icons::SHIELD_LOCK },
        { L"asm",                    Icons::LANG_ASSEMBLY },
        { L"asp",                    Icons::XML },
        { L"avi",                    Icons::VIDEO },
        { L"avif",                   Icons::IMAGE },
        { L"avro",                   Icons::JSON },
        { L"awk",                    Icons::SHELL_CMD },
        { L"bash",                   Icons::SHELL_CMD },
        { L"bat",                    Icons::OS_WINDOWS_CMD },
        { L"bats",                   Icons::SHELL_CMD },
        { L"bdf",                    Icons::FONT },
        { L"bib",                    Icons::LANG_TEX },
        { L"bin",                    Icons::BINARY },
        { L"bmp",                    Icons::IMAGE },
        { L"br",                     Icons::COMPRESSED },
        { L"bst",                    Icons::LANG_TEX },
        { L"bundle",                 Icons::OS_APPLE },
        { L"bz",                     Icons::COMPRESSED },
        { L"bz2",                    Icons::COMPRESSED },
        { L"bz3",                    Icons::COMPRESSED },
        { L"c",                      Icons::LANG_C },
        { L"c++" ,                   Icons::LANG_CPP },
        { L"cab",                    Icons::OS_WINDOWS },
        { L"cbr",                    Icons::IMAGE },
        { L"cbz",                    Icons::IMAGE },
        { L"cc",                     Icons::LANG_CPP },
        { L"cert",                   Icons::GIST_SECRET },
        { L"cfg",                    Icons::CONFIG },
        { L"cjs",                    Icons::LANG_JAVASCRIPT },
        { L"class",                  Icons::LANG_JAVA },
        { L"clj",                    Icons::CLJ },
        { L"cljs",                   Icons::CLJS },
        { L"cls",                    Icons::LANG_TEX },
        { L"cmake",                  Icons::MAKE },
        { L"cmd",                    Icons::OS_WINDOWS },
        { L"coffee",                 Icons::COFFEE },
        { L"com",                    Icons::OS_WINDOWS_CMD },
        { L"conf",                   Icons::CONFIG },
        { L"config",                 Icons::CONFIG },
        { L"cp",                     Icons::LANG_CPP },
        { L"cpio",                   Icons::COMPRESSED },
        { L"cpp",                    Icons::LANG_CPP },
        { L"cr",                     Icons::CR },
        { L"cr2",                    Icons::IMAGE },
        { L"crdownload",             Icons::DOWNLOAD },
        { L"crt",                    Icons::GIST_SECRET },
        { L"cs",                     Icons::LANG_CSHARP },
        { L"csh",                    Icons::SHELL_CMD },
        { L"cshtml",                 Icons::RAZOR },
        { L"csproj",                 Icons::LANG_CSHARP },
        { L"css",                    Icons::CSS3 },
        { L"csv",                    Icons::SHEET },
        { L"csx",                    Icons::LANG_CSHARP },
        { L"cts",                    Icons::LANG_TYPESCRIPT },
        { L"cu",                     Icons::CU },
        { L"cue",                    Icons::PLAYLIST },
        { L"cxx",                    Icons::LANG_CPP },
        { L"d",                      Icons::LANG_D },
        { L"dart",                   Icons::DART },
        { L"db",                     Icons::DATABASE },
        { L"deb",                    Icons::DEB },
        { L"desktop",                Icons::DESKTOP },
        { L"di",                     Icons::LANG_D },
        { L"diff",                   Icons::DIFF },
        { L"djv",                    Icons::DOCUMENT },
        { L"djvu",                   Icons::DOCUMENT },
        { L"dll",                    Icons::LIBRARY },
        { L"dmg",                    Icons::DISK_IMAGE },
        { L"doc",                    Icons::DOCUMENT },
        { L"docx",                   Icons::DOCUMENT },
        { L"dot",                    Icons::GV },
        { L"download",               Icons::DOWNLOAD },
        { L"drawio",                 Icons::DRAWIO },
        { L"dump",                   Icons::DATABASE },
        { L"dvi",                    Icons::IMAGE },
        { L"dylib",                  Icons::OS_APPLE },
        { L"ebook",                  Icons::BOOK },
        { L"ebuild",                 Icons::EBUILD },
        { L"editorconfig",           Icons::CONFIG },
        { L"ejs",                    Icons::EJS },
        { L"el",                     Icons::EMACS },
        { L"elc",                    Icons::EMACS },
        { L"elm",                    Icons::ELM },
        { L"eml",                    Icons::EML },
        { L"env",                    Icons::ENV },
        { L"eot",                    Icons::FONT },
        { L"eps",                    Icons::VECTOR },
        { L"epub",                   Icons::BOOK },
        { L"erb",                    Icons::LANG_RUBYRAILS },
        { L"erl",                    Icons::ERL },
        { L"ex",                     Icons::LANG_ELIXIR },
        { L"exe",                    Icons::OS_WINDOWS_CMD },
        { L"exs",                    Icons::LANG_ELIXIR },
        { L"f",                      Icons::LANG_FORTRAN },
        { L"f90",                    Icons::LANG_FORTRAN },
        { L"fdmdownload",            Icons::DOWNLOAD },
        { L"fish",                   Icons::SHELL_CMD },
        { L"flac",                   Icons::AUDIO },
        { L"flv",                    Icons::VIDEO },
        { L"fnt",                    Icons::FONT },
        { L"fon",                    Icons::FONT },
        { L"font",                   Icons::FONT },
        { L"for",                    Icons::LANG_FORTRAN },
        { L"fs",                     Icons::LANG_FSHARP },
        { L"fsi",                    Icons::LANG_FSHARP },
        { L"fsx",                    Icons::LANG_FSHARP },
        { L"gdoc",                   Icons::DOCUMENT },
        { L"gem",                    Icons::LANG_RUBY },
        { L"gemfile",                Icons::LANG_RUBY },
        { L"gemspec",                Icons::LANG_RUBY },
        { L"gform",                  Icons::GFORM },
        { L"gif",                    Icons::IMAGE },
        { L"git",                    Icons::GIT },
        { L"go",                     Icons::LANG_GO },
        { L"gpg",                    Icons::SHIELD_LOCK },
        { L"gradle",                 Icons::GRADLE },
        { L"groovy",                 Icons::LANG_GROOVY },
        { L"gsheet",                 Icons::SHEET },
        { L"gslides",                Icons::SLIDE },
        { L"guardfile",              Icons::LANG_RUBY },
        { L"gv",                     Icons::GV },
        { L"gvy",                    Icons::LANG_GROOVY },
        { L"gz",                     Icons::COMPRESSED },
        { L"h",                      Icons::LANG_C },
        { L"h++" ,                   Icons::LANG_CPP },
        { L"h264",                   Icons::VIDEO },
        { L"haml",                   Icons::HAML },
        { L"hbs",                    Icons::MUSTACHE },
        { L"heic",                   Icons::IMAGE },
        { L"heics",                  Icons::VIDEO },
        { L"heif",                   Icons::IMAGE },
        { L"hpp",                    Icons::LANG_CPP },
        { L"hs",                     Icons::LANG_HASKELL },
        { L"htm",                    Icons::HTML5 },
        { L"html",                   Icons::HTML5 },
        { L"hxx",                    Icons::LANG_CPP },
        { L"ical",                   Icons::CALENDAR },
        { L"icalendar",              Icons::CALENDAR },
        { L"ico",                    Icons::IMAGE },
        { L"ics",                    Icons::CALENDAR },
        { L"ifb",                    Icons::CALENDAR },
        { L"image",                  Icons::DISK_IMAGE },
        { L"img",                    Icons::DISK_IMAGE },
        { L"iml",                    Icons::INTELLIJ },
        { L"inl",                    Icons::LANG_C },
        { L"ini",                    Icons::CONFIG },
        { L"ipynb",                  Icons::IPYNB },
        { L"iso",                    Icons::DISK_IMAGE },
        { L"j2c",                    Icons::IMAGE },
        { L"j2k",                    Icons::IMAGE },
        { L"jad",                    Icons::LANG_JAVA },
        { L"jar",                    Icons::LANG_JAVA },
        { L"java",                   Icons::LANG_JAVA },
        { L"jfi",                    Icons::IMAGE },
        { L"jfif",                   Icons::IMAGE },
        { L"jif",                    Icons::IMAGE },
        { L"jl",                     Icons::JL },
        { L"jmd",                    Icons::MARKDOWN },
        { L"jp2",                    Icons::IMAGE },
        { L"jpe",                    Icons::IMAGE },
        { L"jpeg",                   Icons::IMAGE },
        { L"jpf",                    Icons::IMAGE },
        { L"jpg",                    Icons::IMAGE },
        { L"jpx",                    Icons::IMAGE },
        { L"js",                     Icons::LANG_JAVASCRIPT },
        { L"json",                   Icons::JSON },
        { L"jsx",                    Icons::REACT },
        { L"jxl",                    Icons::IMAGE },
        { L"kbx",                    Icons::SHIELD_KEY },
        { L"kdb",                    Icons::KEYPASS },
        { L"kdbx",                   Icons::KEYPASS },
        { L"key",                    Icons::KEY },
        { L"ko",                     Icons::OS_LINUX },
        { L"ksh",                    Icons::SHELL_CMD },
        { L"kt",                     Icons::LANG_KOTLIN },
        { L"kts",                    Icons::LANG_KOTLIN },
        { L"latex",                  Icons::LANG_TEX },
        { L"ldb",                    Icons::DATABASE },
        { L"less",                   Icons::LESS },
        { L"lhs",                    Icons::LANG_HASKELL },
        { L"lib",                    Icons::LIBRARY },
        { L"license",                Icons::LICENSE },
        { L"lisp",                   Icons::LISP },
        { L"localized",              Icons::OS_APPLE },
        { L"lock",                   Icons::LOCK },
        { L"log",                    Icons::LOG },
        { L"ltx",                    Icons::LANG_TEX },
        { L"lua",                    Icons::LUA },
        { L"lz",                     Icons::COMPRESSED },
        { L"lz4",                    Icons::COMPRESSED },
        { L"lzh",                    Icons::COMPRESSED },
        { L"lzma",                   Icons::COMPRESSED },
        { L"lzo",                    Icons::COMPRESSED },
        { L"m",                      Icons::LANG_C },
        { L"m2ts",                   Icons::VIDEO },
        { L"m2v",                    Icons::VIDEO },
        { L"m3u",                    Icons::PLAYLIST },
        { L"m3u8",                   Icons::PLAYLIST },
        { L"m4a",                    Icons::AUDIO },
        { L"m4v",                    Icons::VIDEO },
        { L"magnet",                 Icons::MAGNET },
        { L"markdown",               Icons::MARKDOWN },
        { L"md",                     Icons::MARKDOWN },
        { L"md5",                    Icons::SHIELD_CHECK },
        { L"mdb",                    Icons::DATABASE },
        { L"mid",                    Icons::MID },
        { L"mjs",                    Icons::LANG_JAVASCRIPT },
        { L"mk",                     Icons::MAKE },
        { L"mka",                    Icons::AUDIO },
        { L"mkd",                    Icons::MARKDOWN },
        { L"mkv",                    Icons::VIDEO },
        { L"ml",                     Icons::LANG_OCAML },
        { L"mli",                    Icons::LANG_OCAML },
        { L"mll",                    Icons::LANG_OCAML },
        { L"mly",                    Icons::LANG_OCAML },
        { L"mm",                     Icons::LANG_CPP },
        { L"mobi",                   Icons::BOOK },
        { L"mov",                    Icons::VIDEO },
        { L"mp2",                    Icons::AUDIO },
        { L"mp3",                    Icons::AUDIO },
        { L"mp4",                    Icons::VIDEO },
        { L"mpeg",                   Icons::VIDEO },
        { L"mpg",                    Icons::VIDEO },
        { L"msi",                    Icons::OS_WINDOWS },
        { L"mts",                    Icons::LANG_TYPESCRIPT },
        { L"mustache",               Icons::MUSTACHE },
        { L"nef",                    Icons::IMAGE },
        { L"ninja",                  Icons::NINJA },
        { L"nix",                    Icons::NIX },
        { L"node",                   Icons::NODEJS },
        { L"o",                      Icons::BINARY },
        { L"odp",                    Icons::SLIDE },
        { L"ods",                    Icons::SHEET },
        { L"odt",                    Icons::DOCUMENT },
        { L"ogg",                    Icons::AUDIO },
        { L"ogm",                    Icons::VIDEO },
        { L"ogv",                    Icons::VIDEO },
        { L"opus",                   Icons::AUDIO },
        { L"orf",                    Icons::IMAGE },
        { L"org",                    Icons::ORG },
        { L"otf",                    Icons::FONT },
        { L"out",                    Icons::OUT_EXT },
        { L"p12",                    Icons::KEY },
        { L"par",                    Icons::COMPRESSED },
        { L"part",                   Icons::DOWNLOAD },
        { L"patch",                  Icons::DIFF },
        { L"pbm",                    Icons::IMAGE },
        { L"pcm",                    Icons::AUDIO },
        { L"pdf",                    Icons::PDF },
        { L"pem",                    Icons::KEY },
        { L"pfx",                    Icons::KEY },
        { L"pgm",                    Icons::IMAGE },
        { L"phar",                   Icons::LANG_PHP },
        { L"php",                    Icons::LANG_PHP },
        { L"pkg",                    Icons::PKG },
        { L"pl",                     Icons::LANG_PERL },
        { L"plist",                  Icons::OS_APPLE },
        { L"plx",                    Icons::LANG_PERL },
        { L"pm",                     Icons::LANG_PERL },
        { L"png",                    Icons::IMAGE },
        { L"pnm",                    Icons::IMAGE },
        { L"pod",                    Icons::LANG_PERL },
        { L"pp",                     Icons::PP },
        { L"ppm",                    Icons::IMAGE },
        { L"pps",                    Icons::SLIDE },
        { L"ppsx",                   Icons::SLIDE },
        { L"ppt",                    Icons::SLIDE },
        { L"pptx",                   Icons::SLIDE },
        { L"properties",             Icons::JSON },
        { L"prql",                   Icons::DATABASE },
        { L"ps",                     Icons::VECTOR },
        { L"ps1",                    Icons::POWERSHELL },
        { L"psd",                    Icons::PSD },
        { L"psd1",                   Icons::POWERSHELL },
        { L"psf",                    Icons::FONT },
        { L"psm1",                   Icons::POWERSHELL },
        { L"pub",                    Icons::PUBLIC_KEY },
        { L"purs",                   Icons::PURS },
        { L"pxm",                    Icons::IMAGE },
        { L"py",                     Icons::LANG_PYTHON },
        { L"pyc",                    Icons::LANG_PYTHON },
        { L"pyd",                    Icons::LANG_PYTHON },
        { L"pyi",                    Icons::LANG_PYTHON },
        { L"pyo",                    Icons::LANG_PYTHON },
        { L"qcow",                   Icons::DISK_IMAGE },
        { L"qcow2",                  Icons::DISK_IMAGE },
        { L"r",                      Icons::LANG_R },
        { L"rar",                    Icons::COMPRESSED },
        { L"raw",                    Icons::IMAGE },
        { L"razor",                  Icons::RAZOR },
        { L"rb",                     Icons::LANG_RUBY },
        { L"rdata",                  Icons::LANG_R },
        { L"rdb",                    Icons::RDB },
        { L"rdoc",                   Icons::MARKDOWN },
        { L"rds",                    Icons::LANG_R },
        { L"readme",                 Icons::README },
        { L"rlib",                   Icons::LANG_RUST },
        { L"rmd",                    Icons::MARKDOWN },
        { L"rmeta",                  Icons::LANG_RUST },
        { L"rpm",                    Icons::RPM },
        { L"rs",                     Icons::LANG_RUST },
        { L"rspec",                  Icons::LANG_RUBY },
        { L"rspec_parallel",         Icons::LANG_RUBY },
        { L"rspec_status",           Icons::LANG_RUBY },
        { L"rss",                    Icons::RSS },
        { L"rst",                    Icons::TEXT },
        { L"rtf",                    Icons::TEXT },
        { L"ru",                     Icons::LANG_RUBY },
        { L"rubydoc",                Icons::LANG_RUBYRAILS },
        { L"s",                      Icons::LANG_ASSEMBLY },
        { L"sass",                   Icons::LANG_SASS },
        { L"sbt",                    Icons::SUBTITLE },
        { L"scala",                  Icons::SCALA },
        { L"scss",                   Icons::LANG_SASS },
        { L"service",                Icons::SERVICE },
        { L"sh",                     Icons::SHELL_CMD },
        { L"sha1",                   Icons::SHIELD_CHECK },
        { L"sha224",                 Icons::SHIELD_CHECK },
        { L"sha256",                 Icons::SHIELD_CHECK },
        { L"sha384",                 Icons::SHIELD_CHECK },
        { L"sha512",                 Icons::SHIELD_CHECK },
        { L"shell",                  Icons::SHELL_CMD },
        { L"shtml",                  Icons::HTML5 },
        { L"sig",                    Icons::SIGNED_FILE },
        { L"signature",              Icons::SIGNED_FILE },
        { L"slim",                   Icons::LANG_RUBYRAILS },
        { L"sln",                    Icons::SLN },
        { L"so",                     Icons::OS_LINUX },
        { L"sql",                    Icons::DATABASE },
        { L"sqlite3",                Icons::SQLITE3 },
        { L"srt",                    Icons::SUBTITLE },
        { L"ssa",                    Icons::SUBTITLE },
        { L"stl",                    Icons::IMAGE },
        { L"sty",                    Icons::LANG_TEX },
        { L"styl",                   Icons::LANG_STYLUS },
        { L"stylus",                 Icons::LANG_STYLUS },
        { L"sub",                    Icons::SUBTITLE },
        { L"sublime-build",          Icons::SUBLIME },
        { L"sublime-keymap",         Icons::SUBLIME },
        { L"sublime-menu",           Icons::SUBLIME },
        { L"sublime-options",        Icons::SUBLIME },
        { L"sublime-package",        Icons::SUBLIME },
        { L"sublime-project",        Icons::SUBLIME },
        { L"sublime-session",        Icons::SUBLIME },
        { L"sublime-settings",       Icons::SUBLIME },
        { L"sublime-snippet",        Icons::SUBLIME },
        { L"sublime-theme",          Icons::SUBLIME },
        { L"svelte",                 Icons::SVELTE },
        { L"svg",                    Icons::VECTOR },
        { L"swift",                  Icons::SWIFT },
        { L"t",                      Icons::LANG_PERL },
        { L"tar",                    Icons::COMPRESSED },
        { L"taz",                    Icons::COMPRESSED },
        { L"tbz",                    Icons::COMPRESSED },
        { L"tbz2",                   Icons::COMPRESSED },
        { L"tc",                     Icons::DISK_IMAGE },
        { L"tex",                    Icons::LANG_TEX },
        { L"tf",                     Icons::TERRAFORM },
        { L"tfstate",                Icons::TERRAFORM },
        { L"tfvars",                 Icons::TERRAFORM },
        { L"tgz",                    Icons::COMPRESSED },
        { L"tif",                    Icons::IMAGE },
        { L"tiff",                   Icons::IMAGE },
        { L"tlz",                    Icons::COMPRESSED },
        { L"tml",                    Icons::CONFIG },
        { L"toml",                   Icons::CONFIG },
        { L"torrent",                Icons::TORRENT },
        { L"ts",                     Icons::LANG_TYPESCRIPT },
        { L"tsv",                    Icons::SHEET },
        { L"tsx",                    Icons::REACT },
        { L"ttc",                    Icons::FONT },
        { L"ttf",                    Icons::FONT },
        { L"twig",                   Icons::TWIG },
        { L"txt",                    Icons::TEXT },
        { L"typ",                    Icons::TYPST },
        { L"txz",                    Icons::COMPRESSED },
        { L"tz",                     Icons::COMPRESSED },
        { L"tzo",                    Icons::COMPRESSED },
        { L"unity",                  Icons::UNITY },
        { L"unity3d",                Icons::UNITY },
        { L"v",                      Icons::LANG_V },
        { L"vcxproj",                Icons::SLN },
        { L"vdi",                    Icons::DISK_IMAGE },
        { L"vhd",                    Icons::DISK_IMAGE },
        { L"video",                  Icons::VIDEO },
        { L"vim",                    Icons::VIM },
        { L"vmdk",                   Icons::DISK_IMAGE },
        { L"vob",                    Icons::VIDEO },
        { L"vue",                    Icons::VUE },
        { L"war",                    Icons::LANG_JAVA },
        { L"wav",                    Icons::AUDIO },
        { L"webm",                   Icons::VIDEO },
        { L"webmanifest",            Icons::JSON },
        { L"webp",                   Icons::IMAGE },
        { L"whl",                    Icons::LANG_PYTHON },
        { L"windows",                Icons::OS_WINDOWS },
        { L"wma",                    Icons::AUDIO },
        { L"wmv",                    Icons::VIDEO },
        { L"woff",                   Icons::FONT },
        { L"woff2",                  Icons::FONT },
        { L"wv",                     Icons::AUDIO },
        { L"xcf",                    Icons::IMAGE },
        { L"xhtml",                  Icons::HTML5 },
        { L"xlr",                    Icons::SHEET },
        { L"xls",                    Icons::SHEET },
        { L"xlsm",                   Icons::SHEET },
        { L"xlsx",                   Icons::SHEET },
        { L"xml",                    Icons::XML },
        { L"xpm",                    Icons::IMAGE },
        { L"xul",                    Icons::XML },
        { L"xz",                     Icons::COMPRESSED },
        { L"yaml",                   Icons::YAML },
        { L"yml",                    Icons::YAML },
        { L"z",                      Icons::COMPRESSED },
        { L"zig",                    Icons::ZIG },
        { L"zip",                    Icons::COMPRESSED },
        { L"zsh",                    Icons::SHELL_CMD },
        { L"zsh-theme",              Icons::SHELL },
        { L"zst",                    Icons::COMPRESSED },
    };

    const auto& iter = c_extension_icons.find(ext);
    if (iter != c_extension_icons.end())
        return GetIcon(iter->second);

    return nullptr;
}

void SetNerdFontsVersion(unsigned ver)
{
    if (ver == 2)
        s_nerd_font_version_index = 1;
    else if (ver == 3)
        s_nerd_font_version_index = 0;
}

const WCHAR* LookupIcon(const WCHAR* full, DWORD attr)
{
    const WCHAR* icon = nullptr;
    const WCHAR* name = FindName(full);

    if (attr & FILE_ATTRIBUTE_DIRECTORY)
    {
        icon = GetDirectoryIcon(name);
        if (!icon)
            icon = GetIcon(Icons::FOLDER);
    }
    else
    {
        icon = GetFilenameIcon(name);
        if (!icon)
        {
            if (_wcsnicmp(name, L"readme", 6) == 0)
                icon = GetIcon(Icons::INFO);

            if (!icon)
            {
                const WCHAR* ext = FindExtension(name);
                if (ext)
                {
                    ++ext;
                    icon = GetExtensionIcon(ext);
                }

                if (!icon)
                    icon = GetIcon(ext ? Icons::FILE : Icons::FILE_OUTLINE);
            }
        }
    }

    assert(icon && *icon);
    return icon;
}

