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

#include "Common.h"

#include "Client.h"
#include "GenericUtils.h"
#include "Mapper.h"
#include "NetCommand.h"
#include "PropertiesSerializator.h"
#include "Server.h"
#include "Sprites.h"
#include "Version-Include.h"

#include "sha1.h"
#include "sha2.h"

class MapSprite final
{
public:
    void AddRef() const { ++RefCount; }
    void Release() const
    {
        if (--RefCount == 0) {
            delete this;
        }
    }
    static auto Factory() -> MapSprite* { return new MapSprite(); }

    mutable int RefCount {1};
    bool Valid {};
    uint SprId {};
    ushort HexX {};
    ushort HexY {};
    hash ProtoId {};
    int FrameIndex {};
    int OffsX {};
    int OffsY {};
    bool IsFlat {};
    bool NoLight {};
    int DrawOrder {};
    int DrawOrderHyOffset {};
    int Corner {};
    bool DisableEgg {};
    uint Color {};
    uint ContourColor {};
    bool IsTweakOffs {};
    short TweakOffsX {};
    short TweakOffsY {};
    bool IsTweakAlpha {};
    uchar TweakAlpha {};
};

#define FO_API_ARG(type, name) const type& name
#define FO_API_ARG_ARR(type, name) const vector<type>& name
#define FO_API_ARG_OBJ(type, name) type* const name
#define FO_API_ARG_OBJ_ARR(type, name) const vector<type*>& name
#define FO_API_ARG_REF(type, name) type& name
#define FO_API_ARG_ARR_REF(type, name) vector<type>& name
#define FO_API_ARG_ENUM(type, name) const type& name
#define FO_API_ARG_CALLBACK(type, name) const std::function<void(type*)>& name
#define FO_API_ARG_PREDICATE(type, name) const std::function<bool(type*)>& name
#define FO_API_ARG_DICT(key, val, name) const map<key, val>& name
#define FO_API_ARG_MARSHAL(type, name)
#define FO_API_ARG_ARR_MARSHAL(type, name)
#define FO_API_ARG_OBJ_MARSHAL(type, name)
#define FO_API_ARG_OBJ_ARR_MARSHAL(type, name)
#define FO_API_ARG_REF_MARSHAL(type, name)
#define FO_API_ARG_ARR_REF_MARSHAL(type, name)
#define FO_API_ARG_ENUM_MARSHAL(type, name)
#define FO_API_ARG_CALLBACK_MARSHAL(type, name)
#define FO_API_ARG_PREDICATE_MARSHAL(type, name)
#define FO_API_ARG_DICT_MARSHAL(key, val, name)
#define FO_API_RET(type) type
#define FO_API_RET_ARR(type) vector<type>
#define FO_API_RET_OBJ(type) type*
#define FO_API_RET_OBJ_ARR(type) vector<type*>
#define FO_API_RETURN(expr) return expr
#define FO_API_RETURN_VOID() return
#define FO_API_PROPERTY_TYPE(type) type
#define FO_API_PROPERTY_TYPE_ARR(type) vector<type>
#define FO_API_PROPERTY_TYPE_OBJ(type) type*
#define FO_API_PROPERTY_TYPE_OBJ_ARR(type) vector<type*>
#define FO_API_PROPERTY_TYPE_ENUM(type) type
#define FO_API_PROPERTY_MOD(mod)

#define FO_API_PROLOG(...)
#define FO_API_EPILOG(...) }

#define DUMMY_PTR(type) reinterpret_cast<type*>(1000000)

