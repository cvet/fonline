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

#include "catch_amalgamated.hpp"

#include "Common.h"
#include "DataSerialization.h"
#include "EntityProtos.h"
#include "Properties.h"
#include "PropertiesSerializator.h"

FO_BEGIN_NAMESPACE

namespace
{
    class TestNameResolver final : public NameResolver
    {
    public:
        TestNameResolver()
        {
            auto make_int8 = []() {
                BaseTypeDesc type;
                type.Name = "int8";
                type.IsPrimitive = true;
                type.IsInt = true;
                type.IsSignedInt = true;
                type.IsInt8 = true;
                type.Size = sizeof(int8_t);
                return type;
            };

            auto make_int16 = []() {
                BaseTypeDesc type;
                type.Name = "int16";
                type.IsPrimitive = true;
                type.IsInt = true;
                type.IsSignedInt = true;
                type.IsInt16 = true;
                type.Size = sizeof(int16_t);
                return type;
            };

            auto make_int32 = []() {
                BaseTypeDesc type;
                type.Name = "int32";
                type.IsPrimitive = true;
                type.IsInt = true;
                type.IsSignedInt = true;
                type.IsInt32 = true;
                type.Size = sizeof(int32_t);
                return type;
            };

            auto make_int64 = []() {
                BaseTypeDesc type;
                type.Name = "int64";
                type.IsPrimitive = true;
                type.IsInt = true;
                type.IsSignedInt = true;
                type.IsInt64 = true;
                type.Size = sizeof(int64_t);
                return type;
            };

            auto make_uint8 = []() {
                BaseTypeDesc type;
                type.Name = "uint8";
                type.IsPrimitive = true;
                type.IsInt = true;
                type.IsSignedInt = false;
                type.IsUInt8 = true;
                type.Size = sizeof(uint8_t);
                return type;
            };

            auto make_uint16 = []() {
                BaseTypeDesc type;
                type.Name = "uint16";
                type.IsPrimitive = true;
                type.IsInt = true;
                type.IsSignedInt = false;
                type.IsUInt16 = true;
                type.Size = sizeof(uint16_t);
                return type;
            };

            auto make_uint32 = []() {
                BaseTypeDesc type;
                type.Name = "uint32";
                type.IsPrimitive = true;
                type.IsInt = true;
                type.IsSignedInt = false;
                type.IsUInt32 = true;
                type.Size = sizeof(uint32_t);
                return type;
            };

            auto make_bool = []() {
                BaseTypeDesc type;
                type.Name = "bool";
                type.IsPrimitive = true;
                type.IsBool = true;
                type.Size = sizeof(bool);
                return type;
            };

            auto make_float32 = []() {
                BaseTypeDesc type;
                type.Name = "float32";
                type.IsPrimitive = true;
                type.IsFloat = true;
                type.IsSingleFloat = true;
                type.Size = sizeof(float32_t);
                return type;
            };

            auto make_float64 = []() {
                BaseTypeDesc type;
                type.Name = "float64";
                type.IsPrimitive = true;
                type.IsFloat = true;
                type.IsDoubleFloat = true;
                type.Size = sizeof(float64_t);
                return type;
            };

            auto make_string = []() {
                BaseTypeDesc type;
                type.Name = "string";
                type.IsObject = true;
                type.IsString = true;
                return type;
            };

            auto make_hstring = []() {
                BaseTypeDesc type;
                type.Name = "hstring";
                type.IsObject = true;
                type.IsHashedString = true;
                type.Size = sizeof(hstring::hash_t);
                return type;
            };

            auto make_fixed_hash = []() {
                BaseTypeDesc type;
                type.Name = "FixedHash";
                type.IsObject = true;
                type.IsFixedType = true;
                type.IsHashedString = true;
                type.Size = sizeof(hstring::hash_t);
                return type;
            };

            auto make_proto = [](string_view name) {
                BaseTypeDesc type;
                type.Name = string(name);
                type.IsObject = true;
                type.IsEntity = true;
                type.IsEntityProto = true;
                type.Size = sizeof(hstring::hash_t);
                return type;
            };

            auto make_mode = [this]() {
                BaseTypeDesc type;
                type.Name = "Mode";
                type.IsEnum = true;
                type.IsPrimitive = true;
                type.IsInt = true;
                type.IsSignedInt = true;
                type.IsInt32 = true;
                type.Size = sizeof(int32_t);
                nptr<const BaseTypeDesc> int32_type = &_types.at("int32");
                type.EnumUnderlyingType = int32_type;
                return type;
            };

            auto make_waypoint = [this]() {
                StructLayoutDesc layout;

                auto& id_field = layout.Fields.emplace_back();
                id_field.Name = "Id";
                id_field.Type = _types.at("int32");
                id_field.Offset = layout.Size;
                layout.Size += id_field.Type.Size;

                auto& distance_field = layout.Fields.emplace_back();
                distance_field.Name = "Distance";
                distance_field.Type = _types.at("float32");
                distance_field.Offset = layout.Size;
                layout.Size += distance_field.Type.Size;

                auto& active_field = layout.Fields.emplace_back();
                active_field.Name = "Active";
                active_field.Type = _types.at("bool");
                active_field.Offset = layout.Size;
                layout.Size += active_field.Type.Size;

                _layouts.emplace("Waypoint", std::move(layout));

                BaseTypeDesc type;
                type.Name = "Waypoint";
                nptr<const StructLayoutDesc> waypoint_layout = &_layouts.at("Waypoint");
                type.StructLayout = waypoint_layout;
                type.Size = type.StructLayout->Size;
                type.IsComplexStruct = true;
                return type;
            };

            auto make_route_snapshot = [this]() {
                auto [registrator_it, inserted] = _ref_type_registrators.try_emplace("RouteSnapshot", "RouteSnapshotRefType", EngineSideKind::ServerSide, &_proto_hashes, this);
                FO_VERIFY_AND_THROW(inserted, "RouteSnapshot registrator already registered");

                PropertyRegistrator& fields_registrator = registrator_it->second;
                (void)fields_registrator.RegisterProperty({"Common", "int32[]", "Values"});
                (void)fields_registrator.RegisterProperty({"Common", "hstring[]", "Tags"});
                (void)fields_registrator.RegisterProperty({"Common", "Waypoint", "Anchor"});
                (void)fields_registrator.RegisterProperty({"Common", "string", "Note"});

                auto& ref_type = _ref_types["RouteSnapshot"];
                ref_type.FieldsRegistrator = &fields_registrator;
                ref_type.IsDynamicLayout = true;

                BaseTypeDesc type;
                type.Name = "RouteSnapshot";
                type.IsRefType = true;
                type.IsObject = true;
                nptr<const RefTypeDesc> nullable_ref_type = &_ref_types.at("RouteSnapshot");
                type.RefType = nullable_ref_type;
                return type;
            };

            auto make_route_envelope = [this]() {
                auto [registrator_it, inserted] = _ref_type_registrators.try_emplace("RouteEnvelope", "RouteEnvelopeRefType", EngineSideKind::ServerSide, &_proto_hashes, this);
                FO_VERIFY_AND_THROW(inserted, "RouteEnvelope registrator already registered");

                PropertyRegistrator& fields_registrator = registrator_it->second;
                (void)fields_registrator.RegisterProperty({"Common", "RouteSnapshot", "Primary"});
                (void)fields_registrator.RegisterProperty({"Common", "RouteSnapshot", "Backup"});
                (void)fields_registrator.RegisterProperty({"Common", "string", "Title"});

                auto& ref_type = _ref_types["RouteEnvelope"];
                ref_type.FieldsRegistrator = &fields_registrator;
                ref_type.IsDynamicLayout = true;

                BaseTypeDesc type;
                type.Name = "RouteEnvelope";
                type.IsRefType = true;
                type.IsObject = true;
                nptr<const RefTypeDesc> nullable_ref_type = &_ref_types.at("RouteEnvelope");
                type.RefType = nullable_ref_type;
                return type;
            };

            _types.emplace("int8", make_int8());
            _types.emplace("int16", make_int16());
            _types.emplace("int32", make_int32());
            _types.emplace("int64", make_int64());
            _types.emplace("uint8", make_uint8());
            _types.emplace("uint16", make_uint16());
            _types.emplace("uint32", make_uint32());
            _types.emplace("bool", make_bool());
            _types.emplace("float32", make_float32());
            _types.emplace("float64", make_float64());
            _types.emplace("string", make_string());
            _types.emplace("hstring", make_hstring());
            _types.emplace("FixedHash", make_fixed_hash());
            _types.emplace("ProtoItem", make_proto("ProtoItem"));
            _types.emplace("ProtoCritter", make_proto("ProtoCritter"));
            _types.emplace("ProtoMap", make_proto("ProtoMap"));
            _types.emplace("ProtoLocation", make_proto("ProtoLocation"));
            _types.emplace("Mode", make_mode());
            _types.emplace("Waypoint", make_waypoint());
            _types.emplace("RouteSnapshot", make_route_snapshot());
            _types.emplace("RouteEnvelope", make_route_envelope());

            _types.at("ProtoItem").HashedName = _proto_hashes.ToHashedString("ProtoItem");
            _types.at("ProtoCritter").HashedName = _proto_hashes.ToHashedString("ProtoCritter");
            _types.at("ProtoMap").HashedName = _proto_hashes.ToHashedString("ProtoMap");
            _types.at("ProtoLocation").HashedName = _proto_hashes.ToHashedString("ProtoLocation");

            _enum_values.emplace("ModeA", 1);
            _enum_values.emplace("ModeB", 2);
            _enum_names.emplace(1, "ModeA");
            _enum_names.emplace(2, "ModeB");

            _proto_registrator.emplace("ProtoResolverEntity", EngineSideKind::ServerSide, &_proto_hashes, this);

            AddProto("ProtoItem", "knife");
            AddProto("ProtoItem", "pistol");
            AddProto("ProtoMap", "wasteland");
        }

        [[nodiscard]] auto GetBaseType(string_view type_str) const -> const BaseTypeDesc& override
        {
            if (const auto it = _types.find(string(type_str)); it != _types.end()) {
                return it->second;
            }

            throw TypeResolveException("Unknown test type", string(type_str));
        }

        [[nodiscard]] auto ResolveComplexType(string_view type_str) const -> ComplexTypeDesc override
        {
            ComplexTypeDesc type;

            if (const auto dict_pos = type_str.find("=>"); dict_pos != string_view::npos) {
                const auto key_type = type_str.substr(0, dict_pos);
                const auto value_type = type_str.substr(dict_pos + 2);

                type.KeyType = GetBaseType(key_type);

                if (value_type.size() > 2 && value_type.substr(value_type.size() - 2) == "[]") {
                    type.Kind = ComplexTypeKind::DictOfArray;
                    type.BaseType = GetBaseType(value_type.substr(0, value_type.size() - 2));
                }
                else {
                    type.Kind = ComplexTypeKind::Dict;
                    type.BaseType = GetBaseType(value_type);
                }
            }
            else if (type_str.size() > 2 && type_str.substr(type_str.size() - 2) == "[]") {
                type.Kind = ComplexTypeKind::Array;
                type.BaseType = GetBaseType(type_str.substr(0, type_str.size() - 2));
            }
            else {
                type.Kind = ComplexTypeKind::Simple;
                type.BaseType = GetBaseType(type_str);
            }

            return type;
        }

        [[nodiscard]] auto ResolveEnumValue(string_view value_name, nptr<bool> failed = nullptr) const -> int32_t override
        {
            if (const auto it = _enum_values.find(string(value_name)); it != _enum_values.end()) {
                if (failed) {
                    *failed = false;
                }

                return it->second;
            }

            if (failed) {
                *failed = true;
                return 0;
            }

            throw EnumResolveException("Enum value is not supported in test resolver");
        }

        [[nodiscard]] auto ResolveEnumValue(string_view, string_view value_name, nptr<bool> failed = nullptr) const -> int32_t override { return ResolveEnumValue(value_name, failed); }

        [[nodiscard]] auto ResolveEnumValueName(string_view, int32_t enum_value, nptr<bool> failed = nullptr) const -> string_view override
        {
            if (const auto it = _enum_names.find(enum_value); it != _enum_names.end()) {
                if (failed) {
                    *failed = false;
                }

                return it->second;
            }

            if (failed) {
                *failed = true;
                return _empty;
            }

            throw EnumResolveException("Enum name is not supported in test resolver");
        }

        void AddMigrationRule(hstring rule_name, hstring extra_info, hstring target, hstring replacement) { _migration_rules[rule_name][extra_info][target] = replacement; }

        [[nodiscard]] auto CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const noexcept -> optional<hstring> override
        {
            if (const auto it = _migration_rules.find(rule_name); it != _migration_rules.end()) {
                if (const auto it2 = it->second.find(extra_info); it2 != it->second.end()) {
                    if (const auto it3 = it2->second.find(target); it3 != it2->second.end()) {
                        return it3->second;
                    }
                }
            }

            return std::nullopt;
        }
        [[nodiscard]] auto GetProtoEntity(hstring type_name, hstring proto_id) const noexcept -> nptr<const ProtoEntity> override
        {
            const auto type_it = _protos.find(type_name.as_hash());

            if (type_it == _protos.end()) {
                return nullptr;
            }

            const auto proto_it = type_it->second.find(proto_id.as_hash());
            if (proto_it != type_it->second.end()) {
                return proto_it->second;
            }

            return nullptr;
        }

        void AddProto(string_view type_name, string_view proto_id)
        {
            const auto type_hname = _proto_hashes.ToHashedString(type_name);
            const auto proto_hname = _proto_hashes.ToHashedString(proto_id);
            FO_VERIFY_AND_THROW(_proto_registrator.has_value(), "Proto registrator not initialized");
            ptr<const PropertyRegistrator> proto_registrator = &*_proto_registrator;

            _protos[type_hname.as_hash()].emplace(proto_hname.as_hash(), SafeAlloc::MakeRefCounted<ProtoCustomEntity>(proto_hname, proto_registrator, nullptr));
        }

    private:
        unordered_map<string, BaseTypeDesc> _types {};
        unordered_map<string, StructLayoutDesc> _layouts {};
        unordered_map<string, RefTypeDesc> _ref_types {};
        unordered_map<string, int32_t> _enum_values {};
        unordered_map<int32_t, string> _enum_names {};
        unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>> _migration_rules {};
        HashStorage _proto_hashes {};
        optional<PropertyRegistrator> _proto_registrator {};
        unordered_map<string, PropertyRegistrator> _ref_type_registrators {};
        unordered_map<hstring::hash_t, unordered_map<hstring::hash_t, refcount_ptr<ProtoCustomEntity>>> _protos {};
        string _empty {};
    };

    auto MakeOwnedStoreData(const Properties::StoredData& stored_data) -> vector<vector<uint8_t>>
    {
        const auto& raw_data = *stored_data.Data;
        const auto& raw_sizes = *stored_data.Sizes;

        vector<vector<uint8_t>> result;
        result.reserve(raw_data.size());

        for (size_t i = 0; i < raw_data.size(); i++) {
            const auto nullable_data = raw_data[i];
            const auto size = raw_sizes[i];

            if (size == 0) {
                result.emplace_back();
            }
            else {
                REQUIRE(nullable_data);
                auto data = nullable_data.as_ptr();
                const auto data_span = make_const_span(data, size);
                result.emplace_back(data_span.begin(), data_span.end());
            }
        }

        return result;
    }

    [[nodiscard]] auto MakeRawUInt16(uint16_t value) -> vector<uint8_t>
    {
        vector<uint8_t> result(sizeof(value));
        MemCopy(result.data(), &value, sizeof(value));
        return result;
    }

    [[nodiscard]] auto MakeRawInt32(int32_t value) -> vector<uint8_t>
    {
        vector<uint8_t> result(sizeof(value));
        MemCopy(result.data(), &value, sizeof(value));
        return result;
    }

    enum class PerfPropertyKind
    {
        PublicInt,
        PublicString,
        OwnerBool,
        PublicFloat,
        LocalInt,
    };

    struct PerfPropertySpec
    {
        nptr<const Property> Prop {};
        PerfPropertyKind Kind {};
        int Index {};
    };

    struct PropertiesStorageStrategyPerfFixture
    {
        HashStorage Hashes {};
        TestNameResolver Resolver {};
        PropertyRegistrator Registrator;
        vector<PerfPropertySpec> Props {};
        vector<PerfPropertySpec> OverrideProps {};
        nptr<const Property> ProbeIntProp {};
        nptr<const Property> ProbeStringProp {};
        Properties Proto;
        Properties PackedSource;
        Properties FullSource;
        vector<vector<uint8_t>> PackedPublicChunks {};
        vector<vector<uint8_t>> FullPublicChunks {};
        vector<uint8_t> PackedAllData {};
        vector<uint8_t> FullAllData {};
        size_t PackedPublicBytes {};
        size_t FullPublicBytes {};
        int TotalProps {};
        int FillPercent {};

        PropertiesStorageStrategyPerfFixture(int total_props, int fill_percent) :
            Registrator("PerfStorageEntity", EngineSideKind::ServerSide, &Hashes, &Resolver),
            Proto(&Registrator),
            PackedSource(&Registrator, &Proto),
            FullSource(&Registrator),
            TotalProps(total_props),
            FillPercent(fill_percent)
        {
            FO_VERIFY_AND_THROW(total_props > 0, "Total properties must be positive");
            FO_VERIFY_AND_THROW(fill_percent > 0, "Fill percent must be positive");

            Props.reserve(numeric_cast<size_t>(total_props));

            for (int i = 0; i < total_props; i++) {
                const auto kind = static_cast<PerfPropertyKind>(i % 5);
                auto prop = RegisterProperty(kind, i);

                Props.emplace_back(PerfPropertySpec {prop, kind, i});

                if (!ProbeIntProp && kind == PerfPropertyKind::PublicInt) {
                    ProbeIntProp = prop;
                }
                if (!ProbeStringProp && kind == PerfPropertyKind::PublicString) {
                    ProbeStringProp = prop;
                }
            }

            FO_VERIFY_AND_THROW(ProbeIntProp != nullptr, "No public int property was registered for probing");
            FO_VERIFY_AND_THROW(ProbeStringProp != nullptr, "No public string property was registered for probing");

            Proto.AllocData();
            FullSource.AllocData();

            for (const auto& spec : Props) {
                SetProtoValue(Proto, spec);
            }

            FullSource.CopyFrom(Proto);

            const auto override_count = std::max(1, total_props * fill_percent / 100);
            OverrideProps.reserve(numeric_cast<size_t>(override_count));

            for (int i = 0; i < override_count; i++) {
                const auto& spec = Props[numeric_cast<size_t>(i)];
                OverrideProps.emplace_back(spec);
                SetOverrideValue(PackedSource, spec);
                SetOverrideValue(FullSource, spec);
            }

            const auto packed_public_data = PackedSource.StoreData(false);
            PackedPublicChunks = MakeOwnedStoreData(packed_public_data);
            PackedPublicBytes = CalcTotalBytes(PackedPublicChunks);

            const auto full_public_data = FullSource.StoreData(false);
            FullPublicChunks = MakeOwnedStoreData(full_public_data);
            FullPublicBytes = CalcTotalBytes(FullPublicChunks);

            set<hstring> str_hashes;
            PackedSource.StoreAllData(PackedAllData, str_hashes);
            FullSource.StoreAllData(FullAllData, str_hashes);
        }

        void ApplyOverrides(Properties& props) const
        {
            for (const auto& spec : OverrideProps) {
                SetOverrideValue(props, spec);
            }
        }

        [[nodiscard]] auto GetProbeIntProp() const -> ptr<const Property>
        {
            FO_VERIFY_AND_THROW(ProbeIntProp, "Probe int property not registered");
            return ProbeIntProp.as_ptr();
        }

        [[nodiscard]] auto GetProbeStringProp() const -> ptr<const Property>
        {
            FO_VERIFY_AND_THROW(ProbeStringProp, "Probe string property not registered");
            return ProbeStringProp.as_ptr();
        }

        [[nodiscard]] static auto CalcTotalBytes(const vector<vector<uint8_t>>& chunks) -> size_t
        {
            size_t total = 0;

            for (const auto& chunk : chunks) {
                total += chunk.size();
            }

            return total;
        }

        [[nodiscard]] auto TryBuildTextData(const Properties& props, map<string, string>& text_data, size_t& text_chars, string& error) -> bool
        {
            try {
                text_data = props.SaveToText(nullptr);
                text_chars = 0;

                for (const auto& [name, value] : text_data) {
                    text_chars += name.size();
                    text_chars += value.size();
                }

                error.clear();
                return true;
            }
            catch (const std::exception& ex) {
                error = ex.what();
                return false;
            }
        }

        [[nodiscard]] auto DiagnoseTextSerializationFailure(const Properties& props) -> string
        {
            for (size_t i = 1; i < Registrator.GetPropertiesCount(); i++) {
                auto prop = Registrator.GetPropertyByIndex(numeric_cast<int32_t>(i));

                if (prop->IsDisabled() || !prop->IsPersistent()) {
                    continue;
                }

                try {
                    const auto text = PropertiesSerializator::SavePropertyToText(&props, prop.get(), Hashes, Resolver);
                    static_cast<void>(text);
                }
                catch (const std::exception& ex) {
                    return strex("{} ({})", prop->GetName(), ex.what()).str();
                }
            }

            return {};
        }

    private:
        auto RegisterProperty(PerfPropertyKind kind, int index) -> ptr<const Property>
        {
            string prop_name;

            switch (kind) {
            case PerfPropertyKind::PublicInt:
                prop_name = strex("PublicValue{}", index);
                return Registrator.RegisterProperty({"Common", "int32", prop_name, "Mutable", "Persistent", "PublicSync"});
            case PerfPropertyKind::PublicString:
                prop_name = strex("PublicName{}", index);
                return Registrator.RegisterProperty({"Common", "string", prop_name, "Mutable", "Persistent", "PublicSync"});
            case PerfPropertyKind::OwnerBool:
                prop_name = strex("OwnerFlag{}", index);
                return Registrator.RegisterProperty({"Common", "bool", prop_name, "Mutable", "Persistent", "OwnerSync"});
            case PerfPropertyKind::PublicFloat:
                prop_name = strex("PublicRatio{}", index);
                return Registrator.RegisterProperty({"Common", "float32", prop_name, "Mutable", "Persistent", "PublicSync"});
            case PerfPropertyKind::LocalInt:
                prop_name = strex("LocalMarker{}", index);
                return Registrator.RegisterProperty({"Common", "int32", prop_name, "Mutable", "Persistent", "NoSync"});
            }

            throw std::runtime_error("Unknown perf property kind");
        }

        static void SetProtoValue(Properties& props, const PerfPropertySpec& spec)
        {
            auto prop = GetSpecProp(spec);

            switch (spec.Kind) {
            case PerfPropertyKind::PublicInt:
                props.SetValue<int32_t>(prop, numeric_cast<int32_t>(spec.Index * 2));
                break;
            case PerfPropertyKind::PublicString:
                props.SetValue<string>(prop, strex("proto-{}-squad", spec.Index));
                break;
            case PerfPropertyKind::OwnerBool:
                props.SetValue<bool>(prop, spec.Index % 2 == 0);
                break;
            case PerfPropertyKind::PublicFloat:
                props.SetValue<float32_t>(prop, static_cast<float32_t>(spec.Index) * 0.25f);
                break;
            case PerfPropertyKind::LocalInt:
                props.SetValue<int32_t>(prop, numeric_cast<int32_t>(spec.Index % 100));
                break;
            }
        }

        static void SetOverrideValue(Properties& props, const PerfPropertySpec& spec)
        {
            auto prop = GetSpecProp(spec);

            switch (spec.Kind) {
            case PerfPropertyKind::PublicInt:
                props.SetValue<int32_t>(prop, numeric_cast<int32_t>(1000 + spec.Index * 3));
                break;
            case PerfPropertyKind::PublicString:
                props.SetValue<string>(prop, strex("override-{}-payload", spec.Index));
                break;
            case PerfPropertyKind::OwnerBool:
                props.SetValue<bool>(prop, spec.Index % 3 == 0);
                break;
            case PerfPropertyKind::PublicFloat:
                props.SetValue<float32_t>(prop, 10.0f + static_cast<float32_t>(spec.Index) * 0.5f);
                break;
            case PerfPropertyKind::LocalInt:
                props.SetValue<int32_t>(prop, numeric_cast<int32_t>((spec.Index * 7) % 300));
                break;
            }
        }

        static auto GetSpecProp(const PerfPropertySpec& spec) -> ptr<const Property>
        {
            FO_VERIFY_AND_THROW(spec.Prop, "Perf property spec has no property");
            return spec.Prop.as_ptr();
        }
    };

    struct PropertiesComplexStrategyPerfFixture
    {
        HashStorage Hashes {};
        TestNameResolver Resolver {};
        PropertyRegistrator Registrator;
        nptr<const Property> PatrolWaypointsProp {};
        nptr<const Property> ModeSetsProp {};
        string PatrolProtoText {};
        string PatrolOverrideTextA {};
        string PatrolOverrideTextB {};
        string ModeProtoText {};
        string ModeOverrideTextA {};
        string ModeOverrideTextB {};
        Properties Proto;
        Properties PackedSource;
        Properties FullSource;

        PropertiesComplexStrategyPerfFixture() :
            Registrator("PerfComplexStorageEntity", EngineSideKind::ServerSide, &Hashes, &Resolver),
            PatrolWaypointsProp(Registrator.RegisterProperty({"Common", "int32=>Waypoint[]", "PatrolWaypoints", "Mutable", "Persistent", "PublicSync"})),
            ModeSetsProp(Registrator.RegisterProperty({"Common", "string=>Mode[]", "ModeSets", "Mutable", "Persistent", "PublicSync"})),
            Proto(&Registrator),
            PackedSource(&Registrator, &Proto),
            FullSource(&Registrator)
        {
            PatrolProtoText = MakePatrolValue(1, 2);
            PatrolOverrideTextA = MakePatrolValue(10, 20);
            PatrolOverrideTextB = MakePatrolValue(30, 40);
            ModeProtoText = MakeModeSetsValue("primary", "reserve");
            ModeOverrideTextA = MakeModeSetsValue("alpha", "beta");
            ModeOverrideTextB = MakeModeSetsValue("gamma", "delta");

            Proto.SetValueAsAnyProps(PatrolWaypointsProp->GetRegIndex(), any_t {PatrolProtoText});
            Proto.SetValueAsAnyProps(ModeSetsProp->GetRegIndex(), any_t {ModeProtoText});

            FullSource.CopyFrom(Proto);

            PackedSource.SetValueAsAnyProps(PatrolWaypointsProp->GetRegIndex(), any_t {PatrolOverrideTextA});
            PackedSource.SetValueAsAnyProps(ModeSetsProp->GetRegIndex(), any_t {ModeOverrideTextA});
            FullSource.SetValueAsAnyProps(PatrolWaypointsProp->GetRegIndex(), any_t {PatrolOverrideTextA});
            FullSource.SetValueAsAnyProps(ModeSetsProp->GetRegIndex(), any_t {ModeOverrideTextA});
        }

