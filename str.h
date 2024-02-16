// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <string>

inline size_t StrLen(const char* p) { return strlen(p); }
inline size_t StrLen(const WCHAR* p) { return wcslen(p); }
inline int StrCmp(const char* pa, const char* pb) { return strcmp(pa, pb); }
inline int StrCmp(const WCHAR* pa, const WCHAR* pb) { return wcscmp(pa, pb); }
inline int StrCmpI(const char* pa, const char* pb) { return _strcmpi(pa, pb); }
inline int StrCmpI(const WCHAR* pa, const WCHAR* pb) { return _wcsicmp(pa, pb); }
inline bool IsPathSeparator(WCHAR ch) { return ch == '/' || ch == '\\'; }

// This is the extended max path length, including the NUL terminator.
// An explanation behind why can be found here:
// https://stackoverflow.com/questions/15262110/what-happens-internally-when-a-file-path-exceeds-approx-32767-characters-in-win
inline unsigned MaxPath() { return 32744; }

template <class T>
class Str
{
public:
                        Str<T>() : m_p(s_empty) {}
                        Str<T>(const T* p) { Set(p); }
                        Str<T>(const Str<T>& s) { Set(s); }
                        Str<T>(Str<T>&& s) { Set(std::move(s)); }
                        ~Str<T>() { if (m_capacity) free(m_p); }

    Str<T>&             operator=(const T* p) { Set(p); return *this; }
    Str<T>&             operator=(const Str<T>& s) { Set(s); return *this; }
    Str<T>&             operator=(Str<T>&& s) { Set(std::move(s)); return *this; }

    const T*            Text() const { return m_p; }
    unsigned            Length() const;
    unsigned            Capacity() const { return m_capacity; }

    bool                Empty() const { return !m_p[0]; }
    void                Clear();
    void                Free();
    WCHAR*              Detach();

    bool                Equal(const T* p) const { return !StrCmp(p, Text()); }
    bool                Equal(const Str<T>* s) const;
    bool                Equal(const Str<T>& s) const { return Equal(&s); }
    bool                EqualI(const T* p) const { return !StrCmpI(p, Text()); }
    bool                EqualI(const Str<T>* s) const;
    bool                EqualI(const Str<T>& s) const { return EqualI(&s); }

    void                SetEnd(const T* end);
    void                SetLength(size_t len);

    void                Set(const T* p, size_t len=-1);
    void                Set(const Str<T>& s) { Set(s.Text(), s.Length()); }
    void                Set(Str<T>&& s);

    T*                  Reserve(size_t capacity=0);
    T*                  ReserveAtEnd(size_t more=0);

    T*                  ReserveMaxPath();
    void                ResyncLength();

    void                SetAt(const T* p, T ch);

    void                Append(T ch);
    void                Append(const T* p, size_t len=-1);
    void                Append(const Str<T>& s) { Append(s.Text(), s.Length()); }
    void                AppendSpaces(int spaces);

    void                PrintfV(const T* format, va_list args);
    void                Printf(const T* format, ...);

    void                TrimRight();

    void                ToLower();
    void                ToUpper();

    void                Swap(Str<T>& s);

protected:
    void                Transform(DWORD dwMapFlags);
    static int          IsSpace(T ch);

protected:
    T*                  m_p;
    mutable unsigned    m_length = 0;
    unsigned            m_capacity = 0;

    static T            s_empty[1];
    static const T      c_spaces[33];
};

class StrA;
class StrW;

inline int Str<char>::IsSpace(char ch) { return isspace(static_cast<unsigned char>(ch)); }
inline int Str<WCHAR>::IsSpace(WCHAR ch) { return iswspace(ch); }

class StrA : public Str<char>
{
public:
                        StrA() = default;
                        StrA(const char* p) { Set(p); }
                        StrA(const StrA& s) { Set(s); }

    void                SetA(const char* p, size_t len=-1) { Set(p, len); }
    void                SetW(const WCHAR* p, size_t len=-1);

    void                SetA(const StrA& s) { SetA(s.Text(), s.Length()); }
    void                SetW(const StrW& s);
};

class StrW : public Str<WCHAR>
{
public:
                        StrW() = default;
                        StrW(const WCHAR* p) { Set(p); }
                        StrW(const StrW& s) { Set(s); }

