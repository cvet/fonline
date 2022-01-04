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

#include "EntityProperties.h"
#include "EntityProtos.h"
#include "ServerEntity.h"

class Map;
class Location;

class Location final : public ServerEntity, public LocationProperties
{
public:
    Location() = delete;
    Location(FOServer* engine, uint id, const ProtoLocation* proto);
    Location(const Location&) = delete;
    Location(Location&&) noexcept = delete;
    auto operator=(const Location&) = delete;
    auto operator=(Location&&) noexcept = delete;
    ~Location() override = default;

    void BindScript();
    auto GetProtoLoc() const -> const ProtoLocation*;
    auto IsLocVisible() const -> bool;
    auto GetMapsRaw() -> vector<Map*>&;
    auto GetMaps() -> vector<Map*>;
    auto GetMaps() const -> vector<const Map*>;
    auto GetMapsCount() const -> uint;
    auto GetMapByIndex(uint index) -> Map*;
    auto GetMapByPid(hstring map_pid) -> Map*;
    auto GetMapIndex(hstring map_pid) -> uint;
    auto IsCanEnter(uint players_count) -> bool;
    auto IsNoCrit() -> bool;
    auto IsNoPlayer() -> bool;
    auto IsNoNpc() -> bool;
    auto IsCanDelete() -> bool;

    ///@ ExportEvent
    ENTITY_EVENT(Finish);

    // Todo: encapsulate Location data
    uint EntranceScriptBindId {};
    int GeckCount {};

private:
    vector<Map*> _locMaps {};
};
