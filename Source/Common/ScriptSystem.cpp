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

#include "ScriptSystem.h"
#include "Application.h"
#include "EngineBase.h"
#include "Entity.h"
#include "FileSystem.h"
#include "Geometry.h"
#include "Properties.h"
#include "PropertiesSerializator.h"

FO_BEGIN_NAMESPACE

DynamicRefTypeInstance::DynamicRefTypeInstance(ptr<const PropertyRegistrator> registrator) noexcept :
    _registrator {registrator},
    _props {std::in_place, _registrator}
{
}

DynamicRefTypeInstance::~DynamicRefTypeInstance() noexcept = default;

auto DynamicRefTypeInstance::GetProps() noexcept -> ptr<Properties>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_props.has_value(), "Dynamic ref-type instance has no properties");
    return &*_props;
}

auto DynamicRefTypeInstance::GetProps() const noexcept -> ptr<const Properties>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_props.has_value(), "Dynamic ref-type instance has no properties");
    return &*_props;
}

void DynamicRefTypeInstance::LoadFromRawData(const BaseTypeDesc& base_type, span<const uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsRefType, "Base type is not a reference type");
    FO_VERIFY_AND_THROW(base_type.RefType, "Reference type descriptor is null");
    FO_VERIFY_AND_THROW(base_type.RefType->FieldsRegistrator, "Reference type has no fields registrator");
    auto fields_registrator = base_type.RefType->FieldsRegistrator;
    FO_VERIFY_AND_THROW(fields_registrator == _registrator, "Dynamic ref-type raw data belongs to a different fields registrator", fields_registrator->GetTypeName(), _registrator->GetTypeName());

    _props.emplace(_registrator);
    auto props = GetProps();

    size_t data_pos = 0;

    for (size_t i = 1; i < fields_registrator->GetPropertiesCount(); i++) {
        auto field_prop = fields_registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));
        FO_VERIFY_AND_THROW(field_prop, "Field property is null");
        span<const uint8_t> field_raw_data {};

        if (data_pos < raw_data.size()) {
            data_pos = align_up(data_pos, sizeof(uint32_t));

            if (data_pos > raw_data.size() || raw_data.size() - data_pos < sizeof(uint32_t)) {
                throw PropertySerializationException("Corrupted ref type property data", base_type.Name, field_prop->GetName());
            }

            const uint32_t field_size = span_read_object<uint32_t>(raw_data, data_pos);

            if (field_prop->IsPlainData() && field_size != 0 && field_size != field_prop->GetBaseSize()) {
                throw PropertySerializationException("Wrong ref field raw size", base_type.Name, field_prop->GetName());
            }

            const size_t field_data_size = field_size;

            if (field_data_size != 0) {
                data_pos = align_up(data_pos, field_prop->GetDataAlignment());
            }

            if (data_pos > raw_data.size() || raw_data.size() - data_pos < field_data_size) {
                throw PropertySerializationException("Corrupted ref type property data", base_type.Name, field_prop->GetName());
            }

            field_raw_data = span_read_bytes(raw_data, data_pos, field_data_size);
        }

        if (!field_raw_data.empty()) {
            props->SetRawData(field_prop, field_raw_data);
        }
    }

    if (data_pos != raw_data.size()) {
        throw PropertySerializationException("Corrupted ref type property data", base_type.Name);
    }

    _cachedRawData.assign(raw_data.begin(), raw_data.end());
    _cachedRawDataDirty = false;
}

auto DynamicRefTypeInstance::GetRawData(ptr<const Property> prop) const -> span<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(prop->GetRegistrator() == _registrator, "Dynamic ref-type property belongs to a different registrator", prop->GetName(), prop->GetRegistrator()->GetTypeName(), _registrator->GetTypeName());
    FO_VERIFY_AND_THROW(_props, "Missing required properties");
    return _props->GetRawData(prop);
}

void DynamicRefTypeInstance::SetValue(ptr<const Property> prop, PropertyRawData& prop_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(prop->GetRegistrator() == _registrator, "Dynamic ref-type property belongs to a different registrator", prop->GetName(), prop->GetRegistrator()->GetTypeName(), _registrator->GetTypeName());
    FO_VERIFY_AND_THROW(_props, "Missing required properties");

    GetProps()->SetValue(prop, prop_data);
    _cachedRawDataDirty = true;
}

