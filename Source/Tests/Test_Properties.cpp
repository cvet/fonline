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
            auto make_int32 = []() {
                BaseTypeDesc type;
                type.Name = "int32";
                type.IsPrimitive = true;
                type.IsInt = true;
                type.IsSignedInt = true;
                type.IsInt32 = true;
                type.Size = sizeof(int32);
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
                type.Size = sizeof(float32);
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
                type.Size = sizeof(int32);
                type.EnumUnderlyingType = &_types.at("int32");
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
                type.StructLayout = &_layouts.at("Waypoint");
                type.Size = type.StructLayout->Size;
                type.IsComplexStruct = true;
                return type;
            };

            _types.emplace("int32", make_int32());
            _types.emplace("bool", make_bool());
            _types.emplace("float32", make_float32());
            _types.emplace("string", make_string());
            _types.emplace("hstring", make_hstring());
            _types.emplace("ProtoItem", make_proto("ProtoItem"));
            _types.emplace("ProtoCritter", make_proto("ProtoCritter"));
            _types.emplace("ProtoMap", make_proto("ProtoMap"));
            _types.emplace("ProtoLocation", make_proto("ProtoLocation"));
            _types.emplace("Mode", make_mode());
            _types.emplace("Waypoint", make_waypoint());

            _types.at("ProtoItem").HashedName = _proto_hashes.ToHashedString("ProtoItem");
            _types.at("ProtoCritter").HashedName = _proto_hashes.ToHashedString("ProtoCritter");
            _types.at("ProtoMap").HashedName = _proto_hashes.ToHashedString("ProtoMap");
            _types.at("ProtoLocation").HashedName = _proto_hashes.ToHashedString("ProtoLocation");

            _enum_values.emplace("ModeA", 1);
            _enum_values.emplace("ModeB", 2);
            _enum_names.emplace(1, "ModeA");
            _enum_names.emplace(2, "ModeB");

            _proto_registrator = SafeAlloc::MakeUnique<PropertyRegistrator>("ProtoResolverEntity", EngineSideKind::ServerSide, _proto_hashes, *this);

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

        [[nodiscard]] auto ResolveEnumValue(string_view value_name, bool* failed = nullptr) const -> int32 override
        {
            if (const auto it = _enum_values.find(string(value_name)); it != _enum_values.end()) {
                if (failed != nullptr) {
                    *failed = false;
                }

                return it->second;
            }

            if (failed != nullptr) {
                *failed = true;
                return 0;
            }

            throw EnumResolveException("Enum value is not supported in test resolver");
        }

        [[nodiscard]] auto ResolveEnumValue(string_view, string_view value_name, bool* failed = nullptr) const -> int32 override { return ResolveEnumValue(value_name, failed); }

        [[nodiscard]] auto ResolveEnumValueName(string_view, int32 enum_value, bool* failed = nullptr) const -> const string& override
        {
            if (const auto it = _enum_names.find(enum_value); it != _enum_names.end()) {
                if (failed != nullptr) {
                    *failed = false;
                }

                return it->second;
            }

            if (failed != nullptr) {
                *failed = true;
                return _empty;
            }

            throw EnumResolveException("Enum name is not supported in test resolver");
        }

        [[nodiscard]] auto ResolveGenericValue(string_view, bool* failed = nullptr) const -> int32 override
        {
            if (failed != nullptr) {
                *failed = true;
                return 0;
            }

            throw TypeResolveException("Generic values are not supported in test resolver");
        }

        [[nodiscard]] auto CheckMigrationRule(hstring, hstring, hstring) const noexcept -> optional<hstring> override { return std::nullopt; }
        [[nodiscard]] auto GetProtoEntity(hstring type_name, hstring proto_id) const noexcept -> const ProtoEntity* override
        {
            const auto type_it = _protos.find(type_name.as_hash());

            if (type_it == _protos.end()) {
                return nullptr;
            }

            const auto proto_it = type_it->second.find(proto_id.as_hash());
            return proto_it != type_it->second.end() ? proto_it->second.get() : nullptr;
        }

        void AddProto(string_view type_name, string_view proto_id)
        {
            const auto type_hname = _proto_hashes.ToHashedString(type_name);
            const auto proto_hname = _proto_hashes.ToHashedString(proto_id);

            _protos[type_hname.as_hash()].emplace(proto_hname.as_hash(), SafeAlloc::MakeRefCounted<ProtoCustomEntity>(proto_hname, _proto_registrator.get(), nullptr));
        }

    private:
        unordered_map<string, BaseTypeDesc> _types {};
        unordered_map<string, StructLayoutDesc> _layouts {};
        unordered_map<string, int32> _enum_values {};
        unordered_map<int32, string> _enum_names {};
        HashStorage _proto_hashes {};
        unique_ptr<PropertyRegistrator> _proto_registrator {};
        unordered_map<hstring::hash_t, unordered_map<hstring::hash_t, refcount_ptr<ProtoCustomEntity>>> _protos {};
        string _empty {};
    };

    auto MakeOwnedStoreData(vector<const uint8*>* raw_data, vector<uint32>* raw_sizes) -> vector<vector<uint8>>
    {
        vector<vector<uint8>> result;
        result.reserve(raw_data->size());

        for (size_t i = 0; i < raw_data->size(); i++) {
            const auto* data = (*raw_data)[i];
            const auto size = (*raw_sizes)[i];

            if (size == 0) {
                result.emplace_back();
            }
            else {
                result.emplace_back(data, data + size);
            }
        }

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
        const Property* Prop {};
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
        const Property* ProbeIntProp {};
        const Property* ProbeStringProp {};
        unique_ptr<Properties> Proto {};
        unique_ptr<Properties> PackedSource {};
        unique_ptr<Properties> FullSource {};
        vector<vector<uint8>> PackedPublicChunks {};
        vector<vector<uint8>> FullPublicChunks {};
        vector<uint8> PackedAllData {};
        vector<uint8> FullAllData {};
        size_t PackedPublicBytes {};
        size_t FullPublicBytes {};
        int TotalProps {};
        int FillPercent {};

        PropertiesStorageStrategyPerfFixture(int total_props, int fill_percent) :
            Registrator("PerfStorageEntity", EngineSideKind::ServerSide, Hashes, Resolver),
            TotalProps(total_props),
            FillPercent(fill_percent)
        {
            FO_RUNTIME_ASSERT(total_props > 0);
            FO_RUNTIME_ASSERT(fill_percent > 0);

            Props.reserve(numeric_cast<size_t>(total_props));

            for (int i = 0; i < total_props; i++) {
                const auto kind = static_cast<PerfPropertyKind>(i % 5);
                auto* prop = RegisterProperty(kind, i);

                Props.emplace_back(PerfPropertySpec {prop, kind, i});

                if (ProbeIntProp == nullptr && kind == PerfPropertyKind::PublicInt) {
                    ProbeIntProp = prop;
                }
                if (ProbeStringProp == nullptr && kind == PerfPropertyKind::PublicString) {
                    ProbeStringProp = prop;
                }
            }

            FO_RUNTIME_ASSERT(ProbeIntProp != nullptr);
            FO_RUNTIME_ASSERT(ProbeStringProp != nullptr);

            Proto = unique_ptr<Properties>(new Properties(&Registrator));

            for (const auto& spec : Props) {
                SetProtoValue(*Proto, spec);
            }

            PackedSource = unique_ptr<Properties>(new Properties(&Registrator, Proto.get()));
            FullSource = unique_ptr<Properties>(new Properties(&Registrator));
            FullSource->CopyFrom(*Proto);

            const auto override_count = std::max(1, total_props * fill_percent / 100);
            OverrideProps.reserve(numeric_cast<size_t>(override_count));

            for (int i = 0; i < override_count; i++) {
                const auto& spec = Props[numeric_cast<size_t>(i)];
                OverrideProps.emplace_back(spec);
                SetOverrideValue(*PackedSource, spec);
                SetOverrideValue(*FullSource, spec);
            }

            vector<const uint8*>* raw_data = nullptr;
            vector<uint32>* raw_sizes = nullptr;

            PackedSource->StoreData(false, &raw_data, &raw_sizes);
            PackedPublicChunks = MakeOwnedStoreData(raw_data, raw_sizes);
            PackedPublicBytes = CalcTotalBytes(PackedPublicChunks);

            FullSource->StoreData(false, &raw_data, &raw_sizes);
            FullPublicChunks = MakeOwnedStoreData(raw_data, raw_sizes);
            FullPublicBytes = CalcTotalBytes(FullPublicChunks);

            set<hstring> str_hashes;
            PackedSource->StoreAllData(PackedAllData, str_hashes);
            FullSource->StoreAllData(FullAllData, str_hashes);
        }

        void ApplyOverrides(Properties& props) const
        {
            for (const auto& spec : OverrideProps) {
                SetOverrideValue(props, spec);
            }
        }

        [[nodiscard]] static auto CalcTotalBytes(const vector<vector<uint8>>& chunks) -> size_t
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
                const auto* prop = Registrator.GetPropertyByIndex(numeric_cast<int32>(i));

                if (prop->IsDisabled() || !prop->IsPersistent()) {
                    continue;
                }

                try {
                    const auto text = PropertiesSerializator::SavePropertyToText(&props, prop, Hashes, Resolver);
                    static_cast<void>(text);
                }
                catch (const std::exception& ex) {
                    return strex("{} ({})", prop->GetName(), ex.what()).str();
                }
            }

            return {};
        }

    private:
        auto RegisterProperty(PerfPropertyKind kind, int index) -> const Property*
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
            switch (spec.Kind) {
            case PerfPropertyKind::PublicInt:
                props.SetValue<int32>(spec.Prop, numeric_cast<int32>(spec.Index * 2));
                break;
            case PerfPropertyKind::PublicString:
                props.SetValue<string>(spec.Prop, strex("proto-{}-squad", spec.Index));
                break;
            case PerfPropertyKind::OwnerBool:
                props.SetValue<bool>(spec.Prop, spec.Index % 2 == 0);
                break;
            case PerfPropertyKind::PublicFloat:
                props.SetValue<float32>(spec.Prop, static_cast<float32>(spec.Index) * 0.25f);
                break;
            case PerfPropertyKind::LocalInt:
                props.SetValue<int32>(spec.Prop, numeric_cast<int32>(spec.Index % 100));
                break;
            }
        }

        static void SetOverrideValue(Properties& props, const PerfPropertySpec& spec)
        {
            switch (spec.Kind) {
            case PerfPropertyKind::PublicInt:
                props.SetValue<int32>(spec.Prop, numeric_cast<int32>(1000 + spec.Index * 3));
                break;
            case PerfPropertyKind::PublicString:
                props.SetValue<string>(spec.Prop, strex("override-{}-payload", spec.Index));
                break;
            case PerfPropertyKind::OwnerBool:
                props.SetValue<bool>(spec.Prop, spec.Index % 3 == 0);
                break;
            case PerfPropertyKind::PublicFloat:
                props.SetValue<float32>(spec.Prop, 10.0f + static_cast<float32>(spec.Index) * 0.5f);
                break;
            case PerfPropertyKind::LocalInt:
                props.SetValue<int32>(spec.Prop, numeric_cast<int32>((spec.Index * 7) % 300));
                break;
            }
        }
    };

    struct PropertiesComplexStrategyPerfFixture
    {
        HashStorage Hashes {};
        TestNameResolver Resolver {};
        PropertyRegistrator Registrator;
        const Property* PatrolWaypointsProp {};
        const Property* ModeSetsProp {};
        string PatrolProtoText {};
        string PatrolOverrideTextA {};
        string PatrolOverrideTextB {};
        string ModeProtoText {};
        string ModeOverrideTextA {};
        string ModeOverrideTextB {};
        unique_ptr<Properties> Proto {};
        unique_ptr<Properties> PackedSource {};
        unique_ptr<Properties> FullSource {};

        PropertiesComplexStrategyPerfFixture() :
            Registrator("PerfComplexStorageEntity", EngineSideKind::ServerSide, Hashes, Resolver)
        {
            PatrolWaypointsProp = Registrator.RegisterProperty({"Common", "int32=>Waypoint[]", "PatrolWaypoints", "Mutable", "Persistent", "PublicSync"});
            ModeSetsProp = Registrator.RegisterProperty({"Common", "string=>Mode[]", "ModeSets", "Mutable", "Persistent", "PublicSync"});

            PatrolProtoText = MakePatrolValue(1, 2);
            PatrolOverrideTextA = MakePatrolValue(10, 20);
            PatrolOverrideTextB = MakePatrolValue(30, 40);
            ModeProtoText = MakeModeSetsValue("primary", "reserve");
            ModeOverrideTextA = MakeModeSetsValue("alpha", "beta");
            ModeOverrideTextB = MakeModeSetsValue("gamma", "delta");

            Proto = unique_ptr<Properties>(new Properties(&Registrator));
            Proto->SetValueAsAnyProps(PatrolWaypointsProp->GetRegIndex(), any_t {PatrolProtoText});
            Proto->SetValueAsAnyProps(ModeSetsProp->GetRegIndex(), any_t {ModeProtoText});

            PackedSource = unique_ptr<Properties>(new Properties(&Registrator, Proto.get()));
            FullSource = unique_ptr<Properties>(new Properties(&Registrator));
            FullSource->CopyFrom(*Proto);

            PackedSource->SetValueAsAnyProps(PatrolWaypointsProp->GetRegIndex(), any_t {PatrolOverrideTextA});
            PackedSource->SetValueAsAnyProps(ModeSetsProp->GetRegIndex(), any_t {ModeOverrideTextA});
            FullSource->SetValueAsAnyProps(PatrolWaypointsProp->GetRegIndex(), any_t {PatrolOverrideTextA});
            FullSource->SetValueAsAnyProps(ModeSetsProp->GetRegIndex(), any_t {ModeOverrideTextA});
        }

    private:
        static auto MakePatrolValue(int first_base, int second_base) -> string
        {
            AnyData::Array first_path;

            AnyData::Array first_a;
            first_a.EmplaceBack(int64 {first_base});
            first_a.EmplaceBack(float64 {0.5});
            first_a.EmplaceBack(true);
            first_path.EmplaceBack(AnyData::Value {std::move(first_a)});

            AnyData::Array first_b;
            first_b.EmplaceBack(int64 {first_base + 1});
            first_b.EmplaceBack(float64 {1.25});
            first_b.EmplaceBack(false);
            first_path.EmplaceBack(AnyData::Value {std::move(first_b)});

            AnyData::Array second_path;

            AnyData::Array second_a;
            second_a.EmplaceBack(int64 {second_base});
            second_a.EmplaceBack(float64 {2.5});
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
        vector<const Property*> PublicIntProps {};
        vector<const Property*> PublicStringProps {};
        vector<const Property*> OwnerBoolProps {};
        unique_ptr<Properties> Proto {};
        unique_ptr<Properties> Full {};
        unique_ptr<Properties> DerivedSource {};
        vector<vector<uint8>> DerivedPublicChunks {};
        vector<uint8> FullAllData {};
        vector<uint8> DerivedAllData {};

        PropertiesPerfFixture() :
            Registrator("PerfEntity", EngineSideKind::ServerSide, Hashes, Resolver)
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

            Proto = unique_ptr<Properties>(new Properties(&Registrator));
            Full = unique_ptr<Properties>(new Properties(&Registrator));

            for (size_t i = 0; i < PublicIntProps.size(); i++) {
                Proto->SetValue<int32>(PublicIntProps[i], numeric_cast<int32>(i));
                Full->SetValue<int32>(PublicIntProps[i], numeric_cast<int32>(i * 3 + 1));
            }

            for (size_t i = 0; i < PublicStringProps.size(); i++) {
                string proto_value {"proto-"};
                proto_value += std::to_string(i).c_str();
                string full_value {"full-"};
                full_value += std::to_string(i).c_str();
                Proto->SetValue<string>(PublicStringProps[i], proto_value);
                Full->SetValue<string>(PublicStringProps[i], full_value);
            }

            for (size_t i = 0; i < OwnerBoolProps.size(); i++) {
                Proto->SetValue<bool>(OwnerBoolProps[i], false);
                Full->SetValue<bool>(OwnerBoolProps[i], i % 2 == 0);
            }

            DerivedSource = unique_ptr<Properties>(new Properties(&Registrator, Proto.get()));

            for (size_t i = 0; i < PublicIntProps.size(); i++) {
                DerivedSource->SetValue<int32>(PublicIntProps[i], numeric_cast<int32>(100 + i));
            }

            for (size_t i = 0; i < PublicStringProps.size(); i++) {
                string override_value {"derived-"};
                override_value += std::to_string(i).c_str();
                DerivedSource->SetValue<string>(PublicStringProps[i], override_value);
            }

            for (size_t i = 0; i < OwnerBoolProps.size(); i++) {
                DerivedSource->SetValue<bool>(OwnerBoolProps[i], i % 3 == 0);
            }

            vector<const uint8*>* raw_data = nullptr;
            vector<uint32>* raw_sizes = nullptr;
            DerivedSource->StoreData(false, &raw_data, &raw_sizes);
            DerivedPublicChunks = MakeOwnedStoreData(raw_data, raw_sizes);

            Full->StoreData(false, &raw_data, &raw_sizes);

            set<hstring> str_hashes;
            Full->StoreAllData(FullAllData, str_hashes);
            DerivedSource->StoreAllData(DerivedAllData, str_hashes);
        }
    };

    struct PropertiesDictPerfFixture
    {
        HashStorage Hashes {};
        TestNameResolver Resolver {};
        PropertyRegistrator Registrator;
        const Property* FloatLabelsProp {};
        const Property* PatrolWaypointsProp {};
        const Property* ModeSetsProp {};
        unique_ptr<Properties> Source {};
        unique_ptr<Properties> PatrolOnlySource {};
        map<string, string> TextData {};
        unique_ptr<AnyData::Value> PatrolWaypointsValue {};

        PropertiesDictPerfFixture() :
            Registrator("PerfDictEntity", EngineSideKind::ServerSide, Hashes, Resolver)
        {
            FloatLabelsProp = Registrator.RegisterProperty({"Common", "float32=>string", "Labels", "Mutable", "Persistent", "PublicSync"});
            PatrolWaypointsProp = Registrator.RegisterProperty({"Common", "int32=>Waypoint[]", "PatrolWaypoints", "Mutable", "Persistent", "PublicSync"});
            ModeSetsProp = Registrator.RegisterProperty({"Common", "string=>Mode[]", "ModeSets", "Mutable", "Persistent", "PublicSync"});

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
                first_path_point_a.EmplaceBack(int64 {1});
                first_path_point_a.EmplaceBack(float64 {0.5});
                first_path_point_a.EmplaceBack(true);
                first_path.EmplaceBack(AnyData::Value {std::move(first_path_point_a)});

                AnyData::Array first_path_point_b;
                first_path_point_b.EmplaceBack(int64 {2});
                first_path_point_b.EmplaceBack(float64 {1.25});
                first_path_point_b.EmplaceBack(false);
                first_path.EmplaceBack(AnyData::Value {std::move(first_path_point_b)});

                AnyData::Array second_path;

                AnyData::Array second_path_point_a;
                second_path_point_a.EmplaceBack(int64 {7});
                second_path_point_a.EmplaceBack(float64 {4.75});
                second_path_point_a.EmplaceBack(true);
                second_path.EmplaceBack(AnyData::Value {std::move(second_path_point_a)});

                AnyData::Array second_path_point_b;
                second_path_point_b.EmplaceBack(int64 {8});
                second_path_point_b.EmplaceBack(float64 {9.5});
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

            Source = unique_ptr<Properties>(new Properties(&Registrator));
            Source->SetValueAsAnyProps(FloatLabelsProp->GetRegIndex(), any_t {AnyData::ValueToString(labels_value)});
            Source->SetValueAsAnyProps(PatrolWaypointsProp->GetRegIndex(), any_t {AnyData::ValueToString(patrol_value)});
            Source->SetValueAsAnyProps(ModeSetsProp->GetRegIndex(), any_t {AnyData::ValueToString(mode_sets_value)});

            PatrolOnlySource = unique_ptr<Properties>(new Properties(&Registrator));
            PatrolOnlySource->SetValueAsAnyProps(PatrolWaypointsProp->GetRegIndex(), any_t {AnyData::ValueToString(patrol_value)});

            TextData = Source->SaveToText(nullptr);
            PatrolWaypointsValue = unique_ptr<AnyData::Value>(new AnyData::Value(PropertiesSerializator::SavePropertyToValue(Source.get(), PatrolWaypointsProp, Hashes, Resolver)));
        }
    };
}

