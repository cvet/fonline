#ifndef __SCRIPT__
#define __SCRIPT__

#include "Common.h"
#include "ScriptPragmas.h"
#include "ScriptInvoker.h"
#include "ScriptProfiler.h"
#include "angelscript.h"
#include "scriptarray.h"
#include "scriptstring.h"
#include "scriptdict.h"
#include "preprocessor.h"
#include <vector>
#include <string>

#define SCRIPT_ERROR_R( error, ... )     do { Script::RaiseException( error, ## __VA_ARGS__ ); return; } while( 0 )
#define SCRIPT_ERROR_R0( error, ... )    do { Script::RaiseException( error, ## __VA_ARGS__ ); return 0; } while( 0 )

typedef void ( *EndExecutionCallback )();
typedef vector< asIScriptContext* > ContextVec;

struct EngineData
{
    ScriptPragmaCallback*                PragmaCB;
    string                               DllTarget;
    bool                                 AllowNativeCalls;
    map< string, pair< string, void* > > LoadedDlls;
    ScriptInvoker*                       Invoker;
    ScriptProfiler*                      Profiler;
    StrIntMap                            CachedEnums;
    map< string, IntStrMap >             CachedEnumNames;
};

struct ReservedScriptFunction
{
    int* BindId;
    char FuncName[ 256 ];
    char FuncDecl[ 256 ];
};

class Script
{
public:
    static bool Init( ScriptPragmaCallback* pragma_callback, const char* dll_target, bool allow_native_calls, uint profiler_sample_time, bool profiler_save_to_file, bool profiler_dynamic_display );
    static void Finish();
    static bool InitThread();
    static void FinishThread();

    static void* LoadDynamicLibrary( const char* dll_name );
    static void  SetConcurrentExecution( bool enabled );
    static void  SetLoadLibraryCompiler( bool enabled );

    static void UnloadScripts();
    static bool ReloadScripts( const char* target, const char* cache_pefix );
    static bool BindReservedFunctions( ReservedScriptFunction* bind_func, uint bind_func_count );
    static bool RunModuleInitFunctions();

    static asIScriptEngine* GetEngine();
    static void             SetEngine( asIScriptEngine* engine );
    static asIScriptEngine* CreateEngine( ScriptPragmaCallback* pragma_callback, const char* dll_target, bool allow_native_calls );
    static void             FinishEngine( asIScriptEngine*& engine );

    static void              CreateContext();
    static void              FinishContext( asIScriptContext* ctx );
    static asIScriptContext* RequestContext();
    static void              ReturnContext( asIScriptContext* ctx );
    static void              GetExecutionContexts( ContextVec& contexts );
    static void              ReleaseExecutionContexts();
    static void              RaiseException( const char* message, ... );
    static void              PassException();
    static void              HandleException( asIScriptContext* ctx, const char* message, ... );
    static string            MakeContextTraceback( asIScriptContext* ctx, bool include_header, bool extended_header );

    static ScriptInvoker* GetInvoker();
    static string         GetDeferredCallsStatistics();
    static void           ProcessDeferredCalls();
    static void           SaveDeferredCalls( IniParser& data );
    static bool           LoadDeferredCalls( IniParser& data );

    static void   ProfilerContextCallback( asIScriptContext* ctx, void* obj );
    static string GetProfilerStatistics();

    static PropertyRegistrator* FindEntityRegistrator( const char* class_name );
    static bool                 RestoreEntity( const char* class_name, uint id, Properties& props );

    static const char* GetActiveModuleName();
    static const char* GetActiveFuncName();

    static void Watcher();
    static void SetRunTimeout( uint abort_timeout, uint message_timeout );

    static void Define( const char* def, ... );
    static void Undef( const char* def );
    static void CallPragmas( const Pragmas& pragmas );
    static bool LoadModuleFromFile( const char* module_name, FileManager& file, const char* dir, const char* cache_pefix );
    static bool LoadModuleFromCache( const char* module_name, const char* cache_pefix );
    static bool RestoreModuleFromBinary( const char* module_name, const UCharVec& bytecode, const UCharVec& lnt_data );

    static bool   BindImportedFunctions();
    static uint   BindByModuleFuncName( const char* module_name, const char* func_name, const char* decl, bool is_temp, bool disable_log = false );
    static uint   BindByScriptName( const char* script_name, const char* decl, bool is_temp, bool disable_log = false );
    static uint   BindByFuncNameInRuntime( const char* func_name, const char* decl, bool is_temp, bool disable_log = false );
    static uint   BindByFunc( asIScriptFunction* func, bool is_temp, bool disable_log = false );
    static uint   BindByFuncNum( hash func_num, bool is_temp, bool disable_log = false );
    static bool   RebindFunctions();
    static bool   ParseScriptName( const char* script_name, char* module_name, char* func_name, bool disable_log = false );
    static string GetBindFuncName( uint bind_id );
    static void MakeScriptNameInRuntime( const char* func_name, char(&script_name)[ MAX_FOTEXT ] );

