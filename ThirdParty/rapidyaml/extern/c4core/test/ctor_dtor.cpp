// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "c4/ctor_dtor.hpp"

#include "c4/libtest/supprwarn_push.hpp"
#include "c4/test.hpp"

C4_BEGIN_NAMESPACE(c4)

C4_BEGIN_HIDDEN_NAMESPACE
struct subject
{
    static size_t ct_cp, ct_mv, cp, mv;
    static void clear() { ct_cp = ct_mv = cp = mv = 0; }
    subject(Counting<std::string> const& s)
    {
        ++ct_cp;
    }
    subject(Counting<std::string> && s)
    {
        ++ct_mv;
    }
    subject(subject const& s)
    {
        ++cp;
    }
    subject(subject && s)
    {
        ++mv;
    }
};
size_t subject::ct_cp = 0;
size_t subject::ct_mv = 0;
size_t subject::cp = 0;
size_t subject::mv = 0;
C4_END_HIDDEN_NAMESPACE

TEST(ctor_dtor, construct_n)
{
    using T = Counting< subject >;
    char buf1[100 * sizeof(T)];
    T* mem1 = reinterpret_cast<T*>(buf1);

    using cs = Counting< std::string >;

    int num = 10;

    {
        auto chc = T::check_num_ctors_dtors(num, 0);
        auto ch = cs::check_num_ctors_dtors(1, 1);
        cs s("bla");
        construct_n(mem1, num, s);
        EXPECT_EQ(subject::ct_cp, num);
        subject::clear();
    }

    {
        auto chc = T::check_num_ctors_dtors(num, 0);
        auto ch = cs::check_num_ctors_dtors(1, 1);
        construct_n(mem1, num, cs("bla"));  // BAD!!! will call 10 moves
        EXPECT_EQ(subject::ct_cp, num);
        subject::clear();
    }
}

//-----------------------------------------------------------------------------
template< class T >
void create_make_room_buffer(std::vector<T> &orig)
{
    C4_STATIC_ASSERT(std::is_integral< T >::value);
    for(T i = 0, e = (T)orig.size(); i < e; ++i)
    {
        orig[i] = (T)(T(33) + i % (T(122) - T(33))); // assign characters
    }
}
template<>
void create_make_room_buffer< std::string >(std::vector<std::string> &orig)
{
    for(int i = 0, e = (int)orig.size(); i < e; ++i)
    {
        char c = (char)(33 + i % (122 - 33));
        orig[i].assign(10, c);
    }
}

template< class T >
void do_make_room_inplace(std::vector< T > const& orig, std::vector< T > & buf,
                          size_t bufsz, size_t room, size_t pos)
{
    buf = orig;
    make_room(buf.data() + pos, bufsz, room);
}
template< class T >
void do_make_room_srcdst(std::vector< T > const& orig, std::vector< T > & buf,
                         size_t bufsz, size_t room, size_t pos)
{
    buf.resize(orig.size());
    for(auto &t : buf)
    {
        t = T();
    }
    make_room(buf.data(), orig.data(), bufsz, room, pos);
}

template< class T >
void do_make_room_check(std::vector< T > const& orig, std::vector< T > & buf,
                        size_t bufsz, size_t room, size_t pos)
{
    for(size_t i = 0, e = orig.size(); i < e; ++i)
    {
        if(i < pos)
        {
            // memory before the move, should be untouched
            EXPECT_EQ(buf[i], orig[i]) << "i=" << (int)i;
        }
        else
        {
            if(i >= pos && i < pos + room)
            {
                // this is the memory that was moved (at its origin)
                //EXPECT_EQ(buf[i], orig[i]) << "i=" << (int)i;
            }
            else if(i >= pos + room && i < pos + room + bufsz)
            {
                // this is the memory that was moved (at its destination)
                EXPECT_EQ(buf[i], orig[i - room]) << "i=" << (int)i;
            }
            else
            {
                // this is memory at the end, should be untouched
                EXPECT_EQ(buf[i], orig[i]) << "i=" << (int)i;
            }
        }
    }
};

template< class T >
void do_make_room_inplace_test(std::vector< T > const& orig, std::vector< T > & buf,
                               size_t bufsz, size_t room, size_t pos)
{
    do_make_room_inplace(orig, buf, bufsz, room, pos);
    do_make_room_check(orig, buf, bufsz, room, pos);
}

template< class T >
void do_make_room_srcdst_test(std::vector< T > const& orig, std::vector< T > & buf,
                              size_t bufsz, size_t room, size_t pos)
{
    do_make_room_srcdst(orig, buf, buf.size() - room, room, pos);
    do_make_room_check(orig, buf, buf.size() - room, room, pos);
}