TEST_CASE("PropertiesOverlay")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("TestEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* value_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    const auto* flag_prop = registrator.RegisterProperty({"Common", "bool", "Flag", "Mutable", "Persistent", "PublicSync"});
    const auto* name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});

    Properties proto(&registrator);
    proto.SetValue<int32>(value_prop, 10);
    proto.SetValue<bool>(flag_prop, true);
    proto.SetValue<string>(name_prop, "proto-name");

    SECTION("ReadsInheritedValuesAndDropsMatchingOverride")
    {
        Properties props(&registrator, &proto);

        CHECK(props.HasBaseProperties());
        CHECK(props.GetValue<int32>(value_prop) == 10);
        CHECK(props.GetValue<bool>(flag_prop));
        CHECK(props.GetValue<string>(name_prop) == "proto-name");

        props.SetValue<int32>(value_prop, 25);
        CHECK(props.GetValue<int32>(value_prop) == 25);
        CHECK(proto.GetValue<int32>(value_prop) == 10);

        props.SetValue<int32>(value_prop, 10);
        CHECK(props.GetValue<int32>(value_prop) == 10);

        props.SetValue<string>(name_prop, "proto-name");
        CHECK(props.GetValue<string>(name_prop) == "proto-name");
        CHECK(props.SaveToText(&proto).empty());
    }

    SECTION("StoresAndRestoresSparseOverrides")
    {
        Properties props(&registrator, &proto);
        props.SetValue<int32>(value_prop, 42);
        props.SetValue<bool>(flag_prop, false);
        props.SetValue<string>(name_prop, "");

        auto diff = props.SaveToText(&proto);
        CHECK(diff.contains("Value"));
        CHECK(diff.contains("Flag"));
        CHECK(diff.contains("Name"));

        vector<const uint8*>* raw_data = nullptr;
        vector<uint32>* raw_sizes = nullptr;
        props.StoreData(false, &raw_data, &raw_sizes);

        auto owned_chunks = MakeOwnedStoreData(raw_data, raw_sizes);

        Properties restored(&registrator, &proto);
        restored.RestoreData(owned_chunks);

        CHECK(restored.GetValue<int32>(value_prop) == 42);
        CHECK_FALSE(restored.GetValue<bool>(flag_prop));
        CHECK(restored.GetValue<string>(name_prop).empty());

        auto copied = props.Copy();
        CHECK(copied.HasBaseProperties());
        CHECK(copied.GetValue<int32>(value_prop) == 42);
        CHECK_FALSE(copied.GetValue<bool>(flag_prop));
        CHECK(copied.GetValue<string>(name_prop).empty());

        Properties snapshot(&registrator);
        snapshot.CopyFrom(props);
        snapshot.CopyFrom(props);
        CHECK(snapshot.GetValue<int32>(value_prop) == 42);
        CHECK_FALSE(snapshot.GetValue<bool>(flag_prop));
        CHECK(snapshot.GetValue<string>(name_prop).empty());

        props.SetValue<string>(name_prop, "mutated-source");
        CHECK(snapshot.GetValue<string>(name_prop).empty());
    }

    SECTION("CopyFromFullSourceBuildsCanonicalDerivedOverlay")
    {
        Properties source(&registrator);
        source.SetValue<int32>(value_prop, 42);
        source.SetValue<bool>(flag_prop, true);
        source.SetValue<string>(name_prop, "full-name");

        Properties derived(&registrator, &proto);
        derived.CopyFrom(source);
        derived.CopyFrom(source);

        CHECK(derived.GetValue<int32>(value_prop) == 42);
        CHECK(derived.GetValue<bool>(flag_prop));
        CHECK(derived.GetValue<string>(name_prop) == "full-name");

        vector<uint8> all_data;
        set<hstring> str_hashes;
        derived.StoreAllData(all_data, str_hashes);

        DataReader reader(all_data);
        CHECK(reader.Read<uint32>() == registrator.GetWholeDataSize());
        CHECK(reader.Read<bool>());
        CHECK(reader.Read<uint32>() == 2);
    }

    SECTION("StoreAllDataUsesOverlayBaseBackedData")
    {
        Properties props(&registrator, &proto);
        props.SetValue<int32>(value_prop, 77);
        props.SetValue<bool>(flag_prop, false);
        props.SetValue<string>(name_prop, "override-name");

        vector<uint8> all_data;
        set<hstring> str_hashes;
        props.StoreAllData(all_data, str_hashes);

        DataReader reader(all_data);
        CHECK(reader.Read<uint32>() == registrator.GetWholeDataSize());
        CHECK(reader.Read<bool>());

        CHECK(reader.Read<uint32>() == 3);

        const auto expected_size = sizeof(uint32) + sizeof(bool) + sizeof(uint32) + 3 * (sizeof(uint16) + sizeof(uint32)) + props.GetRawData(value_prop).size() + props.GetRawData(flag_prop).size() + props.GetRawData(name_prop).size();
        CHECK(all_data.size() == expected_size);

        Properties restored(&registrator, &proto);
        restored.RestoreAllData(all_data);

        CHECK(restored.GetValue<int32>(value_prop) == 77);
        CHECK_FALSE(restored.GetValue<bool>(flag_prop));
        CHECK(restored.GetValue<string>(name_prop) == "override-name");
    }

    SECTION("RepeatedPlainWritesKeepCanonicalOverlayState")
    {
        Properties props(&registrator, &proto);

        props.SetValue<bool>(flag_prop, false);
        props.SetValue<bool>(flag_prop, false);
        props.SetValue<int32>(value_prop, 77);
        props.SetValue<int32>(value_prop, 77);

        vector<uint8> all_data;
        set<hstring> str_hashes;
        props.StoreAllData(all_data, str_hashes);

        DataReader reader(all_data);
        CHECK(reader.Read<uint32>() == registrator.GetWholeDataSize());
        CHECK(reader.Read<bool>());
        CHECK(reader.Read<uint32>() == 2);

        props.SetValue<bool>(flag_prop, true);
        props.SetValue<bool>(flag_prop, true);
        props.SetValue<int32>(value_prop, 10);
        props.SetValue<int32>(value_prop, 10);

        props.StoreAllData(all_data, str_hashes);

        DataReader empty_reader(all_data);
        CHECK(empty_reader.Read<uint32>() == registrator.GetWholeDataSize());
        CHECK(empty_reader.Read<bool>());
        CHECK(empty_reader.Read<uint32>() == 0);
        empty_reader.VerifyEnd();
    }

    SECTION("RestoreAllDataRejectsMismatchedStorageMode")
    {
        vector<uint8> full_data;
        set<hstring> str_hashes;
        proto.StoreAllData(full_data, str_hashes);

        Properties derived(&registrator, &proto);
        CHECK_THROWS(derived.RestoreAllData(full_data));

        Properties derived_data(&registrator, &proto);
        derived_data.SetValue<int32>(value_prop, 55);

        vector<uint8> diff_data;
        derived_data.StoreAllData(diff_data, str_hashes);

        Properties full_props(&registrator);
        CHECK_THROWS(full_props.RestoreAllData(diff_data));
    }
}

