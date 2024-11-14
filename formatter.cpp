// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "formatter.h"
#include "sorting.h"
#include "filesys.h"
#include "colors.h"
#include "output.h"
#include "ecma48.h"
#include "wcwidth.h"
#include "wcwidth_iter.h"
#include "columns.h"
#include "str.h"

#include <algorithm>
#include <memory>

static RepoMap s_repo_map;

std::shared_ptr<const RepoStatus> FindRepo(const WCHAR* dir)
{
    return s_repo_map.Find(dir);
}

struct TreeFiles
{
    size_t              cursor = 0;
    std::vector<std::unique_ptr<FileInfo>> files;
    std::shared_ptr<DirContext> dir;
};

static std::map<const WCHAR*, std::unique_ptr<TreeFiles>, SortCaseless> s_tree_map;
static std::vector<TreeFiles*> s_tree_stack;

bool IsTreeRoot()
{
    return (g_settings->IsSet(FMT_TREE) && s_tree_stack.empty());
}

/*
 * General purpose OutputOperation subclasses.
 */

class OutputText : public OutputOperation
{
public:
    OutputText(StrW&& s)
    {
        m_s = std::move(s);
        assert(s.Empty());
    }

    void Render(HANDLE h, const DirContext* dir) override
    {
        OutputConsole(h, m_s.Text(), m_s.Length());
    }

private:
    StrW m_s;
};

/*
 * DirEntryFormatter.
 */

DirEntryFormatter::DirEntryFormatter()
    : m_picture_template(std::make_shared<PictureFormatter>(m_settings))
{
    g_settings = &m_settings;
}

DirEntryFormatter::~DirEntryFormatter()
{
    assert(m_outputs.empty());
    Finalize();
    g_settings = nullptr;
}

void DirEntryFormatter::Initialize(unsigned num_columns, const FormatFlags flags, WhichTimeStamp whichtimestamp, WhichFileSize whichfilesize, DWORD dwAttrIncludeAny, DWORD dwAttrMatch, DWORD dwAttrExcludeAny, const WCHAR* picture)
{
    m_root_pass = false;
    m_count_usage_dirs = 0;

    m_hout = GetStdHandle(STD_OUTPUT_HANDLE);
    m_settings.m_num_columns = num_columns;
    m_settings.m_flags = flags;
    m_settings.m_whichtimestamp = whichtimestamp;
    m_settings.m_whichfilesize = whichfilesize;
    m_settings.m_dwAttrIncludeAny = dwAttrIncludeAny;
    m_settings.m_dwAttrMatch = dwAttrMatch;
    m_settings.m_dwAttrExcludeAny = dwAttrExcludeAny;
    m_settings.m_need_compressed_size = (whichfilesize == FILESIZE_COMPRESSED ||
                                         (flags & FMT_COMPRESSED) ||
                                         wcschr(g_sort_order, 'c'));
    m_settings.m_need_short_filenames = !!(flags & FMT_SHORTNAMES);

    if (IsRedirectedStdOut())
    {
        if (g_debug)
            Printf(L"debug: output is redirected\n");
        m_settings.m_flags |= FMT_REDIRECTED;
    }

    if (!CanUseEscapeCodes(m_hout))
    {
        if (g_debug)
            Printf(L"debug: escape codes suppressed\n");
        m_settings.m_flags &= ~FMT_HYPERLINKS;
    }

    // Initialize the picture formatter.

    StrW sPic;

    if (!picture)
    {
        switch (m_settings.m_num_columns)
        {
        case 4:
            if (m_settings.IsSet(FMT_SIZE|FMT_MINISIZE|FMT_FAT))
                picture = L"F12 Sm";
            else
                picture = L"F17";
            break;
        case 2:
            if (m_settings.IsSet(FMT_FAT))
                picture = L"F Ss D G?";
            else
            {
                unsigned width = 38;
                StrW tmp;
                const bool size = m_settings.IsSet(FMT_SIZE|FMT_MINISIZE);
                const bool date = m_settings.IsSet(FMT_DATE|FMT_MINIDATE);
                if (size)
                {
                    if (date || m_settings.IsSet(FMT_MINISIZE))
                    {
                        tmp.Append(L" Sm");
                        width -= 1 + 4;
                    }
                    else
                    {
                        tmp.Append(L" Ss");
                        width -= 1 + 9;
                    }
                }
                if (date)
                {
                    if (size || m_settings.IsSet(FMT_MINIDATE))
                    {
                        if (tmp.Length())
                        {
                            tmp.Append(L" ");
                            width -= 1;
                        }
                        tmp.Append(L" Dm");
                        width -= 1 + 11;
                    }
                    else
                    {
                        if (tmp.Length())
                        {
                            tmp.Append(L" ");
                            width -= 1;
                        }
                        tmp.Append(L" Dn");
                        width -= 1 + 17;
                    }
                }
                if (m_settings.IsSet(FMT_GIT))
                {
                    // Because the /2 format uses a fixed with, there isn't a
                    // good way to make the git column disappear when not in a
                    // git repo.  So just let --git always force it.
                    tmp.Append(L" G");
                    width -= 3;
                }
                sPic.Printf(L"F%u%s", width, tmp.Text());
                picture = sPic.Text();
            }
            break;
        case 1:
            if (m_settings.IsSet(FMT_FAT))
                picture = L"F Ss D  C?  T?  G?  R?";
            else
            {
                sPic.Clear();
                if (!m_settings.IsSet(FMT_LONGNODATE))
                    sPic.Append(L"D  ");
                if (!m_settings.IsSet(FMT_LONGNOSIZE))
                    sPic.Append(L"S  ");
                sPic.Append(L"[C?  ]");
                if (!m_settings.IsSet(FMT_LONGNOATTRIBUTES))
                    sPic.Append(L"[T?  ]");
                sPic.Append(L"[X?  ][O?  ][G?  ][R?  ]F");
                picture = sPic.Text();
            }
            break;
        case 0:
            sPic.Set(L"F");
            if (m_settings.IsSet(FMT_SIZE|FMT_MINISIZE|FMT_FAT))
                sPic.Append(m_settings.IsSet(FMT_MINISIZE) ? L" Sm" : L" S");
            if (m_settings.IsSet(FMT_DATE|FMT_MINIDATE))
                sPic.Append(m_settings.IsSet(FMT_MINIDATE) ? L" Dm" : L" D");
            sPic.Append(L" T? G?");
            picture = sPic.Text();
            break;
        default:
            assert(false);
            break;
        }
    }

    m_picture_template->ParsePicture(picture);

    if (m_picture_template->IsCompressedSizeNeeded())
        m_settings.m_need_compressed_size = true;
    if (m_picture_template->AreShortFilenamesNeeded())
        m_settings.m_need_short_filenames = true;

    m_fImmediate = (!IsGradientColorScaleMode() &&
                    !*g_sort_order &&
                    Settings().m_num_columns == 1);
    m_delayed_render = (IsGradientColorScaleMode() && GetColorScaleFields());

    if (g_debug)
    {
        Printf(L"debug: format flags: 0x%08.8x%08.8x\n", DWORD(ULONGLONG(flags) >> 32), DWORD(flags));
        DebugPrintHideDotFilesMode();
        Printf(L"debug: immediate output: %s\n", m_fImmediate ? L"true" : L"false");
        Printf(L"debug: delayed render: %s\n", m_delayed_render ? L"true" : L"false");
    }
}

