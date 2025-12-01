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

#include "ProtoManager.h"
#include "ConfigFile.h"
#include "EngineBase.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE();

ProtoManager::ProtoManager(EngineData& engine) :
    _engine {&engine},
    _migrationRuleName {_engine->Hashes.ToHashedString("Proto")},
    _itemTypeName {_engine->Hashes.ToHashedString(ProtoItem::ENTITY_TYPE_NAME)},
    _crTypeName {_engine->Hashes.ToHashedString(ProtoCritter::ENTITY_TYPE_NAME)},
    _mapTypeName {_engine->Hashes.ToHashedString(ProtoMap::ENTITY_TYPE_NAME)},
    _locTypeName {_engine->Hashes.ToHashedString(ProtoLocation::ENTITY_TYPE_NAME)}
{
    FO_STACK_TRACE_ENTRY();
}

auto ProtoManager::CreateProto(hstring type_name, hstring pid, const Properties* props) -> ProtoEntity*
{
    FO_STACK_TRACE_ENTRY();

    const auto create_proto = [&]() -> refcount_ptr<ProtoEntity> {
        const auto* registrator = _engine->GetPropertyRegistrator(type_name);
        FO_RUNTIME_ASSERT(registrator);

        if (type_name == ProtoLocation::ENTITY_TYPE_NAME) {
            auto proto = SafeAlloc::MakeRefCounted<ProtoLocation>(pid, registrator, props);
            _locProtos.emplace(pid, proto.get());
            return proto;
        }
        else if (type_name == ProtoMap::ENTITY_TYPE_NAME) {
            auto proto = SafeAlloc::MakeRefCounted<ProtoMap>(pid, registrator, props);
            _mapProtos.emplace(pid, proto.get());
            return proto;
        }
        else if (type_name == ProtoCritter::ENTITY_TYPE_NAME) {
            auto proto = SafeAlloc::MakeRefCounted<ProtoCritter>(pid, registrator, props);
            _crProtos.emplace(pid, proto.get());
            return proto;
        }
        else if (type_name == ProtoItem::ENTITY_TYPE_NAME) {
            auto proto = SafeAlloc::MakeRefCounted<ProtoItem>(pid, registrator, props);
            _itemProtos.emplace(pid, proto.get());
            return proto;
        }
        else {
            return SafeAlloc::MakeRefCounted<ProtoCustomEntity>(pid, registrator, props);
        }
    };

    auto proto = create_proto();

    const auto inserted = _protos[type_name].emplace(pid, proto).second;
    FO_RUNTIME_ASSERT(inserted);

    return proto.get();
}

void ProtoManager::LoadFromResources(const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    string protos_ext;

    switch (_engine->GetPropertiesRelation()) {
    case PropertiesRelationType::BothRelative:
        protos_ext = "fopro-bin-mapper";
        break;
    case PropertiesRelationType::ServerRelative:
        protos_ext = "fopro-bin-server";
        break;
    case PropertiesRelationType::ClientRelative:
        protos_ext = "fopro-bin-client";
        break;
    }

    auto proto_files = resources.FilterFiles(protos_ext);

    for (const auto& proto_file_header : proto_files) {
        const auto proto_file = File::Load(proto_file_header);
        auto reader = DataReader({proto_file.GetBuf(), proto_file.GetSize()});

        // Hashes
        {
            const auto hashes_count = reader.Read<uint32>();
            string str;

            for (uint32 i = 0; i < hashes_count; i++) {
                const auto str_len = reader.Read<uint32>();
                str.resize(str_len);
                reader.ReadPtr(str.data(), str.length());
                const auto hstr = _engine->Hashes.ToHashedString(str);
                ignore_unused(hstr);
            }
        }

        // Protos
        {
            const auto types_count = reader.Read<uint32>();
            vector<uint8> props_data;

            for (uint32 i = 0; i < types_count; i++) {
                const auto protos_count = reader.Read<uint32>();

                const auto type_name_len = reader.Read<uint16>();
                const auto type_name_str = string(reader.ReadPtr<char>(type_name_len), type_name_len);
                const auto type_name = _engine->Hashes.ToHashedString(type_name_str);

                FO_RUNTIME_ASSERT(_engine->IsValidEntityType(type_name));

                for (uint32 j = 0; j < protos_count; j++) {
                    const auto proto_name_len = reader.Read<uint16>();
                    const auto proto_name = string(reader.ReadPtr<char>(proto_name_len), proto_name_len);
                    const auto proto_id = _engine->Hashes.ToHashedString(proto_name);

                    auto* proto = CreateProto(type_name, proto_id, nullptr);

                    const auto components_count = reader.Read<uint16>();

                    for (uint16 k = 0; k < components_count; k++) {
                        const auto component_name_len = reader.Read<uint16>();
                        const auto component_name = string(reader.ReadPtr<char>(component_name_len), component_name_len);
                        const auto component_name_hashed = _engine->Hashes.ToHashedString(component_name);

                        proto->EnableComponent(component_name_hashed);
                    }

                    const auto data_size = reader.Read<uint32>();
                    props_data.resize(data_size);
                    reader.ReadPtr<uint8>(props_data.data(), data_size);
                    proto->GetPropertiesForEdit().RestoreAllData(props_data);
                }
            }
        }

        reader.VerifyEnd();
    }
}

