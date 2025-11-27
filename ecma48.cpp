// Copyright (c) 2016 Martin Ridgers
// Portions Copyright (c) 2021-2024 Christopher Antos
// License: http://opensource.org/licenses/MIT

// Copied from https://github.com/chrisant996/clink and modified.

// https://ecma-international.org/publications-and-standards/standards/ecma-48/

#include "pch.h"
#include "ecma48.h"
#include "wcwidth.h"
#include "wcwidth_iter.h"

#include <assert.h>

//------------------------------------------------------------------------------
uint32 cell_count(const WCHAR* in)
{
    uint32 count = 0;

    ecma48_state state;
    ecma48_iter iter(in, state);
    while (const ecma48_code& code = iter.next())
    {
        if (code.get_type() != ecma48_code::type_chars)
            continue;

        count += __wcswidth(code.get_pointer(), code.get_length());
    }

    return count;
}

//------------------------------------------------------------------------------
static bool in_range(int32 value, int32 left, int32 right)
{
    return (unsigned(right - value) <= unsigned(right - left));
}

//------------------------------------------------------------------------------
static void strip_code_terminator(const WCHAR*& ptr, int& len)
{
    if (len <= 0)
        return;

    if (ptr[len - 1] == 0x07)
        len--;
    else if (len > 1 && ptr[len - 1] == 0x5c && ptr[len - 2] == 0x1b)
        len -= 2;
}

//------------------------------------------------------------------------------
static void strip_code_quotes(const WCHAR*& ptr, int& len)
{
    if (len < 2)
        return;

    if (ptr[0] == '"' && ptr[len - 1] == '"')
    {
        ptr++;
        len -= 2;
    }
}



//------------------------------------------------------------------------------
void ecma48_state::reset()
{
    state = ecma48_state_unknown;
    clear_buffer = true;
}



//------------------------------------------------------------------------------
bool ecma48_code::decode_csi(csi_base& base, int* params, unsigned max_params) const
{
    if (get_type() != type_c1 || get_code() != c1_csi)
        return false;

    /* CSI P ... P I .... I F */
    str_iter iter(get_pointer(), get_length());

    // Skip CSI
    if (iter.next() == 0x1b)
        iter.next();

    // Is the parameter string tagged as private/experimental?
    if (base.private_use = in_range(iter.peek(), 0x3c, 0x3f))
        iter.next();

    // Extract parameters.
    base.intermediate = 0;
    base.final = 0;
    int param = 0;
    unsigned count = 0;
    bool trailing_param = false;
    while (WCHAR c = iter.next())
    {
        if (in_range(c, 0x30, 0x3b))
        {
            trailing_param = true;

            if (c == 0x3b)
            {
                if (count < max_params)
                    params[count++] = param;

                param = 0;
            }
            else if (c != 0x3a) // Blissfully gloss over ':' part of spec.
                param = (param * 10) + (c - 0x30);
        }
        else if (in_range(c, 0x20, 0x2f))
            base.intermediate = char(c);
        else if (!in_range(c, 0x3c, 0x3f))
            base.final = char(c);
    }

    if (trailing_param)
        if (count < max_params)
            params[count++] = param;

    base.param_count = char(count);
    return true;
}

