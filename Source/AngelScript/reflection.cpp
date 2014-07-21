#include "reflection.h"
#include "Script.h"
#include <assert.h>
#include "as_objecttype.h"

ScriptType::ScriptType( asIObjectType* type )
{
    ObjType = type;
}

ScriptString* ScriptType::GetName() const
{
    if( !ObjType )
        return ScriptString::Create( "(not assigned)" );

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

ScriptString* ScriptType::GetNameWithoutNamespace() const
{
    if( !ObjType )
        return ScriptString::Create( "(not assigned)" );
    return ScriptString::Create( ObjType->GetName() );
}

ScriptString* ScriptType::GetNamespace() const
{
    if( !ObjType )
        return ScriptString::Create();
    return ScriptString::Create( ObjType->GetNamespace() );
}

ScriptString* ScriptType::GetModule() const
{
    if( !ObjType )
        return ScriptString::Create();
    return ScriptString::Create( ObjType->GetModule() ? ObjType->GetModule()->GetName() : "(global)" );
}

uint ScriptType::GetSize() const
{
    return ObjType ? ObjType->GetSize() : 0;
}

bool ScriptType::IsAssigned() const
{
    return ObjType != NULL;
}

bool ScriptType::IsGlobal() const
{
    return ObjType != NULL ? ObjType->GetModule() == NULL : false;
}

bool ScriptType::IsClass() const
{
    return ObjType != NULL ? ( !IsInterface() && !IsEnum() && !IsFunction() ) : false;
}

bool ScriptType::IsInterface() const
{
    return ObjType ? ( (asCObjectType*) ObjType )->IsInterface() : false;
}

bool ScriptType::IsEnum() const
{
    return ObjType ? ( (asCObjectType*) ObjType )->enumValues.GetLength() > 0 : false;
}

bool ScriptType::IsFunction() const
{
    return ObjType ? !strcmp( ObjType->GetName(), "_builtin_function_" ) : false;
}

bool ScriptType::IsShared() const
{
    return ObjType ? ( (asCObjectType*) ObjType )->IsShared() : false;
}

ScriptType ScriptType::GetBaseType() const
{
    return ScriptType( ObjType ? ObjType->GetBaseType() : NULL );
}

uint ScriptType::GetInterfaceCount() const
{
    if( !ObjType )
        return 0;
    return ObjType->GetInterfaceCount();
}

ScriptType ScriptType::GetInterface( uint index ) const
{
    return ScriptType( ( ObjType && index < ObjType->GetInterfaceCount() ) ? ObjType->GetInterface( index ) : NULL );
}

bool ScriptType::Implements( const ScriptType& other ) const
{
    if( !ObjType || !other.ObjType )
        return false;
    return ObjType->Implements( other.ObjType );
}

bool ScriptType::Equals( const ScriptType& other )
{
    return ObjType == other.ObjType;
}

bool ScriptType::DerivesFrom( const ScriptType& other )
{
    return ( ObjType && other.ObjType ) ? ObjType->DerivesFrom( other.ObjType ) : false;
}

void ScriptType::Instantiate( void* out, int out_type_id ) const
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

void ScriptType::InstantiateCopy( void* in, int in_type_id, void* out, int out_type_id ) const
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

uint ScriptType::GetMethodsCount() const
{
    if( !ObjType )
        return 0;
    return ObjType->GetMethodCount();
}

ScriptString* ScriptType::GetMethodDeclaration( uint index, bool include_object_name, bool include_namespace, bool include_param_names ) const
{
    if( !ObjType || index >= ObjType->GetMethodCount() )
        return ScriptString::Create( "" );
    return ScriptString::Create( ObjType->GetMethodByIndex( index )->GetDeclaration( include_object_name, include_namespace, include_param_names ) );
}

uint ScriptType::GetPropertiesCount() const
{
    if( !ObjType )
        return 0;
    return ObjType->GetPropertyCount();
}

ScriptString* ScriptType::GetPropertyDeclaration( uint index, bool include_namespace ) const
{
    if( !ObjType || index >= ObjType->GetPropertyCount() )
        return ScriptString::Create( "" );
    return ScriptString::Create( ObjType->GetPropertyDeclaration( index, include_namespace ) );
}

uint ScriptType::GetEnumLength() const
{
    asCObjectType* objTypeExt = (asCObjectType*) ObjType;
    return (uint) objTypeExt->enumValues.GetLength();
}

ScriptArray* ScriptType::GetEnumNames() const
{
    ScriptArray*   result = ScriptArray::Create( asGetActiveContext()->GetEngine()->GetObjectTypeByDecl( "string[]" ) );
    asCObjectType* objTypeExt = (asCObjectType*) ObjType;
    for( size_t i = 0, j = objTypeExt->enumValues.GetLength(); i < j; i++ )
        result->InsertLast( ScriptString::Create( objTypeExt->enumValues[ i ]->name.AddressOf() ) );
    return result;
}

ScriptArray* ScriptType::GetEnumValues() const
{
    ScriptArray*   result = ScriptArray::Create( asGetActiveContext()->GetEngine()->GetObjectTypeByDecl( "int[]" ) );
    asCObjectType* objTypeExt = (asCObjectType*) ObjType;
    for( size_t i = 0, j = objTypeExt->enumValues.GetLength(); i < j; i++ )
        result->InsertLast( &objTypeExt->enumValues[ i ]->value );
    return result;
}

static bool ScriptTypeOfTemplateCallback( asIObjectType* ot, bool& )
{
    if( ot->GetSubTypeCount() != 1 || ot->GetSubTypeId() & asTYPEID_OBJHANDLE || ( ot->GetSubTypeId() & asTYPEID_MASK_SEQNBR ) < asTYPEID_DOUBLE )
        return false;
    return true;
}

static ScriptTypeOf* ScriptTypeOfFactory( asIObjectType* ot )
{
    return new ScriptTypeOf( ot->GetSubType() );
}

static ScriptTypeOf* ScriptTypeOfFactory2( asIObjectType* ot, void* ref )
{
    return new ScriptTypeOf( ( (asIScriptObject*) ref )->GetObjectType() );
}

ScriptTypeOf::ScriptTypeOf( asIObjectType* ot ): ScriptType( ot )
{
    refCount = 1;
}

ScriptTypeOf::~ScriptTypeOf()
{
    //
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
    return ScriptType( ObjType );
}

ScriptArray* GetLoadedModules()
{
    asIScriptContext* ctx = asGetActiveContext();
    asIScriptEngine*  engine = ctx->GetEngine();
    ScriptArray*      modules = ScriptArray::Create( engine->GetObjectTypeByDecl( "string[]" ) );

    for( uint i = 0, j = engine->GetModuleCount(); i < j; i++ )
        modules->InsertLast( ScriptString::Create( engine->GetModuleByIndex( i )->GetName() ) );
    return modules;
}

asIScriptModule* GetModule( const char* name )
{
    if( name )
        return asGetActiveContext()->GetEngine()->GetModule( name, asGM_ONLY_IF_EXISTS );
    else
        return asGetActiveContext()->GetFunction( 0 )->GetModule();
}

ScriptString* GetCurrentModule()
{
    return ScriptString::Create( GetModule( NULL )->GetName() );
}

ScriptArray* GetEnumsInternal( bool global, const char* module_name )
{
    asIScriptEngine* engine = asGetActiveContext()->GetEngine();
    ScriptArray*     enums = ScriptArray::Create( engine->GetObjectTypeByDecl( "type[]" ) );

    asIScriptModule* module;
    if( !global )
    {
        module = GetModule( module_name );
        if( !module )
            return enums;
    }

    for( uint i = 0, j = ( global ? engine->GetEnumCount() : module->GetEnumCount() ); i < j; i++ )
    {
        int enum_type_id;
        if( global )
            engine->GetEnumByIndex( i, &enum_type_id );
        else
            module->GetEnumByIndex( i, &enum_type_id );
        ScriptType type = ScriptType( engine->GetObjectTypeById( enum_type_id ) );
        enums->InsertLast( &type );
    }
    return enums;
}

ScriptArray* GetGlobalEnums()
{
    return GetEnumsInternal( true, NULL );
}

ScriptArray* GetEnums()
{
    return GetEnumsInternal( false, NULL );
}

ScriptArray* GetEnumsModule( ScriptString& module_name )
{
    return GetEnumsInternal( false, module_name.c_str() );
}

void RegisterMethod( asIScriptEngine* engine, const char* declaration, const asSFuncPtr& func_pointer )
{
    engine->RegisterObjectMethod( "type", declaration, func_pointer, asCALL_THISCALL );
    engine->RegisterObjectMethod( "typeof<T>", declaration, func_pointer, asCALL_THISCALL );
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

    RegisterMethod( engine, "string@ get_name() const", asMETHOD( ScriptType, GetName ) );
    RegisterMethod( engine, "string@ get_nameWithoutNamespace() const", asMETHOD( ScriptType, GetNameWithoutNamespace ) );
    RegisterMethod( engine, "string@ get_namespace() const", asMETHOD( ScriptType, GetNamespace ) );
    RegisterMethod( engine, "string@ get_module() const", asMETHOD( ScriptType, GetModule ) );
    RegisterMethod( engine, "uint get_size() const", asMETHOD( ScriptType, GetSize ) );
    RegisterMethod( engine, "bool get_isAssigned() const", asMETHOD( ScriptType, IsAssigned ) );
    RegisterMethod( engine, "bool get_isGlobal() const", asMETHOD( ScriptType, IsGlobal ) );
    RegisterMethod( engine, "bool get_isClass() const", asMETHOD( ScriptType, IsClass ) );
    RegisterMethod( engine, "bool get_isInterface() const", asMETHOD( ScriptType, IsInterface ) );
    RegisterMethod( engine, "bool get_isEnum() const", asMETHOD( ScriptType, IsEnum ) );
    RegisterMethod( engine, "bool get_isFunction() const", asMETHOD( ScriptType, IsFunction ) );
    RegisterMethod( engine, "bool get_isShared() const", asMETHOD( ScriptType, IsShared ) );
    RegisterMethod( engine, "type get_baseType() const", asMETHOD( ScriptType, GetBaseType ) );
    RegisterMethod( engine, "uint get_interfaceCount() const", asMETHOD( ScriptType, GetInterfaceCount ) );
    RegisterMethod( engine, "type getInterface( uint index ) const", asMETHOD( ScriptType, GetInterface ) );
    RegisterMethod( engine, "bool implements( const type&in other ) const", asMETHOD( ScriptType, Implements ) );
    RegisterMethod( engine, "bool opEquals(const type&in other) const", asMETHOD( ScriptType, Equals ) );
    RegisterMethod( engine, "bool derivesFrom(const type&in other) const", asMETHOD( ScriptType, DerivesFrom ) );
    RegisterMethod( engine, "void instantiate(?&out instance) const", asMETHOD( ScriptType, Instantiate ) );
    RegisterMethod( engine, "void instantiate(?&in copyFrom, ?&out instance) const", asMETHOD( ScriptType, InstantiateCopy ) );
    RegisterMethod( engine, "uint get_methodsCount() const", asMETHOD( ScriptType, GetMethodsCount ) );
    RegisterMethod( engine, "string@ getMethodDeclaration(uint index, bool includeObjectName = false, bool includeNamespace = false, bool includeParamNames = true) const", asMETHOD( ScriptType, GetMethodDeclaration ) );
    RegisterMethod( engine, "uint get_propertiesCount() const", asMETHOD( ScriptType, GetPropertiesCount ) );
    RegisterMethod( engine, "string@ getPropertyDeclaration(uint index, bool includeNamespace = false) const", asMETHOD( ScriptType, GetPropertyDeclaration ) );
    RegisterMethod( engine, "uint get_enumLength() const", asMETHOD( ScriptType, GetEnumLength ) );
    RegisterMethod( engine, "string[]@ get_enumNames() const", asMETHOD( ScriptType, GetEnumNames ) );
    RegisterMethod( engine, "int[]@ get_enumValues() const", asMETHOD( ScriptType, GetEnumValues ) );

    engine->RegisterGlobalFunction( "string[]@ getLoadedModules()", asFUNCTION( GetLoadedModules ), asCALL_CDECL );
    engine->RegisterGlobalFunction( "string@ getCurrentModule()", asFUNCTION( GetCurrentModule ), asCALL_CDECL );
    engine->RegisterGlobalFunction( "type[]@ getGlobalEnums()", asFUNCTION( GetGlobalEnums ), asCALL_CDECL );
    engine->RegisterGlobalFunction( "type[]@ getEnums()", asFUNCTION( GetEnums ), asCALL_CDECL );
    engine->RegisterGlobalFunction( "type[]@ getEnums( string& moduleName )", asFUNCTION( GetEnumsModule ), asCALL_CDECL );

    engine->SetDefaultNamespace( "" );
}
