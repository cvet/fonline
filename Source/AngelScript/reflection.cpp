#include "scriptarray.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_typeinfo.h"

#include "reflection.h"

#include "generic_helpers.h"

using namespace std;

BEGIN_AS_NAMESPACE

static asITypeInfo* GetTypeInfoById(asIScriptEngine* engine, int typeId)
{
    asCDataType dt = ((asCScriptEngine*)engine)->GetDataTypeFromTypeId(typeId);
    if (dt.IsValid())
        return dt.GetTypeInfo();
    return NULL;
}

ScriptType::ScriptType(asITypeInfo* type)
{
    ObjType = type;
}

string ScriptType::GetName() const
{
    if (!ObjType)
        return "(not assigned)";

    const char* ns = ObjType->GetNamespace();
    if (ns[0])
        return string(ns).append("::").append(ObjType->GetName());

    return ObjType->GetName();
}

string ScriptType::GetNameWithoutNamespace() const
{
    if (!ObjType)
        return "(not assigned)";
    return ObjType->GetName();
}

string ScriptType::GetNamespace() const
{
    if (!ObjType)
        return "";
    return ObjType->GetNamespace();
}

string ScriptType::GetModule() const
{
    if (!ObjType)
        return "";
    return ObjType->GetModule() ? ObjType->GetModule()->GetName() : "(global)";
}

asUINT ScriptType::GetSize() const
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
    return ObjType != NULL ? (!IsInterface() && !IsEnum() && !IsFunction()) : false;
}

bool ScriptType::IsInterface() const
{
    return ObjType ? ((asCObjectType*)ObjType)->IsInterface() : false;
}

bool ScriptType::IsEnum() const
{
    asCEnumType* enum_type = ((asCTypeInfo*)ObjType)->CastToEnumType();
    return enum_type ? enum_type->enumValues.GetLength() > 0 : false;
}

bool ScriptType::IsFunction() const
{
    return ObjType ? !strcmp(ObjType->GetName(), "_builtin_function_") : false;
}

bool ScriptType::IsShared() const
{
    return ObjType ? ((asCObjectType*)ObjType)->IsShared() : false;
}

ScriptType ScriptType::GetBaseType() const
{
    return ScriptType(ObjType ? ObjType->GetBaseType() : NULL);
}

asUINT ScriptType::GetInterfaceCount() const
{
    if (!ObjType)
        return 0;
    return ObjType->GetInterfaceCount();
}

ScriptType ScriptType::GetInterface(asUINT index) const
{
    return ScriptType((ObjType && index < ObjType->GetInterfaceCount()) ? ObjType->GetInterface(index) : NULL);
}

bool ScriptType::Implements(const ScriptType& other) const
{
    if (!ObjType || !other.ObjType)
        return false;
    return ObjType->Implements(other.ObjType);
}

bool ScriptType::Equals(const ScriptType& other)
{
    return ObjType == other.ObjType;
}

bool ScriptType::DerivesFrom(const ScriptType& other)
{
    return (ObjType && other.ObjType) ? ObjType->DerivesFrom(other.ObjType) : false;
}

void ScriptType::Instantiate(void* out, int out_type_id) const
{
    if (!ObjType)
    {
        asGetActiveContext()->SetException("Type is not assigned");
        return;
    }

    asIScriptEngine* engine = ObjType->GetEngine();

    if (!(out_type_id & asTYPEID_OBJHANDLE))
    {
        asGetActiveContext()->SetException("Invalid 'instance' argument, not an handle");
        return;
    }
    if (*(void**)out)
    {
        asGetActiveContext()->SetException("Invalid 'instance' argument, handle must be null");
        return;
    }
    if (!ObjType->DerivesFrom(GetTypeInfoById(engine, out_type_id)))
    {
        asGetActiveContext()->SetException("Invalid 'instance' argument, incompatible types");
        return;
    }

    *(void**)out = engine->CreateScriptObject(ObjType);
}