//------------------------------------------------------------------------------
bool ecma48_code::decode_osc(osc& out) const
{
    if (get_type() != type_c1 || get_code() != c1_osc)
        return false;

    str_iter iter(get_pointer(), get_length());

    // Skip CSI OSC
    if (iter.next() == 0x1b)
        iter.next();

    // Extract command.
    out.visible = false;
    out.command = iter.next(); // BUGBUG: UTF32 gets truncated to char.
    out.subcommand = 0;
    switch (out.command)
    {
    case '0':
    case '1':
    case '2':
        if (iter.next() == ';')
        {
            // Strip the terminator and optional quotes.
            const WCHAR* ptr = iter.get_pointer();
            int32 len = iter.length();
            strip_code_terminator(ptr, len);
            strip_code_quotes(ptr, len);

            ecma48_state state;
            ecma48_iter inner_iter(ptr, state, len);
            while (const ecma48_code& code = inner_iter.next())
            {
                if (code.get_type() == ecma48_code::type_c1 &&
                    code.get_code() == ecma48_code::c1_osc)
                {
                    // For OSC codes, only include output text.
                    ecma48_code::osc osc;
                    if (code.decode_osc(osc))
                        out.param.Append(osc.output.Text(), osc.output.Length());
                }
                else
                {
                    // Otherwise include the text verbatim.
                    out.param.Append(code.get_pointer(), code.get_length());
                }
            }
            return true;
        }
        break;

    case '9':
        out.subcommand = (iter.next() == ';') ? iter.next() : 0; // BUGBUG: UTF32 gets truncated to char.
        switch (out.subcommand)
        {
        case '8': /* get envvar */
            out.visible = true;
            out.output.Clear();
            if (iter.next() == ';')
            {
                const WCHAR* ptr = iter.get_pointer();
                while (true)
                {
                    const WCHAR* end = iter.get_pointer();
                    int32 c = iter.next();
                    if (!c || c == 0x07 || (c == 0x1b && iter.peek() == 0x5c))
                    {
                        int32 len = int32(end - ptr);
                        strip_code_quotes(ptr, len);

                        StrW name;
                        StrW value;

                        name.Set(ptr, len);
                        DWORD needed = GetEnvironmentVariableW(name.Text(), 0, 0);
                        value.Reserve(needed);
                        needed = GetEnvironmentVariableW(name.Text(), value.Reserve(), value.Capacity());
                        value.ResyncLength();
                        break;
                    }
                }
                return true;
            }
            break;
        }
        break;
    }

    return false;
}

//------------------------------------------------------------------------------
bool ecma48_code::get_c1_str(StrW& out) const
{
    if (get_type() != type_c1 || get_code() == c1_csi)
        return false;

    str_iter iter(get_pointer(), get_length());

    // Skip announce
    if (iter.next() == 0x1b)
        iter.next();

    const WCHAR* start = iter.get_pointer();

    // Skip until terminator
    while (int32 c = iter.peek())
    {
        if (c == 0x9c || c == 0x1b)
            break;

        iter.next();
    }

    out.Clear();
    out.Append(start, int32(iter.get_pointer() - start));
    return true;
}



//------------------------------------------------------------------------------
str_iter::str_iter(const WCHAR* s, int len)
: m_ptr(s)
, m_end(s + len)
{
}

//------------------------------------------------------------------------------
str_iter::str_iter(const StrW& s, int len)
: m_ptr(s.Text())
, m_end(s.Text() + len)
{
}

//------------------------------------------------------------------------------
str_iter::str_iter(const str_iter& i)
: m_ptr(i.m_ptr)
, m_end(i.m_end)
{
}

//------------------------------------------------------------------------------
const WCHAR* str_iter::get_pointer() const
{
    return m_ptr;
};

//------------------------------------------------------------------------------
const WCHAR* str_iter::get_next_pointer()
{
    const WCHAR* ptr = m_ptr;
    next();
    const WCHAR* ret = m_ptr;
    m_ptr = ptr;
    return ret;
};

//------------------------------------------------------------------------------
void str_iter::reset_pointer(const WCHAR* ptr)
{
    assert(ptr);
    assert(ptr <= m_ptr);
    m_ptr = ptr;
}

//------------------------------------------------------------------------------
void str_iter::truncate(unsigned len)
{
    assert(m_ptr);
    assert(len <= length());
    m_end = m_ptr + len;
}

//------------------------------------------------------------------------------
int32 str_iter::peek()
{
    const WCHAR* ptr = m_ptr;
    int32 ret = next();
    m_ptr = ptr;
    return ret;
}

