// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "c4/span.hpp"

#include "c4/libtest/supprwarn_push.hpp"
#include "c4/test.hpp"

C4_BEGIN_NAMESPACE(c4)

//-----------------------------------------------------------------------------
TEST(span, default_init)
{
    span< int > s;
    EXPECT_EQ(s.size(), 0);
    EXPECT_EQ(s.capacity(), 0);
    EXPECT_EQ(s.data(), nullptr);
}

TEST(spanrs, default_init)
{
    spanrs< int > s;
    EXPECT_EQ(s.size(), 0);
    EXPECT_EQ(s.capacity(), 0);
    EXPECT_EQ(s.data(), nullptr);
}

TEST(spanrsl, default_init)
{
    spanrsl< int > s;
    EXPECT_EQ(s.size(), 0);
    EXPECT_EQ(s.capacity(), 0);
    EXPECT_EQ(s.data(), nullptr);
}

//-----------------------------------------------------------------------------
TEST(span, empty_init)
{
    int arr[10];
    span< int > s(arr, 0);
    EXPECT_EQ(s.size(), 0);
    EXPECT_EQ(s.capacity(), 0);
    EXPECT_NE(s.data(), nullptr);
}

TEST(spanrs, empty_init)
{
    int arr[10];

    {
        spanrs< int > s(arr, 0);
        EXPECT_EQ(s.size(), 0);
        EXPECT_EQ(s.capacity(), 0);
        EXPECT_EQ(s.data(), arr);
    }

    {
        spanrs< int > s(arr, 0, C4_COUNTOF(arr));
        EXPECT_EQ(s.size(), 0);
        EXPECT_EQ(s.capacity(), 10);
        EXPECT_EQ(s.data(), arr);
    }
}

TEST(spanrsl, empty_init)
{
    int arr[10];

    {
        spanrsl< int > s(arr, 0);
        EXPECT_EQ(s.size(), 0);
        EXPECT_EQ(s.capacity(), 0);
        EXPECT_EQ(s.data(), arr);
        EXPECT_EQ(s.offset(), 0);
    }

    {
        spanrsl< int > s(arr, 0, C4_COUNTOF(arr));
        EXPECT_EQ(s.size(), 0);
        EXPECT_EQ(s.capacity(), 10);
        EXPECT_EQ(s.data(), arr);
        EXPECT_EQ(s.offset(), 0);
    }
}

//-----------------------------------------------------------------------------
template< class SpanClass >
void test_fromArray()
{
    using T = typename SpanClass::value_type;
    T arr1[10];
    T arr2[20];

    T a = 0;
    for(auto &v : arr1) { v = a; a += T(1); }
    for(auto &v : arr2) { v = a; a += T(1); }

    {
        SpanClass s(arr1);
        EXPECT_EQ(s.size(), C4_COUNTOF(arr1));
        EXPECT_EQ(s.capacity(), C4_COUNTOF(arr1));
        EXPECT_EQ(s.data(), arr1);
    }

    {
        SpanClass s = arr1;
        EXPECT_EQ(s.size(), C4_COUNTOF(arr1));
        EXPECT_EQ(s.capacity(), C4_COUNTOF(arr1));
        EXPECT_EQ(s.data(), arr1);
    }

    {
        SpanClass s = arr1;
        EXPECT_EQ(s.size(), C4_COUNTOF(arr1));
        EXPECT_EQ(s.capacity(), C4_COUNTOF(arr1));
        EXPECT_EQ(s.data(), arr1);
        s = arr2;
        EXPECT_EQ(s.size(), C4_COUNTOF(arr2));
        EXPECT_EQ(s.capacity(), C4_COUNTOF(arr2));
        EXPECT_EQ(s.data(), arr2);
    }
}

TEST(span, fromArray)
{
    test_fromArray< span<char> >();
    test_fromArray< span<int> >();
    test_fromArray< span<uint32_t> >();
}

TEST(spanrs, fromArray)
{
    test_fromArray< spanrs<char> >();
    test_fromArray< spanrs<int> >();
    test_fromArray< spanrs<uint32_t> >();
}

TEST(spanrsl, fromArray)
{
    test_fromArray< spanrsl<char> >();
    test_fromArray< spanrsl<int> >();
    test_fromArray< spanrsl<uint32_t> >();
}

