// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "flags.h"
#include "sorting.h"
#include "patterns.h"

static bool s_ascii_sort = false;
static bool s_reverse_all = false;
static bool s_explicit_extension = false;

// Reverses the combination of g_sort_order and s_reverse_all.
static bool s_reverse_sort_order = false;

// Longest possible sort order string is "-c-d-e-g-n-s".
WCHAR g_sort_order[16] = L"";

void SetSortOrder(const WCHAR* order, Error& e)
{
    s_ascii_sort = false;
    s_reverse_all = false;
    s_explicit_extension = false;
    ZeroMemory(g_sort_order, sizeof(g_sort_order));

    WCHAR* set = g_sort_order;

    SkipColonOrEqual(order);

    if (wcsicmp(order, L"u") == 0)
        return;
    if (wcschr(order, 'u'))
    {
        e.Set(L"Invalid sort order '%1'; 'u' may only be used by itself.") << order;
        return;
    }

    bool need_gn = true;
    bool minus = false;

    while (*order)
    {
        if (*order == '-')
        {
            if (*(order + 1) == '-')
            {
                e.Set(L"Invalid sort order '--'.");
                return;
            }
            minus = true;
        }
        else
        {
            if (*order == 'a')
            {
                s_ascii_sort = true;
            }
            else if (*order == 'r')
            {
                s_reverse_all = true;
                need_gn = false;
            }
            else
            {
                if (*order == 'e')
                    s_explicit_extension = true;
                if (!wcschr(L"nesgdc", *order))
                {
                    WCHAR tmp[2];
                    tmp[0] = *order;
                    tmp[1] = 0;
                    e.Set(L"Invalid sort order '%1'.") << tmp;
                    return;
                }
                // Prevent duplicates, for efficiency.
                if (!wcschr(g_sort_order, *order))
                {
                    if (minus)
                        *(set++) = '-';
                    *(set++) = *order;
                }
                need_gn = false;
            }
            minus = false;
        }
        ++order;
    }

    if (need_gn)
    {
        if (minus)
            *(set++) = '-';
        *(set++) = 'g';
        *(set++) = 'n';
    }

    assert(!*set);
}

void SetReverseSort(bool reverse)
{
    s_reverse_sort_order = reverse;
}

bool IsReversedSort()
{
    return (s_reverse_all ^ s_reverse_sort_order);
}

inline bool IsBeginNumeric(const WCHAR* p)
{
    return (*p >= '0' && *p <= '9');
}

static LONG ParseNum(const WCHAR*& p)
{
    LONG num = 0;

    while (*p <= '9' && *p >= '0')
    {
        num *= 10;
        num += (*p) - '0';
        p++;
    }

    return num;
}

DWORD g_dwCmpStrFlags = 0;

int Sorting::CmpStrN(const WCHAR* p1, int len1, const WCHAR* p2, int len2)
{
    const int n = CompareStringW(LOCALE_USER_DEFAULT, g_dwCmpStrFlags, p1, len1, p2, len2);
    if (!n)
    {
        assert(false);
        return 0;
    }
    return n - 2;
}

int Sorting::CmpStrNI(const WCHAR* p1, int len1, const WCHAR* p2, int len2)
{
    const int n = CompareStringW(LOCALE_USER_DEFAULT, g_dwCmpStrFlags|NORM_IGNORECASE, p1, len1, p2, len2);
    if (!n)
    {
        assert(false);
        return 0;
    }
    return n - 2;
}

static int StrAlphaNumCompare(const WCHAR* p1, const WCHAR* p2)
{
    while (true)
    {
        const WCHAR* end1 = p1;
        const WCHAR* end2 = p2;

        // Compare a run of text.

        if (g_dwCmpStrFlags & SORT_STRINGSORT)
        {
            while (*end1 &&
                   *end2 &&
                   !(IsBeginNumeric(end1) && IsBeginNumeric(end2)))
            {
                end1 = CharNext(end1);
                end2 = CharNext(end2);
            }

            if (!IsBeginNumeric(end1))
            {
                while (*end1)
                    end1 = CharNext(end1);
            }
            if (!IsBeginNumeric(end2))
            {
                while (*end2)
                    end2 = CharNext(end2);
            }
        }
        else
        {
            while (*end1 && !IsBeginNumeric(end1))
                end1 = CharNext(end1);
            while (*end2 && !IsBeginNumeric(end2))
                end2 = CharNext(end2);
        }

        assert(implies(*end1, IsBeginNumeric(end1)));
        assert(implies(*end2, IsBeginNumeric(end2)));

        const int result = Sorting::CmpStrNI(p1, int(end1 - p1), p2, int(end2 - p2));
        if (result || !*end1)
            return result;

        p1 = end1;
        p2 = end2;

        // Compare a run of digits.

        assert(!*p1 || IsBeginNumeric(p1));
        assert(!*p2 || IsBeginNumeric(p2));

        const LONG num1 = ParseNum(p1);  // passed by reference
        const LONG num2 = ParseNum(p2);  // passed by reference

        if (num1 < num2)
            return -1;
        if (num1 > num2)
            return 1;
    }
}

