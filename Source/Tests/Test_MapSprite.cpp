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

#include "MapSprite.h"

FO_BEGIN_NAMESPACE

static auto AddTestSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, int8_t sub_layer) -> ptr<MapSprite>
{
    return list.AddSprite(draw_order, hex, ipos32 {}, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, sub_layer);
}

TEST_CASE("MapSpriteListDrawOrder")
{
    constexpr mpos WALL_CELL {100, 100};
    constexpr mpos NEARER_ROW {100, 101};
    constexpr int8_t WALL_SUB_LAYER = -1;
    constexpr int8_t SCENERY_SUB_LAYER = 0;

    SECTION("LowerSubLayerDrawsFirstOnOneHex")
    {
        // A merged wall run draws its sprite again on every mesh cell, and a vent hung on such a cell has to paint
        // over that copy whichever of the two sprites the view happened to create first
        for (bool wall_created_first : {false, true}) {
            MapSpriteList list;
            ptr<MapSprite> created_first = AddTestSprite(list, DrawOrderType::Item, WALL_CELL, wall_created_first ? WALL_SUB_LAYER : SCENERY_SUB_LAYER);
            ptr<MapSprite> created_second = AddTestSprite(list, DrawOrderType::Item, WALL_CELL, wall_created_first ? SCENERY_SUB_LAYER : WALL_SUB_LAYER);
            ptr<MapSprite> wall = wall_created_first ? created_first : created_second;
            ptr<MapSprite> vent = wall_created_first ? created_second : created_first;

            list.SortIfNeeded();
            const_span<unique_ptr<MapSprite>> sprites = list.GetActiveSprites();

            REQUIRE(sprites.size() == 2);

            ptr<const MapSprite> drawn_first = sprites[0].as_ptr();
            ptr<const MapSprite> drawn_second = sprites[1].as_ptr();

            CHECK(drawn_first == ptr<const MapSprite>(wall));
            CHECK(drawn_second == ptr<const MapSprite>(vent));
        }
    }

    SECTION("EqualSubLayerKeepsCreationOrder")
    {
        MapSpriteList list;
        ptr<MapSprite> first = AddTestSprite(list, DrawOrderType::Item, WALL_CELL, WALL_SUB_LAYER);
        ptr<MapSprite> second = AddTestSprite(list, DrawOrderType::Item, WALL_CELL, WALL_SUB_LAYER);

        list.SortIfNeeded();
        const_span<unique_ptr<MapSprite>> sprites = list.GetActiveSprites();

        REQUIRE(sprites.size() == 2);

        ptr<const MapSprite> drawn_first = sprites[0].as_ptr();
        ptr<const MapSprite> drawn_second = sprites[1].as_ptr();

        CHECK(drawn_first == ptr<const MapSprite>(first));
        CHECK(drawn_second == ptr<const MapSprite>(second));
    }

    SECTION("LowerSubLayerDrawsFirstInOneScreenRow")
    {
        // Hex 102:99 shares the screen row of 100:100 and stands further left: a wall slice there used to draw after
        // an item on 100:100 and cut off whatever of the item reached sideways over it
        constexpr mpos ROW_NEIGHBOUR {102, 99};
        uint64_t wall_on_neighbour = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, ROW_NEIGHBOUR, WALL_SUB_LAYER);
        uint64_t item_here = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, WALL_CELL, SCENERY_SUB_LAYER);
        uint64_t item_on_neighbour = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, ROW_NEIGHBOUR, SCENERY_SUB_LAYER);

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
            uint64_t critter = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, critter_hex, SCENERY_SUB_LAYER);

            for (mpos item_hex : {WALL_CELL, ROW_NEIGHBOUR}) {
                uint64_t lowest_item = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, item_hex, std::numeric_limits<int8_t>::min());
                uint64_t highest_item = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, item_hex, std::numeric_limits<int8_t>::max());
                uint64_t particles = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Particles, item_hex, SCENERY_SUB_LAYER);

                CHECK(lowest_item < critter);
                CHECK(highest_item < critter);
                CHECK(critter < particles);
            }
        }

        // Critters of one row keep their sideways order among themselves
        uint64_t critter_here = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, WALL_CELL, SCENERY_SUB_LAYER);
        uint64_t critter_on_neighbour = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, ROW_NEIGHBOUR, SCENERY_SUB_LAYER);

        CHECK(critter_here < critter_on_neighbour);
    }

    SECTION("SubLayerNeverOutranksTheRow")
    {
        // Anything on a nearer row still paints over the farther one, whatever sub-layers the two carry
        uint64_t highest_here = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, WALL_CELL, std::numeric_limits<int8_t>::max());
        uint64_t lowest_nearer = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, NEARER_ROW, std::numeric_limits<int8_t>::min());

        CHECK(highest_here < lowest_nearer);
    }

    SECTION("SubLayerAppliesToFlatLayersWithoutLeavingThem")
    {
        uint64_t flat_low = MapSpriteList::MakeDrawOrderPos(DrawOrderType::FlatItemAfterLight, WALL_CELL, std::numeric_limits<int8_t>::min());
        uint64_t flat_high = MapSpriteList::MakeDrawOrderPos(DrawOrderType::FlatItemAfterLight, WALL_CELL, std::numeric_limits<int8_t>::max());
        uint64_t standing = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, mpos {0, 0}, std::numeric_limits<int8_t>::min());

        CHECK(flat_low < flat_high);
        CHECK(flat_high < standing);
    }

    SECTION("OpenDoorStaysBehindItsSameRowFrame")
    {
        // The hamlet doorway has its flap at 192:157 and its right frame at 193:157. Both share screen row 253;
        // the flap must paint before the frame even though its hex X is lower and its sprite may be created later
        constexpr mpos DOOR_HEX {192, 157};
        constexpr mpos FRAME_HEX {193, 157};
        REQUIRE(GeometryHelper::GetHexScreenRow(DOOR_HEX) == GeometryHelper::GetHexScreenRow(FRAME_HEX));

        uint64_t flap = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, DOOR_HEX, -2);
        uint64_t frame = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, FRAME_HEX, WALL_SUB_LAYER);
        CHECK(flap < frame);
    }

    SECTION("DeadCritterStaysBelowStandingSpritesOnEveryRow")
    {
        uint64_t corpse = MapSpriteList::MakeDrawOrderPos(DrawOrderType::DeadCritter, WALL_CELL, std::numeric_limits<int8_t>::max());
        uint64_t far_item = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, mpos {100, 99}, std::numeric_limits<int8_t>::min());
        uint64_t far_critter = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, mpos {100, 99}, std::numeric_limits<int8_t>::min());
        uint64_t same_hex_item = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, WALL_CELL, std::numeric_limits<int8_t>::min());
        uint64_t same_hex_critter = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, WALL_CELL, std::numeric_limits<int8_t>::min());
        uint64_t near_item = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, NEARER_ROW, std::numeric_limits<int8_t>::min());

        CHECK(corpse < far_item);
        CHECK(corpse < far_critter);
        CHECK(corpse < same_hex_item);
        CHECK(corpse < same_hex_critter);
        CHECK(corpse < near_item);
    }
}

FO_END_NAMESPACE
