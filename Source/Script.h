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
    static bool Init( ScriptPragmaCallback* pragma_callback, const char* dll_target, bool allow_native_calls, uint profiler_sample_time, uint profiler_save_time, bool profiler_dynamic_display );
    static void Finish();
    static bool InitThread();
    static void FinishThread();

    static void* LoadDynamicLibrary( const char* dll_name );
    static void  SetWrongGlobalObjects( StrVec& names );
    static void  SetConcurrentExecution( bool enabled );
    static void  SetLoadLibraryCompiler( bool enabled );

    static void UnloadScripts();
    static bool ReloadScripts( const char* target, bool skip_binaries, const char* file_pefix = NULL );
    static bool BindReservedFunctions( ReservedScriptFunction* bind_func, uint bind_func_count );
    static bool RunModuleInitFunctions();

    static void DummyAddRef( void* );
    static void DummyRelease( void* );

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
    static void              HandleException( asIScriptContext* ctx, const char* message, ... );
    static string            MakeContextTraceback( asIScriptContext* ctx );

    static ScriptInvoker* GetInvoker();
    static string         GetInvocationsStatistics();
    static void           ProcessInvocations();
    static void           SaveInvocations( void ( * save_func )( void*, size_t ) );
    static bool           LoadInvocations( void* f, uint version );

    static void   ProfilerContextCallback( asIScriptContext* ctx, void* obj );
    static string GetProfilerStatistics();

    static const char*      GetActiveModuleName();
    static const char*      GetActiveFuncName();
    static asIScriptModule* GetModule( const char* name );
    static asIScriptModule* CreateModule( const char* module_name );

    static void Watcher();
    static void SetRunTimeout( uint abort_timeout, uint message_timeout );

    static void Define( const char* def, ... );
    static void Undef( const char* def );
    static void CallPragmas( const Pragmas& pragmas );
    static bool LoadScript( const char* module_name, const char* source, bool skip_binary, const char* file_prefix = NULL );
    static bool LoadScript( const char* module_name, const uchar* bytecode, uint len );

    static bool   BindImportedFunctions();
    static uint   Bind( const char* module_name, const char* func_name, const char* decl, bool is_temp, bool disable_log = false );
    static uint   Bind( const char* script_name, const char* decl, bool is_temp, bool disable_log = false );
    static uint   Bind( asIScriptFunction* func, bool is_temp, bool disable_log = false );
    static uint   Bind( hash func_num, bool is_temp, bool disable_log = false );
    static bool   RebindFunctions();
    static bool   ReparseScriptName( const char* script_name, char* module_name, char* func_name, bool disable_log = false );
    static string GetBindFuncName( uint bind_id );

    static hash               GetFuncNum( asIScriptFunction* func );
    static asIScriptFunction* FindFunc( hash func_num );
    static hash               BindScriptFuncNum( const char* script_name, const char* decl );
    static hash               BindScriptFuncNum( asIScriptFunction* func );
    static uint               GetScriptFuncBindId( hash func_num );
    static bool               PrepareScriptFuncContext( hash func_num, const char* call_func, const char* ctx_info );
    static string             GetScriptFuncName( hash func_num );

    // Script execution
    static void BeginExecution();
    static void EndExecution();
    static void AddEndExecutionCallback( EndExecutionCallback func );

    static bool   PrepareContext( uint bind_id, const char* call_func, const char* ctx_info );
    static void   SetArgUChar( uchar value );
    static void   SetArgUShort( ushort value );
    static void   SetArgUInt( uint value );
    static void   SetArgUInt64( uint64 value );
    static void   SetArgBool( bool value );
    static void   SetArgFloat( float value );
    static void   SetArgDouble( double value );
    static void   SetArgObject( void* value );
    static void   SetArgAddress( void* value );
    static bool   RunPrepared();
    static void   SuspendCurrentContext( uint time );
    static void   RunSuspended();
    static uint   GetReturnedUInt();
    static bool   GetReturnedBool();
    static void*  GetReturnedObject();
    static float  GetReturnedFloat();
    static double GetReturnedDouble();
    static void*  GetReturnedRawAddress();

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
