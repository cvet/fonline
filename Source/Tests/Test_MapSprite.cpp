// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "MapSprite.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

static auto AddTestSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, int8_t sub_layer) -> ptr<MapSprite>
{
    return list.AddSprite(draw_order, hex, ipos32 {}, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, sub_layer);
}

TEST_CASE("MapSpriteListClearDetachesRetainedHolders")
{
    MapSpriteList list {};
    bool valid {false};
    nptr<MapSprite> chain_root {};
    auto sprite {list.AddSprite(DrawOrderType::Item, mpos {100, 100}, ipos32 {}, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &valid, 0)};
    chain_root = sprite;
    sprite->CreateExtraChain(chain_root.get_pp());
    REQUIRE(valid);
    REQUIRE(chain_root);

    list.Clear();
    CHECK_FALSE(valid);
    CHECK_FALSE(chain_root);
    CHECK_FALSE(list.HasActiveSprites());
    CHECK(list.GetDrawOrderRange(DrawOrderType::Item, DrawOrderType::Item).first == 0);
    CHECK(list.GetDrawOrderRange(DrawOrderType::Item, DrawOrderType::Item).second == 0);

    list.Clear();
    auto replacement {AddTestSprite(list, DrawOrderType::Item, mpos {100, 100}, 0)};
    CHECK(replacement->IsValid());
    list.SortIfNeeded();
    CHECK(list.GetActiveSprites().size() == 1);
    list.Clear();
}

