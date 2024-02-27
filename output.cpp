// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "output.h"
#include "colors.h"
#include "wcwidth.h"
#include "ecma48.h"

#include <VersionHelpers.h>

static int s_escape_codes = -1;   // 0=no, 1=yes, -1=auto (when not redirected).
static bool s_utf8 = false;

void SetUtf8Output(bool utf8)
{
    s_utf8 = utf8;
}

bool SetUseEscapeCodes(const WCHAR* s)
{
    if (!s)
        return false;
    else if (!_wcsicmp(s, L"") || !_wcsicmp(s, L"always"))
        s_escape_codes = 1;
    else if (!_wcsicmp(s, L"never"))
        s_escape_codes = 0;
    else if (!_wcsicmp(s, L"auto"))
        s_escape_codes = -1;
    else
        return false;
    return true;
}

bool CanUseEscapeCodes(HANDLE hout)
{
    if (s_escape_codes > 0)
        return true;
    if (s_escape_codes == 0)
        return false;

    // See https://no-color.org/.
    const WCHAR* env = _wgetenv(L"NO_COLOR");
    if (env && *env)
    {
        s_escape_codes = 0;
        return false;
    }

    if (!IsWindows8Point1OrGreater())
        return false;

    static HANDLE s_h = 0;
    static bool s_console = false;
    if (s_h != hout)
    {
        DWORD dummy;
        s_console = !!GetConsoleMode(hout, &dummy);
        s_h = hout;
    }
    return s_console;
}

int ValidateColor(const WCHAR* p, const WCHAR* end)
{
    // NOTE:  The caller is responsible for stripping leading/trailing spaces.

    if (!p || !*p || p == end)          return false;   // nullptr or "" is "has no color specified".
    else if (p[0] == '0')
    {
        if (!p[1] || p + 1 == end)      return false;   // "0" is default.
        if (p[1] == '0')
        {
            if (!p[2] || p + 2 == end)  return false;   // "00" is default.
        }
    }

    enum { ST_NORMAL, ST_BYTES1, ST_BYTES2, ST_BYTES3, ST_XCOLOR };
    int state = ST_NORMAL;

    // Validate recognized color/style escape codes.
    for (unsigned num = 0; true; ++p)
    {
        if (p == end || *p == ';' || !*p)
        {
            if (state == ST_NORMAL)
            {
                switch (num)
                {
                case 0:     // reset or normal
                case 1:     // bold
                case 2:     // faint or dim
                case 3:     // italic
                case 4:     // underline
                // case 5:     // slow blink
                // case 6:     // rapid blink
                case 7:     // reverse
                // case 8:     // conceal (hide)
                case 9:     // strikethrough
                case 21:    // double underline
                case 22:    // not bold and not faint
                case 23:    // not italic
                case 24:    // not underline
                case 25:    // not blink
                case 27:    // not reverse
                // case 28:    // reveal (not conceal/hide)
                case 29:    // not strikethrough
                case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37: case 39:
                case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47: case 49:
                // case 51:    // framed
                // case 52:    // encircled
                case 53:    // overline
                // case 54:    // not framed and not encircled
                case 55:    // not overline
                case 59:    // default underline color
                // case 73:    // superscript
                // case 74:    // subscript
                // case 75:    // not superscript and not subscript
                case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
                case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
                    break;
                case 38:    // set foreground color
                case 48:    // set background color
                // case 58:    // set underline color
                    state = ST_XCOLOR;
                    break;
                default:
                    return -1; // Unsupported SGR code.
                }
            }
            else if (state == ST_XCOLOR)
            {
                if (num == 2)
                    state = ST_BYTES3;
                else if (num == 5)
                    state = ST_BYTES1;
                else
                    return -1; // Unsupported extended color mode.
            }
            else if (state >= ST_BYTES1 && state <= ST_BYTES3)
            {
                if (num >= 0 && num <= 255)
                    --state;
                else
                    return -1; // Unsupported extended color.
            }
            else
            {
                assert(false);
                return -1; // Internal error.
            }

            num = 0;
        }

        if (p == end || !*p)
            return true; // The SGR code has been successfully validated.

        if (*p >= '0' && *p <= '9')
        {
            num *= 10;
            num += *p - '0';
        }
        else if (*p != ';')
            return -1; // Unsupported or invalid SGR code.
    }
}

/*
 * Restore console mode and attributes on exit or ^C or ^Break.
 */

static HANDLE s_hConsoleMutex = CreateMutex(0, false, 0);

