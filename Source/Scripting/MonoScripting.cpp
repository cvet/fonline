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

#if FO_SERVER_SCRIPTING || FO_SINGLEPLAYER_SCRIPTING
#include "ServerScripting.h"
#elif FO_CLIENT_SCRIPTING
#include "ClientScripting.h"
#elif FO_MAPPER_SCRIPTING
#include "MapperScripting.h"
#endif

#if FO_SERVER_SCRIPTING || FO_SINGLEPLAYER_SCRIPTING
#include "Server.h"
#define FO_API_COMMON_IMPL 1
#define FO_API_SERVER_IMPL 1
#include "ScriptApi.h"
#define SCRIPTING_CLASS ServerScriptSystem
#define IS_SERVER true
#define IS_CLIENT false
#define IS_MAPPER false
#elif FO_CLIENT_SCRIPTING
#include "Client.h"
#define FO_API_COMMON_IMPL 1
#define FO_API_CLIENT_IMPL 1
#include "ScriptApi.h"
#define SCRIPTING_CLASS ClientScriptSystem
#define IS_SERVER false
#define IS_CLIENT true
#define IS_MAPPER false
#elif FO_MAPPER_SCRIPTING
#include "Mapper.h"
#define FO_API_COMMON_IMPL 1
#define FO_API_MAPPER_IMPL 1
#include "ScriptApi.h"
#define SCRIPTING_CLASS MapperScriptSystem
#define IS_SERVER false
#define IS_CLIENT false
#define IS_MAPPER true
#endif

#if FO_MONO_SCRIPTING
#include "Log.h"

#include <mono/dis/meta.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/exception.h>
#include <mono/metadata/mono-config.h>
#include <mono/mini/jit.h>

#define FO_API_ENUM_ENTRY(group, name, value) static int group##_##name = value;
#include "ScriptApi.h"

template<typename T, std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>, int> = 0>
inline T Marshal(T value)
{
    return value;
}

template<typename T, std::enable_if_t<!std::is_same_v<T, string>, int> = 0>
inline T Marshal(MonoArray* arr)
{
    return {};
}

template<typename T, std::enable_if_t<std::is_same_v<T, string>, int> = 0>
inline T Marshal(MonoString* str)
{
    return "";
}

template<typename T>
inline T* MarshalObj(MonoObject* value)
{
    return 0;
}

template<typename T>
inline vector<T*> MarshalObjArr(MonoArray* arr)
{
    return {};
}

template<typename T>
inline std::function<void(T*)> MarshalCallback(MonoObject* func)
{
    return {};
}

template<typename T>
inline std::function<bool(T*)> MarshalPredicate(MonoObject* func)
{
    return {};
}

template<typename TKey, typename TValue>
inline map<TKey, TValue> MarshalDict(MonoObject* dict)
{
    return {};
}

template<typename T>
inline MonoArray* MarshalBack(vector<T> arr)
{
    return 0;
}

inline MonoObject* MarshalBack(Entity* obj)
{
    return 0;
}

inline MonoString* MarshalBack(string value)
{
    return mono_string_new(mono_domain_get(), value.c_str());
}

template<typename T, std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>, int> = 0>
inline T MarshalBack(T value)
{
    return value;
}

