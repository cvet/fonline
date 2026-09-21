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

#include "catch_amalgamated.hpp"

#include "Common.h"

#if FO_NATIVE_SCRIPTING

#include "NativeScriptSynth.h"

FO_USING_NAMESPACE();

// The authoring contract of a native script module — the rules `LF_NativeScriptSynth` enforces before
// anything compiles, so a misplaced or malformed module fails synthesis with its source path instead of
// producing a dispatcher that silently skips it. Documented in Docs/NativeScripting.md.

static constexpr string_view VALID_SERVER_MODULE = R"(module;

#include <memory>

export module NativeScripts.User.Server.CombatTweaks;

import NativeApi.Server;

export void CombatTweaksInit(const ::NativeScripts::ModuleInitContext& ctx)
{
    (void)ctx;
}
)";

TEST_CASE("Native script module scan reads the module name and its initializers")
{
    const auto scan = ScanNativeScriptModuleSource(VALID_SERVER_MODULE, "Server/CombatTweaks.cppm", "CombatTweaks.cppm", "Server");

    CHECK(scan.Module == "NativeScripts.User.Server.CombatTweaks");
    REQUIRE(scan.Inits.size() == 1);
    CHECK(scan.Inits[0].Module == "NativeScripts.User.Server.CombatTweaks");
    CHECK(scan.Inits[0].Function == "CombatTweaksInit");
    CHECK(scan.Inits[0].SourceFileName == "CombatTweaks.cppm");
}

TEST_CASE("Native script module scan accepts every initializer a module exports")
{
    static constexpr string_view SOURCE = R"(export module NativeScripts.User.Common.Probes;

import NativeApi.Common;

export void FirstProbeInit(const NativeScripts::ModuleInitContext& ctx) { (void)ctx; }
export void SecondProbeInit(const ::NativeScripts::ModuleInitContext &ctx) { (void)ctx; }
)";

    const auto scan = ScanNativeScriptModuleSource(SOURCE, "Common/Probes.cppm", "Probes.cppm", "Common");

    REQUIRE(scan.Inits.size() == 2);
    CHECK(scan.Inits[0].Function == "FirstProbeInit");
    CHECK(scan.Inits[1].Function == "SecondProbeInit");
}

TEST_CASE("Native script module scan tolerates an exported API import")
{
    static constexpr string_view SOURCE = R"(export module NativeScripts.User.Client.UiState;

export import NativeApi.Client;

export void UiStateInit(const ::NativeScripts::ModuleInitContext& ctx) { (void)ctx; }
)";

    CHECK_NOTHROW(ScanNativeScriptModuleSource(SOURCE, "Client/UiState.cppm", "UiState.cppm", "Client"));
}

TEST_CASE("Native script module scan ignores commented-out declarations")
{
    // A commented-out module declaration or initializer must not satisfy the contract: the scanner strips
    // comments first, so the file below reads as having neither
    static constexpr string_view COMMENTED_MODULE = R"(// export module NativeScripts.User.Server.Ghost;
/* export module NativeScripts.User.Server.Ghost; */

import NativeApi.Server;
)";

    CHECK_THROWS_AS(ScanNativeScriptModuleSource(COMMENTED_MODULE, "Server/Ghost.cppm", "Ghost.cppm", "Server"), NativeScriptSynthException);

    static constexpr string_view COMMENTED_INIT = R"(export module NativeScripts.User.Server.Ghost;

import NativeApi.Server;

// export void GhostInit(const ::NativeScripts::ModuleInitContext& ctx) { (void)ctx; }
)";

    CHECK_THROWS_AS(ScanNativeScriptModuleSource(COMMENTED_INIT, "Server/Ghost.cppm", "Ghost.cppm", "Server"), NativeScriptSynthException);
}

TEST_CASE("Native script module scan rejects a module whose role does not match its folder")
{
    static constexpr string_view SOURCE = R"(export module NativeScripts.User.Client.Misplaced;

import NativeApi.Client;

export void MisplacedInit(const ::NativeScripts::ModuleInitContext& ctx) { (void)ctx; }
)";

    CHECK_THROWS_AS(ScanNativeScriptModuleSource(SOURCE, "Server/Misplaced.cppm", "Misplaced.cppm", "Server"), NativeScriptSynthException);
}

TEST_CASE("Native script module scan rejects a module name outside the user namespace")
{
    static constexpr string_view SOURCE = R"(export module Gameplay.CombatTweaks;

import NativeApi.Server;

export void CombatTweaksInit(const ::NativeScripts::ModuleInitContext& ctx) { (void)ctx; }
)";

    CHECK_THROWS_AS(ScanNativeScriptModuleSource(SOURCE, "Server/CombatTweaks.cppm", "CombatTweaks.cppm", "Server"), NativeScriptSynthException);
}

TEST_CASE("Native script module scan requires the role API import")
{
    static constexpr string_view NO_IMPORT = R"(export module NativeScripts.User.Server.CombatTweaks;

export void CombatTweaksInit(const ::NativeScripts::ModuleInitContext& ctx) { (void)ctx; }
)";

    CHECK_THROWS_AS(ScanNativeScriptModuleSource(NO_IMPORT, "Server/CombatTweaks.cppm", "CombatTweaks.cppm", "Server"), NativeScriptSynthException);

    // Importing another role's API is what would let server-only wrappers into a client module, so it is
    // rejected even though the module name itself is in the right place
    static constexpr string_view WRONG_IMPORT = R"(export module NativeScripts.User.Client.UiState;

import NativeApi.Server;

export void UiStateInit(const ::NativeScripts::ModuleInitContext& ctx) { (void)ctx; }
)";

    CHECK_THROWS_AS(ScanNativeScriptModuleSource(WRONG_IMPORT, "Client/UiState.cppm", "UiState.cppm", "Client"), NativeScriptSynthException);
}

TEST_CASE("Native script module scan requires an exported initializer")
{
    // A non-exported initializer is invisible to the generated dispatcher, which reaches it through the
    // module interface rather than an extern declaration
    static constexpr string_view SOURCE = R"(export module NativeScripts.User.Server.CombatTweaks;

import NativeApi.Server;

void CombatTweaksInit(const ::NativeScripts::ModuleInitContext& ctx) { (void)ctx; }
)";

    CHECK_THROWS_AS(ScanNativeScriptModuleSource(SOURCE, "Server/CombatTweaks.cppm", "CombatTweaks.cppm", "Server"), NativeScriptSynthException);
}

TEST_CASE("Native script roles cover every dispatcher the engine forward-declares")
{
    const auto& roles = GetNativeScriptRoles();

    CHECK(roles == vector<string> {"Common", "Server", "Client", "Mapper", "Baker"});
}

TEST_CASE("Native script module scan of a missing tree yields an empty bucket per role")
{
    // Engine startup glue forward-declares RegisterNativeScriptModules_<Role> unconditionally, so a project
    // without a native tree still needs a dispatcher emitted for every role
    const auto modules = ScanNativeScriptModules(std::filesystem::path {"NoSuchNativeScriptsDir"});

    REQUIRE(modules.size() == GetNativeScriptRoles().size());

    for (const auto& role : GetNativeScriptRoles()) {
        const auto it = modules.find(role);
        REQUIRE(it != modules.end());
        CHECK(it->second.empty());
    }
}

TEST_CASE("Native bindings dispatcher stays emittable without user modules")
{
    const string body = SynthesizeNativeBindings("Server", {});

    CHECK(body.find("RegisterNativeScriptModules_Server") != string::npos);
}

#endif // FO_NATIVE_SCRIPTING
