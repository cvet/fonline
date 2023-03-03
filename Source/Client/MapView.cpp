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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "MapView.h"
#include "Client.h"
#include "GenericUtils.h"
#include "LineTracer.h"
#include "Log.h"
#include "StringUtils.h"

static constexpr auto MAX_LIGHT_VALUE = 10000;
static constexpr auto MAX_LIGHT_HEX = 200;
static constexpr auto MAX_LIGHT_ALPHA = 255;

static auto EvaluateItemDrawOrder(const ItemHexView* item) -> DrawOrderType
{
    return item->GetIsFlat() ? (!item->IsAnyScenery() ? DrawOrderType::FlatItem : DrawOrderType::FlatScenery) : (!item->IsAnyScenery() ? DrawOrderType::Item : DrawOrderType::Scenery);
}

static auto EvaluateCritterDrawOrder(const CritterHexView* cr) -> DrawOrderType
{
    return cr->IsDead() && !cr->GetIsNoFlatten() ? DrawOrderType::DeadCritter : DrawOrderType::Critter;
}

static auto EvaluateTileDrawOrder(const Field::Tile& tile) -> DrawOrderType
{
    return static_cast<DrawOrderType>(static_cast<int>(DrawOrderType::Tile) + static_cast<int>(tile.Layer));
}

Field::~Field()
{
    delete Critters;
    delete Tiles[0];
    delete Tiles[1];
    delete Items;
    delete BlockLinesItems;
}

void Field::AddItem(ItemHexView* item, ItemHexView* block_lines_item)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item || block_lines_item);

    if (item != nullptr) {
        if (Items == nullptr) {
            Items = new vector<ItemHexView*>();
        }
        Items->push_back(item);
    }

    if (block_lines_item != nullptr) {
        if (BlockLinesItems == nullptr) {
            BlockLinesItems = new vector<ItemHexView*>();
        }
        BlockLinesItems->push_back(block_lines_item);
    }

    ProcessCache();
}

void Field::EraseItem(ItemHexView* item, ItemHexView* block_lines_item)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item || block_lines_item);

    if (item != nullptr) {
        RUNTIME_ASSERT(Items);

        const auto it = std::find(Items->begin(), Items->end(), item);
        RUNTIME_ASSERT(it != Items->end());

        Items->erase(it);

        if (Items->empty()) {
            delete Items;
            Items = nullptr;
        }
    }

    if (block_lines_item != nullptr) {
        RUNTIME_ASSERT(BlockLinesItems);

        const auto it = std::find(BlockLinesItems->begin(), BlockLinesItems->end(), block_lines_item);
        RUNTIME_ASSERT(it != BlockLinesItems->end());

        BlockLinesItems->erase(it);

        if (BlockLinesItems->empty()) {
            delete BlockLinesItems;
            BlockLinesItems = nullptr;
        }
    }

    ProcessCache();
}

auto Field::AddTile(AnyFrames* anim, int16 ox, int16 oy, uint8 layer, bool is_roof) -> Tile&
{
    STACK_TRACE_ENTRY();

    Tile* tile;
    auto*& tiles_vec = Tiles[is_roof ? 1 : 0];
    auto*& stile = SimpleTile[is_roof ? 1 : 0];

    if (tiles_vec == nullptr && stile == nullptr && ox == 0 && oy == 0 && layer == 0u) {
        stile = anim;
        tile = &SimpleTileStub;
    }
    else {
        if (tiles_vec == nullptr) {
            tiles_vec = new vector<Tile>();
        }

        tiles_vec->push_back(Tile());
        tile = &tiles_vec->back();
    }

    tile->Anim = anim;
    tile->OffsX = ox;
    tile->OffsY = oy;
    tile->Layer = layer;
    return *tile;
}

void Field::EraseTile(uint index, bool is_roof)
{
    STACK_TRACE_ENTRY();

    auto*& tiles_vec = Tiles[is_roof ? 1 : 0];
    auto*& stile = SimpleTile[is_roof ? 1 : 0];

    if (index == 0 && (stile != nullptr)) {
        stile = nullptr;
        if ((tiles_vec != nullptr) && (tiles_vec->front().OffsX == 0) && (tiles_vec->front().OffsY == 0) && (tiles_vec->front().Layer == 0u)) {
            stile = tiles_vec->front().Anim;
            tiles_vec->erase(tiles_vec->begin());
        }
    }
    else {
        tiles_vec->erase(tiles_vec->begin() + index - (stile != nullptr ? 1 : 0));
    }

    if (tiles_vec != nullptr && tiles_vec->empty()) {
        delete tiles_vec;
        tiles_vec = nullptr;
    }
}

auto Field::GetTilesCount(bool is_roof) -> uint
{
    STACK_TRACE_ENTRY();

    auto*& tiles_vec = Tiles[is_roof ? 1 : 0];
    auto*& stile = SimpleTile[is_roof ? 1 : 0];

    return (stile != nullptr ? 1 : 0) + (tiles_vec != nullptr ? static_cast<uint>(tiles_vec->size()) : 0);
}

auto Field::GetTile(uint index, bool is_roof) -> Field::Tile&
{
    STACK_TRACE_ENTRY();

    auto*& tiles_vec = Tiles[is_roof ? 1 : 0];
    auto*& stile = SimpleTile[is_roof ? 1 : 0];

    if (index == 0 && (stile != nullptr)) {
        SimpleTileStub.Anim = stile;
        return SimpleTileStub;
    }
    return tiles_vec->at(index - (stile != nullptr ? 1 : 0));
}

auto Field::GetActiveCritter() -> CritterHexView*
{
    STACK_TRACE_ENTRY();

    if (Critters != nullptr) {
        for (auto* cr : *Critters) {
            if (!cr->IsDead()) {
                return cr;
            }
        }
    }

    return nullptr;
}

void Field::AddCritter(CritterHexView* cr)
{
    STACK_TRACE_ENTRY();

    if (Critters == nullptr) {
        Critters = new vector<CritterHexView*>();
    }

    RUNTIME_ASSERT(std::find(Critters->begin(), Critters->end(), cr) == Critters->end());
    Critters->push_back(cr);
}

void Field::EraseCritter(CritterHexView* cr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(Critters != nullptr);

    const auto it = std::find(Critters->begin(), Critters->end(), cr);
    RUNTIME_ASSERT(it != Critters->end());

    Critters->erase(it);

    if (Critters->empty()) {
        delete Critters;
        Critters = nullptr;
    }
}

void Field::ProcessCache()
{
    STACK_TRACE_ENTRY();

    Flags.IsWall = false;
    Flags.IsWallTransp = false;
    Flags.IsScen = false;
    Flags.IsMoveBlocked = Flags.IsMultihex;
    Flags.IsShootBlocked = false;
    Flags.IsLightBlocked = false;
    Flags.ScrollBlock = false;
    Corner = CornerType::NorthSouth;

    if (Critters != nullptr) {
        for (const auto* cr : *Critters) {
            if (!Flags.IsMoveBlocked && !cr->IsDead()) {
                Flags.IsMoveBlocked = true;
            }
        }
    }

    if (Items != nullptr) {
        for (const auto* item : *Items) {
            if (!Flags.IsWall && item->IsWall()) {
                Flags.IsWall = true;
                Flags.IsWallTransp = item->GetIsLightThru();

                Corner = item->GetCorner();
            }
            else if (!Flags.IsScen && item->IsScenery()) {
                Flags.IsScen = true;

                if (!Flags.IsWall) {
                    Corner = item->GetCorner();
                }
            }

            if (!Flags.IsMoveBlocked && !item->GetIsNoBlock()) {
                Flags.IsMoveBlocked = true;
            }
            if (!Flags.IsShootBlocked && !item->GetIsShootThru()) {
                Flags.IsShootBlocked = true;
            }
            if (!Flags.ScrollBlock && item->GetIsScrollBlock()) {
                Flags.ScrollBlock = true;
            }
            if (!Flags.IsLightBlocked && !item->GetIsLightThru()) {
                Flags.IsLightBlocked = true;
            }
        }
    }

    if (BlockLinesItems != nullptr) {
        for (const auto* item : *BlockLinesItems) {
            Flags.IsMoveBlocked = true;

            if (!Flags.IsShootBlocked && !item->GetIsShootThru()) {
                Flags.IsShootBlocked = true;
            }
            if (!Flags.IsLightBlocked && !item->GetIsLightThru()) {
                Flags.IsLightBlocked = true;
            }
        }
    }
}

void Field::AddSpriteToChain(Sprite* spr)
{
    STACK_TRACE_ENTRY();

    if (SpriteChain == nullptr) {
        SpriteChain = spr;
        spr->ExtraChainRoot = &SpriteChain;
    }
    else {
        auto* last_spr = SpriteChain;
        while (last_spr->ExtraChainChild != nullptr) {
            last_spr = last_spr->ExtraChainChild;
        }
        last_spr->ExtraChainChild = spr;
        spr->ExtraChainParent = last_spr;
    }
}

void Field::UnvalidateSpriteChain() const
{
    STACK_TRACE_ENTRY();

    if (SpriteChain != nullptr) {
        while (SpriteChain != nullptr) {
            SpriteChain->Unvalidate();
        }
    }
}

MapView::MapView(FOClient* engine, ident_t id, const ProtoMap* proto) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME)), //
    EntityWithProto(this, proto),
    MapProperties(GetInitRef()),
    _mainTree(engine->SprMngr, _spritesPool),
    _tilesTree(engine->SprMngr, _spritesPool),
    _roofTree(engine->SprMngr, _spritesPool)
{
    STACK_TRACE_ENTRY();

    _name = _str("{}_{}", proto->GetName(), id);

    SetSpritesZoom(1.0f);

    _rtScreenOx = iround(std::ceil(static_cast<float>(_engine->Settings.MapHexWidth) / MIN_ZOOM));
    _rtScreenOy = iround(std::ceil(static_cast<float>(_engine->Settings.MapHexLineHeight * 2) / MIN_ZOOM));

#if !FO_DIRECT_MAP_DRAW
    _rtMap = _engine->SprMngr.CreateRenderTarget(false, RenderTarget::SizeType::Map, 0, 0, false);
    _rtMap->CustomDrawEffect = _engine->EffectMngr.Effects.FlushMap;
#endif

    _rtLight = _engine->SprMngr.CreateRenderTarget(false, RenderTarget::SizeType::Map, _rtScreenOx * 2, _rtScreenOy * 2, false);
    _rtLight->CustomDrawEffect = _engine->EffectMngr.Effects.FlushLight;

    _rtFog = _engine->SprMngr.CreateRenderTarget(false, RenderTarget::SizeType::Map, _rtScreenOx * 2, _rtScreenOy * 2, false);
    _rtFog->CustomDrawEffect = _engine->EffectMngr.Effects.FlushFog;

    _engine->SprMngr.PushAtlasType(AtlasType::Dynamic);
    _picHex[0] = _engine->SprMngr.LoadAnimation(_engine->Settings.MapDataPrefix + "hex1.png", true);
    _picHex[1] = _engine->SprMngr.LoadAnimation(_engine->Settings.MapDataPrefix + "hex2.png", true);
    _picHex[2] = _engine->SprMngr.LoadAnimation(_engine->Settings.MapDataPrefix + "hex3.png", true);
    _cursorPrePic = _engine->SprMngr.LoadAnimation(_engine->Settings.MapDataPrefix + "move_pre.png", false);
    _cursorPostPic = _engine->SprMngr.LoadAnimation(_engine->Settings.MapDataPrefix + "move_post.png", false);
    _cursorXPic = _engine->SprMngr.LoadAnimation(_engine->Settings.MapDataPrefix + "move_x.png", false);
    _picTrack1 = _engine->SprMngr.LoadAnimation(_engine->Settings.MapDataPrefix + "track1.png", true);
    _picTrack2 = _engine->SprMngr.LoadAnimation(_engine->Settings.MapDataPrefix + "track2.png", true);
    _picHexMask = _engine->SprMngr.LoadAnimation(_engine->Settings.MapDataPrefix + "hex_mask.png", false);
    _engine->SprMngr.PopAtlasType();

    if (_picHexMask != nullptr) {
        const auto* si = _engine->SprMngr.GetSpriteInfo(_picHexMask->Ind[0]);
        const auto mask_x = iround(static_cast<float>(si->Atlas->MainTex->Width) * si->SprRect.Left);
        const auto mask_y = iround(static_cast<float>(si->Atlas->MainTex->Height) * si->SprRect.Top);
        _picHexMaskData = si->Atlas->MainTex->GetTextureRegion(mask_x, mask_y, si->Width, si->Height);
    }

    _maxHexX = GetWidth();
    _maxHexY = GetHeight();

    _findPathGrid.resize((MAX_FIND_PATH * 2 + 2) * (MAX_FIND_PATH * 2 + 2));
    _hexField.resize(_maxHexX * _maxHexY);
    _hexLight.resize(_maxHexX * _maxHexY * 3);

    _eventUnsubscriber += _engine->SprMngr.GetWindow()->OnScreenSizeChanged += [this] { OnScreenSizeChanged(); };
}

MapView::~MapView()
{
    STACK_TRACE_ENTRY();

    if (!_critters.empty() || !_crittersMap.empty() || !_allItems.empty() || !_staticItems.empty() || !_dynamicItems.empty() || !_itemsMap.empty()) {
        BreakIntoDebugger();
    }

    if (_rtMap != nullptr) {
        _engine->SprMngr.DeleteRenderTarget(_rtMap);
    }
    _engine->SprMngr.DeleteRenderTarget(_rtLight);
    _engine->SprMngr.DeleteRenderTarget(_rtFog);

    _mainTree.Clear();
    _roofTree.Clear();
    _tilesTree.Clear();

    for (const auto* spr : _spritesPool) {
        delete spr;
    }
}

void MapView::MarkAsDestroyed()
{
    STACK_TRACE_ENTRY();

    for (auto* cr : copy(_critters)) {
        DestroyCritter(cr);
    }
    RUNTIME_ASSERT(_critters.empty());
    RUNTIME_ASSERT(_crittersMap.empty());

    for (auto* item : copy(_allItems)) {
        DestroyItem(item);
    }
    RUNTIME_ASSERT(_allItems.empty());
    RUNTIME_ASSERT(_staticItems.empty());
    RUNTIME_ASSERT(_dynamicItems.empty());
    RUNTIME_ASSERT(_itemsMap.empty());

    Entity::MarkAsDestroying();
    Entity::MarkAsDestroyed();
}

void MapView::EnableMapperMode()
{
    STACK_TRACE_ENTRY();

    _mapperMode = true;
    _isShowTrack = true;

    _tilesField.resize(_maxHexX * _maxHexY);
    _roofsField.resize(_maxHexX * _maxHexY);
    _hexTrack.resize(_maxHexX * _maxHexY);
}

void MapView::LoadStaticData()
{
    STACK_TRACE_ENTRY();

    const auto file = _engine->Resources.ReadFile(_str("{}.fomapb2", GetProtoId()));
    if (!file) {
        throw MapViewLoadException("Map file not found", GetProtoId());
    }

    auto reader = DataReader({file.GetBuf(), file.GetSize()});

    // Read static items
    {
        vector<uint8> props_data;

        const auto scen_count = reader.Read<uint>();
        for (uint i = 0; i < scen_count; i++) {
            const auto static_id = ident_t {reader.Read<uint>()};
            const auto item_pid_hash = reader.Read<hstring::hash_t>();
            const auto item_pid = _engine->ResolveHash(item_pid_hash);
            const auto* item_proto = _engine->ProtoMngr.GetProtoItem(item_pid);

            auto item_props = Properties(item_proto->GetProperties().GetRegistrator());
            const auto props_data_size = reader.Read<uint>();
            props_data.resize(props_data_size);
            reader.ReadPtr<uint8>(props_data.data(), props_data_size);
            item_props.RestoreAllData(props_data);

            auto* static_item = new ItemHexView(this, static_id, item_proto, item_props);

            AddItemInternal(static_item);
        }
    }

    // Read tiles
    {
        const auto tile_hashes_count = reader.Read<uint>();
        string str;
        for (uint i = 0; i < tile_hashes_count; i++) {
            const auto str_len = reader.Read<uint>();
            str.resize(str_len);
            reader.ReadPtr(str.data(), str.length());
            const auto hstr = _engine->ToHashedString(str);
            UNUSED_VARIABLE(hstr);
        }

        const auto tiles_count = reader.Read<uint>();
        for (uint i = 0; i < tiles_count; i++) {
            MapTile tile;
            reader.ReadPtr(&tile, sizeof(tile));

            RUNTIME_ASSERT(tile.HexX < _maxHexX && tile.HexY < _maxHexY);
            auto& field = GetField(tile.HexX, tile.HexY);
            const auto anim_name = _engine->ResolveHash(tile.NameHash);
            auto* anim = _engine->ResMngr.GetItemAnim(anim_name);
            if (anim != nullptr) {
                auto& ftile = field.AddTile(anim, tile.OffsX, tile.OffsY, tile.Layer, tile.IsRoof);
                MeasureMapBorders(ftile, tile.IsRoof);
            }
            else {
                BreakIntoDebugger();
            }
        }
    }

    reader.VerifyEnd();

    // Index roof
    auto roof_num = 1;
    for (const auto hx : xrange(_maxHexX)) {
        for (const auto hy : xrange(_maxHexY)) {
            if (GetField(hx, hy).GetTilesCount(true) != 0u) {
                MarkRoofNum(hx, hy, static_cast<int16>(roof_num));
                roof_num++;
            }
        }
    }

    // Scroll blocks borders
    for (const auto hx : xrange(_maxHexX)) {
        for (const auto hy : xrange(_maxHexY)) {
            auto& field = GetField(hx, hy);
            if (field.Flags.ScrollBlock) {
                for (const auto dir : xrange(GameSettings::MAP_DIR_COUNT)) {
                    auto hx_ = hx;
                    auto hy_ = hy;
                    _engine->Geometry.MoveHexByDir(hx_, hy_, static_cast<uint8>(dir), _maxHexX, _maxHexY);
                    GetField(hx_, hy_).Flags.IsMoveBlocked = true;
                }
            }
        }
    }

    ResizeView();

    CollectLightSources();
    _lightPoints.clear();
    _lightPointsCount = 0;
    RealRebuildLight();
    _requestRebuildLight = false;
    _requestRenderLight = true;

    AutoScroll.Active = false;
}

void MapView::Process()
{
    STACK_TRACE_ENTRY();

    // Map time and color
    {
        const auto map_day_time = GetMapDayTime();
        const auto global_day_time = GetGlobalDayTime();

        if (map_day_time != _prevMapDayTime || global_day_time != _prevGlobalDayTime) {
            _prevMapDayTime = map_day_time;
            _prevGlobalDayTime = global_day_time;

            auto&& day_time = GetDayTime();
            auto&& day_color = GetDayColor();

            _mapDayColor = GenericUtils::GetColorDay(day_time, day_color, map_day_time, &_mapDayLightCapacity);
            _globalDayColor = GenericUtils::GetColorDay(day_time, day_color, global_day_time, &_globalDayLightCapacity);

            if (_mapDayColor != _prevMapDayColor) {
                _prevMapDayColor = _mapDayColor;

                _requestRebuildLight = true;
            }

            if (_globalDayColor != _prevGlobalDayColor) {
                _prevGlobalDayColor = _globalDayColor;

                if (_hasGlobalLights) {
                    _requestRebuildLight = true;
                }
            }
        }
    }

    // Critters
    {
        vector<CritterHexView*> critter_to_delete;

        for (auto* cr : _critters) {
            cr->Process();

            if (cr->IsNeedReset()) {
                RemoveCritterFromField(cr);
                AddCritterToField(cr);
                cr->ResetOk();
            }

            if (cr->IsFinishing() && cr->IsFinished()) {
                critter_to_delete.emplace_back(cr);
            }
        }

        for (auto* cr : critter_to_delete) {
            DestroyCritter(cr);
        }
    }

    // Items
    {
        vector<ItemHexView*> item_to_delete;

        for (auto* item : _allItems) {
            if (item->IsNeedProcess()) {
                item->Process();
            }

            if (item->IsFinishing() && item->IsFinished()) {
                item_to_delete.emplace_back(item);
            }
        }

        for (auto* item : item_to_delete) {
            DestroyItem(item);
        }
    }

    if (Scroll()) {
        RebuildFog();
    }
}

