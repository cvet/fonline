// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "EffekseerCompiler.h"
#include "EffekseerExtension.h"
#include "MapView.h"
#include "ParticleSprites.h"
#include "Test_ClientEngineHelpers.h"

FO_BEGIN_NAMESPACE

using namespace ClientEngineTests;

void Client_Map_DrawMapSprite(ptr<MapView> self, ptr<MapSpriteHolder> mapSpr);
bool Client_Critter_GetBoneMapAnchor(ptr<CritterView> self, hstring boneName, float32_t moveX, float32_t moveY, float32_t moveZ, ipos32& boneOffset, float32_t& depthOffset);
bool Client_Critter_GetBoneSpriteMapAnchor(ptr<CritterView> self, hstring boneName, float32_t moveX, float32_t moveY, float32_t moveZ, ipos32& boneOffset, float32_t& depthOffset);

class TestUnlitOverlaySprite final : public Sprite
{
public:
    TestUnlitOverlaySprite(ptr<SpriteManager> spr_mngr, shared_ptr<Sprite> base, shared_ptr<Sprite> overlay) :
        Sprite(spr_mngr, base->GetSize(), base->GetOffset()),
        _base {std::move(base)},
        _overlay {std::move(overlay)}
    {
    }

    [[nodiscard]] auto GetUnlitOverlay() const -> nptr<const Sprite> override { return nptr<const Sprite> {_overlay.get()}; }
    [[nodiscard]] auto GetBatchTexture() const -> nptr<const RenderTexture> override { return _base->GetBatchTexture(); }

    auto FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t override { return _base->FillData(dbuf, pos, colors); }

private:
    shared_ptr<Sprite> _base;
    shared_ptr<Sprite> _overlay;
};

static auto ReadRecordedLayerQuad(const RecordedQuadDraw& draw) -> vector<Vertex2D>
{
    REQUIRE(draw.IndicesToDraw.has_value());
    REQUIRE(*draw.IndicesToDraw == 6);
    REQUIRE(draw.StartIndex + 6 <= draw.Indices.size());

    size_t vertex_start {numeric_cast<size_t>(draw.Indices[draw.StartIndex])};

    REQUIRE(vertex_start + 4 <= draw.Vertices.size());
    REQUIRE(draw.Indices[draw.StartIndex + 0] == numeric_cast<vindex_t>(vertex_start + 0));
    REQUIRE(draw.Indices[draw.StartIndex + 1] == numeric_cast<vindex_t>(vertex_start + 1));
    REQUIRE(draw.Indices[draw.StartIndex + 2] == numeric_cast<vindex_t>(vertex_start + 3));
    REQUIRE(draw.Indices[draw.StartIndex + 3] == numeric_cast<vindex_t>(vertex_start + 1));
    REQUIRE(draw.Indices[draw.StartIndex + 4] == numeric_cast<vindex_t>(vertex_start + 2));
    REQUIRE(draw.Indices[draw.StartIndex + 5] == numeric_cast<vindex_t>(vertex_start + 3));

    vector<Vertex2D> vertices {};
    vertices.reserve(4);

    for (size_t i {0}; i < 4; i++) {
        vertices.emplace_back(draw.Vertices[vertex_start + i]);
    }

    return vertices;
}

TEST_CASE("MapSpriteDepthOffsetReachesBothRenderPaths")
{
    class RecordingSceneSprite final : public Sprite
    {
    public:
        explicit RecordingSceneSprite(ptr<SpriteManager> manager) :
            Sprite(manager, isize32 {8, 8}, ipos32 {})
        {
        }

        auto IsDirectDraw() const -> bool override { return true; }

        auto FillData(ptr<RenderDrawBuffer> buffer, const frect32& position, const tuple<ucolor, ucolor>& colors) const -> size_t override
        {
            ignore_unused(buffer, position, colors);
            return 0;
        }

        void DrawInScene(fpos32 position, float32_t depth) const override { Calls.emplace_back(position, depth); }

        mutable vector<pair<fpos32, float32_t>> Calls {};
    };

    auto settings {MakeClientTestSettings()};
    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back("DepthSprite.png", BakerTests::MakeMinimalBakedSprite(8, 8));
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    RecordingQuadEffect effect {};
    ipos32 field_offset {100, 100};

    SECTION("DirectSceneReceivesTheDepthWithoutMovingItsAnchor")
    {
        RecordingSceneSprite visual {&client->SprMngr};
        MapSpriteList list {};
        auto sprite {list.AddSprite(DrawOrderType::Particles, mpos {3, 3}, ipos32 {12, -17}, &field_offset, &visual, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0)};

        for (float32_t delta : {0.0f, 12.5f, -9.25f}) {
            sprite->SetDepthOffset(delta);
            client->SprMngr.DrawSprites(list, irect32 {0, 0, 320, 200}, 0, false, DrawOrderType::Particles, DrawOrderType::Particles, ucolor {255, 255, 255}, &effect);
            REQUIRE(visual.Calls.size() > 0);
            const auto& baseline {visual.Calls.front()};
            const auto& actual {visual.Calls.back()};
            CHECK(actual.first == baseline.first);
            CHECK(is_float_equal(actual.second, baseline.second + delta));
        }

        REQUIRE(visual.Calls.size() == 3);
    }

    SECTION("StandingSpriteVerticesReceiveTheSameDepthAdjustment")
    {
        auto visual {client->ResMngr.LoadSprite(client->Hashes.to_hashed_string("DepthSprite.png"), AtlasType::MapSprites)};
        REQUIRE(visual);
        MapSpriteList list {};
        auto sprite {list.AddSprite(DrawOrderType::Particles, mpos {3, 3}, ipos32 {12, -17}, &field_offset, visual.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0)};
        vector<Vertex2D> baseline {};

        for (float32_t delta : {0.0f, 12.5f, -9.25f}) {
            sprite->SetDepthOffset(delta);
            effect.Draws.clear();
            client->SprMngr.DrawSprites(list, irect32 {0, 0, 320, 200}, 0, false, DrawOrderType::Particles, DrawOrderType::Particles, ucolor {255, 255, 255}, &effect);
            REQUIRE(effect.Draws.size() == 1);
            const auto& vertices {effect.Draws.front().Vertices};
            REQUIRE(vertices.size() == 4);

            if (baseline.empty()) {
                baseline = vertices;
            }

            for (size_t index {0}; index < vertices.size(); index++) {
                CHECK(is_float_equal(vertices[index].PosX, baseline[index].PosX));
                CHECK(is_float_equal(vertices[index].PosY, baseline[index].PosY));
                CHECK(is_float_equal(vertices[index].PosZ, baseline[index].PosZ + delta));
            }
        }
    }

    SECTION("PointAnchorRejectsInvalidHiddenAndDirectSceneSprites")
    {
        auto visual {client->ResMngr.LoadSprite(client->Hashes.to_hashed_string("DepthSprite.png"), AtlasType::MapSprites)};
        REQUIRE(visual);
        MapSpriteList list {};
        auto sprite {list.AddSprite(DrawOrderType::Item, mpos {3, 3}, ipos32 {12, -17}, &field_offset, visual.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0)};
        CHECK(client->SprMngr.GetMapSpritePointAnchor(sprite, ipos32 {}).has_value());

        sprite->SetHidden(true);
        CHECK_FALSE(client->SprMngr.GetMapSpritePointAnchor(sprite, ipos32 {}).has_value());
        sprite->SetHidden(false);
        sprite->Invalidate();
        CHECK_FALSE(client->SprMngr.GetMapSpritePointAnchor(sprite, ipos32 {}).has_value());

        auto unavailable {list.AddSprite(DrawOrderType::Item, mpos {3, 3}, ipos32 {12, -17}, &field_offset, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0)};
        CHECK_FALSE(client->SprMngr.GetMapSpritePointAnchor(unavailable, ipos32 {}).has_value());

        RecordingSceneSprite direct {&client->SprMngr};
        auto scene {list.AddSprite(DrawOrderType::Particles, mpos {3, 3}, ipos32 {12, -17}, &field_offset, &direct, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0)};
        CHECK_FALSE(client->SprMngr.GetMapSpritePointAnchor(scene, ipos32 {}).has_value());
    }

    SECTION("PointAnchorRejectsNonFiniteDepthAndUnrepresentableSums")
    {
        auto visual {client->ResMngr.LoadSprite(client->Hashes.to_hashed_string("DepthSprite.png"), AtlasType::MapSprites)};
        REQUIRE(visual);
        MapSpriteList list {};
        auto sprite {list.AddSprite(DrawOrderType::Item, mpos {3, 3}, ipos32 {12, -17}, &field_offset, visual.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0)};
        optional<pair<ipos32, float32_t>> anchor {};

        for (float32_t invalid : {std::numeric_limits<float32_t>::quiet_NaN(), std::numeric_limits<float32_t>::infinity(), -std::numeric_limits<float32_t>::infinity()}) {
            sprite->SetDepthOffset(invalid);
            CHECK_NOTHROW(anchor = client->SprMngr.GetMapSpritePointAnchor(sprite, ipos32 {}));
            CHECK_FALSE(anchor.has_value());
        }

        sprite->SetDepthOffset(0.0f);
        CHECK(client->SprMngr.GetMapSpritePointAnchor(sprite, ipos32 {}).has_value());

        ipos32 safe_base {std::numeric_limits<int32_t>::max() - 1024, std::numeric_limits<int32_t>::min() + 1024};
        auto large {list.AddSprite(DrawOrderType::Item, mpos {3, 3}, safe_base, &field_offset, visual.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0)};
        ipos32 safe_local {123, -456};
        CHECK_NOTHROW(anchor = client->SprMngr.GetMapSpritePointAnchor(large, safe_local));
        REQUIRE(anchor.has_value());
        CHECK(anchor->first == ipos32 {safe_base.x + safe_local.x - GameSettings::MAP_HEX_WIDTH / 2, safe_base.y + safe_local.y - GameSettings::MAP_HEX_HEIGHT / 2});

        // The identity path must preserve integers above float32's exact range
        ipos32 exact_local {16777217, -16777219};
        auto exact {list.AddSprite(DrawOrderType::Item, mpos {3, 3}, ipos32 {}, &field_offset, visual.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0)};
        CHECK_NOTHROW(anchor = client->SprMngr.GetMapSpritePointAnchor(exact, exact_local));
        REQUIRE(anchor.has_value());
        CHECK(anchor->first == ipos32 {exact_local.x - GameSettings::MAP_HEX_WIDTH / 2, exact_local.y - GameSettings::MAP_HEX_HEIGHT / 2});

        auto overflow {list.AddSprite(DrawOrderType::Item, mpos {3, 3}, ipos32 {std::numeric_limits<int32_t>::max() - 4, 0}, &field_offset, visual.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0)};
        CHECK_NOTHROW(anchor = client->SprMngr.GetMapSpritePointAnchor(overflow, ipos32 {100, 0}));
        CHECK_FALSE(anchor.has_value());

        auto center_overflow {list.AddSprite(DrawOrderType::Item, mpos {3, 3}, ipos32 {std::numeric_limits<int32_t>::max() - 2, 0}, &field_offset, visual.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0)};
        CHECK_NOTHROW(anchor = client->SprMngr.GetMapSpritePointAnchor(center_overflow, ipos32 {4, 0}));
        CHECK_FALSE(anchor.has_value());

        auto underflow {list.AddSprite(DrawOrderType::Item, mpos {3, 3}, ipos32 {std::numeric_limits<int32_t>::min() + 4, 0}, &field_offset, visual.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0)};
        CHECK_NOTHROW(anchor = client->SprMngr.GetMapSpritePointAnchor(underflow, ipos32 {-100, 0}));
        CHECK_FALSE(anchor.has_value());

        auto rotated {list.AddSprite(DrawOrderType::Item, mpos {3, 3}, ipos32 {}, &field_offset, visual.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0)};
        rotated->SetAngle(45);
        CHECK_NOTHROW(anchor = client->SprMngr.GetMapSpritePointAnchor(rotated, ipos32 {std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max()}));
        CHECK_FALSE(anchor.has_value());
    }
}

TEST_CASE("MapSpriteStandingPointAnchorOracle")
{
    struct PointTransform
    {
        int16_t Angle {};
        bool Flat {};
        bool MapProjected {};
    };

    auto settings {MakeClientTestSettings()};
    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back("StandingAnchor.png", BakerTests::MakeMinimalBakedSprite(32, 96));
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto visual {client->ResMngr.LoadSprite(client->Hashes.to_hashed_string("StandingAnchor.png"), AtlasType::MapSprites)};
    REQUIRE(visual);
    REQUIRE(visual->GetSize() == isize32 {32, 96});

    visual->SetOffset(ipos32 {4, -3});
    RecordingQuadEffect effect {};
    ipos32 center {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
    ipos32 sprite_offset {12, -9};
    ipos32 root_offset {7, -11};
    ipos32 local_point {3, -36};
    array<PointTransform, 6> transforms {{{0, false, false}, {37, false, false}, {-23, false, true}, {0, true, false}, {37, true, false}, {-23, true, true}}};

    for (mpos contact : {mpos {8, 8}, mpos {10, 8}}) {
        ipos32 field_offset {ipos32 {180, 160} + GeometryHelper::GetHexOffset(mpos {8, 8}, contact)};

        for (int16_t elevation : {int16_t {0}, int16_t {18}, int16_t {-13}}) {
            for (auto transform : transforms) {
                DrawOrderType order {transform.Flat ? DrawOrderType::FlatItemAfterLight : DrawOrderType::Item};
                MapSpriteList walls {};
                auto wall {walls.AddSprite(order, contact, center, &field_offset, visual.get(), nullptr, &sprite_offset, &root_offset, nullptr, nullptr, nullptr, 0)};
                wall->SetElevation(elevation);
                wall->SetDepthOffset(7.5f);
                wall->SetAngle(transform.Angle);
                wall->SetMapProjected(transform.MapProjected);
                effect.Draws.clear();
                client->SprMngr.DrawSprites(walls, irect32 {0, 0, 640, 400}, 0, false, order, order, ucolor {255, 255, 255}, &effect);
                REQUIRE(effect.Draws.size() == 1);
                vector<Vertex2D> vertices {ReadRecordedLayerQuad(effect.Draws.front())};
                REQUIRE(vertices[1].TexU == vertices[0].TexU);
                REQUIRE(vertices[1].TexU < vertices[2].TexU);
                REQUIRE(vertices[1].TexV == vertices[2].TexV);
                REQUIRE(vertices[1].TexV < vertices[0].TexV);
                REQUIRE(wall->GetSpriteRootOffset() == ipos32 {12, 99});

                // Texture coordinates identify the original corners after the renderer transforms their positions
                float32_t u {15.0f / 32.0f};
                float32_t v {63.0f / 96.0f};
                fpos32 precise {vertices[1].PosX + u * (vertices[2].PosX - vertices[1].PosX) + v * (vertices[0].PosX - vertices[1].PosX), vertices[1].PosY + u * (vertices[2].PosY - vertices[1].PosY) + v * (vertices[0].PosY - vertices[1].PosY)};
                float32_t precise_depth {vertices[1].PosZ + u * (vertices[2].PosZ - vertices[1].PosZ) + v * (vertices[0].PosZ - vertices[1].PosZ)};
                auto top {std::ranges::min_element(vertices, {}, &Vertex2D::PosY)};
                auto bottom {std::ranges::max_element(vertices, {}, &Vertex2D::PosY)};
                REQUIRE(top->PosY < bottom->PosY);
                float32_t slope {(bottom->PosZ - top->PosZ) / (bottom->PosY - top->PosY)};

                ipos32 draw_root {wall->GetDrawRootPos()};
                ipos32 point {draw_root + ipos32 {iround<int32_t>(precise.x - numeric_cast<float32_t>(draw_root.x)), iround<int32_t>(precise.y - numeric_cast<float32_t>(draw_root.y))}};
                float32_t target_depth {precise_depth + (numeric_cast<float32_t>(point.y) - precise.y) * slope};
                REQUIRE(std::isfinite(target_depth));

                ipos32 offset {point - field_offset - center};
                MapSpriteList points {};
                auto marker {points.AddSprite(DrawOrderType::Particles, contact, center, &field_offset, visual.get(), nullptr, &offset, nullptr, nullptr, nullptr, nullptr, 0)};
                CHECK(marker->GetDrawRootPos() == point);
                vec3 ground_projection {marker->GetMapRootProjection()};
                REQUIRE(std::abs(ground_projection.z - target_depth) > 1.0f);

                float32_t correction {target_depth - ground_projection.z};
                marker->SetDepthOffset(correction);
                CHECK(std::abs(marker->GetMapRootProjection().z - target_depth) < 0.002f);
                CHECK(marker->GetDrawRootPos() == point);

                optional<pair<ipos32, float32_t>> native_anchor {client->SprMngr.GetMapSpritePointAnchor(wall, local_point)};
                REQUIRE(native_anchor.has_value());
                CHECK(native_anchor->first == offset);
                CHECK(field_offset + center + native_anchor->first == point);
                CHECK(std::abs(native_anchor->second - correction) < 0.002f);
                CHECK(std::abs(ground_projection.z + native_anchor->second - target_depth) < 0.002f);

                vec3 carrier_projection {wall->GetMapRootProjection()};

                if (transform.Flat) {
                    CHECK(std::abs(target_depth - carrier_projection.z) < 0.002f);
                }
                else if (transform.Angle == 0 && !transform.MapProjected) {
                    float32_t without_root {GeometryHelper::ProjectMapYToVerticalDepth(carrier_projection.y + numeric_cast<float32_t>(local_point.y), carrier_projection.y, carrier_projection.z)};
                    REQUIRE(std::abs(without_root - target_depth) > 1.0f);

                    if (elevation != 0) {
                        ipos32 old_offset {sprite_offset + local_point};
                        CHECK(field_offset + center + old_offset != point);
                        float32_t without_rounding {GeometryHelper::ProjectMapYToVerticalDepth(ground_projection.y, carrier_projection.y, carrier_projection.z)};
                        REQUIRE(std::abs(without_rounding - target_depth) > 0.02f);
                    }
                }

                if (transform.Angle != 0 || transform.MapProjected) {
                    CHECK(point != draw_root + local_point);
                }

                string quad {};

                for (auto&& vertex : vertices) {
                    quad += strex("|v={:.9g},{:.9g},{:.9g}", vertex.PosX, vertex.PosY, vertex.PosZ).str();
                }

                INFO(strex("POINT_MATRIX|hex={},{}|elevation={}|angle={}|flat={}|projected={}|x={}|y={}|depth={:.9g}|correction={:.9g}{}", contact.x, contact.y, elevation, transform.Angle, transform.Flat ? 1 : 0, transform.MapProjected ? 1 : 0, point.x, point.y, target_depth, correction, quad).str());
                CHECK(std::isfinite(correction));
            }
        }
    }
}

TEST_CASE("MapSpriteDepthOffsetIsCopiedAndValidatedBeforeAttachment")
{
    auto settings {MakeClientTestSettings()};
    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back("DepthSprite.png", BakerTests::MakeMinimalBakedSprite(8, 8));
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    ProtoMap proto {client->Hashes.to_hashed_string("DepthMap"), client->GetPropertyRegistrar(MapView::ENTITY_TYPE_NAME)};
    proto.SetSize(msize {16, 16});
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, make_ptr(&proto), isize32 {320, 200}, nullptr)};
    auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
    map->InstantScrollTo(mpos {8, 8});
    map->RebuildMap();
    REQUIRE(map->IsHexToDraw(mpos {8, 8}));

    MapSpriteHolder holder {};
    holder.SprId = client->AnimLoad(client->Hashes.to_hashed_string("DepthSprite.png"), AtlasType::MapSprites);
    holder.Hex = mpos {8, 8};
    holder.Elevation = 18;
    holder.DepthOffset = 12.5f;
    REQUIRE(holder.SprId != 0);

    REQUIRE_NOTHROW(Client_Map_DrawMapSprite(map, &holder));
    REQUIRE(holder.Valid);
    REQUIRE(holder.MSpr);

    auto original_sprite {holder.MSpr};

    CHECK(holder.MSpr->GetElevation() == 18);
    CHECK(holder.MSpr->GetDepthOffset() == 12.5f);

    for (float32_t invalid : {std::numeric_limits<float32_t>::quiet_NaN(), std::numeric_limits<float32_t>::infinity(), -std::numeric_limits<float32_t>::infinity()}) {
        holder.DepthOffset = invalid;
        CHECK_THROWS_AS(Client_Map_DrawMapSprite(map, &holder), ScriptException);
        CHECK(holder.Valid);
        CHECK(holder.MSpr == original_sprite);
        CHECK(holder.MSpr->GetElevation() == 18);
        CHECK(holder.MSpr->GetDepthOffset() == 12.5f);

        // Validation must also reject non-finite input before the invalid-hex/missing-sprite early returns
        MapSpriteHolder invalid_holder {};
        invalid_holder.Hex = mpos {100, 100};
        invalid_holder.DepthOffset = invalid;
        CHECK_THROWS_AS(Client_Map_DrawMapSprite(map, &invalid_holder), ScriptException);
        CHECK_FALSE(invalid_holder.Valid);
        CHECK_FALSE(invalid_holder.MSpr);
    }

    holder.StopDraw();
    CHECK_FALSE(holder.Valid);
}

template<typename T>
concept client_animation_playback_init = requires(T& client) { client.OnCritterAnimationPlaybackInit; };

template<typename T>
concept client_legacy_animation_init = requires(T& client) { client.OnCritterAnimationInit; };

static_assert(client_animation_playback_init<ClientEngine>);
static_assert(!client_legacy_animation_init<ClientEngine>);

FO_SCRIPT_API vector<ident_t> Client_Critter_GetAttachedCritterIds(ptr<CritterView> self);

void Client_Game_DrawText(ptr<ClientEngine> client, string_view text, ipos32 pos, isize32 size, ucolor color, TextFormat format);
void Client_Game_ActivateOffscreenSurface(ptr<ClientEngine> client, bool forceClear);
void Client_Game_PresentOffscreenSurface(ptr<ClientEngine> client, int32_t effectSubtype, ipos32 pos, isize32 size);
void Client_Game_PresentOffscreenSurface(ptr<ClientEngine> client, int32_t effectSubtype, ipos32 pos, isize32 size, float32_t scriptValue0, float32_t scriptValue1, float32_t scriptValue2, float32_t scriptValue3);
void Client_Game_PresentOffscreenSurface(ptr<ClientEngine> client, int32_t effectSubtype, int32_t fromX, int32_t fromY, int32_t fromW, int32_t fromH, int32_t toX, int32_t toY, int32_t toW, int32_t toH);
void Client_Game_SetEffect(ptr<ClientEngine> client, EffectType effectType, int64_t effectSubtype, string_view effectPath);

TEST_CASE("ClientScriptTextRejectsArithmeticOverflow")
{
    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(MakeUnitTestFontResources()))};

    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    client->FontMngr.BindFoFont(FontType::Default, "UnitTestFont.fofnt", AtlasType::IfaceSprites, false, false, 1.0f);
    client->CanDrawInScripts = true;

    CHECK_THROWS_AS(Client_Game_DrawText(client, "x", {}, {std::numeric_limits<int32_t>::min(), 1}, {}, {}), OverflowException);
    CHECK_THROWS_AS(Client_Game_DrawText(client, "x", {}, {1, std::numeric_limits<int32_t>::min()}, {}, {}), OverflowException);
    CHECK_THROWS_AS(Client_Game_DrawText(client, "x", {std::numeric_limits<int32_t>::min(), 0}, {-1, 1}, {}, {}), OverflowException);
    CHECK_THROWS_AS(Client_Game_DrawText(client, "x", {0, std::numeric_limits<int32_t>::min()}, {1, -1}, {}, {}), OverflowException);
    CHECK_NOTHROW(Client_Game_DrawText(client, "x", {10, 10}, {-1, -1}, {}, {}));
}

TEST_CASE("ClientScriptOffscreenOverflowPreservesActiveSurface")
{
    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(MakeBakedEffectResources("Effects/UnitTestOffscreen.fofx")))};

    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    client->CanDrawInScripts = true;
    Client_Game_SetEffect(client, EffectType::Offscreen, 0, "Effects/UnitTestOffscreen.fofx");
    client->SprMngr.BeginScene();

    auto abort_scene {scope_exit([&client]() noexcept { client->SprMngr.AbortScene(); })};
    Client_Game_ActivateOffscreenSurface(client, true);
    REQUIRE(client->ActiveOffscreenSurfaces.size() == 1);
    auto surface {client->ActiveOffscreenSurfaces.back()};

    SECTION("Rectangle")
    {
        CHECK_THROWS_AS(Client_Game_PresentOffscreenSurface(client, 0, {std::numeric_limits<int32_t>::max(), 0}, {1, 1}), OverflowException);
    }

    SECTION("RectangleWithScriptValues")
    {
        CHECK_THROWS_AS(Client_Game_PresentOffscreenSurface(client, 0, {0, std::numeric_limits<int32_t>::max()}, {1, 1}, 0.0f, 0.0f, 0.0f, 0.0f), OverflowException);
    }

    SECTION("IndependentRectangles")
    {
        CHECK_THROWS_AS(Client_Game_PresentOffscreenSurface(client, 0, std::numeric_limits<int32_t>::min(), 0, 1, 1, 0, 0, 1, 1), OverflowException);
    }

    REQUIRE(client->ActiveOffscreenSurfaces.size() == 1);
    CHECK(client->ActiveOffscreenSurfaces.back() == surface);
    CHECK(client->SprMngr.GetRtMngr().GetCurrentRenderTarget() == surface);

    Client_Game_PresentOffscreenSurface(client, 0, {0, 0}, {1, 1});
    CHECK(client->ActiveOffscreenSurfaces.empty());
    client->SprMngr.EndScene();
}

TEST_CASE("ClientResourcesRecoverOutdatedInstalledMetadataFromWritableBase")
{
    string unique_name {strex("lf_client_metadata_mount_{}", std::chrono::steady_clock::now().time_since_epoch().count()).str()};
    string writable_root {MakeTempClientResourceDir("writable_overlay")};
    bool removed_base_before {fs::remove_dir_tree(unique_name)};
    bool removed_writable_before {fs::remove_dir_tree(writable_root)};
    ignore_unused(removed_base_before);
    ignore_unused(removed_writable_before);

    auto cleanup {scope_exit([&unique_name, &writable_root]() noexcept {
        (void)fs::remove_dir_tree(unique_name);
        (void)fs::remove_dir_tree(writable_root);
    })};

    string pack_name {"ClientPack"};
    string metadata_file {"Metadata.fometa-client"};
    string writable_resources {fs::make_writable_path(writable_root, unique_name)};

    STATIC_REQUIRE(METADATA_FILE_VERSION > 1);
    constexpr uint16_t outdated_file_version {METADATA_FILE_VERSION - 1};
    vector<uint8_t> outdated_metadata {};
    data_writer outdated_writer {outdated_metadata};
    outdated_writer.write<uint32_t>(METADATA_FILE_MAGIC);
    outdated_writer.write<uint16_t>(outdated_file_version);
    outdated_writer.write<uint16_t>(numeric_cast<uint16_t>(BakerTests::TEST_METADATA_VERSION.length()));
    outdated_writer.write_string_bytes(BakerTests::TEST_METADATA_VERSION);
    outdated_writer.write<uint16_t>(uint16_t {0});

    vector<uint8_t> current_metadata {BakerTests::MakeEmptyMetadataBlob()};
    WriteClientTestPack(unique_name, pack_name, {{metadata_file, string(outdated_metadata.begin(), outdated_metadata.end())}});
    WriteClientTestPack(writable_resources, pack_name, {{metadata_file, string(current_metadata.begin(), current_metadata.end())}});

    FileSystem install_resources {};
    install_resources.AddCustomSource(safe_alloc::make_unique<ResourcePackSource>(strex(unique_name).combine_path(strex("{}.fores", pack_name)).str()));
    vector<uint8_t> installed_metadata {ReadMetadataBin(&install_resources, "Client")};
    CHECK_THROWS_AS(ReadMetadataVersion(installed_metadata), MetadataOutdatedException);

    GlobalSettings settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Common.Packaged, true);
    BakerTests::OverrideSetting(settings.Common.ResourcesPath, unique_name);
    auto pack_config {ConfigFile(strex("[ResourcePack]\nName = {}\nClientOnly = True\n", pack_name).str())};
    settings.ApplyConfigFile(pack_config, "");
    settings.ApplyWritableRoot(writable_root);

    FileSystem resources {GetClientResources(settings)};
    vector<uint8_t> recovered_metadata {ReadMetadataBin(&resources, "Client")};

    CHECK(ReadMetadataVersion(recovered_metadata) == BakerTests::TEST_METADATA_VERSION);

    EngineMetadata metadata {[] { }};
    metadata.RegisterSide(EngineSideKind::ClientSide);
    CHECK_NOTHROW(RegisterDynamicMetadata(&metadata, recovered_metadata));
}

