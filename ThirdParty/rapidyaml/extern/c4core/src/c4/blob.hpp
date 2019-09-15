#ifndef _C4_BLOB_HPP_
#define _C4_BLOB_HPP_

#include "c4/types.hpp"

/** @file blob.hpp Mutable and immutable binary data blobs.
*/

C4_BEGIN_NAMESPACE(c4)

template<class T> struct blob_;

/** an immutable binary blob */
using cblob = blob_<cbyte>;
/** a mutable binary blob */
using  blob = blob_< byte>;

template<class T>
struct blob_
{
    T *    buf;
    size_t len;

    C4_DEFAULT_COPY_AND_MOVE(blob_);

    C4_ALWAYS_INLINE constexpr blob_() noexcept : buf(), len() {}

    template<class U>
    C4_ALWAYS_INLINE constexpr blob_(U *ptr, size_t n=1) noexcept : buf(reinterpret_cast<T*>(ptr)), len(sizeof(U) * n) {}

    template<class U, size_t N>
    C4_ALWAYS_INLINE constexpr blob_(U (&arr)[N]) noexcept : buf(reinterpret_cast<T*>(arr)), len(sizeof(U) * N) {}

    template<size_t N>
    C4_ALWAYS_INLINE constexpr blob_(const char (&arr)[N]) noexcept : buf(reinterpret_cast<T*>(arr)), len(N-1) {}
    
    C4_ALWAYS_INLINE constexpr blob_(void       *ptr, size_t n) noexcept : buf(reinterpret_cast<T*>(ptr)), len(n) {}
    C4_ALWAYS_INLINE constexpr blob_(void const *ptr, size_t n) noexcept : buf(reinterpret_cast<T*>(ptr)), len(n) {}
};
C4_MUST_BE_TRIVIAL_COPY(blob);
C4_MUST_BE_TRIVIAL_COPY(cblob);


C4_END_NAMESPACE(c4)

#endif // _C4_BLOB_HPP_