void ScriptType::InstantiateCopy(void* in, int in_type_id, void* out, int out_type_id) const
{
    if (!ObjType)
    {
        asGetActiveContext()->SetException("Type is not assigned");
        return;
    }

    asIScriptEngine* engine = ObjType->GetEngine();

    if (!(out_type_id & asTYPEID_OBJHANDLE))
    {
        asGetActiveContext()->SetException("Invalid 'instance' argument, not an handle");
        return;
    }
    if (*(void**)out)
    {
        asGetActiveContext()->SetException("Invalid 'instance' argument, handle must be null");
        return;
    }
    if (!ObjType->DerivesFrom(GetTypeInfoById(engine, out_type_id)))
    {
        asGetActiveContext()->SetException("Invalid 'instance' argument, incompatible types");
        return;
    }

    if (!(in_type_id & asTYPEID_OBJHANDLE))
    {
        asGetActiveContext()->SetException("Invalid 'copyFrom' argument, not an handle");
        return;
    }
    if (!*(void**)in)
    {
        asGetActiveContext()->SetException("Invalid 'copyFrom' argument, handle must be not null");
        return;
    }
    in = *(void**)in;
    asIScriptObject* in_obj = (asIScriptObject*)in;
    if (in_obj->GetObjectType() != ObjType)
    {
        asGetActiveContext()->SetException("Invalid 'copyFrom' argument, incompatible types");
        return;
    }

    *(void**)out = engine->CreateScriptObjectCopy(in, ObjType);
}

asUINT ScriptType::GetMethodsCount() const
{
    if (!ObjType)
        return 0;
    return ObjType->GetMethodCount();
}

string ScriptType::GetMethodDeclaration(asUINT index, bool include_object_name, bool include_namespace, bool include_param_names) const
{
    if (!ObjType || index >= ObjType->GetMethodCount())
        return "";
    return ObjType->GetMethodByIndex(index)->GetDeclaration(include_object_name, include_namespace, include_param_names);
}

asUINT ScriptType::GetPropertiesCount() const
{
    if (!ObjType)
        return 0;
    return ObjType->GetPropertyCount();
}

string ScriptType::GetPropertyDeclaration(asUINT index, bool include_namespace) const
{
    if (!ObjType || index >= ObjType->GetPropertyCount())
        return "";
    return ObjType->GetPropertyDeclaration(index, include_namespace);
}

asUINT ScriptType::GetEnumLength() const
{
    asCEnumType* enum_type = ((asCTypeInfo*)ObjType)->CastToEnumType();
    return enum_type ? enum_type->enumValues.GetLength() : 0;
}

CScriptArray* ScriptType::GetEnumNames() const
{
    asCEnumType* enum_type = ((asCTypeInfo*)ObjType)->CastToEnumType();
    CScriptArray* result = CScriptArray::Create(asGetActiveContext()->GetEngine()->GetTypeInfoByDecl("string[]"));
    for (asUINT i = 0, j = enum_type ? enum_type->enumValues.GetLength() : 0; i < j; i++)
        result->InsertLast(enum_type->enumValues[i]->name.AddressOf());
    return result;
}

CScriptArray* ScriptType::GetEnumValues() const
{
    asCEnumType* enum_type = ((asCTypeInfo*)ObjType)->CastToEnumType();
    CScriptArray* result = CScriptArray::Create(asGetActiveContext()->GetEngine()->GetTypeInfoByDecl("int[]"));
    for (asUINT i = 0, j = enum_type ? enum_type->enumValues.GetLength() : 0; i < j; i++)
        result->InsertLast(&enum_type->enumValues[i]->value);
    return result;
}

static bool ScriptTypeOfTemplateCallback(asITypeInfo* ot, bool&)
{
    if (ot->GetSubTypeCount() != 1 || (ot->GetSubTypeId() & asTYPEID_MASK_SEQNBR) <= asTYPEID_DOUBLE)
        return false;
    return true;
}

static ScriptTypeOf* ScriptTypeOfFactory(asITypeInfo* ot)
{
    return new ScriptTypeOf(ot->GetSubType());
}

static ScriptTypeOf* ScriptTypeOfFactory2(asITypeInfo* ot, void* ref)
{
    if (ot->GetSubType()->GetTypeId() <= asTYPEID_DOUBLE)
        return new ScriptTypeOf(nullptr);
    ref = *(void**)ref;
    return new ScriptTypeOf(((asIScriptObject*)ref)->GetObjectType());
}

ScriptTypeOf::ScriptTypeOf(asITypeInfo* ot) : ScriptType(ot)
{
    refCount = 1;
}