//-----------------------------------------------------------------------------
TEST(span, subspan)
{
    int arr[10];
    span< int > s(arr);
    C4_STATIC_ASSERT((std::is_same< decltype(s.subspan(0)), decltype(s) >::value));

    {
        auto ss = s.subspan(0, 5);
        EXPECT_EQ(ss.size(), 5);
        EXPECT_EQ(ss.capacity(), 5);
        EXPECT_EQ(ss.data(), arr);

        ss = s.subspan(5);
        EXPECT_EQ(ss.size(), 5);
        EXPECT_EQ(ss.capacity(), 5);
        EXPECT_EQ(ss.data(), &arr[5]);
    }
    {
        int buf10[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        int buf_5[]  = {-1, 0, 1, 2, 3, 4};
        int *buf5 = buf_5 + 1; // to make sure that one does not immediately follow the other in memory

        span<int> n(buf10);
        span<int> m(buf5, 5);

        auto ss = n.subspan(0);
        EXPECT_EQ(ss.data(), buf10);
        EXPECT_EQ(ss.size(), 10);
        ss = m.subspan(0);
        EXPECT_EQ(ss.data(), buf5);
        EXPECT_EQ(ss.size(), 5);
        ss = n.subspan(0, 0);
        EXPECT_NE(ss.data(), nullptr);
        EXPECT_EQ(ss.data(), &buf10[0]);
        EXPECT_EQ(ss.size(), 0);
        ss = m.subspan(0, 0);
        EXPECT_NE(ss.data(), nullptr);
        EXPECT_EQ(ss.data(), &buf5[0]);
        EXPECT_EQ(ss.size(), 0);
    }
}
TEST(spanrs, subspan)
{
    int arr[10];
    spanrs< int > s(arr);
    C4_STATIC_ASSERT((std::is_same< decltype(s.subspan(0)), decltype(s) >::value));

    auto ss = s.subspan(0, 5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);

    ss = s.subspan(5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), &arr[5]);
}
TEST(spanrsl, subspan)
{
    int arr[10];
    spanrsl< int > s(arr);
    C4_STATIC_ASSERT((std::is_same< decltype(s.subspan(0)), decltype(s) >::value));

    auto ss = s.subspan(0, 5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);
    EXPECT_EQ(ss.offset(), 0);

    ss = ss.original();
    EXPECT_EQ(ss.size(), 10);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);
    EXPECT_EQ(ss.offset(), 0);

    ss = s.subspan(5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), &arr[5]);
    EXPECT_EQ(ss.offset(), 5);

    ss = ss.original();
    EXPECT_EQ(ss.size(), 10);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);
    EXPECT_EQ(ss.offset(), 0);
}

//-----------------------------------------------------------------------------
TEST(span, range)
{
    int arr[10];
    span< int > s(arr);
    C4_STATIC_ASSERT((std::is_same< decltype(s.range(0)), decltype(s) >::value));

    auto ss = s.range(0, 5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), arr);

    ss = s.range(5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), &arr[5]);

    ss = s.range(5, 10);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), &arr[5]);
}
TEST(spanrs, range)
{
    int arr[10];
    spanrs< int > s(arr);
    C4_STATIC_ASSERT((std::is_same< decltype(s.range(0)), decltype(s) >::value));

    auto ss = s.range(0, 5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);

    ss = s.range(5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), &arr[5]);

    ss = s.range(5, 10);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), &arr[5]);
}
TEST(spanrsl, range)
{
    int arr[10];
    spanrsl< int > s(arr);
    C4_STATIC_ASSERT((std::is_same< decltype(s.range(0)), decltype(s) >::value));

    auto ss = s.range(0, 5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);

    ss = ss.original();
    EXPECT_EQ(ss.size(), 10);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);

    ss = s.range(5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), &arr[5]);

    ss = s.range(5, 10);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), &arr[5]);

    ss = ss.original();
    EXPECT_EQ(ss.size(), 10);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);
}

