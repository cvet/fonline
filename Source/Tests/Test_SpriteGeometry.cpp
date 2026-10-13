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

TEST_CASE("SpriteGeometryBatchUsesClientWorkersAndPreservesQuads")
{
    auto settings = MakeClientTestSettings();
    BakerTests::OverrideSetting(settings.Client.Multithreading, true);
    BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, 2);
    BakerTests::OverrideSetting(settings.Client.MultithreadingHeadroomMinCores, 0);
    auto client = MakeClientEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    if (!client->WorkSched.IsParallel()) {
        SKIP("This platform does not start client workers");
    }

    auto [atlas, allocation, atlas_pos] = client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {5, 7});
    ignore_unused(atlas_pos);
    frect32 texture_rect {0.125f, 0.25f, 0.5f, 0.375f};
    auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {5, 7}, ipos32 {}, atlas, std::move(allocation), texture_rect, vector<bool> {});
    RecordingQuadEffect effect;
    sprite->SetDrawEffect(&effect);
    auto clear_effect = scope_exit([&sprite]() noexcept { sprite->SetDrawEffect(nullptr); });

    uint64_t batches_before = client->WorkSched.GetDiagnostics().ParallelBatches;
    constexpr size_t sprite_count = 512;
    ucolor color {26, 52, 78, 130};

    for (size_t i = 0; i < sprite_count; i++) {
        client->SprMngr.DrawSprite(sprite, {numeric_cast<int32_t>(i), 11}, color);
    }

    client->SprMngr.Flush();

    CHECK(client->WorkSched.GetDiagnostics().ParallelBatches > batches_before);
    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& draw = effect.Draws.front();
    REQUIRE(draw.Vertices.size() == sprite_count * 4);
    REQUIRE(draw.Indices.size() == sprite_count * 6);

    for (size_t i = 0; i < sprite_count; i++) {
        size_t vertex = i * 4;
        size_t index = i * 6;
        float32_t x = numeric_cast<float32_t>(i);

        CHECK(draw.Vertices[vertex + 0].PosX == x);
        CHECK(draw.Vertices[vertex + 0].PosY == 18.0f);
        CHECK(draw.Vertices[vertex + 1].PosX == x);
        CHECK(draw.Vertices[vertex + 1].PosY == 11.0f);
        CHECK(draw.Vertices[vertex + 2].PosX == x + 5.0f);
        CHECK(draw.Vertices[vertex + 2].PosY == 11.0f);
        CHECK(draw.Vertices[vertex + 3].PosX == x + 5.0f);
        CHECK(draw.Vertices[vertex + 3].PosY == 18.0f);

        CHECK(draw.Vertices[vertex + 0].TexU == 0.125f);
        CHECK(draw.Vertices[vertex + 0].TexV == 0.625f);
        CHECK(draw.Vertices[vertex + 1].TexU == 0.125f);
        CHECK(draw.Vertices[vertex + 1].TexV == 0.25f);
        CHECK(draw.Vertices[vertex + 2].TexU == 0.625f);
        CHECK(draw.Vertices[vertex + 2].TexV == 0.25f);
        CHECK(draw.Vertices[vertex + 3].TexU == 0.625f);
        CHECK(draw.Vertices[vertex + 3].TexV == 0.625f);

        for (size_t corner = 0; corner < 4; corner++) {
            CHECK(draw.Vertices[vertex + corner].PosZ == 0.0f);
            CHECK(draw.Vertices[vertex + corner].Color == color);
            CHECK(draw.Vertices[vertex + corner].EggFlags[0] == 0.0f);
            CHECK(draw.Vertices[vertex + corner].EggFlags[1] == 0.0f);
        }

        CHECK(draw.Indices[index + 0] == vertex + 0);
        CHECK(draw.Indices[index + 1] == vertex + 1);
        CHECK(draw.Indices[index + 2] == vertex + 3);
        CHECK(draw.Indices[index + 3] == vertex + 1);
        CHECK(draw.Indices[index + 4] == vertex + 2);
        CHECK(draw.Indices[index + 5] == vertex + 3);
    }
}

static void CheckSpriteVertex(const Vertex2D& actual, const Vertex2D& expected)
{
    CHECK(actual.PosX == Catch::Approx(expected.PosX));
    CHECK(actual.PosY == Catch::Approx(expected.PosY));
    CHECK(actual.PosZ == Catch::Approx(expected.PosZ));
    CHECK(actual.Color == expected.Color);
    CHECK(actual.TexU == Catch::Approx(expected.TexU));
    CHECK(actual.TexV == Catch::Approx(expected.TexV));
    CHECK(actual.EggFlags[0] == expected.EggFlags[0]);
    CHECK(actual.EggFlags[1] == expected.EggFlags[1]);
}

