// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "c4/language.hpp"

C4_BEGIN_NAMESPACE(c4)
C4_BEGIN_NAMESPACE(detail)

#ifndef __GNUC__
void use_char_pointer(char const volatile* v)
{
    C4_UNUSED(v);
}
#endif

C4_END_NAMESPACE(detail)
C4_END_NAMESPACE(c4)