#define Mono_bool MonoBoolean
#define Mono_char signed char
#define Mono_short gint16
#define Mono_int gint32
#define Mono_int64 gint64
#define Mono_uchar guchar
#define Mono_ushort guint16
#define Mono_uint guint32
#define Mono_uint64 guint64
#define Mono_float float
#define Mono_double double
#define Mono_string MonoString*
#define Mono_hash hash
#define Mono_void void
#define FO_API_PARTLY_UNDEF 1
#define FO_API_ARG(type, name) Mono_##type _##name
#define FO_API_ARG_ARR(type, name) MonoArray* _##name
#define FO_API_ARG_OBJ(type, name) MonoObject* _##name
#define FO_API_ARG_OBJ_ARR(type, name) MonoArray* _##name
#define FO_API_ARG_REF(type, name) type* _##name
#define FO_API_ARG_ARR_REF(type, name) MonoArray* _##name
#define FO_API_ARG_ENUM(type, name) int name
#define FO_API_ARG_CALLBACK(type, name) MonoObject* _##name
#define FO_API_ARG_PREDICATE(type, name) MonoObject* _##name
#define FO_API_ARG_DICT(key, val, name) MonoObject* _##name
#define FO_API_ARG_MARSHAL(type, name) type name = Marshal<type>(_##name);
#define FO_API_ARG_ARR_MARSHAL(type, name) vector<type> name = Marshal<vector<type>>(_##name);
#define FO_API_ARG_OBJ_MARSHAL(type, name) type* name = MarshalObj<type>(_##name);
#define FO_API_ARG_OBJ_ARR_MARSHAL(type, name) vector<type*> name = MarshalObjArr<type>(_##name);
#define FO_API_ARG_REF_MARSHAL(type, name) type name = *_##name;
#define FO_API_ARG_ARR_REF_MARSHAL(type, name) vector<type> name = Marshal<vector<type>>(_##name);
#define FO_API_ARG_ENUM_MARSHAL(type, name)
#define FO_API_ARG_CALLBACK_MARSHAL(type, name) std::function<void(type*)> name = MarshalCallback<type>(_##name);
#define FO_API_ARG_PREDICATE_MARSHAL(type, name) std::function<bool(type*)> name = MarshalPredicate<type>(_##name);
#define FO_API_ARG_DICT_MARSHAL(key, val, name) map<key, val> name = MarshalDict<key, val>(_##name);
#define FO_API_RET(type) Mono_##type
#define FO_API_RET_ARR(type) MonoArray*
#define FO_API_RET_OBJ(type) MonoObject*
#define FO_API_RET_OBJ_ARR(type) MonoArray*
#define FO_API_RETURN(expr) return MarshalBack(expr)
#define FO_API_RETURN_VOID() return
#define FO_API_PROPERTY_TYPE(type) Mono_##type
#define FO_API_PROPERTY_TYPE_ARR(type) MonoArray*
#define FO_API_PROPERTY_TYPE_OBJ(type) MonoObject*
#define FO_API_PROPERTY_TYPE_OBJ_ARR(type) MonoArray*
#define FO_API_PROPERTY_TYPE_ENUM(type) int
#define FO_API_PROPERTY_MOD(mod)

static MonoException* ReportException(MonoDomain* domain, const std::exception& ex)
{
    return mono_get_exception_invalid_operation(ex.what());
}

static void SetDomainUserData(MonoDomain* domain, void* user_data)
{
    // Todo: set Mono domain user data
}

inline void* GetDomainUserData(MonoDomain* domain)
{
    return nullptr; // Todo: get Mono domain user data
}

#if FO_SERVER_SCRIPTING || FO_SINGLEPLAYER_SCRIPTING
#define CONTEXT_ARG \
    FOServer* _server = (FOServer*)GetDomainUserData(_domain); \
    FOServer* _common = _server
#elif FO_CLIENT_SCRIPTING
#define CONTEXT_ARG \
    FOClient* _client = (FOClient*)GetDomainUserData(_domain); \
    FOClient* _common = _client
#elif FO_MAPPER_SCRIPTING
#define CONTEXT_ARG \
    FOMapper* _mapper = (FOMapper*)GetDomainUserData(_domain); \
    FOMapper* _common = _mapper
#endif

#define FO_API_PROLOG(...) \
    { \
        *_ex = nullptr; \
        try { \
            CONTEXT_ARG; \
            THIS_ARG; \
            __VA_ARGS__
#define FO_API_EPILOG(...) \
    } \
    catch (std::exception & ex) \
    { \
        *_ex = ReportException(_domain, ex); \
        return __VA_ARGS__; \
    } \
    }