TEST_CASE("SpriteGeometryKernelPreservesQuadMeshAndIndependentRanges")
{
    ucolor left {10, 20, 30, 40};
    ucolor right {110, 120, 130, 140};
    SpriteMeshData mesh {{10, 20}, {2, 4}, {{0, 0}, {3, 6}, {6, 0}}, {2, 0, 1}};
    PreparedSpriteGeometry quad {.DrawRect = {100, 200, 20, 40}, .TextureRect = {0.25f, 0.5f, 0.5f, 0.25f}, .ColorLeft = left, .ColorRight = right};
    PreparedSpriteGeometry triangle = quad;
    triangle.Mesh = &mesh;
    triangle.FrameSize = {6, 6};
    vector<PreparedSpriteGeometry> commands {quad, triangle};
    Vertex2D sentinel {999, 998, 997, ucolor {1, 2, 3, 4}, 996, 995, {994, 993}};
    vector<Vertex2D> direct(11, sentinel);
    vector<Vertex2D> parallel(11, sentinel);
    vector<vindex_t> direct_indices(13, 123);
    vector<vindex_t> parallel_indices(13, 123);
    const size_t offsets[] {1, 6};
    const size_t index_offsets[] {1, 9};
    auto write = [&](size_t index, span<Vertex2D> vertices, span<vindex_t> indices) { WritePreparedSpriteGeometry(commands[index], vertices.subspan(offsets[index], GetPreparedSpriteVertexCount(commands[index])), indices.subspan(index_offsets[index], GetPreparedSpriteIndexCount(commands[index])), offsets[index]); };

    for (size_t i = 0; i < commands.size(); i++) {
        write(i, direct, direct_indices);
    }

    WorkScheduler scheduler {"GeometryTest", 2};
    scheduler.RunBatch("GeometryRanges", commands.size(), 1, [&](size_t i) { write(i, parallel, parallel_indices); });

    const Vertex2D expected[] {
        {100, 240, 0, left, 0.25f, 0.75f, {}},
        {100, 200, 0, left, 0.25f, 0.5f, {}},
        {120, 200, 0, right, 0.75f, 0.5f, {}},
        {120, 240, 0, right, 0.75f, 0.75f, {}},
        {104, 208, 0, ucolor {30, 40, 50, 60}, 0.25f, 0.5f, {}},
        {110, 220, 0, ucolor {60, 70, 80, 90}, 0.5f, 0.75f, {}},
        {116, 208, 0, ucolor {90, 100, 110, 120}, 0.75f, 0.5f, {}},
    };

    for (size_t i = 0; i < 4; i++) {
        CheckSpriteVertex(direct[1 + i], expected[i]);
    }

    for (size_t i = 0; i < 3; i++) {
        CheckSpriteVertex(direct[6 + i], expected[4 + i]);
    }

    for (size_t i = 0; i < direct.size(); i++) {
        CheckSpriteVertex(parallel[i], direct[i]);
    }

    for (size_t index : {size_t {0}, size_t {5}, size_t {9}, size_t {10}}) {
        CheckSpriteVertex(direct[index], sentinel);
    }

    CHECK(direct_indices == vector<vindex_t> {123, 1, 2, 4, 2, 3, 4, 123, 123, 8, 6, 7, 123});
    CHECK(parallel_indices == direct_indices);
}

TEST_CASE("SpriteGeometryKernelValidatesBeforeWritingAndHandlesEmptyMeshes")
{
    PreparedSpriteGeometry geometry {.DrawRect = {0, 0, 4, 8}, .ColorLeft = ucolor {5, 6, 7, 8}, .ColorRight = ucolor {5, 6, 7, 8}};
    vector<Vertex2D> vertices(4, Vertex2D {.PosX = 99});
    vector<vindex_t> indices(6, 42);
    size_t max_index = std::numeric_limits<vindex_t>::max();

    WritePreparedSpriteGeometry(geometry, vertices, indices, max_index - 3);
    CHECK(indices == vector<vindex_t> {numeric_cast<vindex_t>(max_index - 3), numeric_cast<vindex_t>(max_index - 2), numeric_cast<vindex_t>(max_index), numeric_cast<vindex_t>(max_index - 2), numeric_cast<vindex_t>(max_index - 1), numeric_cast<vindex_t>(max_index)});
    vertices.assign(4, Vertex2D {.PosX = 99});
    indices.assign(6, 42);

    CHECK_THROWS(WritePreparedSpriteGeometry(geometry, vertices, indices, max_index - 2));
    CHECK_THROWS(WritePreparedSpriteGeometry(geometry, span<Vertex2D> {vertices}.first(3), indices, 0));
    CHECK_THROWS(WritePreparedSpriteGeometry(geometry, vertices, span<vindex_t> {indices}.first(5), 0));

    SpriteMeshData invalid {{4, 8}, {}, {{0, 0}, {4, 8}, {4, 0}}, {0, 3, 1}};
    geometry.Mesh = &invalid;
    geometry.FrameSize = {4, 8};
    CHECK_THROWS(WritePreparedSpriteGeometry(geometry, span<Vertex2D> {vertices}.first(3), span<vindex_t> {indices}.first(3), 0));
    invalid.Indices = {0, 2, 1};
    geometry.FrameSize = {};
    CHECK_THROWS(WritePreparedSpriteGeometry(geometry, span<Vertex2D> {vertices}.first(3), span<vindex_t> {indices}.first(3), 0));

    geometry.FrameSize = {4, 8};
    invalid.SourceOffset.x = std::numeric_limits<int32_t>::max();
    CHECK_THROWS(WritePreparedSpriteGeometry(geometry, span<Vertex2D> {vertices}.first(3), span<vindex_t> {indices}.first(3), 0));

    for (const auto& vertex : vertices) {
        CHECK(vertex.PosX == 99);
    }

    CHECK(indices == vector<vindex_t>(6, 42));
    SpriteMeshData empty;
    geometry.Mesh = &empty;
    CHECK(GetPreparedSpriteVertexCount(geometry) == 0);
    CHECK(GetPreparedSpriteIndexCount(geometry) == 0);
    CHECK_NOTHROW(WritePreparedSpriteGeometry(geometry, {}, {}, max_index));
}