//------------------------------------------------------------------------------
int32 str_iter::next()
{
    int32 c;
    int32 ax = 0;

    while (more() && (c = *m_ptr++))
    {
        // Decode surrogate pairs.
        if ((c & 0xfc00) == 0xd800)
        {
            if (!more() || (*m_ptr & 0xfc00) != 0xdc00)         // Invalid.
                return 0xfffd;
            ax = c << 10;
            continue;
        }
        else if ((c & 0xfc00) == 0xdc00)
        {
            if (ax < (1 << 10))                                 // Invalid.
                return 0xfffd;
            c = ax + c - 0x35fdc00;
            ax = 0;
        }
        else
        {
            if (ax)                                             // Invalid.
               return 0xfffd;
        }
        return c;
    }

    if (ax)                                                     // Invalid.
        return 0xfffd;
    return 0;
}

//------------------------------------------------------------------------------
bool str_iter::more() const
{
    return (m_ptr != m_end && *m_ptr != '\0');
}

//------------------------------------------------------------------------------
uint32 str_iter::length() const
{
    return (uint32)((m_ptr <= m_end) ? m_end - m_ptr : wcslen(m_ptr));
}



//------------------------------------------------------------------------------
ecma48_iter::ecma48_iter(const WCHAR* s, ecma48_state& state, int32 len)
: m_iter(s, len)
, m_code(state.code)
, m_state(state)
, m_nested_cmd_str(0)
{
}

//------------------------------------------------------------------------------
const ecma48_code& ecma48_iter::next()
{
    m_code.m_str = m_iter.get_pointer();

    const WCHAR* copy = m_iter.get_pointer();
    bool done = true;
    while (1)
    {
        int32 c = m_iter.peek();
        if (!c)
        {
            if (m_state.state != ecma48_state_char)
            {
                m_code.m_length = 0;
                return m_code;
            }

            break;
        }

        assert(m_nested_cmd_str == 0 || m_state.state == ecma48_state_cmd_str);

        switch (m_state.state)
        {
        case ecma48_state_char:     done = next_char(c);     break;
        case ecma48_state_char_str: done = next_char_str(c); break;
        case ecma48_state_cmd_str:  done = next_cmd_str(c);  break;
        case ecma48_state_csi_f:    done = next_csi_f(c);    break;
        case ecma48_state_csi_p:    done = next_csi_p(c);    break;
        case ecma48_state_esc:      done = next_esc(c);      break;
        case ecma48_state_esc_st:   done = next_esc_st(c);   break;
        case ecma48_state_unknown:  done = next_unknown(c);  break;
        }

        if (m_state.state != ecma48_state_char)
        {
            if (m_state.clear_buffer)
            {
                m_state.clear_buffer = false;
                m_state.buffer.Clear();
            }
            const int32 len = int32(m_iter.get_pointer() - copy);
            m_state.buffer.Append(copy, len);
            copy += len;
        }

        if (done)
            break;
    }

    if (m_state.state != ecma48_state_char)
    {
        m_code.m_str = m_state.buffer.Text();
        m_code.m_length = m_state.buffer.Length();
    }
    else
        m_code.m_length = int32(m_iter.get_pointer() - m_code.get_pointer());

    m_state.reset();

    assert(m_nested_cmd_str == 0);
    m_nested_cmd_str = 0;

    return m_code;
}

//------------------------------------------------------------------------------
bool ecma48_iter::next_c1()
{
    // Convert c1 code to its 7-bit version.
    m_code.m_code = (m_code.m_code & 0x1f) | 0x40;

    switch (m_code.get_code())
    {
        case 0x50: /* dcs */
        case 0x5d: /* osc */
        case 0x5e: /* pm  */
        case 0x5f: /* apc */
            m_state.state = ecma48_state_cmd_str;
            return false;

        case 0x5b: /* csi */
            m_state.state = ecma48_state_csi_p;
            return false;

        case 0x58: /* sos */
            m_state.state = ecma48_state_char_str;
            return false;
    }

    return true;
}

//------------------------------------------------------------------------------
bool ecma48_iter::next_char(int32 c)
{
    if (in_range(c, 0x00, 0x1f))
    {
        m_code.m_type = ecma48_code::type_chars;
        return true;
    }

    m_iter.next();
    return false;
}

//------------------------------------------------------------------------------
bool ecma48_iter::next_char_str(int32 c)
{
    m_iter.next();

    if (c == 0x1b)
    {
        m_state.state = ecma48_state_esc_st;
        return false;
    }

    return (c == 0x9c);
}

