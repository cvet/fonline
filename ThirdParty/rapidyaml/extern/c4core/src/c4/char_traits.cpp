// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "c4/char_traits.hpp"

C4_BEGIN_NAMESPACE(c4)

constexpr const char char_traits< char >::whitespace_chars[];
constexpr const size_t char_traits< char >::num_whitespace_chars;
constexpr const wchar_t char_traits< wchar_t >::whitespace_chars[];
constexpr const size_t char_traits< wchar_t >::num_whitespace_chars;

C4_END_NAMESPACE(c4)