auto ProtoManager::GetProtoItem(hstring proto_id) const noexcept(false) -> const ProtoItem*
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _itemTypeName, proto_id).value_or(proto_id);

    if (const auto it = _itemProtos.find(proto_id); it != _itemProtos.end()) {
        return it->second.get();
    }

    throw ProtoManagerException("Item proto not exists", proto_id);
}

auto ProtoManager::GetProtoCritter(hstring proto_id) const noexcept(false) -> const ProtoCritter*
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _crTypeName, proto_id).value_or(proto_id);

    if (const auto it = _crProtos.find(proto_id); it != _crProtos.end()) {
        return it->second.get();
    }

    throw ProtoManagerException("Critter proto not exists", proto_id);
}

auto ProtoManager::GetProtoMap(hstring proto_id) const noexcept(false) -> const ProtoMap*
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _mapTypeName, proto_id).value_or(proto_id);

    if (const auto it = _mapProtos.find(proto_id); it != _mapProtos.end()) {
        return it->second.get();
    }

    throw ProtoManagerException("Map proto not exists", proto_id);
}

auto ProtoManager::GetProtoLocation(hstring proto_id) const noexcept(false) -> const ProtoLocation*
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _locTypeName, proto_id).value_or(proto_id);

    if (const auto it = _locProtos.find(proto_id); it != _locProtos.end()) {
        return it->second.get();
    }

    throw ProtoManagerException("Location proto not exists", proto_id);
}

auto ProtoManager::GetProtoEntity(hstring type_name, hstring proto_id) const noexcept(false) -> const ProtoEntity*
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, type_name, proto_id).value_or(proto_id);

    const auto it_type = _protos.find(type_name);

    if (it_type == _protos.end()) {
        throw ProtoManagerException("Entity type protos not exist", type_name);
    }

    if (const auto it = it_type->second.find(proto_id); it != it_type->second.end()) {
        return it->second.get();
    }

    throw ProtoManagerException("Entity proto not exists", type_name, proto_id);
}

auto ProtoManager::GetProtoItemSafe(hstring proto_id) const noexcept -> const ProtoItem*
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _itemTypeName, proto_id).value_or(proto_id);

    if (const auto it = _itemProtos.find(proto_id); it != _itemProtos.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto ProtoManager::GetProtoCritterSafe(hstring proto_id) const noexcept -> const ProtoCritter*
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _crTypeName, proto_id).value_or(proto_id);

    if (const auto it = _crProtos.find(proto_id); it != _crProtos.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto ProtoManager::GetProtoMapSafe(hstring proto_id) const noexcept -> const ProtoMap*
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _mapTypeName, proto_id).value_or(proto_id);

    if (const auto it = _mapProtos.find(proto_id); it != _mapProtos.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto ProtoManager::GetProtoLocationSafe(hstring proto_id) const noexcept -> const ProtoLocation*
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _locTypeName, proto_id).value_or(proto_id);

    if (const auto it = _locProtos.find(proto_id); it != _locProtos.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto ProtoManager::GetProtoEntitySafe(hstring type_name, hstring proto_id) const noexcept -> const ProtoEntity*
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, type_name, proto_id).value_or(proto_id);

    const auto it_type = _protos.find(type_name);

    if (it_type == _protos.end()) {
        return nullptr;
    }

    if (const auto it = it_type->second.find(proto_id); it != it_type->second.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto ProtoManager::GetProtoEntities(hstring type_name) const noexcept -> const unordered_map<hstring, refcount_ptr<ProtoEntity>>&
{
    FO_STACK_TRACE_ENTRY();

    const auto it_type = _protos.find(type_name);

    if (it_type == _protos.end()) {
        return _emptyProtos;
    }

    return it_type->second;
}

FO_END_NAMESPACE();