//------------------------------------------------------------------------------
bool ecma48_iter::next_cmd_str(int32 c)
{
    if (c == 0x1b)
    {
        m_iter.next();
        int32 d = m_iter.peek();
        if (d == 0x5d)
            m_nested_cmd_str++;
        else if (d == 0x5c && m_nested_cmd_str > 0)
            m_nested_cmd_str--;
        else
            m_state.state = ecma48_state_esc_st;
        return false;
    }
    else if (c == 0x9c || c == 0x07) // Xterm supports OSC terminated by BEL.
    {
        m_iter.next();
        if (c == 0x07 && m_nested_cmd_str > 0)
        {
            m_nested_cmd_str--;
            return false;
        }
        return true;
    }
    else if (in_range(c, 0x08, 0x0d) || uint32(c) >= uint32(0x20))
    {
        m_iter.next();
        return false;
    }

    // Reset
    m_code.m_str = m_iter.get_pointer();
    m_code.m_length = 0;
    m_state.reset();
    return false;
}

//------------------------------------------------------------------------------
bool ecma48_iter::next_csi_f(int32 c)
{
    if (in_range(c, 0x20, 0x2f))
    {
        m_iter.next();
        return false;
    }
    else if (in_range(c, 0x40, 0x7e))
    {
        m_iter.next();
        return true;
    }

    // Reset
    m_code.m_str = m_iter.get_pointer();
    m_code.m_length = 0;
    m_state.reset();
    return false;
}

//------------------------------------------------------------------------------
bool ecma48_iter::next_csi_p(int32 c)
{
    if (in_range(c, 0x30, 0x3f))
    {
        m_iter.next();
        return false;
    }

    m_state.state = ecma48_state_csi_f;
    return next_csi_f(c);
}

//------------------------------------------------------------------------------
bool ecma48_iter::next_esc(int32 c)
{
    m_iter.next();

    if (in_range(c, 0x40, 0x5f))
    {
        m_code.m_type = ecma48_code::type_c1;
        m_code.m_code = c;
        return next_c1();
    }
    else if (in_range(c, 0x60, 0x7f))
    {
        m_code.m_type = ecma48_code::type_icf;
        m_code.m_code = c;
        return true;
    }

    m_state.state = ecma48_state_char;
    return false;
}

//------------------------------------------------------------------------------
bool ecma48_iter::next_esc_st(int32 c)
{
    if (c == 0x5c)
    {
        m_iter.next();
        assert(m_nested_cmd_str == 0);
        return true;
    }

    m_code.m_str = m_iter.get_pointer();
    m_code.m_length = 0;
    m_state.reset();
    m_nested_cmd_str = 0;
    return false;
}

//------------------------------------------------------------------------------
bool ecma48_iter::next_unknown(int32 c)
{
    m_iter.next();

    if (c == 0x1b)
    {
        m_state.state = ecma48_state_esc;
        return false;
    }
    else if (in_range(c, 0x00, 0x1f))
    {
        m_code.m_type = ecma48_code::type_c0;
        m_code.m_code = c;
        return true;
    }
    else if (in_range(c, 0x80, 0x9f))
    {
        m_code.m_type = ecma48_code::type_c1;
        m_code.m_code = c;
        return next_c1();
    }

    m_code.m_type = ecma48_code::type_chars;
    m_state.state = ecma48_state_char;
    return false;
}