    static hash               GetFuncNum( asIScriptFunction* func );
    static asIScriptFunction* FindFunc( hash func_num );
    static hash               BindScriptFuncNumByScriptName( const char* script_name, const char* decl );
    static hash               BindScriptFuncNumByFuncNameInRuntime( const char* func_name, const char* decl );
    static hash               BindScriptFuncNumByFunc( asIScriptFunction* func );
    static uint               GetScriptFuncBindId( hash func_num );
    static bool               PrepareScriptFuncContext( hash func_num, const char* call_func, const char* ctx_info );

    static void        CacheEnumValues();
    static int         GetEnumValue( const char* enum_value_name, bool& fail );
    static int         GetEnumValue( const char* enum_name, const char* value_name, bool& fail );
    static const char* GetEnumValueName( const char* enum_name, int value );

    // Script execution
    static void BeginExecution();
    static void EndExecution();
    static void AddEndExecutionCallback( EndExecutionCallback func );

    static bool              PrepareContext( uint bind_id, const char* call_func, const char* ctx_info );
    static void              SetArgUChar( uchar value );
    static void              SetArgUShort( ushort value );
    static void              SetArgUInt( uint value );
    static void              SetArgUInt64( uint64 value );
    static void              SetArgBool( bool value );
    static void              SetArgFloat( float value );
    static void              SetArgDouble( double value );
    static void              SetArgObject( void* value );
    static void              SetArgAddress( void* value );
    static bool              RunPrepared();
    static asIScriptContext* SuspendCurrentContext( uint time );
    static void              ResumeContext( asIScriptContext* ctx );
    static void              RunSuspended();
    static uint              GetReturnedUInt();
    static bool              GetReturnedBool();
    static void*             GetReturnedObject();
    static float             GetReturnedFloat();
    static double            GetReturnedDouble();
    static void*             GetReturnedRawAddress();

    static bool SynchronizeThread();
    static bool ResynchronizeThread();

    // Logging
    static void Log( const char* str );
    static void LogA( const char* str );
    static void LogError( const char* call_func, const char* error );
    static void SetLogDebugInfo( bool enabled );

    static void CallbackMessage( const asSMessageInfo* msg, void* param );
    static void CallbackException( asIScriptContext* ctx, void* param );

    // Arrays stuff
    static ScriptArray* CreateArray( const char* type );

    template< typename Type >
    static void AppendVectorToArray( const vector< Type >& vec, ScriptArray* arr )
    {
        if( !vec.empty() && arr )
        {
            uint i = (uint) arr->GetSize();
            arr->Resize( (asUINT) ( i + (uint) vec.size() ) );
            for( uint k = 0, l = (uint) vec.size(); k < l; k++, i++ )
            {
                Type* p = (Type*) arr->At( i );
                *p = vec[ k ];
            }
        }
    }
    template< typename Type >
    static void AppendVectorToArrayRef( const vector< Type >& vec, ScriptArray* arr )
    {
        if( !vec.empty() && arr )
        {
            uint i = (uint) arr->GetSize();
            arr->Resize( (asUINT) ( i + (uint) vec.size() ) );
            for( uint k = 0, l = (uint) vec.size(); k < l; k++, i++ )
            {
                Type* p = (Type*) arr->At( i );
                *p = vec[ k ];
                ( *p )->AddRef();
            }
        }
    }
    template< typename Type >
    static void AssignScriptArrayInVector( vector< Type >& vec, const ScriptArray* arr )
    {
        if( arr )
        {
            uint count = (uint) arr->GetSize();
            if( count )
            {
                vec.resize( count );
                for( uint i = 0; i < count; i++ )
                {
                    Type* p = (Type*) arr->At( i );
                    vec[ i ] = *p;
                }
            }
        }
    }
};

class CBytecodeStream: public asIBinaryStream
{
private:
    int                   readPos;
    int                   writePos;
    std::vector< asBYTE > binBuf;

public:
    CBytecodeStream()
    {
        writePos = 0;
        readPos = 0;
    }
    void Write( const void* ptr, asUINT size )
    {
        if( !ptr || !size ) return;
        binBuf.resize( binBuf.size() + size );
        memcpy( &binBuf[ writePos ], ptr, size );
        writePos += size;
    }
    void Read( void* ptr, asUINT size )
    {
        if( !ptr || !size ) return;
        memcpy( ptr, &binBuf[ readPos ], size );
        readPos += size;
    }
    std::vector< asBYTE >& GetBuf() { return binBuf; }
};

#endif // __SCRIPT__
