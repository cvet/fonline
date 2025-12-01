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

#pragma once

#include "Common.h"

#include "EntityProtos.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(ProtoManagerException);

class EngineData;

class ProtoManager final
{
public:
    explicit ProtoManager(EngineData& engine);
    ProtoManager(const ProtoManager&) = delete;
    ProtoManager(ProtoManager&&) noexcept = delete;
    auto operator=(const ProtoManager&) = delete;
    auto operator=(ProtoManager&&) noexcept = delete;
    ~ProtoManager() = default;

    [[nodiscard]] auto GetAllProtos() const -> const auto& { return _protos; }

    [[nodiscard]] auto GetProtoItem(hstring proto_id) const noexcept(false) -> FO_NON_NULL const ProtoItem*;
    [[nodiscard]] auto GetProtoCritter(hstring proto_id) const noexcept(false) -> FO_NON_NULL const ProtoCritter*;
    [[nodiscard]] auto GetProtoMap(hstring proto_id) const noexcept(false) -> FO_NON_NULL const ProtoMap*;
    [[nodiscard]] auto GetProtoLocation(hstring proto_id) const noexcept(false) -> FO_NON_NULL const ProtoLocation*;
    [[nodiscard]] auto GetProtoEntity(hstring type_name, hstring proto_id) const noexcept(false) -> FO_NON_NULL const ProtoEntity*;

    [[nodiscard]] auto GetProtoItemSafe(hstring proto_id) const noexcept -> const ProtoItem*;
    [[nodiscard]] auto GetProtoCritterSafe(hstring proto_id) const noexcept -> const ProtoCritter*;
    [[nodiscard]] auto GetProtoMapSafe(hstring proto_id) const noexcept -> const ProtoMap*;
    [[nodiscard]] auto GetProtoLocationSafe(hstring proto_id) const noexcept -> const ProtoLocation*;
    [[nodiscard]] auto GetProtoEntitySafe(hstring type_name, hstring proto_id) const noexcept -> const ProtoEntity*;

    [[nodiscard]] auto GetProtoItems() const noexcept -> const auto& { return _itemProtos; }
    [[nodiscard]] auto GetProtoCritters() const noexcept -> const auto& { return _crProtos; }
    [[nodiscard]] auto GetProtoMaps() const noexcept -> const auto& { return _mapProtos; }
    [[nodiscard]] auto GetProtoLocations() const noexcept -> const auto& { return _locProtos; }
    [[nodiscard]] auto GetProtoEntities(hstring type_name) const noexcept -> const unordered_map<hstring, refcount_ptr<ProtoEntity>>&;

    void LoadFromResources(const FileSystem& resources);

private:
    auto CreateProto(hstring type_name, hstring pid, const Properties* props) -> ProtoEntity*;

    raw_ptr<EngineData> _engine;
    const hstring _migrationRuleName;
    const hstring _itemTypeName;
    const hstring _crTypeName;
    const hstring _mapTypeName;
    const hstring _locTypeName;
    unordered_map<hstring, raw_ptr<const ProtoItem>> _itemProtos {};
    unordered_map<hstring, raw_ptr<const ProtoCritter>> _crProtos {};
    unordered_map<hstring, raw_ptr<const ProtoMap>> _mapProtos {};
    unordered_map<hstring, raw_ptr<const ProtoLocation>> _locProtos {};
    unordered_map<hstring, unordered_map<hstring, refcount_ptr<ProtoEntity>>> _protos {};
    const unordered_map<hstring, refcount_ptr<ProtoEntity>> _emptyProtos {};
};

FO_END_NAMESPACE();
