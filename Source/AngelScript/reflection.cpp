#include "reflection.h"
#include "Script.h"
#include <assert.h>

uint ScriptType::GetLoadedModules( ScriptArray* modules )
{
#ifdef FONLINE_SCRIPT_COMPILER
    asIScriptContext* ctx = asGetActiveContext();
    if( !ctx )
        return ( 0 );

    if( modules )
    {
        asIScriptModule* module = (asIScriptModule*) ctx->GetUserData();
        modules->InsertLast( ScriptString::Create( module->GetName() ) );
    }

    return ( 1 );
#else
    EngineData* edata = (EngineData*) ( Script::GetEngine()->GetUserData() );
    if( !edata || !edata->Modules.size() )
        return ( 0 );

    uint result = 0;

    for( auto it = edata->Modules.begin(); it != edata->Modules.end(); ++it )
    {
        asIScriptModule* module = *it;
        if( modules )
            modules->InsertLast( ScriptString::Create( module->GetName() ) );
        result++;
    }

    return ( result );
#endif
}

ScriptString* ScriptType::GetName() const
{
    if( ObjType )
    {
        const char* ns = ObjType->GetNamespace();
        if( ns[ 0 ] )
        {
            ScriptString* str = ScriptString::Create( ns );
            str->append( "::" );
            str->append( ObjType->GetName() );
            return str;
        }
        return ScriptString::Create( ObjType->GetName() );
    }
    return ScriptString::Create( "(not assigned)" );
}

ScriptString* ScriptType::GetNameWithoutNamespace() const
{
    return ScriptString::Create( ObjType ? ObjType->GetName() : "(not assigned)" );
}

ScriptString* ScriptType::GetNamespace() const
{
    return ScriptString::Create( ObjType->GetNamespace() );
}

bool ScriptType::IsAssigned() const
{
    return ObjType != NULL;
}

bool ScriptType::Equals( const ScriptType& other )
{
    return ObjType == other.ObjType;
}

bool ScriptType::DerivesFrom( const ScriptType& other )
{
    return ( ObjType && other.ObjType ) ? ObjType->DerivesFrom( other.ObjType ) : false;
}

void ScriptType::Instanciate( void* out, int out_type_id ) const
{
    if( !ObjType )
    {
        asGetActiveContext()->SetException( "Type is not assigned" );
        return;
    }

    asIScriptEngine* engine = ObjType->GetEngine();

    if( !( out_type_id & asTYPEID_OBJHANDLE ) )
    {
        asGetActiveContext()->SetException( "Invalid 'instance' argument, not an handle" );
        return;
    }
    if( *(void**) out )
    {
        asGetActiveContext()->SetException( "Invalid 'instance' argument, handle must be null" );
        return;
    }
    if( !ObjType->DerivesFrom( engine->GetObjectTypeById( out_type_id ) ) )
    {
        asGetActiveContext()->SetException( "Invalid 'instance' argument, incompatible types" );
        return;
    }

    *(void**) out = engine->CreateScriptObject( ObjType );
}

void ScriptType::InstanciateCopy( void* in, int in_type_id, void* out, int out_type_id ) const
{
    if( !ObjType )
    {
        asGetActiveContext()->SetException( "Type is not assigned" );
        return;
    }

    asIScriptEngine* engine = ObjType->GetEngine();

    if( !( out_type_id & asTYPEID_OBJHANDLE ) )
    {
        asGetActiveContext()->SetException( "Invalid 'instance' argument, not an handle" );
        return;
    }
    if( *(void**) out )
    {
        asGetActiveContext()->SetException( "Invalid 'instance' argument, handle must be null" );
        return;
    }
    if( !ObjType->DerivesFrom( engine->GetObjectTypeById( out_type_id ) ) )
    {
        asGetActiveContext()->SetException( "Invalid 'instance' argument, incompatible types" );
        return;
    }

    if( !( in_type_id & asTYPEID_OBJHANDLE ) )
    {
        asGetActiveContext()->SetException( "Invalid 'copyFrom' argument, not an handle" );
        return;
    }
    if( !*(void**) in )
    {
        asGetActiveContext()->SetException( "Invalid 'copyFrom' argument, handle must be not null" );
        return;
    }
    in = *(void**) in;
    asIScriptObject* in_obj = (asIScriptObject*) in;
    if( in_obj->GetObjectType() != ObjType )
    {
        asGetActiveContext()->SetException( "Invalid 'copyFrom' argument, incompatible types" );
        return;
    }

    *(void**) out = engine->CreateScriptObjectCopy( in, ObjType );
}