ScriptTypeOf::~ScriptTypeOf()
{
    //
}

void ScriptTypeOf::AddRef() const
{
    asAtomicInc(refCount);
}

void ScriptTypeOf::Release() const
{
    if (asAtomicDec(refCount) == 0)
        delete this;
}

ScriptType ScriptTypeOf::ConvertToType() const
{
    return ScriptType(ObjType);
}

CScriptArray* GetLoadedModules()
{
    asIScriptContext* ctx = asGetActiveContext();
    asIScriptEngine* engine = ctx->GetEngine();
    CScriptArray* modules = CScriptArray::Create(engine->GetTypeInfoByDecl("string[]"));

    for (asUINT i = 0; i < engine->GetModuleCount(); i++)
    {
        string name = engine->GetModuleByIndex(i)->GetName();
        modules->InsertLast(&name);
    }
    return modules;
}

asIScriptModule* GetModule(const char* name)
{
    if (name)
        return asGetActiveContext()->GetEngine()->GetModule(name, asGM_ONLY_IF_EXISTS);
    else
        return asGetActiveContext()->GetFunction(0)->GetModule();
}

static string GetCurrentModule()
{
    return GetModule(NULL)->GetName();
}

static CScriptArray* GetEnumsInternal(bool global, const char* module_name)
{
    asIScriptEngine* engine = asGetActiveContext()->GetEngine();
    CScriptArray* enums = CScriptArray::Create(engine->GetTypeInfoByDecl("reflection::type[]"));

    asIScriptModule* module;
    if (!global)
    {
        module = GetModule(module_name);
        if (!module)
            return enums;
    }

    asUINT count = (global ? engine->GetEnumCount() : module->GetEnumCount());
    for (asUINT i = 0; i < count; i++)
    {
        asITypeInfo* enum_type;
        if (global)
            enum_type = engine->GetEnumByIndex(i);
        else
            enum_type = module->GetEnumByIndex(i);
        ScriptType type = ScriptType(enum_type);
        enums->InsertLast(&type);
    }
    return enums;
}

static CScriptArray* GetGlobalEnums()
{
    return GetEnumsInternal(true, NULL);
}

static CScriptArray* GetEnums()
{
    return GetEnumsInternal(false, NULL);
}

static CScriptArray* GetEnumsModule(string module_name)
{
    return GetEnumsInternal(false, module_name.c_str());
}

static asUINT GetCallstack(CScriptArray* modules, CScriptArray* names, CScriptArray* lines, CScriptArray* columns, bool include_object_name, bool include_namespace, bool include_param_names)
{
    asIScriptContext* ctx = asGetActiveContext();
    if (!ctx)
        return 0;

    asUINT count = 0, stack_size = ctx->GetCallstackSize();
    int line, column;
    const asIScriptFunction* func;

    for (asUINT i = 0; i < stack_size; i++)
    {
        func = ctx->GetFunction(i);
        line = ctx->GetLineNumber(i, &column);
        if (func)
        {
            string name = func->GetModuleName();
            modules->InsertLast(&name);

            bool include_ns = (include_namespace && func->GetNamespace()[0]);
            string decl = func->GetDeclaration(include_object_name, include_ns, include_param_names);
            names->InsertLast(&decl);

            lines->InsertLast(&line);
            columns->InsertLast(&column);

            count++;
        }
    }

    return count;
}

