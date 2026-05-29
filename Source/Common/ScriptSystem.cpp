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
#include "FileSystem.h"
#include "Geometry.h"
#include "Properties.h"
#include "PropertiesSerializator.h"

FO_BEGIN_NAMESPACE

static void AddModuleFunc(vector<pair<ScriptFunc<void>, int32_t>>& funcs, ScriptFunc<void> func, int32_t priority)
{
    FO_STACK_TRACE_ENTRY();

    funcs.emplace_back(std::move(func), priority);
    std::ranges::stable_sort(funcs, [](auto&& a, auto&& b) { return a.second < b.second; });
}

static void RunModuleFuncs(vector<pair<ScriptFunc<void>, int32_t>>& funcs, string_view error)
{
    FO_STACK_TRACE_ENTRY();

    for (auto& func : funcs | std::views::keys) {
        if (!func.Call()) {
            throw ScriptSystemException(error);
        }
    }
}

DynamicRefTypeInstance::DynamicRefTypeInstance(const PropertyRegistrator* registrator) noexcept :
    _registrator {registrator},
    _props {SafeAlloc::MakeUnique<Properties>(registrator)}
{
}

DynamicRefTypeInstance::~DynamicRefTypeInstance() noexcept = default;

void DynamicRefTypeInstance::LoadFromRawData(const BaseTypeDesc& base_type, span<const uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(base_type.IsRefType);
    FO_RUNTIME_ASSERT(base_type.RefType);
    FO_RUNTIME_ASSERT(base_type.RefType->FieldsRegistrator);
    const auto* fields_registrator = base_type.RefType->FieldsRegistrator.get();
    FO_RUNTIME_ASSERT(fields_registrator == _registrator);

    _props = SafeAlloc::MakeUnique<Properties>(_registrator.get());

    const auto* pdata = raw_data.data();
    const auto* pdata_end = raw_data.data() + raw_data.size();

    for (size_t i = 1; i < fields_registrator->GetPropertiesCount(); i++) {
        const auto* field_prop = fields_registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));
        span<const uint8_t> field_raw_data {};

        if (pdata < pdata_end) {
            if (static_cast<size_t>(pdata_end - pdata) < sizeof(uint32_t)) {
                throw PropertySerializationException("Corrupted ref type property data", base_type.Name, field_prop->GetName());
            }

            uint32_t field_size;
            MemCopy(&field_size, pdata, sizeof(field_size));
            pdata += sizeof(field_size);

            if (field_prop->IsPlainData() && field_size != 0 && field_size != field_prop->GetBaseSize()) {
                throw PropertySerializationException("Wrong ref field raw size", base_type.Name, field_prop->GetName());
            }
            if (static_cast<size_t>(pdata_end - pdata) < field_size) {
                throw PropertySerializationException("Corrupted ref type property data", base_type.Name, field_prop->GetName());
            }

            field_raw_data = {pdata, field_size};
            pdata += field_size;
        }

        if (!field_raw_data.empty()) {
            _props->SetRawData(field_prop, field_raw_data);
        }
    }

    if (pdata != pdata_end) {
        throw PropertySerializationException("Corrupted ref type property data", base_type.Name);
    }

    _cachedRawData.assign(raw_data.begin(), raw_data.end());
    _cachedRawDataDirty = false;
}

auto DynamicRefTypeInstance::GetRawData(const Property* prop) const -> span<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop != nullptr);
    FO_RUNTIME_ASSERT(prop->GetRegistrator() == _registrator);
    FO_RUNTIME_ASSERT(_props);
    return _props->GetRawData(prop);
}

void DynamicRefTypeInstance::SetValue(const Property* prop, PropertyRawData& prop_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop != nullptr);
    FO_RUNTIME_ASSERT(prop->GetRegistrator() == _registrator);
    FO_RUNTIME_ASSERT(_props);

    _props->SetValue(prop, prop_data);
    _cachedRawDataDirty = true;
}