void MapView::AddTile(const MapTile& tile)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    if (!tile.IsRoof) {
        _tilesField[tile.HexY * _maxHexX + tile.HexX].push_back(tile);
        _tilesField[tile.HexY * _maxHexX + tile.HexX].back().IsSelected = false;
    }
    else {
        _roofsField[tile.HexY * _maxHexX + tile.HexX].push_back(tile);
        _roofsField[tile.HexY * _maxHexX + tile.HexX].back().IsSelected = false;
    }

    const auto anim_name = _engine->ResolveHash(tile.NameHash);
    AnyFrames* anim = _engine->ResMngr.GetItemAnim(anim_name);
    if (anim != nullptr) {
        Field& field = GetField(tile.HexX, tile.HexY);
        Field::Tile& ftile = field.AddTile(anim, tile.OffsX, tile.OffsY, tile.Layer, tile.IsRoof);
        MeasureMapBorders(ftile, tile.IsRoof);
    }
}

void MapView::AddMapText(string_view str, uint16 hx, uint16 hy, uint color, time_duration show_time, bool fade, int ox, int oy)
{
    STACK_TRACE_ENTRY();

    MapText map_text;
    map_text.HexX = hx;
    map_text.HexY = hy;
    map_text.Color = (color != 0u ? color : COLOR_TEXT);
    map_text.Fade = fade;
    map_text.StartTime = _engine->GameTime.GameplayTime();
    map_text.Duration = show_time != time_duration {} ? show_time : std::chrono::milliseconds {_engine->Settings.TextDelay + static_cast<uint>(str.length()) * 100};
    map_text.Text = str;
    map_text.Pos = GetRectForText(hx, hy);
    map_text.EndPos = IRect(map_text.Pos, ox, oy);

    const auto it = std::find_if(_mapTexts.begin(), _mapTexts.end(), [&map_text](const MapText& t) { return map_text.HexX == t.HexX && map_text.HexY == t.HexY; });
    if (it != _mapTexts.end()) {
        _mapTexts.erase(it);
    }

    _mapTexts.emplace_back(std::move(map_text));
}

auto MapView::GetViewWidth() const -> int
{
    STACK_TRACE_ENTRY();

    return iround(static_cast<float>(_engine->Settings.ScreenWidth / _engine->Settings.MapHexWidth + ((_engine->Settings.ScreenWidth % _engine->Settings.MapHexWidth) != 0 ? 1 : 0)) * GetSpritesZoom());
}

auto MapView::GetViewHeight() const -> int
{
    STACK_TRACE_ENTRY();

    return iround(static_cast<float>((_engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight) / _engine->Settings.MapHexLineHeight + (((_engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight) % _engine->Settings.MapHexLineHeight) != 0 ? 1 : 0)) * GetSpritesZoom());
}

void MapView::AddItemToField(ItemHexView* item)
{
    STACK_TRACE_ENTRY();

    const uint16 hx = item->GetHexX();
    const uint16 hy = item->GetHexY();

    auto& field = GetField(hx, hy);
    field.AddItem(item, nullptr);

    if (item->IsNonEmptyBlockLines()) {
        _engine->Geometry.ForEachBlockLines(item->GetBlockLines(), hx, hy, _maxHexX, _maxHexY, [this, item](auto hx2, auto hy2) { GetField(hx2, hy2).AddItem(nullptr, item); });
    }
}

void MapView::RemoveItemFromField(ItemHexView* item)
{
    STACK_TRACE_ENTRY();

    const uint16 hx = item->GetHexX();
    const uint16 hy = item->GetHexY();

    auto& field = GetField(hx, hy);
    field.EraseItem(item, nullptr);

    if (item->IsNonEmptyBlockLines()) {
        _engine->Geometry.ForEachBlockLines(item->GetBlockLines(), hx, hy, _maxHexX, _maxHexY, [this, item](auto hx2, auto hy2) { GetField(hx2, hy2).EraseItem(nullptr, item); });
    }
}

auto MapView::AddItem(ident_t id, const ProtoItem* proto, const map<string, string>& props_kv) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(id);

    if (_itemsMap.count(id) != 0) {
        return nullptr;
    }

    auto* item = new ItemHexView(this, id, proto, proto->GetProperties());
    if (!item->LoadFromText(props_kv)) {
        item->Release();
        return nullptr;
    }

    AddItemInternal(item);

    return item;
}

auto MapView::AddItem(ident_t id, hstring pid, uint16 hx, uint16 hy, bool is_added, const vector<vector<uint8>>* data) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(id);
    RUNTIME_ASSERT(!(hx >= _maxHexX || hy >= _maxHexY));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    RUNTIME_ASSERT(proto);

    auto* item = new ItemHexView(this, id, proto, data, hx, hy);

    if (is_added) {
        item->PlayShowAnim();
    }

    AddItemInternal(item);

    return item;
}

void MapView::AddItemInternal(ItemHexView* item)
{
    STACK_TRACE_ENTRY();

    const auto hx = item->GetHexX();
    const auto hy = item->GetHexY();
    RUNTIME_ASSERT(hx < _maxHexX && hy < _maxHexY);

    if (item->GetId()) {
        if (auto* prev_item = GetItem(item->GetId()); prev_item != nullptr) {
            // Todo: rework smooth item re-appearing before same item still on map
            /*if (item_old->GetProtoId() == pid && item_old->GetHexX() == hx && item_old->GetHexY() == hy) {
                item_old->IsDestroyed() = false;
                if (item_old->IsFinishing()) {
                    item_old->StopFinishing();
                }
                if (data != nullptr) {
                    item_old->RestoreData(*data);
                }
                return item_old->GetId();
            }*/

            DestroyItem(prev_item);
        }
    }

    _allItems.emplace_back(item);

    if (item->IsStatic()) {
        _staticItems.emplace_back(item);
    }
    else {
        _dynamicItems.emplace_back(item);
    }

    if (item->GetId()) {
        _itemsMap.emplace(item->GetId(), item);
    }

    AddItemToField(item);

    if (item->IsStatic() && item->GetIsLight()) {
        _staticLightSources.push_back({item->GetHexX(), item->GetHexY(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity()});
    }

    if (!MeasureMapBorders(item->SprId, item->GetOffsetX(), item->GetOffsetY())) {
        auto& field = GetField(hx, hy);

        if (IsHexToDraw(hx, hy) && !item->GetIsHidden() && !item->GetIsHiddenPicture() && !item->IsFullyTransparent()) {
            auto& spr = _mainTree.InsertSprite(EvaluateItemDrawOrder(item), hx, hy + item->GetDrawOrderOffsetHexY(), //
                (_engine->Settings.MapHexWidth / 2), (_engine->Settings.MapHexHeight / 2), &field.ScrX, &field.ScrY, //
                0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
            if (!item->GetIsNoLightInfluence()) {
                spr.SetLight(item->GetCorner(), _hexLight.data(), _maxHexX, _maxHexY);
            }
            item->SetSprite(&spr);

            field.AddSpriteToChain(&spr);
        }

        if (item->GetIsLight() || !item->GetIsLightThru()) {
            RebuildLight();
        }
    }

    // Sort
    const auto& field = GetField(hx, hy);
    std::sort(field.Items->begin(), field.Items->end(), [](auto* i1, auto* i2) { return i1->IsScenery() && !i2->IsScenery(); });
    std::sort(field.Items->begin(), field.Items->end(), [](auto* i1, auto* i2) { return i1->IsWall() && !i2->IsWall(); });
}

void MapView::MoveItem(ItemHexView* item, uint16 hx, uint16 hy)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item->GetMap() == this);

    RemoveItemFromField(item);
    item->SetHexX(hx);
    item->SetHexY(hy);
    AddItemToField(item);

    auto& field = GetField(hx, hy);

    if (item->SprDrawValid) {
        item->SprDraw->Unvalidate();
    }

    if (IsHexToDraw(hx, hy)) {
        item->SprDraw = &_mainTree.InsertSprite(EvaluateItemDrawOrder(item), hx, hy + item->GetDrawOrderOffsetHexY(), //
            (_engine->Settings.MapHexWidth / 2), (_engine->Settings.MapHexHeight / 2), //
            &field.ScrX, &field.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
        if (!item->GetIsNoLightInfluence()) {
            item->SprDraw->SetLight(item->GetCorner(), _hexLight.data(), _maxHexX, _maxHexY);
        }
        field.AddSpriteToChain(item->SprDraw);
    }
}

void MapView::DestroyItem(ItemHexView* item)
{
    STACK_TRACE_ENTRY();

    if (item->SprDrawValid) {
        item->SprDraw->Unvalidate();
    }

    {
        const auto it = std::find(_allItems.begin(), _allItems.end(), item);
        RUNTIME_ASSERT(it != _allItems.end());
        _allItems.erase(it);
    }

    if (item->IsStatic()) {
        const auto it = std::find(_staticItems.begin(), _staticItems.end(), item);
        RUNTIME_ASSERT(it != _staticItems.end());
        _staticItems.erase(it);
    }
    else {
        const auto it = std::find(_dynamicItems.begin(), _dynamicItems.end(), item);
        RUNTIME_ASSERT(it != _dynamicItems.end());
        _dynamicItems.erase(it);
    }

    if (item->GetId()) {
        const auto it = _itemsMap.find(item->GetId());
        RUNTIME_ASSERT(it != _itemsMap.end());
        _itemsMap.erase(it);
    }

    RemoveItemFromField(item);

    if (item->GetIsLight() || !item->GetIsLightThru()) {
        RebuildLight();
    }

    item->MarkAsDestroyed();
    item->Release();
}

void MapView::SkipItemsFade()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* item : _allItems) {
        item->SkipFade();
    }
}

auto MapView::GetItem(uint16 hx, uint16 hy, hstring pid) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    if (hx >= _maxHexX || hy >= _maxHexY || GetField(hx, hy).Items == nullptr) {
        return nullptr;
    }

    for (auto* item : *GetField(hx, hy).Items) {
        if (item->GetProtoId() == pid) {
            return item;
        }
    }

    return nullptr;
}

auto MapView::GetItem(uint16 hx, uint16 hy, ident_t id) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    if (hx >= _maxHexX || hy >= _maxHexY || GetField(hx, hy).Items == nullptr) {
        return nullptr;
    }

    for (auto* item : *GetField(hx, hy).Items) {
        if (item->GetId() == id) {
            return item;
        }
    }

    return nullptr;
}

auto MapView::GetItem(ident_t id) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _itemsMap.find(id); it != _itemsMap.end()) {
        return it->second;
    }

    return nullptr;
}

auto MapView::GetItems() -> const vector<ItemHexView*>&
{
    STACK_TRACE_ENTRY();

    return _allItems;
}

auto MapView::GetItems(uint16 hx, uint16 hy) -> const vector<ItemHexView*>&
{
    STACK_TRACE_ENTRY();

    const auto& field = GetField(hx, hy);
    if (field.Items != nullptr) {
        return *field.Items;
    }

    return _emptyList;
}

auto MapView::GetRectForText(uint16 hx, uint16 hy) -> IRect
{
    STACK_TRACE_ENTRY();

    auto result = IRect();

    if (const auto& field = GetField(hx, hy); field.IsView) {
        if (field.Critters != nullptr) {
            for (const auto* cr : *field.Critters) {
                if (cr->SprDrawValid) {
                    const auto rect = cr->GetViewRect();
                    if (result.IsZero()) {
                        result = rect;
                    }
                    else {
                        result.Left = std::min(result.Left, rect.Left);
                        result.Top = std::min(result.Top, rect.Top);
                        result.Right = std::max(result.Right, rect.Right);
                        result.Bottom = std::max(result.Bottom, rect.Bottom);
                    }
                }
            }
        }

        if (field.Items != nullptr) {
            for (const auto* item : *field.Items) {
                if (item->SprDrawValid) {
                    const auto* si = _engine->SprMngr.GetSpriteInfo(item->SprId);
                    if (si != nullptr) {
                        const auto l = field.ScrX + _engine->Settings.MapHexWidth / 2 - si->OffsX;
                        const auto t = field.ScrY + _engine->Settings.MapHexHeight / 2 - si->OffsY;
                        const auto r = l + si->Width;
                        const auto b = t + si->Height;
                        if (result.IsZero()) {
                            result = IRect {l, t, r, b};
                        }
                        else {
                            result.Left = std::min(result.Left, l);
                            result.Top = std::min(result.Top, t);
                            result.Right = std::max(result.Right, r);
                            result.Bottom = std::max(result.Bottom, b);
                        }
                    }
                }
            }
        }
    }

    return {-result.Width() / 2, -result.Height(), result.Width() / 2, 0};
}

auto MapView::RunEffectItem(hstring eff_pid, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!(from_hx >= _maxHexX || from_hy >= _maxHexY || to_hx >= _maxHexX || to_hy >= _maxHexY));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(eff_pid);
    RUNTIME_ASSERT(proto);
    RUNTIME_ASSERT(proto->IsStatic());

    auto* effect_item = new ItemHexView(this, ident_t {}, proto, nullptr, from_hx, from_hy);
    effect_item->SetEffect(to_hx, to_hy);

    AddItemToField(effect_item);

    _allItems.emplace_back(effect_item);
    _staticItems.emplace_back(effect_item);

    if (IsHexToDraw(from_hx, from_hy)) {
        auto& field = GetField(from_hx, from_hy);
        effect_item->SprDraw = &_mainTree.InsertSprite(EvaluateItemDrawOrder(effect_item), from_hx, from_hy + effect_item->GetDrawOrderOffsetHexY(), //
            _engine->Settings.MapHexWidth / 2, (_engine->Settings.MapHexHeight / 2), &field.ScrX, &field.ScrY, //
            0, &effect_item->SprId, &effect_item->ScrX, &effect_item->ScrY, &effect_item->Alpha, &effect_item->DrawEffect, &effect_item->SprDrawValid);
        if (!effect_item->GetIsNoLightInfluence()) {
            effect_item->SprDraw->SetLight(effect_item->GetCorner(), _hexLight.data(), _maxHexX, _maxHexY);
        }
        field.AddSpriteToChain(effect_item->SprDraw);
    }

    return true;
}

void MapView::SetCursorPos(CritterHexView* cr, int x, int y, bool show_steps, bool refresh)
{
    STACK_TRACE_ENTRY();

    uint16 hx = 0;
    uint16 hy = 0;
    if (GetHexAtScreenPos(x, y, hx, hy, nullptr, nullptr)) {
        const auto& field = GetField(hx, hy);

        _cursorX = field.ScrX + 1 - 1;
        _cursorY = field.ScrY - 1 - 1;

        if (cr == nullptr) {
            _drawCursorX = -1;
            return;
        }

        const auto cx = cr->GetHexX();
        const auto cy = cr->GetHexY();
        const auto mh = cr->GetMultihex();

        if ((cx == hx && cy == hy) || (field.Flags.IsMoveBlocked && (mh == 0u || !_engine->Geometry.CheckDist(cx, cy, hx, hy, mh)))) {
            _drawCursorX = -1;
        }
        else {
            if (refresh || hx != _lastCurHx || hy != _lastCurHy) {
                if (cr->IsAlive()) {
                    const auto find_path = FindPath(cr, cx, cy, hx, hy, -1);
                    if (!find_path) {
                        _drawCursorX = -1;
                    }
                    else {
                        _drawCursorX = static_cast<int>(show_steps ? find_path->Steps.size() : 0);
                    }
                }
                else {
                    _drawCursorX = -1;
                }

                _lastCurHx = hx;
                _lastCurHy = hy;
                _lastCurX = _drawCursorX;
            }
            else {
                _drawCursorX = _lastCurX;
            }
        }
    }
}

void MapView::DrawCursor(uint spr_id)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_engine->Settings.HideCursor || !_engine->Settings.ShowMoveCursor) {
        return;
    }

    const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
    if (si != nullptr) {
        _engine->SprMngr.DrawSpriteSize(spr_id,
            iround(static_cast<float>(_cursorX + _engine->Settings.ScrOx) / GetSpritesZoom()), //
            iround(static_cast<float>(_cursorY + _engine->Settings.ScrOy) / GetSpritesZoom()), //
            iround(static_cast<float>(si->Width) / GetSpritesZoom()), //
            iround(static_cast<float>(si->Height) / GetSpritesZoom()), true, false, 0);
    }
}

void MapView::DrawCursor(string_view text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_engine->Settings.HideCursor || !_engine->Settings.ShowMoveCursor) {
        return;
    }

    const auto x = iround(static_cast<float>(_cursorX + _engine->Settings.ScrOx) / GetSpritesZoom());
    const auto y = iround(static_cast<float>(_cursorY + _engine->Settings.ScrOy) / GetSpritesZoom());
    const auto r = IRect(x, y, iround(static_cast<float>(x + _engine->Settings.MapHexWidth) / GetSpritesZoom()), //
        iround(static_cast<float>(y + _engine->Settings.MapHexHeight) / GetSpritesZoom()));

    _engine->SprMngr.DrawStr(r, text, FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, -1);
}

