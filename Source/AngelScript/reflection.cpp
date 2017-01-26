#include "scriptarray.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_typeinfo.h"
#include "../autowrapper/aswrappedcall.h"

#include "reflection.h"

using namespace std;

BEGIN_AS_NAMESPACE

static asITypeInfo* GetTypeInfoById(asIScriptEngine* engine, int typeId)
{
    asCDataType dt = ((asCScriptEngine*)engine)->GetDataTypeFromTypeId(typeId);
    if (dt.IsValid())
        return dt.GetTypeInfo();
    return nullptr;
}

ScriptType::ScriptType(asITypeInfo* type)
{
    refCount = 1;
    objType = type;
}

void ScriptType::AddRef() const
{
    asAtomicInc(refCount);
}

void ScriptType::Release() const
{
    if (asAtomicDec(refCount) == 0)
        delete this;
}

string ScriptType::GetName() const
{
    const char* ns = objType->GetNamespace();
    if (ns[0])
        return string(ns).append("::").append(objType->GetName());

    return objType->GetName();
}

string ScriptType::GetNameWithoutNamespace() const
{
    return objType->GetName();
}

string ScriptType::GetNamespace() const
{
    return objType->GetNamespace();
}

string ScriptType::GetModule() const
{
    return objType->GetModule() ? objType->GetModule()->GetName() : "(global)";
}

asUINT ScriptType::GetSize() const
{
    return objType->GetSize();
}

bool ScriptType::IsGlobal() const
{
    return objType->GetModule() == nullptr;
}

bool ScriptType::IsClass() const
{
    return !IsInterface() && !IsEnum() && !IsFunction();
}

bool ScriptType::IsInterface() const
{
    return ((asCObjectType*)objType)->IsInterface();
}

bool ScriptType::IsEnum() const
{
    asCEnumType* enum_type = ((asCTypeInfo*)objType)->CastToEnumType();
    return enum_type ? enum_type->enumValues.GetLength() > 0 : false;
}

bool ScriptType::IsFunction() const
{
    return !strcmp(objType->GetName(), "_builtin_function_");
}

bool ScriptType::IsShared() const
{
    return ((asCObjectType*)objType)->IsShared();
}

ScriptType* ScriptType::GetBaseType() const
{
    asITypeInfo* base = objType->GetBaseType();
    return base ? new ScriptType(base) : nullptr;
}

asUINT ScriptType::GetInterfaceCount() const
{
    return objType->GetInterfaceCount();
}

ScriptType* ScriptType::GetInterface(asUINT index) const
{
    return index < objType->GetInterfaceCount() ? new ScriptType(objType->GetInterface(index)) : nullptr;
}

bool ScriptType::Implements(const ScriptType* other) const
{
    return other && objType->Implements(other->objType);
}

bool ScriptType::Equals(const ScriptType* other)
{
    return other && objType == other->objType;
}

bool ScriptType::DerivesFrom(const ScriptType* other)
{
    return other && objType->DerivesFrom(other->objType);
}

void ScriptType::Instantiate(void* out, int out_type_id) const
{
    asIScriptEngine* engine = objType->GetEngine();

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
    if (!objType->DerivesFrom(GetTypeInfoById(engine, out_type_id)))
    {
        asGetActiveContext()->SetException("Invalid 'instance' argument, incompatible types");
        return;
    }

    *(void**)out = engine->CreateScriptObject(objType);
}

void ScriptType::InstantiateCopy(void* in, int in_type_id, void* out, int out_type_id) const
{
    asIScriptEngine* engine = objType->GetEngine();

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
    if (!objType->DerivesFrom(GetTypeInfoById(engine, out_type_id)))
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
    if (in_obj->GetObjectType() != objType)
    {
        asGetActiveContext()->SetException("Invalid 'copyFrom' argument, incompatible types");
        return;
    }

    *(void**)out = engine->CreateScriptObjectCopy(in, objType);
}

asUINT ScriptType::GetMethodsCount() const
{
    return objType->GetMethodCount();
}

string ScriptType::GetMethodDeclaration(asUINT index, bool include_object_name, bool include_namespace, bool include_param_names) const
{
    if (index >= objType->GetMethodCount())
        return "";
    return objType->GetMethodByIndex(index)->GetDeclaration(include_object_name, include_namespace, include_param_names);
}

