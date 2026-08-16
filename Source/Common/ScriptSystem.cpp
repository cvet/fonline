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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
#include "PropertiesSerializer.h"

FO_BEGIN_NAMESPACE

static constexpr string_view TEMPORARY_SCRIPT_CALLBACK_TOKEN_PREFIX = "__fonline_callback_";

static auto IsSameScriptComplexType(const ComplexTypeDesc& left, const ComplexTypeDesc& right) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (left.Kind != right.Kind || left.IsMutable != right.IsMutable) {
        return false;
    }

    switch (left.Kind) {
    case ComplexTypeKind::None:
        return true;
    case ComplexTypeKind::Simple:
    case ComplexTypeKind::Array:
        return left.BaseType == right.BaseType;
    case ComplexTypeKind::Dict:
    case ComplexTypeKind::DictOfArray:
        return left.BaseType == right.BaseType && left.KeyType == right.KeyType;
    case ComplexTypeKind::Callback: {
        if (!left.CallbackArgs || !right.CallbackArgs || left.CallbackArgs->size() != right.CallbackArgs->size()) {
            return false;
        }

        for (size_t i = 0; i < left.CallbackArgs->size(); i++) {
            if (!IsSameScriptComplexType((*left.CallbackArgs)[i], (*right.CallbackArgs)[i])) {
                return false;
            }
        }

        return true;
    }
    default:
        return false;
    }
}

static auto IsScriptFuncCompatibleWithCallback(nptr<const ScriptFuncDesc> func, const ComplexTypeDesc& callback_type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!func || !func->Call || callback_type.Kind != ComplexTypeKind::Callback || !callback_type.CallbackArgs || callback_type.CallbackArgs->empty()) {
        return false;
    }

    const vector<ComplexTypeDesc>& callback_args = *callback_type.CallbackArgs;

    if (!IsSameScriptComplexType(func->Ret, callback_args.front())) {
        return false;
    }
    if (func->Args.size() != callback_args.size() - 1) {
        return false;
    }

    for (size_t i = 0; i < func->Args.size(); i++) {
        if (!IsSameScriptComplexType(func->Args[i].Type, callback_args[i + 1])) {
            return false;
        }
    }

    return true;
}

