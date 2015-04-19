#ifndef __SCRIPT__
#define __SCRIPT__

#include "Common.h"
#include "ScriptPragmas.h"
#include "angelscript.h"
#include "scriptarray.h"
#include "scriptstring.h"
#include "scriptdictionary.h"
#include "preprocessor.h"
#include <vector>
#include <string>

#define GLOBAL_CONTEXT_STACK_SIZE    ( 10 )
#define CONTEXT_BUFFER_SIZE          ( 512 )

#ifndef FONLINE_SCRIPT_COMPILER
# define SCRIPT_ERROR_R( error, ... )     do { Script::RaiseException( error, ## __VA_ARGS__ ); return; } while( 0 )
# define SCRIPT_ERROR_R0( error, ... )    do { Script::RaiseException( error, ## __VA_ARGS__ ); return 0; } while( 0 )
#else
# define SCRIPT_ERROR_R( error, ... )     (void) 0
# define SCRIPT_ERROR_R0( error, ... )    (void) 0
#endif

typedef void ( *EndExecutionCallback )();

struct EngineData
{
    ScriptPragmaCallback*                PragmaCB;
    string                               DllTarget;
    bool                                 AllowNativeCalls;
    map< string, pair< string, void* > > LoadedDlls;
};

struct ReservedScriptFunction
{
    int* BindId;
    char FuncName[ 256 ];
    char FuncDecl[ 256 ];
};

namespace Script
{
    bool Init( ScriptPragmaCallback* pragma_callback, const char* dll_target, bool allow_native_calls );
    void Finish();
    bool InitThread();
    void FinishThread();

    void* LoadDynamicLibrary( const char* dll_name );
    void  SetWrongGlobalObjects( StrVec& names );
    void  SetConcurrentExecution( bool enabled );
    void  SetLoadLibraryCompiler( bool enabled );

    void UnloadScripts();
    bool ReloadScripts( const char* config, const char* key, bool skip_binaries, const char* file_pefix = NULL );
    bool BindReservedFunctions( ReservedScriptFunction* bind_func, uint bind_func_count );
    bool RunModuleInitFunctions();

    #ifdef FONLINE_SERVER
    namespace Profiler
    {
        void   SetData( uint sample_time, uint save_time, bool dynamic_display );
        void   Init();
        void   AddModule( const char* module_name );
        void   EndModules();
        void   SaveFunctionsData();
        void   Finish();
        string GetStatistics();
        bool   IsActive();
    }
    #endif

    void DummyAddRef( void* );
    void DummyRelease( void* );

    asIScriptEngine* GetEngine();
    void             SetEngine( asIScriptEngine* engine );
    asIScriptEngine* CreateEngine( ScriptPragmaCallback* pragma_callback, const char* dll_target, bool allow_native_calls );
    void             FinishEngine( asIScriptEngine*& engine );

    asIScriptContext* CreateContext();
    void              FinishContext( asIScriptContext*& ctx );
    asIScriptContext* GetGlobalContext();
    void              PrintContextCallstack( asIScriptContext* ctx );
    void              RaiseException( const char* message, ... );

    const char*      GetActiveModuleName();
    const char*      GetActiveFuncName();
    asIScriptModule* GetModule( const char* name );
    asIScriptModule* CreateModule( const char* module_name );

    void SetRunTimeout( uint suspend_timeout, uint message_timeout );

    void SetScriptsPath( int path_type );
    void Define( const char* def, ... );
    void Undef( const char* def );
    void CallPragmas( const Pragmas& pragmas );
    bool LoadScript( const char* module_name, const char* source, bool skip_binary, const char* file_prefix = NULL );
    bool LoadScript( const char* module_name, const uchar* bytecode, uint len );

    int    BindImportedFunctions();
    int    Bind( const char* module_name, const char* func_name, const char* decl, bool is_temp, bool disable_log = false );
    int    Bind( const char* script_name, const char* decl, bool is_temp, bool disable_log = false );
    int    Bind( asIScriptFunction* func, bool is_temp, bool disable_log = false );
    int    RebindFunctions();
    bool   ReparseScriptName( const char* script_name, char* module_name, char* func_name, bool disable_log = false );
    string GetBindFuncName( int bind_id );

    hash   BindScriptFuncNum( const char* script_name, const char* decl );
    hash   BindScriptFuncNum( asIScriptFunction* func );
    bool   PrepareScriptFuncContext( hash func_num, const char* call_func, const char* ctx_info );
    string GetScriptFuncName( hash func_num );

    // Script execution
    void BeginExecution();
    void EndExecution();
    void AddEndExecutionCallback( EndExecutionCallback func );

    bool   PrepareContext( int bind_id, const char* call_func, const char* ctx_info );
    void   SetArgUChar( uchar value );
    void   SetArgUShort( ushort value );
    void   SetArgUInt( uint value );
    void   SetArgUInt64( uint64 value );
    void   SetArgBool( bool value );
    void   SetArgFloat( float value );
    void   SetArgDouble( double value );
    void   SetArgObject( void* value );
    void   SetArgAddress( void* value );
    bool   RunPrepared();
    uint   GetReturnedUInt();
    bool   GetReturnedBool();
    void*  GetReturnedObject();
    float  GetReturnedFloat();
    double GetReturnedDouble();
    void*  GetReturnedRawAddress();

    bool SynchronizeThread();
    bool ResynchronizeThread();

    // Logging
    void Log( const char* str );
    void LogA( const char* str );
    void LogError( const char* call_func, const char* error );
    void SetLogDebugInfo( bool enabled );

    void CallbackMessage( const asSMessageInfo* msg, void* param );
    void CallbackException( asIScriptContext* ctx, void* param );

    // Arrays stuff
    ScriptArray* CreateArray( const char* type );

    template< typename Type >
    void AppendVectorToArray( const vector< Type >& vec, ScriptArray* arr )
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
    void AppendVectorToArrayRef( const vector< Type >& vec, ScriptArray* arr )
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
    void AssignScriptArrayInVector( vector< Type >& vec, const ScriptArray* arr )
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
}

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