    void                SetA(const char* p, size_t len=-1);
    void                SetW(const WCHAR* p, size_t len=-1) { Set(p, len); }

    void                SetA(const StrA& s);
    void                SetW(const StrW& s) { SetW(s.Text(), s.Length()); }
};

template <class T>
unsigned Str<T>::Length() const
{
    if (m_length == unsigned(-1))
        m_length = unsigned(StrLen(m_p));
    return m_length;
}

template <class T>
void Str<T>::Clear()
{
    m_length = 0;
    if (m_capacity)
        m_p[0] = '\0';
}

template <class T>
void Str<T>::Free()
{
    if (m_capacity)
        free(m_p);
    m_p = s_empty;
    m_length = 0;
    m_capacity = 0;
}

template <class T>
WCHAR* Str<T>::Detach()
{
    WCHAR* p = Capacity() ? m_p : nullptr;
    m_p = nullptr;
    Free();
    return p;
}

template <class T>
bool Str<T>::Equal(const Str<T>* s) const
{
    assert(s);
    if (Length() != s->Length())
        return false;
    if (StrCmp(Text(), s->Text()))
        return false;
    return true;
}

template <class T>
bool Str<T>::EqualI(const Str<T>* s) const
{
    assert(s);
    if (Length() != s->Length())
       return false;
    if (StrCmpI(Text(), s->Text()))
       return false;
    return true;
}

template <class T>
void Str<T>::SetEnd(const T* end)
{
    SetLength(end - m_p);
}

template <class T>
void Str<T>::SetLength(size_t len)
{
    assert(len <= Length());
    if (len < Length())
    {
        m_length = unsigned(len);
        m_p[len] = '\0';
    }
}

template <class T>
void Str<T>::Set(const T* p, size_t len)
{
    assert(p != m_p || !m_capacity);
    Clear();
    Append(p, len);
}

template <class T>
void Str<T>::Set(Str<T>&& s)
{
    m_p = s.m_p;
    m_length = s.m_length;
    m_capacity = s.m_capacity;

    s.m_p = s.s_empty;
    s.m_length = 0;
    s.m_capacity = 0;
}

template <class T>
void Str<T>::SetAt(const T* p, T ch)
{
    assert(p);
    assert(ch);
    assert(m_p <= p);
    assert(p < m_p + StrLen(m_p));
    *const_cast<T*>(p) = ch;
}

template <class T>
void Str<T>::Append(T ch)
{
    assert(ch);
    Append(&ch, 1);
}

template <class T>
void Str<T>::Append(const T* p, size_t len)
{
    if (p)
    {
        if (int(len) < 0)
            len = StrLen(p);
        T* concat = ReserveAtEnd(len + 1);
        memcpy(concat, p, len * sizeof(T));
        m_length += unsigned(len);
        m_p[m_length] = '\0';
    }
}

template <class T>
void Str<T>::AppendSpaces(int spaces)
{
    while (spaces > 0)
    {
        unsigned add = std::min<unsigned>(spaces, _countof(c_spaces) - 1);
        Append(c_spaces, add);
        spaces -= add;
    }
}

template <class T>
T* Str<T>::Reserve(size_t capacity)
{
    if (capacity > m_capacity)
    {
        T* const old = m_capacity ? m_p : nullptr;
        T* const p = static_cast<T*>(realloc(old, capacity * sizeof(T)));
        if (p)
        {
            m_p = p;
            m_capacity = unsigned(capacity);
        }
    }
    return m_p;
}

template <class T>
T* Str<T>::ReserveAtEnd(size_t more)
{
    unsigned len = Length();
    T* p = Reserve(len + more);
    if (p)
        p += len;
    return p;
}

template <class T>
T* Str<T>::ReserveMaxPath()
{
    Clear();
    Reserve(MaxPath());
    ResyncLength();
    return m_p;
}

template <class T>
void Str<T>::ResyncLength()
{
    m_length = unsigned(-1);
}

int __vsnprintf(char* buffer, size_t len, const char* format, va_list args);
int __vsnprintf(WCHAR* buffer, size_t len, const WCHAR* format, va_list args);

