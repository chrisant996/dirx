// Copyright (c) 2023-2024 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#include "ecma48.h"

//------------------------------------------------------------------------------
uint32 __wcswidth(const WCHAR* s, uint32 len=-1);

//------------------------------------------------------------------------------
class wcwidth_iter
{
public:
    explicit        wcwidth_iter(const WCHAR* s, int32 len=-1);
                    wcwidth_iter(const wcwidth_iter& i);
    char32_t        next();
    void            unnext();
    const WCHAR*    character_pointer() const { return m_chr_ptr; }
    uint32          character_length() const { return uint32(m_chr_end - m_chr_ptr); }
    int32           character_wcwidth_signed() const { return m_chr_wcwidth; }
    uint32          character_wcwidth_zeroctrl() const { return (m_chr_wcwidth < 0) ? 0 : m_chr_wcwidth; }
    uint32          character_wcwidth_onectrl() const { return (m_chr_wcwidth < 0) ? 1 : m_chr_wcwidth; }
    uint32          character_wcwidth_twoctrl() const { return (m_chr_wcwidth < 0) ? 2 : m_chr_wcwidth; }
    bool            character_is_emoji() const { return m_emoji; }
    const WCHAR*    get_pointer() const;
    void            reset_pointer(const WCHAR* s);
    bool            more() const;
    uint32          length() const;

private:
    void            consume_emoji_sequence();

private:
    str_iter        m_iter;
    char32_t        m_next;
    const WCHAR*    m_chr_ptr;
    const WCHAR*    m_chr_end;
    int32           m_chr_wcwidth = 0;
    bool            m_emoji = false;
};