TEST_CASE("PropertiesOverlayFiltersAndCopies")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("SyncEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* public_value_prop = registrator.RegisterProperty({"Common", "int32", "PublicValue", "Mutable", "Persistent", "PublicSync"});
    const auto* owner_flag_prop = registrator.RegisterProperty({"Common", "bool", "OwnerFlag", "Mutable", "Persistent", "OwnerSync"});
    const auto* public_name_prop = registrator.RegisterProperty({"Common", "string", "PublicName", "Mutable", "Persistent", "PublicSync"});

    Properties proto(&registrator);
    proto.SetValue<int32>(public_value_prop, 10);
    proto.SetValue<bool>(owner_flag_prop, true);
    proto.SetValue<string>(public_name_prop, "base");

    SECTION("StoreDataPublicOnlySkipsOwnerSyncOverrides")
    {
        Properties props(&registrator, &proto);
        props.SetValue<int32>(public_value_prop, 42);
        props.SetValue<bool>(owner_flag_prop, false);
        props.SetValue<string>(public_name_prop, "public-override");

        vector<const uint8*>* raw_data = nullptr;
        vector<uint32>* raw_sizes = nullptr;
        props.StoreData(false, &raw_data, &raw_sizes);

        REQUIRE(raw_data != nullptr);
        REQUIRE(raw_sizes != nullptr);
        REQUIRE(raw_data->size() == 4);
        REQUIRE(raw_sizes->size() == 4);
        CHECK((*raw_sizes)[0] == sizeof(uint8));
        CHECK((*raw_sizes)[1] == sizeof(uint16) * 2);

        uint8 store_type = 0xFF;
        MemCopy(&store_type, raw_data->at(0), sizeof(store_type));
        CHECK(store_type == 1);

        auto owned_chunks = MakeOwnedStoreData(raw_data, raw_sizes);

        Properties restored(&registrator, &proto);
        restored.RestoreData(owned_chunks);

        CHECK(restored.GetValue<int32>(public_value_prop) == 42);
        CHECK(restored.GetValue<bool>(owner_flag_prop));
        CHECK(restored.GetValue<string>(public_name_prop) == "public-override");

        Properties restored_full(&registrator);
        restored_full.CopyFrom(proto);
        restored_full.RestoreData(owned_chunks);

        CHECK(restored_full.GetValue<int32>(public_value_prop) == 42);
        CHECK(restored_full.GetValue<bool>(owner_flag_prop));
        CHECK(restored_full.GetValue<string>(public_name_prop) == "public-override");
    }

    SECTION("StoreDataWithProtectedKeepsOwnerSyncOverrides")
    {
        Properties props(&registrator, &proto);
        props.SetValue<int32>(public_value_prop, 42);
        props.SetValue<bool>(owner_flag_prop, false);
        props.SetValue<string>(public_name_prop, "public-override");

        vector<const uint8*>* raw_data = nullptr;
        vector<uint32>* raw_sizes = nullptr;
        props.StoreData(true, &raw_data, &raw_sizes);

        REQUIRE(raw_data != nullptr);
        REQUIRE(raw_sizes != nullptr);
        REQUIRE(raw_data->size() == 5);
        REQUIRE(raw_sizes->size() == 5);
        CHECK((*raw_sizes)[0] == sizeof(uint8));
        CHECK((*raw_sizes)[1] == sizeof(uint16) * 3);

        uint8 store_type = 0xFF;
        MemCopy(&store_type, raw_data->at(0), sizeof(store_type));
        CHECK(store_type == 1);

        auto owned_chunks = MakeOwnedStoreData(raw_data, raw_sizes);

        Properties restored(&registrator, &proto);
        restored.RestoreData(owned_chunks);

        CHECK(restored.GetValue<int32>(public_value_prop) == 42);
        CHECK_FALSE(restored.GetValue<bool>(owner_flag_prop));
        CHECK(restored.GetValue<string>(public_name_prop) == "public-override");

        Properties restored_full(&registrator);
        restored_full.CopyFrom(proto);
        restored_full.RestoreData(owned_chunks);

        CHECK(restored_full.GetValue<int32>(public_value_prop) == 42);
        CHECK_FALSE(restored_full.GetValue<bool>(owner_flag_prop));
        CHECK(restored_full.GetValue<string>(public_name_prop) == "public-override");
    }

    SECTION("StoreDataPreservesZeroSizedOverlayEntry")
    {
        Properties props(&registrator, &proto);
        props.SetValue<string>(public_name_prop, "");

        vector<const uint8*>* raw_data = nullptr;
        vector<uint32>* raw_sizes = nullptr;
        props.StoreData(false, &raw_data, &raw_sizes);

        REQUIRE(raw_data != nullptr);
        REQUIRE(raw_sizes != nullptr);
        REQUIRE(raw_data->size() == 3);
        REQUIRE(raw_sizes->size() == 3);
        CHECK((*raw_sizes)[0] == sizeof(uint8));
        CHECK((*raw_sizes)[1] == sizeof(uint16));
        CHECK((*raw_sizes)[2] == 0);

        uint8 store_type = 0xFF;
        MemCopy(&store_type, raw_data->at(0), sizeof(store_type));
        CHECK(store_type == 1);

        auto owned_chunks = MakeOwnedStoreData(raw_data, raw_sizes);

        Properties restored(&registrator, &proto);
        restored.RestoreData(owned_chunks);

        CHECK(restored.GetValue<string>(public_name_prop).empty());
    }

    SECTION("StoreDataKeepsSeparateCachesPerProtectionMode")
    {
        Properties props(&registrator, &proto);
        props.SetValue<int32>(public_value_prop, 42);
        props.SetValue<bool>(owner_flag_prop, false);
        props.SetValue<string>(public_name_prop, "public-override");

        vector<const uint8*>* public_data = nullptr;
        vector<uint32>* public_sizes = nullptr;
        props.StoreData(false, &public_data, &public_sizes);

        vector<const uint8*>* protected_data = nullptr;
        vector<uint32>* protected_sizes = nullptr;
        props.StoreData(true, &protected_data, &protected_sizes);

        vector<const uint8*>* public_data_again = nullptr;
        vector<uint32>* public_sizes_again = nullptr;
        props.StoreData(false, &public_data_again, &public_sizes_again);

        REQUIRE(public_data != nullptr);
        REQUIRE(public_sizes != nullptr);
        REQUIRE(protected_data != nullptr);
        REQUIRE(protected_sizes != nullptr);
        CHECK(public_data != protected_data);
        CHECK(public_sizes != protected_sizes);
        CHECK(public_data == public_data_again);
        CHECK(public_sizes == public_sizes_again);

        auto public_chunks = MakeOwnedStoreData(public_data, public_sizes);
        auto protected_chunks = MakeOwnedStoreData(protected_data, protected_sizes);

        Properties restored_public(&registrator, &proto);
        restored_public.RestoreData(public_chunks);
        CHECK(restored_public.GetValue<int32>(public_value_prop) == 42);
        CHECK(restored_public.GetValue<bool>(owner_flag_prop));
        CHECK(restored_public.GetValue<string>(public_name_prop) == "public-override");

        Properties restored_protected(&registrator, &proto);
        restored_protected.RestoreData(protected_chunks);
        CHECK(restored_protected.GetValue<int32>(public_value_prop) == 42);
        CHECK_FALSE(restored_protected.GetValue<bool>(owner_flag_prop));
        CHECK(restored_protected.GetValue<string>(public_name_prop) == "public-override");
    }

    SECTION("StoreDataCacheInvalidatesAfterMutationAndRestore")
    {
        Properties derived(&registrator, &proto);
        derived.SetValue<int32>(public_value_prop, 42);

        vector<const uint8*>* derived_before_data = nullptr;
        vector<uint32>* derived_before_sizes = nullptr;
        derived.StoreData(false, &derived_before_data, &derived_before_sizes);
        auto derived_before_chunks = MakeOwnedStoreData(derived_before_data, derived_before_sizes);

        derived.SetValue<string>(public_name_prop, "changed");

        vector<const uint8*>* derived_after_data = nullptr;
        vector<uint32>* derived_after_sizes = nullptr;
        derived.StoreData(false, &derived_after_data, &derived_after_sizes);
        auto derived_after_chunks = MakeOwnedStoreData(derived_after_data, derived_after_sizes);

        Properties restored_before(&registrator, &proto);
        restored_before.RestoreData(derived_before_chunks);
        CHECK(restored_before.GetValue<int32>(public_value_prop) == 42);
        CHECK(restored_before.GetValue<string>(public_name_prop) == "base");

        Properties restored_after(&registrator, &proto);
        restored_after.RestoreData(derived_after_chunks);
        CHECK(restored_after.GetValue<int32>(public_value_prop) == 42);
        CHECK(restored_after.GetValue<string>(public_name_prop) == "changed");

        Properties full_target(&registrator);
        full_target.SetValue<int32>(public_value_prop, 10);
        full_target.SetValue<bool>(owner_flag_prop, true);
        full_target.SetValue<string>(public_name_prop, "base");

        vector<const uint8*>* full_before_data = nullptr;
        vector<uint32>* full_before_sizes = nullptr;
        full_target.StoreData(false, &full_before_data, &full_before_sizes);

        Properties full_source(&registrator);
        full_source.SetValue<int32>(public_value_prop, 77);
        full_source.SetValue<bool>(owner_flag_prop, false);
        full_source.SetValue<string>(public_name_prop, "restored");

        vector<const uint8*>* full_source_data = nullptr;
        vector<uint32>* full_source_sizes = nullptr;
        full_source.StoreData(false, &full_source_data, &full_source_sizes);
        auto full_source_chunks = MakeOwnedStoreData(full_source_data, full_source_sizes);

        full_target.RestoreData(full_source_chunks);

        vector<const uint8*>* full_after_data = nullptr;
        vector<uint32>* full_after_sizes = nullptr;
        full_target.StoreData(false, &full_after_data, &full_after_sizes);
        auto full_after_chunks = MakeOwnedStoreData(full_after_data, full_after_sizes);

        Properties full_restored(&registrator);
        full_restored.RestoreData(full_after_chunks);
        CHECK(full_restored.GetValue<int32>(public_value_prop) == 77);
        CHECK_FALSE(full_restored.GetValue<bool>(owner_flag_prop));
        CHECK(full_restored.GetValue<string>(public_name_prop) == "restored");
    }

    SECTION("StoreDataCacheInvalidatesOnOwnerOnlyMutation")
    {
        Properties props(&registrator);
        props.SetValue<int32>(public_value_prop, 42);
        props.SetValue<bool>(owner_flag_prop, false);
        props.SetValue<string>(public_name_prop, "public-override");

        vector<const uint8*>* public_before_data = nullptr;
        vector<uint32>* public_before_sizes = nullptr;
        props.StoreData(false, &public_before_data, &public_before_sizes);
        const auto public_before_chunks = MakeOwnedStoreData(public_before_data, public_before_sizes);

        vector<const uint8*>* protected_before_data = nullptr;
        vector<uint32>* protected_before_sizes = nullptr;
        props.StoreData(true, &protected_before_data, &protected_before_sizes);
        const auto protected_before_chunks = MakeOwnedStoreData(protected_before_data, protected_before_sizes);

        props.SetValue<bool>(owner_flag_prop, true);

        vector<const uint8*>* public_after_data = nullptr;
        vector<uint32>* public_after_sizes = nullptr;
        props.StoreData(false, &public_after_data, &public_after_sizes);
        const auto public_after_chunks = MakeOwnedStoreData(public_after_data, public_after_sizes);

        vector<const uint8*>* protected_after_data = nullptr;
        vector<uint32>* protected_after_sizes = nullptr;
        props.StoreData(true, &protected_after_data, &protected_after_sizes);
        const auto protected_after_chunks = MakeOwnedStoreData(protected_after_data, protected_after_sizes);

        Properties restored_public_before(&registrator);
        restored_public_before.RestoreData(public_before_chunks);
        CHECK(restored_public_before.GetValue<int32>(public_value_prop) == 42);
        CHECK_FALSE(restored_public_before.GetValue<bool>(owner_flag_prop));
        CHECK(restored_public_before.GetValue<string>(public_name_prop) == "public-override");

        Properties restored_public_after(&registrator);
        restored_public_after.RestoreData(public_after_chunks);
        CHECK(restored_public_after.GetValue<int32>(public_value_prop) == 42);
        CHECK_FALSE(restored_public_after.GetValue<bool>(owner_flag_prop));
        CHECK(restored_public_after.GetValue<string>(public_name_prop) == "public-override");

        Properties restored_protected_before(&registrator);
        restored_protected_before.RestoreData(protected_before_chunks);
        CHECK(restored_protected_before.GetValue<int32>(public_value_prop) == 42);
        CHECK_FALSE(restored_protected_before.GetValue<bool>(owner_flag_prop));
        CHECK(restored_protected_before.GetValue<string>(public_name_prop) == "public-override");

        Properties restored_protected_after(&registrator);
        restored_protected_after.RestoreData(protected_after_chunks);
        CHECK(restored_protected_after.GetValue<int32>(public_value_prop) == 42);
        CHECK(restored_protected_after.GetValue<bool>(owner_flag_prop));
        CHECK(restored_protected_after.GetValue<string>(public_name_prop) == "public-override");
    }

    SECTION("StoreAllDataHandlesEmptyOverlay")
    {
        Properties props(&registrator, &proto);

        vector<uint8> all_data;
        set<hstring> str_hashes;
        props.StoreAllData(all_data, str_hashes);

        DataReader reader(all_data);
        CHECK(reader.Read<uint32>() == registrator.GetWholeDataSize());
        CHECK(reader.Read<bool>());
        CHECK(reader.Read<uint32>() == 0);
        reader.VerifyEnd();

        Properties restored(&registrator, &proto);
        restored.RestoreAllData(all_data);

        CHECK(restored.GetValue<int32>(public_value_prop) == 10);
        CHECK(restored.GetValue<bool>(owner_flag_prop));
        CHECK(restored.GetValue<string>(public_name_prop) == "base");
    }

    SECTION("CopyFromRebasesInheritedDerivedValuesToAnotherBase")
    {
        Properties source_base(&registrator);
        source_base.SetValue<int32>(public_value_prop, 5);
        source_base.SetValue<bool>(owner_flag_prop, false);
        source_base.SetValue<string>(public_name_prop, "source");

        Properties target_base(&registrator);
        target_base.SetValue<int32>(public_value_prop, 100);
        target_base.SetValue<bool>(owner_flag_prop, true);
        target_base.SetValue<string>(public_name_prop, "target");

        Properties source_props(&registrator, &source_base);
        source_props.SetValue<int32>(public_value_prop, 77);

        Properties target_props(&registrator, &target_base);
        target_props.CopyFrom(source_props);
        target_props.CopyFrom(source_props);

        CHECK(target_props.GetValue<int32>(public_value_prop) == 77);
        CHECK_FALSE(target_props.GetValue<bool>(owner_flag_prop));
        CHECK(target_props.GetValue<string>(public_name_prop) == "source");

        vector<uint8> all_data;
        set<hstring> str_hashes;
        target_props.StoreAllData(all_data, str_hashes);

        DataReader reader(all_data);
        CHECK(reader.Read<uint32>() == registrator.GetWholeDataSize());
        CHECK(reader.Read<bool>());
        CHECK(reader.Read<uint32>() == 3);
    }

    SECTION("CopyFromRebasesDerivedValuesToAnotherBase")
    {
        Properties source_base(&registrator);
        source_base.SetValue<int32>(public_value_prop, 5);
        source_base.SetValue<bool>(owner_flag_prop, false);
        source_base.SetValue<string>(public_name_prop, "source");

        Properties target_base(&registrator);
        target_base.SetValue<int32>(public_value_prop, 100);
        target_base.SetValue<bool>(owner_flag_prop, true);
        target_base.SetValue<string>(public_name_prop, "target");

        Properties source_props(&registrator, &source_base);
        source_props.SetValue<int32>(public_value_prop, 77);
        source_props.SetValue<bool>(owner_flag_prop, true);
        source_props.SetValue<string>(public_name_prop, "custom");

        Properties target_props(&registrator, &target_base);
        target_props.CopyFrom(source_props);
        target_props.CopyFrom(source_props);

        CHECK(target_props.GetValue<int32>(public_value_prop) == 77);
        CHECK(target_props.GetValue<bool>(owner_flag_prop));
        CHECK(target_props.GetValue<string>(public_name_prop) == "custom");

        vector<uint8> all_data;
        set<hstring> str_hashes;
        target_props.StoreAllData(all_data, str_hashes);

        DataReader reader(all_data);
        CHECK(reader.Read<uint32>() == registrator.GetWholeDataSize());
        CHECK(reader.Read<bool>());
        CHECK(reader.Read<uint32>() == 2);
    }
}