auto DynamicRefTypeInstance::GetSerializedRawData(const BaseTypeDesc& base_type) -> const_span<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsRefType, "Base type is not a reference type");
    FO_VERIFY_AND_THROW(base_type.RefType, "Reference type descriptor is null");
    FO_VERIFY_AND_THROW(base_type.RefType->FieldsRegistrator, "Reference type has no fields registrator");
    auto fields_registrator = base_type.RefType->FieldsRegistrator;
    FO_VERIFY_AND_THROW(fields_registrator == _registrator, "Dynamic ref-type serialization requested a different fields registrator", fields_registrator->GetTypeName(), _registrator->GetTypeName());
    FO_VERIFY_AND_THROW(_props, "Missing required properties");

    if (_cachedRawDataDirty) {
        vector<span<const uint8_t>> field_raw_entries(fields_registrator->GetPropertiesCount());
        vector<bool> field_is_default(fields_registrator->GetPropertiesCount(), true);
        size_t last_non_default_field = 0;

        for (size_t i = 1; i < fields_registrator->GetPropertiesCount(); i++) {
            auto field_prop = fields_registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));
            const auto field_raw_data = GetProps()->GetRawData(field_prop.as_ptr());

            bool is_default = field_raw_data.empty();

            if (!is_default && field_prop->IsPlainData()) {
                is_default = true;

                for (const auto byte : field_raw_data) {
                    if (byte != 0) {
                        is_default = false;
                        break;
                    }
                }
            }

            field_raw_entries[i] = field_raw_data;
            field_is_default[i] = is_default;

            if (!is_default) {
                last_non_default_field = i;
            }
        }

        if (last_non_default_field == 0) {
            _cachedRawData.clear();
        }
        else {
            size_t data_size = 0;

            for (size_t i = 1; i <= last_non_default_field; i++) {
                data_size = align_up(data_size, sizeof(uint32_t));
                data_size += sizeof(uint32_t);

                if (!field_is_default[i]) {
                    auto field_prop = fields_registrator->GetPropertyByIndexUnsafe(i);
                    data_size = align_up(data_size, field_prop->GetDataAlignment());
                    data_size += field_raw_entries[i].size();
                }
            }

            _cachedRawData.assign(data_size, 0);
            auto raw_buffer = make_span(_cachedRawData);
            size_t data_pos = 0;

            for (size_t i = 1; i <= last_non_default_field; i++) {
                const uint32_t field_size = !field_is_default[i] ? numeric_cast<uint32_t>(field_raw_entries[i].size()) : 0;
                data_pos = align_up(data_pos, sizeof(uint32_t));
                span_write_object(raw_buffer, data_pos, field_size);

                if (field_size != 0) {
                    auto field_prop = fields_registrator->GetPropertyByIndexUnsafe(i);
                    data_pos = align_up(data_pos, field_prop->GetDataAlignment());
                    span_write_bytes(raw_buffer, data_pos, field_raw_entries[i]);
                }
            }

            FO_VERIFY_AND_THROW(data_pos == data_size, "Dynamic ref-type cached raw buffer size does not match bytes written", _registrator->GetTypeName(), data_pos, data_size);
        }

        _cachedRawDataDirty = false;
    }

    return _cachedRawData;
}

