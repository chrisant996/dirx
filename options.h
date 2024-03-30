// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// An evolution in C++ of the classic getopt by Gregory Pietsch.

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

enum HasArg { LOHA_NOARG, LOHA_REQUIRED, LOHA_OPTIONAL };

template<class T>
struct LongOption
{
    // arg is always filled in with non-null if the long option is used.  So,
    // it's possible to provide an array with only the name field filled in,
    // and then check whether arg is null for specific options.

    // However, it may be simpler in practice to fill in name, flag, and value
    // so that the long options array ends up directly setting other variables
    // (and thus the array itself doesn't need to be referenced again).

    const T*            name;       /* A long name, e.g. "foo" (for --foo). */
    int*                flag;       /* If flag is non-null, *flag receives
                                       value only if --name is used. */
    int                 value;      /* If flag is null and value is unique,
                                       value can be used to identify the long
                                       option returned by GetValue() without
                                       needing a string comparison. */
    HasArg              has_arg;
    const T**           arg;        /* If arg is non-null, *arg receives the
                                       argument if --name is used (or receives
                                       "" if there's no argument). */
};

enum OptionsFlags
{
    OPT_DEFAULT         = 0x0000,

    OPT_ONE             = 0x0001,   // Exactly one argument.
    OPT_TWO             = 0x0002,   // Exactly two arguments.
    OPT_THREE           = 0x0004,   // Exactly three arguments.
    OPT_MORE            = 0x0010,   // More than three arguments.
    OPT_NONE            = 0x0020,   // Require no arguments.

    OPT_ANYWHERE        = 0x0100,   // Flags can appear anywhere, even after other arguments.
    OPT_LONGABBR        = 0x0200,   // Long options can be abbreviated, if the abbreviation is unique.
    OPT_LONGANYCASE     = 0x0400,   // Long options are case insensitive.

    OPT_OPT             = OPT_NONE|OPT_ONE,
    OPT_ANY             = OPT_NONE|OPT_ONE|OPT_TWO|OPT_THREE|OPT_MORE,
    OPT_SOME            = OPT_ONE|OPT_TWO|OPT_THREE|OPT_MORE,
};

template<class T>
class OptionsTemplate
{
    enum OptionsTemplateError
    {
        OTE_INVALID_OPTION_C,
        OTE_INVALID_OPTION_AS,
        OTE_TOO_MANY_OPTIONS,
        OTE_MISSING_REQUIRED_ARGUMENT_C,
        OTE_MISSING_REQUIRED_ARGUMENT_S,
        OTE_ARGUMENT_NOT_ALLOWED_S,
        OTE_WRONG_NUMBER_OF_ARGUMENTS,
        OTE_AMBIGUOUS_OPTION_ASS,
        OTE_AMBIGUOUS_POSSIBILITY_S,
        OTE_USAGE_S,
        OTE_MAX
    };

public:
                        OptionsTemplate(unsigned max=10);
                        ~OptionsTemplate();

                        // Parse argc and argv.
    bool                Parse(int& argc, const T**& argv, const T* opts, const T* usage,
                              int flags=OPT_DEFAULT, LongOption<T>* long_opts=nullptr);
    const T*            ErrorString() const { assert(m_error); return m_error; }

                        // Get parsed flags and values.
    const T*            operator[](T chOpt) const { return GetValue(chOpt, 0); }
    const T*            GetValue(T chOpt, unsigned iSubOpt) const;
    bool                GetValue(unsigned iOpt, T& chOpt, const T*& value,
                                 const LongOption<T>** long_opt=nullptr) const;

                        // When OPT_ANYWHERE and opts includes '-' and '--' is
                        // present, this is its index in argv (else -1).
    int                 EndOfFlagsIndex() const { return m_eof_index; }

protected:
    void                ClearError() { delete [] m_error; m_error = nullptr; }
    void                SetError(OptionsTemplateError e, const T* usage, ...);

private:
    static const T*     GetArg(const T* p, int& argc, const T**& argv, const T**& move_to, const T**& walk, int& argc_to_process);

    static int          IsSpace(T ch);
    static const T*     PlusString();
    static const T*     MinusString();
    static bool         IsEqual(const T* name1, const T* name2, size_t len, bool caseless=false);
    static int          Sprintf(const T* buffer, size_t size, T* append, const T* format, ...);
    static int          SprintfV(const T* buffer, size_t size, T* append, const T* format, va_list args);

    static const T* const c_errors[];

private:
    const unsigned      m_max;
    unsigned            m_count = 0;
    T*                  m_flags = nullptr;
    const T**           m_values = nullptr;
    const LongOption<T>** m_long_opts = nullptr;
    int                 m_eof_index = -1;
    T*                  m_error = nullptr;
};