TEST_CASE("SpriteGeometryKernelTransformsRotationFlatteningDepthAndBothEggs")
{
    PreparedSpriteGeometry geometry {
        .DrawRect = {10, 20, 4, 8},
        .TextureRect = {0.25f, 0.5f, 0.5f, 0.25f},
        .ColorLeft = ucolor {10, 20, 30, 40},
        .ColorRight = ucolor {110, 120, 130, 140},
        .Transform = {.MapSprite = true, .ProjectionY = 100, .ProjectionZ = 50, .Rotate = true, .Center = {12, 24}, .CosAngle = 0, .SinAngle = 1, .YScale = 0.5f, .Standing = true, .SceneRootY = 26, .DepthSlope = 2, .EggFlags = {1, 1}},
    };
    vector<Vertex2D> vertices(4);
    vector<vindex_t> indices(6);
    WritePreparedSpriteGeometry(geometry, vertices, indices, 0);
    const Vertex2D expected[] {
        {8, 23, 56, ucolor {10, 20, 30, 40}, 0.25f, 0.75f, {1, 1}},
        {16, 23, 56, ucolor {10, 20, 30, 40}, 0.25f, 0.5f, {1, 1}},
        {16, 25, 52, ucolor {110, 120, 130, 140}, 0.75f, 0.5f, {1, 1}},
        {8, 25, 52, ucolor {110, 120, 130, 140}, 0.75f, 0.75f, {1, 1}},
    };

    for (size_t i = 0; i < 4; i++) {
        CheckSpriteVertex(vertices[i], expected[i]);
    }

    geometry.Transform.Standing = false;
    geometry.Transform.Rotate = false;
    geometry.Transform.EggFlags[1] = 0;
    WritePreparedSpriteGeometry(geometry, vertices, indices, 0);
    CHECK(vertices[0].PosX == 10);
    CHECK(vertices[0].PosY == 28);

    for (const auto& vertex : vertices) {
        CHECK(vertex.PosZ == 50);
        CHECK(vertex.EggFlags[0] == 1);
        CHECK(vertex.EggFlags[1] == 0);
    }
}

static auto MakeGeometrySettings(bool parallel) -> GlobalSettings
{
    auto settings = MakeClientTestSettings();
    BakerTests::OverrideSetting(settings.Client.Multithreading, parallel);
    BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, 2);
    BakerTests::OverrideSetting(settings.Client.MultithreadingHeadroomMinCores, 0);
    return settings;
}

TEST_CASE("SpriteGeometryGlyphsUseWorkersAndPreserveColorPositionsAndOrder")
{
    bool parallel = GENERATE(false, true);
    auto settings = MakeGeometrySettings(parallel);
    auto client = MakeClientEngine(settings, MakeClientTestResources(MakeUnitTestFontResources()));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    client->FontMngr.BindFoFont(FontType::Default, "UnitTestFont.fofnt", AtlasType::OneImage, false, false, 1.0f);
    RecordingQuadEffect effect;
    client->FontMngr.SetFontEffect(FontType::Default, &effect);
    auto clear_effect = scope_exit([&client]() noexcept { client->FontMngr.SetFontEffect(FontType::Default, nullptr); });
    uint64_t before = client->WorkSched.GetDiagnostics().ParallelBatches;
    ucolor color {26, 52, 78, 130};
    string text(512, 'A');
    client->FontMngr.DrawText({10, 20, 4000, 30}, text, color, {});
    client->SprMngr.Flush();
    REQUIRE(effect.Draws.size() == 1);
    REQUIRE(effect.Draws[0].Vertices.size() == 2048);
    REQUIRE(effect.Draws[0].Indices.size() == 3072);

    for (size_t i = 0; i < 512; i++) {
        float32_t x = 9 + numeric_cast<float32_t>(i * 5);
        const auto& v = effect.Draws[0].Vertices;
        CHECK(v[i * 4].PosX == x);
        CHECK(v[i * 4].PosY == 27);
        CHECK(v[i * 4 + 1].PosY == 19);
        CHECK(v[i * 4 + 2].PosX == x + 6);
        CHECK(v[i * 4 + 3].PosY == 27);

        for (size_t corner = 0; corner < 4; corner++) {
            CHECK(v[i * 4 + corner].Color == color);
            CHECK(v[i * 4 + corner].PosZ == 0);
            CHECK(v[i * 4 + corner].EggFlags[0] == 0);
            CHECK(v[i * 4 + corner].EggFlags[1] == 0);
        }

        const Vertex2D expected[] {
            {x, 27, 0, color, 0, 4.0f / 9.0f, {}},
            {x, 19, 0, color, 0, 0, {}},
            {x + 6, 19, 0, color, 1.0f / 3.0f, 0, {}},
            {x + 6, 27, 0, color, 1.0f / 3.0f, 4.0f / 9.0f, {}},
        };
        const size_t local_indices[] {0, 1, 3, 1, 2, 3};

        for (size_t corner = 0; corner < 4; corner++) {
            CheckSpriteVertex(v[i * 4 + corner], expected[corner]);
        }

        for (size_t index = 0; index < 6; index++) {
            CHECK(effect.Draws[0].Indices[i * 6 + index] == i * 4 + local_indices[index]);
        }
    }

    CHECK((client->WorkSched.GetDiagnostics().ParallelBatches > before) == client->WorkSched.IsParallel());
}