#if FO_SERVER_SCRIPTING || FO_SINGLEPLAYER_SCRIPTING
#define THIS_ARG Item* _item = (Item*)_thisPtr
#define FO_API_ITEM_METHOD(name, ret, ...) static ret MonoItem_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr, ##__VA_ARGS__)
#define FO_API_ITEM_METHOD_IMPL 1
#define ITEM_CLASS Item
#elif FO_CLIENT_SCRIPTING
#define THIS_ARG ItemView* _itemView = (ItemView*)_thisPtr
#define FO_API_ITEM_VIEW_METHOD(name, ret, ...) static ret MonoItem_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr, ##__VA_ARGS__)
#define FO_API_ITEM_VIEW_METHOD_IMPL 1
#define ITEM_CLASS ItemView
#elif FO_MAPPER_SCRIPTING
#define ITEM_CLASS ItemView
#endif
#define FO_API_ITEM_READONLY_PROPERTY(access, type, name, ...) \
    static type MonoItem_Get_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr) \
    { \
        *_ex = nullptr; \
        try { \
            return MarshalBack(((ITEM_CLASS*)_thisPtr)->Get##name()); \
        } \
        catch (std::exception & ex) { \
            *_ex = ReportException(_domain, ex); \
            return 0; \
        } \
    }
#define FO_API_ITEM_PROPERTY(access, type, name, ...) \
    FO_API_ITEM_READONLY_PROPERTY(access, type, name, __VA_ARGS__); \
    static void MonoItem_Set_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr, type value) \
    { \
        *_ex = nullptr; \
        try { \
            ((ITEM_CLASS*)_thisPtr)->Set##name(Marshal<decltype(((ITEM_CLASS*)_thisPtr)->Get##name())>(value)); \
        } \
        catch (std::exception & ex) { \
            *_ex = ReportException(_domain, ex); \
        } \
    }
#include "ScriptApi.h"
#undef THIS_ARG
#undef ITEM_CLASS

#if FO_SERVER_SCRIPTING || FO_SINGLEPLAYER_SCRIPTING
#define THIS_ARG Critter* _critter = (Critter*)_thisPtr
#define FO_API_CRITTER_METHOD(name, ret, ...) static ret MonoCritter_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr, ##__VA_ARGS__)
#define FO_API_CRITTER_METHOD_IMPL 1
#define CRITTER_CLASS Critter
#elif FO_CLIENT_SCRIPTING
#define THIS_ARG CritterView* _critterView = (CritterView*)_thisPtr
#define FO_API_CRITTER_VIEW_METHOD(name, ret, ...) static ret MonoCritter_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr, ##__VA_ARGS__)
#define FO_API_CRITTER_VIEW_METHOD_IMPL 1
#define CRITTER_CLASS CritterView
#elif FO_MAPPER_SCRIPTING
#define CRITTER_CLASS CritterView
#endif
#define FO_API_CRITTER_READONLY_PROPERTY(access, type, name, ...) \
    static type MonoCritter_Get_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr) \
    { \
        *_ex = nullptr; \
        try { \
            return MarshalBack(((CRITTER_CLASS*)_thisPtr)->Get##name()); \
        } \
        catch (std::exception & ex) { \
            *_ex = ReportException(_domain, ex); \
            return 0; \
        } \
    }
#define FO_API_CRITTER_PROPERTY(access, type, name, ...) \
    FO_API_CRITTER_READONLY_PROPERTY(access, type, name, __VA_ARGS__); \
    static void MonoCritter_Set_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr, type value) \
    { \
        *_ex = nullptr; \
        try { \
            ((CRITTER_CLASS*)_thisPtr)->Set##name(Marshal<decltype(((CRITTER_CLASS*)_thisPtr)->Get##name())>(value)); \
        } \
        catch (std::exception & ex) { \
            *_ex = ReportException(_domain, ex); \
        } \
    }
#include "ScriptApi.h"
#undef THIS_ARG
#undef CRITTER_CLASS

