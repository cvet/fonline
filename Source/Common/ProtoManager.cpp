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

#include "ProtoManager.h"
#include "ConfigFile.h"
#include "EngineBase.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

ProtoManager::ProtoManager(ptr<EngineMetadata> meta) :
    _meta {meta},
    _migrationRuleName {_meta->Hashes.ToHashedString("Proto")},
    _itemTypeName {_meta->Hashes.ToHashedString(ProtoItem::ENTITY_TYPE_NAME)},
    _crTypeName {_meta->Hashes.ToHashedString(ProtoCritter::ENTITY_TYPE_NAME)},
    _mapTypeName {_meta->Hashes.ToHashedString(ProtoMap::ENTITY_TYPE_NAME)},
    _locTypeName {_meta->Hashes.ToHashedString(ProtoLocation::ENTITY_TYPE_NAME)}
{
    FO_STACK_TRACE_ENTRY();
}

void ProtoManager::AddProto(hstring type_name, refcount_ptr<ProtoEntity> proto)
{
    FO_NO_STACK_TRACE_ENTRY();

    hstring proto_id = proto->GetProtoId();
    auto loc = proto.dyn_cast<ProtoLocation>();
    auto map = proto.dyn_cast<ProtoMap>();
    auto cr = proto.dyn_cast<ProtoCritter>();
    auto item = proto.dyn_cast<ProtoItem>();

    auto it = _protos[type_name].emplace(proto_id, std::move(proto));
    FO_VERIFY_AND_THROW(it.second, "Duplicate prototype id", type_name, proto_id);

    if (loc) {
        _locProtos.insert_or_assign(proto_id, loc);
    }
    else if (map) {
        _mapProtos.insert_or_assign(proto_id, map);
    }
    else if (cr) {
        _crProtos.insert_or_assign(proto_id, cr);
    }
    else if (item) {
        _itemProtos.insert_or_assign(proto_id, item);
    }
}

auto ProtoManager::CreateProto(hstring type_name, hstring pid, nptr<const Properties> props) -> ptr<ProtoEntity>
{
    FO_STACK_TRACE_ENTRY();

    auto create_proto = [&]() -> refcount_ptr<ProtoEntity> {
        auto registrator = _meta->GetPropertyRegistrator(type_name);
        FO_VERIFY_AND_THROW(registrator, "Missing property registrator");

        if (type_name == ProtoLocation::ENTITY_TYPE_NAME) {
            return SafeAlloc::MakeRefCounted<ProtoLocation>(pid, registrator, props);
        }
        else if (type_name == ProtoMap::ENTITY_TYPE_NAME) {
            return SafeAlloc::MakeRefCounted<ProtoMap>(pid, registrator, props);
        }
        else if (type_name == ProtoCritter::ENTITY_TYPE_NAME) {
            return SafeAlloc::MakeRefCounted<ProtoCritter>(pid, registrator, props);
        }
        else if (type_name == ProtoItem::ENTITY_TYPE_NAME) {
            return SafeAlloc::MakeRefCounted<ProtoItem>(pid, registrator, props);
        }
        else {
            return SafeAlloc::MakeRefCounted<ProtoCustomEntity>(pid, registrator, props);
        }
    };

    auto proto = create_proto();
    AddProto(type_name, proto);
    return proto;
}

