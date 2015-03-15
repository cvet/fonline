#ifndef ___EXCEPTION___
#define ___EXCEPTION___

// Uncomment for use minidumps (dmp) instead readable dumps (txt)
// #define EXCEPTION_MINIDUMP

void CatchExceptions( const char* app_name, int app_ver );
void SetExceptionsRussianText();
void CreateDump( const char* appendix );
bool RaiseAssert( const char* message, const char* file, int line );

#endif // ___EXCEPTION___