//-----------------------------------------------------------------------------
TEST(span, first)
{
    int arr[10];
    span< int > s(arr);
    C4_STATIC_ASSERT((std::is_same< decltype(s.first(1)), decltype(s) >::value));

    auto ss = s.first(0);
    EXPECT_EQ(ss.size(), 0);
    EXPECT_EQ(ss.capacity(), 0);
    EXPECT_EQ(ss.data(), arr);

    ss = s.first(5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), arr);
}
TEST(spanrs, first)
{
    int arr[10];
    spanrs< int > s(arr);
    C4_STATIC_ASSERT((std::is_same< decltype(s.first(1)), decltype(s) >::value));

    auto ss = s.first(0);
    EXPECT_EQ(ss.size(), 0);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);

    ss = s.first(5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);
}
TEST(spanrsl, first)
{
    int arr[10];
    spanrsl< int > s(arr);
    C4_STATIC_ASSERT((std::is_same< decltype(s.first(1)), decltype(s) >::value));

    auto ss = s.first(0);
    EXPECT_EQ(ss.size(), 0);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);

    ss = ss.original();
    EXPECT_EQ(ss.size(), 10);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);

    ss = s.first(5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);

    ss = ss.original();
    EXPECT_EQ(ss.size(), 10);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);
}

//-----------------------------------------------------------------------------
TEST(span, last)
{
    int arr[10];
    span< int > s(arr);
    C4_STATIC_ASSERT((std::is_same< decltype(s.last(1)), decltype(s) >::value));

    auto ss = s.last(0);
    EXPECT_EQ(ss.size(), 0);
    EXPECT_EQ(ss.capacity(), 0);
    EXPECT_EQ(ss.data(), arr + s.size());

    ss = s.last(5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), arr + 5);
}
TEST(spanrs, last)
{
    int arr[10];
    spanrs< int > s(arr);
    C4_STATIC_ASSERT((std::is_same< decltype(s.last(1)), decltype(s) >::value));

    auto ss = s.last(0);
    EXPECT_EQ(ss.size(), 0);
    EXPECT_EQ(ss.capacity(), 0);
    EXPECT_EQ(ss.data(), arr + s.size());

    ss = s.last(5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), arr + 5);
}
TEST(spanrsl, last)
{
    int arr[10];
    spanrsl< int > s(arr);
    C4_STATIC_ASSERT((std::is_same< decltype(s.last(1)), decltype(s) >::value));

    auto ss = s.last(0);
    EXPECT_EQ(ss.size(), 0);
    EXPECT_EQ(ss.capacity(), 0);
    EXPECT_EQ(ss.data(), arr + s.size());

    ss = ss.original();
    EXPECT_EQ(ss.size(), 10);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);

    ss = s.last(5);
    EXPECT_EQ(ss.size(), 5);
    EXPECT_EQ(ss.capacity(), 5);
    EXPECT_EQ(ss.data(), arr + 5);

    ss = ss.original();
    EXPECT_EQ(ss.size(), 10);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);
}

