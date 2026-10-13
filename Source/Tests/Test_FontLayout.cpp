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

#include "Common.h"

#include "Test_ClientEngineHelpers.h"

FO_BEGIN_NAMESPACE

using namespace ClientEngineTests;

TEST_CASE("FontLayoutBatchPreparesColdDrawAndReusesWarmLayouts")
{
    auto settings = MakeClientTestSettings();
    BakerTests::OverrideSetting(settings.Client.Multithreading, true);
    BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, int32_t {2});
    auto client = MakeClientEngine(settings, MakeClientTestResources(MakeUnitTestFontResources()));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    client->FontMngr.BindFoFont(FontType::Default, "UnitTestFont.fofnt", AtlasType::IfaceSprites, false, false);
    if (!client->WorkSched.IsParallel()) {
        SKIP("Text-layout worker diagnostics require a thread-capable host with spare cores");
    }

    vector<TextLayoutRequest> requests;

    for (int32_t i = 0; i < 64; i++) {
        requests.emplace_back(TextLayoutRequest {
            .Text = strex("AB {}", i).str(),
            .Size = {200, 500},
            .Format = {FontType::Default, combine_enum(FontFlag::CenterX, FontFlag::AlignBottom, FontFlag::Bordered), 0},
            .Color = {100, 150, 200, 120},
        });
    }

    auto before = client->WorkSched.GetDiagnostics();
    vector<TextLayoutResult> results = client->FontMngr.PrepareTextLayouts(requests);
    REQUIRE(results.size() == requests.size());
    CHECK(isize32 {results[0].Width, results[0].Height} == isize32 {20, 8});
    CHECK(isize32 {results[10].Width, results[10].Height} == isize32 {25, 8});
    CHECK(results[0].Lines == 1);
    auto cold = client->WorkSched.GetDiagnostics();
    CHECK(cold.ParallelBatches - before.ParallelBatches == 2);
    CHECK(cold.ParallelItems - before.ParallelItems == 128);

    (void)client->FontMngr.PrepareTextLayouts(requests);
    CHECK(client->WorkSched.GetDiagnostics().ParallelBatches == cold.ParallelBatches);

    for (int32_t frame = 0; frame < 8; frame++) {
        client->FontMngr.FrameUpdate();
        (void)client->FontMngr.PrepareTextLayouts(requests);
    }

    CHECK(client->WorkSched.GetDiagnostics().ParallelBatches == cold.ParallelBatches);
    client->FontMngr.BindFoFont(FontType::Default, "UnitTestFont.fofnt", AtlasType::IfaceSprites, false, false);
    vector<TextLayoutRequest> repeated(64, requests.front());
    (void)client->FontMngr.PrepareTextLayouts(repeated);
    CHECK(client->WorkSched.GetDiagnostics().ParallelBatches == cold.ParallelBatches);
    vector<TextLayoutRequest> small(requests.begin(), requests.begin() + 31);
    (void)client->FontMngr.PrepareTextLayouts(small);
    CHECK(client->WorkSched.GetDiagnostics().ParallelBatches == cold.ParallelBatches);

    client->FontMngr.BindFoFont(FontType::Default, "UnitTestFont.fofnt", AtlasType::IfaceSprites, false, false);
    vector<TextLayoutRequest> invalid = requests;
    invalid.back().Format.SkipLines = 200;
    CHECK_THROWS(client->FontMngr.PrepareTextLayouts(invalid));
    uint64_t failed_batches = client->WorkSched.GetDiagnostics().ParallelBatches;
    (void)client->FontMngr.PrepareTextLayouts(requests);
    CHECK(client->WorkSched.GetDiagnostics().ParallelBatches == failed_batches + 2);
}