TEST_CASE("ClientResourceIndexPreservesBootstrapPacksAndWritablePrecedence")
{
    string dir {MakeTempClientResourceDir("merged_index")};
    string install {strex("lf_client_index_{}", std::chrono::steady_clock::now().time_since_epoch().count()).str()};
    auto cleanup {scope_exit([&dir, &install]() noexcept {
        (void)fs::remove_dir_tree(dir);
        (void)fs::remove_dir_tree(install);
    })};

    string writable {strex(dir).combine_path("Writable").str()};
    string overlay {fs::make_writable_path(writable, install)};

    WriteClientTestPack(install, "Before", {{"Shared.txt", "before"}, {"Bootstrap.txt", "bootstrap"}});
    REQUIRE(fs::write_file(strex(install).combine_path("Embedded/Shared.txt").str(), string_view {"embedded"}));
    REQUIRE(fs::write_file(strex(install).combine_path("Embedded/Bootstrap.txt").str(), string_view {"wrong-directory-bootstrap"}));
    WriteClientTestPack(overlay, "Before", {{"Shared.txt", "before"}, {"Bootstrap.txt", "bootstrap"}, {"Overlay.txt", "writable"}});

    {
        ResourcePackWriter writer {strex(install).combine_path("Art.fores").str()};
        string payload {"art"};
        writer.AddFile("Shared.txt", {reinterpret_cast<const uint8_t*>(payload.data()), payload.size()});
        writer.AddFile("Overlay.txt", {reinterpret_cast<const uint8_t*>(payload.data()), payload.size()});
        writer.Finish();
    }

    GlobalSettings settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Common.Packaged, true);
    BakerTests::OverrideSetting(settings.Common.ResourcesPath, install);
    auto pack_config {ConfigFile("[ResourcePack]\nName = Before\nClientOnly = True\n[ResourcePack]\nName = Embedded\nClientOnly = True\n[ResourcePack]\nName = Art\nClientOnly = True\n")};
    settings.ApplyConfigFile(pack_config, "");
    settings.ApplyWritableRoot(writable);

    vector<ResourceIndexPack> packs {};
    vector<string> pack_paths {};
    vector<string> indexed_names {GetResourceIndexPackNames(settings.GetClientResourcePacks())};
    REQUIRE(ResolveResourceIndexPacks({install}, indexed_names, packs, pack_paths));
    string index_path {GetClientResourceIndexPath(settings)};
    REQUIRE(fs::create_directories(strex(index_path).extract_dir().str()));
    BuildResourceIndex(index_path, pack_paths, packs, ResourcePackWriteSettings {0, 100});

    SECTION("MountsTheIndexWithoutDroppingBootstrapOrOverlayFiles")
    {
        FileSystem resources {GetClientResources(settings)};
        CHECK(resources.ReadFileText("Shared.txt") == "art");
        CHECK(resources.ReadFileText("Bootstrap.txt") == "bootstrap");
        CHECK_FALSE(resources.ReadFileHeader("Bootstrap.txt").GetDataSource()->IsDiskDir());
        CHECK(resources.ReadFileText("Overlay.txt") == "art");
        CHECK(resources.ReadFileHeader("Shared.txt").GetDataSource()->GetPackName() == index_path);
    }

    SECTION("DiscardsACorruptDerivedIndexAndMountsThePacks")
    {
        auto original {fs::read_file(index_path)};
        REQUIRE(original.has_value());
        vector<uint8_t> bytes {vector<uint8_t>(original->begin(), original->end())};
        span_write_uint32(bytes, RESOURCE_INDEX_HEADER_SIZE + RESOURCE_INDEX_PACK_SIZE, std::numeric_limits<uint32_t>::max());
        REQUIRE(fs::write_file(index_path, bytes));

        FileSystem resources {GetClientResources(settings)};
        CHECK(resources.ReadFileText("Shared.txt") == "art");
        CHECK(resources.ReadFileText("Bootstrap.txt") == "bootstrap");
        CHECK_FALSE(resources.ReadFileHeader("Bootstrap.txt").GetDataSource()->IsDiskDir());
        CHECK_FALSE(fs::exists(index_path));
    }
}

TEST_CASE("InstalledClientResourcesSelectWritableBaseWithoutOldCatalogLayering")
{
    string unique_name {strex("lf_client_pack_mount_{}", std::chrono::steady_clock::now().time_since_epoch().count()).str()};
    string writable_root {MakeTempClientResourceDir("writable_overlay")};
    bool removed_base_before {fs::remove_dir_tree(unique_name)};
    bool removed_writable_before {fs::remove_dir_tree(writable_root)};
    ignore_unused(removed_base_before);
    ignore_unused(removed_writable_before);

    auto cleanup {scope_exit([&unique_name, &writable_root]() noexcept {
        (void)fs::remove_dir_tree(unique_name);
        (void)fs::remove_dir_tree(writable_root);
    })};

    string writable_resources {fs::make_writable_path(writable_root, unique_name)};
    WriteClientTestPack(unique_name, "Main", {{"shared.txt", "install-base"}, {"base-only.txt", "base-only"}});
    WriteClientTestPack(unique_name, "Fallback", {{"fallback.txt", "base-fallback"}});
    WriteClientTestPack(writable_resources, "Main", {{"shared.txt", "writable-base"}, {"overlay-only.txt", "overlay-only"}});

    GlobalSettings settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Common.Packaged, true);
    BakerTests::OverrideSetting(settings.Common.ResourcesPath, unique_name);
    auto pack_config {ConfigFile("[ResourcePack]\nName = Main\nClientOnly = True\n[ResourcePack]\nName = Fallback\nClientOnly = True\n")};
    settings.ApplyConfigFile(pack_config, "");
    settings.ApplyWritableRoot(writable_root);

    FileSystem resources {GetClientResources(settings)};
    CHECK(resources.ReadFileText("shared.txt") == "writable-base");
    CHECK_FALSE(resources.IsFileExists("base-only.txt"));
    CHECK(resources.ReadFileText("overlay-only.txt") == "overlay-only");
    CHECK(resources.ReadFileText("fallback.txt") == "base-fallback");

    SECTION("MissingBaseDoesNotMountLooseFiles")
    {
        resources.CleanDataSources();
        REQUIRE(fs::remove_file(strex(unique_name).combine_path("Fallback.fores").str()));
        REQUIRE(fs::write_file(strex(unique_name).combine_path("Fallback/fallback.txt").str(), string_view {"loose"}));
        CHECK_THROWS(GetClientResources(settings));
    }
}

#if FO_EFFEKSEER_PARTICLES
TEST_CASE("ParticleSpriteInitialSimulationUsesSceneSpace", "[particles][effekseer][particle-sprites]")
{
    constexpr string_view particle_path {"Particles/SceneBirth.efk"};
    constexpr string_view particle_project {R"EFK(<?xml version="1.0" encoding="utf-8"?>
<EffekseerProject>
  <Root>
    <Name>Root</Name>
    <Children>
      <Node>
        <Name>Stationary</Name>
        <CommonValues>
          <MaxGeneration>
            <Value>1</Value>
            <Infinite>False</Infinite>
          </MaxGeneration>
          <Life>
            <Min>30</Min>
            <Max>30</Max>
          </Life>
          <Generation>
            <GenerationTime>
              <Min>0</Min>
              <Max>0</Max>
            </GenerationTime>
          </Generation>
          <LocationEffectType>1</LocationEffectType>
        </CommonValues>
        <LocationValues>
          <Type>0</Type>
          <Fixed>
            <Location>
              <X>3</X>
              <Y>0</Y>
              <Z>0</Z>
            </Location>
          </Fixed>
        </LocationValues>
        <RendererCommonValues>
          <AlphaBlend>1</AlphaBlend>
          <ZTest>False</ZTest>
          <ZWrite>False</ZWrite>
        </RendererCommonValues>
        <DrawingValues>
          <ColorAll>
            <Fixed>
              <R>255</R>
              <G>255</G>
              <B>255</B>
              <A>255</A>
            </Fixed>
          </ColorAll>
          <Sprite>
            <Billboard>0</Billboard>
          </Sprite>
        </DrawingValues>
        <Children />
      </Node>
    </Children>
  </Root>
  <ToolVersion>1.80.5</ToolVersion>
  <Version>3</Version>
  <StartFrame>0</StartFrame>
  <EndFrame>30</EndFrame>
  <IsLoop>False</IsLoop>
</EffekseerProject>
)EFK"};

    auto project_bytes {const_span<uint8_t> {reinterpret_cast<const uint8_t*>(particle_project.data()), particle_project.size()}};
    EffekseerCompilerOutput particle {CompileEffekseerProject("Particles/SceneBirth.efkproj", project_bytes)};
    REQUIRE(particle.Dependencies.empty());

    AppendEffekseerBoundsTrailer(particle.Binary, vec3 {3.0f, 0.0f, 0.0f}, vec3 {3.0f, 0.0f, 0.0f}, 0.75f);

    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back(string {particle_path}, std::move(particle.Binary));

    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    float32_t scale {GENERATE(1.0f, 2.0f)};
    bool switch_from_atlas {GENERATE(false, true)};
    CAPTURE(scale, switch_from_atlas);

    auto actual {client->ResMngr.LoadSprite(particle_path, AtlasType::MapSprites).dyn_cast<ParticleSprite>()};
    REQUIRE(actual);
    REQUIRE(actual->IsDirectDraw());

    auto reference {client->ResMngr.LoadSprite(particle_path, AtlasType::MapSprites).dyn_cast<ParticleSprite>()};
    REQUIRE(reference);

    auto wrong {client->ResMngr.LoadSprite(particle_path, AtlasType::MapSprites).dyn_cast<ParticleSprite>()};
    REQUIRE(wrong);

    auto atlas {client->ResMngr.LoadSprite(particle_path, AtlasType::MapSprites).dyn_cast<ParticleSprite>()};
    REQUIRE(atlas);

    mat44 projection {1.0f};
    mat44 scene_world {1.0f};

    for (int32_t replay {0}; replay < 2; replay++) {
        CAPTURE(replay);

        if (switch_from_atlas) {
            actual->SetDrawInScene(false);
            actual->SetDrawInScene(true);
        }

        reference->GetParticle()->Setup(projection, scene_world, {}, 0.0f, {}, true);
        wrong->GetParticle()->Setup(projection, glm::translate(scene_world, vec3 {8.0f, 0.0f, 0.0f}), {}, 0.0f, {}, true);

        atlas->SetDrawInScene(true);
        atlas->SetDrawInScene(false);

        for (auto sprite : {actual, reference, wrong, atlas}) {
            auto system {sprite->GetParticle()};
            system->SetScale(scale);

            bool played {sprite->PlayWithSeed(1337)};
            REQUIRE(played);

            system->Update(timespan {std::chrono::nanoseconds {16'666'667}});
            system->Setup(projection, scene_world, {}, 0.0f, {}, true);
            system->RefreshRenderTransform();
        }

        auto actual_bounds {actual->GetParticle()->GetLiveBounds()};
        auto reference_bounds {reference->GetParticle()->GetLiveBounds()};
        auto wrong_bounds {wrong->GetParticle()->GetLiveBounds()};
        REQUIRE(actual_bounds);
        REQUIRE(reference_bounds);
        REQUIRE(wrong_bounds);

        auto atlas_bounds {atlas->GetParticle()->GetLiveBounds()};
        REQUIRE(atlas_bounds);

        CAPTURE(actual_bounds->PositionMin.x, actual_bounds->PositionMax.x, reference_bounds->PositionMin.x, reference_bounds->PositionMax.x, wrong_bounds->PositionMin.x, wrong_bounds->PositionMax.x);
        REQUIRE(wrong_bounds->PositionMax.x > reference_bounds->PositionMax.x + 1.0f);
        CHECK(atlas_bounds->PositionMin.x < reference_bounds->PositionMin.x - 1.0f);

        for (int32_t axis {0}; axis < 3; axis++) {
            CAPTURE(axis);
            CHECK(is_float_equal(actual_bounds->PositionMin[axis], reference_bounds->PositionMin[axis]));
            CHECK(is_float_equal(actual_bounds->PositionMax[axis], reference_bounds->PositionMax[axis]));
        }
    }
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ClientEngineRejectsMultiFrameModelTextures")
{
    constexpr string_view model_path {"Models/MultiFrameTexture.fo3d"};
    constexpr string_view mesh_path {"Models/MultiFrameTexture.fbx"};
    vector<pair<string, vector<uint8_t>>> resources {{"Models/Cycle.fofrm", BakerTests::MakeMinimalBakedSprite()}};
    auto mesh {MakeRuntimeModelTriangleMesh()};
    auto description {MakeRuntimeModelDescription(model_path, mesh_path, mesh, "Texture 0 Cycle.fofrm\n", {}, nullptr, false, resources)};

    // The baker saw a one-frame resource, so the description is valid; only the mounted bytes become multi-frame
    resources.front().second = BakerTests::MakeMultiFrameBakedSprite(2, 1, 1, 200);
    resources.emplace_back(string {mesh_path}, std::move(mesh));
    resources.emplace_back(string {model_path}, std::move(description));
    resources.emplace_back("ModelAnimationInfo.foinfo", MakeUnitTestModelAnimationInfo(model_path));
    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);
    REQUIRE_THROWS_WITH(factory->GetModelMngr()->CreateModel(model_path), Catch::Matchers::ContainsSubstring("Image resource must contain exactly one frame"));
}

TEST_CASE("ClientEngineOverridesConsumedImportedDiffuse")
{
#if FO_ENABLE_3D
    constexpr string_view model_path {"Models/EffectiveTexture.fo3d"};
    constexpr string_view mesh_path {"Models/EffectiveTexture.fbx"};
    auto mesh {MakeRuntimeModelTriangleMesh({}, "Consumed.png")};
    vector<pair<string, vector<uint8_t>>> resources {{"Models/Effective.fofrm", BakerTests::MakeMinimalBakedSprite()}};
    auto description {MakeRuntimeModelDescription(model_path, mesh_path, mesh, "Texture 0 Effective.fofrm\n", {}, nullptr, false, resources)};
    resources.emplace_back(string {mesh_path}, std::move(mesh));
    resources.emplace_back(string {model_path}, std::move(description));
    resources.emplace_back("ModelAnimationInfo.foinfo", MakeUnitTestModelAnimationInfo(model_path));
    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);
    auto model {factory->GetModelMngr()->CreateModel(model_path)};
    REQUIRE(model);
    model->StartMeshGeneration();
    REQUIRE(model->GetMeshTexture(0, 0));
    CHECK(model->GetMeshTexture(0, 0)->MainTex->Size.width > 0);
#endif
}

TEST_CASE("ClientEngineLoadsLargeFofrmModelTextures")
{
#if FO_ENABLE_3D
    constexpr string_view model_path {"Models/LargeTexture.fo3d"};
    constexpr string_view mesh_path {"Models/LargeTexture.fbx"};
    auto mesh {MakeRuntimeModelTriangleMesh()};
    vector<pair<string, vector<uint8_t>>> resources {{"Models/Large.fofrm", BakerTests::MakeMinimalBakedSprite(1024, 1024)}};
    auto description {MakeRuntimeModelDescription(model_path, mesh_path, mesh, "Texture 0 Large.fofrm\n", {}, nullptr, false, resources)};
    resources.emplace_back(string {mesh_path}, std::move(mesh));
    resources.emplace_back(string {model_path}, std::move(description));
    resources.emplace_back("ModelAnimationInfo.foinfo", MakeUnitTestModelAnimationInfo(model_path));
    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);
    auto model {factory->GetModelMngr()->CreateModel(model_path)};
    REQUIRE(model);
    model->StartMeshGeneration();
    auto texture {model->GetMeshTexture(0, 0)};
    REQUIRE(texture);
    CHECK(texture->MainTex->Size.width >= 1026);
    CHECK(texture->MainTex->Size.height >= 1026);
#endif
}

TEST_CASE("ClientEngineRetriesMissingImportedDiffuse")
{
#if FO_ENABLE_3D
    constexpr string_view model_path {"Models/MissingTexture.fo3d"};
    constexpr string_view mesh_path {"Models/MissingTexture.fbx"};
    auto mesh {MakeRuntimeModelTriangleMesh({}, "Missing.png")};
    vector<pair<string, vector<uint8_t>>> bake_resources {{"Models/Missing.png", BakerTests::MakeMinimalBakedSprite()}};
    auto description {MakeRuntimeModelDescription(model_path, mesh_path, mesh, "", {}, nullptr, false, bake_resources)};
    vector<pair<string, vector<uint8_t>>> resources {{string {mesh_path}, std::move(mesh)}, {string {model_path}, std::move(description)}, {"ModelAnimationInfo.foinfo", MakeUnitTestModelAnimationInfo(model_path)}};
    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);
    auto model {factory->GetModelMngr()->CreateModel(model_path)};
    REQUIRE(model);
    REQUIRE_THROWS_WITH(model->StartMeshGeneration(), Catch::Matchers::ContainsSubstring("Model texture could not be loaded"));
    REQUIRE_THROWS_WITH(model->StartMeshGeneration(), Catch::Matchers::ContainsSubstring("Model texture could not be loaded"));
#endif
}

