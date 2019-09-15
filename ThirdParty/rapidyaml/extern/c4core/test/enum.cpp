#include <vector>

#include <c4/enum.hpp>
#include <c4/test.hpp>

#include "./enum_common.hpp"


TEST(eoffs, simple_enum)
{
    using namespace c4;
    EXPECT_EQ(eoffs_cls< MyEnum >(), 0);
    EXPECT_EQ(eoffs_pfx< MyEnum >(), 0);
}

TEST(eoffs, scoped_enum)
{
    using namespace c4;
    EXPECT_EQ(eoffs_cls< MyEnumClass >(), strlen("MyEnumClass::"));
    EXPECT_EQ(eoffs_pfx< MyEnumClass >(), 0);
}

TEST(eoffs, simple_bitmask)
{
    using namespace c4;
    EXPECT_EQ(eoffs_cls< MyBitmask >(), 0);
    EXPECT_EQ(eoffs_pfx< MyBitmask >(), strlen("BM_"));
}

TEST(eoffs, scoped_bitmask)
{
    using namespace c4;
    EXPECT_EQ(eoffs_cls< MyBitmaskClass >(), strlen("MyBitmaskClass::"));
    EXPECT_EQ(eoffs_pfx< MyBitmaskClass >(), strlen("MyBitmaskClass::BM_"));
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template<class Enum>
void test_esyms()
{
    auto ss = c4::esyms<Enum>();
    EXPECT_NE(ss.size(), 0);
    EXPECT_FALSE(ss.empty());
    for(auto s : ss)
    {
        EXPECT_STREQ(ss.find(s.name)->name, s.name);
        EXPECT_STREQ(ss.find(s.value)->name, s.name);
        EXPECT_EQ(ss.find(s.name)->value, s.value);
        EXPECT_EQ(ss.find(s.value)->value, s.value);
    }
}

TEST(esyms, simple_enum)
{
    test_esyms<MyEnum>();
}

TEST(esyms, scoped_enum)
{
    test_esyms<MyEnumClass>();
}

TEST(esyms, simple_bitmask)
{
    test_esyms<MyBitmask>();
}

TEST(esyms, scoped_bitmask)
{
    test_esyms<MyBitmaskClass>();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template< typename Enum >
void test_e2str()
{
    using namespace c4;
    using I = typename std::underlying_type<Enum>::type;
    auto ss = esyms<Enum>();
    EXPECT_NE(ss.size(), 0);
    EXPECT_FALSE(ss.empty());
    for(auto const& p : ss)
    {
        // test a round trip
        EXPECT_EQ((I)str2e<Enum>(e2str(p.value)), (I)p.value);
        // test the other way around
        EXPECT_STREQ(e2str(str2e<Enum>(p.name)), p.name);
    }
}


TEST(e2str, simple_enum)
{
    test_e2str<MyEnum>();
}

TEST(e2str, scoped_enum)
{
    test_e2str<MyEnumClass>();
    EXPECT_EQ(c4::str2e<MyEnumClass>("MyEnumClass::FOO"), MyEnumClass::FOO);
    EXPECT_EQ(c4::str2e<MyEnumClass>("FOO"), MyEnumClass::FOO);
}

TEST(e2str, simple_bitmask)
{
    test_e2str<MyBitmask>();
    EXPECT_EQ(c4::str2e<MyBitmask>("BM_FOO"), BM_FOO);
    EXPECT_EQ(c4::str2e<MyBitmask>("FOO"), BM_FOO);
}

TEST(e2str, scoped_bitmask)
{
    test_e2str<MyBitmaskClass>();
    EXPECT_EQ(c4::str2e<MyBitmaskClass>("MyBitmaskClass::BM_FOO"), MyBitmaskClass::BM_FOO);
    EXPECT_EQ(c4::str2e<MyBitmaskClass>("BM_FOO"), MyBitmaskClass::BM_FOO);
    EXPECT_EQ(c4::str2e<MyBitmaskClass>("FOO"), MyBitmaskClass::BM_FOO);
}