//-----------------------------------------------------------------------------
template< typename S >
void test_span_is_subspan()
{
    int buf10[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int buf_5[]  = {-1, 0, 1, 2, 3, 4};
    int *buf5 = buf_5 + 1; // to make sure that one does not immediately follow the other in memory

    S n(buf10);
    S m(buf5, 5);

    EXPECT_EQ(n.data(), buf10);
    EXPECT_EQ(m.data(), buf5);

    EXPECT_TRUE(n.is_subspan(n.subspan(0   )));
    EXPECT_TRUE(n.is_subspan(n.subspan(0, 3)));
    EXPECT_TRUE(n.is_subspan(n.subspan(0, 0)));

    EXPECT_FALSE(n.is_subspan(m.subspan(0   )));
    EXPECT_FALSE(n.is_subspan(m.subspan(0, 3)));
    EXPECT_FALSE(n.is_subspan(m.subspan(0, 0)));
}
TEST(span, is_subspan)
{
    SCOPED_TRACE("span.is_subspan");
    test_span_is_subspan< span<int> >();
}
TEST(spanrs, is_subspan)
{
    SCOPED_TRACE("spanrs.is_subspan");
    test_span_is_subspan< spanrs<int> >();
}
TEST(spanrsl, is_subspan)
{
    SCOPED_TRACE("spanrsl.is_subspan");
    test_span_is_subspan< spanrsl<int> >();
}

//-----------------------------------------------------------------------------
template< typename S >
void test_span_compll()
{
    int buf10[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    S n(buf10);

    EXPECT_EQ(n.compll(n.subspan(0)), n.subspan(0, 0));
    EXPECT_EQ(n.is_subspan(n.compll(n.subspan(0))), true);
    EXPECT_EQ(n.compll(n.subspan(0, 0)), n.subspan(0, 0));
    EXPECT_EQ(n.is_subspan(n.compll(n.subspan(0, 0))), true);

    EXPECT_EQ(n.compll(n.subspan(0, 1)), n.subspan(0, 0));
    EXPECT_EQ(n.compll(n.subspan(0, 3)), n.subspan(0, 0));

    EXPECT_EQ(n.compll(n.range(5, 10)), n.subspan(0, 5));
    EXPECT_EQ(n.compll(n.range(5, 5)),  n.subspan(0, 5));

    EXPECT_EQ(n.compll(n.subspan(n.size(), 0)), n);
    EXPECT_EQ(n.compll(n.range(n.size(), n.size())), n);
}
TEST(span, compll)
{
    SCOPED_TRACE("span.compll");
    test_span_compll< span<int> >();
}
TEST(spanrs, compll)
{
    SCOPED_TRACE("spanrs.compll");
    test_span_compll< spanrs<int> >();
}
TEST(spanrsl, compll)
{
    SCOPED_TRACE("spanrsl.compll");
    test_span_compll< spanrsl<int> >();
}


//-----------------------------------------------------------------------------

template< typename S >
void test_span_complr()
{
    int buf10[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    S n(buf10);

    EXPECT_EQ(n.complr(n.subspan(0)), n.subspan(0, 0));
    EXPECT_EQ(n.is_subspan(n.complr(n.subspan(0))), true);
    EXPECT_EQ(n.complr(n.subspan(0, 0)), n.subspan(0));
    EXPECT_EQ(n.is_subspan(n.complr(n.subspan(0, 0))), true);

    EXPECT_EQ(n.complr(n.subspan(0, 1)), n.subspan(1));
    EXPECT_EQ(n.complr(n.subspan(0, 3)), n.subspan(3));

    EXPECT_EQ(n.complr(n.subspan(5)), n.subspan(0, 0));
    EXPECT_EQ(n.complr(n.range(5, 10)), n.subspan(0, 0));

    EXPECT_EQ(n.complr(n.subspan(5, 0)), n.subspan(5));
    EXPECT_EQ(n.complr(n.range(5, 5)), n.subspan(5));

    EXPECT_EQ(n.complr(n.subspan(0, 0)), n);
    EXPECT_EQ(n.complr(n.range(0, 0)), n);
}
TEST(span, complr)
{
    SCOPED_TRACE("span.compll");
    test_span_complr< span<int> >();
}
TEST(spanrs, complr)
{
    SCOPED_TRACE("spanrs.complr");
    test_span_complr< spanrs<int> >();
}
TEST(spanrsl, complr)
{
    SCOPED_TRACE("spanrsl.complr");
    test_span_complr< spanrsl<int> >();
}



//-----------------------------------------------------------------------------
TEST(span, rtrim)
{
    int arr[10];
    span< int > s(arr);
    auto ss = s;

    ss.rtrim(0);
    EXPECT_EQ(ss.size(), s.size());
    EXPECT_EQ(ss.capacity(), s.capacity());
    EXPECT_EQ(ss.data(), arr);

    ss.rtrim(5);
    EXPECT_EQ(ss.size(), s.size() - 5);
    EXPECT_EQ(ss.capacity(), s.capacity() - 5);
    EXPECT_EQ(ss.data(), arr);
}
TEST(spanrs, rtrim)
{
    int arr[10];
    spanrs< int > s(arr);
    auto ss = s;

    ss.rtrim(0);
    EXPECT_EQ(ss.size(), s.size());
    EXPECT_EQ(ss.capacity(), s.capacity());
    EXPECT_EQ(ss.data(), arr);

    ss.rtrim(5);
    EXPECT_EQ(ss.size(), s.size() - 5);
    EXPECT_EQ(ss.capacity(), s.capacity());
    EXPECT_EQ(ss.data(), arr);
}
TEST(spanrsl, rtrim)
{
    int arr[10];
    spanrsl< int > s(arr);
    auto ss = s;

    ss.rtrim(0);
    EXPECT_EQ(ss.size(), s.size());
    EXPECT_EQ(ss.capacity(), s.capacity());
    EXPECT_EQ(ss.data(), arr);

    ss = ss.original();
    EXPECT_EQ(ss.size(), 10);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);

    ss.rtrim(5);
    EXPECT_EQ(ss.size(), s.size() - 5);
    EXPECT_EQ(ss.capacity(), s.capacity());
    EXPECT_EQ(ss.data(), arr);

    ss = ss.original();
    EXPECT_EQ(ss.size(), 10);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);
}

//-----------------------------------------------------------------------------
TEST(span, ltrim)
{
    int arr[10];
    span< int > s(arr);
    auto ss = s;

    ss.ltrim(0);
    EXPECT_EQ(ss.size(), s.size());
    EXPECT_EQ(ss.capacity(), s.capacity());
    EXPECT_EQ(ss.data(), arr);

    ss.ltrim(5);
    EXPECT_EQ(ss.size(), s.size() - 5);
    EXPECT_EQ(ss.capacity(), s.capacity() - 5);
    EXPECT_EQ(ss.data(), arr + 5);
}
TEST(spanrs, ltrim)
{
    int arr[10];
    spanrs< int > s(arr);
    auto ss = s;

    ss.ltrim(0);
    EXPECT_EQ(ss.size(), s.size());
    EXPECT_EQ(ss.capacity(), ss.capacity());
    EXPECT_EQ(ss.data(), arr);

    ss.ltrim(5);
    EXPECT_EQ(ss.size(), s.size() - 5);
    EXPECT_EQ(ss.capacity(), s.size() - 5);
    EXPECT_EQ(ss.data(), arr + 5);
}
TEST(spanrsl, ltrim)
{
    int arr[10];
    spanrsl< int > s(arr);
    auto ss = s;

    ss.ltrim(0);
    EXPECT_EQ(ss.size(), s.size());
    EXPECT_EQ(ss.capacity(), ss.capacity());
    EXPECT_EQ(ss.data(), arr);

    ss = ss.original();
    EXPECT_EQ(ss.size(), 10);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);

    ss.ltrim(5);
    EXPECT_EQ(ss.size(), s.size() - 5);
    EXPECT_EQ(ss.capacity(), s.size() - 5);
    EXPECT_EQ(ss.data(), arr + 5);

    ss = ss.original();
    EXPECT_EQ(ss.size(), 10);
    EXPECT_EQ(ss.capacity(), 10);
    EXPECT_EQ(ss.data(), arr);
}