TEST_CASE("ClientEngineLoadsModelMeshBakerOutputThroughRuntimeParser")
{
    constexpr string_view model_path {"Models/RuntimeParserTriangle.obj"};

    BakerTests::TestRig rig {};
    rig.AddSourceFile(model_path, R"(o RuntimeParserTriangle
v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)");

    ModelMeshBaker baker {rig.MakeContext()};
    baker.BakeFiles(rig.GetAllSourceFiles(), model_path);

    auto output_it {rig.Outputs.find(string(model_path))};
    REQUIRE(output_it != rig.Outputs.end());

    auto resources {MakeClientTestResources({{string {model_path}, output_it->second}})};
    REQUIRE(resources.IsFileExists(model_path));

    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, std::move(resources))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);
    CHECK_NOTHROW(factory->GetModelMngr()->PreloadModel(model_path));
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ClientEngineRejectsMalformedBakedModelCountsAndBounds")
{
    static_assert(MODEL_DESCRIPTION_SCHEMA_VERSION == 5);

    vector<pair<string, vector<uint8_t>>> malformed_resources {};

    malformed_resources.emplace_back("Models/VertexCountBomb.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.write<uint32_t>(std::numeric_limits<uint32_t>::max());
    }));

    malformed_resources.emplace_back("Models/IndexCountBomb.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(std::numeric_limits<uint32_t>::max());
    }));

    malformed_resources.emplace_back("Models/IndexOutOfBounds.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        array<Vertex3D, 1> vertices {};
        writer.write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
        writer.write_object_array(const_span<Vertex3D> {vertices});
        array<vindex_t, 1> indices {vindex_t {1}};
        writer.write<uint32_t>(numeric_cast<uint32_t>(indices.size()));
        writer.write_object_array(const_span<vindex_t> {indices});
        writer.write_string({});
        writer.write<uint32_t>(uint32_t {1});
        writer.write_string({});
        writer.write<uint32_t>(uint32_t {1});
        writer.write<mat44>(mat44 {1.0f});
        writer.write<uint32_t>(uint32_t {0});
    }));

    malformed_resources.emplace_back("Models/SkinCountBomb.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write_string({});
        writer.write<uint32_t>(numeric_cast<uint32_t>(MODEL_MAX_BONES + 1));
    }));

    malformed_resources.emplace_back("Models/SkinOffsetMismatch.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write_string({});
        writer.write<uint32_t>(uint32_t {1});
        writer.write_string({});
        writer.write<uint32_t>(uint32_t {0});
    }));

    Vertex3D valid_skin_vertex {};
    valid_skin_vertex.BlendWeights[0] = 1.0f;

    Vertex3D non_finite_skin_weight {valid_skin_vertex};
    non_finite_skin_weight.BlendWeights[0] = std::numeric_limits<float32_t>::quiet_NaN();
    malformed_resources.emplace_back("Models/NonFiniteSkinWeight.fbx", MakeRuntimeModelMeshWithVertex(non_finite_skin_weight));

    Vertex3D out_of_range_skin_weight {valid_skin_vertex};
    out_of_range_skin_weight.BlendWeights[0] = -0.25f;
    malformed_resources.emplace_back("Models/OutOfRangeSkinWeight.fbx", MakeRuntimeModelMeshWithVertex(out_of_range_skin_weight));

    Vertex3D non_finite_skin_index {valid_skin_vertex};
    non_finite_skin_index.BlendIndices[0] = std::numeric_limits<float32_t>::infinity();
    malformed_resources.emplace_back("Models/NonFiniteSkinIndex.fbx", MakeRuntimeModelMeshWithVertex(non_finite_skin_index));

    Vertex3D non_integral_skin_index {valid_skin_vertex};
    non_integral_skin_index.BlendIndices[0] = 0.5f;
    malformed_resources.emplace_back("Models/NonIntegralSkinIndex.fbx", MakeRuntimeModelMeshWithVertex(non_integral_skin_index));

    Vertex3D out_of_range_skin_index {valid_skin_vertex};
    out_of_range_skin_index.BlendIndices[0] = 1.0f;
    malformed_resources.emplace_back("Models/OutOfRangeSkinIndex.fbx", MakeRuntimeModelMeshWithVertex(out_of_range_skin_index));

    Vertex3D invalid_skin_weight_sum {valid_skin_vertex};
    invalid_skin_weight_sum.BlendWeights[0] = 0.5f;
    malformed_resources.emplace_back("Models/InvalidSkinWeightSum.fbx", MakeRuntimeModelMeshWithVertex(invalid_skin_weight_sum));

    malformed_resources.emplace_back("Models/ChildCountBomb.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", false);
        writer.write<uint32_t>(std::numeric_limits<uint32_t>::max());
    }));

    malformed_resources.emplace_back("Models/HierarchyDepthBomb.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        for (uint32_t depth {0}; depth <= MODEL_MESH_MAX_HIERARCHY_DEPTH; depth++) {
            WriteRuntimeModelBoneHeader(writer, "Bone", false);
            writer.write<uint32_t>(depth < MODEL_MESH_MAX_HIERARCHY_DEPTH ? uint32_t {1} : uint32_t {0});
        }
    }));

    {
        FO_TEMPORARY_COMPAT(test_model_description_schema4, "2026-12-31");

        vector<uint8_t> data {};
        data_writer writer {data};
        writer.write_bytes({MODEL_DESCRIPTION_MAGIC.data(), MODEL_DESCRIPTION_MAGIC.size()});
        writer.write<uint16_t>(uint16_t {4});
        writer.write<uint16_t>(MODEL_DESCRIPTION_SUPPORTED_FLAGS);
        malformed_resources.emplace_back("Models/DescriptionOldSchema.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data {};
        data_writer writer {data};
        writer.write_bytes({MODEL_DESCRIPTION_MAGIC.data(), MODEL_DESCRIPTION_MAGIC.size()});
        writer.write<uint16_t>(MODEL_DESCRIPTION_SCHEMA_VERSION);
        writer.write<uint16_t>(MODEL_DESCRIPTION_SUPPORTED_FLAGS);
        writer.write<uint32_t>(std::numeric_limits<uint32_t>::max());
        malformed_resources.emplace_back("Models/DescriptionStringBomb.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data {};
        data_writer writer {data};
        WriteRuntimeModelDescriptionPrefix(writer);
        WriteRuntimeModelDescriptionLink(writer);
        writer.write<uint32_t>(std::numeric_limits<uint32_t>::max());
        malformed_resources.emplace_back("Models/DescriptionLinksBomb.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data {};
        data_writer writer {data};
        WriteRuntimeModelDescriptionPrefix(writer);
        WriteRuntimeModelDescriptionLinkPrefix(writer);
        writer.write<uint32_t>(std::numeric_limits<uint32_t>::max());
        malformed_resources.emplace_back("Models/DescriptionNestedCountBomb.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data {};
        data_writer writer {data};
        WriteRuntimeModelDescriptionPrefix(writer);
        WriteRuntimeModelDescriptionLinkPrefix(writer);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint8_t>(uint8_t {1});
        malformed_resources.emplace_back("Models/DescriptionGeometryFlagMismatch.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data {};
        data_writer writer {data};
        WriteRuntimeModelDescriptionPrefix(writer);
        WriteRuntimeModelDescriptionLinkPrefix(writer);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint8_t>(uint8_t {2});
        malformed_resources.emplace_back("Models/DescriptionGeometryFlagOutOfRange.fo3d", std::move(data));
    }

    array<pair<string_view, string_view>, 19> expected_failures {{
        {"Models/VertexCountBomb.fbx", "vertex count exceeds maximum addressable count"},
        {"Models/IndexCountBomb.fbx", "mesh indices"},
        {"Models/IndexOutOfBounds.fbx", "outside vertex count"},
        {"Models/SkinCountBomb.fbx", "skin bone count exceeds maximum"},
        {"Models/SkinOffsetMismatch.fbx", "skin bone offset count mismatch"},
        {"Models/NonFiniteSkinWeight.fbx", "non-finite skin weight"},
        {"Models/OutOfRangeSkinWeight.fbx", "skin weight outside [0, 1]"},
        {"Models/NonFiniteSkinIndex.fbx", "non-finite skin index"},
        {"Models/NonIntegralSkinIndex.fbx", "non-integral skin index"},
        {"Models/OutOfRangeSkinIndex.fbx", "skin index outside valid range"},
        {"Models/InvalidSkinWeightSum.fbx", "skin-weight sum"},
        {"Models/ChildCountBomb.fbx", "child count exceeds maximum"},
        {"Models/HierarchyDepthBomb.fbx", "hierarchy depth"},
        {"Models/DescriptionOldSchema.fo3d", "Unsupported baked model description schema"},
        {"Models/DescriptionStringBomb.fo3d", "String length exceeds remaining buffer"},
        {"Models/DescriptionLinksBomb.fo3d", "links"},
        {"Models/DescriptionNestedCountBomb.fo3d", "disabled meshes"},
        {"Models/DescriptionGeometryFlagMismatch.fo3d", "geometry flag does not match"},
        {"Models/DescriptionGeometryFlagOutOfRange.fo3d", "geometry flag is not 0 or 1"},
    }};

    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(malformed_resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);
    auto model_mngr {factory->GetModelMngr()};

    for (const auto& [model_path, expected_failure] : expected_failures) {
        INFO(model_path);
        CHECK_THROWS_WITH(model_mngr->PreloadModel(model_path), Catch::Matchers::ContainsSubstring(std::string {expected_failure}));
    }
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ModelSpriteBoundsFollowEveryStateChangeThatMovesTheEnvelope")
{
    // Pinned differentially against a fresh instance rather than hardcoded rectangles, because the frame is derived
    // through per-instance state that can go stale, and a stale rectangle clips the model with nothing logged
    constexpr string_view model_path {"Models/SkinnedSpriteBounds.fbx"};

    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources({{string {model_path}, MakeSkinnedRuntimeModelMesh()}}))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);
    auto model_mngr {factory->GetModelMngr()};

    struct SpriteBoundsStep
    {
        string_view Name {};
        void (*Apply)(ptr<ModelInstance>) {};
    };

    vector<SpriteBoundsStep> steps {
        {"rest pose", [](ptr<ModelInstance>) {}},
        {"scaled up", [](ptr<ModelInstance> model) { model->SetScale(1.7f, 1.7f, 1.7f); }},
        {"camera tilt", [](ptr<ModelInstance> model) { model->SetRotation(0.4f, 0.0f, 0.25f); }},
        {"turned", [](ptr<ModelInstance> model) { model->SetDir(mdir {120}, false); }},
        {"shadow off", [](ptr<ModelInstance> model) { model->EnableShadow(false); }},
        {"scaled back down", [](ptr<ModelInstance> model) { model->SetScale(0.6f, 0.6f, 0.6f); }},
    };

    // model_path is a constexpr string_view, but passing it by value and by reference odr-uses it, so it
    // has to be captured rather than relied on as a constant expression
    auto make_model {[&model_mngr, model_path]() {
        auto model {model_mngr->CreateModel(model_path)};
        FO_VERIFY_AND_THROW(model, "Skinned test model was not created", model_path);
        auto owned {model.take_not_null()};

        owned->StartMeshGeneration();
        owned->PrepareFrameLayout();
        owned->SetupFrame(owned->GetDrawSize(), owned->GetFramePivot());
        return owned;
    }};

    auto measure {[](ptr<ModelInstance> model) -> optional<ModelSpriteBounds> {
        model->PoseSpriteFrame(false);
        return model->GetSpriteBounds();
    }};

    auto same_bounds {[](const ModelSpriteBounds& first, const ModelSpriteBounds& second) { //
        return first.Rect == second.Rect && first.RequiredFrameSize == second.RequiredFrameSize && first.Pivot == second.Pivot;
    }};

    auto warm_model {make_model()};

    // The hit path on its own: measuring twice without touching anything must reproduce the first answer exactly
    optional<ModelSpriteBounds> first_bounds {measure(warm_model.as_ptr())};
    optional<ModelSpriteBounds> repeated_bounds {measure(warm_model.as_ptr())};

    REQUIRE(first_bounds);
    REQUIRE(repeated_bounds);
    CHECK(same_bounds(*first_bounds, *repeated_bounds));

    optional<ModelSpriteBounds> previous_bounds {};
    size_t moved_steps {0};

    for (size_t step_index {0}; step_index < steps.size(); step_index++) {
        INFO("step " << step_index << " (" << steps[step_index].Name << ")");

        steps[step_index].Apply(warm_model.as_ptr());
        optional<ModelSpriteBounds> warm_bounds {measure(warm_model.as_ptr())};
        auto cold_model {make_model()};

        for (size_t applied_index {0}; applied_index <= step_index; applied_index++) {
            steps[applied_index].Apply(cold_model.as_ptr());
        }

        optional<ModelSpriteBounds> cold_bounds {measure(cold_model.as_ptr())};

        REQUIRE(warm_bounds);
        REQUIRE(cold_bounds);
        CHECK(same_bounds(*warm_bounds, *cold_bounds));

        if (previous_bounds && !same_bounds(*cold_bounds, *previous_bounds)) {
            moved_steps++;
        }

        previous_bounds = cold_bounds;
    }

    // Guard the guard: a step that leaves the envelope where it was cannot tell a stale cache from a correct one, so
    // the sequence above only tests anything as long as it keeps moving the rectangle
    CHECK(moved_steps + 1 >= steps.size() - 1);
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ModelDefaultLinkDisablesItsOwnMeshes")
{
    // Baked model and animation bounds are calculated with the default link's disabled meshes excluded, so the
    // runtime model state must agree with the geometry the bake budgeted for
    constexpr string_view MESH_PATH {"Models/DefaultLinkDisabled.fbx"};
    constexpr string_view MODEL_PATH {"Models/DefaultLinkDisabled.fo3d"};

    // Far outside the declared bounds; the test is about applying the authored disable state, not measuring vertices
    vector<uint8_t> mesh_blob {MakeRuntimeModelTriangleMesh(vec3 {12.0f, 12.0f, 0.0f})};

    vector<pair<string, vector<uint8_t>>> model_resources {};
    model_resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    model_resources.emplace_back(string {MESH_PATH}, mesh_blob);
    model_resources.emplace_back(string {MODEL_PATH}, MakeRuntimeModelDescription(MODEL_PATH, MESH_PATH, mesh_blob, "DisableMesh All\n"));

    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(model_resources)))};

    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);

    auto model_mngr {factory->GetModelMngr()};

    REQUIRE_NOTHROW(model_mngr->PreloadModel(MODEL_PATH));

    auto model {model_mngr->CreateModel(MODEL_PATH)};
    REQUIRE(static_cast<bool>(model));

    // Creation leaves the root layout pending: _parent is assigned only after CreateModel returns, and an
    // attachment laid out as a root measures the joint it hangs from plus its shadow, past any frame it can hold
    CHECK(model->GetDrawSize() == isize32 {4, 4});

    model->StartMeshGeneration();
    model->PrepareFrameLayout();

    // Guard the fixture: the layout must come from the declared +/-1 bounds, or the far triangle is already
    // inside the frame and the check below would pass without proving anything
    isize32 layout_size {model->GetDrawSize()};
    int32_t bounds_span_limit {iround<int32_t>(6.0f * client->Settings->Render.ModelProjFactor)};
    REQUIRE(layout_size.width <= bounds_span_limit);
    REQUIRE(layout_size.height <= bounds_span_limit);

    model->SetupFrame(layout_size, model->GetFramePivot());
    model->PoseSpriteFrame(true);

    auto bounds {model->GetSpriteBounds()};
    REQUIRE(bounds);
    CHECK(bounds->RequiredFrameSize == layout_size);
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ModelSpriteBoundsUseBakedBoundsForTheActiveGeometryLink")
{
    constexpr string_view BASE_MESH_PATH {"Models/BakedLinkBase.fbx"};
    constexpr string_view LINK_MESH_PATH {"Models/BakedLinkEquipment.fbx"};
    constexpr string_view MODEL_PATH {"Models/BakedLink.fo3d"};
    vector<uint8_t> base_mesh_blob {MakeSkinnedRuntimeModelMesh()};
    vector<uint8_t> link_mesh_blob {MakeSkinnedRuntimeModelMesh()};
    vector<uint8_t> model_blob {MakeRuntimeModelDescription(MODEL_PATH, BASE_MESH_PATH, base_mesh_blob, "Layer 3\n Value 1 Attach BakedLinkEquipment.fbx MoveX 3\n", LINK_MESH_PATH, &link_mesh_blob, true)};
    vector<pair<string, vector<uint8_t>>> model_resources {};
    model_resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    model_resources.emplace_back(string {BASE_MESH_PATH}, base_mesh_blob);
    model_resources.emplace_back(string {LINK_MESH_PATH}, link_mesh_blob);
    model_resources.emplace_back(string {MODEL_PATH}, std::move(model_blob));

    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(model_resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);
    auto model {factory->GetModelMngr()->CreateModel(MODEL_PATH)};
    REQUIRE(static_cast<bool>(model));

    model->StartMeshGeneration();
    model->PrepareFrameLayout();
    isize32 base_size {model->GetDrawSize()};
    array<int32_t, MODEL_LAYERS_COUNT> layers {};
    layers[3] = 1;
    REQUIRE(model->PlayAnim(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), layers.data(), 0.0f, ModelAnimFlags::None));
    isize32 equipped_size {model->GetDrawSize()};
    CHECK(equipped_size.width > base_size.width);

    model->SetupFrame(equipped_size, model->GetFramePivot());
    model->PoseSpriteFrame(true);
    optional<ModelSpriteBounds> bounds {model->GetSpriteBounds()};
    REQUIRE(bounds);
    CHECK(bounds->RequiredFrameSize == equipped_size);

    layers[3] = 0;
    REQUIRE(model->PlayAnim(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), layers.data(), 0.0f, ModelAnimFlags::None));
    isize32 unequipped_size {model->GetDrawSize()};
    CHECK(unequipped_size.width < equipped_size.width);
    CHECK(unequipped_size.height < equipped_size.height);
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("BoneMapAnchorPreservesTheCarrierAndModelDepth")
{
    constexpr string_view MESH_PATH {"Models/BoneAnchor.fbx"};
    constexpr string_view MODEL_PATH {"Models/RuntimeInstance.fo3d"};
    constexpr string_view DESCRIPTION {"Model BoneAnchor.fbx\nScale 1.25\nAnim 1 1 BoneAnchor.fbx Base\n"};
    auto mesh {MakeRuntimeModelTriangleMesh()};
    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    resources.emplace_back(string {MESH_PATH}, mesh);
    resources.emplace_back(string {MODEL_PATH}, MakeRuntimeModelDescription(MODEL_PATH, MESH_PATH, mesh, {}, {}, nullptr, false, {}, 0.0f, nullptr, DESCRIPTION));
    resources.emplace_back("AnchorSprite.png", BakerTests::MakeMinimalBakedSprite(8, 8));
    auto settings {MakeClientTestSettings()};
    bool direct_draw {GENERATE(false, true)};
    CAPTURE(direct_draw);
    BakerTests::OverrideSetting(settings.Render.ModelDirectDraw, direct_draw);
    BakerTests::OverrideSetting(settings.Render.ModelProjFactor, 32.0f);
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    ProtoMap proto {client->Hashes.to_hashed_string("BoneAnchorMap"), client->GetPropertyRegistrar(MapView::ENTITY_TYPE_NAME)};
    proto.SetSize(msize {16, 16});
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, make_ptr(&proto), isize32 {320, 200}, nullptr)};
    auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
    map->EnableMapperMode();
    map->InstantScrollTo(mpos {8, 8});
    auto cr {map->AddMapperCritter(client->Hashes.to_hashed_string("UnitTestModelCritter"), mpos {8, 8}, mdir {int16_t {180}}, nullptr)};
    REQUIRE(cr->IsMapSpriteValid());
    auto model {cr->GetModel()};
    REQUIRE(model);
    cr->SetHexOffset(ipos16 {13, -7});
    cr->RefreshOffs();
    model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
    hstring bone {client->Hashes.to_hashed_string(MESH_PATH)};
    float32_t angle {GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT};

    SECTION("AHolderReconstructsThePhysicalPointAtEachElevationAndFacing")
    {
        MapSpriteHolder holder {};
        holder.SprId = client->AnimLoad(client->Hashes.to_hashed_string("AnchorSprite.png"), AtlasType::MapSprites);
        holder.Hex = cr->GetHex();
        REQUIRE(holder.SprId != 0);
        auto stop_holder {scope_exit([&holder]() noexcept { holder.StopDraw(); })};
        vec3 local {0.7f, 0.4f, 0.3f};

        for (int16_t elevation : {int16_t {0}, int16_t {18}, int16_t {-12}}) {
            cr->SetElevation(elevation);
            cr->RefreshSprite();

            for (int16_t direction : {int16_t {180}, int16_t {90}, int16_t {0}}) {
                model->SetDir(mdir {direction}, false);
                model->PoseSpriteFrame(false);
                float32_t yaw {numeric_cast<float32_t>(180 - direction) * DEG_TO_RAD_FLOAT};
                vec3 local_world {
                    (std::cos(yaw) * local.x + std::sin(yaw) * local.z) * 40.0f,
                    local.y * 40.0f,
                    (-std::sin(yaw) * local.x + std::cos(yaw) * local.z) * 40.0f,
                };
                ipos32 sprite_offset {cr->GetSpriteOffset()};
                ipos32 center {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
                vec3 carrier_world {GeometryHelper::GetHexWorldPos(cr->GetHex(), center + sprite_offset, numeric_cast<float32_t>(elevation))};
                vec3 expected {GeometryHelper::ProjectWorldToMap(carrier_world + local_world)};
                ipos32 offset {};
                float32_t depth {0.0f};
                REQUIRE(Client_Critter_GetBoneMapAnchor(cr, bone, local.x, local.y, local.z, offset, depth));
                vec3 local_screen {GeometryHelper::ProjectWorldToMap(local_world)};
                CHECK(offset.x == sprite_offset.x + iround<int32_t>(local_screen.x));
                CHECK(offset.y == sprite_offset.y + iround<int32_t>(local_screen.y) - iround<int32_t>(std::cos(angle) * elevation));
                holder.StopDraw();
                holder.Offset = offset;
                holder.DepthOffset = depth;
                Client_Map_DrawMapSprite(map, &holder);
                REQUIRE(holder.Valid);
                REQUIRE(holder.MSpr);
                CHECK(is_float_equal(holder.MSpr->GetMapRootProjection().z, expected.z));
            }
        }
    }

    SECTION("IndependentSpritesFollowTheSubmittedCarrierDepth")
    {
        cr->RestoreAlpha();
        MapSpriteHolder holder {};
        holder.SprId = client->AnimLoad(client->Hashes.to_hashed_string("AnchorSprite.png"), AtlasType::MapSprites);
        holder.Hex = cr->GetHex();
        REQUIRE(holder.SprId != 0);
        auto stop_holder {scope_exit([&holder]() noexcept { holder.StopDraw(); })};
        vec3 local {0.7f, 0.4f, 0.3f};
        RecordingQuadEffect effect {};

        for (ipos16 movement : {ipos16 {13, -7}, ipos16 {-18, 11}}) {
            cr->SetHexOffset(movement);
            cr->RefreshOffs();

            for (int16_t elevation : {int16_t {0}, int16_t {18}, int16_t {-12}}) {
                cr->SetElevation(elevation);
                cr->RefreshSprite();

                for (int16_t direction : {int16_t {180}, int16_t {90}, int16_t {0}}) {
                    model->SetDir(mdir {direction}, false);
                    model->PoseSpriteFrame(false);

                    client->SprMngr.BeginScene();
                    client->SprMngr.EndScene();

                    ipos32 offset {};
                    float32_t depth {0.0f};
                    REQUIRE(Client_Critter_GetBoneSpriteMapAnchor(cr, bone, local.x, local.y, local.z, offset, depth));

                    ipos32 physical_offset {};
                    float32_t physical_depth {0.0f};
                    REQUIRE(Client_Critter_GetBoneMapAnchor(cr, bone, local.x, local.y, local.z, physical_offset, physical_depth));
                    CHECK(offset == physical_offset);

                    holder.StopDraw();
                    holder.Offset = offset;
                    holder.DepthOffset = depth;
                    Client_Map_DrawMapSprite(map, &holder);
                    REQUIRE(holder.Valid);
                    REQUIRE(holder.MSpr);

                    if (direct_draw) {
                        CHECK(is_float_equal(depth, physical_depth));
                        continue;
                    }

                    auto carrier {cr->GetMapSprite()};
                    auto local_point {model->GetBonePos(bone, local)};
                    REQUIRE(local_point);
                    ipos32 field_offset {320, 240};
                    ipos32 root_offset {carrier->GetRootOffset()};
                    MapSpriteList carriers {};
                    auto submitted {carriers.AddSprite(DrawOrderType::Critter, cr->GetHex(), carrier->GetHexOffset(), &field_offset, carrier->GetSprite(), nullptr, carrier->GetPSprOffset(), &root_offset, nullptr, nullptr, nullptr, 0)};
                    submitted->SetElevation(elevation);
                    submitted->SetDepthOffset(carrier->GetDepthOffset());
                    CHECK(submitted->GetSpriteRootOffset() == carrier->GetSpriteRootOffset());

                    effect.Draws.clear();
                    client->SprMngr.DrawSprites(carriers, irect32 {0, 0, 1280, 960}, 0, false, DrawOrderType::Critter, DrawOrderType::Critter, ucolor {255, 255, 255}, &effect);
                    REQUIRE(effect.Draws.size() == 1);

                    vector<Vertex2D> vertices {ReadRecordedLayerQuad(effect.Draws.front())};
                    auto top {std::ranges::min_element(vertices, {}, &Vertex2D::PosY)};
                    auto bottom {std::ranges::max_element(vertices, {}, &Vertex2D::PosY)};
                    REQUIRE(top->PosY < bottom->PosY);

                    // The submitted vertices are the oracle, independent of the anchor helper's projection formula
                    ipos32 point {submitted->GetDrawRootPos() + *local_point};
                    float32_t fraction {(numeric_cast<float32_t>(point.y) - top->PosY) / (bottom->PosY - top->PosY)};
                    float32_t expected_depth {top->PosZ + fraction * (bottom->PosZ - top->PosZ)};
                    CHECK(std::abs(holder.MSpr->GetMapRootProjection().z - expected_depth) < 0.002f);
                    CHECK(std::abs(physical_depth - depth) > 1.0f);
                    CHECK(holder.MSpr->GetDrawRootPos() - *holder.MSpr->GetPHexOffset() == point - field_offset);
                }
            }
        }
    }

    SECTION("HiddenSpritesHaveNoRenderedAnchor")
    {
        cr->RestoreAlpha();
        model->PoseSpriteFrame(false);

        client->SprMngr.BeginScene();
        client->SprMngr.EndScene();

        ipos32 offset {};
        float32_t depth {0.0f};
        REQUIRE(Client_Critter_GetBoneSpriteMapAnchor(cr, bone, 0.0f, 0.0f, 0.0f, offset, depth));

        cr->GetMapSprite()->SetHidden(true);
        offset = ipos32 {123, 456};
        depth = 123.0f;
        CHECK_FALSE(Client_Critter_GetBoneSpriteMapAnchor(cr, bone, 0.0f, 0.0f, 0.0f, offset, depth));
        CHECK(offset == cr->GetSpriteOffset());
        CHECK(depth == 0.0f);
    }

    SECTION("RenderedInvalidAndDetachedPointsLeaveDeterministicOutputs")
    {
        cr->RestoreAlpha();
        model->PoseSpriteFrame(false);

        client->SprMngr.BeginScene();
        client->SprMngr.EndScene();

        ipos32 offset {};
        float32_t depth {0.0f};
        REQUIRE(Client_Critter_GetBoneSpriteMapAnchor(cr, bone, 0.0f, 0.0f, 0.0f, offset, depth));

        offset = ipos32 {123, 456};
        depth = 123.0f;
        CHECK_FALSE(Client_Critter_GetBoneSpriteMapAnchor(cr, client->Hashes.to_hashed_string("MissingBone"), 0.0f, 0.0f, 0.0f, offset, depth));
        CHECK(offset == cr->GetSpriteOffset());
        CHECK(depth == 0.0f);

        for (float32_t invalid : {std::numeric_limits<float32_t>::quiet_NaN(), std::numeric_limits<float32_t>::infinity(), std::numeric_limits<float32_t>::max()}) {
            offset = ipos32 {123, 456};
            depth = 123.0f;
            CHECK_FALSE(Client_Critter_GetBoneSpriteMapAnchor(cr, bone, invalid, 0.0f, 0.0f, offset, depth));
            CHECK(offset == cr->GetSpriteOffset());
            CHECK(depth == 0.0f);
        }

        cr->InvalidateSprite();
        depth = 123.0f;
        CHECK_FALSE(Client_Critter_GetBoneSpriteMapAnchor(cr, bone, 0.0f, 0.0f, 0.0f, offset, depth));
        CHECK(offset == cr->GetSpriteOffset());
        CHECK(depth == 0.0f);

        auto flat {map->AddMapperCritter(client->Hashes.to_hashed_string("UnitTestClientCritter"), mpos {9, 8}, mdir {}, nullptr)};
        depth = 123.0f;
        CHECK_FALSE(Client_Critter_GetBoneSpriteMapAnchor(flat, bone, 0.0f, 0.0f, 0.0f, offset, depth));
        CHECK(offset == flat->GetSpriteOffset());
        CHECK(depth == 0.0f);

        auto off_map {safe_alloc::make_refcounted<CritterView>(client, ident_t {2002}, client->GetProtoCritter(client->Hashes.to_hashed_string("UnitTestClientCritter")), nullptr)};
        auto destroy_off_map {scope_exit([&off_map]() noexcept { safe_call([&off_map] { off_map->DestroySelf(); }); })};
        offset = ipos32 {123, 456};
        depth = 123.0f;
        CHECK_THROWS_AS(Client_Critter_GetBoneSpriteMapAnchor(off_map, bone, 0.0f, 0.0f, 0.0f, offset, depth), ScriptException);
        CHECK(offset == ipos32 {123, 456});
        CHECK(depth == 123.0f);
    }

    SECTION("InvalidAndDetachedPointsLeaveDeterministicOutputs")
    {
        model->PoseSpriteFrame(false);
        ipos32 offset {123, 456};
        float32_t depth {123.0f};
        CHECK_FALSE(Client_Critter_GetBoneMapAnchor(cr, client->Hashes.to_hashed_string("MissingBone"), 0.0f, 0.0f, 0.0f, offset, depth));
        CHECK(offset == cr->GetSpriteOffset());
        CHECK(depth == 0.0f);

        for (float32_t invalid : {std::numeric_limits<float32_t>::quiet_NaN(), std::numeric_limits<float32_t>::infinity()}) {
            offset = ipos32 {123, 456};
            depth = 123.0f;
            CHECK_FALSE(Client_Critter_GetBoneMapAnchor(cr, bone, invalid, 0.0f, 0.0f, offset, depth));
            CHECK(offset == cr->GetSpriteOffset());
            CHECK(depth == 0.0f);
        }

        cr->InvalidateSprite();
        offset = ipos32 {123, 456};
        depth = 123.0f;
        CHECK_FALSE(Client_Critter_GetBoneMapAnchor(cr, bone, 0.0f, 0.0f, 0.0f, offset, depth));
        CHECK(offset == cr->GetSpriteOffset());
        CHECK(depth == 0.0f);

        auto flat {map->AddMapperCritter(client->Hashes.to_hashed_string("UnitTestClientCritter"), mpos {9, 8}, mdir {}, nullptr)};
        depth = 123.0f;
        CHECK_FALSE(Client_Critter_GetBoneMapAnchor(flat, bone, 0.0f, 0.0f, 0.0f, offset, depth));
        CHECK(offset == flat->GetSpriteOffset());
        CHECK(depth == 0.0f);

        auto off_map {safe_alloc::make_refcounted<CritterView>(client, ident_t {2002}, client->GetProtoCritter(client->Hashes.to_hashed_string("UnitTestClientCritter")), nullptr)};
        auto destroy_off_map {scope_exit([&off_map]() noexcept { safe_call([&off_map] { off_map->DestroySelf(); }); })};
        CHECK_THROWS_AS(Client_Critter_GetBoneMapAnchor(off_map, bone, 0.0f, 0.0f, 0.0f, offset, depth), ScriptException);
    }
}

TEST_CASE("ModelBoneDepthKeepsThePosedPointAcrossRenderPaths")
{
    constexpr string_view MESH_PATH {"Models/BoneDepth.fbx"};
    constexpr string_view MODEL_PATH {"Models/BoneDepth.fo3d"};
    constexpr string_view DESCRIPTION {"Model BoneDepth.fbx\nScale 1.25\nAnim 1 1 BoneDepth.fbx Base\n"};
    auto mesh {MakeRuntimeModelTriangleMesh()};
    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    resources.emplace_back(string {MESH_PATH}, mesh);
    resources.emplace_back(string {MODEL_PATH}, MakeRuntimeModelDescription(MODEL_PATH, MESH_PATH, mesh, {}, {}, nullptr, false, {}, 0.0f, nullptr, DESCRIPTION));
    auto settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Render.ModelProjFactor, 32.0f);
    BakerTests::OverrideSetting(settings.Geometry.MapCameraAngle, GameSettings::MAP_CAMERA_ANGLE);
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);
    auto model {factory->GetModelMngr()->CreateModel(MODEL_PATH)};
    REQUIRE(model);
    model->StartMeshGeneration();
    REQUIRE(model->HasAnimation(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1)));
    ignore_unused(model->PlayAnim(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), nullptr, 0.0f, ModelAnimFlags::Freeze));
    CHECK(model->GetStateAnim() == static_cast<CritterStateAnim>(1));
    CHECK(model->GetActionAnim() == static_cast<CritterActionAnim>(1));
    hstring bone {client->Hashes.to_hashed_string(MESH_PATH)};
    float32_t angle {GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT};

    SECTION("OneScreenPointCanHaveDifferentPhysicalDepths")
    {
        model->SetDir(mdir {int16_t {180}}, false);
        model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
        model->PoseSpriteFrame(false);
        float32_t near_depth {-1.0f};
        float32_t far_depth {-1.0f};
        auto far {model->GetBonePos(bone, vec3 {}, make_ptr(&far_depth))};
        auto near {model->GetBonePos(bone, vec3 {0.0f, std::sin(angle), std::cos(angle)}, make_ptr(&near_depth))};
        REQUIRE(far);
        REQUIRE(near);
        CHECK(*far == *near);
        CHECK(is_float_equal(far_depth, 0.0f));
        CHECK(is_float_equal(near_depth, 40.0f));
    }

    SECTION("WorldProjectionIsIndependentOfPivotAndDirectSceneScale")
    {
        vec3 local {0.7f, 0.4f, 0.3f};

        for (int16_t direction : {int16_t {180}, int16_t {90}, int16_t {0}}) {
            float32_t yaw {numeric_cast<float32_t>(180 - direction) * DEG_TO_RAD_FLOAT};
            vec3 world {
                (std::cos(yaw) * local.x + std::sin(yaw) * local.z) * 40.0f,
                local.y * 40.0f,
                (-std::sin(yaw) * local.x + std::cos(yaw) * local.z) * 40.0f,
            };
            vec3 expected {GeometryHelper::ProjectWorldToMap(world)};

            for (ipos32 pivot : {ipos32 {64, 96}, ipos32 {23, 71}}) {
                model->SetDir(mdir {direction}, false);
                model->SetupFrame(isize32 {128, 128}, pivot);
                model->PoseSpriteFrame(false);
                float32_t depth {-1.0f};
                auto offset {model->GetBonePos(bone, local, make_ptr(&depth))};
                REQUIRE(offset);
                CHECK(offset == model->GetBonePos(bone, local));
                CHECK(offset->x == iround<int32_t>(expected.x));
                CHECK(offset->y == iround<int32_t>(expected.y));
                CHECK(is_float_equal(depth, expected.z));

                for (float32_t scale : {0.5f, 2.0f}) {
                    mat44 projection {client->SprMngr.GetRender().CreateOrthoMatrix(-50.0f, 200.0f, -75.0f, 175.0f, -10.0f, 10.0f)};
                    model->DrawInScene(projection, scale);
                    float32_t direct_depth {-1.0f};
                    auto direct_offset {model->GetBonePos(bone, local, make_ptr(&direct_depth))};
                    CHECK(direct_offset == offset);
                    CHECK(is_float_equal(direct_depth, expected.z));
                }
            }
        }
    }

    SECTION("InvalidPointsClearTheRequestedDepth")
    {
        model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
        model->PoseSpriteFrame(false);
        float32_t depth {123.0f};
        CHECK_FALSE(model->GetBonePos(client->Hashes.to_hashed_string("MissingBone"), vec3 {}, make_ptr(&depth)));
        CHECK(depth == 0.0f);

        for (float32_t invalid : {std::numeric_limits<float32_t>::quiet_NaN(), std::numeric_limits<float32_t>::infinity(), std::numeric_limits<float32_t>::max()}) {
            depth = 123.0f;
            CHECK_FALSE(model->GetBonePos(bone, vec3 {invalid, 0.0f, 0.0f}, make_ptr(&depth)));
            CHECK(depth == 0.0f);
        }
    }
}

