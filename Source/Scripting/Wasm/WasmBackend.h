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

#if FO_WASM_SCRIPTING && !FO_WEB

#include "ScriptSystem.h"
#include "WasmApiBridge.h"

#include <wasm_export.h>

FO_BEGIN_NAMESPACE

struct ScriptSettings;
class EngineMetadata;
class File;
class FileSystem;
class BaseEngine;
struct WasmApiNativeMethod;

class WasmBackend final : public ScriptSystemBackend
{
public:
    explicit WasmBackend(ptr<const ScriptSettings> settings);
    WasmBackend(const WasmBackend&) = delete;
    WasmBackend(WasmBackend&&) noexcept = delete;
    auto operator=(const WasmBackend&) = delete;
    auto operator=(WasmBackend&&) noexcept = delete;
    ~WasmBackend() override;

    void RegisterMetadata(ptr<EngineMetadata> meta);
    void LoadScripts(const FileSystem& resources);

private:
    struct WasmFunction;
    struct WasmModule;

    void LoadModule(const File& file);
    void RegisterModuleExports(ptr<WasmModule> module);
    void RegisterApiNativeSymbols();
    void UnregisterApiNativeSymbols() noexcept;
    void UnloadModules() noexcept;
    void CallWasmFunction(ptr<WasmModule> module, ptr<const WasmFunction> function, FuncCallData& call) const;

    ptr<const ScriptSettings> _settings;
    nptr<EngineMetadata> _meta {};
    nptr<BaseEngine> _engine {};
    nptr<ScriptSystem> _scriptSys {};
    WasmApiImportTable _apiImports {};
    vector<WasmApiNativeMethod> _apiNativeMethods {};
    vector<string> _apiNativeSymbolNames {};
    vector<string> _apiNativeSymbolSignatures {};
    vector<NativeSymbol> _apiNativeSymbols {};
    bool _apiNativeSymbolsRegistered {};
    vector<unique_ptr<WasmModule>> _modules {};
};

FO_END_NAMESPACE

#endif
