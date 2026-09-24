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
    FO_STACK_TRACE_ENTRY();

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
        uint64_t critter_here = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Critter, WALL_CELL, SCENERY_SUB_LAYER);
        uint64_t item_on_neighbour = MapSpriteList::MakeDrawOrderPos(DrawOrderType::Item, ROW_NEIGHBOUR, SCENERY_SUB_LAYER);

        CHECK(GeometryHelper::GetHexScreenRow(ROW_NEIGHBOUR) == GeometryHelper::GetHexScreenRow(WALL_CELL));
        CHECK(wall_on_neighbour < item_here);
        CHECK(item_here < critter_here);
        CHECK(critter_here < item_on_neighbour);
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
}

FO_END_NAMESPACE
