// Copyright (c) 2023-2024 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

//------------------------------------------------------------------------------
typedef int32 wcwidth_t (char32_t);
extern wcwidth_t *wcwidth;

struct wcwidth_modes
{
    int32 color_emoji = 0;          // 0=no change, 1=force true, -1=force false
    int32 only_ucs2 = 0;            // 0=no change, 1=force true, -1=force false
};

// The first call to this automatically detects the modes.  But the modes can
// be overridden by passing in a wcwidth_modes struct.  Any fields that are 0
// have no effect, but any non-zero field forcibly override the corresponding
// mode (1 forces the mode true, and -1 forces the mode false).
void initialize_wcwidth(const wcwidth_modes* modes=nullptr);
bool get_color_emoji();
bool get_only_ucs2();

bool is_combining(char32_t ucs);
bool is_east_asian_ambiguous(char32_t ucs);
bool is_CJK_codepage(UINT cp);

bool is_variant_selector(char32_t ucs);
bool is_possible_unqualified_half_width(char32_t ucs);
bool is_emoji(char32_t ucs);

//------------------------------------------------------------------------------
class combining_mark_width_scope
{
public:
    combining_mark_width_scope(int32 width);
    ~combining_mark_width_scope();
private:
    const int32 m_old;
};
