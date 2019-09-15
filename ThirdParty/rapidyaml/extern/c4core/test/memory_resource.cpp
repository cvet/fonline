// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "c4/memory_resource.hpp"

#include "c4/test.hpp"
#include <limits>

C4_BEGIN_NAMESPACE(c4)

TEST(set_aalloc, basic)
{
    auto a = get_aalloc();
    set_aalloc(nullptr);
    EXPECT_EQ(get_aalloc(), nullptr);
    set_aalloc(a);
    EXPECT_EQ(get_aalloc(), a);
}

TEST(set_afree, basic)
{
    auto a = get_afree();
    set_afree(nullptr);
    EXPECT_EQ(get_afree(), nullptr);
    set_afree(a);
    EXPECT_EQ(get_afree(), a);
}

TEST(set_arealloc, basic)
{
    auto a = get_arealloc();
    set_arealloc(nullptr);
    EXPECT_EQ(get_arealloc(), nullptr);
    set_arealloc(a);
    EXPECT_EQ(get_arealloc(), a);
}

//-----------------------------------------------------------------------------
namespace detail {
void* aalloc_impl(size_t size, size_t alignment=alignof(max_align_t));
void* arealloc_impl(void *ptr, size_t oldsz, size_t newsz, size_t alignment=alignof(max_align_t));
void  afree_impl(void *ptr);
} // namespace detail

TEST(aalloc_impl, error_bad_align)
{
#if defined(C4_POSIX)
    C4_EXPECT_ERROR_OCCURS(1);
    auto *mem = detail::aalloc_impl(64, 9); // allocating with a non-power of two value is invalid
    EXPECT_EQ(mem, nullptr);
#endif
}

TEST(aalloc_impl, error_out_of_mem)
{
#if defined(C4_POSIX)
    if(sizeof(size_t) != 8) return; // valgrind complains that size is -1
    C4_EXPECT_ERROR_OCCURS(1);
    size_t sz = std::numeric_limits<size_t>::max();
    sz /= 2;
    auto *mem = detail::aalloc_impl(sz);
    EXPECT_EQ(mem, nullptr);
#endif
}

//-----------------------------------------------------------------------------

void do_test_realloc(arealloc_pfn fn)
{
#define _set(dim) for(char i = 0; i < dim; ++i) { mem[i] = size_t(i); }
#define _check(dim) for(char i = 0; i < dim; ++i) { EXPECT_EQ(mem[i], i); }

    char *mem = (char*) aalloc(16, alignof(max_align_t));
    _set(16);
    _check(16);
    mem = (char*) fn(mem, 16, 20, alignof(max_align_t));
    _check(16);
    mem = (char*) fn(mem, 8, 20, alignof(max_align_t));
    _check(8);
    afree(mem);

#undef _set
#undef _check
}

TEST(realloc_impl, basic)
{
    do_test_realloc(&detail::arealloc_impl);
}

TEST(realloc, basic)
{
    do_test_realloc(&arealloc);
}


//-----------------------------------------------------------------------------

void do_memreslinear_realloc_test(MemoryResourceLinear &mr)
{
    C4_ASSERT(mr.capacity() >= 128); // this is needed for the tests below

    char * mem = (char*) mr.allocate(32);
    EXPECT_EQ(mem-(char*)mr.mem(), 0);
    EXPECT_EQ(mr.size(), 32);
    EXPECT_EQ(mr.slack(), mr.capacity() - 32);

    mem = (char*) mr.reallocate(mem, 32, 16);
    EXPECT_EQ(mem-(char*)mr.mem(), 0);
    EXPECT_EQ(mr.size(), 16);
    EXPECT_EQ(mr.slack(), mr.capacity() - 16);

    mem = (char*) mr.reallocate(mem, 16, 64);
    EXPECT_EQ(mem-(char*)mr.mem(), 0);
    EXPECT_EQ(mr.size(), 64);
    EXPECT_EQ(mr.slack(), mr.capacity() - 64);

    mem = (char*) mr.reallocate(mem, 64, 32);
    EXPECT_EQ(mem-(char*)mr.mem(), 0);
    EXPECT_EQ(mr.size(), 32);
    EXPECT_EQ(mr.slack(), mr.capacity() - 32);


    char *mem2 = (char*) mr.allocate(32);
    EXPECT_EQ(mem-(char*)mr.mem(), 0);
    EXPECT_EQ(mem2-(char*)mr.mem(), 32);
    EXPECT_EQ(mr.size(), 64);
    EXPECT_EQ(mr.slack(), mr.capacity() - 64);

    mem = (char*) mr.reallocate(mem, 32, 16);
    EXPECT_EQ(mem-(char*)mr.mem(), 0);
    EXPECT_EQ(mr.size(), 64);
    EXPECT_EQ(mr.slack(), mr.capacity() - 64);
}

