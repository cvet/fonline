// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "Test_MapperHelpers.h"

FO_BEGIN_NAMESPACE

using namespace MapperTests;

TEST_CASE("AngelScriptMapperScriptApiCoverage")
{
    auto settings {MakeMapperTestSettings()};
    auto mapper {safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow)};

    auto shutdown {scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); })};

    mapper->InitIface();

    string body {MakeItemBlock(10, TILE_A, 5, 5) + MakeItemBlock(11, TILE_B, 7, 7)};
    auto map {mapper->LoadMapFromText("ScriptApiMap", "ScriptApiMap.fomap", MakeMapText(body))};
    REQUIRE(map);
    mapper->ShowMap(map.as_ptr());

    auto run_script {[&mapper](string_view name) {
        int32_t result {-1};
        INFO(name);
        REQUIRE(mapper->CallFunc(mapper->Hashes.to_hashed_string(name), result));
        CHECK(result == 0);
    }};

    run_script("MapperMergeTest::UnitTestMapperViewApi");
    run_script("MapperMergeTest::UnitTestMapperEntityApi");
    run_script("MapperMergeTest::UnitTestMapperTabApi");
#if FO_SPARK_PARTICLES
    // The particle sprite factory only has a baked particle to serve when the SPARK fixture is compiled in
    run_script("MapperMergeTest::UnitTestParticleSpriteApi");
#endif
    run_script("MapperMergeTest::UnitTestClientMapApi");
    run_script("MapperMergeTest::UnitTestClientItemApi");
    run_script("MapperMergeTest::UnitTestClientEntityTimeEvents");
    run_script("MapperMergeTest::UnitTestClientCritterApi");
    run_script("MapperMergeTest::UnitTestMapperEditingApi");

    // Last: it creates and unloads maps, so it changes which map is current
    run_script("MapperMergeTest::UnitTestMapperMapLifecycleApi");
}

FO_END_NAMESPACE

#endif
