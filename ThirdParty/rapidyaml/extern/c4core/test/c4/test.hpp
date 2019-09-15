#ifndef _C4_TEST_HPP_
#define _C4_TEST_HPP_

#include "c4/config.hpp"
#include "c4/memory_resource.hpp"
#include "c4/allocator.hpp"
#include "c4/substr.hpp"

// FIXME - these are just dumb placeholders
#define C4_LOGF_ERR(...) fprintf(stderr, __VA_ARGS__)
#define C4_LOGF_WARN(...) fprintf(stderr, __VA_ARGS__)
#define C4_LOGP(msg, ...) printf(msg)

#ifdef __clang__
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wdeprecated-declarations" //  warning : 'mbsrtowcs' is deprecated: This function or variable may be unsafe. Consider using sscanf_s instead
#endif

#include <gtest/gtest.h>

#ifdef __clang__
#   pragma clang diagnostic pop
#endif

#define C4_EXPECT(expr) EXPECT_TRUE(expr) << C4_PRETTY_FUNC << "\n"
#define C4_EXPECT_FALSE(expr) EXPECT_FALSE(expr) << C4_PRETTY_FUNC << "\n"

#define C4_EXPECT_NE(expr1, expr2) EXPECT_NE(expr1, expr2) << C4_PRETTY_FUNC << "\n"
#define C4_EXPECT_EQ(expr1, expr2) EXPECT_EQ(expr1, expr2) << C4_PRETTY_FUNC << "\n"
#define C4_EXPECT_LT(expr1, expr2) EXPECT_LT(expr1, expr2) << C4_PRETTY_FUNC << "\n"
#define C4_EXPECT_GT(expr1, expr2) EXPECT_GT(expr1, expr2) << C4_PRETTY_FUNC << "\n"
#define C4_EXPECT_GE(expr1, expr2) EXPECT_GE(expr1, expr2) << C4_PRETTY_FUNC << "\n"
#define C4_EXPECT_LE(expr1, expr2) EXPECT_LE(expr1, expr2) << C4_PRETTY_FUNC << "\n"

#define C4_EXPECT_STREQ(expr1, expr2) EXPECT_STREQ(expr1, expr2) << C4_PRETTY_FUNC << "\n"
#define C4_EXPECT_STRNE(expr1, expr2) EXPECT_STRNE(expr1, expr2) << C4_PRETTY_FUNC << "\n"

#define C4_EXPECT_STRCASEEQ(expr1, expr2) EXPECT_STRCASEEQ(expr1, expr2) << C4_PRETTY_FUNC << "\n"
#define C4_EXPECT_STRCASENE(expr1, expr2) EXPECT_STRCASENE(expr1, expr2) << C4_PRETTY_FUNC << "\n"


#define C4_TEST(ts, tn) TEST_F(::c4::TestFixture, ts##_##tn)


C4_BEGIN_NAMESPACE(c4)

inline void PrintTo(const  substr& s, ::std::ostream* os) { *os << s; }
inline void PrintTo(const csubstr& s, ::std::ostream* os) { *os << s; }

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

class TestFixture : public ::testing::Test
{
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    // You can do set-up work for each test here.
    TestFixture() {}

    // You can do clean-up work that doesn't throw exceptions here.
    virtual ~TestFixture() {}

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    // Code here will be called immediately after the constructor (right
    // before each test).
    virtual void SetUp() {}

    // Code here will be called immediately after each test (right
    // before the destructor).
    virtual void TearDown() {}

    // Objects declared here can be used by all tests in the test case for Foo.
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/** RAII class that tests whether an error occurs inside a scope. */
struct TestErrorOccurs
{
    TestErrorOccurs(size_t num_expected_errors = 1)
      :
      expected_errors(num_expected_errors),
      tmp_settings(c4::ON_ERROR_CALLBACK, &TestErrorOccurs::error_callback)
    {
        num_errors = 0;
    }
    ~TestErrorOccurs()
    {
        EXPECT_EQ(num_errors, expected_errors);
        num_errors = 0;
    }

