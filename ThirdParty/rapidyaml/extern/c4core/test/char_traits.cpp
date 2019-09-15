// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "c4/char_traits.hpp"

#include "c4/test.hpp"

C4_BEGIN_NAMESPACE(c4)

TEST(num_needed_chars, char)
{
    EXPECT_EQ(num_needed_chars< char >(0), 0);
    EXPECT_EQ(num_needed_chars< char >(1), 1);
    EXPECT_EQ(num_needed_chars< char >(2), 2);
    EXPECT_EQ(num_needed_chars< char >(3), 3);
    EXPECT_EQ(num_needed_chars< char >(4), 4);
    for(int i = 0; i < 100; ++i)
    {
        EXPECT_EQ(num_needed_chars< char >(i), i);
    }
}

TEST(num_needed_chars, wchar_t)
{
    if(sizeof(wchar_t) == 2)
    {
        EXPECT_EQ(num_needed_chars< wchar_t >(  0),  0);
        EXPECT_EQ(num_needed_chars< wchar_t >(  1),  1);
        EXPECT_EQ(num_needed_chars< wchar_t >(  2),  1);
        EXPECT_EQ(num_needed_chars< wchar_t >(  3),  2);
        EXPECT_EQ(num_needed_chars< wchar_t >(  4),  2);
        EXPECT_EQ(num_needed_chars< wchar_t >( 97), 49);
        EXPECT_EQ(num_needed_chars< wchar_t >( 98), 49);
        EXPECT_EQ(num_needed_chars< wchar_t >( 99), 50);
        EXPECT_EQ(num_needed_chars< wchar_t >(100), 50);
        EXPECT_EQ(num_needed_chars< wchar_t >(101), 51);
    }
    else if(sizeof(wchar_t) == 4)
    {
        EXPECT_EQ(num_needed_chars< wchar_t >(  0),  0);
        EXPECT_EQ(num_needed_chars< wchar_t >(  1),  1);
        EXPECT_EQ(num_needed_chars< wchar_t >(  2),  1);
        EXPECT_EQ(num_needed_chars< wchar_t >(  3),  1);
        EXPECT_EQ(num_needed_chars< wchar_t >(  4),  1);
        EXPECT_EQ(num_needed_chars< wchar_t >(  5),  2);
        EXPECT_EQ(num_needed_chars< wchar_t >(  6),  2);
        EXPECT_EQ(num_needed_chars< wchar_t >(  7),  2);
        EXPECT_EQ(num_needed_chars< wchar_t >(  8),  2);
        EXPECT_EQ(num_needed_chars< wchar_t >( 93), 24);
        EXPECT_EQ(num_needed_chars< wchar_t >( 94), 24);
        EXPECT_EQ(num_needed_chars< wchar_t >( 95), 24);
        EXPECT_EQ(num_needed_chars< wchar_t >( 96), 24);
        EXPECT_EQ(num_needed_chars< wchar_t >( 97), 25);
        EXPECT_EQ(num_needed_chars< wchar_t >( 98), 25);
        EXPECT_EQ(num_needed_chars< wchar_t >( 99), 25);
        EXPECT_EQ(num_needed_chars< wchar_t >(100), 25);
        EXPECT_EQ(num_needed_chars< wchar_t >(101), 26);
    }
}

C4_END_NAMESPACE(c4)
