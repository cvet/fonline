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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "EntitySync.h"
#include "ServerEntity.h"

FO_BEGIN_NAMESPACE

class Map;
class Location;

class Location final : public ServerEntity, public EntityWithProto, public LocationProperties
{
public:
    Location() = delete;
    Location(ServerEngine* engine, ident_t id, const ProtoLocation* proto, const Properties* props = nullptr) noexcept;
    Location(const Location&) = delete;
    Location(Location&&) noexcept = delete;
    auto operator=(const Location&) = delete;
    auto operator=(Location&&) noexcept = delete;
    ~Location() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override
    {
        FO_NO_VALIDATE_ENTITY_ACCESS();
        return _proto->GetName();
    }
    [[nodiscard]] auto GetProtoLoc() const noexcept -> const ProtoLocation*
    {
        FO_VALIDATE_ENTITY_ACCESS_STRONG();
        return static_cast<const ProtoLocation*>(_proto.get());
    }
    [[nodiscard]] auto HasMaps() const -> bool;
    [[nodiscard]] auto GetMaps() const -> vector<const Map*>;
    [[nodiscard]] auto GetMaps() -> vector<Map*>;
    [[nodiscard]] auto GetRawMaps() noexcept -> auto&
    {
        FO_VALIDATE_ENTITY_ACCESS_STRONG();
        return _locMaps;
    }
    [[nodiscard]] auto GetMapByIndex(int32_t index) noexcept -> Map*;
    [[nodiscard]] auto GetMapsCount() const -> size_t;
    [[nodiscard]] auto GetMapByPid(hstring map_pid) noexcept -> Map*;
    [[nodiscard]] auto GetMapIndex(hstring map_pid) const -> size_t;

    void AddMap(Map* map);
    void RemoveMap(Map* map);

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapAdded, Map* /*map*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapRemoved, Map* /*map*/);

private:
    vector<refcount_ptr<Map>> _locMaps {};
    EntityLock _ownedLock {};
};

FO_END_NAMESPACE