TEST_CASE("SpriteGeometryRegionAndMeshSnapshotsSurviveOwnerRelease")
{
    auto settings = MakeGeometrySettings(true);
    auto client = MakeClientEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto [atlas, allocation, atlas_pos] = client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {6, 6});
    ignore_unused(atlas_pos);
    SpriteMeshData mesh {{10, 20}, {2, 4}, {{0, 0}, {3, 6}, {6, 0}}, {2, 0, 1}};
    auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {6, 6}, ipos32 {}, atlas, std::move(allocation), frect32 {0.25f, 0.5f, 0.5f, 0.25f}, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
    RecordingQuadEffect effect;
    sprite->SetDrawEffect(&effect);
    client->SprMngr.DrawSpriteSizeExt(sprite, {100, 200}, {20, 40}, false, false, true, ucolor {20, 40, 60, 80});
    REQUIRE(client->SprMngr.DrawSpriteRegion(sprite, {0.1f, 0.1f}, {0.5f, 0.4f}, {10, 20}, {40, 60}, ucolor {10, 20, 30, 40}));
    sprite->SetDrawEffect(nullptr);
    sprite.reset();
    client->SprMngr.Flush();
    REQUIRE(effect.Draws.size() == 1);
    REQUIRE(effect.Draws[0].Vertices.size() == 7);
    REQUIRE(effect.Draws[0].Indices.size() == 9);
    const auto& vertices = effect.Draws[0].Vertices;
    CheckSpriteVertex(vertices[0], {104, 208, 0, ucolor {20, 40, 60, 80}, 0.25f, 0.5f, {}});
    CheckSpriteVertex(vertices[1], {110, 220, 0, ucolor {20, 40, 60, 80}, 0.5f, 0.75f, {}});
    CheckSpriteVertex(vertices[2], {116, 208, 0, ucolor {20, 40, 60, 80}, 0.75f, 0.5f, {}});
    CheckSpriteVertex(vertices[3], {20, 80, 0, ucolor {10, 20, 30, 40}, 0.25f, 2.0f / 3.0f, {}});
    CheckSpriteVertex(vertices[4], {20, 40, 0, ucolor {10, 20, 30, 40}, 0.25f, 0.5f, {}});
    CheckSpriteVertex(vertices[5], {50, 40, 0, ucolor {10, 20, 30, 40}, 0.5f, 0.5f, {}});
    CheckSpriteVertex(vertices[6], {50, 80, 0, ucolor {10, 20, 30, 40}, 0.5f, 2.0f / 3.0f, {}});
    CHECK(effect.Draws[0].Indices == vector<vindex_t> {2, 0, 1, 3, 4, 6, 4, 5, 6});
}

TEST_CASE("SpriteGeometrySheetCapturesTheSelectedFrameBeforeDrain")
{
    auto settings = MakeGeometrySettings(true);
    auto client = MakeClientEngine(settings, MakeClientTestResources({{"Sheet.png", BakerTests::MakeMultiFrameBakedSprite(4, 2, 2, 40)}}));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto sprite = client->SprMngr.LoadSprite("Sheet.png", AtlasType::IfaceSprites);
    auto sheet = sprite.dyn_cast<SpriteSheet>();
    REQUIRE(sheet);
    sheet->SetTime(0);
    auto first = sheet->GetCurSpr().dyn_cast<const AtlasSprite>();
    REQUIRE(first);
    frect32 first_uv = first->GetAtlasRect();
    RecordingQuadEffect effect;
    sheet->SetDrawEffect(&effect);
    client->SprMngr.DrawSprite(sheet, {10, 20}, ucolor {1, 2, 3, 4});
    sheet->SetTime(1);
    auto second = sheet->GetCurSpr().dyn_cast<const AtlasSprite>();
    REQUIRE(second);
    frect32 second_uv = second->GetAtlasRect();
    REQUIRE(first_uv != second_uv);
    client->SprMngr.DrawSprite(sheet, {30, 40}, ucolor {5, 6, 7, 8});
    sheet->SetDrawEffect(nullptr);
    sheet.reset();
    sprite.reset();
    client->SprMngr.Flush();
    REQUIRE(effect.Draws.size() == 1);
    REQUIRE(effect.Draws[0].Vertices.size() == 8);
    const auto& vertices = effect.Draws[0].Vertices;
    CheckSpriteVertex(vertices[1], {10, 20, 0, ucolor {1, 2, 3, 4}, first_uv.x, first_uv.y, {}});
    CheckSpriteVertex(vertices[5], {30, 40, 0, ucolor {5, 6, 7, 8}, second_uv.x, second_uv.y, {}});
}

class GeometryReadingSprite final : public Sprite
{
public:
    GeometryReadingSprite(ptr<SpriteManager> manager, ptr<const Sprite> delegate) :
        Sprite(manager, delegate->GetSize(), {}),
        _delegate {delegate}
    {
    }