//------------------------------------------------------------------------------
void ecma48_processor(const WCHAR* in, StrW* out, uint32* cell_count, ecma48_processor_flags flags)
{
    uint32 cells = 0;
    bool plaintext = !!int32(flags & ecma48_processor_flags::plaintext);
    bool colorless = !!int32(flags & ecma48_processor_flags::colorless);
    bool lineless = !!int32(flags & ecma48_processor_flags::lineless);

    ecma48_state state;
    ecma48_iter iter(in, state);
    while (const ecma48_code& code = iter.next())
    {
        bool c1 = (code.get_type() == ecma48_code::type_c1);
        if (c1)
        {
            if (code.get_code() == ecma48_code::c1_osc)
            {
                // For OSC codes, use visible output text if present.  Readline
                // expects escape codes to be invisible, but `ESC]9;8;"var"ST`
                // outputs the value of the named env var.
                ecma48_code::osc osc;
                if (!code.decode_osc(osc))
                    goto concat_verbatim;
                if (osc.visible)
                {
                    if (out)
                        out->Append(osc.output.Text(), osc.output.Length());
                    if (cell_count)
                        cells += __wcswidth(osc.output.Text(), osc.output.Length());
                }
                else
                    goto concat_verbatim;
            }
            else
            {
concat_verbatim:
                if (out)
                {
                    if (plaintext)
                    {
                    }
                    else if (colorless && code.get_code() == ecma48_code::c1_csi)
                    {
                        ecma48_code::csi<32> csi;
                        if (code.decode_csi(csi) && csi.final == 'm')
                        {
                            StrW tmp;
                            unsigned skip = 0;
                            for (int32 i = 0; i < csi.param_count; ++i)
                            {
                                if (skip)
                                {
                                    --skip;
                                    continue;
                                }
                                switch (csi.params[i])
                                {
                                case 0:
                                    tmp.Append(L";23;24;29");
                                    break;
                                case 3:
                                case 4:
                                case 9:
                                case 21:
                                case 23:
                                case 24:
                                case 29:
                                case 53:
                                case 55:
                                case 73:
                                case 74:
                                case 75:
                                    tmp.Printf(L";%u", csi.params[i]);
                                    break;
                                case 38:
                                case 48:
                                case 58:
                                    if (i + 1 < csi.param_count)
                                    {
                                        if (csi.params[i + 1] == 2)
                                            skip = 4;
                                        else if (csi.params[i + 1] == 5)
                                            skip = 2;
                                    }
                                    break;
                                }
                            }

                            if (!tmp.Empty())
                            {
                                out->Append(L"\x1b[");
                                out->Append(tmp.Text() + 1);   // Skip leading ";".
                                out->Append(L"m");
                            }
                        }
                    }
                    else if (lineless && code.get_code() == ecma48_code::c1_csi)
                    {
                        ecma48_code::csi<32> csi;
                        if (code.decode_csi(csi) && csi.final == 'm')
                        {
                            StrW tmp;
                            unsigned skip = 0;
                            for (int32 i = 0; i < csi.param_count; ++i)
                            {
                                if (skip)
                                    --skip;
                                else
                                {
                                    switch (csi.params[i])
                                    {
                                    case 4:     // Underline.
                                    case 9:     // Strikethrough.
                                    case 21:    // Double underline.
                                    case 53:    // Overline.
                                        continue;
                                    case 38:
                                    case 48:
                                    case 58:
                                        if (i + 1 < csi.param_count)
                                        {
                                            if (csi.params[i + 1] == 2)
                                                skip = 4;
                                            else if (csi.params[i + 1] == 5)
                                                skip = 2;
                                        }
                                        break;
                                    }
                                }
                                tmp.Printf(L";%u", csi.params[i]);
                            }

                            if (!tmp.Empty())
                            {
                                out->Append(L"\x1b[");
                                out->Append(tmp.Text() + 1);   // Skip leading ";".
                                out->Append(L"m");
                            }
                        }
                    }
                    else
                    {
                        out->Append(code.get_pointer(), code.get_length());
                    }
                }
            }
        }
        else
        {
            int32 index = 0;
            const WCHAR* seq = code.get_pointer();
            const WCHAR* end = seq + code.get_length();
            do
            {
                const WCHAR* walk = seq;
                while (walk < end && *walk != '\007')
                    walk++;

                if (walk > seq)
                {
                    uint32 seq_len = uint32(walk - seq);
                    if (out)
                        out->Append(seq, seq_len);
                    if (cell_count)
                        cells += __wcswidth(seq, seq_len);
                    seq = walk;
                }
                if (walk < end && *walk == '\007')
                {
                    if (out)
                        out->Append(L"\007");
                    seq++;
                }
            }
            while (seq < end);
        }
    }

    if (cell_count)
        *cell_count = cells;
}