bool DirEntryFormatter::IsOnlyRootSubDir() const
{
    return m_subdirs.empty() && IsRootSubDir();
}

bool DirEntryFormatter::IsRootSubDir() const
{
    return m_root_pass;
}

bool DirEntryFormatter::IsNewRootGroup(const WCHAR* dir) const
{
    // If the pattern is implicit, then it's only a new root group if we
    // don't yet have a root group.

    if (m_implicit)
        return !m_root_group.Length();

    // If we don't yet have a root group, then it's a new root group.

    if (!m_root_group.Length())
        return true;

    // If the root part is different, then it's a new root group.

    StrW tmp;
    tmp.Set(dir);
    EnsureTrailingSlash(tmp);
    if (tmp.Length() < m_root.Length())
        return true;
    tmp.SetLength(m_root.Length());
    if (!tmp.EqualI(m_root))
        return true;

    // At this point we know the root matches.  If the root matches but
    // the next level is different, then it's a new root group.

    tmp.Set(dir + m_root.Length());
    EnsureTrailingSlash(tmp);
    for (unsigned ii = 0; ii < tmp.Length(); ii++)
    {
        if (tmp.Text()[ii] == '\\')
        {
            tmp.SetLength(ii + 1);
            break;
        }
    }
    if (!tmp.EqualI(m_root_group.Text() + m_root.Length()))
        return true;

    // At this point we know the whole m_root_group portion matches, so
    // it's not a new root group.

    return false;
}

void DirEntryFormatter::UpdateRootGroup(const WCHAR* dir)
{
#ifdef DEBUG
    assert(m_root.Length());
    StrW tmp;
    tmp.Set(dir);
    EnsureTrailingSlash(tmp);
    assert(m_root.Length() <= tmp.Length());
    tmp.SetLength(m_root.Length());
    assert(tmp.EqualI(m_root));
#endif

    m_root_group.Set(dir);
    EnsureTrailingSlash(m_root_group);
    for (unsigned ii = m_root.Length(); ii < m_root_group.Length(); ii++)
    {
        if (m_root_group.Text()[ii] == '\\')
        {
            m_root_group.SetLength(ii + 1);
            break;
        }
    }
}

bool DirEntryFormatter::OnVolumeBegin(const WCHAR* dir, Error& e)
{
    StrW root;
    WCHAR volume_name[MAX_PATH];
    DWORD dwSerialNumber;
    const bool line_break_before_volume = m_line_break_before_volume;

#ifdef DEBUG
    m_seen_dirs_storage.clear();
    m_seen_dirs.clear();
#endif

    m_cFilesTotal = 0;
    m_cbTotalTotal = 0;
    m_cbAllocatedTotal = 0;
    m_cbCompressedTotal = 0;
    m_line_break_before_volume = true;

    if (Settings().IsSet(FMT_NOVOLUMEINFO))
        return false;

    if (Settings().IsSet(FMT_USAGE))
    {
        StrW s;
        if (line_break_before_volume)
            s.Append('\n');
        const unsigned size_field_width = GetSizeFieldWidthByStyle(Settings(), 0);
        s.Printf(L"%*.*s  %*.*s  %7s\n",
                 size_field_width, size_field_width, L"Used",
                 size_field_width, size_field_width, L"Allocated",
                 L"Files");
        Render(new OutputText(std::move(s)));
        m_count_usage_dirs = 0;
        return true;
    }

    if (Settings().IsSet(FMT_BARE))
        return false;

    if (!GetDrive(dir, root, e))
    {
        e.Clear();
        return false;
    }

    EnsureTrailingSlash(root);
    if (!GetVolumeInformation(root.Text(), volume_name, _countof(volume_name), &dwSerialNumber, 0, 0, 0, 0))
    {
        const DWORD dwErr = GetLastError();
        if (dwErr != ERROR_DIR_NOT_ROOT)   // Don't fail on subst'd drives.
            e.Set(dwErr);
        return false;
    }

    if (*root.Text() == '\\')
        StripTrailingSlashes(root);
    else
        root.SetLength(1);

    StrW s;
    if (line_break_before_volume)
        s.Append('\n');
    if (volume_name[0])
        s.Printf(L" Volume in drive %s is %s\n", root.Text(), volume_name);
    else
        s.Printf(L" Volume in drive %s has no label.\n", root.Text());
    s.Printf(L" Volume Serial Number is %04X-%04X\n",
             HIWORD(dwSerialNumber), LOWORD(dwSerialNumber));
    Render(new OutputText(std::move(s)));

    m_line_break_before_miniheader = true;
    return true;
}

void DirEntryFormatter::OnPatterns(bool grouped)
{
    m_grouped_patterns = grouped;
}

void DirEntryFormatter::OnScanFiles(const WCHAR* dir, bool implicit, bool root_pass)
{
    m_implicit = implicit;
    m_root_pass = root_pass;
    if (m_root_pass)
    {
        m_root.Set(dir);
        EnsureTrailingSlash(m_root);
        m_root_group.Clear();
    }
}