    size_t expected_errors;
    static size_t num_errors;
    ScopedErrorSettings tmp_settings;
    static void error_callback(const char* /*msg*/, size_t /*msg_size*/)
    {
        ++num_errors;
    }
};
#define C4_EXPECT_ERROR_OCCURS(...) \
  auto _testerroroccurs##__LINE__ = TestErrorOccurs(__VA_ARGS__)

#ifdef C4_USE_ASSERT
#   define C4_EXPECT_ASSERT_TRIGGERS(...) C4_EXPECT_ERROR_OCCURS(__VA_ARGS__)
#else
#   define C4_EXPECT_ASSERT_TRIGGERS(...)
#endif

#ifdef C4_USE_XASSERT
#   define C4_EXPECT_XASSERT_TRIGGERS(...) C4_EXPECT_ERROR_OCCURS(__VA_ARGS__)
#else
#   define C4_EXPECT_XASSERT_TRIGGERS(...)
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** count constructors, destructors and assigns */
template< class T >
struct Counting
{
    using value_type = T;

    T obj;

    bool operator== (T const& that) const { return obj == that; }

    static bool log_ctors;
    static size_t num_ctors;
    template< class ...Args >
    Counting(Args && ...args);

    static bool log_dtors;
    static size_t num_dtors;
    ~Counting();

    static bool log_copy_ctors;
    static size_t num_copy_ctors;
    Counting(Counting const& that);

    static bool log_move_ctors;
    static size_t num_move_ctors;
    Counting(Counting && that);

    static bool log_copy_assigns;
    static size_t num_copy_assigns;
    Counting& operator= (Counting const& that);

    static bool log_move_assigns;
    static size_t num_move_assigns;
    Counting& operator= (Counting && that);


    struct check_num
    {
        char   const* name;
        size_t const& what;
        size_t const  initial;
        size_t const  must_be_num;
        check_num(char const* nm, size_t const& w, size_t n) : name(nm), what(w), initial(what), must_be_num(n) {}
        ~check_num()
        {
            size_t del = what - initial;
            EXPECT_EQ(del, must_be_num) << "# of " << name << " calls: expected " << must_be_num << ", but got " << del;
        }
    };

    static check_num check_ctors(size_t n) { return check_num("ctor", num_ctors, n); }
    static check_num check_dtors(size_t n) { return check_num("dtor", num_dtors, n); }
    static check_num check_copy_ctors(size_t n) { return check_num("copy_ctor", num_copy_ctors, n); }
    static check_num check_move_ctors(size_t n) { return check_num("move_ctor", num_move_ctors, n); }
    static check_num check_copy_assigns(size_t n) { return check_num("copy_assign", num_copy_assigns, n); }
    static check_num check_move_assigns(size_t n) { return check_num("move_assign", num_move_assigns, n); }

    struct check_num_ctors_dtors
    {
        check_num ctors, dtors;
        check_num_ctors_dtors(size_t _ctors, size_t _dtors)
        :
            ctors(check_ctors(_ctors)),
            dtors(check_dtors(_dtors))
        {
        }
    };
    static check_num_ctors_dtors check_ctors_dtors(size_t _ctors, size_t _dtors)
    {
        return check_num_ctors_dtors(_ctors, _dtors);
    }

    struct check_num_all
    {
        check_num ctors, dtors, cp_ctors, mv_ctors, cp_assigns, mv_assigns;
        check_num_all(size_t _ctors, size_t _dtors, size_t _cp_ctors, size_t _mv_ctors, size_t _cp_assigns, size_t _mv_assigns)
        {
            ctors = check_ctors(_ctors);
            dtors = check_dtors(_dtors);
            cp_ctors = check_copy_ctors(_cp_ctors);
            mv_ctors = check_move_ctors(_mv_ctors);
            cp_assigns = check_copy_assigns(_cp_assigns);
            mv_assigns = check_move_assigns(_mv_assigns);
        }
    };
    static check_num_all check_all(size_t _ctors, size_t _dtors, size_t _cp_ctors, size_t _move_ctors, size_t _cp_assigns, size_t _mv_assigns)
    {
        return check_num_all(_ctors, _dtors, _cp_ctors, _move_ctors, _cp_assigns, _mv_assigns);
    }

