//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

///@ CodeGen Template Native

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#if FO_SERVER_SCRIPTING
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

#if FO_NATIVE_SCRIPTING
#include "Log.h"

#define FO_API_ENUM_ENTRY(group, name, value) static int group##_##name = value;
#include "ScriptApi.h"

#define FO_ENTRY_POINT(func_name) extern void func_name(void*);
#include "NativeScriptingEntries.h"

class ScriptEntity
{
protected:
    raw_ptr<void> _mainObjPtr {};
    raw_ptr<Entity> _thisPtr {};
};

class ScriptPlayer;
class ScriptItem;
class ScriptCritter;
class ScriptMap;
class ScriptLocation;
class ScriptGame;

template<typename T, typename T2>
inline T* MarshalObj(T2* value)
{
    return 0;
}

template<typename T, typename T2>
inline vector<T*> MarshalObjArr(vector<T2*> arr)
{
    return {};
}

template<typename T, typename T2>
inline std::function<void(T*)> MarshalCallback(std::function<void(T2*)> func)
{
    return {};
}

template<typename T, typename T2>
inline std::function<bool(T*)> MarshalPredicate(std::function<void(T2*)> func)
{
    return {};
}

template<typename T>
inline vector<T> MarshalBack(vector<T> arr)
{
    return {};
}

inline ScriptEntity* MarshalBack(Entity* obj)
{
    return 0;
}

inline vector<ScriptEntity*> MarshalBack(vector<Entity*> obj)
{
    return {};
}

#if FO_SERVER_SCRIPTING
inline ScriptPlayer* MarshalBack(Player* obj)
{
    return 0;
}
inline ScriptItem* MarshalBack(Item* obj)
{
    return 0;
}
inline ScriptCritter* MarshalBack(Critter* obj)
{
    return 0;
}
inline ScriptMap* MarshalBack(Map* obj)
{
    return 0;
}
inline ScriptLocation* MarshalBack(Location* obj)
{
    return 0;
}
inline vector<ScriptPlayer*> MarshalBack(vector<Player*> obj)
{
    return {};
}
inline vector<ScriptItem*> MarshalBack(vector<Item*> obj)
{
    return {};
}
inline vector<ScriptCritter*> MarshalBack(vector<Critter*> obj)
{
    return {};
}
inline vector<ScriptMap*> MarshalBack(vector<Map*> obj)
{
    return {};
}
inline vector<ScriptLocation*> MarshalBack(vector<Location*> obj)
{
    return {};
}
#else
inline ScriptPlayer* MarshalBack(PlayerView* obj)
{
    return 0;
}
inline ScriptItem* MarshalBack(ItemView* obj)
{
    return 0;
}
inline ScriptCritter* MarshalBack(CritterView* obj)
{
    return 0;
}
inline ScriptMap* MarshalBack(MapView* obj)
{
    return 0;
}
inline ScriptLocation* MarshalBack(LocationView* obj)
{
    return 0;
}
inline vector<ScriptPlayer*> MarshalBack(vector<PlayerView*> obj)
{
    return {};
}
inline vector<ScriptItem*> MarshalBack(vector<ItemView*> obj)
{
    return {};
}
inline vector<ScriptItem*> MarshalBack(vector<ItemHexView*> obj)
{
    return {};
}
inline vector<ScriptCritter*> MarshalBack(vector<CritterView*> obj)
{
    return {};
}
inline vector<ScriptMap*> MarshalBack(vector<MapView*> obj)
{
    return {};
}
inline vector<ScriptLocation*> MarshalBack(vector<LocationView*> obj)
{
    return {};
}
#endif

inline string MarshalBack(string value)
{
    return value;
}

template<typename T, std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>, int> = 0>
inline T MarshalBack(T value)
{
    return value;
}

