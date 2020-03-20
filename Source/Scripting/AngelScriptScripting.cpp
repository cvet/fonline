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

#ifdef FO_SERVER_SCRIPTING
#include "ServerScripting.h"
#endif
#ifdef FO_CLIENT_SCRIPTING
#include "ClientScripting.h"
#endif
#ifdef FO_MAPPER_SCRIPTING
#include "MapperScripting.h"
#endif

#ifdef FO_SERVER_SCRIPTING
#include "Server.h"
#define FO_API_COMMON_IMPL
#define FO_API_SERVER_IMPL
#include "ScriptApi.h"
#define SCRIPTING_CLASS ServerScriptSystem
#endif
#ifdef FO_CLIENT_SCRIPTING
#include "Client.h"
#define FO_API_COMMON_IMPL
#define FO_API_CLIENT_IMPL
#include "ScriptApi.h"
#define SCRIPTING_CLASS ClientScriptSystem
#endif
#ifdef FO_MAPPER_SCRIPTING
#include "Mapper.h"
#define FO_API_COMMON_IMPL
#define FO_API_MAPPER_IMPL
#include "ScriptApi.h"
#define SCRIPTING_CLASS MapperScriptSystem
#endif

#ifdef FO_ANGELSCRIPT_SCRIPTING
#include "AngelScriptExtensions.h"
#include "AngelScriptReflection.h"
#include "AngelScriptScriptDict.h"
#include "Log.h"
#include "Testing.h"

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