auto NativeDataProvider::NativeDataAccessor::GetNestedArraySize(const BaseTypeDesc& element_type, ptr<void> data) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (element_type.Name == "any") {
        return GetVectorSizeByType<any_t>(data);
    }
    if (element_type.IsString) {
        return GetVectorSizeByType<string>(data);
    }
    if (element_type.IsHashedString) {
        return GetVectorSizeByType<hstring>(data);
    }
    if (element_type.Name == "ident") {
        return GetVectorSizeByType<ident_t>(data);
    }
    if (element_type.Name == "timespan") {
        return GetVectorSizeByType<timespan>(data);
    }
    if (element_type.Name == "nanotime") {
        return GetVectorSizeByType<nanotime>(data);
    }
    if (element_type.Name == "synctime") {
        return GetVectorSizeByType<synctime>(data);
    }
    if (element_type.Name == "ucolor") {
        return GetVectorSizeByType<ucolor>(data);
    }
    if (element_type.Name == "isize") {
        return GetVectorSizeByType<isize32>(data);
    }
    if (element_type.Name == "ipos") {
        return GetVectorSizeByType<ipos32>(data);
    }
    if (element_type.Name == "irect") {
        return GetVectorSizeByType<irect32>(data);
    }
    if (element_type.Name == "ipos16") {
        return GetVectorSizeByType<ipos16>(data);
    }
    if (element_type.Name == "ipos8") {
        return GetVectorSizeByType<ipos8>(data);
    }
    if (element_type.Name == "fsize") {
        return GetVectorSizeByType<fsize32>(data);
    }
    if (element_type.Name == "fpos") {
        return GetVectorSizeByType<fpos32>(data);
    }
    if (element_type.Name == "frect") {
        return GetVectorSizeByType<frect32>(data);
    }
    if (element_type.Name == "mpos") {
        return GetVectorSizeByType<mpos>(data);
    }
    if (element_type.Name == "msize") {
        return GetVectorSizeByType<msize>(data);
    }
    if (element_type.Name == "mdir") {
        return GetVectorSizeByType<mdir>(data);
    }
    if (element_type.Name == "hdir") {
        return GetVectorSizeByType<hdir>(data);
    }
    if (element_type.Name == "GameProperty" || element_type.Name == "PlayerProperty" || element_type.Name == "ItemProperty" || element_type.Name == "CritterProperty" || element_type.Name == "MapProperty" || element_type.Name == "LocationProperty") {
        return GetVectorSizeByType<ScriptEnum_uint16>(data);
    }
    if (element_type.IsEntity || element_type.IsEntityProto || element_type.IsFixedType || element_type.IsRefType) {
        return GetVectorSizeByType<nptr<void>>(data);
    }
    if (element_type.IsBool) {
        return GetVectorSizeByType<bool>(data);
    }
    if (element_type.IsInt8) {
        return GetVectorSizeByType<int8_t>(data);
    }
    if (element_type.IsInt16) {
        return GetVectorSizeByType<int16_t>(data);
    }
    if (element_type.IsInt32) {
        return GetVectorSizeByType<int32_t>(data);
    }
    if (element_type.IsInt64) {
        return GetVectorSizeByType<int64_t>(data);
    }
    if (element_type.IsUInt8) {
        return GetVectorSizeByType<uint8_t>(data);
    }
    if (element_type.IsUInt16) {
        return GetVectorSizeByType<uint16_t>(data);
    }
    if (element_type.IsUInt32) {
        return GetVectorSizeByType<uint32_t>(data);
    }
    if (element_type.IsUInt64) {
        return GetVectorSizeByType<uint64_t>(data);
    }
    if (element_type.IsSingleFloat) {
        return GetVectorSizeByType<float32_t>(data);
    }
    if (element_type.IsDoubleFloat) {
        return GetVectorSizeByType<float64_t>(data);
    }

    throw InvalidCallException(element_type.Name);
}