class NameSplitter
{
public:
    NameSplitter(const WCHAR* p, unsigned index) : m_p(const_cast<WCHAR*>(p) + index), m_ch(p[index]) { *m_p = 0; }
    ~NameSplitter() { *m_p = m_ch; }

private:
    WCHAR* const    m_p;
    const WCHAR     m_ch;
};

bool CmpFileInfo(const std::unique_ptr<FileInfo>& fi1, const std::unique_ptr<FileInfo>& fi2)
{
    assert(g_settings);

    const FileInfo* const pfi1 = fi1.get();
    const FileInfo* const pfi2 = fi2.get();
    const bool is_file1 = !(pfi1->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY);
    const bool is_file2 = !(pfi2->GetAttributes() & FILE_ATTRIBUTE_DIRECTORY);

    const WCHAR* const name1 = pfi1->GetLongName().Text();
    const WCHAR* const name2 = pfi2->GetLongName().Text();
    const WCHAR* const _ext1 = FindExtension(name1);
    const WCHAR* const _ext2 = FindExtension(name2);
    const unsigned name_len1 = _ext1 ? unsigned(_ext1 - name1) : unsigned(pfi1->GetLongName().Length());
    const unsigned name_len2 = _ext2 ? unsigned(_ext2 - name2) : unsigned(pfi2->GetLongName().Length());
    const WCHAR* const ext1 = _ext1 ? _ext1 : L"";
    const WCHAR* const ext2 = _ext2 ? _ext2 : L"";

    int n = 0;
    for (const WCHAR* order = g_sort_order; !n && *order; order++)
    {
        bool reverse = (*order == '-');

        if (reverse)
        {
            order++;
            if (!*order)
                break;
        }

        switch (*order)
        {
        case 'g':
            if (is_file1 != is_file2)
                n = is_file1 ? 1 : -1;
            break;
        case 'n':
            if (s_explicit_extension)
            {
                NameSplitter split1(name1, name_len1);
                NameSplitter split2(name2, name_len2);
                if (s_ascii_sort)
                {
                    n = Sorting::CmpStrI(name1, name2);
                }
                else
                {
                    n = StrAlphaNumCompare(name1, name2);
                    if (!n)
                    {
                        // The lengths can differ when the strings are equal
                        // if they contain digits (e.g. "foo001" vs "foo1").
                        if (name_len1 < name_len2)
                            n = -1;
                        else if (name_len1 > name_len2)
                            n = 1;
                        else
                            n = Sorting::CmpStrI(ext1, ext2);
                    }
                }
            }
            else
            {
                if (s_ascii_sort)
                    n = Sorting::CmpStrI(name1, name2);
                else
                    n = StrAlphaNumCompare(name1, name2);
            }
            break;
        case 'e':
            if (s_ascii_sort)
                n = Sorting::CmpStrI(ext1, ext2);
            else
                n = StrAlphaNumCompare(ext1, ext2);
            break;
        case 's':
            {
                const unsigned __int64 cb1 = pfi1->GetFileSize(g_settings->m_whichfilesize);
                const unsigned __int64 cb2 = pfi2->GetFileSize(g_settings->m_whichfilesize);
                if (cb1 < cb2)
                    n = -1;
                else if (cb1 > cb2)
                    n = 1;
            }
            break;
        case 'd':
            {
                const FILETIME* const pft1 = &pfi1->GetFileTime(g_settings->m_whichtimestamp);
                const FILETIME* const pft2 = &pfi2->GetFileTime(g_settings->m_whichtimestamp);
                n = CompareFileTime(pft1, pft2);
            }
            break;
        case 'c':
            {
                const float ratio1 = pfi1->GetCompressionRatio();
                const float ratio2 = pfi2->GetCompressionRatio();
                if (ratio1 < ratio2)
                    n = -1;
                else if (ratio1 > ratio2)
                    n = 1;
            }
            break;
        }

        if (reverse)
            n = -n;
    }

    return n < 0;
}

bool CmpSubDirs(const std::unique_ptr<SubDir>& d1, const std::unique_ptr<SubDir>& d2)
{
    return Sorting::CmpStrI(d1->dir.Text(), d2->dir.Text()) < 0;
}

