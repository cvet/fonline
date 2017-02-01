#ifndef __SCRIPT__
#define __SCRIPT__

#include "Common.h"
#include "ScriptPragmas.h"
#include "ScriptInvoker.h"
#include "ScriptProfiler.h"
#include "angelscript.h"
#include "preprocessor.h"
#include <vector>
#include <string>

#ifdef AS_MAX_PORTABILITY
# include "AngelScript/sdk/add_on/autowrapper/aswrappedcall.h"
# define SCRIPT_FUNC( name )             WRAP_FN( name )
# define SCRIPT_FUNC_THIS( name )        WRAP_OBJ_FIRST( name )
# define SCRIPT_METHOD( type, name )     WRAP_MFN( type, name )
# define SCRIPT_FUNC_CONV         asCALL_GENERIC
# define SCRIPT_FUNC_THIS_CONV    asCALL_GENERIC
# define SCRIPT_METHOD_CONV       asCALL_GENERIC
#else
# define SCRIPT_FUNC( name )             asFUNCTION( name )
# define SCRIPT_FUNC_THIS( name )        asFUNCTION( name )
# define SCRIPT_METHOD( type, name )     asMETHOD( type, name )
# define SCRIPT_FUNC_CONV         asCALL_CDECL
# define SCRIPT_FUNC_THIS_CONV    asCALL_CDECL_OBJFIRST
# define SCRIPT_METHOD_CONV       asCALL_THISCALL
#endif

#define SCRIPT_ERROR_R( error, ... )     do { Script::RaiseException( error, ## __VA_ARGS__ ); return; } while( 0 )
#define SCRIPT_ERROR_R0( error, ... )    do { Script::RaiseException( error, ## __VA_ARGS__ ); return 0; } while( 0 )

typedef void ( *EndExecutionCallback )();
typedef vector< asIScriptContext* >           ContextVec;
typedef std::function< void ( const char* ) > ExceptionCallback;

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

class Script
{
public:
    static bool Init( ScriptPragmaCallback* pragma_callback, const char* dll_target, bool allow_native_calls, uint profiler_sample_time, bool profiler_save_to_file, bool profiler_dynamic_display );
    static void Finish();

    static void* LoadDynamicLibrary( const char* dll_name );
    static void  SetLoadLibraryCompiler( bool enabled );

    static void UnloadScripts();
    static bool ReloadScripts( const char* target );
    static bool PostInitScriptSystem();
    static bool RunModuleInitFunctions();

    static asIScriptEngine* GetEngine();
    static void             SetEngine( asIScriptEngine* engine );
    static asIScriptEngine* CreateEngine( ScriptPragmaCallback* pragma_callback, const char* dll_target, bool allow_native_calls );
    static void             RegisterScriptArrayExtensions( asIScriptEngine* engine );
    static void             RegisterScriptDictExtensions( asIScriptEngine* engine );
    static void             RegisterScriptStdStringExtensions( asIScriptEngine* engine );
    static void             FinishEngine( asIScriptEngine*& engine );

    static void              CreateContext();
    static void              FinishContext( asIScriptContext* ctx );
    static asIScriptContext* RequestContext();
    static void              ReturnContext( asIScriptContext* ctx );

    static void   SetExceptionCallback( ExceptionCallback callback );
    static void   RaiseException( const char* message, ... );
    static void   PassException();
    static void   HandleException( asIScriptContext* ctx, const char* message, ... );
    static string GetTraceback();
    static string MakeContextTraceback( asIScriptContext* ctx );

    static ScriptInvoker* GetInvoker();
    static string         GetDeferredCallsStatistics();
    static void           ProcessDeferredCalls();
    static void           SaveDeferredCalls( IniParser& data );
    static bool           LoadDeferredCalls( IniParser& data );

    static void   ProfilerContextCallback( asIScriptContext* ctx, void* obj );
    static string GetProfilerStatistics();

