/*
 * Copyright (c), 2016 David Aguilar
 * Based on the fnmatch implementation from OpenBSD.
 *
 * Copyright (c) 1989, 1993, 1994
 *  The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* begin_dirx_change */
/*
 * NOTE:    wildmatch used char, but dirx needs WCHAR, so this copy has all
 *          char changed to WCHAR.
 *          Christopher Antos, sparrowhawk996@gmail.com, github.com/chrisant996
 */
/* end_dirx_change */

/* begin_dirx_change */
#include <windows.h>
/* end_dirx_change */
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "wildmatch.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EOS '\0'
#define RANGE_MATCH 1
#define RANGE_NOMATCH 0
#define RANGE_ERROR -1

#define check_flag(flags, opts) ((flags) & (opts))

static int rangematch(const WCHAR *, WCHAR, int, const WCHAR **);

/* begin_dirx_change */
inline int is_slash(WCHAR ch, int flags)
{
    return (ch == '/') || (check_flag(flags, WM_SLASHFOLD) && ch == '\\');
}

static WCHAR const *find_slash(const WCHAR *s, int flags)
{
    while (*s)
    {
        if (is_slash(*s, flags))
            return s;
        s++;
    }
    return NULL;
}
/* end_dirx_change */

int wildmatch(const WCHAR *pattern, const WCHAR *string, int flags)
{
    const WCHAR *stringstart;
    const WCHAR *newp;
    const WCHAR *slash;
    WCHAR c, test;
    WCHAR prev;
    int wild = 0;

    /* WM_WILDSTAR implies WM_PATHNAME. */
    if (check_flag(flags, WM_WILDSTAR)) {
        flags |= WM_PATHNAME;
    }

    for (stringstart = string;;) {
        switch (c = *pattern++) {
        case EOS:
/* begin_dirx_change */
            //if (check_flag(flags, WM_LEADING_DIR) && *string == '/')
            if (check_flag(flags, WM_LEADING_DIR) && is_slash(*string, flags))
/* end_dirx_change */
                return WM_MATCH;
            return (*string == EOS) ? WM_MATCH : WM_NOMATCH;
        case '?':
            if (*string == EOS)
                return WM_NOMATCH;
/* begin_dirx_change */
            //if (*string == '/' && check_flag(flags, WM_PATHNAME))
            if (is_slash(*string, flags) && check_flag(flags, WM_PATHNAME))
/* end_dirx_change */
                return WM_NOMATCH;
            if (*string == '.' && check_flag(flags, WM_PERIOD) &&
                (string == stringstart ||
/* begin_dirx_change */
                //(check_flag(flags, WM_PATHNAME) && *(string - 1) == '/')))
                (check_flag(flags, WM_PATHNAME) && is_slash(*(string - 1), flags))))
/* end_dirx_change */
                return WM_NOMATCH;
            ++string;
            break;
        case '*':
            c = *pattern;
            wild = check_flag(flags, WM_WILDSTAR) && c == '*';
            if (wild) {
                prev = pattern[-2];
                /* Collapse multiple stars and slash-** patterns,
                 * e.g. "** / *** / **** / **" (without spaces)
                 * is treated as a single ** wildstar.
                 */
                while (c == '*') {
                    c = *++pattern;
                }

                while (c == '/' && pattern[1] == '*' && pattern[2] == '*') {
                    prev = c;
                    c = *++pattern;
                    while (c == '*') {
                        c = *++pattern;
                    }
                }
                if (c == '/' &&
                        wildmatch(pattern+1, string, flags) == WM_MATCH) {
                    return WM_MATCH;
                }
            } else {
                /* Collapse multiple stars. */
                while (c == '*') {
                    c = *++pattern;
                }
            }

            if (!wild && *string == '.' && check_flag(flags, WM_PERIOD) &&
                (string == stringstart ||
/* begin_dirx_change */
                //(check_flag(flags, WM_PATHNAME) && *(string - 1) == '/'))) {
                (check_flag(flags, WM_PATHNAME) && is_slash(*(string - 1), flags)))) {
/* end_dirx_change */
                return WM_NOMATCH;
            }
            /* Optimize for pattern with * or ** at end or before /. */
            if (c == EOS) {
                if (wild && prev == '/') {
                    return WM_MATCH;
                }
                if (check_flag(flags, WM_PATHNAME)) {
                    return (check_flag(flags, WM_LEADING_DIR) ||
/* begin_dirx_change */
                        //strchr(string, '/') == NULL ?  WM_MATCH : WM_NOMATCH);
                        find_slash(string, flags) == NULL ?  WM_MATCH : WM_NOMATCH);
/* end_dirx_change */
                } else {
                    return WM_MATCH;
                }
            } else if (c == '/') {
                if (wild) {
/* begin_dirx_change */
                    //slash = strchr(stringstart, '/');
                    slash = find_slash(stringstart, flags);
/* end_dirx_change */
                    if (!slash) {
                        return WM_NOMATCH;
                    }
                    while (slash) {
                        if (wildmatch(pattern+1, slash+1, flags) == 0) {
                            return WM_MATCH;
                        }
/* begin_dirx_change */
                        //slash = strchr(slash+1, '/');
                        slash = find_slash(slash+1, flags);
/* end_dirx_change */
                    }
                } else {
                    if (check_flag(flags, WM_PATHNAME)) {
/* begin_dirx_change */
                        //if ((string = strchr(string, '/')) == NULL) {
                        if ((string = find_slash(string, flags)) == NULL) {
/* end_dirx_change */
                            return WM_NOMATCH;
                        }
                    }
                }
            } else if (wild) {
                return WM_NOMATCH;
            }
            /* General case, use recursion. */
            while ((test = *string) != EOS) {
                if (!wildmatch(pattern, string, flags & ~WM_PERIOD))
                    return WM_MATCH;
/* begin_dirx_change */
                //if (test == '/' && check_flag(flags, WM_PATHNAME))
                if (is_slash(test, flags) && check_flag(flags, WM_PATHNAME))
/* end_dirx_change */
                    break;
                ++string;
            }
            return WM_NOMATCH;
        case '[':
            if (*string == EOS)
                return WM_NOMATCH;
/* begin_dirx_change */
            //if (*string == '/' && check_flag(flags, WM_PATHNAME))
            if (is_slash(*string, flags) && check_flag(flags, WM_PATHNAME))
/* end_dirx_change */
                return WM_NOMATCH;
            if (*string == '.' && check_flag(flags, WM_PERIOD) &&
                    (string == stringstart ||
/* begin_dirx_change */
                     //(check_flag(flags, WM_PATHNAME) && *(string - 1) == '/')))
                     (check_flag(flags, WM_PATHNAME) && is_slash(*(string - 1), flags))))
/* end_dirx_change */
                return WM_NOMATCH;

            switch (rangematch(pattern, *string, flags, &newp)) {
            case RANGE_ERROR:
                /* not a good range, treat as normal text */
                ++string;
                goto normal;
            case RANGE_MATCH:
                pattern = newp;
                break;
            case RANGE_NOMATCH:
                return (WM_NOMATCH);
            }
            ++string;
            break;
        case '\\':
            if (!check_flag(flags, WM_NOESCAPE)) {
                if ((c = *pattern++) == EOS) {
                    c = '\\';
                    --pattern;
                    if (*(string+1) == EOS) {
                        return WM_NOMATCH;
                    }
                }
            }
            /* FALLTHROUGH */
        default:
        normal:
            if (c != *string && !(check_flag(flags, WM_CASEFOLD) &&
                 (towlower(c) ==
                 towlower(*string))))
/* begin_dirx_change */
                if (!(c == '/' && is_slash(*string, flags)))
/* end_dirx_change */
                return WM_NOMATCH;
            ++string;
            break;
        }
    /* NOTREACHED */
    }
}

