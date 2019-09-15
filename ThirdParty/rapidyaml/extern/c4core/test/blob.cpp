#include "c4/test.hpp"
#include "c4/blob.hpp"

namespace c4 {

template<class T>
void test_blob()
{
    T v;
    blob b(&v);
    EXPECT_EQ((T*)b.buf, &v);
    EXPECT_EQ(b.len, sizeof(T));

    blob b2 = b;
    EXPECT_EQ((T*)b2.buf, &v);
    EXPECT_EQ(b2.len, sizeof(T));
}

TEST(blob, basic)
{
    test_blob<int>();
}

} // namespace c4
