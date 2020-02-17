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

#pragma once

#include "Common.h"

#include "Entity.h"
#include "Script.h"

class Map;
using MapVec = vector<Map*>;
using MapMap = map<uint, Map*>;
class Location;
using LocationVec = vector<Location*>;
using LocationMap = map<uint, Location*>;

class Location : public Entity
{
public:
    Location(uint id, ProtoLocation* proto, ScriptSystem& script_sys);
    void BindScript();
    ProtoLocation* GetProtoLoc();
    bool IsLocVisible();
    MapVec& GetMapsRaw();
    MapVec GetMaps();
    uint GetMapsCount();
    Map* GetMapByIndex(uint index);
    Map* GetMapByPid(hash map_pid);
    uint GetMapIndex(hash map_pid);
    bool IsCanEnter(uint players_count);
    bool IsNoCrit();
    bool IsNoPlayer();
    bool IsNoNpc();
    bool IsCanDelete();

    PROPERTIES_HEADER();
#include "LocationProperties.h"
    CLASS_PROPERTY(CScriptArray*, MapProtos); // hash[]
    CLASS_PROPERTY(CScriptArray*, MapEntrances); // hash[]
    CLASS_PROPERTY(CScriptArray*, Automaps); // hash[]
    CLASS_PROPERTY(uint, MaxPlayers);
    CLASS_PROPERTY(bool, AutoGarbage);
    CLASS_PROPERTY(bool, GeckVisible);
    CLASS_PROPERTY(hash, EntranceScript);
    CLASS_PROPERTY(ushort, WorldX);
    CLASS_PROPERTY(ushort, WorldY);
    CLASS_PROPERTY(ushort, Radius);
    CLASS_PROPERTY(bool, Hidden);
    CLASS_PROPERTY(bool, ToGarbage);
    CLASS_PROPERTY(uint, Color);

    // Todo: encapsulate Location data
    uint EntranceScriptBindId {};
    int GeckCount {};

private:
    ScriptSystem& scriptSys;
    MapVec locMaps {};
};
