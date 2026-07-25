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

#include "catch_amalgamated.hpp"

#include "TextureAtlas.h"

FO_BEGIN_NAMESPACE

TEST_CASE("TextureAtlasLayoutPacksOverlappingMaximalFreeRectangles")
{
    TextureAtlasLayout layout {{10, 10}};

    auto square = layout.Allocate({6, 6});
    auto tall = layout.Allocate({4, 10});

    REQUIRE(square);
    REQUIRE(tall);
    CHECK(square->GetPosition() == ipos32 {0, 0});
    CHECK(tall->GetPosition() == ipos32 {6, 0});
    CHECK(layout.GetUsedArea() == 76);
    CHECK_FALSE(layout.IsEmpty());
}

TEST_CASE("TextureAtlasLayoutRestoresReleasedCrossingSpace")
{
    TextureAtlasLayout layout {{10, 10}};
    auto square = layout.Allocate({6, 6});
    auto tall = layout.Allocate({4, 10});

    REQUIRE(square);
    REQUIRE(tall);
    ipos32 stable_square_position = square->GetPosition();
    tall.reset();

    auto bottom = layout.Allocate({10, 4});

    REQUIRE(bottom);
    CHECK(bottom->GetPosition() == ipos32 {0, 6});
    CHECK(square->GetPosition() == stable_square_position);
    CHECK(layout.GetUsedArea() == 76);
}

TEST_CASE("TextureAtlasLayoutReusesInteriorSpaceAfterChurn")
{
    TextureAtlasLayout layout {{6, 6}};
    vector<unique_del_nptr<TextureAtlasLayout::Allocation>> allocations;

    for (size_t i = 0; i < 9; i++) {
        auto allocation = layout.Allocate({2, 2});
        REQUIRE(allocation);
        allocations.emplace_back(std::move(allocation));
    }

    auto center_it = std::find_if(allocations.begin(), allocations.end(), [](const auto& allocation) noexcept { return allocation->GetPosition() == ipos32 {2, 2}; });
    REQUIRE(center_it != allocations.end());
    SpriteMeshData mesh;
    (*center_it)->SetSpriteMesh(&mesh);
    nptr<TextureAtlasLayout::Allocation> released_record = center_it->as_nptr();
    center_it->reset();

    auto replacement = layout.Allocate({2, 2});

    REQUIRE(replacement);
    CHECK(replacement.as_nptr() == released_record);
    CHECK(replacement->GetPosition() == ipos32 {2, 2});
    CHECK(replacement->GetSpriteMesh() == nullptr);
}

TEST_CASE("TextureAtlasLayoutReleaseOrderIsDeterministic")
{
    auto run = [](bool reverse_release) -> ipos32 {
        TextureAtlasLayout layout {{10, 10}};
        auto first = layout.Allocate({4, 4});
        auto second = layout.Allocate({6, 4});
        auto live = layout.Allocate({10, 6});

        FO_STRONG_ASSERT(first && second && live, "Texture atlas determinism fixture must fit");

        if (reverse_release) {
            second.reset();
            first.reset();
        }
        else {
            first.reset();
            second.reset();
        }

        auto replacement = layout.Allocate({10, 4});
        FO_STRONG_ASSERT(replacement, "Texture atlas determinism replacement must fit");
        return replacement->GetPosition();
    };

    CHECK(run(false) == ipos32 {0, 0});
    CHECK(run(true) == ipos32 {0, 0});
}

TEST_CASE("TextureAtlasLayoutFullyRestoresReleasedAtlas")
{
    TextureAtlasLayout layout {{10, 10}};
    auto first = layout.Allocate({6, 6});
    auto second = layout.Allocate({4, 10});

    REQUIRE(first);
    REQUIRE(second);
    first.reset();
    second.reset();
    CHECK(layout.IsEmpty());
    CHECK(layout.GetUsedArea() == 0);

    auto full = layout.Allocate({10, 10});

    REQUIRE(full);
    CHECK(full->GetPosition() == ipos32 {0, 0});
    CHECK(full->GetSize() == isize32 {10, 10});
}