TEST_CASE("PropertiesFullStorageRoundTrip")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("FullEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* value_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    const auto* flag_prop = registrator.RegisterProperty({"Common", "bool", "Flag", "Mutable", "Persistent", "PublicSync"});
    const auto* name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);
    props.SetValue<int32>(value_prop, 123);
    props.SetValue<bool>(flag_prop, true);
    props.SetValue<string>(name_prop, "full-roundtrip");

    vector<uint8> all_data;
    set<hstring> str_hashes;
    props.StoreAllData(all_data, str_hashes);

    Properties restored(&registrator);
    restored.RestoreAllData(all_data);

    CHECK(restored.GetValue<int32>(value_prop) == 123);
    CHECK(restored.GetValue<bool>(flag_prop));
    CHECK(restored.GetValue<string>(name_prop) == "full-roundtrip");

    auto copied = props.Copy();
    CHECK(copied.GetValue<int32>(value_prop) == 123);
    CHECK(copied.GetValue<bool>(flag_prop));
    CHECK(copied.GetValue<string>(name_prop) == "full-roundtrip");

    props.SetValue<string>(name_prop, "changed-after-copy");
    CHECK(copied.GetValue<string>(name_prop) == "full-roundtrip");

    SECTION("StoreAllDataPreservesZeroSizedString")
    {
        Properties empty_string_props(&registrator);
        empty_string_props.SetValue<int32>(value_prop, 123);
        empty_string_props.SetValue<bool>(flag_prop, true);
        empty_string_props.SetValue<string>(name_prop, "");

        vector<uint8> empty_string_data;
        set<hstring> empty_string_hashes;
        empty_string_props.StoreAllData(empty_string_data, empty_string_hashes);

        Properties restored_empty_string(&registrator);
        restored_empty_string.RestoreAllData(empty_string_data);

        CHECK(restored_empty_string.GetValue<int32>(value_prop) == 123);
        CHECK(restored_empty_string.GetValue<bool>(flag_prop));
        CHECK(restored_empty_string.GetValue<string>(name_prop).empty());
    }
}

