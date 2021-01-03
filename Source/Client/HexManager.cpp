//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "HexManager.h"
#include "GenericUtils.h"
#include "LineTracer.h"
#include "Log.h"
#include "StringUtils.h"

static constexpr auto MAX_LIGHT_VALUE = 10000;
static constexpr auto MAX_LIGHT_HEX = 200;
static constexpr auto MAX_LIGHT_ALPHA = 255;

static auto EvaluateItemDrawOrder(const ItemView* item)
{
    return item->GetIsFlat() ? (!item->IsAnyScenery() ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY) : (!item->IsAnyScenery() ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY);
}

static auto EvaluateCritterDrawOrder(const CritterView* cr)
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

void Field::AddDeadCrit(CritterView* cr)
{
    if (DeadCrits == nullptr) {
        DeadCrits = new vector<CritterView*>();
    }

    if (std::find(DeadCrits->begin(), DeadCrits->end(), cr) == DeadCrits->end()) {
        DeadCrits->push_back(cr);
    }
}

void Field::EraseDeadCrit(CritterView* cr)
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
    Corner = 0;

    if (Items != nullptr) {
        for (auto* item : *Items) {
            auto pid = item->GetProtoId();

            if (item->IsWall()) {
                Flags.IsWall = true;
                Flags.IsWallTransp = item->GetIsLightThru();

                Corner = static_cast<uchar>(item->GetCorner());
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
        for (auto* item : *BlockLinesItems) {
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

HexManager::HexManager(bool mapper_mode, HexSettings& settings, ProtoManager& proto_mngr, SpriteManager& spr_mngr, EffectManager& effect_mngr, ResourceManager& res_mngr, ClientScriptSystem& script_sys, GameTimer& game_time) : _settings {settings}, _geomHelper(_settings), _protoMngr {proto_mngr}, _sprMngr {spr_mngr}, _effectMngr {effect_mngr}, _resMngr {res_mngr}, _scriptSys {script_sys}, _gameTime {game_time}, _mainTree(_settings, spr_mngr, _spritesPool), _tilesTree(_settings, spr_mngr, _spritesPool), _roofTree(_settings, spr_mngr, _spritesPool), _roofRainTree(_settings, spr_mngr, _spritesPool)
{
    _mapperMode = mapper_mode;

    _rtScreenOx = static_cast<uint>(std::ceil(static_cast<float>(_settings.MapHexWidth) / MIN_ZOOM));
    _rtScreenOy = static_cast<uint>(std::ceil(static_cast<float>(_settings.MapHexLineHeight * 2) / MIN_ZOOM));

    _rtMap = _sprMngr.CreateRenderTarget(false, true, 0, 0, false);
    _rtMap->DrawEffect = _effectMngr.Effects.FlushMap;

    _rtLight = _sprMngr.CreateRenderTarget(false, true, _rtScreenOx * 2, _rtScreenOy * 2, false);
    _rtLight->DrawEffect = _effectMngr.Effects.FlushLight;

    if (!_mapperMode) {
        _rtFog = _sprMngr.CreateRenderTarget(false, true, _rtScreenOx * 2, _rtScreenOy * 2, false);
        _rtFog->DrawEffect = _effectMngr.Effects.FlushFog;
    }

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

    _picRainFallName = "Rain/Fall.fofrm";
    _picRainDropName = "Rain/Drop.fofrm";

    if (_mapperMode) {
        ClearSelTiles();
    }
}

HexManager::~HexManager()
{
    delete[] _findPathGrid;

    _mainTree.Clear();
    _roofRainTree.Clear();
    _roofTree.Clear();
    _tilesTree.Clear();

    for (auto& it : _rainData) {
        delete it;
    }

    for (auto& hex_item : _hexItems) {
        hex_item->Release();
    }

    delete[] _viewField;
    ResizeField(0, 0);
}

auto HexManager::GetViewWidth() const -> int
{
    return static_cast<int>(static_cast<float>(_settings.ScreenWidth / _settings.MapHexWidth + ((_settings.ScreenWidth % _settings.MapHexWidth) != 0 ? 1 : 0)) * _settings.SpritesZoom);
}

auto HexManager::GetViewHeight() const -> int
{
    return static_cast<int>(static_cast<float>(_settings.ScreenHeight / _settings.MapHexLineHeight + ((_settings.ScreenHeight % _settings.MapHexLineHeight) != 0 ? 1 : 0)) * _settings.SpritesZoom);
}

void HexManager::ReloadSprites()
{
    _curDataPrefix = _settings.MapDataPrefix;

    _picHex[0] = _sprMngr.LoadAnimation(_curDataPrefix + "hex1.png", true, false);
    _picHex[1] = _sprMngr.LoadAnimation(_curDataPrefix + "hex2.png", true, false);
    _picHex[2] = _sprMngr.LoadAnimation(_curDataPrefix + "hex3.png", true, false);
    _cursorPrePic = _sprMngr.LoadAnimation(_curDataPrefix + "move_pre.png", true, false);
    _cursorPostPic = _sprMngr.LoadAnimation(_curDataPrefix + "move_post.png", true, false);
    _cursorXPic = _sprMngr.LoadAnimation(_curDataPrefix + "move_x.png", true, false);
    _picTrack1 = _sprMngr.LoadAnimation(_curDataPrefix + "track1.png", true, false);
    _picTrack2 = _sprMngr.LoadAnimation(_curDataPrefix + "track2.png", true, false);
    _picHexMask = _sprMngr.LoadAnimation(_curDataPrefix + "hex_mask.png", true, false);

    SetRainAnimation(nullptr, nullptr);
}

void HexManager::AddFieldItem(ushort hx, ushort hy, ItemHexView* item)
{
    GetField(hx, hy).AddItem(item, nullptr);

    if (item->IsNonEmptyBlockLines()) {
        _geomHelper.ForEachBlockLines(item->GetBlockLines(), hx, hy, _maxHexX, _maxHexY, [this, item](auto hx2, auto hy2) { GetField(hx2, hy2).AddItem(nullptr, item); });
    }
}

void HexManager::EraseFieldItem(ushort hx, ushort hy, ItemHexView* item)
{
    GetField(hx, hy).EraseItem(item, nullptr);

    if (item->IsNonEmptyBlockLines()) {
        _geomHelper.ForEachBlockLines(item->GetBlockLines(), hx, hy, _maxHexX, _maxHexY, [this, item](auto hx2, auto hy2) { GetField(hx2, hy2).EraseItem(nullptr, item); });
    }
}

auto HexManager::AddItem(uint id, hash pid, ushort hx, ushort hy, bool is_added, vector<vector<uchar>>* data) -> uint
{
    if (id == 0u) {
        WriteLog("Item id is zero.\n");
        return 0;
    }

    if (!IsMapLoaded()) {
        WriteLog("Map is not loaded.");
        return 0;
    }

    if (hx >= _maxHexX || hy >= _maxHexY) {
        WriteLog("Position hx {} or hy {} error value.\n", hx, hy);
        return 0;
    }

    const auto* proto = _protoMngr.GetProtoItem(pid);
    if (proto == nullptr) {
        WriteLog("Proto not found '{}'.\n", _str().parseHash(pid));
        return 0;
    }

    // Change
    auto* item_old = GetItemById(id);
    if (item_old != nullptr) {
        if (item_old->GetProtoId() == pid && item_old->GetHexX() == hx && item_old->GetHexY() == hy) {
            item_old->IsDestroyed = false;
            if (item_old->IsFinishing()) {
                item_old->StopFinishing();
            }
            if (data != nullptr) {
                item_old->Props.RestoreData(*data);
            }
            return item_old->GetId();
        }

        DeleteItem(item_old, true, nullptr);
    }

    // Parse
    auto& field = GetField(hx, hy);
    auto* item = new ItemHexView(id, proto, data, hx, hy, &field.ScrX, &field.ScrY, _resMngr, _effectMngr, _gameTime);
    if (is_added) {
        item->SetShowAnim();
    }
    PushItem(item);

    // Check ViewField size
    if (!ProcessHexBorders(item->Anim->GetSprId(0), item->GetOffsetX(), item->GetOffsetY(), true)) {
        // Draw
        if (IsHexToDraw(hx, hy) && !item->GetIsHidden() && !item->GetIsHiddenPicture() && !item->IsFullyTransparent()) {
            auto& spr = _mainTree.InsertSprite(EvaluateItemDrawOrder(item), hx, hy + item->GetDrawOrderOffsetHexY(), (_settings.MapHexWidth / 2), (_settings.MapHexHeight / 2), &field.ScrX, &field.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
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

    return item->GetId();
}

void HexManager::FinishItem(uint id, bool is_deleted)
{
    if (id == 0u) {
        WriteLog("Item zero id.\n");
        return;
    }

    auto* item = GetItemById(id);
    if (item == nullptr) {
        WriteLog("Item '{}' not found.\n", _str().parseHash(id));
        return;
    }

    item->Finish();
    if (is_deleted) {
        item->SetHideAnim();
    }

    if (_mapperMode) {
        DeleteItem(item, true, nullptr);
    }
}

void HexManager::DeleteItem(ItemHexView* item, bool destroy_item, vector<ItemHexView*>::iterator* it_hex_items)
{
    const auto hx = item->GetHexX();
    const auto hy = item->GetHexY();

    if (item->SprDrawValid) {
        item->SprDraw->Unvalidate();
    }

    auto it = std::find(_hexItems.begin(), _hexItems.end(), item);
    RUNTIME_ASSERT(it != _hexItems.end());
    it = _hexItems.erase(it);

    if (it_hex_items != nullptr) {
        *it_hex_items = it;
    }

    EraseFieldItem(hx, hy, item);

    if (item->GetIsLight() || !item->GetIsLightThru()) {
        RebuildLight();
    }

    if (destroy_item) {
        item->IsDestroyed = true;
        _scriptSys.RemoveEntity(item);
        item->Release();
    }
}

void HexManager::ProcessItems()
{
    for (auto it = _hexItems.begin(); it != _hexItems.end();) {
        auto* item = *it;
        item->Process();

        if (item->IsDynamicEffect() && !item->IsFinishing()) {
            const auto [step_hx, step_hy] = item->GetEffectStep();
            if (item->GetHexX() != step_hx || item->GetHexY() != step_hy) {
                const auto hx = item->GetHexX();
                const auto hy = item->GetHexY();

                const auto [x, y] = _geomHelper.GetHexInterval(hx, hy, step_hx, step_hy);
                item->EffOffsX -= static_cast<float>(x);
                item->EffOffsY -= static_cast<float>(y);

                EraseFieldItem(hx, hy, item);

                auto& field = GetField(step_hx, step_hy);
                item->SetHexX(step_hx);
                item->SetHexY(step_hy);

                AddFieldItem(step_hx, step_hy, item);

                if (item->SprDrawValid) {
                    item->SprDraw->Unvalidate();
                }

                if (IsHexToDraw(step_hx, step_hy)) {
                    item->SprDraw = &_mainTree.InsertSprite(EvaluateItemDrawOrder(item), step_hx, step_hy + item->GetDrawOrderOffsetHexY(), (_settings.MapHexWidth / 2), (_settings.MapHexHeight / 2), &field.ScrX, &field.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
                    if (!item->GetIsNoLightInfluence()) {
                        item->SprDraw->SetLight(item->GetCorner(), _hexLight, _maxHexX, _maxHexY);
                    }
                    field.AddSpriteToChain(item->SprDraw);
                }

                item->SetAnimOffs();
            }
        }

        if (item->IsFinish()) {
            DeleteItem(item, true, &it);
        }
        else {
            ++it;
        }
    }
}

void HexManager::SkipItemsFade()
{
    for (auto* item : _hexItems) {
        item->SkipFade();
    }
}

static auto ItemCompScen(ItemHexView* o1, ItemHexView* o2) -> bool
{
    return o1->IsScenery() && !o2->IsScenery();
}

static auto ItemCompWall(ItemHexView* o1, ItemHexView* o2) -> bool
{
    return o1->IsWall() && !o2->IsWall();
}

void HexManager::PushItem(ItemHexView* item)
{
    _hexItems.push_back(item);

    const auto hx = item->GetHexX();
    const auto hy = item->GetHexY();

    AddFieldItem(hx, hy, item);

    // Sort
    auto& field = GetField(hx, hy);
    std::sort(field.Items->begin(), field.Items->end(), ItemCompScen);
    std::sort(field.Items->begin(), field.Items->end(), ItemCompWall);
}

auto HexManager::GetItem(ushort hx, ushort hy, hash pid) -> ItemHexView*
{
    if (!IsMapLoaded() || hx >= _maxHexX || hy >= _maxHexY || GetField(hx, hy).Items == nullptr) {
        return nullptr;
    }

    for (auto* item : *GetField(hx, hy).Items) {
        if (item->GetProtoId() == pid) {
            return item;
        }
    }
    return nullptr;
}

auto HexManager::GetItemById(ushort hx, ushort hy, uint id) -> ItemHexView*
{
    if (!IsMapLoaded() || hx >= _maxHexX || hy >= _maxHexY || GetField(hx, hy).Items == nullptr) {
        return nullptr;
    }

    for (auto* item : *GetField(hx, hy).Items) {
        if (item->GetId() == id) {
            return item;
        }
    }
    return nullptr;
}

auto HexManager::GetItemById(uint id) -> ItemHexView*
{
    for (auto* item : _hexItems) {
        if (item->GetId() == id) {
            return item;
        }
    }
    return nullptr;
}

void HexManager::GetItems(ushort hx, ushort hy, vector<ItemHexView*>& items)
{
    if (!IsMapLoaded()) {
        return;
    }

    auto& field = GetField(hx, hy);
    if (field.Items == nullptr) {
        return;
    }

    for (auto* item : *field.Items) {
        if (std::find(items.begin(), items.end(), item) == items.end()) {
            items.push_back(item);
        }
    }
}

auto HexManager::GetRectForText(ushort hx, ushort hy) -> IRect
{
    if (!IsMapLoaded()) {
        return IRect();
    }

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
            const auto* si = _sprMngr.GetSpriteInfo(item->SprId);
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

auto HexManager::RunEffect(hash eff_pid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy) -> bool
{
    if (!IsMapLoaded()) {
        return false;
    }

    if (from_hx >= _maxHexX || from_hy >= _maxHexY || to_hx >= _maxHexX || to_hy >= _maxHexY) {
        WriteLog("Incorrect value of from_x {} or from_y {} or to_x {} or to_y {}.\n", from_hx, from_hy, to_hx, to_hy);
        return false;
    }

    const auto* proto = _protoMngr.GetProtoItem(eff_pid);
    if (proto == nullptr) {
        WriteLog("Proto '{}' not found.\n", _str().parseHash(eff_pid));
        return false;
    }

    auto& field = GetField(from_hx, from_hy);
    auto* item = new ItemHexView(0, proto, nullptr, from_hx, from_hy, &field.ScrX, &field.ScrY, _resMngr, _effectMngr, _gameTime);

    auto sx = 0.0f;
    auto sy = 0.0f;
    auto dist = 0u;

    if (from_hx != to_hx || from_hy != to_hy) {
        item->EffSteps.push_back(std::make_pair(from_hx, from_hy));
        TraceBullet(from_hx, from_hy, to_hx, to_hy, 0, 0.0f, nullptr, false, nullptr, 0, nullptr, nullptr, &item->EffSteps, false);
        auto [x, y] = _geomHelper.GetHexInterval(from_hx, from_hy, to_hx, to_hy);
        y += GenericUtils::Random(5, 25); // Center of body
        std::tie(sx, sy) = GenericUtils::GetStepsXY(0, 0, x, y);
        dist = GenericUtils::DistSqrt(0, 0, x, y);
    }

    item->SetEffect(sx, sy, dist, _geomHelper.GetFarDir(from_hx, from_hy, to_hx, to_hy));

    AddFieldItem(from_hx, from_hy, item);
    _hexItems.push_back(item);

    if (IsHexToDraw(from_hx, from_hy)) {
        item->SprDraw = &_mainTree.InsertSprite(EvaluateItemDrawOrder(item), from_hx, from_hy + item->GetDrawOrderOffsetHexY(), (_settings.MapHexWidth / 2), (_settings.MapHexHeight / 2), &field.ScrX, &field.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
        if (!item->GetIsNoLightInfluence()) {
            item->SprDraw->SetLight(item->GetCorner(), _hexLight, _maxHexX, _maxHexY);
        }
        field.AddSpriteToChain(item->SprDraw);
    }

    return true;
}

void HexManager::ProcessRain()
{
    if (_rainCapacity == 0) {
        return;
    }

    const auto delta = _gameTime.GameTick() - _rainLastTick;
    if (delta < _settings.RainTick) {
        return;
    }

    _rainLastTick = _gameTime.GameTick();

    for (auto* cur_drop : _rainData) {
        if (cur_drop->DropCnt == -1) {
            cur_drop->OffsX = static_cast<short>(cur_drop->OffsX + _settings.RainSpeedX);
            cur_drop->OffsY = static_cast<short>(cur_drop->OffsY + _settings.RainSpeedY);
            if (cur_drop->OffsY >= cur_drop->GroundOffsY) {
                cur_drop->DropCnt = 0;
                cur_drop->CurSprId = _picRainDrop->GetSprId(0);
            }
        }
        else {
            cur_drop->DropCnt++;
            if (static_cast<uint>(cur_drop->DropCnt) >= _picRainDrop->CntFrm) {
                cur_drop->CurSprId = _picRainFall->GetCurSprId(_gameTime.GameTick());
                cur_drop->DropCnt = -1;
                cur_drop->OffsX = static_cast<short>(GenericUtils::Random(-10, 10));
                cur_drop->OffsY = static_cast<short>(-100 - GenericUtils::Random(0, 100));
            }
            else {
                cur_drop->CurSprId = _picRainDrop->GetSprId(cur_drop->DropCnt);
            }
        }
    }
}

void HexManager::SetRainAnimation(const char* fall_anim_name, const char* drop_anim_name)
{
    if (fall_anim_name != nullptr) {
        _picRainFallName = fall_anim_name;
    }
    if (drop_anim_name != nullptr) {
        _picRainDropName = drop_anim_name;
    }

    if (_picRainFall == _sprMngr.DummyAnimation) {
        _picRainFall = nullptr;
    }
    else {
        _sprMngr.DestroyAnyFrames(_picRainFall);
    }

    if (_picRainDrop == _sprMngr.DummyAnimation) {
        _picRainDrop = nullptr;
    }
    else {
        _sprMngr.DestroyAnyFrames(_picRainDrop);
    }

    _picRainFall = _sprMngr.LoadAnimation(_picRainFallName, true, false);
    _picRainDrop = _sprMngr.LoadAnimation(_picRainDropName, true, false);
}

void HexManager::SetCursorPos(int x, int y, bool show_steps, bool refresh)
{
    ushort hx = 0;
    ushort hy = 0;
    if (GetHexPixel(x, y, hx, hy)) {
        auto& field = GetField(hx, hy);

        _cursorX = field.ScrX + 1 - 1;
        _cursorY = field.ScrY - 1 - 1;

        auto* chosen = GetChosen();
        if (chosen == nullptr) {
            _drawCursorX = -1;
            return;
        }

        const auto cx = chosen->GetHexX();
        const auto cy = chosen->GetHexY();
        const auto mh = chosen->GetMultihex();

        if ((cx == hx && cy == hy) || (field.Flags.IsNotPassed && (mh == 0u || !_geomHelper.CheckDist(cx, cy, hx, hy, mh)))) {
            _drawCursorX = -1;
        }
        else {
            static auto last_cur_x = 0;
            static ushort last_hx = 0;
            static ushort last_hy = 0;

            if (refresh || hx != last_hx || hy != last_hy) {
                if (chosen->IsAlive()) {
                    vector<uchar> steps;
                    if (!FindPath(chosen, cx, cy, hx, hy, steps, -1)) {
                        _drawCursorX = -1;
                    }
                    else {
                        _drawCursorX = static_cast<int>(show_steps ? steps.size() : 0);
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

void HexManager::DrawCursor(uint spr_id)
{
    NON_CONST_METHOD_HINT();

    if (_settings.HideCursor || !_settings.ShowMoveCursor) {
        return;
    }

    const auto* si = _sprMngr.GetSpriteInfo(spr_id);
    if (si != nullptr) {
        _sprMngr.DrawSpriteSize(spr_id, static_cast<int>(static_cast<float>(_cursorX + _settings.ScrOx) / _settings.SpritesZoom), static_cast<int>(static_cast<float>(_cursorY + _settings.ScrOy) / _settings.SpritesZoom), static_cast<int>(static_cast<float>(si->Width) / _settings.SpritesZoom), static_cast<int>(static_cast<float>(si->Height) / _settings.SpritesZoom), true, false, 0);
    }
}

void HexManager::DrawCursor(const char* text)
{
    NON_CONST_METHOD_HINT();

    if (_settings.HideCursor || !_settings.ShowMoveCursor) {
        return;
    }

    const auto x = static_cast<int>(static_cast<float>(_cursorX + _settings.ScrOx) / _settings.SpritesZoom);
    const auto y = static_cast<int>(static_cast<float>(_cursorY + _settings.ScrOy) / _settings.SpritesZoom);
    const auto r = IRect(x, y, static_cast<int>(static_cast<float>(x + _settings.MapHexWidth) / _settings.SpritesZoom), static_cast<int>(static_cast<float>(y + _settings.MapHexHeight) / _settings.SpritesZoom));
    _sprMngr.DrawStr(r, text, FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, 0);
}

void HexManager::RebuildMap(int rx, int ry)
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
    _roofRainTree.Unvalidate();

    for (auto* drop : _rainData) {
        delete drop;
    }
    _rainData.clear();

    _sprMngr.EggNotValid();

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
            const auto spr_id = (GetHexTrack(nx, ny) == 1 ? _picTrack1->GetCurSprId(_gameTime.GameTick()) : _picTrack2->GetCurSprId(_gameTime.GameTick()));
            const auto* si = _sprMngr.GetSpriteInfo(spr_id);
            auto& spr = _mainTree.AddSprite(DRAW_ORDER_TRACK, nx, ny, (_settings.MapHexWidth / 2), (_settings.MapHexHeight / 2) + (si != nullptr ? si->Height / 2 : 0), &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            field.AddSpriteToChain(&spr);
        }

        // Hex Lines
        if (_isShowHex) {
            const auto spr_id = _picHex[0]->GetCurSprId(_gameTime.GameTick());
            const auto* si = _sprMngr.GetSpriteInfo(spr_id);
            auto& spr = _mainTree.AddSprite(DRAW_ORDER_HEX_GRID, nx, ny, si != nullptr ? si->Width / 2 : 0, si != nullptr ? si->Height : 0, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            field.AddSpriteToChain(&spr);
        }

        // Rain
        if (_rainCapacity != 0 && _rainCapacity >= GenericUtils::Random(0, 255)) {
            auto rofx = nx;
            auto rofy = ny;
            if ((rofx % 2) != 0) {
                rofx--;
            }
            if ((rofy % 2) != 0) {
                rofy--;
            }

            Drop* new_drop = nullptr;

            if (GetField(rofx, rofy).GetTilesCount(true) == 0u) {
                new_drop = new Drop {_picRainFall->GetCurSprId(_gameTime.GameTick()), static_cast<short>(GenericUtils::Random(-10, 10)), static_cast<short>(-GenericUtils::Random(0, 200)), 0, -1};
                _rainData.push_back(new_drop);

                auto& spr = _mainTree.AddSprite(DRAW_ORDER_RAIN, nx, ny, _settings.MapHexWidth / 2, _settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &new_drop->CurSprId, &new_drop->OffsX, &new_drop->OffsY, nullptr, &_effectMngr.Effects.Rain, nullptr);
                spr.SetLight(CORNER_EAST_WEST, _hexLight, _maxHexX, _maxHexY);
                field.AddSpriteToChain(&spr);
            }
            else if (_roofSkip == 0 || _roofSkip != GetField(rofx, rofy).RoofNum) {
                new_drop = new Drop {_picRainFall->GetCurSprId(_gameTime.GameTick()), static_cast<short>(GenericUtils::Random(-10, 10)), static_cast<short>(-100 - GenericUtils::Random(0, 100)), -100, -1};
                _rainData.push_back(new_drop);

                auto& spr = _roofRainTree.AddSprite(DRAW_ORDER_RAIN, nx, ny, _settings.MapHexWidth / 2, _settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &new_drop->CurSprId, &new_drop->OffsX, &new_drop->OffsY, nullptr, &_effectMngr.Effects.Rain, nullptr);
                spr.SetLight(CORNER_EAST_WEST, _hexLight, _maxHexX, _maxHexY);
                field.AddSpriteToChain(&spr);
            }

            if (new_drop != nullptr) {
                new_drop->OffsX = static_cast<short>(GenericUtils::Random(-10, 10));
                new_drop->OffsY = static_cast<short>(-100 - GenericUtils::Random(0, 100));

                if (new_drop->OffsY < 0) {
                    new_drop->OffsY = static_cast<short>(GenericUtils::Random(new_drop->OffsY, 0));
                }
            }
        }

        // Items on hex
        if (field.Items != nullptr) {
            for (auto* item : *field.Items) {
                if (!_mapperMode) {
                    if (item->GetIsHidden() || item->GetIsHiddenPicture() || item->IsFullyTransparent()) {
                        continue;
                    }
                    if (!_settings.ShowScen && item->IsScenery()) {
                        continue;
                    }
                    if (!_settings.ShowItem && !item->IsAnyScenery()) {
                        continue;
                    }
                    if (!_settings.ShowWall && item->IsWall()) {
                        continue;
                    }
                }
                else {
                    const auto is_fast = _fastPids.count(item->GetProtoId()) != 0;
                    if (!_settings.ShowScen && !is_fast && item->IsScenery()) {
                        continue;
                    }
                    if (!_settings.ShowItem && !is_fast && !item->IsAnyScenery()) {
                        continue;
                    }
                    if (!_settings.ShowWall && !is_fast && item->IsWall()) {
                        continue;
                    }
                    if (!_settings.ShowFast && is_fast) {
                        continue;
                    }
                    if (_ignorePids.count(item->GetProtoId()) != 0u) {
                        continue;
                    }
                }

                auto& spr = _mainTree.AddSprite(EvaluateItemDrawOrder(item), nx, ny + item->GetDrawOrderOffsetHexY(), _settings.MapHexWidth / 2, _settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
                if (!item->GetIsNoLightInfluence()) {
                    spr.SetLight(item->GetCorner(), _hexLight, _maxHexX, _maxHexY);
                }
                item->SetSprite(&spr);
                field.AddSpriteToChain(&spr);
            }
        }

        // Critters
        auto* cr = field.Crit;
        if (cr != nullptr && _settings.ShowCrit && cr->Visible) {
            auto& spr = _mainTree.AddSprite(EvaluateCritterDrawOrder(cr), nx, ny, _settings.MapHexWidth / 2, _settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy, &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid);
            spr.SetLight(CORNER_EAST_WEST, _hexLight, _maxHexX, _maxHexY);
            cr->SprDraw = &spr;
            cr->SetSprRect();

            auto contour = 0;
            if (cr->GetId() == _critterContourCrId) {
                contour = _critterContour;
            }
            else if (!cr->IsChosen()) {
                contour = _crittersContour;
            }
            spr.SetContour(contour, cr->ContourColor);

            field.AddSpriteToChain(&spr);
        }

        // Dead critters
        if (field.DeadCrits != nullptr && _settings.ShowCrit) {
            for (auto* dead_cr : *field.DeadCrits) {
                if (!dead_cr->Visible) {
                    continue;
                }

                auto& spr = _mainTree.AddSprite(EvaluateCritterDrawOrder(dead_cr), nx, ny, _settings.MapHexWidth / 2, _settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &dead_cr->SprId, &dead_cr->SprOx, &dead_cr->SprOy, &dead_cr->Alpha, &dead_cr->DrawEffect, &dead_cr->SprDrawValid);
                spr.SetLight(CORNER_EAST_WEST, _hexLight, _maxHexX, _maxHexY);
                dead_cr->SprDraw = &spr;
                dead_cr->SetSprRect();

                field.AddSpriteToChain(&spr);
            }
        }
    }
    _mainTree.SortByMapPos();

    _screenHexX = rx;
    _screenHexY = ry;

    _scriptSys.RenderMapEvent();
}

void HexManager::RebuildMapOffset(int ox, int oy)
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
            const auto spr_id = (GetHexTrack(nx, ny) == 1 ? _picTrack1->GetCurSprId(_gameTime.GameTick()) : _picTrack2->GetCurSprId(_gameTime.GameTick()));
            const auto* si = _sprMngr.GetSpriteInfo(spr_id);
            auto& spr = _mainTree.InsertSprite(DRAW_ORDER_TRACK, nx, ny, _settings.MapHexWidth / 2, (_settings.MapHexHeight / 2) + (si != nullptr ? si->Height / 2 : 0), &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            field.AddSpriteToChain(&spr);
        }

        // Hex lines
        if (_isShowHex) {
            const auto spr_id = _picHex[0]->GetCurSprId(_gameTime.GameTick());
            const auto* si = _sprMngr.GetSpriteInfo(spr_id);
            auto& spr = _mainTree.InsertSprite(DRAW_ORDER_HEX_GRID, nx, ny, si != nullptr ? si->Width / 2 : 0, si != nullptr ? si->Height : 0, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            field.AddSpriteToChain(&spr);
        }

        // Rain
        if (_rainCapacity != 0 && _rainCapacity >= GenericUtils::Random(0, 255)) {
            auto rofx = nx;
            auto rofy = ny;
            if ((rofx & 1) != 0) {
                rofx--;
            }
            if ((rofy & 1) != 0) {
                rofy--;
            }

            Drop* new_drop = nullptr;

            if (GetField(rofx, rofy).GetTilesCount(true) == 0u) {
                new_drop = new Drop {_picRainFall->GetCurSprId(_gameTime.GameTick()), static_cast<short>(GenericUtils::Random(-10, 10)), static_cast<short>(-GenericUtils::Random(0, 200)), 0, -1};
                _rainData.push_back(new_drop);

                auto& spr = _mainTree.InsertSprite(DRAW_ORDER_RAIN, nx, ny, _settings.MapHexWidth / 2, _settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &new_drop->CurSprId, &new_drop->OffsX, &new_drop->OffsY, nullptr, &_effectMngr.Effects.Rain, nullptr);
                spr.SetLight(CORNER_EAST_WEST, _hexLight, _maxHexX, _maxHexY);
                field.AddSpriteToChain(&spr);
            }
            else if (_roofSkip == 0 || _roofSkip != GetField(rofx, rofy).RoofNum) {
                new_drop = new Drop {_picRainFall->GetCurSprId(_gameTime.GameTick()), static_cast<short>(GenericUtils::Random(-10, 10)), static_cast<short>(-100 - GenericUtils::Random(0, 100)), -100, -1};
                _rainData.push_back(new_drop);

                auto& spr = _roofRainTree.InsertSprite(DRAW_ORDER_RAIN, nx, ny, _settings.MapHexWidth / 2, _settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &new_drop->CurSprId, &new_drop->OffsX, &new_drop->OffsY, nullptr, &_effectMngr.Effects.Rain, nullptr);
                spr.SetLight(CORNER_EAST_WEST, _hexLight, _maxHexX, _maxHexY);
                field.AddSpriteToChain(&spr);
            }

            if (new_drop != nullptr) {
                new_drop->OffsX = static_cast<short>(GenericUtils::Random(-10, 10));
                new_drop->OffsY = static_cast<short>(-100 - GenericUtils::Random(0, 100));

                if (new_drop->OffsY < 0) {
                    new_drop->OffsY = static_cast<short>(GenericUtils::Random(new_drop->OffsY, 0));
                }
            }
        }

        // Items on hex
        if (field.Items != nullptr) {
            for (auto* item : *field.Items) {
                if (!_mapperMode) {
                    if (item->GetIsHidden() || item->GetIsHiddenPicture() || item->IsFullyTransparent()) {
                        continue;
                    }
                    if (!_settings.ShowScen && item->IsScenery()) {
                        continue;
                    }
                    if (!_settings.ShowItem && !item->IsAnyScenery()) {
                        continue;
                    }
                    if (!_settings.ShowWall && item->IsWall()) {
                        continue;
                    }
                }
                else {
                    const auto is_fast = _fastPids.count(item->GetProtoId()) != 0;
                    if (!_settings.ShowScen && !is_fast && item->IsScenery()) {
                        continue;
                    }
                    if (!_settings.ShowItem && !is_fast && !item->IsAnyScenery()) {
                        continue;
                    }
                    if (!_settings.ShowWall && !is_fast && item->IsWall()) {
                        continue;
                    }
                    if (!_settings.ShowFast && is_fast) {
                        continue;
                    }
                    if (_ignorePids.count(item->GetProtoId()) != 0u) {
                        continue;
                    }
                }

                auto& spr = _mainTree.InsertSprite(EvaluateItemDrawOrder(item), nx, ny + item->GetDrawOrderOffsetHexY(), _settings.MapHexWidth / 2, _settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &item->SprId, &item->ScrX, &item->ScrY, &item->Alpha, &item->DrawEffect, &item->SprDrawValid);
                if (!item->GetIsNoLightInfluence()) {
                    spr.SetLight(item->GetCorner(), _hexLight, _maxHexX, _maxHexY);
                }
                item->SetSprite(&spr);
                field.AddSpriteToChain(&spr);
            }
        }

        // Critters
        auto* cr = field.Crit;
        if (cr != nullptr && _settings.ShowCrit && cr->Visible) {
            auto& spr = _mainTree.InsertSprite(EvaluateCritterDrawOrder(cr), nx, ny, _settings.MapHexWidth / 2, _settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy, &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid);
            spr.SetLight(CORNER_EAST_WEST, _hexLight, _maxHexX, _maxHexY);
            cr->SprDraw = &spr;
            cr->SetSprRect();

            auto contour = 0;
            if (cr->GetId() == _critterContourCrId) {
                contour = _critterContour;
            }
            else if (!cr->IsChosen()) {
                contour = _crittersContour;
            }

            spr.SetContour(contour, cr->ContourColor);

            field.AddSpriteToChain(&spr);
        }

        // Dead critters
        if (field.DeadCrits != nullptr && _settings.ShowCrit) {
            for (auto* cr : *field.DeadCrits) {
                if (!cr->Visible) {
                    continue;
                }

                auto& spr = _mainTree.InsertSprite(EvaluateCritterDrawOrder(cr), nx, ny, _settings.MapHexWidth / 2, _settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy, &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid);
                spr.SetLight(CORNER_EAST_WEST, _hexLight, _maxHexX, _maxHexY);
                cr->SprDraw = &spr;
                cr->SetSprRect();

                field.AddSpriteToChain(&spr);
            }
        }

        // Tiles
        const auto tiles_count = field.GetTilesCount(false);
        if (_settings.ShowTile && (tiles_count != 0u)) {
            for (uint i = 0; i < tiles_count; i++) {
                const auto& tile = field.GetTile(i, false);
                const auto spr_id = tile.Anim->GetSprId(0);

                if (_mapperMode) {
                    auto& tiles = GetTiles(nx, ny, false);
                    auto& spr = _tilesTree.InsertSprite(DRAW_ORDER_TILE + tile.Layer, nx, ny, tile.OffsX + _settings.MapTileOffsX, tile.OffsY + _settings.MapTileOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, tiles[i].IsSelected ? &SelectAlpha : nullptr, &_effectMngr.Effects.Tile, nullptr);
                    field.AddSpriteToChain(&spr);
                }
                else {
                    auto& spr = _tilesTree.InsertSprite(DRAW_ORDER_TILE + tile.Layer, nx, ny, tile.OffsX + _settings.MapTileOffsX, tile.OffsY + _settings.MapTileOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, nullptr, &_effectMngr.Effects.Tile, nullptr);
                    field.AddSpriteToChain(&spr);
                }
            }
        }

        // Roof
        const auto roofs_count = field.GetTilesCount(true);
        if (_settings.ShowRoof && (roofs_count != 0u) && ((_roofSkip == 0) || _roofSkip != field.RoofNum)) {
            for (uint i = 0; i < roofs_count; i++) {
                const auto& roof = field.GetTile(i, true);
                const auto spr_id = roof.Anim->GetSprId(0);

                if (_mapperMode) {
                    auto& roofs = GetTiles(nx, ny, true);
                    auto& spr = _roofTree.InsertSprite(DRAW_ORDER_TILE + roof.Layer, nx, ny, roof.OffsX + _settings.MapRoofOffsX, roof.OffsY + _settings.MapRoofOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, roofs[i].IsSelected ? &SelectAlpha : &_settings.RoofAlpha, &_effectMngr.Effects.Roof, nullptr);
                    spr.SetEgg(EGG_ALWAYS);
                    field.AddSpriteToChain(&spr);
                }
                else {
                    auto& spr = _roofTree.InsertSprite(DRAW_ORDER_TILE + roof.Layer, nx, ny, roof.OffsX + _settings.MapRoofOffsX, roof.OffsY + _settings.MapRoofOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, &_settings.RoofAlpha, &_effectMngr.Effects.Roof, nullptr);
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
    for (auto& [id, cr] : _critters) {
        cr->SetSprRect();
    }

    // Light
    RealRebuildLight();
    _requestRebuildLight = false;
    _requestRenderLight = true;

    _scriptSys.RenderMapEvent();
}

void HexManager::ClearHexLight()
{
    NON_CONST_METHOD_HINT();

    std::memset(_hexLight, 0, _maxHexX * _maxHexY * sizeof(uchar) * 3);
}

void HexManager::PrepareLightToDraw()
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
        _sprMngr.PushRenderTarget(_rtLight);
        _sprMngr.ClearCurrentRenderTarget(0);
        FPoint offset(static_cast<float>(_rtScreenOx), static_cast<float>(_rtScreenOy));
        for (uint i = 0; i < _lightPointsCount; i++) {
            _sprMngr.DrawPoints(_lightPoints[i], RenderPrimitiveType::TriangleFan, &_settings.SpritesZoom, &offset, _effectMngr.Effects.Light);
        }
        _sprMngr.DrawPoints(_lightSoftPoints, RenderPrimitiveType::TriangleList, &_settings.SpritesZoom, &offset, _effectMngr.Effects.Light);
        _sprMngr.PopRenderTarget();
    }
}

void HexManager::MarkLight(ushort hx, ushort hy, uint inten)
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

void HexManager::MarkLightEndNeighbor(ushort hx, ushort hy, bool north_south, uint inten)
{
    auto& field = GetField(hx, hy);
    if (field.Flags.IsWall) {
        const int lt = field.Corner;
        if ((north_south && (lt == CORNER_NORTH_SOUTH || lt == CORNER_NORTH || lt == CORNER_WEST)) || (!north_south && (lt == CORNER_EAST_WEST || lt == CORNER_EAST)) || lt == CORNER_SOUTH) {
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

void HexManager::MarkLightEnd(ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint inten)
{
    auto is_wall = false;
    auto north_south = false;
    auto& field = GetField(to_hx, to_hy);
    if (field.Flags.IsWall) {
        is_wall = true;
        if (field.Corner == CORNER_NORTH_SOUTH || field.Corner == CORNER_NORTH || field.Corner == CORNER_WEST) {
            north_south = true;
        }
    }

    const int dir = _geomHelper.GetFarDir(from_hx, from_hy, to_hx, to_hy);
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

void HexManager::MarkLightStep(ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint inten)
{
    auto& field = GetField(to_hx, to_hy);
    if (field.Flags.IsWallTransp) {
        const auto north_south = (field.Corner == CORNER_NORTH_SOUTH || field.Corner == CORNER_NORTH || field.Corner == CORNER_WEST);
        const int dir = _geomHelper.GetFarDir(from_hx, from_hy, to_hx, to_hy);
        if (dir == 0 || (north_south && dir == 1) || (!north_south && (dir == 4 || dir == 5))) {
            MarkLight(to_hx, to_hy, inten);
        }
    }
    else {
        MarkLight(to_hx, to_hy, inten);
    }
}

void HexManager::TraceLight(ushort from_hx, ushort from_hy, ushort& hx, ushort& hy, int dist, uint inten)
{
    const auto [base_sx, base_sy] = GenericUtils::GetStepsXY(from_hx, from_hy, hx, hy);
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

void HexManager::ParseLightTriangleFan(LightSource& ls)
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
    base_x += _settings.MapHexWidth / 2;
    base_y += _settings.MapHexHeight / 2;

    _lightPointsCount++;
    if (_lightPoints.size() < _lightPointsCount) {
        _lightPoints.push_back(PrimitivePoints());
    }
    auto& points = _lightPoints[_lightPointsCount - 1];
    points.clear();
    points.reserve(3 + dist * _settings.MapDirCount);
    points.push_back({base_x, base_y, color, ls.OffsX, ls.OffsY}); // Center of light

    int hx_far = hx;
    int hy_far = hy;
    auto seek_start = true;
    ushort last_hx = -1;
    ushort last_hy = -1;

    for (auto i = 0, ii = (_settings.MapHexagonal ? 6 : 4); i < ii; i++) {
        const auto dir = static_cast<uchar>(_settings.MapHexagonal ? (i + 2) % 6 : ((i + 1) * 2) % 8);

        for (auto j = 0, jj = (_settings.MapHexagonal ? dist : dist * 2); j < jj; j++) {
            if (seek_start) {
                // Move to start position
                for (auto l = 0; l < dist; l++) {
                    _geomHelper.MoveHexByDirUnsafe(hx_far, hy_far, _settings.MapHexagonal ? 0 : 7);
                }
                seek_start = false;
                j = -1;
            }
            else {
                // Move to next hex
                _geomHelper.MoveHexByDirUnsafe(hx_far, hy_far, dir);
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
                    int a = alpha - _geomHelper.DistGame(hx, hy, hx_, hy_) * alpha / dist;
                    a = std::clamp(a, 0, alpha);
                    color = COLOR_RGBA(a, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
                }
                else {
                    color = COLOR_RGBA(0, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
                    ox = ls.OffsX;
                    oy = ls.OffsY;
                }
                const auto [x, y] = _geomHelper.GetHexInterval(hx, hy, hx_, hy_);
                points.push_back({base_x + x, base_y + y, color, ox, oy});
                last_hx = hx_;
                last_hy = hy_;
            }
        }
    }

    for (uint i = 1, j = static_cast<uint>(points.size()); i < j; i++) {
        auto& cur = points[i];
        auto& next = points[i >= points.size() - 1 ? 1 : i + 1];
        if (GenericUtils::DistSqrt(cur.PointX, cur.PointY, next.PointX, next.PointY) > static_cast<uint>(_settings.MapHexWidth)) {
            const auto dist_comp = (GenericUtils::DistSqrt(base_x, base_y, cur.PointX, cur.PointY) > GenericUtils::DistSqrt(base_x, base_y, next.PointX, next.PointY));
            _lightSoftPoints.push_back({next.PointX, next.PointY, next.PointColor, next.PointOffsX, next.PointOffsY});
            _lightSoftPoints.push_back({cur.PointX, cur.PointY, cur.PointColor, cur.PointOffsX, cur.PointOffsY});
            auto x = static_cast<float>(dist_comp ? next.PointX - cur.PointX : cur.PointX - next.PointX);
            auto y = static_cast<float>(dist_comp ? next.PointY - cur.PointY : cur.PointY - next.PointY);
            std::tie(x, y) = GenericUtils::ChangeStepsXY(x, y, dist_comp ? -2.5f : 2.5f);
            if (dist_comp) {
                _lightSoftPoints.push_back({cur.PointX + static_cast<int>(x), cur.PointY + static_cast<int>(y), cur.PointColor, cur.PointOffsX, cur.PointOffsY});
            }
            else {
                _lightSoftPoints.push_back({next.PointX + static_cast<int>(x), next.PointY + static_cast<int>(y), next.PointColor, next.PointOffsX, next.PointOffsY});
            }
        }
    }
}

void HexManager::RealRebuildLight()
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

void HexManager::CollectLightSources()
{
    _lightSources.clear();

    if (!IsMapLoaded()) {
        return;
    }

    // Scenery
    if (_mapperMode) {
        for (auto& item : _hexItems) {
            if (item->IsStatic() && item->GetIsLight()) {
                _lightSources.push_back({item->GetHexX(), item->GetHexY(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity()});
            }
        }
    }
    else {
        _lightSources = _lightSourcesScen;
    }

    // Items on ground
    for (auto& item : _hexItems) {
        if (!item->IsStatic() && item->GetIsLight()) {
            _lightSources.push_back({item->GetHexX(), item->GetHexY(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity()});
        }
    }

    // Items in critters slots
    for (auto& [id, cr] : _critters) {
        auto added = false;
        for (auto* item : cr->InvItems) {
            if (item->GetIsLight() && (item->GetCritSlot() != 0u)) {
                _lightSources.push_back({cr->GetHexX(), cr->GetHexY(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity(), &cr->SprOx, &cr->SprOy});
                added = true;
            }
        }

        // Default chosen light
        if (!_mapperMode) {
            if (cr->IsChosen() && !added) {
                _lightSources.push_back({cr->GetHexX(), cr->GetHexY(), _settings.ChosenLightColor, _settings.ChosenLightDistance, _settings.ChosenLightFlags, _settings.ChosenLightIntensity, &cr->SprOx, &cr->SprOy});
            }
        }
    }
}

auto HexManager::ProcessTileBorder(Field::Tile& tile, bool is_roof) -> bool
{
    const auto ox = (is_roof ? _settings.MapRoofOffsX : _settings.MapTileOffsX) + tile.OffsX;
    const auto oy = (is_roof ? _settings.MapRoofOffsY : _settings.MapTileOffsY) + tile.OffsY;
    return ProcessHexBorders(tile.Anim->GetSprId(0), ox, oy, false);
}

void HexManager::RebuildTiles()
{
    _tilesTree.Unvalidate();

    if (!_settings.ShowTile) {
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
                    auto& spr = _tilesTree.AddSprite(DRAW_ORDER_TILE + tile.Layer, hx, hy, tile.OffsX + _settings.MapTileOffsX, tile.OffsY + _settings.MapTileOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, tiles[i].IsSelected ? &SelectAlpha : nullptr, &_effectMngr.Effects.Tile, nullptr);
                    field.AddSpriteToChain(&spr);
                }
                else {
                    auto& spr = _tilesTree.AddSprite(DRAW_ORDER_TILE + tile.Layer, hx, hy, tile.OffsX + _settings.MapTileOffsX, tile.OffsY + _settings.MapTileOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, nullptr, &_effectMngr.Effects.Tile, nullptr);
                    field.AddSpriteToChain(&spr);
                }
            }
        }
        y2 += _wVisible;
    }

    // Sort
    _tilesTree.SortByMapPos();
}

void HexManager::RebuildRoof()
{
    _roofTree.Unvalidate();

    if (!_settings.ShowRoof) {
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
                        auto& spr = _roofTree.AddSprite(DRAW_ORDER_TILE + roof.Layer, hx, hy, roof.OffsX + _settings.MapRoofOffsX, roof.OffsY + _settings.MapRoofOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, roofs[i].IsSelected ? &SelectAlpha : &_settings.RoofAlpha, &_effectMngr.Effects.Roof, nullptr);
                        spr.SetEgg(EGG_ALWAYS);
                        field.AddSpriteToChain(&spr);
                    }
                    else {
                        auto& spr = _roofTree.AddSprite(DRAW_ORDER_TILE + roof.Layer, hx, hy, roof.OffsX + _settings.MapRoofOffsX, roof.OffsY + _settings.MapRoofOffsY, &field.ScrX, &field.ScrY, spr_id, nullptr, nullptr, nullptr, &_settings.RoofAlpha, &_effectMngr.Effects.Roof, nullptr);
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

void HexManager::SetSkipRoof(ushort hx, ushort hy)
{
    if (_roofSkip != GetField(hx, hy).RoofNum) {
        _roofSkip = GetField(hx, hy).RoofNum;
        if (_rainCapacity != 0) {
            RefreshMap();
        }
        else {
            RebuildRoof();
        }
    }
}

void HexManager::MarkRoofNum(int hxi, int hyi, short num)
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

    for (auto x = 0; x < _settings.MapRoofSkipSize; x++) {
        for (auto y = 0; y < _settings.MapRoofSkipSize; y++) {
            if (hxi + x >= 0 && hxi + x < _maxHexX && hyi + y >= 0 && hyi + y < _maxHexY) {
                GetField(static_cast<ushort>(hxi + x), static_cast<ushort>(hyi + y)).RoofNum = num;
            }
        }
    }

    MarkRoofNum(hxi + _settings.MapRoofSkipSize, hy, num);
    MarkRoofNum(hxi - _settings.MapRoofSkipSize, hy, num);
    MarkRoofNum(hxi, hyi + _settings.MapRoofSkipSize, num);
    MarkRoofNum(hxi, hyi - _settings.MapRoofSkipSize, num);
}

auto HexManager::IsVisible(uint spr_id, int ox, int oy) const -> bool
{
    if (spr_id == 0u) {
        return false;
    }

    const auto* si = _sprMngr.GetSpriteInfo(spr_id);
    if (si == nullptr) {
        return false;
    }

    const auto top = oy + si->OffsY - si->Height - (_settings.MapHexLineHeight * 2);
    const auto bottom = oy + si->OffsY + (_settings.MapHexLineHeight * 2);
    const auto left = ox + si->OffsX - si->Width / 2 - _settings.MapHexWidth;
    const auto right = ox + si->OffsX + si->Width / 2 + _settings.MapHexWidth;
    const auto zoomed_screen_height = static_cast<int>(std::ceil(static_cast<float>(_settings.ScreenHeight) * _settings.SpritesZoom));
    const auto zoomed_screen_width = static_cast<int>(std::ceil(static_cast<float>(_settings.ScreenWidth) * _settings.SpritesZoom));
    return !(top > zoomed_screen_height || bottom < 0 || left > zoomed_screen_width || right < 0);
}

void HexManager::ProcessHexBorders(ItemHexView* item)
{
    ProcessHexBorders(item->Anim->GetSprId(0), item->GetOffsetX(), item->GetOffsetY(), true);
}

auto HexManager::ProcessHexBorders(uint spr_id, int ox, int oy, bool resize_map) -> bool
{
    const auto* si = _sprMngr.GetSpriteInfo(spr_id);
    if (si == nullptr) {
        return false;
    }

    auto top = si->OffsY + oy - _hTop * _settings.MapHexLineHeight + (_settings.MapHexLineHeight * 2);
    if (top < 0) {
        top = 0;
    }

    auto bottom = si->Height - si->OffsY - oy - _hBottom * _settings.MapHexLineHeight + (_settings.MapHexLineHeight * 2);
    if (bottom < 0) {
        bottom = 0;
    }

    auto left = si->Width / 2 + si->OffsX + ox - _wLeft * _settings.MapHexWidth + _settings.MapHexWidth;
    if (left < 0) {
        left = 0;
    }

    auto right = si->Width / 2 - si->OffsX - ox - _wRight * _settings.MapHexWidth + _settings.MapHexWidth;
    if (right < 0) {
        right = 0;
    }

    if ((top != 0) || (bottom != 0) || (left != 0) || (right != 0)) {
        // Resize
        _hTop += top / _settings.MapHexLineHeight + ((top % _settings.MapHexLineHeight) != 0 ? 1 : 0);
        _hBottom += bottom / _settings.MapHexLineHeight + ((bottom % _settings.MapHexLineHeight) != 0 ? 1 : 0);
        _wLeft += left / _settings.MapHexWidth + ((left % _settings.MapHexWidth) != 0 ? 1 : 0);
        _wRight += right / _settings.MapHexWidth + ((right % _settings.MapHexWidth) != 0 ? 1 : 0);

        if (resize_map) {
            ResizeView();
            RefreshMap();
        }
        return true;
    }
    return false;
}

void HexManager::SwitchShowHex()
{
    _isShowHex = !_isShowHex;
    RefreshMap();
}

void HexManager::SwitchShowRain()
{
    _rainCapacity = (_rainCapacity != 0 ? 0 : 255);
    RefreshMap();
}

void HexManager::SetWeather(int time, uchar rain)
{
    _curMapTime = time;
    _rainCapacity = rain;
}

void HexManager::ResizeField(ushort w, ushort h)
{
    _maxHexX = w;
    _maxHexY = h;

    delete[] _hexField;
    _hexField = nullptr;
    delete[] _hexTrack;
    _hexTrack = nullptr;
    delete[] _hexLight;
    _hexLight = nullptr;

    if (w == 0u || h == 0u) {
        return;
    }

    const auto size = static_cast<size_t>(w) * static_cast<size_t>(h);
    _hexField = new Field[size];
    _hexTrack = new char[size];
    std::memset(_hexTrack, 0, size * sizeof(char));
    _hexLight = new uchar[size * 3u];
    std::memset(_hexLight, 0, size * 3u * sizeof(uchar));
}

void HexManager::ClearHexTrack()
{
    NON_CONST_METHOD_HINT();

    std::memset(_hexTrack, 0, _maxHexX * _maxHexY * sizeof(char));
}

void HexManager::SwitchShowTrack()
{
    _isShowTrack = !_isShowTrack;

    if (!_isShowTrack) {
        ClearHexTrack();
    }

    if (IsMapLoaded()) {
        RefreshMap();
    }
}

void HexManager::InitView(int cx, int cy)
{
    NON_CONST_METHOD_HINT();

    if (_settings.MapHexagonal) {
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

        auto xa = -(_wRight * _settings.MapHexWidth);
        auto xb = -(_settings.MapHexWidth / 2) - (_wRight * _settings.MapHexWidth);
        auto oy = -_settings.MapHexLineHeight * _hTop;
        const auto wx = static_cast<int>(static_cast<float>(_settings.ScreenWidth) * _settings.SpritesZoom);

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
                ox += _settings.MapHexWidth;
            }
            oy += _settings.MapHexLineHeight;
        }
    }
    else {
        // Calculate data
        const auto halfw = GetViewWidth() / 2 + _wRight;
        const auto halfh = GetViewHeight() / 2 + _hTop;
        auto basehx = cx - halfh / 2 - halfw;
        auto basehy = cy - halfh / 2 + halfw;
        auto y2 = 0;
        auto xa = -_settings.MapHexWidth * _wRight;
        auto xb = -_settings.MapHexWidth * _wRight - _settings.MapHexWidth / 2;
        auto y = -_settings.MapHexLineHeight * _hTop;
        const auto wx = static_cast<int>(static_cast<float>(_settings.ScreenWidth) * _settings.SpritesZoom);

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
                x += _settings.MapHexWidth;
            }

            if ((j & 1) != 0) {
                basehy++;
            }
            else {
                basehx++;
            }

            y += _settings.MapHexLineHeight;
            y2 += _wVisible;
        }
    }
}

void HexManager::ResizeView()
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

void HexManager::ChangeZoom(int zoom)
{
    if (!IsMapLoaded()) {
        return;
    }
    if (_settings.SpritesZoomMin == _settings.SpritesZoomMax) {
        return;
    }
    if (zoom == 0 && _settings.SpritesZoom == 1.0f) {
        return;
    }
    if (zoom > 0 && _settings.SpritesZoom >= std::min(_settings.SpritesZoomMax, MAX_ZOOM)) {
        return;
    }
    if (zoom < 0 && _settings.SpritesZoom <= std::max(_settings.SpritesZoomMin, MIN_ZOOM)) {
        return;
    }

    // Check screen blockers
    if (_settings.ScrollCheck && (zoom > 0 || (zoom == 0 && _settings.SpritesZoom < 1.0f))) {
        for (auto x = -1; x <= 1; x++) {
            for (auto y = -1; y <= 1; y++) {
                if (((x != 0) || (y != 0)) && ScrollCheck(x, y)) {
                    return;
                }
            }
        }
    }

    if (zoom != 0 || _settings.SpritesZoom < 1.0f) {
        const auto old_zoom = _settings.SpritesZoom;
        const auto w = static_cast<float>(_settings.ScreenWidth / _settings.MapHexWidth + ((_settings.ScreenWidth % _settings.MapHexWidth) != 0 ? 1 : 0));
        _settings.SpritesZoom = (w * _settings.SpritesZoom + (zoom >= 0 ? 2.0f : -2.0f)) / w;

        if (_settings.SpritesZoom < std::max(_settings.SpritesZoomMin, MIN_ZOOM) || _settings.SpritesZoom > std::min(_settings.SpritesZoomMax, MAX_ZOOM)) {
            _settings.SpritesZoom = old_zoom;
            return;
        }
    }
    else {
        _settings.SpritesZoom = 1.0f;
    }

    ResizeView();
    RefreshMap();

    if (zoom == 0 && _settings.SpritesZoom != 1.0f) {
        ChangeZoom(0);
    }
}

void HexManager::GetScreenHexes(int& sx, int& sy) const
{
    sx = _screenHexX;
    sy = _screenHexY;
}

void HexManager::GetHexCurrentPosition(ushort hx, ushort hy, int& x, int& y) const
{
    auto& center_hex = _viewField[_hVisible / 2 * _wVisible + _wVisible / 2];
    const auto center_hx = center_hex.HexX;
    const auto center_hy = center_hex.HexY;
    auto [xx, yy] = _geomHelper.GetHexInterval(center_hx, center_hy, hx, hy);

    x += center_hex.ScrX;
    y += center_hex.ScrY;
    x += xx;
    y += yy;
}

void HexManager::DrawMap()
{
    // Prepare light
    PrepareLightToDraw();

    // Prepare fog
    PrepareFogToDraw();

    // Prerendered offsets
    const auto ox = static_cast<int>(_rtScreenOx) - static_cast<int>(std::round(static_cast<float>(_settings.ScrOx) / _settings.SpritesZoom));
    const auto oy = static_cast<int>(_rtScreenOy) - static_cast<int>(std::round(static_cast<float>(_settings.ScrOy) / _settings.SpritesZoom));
    auto prerenderedRect = IRect(ox, oy, ox + _settings.ScreenWidth, oy + _settings.ScreenHeight);

    // Separate render target
    if (_rtMap != nullptr) {
        _sprMngr.PushRenderTarget(_rtMap);
        _sprMngr.ClearCurrentRenderTarget(0);
    }

    // Tiles
    if (_settings.ShowTile) {
        _sprMngr.DrawSprites(_tilesTree, false, false, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END, false, 0, 0);
    }

    // Flat sprites
    _sprMngr.DrawSprites(_mainTree, true, false, DRAW_ORDER_FLAT, DRAW_ORDER_LIGHT - 1, false, 0, 0);

    // Light
    if (_rtLight != nullptr) {
        _sprMngr.DrawRenderTarget(_rtLight, true, &prerenderedRect, nullptr);
    }

    // Cursor flat
    DrawCursor(_cursorPrePic->GetCurSprId(_gameTime.GameTick()));

    // Sprites
    _sprMngr.DrawSprites(_mainTree, true, true, DRAW_ORDER_LIGHT, DRAW_ORDER_LAST, false, 0, 0);

    // Roof
    if (_settings.ShowRoof) {
        _sprMngr.DrawSprites(_roofTree, false, true, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END, false, 0, 0);
        if (_rainCapacity != 0) {
            _sprMngr.DrawSprites(_roofRainTree, false, false, DRAW_ORDER_RAIN, DRAW_ORDER_RAIN, false, 0, 0);
        }
    }

    // Contours
    _sprMngr.DrawContours();

    // Fog
    if (_rtFog != nullptr) {
        _sprMngr.DrawRenderTarget(_rtFog, true, &prerenderedRect, nullptr);
    }

    // Cursor
    DrawCursor(_cursorPostPic->GetCurSprId(_gameTime.GameTick()));
    if (_drawCursorX < 0) {
        DrawCursor(_cursorXPic->GetCurSprId(_gameTime.GameTick()));
    }
    else if (_drawCursorX > 0) {
        DrawCursor(_str("{}", _drawCursorX).c_str());
    }

    // Draw map from render target
    if (_rtMap != nullptr) {
        _sprMngr.PopRenderTarget();
        _sprMngr.DrawRenderTarget(_rtMap, false, nullptr, nullptr);
    }
}

void HexManager::SetFog(PrimitivePoints& look_points, PrimitivePoints& shoot_points, short* offs_x, short* offs_y)
{
    if (_rtFog == nullptr) {
        return;
    }

    _fogOffsX = offs_x;
    _fogOffsY = offs_y;
    _fogLastOffsX = static_cast<short>(_fogOffsX != nullptr ? *_fogOffsX : 0);
    _fogLastOffsY = static_cast<short>(_fogOffsY != nullptr ? *_fogOffsY : 0);
    _fogForceRerender = true;
    _fogLookPoints = look_points;
    _fogShootPoints = shoot_points;
}

void HexManager::PrepareFogToDraw()
{
    if (_rtFog == nullptr) {
        return;
    }
    if (!_fogForceRerender && ((_fogOffsX != nullptr) && *_fogOffsX == _fogLastOffsX && *_fogOffsY == _fogLastOffsY)) {
        return;
    }
    if (_fogOffsX != nullptr) {
        _fogLastOffsX = *_fogOffsX;
        _fogLastOffsY = *_fogOffsY;
    }
    _fogForceRerender = false;

    FPoint offset(static_cast<float>(_rtScreenOx), static_cast<float>(_rtScreenOy));
    _sprMngr.PushRenderTarget(_rtFog);
    _sprMngr.ClearCurrentRenderTarget(0);
    _sprMngr.DrawPoints(_fogLookPoints, RenderPrimitiveType::TriangleFan, &_settings.SpritesZoom, &offset, _effectMngr.Effects.Fog);
    _sprMngr.DrawPoints(_fogShootPoints, RenderPrimitiveType::TriangleFan, &_settings.SpritesZoom, &offset, _effectMngr.Effects.Fog);
    _sprMngr.PopRenderTarget();
}

auto HexManager::Scroll() -> bool
{
    if (!IsMapLoaded()) {
        return false;
    }

    // Scroll delay
    auto time_k = 1.0f;
    if (_settings.ScrollDelay != 0u) {
        const auto tick = _gameTime.FrameTick();
        static auto last_tick = tick;
        if (tick - last_tick < _settings.ScrollDelay / 2) {
            return false;
        }
        time_k = static_cast<float>(tick - last_tick) / static_cast<float>(_settings.ScrollDelay);
        last_tick = tick;
    }

    const auto is_scroll = (_settings.ScrollMouseLeft || _settings.ScrollKeybLeft || _settings.ScrollMouseRight || _settings.ScrollKeybRight || _settings.ScrollMouseUp || _settings.ScrollKeybUp || _settings.ScrollMouseDown || _settings.ScrollKeybDown);
    auto scr_ox = _settings.ScrOx;
    auto scr_oy = _settings.ScrOy;
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
            const auto [ox, oy] = _geomHelper.GetHexInterval(AutoScroll.CritterLastHexX, AutoScroll.CritterLastHexY, cr->GetHexX(), cr->GetHexY());
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

        if (xscroll > _settings.MapHexWidth) {
            xscroll = _settings.MapHexWidth;
            AutoScroll.OffsXStep = static_cast<float>(_settings.MapHexWidth);
        }
        if (xscroll < -_settings.MapHexWidth) {
            xscroll = -_settings.MapHexWidth;
            AutoScroll.OffsXStep = -static_cast<float>(_settings.MapHexWidth);
        }
        if (yscroll > _settings.MapHexLineHeight * 2) {
            yscroll = _settings.MapHexLineHeight * 2;
            AutoScroll.OffsYStep = static_cast<float>(_settings.MapHexLineHeight * 2);
        }
        if (yscroll < -_settings.MapHexLineHeight * 2) {
            yscroll = -_settings.MapHexLineHeight * 2;
            AutoScroll.OffsYStep = -static_cast<float>(_settings.MapHexLineHeight * 2);
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

        if (_settings.ScrollMouseLeft || _settings.ScrollKeybLeft) {
            xscroll += 1;
        }
        if (_settings.ScrollMouseRight || _settings.ScrollKeybRight) {
            xscroll -= 1;
        }
        if (_settings.ScrollMouseUp || _settings.ScrollKeybUp) {
            yscroll += 1;
        }
        if (_settings.ScrollMouseDown || _settings.ScrollKeybDown) {
            yscroll -= 1;
        }
        if ((xscroll == 0) && (yscroll == 0)) {
            return false;
        }

        xscroll = static_cast<int>(static_cast<float>(xscroll * _settings.ScrollStep) * _settings.SpritesZoom * time_k);
        yscroll = static_cast<int>(static_cast<float>(yscroll * (_settings.ScrollStep * (_settings.MapHexLineHeight * 2) / _settings.MapHexWidth)) * _settings.SpritesZoom * time_k);
    }
    scr_ox += xscroll;
    scr_oy += yscroll;

    if (_settings.ScrollCheck) {
        auto xmod = 0;
        auto ymod = 0;
        if (scr_ox - _settings.ScrOx > 0) {
            xmod = 1;
        }
        if (scr_ox - _settings.ScrOx < 0) {
            xmod = -1;
        }
        if (scr_oy - _settings.ScrOy > 0) {
            ymod = -1;
        }
        if (scr_oy - _settings.ScrOy < 0) {
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
    if (scr_ox >= _settings.MapHexWidth) {
        xmod = 1;
        scr_ox -= _settings.MapHexWidth;
        if (scr_ox > _settings.MapHexWidth) {
            scr_ox = _settings.MapHexWidth;
        }
    }
    else if (scr_ox <= -_settings.MapHexWidth) {
        xmod = -1;
        scr_ox += _settings.MapHexWidth;
        if (scr_ox < -_settings.MapHexWidth) {
            scr_ox = -_settings.MapHexWidth;
        }
    }
    if (scr_oy >= (_settings.MapHexLineHeight * 2)) {
        ymod = -2;
        scr_oy -= (_settings.MapHexLineHeight * 2);
        if (scr_oy > (_settings.MapHexLineHeight * 2)) {
            scr_oy = (_settings.MapHexLineHeight * 2);
        }
    }
    else if (scr_oy <= -(_settings.MapHexLineHeight * 2)) {
        ymod = 2;
        scr_oy += (_settings.MapHexLineHeight * 2);
        if (scr_oy < -(_settings.MapHexLineHeight * 2)) {
            scr_oy = -(_settings.MapHexLineHeight * 2);
        }
    }

    _settings.ScrOx = scr_ox;
    _settings.ScrOy = scr_oy;

    if ((xmod != 0) || (ymod != 0)) {
        RebuildMapOffset(xmod, ymod);

        if (_settings.ScrollCheck) {
            if (_settings.ScrOx > 0 && ScrollCheck(1, 0)) {
                _settings.ScrOx = 0;
            }
            else if (_settings.ScrOx < 0 && ScrollCheck(-1, 0)) {
                _settings.ScrOx = 0;
            }
            if (_settings.ScrOy > 0 && ScrollCheck(0, -1)) {
                _settings.ScrOy = 0;
            }
            else if (_settings.ScrOy < 0 && ScrollCheck(0, 1)) {
                _settings.ScrOy = 0;
            }
        }
    }

    if (!_mapperMode) {
        const auto final_scr_ox = _settings.ScrOx - prev_scr_ox + xmod * _settings.MapHexWidth;
        const auto final_scr_oy = _settings.ScrOy - prev_scr_oy + (-ymod / 2) * (_settings.MapHexLineHeight * 2);
        if ((final_scr_ox != 0) || (final_scr_oy != 0)) {
            _scriptSys.ScreenScrollEvent(final_scr_ox, final_scr_oy);
        }
    }

    return (xmod != 0) || (ymod != 0);
}

auto HexManager::ScrollCheckPos(int (&positions)[4], int dir1, int dir2) -> bool
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

        _geomHelper.MoveHexByDir(hx, hy, dir1, _maxHexX, _maxHexY);
        if (GetField(hx, hy).Flags.ScrollBlock) {
            return true;
        }

        if (dir2 >= 0) {
            _geomHelper.MoveHexByDir(hx, hy, dir2, _maxHexX, _maxHexY);
            if (GetField(hx, hy).Flags.ScrollBlock) {
                return true;
            }
        }
    }
    return false;
}

auto HexManager::ScrollCheck(int xmod, int ymod) -> bool
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
    if (!_settings.MapHexagonal) {
        dirs[0] = 7; // Square
        dirs[1] = -1;
        dirs[2] = 3;
        dirs[3] = -1;
        dirs[4] = 5;
        dirs[5] = -1;
        dirs[6] = 1;
        dirs[7] = -1;
    }

    if (_settings.MapHexagonal) {
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
    if (_settings.SpritesZoom != 1.0f) {
        for (auto& i : positions_left) {
            i--;
        }
        for (auto& i : positions_right) {
            i++;
        }

        if (_settings.MapHexagonal) {
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

void HexManager::ScrollToHex(int hx, int hy, float speed, bool can_stop)
{
    auto sx = 0;
    auto sy = 0;
    GetScreenHexes(sx, sy);

    const auto [ox, oy] = _geomHelper.GetHexInterval(sx, sy, hx, hy);

    AutoScroll.Active = false;
    ScrollOffset(ox, oy, speed, can_stop);
}

void HexManager::ScrollOffset(int ox, int oy, float speed, bool can_stop)
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

void HexManager::SetCritter(CritterView* cr)
{
    if (!IsMapLoaded()) {
        return;
    }

    const auto hx = cr->GetHexX();
    const auto hy = cr->GetHexY();
    auto& field = GetField(hx, hy);

    if (cr->IsDead()) {
        field.AddDeadCrit(cr);
    }
    else {
        if (field.Crit != nullptr && field.Crit != cr) {
            WriteLog("Hex {} {} busy, critter old {}, new {}.\n", hx, hy, field.Crit->GetId(), cr->GetId());
            return;
        }

        field.Crit = cr;
        SetMultihex(cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), true);
    }

    if (IsHexToDraw(hx, hy) && cr->Visible) {
        auto& spr = _mainTree.InsertSprite(EvaluateCritterDrawOrder(cr), hx, hy, _settings.MapHexWidth / 2, _settings.MapHexHeight / 2, &field.ScrX, &field.ScrY, 0, &cr->SprId, &cr->SprOx, &cr->SprOy, &cr->Alpha, &cr->DrawEffect, &cr->SprDrawValid);
        spr.SetLight(CORNER_EAST_WEST, _hexLight, _maxHexX, _maxHexY);
        cr->SprDraw = &spr;

        cr->SetSprRect();
        cr->FixLastHexes();

        auto contour = 0;
        if (cr->GetId() == _critterContourCrId) {
            contour = _critterContour;
        }
        else if (!cr->IsDead() && !cr->IsChosen()) {
            contour = _crittersContour;
        }
        spr.SetContour(contour, cr->ContourColor);

        field.AddSpriteToChain(&spr);
    }

    field.ProcessCache();
}

void HexManager::RemoveCritter(CritterView* cr)
{
    if (!IsMapLoaded()) {
        return;
    }

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

auto HexManager::GetCritter(uint crid) -> CritterView*
{
    if (crid == 0u) {
        return nullptr;
    }

    const auto it = _critters.find(crid);
    return it != _critters.end() ? it->second : nullptr;
}

auto HexManager::GetChosen() -> CritterView*
{
    if (_chosenId == 0u) {
        return nullptr;
    }
    const auto it = _critters.find(_chosenId);
    return it != _critters.end() ? it->second : nullptr;
}

void HexManager::AddCritter(CritterView* cr)
{
    if (_critters.count(cr->GetId()) != 0u) {
        return;
    }
    _critters.insert(std::make_pair(cr->GetId(), cr));
    if (cr->IsChosen()) {
        _chosenId = cr->GetId();
    }
    SetCritter(cr);
}

void HexManager::DeleteCritter(uint crid)
{
    const auto it = _critters.find(crid);
    if (it == _critters.end()) {
        return;
    }
    auto* cr = it->second;
    if (cr->IsChosen()) {
        _chosenId = 0;
    }
    RemoveCritter(cr);
    cr->DeleteAllItems();
    cr->IsDestroyed = true;
    _scriptSys.RemoveEntity(cr);
    cr->Release();
    _critters.erase(it);
}

void HexManager::DeleteCritters()
{
    for (auto& [id, cr] : _critters) {
        RemoveCritter(cr);
        cr->DeleteAllItems();
        cr->IsDestroyed = true;
        _scriptSys.RemoveEntity(cr);
        cr->Release();
    }

    _critters.clear();
    _chosenId = 0;
}

void HexManager::GetCritters(ushort hx, ushort hy, vector<CritterView*>& crits, uchar find_type)
{
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
}

void HexManager::SetCritterContour(uint crid, int contour)
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
        if ((cr != nullptr) && cr->SprDrawValid) {
            cr->SprDraw->SetContour(contour);
        }
    }
}

void HexManager::SetCrittersContour(int contour)
{
    if (_crittersContour == contour) {
        return;
    }

    _crittersContour = contour;

    for (auto& [id, cr] : _critters) {
        if (!cr->IsChosen() && cr->SprDrawValid && !cr->IsDead() && cr->GetId() != _critterContourCrId) {
            cr->SprDraw->SetContour(contour);
        }
    }
}

auto HexManager::TransitCritter(CritterView* cr, ushort hx, ushort hy, bool animate, bool force) -> bool
{
    if (!IsMapLoaded() || hx >= _maxHexX || hy >= _maxHexY) {
        return false;
    }
    if (cr->GetHexX() == hx && cr->GetHexY() == hy) {
        return true;
    }

    // Dead transit
    if (cr->IsDead()) {
        RemoveCritter(cr);
        cr->SetHexX(hx);
        cr->SetHexY(hy);
        SetCritter(cr);

        if (cr->IsChosen() || cr->IsHaveLightSources()) {
            RebuildLight();
        }
        return true;
    }

    // Not dead transit
    auto& field = GetField(hx, hy);
    if (field.Crit != nullptr) // Hex busy
    {
        // Try move critter on busy hex in previous position
        if (force && field.Crit->IsLastHexes()) {
            const auto [last_hx, last_hy] = field.Crit->PopLastHex();
            TransitCritter(field.Crit, last_hx, last_hy, false, true);
        }
        if (field.Crit != nullptr) {
            // Try move in next game cycle
            return false;
        }
    }

    RemoveCritter(cr);

    const auto old_hx = cr->GetHexX();
    const auto old_hy = cr->GetHexY();

    cr->SetHexX(hx);
    cr->SetHexY(hy);

    const auto dir = _geomHelper.GetFarDir(old_hx, old_hy, hx, hy);

    if (animate) {
        cr->Move(dir);
        if (_geomHelper.DistGame(old_hx, old_hy, hx, hy) > 1) {
            if (_geomHelper.MoveHexByDir(hx, hy, _geomHelper.ReverseDir(dir), _maxHexX, _maxHexY)) {
                const auto [ox, oy] = _geomHelper.GetHexInterval(hx, hy, old_hx, old_hy);
                cr->AddOffsExt(static_cast<short>(ox), static_cast<short>(oy));
            }
        }
    }
    else {
        const auto [ox, oy] = _geomHelper.GetHexInterval(hx, hy, old_hx, old_hy);
        cr->AddOffsExt(static_cast<short>(ox), static_cast<short>(oy));
    }

    SetCritter(cr);
    return true;
}

void HexManager::SetMultihex(ushort hx, ushort hy, uint multihex, bool set)
{
    if (IsMapLoaded() && (multihex != 0u)) {
        const auto [sx, sy] = _geomHelper.GetHexOffsets((hx % 2) != 0);

        for (uint i = 0, j = GenericUtils::NumericalNumber(multihex) * _settings.MapDirCount; i < j; i++) {
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

auto HexManager::GetHexPixel(int x, int y, ushort& hx, ushort& hy) const -> bool
{
    if (!IsMapLoaded()) {
        return false;
    }

    const auto xf = static_cast<float>(x) - static_cast<float>(_settings.ScrOx) / _settings.SpritesZoom;
    const auto yf = static_cast<float>(y) - static_cast<float>(_settings.ScrOy) / _settings.SpritesZoom;
    const auto ox = static_cast<float>(_settings.MapHexWidth) / _settings.SpritesZoom;
    const auto oy = static_cast<float>(_settings.MapHexHeight) / _settings.SpritesZoom;
    auto y2 = 0;
    auto vpos = 0;

    for (auto ty = 0; ty < _hVisible; ty++) {
        for (auto tx = 0; tx < _wVisible; tx++) {
            vpos = y2 + tx;

            const auto x_ = _viewField[vpos].ScrXf / _settings.SpritesZoom;
            const auto y_ = _viewField[vpos].ScrYf / _settings.SpritesZoom;

            if (xf >= x_ && xf < x_ + ox && yf >= y_ && yf < y_ + oy) {
                auto hx_ = _viewField[vpos].HexX;
                auto hy_ = _viewField[vpos].HexY;

                // Correct with hex color mask
                if (_picHexMask != nullptr) {
                    const auto r = (_sprMngr.GetPixColor(_picHexMask->Ind[0], static_cast<int>(xf - x_), static_cast<int>(yf - y_), true) & 0x00FF0000) >> 16;
                    if (r == 50) {
                        _geomHelper.MoveHexByDirUnsafe(hx_, hy_, _settings.MapHexagonal ? 5u : 6u);
                    }
                    else if (r == 100) {
                        _geomHelper.MoveHexByDirUnsafe(hx_, hy_, _settings.MapHexagonal ? 0u : 0u);
                    }
                    else if (r == 150) {
                        _geomHelper.MoveHexByDirUnsafe(hx_, hy_, _settings.MapHexagonal ? 3u : 4u);
                    }
                    else if (r == 200) {
                        _geomHelper.MoveHexByDirUnsafe(hx_, hy_, _settings.MapHexagonal ? 2u : 2u);
                    }
                }

                if (hx_ >= 0 && hy_ >= 0 && hx_ < _maxHexX && hy_ < _maxHexY) {
                    hx = hx_;
                    hy = hy_;
                    return true;
                }
            }
        }
        y2 += _wVisible;
    }

    return false;
}

auto HexManager::GetItemPixel(int x, int y, bool& item_egg) -> ItemHexView*
{
    if (!IsMapLoaded()) {
        return nullptr;
    }

    vector<ItemHexView*> pix_item;
    vector<ItemHexView*> pix_item_egg;
    const auto is_egg = _sprMngr.IsEggTransp(x, y);

    for (auto* item : _hexItems) {
        const auto hx = item->GetHexX();
        const auto hy = item->GetHexY();

        if (item->IsFinishing() || !item->SprDrawValid) {
            continue;
        }

        if (!_mapperMode) {
            if (item->GetIsHidden() || item->GetIsHiddenPicture() || item->IsFullyTransparent()) {
                continue;
            }
            if (item->IsScenery() && !_settings.ShowScen) {
                continue;
            }
            if (!item->IsAnyScenery() && !_settings.ShowItem) {
                continue;
            }
            if (item->IsWall() && !_settings.ShowWall) {
                continue;
            }
        }
        else {
            const auto is_fast = _fastPids.count(item->GetProtoId()) != 0;
            if (item->IsScenery() && !_settings.ShowScen && !is_fast) {
                continue;
            }
            if (!item->IsAnyScenery() && !_settings.ShowItem && !is_fast) {
                continue;
            }
            if (item->IsWall() && !_settings.ShowWall && !is_fast) {
                continue;
            }
            if (!_settings.ShowFast && is_fast) {
                continue;
            }
            if (_ignorePids.count(item->GetProtoId()) != 0u) {
                continue;
            }
        }

        const auto* si = _sprMngr.GetSpriteInfo(item->SprId);
        if (si == nullptr) {
            continue;
        }

        const auto l = static_cast<int>((*item->HexScrX + item->ScrX + si->OffsX + (_settings.MapHexWidth / 2) + _settings.ScrOx - si->Width / 2) / _settings.SpritesZoom);
        const auto r = static_cast<int>((*item->HexScrX + item->ScrX + si->OffsX + (_settings.MapHexWidth / 2) + _settings.ScrOx + si->Width / 2) / _settings.SpritesZoom);
        const auto t = static_cast<int>((*item->HexScrY + item->ScrY + si->OffsY + (_settings.MapHexHeight / 2) + _settings.ScrOy - si->Height) / _settings.SpritesZoom);
        const auto b = static_cast<int>((*item->HexScrY + item->ScrY + si->OffsY + (_settings.MapHexHeight / 2) + _settings.ScrOy) / _settings.SpritesZoom);

        if (x >= l && x <= r && y >= t && y <= b) {
            auto* spr = item->SprDraw->GetIntersected(x - l, y - t);
            if (spr != nullptr) {
                item->SprTemp = spr;
                if (is_egg && _sprMngr.CompareHexEgg(hx, hy, item->GetEggType())) {
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

auto HexManager::GetCritterPixel(int x, int y, bool ignore_dead_and_chosen) -> CritterView*
{
    if (!IsMapLoaded() || !_settings.ShowCrit) {
        return nullptr;
    }

    vector<CritterView*> crits;
    for (auto& [id, cr] : _critters) {
        if (!cr->Visible || cr->IsFinishing() || !cr->SprDrawValid) {
            continue;
        }
        if (ignore_dead_and_chosen && (cr->IsDead() || cr->IsChosen())) {
            continue;
        }

        const auto l = static_cast<int>(static_cast<float>(cr->DRect.Left + _settings.ScrOx) / _settings.SpritesZoom);
        const auto r = static_cast<int>(static_cast<float>(cr->DRect.Right + _settings.ScrOx) / _settings.SpritesZoom);
        const auto t = static_cast<int>(static_cast<float>(cr->DRect.Top + _settings.ScrOy) / _settings.SpritesZoom);
        const auto b = static_cast<int>(static_cast<float>(cr->DRect.Bottom + _settings.ScrOy) / _settings.SpritesZoom);
        if (x >= l && x <= r && y >= t && y <= b) {
            if (_sprMngr.IsPixNoTransp(cr->SprId, x - l, y - t, true)) {
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

void HexManager::GetSmthPixel(int x, int y, ItemHexView*& item, CritterView*& cr)
{
    auto item_egg = false;
    item = GetItemPixel(x, y, item_egg);
    cr = GetCritterPixel(x, y, false);

    if (cr != nullptr && item != nullptr) {
        if (item->IsTransparent() || item_egg || item->SprDraw->TreeIndex <= cr->SprDraw->TreeIndex) {
            item = nullptr;
        }
        else {
            cr = nullptr;
        }
    }
}

auto HexManager::FindPath(CritterView* cr, ushort start_x, ushort start_y, ushort& end_x, ushort& end_y, vector<uchar>& steps, int cut) -> bool
{
#define GRID(x, y) _findPathGrid[((MAX_FIND_PATH + 1) + (y)-grid_oy) * (MAX_FIND_PATH * 2 + 2) + ((MAX_FIND_PATH + 1) + (x)-grid_ox)]

    if (_findPathGrid == nullptr) {
        _findPathGrid = new short[(MAX_FIND_PATH * 2 + 2) * (MAX_FIND_PATH * 2 + 2)];
    }

    if (!IsMapLoaded()) {
        return false;
    }
    if (start_x == end_x && start_y == end_y) {
        return true;
    }

    short numindex = 1;
    std::memset(_findPathGrid, 0, (MAX_FIND_PATH * 2 + 2) * (MAX_FIND_PATH * 2 + 2) * sizeof(short));

    auto grid_ox = start_x;
    auto grid_oy = start_y;
    GRID(start_x, start_y) = numindex;

    vector<pair<ushort, ushort>> coords;
    coords.reserve(MAX_FIND_PATH);
    coords.push_back(std::make_pair(start_x, start_y));

    auto mh = (cr != nullptr ? cr->GetMultihex() : 0);
    auto p = 0;
    auto find_ok = false;
    while (!find_ok) {
        if (++numindex > MAX_FIND_PATH) {
            return false;
        }

        auto p_togo = static_cast<int>(coords.size()) - p;
        if (p_togo == 0) {
            return false;
        }

        for (auto i = 0; i < p_togo && !find_ok; ++i, ++p) {
            int hx = coords[p].first;
            int hy = coords[p].second;

            const auto [sx, sy] = _geomHelper.GetHexOffsets((hx % 2) != 0);

            for (const auto j : xrange(_settings.MapDirCount)) {
                auto nx = hx + sx[j];
                auto ny = hy + sy[j];
                if (nx < 0 || ny < 0 || nx >= _maxHexX || ny >= _maxHexY || GRID(nx, ny)) {
                    continue;
                }

                GRID(nx, ny) = -1;

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
                        _geomHelper.MoveHexByDirUnsafe(nx_, ny_, static_cast<uchar>(j));
                    }
                    if (nx_ < 0 || ny_ < 0 || nx_ >= _maxHexX || ny_ >= _maxHexY) {
                        continue;
                    }
                    if (GetField(static_cast<ushort>(nx_), static_cast<ushort>(ny_)).Flags.IsNotPassed) {
                        continue;
                    }

                    // Clock wise hexes
                    auto is_square_corner = (!_settings.MapHexagonal && (j % 2) != 0);
                    auto steps_count = (is_square_corner ? mh * 2 : mh);
                    auto not_passed = false;
                    auto dir_ = (_settings.MapHexagonal ? ((j + 2) % 6) : ((j + 2) % 8));
                    if (is_square_corner) {
                        dir_ = (dir_ + 1) % 8;
                    }

                    auto nx_2 = nx_;
                    auto ny_2 = ny_;
                    for (uint k = 0; k < steps_count && !not_passed; k++) {
                        _geomHelper.MoveHexByDirUnsafe(nx_2, ny_2, static_cast<uchar>(dir_));
                        not_passed = GetField(static_cast<ushort>(nx_2), static_cast<ushort>(ny_2)).Flags.IsNotPassed;
                    }
                    if (not_passed) {
                        continue;
                    }

                    // Counter clock wise hexes
                    dir_ = (_settings.MapHexagonal ? (j + 4) % 6 : (j + 6) % 8);
                    if (is_square_corner) {
                        dir_ = (dir_ + 7) % 8;
                    }

                    nx_2 = nx_;
                    ny_2 = ny_;
                    for (uint k = 0; k < steps_count && !not_passed; k++) {
                        _geomHelper.MoveHexByDirUnsafe(nx_2, ny_2, static_cast<uchar>(dir_));
                        not_passed = GetField(static_cast<ushort>(nx_2), static_cast<ushort>(ny_2)).Flags.IsNotPassed;
                    }
                    if (not_passed) {
                        continue;
                    }
                }

                GRID(nx, ny) = numindex;
                coords.push_back(std::make_pair(nx, ny));

                if (cut >= 0 && _geomHelper.CheckDist(static_cast<ushort>(nx), static_cast<ushort>(ny), end_x, end_y, cut)) {
                    end_x = static_cast<ushort>(nx);
                    end_y = static_cast<ushort>(ny);
                    return true;
                }

                if (cut < 0 && nx == end_x && ny == end_y) {
                    find_ok = true;
                    break;
                }
            }
        }
    }
    if (cut >= 0) {
        return false;
    }

    int x1 = coords.back().first;
    int y1 = coords.back().second;

    RUNTIME_ASSERT(numindex > 0);
    steps.resize(static_cast<size_t>(numindex - 1));

    // From end
    if (_settings.MapHexagonal) {
        static auto switcher = false;
        if (!_settings.MapSmoothPath) {
            switcher = false;
        }

        while (numindex > 1) {
            if (_settings.MapSmoothPath && ((numindex & 1) != 0)) {
                switcher = !switcher;
            }
            numindex--;

            if (switcher) {
                if ((x1 & 1) != 0) {
                    if (GRID(x1 - 1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 3;
                        x1--;
                        y1--;
                        continue;
                    } // 0
                    if (GRID(x1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 2;
                        y1--;
                        continue;
                    } // 5
                    if (GRID(x1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 5;
                        y1++;
                        continue;
                    } // 2
                    if (GRID(x1 + 1, y1) == numindex) {
                        steps[numindex - 1] = 0;
                        x1++;
                        continue;
                    } // 3
                    if (GRID(x1 - 1, y1) == numindex) {
                        steps[numindex - 1] = 4;
                        x1--;
                        continue;
                    } // 1
                    if (GRID(x1 + 1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 1;
                        x1++;
                        y1--;
                        continue;
                    } // 4
                }
                else {
                    if (GRID(x1 - 1, y1) == numindex) {
                        steps[numindex - 1] = 3;
                        x1--;
                        continue;
                    } // 0
                    if (GRID(x1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 2;
                        y1--;
                        continue;
                    } // 5
                    if (GRID(x1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 5;
                        y1++;
                        continue;
                    } // 2
                    if (GRID(x1 + 1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 0;
                        x1++;
                        y1++;
                        continue;
                    } // 3
                    if (GRID(x1 - 1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 4;
                        x1--;
                        y1++;
                        continue;
                    } // 1
                    if (GRID(x1 + 1, y1) == numindex) {
                        steps[numindex - 1] = 1;
                        x1++;
                        continue;
                    } // 4
                }
            }
            else {
                if ((x1 & 1) != 0) {
                    if (GRID(x1 - 1, y1) == numindex) {
                        steps[numindex - 1] = 4;
                        x1--;
                        continue;
                    } // 1
                    if (GRID(x1 + 1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 1;
                        x1++;
                        y1--;
                        continue;
                    } // 4
                    if (GRID(x1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 2;
                        y1--;
                        continue;
                    } // 5
                    if (GRID(x1 - 1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 3;
                        x1--;
                        y1--;
                        continue;
                    } // 0
                    if (GRID(x1 + 1, y1) == numindex) {
                        steps[numindex - 1] = 0;
                        x1++;
                        continue;
                    } // 3
                    if (GRID(x1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 5;
                        y1++;
                        continue;
                    } // 2
                }
                else {
                    if (GRID(x1 - 1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 4;
                        x1--;
                        y1++;
                        continue;
                    } // 1
                    if (GRID(x1 + 1, y1) == numindex) {
                        steps[numindex - 1] = 1;
                        x1++;
                        continue;
                    } // 4
                    if (GRID(x1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 2;
                        y1--;
                        continue;
                    } // 5
                    if (GRID(x1 - 1, y1) == numindex) {
                        steps[numindex - 1] = 3;
                        x1--;
                        continue;
                    } // 0
                    if (GRID(x1 + 1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 0;
                        x1++;
                        y1++;
                        continue;
                    } // 3
                    if (GRID(x1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 5;
                        y1++;
                        continue;
                    } // 2
                }
            }
            return false;
        }
    }
    else {
        // Smooth data
        auto switch_count = 0;
        auto switch_begin = 0;
        if (_settings.MapSmoothPath) {
            int x2 = start_x;
            int y2 = start_y;
            auto dx = abs(x1 - x2);
            auto dy = abs(y1 - y2);
            auto d = std::max(dx, dy);
            auto h1 = abs(dx - dy);
            auto h2 = d - h1;
            switch_count = (((h1 != 0) && (h2 != 0)) ? std::max(h1, h2) / std::min(h1, h2) + 1 : 0);
            if ((h1 != 0) && (h2 != 0) && switch_count < 2) {
                switch_count = 2;
            }
            switch_begin = (((h1 != 0) && (h2 != 0)) ? std::min(h1, h2) % std::max(h1, h2) : 0);
        }

        for (auto i = switch_begin; numindex > 1; i++) {
            numindex--;

            // Without smoothing
            if (!_settings.MapSmoothPath) {
                if (GRID(x1 - 1, y1) == numindex) {
                    steps[numindex - 1] = 4;
                    x1--;
                    continue;
                } // 0
                if (GRID(x1, y1 - 1) == numindex) {
                    steps[numindex - 1] = 2;
                    y1--;
                    continue;
                } // 6
                if (GRID(x1, y1 + 1) == numindex) {
                    steps[numindex - 1] = 6;
                    y1++;
                    continue;
                } // 2
                if (GRID(x1 + 1, y1) == numindex) {
                    steps[numindex - 1] = 0;
                    x1++;
                    continue;
                } // 4
                if (GRID(x1 - 1, y1 + 1) == numindex) {
                    steps[numindex - 1] = 5;
                    x1--;
                    y1++;
                    continue;
                } // 1
                if (GRID(x1 + 1, y1 - 1) == numindex) {
                    steps[numindex - 1] = 1;
                    x1++;
                    y1--;
                    continue;
                } // 5
                if (GRID(x1 + 1, y1 + 1) == numindex) {
                    steps[numindex - 1] = 7;
                    x1++;
                    y1++;
                    continue;
                } // 3
                if (GRID(x1 - 1, y1 - 1) == numindex) {
                    steps[numindex - 1] = 3;
                    x1--;
                    y1--;
                    continue;
                } // 7
            }
            // With smoothing
            else {
                if (switch_count < 2 || ((i % switch_count) != 0)) {
                    if (GRID(x1 - 1, y1) == numindex) {
                        steps[numindex - 1] = 4;
                        x1--;
                        continue;
                    } // 0
                    if (GRID(x1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 6;
                        y1++;
                        continue;
                    } // 2
                    if (GRID(x1 + 1, y1) == numindex) {
                        steps[numindex - 1] = 0;
                        x1++;
                        continue;
                    } // 4
                    if (GRID(x1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 2;
                        y1--;
                        continue;
                    } // 6
                    if (GRID(x1 + 1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 7;
                        x1++;
                        y1++;
                        continue;
                    } // 3
                    if (GRID(x1 - 1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 3;
                        x1--;
                        y1--;
                        continue;
                    } // 7
                    if (GRID(x1 - 1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 5;
                        x1--;
                        y1++;
                        continue;
                    } // 1
                    if (GRID(x1 + 1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 1;
                        x1++;
                        y1--;
                        continue;
                    } // 5
                }
                else {
                    if (GRID(x1 + 1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 7;
                        x1++;
                        y1++;
                        continue;
                    } // 3
                    if (GRID(x1 - 1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 3;
                        x1--;
                        y1--;
                        continue;
                    } // 7
                    if (GRID(x1 - 1, y1) == numindex) {
                        steps[numindex - 1] = 4;
                        x1--;
                        continue;
                    } // 0
                    if (GRID(x1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 6;
                        y1++;
                        continue;
                    } // 2
                    if (GRID(x1 + 1, y1) == numindex) {
                        steps[numindex - 1] = 0;
                        x1++;
                        continue;
                    } // 4
                    if (GRID(x1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 2;
                        y1--;
                        continue;
                    } // 6
                    if (GRID(x1 - 1, y1 + 1) == numindex) {
                        steps[numindex - 1] = 5;
                        x1--;
                        y1++;
                        continue;
                    } // 1
                    if (GRID(x1 + 1, y1 - 1) == numindex) {
                        steps[numindex - 1] = 1;
                        x1++;
                        y1--;
                        continue;
                    } // 5
                }
            }
            return false;
        }
    }
    return true;

#undef GRID
}

auto HexManager::CutPath(CritterView* cr, ushort start_x, ushort start_y, ushort& end_x, ushort& end_y, int cut) -> bool
{
    vector<uchar> dummy;
    return FindPath(cr, start_x, start_y, end_x, end_y, dummy, cut);
}

auto HexManager::TraceBullet(ushort hx, ushort hy, ushort tx, ushort ty, uint dist, float angle, CritterView* find_cr, bool find_cr_safe, vector<CritterView*>* critters, uchar find_type, pair<ushort, ushort>* pre_block, pair<ushort, ushort>* block, vector<pair<ushort, ushort>>* steps, bool check_passed) -> bool
{
    if (_isShowTrack) {
        ClearHexTrack();
    }

    if (dist == 0u) {
        dist = _geomHelper.DistGame(hx, hy, tx, ty);
    }

    auto cx = hx;
    auto cy = hy;
    auto old_cx = cx;
    auto old_cy = cy;

    LineTracer line_tracer(_settings, hx, hy, tx, ty, _maxHexX, _maxHexY, angle);

    for (uint i = 0; i < dist; i++) {
        if (_settings.MapHexagonal) {
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

        auto& field = GetField(cx, cy);
        if (check_passed && field.Flags.IsNotRaked) {
            break;
        }
        if (critters != nullptr) {
            GetCritters(cx, cy, *critters, find_type);
        }
        if (find_cr != nullptr && (field.Crit != nullptr)) {
            auto* cr = field.Crit;
            if ((cr != nullptr) && cr == find_cr) {
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

void HexManager::FindSetCenter(int cx, int cy)
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
            if (!_geomHelper.MoveHexByDir(sx, sy, static_cast<uchar>(dirs[i % dirs_index]), _maxHexX, _maxHexY)) {
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
            _geomHelper.MoveHexByDir(hx, hy, _geomHelper.ReverseDir(static_cast<uchar>(dirs[i % dirs_index])), _maxHexX, _maxHexY);
        }
    };

    // Up
    if (_settings.MapHexagonal) {
        find_set_center_dir({0, 5}, ih);
    }
    else {
        find_set_center_dir({0, 6}, ih);
    }

    // Down
    if (_settings.MapHexagonal) {
        find_set_center_dir({3, 2}, ih);
    }
    else {
        find_set_center_dir({4, 2}, ih);
    }

    // Right
    if (_settings.MapHexagonal) {
        find_set_center_dir({1, -1}, iw);
    }
    else {
        find_set_center_dir({1, -1}, iw);
    }

    // Left
    if (_settings.MapHexagonal) {
        find_set_center_dir({4, -1}, iw);
    }
    else {
        find_set_center_dir({5, -1}, iw);
    }

    // Up-Right
    if (_settings.MapHexagonal) {
        find_set_center_dir({0, -1}, ih);
    }
    else {
        find_set_center_dir({0, -1}, ih);
    }

    // Down-Left
    if (_settings.MapHexagonal) {
        find_set_center_dir({3, -1}, ih);
    }
    else {
        find_set_center_dir({4, -1}, ih);
    }

    // Up-Left
    if (_settings.MapHexagonal) {
        find_set_center_dir({5, -1}, ih);
    }
    else {
        find_set_center_dir({6, -1}, ih);
    }

    // Down-Right
    if (_settings.MapHexagonal) {
        find_set_center_dir({2, -1}, ih);
    }
    else {
        find_set_center_dir({2, 1}, ih);
    }

    RebuildMap(hx, hy);
}

auto HexManager::LoadMap(CacheStorage& cache, hash map_pid) -> bool
{
    WriteLog("Load map...\n");

    // Unload previous
    UnloadMap();

    // Check data sprites reloading
    if (_curDataPrefix != _settings.MapDataPrefix) {
        ReloadSprites();
    }

    // Make name
    string map_name = _str("{}.map", map_pid);

    // Find in cache
    uint data_len = 0;
    auto* data = cache.GetRawData(map_name, data_len);
    if (data == nullptr) {
        WriteLog("Load map '{}' from cache fail.\n", map_name);
        return false;
    }

    // Header
    auto cache_file = File(data, data_len);
    auto buf_len = cache_file.GetFsize();
    auto* buf = Compressor::Uncompress(cache_file.GetBuf(), buf_len, 50);
    if (buf == nullptr) {
        WriteLog("Uncompress map fail.\n");
        return false;
    }

    auto map_file = File(buf, buf_len);
    if (map_file.GetBEUInt() != CLIENT_MAP_FORMAT_VER) {
        WriteLog("Map format is deprecated.\n");
        return false;
    }

    if (map_file.GetBEUInt() != map_pid) {
        WriteLog("Data truncated.\n");
        return false;
    }

    // Data
    auto maxhx = map_file.GetBEUShort();
    auto maxhy = map_file.GetBEUShort();
    auto tiles_len = map_file.GetBEUInt();
    auto scen_len = map_file.GetBEUInt();

    // Create field
    ResizeField(maxhx, maxhy);

    // Tiles
    _curHashTiles = (tiles_len != 0u ? Hashing::MurmurHash2(map_file.GetCurBuf(), tiles_len) : maxhx * maxhy);
    for (uint i = 0; i < tiles_len / sizeof(MapTile); i++) {
        MapTile tile;
        map_file.CopyMem(&tile, sizeof(tile));
        if (tile.HexX >= _maxHexX || tile.HexY >= _maxHexY) {
            continue;
        }

        auto& field = GetField(tile.HexX, tile.HexY);
        auto* anim = _resMngr.GetItemAnim(tile.Name);
        if (anim != nullptr) {
            auto& ftile = field.AddTile(anim, tile.OffsX, tile.OffsY, tile.Layer, tile.IsRoof);
            ProcessTileBorder(ftile, tile.IsRoof);
        }
    }

    // Roof indexes
    auto roof_num = 1;
    for (const auto hx : xrange(_maxHexX)) {
        for (const auto hy : xrange(_maxHexY)) {
            if (GetField(hx, hy).GetTilesCount(true) != 0u) {
                MarkRoofNum(hx, hy, static_cast<short>(roof_num));
                roof_num++;
            }
        }
    }

    // Scenery
    _curHashScen = (scen_len != 0u ? Hashing::MurmurHash2(map_file.GetCurBuf(), scen_len) : maxhx * maxhy);
    auto scen_count = map_file.GetLEUInt();
    for (uint i = 0; i < scen_count; i++) {
        auto id = map_file.GetLEUInt();
        auto proto_id = map_file.GetLEUInt();
        auto datas_size = map_file.GetLEUInt();
        vector<vector<uchar>> props_data(datas_size);
        for (uint i = 0; i < datas_size; i++) {
            auto data_size = map_file.GetLEUInt();
            if (data_size != 0u) {
                props_data[i].resize(data_size);
                map_file.CopyMem(&props_data[i][0], data_size);
            }
        }
        Properties props(ItemView::PropertiesRegistrator);
        props.RestoreData(props_data);

        GenerateItem(id, proto_id, props);
    }

    // Scroll blocks borders
    for (const auto hx : xrange(_maxHexX)) {
        for (const auto hy : xrange(_maxHexY)) {
            auto& field = GetField(hx, hy);
            if (field.Flags.ScrollBlock) {
                for (const auto dir : xrange(_settings.MapDirCount)) {
                    auto hx_ = hx;
                    auto hy_ = hy;
                    _geomHelper.MoveHexByDir(hx_, hy_, static_cast<uchar>(dir), _maxHexX, _maxHexY);
                    GetField(hx_, hy_).Flags.IsNotPassed = true;
                }
            }
        }
    }

    // Visible
    ResizeView();

    // Light
    CollectLightSources();
    _lightPoints.clear();
    _lightPointsCount = 0;
    RealRebuildLight();
    _requestRebuildLight = false;
    _requestRenderLight = true;

    // Finish
    _curPidMap = map_pid;
    _curMapTime = -1;
    AutoScroll.Active = false;
    WriteLog("Load map success.\n");

    _scriptSys.MapLoadEvent();

    return true;
}

void HexManager::UnloadMap()
{
    if (!IsMapLoaded()) {
        return;
    }

    WriteLog("Unload map.\n");

    _scriptSys.MapUnloadEvent();

    _curPidMap = 0;
    _curMapTime = -1;
    _curHashTiles = 0;
    _curHashScen = 0;

    _crittersContour = 0;
    _critterContour = 0;

    delete[] _viewField;
    _viewField = nullptr;

    _hTop = 0;
    _hBottom = 0;
    _wLeft = 0;
    _wRight = 0;
    _hVisible = 0;
    _wVisible = 0;
    _screenHexX = 0;
    _screenHexY = 0;

    _lightSources.clear();
    _lightSourcesScen.clear();

    _fogLookPoints.clear();
    _fogShootPoints.clear();
    _fogForceRerender = true;

    _mainTree.Unvalidate();
    _roofTree.Unvalidate();
    _tilesTree.Unvalidate();
    _roofRainTree.Unvalidate();

    for (auto* drop : _rainData) {
        delete drop;
    }
    _rainData.clear();

    for (auto* item : _hexItems) {
        item->Release();
    }
    _hexItems.clear();

    ResizeField(0, 0);
    DeleteCritters();

    if (_mapperMode) {
        TilesField.clear();
        RoofsField.clear();
    }
}

void HexManager::GetMapHash(CacheStorage& cache, hash map_pid, hash& hash_tiles, hash& hash_scen) const
{
    WriteLog("Get map info...");

    hash_tiles = 0u;
    hash_scen = 0u;

    if (map_pid == _curPidMap) {
        hash_tiles = _curHashTiles;
        hash_scen = _curHashScen;

        WriteLog("Hashes of loaded map: tiles {}, scenery {}.\n", hash_tiles, hash_scen);
        return;
    }

    const string map_name = _str("{}.map", map_pid);

    uint data_len = 0;
    auto* data = cache.GetRawData(map_name, data_len);
    if (data == nullptr) {
        WriteLog("Load map '{}' from cache fail.\n", map_name);
        return;
    }

    const auto cache_file = File(data, data_len);
    auto buf_len = cache_file.GetFsize();
    auto* buf = Compressor::Uncompress(cache_file.GetBuf(), buf_len, 50);
    if (buf == nullptr) {
        WriteLog("Uncompress map fail.\n");
        return;
    }

    auto map_file = File(buf, buf_len);
    if (map_file.GetBEUInt() != CLIENT_MAP_FORMAT_VER) {
        WriteLog("Map format is not supported.\n");
        return;
    }

    if (map_file.GetBEUInt() != map_pid) {
        WriteLog("Invalid proto number.\n");
        return;
    }

    // Data
    const auto maxhx = map_file.GetBEUShort();
    const auto maxhy = map_file.GetBEUShort();
    const auto tiles_len = map_file.GetBEUInt();
    const auto scen_len = map_file.GetBEUInt();

    hash_tiles = (tiles_len != 0u ? Hashing::MurmurHash2(map_file.GetCurBuf(), tiles_len) : maxhx * maxhy);
    map_file.GoForward(tiles_len);
    hash_scen = (scen_len != 0u ? Hashing::MurmurHash2(map_file.GetCurBuf(), scen_len) : maxhx * maxhy);

    WriteLog("complete.\n");
}

void HexManager::GenerateItem(uint id, hash proto_id, Properties& props)
{
    const auto* proto = _protoMngr.GetProtoItem(proto_id);
    RUNTIME_ASSERT(proto);

    auto* scenery = new ItemHexView(id, proto, props, _resMngr, _effectMngr, _gameTime);
    auto& field = GetField(scenery->GetHexX(), scenery->GetHexY());

    scenery->HexScrX = &field.ScrX;
    scenery->HexScrY = &field.ScrY;

    if (scenery->GetIsLight()) {
        _lightSourcesScen.push_back({scenery->GetHexX(), scenery->GetHexY(), scenery->GetLightColor(), scenery->GetLightDistance(), scenery->GetLightFlags(), scenery->GetLightIntensity()});
    }

    PushItem(scenery);

    if (!scenery->GetIsHidden() && !scenery->GetIsHiddenPicture() && !scenery->IsFullyTransparent()) {
        ProcessHexBorders(scenery->Anim->GetSprId(0), scenery->GetOffsetX(), scenery->GetOffsetY(), false);
    }
}

auto HexManager::GetDayTime() -> int
{
    // Todo: need attention!
    // return Globals->GetHour() * 60 + Globals->GetMinute();
    return 0;
}

auto HexManager::GetMapTime() -> int
{
    if (_curMapTime < 0) {
        return GetDayTime();
    }
    return _curMapTime;
}

auto HexManager::GetMapDayTime() -> int*
{
    return _dayTime;
}

auto HexManager::GetMapDayColor() -> uchar*
{
    return _dayColor;
}

void HexManager::OnResolutionChanged()
{
    _fogForceRerender = true;
    ResizeView();
    RefreshMap();
}

auto HexManager::SetProtoMap(ProtoMap & /*pmap*/) -> bool
{
    // Todo: need attention!
    /*WriteLog("Create map from prototype.\n");

    UnloadMap();

    if (curDataPrefix != settings.MapDataPrefix)
        ReloadSprites();

    ResizeField(pmap.GetWidth(), pmap.GetHeight());

    curPidMap = 1;

    int day_time = pmap.GetCurDayTime();
    Globals->SetMinute(day_time % 60);
    Globals->SetHour(day_time / 60 % 24);
    uint color = GenericUtils::GetColorDay(GetMapDayTime(), GetMapDayColor(), GetMapTime(), nullptr);
    sprMngr.SetSpritesColor(COLOR_GAME_RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));

    CScriptArray* dt = pmap.GetDayTime();
    CScriptArray* dc = pmap.GetDayColor();
    for (int i = 0; i < 4; i++)
        dayTime[i] = *(int*)dt->At(i);
    for (int i = 0; i < 12; i++)
        dayColor[i] = *(uchar*)dc->At(i);
    dt->Release();
    dc->Release();

    // Tiles
    ushort width = pmap.GetWidth();
    ushort height = pmap.GetHeight();
    TilesField.resize(width * height);
    RoofsField.resize(width * height);
    for (auto& tile : pmap.Tiles)
    {
        if (!tile.IsRoof)
        {
            TilesField[tile.HexY * width + tile.HexX].push_back(tile);
            TilesField[tile.HexY * width + tile.HexX].back().IsSelected = false;
        }
        else
        {
            RoofsField[tile.HexY * width + tile.HexX].push_back(tile);
            RoofsField[tile.HexY * width + tile.HexX].back().IsSelected = false;
        }
    }
    for (ushort hy = 0; hy < height; hy++)
    {
        for (ushort hx = 0; hx < width; hx++)
        {
            Field& f = GetField(hx, hy);

            for (int r = 0; r <= 1; r++) // Tile/roof
            {
                MapTileVec& tiles = GetTiles(hx, hy, r != 0);

                for (uint i = 0, j = (uint)tiles.size(); i < j; i++)
                {
                    MapTile& tile = tiles[i];
                    AnyFrames* anim = resMngr.GetItemAnim(tile.Name);
                    if (anim)
                    {
                        Field::Tile& ftile = f.AddTile(anim, tile.OffsX, tile.OffsY, tile.Layer, tile.IsRoof);
                        ProcessTileBorder(ftile, tile.IsRoof);
                    }
                }
            }
        }
    }

    // Entities
    for (auto& entity : pmap.AllEntities)
    {
        if (entity->Type == EntityType::Item)
        {
            ItemView* entity_item = (ItemView*)entity;
            if (entity_item->GetAccessory() == ITEM_ACCESSORY_HEX)
            {
                GenerateItem(entity_item->Id, entity_item->GetProtoId(), entity_item->Props);
            }
            else if (entity_item->GetAccessory() == ITEM_ACCESSORY_CRITTER)
            {
                CritterView* cr = GetCritter(entity_item->GetCritId());
                if (cr)
                    cr->AddItem(entity_item->Clone());
            }
            else if (entity_item->GetAccessory() == ITEM_ACCESSORY_CONTAINER)
            {
                ItemView* cont = GetItemById(entity_item->GetContainerId());
                if (cont)
                    cont->ContSetItem(entity_item->Clone());
            }
            else
            {
                throw UnreachablePlaceException(LINE_STR);
            }
        }
        else if (entity->Type == EntityType::CritterView)
        {
            CritterView* entity_cr = (CritterView*)entity;
            CritterView* cr =
                new CritterView(entity_cr->Id, (ProtoCritter*)entity_cr->Proto, settings, sprMngr, resMngr);
            cr->Props = entity_cr->Props;
            cr->Init();
            AddCritter(cr);
        }
        else
        {
            throw UnreachablePlaceException(LINE_STR);
        }
    }

    ResizeView();

    curHashTiles = hash(-1);
    curHashScen = hash(-1);
    WriteLog("Create map from prototype complete.\n");*/
    return true;
}

void HexManager::GetProtoMap(ProtoMap& pmap)
{
    // Todo: need attention!
    /*pmap.SetWorkHexX(screenHexX);
    pmap.SetWorkHexY(screenHexY);
    pmap.SetCurDayTime(GetDayTime());

    // Fill entities
    for (auto& entity : pmap.AllEntities)
        entity->Release();
    pmap.AllEntities.clear();

    auto* spr_mngr = &sprMngr;
    auto* res_mngr = &resMngr;
    auto* sett = &settings;
    std::function<void(Entity*)> fill_recursively = [&fill_recursively, &pmap, sett, spr_mngr, res_mngr](
                                                        Entity* entity) {
        Entity* store_entity = nullptr;
        if (entity->Type == EntityType::ItemHexView || entity->Type == EntityType::Item)
            store_entity = new ItemView(entity->Id, (ProtoItem*)entity->Proto);
        else if (entity->Type == EntityType::CritterView)
            store_entity = new CritterView(entity->Id, (ProtoCritter*)entity->Proto, *sett, *spr_mngr, *res_mngr);
        else
            throw UnreachablePlaceException(LINE_STR);

        store_entity->Props = entity->Props;
        pmap.AllEntities.push_back(store_entity);
        for (auto& child : entity->GetChildren())
        {
            RUNTIME_ASSERT(child->Type == EntityType::Item);
            fill_recursively(child);
        }
    };

    for (auto& kv : allCritters)
        fill_recursively(kv.second);
    for (auto& item : hexItems)
        fill_recursively(item);

    // Fill tiles
    pmap.Tiles.clear();
    ushort width = GetWidth();
    ushort height = GetHeight();
    TilesField.resize(width * height);
    RoofsField.resize(width * height);
    for (ushort hy = 0; hy < height; hy++)
    {
        for (ushort hx = 0; hx < width; hx++)
        {
            MapTileVec& tiles = TilesField[hy * width + hx];
            for (uint i = 0, j = (uint)tiles.size(); i < j; i++)
                pmap.Tiles.push_back(tiles[i]);
            MapTileVec& roofs = RoofsField[hy * width + hx];
            for (uint i = 0, j = (uint)roofs.size(); i < j; i++)
                pmap.Tiles.push_back(roofs[i]);
        }
    }*/
}

auto HexManager::GetTiles(ushort hx, ushort hy, bool is_roof) -> vector<MapTile>&
{
    return is_roof ? RoofsField[hy * GetWidth() + hx] : TilesField[hy * GetWidth() + hx];
}

void HexManager::ClearSelTiles()
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

void HexManager::ParseSelTiles()
{
    auto borders_changed = false;
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

void HexManager::SetTile(hash name, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof, bool select)
{
    if (hx >= _maxHexX || hy >= _maxHexY) {
        return;
    }

    auto* anim = _resMngr.GetItemAnim(name);
    if (anim == nullptr) {
        return;
    }

    if (!select) {
        EraseTile(hx, hy, layer, is_roof, static_cast<uint>(-1));
    }

    auto& field = GetField(hx, hy);
    auto& ftile = field.AddTile(anim, 0, 0, layer, is_roof);
    auto& tiles = GetTiles(hx, hy, is_roof);
    tiles.push_back({name, hx, hy, ox, oy, layer, is_roof});
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

void HexManager::EraseTile(ushort hx, ushort hy, uchar layer, bool is_roof, uint skip_index)
{
    auto& field = GetField(hx, hy);
    for (uint i = 0, j = field.GetTilesCount(is_roof); i < j; i++) {
        auto& tile = field.GetTile(i, is_roof);
        if (tile.Layer == layer && i != skip_index) {
            field.EraseTile(i, is_roof);
            auto& tiles = GetTiles(hx, hy, is_roof);
            tiles.erase(tiles.begin() + i);
            break;
        }
    }
}

void HexManager::AddFastPid(hash pid)
{
    _fastPids.insert(pid);
}

auto HexManager::IsFastPid(hash pid) const -> bool
{
    return _fastPids.count(pid) != 0;
}

void HexManager::ClearFastPids()
{
    _fastPids.clear();
}

void HexManager::AddIgnorePid(hash pid)
{
    _ignorePids.insert(pid);
}

void HexManager::SwitchIgnorePid(hash pid)
{
    if (_ignorePids.count(pid) != 0u) {
        _ignorePids.erase(pid);
    }
    else {
        _ignorePids.insert(pid);
    }
}

auto HexManager::IsIgnorePid(hash pid) const -> bool
{
    return _ignorePids.count(pid) != 0;
}

void HexManager::ClearIgnorePids()
{
    _ignorePids.clear();
}

auto HexManager::GetHexesRect(const IRect& rect) const -> vector<pair<ushort, ushort>>
{
    vector<pair<ushort, ushort>> hexes;

    if (_settings.MapHexagonal) {
        auto [x, y] = _geomHelper.GetHexInterval(rect.Left, rect.Top, rect.Right, rect.Bottom);
        x = -x;

        const auto dx = x / _settings.MapHexWidth;
        const auto dy = y / _settings.MapHexLineHeight;
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
                    hexes.push_back(std::make_pair(hx, hy));
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
        auto [rw, rh] = _geomHelper.GetHexInterval(rect.Left, rect.Top, rect.Right, rect.Bottom);
        if (rw == 0) {
            rw = 1;
        }
        if (rh == 0) {
            rh = 1;
        }

        const auto hw = std::abs(rw / (_settings.MapHexWidth / 2)) + ((rw % (_settings.MapHexWidth / 2)) != 0 ? 1 : 0) + (std::abs(rw) >= _settings.MapHexWidth / 2 ? 1 : 0); // Hexes width
        const auto hh = std::abs(rh / _settings.MapHexLineHeight) + ((rh % _settings.MapHexLineHeight) != 0 ? 1 : 0) + (std::abs(rh) >= _settings.MapHexLineHeight ? 1 : 0); // Hexes height
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

void HexManager::MarkPassedHexes()
{
    for (ushort hx = 0; hx < _maxHexX; hx++) {
        for (ushort hy = 0; hy < _maxHexY; hy++) {
            auto& field = GetField(hx, hy);
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