    static void reset()
    {
        num_ctors = 0;
        num_dtors = 0;
        num_copy_ctors = 0;
        num_move_ctors = 0;
        num_copy_assigns = 0;
        num_move_assigns = 0;
    }
};

template< class T > size_t Counting< T >::num_ctors = 0;
template< class T > bool   Counting< T >::log_ctors = false;
template< class T > size_t Counting< T >::num_dtors = 0;
template< class T > bool   Counting< T >::log_dtors = false;
template< class T > size_t Counting< T >::num_copy_ctors = 0;
template< class T > bool   Counting< T >::log_copy_ctors = false;
template< class T > size_t Counting< T >::num_move_ctors = 0;
template< class T > bool   Counting< T >::log_move_ctors = false;
template< class T > size_t Counting< T >::num_copy_assigns = 0;
template< class T > bool   Counting< T >::log_copy_assigns = false;
template< class T > size_t Counting< T >::num_move_assigns = 0;
template< class T > bool   Counting< T >::log_move_assigns = false;

template< class T >
template< class ...Args >
Counting< T >::Counting(Args && ...args) : obj(std::forward< Args >(args)...)
{
    if(log_ctors) C4_LOGP("Counting[{}]::ctor #{}\n", (void*)this, num_ctors);
    ++num_ctors;
}

template< class T >
Counting< T >::~Counting()
{
    if(log_dtors) C4_LOGP("Counting[{}]::dtor #{}\n", (void*)this, num_dtors);
    ++num_dtors;
}

template< class T >
Counting< T >::Counting(Counting const& that) : obj(that.obj)
{
    if(log_copy_ctors) C4_LOGP("Counting[{}]::copy_ctor #{}\n", (void*)this, num_copy_ctors);
    ++num_copy_ctors;
}

template< class T >
Counting< T >::Counting(Counting && that) : obj(std::move(that.obj))
{
    if(log_move_ctors) C4_LOGP("Counting[{}]::move_ctor #{}\n", (void*)this, num_move_ctors);
    ++num_move_ctors;
}

template< class T >
Counting< T >& Counting< T >::operator= (Counting const& that)
{
    obj = that.obj;
    if(log_copy_assigns) C4_LOGP("Counting[{}]::copy_assign #{}\n", (void*)this, num_copy_assigns);
    ++num_copy_assigns;
    return *this;
}

template< class T >
Counting< T >& Counting< T >::operator= (Counting && that)
{
    obj = std::move(that.obj);
    if(log_move_assigns) C4_LOGP("Counting[{}]::move_assign #{}\n", (void*)this, num_move_assigns);
    ++num_move_assigns;
    return *this;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/** @todo refactor to use RAII @see Counting */
struct AllocationCountsChecker : public ScopedMemoryResourceCounts
{
    AllocationCounts first;

public:

    AllocationCountsChecker()
    :
        ScopedMemoryResourceCounts(),
        first(mr.counts())
    {
    }

    AllocationCountsChecker(MemoryResource *mr_)
    :
        ScopedMemoryResourceCounts(mr_),
        first(mr.counts())
    {
    }

    void reset()
    {
        first = mr.counts();
    }

    /** check value of curr allocations and size */
    void check_curr(ssize_t expected_allocs, ssize_t expected_size) const
    {
        EXPECT_EQ(mr.counts().curr.allocs, expected_allocs);
        EXPECT_EQ(mr.counts().curr.size, expected_size);
    }
    /** check delta of curr allocations and size since construction or last reset() */
    void check_curr_delta(ssize_t expected_allocs, ssize_t expected_size) const
    {
        AllocationCounts delta = mr.counts() - first;
        EXPECT_EQ(delta.curr.allocs, expected_allocs);
        EXPECT_EQ(delta.curr.size, expected_size);
    }

    /** check value of total allocations and size */
    void check_total(ssize_t expected_allocs, ssize_t expected_size) const
    {
        EXPECT_EQ(mr.counts().total.allocs, expected_allocs);
        EXPECT_EQ(mr.counts().total.size, expected_size);
    }
    /** check delta of total allocations and size since construction or last reset() */
    void check_total_delta(ssize_t expected_allocs, ssize_t expected_size) const
    {
        AllocationCounts delta = mr.counts() - first;
        EXPECT_EQ(delta.total.allocs, expected_allocs);
        EXPECT_EQ(delta.total.size, expected_size);
    }

    /** check value of max allocations and size */
    void check_max(ssize_t expected_max_allocs, ssize_t expected_max_size) const
    {
        EXPECT_EQ(mr.counts().max.allocs, expected_max_allocs);
        EXPECT_EQ(mr.counts().max.size, expected_max_size);
    }

    /** check that since construction or the last reset():
     *    - num_allocs occcurred
     *    - totaling total_size
     *    - of which the largest is max_size */
    void check_all_delta(ssize_t num_allocs, ssize_t total_size, ssize_t max_size) const
    {
        check_curr_delta(num_allocs, total_size);
        check_total_delta(num_allocs, total_size);
        check_max(num_allocs > mr.counts().max.allocs ? num_allocs : mr.counts().max.allocs,
                  max_size   > mr.counts().max.size   ? max_size   : mr.counts().max.size);
    }
};

C4_END_NAMESPACE(c4)

#endif // _C4_TEST_HPP_
