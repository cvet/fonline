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

auto ScriptSystem::ValidateArgs(const ScriptFuncDesc& func_desc, initializer_list<std::type_index> args_type, std::type_index ret_type) const -> bool
{
    STACK_TRACE_ENTRY();

    if (!func_desc.CallSupported) {
        return false;
    }

    if (func_desc.ArgsType.size() != args_type.size()) {
        return false;
    }

    if (func_desc.RetType != ret_type && string_view(func_desc.RetType.name()) != string_view(ret_type.name())) {
        return false;
    }

    size_t index = 0;

    for (const auto& arg_type : args_type) {
        if (arg_type != func_desc.ArgsType[index] && string_view(arg_type.name()) != string_view(func_desc.ArgsType[index].name())) {
            return false;
        }

        ++index;
    }

    return true;
}

void ScriptSystem::InitModules()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (const auto* func : _initFunc) {
        if (!func->Call({}, nullptr)) {
            throw ScriptSystemException("Module initialization failed");
        }
    }
}

void ScriptSystem::HandleRemoteCall(uint rpc_num, Entity* entity)
{
    STACK_TRACE_ENTRY();

    const auto it = _rpcReceivers.find(rpc_num);
    if (it == _rpcReceivers.end()) {
        throw ScriptException("Invalid remote call", rpc_num);
    }

    it->second(entity);
}

void ScriptSystem::Process()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto&& callback : _loopCallbacks) {
        try {
            callback();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
        catch (...) {
            UNKNOWN_EXCEPTION();
        }
    }
}

auto ScriptHelpers::GetIntConvertibleEntityProperty(const FOEngineBase* engine, string_view type_name, int prop_index) -> const Property*
{
    STACK_TRACE_ENTRY();

    const auto* prop_reg = engine->GetPropertyRegistrator(type_name);
    RUNTIME_ASSERT(prop_reg);
    const auto* prop = prop_reg->GetPropertyByIndex(static_cast<int>(prop_index));

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