        [[nodiscard]] auto GetPatrolWaypointsProp() const -> ptr<const Property>
        {
            FO_VERIFY_AND_THROW(PatrolWaypointsProp, "Patrol waypoints property not registered");
            return PatrolWaypointsProp.as_ptr();
        }

        [[nodiscard]] auto GetModeSetsProp() const -> ptr<const Property>
        {
            FO_VERIFY_AND_THROW(ModeSetsProp, "Mode sets property not registered");
            return ModeSetsProp.as_ptr();
        }

    private:
        static auto MakePatrolValue(int first_base, int second_base) -> string
        {
            AnyData::Array first_path;

            AnyData::Array first_a;
            first_a.EmplaceBack(int64_t {first_base});
            first_a.EmplaceBack(float64_t {0.5});
            first_a.EmplaceBack(true);
            first_path.EmplaceBack(AnyData::Value {std::move(first_a)});

            AnyData::Array first_b;
            first_b.EmplaceBack(int64_t {first_base + 1});
            first_b.EmplaceBack(float64_t {1.25});
            first_b.EmplaceBack(false);
            first_path.EmplaceBack(AnyData::Value {std::move(first_b)});

            AnyData::Array second_path;

            AnyData::Array second_a;
            second_a.EmplaceBack(int64_t {second_base});
            second_a.EmplaceBack(float64_t {2.5});
            second_a.EmplaceBack(true);
            second_path.EmplaceBack(AnyData::Value {std::move(second_a)});

            AnyData::Dict patrols;
            patrols.Emplace("1", AnyData::Value {std::move(first_path)});
            patrols.Emplace("2", AnyData::Value {std::move(second_path)});

            return AnyData::ValueToString(AnyData::Value {std::move(patrols)});
        }

        static auto MakeModeSetsValue(string_view primary_name, string_view reserve_name) -> string
        {
            AnyData::Array primary_modes;
            primary_modes.EmplaceBack(string {"ModeA"});
            primary_modes.EmplaceBack(string {"ModeB"});

            AnyData::Array reserve_modes;
            reserve_modes.EmplaceBack(string {"ModeB"});
            reserve_modes.EmplaceBack(string {"ModeA"});

            AnyData::Dict mode_sets;
            mode_sets.Emplace(string {primary_name}, AnyData::Value {std::move(primary_modes)});
            mode_sets.Emplace(string {reserve_name}, AnyData::Value {std::move(reserve_modes)});

            return AnyData::ValueToString(AnyData::Value {std::move(mode_sets)});
        }
    };

    struct PropertiesPerfFixture
    {
        HashStorage Hashes {};
        TestNameResolver Resolver {};
        PropertyRegistrator Registrator;
        vector<ptr<const Property>> PublicIntProps {};
        vector<ptr<const Property>> PublicStringProps {};
        vector<ptr<const Property>> OwnerBoolProps {};
        Properties Proto;
        Properties Full;
        Properties DerivedSource;
        vector<vector<uint8_t>> DerivedPublicChunks {};
        vector<uint8_t> FullAllData {};
        vector<uint8_t> DerivedAllData {};

        PropertiesPerfFixture() :
            Registrator("PerfEntity", EngineSideKind::ServerSide, &Hashes, &Resolver),
            Proto(&Registrator),
            Full(&Registrator),
            DerivedSource(&Registrator, &Proto)
        {
            PublicIntProps.reserve(48);
            PublicStringProps.reserve(12);
            OwnerBoolProps.reserve(8);

            for (int i = 0; i < 48; i++) {
                string prop_name {"Value"};
                prop_name += std::to_string(i).c_str();
                const array<string_view, 6> tokens {"Common", "int32", prop_name, "Mutable", "Persistent", "PublicSync"};
                PublicIntProps.emplace_back(Registrator.RegisterProperty(tokens));
            }

            for (int i = 0; i < 12; i++) {
                string prop_name {"Name"};
                prop_name += std::to_string(i).c_str();
                const array<string_view, 6> tokens {"Common", "string", prop_name, "Mutable", "Persistent", "PublicSync"};
                PublicStringProps.emplace_back(Registrator.RegisterProperty(tokens));
            }

            for (int i = 0; i < 8; i++) {
                string prop_name {"OwnerFlag"};
                prop_name += std::to_string(i).c_str();
                const array<string_view, 6> tokens {"Common", "bool", prop_name, "Mutable", "Persistent", "OwnerSync"};
                OwnerBoolProps.emplace_back(Registrator.RegisterProperty(tokens));
            }

            for (size_t i = 0; i < PublicIntProps.size(); i++) {
                Proto.SetValue<int32_t>(PublicIntProps[i], numeric_cast<int32_t>(i));
                Full.SetValue<int32_t>(PublicIntProps[i], numeric_cast<int32_t>(i * 3 + 1));
            }

            for (size_t i = 0; i < PublicStringProps.size(); i++) {
                string proto_value {"proto-"};
                proto_value += std::to_string(i).c_str();
                string full_value {"full-"};
                full_value += std::to_string(i).c_str();
                Proto.SetValue<string>(PublicStringProps[i], proto_value);
                Full.SetValue<string>(PublicStringProps[i], full_value);
            }

            for (size_t i = 0; i < OwnerBoolProps.size(); i++) {
                Proto.SetValue<bool>(OwnerBoolProps[i], false);
                Full.SetValue<bool>(OwnerBoolProps[i], i % 2 == 0);
            }

            for (size_t i = 0; i < PublicIntProps.size(); i++) {
                DerivedSource.SetValue<int32_t>(PublicIntProps[i], numeric_cast<int32_t>(100 + i));
            }

            for (size_t i = 0; i < PublicStringProps.size(); i++) {
                string override_value {"derived-"};
                override_value += std::to_string(i).c_str();
                DerivedSource.SetValue<string>(PublicStringProps[i], override_value);
            }

            for (size_t i = 0; i < OwnerBoolProps.size(); i++) {
                DerivedSource.SetValue<bool>(OwnerBoolProps[i], i % 3 == 0);
            }

            const auto derived_public_data = DerivedSource.StoreData(false);
            DerivedPublicChunks = MakeOwnedStoreData(derived_public_data);

            (void)Full.StoreData(false);

            set<hstring> str_hashes;
            Full.StoreAllData(FullAllData, str_hashes);
            DerivedSource.StoreAllData(DerivedAllData, str_hashes);
        }
    };

    struct PropertiesDictPerfFixture
    {
        HashStorage Hashes {};
        TestNameResolver Resolver {};
        PropertyRegistrator Registrator;
        nptr<const Property> FloatLabelsProp {};
        nptr<const Property> PatrolWaypointsProp {};
        nptr<const Property> ModeSetsProp {};
        Properties Source;
        Properties PatrolOnlySource;
        map<string, string> TextData {};
        optional<AnyData::Value> PatrolWaypointsValue {};

        PropertiesDictPerfFixture() :
            Registrator("PerfDictEntity", EngineSideKind::ServerSide, &Hashes, &Resolver),
            FloatLabelsProp(Registrator.RegisterProperty({"Common", "float32=>string", "Labels", "Mutable", "Persistent", "PublicSync"})),
            PatrolWaypointsProp(Registrator.RegisterProperty({"Common", "int32=>Waypoint[]", "PatrolWaypoints", "Mutable", "Persistent", "PublicSync"})),
            ModeSetsProp(Registrator.RegisterProperty({"Common", "string=>Mode[]", "ModeSets", "Mutable", "Persistent", "PublicSync"})),
            Source(&Registrator),
            PatrolOnlySource(&Registrator)
        {
            const auto labels_value = []() {
                AnyData::Dict labels;
                labels.Emplace("-0.5", string {"low"});
                labels.Emplace("1.25", string {"high tide"});
                labels.Emplace("3.75", string {"fallback route"});
                return AnyData::Value {std::move(labels)};
            }();

            const auto patrol_value = []() {
                AnyData::Array first_path;

                AnyData::Array first_path_point_a;
                first_path_point_a.EmplaceBack(int64_t {1});
                first_path_point_a.EmplaceBack(float64_t {0.5});
                first_path_point_a.EmplaceBack(true);
                first_path.EmplaceBack(AnyData::Value {std::move(first_path_point_a)});

                AnyData::Array first_path_point_b;
                first_path_point_b.EmplaceBack(int64_t {2});
                first_path_point_b.EmplaceBack(float64_t {1.25});
                first_path_point_b.EmplaceBack(false);
                first_path.EmplaceBack(AnyData::Value {std::move(first_path_point_b)});

                AnyData::Array second_path;

                AnyData::Array second_path_point_a;
                second_path_point_a.EmplaceBack(int64_t {7});
                second_path_point_a.EmplaceBack(float64_t {4.75});
                second_path_point_a.EmplaceBack(true);
                second_path.EmplaceBack(AnyData::Value {std::move(second_path_point_a)});

                AnyData::Array second_path_point_b;
                second_path_point_b.EmplaceBack(int64_t {8});
                second_path_point_b.EmplaceBack(float64_t {9.5});
                second_path_point_b.EmplaceBack(false);
                second_path.EmplaceBack(AnyData::Value {std::move(second_path_point_b)});

                AnyData::Dict patrols;
                patrols.Emplace("1", AnyData::Value {std::move(first_path)});
                patrols.Emplace("2", AnyData::Value {std::move(second_path)});
                return AnyData::Value {std::move(patrols)};
            }();

            const auto mode_sets_value = []() {
                AnyData::Array primary_modes;
                primary_modes.EmplaceBack(string {"ModeA"});
                primary_modes.EmplaceBack(string {"ModeB"});

                AnyData::Array reserve_modes;
                reserve_modes.EmplaceBack(string {"ModeB"});
                reserve_modes.EmplaceBack(string {"ModeA"});

                AnyData::Dict mode_sets;
                mode_sets.Emplace("primary", AnyData::Value {std::move(primary_modes)});
                mode_sets.Emplace("reserve squad", AnyData::Value {std::move(reserve_modes)});
                return AnyData::Value {std::move(mode_sets)};
            }();

            Source.SetValueAsAnyProps(FloatLabelsProp->GetRegIndex(), any_t {AnyData::ValueToString(labels_value)});
            Source.SetValueAsAnyProps(PatrolWaypointsProp->GetRegIndex(), any_t {AnyData::ValueToString(patrol_value)});
            Source.SetValueAsAnyProps(ModeSetsProp->GetRegIndex(), any_t {AnyData::ValueToString(mode_sets_value)});

            PatrolOnlySource.SetValueAsAnyProps(PatrolWaypointsProp->GetRegIndex(), any_t {AnyData::ValueToString(patrol_value)});

            TextData = Source.SaveToText(nullptr);
            PatrolWaypointsValue.emplace(PropertiesSerializator::SavePropertyToValue(&Source, GetPatrolWaypointsProp(), Hashes, Resolver));
        }

        [[nodiscard]] auto GetFloatLabelsProp() const -> ptr<const Property>
        {
            FO_VERIFY_AND_THROW(FloatLabelsProp, "Float labels property not registered");
            return FloatLabelsProp.as_ptr();
        }

        [[nodiscard]] auto GetPatrolWaypointsProp() const -> ptr<const Property>
        {
            FO_VERIFY_AND_THROW(PatrolWaypointsProp, "Patrol waypoints property not registered");
            return PatrolWaypointsProp.as_ptr();
        }

        [[nodiscard]] auto GetModeSetsProp() const -> ptr<const Property>
        {
            FO_VERIFY_AND_THROW(ModeSetsProp, "Mode sets property not registered");
            return ModeSetsProp.as_ptr();
        }

        [[nodiscard]] auto GetPatrolWaypointsValue() const -> ptr<const AnyData::Value>
        {
            FO_VERIFY_AND_THROW(PatrolWaypointsValue.has_value(), "Patrol waypoints value not computed");
            return &*PatrolWaypointsValue;
        }
    };
}

TEST_CASE("PropertiesOverlay")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("TestEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto value_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    auto flag_prop = registrator.RegisterProperty({"Common", "bool", "Flag", "Mutable", "Persistent", "PublicSync"});
    auto name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});

    Properties proto(&registrator);
    proto.SetValue<int32_t>(value_prop, 10);
    proto.SetValue<bool>(flag_prop, true);
    proto.SetValue<string>(name_prop, "proto-name");

    SECTION("ReadsInheritedValuesAndDropsMatchingOverride")
    {
        Properties props(&registrator, &proto);

        CHECK(props.HasBaseProperties());
        CHECK(props.GetValue<int32_t>(value_prop) == 10);
        CHECK(props.GetValue<bool>(flag_prop));
        CHECK(props.GetValue<string>(name_prop) == "proto-name");

        props.SetValue<int32_t>(value_prop, 25);
        CHECK(props.GetValue<int32_t>(value_prop) == 25);
        CHECK(proto.GetValue<int32_t>(value_prop) == 10);

        props.SetValue<int32_t>(value_prop, 10);
        CHECK(props.GetValue<int32_t>(value_prop) == 10);

        props.SetValue<string>(name_prop, "proto-name");
        CHECK(props.GetValue<string>(name_prop) == "proto-name");
        CHECK(props.SaveToText(nptr<const Properties>(&proto)).empty());
    }

    SECTION("StoresAndRestoresSparseOverrides")
    {
        Properties props(&registrator, &proto);
        props.SetValue<int32_t>(value_prop, 42);
        props.SetValue<bool>(flag_prop, false);
        props.SetValue<string>(name_prop, "");

        auto diff = props.SaveToText(nptr<const Properties>(&proto));
        CHECK(diff.contains("Value"));
        CHECK(diff.contains("Flag"));
        CHECK(diff.contains("Name"));

        const auto stored_data = props.StoreData(false);
        auto owned_chunks = MakeOwnedStoreData(stored_data);

        Properties restored(&registrator, &proto);
        restored.RestoreData(owned_chunks);

        CHECK(restored.GetValue<int32_t>(value_prop) == 42);
        CHECK_FALSE(restored.GetValue<bool>(flag_prop));
        CHECK(restored.GetValue<string>(name_prop).empty());

        auto copied = props.Copy();
        CHECK(copied.HasBaseProperties());
        CHECK(copied.GetValue<int32_t>(value_prop) == 42);
        CHECK_FALSE(copied.GetValue<bool>(flag_prop));
        CHECK(copied.GetValue<string>(name_prop).empty());

        Properties snapshot(&registrator);
        snapshot.CopyFrom(props);
        snapshot.CopyFrom(props);
        CHECK(snapshot.GetValue<int32_t>(value_prop) == 42);
        CHECK_FALSE(snapshot.GetValue<bool>(flag_prop));
        CHECK(snapshot.GetValue<string>(name_prop).empty());

        props.SetValue<string>(name_prop, "mutated-source");
        CHECK(snapshot.GetValue<string>(name_prop).empty());
    }

    SECTION("CopyFromFullSourceBuildsCanonicalDerivedOverlay")
    {
        Properties source(&registrator);
        source.SetValue<int32_t>(value_prop, 42);
        source.SetValue<bool>(flag_prop, true);
        source.SetValue<string>(name_prop, "full-name");

        Properties derived(&registrator, &proto);
        derived.CopyFrom(source);
        derived.CopyFrom(source);

        CHECK(derived.GetValue<int32_t>(value_prop) == 42);
        CHECK(derived.GetValue<bool>(flag_prop));
        CHECK(derived.GetValue<string>(name_prop) == "full-name");

        vector<uint8_t> all_data;
        set<hstring> str_hashes;
        derived.StoreAllData(all_data, str_hashes);

        DataReader reader(all_data);
        CHECK(reader.Read<uint32_t>() == registrator.GetWholeDataSize());
        CHECK(reader.Read<bool>());
        CHECK(reader.Read<uint32_t>() == 2);
    }

    SECTION("StoreAllDataUsesOverlayBaseBackedData")
    {
        Properties props(&registrator, &proto);
        props.SetValue<int32_t>(value_prop, 77);
        props.SetValue<bool>(flag_prop, false);
        props.SetValue<string>(name_prop, "override-name");

        vector<uint8_t> all_data;
        set<hstring> str_hashes;
        props.StoreAllData(all_data, str_hashes);

        DataReader reader(all_data);
        CHECK(reader.Read<uint32_t>() == registrator.GetWholeDataSize());
        CHECK(reader.Read<bool>());

        CHECK(reader.Read<uint32_t>() == 3);

        const auto expected_size = sizeof(uint32_t) + sizeof(bool) + sizeof(uint32_t) + 3 * (sizeof(uint16_t) + sizeof(uint32_t)) + props.GetRawData(value_prop).size() + props.GetRawData(flag_prop).size() + props.GetRawData(name_prop).size();
        CHECK(all_data.size() == expected_size);

        Properties restored(&registrator, &proto);
        restored.RestoreAllData(all_data);

        CHECK(restored.GetValue<int32_t>(value_prop) == 77);
        CHECK_FALSE(restored.GetValue<bool>(flag_prop));
        CHECK(restored.GetValue<string>(name_prop) == "override-name");
    }

    SECTION("RepeatedPlainWritesKeepCanonicalOverlayState")
    {
        Properties props(&registrator, &proto);

        props.SetValue<bool>(flag_prop, false);
        props.SetValue<bool>(flag_prop, false);
        props.SetValue<int32_t>(value_prop, 77);
        props.SetValue<int32_t>(value_prop, 77);

        vector<uint8_t> all_data;
        set<hstring> str_hashes;
        props.StoreAllData(all_data, str_hashes);

        DataReader reader(all_data);
        CHECK(reader.Read<uint32_t>() == registrator.GetWholeDataSize());
        CHECK(reader.Read<bool>());
        CHECK(reader.Read<uint32_t>() == 2);

        props.SetValue<bool>(flag_prop, true);
        props.SetValue<bool>(flag_prop, true);
        props.SetValue<int32_t>(value_prop, 10);
        props.SetValue<int32_t>(value_prop, 10);

        props.StoreAllData(all_data, str_hashes);

        DataReader empty_reader(all_data);
        CHECK(empty_reader.Read<uint32_t>() == registrator.GetWholeDataSize());
        CHECK(empty_reader.Read<bool>());
        CHECK(empty_reader.Read<uint32_t>() == 0);
        empty_reader.VerifyEnd();
    }

    SECTION("RestoreAllDataRejectsMismatchedStorageMode")
    {
        vector<uint8_t> full_data;
        set<hstring> str_hashes;
        proto.StoreAllData(full_data, str_hashes);
        Properties derived(&registrator, &proto);
        CHECK_THROWS(derived.RestoreAllData(full_data));

        Properties derived_data(&registrator, &proto);
        derived_data.SetValue<int32_t>(value_prop, 55);

        vector<uint8_t> diff_data;
        derived_data.StoreAllData(diff_data, str_hashes);

        Properties full_props(&registrator);
        CHECK_THROWS(full_props.RestoreAllData(diff_data));
    }
}

TEST_CASE("PropertiesRawDataCopy")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("RawCopyEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto value_prop = registrator.RegisterProperty({"Common", "uint16", "Value", "Mutable", "Persistent", "PublicSync"});
    auto flag_prop = registrator.RegisterProperty({"Common", "bool", "Flag", "Mutable", "Persistent", "PublicSync"});
    auto name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);
    const string first_value(4096, 'A');
    const string second_value(8192, 'B');

    SECTION("CopyOwnsStableBuffer")
    {
        props.SetValue<string>(name_prop, first_value);

        PropertyRawData copied_data;
        props.CopyRawData(name_prop, copied_data);

        props.SetValue<string>(name_prop, second_value);

        REQUIRE(copied_data.GetSize() == first_value.size());
        auto copied_chars = copied_data.GetPtrAs<char>();

        for (size_t i = 0; i < copied_data.GetSize(); i++) {
            CHECK(copied_chars[i] == 'A');
        }
    }

    SECTION("PassedRawDataCanBeMaterialized")
    {
        auto raw_value = MakeRawUInt16(0x1234);

        PropertyRawData passed_data;
        passed_data.Pass(raw_value);
        CHECK(passed_data.GetSize() == raw_value.size());

        passed_data.StoreIfPassed();
        raw_value[0] = 0;
        raw_value[1] = 0;

        CHECK(passed_data.GetAs<uint16_t>() == 0x1234);
    }

    SECTION("RawDataSizeAndOverlaySetRawDataPaths")
    {
        props.SetValue<uint16_t>(value_prop, 0x1234);
        props.SetValue<bool>(flag_prop, true);
        props.SetValue<string>(name_prop, "");

        Properties derived(&registrator, &props);
        const auto string_bytes = [](string_view value) -> span<const uint8_t> { return {reinterpret_cast<const uint8_t*>(value.data()), value.size()}; };

        CHECK(props.GetRawDataSize(value_prop) == sizeof(uint16_t));
        CHECK(props.GetRawDataSize(flag_prop) == sizeof(bool));
        CHECK(props.GetRawDataSize(name_prop) == 0);
        CHECK(derived.GetRawDataSize(value_prop) == sizeof(uint16_t));
        CHECK(derived.GetRawDataSize(name_prop) == 0);

        PropertyRawData fallback_name;
        derived.CopyRawData(name_prop, fallback_name);
        CHECK(fallback_name.GetSize() == 0);

        const auto same_value = MakeRawUInt16(0x1234);
        derived.SetRawData(value_prop, same_value);
        CHECK(derived.GetValue<uint16_t>(value_prop) == 0x1234);

        const bool same_flag_value = true;
        derived.SetRawData(flag_prop, {reinterpret_cast<const uint8_t*>(&same_flag_value), sizeof(same_flag_value)});
        CHECK(derived.GetValue<bool>(flag_prop));

        derived.SetRawData(name_prop, {});
        CHECK(derived.GetValue<string>(name_prop).empty());

        const auto override_value = MakeRawUInt16(0x2222);
        derived.SetRawData(value_prop, override_value);
        CHECK(derived.GetValue<uint16_t>(value_prop) == 0x2222);
        CHECK(derived.GetRawDataSize(value_prop) == sizeof(uint16_t));

        derived.SetRawData(value_prop, override_value);
        CHECK(derived.GetValue<uint16_t>(value_prop) == 0x2222);

        const auto replacement_value = MakeRawUInt16(0x3333);
        derived.SetRawData(value_prop, replacement_value);
        CHECK(derived.GetValue<uint16_t>(value_prop) == 0x3333);

        const string overlay_name = "raw";
        derived.SetRawData(name_prop, string_bytes(overlay_name));
        CHECK(derived.GetValue<string>(name_prop) == overlay_name);
        CHECK(derived.GetRawDataSize(name_prop) == overlay_name.size());

        PropertyRawData overlay_name_copy;
        derived.CopyRawData(name_prop, overlay_name_copy);
        REQUIRE(overlay_name_copy.GetSize() == overlay_name.size());
        CHECK(string_view(overlay_name_copy.GetPtrAs<char>().get(), overlay_name_copy.GetSize()) == overlay_name);

        derived.SetRawData(name_prop, string_bytes(overlay_name));
        CHECK(derived.GetValue<string>(name_prop) == overlay_name);

        derived.SetRawData(name_prop, {});
        CHECK(derived.GetValue<string>(name_prop).empty());
        CHECK(derived.GetRawDataSize(name_prop) == 0);
    }

    SECTION("ConcurrentCopyDoesNotTear")
    {
        props.SetValue<string>(name_prop, first_value);

        std::atomic_bool writer_done {false};
        std::atomic_bool torn_copy {false};

        std::thread writer([&]() {
            for (int32_t i = 0; i < 10000; i++) {
                props.SetValue<string>(name_prop, i % 2 == 0 ? first_value : second_value);
            }

            writer_done.store(true, std::memory_order_release);
        });

        while (!writer_done.load(std::memory_order_acquire)) {
            PropertyRawData copied_data;
            props.CopyRawData(name_prop, copied_data);

            const size_t size = copied_data.GetSize();
            const bool first_size = size == first_value.size();
            const bool second_size = size == second_value.size();

            if (!first_size && !second_size) {
                torn_copy.store(true, std::memory_order_relaxed);
                break;
            }

            const char expected = first_size ? 'A' : 'B';
            auto copied_chars = copied_data.GetPtrAs<char>();

            for (size_t i = 0; i < size; i++) {
                if (copied_chars[i] != expected) {
                    torn_copy.store(true, std::memory_order_relaxed);
                    break;
                }
            }

            if (torn_copy.load(std::memory_order_relaxed)) {
                break;
            }
        }

        writer.join();
        CHECK_FALSE(torn_copy.load(std::memory_order_relaxed));
    }
}