auto DynamicRefTypeInstance::GetSerializedRawData(const BaseTypeDesc& base_type) -> const vector<uint8_t>&
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(base_type.IsRefType);
    FO_RUNTIME_ASSERT(base_type.RefType);
    FO_RUNTIME_ASSERT(base_type.RefType->FieldsRegistrator);
    const auto* fields_registrator = base_type.RefType->FieldsRegistrator.get();
    FO_RUNTIME_ASSERT(fields_registrator == _registrator);
    FO_RUNTIME_ASSERT(_props);

    if (_cachedRawDataDirty) {
        vector<span<const uint8_t>> field_raw_entries(fields_registrator->GetPropertiesCount());
        vector<bool> field_is_default(fields_registrator->GetPropertiesCount(), true);
        size_t last_non_default_field = 0;

        for (size_t i = 1; i < fields_registrator->GetPropertiesCount(); i++) {
            const auto* field_prop = fields_registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));
            const auto field_raw_data = _props->GetRawData(field_prop);

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
                data_size += sizeof(uint32_t);

                if (!field_is_default[i]) {
                    data_size += field_raw_entries[i].size();
                }
            }

            _cachedRawData.resize(data_size);
            auto* pdata = _cachedRawData.data();

            for (size_t i = 1; i <= last_non_default_field; i++) {
                const uint32_t field_size = !field_is_default[i] ? numeric_cast<uint32_t>(field_raw_entries[i].size()) : 0;
                MemCopy(pdata, &field_size, sizeof(field_size));
                pdata += sizeof(field_size);

                if (field_size != 0) {
                    MemCopy(pdata, field_raw_entries[i].data(), field_size);
                    pdata += field_size;
                }
            }

            FO_RUNTIME_ASSERT(static_cast<size_t>(pdata - _cachedRawData.data()) == data_size);
        }

        _cachedRawDataDirty = false;
    }

    return _cachedRawData;
}

void ScriptSystem::MapScriptTypes(EngineMetadata* meta)
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

    _backends.resize(index + 1);
    FO_RUNTIME_ASSERT(!_backends[index]);
    _backends[index] = std::move(backend);
}

void ScriptSystem::ShutdownBackends()
{
    FO_STACK_TRACE_ENTRY();

    _loopCallbacks.clear();
    _engineTypes.clear();
    _globalFuncMap.clear();
    _initFunc.clear();
    _backends.clear();
}

void ScriptSystem::AddInitFunc(ScriptFunc<void> func, int32_t priority)
{
    FO_STACK_TRACE_ENTRY();

    AddModuleFunc(_initFunc, std::move(func), priority);
}

auto ScriptSystem::ValidateArgs(const ScriptFuncDesc* func, const_span<size_t> arg_types, size_t ret_type) const noexcept -> bool
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
        FO_RUNTIME_VERIFY_AND_RETURN(it != _engineTypes.end(), false);
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

void ScriptSystem::AddLoopCallback(function<void()> callback)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(callback);

    _loopCallbacks.emplace_back(std::move(callback));
}

void ScriptSystem::AddGlobalScriptFunc(ScriptFuncDesc* func)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(func);
    FO_RUNTIME_ASSERT(func->Name);

    _globalFuncMap.emplace(func->Name, func);
}

void ScriptSystem::InitModules()
{
    FO_STACK_TRACE_ENTRY();

    RunModuleFuncs(_initFunc, "Module initialization failed");
}

void ScriptSystem::ProcessScriptEvents()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& callback : _loopCallbacks) {
        try {
            callback();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
    }
}

auto ScriptHelpers::GetIntConvertibleEntityProperty(const BaseEngine* engine, string_view type_name, int32_t prop_index) -> const Property*
{
    FO_STACK_TRACE_ENTRY();

    const auto* prop_reg = engine->GetPropertyRegistrator(type_name);
    FO_RUNTIME_ASSERT(prop_reg);
    const auto* prop = prop_reg->GetPropertyByIndex(prop_index);

    if (prop == nullptr) {
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
