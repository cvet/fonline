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

#if FO_MANAGED_SCRIPTING

#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

class ManagedScriptBackend final : public ScriptSystemBackend
{
public:
    ~ManagedScriptBackend() override;

    [[nodiscard]] auto GetDomain() const -> void* { return _domain.get_no_const(); }
    [[nodiscard]] auto GetMetadata() const noexcept -> EngineMetadata* { return _meta.get_no_const(); }
    [[nodiscard]] auto GetGlobalEntity() const noexcept -> Entity*;
    [[nodiscard]] auto GetImages() const noexcept -> const vector<nptr<void>>& { return _images; }

    void RegisterMetadata(EngineMetadata* meta);
    void LoadAssemblies(const FileSystem& resources, string_view bake_output_dir = {});
    void BindRequiredStuff();
    void AddManagedGlobalFunc(unique_ptr<ScriptFuncDesc> desc, uint32_t gc_handle);
    void AddRemoteCallHandlerGcHandle(uint32_t gc_handle) { _globalFuncGcHandles.emplace_back(gc_handle); }

private:
    static auto GetTargetName(EngineSideKind side) -> string_view;
    static auto SplitEnvList(const char* raw) -> vector<string>;
    static auto BuildAssemblyPathCandidates(const std::filesystem::path& base_dir, string_view target_name, string_view assembly_name) -> vector<std::filesystem::path>;
    static auto FindAssemblyPath(string_view target_name, string_view assembly_name) -> optional<std::filesystem::path>;

    void InvokeInitializator(void* assembly, const char* method_name);

    nptr<EngineMetadata> _meta {};
    nptr<ScriptSystem> _scriptSys {};
    nptr<void> _domain {};
    vector<nptr<void>> _images {};
    vector<unique_ptr<ScriptFuncDesc>> _globalFuncs {};
    vector<uint32_t> _globalFuncGcHandles {};
};

FO_END_NAMESPACE

#endif