TEST_CASE("PropertiesOverlayFiltersAndCopies")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("SyncEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto public_value_prop = registrator.RegisterProperty({"Common", "int32", "PublicValue", "Mutable", "Persistent", "PublicSync"});
    auto owner_flag_prop = registrator.RegisterProperty({"Common", "bool", "OwnerFlag", "Mutable", "Persistent", "OwnerSync"});
    auto public_name_prop = registrator.RegisterProperty({"Common", "string", "PublicName", "Mutable", "Persistent", "PublicSync"});

    Properties proto(&registrator);
    proto.SetValue<int32_t>(public_value_prop, 10);
    proto.SetValue<bool>(owner_flag_prop, true);
    proto.SetValue<string>(public_name_prop, "base");

    SECTION("StoreDataPublicOnlySkipsOwnerSyncOverrides")
    {
        Properties props(&registrator, &proto);
        props.SetValue<int32_t>(public_value_prop, 42);
        props.SetValue<bool>(owner_flag_prop, false);
        props.SetValue<string>(public_name_prop, "public-override");

        const auto stored_data = props.StoreData(false);
        const auto& raw_data = *stored_data.Data;
        const auto& raw_sizes = *stored_data.Sizes;

        REQUIRE(raw_data.size() == 4);
        REQUIRE(raw_sizes.size() == 4);
        CHECK(raw_sizes[0] == sizeof(uint8_t));
        CHECK(raw_sizes[1] == sizeof(uint16_t) * 2);

        uint8_t store_type = 0xFF;
        REQUIRE(raw_data.at(0));
        auto store_type_data = raw_data.at(0).as_ptr();
        MemCopy(&store_type, store_type_data, sizeof(store_type));
        CHECK(store_type == 1);

        auto owned_chunks = MakeOwnedStoreData(stored_data);

        Properties restored(&registrator, &proto);
        restored.RestoreData(owned_chunks);

        CHECK(restored.GetValue<int32_t>(public_value_prop) == 42);
        CHECK(restored.GetValue<bool>(owner_flag_prop));
        CHECK(restored.GetValue<string>(public_name_prop) == "public-override");

        Properties restored_full(&registrator);
        restored_full.CopyFrom(proto);
        restored_full.RestoreData(owned_chunks);

        CHECK(restored_full.GetValue<int32_t>(public_value_prop) == 42);
        CHECK(restored_full.GetValue<bool>(owner_flag_prop));
        CHECK(restored_full.GetValue<string>(public_name_prop) == "public-override");
    }

    SECTION("StoreDataWithProtectedKeepsOwnerSyncOverrides")
    {
        Properties props(&registrator, &proto);
        props.SetValue<int32_t>(public_value_prop, 42);
        props.SetValue<bool>(owner_flag_prop, false);
        props.SetValue<string>(public_name_prop, "public-override");

        const auto stored_data = props.StoreData(true);
        const auto& raw_data = *stored_data.Data;
        const auto& raw_sizes = *stored_data.Sizes;

        REQUIRE(raw_data.size() == 5);
        REQUIRE(raw_sizes.size() == 5);
        CHECK(raw_sizes[0] == sizeof(uint8_t));
        CHECK(raw_sizes[1] == sizeof(uint16_t) * 3);

        uint8_t store_type = 0xFF;
        REQUIRE(raw_data.at(0));
        auto store_type_data = raw_data.at(0).as_ptr();
        MemCopy(&store_type, store_type_data, sizeof(store_type));
        CHECK(store_type == 1);

        auto owned_chunks = MakeOwnedStoreData(stored_data);

        Properties restored(&registrator, &proto);
        restored.RestoreData(owned_chunks);

        CHECK(restored.GetValue<int32_t>(public_value_prop) == 42);
        CHECK_FALSE(restored.GetValue<bool>(owner_flag_prop));
        CHECK(restored.GetValue<string>(public_name_prop) == "public-override");

        Properties restored_full(&registrator);
        restored_full.CopyFrom(proto);
        restored_full.RestoreData(owned_chunks);

        CHECK(restored_full.GetValue<int32_t>(public_value_prop) == 42);
        CHECK_FALSE(restored_full.GetValue<bool>(owner_flag_prop));
        CHECK(restored_full.GetValue<string>(public_name_prop) == "public-override");
    }

    SECTION("StoreDataPreservesZeroSizedOverlayEntry")
    {
        Properties props(&registrator, &proto);
        props.SetValue<string>(public_name_prop, "");

        const auto stored_data = props.StoreData(false);
        const auto& raw_data = *stored_data.Data;
        const auto& raw_sizes = *stored_data.Sizes;

        REQUIRE(raw_data.size() == 3);
        REQUIRE(raw_sizes.size() == 3);
        CHECK(raw_sizes[0] == sizeof(uint8_t));
        CHECK(raw_sizes[1] == sizeof(uint16_t));
        CHECK(raw_sizes[2] == 0);

        uint8_t store_type = 0xFF;
        REQUIRE(raw_data.at(0));
        auto store_type_data = raw_data.at(0).as_ptr();
        MemCopy(&store_type, store_type_data, sizeof(store_type));
        CHECK(store_type == 1);

        auto owned_chunks = MakeOwnedStoreData(stored_data);

        Properties restored(&registrator, &proto);
        restored.RestoreData(owned_chunks);

        CHECK(restored.GetValue<string>(public_name_prop).empty());
    }

    SECTION("StoreDataKeepsSeparateCachesPerProtectionMode")
    {
        Properties props(&registrator, &proto);
        props.SetValue<int32_t>(public_value_prop, 42);
        props.SetValue<bool>(owner_flag_prop, false);
        props.SetValue<string>(public_name_prop, "public-override");

        const auto public_data = props.StoreData(false);
        const auto protected_data = props.StoreData(true);
        const auto public_data_again = props.StoreData(false);

        CHECK(public_data.Data != protected_data.Data);
        CHECK(public_data.Sizes != protected_data.Sizes);
        CHECK(public_data.Data == public_data_again.Data);
        CHECK(public_data.Sizes == public_data_again.Sizes);

        auto public_chunks = MakeOwnedStoreData(public_data);
        auto protected_chunks = MakeOwnedStoreData(protected_data);

        Properties restored_public(&registrator, &proto);
        restored_public.RestoreData(public_chunks);
        CHECK(restored_public.GetValue<int32_t>(public_value_prop) == 42);
        CHECK(restored_public.GetValue<bool>(owner_flag_prop));
        CHECK(restored_public.GetValue<string>(public_name_prop) == "public-override");

        Properties restored_protected(&registrator, &proto);
        restored_protected.RestoreData(protected_chunks);
        CHECK(restored_protected.GetValue<int32_t>(public_value_prop) == 42);
        CHECK_FALSE(restored_protected.GetValue<bool>(owner_flag_prop));
        CHECK(restored_protected.GetValue<string>(public_name_prop) == "public-override");
    }

    SECTION("StoreDataCacheInvalidatesAfterMutationAndRestore")
    {
        Properties derived(&registrator, &proto);
        derived.SetValue<int32_t>(public_value_prop, 42);

        const auto derived_before_data = derived.StoreData(false);
        auto derived_before_chunks = MakeOwnedStoreData(derived_before_data);

        derived.SetValue<string>(public_name_prop, "changed");

        const auto derived_after_data = derived.StoreData(false);
        auto derived_after_chunks = MakeOwnedStoreData(derived_after_data);

        Properties restored_before(&registrator, &proto);
        restored_before.RestoreData(derived_before_chunks);
        CHECK(restored_before.GetValue<int32_t>(public_value_prop) == 42);
        CHECK(restored_before.GetValue<string>(public_name_prop) == "base");

        Properties restored_after(&registrator, &proto);
        restored_after.RestoreData(derived_after_chunks);
        CHECK(restored_after.GetValue<int32_t>(public_value_prop) == 42);
        CHECK(restored_after.GetValue<string>(public_name_prop) == "changed");

        Properties full_target(&registrator);
        full_target.SetValue<int32_t>(public_value_prop, 10);
        full_target.SetValue<bool>(owner_flag_prop, true);
        full_target.SetValue<string>(public_name_prop, "base");

        (void)full_target.StoreData(false);

        Properties full_source(&registrator);
        full_source.SetValue<int32_t>(public_value_prop, 77);
        full_source.SetValue<bool>(owner_flag_prop, false);
        full_source.SetValue<string>(public_name_prop, "restored");

        const auto full_source_data = full_source.StoreData(false);
        auto full_source_chunks = MakeOwnedStoreData(full_source_data);

        full_target.RestoreData(full_source_chunks);

        const auto full_after_data = full_target.StoreData(false);
        auto full_after_chunks = MakeOwnedStoreData(full_after_data);

        Properties full_restored(&registrator);
        full_restored.RestoreData(full_after_chunks);
        CHECK(full_restored.GetValue<int32_t>(public_value_prop) == 77);
        CHECK_FALSE(full_restored.GetValue<bool>(owner_flag_prop));
        CHECK(full_restored.GetValue<string>(public_name_prop) == "restored");
    }

    SECTION("StoreDataCacheInvalidatesOnOwnerOnlyMutation")
    {
        Properties props(&registrator);
        props.SetValue<int32_t>(public_value_prop, 42);
        props.SetValue<bool>(owner_flag_prop, false);
        props.SetValue<string>(public_name_prop, "public-override");

        const auto public_before_data = props.StoreData(false);
        const auto public_before_chunks = MakeOwnedStoreData(public_before_data);

        const auto protected_before_data = props.StoreData(true);
        const auto protected_before_chunks = MakeOwnedStoreData(protected_before_data);

        props.SetValue<bool>(owner_flag_prop, true);

        const auto public_after_data = props.StoreData(false);
        const auto public_after_chunks = MakeOwnedStoreData(public_after_data);

        const auto protected_after_data = props.StoreData(true);
        const auto protected_after_chunks = MakeOwnedStoreData(protected_after_data);

        Properties restored_public_before(&registrator);
        restored_public_before.RestoreData(public_before_chunks);
        CHECK(restored_public_before.GetValue<int32_t>(public_value_prop) == 42);
        CHECK_FALSE(restored_public_before.GetValue<bool>(owner_flag_prop));
        CHECK(restored_public_before.GetValue<string>(public_name_prop) == "public-override");

        Properties restored_public_after(&registrator);
        restored_public_after.RestoreData(public_after_chunks);
        CHECK(restored_public_after.GetValue<int32_t>(public_value_prop) == 42);
        CHECK_FALSE(restored_public_after.GetValue<bool>(owner_flag_prop));
        CHECK(restored_public_after.GetValue<string>(public_name_prop) == "public-override");

        Properties restored_protected_before(&registrator);
        restored_protected_before.RestoreData(protected_before_chunks);
        CHECK(restored_protected_before.GetValue<int32_t>(public_value_prop) == 42);
        CHECK_FALSE(restored_protected_before.GetValue<bool>(owner_flag_prop));
        CHECK(restored_protected_before.GetValue<string>(public_name_prop) == "public-override");

        Properties restored_protected_after(&registrator);
        restored_protected_after.RestoreData(protected_after_chunks);
        CHECK(restored_protected_after.GetValue<int32_t>(public_value_prop) == 42);
        CHECK(restored_protected_after.GetValue<bool>(owner_flag_prop));
        CHECK(restored_protected_after.GetValue<string>(public_name_prop) == "public-override");
    }

    SECTION("StoreAllDataHandlesEmptyOverlay")
    {
        Properties props(&registrator, &proto);

        vector<uint8_t> all_data;
        set<hstring> str_hashes;
        props.StoreAllData(all_data, str_hashes);

        DataReader reader(all_data);
        CHECK(reader.Read<uint32_t>() == registrator.GetWholeDataSize());
        CHECK(reader.Read<bool>());
        CHECK(reader.Read<uint32_t>() == 0);
        reader.VerifyEnd();

        Properties restored(&registrator, &proto);
        restored.RestoreAllData(all_data);

        CHECK(restored.GetValue<int32_t>(public_value_prop) == 10);
        CHECK(restored.GetValue<bool>(owner_flag_prop));
        CHECK(restored.GetValue<string>(public_name_prop) == "base");
    }

    SECTION("CopyFromRebasesInheritedDerivedValuesToAnotherBase")
    {
        Properties source_base(&registrator);
        source_base.SetValue<int32_t>(public_value_prop, 5);
        source_base.SetValue<bool>(owner_flag_prop, false);
        source_base.SetValue<string>(public_name_prop, "source");

        Properties target_base(&registrator);
        target_base.SetValue<int32_t>(public_value_prop, 100);
        target_base.SetValue<bool>(owner_flag_prop, true);
        target_base.SetValue<string>(public_name_prop, "target");

        Properties source_props(&registrator, &source_base);
        source_props.SetValue<int32_t>(public_value_prop, 77);

        Properties target_props(&registrator, &target_base);
        target_props.CopyFrom(source_props);
        target_props.CopyFrom(source_props);

        CHECK(target_props.GetValue<int32_t>(public_value_prop) == 77);
        CHECK_FALSE(target_props.GetValue<bool>(owner_flag_prop));
        CHECK(target_props.GetValue<string>(public_name_prop) == "source");

        vector<uint8_t> all_data;
        set<hstring> str_hashes;
        target_props.StoreAllData(all_data, str_hashes);

        DataReader reader(all_data);
        CHECK(reader.Read<uint32_t>() == registrator.GetWholeDataSize());
        CHECK(reader.Read<bool>());
        CHECK(reader.Read<uint32_t>() == 3);
    }

    SECTION("CopyFromRebasesDerivedValuesToAnotherBase")
    {
        Properties source_base(&registrator);
        source_base.SetValue<int32_t>(public_value_prop, 5);
        source_base.SetValue<bool>(owner_flag_prop, false);
        source_base.SetValue<string>(public_name_prop, "source");

        Properties target_base(&registrator);
        target_base.SetValue<int32_t>(public_value_prop, 100);
        target_base.SetValue<bool>(owner_flag_prop, true);
        target_base.SetValue<string>(public_name_prop, "target");

        Properties source_props(&registrator, &source_base);
        source_props.SetValue<int32_t>(public_value_prop, 77);
        source_props.SetValue<bool>(owner_flag_prop, true);
        source_props.SetValue<string>(public_name_prop, "custom");

        Properties target_props(&registrator, &target_base);
        target_props.CopyFrom(source_props);
        target_props.CopyFrom(source_props);

        CHECK(target_props.GetValue<int32_t>(public_value_prop) == 77);
        CHECK(target_props.GetValue<bool>(owner_flag_prop));
        CHECK(target_props.GetValue<string>(public_name_prop) == "custom");

        vector<uint8_t> all_data;
        set<hstring> str_hashes;
        target_props.StoreAllData(all_data, str_hashes);

        DataReader reader(all_data);
        CHECK(reader.Read<uint32_t>() == registrator.GetWholeDataSize());
        CHECK(reader.Read<bool>());
        CHECK(reader.Read<uint32_t>() == 2);
    }
}

TEST_CASE("PropertiesFullStorageRoundTrip")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("FullEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto value_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    auto flag_prop = registrator.RegisterProperty({"Common", "bool", "Flag", "Mutable", "Persistent", "PublicSync"});
    auto name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);
    props.SetValue<int32_t>(value_prop, 123);
    props.SetValue<bool>(flag_prop, true);
    props.SetValue<string>(name_prop, "full-roundtrip");

    vector<uint8_t> all_data;
    set<hstring> str_hashes;
    props.StoreAllData(all_data, str_hashes);

    Properties restored(&registrator);
    restored.RestoreAllData(all_data);

    CHECK(restored.GetValue<int32_t>(value_prop) == 123);
    CHECK(restored.GetValue<bool>(flag_prop));
    CHECK(restored.GetValue<string>(name_prop) == "full-roundtrip");

    auto copied = props.Copy();
    CHECK(copied.GetValue<int32_t>(value_prop) == 123);
    CHECK(copied.GetValue<bool>(flag_prop));
    CHECK(copied.GetValue<string>(name_prop) == "full-roundtrip");

    props.SetValue<string>(name_prop, "changed-after-copy");
    CHECK(copied.GetValue<string>(name_prop) == "full-roundtrip");

    SECTION("StoreAllDataPreservesZeroSizedString")
    {
        Properties empty_string_props(&registrator);
        empty_string_props.SetValue<int32_t>(value_prop, 123);
        empty_string_props.SetValue<bool>(flag_prop, true);
        empty_string_props.SetValue<string>(name_prop, "");

        vector<uint8_t> empty_string_data;
        set<hstring> empty_string_hashes;
        empty_string_props.StoreAllData(empty_string_data, empty_string_hashes);

        Properties restored_empty_string(&registrator);
        restored_empty_string.RestoreAllData(empty_string_data);

        CHECK(restored_empty_string.GetValue<int32_t>(value_prop) == 123);
        CHECK(restored_empty_string.GetValue<bool>(flag_prop));
        CHECK(restored_empty_string.GetValue<string>(name_prop).empty());
    }
}

TEST_CASE("PropertiesFullStorageCopyFrom")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("FullCopyEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto value_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    auto flag_prop = registrator.RegisterProperty({"Common", "bool", "Flag", "Mutable", "Persistent", "PublicSync"});
    auto name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});

    Properties source(&registrator);
    source.SetValue<int32_t>(value_prop, 321);
    source.SetValue<bool>(flag_prop, true);
    source.SetValue<string>(name_prop, "copy-source");

    Properties target(&registrator);
    target.SetValue<int32_t>(value_prop, 111);
    target.SetValue<bool>(flag_prop, false);
    target.SetValue<string>(name_prop, "copy-target");

    target.CopyFrom(source);

    CHECK(target.GetValue<int32_t>(value_prop) == 321);
    CHECK(target.GetValue<bool>(flag_prop));
    CHECK(target.GetValue<string>(name_prop) == "copy-source");

    source.SetValue<string>(name_prop, "source-mutated");
    CHECK(target.GetValue<string>(name_prop) == "copy-source");
}

TEST_CASE("PropertiesOverlayPreservesUnsyncedLocalOverridesOnRestore")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("ClientLocalEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto synced_value_prop = registrator.RegisterProperty({"Common", "int32", "SyncedValue", "Mutable", "Persistent", "PublicSync"});
    auto local_value_prop = registrator.RegisterProperty({"Common", "int32", "LocalValue", "Mutable", "Persistent", "NoSync"});

    Properties proto(&registrator);
    proto.SetValue<int32_t>(synced_value_prop, 10);
    proto.SetValue<int32_t>(local_value_prop, 0);

    SECTION("EmptyRestoreKeepsUnsyncedOverride")
    {
        Properties props(&registrator, &proto);
        props.SetValue<int32_t>(local_value_prop, 1);

        vector<vector<uint8_t>> empty_data;
        props.RestoreData(empty_data);

        CHECK(props.GetValue<int32_t>(synced_value_prop) == 10);
        CHECK(props.GetValue<int32_t>(local_value_prop) == 1);
    }

    SECTION("NetworkRestoreKeepsUnsyncedOverride")
    {
        Properties server_state(&registrator, &proto);
        server_state.SetValue<int32_t>(synced_value_prop, 42);

        const auto stored_data = server_state.StoreData(false);
        auto owned_chunks = MakeOwnedStoreData(stored_data);

        Properties client_state(&registrator, &proto);
        client_state.SetValue<int32_t>(local_value_prop, 1);
        client_state.RestoreData(owned_chunks);

        CHECK(client_state.GetValue<int32_t>(synced_value_prop) == 42);
        CHECK(client_state.GetValue<int32_t>(local_value_prop) == 1);
    }

    SECTION("EmptyRestoreClearsSyncedOverrideButKeepsUnsyncedOverride")
    {
        Properties client_state(&registrator, &proto);
        client_state.SetValue<int32_t>(synced_value_prop, 99);
        client_state.SetValue<int32_t>(local_value_prop, 1);

        vector<vector<uint8_t>> empty_data;
        client_state.RestoreData(empty_data);

        CHECK(client_state.GetValue<int32_t>(synced_value_prop) == 10);
        CHECK(client_state.GetValue<int32_t>(local_value_prop) == 1);
    }

    SECTION("RestoreAllDataEmptySnapshotClearsUnsyncedOverride")
    {
        Properties server_snapshot(&registrator, &proto);
        vector<uint8_t> all_data;
        set<hstring> str_hashes;
        server_snapshot.StoreAllData(all_data, str_hashes);

        Properties client_state(&registrator, &proto);
        client_state.SetValue<int32_t>(synced_value_prop, 99);
        client_state.SetValue<int32_t>(local_value_prop, 1);
        client_state.RestoreAllData(all_data);

        CHECK(client_state.GetValue<int32_t>(synced_value_prop) == 10);
        CHECK(client_state.GetValue<int32_t>(local_value_prop) == 0);
    }

    SECTION("RestoreAllDataRestoresUnsyncedOverrideFromSnapshot")
    {
        Properties server_snapshot(&registrator, &proto);
        server_snapshot.SetValue<int32_t>(synced_value_prop, 42);
        server_snapshot.SetValue<int32_t>(local_value_prop, 7);

        vector<uint8_t> all_data;
        set<hstring> str_hashes;
        server_snapshot.StoreAllData(all_data, str_hashes);

        Properties client_state(&registrator, &proto);
        client_state.SetValue<int32_t>(synced_value_prop, 99);
        client_state.SetValue<int32_t>(local_value_prop, 1);
        client_state.RestoreAllData(all_data);

        CHECK(client_state.GetValue<int32_t>(synced_value_prop) == 42);
        CHECK(client_state.GetValue<int32_t>(local_value_prop) == 7);
    }
}

TEST_CASE("PropertiesRestoreDataRejectsMalformedPayloads")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("MalformedRestoreEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto value_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    auto name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);
    props.SetValue<int32_t>(value_prop, 10);
    props.SetValue<string>(name_prop, "valid");

    const uint8_t full_store_type = 0;
    const uint8_t separate_store_type = 1;
    const uint8_t invalid_store_type = 0xFE;
    const int32_t payload_value = 42;

    SECTION("PointerAndSizeListsMustMatch")
    {
        vector<nptr<const uint8_t>> payload {&full_store_type};
        vector<uint32_t> sizes;

        CHECK_THROWS(props.RestoreData(payload, sizes));
    }

    SECTION("StoreTypeMarkerMustBeOneByte")
    {
        vector<nptr<const uint8_t>> payload {&full_store_type};
        vector<uint32_t> sizes {numeric_cast<uint32_t>(sizeof(uint16_t))};

        CHECK_THROWS(props.RestoreData(payload, sizes));
    }

    SECTION("StoreTypeMustBeKnown")
    {
        vector<nptr<const uint8_t>> payload {&invalid_store_type};
        vector<uint32_t> sizes {numeric_cast<uint32_t>(sizeof(invalid_store_type))};

        CHECK_THROWS(props.RestoreData(payload, sizes));
    }

    SECTION("SeparateIndexTableMustBeAligned")
    {
        const array<uint8_t, 1> misaligned_index_table {0};
        vector<nptr<const uint8_t>> payload {&separate_store_type, misaligned_index_table.data()};
        vector<uint32_t> sizes {numeric_cast<uint32_t>(sizeof(separate_store_type)), numeric_cast<uint32_t>(misaligned_index_table.size())};

        CHECK_THROWS(props.RestoreData(payload, sizes));
    }

    SECTION("SeparatePayloadCountMustMatchIndexTable")
    {
        const auto index_table = MakeRawUInt16(value_prop->GetRegIndex());
        vector<nptr<const uint8_t>> payload {&separate_store_type, index_table.data()};
        vector<uint32_t> sizes {numeric_cast<uint32_t>(sizeof(separate_store_type)), numeric_cast<uint32_t>(index_table.size())};

        CHECK_THROWS(props.RestoreData(payload, sizes));
    }

    SECTION("SeparateIndexMustPointAtARealProperty")
    {
        const auto payload_value_data = MakeRawInt32(payload_value);
        const auto zero_index_table = MakeRawUInt16(0);
        vector<nptr<const uint8_t>> zero_payload {&separate_store_type, zero_index_table.data(), payload_value_data.data()};
        vector<uint32_t> zero_sizes {numeric_cast<uint32_t>(sizeof(separate_store_type)), numeric_cast<uint32_t>(zero_index_table.size()), numeric_cast<uint32_t>(payload_value_data.size())};

        CHECK_THROWS(props.RestoreData(zero_payload, zero_sizes));

        const auto out_of_bounds_index_table = MakeRawUInt16(999);
        vector<nptr<const uint8_t>> out_of_bounds_payload {&separate_store_type, out_of_bounds_index_table.data(), payload_value_data.data()};
        vector<uint32_t> out_of_bounds_sizes {numeric_cast<uint32_t>(sizeof(separate_store_type)), numeric_cast<uint32_t>(out_of_bounds_index_table.size()), numeric_cast<uint32_t>(payload_value_data.size())};

        CHECK_THROWS(props.RestoreData(out_of_bounds_payload, out_of_bounds_sizes));
    }

    SECTION("FullPayloadMustContainPodData")
    {
        vector<nptr<const uint8_t>> payload {&full_store_type};
        vector<uint32_t> sizes {numeric_cast<uint32_t>(sizeof(full_store_type))};

        CHECK_THROWS(props.RestoreData(payload, sizes));
    }

    SECTION("FullPodDataSizeMustMatchASectionBoundary")
    {
        const array<uint8_t, 1> pod_data {0};
        vector<nptr<const uint8_t>> payload {&full_store_type, pod_data.data()};
        vector<uint32_t> sizes {numeric_cast<uint32_t>(sizeof(full_store_type)), numeric_cast<uint32_t>(pod_data.size())};

        CHECK_THROWS(props.RestoreData(payload, sizes));
    }

    SECTION("FullComplexIndexTableMustContainValidComplexProperties")
    {
        const auto stored_data = props.StoreData(false);
        auto chunks = MakeOwnedStoreData(stored_data);
        REQUIRE(chunks.size() == 4);

        auto zero_entry_chunks = chunks;
        zero_entry_chunks[2].clear();
        CHECK_THROWS(props.RestoreData(zero_entry_chunks));

        auto zero_index_chunks = chunks;
        zero_index_chunks[2] = MakeRawUInt16(0);
        CHECK_THROWS(props.RestoreData(zero_index_chunks));

        auto plain_index_chunks = chunks;
        plain_index_chunks[2] = MakeRawUInt16(value_prop->GetRegIndex());
        CHECK_THROWS(props.RestoreData(plain_index_chunks));

        auto out_of_bounds_chunks = chunks;
        out_of_bounds_chunks[2] = MakeRawUInt16(999);
        CHECK_THROWS(props.RestoreData(out_of_bounds_chunks));
    }
}

TEST_CASE("PropertiesRestoreAllDataRejectsOutOfBoundsPodSection")
{
    // RestoreAllData's full-data POD records carry (start_pos, len) offsets straight from the blob. A
    // corrupted or hostile snapshot whose layout-size header still matches the registrator must not be
    // able to drive an out-of-bounds MemCopy into _podData — the offsets have to be validated against the
    // POD layout. (The sibling RestoreData(ptrs, sizes) path already size-checks its single POD block;
    // this is the matching guard for the sparse start_pos/len record format.)
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("OobPodRestoreEntity", EngineSideKind::ServerSide, &hashes, &resolver);
    registrator.RegisterProperty({"Common", "int32", "A", "Mutable", "Persistent", "PublicSync"});
    registrator.RegisterProperty({"Common", "int32", "B", "Mutable", "Persistent", "PublicSync"});

    const auto whole = numeric_cast<uint32_t>(registrator.GetWholeDataSize());

    vector<uint8_t> blob;
    const auto append_u32 = [&blob](uint32_t value) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        blob.insert(blob.end(), bytes, bytes + sizeof(value));
    };

    append_u32(whole); // layout-size header — matches the registrator, so the top-level guard passes
    blob.push_back(uint8_t {0}); // bool: full-data (no overlay) storage mode
    append_u32(whole); // start_pos: exactly at the end of _podData — out of bounds
    append_u32(32U); // len: 32 bytes copied from the buffer end, well past the allocation
    blob.insert(blob.end(), 32, uint8_t {0}); // the matching in-bounds source payload
    append_u32(0U); // POD record list terminator (start_pos, len) == (0, 0)
    append_u32(0U);
    append_u32(0U); // complex property count (this registrator has none)

    Properties restored(&registrator);
    CHECK_THROWS(restored.RestoreAllData(blob));
}