#define FO_API_ITEM_METHOD(name, ret, ...) \
    [[maybe_unused]] static ret Lint_Item_##name(__VA_ARGS__) \
    { \
        auto* _server = DUMMY_PTR(FOServer); \
        auto* _common = DUMMY_PTR(FOServer); \
        auto* _item = DUMMY_PTR(Item);
#define FO_API_ITEM_METHOD_IMPL 1
#define FO_API_CRITTER_METHOD(name, ret, ...) \
    [[maybe_unused]] static ret Lint_Critter_##name(__VA_ARGS__) \
    { \
        auto* _server = DUMMY_PTR(FOServer); \
        auto* _common = DUMMY_PTR(FOServer); \
        auto* _critter = DUMMY_PTR(Critter);
#define FO_API_CRITTER_METHOD_IMPL 1
#define FO_API_MAP_METHOD(name, ret, ...) \
    [[maybe_unused]] static ret Lint_Map_##name(__VA_ARGS__) \
    { \
        auto* _server = DUMMY_PTR(FOServer); \
        auto* _common = DUMMY_PTR(FOServer); \
        auto* _map = DUMMY_PTR(Map);
#define FO_API_MAP_METHOD_IMPL 1
#define FO_API_LOCATION_METHOD(name, ret, ...) \
    [[maybe_unused]] static ret Lint_Location_##name(__VA_ARGS__) \
    { \
        auto* _server = DUMMY_PTR(FOServer); \
        auto* _common = DUMMY_PTR(FOServer); \
        auto* _location = DUMMY_PTR(Location);
#define FO_API_LOCATION_METHOD_IMPL 1
#define FO_API_ITEM_VIEW_METHOD(name, ret, ...) \
    [[maybe_unused]] static ret Lint_ItemView_##name(__VA_ARGS__) \
    { \
        auto* _client = DUMMY_PTR(FOClient); \
        auto* _common = DUMMY_PTR(FOClient); \
        auto* _itemView = DUMMY_PTR(ItemView);
#define FO_API_ITEM_VIEW_METHOD_IMPL 1
#define FO_API_HEX_ITEM_VIEW_METHOD(name, ret, ...) \
    [[maybe_unused]] static ret Lint_HexItemView_##name(__VA_ARGS__) \
    { \
        auto* _client = DUMMY_PTR(FOClient); \
        auto* _common = DUMMY_PTR(FOClient); \
        auto* _itemHexView = DUMMY_PTR(ItemHexView);
#define FO_API_HEX_ITEM_VIEW_METHOD_IMPL 1
#define FO_API_CRITTER_VIEW_METHOD(name, ret, ...) \
    [[maybe_unused]] static ret Lint_CritterView_##name(__VA_ARGS__) \
    { \
        auto* _client = DUMMY_PTR(FOClient); \
        auto* _common = DUMMY_PTR(FOClient); \
        auto* _critterView = DUMMY_PTR(CritterView);
#define FO_API_CRITTER_VIEW_METHOD_IMPL 1
#define FO_API_MAP_VIEW_METHOD(name, ret, ...) \
    [[maybe_unused]] static ret Lint_MapView_##name(__VA_ARGS__) \
    { \
        auto* _client = DUMMY_PTR(FOClient); \
        auto* _common = DUMMY_PTR(FOClient); \
        auto* _mapView = DUMMY_PTR(MapView);
#define FO_API_MAP_VIEW_METHOD_IMPL 1
#define FO_API_LOCATION_VIEW_METHOD(name, ret, ...) \
    [[maybe_unused]] static ret Lint_LocationView_##name(__VA_ARGS__) \
    { \
        auto* _client = DUMMY_PTR(FOClient); \
        auto* _common = DUMMY_PTR(FOClient); \
        auto* _locationView = DUMMY_PTR(LocationView);
#define FO_API_LOCATION_VIEW_METHOD_IMPL 1
#define FO_API_GLOBAL_COMMON_FUNC(name, ret, ...) \
    [[maybe_unused]] static ret Lint_Common_##name(__VA_ARGS__) \
    { \
        auto* _common = DUMMY_PTR(FOServer);
#define FO_API_GLOBAL_COMMON_FUNC_IMPL 1
#define FO_API_GLOBAL_SERVER_FUNC(name, ret, ...) \
    [[maybe_unused]] static ret Lint_Server_##name(__VA_ARGS__) \
    { \
        auto* _server = DUMMY_PTR(FOServer); \
        auto* _common = DUMMY_PTR(FOServer);
#define FO_API_GLOBAL_SERVER_FUNC_IMPL 1
#define FO_API_GLOBAL_CLIENT_FUNC(name, ret, ...) \
    [[maybe_unused]] static ret Lint_Client_##name(__VA_ARGS__) \
    { \
        auto* _client = DUMMY_PTR(FOClient); \
        auto* _common = DUMMY_PTR(FOClient);
#define FO_API_GLOBAL_CLIENT_FUNC_IMPL 1
#define FO_API_GLOBAL_MAPPER_FUNC(name, ret, ...) \
    [[maybe_unused]] static ret Lint_Mapper_##name(__VA_ARGS__) \
    { \
        auto* _mapper = DUMMY_PTR(FOMapper); \
        auto* _common = DUMMY_PTR(FOMapper);
#define FO_API_GLOBAL_MAPPER_FUNC_IMPL 1

#define FO_API_ENUM_GROUP(...)
#define FO_API_ENUM_ENTRY(group, name, value) static int group##_##name = value;
#define FO_API_COMMON_EVENT(...)
#define FO_API_SERVER_EVENT(...)
#define FO_API_CLIENT_EVENT(...)
#define FO_API_MAPPER_EVENT(...)
#define FO_API_SETTING(...)
#define FO_API_GLOBAL_PROPERTY(...)
#define FO_API_GLOBAL_READONLY_PROPERTY(...)
#define FO_API_ITEM_PROPERTY(...)
#define FO_API_ITEM_READONLY_PROPERTY(...)
#define FO_API_CRITTER_PROPERTY(...)
#define FO_API_CRITTER_READONLY_PROPERTY(...)
#define FO_API_MAP_PROPERTY(...)
#define FO_API_MAP_READONLY_PROPERTY(...)
#define FO_API_LOCATION_PROPERTY(...)
#define FO_API_LOCATION_READONLY_PROPERTY(...)

#define FO_API_ANGELSCRIPT_ONLY 1
#define FO_API_MONO_ONLY 1
#define FO_API_NATIVE_ONLY 1

// #define FO_API_COMMON_IMPL
// #define FO_API_SERVER_IMPL
// #define FO_API_CLIENT_IMPL
// #define FO_API_MAPPER_IMPL

#if !FO_API_MULTIPLAYER_ONLY
#undef FO_API_MULTIPLAYER_ONLY
#define FO_API_MULTIPLAYER_ONLY 1
#endif
#if !FO_API_SINGLEPLAYER_ONLY
#undef FO_API_SINGLEPLAYER_ONLY
#define FO_API_SINGLEPLAYER_ONLY 1
#endif

#include "ScriptApi-Enums.h"

#include "ScriptApi-Events.h"

#include "ScriptApi-Properties.h"

#include "ScriptApi-Common.h"

#include "ScriptApi-Client.h"
#include "ScriptApi-Mapper.h"
#include "ScriptApi-Server.h"

#include "ScriptApi-Custom.h"
