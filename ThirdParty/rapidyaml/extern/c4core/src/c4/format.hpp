#ifndef _C4_FORMAT_HPP_
#define _C4_FORMAT_HPP_

/** @file format.hpp provides type-safe facilities for formatting arguments
 * to string buffers */

#include "c4/to_chars.hpp"
#include "c4/blob.hpp"

#include <memory>
#include <functional>

#ifdef _MSC_VER
#   pragma warning(push)
#   pragma warning(disable: 4800) // forcing value to bool 'true' or 'false' (performance warning)
#   pragma warning(disable: 4996) // snprintf/scanf: this function or variable may be unsafe
#endif

namespace c4 {

/** @defgroup formatting_functions Formatting multiple values<->string
 * @brief Convert a sequence of values to/from a string.
 * @ingroup formatting
 */

/** C-style printing into a buffer
 * @ingroup formatting_functions */
size_t sprintf(substr buf, const char * fmt, ...);
//size_t sscanf(csubstr buf, const char *fmt, ...);

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @addtogroup generic_tofrom_chars
 * @{ */

namespace fmt {

/** a generic class for providing type-specific formatting settings */
template<class T>
struct fmt_wrapper;

/** mark a variable to be written in its default custom format wrapper */
template<class T, class... Args>
inline fmt_wrapper<T> fmt(T &v, Args && ...args)
{
    return fmt_wrapper<T>(std::ref(v), std::forward<Args>(args)...);
}

} // namespace fmt

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// formatting integral types

/** @addtogroup generic_tofrom_chars
 * @{ */

namespace fmt {

/** format an integral type */
template<class T>
struct integral
{
    T val;
    T radix;
    integral(T val_, uint8_t radix_=10) : val(val_), radix(radix_) {}
};


template<> struct fmt_wrapper<  int8_t> : public integral<  int8_t> { using integral<  int8_t>::integral; };
template<> struct fmt_wrapper< int16_t> : public integral< int16_t> { using integral< int16_t>::integral; };
template<> struct fmt_wrapper< int32_t> : public integral< int32_t> { using integral< int32_t>::integral; };
template<> struct fmt_wrapper< int64_t> : public integral< int64_t> { using integral< int64_t>::integral; };

template<> struct fmt_wrapper< uint8_t> : public integral< uint8_t> { using integral< uint8_t>::integral; };
template<> struct fmt_wrapper<uint16_t> : public integral<uint16_t> { using integral<uint16_t>::integral; };
template<> struct fmt_wrapper<uint32_t> : public integral<uint32_t> { using integral<uint32_t>::integral; };
template<> struct fmt_wrapper<uint64_t> : public integral<uint64_t> { using integral<uint64_t>::integral; };


/** format the integral argument as a hex value */
template<class T>
inline fmt_wrapper<T> hex(T v)
{
    static_assert(std::is_integral<T>::value, "must be an integral type");
    return fmt_wrapper<T>(v, T(16));
}

/** format the integral argument as an octal value */
template<class T>
inline fmt_wrapper<T> oct(T v)
{
    static_assert(std::is_integral<T>::value, "must be an integral type");
    return fmt_wrapper<T>(v, T(8));
}

/** format the integral argument as a binary 0-1 value */
template<class T>
inline fmt_wrapper<T> bin(T v)
{
    static_assert(std::is_integral<T>::value, "must be an integral type");
    return fmt_wrapper<T>(v, T(2));
}

} // namespace fmt

inline size_t to_chars(substr buf, fmt::fmt_wrapper< int8_t> fmt) { return itoa(buf, fmt.val, fmt.radix); }
inline size_t to_chars(substr buf, fmt::fmt_wrapper<int16_t> fmt) { return itoa(buf, fmt.val, fmt.radix); }
inline size_t to_chars(substr buf, fmt::fmt_wrapper<int32_t> fmt) { return itoa(buf, fmt.val, fmt.radix); }
inline size_t to_chars(substr buf, fmt::fmt_wrapper<int64_t> fmt) { return itoa(buf, fmt.val, fmt.radix); }

inline size_t to_chars(substr buf, fmt::fmt_wrapper< uint8_t> fmt) { return utoa(buf, fmt.val, fmt.radix); }
inline size_t to_chars(substr buf, fmt::fmt_wrapper<uint16_t> fmt) { return utoa(buf, fmt.val, fmt.radix); }
inline size_t to_chars(substr buf, fmt::fmt_wrapper<uint32_t> fmt) { return utoa(buf, fmt.val, fmt.radix); }
inline size_t to_chars(substr buf, fmt::fmt_wrapper<uint64_t> fmt) { return utoa(buf, fmt.val, fmt.radix); }

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// formatting real types

/** @addtogroup generic_tofrom_chars
 * @{ */

namespace fmt {

template<class T>
struct real
{
    T val;
    int precision;
    RealFormat_e fmt;
    real(T v, int prec=-1, RealFormat_e f=FTOA_FLOAT) : val(v), precision(prec), fmt(f)  {}
};


template<> struct fmt_wrapper< float> : public real< float> { using real< float>::real; };
template<> struct fmt_wrapper<double> : public real<double> { using real<double>::real; };

} // namespace fmt

inline size_t to_chars(substr buf, fmt::fmt_wrapper< float> fmt) { return ftoa(buf, fmt.val, fmt.precision, fmt.fmt); }
inline size_t to_chars(substr buf, fmt::fmt_wrapper<double> fmt) { return dtoa(buf, fmt.val, fmt.precision, fmt.fmt); }

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// writing raw binary data

/** @addtogroup generic_tofrom_chars
 * @{ */

namespace fmt {

/** @see blob_ */
template<class T>
struct raw_wrapper_ : public blob_<T>
{
    size_t alignment;

    template<class... BlobArgs>
    C4_ALWAYS_INLINE raw_wrapper_(BlobArgs&& ...args, size_t alignment_) noexcept
        :
        blob_<T>(std::forward<BlobArgs>(args)...),
        alignment(alignment_)
    {
        C4_ASSERT_MSG(alignment > 0 && (alignment & (alignment - 1)) == 0, "alignment must be a power of two");
    }
};

using const_raw_wrapper = raw_wrapper_<cbyte>;
using raw_wrapper = raw_wrapper_<byte>;


/** mark a variable to be written in raw binary format
 * @see blob_ */
template<class... BlobArgs>
inline const_raw_wrapper craw(BlobArgs&& ...args, size_t alignment=alignof(max_align_t))
{
    return const_raw_wrapper(std::forward<BlobArgs>(args)..., alignment);
}

/** mark a variable to be read in raw binary format
 * @see blob_  */
template<class... BlobArgs>
inline raw_wrapper raw(BlobArgs&& ...args, size_t alignment=alignof(max_align_t))
{
    return raw_wrapper(std::forward<BlobArgs>(args)..., alignment);
}

} // namespace fmt


/** write a variable in raw binary format */
template<class T>
inline size_t to_chars(substr buf, fmt::const_raw_wrapper r)
{
    void * vptr = buf.str;
    size_t space = buf.len;
    auto ptr = (decltype(buf.str)) std::align(r.alignment, r.len, vptr, space);
    if(ptr == nullptr) return r.alignment + r.len;
    C4_CHECK(ptr >= r.buf && ptr <= r.buf + r.len);
    size_t dim = (ptr - buf.str) + r.len;
    if(dim <= buf.len) memcpy(ptr, r.buf, r.len);
    return dim;
}


/** read a variable in raw binary format */
template<class T>
inline size_t from_chars(csubstr buf, fmt::raw_wrapper *r)
{
    void * vptr = (void*)buf.str;
    size_t space = buf.len;
    auto ptr = (decltype(buf.str)) std::align(r->alignment, r->len, vptr, space);
    C4_CHECK(ptr != nullptr);
    C4_CHECK(ptr >= r->buf && ptr <= r->buf + r->len);
    size_t dim = (ptr - buf.str) + r->len;
    memcpy(r->buf, ptr, r->len);
    return r->len;
}

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** terminates the variadic recursion
 * @ingroup formatting_functions */
inline size_t cat(substr /*buf*/)
{
    return 0;
}

/** serialize the arguments, concatenating them to the given fixed-size buffer.
 * The buffer size is strictly respected: no writes will occur beyond its end.
 * @return the number of characters needed to write all the arguments into the buffer.
 * @see catrs() if instead of a fixed-size buffer, a resizeable container is desired
 * @see uncat() for the inverse function
 * @see catsep() if a separator between each argument is to be used
 * @see format() if a format string is desired
 * @ingroup formatting_functions */
template<class Arg, class... Args>
size_t cat(substr buf, Arg const& C4_RESTRICT a, Args const& C4_RESTRICT ...more)
{
    size_t num = to_chars(buf, a);
    buf  = buf.len >= num ? buf.sub(num) : substr{};
    num += cat(buf, more...);
    return num;
}

/** like cat but return a substr instead of a size */
template<class... Args>
substr cat_sub(substr buf, Args && ...args)
{
    size_t sz = cat(buf, std::forward<Args>(args)...);
    return {buf.str, sz <= buf.len ? sz : buf.len};
}


//-----------------------------------------------------------------------------

/** terminates the variadic recursion
 * @ingroup formatting_functions */
inline size_t uncat(csubstr /*buf*/)
{
    return 0;
}

/** deserialize the arguments from the given buffer.
 *
 * @return the number of characters read from the buffer, or csubstr::npos
 *   if a conversion was not successful.
 * @see cat(). uncat() is the inverse of cat().
 * @ingroup formatting_functions */
template<class Arg, class... Args>
size_t uncat(csubstr buf, Arg & C4_RESTRICT a, Args & C4_RESTRICT ...more)
{
    size_t out = from_chars_first(buf, &a);
    if(C4_UNLIKELY(out == csubstr::npos)) return csubstr::npos;
    buf  = buf.len >= out ? buf.sub(out) : substr{};
    size_t num = uncat(buf, more...);
    if(C4_UNLIKELY(num == csubstr::npos)) return csubstr::npos;
    return out + num;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

namespace detail {

template<class Sep>
inline size_t catsep_more(substr /*buf*/, Sep const& C4_RESTRICT /*sep*/)
{
    return 0;
}

template<class Sep, class Arg, class... Args>
size_t catsep_more(substr buf, Sep const& C4_RESTRICT sep, Arg const& C4_RESTRICT a, Args const& C4_RESTRICT ...more)
{
    size_t ret = to_chars(buf, sep), num = ret;
    buf  = buf.len >= ret ? buf.sub(ret) : substr{};
    ret  = to_chars(buf, a);
    num += ret;
    buf  = buf.len >= ret ? buf.sub(ret) : substr{};
    ret  = catsep_more(buf, sep, more...);
    num += ret;
    return num;
}

template<class Sep>
inline size_t uncatsep_more(csubstr /*buf*/, Sep & /*sep*/)
{
    return 0;
}

template<class Sep, class Arg, class... Args>
size_t uncatsep_more(csubstr buf, Sep & C4_RESTRICT sep, Arg & C4_RESTRICT a, Args & C4_RESTRICT ...more)
{
    size_t ret = from_chars_first(buf, &sep), num = ret;
    if(C4_UNLIKELY(ret == csubstr::npos)) return csubstr::npos;
    buf  = buf.len >= ret ? buf.sub(ret) : substr{};
    ret  = from_chars_first(buf, &a);
    if(C4_UNLIKELY(ret == csubstr::npos)) return csubstr::npos;
    num += ret;
    buf  = buf.len >= ret ? buf.sub(ret) : substr{};
    ret  = uncatsep_more(buf, sep, more...);
    if(C4_UNLIKELY(ret == csubstr::npos)) return csubstr::npos;
    num += ret;
    return num;
}

} // namespace detail


/** serialize the arguments, concatenating them to the given fixed-size
 * buffer, using a separator between each argument.
 * The buffer size is strictly respected: no writes will occur beyond its end.
 * @return the number of characters needed to write all the arguments into the buffer.
 * @see catseprs() if instead of a fixed-size buffer, a resizeable container is desired
 * @see uncatsep() for the inverse function
 * @see cat() if no separator is needed
 * @see format() if a format string is desired
 * @ingroup formatting_functions */
template<class Sep, class Arg, class... Args>
size_t catsep(substr buf, Sep const& C4_RESTRICT sep, Arg const& C4_RESTRICT a, Args const& C4_RESTRICT ...more)
{
    size_t num = to_chars(buf, a);
    buf  = buf.len >= num ? buf.sub(num) : substr{};
    num += detail::catsep_more(buf, sep, more...);
    return num;
}

/** like catsep but return a substr instead of a size
 * @see catsep(). uncatsep() is the inverse of catsep().
 * @ingroup formatting_functions */
template<class... Args>
substr catsep_sub(substr buf, Args && ...args)
{
    size_t sz = catsep(buf, std::forward<Args>(args)...);
    return {buf.str, sz <= buf.len ? sz : buf.len};
}

/** deserialize the arguments from the given buffer, using a separator.
 *
 * @return the number of characters read from the buffer, or csubstr::npos
 *   if a conversion was not successful
 * @see catsep(). uncatsep() is the inverse of catsep().
 * @ingroup formatting_functions */
template<class Sep, class Arg, class... Args>
size_t uncatsep(csubstr buf, Sep & C4_RESTRICT sep, Arg & C4_RESTRICT a, Args & C4_RESTRICT ...more)
{
    size_t ret = from_chars_first(buf, &a), num = ret;
    if(C4_UNLIKELY(ret == csubstr::npos)) return csubstr::npos;
    buf  = buf.len >= ret ? buf.sub(ret) : substr{};
    ret  = detail::uncatsep_more(buf, sep, more...);
    if(C4_UNLIKELY(ret == csubstr::npos)) return csubstr::npos;
    num += ret;
    return num;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** terminates the variadic recursion
 * @ingroup formatting_functions */
inline size_t format(substr buf, csubstr fmt)
{
    return to_chars(buf, fmt);
}

/** using a format string, serialize the arguments into the given
 * fixed-size buffer.
 * The buffer size is strictly respected: no writes will occur beyond its end.
 * In the format string, each argument is marked with a compact
 * curly-bracket pair: {}. Arguments beyond the last curly bracket pair
 * are silently ignored. For example:
 * @code{.cpp}
 * c4::format(buf, "the {} drank {} {}", "partier", 5, "beers"); // the partier drank 5 beers
 * c4::format(buf, "the {} drank {} {}", "programmer", 6, "coffees"); // the programmer drank 6 coffees
 * @endcode
 * @return the number of characters needed to write into the buffer.
 * @see formatrs() if instead of a fixed-size buffer, a resizeable container is desired
 * @see unformat() for the inverse function
 * @see cat() if no format or separator is needed
 * @see catsep() if no format is needed, but a separator must be used
 * @ingroup formatting_functions */
template<class Arg, class... Args>
size_t format(substr buf, csubstr fmt, Arg const& C4_RESTRICT a, Args const& C4_RESTRICT ...more)
{
    auto pos = fmt.find("{}");
    if(C4_UNLIKELY(pos == csubstr::npos))
    {
        return format(buf, fmt);
    }
    size_t num = to_chars(buf, fmt.sub(0, pos));
    size_t out = num;
    buf  = buf.len >= num ? buf.sub(num) : substr{};
    num  = to_chars(buf, a);
    out += num;
    buf  = buf.len >= num ? buf.sub(num) : substr{};
    num  = format(buf, fmt.sub(pos + 2), more...);
    out += num;
    return out;
}

/** like format but return a substr instead of a size
 * @see format()
 * @see catsep(). uncatsep() is the inverse of catsep().
 * @ingroup formatting_functions */
template<class... Args>
substr format_sub(substr buf, csubstr fmt, Args && ...args)
{
    size_t sz = c4::format(buf, fmt, std::forward<Args>(args)...);
    return {buf.str, sz <= buf.len ? sz : buf.len};
}


//-----------------------------------------------------------------------------

/** terminates the variadic recursion
 * @ingroup formatting_functions */
inline size_t unformat(csubstr buf, csubstr fmt)
{
    return 0;
}

/** using a format string, deserialize the arguments from the given
 * buffer.
 * @return the number of characters read from the buffer, or npos if a conversion failed.
 * @see format() this is the inverse function to format().
 * @ingroup formatting_functions */
template<class Arg, class... Args>
size_t unformat(csubstr buf, csubstr fmt, Arg & C4_RESTRICT a, Args & C4_RESTRICT ...more)
{
    size_t pos = fmt.find("{}");
    if(C4_UNLIKELY(pos == csubstr::npos))
    {
        return unformat(buf, fmt);
    }
    size_t num = pos;
    size_t out = num;
    buf  = buf.len >= num ? buf.sub(num) : substr{};
    num  = from_chars_first(buf, &a);
    if(C4_UNLIKELY(num == csubstr::npos)) return csubstr::npos;
    out += num;
    buf  = buf.len >= num ? buf.sub(num) : substr{};
    num  = unformat(buf, fmt.sub(pos + 2), more...);
    if(C4_UNLIKELY(num == csubstr::npos)) return csubstr::npos;
    out += num;
    return out;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** a tag type
 * @see catrs
 * @ingroup formatting_functions
 * */
struct append_t {};

/** a tag variable
 * @see catrs
 * @ingroup formatting_functions
 * */
constexpr const append_t append = {};

//-----------------------------------------------------------------------------

/** like cat(), but receives a container, and resizes it as needed to contain
 * the result. The container is overwritten. To append to it, use the append
 * overload.
 *
 * @see cat()
 * @ingroup formatting_functions */
template<class CharOwningContainer, class... Args>
inline void catrs(CharOwningContainer * C4_RESTRICT cont, Args const& C4_RESTRICT ...args)
{
    substr buf = to_substr(*cont);
    size_t ret = cat(buf, args...);
    cont->resize(ret);
    if(ret > buf.len)
    {
        buf = to_substr(*cont);
        ret = cat(buf, args...);
        if(ret != buf.len)
        {
            cont->resize(ret);
        }
    }
}

/** like cat(), but receives a container, and appends to it instead of
 * overwriting it. The container is resized as needed to contain the result.
 *
 * @return the region newly appended to the original container
 * @see cat()
 * @see catrs()
 * @ingroup formatting_functions */
template<class CharOwningContainer, class... Args>
inline csubstr catrs(append_t, CharOwningContainer * C4_RESTRICT cont, Args const& C4_RESTRICT ...args)
{
    const size_t pos = cont->size();
    substr buf = to_substr(*cont).sub(pos);
    size_t ret = cat(buf, args...);
    cont->resize(pos + ret);
    if(ret > buf.len)
    {
        buf = to_substr(*cont).sub(pos);
        ret = cat(buf, args...);
        if(ret != buf.len)
        {
            cont->resize(pos + ret);
        }
    }
    return to_csubstr(*cont).range(pos, cont->size());
}

//-----------------------------------------------------------------------------

/** like catsep(), but receives a container, and resizes it as needed to contain the result.
 * The container is overwritten. To append to the container use the append overload.
 * @see catsep()
 * @ingroup formatting_functions */
template<class CharOwningContainer, class Sep, class... Args>
inline void catseprs(CharOwningContainer * C4_RESTRICT cont, Sep const& C4_RESTRICT sep, Args const& C4_RESTRICT ...args)
{
    substr buf = to_substr(*cont);
    size_t ret = catsep(buf, sep, args...);
    cont->resize(ret);
    if(ret > buf.len)
    {
        buf = to_substr(*cont);
        ret = catsep(buf, sep, args...);
        if(ret != buf.len)
        {
            cont->resize(ret);
        }
    }
}

/**
 * @overload catseprs
 * @ingroup formatting_functions */
template<class CharOwningContainer, class Sep, class... Args>
inline CharOwningContainer catseprs(Sep const& C4_RESTRICT sep, Args const& C4_RESTRICT ...args)
{
    CharOwningContainer cont;
    catseprs(&cont, std::cref(sep), std::forward<Args>(args)...);
    return cont;
}

/** like catsep(), but receives a container, and appends the arguments, resizing the
 * container as needed to contain the result. The buffer is appended to.
 * @ingroup formatting_functions */
template<class CharOwningContainer, class Sep, class... Args>
inline csubstr catseprs(append_t, CharOwningContainer * C4_RESTRICT cont, Sep const& C4_RESTRICT sep, Args const& C4_RESTRICT ...args)
{
    const size_t pos = cont->size();
    substr buf = to_substr(*cont).sub(pos);
    size_t ret = catsep(buf, sep, args...);
    cont->resize(pos + ret);
    if(ret > buf.len)
    {
        buf = to_substr(*cont).sub(pos);
        ret = catsep(buf, sep, args...);
        if(ret != buf.len)
        {
            cont->resize(pos + ret);
        }
    }
    return to_csubstr(*cont).range(pos, cont->size());
}


//-----------------------------------------------------------------------------

/** like format(), but receives a container, and resizes it as needed
 * to contain the result.  The container is overwritten. To append to
 * the container use the append overload.
 * @see format()
 * @ingroup formatting_functions */
template<class CharOwningContainer, class... Args>
inline void formatrs(CharOwningContainer * C4_RESTRICT cont, csubstr fmt, Args const&  C4_RESTRICT ...args)
{
    substr buf = to_substr(*cont);
    size_t ret = format(buf, fmt, args...);
    cont->resize(ret);
    if(ret > buf.len)
    {
        buf = to_substr(*cont);
        ret = format(buf, fmt, args...);
        if(ret != buf.len)
        {
            cont->resize(ret);
        }
    }
}

/**
 * @overload formatrs
 * @ingroup formatting_functions */
template<class CharOwningContainer, class... Args>
inline CharOwningContainer formatrs(csubstr fmt, Args const&  C4_RESTRICT ...args)
{
    CharOwningContainer cont;
    formatrs(&cont, fmt, std::forward<Args>(args)...);
    return cont;
}

/** like format(), but receives a container, and appends the
 * arguments, resizing the container as needed to contain the
 * result. The buffer is appended to.
 * @return the region newly appended to the original container
 * @ingroup formatting_functions */
template<class CharOwningContainer, class... Args>
inline csubstr formatrs(append_t, CharOwningContainer * C4_RESTRICT cont, csubstr fmt, Args const& C4_RESTRICT ...args)
{
    const size_t pos = cont->size();
    substr buf = to_substr(*cont).sub(pos);
    size_t ret = format(buf, fmt, args...);
    cont->resize(pos + ret);
    if(ret > buf.len)
    {
        buf = to_substr(*cont).sub(pos);
        ret = format(buf, fmt, args...);
        if(ret != buf.len)
        {
            cont->resize(pos + ret);
        }
    }
    return to_csubstr(*cont).range(pos, cont->size());
}

} // namespace c4

#ifdef _MSC_VER
#   pragma warning(pop)
#endif

#endif /* _C4_FORMAT_HPP_ */
