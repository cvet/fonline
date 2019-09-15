#include <iostream>
#include <vector>

#include <c4/bitmask.hpp>
#include <c4/std/vector.hpp>

#include <c4/test.hpp>

#include "./enum_common.hpp"



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


TEST(str2bm, simple_bitmask)
{
    using namespace c4;
    std::vector<char> str;

    EXPECT_EQ(BM_NONE,              str2bm<MyBitmask>("BM_NONE"));
    EXPECT_EQ(BM_NONE,              str2bm<MyBitmask>("NONE"));
    EXPECT_EQ(BM_NONE,              str2bm<MyBitmask>("0"));
    EXPECT_EQ(BM_NONE,              str2bm<MyBitmask>("0x0"));

    EXPECT_EQ(BM_FOO,               str2bm<MyBitmask>("BM_FOO"));
    EXPECT_EQ(BM_FOO,               str2bm<MyBitmask>("FOO"));
    EXPECT_EQ(BM_FOO,               str2bm<MyBitmask>("1"));
    EXPECT_EQ(BM_FOO,               str2bm<MyBitmask>("0x1"));
    EXPECT_EQ(BM_FOO,               str2bm<MyBitmask>("BM_NONE|0x1"));

    EXPECT_EQ(BM_BAR,               str2bm<MyBitmask>("BM_BAR"));
    EXPECT_EQ(BM_BAR,               str2bm<MyBitmask>("BAR"));
    EXPECT_EQ(BM_BAR,               str2bm<MyBitmask>("2"));
    EXPECT_EQ(BM_BAR,               str2bm<MyBitmask>("0x2"));
    EXPECT_EQ(BM_BAR,               str2bm<MyBitmask>("BM_NONE|0x2"));

    EXPECT_EQ(BM_BAZ,               str2bm<MyBitmask>("BM_BAZ"));
    EXPECT_EQ(BM_BAZ,               str2bm<MyBitmask>("BAZ"));
    EXPECT_EQ(BM_BAZ,               str2bm<MyBitmask>("4"));
    EXPECT_EQ(BM_BAZ,               str2bm<MyBitmask>("0x4"));

    EXPECT_EQ(BM_FOO_BAR,           str2bm<MyBitmask>("BM_FOO|BM_BAR"));
    EXPECT_EQ(BM_FOO_BAR,           str2bm<MyBitmask>("BM_FOO|BAR"));
    EXPECT_EQ(BM_FOO_BAR,           str2bm<MyBitmask>("FOO|BM_BAR"));
    EXPECT_EQ(BM_FOO_BAR,           str2bm<MyBitmask>("BM_FOO_BAR"));
    EXPECT_EQ(BM_FOO_BAR,           str2bm<MyBitmask>("FOO_BAR"));
    EXPECT_EQ(BM_FOO_BAR,           str2bm<MyBitmask>("FOO|BAR"));
    EXPECT_EQ(BM_FOO_BAR,           str2bm<MyBitmask>("0x1|0x2"));
    EXPECT_EQ(BM_FOO_BAR,           str2bm<MyBitmask>("1|2"));
    EXPECT_EQ(BM_FOO_BAR,           str2bm<MyBitmask>("0x3"));
    EXPECT_EQ(BM_FOO_BAR,           str2bm<MyBitmask>("3"));

    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("BM_FOO|BM_BAR|BM_BAZ"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("BM_FOO|BM_BAR|BAZ"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("BM_FOO|BAR|BM_BAZ"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("FOO|BM_BAR|BM_BAZ"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("FOO|BM_BAR|BAZ"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("FOO|BAR|BM_BAZ"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("FOO|BAR|BAZ"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("FOO_BAR|BAZ"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("BM_FOO_BAR|BAZ"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("0x1|BAR|BAZ"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("FOO|0x2|BAZ"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("FOO|BAR|0x4"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("0x1|0x2|0x4"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("0x7"));
    EXPECT_EQ(BM_FOO_BAR_BAZ,       str2bm<MyBitmask>("7"));
}

TEST(str2bm, scoped_bitmask)
{
    using namespace c4;
    std::vector<char> str;
    using bmt = MyBitmaskClass;

    EXPECT_EQ(bmt::BM_NONE,        (bmt)str2bm<bmt>("MyBitmaskClass::BM_NONE"));
    EXPECT_EQ(bmt::BM_FOO,         (bmt)str2bm<bmt>("MyBitmaskClass::BM_FOO"));
    EXPECT_EQ(bmt::BM_BAR,         (bmt)str2bm<bmt>("MyBitmaskClass::BM_BAR"));
    EXPECT_EQ(bmt::BM_BAZ,         (bmt)str2bm<bmt>("MyBitmaskClass::BM_BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR,     (bmt)str2bm<bmt>("MyBitmaskClass::BM_FOO_BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ, (bmt)str2bm<bmt>("MyBitmaskClass::BM_FOO_BAR_BAZ"));
    EXPECT_EQ(bmt::BM_NONE,        (bmt)str2bm<bmt>("BM_NONE"));
    EXPECT_EQ(bmt::BM_FOO,         (bmt)str2bm<bmt>("BM_FOO"));
    EXPECT_EQ(bmt::BM_BAR,         (bmt)str2bm<bmt>("BM_BAR"));
    EXPECT_EQ(bmt::BM_BAZ,         (bmt)str2bm<bmt>("BM_BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR,     (bmt)str2bm<bmt>("BM_FOO_BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ, (bmt)str2bm<bmt>("BM_FOO_BAR_BAZ"));
    EXPECT_EQ(bmt::BM_NONE,        (bmt)str2bm<bmt>("NONE"));
    EXPECT_EQ(bmt::BM_FOO,         (bmt)str2bm<bmt>("FOO"));
    EXPECT_EQ(bmt::BM_BAR,         (bmt)str2bm<bmt>("BAR"));
    EXPECT_EQ(bmt::BM_BAZ,         (bmt)str2bm<bmt>("BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR,     (bmt)str2bm<bmt>("FOO_BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ, (bmt)str2bm<bmt>("FOO_BAR_BAZ"));

    EXPECT_EQ(bmt::BM_NONE,              (bmt)str2bm<bmt>("NONE"));
    EXPECT_EQ(bmt::BM_NONE,              (bmt)str2bm<bmt>("BM_NONE"));
    EXPECT_EQ(bmt::BM_NONE,              (bmt)str2bm<bmt>("MyBitmaskClass::BM_NONE"));
    EXPECT_EQ(bmt::BM_NONE,              (bmt)str2bm<bmt>("0"));
    EXPECT_EQ(bmt::BM_NONE,              (bmt)str2bm<bmt>("0x0"));
    EXPECT_EQ(bmt::BM_FOO,               (bmt)str2bm<bmt>("FOO"));
    EXPECT_EQ(bmt::BM_FOO,               (bmt)str2bm<bmt>("BM_FOO"));
    EXPECT_EQ(bmt::BM_FOO,               (bmt)str2bm<bmt>("MyBitmaskClass::BM_FOO"));
    EXPECT_EQ(bmt::BM_FOO,               (bmt)str2bm<bmt>("1"));
    EXPECT_EQ(bmt::BM_FOO,               (bmt)str2bm<bmt>("0x1"));
    EXPECT_EQ(bmt::BM_FOO,               (bmt)str2bm<bmt>("NONE|0x1"));
    EXPECT_EQ(bmt::BM_FOO,               (bmt)str2bm<bmt>("BM_NONE|0x1"));
    EXPECT_EQ(bmt::BM_FOO,               (bmt)str2bm<bmt>("MyBitmaskClass::BM_NONE|0x1"));
    EXPECT_EQ(bmt::BM_BAR,               (bmt)str2bm<bmt>("BAR"));
    EXPECT_EQ(bmt::BM_BAR,               (bmt)str2bm<bmt>("BM_BAR"));
    EXPECT_EQ(bmt::BM_BAR,               (bmt)str2bm<bmt>("MyBitmaskClass::BM_BAR"));
    EXPECT_EQ(bmt::BM_BAR,               (bmt)str2bm<bmt>("2"));
    EXPECT_EQ(bmt::BM_BAR,               (bmt)str2bm<bmt>("0x2"));
    EXPECT_EQ(bmt::BM_BAZ,               (bmt)str2bm<bmt>("BAZ"));
    EXPECT_EQ(bmt::BM_BAZ,               (bmt)str2bm<bmt>("BM_BAZ"));
    EXPECT_EQ(bmt::BM_BAZ,               (bmt)str2bm<bmt>("MyBitmaskClass::BM_BAZ"));
    EXPECT_EQ(bmt::BM_BAR,               (bmt)str2bm<bmt>("BM_NONE|0x2"));
    EXPECT_EQ(bmt::BM_BAR,               (bmt)str2bm<bmt>("MyBitmaskClass::BM_NONE|0x2"));
    EXPECT_EQ(bmt::BM_BAZ,               (bmt)str2bm<bmt>("4"));
    EXPECT_EQ(bmt::BM_BAZ,               (bmt)str2bm<bmt>("0x4"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("BM_FOO|BM_BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("MyBitmaskClass::BM_FOO|MyBitmaskClass::BM_BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("BM_FOO|BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("MyBitmaskClass::BM_FOO|BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("FOO|BM_BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("FOO|MyBitmaskClass::BM_BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("BM_FOO_BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("MyBitmaskClass::BM_FOO_BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("FOO_BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("FOO|BAR"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("0x1|0x2"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("1|2"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("0x3"));
    EXPECT_EQ(bmt::BM_FOO_BAR,           (bmt)str2bm<bmt>("3"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("BM_FOO|BM_BAR|BM_BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("MyBitmaskClass::BM_FOO|MyBitmaskClass::BM_BAR|MyBitmaskClass::BM_BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("BM_FOO|BM_BAR|BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("MyBitmaskClass::BM_FOO|MyBitmaskClass::BM_BAR|BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("BM_FOO|BAR|BM_BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("BM_FOO|BAR|MyBitmaskClass::BM_BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("FOO|BM_BAR|BM_BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("FOO|MyBitmaskClass::BM_BAR|MyBitmaskClass::BM_BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("FOO|BM_BAR|BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("FOO|MyBitmaskClass::BM_BAR|BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("FOO|BAR|BM_BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("FOO|BAR|MyBitmaskClass::BM_BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("FOO|BAR|BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("FOO_BAR|BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("MyBitmaskClass::BM_FOO_BAR|BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("0x1|BAR|BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("FOO|0x2|BAZ"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("FOO|BAR|0x4"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("0x1|0x2|0x4"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("0x7"));
    EXPECT_EQ(bmt::BM_FOO_BAR_BAZ,       (bmt)str2bm<bmt>("0x7"));
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//----------------------------------------------------------------------------

template< class Enum >
const char* do_bm2str(Enum e, std::vector<char> *s, c4::EnumOffsetType which)
{
    size_t len = c4::bm2str<Enum>(e, nullptr, 0, which);
    s->resize(len);
    c4::bm2str<Enum>(e, &((*s)[0]), s->size(), which);
    return s->data();
}

TEST(bm2str, simple_bitmask)
{
    using namespace c4;
    std::vector<char> str;

    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_NONE,        &str, EOFFS_NONE), "BM_NONE");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_FOO,         &str, EOFFS_NONE), "BM_FOO");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_BAR,         &str, EOFFS_NONE), "BM_BAR");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_BAZ,         &str, EOFFS_NONE), "BM_BAZ");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_FOO_BAR,     &str, EOFFS_NONE), "BM_FOO_BAR");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_FOO_BAR_BAZ, &str, EOFFS_NONE), "BM_FOO_BAR_BAZ");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_NONE,        &str, EOFFS_CLS ), "BM_NONE");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_FOO,         &str, EOFFS_CLS ), "BM_FOO");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_BAR,         &str, EOFFS_CLS ), "BM_BAR");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_BAZ,         &str, EOFFS_CLS ), "BM_BAZ");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_FOO_BAR,     &str, EOFFS_CLS ), "BM_FOO_BAR");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_FOO_BAR_BAZ, &str, EOFFS_CLS ), "BM_FOO_BAR_BAZ");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_NONE,        &str, EOFFS_PFX ), "NONE");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_FOO,         &str, EOFFS_PFX ), "FOO");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_BAR,         &str, EOFFS_PFX ), "BAR");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_BAZ,         &str, EOFFS_PFX ), "BAZ");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_FOO_BAR,     &str, EOFFS_PFX ), "FOO_BAR");
    EXPECT_STREQ(do_bm2str<MyBitmask>(BM_FOO_BAR_BAZ, &str, EOFFS_PFX ), "FOO_BAR_BAZ");
}

TEST(bm2str, scoped_bitmask)
{
    using namespace c4;
    std::vector<char> str;

    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_NONE,        &str, EOFFS_NONE), "MyBitmaskClass::BM_NONE");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_FOO,         &str, EOFFS_NONE), "MyBitmaskClass::BM_FOO");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_BAR,         &str, EOFFS_NONE), "MyBitmaskClass::BM_BAR");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_BAZ,         &str, EOFFS_NONE), "MyBitmaskClass::BM_BAZ");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_FOO_BAR,     &str, EOFFS_NONE), "MyBitmaskClass::BM_FOO_BAR");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_FOO_BAR_BAZ, &str, EOFFS_NONE), "MyBitmaskClass::BM_FOO_BAR_BAZ");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_NONE,        &str, EOFFS_CLS ), "BM_NONE");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_FOO,         &str, EOFFS_CLS ), "BM_FOO");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_BAR,         &str, EOFFS_CLS ), "BM_BAR");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_BAZ,         &str, EOFFS_CLS ), "BM_BAZ");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_FOO_BAR,     &str, EOFFS_CLS ), "BM_FOO_BAR");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_FOO_BAR_BAZ, &str, EOFFS_CLS ), "BM_FOO_BAR_BAZ");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_NONE,        &str, EOFFS_PFX ), "NONE");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_FOO,         &str, EOFFS_PFX ), "FOO");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_BAR,         &str, EOFFS_PFX ), "BAR");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_BAZ,         &str, EOFFS_PFX ), "BAZ");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_FOO_BAR,     &str, EOFFS_PFX ), "FOO_BAR");
    EXPECT_STREQ(do_bm2str<MyBitmaskClass>(MyBitmaskClass::BM_FOO_BAR_BAZ, &str, EOFFS_PFX ), "FOO_BAR_BAZ");
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//----------------------------------------------------------------------------