TEST_CASE("PropertiesCompareData")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("CompareEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto value_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    auto flag_prop = registrator.RegisterProperty({"Common", "bool", "Flag", "Mutable", "Persistent", "PublicSync"});
    auto name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});
    auto temp_prop = registrator.RegisterProperty({"Common", "int32", "Temp", "Mutable", "NoSync"});

    SECTION("FullStorageFastPath")
    {
        Properties left(&registrator);
        left.SetValue<int32_t>(value_prop, 10);
        left.SetValue<bool>(flag_prop, true);
        left.SetValue<string>(name_prop, "same");

        Properties right(&registrator);
        right.SetValue<int32_t>(value_prop, 10);
        right.SetValue<bool>(flag_prop, true);
        right.SetValue<string>(name_prop, "same");

        CHECK(left.CompareData(right, {}, false));

        right.SetValue<string>(name_prop, "diff");
        CHECK_FALSE(left.CompareData(right, {}, false));
    }

    SECTION("DerivedSameBaseFastPath")
    {
        Properties proto(&registrator);
        proto.SetValue<int32_t>(value_prop, 1);
        proto.SetValue<bool>(flag_prop, false);
        proto.SetValue<string>(name_prop, "proto");

        Properties left(&registrator, &proto);
        left.SetValue<int32_t>(value_prop, 20);
        left.SetValue<string>(name_prop, "override");

        Properties right(&registrator, &proto);
        right.SetValue<string>(name_prop, "override");
        right.SetValue<int32_t>(value_prop, 20);

        CHECK(left.CompareData(right, {}, false));

        right.SetValue<bool>(flag_prop, true);
        CHECK_FALSE(left.CompareData(right, {}, false));
    }

    SECTION("IgnoreTemporaryAndIgnorePropsUseFallback")
    {
        Properties left(&registrator);
        left.SetValue<int32_t>(value_prop, 10);
        left.SetValue<int32_t>(temp_prop, 1);

        Properties right(&registrator);
        right.SetValue<int32_t>(value_prop, 99);
        right.SetValue<int32_t>(temp_prop, 2);

        array<ptr<const Property>, 1> ignored_props {{value_prop}};
        const const_span<ptr<const Property>> ignored_props_span {ignored_props.data(), ignored_props.size()};

        CHECK(left.CompareData(right, ignored_props_span, true));
        CHECK_FALSE(left.CompareData(right, {}, true));
        CHECK_FALSE(left.CompareData(right, ignored_props_span, false));
    }

    SECTION("SelfCompareReturnsTrue")
    {
        Properties props(&registrator);
        props.SetValue<int32_t>(value_prop, 10);
        props.SetValue<string>(name_prop, "self");

        CHECK(props.CompareData(props, {}, false));
    }

    SECTION("MixedFullAndDerivedUseFallback")
    {
        Properties proto(&registrator);
        proto.SetValue<int32_t>(value_prop, 1);
        proto.SetValue<bool>(flag_prop, false);
        proto.SetValue<string>(name_prop, "proto");

        Properties derived(&registrator, &proto);
        derived.SetValue<int32_t>(value_prop, 10);
        derived.SetValue<string>(name_prop, "override");

        Properties full(&registrator);
        full.SetValue<int32_t>(value_prop, 10);
        full.SetValue<bool>(flag_prop, false);
        full.SetValue<string>(name_prop, "override");

        CHECK(derived.CompareData(full, {}, false));
        CHECK(full.CompareData(derived, {}, false));

        full.SetValue<bool>(flag_prop, true);
        CHECK_FALSE(derived.CompareData(full, {}, false));
        CHECK_FALSE(full.CompareData(derived, {}, false));
    }
}

TEST_CASE("PropertiesCustomAccessors")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("AccessorEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto plain_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    auto virtual_prop = registrator.RegisterProperty({"Common", "int32", "VirtualValue", "Mutable", "Virtual"});
    auto virtual_without_setter_prop = registrator.RegisterProperty({"Common", "int32", "VirtualWithoutSetter", "Mutable", "Virtual"});

    Properties props(&registrator);
    props.SetEntity(reinterpret_cast<Entity*>(size_t {1}));

    int32_t setter_calls = 0;
    int32_t post_setter_calls = 0;
    int32_t virtual_value = 7;

    plain_prop->AddSetter([&](nptr<Entity>, ptr<const Property>, PropertyRawData& prop_data) {
        setter_calls++;
        prop_data.SetAs<int32_t>(prop_data.GetAs<int32_t>() + 5);
    });
    plain_prop->AddPostSetter([&](nptr<Entity>, ptr<const Property>) { post_setter_calls++; });

    virtual_prop->SetGetter([&](nptr<Entity>, ptr<const Property>) {
        PropertyRawData prop_data;
        prop_data.SetAs<int32_t>(virtual_value);
        return prop_data;
    });
    virtual_prop->AddSetter([&](nptr<Entity>, ptr<const Property>, PropertyRawData& prop_data) { virtual_value = prop_data.GetAs<int32_t>(); });

    props.SetValue<int32_t>(plain_prop, 10);
    CHECK(props.GetValue<int32_t>(plain_prop) == 15);
    CHECK(setter_calls == 1);
    CHECK(post_setter_calls == 1);

    CHECK(props.GetValue<int32_t>(virtual_prop) == 7);
    props.SetValue<int32_t>(virtual_prop, 21);
    CHECK(virtual_value == 21);
    CHECK(props.GetValue<int32_t>(virtual_prop) == 21);

    PropertyRawData prop_data;
    prop_data.SetAs<int32_t>(1);
    CHECK_THROWS(props.SetValue(virtual_without_setter_prop, prop_data));

    PropertyRawData same_data;
    same_data.SetAs<int32_t>(15);
    props.SetValue(plain_prop, same_data);
    CHECK(props.GetValue<int32_t>(plain_prop) == 15);
    CHECK(setter_calls == 1);
    CHECK(post_setter_calls == 1);

    PropertyRawData changed_data;
    changed_data.SetAs<int32_t>(30);
    props.SetValueFromData(plain_prop, changed_data);
    CHECK(props.GetValue<int32_t>(plain_prop) == 35);
    CHECK(setter_calls == 2);
    CHECK(post_setter_calls == 2);
}

TEST_CASE("PropertyRawDataStorageModes")
{
    SECTION("SmallAllocAndSetAsUseOwnedStorage")
    {
        PropertyRawData data;

        data.SetAs<int32_t>(1234);

        CHECK(data.GetSize() == sizeof(int32_t));
        CHECK(data.GetAs<int32_t>() == 1234);

        *data.GetPtrAs<int32_t>() = 4321;
        CHECK(data.GetAs<int32_t>() == 4321);
    }

    SECTION("PassAliasesExternalDataUntilStoreIfPassed")
    {
        int32_t external_value = 55;
        PropertyRawData data;

        data.Pass(&external_value, sizeof(external_value));

        CHECK(data.GetAs<int32_t>() == 55);

        external_value = 77;
        CHECK(data.GetAs<int32_t>() == 77);

        data.StoreIfPassed();
        CHECK(data.GetAs<int32_t>() == 77);

        external_value = 99;
        CHECK(data.GetAs<int32_t>() == 77);

        *data.GetPtrAs<int32_t>() = 101;
        CHECK(data.GetAs<int32_t>() == 101);
        CHECK(external_value == 99);
    }

    SECTION("LargeAllocUsesDynamicOwnedBuffer")
    {
        PropertyRawData data;
        array<uint8_t, PropertyRawData::LOCAL_BUF_SIZE + 8> source {};

        for (size_t i = 0; i < source.size(); i++) {
            source[i] = numeric_cast<uint8_t>(i % 251);
        }

        data.Set(source.data(), source.size());

        CHECK(data.GetSize() == source.size());

        auto stored = data.GetPtrAs<uint8_t>();
        CHECK(std::equal(source.begin(), source.end(), stored.get()));

        source[0] = 255;
        source.back() = 254;
        CHECK(stored[0] != source[0]);
        CHECK(stored[data.GetSize() - 1] != source.back());
    }
}

TEST_CASE("PropertiesOverlayIndexMaintenance")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("OverlayIndexEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    vector<ptr<const Property>> props;
    props.reserve(20);

    for (int32_t i = 0; i < 20; i++) {
        const string prop_name = strex("Value{}", i);
        props.emplace_back(registrator.RegisterProperty({"Common", "string", prop_name, "Mutable", "Persistent", "PublicSync"}));
    }

    Properties base(&registrator);
    Properties derived(&registrator, &base);

    const auto string_bytes = [](string_view value) -> span<const uint8_t> { return {reinterpret_cast<const uint8_t*>(value.data()), value.size()}; };
    const string large_value(5000, 'L');

    for (size_t i = 0; i < 17; i++) {
        const string value = i == 0 ? large_value : strex("value-{}", i).str();
        derived.SetRawData(props[i], string_bytes(value));
    }

    CHECK(derived.GetValue<string>(props[0]) == large_value);
    CHECK(derived.GetValue<string>(props[16]) == "value-16");

    derived.SetRawData(props[1], {});
    CHECK(derived.GetValue<string>(props[1]).empty());
    CHECK(derived.GetValue<string>(props[16]) == "value-16");

    derived.SetRawData(props[0], {});
    CHECK(derived.GetValue<string>(props[0]).empty());
    CHECK(derived.GetValue<string>(props[16]) == "value-16");

    const string tail_value = "tail-after-index-release";
    derived.SetRawData(props[19], string_bytes(tail_value));
    CHECK(derived.GetValue<string>(props[19]) == tail_value);
}

TEST_CASE("PropertiesOverlayDataKeepsNaturalAlignment")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("OverlayAlignedEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    const auto flag_prop = registrator.RegisterProperty({"Common", "bool", "Flag", "Mutable", "Persistent", "PublicSync"});
    const auto hash_prop = registrator.RegisterProperty({"Common", "hstring", "HashValue", "Mutable", "Persistent", "PublicSync"});
    const auto wide_prop = registrator.RegisterProperty({"Common", "int64", "WideValue", "Mutable", "Persistent", "PublicSync"});
    const auto arr_prop = registrator.RegisterProperty({"Common", "int32[]", "ArrValue", "Mutable", "Persistent", "PublicSync"});
    const auto name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});
    const auto ref_arr_prop = registrator.RegisterProperty({"Common", "RouteSnapshot[]", "RefArrValue", "Mutable", "Persistent", "PublicSync"});
    const auto dict_prop = registrator.RegisterProperty({"Common", "int32=>int32", "DictValue", "Mutable", "Persistent", "PublicSync"});

    CHECK(flag_prop->GetDataAlignment() == 1);
    CHECK(hash_prop->GetDataAlignment() == sizeof(hstring::hash_t));
    CHECK(wide_prop->GetDataAlignment() == sizeof(int64_t));
    CHECK(arr_prop->GetDataAlignment() == sizeof(int32_t));
    CHECK(name_prop->GetDataAlignment() == 1);
    CHECK(ref_arr_prop->GetDataAlignment() == MAX_SERIALIZED_ALIGNMENT);
    CHECK(dict_prop->GetDataAlignment() == sizeof(int32_t));

    const hstring base_hash = hashes.ToHashedString("base-hash");
    const hstring overlay_hash = hashes.ToHashedString("overlay-hash");
    constexpr int64_t base_wide_value = 0x1111111111111111;
    constexpr int64_t overlay_wide_value = 0x2222222222222222;
    const vector<int32_t> overlay_arr_value = {1, 2, 3};

    Properties base(&registrator);
    base.SetValue<bool>(flag_prop, false);
    base.SetValue<hstring>(hash_prop, base_hash);
    base.SetValue<int64_t>(wide_prop, base_wide_value);

    const auto is_aligned = [](const uint8_t* data, size_t alignment) -> bool { return reinterpret_cast<uintptr_t>(data) % alignment == 0; };

    const auto check_overlay_values = [&](const Properties& props) {
        const auto flag_raw_data = props.GetRawData(flag_prop);
        const auto hash_raw_data = props.GetRawData(hash_prop);
        const auto wide_raw_data = props.GetRawData(wide_prop);
        const auto arr_raw_data = props.GetRawData(arr_prop);

        REQUIRE(flag_raw_data.size() == sizeof(bool));
        REQUIRE(hash_raw_data.size() == sizeof(hstring::hash_t));
        REQUIRE(wide_raw_data.size() == sizeof(int64_t));
        REQUIRE(arr_raw_data.size() == overlay_arr_value.size() * sizeof(int32_t));
        CHECK(is_aligned(hash_raw_data.data(), sizeof(hstring::hash_t)));
        CHECK(is_aligned(wide_raw_data.data(), sizeof(int64_t)));
        CHECK(is_aligned(arr_raw_data.data(), sizeof(int32_t)));
        CHECK(props.GetValue<bool>(flag_prop));
        CHECK(props.GetValueFast<bool>(flag_prop));
        CHECK(props.GetValue<hstring>(hash_prop) == overlay_hash);
        CHECK(props.GetValueFast<hstring>(hash_prop) == overlay_hash);
        CHECK(props.GetValue<int64_t>(wide_prop) == overlay_wide_value);
        CHECK(props.GetValueFast<int64_t>(wide_prop) == overlay_wide_value);
        CHECK(props.GetValue<vector<int32_t>>(arr_prop) == overlay_arr_value);
    };

    Properties derived(&registrator, &base);
    derived.SetValue<bool>(flag_prop, true);
    derived.SetValue<hstring>(hash_prop, overlay_hash);
    derived.SetValue<int64_t>(wide_prop, overlay_wide_value);
    derived.SetValue(arr_prop, overlay_arr_value);

    check_overlay_values(derived);

    Properties full_source(&registrator);
    full_source.SetValue<bool>(flag_prop, true);
    full_source.SetValue<hstring>(hash_prop, overlay_hash);
    full_source.SetValue<int64_t>(wide_prop, overlay_wide_value);
    full_source.SetValue(arr_prop, overlay_arr_value);

    Properties rebuilt_from_full(&registrator, &base);
    rebuilt_from_full.CopyFrom(full_source);

    check_overlay_values(rebuilt_from_full);

    // Rebuilt layout packs entries alignment-descending (stable by property index) with zero padding:
    // hash (8-byte hash storage) and wide share the strictest alignment, then arr, then flag
    {
        const auto flag_raw_data = rebuilt_from_full.GetRawData(flag_prop);
        const auto hash_raw_data = rebuilt_from_full.GetRawData(hash_prop);
        const auto wide_raw_data = rebuilt_from_full.GetRawData(wide_prop);
        const auto arr_raw_data = rebuilt_from_full.GetRawData(arr_prop);
        CHECK(wide_raw_data.data() == hash_raw_data.data() + hash_raw_data.size());
        CHECK(arr_raw_data.data() == wide_raw_data.data() + wide_raw_data.size());
        CHECK(flag_raw_data.data() == arr_raw_data.data() + arr_raw_data.size());
    }

    const Properties cloned = rebuilt_from_full.Copy();
    check_overlay_values(cloned);
}

TEST_CASE("PropertiesOverlayDataReusesAlignmentPaddings")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("OverlayPaddingEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    const auto first_flag_prop = registrator.RegisterProperty({"Common", "bool", "FirstFlag", "Mutable", "Persistent", "PublicSync"});
    const auto second_flag_prop = registrator.RegisterProperty({"Common", "bool", "SecondFlag", "Mutable", "Persistent", "PublicSync"});
    const auto short_prop = registrator.RegisterProperty({"Common", "uint16", "ShortValue", "Mutable", "Persistent", "PublicSync"});
    const auto int_prop = registrator.RegisterProperty({"Common", "uint32", "IntValue", "Mutable", "Persistent", "PublicSync"});
    const auto wide_prop = registrator.RegisterProperty({"Common", "int64", "WideValue", "Mutable", "Persistent", "PublicSync"});

    Properties base(&registrator);
    Properties derived(&registrator, &base);

    // Tail extension keeps natural alignment and leaves paddings behind
    derived.SetValue<bool>(first_flag_prop, true);
    derived.SetValue<int64_t>(wide_prop, 0x2222222222222222);

    const auto first_flag_data = derived.GetRawData(first_flag_prop).data();
    CHECK(derived.GetRawData(wide_prop).data() == first_flag_data + 8);

    // Following allocations fill the alignment paddings instead of extending the overlay
    derived.SetValue<uint32_t>(int_prop, 0x33333333);
    CHECK(derived.GetRawData(int_prop).data() == first_flag_data + 4);

    derived.SetValue<uint16_t>(short_prop, 0x4444);
    CHECK(derived.GetRawData(short_prop).data() == first_flag_data + 2);

    derived.SetValue<bool>(second_flag_prop, true);
    CHECK(derived.GetRawData(second_flag_prop).data() == first_flag_data + 1);

    CHECK(derived.GetValue<bool>(first_flag_prop));
    CHECK(derived.GetValue<bool>(second_flag_prop));
    CHECK(derived.GetValue<uint16_t>(short_prop) == 0x4444);
    CHECK(derived.GetValue<uint32_t>(int_prop) == 0x33333333);
    CHECK(derived.GetValue<int64_t>(wide_prop) == 0x2222222222222222);

    // Freed hole is reused by the next allocation with a matching alignment
    derived.SetValue<int64_t>(wide_prop, 0);
    CHECK(derived.GetRawData(wide_prop).data() != first_flag_data + 8);

    derived.SetValue<int64_t>(wide_prop, 0x5555555555555555);
    CHECK(derived.GetRawData(wide_prop).data() == first_flag_data + 8);
    CHECK(derived.GetValue<int64_t>(wide_prop) == 0x5555555555555555);

    // Cloned overlay keeps the garbage accounting, so the freed hole stays reusable in the copy
    derived.SetValue<int64_t>(wide_prop, 0);

    Properties cloned = derived.Copy();
    const auto cloned_flag_data = cloned.GetRawData(first_flag_prop).data();
    cloned.SetValue<int64_t>(wide_prop, 0x6666666666666666);
    CHECK(cloned.GetRawData(wide_prop).data() == cloned_flag_data + 8);
    CHECK(cloned.GetValue<int64_t>(wide_prop) == 0x6666666666666666);
}

TEST_CASE("PropertiesOverlayDataPrefersBestFitHole")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("OverlayBestFitEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    const auto wide_prop = registrator.RegisterProperty({"Common", "int64", "WideValue", "Mutable", "Persistent", "PublicSync"});
    const auto flag_prop = registrator.RegisterProperty({"Common", "bool", "Flag", "Mutable", "Persistent", "PublicSync"});
    const auto first_int_prop = registrator.RegisterProperty({"Common", "uint32", "FirstIntValue", "Mutable", "Persistent", "PublicSync"});
    const auto second_int_prop = registrator.RegisterProperty({"Common", "uint32", "SecondIntValue", "Mutable", "Persistent", "PublicSync"});
    const auto short_prop = registrator.RegisterProperty({"Common", "uint16", "ShortValue", "Mutable", "Persistent", "PublicSync"});
    const auto third_int_prop = registrator.RegisterProperty({"Common", "uint32", "ThirdIntValue", "Mutable", "Persistent", "PublicSync"});

    Properties base(&registrator);
    Properties derived(&registrator, &base);

    // Build the layout wide@0, first-int@8, flag@12, second-int@16 with a 3-byte padding hole at 13
    derived.SetValue<int64_t>(wide_prop, 0x2222222222222222);
    derived.SetValue<bool>(flag_prop, true);
    derived.SetValue<uint32_t>(first_int_prop, 0x33333333);
    derived.SetValue<uint32_t>(second_int_prop, 0x44444444);

    const auto wide_data = derived.GetRawData(wide_prop).data();
    const auto flag_data = derived.GetRawData(flag_prop).data();
    REQUIRE(derived.GetRawData(first_int_prop).data() == wide_data + 8);
    REQUIRE(flag_data == wide_data + 12);
    REQUIRE(derived.GetRawData(second_int_prop).data() == wide_data + 16);

    // Freeing the first int opens a 4-byte hole at 8 before the padding hole at 13
    derived.SetValue<uint32_t>(first_int_prop, 0);

    // Best-fit placement picks the smaller padding hole at 14, not the first fitting hole at 8
    derived.SetValue<uint16_t>(short_prop, 0x5555);
    CHECK(derived.GetRawData(short_prop).data() == flag_data + 2);

    // The exact-size hole at 8 is then reused by the next int
    derived.SetValue<uint32_t>(third_int_prop, 0x66666666);
    CHECK(derived.GetRawData(third_int_prop).data() == wide_data + 8);

    CHECK(derived.GetValue<int64_t>(wide_prop) == 0x2222222222222222);
    CHECK(derived.GetValue<bool>(flag_prop));
    CHECK(derived.GetValue<uint16_t>(short_prop) == 0x5555);
    CHECK(derived.GetValue<uint32_t>(second_int_prop) == 0x44444444);
    CHECK(derived.GetValue<uint32_t>(third_int_prop) == 0x66666666);
}

TEST_CASE("PropertiesOverlayDataStaysAlignedThroughRepack")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("OverlayRepackEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    const auto flag_prop = registrator.RegisterProperty({"Common", "bool", "Flag", "Mutable", "Persistent", "PublicSync"});
    const auto int_prop = registrator.RegisterProperty({"Common", "uint32", "IntValue", "Mutable", "Persistent", "PublicSync"});
    const auto wide_prop = registrator.RegisterProperty({"Common", "int64", "WideValue", "Mutable", "Persistent", "PublicSync"});
    const auto name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});

    Properties base(&registrator);
    Properties derived(&registrator, &base);

    derived.SetValue<bool>(flag_prop, true);
    derived.SetValue<uint32_t>(int_prop, 0x33333333);
    derived.SetValue<int64_t>(wide_prop, 0x2222222222222222);

    const auto is_aligned = [](const uint8_t* data, size_t alignment) -> bool { return reinterpret_cast<uintptr_t>(data) % alignment == 0; };

    // Growing and shrinking string payloads force tail growth, garbage buildup and repacks
    for (const size_t str_size : {100, 300, 50, 1000, 10, 500}) {
        derived.SetValue<string>(name_prop, string(str_size, 'x'));

        CHECK(derived.GetValue<string>(name_prop) == string(str_size, 'x'));
        CHECK(is_aligned(derived.GetRawData(int_prop).data(), sizeof(uint32_t)));
        CHECK(is_aligned(derived.GetRawData(wide_prop).data(), sizeof(int64_t)));
        CHECK(derived.GetValue<bool>(flag_prop));
        CHECK(derived.GetValue<uint32_t>(int_prop) == 0x33333333);
        CHECK(derived.GetValue<int64_t>(wide_prop) == 0x2222222222222222);
    }
}

TEST_CASE("PropertiesFullRestoreAndStoreDataEdges")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("FullRestoreEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    const auto value_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    const auto name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});

    Properties empty_complex(&registrator);

    const auto empty_complex_data = empty_complex.StoreData(false);
    CHECK(empty_complex_data.Data->size() == 2);
    CHECK(empty_complex_data.Sizes->size() == 2);

    Properties base(&registrator);
    base.SetValue<int32_t>(value_prop, 10);
    base.SetValue<string>(name_prop, "base");

    Properties full_source(&registrator);
    full_source.SetValue<int32_t>(value_prop, 77);
    full_source.SetValue<string>(name_prop, "full");

    const auto full_data = full_source.StoreData(false);

    Properties derived(&registrator, &base);
    derived.SetValue<int32_t>(value_prop, 25);
    CHECK(derived.GetValue<int32_t>(value_prop) == 25);
    CHECK(derived.GetValue<string>(name_prop) == "base");

    derived.RestoreData(*full_data.Data, *full_data.Sizes);
    CHECK(derived.GetValue<int32_t>(value_prop) == 77);
    CHECK(derived.GetValue<string>(name_prop) == "full");
}

TEST_CASE("PropertiesTextRoundTrip")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("TextEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto tags_prop = registrator.RegisterProperty({"Common", "string[]", "Tags", "Mutable", "Persistent", "PublicSync"});
    auto values_prop = registrator.RegisterProperty({"Common", "int32[]", "Values", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);
    props.SetValue(tags_prop, vector<string> {"alpha", "beta", ""});
    props.SetValue(values_prop, vector<int32_t> {10, 20, 30});

    const auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("Tags"));
    REQUIRE(text_data.contains("Values"));

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);

    CHECK(restored.GetValue<vector<string>>(tags_prop) == vector<string> {"alpha", "beta", ""});
    CHECK(restored.GetValue<vector<int32_t>>(values_prop) == vector<int32_t> {10, 20, 30});
}

TEST_CASE("PropertiesApplyFromTextErrorsAndSkips")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("ApplyTextEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    const auto value_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    const auto client_only_prop = registrator.RegisterProperty({"Client", "int32", "ClientOnlyValue", "Mutable"});
    (void)client_only_prop;
    const auto virtual_prop = registrator.RegisterProperty({"Common", "int32", "VirtualValue", "Mutable", "Virtual"});
    const auto temp_prop = registrator.RegisterProperty({"Common", "int32", "TempValue", "Mutable", "NoSync"});

    Properties props(&registrator);

    CHECK_NOTHROW(props.ApplyFromText(map<string, string> {{"$meta", "ignored"}, {"_technical", "ignored"}, {"Value", "42"}}));
    CHECK(props.GetValue<int32_t>(value_prop) == 42);

    CHECK_NOTHROW(props.ApplyFromText(map<string, string> {{"ClientOnlyValue", "17"}}));
    CHECK_THROWS(props.ApplyFromText(map<string, string> {{"MissingValue", "1"}}));
    CHECK_THROWS(props.ApplyFromText(map<string, string> {{string(virtual_prop->GetName()), "1"}}));
    CHECK_THROWS(props.ApplyFromText(map<string, string> {{string(temp_prop->GetName()), "1"}}));
    CHECK_THROWS(props.ApplyFromText(map<string, string> {{"Value", "not-an-int"}}));
}

TEST_CASE("PropertiesHashAndEnumConversions")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("TypedEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto hash_prop = registrator.RegisterProperty({"Common", "hstring", "HashValue", "Mutable", "Persistent", "PublicSync"});
    auto enum_prop = registrator.RegisterProperty({"Common", "Mode", "ModeValue", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);
    props.SetValue<hstring>(hash_prop, hashes.ToHashedString("alpha"));
    props.SetValueAsInt(enum_prop->GetRegIndex(), 2);

    CHECK(props.GetValue<hstring>(hash_prop) == hashes.ToHashedString("alpha"));
    CHECK(props.GetValueAsInt(enum_prop->GetRegIndex()) == 2);

    auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("HashValue"));
    REQUIRE(text_data.contains("ModeValue"));
    CHECK(text_data["HashValue"] == "alpha");
    CHECK(text_data["ModeValue"] == "ModeB");

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);
    CHECK(restored.GetValue<hstring>(hash_prop) == hashes.ToHashedString("alpha"));
    CHECK(restored.GetValueAsInt(enum_prop->GetRegIndex()) == 2);

    Properties from_any(&registrator);
    from_any.SetValueAsAnyProps(hash_prop->GetRegIndex(), any_t {string {"beta"}});
    from_any.SetValueAsAnyProps(enum_prop->GetRegIndex(), any_t {string {"ModeA"}});
    CHECK(from_any.GetValue<hstring>(hash_prop) == hashes.ToHashedString("beta"));
    CHECK(from_any.GetValueAsInt(enum_prop->GetRegIndex()) == 1);
}