template<class T>
OptionsTemplate<T>::OptionsTemplate(unsigned max)
: m_max(max)
{
    m_flags = new T[max];
    m_values = new const T*[max];
}

template<class T>
OptionsTemplate<T>::~OptionsTemplate()
{
    delete [] m_flags;
    delete [] m_values;
    delete [] m_long_opts;
    delete [] m_error;
}

template<class T>
bool OptionsTemplate<T>::Parse(
    int& argc,
    const T**& argv,
    const T* opts,
    const T* usage,
    int flags,
    LongOption<T>* long_opts)
{
    assert(opts);
    assert(usage);

    ClearError();

    if (long_opts && !m_long_opts)
    {
        m_long_opts = new const LongOption<T>*[m_max];
        memset(m_long_opts, 0, m_max * sizeof(*m_long_opts));
    }

    // If the options string begins with a slash, then recognize both '/' and
    // '-' and flag characters.

    const bool slash = (*opts == '/');
    if (slash)
        ++opts;

    // Parse flags.

    bool nomore = false;
    unsigned parsed = 0;
    const T** walk = argv;
    const T** move_to = argv;

    for (int argc_to_process = argc; argc_to_process--;)
    {
        if (move_to != walk)
            *move_to = *walk;

        // If this is not a flag, then parsing is done unless OPT_ANYWHERE was
        // used.  Note that two slashes in a row do not count as a flag.
        if (nomore || (walk[0][0] != '-' && (!slash || walk[0][0] != '/' || walk[0][1] == '/')))
        {
            if (!(flags & OPT_ANYWHERE))
                break;

            ++move_to;
            ++walk;
            ++parsed;
            continue;
        }

        // '--' is special and means subsequent arguments should not be
        // treated as flags even if they begin with '-' (or '/').
        if (walk[0][0] == '-' && walk[0][1] == '-' && !walk[0][2])
        {
            --argc;
            if (argv == walk)
            {
                ++argv;
                ++move_to;
            }
            ++walk;
            if (flags & OPT_ANYWHERE)
            {
                nomore = true;
                m_eof_index = parsed;
                continue;
            }
            break;
        }

        const T* arg = walk[0];

        // Handle long options specially.
        if (arg[0] == '-' && long_opts && arg[1] == '-')
        {
            arg += 2;
            const T* const name = arg;
            const T* name_end = name;
            while (*name_end &&
                    *name_end != ' ' &&
                    *name_end != '=')
                ++name_end;
            const size_t name_len = name_end - name;

            // Look for a match.

            LongOption<T>* found = nullptr;
            LongOption<T>* abbrev = nullptr;
            bool ambiguous = false;
            const bool caseless = !!(flags & OPT_LONGANYCASE);

            for (LongOption<T>* p = long_opts; p->name; ++p)
            {
                if (IsEqual(p->name, name, name_len, caseless))
                {
                    if (abbrev)
                    {
                        ambiguous = true;
                        break;
                    }

                    abbrev = p;

                    size_t long_name_len = 0;
                    for (const T* long_name = p->name; *long_name; ++long_name)
                        long_name_len++;

                    if (name_len == long_name_len)
                    {
                        found = p;
                        break;
                    }
                }
            }

            if (flags & OPT_LONGABBR)
            {
                if (ambiguous)
                {
                    T possibilities[256];
                    T* append = possibilities;
                    for (LongOption<T>* p = long_opts; p->name; ++p)
                    {
                        if (IsEqual(p->name, name, name_len, caseless))
                        {
                            const size_t len = Sprintf(possibilities, _countof(possibilities),
                                                        append, c_errors[OTE_AMBIGUOUS_POSSIBILITY_S], p->name);
                            if (len >= _countof(possibilities ) - (append - possibilities))
                            {
                                *append = 0;
                                break;
                            }
                            append += len;
                        }
                    }
                    SetError(OTE_AMBIGUOUS_OPTION_ASS, usage, name_len, name, possibilities);
                    return false;
                }

                if (!found && abbrev)
                    found = abbrev;
            }

            if (!found)
            {
                SetError(OTE_INVALID_OPTION_AS, usage, name_len, name);
                return false;
            }

            // Handle the long option.

            static const T empty_arg = '\0';
            const T* long_arg = &empty_arg;

            if (*name_end)
            {
                if (found->has_arg == LOHA_NOARG)
                {
                    SetError(OTE_ARGUMENT_NOT_ALLOWED_S, usage, found->name);
                    return false;
                }
                long_arg = name_end + 1;
            }
            else if (found->has_arg == LOHA_REQUIRED)
            {
                long_arg = GetArg(0, argc, argv, move_to, walk, argc_to_process);
                if (!long_arg)
                {
                    SetError(OTE_MISSING_REQUIRED_ARGUMENT_S, usage, found->name);
                    return false;
                }
            }

            if (m_count >= m_max)
            {
                SetError(OTE_TOO_MANY_OPTIONS, usage);
                return false;
            }

            if (found->flag)
                *found->flag = found->value;
            if (found->arg)
                *found->arg = long_arg;

            m_flags[m_count] = found->value ? T(found->value) : '-';
            m_values[m_count] = long_arg;
            m_long_opts[m_count] = found;
            ++m_count;
            goto next_arg;
        }

        while (true)
        {
            ++arg;                      // Skip the '-' or '/' option character.
            if (!*arg)
                break;

            // Find the option character in opts.
            const T* o = opts;
            while (*o)
            {
                if (*o == *arg)
                    break;
                // Skip option char.
                ++o;
                // Skip option style char, if any.  ' ' gives a flag the same
                // behavior (the default) as not specifying a style, but makes
                // it possible to have an opts string where style characters
                // are also flag characters:  "abc + * : ." etc.
                if (strchr("-.:+ ", *o))
                    ++o;
            }

            // No match; invalid option.
            if (!*o)
            {
                SetError(OTE_INVALID_OPTION_C, usage, *arg);
                return false;
            }

            // '-' enables '--long' options.
            if (*o == '-' && arg != walk[0] + 1 && *(o - 1) == '-')
            {
                SetError(OTE_INVALID_OPTION_C, usage, *arg);
                return false;
            }

            // Exceeded max options?
            if (m_count >= m_max)
            {
                SetError(OTE_TOO_MANY_OPTIONS, usage);
                return false;
            }

            // Remember the flag.
            m_flags[m_count] = *arg;
            m_values[m_count] = PlusString();

            // '.' lets a flag optionally accept an argument.  The next
            // characters until a whitespace are taken as the argument.
            //
            // Examples where opts == "x.":
            //      "-x"        => flag 'x' is set to "", argv is empty.
            //      "-xfoo"     => flag 'x' is set to "foo", argv is empty.
            //      "-x foo"    => flag 'x' is set to "", argv is "foo".
            if (o[1] == '.')
            {
                ++arg;
                while (IsSpace(*arg))
                    ++arg;
                m_values[m_count++] = arg;
                break;
            }

            // ':' makes a flag require an argument.  If the flag is
            // immediately followed by whitespace (or ':' or '=') then the
            // whitespace (or ':' or '=') is skipped.  The next characters
            // until another whitespace are taken as the argument.
            //
            // Examples where opts == "x:":
            //      "-x"        => usage error.
            //      "-xfoo"     => flag 'x' is set to "foo", argv is empty.
            //      "-x foo"    => flag 'x' is set to "foo", argv is empty.
            if (o[1] == ':')
            {
                ++arg;
                while (IsSpace(*arg))
                    ++arg;
                if (!*arg)
                {
                    arg = GetArg(arg, argc, argv, move_to, walk, argc_to_process);
                    if (!arg)
                    {
                        SetError(OTE_MISSING_REQUIRED_ARGUMENT_C, usage, *o);
                        return false;
                    }
                }
                m_values[m_count++] = arg;
                break;
            }

            // '+' lets a flag optionally be followed by a '+' or '-' to
            // explicitly indicate 'on' or 'off'.
            //
            // Examples where opts = "x+":
            //      "-x"        => flag 'x' is set to "+".
            //      "-x+"       => flag 'x' is set to "+".
            //      "-x-"       => flag 'x' is set to "-".
            //
            // Examples where opts = "x+y":
            //      "-xy"       => flags 'x' and 'y' are "+".
            //      "-x-y"      => flag 'x' is "-", flag 'y' is "+".
            if (o[1] == '+')
            {
                if (arg[1] == '+')
                    ++arg;
                else if (arg[1] == '-')
                {
                    m_values[m_count] = MinusString();
                    ++arg;
                }
            }

            ++m_count;
        }

next_arg:
        --argc;
        if (argv == walk)
        {
            ++argv;
            ++move_to;
        }
        ++walk;
    }

    // Keep arguments contiguous by shifting to replace flags that have been
    // parsed.
    if (flags & OPT_ANYWHERE)
        *move_to = *walk;

    // Check number of arguments.
    if (!((argc == 0 && (flags & OPT_NONE)) ||
          (argc == 1 && (flags & OPT_ONE)) ||
          (argc == 2 && (flags & OPT_TWO)) ||
          (argc == 3 && (flags & OPT_THREE)) ||
          (argc > 3 && (flags & OPT_MORE))))
    {
        SetError(OTE_WRONG_NUMBER_OF_ARGUMENTS, usage);
        return false;
    }

    return true;
}