auto NativeDataProvider::NativeDataAccessor::GetNestedArrayElement(const BaseTypeDesc& element_type, ptr<void> data, size_t index) const -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    if (element_type.Name == "any") {
        return GetVectorElementByType<any_t>(data, index);
    }
    if (element_type.IsString) {
        return GetVectorElementByType<string>(data, index);
    }
    if (element_type.IsHashedString) {
        return GetVectorElementByType<hstring>(data, index);
    }
    if (element_type.Name == "ident") {
        return GetVectorElementByType<ident_t>(data, index);
    }
    if (element_type.Name == "timespan") {
        return GetVectorElementByType<timespan>(data, index);
    }
    if (element_type.Name == "nanotime") {
        return GetVectorElementByType<nanotime>(data, index);
    }
    if (element_type.Name == "synctime") {
        return GetVectorElementByType<synctime>(data, index);
    }
    if (element_type.Name == "ucolor") {
        return GetVectorElementByType<ucolor>(data, index);
    }
    if (element_type.Name == "isize") {
        return GetVectorElementByType<isize32>(data, index);
    }
    if (element_type.Name == "ipos") {
        return GetVectorElementByType<ipos32>(data, index);
    }
    if (element_type.Name == "irect") {
        return GetVectorElementByType<irect32>(data, index);
    }
    if (element_type.Name == "ipos16") {
        return GetVectorElementByType<ipos16>(data, index);
    }
    if (element_type.Name == "ipos8") {
        return GetVectorElementByType<ipos8>(data, index);
    }
    if (element_type.Name == "fsize") {
        return GetVectorElementByType<fsize32>(data, index);
    }
    if (element_type.Name == "fpos") {
        return GetVectorElementByType<fpos32>(data, index);
    }
    if (element_type.Name == "frect") {
        return GetVectorElementByType<frect32>(data, index);
    }
    if (element_type.Name == "mpos") {
        return GetVectorElementByType<mpos>(data, index);
    }
    if (element_type.Name == "msize") {
        return GetVectorElementByType<msize>(data, index);
    }
    if (element_type.Name == "mdir") {
        return GetVectorElementByType<mdir>(data, index);
    }
    if (element_type.Name == "hdir") {
        return GetVectorElementByType<hdir>(data, index);
    }
    if (element_type.Name == "GameProperty" || element_type.Name == "PlayerProperty" || element_type.Name == "ItemProperty" || element_type.Name == "CritterProperty" || element_type.Name == "MapProperty" || element_type.Name == "LocationProperty") {
        return GetVectorElementByType<ScriptEnum_uint16>(data, index);
    }
    if (element_type.IsEntity || element_type.IsEntityProto || element_type.IsFixedType || element_type.IsRefType) {
        return GetVectorElementByType<nptr<void>>(data, index);
    }
    if (element_type.IsBool) {
        throw InvalidCallException(element_type.Name);
    }
    if (element_type.IsInt8) {
        return GetVectorElementByType<int8_t>(data, index);
    }
    if (element_type.IsInt16) {
        return GetVectorElementByType<int16_t>(data, index);
    }
    if (element_type.IsInt32) {
        return GetVectorElementByType<int32_t>(data, index);
    }
    if (element_type.IsInt64) {
        return GetVectorElementByType<int64_t>(data, index);
    }
    if (element_type.IsUInt8) {
        return GetVectorElementByType<uint8_t>(data, index);
    }
    if (element_type.IsUInt16) {
        return GetVectorElementByType<uint16_t>(data, index);
    }
    if (element_type.IsUInt32) {
        return GetVectorElementByType<uint32_t>(data, index);
    }
    if (element_type.IsUInt64) {
        return GetVectorElementByType<uint64_t>(data, index);
    }
    if (element_type.IsSingleFloat) {
        return GetVectorElementByType<float32_t>(data, index);
    }
    if (element_type.IsDoubleFloat) {
        return GetVectorElementByType<float64_t>(data, index);
    }

    throw InvalidCallException(element_type.Name);
}

auto NativeDataProvider::NativeDataAccessor::GetNestedArrayBoolElement(const BaseTypeDesc& element_type, ptr<void> data, size_t index) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (element_type.IsBool) {
        return (*cast_from_void<vector<bool>*>(data.get()))[index];
    }

    return DataAccessor::GetNestedArrayBoolElement(element_type, data, index);
}