void DirEntryFormatter::OnDirectoryBegin(const WCHAR* const dir, const WCHAR* const dir_rel, const std::shared_ptr<const RepoStatus>& repo)
{
#ifdef DEBUG
    assert(!m_in_dir);
    m_in_dir = true;
#endif

    const bool fReset = (Settings().IsSet(FMT_USAGEGROUPED) ?
                         (IsRootSubDir() || IsNewRootGroup(dir)) :
                         (!m_dir || !m_dir->dir.EqualI(dir)));
    if (fReset)
        UpdateRootGroup(dir);

    if (g_debug)
    {
        Printf(L"debug: OnDirectoryBegin '%s'%s\n", dir, fReset ? L", reset root group" : L"");
        m_tick_begin = GetTickCount();
    }

#ifdef DEBUG
    {
        // Make sure OnDirectoryBegin is called only once per directory per
        // volume.
        const bool seen = m_seen_dirs.find(dir) != m_seen_dirs.end();
        assert(!seen);
        if (!seen)
        {
            m_seen_dirs_storage.emplace_back(dir);
            m_seen_dirs.insert(m_seen_dirs_storage.back().Text());
        }
    }
#endif

    m_files.clear();
    m_longest_file_width = 0;
    m_longest_dir_width = 0;
    m_granularity = 0;

    DWORD dwSectorsPerCluster;
    DWORD dwBytesPerSector;
    DWORD dwFreeClusters;
    DWORD dwTotalClusters;
    if (GetDiskFreeSpace(dir, &dwSectorsPerCluster, &dwBytesPerSector, &dwFreeClusters, &dwTotalClusters))
        m_granularity = dwSectorsPerCluster * dwBytesPerSector;

    {
        std::shared_ptr<PictureFormatter> picture = std::make_shared<PictureFormatter>(*m_picture_template);
        std::shared_ptr<DirContext> context = std::make_shared<DirContext>(Settings().m_flags, picture);
        context->repo = repo;
        if (Settings().IsSet(FMT_SHORTNAMES))
        {
            StrW short_name;
            short_name.ReserveMaxPath();
            const DWORD len = GetShortPathName(dir, short_name.Reserve(), short_name.Capacity());
            if (len && len < short_name.Capacity())
                context->dir.Set(short_name);
        }
        if (context->dir.Empty())
        {
            context->dir.Set(dir);
            context->dir_rel.Set(dir_rel);
        }

        class OutputDirectoryContext : public OutputOperation
        {
        public:
            OutputDirectoryContext(DirEntryFormatter* def, const std::shared_ptr<DirContext>& context)
            : m_def(def), m_dir(context) {}

            void Render(HANDLE h, const DirContext* dir) override
            {
                m_def->m_dir = m_dir;
                m_dir->picture->SetDirContext(m_dir);
            }

        private:
            DirEntryFormatter* m_def;
            std::shared_ptr<DirContext> m_dir;
        };

        OutputDirectoryContext* o = new OutputDirectoryContext(this, context);
        if (IsDelayedRender())
        {
            // Must happen immediately, in addition to delayed, so DirContext
            // is managed properly during both the scan and the render.
            o->Render(0, context.get());
        }
        Render(o);
    }

    if (!Settings().IsSet(FMT_BARE|FMT_TREE))
    {
        StrW s;
        if (Settings().IsSet(FMT_MINIHEADER))
        {
            const WCHAR* header_color = Settings().IsSet(FMT_COLORS) ? GetColorByKey(L"hM") : nullptr;

            if (m_line_break_before_miniheader)
                s.Append(L"\n");
            s.AppendColor(header_color);
            s.Printf(L"%s%s:", dir, wcschr(dir, '\\') ? L"" : L"\\");
            s.AppendNormalIf(header_color);
            s.Append(L"\n");
        }
        else if (!Settings().IsSet(FMT_NOHEADER))
        {
            s.Printf(L"\n Directory of %s%s\n\n", dir, wcschr(dir, '\\') ? L"" : L"\\");
        }
        Render(new OutputText(std::move(s)));
    }
}

static void DisplayOne(HANDLE h, const FileInfo* const pfi, const FileInfo* const stream, const DirContext* const dir)
{
    StrW s;
    FormatFlags flags = dir->flags;

    if (flags & FMT_BARE)
    {
        assert(!pfi->IsPseudoDirectory());

        if (flags & FMT_SUBDIRECTORIES)
            flags |= FMT_FULLNAME;
        const WCHAR* dir_str = (flags & FMT_BARERELATIVE) ? dir->dir_rel.Text() : dir->dir.Text();
        FormatFilename(s, pfi, flags, 0, dir_str, SelectColor(pfi, flags, dir->dir.Text()));
    }
    else
    {
        dir->picture->Format(s, pfi, stream);
    }

    s.Append(L"\n");
    OutputConsole(h, s.Text(), s.Length());

    if (!stream)
    {
        for (auto pp = pfi->GetStreams(); pp && *pp; ++pp)
            DisplayOne(h, pfi, pp[0].get(), dir);
    }
}

