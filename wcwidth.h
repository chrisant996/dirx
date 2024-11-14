// Copyright (c) 2023-2024 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

//------------------------------------------------------------------------------
typedef int32 wcwidth_t (char32_t);
extern wcwidth_t *wcwidth;

bool detect_ucs2_limitation(bool force=false);
void reset_wcwidths();
bool is_east_asian_ambiguous(char32_t ucs);
bool is_ideograph(char32_t ucs);
bool is_kana(char32_t ucs);
int32 is_CJK_codepage(UINT cp);
const char* is_assigned(char32_t ucs);
bool is_combining(char32_t ucs);

bool is_variant_selector(char32_t ucs);
bool is_possible_unqualified_half_width(char32_t ucs);
bool is_emoji(char32_t ucs);

extern bool g_color_emoji;

//------------------------------------------------------------------------------
class combining_mark_width_scope
{
public:
    combining_mark_width_scope(int32 width);
    ~combining_mark_width_scope();
private:
    const int32 m_old;
};