#define FO_API_PARTLY_UNDEF 1
#define EntityType_Entity ScriptEntity
#define EntityType_Player ScriptPlayer
#define EntityType_Item ScriptItem
#define EntityType_Critter ScriptCritter
#define EntityType_Map ScriptMap
#define EntityType_Location ScriptLocation
#define EntityType_PlayerView ScriptPlayer
#define EntityType_ItemView ScriptItem
#define EntityType_ItemHexView ScriptItem
#define EntityType_CritterView ScriptCritter
#define EntityType_MapView ScriptMap
#define EntityType_LocationView ScriptLocation
#define EntityType_MapSprite MapSprite
#define FO_API_ARG(type, name) type name
#define FO_API_ARG_ARR(type, name) vector<type> name
#define FO_API_ARG_OBJ(type, name) EntityType_##type* _##name
#define FO_API_ARG_OBJ_ARR(type, name) vector<EntityType_##type*> _##name
#define FO_API_ARG_REF(type, name) type& name
#define FO_API_ARG_ARR_REF(type, name) vector<type>& name
#define FO_API_ARG_ENUM(type, name) int name
#define FO_API_ARG_CALLBACK(type, name) std::function<void(EntityType_##type*)> _##name
#define FO_API_ARG_PREDICATE(type, name) std::function<bool(EntityType_##type*)> _##name
#define FO_API_ARG_DICT(key, val, name) map<key, val> name
#define FO_API_ARG_MARSHAL(type, name)
#define FO_API_ARG_ARR_MARSHAL(type, name)
#define FO_API_ARG_OBJ_MARSHAL(type, name) type* name = MarshalObj<type, EntityType_##type>(_##name);
#define FO_API_ARG_OBJ_ARR_MARSHAL(type, name) vector<type*> name = MarshalObjArr<type, EntityType_##type>(_##name);
#define FO_API_ARG_REF_MARSHAL(type, name)
#define FO_API_ARG_ARR_REF_MARSHAL(type, name)
#define FO_API_ARG_ENUM_MARSHAL(type, name)
#define FO_API_ARG_CALLBACK_MARSHAL(type, name) std::function<void(type*)> name = MarshalCallback<type, EntityType_##type>(_##name);
#define FO_API_ARG_PREDICATE_MARSHAL(type, name) std::function<bool(type*)> name = MarshalPredicate<type, EntityType_##type>(_##name);
#define FO_API_ARG_DICT_MARSHAL(key, val, name)
#define FO_API_RET(type) type
#define FO_API_RET_ARR(type) vector<type>
#define FO_API_RET_OBJ(type) EntityType_##type*
#define FO_API_RET_OBJ_ARR(type) vector<EntityType_##type*>
#define FO_API_RETURN(expr) return MarshalBack(expr)
#define FO_API_RETURN_VOID() return
#define FO_API_PROPERTY_TYPE(type) type
#define FO_API_PROPERTY_TYPE_ARR(type) vector<type>
#define FO_API_PROPERTY_TYPE_OBJ(type) EntityType_##type*
#define FO_API_PROPERTY_TYPE_OBJ_ARR(type) vector<EntityType_##type*>
#define FO_API_PROPERTY_TYPE_ENUM(type) int
#define FO_API_PROPERTY_MOD(mod)

#if FO_SERVER_SCRIPTING
#define CONTEXT_ARG \
    FOServer* _server = (FOServer*)_mainObjPtr; \
    FOServer* _common = _server
#elif FO_CLIENT_SCRIPTING
#define CONTEXT_ARG \
    FOClient* _client = (FOClient*)_mainObjPtr; \
    FOClient* _common = _client
#elif FO_MAPPER_SCRIPTING
#define CONTEXT_ARG \
    FOMapper* _mapper = (FOMapper*)_mainObjPtr; \
    FOMapper* _common = _mapper
#endif

