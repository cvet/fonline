//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// ReSharper disable CppInconsistentNaming
// ReSharper disable CppCStyleCast
// ReSharper disable CppLocalVariableMayBeConst
// ReSharper disable CppClangTidyReadabilityQualifiedAuto
// ReSharper disable CppClangTidyModernizeUseNodiscard
// ReSharper disable CppClangTidyClangDiagnosticExtraSemiStmt
// ReSharper disable CppUseAuto

///@ CodeGen Defines

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#include "Common.h"

#if !COMPILER_MODE
#if SERVER_SCRIPTING
#include "Server.h"
#include "ServerScripting.h"
#elif CLIENT_SCRIPTING
#include "Client.h"
#include "ClientScripting.h"
#elif MAPPER_SCRIPTING
#include "Mapper.h"
#include "MapperScripting.h"
#endif

#else
#include "FileSystem.h"
#include "StringUtils.h"

DECLARE_EXCEPTION(ScriptInitException);
DECLARE_EXCEPTION(ScriptCompilerException);
#endif

#if SERVER_SCRIPTING
#define SCRIPTING_CLASS ServerScriptSystem
#define FOEngine FOServer
#define BaseEntity ServerEntity
#elif CLIENT_SCRIPTING
#define SCRIPTING_CLASS ClientScriptSystem
#define FOEngine FOClient
#define BaseEntity ClientEntity
#elif MAPPER_SCRIPTING
#define SCRIPTING_CLASS MapperScriptSystem
#define FOEngine FOMapper
#define BaseEntity ClientEntity
#else
#error Invalid setup
#endif

#include "AngelScriptExtensions.h"
#include "AngelScriptReflection.h"
#include "AngelScriptScriptDict.h"
#include "Log.h"

#include "../autowrapper/aswrappedcall.h"
#include "angelscript.h"
#include "datetime/datetime.h"
#include "preprocessor.h"
#include "scriptany/scriptany.h"
#include "scriptarray.h"
#include "scriptarray/scriptarray.h"
#include "scriptdictionary/scriptdictionary.h"
#include "scriptfile/scriptfile.h"
#include "scriptfile/scriptfilesystem.h"
#include "scripthelper/scripthelper.h"
#include "scriptmath/scriptmath.h"
#include "scriptstdstring/scriptstdstring.h"
#include "weakref/weakref.h"

#if COMPILER_MODE
struct FOServer;
struct FOClient;
struct FOMapper;

struct BaseEntity
{
    void AddRef() { }
    void Release() { }

    uint GetId() { return 0u; }
    hash GetProtoId() { return 0u; }
    FOEngine* GetEngine() { return nullptr; }

    int RefCounter {1};
    bool IsDestroyed {};
    bool IsDestroying {};
};

#define INIT_ARGS const char* script_path
struct SCRIPTING_CLASS
{
    void InitAngelScriptScripting(INIT_ARGS);
};

#define ENTITY_VERIFY(e)
#endif

#if !COMPILER_MODE
#define INIT_ARGS

#define ENTITY_VERIFY(e) \
    if (e != nullptr && e->IsDestroyed) { \
        throw ScriptException("Access to destroyed entity"); \
    }
#endif