void DirEntryFormatter::OnFile(const WCHAR* const dir, const WIN32_FIND_DATA* const pfd)
{
    std::unique_ptr<FileInfo> pfi;
    const auto picture = m_dir->picture.get();
    const bool fUsage = Settings().IsSet(FMT_USAGE);
    const bool fImmediate = (m_fImmediate &&
                             picture->IsImmediate() &&
                             !m_grouped_patterns);

    pfi = std::make_unique<FileInfo>();
    pfi->Init(dir, m_granularity, pfd, Settings());

    // Skip the file if filtering out files without alternate data streams.

    if (Settings().IsSet(FMT_ALTDATASTEAMS|FMT_ONLYALTDATASTREAMS))
    {
        assert(implies(Settings().IsSet(FMT_ALTDATASTEAMS), !(Settings().IsSet(FMT_FAT|FMT_BARE))));

        StrW tmp;
        PathJoin(tmp, dir, pfi->GetLongName());

        StrW full;
        full.Set(tmp);

        bool fAnyAltDataStreams = false;
        WIN32_FIND_STREAM_DATA fsd;
        SHFind shFind = __FindFirstStreamW(full.Text(), FindStreamInfoStandard, &fsd, 0);
        if (!shFind.Empty())
        {
            std::unique_ptr<std::vector<std::unique_ptr<FileInfo>>> streams;

            do
            {
                if (!wcsicmp(fsd.cStreamName, L"::$DATA"))
                    continue;
                fAnyAltDataStreams = true;
                if (!Settings().IsSet(FMT_ALTDATASTEAMS))
                    break;
                if (!streams)
                    streams = std::make_unique<std::vector<std::unique_ptr<FileInfo>>>();
                streams->emplace_back(std::make_unique<FileInfo>());
                streams->back()->InitStream(fsd);
            }
            while (__FindNextStreamW(shFind, &fsd));

            if (streams && streams->size())
                pfi->InitStreams(*streams);
        }

        if (fAnyAltDataStreams)
            pfi->SetAltDataStreams();
        else if (Settings().IsSet(FMT_ONLYALTDATASTREAMS))
            return;
    }

    // Get git status if needed.

    if ((Settings().m_flags & (FMT_GIT|FMT_SUBDIRECTORIES)) == (FMT_GIT|FMT_SUBDIRECTORIES) ||
        (Settings().IsSet(FMT_GITREPOS)))
    {
        StrW full;
        PathJoin(full, dir, pfd->cFileName);
        auto repo = GitStatus(full.Text(), Settings().IsSet(FMT_SUBDIRECTORIES));
        s_repo_map.Add(repo);
    }

    // Update statistics.

    const FormatFlags flags = Settings().m_flags;
    const unsigned num_columns = Settings().m_num_columns;

    if (pfi->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY)
    {
        m_cDirs++;
        if (!num_columns || picture->IsFilenameWidthNeeded())
        {
            unsigned name_width = __wcswidth(pfi->GetFileName(flags).Text());
            if ((flags & (FMT_CLASSIFY|FMT_DIRBRACKETS)) == FMT_CLASSIFY)
                ++name_width;   // For appending '\' symbol.
            if (m_longest_dir_width < name_width)
                m_longest_dir_width = name_width;
        }
    }
    else
    {
        m_cFiles++;
        m_cbTotal += pfi->GetFileSize();
        m_cbAllocated += pfi->GetFileSize(FILESIZE_ALLOCATION);
        if (Settings().IsSet(FMT_COMPRESSED))
            m_cbCompressed += pfi->GetFileSize(FILESIZE_COMPRESSED);
        if (!num_columns || picture->IsFilenameWidthNeeded())
        {
            unsigned name_width = __wcswidth(pfi->GetFileName(flags).Text());
            if ((flags & (FMT_CLASSIFY|FMT_FAT|FMT_JUSTIFY_NONFAT)) == FMT_CLASSIFY && pfi->IsSymLink())
                ++name_width;   // For appending '@' symbol.
            if (m_longest_file_width < name_width)
                m_longest_file_width = name_width;
            if (picture->IsFilenameWidthNeeded())
            {
                const bool isFAT = Settings().IsSet(FMT_FAT);
                if ((Settings().IsSet(FMT_JUSTIFY_FAT) && isFAT) ||
                    (Settings().IsSet(FMT_JUSTIFY_NONFAT) && !isFAT))
                {
                    const WCHAR* name = pfi->GetFileName(flags).Text();
                    const WCHAR* ext = FindExtension(name);
                    unsigned noext_width = 0;
                    if (ext)
                        noext_width = __wcswidth(name, unsigned(ext - name));
                    if (!noext_width)
                        noext_width = name_width;
                    if (m_longest_file_width < noext_width + 4)
                        m_longest_file_width = noext_width + 4;
                }
                for (auto stream = pfi->GetStreams(); stream && *stream; ++stream)
                {
                    // WARNING: Keep in sync with PictureFormatter::Format and
                    // its stream case for FLD_FILENAME.
                    name_width = GetIconWidth();
                    if (Settings().IsSet(FMT_REDIRECTED))
                    {
                        if (Settings().IsSet(FMT_FULLNAME))
                            name_width += __wcswidth(dir) + 1;
                        name_width += __wcswidth(pfi->GetLongName().Text());
                    }
                    else
                        name_width += 2;
                    name_width += __wcswidth(stream[0]->GetLongName().Text());
                    if (m_longest_file_width < name_width)
                        m_longest_file_width = name_width;
                }
            }
        }
        if (IsGradientColorScaleMode() && (GetColorScaleFields() & SCALE_SIZE))
        {
            for (WhichFileSize which = FILESIZE_ARRAY_SIZE; which = WhichFileSize(int(which) - 1);)
            {
                if (which != FILESIZE_COMPRESSED || Settings().m_need_compressed_size)
                    Settings().UpdateMinMaxSize(which, pfi->GetFileSize(which));
            }
            for (auto stream = pfi->GetStreams(); stream && *stream; ++stream)
                Settings().UpdateMinMaxSize(FILESIZE_FILESIZE, stream[0]->GetFileSize());
        }
    }

    if (IsGradientColorScaleMode() && (GetColorScaleFields() & SCALE_TIME))
    {
        for (WhichTimeStamp which = TIMESTAMP_ARRAY_SIZE; which = WhichTimeStamp(int(which) - 1);)
            Settings().UpdateMinMaxTime(which, pfi->GetFileTime(which));
    }

    // Update the picture formatter.

    picture->OnFile(pfi.get());

    // The file might get displayed now, or might get deferred.

    if (!fUsage)
    {
        if (g_debug)
        {
            const bool is_dir = !!(pfi->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY);
            Printf(L"debug: OnFile %s '%s'\n", is_dir ? L"DIR " : L"file", pfi->GetLongName().Text());
        }

        if (fImmediate)
        {
            class OutputDisplayOne : public OutputOperation
            {
            public:
                OutputDisplayOne(std::unique_ptr<FileInfo>&& pfi)
                : m_pfi(std::move(pfi)) {}

                void Render(HANDLE h, const DirContext* dir) override
                {
                    DisplayOne(h, m_pfi.get(), nullptr, dir);
                }

            private:
                std::unique_ptr<FileInfo> m_pfi;
            };

            Render(new OutputDisplayOne(std::move(pfi)));
        }
        else
        {
            m_files.emplace_back(std::move(pfi));
        }
    }
}