TEST_CASE("TextureAtlasLayoutNeverOverlapsLiveAllocations")
{
    TextureAtlasLayout layout {{16, 16}};
    const isize32 sizes[] = {{5, 7}, {4, 9}, {7, 3}, {3, 6}, {2, 8}, {5, 4}, {4, 4}, {2, 5}};
    vector<unique_del_nptr<TextureAtlasLayout::Allocation>> allocations;
    nptr<TextureAtlasLayout::Allocation> first_observer {};
    ipos32 first_position {};

    auto validate = [&layout, &allocations] {
        vector<uint8_t> occupancy(16 * 16);
        size_t live_area = 0;

        for (const auto& allocation : allocations) {
            if (!allocation) {
                continue;
            }

            ipos32 pos = allocation->GetPosition();
            isize32 size = allocation->GetSize();
            REQUIRE(pos.x >= 0);
            REQUIRE(pos.y >= 0);
            REQUIRE(pos.x + size.width <= layout.GetSize().width);
            REQUIRE(pos.y + size.height <= layout.GetSize().height);
            live_area += numeric_cast<size_t>(size.width) * numeric_cast<size_t>(size.height);

            for (int32_t y = pos.y; y < pos.y + size.height; y++) {
                for (int32_t x = pos.x; x < pos.x + size.width; x++) {
                    size_t index = numeric_cast<size_t>(y) * layout.GetSize().width + x;
                    CHECK(occupancy[index] == 0);
                    occupancy[index] = 1;
                }
            }
        }

        CHECK(layout.GetUsedArea() == live_area);
    };

    for (isize32 size : sizes) {
        auto allocation = layout.Allocate(size);
        if (allocation) {
            allocations.emplace_back(std::move(allocation));
            if (!first_observer) {
                first_observer = allocations.back().as_nptr();
                first_position = first_observer->GetPosition();
            }
            validate();
            CHECK(first_observer == allocations.front().as_nptr());
            CHECK(first_observer->GetPosition() == first_position);
        }
    }

    REQUIRE(allocations.size() >= 5);
    allocations[1].reset();
    validate();
    allocations[3].reset();
    validate();

    if (auto replacement = layout.Allocate({4, 6})) {
        allocations.emplace_back(std::move(replacement));
    }

    validate();
    CHECK(first_observer == allocations.front().as_nptr());
    CHECK(first_observer->GetPosition() == first_position);
}

TEST_CASE("TextureAtlasLayoutClearsReleasedMeshObserver")
{
    TextureAtlasLayout layout {{6, 6}};
    auto allocation = layout.Allocate({6, 6});
    REQUIRE(allocation);
    nptr<TextureAtlasLayout::Allocation> observer = allocation.as_nptr();
    SpriteMeshData mesh;

    allocation->SetSpriteMesh(&mesh);
    REQUIRE(allocation->GetSpriteMesh());
    allocation.reset();

    CHECK_FALSE(observer->IsActive());
    CHECK(observer->GetSpriteMesh() == nullptr);
}

TEST_CASE("TextureAtlasLayoutDumpOverlayDrawsMeshGeometry")
{
    isize32 atlas_size = {6, 6};
    ucolor background {0, 0, 0, 255};
    vector<ucolor> pixels(numeric_cast<size_t>(atlas_size.width) * atlas_size.height, background);
    TextureAtlasLayout layout {atlas_size};
    auto allocation = layout.Allocate(atlas_size);
    REQUIRE(allocation);
    SpriteMeshData mesh {
        .Vertices = {{0, 0}, {4, 0}, {0, 4}},
        .Indices = {0, 1, 2},
    };
    allocation->SetSpriteMesh(&mesh);

    layout.DrawDumpOverlay(pixels);

    auto pixel = [&pixels, atlas_size](int32_t x, int32_t y) -> ucolor { return pixels[numeric_cast<size_t>(y) * atlas_size.width + x]; };
    CHECK(pixel(1, 1) == ucolor {0, 255, 255, 255});
    CHECK(pixel(5, 1) == ucolor {0, 255, 255, 255});
    CHECK(pixel(1, 5) == ucolor {0, 255, 255, 255});
    CHECK(pixel(3, 1) == ucolor {255, 0, 255, 255});
    CHECK(pixel(3, 3) == ucolor {255, 0, 255, 255});
    CHECK(pixel(2, 2) == background);
}