void ProtoManager::LoadFromResources(const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    string protos_ext;

    switch (_meta->GetSide()) {
    case EngineSideKind::ServerSide:
        protos_ext = "fopro-bin-server";
        break;
    case EngineSideKind::ClientSide:
        protos_ext = "fopro-bin-client";
        break;
    case EngineSideKind::MapperSide:
        protos_ext = "fopro-bin-mapper";
        break;
    }

    auto proto_files = resources.FilterFiles(protos_ext);

    for (const auto& proto_file_header : proto_files) {
        auto proto_file = File::Load(proto_file_header);
        auto reader = DataReader(proto_file.GetDataSpan());

        // Hashes
        {
            auto hashes_count = reader.Read<uint32_t>();
            string str;

            for (uint32_t i = 0; i < hashes_count; i++) {
                auto str_len = reader.Read<uint32_t>();
                str.resize(str_len);
                reader.ReadStringBytes(str);
                hstring hstr = _meta->Hashes.ToHashedString(str);
                ignore_unused(hstr);
            }
        }

        // Protos
        {
            auto types_count = reader.Read<uint32_t>();
            vector<uint8_t> props_data;

            for (uint32_t i = 0; i < types_count; i++) {
                auto protos_count = reader.Read<uint32_t>();

                auto type_name_len = reader.Read<uint16_t>();
                string type_name_str;
                type_name_str.resize(type_name_len);
                reader.ReadStringBytes(type_name_str);
                hstring type_name = _meta->Hashes.ToHashedString(type_name_str);

                FO_VERIFY_AND_THROW(_meta->IsValidEntityType(type_name) || _meta->IsFixedType(type_name), "Proto file references unknown entity or fixed type");

                for (uint32_t j = 0; j < protos_count; j++) {
                    auto proto_name_len = reader.Read<uint16_t>();
                    string proto_name;
                    proto_name.resize(proto_name_len);
                    reader.ReadStringBytes(proto_name);
                    hstring proto_id = _meta->Hashes.ToHashedString(proto_name);

                    auto proto = CreateProto(type_name, proto_id, nullptr);

                    auto data_size = reader.Read<uint32_t>();
                    props_data.resize(data_size);
                    span<uint8_t> props_data_span = props_data;
                    reader.ReadBytes(props_data_span);
                    proto->GetPropertiesForEdit()->RestoreAllData(props_data);
                }
            }
        }

        reader.VerifyEnd();
    }
}

auto ProtoManager::GetProtoItem(hstring proto_id) const noexcept -> nptr<const ProtoItem>
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _meta->CheckMigrationRule(_migrationRuleName, _itemTypeName, proto_id).value_or(proto_id);

    if (auto it = _itemProtos.find(proto_id); it != _itemProtos.end()) {
        return it->second;
    }

    return nullptr;
}

auto ProtoManager::GetProtoCritter(hstring proto_id) const noexcept -> nptr<const ProtoCritter>
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _meta->CheckMigrationRule(_migrationRuleName, _crTypeName, proto_id).value_or(proto_id);

    if (auto it = _crProtos.find(proto_id); it != _crProtos.end()) {
        return it->second;
    }

    return nullptr;
}

auto ProtoManager::GetProtoMap(hstring proto_id) const noexcept -> nptr<const ProtoMap>
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _meta->CheckMigrationRule(_migrationRuleName, _mapTypeName, proto_id).value_or(proto_id);

    if (auto it = _mapProtos.find(proto_id); it != _mapProtos.end()) {
        return it->second;
    }

    return nullptr;
}

auto ProtoManager::GetProtoLocation(hstring proto_id) const noexcept -> nptr<const ProtoLocation>
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _meta->CheckMigrationRule(_migrationRuleName, _locTypeName, proto_id).value_or(proto_id);

    if (auto it = _locProtos.find(proto_id); it != _locProtos.end()) {
        return it->second;
    }

    return nullptr;
}

auto ProtoManager::GetProtoEntity(hstring type_name, hstring proto_id) const noexcept -> nptr<const ProtoEntity>
{
    FO_STACK_TRACE_ENTRY();

    proto_id = _meta->CheckMigrationRule(_migrationRuleName, type_name, proto_id).value_or(proto_id);

    auto it_type = _protos.find(type_name);

    if (it_type == _protos.end()) {
        return nullptr;
    }

    if (auto it = it_type->second.find(proto_id); it != it_type->second.end()) {
        return it->second;
    }

    return nullptr;
}

auto ProtoManager::GetProtoEntities(hstring type_name) const noexcept -> const unordered_map<hstring, refcount_ptr<ProtoEntity>>&
{
    FO_STACK_TRACE_ENTRY();

    auto it_type = _protos.find(type_name);

    if (it_type == _protos.end()) {
        return _emptyProtos;
    }

    return it_type->second;
}

FO_END_NAMESPACE