void MapView::RebuildMap(int rx, int ry)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_viewField.empty());

    for (auto i = 0, j = _hVisible * _wVisible; i < j; i++) {
        const auto hx = _viewField[i].HexX;
        const auto hy = _viewField[i].HexY;
        if (hx < 0 || hy < 0 || hx >= _maxHexX || hy >= _maxHexY) {
            continue;
        }

        auto& field = GetField(static_cast<uint16>(hx), static_cast<uint16>(hy));
        field.IsView = false;
        field.UnvalidateSpriteChain();
    }

    InitView(rx, ry);

    // Light
    RealRebuildLight();
    _requestRebuildLight = false;
    _requestRenderLight = true;

    // Tiles, roof
    RebuildTiles();
    RebuildRoof();

    // Erase old sprites
    _mainTree.Unvalidate();

    _engine->SprMngr.EggNotValid();

    // Begin generate new sprites
    for (const auto i : xrange(_hVisible * _wVisible)) {
        const auto& vf = _viewField[i];

        if (vf.HexX < 0 || vf.HexY < 0 || vf.HexX >= _maxHexX || vf.HexY >= _maxHexY) {
            continue;
        }

        const auto nx = static_cast<uint16>(vf.HexX);
        const auto ny = static_cast<uint16>(vf.HexY);

        auto& field = GetField(nx, ny);

        field.IsView = true;
        field.ScrX = vf.ScrX;
        field.ScrY = vf.ScrY;

        // Track
        if (_isShowTrack && GetHexTrack(nx, ny) != 0) {
            const auto spr_id = (GetHexTrack(nx, ny) == 1 ? _picTrack1->GetCurSprId(_engine->GameTime.GameplayTime()) : _picTrack2->GetCurSprId(_engine->GameTime.GameplayTime()));
            const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
            auto& spr = _mainTree.AddSprite(DrawOrderType::Track, nx, ny, //
                _engine->Settings.MapHexWidth / 2, (_engine->Settings.MapHexHeight / 2) + (si != nullptr ? si->Height / 2 : 0), &field.ScrX, &field.ScrY, //
                spr_id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            field.AddSpriteToChain(&spr);
        }

        // Hex Lines
        if (_isShowHex) {
            const auto spr_id = _picHex[0]->GetCurSprId(_engine->GameTime.GameplayTime());
            const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
            auto& spr = _mainTree.AddSprite(DrawOrderType::HexGrid, nx, ny, //
                si != nullptr ? si->Width / 2 : 0, si != nullptr ? si->Height : 0, &field.ScrX, &field.ScrY, //
                spr_id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            field.AddSpriteToChain(&spr);
        }

        // Items on hex
        if (field.Items != nullptr) {
            for (auto* item : *field.Items) {
                if (!_mapperMode) {
                    if (item->GetIsHidden() || item->GetIsHiddenPicture() || item->IsFullyTransparent()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowScen && item->IsScenery()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowItem && !item->IsAnyScenery()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowWall && item->IsWall()) {
                        continue;
                    }
                }
                else {
                    const auto is_fast = _fastPids.count(item->GetProtoId()) != 0;
                    if (!_engine->Settings.ShowScen && !is_fast && item->IsScenery()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowItem && !is_fast && !item->IsAnyScenery()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowWall && !is_fast && item->IsWall()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowFast && is_fast) {
                        continue;
                    }
                    if (_ignorePids.count(item->GetProtoId()) != 0u) {
                        continue;
                    }
                }

                auto& spr = _mainTree.AddSprite(EvaluateItemDrawOrder(item), nx, ny + item->GetDrawOrderOffsetHexY(), //
                    _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, //
                    0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
                if (!item->GetIsNoLightInfluence()) {
                    spr.SetLight(item->GetCorner(), _hexLight.data(), _maxHexX, _maxHexY);
                }
                item->SetSprite(&spr);
                field.AddSpriteToChain(&spr);
            }
        }

        // Critters
        if (field.Critters != nullptr && _engine->Settings.ShowCrit) {
            for (auto* cr : *field.Critters) {
                if (!cr->Visible) {
                    continue;
                }

                auto& spr = _mainTree.AddSprite(EvaluateCritterDrawOrder(cr), nx, ny, //
                    _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, //
                    0, &cr->SprId, &cr->SprOx, &cr->SprOy, &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid);
                spr.SetLight(CornerType::EastWest, _hexLight.data(), _maxHexX, _maxHexY);
                cr->SprDraw = &spr;

                cr->RefreshOffs();
                cr->ResetOk();

                auto contour = ContourType::None;
                if (cr->GetId() == _critterContourCrId) {
                    contour = _critterContour;
                }
                else if (!cr->IsChosen()) {
                    contour = _crittersContour;
                }
                spr.SetContour(contour, cr->GetContourColor());

                field.AddSpriteToChain(&spr);
            }
        }
    }
    _mainTree.SortByMapPos();

    _screenHexX = rx;
    _screenHexY = ry;

    _engine->OnRenderMap.Fire();
}

void MapView::RebuildMapOffset(int ox, int oy)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_viewField.empty());
    RUNTIME_ASSERT(ox == 0 || ox == -1 || ox == 1);
    RUNTIME_ASSERT(oy == 0 || oy == -2 || oy == 2);

    auto hide_hex = [this](ViewField& vf) {
        const auto nxi = vf.HexX;
        const auto nyi = vf.HexY;
        if (nxi < 0 || nyi < 0 || nxi >= _maxHexX || nyi >= _maxHexY) {
            return;
        }

        const auto nx = static_cast<uint16>(nxi);
        const auto ny = static_cast<uint16>(nyi);
        if (!IsHexToDraw(nx, ny)) {
            return;
        }

        auto& field = GetField(nx, ny);
        field.IsView = false;
        field.UnvalidateSpriteChain();
    };

    if (ox != 0) {
        const auto from_x = (ox > 0 ? 0 : _wVisible + ox);
        const auto to_x = (ox > 0 ? ox : _wVisible);
        for (auto x = from_x; x < to_x; x++) {
            for (auto y = 0; y < _hVisible; y++) {
                hide_hex(_viewField[y * _wVisible + x]);
            }
        }
    }

    if (oy != 0) {
        const auto from_y = (oy > 0 ? 0 : _hVisible + oy);
        const auto to_y = (oy > 0 ? oy : _hVisible);
        for (auto y = from_y; y < to_y; y++) {
            for (auto x = 0; x < _wVisible; x++) {
                hide_hex(_viewField[y * _wVisible + x]);
            }
        }
    }

    const auto vpos1 = 5 * _wVisible + 4;
    const auto vpos2 = (5 + oy) * _wVisible + 4 + ox;
    _screenHexX += _viewField[vpos2].HexX - _viewField[vpos1].HexX;
    _screenHexY += _viewField[vpos2].HexY - _viewField[vpos1].HexY;

    for (const auto i : xrange(_wVisible * _hVisible)) {
        auto& vf = _viewField[i];

        if (ox < 0) {
            vf.HexX--;
            if ((vf.HexX % 2) != 0) {
                vf.HexY++;
            }
        }
        else if (ox > 0) {
            vf.HexX++;
            if ((vf.HexX % 2) == 0) {
                vf.HexY--;
            }
        }

        if (oy < 0) {
            vf.HexX--;
            vf.HexY--;
            if ((vf.HexX % 2) == 0) {
                vf.HexY--;
            }
        }
        else if (oy > 0) {
            vf.HexX++;
            vf.HexY++;
            if ((vf.HexX % 2) != 0) {
                vf.HexY++;
            }
        }

        if (vf.HexX >= 0 && vf.HexY >= 0 && vf.HexX < _maxHexX && vf.HexY < _maxHexY) {
            auto& field = GetField(static_cast<uint16>(vf.HexX), static_cast<uint16>(vf.HexY));
            field.ScrX = vf.ScrX;
            field.ScrY = vf.ScrY;
        }
    }

    auto show_hex = [this](ViewField& vf) {
        if (vf.HexX < 0 || vf.HexY < 0 || vf.HexX >= _maxHexX || vf.HexY >= _maxHexY) {
            return;
        }

        const auto nx = static_cast<uint16>(vf.HexX);
        const auto ny = static_cast<uint16>(vf.HexY);

        if (IsHexToDraw(nx, ny)) {
            return;
        }

        auto& field = GetField(nx, ny);
        field.IsView = true;

        // Track
        if (_isShowTrack && (GetHexTrack(nx, ny) != 0)) {
            const auto spr_id = (GetHexTrack(nx, ny) == 1 ? _picTrack1->GetCurSprId(_engine->GameTime.GameplayTime()) : _picTrack2->GetCurSprId(_engine->GameTime.GameplayTime()));
            const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
            auto& spr = _mainTree.InsertSprite(DrawOrderType::Track, nx, ny, //
                _engine->Settings.MapHexWidth / 2, (_engine->Settings.MapHexHeight / 2) + (si != nullptr ? si->Height / 2 : 0), &field.ScrX, &field.ScrY, //
                spr_id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            field.AddSpriteToChain(&spr);
        }

        // Hex lines
        if (_isShowHex) {
            const auto spr_id = _picHex[0]->GetCurSprId(_engine->GameTime.GameplayTime());
            const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
            auto& spr = _mainTree.InsertSprite(DrawOrderType::HexGrid, nx, ny, //
                si != nullptr ? si->Width / 2 : 0, si != nullptr ? si->Height : 0, &field.ScrX, &field.ScrY, //
                spr_id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            field.AddSpriteToChain(&spr);
        }

        // Items on hex
        if (field.Items != nullptr) {
            for (auto* item : *field.Items) {
                if (!_mapperMode) {
                    if (item->GetIsHidden() || item->GetIsHiddenPicture() || item->IsFullyTransparent()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowScen && item->IsScenery()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowItem && !item->IsAnyScenery()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowWall && item->IsWall()) {
                        continue;
                    }
                }
                else {
                    const auto is_fast = _fastPids.count(item->GetProtoId()) != 0;
                    if (!_engine->Settings.ShowScen && !is_fast && item->IsScenery()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowItem && !is_fast && !item->IsAnyScenery()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowWall && !is_fast && item->IsWall()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowFast && is_fast) {
                        continue;
                    }
                    if (_ignorePids.count(item->GetProtoId()) != 0u) {
                        continue;
                    }
                }

                auto& spr = _mainTree.InsertSprite(EvaluateItemDrawOrder(item), nx, ny + item->GetDrawOrderOffsetHexY(), //
                    _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, //
                    0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
                if (!item->GetIsNoLightInfluence()) {
                    spr.SetLight(item->GetCorner(), _hexLight.data(), _maxHexX, _maxHexY);
                }
                item->SetSprite(&spr);
                field.AddSpriteToChain(&spr);
            }
        }

        // Critters
        if (field.Critters != nullptr && _engine->Settings.ShowCrit) {
            for (auto* cr : *field.Critters) {
                if (!cr->Visible) {
                    continue;
                }

                auto& spr = _mainTree.InsertSprite(EvaluateCritterDrawOrder(cr), nx, ny, //
                    _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, //
                    0, &cr->SprId, &cr->SprOx, &cr->SprOy, &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid);
                spr.SetLight(CornerType::EastWest, _hexLight.data(), _maxHexX, _maxHexY);
                cr->SprDraw = &spr;

                cr->RefreshOffs();
                cr->ResetOk();

                auto contour = ContourType::None;
                if (cr->GetId() == _critterContourCrId) {
                    contour = _critterContour;
                }
                else if (!cr->IsChosen()) {
                    contour = _crittersContour;
                }
                spr.SetContour(contour, cr->GetContourColor());

                field.AddSpriteToChain(&spr);
            }
        }

        // Tiles
        const auto tiles_count = field.GetTilesCount(false);
        if (_engine->Settings.ShowTile && (tiles_count != 0u)) {
            for (uint i = 0; i < tiles_count; i++) {
                const auto& tile = field.GetTile(i, false);
                const auto spr_id = tile.Anim->GetSprId(0);

                if (_mapperMode) {
                    auto& tiles = GetTiles(nx, ny, false);
                    auto& spr = _tilesTree.InsertSprite(EvaluateTileDrawOrder(tile), nx, ny, //
                        tile.OffsX + _engine->Settings.MapTileOffsX, tile.OffsY + _engine->Settings.MapTileOffsY, &field.ScrX, &field.ScrY, //
                        spr_id, nullptr, nullptr, nullptr, tiles[i].IsSelected ? &SelectAlpha : nullptr, &_engine->EffectMngr.Effects.Tile, nullptr);
                    field.AddSpriteToChain(&spr);
                }
                else {
                    auto& spr = _tilesTree.InsertSprite(EvaluateTileDrawOrder(tile), nx, ny, //
                        tile.OffsX + _engine->Settings.MapTileOffsX, tile.OffsY + _engine->Settings.MapTileOffsY, &field.ScrX, &field.ScrY, //
                        spr_id, nullptr, nullptr, nullptr, nullptr, &_engine->EffectMngr.Effects.Tile, nullptr);
                    field.AddSpriteToChain(&spr);
                }
            }
        }

        // Roof
        const auto roofs_count = field.GetTilesCount(true);
        if (_engine->Settings.ShowRoof && (roofs_count != 0u) && ((_roofSkip == 0) || _roofSkip != field.RoofNum)) {
            for (uint i = 0; i < roofs_count; i++) {
                const auto& roof = field.GetTile(i, true);
                const auto spr_id = roof.Anim->GetSprId(0);

                if (_mapperMode) {
                    auto& roofs = GetTiles(nx, ny, true);
                    auto& spr = _roofTree.InsertSprite(EvaluateTileDrawOrder(roof), nx, ny, //
                        roof.OffsX + _engine->Settings.MapRoofOffsX, roof.OffsY + _engine->Settings.MapRoofOffsY, &field.ScrX, &field.ScrY, //
                        spr_id, nullptr, nullptr, nullptr, roofs[i].IsSelected ? &SelectAlpha : &_engine->Settings.RoofAlpha, &_engine->EffectMngr.Effects.Roof, nullptr);
                    spr.SetEggAppearence(EggAppearenceType::Always);
                    field.AddSpriteToChain(&spr);
                }
                else {
                    auto& spr = _roofTree.InsertSprite(EvaluateTileDrawOrder(roof), nx, ny, //
                        roof.OffsX + _engine->Settings.MapRoofOffsX, roof.OffsY + _engine->Settings.MapRoofOffsY, &field.ScrX, &field.ScrY, //
                        spr_id, nullptr, nullptr, nullptr, &_engine->Settings.RoofAlpha, &_engine->EffectMngr.Effects.Roof, nullptr);
                    spr.SetEggAppearence(EggAppearenceType::Always);
                    field.AddSpriteToChain(&spr);
                }
            }
        }
    };

    if (ox != 0) {
        const auto from_x = (ox > 0 ? _wVisible - ox : 0);
        const auto to_x = (ox > 0 ? _wVisible : -ox);
        for (auto x = from_x; x < to_x; x++) {
            for (auto y = 0; y < _hVisible; y++) {
                show_hex(_viewField[y * _wVisible + x]);
            }
        }
    }

    if (oy != 0) {
        const auto from_y = (oy > 0 ? _hVisible - oy : 0);
        const auto to_y = (oy > 0 ? _hVisible : -oy);
        for (auto y = from_y; y < to_y; y++) {
            for (auto x = 0; x < _wVisible; x++) {
                show_hex(_viewField[y * _wVisible + x]);
            }
        }
    }

    // Critters text rect
    for (auto* cr : _critters) {
        cr->RefreshOffs();
    }

    // Light
    RealRebuildLight();
    _requestRebuildLight = false;
    _requestRenderLight = true;

    _engine->OnRenderMap.Fire();
}

void MapView::ClearHexLight()
{
    STACK_TRACE_ENTRY();

    std::memset(_hexLight.data(), 0, _hexLight.size() * sizeof(uint8));
}

void MapView::PrepareLightToDraw()
{
    STACK_TRACE_ENTRY();

    if (_rtLight == nullptr) {
        return;
    }

    // Rebuild light
    if (_requestRebuildLight) {
        _requestRebuildLight = false;
        RealRebuildLight();
    }

    // Check dynamic light sources
    if (!_requestRenderLight) {
        for (auto& ls : _lightSources) {
            if (ls.OffsX != nullptr && (*ls.OffsX != ls.LastOffsX || *ls.OffsY != ls.LastOffsY)) {
                ls.LastOffsX = *ls.OffsX;
                ls.LastOffsY = *ls.OffsY;
                _requestRenderLight = true;
            }
        }
    }

    // Prerender light
    if (_requestRenderLight) {
        _requestRenderLight = false;
        _engine->SprMngr.PushRenderTarget(_rtLight);
        _engine->SprMngr.ClearCurrentRenderTarget(0);
        const auto zoom = GetSpritesZoom();
        const auto offset = FPoint(static_cast<float>(_rtScreenOx), static_cast<float>(_rtScreenOy));
        for (size_t i = 0; i < _lightPointsCount; i++) {
            _engine->SprMngr.DrawPoints(_lightPoints[i], RenderPrimitiveType::TriangleStrip, &zoom, &offset, _engine->EffectMngr.Effects.Light);
        }
        _engine->SprMngr.DrawPoints(_lightSoftPoints, RenderPrimitiveType::TriangleList, &zoom, &offset, _engine->EffectMngr.Effects.Light);
        _engine->SprMngr.PopRenderTarget();
    }
}

void MapView::MarkLight(uint16 hx, uint16 hy, uint inten)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto light = static_cast<int>(inten) * MAX_LIGHT_HEX / MAX_LIGHT_VALUE * _lightCapacity / 100;
    const auto lr = light * _lightProcentR / 100;
    const auto lg = light * _lightProcentG / 100;
    const auto lb = light * _lightProcentB / 100;
    auto* p = &_hexLight[hy * _maxHexX * 3 + hx * 3];

    if (lr > *p) {
        *p = static_cast<uint8>(lr);
    }
    if (lg > *(p + 1)) {
        *(p + 1) = static_cast<uint8>(lg);
    }
    if (lb > *(p + 2)) {
        *(p + 2) = static_cast<uint8>(lb);
    }
}

void MapView::MarkLightEndNeighbor(uint16 hx, uint16 hy, bool north_south, uint inten)
{
    STACK_TRACE_ENTRY();

    auto& field = GetField(hx, hy);
    if (field.Flags.IsWall) {
        const auto lt = field.Corner;
        if ((north_south && (lt == CornerType::NorthSouth || lt == CornerType::North || lt == CornerType::West)) || (!north_south && (lt == CornerType::EastWest || lt == CornerType::East)) || lt == CornerType::South) {
            auto* p = &_hexLight[hy * _maxHexX * 3 + hx * 3];
            const auto light_full = static_cast<int>(inten) * MAX_LIGHT_HEX / MAX_LIGHT_VALUE * _lightCapacity / 100;
            const auto light_self = static_cast<int>(inten / 2u) * MAX_LIGHT_HEX / MAX_LIGHT_VALUE * _lightCapacity / 100;
            const auto lr_full = light_full * _lightProcentR / 100;
            const auto lg_full = light_full * _lightProcentG / 100;
            const auto lb_full = light_full * _lightProcentB / 100;
            auto lr_self = static_cast<int>(*p) + light_self * _lightProcentR / 100;
            auto lg_self = static_cast<int>(*(p + 1)) + light_self * _lightProcentG / 100;
            auto lb_self = static_cast<int>(*(p + 2)) + light_self * _lightProcentB / 100;
            if (lr_self > lr_full) {
                lr_self = lr_full;
            }
            if (lg_self > lg_full) {
                lg_self = lg_full;
            }
            if (lb_self > lb_full) {
                lb_self = lb_full;
            }
            if (lr_self > *p) {
                *p = static_cast<uint8>(lr_self);
            }
            if (lg_self > *(p + 1)) {
                *(p + 1) = static_cast<uint8>(lg_self);
            }
            if (lb_self > *(p + 2)) {
                *(p + 2) = static_cast<uint8>(lb_self);
            }
        }
    }
}

void MapView::MarkLightEnd(uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy, uint inten)
{
    STACK_TRACE_ENTRY();

    auto is_wall = false;
    auto north_south = false;
    auto& field = GetField(to_hx, to_hy);
    if (field.Flags.IsWall) {
        is_wall = true;
        if (field.Corner == CornerType::NorthSouth || field.Corner == CornerType::North || field.Corner == CornerType::West) {
            north_south = true;
        }
    }

    const int dir = _engine->Geometry.GetFarDir(from_hx, from_hy, to_hx, to_hy);
    if (dir == 0 || (north_south && dir == 1) || (!north_south && (dir == 4 || dir == 5))) {
        MarkLight(to_hx, to_hy, inten);
        if (is_wall) {
            if (north_south) {
                if (to_hy > 0) {
                    MarkLightEndNeighbor(to_hx, to_hy - 1, true, inten);
                }
                if (to_hy < _maxHexY - 1) {
                    MarkLightEndNeighbor(to_hx, to_hy + 1, true, inten);
                }
            }
            else {
                if (to_hx > 0) {
                    MarkLightEndNeighbor(to_hx - 1, to_hy, false, inten);
                    if (to_hy > 0) {
                        MarkLightEndNeighbor(to_hx - 1, to_hy - 1, false, inten);
                    }
                    if (to_hy < _maxHexY - 1) {
                        MarkLightEndNeighbor(to_hx - 1, to_hy + 1, false, inten);
                    }
                }
                if (to_hx < _maxHexX - 1) {
                    MarkLightEndNeighbor(to_hx + 1, to_hy, false, inten);
                    if (to_hy > 0) {
                        MarkLightEndNeighbor(to_hx + 1, to_hy - 1, false, inten);
                    }
                    if (to_hy < _maxHexY - 1) {
                        MarkLightEndNeighbor(to_hx + 1, to_hy + 1, false, inten);
                    }
                }
            }
        }
    }
}

void MapView::MarkLightStep(uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy, uint inten)
{
    STACK_TRACE_ENTRY();

    auto& field = GetField(to_hx, to_hy);
    if (field.Flags.IsWallTransp) {
        const auto north_south = (field.Corner == CornerType::NorthSouth || field.Corner == CornerType::North || field.Corner == CornerType::West);
        const int dir = _engine->Geometry.GetFarDir(from_hx, from_hy, to_hx, to_hy);
        if (dir == 0 || (north_south && dir == 1) || (!north_south && (dir == 4 || dir == 5))) {
            MarkLight(to_hx, to_hy, inten);
        }
    }
    else {
        MarkLight(to_hx, to_hy, inten);
    }
}

void MapView::TraceLight(uint16 from_hx, uint16 from_hy, uint16& hx, uint16& hy, int dist, uint inten)
{
    STACK_TRACE_ENTRY();

    const auto [base_sx, base_sy] = GenericUtils::GetStepsCoords(from_hx, from_hy, hx, hy);
    const auto sx1_f = base_sx;
    const auto sy1_f = base_sy;
    auto curx1_f = static_cast<float>(from_hx);
    auto cury1_f = static_cast<float>(from_hy);
    int curx1_i = from_hx;
    int cury1_i = from_hy;
    const auto inten_sub = inten / dist;

    for (;;) {
        inten -= inten_sub;
        curx1_f += sx1_f;
        cury1_f += sy1_f;
        const auto old_curx1_i = curx1_i;
        const auto old_cury1_i = cury1_i;

        // Casts
        curx1_i = iround(curx1_f);
        if (curx1_f - static_cast<float>(curx1_i) >= 0.5f) {
            curx1_i++;
        }
        cury1_i = iround(cury1_f);
        if (cury1_f - static_cast<float>(cury1_i) >= 0.5f) {
            cury1_i++;
        }
        const auto can_mark = (curx1_i >= _lightMinHx && curx1_i <= _lightMaxHx && cury1_i >= _lightMinHy && cury1_i <= _lightMaxHy);

        // Left&Right trace
        auto ox = 0;
        auto oy = 0;

        if ((old_curx1_i & 1) != 0) {
            if (old_curx1_i + 1 == curx1_i && old_cury1_i + 1 == cury1_i) {
                ox = 1;
                oy = 1;
            }
            if (old_curx1_i - 1 == curx1_i && old_cury1_i + 1 == cury1_i) {
                ox = -1;
                oy = 1;
            }
        }
        else {
            if (old_curx1_i - 1 == curx1_i && old_cury1_i - 1 == cury1_i) {
                ox = -1;
                oy = -1;
            }
            if (old_curx1_i + 1 == curx1_i && old_cury1_i - 1 == cury1_i) {
                ox = 1;
                oy = -1;
            }
        }

        if (ox != 0) {
            // Left side
            ox = old_curx1_i + ox;
            if (ox < 0 || ox >= _maxHexX || GetField(ox, old_cury1_i).Flags.IsLightBlocked) {
                hx = (ox < 0 || ox >= _maxHexX ? old_curx1_i : ox);
                hy = old_cury1_i;
                if (can_mark) {
                    MarkLightEnd(old_curx1_i, old_cury1_i, hx, hy, inten);
                }
                break;
            }
            if (can_mark) {
                MarkLightStep(old_curx1_i, old_cury1_i, ox, old_cury1_i, inten);
            }

            // Right side
            oy = old_cury1_i + oy;
            if (oy < 0 || oy >= _maxHexY || GetField(old_curx1_i, oy).Flags.IsLightBlocked) {
                hx = old_curx1_i;
                hy = (oy < 0 || oy >= _maxHexY ? old_cury1_i : oy);
                if (can_mark) {
                    MarkLightEnd(old_curx1_i, old_cury1_i, hx, hy, inten);
                }
                break;
            }
            if (can_mark) {
                MarkLightStep(old_curx1_i, old_cury1_i, old_curx1_i, oy, inten);
            }
        }

        // Main trace
        if (curx1_i < 0 || curx1_i >= _maxHexX || cury1_i < 0 || cury1_i >= _maxHexY || GetField(curx1_i, cury1_i).Flags.IsLightBlocked) {
            hx = (curx1_i < 0 || curx1_i >= _maxHexX ? old_curx1_i : curx1_i);
            hy = (cury1_i < 0 || cury1_i >= _maxHexY ? old_cury1_i : cury1_i);
            if (can_mark) {
                MarkLightEnd(old_curx1_i, old_cury1_i, hx, hy, inten);
            }
            break;
        }
        if (can_mark) {
            MarkLightEnd(old_curx1_i, old_cury1_i, curx1_i, cury1_i, inten);
        }
        if (curx1_i == hx && cury1_i == hy) {
            break;
        }
    }
}

void MapView::ParseLightTriangleFan(const LightSource& ls)
{
    STACK_TRACE_ENTRY();

    // All dirs disabled
    if ((ls.Flags & 0x3F) == 0x3F) {
        return;
    }

    const auto hx = ls.HexX;
    const auto hy = ls.HexY;

    // Distance
    const auto dist = ls.Distance;
    if (dist < 1) {
        return;
    }

    // Intensity
    auto inten = std::abs(ls.Intensity);
    if (inten > 100) {
        inten = 100;
    }

    inten *= 100;

    if (IsBitSet(ls.Flags, LIGHT_GLOBAL)) {
        _lightCapacity = _globalDayLightCapacity;
        _hasGlobalLights = true;
    }
    else if (ls.Intensity >= 0) {
        _lightCapacity = _mapDayLightCapacity;
    }
    else {
        _lightCapacity = 100;
    }

    if (IsBitSet(ls.Flags, LIGHT_INVERSE)) {
        _lightCapacity = 100 - _lightCapacity;
    }

    // Color
    auto color = ls.ColorRGB;
    const auto alpha = MAX_LIGHT_ALPHA * _lightCapacity / 100 * inten / MAX_LIGHT_VALUE;
    color = COLOR_RGBA(alpha, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    _lightProcentR = ((color >> 16) & 0xFF) * 100 / 0xFF;
    _lightProcentG = ((color >> 8) & 0xFF) * 100 / 0xFF;
    _lightProcentB = (color & 0xFF) * 100 / 0xFF;

    // Begin
    MarkLight(hx, hy, inten);
    auto base_x = 0;
    auto base_y = 0;
    GetHexCurrentPosition(hx, hy, base_x, base_y);
    base_x += _engine->Settings.MapHexWidth / 2;
    base_y += _engine->Settings.MapHexHeight / 2;

    _lightPointsCount++;
    if (_lightPoints.size() < _lightPointsCount) {
        _lightPoints.emplace_back();
    }

    auto& points = _lightPoints[_lightPointsCount - 1];
    points.clear();
    points.reserve(dist * GameSettings::MAP_DIR_COUNT * 2);

    const auto center_point = PrimitivePoint {base_x, base_y, color, ls.OffsX, ls.OffsY};
    size_t added_points = 0;

    int hx_far = hx;
    int hy_far = hy;
    auto seek_start = true;
    uint16 last_hx = -1;
    uint16 last_hy = -1;

    for (auto i = 0, ii = (GameSettings::HEXAGONAL_GEOMETRY ? 6 : 4); i < ii; i++) {
        const auto dir = static_cast<uint8>(GameSettings::HEXAGONAL_GEOMETRY ? (i + 2) % 6 : ((i + 1) * 2) % 8);

        for (auto j = 0, jj = (GameSettings::HEXAGONAL_GEOMETRY ? dist : dist * 2); j < jj; j++) {
            if (seek_start) {
                // Move to start position
                for (auto l = 0; l < dist; l++) {
                    _engine->Geometry.MoveHexByDirUnsafe(hx_far, hy_far, GameSettings::HEXAGONAL_GEOMETRY ? 0 : 7);
                }
                seek_start = false;
                j = -1;
            }
            else {
                // Move to next hex
                _engine->Geometry.MoveHexByDirUnsafe(hx_far, hy_far, dir);
            }

            auto hx_ = static_cast<uint16>(std::clamp(hx_far, 0, _maxHexX - 1));
            auto hy_ = static_cast<uint16>(std::clamp(hy_far, 0, _maxHexY - 1));
            if (IsBitSet(ls.Flags, LIGHT_DISABLE_DIR(i))) {
                hx_ = hx;
                hy_ = hy;
            }
            else {
                TraceLight(hx, hy, hx_, hy_, dist, inten);
            }

            if (hx_ != last_hx || hy_ != last_hy) {
                int* ox = nullptr;
                int* oy = nullptr;
                if (static_cast<int>(hx_) != hx_far || static_cast<int>(hy_) != hy_far) {
                    int a = alpha - _engine->Geometry.DistGame(hx, hy, hx_, hy_) * alpha / dist;
                    a = std::clamp(a, 0, alpha);
                    color = COLOR_RGBA(a, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
                    if (hx_ == hx && hy_ == hy) {
                        ox = ls.OffsX;
                        oy = ls.OffsY;
                    }
                }
                else {
                    color = COLOR_RGBA(0, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
                    ox = ls.OffsX;
                    oy = ls.OffsY;
                }

                const auto [x, y] = _engine->Geometry.GetHexInterval(hx, hy, hx_, hy_);
                points.emplace_back(PrimitivePoint {base_x + x, base_y + y, color, ox, oy});
                if (++added_points % 2 == 0) {
                    points.emplace_back(center_point);
                }

                last_hx = hx_;
                last_hy = hy_;
            }
        }
    }

    for (size_t i = 0, j = points.size(); i < j; i++) {
        if (i % 3 == 0) {
            continue;
        }

        const auto& cur = points[i];
        const auto next_i = (i + 1) % 3 == 0 ? i + 2 : i + 1;
        const auto& next = points[next_i >= points.size() ? 0 : next_i];

        if (GenericUtils::DistSqrt(cur.PointX, cur.PointY, next.PointX, next.PointY) > static_cast<uint>(_engine->Settings.MapHexWidth)) {
            _lightSoftPoints.push_back({next.PointX, next.PointY, next.PointColor, next.PointOffsX, next.PointOffsY});
            _lightSoftPoints.push_back({cur.PointX, cur.PointY, cur.PointColor, cur.PointOffsX, cur.PointOffsY});

            const auto dist_comp = GenericUtils::DistSqrt(base_x, base_y, cur.PointX, cur.PointY) > GenericUtils::DistSqrt(base_x, base_y, next.PointX, next.PointY);
            auto x = static_cast<float>(dist_comp ? next.PointX - cur.PointX : cur.PointX - next.PointX);
            auto y = static_cast<float>(dist_comp ? next.PointY - cur.PointY : cur.PointY - next.PointY);
            std::tie(x, y) = GenericUtils::ChangeStepsCoords(x, y, dist_comp ? -2.5f : 2.5f);

            if (dist_comp) {
                _lightSoftPoints.push_back({cur.PointX + iround(x), cur.PointY + iround(y), cur.PointColor, cur.PointOffsX, cur.PointOffsY});
            }
            else {
                _lightSoftPoints.push_back({next.PointX + iround(x), next.PointY + iround(y), next.PointColor, next.PointOffsX, next.PointOffsY});
            }
        }
    }
}

void MapView::RealRebuildLight()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_viewField.empty());

    _hasGlobalLights = false;
    _lightPointsCount = 0;
    _lightSoftPoints.clear();
    ClearHexLight();
    CollectLightSources();

    _lightMinHx = _viewField[0].HexX;
    _lightMaxHx = _viewField[_hVisible * _wVisible - 1].HexX;
    _lightMinHy = _viewField[_wVisible - 1].HexY;
    _lightMaxHy = _viewField[_hVisible * _wVisible - _wVisible].HexY;

    for (auto& ls : _lightSources) {
        if (static_cast<int>(ls.HexX) >= _lightMinHx - ls.Distance && static_cast<int>(ls.HexX) <= _lightMaxHx + ls.Distance && //
            static_cast<int>(ls.HexY) >= _lightMinHy - ls.Distance && static_cast<int>(ls.HexY) <= _lightMaxHy + ls.Distance) {
            ParseLightTriangleFan(ls);
        }
    }
}

void MapView::CollectLightSources()
{
    STACK_TRACE_ENTRY();

    _lightSources.clear();

    // Scenery
    if (_mapperMode) {
        for (const auto* item : _staticItems) {
            if (item->GetIsLight()) {
                _lightSources.push_back({item->GetHexX(), item->GetHexY(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity()});
            }
        }
    }
    else {
        _lightSources = _staticLightSources;
    }

    // Items on ground
    for (const auto* item : _dynamicItems) {
        if (item->GetIsLight()) {
            _lightSources.push_back({item->GetHexX(), item->GetHexY(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity()});
        }
    }

    // Items in critters slots
    for (auto* cr : _critters) {
        auto added = false;
        for (const auto* item : cr->GetItems()) {
            if (item->GetIsLight() && item->GetCritterSlot() != 0u) {
                _lightSources.push_back({cr->GetHexX(), cr->GetHexY(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity(), &cr->SprOx, &cr->SprOy});
                added = true;
            }
        }

        // Default chosen light
        if (!_mapperMode) {
            if (cr->IsChosen() && !added) {
                _lightSources.push_back({cr->GetHexX(), cr->GetHexY(), _engine->Settings.ChosenLightColor, _engine->Settings.ChosenLightDistance, _engine->Settings.ChosenLightFlags, _engine->Settings.ChosenLightIntensity, &cr->SprOx, &cr->SprOy});
            }
        }
    }
}

void MapView::RebuildTiles()
{
    STACK_TRACE_ENTRY();

    _tilesTree.Unvalidate();

    if (!_engine->Settings.ShowTile) {
        return;
    }

    auto y2 = 0;
    for (auto ty = 0; ty < _hVisible; ty++) {
        for (auto x = 0; x < _wVisible; x++) {
            const auto vpos = y2 + x;
            if (vpos < 0 || vpos >= _wVisible * _hVisible) {
                continue;
            }

            const auto hxi = _viewField[vpos].HexX;
            const auto hyi = _viewField[vpos].HexY;
            if (hxi < 0 || hyi < 0 || hxi >= _maxHexX || hyi >= _maxHexY) {
                continue;
            }

            const auto hx = static_cast<uint16>(hxi);
            const auto hy = static_cast<uint16>(hyi);

            auto& field = GetField(hx, hy);
            const auto tiles_count = field.GetTilesCount(false);
            if (tiles_count == 0u) {
                continue;
            }

            for (uint i = 0; i < tiles_count; i++) {
                auto& tile = field.GetTile(i, false);
                const auto spr_id = tile.Anim->GetSprId(0);

                if (_mapperMode) {
                    auto& tiles = GetTiles(hx, hy, false);
                    auto& spr = _tilesTree.AddSprite(EvaluateTileDrawOrder(tile), hx, hy, //
                        tile.OffsX + _engine->Settings.MapTileOffsX, tile.OffsY + _engine->Settings.MapTileOffsY, &field.ScrX, &field.ScrY, //
                        spr_id, nullptr, nullptr, nullptr, tiles[i].IsSelected ? &SelectAlpha : nullptr, &_engine->EffectMngr.Effects.Tile, nullptr);
                    field.AddSpriteToChain(&spr);
                }
                else {
                    auto& spr = _tilesTree.AddSprite(EvaluateTileDrawOrder(tile), hx, hy, //
                        tile.OffsX + _engine->Settings.MapTileOffsX, tile.OffsY + _engine->Settings.MapTileOffsY, &field.ScrX, &field.ScrY, //
                        spr_id, nullptr, nullptr, nullptr, nullptr, &_engine->EffectMngr.Effects.Tile, nullptr);
                    field.AddSpriteToChain(&spr);
                }
            }
        }
        y2 += _wVisible;
    }

    // Sort
    _tilesTree.SortByMapPos();
}

void MapView::RebuildRoof()
{
    STACK_TRACE_ENTRY();

    _roofTree.Unvalidate();

    if (!_engine->Settings.ShowRoof) {
        return;
    }

    auto y2 = 0;
    for (auto ty = 0; ty < _hVisible; ty++) {
        for (auto x = 0; x < _wVisible; x++) {
            const auto vpos = y2 + x;
            const auto hxi = _viewField[vpos].HexX;
            const auto hyi = _viewField[vpos].HexY;
            if (hxi < 0 || hyi < 0 || hxi >= _maxHexX || hyi >= _maxHexY) {
                continue;
            }

            const auto hx = static_cast<uint16>(hxi);
            const auto hy = static_cast<uint16>(hyi);

            auto& field = GetField(hx, hy);
            const auto roofs_count = field.GetTilesCount(true);
            if (roofs_count == 0u) {
                continue;
            }

            if (_roofSkip == 0 || _roofSkip != field.RoofNum) {
                for (uint i = 0; i < roofs_count; i++) {
                    auto& roof = field.GetTile(i, true);
                    const auto spr_id = roof.Anim->GetSprId(0);

                    if (_mapperMode) {
                        auto& roofs = GetTiles(hx, hy, true);
                        auto& spr = _roofTree.AddSprite(EvaluateTileDrawOrder(roof), hx, hy, //
                            roof.OffsX + _engine->Settings.MapRoofOffsX, roof.OffsY + _engine->Settings.MapRoofOffsY, &field.ScrX, &field.ScrY, //
                            spr_id, nullptr, nullptr, nullptr, roofs[i].IsSelected ? &SelectAlpha : &_engine->Settings.RoofAlpha, &_engine->EffectMngr.Effects.Roof, nullptr);
                        spr.SetEggAppearence(EggAppearenceType::Always);
                        field.AddSpriteToChain(&spr);
                    }
                    else {
                        auto& spr = _roofTree.AddSprite(EvaluateTileDrawOrder(roof), hx, hy, //
                            roof.OffsX + _engine->Settings.MapRoofOffsX, roof.OffsY + _engine->Settings.MapRoofOffsY, &field.ScrX, &field.ScrY, //
                            spr_id, nullptr, nullptr, nullptr, &_engine->Settings.RoofAlpha, &_engine->EffectMngr.Effects.Roof, nullptr);
                        spr.SetEggAppearence(EggAppearenceType::Always);
                        field.AddSpriteToChain(&spr);
                    }
                }
            }
        }

        y2 += _wVisible;
    }

    // Sort
    _roofTree.SortByMapPos();
}

void MapView::SetSkipRoof(uint16 hx, uint16 hy)
{
    STACK_TRACE_ENTRY();

    if (_roofSkip != GetField(hx, hy).RoofNum) {
        _roofSkip = GetField(hx, hy).RoofNum;
        RebuildRoof();
    }
}

void MapView::MarkRoofNum(int hxi, int hyi, int16 num)
{
    STACK_TRACE_ENTRY();

    if (hxi < 0 || hyi < 0 || hxi >= _maxHexX || hyi >= _maxHexY) {
        return;
    }

    const auto hx = static_cast<uint16>(hxi);
    const auto hy = static_cast<uint16>(hyi);
    if (GetField(hx, hy).GetTilesCount(true) == 0u) {
        return;
    }
    if (GetField(hx, hy).RoofNum != 0) {
        return;
    }

    for (auto x = 0; x < _engine->Settings.MapRoofSkipSize; x++) {
        for (auto y = 0; y < _engine->Settings.MapRoofSkipSize; y++) {
            if (hxi + x >= 0 && hxi + x < _maxHexX && hyi + y >= 0 && hyi + y < _maxHexY) {
                GetField(static_cast<uint16>(hxi + x), static_cast<uint16>(hyi + y)).RoofNum = num;
            }
        }
    }

    MarkRoofNum(hxi + _engine->Settings.MapRoofSkipSize, hy, num);
    MarkRoofNum(hxi - _engine->Settings.MapRoofSkipSize, hy, num);
    MarkRoofNum(hxi, hyi + _engine->Settings.MapRoofSkipSize, num);
    MarkRoofNum(hxi, hyi - _engine->Settings.MapRoofSkipSize, num);
}

auto MapView::IsVisible(uint spr_id, int ox, int oy) const -> bool
{
    STACK_TRACE_ENTRY();

    if (spr_id == 0u) {
        return false;
    }

    const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
    if (si == nullptr) {
        return false;
    }

    const auto top = oy + si->OffsY - si->Height - _engine->Settings.MapHexLineHeight * 2;
    const auto bottom = oy + si->OffsY + _engine->Settings.MapHexLineHeight * 2;
    const auto left = ox + si->OffsX - si->Width / 2 - _engine->Settings.MapHexWidth;
    const auto right = ox + si->OffsX + si->Width / 2 + _engine->Settings.MapHexWidth;
    const auto zoomed_screen_height = iround(std::ceil(static_cast<float>(_engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight) * GetSpritesZoom()));
    const auto zoomed_screen_width = iround(std::ceil(static_cast<float>(_engine->Settings.ScreenWidth) * GetSpritesZoom()));
    return !(top > zoomed_screen_height || bottom < 0 || left > zoomed_screen_width || right < 0);
}

auto MapView::MeasureMapBorders(uint spr_id, int ox, int oy) -> bool
{
    STACK_TRACE_ENTRY();

    const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
    RUNTIME_ASSERT(si);

    const auto top = std::max(si->OffsY + oy - _hTop * _engine->Settings.MapHexLineHeight + _engine->Settings.MapHexLineHeight * 2, 0);
    const auto bottom = std::max(si->Height - si->OffsY - oy - _hBottom * _engine->Settings.MapHexLineHeight + _engine->Settings.MapHexLineHeight * 2, 0);
    const auto left = std::max(si->Width / 2 + si->OffsX + ox - _wLeft * _engine->Settings.MapHexWidth + _engine->Settings.MapHexWidth, 0);
    const auto right = std::max(si->Width / 2 - si->OffsX - ox - _wRight * _engine->Settings.MapHexWidth + _engine->Settings.MapHexWidth, 0);

    if (top > 0 || bottom > 0 || left > 0 || right > 0) {
        _hTop += top / _engine->Settings.MapHexLineHeight + ((top % _engine->Settings.MapHexLineHeight) != 0 ? 1 : 0);
        _hBottom += bottom / _engine->Settings.MapHexLineHeight + ((bottom % _engine->Settings.MapHexLineHeight) != 0 ? 1 : 0);
        _wLeft += left / _engine->Settings.MapHexWidth + ((left % _engine->Settings.MapHexWidth) != 0 ? 1 : 0);
        _wRight += right / _engine->Settings.MapHexWidth + ((right % _engine->Settings.MapHexWidth) != 0 ? 1 : 0);

        ResizeView();
        RefreshMap();

        return true;
    }

    return false;
}

auto MapView::MeasureMapBorders(const ItemHexView* item) -> bool
{
    STACK_TRACE_ENTRY();

    return MeasureMapBorders(item->SprId, item->GetOffsetX(), item->GetOffsetY());
}

auto MapView::MeasureMapBorders(const Field::Tile& tile, bool is_roof) -> bool
{
    STACK_TRACE_ENTRY();

    const auto ox = (is_roof ? _engine->Settings.MapRoofOffsX : _engine->Settings.MapTileOffsX) + tile.OffsX;
    const auto oy = (is_roof ? _engine->Settings.MapRoofOffsY : _engine->Settings.MapTileOffsY) + tile.OffsY;

    return MeasureMapBorders(tile.Anim->GetSprId(0), ox, oy);
}

void MapView::SwitchShowHex()
{
    STACK_TRACE_ENTRY();

    _isShowHex = !_isShowHex;

    RefreshMap();
}

void MapView::ClearHexTrack()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    std::memset(_hexTrack.data(), 0, _hexTrack.size() * sizeof(char));
}

void MapView::SwitchShowTrack()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    _isShowTrack = !_isShowTrack;

    if (!_isShowTrack) {
        ClearHexTrack();
    }

    RefreshMap();
}

void MapView::InitView(int cx, int cy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        // Get center offset
        const auto hw = GetViewWidth() / 2 + _wRight;
        const auto hv = GetViewHeight() / 2 + _hTop;
        auto vw = hv / 2 + (hv & 1) + 1;
        auto vh = hv - vw / 2 - 1;
        for (auto i = 0; i < hw; i++) {
            if ((vw & 1) != 0) {
                vh--;
            }
            vw++;
        }

        // Subtract offset
        cx -= abs(vw);
        cy -= abs(vh);

        const auto xa = -(_wRight * _engine->Settings.MapHexWidth);
        const auto xb = -(_engine->Settings.MapHexWidth / 2) - (_wRight * _engine->Settings.MapHexWidth);
        auto oy = -_engine->Settings.MapHexLineHeight * _hTop;
        const auto wx = iround(static_cast<float>(_engine->Settings.ScreenWidth) * GetSpritesZoom());

        for (auto yv = 0; yv < _hVisible; yv++) {
            auto hx = cx + yv / 2 + (yv & 1);
            auto hy = cy + (yv - (hx - cx - (cx & 1)) / 2);
            auto ox = ((yv & 1) != 0 ? xa : xb);

            if (yv == 0 && ((cx & 1) != 0)) {
                hy++;
            }

            for (auto xv = 0; xv < _wVisible; xv++) {
                auto& vf = _viewField[yv * _wVisible + xv];
                vf.ScrX = wx - ox;
                vf.ScrY = oy;
                vf.ScrXf = static_cast<float>(vf.ScrX);
                vf.ScrYf = static_cast<float>(vf.ScrY);
                vf.HexX = hx;
                vf.HexY = hy;

                if ((hx & 1) != 0) {
                    hy--;
                }
                hx++;

                ox += _engine->Settings.MapHexWidth;
            }

            oy += _engine->Settings.MapHexLineHeight;
        }
    }
    else {
        // Calculate data
        const auto halfw = GetViewWidth() / 2 + _wRight;
        const auto halfh = GetViewHeight() / 2 + _hTop;
        auto basehx = cx - halfh / 2 - halfw;
        auto basehy = cy - halfh / 2 + halfw;
        auto y2 = 0;
        auto xa = -_engine->Settings.MapHexWidth * _wRight;
        auto xb = -_engine->Settings.MapHexWidth * _wRight - _engine->Settings.MapHexWidth / 2;
        auto y = -_engine->Settings.MapHexLineHeight * _hTop;
        const auto wx = iround(static_cast<float>(_engine->Settings.ScreenWidth) * GetSpritesZoom());

        // Initialize field
        for (auto j = 0; j < _hVisible; j++) {
            auto x = ((j & 1) != 0 ? xa : xb);
            auto hx = basehx;
            auto hy = basehy;

            for (auto i = 0; i < _wVisible; i++) {
                const auto vpos = y2 + i;
                _viewField[vpos].ScrX = wx - x;
                _viewField[vpos].ScrY = y;
                _viewField[vpos].ScrXf = static_cast<float>(_viewField[vpos].ScrX);
                _viewField[vpos].ScrYf = static_cast<float>(_viewField[vpos].ScrY);
                _viewField[vpos].HexX = hx;
                _viewField[vpos].HexY = hy;

                hx++;
                hy--;
                x += _engine->Settings.MapHexWidth;
            }

            if ((j & 1) != 0) {
                basehy++;
            }
            else {
                basehx++;
            }

            y += _engine->Settings.MapHexLineHeight;
            y2 += _wVisible;
        }
    }
}

void MapView::ResizeView()
{
    STACK_TRACE_ENTRY();

    if (!_viewField.empty()) {
        for (auto i = 0, j = _hVisible * _wVisible; i < j; i++) {
            const auto& vf = _viewField[i];
            if (vf.HexX >= 0 && vf.HexY >= 0 && vf.HexX < _maxHexX && vf.HexY < _maxHexY) {
                auto& field = GetField(static_cast<uint16>(vf.HexX), static_cast<uint16>(vf.HexY));
                field.IsView = false;
                field.UnvalidateSpriteChain();
            }
        }
    }

    _hVisible = GetViewHeight() + _hTop + _hBottom;
    _wVisible = GetViewWidth() + _wLeft + _wRight;

    _viewField.resize(_hVisible * _wVisible);
}

void MapView::ChangeZoom(int zoom)
{
    STACK_TRACE_ENTRY();

    if (Math::FloatCompare(_engine->Settings.SpritesZoomMin, _engine->Settings.SpritesZoomMax)) {
        return;
    }
    if (zoom == 0 && GetSpritesZoom() == 1.0f) {
        return;
    }
    if (zoom > 0 && GetSpritesZoom() >= std::min(_engine->Settings.SpritesZoomMax, MAX_ZOOM)) {
        return;
    }
    if (zoom < 0 && GetSpritesZoom() <= std::max(_engine->Settings.SpritesZoomMin, MIN_ZOOM)) {
        return;
    }

    // Check screen blockers
    if (_engine->Settings.ScrollCheck && (zoom > 0 || (zoom == 0 && GetSpritesZoom() < 1.0f))) {
        for (auto x = -1; x <= 1; x++) {
            for (auto y = -1; y <= 1; y++) {
                if (((x != 0) || (y != 0)) && ScrollCheck(x, y)) {
                    return;
                }
            }
        }
    }

    if (zoom != 0 || GetSpritesZoom() < 1.0f) {
        const auto old_zoom = GetSpritesZoom();
        const auto w = static_cast<float>(_engine->Settings.ScreenWidth / _engine->Settings.MapHexWidth + ((_engine->Settings.ScreenWidth % _engine->Settings.MapHexWidth) != 0 ? 1 : 0));
        SetSpritesZoom((w * GetSpritesZoom() + (zoom >= 0 ? 2.0f : -2.0f)) / w);

        if (GetSpritesZoom() < std::max(_engine->Settings.SpritesZoomMin, MIN_ZOOM) || GetSpritesZoom() > std::min(_engine->Settings.SpritesZoomMax, MAX_ZOOM)) {
            SetSpritesZoom(old_zoom);
            return;
        }
    }
    else {
        SetSpritesZoom(1.0f);
    }

    ResizeView();
    RefreshMap();

    if (zoom == 0 && GetSpritesZoom() != 1.0f) {
        ChangeZoom(0);
    }
}

void MapView::GetScreenHexes(int& sx, int& sy) const
{
    STACK_TRACE_ENTRY();

    sx = _screenHexX;
    sy = _screenHexY;
}

void MapView::GetHexCurrentPosition(uint16 hx, uint16 hy, int& x, int& y) const
{
    STACK_TRACE_ENTRY();

    auto& center_hex = _viewField[_hVisible / 2 * _wVisible + _wVisible / 2];
    const auto center_hx = center_hex.HexX;
    const auto center_hy = center_hex.HexY;
    auto [xx, yy] = _engine->Geometry.GetHexInterval(center_hx, center_hy, hx, hy);

    x = center_hex.ScrX;
    y = center_hex.ScrY;
    x += xx;
    y += yy;
}

void MapView::DrawMap()
{
    STACK_TRACE_ENTRY();

    _engine->SprMngr.SetSpritesZoom(GetSpritesZoom());
    auto reset_spr_mngr_zoom = ScopeCallback([this]() noexcept { _engine->SprMngr.SetSpritesZoom(1.0f); });

    // Prepare light
    PrepareLightToDraw();

    // Prepare fog
    PrepareFogToDraw();

    // Prerendered offsets
    const auto ox = _rtScreenOx - iround(static_cast<float>(_engine->Settings.ScrOx) / GetSpritesZoom());
    const auto oy = _rtScreenOy - iround(static_cast<float>(_engine->Settings.ScrOy) / GetSpritesZoom());
    const auto prerendered_rect = IRect(ox, oy, ox + _engine->Settings.ScreenWidth, oy + (_engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight));

    // Separate render target
    if (_rtMap != nullptr) {
        _engine->SprMngr.PushRenderTarget(_rtMap);
        _engine->SprMngr.ClearCurrentRenderTarget(0);
    }

    // Tiles
    if (_engine->Settings.ShowTile) {
        _engine->SprMngr.DrawSprites(_tilesTree, false, false, DrawOrderType::Tile, DrawOrderType::TileEnd, _mapDayColor);
    }

    // Flat sprites
    _engine->SprMngr.DrawSprites(_mainTree, true, false, DrawOrderType::HexGrid, DrawOrderType::FlatScenery, _mapDayColor);

    // Light
    if (_rtLight != nullptr) {
        _engine->SprMngr.DrawRenderTarget(_rtLight, true, &prerendered_rect);
    }

    // Cursor flat
    if (_cursorPrePic != nullptr) {
        DrawCursor(_cursorPrePic->GetCurSprId(_engine->GameTime.GameplayTime()));
    }

    // Sprites
    _engine->SprMngr.DrawSprites(_mainTree, true, true, DrawOrderType::Ligth, DrawOrderType::Last, _mapDayColor);

    // Roof
    if (_engine->Settings.ShowRoof) {
        _engine->SprMngr.DrawSprites(_roofTree, false, true, DrawOrderType::Tile, DrawOrderType::TileEnd, _mapDayColor);
    }

    // Contours
    _engine->SprMngr.DrawContours();

    // Fog
    if (!_mapperMode && _rtFog != nullptr) {
        _engine->SprMngr.DrawRenderTarget(_rtFog, true, &prerendered_rect);
    }

    // Cursor
    if (_cursorPostPic != nullptr) {
        DrawCursor(_cursorPostPic->GetCurSprId(_engine->GameTime.GameplayTime()));
    }

    if (_cursorXPic != nullptr) {
        if (_drawCursorX < 0) {
            DrawCursor(_cursorXPic->GetCurSprId(_engine->GameTime.GameplayTime()));
        }
        else if (_drawCursorX > 0) {
            DrawCursor(_str("{}", _drawCursorX));
        }
    }

    // Texts
    DrawMapTexts();

    // Draw map from render target
    if (_rtMap != nullptr) {
        _engine->SprMngr.PopRenderTarget();
        _engine->SprMngr.DrawRenderTarget(_rtMap, false);
    }
}

void MapView::DrawMapTexts()
{
    STACK_TRACE_ENTRY();

    for (auto* cr : _critters) {
        cr->DrawTextOnHead();
    }

    const auto time = _engine->GameTime.GameplayTime();

    for (auto it = _mapTexts.begin(); it != _mapTexts.end();) {
        const auto& map_text = *it;

        if (time < map_text.StartTime + map_text.Duration) {
            const auto& field = GetField(map_text.HexX, map_text.HexY);

            if (field.IsView) {
                const auto dt = time_duration_to_ms<uint>(time - map_text.StartTime);
                const auto percent = GenericUtils::Percent(time_duration_to_ms<uint>(map_text.Duration), dt);
                const auto text_pos = map_text.Pos.Interpolate(map_text.EndPos, percent);
                const auto half_hex_width = _engine->Settings.MapHexWidth / 2;
                const auto half_hex_height = _engine->Settings.MapHexHeight / 2;
                const auto x = iround(static_cast<float>(field.ScrX + half_hex_width + _engine->Settings.ScrOx) / GetSpritesZoom() - 100.0f - static_cast<float>(map_text.Pos.Left - text_pos.Left));
                const auto y = iround(static_cast<float>(field.ScrY + half_hex_height - map_text.Pos.Height() - (map_text.Pos.Top - text_pos.Top) + _engine->Settings.ScrOy) / GetSpritesZoom() - 70.0f);

                auto color = map_text.Color;
                if (map_text.Fade) {
                    color = (color ^ 0xFF000000) | ((0xFF * (100 - percent) / 100) << 24);
                }
                else if (map_text.Duration > std::chrono::milliseconds {500}) {
                    const auto hide = time_duration_to_ms<uint>(map_text.Duration - std::chrono::milliseconds {200});
                    if (dt >= hide) {
                        const auto alpha = 255u * (100u - GenericUtils::Percent(time_duration_to_ms<uint>(map_text.Duration) - hide, dt - hide)) / 100;
                        color = (alpha << 24) | (color & 0xFFFFFF);
                    }
                }

                _engine->SprMngr.DrawStr(IRect(x, y, x + 200, y + 70), map_text.Text, FT_CENTERX | FT_BOTTOM | FT_BORDERED, color, -1);
            }

            ++it;
        }
        else {
            it = _mapTexts.erase(it);
        }
    }
}

void MapView::PrepareFogToDraw()
{
    STACK_TRACE_ENTRY();

    if (_mapperMode || _rtFog == nullptr) {
        return;
    }

    if (_rebuildFog) {
        _rebuildFog = false;

        _fogLookPoints.clear();
        _fogShootPoints.clear();

        CritterHexView* chosen;

        const auto it = std::find_if(_critters.begin(), _critters.end(), [](auto* cr) { return cr->IsChosen(); });
        if (it != _critters.end()) {
            chosen = *it;
        }
        else {
            chosen = nullptr;
        }

        if (chosen != nullptr && (_drawLookBorders || _drawShootBorders)) {
            const auto dist = chosen->GetLookDistance() + _engine->Settings.FogExtraLength;
            const auto base_hx = chosen->GetHexX();
            const auto base_hy = chosen->GetHexY();
            int hx = base_hx;
            int hy = base_hy;
            const int chosen_dir = chosen->GetDir();
            const auto dist_shoot = chosen->GetAttackDist();
            const auto maxhx = GetWidth();
            const auto maxhy = GetHeight();

            const auto half_hw = _engine->Settings.MapHexWidth / 2;
            const auto half_hh = _engine->Settings.MapHexHeight / 2;

            auto base_x = 0;
            auto base_y = 0;
            GetHexCurrentPosition(base_hx, base_hy, base_x, base_y);
            const auto center_look_point = PrimitivePoint {base_x + half_hw, base_y + half_hh, COLOR_RGBA(0, 0, 0, 0), &chosen->SprOx, &chosen->SprOy};
            const auto center_shoot_point = PrimitivePoint {base_x + half_hw, base_y + half_hh, COLOR_RGBA(255, 0, 0, 0), &chosen->SprOx, &chosen->SprOy};

            size_t look_points_added = 0;
            size_t shoot_points_added = 0;

            auto seek_start = true;
            for (auto i = 0; i < (GameSettings::HEXAGONAL_GEOMETRY ? 6 : 4); i++) {
                const auto dir = (GameSettings::HEXAGONAL_GEOMETRY ? (i + 2) % 6 : ((i + 1) * 2) % 8);

                for (uint j = 0, jj = (GameSettings::HEXAGONAL_GEOMETRY ? dist : dist * 2); j < jj; j++) {
                    if (seek_start) {
                        // Move to start position
                        for (uint l = 0; l < dist; l++) {
                            _engine->Geometry.MoveHexByDirUnsafe(hx, hy, GameSettings::HEXAGONAL_GEOMETRY ? 0 : 7);
                        }
                        seek_start = false;
                        j = -1;
                    }
                    else {
                        // Move to next hex
                        _engine->Geometry.MoveHexByDirUnsafe(hx, hy, static_cast<uint8>(dir));
                    }

                    auto hx_ = static_cast<uint16>(std::clamp(hx, 0, maxhx - 1));
                    auto hy_ = static_cast<uint16>(std::clamp(hy, 0, maxhy - 1));
                    if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
                        const int dir_ = _engine->Geometry.GetFarDir(base_hx, base_hy, hx_, hy_);
                        auto ii = (chosen_dir > dir_ ? chosen_dir - dir_ : dir_ - chosen_dir);
                        if (ii > static_cast<int>(GameSettings::MAP_DIR_COUNT / 2)) {
                            ii = GameSettings::MAP_DIR_COUNT - ii;
                        }
                        const auto dist_ = dist - dist * _engine->Settings.LookDir[ii] / 100;
                        pair<uint16, uint16> block = {};
                        TraceBullet(base_hx, base_hy, hx_, hy_, dist_, 0.0f, nullptr, CritterFindType::Any, nullptr, &block, nullptr, false);
                        hx_ = block.first;
                        hy_ = block.second;
                    }

                    if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_TRACE_CLIENT)) {
                        pair<uint16, uint16> block = {};
                        TraceBullet(base_hx, base_hy, hx_, hy_, 0, 0.0f, nullptr, CritterFindType::Any, nullptr, &block, nullptr, true);
                        hx_ = block.first;
                        hy_ = block.second;
                    }

                    auto dist_look = _engine->Geometry.DistGame(base_hx, base_hy, hx_, hy_);
                    if (_drawLookBorders) {
                        auto x = 0;
                        auto y = 0;
                        GetHexCurrentPosition(hx_, hy_, x, y);
                        auto* ox = (dist_look == dist ? &chosen->SprOx : nullptr);
                        auto* oy = (dist_look == dist ? &chosen->SprOy : nullptr);
                        _fogLookPoints.emplace_back(PrimitivePoint {x + half_hw, y + half_hh, COLOR_RGBA(0, 255, dist_look * 255 / dist, 0), ox, oy});
                        if (++look_points_added % 2 == 0) {
                            _fogLookPoints.emplace_back(center_look_point);
                        }
                    }

                    if (_drawShootBorders) {
                        pair<uint16, uint16> block = {};
                        const auto max_shoot_dist = std::max(std::min(dist_look, dist_shoot), 0u) + 1u;
                        TraceBullet(base_hx, base_hy, hx_, hy_, max_shoot_dist, 0.0f, nullptr, CritterFindType::Any, nullptr, &block, nullptr, true);
                        const auto hx_2 = block.first;
                        const auto hy_2 = block.second;

                        auto x_ = 0;
                        auto y_ = 0;
                        GetHexCurrentPosition(hx_2, hy_2, x_, y_);
                        const auto result_shoot_dist = _engine->Geometry.DistGame(base_hx, base_hy, hx_2, hy_2);
                        auto* ox = (result_shoot_dist == max_shoot_dist ? &chosen->SprOx : nullptr);
                        auto* oy = (result_shoot_dist == max_shoot_dist ? &chosen->SprOy : nullptr);
                        _fogShootPoints.emplace_back(PrimitivePoint {x_ + half_hw, y_ + half_hh, COLOR_RGBA(255, 255, result_shoot_dist * 255 / max_shoot_dist, 0), ox, oy});
                        if (++shoot_points_added % 2 == 0) {
                            _fogShootPoints.emplace_back(center_shoot_point);
                        }
                    }
                }
            }

            _fogOffsX = chosen != nullptr ? &chosen->SprOx : nullptr;
            _fogOffsY = chosen != nullptr ? &chosen->SprOy : nullptr;
            _fogLastOffsX = _fogOffsX != nullptr ? *_fogOffsX : 0;
            _fogLastOffsY = _fogOffsY != nullptr ? *_fogOffsY : 0;
            _fogForceRerender = true;
        }
    }

    if (_fogForceRerender || _fogOffsX == nullptr || *_fogOffsX != _fogLastOffsX || *_fogOffsY != _fogLastOffsY) {
        _fogForceRerender = false;

        if (_fogOffsX != nullptr) {
            _fogLastOffsX = *_fogOffsX;
            _fogLastOffsY = *_fogOffsY;
        }

        const auto offset = FPoint(static_cast<float>(_rtScreenOx), static_cast<float>(_rtScreenOy));
        _engine->SprMngr.PushRenderTarget(_rtFog);
        _engine->SprMngr.ClearCurrentRenderTarget(0);
        const float zoom = GetSpritesZoom();
        _engine->SprMngr.DrawPoints(_fogLookPoints, RenderPrimitiveType::TriangleStrip, &zoom, &offset, _engine->EffectMngr.Effects.Fog);
        _engine->SprMngr.DrawPoints(_fogShootPoints, RenderPrimitiveType::TriangleStrip, &zoom, &offset, _engine->EffectMngr.Effects.Fog);
        _engine->SprMngr.PopRenderTarget();
    }
}