#if FO_SERVER_SCRIPTING || FO_SINGLEPLAYER_SCRIPTING
#define THIS_ARG Map* _map = (Map*)_thisPtr
#define FO_API_MAP_METHOD(name, ret, ...) static ret MonoMap_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr, ##__VA_ARGS__)
#define FO_API_MAP_METHOD_IMPL 1
#define MAP_CLASS Map
#elif FO_CLIENT_SCRIPTING
#define THIS_ARG MapView* _mapView = (MapView*)_thisPtr
#define FO_API_MAP_VIEW_METHOD(name, ret, ...) static ret MonoMap_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr, ##__VA_ARGS__)
#define FO_API_MAP_VIEW_METHOD_IMPL 1
#define MAP_CLASS MapView
#elif FO_MAPPER_SCRIPTING
#define MAP_CLASS MapView
#endif
#define FO_API_MAP_READONLY_PROPERTY(access, type, name, ...) \
    static type MonoMap_Get_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr) \
    { \
        *_ex = nullptr; \
        try { \
            return MarshalBack(((MAP_CLASS*)_thisPtr)->Get##name()); \
        } \
        catch (std::exception & ex) { \
            *_ex = ReportException(_domain, ex); \
            return 0; \
        } \
    }
#define FO_API_MAP_PROPERTY(access, type, name, ...) \
    FO_API_MAP_READONLY_PROPERTY(access, type, name, __VA_ARGS__); \
    static void MonoMap_Set_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr, type value) \
    { \
        *_ex = nullptr; \
        try { \
            ((MAP_CLASS*)_thisPtr)->Set##name(Marshal<decltype(((MAP_CLASS*)_thisPtr)->Get##name())>(value)); \
        } \
        catch (std::exception & ex) { \
            *_ex = ReportException(_domain, ex); \
        } \
    }
#include "ScriptApi.h"
#undef THIS_ARG
#undef MAP_CLASS