template< class T, class Func >
void test_make_room(Func test_func)
{
    std::vector< T > orig(100), buf(100);

    create_make_room_buffer(orig);

    {
        SCOPED_TRACE("in the beginning without overlap");
        test_func(orig, buf, /*bufsz*/10, /*room*/10, /*pos*/0);
    }

    {
        SCOPED_TRACE("in the beginning with overlap");
        test_func(orig, buf, /*bufsz*/10, /*room*/15, /*pos*/0);
    }

    {
        SCOPED_TRACE("in the middle without overlap");
        test_func(orig, buf, /*bufsz*/10, /*room*/10, /*pos*/10);
    }

    {
        SCOPED_TRACE("in the middle with overlap");
        test_func(orig, buf, /*bufsz*/10, /*room*/15, /*pos*/10);
    }
}
TEST(ctor_dtor, make_room_inplace)
{
    {
        SCOPED_TRACE("uint8_t");
        test_make_room< uint8_t >(do_make_room_inplace_test< uint8_t >);
    }
    {
        SCOPED_TRACE("uint64_t");
        test_make_room< uint64_t >(do_make_room_inplace_test< uint64_t >);
    }
    {
        SCOPED_TRACE("std::string");
        test_make_room< std::string >(&do_make_room_inplace_test< std::string >);
    }
}
TEST(ctor_dtor, make_room_srcdst)
{
    {
        SCOPED_TRACE("uint8_t");
        test_make_room< uint8_t >(&do_make_room_srcdst_test< uint8_t >);
    }
    {
        SCOPED_TRACE("uint64_t");
        test_make_room< uint64_t >(&do_make_room_srcdst_test< uint64_t >);
    }
    {
        SCOPED_TRACE("std::string");
        test_make_room< std::string >(&do_make_room_srcdst_test< std::string >);
    }
}

//-----------------------------------------------------------------------------

template< class T >
void do_destroy_room_inplace(std::vector< T > const& orig, std::vector< T > & buf,
                          size_t bufsz, size_t room, size_t pos)
{
    buf = orig;
    destroy_room(buf.data() + pos, bufsz - pos, room);
}
template< class T >
void do_destroy_room_srcdst(std::vector< T > const& orig, std::vector< T > & buf,
                            size_t bufsz, size_t room, size_t pos)
{
    buf = orig;
    destroy_room(buf.data(), orig.data(), bufsz, room, pos);
}

template< class T >
void do_destroy_room_check(std::vector< T > const& orig, std::vector< T > & buf,
                           size_t bufsz, size_t room, size_t pos)
{
    for(size_t i = 0, e = orig.size(); i < e; ++i)
    {
        if(i < pos)
        {
            // memory before the destroy, should be untouched
            EXPECT_EQ(buf[i], orig[i]) << "i=" << (int)i << "  room=" << room  << "  pos=" << pos;
        }
        else
        {
            if(i >= pos && i < pos + room)
            {
                // this is the memory that was destroyed (at its origin)
            }
            else if(i >= pos + room && i < pos + room + bufsz)
            {
                // this is the memory that was moved (at its destination)
                EXPECT_EQ(buf[i - room], orig[i]) << "i=" << (int)i << "  room=" << room  << "  pos=" << pos;
            }
            else
            {
                // this is memory at the end, should be untouched
                EXPECT_EQ(buf[i], orig[i]) << "i=" << (int)i << "  room=" << room  << "  pos=" << pos;
            }
        }
    }
};

template< class T >
void do_destroy_room_inplace_test(std::vector< T > const& orig, std::vector< T > & buf,
                                  size_t room, size_t pos)
{
    do_destroy_room_inplace(orig, buf, buf.size(), room, pos);
    do_destroy_room_check(orig, buf, buf.size(), room, pos);
}

template< class T >
void do_destroy_room_srcdst_test(std::vector< T > const& orig, std::vector< T > & buf,
                                 size_t room, size_t pos)
{
    do_destroy_room_srcdst(orig, buf, buf.size(), room, pos);
    do_destroy_room_check(orig, buf, buf.size(), room, pos);
}
template< class T, class Func >
void test_destroy_room(Func test_func)
{
    std::vector< T > orig(100), buf(100);

    create_make_room_buffer(orig);

    {
        SCOPED_TRACE("in the beginning, room=10");
        test_func(orig, buf, /*room*/10, /*pos*/0);
    }

    {
        SCOPED_TRACE("in the beginning, room=20");
        test_func(orig, buf, /*room*/20, /*pos*/0);
    }

    {
        SCOPED_TRACE("in the middle, room=10");
        test_func(orig, buf, /*room*/10, /*pos*/10);
    }

    {
        SCOPED_TRACE("in the middle, room=20");
        test_func(orig, buf, /*room*/20, /*pos*/10);
    }
}
TEST(ctor_dtor, destroy_room_inplace)
{
    {
        SCOPED_TRACE("uint8_t");
        test_destroy_room< uint8_t >(do_destroy_room_inplace_test< uint8_t >);
    }
    {
        SCOPED_TRACE("uint64_t");
        test_destroy_room< uint64_t >(do_destroy_room_inplace_test< uint64_t >);
    }
    {
        SCOPED_TRACE("std::string");
        test_destroy_room< std::string >(&do_destroy_room_inplace_test< std::string >);
    }
}
TEST(ctor_dtor, destroy_room_srcdst)
{
    {
        SCOPED_TRACE("uint8_t");
        test_destroy_room< uint8_t >(&do_destroy_room_srcdst_test< uint8_t >);
    }
    {
        SCOPED_TRACE("uint64_t");
        test_destroy_room< uint64_t >(&do_destroy_room_srcdst_test< uint64_t >);
    }
    {
        SCOPED_TRACE("std::string");
        test_destroy_room< std::string >(&do_destroy_room_srcdst_test< std::string >);
    }
}

C4_END_NAMESPACE(c4)

#include "c4/libtest/supprwarn_pop.hpp"