static void FormatTotalCount(StrW& s, unsigned c, const DirFormatSettings& settings)
{
    const bool fCompressed = settings.IsSet(FMT_COMPRESSED);
    const unsigned width = fCompressed ? 13 : 16;

    FormatSizeForReading(s, c, width, settings);
}

static void FormatFileTotals(StrW& s, unsigned cFiles, unsigned __int64 cbTotal, unsigned __int64 cbAllocated, unsigned __int64 cbCompressed, const DirFormatSettings& settings)
{
    const bool fCompressed = settings.IsSet(FMT_COMPRESSED);
    const unsigned width = fCompressed ? 13 : 15;

    FormatTotalCount(s, cFiles, settings);
    s.Append(L" File(s) ");
    FormatSizeForReading(s, cbTotal, 14, settings);
    s.Append(L" bytes");
    if (fCompressed && cbCompressed)
    {
        s.Append(L"  ");
        FormatCompressed(s, cbCompressed, cbTotal, FILE_ATTRIBUTE_COMPRESSED);
    }
    if (cbAllocated)
    {
        s.Append(' ');
        FormatSizeForReading(s, cbAllocated, width, settings);
        s.Append(L" bytes allocated");
    }
}

void DirEntryFormatter::OnDirectoryEnd(const WCHAR* dir, bool next_dir_is_different)
{
#ifdef DEBUG
    assert(m_in_dir);
    m_in_dir = false;
#endif

    bool do_end = next_dir_is_different;

    if (Settings().IsSet(FMT_USAGEGROUPED))
        do_end = (m_subdirs.empty() ||
                  IsNewRootGroup(m_subdirs.front()->dir.Text()));

    if (do_end)
    {
        m_cFilesTotal += m_cFiles;
        m_cbTotalTotal += m_cbTotal;
        m_cbAllocatedTotal += m_cbAllocated;
        m_cbCompressedTotal += m_cbCompressed;
    }

    // List any files not already listed by OnFile.

    if (!m_files.empty())
    {
        // Sort files.

        bool clear_sort_order = false;
        if (m_grouped_patterns && !*g_sort_order)
        {
            clear_sort_order = true;
            wcscpy_s(g_sort_order, L"n");
        }

        if (*g_sort_order)
        {
            UINT tick_begin;
            if (g_debug)
                tick_begin = GetTickCount();

            std::sort(m_files.begin(), m_files.end(), CmpFileInfo);

            if (g_debug)
            {
                const UINT elapsed = GetTickCount() - tick_begin;
                Printf(L"debug: sort %zu file(s) in %u ms\n", m_files.size(), elapsed);
            }
        }

        if (m_grouped_patterns && m_files.size())
        {
            // Remove duplicates.  By moving unique items into a new array.
            // This is (much) more efficient than modifying the array in situ,
            // because this doesn't insert or delete and thus doesn't shift
            // items.  It's easy to overlook that insert and delete are O(N)
            // operations.  But when inserting or deleting, the loop becomes
            // exponential instead of linear.

            // Move the first item, which by definition is unique.
            std::vector<std::unique_ptr<FileInfo>> files;
            files.emplace_back(std::move(m_files[0]));

            // Loop and move other unique items.
            for (size_t i = 1; i < m_files.size(); ++i)
            {
                auto& hare = m_files[i];
                if (!hare->GetLongName().Equal(files.back()->GetLongName()))
                    files.emplace_back(std::move(hare));
            }

            // Swap the uniquely filtered array into place.
            m_files.swap(files);
        }

        if (clear_sort_order)
            *g_sort_order = '\0';

        // List files.

        class OutputFileList : public OutputOperation
        {
        public:
            OutputFileList(std::vector<std::unique_ptr<FileInfo>>&& files, unsigned num_columns,
                           unsigned longest_file_width, unsigned longest_dir_width)
            : m_files(std::move(files)), m_num_columns(num_columns)
            , m_longest_file_width(longest_file_width), m_longest_dir_width(longest_dir_width) {}

            void Render(HANDLE h, const DirContext* dir) override
            {
                const FormatFlags flags = dir->flags;
                const PictureFormatter& picture = *dir->picture;

                // Must use dir->picture directly due to const.
                dir->picture->SetMaxFileDirWidth(m_longest_file_width, m_longest_dir_width);

                switch (m_num_columns)
                {
                case 1:
                    {
                        for (size_t ii = 0; ii < m_files.size(); ii++)
                        {
                            const FileInfo* const pfi = m_files[ii].get();

                            DisplayOne(h, pfi, nullptr, dir);
                        }
                    }
                    break;

                case 0:
                case 2:
                case 4:
                    {
                        assert(!(flags & FMT_BARE));
                        assert(!(flags & FMT_COMPRESSED));
                        assert(implies(m_num_columns != 0, !(flags & FMT_ATTRIBUTES)));

                        const bool isFAT = !!(flags & FMT_FAT);
                        unsigned console_width = LOWORD(GetConsoleColsRows(h));
                        if (!console_width)
                            console_width = 80;

                        if (GetIconWidth() && isFAT)
                        {
                            if ((m_num_columns == 2 && console_width <= (GetIconWidth() + 38) * 2 + 3) ||
                                (m_num_columns == 4 && console_width <= (GetIconWidth() + 17) * 4 + 3))
                            {
                                SetUseIcons(L"never");
                            }
                        }

                        StrW s;
                        const bool vertical = !!(flags & FMT_SORTVERTICAL);
                        const unsigned spacing = (m_num_columns != 0 || isFAT || picture.HasDate() ||
                                                (m_num_columns == 0 && picture.HasGit())) ? 3 : 2;

                        ColumnWidths col_widths;
                        std::vector<PictureFormatter> col_pictures;
                        const bool autofit = picture.CanAutoFitFilename();
                        if (!autofit)
                        {
                            // Must use dir->picture directly due to const.
                            const unsigned max_per_file_width = dir->picture->GetMaxWidth(console_width - 1, true);
                            assert(implies(console_width >= 80, m_num_columns * (max_per_file_width + 3) < console_width + 3));
                            for (unsigned num = std::max<unsigned>((console_width + spacing - 1) / (max_per_file_width + spacing), unsigned(1)); num--;)
                            {
                                col_widths.emplace_back(max_per_file_width);
                                col_pictures.emplace_back(picture);
                            }
                        }
                        else
                        {
                            // This would like to fit each field independently
                            // for each column.  But the bookkeeping for that
                            // is expensive -- it needs (C*(C+1))/2 picture
                            // formatters, where C is max number of columns
                            // supported (SUPPORTED, not used).  It would need
                            // to do width fitting in all of them, but only
                            // for a subset of items in each, until that
                            // candidate number of columns gets invalidated.
                            // It's certainly possible, but I'm not convinced
                            // it's worth the overhead performance cost, just
                            // to save a couple characters here or there.
                            //
                            // Instead, this allows the Filename field(s) to
                            // autofit, and uses the pre-calculated minimum
                            // field widths based on the full collection of
                            // files.
                            //
                            // But, it might be reasonable to refactor the
                            // width fitting calculations so they happen
                            // incrementally during CalculateColumns, instead
                            // of happening during the file system scan, when
                            // rendering is not Immediate.
                            col_widths = CalculateColumns([this, &picture](size_t i){
                                return picture.GetMinWidth(m_files[i].get());
                            }, m_files.size(), vertical, spacing, console_width - 1);

                            if (col_widths.empty())
                            {
                                col_pictures.emplace_back(picture);
                                col_pictures.back().GetMaxWidth(console_width - 1, true);
                            }
                            else
                            {
                                for (unsigned i = 0; i < col_widths.size(); ++i)
                                {
                                    col_pictures.emplace_back(picture);
                                    col_pictures.back().GetMaxWidth(col_widths[i], true);
                                }
                            }
                        }

                        const unsigned num_per_row = std::max<unsigned>(1, unsigned(col_widths.size()));
                        const unsigned num_rows = unsigned(m_files.size() + num_per_row - 1) / num_per_row;
                        const unsigned num_add = vertical ? num_rows : 1;

                        unsigned prev_len;
                        for (unsigned ii = 0; ii < num_rows; ii++)
                        {
                            auto picture = col_pictures.begin();

                            s.Clear();
                            prev_len = 0;

                            unsigned iItem = vertical ? ii : ii * num_per_row;
                            for (unsigned jj = 0; jj < num_per_row && iItem < m_files.size(); jj++, iItem += num_add)
                            {
                                const FileInfo* pfi = m_files[iItem].get();
                                assert(!pfi->GetStreams());

                                if (jj)
                                {
                                    const unsigned width = cell_count(s.Text() + prev_len);
                                    const unsigned spaces = col_widths[jj - 1] - width + spacing;
                                    s.AppendSpaces(spaces);
                                }

                                prev_len = s.Length();
                                picture->Format(s, pfi, nullptr, false/*one_per_line*/);

                                ++picture;
                            }

                            s.Append(L"\n");
                            OutputConsole(h, s.Text(), s.Length());
                        }
                    }
                    break;

                default:
                    assert(false);
                    break;
                }
            }

        private:
            const std::vector<std::unique_ptr<FileInfo>> m_files;
            const unsigned m_num_columns;
            const unsigned m_longest_file_width;
            const unsigned m_longest_dir_width;
        };

        if (Settings().IsSet(FMT_TREE))
        {
            auto& find = s_tree_map.find(m_dir->dir.Text());
            if (find == s_tree_map.end())
            {
                std::unique_ptr<TreeFiles> tree_files = std::make_unique<TreeFiles>();
                tree_files->files = std::move(m_files);
                tree_files->dir = m_dir;
                s_tree_map.emplace(m_dir->dir.Text(), std::move(tree_files));
            }
            else
            {
                auto& tree_files = find->second->files;
                tree_files.insert(tree_files.end(),
                    std::make_move_iterator(m_files.begin()),
                    std::make_move_iterator(m_files.end()));
            }
        }
        else
        {
            Render(new OutputFileList(std::move(m_files), Settings().m_num_columns, m_longest_file_width, m_longest_dir_width));
        }
    }

    // Display summary.

    if (do_end)
    {
        if (Settings().IsSet(FMT_USAGE))
        {
            class OutputUsage : public OutputOperation
            {
            public:
                OutputUsage(DirEntryFormatter& def, unsigned __int64 total, unsigned __int64 alloc, unsigned count, const WCHAR* dir)
                : m_def(def), m_total(total), m_alloc(alloc), m_count(count), m_dir(dir) {}

                void Render(HANDLE h, const DirContext* dir) override
                {
                    const FormatFlags flags = dir->flags;

                    StrW s;
                    const WhichFileSize which = FILESIZE_FILESIZE;
                    FormatSize(s, m_total, &which, m_def.Settings(), 0);
                    s.Append(L"  ");
                    FormatSize(s, m_alloc, &which, m_def.Settings(), 0);
                    s.Printf(L"  %7u  ", m_count);
                    s.Append(m_dir);
                    s.Append('\n');

                    OutputConsole(h, s.Text(), s.Length());
                }

            private:
                DirEntryFormatter& m_def;
                const unsigned __int64 m_total;
                const unsigned __int64 m_alloc;
                const unsigned m_count;
                const StrW m_dir;
            };

            StrW dir;
            dir.Set(Settings().IsSet(FMT_USAGEGROUPED) ? m_root_group : m_dir->dir);
            StripTrailingSlashes(dir);
            if (Settings().IsSet(FMT_LOWERCASE))
                dir.ToLower();
            Render(new OutputUsage(*this, m_cbTotal, m_cbAllocated, CountFiles(), dir.Text()));
            m_count_usage_dirs++;
        }
        else if (!Settings().IsSet(FMT_BARE|FMT_NOSUMMARY))
        {
            StrW s;
            FormatFileTotals(s, CountFiles(), m_cbTotal, m_cbAllocated, m_cbCompressed, Settings());
            s.Append('\n');
            Render(new OutputText(std::move(s)));
        }

        if (g_debug)
        {
            const UINT elapsed = GetTickCount() - m_tick_begin;
            Printf(L"debug: elapsed %u ms\n", elapsed);
        }

        m_cFiles = 0;
        m_cbTotal = 0;
        m_cbAllocated = 0;
        m_cbCompressed = 0;
    }

    m_line_break_before_miniheader = true;
}