void RegisterScriptReflection(asIScriptEngine* engine)
{
    if (strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") == 0)
        RegisterScriptReflection_Native(engine);
    else
        RegisterScriptReflection_Generic(engine);
}

static void RegisterMethod_Native(asIScriptEngine* engine, const char* declaration, const asSFuncPtr& func_pointer)
{
    engine->RegisterObjectMethod("type", declaration, func_pointer, asCALL_THISCALL);
    engine->RegisterObjectMethod("typeof<T>", declaration, func_pointer, asCALL_THISCALL);
}

void RegisterScriptReflection_Native(asIScriptEngine* engine)
{
    engine->SetDefaultNamespace("reflection");

    engine->RegisterObjectType("type", sizeof(ScriptType), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS);

    engine->RegisterObjectType("typeof<class T>", sizeof(ScriptTypeOf), asOBJ_REF | asOBJ_TEMPLATE);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", asFUNCTION(ScriptTypeOfTemplateCallback), asCALL_CDECL);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_FACTORY, "typeof<T>@ f(int&in)", asFUNCTION(ScriptTypeOfFactory), asCALL_CDECL);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_FACTORY, "typeof<T>@ f(int&in, const T&in)", asFUNCTION(ScriptTypeOfFactory2), asCALL_CDECL);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptTypeOf, AddRef), asCALL_THISCALL);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptTypeOf, Release), asCALL_THISCALL);
    engine->RegisterObjectMethod("typeof<T>", "type opImplConv()", asMETHOD(ScriptTypeOf, ConvertToType), asCALL_THISCALL);

    RegisterMethod_Native(engine, "string get_name() const", asMETHOD(ScriptType, GetName));
    RegisterMethod_Native(engine, "string get_nameWithoutNamespace() const", asMETHOD(ScriptType, GetNameWithoutNamespace));
    RegisterMethod_Native(engine, "string get_namespace() const", asMETHOD(ScriptType, GetNamespace));
    RegisterMethod_Native(engine, "string get_module() const", asMETHOD(ScriptType, GetModule));
    RegisterMethod_Native(engine, "uint get_size() const", asMETHOD(ScriptType, GetSize));
    RegisterMethod_Native(engine, "bool get_isAssigned() const", asMETHOD(ScriptType, IsAssigned));
    RegisterMethod_Native(engine, "bool get_isGlobal() const", asMETHOD(ScriptType, IsGlobal));
    RegisterMethod_Native(engine, "bool get_isClass() const", asMETHOD(ScriptType, IsClass));
    RegisterMethod_Native(engine, "bool get_isInterface() const", asMETHOD(ScriptType, IsInterface));
    RegisterMethod_Native(engine, "bool get_isEnum() const", asMETHOD(ScriptType, IsEnum));
    RegisterMethod_Native(engine, "bool get_isFunction() const", asMETHOD(ScriptType, IsFunction));
    RegisterMethod_Native(engine, "bool get_isShared() const", asMETHOD(ScriptType, IsShared));
    RegisterMethod_Native(engine, "type get_baseType() const", asMETHOD(ScriptType, GetBaseType));
    RegisterMethod_Native(engine, "uint get_interfaceCount() const", asMETHOD(ScriptType, GetInterfaceCount));
    RegisterMethod_Native(engine, "type getInterface(uint index) const", asMETHOD(ScriptType, GetInterface));
    RegisterMethod_Native(engine, "bool implements(const type&in other) const", asMETHOD(ScriptType, Implements));
    RegisterMethod_Native(engine, "bool opEquals(const type&in other) const", asMETHOD(ScriptType, Equals));
    RegisterMethod_Native(engine, "bool derivesFrom(const type&in other) const", asMETHOD(ScriptType, DerivesFrom));
    RegisterMethod_Native(engine, "void instantiate(?&out instance) const", asMETHOD(ScriptType, Instantiate));
    RegisterMethod_Native(engine, "void instantiate(?&in copyFrom, ?&out instance) const", asMETHOD(ScriptType, InstantiateCopy));
    RegisterMethod_Native(engine, "uint get_methodsCount() const", asMETHOD(ScriptType, GetMethodsCount));
    RegisterMethod_Native(engine, "string getMethodDeclaration(uint index, bool includeObjectName = false, bool includeNamespace = false, bool includeParamNames = true) const", asMETHOD(ScriptType, GetMethodDeclaration));
    RegisterMethod_Native(engine, "uint get_propertiesCount() const", asMETHOD(ScriptType, GetPropertiesCount));
    RegisterMethod_Native(engine, "string getPropertyDeclaration(uint index, bool includeNamespace = false) const", asMETHOD(ScriptType, GetPropertyDeclaration));
    RegisterMethod_Native(engine, "uint get_enumLength() const", asMETHOD(ScriptType, GetEnumLength));
    RegisterMethod_Native(engine, "string[]@ get_enumNames() const", asMETHOD(ScriptType, GetEnumNames));
    RegisterMethod_Native(engine, "int[]@ get_enumValues() const", asMETHOD(ScriptType, GetEnumValues));

    engine->RegisterGlobalFunction("string[]@ getLoadedModules()", asFUNCTION(GetLoadedModules), asCALL_CDECL);
    engine->RegisterGlobalFunction("string getCurrentModule()", asFUNCTION(GetCurrentModule), asCALL_CDECL);
    engine->RegisterGlobalFunction("type[]@ getGlobalEnums()", asFUNCTION(GetGlobalEnums), asCALL_CDECL);
    engine->RegisterGlobalFunction("type[]@ getEnums()", asFUNCTION(GetEnums), asCALL_CDECL);
    engine->RegisterGlobalFunction("type[]@ getEnums(string moduleName)", asFUNCTION(GetEnumsModule), asCALL_CDECL);
    engine->RegisterGlobalFunction("uint getCallstack(string[]& modules, string[]& names, uint[]& lines, uint[]& columns, bool includeObjectName = false, bool includeNamespace = false, bool includeParamNames = true)", asFUNCTION(GetCallstack), asCALL_CDECL);

    engine->SetDefaultNamespace("");
}

static void ScriptType_Instantiate_Generic(asIScriptGeneric* gen)
{
    ((ScriptType*)gen->GetObject())->Instantiate(*(void**)gen->GetArgAddress(0), gen->GetArgTypeId(0));
}

static void ScriptType_InstantiateCopy_Generic(asIScriptGeneric* gen)
{
    ((ScriptType*)gen->GetObject())->InstantiateCopy(*(void**)gen->GetArgAddress(0), gen->GetArgTypeId(0), *(void**)gen->GetArgAddress(1), gen->GetArgTypeId(1));
}

static void ScriptType_GetMethodDeclaration_Generic(asIScriptGeneric* gen)
{
    new(gen->GetAddressOfReturnLocation()) string(((ScriptType*)gen->GetObject())->GetMethodDeclaration(
        *(asUINT*)gen->GetArgAddress(0), *(bool*)gen->GetArgAddress(1), *(bool*)gen->GetArgAddress(2), *(bool*)gen->GetArgAddress(3)));
}

static void GetCallstack_Generic(asIScriptGeneric* gen)
{
    new(gen->GetAddressOfReturnLocation()) asUINT(GetCallstack(
        *(CScriptArray**)gen->GetArgAddress(0), *(CScriptArray**)gen->GetArgAddress(1), *(CScriptArray**)gen->GetArgAddress(2), *(CScriptArray**)gen->GetArgAddress(3),
        *(bool*)gen->GetArgAddress(4), *(bool*)gen->GetArgAddress(5), *(bool*)gen->GetArgAddress(6)));
}

WRAP_CDECL_TO_GENERIC(bool, ScriptTypeOfTemplateCallback, asITypeInfo*, bool);
WRAP_CDECL_TO_GENERIC(ScriptTypeOf*, ScriptTypeOfFactory, asITypeInfo*);
WRAP_CDECL_TO_GENERIC(ScriptTypeOf*, ScriptTypeOfFactory2, asITypeInfo*, void*);
WRAP_THISCALL_TO_GENERIC(void, ScriptTypeOf, AddRef);
WRAP_THISCALL_TO_GENERIC(void, ScriptTypeOf, Release);
WRAP_THISCALL_TO_GENERIC(ScriptType, ScriptTypeOf, ConvertToType);
WRAP_THISCALL_TO_GENERIC(string, ScriptType, GetName);
WRAP_THISCALL_TO_GENERIC(string, ScriptType, GetNameWithoutNamespace);
WRAP_THISCALL_TO_GENERIC(string, ScriptType, GetNamespace);
WRAP_THISCALL_TO_GENERIC(string, ScriptType, GetModule);
WRAP_THISCALL_TO_GENERIC(asUINT, ScriptType, GetSize);
WRAP_THISCALL_TO_GENERIC(bool, ScriptType, IsAssigned);
WRAP_THISCALL_TO_GENERIC(bool, ScriptType, IsGlobal);
WRAP_THISCALL_TO_GENERIC(bool, ScriptType, IsClass);
WRAP_THISCALL_TO_GENERIC(bool, ScriptType, IsInterface);
WRAP_THISCALL_TO_GENERIC(bool, ScriptType, IsEnum);
WRAP_THISCALL_TO_GENERIC(bool, ScriptType, IsFunction);
WRAP_THISCALL_TO_GENERIC(bool, ScriptType, IsShared);
WRAP_THISCALL_TO_GENERIC(ScriptType, ScriptType, GetBaseType);
WRAP_THISCALL_TO_GENERIC(asUINT, ScriptType, GetInterfaceCount);
WRAP_THISCALL_TO_GENERIC(ScriptType, ScriptType, GetInterface, asUINT);
WRAP_THISCALL_TO_GENERIC(bool, ScriptType, Implements, ScriptType);
WRAP_THISCALL_TO_GENERIC(bool, ScriptType, Equals, ScriptType);
WRAP_THISCALL_TO_GENERIC(bool, ScriptType, DerivesFrom, ScriptType);
WRAP_THISCALL_TO_GENERIC(asUINT, ScriptType, GetMethodsCount);
WRAP_THISCALL_TO_GENERIC(asUINT, ScriptType, GetPropertiesCount);
WRAP_THISCALL_TO_GENERIC(string, ScriptType, GetPropertyDeclaration, asUINT, bool);
WRAP_THISCALL_TO_GENERIC(asUINT, ScriptType, GetEnumLength);
WRAP_THISCALL_TO_GENERIC(CScriptArray*, ScriptType, GetEnumNames);
WRAP_THISCALL_TO_GENERIC(CScriptArray*, ScriptType, GetEnumValues);
WRAP_CDECL_TO_GENERIC(CScriptArray*, GetLoadedModules);
WRAP_CDECL_TO_GENERIC(string, GetCurrentModule);
WRAP_CDECL_TO_GENERIC(CScriptArray*, GetGlobalEnums);
WRAP_CDECL_TO_GENERIC(CScriptArray*, GetEnums);
WRAP_CDECL_TO_GENERIC(CScriptArray*, GetEnumsModule, string);

static void RegisterMethod_Generic(asIScriptEngine* engine, const char* declaration, const asSFuncPtr& func_pointer)
{
    engine->RegisterObjectMethod("type", declaration, func_pointer, asCALL_GENERIC);
    engine->RegisterObjectMethod("typeof<T>", declaration, func_pointer, asCALL_GENERIC);
}

void RegisterScriptReflection_Generic(asIScriptEngine* engine)
{
    engine->SetDefaultNamespace("reflection");

    engine->RegisterObjectType("type", sizeof(ScriptType), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS);

    engine->RegisterObjectType("typeof<class T>", sizeof(ScriptTypeOf), asOBJ_REF | asOBJ_TEMPLATE);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", asFUNCTION(ScriptTypeOfTemplateCallback_Generic), asCALL_GENERIC);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_FACTORY, "typeof<T>@ f(int&in)", asFUNCTION(ScriptTypeOfFactory_Generic), asCALL_GENERIC);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_FACTORY, "typeof<T>@ f(int&in, const T&in)", asFUNCTION(ScriptTypeOfFactory2_Generic), asCALL_GENERIC);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_ADDREF, "void f()", asFUNCTION(ScriptTypeOf_AddRef_Generic), asCALL_GENERIC);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_RELEASE, "void f()", asFUNCTION(ScriptTypeOf_Release_Generic), asCALL_GENERIC);
    engine->RegisterObjectMethod("typeof<T>", "type opImplConv()", asFUNCTION(ScriptTypeOf_ConvertToType_Generic), asCALL_GENERIC);

    RegisterMethod_Generic(engine, "string get_name() const", asFUNCTION(ScriptType_GetName_Generic));
    RegisterMethod_Generic(engine, "string get_nameWithoutNamespace() const", asFUNCTION(ScriptType_GetNameWithoutNamespace_Generic));
    RegisterMethod_Generic(engine, "string get_namespace() const", asFUNCTION(ScriptType_GetNamespace_Generic));
    RegisterMethod_Generic(engine, "string get_module() const", asFUNCTION(ScriptType_GetModule_Generic));
    RegisterMethod_Generic(engine, "uint get_size() const", asFUNCTION(ScriptType_GetSize_Generic));
    RegisterMethod_Generic(engine, "bool get_isAssigned() const", asFUNCTION(ScriptType_IsAssigned_Generic));
    RegisterMethod_Generic(engine, "bool get_isGlobal() const", asFUNCTION(ScriptType_IsGlobal_Generic));
    RegisterMethod_Generic(engine, "bool get_isClass() const", asFUNCTION(ScriptType_IsClass_Generic));
    RegisterMethod_Generic(engine, "bool get_isInterface() const", asFUNCTION(ScriptType_IsInterface_Generic));
    RegisterMethod_Generic(engine, "bool get_isEnum() const", asFUNCTION(ScriptType_IsEnum_Generic));
    RegisterMethod_Generic(engine, "bool get_isFunction() const", asFUNCTION(ScriptType_IsFunction_Generic));
    RegisterMethod_Generic(engine, "bool get_isShared() const", asFUNCTION(ScriptType_IsShared_Generic));
    RegisterMethod_Generic(engine, "type get_baseType() const", asFUNCTION(ScriptType_GetBaseType_Generic));
    RegisterMethod_Generic(engine, "uint get_interfaceCount() const", asFUNCTION(ScriptType_GetInterfaceCount_Generic));
    RegisterMethod_Generic(engine, "type getInterface(uint index) const", asFUNCTION(ScriptType_GetInterface_Generic));
    RegisterMethod_Generic(engine, "bool implements(const type&in other) const", asFUNCTION(ScriptType_Implements_Generic));
    RegisterMethod_Generic(engine, "bool opEquals(const type&in other) const", asFUNCTION(ScriptType_Equals_Generic));
    RegisterMethod_Generic(engine, "bool derivesFrom(const type&in other) const", asFUNCTION(ScriptType_DerivesFrom_Generic));
    RegisterMethod_Generic(engine, "void instantiate(?&out instance) const", asFUNCTION(ScriptType_Instantiate_Generic));
    RegisterMethod_Generic(engine, "void instantiate(?&in copyFrom, ?&out instance) const", asFUNCTION(ScriptType_InstantiateCopy_Generic));
    RegisterMethod_Generic(engine, "uint get_methodsCount() const", asFUNCTION(ScriptType_GetMethodsCount_Generic));
    RegisterMethod_Generic(engine, "string getMethodDeclaration(uint index, bool includeObjectName = false, bool includeNamespace = false, bool includeParamNames = true) const", asFUNCTION(ScriptType_GetMethodDeclaration_Generic));
    RegisterMethod_Generic(engine, "uint get_propertiesCount() const", asFUNCTION(ScriptType_GetPropertiesCount_Generic));
    RegisterMethod_Generic(engine, "string getPropertyDeclaration(uint index, bool includeNamespace = false) const", asFUNCTION(ScriptType_GetPropertyDeclaration_Generic));
    RegisterMethod_Generic(engine, "uint get_enumLength() const", asFUNCTION(ScriptType_GetEnumLength_Generic));
    RegisterMethod_Generic(engine, "string[]@ get_enumNames() const", asFUNCTION(ScriptType_GetEnumNames_Generic));
    RegisterMethod_Generic(engine, "int[]@ get_enumValues() const", asFUNCTION(ScriptType_GetEnumValues_Generic));

    engine->RegisterGlobalFunction("string[]@ getLoadedModules()", asFUNCTION(GetLoadedModules_Generic), asCALL_GENERIC);
    engine->RegisterGlobalFunction("string getCurrentModule()", asFUNCTION(GetCurrentModule_Generic), asCALL_GENERIC);
    engine->RegisterGlobalFunction("type[]@ getGlobalEnums()", asFUNCTION(GetGlobalEnums_Generic), asCALL_GENERIC);
    engine->RegisterGlobalFunction("type[]@ getEnums()", asFUNCTION(GetEnums_Generic), asCALL_GENERIC);
    engine->RegisterGlobalFunction("type[]@ getEnums(string moduleName)", asFUNCTION(GetEnumsModule_Generic), asCALL_GENERIC);
    engine->RegisterGlobalFunction("uint getCallstack(string[]& modules, string[]& names, uint[]& lines, uint[]& columns, bool includeObjectName = false, bool includeNamespace = false, bool includeParamNames = true)", asFUNCTION(GetCallstack_Generic), asCALL_GENERIC);

    engine->SetDefaultNamespace("");
}

END_AS_NAMESPACE