#define AS_VERIFY(expr) \
    as_result = expr; \
    RUNTIME_ASSERT_STR(as_result >= 0, #expr)

#ifndef FO_SCRIPTING_REF
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

struct ScriptSystem::AngelScriptImpl
{
    asIScriptEngine* Engine {};
};

struct ScriptEntity
{
    void AddRef() {}
    void Release() {}
    int RefCounter {1};
    Entity* GameEntity {};
    bool OwnedEntity {};
};

template<typename T,
    std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_same_v<T, string>, int> = 0>
inline T Marshal(T value)
{
    return value;
}

template<typename T>
inline T Marshal(CScriptArray* arr)
{
    return {};
}

template<typename T>
inline T* MarshalObj(ScriptEntity* value)
{
    return 0;
}

template<typename T>
inline vector<T*> MarshalObjArr(CScriptArray* arr)
{
    return {};
}

inline std::function<void()> MarshalCallback(asIScriptFunction* func)
{
    return {};
}

template<typename T>
inline std::function<bool(T*)> MarshalPredicate(asIScriptFunction* func)
{
    return {};
}

template<typename TKey, typename TValue>
inline map<TKey, TValue> MarshalDict(CScriptDict* dict)
{
    return {};
}

template<typename T>
inline CScriptArray* MarshalBack(vector<T> arr)
{
    return 0;
}

inline ScriptEntity* MarshalBack(Entity* obj)
{
    return 0;
}

template<typename T,
    std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_same_v<T, string>, int> = 0>
inline T MarshalBack(T value)
{
    return value;
}

template<typename T>
inline string GetASType(string name)
{
    static unordered_map<size_t, string> as_types = {{typeid(char).hash_code(), "int8"},
        {typeid(short).hash_code(), "int16"}, {typeid(int).hash_code(), "int"}, {typeid(int64).hash_code(), "int64"},
        {typeid(uchar).hash_code(), "uint8"}, {typeid(ushort).hash_code(), "uint16"},
        {typeid(uint).hash_code(), "uint"}, {typeid(uint64).hash_code(), "uint64"},
        {typeid(string).hash_code(), "string"}};
    string type = as_types[typeid(T).hash_code()];
    return name.empty() ? type : type + " " + name;
}

inline string MergeASTypes(vector<string> args)
{
    string result;
    for (size_t i = 0; i < args.size(); i++)
        result += (i > 0 ? ", " : "") + args[i];
    return result;
}

inline string MakeMethodDecl(string name, string ret, string decl)
{
    return fmt::format("{} {}({}}", ret, name, decl);
}

template<class T>
static Entity* EntityDownCast(T* a)
{
    if (!a)
        return nullptr;
    Entity* b = (Entity*)a;
    b->AddRef();
    return b;
}

template<class T>
static T* EntityUpCast(Entity* a)
{
    if (!a)
        return nullptr;
#define CHECK_CAST(cast_class, entity_type) \
    if (std::is_same<T, cast_class>::value && (EntityType)a->Type == entity_type) \
    { \
        T* b = (T*)a; \
        b->AddRef(); \
        return b; \
    }
#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    CHECK_CAST(Location, EntityType::Location);
    CHECK_CAST(Map, EntityType::Map);
    CHECK_CAST(Critter, EntityType::Npc);
    CHECK_CAST(Critter, EntityType::Client);
    CHECK_CAST(Item, EntityType::Item);
#endif
#if defined(FONLINE_CLIENT) || defined(FONLINE_EDITOR)
    CHECK_CAST(LocationView, EntityType::LocationView);
    CHECK_CAST(MapView, EntityType::MapView);
    CHECK_CAST(CritterView, EntityType::CritterView);
    CHECK_CAST(ItemView, EntityType::ItemView);
    CHECK_CAST(ItemView, EntityType::ItemHexView);
#endif
#undef CHECK_CAST
    return nullptr;
}

static void ReportException(std::exception& ex)
{
}

#define FO_API_PARTLY_UNDEF
#define FO_API_ARG(type, name) type name
#define FO_API_ARG_ARR(type, name) CScriptArray* _##name
#define FO_API_ARG_OBJ(type, name) ScriptEntity* _##name
#define FO_API_ARG_OBJ_ARR(type, name) CScriptArray* _##name
#define FO_API_ARG_REF(type, name) type& name
#define FO_API_ARG_ARR_REF(type, name) CScriptArray* _##name
#define FO_API_ARG_ENUM(type, name) int name
#define FO_API_ARG_CALLBACK(name) asIScriptFunction* _##name
#define FO_API_ARG_PREDICATE(type, name) asIScriptFunction* _##name
#define FO_API_ARG_DICT(key, val, name) CScriptDict* _##name
#define FO_API_ARG_MARSHAL(type, name)
#define FO_API_ARG_ARR_MARSHAL(type, name) vector<type> name = Marshal<vector<type>>(_##name);
#define FO_API_ARG_OBJ_MARSHAL(type, name) type* name = MarshalObj<type>(_##name);
#define FO_API_ARG_OBJ_ARR_MARSHAL(type, name) vector<type*> name = MarshalObjArr<type>(_##name);
#define FO_API_ARG_REF_MARSHAL(type, name)
#define FO_API_ARG_ARR_REF_MARSHAL(type, name) vector<type> name = Marshal<vector<type>>(_##name);
#define FO_API_ARG_ENUM_MARSHAL(type, name)
#define FO_API_ARG_CALLBACK_MARSHAL(name) std::function<void()> name = MarshalCallback(_##name);
#define FO_API_ARG_PREDICATE_MARSHAL(type, name) std::function<bool(type*)> name = MarshalPredicate<type>(_##name);
#define FO_API_ARG_DICT_MARSHAL(key, val, name) map<key, val> name = MarshalDict<key, val>(_##name);
#define FO_API_RET(type) type
#define FO_API_RET_ARR(type) CScriptArray*
#define FO_API_RET_OBJ(type) ScriptEntity*
#define FO_API_RET_OBJ_ARR(type) CScriptArray*
#define FO_API_RETURN(expr) return MarshalBack(expr)
#define FO_API_RETURN_VOID() return
#define FO_API_PROPERTY_TYPE(type) type
#define FO_API_PROPERTY_TYPE_ARR(type) CScriptArray*
#define FO_API_PROPERTY_TYPE_OBJ(type) ScriptEntity*
#define FO_API_PROPERTY_TYPE_OBJ_ARR(type) CScriptArray*
#define FO_API_PROPERTY_TYPE_ENUM(type) int
#define FO_API_PROPERTY_MOD(mod)

#if defined(FO_SERVER_SCRIPTING)
#define CONTEXT_ARG \
    FOServer* _server = (FOServer*)asGetActiveContext()->GetEngine()->GetUserData(); \
    FOServer* _common = _server
#elif defined(FO_CLIENT_SCRIPTING)
#define CONTEXT_ARG \
    FOClient* _client = (FOClient*)asGetActiveContext()->GetEngine()->GetUserData(); \
    FOClient* _common = _client
#elif defined(FO_MAPPER_SCRIPTING)
#define CONTEXT_ARG \
    FOMapper* _mapper = (FOMapper*)asGetActiveContext()->GetEngine()->GetUserData(); \
    FOMapper* _common = _mapper
#endif

#define FO_API_PROLOG(...) \
    { \
        try \
        { \
            CONTEXT_ARG; \
            THIS_ARG; \
            __VA_ARGS__
#define FO_API_EPILOG(...) \
    } \
    catch (std::exception & ex) \
    { \
        ReportException(ex); \
        return __VA_ARGS__; \
    } \
    }

struct ScriptItem : ScriptEntity
{
#if defined(FO_SERVER_SCRIPTING)
#define THIS_ARG Item* _this = (Item*)GameEntity
#define FO_API_ITEM_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_ITEM_METHOD_IMPL
#define ITEM_CLASS Item
#elif defined(FO_CLIENT_SCRIPTING)
#define THIS_ARG ItemView* _this = (ItemView*)GameEntity
#define FO_API_ITEM_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_ITEM_VIEW_METHOD_IMPL
#define ITEM_CLASS ItemView
#elif defined(FO_MAPPER_SCRIPTING)
#define ITEM_CLASS ItemView
#endif
#define FO_API_ITEM_READONLY_PROPERTY(access, type, name, ...) \
    type Get_##name() { return MarshalBack(((ITEM_CLASS*)GameEntity)->Get##name()); }
#define FO_API_ITEM_PROPERTY(access, type, name, ...) \
    type Get_##name() { return MarshalBack(((ITEM_CLASS*)GameEntity)->Get##name()); } \
    void Set_##name(type value) \
    { \
        ((ITEM_CLASS*)GameEntity)->Set##name(Marshal<decltype(((ITEM_CLASS*)GameEntity)->Get##name())>(value)); \
    }
#include "ScriptApi.h"
#undef THIS_ARG
#undef ITEM_CLASS
};

struct ScriptCritter : ScriptEntity
{
#if defined(FO_SERVER_SCRIPTING)
#define THIS_ARG Critter* _this = (Critter*)GameEntity
#define FO_API_CRITTER_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_CRITTER_METHOD_IMPL
#define CRITTER_CLASS Critter
#elif defined(FO_CLIENT_SCRIPTING)
#define THIS_ARG CritterView* _this = (CritterView*)GameEntity
#define FO_API_CRITTER_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_CRITTER_VIEW_METHOD_IMPL
#define CRITTER_CLASS CritterView
#elif defined(FO_MAPPER_SCRIPTING)
#define CRITTER_CLASS CritterView
#endif
#define FO_API_CRITTER_READONLY_PROPERTY(access, type, name, ...) \
    type Get_##name() { return MarshalBack(((CRITTER_CLASS*)GameEntity)->Get##name()); }
#define FO_API_CRITTER_PROPERTY(access, type, name, ...) \
    type Get_##name() { return MarshalBack(((CRITTER_CLASS*)GameEntity)->Get##name()); } \
    void Set_##name(type value) \
    { \
        ((CRITTER_CLASS*)GameEntity)->Set##name(Marshal<decltype(((CRITTER_CLASS*)GameEntity)->Get##name())>(value)); \
    }
#include "ScriptApi.h"
#undef THIS_ARG
#undef CRITTER_CLASS
};

struct ScriptMap : ScriptEntity
{
#if defined(FO_SERVER_SCRIPTING)
#define THIS_ARG Map* _this = (Map*)GameEntity
#define FO_API_MAP_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_MAP_METHOD_IMPL
#define MAP_CLASS Map
#elif defined(FO_CLIENT_SCRIPTING)
#define THIS_ARG MapView* _this = (MapView*)GameEntity
#define FO_API_MAP_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_MAP_VIEW_METHOD_IMPL
#define MAP_CLASS MapView
#elif defined(FO_MAPPER_SCRIPTING)
#define MAP_CLASS MapView
#endif
#define FO_API_MAP_READONLY_PROPERTY(access, type, name, ...) \
    type Get_##name() { return MarshalBack(((MAP_CLASS*)GameEntity)->Get##name()); }
#define FO_API_MAP_PROPERTY(access, type, name, ...) \
    type Get_##name() { return MarshalBack(((MAP_CLASS*)GameEntity)->Get##name()); } \
    void Set_##name(type value) \
    { \
        ((MAP_CLASS*)GameEntity)->Set##name(Marshal<decltype(((MAP_CLASS*)GameEntity)->Get##name())>(value)); \
    }
#include "ScriptApi.h"
#undef THIS_ARG
#undef MAP_CLASS
};

struct ScriptLocation : ScriptEntity
{
#if defined(FO_SERVER_SCRIPTING)
#define THIS_ARG Location* _this = (Location*)GameEntity
#define FO_API_LOCATION_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_LOCATION_METHOD_IMPL
#define LOCATION_CLASS Location
#elif defined(FO_CLIENT_SCRIPTING)
#define THIS_ARG LocationView* _this = (LocationView*)GameEntity
#define FO_API_LOCATION_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_LOCATION_VIEW_METHOD_IMPL
#define LOCATION_CLASS LocationView
#elif defined(FO_MAPPER_SCRIPTING)
#define LOCATION_CLASS LocationView
#endif
#define FO_API_LOCATION_READONLY_PROPERTY(access, type, name, ...) \
    type Get_##name() { return MarshalBack(((LOCATION_CLASS*)GameEntity)->Get##name()); }
#define FO_API_LOCATION_PROPERTY(access, type, name, ...) \
    type Get_##name() { return MarshalBack(((LOCATION_CLASS*)GameEntity)->Get##name()); } \
    void Set_##name(type value) \
    { \
        ((LOCATION_CLASS*)GameEntity) \
            ->Set##name(Marshal<decltype(((LOCATION_CLASS*)GameEntity)->Get##name())>(value)); \
    }
#include "ScriptApi.h"
#undef THIS_ARG
#undef LOCATION_CLASS
};

struct ScriptGlobal
{
#define THIS_ARG (void)0
#define FO_API_GLOBAL_COMMON_FUNC(name, ret, ...) static ret name(__VA_ARGS__)
#define FO_API_GLOBAL_COMMON_FUNC_IMPL
#if defined(FO_SERVER_SCRIPTING)
#define FO_API_GLOBAL_SERVER_FUNC(name, ret, ...) static ret name(__VA_ARGS__)
#define FO_API_GLOBAL_SERVER_FUNC_IMPL
#elif defined(FO_CLIENT_SCRIPTING)
#define FO_API_GLOBAL_CLIENT_FUNC(name, ret, ...) static ret name(__VA_ARGS__)
#define FO_API_GLOBAL_CLIENT_FUNC_IMPL
#elif defined(FO_MAPPER_SCRIPTING)
#define FO_API_GLOBAL_MAPPER_FUNC(name, ret, ...) static ret name(__VA_ARGS__)
#define FO_API_GLOBAL_MAPPER_FUNC_IMPL
#endif
#include "ScriptApi.h"
#undef THIS_ARG
};

#undef FO_API_PARTLY_UNDEF
#include "ScriptApi.h"

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
    if (msg->type == asMSGTYPE_WARNING)
        type = "Warning";
    else if (msg->type == asMSGTYPE_INFORMATION)
        type = "Info";

    WriteLog("{} : {} : {} : Line {}.\n", Preprocessor::ResolveOriginalFile(msg->row), type, msg->message,
        Preprocessor::ResolveOriginalLine(msg->row));
}