template< typename Enum >
void test_bm2str()
{
    using namespace c4;
    using I = typename std::underlying_type<Enum>::type;
    int combination_depth = 4;
    auto syms = esyms<Enum>();

    std::vector< int > indices;
    std::string str;
    std::vector< char > ws;
    I val = 0, res;
    size_t len;

    for(int k = 1; k <= combination_depth; ++k)
    {
        indices.clear();
        indices.resize(k);
        while(1)
        {
            str.clear();
            val = 0;
            for(auto i : indices)
            {
                if(!str.empty()) str += '|';
                str += syms[i].name;
                val |= static_cast< I >(syms[i].value);
                //printf("%d", i);
            }
            //len = bm2str<Enum>(val); // needed length
            //ws.resize(len);
            //bm2str<Enum>(val, &ws[0], len);
            //printf(": %s (%zu) %s\n", str.c_str(), (uint64_t)val, ws.data());

            res = str2bm<Enum>(str.data());
            EXPECT_EQ(res, val);

            len = bm2str<Enum>(res); // needed length
            ws.resize(len);
            bm2str<Enum>(val, &ws[0], len);
            res = str2bm<Enum>(ws.data());
            EXPECT_EQ(res, val);

            // write a string with the bitmask as an int
            c4::catrs(&ws, val);
            res = str2bm<Enum>(str.data());
            EXPECT_EQ(res, val);

            bool carry = true;
            for(int i = k-1; i >= 0; --i)
            {
                if(indices[i] + 1 < syms.size())
                {
                    ++indices[i];
                    carry = false;
                    break;
                }
                else
                {
                    indices[i] = 0;
                }
            }
            if(carry)
            {
                break;
            }
        } // while(1)
    } // for k
}