TEST_CASE("PropertiesFullStorageCopyFrom")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("FullCopyEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* value_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    const auto* flag_prop = registrator.RegisterProperty({"Common", "bool", "Flag", "Mutable", "Persistent", "PublicSync"});
    const auto* name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});

    Properties source(&registrator);
    source.SetValue<int32>(value_prop, 321);
    source.SetValue<bool>(flag_prop, true);
    source.SetValue<string>(name_prop, "copy-source");

    Properties target(&registrator);
    target.SetValue<int32>(value_prop, 111);
    target.SetValue<bool>(flag_prop, false);
    target.SetValue<string>(name_prop, "copy-target");

    target.CopyFrom(source);

    CHECK(target.GetValue<int32>(value_prop) == 321);
    CHECK(target.GetValue<bool>(flag_prop));
    CHECK(target.GetValue<string>(name_prop) == "copy-source");

    source.SetValue<string>(name_prop, "source-mutated");
    CHECK(target.GetValue<string>(name_prop) == "copy-source");
}

TEST_CASE("PropertiesOverlayPreservesUnsyncedLocalOverridesOnRestore")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("ClientLocalEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* synced_value_prop = registrator.RegisterProperty({"Common", "int32", "SyncedValue", "Mutable", "Persistent", "PublicSync"});
    const auto* local_value_prop = registrator.RegisterProperty({"Common", "int32", "LocalValue", "Mutable", "Persistent", "NoSync"});

    Properties proto(&registrator);
    proto.SetValue<int32>(synced_value_prop, 10);
    proto.SetValue<int32>(local_value_prop, 0);

    SECTION("EmptyRestoreKeepsUnsyncedOverride")
    {
        Properties props(&registrator, &proto);
        props.SetValue<int32>(local_value_prop, 1);

        vector<vector<uint8>> empty_data;
        props.RestoreData(empty_data);

        CHECK(props.GetValue<int32>(synced_value_prop) == 10);
        CHECK(props.GetValue<int32>(local_value_prop) == 1);
    }

    SECTION("NetworkRestoreKeepsUnsyncedOverride")
    {
        Properties server_state(&registrator, &proto);
        server_state.SetValue<int32>(synced_value_prop, 42);

        vector<const uint8*>* raw_data = nullptr;
        vector<uint32>* raw_sizes = nullptr;
        server_state.StoreData(false, &raw_data, &raw_sizes);
        auto owned_chunks = MakeOwnedStoreData(raw_data, raw_sizes);

        Properties client_state(&registrator, &proto);
        client_state.SetValue<int32>(local_value_prop, 1);
        client_state.RestoreData(owned_chunks);

        CHECK(client_state.GetValue<int32>(synced_value_prop) == 42);
        CHECK(client_state.GetValue<int32>(local_value_prop) == 1);
    }

    SECTION("EmptyRestoreClearsSyncedOverrideButKeepsUnsyncedOverride")
    {
        Properties client_state(&registrator, &proto);
        client_state.SetValue<int32>(synced_value_prop, 99);
        client_state.SetValue<int32>(local_value_prop, 1);

        vector<vector<uint8>> empty_data;
        client_state.RestoreData(empty_data);

        CHECK(client_state.GetValue<int32>(synced_value_prop) == 10);
        CHECK(client_state.GetValue<int32>(local_value_prop) == 1);
    }

    SECTION("RestoreAllDataEmptySnapshotClearsUnsyncedOverride")
    {
        Properties server_snapshot(&registrator, &proto);
        vector<uint8> all_data;
        set<hstring> str_hashes;
        server_snapshot.StoreAllData(all_data, str_hashes);

        Properties client_state(&registrator, &proto);
        client_state.SetValue<int32>(synced_value_prop, 99);
        client_state.SetValue<int32>(local_value_prop, 1);
        client_state.RestoreAllData(all_data);

        CHECK(client_state.GetValue<int32>(synced_value_prop) == 10);
        CHECK(client_state.GetValue<int32>(local_value_prop) == 0);
    }

    SECTION("RestoreAllDataRestoresUnsyncedOverrideFromSnapshot")
    {
        Properties server_snapshot(&registrator, &proto);
        server_snapshot.SetValue<int32>(synced_value_prop, 42);
        server_snapshot.SetValue<int32>(local_value_prop, 7);

        vector<uint8> all_data;
        set<hstring> str_hashes;
        server_snapshot.StoreAllData(all_data, str_hashes);

        Properties client_state(&registrator, &proto);
        client_state.SetValue<int32>(synced_value_prop, 99);
        client_state.SetValue<int32>(local_value_prop, 1);
        client_state.RestoreAllData(all_data);

        CHECK(client_state.GetValue<int32>(synced_value_prop) == 42);
        CHECK(client_state.GetValue<int32>(local_value_prop) == 7);
    }
}

