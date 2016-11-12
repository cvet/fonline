#include "Common.h"
#include "Methods.h"
#include "Entity.h"
#include "Script.h"

Method::Method()
{
    callType = (CallType) 0;
    registrator = nullptr;
    regIndex = 0;
    callbackBindId = 0;
}

void Method::Wrap( asIScriptGeneric* gen )
{
    Method*  self = nullptr;    // gen->GetObjForThiscall();
    Entity*  entity = (Entity*) gen->GetObject();
    Methods* methods = &entity->Meths;

    UIntVec  bind_ids;
    bind_ids.push_back( self->callbackBindId );
    bind_ids.insert( bind_ids.end(), self->watcherBindIds.begin(), self->watcherBindIds.end() );
    bind_ids.insert( bind_ids.end(), methods->watcherBindIds.begin(), methods->watcherBindIds.end() );

    for( size_t cbk = 0; cbk < bind_ids.size(); cbk++ )
    {
        Script::PrepareContext( bind_ids[ cbk ], "Method" );

        // First arg is entity
        Script::SetArgEntity( entity );

        // Arguments
        int arg_count = gen->GetArgCount();
        for( int i = 1; i < arg_count; i++ )
            Script::SetArgAddress( gen->GetArgAddress( i ) );

        // Run
        if( cbk == 0 )
        {
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
                Script::PassException();
                break;
            }
        }
        else
        {
            Script::RunPrepared();
        }

        if( entity->IsDestroyed )
            break;
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

MethodRegistrator::MethodRegistrator( bool is_server, const char* class_name )
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

Method* MethodRegistrator::Register( const char* decl, const char* bind_func, Method::CallType call )
{
    if( registrationFinished )
    {
        WriteLog( "Registration of class properties is finished.\n" );
        return nullptr;
    }

    // Get engine
    asIScriptEngine* engine = Script::GetEngine();
    RUNTIME_ASSERT( engine );

    // Register enum

//      char enum_type[ MAX_FOTEXT ];
//      Str::Format( enum_type, "%sFunc",  )
//      bool is_client = (class_name[ Str::Length( class_name ) - 2 ] == 'C' && class_name[ Str::Length( class_name ) - 1 ] == 'l');
//      RUNTIME_ASSERT(enumTypeName.length() > 0);
//
//      engine->RegisterFuncdef( "void CallFunc()" );
//
//      int result = engine->RegisterEnum(enum_type.c_str());
//      if (result < 0)
//      {
//              WriteLog("Register object property enum '{}' fail, error {}.\n", enum_type.c_str(), result);
//              return false;
//      }
//
//      result = engine->RegisterEnumValue(enum_type.c_str(), "Invalid", 0);
//      if (result < 0)
//      {
//              WriteLog("Register object property enum '{}::Invalid' zero value fail, error {}.\n", enum_type.c_str(), result);
//              return false;
//      }

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
        // void DonThisThing(int a, int b)
        // funcdef void DonThisThingFunc(int a, int b)
        // void SetWatcher_DoThisThing( DonThisThingFunc@ func )

        int result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( Method::Wrap ), asCALL_GENERIC, method );
        if( result < 0 )
        {
            WriteLog( "Register entity method '{}' fail, error {}.\n", decl, result );
            return nullptr;
        }

        asIScriptFunction* func = engine->GetFunctionById( result );
        char               bind_decl[ MAX_FOTEXT ];
        Str::Copy( bind_decl, decl );
        Str::ReplaceText( bind_decl, func->GetName(), "%s" );
        char* args = Str::Substring( bind_decl, "(" );
        Str::Insert( args + 1, Str::FormatBuf( "%s&%s", scriptClassName.c_str(), func->GetParamCount() > 0 ? ", " : "" ) );
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
        RUNTIME_ASSERT( method->bindFunc.length() > 0 );
        method->callbackBindId = Script::BindByFuncName( method->bindFunc.c_str(), method->bindDecl.c_str(), false );
        if( !method->callbackBindId )
            errors++;
    }
    #endif
    return errors == 0;
}

Method* MethodRegistrator::Get( uint reg_index )
{
    if( reg_index < (uint) registeredMethods.size() )
        return registeredMethods[ reg_index ];
    return nullptr;
}