template <class T>
void Str<T>::PrintfV(const T* format, va_list args)
{
    const size_t len = Length();
    size_t cap = Capacity() - Length();

    int res = -1;
    if (cap > 1)
    {
        errno = 0;
        res = __vsnprintf(m_p + len, cap, format, args);
        if (errno)
        {
            assert(false);
            return;
        }
    }

    if (res < 0)
    {
        cap = std::max<size_t>(cap * 2, 100);
        while (res < 0)
        {
            // Subtract 1 from the max character count to ensure room for null
            // terminator; see MSDN for idiosyncracy of the snprintf family of
            // functions.
            errno = 0;
            res = __vsnprintf(Reserve(len + cap) + len, cap, format, args);
            if (errno)
            {
                assert(false);
                return;
            }
            cap *= 2;
        }
    }

    m_length += res;
    assert(m_length == StrLen(m_p));
    assert(m_length < m_capacity);
}

template <class T>
void Str<T>::Printf(const T* format, ...)
{
    va_list args;
    va_start(args, format);

    PrintfV(format, args);

    va_end(args);
}

template <class T>
void Str<T>::TrimRight()
{
    if (Length())
    {
        const T* last = Text() + Length() - 1;
        while (last >= Text() && IsSpace(*last))
            --last;
        SetEnd(last + 1);
    }
}

template <>
inline void Str<char>::ToLower()
{
    StrW tmp;
    tmp.SetA(Text(), Length());
    tmp.ToLower();
    StrA tmp2;
    tmp2.SetW(tmp.Text(), tmp.Length());
    Set(std::move(tmp2));
}

template <>
inline void Str<WCHAR>::ToLower()
{
    Transform(LCMAP_LOWERCASE);
}

template <>
inline void Str<char>::ToUpper()
{
    StrW tmp;
    tmp.SetA(Text(), Length());
    tmp.ToUpper();
    StrA tmp2;
    tmp2.SetW(tmp.Text(), tmp.Length());
    Set(std::move(tmp2));
}

template <>
inline void Str<WCHAR>::ToUpper()
{
    Transform(LCMAP_UPPERCASE);
}

template <>
inline void Str<WCHAR>::Transform(DWORD dwMapFlags)
{
    int len = Length();
    Str<WCHAR> tmp;
    WCHAR* p = tmp.Reserve(len + std::max<size_t>(len / 10, 10));
    len = LCMapStringW(LOCALE_USER_DEFAULT, dwMapFlags, Text(), int(Length()), p, int(tmp.Capacity()));
    if (!len)
    {
        len = LCMapStringW(LOCALE_USER_DEFAULT, dwMapFlags, Text(), int(Length()), nullptr, 0);
        p = tmp.Reserve(len + 1);
        len = LCMapStringW(LOCALE_USER_DEFAULT, dwMapFlags, Text(), int(Length()), p, int(tmp.Capacity()));
    }
    m_length = len;
    m_p[len] = '\0';
    assert(m_length < m_capacity);
}

template <class T>
void Str<T>::Swap(Str<T>& s)
{
    T* p = m_p;
    m_p = s.m_p;
    s.m_p = p;

    unsigned len = m_length;
    m_length = s.m_length;
    s.m_length = len;

    unsigned cap = m_capacity;
    m_capacity = s.m_capacity;
    s.m_capacity = cap;
}

/*
 * StrA and StrW classes.
 */

inline void StrA::SetW(const StrW& s)
{
    SetW(s.Text(), s.Length());
}

inline void StrW::SetA(const StrA& s)
{
    SetA(s.Text(), s.Length());
}

/*
 * String helpers.
 */

void StripTrailingSlashes(StrW& s);
void EnsureTrailingSlash(StrW& s);
unsigned TruncateWcwidth(StrW& s, unsigned truncate_width, WCHAR truncation_char);

struct EqualCase        { bool operator()(const WCHAR* a, const WCHAR* b) const noexcept; };
struct EqualCaseless    { bool operator()(const WCHAR* a, const WCHAR* b) const noexcept; };
struct HashCase         { _NODISCARD size_t operator()(const WCHAR* key) const noexcept; };
struct HashCaseless     { _NODISCARD size_t operator()(const WCHAR* key) const noexcept; };