auto MapView::IsScrollEnabled() const -> bool
{
    STACK_TRACE_ENTRY();

    return _engine->Settings.ScrollMouseLeft || _engine->Settings.ScrollKeybLeft || _engine->Settings.ScrollMouseRight || //
        _engine->Settings.ScrollKeybRight || _engine->Settings.ScrollMouseUp || _engine->Settings.ScrollKeybUp || //
        _engine->Settings.ScrollMouseDown || _engine->Settings.ScrollKeybDown;
}

auto MapView::Scroll() -> bool
{
    STACK_TRACE_ENTRY();

    // Scroll delay
    auto time_k = 1.0f;
    if (_engine->Settings.ScrollDelay != 0u) {
        const auto time = _engine->GameTime.FrameTime();
        if (time - _scrollLastTime < std::chrono::milliseconds {_engine->Settings.ScrollDelay / 2}) {
            return false;
        }

        time_k = time_duration_to_ms<float>(time - _scrollLastTime) / static_cast<float>(_engine->Settings.ScrollDelay);
        _scrollLastTime = time;
    }

    const auto is_scroll = IsScrollEnabled();
    auto scr_ox = _engine->Settings.ScrOx;
    auto scr_oy = _engine->Settings.ScrOy;
    const auto prev_scr_ox = scr_ox;
    const auto prev_scr_oy = scr_oy;

    if (is_scroll && AutoScroll.CanStop) {
        AutoScroll.Active = false;
    }

    // Check critter scroll lock
    if (AutoScroll.HardLockedCritter && !is_scroll) {
        const auto* cr = GetCritter(AutoScroll.HardLockedCritter);
        if (cr != nullptr && (cr->GetHexX() != _screenHexX || cr->GetHexY() != _screenHexY)) {
            ScrollToHex(cr->GetHexX(), cr->GetHexY(), 0.02f, true);
        }
    }

    if (AutoScroll.SoftLockedCritter && !is_scroll) {
        const auto* cr = GetCritter(AutoScroll.SoftLockedCritter);
        if (cr != nullptr && (cr->GetHexX() != AutoScroll.CritterLastHexX || cr->GetHexY() != AutoScroll.CritterLastHexY)) {
            const auto [ox, oy] = _engine->Geometry.GetHexInterval(AutoScroll.CritterLastHexX, AutoScroll.CritterLastHexY, cr->GetHexX(), cr->GetHexY());
            ScrollOffset(ox, oy, 0.02f, true);
            AutoScroll.CritterLastHexX = cr->GetHexX();
            AutoScroll.CritterLastHexY = cr->GetHexY();
        }
    }

    auto xscroll = 0;
    auto yscroll = 0;
    if (AutoScroll.Active) {
        AutoScroll.OffsXStep += AutoScroll.OffsX * AutoScroll.Speed * time_k;
        AutoScroll.OffsYStep += AutoScroll.OffsY * AutoScroll.Speed * time_k;

        xscroll = iround(AutoScroll.OffsXStep);
        yscroll = iround(AutoScroll.OffsYStep);

        if (xscroll > _engine->Settings.MapHexWidth) {
            xscroll = _engine->Settings.MapHexWidth;
            AutoScroll.OffsXStep = static_cast<float>(_engine->Settings.MapHexWidth);
        }
        if (xscroll < -_engine->Settings.MapHexWidth) {
            xscroll = -_engine->Settings.MapHexWidth;
            AutoScroll.OffsXStep = -static_cast<float>(_engine->Settings.MapHexWidth);
        }
        if (yscroll > _engine->Settings.MapHexLineHeight * 2) {
            yscroll = _engine->Settings.MapHexLineHeight * 2;
            AutoScroll.OffsYStep = static_cast<float>(_engine->Settings.MapHexLineHeight * 2);
        }
        if (yscroll < -_engine->Settings.MapHexLineHeight * 2) {
            yscroll = -_engine->Settings.MapHexLineHeight * 2;
            AutoScroll.OffsYStep = -static_cast<float>(_engine->Settings.MapHexLineHeight * 2);
        }

        AutoScroll.OffsX -= static_cast<float>(xscroll);
        AutoScroll.OffsY -= static_cast<float>(yscroll);
        AutoScroll.OffsXStep -= static_cast<float>(xscroll);
        AutoScroll.OffsYStep -= static_cast<float>(yscroll);

        if (xscroll == 0 && yscroll == 0) {
            return false;
        }

        if (GenericUtils::DistSqrt(0, 0, iround(AutoScroll.OffsX), iround(AutoScroll.OffsY)) == 0u) {
            AutoScroll.Active = false;
        }
    }
    else {
        if (!is_scroll) {
            return false;
        }

        if (_engine->Settings.ScrollMouseLeft || _engine->Settings.ScrollKeybLeft) {
            xscroll += 1;
        }
        if (_engine->Settings.ScrollMouseRight || _engine->Settings.ScrollKeybRight) {
            xscroll -= 1;
        }
        if (_engine->Settings.ScrollMouseUp || _engine->Settings.ScrollKeybUp) {
            yscroll += 1;
        }
        if (_engine->Settings.ScrollMouseDown || _engine->Settings.ScrollKeybDown) {
            yscroll -= 1;
        }
        if ((xscroll == 0) && (yscroll == 0)) {
            return false;
        }

        xscroll = iround(static_cast<float>(xscroll * _engine->Settings.ScrollStep) * GetSpritesZoom() * time_k);
        yscroll = iround(static_cast<float>(yscroll * (_engine->Settings.ScrollStep * (_engine->Settings.MapHexLineHeight * 2) / _engine->Settings.MapHexWidth)) * GetSpritesZoom() * time_k);
    }
    scr_ox += xscroll;
    scr_oy += yscroll;

    if (_engine->Settings.ScrollCheck) {
        auto xmod = 0;
        auto ymod = 0;
        if (scr_ox - _engine->Settings.ScrOx > 0) {
            xmod = 1;
        }
        if (scr_ox - _engine->Settings.ScrOx < 0) {
            xmod = -1;
        }
        if (scr_oy - _engine->Settings.ScrOy > 0) {
            ymod = -1;
        }
        if (scr_oy - _engine->Settings.ScrOy < 0) {
            ymod = 1;
        }
        if (((xmod != 0) || (ymod != 0)) && ScrollCheck(xmod, ymod)) {
            if ((xmod != 0) && (ymod != 0) && !ScrollCheck(0, ymod)) {
                scr_ox = 0;
            }
            else if ((xmod != 0) && (ymod != 0) && !ScrollCheck(xmod, 0)) {
                scr_oy = 0;
            }
            else {
                if (xmod != 0) {
                    scr_ox = 0;
                }
                if (ymod != 0) {
                    scr_oy = 0;
                }
            }
        }
    }

    auto xmod = 0;
    auto ymod = 0;
    if (scr_ox >= _engine->Settings.MapHexWidth) {
        xmod = 1;
        scr_ox -= _engine->Settings.MapHexWidth;
        if (scr_ox > _engine->Settings.MapHexWidth) {
            scr_ox = _engine->Settings.MapHexWidth;
        }
    }
    else if (scr_ox <= -_engine->Settings.MapHexWidth) {
        xmod = -1;
        scr_ox += _engine->Settings.MapHexWidth;
        if (scr_ox < -_engine->Settings.MapHexWidth) {
            scr_ox = -_engine->Settings.MapHexWidth;
        }
    }
    if (scr_oy >= (_engine->Settings.MapHexLineHeight * 2)) {
        ymod = -2;
        scr_oy -= (_engine->Settings.MapHexLineHeight * 2);
        if (scr_oy > (_engine->Settings.MapHexLineHeight * 2)) {
            scr_oy = (_engine->Settings.MapHexLineHeight * 2);
        }
    }
    else if (scr_oy <= -(_engine->Settings.MapHexLineHeight * 2)) {
        ymod = 2;
        scr_oy += (_engine->Settings.MapHexLineHeight * 2);
        if (scr_oy < -(_engine->Settings.MapHexLineHeight * 2)) {
            scr_oy = -(_engine->Settings.MapHexLineHeight * 2);
        }
    }

    _engine->Settings.ScrOx = scr_ox;
    _engine->Settings.ScrOy = scr_oy;

    if ((xmod != 0) || (ymod != 0)) {
        RebuildMapOffset(xmod, ymod);

        if (_engine->Settings.ScrollCheck) {
            if (_engine->Settings.ScrOx > 0 && ScrollCheck(1, 0)) {
                _engine->Settings.ScrOx = 0;
            }
            else if (_engine->Settings.ScrOx < 0 && ScrollCheck(-1, 0)) {
                _engine->Settings.ScrOx = 0;
            }
            if (_engine->Settings.ScrOy > 0 && ScrollCheck(0, -1)) {
                _engine->Settings.ScrOy = 0;
            }
            else if (_engine->Settings.ScrOy < 0 && ScrollCheck(0, 1)) {
                _engine->Settings.ScrOy = 0;
            }
        }
    }

    if (!_mapperMode) {
        const auto final_scr_ox = _engine->Settings.ScrOx - prev_scr_ox + xmod * _engine->Settings.MapHexWidth;
        const auto final_scr_oy = _engine->Settings.ScrOy - prev_scr_oy + (-ymod / 2) * (_engine->Settings.MapHexLineHeight * 2);
        if ((final_scr_ox != 0) || (final_scr_oy != 0)) {
            _engine->OnScreenScroll.Fire(final_scr_ox, final_scr_oy);
        }
    }

    return (xmod != 0) || (ymod != 0);
}