    [[nodiscard]] auto GetBatchTexture() const -> nptr<const RenderTexture> override { return _delegate->GetBatchTexture(); }
    auto FillData(ptr<RenderDrawBuffer> buffer, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t override
    {
        ObservedVertex = buffer->Vertices[0];
        ObservedIndex = buffer->Indices[2];
        return _delegate->FillData(buffer, pos, colors);
    }

    mutable Vertex2D ObservedVertex {};
    mutable vindex_t ObservedIndex {};

private:
    ptr<const Sprite> _delegate;
};

TEST_CASE("SpriteGeometryUnsupportedSpriteAndAbortAreCompletionBoundaries")
{
    auto settings = MakeGeometrySettings(true);
    auto client = MakeClientEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto [atlas, allocation, atlas_pos] = client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {5, 7});
    ignore_unused(atlas_pos);
    auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {5, 7}, ipos32 {}, atlas, std::move(allocation), frect32 {0.25f, 0.5f, 0.5f, 0.25f}, vector<bool> {});
    RecordingQuadEffect effect;
    sprite->SetDrawEffect(&effect);
    auto clear_effect = scope_exit([&sprite]() noexcept { sprite->SetDrawEffect(nullptr); });
    GeometryReadingSprite external {&client->SprMngr, sprite};
    external.SetDrawEffect(&effect);
    client->SprMngr.DrawSprite(sprite, {10, 20}, ucolor {1, 2, 3, 4});
    client->SprMngr.DrawSprite(&external, {30, 40}, ucolor {5, 6, 7, 8});
    CheckSpriteVertex(external.ObservedVertex, {10, 27, 0, ucolor {1, 2, 3, 4}, 0.25f, 0.75f, {}});
    CHECK(external.ObservedIndex == 3);
    client->SprMngr.DrawSprite(sprite, {90, 100}, ucolor {13, 14, 15, 16});
    client->SprMngr.AbortScene();
    client->SprMngr.Flush();
    CHECK(effect.Draws.empty());
    client->SprMngr.DrawSprite(sprite, {70, 80}, ucolor {9, 10, 11, 12});
    client->SprMngr.Flush();
    REQUIRE(effect.Draws.size() == 1);
    REQUIRE(effect.Draws[0].Vertices.size() == 4);
    CheckSpriteVertex(effect.Draws[0].Vertices[1], {70, 80, 0, ucolor {9, 10, 11, 12}, 0.25f, 0.5f, {}});
}

TEST_CASE("SpriteGeometryFailedWorkerBatchDrainsAndDiscardsBeforePublication")
{
    auto settings = MakeGeometrySettings(true);
    auto client = MakeClientEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto [atlas, allocation, atlas_pos] = client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {5, 7});
    ignore_unused(atlas_pos);
    SpriteMeshData mesh {{5, 7}, {}, {{0, 0}, {5, 7}, {5, 0}}, {0, 9, 1}};
    auto invalid = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {5, 7}, ipos32 {}, atlas, std::move(allocation), frect32 {}, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
    RecordingQuadEffect effect;
    invalid->SetDrawEffect(&effect);
    auto clear_effect = scope_exit([&invalid]() noexcept { invalid->SetDrawEffect(nullptr); });

    if (!client->WorkSched.IsParallel()) {
        SKIP("This platform does not start client workers");
    }

    for (size_t i = 0; i < 128; i++) {
        client->SprMngr.DrawSprite(invalid, {}, ucolor {1, 2, 3, 4});
    }

    uint64_t before = client->WorkSched.GetDiagnostics().ParallelBatches;
    CHECK_THROWS(client->SprMngr.Flush());
    CHECK(client->WorkSched.GetDiagnostics().ParallelBatches == before + 1);
    CHECK(effect.Draws.empty());
    CHECK_NOTHROW(client->SprMngr.Flush());
    auto valid = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {5, 7}, ipos32 {}, atlas, nullptr, frect32 {}, vector<bool> {});
    valid->SetDrawEffect(&effect);
    auto clear_valid_effect = scope_exit([&valid]() noexcept { valid->SetDrawEffect(nullptr); });
    client->SprMngr.DrawSprite(valid, {10, 20}, ucolor {1, 2, 3, 4});
    client->SprMngr.Flush();
    REQUIRE(effect.Draws.size() == 1);
    REQUIRE(effect.Draws[0].Vertices.size() == 4);
    CHECK(effect.Draws[0].Indices == vector<vindex_t> {0, 1, 3, 1, 2, 3});
}

TEST_CASE("SpriteGeometryMapDrawCapturesAlphaRotationDepthAndTwoEggs")
{
    bool parallel = GENERATE(false, true);
    auto settings = MakeGeometrySettings(parallel);
    BakerTests::OverrideSetting(settings.Geometry.MapCameraAngle, 60.0f);
    auto client = MakeClientEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto [atlas, allocation, atlas_pos] = client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {4, 8});
    ignore_unused(atlas_pos);
    auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {4, 8}, ipos32 {}, atlas, std::move(allocation), frect32 {0.25f, 0.5f, 0.5f, 0.25f}, vector<bool> {});
    RecordingQuadEffect effect;
    sprite->SetDrawEffect(&effect);
    auto clear_effect = scope_exit([&sprite]() noexcept { sprite->SetDrawEffect(nullptr); });
    MapSpriteList sprites;
    ipos32 screen_offset {40, 60};
    uint8_t alpha = 130;

    for (size_t i = 0; i < 128; i++) {
        auto map_sprite = sprites.AddSprite(DrawOrderType::Item, {0, 0}, {}, &screen_offset, sprite, nullptr, nullptr, nullptr, &alpha, nullptr, nullptr, 0);
        map_sprite->SetAngle(90);
        map_sprite->SetMapProjected(true);
        map_sprite->SetEggAppearence(EggAppearenceType::Always);
        map_sprite->SetEggStructure(true);
    }

    client->SprMngr.SetEgg(TransparentEggSlot::Primary, {0, 0}, {40, 60}, {20, 30}, TransparentEggTarget::AnyOccluder);
    client->SprMngr.SetEgg(TransparentEggSlot::Secondary, {0, 0}, {40, 60}, {20, 30}, TransparentEggTarget::Structure);
    uint64_t before = client->WorkSched.GetDiagnostics().ParallelBatches;
    client->SprMngr.DrawSprites(sprites, {0, 0, 100, 100}, true, DrawOrderType::Item, DrawOrderType::Item, ucolor {26, 52, 78, 255}, &effect);
    REQUIRE(effect.Draws.size() == 1);
    REQUIRE(effect.Draws[0].Vertices.size() == 512);
    REQUIRE(effect.Draws[0].Indices.size() == 768);
    float32_t depth_slope = std::tan(GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT);
    const Vertex2D expected[] {
        {36, 55, 5 * depth_slope, ucolor {26, 52, 78, 130}, 0.25f, 0.75f, {1, 1}},
        {44, 55, 5 * depth_slope, ucolor {26, 52, 78, 130}, 0.25f, 0.5f, {1, 1}},
        {44, 57, 3 * depth_slope, ucolor {26, 52, 78, 130}, 0.75f, 0.5f, {1, 1}},
        {36, 57, 3 * depth_slope, ucolor {26, 52, 78, 130}, 0.75f, 0.75f, {1, 1}},
    };

    for (size_t i = 0; i < 128; i++) {
        for (size_t corner = 0; corner < 4; corner++) {
            CheckSpriteVertex(effect.Draws[0].Vertices[i * 4 + corner], expected[corner]);
        }
    }

    CHECK((client->WorkSched.GetDiagnostics().ParallelBatches > before) == client->WorkSched.IsParallel());
}