//-----------------------------------------------------------------------------
const char larrc[11] = "0123456789";
const char rarrc[11] = "1234567890";
const int larri[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
const int rarri[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
TEST(span, reverse_iter)
{
    cspan< int > s(larri);
    using rit = cspan< int >::const_reverse_iterator;
    int pos = szconv< int >(s.size()) - 1;
    for(rit b = s.rbegin(), e = s.rend(); b != e; ++b)
    {
        EXPECT_EQ(*b, s[pos--]);
    }
        EXPECT_EQ(pos, -1);
}

//-----------------------------------------------------------------------------
TEST(span_impl, eq)
{
    EXPECT_EQ(cspan  <char>(larrc), cspan  <char>(larrc));
    EXPECT_EQ(cspanrs<char>(larrc), cspan  <char>(larrc));
    EXPECT_EQ(cspan  <char>(larrc), cspanrs<char>(larrc));
    EXPECT_EQ(cspanrs<char>(larrc), cspanrs<char>(larrc));

    EXPECT_EQ(cspan  <int>(larri), cspan  <int>(larri));
    EXPECT_EQ(cspanrs<int>(larri), cspan  <int>(larri));
    EXPECT_EQ(cspan  <int>(larri), cspanrs<int>(larri));
    EXPECT_EQ(cspanrs<int>(larri), cspanrs<int>(larri));
}

TEST(span_impl, lt)
{
    EXPECT_LT(cspan  <char>(larrc), cspan  <char>(rarrc));
    EXPECT_LT(cspanrs<char>(larrc), cspan  <char>(rarrc));
    EXPECT_LT(cspan  <char>(larrc), cspanrs<char>(rarrc));
    EXPECT_LT(cspanrs<char>(larrc), cspanrs<char>(rarrc));

    EXPECT_LT(cspan  <int>(larri), cspan  <int>(rarri));
    EXPECT_LT(cspanrs<int>(larri), cspan  <int>(rarri));
    EXPECT_LT(cspan  <int>(larri), cspanrs<int>(rarri));
    EXPECT_LT(cspanrs<int>(larri), cspanrs<int>(rarri));
}
TEST(span_impl, gt)
{
    EXPECT_GT(cspan  <char>(rarrc), cspan  <char>(larrc));
    EXPECT_GT(cspan  <char>(rarrc), cspanrs<char>(larrc));
    EXPECT_GT(cspanrs<char>(rarrc), cspan  <char>(larrc));
    EXPECT_GT(cspanrs<char>(rarrc), cspanrs<char>(larrc));

    EXPECT_GT(cspan  <int>(rarri), cspan  <int>(larri));
    EXPECT_GT(cspan  <int>(rarri), cspanrs<int>(larri));
    EXPECT_GT(cspanrs<int>(rarri), cspan  <int>(larri));
    EXPECT_GT(cspanrs<int>(rarri), cspanrs<int>(larri));
}

TEST(span_impl, ge)
{
    EXPECT_GE(cspan  <char>(rarrc), cspan  <char>(larrc));
    EXPECT_GE(cspan  <char>(rarrc), cspanrs<char>(larrc));
    EXPECT_GE(cspanrs<char>(rarrc), cspan  <char>(larrc));
    EXPECT_GE(cspanrs<char>(rarrc), cspanrs<char>(larrc));
    EXPECT_GE(cspan  <char>(larrc), cspan  <char>(larrc));
    EXPECT_GE(cspan  <char>(larrc), cspanrs<char>(larrc));
    EXPECT_GE(cspanrs<char>(larrc), cspan  <char>(larrc));
    EXPECT_GE(cspanrs<char>(larrc), cspanrs<char>(larrc));
    EXPECT_GE(cspan  <int >(rarri), cspan  <int >(larri));
    EXPECT_GE(cspan  <int >(rarri), cspanrs<int >(larri));
    EXPECT_GE(cspanrs<int >(rarri), cspan  <int >(larri));
    EXPECT_GE(cspanrs<int >(rarri), cspanrs<int >(larri));
    EXPECT_GE(cspan  <int >(larri), cspan  <int >(larri));
    EXPECT_GE(cspan  <int >(larri), cspanrs<int >(larri));
    EXPECT_GE(cspanrs<int >(larri), cspan  <int >(larri));
    EXPECT_GE(cspanrs<int >(larri), cspanrs<int >(larri));
}
TEST(span_impl, le)
{
    EXPECT_LE(cspan  <char>(larrc), cspan  <char>(rarrc));
    EXPECT_LE(cspanrs<char>(larrc), cspan  <char>(rarrc));
    EXPECT_LE(cspan  <char>(larrc), cspanrs<char>(rarrc));
    EXPECT_LE(cspanrs<char>(larrc), cspanrs<char>(rarrc));
    EXPECT_LE(cspan  <char>(larrc), cspan  <char>(larrc));
    EXPECT_LE(cspanrs<char>(larrc), cspan  <char>(larrc));
    EXPECT_LE(cspan  <char>(larrc), cspanrs<char>(larrc));
    EXPECT_LE(cspanrs<char>(larrc), cspanrs<char>(larrc));
    EXPECT_LE(cspan  <int >(larri), cspan  <int >(rarri));
    EXPECT_LE(cspanrs<int >(larri), cspan  <int >(rarri));
    EXPECT_LE(cspan  <int >(larri), cspanrs<int >(rarri));
    EXPECT_LE(cspanrs<int >(larri), cspanrs<int >(rarri));
    EXPECT_LE(cspan  <int >(larri), cspan  <int >(larri));
    EXPECT_LE(cspanrs<int >(larri), cspan  <int >(larri));
    EXPECT_LE(cspan  <int >(larri), cspanrs<int >(larri));
    EXPECT_LE(cspanrs<int >(larri), cspanrs<int >(larri));
}

C4_END_NAMESPACE(c4)

#include "c4/libtest/supprwarn_pop.hpp"
