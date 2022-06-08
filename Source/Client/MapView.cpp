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

static auto EvaluateItemDrawOrder(const ItemHexView* item)
{
    return item->GetIsFlat() ? (!item->IsAnyScenery() ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY) : (!item->IsAnyScenery() ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY);
}

static auto EvaluateCritterDrawOrder(const CritterHexView* cr)
{
    return cr->IsDead() && !cr->GetIsNoFlatten() ? DRAW_ORDER_DEAD_CRITTER : DRAW_ORDER_CRITTER;
}

Field::~Field()
{
    delete DeadCrits;
    delete Tiles[0];
    delete Tiles[1];
    delete Items;
    delete BlockLinesItems;
}

void Field::AddItem(ItemHexView* item, ItemHexView* block_lines_item)
{
    RUNTIME_ASSERT(item || block_lines_item);

    if (item != nullptr) {
        item->HexScrX = &ScrX;
        item->HexScrY = &ScrY;

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

auto Field::AddTile(AnyFrames* anim, short ox, short oy, uchar layer, bool is_roof) -> Tile&
{
    Tile* tile;
    auto*& tiles_vec = Tiles[is_roof ? 1 : 0];
    auto*& stile = SimplyTile[is_roof ? 1 : 0];

    if (tiles_vec == nullptr && stile == nullptr && ox == 0 && oy == 0 && layer == 0u) {
        static Tile simply_tile;

        stile = anim;
        tile = &simply_tile;
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
    auto*& tiles_vec = Tiles[is_roof ? 1 : 0];
    auto*& stile = SimplyTile[is_roof ? 1 : 0];

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
    auto*& tiles_vec = Tiles[is_roof ? 1 : 0];
    auto*& stile = SimplyTile[is_roof ? 1 : 0];

    return (stile != nullptr ? 1 : 0) + (tiles_vec != nullptr ? static_cast<uint>(tiles_vec->size()) : 0);
}

auto Field::GetTile(uint index, bool is_roof) -> Field::Tile&
{
    auto*& tiles_vec = Tiles[is_roof ? 1 : 0];
    auto*& stile = SimplyTile[is_roof ? 1 : 0];

    if (index == 0 && (stile != nullptr)) {
        static Tile simply_tile;
        simply_tile.Anim = stile;
        return simply_tile;
    }
    return tiles_vec->at(index - (stile != nullptr ? 1 : 0));
}

void Field::AddDeadCrit(CritterHexView* cr)
{
    if (DeadCrits == nullptr) {
        DeadCrits = new vector<CritterHexView*>();
    }

    if (std::find(DeadCrits->begin(), DeadCrits->end(), cr) == DeadCrits->end()) {
        DeadCrits->push_back(cr);
    }
}

void Field::EraseDeadCrit(CritterHexView* cr)
{
    if (DeadCrits == nullptr) {
        return;
    }

    const auto it = std::find(DeadCrits->begin(), DeadCrits->end(), cr);
    if (it != DeadCrits->end()) {
        DeadCrits->erase(it);

        if (DeadCrits->empty()) {
            delete DeadCrits;
            DeadCrits = nullptr;
        }
    }
}

void Field::ProcessCache()
{
    Flags.IsWall = false;
    Flags.IsWallTransp = false;
    Flags.IsScen = false;
    Flags.IsNotPassed = (Crit != nullptr || Flags.IsMultihex);
    Flags.IsNotRaked = false;
    Flags.IsNoLight = false;
    Flags.ScrollBlock = false;
    Corner = CornerType::NorthSouth;

    if (Items != nullptr) {
        for (const auto* item : *Items) {
            if (item->IsWall()) {
                Flags.IsWall = true;
                Flags.IsWallTransp = item->GetIsLightThru();

                Corner = item->GetCorner();
            }
            else if (item->IsScenery()) {
                Flags.IsScen = true;
            }

            if (!item->GetIsNoBlock()) {
                Flags.IsNotPassed = true;
            }
            if (!item->GetIsShootThru()) {
                Flags.IsNotRaked = true;
            }
            if (item->GetIsScrollBlock()) {
                Flags.ScrollBlock = true;
            }
            if (!item->GetIsLightThru()) {
                Flags.IsNoLight = true;
            }
        }
    }

    if (BlockLinesItems != nullptr) {
        for (const auto* item : *BlockLinesItems) {
            Flags.IsNotPassed = true;
            if (!item->GetIsShootThru()) {
                Flags.IsNotRaked = true;
            }
            if (!item->GetIsLightThru()) {
                Flags.IsNoLight = true;
            }
        }
    }
}

void Field::AddSpriteToChain(Sprite* spr)
{
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
    if (SpriteChain != nullptr) {
        while (SpriteChain != nullptr) {
            SpriteChain->Unvalidate();
        }
    }
}

MapView::MapView(FOClient* engine, uint id, const ProtoMap* proto) : ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME), proto), MapProperties(GetInitRef()), _mainTree(engine->SprMngr, _spritesPool), _tilesTree(engine->SprMngr, _spritesPool), _roofTree(engine->SprMngr, _spritesPool)
{
    _rtScreenOx = static_cast<uint>(std::ceil(static_cast<float>(_engine->Settings.MapHexWidth) / MIN_ZOOM));
    _rtScreenOy = static_cast<uint>(std::ceil(static_cast<float>(_engine->Settings.MapHexLineHeight * 2) / MIN_ZOOM));

    _rtMap = _engine->SprMngr.CreateRenderTarget(false, RenderTarget::SizeType::Map, 0, 0, false);
    _rtMap->CustomDrawEffect = _engine->EffectMngr.Effects.FlushMap;

    _rtLight = _engine->SprMngr.CreateRenderTarget(false, RenderTarget::SizeType::Map, _rtScreenOx * 2, _rtScreenOy * 2, false);
    _rtLight->CustomDrawEffect = _engine->EffectMngr.Effects.FlushLight;

    _rtFog = _engine->SprMngr.CreateRenderTarget(false, RenderTarget::SizeType::Map, _rtScreenOx * 2, _rtScreenOy * 2, false);
    _rtFog->CustomDrawEffect = _engine->EffectMngr.Effects.FlushFog;

    _dayTime[0] = 300;
    _dayTime[1] = 600;
    _dayTime[2] = 1140;
    _dayTime[3] = 1380;
    _dayColor[0] = 18;
    _dayColor[1] = 128;
    _dayColor[2] = 103;
    _dayColor[3] = 51;
    _dayColor[4] = 18;
    _dayColor[5] = 128;
    _dayColor[6] = 95;
    _dayColor[7] = 40;
    _dayColor[8] = 53;
    _dayColor[9] = 128;
    _dayColor[10] = 86;
    _dayColor[11] = 29;

    _findPathGrid = new short[(MAX_FIND_PATH * 2 + 2) * (MAX_FIND_PATH * 2 + 2)];

    _eventUnsubscriber += _engine->SprMngr.GetWindow()->OnWindowSizeChanged += [this] { OnWindowSizeChanged(); };
}

MapView::~MapView()
{
    delete[] _findPathGrid;

    _mainTree.Clear();
    _roofTree.Clear();
    _tilesTree.Clear();

    for (auto* cr : _critters) {
        cr->MarkAsDestroyed();
        cr->Release();
    }
    for (auto* item : _items) {
        item->MarkAsDestroyed();
        item->Release();
    }

    delete[] _viewField;
    delete[] _hexField;
    delete[] _hexTrack;
    delete[] _hexLight;
}

void MapView::MarkAsDestroyed()
{
    for (auto* cr : _critters) {
        cr->MarkAsDestroyed();
        cr->Release();
    }
    _critters.clear();
    _crittersMap.clear();

    for (auto* item : _items) {
        item->MarkAsDestroyed();
        item->Release();
    }
    _items.clear();
    _itemsMap.clear();
}

void MapView::EnableMapperMode()
{
    _mapperMode = true;
}

void MapView::Process()
{
    for (auto* cr : copy(_critters)) {
        cr->Process();

        if (cr->IsNeedReset()) {
            RemoveCritterFromField(cr);
            AddCritterToField(cr);
            cr->ResetOk();
        }

        if (cr->IsFinished()) {
            DestroyCritter(cr);
        }
    }

    if (Scroll()) {
        // LookBordersPrepare();
    }

    ProcessItems();
}

void MapView::AddTile(const MapTile& tile)
{
    if (!tile.IsRoof) {
        TilesField[tile.HexY * _maxHexX + tile.HexX].push_back(tile);
        TilesField[tile.HexY * _maxHexX + tile.HexX].back().IsSelected = false;
    }
    else {
        RoofsField[tile.HexY * _maxHexX + tile.HexX].push_back(tile);
        RoofsField[tile.HexY * _maxHexX + tile.HexX].back().IsSelected = false;
    }

    const auto anim_name = _engine->ResolveHash(tile.NameHash);
    AnyFrames* anim = _engine->ResMngr.GetItemAnim(anim_name);
    if (anim != nullptr) {
        Field& field = GetField(tile.HexX, tile.HexY);
        Field::Tile& ftile = field.AddTile(anim, tile.OffsX, tile.OffsY, tile.Layer, tile.IsRoof);
        ProcessTileBorder(ftile, tile.IsRoof);
    }
}

void MapView::AddMapText(string_view str, ushort hx, ushort hy, uint color, uint show_time, bool fade, int ox, int oy)
{
    MapText map_text;
    map_text.HexX = hx;
    map_text.HexY = hy;
    map_text.Color = (color != 0u ? color : COLOR_TEXT);
    map_text.Fade = fade;
    map_text.StartTick = _engine->GameTime.GameTick();
    map_text.Tick = show_time;
    map_text.Text = str;
    map_text.Pos = GetRectForText(hx, hy);
    map_text.EndPos = IRect(map_text.Pos, ox, oy);

    const auto it = std::find_if(_mapTexts.begin(), _mapTexts.end(), [&map_text](const MapText& t) { return map_text.HexX == t.HexX && map_text.HexY == t.HexY; });
    if (it != _mapTexts.end()) {
        _mapTexts.erase(it);
    }

    _mapTexts.push_back(map_text);
}

auto MapView::GetViewWidth() const -> int
{
    return static_cast<int>(static_cast<float>(_engine->Settings.ScreenWidth / _engine->Settings.MapHexWidth + ((_engine->Settings.ScreenWidth % _engine->Settings.MapHexWidth) != 0 ? 1 : 0)) * _engine->Settings.SpritesZoom);
}

auto MapView::GetViewHeight() const -> int
{
    return static_cast<int>(static_cast<float>((_engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight) / _engine->Settings.MapHexLineHeight + (((_engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight) % _engine->Settings.MapHexLineHeight) != 0 ? 1 : 0)) * _engine->Settings.SpritesZoom);
}

void MapView::ReloadSprites()
{
    _curDataPrefix = _engine->Settings.MapDataPrefix;

    _picHex[0] = _engine->SprMngr.LoadAnimation(_curDataPrefix + "hex1.png", true, false);
    _picHex[1] = _engine->SprMngr.LoadAnimation(_curDataPrefix + "hex2.png", true, false);
    _picHex[2] = _engine->SprMngr.LoadAnimation(_curDataPrefix + "hex3.png", true, false);
    _cursorPrePic = _engine->SprMngr.LoadAnimation(_curDataPrefix + "move_pre.png", false, false);
    _cursorPostPic = _engine->SprMngr.LoadAnimation(_curDataPrefix + "move_post.png", false, false);
    _cursorXPic = _engine->SprMngr.LoadAnimation(_curDataPrefix + "move_x.png", false, false);
    _picTrack1 = _engine->SprMngr.LoadAnimation(_curDataPrefix + "track1.png", true, false);
    _picTrack2 = _engine->SprMngr.LoadAnimation(_curDataPrefix + "track2.png", true, false);
    _picHexMask = _engine->SprMngr.LoadAnimation(_curDataPrefix + "hex_mask.png", false, false);
}

void MapView::AddItemToField(ItemHexView* item)
{
    const ushort hx = item->GetHexX();
    const ushort hy = item->GetHexY();

    GetField(hx, hy).AddItem(item, nullptr);

    if (item->IsNonEmptyBlockLines()) {
        _engine->Geometry.ForEachBlockLines(item->GetBlockLines(), hx, hy, _maxHexX, _maxHexY, [this, item](auto hx2, auto hy2) { GetField(hx2, hy2).AddItem(nullptr, item); });
    }
}

void MapView::RemoveItemFromField(ItemHexView* item)
{
    const ushort hx = item->GetHexX();
    const ushort hy = item->GetHexY();

    GetField(hx, hy).EraseItem(item, nullptr);

    if (item->IsNonEmptyBlockLines()) {
        _engine->Geometry.ForEachBlockLines(item->GetBlockLines(), hx, hy, _maxHexX, _maxHexY, [this, item](auto hx2, auto hy2) { GetField(hx2, hy2).EraseItem(nullptr, item); });
    }
}

auto MapView::AddItem(uint id, const ProtoItem* proto, const map<string, string>& props_kv) -> ItemHexView*
{
    if (_itemsMap.count(id) != 0u) {
        return nullptr;
    }

    auto* item = new ItemHexView(this, id, proto);
    if (!item->LoadFromText(props_kv)) {
        item->Release();
        return nullptr;
    }

    _items.emplace_back(item);
    _itemsMap.emplace(id, item);

    return item;
}

auto MapView::AddItem(uint id, hstring pid, ushort hx, ushort hy, bool is_added, vector<vector<uchar>>* data) -> ItemHexView*
{
    RUNTIME_ASSERT(id != 0u);
    RUNTIME_ASSERT(!(hx >= _maxHexX || hy >= _maxHexY));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    RUNTIME_ASSERT(proto);

    // Parse
    auto& field = GetField(hx, hy);
    auto* item = new ItemHexView(this, id, proto, data, hx, hy, &field.ScrX, &field.ScrY);
    if (is_added) {
        item->SetShowAnim();
    }
    AddItem(item);

    // Check ViewField size
    if (!ProcessHexBorders(item->Anim->GetSprId(0), item->GetOffsetX(), item->GetOffsetY(), true)) {
        // Draw
        if (IsHexToDraw(hx, hy) && !item->GetIsHidden() && !item->GetIsHiddenPicture() && !item->IsFullyTransparent()) {
            auto& spr = _mainTree.InsertSprite(EvaluateItemDrawOrder(item), hx, hy + item->GetDrawOrderOffsetHexY(), (_engine->Settings.MapHexWidth / 2), (_engine->Settings.MapHexHeight / 2), &field.ScrX, &field.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
            if (!item->GetIsNoLightInfluence()) {
                spr.SetLight(item->GetCorner(), _hexLight, _maxHexX, _maxHexY);
            }
            item->SetSprite(&spr);

            field.AddSpriteToChain(&spr);
        }

        if (item->GetIsLight() || !item->GetIsLightThru()) {
            RebuildLight();
        }
    }

    return item;
}

void MapView::AddItem(ItemHexView* item)
{
    if (item->GetId() != 0u) {
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

    _items.emplace_back(item);
    _itemsMap.emplace(item->GetId(), item);

    AddItemToField(item);

    // Sort
    const auto hx = item->GetHexX();
    const auto hy = item->GetHexY();
    auto& field = GetField(hx, hy);
    std::sort(field.Items->begin(), field.Items->end(), [](auto* i1, auto* i2) { return i1->IsScenery() && !i2->IsScenery(); });
    std::sort(field.Items->begin(), field.Items->end(), [](auto* i1, auto* i2) { return i1->IsWall() && !i2->IsWall(); });
}

void MapView::MoveItem(ItemHexView* item, ushort hx, ushort hy)
{
    RUNTIME_ASSERT(item->GetMap() == this);

    RemoveItemFromField(item);
    item->SetHexX(hx);
    item->SetHexY(hy);
    AddItemToField(item);
}

void MapView::DestroyItem(ItemHexView* item)
{
    if (item->SprDrawValid) {
        item->SprDraw->Unvalidate();
    }

    const auto it = std::find(_items.begin(), _items.end(), item);
    RUNTIME_ASSERT(it != _items.end());
    _items.erase(it);

    if (item->GetId() != 0u) {
        const auto it_map = _itemsMap.find(item->GetId());
        RUNTIME_ASSERT(it_map != _itemsMap.end());
        _itemsMap.erase(it_map);
    }

    RemoveItemFromField(item);

    if (item->GetIsLight() || !item->GetIsLightThru()) {
        RebuildLight();
    }

    item->MarkAsDestroyed();
    item->Release();
}

void MapView::ProcessItems()
{
    for (auto* item : copy(_items)) {
        item->Process();

        if (item->IsDynamicEffect() && !item->IsFinishing()) {
            const auto [step_hx, step_hy] = item->GetEffectStep();
            if (item->GetHexX() != step_hx || item->GetHexY() != step_hy) {
                const auto hx = item->GetHexX();
                const auto hy = item->GetHexY();

                const auto [x, y] = _engine->Geometry.GetHexInterval(hx, hy, step_hx, step_hy);
                item->EffOffsX -= static_cast<float>(x);
                item->EffOffsY -= static_cast<float>(y);

                RemoveItemFromField(item);
                item->SetHexX(step_hx);
                item->SetHexY(step_hy);
                AddItemToField(item);

                auto& field = GetField(step_hx, step_hy);

                if (item->SprDrawValid) {
                    item->SprDraw->Unvalidate();
                }

                if (IsHexToDraw(step_hx, step_hy)) {
                    item->SprDraw = &_mainTree.InsertSprite(EvaluateItemDrawOrder(item), step_hx, step_hy + item->GetDrawOrderOffsetHexY(), (_engine->Settings.MapHexWidth / 2), (_engine->Settings.MapHexHeight / 2), &field.ScrX, &field.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
                    if (!item->GetIsNoLightInfluence()) {
                        item->SprDraw->SetLight(item->GetCorner(), _hexLight, _maxHexX, _maxHexY);
                    }
                    field.AddSpriteToChain(item->SprDraw);
                }

                item->SetAnimOffs();
            }
        }

        if (item->IsFinished()) {
            DestroyItem(item);
        }
    }
}

void MapView::SkipItemsFade()
{
    for (auto* item : _items) {
        item->SkipFade();
    }
}

auto MapView::GetItem(ushort hx, ushort hy, hstring pid) -> ItemHexView*
{
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

auto MapView::GetItem(ushort hx, ushort hy, uint id) -> ItemHexView*
{
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

auto MapView::GetItem(uint id) -> ItemHexView*
{
    if (const auto it = _itemsMap.find(id); it != _itemsMap.end()) {
        return it->second;
    }

    return nullptr;
}

auto MapView::GetItems(ushort hx, ushort hy) -> vector<ItemHexView*>
{
    const auto& field = GetField(hx, hy);
    if (field.Items != nullptr) {
        return *field.Items;
    }

    return {};
}

auto MapView::GetRectForText(ushort hx, ushort hy) -> IRect
{
    auto& field = GetField(hx, hy);

    // Critters first
    if (field.Crit != nullptr) {
        return field.Crit->GetTextRect();
    }
    if (field.DeadCrits != nullptr) {
        return field.DeadCrits->front()->GetTextRect();
    }

    // Items
    IRect r = {0, 0, 0, 0};
    if (field.Items != nullptr) {
        for (auto& item : *field.Items) {
            const auto* si = _engine->SprMngr.GetSpriteInfo(item->SprId);
            if (si != nullptr) {
                const auto w = si->Width - si->OffsX;
                const auto h = si->Height - si->OffsY;
                if (w > r.Left) {
                    r.Left = w;
                }
                if (h > r.Bottom) {
                    r.Bottom = h;
                }
            }
        }
    }
    return r;
}

auto MapView::RunEffect(hstring eff_pid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy) -> bool
{
    RUNTIME_ASSERT(!(from_hx >= _maxHexX || from_hy >= _maxHexY || to_hx >= _maxHexX || to_hy >= _maxHexY));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(eff_pid);
    RUNTIME_ASSERT(proto);

    auto& field = GetField(from_hx, from_hy);
    auto* item = new ItemHexView(this, 0, proto, nullptr, from_hx, from_hy, &field.ScrX, &field.ScrY);

    auto sx = 0.0f;
    auto sy = 0.0f;
    auto dist = 0u;

    if (from_hx != to_hx || from_hy != to_hy) {
        item->EffSteps.emplace_back(from_hx, from_hy);
        TraceBullet(from_hx, from_hy, to_hx, to_hy, 0, 0.0f, nullptr, false, nullptr, CritterFindType::Any, nullptr, nullptr, &item->EffSteps, false);
        auto [x, y] = _engine->Geometry.GetHexInterval(from_hx, from_hy, to_hx, to_hy);
        y += GenericUtils::Random(5, 25); // Center of body
        std::tie(sx, sy) = GenericUtils::GetStepsCoords(0, 0, x, y);
        dist = GenericUtils::DistSqrt(0, 0, x, y);
    }

    item->SetEffect(sx, sy, dist, _engine->Geometry.GetFarDir(from_hx, from_hy, to_hx, to_hy));

    AddItemToField(item);

    _items.push_back(item);

    if (IsHexToDraw(from_hx, from_hy)) {
        item->SprDraw = &_mainTree.InsertSprite(EvaluateItemDrawOrder(item), from_hx, from_hy + item->GetDrawOrderOffsetHexY(), (_engine->Settings.MapHexWidth / 2), (_engine->Settings.MapHexHeight / 2), &field.ScrX, &field.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
        if (!item->GetIsNoLightInfluence()) {
            item->SprDraw->SetLight(item->GetCorner(), _hexLight, _maxHexX, _maxHexY);
        }
        field.AddSpriteToChain(item->SprDraw);
    }

    return true;
}

void MapView::SetCursorPos(CritterHexView* cr, int x, int y, bool show_steps, bool refresh)
{
    ushort hx = 0;
    ushort hy = 0;
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

        if ((cx == hx && cy == hy) || (field.Flags.IsNotPassed && (mh == 0u || !_engine->Geometry.CheckDist(cx, cy, hx, hy, mh)))) {
            _drawCursorX = -1;
        }
        else {
            static auto last_cur_x = 0;
            static ushort last_hx = 0;
            static ushort last_hy = 0;

            if (refresh || hx != last_hx || hy != last_hy) {
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

                last_hx = hx;
                last_hy = hy;
                last_cur_x = _drawCursorX;
            }
            else {
                _drawCursorX = last_cur_x;
            }
        }
    }
}

void MapView::DrawCursor(uint spr_id)
{
    NON_CONST_METHOD_HINT();

    if (_engine->Settings.HideCursor || !_engine->Settings.ShowMoveCursor) {
        return;
    }

    const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
    if (si != nullptr) {
        _engine->SprMngr.DrawSpriteSize(spr_id,
            static_cast<int>(static_cast<float>(_cursorX + _engine->Settings.ScrOx) / _engine->Settings.SpritesZoom), //
            static_cast<int>(static_cast<float>(_cursorY + _engine->Settings.ScrOy) / _engine->Settings.SpritesZoom), //
            static_cast<int>(static_cast<float>(si->Width) / _engine->Settings.SpritesZoom), //
            static_cast<int>(static_cast<float>(si->Height) / _engine->Settings.SpritesZoom), true, false, 0);
    }
}

void MapView::DrawCursor(string_view text)
{
    NON_CONST_METHOD_HINT();

    if (_engine->Settings.HideCursor || !_engine->Settings.ShowMoveCursor) {
        return;
    }

    const auto x = static_cast<int>(static_cast<float>(_cursorX + _engine->Settings.ScrOx) / _engine->Settings.SpritesZoom);
    const auto y = static_cast<int>(static_cast<float>(_cursorY + _engine->Settings.ScrOy) / _engine->Settings.SpritesZoom);
    const auto r = IRect(x, y, static_cast<int>(static_cast<float>(x + _engine->Settings.MapHexWidth) / _engine->Settings.SpritesZoom), //
        static_cast<int>(static_cast<float>(y + _engine->Settings.MapHexHeight) / _engine->Settings.SpritesZoom));
    _engine->SprMngr.DrawStr(r, text, FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, 0);
}

void MapView::RebuildMap(int rx, int ry)
{
    RUNTIME_ASSERT(_viewField);

    for (auto i = 0, j = _hVisible * _wVisible; i < j; i++) {
        const auto hx = _viewField[i].HexX;
        const auto hy = _viewField[i].HexY;
        if (hx < 0 || hy < 0 || hx >= _maxHexX || hy >= _maxHexY) {
            continue;
        }

        auto& field = GetField(static_cast<ushort>(hx), static_cast<ushort>(hy));
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
        auto& vf = _viewField[i];

        if (vf.HexX < 0 || vf.HexY < 0 || vf.HexX >= _maxHexX || vf.HexY >= _maxHexY) {
            continue;
        }

        const auto nx = static_cast<ushort>(vf.HexX);
        const auto ny = static_cast<ushort>(vf.HexY);

        auto& field = GetField(nx, ny);

        field.IsView = true;
        field.ScrX = vf.ScrX;
        field.ScrY = vf.ScrY;

        // Track
        if (_isShowTrack && GetHexTrack(nx, ny) != 0) {
            const auto spr_id = (GetHexTrack(nx, ny) == 1 ? _picTrack1->GetCurSprId(_engine->GameTime.GameTick()) : _picTrack2->GetCurSprId(_engine->GameTime.GameTick()));
            const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
            auto& spr = _mainTree.AddSprite(DRAW_ORDER_TRACK, nx, ny, (_engine->Settings.MapHexWidth / 2), (_engine->Settings.MapHexHeight / 2) + (si != nullptr ? si->Height / 2 : 0), &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            field.AddSpriteToChain(&spr);
        }

        // Hex Lines
        if (_isShowHex) {
            const auto spr_id = _picHex[0]->GetCurSprId(_engine->GameTime.GameTick());
            const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
            auto& spr = _mainTree.AddSprite(DRAW_ORDER_HEX_GRID, nx, ny, si != nullptr ? si->Width / 2 : 0, si != nullptr ? si->Height : 0, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
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

                auto& spr = _mainTree.AddSprite(EvaluateItemDrawOrder(item), nx, ny + item->GetDrawOrderOffsetHexY(), _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
                if (!item->GetIsNoLightInfluence()) {
                    spr.SetLight(item->GetCorner(), _hexLight, _maxHexX, _maxHexY);
                }
                item->SetSprite(&spr);
                field.AddSpriteToChain(&spr);
            }
        }

        // Critters
        auto* cr = field.Crit;
        if (cr != nullptr && _engine->Settings.ShowCrit && cr->Visible) {
            auto& spr = _mainTree.AddSprite(EvaluateCritterDrawOrder(cr), nx, ny, _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy, &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid);
            spr.SetLight(CornerType::EastWest, _hexLight, _maxHexX, _maxHexY);
            cr->SprDraw = &spr;
            cr->SetSprRect();

            auto contour = 0;
            if (cr->GetId() == _critterContourCrId) {
                contour = _critterContour;
            }
            else if (!cr->IsChosen()) {
                contour = _crittersContour;
            }
            spr.SetContour(contour, cr->GetContourColor());

            field.AddSpriteToChain(&spr);
        }

        // Dead critters
        if (field.DeadCrits != nullptr && _engine->Settings.ShowCrit) {
            for (auto* dead_cr : *field.DeadCrits) {
                if (!dead_cr->Visible) {
                    continue;
                }

                auto& spr = _mainTree.AddSprite(EvaluateCritterDrawOrder(dead_cr), nx, ny, _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &dead_cr->SprId, &dead_cr->SprOx, &dead_cr->SprOy, &dead_cr->Alpha, &dead_cr->DrawEffect, &dead_cr->SprDrawValid);
                spr.SetLight(CornerType::EastWest, _hexLight, _maxHexX, _maxHexY);
                dead_cr->SprDraw = &spr;
                dead_cr->SetSprRect();

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
    RUNTIME_ASSERT(_viewField);
    RUNTIME_ASSERT(ox == 0 || ox == -1 || ox == 1);
    RUNTIME_ASSERT(oy == 0 || oy == -2 || oy == 2);

    auto hide_hex = [this](ViewField& vf) {
        const auto nxi = vf.HexX;
        const auto nyi = vf.HexY;
        if (nxi < 0 || nyi < 0 || nxi >= _maxHexX || nyi >= _maxHexY) {
            return;
        }

        const auto nx = static_cast<ushort>(nxi);
        const auto ny = static_cast<ushort>(nyi);
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
            auto& field = GetField(static_cast<ushort>(vf.HexX), static_cast<ushort>(vf.HexY));
            field.ScrX = vf.ScrX;
            field.ScrY = vf.ScrY;
        }
    }

    auto show_hex = [this](ViewField& vf) {
        if (vf.HexX < 0 || vf.HexY < 0 || vf.HexX >= _maxHexX || vf.HexY >= _maxHexY) {
            return;
        }

        const auto nx = static_cast<ushort>(vf.HexX);
        const auto ny = static_cast<ushort>(vf.HexY);

        if (IsHexToDraw(nx, ny)) {
            return;
        }

        auto& field = GetField(nx, ny);
        field.IsView = true;

        // Track
        if (_isShowTrack && (GetHexTrack(nx, ny) != 0)) {
            const auto spr_id = (GetHexTrack(nx, ny) == 1 ? _picTrack1->GetCurSprId(_engine->GameTime.GameTick()) : _picTrack2->GetCurSprId(_engine->GameTime.GameTick()));
            const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
            auto& spr = _mainTree.InsertSprite(DRAW_ORDER_TRACK, nx, ny, _engine->Settings.MapHexWidth / 2, (_engine->Settings.MapHexHeight / 2) + (si != nullptr ? si->Height / 2 : 0), &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            field.AddSpriteToChain(&spr);
        }

        // Hex lines
        if (_isShowHex) {
            const auto spr_id = _picHex[0]->GetCurSprId(_engine->GameTime.GameTick());
            const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
            auto& spr = _mainTree.InsertSprite(DRAW_ORDER_HEX_GRID, nx, ny, si != nullptr ? si->Width / 2 : 0, si != nullptr ? si->Height : 0, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
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

                auto& spr = _mainTree.InsertSprite(EvaluateItemDrawOrder(item), nx, ny + item->GetDrawOrderOffsetHexY(), _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
                if (!item->GetIsNoLightInfluence()) {
                    spr.SetLight(item->GetCorner(), _hexLight, _maxHexX, _maxHexY);
                }
                item->SetSprite(&spr);
                field.AddSpriteToChain(&spr);
            }
        }

        // Critters
        auto* cr = field.Crit;
        if (cr != nullptr && _engine->Settings.ShowCrit && cr->Visible) {
            auto& spr = _mainTree.InsertSprite(EvaluateCritterDrawOrder(cr), nx, ny, _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy, &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid);
            spr.SetLight(CornerType::EastWest, _hexLight, _maxHexX, _maxHexY);
            cr->SprDraw = &spr;
            cr->SetSprRect();

            auto contour = 0;
            if (cr->GetId() == _critterContourCrId) {
                contour = _critterContour;
            }
            else if (!cr->IsChosen()) {
                contour = _crittersContour;
            }

            spr.SetContour(contour, cr->GetContourColor());

            field.AddSpriteToChain(&spr);
        }

        // Dead critters
        if (field.DeadCrits != nullptr && _engine->Settings.ShowCrit) {
            for (auto* cr : *field.DeadCrits) {
                if (!cr->Visible) {
                    continue;
                }

                auto& spr = _mainTree.InsertSprite(EvaluateCritterDrawOrder(cr), nx, ny, _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy, &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid);
                spr.SetLight(CornerType::EastWest, _hexLight, _maxHexX, _maxHexY);
                cr->SprDraw = &spr;
                cr->SetSprRect();

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
                    auto& spr = _tilesTree.InsertSprite(DRAW_ORDER_TILE + tile.Layer, nx, ny, tile.OffsX + _engine->Settings.MapTileOffsX, tile.OffsY + _engine->Settings.MapTileOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, tiles[i].IsSelected ? &SelectAlpha : nullptr, &_engine->EffectMngr.Effects.Tile, nullptr);
                    field.AddSpriteToChain(&spr);
                }
                else {
                    auto& spr = _tilesTree.InsertSprite(DRAW_ORDER_TILE + tile.Layer, nx, ny, tile.OffsX + _engine->Settings.MapTileOffsX, tile.OffsY + _engine->Settings.MapTileOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, nullptr, &_engine->EffectMngr.Effects.Tile, nullptr);
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
                    auto& spr = _roofTree.InsertSprite(DRAW_ORDER_TILE + roof.Layer, nx, ny, roof.OffsX + _engine->Settings.MapRoofOffsX, roof.OffsY + _engine->Settings.MapRoofOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, roofs[i].IsSelected ? &SelectAlpha : &_engine->Settings.RoofAlpha, &_engine->EffectMngr.Effects.Roof, nullptr);
                    spr.SetEgg(EGG_ALWAYS);
                    field.AddSpriteToChain(&spr);
                }
                else {
                    auto& spr = _roofTree.InsertSprite(DRAW_ORDER_TILE + roof.Layer, nx, ny, roof.OffsX + _engine->Settings.MapRoofOffsX, roof.OffsY + _engine->Settings.MapRoofOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, &_engine->Settings.RoofAlpha, &_engine->EffectMngr.Effects.Roof, nullptr);
                    spr.SetEgg(EGG_ALWAYS);
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
        cr->SetSprRect();
    }

    // Light
    RealRebuildLight();
    _requestRebuildLight = false;
    _requestRenderLight = true;

    _engine->OnRenderMap.Fire();
}

void MapView::ClearHexLight()
{
    NON_CONST_METHOD_HINT();

    std::memset(_hexLight, 0, _maxHexX * _maxHexY * sizeof(uchar) * 3);
}

void MapView::PrepareLightToDraw()
{
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
            if ((ls.OffsX != nullptr) && (*ls.OffsX != ls.LastOffsX || *ls.OffsY != ls.LastOffsY)) {
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
        FPoint offset(static_cast<float>(_rtScreenOx), static_cast<float>(_rtScreenOy));
        for (uint i = 0; i < _lightPointsCount; i++) {
            _engine->SprMngr.DrawPoints(_lightPoints[i], RenderPrimitiveType::TriangleFan, &_engine->Settings.SpritesZoom, &offset, _engine->EffectMngr.Effects.Light);
        }
        _engine->SprMngr.DrawPoints(_lightSoftPoints, RenderPrimitiveType::TriangleList, &_engine->Settings.SpritesZoom, &offset, _engine->EffectMngr.Effects.Light);
        _engine->SprMngr.PopRenderTarget();
    }
}

void MapView::MarkLight(ushort hx, ushort hy, uint inten)
{
    NON_CONST_METHOD_HINT();

    const auto light = static_cast<int>(inten) * MAX_LIGHT_HEX / MAX_LIGHT_VALUE * _lightCapacity / 100;
    const auto lr = light * _lightProcentR / 100;
    const auto lg = light * _lightProcentG / 100;
    const auto lb = light * _lightProcentB / 100;
    auto* p = &_hexLight[hy * _maxHexX * 3 + hx * 3];

    if (lr > *p) {
        *p = static_cast<uchar>(lr);
    }
    if (lg > *(p + 1)) {
        *(p + 1) = static_cast<uchar>(lg);
    }
    if (lb > *(p + 2)) {
        *(p + 2) = static_cast<uchar>(lb);
    }
}

void MapView::MarkLightEndNeighbor(ushort hx, ushort hy, bool north_south, uint inten)
{
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
                *p = static_cast<uchar>(lr_self);
            }
            if (lg_self > *(p + 1)) {
                *(p + 1) = static_cast<uchar>(lg_self);
            }
            if (lb_self > *(p + 2)) {
                *(p + 2) = static_cast<uchar>(lb_self);
            }
        }
    }
}

void MapView::MarkLightEnd(ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint inten)
{
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

void MapView::MarkLightStep(ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint inten)
{
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

void MapView::TraceLight(ushort from_hx, ushort from_hy, ushort& hx, ushort& hy, int dist, uint inten)
{
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
        curx1_i = static_cast<int>(curx1_f);
        if (curx1_f - static_cast<float>(curx1_i) >= 0.5f) {
            curx1_i++;
        }
        cury1_i = static_cast<int>(cury1_f);
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
            if (ox < 0 || ox >= _maxHexX || GetField(ox, old_cury1_i).Flags.IsNoLight) {
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
            if (oy < 0 || oy >= _maxHexY || GetField(old_curx1_i, oy).Flags.IsNoLight) {
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
        if (curx1_i < 0 || curx1_i >= _maxHexX || cury1_i < 0 || cury1_i >= _maxHexY || GetField(curx1_i, cury1_i).Flags.IsNoLight) {
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

void MapView::ParseLightTriangleFan(LightSource& ls)
{
    // All dirs disabled
    if ((ls.Flags & 0x3F) == 0x3F) {
        return;
    }

    const auto hx = ls.HexX;
    const auto hy = ls.HexY;

    // Distance
    const int dist = ls.Distance;
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
        auto color = GenericUtils::GetColorDay(GetMapDayTime(), GetMapDayColor(), GetDayTime(), &_lightCapacity);
        UNUSED_VARIABLE(color);
    }
    else if (ls.Intensity >= 0) {
        auto color = GenericUtils::GetColorDay(GetMapDayTime(), GetMapDayColor(), GetMapTime(), &_lightCapacity);
        UNUSED_VARIABLE(color);
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
        _lightPoints.push_back(vector<PrimitivePoint>());
    }
    auto& points = _lightPoints[_lightPointsCount - 1];
    points.clear();
    points.reserve(3 + dist * _engine->Settings.MapDirCount);
    points.push_back({base_x, base_y, color, ls.OffsX, ls.OffsY}); // Center of light

    int hx_far = hx;
    int hy_far = hy;
    auto seek_start = true;
    ushort last_hx = -1;
    ushort last_hy = -1;

    for (auto i = 0, ii = (_engine->Settings.MapHexagonal ? 6 : 4); i < ii; i++) {
        const auto dir = static_cast<uchar>(_engine->Settings.MapHexagonal ? (i + 2) % 6 : ((i + 1) * 2) % 8);

        for (auto j = 0, jj = (_engine->Settings.MapHexagonal ? dist : dist * 2); j < jj; j++) {
            if (seek_start) {
                // Move to start position
                for (auto l = 0; l < dist; l++) {
                    _engine->Geometry.MoveHexByDirUnsafe(hx_far, hy_far, _engine->Settings.MapHexagonal ? 0 : 7);
                }
                seek_start = false;
                j = -1;
            }
            else {
                // Move to next hex
                _engine->Geometry.MoveHexByDirUnsafe(hx_far, hy_far, dir);
            }

            auto hx_ = static_cast<ushort>(std::clamp(hx_far, 0, _maxHexX - 1));
            auto hy_ = static_cast<ushort>(std::clamp(hy_far, 0, _maxHexY - 1));
            if (j >= 0 && IsBitSet(ls.Flags, LIGHT_DISABLE_DIR(i))) {
                hx_ = hx;
                hy_ = hy;
            }
            else {
                TraceLight(hx, hy, hx_, hy_, dist, inten);
            }

            if (hx_ != last_hx || hy_ != last_hy) {
                short* ox = nullptr;
                short* oy = nullptr;
                if (static_cast<int>(hx_) != hx_far || static_cast<int>(hy_) != hy_far) {
                    int a = alpha - _engine->Geometry.DistGame(hx, hy, hx_, hy_) * alpha / dist;
                    a = std::clamp(a, 0, alpha);
                    color = COLOR_RGBA(a, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
                }
                else {
                    color = COLOR_RGBA(0, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
                    ox = ls.OffsX;
                    oy = ls.OffsY;
                }
                const auto [x, y] = _engine->Geometry.GetHexInterval(hx, hy, hx_, hy_);
                points.push_back({base_x + x, base_y + y, color, ox, oy});
                last_hx = hx_;
                last_hy = hy_;
            }
        }
    }

    for (uint i = 1, j = static_cast<uint>(points.size()); i < j; i++) {
        auto& cur = points[i];
        auto& next = points[i >= points.size() - 1 ? 1 : i + 1];
        if (GenericUtils::DistSqrt(cur.PointX, cur.PointY, next.PointX, next.PointY) > static_cast<uint>(_engine->Settings.MapHexWidth)) {
            const auto dist_comp = (GenericUtils::DistSqrt(base_x, base_y, cur.PointX, cur.PointY) > GenericUtils::DistSqrt(base_x, base_y, next.PointX, next.PointY));
            _lightSoftPoints.push_back({next.PointX, next.PointY, next.PointColor, next.PointOffsX, next.PointOffsY});
            _lightSoftPoints.push_back({cur.PointX, cur.PointY, cur.PointColor, cur.PointOffsX, cur.PointOffsY});
            auto x = static_cast<float>(dist_comp ? next.PointX - cur.PointX : cur.PointX - next.PointX);
            auto y = static_cast<float>(dist_comp ? next.PointY - cur.PointY : cur.PointY - next.PointY);
            std::tie(x, y) = GenericUtils::ChangeStepsCoords(x, y, dist_comp ? -2.5f : 2.5f);
            if (dist_comp) {
                _lightSoftPoints.push_back({cur.PointX + static_cast<int>(x), cur.PointY + static_cast<int>(y), cur.PointColor, cur.PointOffsX, cur.PointOffsY});
            }
            else {
                _lightSoftPoints.push_back({next.PointX + static_cast<int>(x), next.PointY + static_cast<int>(y), next.PointColor, next.PointOffsX, next.PointOffsY});
            }
        }
    }
}

void MapView::RealRebuildLight()
{
    RUNTIME_ASSERT(_viewField);

    _lightPointsCount = 0;
    _lightSoftPoints.clear();
    ClearHexLight();
    CollectLightSources();

    _lightMinHx = _viewField[0].HexX;
    _lightMaxHx = _viewField[_hVisible * _wVisible - 1].HexX;
    _lightMinHy = _viewField[_wVisible - 1].HexY;
    _lightMaxHy = _viewField[_hVisible * _wVisible - _wVisible].HexY;

    for (auto& ls : _lightSources) {
        // Todo: optimize lighting rebuilding to skip unvisible lights
        // if( (int) ls.HexX < lightMinHx - (int) ls.Distance || (int) ls.HexX > lightMaxHx + (int) ls.Distance ||
        //    (int) ls.HexY < lightMinHy - (int) ls.Distance || (int) ls.HexY > lightMaxHy + (int) ls.Distance )
        //    continue;
        ParseLightTriangleFan(ls);
    }
}

void MapView::CollectLightSources()
{
    _lightSources.clear();

    // Scenery
    if (_mapperMode) {
        for (const auto* item : _items) {
            if (item->IsStatic() && item->GetIsLight()) {
                _lightSources.push_back({item->GetHexX(), item->GetHexY(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity()});
            }
        }
    }
    else {
        _lightSources = _lightSourcesScen;
    }

    // Items on ground
    for (const auto* item : _items) {
        if (!item->IsStatic() && item->GetIsLight()) {
            _lightSources.push_back({item->GetHexX(), item->GetHexY(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity()});
        }
    }

    // Items in critters slots
    for (auto* cr : _critters) {
        auto added = false;
        for (const auto* inv_item : cr->InvItems) {
            if (inv_item->GetIsLight() && inv_item->GetCritSlot() != 0u) {
                _lightSources.push_back({cr->GetHexX(), cr->GetHexY(), inv_item->GetLightColor(), inv_item->GetLightDistance(), inv_item->GetLightFlags(), inv_item->GetLightIntensity(), &cr->SprOx, &cr->SprOy});
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

auto MapView::ProcessTileBorder(Field::Tile& tile, bool is_roof) -> bool
{
    const auto ox = (is_roof ? _engine->Settings.MapRoofOffsX : _engine->Settings.MapTileOffsX) + tile.OffsX;
    const auto oy = (is_roof ? _engine->Settings.MapRoofOffsY : _engine->Settings.MapTileOffsY) + tile.OffsY;
    return ProcessHexBorders(tile.Anim->GetSprId(0), ox, oy, false);
}

void MapView::RebuildTiles()
{
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

            const auto hx = static_cast<ushort>(hxi);
            const auto hy = static_cast<ushort>(hyi);

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
                    auto& spr = _tilesTree.AddSprite(DRAW_ORDER_TILE + tile.Layer, hx, hy, tile.OffsX + _engine->Settings.MapTileOffsX, tile.OffsY + _engine->Settings.MapTileOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, tiles[i].IsSelected ? &SelectAlpha : nullptr, &_engine->EffectMngr.Effects.Tile, nullptr);
                    field.AddSpriteToChain(&spr);
                }
                else {
                    auto& spr = _tilesTree.AddSprite(DRAW_ORDER_TILE + tile.Layer, hx, hy, tile.OffsX + _engine->Settings.MapTileOffsX, tile.OffsY + _engine->Settings.MapTileOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, nullptr, &_engine->EffectMngr.Effects.Tile, nullptr);
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

            const auto hx = static_cast<ushort>(hxi);
            const auto hy = static_cast<ushort>(hyi);

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
                        auto& spr = _roofTree.AddSprite(DRAW_ORDER_TILE + roof.Layer, hx, hy, roof.OffsX + _engine->Settings.MapRoofOffsX, roof.OffsY + _engine->Settings.MapRoofOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, roofs[i].IsSelected ? &SelectAlpha : &_engine->Settings.RoofAlpha, &_engine->EffectMngr.Effects.Roof, nullptr);
                        spr.SetEgg(EGG_ALWAYS);
                        field.AddSpriteToChain(&spr);
                    }
                    else {
                        auto& spr = _roofTree.AddSprite(DRAW_ORDER_TILE + roof.Layer, hx, hy, roof.OffsX + _engine->Settings.MapRoofOffsX, roof.OffsY + _engine->Settings.MapRoofOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, &_engine->Settings.RoofAlpha, &_engine->EffectMngr.Effects.Roof, nullptr);
                        spr.SetEgg(EGG_ALWAYS);
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

void MapView::SetSkipRoof(ushort hx, ushort hy)
{
    if (_roofSkip != GetField(hx, hy).RoofNum) {
        _roofSkip = GetField(hx, hy).RoofNum;
        RebuildRoof();
    }
}

void MapView::MarkRoofNum(int hxi, int hyi, short num)
{
    if (hxi < 0 || hyi < 0 || hxi >= _maxHexX || hyi >= _maxHexY) {
        return;
    }

    const auto hx = static_cast<ushort>(hxi);
    const auto hy = static_cast<ushort>(hyi);
    if (GetField(hx, hy).GetTilesCount(true) == 0u) {
        return;
    }
    if (GetField(hx, hy).RoofNum != 0) {
        return;
    }

    for (auto x = 0; x < _engine->Settings.MapRoofSkipSize; x++) {
        for (auto y = 0; y < _engine->Settings.MapRoofSkipSize; y++) {
            if (hxi + x >= 0 && hxi + x < _maxHexX && hyi + y >= 0 && hyi + y < _maxHexY) {
                GetField(static_cast<ushort>(hxi + x), static_cast<ushort>(hyi + y)).RoofNum = num;
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
    if (spr_id == 0u) {
        return false;
    }

    const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
    if (si == nullptr) {
        return false;
    }

    const auto top = oy + si->OffsY - si->Height - (_engine->Settings.MapHexLineHeight * 2);
    const auto bottom = oy + si->OffsY + (_engine->Settings.MapHexLineHeight * 2);
    const auto left = ox + si->OffsX - si->Width / 2 - _engine->Settings.MapHexWidth;
    const auto right = ox + si->OffsX + si->Width / 2 + _engine->Settings.MapHexWidth;
    const auto zoomed_screen_height = static_cast<int>(std::ceil(static_cast<float>(_engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight) * _engine->Settings.SpritesZoom));
    const auto zoomed_screen_width = static_cast<int>(std::ceil(static_cast<float>(_engine->Settings.ScreenWidth) * _engine->Settings.SpritesZoom));
    return !(top > zoomed_screen_height || bottom < 0 || left > zoomed_screen_width || right < 0);
}

void MapView::ProcessHexBorders(ItemHexView* item)
{
    ProcessHexBorders(item->Anim->GetSprId(0), item->GetOffsetX(), item->GetOffsetY(), true);
}

auto MapView::ProcessHexBorders(uint spr_id, int ox, int oy, bool resize_map) -> bool
{
    const auto* si = _engine->SprMngr.GetSpriteInfo(spr_id);
    if (si == nullptr) {
        return false;
    }

    auto top = si->OffsY + oy - _hTop * _engine->Settings.MapHexLineHeight + (_engine->Settings.MapHexLineHeight * 2);
    if (top < 0) {
        top = 0;
    }

    auto bottom = si->Height - si->OffsY - oy - _hBottom * _engine->Settings.MapHexLineHeight + (_engine->Settings.MapHexLineHeight * 2);
    if (bottom < 0) {
        bottom = 0;
    }

    auto left = si->Width / 2 + si->OffsX + ox - _wLeft * _engine->Settings.MapHexWidth + _engine->Settings.MapHexWidth;
    if (left < 0) {
        left = 0;
    }

    auto right = si->Width / 2 - si->OffsX - ox - _wRight * _engine->Settings.MapHexWidth + _engine->Settings.MapHexWidth;
    if (right < 0) {
        right = 0;
    }

    if ((top != 0) || (bottom != 0) || (left != 0) || (right != 0)) {
        // Resize
        _hTop += top / _engine->Settings.MapHexLineHeight + ((top % _engine->Settings.MapHexLineHeight) != 0 ? 1 : 0);
        _hBottom += bottom / _engine->Settings.MapHexLineHeight + ((bottom % _engine->Settings.MapHexLineHeight) != 0 ? 1 : 0);
        _wLeft += left / _engine->Settings.MapHexWidth + ((left % _engine->Settings.MapHexWidth) != 0 ? 1 : 0);
        _wRight += right / _engine->Settings.MapHexWidth + ((right % _engine->Settings.MapHexWidth) != 0 ? 1 : 0);

        if (resize_map) {
            ResizeView();
            RefreshMap();
        }
        return true;
    }
    return false;
}

void MapView::SwitchShowHex()
{
    _isShowHex = !_isShowHex;
    RefreshMap();
}

void MapView::ResizeField(ushort w, ushort h)
{
    _maxHexX = w;
    _maxHexY = h;

    delete[] _hexField;
    _hexField = nullptr;
    delete[] _hexTrack;
    _hexTrack = nullptr;
    delete[] _hexLight;
    _hexLight = nullptr;

    TilesField.clear();
    RoofsField.clear();

    if (w == 0u || h == 0u) {
        return;
    }

    const auto size = static_cast<size_t>(w) * static_cast<size_t>(h);
    _hexField = new Field[size];
    _hexTrack = new char[size];
    std::memset(_hexTrack, 0, size * sizeof(char));
    _hexLight = new uchar[size * 3u];
    std::memset(_hexLight, 0, size * 3u * sizeof(uchar));
    TilesField.resize(size);
    RoofsField.resize(size);
}

void MapView::ClearHexTrack()
{
    NON_CONST_METHOD_HINT();

    std::memset(_hexTrack, 0, _maxHexX * _maxHexY * sizeof(char));
}

void MapView::SwitchShowTrack()
{
    _isShowTrack = !_isShowTrack;

    if (!_isShowTrack) {
        ClearHexTrack();
    }

    RefreshMap();
}

void MapView::InitView(int cx, int cy)
{
    NON_CONST_METHOD_HINT();

    if (_engine->Settings.MapHexagonal) {
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

        auto xa = -(_wRight * _engine->Settings.MapHexWidth);
        auto xb = -(_engine->Settings.MapHexWidth / 2) - (_wRight * _engine->Settings.MapHexWidth);
        auto oy = -_engine->Settings.MapHexLineHeight * _hTop;
        const auto wx = static_cast<int>(static_cast<float>(_engine->Settings.ScreenWidth) * _engine->Settings.SpritesZoom);

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
        const auto wx = static_cast<int>(static_cast<float>(_engine->Settings.ScreenWidth) * _engine->Settings.SpritesZoom);

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
    if (_viewField != nullptr) {
        for (auto i = 0, j = _hVisible * _wVisible; i < j; i++) {
            auto& vf = _viewField[i];
            if (vf.HexX >= 0 && vf.HexY >= 0 && vf.HexX < _maxHexX && vf.HexY < _maxHexY) {
                auto& field = GetField(static_cast<ushort>(vf.HexX), static_cast<ushort>(vf.HexY));
                field.IsView = false;
                field.UnvalidateSpriteChain();
            }
        }
    }

    _hVisible = GetViewHeight() + _hTop + _hBottom;
    _wVisible = GetViewWidth() + _wLeft + _wRight;

    delete[] _viewField;
    _viewField = new ViewField[_hVisible * _wVisible];
}

void MapView::ChangeZoom(int zoom)
{
    if (_engine->Settings.SpritesZoomMin == _engine->Settings.SpritesZoomMax) {
        return;
    }
    if (zoom == 0 && _engine->Settings.SpritesZoom == 1.0f) {
        return;
    }
    if (zoom > 0 && _engine->Settings.SpritesZoom >= std::min(_engine->Settings.SpritesZoomMax, MAX_ZOOM)) {
        return;
    }
    if (zoom < 0 && _engine->Settings.SpritesZoom <= std::max(_engine->Settings.SpritesZoomMin, MIN_ZOOM)) {
        return;
    }

    // Check screen blockers
    if (_engine->Settings.ScrollCheck && (zoom > 0 || (zoom == 0 && _engine->Settings.SpritesZoom < 1.0f))) {
        for (auto x = -1; x <= 1; x++) {
            for (auto y = -1; y <= 1; y++) {
                if (((x != 0) || (y != 0)) && ScrollCheck(x, y)) {
                    return;
                }
            }
        }
    }

    if (zoom != 0 || _engine->Settings.SpritesZoom < 1.0f) {
        const auto old_zoom = _engine->Settings.SpritesZoom;
        const auto w = static_cast<float>(_engine->Settings.ScreenWidth / _engine->Settings.MapHexWidth + ((_engine->Settings.ScreenWidth % _engine->Settings.MapHexWidth) != 0 ? 1 : 0));
        _engine->Settings.SpritesZoom = (w * _engine->Settings.SpritesZoom + (zoom >= 0 ? 2.0f : -2.0f)) / w;

        if (_engine->Settings.SpritesZoom < std::max(_engine->Settings.SpritesZoomMin, MIN_ZOOM) || _engine->Settings.SpritesZoom > std::min(_engine->Settings.SpritesZoomMax, MAX_ZOOM)) {
            _engine->Settings.SpritesZoom = old_zoom;
            return;
        }
    }
    else {
        _engine->Settings.SpritesZoom = 1.0f;
    }

    ResizeView();
    RefreshMap();

    if (zoom == 0 && _engine->Settings.SpritesZoom != 1.0f) {
        ChangeZoom(0);
    }
}

void MapView::GetScreenHexes(int& sx, int& sy) const
{
    sx = _screenHexX;
    sy = _screenHexY;
}

void MapView::GetHexCurrentPosition(ushort hx, ushort hy, int& x, int& y) const
{
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
    // Prepare light
    PrepareLightToDraw();

    // Prepare fog
    PrepareFogToDraw();

    // Prerendered offsets
    const auto ox = static_cast<int>(_rtScreenOx) - static_cast<int>(std::round(static_cast<float>(_engine->Settings.ScrOx) / _engine->Settings.SpritesZoom));
    const auto oy = static_cast<int>(_rtScreenOy) - static_cast<int>(std::round(static_cast<float>(_engine->Settings.ScrOy) / _engine->Settings.SpritesZoom));
    const auto prerendered_rect = IRect(ox, oy, ox + _engine->Settings.ScreenWidth, oy + (_engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight));

    // Separate render target
    if (_rtMap != nullptr) {
        _engine->SprMngr.PushRenderTarget(_rtMap);
        _engine->SprMngr.ClearCurrentRenderTarget(0);
    }

    // Tiles
    if (_engine->Settings.ShowTile) {
        _engine->SprMngr.DrawSprites(_tilesTree, false, false, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END, false, 0, 0);
    }

    // Flat sprites
    _engine->SprMngr.DrawSprites(_mainTree, true, false, DRAW_ORDER_FLAT, DRAW_ORDER_LIGHT - 1, false, 0, 0);

    // Light
    if (_rtLight != nullptr) {
        _engine->SprMngr.DrawRenderTarget(_rtLight, true, &prerendered_rect, nullptr);
    }

    // Cursor flat
    if (_cursorPrePic != nullptr) {
        DrawCursor(_cursorPrePic->GetCurSprId(_engine->GameTime.GameTick()));
    }

    // Sprites
    _engine->SprMngr.DrawSprites(_mainTree, true, true, DRAW_ORDER_LIGHT, DRAW_ORDER_LAST, false, 0, 0);

    // Roof
    if (_engine->Settings.ShowRoof) {
        _engine->SprMngr.DrawSprites(_roofTree, false, true, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END, false, 0, 0);
    }

    // Contours
    _engine->SprMngr.DrawContours();

    // Fog
    if (!_mapperMode) {
        _engine->SprMngr.DrawRenderTarget(_rtFog, true, &prerendered_rect, nullptr);
    }

    // Cursor
    if (_cursorPostPic != nullptr) {
        DrawCursor(_cursorPostPic->GetCurSprId(_engine->GameTime.GameTick()));
    }

    if (_cursorXPic != nullptr) {
        if (_drawCursorX < 0) {
            DrawCursor(_cursorXPic->GetCurSprId(_engine->GameTime.GameTick()));
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
        _engine->SprMngr.DrawRenderTarget(_rtMap, false, nullptr, nullptr);
    }
}

void MapView::DrawMapTexts()
{
    for (auto* cr : _critters) {
        cr->DrawTextOnHead();
    }

    if (_mapperMode) {
        // Texts on map
        const auto tick = _engine->GameTime.FrameTick();
        for (auto it = _mapTexts.begin(); it != _mapTexts.end();) {
            auto& map_text = *it;

            if (tick >= map_text.StartTick + map_text.Tick) {
                it = _mapTexts.erase(it);
            }
            else {
                const auto percent = GenericUtils::Percent(map_text.Tick, tick - map_text.StartTick);
                const auto text_pos = map_text.Pos.Interpolate(map_text.EndPos, percent);
                const auto& field = GetField(map_text.HexX, map_text.HexY);

                const auto x = static_cast<int>((field.ScrX + _engine->Settings.MapHexWidth / 2 + _engine->Settings.ScrOx) / _engine->Settings.SpritesZoom - 100.0f - static_cast<float>(map_text.Pos.Left - text_pos.Left));
                const auto y = static_cast<int>((field.ScrY + _engine->Settings.MapHexLineHeight / 2 - map_text.Pos.Height() - (map_text.Pos.Top - text_pos.Top) + _engine->Settings.ScrOy) / _engine->Settings.SpritesZoom - 70.0f);

                auto color = map_text.Color;
                if (map_text.Fade) {
                    color = (color ^ 0xFF000000) | ((0xFF * (100 - percent) / 100) << 24);
                }

                _engine->SprMngr.DrawStr(IRect(x, y, x + 200, y + 70), map_text.Text, FT_CENTERX | FT_BOTTOM | FT_BORDERED, color, FONT_DEFAULT);

                ++it;
            }
        }
    }
    else {
        // Texts on map
        const auto tick = _engine->GameTime.GameTick();
        for (auto it = _mapTexts.begin(); it != _mapTexts.end();) {
            auto& mt = *it;
            if (tick < mt.StartTick + mt.Tick) {
                const auto dt = tick - mt.StartTick;
                const auto percent = GenericUtils::Percent(mt.Tick, dt);
                const auto r = mt.Pos.Interpolate(mt.EndPos, percent);
                auto& f = GetField(mt.HexX, mt.HexY);
                const auto half_hex_width = _engine->Settings.MapHexWidth / 2;
                const auto half_hex_height = _engine->Settings.MapHexHeight / 2;
                const auto x = static_cast<int>(static_cast<float>(f.ScrX + half_hex_width + _engine->Settings.ScrOx) / _engine->Settings.SpritesZoom - 100.0f - static_cast<float>(mt.Pos.Left - r.Left));
                const auto y = static_cast<int>(static_cast<float>(f.ScrY + half_hex_height - mt.Pos.Height() - (mt.Pos.Top - r.Top) + _engine->Settings.ScrOy) / _engine->Settings.SpritesZoom - 70.0f);

                auto color = mt.Color;
                if (mt.Fade) {
                    color = (color ^ 0xFF000000) | ((0xFF * (100 - percent) / 100) << 24);
                }
                else if (mt.Tick > 500) {
                    const auto hide = mt.Tick - 200;
                    if (dt >= hide) {
                        const auto alpha = 255u * (100u - GenericUtils::Percent(mt.Tick - hide, dt - hide)) / 100u;
                        color = (alpha << 24) | (color & 0xFFFFFF);
                    }
                }

                _engine->SprMngr.DrawStr(IRect(x, y, x + 200, y + 70), mt.Text, FT_CENTERX | FT_BOTTOM | FT_BORDERED, color, FONT_DEFAULT);

                ++it;
            }
            else {
                it = _mapTexts.erase(it);
            }
        }
    }
}

void MapView::SetFog(vector<PrimitivePoint>& look_points, vector<PrimitivePoint>& shoot_points, short* offs_x, short* offs_y)
{
    _fogOffsX = offs_x;
    _fogOffsY = offs_y;
    _fogLastOffsX = static_cast<short>(_fogOffsX != nullptr ? *_fogOffsX : 0);
    _fogLastOffsY = static_cast<short>(_fogOffsY != nullptr ? *_fogOffsY : 0);
    _fogForceRerender = true;
    _fogLookPoints = look_points;
    _fogShootPoints = shoot_points;
}

void MapView::PrepareFogToDraw()
{
    if (!_mapperMode) {
        return;
    }

    if (!_fogForceRerender && (_fogOffsX != nullptr && *_fogOffsX == _fogLastOffsX && *_fogOffsY == _fogLastOffsY)) {
        return;
    }
    if (_fogOffsX != nullptr) {
        _fogLastOffsX = *_fogOffsX;
        _fogLastOffsY = *_fogOffsY;
    }
    _fogForceRerender = false;

    FPoint offset(static_cast<float>(_rtScreenOx), static_cast<float>(_rtScreenOy));
    _engine->SprMngr.PushRenderTarget(_rtFog);
    _engine->SprMngr.ClearCurrentRenderTarget(0);
    _engine->SprMngr.DrawPoints(_fogLookPoints, RenderPrimitiveType::TriangleFan, &_engine->Settings.SpritesZoom, &offset, _engine->EffectMngr.Effects.Fog);
    _engine->SprMngr.DrawPoints(_fogShootPoints, RenderPrimitiveType::TriangleFan, &_engine->Settings.SpritesZoom, &offset, _engine->EffectMngr.Effects.Fog);
    _engine->SprMngr.PopRenderTarget();
}

auto MapView::Scroll() -> bool
{
    // Scroll delay
    auto time_k = 1.0f;
    if (_engine->Settings.ScrollDelay != 0u) {
        const auto tick = _engine->GameTime.FrameTick();
        static auto last_tick = tick;
        if (tick - last_tick < _engine->Settings.ScrollDelay / 2) {
            return false;
        }
        time_k = static_cast<float>(tick - last_tick) / static_cast<float>(_engine->Settings.ScrollDelay);
        last_tick = tick;
    }

    const auto is_scroll = (_engine->Settings.ScrollMouseLeft || _engine->Settings.ScrollKeybLeft || _engine->Settings.ScrollMouseRight || _engine->Settings.ScrollKeybRight || _engine->Settings.ScrollMouseUp || _engine->Settings.ScrollKeybUp || _engine->Settings.ScrollMouseDown || _engine->Settings.ScrollKeybDown);
    auto scr_ox = _engine->Settings.ScrOx;
    auto scr_oy = _engine->Settings.ScrOy;
    const auto prev_scr_ox = scr_ox;
    const auto prev_scr_oy = scr_oy;

    if (is_scroll && AutoScroll.CanStop) {
        AutoScroll.Active = false;
    }

    // Check critter scroll lock
    if ((AutoScroll.HardLockedCritter != 0u) && !is_scroll) {
        auto* cr = GetCritter(AutoScroll.HardLockedCritter);
        if ((cr != nullptr) && (cr->GetHexX() != _screenHexX || cr->GetHexY() != _screenHexY)) {
            ScrollToHex(cr->GetHexX(), cr->GetHexY(), 0.02f, true);
        }
    }

    if ((AutoScroll.SoftLockedCritter != 0u) && !is_scroll) {
        auto* cr = GetCritter(AutoScroll.SoftLockedCritter);
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

        xscroll = static_cast<int>(AutoScroll.OffsXStep);
        yscroll = static_cast<int>(AutoScroll.OffsYStep);

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

        if (GenericUtils::DistSqrt(0, 0, static_cast<int>(AutoScroll.OffsX), static_cast<int>(AutoScroll.OffsY)) == 0u) {
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

        xscroll = static_cast<int>(static_cast<float>(xscroll * _engine->Settings.ScrollStep) * _engine->Settings.SpritesZoom * time_k);
        yscroll = static_cast<int>(static_cast<float>(yscroll * (_engine->Settings.ScrollStep * (_engine->Settings.MapHexLineHeight * 2) / _engine->Settings.MapHexWidth)) * _engine->Settings.SpritesZoom * time_k);
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
    const auto max_pos = _wVisible * _hVisible;
    for (auto pos : positions) {
        if (pos < 0 || pos >= max_pos) {
            return true;
        }

        if (_viewField[pos].HexX < 0 || _viewField[pos].HexY < 0 || _viewField[pos].HexX >= _maxHexX || _viewField[pos].HexY >= _maxHexY) {
            return true;
        }

        auto hx = static_cast<ushort>(_viewField[pos].HexX);
        auto hy = static_cast<ushort>(_viewField[pos].HexY);

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
    if (!_engine->Settings.MapHexagonal) {
        dirs[0] = 7; // Square
        dirs[1] = -1;
        dirs[2] = 3;
        dirs[3] = -1;
        dirs[4] = 5;
        dirs[5] = -1;
        dirs[6] = 1;
        dirs[7] = -1;
    }

    if (_engine->Settings.MapHexagonal) {
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
    if (_engine->Settings.SpritesZoom != 1.0f) {
        for (auto& i : positions_left) {
            i--;
        }
        for (auto& i : positions_right) {
            i++;
        }

        if (_engine->Settings.MapHexagonal) {
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
    auto sx = 0;
    auto sy = 0;
    GetScreenHexes(sx, sy);

    const auto [ox, oy] = _engine->Geometry.GetHexInterval(sx, sy, hx, hy);

    AutoScroll.Active = false;
    ScrollOffset(ox, oy, speed, can_stop);
}

void MapView::ScrollOffset(int ox, int oy, float speed, bool can_stop)
{
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
    const auto hx = cr->GetHexX();
    const auto hy = cr->GetHexY();
    auto& field = GetField(hx, hy);

    if (cr->IsDead()) {
        field.AddDeadCrit(cr);
    }
    else {
        if (field.Crit != nullptr && field.Crit != cr) {
            WriteLog("Hex {} {} busy, critter old {}, new {}", hx, hy, field.Crit->GetId(), cr->GetId());
            return;
        }

        field.Crit = cr;
        SetMultihex(cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), true);
    }

    if (IsHexToDraw(hx, hy) && cr->Visible) {
        auto& spr = _mainTree.InsertSprite(EvaluateCritterDrawOrder(cr), hx, hy, _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy, &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid);
        spr.SetLight(CornerType::EastWest, _hexLight, _maxHexX, _maxHexY);
        cr->SprDraw = &spr;
        cr->SetSprRect();

        auto contour = 0;
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
    const auto hx = cr->GetHexX();
    const auto hy = cr->GetHexY();

    auto& field = GetField(hx, hy);
    if (field.Crit == cr) {
        field.Crit = nullptr;
        SetMultihex(cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), false);
    }
    else {
        field.EraseDeadCrit(cr);
    }

    if (cr->IsChosen() || cr->IsHaveLightSources()) {
        RebuildLight();
    }
    if (cr->SprDrawValid) {
        cr->SprDraw->Unvalidate();
    }

    field.ProcessCache();
}

auto MapView::GetCritter(uint id) -> CritterHexView*
{
    if (id == 0u) {
        return nullptr;
    }

    const auto it = _crittersMap.find(id);
    return it != _crittersMap.end() ? it->second : nullptr;
}

auto MapView::AddCritter(uint id, const ProtoCritter* proto, const map<string, string>& props_kv) -> CritterHexView*
{
    if (id != 0u && _crittersMap.count(id) != 0u) {
        return nullptr;
    }

    auto* cr = new CritterHexView(this, id, proto);
    if (!cr->LoadFromText(props_kv)) {
        cr->Release();
        return nullptr;
    }

    AddCritter(cr);
    return cr;
}

auto MapView::AddCritter(uint id, const ProtoCritter* proto, ushort hx, ushort hy, const vector<vector<uchar>>& data) -> CritterHexView*
{
    auto* cr = new CritterHexView(this, id, proto);
    cr->RestoreData(data);
    cr->SetHexX(hx);
    cr->SetHexY(hy);

    AddCritter(cr);
    return cr;
}

void MapView::AddCritter(CritterHexView* cr)
{
    uint fading_tick = 0u;

    if (cr->GetId() != 0u) {
        if (auto* prev_cr = GetCritter(cr->GetId()); prev_cr != nullptr) {
            fading_tick = prev_cr->FadingTick;
            DestroyCritter(prev_cr);
        }

        _crittersMap.emplace(cr->GetId(), cr);
    }

    _critters.emplace_back(cr);

    AddCritterToField(cr);

    cr->Init();

    const auto game_tick = _engine->GameTime.GameTick();
    cr->FadingTick = game_tick + FADING_PERIOD - (fading_tick > game_tick ? fading_tick - game_tick : 0);
}

void MapView::DestroyCritter(CritterHexView* cr)
{
    RUNTIME_ASSERT(cr->GetMap() == this);

    const auto it = std::find(_critters.begin(), _critters.end(), cr);
    RUNTIME_ASSERT(it != _critters.end());
    _critters.erase(it);

    if (cr->GetId() != 0u) {
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
    return _critters;
}

auto MapView::GetCritters(ushort hx, ushort hy, CritterFindType find_type) -> vector<CritterHexView*>
{
    vector<CritterHexView*> crits;
    auto& field = GetField(hx, hy);

    if (field.Crit != nullptr && field.Crit->CheckFind(find_type)) {
        crits.push_back(field.Crit);
    }

    if (field.DeadCrits != nullptr) {
        for (auto* cr : *field.DeadCrits) {
            if (cr->CheckFind(find_type)) {
                crits.push_back(cr);
            }
        }
    }

    return crits;
}

void MapView::SetCritterContour(uint crid, int contour)
{
    if (_critterContourCrId != 0u) {
        auto* cr = GetCritter(_critterContourCrId);
        if ((cr != nullptr) && cr->SprDrawValid) {
            if (!cr->IsDead() && !cr->IsChosen()) {
                cr->SprDraw->SetContour(_crittersContour);
            }
            else {
                cr->SprDraw->SetContour(0);
            }
        }
    }

    _critterContourCrId = crid;
    _critterContour = contour;

    if (crid != 0u) {
        auto* cr = GetCritter(crid);
        if (cr != nullptr && cr->SprDrawValid) {
            cr->SprDraw->SetContour(contour);
        }
    }
}

void MapView::SetCrittersContour(int contour)
{
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

void MapView::MoveCritter(CritterHexView* cr, ushort hx, ushort hy)
{
    RUNTIME_ASSERT(cr->GetMap() == this);

    if (!cr->IsDead() && GetField(hx, hy).Crit != nullptr) {
        DestroyCritter(GetField(hx, hy).Crit);
    }

    RemoveCritterFromField(cr);
    cr->SetHexX(hx);
    cr->SetHexY(hy);
    AddCritterToField(cr);
}

auto MapView::TransitCritter(CritterHexView* cr, ushort hx, ushort hy, bool smoothly) -> bool
{
    RUNTIME_ASSERT(cr->GetMap() == this);

    if (hx >= _maxHexX || hy >= _maxHexY) {
        return false;
    }
    if (cr->GetHexX() == hx && cr->GetHexY() == hy) {
        return true;
    }

    // Dead transit
    if (cr->IsDead()) {
        RemoveCritterFromField(cr);
        cr->SetHexX(hx);
        cr->SetHexY(hy);
        AddCritterToField(cr);

        if (cr->IsChosen() || cr->IsHaveLightSources()) {
            RebuildLight();
        }
        return true;
    }

    // Hex busy
    if (GetField(hx, hy).Crit != nullptr) {
        return false;
    }

    RemoveCritterFromField(cr);

    const auto old_hx = cr->GetHexX();
    const auto old_hy = cr->GetHexY();

    cr->SetHexX(hx);
    cr->SetHexY(hy);

    if (smoothly) {
        auto&& [ox, oy] = _engine->Geometry.GetHexInterval(hx, hy, old_hx, old_hy);
        cr->AddExtraOffs(ox, oy);
    }

    AddCritterToField(cr);
    return true;
}

void MapView::SetMultihex(ushort hx, ushort hy, uint multihex, bool set)
{
    if (multihex != 0u) {
        auto&& [sx, sy] = _engine->Geometry.GetHexOffsets((hx % 2) != 0);

        for (uint i = 0, j = GenericUtils::NumericalNumber(multihex) * _engine->Settings.MapDirCount; i < j; i++) {
            const auto cx = static_cast<short>(hx) + sx[i];
            const auto cy = static_cast<short>(hy) + sy[i];
            if (cx >= 0 && cy >= 0 && cx < _maxHexX && cy < _maxHexY) {
                auto& neighbor = GetField(static_cast<ushort>(cx), static_cast<ushort>(cy));
                neighbor.Flags.IsMultihex = set;
                neighbor.ProcessCache();
            }
        }
    }
}

auto MapView::GetHexAtScreenPos(int x, int y, ushort& hx, ushort& hy, int* hex_ox, int* hex_oy) const -> bool
{
    const auto xf = static_cast<float>(x) - static_cast<float>(_engine->Settings.ScrOx) / _engine->Settings.SpritesZoom;
    const auto yf = static_cast<float>(y) - static_cast<float>(_engine->Settings.ScrOy) / _engine->Settings.SpritesZoom;
    const auto ox = static_cast<float>(_engine->Settings.MapHexWidth) / _engine->Settings.SpritesZoom;
    const auto oy = static_cast<float>(_engine->Settings.MapHexHeight) / _engine->Settings.SpritesZoom;
    auto y2 = 0;

    for (auto ty = 0; ty < _hVisible; ty++) {
        for (auto tx = 0; tx < _wVisible; tx++) {
            const auto vpos = y2 + tx;
            const auto x_ = _viewField[vpos].ScrXf / _engine->Settings.SpritesZoom;
            const auto y_ = _viewField[vpos].ScrYf / _engine->Settings.SpritesZoom;

            if (xf >= x_ && xf < x_ + ox && yf >= y_ && yf < y_ + oy) {
                auto hx_ = _viewField[vpos].HexX;
                auto hy_ = _viewField[vpos].HexY;

                // Correct with hex color mask
                if (_picHexMask != nullptr) {
                    const auto r = (_engine->SprMngr.GetPixColor(_picHexMask->Ind[0], static_cast<int>(xf - x_), static_cast<int>(yf - y_), true) & 0x00FF0000) >> 16;
                    if (r == 50) {
                        _engine->Geometry.MoveHexByDirUnsafe(hx_, hy_, _engine->Settings.MapHexagonal ? 5u : 6u);
                    }
                    else if (r == 100) {
                        _engine->Geometry.MoveHexByDirUnsafe(hx_, hy_, _engine->Settings.MapHexagonal ? 0u : 0u);
                    }
                    else if (r == 150) {
                        _engine->Geometry.MoveHexByDirUnsafe(hx_, hy_, _engine->Settings.MapHexagonal ? 3u : 4u);
                    }
                    else if (r == 200) {
                        _engine->Geometry.MoveHexByDirUnsafe(hx_, hy_, _engine->Settings.MapHexagonal ? 2u : 2u);
                    }
                }

                if (hx_ >= 0 && hy_ >= 0 && hx_ < _maxHexX && hy_ < _maxHexY) {
                    hx = static_cast<ushort>(hx_);
                    hy = static_cast<ushort>(hy_);

                    if (hex_ox != nullptr && hex_oy != nullptr) {
                        *hex_ox = static_cast<int>(xf - x_) - 16;
                        *hex_oy = static_cast<int>(yf - y_) - 8;
                    }
                    return true;
                }
            }
        }
        y2 += _wVisible;
    }

    return false;
}

auto MapView::GetItemAtScreenPos(int x, int y, bool& item_egg) -> ItemHexView*
{
    NON_CONST_METHOD_HINT();

    vector<ItemHexView*> pix_item;
    vector<ItemHexView*> pix_item_egg;
    const auto is_egg = _engine->SprMngr.IsEggTransp(x, y);

    for (auto* item : _items) {
        const auto hx = item->GetHexX();
        const auto hy = item->GetHexY();

        if (item->IsFinishing() || !item->SprDrawValid) {
            continue;
        }

        if (!_mapperMode) {
            if (item->GetIsHidden() || item->GetIsHiddenPicture() || item->IsFullyTransparent()) {
                continue;
            }
            if (item->IsScenery() && !_engine->Settings.ShowScen) {
                continue;
            }
            if (!item->IsAnyScenery() && !_engine->Settings.ShowItem) {
                continue;
            }
            if (item->IsWall() && !_engine->Settings.ShowWall) {
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

        const auto* si = _engine->SprMngr.GetSpriteInfo(item->SprId);
        if (si == nullptr) {
            continue;
        }

        const auto l = static_cast<int>((*item->HexScrX + item->ScrX + si->OffsX + (_engine->Settings.MapHexWidth / 2) + _engine->Settings.ScrOx - si->Width / 2) / _engine->Settings.SpritesZoom);
        const auto r = static_cast<int>((*item->HexScrX + item->ScrX + si->OffsX + (_engine->Settings.MapHexWidth / 2) + _engine->Settings.ScrOx + si->Width / 2) / _engine->Settings.SpritesZoom);
        const auto t = static_cast<int>((*item->HexScrY + item->ScrY + si->OffsY + (_engine->Settings.MapHexHeight / 2) + _engine->Settings.ScrOy - si->Height) / _engine->Settings.SpritesZoom);
        const auto b = static_cast<int>((*item->HexScrY + item->ScrY + si->OffsY + (_engine->Settings.MapHexHeight / 2) + _engine->Settings.ScrOy) / _engine->Settings.SpritesZoom);

        if (x >= l && x <= r && y >= t && y <= b) {
            auto* spr = item->SprDraw->GetIntersected(x - l, y - t);
            if (spr != nullptr) {
                item->SprTemp = spr;
                if (is_egg && _engine->SprMngr.CompareHexEgg(hx, hy, item->GetEggType())) {
                    pix_item_egg.push_back(item);
                }
                else {
                    pix_item.push_back(item);
                }
            }
        }
    }

    // Sorters
    struct Sorter
    {
        static auto ByTreeIndex(ItemHexView* o1, ItemHexView* o2) -> bool { return o1->SprTemp->TreeIndex > o2->SprTemp->TreeIndex; }
        static auto ByTransparent(ItemHexView* o1, ItemHexView* o2) -> bool { return !o1->IsTransparent() && o2->IsTransparent(); }
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

auto MapView::GetCritterAtScreenPos(int x, int y, bool ignore_dead_and_chosen, bool wide_rangle) -> CritterHexView*
{
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

        const auto l = static_cast<int>(static_cast<float>(cr->DRect.Left + _engine->Settings.ScrOx) / _engine->Settings.SpritesZoom);
        const auto r = static_cast<int>(static_cast<float>(cr->DRect.Right + _engine->Settings.ScrOx) / _engine->Settings.SpritesZoom);
        const auto t = static_cast<int>(static_cast<float>(cr->DRect.Top + _engine->Settings.ScrOy) / _engine->Settings.SpritesZoom);
        const auto b = static_cast<int>(static_cast<float>(cr->DRect.Bottom + _engine->Settings.ScrOy) / _engine->Settings.SpritesZoom);

        if (wide_rangle) {
            const auto check_pixel = [this, cr, l, r, t, b](int xx, int yy) -> bool {
                if (xx >= l && xx <= r && yy >= t && yy <= b) {
                    return _engine->SprMngr.IsPixNoTransp(cr->SprId, xx - l, yy - t, true);
                }
                return false;
            };

            bool ok = false;
            for (int sx = -6; sx <= 6 && !ok; sx++) {
                for (int sy = -6; sy <= 6 && !ok; sy++) {
                    if (check_pixel(x + sx * 7, y + sy * 7)) {
                        crits.push_back(cr);
                        ok = true;
                    }
                }
            }
        }
        else {
            if (x >= l && x <= r && y >= t && y <= b) {
                if (_engine->SprMngr.IsPixNoTransp(cr->SprId, x - l, y - t, true)) {
                    crits.push_back(cr);
                }
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

auto MapView::GetEntityAtScreenPos(int x, int y) -> ClientEntity*
{
    auto item_egg = false;
    ItemHexView* item = GetItemAtScreenPos(x, y, item_egg);
    CritterHexView* cr = GetCritterAtScreenPos(x, y, false, false);

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

auto MapView::FindPath(CritterHexView* cr, ushort start_x, ushort start_y, ushort& end_x, ushort& end_y, int cut) -> optional<FindPathResult>
{
#define GridAt(x, y) _findPathGrid[((MAX_FIND_PATH + 1) + (y)-grid_oy) * (MAX_FIND_PATH * 2 + 2) + ((MAX_FIND_PATH + 1) + (x)-grid_ox)]

    if (start_x == end_x && start_y == end_y) {
        return FindPathResult();
    }

    short numindex = 1;
    std::memset(_findPathGrid, 0, (MAX_FIND_PATH * 2 + 2) * (MAX_FIND_PATH * 2 + 2) * sizeof(short));

    auto grid_ox = start_x;
    auto grid_oy = start_y;
    GridAt(start_x, start_y) = numindex;

    vector<pair<ushort, ushort>> coords;
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

            for (const auto j : xrange(_engine->Settings.MapDirCount)) {
                auto nx = hx + sx[j];
                auto ny = hy + sy[j];
                if (nx < 0 || ny < 0 || nx >= _maxHexX || ny >= _maxHexY || GridAt(nx, ny)) {
                    continue;
                }

                GridAt(nx, ny) = -1;

                if (mh == 0u) {
                    if (GetField(static_cast<ushort>(nx), static_cast<ushort>(ny)).Flags.IsNotPassed) {
                        continue;
                    }
                }
                else {
                    // Base hex
                    auto nx_ = nx;
                    auto ny_ = ny;
                    for (uint k = 0; k < mh; k++) {
                        _engine->Geometry.MoveHexByDirUnsafe(nx_, ny_, static_cast<uchar>(j));
                    }
                    if (nx_ < 0 || ny_ < 0 || nx_ >= _maxHexX || ny_ >= _maxHexY) {
                        continue;
                    }
                    if (GetField(static_cast<ushort>(nx_), static_cast<ushort>(ny_)).Flags.IsNotPassed) {
                        continue;
                    }

                    // Clock wise hexes
                    auto is_square_corner = (!_engine->Settings.MapHexagonal && (j % 2) != 0);
                    auto steps_count = (is_square_corner ? mh * 2 : mh);
                    auto not_passed = false;
                    auto dir_ = (_engine->Settings.MapHexagonal ? ((j + 2) % 6) : ((j + 2) % 8));
                    if (is_square_corner) {
                        dir_ = (dir_ + 1) % 8;
                    }

                    auto nx_2 = nx_;
                    auto ny_2 = ny_;
                    for (uint k = 0; k < steps_count && !not_passed; k++) {
                        _engine->Geometry.MoveHexByDirUnsafe(nx_2, ny_2, static_cast<uchar>(dir_));
                        not_passed = GetField(static_cast<ushort>(nx_2), static_cast<ushort>(ny_2)).Flags.IsNotPassed;
                    }
                    if (not_passed) {
                        continue;
                    }

                    // Counter clock wise hexes
                    dir_ = (_engine->Settings.MapHexagonal ? (j + 4) % 6 : (j + 6) % 8);
                    if (is_square_corner) {
                        dir_ = (dir_ + 7) % 8;
                    }

                    nx_2 = nx_;
                    ny_2 = ny_;
                    for (uint k = 0; k < steps_count && !not_passed; k++) {
                        _engine->Geometry.MoveHexByDirUnsafe(nx_2, ny_2, static_cast<uchar>(dir_));
                        not_passed = GetField(static_cast<ushort>(nx_2), static_cast<ushort>(ny_2)).Flags.IsNotPassed;
                    }
                    if (not_passed) {
                        continue;
                    }
                }

                GridAt(nx, ny) = numindex;
                coords.emplace_back(nx, ny);

                if (cut >= 0 && _engine->Geometry.CheckDist(static_cast<ushort>(nx), static_cast<ushort>(ny), end_x, end_y, cut)) {
                    end_x = static_cast<ushort>(nx);
                    end_y = static_cast<ushort>(ny);
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

    vector<uchar> raw_steps;
    raw_steps.resize(numindex - 1);

    float base_angle = _engine->Geometry.GetDirAngle(end_x, end_y, start_x, start_y);

    // From end
    while (numindex > 1) {
        numindex--;

        int best_step_dir = -1;
        float best_step_angle_diff = 0.0f;

        const auto check_hex = [&](int dir, int step_hx, int step_hy) {
            if (GridAt(step_hx, step_hy) == numindex) {
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

    ushort trace_hx = start_x;
    ushort trace_hy = start_y;

    while (true) {
        ushort trace_tx = end_x;
        ushort trace_ty = end_y;

        for (auto i = static_cast<int>(raw_steps.size()) - 1; i >= 0; i--) {
            LineTracer tracer(_engine->Geometry, trace_hx, trace_hy, trace_tx, trace_ty, _maxHexX, _maxHexY, 0.0f);
            ushort next_hx = trace_hx;
            ushort next_hy = trace_hy;
            vector<uchar> direct_steps;
            bool failed = false;

            while (true) {
                uchar dir = tracer.GetNextHex(next_hx, next_hy);
                direct_steps.push_back(dir);

                if (next_hx == trace_tx && next_hy == trace_ty) {
                    break;
                }

                if (GridAt(next_hx, next_hy) <= 0) {
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

            result.ControlSteps.emplace_back(static_cast<ushort>(result.Steps.size()));

            trace_hx = trace_tx;
            trace_hy = trace_ty;
            break;
        }

        if (trace_tx == end_x && trace_ty == end_y) {
            break;
        }
    }

    RUNTIME_ASSERT(!result.Steps.empty());
    RUNTIME_ASSERT(!result.ControlSteps.empty());

    return {result};

#undef GridAt
}

bool MapView::CutPath(CritterHexView* cr, ushort start_x, ushort start_y, ushort& end_x, ushort& end_y, int cut)
{
    return !!FindPath(cr, start_x, start_y, end_x, end_y, cut);
}

bool MapView::TraceMoveWay(ushort& hx, ushort& hy, short& ox, short& oy, vector<uchar>& steps, int quad_dir)
{
    ox = 0;
    oy = 0;

    const auto try_move = [this, &hx, &hy, &steps](uchar dir, bool check_only = false) {
        auto check_hx = hx;
        auto check_hy = hy;

        if (!_engine->Geometry.MoveHexByDir(check_hx, check_hy, dir, _maxHexX, _maxHexY)) {
            return false;
        }

        const auto& f = GetField(check_hx, check_hy);
        if (f.Flags.IsNotPassed) {
            return false;
        }

        hx = check_hx;
        hy = check_hy;
        steps.push_back(dir);
        return true;
    };

    const auto try_move2 = [&try_move, &ox](uchar dir1, uchar dir2) {
        if (try_move(dir1)) {
            if (!try_move(dir2)) {
                ox = static_cast<short>(dir1 == 0 ? -16 : 16);
                return false;
            }
            return true;
        }

        if (try_move(dir2)) {
            if (!try_move(dir1)) {
                ox = static_cast<short>(dir2 == 5 ? 16 : -16);
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

auto MapView::TraceBullet(ushort hx, ushort hy, ushort tx, ushort ty, uint dist, float angle, CritterHexView* find_cr, bool find_cr_safe, vector<CritterHexView*>* critters, CritterFindType find_type, pair<ushort, ushort>* pre_block, pair<ushort, ushort>* block, vector<pair<ushort, ushort>>* steps, bool check_passed) -> bool
{
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
        if (_engine->Settings.MapHexagonal) {
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

        const auto& field = GetField(cx, cy);
        if (check_passed && field.Flags.IsNotRaked) {
            break;
        }
        if (critters != nullptr) {
            auto hex_critters = GetCritters(cx, cy, find_type);
            critters->insert(critters->end(), hex_critters.begin(), hex_critters.end());
        }
        if (find_cr != nullptr && field.Crit != nullptr) {
            if (field.Crit != nullptr && field.Crit == find_cr) {
                return true;
            }
            if (find_cr_safe) {
                return false;
            }
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
    RUNTIME_ASSERT(_viewField);

    RebuildMap(cx, cy);

    if (_mapperMode) {
        return;
    }

    const auto iw = GetViewWidth() / 2 + 2;
    const auto ih = GetViewHeight() / 2 + 2;
    auto hx = static_cast<ushort>(cx);
    auto hy = static_cast<ushort>(cy);

    auto find_set_center_dir = [this, &hx, &hy](const array<int, 2>& dirs, int steps) {
        auto sx = hx;
        auto sy = hy;
        const auto dirs_index = (dirs[1] == -1 ? 1 : 2);

        auto i = 0;

        for (; i < steps; i++) {
            if (!_engine->Geometry.MoveHexByDir(sx, sy, static_cast<uchar>(dirs[i % dirs_index]), _maxHexX, _maxHexY)) {
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
            _engine->Geometry.MoveHexByDir(hx, hy, _engine->Geometry.ReverseDir(static_cast<uchar>(dirs[i % dirs_index])), _maxHexX, _maxHexY);
        }
    };

    // Up
    if (_engine->Settings.MapHexagonal) {
        find_set_center_dir({0, 5}, ih);
    }
    else {
        find_set_center_dir({0, 6}, ih);
    }

    // Down
    if (_engine->Settings.MapHexagonal) {
        find_set_center_dir({3, 2}, ih);
    }
    else {
        find_set_center_dir({4, 2}, ih);
    }

    // Right
    if (_engine->Settings.MapHexagonal) {
        find_set_center_dir({1, -1}, iw);
    }
    else {
        find_set_center_dir({1, -1}, iw);
    }

    // Left
    if (_engine->Settings.MapHexagonal) {
        find_set_center_dir({4, -1}, iw);
    }
    else {
        find_set_center_dir({5, -1}, iw);
    }

    // Up-Right
    if (_engine->Settings.MapHexagonal) {
        find_set_center_dir({0, -1}, ih);
    }
    else {
        find_set_center_dir({0, -1}, ih);
    }

    // Down-Left
    if (_engine->Settings.MapHexagonal) {
        find_set_center_dir({3, -1}, ih);
    }
    else {
        find_set_center_dir({4, -1}, ih);
    }

    // Up-Left
    if (_engine->Settings.MapHexagonal) {
        find_set_center_dir({5, -1}, ih);
    }
    else {
        find_set_center_dir({6, -1}, ih);
    }

    // Down-Right
    if (_engine->Settings.MapHexagonal) {
        find_set_center_dir({2, -1}, ih);
    }
    else {
        find_set_center_dir({2, 1}, ih);
    }

    RebuildMap(hx, hy);
}

auto MapView::GetDayTime() const -> int
{
    return _engine->GetHour() * 60 + _engine->GetMinute();
}

auto MapView::GetMapTime() const -> int
{
    if (_curMapTime < 0) {
        return GetDayTime();
    }
    return _curMapTime;
}

auto MapView::GetMapDayTime() -> int*
{
    return _dayTime;
}

auto MapView::GetMapDayColor() -> uchar*
{
    return _dayColor;
}

void MapView::OnWindowSizeChanged()
{
    _fogForceRerender = true;

    ResizeView();
    RefreshMap();
}

auto MapView::GetTiles(ushort hx, ushort hy, bool is_roof) -> vector<MapTile>&
{
    return is_roof ? RoofsField[hy * GetWidth() + hx] : TilesField[hy * GetWidth() + hx];
}

void MapView::ClearSelTiles()
{
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

void MapView::SetTile(hstring name, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof, bool select)
{
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
    auto& ftile = field.AddTile(anim, 0, 0, layer, is_roof);
    auto& tiles = GetTiles(hx, hy, is_roof);
    tiles.push_back({name.as_hash(), hx, hy, ox, oy, layer, is_roof});
    tiles.back().IsSelected = select;

    if (ProcessTileBorder(ftile, is_roof)) {
        ResizeView();
        RefreshMap();
    }
    else {
        if (is_roof) {
            RebuildRoof();
        }
        else {
            RebuildTiles();
        }
    }
}

void MapView::EraseTile(ushort hx, ushort hy, uchar layer, bool is_roof, uint skip_index)
{
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
    _fastPids.insert(pid);
}

auto MapView::IsFastPid(hstring pid) const -> bool
{
    return _fastPids.count(pid) != 0;
}

void MapView::ClearFastPids()
{
    _fastPids.clear();
}

void MapView::AddIgnorePid(hstring pid)
{
    _ignorePids.insert(pid);
}

void MapView::SwitchIgnorePid(hstring pid)
{
    if (_ignorePids.count(pid) != 0u) {
        _ignorePids.erase(pid);
    }
    else {
        _ignorePids.insert(pid);
    }
}

auto MapView::IsIgnorePid(hstring pid) const -> bool
{
    return _ignorePids.count(pid) != 0;
}

void MapView::ClearIgnorePids()
{
    _ignorePids.clear();
}

auto MapView::GetHexesRect(const IRect& rect) const -> vector<pair<ushort, ushort>>
{
    vector<pair<ushort, ushort>> hexes;

    if (_engine->Settings.MapHexagonal) {
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

void MapView::MarkPassedHexes()
{
    for (ushort hx = 0; hx < _maxHexX; hx++) {
        for (ushort hy = 0; hy < _maxHexY; hy++) {
            const auto& field = GetField(hx, hy);
            auto& track = GetHexTrack(hx, hy);

            track = 0;

            if (field.Flags.IsNotPassed) {
                track = 2;
            }
            if (field.Flags.IsNotRaked) {
                track = 1;
            }
        }
    }

    RefreshMap();
}
