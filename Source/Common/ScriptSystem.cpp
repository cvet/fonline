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

#include "ScriptSystem.h"
#include "Application.h"
#include "EngineBase.h"
#include "Geometry.h"

FO_BEGIN_NAMESPACE();

ScriptSystem::ScriptSystem()
{
    FO_STACK_TRACE_ENTRY();

    _engineToScriptType.emplace(typeid(void).name(), SafeAlloc::MakeShared<ScriptTypeInfo>("void", nullptr));
    MapEnginePlainType<bool>("bool");
    MapEnginePlainType<int8>("int8");
    MapEnginePlainType<int16>("int16");
    MapEnginePlainType<int32>("int32");
    MapEnginePlainType<int64>("int64");
    MapEnginePlainType<uint8>("uint8");
    MapEnginePlainType<uint16>("uint16");
    MapEnginePlainType<uint32>("uint32");
    MapEnginePlainType<uint64>("uint64");
    MapEnginePlainType<float32>("float32");
    MapEnginePlainType<float64>("float64");
    MapEnginePlainType<ident_t>("ident");
    MapEnginePlainType<timespan>("timespan");
    MapEnginePlainType<nanotime>("nanotime");
    MapEnginePlainType<synctime>("synctime");
    MapEnginePlainType<ucolor>("ucolor");
    MapEnginePlainType<isize32>("isize32");
    MapEnginePlainType<ipos32>("ipos32");
    MapEnginePlainType<irect32>("irect32");
    MapEnginePlainType<ipos16>("ipos16");
    MapEnginePlainType<ipos8>("ipos8");
    MapEnginePlainType<fsize32>("fsize32");
    MapEnginePlainType<fpos32>("fpos32");
    MapEnginePlainType<frect32>("frect32");
    MapEnginePlainType<mpos>("mpos");
    MapEnginePlainType<msize>("msize");
    MapEngineObjectType<string>("string");
    MapEngineObjectType<hstring>("hstring");
    MapEngineObjectType<any_t>("any");
    MapEngineEntityType<Entity>("Entity");
}

void ScriptSystem::RegisterBackend(size_t index, shared_ptr<ScriptSystemBackend> backend)
{
    FO_STACK_TRACE_ENTRY();

    _backends.resize(index + 1);
    FO_RUNTIME_ASSERT(!_backends[index]);

    _backends[index] = std::move(backend);
}

auto ScriptSystem::ResolveEngineType(std::type_index ti) const -> shared_ptr<ScriptTypeInfo>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _engineToScriptType.find(ti.name());

    if (it == _engineToScriptType.end()) {
        throw GenericException("Invalid script type", ti.name());
    }

    return it->second;
}

void ScriptSystem::AddInitFunc(ScriptFuncDesc* func, int32 priority)
{
    FO_STACK_TRACE_ENTRY();

    _initFunc.emplace_back(func, priority);
    std::ranges::stable_sort(_initFunc, [](auto&& a, auto&& b) { return a.second < b.second; });
}

auto ScriptSystem::ValidateArgs(const ScriptFuncDesc& func_desc, initializer_list<std::type_index> args_type, std::type_index ret_type) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!func_desc.CallSupported) {
        return false;
    }

    if (func_desc.ArgsType.size() != args_type.size()) {
        return false;
    }

    const auto check_type = [this](const shared_ptr<ScriptTypeInfo>& left, const std::type_index& right) -> bool {
        const auto it = _engineToScriptType.find(right.name());
        FO_RUNTIME_VERIFY(it != _engineToScriptType.end(), false);
        return it->second == left;
    };

    if (!check_type(func_desc.RetType, ret_type)) {
        return false;
    }

    size_t index = 0;

    for (const auto& arg_type : args_type) {
        if (!check_type(func_desc.ArgsType[index], arg_type)) {
            return false;
        }

        ++index;
    }

    return true;
}

void ScriptSystem::InitModules()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (const auto& func : _initFunc | std::views::keys) {
        if (!func->Call({}, nullptr)) {
            throw ScriptSystemException("Module initialization failed");
        }
    }
}

void ScriptSystem::HandleRemoteCall(uint32 rpc_num, Entity* entity)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _rpcReceivers.find(rpc_num);

    if (it == _rpcReceivers.end()) {
        throw ScriptException("Invalid remote call", rpc_num);
    }

    it->second(entity);
}

void ScriptSystem::Process()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto& callback : _loopCallbacks) {
        try {
            callback();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
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

FO_END_NAMESPACE();
