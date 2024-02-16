// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

template<class T, DWORD_PTR empty_value, class base>
class SH : public base
{
    typedef SH<T,empty_value,base> this_type;

public:
            SH(T h=T(empty_value))  { m_h = h; }
            SH(this_type&& other)   { m_h = other.m_h; other.m_h = T(empty_value); }
            ~SH()                   { if (T(empty_value) != m_h) base::Free(m_h); }

            operator T() const      { return m_h; }
    T       Get() const             { return m_h; }
    T       operator=(T h)          { assert(T(empty_value) == m_h); return m_h = h; }
    T       operator=(this_type&& other) { m_h = other.m_h; other.m_h = T(empty_value); return m_h; }
    void    Set(T h)                { if (T(empty_value) != m_h) base::Free(m_h); m_h = h; }
    void    Close()                 { Set(T(empty_value)); }
    bool    Empty() const           { return empty_value == reinterpret_cast<DWORD_PTR>(m_h); }

protected:
    T m_h;

private:
    SH(const this_type&) = delete;
    this_type& operator=(const this_type&) = delete;
};

class SH_CloseHandle { protected: void Free(HANDLE h) { CloseHandle(h); } };
class SH_FindClose { protected: void Free(HANDLE h) { FindClose(h); } };

typedef SH<HANDLE, NULL, SH_CloseHandle> SHBasic;
typedef SH<HANDLE, DWORD_PTR(INVALID_HANDLE_VALUE), SH_CloseHandle> SHFile;
typedef SH<HANDLE, DWORD_PTR(INVALID_HANDLE_VALUE), SH_FindClose> SHFind;