TEST_CASE("PropertiesCompareData")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("CompareEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* value_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    const auto* flag_prop = registrator.RegisterProperty({"Common", "bool", "Flag", "Mutable", "Persistent", "PublicSync"});
    const auto* name_prop = registrator.RegisterProperty({"Common", "string", "Name", "Mutable", "Persistent", "PublicSync"});
    const auto* temp_prop = registrator.RegisterProperty({"Common", "int32", "Temp", "Mutable", "NoSync"});

    SECTION("FullStorageFastPath")
    {
        Properties left(&registrator);
        left.SetValue<int32>(value_prop, 10);
        left.SetValue<bool>(flag_prop, true);
        left.SetValue<string>(name_prop, "same");

        Properties right(&registrator);
        right.SetValue<int32>(value_prop, 10);
        right.SetValue<bool>(flag_prop, true);
        right.SetValue<string>(name_prop, "same");

        CHECK(left.CompareData(right, {}, false));

        right.SetValue<string>(name_prop, "diff");
        CHECK_FALSE(left.CompareData(right, {}, false));
    }

    SECTION("DerivedSameBaseFastPath")
    {
        Properties proto(&registrator);
        proto.SetValue<int32>(value_prop, 1);
        proto.SetValue<bool>(flag_prop, false);
        proto.SetValue<string>(name_prop, "proto");

        Properties left(&registrator, &proto);
        left.SetValue<int32>(value_prop, 20);
        left.SetValue<string>(name_prop, "override");

        Properties right(&registrator, &proto);
        right.SetValue<string>(name_prop, "override");
        right.SetValue<int32>(value_prop, 20);

        CHECK(left.CompareData(right, {}, false));

        right.SetValue<bool>(flag_prop, true);
        CHECK_FALSE(left.CompareData(right, {}, false));
    }

    SECTION("IgnoreTemporaryAndIgnorePropsUseFallback")
    {
        Properties left(&registrator);
        left.SetValue<int32>(value_prop, 10);
        left.SetValue<int32>(temp_prop, 1);

        Properties right(&registrator);
        right.SetValue<int32>(value_prop, 99);
        right.SetValue<int32>(temp_prop, 2);

        array<const Property*, 1> ignored_props {{value_prop}};
        const span<const Property*> ignored_props_span {ignored_props.data(), ignored_props.size()};

        CHECK(left.CompareData(right, ignored_props_span, true));
        CHECK_FALSE(left.CompareData(right, {}, true));
        CHECK_FALSE(left.CompareData(right, ignored_props_span, false));
    }

    SECTION("SelfCompareReturnsTrue")
    {
        Properties props(&registrator);
        props.SetValue<int32>(value_prop, 10);
        props.SetValue<string>(name_prop, "self");

        CHECK(props.CompareData(props, {}, false));
    }

    SECTION("MixedFullAndDerivedUseFallback")
    {
        Properties proto(&registrator);
        proto.SetValue<int32>(value_prop, 1);
        proto.SetValue<bool>(flag_prop, false);
        proto.SetValue<string>(name_prop, "proto");

        Properties derived(&registrator, &proto);
        derived.SetValue<int32>(value_prop, 10);
        derived.SetValue<string>(name_prop, "override");

        Properties full(&registrator);
        full.SetValue<int32>(value_prop, 10);
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
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("AccessorEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* plain_prop = registrator.RegisterProperty({"Common", "int32", "Value", "Mutable", "Persistent", "PublicSync"});
    const auto* virtual_prop = registrator.RegisterProperty({"Common", "int32", "VirtualValue", "Mutable", "Virtual"});
    const auto* virtual_without_setter_prop = registrator.RegisterProperty({"Common", "int32", "VirtualWithoutSetter", "Mutable", "Virtual"});

    Properties props(&registrator);
    props.SetEntity(reinterpret_cast<Entity*>(size_t {1}));

    int32 setter_calls = 0;
    int32 post_setter_calls = 0;
    int32 virtual_value = 7;

    plain_prop->AddSetter([&](Entity*, const Property*, PropertyRawData& prop_data) {
        setter_calls++;
        prop_data.SetAs<int32>(prop_data.GetAs<int32>() + 5);
    });
    plain_prop->AddPostSetter([&](Entity*, const Property*) { post_setter_calls++; });

    virtual_prop->SetGetter([&](Entity*, const Property*) {
        PropertyRawData prop_data;
        prop_data.SetAs<int32>(virtual_value);
        return prop_data;
    });
    virtual_prop->AddSetter([&](Entity*, const Property*, PropertyRawData& prop_data) { virtual_value = prop_data.GetAs<int32>(); });

    props.SetValue<int32>(plain_prop, 10);
    CHECK(props.GetValue<int32>(plain_prop) == 15);
    CHECK(setter_calls == 1);
    CHECK(post_setter_calls == 1);

    CHECK(props.GetValue<int32>(virtual_prop) == 7);
    props.SetValue<int32>(virtual_prop, 21);
    CHECK(virtual_value == 21);
    CHECK(props.GetValue<int32>(virtual_prop) == 21);

    PropertyRawData prop_data;
    prop_data.SetAs<int32>(1);
    CHECK_THROWS(props.SetValue(virtual_without_setter_prop, prop_data));
}

TEST_CASE("PropertiesTextRoundTrip")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("TextEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* tags_prop = registrator.RegisterProperty({"Common", "string[]", "Tags", "Mutable", "Persistent", "PublicSync"});
    const auto* values_prop = registrator.RegisterProperty({"Common", "int32[]", "Values", "Mutable", "Persistent", "PublicSync"});

    Properties props(&registrator);
    props.SetValue(tags_prop, vector<string> {"alpha", "beta", ""});
    props.SetValue(values_prop, vector<int32> {10, 20, 30});

    const auto text_data = props.SaveToText(nullptr);
    REQUIRE(text_data.contains("Tags"));
    REQUIRE(text_data.contains("Values"));

    Properties restored(&registrator);
    restored.ApplyFromText(text_data);

    CHECK(restored.GetValue<vector<string>>(tags_prop) == vector<string> {"alpha", "beta", ""});
    CHECK(restored.GetValue<vector<int32>>(values_prop) == vector<int32> {10, 20, 30});
}

TEST_CASE("PropertiesHashAndEnumConversions")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("TypedEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* hash_prop = registrator.RegisterProperty({"Common", "hstring", "HashValue", "Mutable", "Persistent", "PublicSync"});
    const auto* enum_prop = registrator.RegisterProperty({"Common", "Mode", "ModeValue", "Mutable", "Persistent", "PublicSync"});

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

TEST_CASE("PropertiesBuiltinProtoReferenceSupport")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("ProtoTypedEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* item_prop = registrator.RegisterProperty({"Common", "ProtoItem", "ItemProto", "Mutable", "Persistent", "PublicSync"});
    const auto* map_prop = registrator.RegisterProperty({"Common", "ProtoMap", "SpawnMapProto", "Mutable", "Persistent", "PublicSync", "MaybeNull"});
    const auto* loot_sets_prop = registrator.RegisterProperty({"Common", "string=>ProtoItem[]", "LootSets", "Mutable", "Persistent", "PublicSync"});

    CHECK(item_prop->GetBaseType().IsEntity);
    CHECK(item_prop->IsBaseTypeEntityProto());
    CHECK(item_prop->IsBaseTypeProtoReference());
    CHECK(map_prop->IsMaybeNull());
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

    vector<uint8> all_data;
    set<hstring> str_hashes;
    restored.StoreAllData(all_data, str_hashes);

    CHECK(str_hashes.contains(hashes.ToHashedString("knife")));
    CHECK(str_hashes.contains(hashes.ToHashedString("pistol")));
}

TEST_CASE("PropertiesStoreAllDataAccumulatesHashesAcrossObjects")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("AccumulatedHashesEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* hash_prop = registrator.RegisterProperty({"Common", "hstring", "HashValue", "Mutable", "Persistent", "PublicSync"});

    Properties first(&registrator);
    first.SetValue<hstring>(hash_prop, hashes.ToHashedString("alpha"));

    Properties second(&registrator);
    second.SetValue<hstring>(hash_prop, hashes.ToHashedString("beta"));

    vector<uint8> all_data;
    set<hstring> str_hashes;

    first.StoreAllData(all_data, str_hashes);
    CHECK(str_hashes.contains(hashes.ToHashedString("alpha")));
    CHECK_FALSE(str_hashes.contains(hashes.ToHashedString("beta")));

    second.StoreAllData(all_data, str_hashes);
    CHECK(str_hashes.contains(hashes.ToHashedString("alpha")));
    CHECK(str_hashes.contains(hashes.ToHashedString("beta")));
}

TEST_CASE("PropertiesDictConversions")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("DictEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* counters_prop = registrator.RegisterProperty({"Common", "string=>int32", "Counters", "Mutable", "Persistent", "PublicSync"});
    const auto* tags_prop = registrator.RegisterProperty({"Common", "Mode=>string[]", "ModeTags", "Mutable", "Persistent", "PublicSync"});
    const auto* routes_prop = registrator.RegisterProperty({"Common", "hstring=>Mode", "RouteModes", "Mutable", "Persistent", "PublicSync"});

    const auto counters_value = []() {
        AnyData::Dict counters;
        counters.Emplace("alpha", int64 {10});
        counters.Emplace("beta", int64 {-5});
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

TEST_CASE("PropertiesNumericDictConversions")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("NumericDictEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* flags_prop = registrator.RegisterProperty({"Common", "int32=>bool", "Flags", "Mutable", "Persistent", "PublicSync"});
    const auto* checkpoints_prop = registrator.RegisterProperty({"Common", "bool=>int32[]", "Checkpoints", "Mutable", "Persistent", "PublicSync"});

    const auto flags_value = []() {
        AnyData::Dict flags;
        flags.Emplace("-7", true);
        flags.Emplace("42", false);
        return AnyData::Value {std::move(flags)};
    }();

    const auto checkpoints_value = []() {
        AnyData::Array false_points;
        false_points.EmplaceBack(int64 {0});
        false_points.EmplaceBack(int64 {5});

        AnyData::Array true_points;
        true_points.EmplaceBack(int64 {11});
        true_points.EmplaceBack(int64 {22});
        true_points.EmplaceBack(int64 {33});

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
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("SpecialDictEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* mode_sets_prop = registrator.RegisterProperty({"Common", "string=>Mode[]", "ModeSets", "Mutable", "Persistent", "PublicSync"});
    const auto* route_tags_prop = registrator.RegisterProperty({"Common", "int32=>hstring[]", "RouteTags", "Mutable", "Persistent", "PublicSync"});

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
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("FloatDictEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* labels_prop = registrator.RegisterProperty({"Common", "float32=>string", "Labels", "Mutable", "Persistent", "PublicSync"});
    const auto* samples_prop = registrator.RegisterProperty({"Common", "float32=>float32[]", "Samples", "Mutable", "Persistent", "PublicSync"});

    const auto labels_value = []() {
        AnyData::Dict labels;
        labels.Emplace("-0.5", string {"low"});
        labels.Emplace("1.25", string {"high tide"});
        return AnyData::Value {std::move(labels)};
    }();

    const auto samples_value = []() {
        AnyData::Array low_samples;
        low_samples.EmplaceBack(float64 {-2.5});
        low_samples.EmplaceBack(float64 {0.125});

        AnyData::Array high_samples;
        high_samples.EmplaceBack(float64 {1.25});
        high_samples.EmplaceBack(float64 {2.5});

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
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("StructDictEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* leader_prop = registrator.RegisterProperty({"Common", "string=>Waypoint", "LeaderWaypoint", "Mutable", "Persistent", "PublicSync"});
    const auto* patrol_prop = registrator.RegisterProperty({"Common", "int32=>Waypoint[]", "PatrolWaypoints", "Mutable", "Persistent", "PublicSync"});

    const auto leader_value = []() {
        AnyData::Dict leaders;

        AnyData::Array north_waypoint;
        north_waypoint.EmplaceBack(int64 {10});
        north_waypoint.EmplaceBack(float64 {1.5});
        north_waypoint.EmplaceBack(true);

        AnyData::Array south_waypoint;
        south_waypoint.EmplaceBack(int64 {20});
        south_waypoint.EmplaceBack(float64 {3.25});
        south_waypoint.EmplaceBack(false);

        leaders.Emplace("north", AnyData::Value {std::move(north_waypoint)});
        leaders.Emplace("south gate", AnyData::Value {std::move(south_waypoint)});
        return AnyData::Value {std::move(leaders)};
    }();

    const auto patrol_value = []() {
        AnyData::Array first_path;

        AnyData::Array first_path_point_a;
        first_path_point_a.EmplaceBack(int64 {1});
        first_path_point_a.EmplaceBack(float64 {0.5});
        first_path_point_a.EmplaceBack(true);
        first_path.EmplaceBack(AnyData::Value {std::move(first_path_point_a)});

        AnyData::Array first_path_point_b;
        first_path_point_b.EmplaceBack(int64 {2});
        first_path_point_b.EmplaceBack(float64 {1.25});
        first_path_point_b.EmplaceBack(false);
        first_path.EmplaceBack(AnyData::Value {std::move(first_path_point_b)});

        AnyData::Array second_path;

        AnyData::Array second_path_point_a;
        second_path_point_a.EmplaceBack(int64 {7});
        second_path_point_a.EmplaceBack(float64 {4.75});
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

TEST_CASE("PropertiesSaveToDocumentSkipsDefaultAndBaseValues")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("DocumentEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* counter_prop = registrator.RegisterProperty({"Common", "int32", "Counter", "Mutable", "Persistent", "PublicSync"});
    const auto* title_prop = registrator.RegisterProperty({"Common", "string", "Title", "Mutable", "Persistent", "PublicSync"});
    const auto* enabled_prop = registrator.RegisterProperty({"Common", "bool", "Enabled", "Mutable", "Persistent", "PublicSync"});

    Properties empty_props(&registrator);
    const auto empty_doc = PropertiesSerializator::SaveToDocument(&empty_props, nullptr, hashes, resolver);
    CHECK(empty_doc.Empty());

    Properties proto(&registrator);
    proto.SetValue<int32>(counter_prop, 7);
    proto.SetValue<string>(title_prop, "proto title");

    Properties props(&registrator, &proto);
    props.SetValue<string>(title_prop, "shift lead");
    props.SetValue<bool>(enabled_prop, true);

    const auto doc = PropertiesSerializator::SaveToDocument(&props, &proto, hashes, resolver);
    CHECK(doc.Size() == 2);
    CHECK_FALSE(doc.Contains("Counter"));
    REQUIRE(doc.Contains("Title"));
    REQUIRE(doc.Contains("Enabled"));
    CHECK(doc["Title"].AsString() == "shift lead");
    CHECK(doc["Enabled"].AsBool());
}

TEST_CASE("PropertiesLoadFromDocumentSkipsTechnicalAndUnknownFields")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("DocumentLoadEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* counter_prop = registrator.RegisterProperty({"Common", "int32", "Counter", "Mutable", "Persistent", "PublicSync"});
    const auto* title_prop = registrator.RegisterProperty({"Common", "string", "Title", "Mutable", "Persistent", "PublicSync"});

    AnyData::Document doc;
    doc.Emplace("$version", int64 {3});
    doc.Emplace("_meta", string {"ignored"});
    doc.Emplace("Counter", int64 {15});
    doc.Emplace("Title", string {"  south gate  "});
    doc.Emplace("UnknownField", int64 {99});

    Properties props(&registrator);
    CHECK(PropertiesSerializator::LoadFromDocument(&props, doc, hashes, resolver));
    CHECK(props.GetValue<int32>(counter_prop) == 15);
    CHECK(props.GetValue<string>(title_prop) == "  south gate  ");
}

TEST_CASE("PropertiesLoadFromDocumentReportsInvalidFieldButContinues")
{
    HashStorage hashes;
    TestNameResolver resolver;
    PropertyRegistrator registrator("DocumentErrorEntity", EngineSideKind::ServerSide, hashes, resolver);

    const auto* counter_prop = registrator.RegisterProperty({"Common", "int32", "Counter", "Mutable", "Persistent", "PublicSync"});
    const auto* enabled_prop = registrator.RegisterProperty({"Common", "bool", "Enabled", "Mutable", "Persistent", "PublicSync"});

    AnyData::Document doc;
    doc.Emplace("Counter", string {"wrong-type"});
    doc.Emplace("Enabled", true);

    Properties props(&registrator);
    CHECK_FALSE(PropertiesSerializator::LoadFromDocument(&props, doc, hashes, resolver));
    CHECK(props.GetValue<int32>(counter_prop) == 0);
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
        vector<const uint8*>* raw_data = nullptr;
        vector<uint32>* raw_sizes = nullptr;
        fixture.Full->StoreData(false, &raw_data, &raw_sizes);
        return raw_sizes->size();
    };

    BENCHMARK_ADVANCED("StoreData full after plain mutation")(Catch::Benchmark::Chronometer meter)
    {
        uint32 counter = 0;
        vector<const uint8*>* raw_data = nullptr;
        vector<uint32>* raw_sizes = nullptr;

        meter.measure([&](int) {
            fixture.Full->SetValue<int32>(fixture.PublicIntProps.front(), numeric_cast<int32>(++counter));
            fixture.Full->StoreData(false, &raw_data, &raw_sizes);
            return (*raw_sizes)[0];
        });
    };

    BENCHMARK_ADVANCED("RestoreData derived public payload")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            Properties target(&fixture.Registrator, fixture.Proto.get());
            target.RestoreData(fixture.DerivedPublicChunks);
            return target.GetValueFast<int32>(fixture.PublicIntProps.front());
        });
    };

    BENCHMARK_ADVANCED("StoreAllData full snapshot")(Catch::Benchmark::Chronometer meter)
    {
        vector<uint8> all_data;
        set<hstring> str_hashes;

        meter.measure([&](int) {
            fixture.Full->StoreAllData(all_data, str_hashes);
            return all_data.size();
        });
    };

    BENCHMARK_ADVANCED("StoreAllData derived snapshot")(Catch::Benchmark::Chronometer meter)
    {
        vector<uint8> all_data;
        set<hstring> str_hashes;

        meter.measure([&](int) {
            fixture.DerivedSource->StoreAllData(all_data, str_hashes);
            return all_data.size();
        });
    };

    BENCHMARK_ADVANCED("RestoreAllData full snapshot")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            Properties target(&fixture.Registrator);
            target.RestoreAllData(fixture.FullAllData);
            return target.GetValueFast<int32>(fixture.PublicIntProps.front());
        });
    };

    BENCHMARK_ADVANCED("RestoreAllData derived snapshot")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            Properties target(&fixture.Registrator, fixture.Proto.get());
            target.RestoreAllData(fixture.DerivedAllData);
            return target.GetValueFast<int32>(fixture.PublicIntProps.front());
        });
    };

    BENCHMARK_ADVANCED("CopyFrom full to derived")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            Properties target(&fixture.Registrator, fixture.Proto.get());
            target.CopyFrom(*fixture.Full);
            return target.GetValueFast<int32>(fixture.PublicIntProps.front());
        });
    };

    Properties left(&fixture.Registrator, fixture.Proto.get());
    left.CopyFrom(*fixture.DerivedSource);

    Properties right(&fixture.Registrator, fixture.Proto.get());
    right.CopyFrom(*fixture.DerivedSource);

    BENCHMARK("CompareData same-base derived fast path")
    {
        return left.CompareData(right, {}, false);
    };

    BENCHMARK_ADVANCED("SaveToText dict-rich payload")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            const auto text_data = dict_fixture.Source->SaveToText(nullptr);
            return text_data.size();
        });
    };

    BENCHMARK_ADVANCED("ApplyFromText dict-rich payload")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            Properties target(&dict_fixture.Registrator);
            target.ApplyFromText(dict_fixture.TextData);
            return target.CompareData(*dict_fixture.Source, {}, false);
        });
    };

    BENCHMARK_ADVANCED("SavePropertyToValue struct dict-of-array")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            const auto value = PropertiesSerializator::SavePropertyToValue(dict_fixture.Source.get(), dict_fixture.PatrolWaypointsProp, dict_fixture.Hashes, dict_fixture.Resolver);
            return value.AsDict().Size();
        });
    };

    BENCHMARK_ADVANCED("LoadPropertyFromValue struct dict-of-array")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            Properties target(&dict_fixture.Registrator);
            PropertiesSerializator::LoadPropertyFromValue(&target, dict_fixture.PatrolWaypointsProp, dict_fixture.PatrolWaypointsValue->Copy(), dict_fixture.Hashes, dict_fixture.Resolver);
            return target.CompareData(*dict_fixture.PatrolOnlySource, {}, false);
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

            CHECK(fixture.Proto);
            CHECK(fixture.PackedSource);
            CHECK(fixture.FullSource);
            CHECK(fixture.ProbeIntProp != nullptr);
            CHECK(fixture.ProbeStringProp != nullptr);
            CHECK(numeric_cast<int>(fixture.OverrideProps.size()) == std::max(1, prop_count * fill_percent / 100));

            BENCHMARK("Read simple int packed")
            {
                return fixture.PackedSource->GetValueFast<int32>(fixture.ProbeIntProp);
            };

            BENCHMARK("Read simple int full")
            {
                return fixture.FullSource->GetValueFast<int32>(fixture.ProbeIntProp);
            };

            BENCHMARK("Read simple string packed")
            {
                return fixture.PackedSource->GetValueFast<string>(fixture.ProbeStringProp).size();
            };

            BENCHMARK("Read simple string full")
            {
                return fixture.FullSource->GetValueFast<string>(fixture.ProbeStringProp).size();
            };

            BENCHMARK_ADVANCED("Modify simple int packed")(Catch::Benchmark::Chronometer meter)
            {
                Properties target(&fixture.Registrator, fixture.Proto.get());
                int32 counter = 0;

                meter.measure([&](int) {
                    target.SetValue<int32>(fixture.ProbeIntProp, numeric_cast<int32>(10000 + ++counter));
                    return target.GetValueFast<int32>(fixture.ProbeIntProp);
                });
            };

            BENCHMARK_ADVANCED("Modify simple int full")(Catch::Benchmark::Chronometer meter)
            {
                Properties target(&fixture.Registrator);
                target.CopyFrom(*fixture.Proto);
                int32 counter = 0;

                meter.measure([&](int) {
                    target.SetValue<int32>(fixture.ProbeIntProp, numeric_cast<int32>(10000 + ++counter));
                    return target.GetValueFast<int32>(fixture.ProbeIntProp);
                });
            };

            BENCHMARK_ADVANCED("Modify simple string packed")(Catch::Benchmark::Chronometer meter)
            {
                Properties target(&fixture.Registrator, fixture.Proto.get());
                uint32 counter = 0;

                meter.measure([&](int) {
                    target.SetValue<string>(fixture.ProbeStringProp, strex("packed-{}", ++counter).str());
                    return target.GetValueFast<string>(fixture.ProbeStringProp).size();
                });
            };

            BENCHMARK_ADVANCED("Modify simple string full")(Catch::Benchmark::Chronometer meter)
            {
                Properties target(&fixture.Registrator);
                target.CopyFrom(*fixture.Proto);
                uint32 counter = 0;

                meter.measure([&](int) {
                    target.SetValue<string>(fixture.ProbeStringProp, strex("full-{}", ++counter).str());
                    return target.GetValueFast<string>(fixture.ProbeStringProp).size();
                });
            };

            BENCHMARK_ADVANCED("Construct packed from proto and apply overrides")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    Properties target(&fixture.Registrator, fixture.Proto.get());
                    fixture.ApplyOverrides(target);
                    return target.GetValueFast<int32>(fixture.ProbeIntProp);
                });
            };

            BENCHMARK_ADVANCED("Construct full from proto and apply overrides")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    Properties target(&fixture.Registrator);
                    target.CopyFrom(*fixture.Proto);
                    fixture.ApplyOverrides(target);
                    return target.GetValueFast<int32>(fixture.ProbeIntProp);
                });
            };

            BENCHMARK_ADVANCED("StoreData packed public payload")(Catch::Benchmark::Chronometer meter)
            {
                vector<const uint8*>* raw_data = nullptr;
                vector<uint32>* raw_sizes = nullptr;

                meter.measure([&](int) {
                    fixture.PackedSource->StoreData(false, &raw_data, &raw_sizes);
                    return raw_sizes->size();
                });
            };

            BENCHMARK_ADVANCED("StoreData full public payload")(Catch::Benchmark::Chronometer meter)
            {
                vector<const uint8*>* raw_data = nullptr;
                vector<uint32>* raw_sizes = nullptr;

                meter.measure([&](int) {
                    fixture.FullSource->StoreData(false, &raw_data, &raw_sizes);
                    return raw_sizes->size();
                });
            };

            BENCHMARK_ADVANCED("RestoreData packed public payload into fresh object")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    Properties target(&fixture.Registrator, fixture.Proto.get());
                    target.RestoreData(fixture.PackedPublicChunks);
                    return target.GetValueFast<int32>(fixture.ProbeIntProp);
                });
            };

            BENCHMARK_ADVANCED("RestoreData full public payload into fresh object")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    Properties target(&fixture.Registrator);
                    target.CopyFrom(*fixture.Proto);
                    target.RestoreData(fixture.FullPublicChunks);
                    return target.GetValueFast<int32>(fixture.ProbeIntProp);
                });
            };

            BENCHMARK_ADVANCED("StoreAllData packed snapshot")(Catch::Benchmark::Chronometer meter)
            {
                vector<uint8> all_data;
                set<hstring> str_hashes;

                meter.measure([&](int) {
                    fixture.PackedSource->StoreAllData(all_data, str_hashes);
                    return all_data.size();
                });
            };

            BENCHMARK_ADVANCED("StoreAllData full snapshot")(Catch::Benchmark::Chronometer meter)
            {
                vector<uint8> all_data;
                set<hstring> str_hashes;

                meter.measure([&](int) {
                    fixture.FullSource->StoreAllData(all_data, str_hashes);
                    return all_data.size();
                });
            };

            BENCHMARK_ADVANCED("RestoreAllData packed snapshot into fresh object")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    Properties target(&fixture.Registrator, fixture.Proto.get());
                    target.RestoreAllData(fixture.PackedAllData);
                    return target.GetValueFast<int32>(fixture.ProbeIntProp);
                });
            };

            BENCHMARK_ADVANCED("RestoreAllData full snapshot into fresh object")(Catch::Benchmark::Chronometer meter)
            {
                meter.measure([&](int) {
                    Properties target(&fixture.Registrator);
                    target.RestoreAllData(fixture.FullAllData);
                    return target.GetValueFast<int32>(fixture.ProbeIntProp);
                });
            };
        }
    }
}

