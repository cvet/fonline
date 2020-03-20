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

#define AS_ASSERT(expr) \
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

template<typename T>
inline T* MarshalObj(ScriptEntity* obj)
{
    return 0;
}

template<typename T>
inline vector<T> MarshalArray(CScriptArray* arr)
{
    return {};
}

template<typename T>
inline vector<T*> MarshalObjArray(CScriptArray* arr)
{
    return {};
}

inline std::function<void()> MarshalCallback(asIScriptFunction* func)
{
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

inline string MarshalBack(string str)
{
    return str;
}

inline ScriptEntity* MarshalBack(Entity* obj)
{
    return 0;
}

template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
inline T MarshalBack(T value)
{
    return value;
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
#define FO_API_ARG_ARR_MARSHAL(type, name) vector<type> name = MarshalArray<type>(_##name);
#define FO_API_ARG_OBJ_MARSHAL(type, name) type* name = MarshalObj<type>(_##name);
#define FO_API_ARG_OBJ_ARR_MARSHAL(type, name) vector<type*> name = MarshalObjArray<type>(_##name);
#define FO_API_ARG_REF_MARSHAL(type, name)
#define FO_API_ARG_ARR_REF_MARSHAL(type, name) vector<type> name = MarshalArray<type>(_##name);
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
#elif defined(FO_CLIENT_SCRIPTING)
#define THIS_ARG ItemView* _this = (ItemView*)GameEntity
#define FO_API_ITEM_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_ITEM_VIEW_METHOD_IMPL
#endif
#include "ScriptApi.h"
#undef THIS_ARG
};

struct ScriptCritter : ScriptEntity
{
#if defined(FO_SERVER_SCRIPTING)
#define THIS_ARG Critter* _this = (Critter*)GameEntity
#define FO_API_CRITTER_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_CRITTER_METHOD_IMPL
#elif defined(FO_CLIENT_SCRIPTING)
#define THIS_ARG CritterView* _this = (CritterView*)GameEntity
#define FO_API_CRITTER_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_CRITTER_VIEW_METHOD_IMPL
#endif
#include "ScriptApi.h"
#undef THIS_ARG
};

struct ScriptMap : ScriptEntity
{
#if defined(FO_SERVER_SCRIPTING)
#define THIS_ARG Map* _this = (Map*)GameEntity
#define FO_API_MAP_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_MAP_METHOD_IMPL
#elif defined(FO_CLIENT_SCRIPTING)
#define THIS_ARG MapView* _this = (MapView*)GameEntity
#define FO_API_MAP_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_MAP_VIEW_METHOD_IMPL
#endif
#include "ScriptApi.h"
#undef THIS_ARG
};

struct ScriptLocation : ScriptEntity
{
#if defined(FO_SERVER_SCRIPTING)
#define THIS_ARG Location* _this = (Location*)GameEntity
#define FO_API_LOCATION_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_LOCATION_METHOD_IMPL
#elif defined(FO_CLIENT_SCRIPTING)
#define THIS_ARG LocationView* _this = (LocationView*)GameEntity
#define FO_API_LOCATION_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_LOCATION_VIEW_METHOD_IMPL
#endif
#include "ScriptApi.h"
#undef THIS_ARG
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

//#undef FO_API_PARTLY_UNDEF
//#include "ScriptApi.h"

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
    AS_ASSERT(engine->SetMessageCallback(asFUNCTION(CallbackMessage), nullptr, asCALL_CDECL));

    AS_ASSERT(engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, true));
    AS_ASSERT(engine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, true));
    AS_ASSERT(engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, true));
    AS_ASSERT(engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true));
    AS_ASSERT(engine->SetEngineProperty(asEP_DISALLOW_EMPTY_LIST_ELEMENTS, true));
    AS_ASSERT(engine->SetEngineProperty(asEP_PRIVATE_PROP_AS_PROTECTED, true));
    AS_ASSERT(engine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE, true));
    AS_ASSERT(engine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, true));
    AS_ASSERT(engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true));

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

    AS_ASSERT(engine->RegisterTypedef("hash", "uint"));
    AS_ASSERT(engine->RegisterTypedef("resource", "uint"));

#define REGISTER_ENTITY(class_name, real_class) \
    AS_ASSERT(engine->RegisterObjectType(class_name, 0, asOBJ_REF)); \
    AS_ASSERT(engine->RegisterObjectBehaviour( \
        class_name, asBEHAVE_ADDREF, "void f()", SCRIPT_METHOD(real_class, AddRef), SCRIPT_METHOD_CONV)); \
    AS_ASSERT(engine->RegisterObjectBehaviour( \
        class_name, asBEHAVE_RELEASE, "void f()", SCRIPT_METHOD(real_class, Release), SCRIPT_METHOD_CONV)); \
    AS_ASSERT(engine->RegisterObjectProperty(class_name, "const uint Id", OFFSETOF(real_class, Id))); \
    AS_ASSERT(engine->RegisterObjectMethod( \
        class_name, "hash get_ProtoId() const", SCRIPT_METHOD(real_class, GetProtoId), SCRIPT_METHOD_CONV)); \
    AS_ASSERT( \
        engine->RegisterObjectProperty(class_name, "const bool IsDestroyed", OFFSETOF(real_class, IsDestroyed))); \
    AS_ASSERT( \
        engine->RegisterObjectProperty(class_name, "const bool IsDestroying", OFFSETOF(real_class, IsDestroying))); \
    AS_ASSERT(engine->RegisterObjectProperty(class_name, "const int RefCounter", OFFSETOF(real_class, RefCounter)));
}

#else
struct ScriptSystem::AngelScriptImpl
{
};
void SCRIPTING_CLASS::InitAngelScriptScripting()
{
}
#endif
