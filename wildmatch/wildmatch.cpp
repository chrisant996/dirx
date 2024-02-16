/* begin_dirx_change */
#include "pch.h"
/* end_dirx_change */
#include "wildmatch.hpp"
#include "wildmatch.c" // avoids a link-time dependency

namespace wild {

bool match(const WCHAR *pattern, const WCHAR *string, int flags)
{
    return wildmatch(pattern, string, flags) == WM_MATCH;
}

bool match(const std::wstring& pattern, const std::wstring& string, int flags)
{
    return wildmatch(pattern.c_str(), string.c_str(), flags) == WM_MATCH;
}

}