asUINT ScriptType::GetPropertiesCount() const
{
    return objType->GetPropertyCount();
}

string ScriptType::GetPropertyDeclaration(asUINT index, bool include_namespace) const
{
    if (index >= objType->GetPropertyCount())
        return "";
    return objType->GetPropertyDeclaration(index, include_namespace);
}

asUINT ScriptType::GetEnumLength() const
{
    asCEnumType* enum_type = ((asCTypeInfo*)objType)->CastToEnumType();
    return enum_type ? enum_type->enumValues.GetLength() : 0;
}

CScriptArray* ScriptType::GetEnumNames() const
{
    asCEnumType* enum_type = ((asCTypeInfo*)objType)->CastToEnumType();
    CScriptArray* result = CScriptArray::Create(asGetActiveContext()->GetEngine()->GetTypeInfoByDecl("string[]"));
    for (asUINT i = 0, j = enum_type ? enum_type->enumValues.GetLength() : 0; i < j; i++)
        result->InsertLast(enum_type->enumValues[i]->name.AddressOf());
    return result;
}

CScriptArray* ScriptType::GetEnumValues() const
{
    asCEnumType* enum_type = ((asCTypeInfo*)objType)->CastToEnumType();
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

void ScriptTypeOf::AddRef() const
{
    asAtomicInc(refCount);
}

void ScriptTypeOf::Release() const
{
    if (asAtomicDec(refCount) == 0)
        delete this;
}

ScriptType* ScriptTypeOf::ConvertToType() const
{
    return objType ? new ScriptType(objType) : nullptr;
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
    return GetModule(nullptr)->GetName();
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
        ScriptType* type = new ScriptType(enum_type);
        enums->InsertLast(&type);
        type->Release();
    }
    return enums;
}

static CScriptArray* GetGlobalEnums()
{
    return GetEnumsInternal(true, nullptr);
}

static CScriptArray* GetEnums()
{
    return GetEnumsInternal(false, nullptr);
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

    engine->RegisterObjectType("type", sizeof(ScriptType), asOBJ_REF);
    engine->RegisterObjectBehaviour("type", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptType, AddRef), asCALL_THISCALL);
    engine->RegisterObjectBehaviour("type", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptType, Release), asCALL_THISCALL);

    engine->RegisterObjectType("typeof<class T>", sizeof(ScriptTypeOf), asOBJ_REF | asOBJ_TEMPLATE);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", asFUNCTION(ScriptTypeOfTemplateCallback), asCALL_CDECL);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_FACTORY, "typeof<T>@ f(int&in)", asFUNCTION(ScriptTypeOfFactory), asCALL_CDECL);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_FACTORY, "typeof<T>@ f(int&in, const T&in)", asFUNCTION(ScriptTypeOfFactory2), asCALL_CDECL);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptTypeOf, AddRef), asCALL_THISCALL);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptTypeOf, Release), asCALL_THISCALL);
    engine->RegisterObjectMethod("typeof<T>", "type@ opImplConv()", asMETHOD(ScriptTypeOf, ConvertToType), asCALL_THISCALL);

    RegisterMethod_Native(engine, "string get_name() const", asMETHOD(ScriptType, GetName));
    RegisterMethod_Native(engine, "string get_nameWithoutNamespace() const", asMETHOD(ScriptType, GetNameWithoutNamespace));
    RegisterMethod_Native(engine, "string get_namespace() const", asMETHOD(ScriptType, GetNamespace));
    RegisterMethod_Native(engine, "string get_module() const", asMETHOD(ScriptType, GetModule));
    RegisterMethod_Native(engine, "uint get_size() const", asMETHOD(ScriptType, GetSize));
    RegisterMethod_Native(engine, "bool get_isGlobal() const", asMETHOD(ScriptType, IsGlobal));
    RegisterMethod_Native(engine, "bool get_isClass() const", asMETHOD(ScriptType, IsClass));
    RegisterMethod_Native(engine, "bool get_isInterface() const", asMETHOD(ScriptType, IsInterface));
    RegisterMethod_Native(engine, "bool get_isEnum() const", asMETHOD(ScriptType, IsEnum));
    RegisterMethod_Native(engine, "bool get_isFunction() const", asMETHOD(ScriptType, IsFunction));
    RegisterMethod_Native(engine, "bool get_isShared() const", asMETHOD(ScriptType, IsShared));
    RegisterMethod_Native(engine, "type@ get_baseType() const", asMETHOD(ScriptType, GetBaseType));
    RegisterMethod_Native(engine, "uint get_interfaceCount() const", asMETHOD(ScriptType, GetInterfaceCount));
    RegisterMethod_Native(engine, "type@ getInterface(uint index) const", asMETHOD(ScriptType, GetInterface));
    RegisterMethod_Native(engine, "bool implements(const type@+ other) const", asMETHOD(ScriptType, Implements));
    RegisterMethod_Native(engine, "bool opEquals(const type@+ other) const", asMETHOD(ScriptType, Equals));
    RegisterMethod_Native(engine, "bool derivesFrom(const type@+ other) const", asMETHOD(ScriptType, DerivesFrom));
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
    engine->RegisterGlobalFunction("type@[]@ getGlobalEnums()", asFUNCTION(GetGlobalEnums), asCALL_CDECL);
    engine->RegisterGlobalFunction("type@[]@ getEnums()", asFUNCTION(GetEnums), asCALL_CDECL);
    engine->RegisterGlobalFunction("type@[]@ getEnums(string moduleName)", asFUNCTION(GetEnumsModule), asCALL_CDECL);
    engine->RegisterGlobalFunction("uint getCallstack(string[]& modules, string[]& names, uint[]& lines, uint[]& columns, bool includeObjectName = false, bool includeNamespace = false, bool includeParamNames = true)", asFUNCTION(GetCallstack), asCALL_CDECL);

    engine->SetDefaultNamespace("");
}