#if FO_EFFEKSEER_PARTICLES
TEST_CASE("ModelParticleEmitsAtItsBoneBeforeAnotherPose")
{
    constexpr string_view mesh_path {"Models/ImmediateParticle.fbx"};
    constexpr string_view model_path {"Models/ImmediateParticle.fo3d"};
    constexpr string_view particle_path {"Particles/Immediate.efk"};
    bool unlit {GENERATE(false, true)};
    CAPTURE(unlit);
    constexpr string_view particle_project {R"EFK(<?xml version="1.0" encoding="utf-8"?>
<EffekseerProject>
  <Root>
    <Name>Root</Name>
    <Children>
      <Node>
        <Name>Instant</Name>
        <CommonValues>
          <MaxGeneration>
            <Value>1</Value>
            <Infinite>False</Infinite>
          </MaxGeneration>
          <Life>
            <Min>5</Min>
            <Max>5</Max>
          </Life>
          <Generation>
            <GenerationTime>
              <Min>0</Min>
              <Max>0</Max>
            </GenerationTime>
          </Generation>
        </CommonValues>
        <RendererCommonValues>
          <AlphaBlend>1</AlphaBlend>
          <ZTest>False</ZTest>
          <ZWrite>False</ZWrite>
        </RendererCommonValues>
        <DrawingValues>
          <ColorAll>
            <Fixed>
              <R>255</R>
              <G>255</G>
              <B>255</B>
              <A>255</A>
            </Fixed>
          </ColorAll>
          <Sprite>
            <Billboard>0</Billboard>
          </Sprite>
        </DrawingValues>
        <Children />
      </Node>
    </Children>
  </Root>
  <ToolVersion>1.80.5</ToolVersion>
  <Version>3</Version>
  <StartFrame>0</StartFrame>
  <EndFrame>5</EndFrame>
  <IsLoop>False</IsLoop>
</EffekseerProject>
)EFK"};

    auto project_bytes {const_span<uint8_t> {reinterpret_cast<const uint8_t*>(particle_project.data()), particle_project.size()}};
    EffekseerCompilerOutput particle {CompileEffekseerProject("Particles/Immediate.efkproj", project_bytes)};
    REQUIRE(particle.Dependencies.empty());

    AppendEffekseerBoundsTrailer(particle.Binary, vec3 {-0.5f}, vec3 {0.5f}, 0.75f);

    vector<uint8_t> mesh {MakeSkinnedRuntimeModelMesh()};
    vector<uint8_t> animation_info {};
    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back(string {mesh_path}, mesh);
    resources.emplace_back(string {model_path}, MakeRuntimeModelDescription(model_path, mesh_path, mesh, {}, {}, nullptr, true, {}, 2.0f, &animation_info, "Model ImmediateParticle.fbx\nAnim 1 1 ModelFile Base\n"));
    resources.emplace_back("ModelAnimationInfo.foinfo", std::move(animation_info));
    resources.emplace_back(string {particle_path}, std::move(particle.Binary));

    constexpr string_view particle_effect {"[Effect]\nPasses = 1\nDepthWrite = False\nDepthFunc = LessEqual\nDepthVariants = True\nCullVariants = True\n"};
    resources.emplace_back("Effects/Particles_ColorMulAtlas.fofx", vector<uint8_t> {particle_effect.begin(), particle_effect.end()});

    auto settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Render.ModelProjFactor, 32.0f);

    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);

    auto model {factory->GetModelMngr()->CreateModel(model_path)};
    REQUIRE(model);

    model->StartMeshGeneration();
    model->PrepareFrameLayout();
    hstring bone {client->Hashes.to_hashed_string(mesh_path)};
    auto state {static_cast<CritterStateAnim>(1)};
    auto action {static_cast<CritterActionAnim>(1)};
    (void)model->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::Freeze);
    model->SetupFrame(isize32 {512, 512}, ipos32 {256, 384});
    model->PoseSpriteFrame(false);

    auto before {model->GetSpriteBounds()};
    REQUIRE(before);
    REQUIRE(before->Rect.width < 512);

    auto particle_attach_count {[](const auto& instance) -> int32_t {
        int32_t count {0};

        for (const ModelAttachPoint& attach : instance->GetAttachPoints()) {
            if (attach.Kind == ModelAttachKind::Particle) {
                count++;
            }
        }

        return count;
    }};

    auto run_particle {[unlit](ptr<ModelInstance> instance, string_view path, hstring bone_name, vec3 offset) {
        if (unlit) {
            instance->RunUnlitParticle(path, bone_name, offset);
        }
        else {
            instance->RunParticle(path, bone_name, offset);
        }
    }};

    SECTION("CallbackBirthIsReadyBeforeTheNextPose")
    {
        vec3 offset {4.0f, 0.0f, 0.0f};
        int32_t callbacks {0};

        // Consume the construction interval before arming the callback, so only the explicit 200 ms crosses it
        client->GameTime.FrameAdvance(false);
        model->PoseSpriteFrame(true);

        model->AddAnimationCallback(ModelAnimationCallback {
            .StateAnim = state,
            .ActionAnim = action,
            .NormalizedTime = 0.5f,
            .Callback =
                [&model, &run_particle, particle_path, bone, offset, &callbacks] {
                    callbacks++;

                    run_particle(model.get(), particle_path, bone, offset);
                },
        });
        model->SetSpeed(10.0f);
        (void)model->PlayAnim(state, action, nullptr, 0.0f, combine_enum(ModelAnimFlags::PlayOnce, ModelAnimFlags::NoSmooth));
        model->SetupFrame(isize32 {512, 512}, ipos32 {256, 384});
        model->PoseSpriteFrame(true);

        coarse_sleep(std::chrono::milliseconds {200});
        client->GameTime.FrameAdvance(false);
        model->PoseSpriteFrame(true);
        REQUIRE(callbacks == 1);

        // The callback runs after the particle loop; the first observation must contain the zero-time effect itself
        CHECK(particle_attach_count(model) == 1);
        CHECK(model->HasUnlitParticles() == unlit);
        auto first {model->GetSpriteBounds()};
        REQUIRE(first);
        CAPTURE(first->Rect.x, first->Rect.y, first->Rect.width, first->Rect.height, first->RequiredFrameSize.width, first->RequiredFrameSize.height);
        CHECK(model->GetDrawSize() == isize32 {512, 512});

        auto relative {model->GetBonePos(bone, offset)};
        REQUIRE(relative);

        ipos32 point {*relative + model->GetFramePivot()};
        REQUIRE((point.x < before->Rect.x || point.x >= before->Rect.x + before->Rect.width || point.y < before->Rect.y || point.y >= before->Rect.y + before->Rect.height));
        CHECK(point.x >= first->Rect.x);
        CHECK(point.x < first->Rect.x + first->Rect.width);
        CHECK(point.y >= first->Rect.y);
        CHECK(point.y < first->Rect.y + first->Rect.height);
        CHECK(first->RequiredFrameSize.width <= 512);
        CHECK(first->RequiredFrameSize.height <= 512);

        // A zero-time re-pose is the independent resource control and must reveal the same live particle
        model->PoseSpriteFrame(false);
        auto after_repose {model->GetSpriteBounds()};
        REQUIRE(after_repose);
        CHECK(particle_attach_count(model) == 1);
        CHECK(model->HasUnlitParticles() == unlit);

        auto after_relative {model->GetBonePos(bone, offset)};
        REQUIRE(after_relative);

        ipos32 after_point {*after_relative + model->GetFramePivot()};
        CHECK(after_point.x >= after_repose->Rect.x);
        CHECK(after_point.x < after_repose->Rect.x + after_repose->Rect.width);
        CHECK(after_point.y >= after_repose->Rect.y);
        CHECK(after_point.y < after_repose->Rect.y + after_repose->Rect.height);
        CHECK(after_repose->RequiredFrameSize.width <= 512);
        CHECK(after_repose->RequiredFrameSize.height <= 512);
    }

    SECTION("DirectBirthUsesThePosedBoneAndIgnoresPreBirthTime")
    {
        vec3 offset {4.0f, 0.0f, 0.0f};

        // The unconsumed interval reaches the first pose after the request; birth must not inherit it
        coarse_sleep(std::chrono::milliseconds {200});
        client->GameTime.FrameAdvance(false);
        run_particle(model.get(), particle_path, bone, offset);
        model->PoseSpriteFrame(true);

        CHECK(particle_attach_count(model) == 1);
        CHECK(model->HasUnlitParticles() == unlit);
        auto first {model->GetSpriteBounds()};
        REQUIRE(first);
        CAPTURE(first->Rect.x, first->Rect.y, first->Rect.width, first->Rect.height, first->RequiredFrameSize.width, first->RequiredFrameSize.height);
        CHECK(model->GetDrawSize() == isize32 {512, 512});

        auto relative {model->GetBonePos(bone, offset)};
        REQUIRE(relative);

        ipos32 point {*relative + model->GetFramePivot()};
        REQUIRE((point.x < before->Rect.x || point.x >= before->Rect.x + before->Rect.width || point.y < before->Rect.y || point.y >= before->Rect.y + before->Rect.height));
        CHECK(point.x >= first->Rect.x);
        CHECK(point.x < first->Rect.x + first->Rect.width);
        CHECK(point.y >= first->Rect.y);
        CHECK(point.y < first->Rect.y + first->Rect.height);
        CHECK(first->RequiredFrameSize.width <= 512);
        CHECK(first->RequiredFrameSize.height <= 512);
    }

    SECTION("DirectBirthKeepsTheRequestedOffset")
    {
        vec3 offset {6.0f, 0.0f, 0.0f};

        coarse_sleep(std::chrono::milliseconds {200});
        client->GameTime.FrameAdvance(false);
        run_particle(model.get(), particle_path, bone, offset);
        model->PoseSpriteFrame(true);

        CHECK(particle_attach_count(model) == 1);
        CHECK(model->HasUnlitParticles() == unlit);
        auto first {model->GetSpriteBounds()};
        REQUIRE(first);
        CAPTURE(first->Rect.x, first->Rect.y, first->Rect.width, first->Rect.height, first->RequiredFrameSize.width, first->RequiredFrameSize.height);
        CHECK(model->GetDrawSize() == isize32 {512, 512});

        auto relative {model->GetBonePos(bone, offset)};
        REQUIRE(relative);

        ipos32 point {*relative + model->GetFramePivot()};
        CHECK(point.x >= first->Rect.x);
        CHECK(point.x < first->Rect.x + first->Rect.width);
        CHECK(point.y >= first->Rect.y);
        CHECK(point.y < first->Rect.y + first->Rect.height);

        // Six authored units need more than 320 pixels; dropping the request offset cannot reach that width
        CHECK(first->RequiredFrameSize.width > 320);
        CHECK(first->RequiredFrameSize.width <= 512);
        CHECK(first->RequiredFrameSize.height <= 512);
    }

    SECTION("DirectBirthAtZeroTimeIsAValidResourceControl")
    {
        vec3 offset {4.0f, 0.0f, 0.0f};
        CHECK_FALSE(model->NeedForceDraw());

        run_particle(model.get(), particle_path, bone, offset);
        CHECK(model->NeedForceDraw());

        model->PoseSpriteFrame(false);

        CHECK(particle_attach_count(model) == 1);
        CHECK(model->HasUnlitParticles() == unlit);
        auto control {model->GetSpriteBounds()};
        REQUIRE(control);
        CAPTURE(control->Rect.x, control->Rect.y, control->Rect.width, control->Rect.height, control->RequiredFrameSize.width, control->RequiredFrameSize.height);
        CHECK(model->GetDrawSize() == isize32 {512, 512});

        auto relative {model->GetBonePos(bone, offset)};
        REQUIRE(relative);

        ipos32 point {*relative + model->GetFramePivot()};
        REQUIRE((point.x < before->Rect.x || point.x >= before->Rect.x + before->Rect.width || point.y < before->Rect.y || point.y >= before->Rect.y + before->Rect.height));
        CHECK(point.x >= control->Rect.x);
        CHECK(point.x < control->Rect.x + control->Rect.width);
        CHECK(point.y >= control->Rect.y);
        CHECK(point.y < control->Rect.y + control->Rect.height);
        CHECK(control->RequiredFrameSize.width <= 512);
        CHECK(control->RequiredFrameSize.height <= 512);
    }

    SECTION("UnposedRequestWaitsForTheFirstPoseAndThenExpires")
    {
        vec3 offset {4.0f, 0.0f, 0.0f};
        auto fresh_model {factory->GetModelMngr()->CreateModel(model_path)};
        REQUIRE(fresh_model);

        fresh_model->StartMeshGeneration();
        fresh_model->PrepareFrameLayout();
        (void)fresh_model->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::Freeze);
        fresh_model->SetupFrame(isize32 {512, 512}, ipos32 {256, 384});

        auto mesh_bounds {fresh_model->GetSpriteBounds()};
        REQUIRE(mesh_bounds);
        REQUIRE(mesh_bounds->Rect.width < 512);

        run_particle(fresh_model.get(), particle_path, bone, offset);

        // A request without a valid pose must not reserve an identity-origin envelope
        auto pending_bounds {fresh_model->GetSpriteBounds()};
        REQUIRE(pending_bounds);
        CHECK(pending_bounds->Rect == mesh_bounds->Rect);
        CHECK(pending_bounds->RequiredFrameSize == mesh_bounds->RequiredFrameSize);

        CHECK_FALSE(fresh_model->HasUnlitParticles());

        fresh_model->PoseSpriteFrame(false);
        auto placed {fresh_model->GetSpriteBounds()};
        REQUIRE(placed);
        CAPTURE(placed->Rect.x, placed->Rect.y, placed->Rect.width, placed->Rect.height, placed->RequiredFrameSize.width, placed->RequiredFrameSize.height);
        CHECK(particle_attach_count(fresh_model) == 1);
        CHECK(fresh_model->HasUnlitParticles() == unlit);
        CHECK(fresh_model->GetDrawSize() == isize32 {512, 512});

        auto relative {fresh_model->GetBonePos(bone, offset)};
        REQUIRE(relative);

        ipos32 point {*relative + fresh_model->GetFramePivot()};
        REQUIRE((point.x < before->Rect.x || point.x >= before->Rect.x + before->Rect.width || point.y < before->Rect.y || point.y >= before->Rect.y + before->Rect.height));
        CHECK(point.x >= placed->Rect.x);
        CHECK(point.x < placed->Rect.x + placed->Rect.width);
        CHECK(point.y >= placed->Rect.y);
        CHECK(point.y < placed->Rect.y + placed->Rect.height);

        // A ghost left at the identity world origin would demand more than the allocated 512-pixel frame
        CHECK(placed->RequiredFrameSize.width <= 512);
        CHECK(placed->RequiredFrameSize.height <= 512);

        // A live particle advances with positive time and must disappear from both consumers after its short life
        coarse_sleep(std::chrono::milliseconds {200});
        client->GameTime.FrameAdvance(false);
        fresh_model->PoseSpriteFrame(true);

        auto finished {fresh_model->GetSpriteBounds()};
        REQUIRE(finished);

        int32_t retirement_poses {0};

        while (particle_attach_count(fresh_model) != 0 && retirement_poses < 3) {
            fresh_model->PoseSpriteFrame(false);
            finished = fresh_model->GetSpriteBounds();
            REQUIRE(finished);
            retirement_poses++;
        }

        CHECK(particle_attach_count(fresh_model) == 0);
        CHECK_FALSE(fresh_model->HasUnlitParticles());
        REQUIRE(finished);
        CHECK((point.x < finished->Rect.x || point.x >= finished->Rect.x + finished->Rect.width || point.y < finished->Rect.y || point.y >= finished->Rect.y + finished->Rect.height));
    }

    SECTION("MissingBoneCreatesNoParticleAndLeavesTheModelUsable")
    {
        vec3 offset {4.0f, 0.0f, 0.0f};
        CHECK(particle_attach_count(model) == 0);
        CHECK_FALSE(model->HasUnlitParticles());

        run_particle(model.get(), particle_path, client->Hashes.to_hashed_string("MissingParticleBone"), offset);

        auto rejected {model->GetSpriteBounds()};
        REQUIRE(rejected);
        CHECK(rejected->Rect == before->Rect);
        CHECK(rejected->RequiredFrameSize == before->RequiredFrameSize);
        CHECK(particle_attach_count(model) == 0);
        CHECK_FALSE(model->HasUnlitParticles());

        run_particle(model.get(), particle_path, bone, offset);
        model->PoseSpriteFrame(false);

        auto accepted {model->GetSpriteBounds()};
        REQUIRE(accepted);
        CHECK(particle_attach_count(model) == 1);
        CHECK(model->HasUnlitParticles() == unlit);

        auto relative {model->GetBonePos(bone, offset)};
        REQUIRE(relative);

        ipos32 point {*relative + model->GetFramePivot()};
        CHECK(point.x >= accepted->Rect.x);
        CHECK(point.x < accepted->Rect.x + accepted->Rect.width);
        CHECK(point.y >= accepted->Rect.y);
        CHECK(point.y < accepted->Rect.y + accepted->Rect.height);
        CHECK(accepted->RequiredFrameSize.width <= 512);
        CHECK(accepted->RequiredFrameSize.height <= 512);
    }
    SECTION("AtlasOverlayUsesTheBornPoseAndRetires")
    {
        bool direct_draw {GENERATE(false, true)};
        CAPTURE(direct_draw);
        BakerTests::OverrideSetting(settings.Render.ModelDirectDraw, direct_draw);

        auto sprite {factory->LoadSprite(client->Hashes.to_hashed_string(model_path), AtlasType::OneImage).dyn_cast<ModelSprite>()};
        REQUIRE(sprite);
        REQUIRE(sprite->IsDirectDraw() == direct_draw);

        auto instance {sprite->GetModel()};
        instance->StartMeshGeneration();
        instance->PrepareFrameLayout();
        (void)instance->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::Freeze);
        instance->SetupFrame(isize32 {512, 512}, ipos32 {256, 384});
        sprite->SetSize(isize32 {512, 512});

        run_particle(instance, particle_path, bone, vec3 {4.0f, 0.0f, 0.0f});
        CHECK_FALSE(instance->HasUnlitParticles());
        CHECK(sprite->Update());
        CHECK(particle_attach_count(instance) == 1);
        CHECK(instance->HasUnlitParticles() == unlit);

        auto overlay {sprite->GetUnlitOverlay()};

        if (unlit) {
            REQUIRE(overlay);
            CHECK(overlay->GetSize() == sprite->GetSize());
            CHECK(overlay->GetOffset() == sprite->GetOffset());
            CHECK(overlay->GetBatchTexture());
            CHECK(overlay->GetBatchTexture() != sprite->GetBatchTexture());

            instance->RequestRedraw();
            CHECK(sprite->Update());
            CHECK(sprite->GetUnlitOverlay() == overlay);
        }
        else {
            CHECK_FALSE(overlay);
        }

        coarse_sleep(std::chrono::milliseconds {200});
        client->GameTime.FrameAdvance(false);

        if (direct_draw) {
            instance->DrawInScene(mat44 {1.0f}, 1.0f);
            CHECK_FALSE(instance->NeedForceDraw());
        }

        CHECK(sprite->Update());

        int32_t retirement_poses {0};

        while (particle_attach_count(instance) != 0 && retirement_poses < 3) {
            if (direct_draw) {
                instance->DrawInScene(mat44 {1.0f}, 1.0f);
                CHECK_FALSE(instance->NeedForceDraw());
            }
            else {
                instance->RequestRedraw();
            }

            CHECK(sprite->Update());
            retirement_poses++;
        }

        CHECK(particle_attach_count(instance) == 0);
        CHECK_FALSE(instance->HasUnlitParticles());
        CHECK_FALSE(sprite->GetUnlitOverlay());
    }
}
#endif

TEST_CASE("ModelManagerInstantiatesABakedModel")
{
    // The 3D instance surface was assumed to need a GPU, but the headless Null renderer serves it: with a
    // baked mesh and a valid baked description the manager builds a real ModelInstance
    constexpr string_view MESH_PATH {"Models/RuntimeInstance.fbx"};
    constexpr string_view MODEL_PATH {"Models/RuntimeInstance.fo3d"};

    Vertex3D vertex {};
    vertex.BlendWeights[0] = 1.0f;
    vertex.BlendIndices[0] = 0.0f;

    vector<pair<string, vector<uint8_t>>> model_resources {};
    ignore_unused(vertex);
    vector<uint8_t> mesh_blob {MakeRuntimeModelTriangleMesh()};

    model_resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    model_resources.emplace_back(string {MESH_PATH}, mesh_blob);
    model_resources.emplace_back(string {MODEL_PATH}, MakeRuntimeModelDescription(MODEL_PATH, MESH_PATH, mesh_blob));

    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(model_resources)))};

    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);

    auto model_mngr {factory->GetModelMngr()};

    REQUIRE_NOTHROW(model_mngr->PreloadModel(MODEL_PATH));

    auto model {model_mngr->CreateModel(MODEL_PATH)};
    REQUIRE(static_cast<bool>(model));

    SECTION("TheInstanceAnswersItsGeometryAndAnimationState")
    {
        ignore_unused(model->GetInformation());
        ignore_unused(model->GetStateAnim());
        ignore_unused(model->GetActionAnim());
        ignore_unused(model->GetMovingContext());
        ignore_unused(model->GetDrawSize());
        ignore_unused(model->GetLightingSize());
        ignore_unused(model->GetSpriteBounds());
        ignore_unused(model->GetDrawRect());
        ignore_unused(model->GetFramePivot());
        ignore_unused(model->NeedForceDraw());
        ignore_unused(model->NeedDraw());
        ignore_unused(model->HasAnimation(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1)));

        ipos32 screen_pos {model->Convert3dTo2d(vec3 {0.0f, 0.0f, 0.0f})};
        vec3 world_pos {model->Convert2dTo3d(screen_pos)};
        CHECK(std::isfinite(world_pos.x));
        CHECK(std::isfinite(world_pos.y));
        CHECK(std::isfinite(world_pos.z));

        auto state_anim {static_cast<CritterStateAnim>(1)};
        auto action_anim {static_cast<CritterActionAnim>(1)};
        ignore_unused(model->ResolveAnimation(state_anim, action_anim));
    }

    SECTION("TheInstanceAcceptsOrientationScaleAndMovementState")
    {
        REQUIRE_NOTHROW(model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96}));
        REQUIRE_NOTHROW(model->PrepareFrameLayout());
        REQUIRE_NOTHROW(model->RequestRedraw());

        for (int32_t dir {0}; dir < 6; dir++) {
            REQUIRE_NOTHROW(model->SetDir(mdir {numeric_cast<uint8_t>(dir)}, false));
            REQUIRE_NOTHROW(model->SetLookDir(mdir {numeric_cast<uint8_t>(dir)}));
            REQUIRE_NOTHROW(model->SetMoveDir(mdir {numeric_cast<uint8_t>(dir)}, true));
        }

        REQUIRE_NOTHROW(model->SetRotation(0.0f, 30.0f, 0.0f));
        REQUIRE_NOTHROW(model->SetScale(1.0f, 1.0f, 1.0f));
        REQUIRE_NOTHROW(model->SetSpeed(2.0f));
        REQUIRE_NOTHROW(model->EnableShadow(true));
        REQUIRE_NOTHROW(model->EnableShadow(false));
        REQUIRE_NOTHROW(model->AddMoveOffset(ipos32 {4, 4}));
        REQUIRE_NOTHROW(model->SetMovementState(false, true, 10));
        REQUIRE_NOTHROW(model->SetMovementState(true, false, 0));

        ignore_unused(model->GetMoveDirAngle());
        ignore_unused(model->HasBodyRotation());
        ignore_unused(model->GetViewRect());
        ignore_unused(model->GetAttachPoints());
        ignore_unused(model->GetAnimDuration());
        ignore_unused(model->GetAnimDuration(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1)));
        ignore_unused(model->IsAnimationPlaying());

        hstring root_bone {client->Hashes.to_hashed_string("Root")};
        ignore_unused(model->FindBone(root_bone));
        ignore_unused(model->GetBonePos(root_bone));
        ignore_unused(model->GetBoneSpritePos(root_bone));

        REQUIRE_NOTHROW(model->ClearAnimationCallbacks());
        REQUIRE_NOTHROW(model->SetAnimInitCallback([](CritterAnimationContext, int32_t, CritterStateAnim&, CritterActionAnim&, CritterAnimationFlags&, float32_t&, float32_t&) { }));
    }

    SECTION("AFrameLargerThanTheMaximumRenderTextureIsRejected")
    {
        // Rejected while the model is still identifiable, instead of reaching the graphics API and failing there as
        // an anonymous invalid argument
        int32_t max_draw_width {AppRender::MAX_ATLAS_WIDTH / ModelInstance::FRAME_SCALE};
        int32_t max_draw_height {AppRender::MAX_ATLAS_HEIGHT / ModelInstance::FRAME_SCALE};

        REQUIRE(max_draw_width > 0);
        REQUIRE(max_draw_height > 0);
        REQUIRE_NOTHROW(model->SetupFrame(isize32 {max_draw_width, max_draw_height}, ipos32 {}));
        REQUIRE_THROWS(model->SetupFrame(isize32 {max_draw_width + 1, max_draw_height}, ipos32 {}));
        REQUIRE_THROWS(model->SetupFrame(isize32 {max_draw_width, max_draw_height + 1}, ipos32 {}));
    }

    SECTION("LayerValuesDriveTheAnimationDataAndLinkTransforms")
    {
        // Each link block applies only when its layer value is requested, so nothing under SetAnimData runs until
        // an animation plays with layers set
        model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});

        auto state_anim {static_cast<CritterStateAnim>(1)};
        auto action_anim {static_cast<CritterActionAnim>(1)};

        std::array<int32_t, MODEL_LAYERS_COUNT> layers {};

        auto play_with_layers {[&](ModelAnimFlags flags) {
            REQUIRE_NOTHROW(ignore_unused(model->PlayAnim(state_anim, action_anim, layers.data(), 0.0f, flags)));

            for (int32_t frame {0}; frame < 3; frame++) {
                REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
            }
        }};

        // Layer 1 / value 1 is the block that links a bone and scales, rotates and moves it
        layers[1] = 1;
        play_with_layers(ModelAnimFlags::Init);

        // Layer 1 / value 2 disables the second layer and every mesh under it
        layers[1] = 2;
        layers[2] = 1;
        play_with_layers(ModelAnimFlags::None);

        // The same walk again with the smoothing and rotation paths switched off
        layers[1] = 1;
        layers[2] = 1;
        play_with_layers(ModelAnimFlags::NoSmooth);
        play_with_layers(ModelAnimFlags::NoRotate);
        play_with_layers(ModelAnimFlags::PlayOnce);
        play_with_layers(ModelAnimFlags::Freeze);

        // Clearing the layers takes the removal side of the same walk
        layers[1] = 0;
        layers[2] = 0;
        play_with_layers(ModelAnimFlags::None);

        // A speed override and a missing animation are the two remaining arms of PlayAnim
        REQUIRE_NOTHROW(ignore_unused(model->PlayAnim(state_anim, action_anim, layers.data(), 0.5f, ModelAnimFlags::None)));
        CHECK_FALSE(model->PlayAnim(static_cast<CritterStateAnim>(9), static_cast<CritterActionAnim>(9), layers.data(), 0.0f, ModelAnimFlags::None));

        // Switching between the authored animations is what runs the cross-fade and the per-animation
        // speed and layer-value overrides, none of which a single-animation model can reach
        for (CritterActionAnim authored : {static_cast<CritterActionAnim>(1), static_cast<CritterActionAnim>(3), static_cast<CritterActionAnim>(5), static_cast<CritterActionAnim>(17), static_cast<CritterActionAnim>(3), static_cast<CritterActionAnim>(1)}) {
            CHECK(model->HasAnimation(state_anim, authored));
            REQUIRE_NOTHROW(ignore_unused(model->PlayAnim(state_anim, authored, layers.data(), 0.0f, ModelAnimFlags::None)));

            for (int32_t frame {0}; frame < 3; frame++) {
                REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
            }

            ignore_unused(model->GetAnimDuration(state_anim, authored));
        }

        // Movement animations are chosen from the authored walk/run pair rather than played by name
        for (int32_t speed : {0, 5, 20}) {
            REQUIRE_NOTHROW(model->SetMovementState(true, speed != 0, speed));
            REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
        }

        REQUIRE_NOTHROW(model->SetMovementState(false, false, 0));

        ignore_unused(model->GetSpriteBounds());
        ignore_unused(model->GetAttachPoints());
    }

    SECTION("PosingAdvancesTheAnimationAndBuildsTheFrame")
    {
        model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});

        for (int32_t frame {0}; frame < 4; frame++) {
            REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
            std::this_thread::sleep_for(std::chrono::milliseconds {8});
        }

        ignore_unused(model->GetSpriteBounds());
        ignore_unused(model->GetDrawSize());
        ignore_unused(model->NeedDraw());
    }

    SECTION("ScenePreparationIncludesPoseChangesRequestedByAnimationCallbacks")
    {
        auto state {static_cast<CritterStateAnim>(1)};
        auto action {static_cast<CritterActionAnim>(1)};
        (void)model->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::Freeze);
        model->StartMeshGeneration();
        model->SetDir(mdir {int16_t {180}}, false);
        client->GameTime.FrameAdvance(false);
        model->PoseSpriteFrame(true);

        int32_t callbacks {0};
        model->AddAnimationCallback(ModelAnimationCallback {
            .StateAnim = state,
            .ActionAnim = action,
            .NormalizedTime = 0.5f,
            .Callback =
                [&model, &callbacks] {
                    callbacks++;
                    model->SetDir(mdir {int16_t {90}}, false);
                },
        });
        model->SetSpeed(10.0f);
        (void)model->PlayAnim(state, action, nullptr, 0.0f, combine_enum(ModelAnimFlags::PlayOnce, ModelAnimFlags::NoSmooth));
        model->PoseSpriteFrame(true);

        coarse_sleep(std::chrono::milliseconds {200});
        client->GameTime.FrameAdvance(false);
        REQUIRE(model->PrepareSceneFrame(1.0f));
        REQUIRE(callbacks == 1);

        hstring root_bone {client->Hashes.to_hashed_string(MESH_PATH)};
        auto point {vec3 {4.0f, 2.0f, 1.0f}};
        auto prepared {model->GetBonePos(root_bone, point)};
        REQUIRE(prepared);

        model->PoseSpriteFrame(false);
        auto settled {model->GetBonePos(root_bone, point)};
        REQUIRE(settled);

        CAPTURE(prepared->x, prepared->y, settled->x, settled->y);
        CHECK(prepared == settled);
        CHECK(callbacks == 1);
    }

    SECTION("RepeatedDirectSceneDrawKeepsTheDisplayedBonePose")
    {
        BakerTests::OverrideSetting(settings.Render.ModelDirectDraw, true);

        auto sprite {factory->LoadSprite(client->Hashes.to_hashed_string(MODEL_PATH), AtlasType::OneImage).dyn_cast<ModelSprite>()};
        REQUIRE(sprite);
        REQUIRE(sprite->IsDirectDraw());

        auto instance {sprite->GetModel()};
        instance->StartMeshGeneration();
        instance->SetDir(mdir {int16_t {0}}, false);
        CHECK(sprite->Update());

        instance->SetDir(mdir {int16_t {90}}, true);
        coarse_sleep(std::chrono::milliseconds {50});
        client->GameTime.FrameAdvance(false);
        CHECK(sprite->Update());

        sprite->DrawInScene(fpos32 {64.0f, 64.0f}, 0.0f);

        hstring root_bone {client->Hashes.to_hashed_string(MESH_PATH)};
        auto point {vec3 {4.0f, 2.0f, 1.0f}};
        auto submitted_anchor {instance->GetBonePos(root_bone, point)};
        REQUIRE(submitted_anchor);

        float32_t submitted_angle {instance->GetMoveDirAngle()};

        for (fpos32 scene_pos : {fpos32 {160.0f, 96.0f}, fpos32 {240.0f, 128.0f}}) {
            sprite->DrawInScene(scene_pos, 0.0f);

            auto drawn_anchor {instance->GetBonePos(root_bone, point)};
            REQUIRE(drawn_anchor);
            CAPTURE(submitted_anchor->x, submitted_anchor->y, drawn_anchor->x, drawn_anchor->y, submitted_angle, instance->GetMoveDirAngle());

            CHECK(drawn_anchor == submitted_anchor);
            CHECK(instance->GetMoveDirAngle() == submitted_angle);
        }
    }

    SECTION("BoneOffsetsKeepSpriteCoordinatesAfterDirectSceneDraw")
    {
        // ModelManager names the physical root after the loaded mesh resource
        hstring root_bone {client->Hashes.to_hashed_string(MESH_PATH)};
        CHECK_FALSE(model->GetBonePos(client->Hashes.to_hashed_string("MissingBone")));

        ignore_unused(model->PlayAnim(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), nullptr, 0.0f, ModelAnimFlags::Freeze));
        model->StartMeshGeneration();

        for (ipos32 pivot : {ipos32 {64, 96}, ipos32 {23, 71}}) {
            model->SetupFrame(isize32 {128, 128}, pivot);
            model->PoseSpriteFrame(false);
            auto atlas_offset {model->GetBonePos(root_bone)};
            REQUIRE(atlas_offset);

            // The independently posed atlas is the reference, including authored/default layer transforms
            for (float32_t scene_scale : {0.5f, 2.0f}) {
                mat44 proj {client->SprMngr.GetRender().CreateOrthoMatrix(-50.0f, 200.0f, -75.0f, 175.0f, -10.0f, 10.0f)};
                model->DrawInScene(proj, scene_scale);
                auto direct_offset {model->GetBonePos(root_bone)};
                REQUIRE(direct_offset);
                CHECK(direct_offset->x == atlas_offset->x);
                CHECK(direct_offset->y == atlas_offset->y);
            }

            model->PoseSpriteFrame(false);

            CHECK(model->GetBonePos(root_bone) == atlas_offset);
        }
    }

    SECTION("BoneLocalPointsRotateAndKeepSpriteCoordinatesAfterDirectDraw")
    {
        hstring root_bone {client->Hashes.to_hashed_string(MESH_PATH)};
        model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
        model->StartMeshGeneration();
        auto point {vec3 {0.7f, 0.4f, 0.3f}};
        optional<ipos32> previous {};

        for (int16_t angle : {int16_t {0}, int16_t {90}, int16_t {180}}) {
            model->SetDir(mdir {angle}, false);
            model->PoseSpriteFrame(false);
            auto origin {model->GetBonePos(root_bone)};
            auto shifted {model->GetBonePos(root_bone, point)};
            REQUIRE(origin);
            REQUIRE(shifted);
            CHECK(shifted != origin);
            CHECK(model->GetBonePos(root_bone, vec3 {0.0f}) == origin);

            if (previous) {
                CHECK(shifted != previous);
            }

            previous = shifted;

            for (float32_t scene_scale : {0.5f, 2.0f}) {
                mat44 proj {client->SprMngr.GetRender().CreateOrthoMatrix(-50.0f, 200.0f, -75.0f, 175.0f, -10.0f, 10.0f)};
                model->DrawInScene(proj, scene_scale);

                CHECK(model->GetBonePos(root_bone, point) == shifted);
            }
        }

        CHECK_FALSE(model->GetBonePos(client->Hashes.to_hashed_string("MissingBone"), point));
        CHECK_FALSE(model->GetBonePos(root_bone, vec3 {std::numeric_limits<float32_t>::quiet_NaN(), 0.0f, 0.0f}));
        CHECK_FALSE(model->GetBonePos(root_bone, vec3 {0.0f, std::numeric_limits<float32_t>::infinity(), 0.0f}));
        CHECK_FALSE(model->GetBonePos(root_bone, vec3 {1.0e20f, 0.0f, 0.0f}));
        CHECK_FALSE(model->GetBonePos(root_bone, vec3 {-1.0e20f, 0.0f, 0.0f}));
    }

    SECTION("TheInstanceDrawsThroughTheHeadlessRenderer")
    {
        model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
        model->StartMeshGeneration();
        model->PoseSpriteFrame(true);

        // The Null renderer accepts the draw calls, so the mesh combine and submit paths run for real
        REQUIRE_NOTHROW(model->DrawSpriteFrame());

        mat44 proj {client->SprMngr.GetRender().CreateOrthoMatrix(0.0f, 128.0f, 0.0f, 128.0f, -10.0f, 10.0f)};
        REQUIRE_NOTHROW(model->DrawInScene(proj, 1.0f));

        REQUIRE_NOTHROW(model->PrewarmParticles());
        REQUIRE_NOTHROW(model->PoseSpriteFrame(false));
        REQUIRE_NOTHROW(model->DrawSpriteFrame());
    }

    SECTION("TheAnimationViewerPreviewsAModelBackedCritter")
    {
        // The viewer selects through a list click or through its persisted "last critter" setting, so the
        // setting is seeded and restored around the run
        string saved_selected_proto {};

        {
            SettingsStorage viewer_settings {"AnimationViewer"};
            saved_selected_proto = viewer_settings.GetString("SelectedProto");
            viewer_settings.SetString("SelectedProto", "UnitTestModelCritter");
        }

        auto restore_viewer_settings {scope_exit([&saved_selected_proto]() noexcept {
            safe_call([&saved_selected_proto] {
                SettingsStorage viewer_settings {"AnimationViewer"};
                viewer_settings.SetString("SelectedProto", saved_selected_proto);
            });
        })};

        REQUIRE(ImGui::GetCurrentContext() == nullptr);
        ImGuiExt::Init();

        auto destroy_context {scope_exit([]() noexcept {
            safe_call([] {
                if (ImGui::GetCurrentContext() != nullptr) {
                    ImGui::DestroyContext();
                }
            });
        })};

        ImGuiIO& io {ImGui::GetIO()};
        io.DisplaySize = ImVec2 {1280.0f, 720.0f};
        io.DeltaTime = 1.0f / 60.0f;
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

        AnimationViewer viewer {client.as_ptr(), &client->SprMngr, &client->ResMngr, &client->CritterFrames, &client->GameTime};
        viewer.SetVisible(true);
        viewer.SetFillViewport(true);

        for (int32_t frame {0}; frame < 4; frame++) {
            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(viewer.Draw());
            ImGui::LogFinish();
            ImGui::Render();
            std::this_thread::sleep_for(std::chrono::milliseconds {8});
        }

        CHECK(viewer.IsVisible());
    }

    SECTION("TheSpriteFactoryWrapsTheModelAsASprite")
    {
        // Going through the resource manager takes the model down the model-sprite path instead of the raw
        // instance one: atlas placement, per-frame update and the sprite-side draw
        shared_ptr<Sprite> sprite {client->ResMngr.LoadSprite(client->Hashes.to_hashed_string(MODEL_PATH), AtlasType::MapSprites, true)};
        REQUIRE(static_cast<bool>(sprite));

        CHECK(sprite->GetSize().width > 0);
        CHECK(sprite->GetSize().height > 0);

        REQUIRE_NOTHROW(sprite->Prewarm());
        REQUIRE_NOTHROW(sprite->PlayDefault());

        for (int32_t frame {0}; frame < 4; frame++) {
            REQUIRE_NOTHROW(sprite->Update());
            std::this_thread::sleep_for(std::chrono::milliseconds {8});
        }

        ignore_unused(sprite->IsHitTest(ipos32 {0, 0}));
        ignore_unused(sprite->IsPlaying());
        REQUIRE_NOTHROW(sprite->Stop());
    }
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ModelSpritePreparesItsHitMaskDuringRenderUpdate")
{
    constexpr string_view MESH_PATH {"Models/InitialHitMask.fbx"};
    constexpr string_view MODEL_PATH {"Models/InitialHitMask.fo3d"};

    vector<uint8_t> mesh_blob {MakeRuntimeModelTriangleMesh()};
    vector<pair<string, vector<uint8_t>>> model_resources {};
    model_resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    model_resources.emplace_back(string {MESH_PATH}, mesh_blob);
    model_resources.emplace_back(string {MODEL_PATH}, MakeRuntimeModelDescription(MODEL_PATH, MESH_PATH, mesh_blob));

    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(model_resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    shared_ptr<Sprite> sprite {client->ResMngr.LoadSprite(client->Hashes.to_hashed_string(MODEL_PATH), AtlasType::MapSprites, true)};
    auto model_spr {sprite.dyn_cast<ModelSprite>()};
    REQUIRE(static_cast<bool>(model_spr));
    REQUIRE(model_spr->Update());

    SECTION("FirstPictureIsPreparedBeforePicking")
    {
    }
    SECTION("PickingARedrawnPictureDoesNotStartAReadback")
    {
        model_spr->DrawToAtlas();
    }

    // Picking must use the completed mask even after a redraw. GPU work belongs to the render update
    ptr<RenderTexture> atlas_tex {model_spr->GetAtlas()->GetTexture()};
    ipos32 origin {iround<int32_t>(atlas_tex->SizeData[0] * model_spr->GetAtlasRect().x), iround<int32_t>(atlas_tex->SizeData[1] * model_spr->GetAtlasRect().y)};
    atlas_tex->UpdateTextureRegion(origin, {1, 1}, vector<ucolor>(1, ucolor {255, 255, 255, 255}));

    CHECK_FALSE(model_spr->IsHitTest({0, 0}));
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ModelSpriteHitTestReadsItsMaskFromTheAtlas")
{
    // A model sprite has no pixels on the CPU until its atlas picture comes back from the GPU. The null renderer reads
    // at once, which pins what the mask holds and when it is refreshed; the non-blocking backends only delay the answer
    constexpr string_view MESH_PATH {"Models/HitMask.fbx"};
    constexpr string_view MODEL_PATH {"Models/HitMask.fo3d"};

    vector<uint8_t> mesh_blob {MakeRuntimeModelTriangleMesh()};
    vector<pair<string, vector<uint8_t>>> model_resources {};
    model_resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    model_resources.emplace_back(string {MESH_PATH}, mesh_blob);
    model_resources.emplace_back(string {MODEL_PATH}, MakeRuntimeModelDescription(MODEL_PATH, MESH_PATH, mesh_blob));

    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(model_resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    shared_ptr<Sprite> sprite {client->ResMngr.LoadSprite(client->Hashes.to_hashed_string(MODEL_PATH), AtlasType::MapSprites, true)};
    auto model_spr {sprite.dyn_cast<ModelSprite>()};
    REQUIRE(static_cast<bool>(model_spr));

    // The null renderer draws nothing, so each picture is painted into the atlas right after the draw that claims it
    auto draw_with_opaque_pixel {[&model_spr](ipos32 opaque_pos) {
        model_spr->DrawToAtlas();

        isize32 size {model_spr->GetSize()};
        REQUIRE(size.width >= 2);
        REQUIRE(size.height >= 2);

        ptr<RenderTexture> atlas_tex {model_spr->GetAtlas()->GetTexture()};
        ipos32 origin {iround<int32_t>(atlas_tex->SizeData[0] * model_spr->GetAtlasRect().x), iround<int32_t>(atlas_tex->SizeData[1] * model_spr->GetAtlasRect().y)};
        vector<ucolor> pixels {vector<ucolor>(size.square<size_t>(), ucolor {0, 0, 0, 0})};
        pixels[numeric_cast<size_t>(opaque_pos.y) * numeric_cast<size_t>(size.width) + numeric_cast<size_t>(opaque_pos.x)] = ucolor {255, 255, 255, 255};
        atlas_tex->UpdateTextureRegion(origin, size, pixels);

        return origin;
    }};

    ipos32 first {0, 0};
    ipos32 second {1, 1};
    ipos32 origin {draw_with_opaque_pixel(first)};

    CHECK_FALSE(model_spr->IsHitTest(first));
    REQUIRE(model_spr->Update());
    CHECK(model_spr->IsHitTest(first));
    CHECK_FALSE(model_spr->IsHitTest(second));

    // Without a new picture the mask stands, so picking again reads nothing back, whatever the atlas holds now
    ptr<RenderTexture> atlas_tex {model_spr->GetAtlas()->GetTexture()};
    atlas_tex->UpdateTextureRegion(origin + second, {1, 1}, vector<ucolor>(1, ucolor {255, 255, 255, 255}));

    CHECK(model_spr->IsHitTest(first));
    CHECK_FALSE(model_spr->IsHitTest(second));

    // A redraw leaves the mask stale; picking only requests a copy for the next render update
    coarse_sleep(std::chrono::milliseconds {110});
    client->GameTime.FrameAdvance(false);
    draw_with_opaque_pixel(second);

    CHECK(model_spr->IsHitTest(first));
    CHECK_FALSE(model_spr->IsHitTest(second));
    REQUIRE(model_spr->Update());
    CHECK_FALSE(model_spr->IsHitTest(first));
    CHECK(model_spr->IsHitTest(second));
    CHECK_FALSE(model_spr->IsHitTest({-1, 0}));
    CHECK_FALSE(model_spr->IsHitTest({model_spr->GetSize().width, 0}));
}
#endif

TEST_CASE("ClientEngineStartsAndRegistersEntities")
{
    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings)};

    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK_FALSE(static_cast<bool>(client->GetCurPlayer()));
    CHECK_FALSE(static_cast<bool>(client->GetCurLocation()));
    CHECK_FALSE(static_cast<bool>(client->GetCurMap()));

    hstring critter_pid {client->Hashes.to_hashed_string("UnitTestClientCritter")};
    auto critter_proto {client->GetProtoCritter(critter_pid)};
    REQUIRE(static_cast<bool>(critter_proto));

    auto player {safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001})};
    auto critter {safe_alloc::make_refcounted<CritterView>(client, ident_t {1002}, critter_proto)};

    REQUIRE(client->GetEntity(player->GetId()) == player);
    REQUIRE(client->GetEntity(critter->GetId()) == critter);
    CHECK(critter->GetProtoId() == critter_pid);
    CHECK(critter->GetName() == "UnitTestClientCritter_1002");

    critter->DestroySelf();
    player->DestroySelf();

    CHECK_FALSE(static_cast<bool>(client->GetEntity(ident_t {1002})));
    CHECK_FALSE(static_cast<bool>(client->GetEntity(ident_t {1001})));
}