TEST_CASE("SpriteGeometryPreservesEffectScissorAndRenderTargetBoundaries")
{
    auto settings = MakeGeometrySettings(true);
    auto client = MakeClientEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto [atlas, allocation, atlas_pos] = client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {5, 7});
    ignore_unused(atlas_pos);
    auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {5, 7}, ipos32 {}, atlas, std::move(allocation), frect32 {}, vector<bool> {});
    RecordingQuadEffect first;
    RecordingQuadEffect second;
    auto clear_effect = scope_exit([&sprite]() noexcept { sprite->SetDrawEffect(nullptr); });
    sprite->SetDrawEffect(&first);
    client->SprMngr.DrawSprite(sprite, {10, 0}, ucolor {1, 2, 3, 4});
    sprite->SetDrawEffect(&second);
    client->SprMngr.DrawSprite(sprite, {20, 0}, ucolor {1, 2, 3, 4});
    client->SprMngr.PushScissor({0, 0, 100, 100});
    REQUIRE(first.Draws.size() == 1);
    REQUIRE(second.Draws.size() == 1);
    CHECK(first.Draws[0].StartIndex == 0);
    CHECK(second.Draws[0].StartIndex == 6);
    CHECK(first.Draws[0].IndicesToDraw == optional<size_t> {6});
    CHECK(second.Draws[0].IndicesToDraw == optional<size_t> {6});
    CHECK(second.Draws[0].Vertices[4].PosX == 20);
    client->SprMngr.DrawSprite(sprite, {30, 0}, ucolor {1, 2, 3, 4});
    client->SprMngr.PopScissor();
    REQUIRE(second.Draws.size() == 2);
    CHECK(second.Draws[1].Vertices[0].PosX == 30);
    auto& targets = client->SprMngr.GetRtMngr();
    auto first_target = targets.CreateRenderTarget(false, {80, 80}, false);
    auto second_target = targets.CreateRenderTarget(false, {90, 90}, false);
    targets.PushRenderTarget(first_target);
    client->SprMngr.DrawSprite(sprite, {40, 0}, ucolor {1, 2, 3, 4});
    targets.PushRenderTarget(second_target);
    REQUIRE(second.Draws.size() == 3);
    CHECK(second.Draws[2].Vertices[0].PosX == 40);
    client->SprMngr.DrawSprite(sprite, {50, 0}, ucolor {1, 2, 3, 4});
    targets.PopRenderTarget();
    REQUIRE(second.Draws.size() == 4);
    CHECK(second.Draws[3].Vertices[0].PosX == 50);
    targets.PopRenderTarget();
    targets.DeleteRenderTarget(second_target);
    targets.DeleteRenderTarget(first_target);
}

class GeometryPrimitiveEffect final : public RenderEffect
{
public:
    GeometryPrimitiveEffect() :
        RenderEffect(EffectUsage::Primitive, "Effects/Test_Recording.fofx", MakeRecordingQuadEffectLoader())
    {
    }

    void DrawBuffer(ptr<RenderDrawBuffer> buffer, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_texture) override
    {
        ignore_unused(start_index, indices_to_draw, custom_texture);
        Vertices.assign(buffer->Vertices.begin(), buffer->Vertices.begin() + numeric_cast<ptrdiff_t>(buffer->VertCount));
    }

    vector<Vertex2D> Vertices {};
};

