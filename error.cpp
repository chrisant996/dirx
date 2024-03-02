// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "error.h"
#include "output.h"

void ErrorStr::Set(const WCHAR* message)
{
    assert(!m_s.Length());
    m_s.Set(message);
    m_s.TrimRight();
}

void ErrorStr::ReplaceArg(WCHAR chArg, const WCHAR* p)
{
    assert(chArg >= '1' && chArg <= '9');
    assert(p);
    assert(m_s.Length());

    if (m_s.Length())
    {
        StrW s;
        s.Reserve(m_s.Length() + std::max<size_t>(10, m_s.Length() / 10));

        for (const WCHAR* from = m_s.Text(); *from;)
        {
            const WCHAR* pct = wcschr(from, '%');
            if (!pct)
            {
                s.Append(from);
                break;
            }

            s.Append(from, pct - from);
            from = pct;

            assert(*from == '%');
            ++from;

            if (*from == chArg)
            {
                s.Append(p);
            }
            else
            {
                s.Append('%');
                s.Append(*from);
            }

            ++from;
        }

        m_s = std::move(s);
    }
}

void ErrorArgs::ClearStrings()
{
    while (m_head)
    {
        ErrorStr* const p = m_head;
        m_head = p->Next();
        p->LinkNext(nullptr);
        delete p;
    }
}

void ErrorArgs::AddString(ErrorStr* p)
{
    p->LinkNext(m_head);
    m_head = p;
    m_chArg = '1';
}

ErrorArgs& ErrorArgs::operator<<(int x)
{
    WCHAR sz[32];
    _itow_s(x, sz, _countof(sz), 10);
    return *this << sz;
}

ErrorArgs& ErrorArgs::operator<<(unsigned x)
{
    WCHAR sz[32];
    _ui64tow_s(x, sz, _countof(sz), 10);
    return *this << sz;
}

ErrorArgs& ErrorArgs::operator<<(DWORD x)
{
    WCHAR sz[32];
    _ui64tow_s(x, sz, _countof(sz), 10);
    return *this << sz;
}

ErrorArgs& ErrorArgs::operator<<(const WCHAR* text)
{
    assert(text);
    assert(m_head);

    ErrorStr* const p = m_head;

    p->ReplaceArg(m_chArg, text);
    ++m_chArg;

    return *this;
}

ErrorArgs& ErrorArgs::operator<<(WCHAR ch)
{
    assert(ch);
    assert(m_head);

    ErrorStr* const p = m_head;

    StrW s;
    s.SetW(&ch, 1);

    p->ReplaceArg(m_chArg, s.Text());
    ++m_chArg;

    return *this;
}

void Error::Clear()
{
    m_set = false;
    ClearStrings();
}

ErrorArgs& Error::Set(const WCHAR* message)
{
    assert(message);

    m_set = true;

    ErrorStr* p = new ErrorStr;
    if (p)
    {
        p->Set(message);
        AddString(p);
    }
    return *this;
}

ErrorArgs& Error::Set(HRESULT hrError, const WCHAR* message)
{
    assert(hrError != S_OK);
    assert(hrError != E_ABORT);
    assert(FAILED(hrError)); // Use Sys- methods instead for Win32 errors.

    if (!message)
        return SysMessage(hrError);
    UpdateCode(hrError);
    return Set(message);
}

void Error::Sys()
{
    Sys(GetLastError());
}

ErrorArgs& Error::Sys(const WCHAR* message)
{
    return Sys(GetLastError(), message);
}

ErrorArgs& Error::Sys(DWORD dwError, const WCHAR* message)
{
    assert(dwError != ERROR_SUCCESS);
    assert(!FAILED(dwError));

    if (!message)
        return SysMessage(dwError);
    UpdateCode(dwError);
    return Set(message);
}

void Error::Format(StrW& s) const
{
    assert(Test());
    assert(m_head);

    s.Clear();

    for (ErrorStr* p = m_head; p; p = p->Next())
    {
        // Strip \r characters.
        for (const WCHAR* walk = p->Text(); *walk;)
        {
            const WCHAR* r = wcschr(walk, '\r');
            if (!r)
                r = walk + wcslen(walk);

            const unsigned len = unsigned(r - walk);
            s.Append(walk, len);

            walk = r;
            if (*walk)
            {
                assert(*walk == '\r');
                ++walk;
            }
        }

        // Ensure a final \n.
        if (!s.Length() || s.Text()[s.Length() - 1] != '\n')
            s.Append('\n');
    }
}

bool Error::Report()
{
    if (!Test())
        return false;

    StrW s;
    Format(s);

    HANDLE herr = GetStdHandle(STD_ERROR_HANDLE);
    if (IsConsole(herr))
    {
        while (s.Length() && s.Text()[s.Length() - 1] == ' ')
            s.SetLength(s.Length() - 1);
        OutputConsole(herr, s.Text(), s.Length(), L"0;91");
    }
    else
    {
        fputws(s.Text(), stderr);
    }

    Clear();
    return true;
}

ErrorArgs& Error::SysMessage(DWORD code, const WCHAR* context)
{
    WCHAR sz[1024];
    const DWORD dwFlags = FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageW(dwFlags, 0, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), sz, _countof(sz), 0);

    StrW s;
    if (context)
    {
        s.Set(context);
        s.Append(L": ");
    }

    if (len)
        s.Append(sz);
    else if (code < 65536)
        s.Printf(L"Error %u.", code);
    else
        s.Printf(L"Error 0x%08X.", code);

    UpdateCode(code);
    Set(s.Text());

    // Forbid arg replacement in a system-generated error message.
    return static_cast<ErrorArgs&>(*reinterpret_cast<ErrorArgs*>(0));
}

void Error::UpdateCode(DWORD code)
{
    if (!m_code)
        m_code = code;
}

