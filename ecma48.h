// Copyright (c) 2016 Martin Ridgers
// Portions Copyright (c) 2021-2024 Christopher Antos
// License: http://opensource.org/licenses/MIT

// Copied from https://github.com/chrisant996/clink and modified.

#pragma once

#include <windows.h>
#include "str.h"

#include <assert.h>

//------------------------------------------------------------------------------
typedef unsigned __int8 uint8;
typedef __int32 int32;
typedef unsigned __int32 uint32;

//------------------------------------------------------------------------------
enum class ecma48_processor_flags { none = 0, plaintext = 1<<1, colorless = 1<<2, lineless = 1<<3 };
DEFINE_ENUM_FLAG_OPERATORS(ecma48_processor_flags);
void ecma48_processor(const WCHAR* in, StrW* out, uint32* cell_count, ecma48_processor_flags flags=ecma48_processor_flags::none);
uint32 cell_count(const WCHAR*);

//------------------------------------------------------------------------------
enum ecma48_state_enum
{
    ecma48_state_unknown = 0,
    ecma48_state_char,
    ecma48_state_esc,
    ecma48_state_esc_st,
    ecma48_state_csi_p,
    ecma48_state_csi_f,
    ecma48_state_cmd_str,
    ecma48_state_char_str,
};

//------------------------------------------------------------------------------
class ecma48_code
{
public:
    enum type : uint8
    {
        type_none,
        type_chars,
        type_c0,
        type_c1,
        type_icf
    };

    enum : WCHAR
    {
        c0_nul, c0_soh, c0_stx, c0_etx, c0_eot, c0_enq, c0_ack, c0_bel,
        c0_bs,  c0_ht,  c0_lf,  c0_vt,  c0_ff,  c0_cr,  c0_so,  c0_si,
        c0_dle, c0_dc1, c0_dc2, c0_dc3, c0_dc4, c0_nak, c0_syn, c0_etb,
        c0_can, c0_em,  c0_sub, c0_esc, c0_fs,  c0_gs,  c0_rs,  c0_us,
    };

    enum : WCHAR
    {
        c1_apc          = 0x5f,     // '_'
        c1_csi          = 0x5b,     // '['
        c1_dcs          = 0x50,     // 'P'
        c1_osc          = 0x5d,     // ']'
        c1_pm           = 0x5e,     // '^'
        c1_sos          = 0x58,     // 'X'
        icf_vb          = 0x67,     // 'g'
    };

    struct csi_base
    {
        char                final;
        char                intermediate;
        bool                private_use;
        uint8               param_count;
        int32               params[1];
        int32               get_param(int32 index, int32 fallback=0) const;
    };

    template <int32 PARAM_N>
    struct csi : public csi_base
    {
        static const int32  max_param_count = PARAM_N;

    private:
        int32               buffer[PARAM_N - 1];
    };

    struct osc
    {
        WCHAR               command;
        WCHAR               subcommand;
        bool                visible;
        StrW                param;
        StrW                output;
    };

    explicit                operator bool () const { return !!get_length(); }
    const WCHAR*            get_pointer() const    { return m_str; }
    unsigned                get_length() const     { return m_length; }
    type                    get_type() const       { return m_type; }
    unsigned                get_code() const       { return m_code; }
    template <int S> bool   decode_csi(csi<S>& out) const;
    bool                    decode_osc(osc& out) const;
    bool                    get_c1_str(StrW& out) const;

private:
    friend class            ecma48_iter;
    friend class            ecma48_state;
                            ecma48_code() = default;
                            ecma48_code(ecma48_code&) = delete;
                            ecma48_code(ecma48_code&&) = delete;
    void                    operator = (ecma48_code&) = delete;
    bool                    decode_csi(csi_base& base, int32* params, uint32 max_params) const;
    const WCHAR*            m_str;
    unsigned short          m_length;
    type                    m_type;
    uint8                   m_code;
};

//------------------------------------------------------------------------------
inline int32 ecma48_code::csi_base::get_param(int32 index, int32 fallback) const
{
    if (unsigned(index) < unsigned(param_count))
        return *(params + index);

    return fallback;
}

//------------------------------------------------------------------------------
template <int32 S>
bool ecma48_code::decode_csi(csi<S>& csi) const
{
    return decode_csi(csi, csi.params, S);
}



//------------------------------------------------------------------------------
class ecma48_state
{
public:
                        ecma48_state()  { reset(); }
    void                reset();

private:
    friend class        ecma48_iter;
    ecma48_code         code;
    ecma48_state_enum   state;
    StrW                buffer;
    bool                clear_buffer;
};



//------------------------------------------------------------------------------
class str_iter
{
public:
    explicit        str_iter(const WCHAR* s=(const WCHAR*)L"", int32 len=-1);
    explicit        str_iter(const StrW& s, int32 len=-1);
                    str_iter(const str_iter& i);
    const WCHAR*    get_pointer() const;
    const WCHAR*    get_next_pointer();
    void            reset_pointer(const WCHAR* ptr);
    void            truncate(unsigned len);
    int32           peek();
    int32           next();
    bool            more() const;
    uint32          length() const;

private:
    const WCHAR*    m_ptr;
    const WCHAR*    m_end;
};



//------------------------------------------------------------------------------
class ecma48_iter
{
public:
                        ecma48_iter(const WCHAR* s, ecma48_state& state, int32 len=-1);
    const ecma48_code&  next();
    const WCHAR*        get_pointer() const { return m_iter.get_pointer(); }

private:
    bool                next_c1();
    bool                next_char(int32 c);
    bool                next_char_str(int32 c);
    bool                next_cmd_str(int32 c);
    bool                next_csi_f(int32 c);
    bool                next_csi_p(int32 c);
    bool                next_esc(int32 c);
    bool                next_esc_st(int32 c);
    bool                next_unknown(int32 c);
    str_iter            m_iter;
    ecma48_code&        m_code;
    ecma48_state&       m_state;
    int32               m_nested_cmd_str;
};
