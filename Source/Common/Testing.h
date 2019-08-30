#pragma once

#include "Common.h"

#ifdef FO_TESTING
# define CATCH_CONFIG_PREFIX_ALL
# include "catch.hpp"
# define RUNTIME_ASSERT( expr )             CATCH_REQUIRE( expr )
# define RUNTIME_ASSERT_STR( expr, str )    RUNTIME_ASSERT( expr )
# define TEST_CASE()                        CATCH_TEST_CASE( __FILE__ )
# define TEST_SECTION()                     CATCH_SECTION( _FUNC_ "__" ___MSG0( __LINE__ ) )
#else
# define RUNTIME_ASSERT( expr )             ( !!( expr ) || RaiseAssert( # expr, __FILE__, __LINE__ ) )
# define RUNTIME_ASSERT_STR( expr, str )    ( !!( expr ) || RaiseAssert( str, __FILE__, __LINE__ ) )
# define TEST_CASE()                        static void Test_ ## __LINE__ ## ( )
# define TEST_SECTION()                     if( !!( __LINE__ ) )
#endif

extern void RunTestCases( int argc, char** argv );
extern void CatchExceptions( const string& app_name, int app_ver );
extern void CreateDump( const string& appendix, const string& message );
extern bool RaiseAssert( const string& message, const string& file, int line );
