// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "str.h"

class ErrorStr
{
public:
                        ErrorStr() = default;
                        ~ErrorStr() { assert(!m_next); }

    ErrorStr*           Next() const { return m_next; }
    const WCHAR*        Text() const { return m_s.Text(); }

    void                Set(const WCHAR* p);
    void                ReplaceArg(WCHAR chArg, const WCHAR* p);

    void                LinkNext(ErrorStr* p) { m_next = p; }

private:
    ErrorStr*           m_next = nullptr;
    StrW                m_s;
};

class ErrorArgs
{
public:
    ErrorArgs&          operator<<(int x);
    ErrorArgs&          operator<<(unsigned x);
    ErrorArgs&          operator<<(DWORD x);
    ErrorArgs&          operator<<(const WCHAR* text);
    ErrorArgs&          operator<<(WCHAR ch);

protected:
                        ErrorArgs() = default;
                        ~ErrorArgs() { ClearStrings(); }

    void                ClearStrings();
    void                AddString(ErrorStr* p);

    ErrorStr*           m_head = nullptr;
    WCHAR               m_chArg = 0;
};

class Error : protected ErrorArgs
{
public:
                        Error() = default;
                        ~Error() = default;

    void                Clear();

    bool                Test() const { return m_set; }
    DWORD               Code() const { return m_code; }

    ErrorArgs&          Set(const WCHAR* message);
    ErrorArgs&          Set(HRESULT hrError, const WCHAR* message=nullptr);

    void                Sys();
    ErrorArgs&          Sys(const WCHAR* message);
    ErrorArgs&          Sys(DWORD dwError, const WCHAR* message=nullptr);
    ErrorArgs&          SysMessage(DWORD code, const WCHAR* context=nullptr);

    void                Format(StrW& s) const;
    bool                Report();

private:
    void                UpdateCode(DWORD code);

private:
    bool                m_set = false;
    DWORD               m_code = 0;
};