#define FO_API_PROLOG(...) \
    { \
        CONTEXT_ARG; \
        THIS_ARG; \
        __VA_ARGS__
#define FO_API_EPILOG(...) }

class ScriptPlayer : public ScriptEntity
{
public:
#if FO_SERVER_SCRIPTING
#define THIS_ARG Player* _player = (Player*)_thisPtr
#define FO_API_PLAYER_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_PLAYER_METHOD_IMPL 1
#define PLAYER_CLASS Player
#elif FO_CLIENT_SCRIPTING
#define THIS_ARG PlayerView* _playerView = (PlayerView*)_thisPtr
#define FO_API_PLAYER_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_PLAYER_VIEW_METHOD_IMPL 1
#define PLAYER_CLASS PlayerView
#elif FO_MAPPER_SCRIPTING
#define PLAYER_CLASS PlayerView
#endif
#define FO_API_PLAYER_READONLY_PROPERTY(access, type, name, ...) \
    type Get##name() { return MarshalBack(((PLAYER_CLASS*)_thisPtr)->Get##name()); }
#define FO_API_PLAYER_PROPERTY(access, type, name, ...) \
    FO_API_PLAYER_READONLY_PROPERTY(access, type, name, __VA_ARGS__); \
    void Set##name(type value) { ((PLAYER_CLASS*)_thisPtr)->Set##name(value); }
#include "ScriptApi.h"
#undef THIS_ARG
#undef PLAYER_CLASS
};

class ScriptItem : public ScriptEntity
{
public:
#if FO_SERVER_SCRIPTING
#define THIS_ARG Item* _item = (Item*)_thisPtr
#define FO_API_ITEM_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_ITEM_METHOD_IMPL 1
#define ITEM_CLASS Item
#elif FO_CLIENT_SCRIPTING
#define THIS_ARG ItemView* _itemView = (ItemView*)_thisPtr
#define FO_API_ITEM_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_ITEM_VIEW_METHOD_IMPL 1
#define ITEM_CLASS ItemView
#elif FO_MAPPER_SCRIPTING
#define ITEM_CLASS ItemView
#endif
#define FO_API_ITEM_READONLY_PROPERTY(access, type, name, ...) \
    type Get##name() { return MarshalBack(((ITEM_CLASS*)_thisPtr)->Get##name()); }
#define FO_API_ITEM_PROPERTY(access, type, name, ...) \
    FO_API_ITEM_READONLY_PROPERTY(access, type, name, __VA_ARGS__); \
    void Set##name(type value) { ((ITEM_CLASS*)_thisPtr)->Set##name(value); }
#include "ScriptApi.h"
#undef THIS_ARG
#undef ITEM_CLASS
};

class ScriptCritter : public ScriptEntity
{
public:
#if FO_SERVER_SCRIPTING
#define THIS_ARG Critter* _critter = (Critter*)_thisPtr
#define FO_API_CRITTER_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_CRITTER_METHOD_IMPL 1
#define CRITTER_CLASS Critter
#elif FO_CLIENT_SCRIPTING
#define THIS_ARG CritterView* _critterView = (CritterView*)_thisPtr
#define FO_API_CRITTER_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_CRITTER_VIEW_METHOD_IMPL 1
#define CRITTER_CLASS CritterView
#elif FO_MAPPER_SCRIPTING
#define CRITTER_CLASS CritterView
#endif
#define FO_API_CRITTER_READONLY_PROPERTY(access, type, name, ...) \
    type Get##name() { return MarshalBack(((CRITTER_CLASS*)_thisPtr)->Get##name()); }
#define FO_API_CRITTER_PROPERTY(access, type, name, ...) \
    FO_API_CRITTER_READONLY_PROPERTY(access, type, name, __VA_ARGS__); \
    void Set##name(type value) { ((CRITTER_CLASS*)_thisPtr)->Set##name(value); }
#include "ScriptApi.h"
#undef THIS_ARG
#undef CRITTER_CLASS
};

class ScriptMap : public ScriptEntity
{
public:
#if FO_SERVER_SCRIPTING
#define THIS_ARG Map* _map = (Map*)_thisPtr
#define FO_API_MAP_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_MAP_METHOD_IMPL 1
#define MAP_CLASS Map
#elif FO_CLIENT_SCRIPTING
#define THIS_ARG MapView* _mapView = (MapView*)_thisPtr
#define FO_API_MAP_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_MAP_VIEW_METHOD_IMPL 1
#define MAP_CLASS MapView
#elif FO_MAPPER_SCRIPTING
#define MAP_CLASS MapView
#endif
#define FO_API_MAP_READONLY_PROPERTY(access, type, name, ...) \
    type Get##name() { return MarshalBack(((MAP_CLASS*)_thisPtr)->Get##name()); }
#define FO_API_MAP_PROPERTY(access, type, name, ...) \
    FO_API_MAP_READONLY_PROPERTY(access, type, name, __VA_ARGS__); \
    void Set##name(type value) { ((MAP_CLASS*)_thisPtr)->Set##name(value); }
#include "ScriptApi.h"
#undef THIS_ARG
#undef MAP_CLASS
};

class ScriptLocation : public ScriptEntity
{
public:
#if FO_SERVER_SCRIPTING
#define THIS_ARG Location* _location = (Location*)_thisPtr
#define FO_API_LOCATION_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_LOCATION_METHOD_IMPL 1
#define LOCATION_CLASS Location
#elif FO_CLIENT_SCRIPTING
#define THIS_ARG LocationView* _locationView = (LocationView*)_thisPtr
#define FO_API_LOCATION_VIEW_METHOD(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_LOCATION_VIEW_METHOD_IMPL 1
#define LOCATION_CLASS LocationView
#elif FO_MAPPER_SCRIPTING
#define LOCATION_CLASS LocationView
#endif
#define FO_API_LOCATION_READONLY_PROPERTY(access, type, name, ...) \
    type Get##name() { return MarshalBack(((LOCATION_CLASS*)_thisPtr)->Get##name()); }
#define FO_API_LOCATION_PROPERTY(access, type, name, ...) \
    FO_API_LOCATION_READONLY_PROPERTY(access, type, name, __VA_ARGS__); \
    void Set##name(type value) { ((LOCATION_CLASS*)_thisPtr)->Set##name(value); }
#include "ScriptApi.h"
#undef THIS_ARG
#undef LOCATION_CLASS
};

class ScriptGame
{
public:
#define THIS_ARG (void)0
#define FO_API_GLOBAL_COMMON_FUNC(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_GLOBAL_COMMON_FUNC_IMPL 1
#if FO_SERVER_SCRIPTING
#define FO_API_GLOBAL_SERVER_FUNC(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_GLOBAL_SERVER_FUNC_IMPL 1
#elif FO_CLIENT_SCRIPTING
#define FO_API_GLOBAL_CLIENT_FUNC(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_GLOBAL_CLIENT_FUNC_IMPL 1
#elif FO_MAPPER_SCRIPTING
#define FO_API_GLOBAL_MAPPER_FUNC(name, ret, ...) ret name(__VA_ARGS__)
#define FO_API_GLOBAL_MAPPER_FUNC_IMPL 1
#endif
#include "ScriptApi.h"
#undef THIS_ARG
private:
    raw_ptr<void> _mainObjPtr {};
};

#undef FO_API_PARTLY_UNDEF
#include "ScriptApi.h"

struct ScriptSystem::NativeImpl
{
};

void SCRIPTING_CLASS::InitNativeScripting()
{
    _pNativeImpl = std::make_unique<NativeImpl>();

#define FO_ENTRY_POINT(func_name) func_name(nullptr);
#include "NativeScriptingEntries.h"
}

#else
struct ScriptSystem::NativeImpl
{
};
void SCRIPTING_CLASS::InitNativeScripting()
{
}
#endif