TEST_CASE("TextureAtlasLayoutDumpOverlayDistinguishesQuadAndEmptyGeometry")
{
    isize32 atlas_size = {6, 6};
    ucolor background {0, 0, 0, 255};

    TextureAtlasLayout quad_layout {atlas_size};
    auto quad = quad_layout.Allocate(atlas_size);
    REQUIRE(quad);
    vector<ucolor> quad_pixels(numeric_cast<size_t>(atlas_size.width) * atlas_size.height, background);
    quad_layout.DrawDumpOverlay(quad_pixels);
    CHECK(quad_pixels[1 * atlas_size.width + 3] == ucolor {255, 255, 0, 255});
    CHECK(quad_pixels[3 * atlas_size.width + 3] == background);

    TextureAtlasLayout empty_layout {atlas_size};
    auto empty = empty_layout.Allocate(atlas_size);
    REQUIRE(empty);
    SpriteMeshData empty_mesh;
    empty->SetSpriteMesh(&empty_mesh);
    vector<ucolor> empty_pixels(numeric_cast<size_t>(atlas_size.width) * atlas_size.height, background);
    empty_layout.DrawDumpOverlay(empty_pixels);
    CHECK(empty_pixels[3 * atlas_size.width + 3] == ucolor {255, 0, 0, 255});
    CHECK(empty_pixels[1 * atlas_size.width + 3] == background);
}

// Representative runtime sprite corpus, shared by the always-on packing efficiency gate and the hidden benchmark
static auto MakeAtlasCorpus() -> vector<isize32>
{
    FO_STACK_TRACE_ENTRY();

    vector<isize32> corpus;
    corpus.reserve(768);
    uint32_t random_state = 0x7A11A5u;

    for (size_t i = 0; i < 768; i++) {
        random_state = random_state * 1664525u + 1013904223u;
        int32_t width = 24 + numeric_cast<int32_t>(random_state % 489u);
        random_state = random_state * 1664525u + 1013904223u;
        int32_t height = 24 + numeric_cast<int32_t>(random_state % 745u);
        corpus.emplace_back(width + 2, height + 2);
    }

    return corpus;
}

// Packs the corpus the way the runtime atlas filler does and returns the resulting atlas page count
static auto RunAtlasCorpus(const vector<isize32>& corpus, bool churn) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    constexpr isize32 atlas_size = {2048, 8192};
    vector<unique_ptr<TextureAtlasLayout>> layouts;
    vector<unique_del_nptr<TextureAtlasLayout::Allocation>> allocations;

    auto allocate = [&layouts, &allocations, atlas_size](isize32 size) {
        nptr<TextureAtlasLayout> best_layout {};
        optional<TextureAtlasLayout::FitScore> best_fit;

        for (auto& layout : layouts) {
            optional<TextureAtlasLayout::FitScore> fit = layout->FindBestFitScore(size);
            if (!fit) {
                continue;
            }

            auto fit_key = std::tie(fit->ShortSideFit, fit->LongSideFit, fit->AreaWaste);
            if (!best_fit) {
                best_layout = layout;
                best_fit = fit;
            }
            else {
                auto best_key = std::tie(best_fit->ShortSideFit, best_fit->LongSideFit, best_fit->AreaWaste);
                if (fit_key < best_key) {
                    best_layout = layout;
                    best_fit = fit;
                }
            }
        }

        if (!best_layout) {
            layouts.emplace_back(SafeAlloc::MakeUnique<TextureAtlasLayout>(atlas_size));
            best_layout = layouts.back();
        }

        auto allocation = best_layout->Allocate(size);
        FO_STRONG_ASSERT(allocation, "Texture atlas corpus rectangle must fit a new page", size);
        allocations.emplace_back(std::move(allocation));
    };

    for (isize32 size : corpus) {
        allocate(size);
    }

    if (churn) {
        for (size_t i = 0; i < allocations.size(); i += 3) {
            allocations[i].reset();
        }
        for (size_t i = 0; i < corpus.size() / 3; i++) {
            allocate({corpus[i].height, corpus[i].width});
        }
    }

    return layouts.size();
}

TEST_CASE("TextureAtlasLayoutPackingEfficiency", "[texture-atlas]")
{
    vector<isize32> corpus = MakeAtlasCorpus();

    size_t packed_pages = RunAtlasCorpus(corpus, false);
    CAPTURE(packed_pages);
    CHECK(packed_pages <= 6);

    size_t churned_pages = RunAtlasCorpus(corpus, true);
    CAPTURE(churned_pages);
    CHECK(churned_pages <= 6);
}

TEST_CASE("TextureAtlasLayoutPerformance", "[!benchmark][texture-atlas]")
{
    vector<isize32> corpus = MakeAtlasCorpus();

    BENCHMARK("Pack representative runtime sprite corpus")
    {
        return RunAtlasCorpus(corpus, false);
    };

    BENCHMARK("Pack and refill after runtime churn")
    {
        return RunAtlasCorpus(corpus, true);
    };
}

FO_END_NAMESPACE
