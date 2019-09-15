#include "c4/test.hpp"
#include "c4/std/string.hpp"

namespace c4 {

TEST(std_string, to_substr)
{
    std::string s("barnabe");
    substr ss = to_substr(s);
    EXPECT_EQ(ss.str, s.data());
    EXPECT_EQ(ss.len, s.size());
    s[0] = 'B';
    EXPECT_EQ(ss[0], 'B');
    ss[0] = 'b';
    EXPECT_EQ(s[0], 'b');
}

TEST(std_string, to_csubstr)
{
    std::string s("barnabe");
    csubstr ss = to_csubstr(s);
    EXPECT_EQ(ss.str, s.data());
    EXPECT_EQ(ss.len, s.size());
    s[0] = 'B';
    EXPECT_EQ(ss[0], 'B');
}

} // namespace c4