void NativeDataProvider::NativeDataAccessor::AddDictArrayElement(ptr<void> data, ptr<void> key, const BaseTypeDesc& element_type, const_span<ptr<void>> values) const
{
    FO_STACK_TRACE_ENTRY();

    ptr<DictDataProxy> dict = cast_from_void<DictDataProxy*>(data.get());

    if (element_type.Name == "any") {
        AddDictVectorElementByType<any_t>(dict, key, values);
        return;
    }
    if (element_type.IsString) {
        AddDictVectorElementByType<string>(dict, key, values);
        return;
    }
    if (element_type.IsHashedString) {
        AddDictVectorElementByType<hstring>(dict, key, values);
        return;
    }
    if (element_type.Name == "ident") {
        AddDictVectorElementByType<ident_t>(dict, key, values);
        return;
    }
    if (element_type.Name == "timespan") {
        AddDictVectorElementByType<timespan>(dict, key, values);
        return;
    }
    if (element_type.Name == "nanotime") {
        AddDictVectorElementByType<nanotime>(dict, key, values);
        return;
    }
    if (element_type.Name == "synctime") {
        AddDictVectorElementByType<synctime>(dict, key, values);
        return;
    }
    if (element_type.Name == "ucolor") {
        AddDictVectorElementByType<ucolor>(dict, key, values);
        return;
    }
    if (element_type.Name == "isize") {
        AddDictVectorElementByType<isize32>(dict, key, values);
        return;
    }
    if (element_type.Name == "ipos") {
        AddDictVectorElementByType<ipos32>(dict, key, values);
        return;
    }
    if (element_type.Name == "irect") {
        AddDictVectorElementByType<irect32>(dict, key, values);
        return;
    }
    if (element_type.Name == "ipos16") {
        AddDictVectorElementByType<ipos16>(dict, key, values);
        return;
    }
    if (element_type.Name == "ipos8") {
        AddDictVectorElementByType<ipos8>(dict, key, values);
        return;
    }
    if (element_type.Name == "fsize") {
        AddDictVectorElementByType<fsize32>(dict, key, values);
        return;
    }
    if (element_type.Name == "fpos") {
        AddDictVectorElementByType<fpos32>(dict, key, values);
        return;
    }
    if (element_type.Name == "frect") {
        AddDictVectorElementByType<frect32>(dict, key, values);
        return;
    }
    if (element_type.Name == "mpos") {
        AddDictVectorElementByType<mpos>(dict, key, values);
        return;
    }
    if (element_type.Name == "msize") {
        AddDictVectorElementByType<msize>(dict, key, values);
        return;
    }
    if (element_type.Name == "mdir") {
        AddDictVectorElementByType<mdir>(dict, key, values);
        return;
    }
    if (element_type.Name == "hdir") {
        AddDictVectorElementByType<hdir>(dict, key, values);
        return;
    }
    if (element_type.Name == "GameProperty" || element_type.Name == "PlayerProperty" || element_type.Name == "ItemProperty" || element_type.Name == "CritterProperty" || element_type.Name == "MapProperty" || element_type.Name == "LocationProperty") {
        AddDictVectorElementByType<ScriptEnum_uint16>(dict, key, values);
        return;
    }
    if (element_type.IsEntity || element_type.IsEntityProto || element_type.IsFixedType || element_type.IsRefType) {
        AddDictVectorElementByType<nptr<void>>(dict, key, values);
        return;
    }
    if (element_type.IsBool) {
        AddDictVectorElementByType<bool>(dict, key, values);
        return;
    }
    if (element_type.IsInt8) {
        AddDictVectorElementByType<int8_t>(dict, key, values);
        return;
    }
    if (element_type.IsInt16) {
        AddDictVectorElementByType<int16_t>(dict, key, values);
        return;
    }
    if (element_type.IsInt32) {
        AddDictVectorElementByType<int32_t>(dict, key, values);
        return;
    }
    if (element_type.IsInt64) {
        AddDictVectorElementByType<int64_t>(dict, key, values);
        return;
    }
    if (element_type.IsUInt8) {
        AddDictVectorElementByType<uint8_t>(dict, key, values);
        return;
    }
    if (element_type.IsUInt16) {
        AddDictVectorElementByType<uint16_t>(dict, key, values);
        return;
    }
    if (element_type.IsUInt32) {
        AddDictVectorElementByType<uint32_t>(dict, key, values);
        return;
    }
    if (element_type.IsUInt64) {
        AddDictVectorElementByType<uint64_t>(dict, key, values);
        return;
    }
    if (element_type.IsSingleFloat) {
        AddDictVectorElementByType<float32_t>(dict, key, values);
        return;
    }
    if (element_type.IsDoubleFloat) {
        AddDictVectorElementByType<float64_t>(dict, key, values);
        return;
    }

    throw InvalidCallException(element_type.Name);
}

