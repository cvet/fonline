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

#pragma once

#include "Common.h"

#if FO_WASM_SCRIPTING && FO_WEB

#include "ScriptSystem.h"
#include "WasmApiBridge.h"

#include <json.hpp>

FO_BEGIN_NAMESPACE

struct ScriptSettings;
class EngineMetadata;
class FileSystem;
class BaseEngine;

class WebWasmBackend final : public ScriptSystemBackend
{
public:
    explicit WebWasmBackend(ptr<const ScriptSettings> settings);
    WebWasmBackend(const WebWasmBackend&) = delete;
    WebWasmBackend(WebWasmBackend&&) noexcept = delete;
    auto operator=(const WebWasmBackend&) = delete;
    auto operator=(WebWasmBackend&&) noexcept = delete;
    ~WebWasmBackend() override;

    void RegisterMetadata(ptr<EngineMetadata> meta);
    void LoadScripts(const FileSystem& resources);

private:
    struct WasmFunction;
    struct WasmModule;

    void RegisterModule(ptr<WasmModule> module, const nlohmann::json& module_json);
    void RegisterApiImports();
    void CallWasmFunction(ptr<const WasmModule> module, ptr<const WasmFunction> function, FuncCallData& call) const;
    void CallApiImport(size_t method_index, const_span<uint64_t> raw_args, uint64_t* raw_result);
    void CallPropertyImport(size_t property_index, const_span<uint64_t> raw_args, uint64_t* raw_result);
    static auto CallApiImportCallback(uintptr_t backend_ptr, int32_t method_index, int32_t argc, const uint64_t* raw_args, uint64_t* raw_result) -> int32_t;
    static auto CallPropertyImportCallback(uintptr_t backend_ptr, int32_t property_index, int32_t argc, const uint64_t* raw_args, uint64_t* raw_result) -> int32_t;

    nptr<EngineMetadata> _meta {};
    nptr<BaseEngine> _engine {};
    nptr<ScriptSystem> _scriptSys {};
    WasmApiImportTable _apiImports {};
    vector<unique_ptr<WasmModule>> _modules {};
};

FO_END_NAMESPACE

#endif