TEST_CASE("MapSpriteListDrawOrder")
{
    constexpr mpos WALL_CELL {100, 100};
    constexpr mpos NEARER_ROW {100, 101};
    constexpr int8_t WALL_SUB_LAYER {-1};
    constexpr int8_t SCENERY_SUB_LAYER {0};
    constexpr int8_t MARKER_SUB_LAYER {1};

    SECTION("LowerSubLayerDrawsFirstOnOneHex")
    {
        // A merged wall run draws its sprite again on every mesh cell, and a vent hung on such a cell has to paint
        // over that copy whichever of the two sprites the view happened to create first
        for (bool wall_created_first : {false, true}) {
            MapSpriteList list {};
            ptr<MapSprite> created_first {AddTestSprite(list, DrawOrderType::Item, WALL_CELL, wall_created_first ? WALL_SUB_LAYER : SCENERY_SUB_LAYER)};
            ptr<MapSprite> created_second {AddTestSprite(list, DrawOrderType::Item, WALL_CELL, wall_created_first ? SCENERY_SUB_LAYER : WALL_SUB_LAYER)};
            ptr<MapSprite> wall {wall_created_first ? created_first : created_second};
            ptr<MapSprite> vent {wall_created_first ? created_second : created_first};

            list.SortIfNeeded();
            const_span<unique_ptr<MapSprite>> sprites {list.GetActiveSprites()};

            REQUIRE(sprites.size() == 2);

            ptr<const MapSprite> drawn_first {sprites[0].as_ptr()};
            ptr<const MapSprite> drawn_second {sprites[1].as_ptr()};

            CHECK(drawn_first == ptr<const MapSprite>(wall));
            CHECK(drawn_second == ptr<const MapSprite>(vent));
        }
    }

    SECTION("EqualSubLayerKeepsCreationOrder")
    {
        MapSpriteList list {};
        ptr<MapSprite> first {AddTestSprite(list, DrawOrderType::Item, WALL_CELL, WALL_SUB_LAYER)};
        ptr<MapSprite> second {AddTestSprite(list, DrawOrderType::Item, WALL_CELL, WALL_SUB_LAYER)};

        list.SortIfNeeded();
        const_span<unique_ptr<MapSprite>> sprites {list.GetActiveSprites()};

        REQUIRE(sprites.size() == 2);

        ptr<const MapSprite> drawn_first {sprites[0].as_ptr()};
        ptr<const MapSprite> drawn_second {sprites[1].as_ptr()};

        CHECK(drawn_first == ptr<const MapSprite>(first));
        CHECK(drawn_second == ptr<const MapSprite>(second));
    }

    SECTION("LowerSubLayerDrawsFirstInOneScreenRow")
    {
        // Hex 102:99 shares the screen row of 100:100 and stands further left: a wall slice there used to draw after
        // an item on 100:100 and cut off whatever of the item reached sideways over it
        constexpr mpos ROW_NEIGHBOUR {102, 99};
        uint64_t wall_on_neighbour {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, ROW_NEIGHBOUR, WALL_SUB_LAYER)};
        uint64_t item_here {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, WALL_CELL, SCENERY_SUB_LAYER)};
        uint64_t item_on_neighbour {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, ROW_NEIGHBOUR, SCENERY_SUB_LAYER)};

        CHECK(GeometryHelper::GetHexScreenRow(ROW_NEIGHBOUR) == GeometryHelper::GetHexScreenRow(WALL_CELL));
        CHECK(wall_on_neighbour < item_here);
        CHECK(item_here < item_on_neighbour);
    }

    SECTION("CritterStandsInFrontOfEveryItemOfItsRow")
    {
        // A critter on an item's row stands at the item's front edge or beside it, so a wide item anchored on the larger X
        // (screen left) must not draw later and paint over the critter's body while its feet stand in front of the item
        constexpr mpos ROW_NEIGHBOUR {102, 99};

        CHECK(GeometryHelper::GetHexScreenRow(ROW_NEIGHBOUR) == GeometryHelper::GetHexScreenRow(WALL_CELL));

        for (mpos critter_hex : {WALL_CELL, ROW_NEIGHBOUR}) {
            uint64_t critter {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, critter_hex, SCENERY_SUB_LAYER)};

            for (mpos item_hex : {WALL_CELL, ROW_NEIGHBOUR}) {
                uint64_t lowest_item {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, item_hex, std::numeric_limits<int8_t>::min())};
                uint64_t highest_item {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, item_hex, std::numeric_limits<int8_t>::max())};
                uint64_t particles {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Particles, item_hex, SCENERY_SUB_LAYER)};

                CHECK(lowest_item < critter);
                CHECK(highest_item < critter);
                CHECK(critter < particles);
            }
        }

        // Critters of one row keep their sideways order among themselves
        uint64_t critter_here {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, WALL_CELL, SCENERY_SUB_LAYER)};
        uint64_t critter_on_neighbour {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, ROW_NEIGHBOUR, SCENERY_SUB_LAYER)};

        CHECK(critter_here < critter_on_neighbour);
    }

    SECTION("SubLayerNeverOutranksTheRow")
    {
        // Anything on a nearer row still paints over the farther one, whatever sub-layers the two carry
        uint64_t highest_here {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, WALL_CELL, std::numeric_limits<int8_t>::max())};
        uint64_t lowest_nearer {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, NEARER_ROW, std::numeric_limits<int8_t>::min())};

        CHECK(highest_here < lowest_nearer);
    }

    SECTION("FarthestCoordinatesKeepTheirFieldsApart")
    {
        // The widest X the key holds still orders below the sub-layer and the row, at either coordinate width
        mcoord_t far_x {sizeof(mcoord_t) == sizeof(int32_t) ? numeric_cast<mcoord_t>((1 << 22) - 1) : std::numeric_limits<mcoord_t>::max()};
        mcoord_t row_step {numeric_cast<mcoord_t>(GameSettings::HEXAGONAL_GEOMETRY ? 2 : 1)};
        mpos far_hex {far_x, 0};
        mpos same_row_hex {numeric_cast<mcoord_t>(far_x - row_step), 1};
        mpos nearer_row_hex {far_x, 1};
        REQUIRE(GeometryHelper::GetHexScreenRow(same_row_hex) == GeometryHelper::GetHexScreenRow(far_hex));
        REQUIRE(GeometryHelper::GetHexScreenRow(nearer_row_hex) == GeometryHelper::GetHexScreenRow(far_hex) + 1);

        uint64_t far_wall {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, far_hex, WALL_SUB_LAYER)};
        uint64_t same_row_item {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, same_row_hex, SCENERY_SUB_LAYER)};
        uint64_t far_item {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, far_hex, SCENERY_SUB_LAYER)};
        uint64_t far_critter {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, far_hex, std::numeric_limits<int8_t>::max())};
        uint64_t nearer_item {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, nearer_row_hex, std::numeric_limits<int8_t>::min())};

        CHECK(far_wall < same_row_item);
        CHECK(same_row_item < far_item);
        CHECK(far_critter < nearer_item);
    }

    SECTION("SubLayerAppliesToFlatLayersWithoutLeavingThem")
    {
        uint64_t flat_low {MapSpriteList::MakeDrawOrderPos(DrawOrderType::FlatItemAfterLight, WALL_CELL, std::numeric_limits<int8_t>::min())};
        uint64_t flat_high {MapSpriteList::MakeDrawOrderPos(DrawOrderType::FlatItemAfterLight, WALL_CELL, std::numeric_limits<int8_t>::max())};
        uint64_t standing {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, mpos {0, 0}, std::numeric_limits<int8_t>::min())};

        CHECK(flat_low < flat_high);
        CHECK(flat_high < standing);
    }

    SECTION("FlatSubLayerOutranksTheRow")
    {
        // A flat ground sprite has no depth: the paint layer decides, so a higher sub-layer covers a lower one on any row
        for (DrawOrderType layer : {DrawOrderType::FlatItemPreLight, DrawOrderType::FlatItemAfterLight}) {
            uint64_t decal_nearer {MapSpriteList::MakeDrawOrderPos(layer, NEARER_ROW, SCENERY_SUB_LAYER)};
            uint64_t marker_farther {MapSpriteList::MakeDrawOrderPos(layer, WALL_CELL, MARKER_SUB_LAYER)};
            uint64_t under_everything {MapSpriteList::MakeDrawOrderPos(layer, mpos {0, 1000}, std::numeric_limits<int8_t>::min())};
            uint64_t decal_farther {MapSpriteList::MakeDrawOrderPos(layer, WALL_CELL, SCENERY_SUB_LAYER)};

            CHECK(decal_nearer < marker_farther);
            CHECK(decal_farther < marker_farther);
            CHECK(under_everything < decal_farther);
            CHECK(decal_farther < decal_nearer);
        }
    }

    SECTION("FlatLayerKeepsRowThenXWithinOneSubLayer")
    {
        uint64_t same_row {MapSpriteList::MakeDrawOrderPos(DrawOrderType::FlatItemPreLight, WALL_CELL, SCENERY_SUB_LAYER)};
        uint64_t same_row_right {MapSpriteList::MakeDrawOrderPos(DrawOrderType::FlatItemPreLight, mpos {101, 100}, SCENERY_SUB_LAYER)};
        uint64_t nearer {MapSpriteList::MakeDrawOrderPos(DrawOrderType::FlatItemPreLight, NEARER_ROW, SCENERY_SUB_LAYER)};

        CHECK(same_row < same_row_right);
        CHECK(same_row_right < nearer);
    }

    SECTION("OpenDoorStaysBehindItsSameRowFrame")
    {
        // The hamlet doorway has its flap at 192:157 and its right frame at 193:157. Both share screen row 253;
        // the flap must paint before the frame even though its hex X is lower and its sprite may be created later
        constexpr mpos DOOR_HEX {192, 157};
        constexpr mpos FRAME_HEX {193, 157};
        REQUIRE(GeometryHelper::GetHexScreenRow(DOOR_HEX) == GeometryHelper::GetHexScreenRow(FRAME_HEX));

        uint64_t flap {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, DOOR_HEX, -2)};
        uint64_t frame {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, FRAME_HEX, WALL_SUB_LAYER)};
        CHECK(flap < frame);
    }

    SECTION("DeadCritterStaysBelowStandingSpritesOnEveryRow")
    {
        uint64_t corpse {MapSpriteList::MakeDrawOrderPos(DrawOrderType::FlatCritter, WALL_CELL, std::numeric_limits<int8_t>::max())};
        uint64_t far_item {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, mpos {100, 99}, std::numeric_limits<int8_t>::min())};
        uint64_t far_critter {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, mpos {100, 99}, std::numeric_limits<int8_t>::min())};
        uint64_t same_hex_item {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, WALL_CELL, std::numeric_limits<int8_t>::min())};
        uint64_t same_hex_critter {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, WALL_CELL, std::numeric_limits<int8_t>::min())};
        uint64_t near_item {MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, NEARER_ROW, std::numeric_limits<int8_t>::min())};

        CHECK(corpse < far_item);
        CHECK(corpse < far_critter);
        CHECK(corpse < same_hex_item);
        CHECK(corpse < same_hex_critter);
        CHECK(corpse < near_item);
    }
}