void SCRIPTING_CLASS::InitAngelScriptScripting()
{
    pAngelScriptImpl = std::make_unique<AngelScriptImpl>();

    asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    RUNTIME_ASSERT(engine);
    pAngelScriptImpl->Engine = engine;
    // asEngine->ShutDownAndRelease();

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

    engine->SetUserData(mainObj);

    AS_VERIFY(engine->RegisterTypedef("hash", "uint"));
    AS_VERIFY(engine->RegisterTypedef("resource", "uint"));

#define REGISTER_ENTITY(class_name, real_class) \
    AS_VERIFY(engine->RegisterObjectType(class_name, 0, asOBJ_REF)); \
    AS_VERIFY(engine->RegisterObjectBehaviour( \
        class_name, asBEHAVE_ADDREF, "void f()", SCRIPT_METHOD(real_class, AddRef), SCRIPT_METHOD_CONV)); \
    AS_VERIFY(engine->RegisterObjectBehaviour( \
        class_name, asBEHAVE_RELEASE, "void f()", SCRIPT_METHOD(real_class, Release), SCRIPT_METHOD_CONV)); \
    AS_VERIFY(engine->RegisterObjectProperty(class_name, "const uint Id", OFFSETOF(real_class, Id))); \
    AS_VERIFY(engine->RegisterObjectMethod( \
        class_name, "hash get_ProtoId() const", SCRIPT_METHOD(real_class, GetProtoId), SCRIPT_METHOD_CONV)); \
    AS_VERIFY( \
        engine->RegisterObjectProperty(class_name, "const bool IsDestroyed", OFFSETOF(real_class, IsDestroyed))); \
    AS_VERIFY( \
        engine->RegisterObjectProperty(class_name, "const bool IsDestroying", OFFSETOF(real_class, IsDestroying))); \
    AS_VERIFY(engine->RegisterObjectProperty(class_name, "const int RefCounter", OFFSETOF(real_class, RefCounter)));
#define REGISTER_ENTITY_CAST(class_name, real_class) \
    AS_VERIFY(engine->RegisterObjectMethod( \
        "Entity", class_name "@ opCast()", SCRIPT_FUNC_THIS((EntityUpCast<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod("Entity", "const " class_name "@ opCast() const", \
        SCRIPT_FUNC_THIS((EntityUpCast<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod( \
        class_name, "Entity@ opImplCast()", SCRIPT_FUNC_THIS((EntityDownCast<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "const Entity@ opImplCast() const", \
        SCRIPT_FUNC_THIS((EntityDownCast<real_class>)), SCRIPT_FUNC_THIS_CONV));

    REGISTER_ENTITY("Entity", Entity);
#if defined(FO_CLIENT_SCRIPTING) || defined(FO_MAPPER_SCRIPTING)
    REGISTER_ENTITY("Item", ItemView);
    REGISTER_ENTITY_CAST("Item", ItemView);
    REGISTER_ENTITY("Critter", CritterView);
    REGISTER_ENTITY_CAST("Critter", CritterView);
    REGISTER_ENTITY("Map", MapView);
    REGISTER_ENTITY_CAST("Map", MapView);
    REGISTER_ENTITY("Location", LocationView);
    REGISTER_ENTITY_CAST("Location", LocationView);
#elif defined(FO_SERVER_SCRIPTING)
    REGISTER_ENTITY("Item", Item);
    REGISTER_ENTITY_CAST("Item", Item);
    REGISTER_ENTITY("Critter", Critter);
    REGISTER_ENTITY_CAST("Critter", Critter);
    REGISTER_ENTITY("Map", Map);
    REGISTER_ENTITY_CAST("Map", Map);
    REGISTER_ENTITY("Location", Location);
    REGISTER_ENTITY_CAST("Location", Location);
#endif

    // AS_VERIFY(engine->RegisterFuncdef("bool ItemPredicate(const Item@+)"));
    // AS_VERIFY(engine->RegisterFuncdef("void ItemInitFunc(Item@+, bool)"));
#if defined(FO_CLIENT_SCRIPTING)
    // AS_VERIFY(engine->RegisterGlobalProperty("Map@ CurMap", &BIND_CLASS ClientCurMap));
    // AS_VERIFY(engine->RegisterGlobalProperty("Location@ CurLocation", &BIND_CLASS ClientCurLocation));
#endif

#define FO_API_ARG(type, name) GetASType<type>(#name)
#define FO_API_ARG_ARR(type, name) GetASType<vector<type>>(#name)
#define FO_API_ARG_OBJ(type, name) GetASType<type>(#name)
#define FO_API_ARG_OBJ_ARR(type, name) GetASType<vector<type>>(#name)
#define FO_API_ARG_REF(type, name) GetASType<type&>(#name)
#define FO_API_ARG_ARR_REF(type, name) GetASType<vector<type>&>(#name)
#define FO_API_ARG_ENUM(type, name) fmt::format("{} {}", #type, #name)
#define FO_API_ARG_CALLBACK(name) fmt::format("Callback {}", #name)
#define FO_API_ARG_PREDICATE(type, name) fmt::format("{}Predicate {}", GetASType<type>(""), #name)
#define FO_API_ARG_DICT(key, val, name) GetASType<map<key, val>>(#name)
#define FO_API_RET(type) GetASType<type>("")
#define FO_API_RET_ARR(type) GetASType<vector<type>>("")
#define FO_API_RET_OBJ(type) GetASType<type>("")
#define FO_API_RET_OBJ_ARR(type) GetASType<vector<type>>("")

#if defined(FO_SERVER_SCRIPTING)
#define FO_API_ITEM_METHOD(name, ret, ...) \
    AS_VERIFY(engine->RegisterObjectMethod("Item", MakeMethodDecl(#name, ret, MergeASTypes({__VA_ARGS__})).c_str(), \
        SCRIPT_METHOD(ScriptItem, name), SCRIPT_METHOD_CONV));
#define FO_API_CRITTER_METHOD(name, ret, ...) \
    AS_VERIFY(engine->RegisterObjectMethod("Critter", MakeMethodDecl(#name, ret, MergeASTypes({__VA_ARGS__})).c_str(), \
        SCRIPT_METHOD(ScriptCritter, name), SCRIPT_METHOD_CONV));
#define FO_API_MAP_METHOD(name, ret, ...) \
    AS_VERIFY(engine->RegisterObjectMethod("Map", MakeMethodDecl(#name, ret, MergeASTypes({__VA_ARGS__})).c_str(), \
        SCRIPT_METHOD(ScriptMap, name), SCRIPT_METHOD_CONV));
#define FO_API_LOCATION_METHOD(name, ret, ...) \
    AS_VERIFY( \
        engine->RegisterObjectMethod("Location", MakeMethodDecl(#name, ret, MergeASTypes({__VA_ARGS__})).c_str(), \
            SCRIPT_METHOD(ScriptLocation, name), SCRIPT_METHOD_CONV));
#elif defined(FO_CLIENT_SCRIPTING)
#define FO_API_ITEM_VIEW_METHOD(name, ret, ...) \
    AS_VERIFY(engine->RegisterObjectMethod("Item", MakeMethodDecl(#name, ret, MergeASTypes({__VA_ARGS__})).c_str(), \
        SCRIPT_METHOD(ScriptItem, name), SCRIPT_METHOD_CONV));
#define FO_API_CRITTER_VIEW_METHOD(name, ret, ...) \
    AS_VERIFY(engine->RegisterObjectMethod("Critter", MakeMethodDecl(#name, ret, MergeASTypes({__VA_ARGS__})).c_str(), \
        SCRIPT_METHOD(ScriptCritter, name), SCRIPT_METHOD_CONV));
#define FO_API_MAP_VIEW_METHOD(name, ret, ...) \
    AS_VERIFY(engine->RegisterObjectMethod("Map", MakeMethodDecl(#name, ret, MergeASTypes({__VA_ARGS__})).c_str(), \
        SCRIPT_METHOD(ScriptMap, name), SCRIPT_METHOD_CONV));
#define FO_API_LOCATION_VIEW_METHOD(name, ret, ...) \
    AS_VERIFY( \
        engine->RegisterObjectMethod("Location", MakeMethodDecl(#name, ret, MergeASTypes({__VA_ARGS__})).c_str(), \
            SCRIPT_METHOD(ScriptLocation, name), SCRIPT_METHOD_CONV));
#endif
#include "ScriptApi.h"

#define FO_API_PROPERTY_TYPE(type) GetASType<type>("")
#define FO_API_PROPERTY_TYPE_ARR(type) GetASType<vector<type>>("")
#define FO_API_PROPERTY_TYPE_OBJ(type) GetASType<type*>("")
#define FO_API_PROPERTY_TYPE_OBJ_ARR(type) GetASType<vector<type*>>("")
#define FO_API_PROPERTY_TYPE_ENUM(type) string(#type)

#define FO_API_ITEM_READONLY_PROPERTY(access, type, name, ...) \
    AS_VERIFY(engine->RegisterObjectMethod("Item", MakeMethodDecl("get_" #name, type, "").c_str(), \
        SCRIPT_METHOD(ScriptItem, Get_##name), SCRIPT_METHOD_CONV));
#define FO_API_ITEM_PROPERTY(access, type, name, ...) \
    FO_API_ITEM_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
    AS_VERIFY(engine->RegisterObjectMethod("Item", MakeMethodDecl("set_" #name, "void", type).c_str(), \
        SCRIPT_METHOD(ScriptItem, Set_##name), SCRIPT_METHOD_CONV));
#define FO_API_CRITTER_READONLY_PROPERTY(access, type, name, ...) \
    AS_VERIFY(engine->RegisterObjectMethod("Critter", MakeMethodDecl("get_" #name, type, "").c_str(), \
        SCRIPT_METHOD(ScriptCritter, Get_##name), SCRIPT_METHOD_CONV));
#define FO_API_CRITTER_PROPERTY(access, type, name, ...) \
    FO_API_CRITTER_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
    AS_VERIFY(engine->RegisterObjectMethod("Critter", MakeMethodDecl("set_" #name, "void", type).c_str(), \
        SCRIPT_METHOD(ScriptCritter, Set_##name), SCRIPT_METHOD_CONV));
#define FO_API_MAP_READONLY_PROPERTY(access, type, name, ...) \
    AS_VERIFY(engine->RegisterObjectMethod("Map", MakeMethodDecl("get_" #name, type, "").c_str(), \
        SCRIPT_METHOD(ScriptMap, Get_##name), SCRIPT_METHOD_CONV));
#define FO_API_MAP_PROPERTY(access, type, name, ...) \
    FO_API_MAP_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
    AS_VERIFY(engine->RegisterObjectMethod("Map", MakeMethodDecl("set_" #name, "void", type).c_str(), \
        SCRIPT_METHOD(ScriptMap, Set_##name), SCRIPT_METHOD_CONV));
#define FO_API_LOCATION_READONLY_PROPERTY(access, type, name, ...) \
    AS_VERIFY(engine->RegisterObjectMethod("Location", MakeMethodDecl("get_" #name, type, "").c_str(), \
        SCRIPT_METHOD(ScriptLocation, Get_##name), SCRIPT_METHOD_CONV));
#define FO_API_LOCATION_PROPERTY(access, type, name, ...) \
    FO_API_LOCATION_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
    AS_VERIFY(engine->RegisterObjectMethod("Location", MakeMethodDecl("set_" #name, "void", type).c_str(), \
        SCRIPT_METHOD(ScriptLocation, Set_##name), SCRIPT_METHOD_CONV));
#include "ScriptApi.h"
}

#else
struct ScriptSystem::AngelScriptImpl
{
};
void SCRIPTING_CLASS::InitAngelScriptScripting()
{
}
#endif
