#ifndef ___EXCEPTION___
#define ___EXCEPTION___

extern void CatchExceptions( const string& app_name, int app_ver );
extern void CreateDump( const string& appendix, const string& message );
extern bool RaiseAssert( const string& message, const string& file, int line );

#endif // ___EXCEPTION___