DynamicRefTypeInstance::DynamicRefTypeInstance(ptr<const PropertyRegistrar> registrar) noexcept :
    _registrar {registrar},
    _props {std::in_place, _registrar}
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
    FO_VERIFY_AND_THROW(base_type.RefType->FieldsRegistrar, "Reference type has no fields registrar");
    auto fields_registrar = base_type.RefType->FieldsRegistrar;
    FO_VERIFY_AND_THROW(fields_registrar == _registrar, "Dynamic ref-type raw data belongs to a different fields registrar", fields_registrar->GetTypeName(), _registrar->GetTypeName());

    _cachedRawDataDirty = true;
    _props.emplace(_registrar);
    auto props = GetProps();

    size_t data_pos = 0;

    for (size_t i = 1; i < fields_registrar->GetPropertiesCount(); i++) {
        auto field_prop = fields_registrar->GetPropertyByIndex(numeric_cast<int32_t>(i));
        FO_VERIFY_AND_THROW(field_prop, "Field property is null");
        span<const uint8_t> field_raw_data {};

        if (data_pos < raw_data.size()) {
            data_pos = align_up(data_pos, sizeof(uint32_t));

            if (data_pos > raw_data.size() || raw_data.size() - data_pos < sizeof(uint32_t)) {
                throw PropertySerializationException("Corrupted ref type property data", base_type.Name, field_prop->GetName());
            }

            uint32_t field_size = span_read_object<uint32_t>(raw_data, data_pos);

            if (field_prop->IsPlainData() && field_size != 0 && field_size != field_prop->GetBaseSize()) {
                throw PropertySerializationException("Wrong ref field raw size", base_type.Name, field_prop->GetName());
            }

            size_t field_data_size = field_size;

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

    FO_VERIFY_AND_THROW(prop->GetRegistrar() == _registrar, "Dynamic ref-type property belongs to a different registrar", prop->GetName(), prop->GetRegistrar()->GetTypeName(), _registrar->GetTypeName());
    FO_VERIFY_AND_THROW(_props, "Missing required properties");
    return _props->GetRawData(prop);
}

void DynamicRefTypeInstance::SetValue(ptr<const Property> prop, PropertyRawData& prop_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(prop->GetRegistrar() == _registrar, "Dynamic ref-type property belongs to a different registrar", prop->GetName(), prop->GetRegistrar()->GetTypeName(), _registrar->GetTypeName());
    FO_VERIFY_AND_THROW(_props, "Missing required properties");

    GetProps()->SetValue(prop, prop_data);
    _cachedRawDataDirty = true;
}

auto DynamicRefTypeInstance::GetSerializedRawData(const BaseTypeDesc& base_type) -> const_span<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsRefType, "Base type is not a reference type");
    FO_VERIFY_AND_THROW(base_type.RefType, "Reference type descriptor is null");
    FO_VERIFY_AND_THROW(base_type.RefType->FieldsRegistrar, "Reference type has no fields registrar");
    auto fields_registrar = base_type.RefType->FieldsRegistrar;
    FO_VERIFY_AND_THROW(fields_registrar == _registrar, "Dynamic ref-type serialization requested a different fields registrar", fields_registrar->GetTypeName(), _registrar->GetTypeName());
    FO_VERIFY_AND_THROW(_props, "Missing required properties");

    if (_cachedRawDataDirty) {
        vector<span<const uint8_t>> field_raw_entries(fields_registrar->GetPropertiesCount());
        vector<bool> field_is_default(fields_registrar->GetPropertiesCount(), true);
        size_t last_non_default_field = 0;

        for (size_t i = 1; i < fields_registrar->GetPropertiesCount(); i++) {
            auto field_prop = fields_registrar->GetPropertyByIndex(numeric_cast<int32_t>(i));
            auto field_raw_data = GetProps()->GetRawData(field_prop);

            bool is_default = field_raw_data.empty();

            if (!is_default && field_prop->IsPlainData()) {
                is_default = true;

                for (auto byte : field_raw_data) {
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
                    auto field_prop = fields_registrar->GetPropertyByIndexUnsafe(i);
                    data_size = align_up(data_size, field_prop->GetDataAlignment());
                    data_size += field_raw_entries[i].size();
                }
            }

            _cachedRawData.assign(data_size, 0);
            auto raw_buffer = make_span(_cachedRawData);
            size_t data_pos = 0;

            for (size_t i = 1; i <= last_non_default_field; i++) {
                uint32_t field_size = !field_is_default[i] ? numeric_cast<uint32_t>(field_raw_entries[i].size()) : 0;
                data_pos = align_up(data_pos, sizeof(uint32_t));
                span_write_object(raw_buffer, data_pos, field_size);

                if (field_size != 0) {
                    auto field_prop = fields_registrar->GetPropertyByIndexUnsafe(i);
                    data_pos = align_up(data_pos, field_prop->GetDataAlignment());
                    span_write_bytes(raw_buffer, data_pos, field_raw_entries[i]);
                }
            }

            FO_VERIFY_AND_THROW(data_pos == data_size, "Dynamic ref-type cached raw buffer size does not match bytes written", _registrar->GetTypeName(), data_pos, data_size);
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
    _temporaryScriptCallbacks.clear();
    _temporaryScriptCallbackCounter = 0;
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

    auto check_type = [this](const ComplexTypeDesc& left, size_t right) -> bool {
        auto it = _engineTypes.find(right);
        FO_VERIFY_AND_RETURN_VALUE(it != _engineTypes.end(), false, "Script engine type index is not registered while validating arguments", right);
        return IsSameScriptComplexType(left, it->second);
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

auto ScriptSystem::FindFuncDesc(hstring func_name, const ComplexTypeDesc& callback_type) noexcept -> nptr<ScriptFuncDesc>
{
    FO_STACK_TRACE_ENTRY();

    auto range = _globalFuncMap.equal_range(func_name);

    for (auto it = range.first; it != range.second; ++it) {
        ptr<ScriptFuncDesc> func = it->second;

        if (IsScriptFuncCompatibleWithCallback(func, callback_type)) {
            return func;
        }
    }

    return nullptr;
}

auto ScriptSystem::FindFuncDesc(hstring func_name, const ComplexTypeDesc& ret_type, const_span<ComplexTypeDesc> arg_types) noexcept -> nptr<ScriptFuncDesc>
{
    FO_STACK_TRACE_ENTRY();

    auto range = _globalFuncMap.equal_range(func_name);

    for (auto it = range.first; it != range.second; ++it) {
        ptr<ScriptFuncDesc> func = it->second;

        if (!func->Call || func->Args.size() != arg_types.size()) {
            continue;
        }
        if (!IsSameScriptComplexType(func->Ret, ret_type)) {
            continue;
        }

        bool args_match = true;

        for (size_t i = 0; i < arg_types.size(); i++) {
            if (!IsSameScriptComplexType(func->Args[i].Type, arg_types[i])) {
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

auto ScriptSystem::IsTemporaryScriptCallbackToken(string_view token) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return token.starts_with(TEMPORARY_SCRIPT_CALLBACK_TOKEN_PREFIX);
}

auto ScriptSystem::PushTemporaryScriptCallbackScope() const noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _temporaryScriptCallbacks.size();
}

void ScriptSystem::PopTemporaryScriptCallbackScope(size_t scope) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (scope > _temporaryScriptCallbacks.size()) {
        FO_STRONG_ASSERT(false, "Temporary script callback scope is out of range");
        return;
    }

    size_t index = _temporaryScriptCallbacks.size();

    while (index > scope) {
        index--;

        TemporaryScriptCallback& callback = _temporaryScriptCallbacks[index];

        if (callback.RetainCount != 0) {
            callback.ScopeReleased = true;
            continue;
        }

        _temporaryScriptCallbacks.erase(std::next(_temporaryScriptCallbacks.begin(), numeric_cast<ptrdiff_t>(index)));
    }
}

auto ScriptSystem::RegisterTemporaryScriptCallback(unique_del_ptr<ScriptFuncDesc> func) -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(func->Call, "Temporary script callback has no call entry");

    _temporaryScriptCallbackCounter++;
    string token = strex("{}{}", TEMPORARY_SCRIPT_CALLBACK_TOKEN_PREFIX, _temporaryScriptCallbackCounter).str();
    _temporaryScriptCallbacks.emplace_back(TemporaryScriptCallback {
        .Token = token,
        .Func = std::move(func),
    });
    return token;
}

auto ScriptSystem::RetainTemporaryScriptCallback(string_view token) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IsTemporaryScriptCallbackToken(token)) {
        return false;
    }

    for (size_t index = _temporaryScriptCallbacks.size(); index != 0; index--) {
        TemporaryScriptCallback& callback = _temporaryScriptCallbacks[index - 1];

        if (callback.Token != token) {
            continue;
        }

        callback.RetainCount++;
        return true;
    }

    return false;
}

auto ScriptSystem::ReleaseTemporaryScriptCallback(string_view token) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IsTemporaryScriptCallbackToken(token)) {
        return false;
    }

    for (size_t index = _temporaryScriptCallbacks.size(); index != 0; index--) {
        TemporaryScriptCallback& callback = _temporaryScriptCallbacks[index - 1];

        if (callback.Token != token) {
            continue;
        }
        if (callback.RetainCount == 0) {
            return false;
        }

        callback.RetainCount--;

        if (callback.RetainCount == 0 && callback.ScopeReleased) {
            _temporaryScriptCallbacks.erase(std::next(_temporaryScriptCallbacks.begin(), numeric_cast<ptrdiff_t>(index - 1)));
        }

        return true;
    }

    return false;
}

