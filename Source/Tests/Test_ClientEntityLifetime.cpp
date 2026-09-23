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

#include "catch_amalgamated.hpp"

#include "Application.h"
#include "Client.h"
#include "PlayerView.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

static auto MakeClientLifetimeSettings() -> GlobalSettings
{
    FO_STACK_TRACE_ENTRY();

    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    return settings;
}

static auto MakeClientLifetimeEngine(GlobalSettings& settings) -> refcount_ptr<ClientEngine>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> metadata = BakerTests::MakeEmptyMetadataBlob();
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientEntityLifetime");
    source->AddFile("Metadata.fometa-client", metadata);
#if FO_ANGELSCRIPT_SCRIPTING
    auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientEntityLifetimeCompiler");
    compiler_source->AddFile("Metadata.fometa-client", metadata);
    FileSystem compiler_resources;
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerClientEngine compiler {compiler_resources};
    source->AddFile("ClientEntityLifetime.fos-bin-client", BakerTests::CompileInlineScripts(&compiler, "ClientEntityLifetimeScripts", {{"Scripts/Lifetime.fos", "void LifetimeFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return safe_alloc::make_refcounted<ClientEngine>(&settings, std::move(resources), &GetApp()->MainWindow);
}

TEST_CASE("ClientEngineFinishesStartingUpOnceItRuns")
{
    // An engine is starting up from the moment it exists, so the client owes the finish; one that never
    // performed it reported every one-shot load of its own construction as a responsiveness failure
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    CHECK_FALSE(client->IsStartingUp());

    // There is no way back into the state, so a second finish is a start path running twice
    CHECK_THROWS_AS(client->FinishStartingUp(), VerificationException);
}

TEST_CASE("ClientEntityLookupRetainsUntilCallerFinishes")
{
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto player = safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001});

    {
        auto retained = client->GetEntity(ident_t {1001});
        REQUIRE(retained);
        REQUIRE(player->GetRefCount() == 2);

        std::thread release_thread {[owner = std::move(player)] { ignore_unused(owner); }};
        release_thread.join();

        CHECK(retained->GetId() == ident_t {1001});
        CHECK(retained->GetRefCount() == 1);
    }

    CHECK_FALSE(client->GetEntity(ident_t {1001}));
}

TEST_CASE("ClientEntityFinalReleasePreservesSuccessorRegistration")
{
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto old_player = safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001});
    auto new_player = safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001});
    std::thread release_thread {[owner = std::move(old_player)] { ignore_unused(owner); }};
    release_thread.join();

    CHECK(client->GetEntity(ident_t {1001}) == new_player);
    new_player->DestroySelf();
    CHECK_FALSE(client->GetEntity(ident_t {1001}));
}

FO_END_NAMESPACE