TEST_CASE("PropertiesComplexStrategyPerformance", "[!benchmark][properties]")
{
    PropertiesComplexStrategyPerfFixture fixture;

    CHECK(fixture.PackedSource);
    CHECK(fixture.FullSource);

    BENCHMARK_ADVANCED("Access complex patrol packed as structured value")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            const auto value = PropertiesSerializator::SavePropertyToValue(fixture.PackedSource.get(), fixture.PatrolWaypointsProp, fixture.Hashes, fixture.Resolver);
            return value.AsDict().Size();
        });
    };

    BENCHMARK_ADVANCED("Access complex patrol full as structured value")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            const auto value = PropertiesSerializator::SavePropertyToValue(fixture.FullSource.get(), fixture.PatrolWaypointsProp, fixture.Hashes, fixture.Resolver);
            return value.AsDict().Size();
        });
    };

    BENCHMARK_ADVANCED("Access complex mode sets packed as structured value")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            const auto value = PropertiesSerializator::SavePropertyToValue(fixture.PackedSource.get(), fixture.ModeSetsProp, fixture.Hashes, fixture.Resolver);
            return value.AsDict().Size();
        });
    };

    BENCHMARK_ADVANCED("Access complex mode sets full as structured value")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&](int) {
            const auto value = PropertiesSerializator::SavePropertyToValue(fixture.FullSource.get(), fixture.ModeSetsProp, fixture.Hashes, fixture.Resolver);
            return value.AsDict().Size();
        });
    };

    BENCHMARK_ADVANCED("Modify complex patrol packed via any props")(Catch::Benchmark::Chronometer meter)
    {
        Properties target(&fixture.Registrator, fixture.Proto.get());
        uint32 counter = 0;

        meter.measure([&](int) {
            const auto& text = (++counter % 2 == 0) ? fixture.PatrolOverrideTextA : fixture.PatrolOverrideTextB;
            target.SetValueAsAnyProps(fixture.PatrolWaypointsProp->GetRegIndex(), any_t {text});
            return target.GetRawData(fixture.PatrolWaypointsProp).size();
        });
    };

    BENCHMARK_ADVANCED("Modify complex patrol full via any props")(Catch::Benchmark::Chronometer meter)
    {
        Properties target(&fixture.Registrator);
        target.CopyFrom(*fixture.Proto);
        uint32 counter = 0;

        meter.measure([&](int) {
            const auto& text = (++counter % 2 == 0) ? fixture.PatrolOverrideTextA : fixture.PatrolOverrideTextB;
            target.SetValueAsAnyProps(fixture.PatrolWaypointsProp->GetRegIndex(), any_t {text});
            return target.GetRawData(fixture.PatrolWaypointsProp).size();
        });
    };

    BENCHMARK_ADVANCED("Modify complex mode sets packed via any props")(Catch::Benchmark::Chronometer meter)
    {
        Properties target(&fixture.Registrator, fixture.Proto.get());
        uint32 counter = 0;

        meter.measure([&](int) {
            const auto& text = (++counter % 2 == 0) ? fixture.ModeOverrideTextA : fixture.ModeOverrideTextB;
            target.SetValueAsAnyProps(fixture.ModeSetsProp->GetRegIndex(), any_t {text});
            return target.GetRawData(fixture.ModeSetsProp).size();
        });
    };

    BENCHMARK_ADVANCED("Modify complex mode sets full via any props")(Catch::Benchmark::Chronometer meter)
    {
        Properties target(&fixture.Registrator);
        target.CopyFrom(*fixture.Proto);
        uint32 counter = 0;

        meter.measure([&](int) {
            const auto& text = (++counter % 2 == 0) ? fixture.ModeOverrideTextA : fixture.ModeOverrideTextB;
            target.SetValueAsAnyProps(fixture.ModeSetsProp->GetRegIndex(), any_t {text});
            return target.GetRawData(fixture.ModeSetsProp).size();
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

        const auto packed_text_ok = fixture.TryBuildTextData(*fixture.PackedSource, packed_text_data, packed_text_chars, packed_text_error);
        const auto full_text_ok = fixture.TryBuildTextData(*fixture.FullSource, full_text_data, full_text_chars, full_text_error);
        const auto packed_failing_prop = packed_text_ok ? string {} : fixture.DiagnoseTextSerializationFailure(*fixture.PackedSource);
        const auto full_failing_prop = full_text_ok ? string {} : fixture.DiagnoseTextSerializationFailure(*fixture.FullSource);

        std::cout << strex("StorageMetrics Props={} Fill={} Overrides={} PackedPublicChunks={} PackedPublicBytes={} FullPublicChunks={} FullPublicBytes={} PackedAllDataBytes={} FullAllDataBytes={} PackedTextOk={} PackedTextEntries={} PackedTextChars={} PackedTextError={} PackedTextFailingProp={} FullTextOk={} FullTextEntries={} FullTextChars={} FullTextError={} FullTextFailingProp={}\n", prop_count, fill_percent, fixture.OverrideProps.size(), fixture.PackedPublicChunks.size(), fixture.PackedPublicBytes, fixture.FullPublicChunks.size(), fixture.FullPublicBytes, fixture.PackedAllData.size(), fixture.FullAllData.size(), packed_text_ok, packed_text_data.size(), packed_text_chars, packed_text_error, packed_failing_prop, full_text_ok, full_text_data.size(), full_text_chars, full_text_error, full_failing_prop).str();

        CHECK(fixture.PackedSource);
        CHECK(fixture.FullSource);
    }
}

FO_END_NAMESPACE