TEST(MemoryResourceLinear, reallocate)
{
    MemoryResourceLinear mr(128);
    do_memreslinear_realloc_test(mr);
}

TEST(MemoryResourceLinearArr, reallocate)
{
    MemoryResourceLinearArr<128> mr;
    do_memreslinear_realloc_test(mr);
}


//-----------------------------------------------------------------------------

TEST(MemoryResourceLinear, error_out_of_mem)
{
    {
        C4_EXPECT_ERROR_OCCURS(0);
        MemoryResourceLinear mr(8);
        mr.allocate(2);
    }

    {
        C4_EXPECT_ERROR_OCCURS(2);
        MemoryResourceLinear mr(8);
        mr.allocate(9);
    }
}

TEST(MemoryResourceLinearArr, error_out_of_mem)
{
    {
        C4_EXPECT_ERROR_OCCURS(0);
        MemoryResourceLinearArr<8> mr;
        mr.allocate(2);
    }

    {
        C4_EXPECT_ERROR_OCCURS(2);
        MemoryResourceLinearArr<8> mr;
        mr.allocate(9);
    }
}


//-----------------------------------------------------------------------------

TEST(ScopedMemoryResource, basic)
{
    auto *before = get_memory_resource();
    {
        MemoryResourceCounts mrc;
        ScopedMemoryResource smr(&mrc);
        EXPECT_EQ(get_memory_resource(), &mrc);
    }
    EXPECT_EQ(get_memory_resource(), before);
}

TEST(ScopedMemoryResourceCounts, basic)
{
    auto *before = get_memory_resource();
    {
        auto sac = ScopedMemoryResourceCounts{};
        EXPECT_EQ(get_memory_resource(), &sac.mr);
    }
    EXPECT_EQ(get_memory_resource(), before);
}

TEST(ScopedMemoryResourceCounts, counts)
{
    auto *before = get_memory_resource();
    C4_UNUSED(before);

    {
        auto checker = AllocationCountsChecker();
        auto *mr = &checker.mr;

        for(size_t sz : {16, 32, 64, 128})
        {
            void *mem = mr->allocate(sz);
            checker.check_all_delta(1, sz, sz);
            mr->deallocate(mem, sz);
            checker.reset();
        }
        checker.check_curr(0, 0);
        checker.check_total(4, 240);
        checker.check_max(1, 128);
    }

    {
        auto checker = AllocationCountsChecker();
        auto *mr = &checker.mr;

        std::pair<void *, size_t> mem[4] = {{0,16}, {0,32}, {0,64}, {0,128}};
        for(auto& m : mem)
        {
            m.first = mr->allocate(m.second);
            checker.check_curr_delta(1, m.second);
            checker.reset();
        }
        checker.check_curr(4, 240);
        checker.check_total(4, 240);
        checker.check_max(4, 240);
        for(auto& m : mem)
        {
            mr->deallocate(m.first, m.second);
        }
        checker.check_curr(0, 0);
        checker.check_total(4, 240);
        checker.check_max(4, 240);
    }

}

C4_END_NAMESPACE(c4)