auto MapView::ScrollCheckPos(int (&positions)[4], int dir1, int dir2) -> bool
{
    STACK_TRACE_ENTRY();

    const auto max_pos = _wVisible * _hVisible;
    for (auto pos : positions) {
        if (pos < 0 || pos >= max_pos) {
            return true;
        }

        if (_viewField[pos].HexX < 0 || _viewField[pos].HexY < 0 || _viewField[pos].HexX >= _maxHexX || _viewField[pos].HexY >= _maxHexY) {
            return true;
        }

        auto hx = static_cast<uint16>(_viewField[pos].HexX);
        auto hy = static_cast<uint16>(_viewField[pos].HexY);

        _engine->Geometry.MoveHexByDir(hx, hy, dir1, _maxHexX, _maxHexY);
        if (GetField(hx, hy).Flags.ScrollBlock) {
            return true;
        }

        if (dir2 >= 0) {
            _engine->Geometry.MoveHexByDir(hx, hy, dir2, _maxHexX, _maxHexY);
            if (GetField(hx, hy).Flags.ScrollBlock) {
                return true;
            }
        }
    }
    return false;
}

auto MapView::ScrollCheck(int xmod, int ymod) -> bool
{
    STACK_TRACE_ENTRY();

    if (_mapperMode) {
        return false;
    }

    int positions_left[4] = {
        _hTop * _wVisible + _wRight + GetViewWidth(), // Left top
        (_hTop + GetViewHeight() - 1) * _wVisible + _wRight + GetViewWidth(), // Left bottom
        (_hTop + 1) * _wVisible + _wRight + GetViewWidth(), // Left top 2
        (_hTop + GetViewHeight() - 1 - 1) * _wVisible + _wRight + GetViewWidth(), // Left bottom 2
    };
    int positions_right[4] = {
        (_hTop + GetViewHeight() - 1) * _wVisible + _wRight + 1, // Right bottom
        _hTop * _wVisible + _wRight + 1, // Right top
        (_hTop + GetViewHeight() - 1 - 1) * _wVisible + _wRight + 1, // Right bottom 2
        (_hTop + 1) * _wVisible + _wRight + 1, // Right top 2
    };

    int dirs[8] = {0, 5, 2, 3, 4, -1, 1, -1}; // Hexagonal

    if constexpr (GameSettings::SQUARE_GEOMETRY) {
        dirs[0] = 7; // Square
        dirs[1] = -1;
        dirs[2] = 3;
        dirs[3] = -1;
        dirs[4] = 5;
        dirs[5] = -1;
        dirs[6] = 1;
        dirs[7] = -1;
    }

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        if (ymod < 0 && (ScrollCheckPos(positions_left, 0, 5) || ScrollCheckPos(positions_right, 5, 0))) {
            return true; // Up
        }
        if (ymod > 0 && (ScrollCheckPos(positions_left, 2, 3) || ScrollCheckPos(positions_right, 3, 2))) {
            return true; // Down
        }
        if (xmod > 0 && (ScrollCheckPos(positions_left, 4, -1) || ScrollCheckPos(positions_right, 4, -1))) {
            return true; // Left
        }
        if (xmod < 0 && (ScrollCheckPos(positions_right, 1, -1) || ScrollCheckPos(positions_left, 1, -1))) {
            return true; // Right
        }
    }
    else {
        if (ymod < 0 && (ScrollCheckPos(positions_left, 0, 6) || ScrollCheckPos(positions_right, 6, 0))) {
            return true; // Up
        }
        if (ymod > 0 && (ScrollCheckPos(positions_left, 2, 4) || ScrollCheckPos(positions_right, 4, 2))) {
            return true; // Down
        }
        if (xmod > 0 && (ScrollCheckPos(positions_left, 6, 4) || ScrollCheckPos(positions_right, 4, 6))) {
            return true; // Left
        }
        if (xmod < 0 && (ScrollCheckPos(positions_right, 0, 2) || ScrollCheckPos(positions_left, 2, 0))) {
            return true; // Right
        }
    }

    // Add precise for zooming infelicity
    if (GetSpritesZoom() != 1.0f) {
        for (auto& i : positions_left) {
            i--;
        }
        for (auto& i : positions_right) {
            i++;
        }

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            if (ymod < 0 && (ScrollCheckPos(positions_left, 0, 5) || ScrollCheckPos(positions_right, 5, 0))) {
                return true; // Up
            }
            if (ymod > 0 && (ScrollCheckPos(positions_left, 2, 3) || ScrollCheckPos(positions_right, 3, 2))) {
                return true; // Down
            }
            if (xmod > 0 && (ScrollCheckPos(positions_left, 4, -1) || ScrollCheckPos(positions_right, 4, -1))) {
                return true; // Left
            }
            if (xmod < 0 && (ScrollCheckPos(positions_right, 1, -1) || ScrollCheckPos(positions_left, 1, -1))) {
                return true; // Right
            }
        }
        else {
            if (ymod < 0 && (ScrollCheckPos(positions_left, 0, 6) || ScrollCheckPos(positions_right, 6, 0))) {
                return true; // Up
            }
            if (ymod > 0 && (ScrollCheckPos(positions_left, 2, 4) || ScrollCheckPos(positions_right, 4, 2))) {
                return true; // Down
            }
            if (xmod > 0 && (ScrollCheckPos(positions_left, 6, 4) || ScrollCheckPos(positions_right, 4, 6))) {
                return true; // Left
            }
            if (xmod < 0 && (ScrollCheckPos(positions_right, 0, 2) || ScrollCheckPos(positions_left, 2, 0))) {
                return true; // Right
            }
        }
    }
    return false;
}