template<class T>
bool OptionsTemplate<T>::GetValue(
    unsigned iOpt,
    T& chOpt,
    const T*& value,
    const LongOption<T>** long_opt) const
{
    if (iOpt >= m_count)
        return false;
    chOpt = m_flags[iOpt];
    value = m_values[iOpt];
    if (long_opt)
        *long_opt = m_long_opts[iOpt];
    return true;
}

template<class T>
const T* OptionsTemplate<T>::GetValue(
    T chOpt,
    unsigned iSubOpt) const
{
    for (unsigned ii = 0; ii < m_count; ++ii)
    {
        if (chOpt == m_flags[ii])
        {
            if (iSubOpt)
            {
                --iSubOpt;
                continue;
            }
            return m_values[ii];
        }
    }
    return nullptr;
}

template<class T>
void OptionsTemplate<T>::SetError(
    OptionsTemplateError i,
    const T* usage,
    ...)
{
    va_list args;
    va_start(args, usage);

    ClearError();

    const T* const format = c_errors[i];

    const size_t size = 1024;
    m_error = new T[size];

    T* p = m_error;
    p += Sprintf(m_error, size, p, c_errors[OTE_USAGE_S], usage);
    p += SprintfV(m_error, size, p, format, args);

    va_end(args);
}

template<class T>
const T* OptionsTemplate<T>::GetArg(
    const T* p,
    int& argc,
    const T**& argv,
    const T**& move_to,
    const T**& walk,
    int& argc_to_process)
{
    if (p && *p)
        return p;

    if (!argc)
        return nullptr;

    --argc;
    --argc_to_process;
    if (argv == move_to)
    {
        ++argv;
        ++move_to;
    }
    ++walk;
    return walk[0];
}