static void ScriptType_Instantiate_Generic(asIScriptGeneric* gen)
{
    ((ScriptType*)gen->GetObject())->Instantiate(gen->GetArgAddress(0), gen->GetArgTypeId(0));
}

static void ScriptType_InstantiateCopy_Generic(asIScriptGeneric* gen)
{
    ((ScriptType*)gen->GetObject())->InstantiateCopy(gen->GetArgAddress(0), gen->GetArgTypeId(0), gen->GetArgAddress(1), gen->GetArgTypeId(1));
}

static void RegisterMethod_Generic(asIScriptEngine* engine, const char* declaration, const asSFuncPtr& func_pointer)
{
    engine->RegisterObjectMethod("type", declaration, func_pointer, asCALL_GENERIC);
    engine->RegisterObjectMethod("typeof<T>", declaration, func_pointer, asCALL_GENERIC);
}

void RegisterScriptReflection_Generic(asIScriptEngine* engine)
{
    engine->SetDefaultNamespace("reflection");

    engine->RegisterObjectType("type", sizeof(ScriptType), asOBJ_REF);
    engine->RegisterObjectBehaviour("type", asBEHAVE_ADDREF, "void f()", WRAP_MFN(ScriptType, AddRef), asCALL_GENERIC);
    engine->RegisterObjectBehaviour("type", asBEHAVE_RELEASE, "void f()", WRAP_MFN(ScriptType, Release), asCALL_GENERIC);

    engine->RegisterObjectType("typeof<class T>", sizeof(ScriptTypeOf), asOBJ_REF | asOBJ_TEMPLATE);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", WRAP_FN(ScriptTypeOfTemplateCallback), asCALL_GENERIC);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_FACTORY, "typeof<T>@ f(int&in)", WRAP_FN(ScriptTypeOfFactory), asCALL_GENERIC);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_FACTORY, "typeof<T>@ f(int&in, const T&in)", WRAP_FN(ScriptTypeOfFactory2), asCALL_GENERIC);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_ADDREF, "void f()", WRAP_MFN(ScriptTypeOf, AddRef), asCALL_GENERIC);
    engine->RegisterObjectBehaviour("typeof<T>", asBEHAVE_RELEASE, "void f()", WRAP_MFN(ScriptTypeOf, Release), asCALL_GENERIC);
    engine->RegisterObjectMethod("typeof<T>", "type@ opImplConv()", WRAP_MFN(ScriptTypeOf, ConvertToType), asCALL_GENERIC);

    RegisterMethod_Generic(engine, "string get_name() const", WRAP_MFN(ScriptType, GetName));
    RegisterMethod_Generic(engine, "string get_nameWithoutNamespace() const", WRAP_MFN(ScriptType, GetNameWithoutNamespace));
    RegisterMethod_Generic(engine, "string get_namespace() const", WRAP_MFN(ScriptType, GetNamespace));
    RegisterMethod_Generic(engine, "string get_module() const", WRAP_MFN(ScriptType, GetModule));
    RegisterMethod_Generic(engine, "uint get_size() const", WRAP_MFN(ScriptType, GetSize));
    RegisterMethod_Generic(engine, "bool get_isGlobal() const", WRAP_MFN(ScriptType, IsGlobal));
    RegisterMethod_Generic(engine, "bool get_isClass() const", WRAP_MFN(ScriptType, IsClass));
    RegisterMethod_Generic(engine, "bool get_isInterface() const", WRAP_MFN(ScriptType, IsInterface));
    RegisterMethod_Generic(engine, "bool get_isEnum() const", WRAP_MFN(ScriptType, IsEnum));
    RegisterMethod_Generic(engine, "bool get_isFunction() const", WRAP_MFN(ScriptType, IsFunction));
    RegisterMethod_Generic(engine, "bool get_isShared() const", WRAP_MFN(ScriptType, IsShared));
    RegisterMethod_Generic(engine, "type@ get_baseType() const", WRAP_MFN(ScriptType, GetBaseType));
    RegisterMethod_Generic(engine, "uint get_interfaceCount() const", WRAP_MFN(ScriptType, GetInterfaceCount));
    RegisterMethod_Generic(engine, "type@ getInterface(uint index) const", WRAP_MFN(ScriptType, GetInterface));
    RegisterMethod_Generic(engine, "bool implements(const type@+ other) const", WRAP_MFN(ScriptType, Implements));
    RegisterMethod_Generic(engine, "bool opEquals(const type@+ other) const", WRAP_MFN(ScriptType, Equals));
    RegisterMethod_Generic(engine, "bool derivesFrom(const type@+ other) const", WRAP_MFN(ScriptType, DerivesFrom));
    RegisterMethod_Generic(engine, "void instantiate(?&out instance) const", asFUNCTION(ScriptType_Instantiate_Generic));
    RegisterMethod_Generic(engine, "void instantiate(?&in copyFrom, ?&out instance) const", asFUNCTION(ScriptType_InstantiateCopy_Generic));
    RegisterMethod_Generic(engine, "uint get_methodsCount() const", WRAP_MFN(ScriptType, GetMethodsCount));
    RegisterMethod_Generic(engine, "string getMethodDeclaration(uint index, bool includeObjectName = false, bool includeNamespace = false, bool includeParamNames = true) const", WRAP_MFN(ScriptType, GetMethodDeclaration));
    RegisterMethod_Generic(engine, "uint get_propertiesCount() const", WRAP_MFN(ScriptType, GetPropertiesCount));
    RegisterMethod_Generic(engine, "string getPropertyDeclaration(uint index, bool includeNamespace = false) const", WRAP_MFN(ScriptType, GetPropertyDeclaration));
    RegisterMethod_Generic(engine, "uint get_enumLength() const", WRAP_MFN(ScriptType, GetEnumLength));
    RegisterMethod_Generic(engine, "string[]@ get_enumNames() const", WRAP_MFN(ScriptType, GetEnumNames));
    RegisterMethod_Generic(engine, "int[]@ get_enumValues() const", WRAP_MFN(ScriptType, GetEnumValues));

    engine->RegisterGlobalFunction("string[]@ getLoadedModules()", WRAP_FN(GetLoadedModules), asCALL_GENERIC);
    engine->RegisterGlobalFunction("string getCurrentModule()", WRAP_FN(GetCurrentModule), asCALL_GENERIC);
    engine->RegisterGlobalFunction("type@[]@ getGlobalEnums()", WRAP_FN(GetGlobalEnums), asCALL_GENERIC);
    engine->RegisterGlobalFunction("type@[]@ getEnums()", WRAP_FN(GetEnums), asCALL_GENERIC);
    engine->RegisterGlobalFunction("type@[]@ getEnums(string moduleName)", WRAP_FN(GetEnumsModule), asCALL_GENERIC);
    engine->RegisterGlobalFunction("uint getCallstack(string[]& modules, string[]& names, uint[]& lines, uint[]& columns, bool includeObjectName = false, bool includeNamespace = false, bool includeParamNames = true)", WRAP_FN(GetCallstack), asCALL_GENERIC);

    engine->SetDefaultNamespace("");
}

END_AS_NAMESPACE