static int
rangematch(const WCHAR *pattern, WCHAR test, int flags, const WCHAR **newp)
{
    int negate, ok;
    WCHAR c, c2;
    WCHAR tmp;

    /*
     * A bracket expression starting with an unquoted circumflex
     * character produces unspecified results (IEEE 1003.2-1992,
     * 3.13.2).  This implementation treats it like '!', for
     * consistency with the regular expression syntax.
     * J.T. Conklin (conklin@ngai.kaleida.com)
     */
    if ((negate = (*pattern == '!' || *pattern == '^')))
        ++pattern;

/* begin_dirx_change */
    if (is_slash(test, flags))
        test = '/';
    else
/* end_dirx_change */
    if (check_flag(flags, WM_CASEFOLD))
        test = towlower(test);

    /*
     * A right bracket shall lose its special meaning and represent
     * itself in a bracket expression if it occurs first in the list.
     * -- POSIX.2 2.8.3.2
     */
    ok = 0;
    c = *pattern++;
    do {
        if (c == '\\' && !check_flag(flags, WM_NOESCAPE))
            c = *pattern++;

        if (c == EOS)
            return RANGE_ERROR;

        if (c == '/' && check_flag(flags, WM_PATHNAME))
            return RANGE_NOMATCH;

        if (*pattern == '-'
            && (c2 = *(pattern+1)) != EOS && c2 != ']') {
            pattern += 2;
            if (c2 == '\\' && !check_flag(flags, WM_NOESCAPE))
                c2 = *pattern++;
            if (c2 == EOS)
                return RANGE_ERROR;

            if (check_flag(flags, WM_CASEFOLD)) {
                c = towlower(c);
                c2 = towlower(c2);
            }
            if (c > c2) {
                tmp = c2;
                c2 = c;
                c = tmp;
            }
            if (c <= test && test <= c2) {
                ok = 1;
            }
        }

        if (c == '[' && *pattern == ':' && *(pattern+1) != EOS) {

            #define match_pattern(name) \
                !wcsncmp(pattern+1, name, sizeof(name)-1)

            #define check_pattern(name, predicate, value) {{ \
                if (match_pattern(name L":]")) { \
                    if (predicate(value)) { \
                        ok = 1; \
                    } \
                    pattern += sizeof(name L":]"); \
                    continue; \
                } \
            }}

            if (match_pattern(L":]")) {
                continue;
            }
            check_pattern(L"alnum", iswalnum, test);
            check_pattern(L"alpha", iswalpha, test);
            check_pattern(L"blank", iswblank, test);
            check_pattern(L"cntrl", iswcntrl, test);
            check_pattern(L"digit", iswdigit, test);
            check_pattern(L"graph", iswgraph, test);
            check_pattern(L"lower", iswlower, test);
            check_pattern(L"print", iswprint, test);
            check_pattern(L"punct", iswpunct, test);
            check_pattern(L"space", iswspace, test);
            check_pattern(L"xdigit", iswxdigit, test);
            c2 = check_flag(flags, WM_CASEFOLD) ? towupper(test) : test;
            check_pattern(L"upper", iswupper, c2);
            /* fallthrough means match like a normal character */
        }
        if (c == test) {
            ok = 1;
        }
    } while ((c = *pattern++) != ']');

    *newp = (const WCHAR *)pattern;
    return (ok == negate) ? RANGE_NOMATCH : RANGE_MATCH;
}

#ifdef __cplusplus
}
#endif