TEST_CASE("PropertiesEnumValueMigration")
{
    HashStorage hashes {};
    TestNameResolver resolver;

    // A removed/renamed enum value "ModeLegacy" should migrate to "ModeA" on load instead of failing resolution.
    resolver.AddMigrationRule(hashes.ToHashedString("Enum"), hashes.ToHashedString("Mode"), hashes.ToHashedString("ModeLegacy"), hashes.ToHashedString("ModeA"));

    PropertyRegistrator registrator("EnumMigrationEntity", EngineSideKind::ServerSide, &hashes, &resolver);
    auto enum_prop = registrator.RegisterProperty({"Common", "Mode", "ModeValue", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);

    // Removed value name resolves through the migration rule.
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, enum_prop, AnyData::Value {string {"ModeLegacy"}}, hashes, resolver));
    CHECK(props.GetValueAsInt(enum_prop->GetRegIndex()) == 1);

    // Current value name still loads directly.
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, enum_prop, AnyData::Value {string {"ModeB"}}, hashes, resolver));
    CHECK(props.GetValueAsInt(enum_prop->GetRegIndex()) == 2);

    // Unknown value without a migration rule still throws.
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, enum_prop, AnyData::Value {string {"ModeNonexistent"}}, hashes, resolver));
}

TEST_CASE("PropertiesNumericWidthConversions")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("NumericWidthsEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto int8_prop = registrator.RegisterProperty({"Common", "int8", "Int8Value", "Mutable", "Persistent", "PublicSync"});
    auto int16_prop = registrator.RegisterProperty({"Common", "int16", "Int16Value", "Mutable", "Persistent", "PublicSync"});
    auto int32_prop = registrator.RegisterProperty({"Common", "int32", "Int32Value", "Mutable", "Persistent", "PublicSync"});
    auto int64_prop = registrator.RegisterProperty({"Common", "int64", "Int64Value", "Mutable", "Persistent", "PublicSync"});
    auto uint8_prop = registrator.RegisterProperty({"Common", "uint8", "UInt8Value", "Mutable", "Persistent", "PublicSync"});
    auto uint16_prop = registrator.RegisterProperty({"Common", "uint16", "UInt16Value", "Mutable", "Persistent", "PublicSync"});
    auto uint32_prop = registrator.RegisterProperty({"Common", "uint32", "UInt32Value", "Mutable", "Persistent", "PublicSync"});
    auto bool_prop = registrator.RegisterProperty({"Common", "bool", "BoolValue", "Mutable", "Persistent", "PublicSync"});
    auto float32_prop = registrator.RegisterProperty({"Common", "float32", "Float32Value", "Mutable", "Persistent", "PublicSync"});
    auto float64_prop = registrator.RegisterProperty({"Common", "float64", "Float64Value", "Mutable", "Persistent", "PublicSync"});
    auto string_prop = registrator.RegisterProperty({"Common", "string", "StringValue", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);

    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, int8_prop, AnyData::Value {string {"-12"}}, hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, int16_prop, AnyData::Value {float64_t {345.0}}, hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, int32_prop, AnyData::Value {string {"True"}}, hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, int64_prop, AnyData::Value {true}, hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, uint8_prop, AnyData::Value {string {"7"}}, hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, uint16_prop, AnyData::Value {float64_t {1024.0}}, hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, uint32_prop, AnyData::Value {string {"false"}}, hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, bool_prop, AnyData::Value {int64_t {2}}, hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, bool_prop, AnyData::Value {float64_t {0.0}}, hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, bool_prop, AnyData::Value {string {"True"}}, hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, float32_prop, AnyData::Value {string {"1.25"}}, hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, float64_prop, AnyData::Value {string {"False"}}, hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, string_prop, AnyData::Value {true}, hashes, resolver));

    CHECK(props.GetValue<int8_t>(int8_prop) == -12);
    CHECK(props.GetValue<int16_t>(int16_prop) == 345);
    CHECK(props.GetValue<int32_t>(int32_prop) == 1);
    CHECK(props.GetValue<int64_t>(int64_prop) == 1);
    CHECK(props.GetValue<uint8_t>(uint8_prop) == 7);
    CHECK(props.GetValue<uint16_t>(uint16_prop) == 1024);
    CHECK(props.GetValue<uint32_t>(uint32_prop) == 0);
    CHECK(props.GetValue<bool>(bool_prop));
    CHECK(props.GetValue<float32_t>(float32_prop) == Catch::Approx(1.25f));
    CHECK(props.GetValue<float64_t>(float64_prop) == Catch::Approx(0.0));
    CHECK(props.GetValue<string>(string_prop) == "true");
    CHECK(PropertiesSerializator::SavePropertyToText(&props, string_prop, hashes, resolver) == "true");
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, string_prop, hashes, resolver) == AnyData::Value {string {"true"}});
}

TEST_CASE("PropertiesPlainDataValueAccessors")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("PlainAccessorsEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto int8_prop = registrator.RegisterProperty({"Common", "int8", "Int8Value", "Mutable", "Persistent", "PublicSync"});
    auto int16_prop = registrator.RegisterProperty({"Common", "int16", "Int16Value", "Mutable", "Persistent", "PublicSync"});
    auto int32_prop = registrator.RegisterProperty({"Common", "int32", "Int32Value", "Mutable", "Persistent", "PublicSync"});
    auto int64_prop = registrator.RegisterProperty({"Common", "int64", "Int64Value", "Mutable", "Persistent", "PublicSync"});
    auto uint8_prop = registrator.RegisterProperty({"Common", "uint8", "UInt8Value", "Mutable", "Persistent", "PublicSync"});
    auto uint16_prop = registrator.RegisterProperty({"Common", "uint16", "UInt16Value", "Mutable", "Persistent", "PublicSync"});
    auto uint32_prop = registrator.RegisterProperty({"Common", "uint32", "UInt32Value", "Mutable", "Persistent", "PublicSync"});
    auto enum_prop = registrator.RegisterProperty({"Common", "Mode", "ModeValue", "Mutable", "Persistent", "PublicSync"});
    auto bool_prop = registrator.RegisterProperty({"Common", "bool", "BoolValue", "Mutable", "Persistent", "PublicSync"});
    auto float32_prop = registrator.RegisterProperty({"Common", "float32", "Float32Value", "Mutable", "Persistent", "PublicSync"});
    auto float64_prop = registrator.RegisterProperty({"Common", "float64", "Float64Value", "Mutable", "Persistent", "PublicSync"});
    auto fixed_hash_prop = registrator.RegisterProperty({"Common", "FixedHash", "FixedHashValue", "Mutable", "Persistent", "PublicSync"});
    auto string_prop = registrator.RegisterProperty({"Common", "string", "StringValue", "Mutable", "Persistent", "PublicSync"});
    auto disabled_prop = registrator.RegisterProperty({"Client", "int32", "ClientOnlyValue", "Mutable"});
    auto readonly_prop = registrator.RegisterProperty({"Common", "int32", "ReadOnlyValue"});
    auto virtual_prop = registrator.RegisterProperty({"Common", "int32", "VirtualValue", "Mutable", "Virtual"});

    Properties props(&registrator);

    props.SetValueAsInt(int8_prop->GetRegIndex(), 12);
    props.SetValueAsInt(int16_prop->GetRegIndex(), -32000);
    props.SetValueAsInt(int32_prop->GetRegIndex(), -123456);
    props.SetValueAsInt(int64_prop->GetRegIndex(), -77);
    props.SetValueAsInt(uint8_prop->GetRegIndex(), 201);
    props.SetValueAsInt(uint16_prop->GetRegIndex(), 65000);
    props.SetValueAsInt(uint32_prop->GetRegIndex(), 123456);
    props.SetValueAsInt(enum_prop->GetRegIndex(), 2);
    props.SetValueAsInt(bool_prop->GetRegIndex(), 1);
    props.SetValueAsInt(float32_prop->GetRegIndex(), 13);
    props.SetValueAsInt(float64_prop->GetRegIndex(), -42);

    CHECK(props.GetValueAsInt(int8_prop->GetRegIndex()) == 12);
    CHECK(props.GetValueAsInt(int16_prop->GetRegIndex()) == -32000);
    CHECK(props.GetValueAsInt(int32_prop->GetRegIndex()) == -123456);
    CHECK(props.GetValueAsInt(int64_prop->GetRegIndex()) == -77);
    CHECK(props.GetValueAsInt(uint8_prop->GetRegIndex()) == 201);
    CHECK(props.GetValueAsInt(uint16_prop->GetRegIndex()) == 65000);
    CHECK(props.GetValueAsInt(uint32_prop->GetRegIndex()) == 123456);
    CHECK(props.GetValueAsInt(enum_prop->GetRegIndex()) == 2);
    CHECK(props.GetValueAsInt(bool_prop->GetRegIndex()) == 1);
    CHECK(props.GetValueAsInt(float32_prop->GetRegIndex()) == 13);
    CHECK(props.GetValueAsInt(float64_prop->GetRegIndex()) == -42);

    props.SetValueAsAny(int8_prop->GetRegIndex(), any_t {string {"11"}});
    props.SetValueAsAny(int16_prop->GetRegIndex(), any_t {string {"-1234"}});
    props.SetValueAsAny(int32_prop->GetRegIndex(), any_t {string {"56789"}});
    props.SetValueAsAny(int64_prop->GetRegIndex(), any_t {string {"-88"}});
    props.SetValueAsAny(uint8_prop->GetRegIndex(), any_t {string {"200"}});
    props.SetValueAsAny(uint16_prop->GetRegIndex(), any_t {string {"64000"}});
    props.SetValueAsAny(uint32_prop->GetRegIndex(), any_t {string {"12345"}});
    props.SetValueAsAny(enum_prop->GetRegIndex(), any_t {string {"1"}});
    props.SetValueAsAny(bool_prop->GetRegIndex(), any_t {string {"true"}});
    props.SetValueAsAny(float32_prop->GetRegIndex(), any_t {string {"13.5"}});
    props.SetValueAsAny(float64_prop->GetRegIndex(), any_t {string {"-42.25"}});
    props.SetValueAsAny(fixed_hash_prop->GetRegIndex(), any_t {string {"knife"}});

    CHECK(props.GetValueAsInt(int8_prop->GetRegIndex()) == 11);
    CHECK(props.GetValueAsInt(int16_prop->GetRegIndex()) == -1234);
    CHECK(props.GetValueAsInt(int32_prop->GetRegIndex()) == 56789);
    CHECK(props.GetValueAsInt(int64_prop->GetRegIndex()) == -88);
    CHECK(props.GetValueAsInt(uint8_prop->GetRegIndex()) == 200);
    CHECK(props.GetValueAsInt(uint16_prop->GetRegIndex()) == 64000);
    CHECK(props.GetValueAsInt(uint32_prop->GetRegIndex()) == 12345);
    CHECK(props.GetValueAsInt(enum_prop->GetRegIndex()) == 1);
    CHECK(props.GetValueAsInt(bool_prop->GetRegIndex()) == 1);
    CHECK(props.GetValue<float32_t>(float32_prop) == Catch::Approx(13.5f));
    CHECK(props.GetValue<float64_t>(float64_prop) == Catch::Approx(-42.25));
    CHECK(props.GetValue<hstring>(fixed_hash_prop) == hashes.ToHashedString("knife"));

    CHECK(!props.GetValueAsAny(int8_prop->GetRegIndex()).empty());
    CHECK(props.GetValueAsAny(int16_prop->GetRegIndex()) == any_t {string {"-1234"}});
    CHECK(props.GetValueAsAny(int32_prop->GetRegIndex()) == any_t {string {"56789"}});
    CHECK(props.GetValueAsAny(int64_prop->GetRegIndex()) == any_t {string {"-88"}});
    CHECK(!props.GetValueAsAny(uint8_prop->GetRegIndex()).empty());
    CHECK(props.GetValueAsAny(uint16_prop->GetRegIndex()) == any_t {string {"64000"}});
    CHECK(props.GetValueAsAny(uint32_prop->GetRegIndex()) == any_t {string {"12345"}});
    CHECK(props.GetValueAsAny(enum_prop->GetRegIndex()) == any_t {string {"1"}});
    CHECK(props.GetValueAsAny(bool_prop->GetRegIndex()) == any_t {string {"true"}});
    CHECK(strvex(props.GetValueAsAny(float32_prop->GetRegIndex())).to_float32() == Catch::Approx(13.5f));
    CHECK(strvex(props.GetValueAsAny(float64_prop->GetRegIndex())).to_float64() == Catch::Approx(-42.25));
    CHECK(props.GetValueAsAny(fixed_hash_prop->GetRegIndex()) == any_t {string {"knife"}});

    props.SetValueAsIntProps(int8_prop->GetRegIndex(), -8);
    props.SetValueAsIntProps(int16_prop->GetRegIndex(), -1600);
    props.SetValueAsIntProps(int32_prop->GetRegIndex(), 32000);
    props.SetValueAsIntProps(int64_prop->GetRegIndex(), -64000);
    props.SetValueAsIntProps(uint8_prop->GetRegIndex(), 8);
    props.SetValueAsIntProps(uint16_prop->GetRegIndex(), 1600);
    props.SetValueAsIntProps(uint32_prop->GetRegIndex(), 32000);
    props.SetValueAsIntProps(enum_prop->GetRegIndex(), 2);
    props.SetValueAsIntProps(bool_prop->GetRegIndex(), 0);
    props.SetValueAsIntProps(float32_prop->GetRegIndex(), 15);
    props.SetValueAsIntProps(float64_prop->GetRegIndex(), -17);

    CHECK(props.GetValue<int8_t>(int8_prop) == -8);
    CHECK(props.GetValue<int16_t>(int16_prop) == -1600);
    CHECK(props.GetValue<int32_t>(int32_prop) == 32000);
    CHECK(props.GetValue<int64_t>(int64_prop) == -64000);
    CHECK(props.GetValue<uint8_t>(uint8_prop) == 8);
    CHECK(props.GetValue<uint16_t>(uint16_prop) == 1600);
    CHECK(props.GetValue<uint32_t>(uint32_prop) == 32000);
    CHECK(props.GetValueAsInt(enum_prop->GetRegIndex()) == 2);
    CHECK_FALSE(props.GetValue<bool>(bool_prop));
    CHECK(props.GetValue<float32_t>(float32_prop) == Catch::Approx(15.0f));
    CHECK(props.GetValue<float64_t>(float64_prop) == Catch::Approx(-17.0));

    HashStorage small_hashes {[](const_span<uint8_t> data) -> uint64_t {
        const string_view text {reinterpret_cast<const char*>(data.data()), data.size()};
        return text == "SmallHash" ? uint64_t {7} : HashStorage::DefaultHash(data);
    }};
    TestNameResolver small_resolver;
    PropertyRegistrator small_registrator("PlainAccessorsHashEntity", EngineSideKind::ServerSide, &small_hashes, &small_resolver);
    const auto hash_prop = small_registrator.RegisterProperty({"Common", "hstring", "HashValue", "Mutable", "Persistent", "PublicSync"});
    const hstring small_hash = small_hashes.ToHashedString("SmallHash");
    Properties small_props(&small_registrator);

    small_props.SetValueAsIntProps(hash_prop->GetRegIndex(), 7);
    CHECK(small_props.GetValue<hstring>(hash_prop) == small_hash);

    const int32_t missing_property_index = std::numeric_limits<int32_t>::max();

    CHECK_THROWS(props.GetValueAsInt(missing_property_index));
    CHECK_THROWS(props.GetValueAsAny(missing_property_index));
    CHECK_THROWS(props.SetValueAsInt(missing_property_index, 1));
    CHECK_THROWS(props.SetValueAsAny(missing_property_index, any_t {string {"1"}}));
    CHECK_THROWS(props.SetValueAsIntProps(missing_property_index, 1));

    CHECK_THROWS(props.GetValueAsInt(string_prop->GetRegIndex()));
    CHECK_THROWS(props.GetValueAsAny(string_prop->GetRegIndex()));
    CHECK_THROWS(props.SetValueAsInt(string_prop->GetRegIndex(), 1));
    CHECK_THROWS(props.SetValueAsAny(string_prop->GetRegIndex(), any_t {string {"text"}}));
    CHECK_THROWS(props.SetValueAsIntProps(string_prop->GetRegIndex(), 1));

    CHECK_THROWS(props.GetValueAsInt(disabled_prop->GetRegIndex()));
    CHECK_THROWS(props.GetValueAsAny(disabled_prop->GetRegIndex()));
    CHECK_THROWS(props.SetValueAsInt(disabled_prop->GetRegIndex(), 1));
    CHECK_THROWS(props.SetValueAsAny(disabled_prop->GetRegIndex(), any_t {string {"1"}}));
    CHECK_THROWS(props.SetValueAsIntProps(disabled_prop->GetRegIndex(), 1));

    CHECK_THROWS(props.SetValueAsIntProps(virtual_prop->GetRegIndex(), 1));
    CHECK_THROWS(props.SetValueAsIntProps(readonly_prop->GetRegIndex(), 1));

    CHECK_THROWS(props.GetValueAsInt(fixed_hash_prop->GetRegIndex()));
}

TEST_CASE("PropertiesNumericRangeValidation")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("NumericRangeEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto int8_prop = registrator.RegisterProperty({"Common", "int8", "Int8Value", "Mutable", "Persistent", "PublicSync"});
    auto int64_prop = registrator.RegisterProperty({"Common", "int64", "Int64Value", "Mutable", "Persistent", "PublicSync"});
    auto uint8_prop = registrator.RegisterProperty({"Common", "uint8", "UInt8Value", "Mutable", "Persistent", "PublicSync"});
    auto float32_prop = registrator.RegisterProperty({"Common", "float32", "Float32Value", "Mutable", "Persistent", "PublicSync"});
    auto float64_prop = registrator.RegisterProperty({"Common", "float64", "Float64Value", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);

    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, int8_prop, AnyData::Value {int64_t {128}}, hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, int8_prop, AnyData::Value {float64_t {127.6}}, hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, uint8_prop, AnyData::Value {int64_t {-1}}, hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, uint8_prop, AnyData::Value {float64_t {-0.6}}, hashes, resolver));

    const auto float32_overflow = static_cast<float64_t>(std::numeric_limits<float32_t>::max()) * 2.0;
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, float32_prop, AnyData::Value {float32_overflow}, hashes, resolver));
    // Build the IEEE-754 +inf bit pattern via bit_cast: numeric_limits::infinity() is flagged as UB under the
    // engine's /fp:fast (-Wnan-infinity-disabled), but this test must feed a real non-finite float.
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, float32_prop, AnyData::Value {std::bit_cast<float64_t>(uint64_t {0x7FF0000000000000})}, hashes, resolver));

    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, int8_prop, "128", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, uint8_prop, "-1", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, int64_prop, "9223372036854775808", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, int64_prop, "9223372036854775808.0", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, int64_prop, "-9223372036854775809", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, float32_prop, "3.5e38", hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, int8_prop, "127.4", hashes, resolver));
    CHECK(props.GetValue<int8_t>(int8_prop) == 127);

    const string long_float_text = string {"1."} + string(1200, '0');
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, float32_prop, long_float_text, hashes, resolver));
    CHECK(props.GetValue<float32_t>(float32_prop) == Catch::Approx(1.0f));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, float32_prop, AnyData::Value {long_float_text}, hashes, resolver));
    CHECK(props.GetValue<float32_t>(float32_prop) == Catch::Approx(1.0f));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, float64_prop, AnyData::Value {long_float_text}, hashes, resolver));
    CHECK(props.GetValue<float64_t>(float64_prop) == Catch::Approx(1.0));
}

TEST_CASE("PropertiesTextScalarWidthConversions")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("NumericTextWidthsEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto int8_prop = registrator.RegisterProperty({"Common", "int8", "Int8Value", "Mutable", "Persistent", "PublicSync"});
    auto int16_prop = registrator.RegisterProperty({"Common", "int16", "Int16Value", "Mutable", "Persistent", "PublicSync"});
    auto int64_prop = registrator.RegisterProperty({"Common", "int64", "Int64Value", "Mutable", "Persistent", "PublicSync"});
    auto uint8_prop = registrator.RegisterProperty({"Common", "uint8", "UInt8Value", "Mutable", "Persistent", "PublicSync"});
    auto uint16_prop = registrator.RegisterProperty({"Common", "uint16", "UInt16Value", "Mutable", "Persistent", "PublicSync"});
    auto uint32_prop = registrator.RegisterProperty({"Common", "uint32", "UInt32Value", "Mutable", "Persistent", "PublicSync"});
    auto float32_prop = registrator.RegisterProperty({"Common", "float32", "Float32Value", "Mutable", "Persistent", "PublicSync"});
    auto float64_prop = registrator.RegisterProperty({"Common", "float64", "Float64Value", "Mutable", "Persistent", "PublicSync"});
    auto bool_prop = registrator.RegisterProperty({"Common", "bool", "BoolValue", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);

    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, int8_prop, "-12", hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, int16_prop, "345", hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, int64_prop, "9876543210", hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, uint8_prop, "7", hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, uint16_prop, "1024", hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, uint32_prop, "65536", hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, float32_prop, "1.25", hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, float64_prop, "2.5", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, int8_prop, "not-number", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, float32_prop, "bad-float", hashes, resolver));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, bool_prop, "True", hashes, resolver));
    CHECK(props.GetValue<bool>(bool_prop));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, bool_prop, "0", hashes, resolver));
    CHECK_FALSE(props.GetValue<bool>(bool_prop));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, bool_prop, "1", hashes, resolver));
    CHECK(props.GetValue<bool>(bool_prop));
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, bool_prop, "false", hashes, resolver));
    CHECK_FALSE(props.GetValue<bool>(bool_prop));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, bool_prop, "not-bool", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, bool_prop, "Enabled", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, bool_prop, "2", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, bool_prop, "-1", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, bool_prop, "0.5", hashes, resolver));

    CHECK(props.GetValue<int8_t>(int8_prop) == -12);
    CHECK(props.GetValue<int16_t>(int16_prop) == 345);
    CHECK(props.GetValue<int64_t>(int64_prop) == 9876543210);
    CHECK(props.GetValue<uint8_t>(uint8_prop) == 7);
    CHECK(props.GetValue<uint16_t>(uint16_prop) == 1024);
    CHECK(props.GetValue<uint32_t>(uint32_prop) == 65536);
    CHECK(props.GetValue<float32_t>(float32_prop) == Catch::Approx(1.25f));
    CHECK(props.GetValue<float64_t>(float64_prop) == Catch::Approx(2.5));
    CHECK_FALSE(props.GetValue<bool>(bool_prop));

    CHECK(PropertiesSerializator::SavePropertyToText(&props, int8_prop, hashes, resolver) == "-12");
    CHECK(PropertiesSerializator::SavePropertyToText(&props, int16_prop, hashes, resolver) == "345");
    CHECK(PropertiesSerializator::SavePropertyToText(&props, int64_prop, hashes, resolver) == "9876543210");
    CHECK(PropertiesSerializator::SavePropertyToText(&props, uint8_prop, hashes, resolver) == "7");
    CHECK(PropertiesSerializator::SavePropertyToText(&props, uint16_prop, hashes, resolver) == "1024");
    CHECK(PropertiesSerializator::SavePropertyToText(&props, uint32_prop, hashes, resolver) == "65536");
    CHECK(PropertiesSerializator::SavePropertyToText(&props, float32_prop, hashes, resolver) == "1.25");
    CHECK(PropertiesSerializator::SavePropertyToText(&props, float64_prop, hashes, resolver) == "2.5");
    CHECK(PropertiesSerializator::SavePropertyToText(&props, bool_prop, hashes, resolver) == "False");
}

TEST_CASE("PropertiesPrimitiveDictKeyTextConversions")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("PrimitiveDictKeyEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto labels_prop = registrator.RegisterProperty({"Common", "int8=>string", "Labels", "Mutable", "Persistent", "PublicSync"});
    auto flags_prop = registrator.RegisterProperty({"Common", "uint16=>bool", "Flags", "Mutable", "Persistent", "PublicSync"});
    auto samples_prop = registrator.RegisterProperty({"Common", "float64=>int8[]", "Samples", "Mutable", "Persistent", "PublicSync"});

    const auto labels_value = []() {
        AnyData::Dict labels;
        labels.Emplace("-7", string {"low"});
        labels.Emplace("12", string {"ridge line"});
        return AnyData::Value {std::move(labels)};
    }();

    const auto flags_value = []() {
        AnyData::Dict flags;
        flags.Emplace("7", true);
        flags.Emplace("1024", false);
        return AnyData::Value {std::move(flags)};
    }();

    const auto samples_value = []() {
        AnyData::Array low_samples;
        low_samples.EmplaceBack(int64_t {-3});
        low_samples.EmplaceBack(int64_t {0});

        AnyData::Array high_samples;
        high_samples.EmplaceBack(int64_t {4});
        high_samples.EmplaceBack(int64_t {9});

        AnyData::Dict samples;
        samples.Emplace("-0.5", AnyData::Value {std::move(low_samples)});
        samples.Emplace("1.25", AnyData::Value {std::move(high_samples)});
        return AnyData::Value {std::move(samples)};
    }();

    Properties props(&registrator);
    props.SetValueAsAnyProps(labels_prop->GetRegIndex(), any_t {AnyData::ValueToString(labels_value)});
    props.SetValueAsAnyProps(flags_prop->GetRegIndex(), any_t {AnyData::ValueToString(flags_value)});
    props.SetValueAsAnyProps(samples_prop->GetRegIndex(), any_t {AnyData::ValueToString(samples_value)});

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, labels_prop, hashes, resolver) == labels_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, flags_prop, hashes, resolver) == flags_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, samples_prop, hashes, resolver) == samples_value);

    const auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("Labels"));
    REQUIRE(text_data.contains("Flags"));
    REQUIRE(text_data.contains("Samples"));
    CHECK(text_data.at("Labels").find("-7") != string::npos);
    CHECK(text_data.at("Labels").find("ridge line") != string::npos);
    CHECK(text_data.at("Flags").find("1024") != string::npos);
    CHECK(text_data.at("Flags").find("False") != string::npos);
    CHECK(text_data.at("Samples").find("-0.5") != string::npos);
    CHECK(text_data.at("Samples").find("\"-3 0\"") != string::npos);
    CHECK(text_data.at("Samples").find("\"4 9\"") != string::npos);

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);

    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, labels_prop, hashes, resolver) == labels_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, flags_prop, hashes, resolver) == flags_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, samples_prop, hashes, resolver) == samples_value);
    CHECK(restored.SaveToText(nullptr) == text_data);
}