TEST_CASE("ClientCritterAttachedIdsSnapshotIncludesUnloadedIdsAndSurvivesListChanges")
{
    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings)};

    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    hstring critter_pid {client->Hashes.to_hashed_string("UnitTestClientCritter")};
    auto critter_proto {client->GetProtoCritter(critter_pid)};
    REQUIRE(static_cast<bool>(critter_proto));

    auto vehicle {safe_alloc::make_refcounted<CritterView>(client, ident_t {1002}, critter_proto)};
    auto driver {safe_alloc::make_refcounted<CritterView>(client, ident_t {11}, critter_proto)};

    CHECK(Client_Critter_GetAttachedCritterIds(vehicle).empty());

    // The passenger id replicates as a bare id, so its critter is deliberately never created on this client
    vehicle->SetAttachedCritters({driver->GetId(), ident_t {22}});
    CHECK_FALSE(static_cast<bool>(client->GetEntity(ident_t {22})));

    vector<ident_t> snapshot {Client_Critter_GetAttachedCritterIds(vehicle)};
    REQUIRE(snapshot.size() == 2);
    CHECK(snapshot[0] == driver->GetId());
    CHECK(snapshot[1] == ident_t {22});

    // Caller mutations leave the critter intact, and replacing its native list must preserve the old snapshot
    snapshot[0] = ident_t {9999};
    CHECK(vehicle->GetAttachedCritters()[0] == driver->GetId());

    vehicle->SetAttachedCritters({driver->GetId()});
    CHECK(snapshot[0] == ident_t {9999});
    CHECK(snapshot[1] == ident_t {22});
    CHECK(Client_Critter_GetAttachedCritterIds(vehicle).size() == 1);

    vehicle->SetAttachedCritters({});
    CHECK(snapshot.size() == 2);
    CHECK(Client_Critter_GetAttachedCritterIds(vehicle).empty());

    vehicle->DestroySelf();
    driver->DestroySelf();

    CHECK_FALSE(static_cast<bool>(client->GetEntity(ident_t {1002})));
    CHECK_FALSE(static_cast<bool>(client->GetEntity(ident_t {11})));
}

TEST_CASE("ClientEngineScheduledCallbacksDoNotRunNestedZeroDelayInSamePass")
{
    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings)};

    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    int32_t callback_count {0};

    client->ScheduleDelayedCallback(timespan::zero, [&client, &callback_count] {
        callback_count++;

        client->ScheduleDelayedCallback(timespan::zero, [&callback_count] { callback_count++; });
    });

    client->ProcessScheduledCallbacks();
    CHECK(callback_count == 1);

    client->ProcessScheduledCallbacks();
    CHECK(callback_count == 2);
}

TEST_CASE("FontManagerMeasuresAndSplitsText")
{
    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources(MakeUnitTestFontResources()))};

    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    client->FontMngr.BindFoFont(FontType::Default, "UnitTestFont.fofnt", AtlasType::IfaceSprites, false, false, 1.0f);

    SECTION("MetricsAnswerForABoundFont")
    {
        CHECK(client->FontMngr.GetLineHeight(FontType::Default) > 0);
        CHECK(client->FontMngr.GetLinesHeight({200, 100}, "one line", FontType::Default) > 0);
        CHECK(client->FontMngr.GetLinesCount({200, 100}, "one line", FontType::Default) == 1);
        CHECK(client->FontMngr.GetLinesCount({40, 200}, "a sentence long enough to wrap over several lines", FontType::Default) > 1);

        CHECK(client->FontMngr.HaveLetter(FontType::Default, uint32_t {'A'}));
        CHECK_FALSE(client->FontMngr.HaveLetter(FontType::Default, uint32_t {0x4E2D}));
    }

    SECTION("SplittingPaginatesWhatDoesNotFitTheRect")
    {
        // Splitting yields rect-sized pages, not individual lines, so text that fits comes back as one entry
        vector<string> single {client->FontMngr.SplitLines(irect32 {0, 0, 200, 100}, "one line", FontType::Default)};
        CHECK(single.size() == 1);

        vector<string> explicit_breaks {client->FontMngr.SplitLines(irect32 {0, 0, 200, 10}, "first\nsecond\nthird", FontType::Default)};
        CHECK(explicit_breaks.size() > 1);

        vector<string> wrapped {client->FontMngr.SplitLines(irect32 {0, 0, 40, 10}, "a sentence long enough to wrap over several lines", FontType::Default)};
        CHECK(wrapped.size() > 1);

        vector<string> empty {client->FontMngr.SplitLines(irect32 {0, 0, 200, 100}, "", FontType::Default)};
        CHECK(empty.empty());
    }

    SECTION("TheBinaryBmfLoaderProducesAUsableFont")
    {
        client->FontMngr.BindBmfFont(FontType::Default, "UnitTestFont.fnt", AtlasType::IfaceSprites, 1.0f);

        CHECK(client->FontMngr.GetLineHeight(FontType::Default) > 0);
        CHECK(client->FontMngr.HaveLetter(FontType::Default, uint32_t {'A'}));
        CHECK(client->FontMngr.GetLinesCount({200, 100}, "bmf line", FontType::Default) == 1);
    }

    SECTION("UnboundSlotsAndMissingFilesAreRejected")
    {
        CHECK_THROWS_AS(client->FontMngr.GetLineHeight(static_cast<FontType>(7)), FontManagerException);
        CHECK_THROWS_AS(client->FontMngr.BindFoFont(FontType::Default, "NoSuchFont.fofnt", AtlasType::IfaceSprites, false, false, 1.0f), FontManagerException);
        CHECK_THROWS_AS(client->FontMngr.BindBmfFont(FontType::Default, "NoSuchFont.fnt", AtlasType::IfaceSprites, 1.0f), FontManagerException);

        // Skip-if-loaded short-circuits before the resource is even looked up
        CHECK_NOTHROW(client->FontMngr.BindFoFont(FontType::Default, "NoSuchFont.fofnt", AtlasType::IfaceSprites, false, true, 1.0f));
    }

    SECTION("EffectOverrideAndTeardownAreAddressable")
    {
        CHECK_NOTHROW(client->FontMngr.SetFontEffect(FontType::Default, nullptr));
        CHECK_NOTHROW(client->FontMngr.ClearFonts());
        CHECK_THROWS_AS(client->FontMngr.GetLineHeight(FontType::Default), FontManagerException);
    }
}

TEST_CASE("MultiFrameSpritesPlayAndCopy")
{
    // A single-frame sprite resolves to an atlas sprite, so sheet playback has nothing to run on until the fixture
    // serves a real multi-frame one
    auto settings {MakeClientTestSettings()};

    vector<pair<string, vector<uint8_t>>> sprite_resources {};
    sprite_resources.emplace_back(string {"AnimSheet.png"}, BakerTests::MakeMultiFrameBakedSprite(4, 2, 2, 40));

    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(sprite_resources)))};

    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    auto sheet {client->ResMngr.LoadSprite(client->Hashes.to_hashed_string("AnimSheet.png"), AtlasType::IfaceSprites)};
    REQUIRE(sheet);
    CHECK(sheet->GetSize() == isize32 {2, 2});

    SECTION("PlaybackAdvancesThroughTheFramesAndWrapsWhenLooped")
    {
        REQUIRE_NOTHROW(sheet->PlayDefault());
        CHECK(sheet->IsPlaying());

        // The frame clock is driven by the game time, so the update is run over enough frames to wrap
        for (int32_t frame {0}; frame < 24; frame++) {
            client->GameTime.FrameAdvance(false);
            ignore_unused(sheet->Update());
        }

        CHECK(sheet->IsPlaying());

        // Seeking addresses a frame directly rather than waiting for the clock
        for (float32_t phase : {0.0f, 0.25f, 0.5f, 0.99f, 1.0f}) {
            REQUIRE_NOTHROW(sheet->SetPhase(phase));
            ignore_unused(sheet->Update());
        }

        // A one-shot run stops at the end instead of wrapping
        REQUIRE_NOTHROW(sheet->Play({}, false, false));

        for (int32_t frame {0}; frame < 24; frame++) {
            client->GameTime.FrameAdvance(false);
            ignore_unused(sheet->Update());
        }

        // Reversed playback walks the same frames the other way
        REQUIRE_NOTHROW(sheet->Play({}, true, true));

        for (int32_t frame {0}; frame < 12; frame++) {
            client->GameTime.FrameAdvance(false);
            ignore_unused(sheet->Update());
        }

        REQUIRE_NOTHROW(sheet->Stop());
        CHECK_FALSE(sheet->IsPlaying());
    }

    SECTION("ACopyPlaysIndependentlyOfTheSpriteItWasMadeFrom")
    {
        auto copy {sheet->MakeCopy()};
        REQUIRE(copy);
        CHECK(copy != sheet);

        copy->PlayDefault();
        CHECK(copy->IsPlaying());
        CHECK_FALSE(sheet->IsPlaying());

        for (int32_t frame {0}; frame < 12; frame++) {
            client->GameTime.FrameAdvance(false);
            ignore_unused(copy->Update());
        }

        CHECK(copy->IsPlaying());
    }
}

TEST_CASE("AtlasSpriteFillDataSupportsBakedMeshes")
{
    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings)};

    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    frect32 atlas_rect {0.25f, 0.5f, 0.5f, 0.25f};
    frect32 draw_rect {100.0f, 200.0f, 20.0f, 40.0f};
    ucolor color_left {10, 20, 30, 40};
    ucolor color_right {110, 120, 130, 140};

    SECTION("Absent mesh keeps the legacy quad")
    {
        auto sprite {safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {})};
        auto draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};

        size_t index_count {sprite->FillData(draw_buf, draw_rect, {color_left, color_right})};

        REQUIRE(index_count == 6);
        REQUIRE(draw_buf->VertCount == 4);
        REQUIRE(draw_buf->IndCount == 6);
        CHECK(draw_buf->Indices[0] == 0);
        CHECK(draw_buf->Indices[1] == 1);
        CHECK(draw_buf->Indices[2] == 3);
        CHECK(draw_buf->Indices[3] == 1);
        CHECK(draw_buf->Indices[4] == 2);
        CHECK(draw_buf->Indices[5] == 3);
        CHECK(draw_buf->Vertices[0].Color == color_left);
        CHECK(draw_buf->Vertices[1].Color == color_left);
        CHECK(draw_buf->Vertices[2].Color == color_right);
        CHECK(draw_buf->Vertices[3].Color == color_right);
    }

    SECTION("Explicit empty mesh emits no draw data")
    {
        auto sprite {safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, SpriteMeshData {})};
        auto draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};

        size_t index_count {sprite->FillData(draw_buf, draw_rect, {color_left, color_right})};

        CHECK(index_count == 0);
        CHECK_FALSE(sprite->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect).has_value());
        CHECK(draw_buf->VertCount == 0);
        CHECK(draw_buf->IndCount == 0);
    }

    SECTION("Mesh maps positions UVs indices and horizontal light colors")
    {
        SpriteMeshData mesh {};
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 10}, {10, 0}};
        mesh.Indices = {0, 1, 2};

        auto sprite {safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)})};
        auto draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};
        draw_buf->Vertices.resize(2);
        draw_buf->VertCount = 2;
        draw_buf->Indices.resize(1);
        draw_buf->Indices[0] = 0;
        draw_buf->IndCount = 1;

        size_t index_count {sprite->FillData(draw_buf, draw_rect, {color_left, color_right})};

        REQUIRE(index_count == 3);
        REQUIRE(draw_buf->VertCount == 5);
        REQUIRE(draw_buf->IndCount == 4);
        CHECK(draw_buf->Indices[1] == 2);
        CHECK(draw_buf->Indices[2] == 3);
        CHECK(draw_buf->Indices[3] == 4);

        const Vertex2D& left {draw_buf->Vertices[2]};
        const Vertex2D& center {draw_buf->Vertices[3]};
        const Vertex2D& right {draw_buf->Vertices[4]};
        CHECK(left.PosX == Catch::Approx(100.0f));
        CHECK(left.PosY == Catch::Approx(200.0f));
        CHECK(left.TexU == Catch::Approx(0.25f));
        CHECK(left.TexV == Catch::Approx(0.5f));
        CHECK(left.Color == color_left);
        CHECK(center.PosX == Catch::Approx(110.0f));
        CHECK(center.PosY == Catch::Approx(240.0f));
        CHECK(center.TexU == Catch::Approx(0.5f));
        CHECK(center.TexV == Catch::Approx(0.75f));
        CHECK(center.Color == (ucolor {60, 70, 80, 90}));
        CHECK(right.PosX == Catch::Approx(120.0f));
        CHECK(right.PosY == Catch::Approx(200.0f));
        CHECK(right.TexU == Catch::Approx(0.75f));
        CHECK(right.TexV == Catch::Approx(0.5f));
        CHECK(right.Color == color_right);
    }

    SECTION("Cropped mesh preserves source-relative horizontal light colors")
    {
        SpriteMeshData mesh {};
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {2, 0};
        mesh.Vertices = {{0, 0}, {3, 10}, {6, 0}};
        mesh.Indices = {0, 1, 2};

        auto sprite {safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {6, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)})};
        auto draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};

        CHECK(sprite->GetSize() == isize32 {10, 10});
        CHECK(sprite->GetOffset() == ipos32 {});
        REQUIRE(sprite->FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        REQUIRE(draw_buf->VertCount == 3);
        CHECK(draw_buf->Vertices[0].PosX == Catch::Approx(104.0f));
        CHECK(draw_buf->Vertices[1].PosX == Catch::Approx(110.0f));
        CHECK(draw_buf->Vertices[2].PosX == Catch::Approx(116.0f));
        CHECK(draw_buf->Vertices[0].TexU == Catch::Approx(0.25f));
        CHECK(draw_buf->Vertices[1].TexU == Catch::Approx(0.5f));
        CHECK(draw_buf->Vertices[2].TexU == Catch::Approx(0.75f));
        CHECK(draw_buf->Vertices[0].Color == (ucolor {30, 40, 50, 60}));
        CHECK(draw_buf->Vertices[1].Color == (ucolor {60, 70, 80, 90}));
        CHECK(draw_buf->Vertices[2].Color == (ucolor {90, 100, 110, 120}));
    }

    SECTION("Cropped mesh region preserves logical source coordinates")
    {
        SpriteMeshData mesh {};
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {2, 3};
        mesh.Vertices = {{0, 0}, {6, 0}, {0, 5}};
        mesh.Indices = {0, 1, 2};

        auto sprite {safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {6, 5}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)})};
        auto draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};
        optional<AtlasSpriteRegion> region {sprite->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect)};

        REQUIRE(region.has_value());
        CHECK(region->DrawRect.x == Catch::Approx(104.0f));
        CHECK(region->DrawRect.y == Catch::Approx(212.0f));
        CHECK(region->DrawRect.width == Catch::Approx(12.0f));
        CHECK(region->DrawRect.height == Catch::Approx(20.0f));
        CHECK(region->TextureRect.x == Catch::Approx(0.25f));
        CHECK(region->TextureRect.y == Catch::Approx(0.5f));
        CHECK(region->TextureRect.width == Catch::Approx(0.5f));
        CHECK(region->TextureRect.height == Catch::Approx(0.25f));
        REQUIRE(sprite->FillRegionData(draw_buf, {0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect, color_left) == 6);
        REQUIRE(draw_buf->VertCount == 4);
        CHECK(draw_buf->Vertices[0].PosX == Catch::Approx(104.0f));
        CHECK(draw_buf->Vertices[0].PosY == Catch::Approx(232.0f));
        CHECK(draw_buf->Vertices[0].TexU == Catch::Approx(0.25f));
        CHECK(draw_buf->Vertices[0].TexV == Catch::Approx(0.75f));
        CHECK(draw_buf->Vertices[1].PosX == Catch::Approx(104.0f));
        CHECK(draw_buf->Vertices[1].PosY == Catch::Approx(212.0f));
        CHECK(draw_buf->Vertices[1].TexU == Catch::Approx(0.25f));
        CHECK(draw_buf->Vertices[1].TexV == Catch::Approx(0.5f));
        CHECK(draw_buf->Vertices[2].PosX == Catch::Approx(116.0f));
        CHECK(draw_buf->Vertices[2].PosY == Catch::Approx(212.0f));
        CHECK(draw_buf->Vertices[2].TexU == Catch::Approx(0.75f));
        CHECK(draw_buf->Vertices[2].TexV == Catch::Approx(0.5f));
    }

    SECTION("Expanded mesh region clips atlas padding outside the logical source")
    {
        SpriteMeshData mesh {};
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {-2, -1};
        mesh.Vertices = {{0, 0}, {14, 0}, {0, 13}};
        mesh.Indices = {0, 1, 2};

        auto sprite {safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {14, 13}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)})};
        auto draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};
        optional<AtlasSpriteRegion> region {sprite->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect)};

        REQUIRE(region.has_value());
        CHECK(region->DrawRect.x == Catch::Approx(100.0f));
        CHECK(region->DrawRect.y == Catch::Approx(200.0f));
        CHECK(region->DrawRect.width == Catch::Approx(20.0f));
        CHECK(region->DrawRect.height == Catch::Approx(40.0f));
        CHECK(region->TextureRect.x == Catch::Approx(0.25f + 0.5f * 2.0f / 14.0f));
        CHECK(region->TextureRect.y == Catch::Approx(0.5f + 0.25f / 13.0f));
        CHECK(region->TextureRect.width == Catch::Approx(0.5f * 10.0f / 14.0f));
        CHECK(region->TextureRect.height == Catch::Approx(0.25f * 10.0f / 13.0f));
        REQUIRE(sprite->FillRegionData(draw_buf, {0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect, color_left) == 6);
        REQUIRE(draw_buf->VertCount == 4);
        CHECK(draw_buf->Vertices[0].PosX == Catch::Approx(100.0f));
        CHECK(draw_buf->Vertices[0].PosY == Catch::Approx(240.0f));
        CHECK(draw_buf->Vertices[0].TexU == Catch::Approx(0.25f + 0.5f * 2.0f / 14.0f));
        CHECK(draw_buf->Vertices[0].TexV == Catch::Approx(0.5f + 0.25f * 11.0f / 13.0f));
        CHECK(draw_buf->Vertices[2].PosX == Catch::Approx(120.0f));
        CHECK(draw_buf->Vertices[2].PosY == Catch::Approx(200.0f));
        CHECK(draw_buf->Vertices[2].TexU == Catch::Approx(0.25f + 0.5f * 12.0f / 14.0f));
        CHECK(draw_buf->Vertices[2].TexV == Catch::Approx(0.5f + 0.25f / 13.0f));
    }

    SECTION("Partial logical region intersects the cropped atlas frame")
    {
        SpriteMeshData mesh {};
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {2, 3};
        mesh.Vertices = {{0, 0}, {6, 0}, {0, 5}};
        mesh.Indices = {0, 1, 2};

        auto sprite {safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {6, 5}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)})};
        optional<AtlasSpriteRegion> region {sprite->ResolveRegion({0.1f, 0.2f}, {0.5f, 0.6f}, draw_rect)};

        REQUIRE(region.has_value());
        CHECK(region->DrawRect.x == Catch::Approx(105.0f));
        CHECK(region->DrawRect.y == Catch::Approx(210.0f));
        CHECK(region->DrawRect.width == Catch::Approx(15.0f));
        CHECK(region->DrawRect.height == Catch::Approx(30.0f));
        CHECK(region->TextureRect.x == Catch::Approx(0.25f));
        CHECK(region->TextureRect.y == Catch::Approx(0.5f));
        CHECK(region->TextureRect.width == Catch::Approx(0.25f));
        CHECK(region->TextureRect.height == Catch::Approx(0.15f));
    }

    SECTION("Live atlas allocation observes sprite mesh metadata for dump lifetime")
    {
        TextureAtlasLayout layout {{12, 12}};
        auto atlas_allocation {layout.Allocate({12, 12})};
        REQUIRE(atlas_allocation);
        nptr<TextureAtlasLayout::Allocation> allocation_observer {atlas_allocation.as_nptr()};
        SpriteMeshData mesh {};
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 5}, {10, 0}};
        mesh.Indices = {0, 1, 2};

        {
            auto sprite {safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, std::move(atlas_allocation), atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)})};
            auto draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};

            REQUIRE(allocation_observer->GetSpriteMesh());
            CHECK(allocation_observer->GetSpriteMesh()->Vertices.size() == 3);
            CHECK(sprite->FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        }

        CHECK_FALSE(allocation_observer->IsActive());
        CHECK_FALSE(allocation_observer->GetSpriteMesh());
    }

    SECTION("Moving an atlas sprite rebinds the allocation mesh observer")
    {
        TextureAtlasLayout layout {{12, 12}};
        auto atlas_allocation {layout.Allocate({12, 12})};
        REQUIRE(atlas_allocation);
        nptr<TextureAtlasLayout::Allocation> allocation_observer {atlas_allocation.as_nptr()};
        SpriteMeshData mesh {};
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 5}, {10, 0}};
        mesh.Indices = {0, 1, 2};

        {
            AtlasSprite source {&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, std::move(atlas_allocation), atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)}};
            nptr<const SpriteMeshData> source_mesh {allocation_observer->GetSpriteMesh()};
            AtlasSprite moved {std::move(source)};

            REQUIRE(allocation_observer->GetSpriteMesh());
            CHECK(allocation_observer->GetSpriteMesh() != source_mesh);
            auto draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};
            CHECK(moved.FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        }

        CHECK_FALSE(allocation_observer->IsActive());
        CHECK_FALSE(allocation_observer->GetSpriteMesh());
    }
}