ScriptString* ScriptType::ShowMembers()
{
    ScriptString* members = ScriptString::Create();
    if( ObjType->GetMethodCount() )
    {
        members->append( "Methods:\n" );
        for( asUINT i = 0, j = ObjType->GetMethodCount(); i < j; i++ )
        {
            members->append( ObjType->GetMethodByIndex( i, false )->GetDeclaration( false, false, true ) );
            members->append( "\n" );
        }
    }
    if( ObjType->GetPropertyCount() )
    {
        members->append( "Properties:\n" );
        for( asUINT i = 0, j = ObjType->GetPropertyCount(); i < j; i++ )
        {
            members->append( ObjType->GetPropertyDeclaration( i, false ) );
            members->append( "\n" );
        }
    }
    return members;
}

static bool ScriptTypeOfTemplateCallback( asIObjectType* ot, bool& dontGarbageCollect )
{
    if( ot->GetSubTypeId() & asTYPEID_OBJHANDLE )
        return false;
    return true;
}

static ScriptTypeOf* ScriptTypeOfFactory( asIObjectType* ot )
{
    return new ScriptTypeOf( ot->GetSubType( 0 ) );
}

static ScriptTypeOf* ScriptTypeOfFactory2( asIObjectType* ot, void* ref )
{
    return new ScriptTypeOf( ( (asIScriptObject*) ref )->GetObjectType() );
}

ScriptTypeOf::ScriptTypeOf( asIObjectType* ot )
{
    refCount = 1;
    ObjType = ot;
    ObjType->AddRef();
}

ScriptTypeOf::~ScriptTypeOf()
{
    ObjType->Release();
}

void ScriptTypeOf::AddRef() const
{
    asAtomicInc( refCount );
}

void ScriptTypeOf::Release() const
{
    if( asAtomicDec( refCount ) == 0 )
        delete this;
}

ScriptType ScriptTypeOf::ConvertToType() const
{
    ScriptType type;
    type.ObjType = ObjType;
    return type;
}

void RegisterMethod( asIScriptEngine* engine, const char* declaration, const asSFuncPtr& funcPointer )
{
    engine->RegisterObjectMethod( "type", declaration, funcPointer, asCALL_THISCALL );
    engine->RegisterObjectMethod( "typeof<T>", declaration, funcPointer, asCALL_THISCALL );
}

void RegisterScriptReflection( asIScriptEngine* engine )
{
    engine->SetDefaultNamespace( "reflection" );

    engine->RegisterObjectType( "type", sizeof( ScriptType ), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS );

    engine->RegisterObjectType( "typeof<class T>", sizeof( ScriptTypeOf ), asOBJ_REF | asOBJ_TEMPLATE );
    engine->RegisterObjectBehaviour( "typeof<T>", asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", asFUNCTION( ScriptTypeOfTemplateCallback ), asCALL_CDECL );
    engine->RegisterObjectBehaviour( "typeof<T>", asBEHAVE_FACTORY, "typeof<T>@ f(int&in)", asFUNCTION( ScriptTypeOfFactory ), asCALL_CDECL );
    engine->RegisterObjectBehaviour( "typeof<T>", asBEHAVE_FACTORY, "typeof<T>@ f(int&in, const T&in)", asFUNCTION( ScriptTypeOfFactory2 ), asCALL_CDECL );
    engine->RegisterObjectBehaviour( "typeof<T>", asBEHAVE_ADDREF, "void f()", asMETHOD( ScriptTypeOf, AddRef ), asCALL_THISCALL );
    engine->RegisterObjectBehaviour( "typeof<T>", asBEHAVE_RELEASE, "void f()", asMETHOD( ScriptTypeOf, Release ), asCALL_THISCALL );
    engine->RegisterObjectBehaviour( "typeof<T>", asBEHAVE_IMPLICIT_VALUE_CAST, "type f()", asMETHOD( ScriptTypeOf, ConvertToType ), asCALL_THISCALL );

    engine->RegisterGlobalFunction( "getLoadedModules(string[]@+ modules)", asFUNCTION(ScriptType::GetLoadedModules), asCALL_CDECL );

    RegisterMethod( engine, "string@ get_name() const", asMETHOD( ScriptType, GetName ) );
    RegisterMethod( engine, "string@ get_nameWithoutNamespace() const", asMETHOD( ScriptType, GetNameWithoutNamespace ) );
    RegisterMethod( engine, "string@ get_namespace() const", asMETHOD( ScriptType, GetNamespace ) );
    RegisterMethod( engine, "bool get_assigned() const", asMETHOD( ScriptType, IsAssigned ) );
    RegisterMethod( engine, "bool opEquals(const type&in other) const", asMETHOD( ScriptType, Equals ) );
    RegisterMethod( engine, "bool derivesFrom(const type&in other) const", asMETHOD( ScriptType, DerivesFrom ) );
    RegisterMethod( engine, "void instantiate(?&out instance) const", asMETHOD( ScriptType, Instanciate ) );
    RegisterMethod( engine, "void instantiate(?&in copyFrom, ?&out instance) const", asMETHOD( ScriptType, InstanciateCopy ) );
    RegisterMethod( engine, "string@ showMembers() const", asMETHOD( ScriptType, ShowMembers ) );

    engine->SetDefaultNamespace( "" );
}