TEST_CASE("PropertiesBuiltinProtoReferenceSupport")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("ProtoTypedEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto item_prop = registrator.RegisterProperty({"Common", "ProtoItem", "ItemProto", "Mutable", "Persistent", "PublicSync"});
    auto map_prop = registrator.RegisterProperty({"Common", "ProtoMap", "SpawnMapProto", "Mutable", "Persistent", "PublicSync", "Nullable"});
    auto loot_sets_prop = registrator.RegisterProperty({"Common", "string=>ProtoItem[]", "LootSets", "Mutable", "Persistent", "PublicSync"});

    CHECK(item_prop->GetBaseType().IsEntity);
    CHECK(item_prop->IsBaseTypeEntityProto());
    CHECK(item_prop->IsBaseTypeProtoReference());
    CHECK(map_prop->IsNullable());
    CHECK(loot_sets_prop->IsDictOfArray());
    CHECK(loot_sets_prop->IsBaseTypeEntityProto());
    CHECK(loot_sets_prop->IsBaseTypeProtoReference());

    const auto loot_sets_value = []() {
        AnyData::Array default_loot;
        default_loot.EmplaceBack(string {"knife"});
        default_loot.EmplaceBack(string {"pistol"});

        AnyData::Array backup_loot;
        backup_loot.EmplaceBack(string {"pistol"});

        AnyData::Dict loot_sets;
        loot_sets.Emplace("default", AnyData::Value {std::move(default_loot)});
        loot_sets.Emplace("backup set", AnyData::Value {std::move(backup_loot)});
        return AnyData::Value {std::move(loot_sets)};
    }();

    Properties props(&registrator);
    props.SetValueAsAnyProps(item_prop->GetRegIndex(), any_t {string {"knife"}});
    props.SetValueAsAnyProps(loot_sets_prop->GetRegIndex(), any_t {AnyData::ValueToString(loot_sets_value)});

    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromText(&props, map_prop, "", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, item_prop, "", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, item_prop, "missing_proto", hashes, resolver));

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, item_prop, hashes, resolver) == AnyData::Value {string {"knife"}});
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, loot_sets_prop, hashes, resolver) == loot_sets_value);

    const auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("ItemProto"));
    REQUIRE(text_data.contains("LootSets"));
    CHECK(text_data.at("ItemProto") == "knife");
    CHECK(text_data.at("LootSets").find("backup set") != string::npos);
    CHECK(text_data.at("LootSets").find("knife") != string::npos);
    CHECK(text_data.at("LootSets").find("pistol") != string::npos);

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);

    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, item_prop, hashes, resolver) == AnyData::Value {string {"knife"}});
    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, loot_sets_prop, hashes, resolver) == loot_sets_value);

    vector<uint8_t> all_data;
    set<hstring> str_hashes;
    restored.StoreAllData(all_data, str_hashes);

    CHECK(str_hashes.contains(hashes.ToHashedString("knife")));
    CHECK(str_hashes.contains(hashes.ToHashedString("pistol")));
}

TEST_CASE("PropertiesSerializatorRejectsInvalidTypedInputs")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("InvalidTypedEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto hash_prop = registrator.RegisterProperty({"Common", "hstring", "HashValue", "Mutable", "Persistent", "PublicSync"});
    auto enum_prop = registrator.RegisterProperty({"Common", "Mode", "ModeValue", "Mutable", "Persistent", "PublicSync"});
    auto item_prop = registrator.RegisterProperty({"Common", "ProtoItem", "ItemProto", "Mutable", "Persistent", "PublicSync"});
    auto map_prop = registrator.RegisterProperty({"Common", "ProtoMap", "SpawnMapProto", "Mutable", "Persistent", "PublicSync", "Nullable"});
    auto values_prop = registrator.RegisterProperty({"Common", "int32[]", "Values", "Mutable", "Persistent", "PublicSync"});
    auto tags_prop = registrator.RegisterProperty({"Common", "string[]", "Tags", "Mutable", "Persistent", "PublicSync"});
    auto labels_prop = registrator.RegisterProperty({"Common", "string=>string", "Labels", "Mutable", "Persistent", "PublicSync"});
    auto loot_sets_prop = registrator.RegisterProperty({"Common", "string=>ProtoItem[]", "LootSets", "Mutable", "Persistent", "PublicSync"});
    auto leader_prop = registrator.RegisterProperty({"Common", "string=>Waypoint", "LeaderWaypoint", "Mutable", "Persistent", "PublicSync"});
    auto bool_prop = registrator.RegisterProperty({"Common", "bool", "BoolValue", "Mutable", "Persistent", "PublicSync"});
    auto string_prop = registrator.RegisterProperty({"Common", "string", "StringValue", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);

    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, hash_prop, AnyData::Value {int64_t {42}}, hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, enum_prop, AnyData::Value {true}, hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, enum_prop, AnyData::Value {int64_t {99}}, hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, bool_prop, AnyData::Value {string {"not-bool"}}, hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, bool_prop, AnyData::Value {AnyData::Array {}}, hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, string_prop, AnyData::Value {AnyData::Array {}}, hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, values_prop, AnyData::Value {int64_t {10}}, hashes, resolver));

    AnyData::Array invalid_numeric_values;
    invalid_numeric_values.EmplaceBack(string {"not-a-number"});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, values_prop, AnyData::Value {std::move(invalid_numeric_values)}, hashes, resolver));

    AnyData::Array invalid_tags;
    invalid_tags.EmplaceBack(AnyData::Value {AnyData::Dict {}});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, tags_prop, AnyData::Value {std::move(invalid_tags)}, hashes, resolver));

    AnyData::Dict invalid_labels;
    invalid_labels.Emplace("alpha", AnyData::Value {AnyData::Array {}});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, labels_prop, AnyData::Value {std::move(invalid_labels)}, hashes, resolver));

    AnyData::Dict invalid_loot_sets;
    invalid_loot_sets.Emplace("default", AnyData::Value {string {"knife"}});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, loot_sets_prop, AnyData::Value {std::move(invalid_loot_sets)}, hashes, resolver));

    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, leader_prop, AnyData::Value {AnyData::Array {}}, hashes, resolver));

    AnyData::Dict invalid_leader_value_type;
    invalid_leader_value_type.Emplace("north", AnyData::Value {true});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, leader_prop, AnyData::Value {std::move(invalid_leader_value_type)}, hashes, resolver));

    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, item_prop, AnyData::Value {string {""}}, hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, item_prop, AnyData::Value {string {"missing_proto"}}, hashes, resolver));

    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, map_prop, AnyData::Value {string {""}}, hashes, resolver));
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, map_prop, hashes, resolver) == AnyData::Value {string {""}});

    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&props, enum_prop, AnyData::Value {int64_t {1}}, hashes, resolver));
    CHECK(props.GetValueAsInt(enum_prop->GetRegIndex()) == 1);
}

TEST_CASE("PropertiesStoreAllDataAccumulatesHashesAcrossObjects")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("AccumulatedHashesEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto hash_prop = registrator.RegisterProperty({"Common", "hstring", "HashValue", "Mutable", "Persistent", "PublicSync"});
    auto hash_array_prop = registrator.RegisterProperty({"Common", "hstring[]", "HashValues", "Mutable", "Persistent", "PublicSync"});
    auto hash_dict_prop = registrator.RegisterProperty({"Common", "hstring=>hstring", "HashLookup", "Mutable", "Persistent", "PublicSync"});

    Properties first(&registrator);
    first.SetValue<hstring>(hash_prop, hashes.ToHashedString("alpha"));

    Properties second(&registrator);
    second.SetValue<hstring>(hash_prop, hashes.ToHashedString("beta"));

    vector<uint8_t> all_data;
    set<hstring> str_hashes;

    first.StoreAllData(all_data, str_hashes);
    CHECK(str_hashes.contains(hashes.ToHashedString("alpha")));
    CHECK_FALSE(str_hashes.contains(hashes.ToHashedString("beta")));

    AnyData::Array hash_values;
    hash_values.EmplaceBack(string {"gamma"});
    hash_values.EmplaceBack(string {""});
    hash_values.EmplaceBack(string {"delta"});
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&first, hash_array_prop, AnyData::Value {std::move(hash_values)}, hashes, resolver));

    AnyData::Dict hash_lookup;
    hash_lookup.Emplace("route_alpha", string {"marker_beta"});
    CHECK_NOTHROW(PropertiesSerializator::LoadPropertyFromValue(&first, hash_dict_prop, AnyData::Value {std::move(hash_lookup)}, hashes, resolver));

    first.StoreAllData(all_data, str_hashes);
    CHECK(str_hashes.contains(hashes.ToHashedString("gamma")));
    CHECK(str_hashes.contains(hashes.ToHashedString("delta")));
    CHECK(str_hashes.contains(hashes.ToHashedString("route_alpha")));
    CHECK(str_hashes.contains(hashes.ToHashedString("marker_beta")));

    second.StoreAllData(all_data, str_hashes);
    CHECK(str_hashes.contains(hashes.ToHashedString("alpha")));
    CHECK(str_hashes.contains(hashes.ToHashedString("beta")));
}

TEST_CASE("PropertyRegistratorMetadataBranches")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("MetadataEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    const auto high_group_prop = registrator.RegisterProperty({"Common", "int32", "HighPriority", "Mutable", "Persistent", "PublicSync", "Group", "=", "Main", "^", "10"});
    const auto low_group_prop = registrator.RegisterProperty({"Common", "int32", "LowPriority", "Mutable", "Persistent", "PublicSync", "Group", "=", "Main", "^", "-5", "Max", "=", "100", "Min", "=", "-100", "Quest", "=", "QuestA"});
    const auto component_prop = registrator.RegisterProperty({"Common", "bool", "Marker", "Component"});
    const auto component_value_prop = registrator.RegisterProperty({"Common", "int32", "Marker.Step", "Mutable", "Persistent", "PublicSync"});
    const auto core_prop = registrator.RegisterProperty({"Common", "int32", "CoreValue", "CoreProperty", "Persistent", "SharedProperty"});
    const auto resource_prop = registrator.RegisterProperty({"Common", "hstring", "ResourceHash", "Mutable", "Persistent", "PublicSync", "ModifiableByClient", "ModifiableByAnyClient", "Historical", "Resource", "ScriptFuncType", "=", "ResourceCallback"});
    const auto virtual_proto_prop = registrator.RegisterProperty({"Common", "ProtoItem", "VirtualProto", "Mutable", "Virtual", "NullGetterForProto", "Nullable"});

    const auto groups = registrator.GetPropertyGroups();
    REQUIRE(groups.contains("Main"));
    REQUIRE(groups.at("Main").size() == 2);
    CHECK(groups.at("Main")[0] == low_group_prop);
    CHECK(groups.at("Main")[1] == high_group_prop);

    CHECK(registrator.GetComponents().contains("Marker"));
    CHECK(registrator.GetComponents().at("Marker") == component_prop);

    CHECK(component_prop->IsComponentItself());
    CHECK(component_value_prop->IsInComponent());
    CHECK(component_value_prop->GetComponentName() == "Marker");
    CHECK(component_value_prop->GetNameWithoutComponent() == "Step");
    CHECK(core_prop->IsCoreProperty());
    CHECK(resource_prop->IsBaseTypeResource());
    CHECK(resource_prop->IsModifiableByClient());
    CHECK(resource_prop->IsModifiableByAnyClient());
    CHECK(resource_prop->IsHistorical());
    CHECK(resource_prop->GetBaseScriptFuncType() == "ResourceCallback");
    CHECK(virtual_proto_prop->IsVirtual());
    CHECK(virtual_proto_prop->IsNullGetterForProto());
    CHECK(virtual_proto_prop->IsNullable());
}

TEST_CASE("PropertiesDictConversions")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("DictEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto counters_prop = registrator.RegisterProperty({"Common", "string=>int32", "Counters", "Mutable", "Persistent", "PublicSync"});
    auto tags_prop = registrator.RegisterProperty({"Common", "Mode=>string[]", "ModeTags", "Mutable", "Persistent", "PublicSync"});
    auto routes_prop = registrator.RegisterProperty({"Common", "hstring=>Mode", "RouteModes", "Mutable", "Persistent", "PublicSync"});

    const auto counters_value = []() {
        AnyData::Dict counters;
        counters.Emplace("alpha", int64_t {10});
        counters.Emplace("beta", int64_t {-5});
        return AnyData::Value {std::move(counters)};
    }();

    const auto tags_value = []() {
        AnyData::Array mode_a_tags;
        mode_a_tags.EmplaceBack(string {"front"});
        mode_a_tags.EmplaceBack(string {"rear gate"});

        AnyData::Array mode_b_tags;
        mode_b_tags.EmplaceBack(string {""});
        mode_b_tags.EmplaceBack(string {"solo"});

        AnyData::Dict tags;
        tags.Emplace("ModeA", AnyData::Value {std::move(mode_a_tags)});
        tags.Emplace("ModeB", AnyData::Value {std::move(mode_b_tags)});
        return AnyData::Value {std::move(tags)};
    }();

    const auto routes_value = []() {
        AnyData::Dict routes;
        routes.Emplace("north_route", string {"ModeB"});
        routes.Emplace("south route", string {"ModeA"});
        return AnyData::Value {std::move(routes)};
    }();

    Properties props(&registrator);
    props.SetValueAsAnyProps(counters_prop->GetRegIndex(), any_t {AnyData::ValueToString(counters_value)});
    props.SetValueAsAnyProps(tags_prop->GetRegIndex(), any_t {AnyData::ValueToString(tags_value)});
    props.SetValueAsAnyProps(routes_prop->GetRegIndex(), any_t {AnyData::ValueToString(routes_value)});

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, counters_prop, hashes, resolver) == counters_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, tags_prop, hashes, resolver) == tags_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, routes_prop, hashes, resolver) == routes_value);

    const auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("Counters"));
    REQUIRE(text_data.contains("ModeTags"));
    REQUIRE(text_data.contains("RouteModes"));
    CHECK(text_data.at("Counters") == "alpha 10 beta -5");
    CHECK(text_data.at("ModeTags").find("ModeA") != string::npos);
    CHECK(text_data.at("ModeTags").find("rear gate") != string::npos);
    CHECK(text_data.at("RouteModes").find("north_route") != string::npos);
    CHECK(text_data.at("RouteModes").find("south route") != string::npos);

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);

    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, counters_prop, hashes, resolver) == counters_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, tags_prop, hashes, resolver) == tags_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, routes_prop, hashes, resolver) == routes_value);
    CHECK(restored.SaveToText(nullptr) == text_data);
}

TEST_CASE("PropertiesComplexDataInteriorAlignment")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("ComplexAlignedEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    const auto wide_dict_prop = registrator.RegisterProperty({"Common", "uint8=>int64", "WideDict", "Mutable", "Persistent", "PublicSync"});
    const auto str_dict_prop = registrator.RegisterProperty({"Common", "int32=>string", "StrDict", "Mutable", "Persistent", "PublicSync"});
    const auto arr_dict_prop = registrator.RegisterProperty({"Common", "int32=>int32[]", "ArrDict", "Mutable", "Persistent", "PublicSync"});
    const auto str_arr_prop = registrator.RegisterProperty({"Common", "string[]", "StrArr", "Mutable", "Persistent", "PublicSync"});
    const auto str_key_dict_prop = registrator.RegisterProperty({"Common", "string=>int64", "StrKeyDict", "Mutable", "Persistent", "PublicSync"});
    const auto ref_prop = registrator.RegisterProperty({"Common", "RouteSnapshot", "Snapshot", "Mutable", "Persistent", "PublicSync"});

    CHECK(wide_dict_prop->GetDataAlignment() == sizeof(int64_t));
    CHECK(str_dict_prop->GetDataAlignment() == sizeof(uint32_t));
    CHECK(arr_dict_prop->GetDataAlignment() == sizeof(uint32_t));
    CHECK(str_arr_prop->GetDataAlignment() == sizeof(uint32_t));
    CHECK(str_key_dict_prop->GetDataAlignment() == sizeof(int64_t));
    CHECK(ref_prop->GetDataAlignment() == MAX_SERIALIZED_ALIGNMENT);
    CHECK(registrator.RegisterProperty({"Common", "hstring=>Mode", "HashKeyDict", "Mutable", "Persistent", "PublicSync"})->GetDataAlignment() == sizeof(hstring::hash_t));
    CHECK(registrator.RegisterProperty({"Common", "Mode=>string", "EnumKeyDict", "Mutable", "Persistent", "PublicSync"})->GetDataAlignment() == sizeof(uint32_t));
    CHECK(registrator.RegisterProperty({"Common", "int32=>RouteSnapshot", "RefDict", "Mutable", "Persistent", "PublicSync"})->GetDataAlignment() == MAX_SERIALIZED_ALIGNMENT);

    Properties props(&registrator);

    // dict<uint8, int64>: per entry the key is byte-aligned and the value is 8-aligned
    const auto wide_dict_value = []() {
        AnyData::Dict dict;
        dict.Emplace("1", int64_t {0x1111111111111111});
        dict.Emplace("2", int64_t {-0x2222222222222222});
        return AnyData::Value {std::move(dict)};
    }();

    PropertiesSerializator::LoadPropertyFromValue(&props, wide_dict_prop, wide_dict_value, hashes, resolver);

    {
        const auto raw_data = props.GetRawData(wide_dict_prop);
        REQUIRE(raw_data.size() == 32);
        CHECK(raw_data[0] == 1);
        CHECK(*reinterpret_cast<const int64_t*>(raw_data.data() + 8) == 0x1111111111111111);
        CHECK(raw_data[16] == 2);
        CHECK(*reinterpret_cast<const int64_t*>(raw_data.data() + 24) == -0x2222222222222222);

        for (const size_t pad_pos : {1, 2, 3, 4, 5, 6, 7, 17, 18, 19, 20, 21, 22, 23}) {
            CHECK(raw_data[pad_pos] == 0);
        }
    }

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, wide_dict_prop, hashes, resolver) == wide_dict_value);

    // dict<int32, string>: the second entry key re-aligns to 4 after the odd-length string payload
    const auto str_dict_value = []() {
        AnyData::Dict dict;
        dict.Emplace("7", string {"abc"});
        dict.Emplace("8", string {"x"});
        return AnyData::Value {std::move(dict)};
    }();

    PropertiesSerializator::LoadPropertyFromValue(&props, str_dict_prop, str_dict_value, hashes, resolver);

    {
        const auto raw_data = props.GetRawData(str_dict_prop);
        REQUIRE(raw_data.size() == 21);
        CHECK(*reinterpret_cast<const int32_t*>(raw_data.data()) == 7);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data() + 4) == 3);
        CHECK(raw_data[8] == 'a');
        CHECK(*reinterpret_cast<const int32_t*>(raw_data.data() + 12) == 8);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data() + 16) == 1);
        CHECK(raw_data[20] == 'x');
        CHECK(raw_data[11] == 0);
    }

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, str_dict_prop, hashes, resolver) == str_dict_value);

    // dict<int32, int32[]>: key, count prefix and the packed element run stay 4-aligned
    const auto arr_dict_value = []() {
        AnyData::Array arr;
        arr.EmplaceBack(int64_t {10});
        arr.EmplaceBack(int64_t {20});
        arr.EmplaceBack(int64_t {30});

        AnyData::Dict dict;
        dict.Emplace("5", AnyData::Value {std::move(arr)});
        return AnyData::Value {std::move(dict)};
    }();

    PropertiesSerializator::LoadPropertyFromValue(&props, arr_dict_prop, arr_dict_value, hashes, resolver);

    {
        const auto raw_data = props.GetRawData(arr_dict_prop);
        REQUIRE(raw_data.size() == 20);
        CHECK(*reinterpret_cast<const int32_t*>(raw_data.data()) == 5);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data() + 4) == 3);
        CHECK(*reinterpret_cast<const int32_t*>(raw_data.data() + 8) == 10);
        CHECK(*reinterpret_cast<const int32_t*>(raw_data.data() + 12) == 20);
        CHECK(*reinterpret_cast<const int32_t*>(raw_data.data() + 16) == 30);
    }

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, arr_dict_prop, hashes, resolver) == arr_dict_value);

    // string[]: every length prefix re-aligns to 4 after a variable string payload
    props.SetValue(str_arr_prop, vector<string> {"a", "bc"});

    {
        const auto raw_data = props.GetRawData(str_arr_prop);
        REQUIRE(raw_data.size() == 18);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data()) == 2);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data() + 4) == 1);
        CHECK(raw_data[8] == 'a');
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data() + 12) == 2);
        CHECK(raw_data[16] == 'b');
        CHECK(raw_data[17] == 'c');
        CHECK(raw_data[9] == 0);
        CHECK(raw_data[10] == 0);
        CHECK(raw_data[11] == 0);
    }

    CHECK(props.GetValue<vector<string>>(str_arr_prop) == vector<string> {"a", "bc"});

    // dict<string, int64>: the wide value re-aligns to 8 after the variable-length key
    const auto str_key_dict_value = []() {
        AnyData::Dict dict;
        dict.Emplace("ab", int64_t {0x3333333333333333});
        dict.Emplace("c", int64_t {0x4444444444444444});
        return AnyData::Value {std::move(dict)};
    }();

    PropertiesSerializator::LoadPropertyFromValue(&props, str_key_dict_prop, str_key_dict_value, hashes, resolver);

    {
        const auto raw_data = props.GetRawData(str_key_dict_prop);
        REQUIRE(raw_data.size() == 32);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data()) == 2);
        CHECK(raw_data[4] == 'a');
        CHECK(raw_data[5] == 'b');
        CHECK(*reinterpret_cast<const int64_t*>(raw_data.data() + 8) == 0x3333333333333333);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data() + 16) == 1);
        CHECK(raw_data[20] == 'c');
        CHECK(*reinterpret_cast<const int64_t*>(raw_data.data() + 24) == 0x4444444444444444);
        CHECK(raw_data[6] == 0);
        CHECK(raw_data[7] == 0);
        CHECK(raw_data[21] == 0);
    }

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, str_key_dict_prop, hashes, resolver) == str_key_dict_value);

    // Ref-type blob: field-size prefixes re-align to 4, non-empty field payloads to the field alignment
    const auto note_only_snapshot = []() {
        AnyData::Dict fields;
        fields.Emplace("Note", string {"hi"});
        return AnyData::Value {std::move(fields)};
    }();

    PropertiesSerializator::LoadPropertyFromValue(&props, ref_prop, note_only_snapshot, hashes, resolver);

    {
        // Fields: Values(int32[]) Tags(hstring[]) Anchor(Waypoint) Note(string) - three empty prefixes, then the note
        const auto raw_data = props.GetRawData(ref_prop);
        REQUIRE(raw_data.size() == 18);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data()) == 0);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data() + 4) == 0);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data() + 8) == 0);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data() + 12) == 2);
        CHECK(raw_data[16] == 'h');
        CHECK(raw_data[17] == 'i');
    }

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, ref_prop, hashes, resolver) == note_only_snapshot);

    const hstring tag_hash = hashes.ToHashedString("tag-one");
    const auto tags_only_snapshot = [&tag_hash]() {
        AnyData::Array tags;
        tags.EmplaceBack(string {tag_hash.as_str()});

        AnyData::Dict fields;
        fields.Emplace("Tags", AnyData::Value {std::move(tags)});
        return AnyData::Value {std::move(fields)};
    }();

    PropertiesSerializator::LoadPropertyFromValue(&props, ref_prop, tags_only_snapshot, hashes, resolver);

    {
        // The 8-aligned hstring[] payload of the second field lands at offset 8 after two u32 prefixes
        const auto raw_data = props.GetRawData(ref_prop);
        REQUIRE(raw_data.size() == 16);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data()) == 0);
        CHECK(*reinterpret_cast<const uint32_t*>(raw_data.data() + 4) == sizeof(hstring::hash_t));
        CHECK(*reinterpret_cast<const hstring::hash_t*>(raw_data.data() + 8) == tag_hash.as_hash());
    }

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, ref_prop, hashes, resolver) == tags_only_snapshot);
}

TEST_CASE("PropertiesOverlayRepackHandlesUnevenComplexSizes")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("OverlayUnevenEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    const auto wide_prop = registrator.RegisterProperty({"Common", "int64", "WideValue", "Mutable", "Persistent", "PublicSync"});
    const auto str_arr_prop = registrator.RegisterProperty({"Common", "string[]", "StrArr", "Mutable", "Persistent", "PublicSync"});
    const auto int_prop = registrator.RegisterProperty({"Common", "uint32", "IntValue", "Mutable", "Persistent", "PublicSync"});

    Properties base(&registrator);
    Properties derived(&registrator, &base);

    const auto is_aligned = [](const uint8_t* data, size_t alignment) -> bool { return reinterpret_cast<uintptr_t>(data) % alignment == 0; };

    derived.SetValue<int64_t>(wide_prop, 0x2222222222222222);
    derived.SetValue<uint32_t>(int_prop, 0x33333333);

    // The string-array entry size (18, 41, ... bytes) is not a multiple of its 4-byte alignment,
    // so repacks must insert aligned gaps for the entries that follow it in the pack order
    for (const size_t str_size : {1, 25, 2, 100, 3, 50}) {
        derived.SetValue(str_arr_prop, vector<string> {string(str_size, 'x'), "bc"});

        CHECK(derived.GetValue<vector<string>>(str_arr_prop) == vector<string> {string(str_size, 'x'), "bc"});
        CHECK(is_aligned(derived.GetRawData(str_arr_prop).data(), sizeof(uint32_t)));
        CHECK(is_aligned(derived.GetRawData(wide_prop).data(), sizeof(int64_t)));
        CHECK(is_aligned(derived.GetRawData(int_prop).data(), sizeof(uint32_t)));
        CHECK(derived.GetValue<int64_t>(wide_prop) == 0x2222222222222222);
        CHECK(derived.GetValue<uint32_t>(int_prop) == 0x33333333);
    }

    const Properties cloned = derived.Copy();
    CHECK(cloned.GetValue<vector<string>>(str_arr_prop) == vector<string> {string(50, 'x'), "bc"});
    CHECK(cloned.GetValue<int64_t>(wide_prop) == 0x2222222222222222);
}