TEST_CASE("DefaultSpriteFactoryValidatesBakedMeshPayload")
{
    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings)};

    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    SpriteMeshData mesh {};
    mesh.SourceSize = {2, 2};
    mesh.Vertices = {{0, 0}, {2, 0}, {0, 2}};
    mesh.Indices = {0, 1, 2};

    vector<uint8_t> valid_blob {BakerTests::MakeMinimalBakedSprite(2, 2, SpriteMeshKind::Mesh, mesh)};
    SpriteMeshData cropped_mesh {};
    cropped_mesh.SourceSize = {4, 3};
    cropped_mesh.SourceOffset = {1, -1};
    cropped_mesh.Vertices = {{0, 0}, {3, 0}, {0, 3}};
    cropped_mesh.Indices = {0, 1, 2};
    static_assert(SPRITE_RESOURCE_VERSION == 3);
    constexpr size_t mesh_kind_offset {30 + 2 * 2 * sizeof(ucolor)};
    constexpr size_t mesh_vertex_count_offset {mesh_kind_offset + 1};
    constexpr size_t mesh_index_count_offset {mesh_vertex_count_offset + sizeof(uint16_t)};
    constexpr size_t mesh_source_size_offset {mesh_index_count_offset + sizeof(uint32_t)};
    constexpr size_t mesh_source_offset_offset {mesh_source_size_offset + sizeof(uint16_t) * 2};
    constexpr size_t mesh_vertices_offset {mesh_source_offset_offset + sizeof(int32_t) * 2};
    constexpr size_t mesh_indices_offset {mesh_vertices_offset + 3 * sizeof(uint16_t) * 2};

    auto write_u16 {[](vector<uint8_t>& data, size_t offset, uint16_t value) {
        data[offset] = numeric_cast<uint8_t>(value & 0xFF);
        data[offset + 1] = numeric_cast<uint8_t>(value >> 8);
    }};

    auto write_u32 {[](vector<uint8_t>& data, size_t offset, uint32_t value) {
        data[offset] = numeric_cast<uint8_t>(value & 0xFF);
        data[offset + 1] = numeric_cast<uint8_t>((value >> 8) & 0xFF);
        data[offset + 2] = numeric_cast<uint8_t>((value >> 16) & 0xFF);
        data[offset + 3] = numeric_cast<uint8_t>(value >> 24);
    }};

    auto source {safe_alloc::make_unique<BakerTests::MemoryDataSource>("PolygonSpriteResources")};
    source->AddFile("Quad.png", BakerTests::MakeMinimalBakedSprite(2, 2));
    source->AddFile("Empty.png", BakerTests::MakeMinimalBakedSprite(2, 2, SpriteMeshKind::Empty));
    source->AddFile("ValidMesh.png", valid_blob);
    source->AddFile("CroppedMesh.png", BakerTests::MakeMinimalBakedSprite(3, 3, SpriteMeshKind::Mesh, cropped_mesh));

    vector<uint8_t> bad_version {valid_blob};
    bad_version[1]++;
    source->AddFile("BadVersion.png", std::move(bad_version));

    vector<uint8_t> bad_kind {valid_blob};
    bad_kind[mesh_kind_offset] = 0xFF;
    source->AddFile("BadKind.png", std::move(bad_kind));

    vector<uint8_t> bad_vertex_count {valid_blob};
    write_u16(bad_vertex_count, mesh_vertex_count_offset, uint16_t {2});
    source->AddFile("BadVertexCount.png", std::move(bad_vertex_count));

    vector<uint8_t> bad_index_count {valid_blob};
    write_u32(bad_index_count, mesh_index_count_offset, uint32_t {4});
    source->AddFile("BadIndexCount.png", std::move(bad_index_count));

    vector<uint8_t> implausible_index_count {valid_blob};
    write_u32(implausible_index_count, mesh_index_count_offset, uint32_t {21});
    source->AddFile("ImplausibleIndexCount.png", std::move(implausible_index_count));

    vector<uint8_t> bad_source_size {valid_blob};
    write_u16(bad_source_size, mesh_source_size_offset, uint16_t {0});
    source->AddFile("BadSourceSize.png", std::move(bad_source_size));

    vector<uint8_t> bad_source_offset {valid_blob};
    write_u32(bad_source_offset, mesh_source_offset_offset, uint32_t {2});
    source->AddFile("BadSourceOffset.png", std::move(bad_source_offset));

    vector<uint8_t> bad_coordinate {valid_blob};
    write_u16(bad_coordinate, mesh_vertices_offset, uint16_t {3});
    source->AddFile("BadCoordinate.png", std::move(bad_coordinate));

    vector<uint8_t> bad_index {valid_blob};
    write_u16(bad_index, mesh_indices_offset, uint16_t {3});
    source->AddFile("BadIndex.png", std::move(bad_index));

    vector<uint8_t> degenerate_triangle {valid_blob};
    write_u16(degenerate_triangle, mesh_vertices_offset + 2 * sizeof(uint16_t) * 2, uint16_t {1});
    write_u16(degenerate_triangle, mesh_vertices_offset + 2 * sizeof(uint16_t) * 2 + sizeof(uint16_t), uint16_t {0});
    source->AddFile("DegenerateTriangle.png", std::move(degenerate_triangle));

    SpriteMeshData inconsistent_winding_mesh {};
    inconsistent_winding_mesh.Vertices = {{0, 0}, {2, 0}, {0, 2}, {2, 2}};
    inconsistent_winding_mesh.Indices = {0, 1, 2, 1, 2, 3};
    source->AddFile("InconsistentWinding.png", BakerTests::MakeMinimalBakedSprite(2, 2, SpriteMeshKind::Mesh, inconsistent_winding_mesh));

    vector<uint8_t> trailing_data {valid_blob};
    trailing_data.emplace_back(uint8_t {0});
    source->AddFile("TrailingData.png", std::move(trailing_data));

    vector<uint8_t> truncated_payload {valid_blob};
    truncated_payload.resize(mesh_indices_offset);
    source->AddFile("TruncatedPayload.png", std::move(truncated_payload));

    client->ResMngr.GetResources()->AddCustomSource(std::move(source));
    DefaultSpriteFactory factory {&client->SprMngr, &client->ResMngr};
    auto load {[&client, &factory](string_view path) { return factory.LoadSprite(client->Hashes.to_hashed_string(path), AtlasType::MapSprites); }};

    auto valid_sprite {load("ValidMesh.png")};
    REQUIRE(static_cast<bool>(valid_sprite));
    auto valid_draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};
    CHECK(valid_sprite->FillData(valid_draw_buf, frect32 {0.0f, 0.0f, 2.0f, 2.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 3);

    auto cropped_sprite {load("CroppedMesh.png")};
    REQUIRE(cropped_sprite);
    CHECK(cropped_sprite->GetSize() == cropped_mesh.SourceSize);
    CHECK(cropped_sprite->GetOffset() == ipos32 {0, 1});
    CHECK_FALSE(cropped_sprite->IsHitTest({0, 0}));
    CHECK(cropped_sprite->IsHitTest({1, 0}));
    CHECK(cropped_sprite->IsHitTest({3, 1}));
    CHECK_FALSE(cropped_sprite->IsHitTest({3, 2}));
    auto cropped_draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};
    REQUIRE(cropped_sprite->FillData(cropped_draw_buf, frect32 {0.0f, 0.0f, 4.0f, 3.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 3);
    REQUIRE(cropped_draw_buf->VertCount == 3);
    CHECK(cropped_draw_buf->Vertices[0].PosX == Catch::Approx(1.0f));
    CHECK(cropped_draw_buf->Vertices[0].PosY == Catch::Approx(-1.0f));
    CHECK(cropped_draw_buf->Vertices[1].PosX == Catch::Approx(4.0f));
    CHECK(cropped_draw_buf->Vertices[1].PosY == Catch::Approx(-1.0f));
    CHECK(cropped_draw_buf->Vertices[2].PosX == Catch::Approx(1.0f));
    CHECK(cropped_draw_buf->Vertices[2].PosY == Catch::Approx(2.0f));

    auto restored_image {client->ResMngr.LoadSpriteAsQuad(client->Hashes.to_hashed_string("CroppedMesh.png"), AtlasType::IfaceSprites)};
    REQUIRE(restored_image);
    CHECK(restored_image->GetSize() == cropped_mesh.SourceSize);
    CHECK(restored_image->GetAtlasRect().width * restored_image->GetAtlas()->GetTexture()->SizeData[0] == Catch::Approx(4.0f));
    CHECK(restored_image->GetAtlasRect().height * restored_image->GetAtlas()->GetTexture()->SizeData[1] == Catch::Approx(3.0f));
    optional<AtlasSpriteRegion> restored_region {restored_image->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 4.0f, 3.0f})};
    REQUIRE(restored_region.has_value());
    CHECK(restored_region->DrawRect == frect32 {0.0f, 0.0f, 4.0f, 3.0f});
    CHECK(restored_region->TextureRect == restored_image->GetAtlasRect());
    auto restored_draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};
    CHECK(restored_image->FillData(restored_draw_buf, frect32 {0.0f, 0.0f, 4.0f, 3.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 6);

    auto quad_sprite {load("Quad.png")};
    REQUIRE(static_cast<bool>(quad_sprite));
    auto quad_draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};
    CHECK(quad_sprite->FillData(quad_draw_buf, frect32 {0.0f, 0.0f, 2.0f, 2.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 6);

    auto empty_sprite {load("Empty.png")};
    REQUIRE(static_cast<bool>(empty_sprite));
    auto empty_draw_buf {client->SprMngr.GetRender().CreateDrawBuffer(false)};
    CHECK(empty_sprite->FillData(empty_draw_buf, frect32 {0.0f, 0.0f, 2.0f, 2.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 0);

    CHECK_THROWS(load("BadVersion.png"));
    CHECK_THROWS(load("BadKind.png"));
    CHECK_THROWS(load("BadVertexCount.png"));
    CHECK_THROWS(load("BadIndexCount.png"));
    CHECK_THROWS(load("ImplausibleIndexCount.png"));
    CHECK_THROWS(load("BadSourceSize.png"));
    CHECK_THROWS(load("BadSourceOffset.png"));
    CHECK_THROWS(load("BadCoordinate.png"));
    CHECK_THROWS(load("BadIndex.png"));
    CHECK_THROWS(load("DegenerateTriangle.png"));
    CHECK_THROWS(load("InconsistentWinding.png"));
    CHECK_THROWS(load("TrailingData.png"));
    CHECK_THROWS(load("TruncatedPayload.png"));
}

TEST_CASE("SpriteManagerMapsPolygonAtlasPatternsAndPaddedEffects")
{
    SpriteMeshData mesh {};
    mesh.SourceSize = {4, 3};
    mesh.SourceOffset = {1, 0};
    mesh.Vertices = {{0, 0}, {3, 0}, {0, 3}};
    mesh.Indices = {0, 1, 2};

    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources({{"PatternMesh.png", BakerTests::MakeMinimalBakedSprite(3, 3, SpriteMeshKind::Mesh, mesh)}}))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    RecordingQuadEffect effect {};

    auto sprite {client->ResMngr.LoadSprite("PatternMesh.png", AtlasType::IfaceSprites)};
    REQUIRE(sprite);
    auto atlas_sprite {sprite.dyn_cast<AtlasSprite>()};
    REQUIRE(atlas_sprite);
    sprite->SetDrawEffect(make_nptr(&effect));

    client->SprMngr.DrawSpritePattern(sprite, {10, 20}, {7, 5}, {4, 3}, ucolor {255, 255, 255, 255});
    client->SprMngr.Flush();

    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& pattern_draw {effect.Draws.front()};
    REQUIRE(pattern_draw.Vertices.size() == 16);
    REQUIRE(pattern_draw.Indices.size() == 24);
    CHECK(pattern_draw.StartIndex == 0);
    REQUIRE(pattern_draw.IndicesToDraw.has_value());
    CHECK(*pattern_draw.IndicesToDraw == 24);
    CHECK(pattern_draw.CustomTexture == atlas_sprite->GetBatchTexture());

    frect32 atlas_rect {atlas_sprite->GetAtlasRect()};
    auto check_quad {[](const RecordedQuadDraw& draw, size_t vertex, frect32 draw_rect, frect32 texture_rect) {
        REQUIRE(vertex + 3 < draw.Vertices.size());
        CHECK(draw.Vertices[vertex + 0].PosX == Catch::Approx(draw_rect.x));
        CHECK(draw.Vertices[vertex + 0].PosY == Catch::Approx(draw_rect.y + draw_rect.height));
        CHECK(draw.Vertices[vertex + 0].TexU == Catch::Approx(texture_rect.x));
        CHECK(draw.Vertices[vertex + 0].TexV == Catch::Approx(texture_rect.y + texture_rect.height));
        CHECK(draw.Vertices[vertex + 1].PosX == Catch::Approx(draw_rect.x));
        CHECK(draw.Vertices[vertex + 1].PosY == Catch::Approx(draw_rect.y));
        CHECK(draw.Vertices[vertex + 1].TexU == Catch::Approx(texture_rect.x));
        CHECK(draw.Vertices[vertex + 1].TexV == Catch::Approx(texture_rect.y));
        CHECK(draw.Vertices[vertex + 2].PosX == Catch::Approx(draw_rect.x + draw_rect.width));
        CHECK(draw.Vertices[vertex + 2].PosY == Catch::Approx(draw_rect.y));
        CHECK(draw.Vertices[vertex + 2].TexU == Catch::Approx(texture_rect.x + texture_rect.width));
        CHECK(draw.Vertices[vertex + 2].TexV == Catch::Approx(texture_rect.y));
        CHECK(draw.Vertices[vertex + 3].PosX == Catch::Approx(draw_rect.x + draw_rect.width));
        CHECK(draw.Vertices[vertex + 3].PosY == Catch::Approx(draw_rect.y + draw_rect.height));
        CHECK(draw.Vertices[vertex + 3].TexU == Catch::Approx(texture_rect.x + texture_rect.width));
        CHECK(draw.Vertices[vertex + 3].TexV == Catch::Approx(texture_rect.y + texture_rect.height));
    }};

    check_quad(pattern_draw, 0, {11.0f, 20.0f, 3.0f, 3.0f}, atlas_rect);
    check_quad(pattern_draw, 4, {15.0f, 20.0f, 2.0f, 3.0f}, {atlas_rect.x, atlas_rect.y, atlas_rect.width * 2.0f / 3.0f, atlas_rect.height});
    check_quad(pattern_draw, 8, {11.0f, 23.0f, 3.0f, 2.0f}, {atlas_rect.x, atlas_rect.y, atlas_rect.width, atlas_rect.height * 2.0f / 3.0f});
    check_quad(pattern_draw, 12, {15.0f, 23.0f, 2.0f, 2.0f}, {atlas_rect.x, atlas_rect.y, atlas_rect.width * 2.0f / 3.0f, atlas_rect.height * 2.0f / 3.0f});

    effect.Draws.clear();
    REQUIRE(client->SprMngr.DrawSpriteRegion(sprite, {0.0f, 0.0f}, {1.0f, 1.0f}, {50.0f, 60.0f}, {4.0f, 3.0f}, ucolor {255, 255, 255, 255}));
    client->SprMngr.Flush();
    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& region_draw {effect.Draws.front()};
    REQUIRE(region_draw.Vertices.size() == 4);
    REQUIRE(region_draw.Indices.size() == 6);
    check_quad(region_draw, 0, {51.0f, 60.0f, 3.0f, 3.0f}, atlas_rect);

    effect.Draws.clear();
    effect.SpriteBorderBuf.reset();
    constexpr int32_t padding {2};
    client->SprMngr.DrawSpriteWithEffect(sprite, {100, 200}, ucolor {255, 255, 255, 255}, make_ptr(&effect), padding);

    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& effect_draw {effect.Draws.front()};
    REQUIRE(effect_draw.Vertices.size() == 4);
    REQUIRE(effect_draw.Indices.size() == 6);
    CHECK(effect_draw.StartIndex == 0);
    CHECK_FALSE(effect_draw.IndicesToDraw.has_value());
    CHECK(effect_draw.CustomTexture == atlas_sprite->GetBatchTexture());

    float32_t texture_padding_x {atlas_sprite->GetAtlas()->GetTexture()->SizeData[2] * numeric_cast<float32_t>(padding)};
    float32_t texture_padding_y {atlas_sprite->GetAtlas()->GetTexture()->SizeData[3] * numeric_cast<float32_t>(padding)};
    frect32 effect_draw_rect {99.0f, 198.0f, 7.0f, 7.0f};
    frect32 effect_texture_rect {atlas_rect.x - texture_padding_x, atlas_rect.y - texture_padding_y, atlas_rect.width + texture_padding_x * 2.0f, atlas_rect.height + texture_padding_y * 2.0f};

    CHECK(effect_draw.Vertices[0].PosX == Catch::Approx(effect_draw_rect.x));
    CHECK(effect_draw.Vertices[0].PosY == Catch::Approx(effect_draw_rect.y + effect_draw_rect.height));
    CHECK(effect_draw.Vertices[0].TexU == Catch::Approx(effect_texture_rect.x));
    CHECK(effect_draw.Vertices[0].TexV == Catch::Approx(effect_texture_rect.y + effect_texture_rect.height));
    CHECK(effect_draw.Vertices[1].PosX == Catch::Approx(effect_draw_rect.x));
    CHECK(effect_draw.Vertices[1].PosY == Catch::Approx(effect_draw_rect.y));
    CHECK(effect_draw.Vertices[1].TexU == Catch::Approx(effect_texture_rect.x));
    CHECK(effect_draw.Vertices[1].TexV == Catch::Approx(effect_texture_rect.y));
    CHECK(effect_draw.Vertices[2].PosX == Catch::Approx(effect_draw_rect.x + effect_draw_rect.width));
    CHECK(effect_draw.Vertices[2].PosY == Catch::Approx(effect_draw_rect.y));
    CHECK(effect_draw.Vertices[2].TexU == Catch::Approx(effect_texture_rect.x + effect_texture_rect.width));
    CHECK(effect_draw.Vertices[2].TexV == Catch::Approx(effect_texture_rect.y));
    CHECK(effect_draw.Vertices[3].PosX == Catch::Approx(effect_draw_rect.x + effect_draw_rect.width));
    CHECK(effect_draw.Vertices[3].PosY == Catch::Approx(effect_draw_rect.y + effect_draw_rect.height));
    CHECK(effect_draw.Vertices[3].TexU == Catch::Approx(effect_texture_rect.x + effect_texture_rect.width));
    CHECK(effect_draw.Vertices[3].TexV == Catch::Approx(effect_texture_rect.y + effect_texture_rect.height));

    REQUIRE(effect.SpriteBorderBuf.has_value());
    CHECK(effect.SpriteBorderBuf->SpriteBorder[0] == Catch::Approx(atlas_rect.x));
    CHECK(effect.SpriteBorderBuf->SpriteBorder[1] == Catch::Approx(atlas_rect.y));
    CHECK(effect.SpriteBorderBuf->SpriteBorder[2] == Catch::Approx(atlas_rect.x + atlas_rect.width));
    CHECK(effect.SpriteBorderBuf->SpriteBorder[3] == Catch::Approx(atlas_rect.y + atlas_rect.height));
    sprite->SetDrawEffect(nullptr);
}

TEST_CASE("SpriteManagerUnlitOverlayReplaysGuiRectangleAndTint")
{
    auto settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Render.Brightness, 0);

    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back("OverlayBase.png", BakerTests::MakeMinimalBakedSprite(8, 6));
    resources.emplace_back("OverlayLayer.png", BakerTests::MakeMinimalBakedSprite(3, 5));

    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    RecordingQuadEffect effect {};
    auto base_sprite {client->ResMngr.LoadSprite("OverlayBase.png", AtlasType::OneImage)};
    auto overlay_sprite {client->ResMngr.LoadSprite("OverlayLayer.png", AtlasType::OneImage)};

    REQUIRE(base_sprite);
    REQUIRE(overlay_sprite);
    REQUIRE(base_sprite->GetBatchTexture());
    REQUIRE(overlay_sprite->GetBatchTexture());
    REQUIRE(base_sprite->GetBatchTexture() != overlay_sprite->GetBatchTexture());

    SECTION("NoOverlayKeepsOneGuiQuad")
    {
        base_sprite->SetDrawEffect(make_nptr(&effect));

        client->SprMngr.DrawSprite(base_sprite, ipos32 {13, 29}, ucolor {40, 70, 90, 110});
        client->SprMngr.Flush();

        REQUIRE(effect.Draws.size() == 1);
        const RecordedQuadDraw& draw {effect.Draws.front()};

        REQUIRE(draw.Vertices.size() == 4);
        REQUIRE(draw.Indices.size() == 6);
        CHECK(draw.StartIndex == 0);
        REQUIRE(draw.IndicesToDraw.has_value());
        CHECK(*draw.IndicesToDraw == 6);
        CHECK(draw.CustomTexture == base_sprite->GetBatchTexture());

        CHECK(draw.Vertices[0].PosX == Catch::Approx(13.0f));
        CHECK(draw.Vertices[0].PosY == Catch::Approx(35.0f));
        CHECK(draw.Vertices[1].PosX == Catch::Approx(13.0f));
        CHECK(draw.Vertices[1].PosY == Catch::Approx(29.0f));
        CHECK(draw.Vertices[2].PosX == Catch::Approx(21.0f));
        CHECK(draw.Vertices[2].PosY == Catch::Approx(29.0f));
        CHECK(draw.Vertices[3].PosX == Catch::Approx(21.0f));
        CHECK(draw.Vertices[3].PosY == Catch::Approx(35.0f));
        CHECK(draw.Vertices[0].Color == (ucolor {40, 70, 90, 110}));
        CHECK(draw.Vertices[1].Color == (ucolor {40, 70, 90, 110}));
        CHECK(draw.Vertices[2].Color == (ucolor {40, 70, 90, 110}));
        CHECK(draw.Vertices[3].Color == (ucolor {40, 70, 90, 110}));
    }

    SECTION("DrawSpriteReplaysOverlayAtTheMainRectangle")
    {
        auto sprite {safe_alloc::make_shared<TestUnlitOverlaySprite>(&client->SprMngr, base_sprite, overlay_sprite)};

        sprite->SetDrawEffect(make_nptr(&effect));

        client->SprMngr.DrawSprite(sprite, ipos32 {23, 41}, ucolor {200, 120, 60, 77});
        client->SprMngr.Flush();

        REQUIRE(effect.Draws.size() == 2);
        vector<Vertex2D> main_vertices {ReadRecordedLayerQuad(effect.Draws[0])};
        vector<Vertex2D> overlay_vertices {ReadRecordedLayerQuad(effect.Draws[1])};

        CHECK(effect.Draws[0].CustomTexture == base_sprite->GetBatchTexture());
        CHECK(effect.Draws[1].CustomTexture == overlay_sprite->GetBatchTexture());
        CHECK(effect.Draws[0].CustomTexture != effect.Draws[1].CustomTexture);

        REQUIRE(main_vertices.size() == 4);
        REQUIRE(overlay_vertices.size() == 4);

        for (size_t i {0}; i < 4; i++) {
            CHECK(main_vertices[i].PosX == Catch::Approx(overlay_vertices[i].PosX));
            CHECK(main_vertices[i].PosY == Catch::Approx(overlay_vertices[i].PosY));
            CHECK(main_vertices[i].Color == (ucolor {200, 120, 60, 77}));
            CHECK(overlay_vertices[i].Color == (ucolor {200, 120, 60, 77}));
        }

        CHECK(main_vertices[0].PosX == Catch::Approx(23.0f));
        CHECK(main_vertices[0].PosY == Catch::Approx(47.0f));
        CHECK(main_vertices[1].PosX == Catch::Approx(23.0f));
        CHECK(main_vertices[1].PosY == Catch::Approx(41.0f));
        CHECK(main_vertices[2].PosX == Catch::Approx(31.0f));
        CHECK(main_vertices[2].PosY == Catch::Approx(41.0f));
        CHECK(main_vertices[3].PosX == Catch::Approx(31.0f));
        CHECK(main_vertices[3].PosY == Catch::Approx(47.0f));
    }

    SECTION("DrawSpriteSizeDelegatesOverlayToTheScaledRectangle")
    {
        auto sprite {safe_alloc::make_shared<TestUnlitOverlaySprite>(&client->SprMngr, base_sprite, overlay_sprite)};

        sprite->SetDrawEffect(make_nptr(&effect));

        client->SprMngr.DrawSpriteSize(sprite, ipos32 {5, 7}, isize32 {16, 12}, true, true, ucolor {90, 80, 70, 60});
        client->SprMngr.Flush();

        REQUIRE(effect.Draws.size() == 2);
        vector<Vertex2D> main_vertices {ReadRecordedLayerQuad(effect.Draws[0])};
        vector<Vertex2D> overlay_vertices {ReadRecordedLayerQuad(effect.Draws[1])};

        CHECK(effect.Draws[0].CustomTexture == base_sprite->GetBatchTexture());
        CHECK(effect.Draws[1].CustomTexture == overlay_sprite->GetBatchTexture());
        CHECK(effect.Draws[0].CustomTexture != effect.Draws[1].CustomTexture);

        REQUIRE(main_vertices.size() == 4);
        REQUIRE(overlay_vertices.size() == 4);

        for (size_t i {0}; i < 4; i++) {
            CHECK(main_vertices[i].PosX == Catch::Approx(overlay_vertices[i].PosX));
            CHECK(main_vertices[i].PosY == Catch::Approx(overlay_vertices[i].PosY));
            CHECK(main_vertices[i].Color == (ucolor {90, 80, 70, 60}));
            CHECK(overlay_vertices[i].Color == (ucolor {90, 80, 70, 60}));
        }

        CHECK(main_vertices[0].PosX == Catch::Approx(5.0f));
        CHECK(main_vertices[0].PosY == Catch::Approx(19.0f));
        CHECK(main_vertices[1].PosX == Catch::Approx(5.0f));
        CHECK(main_vertices[1].PosY == Catch::Approx(7.0f));
        CHECK(main_vertices[2].PosX == Catch::Approx(21.0f));
        CHECK(main_vertices[2].PosY == Catch::Approx(7.0f));
        CHECK(main_vertices[3].PosX == Catch::Approx(21.0f));
        CHECK(main_vertices[3].PosY == Catch::Approx(19.0f));
    }

    SECTION("DrawSpriteSizeExtScalesOverlayToTheMainRectangle")
    {
        auto sprite {safe_alloc::make_shared<TestUnlitOverlaySprite>(&client->SprMngr, base_sprite, overlay_sprite)};

        sprite->SetDrawEffect(make_nptr(&effect));

        client->SprMngr.DrawSpriteSizeExt(sprite, fpos32 {51.5f, 63.25f}, fsize32 {20.0f, 30.0f}, true, true, false, ucolor {10, 220, 140, 33});
        client->SprMngr.Flush();

        REQUIRE(effect.Draws.size() == 2);
        vector<Vertex2D> main_vertices {ReadRecordedLayerQuad(effect.Draws[0])};
        vector<Vertex2D> overlay_vertices {ReadRecordedLayerQuad(effect.Draws[1])};

        CHECK(effect.Draws[0].CustomTexture == base_sprite->GetBatchTexture());
        CHECK(effect.Draws[1].CustomTexture == overlay_sprite->GetBatchTexture());
        CHECK(effect.Draws[0].CustomTexture != effect.Draws[1].CustomTexture);

        REQUIRE(main_vertices.size() == 4);
        REQUIRE(overlay_vertices.size() == 4);

        for (size_t i {0}; i < 4; i++) {
            CHECK(main_vertices[i].PosX == Catch::Approx(overlay_vertices[i].PosX));
            CHECK(main_vertices[i].PosY == Catch::Approx(overlay_vertices[i].PosY));
            CHECK(main_vertices[i].Color == (ucolor {10, 220, 140, 33}));
            CHECK(overlay_vertices[i].Color == (ucolor {10, 220, 140, 33}));
        }

        CHECK(main_vertices[0].PosX == Catch::Approx(51.5f));
        CHECK(main_vertices[0].PosY == Catch::Approx(86.25f));
        CHECK(main_vertices[1].PosX == Catch::Approx(51.5f));
        CHECK(main_vertices[1].PosY == Catch::Approx(71.25f));
        CHECK(main_vertices[2].PosX == Catch::Approx(71.5f));
        CHECK(main_vertices[2].PosY == Catch::Approx(71.25f));
        CHECK(main_vertices[3].PosX == Catch::Approx(71.5f));
        CHECK(main_vertices[3].PosY == Catch::Approx(86.25f));
    }
}

TEST_CASE("SpriteManagerUnlitOverlayMapKeepsUnlitColorGeometryAndDepth")
{
    auto settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Render.Brightness, 0);
    BakerTests::OverrideSetting(settings.Geometry.MapCameraAngle, 30.0f);

    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back("MapOverlayBase.png", BakerTests::MakeMinimalBakedSprite(8, 8));
    resources.emplace_back("MapOverlayLayer.png", BakerTests::MakeMinimalBakedSprite(5, 11));

    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    RecordingQuadEffect effect {};
    auto base_sprite {client->ResMngr.LoadSprite("MapOverlayBase.png", AtlasType::OneImage)};
    auto overlay_sprite {client->ResMngr.LoadSprite("MapOverlayLayer.png", AtlasType::OneImage)};

    REQUIRE(base_sprite);
    REQUIRE(overlay_sprite);
    REQUIRE(base_sprite->GetBatchTexture());
    REQUIRE(overlay_sprite->GetBatchTexture());
    REQUIRE(base_sprite->GetBatchTexture() != overlay_sprite->GetBatchTexture());

    auto sprite {safe_alloc::make_shared<TestUnlitOverlaySprite>(&client->SprMngr, base_sprite, overlay_sprite)};

    sprite->SetDrawEffect(make_nptr(&effect));

    uint8_t map_alpha {150};
    ipos32 field_offset {100, 100};
    MapSpriteList list {};
    auto mspr {list.AddSprite(DrawOrderType::Item, mpos {3, 3}, ipos32 {12, -17}, &field_offset, sprite.get(), nullptr, nullptr, nullptr, &map_alpha, nullptr, nullptr, 0)};

    REQUIRE(mspr->GetAlpha());
    CHECK(*mspr->GetAlpha() == 150);

    client->SprMngr.SetEgg(TransparentEggSlot::Primary, mpos {3, 3}, fpos32 {50.0f, 50.0f}, fsize32 {30.0f, 20.0f}, TransparentEggTarget::AnyOccluder);
    client->SprMngr.SetEgg(TransparentEggSlot::Secondary, mpos {3, 3}, fpos32 {70.0f, 45.0f}, fsize32 {15.0f, 25.0f}, TransparentEggTarget::AnyOccluder);

    irect32 draw_area {0, 0, 320, 200};
    auto draw_layers {[&]() -> pair<vector<Vertex2D>, vector<Vertex2D>> {
        effect.Draws.clear();
        client->SprMngr.DrawSprites(list, draw_area, 0, true, DrawOrderType::Item, DrawOrderType::Item, ucolor {36, 24, 12, 255}, &effect);

        REQUIRE(effect.Draws.size() == 2);
        CHECK(effect.Draws[0].CustomTexture == base_sprite->GetBatchTexture());
        CHECK(effect.Draws[1].CustomTexture == overlay_sprite->GetBatchTexture());
        CHECK(effect.Draws[0].CustomTexture != effect.Draws[1].CustomTexture);

        vector<Vertex2D> main_vertices {ReadRecordedLayerQuad(effect.Draws[0])};
        vector<Vertex2D> overlay_vertices {ReadRecordedLayerQuad(effect.Draws[1])};

        REQUIRE(main_vertices.size() == 4);
        REQUIRE(overlay_vertices.size() == 4);

        return {std::move(main_vertices), std::move(overlay_vertices)};
    }};

    SECTION("NeutralOverlayBypassesDayLightAndPreservesAlpha")
    {
        auto layers {draw_layers()};

        for (size_t i {0}; i < 4; i++) {
            CHECK(layers.first[i].Color == (ucolor {36, 24, 12, 150}));
            CHECK(layers.second[i].Color == (ucolor {128, 128, 128, 150}));
        }
    }

    SECTION("LocalLightReachesOnlyTheMainLayer")
    {
        ucolor local_light {20, 30, 40, 255};
        array<ucolor, 256> light_samples {};

        for (auto& sample : light_samples) {
            sample = local_light;
        }

        // A uniform light buffer keeps the corner selection irrelevant while taking the local-light path
        mspr->SetLight(make_ptr(&light_samples[0]), make_ptr(&light_samples[0]), make_ptr(&light_samples[0]));

        auto layers {draw_layers()};

        for (size_t i {0}; i < 4; i++) {
            CHECK(layers.first[i].Color == (ucolor {56, 54, 52, 150}));
            CHECK(layers.second[i].Color == (ucolor {128, 128, 128, 150}));
        }
    }

    SECTION("ExplicitMapColorIsTheOverlayTint")
    {
        mspr->SetColor(ucolor {200, 90, 30, 255});
        mspr->SetAlpha(&map_alpha);

        auto layers {draw_layers()};

        for (size_t i {0}; i < 4; i++) {
            CHECK(layers.first[i].Color == (ucolor {200, 90, 30, 150}));
            CHECK(layers.second[i].Color == (ucolor {200, 90, 30, 150}));
        }
    }

    SECTION("RotationProjectionDepthAndBothEggSlotsCoverBothLayers")
    {
        mspr->SetEggAppearence(EggAppearenceType::Always);
        mspr->SetAngle(int16_t {30});
        mspr->SetMapProjected(true);

        auto transformed {draw_layers()};

        for (size_t i {0}; i < 4; i++) {
            CHECK(transformed.first[i].PosX == Catch::Approx(transformed.second[i].PosX));
            CHECK(transformed.first[i].PosY == Catch::Approx(transformed.second[i].PosY));
            CHECK(transformed.first[i].PosZ == Catch::Approx(transformed.second[i].PosZ));
            CHECK(transformed.first[i].EggFlags[0] == 1.0f);
            CHECK(transformed.first[i].EggFlags[1] == 1.0f);
            CHECK(transformed.second[i].EggFlags[0] == 1.0f);
            CHECK(transformed.second[i].EggFlags[1] == 1.0f);
        }

        mspr->SetAngle(int16_t {0});
        mspr->SetMapProjected(false);

        auto plain {draw_layers()};
        bool vertices_moved {false};

        for (size_t i {0}; i < 4; i++) {
            if (!is_float_equal(transformed.first[i].PosX, plain.first[i].PosX) || !is_float_equal(transformed.first[i].PosY, plain.first[i].PosY)) {
                vertices_moved = true;
            }
        }

        CHECK(vertices_moved);

        mspr->SetAngle(int16_t {30});
        mspr->SetMapProjected(true);
        mspr->SetDepthOffset(7.0f);

        auto depth {draw_layers()};

        for (size_t i {0}; i < 4; i++) {
            CHECK(depth.first[i].PosX == Catch::Approx(transformed.first[i].PosX));
            CHECK(depth.first[i].PosY == Catch::Approx(transformed.first[i].PosY));
            CHECK(depth.second[i].PosX == Catch::Approx(transformed.second[i].PosX));
            CHECK(depth.second[i].PosY == Catch::Approx(transformed.second[i].PosY));
            CHECK(depth.first[i].PosZ == Catch::Approx(transformed.first[i].PosZ + 7.0f));
            CHECK(depth.second[i].PosZ == Catch::Approx(transformed.second[i].PosZ + 7.0f));
            CHECK(depth.first[i].PosZ == Catch::Approx(depth.second[i].PosZ));
            CHECK(depth.first[i].EggFlags[0] == 1.0f);
            CHECK(depth.first[i].EggFlags[1] == 1.0f);
            CHECK(depth.second[i].EggFlags[0] == 1.0f);
            CHECK(depth.second[i].EggFlags[1] == 1.0f);
        }
    }

    SECTION("HiddenMapSpriteEmitsNeitherLayer")
    {
        effect.Draws.clear();
        client->SprMngr.DrawSprites(list, draw_area, 0, true, DrawOrderType::Item, DrawOrderType::Item, ucolor {36, 24, 12, 255}, &effect);

        REQUIRE(effect.Draws.size() == 2);

        mspr->SetHidden(true);

        effect.Draws.clear();
        client->SprMngr.DrawSprites(list, draw_area, 0, true, DrawOrderType::Item, DrawOrderType::Item, ucolor {36, 24, 12, 255}, &effect);

        CHECK(effect.Draws.empty());
    }
}

TEST_CASE("SpriteWireframeRendersThroughPrimitiveOverlay")
{
    auto settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Render.DrawWireframe, true);
    auto client {MakeClientEngine(settings)};

    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    SpriteMeshData mesh {};
    mesh.SourceSize = {10, 10};
    mesh.Vertices = {{0, 0}, {5, 10}, {10, 0}};
    mesh.Indices = {0, 1, 2};

    auto [atlas, atlas_allocation, atlas_pos] {client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {10, 10})};
    CHECK(atlas->GetSize() == isize32 {12, 12});
    CHECK(atlas_pos == ipos32 {1, 1});
    CHECK(atlas_allocation->GetPosition() == ipos32 {0, 0});
    CHECK(atlas_allocation->GetSize() == isize32 {12, 12});
    frect32 sprite_atlas_rect {
        numeric_cast<float32_t>(atlas_pos.x) / numeric_cast<float32_t>(atlas->GetSize().width),
        numeric_cast<float32_t>(atlas_pos.y) / numeric_cast<float32_t>(atlas->GetSize().height),
        10.0f / numeric_cast<float32_t>(atlas->GetSize().width),
        10.0f / numeric_cast<float32_t>(atlas->GetSize().height),
    };
    auto sprite {safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, atlas, std::move(atlas_allocation), sprite_atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)})};

    client->SprMngr.DrawSprite(sprite, {2, 3}, ucolor {255, 255, 255});
    CHECK_NOTHROW(client->SprMngr.Flush());
}