void MapView::ScrollToHex(int hx, int hy, float speed, bool can_stop)
{
    STACK_TRACE_ENTRY();

    auto sx = 0;
    auto sy = 0;
    GetScreenHexes(sx, sy);

    const auto [ox, oy] = _engine->Geometry.GetHexInterval(sx, sy, hx, hy);

    AutoScroll.Active = false;
    ScrollOffset(ox, oy, speed, can_stop);
}

void MapView::ScrollOffset(int ox, int oy, float speed, bool can_stop)
{
    STACK_TRACE_ENTRY();

    if (!AutoScroll.Active) {
        AutoScroll.Active = true;
        AutoScroll.OffsX = 0;
        AutoScroll.OffsY = 0;
        AutoScroll.OffsXStep = 0.0;
        AutoScroll.OffsYStep = 0.0;
    }

    AutoScroll.CanStop = can_stop;
    AutoScroll.Speed = speed;
    AutoScroll.OffsX += -static_cast<float>(ox);
    AutoScroll.OffsY += -static_cast<float>(oy);
}

void MapView::AddCritterToField(CritterHexView* cr)
{
    STACK_TRACE_ENTRY();

    const auto hx = cr->GetHexX();
    const auto hy = cr->GetHexY();
    auto& field = GetField(hx, hy);

    field.AddCritter(cr);

    if (!cr->IsDead()) {
        SetMultihex(cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), true);
    }

    if (IsHexToDraw(hx, hy) && cr->Visible) {
        auto& spr = _mainTree.InsertSprite(EvaluateCritterDrawOrder(cr), hx, hy, //
            _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2, //
            &field.ScrX, &field.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy, //
            &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid);
        spr.SetLight(CornerType::EastWest, _hexLight.data(), _maxHexX, _maxHexY);
        cr->SprDraw = &spr;

        cr->RefreshOffs();
        cr->ResetOk();

        auto contour = ContourType::None;
        if (cr->GetId() == _critterContourCrId) {
            contour = _critterContour;
        }
        else if (!cr->IsDead() && !cr->IsChosen()) {
            contour = _crittersContour;
        }
        spr.SetContour(contour, cr->GetContourColor());

        field.AddSpriteToChain(&spr);
    }

    field.ProcessCache();
}

void MapView::RemoveCritterFromField(CritterHexView* cr)
{
    STACK_TRACE_ENTRY();

    const auto hx = cr->GetHexX();
    const auto hy = cr->GetHexY();

    auto& field = GetField(hx, hy);

    field.EraseCritter(cr);

    if (!cr->IsDead()) {
        SetMultihex(cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), false);
    }

    if (cr->IsChosen() || cr->IsHaveLightSources()) {
        RebuildLight();
    }
    if (cr->SprDrawValid) {
        cr->SprDraw->Unvalidate();
    }

    field.ProcessCache();
}

auto MapView::GetCritter(ident_t id) -> CritterHexView*
{
    STACK_TRACE_ENTRY();

    if (!id) {
        return nullptr;
    }

    const auto it = _crittersMap.find(id);
    return it != _crittersMap.end() ? it->second : nullptr;
}

auto MapView::AddCritter(ident_t id, const ProtoCritter* proto, const map<string, string>& props_kv) -> CritterHexView*
{
    STACK_TRACE_ENTRY();

    if (id && _crittersMap.count(id) != 0u) {
        return nullptr;
    }

    auto* cr = new CritterHexView(this, id, proto);
    if (!cr->LoadFromText(props_kv)) {
        cr->Release();
        return nullptr;
    }

    AddCritterInternal(cr);
    return cr;
}

auto MapView::AddCritter(ident_t id, const ProtoCritter* proto, uint16 hx, uint16 hy, int16 dir_angle, const vector<vector<uint8>>& data) -> CritterHexView*
{
    STACK_TRACE_ENTRY();

    auto* cr = new CritterHexView(this, id, proto);
    cr->RestoreData(data);
    cr->SetHexX(hx);
    cr->SetHexY(hy);
    cr->ChangeDirAngle(dir_angle);

    AddCritterInternal(cr);
    return cr;
}

void MapView::AddCritterInternal(CritterHexView* cr)
{
    STACK_TRACE_ENTRY();

    time_point fading_time;

    if (cr->GetId()) {
        if (auto* prev_cr = GetCritter(cr->GetId()); prev_cr != nullptr) {
            fading_time = prev_cr->FadingTime;
            DestroyCritter(prev_cr);
        }

        _crittersMap.emplace(cr->GetId(), cr);
    }

    _critters.emplace_back(cr);

    AddCritterToField(cr);

    cr->Init();

    const auto time = _engine->GameTime.GameplayTime();
    cr->FadingTime = time + std::chrono::milliseconds {_engine->Settings.FadingDuration} - (fading_time > time ? fading_time - time : time_duration {});
}

void MapView::DestroyCritter(CritterHexView* cr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(cr->GetMap() == this);

    const auto it = std::find(_critters.begin(), _critters.end(), cr);
    RUNTIME_ASSERT(it != _critters.end());
    _critters.erase(it);

    if (cr->GetId()) {
        const auto it_map = _crittersMap.find(cr->GetId());
        RUNTIME_ASSERT(it_map != _crittersMap.end());
        _crittersMap.erase(it_map);
    }

    RemoveCritterFromField(cr);
    cr->DeleteAllItems();
    cr->MarkAsDestroyed();
    cr->Release();
}

auto MapView::GetCritters() -> const vector<CritterHexView*>&
{
    STACK_TRACE_ENTRY();

    return _critters;
}

auto MapView::GetCritters(uint16 hx, uint16 hy, CritterFindType find_type) -> vector<CritterHexView*>
{
    STACK_TRACE_ENTRY();

    vector<CritterHexView*> crits;
    auto& field = GetField(hx, hy);

    if (field.Critters != nullptr) {
        for (auto* cr : *field.Critters) {
            if (cr->CheckFind(find_type)) {
                crits.push_back(cr);
            }
        }
    }

    return crits;
}

