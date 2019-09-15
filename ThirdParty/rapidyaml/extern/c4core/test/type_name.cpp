// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "c4/test.hpp"

#include "c4/type_name.hpp"

class  SomeTypeName {};
struct SomeStructName {};

C4_BEGIN_NAMESPACE(c4)

class  SomeTypeNameInsideANamespace {};
struct SomeStructNameInsideANamespace {};

template< size_t N >
cspan< char > cstr(const char (&s)[N])
{
    cspan< char > o(s, N-1);
    return o;
}

TEST(type_name, intrinsic_types)
{
    C4_EXPECT_EQ(type_name< int >(), cstr("int"));
    C4_EXPECT_EQ(type_name< float >(), cstr("float"));
    C4_EXPECT_EQ(type_name< double >(), cstr("double"));
}
TEST(type_name, classes)
{
    C4_EXPECT_EQ(type_name< SomeTypeName >(), cstr("SomeTypeName"));
    C4_EXPECT_EQ(type_name< ::SomeTypeName >(), cstr("SomeTypeName"));
}
TEST(type_name, structs)
{
    C4_EXPECT_EQ(type_name< SomeStructName >(), cstr("SomeStructName"));
    C4_EXPECT_EQ(type_name< ::SomeStructName >(), cstr("SomeStructName"));
}
TEST(type_name, inside_namespace)
{
    C4_EXPECT_EQ(type_name< SomeTypeNameInsideANamespace >(), cstr("c4::SomeTypeNameInsideANamespace"));
    C4_EXPECT_EQ(type_name< c4::SomeTypeNameInsideANamespace >(), cstr("c4::SomeTypeNameInsideANamespace"));
    C4_EXPECT_EQ(type_name< ::c4::SomeTypeNameInsideANamespace >(), cstr("c4::SomeTypeNameInsideANamespace"));

    C4_EXPECT_EQ(type_name< SomeStructNameInsideANamespace >(), cstr("c4::SomeStructNameInsideANamespace"));
    C4_EXPECT_EQ(type_name< c4::SomeStructNameInsideANamespace >(), cstr("c4::SomeStructNameInsideANamespace"));
    C4_EXPECT_EQ(type_name< ::c4::SomeStructNameInsideANamespace >(), cstr("c4::SomeStructNameInsideANamespace"));
}

C4_END_NAMESPACE(c4)