TEST_CASE("ClientEngineAnimationResolverReportsCleanupExceptions")
{
    class ThrowingStopSprite final : public Sprite
    {
    public:
        ThrowingStopSprite(ptr<SpriteManager> spr_mngr, shared_ptr<Sprite> copy) :
            Sprite(spr_mngr, copy->GetSize(), copy->GetOffset()),
            Copy {std::move(copy)}
        {
        }

        auto MakeCopy() const -> shared_ptr<Sprite> override
        {
            if (ThrowOnCopy) {
                throw InvalidCallException("Animation copy failed");
            }

            return Copy->MakeCopy();
        }

        auto FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t override { return Copy->FillData(dbuf, pos, colors); }

        void Stop() override
        {
            StopCalls++;

            if (ThrowOnStop) {
                throw InvalidCallException("Animation stop failed");
            }
        }

        shared_ptr<Sprite> Copy;
        bool ThrowOnCopy {};
        bool ThrowOnStop {true};
        int32_t StopCalls {};
    };

    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings, MakeClientTestResources({{"Sequence.fofrm", BakerTests::MakeMultiFrameBakedSprite(3, 2, 2, 120)}}))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto source {safe_alloc::make_shared<ThrowingStopSprite>(&client->SprMngr, client->ResMngr.LoadSprite("Sequence.fofrm", AtlasType::MapSprites))};
    uint32_t spr_id {client->AnimAdd(source)};
    bool resolved {true};
    Entity::EventCallbackData callback {};
    callback.Callback = [&spr_id, &resolved](FuncCallData& call) {
        *cast_from_void<uint32_t*>(call.ArgsData[3]) = spr_id;
        return resolved ? Entity::EventResult::ContinueChain : Entity::EventResult::StopChain;
    };

    callback.SubscriptionPtr = reinterpret_cast<uintptr_t>(&spr_id);
    callback.HasExplicitResult = true;
    client->OnCritterAnimationSprite.Subscribe(std::move(callback));
    auto unsubscribe {scope_exit([&client, &spr_id]() noexcept { client->OnCritterAnimationSprite.Unsubscribe(reinterpret_cast<uintptr_t>(&spr_id)); })};
    auto resolve {[&] { return client->ResolveCritterAnimationSprite(client->Hashes.to_hashed_string("ThrowingStop"), CritterStateAnim {}, CritterActionAnim {}); }};

    SECTION("SuccessfulResolution")
    {
        auto sprite {resolve()};
        REQUIRE(sprite);
        CHECK(sprite->GetFramesCount() == 3);
    }

    SECTION("CopyFailurePreservesTheOriginalException")
    {
        source->ThrowOnCopy = true;
        CHECK_THROWS_MATCHES(resolve(), InvalidCallException, Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("Animation copy failed")));
    }

    SECTION("StoppedEventStillCleansUp")
    {
        resolved = false;
        CHECK_FALSE(resolve());
    }

    SECTION("DirectFreeReleasesTheHandleWhenStopThrows")
    {
        CHECK_THROWS_MATCHES(client->AnimFree(spr_id), InvalidCallException, Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("Animation stop failed")));
    }

    CHECK(source->StopCalls == 1);
    CHECK_FALSE(client->AnimGetSpr(spr_id));

    source->ThrowOnStop = false;
    client->AnimFree(spr_id);
    CHECK_FALSE(client->AnimGetSpr(spr_id));
}

