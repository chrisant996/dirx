// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// Parse command line arguments into argc and argv.  This assumes there is no
// program name in the command line; it's intended for parsing an environment
// variable that specifies default command line arguments.
//
// It's more complicated than just handling spaces and quotation marks:
//
// "What's up with the strange treatment of quotation marks and backslashes by
// CommandLineToArgvW"
// https://devblogs.microsoft.com/oldnewthing/20100917-00/?p=12833
//
// "Everyone quotes command line arguments the wrong way"
// https://docs.microsoft.com/en-us/archive/blogs/twistylittlepassagesallalike/everyone-quotes-command-line-arguments-the-wrong-way

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <vector>

class MakeArgv
{
public:
                        MakeArgv(const WCHAR* cmdline);
                        ~MakeArgv() { delete [] m_buffer; }

    int&                Argc() { return m_argc; }
    const WCHAR**&      Argv() { return m_argv; }

private:
    WCHAR*              m_buffer = nullptr;
    std::vector<const WCHAR*> m_array;
    int                 m_argc = 0;
    const WCHAR**       m_argv = nullptr;
};

inline MakeArgv::MakeArgv(const WCHAR* const cmdline)
{
    size_t buffer_size = 0;

    if (cmdline && *cmdline)
    {
        buffer_size = wcslen(cmdline) + 1;
        m_buffer = new WCHAR[buffer_size];
    }

    if (m_buffer)
    {
        // Loop to parse arguments.
        WCHAR* buffer = m_buffer;
        const WCHAR* p = cmdline;
        while (true)
        {
            // Skip whitespace between arguments.
            if (*p)
            {
                while (*p == ' ' || *p == '\t')
                    ++p;
            }

            // End of string ends the loop.
            if (!*p)
                break;

            // Remember the beginning of the copied argument in the buffer.
            m_array.push_back(buffer);

            // Parse one argument.
            bool quote = false;
            for (;;)
            {
                bool copy = true;

                // Even # of backslashes and " becomes #/2 backslashes.
                // Odd # of backslashes and " becomes #/2 backslashes and ".
                // Else treat as literal backslashes.
                unsigned slashes = 0;
                while (*p == '\\')
                {
                    ++p;
                    ++slashes;
                }
                if (p[0] == '\"')
                {
                    if (slashes % 2 == 0)
                    {
                        if (quote)
                        {
                            if (p[1] == '\"')
                            {
                                // Two quote chars inside quoted arg:  \\\\""
                                // Skip first quote char and copy second.
                                ++p;
                            }
                            else
                            {
                                // Skip quote char; just let quote mode end.
                                copy = false;
                            }
                        }
                        else
                        {
                            // Skip quote char; just let quote mode start.
                            copy = false;
                        }

                        // Invert quote mode.
                        quote = !quote;
                    }

                    // Backslashes followed by a quote means the backslashes
                    // are escapes, so copy only half of them.
                    slashes /= 2;
                }

                // Copy slashes, if any.
                while (slashes--)
                    *(buffer++) = '\\';

                // End of string ends the loop.
                if (!*p || (!quote && (*p == ' ' || *p == '\t')))
                    break;

                // Copy character into argument.
                if (copy)
                    *(buffer++) = *p;
                ++p;
            }

            // Null terminate the argument.
            *(buffer++) = '\0';
        }

        assert(buffer <= m_buffer + buffer_size);
    }

    m_argc = int(m_array.size());
    m_array.push_back(nullptr);
    m_argv = &*m_array.begin();
}