TEST_CASE("MapSpriteDepthOffsetKeepsTheScreenAnchor")
{
    MapSpriteList list {};
    ipos32 field_offset {1000, 500};
    ipos32 sprite_offset {5, -8};
    ipos32 root_offset {2, 3};
    auto sprite {list.AddSprite(DrawOrderType::Particles, mpos {100, 100}, ipos32 {12, -17}, &field_offset, nullptr, nullptr, &sprite_offset, &root_offset, nullptr, nullptr, nullptr, 0)};
    sprite->SetElevation(18);

    CHECK(sprite->GetDepthOffset() == 0.0f);
    ipos32 draw_root {sprite->GetDrawRootPos()};
    vec3 anchor_world {GeometryHelper::GetHexWorldPos(ipos32 {100, 100}, ipos32 {15, -28}, 18.0f)};
    vec3 anchor_projection {GeometryHelper::ProjectWorldToMap(anchor_world)};
    float32_t angle {GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT};

    // Moving along the camera ray changes physical depth while leaving the screen point fixed.
    // This world-space reference does not use the sprite's projection or depth-offset implementation
    for (float32_t delta : {12.5f, -9.25f, 0.0f}) {
        vec3 world_point {anchor_world + vec3 {0.0f, std::sin(angle) * delta, std::cos(angle) * delta}};
        vec3 expected {GeometryHelper::ProjectWorldToMap(world_point)};
        sprite->SetDepthOffset(delta);
        vec3 actual {sprite->GetMapRootProjection()};

        CHECK(is_float_equal(actual.x, expected.x));
        CHECK(is_float_equal(actual.y, expected.y));
        CHECK(is_float_equal(actual.z, expected.z));
        CHECK(is_float_equal(actual.x, anchor_projection.x));
        CHECK(is_float_equal(actual.y, anchor_projection.y));
        CHECK(sprite->GetDrawRootPos() == draw_root);
    }
}

TEST_CASE("MapSpriteDepthOffsetResetsWhenThePoolReusesASprite")
{
    MapSpriteList list {};
    auto sprite {AddTestSprite(list, DrawOrderType::Particles, mpos {100, 100}, 0)};
    sprite->SetDepthOffset(73.5f);
    sprite->Invalidate();
    auto replacement {AddTestSprite(list, DrawOrderType::Particles, mpos {100, 100}, 0)};

    // Invalidate pushes this object onto the same LIFO pool AddSprite reads
    REQUIRE(replacement == sprite);
    CHECK(replacement->GetDepthOffset() == 0.0f);

    MapSpriteHolder holder {};
    CHECK(holder.DepthOffset == 0.0f);
}

FO_END_NAMESPACE
