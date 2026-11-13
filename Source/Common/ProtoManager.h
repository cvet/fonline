// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "EntityProtos.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

class EngineMetadata;

class ProtoManager final
{
public:
    explicit ProtoManager(ptr<EngineMetadata> meta);
    ProtoManager(const ProtoManager&) = delete;
    ProtoManager(ProtoManager&&) noexcept = delete;
    auto operator=(const ProtoManager&) = delete;
    auto operator=(ProtoManager&&) noexcept = delete;
    ~ProtoManager() = default;

    [[nodiscard]] auto GetAllProtos() const -> const auto& { return _protos; }

    [[nodiscard]] auto GetProtoItem(hstring proto_id) const noexcept -> nptr<const ProtoItem>;
    [[nodiscard]] auto GetProtoCritter(hstring proto_id) const noexcept -> nptr<const ProtoCritter>;
    [[nodiscard]] auto GetProtoMap(hstring proto_id) const noexcept -> nptr<const ProtoMap>;
    [[nodiscard]] auto GetProtoLocation(hstring proto_id) const noexcept -> nptr<const ProtoLocation>;
    [[nodiscard]] auto GetProtoEntity(hstring type_name, hstring proto_id) const noexcept -> nptr<const ProtoEntity>;

    [[nodiscard]] auto GetProtoItems() const noexcept -> const auto& { return _itemProtos; }
    [[nodiscard]] auto GetProtoCritters() const noexcept -> const auto& { return _crProtos; }
    [[nodiscard]] auto GetProtoMaps() const noexcept -> const auto& { return _mapProtos; }
    [[nodiscard]] auto GetProtoLocations() const noexcept -> const auto& { return _locProtos; }
    [[nodiscard]] auto GetProtoEntities(hstring type_name) const noexcept -> const unordered_map<hstring, refcount_ptr<ProtoEntity>>&;

    void AddProto(hstring type_name, refcount_ptr<ProtoEntity> proto);
    void LoadFromResources(const FileSystem& resources);

private:
    auto CreateProto(hstring type_name, hstring pid, nptr<const Properties> props) -> ptr<ProtoEntity>;

    ptr<EngineMetadata> _meta;
    const hstring _migrationRuleName;
    const hstring _itemTypeName;
    const hstring _crTypeName;
    const hstring _mapTypeName;
    const hstring _locTypeName;
    unordered_map<hstring, ptr<const ProtoItem>> _itemProtos {};
    unordered_map<hstring, ptr<const ProtoCritter>> _crProtos {};
    unordered_map<hstring, ptr<const ProtoMap>> _mapProtos {};
    unordered_map<hstring, ptr<const ProtoLocation>> _locProtos {};
    unordered_map<hstring, unordered_map<hstring, refcount_ptr<ProtoEntity>>> _protos {};
    const unordered_map<hstring, refcount_ptr<ProtoEntity>> _emptyProtos {};
};

FO_END_NAMESPACE
