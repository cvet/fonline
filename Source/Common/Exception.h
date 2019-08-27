#ifndef ___EXCEPTION___
#define ___EXCEPTION___

#include "Common.h"

#define RUNTIME_ASSERT( a )             ( !!( a ) || RaiseAssert( # a, __FILE__, __LINE__ ) )
#define RUNTIME_ASSERT_STR( a, str )    ( !!( a ) || RaiseAssert( str, __FILE__, __LINE__ ) )

extern void CatchExceptions( const string& app_name, int app_ver );
extern void CreateDump( const string& appendix, const string& message );
extern bool RaiseAssert( const string& message, const string& file, int line );

#endif // ___EXCEPTION___