void ScriptSystem::MapScriptTypes(ptr<EngineMetadata> meta)
{
    FO_STACK_TRACE_ENTRY();

    MapEngineType<bool>(meta->GetBaseType("bool"));
    MapEngineType<int8_t>(meta->GetBaseType("int8"));
    MapEngineType<int16_t>(meta->GetBaseType("int16"));
    MapEngineType<int32_t>(meta->GetBaseType("int32"));
    MapEngineType<int64_t>(meta->GetBaseType("int64"));
    MapEngineType<uint8_t>(meta->GetBaseType("uint8"));
    MapEngineType<uint16_t>(meta->GetBaseType("uint16"));
    MapEngineType<uint32_t>(meta->GetBaseType("uint32"));
    MapEngineType<uint64_t>(meta->GetBaseType("uint64"));
    MapEngineType<float32_t>(meta->GetBaseType("float32"));
    MapEngineType<float64_t>(meta->GetBaseType("float64"));
    MapEngineType<ident_t>(meta->GetBaseType("ident"));
    MapEngineType<timespan>(meta->GetBaseType("timespan"));
    MapEngineType<nanotime>(meta->GetBaseType("nanotime"));
    MapEngineType<synctime>(meta->GetBaseType("synctime"));
    MapEngineType<ucolor>(meta->GetBaseType("ucolor"));
    MapEngineType<isize32>(meta->GetBaseType("isize"));
    MapEngineType<ipos32>(meta->GetBaseType("ipos"));
    MapEngineType<irect32>(meta->GetBaseType("irect"));
    MapEngineType<ipos16>(meta->GetBaseType("ipos16"));
    MapEngineType<ipos8>(meta->GetBaseType("ipos8"));
    MapEngineType<fsize32>(meta->GetBaseType("fsize"));
    MapEngineType<fpos32>(meta->GetBaseType("fpos"));
    MapEngineType<frect32>(meta->GetBaseType("frect"));
    MapEngineType<mpos>(meta->GetBaseType("mpos"));
    MapEngineType<msize>(meta->GetBaseType("msize"));
    MapEngineType<mdir>(meta->GetBaseType("mdir"));
    MapEngineType<hdir>(meta->GetBaseType("hdir"));
    MapEngineType<string>(meta->GetBaseType("string"));
    MapEngineType<hstring>(meta->GetBaseType("hstring"));
    MapEngineType<any_t>(meta->GetBaseType("any"));
    MapEngineType<GameProperty>(meta->GetBaseType("GameProperty"));
    MapEngineType<PlayerProperty>(meta->GetBaseType("PlayerProperty"));
    MapEngineType<ItemProperty>(meta->GetBaseType("ItemProperty"));
    MapEngineType<CritterProperty>(meta->GetBaseType("CritterProperty"));
    MapEngineType<MapProperty>(meta->GetBaseType("MapProperty"));
    MapEngineType<LocationProperty>(meta->GetBaseType("LocationProperty"));
    MapEngineType<Entity>(meta->GetBaseType("Entity"));

    MapEngineDictType<int32_t, int32_t>(meta->GetBaseType("int32"), meta->GetBaseType("int32"));
    MapEngineDictType<string, string>(meta->GetBaseType("string"), meta->GetBaseType("string"));
    MapEngineDictType<ItemProperty, int32_t>(meta->GetBaseType("ItemProperty"), meta->GetBaseType("int32"));
    MapEngineDictType<CritterProperty, int32_t>(meta->GetBaseType("CritterProperty"), meta->GetBaseType("int32"));
    MapEngineDictType<CritterProperty, any_t>(meta->GetBaseType("CritterProperty"), meta->GetBaseType("any"));
    MapEngineDictType<LocationProperty, any_t>(meta->GetBaseType("LocationProperty"), meta->GetBaseType("any"));
}

void ScriptSystem::RegisterBackend(size_t index, unique_ptr<ScriptSystemBackend> backend)
{
    FO_STACK_TRACE_ENTRY();

    const auto [it, inserted] = _backends.emplace(index, std::move(backend));
    ignore_unused(it);
    FO_VERIFY_AND_THROW(inserted, "Backends[index] is already set");
}

void ScriptSystem::ShutdownBackends()
{
    FO_STACK_TRACE_ENTRY();

    _engineTypes.clear();
    _globalFuncMap.clear();
    _initFunc.clear();
    _backends.clear();
}

void ScriptSystem::AddInitFunc(ScriptFunc<void> func, int32_t priority)
{
    FO_STACK_TRACE_ENTRY();

    _initFunc.emplace_back(std::move(func), priority);
    std::ranges::stable_sort(_initFunc, [](auto&& a, auto&& b) { return a.second < b.second; });
}

auto ScriptSystem::ValidateArgs(ptr<const ScriptFuncDesc> func, const_span<size_t> arg_types, size_t ret_type) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!func->Call) {
        return false;
    }

    if (func->Args.size() != arg_types.size()) {
        return false;
    }
    if ((func->Ret.Kind != ComplexTypeKind::None) != (ret_type != ArgMapTypeIndex<void>())) {
        return false;
    }

    const auto check_type = [this](const ComplexTypeDesc& left, size_t right) -> bool {
        const auto it = _engineTypes.find(right);
        FO_VERIFY_AND_RETURN_VALUE(it != _engineTypes.end(), false, "Script engine type index is not registered while validating arguments", right);
        return left == it->second;
    };

    if (func->Ret.Kind != ComplexTypeKind::None && !check_type(func->Ret, ret_type)) {
        return false;
    }

    for (size_t i = 0; i < arg_types.size(); i++) {
        if (!check_type(func->Args[i].Type, arg_types[i])) {
            return false;
        }
    }

    return true;
}