#if FO_SERVER_SCRIPTING || FO_SINGLEPLAYER_SCRIPTING
#define THIS_ARG Location* _location = (Location*)_thisPtr
#define FO_API_LOCATION_METHOD(name, ret, ...) static ret MonoLocation_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr, ##__VA_ARGS__)
#define FO_API_LOCATION_METHOD_IMPL 1
#define LOCATION_CLASS Location
#elif FO_CLIENT_SCRIPTING
#define THIS_ARG LocationView* _locationView = (LocationView*)_thisPtr
#define FO_API_LOCATION_VIEW_METHOD(name, ret, ...) static ret MonoLocation_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr, ##__VA_ARGS__)
#define FO_API_LOCATION_VIEW_METHOD_IMPL 1
#define LOCATION_CLASS LocationView
#elif FO_MAPPER_SCRIPTING
#define LOCATION_CLASS LocationView
#endif
#define FO_API_LOCATION_READONLY_PROPERTY(access, type, name, ...) \
    static type MonoLocation_Get_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr) \
    { \
        *_ex = nullptr; \
        try { \
            return MarshalBack(((LOCATION_CLASS*)_thisPtr)->Get##name()); \
        } \
        catch (std::exception & ex) { \
            *_ex = ReportException(_domain, ex); \
            return 0; \
        } \
    }
#define FO_API_LOCATION_PROPERTY(access, type, name, ...) \
    FO_API_LOCATION_READONLY_PROPERTY(access, type, name, __VA_ARGS__); \
    static void MonoLocation_Set_##name(MonoDomain* _domain, MonoException** _ex, void* _thisPtr, type value) \
    { \
        *_ex = nullptr; \
        try { \
            ((LOCATION_CLASS*)_thisPtr)->Set##name(Marshal<decltype(((LOCATION_CLASS*)_thisPtr)->Get##name())>(value)); \
        } \
        catch (std::exception & ex) { \
            *_ex = ReportException(_domain, ex); \
        } \
    }
#include "ScriptApi.h"
#undef THIS_ARG
#undef LOCATION_CLASS

#define THIS_ARG (void)0
#define FO_API_GLOBAL_COMMON_FUNC(name, ret, ...) static ret MonoGlobal_##name(MonoDomain* _domain, MonoException** _ex, ##__VA_ARGS__)
#define FO_API_GLOBAL_COMMON_FUNC_IMPL 1
#if FO_SERVER_SCRIPTING || FO_SINGLEPLAYER_SCRIPTING
#define FO_API_GLOBAL_SERVER_FUNC(name, ret, ...) static ret MonoGlobal_##name(MonoDomain* _domain, MonoException** _ex, ##__VA_ARGS__)
#define FO_API_GLOBAL_SERVER_FUNC_IMPL 1
#elif FO_CLIENT_SCRIPTING
#define FO_API_GLOBAL_CLIENT_FUNC(name, ret, ...) static ret MonoGlobal_##name(MonoDomain* _domain, MonoException** _ex, ##__VA_ARGS__)
#define FO_API_GLOBAL_CLIENT_FUNC_IMPL 1
#elif FO_MAPPER_SCRIPTING
#define FO_API_GLOBAL_MAPPER_FUNC(name, ret, ...) static ret MonoGlobal_##name(MonoDomain* _domain, MonoException** _ex, ##__VA_ARGS__)
#define FO_API_GLOBAL_MAPPER_FUNC_IMPL 1
#endif
#include "ScriptApi.h"
#undef THIS_ARG

#undef FO_API_PARTLY_UNDEF
#include "ScriptApi.h"

// static void SetMonoInternalCalls();
// static MonoAssembly* LoadNetAssembly(const string& name);
// static MonoAssembly* LoadGameAssembly(const string& name, map<string, MonoImage*>& assembly_images);
// static bool CompileGameAssemblies(const string& target, map<string, MonoImage*>& assembly_images);

struct ScriptSystem::MonoImpl
{
    map<string, MonoImage*> EngineAssemblyImages {};
};

void SCRIPTING_CLASS::InitMonoScripting()
{
    _pMonoImpl = std::make_unique<MonoImpl>();

    g_set_print_handler([](const gchar* str) { WriteLog("{}", str); });
    g_set_printerr_handler([](const gchar* str) { WriteLog("{}", str); });

    mono_config_parse_memory(R"(
    <configuration>
        <dllmap dll="i:cygwin1.dll" target="libc.so.6" os="!windows" />
        <dllmap dll="libc" target="libc.so.6" os="!windows"/>
        <dllmap dll="intl" target="libc.so.6" os="!windows"/>
        <dllmap dll="intl" name="bind_textdomain_codeset" target="libc.so.6" os="solaris"/>
        <dllmap dll="libintl" name="bind_textdomain_codeset" target="libc.so.6" os="solaris"/>
        <dllmap dll="libintl" target="libc.so.6" os="!windows"/>
        <dllmap dll="i:libxslt.dll" target="libxslt.so" os="!windows"/>
        <dllmap dll="i:odbc32.dll" target="libodbc.so" os="!windows"/>
        <dllmap dll="i:odbc32.dll" target="libiodbc.dylib" os="osx"/>
        <dllmap dll="oci" target="libclntsh.so" os="!windows"/>
        <dllmap dll="db2cli" target="libdb2_36.so" os="!windows"/>
        <dllmap dll="MonoPosixHelper" target="$mono_libdir/libMonoPosixHelper.so" os="!windows" />
        <dllmap dll="System.Native" target="$mono_libdir/libmono-system-native.so" os="!windows" />
        <dllmap dll="libmono-btls-shared" target="$mono_libdir/libmono-btls-shared.so" os="!windows" />
        <dllmap dll="i:msvcrt" target="libc.so.6" os="!windows"/>
        <dllmap dll="i:msvcrt.dll" target="libc.so.6" os="!windows"/>
        <dllmap dll="sqlite" target="libsqlite.so.0" os="!windows"/>
        <dllmap dll="sqlite3" target="libsqlite3.so.0" os="!windows"/>
        <dllmap dll="libX11" target="libX11.so" os="!windows" />
        <dllmap dll="libgdk-x11-2.0" target="libgdk-x11-2.0.so.0" os="!windows"/>
        <dllmap dll="libgdk_pixbuf-2.0" target="libgdk_pixbuf-2.0.so.0" os="!windows"/>
        <dllmap dll="libgtk-x11-2.0" target="libgtk-x11-2.0.so.0" os="!windows"/>
        <dllmap dll="libglib-2.0" target="libglib-2.0.so.0" os="!windows"/>
        <dllmap dll="libgobject-2.0" target="libgobject-2.0.so.0" os="!windows"/>
        <dllmap dll="libgnomeui-2" target="libgnomeui-2.so.0" os="!windows"/>
        <dllmap dll="librsvg-2" target="librsvg-2.so.2" os="!windows"/>
        <dllmap dll="libXinerama" target="libXinerama.so.1" os="!windows" />
        <dllmap dll="libasound" target="libasound.so.2" os="!windows" />
        <dllmap dll="libcairo-2.dll" target="libcairo.so.2" os="!windows"/>
        <dllmap dll="libcairo-2.dll" target="libcairo.2.dylib" os="osx"/>
        <dllmap dll="libcups" target="libcups.so.2" os="!windows"/>
        <dllmap dll="libcups" target="libcups.dylib" os="osx"/>
        <dllmap dll="i:kernel32.dll">
            <dllentry dll="__Internal" name="CopyMemory" target="mono_win32_compat_CopyMemory"/>
            <dllentry dll="__Internal" name="FillMemory" target="mono_win32_compat_FillMemory"/>
            <dllentry dll="__Internal" name="MoveMemory" target="mono_win32_compat_MoveMemory"/>
            <dllentry dll="__Internal" name="ZeroMemory" target="mono_win32_compat_ZeroMemory"/>
        </dllmap>
        <dllmap dll="gdiplus" target="libgdiplus.so" os="!windows"/>
        <dllmap dll="gdiplus.dll" target="libgdiplus.so"  os="!windows"/>
        <dllmap dll="gdi32" target="libgdiplus.so" os="!windows"/>
        <dllmap dll="gdi32.dll" target="libgdiplus.so" os="!windows"/>
    </configuration>)");

    mono_set_dirs("./dummy/lib", "./dummy/etc");
    mono_set_assemblies_path("./dummy/lib/gac");

    // mono_dllmap_insert
    // mono_install_load_aot_data_hook()
    /*RUNTIME_ASSERT(pMonoImpl->EngineAssemblyImages.empty());
    mono_install_assembly_preload_hook(
        [](MonoAssemblyName* aname, char** assemblies_path, void* user_data) -> MonoAssembly* {
            auto assembly_images = (map<string, MonoImage*>*)user_data;
            if (assembly_images->count(aname->name))
                return LoadGameAssembly(aname->name, *assembly_images);
            return LoadNetAssembly(aname->name);
        },
        (void*)&EngineAssemblyImages);*/

    static std::atomic_int domain_counter;
    int domain_num = domain_counter++;
    MonoDomain* domain = mono_jit_init_version(fmt::format("FOnlineDomain_{}", domain_num).c_str(), "v4.0.30319");
    RUNTIME_ASSERT(domain);

    SetDomainUserData(domain, _mainObj);

#if FO_SERVER_SCRIPTING || FO_SINGLEPLAYER_SCRIPTING
#define FO_API_ITEM_METHOD(name, ret, ...) mono_add_internal_call("Item::_" #name, (void*)&MonoItem_##name);
#define FO_API_CRITTER_METHOD(name, ret, ...) mono_add_internal_call("Critter::_" #name, (void*)&MonoCritter_##name);
#define FO_API_MAP_METHOD(name, ret, ...) mono_add_internal_call("Map::_" #name, (void*)&MonoMap_##name);
#define FO_API_LOCATION_METHOD(name, ret, ...) mono_add_internal_call("Location::_" #name, (void*)&MonoLocation_##name);
#elif FO_CLIENT_SCRIPTING
#define FO_API_ITEM_VIEW_METHOD(name, ret, ...) mono_add_internal_call("Item::_" #name, (void*)&MonoItem_##name);
#define FO_API_CRITTER_VIEW_METHOD(name, ret, ...) mono_add_internal_call("Critter::_" #name, (void*)&MonoCritter_##name);
#define FO_API_MAP_VIEW_METHOD(name, ret, ...) mono_add_internal_call("Map::_" #name, (void*)&MonoMap_##name);
#define FO_API_LOCATION_VIEW_METHOD(name, ret, ...) mono_add_internal_call("Location::_" #name, (void*)&MonoLocation_##name);
#endif
#include "ScriptApi.h"

#define CHECK_GETTER(access) (IS_SERVER && !(Property::AccessType::access & Property::AccessType::ClientOnlyMask)) || (IS_CLIENT && !(Property::AccessType::access & Property::AccessType::ServerOnlyMask)) || (IS_MAPPER && !(Property::AccessType::access & Property::AccessType::VirtualMask))
#define CHECK_SETTER(access) (IS_SERVER && !(Property::AccessType::access & Property::AccessType::ClientOnlyMask)) || (IS_CLIENT && !(Property::AccessType::access & Property::AccessType::ServerOnlyMask) && ((Property::AccessType::access & Property::AccessType::ClientOnlyMask) || (Property::AccessType::access & Property::AccessType::ModifiableMask))) || (IS_MAPPER && !(Property::AccessType::access & Property::AccessType::VirtualMask))

#define FO_API_ITEM_READONLY_PROPERTY(access, type, name, ...) \
    if (CHECK_GETTER(access)) \
        mono_add_internal_call("Item::_Get_" #name, (void*)&MonoItem_Get_##name);
#define FO_API_ITEM_PROPERTY(access, type, name, ...) \
    FO_API_ITEM_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
    if (CHECK_SETTER(access)) \
        mono_add_internal_call("Item::_Set_" #name, (void*)&MonoItem_Set_##name);
#define FO_API_CRITTER_READONLY_PROPERTY(access, type, name, ...) \
    if (CHECK_GETTER(access)) \
        mono_add_internal_call("Critter::_Get_" #name, (void*)&MonoCritter_Get_##name);
#define FO_API_CRITTER_PROPERTY(access, type, name, ...) \
    FO_API_CRITTER_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
    if (CHECK_SETTER(access)) \
        mono_add_internal_call("Critter::_Set_" #name, (void*)&MonoCritter_Set_##name);
#define FO_API_MAP_READONLY_PROPERTY(access, type, name, ...) \
    if (CHECK_GETTER(access)) \
        mono_add_internal_call("Map::_Get_" #name, (void*)&MonoMap_Get_##name);
#define FO_API_MAP_PROPERTY(access, type, name, ...) \
    FO_API_MAP_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
    if (CHECK_SETTER(access)) \
        mono_add_internal_call("Map::_Set_" #name, (void*)&MonoMap_Set_##name);
#define FO_API_LOCATION_READONLY_PROPERTY(access, type, name, ...) \
    if (CHECK_GETTER(access)) \
        mono_add_internal_call("Location::_Get_" #name, (void*)&MonoLocation_Get_##name);
#define FO_API_LOCATION_PROPERTY(access, type, name, ...) \
    FO_API_LOCATION_READONLY_PROPERTY(access, type, name, __VA_ARGS__) \
    if (CHECK_SETTER(access)) \
        mono_add_internal_call("Location::_Set_" #name, (void*)&MonoLocation_Set_##name);
#include "ScriptApi.h"

    /*if (assemblies_data)
    {
        for (auto& kv : *assemblies_data)
        {
            MonoImageOpenStatus status = MONO_IMAGE_OK;
            MonoImage* image = mono_image_open_from_data((char*)&kv.second[0], (uint)kv.second.size(), TRUE, &status);
            RUNTIME_ASSERT(status == MONO_IMAGE_OK && image);

            EngineAssemblyImages[kv.first] = image;
        }
    }
    else
    {
        bool ok = CompileGameAssemblies(target, EngineAssemblyImages);
        RUNTIME_ASSERT(ok);
    }

    for (auto& kv : EngineAssemblyImages)
    {
        bool ok = LoadGameAssembly(kv.first, EngineAssemblyImages);
        RUNTIME_ASSERT(ok);
    }

    MonoClass* global_events_class =
        mono_class_from_name(EngineAssemblyImages["FOnline.Core.dll"], "FOnlineEngine", "GlobalEvents");
    RUNTIME_ASSERT(global_events_class);
    MonoMethodDesc* init_method_desc = mono_method_desc_new(":Initialize()", FALSE);
    RUNTIME_ASSERT(init_method_desc);
    MonoMethod* init_method = mono_method_desc_search_in_class(init_method_desc, global_events_class);
    RUNTIME_ASSERT(init_method);
    mono_method_desc_free(init_method_desc);

    MonoObject* exc;
    MonoObject* init_method_result = mono_runtime_invoke(init_method, NULL, NULL, &exc);
    if (exc)
        mono_print_unhandled_exception(exc);

    RUNTIME_ASSERT(exc || *(bool*)mono_object_unbox(init_method_result) == false)
    return false;*/
}

/*static MonoAssembly* LoadNetAssembly(const string& name)
{
    string assemblies_path = "Assemblies/" + name + (_str(name).endsWith(".dll") ? "" : ".dll");
#if FONLINE_SERVER
    assemblies_path = "Resources/Mono/" + assemblies_path;
#endif

    File file;
    file.LoadFile(assemblies_path);
    RUNTIME_ASSERT(file.IsLoaded());

    MonoImageOpenStatus status = MONO_IMAGE_OK;
    MonoImage* image = mono_image_open_from_data((char*)file.GetBuf(), file.GetFsize(), TRUE, &status);
    RUNTIME_ASSERT(status == MONO_IMAGE_OK && image);

    MonoAssembly* assembly = mono_assembly_load_from(image, name.c_str(), &status);
    RUNTIME_ASSERT(status == MONO_IMAGE_OK && assembly);

    return assembly;
}

static MonoAssembly* LoadGameAssembly(const string& name, map<string, MonoImage*>& assembly_images)
{
    RUNTIME_ASSERT(assembly_images.count(name));
    MonoImage* image = assembly_images[name];

    MonoImageOpenStatus status = MONO_IMAGE_OK;
    MonoAssembly* assembly = mono_assembly_load_from(image, name.c_str(), &status);
    RUNTIME_ASSERT(status == MONO_IMAGE_OK && assembly);

    return assembly;
}

static bool CompileGameAssemblies(const string& target, map<string, MonoImage*>& assembly_images)
{
    string mono_path = MainConfig->GetStr("", "MonoPath");
    string xbuild_path = _str(mono_path + "/bin/xbuild.bat").resolvePath();

    FileCollection proj_files("csproj");
    while (proj_files.IsNextFile())
    {
        string name, path;
        File& file = proj_files.GetNextFile(&name, &path);
        RUNTIME_ASSERT(file.IsLoaded());

        // Compile
        string command =
            _str("{} /property:Configuration={} /nologo /verbosity:quiet \"{}\"", xbuild_path, target, path);
        string output;
        int call_result = SystemCall(command, output);
        if (call_result)
        {
            StrVec errors = _str(output).split('\n');
            WriteLog("Compilation failed! Error{}:", errors.size() > 1 ? "s" : "");
            for (string& error : errors)
                WriteLog("{}\n", error);
            return false;
        }

        // Get output path
        string file_content = (char*)file.GetBuf();
        size_t pos = file_content.find("'$(Configuration)|$(Platform)' == '" + target + "|");
        RUNTIME_ASSERT(pos != string::npos);
        pos = file_content.find("<OutputPath>", pos);
        RUNTIME_ASSERT(pos != string::npos);
        size_t epos = file_content.find("</OutputPath>", pos);
        RUNTIME_ASSERT(epos != string::npos);
        pos += _str("<OutputPath>").length();

        string assembly_name = name + ".dll";
        string assembly_path =
            _str("{}/{}/{}", _str(path).extractDir(), file_content.substr(pos, epos - pos), assembly_name)
                .resolvePath();

        File assembly_file;
        assembly_file.LoadFile(assembly_path);
        RUNTIME_ASSERT(assembly_file.IsLoaded());

        MonoImageOpenStatus status = MONO_IMAGE_OK;
        MonoImage* image =
            mono_image_open_from_data((char*)assembly_file.GetBuf(), assembly_file.GetFsize(), TRUE, &status);
        RUNTIME_ASSERT(status == MONO_IMAGE_OK && image);

        assembly_images[assembly_name] = image;
    }

    return true;
}*/

#else
struct ScriptSystem::MonoImpl
{
};
void SCRIPTING_CLASS::InitMonoScripting()
{
}
#endif
