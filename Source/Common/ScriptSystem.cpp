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
#include "AngelScriptScripting.h"
#include "Application.h"
#include "EngineBase.h"
#include "FileSystem.h"
#include "Geometry.h"

FO_BEGIN_NAMESPACE

void ScriptSystem::InitSubsystems(BaseEngine* engine)
{
    FO_STACK_TRACE_ENTRY();

    InitSubsystems(engine, engine->Resources);
}

void ScriptSystem::InitSubsystems(EngineMetadata* meta, const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    MapEngineType<bool>(meta->GetBaseType("bool"));
    MapEngineType<int8>(meta->GetBaseType("int8"));
    MapEngineType<int16>(meta->GetBaseType("int16"));
    MapEngineType<int32>(meta->GetBaseType("int32"));
    MapEngineType<int64>(meta->GetBaseType("int64"));
    MapEngineType<uint8>(meta->GetBaseType("uint8"));
    MapEngineType<uint16>(meta->GetBaseType("uint16"));
    MapEngineType<uint32>(meta->GetBaseType("uint32"));
    MapEngineType<uint64>(meta->GetBaseType("uint64"));
    MapEngineType<float32>(meta->GetBaseType("float32"));
    MapEngineType<float64>(meta->GetBaseType("float64"));
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
    MapEngineType<string>(meta->GetBaseType("string"));
    MapEngineType<hstring>(meta->GetBaseType("hstring"));
    MapEngineType<any_t>(meta->GetBaseType("any"));
    MapEngineType<Entity>(meta->GetBaseType("Entity"));

#if FO_ANGELSCRIPT_SCRIPTING
    InitAngelScriptScripting(meta, resources);
#endif

    ignore_unused(resources);
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

void ScriptSystem::AddInitFunc(ScriptFunc<void> func, int32 priority)
{
    FO_STACK_TRACE_ENTRY();

    _initFunc.emplace_back(std::move(func), priority);
    std::ranges::stable_sort(_initFunc, [](auto&& a, auto&& b) { return a.second < b.second; });
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
        FO_RUNTIME_VERIFY(it != _engineTypes.end(), false);
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

    for (auto& func : _initFunc | std::views::keys) {
        if (!func.Call()) {
            throw ScriptSystemException("Module initialization failed");
        }
    }
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

auto ScriptHelpers::GetIntConvertibleEntityProperty(const BaseEngine* engine, string_view type_name, int32 prop_index) -> const Property*
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