TEST_CASE("ClientEngineSpriteCleanupReleasesUnusedFrames")
{
    class SpriteCleanupClient final : public ClientEngine
    {
    public:
        using ClientEngine::CleanupSpriteCache;
        using ClientEngine::ClientEngine;
    };

    auto settings {MakeClientTestSettings()};
    auto client {safe_alloc::make_refcounted<SpriteCleanupClient>(&settings,
        MakeClientTestResources({
            {"Released.fofrm", BakerTests::MakeMultiFrameBakedSprite(3, 2, 2, 120)},
            {"Active.fofrm", BakerTests::MakeMultiFrameBakedSprite(3, 2, 2, 120)},
        }),
        &GetApp()->MainWindow)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    uint32_t released_id {client->AnimLoad(client->Hashes.to_hashed_string("Released.fofrm"), AtlasType::MapSprites)};
    auto released {client->AnimGetSpr(released_id).dyn_cast<SpriteSheet>()};
    REQUIRE(released);
    auto frame_owner {released->GetSprite(0)->weak_from_this()};
    client->AnimFree(released_id);
    CHECK(frame_owner.lock());

    uint32_t active_id {client->AnimLoad(client->Hashes.to_hashed_string("Active.fofrm"), AtlasType::MapSprites)};
    client->CleanupSpriteCache();
    CHECK_FALSE(frame_owner.lock());
    REQUIRE(client->AnimGetSpr(active_id));
    CHECK(client->AnimGetSpr(active_id)->GetSize() == isize32 {2, 2});
    client->AnimFree(active_id);
}

TEST_CASE("CritterFramesCacheReentrantResolutionUsesCachedResult")
{
    class ObservedCopySprite final : public Sprite
    {
    public:
        ObservedCopySprite(ptr<SpriteManager> spr_mngr, shared_ptr<Sprite> source, ptr<shared_ptr<Sprite>> observed_copy) :
            Sprite(spr_mngr, source->GetSize(), source->GetOffset()),
            _source {std::move(source)},
            _observedCopy {observed_copy}
        {
        }

        auto MakeCopy() const -> shared_ptr<Sprite> override
        {
            auto copy {_source->MakeCopy()};
            *_observedCopy = copy;
            return copy;
        }

        auto FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t override { return _source->FillData(dbuf, pos, colors); }

    private:
        shared_ptr<Sprite> _source;

        // Const copies still publish their result to the test observer slot
        mutable ptr<shared_ptr<Sprite>> _observedCopy;
    };

    auto settings {MakeClientTestSettings()};
    auto client {MakeClientEngine(settings,
        MakeClientTestResources({
            {"Inner.fofrm", BakerTests::MakeMultiFrameBakedSprite(2, 2, 2, 120)},
            {"Outer.fofrm", BakerTests::MakeMultiFrameBakedSprite(3, 2, 2, 180)},
        }))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    bool nested_missing {false};

    SECTION("AnAlreadyCachedSheetWins")
    {
    }

    SECTION("AnAlreadyCachedMissingResultWins")
    {
        nested_missing = true;
    }

    hstring model_name {client->Hashes.to_hashed_string("ReentrantResolution")};
    CritterStateAnim state_anim {static_cast<CritterStateAnim>(401)};
    CritterActionAnim action_anim {static_cast<CritterActionAnim>(509)};
    auto inner_source {client->ResMngr.LoadSprite("Inner.fofrm", AtlasType::MapSprites)};
    REQUIRE(inner_source);
    shared_ptr<Sprite> observed_outer_copy {};
    auto outer_source {safe_alloc::make_shared<ObservedCopySprite>(&client->SprMngr, client->ResMngr.LoadSprite("Outer.fofrm", AtlasType::MapSprites), make_ptr(&observed_outer_copy))};
    nptr<const SpriteSheet> nested {};
    bool resolving {false};
    client->OnCritterAnimationSprite.UnsubscribeAll();
    Entity::EventCallbackData callback {};
    callback.Callback = [&](FuncCallData& call) {
        if (resolving) {
            *cast_from_void<uint32_t*>(call.ArgsData[3]) = nested_missing ? 0 : client->AnimAdd(inner_source);
        }
        else {
            resolving = true;
            nested = client->CritterFrames.GetCritterAnimFrames(model_name, state_anim, action_anim, mdir {0});
            resolving = false;
            *cast_from_void<uint32_t*>(call.ArgsData[3]) = client->AnimAdd(outer_source);
        }

        return Entity::EventResult::ContinueChain;
    };

    client->OnCritterAnimationSprite.Subscribe(std::move(callback));
    auto unsubscribe {scope_exit([&client]() noexcept { client->OnCritterAnimationSprite.UnsubscribeAll(); })};
    auto result {client->CritterFrames.GetCritterAnimFrames(model_name, state_anim, action_anim, mdir {0})};

    // Keep the rejected outer copy alive so the wrong implementation fails without dereferencing freed memory
    REQUIRE(observed_outer_copy);
    CHECK(result == nested);
    CHECK(client->CritterFrames.GetCritterAnimFrames(model_name, state_anim, action_anim, mdir {0}) == nested);

    if (nested_missing) {
        CHECK_FALSE(result);
    }
    else {
        REQUIRE(result);
        CHECK(result->GetFramesCount() == 2);
    }
}

#if FO_ENABLE_3D

TEST_CASE("ModelAnimationUsesProjectLocomotionAndFrozenPose")
{
    constexpr string_view mesh_path {"Models/RuntimeInstance.fbx"};
    constexpr string_view model_path {"Models/RuntimeInstance.fo3d"};
    vector<uint8_t> mesh {MakeSkinnedRuntimeModelMesh()};
    vector<uint8_t> animation_info {};
    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back(string(mesh_path), mesh);
    resources.emplace_back(string(model_path), MakeRuntimeModelDescription(model_path, mesh_path, mesh, "RotationBone Limb\nAnim 401 509 ModelFile Base\n", {}, nullptr, true, {}, 2.0f, &animation_info));
    resources.emplace_back("ModelAnimationInfo.foinfo", std::move(animation_info));
    auto proto_source {safe_alloc::make_unique<BakerTests::MemoryDataSource>("PolicyMapMetadata")};
    proto_source->AddFile("Metadata.fometa-client", BakerTests::MakeEmptyMetadataBlob());
    FileSystem proto_resources {};
    proto_resources.AddCustomSource(std::move(proto_source));
    BakerClientEngine proto_engine {proto_resources};
    vector<pair<string, function<void(ProtoMap&)>>> maps {};
    maps.emplace_back("PolicyMap", [](ProtoMap& proto) { proto.SetSize(msize {20, 20}); });
    resources.emplace_back("PolicyMap.fopro-bin-client", BakerTests::MakeMultiProtoResourceBlob<ProtoMap>(proto_engine, proto_engine.Hashes.to_hashed_string("Map"), maps));
    auto settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Render.LegBones, vector<string> {"Limb"});
    BakerTests::OverrideSetting(settings.Timer.DeltaTimeCap, 650);

    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);
    auto model {factory->GetModelMngr()->CreateModel(model_path)};
    REQUIRE(model);
    model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
    auto state {static_cast<CritterStateAnim>(401)};
    auto action {static_cast<CritterActionAnim>(509)};
    REQUIRE(model->HasAnimation(state, action));
    hstring root {client->Hashes.to_hashed_string("Limb")};
    auto pose {[&] {
        coarse_sleep(std::chrono::milliseconds {5});
        client->GameTime.FrameAdvance(false);
        model->PoseSpriteFrame(true);
    }};

    SECTION("ActionContextIsRetainedAndDeliveredToBothAnimationEvents")
    {
        auto context_proto {safe_alloc::make_refcounted<ProtoItem>(client->Hashes.to_hashed_string("AnimationContextItem"), client->GetPropertyRegistrar("Item").as_ptr())};
        int32_t initial_refs {context_proto->GetRefCount()};
        nptr<Entity> playback_context {};
        nptr<Entity> process_context {};

        Entity::EventCallbackData playback {};
        playback.Callback = [&](FuncCallData& call) {
            if (*cast_from_void<CritterAnimationContext*>(call.ArgsData[4]) == CritterAnimationContext::Action) {
                playback_context = *cast_from_void<Entity**>(call.ArgsData[3]);
                *cast_from_void<CritterAnimationFlags*>(call.ArgsData[6]) = CritterAnimationFlags::Freeze;
            }

            return Entity::EventResult::ContinueChain;
        };

        Entity::EventCallbackData processing {};
        processing.Callback = [&](FuncCallData& call) {
            nptr<Entity> context {*cast_from_void<Entity**>(call.ArgsData[3])};

            if (context) {
                process_context = context;
            }

            return Entity::EventResult::ContinueChain;
        };

        client->OnCritterAnimationPlaybackInit.Subscribe(std::move(playback));
        client->OnCritterAnimationProcess.Subscribe(std::move(processing));
        auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, client->GetProtoMap(client->Hashes.to_hashed_string("PolicyMap")), isize32 {320, 200})};
        auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
        map->EnableMapperMode();
        auto critter {map->AddMapperCritter(client->Hashes.to_hashed_string("UnitTestModelCritter"), mpos {6, 6}, mdir {0}, nullptr)};

        critter->AppendAnim(state, action, context_proto);
        CHECK(playback_context == context_proto.get());
        CHECK(process_context == context_proto.get());
        CHECK(context_proto->GetRefCount() == initial_refs + 1);

        critter->StopAnim();
        CHECK(context_proto->GetRefCount() == initial_refs);
    }

    SECTION("SamePairCanChangeItsFrozenTime")
    {
        (void)model->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::Freeze);
        pose();
        auto initial {model->GetBonePos(root)};
        REQUIRE(initial);
        (void)model->PlayAnim(state, action, nullptr, 0.5f, ModelAnimFlags::Freeze);
        pose();
        auto middle {model->GetBonePos(root)};
        REQUIRE(middle);
        CHECK(*initial != *middle);
        pose();
        CHECK(model->GetBonePos(root) == middle);
        CHECK(model->GetStateAnim() == state);
        CHECK(model->GetActionAnim() == action);
    }

    SECTION("FrozenMovementEndpointDoesNotWrap")
    {
        float32_t time {0.999f};
        model->SetAnimInitCallback([&](CritterAnimationContext, int32_t, CritterStateAnim& selected_state, CritterActionAnim& selected_action, CritterAnimationFlags& flags, float32_t& normalized_time, float32_t&) {
            selected_state = state;
            selected_action = action;
            flags = CritterAnimationFlags::Freeze;
            normalized_time = time;
        });

        (void)model->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::NoSmooth);
        pose();
        auto first {model->GetBonePos(root)};
        REQUIRE(first);
        model->SetMovementState(true, true, 137);
        pose();
        auto near_end {model->GetBonePos(root)};
        REQUIRE(near_end);
        CHECK(*near_end != *first);
        time = 1.0f;
        model->SetMovementState(true, true, 137);
        pose();
        CHECK(model->GetBonePos(root) == near_end);
        model->SetMoveDir(mdir {180}, false);
        pose();
        auto turned_end {model->GetBonePos(root)};
        time = 0.999f;
        model->SetMovementState(true, true, 137);
        pose();
        CHECK(model->GetBonePos(root) == turned_end);
    }

    SECTION("FrozenTurnCanBeReselected")
    {
        float32_t time {0.0f};
        model->SetAnimInitCallback([&](CritterAnimationContext, int32_t, CritterStateAnim& selected_state, CritterActionAnim& selected_action, CritterAnimationFlags& flags, float32_t& normalized_time, float32_t&) {
            selected_state = state;
            selected_action = action;
            flags = CritterAnimationFlags::Freeze;
            normalized_time = time;
        });

        (void)model->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::NoSmooth);
        model->SetDir(mdir {0}, false);
        model->SetMovementState(true, false, 0);
        pose();
        model->SetLookDir(mdir {120});
        nanotime turn_deadline {client->GameTime.GetFrameTime() + std::chrono::seconds {3}};

        // Bone positions include world facing; settle the turn before comparing frozen animation samples
        while (!is_float_equal(model->GetMoveDirAngle(), 60.0f) && client->GameTime.GetFrameTime() < turn_deadline) {
            pose();
        }

        REQUIRE(is_float_equal(model->GetMoveDirAngle(), 60.0f));
        pose();
        float32_t turn_angle {model->GetMoveDirAngle()};
        auto first {model->GetBonePos(root)};
        REQUIRE(first);
        REQUIRE(model->GetMovingContext() == CritterAnimationContext::TurnRight);
        time = 0.5f;
        model->SetMovementState(true, false, 0);
        pose();
        auto middle {model->GetBonePos(root)};
        REQUIRE(middle);
        CHECK(middle != first);
        pose();
        CHECK(model->GetMoveDirAngle() == turn_angle);
        CHECK(model->GetMovingContext() == CritterAnimationContext::TurnRight);
        CHECK(model->GetBonePos(root) == middle);
    }

    SECTION("SelectionSpeedMultipliesInstanceSpeedAndPreservesPhase")
    {
        auto reference {factory->GetModelMngr()->CreateModel(model_path)};
        REQUIRE(reference);
        reference->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
        (void)reference->PlayAnim(state, action, nullptr, 0.25f, ModelAnimFlags::NoSmooth);
        model->SetSpeed(2.0f);
        (void)model->PlayAnim(state, action, nullptr, 0.25f, ModelAnimFlags::NoSmooth, 0.5f);
        model->PoseSpriteFrame(true);
        reference->PoseSpriteFrame(true);
        coarse_sleep(std::chrono::milliseconds {60});
        client->GameTime.FrameAdvance(false);
        model->PoseSpriteFrame(true);
        reference->PoseSpriteFrame(true);
        CHECK(model->GetBonePos(root) == reference->GetBonePos(root));
        auto previous {model->GetBonePos(root)};
        REQUIRE(previous);
        (void)model->PlayAnim(state, action, nullptr, 0.25f, ModelAnimFlags::NoSmooth, 0.25f);
        model->PoseSpriteFrame(true);
        auto current {model->GetBonePos(root)};
        REQUIRE(current);
        CHECK(current == previous);
    }

    SECTION("OneShotLongFrameCallbacksAndFinalPoseHold")
    {
        float32_t start_time {0.0f};
        int32_t expected_middle_callbacks {1};

        SECTION("StartAtZeroFiresMiddleAndEnd")
        {
            start_time = 0.0f;
            expected_middle_callbacks = 1;
        }

        SECTION("StartPastMiddleSkipsEarlierPhase")
        {
            start_time = 0.75f;
            expected_middle_callbacks = 0;
        }

        // Keep the authored endpoint on a separate instance so the one-shot result cannot define its own expectation
        auto endpoint_model {factory->GetModelMngr()->CreateModel(model_path)};
        REQUIRE(endpoint_model);
        endpoint_model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
        (void)endpoint_model->PlayAnim(state, action, nullptr, 1.0f, ModelAnimFlags::Freeze);
        endpoint_model->PoseSpriteFrame(true);
        auto endpoint {endpoint_model->GetBonePos(root)};
        REQUIRE(endpoint);

        int32_t zero_callbacks {0};
        int32_t middle_callbacks {0};
        int32_t end_callbacks {0};

        model->AddAnimationCallback(ModelAnimationCallback {
            .StateAnim = state,
            .ActionAnim = action,
            .NormalizedTime = 0.0f,
            .Callback = [&zero_callbacks] { zero_callbacks++; },
        });
        model->AddAnimationCallback(ModelAnimationCallback {
            .StateAnim = state,
            .ActionAnim = action,
            .NormalizedTime = 0.5f,
            .Callback = [&middle_callbacks] { middle_callbacks++; },
        });
        model->AddAnimationCallback(ModelAnimationCallback {
            .StateAnim = state,
            .ActionAnim = action,
            .NormalizedTime = 1.0f,
            .Callback = [&end_callbacks] { end_callbacks++; },
        });

        model->SetSpeed(10.0f);
        (void)model->PlayAnim(state, action, nullptr, start_time, combine_enum(ModelAnimFlags::PlayOnce, ModelAnimFlags::NoSmooth));

        // This pose observes the initial position; only the next displayed frame may complete the clip
        model->PoseSpriteFrame(true);
        REQUIRE(model->IsAnimationPlaying());
        CHECK(zero_callbacks == 0);
        CHECK(middle_callbacks == 0);
        CHECK(end_callbacks == 0);

        coarse_sleep(std::chrono::milliseconds {200});
        client->GameTime.FrameAdvance(false);
        model->PoseSpriteFrame(true);
        auto finished {model->GetBonePos(root)};
        REQUIRE(finished);
        CHECK(*finished == *endpoint);
        CHECK_FALSE(model->IsAnimationPlaying());
        CHECK(zero_callbacks == 0);
        CHECK(middle_callbacks == expected_middle_callbacks);
        CHECK(end_callbacks == 1);

        pose();
        CHECK(model->GetBonePos(root) == finished);
        CHECK(zero_callbacks == 0);
        CHECK(middle_callbacks == expected_middle_callbacks);
        CHECK(end_callbacks == 1);

        model->ClearAnimationCallbacks();
    }

    SECTION("TurnSelectionExceptionRecoveryMatchesCleanModel")
    {
        auto reference {factory->GetModelMngr()->CreateModel(model_path)};
        REQUIRE(reference);
        REQUIRE(reference.as_ptr() != model.as_ptr());
        reference->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
        REQUIRE(reference->HasAnimation(state, action));
        REQUIRE(reference->FindBone(root));

        constexpr float32_t tiny_speed {1.0e-12f};
        constexpr float32_t normal_speed {2.0f};
        REQUIRE(std::isfinite(tiny_speed));
        REQUIRE(tiny_speed > std::numeric_limits<float32_t>::min());

        timespan duration {model->GetAnimDuration(state, action)};
        REQUIRE(duration == timespan {std::chrono::seconds {1}});
        REQUIRE(reference->GetAnimDuration(state, action) == duration);
        timespan remaining {duration.nanoseconds() / 2};
        float64_t completion_nanoseconds {numeric_cast<float64_t>(remaining.nanoseconds()) / numeric_cast<float64_t>(tiny_speed)};
        REQUIRE(std::isfinite(completion_nanoseconds));
        REQUIRE(completion_nanoseconds >= numeric_cast<float64_t>(std::numeric_limits<int64_t>::max()));
        REQUIRE_THROWS_AS(ScaleModelAnimationTime(remaining, 1.0 / numeric_cast<float64_t>(tiny_speed)), OverflowException);
        CHECK(ScaleModelAnimationTime(remaining, 1.0 / numeric_cast<float64_t>(normal_speed)) == timespan {std::chrono::milliseconds {250}});

        float32_t selected_speed {normal_speed};
        int32_t turn_callbacks {0};
        int32_t reference_turn_callbacks {0};
        model->SetAnimInitCallback([&](CritterAnimationContext context, int32_t, CritterStateAnim& selected_state, CritterActionAnim& selected_action, CritterAnimationFlags& flags, float32_t& normalized_time, float32_t& playback_speed) {
            selected_state = state;
            selected_action = action;
            flags = CritterAnimationFlags::None;
            normalized_time = context == CritterAnimationContext::TurnRight ? 0.5f : 0.0f;
            playback_speed = context == CritterAnimationContext::TurnRight ? selected_speed : 1.0f;

            if (context == CritterAnimationContext::TurnRight) {
                turn_callbacks++;
            }
        });
        reference->SetAnimInitCallback([&](CritterAnimationContext context, int32_t, CritterStateAnim& selected_state, CritterActionAnim& selected_action, CritterAnimationFlags& flags, float32_t& normalized_time, float32_t& playback_speed) {
            selected_state = state;
            selected_action = action;
            flags = CritterAnimationFlags::None;
            normalized_time = context == CritterAnimationContext::TurnRight ? 0.5f : 0.0f;
            playback_speed = context == CritterAnimationContext::TurnRight ? normal_speed : 1.0f;

            if (context == CritterAnimationContext::TurnRight) {
                reference_turn_callbacks++;
            }
        });

        client->GameTime.FrameAdvance(false);
        client->GameTime.Pause();
        auto resume_clock {scope_exit([&]() noexcept {
            if (client->GameTime.IsPaused()) {
                safe_call([&]() { client->GameTime.Resume(); });
            }
        })};

        (void)model->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::NoSmooth);
        (void)reference->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::NoSmooth);
        model->SetDir(mdir {0}, false);
        reference->SetDir(mdir {0}, false);
        model->SetMovementState(true, false, 0);
        reference->SetMovementState(true, false, 0);
        model->PoseSpriteFrame(true);
        reference->PoseSpriteFrame(true);

        REQUIRE(model->GetMovingContext() == CritterAnimationContext::Default);
        REQUIRE(reference->GetMovingContext() == CritterAnimationContext::Default);
        REQUIRE(turn_callbacks == 0);
        REQUIRE(reference_turn_callbacks == 0);
        auto initial {model->GetBonePos(root)};
        auto reference_initial {reference->GetBonePos(root)};
        REQUIRE(initial);
        REQUIRE(reference_initial);
        REQUIRE(initial == reference_initial);

        selected_speed = tiny_speed;
        REQUIRE_THROWS_MATCHES(model->SetLookDir(mdir {120}), OverflowException, Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("Floating point rounding received value out of integer range")));
        REQUIRE(turn_callbacks == 1);
        CHECK(model->GetMovingContext() == CritterAnimationContext::Default);
        REQUIRE_NOTHROW(reference->SetLookDir(mdir {120}));
        REQUIRE(reference_turn_callbacks == 1);

        // Evaluate the partial selection without advancing its clock, then retry through the ordinary caller
        REQUIRE_NOTHROW(model->PoseSpriteFrame(false));
        REQUIRE_NOTHROW(reference->PoseSpriteFrame(false));
        auto after_throw {model->GetBonePos(root)};
        REQUIRE(after_throw);

        selected_speed = normal_speed;
        REQUIRE_NOTHROW(model->SetMovementState(true, false, 0));
        REQUIRE_NOTHROW(reference->SetMovementState(true, false, 0));
        REQUIRE(turn_callbacks == 2);
        REQUIRE(reference_turn_callbacks == 2);
        model->PoseSpriteFrame(false);
        reference->PoseSpriteFrame(false);

        nanotime recovery_start {client->GameTime.GetFrameTime()};
        client->GameTime.Resume();
        coarse_sleep(std::chrono::milliseconds {20});
        client->GameTime.FrameAdvance(false);
        client->GameTime.Pause();
        timespan recovery_elapsed {client->GameTime.GetFrameTime() - recovery_start};
        INFO("RETIMING_RECOVERY_ELAPSED_NS=" << recovery_elapsed.nanoseconds());
        REQUIRE(recovery_elapsed > timespan {std::chrono::milliseconds {1}});
        REQUIRE(recovery_elapsed < timespan {std::chrono::milliseconds {100}});
        REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
        REQUIRE_NOTHROW(reference->PoseSpriteFrame(true));

        auto recovered {model->GetBonePos(root)};
        auto reference_recovered {reference->GetBonePos(root)};
        REQUIRE(recovered);
        REQUIRE(reference_recovered);
        CAPTURE(initial->x, initial->y, after_throw->x, after_throw->y, recovered->x, recovered->y, reference_recovered->x, reference_recovered->y);
        CHECK(model->GetStateAnim() == reference->GetStateAnim());
        CHECK(model->GetActionAnim() == reference->GetActionAnim());
        CHECK(model->GetMoveDirAngle() == reference->GetMoveDirAngle());
        CHECK(model->GetMovingContext() == CritterAnimationContext::TurnRight);
        CHECK(reference->GetMovingContext() == CritterAnimationContext::TurnRight);
        CHECK(recovered == reference_recovered);

        // A deliberate context reset distinguishes a failed retry from an unusable model
        model->SetMovementState(false, false, 0);
        reference->SetMovementState(false, false, 0);
        model->SetDir(mdir {0}, false);
        reference->SetDir(mdir {0}, false);
        model->SetMovementState(true, false, 0);
        reference->SetMovementState(true, false, 0);
        model->SetLookDir(mdir {120});
        reference->SetLookDir(mdir {120});

        nanotime reset_start {client->GameTime.GetFrameTime()};
        client->GameTime.Resume();
        coarse_sleep(std::chrono::milliseconds {20});
        client->GameTime.FrameAdvance(false);
        client->GameTime.Pause();
        timespan reset_elapsed {client->GameTime.GetFrameTime() - reset_start};
        INFO("RETIMING_RESET_ELAPSED_NS=" << reset_elapsed.nanoseconds());
        REQUIRE(reset_elapsed > timespan {std::chrono::milliseconds {1}});
        REQUIRE(reset_elapsed < timespan {std::chrono::milliseconds {100}});
        REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
        REQUIRE_NOTHROW(reference->PoseSpriteFrame(true));

        auto reset {model->GetBonePos(root)};
        auto reference_reset {reference->GetBonePos(root)};
        REQUIRE(reset);
        REQUIRE(reference_reset);
        CAPTURE(reset->x, reset->y, reference_reset->x, reference_reset->y);
        CHECK(model->GetMovingContext() == CritterAnimationContext::TurnRight);
        CHECK(reference->GetMovingContext() == CritterAnimationContext::TurnRight);
        CHECK(model->GetMoveDirAngle() == reference->GetMoveDirAngle());
        CHECK(reset == reference_reset);
    }

    SECTION("SameTurnRetimingFailurePreservesPhaseAndCompletionBeforeRetry")
    {
        auto reference {factory->GetModelMngr()->CreateModel(model_path)};
        REQUIRE(reference);
        REQUIRE(reference.as_ptr() != model.as_ptr());
        reference->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
        REQUIRE(reference->HasAnimation(state, action));
        REQUIRE(model->HasBodyRotation());
        REQUIRE(reference->HasBodyRotation());
        REQUIRE(reference->FindBone(root));

        constexpr float32_t normal_speed {0.125f};
        constexpr float32_t tiny_speed {1.0e-12f};
        REQUIRE(std::isfinite(tiny_speed));
        REQUIRE(tiny_speed > std::numeric_limits<float32_t>::min());
        REQUIRE(model->GetAnimDuration(state, action) == timespan {std::chrono::seconds {1}});
        REQUIRE(reference->GetAnimDuration(state, action) == timespan {std::chrono::seconds {1}});
        float32_t selected_speed {normal_speed};
        int32_t turn_callbacks {0};
        int32_t reference_turn_callbacks {0};
        model->SetAnimInitCallback([&](CritterAnimationContext context, int32_t, CritterStateAnim& selected_state, CritterActionAnim& selected_action, CritterAnimationFlags& flags, float32_t& normalized_time, float32_t& playback_speed) {
            selected_state = state;
            selected_action = action;
            flags = CritterAnimationFlags::None;
            normalized_time = context == CritterAnimationContext::TurnRight ? 0.5f : 0.0f;
            playback_speed = context == CritterAnimationContext::TurnRight ? selected_speed : 1.0f;

            if (context == CritterAnimationContext::TurnRight) {
                turn_callbacks++;
            }
        });
        reference->SetAnimInitCallback([&](CritterAnimationContext context, int32_t, CritterStateAnim& selected_state, CritterActionAnim& selected_action, CritterAnimationFlags& flags, float32_t& normalized_time, float32_t& playback_speed) {
            selected_state = state;
            selected_action = action;
            flags = CritterAnimationFlags::None;
            normalized_time = context == CritterAnimationContext::TurnRight ? 0.5f : 0.0f;
            playback_speed = context == CritterAnimationContext::TurnRight ? normal_speed : 1.0f;

            if (context == CritterAnimationContext::TurnRight) {
                reference_turn_callbacks++;
            }
        });

        client->GameTime.FrameAdvance(false);
        client->GameTime.Pause();
        auto resume_clock {scope_exit([&]() noexcept {
            if (client->GameTime.IsPaused()) {
                safe_call([&]() { client->GameTime.Resume(); });
            }
        })};
        auto advance_pair {[&](std::chrono::milliseconds wait) -> timespan {
            nanotime before {client->GameTime.GetFrameTime()};
            client->GameTime.Resume();
            coarse_sleep(wait);
            client->GameTime.FrameAdvance(true);
            client->GameTime.Pause();
            timespan elapsed {client->GameTime.GetFrameTime() - before};
            INFO("SAME_TURN_FRAME_NS=" << elapsed.nanoseconds());
            REQUIRE(elapsed > timespan {std::chrono::milliseconds {1}});
            REQUIRE(elapsed < timespan {std::chrono::milliseconds {700}});
            REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
            REQUIRE_NOTHROW(reference->PoseSpriteFrame(true));
            model->PoseSpriteFrame(false);
            reference->PoseSpriteFrame(false);
            return elapsed;
        }};

        (void)model->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::NoSmooth);
        (void)reference->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::NoSmooth);
        model->SetDir(mdir {0}, false);
        reference->SetDir(mdir {0}, false);
        model->SetMovementState(true, false, 0);
        reference->SetMovementState(true, false, 0);
        model->PoseSpriteFrame(true);
        reference->PoseSpriteFrame(true);
        model->SetLookDir(mdir {120});
        reference->SetLookDir(mdir {120});
        nanotime selected_at {client->GameTime.GetFrameTime()};
        REQUIRE(turn_callbacks == 1);
        REQUIRE(reference_turn_callbacks == 1);

        // Finish the speed ramp and settle facing before measuring translation on the offset limb point
        advance_pair(std::chrono::milliseconds {100});
        REQUIRE(is_float_equal(model->GetMoveDirAngle(), 60.0f));
        REQUIRE(model->GetMoveDirAngle() == reference->GetMoveDirAngle());
        vec3 local_offset {4.0f, 2.0f, 1.0f};
        auto settled {model->GetBonePos(root, local_offset)};
        auto reference_settled {reference->GetBonePos(root, local_offset)};
        auto zero_offset {model->GetBonePos(root)};
        REQUIRE(settled);
        REQUIRE(reference_settled);
        REQUIRE(zero_offset);
        REQUIRE(settled == reference_settled);
        REQUIRE(settled != zero_offset);

        advance_pair(std::chrono::milliseconds {500});
        advance_pair(std::chrono::milliseconds {500});
        auto before_failure {model->GetBonePos(root, local_offset)};
        auto reference_before_failure {reference->GetBonePos(root, local_offset)};
        REQUIRE(before_failure);
        REQUIRE(reference_before_failure);
        REQUIRE(before_failure == reference_before_failure);
        REQUIRE(reference_before_failure != reference_settled);
        REQUIRE(model->GetMoveDirAngle() == 60.0f);
        REQUIRE(model->GetMovingContext() == CritterAnimationContext::TurnRight);
        REQUIRE(reference->GetMovingContext() == CritterAnimationContext::TurnRight);

        selected_speed = tiny_speed;
        REQUIRE_THROWS_MATCHES(model->SetMovementState(true, false, 0), OverflowException, Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("Floating point rounding received value out of integer range")));
        REQUIRE(turn_callbacks == 2);
        REQUIRE(reference_turn_callbacks == 1);
        CHECK(model->GetMovingContext() == CritterAnimationContext::TurnRight);
        model->PoseSpriteFrame(false);
        reference->PoseSpriteFrame(false);
        CHECK(model->GetBonePos(root, local_offset) == reference->GetBonePos(root, local_offset));

        // Observe the prior speed and its original completion event before making any successful retry
        advance_pair(std::chrono::milliseconds {500});
        advance_pair(std::chrono::milliseconds {500});
        auto after_failure {model->GetBonePos(root, local_offset)};
        auto reference_after_failure {reference->GetBonePos(root, local_offset)};
        REQUIRE(after_failure);
        REQUIRE(reference_after_failure);
        REQUIRE(reference_after_failure != reference_before_failure);
        CAPTURE(before_failure->x, before_failure->y, after_failure->x, after_failure->y, reference_after_failure->x, reference_after_failure->y);
        CHECK(after_failure == reference_after_failure);
        CHECK(model->GetMoveDirAngle() == reference->GetMoveDirAngle());
        CHECK(model->GetStateAnim() == reference->GetStateAnim());
        CHECK(model->GetActionAnim() == reference->GetActionAnim());
        CHECK(model->GetMovingContext() == CritterAnimationContext::TurnRight);
        CHECK(reference->GetMovingContext() == CritterAnimationContext::TurnRight);
        CHECK(turn_callbacks == 2);
        CHECK(reference_turn_callbacks == 1);

        timespan completion {std::chrono::seconds {4}};
        int32_t completion_frames {0};

        while (client->GameTime.GetFrameTime() - selected_at < completion + std::chrono::milliseconds {200}) {
            REQUIRE(completion_frames < 8);
            advance_pair(std::chrono::milliseconds {500});
            completion_frames++;
        }

        CAPTURE(completion_frames, turn_callbacks, reference_turn_callbacks);
        CHECK(reference->GetMovingContext() == CritterAnimationContext::Default);
        CHECK(model->GetMovingContext() == CritterAnimationContext::Default);
        CHECK(model->GetMoveDirAngle() == reference->GetMoveDirAngle());
        auto completed {model->GetBonePos(root, local_offset)};
        auto reference_completed {reference->GetBonePos(root, local_offset)};
        REQUIRE(completed);
        REQUIRE(reference_completed);
        CHECK(completed == reference_completed);
        CHECK(turn_callbacks == 2);
        CHECK(reference_turn_callbacks == 1);

        selected_speed = normal_speed;
        REQUIRE_NOTHROW(model->SetMovementState(true, false, 0));
        REQUIRE_NOTHROW(reference->SetMovementState(true, false, 0));
        model->PoseSpriteFrame(false);
        reference->PoseSpriteFrame(false);
        CHECK(model->GetMovingContext() == reference->GetMovingContext());
        CHECK(model->GetBonePos(root, local_offset) == reference->GetBonePos(root, local_offset));
    }

    SECTION("NestedInitializerDirectionWinsOverTheOuterSelection")
    {
        float32_t turn_threshold {client->Settings->Render.CritterTurnAngle};
        CAPTURE(turn_threshold);
        REQUIRE(std::isfinite(turn_threshold));
        REQUIRE(turn_threshold >= 0.0f);
        REQUIRE(turn_threshold < 120.0f);

        auto reference {factory->GetModelMngr()->CreateModel(model_path)};
        REQUIRE(reference);
        REQUIRE(reference.as_ptr() != model.as_ptr());
        reference->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
        REQUIRE(reference->HasAnimation(state, action));
        REQUIRE(reference->FindBone(root));

        bool arm_reentry {false};
        bool reentry_requested {false};
        bool inside_reentry {false};
        int32_t outer_callbacks {0};
        int32_t nested_callbacks {0};
        int32_t reference_turn_callbacks {0};
        model->SetAnimInitCallback([&](CritterAnimationContext context, int32_t, CritterStateAnim& selected_state, CritterActionAnim& selected_action, CritterAnimationFlags& flags, float32_t& normalized_time, float32_t& playback_speed) {
            selected_state = state;
            selected_action = action;
            flags = CritterAnimationFlags::None;
            normalized_time = context == CritterAnimationContext::TurnRight ? 0.25f : 0.0f;
            playback_speed = context == CritterAnimationContext::TurnRight ? 0.125f : 1.0f;

            if (context == CritterAnimationContext::TurnRight && inside_reentry) {
                nested_callbacks++;
            }

            if (context == CritterAnimationContext::TurnRight && arm_reentry && !reentry_requested) {
                outer_callbacks++;
                reentry_requested = true;
                inside_reentry = true;
                model->SetLookDir(mdir {150});
                inside_reentry = false;

                // This valid stale result must not replace the nested target and its quarter-clip selection
                normalized_time = 0.75f;
            }
        });
        reference->SetAnimInitCallback([&](CritterAnimationContext context, int32_t, CritterStateAnim& selected_state, CritterActionAnim& selected_action, CritterAnimationFlags& flags, float32_t& normalized_time, float32_t& playback_speed) {
            selected_state = state;
            selected_action = action;
            flags = CritterAnimationFlags::None;
            normalized_time = context == CritterAnimationContext::TurnRight ? 0.25f : 0.0f;
            playback_speed = context == CritterAnimationContext::TurnRight ? 0.125f : 1.0f;

            if (context == CritterAnimationContext::TurnRight) {
                reference_turn_callbacks++;
            }
        });

        client->GameTime.FrameAdvance(false);
        client->GameTime.Pause();
        auto resume_clock {scope_exit([&]() noexcept {
            if (client->GameTime.IsPaused()) {
                safe_call([&]() { client->GameTime.Resume(); });
            }
        })};
        (void)model->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::NoSmooth);
        (void)reference->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::NoSmooth);
        model->SetDir(mdir {0}, false);
        reference->SetDir(mdir {0}, false);
        model->SetMovementState(true, false, 0);
        reference->SetMovementState(true, false, 0);
        model->PoseSpriteFrame(true);
        reference->PoseSpriteFrame(true);
        vec3 local_offset {4.0f, 2.0f, 1.0f};
        auto initial {model->GetBonePos(root, local_offset)};
        auto reference_initial {reference->GetBonePos(root, local_offset)};
        REQUIRE(initial);
        REQUIRE(reference_initial);
        REQUIRE(initial == reference_initial);

        arm_reentry = true;
        REQUIRE_NOTHROW(model->SetLookDir(mdir {120}));
        REQUIRE_NOTHROW(reference->SetLookDir(mdir {150}));
        REQUIRE(reentry_requested);
        REQUIRE_FALSE(inside_reentry);
        REQUIRE(outer_callbacks == 1);
        REQUIRE(nested_callbacks == 1);
        REQUIRE(reference_turn_callbacks == 1);
        CHECK(model->GetMovingContext() == CritterAnimationContext::TurnRight);
        CHECK(model->GetMovingContext() == reference->GetMovingContext());

        nanotime selected_at {client->GameTime.GetFrameTime()};
        client->GameTime.Resume();
        coarse_sleep(std::chrono::milliseconds {150});
        client->GameTime.FrameAdvance(true);
        client->GameTime.Pause();
        timespan elapsed {client->GameTime.GetFrameTime() - selected_at};
        INFO("INITIALIZER_REENTRY_FRAME_NS=" << elapsed.nanoseconds());
        REQUIRE(elapsed > timespan {std::chrono::milliseconds {100}});
        REQUIRE(elapsed < timespan {std::chrono::milliseconds {700}});
        REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
        REQUIRE_NOTHROW(reference->PoseSpriteFrame(true));
        model->PoseSpriteFrame(false);
        reference->PoseSpriteFrame(false);

        auto nested_pose {model->GetBonePos(root, local_offset)};
        auto reference_pose {reference->GetBonePos(root, local_offset)};
        auto zero_offset {model->GetBonePos(root)};
        REQUIRE(nested_pose);
        REQUIRE(reference_pose);
        REQUIRE(zero_offset);
        REQUIRE(nested_pose != zero_offset);
        REQUIRE(reference_pose != reference_initial);
        CAPTURE(nested_pose->x, nested_pose->y, reference_pose->x, reference_pose->y, outer_callbacks, nested_callbacks);
        CHECK(model->GetMoveDirAngle() == reference->GetMoveDirAngle());
        CHECK(is_float_equal(model->GetMoveDirAngle(), 30.0f));
        CHECK(model->GetMovingContext() == reference->GetMovingContext());
        CHECK(model->GetStateAnim() == reference->GetStateAnim());
        CHECK(model->GetActionAnim() == reference->GetActionAnim());
        CHECK(nested_pose == reference_pose);
        CHECK(outer_callbacks == 1);
        CHECK(nested_callbacks == 1);
        CHECK(reference_turn_callbacks == 1);
    }

    SECTION("TurnLifetimeUsesRemainingClipTimeAndCurrentSpeed")
    {
        float32_t start_time {0.8f};
        float32_t speed {1.0f};
        float32_t updated_speed {1.0f};
        bool expect_still_turning {false};
        SECTION("NormalizedStartUsesRemainingDuration")
        {
        }
        SECTION("FasterSelectionReschedulesCompletion")
        {
            start_time = 0.2f;
            updated_speed = 4.0f;
        }
        SECTION("SlowerSelectionReschedulesCompletion")
        {
            start_time = 0.5f;
            speed = 2.0f;
            updated_speed = 0.5f;
            expect_still_turning = true;
        }
        model->SetAnimInitCallback([&](CritterAnimationContext, int32_t, CritterStateAnim& selected_state, CritterActionAnim& selected_action, CritterAnimationFlags&, float32_t& normalized_time, float32_t& playback_speed) {
            selected_state = state;
            selected_action = action;
            normalized_time = start_time;
            playback_speed = speed;
        });

        (void)model->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::NoSmooth);
        client->GameTime.FrameAdvance(false);
        client->GameTime.Pause();
        auto resume_clock {scope_exit([&]() noexcept {
            if (client->GameTime.IsPaused()) {
                safe_call([&]() { client->GameTime.Resume(); });
            }
        })};

        model->PoseSpriteFrame(true);
        model->SetDir(mdir {0}, false);
        model->SetMovementState(true, false, 0);
        model->SetLookDir(mdir {120});
        nanotime selected_at {client->GameTime.GetFrameTime()};
        pose();
        INFO("Initial pose elapsed: " << (client->GameTime.GetFrameTime() - selected_at).milliseconds());
        INFO("Selected clip duration: " << model->GetAnimDuration(state, action).milliseconds());
        REQUIRE(model->GetMovingContext() == CritterAnimationContext::TurnRight);
        model->PoseSpriteFrame(false);
        auto before_speed_change {model->GetBonePos(root)};
        speed = updated_speed;
        model->SetMovementState(true, false, 0);
        model->PoseSpriteFrame(false);
        CHECK(model->GetBonePos(root) == before_speed_change);
        client->GameTime.Resume();

        nanotime retimed_at {client->GameTime.GetFrameTime()};

        // Cap logical frames so scheduler stalls cannot skip the intermediate turn state
        nanotime timed_frame_start {client->GameTime.GetFrameTime()};
        coarse_sleep(std::chrono::milliseconds {1100});
        client->GameTime.FrameAdvance(true);

        int64_t timed_delta_ms {(client->GameTime.GetFrameTime() - timed_frame_start).to_ms<int64_t>()};
        INFO("MODEL_TURN_LOGICAL_FRAME|elapsed_ms=" << timed_delta_ms);
        CHECK(timed_delta_ms == 650);

        model->PoseSpriteFrame(true);
        INFO("Time since retiming: " << (client->GameTime.GetFrameTime() - retimed_at).milliseconds());
        INFO("Clip duration: " << model->GetAnimDuration(state, action).milliseconds());
        CHECK(model->GetMovingContext() == (expect_still_turning ? CritterAnimationContext::TurnRight : CritterAnimationContext::Default));

        if (expect_still_turning) {
            coarse_sleep(std::chrono::milliseconds {650});
            client->GameTime.FrameAdvance(true);

            model->PoseSpriteFrame(true);
            CHECK(model->GetMovingContext() == CritterAnimationContext::Default);
        }
    }

    SECTION("FrozenBodyOwnsTheSkeletonWhileMoving")
    {
        (void)model->PlayAnim(state, action, nullptr, 0.5f, ModelAnimFlags::Freeze);
        pose();
        auto frozen {model->GetBonePos(root)};
        REQUIRE(frozen);
        model->SetAnimInitCallback([&](CritterAnimationContext, int32_t, CritterStateAnim& selected_state, CritterActionAnim& selected_action, CritterAnimationFlags& flags, float32_t& normalized_time, float32_t&) {
            selected_state = state;
            selected_action = action;
            flags = CritterAnimationFlags::Freeze;
            normalized_time = 0.0f;
        });

        model->SetMovementState(true, true, 137);
        pose();
        CHECK(model->GetBonePos(root) == frozen);
    }

    SECTION("MovingSelectionCanFreezeWithoutBeingReplacedByDefaultContext")
    {
        int32_t observed_speed {0};
        Entity::EventCallbackData selection {};
        selection.Callback = [&](FuncCallData& call) {
            if (*cast_from_void<CritterAnimationContext*>(call.ArgsData[4]) == CritterAnimationContext::Moving) {
                observed_speed = *cast_from_void<int32_t*>(call.ArgsData[5]);
                *cast_from_void<CritterStateAnim*>(call.ArgsData[1]) = state;
                *cast_from_void<CritterActionAnim*>(call.ArgsData[2]) = action;
                *cast_from_void<CritterAnimationFlags*>(call.ArgsData[6]) = CritterAnimationFlags::Freeze;
                *cast_from_void<float32_t*>(call.ArgsData[7]) = 0.5f;
            }

            return Entity::EventResult::ContinueChain;
        };

        client->OnCritterAnimationPlaybackInit.Subscribe(std::move(selection));
        auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, client->GetProtoMap(client->Hashes.to_hashed_string("PolicyMap")), isize32 {320, 200})};
        auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
        map->EnableMapperMode();
        auto critter {map->AddMapperCritter(client->Hashes.to_hashed_string("UnitTestModelCritter"), mpos {6, 6}, mdir {0}, nullptr)};
        critter->SetScaleFactor(2000);
        critter->SetMoving(safe_alloc::make_refcounted<MovingContext>(map->GetSize(), uint16_t {137}, vector<mdir> {mdir {0}}, vector<uint16_t> {1}, client->GameTime.GetFrameTime(), timespan {}, critter->GetHex(), ipos16 {}, ipos16 {}));
        critter->RefreshView();
        auto actor_model {critter->GetModel()};
        REQUIRE(actor_model);
        CHECK(actor_model->GetStateAnim() == state);
        CHECK(actor_model->GetActionAnim() == action);
        CHECK(observed_speed == 69);
    }

    SECTION("FrozenDefaultSelectionDisablesLocomotionDuringAdvancingAction")
    {
        Entity::EventCallbackData selection {};
        selection.Callback = [&](FuncCallData& call) {
            *cast_from_void<CritterStateAnim*>(call.ArgsData[1]) = state;
            *cast_from_void<CritterActionAnim*>(call.ArgsData[2]) = action;

            if (*cast_from_void<CritterAnimationContext*>(call.ArgsData[4]) != CritterAnimationContext::Action) {
                *cast_from_void<CritterAnimationFlags*>(call.ArgsData[6]) = CritterAnimationFlags::Freeze;
                *cast_from_void<float32_t*>(call.ArgsData[7]) = 1.0f;
            }

            return Entity::EventResult::ContinueChain;
        };
        client->OnCritterAnimationPlaybackInit.Subscribe(std::move(selection));

        auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, client->GetProtoMap(client->Hashes.to_hashed_string("PolicyMap")), isize32 {320, 200})};
        auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};

        map->EnableMapperMode();

        auto critter {map->AddMapperCritter(client->Hashes.to_hashed_string("UnitTestModelCritter"), mpos {6, 6}, mdir {0}, nullptr)};
        auto actor_model {critter->GetModel()};
        REQUIRE(actor_model);

        critter->AppendAnim(state, action);
        critter->SetMoving(safe_alloc::make_refcounted<MovingContext>(map->GetSize(), uint16_t {137}, vector<mdir> {mdir {0}}, vector<uint16_t> {1}, client->GameTime.GetFrameTime(), timespan {}, critter->GetHex(), ipos16 {}, ipos16 {}));
        critter->RefreshView();

        CHECK(actor_model->GetMovingContext() == CritterAnimationContext::Default);
        CHECK(critter->IsAnimPlaying());
        CHECK(actor_model->GetActionAnim() == action);
    }

    SECTION("QueuedFrozenActionOwnsMovingCritterLegs")
    {
        int32_t action_speed {-1};
        Entity::EventCallbackData selection {};
        selection.Callback = [&](FuncCallData& call) {
            *cast_from_void<CritterStateAnim*>(call.ArgsData[1]) = state;
            *cast_from_void<CritterActionAnim*>(call.ArgsData[2]) = action;

            if (*cast_from_void<CritterAnimationContext*>(call.ArgsData[4]) == CritterAnimationContext::Action) {
                action_speed = *cast_from_void<int32_t*>(call.ArgsData[5]);
                *cast_from_void<CritterAnimationFlags*>(call.ArgsData[6]) = CritterAnimationFlags::Freeze;
                *cast_from_void<float32_t*>(call.ArgsData[7]) = 0.5f;
            }

            return Entity::EventResult::ContinueChain;
        };
        client->OnCritterAnimationPlaybackInit.Subscribe(std::move(selection));
        auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, client->GetProtoMap(client->Hashes.to_hashed_string("PolicyMap")), isize32 {320, 200})};
        auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
        map->EnableMapperMode();
        auto critter {map->AddMapperCritter(client->Hashes.to_hashed_string("UnitTestModelCritter"), mpos {6, 6}, mdir {0}, nullptr)};
        auto actor_model {critter->GetModel()};
        REQUIRE(actor_model);
        critter->AppendAnim(state, action);
        CHECK(action_speed == 0);
        actor_model->PoseSpriteFrame(true);
        auto frozen {actor_model->GetBonePos(root)};
        REQUIRE(frozen);
        critter->SetMoving(safe_alloc::make_refcounted<MovingContext>(map->GetSize(), uint16_t {137}, vector<mdir> {mdir {0}}, vector<uint16_t> {1}, client->GameTime.GetFrameTime(), timespan {}, critter->GetHex(), ipos16 {}, ipos16 {}));
        critter->RefreshView();
        coarse_sleep(std::chrono::milliseconds {20});
        client->GameTime.FrameAdvance(false);
        actor_model->PoseSpriteFrame(true);
        CHECK(actor_model->GetBonePos(root) == frozen);
        CHECK(critter->IsAnimPlaying());
        critter->StopAnim();
        coarse_sleep(std::chrono::milliseconds {20});
        client->GameTime.FrameAdvance(false);
        actor_model->PoseSpriteFrame(true);
        CHECK_FALSE(critter->IsAnimPlaying());
        CHECK(actor_model->GetBonePos(root) != frozen);
        critter->SetScaleFactor(2000);
        critter->AppendAnim(state, action);
        CHECK(action_speed == 69);
    }

    SECTION("LocomotionCallbackReceivesDirectionAndSpeed")
    {
        vector<CritterAnimationContext> contexts {};
        int32_t effective_speed {0};
        model->SetAnimInitCallback([&](CritterAnimationContext context, int32_t moving_speed, CritterStateAnim& selected_state, CritterActionAnim& selected_action, CritterAnimationFlags&, float32_t&, float32_t& speed) {
            contexts.emplace_back(context);
            effective_speed = moving_speed;
            selected_state = state;
            selected_action = action;
            speed = 0.73f;
        });

        model->SetDir(mdir {0}, false);
        model->SetMovementState(true, true, 137);
        CHECK(effective_speed == 137);
        CHECK(model->GetMovingContext() == CritterAnimationContext::Moving);
        model->SetMoveDir(mdir {180}, true);
        CHECK(model->GetMovingContext() == CritterAnimationContext::MovingBack);
        model->SetMovementState(true, false, 0);
        model->SetLookDir(mdir {120});
        CHECK(std::ranges::find(contexts, CritterAnimationContext::TurnRight) != contexts.end());
        model->SetMoveDir(mdir {120}, false);
        model->SetLookDir(mdir {0});
        CHECK(std::ranges::find(contexts, CritterAnimationContext::TurnLeft) != contexts.end());
        coarse_sleep(std::chrono::milliseconds {1500});
        client->GameTime.FrameAdvance(false);
        model->PoseSpriteFrame(true);
        CHECK(model->GetMovingContext() == CritterAnimationContext::Default);
        (void)model->PlayAnim(state, action, nullptr, 0.0f, ModelAnimFlags::PlayOnce);
        pose();
        CHECK(model->GetActionAnim() == action);
    }
}

#endif

FO_END_NAMESPACE
