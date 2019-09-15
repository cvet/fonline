// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "c4/test.hpp"

#ifdef C4CORE_HEADER_ONLY
#include "c4core.src.hpp"
#include "c4core.c4core-libtest.src.hpp"
#elif defined(C4CORE_SINGLE_HEADER)
#include "c4core.all.hpp"
#include "c4core.c4core-libtest.all.hpp"
#endif

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int stat = RUN_ALL_TESTS();
    return stat;
}