void DirEntryFormatter::OnPatternEnd(const DirPattern* pattern)
{
    if (Settings().IsSet(FMT_TREE))
    {
        assert(s_tree_stack.empty());

        bool got_info = false;
        WIN32_FIND_DATAW fd = { 0 };
        {
            SHFile h = CreateFile(pattern->m_dir.Text(), FILE_READ_ATTRIBUTES|SYNCHRONIZE,
                                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, nullptr,
                                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT, 0);
            if (!h.Empty())
            {
                BY_HANDLE_FILE_INFORMATION bhfi;
                got_info = !!GetFileInformationByHandle(h, &bhfi);
                h.Close();
                if (got_info)
                {
                    fd.dwFileAttributes = bhfi.dwFileAttributes;
                    fd.ftCreationTime = bhfi.ftCreationTime;
                    fd.ftLastAccessTime = bhfi.ftLastAccessTime;
                    fd.ftLastWriteTime = bhfi.ftLastWriteTime;
                    // fd.nFileSizeHigh = bhfi.nFileSizeHigh;
                    // fd.nFileSizeLow = bhfi.nFileSizeLow;
                }
            }

            if (!got_info)
                fd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        }

        StrW rel_str;
        if (Settings().IsSet(FMT_BARERELATIVE))
            rel_str.Set(pattern->m_dir_rel);
        if (rel_str.Empty())
            rel_str.Set(pattern->m_dir);
        StripTrailingSlashes(rel_str);
        wcscpy_s(fd.cFileName, rel_str.Text());

        std::unique_ptr<FileInfo> info = std::make_unique<FileInfo>();
        info->Init(pattern->m_dir.Text(), 0, &fd, Settings());
        SetAttrsForColors(~(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM));
        if (got_info)
        {
            DisplayOne(m_hout, info.get(), nullptr, m_dir.get());
        }
        else
        {
            StrW s;
            const WCHAR* color = SelectColor(info.get(), Settings().m_flags, pattern->m_dir.Text());
            FormatFilename(s, info.get(), Settings().m_flags/* & (FMT_COLORS|FMT_CLASSIFY|FMT_HYPERLINKS|FMT_LOWERCASE|FMT_TREE)*/);
            OutputConsole(m_hout, s.Text(), s.Length(), color);
            OutputConsole(m_hout, L"\n");
        }
        SetAttrsForColors(~0);

        const auto& t = s_tree_map.find(pattern->m_dir.Text());
        if (t != s_tree_map.end())
        {
            s_tree_stack.emplace_back(t->second.get());

            StrW tmp;
            while (!s_tree_stack.empty())
            {
                auto frame = s_tree_stack.back();
                if (frame->cursor >= frame->files.size())
                {
                    s_tree_stack.pop_back();
                    continue;
                }

                const FileInfo* pfi = frame->files[frame->cursor].get();
                DisplayOne(m_hout, pfi, nullptr, frame->dir.get());

                if (pfi->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY)
                {
                    PathJoin(tmp, frame->dir->dir.Text(), pfi->GetLongName());
                    const auto& sub = s_tree_map.find(tmp.Text());
                    if (sub != s_tree_map.end())
                        s_tree_stack.emplace_back(sub->second.get());
                }

                ++frame->cursor;
            }
        }

        s_tree_stack.clear();
        s_tree_map.clear();
    }
}

