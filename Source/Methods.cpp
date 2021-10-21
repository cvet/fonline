#include "Common.h"
#include "Methods.h"
#include "Entity.h"
#include "Script.h"

extern bool as_builder_ForceAutoHandles;

Method::Method()
{
    callType = (CallType) 0;
    registrator = nullptr;
    regIndex = 0;
    callbackBindId = 0;
}

void Method::FakeWrap( asIScriptGeneric* gen )
{

}

void Method::Wrap( asIScriptGeneric* gen )
{
    Method*  self = (Method*)gen->GetAuxiliary();
    Entity*  entity = (Entity*) gen->GetObject();
    Methods* methods = &entity->Meths;

    UIntVec  bind_ids;
    bind_ids.push_back( self->callbackBindId );
    bind_ids.insert( bind_ids.end(), self->watcherBindIds.begin(), self->watcherBindIds.end() );
    bind_ids.insert( bind_ids.end(), methods->watcherBindIds.begin(), methods->watcherBindIds.end() );

    for( size_t cbk = 0; cbk < bind_ids.size(); cbk++ )
    {
        Script::PrepareContext( bind_ids[ cbk ], "Method" );
		auto engine = Script::GetEngine();
		for (size_t index = 0; index < Script::GetCountArg(); index++)
		{
			if (index == 0)
			{
				Script::SetArgEntity(entity);
				continue;
			}
			asUINT i = (asUINT)index - 1;
			void* value = gen->GetAddressOfArg(i);
			int type_id = gen->GetArgTypeId(i);
			asITypeInfo* obj_type = (type_id & asTYPEID_MASK_OBJECT ? engine->GetTypeInfoById(type_id) : nullptr);
			if (!(type_id & asTYPEID_MASK_OBJECT))
			{
				int size = engine->GetSizeOfPrimitiveType(type_id);
				if (size == 1)
					Script::SetArgUChar(*(uchar*)value);
				else if (size == 2)
					Script::SetArgUShort(*(ushort*)value);
				else if (size == 4)
					Script::SetArgUInt(*(uint*)value);
				else if (size == 8)
					Script::SetArgUInt64(*(uint64*)value);
				else
					RUNTIME_ASSERT(!"Unreachable place");
			}
			else
			{
				if (type_id & asTYPEID_OBJHANDLE)
				{
					if (*(void**)value)
						engine->AddRefScriptObject(*(void**)value, obj_type);
					Script::SetArgObject(*(void**)value);
				}
				else
				{
					engine->AddRefScriptObject(value, obj_type);
					Script::SetArgObject(value);
				}
			}
		}
        // Run
        if( cbk == 0 )
        {
            if( Script::RunPrepared() )
            {
                int type_id = gen->GetReturnTypeId();
                if( cbk == 0 && type_id != asTYPEID_VOID )
                {
					void* ptr = Script::GetReturnedRawAddress();
					switch (type_id)
					{
					case asTYPEID_INT8:
					case asTYPEID_UINT8:
					case asTYPEID_BOOL:
						gen->SetReturnByte(*(unsigned char*)ptr);
						break;
					case asTYPEID_INT16:
					case asTYPEID_UINT16:
						gen->SetReturnWord(*(unsigned short*)ptr);
						break;
					case asTYPEID_INT32:
					case asTYPEID_UINT32:
						gen->SetReturnDWord(*(unsigned int*)ptr);
						break;
					case asTYPEID_INT64:
					case asTYPEID_UINT64:
						gen->SetReturnQWord(*(unsigned long long*)ptr);
						break;
					case asTYPEID_FLOAT:
						gen->SetReturnFloat(*(float*)ptr);
						break;
					case asTYPEID_DOUBLE:
						gen->SetReturnDouble(*(double*)ptr);
						break;
					default:
						if (type_id & asTYPEID_MASK_OBJECT)
						{
							void *ptr = Script::GetReturnedObject();
							auto type = engine->GetTypeInfoById(type_id);
							engine->AddRefScriptObject(ptr, type);
							gen->SetReturnObject(ptr);
						}
						else
						{
							RUNTIME_ASSERT(!"Unreachable place");
						}
						break;
					}
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
	scriptClassName = class_name;
}

MethodRegistrator::~MethodRegistrator()
{
    //
}

bool MethodRegistrator::Init()
{
    return true;
}

Method* MethodRegistrator::Register( const char* decl, const char* bind_func, Method::CallType call, const string& cur_file)
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
//      X_str( enum_type, "%sFunc",  )
//      bool is_client = (class_name[ Str::Length( class_name ) - 2 ] == 'C' && class_name[ Str::Length( class_name ) - 1 ] == 'l');
//      RUNTIME_ASSERT(enumTypeName.length() > 0);
//
//      engine->RegisterFuncdef( "void CallFunc()" );
//
//      int result = engine->RegisterEnum(enum_type.c_str());
//      if (result < 0)
//      {
//              WriteLog("Register object property enum '{}' fail, error {}.\n", enum_type, result);
//              return false;
//      }
//
//      result = engine->RegisterEnumValue(enum_type.c_str(), "Invalid", 0);
//      if (result < 0)
//      {
//              WriteLog("Register object property enum '{}::Invalid' zero value fail, error {}.\n", enum_type, result);
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
		// WriteLog("'{}' '{}'.\n", scriptClassName, decl);
		as_builder_ForceAutoHandles = true;
        int result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( Method::Wrap ), asCALL_GENERIC, method );
        if( result < 0 )
        {
            WriteLog( "Register entity method '{}' fail, error {}.\n", decl, result );
			as_builder_ForceAutoHandles = false;
            return nullptr;
        }
		as_builder_ForceAutoHandles = false;

        asIScriptFunction* func = engine->GetFunctionById( result );
        string             bind_decl = _str( decl ).replace( func->GetName(), "%s" );
        size_t             args_pos = bind_decl.find( '(' );
        bind_decl.insert( args_pos + 1, _str( "{} {}", scriptClassName, func->GetParamCount() > 0 ? ", " : "" ) );
        method->bindFunc = bind_func;
        method->bindDecl = bind_decl;
    }

    // Make entry
	method->fileName = cur_file;
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
	as_builder_ForceAutoHandles = true;
    for( size_t i = 0; i < registeredMethods.size(); i++ )
    {
        Method* method = registeredMethods[ i ];
        RUNTIME_ASSERT( method->bindFunc.length() > 0 );
        method->callbackBindId = Script::BindByFuncName( _str( "{}::{}", method->fileName, method->bindFunc ).str(), method->bindDecl, false );
        if( !method->callbackBindId )
            errors++;
    }
	as_builder_ForceAutoHandles = false;
    #endif
    return errors == 0;
}

Method* MethodRegistrator::Get( uint reg_index )
{
    if( reg_index < (uint) registeredMethods.size() )
        return registeredMethods[ reg_index ];
    return nullptr;
}