auto ScriptSystem::FindFunc(hstring func_name, const_span<size_t> arg_types) noexcept -> nptr<ScriptFuncDesc>
{
    FO_STACK_TRACE_ENTRY();

    const auto range = _globalFuncMap.equal_range(func_name);

    for (auto it = range.first; it != range.second; ++it) {
        if (ValidateArgs(it->second, arg_types, ArgMapTypeIndex<void>())) {
            return it->second;
        }
    }

    return nullptr;
}

auto ScriptSystem::FindFunc(hstring func_name, span<const ComplexTypeDesc> arg_types) noexcept -> nptr<ScriptFuncDesc>
{
    FO_STACK_TRACE_ENTRY();

    const auto range = _globalFuncMap.equal_range(func_name);

    for (auto it = range.first; it != range.second; ++it) {
        auto func = it->second;

        if (!func->Call || func->Ret.Kind != ComplexTypeKind::None || func->Args.size() != arg_types.size()) {
            continue;
        }

        bool args_match = true;

        for (size_t i = 0; i < arg_types.size(); i++) {
            if (!AreComplexScriptTypesCompatible(func->Args[i].Type, arg_types[i])) {
                args_match = false;
                break;
            }
        }

        if (args_match) {
            return func;
        }
    }

    return nullptr;
}

auto ScriptSystem::FindFunc(hstring func_name, span<const ComplexTypeDesc> arg_types, const ComplexTypeDesc& ret_type) noexcept -> nptr<ScriptFuncDesc>
{
    FO_STACK_TRACE_ENTRY();

    const auto range = _globalFuncMap.equal_range(func_name);

    for (auto it = range.first; it != range.second; ++it) {
        ptr<ScriptFuncDesc> func = it->second;

        if (!func->Call || func->Ret.Kind == ComplexTypeKind::None || !AreComplexScriptTypesCompatible(func->Ret, ret_type) || func->Args.size() != arg_types.size()) {
            continue;
        }

        bool args_match = true;

        for (size_t i = 0; i < arg_types.size(); i++) {
            if (!AreComplexScriptTypesCompatible(func->Args[i].Type, arg_types[i])) {
                args_match = false;
                break;
            }
        }

        if (args_match) {
            return func;
        }
    }

    return nullptr;
}

auto ScriptSystem::FindFuncCandidates(hstring func_name) noexcept -> vector<ptr<ScriptFuncDesc>>
{
    FO_STACK_TRACE_ENTRY();

    vector<ptr<ScriptFuncDesc>> result;
    const auto range = _globalFuncMap.equal_range(func_name);

    for (auto it = range.first; it != range.second; ++it) {
        result.emplace_back(it->second);
    }

    return result;
}

void ScriptSystem::AddGlobalScriptFunc(ptr<ScriptFuncDesc> func)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(func->Name, "Script function descriptor has no name");

    _globalFuncMap.emplace(func->Name, func);
}

void ScriptSystem::InitModules()
{
    FO_STACK_TRACE_ENTRY();

    UnfreezeGlobalVars();

    for (auto& func : _initFunc | std::views::keys) {
        if (!func.Call()) {
            throw ScriptSystemException("Module initialization failed");
        }
    }

    FreezeGlobalVars();
}

auto ScriptSystem::AreComplexScriptTypesCompatible(const ComplexTypeDesc& func_type, const ComplexTypeDesc& caller_type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (func_type.Kind != caller_type.Kind) {
        return false;
    }
    if (func_type.BaseType != caller_type.BaseType) {
        return false;
    }
    if (func_type.KeyType != caller_type.KeyType) {
        return false;
    }

    // Not comparing IsMutable.
    return true;
}

auto ScriptHelpers::GetIntConvertibleEntityProperty(ptr<const BaseEngine> engine, string_view type_name, int32_t prop_index) -> ptr<const Property>
{
    FO_STACK_TRACE_ENTRY();

    auto prop_reg = engine->GetPropertyRegistrator(type_name);
    FO_VERIFY_AND_THROW(prop_reg, "Missing required property registrator");
    auto prop = prop_reg->GetPropertyByIndex(prop_index);

    if (!prop) {
        throw ScriptException("Invalid property index", type_name, prop_index);
    }

    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled", type_name, prop_index);
    }
    if (!prop->IsPlainData()) {
        throw ScriptException("Property is not plain data", type_name, prop_index);
    }

    return prop;
}

FO_END_NAMESPACE