template<class T>
int OptionsTemplate<T>::Sprintf(const T* buffer, size_t size, T* append, const T* format, ...)
{
    va_list args;
    va_start(args, format);
    const int len = SprintfV(buffer, size, append, format, args);
    va_end(args);
    return len;
}

/*
 * Specializations.
 */

inline int OptionsTemplate<char>::IsSpace(char ch) { return isspace(static_cast<unsigned char>(ch)); }
inline int OptionsTemplate<WCHAR>::IsSpace(WCHAR ch) { return iswspace(ch); }
inline const char* OptionsTemplate<char>::PlusString() { return "+"; }
inline const char* OptionsTemplate<char>::MinusString() { return "-"; }
inline const WCHAR* OptionsTemplate<WCHAR>::PlusString() { return L"+"; }
inline const WCHAR* OptionsTemplate<WCHAR>::MinusString() { return L"-"; }

inline bool OptionsTemplate<char>::IsEqual(const char* name1, const char* name2, size_t len, bool caseless)
{
    if (caseless)
        return _strnicmp(name1, name2, len) == 0;
    else
        return strncmp(name1, name2, len) == 0;
}

inline bool OptionsTemplate<WCHAR>::IsEqual(const WCHAR* name1, const WCHAR* name2, size_t len, bool caseless)
{
    if (caseless)
        return _wcsnicmp(name1, name2, len) == 0;
    else
        return wcsncmp(name1, name2, len) == 0;
}

inline int OptionsTemplate<char>::SprintfV(const char* buffer, size_t size, char* append, const char* format, va_list args) { return vsnprintf_s(append, size - (append - buffer), _TRUNCATE, format, args); }
inline int OptionsTemplate<WCHAR>::SprintfV(const WCHAR* buffer, size_t size, WCHAR* append, const WCHAR* format, va_list args) { return _vsnwprintf_s(append, size - (append - buffer), _TRUNCATE, format, args); }

const char* const OptionsTemplate<char>::c_errors[] =
{
    "Invalid option: '%c'.\n",
    "Invalid option: '--%.*s'.\n",
    "Too many options.\n",
    "Option '%c' missing required argument.\n",
    "Option '%s' missing required argument.\n",
    "Option '%s' doesn't allow an argument.\n",
    "Missing/wrong number of arguments.\n",
    "Ambiguous option '--%.*s'; possibilities include:\n%s",
    "    '--%s'\n",
    "Usage: %s\n",
};
const WCHAR* const OptionsTemplate<WCHAR>::c_errors[] =
{
    L"Invalid option: '%c'.\n",
    L"Invalid option: '--%.*s'.\n",
    L"Too many options.\n",
    L"Option '%c' missing required argument.\n",
    L"Option '%s' missing required argument.\n",
    L"Option '%s' doesn't allow an argument.\n",
    L"Missing/wrong number of arguments.\n",
    L"Ambiguous option '--%.*s'; possibilities include:\n%s",
    L"    '--%s'\n",
    L"Usage: %s\n",
};

typedef OptionsTemplate<char> OptionsA;
typedef OptionsTemplate<WCHAR> OptionsW;

#ifdef UNICODE
typedef OptionsW Options;
#else
typedef OptionsA Options;
#endif