TEST_CASE("FontLayoutBatchMatchesMetricAndOwnerDrawOracles")
{
    for (bool parallel : {false, true}) {
        auto settings = MakeClientTestSettings();
        BakerTests::OverrideSetting(settings.Client.Multithreading, parallel);
        BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, int32_t {2});
        BakerTests::OverrideSetting(settings.Render.Brightness, int32_t {10});
        auto resources = MakeUnitTestFontResources();
        string descriptor(resources[0].second.begin(), resources[0].second.end());
        descriptor.resize(descriptor.size() - 4);

        for (string_view letter : {"\xD0\x96", "\xD1\x8F"}) {
            descriptor += strex("Letter '{}'\nPositionX 0\nPositionY 0\nWidth 4\nHeight 6\nOffsetX 0\nOffsetY 0\nXAdvance 5\n", letter);
        }

        descriptor += "End\n";
        resources[0].second.assign(descriptor.begin(), descriptor.end());
        auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(resources)));
        auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
        client->FontMngr.BindFoFont(FontType::Default, "UnitTestFont.fofnt", AtlasType::IfaceSprites, false, false);

        struct Oracle
        {
            string Text {};
            isize32 Bounds {};
            FontFlag Flags {};
            int32_t Skip {};
            isize32 Extent {};
            int32_t Lines {};
        };

        vector<Oracle> oracles {
            {"\xD0\x96\xD1\x8F", {200, 500}, FontFlag::None, 0, {10, 8}, 1},
            {"A@color:0x0000FF@B@color@C", {200, 500}, FontFlag::CenterX, 0, {15, 8}, 1},
            {"ABCD", {10, 500}, FontFlag::None, 0, {10, 17}, 2},
            {"A\nB\nC", {200, 500}, FontFlag::AlignRight, 1, {5, 17}, 2},
            {"A\nB\nC", {200, 500}, FontFlag::AlignBottom, 1, {5, 17}, 2},
            {"ABC\nA", {200, 500}, combine_enum(FontFlag::CenterX, FontFlag::AlignBottom), 0, {15, 17}, 2},
            {"", {200, 500}, FontFlag::None, 0, {}, 0},
        };
        vector<TextLayoutRequest> requests;

        for (const auto& oracle : oracles) {
            requests.emplace_back(TextLayoutRequest {oracle.Text, oracle.Bounds, {FontType::Default, oracle.Flags, oracle.Skip}, {30, 40, 50, 120}});
        }

        vector<TextLayoutRequest> worker_requests;

        for (int32_t i = 0; i < 64; i++) {
            TextLayoutRequest request = requests[numeric_cast<size_t>(i) % 6];

            for (int32_t tag = 0; tag <= i; tag++) {
                request.Text += "@color:0x0000FF@";
            }

            worker_requests.emplace_back(std::move(request));
        }

        auto before = client->WorkSched.GetDiagnostics();
        vector<TextLayoutResult> worker_results = client->FontMngr.PrepareTextLayouts(worker_requests);
        CHECK(client->WorkSched.GetDiagnostics().ParallelBatches - before.ParallelBatches == (client->WorkSched.IsParallel() ? 2 : 0));

        for (size_t i = 0; i < worker_results.size(); i++) {
            CAPTURE(parallel, i);
            CHECK(isize32 {worker_results[i].Width, worker_results[i].Height} == oracles[i % 6].Extent);
            CHECK(worker_results[i].Lines == oracles[i % 6].Lines);
        }

        vector<TextLayoutResult> results = client->FontMngr.PrepareTextLayouts(requests);
        REQUIRE(results.size() == oracles.size());

        for (size_t i = 0; i < oracles.size(); i++) {
            CAPTURE(parallel, i);
            CHECK(isize32 {results[i].Width, results[i].Height} == oracles[i].Extent);
            CHECK(results[i].Lines == oracles[i].Lines);
            isize32 direct_size;
            int32_t direct_lines = 0;
            (void)client->FontMngr.GetTextInfo(requests[i].Size, requests[i].Text, requests[i].Format, direct_size, direct_lines);
            CHECK(isize32 {results[i].Width, results[i].Height} == direct_size);
            CHECK(results[i].Lines == direct_lines);
        }

        RecordingQuadEffect effect;
        client->FontMngr.SetFontEffect(FontType::Default, &effect);
        size_t cached_before_draw = client->FontMngr.GetCachedLayoutCount();
        client->FontMngr.DrawText({10, 20, isize32 {worker_results[1].Width, worker_results[1].Height}}, worker_requests[1].Text, worker_requests[1].Color, worker_requests[1].Format);
        client->FontMngr.DrawText({10, 20, isize32 {worker_results[5].Width, worker_results[5].Height}}, worker_requests[5].Text, worker_requests[5].Color, worker_requests[5].Format);
        client->SprMngr.Flush();
        CHECK(client->FontMngr.GetCachedLayoutCount() == cached_before_draw);
        REQUIRE(effect.Draws.size() == 1);
        const auto& vertices = effect.Draws[0].Vertices;
        REQUIRE(vertices.size() == 28);
        CHECK(vertices[0].PosX == Catch::Approx(9));
        CHECK(vertices[0].PosY == Catch::Approx(27));
        CHECK(vertices[1].PosY == Catch::Approx(19));
        CHECK(vertices[4].PosX == Catch::Approx(14));
        CHECK(vertices[0].Color == ucolor {40, 50, 60, 120});
        CHECK(vertices[4].Color == ucolor {255, 10, 10, 120});
        CHECK(vertices[8].Color == ucolor {50, 60, 70, 120});
        CHECK(vertices[24].PosX == Catch::Approx(14));
        CHECK(vertices[24].PosY == Catch::Approx(36));
        CHECK(vertices[25].PosY == Catch::Approx(28));

        client->FontMngr.BindFoFont(FontType::Default, "UnitTestFont.fofnt", AtlasType::IfaceSprites, false, false, 0.5f);
        results = client->FontMngr.PrepareTextLayouts(requests);
        CHECK(isize32 {results[0].Width, results[0].Height} == isize32 {6, 4});
        CHECK(isize32 {results[1].Width, results[1].Height} == isize32 {9, 4});
    }
}

TEST_CASE("FontLayoutBatchWithoutWorkersKeepsDirectSemantics")
{
    auto settings = MakeClientTestSettings();
    BakerTests::OverrideSetting(settings.Client.Multithreading, true);
    BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, int32_t {0});
    auto client = MakeClientEngine(settings, MakeClientTestResources(MakeUnitTestFontResources()));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    client->FontMngr.BindFoFont(FontType::Default, "UnitTestFont.fofnt", AtlasType::IfaceSprites, false, false);
    REQUIRE_FALSE(client->WorkSched.IsParallel());
    vector<TextLayoutRequest> requests {
        {"ABC", {200, 500}, {}, {30, 40, 50, 120}},
        {"", {200, 500}, {}, {}},
    };
    vector<TextLayoutResult> results = client->FontMngr.PrepareTextLayouts(requests);
    REQUIRE(results.size() == 2);
    CHECK(results[0].Width == 15);
    CHECK(results[0].Height == 8);
    CHECK(results[0].Lines == 1);
    CHECK(results[1].Width == 0);
    CHECK(results[1].Height == 0);
    CHECK(results[1].Lines == 0);
    CHECK(client->WorkSched.GetDiagnostics().ParallelBatches == 0);
    client->FontMngr.ClearFonts();
    CHECK_THROWS(client->FontMngr.PrepareTextLayouts(requests));
}

FO_END_NAMESPACE