TEST_CASE("SpriteGeometryWireframeCompletesPreparedVerticesBeforeReading")
{
    auto settings = MakeGeometrySettings(true);
    BakerTests::OverrideSetting(settings.Render.DrawWireframe, true);
    auto client = MakeClientEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto [atlas, allocation, atlas_pos] = client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {5, 7});
    ignore_unused(atlas_pos);
    auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {5, 7}, ipos32 {}, atlas, std::move(allocation), frect32 {0.25f, 0.5f, 0.5f, 0.25f}, vector<bool> {});
    RecordingQuadEffect effect;
    GeometryPrimitiveEffect primitive;
    auto original_primitive = client->EffectMngr.Effects.Primitive;
    client->EffectMngr.Effects.Primitive = &primitive;
    auto restore_primitive = scope_exit([&client, original_primitive]() noexcept { client->EffectMngr.Effects.Primitive = original_primitive; });
    sprite->SetDrawEffect(&effect);
    auto clear_effect = scope_exit([&sprite]() noexcept { sprite->SetDrawEffect(nullptr); });
    client->SprMngr.DrawSprite(sprite, {10, 20}, ucolor {1, 2, 3, 4});
    client->SprMngr.Flush();
    REQUIRE(primitive.Vertices.size() == 12);
    CheckSpriteVertex(primitive.Vertices[0], {10, 27, 0, ucolor {255, 0, 255, 255}, 0, 0, {}});
    CheckSpriteVertex(primitive.Vertices[1], {10, 20, 0, ucolor {255, 0, 255, 255}, 0, 0, {}});
    CheckSpriteVertex(primitive.Vertices[3], {15, 27, 0, ucolor {255, 0, 255, 255}, 0, 0, {}});
    REQUIRE(effect.Draws.size() == 1);
    REQUIRE(effect.Draws[0].Vertices.size() == 4);
    CheckSpriteVertex(effect.Draws[0].Vertices[0], {10, 27, 0, ucolor {1, 2, 3, 4}, 0.25f, 0.75f, {}});
}

TEST_CASE("SpriteGeometryMeshRoundsColorComponents")
{
    SpriteMeshData mesh {{3, 3}, {}, {{1, 1}, {2, 2}, {3, 0}}, {0, 1, 2}};
    PreparedSpriteGeometry geometry {.DrawRect = {0, 0, 3, 3}, .ColorLeft = ucolor {0, 10, 0, 10}, .ColorRight = ucolor {10, 0, 5, 5}, .Mesh = &mesh, .FrameSize = {3, 3}};
    vector<Vertex2D> vertices(3);
    vector<vindex_t> indices(3);
    WritePreparedSpriteGeometry(geometry, vertices, indices, 0);
    CHECK(vertices[0].Color == ucolor {3, 7, 2, 8});
    CHECK(vertices[1].Color == ucolor {7, 3, 3, 7});
    CHECK(vertices[2].Color == ucolor {10, 0, 5, 5});
}

TEST_CASE("SpriteGeometryMixedTextAndSpritesSurviveBufferGrowth")
{
    bool parallel = GENERATE(false, true);
    auto settings = MakeGeometrySettings(parallel);
    auto client = MakeClientEngine(settings, MakeClientTestResources(MakeUnitTestFontResources()));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    client->FontMngr.BindFoFont(FontType::Default, "UnitTestFont.fofnt", AtlasType::OneImage, false, false, 1.0f);
    RecordingQuadEffect glyph_effect;
    client->FontMngr.SetFontEffect(FontType::Default, &glyph_effect);
    auto clear_font_effect = scope_exit([&client]() noexcept { client->FontMngr.SetFontEffect(FontType::Default, nullptr); });
    auto [atlas, allocation, atlas_pos] = client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {5, 7});
    ignore_unused(atlas_pos);
    auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {5, 7}, ipos32 {}, atlas, std::move(allocation), frect32 {0.25f, 0.5f, 0.5f, 0.25f}, vector<bool> {});
    RecordingQuadEffect sprite_effect;
    sprite->SetDrawEffect(&sprite_effect);
    auto clear_sprite_effect = scope_exit([&sprite]() noexcept { sprite->SetDrawEffect(nullptr); });
    ucolor color {26, 52, 78, 130};
    uint64_t before = client->WorkSched.GetDiagnostics().ParallelBatches;
    client->SprMngr.DrawSprite(sprite, {10, 20}, color);
    client->FontMngr.DrawText({10, 20, 12000, 30}, string(2048, 'A'), color, {});
    client->SprMngr.Flush();
    REQUIRE(sprite_effect.Draws.size() == 1);
    REQUIRE(glyph_effect.Draws.size() == 1);
    CHECK(sprite_effect.Draws[0].StartIndex == 0);
    CHECK(sprite_effect.Draws[0].IndicesToDraw == optional<size_t> {6});
    CHECK(glyph_effect.Draws[0].StartIndex == 6);
    CHECK(glyph_effect.Draws[0].IndicesToDraw == optional<size_t> {2048 * 6});
    REQUIRE(glyph_effect.Draws[0].Vertices.size() == 2049 * 4);
    REQUIRE(glyph_effect.Draws[0].Indices.size() == 2049 * 6);
    CheckSpriteVertex(sprite_effect.Draws[0].Vertices[0], {10, 27, 0, color, 0.25f, 0.75f, {}});
    CheckSpriteVertex(glyph_effect.Draws[0].Vertices[4], {9, 27, 0, color, 0, 4.0f / 9.0f, {}});
    CheckSpriteVertex(glyph_effect.Draws[0].Vertices[8195], {10250, 27, 0, color, 1.0f / 3.0f, 4.0f / 9.0f, {}});
    CHECK(glyph_effect.Draws[0].Indices.back() == 8195);
    CHECK((client->WorkSched.GetDiagnostics().ParallelBatches > before) == client->WorkSched.IsParallel());
}

class GeometryOverridingAtlasSprite final : public AtlasSprite
{
public:
    using AtlasSprite::AtlasSprite;