static void AcquireConsoleMutex()
{
    if (s_hConsoleMutex)
        WaitForSingleObject(s_hConsoleMutex, INFINITE);
}

static void ReleaseConsoleMutex()
{
    if (s_hConsoleMutex)
        ReleaseMutex(s_hConsoleMutex);
}

class AutoConsoleMutex
{
public:
    AutoConsoleMutex() { AcquireConsoleMutex(); }
    ~AutoConsoleMutex() { ReleaseConsoleMutex(); }
};

class CRestoreConsole
{
public:
    CRestoreConsole();
    ~CRestoreConsole();

    void                End() { m_graceful = true; Restore(); }

private:
    void                Restore();
    static BOOL WINAPI  BreakHandler(DWORD CtrlType);

private:
    HANDLE              m_hout = 0;
    HANDLE              m_herr = 0;
    DWORD               m_mode_out;
    DWORD               m_mode_err;
    bool                m_graceful = false;
};

static CRestoreConsole s_restoreConsole;

CRestoreConsole::CRestoreConsole()
{
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE herr = GetStdHandle(STD_ERROR_HANDLE);

    if (hout && !GetConsoleMode(hout, &m_mode_out))
        hout = 0;
    if (herr && !GetConsoleMode(herr, &m_mode_err))
        herr = 0;

    if (!hout && !herr)
        return;

    m_hout = hout;
    m_herr = herr;

    SetConsoleCtrlHandler(BreakHandler, true);

    if (hout)
        SetConsoleMode(hout, m_mode_out|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    if (herr)
        SetConsoleMode(herr, m_mode_err|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

CRestoreConsole::~CRestoreConsole()
{
    Restore();
}

void CRestoreConsole::Restore()
{
    if (m_hout || m_herr)
    {
        if (!m_graceful && s_escape_codes)
        {
            DWORD written;
            if (m_hout)
                WriteConsoleW(m_hout, L"\x1b[m", 3, &written, nullptr);
            if (m_herr)
                WriteConsoleW(m_herr, L"\x1b[m", 3, &written, nullptr);
        }
    }

    if (m_hout)
        SetConsoleMode(m_hout, m_mode_out);
    if (m_herr)
        SetConsoleMode(m_herr, m_mode_err);

    m_hout = 0;
    m_herr = 0;
}

BOOL CRestoreConsole::BreakHandler(DWORD CtrlType)
{
    if (CtrlType == CTRL_C_EVENT || CTRL_C_EVENT == CTRL_BREAK_EVENT)
    {
        AcquireConsoleMutex();
        s_restoreConsole.Restore();
        ExitProcess(-1);
        return true;
    }
    return false;
}

/*
 * Pagination.
 */

static BOOL WINAPI RestoreConsole(DWORD dwCtrlType);

static bool s_paginate = false;
static HANDLE s_hResetConsoleMode = INVALID_HANDLE_VALUE;
static DWORD s_dwResetConsoleMode;

const WORD c_cxTab = 8;

class AutoRestoreConsole
{
public:
    AutoRestoreConsole() {}
    ~AutoRestoreConsole()
    {
        if (s_hResetConsoleMode != INVALID_HANDLE_VALUE)
        {
            SetConsoleCtrlHandler(RestoreConsole, false);
            RestoreConsole(0);
        }
    }
};

static AutoRestoreConsole s_autorestore;

static BOOL WINAPI RestoreConsole(DWORD dwCtrlType)
{
    if (s_hResetConsoleMode != INVALID_HANDLE_VALUE)
        SetConsoleMode(s_hResetConsoleMode, s_dwResetConsoleMode);
    return false;
}

bool SetPagination(bool paginate)
{
    if (paginate)
    {
        HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
        if (!GetConsoleMode(hin, &s_dwResetConsoleMode) ||
            !SetConsoleCtrlHandler(RestoreConsole, true) ||
            !SetConsoleMode(hin, ENABLE_PROCESSED_INPUT|ENABLE_ECHO_INPUT|ENABLE_LINE_INPUT))
            return false;
        s_hResetConsoleMode = hin;
    }
    s_paginate = paginate;
    return true;
}

static unsigned CLinesFromString(const WCHAR* p, unsigned len, unsigned max_width)
{
    unsigned cx = 0;
    unsigned cLines = 1;

    ecma48_state state;
    ecma48_iter iter(p, state, len);
    while (const ecma48_code& code = iter.next())
    {
        if (code.get_type() != ecma48_code::type_chars)
            continue;

        len = code.get_length();
        for (p = code.get_pointer(); *p && len; ++p, len--)
        {
            switch (*p)
            {
            case '\b':
            case '\r':
            case '\n':
                continue;
            case '\t':
                cx += c_cxTab - (cx % c_cxTab);
                if (cx >= max_width)
                {
                    cx = 0;
                    cLines++;
                }
                break;
            default:
                const char32_t ch = __decode(p, &len);
                int w = __wcwidth(ch);
                cx += (w < 0) ? 1 : w;
                if (cx >= max_width)
                {
                    cx = (cx > max_width) ? w : 0;
                    cLines++;
                }
                break;
            }
        }
    }

    return cLines;
}

static void ErasePrompt(HANDLE hout, CONSOLE_SCREEN_BUFFER_INFO* pcsbi, WCHAR* p)
{
    DWORD dw;

    if (!*p)
        return;

    for (WCHAR* erase = p; *erase; erase++)
        *erase = ' ';

    SetConsoleCursorPosition(hout, pcsbi->dwCursorPosition);
    WriteConsole(hout, p, DWORD(wcslen(p)), &dw, 0);
    SetConsoleCursorPosition(hout, pcsbi->dwCursorPosition);
}

PaginationAction PageBreak(HANDLE hin, HANDLE hout)
{
    WCHAR prompt[120];
    WCHAR details[120];
    INPUT_RECORD record;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    PaginationAction action = pageFull;
    DWORD dw;

    *prompt = 0;
    *details = 0;

    GetConsoleScreenBufferInfo(hout, &csbi);
    csbi.dwCursorPosition.X = 0;

    lstrcpy(details, L"press SPACE to continue [? for help]");

LPrompt:
    ErasePrompt(hout, &csbi, prompt);

    wsprintf(prompt, L"-- %s -- ", details);
    *details = 0;

    SetConsoleCursorPosition(hout, csbi.dwCursorPosition);
    if (!WriteConsoleW(hout, prompt, DWORD(wcslen(prompt)), &dw, nullptr))
    {
        action = pageAbort;
        goto exit;
    }

    SetConsoleMode(hin, ENABLE_PROCESSED_INPUT);
    while (true)
    {
        if (!ReadConsoleInput(hin, &record, 1, &dw) || !dw)
        {
            action = pageAbort;
            goto exit;
        }

        if (record.EventType != KEY_EVENT)
            continue;
        if (!record.Event.KeyEvent.bKeyDown)
            continue;

        switch (record.Event.KeyEvent.wVirtualKeyCode)
        {
        case VK_RETURN:
            action = pageOneLine;
            break;
        case 'P':
        case 'H':
        case 'D':
            action = pageHalf;
            break;
        case VK_SPACE:
            action = pageFull;
            break;
        case VK_ESCAPE:
        case 'Q':
            action = pageAbort;
            break;
        default:
            switch (record.Event.KeyEvent.uChar.AsciiChar)
            {
            case '?':
                lstrcpy(details, L"SPACE=next page, ENTER=next line, P=half page, Q=quit");
                goto LPrompt;
            }
            continue;
        }

        break;
    }

exit:
    ErasePrompt(hout, &csbi, prompt);
    SetConsoleMode(hin, 0);
    return action;
}

/*
 * OutputConsole.
 */

static unsigned s_console_width = 0;

void SetConsoleWidth(unsigned long width)
{
    if ((signed long)width < 0)
        width = 0;
    else if (width > 1024)
        width = 1024;
    s_console_width = width;
}

DWORD GetConsoleColsRows(HANDLE hout)
{
    assert(hout);

    static BOOL s_initialized = false;
    static HANDLE s_hout = INVALID_HANDLE_VALUE;
    static BOOL s_is_console;
    static WORD s_num_cols = 80;
    static WORD s_num_rows = 25;

    if (hout != s_hout)
    {
        s_initialized = false;
        s_hout = hout;
    }

    if (!s_initialized)
    {
        DWORD dw;

        s_is_console = !!GetConsoleMode(hout, &dw);

        if (!s_is_console)
        {
            s_num_cols = 0;
            s_num_rows = 0;
        }

        s_initialized = true;
    }

    if (s_is_console)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;

        if (GetConsoleScreenBufferInfo(hout, &csbi))
        {
            s_num_cols = (csbi.srWindow.Right - csbi.srWindow.Left) + 1;
            s_num_rows = (csbi.srWindow.Bottom - csbi.srWindow.Top) + 1;
        }
    }

    if (s_console_width)
        s_num_cols = s_console_width;

    return (s_num_rows << 16) | s_num_cols;
}

static bool WriteConsoleInternal(HANDLE h, const WCHAR* p, unsigned len, const WCHAR* color=nullptr)
{
    static HANDLE s_h = 0;
    static bool s_console = false;
    DWORD written;

    if (s_h != h)
    {
        DWORD dummy;
        s_console = !!GetConsoleMode(h, &dummy);
        s_h = h;
    }

    AutoConsoleMutex acm;

    if (color)
    {
        if (!CanUseEscapeCodes(h) || ValidateColor(color) <= 0)
            color = nullptr;
    }

    if (color)
    {
        StrW tmp;
        tmp.Printf(L"\x1b[0;%sm", color);
        if (s_console)
        {
            if (!WriteConsoleW(h, tmp.Text(), tmp.Length(), &written, nullptr))
                return false;
        }
        else
        {
            StrA tmp2;
            tmp2.SetW(tmp);
            if (!WriteFile(h, tmp2.Text(), tmp2.Length(), &written, nullptr))
                return false;
        }
    }

    StrA tmp;
    while (len)
    {
        if (p[0] == '\n')
        {
            if (!WriteFile(h, "\r\n", 2, &written, nullptr))
                return false;
            --len;
            ++p;
        }

        unsigned run = 0;
        while (run < len && p[run] != '\n')
            ++run;

        if (run)
        {
            if (s_console)
            {
                if (!WriteConsoleW(h, p, run, &written, nullptr))
                    return false;
            }
            else
            {
                if (s_utf8)
                {
                    const size_t needed = WideCharToMultiByte(CP_UTF8, 0, p, int(run), 0, 0, 0, 0);
                    char* out = tmp.Reserve(needed + 1);
                    int used = WideCharToMultiByte(CP_UTF8, 0, p, int(run), out, int(needed), 0, 0);

                    assert(unsigned(used) < tmp.Capacity());
                    out[used] = '\0';
                    tmp.ResyncLength();
                }
                else
                {
                    tmp.SetW(p, run);
                }
                if (!WriteFile(h, tmp.Text(), tmp.Length(), &written, nullptr))
                    return false;
            }
        }

        len -= run;
        p += run;
    }

    if (color)
    {
        if (s_console)
        {
            if (!WriteConsoleW(h, L"\x1b[m", 3, &written, nullptr))
                return false;
        }
        else
        {
            if (!WriteFile(h, "\x1b[m", 3, &written, nullptr))
                return false;
        }
    }

    ReleaseConsoleMutex();
    assert(!len);
    return true;
}

void OutputConsole(HANDLE h, const WCHAR* p, unsigned len, const WCHAR* color)
{
    if (len == unsigned(-1))
        len = unsigned(wcslen(p));
    if (!len)
        return;

    if (!s_paginate)
    {
LDirect:
        if (!WriteConsoleInternal(h, p, len, color))
            exit(1);
        return;
    }

    static unsigned cLines = 0;

    while (len)
    {
        const WCHAR* walk = p;
        const DWORD dwColsRows = GetConsoleColsRows(h);
        const unsigned cxCols = LOWORD(dwColsRows);
        unsigned cyRows = HIWORD(dwColsRows);

        // If the console window is less than 2 rows high, don't paginate.
        // Otherwise paginate such that the last row of the previous
        // output is still visible at the top of the window.

        if (cyRows < 2)
            goto LDirect;
        cyRows--;

        // Parse one logical line of text and calculate the number of
        // physical lines for the logical line.

        while (size_t(walk - p) < len && *walk != '\n')
            walk++;
        bool fNewline = false;
        if (size_t(walk - p) < len && *walk == '\n')
        {
            fNewline = true;
            walk++;
        }
        const unsigned run = unsigned(walk - p);
        const unsigned cPhysical = CLinesFromString(p, run, cxCols);

        assert(run <= len);

        // Paginate.

        if (cLines + cPhysical >= cyRows)
        {
            const PaginationAction action = PageBreak(GetStdHandle(STD_INPUT_HANDLE), h);

            switch (action)
            {
            default:
                assert(false);
                // Fall through...
            case pageFull:
                cLines = 0;
                break;
            case pageHalf:
                cLines = cyRows / 2;
                break;
            case pageOneLine:
                cLines = cyRows;
                break;
            case pageAbort:
                exit(1);
                break;
            }
        }

        cLines += cPhysical - !fNewline;

        // Output the logical line.

        if (!WriteConsoleInternal(h, p, run, color))
            exit(1);

        p = walk;
        len -= run;
    }
}