TEST_CASE("PropertiesNumericDictConversions")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("NumericDictEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto flags_prop = registrator.RegisterProperty({"Common", "int32=>bool", "Flags", "Mutable", "Persistent", "PublicSync"});
    auto checkpoints_prop = registrator.RegisterProperty({"Common", "bool=>int32[]", "Checkpoints", "Mutable", "Persistent", "PublicSync"});

    const auto flags_value = []() {
        AnyData::Dict flags;
        flags.Emplace("-7", true);
        flags.Emplace("42", false);
        return AnyData::Value {std::move(flags)};
    }();

    const auto checkpoints_value = []() {
        AnyData::Array false_points;
        false_points.EmplaceBack(int64_t {0});
        false_points.EmplaceBack(int64_t {5});

        AnyData::Array true_points;
        true_points.EmplaceBack(int64_t {11});
        true_points.EmplaceBack(int64_t {22});
        true_points.EmplaceBack(int64_t {33});

        AnyData::Dict checkpoints;
        checkpoints.Emplace("False", AnyData::Value {std::move(false_points)});
        checkpoints.Emplace("True", AnyData::Value {std::move(true_points)});
        return AnyData::Value {std::move(checkpoints)};
    }();

    Properties props(&registrator);
    props.SetValueAsAnyProps(flags_prop->GetRegIndex(), any_t {AnyData::ValueToString(flags_value)});
    props.SetValueAsAnyProps(checkpoints_prop->GetRegIndex(), any_t {AnyData::ValueToString(checkpoints_value)});

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, flags_prop, hashes, resolver) == flags_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, checkpoints_prop, hashes, resolver) == checkpoints_value);

    const auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("Flags"));
    REQUIRE(text_data.contains("Checkpoints"));
    CHECK(text_data.at("Flags") == "-7 True 42 False");
    CHECK(text_data.at("Checkpoints").find("False") != string::npos);
    CHECK(text_data.at("Checkpoints").find("\"0 5\"") != string::npos);
    CHECK(text_data.at("Checkpoints").find("True") != string::npos);
    CHECK(text_data.at("Checkpoints").find("\"11 22 33\"") != string::npos);

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);

    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, flags_prop, hashes, resolver) == flags_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, checkpoints_prop, hashes, resolver) == checkpoints_value);
    CHECK(restored.SaveToText(nullptr) == text_data);
}

TEST_CASE("PropertiesSpecialValueDictArrays")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("SpecialDictEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto mode_sets_prop = registrator.RegisterProperty({"Common", "string=>Mode[]", "ModeSets", "Mutable", "Persistent", "PublicSync"});
    auto route_tags_prop = registrator.RegisterProperty({"Common", "int32=>hstring[]", "RouteTags", "Mutable", "Persistent", "PublicSync"});

    const auto mode_sets_value = []() {
        AnyData::Array primary_modes;
        primary_modes.EmplaceBack(string {"ModeA"});
        primary_modes.EmplaceBack(string {"ModeB"});

        AnyData::Array reserve_modes;
        reserve_modes.EmplaceBack(string {"ModeB"});

        AnyData::Dict mode_sets;
        mode_sets.Emplace("primary", AnyData::Value {std::move(primary_modes)});
        mode_sets.Emplace("reserve squad", AnyData::Value {std::move(reserve_modes)});
        return AnyData::Value {std::move(mode_sets)};
    }();

    const auto route_tags_value = []() {
        AnyData::Array north_route_tags;
        north_route_tags.EmplaceBack(string {"scout"});
        north_route_tags.EmplaceBack(string {"night shift"});

        AnyData::Array south_route_tags;
        south_route_tags.EmplaceBack(string {"heavy"});

        AnyData::Dict route_tags;
        route_tags.Emplace("7", AnyData::Value {std::move(north_route_tags)});
        route_tags.Emplace("11", AnyData::Value {std::move(south_route_tags)});
        return AnyData::Value {std::move(route_tags)};
    }();

    Properties props(&registrator);
    props.SetValueAsAnyProps(mode_sets_prop->GetRegIndex(), any_t {AnyData::ValueToString(mode_sets_value)});
    props.SetValueAsAnyProps(route_tags_prop->GetRegIndex(), any_t {AnyData::ValueToString(route_tags_value)});

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, mode_sets_prop, hashes, resolver) == mode_sets_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, route_tags_prop, hashes, resolver) == route_tags_value);

    const auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("ModeSets"));
    REQUIRE(text_data.contains("RouteTags"));
    CHECK(text_data.at("ModeSets").find("reserve squad") != string::npos);
    CHECK(text_data.at("ModeSets").find("ModeA") != string::npos);
    CHECK(text_data.at("ModeSets").find("ModeB") != string::npos);
    CHECK(text_data.at("RouteTags").find("7") != string::npos);
    CHECK(text_data.at("RouteTags").find("night shift") != string::npos);
    CHECK(text_data.at("RouteTags").find("heavy") != string::npos);

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);

    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, mode_sets_prop, hashes, resolver) == mode_sets_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, route_tags_prop, hashes, resolver) == route_tags_value);
    CHECK(restored.SaveToText(nullptr) == text_data);
}

TEST_CASE("PropertiesFloatDictConversions")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("FloatDictEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto labels_prop = registrator.RegisterProperty({"Common", "float32=>string", "Labels", "Mutable", "Persistent", "PublicSync"});
    auto samples_prop = registrator.RegisterProperty({"Common", "float32=>float32[]", "Samples", "Mutable", "Persistent", "PublicSync"});

    const auto labels_value = []() {
        AnyData::Dict labels;
        labels.Emplace("-0.5", string {"low"});
        labels.Emplace("1.25", string {"high tide"});
        return AnyData::Value {std::move(labels)};
    }();

    const auto samples_value = []() {
        AnyData::Array low_samples;
        low_samples.EmplaceBack(float64_t {-2.5});
        low_samples.EmplaceBack(float64_t {0.125});

        AnyData::Array high_samples;
        high_samples.EmplaceBack(float64_t {1.25});
        high_samples.EmplaceBack(float64_t {2.5});

        AnyData::Dict samples;
        samples.Emplace("-0.5", AnyData::Value {std::move(low_samples)});
        samples.Emplace("1.25", AnyData::Value {std::move(high_samples)});
        return AnyData::Value {std::move(samples)};
    }();

    Properties props(&registrator);
    props.SetValueAsAnyProps(labels_prop->GetRegIndex(), any_t {AnyData::ValueToString(labels_value)});
    props.SetValueAsAnyProps(samples_prop->GetRegIndex(), any_t {AnyData::ValueToString(samples_value)});

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, labels_prop, hashes, resolver) == labels_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, samples_prop, hashes, resolver) == samples_value);

    const auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("Labels"));
    REQUIRE(text_data.contains("Samples"));
    CHECK(text_data.at("Labels").find("-0.5") != string::npos);
    CHECK(text_data.at("Labels").find("high tide") != string::npos);
    CHECK(text_data.at("Samples").find("1.25") != string::npos);
    CHECK(text_data.at("Samples").find("\"-2.5 0.125\"") != string::npos);
    CHECK(text_data.at("Samples").find("\"1.25 2.5\"") != string::npos);

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);

    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, labels_prop, hashes, resolver) == labels_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, samples_prop, hashes, resolver) == samples_value);
    CHECK(restored.SaveToText(nullptr) == text_data);
}

TEST_CASE("PropertiesStructDictConversions")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("StructDictEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto leader_prop = registrator.RegisterProperty({"Common", "string=>Waypoint", "LeaderWaypoint", "Mutable", "Persistent", "PublicSync"});
    auto patrol_prop = registrator.RegisterProperty({"Common", "int32=>Waypoint[]", "PatrolWaypoints", "Mutable", "Persistent", "PublicSync"});

    const auto leader_value = []() {
        AnyData::Dict leaders;

        AnyData::Array north_waypoint;
        north_waypoint.EmplaceBack(int64_t {10});
        north_waypoint.EmplaceBack(float64_t {1.5});
        north_waypoint.EmplaceBack(true);

        AnyData::Array south_waypoint;
        south_waypoint.EmplaceBack(int64_t {20});
        south_waypoint.EmplaceBack(float64_t {3.25});
        south_waypoint.EmplaceBack(false);

        leaders.Emplace("north", AnyData::Value {std::move(north_waypoint)});
        leaders.Emplace("south gate", AnyData::Value {std::move(south_waypoint)});
        return AnyData::Value {std::move(leaders)};
    }();

    const auto patrol_value = []() {
        AnyData::Array first_path;

        AnyData::Array first_path_point_a;
        first_path_point_a.EmplaceBack(int64_t {1});
        first_path_point_a.EmplaceBack(float64_t {0.5});
        first_path_point_a.EmplaceBack(true);
        first_path.EmplaceBack(AnyData::Value {std::move(first_path_point_a)});

        AnyData::Array first_path_point_b;
        first_path_point_b.EmplaceBack(int64_t {2});
        first_path_point_b.EmplaceBack(float64_t {1.25});
        first_path_point_b.EmplaceBack(false);
        first_path.EmplaceBack(AnyData::Value {std::move(first_path_point_b)});

        AnyData::Array second_path;

        AnyData::Array second_path_point_a;
        second_path_point_a.EmplaceBack(int64_t {7});
        second_path_point_a.EmplaceBack(float64_t {4.75});
        second_path_point_a.EmplaceBack(true);
        second_path.EmplaceBack(AnyData::Value {std::move(second_path_point_a)});

        AnyData::Dict patrols;
        patrols.Emplace("1", AnyData::Value {std::move(first_path)});
        patrols.Emplace("2", AnyData::Value {std::move(second_path)});
        return AnyData::Value {std::move(patrols)};
    }();

    Properties props(&registrator);
    props.SetValueAsAnyProps(leader_prop->GetRegIndex(), any_t {AnyData::ValueToString(leader_value)});
    props.SetValueAsAnyProps(patrol_prop->GetRegIndex(), any_t {AnyData::ValueToString(patrol_value)});

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, leader_prop, hashes, resolver) == leader_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, patrol_prop, hashes, resolver) == patrol_value);

    const auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("LeaderWaypoint"));
    REQUIRE(text_data.contains("PatrolWaypoints"));
    CHECK(text_data.at("LeaderWaypoint").find("south gate") != string::npos);
    CHECK(text_data.at("LeaderWaypoint").find("\"10 1.5 True\"") != string::npos);
    CHECK(text_data.at("PatrolWaypoints").find("0.5") != string::npos);
    CHECK(text_data.at("PatrolWaypoints").find("1.25") != string::npos);
    CHECK(text_data.at("PatrolWaypoints").find("4.75") != string::npos);
    CHECK(text_data.at("PatrolWaypoints").find("True") != string::npos);
    CHECK(text_data.at("PatrolWaypoints").find("False") != string::npos);

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);

    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, leader_prop, hashes, resolver) == leader_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, patrol_prop, hashes, resolver) == patrol_value);
    CHECK(restored.SaveToText(nullptr) == text_data);
}

TEST_CASE("PropertiesSerializatorRejectsInvalidStructShapes")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("InvalidStructEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto leader_prop = registrator.RegisterProperty({"Common", "string=>Waypoint", "LeaderWaypoint", "Mutable", "Persistent", "PublicSync"});
    auto patrol_prop = registrator.RegisterProperty({"Common", "int32=>Waypoint[]", "PatrolWaypoints", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);

    AnyData::Dict invalid_leader_from_array;
    AnyData::Array short_waypoint;
    short_waypoint.EmplaceBack(int64_t {10});
    short_waypoint.EmplaceBack(float64_t {1.5});
    invalid_leader_from_array.Emplace("north", AnyData::Value {std::move(short_waypoint)});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, leader_prop, AnyData::Value {std::move(invalid_leader_from_array)}, hashes, resolver));

    AnyData::Dict invalid_leader_from_string;
    invalid_leader_from_string.Emplace("south", AnyData::Value {string {"20 2.5"}});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, leader_prop, AnyData::Value {std::move(invalid_leader_from_string)}, hashes, resolver));

    AnyData::Dict invalid_patrol_type;
    invalid_patrol_type.Emplace("1", AnyData::Value {string {"not-an-array"}});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, patrol_prop, AnyData::Value {std::move(invalid_patrol_type)}, hashes, resolver));

    AnyData::Dict invalid_patrol_shape;
    AnyData::Array malformed_path;
    malformed_path.EmplaceBack(string {"10 1.0"});
    invalid_patrol_shape.Emplace("2", AnyData::Value {std::move(malformed_path)});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, patrol_prop, AnyData::Value {std::move(invalid_patrol_shape)}, hashes, resolver));
}

TEST_CASE("PropertiesRefTypeConversions")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("RefTypeEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto snapshot_prop = registrator.RegisterProperty({"Common", "RouteSnapshot", "Snapshot", "Mutable", "Persistent", "PublicSync"});

    const auto snapshot_value = []() {
        AnyData::Array values;
        values.EmplaceBack(int64_t {1});
        values.EmplaceBack(int64_t {2});
        values.EmplaceBack(int64_t {3});

        AnyData::Array tags;
        tags.EmplaceBack(string {"alpha"});
        tags.EmplaceBack(string {"beta"});

        AnyData::Array anchor;
        anchor.EmplaceBack(int64_t {10});
        anchor.EmplaceBack(float64_t {1.5});
        anchor.EmplaceBack(true);

        AnyData::Dict snapshot;
        snapshot.Emplace("Values", AnyData::Value {std::move(values)});
        snapshot.Emplace("Tags", AnyData::Value {std::move(tags)});
        snapshot.Emplace("Anchor", AnyData::Value {std::move(anchor)});
        snapshot.Emplace("Note", AnyData::Value {string {"smoke"}});
        return AnyData::Value {std::move(snapshot)};
    }();

    Properties props(&registrator);
    PropertiesSerializator::LoadPropertyFromValue(&props, snapshot_prop, snapshot_value, hashes, resolver);

    CHECK(snapshot_prop->IsBaseTypeRefType());
    CHECK_FALSE(snapshot_prop->IsPlainData());
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, snapshot_prop, hashes, resolver) == snapshot_value);

    const auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("Snapshot"));
    CHECK(text_data.at("Snapshot").find("Values") != string::npos);
    CHECK(text_data.at("Snapshot").find("Tags") != string::npos);
    CHECK(text_data.at("Snapshot").find("Anchor") != string::npos);
    CHECK(text_data.at("Snapshot").find("Note") != string::npos);
    CHECK(text_data.at("Snapshot").find("alpha") != string::npos);
    CHECK(text_data.at("Snapshot").find("10 1.5 True") != string::npos);

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);

    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, snapshot_prop, hashes, resolver) == snapshot_value);
    CHECK(restored.SaveToText(nullptr) == text_data);
}

TEST_CASE("PropertiesNestedRefTypeConversions")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("NestedRefTypeEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto envelope_prop = registrator.RegisterProperty({"Common", "RouteEnvelope", "Envelope", "Mutable", "Persistent", "PublicSync"});

    const auto make_snapshot = [](int32_t start_value, string_view tag, string_view note, bool anchor_flag) {
        AnyData::Array values;
        values.EmplaceBack(int64_t {start_value});
        values.EmplaceBack(int64_t {start_value + 1});

        AnyData::Array tags;
        tags.EmplaceBack(string {tag});

        AnyData::Array anchor;
        anchor.EmplaceBack(int64_t {start_value * 10});
        anchor.EmplaceBack(float64_t {static_cast<double>(start_value) + 0.5});
        anchor.EmplaceBack(anchor_flag);

        AnyData::Dict snapshot;
        snapshot.Emplace("Values", AnyData::Value {std::move(values)});
        snapshot.Emplace("Tags", AnyData::Value {std::move(tags)});
        snapshot.Emplace("Anchor", AnyData::Value {std::move(anchor)});
        snapshot.Emplace("Note", AnyData::Value {string {note}});
        return AnyData::Value {std::move(snapshot)};
    };

    const auto envelope_value = [&]() {
        AnyData::Dict backup;
        backup.Emplace("Note", AnyData::Value {string {"tail"}});

        AnyData::Dict envelope;
        envelope.Emplace("Primary", make_snapshot(5, "alpha", "lead", true));
        envelope.Emplace("Backup", AnyData::Value {std::move(backup)});
        envelope.Emplace("Title", AnyData::Value {string {"nested"}});
        return AnyData::Value {std::move(envelope)};
    }();

    Properties props(&registrator);
    PropertiesSerializator::LoadPropertyFromValue(&props, envelope_prop, envelope_value, hashes, resolver);

    CHECK(envelope_prop->IsBaseTypeRefType());
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, envelope_prop, hashes, resolver) == envelope_value);

    const auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("Envelope"));
    CHECK(text_data.at("Envelope").find("Primary") != string::npos);
    CHECK(text_data.at("Envelope").find("Backup") != string::npos);
    CHECK(text_data.at("Envelope").find("Title") != string::npos);

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);

    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, envelope_prop, hashes, resolver) == envelope_value);
    CHECK(restored.SaveToText(nullptr) == text_data);
}

TEST_CASE("PropertiesRefTypeCollectionConversions")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("RefTypeCollectionEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto snapshots_prop = registrator.RegisterProperty({"Common", "RouteSnapshot[]", "Snapshots", "Mutable", "Persistent", "PublicSync"});
    auto snapshots_by_name_prop = registrator.RegisterProperty({"Common", "string=>RouteSnapshot", "SnapshotsByName", "Mutable", "Persistent", "PublicSync"});
    auto snapshot_groups_prop = registrator.RegisterProperty({"Common", "int32=>RouteSnapshot[]", "SnapshotGroups", "Mutable", "Persistent", "PublicSync"});

    const auto make_snapshot = [](int32_t start_value, string_view tag, string_view note, bool anchor_flag) {
        AnyData::Array values;
        values.EmplaceBack(int64_t {start_value});
        values.EmplaceBack(int64_t {start_value + 1});

        AnyData::Array tags;
        tags.EmplaceBack(string {tag});

        AnyData::Array anchor;
        anchor.EmplaceBack(int64_t {start_value * 10});
        anchor.EmplaceBack(float64_t {static_cast<double>(start_value) + 0.5});
        anchor.EmplaceBack(anchor_flag);

        AnyData::Dict snapshot;
        snapshot.Emplace("Values", AnyData::Value {std::move(values)});
        snapshot.Emplace("Tags", AnyData::Value {std::move(tags)});
        snapshot.Emplace("Anchor", AnyData::Value {std::move(anchor)});
        snapshot.Emplace("Note", AnyData::Value {string {note}});
        return AnyData::Value {std::move(snapshot)};
    };

    const auto make_sparse_snapshot = [](string_view note) {
        AnyData::Dict snapshot;
        snapshot.Emplace("Note", AnyData::Value {string {note}});
        return AnyData::Value {std::move(snapshot)};
    };

    AnyData::Array snapshots;
    snapshots.EmplaceBack(make_snapshot(1, "alpha", "first", true));
    snapshots.EmplaceBack(AnyData::Value {AnyData::Dict {}});
    snapshots.EmplaceBack(make_sparse_snapshot("tail"));
    const auto snapshots_value = AnyData::Value {std::move(snapshots)};

    AnyData::Dict snapshots_by_name;
    snapshots_by_name.Emplace("lead", make_snapshot(2, "beta", "named", false));
    snapshots_by_name.Emplace("idle patrol", AnyData::Value {AnyData::Dict {}});
    const auto snapshots_by_name_value = AnyData::Value {std::move(snapshots_by_name)};

    AnyData::Array group_one;
    group_one.EmplaceBack(make_snapshot(3, "gamma", "group-one", true));
    group_one.EmplaceBack(make_sparse_snapshot("backup"));

    AnyData::Array group_two;
    group_two.EmplaceBack(AnyData::Value {AnyData::Dict {}});

    AnyData::Dict snapshot_groups;
    snapshot_groups.Emplace("1", AnyData::Value {std::move(group_one)});
    snapshot_groups.Emplace("2", AnyData::Value {std::move(group_two)});
    const auto snapshot_groups_value = AnyData::Value {std::move(snapshot_groups)};

    Properties props(&registrator);
    PropertiesSerializator::LoadPropertyFromValue(&props, snapshots_prop, snapshots_value, hashes, resolver);
    PropertiesSerializator::LoadPropertyFromValue(&props, snapshots_by_name_prop, snapshots_by_name_value, hashes, resolver);
    PropertiesSerializator::LoadPropertyFromValue(&props, snapshot_groups_prop, snapshot_groups_value, hashes, resolver);

    CHECK(snapshots_prop->IsBaseTypeRefType());
    CHECK(snapshots_prop->IsArray());
    CHECK(snapshots_by_name_prop->IsDict());
    CHECK_FALSE(snapshots_by_name_prop->IsDictOfArray());
    CHECK(snapshot_groups_prop->IsDictOfArray());

    CHECK(PropertiesSerializator::SavePropertyToValue(&props, snapshots_prop, hashes, resolver) == snapshots_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, snapshots_by_name_prop, hashes, resolver) == snapshots_by_name_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, snapshot_groups_prop, hashes, resolver) == snapshot_groups_value);

    const auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("Snapshots"));
    REQUIRE(text_data.contains("SnapshotsByName"));
    REQUIRE(text_data.contains("SnapshotGroups"));
    CHECK(text_data.at("Snapshots").find("first") != string::npos);
    CHECK(text_data.at("SnapshotsByName").find("idle patrol") != string::npos);
    CHECK(text_data.at("SnapshotGroups").find("backup") != string::npos);

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);

    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, snapshots_prop, hashes, resolver) == snapshots_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, snapshots_by_name_prop, hashes, resolver) == snapshots_by_name_value);
    CHECK(PropertiesSerializator::SavePropertyToValue(&restored, snapshot_groups_prop, hashes, resolver) == snapshot_groups_value);
    CHECK(restored.SaveToText(nullptr) == text_data);
}

TEST_CASE("PropertiesRefTypeSerializationSkipsDefaultFields")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("SparseRefTypeEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto snapshot_prop = registrator.RegisterProperty({"Common", "RouteSnapshot", "Snapshot", "Mutable", "Persistent", "PublicSync"});

    AnyData::Dict snapshot;
    snapshot.Emplace("Note", AnyData::Value {string {"smoke"}});

    Properties props(&registrator);
    PropertiesSerializator::LoadPropertyFromValue(&props, snapshot_prop, AnyData::Value {std::move(snapshot)}, hashes, resolver);

    const auto saved_value = PropertiesSerializator::SavePropertyToValue(&props, snapshot_prop, hashes, resolver);
    REQUIRE(saved_value.Type() == AnyData::ValueType::Dict);
    CHECK(saved_value.AsDict().Size() == 1);
    CHECK(saved_value.AsDict().Contains("Note"));

    const auto text = PropertiesSerializator::SavePropertyToText(&props, snapshot_prop, hashes, resolver);
    CHECK(text.find("Note") != string::npos);
    CHECK(text.find("smoke") != string::npos);
    CHECK(text.find("Values") == string::npos);
    CHECK(text.find("Tags") == string::npos);
    CHECK(text.find("Anchor") == string::npos);
}

TEST_CASE("PropertiesSerializatorRejectsInvalidRefTypeShapes")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("InvalidRefTypeEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto snapshot_prop = registrator.RegisterProperty({"Common", "RouteSnapshot", "Snapshot", "Mutable", "Persistent", "PublicSync"});
    Properties props(&registrator);

    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, snapshot_prop, AnyData::Value {AnyData::Array {}}, hashes, resolver));

    AnyData::Dict invalid_unknown_field;
    invalid_unknown_field.Emplace("Unknown", AnyData::Value {int64_t {1}});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, snapshot_prop, AnyData::Value {std::move(invalid_unknown_field)}, hashes, resolver));

    AnyData::Dict invalid_anchor;
    AnyData::Array short_anchor;
    short_anchor.EmplaceBack(int64_t {10});
    invalid_anchor.Emplace("Anchor", AnyData::Value {std::move(short_anchor)});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, snapshot_prop, AnyData::Value {std::move(invalid_anchor)}, hashes, resolver));

    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, snapshot_prop, "Unknown 1", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, snapshot_prop, "Note", hashes, resolver));
}

TEST_CASE("PropertiesSerializatorRejectsInvalidRefTypeCollectionShapes")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("InvalidRefTypeCollectionEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto snapshots_prop = registrator.RegisterProperty({"Common", "RouteSnapshot[]", "Snapshots", "Mutable", "Persistent", "PublicSync"});
    auto snapshots_by_name_prop = registrator.RegisterProperty({"Common", "string=>RouteSnapshot", "SnapshotsByName", "Mutable", "Persistent", "PublicSync"});
    auto snapshot_groups_prop = registrator.RegisterProperty({"Common", "int32=>RouteSnapshot[]", "SnapshotGroups", "Mutable", "Persistent", "PublicSync"});
    Properties props(&registrator);

    AnyData::Array invalid_snapshots;
    invalid_snapshots.EmplaceBack(int64_t {1});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, snapshots_prop, AnyData::Value {std::move(invalid_snapshots)}, hashes, resolver));

    AnyData::Dict invalid_snapshots_by_name;
    invalid_snapshots_by_name.Emplace("alpha", int64_t {1});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, snapshots_by_name_prop, AnyData::Value {std::move(invalid_snapshots_by_name)}, hashes, resolver));

    AnyData::Array invalid_group_entries;
    invalid_group_entries.EmplaceBack(int64_t {5});

    AnyData::Dict invalid_snapshot_groups;
    invalid_snapshot_groups.Emplace("1", AnyData::Value {std::move(invalid_group_entries)});
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromValue(&props, snapshot_groups_prop, AnyData::Value {std::move(invalid_snapshot_groups)}, hashes, resolver));

    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, snapshots_prop, "1", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, snapshots_by_name_prop, "alpha 1", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, snapshot_groups_prop, "1 \"1\"", hashes, resolver));
}

TEST_CASE("PropertiesSerializatorRejectsInvalidTextStructShapes")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("InvalidTextStructEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto waypoint_prop = registrator.RegisterProperty({"Common", "Waypoint", "WaypointValue", "Mutable", "Persistent", "PublicSync"});
    auto leader_prop = registrator.RegisterProperty({"Common", "string=>Waypoint", "LeaderWaypoint", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);

    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, waypoint_prop, "10 1.5", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, waypoint_prop, "10 1.5 True extra", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, leader_prop, "\"north\" \"10 1.5\"", hashes, resolver));
    CHECK_THROWS(PropertiesSerializator::LoadPropertyFromText(&props, leader_prop, "\"north\" \"10 1.5 True extra\"", hashes, resolver));
}