    auto FillData(ptr<RenderDrawBuffer> buffer, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t override
    {
        ignore_unused(colors);
        Calls++;
        ObservedVertex = buffer->Vertices[0];
        frect32 adjusted = pos;
        adjusted.x += 100;
        return AtlasSprite::FillData(buffer, adjusted, {ucolor {21, 22, 23, 24}, ucolor {31, 32, 33, 34}});
    }

    mutable size_t Calls {};
    mutable Vertex2D ObservedVertex {};
};

TEST_CASE("SpriteGeometryAtlasSubclassPreservesItsFillDataOverride")
{
    bool parallel = GENERATE(false, true);
    auto settings = MakeGeometrySettings(parallel);
    auto client = MakeClientEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto [atlas, allocation, atlas_pos] = client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {5, 7});
    ignore_unused(atlas_pos);
    auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {5, 7}, ipos32 {}, atlas, std::move(allocation), frect32 {}, vector<bool> {});
    GeometryOverridingAtlasSprite external {&client->SprMngr, {5, 7}, {}, atlas, nullptr, {0.25f, 0.5f, 0.5f, 0.25f}, {}};
    RecordingQuadEffect effect;
    sprite->SetDrawEffect(&effect);
    external.SetDrawEffect(&effect);
    auto clear_effect = scope_exit([&sprite]() noexcept { sprite->SetDrawEffect(nullptr); });
    client->SprMngr.DrawSprite(sprite, {10, 20}, ucolor {1, 2, 3, 4});
    client->SprMngr.DrawSprite(&external, {30, 40}, ucolor {5, 6, 7, 8});
    client->SprMngr.Flush();
    CHECK(external.Calls == 1);
    CheckSpriteVertex(external.ObservedVertex, {10, 27, 0, ucolor {1, 2, 3, 4}, 0, 0, {}});
    REQUIRE(effect.Draws.size() == 1);
    REQUIRE(effect.Draws[0].Vertices.size() == 8);
    CheckSpriteVertex(effect.Draws[0].Vertices[4], {130, 47, 0, ucolor {21, 22, 23, 24}, 0.25f, 0.75f, {}});
    CheckSpriteVertex(effect.Draws[0].Vertices[6], {135, 40, 0, ucolor {31, 32, 33, 34}, 0.75f, 0.5f, {}});
}

#if FO_ENABLE_3D
TEST_CASE("SpriteGeometryModelCropPreservesLightingThroughManagerDraw")
{
    bool parallel = GENERATE(false, true);
    constexpr string_view model_path = "Models/GeometryLighting.fo3d";
    constexpr string_view mesh_path = "Models/GeometryLighting.fbx";
    auto mesh_blob = MakeRuntimeModelTriangleMesh();
    vector<pair<string, vector<uint8_t>>> resources;
    resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(model_path));
    resources.emplace_back(string {mesh_path}, mesh_blob);
    resources.emplace_back(string {model_path}, MakeRuntimeModelDescription(model_path, mesh_path, mesh_blob));
    auto settings = MakeGeometrySettings(parallel);
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(resources)));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto sprite = client->SprMngr.LoadSprite(client->Hashes.to_hashed_string(model_path), AtlasType::MapSprites, true);
    auto model_sprite = sprite.dyn_cast<ModelSprite>();
    REQUIRE(model_sprite);
    model_sprite->DrawToAtlas();
    ucolor left {10, 20, 30, 130};
    ucolor right {110, 120, 130, 130};
    auto direct = client->SprMngr.GetRender().CreateDrawBuffer(false);
    REQUIRE(model_sprite->FillData(direct, {0, 0, 10, 20}, {left, right}) == 6);
    ucolor expected_left = direct->Vertices[0].Color;
    ucolor expected_right = direct->Vertices[2].Color;
    INFO("crop=" << model_sprite->GetSize().width << ", lighting=" << model_sprite->GetModel()->GetLightingSize().width);
    REQUIRE((expected_left != left || expected_right != right));
    RecordingQuadEffect effect;
    model_sprite->SetDrawEffect(&effect);
    auto clear_effect = scope_exit([&model_sprite]() noexcept { model_sprite->SetDrawEffect(nullptr); });
    MapSpriteList sprites;
    ipos32 screen_offset {200, 200};
    uint8_t alpha = 130;
    vector<ucolor> lights(9, ucolor {0, 0, 0, 0});
    lights[3] = ucolor {200, 200, 200, 0};

    for (size_t i = 0; i < 128; i++) {
        auto map_sprite = sprites.AddSprite(DrawOrderType::Item, {1, 1}, {}, &screen_offset, model_sprite, nullptr, nullptr, nullptr, &alpha, nullptr, nullptr, 0);
        map_sprite->SetLight(CornerType::EastWest, &lights[0], {3, 3});
    }

    uint64_t before = client->WorkSched.GetDiagnostics().ParallelBatches;
    client->SprMngr.DrawSprites(sprites, {0, 0, 1000, 1000}, false, DrawOrderType::Item, DrawOrderType::Item, left, &effect);
    REQUIRE(effect.Draws.size() == 1);
    REQUIRE(effect.Draws[0].Vertices.size() == 512);

    for (size_t i = 0; i < 128; i++) {
        CHECK(effect.Draws[0].Vertices[i * 4].Color == expected_left);
        CHECK(effect.Draws[0].Vertices[i * 4 + 1].Color == expected_left);
        CHECK(effect.Draws[0].Vertices[i * 4 + 2].Color == expected_right);
        CHECK(effect.Draws[0].Vertices[i * 4 + 3].Color == expected_right);
    }

    CHECK((client->WorkSched.GetDiagnostics().ParallelBatches > before) == client->WorkSched.IsParallel());
}
#endif

FO_END_NAMESPACE