auto ScriptSystem::FindTemporaryScriptCallback(string_view token, const ComplexTypeDesc& callback_type) noexcept -> nptr<ScriptFuncDesc>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IsTemporaryScriptCallbackToken(token)) {
        return nullptr;
    }

    for (auto it = _temporaryScriptCallbacks.rbegin(); it != _temporaryScriptCallbacks.rend(); ++it) {
        if (it->Token != token) {
            continue;
        }

        ptr<ScriptFuncDesc> func = it->Func;
        return IsScriptFuncCompatibleWithCallback(func, callback_type) ? nptr<ScriptFuncDesc> {func} : nullptr;
    }

    return nullptr;
}

auto ScriptSystem::FindFunc(hstring func_name, const_span<size_t> arg_types) noexcept -> nptr<ScriptFuncDesc>
{
    FO_STACK_TRACE_ENTRY();

    auto range = _globalFuncMap.equal_range(func_name);

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

    auto range = _globalFuncMap.equal_range(func_name);

    auto args_compatible = [](const ComplexTypeDesc& func_arg, const ComplexTypeDesc& caller_arg) noexcept {
        if (func_arg.Kind != caller_arg.Kind) {
            return false;
        }
        if (func_arg.BaseType != caller_arg.BaseType) {
            return false;
        }
        if (func_arg.KeyType != caller_arg.KeyType) {
            return false;
        }

        // Not comparing IsMutable
        return true;
    };

    for (auto it = range.first; it != range.second; ++it) {
        auto func = it->second;

        if (!func->Call || func->Ret.Kind != ComplexTypeKind::None || func->Args.size() != arg_types.size()) {
            continue;
        }

        bool args_match = true;

        for (size_t i = 0; i < arg_types.size(); i++) {
            if (!args_compatible(func->Args[i].Type, arg_types[i])) {
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

auto ScriptHelpers::GetIntConvertibleEntityProperty(ptr<const BaseEngine> engine, string_view type_name, int32_t prop_index) -> ptr<const Property>
{
    FO_STACK_TRACE_ENTRY();

    auto prop_reg = engine->GetPropertyRegistrar(type_name);
    FO_VERIFY_AND_THROW(prop_reg, "Missing required property registrar");
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