    static bool RestoreEntity( const char* class_name, uint id, const StrMap& props_data );

    static void* FindInternalEvent( const char* event_name );
    static bool  RaiseInternalEvent( void* event_ptr, ... );
    static void  RemoveEventsEntity( Entity* entity );

    static void HandleRpc( void* context );

    static const char* GetActiveFuncName();

    static void Watcher( void* );
    static void SetRunTimeout( uint abort_timeout, uint message_timeout );

    static void Define( const char* def, ... );
    static void Undef( const char* def );
    static void CallPragmas( const Pragmas& pragmas );
    static bool LoadRootModule( StrVec& names, StrVec& contents, string& result_code );
    static bool RestoreRootModule( const UCharVec& bytecode, const UCharVec& lnt_data );

    static uint               BindByFuncName( const char* func_name, const char* decl, bool is_temp, bool disable_log = false );
    static uint               BindByFunc( asIScriptFunction* func, bool is_temp, bool disable_log = false );
    static uint               BindByFuncNum( hash func_num, bool is_temp, bool disable_log = false );
    static asIScriptFunction* GetBindFunc( uint bind_id );
    static string             GetBindFuncName( uint bind_id );

    static hash               GetFuncNum( asIScriptFunction* func );
    static asIScriptFunction* FindFunc( hash func_num );
    static hash               BindScriptFuncNumByFuncName( const char* func_name, const char* decl );
    static hash               BindScriptFuncNumByFunc( asIScriptFunction* func );
    static uint               GetScriptFuncBindId( hash func_num );
    static void               PrepareScriptFuncContext( hash func_num, const char* ctx_info );

    static void        CacheEnumValues();
    static int         GetEnumValue( const char* enum_value_name, bool& fail );
    static int         GetEnumValue( const char* enum_name, const char* value_name, bool& fail );
    static const char* GetEnumValueName( const char* enum_name, int value );

    // Script execution
    static void              PrepareContext( uint bind_id, const char* ctx_info );
    static void              SetArgUChar( uchar value );
    static void              SetArgUShort( ushort value );
    static void              SetArgUInt( uint value );
    static void              SetArgUInt64( uint64 value );
    static void              SetArgBool( bool value );
    static void              SetArgFloat( float value );
    static void              SetArgDouble( double value );
    static void              SetArgObject( void* value );
    static void              SetArgEntity( Entity* value );
    static void              SetArgAddress( void* value );
    static bool              RunPrepared();
    static void              RunPreparedSuspend();
    static asIScriptContext* SuspendCurrentContext( uint time );
    static void              ResumeContext( asIScriptContext* ctx );
    static void              RunSuspended();
    static void              RunMandatorySuspended();
    static bool              CheckContextEntities( asIScriptContext* ctx );
    static uint              GetReturnedUInt();
    static bool              GetReturnedBool();
    static void*             GetReturnedObject();
    static float             GetReturnedFloat();
    static double            GetReturnedDouble();
    static void*             GetReturnedRawAddress();

    // Logging
    static void Log( const char* str );
    static void LogA( const char* str );
    static void LogError( const char* call_func, const char* error );

    static void CallbackMessage( const asSMessageInfo* msg, void* param );
    static void CallbackException( asIScriptContext* ctx, void* param );

    // Arrays stuff
    static CScriptArray* CreateArray( const char* type );

    template< typename Type >
    static CScriptArray* CreateArrayRef( const char* type, const vector< Type* >& vec )
    {
        CScriptArray* arr = CreateArray( type );
        AppendVectorToArrayRef( vec, arr );
        return arr;
    }

    template< typename Type >
    static void AppendVectorToArray( const vector< Type >& vec, CScriptArray* arr )
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
    static void AppendVectorToArrayRef( const vector< Type >& vec, CScriptArray* arr )
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
    static void AssignScriptArrayInVector( vector< Type >& vec, const CScriptArray* arr )
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
