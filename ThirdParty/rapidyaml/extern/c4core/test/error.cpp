// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "c4/error.hpp"
#include "c4/test.hpp"

C4_BEGIN_HIDDEN_NAMESPACE
bool got_an_error = false;
void error_callback(const char *msg, size_t msg_sz)
{
    EXPECT_EQ(strncmp(msg, "bla bla", msg_sz), 0);
    EXPECT_EQ(msg_sz, 7);
    got_an_error = true;
}
inline c4::ScopedErrorSettings tmp_err()
{
    got_an_error = false;
    return c4::ScopedErrorSettings(c4::ON_ERROR_CALLBACK, error_callback);
}
C4_END_HIDDEN_NAMESPACE

C4_BEGIN_NAMESPACE(c4)

TEST(Error, scoped_callback)
{
    auto orig = get_error_callback();
    {
        auto tmp = tmp_err();
        EXPECT_EQ(get_error_callback() == error_callback, true);
        C4_ERROR("bla bla");
        EXPECT_EQ(got_an_error, true);
    }
    EXPECT_EQ(get_error_callback() == orig, true);
}

C4_END_NAMESPACE(c4)

TEST(Error, outside_of_c4_namespace)
{
    auto orig = c4::get_error_callback();
    {
        auto tmp = tmp_err();
        EXPECT_EQ(c4::get_error_callback() == error_callback, true);
        C4_ERROR("bla bla");
        EXPECT_EQ(got_an_error, true);
    }
    EXPECT_EQ(c4::get_error_callback() == orig, true);
}