void DirEntryFormatter::OnVolumeEnd(const WCHAR* dir)
{
    if (Settings().IsSet(FMT_NOSUMMARY))
        return;

    Error e;
    StrW root;
    if (!GetDrive(dir, root, e))
        return;

    if (Settings().IsSet(FMT_USAGE))
    {
        if (m_count_usage_dirs > 1)
        {
            StrW s;

            FormatSize(s, m_cbTotalTotal, nullptr, Settings(), 0);
            s.Append(L"  ");
            FormatSize(s, m_cbAllocatedTotal, nullptr, Settings(), 0);
            s.Printf(L"  %7u  Total\n", m_cFilesTotal);

            ULARGE_INTEGER ulFreeToCaller;
            ULARGE_INTEGER ulTotalSize;
            ULARGE_INTEGER ulTotalFree;
            if (SHGetDiskFreeSpace(root.Text(), &ulFreeToCaller, &ulTotalSize, &ulTotalFree))
            {
                ULARGE_INTEGER ulTotalUsed;
                ulTotalUsed.QuadPart = ulTotalSize.QuadPart - ulTotalFree.QuadPart;

                FormatSizeForReading(s, *reinterpret_cast<unsigned __int64*>(&ulTotalUsed), 0, Settings());
                s.Append('/');
                FormatSizeForReading(s, *reinterpret_cast<unsigned __int64*>(&ulTotalSize), 0, Settings());

                double dInUse = double(ulTotalUsed.QuadPart) / double(ulTotalSize.QuadPart);
                s.Printf(L"  %.1f%% of disk in use\n", dInUse * 100);
            }

            Render(new OutputText(std::move(s)));
        }
        return;
    }

    if (Settings().IsSet(FMT_BARE))
        return;

    // Display total files and sizes, if recursing into subdirectories.

    StrW s;

    if (Settings().IsSet(FMT_SUBDIRECTORIES))
    {
        s.Set(L"\n  ");
        if (!Settings().IsSet(FMT_COMPRESSED))
            s.Append(L"   ");
        s.Append(L"Total Files Listed:\n");
        FormatFileTotals(s, m_cFilesTotal, m_cbTotalTotal, m_cbAllocatedTotal, m_cbCompressedTotal, Settings());
        s.Append('\n');
        Render(new OutputText(std::move(s)));
    }

    // Display count of directories, and bytes free on the volume.  For
    // similarity with CMD the count of directories includes the "." and
    // ".." pseudo-directories, even though it is a bit inaccurate.

    ULARGE_INTEGER ulFreeToCaller;
    ULARGE_INTEGER ulTotalSize;
    ULARGE_INTEGER ulTotalFree;

    FormatTotalCount(s, CountDirs(), Settings());
    s.Append(L" Dir(s)  ");
    if (SHGetDiskFreeSpace(root.Text(), &ulFreeToCaller, &ulTotalSize, &ulTotalFree))
    {
        FormatSizeForReading(s, *reinterpret_cast<unsigned __int64*>(&ulFreeToCaller), 15, Settings());
        s.Printf(L" bytes free");
    }
    s.Append('\n');
    Render(new OutputText(std::move(s)));
}

