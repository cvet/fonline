// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "c4/memory_util.hpp"

#include "c4/test.hpp"

C4_BEGIN_NAMESPACE(c4)

TEST(mem_repeatT, one_repetition)
{
    char buf[120] = {0};

    mem_repeat(buf, "123", 2, 1);
    EXPECT_EQ(strcmp(buf, "12"), 0);

    mem_repeat(buf, "123", 3, 1);
    EXPECT_EQ(strcmp(buf, "123"), 0);
}

TEST(mem_repeatT, basic)
{
    char buf[120] = {0};

    mem_zero(buf);

    mem_repeat(buf, "123", 2, 2);
    EXPECT_EQ(strcmp(buf, "1212"), 0);
    EXPECT_EQ(buf[4], '\0');

    mem_zero(buf);

    mem_repeat(buf, "123", 3, 2);
    EXPECT_EQ(strcmp(buf, "123123"), 0);
    EXPECT_EQ(buf[6], '\0');

    mem_zero(buf);

    mem_repeat(buf, "123", 2, 3);
    EXPECT_EQ(strcmp(buf, "121212"), 0);
    EXPECT_EQ(buf[6], '\0');

    mem_zero(buf, sizeof(buf));

    mem_repeat(buf, "123", 3, 3);
    EXPECT_EQ(strcmp(buf, "123123123"), 0);
    EXPECT_EQ(buf[9], '\0');

    mem_zero(buf, sizeof(buf));

    mem_repeat(buf, "123", 2, 4);
    EXPECT_EQ(strcmp(buf, "12121212"), 0);
    EXPECT_EQ(buf[8], '\0');

    mem_zero(buf, sizeof(buf));

    mem_repeat(buf, "123", 3, 4);
    EXPECT_EQ(strcmp(buf, "123123123123"), 0);
    EXPECT_EQ(buf[12], '\0');

    mem_zero(buf, sizeof(buf));

    mem_repeat(buf, "123", 2, 5);
    EXPECT_EQ(strcmp(buf, "1212121212"), 0);
    EXPECT_EQ(buf[10], '\0');

    mem_zero(buf, sizeof(buf));

    mem_repeat(buf, "123", 3, 5);
    EXPECT_EQ(strcmp(buf, "123123123123123"), 0);
    EXPECT_EQ(buf[15], '\0');

    mem_zero(buf, sizeof(buf));

    mem_repeat(buf, "123", 2, 6);
    EXPECT_EQ(strcmp(buf, "121212121212"), 0);
    EXPECT_EQ(buf[12], '\0');

    mem_zero(buf, sizeof(buf));

    mem_repeat(buf, "123", 3, 6);
    EXPECT_EQ(strcmp(buf, "123123123123123123"), 0);
    EXPECT_EQ(buf[18], '\0');
}


//-----------------------------------------------------------------------------

TEST(lsb, basic)
{
    EXPECT_EQ(lsb( 0), 0);
    EXPECT_EQ(lsb( 1), 0);
    EXPECT_EQ(lsb( 2), 1);
    EXPECT_EQ(lsb( 3), 0);
    EXPECT_EQ(lsb( 4), 2);
    EXPECT_EQ(lsb( 5), 0);
    EXPECT_EQ(lsb( 6), 1);
    EXPECT_EQ(lsb( 7), 0);
    EXPECT_EQ(lsb( 8), 3);
    EXPECT_EQ(lsb( 9), 0);
    EXPECT_EQ(lsb(10), 1);
    EXPECT_EQ(lsb(11), 0);
    EXPECT_EQ(lsb(12), 2);
    EXPECT_EQ(lsb(13), 0);
    EXPECT_EQ(lsb(14), 1);
    EXPECT_EQ(lsb(15), 0);
    EXPECT_EQ(lsb(16), 4);
}