void MapView::SetCritterContour(ident_t cr_id, ContourType contour)
{
    STACK_TRACE_ENTRY();

    if (_critterContourCrId) {
        auto* cr = GetCritter(_critterContourCrId);
        if (cr != nullptr && cr->SprDrawValid) {
            if (!cr->IsDead() && !cr->IsChosen()) {
                cr->SprDraw->SetContour(_crittersContour);
            }
            else {
                cr->SprDraw->SetContour(ContourType::None);
            }
        }
    }

    _critterContourCrId = cr_id;
    _critterContour = contour;

    if (cr_id) {
        auto* cr = GetCritter(cr_id);
        if (cr != nullptr && cr->SprDrawValid) {
            cr->SprDraw->SetContour(contour);
        }
    }
}

void MapView::SetCrittersContour(ContourType contour)
{
    STACK_TRACE_ENTRY();

    if (_crittersContour == contour) {
        return;
    }

    _crittersContour = contour;

    for (auto* cr : _critters) {
        if (!cr->IsChosen() && cr->SprDrawValid && !cr->IsDead() && cr->GetId() != _critterContourCrId) {
            cr->SprDraw->SetContour(contour);
        }
    }
}

void MapView::MoveCritter(CritterHexView* cr, uint16 hx, uint16 hy, bool smoothly)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(cr->GetMap() == this);
    RUNTIME_ASSERT(hx < _maxHexX && hy < _maxHexY);

    const auto old_hx = cr->GetHexX();
    const auto old_hy = cr->GetHexY();

    if (old_hx == hx && old_hy == hy) {
        return;
    }

    RemoveCritterFromField(cr);

    cr->SetHexX(hx);
    cr->SetHexY(hy);

    if (smoothly) {
        auto&& [ox, oy] = _engine->Geometry.GetHexInterval(hx, hy, old_hx, old_hy);
        cr->AddExtraOffs(ox, oy);
    }

    AddCritterToField(cr);
}

void MapView::SetMultihex(uint16 hx, uint16 hy, uint multihex, bool set)
{
    STACK_TRACE_ENTRY();

    if (multihex != 0u) {
        auto&& [sx, sy] = _engine->Geometry.GetHexOffsets((hx % 2) != 0);

        for (uint i = 0, j = GenericUtils::NumericalNumber(multihex) * GameSettings::MAP_DIR_COUNT; i < j; i++) {
            const auto cx = static_cast<int16>(hx) + sx[i];
            const auto cy = static_cast<int16>(hy) + sy[i];
            if (cx >= 0 && cy >= 0 && cx < _maxHexX && cy < _maxHexY) {
                auto& neighbor = GetField(static_cast<uint16>(cx), static_cast<uint16>(cy));
                neighbor.Flags.IsMultihex = set;
                neighbor.ProcessCache();
            }
        }
    }
}

auto MapView::GetHexAtScreenPos(int x, int y, uint16& hx, uint16& hy, int* hex_ox, int* hex_oy) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto xf = static_cast<float>(x) - static_cast<float>(_engine->Settings.ScrOx) / GetSpritesZoom();
    const auto yf = static_cast<float>(y) - static_cast<float>(_engine->Settings.ScrOy) / GetSpritesZoom();
    const auto ox = static_cast<float>(_engine->Settings.MapHexWidth) / GetSpritesZoom();
    const auto oy = static_cast<float>(_engine->Settings.MapHexHeight) / GetSpritesZoom();
    auto y2 = 0;

    for (auto ty = 0; ty < _hVisible; ty++) {
        for (auto tx = 0; tx < _wVisible; tx++) {
            const auto vpos = y2 + tx;
            const auto x_ = _viewField[vpos].ScrXf / GetSpritesZoom();
            const auto y_ = _viewField[vpos].ScrYf / GetSpritesZoom();

            if (xf >= x_ && xf < x_ + ox && yf >= y_ && yf < y_ + oy) {
                auto hx_ = _viewField[vpos].HexX;
                auto hy_ = _viewField[vpos].HexY;

                // Correct with hex color mask
                if (_picHexMask != nullptr) {
                    const auto* si = _engine->SprMngr.GetSpriteInfo(_picHexMask->Ind[0]);
                    const auto mask_x = std::clamp(iround((xf - x_) * GetSpritesZoom()), 0, si->Width - 1);
                    const auto mask_y = std::clamp(iround((yf - y_) * GetSpritesZoom()), 0, si->Height - 1);
                    const auto mask_color = _picHexMaskData[mask_y * si->Width + mask_x];
                    const auto mask_color_r = mask_color & 0x000000FF;

                    if (mask_color_r == 50) {
                        _engine->Geometry.MoveHexByDirUnsafe(hx_, hy_, GameSettings::HEXAGONAL_GEOMETRY ? 5u : 6u);
                    }
                    else if (mask_color_r == 100) {
                        _engine->Geometry.MoveHexByDirUnsafe(hx_, hy_, 0u);
                    }
                    else if (mask_color_r == 150) {
                        _engine->Geometry.MoveHexByDirUnsafe(hx_, hy_, GameSettings::HEXAGONAL_GEOMETRY ? 3u : 4u);
                    }
                    else if (mask_color_r == 200) {
                        _engine->Geometry.MoveHexByDirUnsafe(hx_, hy_, 2u);
                    }
                }

                if (hx_ >= 0 && hy_ >= 0 && hx_ < _maxHexX && hy_ < _maxHexY) {
                    hx = static_cast<uint16>(hx_);
                    hy = static_cast<uint16>(hy_);

                    if (hex_ox != nullptr && hex_oy != nullptr) {
                        *hex_ox = iround((xf - x_) * GetSpritesZoom()) - 16;
                        *hex_oy = iround((yf - y_) * GetSpritesZoom()) - 8;
                    }
                    return true;
                }
            }
        }
        y2 += _wVisible;
    }

    return false;
}

auto MapView::GetItemAtScreenPos(int x, int y, bool& item_egg, int extra_range, bool check_transparent) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<ItemHexView*> pix_item;
    vector<ItemHexView*> pix_item_egg;
    const auto is_egg = _engine->SprMngr.IsEggTransp(x, y);

    for (auto* item : _allItems) {
        if (!item->SprDrawValid || item->IsFinishing()) {
            continue;
        }

        const auto* si = _engine->SprMngr.GetSpriteInfo(item->SprId);
        const auto hx = item->GetHexX();
        const auto hy = item->GetHexY();
        const auto& field = GetField(hx, hy);
        const auto l = iround(static_cast<float>(field.ScrX + item->ScrX + si->OffsX + _engine->Settings.MapHexWidth / 2 + _engine->Settings.ScrOx - si->Width / 2) / GetSpritesZoom()) - extra_range;
        const auto r = iround(static_cast<float>(field.ScrX + item->ScrX + si->OffsX + _engine->Settings.MapHexWidth / 2 + _engine->Settings.ScrOx + si->Width / 2) / GetSpritesZoom()) + extra_range;
        const auto t = iround(static_cast<float>(field.ScrY + item->ScrY + si->OffsY + _engine->Settings.MapHexHeight / 2 + _engine->Settings.ScrOy - si->Height) / GetSpritesZoom()) - extra_range;
        const auto b = iround(static_cast<float>(field.ScrY + item->ScrY + si->OffsY + _engine->Settings.MapHexHeight / 2 + _engine->Settings.ScrOy) / GetSpritesZoom()) + extra_range;

        if (x < l || x > r || y < t || y > b) {
            continue;
        }

        if (!_mapperMode) {
            if (item->GetIsHidden() || item->GetIsHiddenPicture() || item->IsFullyTransparent()) {
                continue;
            }
            if (!_engine->Settings.ShowScen && item->IsScenery()) {
                continue;
            }
            if (!_engine->Settings.ShowItem && !item->IsAnyScenery()) {
                continue;
            }
            if (!_engine->Settings.ShowWall && item->IsWall()) {
                continue;
            }
        }
        else {
            const auto is_fast = _fastPids.count(item->GetProtoId()) != 0;
            if (item->IsScenery() && !_engine->Settings.ShowScen && !is_fast) {
                continue;
            }
            if (!item->IsAnyScenery() && !_engine->Settings.ShowItem && !is_fast) {
                continue;
            }
            if (item->IsWall() && !_engine->Settings.ShowWall && !is_fast) {
                continue;
            }
            if (!_engine->Settings.ShowFast && is_fast) {
                continue;
            }
            if (_ignorePids.count(item->GetProtoId()) != 0u) {
                continue;
            }
        }

        const auto* spr = item->SprDraw->GetIntersected(x - l + extra_range, y - t + extra_range, check_transparent);
        if (spr != nullptr) {
            if (is_egg && _engine->SprMngr.CheckEggAppearence(hx, hy, item->GetEggType())) {
                pix_item_egg.push_back(item);
            }
            else {
                pix_item.push_back(item);
            }
        }
    }

    // Sorters
    struct Sorter
    {
        static auto ByTreeIndex(const ItemHexView* o1, const ItemHexView* o2) -> bool { return o1->SprDraw->TreeIndex > o2->SprDraw->TreeIndex; }
        static auto ByTransparent(const ItemHexView* o1, const ItemHexView* o2) -> bool { return !o1->IsTransparent() && o2->IsTransparent(); }
    };

    // Egg items
    if (pix_item.empty()) {
        if (pix_item_egg.empty()) {
            return nullptr;
        }
        if (pix_item_egg.size() > 1) {
            std::sort(pix_item_egg.begin(), pix_item_egg.end(), Sorter::ByTreeIndex);
            std::sort(pix_item_egg.begin(), pix_item_egg.end(), Sorter::ByTransparent);
        }
        item_egg = true;
        return pix_item_egg[0];
    }

    // Visible items
    if (pix_item.size() > 1) {
        std::sort(pix_item.begin(), pix_item.end(), Sorter::ByTreeIndex);
        std::sort(pix_item.begin(), pix_item.end(), Sorter::ByTransparent);
    }

    item_egg = false;
    return pix_item[0];
}

auto MapView::GetCritterAtScreenPos(int x, int y, bool ignore_dead_and_chosen, int extra_range, bool check_transparent) -> CritterHexView*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!_engine->Settings.ShowCrit) {
        return nullptr;
    }

    vector<CritterHexView*> crits;
    for (auto* cr : _critters) {
        if (!cr->Visible || cr->IsFinishing() || !cr->SprDrawValid) {
            continue;
        }
        if (ignore_dead_and_chosen && (cr->IsDead() || cr->IsChosen())) {
            continue;
        }

        const auto rect = cr->GetViewRect();
        const auto l = iround(static_cast<float>(rect.Left + _engine->Settings.ScrOx) / GetSpritesZoom()) - extra_range;
        const auto r = iround(static_cast<float>(rect.Right + _engine->Settings.ScrOx) / GetSpritesZoom()) + extra_range;
        const auto t = iround(static_cast<float>(rect.Top + _engine->Settings.ScrOy) / GetSpritesZoom()) - extra_range;
        const auto b = iround(static_cast<float>(rect.Bottom + _engine->Settings.ScrOy) / GetSpritesZoom()) + extra_range;

        if (x >= l && x <= r && y >= t && y <= b) {
            if (check_transparent) {
                const auto rect_draw = _engine->SprMngr.GetDrawRect(cr->SprDraw);
                const auto l_draw = iround(static_cast<float>(rect_draw.Left + _engine->Settings.ScrOx) / GetSpritesZoom());
                const auto t_draw = iround(static_cast<float>(rect_draw.Top + _engine->Settings.ScrOy) / GetSpritesZoom());

                if (_engine->SprMngr.IsPixNoTransp(cr->SprId, x - l_draw, y - t_draw, true)) {
                    crits.push_back(cr);
                }
            }
            else {
                crits.push_back(cr);
            }
        }
    }

    if (crits.empty()) {
        return nullptr;
    }

    if (crits.size() > 1) {
        std::sort(crits.begin(), crits.end(), [](auto* cr1, auto* cr2) { return cr1->SprDraw->TreeIndex > cr2->SprDraw->TreeIndex; });
    }
    return crits[0];
}

auto MapView::GetEntityAtScreenPos(int x, int y, int extra_range, bool check_transparent) -> ClientEntity*
{
    STACK_TRACE_ENTRY();

    auto item_egg = false;
    ItemHexView* item = GetItemAtScreenPos(x, y, item_egg, extra_range, check_transparent);
    CritterHexView* cr = GetCritterAtScreenPos(x, y, false, extra_range, check_transparent);

    if (cr != nullptr && item != nullptr) {
        if (item->IsTransparent() || item_egg || item->SprDraw->TreeIndex <= cr->SprDraw->TreeIndex) {
            item = nullptr;
        }
        else {
            cr = nullptr;
        }
    }

    return cr != nullptr ? static_cast<ClientEntity*>(cr) : static_cast<ClientEntity*>(item);
}

auto MapView::FindPath(CritterHexView* cr, uint16 start_x, uint16 start_y, uint16& end_x, uint16& end_y, int cut) -> optional<FindPathResult>
{
    STACK_TRACE_ENTRY();

#define GRID_AT(x, y) _findPathGrid[((MAX_FIND_PATH + 1) + (y)-grid_oy) * (MAX_FIND_PATH * 2 + 2) + ((MAX_FIND_PATH + 1) + (x)-grid_ox)]

    if (start_x == end_x && start_y == end_y) {
        return FindPathResult();
    }

    int16 numindex = 1;
    std::memset(_findPathGrid.data(), 0, _findPathGrid.size() * sizeof(int16));

    auto grid_ox = start_x;
    auto grid_oy = start_y;
    GRID_AT(start_x, start_y) = numindex;

    vector<pair<uint16, uint16>> coords;
    coords.reserve(MAX_FIND_PATH);
    coords.emplace_back(start_x, start_y);

    auto mh = (cr != nullptr ? cr->GetMultihex() : 0);
    auto p = 0;
    auto find_ok = false;
    while (!find_ok) {
        if (++numindex > MAX_FIND_PATH) {
            return std::nullopt;
        }

        auto p_togo = static_cast<int>(coords.size()) - p;
        if (p_togo == 0) {
            return std::nullopt;
        }

        for (auto i = 0; i < p_togo && !find_ok; ++i, ++p) {
            int hx = coords[p].first;
            int hy = coords[p].second;

            const auto [sx, sy] = _engine->Geometry.GetHexOffsets((hx % 2) != 0);

            for (const auto j : xrange(GameSettings::MAP_DIR_COUNT)) {
                auto nx = hx + sx[j];
                auto ny = hy + sy[j];
                if (nx < 0 || ny < 0 || nx >= _maxHexX || ny >= _maxHexY || GRID_AT(nx, ny)) {
                    continue;
                }

                GRID_AT(nx, ny) = -1;

                if (mh == 0u) {
                    if (GetField(static_cast<uint16>(nx), static_cast<uint16>(ny)).Flags.IsMoveBlocked) {
                        continue;
                    }
                }
                else {
                    // Base hex
                    auto nx_ = nx;
                    auto ny_ = ny;
                    for (uint k = 0; k < mh; k++) {
                        _engine->Geometry.MoveHexByDirUnsafe(nx_, ny_, static_cast<uint8>(j));
                    }
                    if (nx_ < 0 || ny_ < 0 || nx_ >= _maxHexX || ny_ >= _maxHexY) {
                        continue;
                    }
                    if (GetField(static_cast<uint16>(nx_), static_cast<uint16>(ny_)).Flags.IsMoveBlocked) {
                        continue;
                    }

                    // Clock wise hexes
                    auto is_square_corner = (!GameSettings::HEXAGONAL_GEOMETRY && (j % 2) != 0);
                    auto steps_count = (is_square_corner ? mh * 2 : mh);
                    auto not_passed = false;
                    auto dir_ = (GameSettings::HEXAGONAL_GEOMETRY ? ((j + 2) % 6) : ((j + 2) % 8));
                    if (is_square_corner) {
                        dir_ = (dir_ + 1) % 8;
                    }

                    auto nx_2 = nx_;
                    auto ny_2 = ny_;
                    for (uint k = 0; k < steps_count && !not_passed; k++) {
                        _engine->Geometry.MoveHexByDirUnsafe(nx_2, ny_2, static_cast<uint8>(dir_));
                        not_passed = GetField(static_cast<uint16>(nx_2), static_cast<uint16>(ny_2)).Flags.IsMoveBlocked;
                    }
                    if (not_passed) {
                        continue;
                    }

                    // Counter clock wise hexes
                    dir_ = (GameSettings::HEXAGONAL_GEOMETRY ? (j + 4) % 6 : (j + 6) % 8);
                    if (is_square_corner) {
                        dir_ = (dir_ + 7) % 8;
                    }

                    nx_2 = nx_;
                    ny_2 = ny_;
                    for (uint k = 0; k < steps_count && !not_passed; k++) {
                        _engine->Geometry.MoveHexByDirUnsafe(nx_2, ny_2, static_cast<uint8>(dir_));
                        not_passed = GetField(static_cast<uint16>(nx_2), static_cast<uint16>(ny_2)).Flags.IsMoveBlocked;
                    }
                    if (not_passed) {
                        continue;
                    }
                }

                GRID_AT(nx, ny) = numindex;
                coords.emplace_back(nx, ny);

                if (cut >= 0 && _engine->Geometry.CheckDist(static_cast<uint16>(nx), static_cast<uint16>(ny), end_x, end_y, cut)) {
                    end_x = static_cast<uint16>(nx);
                    end_y = static_cast<uint16>(ny);
                    return FindPathResult();
                }

                if (cut < 0 && nx == end_x && ny == end_y) {
                    find_ok = true;
                    break;
                }
            }
        }
    }
    if (cut >= 0) {
        return std::nullopt;
    }

    int x1 = coords.back().first;
    int y1 = coords.back().second;

    vector<uint8> raw_steps;
    raw_steps.resize(numindex - 1);

    float base_angle = _engine->Geometry.GetDirAngle(end_x, end_y, start_x, start_y);

    // From end
    while (numindex > 1) {
        numindex--;

        int best_step_dir = -1;
        float best_step_angle_diff = 0.0f;

        const auto check_hex = [&best_step_dir, &best_step_angle_diff, numindex, grid_ox, grid_oy, start_x, start_y, base_angle, this](int dir, int step_hx, int step_hy) {
            if (GRID_AT(step_hx, step_hy) == numindex) {
                const float angle = _engine->Geometry.GetDirAngle(step_hx, step_hy, start_x, start_y);
                const float angle_diff = _engine->Geometry.GetDirAngleDiff(base_angle, angle);
                if (best_step_dir == -1 || numindex == 0) {
                    best_step_dir = dir;
                    best_step_angle_diff = _engine->Geometry.GetDirAngleDiff(base_angle, angle);
                }
                else {
                    if (best_step_dir == -1 || angle_diff < best_step_angle_diff) {
                        best_step_dir = dir;
                        best_step_angle_diff = angle_diff;
                    }
                }
            }
        };

        if ((x1 % 2) != 0) {
            check_hex(3, x1 - 1, y1 - 1);
            check_hex(2, x1, y1 - 1);
            check_hex(5, x1, y1 + 1);
            check_hex(0, x1 + 1, y1);
            check_hex(4, x1 - 1, y1);
            check_hex(1, x1 + 1, y1 - 1);

            if (best_step_dir == 3) {
                raw_steps[numindex - 1] = 3;
                x1--;
                y1--;
                continue;
            }
            if (best_step_dir == 2) {
                raw_steps[numindex - 1] = 2;
                y1--;
                continue;
            }
            if (best_step_dir == 5) {
                raw_steps[numindex - 1] = 5;
                y1++;
                continue;
            }
            if (best_step_dir == 0) {
                raw_steps[numindex - 1] = 0;
                x1++;
                continue;
            }
            if (best_step_dir == 4) {
                raw_steps[numindex - 1] = 4;
                x1--;
                continue;
            }
            if (best_step_dir == 1) {
                raw_steps[numindex - 1] = 1;
                x1++;
                y1--;
                continue;
            }
        }
        else {
            check_hex(3, x1 - 1, y1);
            check_hex(2, x1, y1 - 1);
            check_hex(5, x1, y1 + 1);
            check_hex(0, x1 + 1, y1 + 1);
            check_hex(4, x1 - 1, y1 + 1);
            check_hex(1, x1 + 1, y1);

            if (best_step_dir == 3) {
                raw_steps[numindex - 1] = 3;
                x1--;
                continue;
            }
            if (best_step_dir == 2) {
                raw_steps[numindex - 1] = 2;
                y1--;
                continue;
            }
            if (best_step_dir == 5) {
                raw_steps[numindex - 1] = 5;
                y1++;
                continue;
            }
            if (best_step_dir == 0) {
                raw_steps[numindex - 1] = 0;
                x1++;
                y1++;
                continue;
            }
            if (best_step_dir == 4) {
                raw_steps[numindex - 1] = 4;
                x1--;
                y1++;
                continue;
            }
            if (best_step_dir == 1) {
                raw_steps[numindex - 1] = 1;
                x1++;
                continue;
            }
        }

        return std::nullopt;
    }

    FindPathResult result;

    if (_engine->Settings.MapFreeMovement) {
        uint16 trace_hx = start_x;
        uint16 trace_hy = start_y;

        while (true) {
            uint16 trace_tx = end_x;
            uint16 trace_ty = end_y;

            for (auto i = static_cast<int>(raw_steps.size()) - 1; i >= 0; i--) {
                LineTracer tracer(_engine->Geometry, trace_hx, trace_hy, trace_tx, trace_ty, _maxHexX, _maxHexY, 0.0f);
                uint16 next_hx = trace_hx;
                uint16 next_hy = trace_hy;
                vector<uint8> direct_steps;
                bool failed = false;

                while (true) {
                    uint8 dir = tracer.GetNextHex(next_hx, next_hy);
                    direct_steps.push_back(dir);

                    if (next_hx == trace_tx && next_hy == trace_ty) {
                        break;
                    }

                    if (GRID_AT(next_hx, next_hy) <= 0) {
                        failed = true;
                        break;
                    }
                }

                if (failed) {
                    RUNTIME_ASSERT(i > 0);
                    _engine->Geometry.MoveHexByDir(trace_tx, trace_ty, _engine->Geometry.ReverseDir(raw_steps[i]), _maxHexX, _maxHexY);
                    continue;
                }

                for (const auto& ds : direct_steps) {
                    result.Steps.push_back(ds);
                }

                result.ControlSteps.emplace_back(static_cast<uint16>(result.Steps.size()));

                trace_hx = trace_tx;
                trace_hy = trace_ty;
                break;
            }

            if (trace_tx == end_x && trace_ty == end_y) {
                break;
            }
        }
    }
    else {
        for (size_t i = 0; i < raw_steps.size(); i++) {
            const auto cur_dir = raw_steps[i];
            result.Steps.push_back(cur_dir);

            for (size_t j = i + 1; j < raw_steps.size(); j++) {
                if (raw_steps[j] == cur_dir) {
                    result.Steps.push_back(cur_dir);
                    i++;
                }
                else {
                    break;
                }
            }

            result.ControlSteps.emplace_back(static_cast<uint16>(result.Steps.size()));
        }
    }

    RUNTIME_ASSERT(!result.Steps.empty());
    RUNTIME_ASSERT(!result.ControlSteps.empty());

    return {result};

#undef GRID_AT
}