#define AS_VERIFY(expr) \
    as_result = expr; \
    if (as_result < 0) { \
        throw ScriptInitException(#expr); \
    }

#if !COMPILER_MODE
#ifdef AS_MAX_PORTABILITY
#define SCRIPT_FUNC(name) WRAP_FN(name)
#define SCRIPT_FUNC_THIS(name) WRAP_OBJ_FIRST(name)
#define SCRIPT_METHOD(type, name) WRAP_MFN(type, name)
#define SCRIPT_FUNC_CONV asCALL_GENERIC
#define SCRIPT_FUNC_THIS_CONV asCALL_GENERIC
#define SCRIPT_METHOD_CONV asCALL_GENERIC
#else
#define SCRIPT_FUNC(name) asFUNCTION(name)
#define SCRIPT_FUNC_THIS(name) asFUNCTION(name)
#define SCRIPT_METHOD(type, name) asMETHOD(type, name)
#define SCRIPT_FUNC_CONV asCALL_CDECL
#define SCRIPT_FUNC_THIS_CONV asCALL_CDECL_OBJFIRST
#define SCRIPT_METHOD_CONV asCALL_THISCALL
#endif
#else
#undef SCRIPT_FUNC
#undef SCRIPT_FUNC_THIS
#undef SCRIPT_METHOD
#undef SCRIPT_FUNC_CONV
#undef SCRIPT_FUNC_THIS_CONV
#undef SCRIPT_METHOD_CONV
#define SCRIPT_FUNC(...) asFUNCTION(DummyFunc)
#define SCRIPT_FUNC_THIS(...) asFUNCTION(DummyFunc)
#define SCRIPT_METHOD(...) asFUNCTION(DummyFunc)
#define SCRIPT_FUNC_CONV asCALL_GENERIC
#define SCRIPT_FUNC_THIS_CONV asCALL_GENERIC
#define SCRIPT_METHOD_CONV asCALL_GENERIC
static void DummyFunc(asIScriptGeneric* gen)
{
}
#endif

#if !COMPILER_MODE
struct ScriptSystem::AngelScriptImpl
{
    asIScriptEngine* Engine {};
};
#endif

template<typename T>
static T* ScriptableObject_Factory()
{
    return new T();
}

// Arrays stuff
static auto CreateASArray(asIScriptEngine* as_engine, const char* type) -> CScriptArray*
{
    RUNTIME_ASSERT(as_engine);
    const auto type_id = as_engine->GetTypeIdByDecl(type);
    RUNTIME_ASSERT(type_id);
    auto* type_info = as_engine->GetTypeInfoById(type_id);
    RUNTIME_ASSERT(type_info);
    auto* as_array = CScriptArray::Create(type_info);
    RUNTIME_ASSERT(as_array);
    return as_array;
}

template<typename Type>
static void VerifyEntityArray(asIScriptEngine* as_engine, CScriptArray* as_array)
{
    for (const auto i : xrange(as_array->GetSize())) {
        ENTITY_VERIFY(*reinterpret_cast<Type*>(as_array->At(i)));
    }
}

template<typename T>
static auto MarshalArray(asIScriptEngine* as_engine, CScriptArray* as_array) -> vector<T>
{
    if (as_array == nullptr || as_array->GetSize() == 0u) {
        return {};
    }

    vector<T> vec;
    vec.resize(as_array->GetSize());
    for (const auto i : xrange(as_array->GetSize())) {
        vec[i] = *reinterpret_cast<T*>(as_array->At(i));
    }

    return vec;
}

template<typename T>
static auto MarshalBackScalarArray(asIScriptEngine* as_engine, const char* type, const vector<T>& vec) -> CScriptArray*
{
    auto* as_array = CreateASArray(as_engine, type);

    if (!vec.empty()) {
        as_array->Resize(static_cast<asUINT>(vec.size()));
        for (const auto i : xrange(vec)) {
            *reinterpret_cast<T*>(as_array->At(static_cast<asUINT>(i))) = vec[i];
        }
    }

    return as_array;
}

template<typename T>
static auto MarshalBackRefArray(asIScriptEngine* as_engine, const char* type, const vector<T>& vec) -> CScriptArray*
{
    auto* as_array = CreateASArray(as_engine, type);

    if (!vec.empty()) {
        as_array->Resize(static_cast<asUINT>(vec.size()));
        for (const auto i : xrange(vec)) {
            *reinterpret_cast<T*>(as_array->At(static_cast<asUINT>(i))) = vec[i];
            if (vec[i]) {
                vec[i]->AddRef();
            }
        }
    }

    return as_array;
}

static auto CreateASDict(asIScriptEngine* as_engine, const char* type) -> CScriptDict*
{
    RUNTIME_ASSERT(as_engine);
    const auto type_id = as_engine->GetTypeIdByDecl(type);
    RUNTIME_ASSERT(type_id);
    auto* type_info = as_engine->GetTypeInfoById(type_id);
    RUNTIME_ASSERT(type_info);
    auto* as_dict = CScriptDict::Create(type_info);
    RUNTIME_ASSERT(as_dict);
    return as_dict;
}

template<typename T, typename U>
static auto MarshalDict(asIScriptEngine* as_engine, CScriptDict* as_dict) -> map<T, U>
{
    if (as_dict == nullptr || as_dict->GetSize() == 0u) {
        return {};
    }

    map<T, U> map;
    for (const auto i : xrange(as_dict->GetSize())) {
        // vec[i] = *reinterpret_cast<Type*>(as_array->At(i));
    }

    return map;
}

template<typename T, typename U>
static auto MarshalBackScalarDict(asIScriptEngine* as_engine, const char* type, const map<T, U>& map) -> CScriptDict*
{
    auto* as_dict = CreateASDict(as_engine, type);

    if (!map.empty()) {
        // as_array->Resize(static_cast<asUINT>(vec.size()));
        // for (const auto i : xrange(vec)) {
        //    auto* p = reinterpret_cast<T*>(as_array->At(static_cast<asUINT>(i)));
        //    *p = vec[i];
        //}
    }

    return as_dict;
}

static auto Entity_Id(BaseEntity* self) -> uint
{
    ENTITY_VERIFY(self);
    return self->GetId();
}

static auto Entity_ProtoId(BaseEntity* self) -> hash
{
    ENTITY_VERIFY(self);
    return self->GetProtoId();
}

static auto Entity_IsDestroyed(BaseEntity* self) -> bool
{
    // May call on destroyed entity
    return self->IsDestroyed;
}

static auto Entity_IsDestroying(BaseEntity* self) -> bool
{
    // May call on destroyed entity
    return self->IsDestroying;
}

///@ CodeGen Global

template<class T>
static BaseEntity* EntityDownCast(T* a)
{
    if (a == nullptr) {
        return nullptr;
    }

    auto* b = static_cast<BaseEntity*>(a);
    b->AddRef();

    return b;
}

template<class T>
static T* EntityUpCast(BaseEntity* a)
{
    if (a == nullptr) {
        return nullptr;
    }

    auto* b = dynamic_cast<T*>(a);
    if (b != nullptr) {
        b->AddRef();
    }

    return b;
}

static const string ContextStatesStr[] = {
    "Finished",
    "Suspended",
    "Aborted",
    "Exception",
    "Prepared",
    "Uninitialized",
    "Active",
    "Error",
};

static void CallbackMessage(const asSMessageInfo* msg, void* param)
{
    const char* type = "Error";
    if (msg->type == asMSGTYPE_WARNING) {
        type = "Warning";
    }
    else if (msg->type == asMSGTYPE_INFORMATION) {
        type = "Info";
    }

    WriteLog("{} : {} : {} : Line {}.\n", Preprocessor::ResolveOriginalFile(msg->row), type, msg->message, Preprocessor::ResolveOriginalLine(msg->row));
}

#if COMPILER_MODE
static void CompileRootModule(asIScriptEngine* engine, string_view script_path);
#else
static void RestoreRootModule(asIScriptEngine* engine, File& script_file);
#endif

void SCRIPTING_CLASS::InitAngelScriptScripting(INIT_ARGS)
{
    asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    RUNTIME_ASSERT(engine);

#if !COMPILER_MODE
    AngelScriptData = std::make_unique<AngelScriptImpl>();
    AngelScriptData->Engine = engine;
#endif

    int as_result;
    AS_VERIFY(engine->SetMessageCallback(asFUNCTION(CallbackMessage), nullptr, asCALL_CDECL));

    AS_VERIFY(engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_DISALLOW_EMPTY_LIST_ELEMENTS, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_PRIVATE_PROP_AS_PROTECTED, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true));

    RegisterScriptArray(engine, true);
    ScriptExtensions::RegisterScriptArrayExtensions(engine);
    RegisterStdString(engine);
    ScriptExtensions::RegisterScriptStdStringExtensions(engine);
    RegisterScriptAny(engine);
    RegisterScriptDictionary(engine);
    RegisterScriptDict(engine);
    ScriptExtensions::RegisterScriptDictExtensions(engine);
    RegisterScriptFile(engine);
    RegisterScriptFileSystem(engine);
    RegisterScriptDateTime(engine);
    RegisterScriptMath(engine);
    RegisterScriptWeakRef(engine);
    RegisterScriptReflection(engine);

    AS_VERIFY(engine->RegisterTypedef("hash", "uint"));
    AS_VERIFY(engine->RegisterTypedef("resource", "uint"));

#define REGISTER_ENTITY(class_name, real_class) \
    AS_VERIFY(engine->RegisterObjectType(class_name, 0, asOBJ_REF)); \
    AS_VERIFY(engine->RegisterObjectBehaviour(class_name, asBEHAVE_ADDREF, "void f()", SCRIPT_METHOD(real_class, AddRef), SCRIPT_METHOD_CONV)); \
    AS_VERIFY(engine->RegisterObjectBehaviour(class_name, asBEHAVE_RELEASE, "void f()", SCRIPT_METHOD(real_class, Release), SCRIPT_METHOD_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "uint get_Id() const", SCRIPT_FUNC_THIS(Entity_Id), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "hash get_ProtoId() const", SCRIPT_FUNC_THIS(Entity_ProtoId), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "bool get_IsDestroyed() const", SCRIPT_FUNC_THIS(Entity_IsDestroyed), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "bool get_IsDestroying() const", SCRIPT_FUNC_THIS(Entity_IsDestroying), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectProperty(class_name, "const int RefCounter", offsetof(real_class, RefCounter)))
#define REGISTER_ENTITY_CAST(class_name, real_class) \
    AS_VERIFY(engine->RegisterObjectMethod("Entity", class_name "@ opCast()", SCRIPT_FUNC_THIS((EntityUpCast<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod("Entity", "const " class_name "@ opCast() const", SCRIPT_FUNC_THIS((EntityUpCast<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "Entity@ opImplCast()", SCRIPT_FUNC_THIS((EntityDownCast<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "const Entity@ opImplCast() const", SCRIPT_FUNC_THIS((EntityDownCast<real_class>)), SCRIPT_FUNC_THIS_CONV))

    REGISTER_ENTITY("Entity", BaseEntity);

    ///@ CodeGen Register

    /*
    #define FO_API_GLOBAL_COMMON_FUNC(name, ret, ...) AS_VERIFY(engine->RegisterGlobalFunction(MakeMethodDecl(#name, ret, MergeASTypes({__VA_ARGS__})).c_str(), SCRIPT_FUNC(AS_##name), SCRIPT_FUNC_CONV));
    #if SERVER_SCRIPTING
    #define FO_API_GLOBAL_SERVER_FUNC(name, ret, ...) AS_VERIFY(engine->RegisterGlobalFunction(MakeMethodDecl(#name, ret, MergeASTypes({__VA_ARGS__})).c_str(), SCRIPT_FUNC(AS_##name), SCRIPT_FUNC_CONV));
    #elif CLIENT_SCRIPTING
    #define FO_API_GLOBAL_CLIENT_FUNC(name, ret, ...) AS_VERIFY(engine->RegisterGlobalFunction(MakeMethodDecl(#name, ret, MergeASTypes({__VA_ARGS__})).c_str(), SCRIPT_FUNC(AS_##name), SCRIPT_FUNC_CONV));
    #elif MAPPER_SCRIPTING
    #define FO_API_GLOBAL_MAPPER_FUNC(name, ret, ...) AS_VERIFY(engine->RegisterGlobalFunction(MakeMethodDecl(#name, ret, MergeASTypes({__VA_ARGS__})).c_str(), SCRIPT_FUNC(AS_##name), SCRIPT_FUNC_CONV));
    #endif
    */
    /*
    #define CHECK_GETTER(access) (IS_SERVER && !(Property::AccessType::access & Property::AccessType::ClientOnlyMask)) || (IS_CLIENT && !(Property::AccessType::access & Property::AccessType::ServerOnlyMask)) || (IS_MAPPER && !(Property::AccessType::access & Property::AccessType::VirtualMask))
    #define CHECK_SETTER(access) (IS_SERVER && !(Property::AccessType::access & Property::AccessType::ClientOnlyMask)) || (IS_CLIENT && !(Property::AccessType::access & Property::AccessType::ServerOnlyMask) && ((Property::AccessType::access & Property::AccessType::ClientOnlyMask) || (Property::AccessType::access & Property::AccessType::ModifiableMask))) || (IS_MAPPER && !(Property::AccessType::access & Property::AccessType::VirtualMask))

    #define FO_API_PLAYER_READONLY_PROPERTY(access, type, name, ...) \
        if (CHECK_GETTER(access)) \
            AS_VERIFY(engine->RegisterObjectMethod("Player", MakeMethodDecl("get_" #name, type, "").c_str(), SCRIPT_METHOD(ASPlayer, Get_##name), SCRIPT_METHOD_CONV));
    #define FO_API_PLAYER_PROPERTY(access, type, name, ...) \
        FO_API_PLAYER_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
        if (CHECK_SETTER(access)) \
            AS_VERIFY(engine->RegisterObjectMethod("Player", MakeMethodDecl("set_" #name, "void", type).c_str(), SCRIPT_METHOD(ASPlayer, Set_##name), SCRIPT_METHOD_CONV));
    #define FO_API_ITEM_READONLY_PROPERTY(access, type, name, ...) \
        if (CHECK_GETTER(access)) \
            AS_VERIFY(engine->RegisterObjectMethod("Item", MakeMethodDecl("get_" #name, type, "").c_str(), SCRIPT_METHOD(ASItem, Get_##name), SCRIPT_METHOD_CONV));
    #define FO_API_ITEM_PROPERTY(access, type, name, ...) \
        FO_API_ITEM_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
        if (CHECK_SETTER(access)) \
            AS_VERIFY(engine->RegisterObjectMethod("Item", MakeMethodDecl("set_" #name, "void", type).c_str(), SCRIPT_METHOD(ASItem, Set_##name), SCRIPT_METHOD_CONV));
    #define FO_API_CRITTER_READONLY_PROPERTY(access, type, name, ...) \
        if (CHECK_GETTER(access)) \
            AS_VERIFY(engine->RegisterObjectMethod("Critter", MakeMethodDecl("get_" #name, type, "").c_str(), SCRIPT_METHOD(ASCritter, Get_##name), SCRIPT_METHOD_CONV));
    #define FO_API_CRITTER_PROPERTY(access, type, name, ...) \
        FO_API_CRITTER_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
        if (CHECK_SETTER(access)) \
            AS_VERIFY(engine->RegisterObjectMethod("Critter", MakeMethodDecl("set_" #name, "void", type).c_str(), SCRIPT_METHOD(ASCritter, Set_##name), SCRIPT_METHOD_CONV));
    #define FO_API_MAP_READONLY_PROPERTY(access, type, name, ...) \
        if (CHECK_GETTER(access)) \
            AS_VERIFY(engine->RegisterObjectMethod("Map", MakeMethodDecl("get_" #name, type, "").c_str(), SCRIPT_METHOD(ASMap, Get_##name), SCRIPT_METHOD_CONV));
    #define FO_API_MAP_PROPERTY(access, type, name, ...) \
        FO_API_MAP_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
        if (CHECK_SETTER(access)) \
            AS_VERIFY(engine->RegisterObjectMethod("Map", MakeMethodDecl("set_" #name, "void", type).c_str(), SCRIPT_METHOD(ASMap, Set_##name), SCRIPT_METHOD_CONV));
    #define FO_API_LOCATION_READONLY_PROPERTY(access, type, name, ...) \
        if (CHECK_GETTER(access)) \
            AS_VERIFY(engine->RegisterObjectMethod("Location", MakeMethodDecl("get_" #name, type, "").c_str(), SCRIPT_METHOD(ASLocation, Get_##name), SCRIPT_METHOD_CONV));
    #define FO_API_LOCATION_PROPERTY(access, type, name, ...) \
        FO_API_LOCATION_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
        if (CHECK_SETTER(access)) \
            AS_VERIFY(engine->RegisterObjectMethod("Location", MakeMethodDecl("set_" #name, "void", type).c_str(), SCRIPT_METHOD(ASLocation, Set_##name), SCRIPT_METHOD_CONV));
    #include "ScriptApi.h"
    */

#if CLIENT_SCRIPTING
    // AS_VERIFY(engine->RegisterGlobalProperty("Map@ CurMap", &BIND_CLASS ClientCurMap));
    // AS_VERIFY(engine->RegisterGlobalProperty("Location@ CurLocation", &BIND_CLASS ClientCurLocation));
#endif

#if COMPILER_MODE
    CompileRootModule(engine, script_path);
    engine->ShutDownAndRelease();
#else
    File script_file = _engine->FileMngr.ReadFile("...");
    RestoreRootModule(engine, script_file);
#endif
}

class BinaryStream : public asIBinaryStream
{
public:
    explicit BinaryStream(std::vector<asBYTE>& buf) : _binBuf {buf} { }

    void Write(const void* ptr, asUINT size) override
    {
        if (!ptr || !size)
            return;
        _binBuf.resize(_binBuf.size() + size);
        std::memcpy(&_binBuf[_writePos], ptr, size);
        _writePos += size;
    }

    void Read(void* ptr, asUINT size) override
    {
        if (!ptr || !size)
            return;
        std::memcpy(ptr, &_binBuf[_readPos], size);
        _readPos += size;
    }

    auto GetBuf() -> std::vector<asBYTE>& { return _binBuf; }

private:
    std::vector<asBYTE>& _binBuf;
    int _readPos {};
    int _writePos {};
};

#if COMPILER_MODE
static void CompileRootModule(asIScriptEngine* engine, string_view script_path)
{
    RUNTIME_ASSERT(engine->GetModuleCount() == 0);

    // File loader
    class ScriptLoader : public Preprocessor::FileLoader
    {
    public:
        ScriptLoader(vector<uchar>& root) : _rootScript {&root} { }

        auto LoadFile(const string& dir, const string& file_name, vector<char>& data, string& file_path) -> bool override
        {
            if (_rootScript) {
                data.resize(_rootScript->size());
                std::memcpy(data.data(), _rootScript->data(), _rootScript->size());
                _rootScript = nullptr;
                file_path = "(Root)";
                return true;
            }

            _includeDeep++;

            file_path = _str(file_name).extractFileName().eraseFileExtension();
            data.resize(0);

            if (_includeDeep == 1) {
                auto script_file = DiskFileSystem::OpenFile(file_name, false);
                if (!script_file) {
                    return false;
                }
                if (script_file.GetSize() == 0) {
                    return true;
                }

                data.resize(script_file.GetSize());
                if (!script_file.Read(data.data(), script_file.GetSize())) {
                    return false;
                }

                return true;
            }

            return Preprocessor::FileLoader::LoadFile(dir, file_name, data, file_path);
        }

        void FileLoaded() override { _includeDeep--; }

    private:
        vector<uchar>* _rootScript {};
        int _includeDeep {0};
    };

    auto script_file = DiskFileSystem::OpenFile(script_path, false);
    if (!script_file) {
        throw ScriptCompilerException("Root script file not found", script_path);
    }
    if (script_file.GetSize() == 0) {
        throw ScriptCompilerException("Root script file is empty", script_path);
    }

    vector<uchar> script_data(script_file.GetSize());
    if (!script_file.Read(script_data.data(), script_file.GetSize())) {
        throw ScriptCompilerException("Can't read root script file", script_path);
    }

    Preprocessor::UndefAll();
#if SERVER_SCRIPTING
    Preprocessor::Define("SERVER");
#elif CLIENT_SCRIPTING
    Preprocessor::Define("CLIENT");
#elif MAPPER_SCRIPTING
    Preprocessor::Define("MAPPER");
#endif

    ScriptLoader loader {script_data};
    Preprocessor::StringOutStream result, errors;
    const auto errors_count = Preprocessor::Preprocess("Root", result, &errors, &loader);

    while (!errors.String.empty() && errors.String.back() == '\n') {
        errors.String.pop_back();
    }

    if (errors_count) {
        throw ScriptCompilerException("Preprocessor failed", errors.String);
    }
    else if (!errors.String.empty()) {
        WriteLog("Preprocessor message: {}.\n", errors.String);
    }

    asIScriptModule* mod = engine->GetModule("Root", asGM_ALWAYS_CREATE);
    if (mod == nullptr) {
        throw ScriptCompilerException("Create root module failed");
    }

    int as_result = mod->AddScriptSection("Root", result.String.c_str());
    if (as_result < 0) {
        throw ScriptCompilerException("Unable to add script section", as_result);
    }

    as_result = mod->Build();
    if (as_result < 0) {
        throw ScriptCompilerException("Unable to build module", as_result);
    }

    vector<asBYTE> buf;
    BinaryStream binary {buf};
    as_result = mod->SaveByteCode(&binary);
    if (as_result < 0) {
        throw ScriptCompilerException("Unable to save byte code", as_result);
    }

    Preprocessor::LineNumberTranslator* lnt = Preprocessor::GetLineNumberTranslator();
    vector<uchar> lnt_data;
    Preprocessor::StoreLineNumberTranslator(lnt, lnt_data);

    vector<uchar> data;
    DataWriter writer {data};
    writer.Write(static_cast<uint>(buf.size()));
    writer.WritePtr(buf.data(), buf.size());
    writer.Write(static_cast<uint>(lnt_data.size()));
    writer.WritePtr(lnt_data.data(), lnt_data.size());

    auto file = DiskFileSystem::OpenFile(string(script_path) + "b", true);
    if (!file) {
        throw ScriptCompilerException("Can't write binary to file", _str("{}b", script_path));
    }
    if (!file.Write(data.data(), static_cast<uint>(data.size()))) {
        throw ScriptCompilerException("Can't write binary to file", _str("{}b", script_path));
    }
}

#else
static void RestoreRootModule(asIScriptEngine* engine, File& script_file)
{
    RUNTIME_ASSERT(engine->GetModuleCount() == 0);
    RUNTIME_ASSERT(script_file);

    DataReader reader {script_file.GetBuf()};
    vector<asBYTE> buf(reader.Read<uint>());
    std::memcpy(buf.data(), reader.ReadPtr<asBYTE>(buf.size()), buf.size());
    vector<uchar> lnt_data(reader.Read<uint>());
    std::memcpy(lnt_data.data(), reader.ReadPtr<uchar>(lnt_data.size()), lnt_data.size());
    RUNTIME_ASSERT(!buf.empty());
    RUNTIME_ASSERT(!lnt_data.empty());

    asIScriptModule* mod = engine->GetModule("Root", asGM_ALWAYS_CREATE);
    if (mod == nullptr) {
        throw ScriptException("Create root module fail");
    }

    Preprocessor::LineNumberTranslator* lnt = Preprocessor::RestoreLineNumberTranslator(lnt_data);

    BinaryStream binary {buf};
    int as_result = mod->LoadByteCode(&binary);
    if (as_result < 0) {
        throw ScriptException("Can't load binary", as_result);
    }
}
#endif
