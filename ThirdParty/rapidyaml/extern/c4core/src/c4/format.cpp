#include "c4/format.hpp"

namespace c4 {

size_t sprintf(substr buf, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    /* vsnprintf() returns number of characters written if successful or
     * negative value if an error occurred. If the resulting string gets
     * truncated due to buf_size limit, returns the total number of
     * characters (not including the terminating null-byte) which would have
     * been written, if the limit was not imposed.
     *
     * @see http://en.cppreference.com/w/cpp/io/c/vfprintf
     */
    int inum = ::vsnprintf(buf.str, buf.len, fmt, args);

    /* if the retval is negative, maybe there's a formatting error so it's
     * impossible to know the length of the needed string.
     *
     * So return a huge value to try to cause a mess to let the client know
     * early. */
    size_t snum = inum >= 0 ? (size_t)inum : (size_t)-1;

    va_end(args);

    return snum;
}

#ifdef C4_HMM_DONT_KNOW_ABOUT_THIS
/* @todo This is going to be a major headache. How can we reliably know the
 * number of characters read, given that the format string is user
 * defined?  Yes, we're forcing the user to provide %n as the last format
 * specifier, but how can we access the value?
 */
size_t sscanf(csubstr buf, const char *fmt, ...);
{
    va_list args;
    va_start(args, fmt);

    C4_ASSERT_MSG(to_csubstr(fmt).ends_with("%n"), "this function requires that %n is passed as the last argument");

    va_end(args);
}
#endif

} // namespace c4
