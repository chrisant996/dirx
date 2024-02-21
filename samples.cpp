// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "output.h"
#include "ecma48.h"

static const WCHAR c_norm[] = L"\x1b[m";
static const WCHAR c_bold[] = L"\x1b[1m";
static const WCHAR c_italic[] = L"\x1b[3m";
static const WCHAR c_underline[] = L"\x1b[4m";
static const WCHAR c_overline[] = L"\x1b[53m";
static const WCHAR c_emphasis[] = L"\x1b[36m";
static const WCHAR c_CSI[] = L"\\x1b[";
static const WCHAR c_param[] = L"\x1b[33m";

const unsigned c_width = 3 * 4 * 6 + 2;

static void OutputCenter(HANDLE h, const WCHAR* s, bool header=false)
{
    StrW tmp;
    const unsigned width = cell_count(s);
    tmp.AppendSpaces((c_width - width) / 2);
    OutputConsole(h, tmp.Text());
    OutputConsole(h, s, -1, header ? L"3" : nullptr);
    OutputConsole(h, L"\n");
}

static void AppendSample(StrW& s, unsigned n)
{
    s.Printf(L"\x1b[38;5;%um%3u ", n, n);
}

void PrintColorSamples()
{
    StrW tmp;
    StrW line;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

    line.Set(L"\x1b[0;38;5;236m");
    for (unsigned n = 64; n--;)
        line.Append(0x2500);
    line.Append(c_norm);

    OutputConsole(h, L"\n");
    OutputCenter(h, L"This is a chart of ANSI color escape codes for 8-bit colors.");
    OutputCenter(h, L"To make a code, start with \x1b[48;5;237;38;5;253m 38;5; \x1b[m and append the listed number.");

    OutputConsole(h, L"\n");
    OutputCenter(h, L"System colors:", true);
    tmp.Set(c_norm);
    for (unsigned n = 0; n <= 7; ++n)
        AppendSample(tmp, n);
    tmp.AppendSpaces(2);
    for (unsigned n = 8; n <=15; ++n)
        AppendSample(tmp, n);
    OutputCenter(h, tmp.Text());

    OutputConsole(h, L"\n");
    OutputCenter(h, L"Color cube:", true);
    for (unsigned z = 0; z <= 1; ++z)
    {
        const unsigned base = 16 + (z * 18);
        for (unsigned i = 0; i <= 5; ++i)
        {
            tmp.Clear();
            for (unsigned j = 0; j <= 2; ++j)
            {
                for (unsigned k = 0; k <= 5; ++k)
                    AppendSample(tmp, base + i*36 + j*6 + k);
                tmp.AppendSpaces(1);
            }
            tmp.Append(c_norm);
            tmp.Append(L"\n");
            OutputConsole(h, tmp.Text());
        }
    }

    OutputConsole(h, L"\n");
    OutputCenter(h, L"Grayscale ramp:", true);
    tmp.Set(c_norm);
    for (unsigned n = 232; n <= 243; ++n)
        AppendSample(tmp, n);
    OutputCenter(h, tmp.Text());
    tmp.Set(c_norm);
    for (unsigned n = 244; n <= 255; ++n)
        AppendSample(tmp, n);
    OutputCenter(h, tmp.Text());

    OutputConsole(h, L"\n");
    OutputCenter(h, line.Text());

    OutputConsole(h, L"\n");
    OutputCenter(h, L"This is a list of ANSI color escape codes for styles.");
    OutputCenter(h, L"Under each style, the first number applies the");
    OutputCenter(h, L"style and the second number clears the style.");

    OutputConsole(h, L"\n");
    OutputCenter(h, L"Styles:", true);

    static const WCHAR* c_styles[] =
    {
        L"1",   L"22",  L"Bold",
        L"3",   L"23",  L"Italic",
        L"7",   L"27",  L"Reverse",
        L"9",   L"29",  L"Strikethru",
        L"4",   L"24",  L"Underline",
        L"21",  L"24",  L"Double",
        L"53",  L"55",  L"Overline",
    };

    StrW text(c_norm);
    StrW on(c_norm);
    StrW off(c_norm);
    for (unsigned i = 0; i < _countof(c_styles); i += 3)
    {
        if (i)
        {
            text.Append(L"  ");
            on.Append(L"  ");
            off.Append(L"  ");
        }
        text.Printf(L"\x1b[0;%sm%s%s", c_styles[i], c_styles[i + 2], c_norm);
        tmp.Clear();
        on.Printf(L"%*s", wcslen(c_styles[i + 2]), c_styles[i]);
        off.Printf(L"%*s", wcslen(c_styles[i + 2]), c_styles[i + 1]);
    }
    OutputCenter(h, text.Text());
    OutputCenter(h, on.Text());
    OutputCenter(h, off.Text());

    OutputConsole(h, L"\n");
}