TEST(lsb11, basic)
{
    //EXPECT_EQ((lsb11<int, 0>::value), 0);
    EXPECT_EQ((lsb11<int, 1>::value), 0);
    EXPECT_EQ((lsb11<int, 2>::value), 1);
    EXPECT_EQ((lsb11<int, 3>::value), 0);
    EXPECT_EQ((lsb11<int, 4>::value), 2);
    EXPECT_EQ((lsb11<int, 5>::value), 0);
    EXPECT_EQ((lsb11<int, 6>::value), 1);
    EXPECT_EQ((lsb11<int, 7>::value), 0);
    EXPECT_EQ((lsb11<int, 8>::value), 3);
    EXPECT_EQ((lsb11<int, 9>::value), 0);
    EXPECT_EQ((lsb11<int,10>::value), 1);
    EXPECT_EQ((lsb11<int,11>::value), 0);
    EXPECT_EQ((lsb11<int,12>::value), 2);
    EXPECT_EQ((lsb11<int,13>::value), 0);
    EXPECT_EQ((lsb11<int,14>::value), 1);
    EXPECT_EQ((lsb11<int,15>::value), 0);
    EXPECT_EQ((lsb11<int,16>::value), 4);
}


//-----------------------------------------------------------------------------

TEST(msb, basic)
{
    EXPECT_EQ(msb( 0), 0);
    EXPECT_EQ(msb( 1), 1);
    EXPECT_EQ(msb( 2), 2);
    EXPECT_EQ(msb( 3), 2);
    EXPECT_EQ(msb( 4), 3);
    EXPECT_EQ(msb( 5), 3);
    EXPECT_EQ(msb( 6), 3);
    EXPECT_EQ(msb( 7), 3);
    EXPECT_EQ(msb( 8), 4);
    EXPECT_EQ(msb( 9), 4);
    EXPECT_EQ(msb(10), 4);
    EXPECT_EQ(msb(11), 4);
    EXPECT_EQ(msb(12), 4);
    EXPECT_EQ(msb(13), 4);
    EXPECT_EQ(msb(14), 4);
    EXPECT_EQ(msb(15), 4);
    EXPECT_EQ(msb(16), 5);
}

TEST(msb11, basic)
{
    EXPECT_EQ((msb11<int, 0>::value), 0);
    EXPECT_EQ((msb11<int, 1>::value), 1);
    EXPECT_EQ((msb11<int, 2>::value), 2);
    EXPECT_EQ((msb11<int, 3>::value), 2);
    EXPECT_EQ((msb11<int, 4>::value), 3);
    EXPECT_EQ((msb11<int, 5>::value), 3);
    EXPECT_EQ((msb11<int, 6>::value), 3);
    EXPECT_EQ((msb11<int, 7>::value), 3);
    EXPECT_EQ((msb11<int, 8>::value), 4);
    EXPECT_EQ((msb11<int, 9>::value), 4);
    EXPECT_EQ((msb11<int,10>::value), 4);
    EXPECT_EQ((msb11<int,11>::value), 4);
    EXPECT_EQ((msb11<int,12>::value), 4);
    EXPECT_EQ((msb11<int,13>::value), 4);
    EXPECT_EQ((msb11<int,14>::value), 4);
    EXPECT_EQ((msb11<int,15>::value), 4);
    EXPECT_EQ((msb11<int,16>::value), 5);
}

//-----------------------------------------------------------------------------

template< size_t N > struct sz    { char buf[N]; };
template<          > struct sz<0> {              };
template< size_t F, size_t S > void check_tp()
{
    size_t expected;
    if(F == 0 && S == 0) expected = 1;
    else if(F == 0) expected = S;
    else if(S == 0) expected = F;
    else expected = F+S;

    EXPECT_EQ(sizeof(tight_pair<sz<F>, sz<S>>), expected) << "F=" << F << "  S=" << S;
}

TEST(tight_pair, basic)
{
    check_tp<0,0>();
    check_tp<0,1>();
    check_tp<0,2>();
    check_tp<0,3>();
    check_tp<0,4>();

    check_tp<0,0>();
    check_tp<1,0>();
    check_tp<2,0>();
    check_tp<3,0>();
    check_tp<4,0>();

    check_tp<0,0>();
    check_tp<1,1>();
    check_tp<2,2>();
    check_tp<3,3>();
    check_tp<4,4>();
}

C4_END_NAMESPACE(c4)