void DirEntryFormatter::Finalize()
{
    m_dir.reset();

    for (auto& o : m_outputs)
    {
        o->Render(m_hout, m_dir.get());
        o.reset();
    }

    m_outputs.clear();
}

void DirEntryFormatter::ReportError(Error& e)
{
    class OutputErrorMessage : public OutputOperation
    {
    public:
        OutputErrorMessage(StrW&& s, bool color=true)
        : m_color(color)
        {
            m_s = std::move(s);
            assert(s.Empty());
        }

        void Render(HANDLE h, const DirContext* dir) override
        {
            h = GetStdHandle(STD_ERROR_HANDLE);
            const WCHAR* color = (m_color && CanUseEscapeCodes(h)) ? c_error : nullptr;
            const WCHAR* text = m_s.Text();
            const WCHAR* trailing = m_s.Text() + m_s.Length();
            while (trailing > text)
            {
                WCHAR ch = *(trailing - 1);
                if (ch != '\r' && ch != '\n')
                    break;
                --trailing;
            }

            OutputConsole(h, text, unsigned(trailing - text), color);
            if (*trailing)
                OutputConsole(h, trailing);
        }

    private:
        StrW m_s;
        const bool m_color;
    };

    StrW s;
    e.Format(s);
    Render(new OutputErrorMessage(std::move(s)));
}

void DirEntryFormatter::AddSubDir(const StrW& dir, const StrW& dir_rel, unsigned depth, const std::shared_ptr<const GlobPatterns>& git_ignore, const std::shared_ptr<const RepoStatus>& repo)
{
    assert(Settings().IsSet(FMT_SUBDIRECTORIES));
    assert(!IsPseudoDirectory(dir.Text()));

    std::unique_ptr<SubDir> subdir = std::make_unique<SubDir>();
    subdir->dir.Set(dir);
    subdir->dir_rel.Set(dir_rel);
    subdir->depth = depth;
    subdir->git_ignore = git_ignore;
    m_pending_subdirs.emplace_back(std::move(subdir));

    if (Settings().IsSet(FMT_GITIGNORE))
    {
        StrW file(dir);
        EnsureTrailingSlash(file);
        file.Append(L".gitignore");
        SHFile h = CreateFile(file.Text(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, 0);
        if (!h.Empty())
        {
            std::shared_ptr<GlobPatterns> globs = std::make_shared<GlobPatterns>();
            if (globs && globs->Load(h))
            {
                if (globs->Count())
                {
                    Printf(L"debug: .gitignore for '%s'\n", dir.Text());
                    globs->Dump();
                }

                subdir->git_ignore = globs;
            }
        }
    }

    if (Settings().IsSet(FMT_GIT|FMT_GITREPOS))
    {
        subdir->repo = s_repo_map.Find(dir.Text());
        if (!subdir->repo)
            subdir->repo = repo;
    }
}

void DirEntryFormatter::SortSubDirs()
{
    if (!m_pending_subdirs.empty())
    {
        std::sort(m_pending_subdirs.begin(), m_pending_subdirs.end(), CmpSubDirs);

        assert(m_subdirs.empty() || CmpSubDirs(m_pending_subdirs.back(), m_subdirs.front()));

        for (size_t ii = m_pending_subdirs.size(); ii--;)
            m_subdirs.push_front(std::move(m_pending_subdirs[ii]));

        m_pending_subdirs.clear();
    }
}

bool DirEntryFormatter::NextSubDir(StrW& dir, StrW& dir_rel, unsigned& depth, std::shared_ptr<const GlobPatterns>& git_ignore, std::shared_ptr<const RepoStatus>& repo)
{
    if (m_subdirs.empty())
    {
        dir.Clear();
        dir_rel.Clear();
        depth = 0;
        git_ignore.reset();
        repo.reset();
        return false;
    }

    auto& front = m_subdirs.front();
    dir = std::move(front->dir);
    dir_rel = std::move(front->dir_rel);
    depth = front->depth;
    git_ignore = std::move(front->git_ignore);
    repo = std::move(front->repo);
    m_subdirs.pop_front();

    // The only thing that needs the map is FormatGitRepo(), to avoid running
    // "git status" twice for the same repo.  So, once traversal dives into a
    // directory, nothing will try to print that directory entry again, so the
    // map doesn't need to hold a reference anymore.  Removing it from the map
    // ensures the data structures can be freed once traversal through that
    // repo tree finishes.
    s_repo_map.Remove(dir.Text());

    return true;
}

void DirEntryFormatter::Render(OutputOperation* o)
{
    if (IsDelayedRender())
    {
        // When gradient color scale is applied, the min and max should cover
        // the entire collection of output.  So all output operations must be
        // queued until all sizes and times have been analyzed.
        assert(Settings().IsSet(FMT_COLORS));
        m_outputs.emplace_back(std::unique_ptr<OutputOperation>(o));
    }
    else
    {
        assert(m_outputs.empty());
        o->Render(m_hout, m_dir.get());
        delete o;
    }
}

void AppendTreeLines(StrW& s, const FormatFlags flags)
{
    const bool ascii = IsAsciiLineCharMode();
    const bool colors = !!(flags & FMT_COLORS);
    const WCHAR* punct = colors ? GetColorByKey(L"xx") : nullptr;
    for (unsigned ll = 0; ll < s_tree_stack.size(); ++ll)
    {
        const WCHAR* text;
        unsigned spaces;
        const auto& level = s_tree_stack[ll];
        if (level->cursor >= level->files.size())
        {
            text = nullptr;
            spaces = 4;
        }
        else if (ll + 1 < s_tree_stack.size())
        {
            text = ascii ? L"|" : L"\u2502";
            spaces = 3;
        }
        else if (level->cursor + 1 < level->files.size())
        {
            text = ascii ? L"|--" : L"\u251c\u2500\u2500";
            spaces = 1;
        }
        else
        {
            text = ascii ? L"+--" : L"\u2514\u2500\u2500";
            spaces = 1;
        }
        if (text)
        {
            s.AppendColor(punct);
            s.Append(text);
            s.AppendNormalIf(punct);
        }
        s.AppendSpaces(spaces);
    }
}