bool MapView::CutPath(CritterHexView* cr, uint16 start_x, uint16 start_y, uint16& end_x, uint16& end_y, int cut)
{
    STACK_TRACE_ENTRY();

    return !!FindPath(cr, start_x, start_y, end_x, end_y, cut);
}

bool MapView::TraceMoveWay(uint16& hx, uint16& hy, int& ox, int& oy, vector<uint8>& steps, int quad_dir)
{
    STACK_TRACE_ENTRY();

    ox = 0;
    oy = 0;

    const auto try_move = [this, &hx, &hy, &steps](uint8 dir, bool check_only = false) {
        auto check_hx = hx;
        auto check_hy = hy;

        if (!_engine->Geometry.MoveHexByDir(check_hx, check_hy, dir, _maxHexX, _maxHexY)) {
            return false;
        }

        const auto& f = GetField(check_hx, check_hy);
        if (f.Flags.IsMoveBlocked) {
            return false;
        }

        hx = check_hx;
        hy = check_hy;
        steps.push_back(dir);
        return true;
    };

    const auto try_move2 = [&try_move, &ox](uint8 dir1, uint8 dir2) {
        if (try_move(dir1)) {
            if (!try_move(dir2)) {
                ox = dir1 == 0 ? -16 : 16;
                return false;
            }
            return true;
        }

        if (try_move(dir2)) {
            if (!try_move(dir1)) {
                ox = dir2 == 5 ? 16 : -16;
                return false;
            }
            return true;
        }

        return false;
    };

    constexpr auto some_big_path_len = 200;
    for (auto i = 0, j = (quad_dir == 3 || quad_dir == 7 ? some_big_path_len / 2 : some_big_path_len); i < j; i++) {
        if ((quad_dir == 0 && !try_move(hx % 2 == 0 ? 0 : 1)) || //
            (quad_dir == 1 && !try_move(1)) || //
            (quad_dir == 2 && !try_move(2)) || //
            (quad_dir == 3 && !try_move2(3, 2)) || //
            (quad_dir == 4 && !try_move(hx % 2 == 0 ? 4 : 3)) || //
            (quad_dir == 5 && !try_move(4)) || //
            (quad_dir == 6 && !try_move(5)) || //
            (quad_dir == 7 && !try_move2(0, 5))) {
            return i > 0;
        }
    }

    return true;
}

auto MapView::TraceBullet(uint16 hx, uint16 hy, uint16 tx, uint16 ty, uint dist, float angle, vector<CritterHexView*>* critters, CritterFindType find_type, pair<uint16, uint16>* pre_block, pair<uint16, uint16>* block, vector<pair<uint16, uint16>>* steps, bool check_passed) -> bool
{
    STACK_TRACE_ENTRY();

    if (_isShowTrack) {
        ClearHexTrack();
    }

    if (dist == 0u) {
        dist = _engine->Geometry.DistGame(hx, hy, tx, ty);
    }

    auto cx = hx;
    auto cy = hy;
    auto old_cx = cx;
    auto old_cy = cy;

    LineTracer line_tracer(_engine->Geometry, hx, hy, tx, ty, _maxHexX, _maxHexY, angle);

    for (uint i = 0; i < dist; i++) {
        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            line_tracer.GetNextHex(cx, cy);
        }
        else {
            line_tracer.GetNextSquare(cx, cy);
        }

        if (_isShowTrack) {
            GetHexTrack(cx, cy) = static_cast<char>(cx == tx && cy == ty ? 1 : 2);
        }

        if (steps != nullptr) {
            steps->push_back(std::make_pair(cx, cy));
            continue;
        }

        if (check_passed && GetField(cx, cy).Flags.IsShootBlocked) {
            break;
        }
        if (critters != nullptr) {
            auto hex_critters = GetCritters(cx, cy, find_type);
            critters->insert(critters->end(), hex_critters.begin(), hex_critters.end());
        }

        old_cx = cx;
        old_cy = cy;
    }

    if (pre_block != nullptr) {
        (*pre_block).first = old_cx;
        (*pre_block).second = old_cy;
    }
    if (block != nullptr) {
        (*block).first = cx;
        (*block).second = cy;
    }
    return false;
}

void MapView::FindSetCenter(int cx, int cy)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_viewField.empty());

    RebuildMap(cx, cy);

    if (_mapperMode) {
        return;
    }

    const auto iw = GetViewWidth() / 2 + 2;
    const auto ih = GetViewHeight() / 2 + 2;
    auto hx = static_cast<uint16>(cx);
    auto hy = static_cast<uint16>(cy);

    auto find_set_center_dir = [this, &hx, &hy](const array<int, 2>& dirs, int steps) {
        auto sx = hx;
        auto sy = hy;
        const auto dirs_index = (dirs[1] == -1 ? 1 : 2);

        auto i = 0;

        for (; i < steps; i++) {
            if (!_engine->Geometry.MoveHexByDir(sx, sy, static_cast<uint8>(dirs[i % dirs_index]), _maxHexX, _maxHexY)) {
                break;
            }

            if (_isShowTrack) {
                GetHexTrack(sx, sy) = 1;
            }

            if (GetField(sx, sy).Flags.ScrollBlock) {
                break;
            }

            if (_isShowTrack) {
                GetHexTrack(sx, sy) = 2;
            }
        }

        for (; i < steps; i++) {
            _engine->Geometry.MoveHexByDir(hx, hy, _engine->Geometry.ReverseDir(static_cast<uint8>(dirs[i % dirs_index])), _maxHexX, _maxHexY);
        }
    };

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        find_set_center_dir({0, 5}, ih); // Up
        find_set_center_dir({3, 2}, ih); // Down
        find_set_center_dir({1, -1}, iw); // Right
        find_set_center_dir({4, -1}, iw); // Left
        find_set_center_dir({0, -1}, ih); // Up-Right
        find_set_center_dir({3, -1}, ih); // Down-Left
        find_set_center_dir({5, -1}, ih); // Up-Left
        find_set_center_dir({2, -1}, ih); // Down-Right
    }
    else {
        find_set_center_dir({0, 6}, ih); // Up
        find_set_center_dir({4, 2}, ih); // Down
        find_set_center_dir({1, -1}, iw); // Right
        find_set_center_dir({5, -1}, iw); // Left
        find_set_center_dir({0, -1}, ih); // Up-Right
        find_set_center_dir({4, -1}, ih); // Down-Left
        find_set_center_dir({6, -1}, ih); // Up-Left
        find_set_center_dir({2, 1}, ih); // Down-Right
    }

    RebuildMap(hx, hy);
}

void MapView::SetShootBorders(bool enabled)
{
    STACK_TRACE_ENTRY();

    if (_drawShootBorders != enabled) {
        _drawShootBorders = enabled;
        RebuildFog();
    }
}

auto MapView::GetGlobalDayTime() const -> int
{
    STACK_TRACE_ENTRY();

    return _engine->GetHour() * 60 + _engine->GetMinute();
}

auto MapView::GetMapDayTime() const -> int
{
    STACK_TRACE_ENTRY();

    if (_mapperMode) {
        return GetGlobalDayTime();
    }

    const auto map_time = GetCurDayTime();

    return map_time >= 0 ? map_time : GetGlobalDayTime();
}

void MapView::OnScreenSizeChanged()
{
    STACK_TRACE_ENTRY();

    if (_viewField.empty()) {
        return;
    }

    _fogForceRerender = true;

    ResizeView();
    RefreshMap();
}

auto MapView::GetTiles(uint16 hx, uint16 hy, bool is_roof) -> vector<MapTile>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_mapperMode);

    return is_roof ? _roofsField[hy * _maxHexX + hx] : _tilesField[hy * _maxHexX + hx];
}

void MapView::ClearSelTiles()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    for (const auto hx : xrange(_maxHexX)) {
        for (const auto hy : xrange(_maxHexY)) {
            auto& field = GetField(hx, hy);
            if (field.GetTilesCount(false) != 0u) {
                auto& tiles = GetTiles(hx, hy, false);
                for (uint i = 0; i < tiles.size();) {
                    if (tiles[i].IsSelected) {
                        tiles.erase(tiles.begin() + i);
                        field.EraseTile(i, false);
                    }
                    else {
                        i++;
                    }
                }
            }
            if (field.GetTilesCount(true) != 0u) {
                auto& roofs = GetTiles(hx, hy, true);
                for (uint i = 0; i < roofs.size();) {
                    if (roofs[i].IsSelected) {
                        roofs.erase(roofs.begin() + i);
                        field.EraseTile(i, true);
                    }
                    else {
                        i++;
                    }
                }
            }
        }
    }
}

void MapView::ParseSelTiles()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    for (const auto hx : xrange(_maxHexX)) {
        for (const auto hy : xrange(_maxHexY)) {
            for (auto r = 0; r <= 1; r++) {
                const auto roof = (r == 1);
                auto& tiles = GetTiles(hx, hy, roof);
                for (size_t i = 0; i < tiles.size();) {
                    if (tiles[i].IsSelected) {
                        tiles[i].IsSelected = false;
                        EraseTile(hx, hy, tiles[i].Layer, roof, static_cast<int>(i));
                        i = 0;
                    }
                    else {
                        i++;
                    }
                }
            }
        }
    }
}

void MapView::SetTile(hstring name, uint16 hx, uint16 hy, int16 ox, int16 oy, uint8 layer, bool is_roof, bool select)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    if (hx >= _maxHexX || hy >= _maxHexY) {
        return;
    }

    auto* anim = _engine->ResMngr.GetItemAnim(name);
    if (anim == nullptr) {
        return;
    }

    if (!select) {
        EraseTile(hx, hy, layer, is_roof, static_cast<uint>(-1));
    }

    auto& field = GetField(hx, hy);
    const auto& ftile = field.AddTile(anim, 0, 0, layer, is_roof);
    auto& tiles = GetTiles(hx, hy, is_roof);
    tiles.push_back({name.as_hash(), hx, hy, ox, oy, layer, is_roof});
    tiles.back().IsSelected = select;

    if (!MeasureMapBorders(ftile, is_roof)) {
        if (is_roof) {
            RebuildRoof();
        }
        else {
            RebuildTiles();
        }
    }
}

void MapView::EraseTile(uint16 hx, uint16 hy, uint8 layer, bool is_roof, uint skip_index)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    auto& field = GetField(hx, hy);
    for (uint i = 0, j = field.GetTilesCount(is_roof); i < j; i++) {
        const auto& tile = field.GetTile(i, is_roof);
        if (tile.Layer == layer && i != skip_index) {
            field.EraseTile(i, is_roof);
            auto& tiles = GetTiles(hx, hy, is_roof);
            tiles.erase(tiles.begin() + i);
            break;
        }
    }
}

void MapView::AddFastPid(hstring pid)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    _fastPids.insert(pid);
}

auto MapView::IsFastPid(hstring pid) const -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    return _fastPids.count(pid) != 0;
}

void MapView::ClearFastPids()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    _fastPids.clear();
}

void MapView::AddIgnorePid(hstring pid)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);
    _ignorePids.insert(pid);
}

void MapView::SwitchIgnorePid(hstring pid)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    if (_ignorePids.count(pid) != 0u) {
        _ignorePids.erase(pid);
    }
    else {
        _ignorePids.insert(pid);
    }
}

auto MapView::IsIgnorePid(hstring pid) const -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    return _ignorePids.count(pid) != 0;
}

void MapView::ClearIgnorePids()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    _ignorePids.clear();
}

auto MapView::GetHexesRect(const IRect& rect) const -> vector<pair<uint16, uint16>>
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    vector<pair<uint16, uint16>> hexes;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        auto [x, y] = _engine->Geometry.GetHexInterval(rect.Left, rect.Top, rect.Right, rect.Bottom);
        x = -x;

        const auto dx = x / _engine->Settings.MapHexWidth;
        const auto dy = y / _engine->Settings.MapHexLineHeight;
        const auto adx = abs(dx);
        const auto ady = abs(dy);

        int hx;
        int hy;
        for (auto j = 0; j <= ady; j++) {
            if (dy >= 0) {
                hx = rect.Left + j / 2 + (j & 1);
                hy = rect.Top + (j - (hx - rect.Left - ((rect.Left % 2) != 0 ? 1 : 0)) / 2);
            }
            else {
                hx = rect.Left - j / 2 - (j & 1);
                hy = rect.Top - (j - (rect.Left - hx - ((rect.Left % 2) != 0 ? 0 : 1)) / 2);
            }

            for (auto i = 0; i <= adx; i++) {
                if (hx >= 0 && hy >= 0 && hx < _maxHexX && hy < _maxHexY) {
                    hexes.emplace_back(hx, hy);
                }

                if (dx >= 0) {
                    if ((hx % 2) != 0) {
                        hy--;
                    }
                    hx++;
                }
                else {
                    hx--;
                    if ((hx % 2) != 0) {
                        hy++;
                    }
                }
            }
        }
    }
    else {
        auto [rw, rh] = _engine->Geometry.GetHexInterval(rect.Left, rect.Top, rect.Right, rect.Bottom);
        if (rw == 0) {
            rw = 1;
        }
        if (rh == 0) {
            rh = 1;
        }

        const auto hw = std::abs(rw / (_engine->Settings.MapHexWidth / 2)) + ((rw % (_engine->Settings.MapHexWidth / 2)) != 0 ? 1 : 0) + (std::abs(rw) >= _engine->Settings.MapHexWidth / 2 ? 1 : 0); // Hexes width
        const auto hh = std::abs(rh / _engine->Settings.MapHexLineHeight) + ((rh % _engine->Settings.MapHexLineHeight) != 0 ? 1 : 0) + (std::abs(rh) >= _engine->Settings.MapHexLineHeight ? 1 : 0); // Hexes height
        auto shx = rect.Left;
        auto shy = rect.Top;

        for (auto i = 0; i < hh; i++) {
            auto hx = shx;
            auto hy = shy;

            if (rh > 0) {
                if (rw > 0) {
                    if ((i & 1) != 0) {
                        shx++;
                    }
                    else {
                        shy++;
                    }
                }
                else {
                    if ((i & 1) != 0) {
                        shy++;
                    }
                    else {
                        shx++;
                    }
                }
            }
            else {
                if (rw > 0) {
                    if ((i & 1) != 0) {
                        shy--;
                    }
                    else {
                        shx--;
                    }
                }
                else {
                    if ((i & 1) != 0) {
                        shx--;
                    }
                    else {
                        shy--;
                    }
                }
            }

            for (auto j = (i & 1) != 0 ? 1 : 0; j < hw; j += 2) {
                if (hx >= 0 && hy >= 0 && hx < _maxHexX && hy < _maxHexY) {
                    hexes.emplace_back(hx, hy);
                }

                if (rw > 0) {
                    hx--;
                    hy++;
                }
                else {
                    hx++;
                    hy--;
                }
            }
        }
    }

    return hexes;
}

void MapView::MarkBlockedHexes()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    for (uint16 hx = 0; hx < _maxHexX; hx++) {
        for (uint16 hy = 0; hy < _maxHexY; hy++) {
            const auto& field = GetField(hx, hy);
            auto& track = GetHexTrack(hx, hy);

            track = 0;

            if (field.Flags.IsMoveBlocked) {
                track = 2;
            }
            if (field.Flags.IsShootBlocked) {
                track = 1;
            }
        }
    }

    RefreshMap();
}

auto MapView::GetTempEntityId() const -> ident_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    auto max_id = static_cast<ident_t::underlying_type>(-1);

    for (const auto* cr : _critters) {
        RUNTIME_ASSERT(cr->GetId());
        max_id = std::min(cr->GetId().underlying_value(), max_id);
    }
    for (const auto* item : _allItems) {
        RUNTIME_ASSERT(item->GetId());
        max_id = std::min(item->GetId().underlying_value(), max_id);
    }

    RUNTIME_ASSERT(max_id > std::numeric_limits<ident_t::underlying_type>::max() / 2);
    return ident_t {max_id - 1};
}

auto MapView::SaveToText() const -> string
{
    STACK_TRACE_ENTRY();

    string fomap;
    fomap.reserve(0x1000000); // 1mb

    const auto fill_section = [&fomap](string_view name, const map<string, string>& section) {
        fomap.append("[").append(name).append("]").append("\n");
        for (auto&& [key, value] : section) {
            fomap.append(key).append(" = ").append(value).append("\n");
        }
        fomap.append("\n");
    };

    // Header
    fill_section("ProtoMap", GetProperties().SaveToText(nullptr));

    // Critters
    for (const auto* cr : _critters) {
        auto kv = cr->GetProperties().SaveToText(&cr->GetProto()->GetProperties());
        kv["$Id"] = _str("{}", cr->GetId());
        kv["$Proto"] = cr->GetProto()->GetName();
        fill_section("Critter", kv);
    }

    // Items
    for (const auto* item : _allItems) {
        auto kv = item->GetProperties().SaveToText(&item->GetProto()->GetProperties());
        kv["$Id"] = _str("{}", item->GetId());
        kv["$Proto"] = item->GetProto()->GetName();
        fill_section("Item", kv);
    }

    // Tiles
    for (uint16 hy = 0; hy < _maxHexY; hy++) {
        for (uint16 hx = 0; hx < _maxHexX; hx++) {
            const auto fill_tile = [&](const MapTile& tile) {
                map<string, string> kv;
                kv["PicMap"] = _engine->ResolveHash(tile.NameHash);
                kv["HexX"] = _str("{}", tile.HexX);
                kv["HexY"] = _str("{}", tile.HexY);
                if (tile.OffsX != 0) {
                    kv["OffsetX"] = _str("{}", tile.OffsX);
                }
                if (tile.OffsY != 0) {
                    kv["OffsetY"] = _str("{}", tile.OffsY);
                }
                if (tile.Layer != 0) {
                    kv["Layer"] = _str("{}", tile.Layer);
                }
                if (tile.IsRoof) {
                    kv["IsRoof"] = _str("{}", tile.IsRoof);
                }
                fill_section("Tile", kv);
            };

            for (const auto& tile : _tilesField[hy * _maxHexX + hx]) {
                fill_tile(tile);
            }
            for (const auto& tile : _roofsField[hy * _maxHexX + hx]) {
                fill_tile(tile);
            }
        }
    }

    return fomap;
}