TEST_CASE("PropertiesSaveToDocumentSkipsDefaultAndBaseValues")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("DocumentEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto counter_prop = registrator.RegisterProperty({"Common", "int32", "Counter", "Mutable", "Persistent", "PublicSync"});
    auto title_prop = registrator.RegisterProperty({"Common", "string", "Title", "Mutable", "Persistent", "PublicSync"});
    auto enabled_prop = registrator.RegisterProperty({"Common", "bool", "Enabled", "Mutable", "Persistent", "PublicSync"});

    Properties empty_props(&registrator);
    const auto empty_doc = PropertiesSerializator::SaveToDocument(ptr<const Properties>(&empty_props), nullptr, hashes, resolver);
    CHECK(empty_doc.Empty());

    Properties proto(&registrator);
    proto.SetValue<int32_t>(counter_prop, 7);
    proto.SetValue<string>(title_prop, "proto title");

    Properties props(&registrator, &proto);
    props.SetValue<string>(title_prop, "shift lead");
    props.SetValue<bool>(enabled_prop, true);

    const auto doc = PropertiesSerializator::SaveToDocument(ptr<const Properties>(&props), nptr<const Properties>(&proto), hashes, resolver);
    CHECK(doc.Size() == 2);
    CHECK_FALSE(doc.Contains("Counter"));
    REQUIRE(doc.Contains("Title"));
    REQUIRE(doc.Contains("Enabled"));
    CHECK(doc["Title"].AsString() == "shift lead");
    CHECK(doc["Enabled"].AsBool());
}

TEST_CASE("PropertiesLoadFromDocumentSkipsTechnicalAndUnknownFields")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("DocumentLoadEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto counter_prop = registrator.RegisterProperty({"Common", "int32", "Counter", "Mutable", "Persistent", "PublicSync"});
    auto title_prop = registrator.RegisterProperty({"Common", "string", "Title", "Mutable", "Persistent", "PublicSync"});

    AnyData::Document doc;
    doc.Emplace("$version", int64_t {3});
    doc.Emplace("_meta", string {"ignored"});
    doc.Emplace("Counter", int64_t {15});
    doc.Emplace("Title", string {"  south gate  "});
    doc.Emplace("UnknownField", int64_t {99});

    Properties props(&registrator);
    CHECK(PropertiesSerializator::LoadFromDocument(ptr<Properties>(&props), doc, hashes, resolver));
    CHECK(props.GetValue<int32_t>(counter_prop) == 15);
    CHECK(props.GetValue<string>(title_prop) == "  south gate  ");
}

TEST_CASE("PropertiesLoadFromDocumentReportsInvalidFieldButContinues")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("DocumentErrorEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto counter_prop = registrator.RegisterProperty({"Common", "int32", "Counter", "Mutable", "Persistent", "PublicSync"});
    auto enabled_prop = registrator.RegisterProperty({"Common", "bool", "Enabled", "Mutable", "Persistent", "PublicSync"});

    AnyData::Document doc;
    doc.Emplace("Counter", string {"wrong-type"});
    doc.Emplace("Enabled", true);

    Properties props(&registrator);
    CHECK_FALSE(PropertiesSerializator::LoadFromDocument(ptr<Properties>(&props), doc, hashes, resolver));
    CHECK(props.GetValue<int32_t>(counter_prop) == 0);
    CHECK(props.GetValue<bool>(enabled_prop));
}

TEST_CASE("PropertiesLoadFromDocumentRejectsUnsupportedAnyDataValueTypes")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("DocumentTypeErrorEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto counter_prop = registrator.RegisterProperty({"Common", "int32", "Counter", "Mutable", "Persistent", "PublicSync"});
    auto title_prop = registrator.RegisterProperty({"Common", "string", "Title", "Mutable", "Persistent", "PublicSync"});
    auto enabled_prop = registrator.RegisterProperty({"Common", "bool", "Enabled", "Mutable", "Persistent", "PublicSync"});

    AnyData::Document doc;
    doc.Emplace("Counter", AnyData::Value {AnyData::Dict {}});
    doc.Emplace("Title", AnyData::Value {AnyData::Array {}});
    doc.Emplace("Enabled", true);

    Properties props(&registrator);
    CHECK_FALSE(PropertiesSerializator::LoadFromDocument(ptr<Properties>(&props), doc, hashes, resolver));
    CHECK(props.GetValue<int32_t>(counter_prop) == 0);
    CHECK(props.GetValue<string>(title_prop).empty());
    CHECK(props.GetValue<bool>(enabled_prop));
}

TEST_CASE("PropertiesLoadFromDocumentRejectsInvalidHashValueTypes")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("DocumentHashTypeErrorEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto hash_prop = registrator.RegisterProperty({"Common", "hstring", "HashValue", "Mutable", "Persistent", "PublicSync"});
    auto item_prop = registrator.RegisterProperty({"Common", "ProtoItem", "ItemProto", "Mutable", "Persistent", "PublicSync", "Nullable"});
    auto enabled_prop = registrator.RegisterProperty({"Common", "bool", "Enabled", "Mutable", "Persistent", "PublicSync"});

    AnyData::Document doc;
    doc.Emplace("HashValue", AnyData::Value {AnyData::Array {}});
    doc.Emplace("ItemProto", AnyData::Value {int64_t {7}});
    doc.Emplace("Enabled", true);

    Properties props(&registrator);
    CHECK_FALSE(PropertiesSerializator::LoadFromDocument(ptr<Properties>(&props), doc, hashes, resolver));
    CHECK(props.GetValue<hstring>(hash_prop) == hstring {});
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, item_prop, hashes, resolver) == AnyData::Value {string {""}});
    CHECK(props.GetValue<bool>(enabled_prop));
}

TEST_CASE("PropertiesLoadFromDocumentRejectsWrongCollectionValueTypes")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("DocumentCollectionTypeErrorEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto values_prop = registrator.RegisterProperty({"Common", "int32[]", "Values", "Mutable", "Persistent", "PublicSync"});
    auto labels_prop = registrator.RegisterProperty({"Common", "string=>string", "Labels", "Mutable", "Persistent", "PublicSync"});
    auto enabled_prop = registrator.RegisterProperty({"Common", "bool", "Enabled", "Mutable", "Persistent", "PublicSync"});

    AnyData::Document doc;
    doc.Emplace("Values", string {"not-an-array"});
    doc.Emplace("Labels", AnyData::Value {AnyData::Array {}});
    doc.Emplace("Enabled", true);

    Properties props(&registrator);
    CHECK_FALSE(PropertiesSerializator::LoadFromDocument(ptr<Properties>(&props), doc, hashes, resolver));
    CHECK(props.GetValue<vector<int32_t>>(values_prop).empty());
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, labels_prop, hashes, resolver) == AnyData::Value {AnyData::Dict {}});
    CHECK(props.GetValue<bool>(enabled_prop));
}

TEST_CASE("PropertiesLoadFromDocumentRejectsWrongDictArrayValueTypes")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("DocumentDictArrayTypeErrorEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto checkpoints_prop = registrator.RegisterProperty({"Common", "bool=>int32[]", "Checkpoints", "Mutable", "Persistent", "PublicSync"});
    auto enabled_prop = registrator.RegisterProperty({"Common", "bool", "Enabled", "Mutable", "Persistent", "PublicSync"});

    AnyData::Dict invalid_checkpoints;
    invalid_checkpoints.Emplace("True", string {"not-an-array"});

    AnyData::Document doc;
    doc.Emplace("Checkpoints", AnyData::Value {std::move(invalid_checkpoints)});
    doc.Emplace("Enabled", true);

    Properties props(&registrator);
    CHECK_FALSE(PropertiesSerializator::LoadFromDocument(ptr<Properties>(&props), doc, hashes, resolver));
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, checkpoints_prop, hashes, resolver) == AnyData::Value {AnyData::Dict {}});
    CHECK(props.GetValue<bool>(enabled_prop));
}

TEST_CASE("PropertiesLoadFromDocumentRejectsInvalidInnerStringCollectionValues")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("DocumentStringCollectionInnerTypeErrorEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto tags_prop = registrator.RegisterProperty({"Common", "string[]", "Tags", "Mutable", "Persistent", "PublicSync"});
    auto labels_prop = registrator.RegisterProperty({"Common", "string=>string", "Labels", "Mutable", "Persistent", "PublicSync"});
    auto enabled_prop = registrator.RegisterProperty({"Common", "bool", "Enabled", "Mutable", "Persistent", "PublicSync"});

    AnyData::Array invalid_tags;
    invalid_tags.EmplaceBack(AnyData::Value {AnyData::Dict {}});

    AnyData::Dict invalid_labels;
    invalid_labels.Emplace("alpha", AnyData::Value {AnyData::Array {}});

    AnyData::Document doc;
    doc.Emplace("Tags", AnyData::Value {std::move(invalid_tags)});
    doc.Emplace("Labels", AnyData::Value {std::move(invalid_labels)});
    doc.Emplace("Enabled", true);

    Properties props(&registrator);
    CHECK_FALSE(PropertiesSerializator::LoadFromDocument(ptr<Properties>(&props), doc, hashes, resolver));
    CHECK(props.GetValue<vector<string>>(tags_prop).empty());
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, labels_prop, hashes, resolver) == AnyData::Value {AnyData::Dict {}});
    CHECK(props.GetValue<bool>(enabled_prop));
}

TEST_CASE("PropertiesLoadFromDocumentRejectsInvalidInnerDictArrayStringValues")
{
    HashStorage hashes {};
    TestNameResolver resolver;
    PropertyRegistrator registrator("DocumentDictArrayStringInnerTypeErrorEntity", EngineSideKind::ServerSide, &hashes, &resolver);

    auto mode_tags_prop = registrator.RegisterProperty({"Common", "Mode=>string[]", "ModeTags", "Mutable", "Persistent", "PublicSync"});
    auto enabled_prop = registrator.RegisterProperty({"Common", "bool", "Enabled", "Mutable", "Persistent", "PublicSync"});

    AnyData::Array invalid_mode_tags_entries;
    invalid_mode_tags_entries.EmplaceBack(AnyData::Value {AnyData::Dict {}});

    AnyData::Dict invalid_mode_tags;
    invalid_mode_tags.Emplace("ModeA", AnyData::Value {std::move(invalid_mode_tags_entries)});

    AnyData::Document doc;
    doc.Emplace("ModeTags", AnyData::Value {std::move(invalid_mode_tags)});
    doc.Emplace("Enabled", true);

    Properties props(&registrator);
    CHECK_FALSE(PropertiesSerializator::LoadFromDocument(ptr<Properties>(&props), doc, hashes, resolver));
    CHECK(PropertiesSerializator::SavePropertyToValue(&props, mode_tags_prop, hashes, resolver) == AnyData::Value {AnyData::Dict {}});
    CHECK(props.GetValue<bool>(enabled_prop));
}

TEST_CASE("PropertiesPerformance", "[!benchmark][properties]")
{
    PropertiesPerfFixture fixture;
    PropertiesDictPerfFixture dict_fixture;

    CHECK(!fixture.PublicIntProps.empty());
    CHECK(!fixture.PublicStringProps.empty());
    CHECK(!fixture.DerivedPublicChunks.empty());

    BENCHMARK("StoreData full cached public")
    {
        const auto stored_data = fixture.Full.StoreData(false);
        return stored_data.Sizes->size();
    };

    BENCHMARK_ADVANCED("StoreData full after plain mutation")(Catch::Benchmark::Chronometer meter)
    {
        uint32_t counter = 0;

        meter.measure([&](int) {
            fixture.Full.SetValue<int32_t>(fixture.PublicIntProps.front(), numeric_cast<int32_t>(++counter));
            const auto stored_data = fixture.Full.StoreData(false);
            return stored_data.Sizes->at(0);
        });
    };

    BENCHMARK_ADVANCED("RestoreData derived public payload")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            Properties target(&fixture.Registrator, &fixture.Proto);
            target.RestoreData(fixture.DerivedPublicChunks);
            return target.GetValueFast<int32_t>(fixture.PublicIntProps.front());
        });
    };

    BENCHMARK_ADVANCED("StoreAllData full snapshot")(Catch::Benchmark::Chronometer meter)
    {
        vector<uint8_t> all_data;
        set<hstring> str_hashes;

        meter.measure([&](int) {
            fixture.Full.StoreAllData(all_data, str_hashes);
            return all_data.size();
        });
    };

    BENCHMARK_ADVANCED("StoreAllData derived snapshot")(Catch::Benchmark::Chronometer meter)
    {
        vector<uint8_t> all_data;
        set<hstring> str_hashes;

        meter.measure([&](int) {
            fixture.DerivedSource.StoreAllData(all_data, str_hashes);
            return all_data.size();
        });
    };

    BENCHMARK_ADVANCED("RestoreAllData full snapshot")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            Properties target(&fixture.Registrator);
            target.RestoreAllData(fixture.FullAllData);
            return target.GetValueFast<int32_t>(fixture.PublicIntProps.front());
        });
    };

    BENCHMARK_ADVANCED("RestoreAllData derived snapshot")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            Properties target(&fixture.Registrator, &fixture.Proto);
            target.RestoreAllData(fixture.DerivedAllData);
            return target.GetValueFast<int32_t>(fixture.PublicIntProps.front());
        });
    };

    BENCHMARK_ADVANCED("CopyFrom full to derived")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            Properties target(&fixture.Registrator, &fixture.Proto);
            target.CopyFrom(fixture.Full);
            return target.GetValueFast<int32_t>(fixture.PublicIntProps.front());
        });
    };

    Properties left(&fixture.Registrator, &fixture.Proto);
    left.CopyFrom(fixture.DerivedSource);

    Properties right(&fixture.Registrator, &fixture.Proto);
    right.CopyFrom(fixture.DerivedSource);

    BENCHMARK("CompareData same-base derived fast path")
    {
        return left.CompareData(right, {}, false);
    };

    BENCHMARK_ADVANCED("SaveToText dict-rich payload")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            const auto text_data = dict_fixture.Source.SaveToText(nullptr);
            return text_data.size();
        });
    };

    BENCHMARK_ADVANCED("ApplyFromText dict-rich payload")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            Properties target(&dict_fixture.Registrator);
            target.ApplyFromText(dict_fixture.TextData);
            return target.CompareData(dict_fixture.Source, {}, false);
        });
    };

    auto dict_patrol_waypoints_prop = dict_fixture.GetPatrolWaypointsProp();

    BENCHMARK_ADVANCED("SavePropertyToValue struct dict-of-array")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            const auto value = PropertiesSerializator::SavePropertyToValue(&dict_fixture.Source, dict_patrol_waypoints_prop, dict_fixture.Hashes, dict_fixture.Resolver);
            return value.AsDict().Size();
        });
    };

    BENCHMARK_ADVANCED("LoadPropertyFromValue struct dict-of-array")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            Properties target(&dict_fixture.Registrator);
            PropertiesSerializator::LoadPropertyFromValue(&target, dict_patrol_waypoints_prop, dict_fixture.GetPatrolWaypointsValue()->Copy(), dict_fixture.Hashes, dict_fixture.Resolver);
            return target.CompareData(dict_fixture.PatrolOnlySource, {}, false);
        });
    };
}

TEST_CASE("PropertiesStorageStrategyPerformance", "[!benchmark][properties]")
{
    constexpr array scenarios {
        pair {50, 10},
        pair {50, 50},
        pair {500, 10},
        pair {500, 50},
    };

    for (const auto [prop_count, fill_percent] : scenarios) {
        DYNAMIC_SECTION("Props=" << prop_count << ", Fill=" << fill_percent << "%")
        {
            PropertiesStorageStrategyPerfFixture fixture(prop_count, fill_percent);

            CHECK(static_cast<bool>(fixture.ProbeIntProp));
            CHECK(static_cast<bool>(fixture.ProbeStringProp));
            CHECK(numeric_cast<int>(fixture.OverrideProps.size()) == std::max(1, prop_count * fill_percent / 100));
            auto probe_int_prop = fixture.GetProbeIntProp();
            auto probe_string_prop = fixture.GetProbeStringProp();

            BENCHMARK("Read simple int packed")
            {
                return fixture.PackedSource.GetValueFast<int32_t>(probe_int_prop);
            };

            BENCHMARK("Read simple int full")
            {
                return fixture.FullSource.GetValueFast<int32_t>(probe_int_prop);
            };

            BENCHMARK("Read simple string packed")
            {
                return fixture.PackedSource.GetValueFast<string>(probe_string_prop).size();
            };

            BENCHMARK("Read simple string full")
            {
                return fixture.FullSource.GetValueFast<string>(probe_string_prop).size();
            };

            BENCHMARK_ADVANCED("Modify simple int packed")(Catch::Benchmark::Chronometer meter)
            {
                Properties target(&fixture.Registrator, &fixture.Proto);
                int32_t counter = 0;

                meter.measure([&](int) {
                    target.SetValue<int32_t>(probe_int_prop, numeric_cast<int32_t>(10000 + ++counter));
                    return target.GetValueFast<int32_t>(probe_int_prop);
                });
            };

            BENCHMARK_ADVANCED("Modify simple int full")(Catch::Benchmark::Chronometer meter)
            {
                Properties target(&fixture.Registrator);
                target.CopyFrom(fixture.Proto);
                int32_t counter = 0;

                meter.measure([&](int) {
                    target.SetValue<int32_t>(probe_int_prop, numeric_cast<int32_t>(10000 + ++counter));
                    return target.GetValueFast<int32_t>(probe_int_prop);
                });
            };

            BENCHMARK_ADVANCED("Modify simple string packed")(Catch::Benchmark::Chronometer meter)
            {
                Properties target(&fixture.Registrator, &fixture.Proto);
                uint32_t counter = 0;

                meter.measure([&](int) {
                    target.SetValue<string>(probe_string_prop, strex("packed-{}", ++counter).str());
                    return target.GetValueFast<string>(probe_string_prop).size();
                });
            };

            BENCHMARK_ADVANCED("Modify simple string full")(Catch::Benchmark::Chronometer meter)
            {
                Properties target(&fixture.Registrator);
                target.CopyFrom(fixture.Proto);
                uint32_t counter = 0;

                meter.measure([&](int) {
                    target.SetValue<string>(probe_string_prop, strex("full-{}", ++counter).str());
                    return target.GetValueFast<string>(probe_string_prop).size();
                });
            };

            BENCHMARK_ADVANCED("Construct packed from proto and apply overrides")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    Properties target(&fixture.Registrator, &fixture.Proto);
                    fixture.ApplyOverrides(target);
                    return target.GetValueFast<int32_t>(probe_int_prop);
                });
            };

            BENCHMARK_ADVANCED("Construct full from proto and apply overrides")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    Properties target(&fixture.Registrator);
                    target.CopyFrom(fixture.Proto);
                    fixture.ApplyOverrides(target);
                    return target.GetValueFast<int32_t>(probe_int_prop);
                });
            };

            BENCHMARK_ADVANCED("StoreData packed public payload")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    const auto stored_data = fixture.PackedSource.StoreData(false);
                    return stored_data.Sizes->size();
                });
            };

            BENCHMARK_ADVANCED("StoreData full public payload")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    const auto stored_data = fixture.FullSource.StoreData(false);
                    return stored_data.Sizes->size();
                });
            };

            BENCHMARK_ADVANCED("RestoreData packed public payload into fresh object")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    Properties target(&fixture.Registrator, &fixture.Proto);
                    target.RestoreData(fixture.PackedPublicChunks);
                    return target.GetValueFast<int32_t>(probe_int_prop);
                });
            };

            BENCHMARK_ADVANCED("RestoreData full public payload into fresh object")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    Properties target(&fixture.Registrator);
                    target.CopyFrom(fixture.Proto);
                    target.RestoreData(fixture.FullPublicChunks);
                    return target.GetValueFast<int32_t>(probe_int_prop);
                });
            };

            BENCHMARK_ADVANCED("StoreAllData packed snapshot")(Catch::Benchmark::Chronometer meter)
            {
                vector<uint8_t> all_data;
                set<hstring> str_hashes;

                meter.measure([&](int) {
                    fixture.PackedSource.StoreAllData(all_data, str_hashes);
                    return all_data.size();
                });
            };

            BENCHMARK_ADVANCED("StoreAllData full snapshot")(Catch::Benchmark::Chronometer meter)
            {
                vector<uint8_t> all_data;
                set<hstring> str_hashes;

                meter.measure([&](int) {
                    fixture.FullSource.StoreAllData(all_data, str_hashes);
                    return all_data.size();
                });
            };

            BENCHMARK_ADVANCED("RestoreAllData packed snapshot into fresh object")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    Properties target(&fixture.Registrator, &fixture.Proto);
                    target.RestoreAllData(fixture.PackedAllData);
                    return target.GetValueFast<int32_t>(probe_int_prop);
                });
            };

            BENCHMARK_ADVANCED("RestoreAllData full snapshot into fresh object")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    Properties target(&fixture.Registrator);
                    target.RestoreAllData(fixture.FullAllData);
                    return target.GetValueFast<int32_t>(probe_int_prop);
                });
            };
        }
    }
}

TEST_CASE("PropertiesComplexStrategyPerformance", "[!benchmark][properties]")
{
    PropertiesComplexStrategyPerfFixture fixture;

    auto patrol_waypoints_prop = fixture.GetPatrolWaypointsProp();
    auto mode_sets_prop = fixture.GetModeSetsProp();

    BENCHMARK_ADVANCED("Access complex patrol packed as structured value")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            const auto value = PropertiesSerializator::SavePropertyToValue(&fixture.PackedSource, patrol_waypoints_prop, fixture.Hashes, fixture.Resolver);
            return value.AsDict().Size();
        });
    };

    BENCHMARK_ADVANCED("Access complex patrol full as structured value")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            const auto value = PropertiesSerializator::SavePropertyToValue(&fixture.FullSource, patrol_waypoints_prop, fixture.Hashes, fixture.Resolver);
            return value.AsDict().Size();
        });
    };

    BENCHMARK_ADVANCED("Access complex mode sets packed as structured value")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            const auto value = PropertiesSerializator::SavePropertyToValue(&fixture.PackedSource, mode_sets_prop, fixture.Hashes, fixture.Resolver);
            return value.AsDict().Size();
        });
    };

    BENCHMARK_ADVANCED("Access complex mode sets full as structured value")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            const auto value = PropertiesSerializator::SavePropertyToValue(&fixture.FullSource, mode_sets_prop, fixture.Hashes, fixture.Resolver);
            return value.AsDict().Size();
        });
    };

    BENCHMARK_ADVANCED("Modify complex patrol packed via any props")(Catch::Benchmark::Chronometer meter)
    {
        Properties target(&fixture.Registrator, &fixture.Proto);
        uint32_t counter = 0;

        meter.measure([&](int) {
            const auto& text = (++counter % 2 == 0) ? fixture.PatrolOverrideTextA : fixture.PatrolOverrideTextB;
            target.SetValueAsAnyProps(patrol_waypoints_prop->GetRegIndex(), any_t {text});
            return target.GetRawData(patrol_waypoints_prop).size();
        });
    };

    BENCHMARK_ADVANCED("Modify complex patrol full via any props")(Catch::Benchmark::Chronometer meter)
    {
        Properties target(&fixture.Registrator);
        target.CopyFrom(fixture.Proto);
        uint32_t counter = 0;

        meter.measure([&](int) {
            const auto& text = (++counter % 2 == 0) ? fixture.PatrolOverrideTextA : fixture.PatrolOverrideTextB;
            target.SetValueAsAnyProps(patrol_waypoints_prop->GetRegIndex(), any_t {text});
            return target.GetRawData(patrol_waypoints_prop).size();
        });
    };

    BENCHMARK_ADVANCED("Modify complex mode sets packed via any props")(Catch::Benchmark::Chronometer meter)
    {
        Properties target(&fixture.Registrator, &fixture.Proto);
        uint32_t counter = 0;

        meter.measure([&](int) {
            const auto& text = (++counter % 2 == 0) ? fixture.ModeOverrideTextA : fixture.ModeOverrideTextB;
            target.SetValueAsAnyProps(mode_sets_prop->GetRegIndex(), any_t {text});
            return target.GetRawData(mode_sets_prop).size();
        });
    };

    BENCHMARK_ADVANCED("Modify complex mode sets full via any props")(Catch::Benchmark::Chronometer meter)
    {
        Properties target(&fixture.Registrator);
        target.CopyFrom(fixture.Proto);
        uint32_t counter = 0;

        meter.measure([&](int) {
            const auto& text = (++counter % 2 == 0) ? fixture.ModeOverrideTextA : fixture.ModeOverrideTextB;
            target.SetValueAsAnyProps(mode_sets_prop->GetRegIndex(), any_t {text});
            return target.GetRawData(mode_sets_prop).size();
        });
    };
}

TEST_CASE("PropertiesStorageStrategyMetrics", "[properties]")
{
    constexpr array scenarios {
        pair {50, 10},
        pair {50, 50},
        pair {500, 10},
        pair {500, 50},
    };

    for (const auto [prop_count, fill_percent] : scenarios) {
        PropertiesStorageStrategyPerfFixture fixture(prop_count, fill_percent);

        map<string, string> packed_text_data;
        map<string, string> full_text_data;
        size_t packed_text_chars = 0;
        size_t full_text_chars = 0;
        string packed_text_error;
        string full_text_error;

        const auto packed_text_ok = fixture.TryBuildTextData(fixture.PackedSource, packed_text_data, packed_text_chars, packed_text_error);
        const auto full_text_ok = fixture.TryBuildTextData(fixture.FullSource, full_text_data, full_text_chars, full_text_error);
        const auto packed_failing_prop = packed_text_ok ? string {} : fixture.DiagnoseTextSerializationFailure(fixture.PackedSource);
        const auto full_failing_prop = full_text_ok ? string {} : fixture.DiagnoseTextSerializationFailure(fixture.FullSource);

        std::cout << strex("StorageMetrics Props={} Fill={} Overrides={} PackedPublicChunks={} PackedPublicBytes={} FullPublicChunks={} FullPublicBytes={} PackedAllDataBytes={} FullAllDataBytes={} PackedTextOk={} PackedTextEntries={} PackedTextChars={} PackedTextError={} PackedTextFailingProp={} FullTextOk={} FullTextEntries={} FullTextChars={} FullTextError={} FullTextFailingProp={}\n", prop_count, fill_percent, fixture.OverrideProps.size(), fixture.PackedPublicChunks.size(), fixture.PackedPublicBytes, fixture.FullPublicChunks.size(), fixture.FullPublicBytes, fixture.PackedAllData.size(), fixture.FullAllData.size(), packed_text_ok, packed_text_data.size(), packed_text_chars, packed_text_error, packed_failing_prop, full_text_ok, full_text_data.size(), full_text_chars, full_text_error, full_failing_prop).str();

        CHECK(static_cast<bool>(fixture.ProbeIntProp));
        CHECK(static_cast<bool>(fixture.ProbeStringProp));
    }
}

FO_END_NAMESPACE
