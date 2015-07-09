#include "Common.h"
#include "Methods.h"
#include "Script.h"

Method::Method()
{
    callType = (CallType) 0;
    registrator = NULL;
    regIndex = 0;
}

void Method::Wrap( asIScriptGeneric* gen )
{
    Method* self = NULL;    // gen->GetObjForThiscall();
    UIntVec bind_ids = self->callbackBindIds;
    for( size_t cbk = 0; cbk < bind_ids.size(); cbk++ )
    {
        if( Script::PrepareContext( bind_ids[ cbk ], _FUNC_, "Method" ) )
        {
            // First arg is object
            Script::SetArgObject( gen->GetObject() );

            // Arguments
            int arg_count = gen->GetArgCount();
            for( int i = 1; i < arg_count; i++ )
                Script::SetArgAddress( gen->GetArgAddress( i ) );

            // Run
            if( Script::RunPrepared() )
            {
                int type_id = gen->GetReturnTypeId();
                if( cbk == 0 && type_id != asTYPEID_VOID )
                {
                    RUNTIME_ASSERT( false );
                    // gen->SetReturnAddress( Script::GetReturnedRawAddress() );
                }
            }
            else
            {
                if( cbk == 0 )
                {
                    Script::PassException();
                    break;
                }
            }
        }
    }
}

const char* Method::GetDecl()
{
    return methodDecl.c_str();
}

uint Method::GetRegIndex()
{
    return regIndex;
}

Method::CallType Method::GetCallType()
{
    return callType;
}

string Method::SetMainCallback( const char* script_func )
{
    return "";
}

string Method::AddWatcherCallback( const char* script_func )
{
    return "";
}

MethodRegistrator::MethodRegistrator( bool is_server )
{
    registrationFinished = false;
    isServer = is_server;
}

MethodRegistrator::~MethodRegistrator()
{
    //
}

bool MethodRegistrator::Init()
{
    return true;
}

Method* MethodRegistrator::Register( const char* class_name, const char* decl, const char* bind_func, Method::CallType call )
{
    if( registrationFinished )
    {
        WriteLogF( _FUNC_, " - Registration of class properties is finished.\n" );
        return NULL;
    }

    // Get engine
    asIScriptEngine* engine = Script::GetEngine();
    RUNTIME_ASSERT( engine );

    // Allocate method
    Method* method = new Method();
    uint    reg_index = (uint) registeredMethods.size();

    // Disallow registering
    bool disable_registration = false;
    if( isServer && !( call & Method::ServerMask ) )
        disable_registration = true;
    if( !isServer && !( call & Method::ClientMask ) )
        disable_registration = true;
    #ifdef FONLINE_MAPPER
    disable_registration = false;
    #endif

    // Register
    if( !disable_registration )
    {
        int result = engine->RegisterObjectMethod( class_name, decl, asFUNCTION( Method::Wrap ), asCALL_GENERIC, method );
        if( result < 0 )
        {
            WriteLogF( _FUNC_, " - Register object method '%s' fail, error %d.\n", decl, result );
            return NULL;
        }

        asIScriptFunction* func = engine->GetFunctionById( result );
        char               bind_decl[ MAX_FOTEXT ];
        Str::Copy( bind_decl, decl );
        Str::ReplaceText( bind_decl, func->GetName(), "%s" );
        char* args = Str::Substring( bind_decl, "(" );
        Str::Insert( args + 1, Str::FormatBuf( "%s&%s", class_name, func->GetParamCount() > 0 ? ", " : "" ) );
        method->bindFunc = bind_func;
        method->bindDecl = bind_decl;
    }

    // Make entry
    method->registrator = this;
    method->regIndex = reg_index;
    method->methodDecl = decl;
    method->callType = call;
    registeredMethods.push_back( method );
    return method;
}

bool MethodRegistrator::FinishRegistration()
{
    int errors = 0;
    #ifndef FONLINE_SCRIPT_COMPILER
    for( size_t i = 0; i < registeredMethods.size(); i++ )
    {
        Method* method = registeredMethods[ i ];
        if( method->bindFunc.length() > 0 )
        {
            uint bind_id = Script::Bind( method->bindFunc.c_str(), method->bindDecl.c_str(), false );
            if( bind_id )
                method->callbackBindIds.push_back( bind_id );
            else
                errors++;
        }
    }
    #endif
    return errors == 0;
}

Method* MethodRegistrator::Get( uint reg_index )
{
    if( reg_index < (uint) registeredMethods.size() )
        return registeredMethods[ reg_index ];
    return NULL;
}
