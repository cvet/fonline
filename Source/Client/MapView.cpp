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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "MapLoader.h"
#include "StringUtils.h"

static constexpr int MAX_LIGHT_INTEN = 10000;
static constexpr int MAX_LIGHT_HEX = 200;
static constexpr int MAX_LIGHT_ALPHA = 255;

static auto EvaluateItemDrawOrder(const ItemHexView* item) -> DrawOrderType
{
    NO_STACK_TRACE_ENTRY();

    if (item->GetIsTile()) {
        if (item->GetIsRoofTile()) {
            return static_cast<DrawOrderType>(static_cast<int>(DrawOrderType::Roof) + item->GetTileLayer());
        }
        return static_cast<DrawOrderType>(static_cast<int>(DrawOrderType::Tile) + item->GetTileLayer());
    }

    if (item->GetDrawFlatten()) {
        return !item->GetIsScenery() && !item->GetIsWall() ? DrawOrderType::FlatItem : DrawOrderType::FlatScenery;
    }

    return !item->GetIsScenery() && !item->GetIsWall() ? DrawOrderType::Item : DrawOrderType::Scenery;
}

static auto EvaluateCritterDrawOrder(const CritterHexView* cr) -> DrawOrderType
{
    NO_STACK_TRACE_ENTRY();

    return cr->IsDead() && !cr->GetDeadDrawNoFlatten() ? DrawOrderType::DeadCritter : DrawOrderType::Critter;
}

MapView::MapView(FOClient* engine, ident_t id, const ProtoMap* proto, const Properties* props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    MapProperties(GetInitRef()),
    _mapSprites(engine->SprMngr)
{
    STACK_TRACE_ENTRY();

    _name = strex("{}_{}", proto->GetName(), id);

    SetSpritesZoom(1.0f);

    _rtScreenOx = iround(std::ceil(static_cast<float>(_engine->Settings.MapHexWidth) / MIN_ZOOM));
    _rtScreenOy = iround(std::ceil(static_cast<float>(_engine->Settings.MapHexLineHeight * 2) / MIN_ZOOM));

    _rtLight = _engine->SprMngr.GetRtMngr().CreateRenderTarget(false, RenderTarget::SizeType::Map, _rtScreenOx * 2, _rtScreenOy * 2, false);
    _rtLight->CustomDrawEffect = _engine->EffectMngr.Effects.FlushLight;

    _rtFog = _engine->SprMngr.GetRtMngr().CreateRenderTarget(false, RenderTarget::SizeType::Map, _rtScreenOx * 2, _rtScreenOy * 2, false);
    _rtFog->CustomDrawEffect = _engine->EffectMngr.Effects.FlushFog;

    _picHex[0] = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "hex1.png", AtlasType::MapSprites);
    _picHex[1] = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "hex2.png", AtlasType::MapSprites);
    _picHex[2] = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "hex3.png", AtlasType::MapSprites);
    _cursorPrePic = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "move_pre.png", AtlasType::MapSprites);
    _cursorPostPic = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "move_post.png", AtlasType::MapSprites);
    _cursorXPic = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "move_x.png", AtlasType::MapSprites);
    _picTrack1 = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "track1.png", AtlasType::MapSprites);
    _picTrack2 = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "track2.png", AtlasType::MapSprites);
    _picHexMask = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "hex_mask.png", AtlasType::MapSprites);

    if (_picHexMask) {
        const auto* atlas_spr = dynamic_cast<const AtlasSprite*>(_picHexMask.get());
        RUNTIME_ASSERT(atlas_spr);
        const auto mask_x = iround(static_cast<float>(atlas_spr->Atlas->MainTex->Width) * atlas_spr->AtlasRect.Left);
        const auto mask_y = iround(static_cast<float>(atlas_spr->Atlas->MainTex->Height) * atlas_spr->AtlasRect.Top);
        _picHexMaskData = atlas_spr->Atlas->MainTex->GetTextureRegion(mask_x, mask_y, atlas_spr->Width, atlas_spr->Height);
    }

    _width = GetWidth();
    _height = GetHeight();

    _findPathGrid.resize((static_cast<size_t>(MAX_FIND_PATH) * 2 + 2) * (MAX_FIND_PATH * 2 + 2));
    _hexLight.resize(static_cast<size_t>(_width) * _height);
    _hexField = std::make_unique<StaticTwoDimensionalGrid<Field, uint16>>(_width, _height);

    _lightPoints.resize(1);
    _lightSoftPoints.resize(1);

    ResizeView();

    _eventUnsubscriber += _engine->SprMngr.GetWindow()->OnScreenSizeChanged += [this] { OnScreenSizeChanged(); };
}

MapView::~MapView()
{
    STACK_TRACE_ENTRY();

    if (!_critters.empty() || !_crittersMap.empty() || !_allItems.empty() || !_staticItems.empty() || //
        !_dynamicItems.empty() || !_nonTileItems.empty() || !_processingItems.empty() || !_itemsMap.empty()) {
        BreakIntoDebugger();
    }

    if (_rtMap != nullptr) {
        _engine->SprMngr.GetRtMngr().DeleteRenderTarget(_rtMap);
    }
    _engine->SprMngr.GetRtMngr().DeleteRenderTarget(_rtLight);
    _engine->SprMngr.GetRtMngr().DeleteRenderTarget(_rtFog);

    for (auto&& pattern : _spritePatterns) {
        pattern->Sprites.clear();
        pattern->FinishCallback = nullptr;
        pattern->Finished = true;
    }
}

void MapView::OnDestroySelf()
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
    RUNTIME_ASSERT(_nonTileItems.empty());
    RUNTIME_ASSERT(_processingItems.empty());
    RUNTIME_ASSERT(_itemsMap.empty());
}

void MapView::EnableMapperMode()
{
    STACK_TRACE_ENTRY();

    _mapperMode = true;
    _isShowTrack = true;

    _hexTrack.resize(static_cast<size_t>(_width) * _height);
}

void MapView::LoadFromFile(string_view map_name, const string& str)
{
    RUNTIME_ASSERT(_mapperMode);

    _mapLoading = true;

    MapLoader::Load(
        map_name, str, _engine->ProtoMngr, *_engine,
        [this](ident_t id, const ProtoCritter* proto, const map<string, string>& kv) -> bool {
            RUNTIME_ASSERT(id);
            RUNTIME_ASSERT(_crittersMap.count(id) == 0);

            auto props = copy(proto->GetProperties());

            if (!props.ApplyFromText(kv)) {
                return false;
            }

            auto* cr = new CritterHexView(this, id, proto, &props);

            if (cr->GetHexX() >= _width) {
                cr->SetHexX(_width - 1);
            }
            if (cr->GetHexY() >= _height) {
                cr->SetHexY(_height - 1);
            }

            AddCritterInternal(cr);
            return true;
        },
        [this](ident_t id, const ProtoItem* proto, const map<string, string>& kv) -> bool {
            RUNTIME_ASSERT(id);
            RUNTIME_ASSERT(_itemsMap.count(id) == 0);

            auto props = copy(proto->GetProperties());

            if (!props.ApplyFromText(kv)) {
                return false;
            }

            auto* item = new ItemHexView(this, id, proto, &props);

            if (item->GetOwnership() == ItemOwnership::MapHex) {
                if (item->GetHexX() >= _width) {
                    item->SetHexX(_width - 1);
                }
                if (item->GetHexY() >= _height) {
                    item->SetHexY(_height - 1);
                }

                AddItemInternal(item);
            }
            else if (item->GetOwnership() == ItemOwnership::CritterInventory) {
                auto* cr = GetCritter(item->GetCritterId());

                if (cr == nullptr) {
                    return false;
                }

                cr->AddRawInvItem(item);
            }
            else if (item->GetOwnership() == ItemOwnership::ItemContainer) {
                auto* cont = GetItem(item->GetContainerId());

                if (cont == nullptr) {
                    return false;
                }

                cont->AddRawInnerItem(item);
            }
            else {
                return false;
            }

            return true;
        });

    _mapLoading = false;

    ResizeView();
    RefreshMap();
}

void MapView::LoadStaticData()
{
    STACK_TRACE_ENTRY();

    const auto file = _engine->Resources.ReadFile(strex("{}.fomapb2", GetProtoId()));
    if (!file) {
        throw MapViewLoadException("Map file not found", GetProtoId());
    }

    auto reader = DataReader({file.GetBuf(), file.GetSize()});

    // Hashes
    {
        const auto hashes_count = reader.Read<uint>();

        string str;
        for (uint i = 0; i < hashes_count; i++) {
            const auto str_len = reader.Read<uint>();
            str.resize(str_len);
            reader.ReadPtr(str.data(), str.length());
            const auto hstr = _engine->ToHashedString(str);
            UNUSED_VARIABLE(hstr);
        }
    }

    // Read static items
    {
        _mapLoading = true;
        auto reset_loading = ScopeCallback([this]() noexcept { _mapLoading = false; });

        const auto items_count = reader.Read<uint>();

        _allItems.reserve(items_count);
        _staticItems.reserve(items_count);
        _nonTileItems.reserve(items_count);
        _processingItems.reserve(items_count);

        vector<uint8> props_data;

        for (uint i = 0; i < items_count; i++) {
            const auto static_id = ident_t {reader.Read<uint>()};
            const auto item_pid_hash = reader.Read<hstring::hash_t>();
            const auto item_pid = _engine->ResolveHash(item_pid_hash);
            const auto* item_proto = _engine->ProtoMngr.GetProtoItem(item_pid);

            auto item_props = Properties(item_proto->GetProperties().GetRegistrator());
            const auto props_data_size = reader.Read<uint>();
            props_data.resize(props_data_size);
            reader.ReadPtr<uint8>(props_data.data(), props_data_size);
            item_props.RestoreAllData(props_data);

            auto* static_item = new ItemHexView(this, static_id, item_proto, &item_props);

            static_item->SetStatic(true);

            AddItemInternal(static_item);
        }
    }

    reader.VerifyEnd();

    // Index roof
    auto roof_num = 1;
    for (const auto hx : xrange(_width)) {
        for (const auto hy : xrange(_height)) {
            if (!_hexField->GetCellForReading(hx, hy).RoofTiles.empty()) {
                MarkRoofNum(hx, hy, static_cast<int16>(roof_num));
                roof_num++;
            }
        }
    }

    // Scroll blocks borders
    for (const auto hx : xrange(_width)) {
        for (const auto hy : xrange(_height)) {
            const auto& field = _hexField->GetCellForReading(hx, hy);
            if (field.Flags.ScrollBlock) {
                for (const auto dir : xrange(GameSettings::MAP_DIR_COUNT)) {
                    auto hx_ = hx;
                    auto hy_ = hy;
                    GeometryHelper::MoveHexByDir(hx_, hy_, static_cast<uint8>(dir), _width, _height);
                    _hexField->GetCellForWriting(hx_, hy_).Flags.MoveBlocked = true;
                }
            }
        }
    }

    ResizeView();
    RefreshMap();

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

                _needReapplyLights = true;
            }

            if (_globalDayColor != _prevGlobalDayColor) {
                _prevGlobalDayColor = _globalDayColor;

                if (_globalLights != 0) {
                    _needReapplyLights = true;
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

        for (auto* item : _processingItems) {
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

    // Scroll
    if (Scroll()) {
        RebuildFog();
    }
}

void MapView::AddMapText(string_view str, uint16 hx, uint16 hy, ucolor color, time_duration show_time, bool fade, int ox, int oy)
{
    STACK_TRACE_ENTRY();

    MapText map_text;
    map_text.HexX = hx;
    map_text.HexY = hy;
    map_text.Color = color != ucolor::clear ? color : COLOR_TEXT;
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

    auto& field = _hexField->GetCellForWriting(hx, hy);

    if (item->GetIsTile()) {
        if (item->GetIsRoofTile()) {
            field.RoofTiles.emplace_back(item);
        }
        else {
            field.GroundTiles.emplace_back(item);
        }
    }
    else {
        field.Items.emplace_back(item);

        std::sort(field.Items.begin(), field.Items.end(), [](const auto* i1, const auto* i2) { return i1->GetIsScenery() && !i2->GetIsScenery(); });
        std::sort(field.Items.begin(), field.Items.end(), [](const auto* i1, const auto* i2) { return i1->GetIsWall() && !i2->GetIsWall(); });

        RecacheHexFlags(field);

        if (item->IsNonEmptyBlockLines()) {
            GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hx, hy, _width, _height, [this, item](uint16 hx2, uint16 hy2) {
                auto& field2 = _hexField->GetCellForWriting(hx2, hy2);
                field2.BlockLineItems.emplace_back(item);
                RecacheHexFlags(field2);
            });
        }
    }

    if (!item->GetLightThru()) {
        UpdateHexLightSources(hx, hy);
    }

    UpdateItemLightSource(item);
}

void MapView::RemoveItemFromField(ItemHexView* item)
{
    STACK_TRACE_ENTRY();

    const uint16 hx = item->GetHexX();
    const uint16 hy = item->GetHexY();

    auto& field = _hexField->GetCellForWriting(hx, hy);

    if (item->GetIsTile()) {
        if (item->GetIsRoofTile()) {
            const auto it = std::find(field.RoofTiles.begin(), field.RoofTiles.end(), item);
            RUNTIME_ASSERT(it != field.RoofTiles.end());
            field.RoofTiles.erase(it);
        }
        else {
            const auto it = std::find(field.GroundTiles.begin(), field.GroundTiles.end(), item);
            RUNTIME_ASSERT(it != field.GroundTiles.end());
            field.GroundTiles.erase(it);
        }
    }
    else {
        const auto it = std::find(field.Items.begin(), field.Items.end(), item);
        RUNTIME_ASSERT(it != field.Items.end());
        field.Items.erase(it);

        RecacheHexFlags(field);

        if (item->IsNonEmptyBlockLines()) {
            GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hx, hy, _width, _height, [this, item](uint16 hx2, uint16 hy2) {
                auto& field2 = _hexField->GetCellForWriting(hx2, hy2);
                const auto it2 = std::find(field2.BlockLineItems.begin(), field2.BlockLineItems.end(), item);
                RUNTIME_ASSERT(it2 != field2.BlockLineItems.end());
                field2.BlockLineItems.erase(it2);
                RecacheHexFlags(field2);
            });
        }
    }

    if (!item->GetLightThru()) {
        UpdateHexLightSources(hx, hy);
    }

    FinishLightSource(item->GetId());
}

auto MapView::AddReceivedItem(ident_t id, hstring pid, uint16 hx, uint16 hy, const vector<vector<uint8>>& data) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(id);
    RUNTIME_ASSERT(hx < _width && hy < _height);

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto* item = new ItemHexView(this, id, proto);

    item->RestoreData(data);
    item->SetHexX(hx);
    item->SetHexY(hy);

    if (!item->GetShootThru()) {
        RebuildFog();
    }

    return AddItemInternal(item);
}

auto MapView::AddMapperItem(hstring pid, uint16 hx, uint16 hy, const Properties* props) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);
    RUNTIME_ASSERT(hx < _width && hy < _height);

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto* item = new ItemHexView(this, GetTempEntityId(), proto, props);

    item->SetHexX(hx);
    item->SetHexY(hy);

    return AddItemInternal(item);
}

auto MapView::AddMapperTile(hstring pid, uint16 hx, uint16 hy, uint8 layer, bool is_roof) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);
    RUNTIME_ASSERT(hx < _width && hy < _height);

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto* item = new ItemHexView(this, GetTempEntityId(), proto);

    item->SetHexX(hx);
    item->SetHexY(hy);
    item->SetIsTile(true);
    item->SetIsRoofTile(is_roof);
    item->SetTileLayer(layer);

    return AddItemInternal(item);
}

auto MapView::AddItemInternal(ItemHexView* item) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    const auto hx = item->GetHexX();
    const auto hy = item->GetHexY();
    RUNTIME_ASSERT(hx < _width && hy < _height);
    RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::MapHex);

    if (item->GetId()) {
        if (auto* prev_item = GetItem(item->GetId()); prev_item != nullptr) {
            item->RestoreFading(prev_item->StoreFading());
            DestroyItem(prev_item);
        }
    }

    item->Init();

    _allItems.emplace_back(item);

    if (item->GetStatic()) {
        _staticItems.emplace_back(item);

        if (item->IsNeedProcess()) {
            _processingItems.emplace_back(item);
        }
    }
    else {
        _dynamicItems.emplace_back(item);
        _processingItems.emplace_back(item);
    }

    if (!item->GetIsTile()) {
        _nonTileItems.emplace_back(item);
    }

    if (item->GetId()) {
        _itemsMap.emplace(item->GetId(), item);
    }

    AddItemToField(item);

    if (!MeasureMapBorders(item->Spr, item->ScrX, item->ScrY)) {
        if (!_mapLoading && IsHexToDraw(hx, hy) && (_mapperMode || !item->GetAlwaysHideSprite())) {
            auto& field = _hexField->GetCellForWriting(hx, hy);
            auto* spr = item->InsertSprite(_mapSprites, EvaluateItemDrawOrder(item), hx, static_cast<uint16>(hy + item->GetDrawOrderOffsetHexY()), &field.ScrX, &field.ScrY);
            AddSpriteToChain(field, spr);
        }
    }

    return item;
}

void MapView::MoveItem(ItemHexView* item, uint16 hx, uint16 hy)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item->GetMap() == this);

    RemoveItemFromField(item);
    item->SetHexX(hx);
    item->SetHexY(hy);
    AddItemToField(item);

    if (item->IsSpriteValid()) {
        item->InvalidateSprite();
    }

    if (IsHexToDraw(hx, hy) && (_mapperMode || !item->GetAlwaysHideSprite())) {
        auto& field = _hexField->GetCellForWriting(hx, hy);
        auto* spr = item->InsertSprite(_mapSprites, EvaluateItemDrawOrder(item), hx, static_cast<uint16>(hy + item->GetDrawOrderOffsetHexY()), &field.ScrX, &field.ScrY);
        AddSpriteToChain(field, spr);
    }
}

void MapView::DestroyItem(ItemHexView* item)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item->GetMap() == this);

    if (item->IsSpriteValid()) {
        item->InvalidateSprite();
    }

    {
        const auto it = std::find(_allItems.begin(), _allItems.end(), item);
        RUNTIME_ASSERT(it != _allItems.end());
        _allItems.erase(it);
    }

    if (item->GetStatic()) {
        const auto it = std::find(_staticItems.begin(), _staticItems.end(), item);
        RUNTIME_ASSERT(it != _staticItems.end());
        _staticItems.erase(it);
    }
    else {
        const auto it = std::find(_dynamicItems.begin(), _dynamicItems.end(), item);
        RUNTIME_ASSERT(it != _dynamicItems.end());
        _dynamicItems.erase(it);
    }

    if (!item->GetIsTile()) {
        const auto it = std::find(_nonTileItems.begin(), _nonTileItems.end(), item);
        RUNTIME_ASSERT(it != _nonTileItems.end());
        _nonTileItems.erase(it);
    }

    if (const auto it = std::find(_processingItems.begin(), _processingItems.end(), item); it != _processingItems.end()) {
        _processingItems.erase(it);
    }

    if (item->GetId()) {
        const auto it = _itemsMap.find(item->GetId());
        RUNTIME_ASSERT(it != _itemsMap.end());
        _itemsMap.erase(it);
    }

    RemoveItemFromField(item);
    CleanLightSourceOffsets(item->GetId());

    item->DestroySelf();
}

auto MapView::GetItem(uint16 hx, uint16 hy, hstring pid) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (hx >= _width || hy >= _height || _hexField->GetCellForReading(hx, hy).Items.empty()) {
        return nullptr;
    }

    for (auto* item : _hexField->GetCellForReading(hx, hy).Items) {
        if (item->GetProtoId() == pid) {
            return item;
        }
    }

    return nullptr;
}

auto MapView::GetItem(uint16 hx, uint16 hy, ident_t id) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (hx >= _width || hy >= _height || _hexField->GetCellForReading(hx, hy).Items.empty()) {
        return nullptr;
    }

    for (auto* item : _hexField->GetCellForReading(hx, hy).Items) {
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

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    return field.Items;
}

auto MapView::GetTile(uint16 hx, uint16 hy, bool is_roof, int layer) -> ItemHexView*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField->GetCellForReading(hx, hy);
    const auto& field_tiles = is_roof ? field.RoofTiles : field.GroundTiles;

    for (auto* tile : field_tiles) {
        if (layer < 0 || tile->GetTileLayer() == layer) {
            return tile;
        }
    }

    return nullptr;
}

auto MapView::GetTiles(uint16 hx, uint16 hy, bool is_roof) -> const vector<ItemHexView*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField->GetCellForReading(hx, hy);
    const auto& field_tiles = is_roof ? field.RoofTiles : field.GroundTiles;

    return field_tiles;
}

auto MapView::GetRectForText(uint16 hx, uint16 hy) -> IRect
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto result = IRect();

    if (const auto& field = _hexField->GetCellForReading(hx, hy); field.IsView) {
        if (!field.Critters.empty()) {
            for (const auto* cr : field.Critters) {
                if (cr->IsSpriteVisible()) {
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

        if (!field.Items.empty()) {
            for (const auto* item : field.Items) {
                if (item->IsSpriteVisible()) {
                    const auto* spr = item->Spr;
                    if (spr != nullptr) {
                        const auto l = field.ScrX + _engine->Settings.MapHexWidth / 2 - spr->OffsX;
                        const auto t = field.ScrY + _engine->Settings.MapHexHeight / 2 - spr->OffsY;
                        const auto r = l + spr->Width;
                        const auto b = t + spr->Height;
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

void MapView::RunEffectItem(hstring eff_pid, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!(from_hx >= _width || from_hy >= _height || to_hx >= _width || to_hy >= _height));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(eff_pid);
    auto* effect_item = new ItemHexView(this, ident_t {}, proto);

    effect_item->SetHexX(from_hx);
    effect_item->SetHexY(from_hy);
    effect_item->SetEffect(to_hx, to_hy);

    AddItemInternal(effect_item);
}

auto MapView::RunSpritePattern(string_view name, uint count) -> SpritePattern*
{
    STACK_TRACE_ENTRY();

    auto&& spr = _engine->SprMngr.LoadSprite(name, AtlasType::MapSprites);
    if (!spr) {
        return nullptr;
    }

    spr->Prewarm();
    spr->PlayDefault();

    auto&& pattern = unique_release_ptr<SpritePattern>(new SpritePattern());

    pattern->SprName = name;
    pattern->SprCount = count;
    pattern->Sprites.emplace_back(std::move(spr));

    for (uint i = 1; i < count; i++) {
        auto&& next_spr = _engine->SprMngr.LoadSprite(name, AtlasType::MapSprites);

        next_spr->Prewarm();
        next_spr->PlayDefault();

        pattern->Sprites.emplace_back(std::move(next_spr));
    }

    pattern->FinishCallback = [this, pattern = pattern.get()]() {
        const auto it = std::find_if(_spritePatterns.begin(), _spritePatterns.end(), [pattern](auto&& p) { return p.get() == pattern; });
        RUNTIME_ASSERT(it != _spritePatterns.end());
        it->get()->Sprites.clear();
        _spritePatterns.erase(it);
        pattern->FinishCallback = nullptr;
        pattern->Finished = true;
    };

    _spritePatterns.emplace_back(std::move(pattern));

    return _spritePatterns.back().get();
}

void MapView::SetCursorPos(CritterHexView* cr, int x, int y, bool show_steps, bool refresh)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!cr || cr->GetMap() == this);

    uint16 hx = 0;
    uint16 hy = 0;
    if (GetHexAtScreenPos(x, y, hx, hy, nullptr, nullptr)) {
        const auto& field = _hexField->GetCellForReading(hx, hy);

        _cursorX = field.ScrX + 1 - 1;
        _cursorY = field.ScrY - 1 - 1;

        if (cr == nullptr) {
            _drawCursorX = -1;
            return;
        }

        const auto cx = cr->GetHexX();
        const auto cy = cr->GetHexY();
        const auto mh = cr->GetMultihex();

        if ((cx == hx && cy == hy) || (field.Flags.MoveBlocked && (mh == 0 || !GeometryHelper::CheckDist(cx, cy, hx, hy, mh)))) {
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

void MapView::DrawCursor(const Sprite* spr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_engine->Settings.HideCursor || !_engine->Settings.ShowMoveCursor) {
        return;
    }

    _engine->SprMngr.DrawSpriteSize(spr,
        iround(static_cast<float>(_cursorX + _engine->Settings.ScrOx) / GetSpritesZoom()), //
        iround(static_cast<float>(_cursorY + _engine->Settings.ScrOy) / GetSpritesZoom()), //
        iround(static_cast<float>(spr->Width) / GetSpritesZoom()), //
        iround(static_cast<float>(spr->Height) / GetSpritesZoom()), true, false, COLOR_SPRITE);
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

void MapView::RebuildMap(int screen_hx, int screen_hy)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_viewField.empty());

    for (int i = 0, j = _hVisible * _wVisible; i < j; i++) {
        const auto& vf = _viewField[i];

        if (vf.HexX < 0 || vf.HexY < 0 || vf.HexX >= _width || vf.HexY >= _height) {
            continue;
        }

        const auto hx = static_cast<uint16>(vf.HexX);
        const auto hy = static_cast<uint16>(vf.HexY);
        auto& field = _hexField->GetCellForWriting(hx, hy);

        field.IsView = false;
        InvalidateSpriteChain(field);
    }

    InitView(screen_hx, screen_hy);

    // Invalidation
    _mapSprites.Invalidate();
    _engine->SprMngr.EggNotValid();
    _visibleLightSources.clear();
    _needReapplyLights = true;

    // Begin generate new sprites
    for (const auto i : xrange(_hVisible * _wVisible)) {
        const auto& vf = _viewField[i];

        if (vf.HexX < 0 || vf.HexY < 0 || vf.HexX >= _width || vf.HexY >= _height) {
            continue;
        }

        const auto hx = static_cast<uint16>(vf.HexX);
        const auto hy = static_cast<uint16>(vf.HexY);
        auto& field = _hexField->GetCellForWriting(hx, hy);

        RUNTIME_ASSERT(!field.IsView);

        field.IsView = true;
        field.ScrX = vf.ScrX;
        field.ScrY = vf.ScrY;

        // Lighting
        if (!field.LightSources.empty()) {
            for (auto&& [ls, color] : field.LightSources) {
                _visibleLightSources[ls]++;
            }
        }

        // Track
        if (_isShowTrack && GetHexTrack(hx, hy) != 0) {
            auto&& spr = GetHexTrack(hx, hy) == 1 ? _picTrack1 : _picTrack2;
            auto& mspr = _mapSprites.AddSprite(DrawOrderType::Track, hx, hy, //
                _engine->Settings.MapHexWidth / 2, (_engine->Settings.MapHexHeight / 2) + (spr ? spr->Height / 2 : 0), &field.ScrX, &field.ScrY, //
                spr.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            AddSpriteToChain(field, &mspr);
        }

        // Hex Lines
        if (_isShowHex) {
            auto&& spr = _picHex[0];
            auto& mspr = _mapSprites.AddSprite(DrawOrderType::HexGrid, hx, hy, //
                spr ? spr->Width / 2 : 0, spr ? spr->Height : 0, &field.ScrX, &field.ScrY, //
                spr.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            AddSpriteToChain(field, &mspr);
        }

        // Tiles
        if (!field.GroundTiles.empty() && _engine->Settings.ShowTile) {
            for (auto* tile : field.GroundTiles) {
                if (!_mapperMode && tile->GetAlwaysHideSprite()) {
                    continue;
                }

                auto* mspr = tile->AddSprite(_mapSprites, EvaluateItemDrawOrder(tile), hx, static_cast<uint16>(hy + tile->GetDrawOrderOffsetHexY()), &field.ScrX, &field.ScrY);
                AddSpriteToChain(field, mspr);
            }
        }

        // Roof
        if (!field.RoofTiles.empty() && (_roofSkip == 0 || _roofSkip != field.RoofNum) && _engine->Settings.ShowRoof) {
            for (auto* tile : field.RoofTiles) {
                if (!_mapperMode && tile->GetAlwaysHideSprite()) {
                    continue;
                }

                auto* mspr = tile->AddSprite(_mapSprites, EvaluateItemDrawOrder(tile), hx, static_cast<uint16>(hy + tile->GetDrawOrderOffsetHexY()), &field.ScrX, &field.ScrY);
                mspr->SetEggAppearence(EggAppearenceType::Always);
                AddSpriteToChain(field, mspr);
            }
        }

        // Items on hex
        if (!field.Items.empty()) {
            for (auto* item : field.Items) {
                if (!_mapperMode) {
                    if (item->GetAlwaysHideSprite()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowScen && item->GetIsScenery()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowItem && !item->GetIsScenery() && !item->GetIsWall()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowWall && item->GetIsWall()) {
                        continue;
                    }
                }
                else {
                    const auto is_fast = _fastPids.count(item->GetProtoId()) != 0;

                    if (!_engine->Settings.ShowScen && !is_fast && item->GetIsScenery()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowItem && !is_fast && !item->GetIsScenery() && !item->GetIsWall()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowWall && !is_fast && item->GetIsWall()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowFast && is_fast) {
                        continue;
                    }
                    if (_ignorePids.count(item->GetProtoId()) != 0) {
                        continue;
                    }
                }

                auto* mspr = item->AddSprite(_mapSprites, EvaluateItemDrawOrder(item), hx, static_cast<uint16>(hy + item->GetDrawOrderOffsetHexY()), &field.ScrX, &field.ScrY);
                AddSpriteToChain(field, mspr);
            }
        }

        // Critters
        if (!field.Critters.empty() && _engine->Settings.ShowCrit) {
            for (auto* cr : field.Critters) {
                auto* mspr = cr->AddSprite(_mapSprites, EvaluateCritterDrawOrder(cr), hx, hy, &field.ScrX, &field.ScrY);

                cr->RefreshOffs();
                cr->ResetOk();

                auto contour = ContourType::None;
                if (cr->GetId() == _critterContourCrId) {
                    contour = _critterContour;
                }
                else if (!cr->GetIsChosen()) {
                    contour = _crittersContour;
                }
                mspr->SetContour(contour, cr->GetContourColor());

                AddSpriteToChain(field, mspr);
            }
        }

        // Patterns
        if (!_spritePatterns.empty()) {
            for (auto&& pattern : _spritePatterns) {
                if ((hx % pattern->EveryHexX) != 0) {
                    continue;
                }
                if ((hy % pattern->EveryHexY) != 0) {
                    continue;
                }

                if (pattern->InteractWithRoof && _roofSkip != 0 && _roofSkip == field.RoofNum) {
                    continue;
                }

                if (pattern->CheckTileProperty) {
                    if (field.GroundTiles.empty()) {
                        continue;
                    }
                    if (field.GroundTiles.front()->GetValueAsInt(static_cast<int>(pattern->TileProperty)) != pattern->ExpectedTilePropertyValue) {
                        continue;
                    }
                }
                else {
                    continue;
                }

                const auto* spr = pattern->Sprites[(hy * (pattern->Sprites.size() / 5) + hx) % pattern->Sprites.size()].get();
                auto& mspr = _mapSprites.AddSprite(pattern->InteractWithRoof && field.RoofNum != 0 ? DrawOrderType::RoofParticles : DrawOrderType::Particles, hx, hy, //
                    _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2 + (pattern->InteractWithRoof && field.RoofNum != 0 ? _engine->Settings.MapRoofOffsY : 0), &field.ScrX, &field.ScrY, //
                    spr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                AddSpriteToChain(field, &mspr);
            }
        }
    }

    _mapSprites.Sort();

    _screenHexX = screen_hx;
    _screenHexY = screen_hy;

    _needRebuildLightPrimitives = true;

    _engine->OnRenderMap.Fire();
}

void MapView::RebuildMapOffset(int ox, int oy)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_viewField.empty());
    RUNTIME_ASSERT(ox == 0 || ox == -1 || ox == 1);
    RUNTIME_ASSERT(oy == 0 || oy == -2 || oy == 2);

    auto hide_hex = [this](const ViewField& vf) {
        if (vf.HexX < 0 || vf.HexY < 0 || vf.HexX >= _width || vf.HexY >= _height) {
            return;
        }

        const auto hx = static_cast<uint16>(vf.HexX);
        const auto hy = static_cast<uint16>(vf.HexY);
        auto& field = _hexField->GetCellForWriting(hx, hy);

        if (!field.IsView) {
            return;
        }

        field.IsView = false;
        InvalidateSpriteChain(field);

        // Lighting
        if (!field.LightSources.empty()) {
            for (auto&& [ls, color] : field.LightSources) {
                const auto it = _visibleLightSources.find(ls);

                RUNTIME_ASSERT(it != _visibleLightSources.end());
                RUNTIME_ASSERT(it->second > 0);

                if (it->second > 1) {
                    it->second--;
                }
                else {
                    _visibleLightSources.erase(it);
                }
            }
        }
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

        if (vf.HexX >= 0 && vf.HexY >= 0 && vf.HexX < _width && vf.HexY < _height) {
            auto& field = _hexField->GetCellForWriting(static_cast<uint16>(vf.HexX), static_cast<uint16>(vf.HexY));
            field.ScrX = vf.ScrX;
            field.ScrY = vf.ScrY;
        }
    }

    auto show_hex = [this](const ViewField& vf) {
        if (vf.HexX < 0 || vf.HexY < 0 || vf.HexX >= _width || vf.HexY >= _height) {
            return;
        }

        const auto hx = static_cast<uint16>(vf.HexX);
        const auto hy = static_cast<uint16>(vf.HexY);
        auto& field = _hexField->GetCellForWriting(hx, hy);

        if (field.IsView) {
            return;
        }

        field.IsView = true;

        // Lighting
        if (!field.LightSources.empty()) {
            for (auto&& [ls, color] : field.LightSources) {
                const auto it = _visibleLightSources.find(ls);

                if (it == _visibleLightSources.end()) {
                    _visibleLightSources.emplace(ls, 1);
                    ls->NeedReapply = true;
                }
                else {
                    it->second++;
                }
            }
        }

        // Track
        if (_isShowTrack && (GetHexTrack(hx, hy) != 0)) {
            auto&& spr = GetHexTrack(hx, hy) == 1 ? _picTrack1 : _picTrack2;
            auto& mspr = _mapSprites.InsertSprite(DrawOrderType::Track, hx, hy, //
                _engine->Settings.MapHexWidth / 2, (_engine->Settings.MapHexHeight / 2) + (spr ? spr->Height / 2 : 0), &field.ScrX, &field.ScrY, //
                spr.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            AddSpriteToChain(field, &mspr);
        }

        // Hex lines
        if (_isShowHex) {
            auto&& spr = _picHex[0];
            auto& mspr = _mapSprites.InsertSprite(DrawOrderType::HexGrid, hx, hy, //
                spr ? spr->Width / 2 : 0, spr ? spr->Height : 0, &field.ScrX, &field.ScrY, //
                spr.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            AddSpriteToChain(field, &mspr);
        }

        // Tiles
        if (!field.GroundTiles.empty() && _engine->Settings.ShowTile) {
            for (auto* tile : field.GroundTiles) {
                if (!_mapperMode && tile->GetAlwaysHideSprite()) {
                    continue;
                }

                auto* mspr = tile->InsertSprite(_mapSprites, EvaluateItemDrawOrder(tile), hx, static_cast<uint16>(hy + tile->GetDrawOrderOffsetHexY()), &field.ScrX, &field.ScrY);
                AddSpriteToChain(field, mspr);
            }
        }

        // Roof
        if (!field.RoofTiles.empty() && _engine->Settings.ShowRoof && (_roofSkip == 0 || _roofSkip != field.RoofNum)) {
            for (auto* tile : field.RoofTiles) {
                if (!_mapperMode && tile->GetAlwaysHideSprite()) {
                    continue;
                }

                auto* mspr = tile->InsertSprite(_mapSprites, EvaluateItemDrawOrder(tile), hx, static_cast<uint16>(hy + tile->GetDrawOrderOffsetHexY()), &field.ScrX, &field.ScrY);
                mspr->SetEggAppearence(EggAppearenceType::Always);
                AddSpriteToChain(field, mspr);
            }
        }

        // Items on hex
        if (!field.Items.empty()) {
            for (auto* item : field.Items) {
                if (!_mapperMode) {
                    if (item->GetAlwaysHideSprite()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowScen && item->GetIsScenery()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowItem && !item->GetIsScenery() && !item->GetIsWall()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowWall && item->GetIsWall()) {
                        continue;
                    }
                }
                else {
                    const auto is_fast = _fastPids.count(item->GetProtoId()) != 0;

                    if (!_engine->Settings.ShowScen && !is_fast && item->GetIsScenery()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowItem && !is_fast && !item->GetIsScenery() && !item->GetIsWall()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowWall && !is_fast && item->GetIsWall()) {
                        continue;
                    }
                    if (!_engine->Settings.ShowFast && is_fast) {
                        continue;
                    }
                    if (_ignorePids.count(item->GetProtoId()) != 0) {
                        continue;
                    }
                }

                auto* mspr = item->InsertSprite(_mapSprites, EvaluateItemDrawOrder(item), hx, static_cast<uint16>(hy + item->GetDrawOrderOffsetHexY()), &field.ScrX, &field.ScrY);
                AddSpriteToChain(field, mspr);
            }
        }

        // Critters
        if (!field.Critters.empty() && _engine->Settings.ShowCrit) {
            for (auto* cr : field.Critters) {
                auto* mspr = cr->InsertSprite(_mapSprites, EvaluateCritterDrawOrder(cr), hx, hy, &field.ScrX, &field.ScrY);

                cr->RefreshOffs();
                cr->ResetOk();

                auto contour = ContourType::None;
                if (cr->GetId() == _critterContourCrId) {
                    contour = _critterContour;
                }
                else if (!cr->GetIsChosen()) {
                    contour = _crittersContour;
                }
                mspr->SetContour(contour, cr->GetContourColor());

                AddSpriteToChain(field, mspr);
            }
        }

        // Patterns
        if (!_spritePatterns.empty()) {
            for (auto&& pattern : _spritePatterns) {
                if ((hx % pattern->EveryHexX) != 0) {
                    continue;
                }
                if ((hy % pattern->EveryHexY) != 0) {
                    continue;
                }

                if (pattern->InteractWithRoof && _roofSkip != 0 && _roofSkip == field.RoofNum) {
                    continue;
                }

                if (pattern->CheckTileProperty) {
                    if (field.GroundTiles.empty()) {
                        continue;
                    }
                    if (field.GroundTiles.front()->GetValueAsInt(static_cast<int>(pattern->TileProperty)) != pattern->ExpectedTilePropertyValue) {
                        continue;
                    }
                }
                else {
                    continue;
                }

                const auto* spr = pattern->Sprites[(hy * (pattern->Sprites.size() / 5) + hx) % pattern->Sprites.size()].get();
                auto& mspr = _mapSprites.InsertSprite(pattern->InteractWithRoof && field.RoofNum != 0 ? DrawOrderType::RoofParticles : DrawOrderType::Particles, hx, hy, //
                    _engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2 + (pattern->InteractWithRoof && field.RoofNum != 0 ? _engine->Settings.MapRoofOffsY : 0), &field.ScrX, &field.ScrY, //
                    spr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                AddSpriteToChain(field, &mspr);
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

    _needRebuildLightPrimitives = true;

    _engine->OnRenderMap.Fire();
}

void MapView::ProcessLighting()
{
    STACK_TRACE_ENTRY();

    if (_needReapplyLights) {
        _needReapplyLights = false;

        for (auto&& [ls, count] : copy(_visibleLightSources)) {
            ApplyLightFan(ls);
        }
    }

    vector<LightSource*> reapply_sources;
    vector<LightSource*> remove_sources;

    for (auto&& [ls, count] : _visibleLightSources) {
        const auto prev_intensity = ls->CurIntensity;

        if (ls->CurIntensity != ls->TargetIntensity) {
            const auto elapsed_time = time_duration_div(_engine->GameTime.GameplayTime() - ls->Time, time_duration {std::chrono::milliseconds {200}});

            ls->CurIntensity = lerp(ls->StartIntensity, ls->TargetIntensity, std::clamp(elapsed_time, 0.0f, 1.0f));
        }

        if (ls->Finishing && ls->CurIntensity == 0) {
            remove_sources.emplace_back(ls);
        }
        else if (ls->CurIntensity != prev_intensity || ls->NeedReapply) {
            reapply_sources.emplace_back(ls);
        }
    }

    for (auto* ls : reapply_sources) {
        ApplyLightFan(ls);
    }

    for (auto* ls : remove_sources) {
        RUNTIME_ASSERT(_lightSources.count(ls->Id) != 0);

        CleanLightFan(ls);
        _lightSources.erase(ls->Id);
    }

    bool need_render_light = false;

    if (_needRebuildLightPrimitives) {
        _needRebuildLightPrimitives = false;
        need_render_light = true;

        for (auto& points : _lightPoints) {
            points.clear();
        }
        for (auto& points : _lightSoftPoints) {
            points.clear();
        }

        size_t cur_points = 0;

        for (auto&& [ls, count] : _visibleLightSources) {
            RUNTIME_ASSERT(ls->Applied);

            // Split large entries to fit into 16-bit index
            if constexpr (sizeof(vindex_t) == 2) {
                while (_lightPoints[cur_points].size() > 0x7FFF || _lightSoftPoints.size() > 0x7FFF) {
                    cur_points++;

                    if (cur_points >= _lightPoints.size()) {
                        _lightPoints.emplace_back();
                        _lightSoftPoints.emplace_back();
                        break;
                    }
                }
            }

            LightFanToPrimitves(ls, _lightPoints[cur_points], _lightSoftPoints[cur_points]);
        }
    }

    if (!need_render_light) {
        for (auto&& [ls, count] : _visibleLightSources) {
            if (ls->OffsX != nullptr && (*ls->OffsX != ls->LastOffsX || *ls->OffsY != ls->LastOffsY)) {
                ls->LastOffsX = *ls->OffsX;
                ls->LastOffsY = *ls->OffsY;
                need_render_light = true;
            }
        }
    }

    // Prerender light
    if (need_render_light && _rtLight != nullptr) {
        _engine->SprMngr.GetRtMngr().PushRenderTarget(_rtLight);
        _engine->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);

        const auto zoom = GetSpritesZoom();
        const auto offset = FPoint(static_cast<float>(_rtScreenOx), static_cast<float>(_rtScreenOy));

        for (auto& points : _lightPoints) {
            if (!points.empty()) {
                _engine->SprMngr.DrawPoints(points, RenderPrimitiveType::TriangleList, &zoom, &offset, _engine->EffectMngr.Effects.Light);
            }
        }
        for (auto& points : _lightPoints) {
            if (!points.empty()) {
                _engine->SprMngr.DrawPoints(points, RenderPrimitiveType::TriangleList, &zoom, &offset, _engine->EffectMngr.Effects.Light);
            }
        }

        _engine->SprMngr.GetRtMngr().PopRenderTarget();
    }
}

void MapView::UpdateCritterLightSource(const CritterHexView* cr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(cr->GetMap() == this);

    bool light_added = false;

    for (const auto* item : cr->GetConstInvItems()) {
        if (item->GetLightSource() && item->GetCritterSlot() != CritterItemSlot::Inventory) {
            UpdateLightSource(cr->GetId(), cr->GetHexX(), cr->GetHexY(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity(), &cr->ScrX, &cr->ScrY);
            light_added = true;
            break;
        }
    }

    // Default chosen light
    if (!light_added && cr->GetIsChosen()) {
        UpdateLightSource(cr->GetId(), cr->GetHexX(), cr->GetHexY(), _engine->Settings.ChosenLightColor, _engine->Settings.ChosenLightDistance, _engine->Settings.ChosenLightFlags, _engine->Settings.ChosenLightIntensity, &cr->ScrX, &cr->ScrY);
        light_added = true;
    }

    if (!light_added) {
        FinishLightSource(cr->GetId());
    }
}

void MapView::UpdateItemLightSource(const ItemHexView* item)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item->GetMap() == this);

    if (item->GetLightSource()) {
        UpdateLightSource(item->GetId(), item->GetHexX(), item->GetHexY(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity(), &item->ScrX, &item->ScrY);
    }
    else {
        FinishLightSource(item->GetId());
    }
}

void MapView::UpdateHexLightSources(uint16 hx, uint16 hy)
{
    STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    for (auto&& ls_pair : copy(field.LightSources)) {
        ApplyLightFan(ls_pair.first);
    }
}

void MapView::UpdateLightSource(ident_t id, uint16 hx, uint16 hy, ucolor color, uint distance, uint8 flags, int intensity, const int* ox, const int* oy)
{
    STACK_TRACE_ENTRY();

    LightSource* ls;

    const auto it = _lightSources.find(id);

    if (it == _lightSources.end()) {
        ls = _lightSources.emplace(id, std::make_unique<LightSource>(LightSource {id, hx, hy, color, distance, flags, intensity, ox, oy})).first->second.get();
    }
    else {
        ls = it->second.get();

        // Ignore redundant updates
        if (!ls->Finishing && ls->HexX == hx && ls->HexY == hy && ls->Color == color && ls->Distance == distance && ls->Flags == flags && ls->Intensity == intensity) {
            return;
        }

        CleanLightFan(ls);

        ls->Finishing = false;
        ls->HexX = hx;
        ls->HexY = hy;
        ls->Color = color;
        ls->Distance = distance;
        ls->Flags = flags;
        ls->Intensity = intensity;
    }

    ls->TargetIntensity = static_cast<uint>(std::min(std::abs(ls->Intensity), 100));

    if (_mapLoading) {
        ls->CurIntensity = ls->TargetIntensity;
    }
    else {
        if (ls->CurIntensity != ls->TargetIntensity) {
            ls->StartIntensity = ls->CurIntensity;
            ls->Time = _engine->GameTime.GameplayTime();
        }
    }

    ApplyLightFan(ls);
}

void MapView::FinishLightSource(ident_t id)
{
    STACK_TRACE_ENTRY();

    const auto it = _lightSources.find(id);

    if (it != _lightSources.end()) {
        auto&& ls = it->second;

        if (!ls->Finishing) {
            ls->Finishing = true;
            ls->StartIntensity = ls->CurIntensity;
            ls->TargetIntensity = 0;
            ls->Time = _engine->GameTime.GameplayTime();
        }
    }
}

void MapView::CleanLightSourceOffsets(ident_t id)
{
    STACK_TRACE_ENTRY();

    const auto it = _lightSources.find(id);

    if (it != _lightSources.end()) {
        auto&& ls = it->second;

        ls->OffsX = nullptr;
        ls->OffsY = nullptr;
    }
}

void MapView::ApplyLightFan(LightSource* ls)
{
    STACK_TRACE_ENTRY();

    if (ls->Applied) {
        CleanLightFan(ls);
    }

    RUNTIME_ASSERT(!ls->Applied);

    ls->Applied = true;
    ls->NeedReapply = false;

    const auto center_hx = ls->HexX;
    const auto center_hy = ls->HexY;
    const auto distance = ls->Distance;
    const auto prev_fan_hexes = std::move(ls->FanHexes);

    ls->MarkedHexes.clear();
    ls->MarkedHexes.reserve(static_cast<size_t>(GenericUtils::NumericalNumber(distance)) * GameSettings::MAP_DIR_COUNT + 1);
    ls->FanHexes.clear();
    ls->FanHexes.reserve(static_cast<size_t>(distance) * GameSettings::MAP_DIR_COUNT);

    if (IsBitSet(ls->Flags, LIGHT_GLOBAL)) {
        _globalLights++;
    }

    if (IsBitSet(ls->Flags, LIGHT_GLOBAL)) {
        ls->Capacity = _globalDayLightCapacity;
    }
    else if (ls->Intensity >= 0) {
        ls->Capacity = _mapDayLightCapacity;
    }
    else {
        ls->Capacity = 100;
    }

    if (IsBitSet(ls->Flags, LIGHT_INVERSE)) {
        ls->Capacity = 100 - ls->Capacity;
    }

    const auto intensity = ls->CurIntensity * 100; // To MAX_LIGHT_INTEN
    const auto center_alpha = static_cast<uint8>(MAX_LIGHT_ALPHA * ls->Capacity / 100 * intensity / MAX_LIGHT_INTEN);

    ls->CenterColor = ucolor {ls->Color, center_alpha};

    MarkLight(ls, center_hx, center_hy, intensity);

    int raw_traced_hx = center_hx;
    int raw_traced_hy = center_hy;
    bool seek_start = true;
    auto last_hx = static_cast<uint16>(-1);
    auto last_hy = static_cast<uint16>(-1);

    for (int i = 0, ii = (GameSettings::HEXAGONAL_GEOMETRY ? 6 : 4); i < ii; i++) {
        const auto dir = static_cast<uint8>(GameSettings::HEXAGONAL_GEOMETRY ? (i + 2) % 6 : ((i + 1) * 2) % 8);

        for (int j = 0, jj = static_cast<int>(GameSettings::HEXAGONAL_GEOMETRY ? distance : distance * 2); j < jj; j++) {
            if (seek_start) {
                for (uint l = 0; l < distance; l++) {
                    GeometryHelper::MoveHexByDirUnsafe(raw_traced_hx, raw_traced_hy, GameSettings::HEXAGONAL_GEOMETRY ? 0 : 7);
                }

                seek_start = false;
                j = -1;
            }
            else {
                GeometryHelper::MoveHexByDirUnsafe(raw_traced_hx, raw_traced_hy, dir);
            }

            auto traced_hx = static_cast<uint16>(std::clamp(raw_traced_hx, 0, _width - 1));
            auto traced_hy = static_cast<uint16>(std::clamp(raw_traced_hy, 0, _height - 1));

            if (IsBitSet(ls->Flags, LIGHT_DISABLE_DIR(i))) {
                traced_hx = center_hx;
                traced_hy = center_hy;
            }
            else {
                TraceLightLine(ls, center_hx, center_hy, traced_hx, traced_hy, distance, intensity);
            }

            if (traced_hx != last_hx || traced_hy != last_hy) {
                uint8 traced_alpha;
                bool use_offsets = false;

                if (static_cast<int>(traced_hx) != raw_traced_hx || static_cast<int>(traced_hy) != raw_traced_hy) {
                    traced_alpha = static_cast<uint8>(lerp(static_cast<int>(center_alpha), 0, static_cast<float>(GeometryHelper::DistGame(center_hx, center_hy, traced_hx, traced_hy)) / static_cast<float>(distance)));

                    if (traced_hx == center_hx && traced_hy == center_hy) {
                        use_offsets = true;
                    }
                }
                else {
                    traced_alpha = 0;
                    use_offsets = true;
                }

                ls->FanHexes.emplace_back(traced_hx, traced_hy, traced_alpha, use_offsets);

                last_hx = traced_hx;
                last_hy = traced_hy;
            }
        }
    }

    if (!ls->FanHexes.empty()) {
        _needRebuildLightPrimitives = true;
    }
}

void MapView::CleanLightFan(LightSource* ls)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(ls->Applied);

    ls->Applied = false;

    if (IsBitSet(ls->Flags, LIGHT_GLOBAL)) {
        RUNTIME_ASSERT(_globalLights > 0);

        _globalLights--;
    }

    for (auto&& [hx, hy] : ls->MarkedHexes) {
        auto& field = _hexField->GetCellForWriting(hx, hy);

        field.LightSources.erase(ls);
        CalculateHexLight(hx, hy, field);

        if constexpr (FO_DEBUG) {
            if (field.IsView) {
                RUNTIME_ASSERT(_visibleLightSources[ls] > 0);

                _visibleLightSources[ls]--;
            }
        }
    }

    if (!ls->FanHexes.empty()) {
        _needRebuildLightPrimitives = true;
    }

    if constexpr (FO_DEBUG) {
        RUNTIME_ASSERT(_visibleLightSources.count(ls) == 0 || _visibleLightSources[ls] == 0);
    }

    _visibleLightSources.erase(ls);
}

void MapView::TraceLightLine(LightSource* ls, uint16 from_hx, uint16 from_hy, uint16& hx, uint16& hy, uint distance, uint intensity)
{
    NO_STACK_TRACE_ENTRY();

    const auto [base_sx, base_sy] = GenericUtils::GetStepsCoords(from_hx, from_hy, hx, hy);
    const auto sx1_f = base_sx;
    const auto sy1_f = base_sy;

    auto curx1_f = static_cast<float>(from_hx);
    auto cury1_f = static_cast<float>(from_hy);
    auto curx1_i = static_cast<int>(from_hx);
    auto cury1_i = static_cast<int>(from_hy);

    auto cur_inten = intensity;
    const auto inten_sub = intensity / distance;

    for (;;) {
        cur_inten -= inten_sub;
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

        // Left&Right trace
        auto ox = 0;
        auto oy = 0;

        if ((old_curx1_i % 2) != 0) {
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

            if (ox < 0 || ox >= _width || _hexField->GetCellForReading(static_cast<uint16>(ox), static_cast<uint16>(old_cury1_i)).Flags.LightBlocked) {
                hx = static_cast<uint16>(ox < 0 || ox >= _width ? old_curx1_i : ox);
                hy = static_cast<uint16>(old_cury1_i);

                MarkLightEnd(ls, static_cast<uint16>(old_curx1_i), static_cast<uint16>(old_cury1_i), hx, hy, cur_inten);
                break;
            }

            MarkLightStep(ls, static_cast<uint16>(old_curx1_i), static_cast<uint16>(old_cury1_i), static_cast<uint16>(ox), static_cast<uint16>(old_cury1_i), cur_inten);

            // Right side
            oy = old_cury1_i + oy;

            if (oy < 0 || oy >= _height || _hexField->GetCellForReading(static_cast<uint16>(old_curx1_i), static_cast<uint16>(oy)).Flags.LightBlocked) {
                hx = static_cast<uint16>(old_curx1_i);
                hy = static_cast<uint16>(oy < 0 || oy >= _height ? old_cury1_i : oy);

                MarkLightEnd(ls, static_cast<uint16>(old_curx1_i), static_cast<uint16>(old_cury1_i), hx, hy, cur_inten);
                break;
            }

            MarkLightStep(ls, static_cast<uint16>(old_curx1_i), static_cast<uint16>(old_cury1_i), static_cast<uint16>(old_curx1_i), static_cast<uint16>(oy), cur_inten);
        }

        // Main trace
        if (curx1_i < 0 || curx1_i >= _width || cury1_i < 0 || cury1_i >= _height || _hexField->GetCellForReading(static_cast<uint16>(curx1_i), static_cast<uint16>(cury1_i)).Flags.LightBlocked) {
            hx = static_cast<uint16>(curx1_i < 0 || curx1_i >= _width ? old_curx1_i : curx1_i);
            hy = static_cast<uint16>(cury1_i < 0 || cury1_i >= _height ? old_cury1_i : cury1_i);

            MarkLightEnd(ls, static_cast<uint16>(old_curx1_i), static_cast<uint16>(old_cury1_i), hx, hy, cur_inten);
            break;
        }

        MarkLightEnd(ls, static_cast<uint16>(old_curx1_i), static_cast<uint16>(old_cury1_i), static_cast<uint16>(curx1_i), static_cast<uint16>(cury1_i), cur_inten);

        if (curx1_i == hx && cury1_i == hy) {
            break;
        }
    }
}

void MapView::MarkLightStep(LightSource* ls, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy, uint intensity)
{
    NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(to_hx, to_hy);

    if (field.Flags.HasTransparentWall) {
        const bool north_south = field.Corner == CornerType::NorthSouth || field.Corner == CornerType::North || field.Corner == CornerType::West;
        const auto dir = GeometryHelper::GetFarDir(from_hx, from_hy, to_hx, to_hy);

        if (dir == 0 || (north_south && dir == 1) || (!north_south && (dir == 4 || dir == 5))) {
            MarkLight(ls, to_hx, to_hy, intensity);
        }
    }
    else {
        MarkLight(ls, to_hx, to_hy, intensity);
    }
}

void MapView::MarkLightEnd(LightSource* ls, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy, uint intensity)
{
    NO_STACK_TRACE_ENTRY();

    bool is_wall = false;
    bool north_south = false;
    const auto& field = _hexField->GetCellForReading(to_hx, to_hy);

    if (field.Flags.HasWall) {
        is_wall = true;

        if (field.Corner == CornerType::NorthSouth || field.Corner == CornerType::North || field.Corner == CornerType::West) {
            north_south = true;
        }
    }

    const int dir = GeometryHelper::GetFarDir(from_hx, from_hy, to_hx, to_hy);

    if (dir == 0 || (north_south && dir == 1) || (!north_south && (dir == 4 || dir == 5))) {
        MarkLight(ls, to_hx, to_hy, intensity);

        if (is_wall) {
            if (north_south) {
                if (to_hy > 0) {
                    MarkLightEndNeighbor(ls, to_hx, to_hy - 1, true, intensity);
                }
                if (to_hy < _height - 1) {
                    MarkLightEndNeighbor(ls, to_hx, to_hy + 1, true, intensity);
                }
            }
            else {
                if (to_hx > 0) {
                    MarkLightEndNeighbor(ls, to_hx - 1, to_hy, false, intensity);

                    if (to_hy > 0) {
                        MarkLightEndNeighbor(ls, to_hx - 1, to_hy - 1, false, intensity);
                    }
                    if (to_hy < _height - 1) {
                        MarkLightEndNeighbor(ls, to_hx - 1, to_hy + 1, false, intensity);
                    }
                }
                if (to_hx < _width - 1) {
                    MarkLightEndNeighbor(ls, to_hx + 1, to_hy, false, intensity);

                    if (to_hy > 0) {
                        MarkLightEndNeighbor(ls, to_hx + 1, to_hy - 1, false, intensity);
                    }
                    if (to_hy < _height - 1) {
                        MarkLightEndNeighbor(ls, to_hx + 1, to_hy + 1, false, intensity);
                    }
                }
            }
        }
    }
}

void MapView::MarkLightEndNeighbor(LightSource* ls, uint16 hx, uint16 hy, bool north_south, uint intensity)
{
    NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    if (field.Flags.HasWall) {
        const auto corner = field.Corner;

        if ((north_south && (corner == CornerType::NorthSouth || corner == CornerType::North || corner == CornerType::West)) || (!north_south && (corner == CornerType::EastWest || corner == CornerType::East)) || corner == CornerType::South) {
            MarkLight(ls, hx, hy, intensity / 2);
        }
    }
}

void MapView::MarkLight(LightSource* ls, uint16 hx, uint16 hy, uint intensity)
{
    NO_STACK_TRACE_ENTRY();

    const auto light_value = static_cast<int>(intensity) * MAX_LIGHT_HEX / MAX_LIGHT_INTEN * ls->Capacity / 100;
    const auto light_value_r = static_cast<uint8>(light_value * ls->CenterColor.comp.r / 255);
    const auto light_value_g = static_cast<uint8>(light_value * ls->CenterColor.comp.g / 255);
    const auto light_value_b = static_cast<uint8>(light_value * ls->CenterColor.comp.b / 255);
    const auto light_color = ucolor {light_value_r, light_value_g, light_value_b, 0};

    auto& field = _hexField->GetCellForWriting(hx, hy);
    const auto it = field.LightSources.find(ls);

    if (it == field.LightSources.end()) {
        field.LightSources.emplace(ls, light_color);
        ls->MarkedHexes.emplace_back(hx, hy);
        CalculateHexLight(hx, hy, field);

        if (field.IsView) {
            _visibleLightSources[ls]++;
        }
    }
    else {
        auto& cur_color = it->second;

        if (light_color.comp.r > cur_color.comp.r || light_color.comp.g > cur_color.comp.g || light_color.comp.b > cur_color.comp.b) {
            cur_color = light_color;
            CalculateHexLight(hx, hy, field);
        }
    }
}

void MapView::CalculateHexLight(uint16 hx, uint16 hy, const Field& field)
{
    NO_STACK_TRACE_ENTRY();

    auto& hex_light = _hexLight[hy * _width + hx];

    hex_light = {};

    for (auto&& ls_pair : field.LightSources) {
        hex_light.comp.r = std::max(hex_light.comp.r, ls_pair.second.comp.r);
        hex_light.comp.g = std::max(hex_light.comp.g, ls_pair.second.comp.g);
        hex_light.comp.b = std::max(hex_light.comp.b, ls_pair.second.comp.b);
    }
}

void MapView::LightFanToPrimitves(const LightSource* ls, vector<PrimitivePoint>& points, vector<PrimitivePoint>& soft_points) const
{
    NO_STACK_TRACE_ENTRY();

    if (ls->FanHexes.size() <= 1) {
        return;
    }

    int center_x = 0;
    int center_y = 0;
    GetHexCurrentPosition(ls->HexX, ls->HexY, center_x, center_y);
    center_x += _engine->Settings.MapHexWidth / 2;
    center_y += _engine->Settings.MapHexHeight / 2;

    const auto center_point = PrimitivePoint {center_x, center_y, ls->CenterColor, ls->OffsX, ls->OffsY};

    const auto points_start_size = points.size();
    points.reserve(points.size() + ls->FanHexes.size() * 3);
    soft_points.reserve(soft_points.size() + ls->FanHexes.size() * 3);

    for (size_t i = 0; i < ls->FanHexes.size(); i++) {
        const auto& fan_hex = ls->FanHexes[i];
        const uint16 hx = std::get<0>(fan_hex);
        const uint16 hy = std::get<1>(fan_hex);
        const uint8 alpha = std::get<2>(fan_hex);
        const bool use_offsets = std::get<3>(fan_hex);

        const auto [x, y] = _engine->Geometry.GetHexInterval(ls->HexX, ls->HexY, hx, hy);
        const auto edge_point = PrimitivePoint {center_x + x, center_y + y, ucolor {ls->CenterColor, alpha}, use_offsets ? ls->OffsX : nullptr, use_offsets ? ls->OffsY : nullptr};

        points.emplace_back(edge_point);

        if (i > 0) {
            points.emplace_back(center_point);
            points.emplace_back(edge_point);

            if (i == ls->FanHexes.size() - 1) {
                points.emplace_back(points[points_start_size]);
                points.emplace_back(center_point);
            }
        }
    }

    RUNTIME_ASSERT(points.size() % 3 == 0);

    for (size_t i = points_start_size; i < points.size(); i += 3) {
        const auto& cur = points[i];
        const auto& next = points[i + 1];

        if (GenericUtils::DistSqrt(cur.PointX, cur.PointY, next.PointX, next.PointY) > static_cast<uint>(_engine->Settings.MapHexWidth)) {
            soft_points.emplace_back(PrimitivePoint {next.PointX, next.PointY, next.PointColor, next.PointOffsX, next.PointOffsY, next.PPointColor});
            soft_points.emplace_back(PrimitivePoint {cur.PointX, cur.PointY, cur.PointColor, cur.PointOffsX, cur.PointOffsY, cur.PPointColor});

            const auto dist_comp = GenericUtils::DistSqrt(center_x, center_y, cur.PointX, cur.PointY) > GenericUtils::DistSqrt(center_x, center_y, next.PointX, next.PointY);
            auto x = static_cast<float>(dist_comp ? next.PointX - cur.PointX : cur.PointX - next.PointX);
            auto y = static_cast<float>(dist_comp ? next.PointY - cur.PointY : cur.PointY - next.PointY);
            std::tie(x, y) = GenericUtils::ChangeStepsCoords(x, y, dist_comp ? -2.5f : 2.5f);

            if (dist_comp) {
                soft_points.emplace_back(PrimitivePoint {cur.PointX + iround(x), cur.PointY + iround(y), cur.PointColor, cur.PointOffsX, cur.PointOffsY, cur.PPointColor});
            }
            else {
                soft_points.emplace_back(PrimitivePoint {next.PointX + iround(x), next.PointY + iround(y), next.PointColor, next.PointOffsX, next.PointOffsY, next.PPointColor});
            }
        }
    }
}

void MapView::SetSkipRoof(uint16 hx, uint16 hy)
{
    STACK_TRACE_ENTRY();

    if (_roofSkip != _hexField->GetCellForReading(hx, hy).RoofNum) {
        _roofSkip = _hexField->GetCellForReading(hx, hy).RoofNum;
        RefreshMap();
    }
}

void MapView::MarkRoofNum(int hxi, int hyi, int16 num)
{
    STACK_TRACE_ENTRY();

    if (hxi < 0 || hyi < 0 || hxi >= _width || hyi >= _height) {
        return;
    }

    const auto hx = static_cast<uint16>(hxi);
    const auto hy = static_cast<uint16>(hyi);
    if (_hexField->GetCellForReading(hx, hy).RoofTiles.empty()) {
        return;
    }
    if (_hexField->GetCellForReading(hx, hy).RoofNum != 0) {
        return;
    }

    for (auto x = 0; x < _engine->Settings.MapTileStep; x++) {
        for (auto y = 0; y < _engine->Settings.MapTileStep; y++) {
            if (hxi + x >= 0 && hxi + x < _width && hyi + y >= 0 && hyi + y < _height) {
                _hexField->GetCellForWriting(static_cast<uint16>(hxi + x), static_cast<uint16>(hyi + y)).RoofNum = num;
            }
        }
    }

    MarkRoofNum(hxi + _engine->Settings.MapTileStep, hy, num);
    MarkRoofNum(hxi - _engine->Settings.MapTileStep, hy, num);
    MarkRoofNum(hxi, hyi + _engine->Settings.MapTileStep, num);
    MarkRoofNum(hxi, hyi - _engine->Settings.MapTileStep, num);
}

auto MapView::IsVisible(const Sprite* spr, int ox, int oy) const -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(spr);

    const auto top = oy + spr->OffsY - spr->Height - _engine->Settings.MapHexLineHeight * 2;
    const auto bottom = oy + spr->OffsY + _engine->Settings.MapHexLineHeight * 2;
    const auto left = ox + spr->OffsX - spr->Width / 2 - _engine->Settings.MapHexWidth;
    const auto right = ox + spr->OffsX + spr->Width / 2 + _engine->Settings.MapHexWidth;
    const auto zoomed_screen_height = iround(std::ceil(static_cast<float>(_engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight) * GetSpritesZoom()));
    const auto zoomed_screen_width = iround(std::ceil(static_cast<float>(_engine->Settings.ScreenWidth) * GetSpritesZoom()));

    return top <= zoomed_screen_height && bottom >= 0 && left <= zoomed_screen_width && right >= 0;
}

auto MapView::MeasureMapBorders(const Sprite* spr, int ox, int oy) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(spr);

    const auto top = std::max(spr->OffsY + oy - _hTop * _engine->Settings.MapHexLineHeight + _engine->Settings.MapHexLineHeight * 2, 0);
    const auto bottom = std::max(spr->Height - spr->OffsY - oy - _hBottom * _engine->Settings.MapHexLineHeight + _engine->Settings.MapHexLineHeight * 2, 0);
    const auto left = std::max(spr->Width / 2 + spr->OffsX + ox - _wLeft * _engine->Settings.MapHexWidth + _engine->Settings.MapHexWidth, 0);
    const auto right = std::max(spr->Width / 2 - spr->OffsX - ox - _wRight * _engine->Settings.MapHexWidth + _engine->Settings.MapHexWidth, 0);

    if (top > 0 || bottom > 0 || left > 0 || right > 0) {
        _hTop += top / _engine->Settings.MapHexLineHeight + ((top % _engine->Settings.MapHexLineHeight) != 0 ? 1 : 0);
        _hBottom += bottom / _engine->Settings.MapHexLineHeight + ((bottom % _engine->Settings.MapHexLineHeight) != 0 ? 1 : 0);
        _wLeft += left / _engine->Settings.MapHexWidth + ((left % _engine->Settings.MapHexWidth) != 0 ? 1 : 0);
        _wRight += right / _engine->Settings.MapHexWidth + ((right % _engine->Settings.MapHexWidth) != 0 ? 1 : 0);

        if (!_mapLoading) {
            ResizeView();
            RefreshMap();
        }

        return true;
    }

    return false;
}

auto MapView::MeasureMapBorders(const ItemHexView* item) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item->GetMap() == this);

    return MeasureMapBorders(item->Spr, item->ScrX, item->ScrY);
}

void MapView::RecacheHexFlags(uint16 hx, uint16 hy)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(hx < _width);
    RUNTIME_ASSERT(hy < _height);

    auto& field = _hexField->GetCellForWriting(hx, hy);

    RecacheHexFlags(field);
}

void MapView::RecacheHexFlags(Field& field)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    field.Flags.HasWall = false;
    field.Flags.HasTransparentWall = false;
    field.Flags.HasScenery = false;
    field.Flags.MoveBlocked = false;
    field.Flags.ShootBlocked = false;
    field.Flags.LightBlocked = false;
    field.Flags.ScrollBlock = false;
    field.Corner = CornerType::NorthSouth;

    if (_engine->Settings.CritterBlockHex) {
        if (!field.Critters.empty()) {
            for (const auto* cr : field.Critters) {
                if (!field.Flags.MoveBlocked && !cr->IsDead()) {
                    field.Flags.MoveBlocked = true;
                }
            }
        }

        if (!field.MultihexCritters.empty()) {
            for (const auto* cr : field.MultihexCritters) {
                if (!field.Flags.MoveBlocked && !cr->IsDead()) {
                    field.Flags.MoveBlocked = true;
                }
            }
        }
    }

    if (!field.Items.empty()) {
        for (const auto* item : field.Items) {
            if (!field.Flags.HasWall && item->GetIsWall()) {
                field.Flags.HasWall = true;
                field.Flags.HasTransparentWall = item->GetLightThru();

                field.Corner = item->GetCorner();
            }
            else if (!field.Flags.HasScenery && item->GetIsScenery()) {
                field.Flags.HasScenery = true;

                if (!field.Flags.HasWall) {
                    field.Corner = item->GetCorner();
                }
            }

            if (!field.Flags.MoveBlocked && !item->GetNoBlock()) {
                field.Flags.MoveBlocked = true;
            }
            if (!field.Flags.ShootBlocked && !item->GetShootThru()) {
                field.Flags.ShootBlocked = true;
            }
            if (!field.Flags.ScrollBlock && item->GetScrollBlock()) {
                field.Flags.ScrollBlock = true;
            }
            if (!field.Flags.LightBlocked && !item->GetLightThru()) {
                field.Flags.LightBlocked = true;
            }
        }
    }

    if (!field.BlockLineItems.empty()) {
        for (const auto* item : field.BlockLineItems) {
            field.Flags.MoveBlocked = true;

            if (!field.Flags.ShootBlocked && !item->GetShootThru()) {
                field.Flags.ShootBlocked = true;
            }
            if (!field.Flags.LightBlocked && !item->GetLightThru()) {
                field.Flags.LightBlocked = true;
            }
        }
    }

    if (field.Flags.ShootBlocked) {
        field.Flags.MoveBlocked = true;
    }
}

void MapView::Resize(uint16 width, uint16 height)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    RUNTIME_ASSERT(width >= MAXHEX_MIN && width <= MAXHEX_MAX);
    RUNTIME_ASSERT(height >= MAXHEX_MIN && height <= MAXHEX_MAX);

    for (int i = 0, j = _hVisible * _wVisible; i < j; i++) {
        const auto& vf = _viewField[i];
        if (vf.HexX >= 0 && vf.HexY >= 0 && vf.HexX < _width && vf.HexY < _height) {
            auto& field = _hexField->GetCellForWriting(static_cast<uint16>(vf.HexX), static_cast<uint16>(vf.HexY));
            field.IsView = false;
            InvalidateSpriteChain(field);
        }
    }

    // Remove objects on shrink
    for (uint16 hy = 0; hy < std::max(height, _height); hy++) {
        for (uint16 hx = 0; hx < std::max(width, _width); hx++) {
            if (hx >= width || hy >= height) {
                const auto& field = _hexField->GetCellForReading(hx, hy);

                if (!field.Critters.empty()) {
                    for (auto* cr : copy(field.Critters)) {
                        DestroyCritter(cr);
                    }
                }

                if (!field.Items.empty()) {
                    for (auto* item : copy(field.Items)) {
                        DestroyItem(item);
                    }
                }

                if (!field.GroundTiles.empty()) {
                    for (auto* tile : copy(field.GroundTiles)) {
                        DestroyItem(tile);
                    }
                }

                if (!field.RoofTiles.empty()) {
                    for (auto* tile : copy(field.RoofTiles)) {
                        DestroyItem(tile);
                    }
                }

                RUNTIME_ASSERT(field.Critters.empty());
                RUNTIME_ASSERT(field.Items.empty());
                RUNTIME_ASSERT(field.GroundTiles.empty());
                RUNTIME_ASSERT(field.RoofTiles.empty());
                RUNTIME_ASSERT(!field.SpriteChain);
            }
        }
    }

    SetWidth(width);
    SetHeight(height);
    _width = width;
    _height = height;
    _screenHexX = std::min(_screenHexX, _width - 1);
    _screenHexY = std::min(_screenHexY, _height - 1);

    _hexTrack.resize(static_cast<size_t>(_width) * _height);
    std::memset(_hexTrack.data(), 0, _hexTrack.size());
    _hexLight.resize(static_cast<size_t>(_width) * _height);
    _hexField->Resize(_width, _height);

    for (uint16 hy = 0; hy < _height; hy++) {
        for (uint16 hx = 0; hx < _width; hx++) {
            CalculateHexLight(hx, hy, _hexField->GetCellForReading(hx, hy));
        }
    }

    RefreshMap();
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

void MapView::InitView(int screen_hx, int screen_hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        // Get center offset
        const auto hw = GetViewWidth() / 2 + _wRight;
        const auto hv = GetViewHeight() / 2 + _hTop;
        auto vw = hv / 2 + std::abs(hv % 2) + 1;
        auto vh = hv - vw / 2 - 1;
        for (auto i = 0; i < hw; i++) {
            if ((vw % 2) != 0) {
                vh--;
            }
            vw++;
        }

        // Subtract offset
        screen_hx -= abs(vw);
        screen_hy -= abs(vh);

        const auto xa = -(_wRight * _engine->Settings.MapHexWidth);
        const auto xb = -(_engine->Settings.MapHexWidth / 2) - (_wRight * _engine->Settings.MapHexWidth);
        auto oy = -_engine->Settings.MapHexLineHeight * _hTop;
        const auto wx = iround(static_cast<float>(_engine->Settings.ScreenWidth) * GetSpritesZoom());

        for (auto yv = 0; yv < _hVisible; yv++) {
            auto hx = screen_hx + yv / 2 + std::abs(yv % 2);
            auto hy = screen_hy + (yv - (hx - screen_hx - std::abs(screen_hx % 2)) / 2);
            auto ox = (yv % 2) != 0 ? xa : xb;

            if (yv == 0 && (screen_hx % 2) != 0) {
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

                if ((hx % 2) != 0) {
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
        auto basehx = screen_hx - halfh / 2 - halfw;
        auto basehy = screen_hy - halfh / 2 + halfw;
        auto y2 = 0;
        auto xa = -_engine->Settings.MapHexWidth * _wRight;
        auto xb = -_engine->Settings.MapHexWidth * _wRight - _engine->Settings.MapHexWidth / 2;
        auto y = -_engine->Settings.MapHexLineHeight * _hTop;
        const auto wx = iround(static_cast<float>(_engine->Settings.ScreenWidth) * GetSpritesZoom());

        // Initialize field
        for (auto j = 0; j < _hVisible; j++) {
            auto x = (j % 2) != 0 ? xa : xb;
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

            if ((j % 2) != 0) {
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
        for (int i = 0, j = _hVisible * _wVisible; i < j; i++) {
            const auto& vf = _viewField[i];
            if (vf.HexX >= 0 && vf.HexY >= 0 && vf.HexX < _width && vf.HexY < _height) {
                auto& field = _hexField->GetCellForWriting(static_cast<uint16>(vf.HexX), static_cast<uint16>(vf.HexY));
                field.IsView = false;
                InvalidateSpriteChain(field);
            }
        }
    }

    _hVisible = GetViewHeight() + _hTop + _hBottom;
    _wVisible = GetViewWidth() + _wLeft + _wRight;

    _viewField.resize(static_cast<size_t>(_hVisible) * _wVisible);
}

void MapView::AddSpriteToChain(Field& field, MapSprite* mspr)
{
    NO_STACK_TRACE_ENTRY();

    if (field.SpriteChain == nullptr) {
        field.SpriteChain = mspr;
        mspr->ExtraChainRoot = &field.SpriteChain;
    }
    else {
        auto* last_spr = field.SpriteChain;
        while (last_spr->ExtraChainChild != nullptr) {
            last_spr = last_spr->ExtraChainChild;
        }
        last_spr->ExtraChainChild = mspr;
        mspr->ExtraChainParent = last_spr;
    }
}

void MapView::InvalidateSpriteChain(Field& field)
{
    NO_STACK_TRACE_ENTRY();

    // SpriteChain changed outside loop
    if (field.SpriteChain != nullptr) {
        RUNTIME_ASSERT(field.SpriteChain->Valid);
        while (field.SpriteChain != nullptr) {
            RUNTIME_ASSERT(field.SpriteChain->Valid);
            field.SpriteChain->Invalidate();
        }
    }
}

void MapView::ChangeZoom(int zoom)
{
    STACK_TRACE_ENTRY();

    if (is_float_equal(_engine->Settings.SpritesZoomMin, _engine->Settings.SpritesZoomMax)) {
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
        const auto width = static_cast<float>(_engine->Settings.ScreenWidth / _engine->Settings.MapHexWidth + ((_engine->Settings.ScreenWidth % _engine->Settings.MapHexWidth) != 0 ? 1 : 0));
        SetSpritesZoom((width * GetSpritesZoom() + (zoom >= 0 ? 2.0f : -2.0f)) / width);

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

auto MapView::GetScreenHexes() const -> tuple<int, int>
{
    STACK_TRACE_ENTRY();

    return {_screenHexX, _screenHexY};
}

void MapView::GetHexCurrentPosition(uint16 hx, uint16 hy, int& x, int& y) const
{
    STACK_TRACE_ENTRY();

    const auto& center_hex = _viewField[_hVisible / 2 * _wVisible + _wVisible / 2];
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
    ProcessLighting();

    // Prepare fog
    PrepareFogToDraw();

    // Prerendered offsets
    const auto ox = _rtScreenOx - iround(static_cast<float>(_engine->Settings.ScrOx) / GetSpritesZoom());
    const auto oy = _rtScreenOy - iround(static_cast<float>(_engine->Settings.ScrOy) / GetSpritesZoom());
    const auto prerendered_rect = IRect(ox, oy, ox + _engine->Settings.ScreenWidth, oy + (_engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight));

    // Separate render target
    if (_engine->EffectMngr.Effects.FlushMap != nullptr) {
        if (_rtMap == nullptr) {
            _rtMap = _engine->SprMngr.GetRtMngr().CreateRenderTarget(false, RenderTarget::SizeType::Map, 0, 0, false);
        }

        _rtMap->CustomDrawEffect = _engine->EffectMngr.Effects.FlushMap;

        _engine->SprMngr.GetRtMngr().PushRenderTarget(_rtMap);
        _engine->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);
    }

    // Tiles
    _engine->SprMngr.DrawSprites(_mapSprites, false, false, DrawOrderType::Tile, DrawOrderType::Tile4, _mapDayColor);

    // Flat sprites
    _engine->SprMngr.DrawSprites(_mapSprites, true, false, DrawOrderType::HexGrid, DrawOrderType::FlatScenery, _mapDayColor);

    // Lighting
    if (_rtLight != nullptr) {
        _engine->SprMngr.DrawRenderTarget(_rtLight, true, &prerendered_rect);
    }

    // Cursor flat
    if (_cursorPrePic != nullptr) {
        DrawCursor(_cursorPrePic.get());
    }

    // Sprites
    _engine->SprMngr.DrawSprites(_mapSprites, true, true, DrawOrderType::Ligth, DrawOrderType::Last, _mapDayColor);

    // Contours
    _engine->SprMngr.DrawContours();

    // Fog
    if (!_mapperMode && _rtFog != nullptr) {
        _engine->SprMngr.DrawRenderTarget(_rtFog, true, &prerendered_rect);
    }

    // Cursor
    if (_cursorPostPic != nullptr) {
        DrawCursor(_cursorPostPic.get());
    }

    if (_cursorXPic != nullptr) {
        if (_drawCursorX < 0) {
            DrawCursor(_cursorXPic.get());
        }
        else if (_drawCursorX > 0) {
            DrawCursor(strex("{}", _drawCursorX));
        }
    }

    // Draw map from render target
    if (_engine->EffectMngr.Effects.FlushMap != nullptr) {
        _engine->SprMngr.GetRtMngr().PopRenderTarget();
        _engine->SprMngr.DrawRenderTarget(_rtMap, false);
    }
}

void MapView::DrawMapTexts()
{
    STACK_TRACE_ENTRY();

    for (auto* cr : _critters) {
        cr->DrawName();
    }

    const auto time = _engine->GameTime.GameplayTime();

    for (auto it = _mapTexts.begin(); it != _mapTexts.end();) {
        const auto& map_text = *it;

        if (time < map_text.StartTime + map_text.Duration) {
            const auto& field = _hexField->GetCellForReading(map_text.HexX, map_text.HexY);

            if (field.IsView) {
                const auto dt = time_duration_to_ms<uint>(time - map_text.StartTime);
                const auto percent = GenericUtils::Percent(time_duration_to_ms<uint>(map_text.Duration), dt);
                const auto text_pos = map_text.Pos.Interpolate(map_text.EndPos, static_cast<int>(percent));
                const auto half_hex_width = _engine->Settings.MapHexWidth / 2;
                const auto half_hex_height = _engine->Settings.MapHexHeight / 2;
                const auto x = iround(static_cast<float>(field.ScrX + half_hex_width + _engine->Settings.ScrOx) / GetSpritesZoom() - 100.0f - static_cast<float>(map_text.Pos.Left - text_pos.Left));
                const auto y = iround(static_cast<float>(field.ScrY + half_hex_height - map_text.Pos.Height() - (map_text.Pos.Top - text_pos.Top) + _engine->Settings.ScrOy) / GetSpritesZoom() - 70.0f);

                auto color = map_text.Color;

                if (map_text.Fade) {
                    const auto alpha = 255 * (100 - percent) / 100;
                    color.comp.a = static_cast<uint8>(alpha);
                }
                else if (map_text.Duration > std::chrono::milliseconds {500}) {
                    const auto hide = time_duration_to_ms<uint>(map_text.Duration - std::chrono::milliseconds {200});
                    if (dt >= hide) {
                        const auto alpha = 255 * (100 - GenericUtils::Percent(time_duration_to_ms<uint>(map_text.Duration) - hide, dt - hide)) / 100;
                        color.comp.a = static_cast<uint8>(alpha);
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

        const auto it = std::find_if(_critters.begin(), _critters.end(), [](auto* cr) { return cr->GetIsChosen(); });
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
            const auto dist_shoot = _shootBordersDist;
            const auto half_hw = _engine->Settings.MapHexWidth / 2;
            const auto half_hh = _engine->Settings.MapHexHeight / 2;

            auto base_x = 0;
            auto base_y = 0;
            GetHexCurrentPosition(base_hx, base_hy, base_x, base_y);
            const auto center_look_point = PrimitivePoint {base_x + half_hw, base_y + half_hh, ucolor {0, 0, 0, 0}, &chosen->ScrX, &chosen->ScrY};
            const auto center_shoot_point = PrimitivePoint {base_x + half_hw, base_y + half_hh, ucolor {0, 0, 0, 255}, &chosen->ScrX, &chosen->ScrY};

            size_t look_points_added = 0;
            size_t shoot_points_added = 0;

            auto seek_start = true;
            for (auto i = 0; i < (GameSettings::HEXAGONAL_GEOMETRY ? 6 : 4); i++) {
                const auto dir = (GameSettings::HEXAGONAL_GEOMETRY ? (i + 2) % 6 : ((i + 1) * 2) % 8);

                for (int j = 0, jj = static_cast<int>(GameSettings::HEXAGONAL_GEOMETRY ? dist : dist * 2); j < jj; j++) {
                    if (seek_start) {
                        // Move to start position
                        for (uint l = 0; l < dist; l++) {
                            GeometryHelper::MoveHexByDirUnsafe(hx, hy, GameSettings::HEXAGONAL_GEOMETRY ? 0 : 7);
                        }
                        seek_start = false;
                        j = -1;
                    }
                    else {
                        // Move to next hex
                        GeometryHelper::MoveHexByDirUnsafe(hx, hy, static_cast<uint8>(dir));
                    }

                    auto hx_ = static_cast<uint16>(std::clamp(hx, 0, _width - 1));
                    auto hy_ = static_cast<uint16>(std::clamp(hy, 0, _height - 1));
                    if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
                        const int dir_ = GeometryHelper::GetFarDir(base_hx, base_hy, hx_, hy_);
                        auto ii = (chosen_dir > dir_ ? chosen_dir - dir_ : dir_ - chosen_dir);
                        if (ii > static_cast<int>(GameSettings::MAP_DIR_COUNT / 2)) {
                            ii = static_cast<int>(GameSettings::MAP_DIR_COUNT - ii);
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

                    auto dist_look = GeometryHelper::DistGame(base_hx, base_hy, hx_, hy_);
                    if (_drawLookBorders) {
                        auto x = 0;
                        auto y = 0;
                        GetHexCurrentPosition(hx_, hy_, x, y);
                        const auto* ox = (dist_look == dist ? &chosen->ScrX : nullptr);
                        const auto* oy = (dist_look == dist ? &chosen->ScrY : nullptr);
                        _fogLookPoints.emplace_back(PrimitivePoint {x + half_hw, y + half_hh, ucolor {255, static_cast<uint8>(dist_look * 255 / dist), 0, 0}, ox, oy});
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
                        const auto result_shoot_dist = GeometryHelper::DistGame(base_hx, base_hy, hx_2, hy_2);
                        const auto* ox = (result_shoot_dist == max_shoot_dist ? &chosen->ScrX : nullptr);
                        const auto* oy = (result_shoot_dist == max_shoot_dist ? &chosen->ScrY : nullptr);
                        _fogShootPoints.emplace_back(PrimitivePoint {x_ + half_hw, y_ + half_hh, ucolor {255, static_cast<uint8>(result_shoot_dist * 255 / max_shoot_dist), 0, 255}, ox, oy});
                        if (++shoot_points_added % 2 == 0) {
                            _fogShootPoints.emplace_back(center_shoot_point);
                        }
                    }
                }
            }

            _fogOffsX = chosen != nullptr ? &chosen->ScrX : nullptr;
            _fogOffsY = chosen != nullptr ? &chosen->ScrY : nullptr;
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
        const float zoom = GetSpritesZoom();

        _engine->SprMngr.GetRtMngr().PushRenderTarget(_rtFog);
        _engine->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);
        _engine->SprMngr.DrawPoints(_fogLookPoints, RenderPrimitiveType::TriangleStrip, &zoom, &offset, _engine->EffectMngr.Effects.Fog);
        _engine->SprMngr.DrawPoints(_fogShootPoints, RenderPrimitiveType::TriangleStrip, &zoom, &offset, _engine->EffectMngr.Effects.Fog);
        _engine->SprMngr.GetRtMngr().PopRenderTarget();
    }
}

auto MapView::IsScrollEnabled() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _engine->Settings.ScrollMouseLeft || _engine->Settings.ScrollKeybLeft || _engine->Settings.ScrollMouseRight || //
        _engine->Settings.ScrollKeybRight || _engine->Settings.ScrollMouseUp || _engine->Settings.ScrollKeybUp || //
        _engine->Settings.ScrollMouseDown || _engine->Settings.ScrollKeybDown;
}

auto MapView::Scroll() -> bool
{
    STACK_TRACE_ENTRY();

    // Scroll delay
    auto time_k = 1.0f;

    if (_engine->Settings.ScrollDelay != 0) {
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

        if (GenericUtils::DistSqrt(0, 0, iround(AutoScroll.OffsX), iround(AutoScroll.OffsY)) == 0) {
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

auto MapView::ScrollCheckPos(int (&positions)[4], int dir1, int dir2) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto max_pos = _wVisible * _hVisible;
    for (const auto pos : positions) {
        if (pos < 0 || pos >= max_pos) {
            return true;
        }

        if (_viewField[pos].HexX < 0 || _viewField[pos].HexY < 0 || _viewField[pos].HexX >= _width || _viewField[pos].HexY >= _height) {
            return true;
        }

        auto hx = static_cast<uint16>(_viewField[pos].HexX);
        auto hy = static_cast<uint16>(_viewField[pos].HexY);

        GeometryHelper::MoveHexByDir(hx, hy, static_cast<uint8>(dir1), _width, _height);
        if (_hexField->GetCellForReading(hx, hy).Flags.ScrollBlock) {
            return true;
        }

        if (dir2 >= 0) {
            GeometryHelper::MoveHexByDir(hx, hy, static_cast<uint8>(dir2), _width, _height);
            if (_hexField->GetCellForReading(hx, hy).Flags.ScrollBlock) {
                return true;
            }
        }
    }
    return false;
}

auto MapView::ScrollCheck(int xmod, int ymod) const -> bool
{
    STACK_TRACE_ENTRY();

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

    auto&& [sx, sy] = GetScreenHexes();
    auto&& [ox, oy] = _engine->Geometry.GetHexInterval(sx, sy, hx, hy);

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
    RUNTIME_ASSERT(hx < _width && hy < _height);

    auto& field = _hexField->GetCellForWriting(hx, hy);

    vec_add_unique_value(field.Critters, cr);

    RecacheHexFlags(field);
    SetMultihexCritter(cr, true);
    UpdateCritterLightSource(cr);

    if (!_mapLoading && IsHexToDraw(hx, hy)) {
        auto* spr = cr->InsertSprite(_mapSprites, EvaluateCritterDrawOrder(cr), hx, hy, &field.ScrX, &field.ScrY);

        cr->RefreshOffs();
        cr->ResetOk();

        auto contour = ContourType::None;
        if (cr->GetId() == _critterContourCrId) {
            contour = _critterContour;
        }
        else if (!cr->IsDead() && !cr->GetIsChosen()) {
            contour = _crittersContour;
        }
        spr->SetContour(contour, cr->GetContourColor());

        AddSpriteToChain(field, spr);
    }
}

void MapView::RemoveCritterFromField(CritterHexView* cr)
{
    STACK_TRACE_ENTRY();

    const auto hx = cr->GetHexX();
    const auto hy = cr->GetHexY();

    auto& field = _hexField->GetCellForWriting(hx, hy);

    vec_remove_unique_value(field.Critters, cr);

    RecacheHexFlags(field);
    SetMultihexCritter(cr, false);
    FinishLightSource(cr->GetId());

    if (cr->IsSpriteValid()) {
        cr->InvalidateSprite();
    }
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

auto MapView::GetNonDeadCritter(uint16 hx, uint16 hy) -> CritterHexView*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    if (!field.Critters.empty()) {
        for (auto* cr : field.Critters) {
            if (!cr->IsDead()) {
                return cr;
            }
        }
    }

    if (!field.MultihexCritters.empty()) {
        for (auto* cr : field.MultihexCritters) {
            if (!cr->IsDead()) {
                return cr;
            }
        }
    }

    return nullptr;
}

auto MapView::AddReceivedCritter(ident_t id, hstring pid, uint16 hx, uint16 hy, int16 dir_angle, const vector<vector<uint8>>& data) -> CritterHexView*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(id);
    RUNTIME_ASSERT(hx < _width && hy < _height);

    const auto* proto = _engine->ProtoMngr.GetProtoCritter(pid);
    auto* cr = new CritterHexView(this, id, proto);

    cr->RestoreData(data);
    cr->SetHexX(hx);
    cr->SetHexY(hy);
    cr->ChangeDirAngle(dir_angle);

    return AddCritterInternal(cr);
}

auto MapView::AddMapperCritter(hstring pid, uint16 hx, uint16 hy, int16 dir_angle, const Properties* props) -> CritterHexView*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);
    RUNTIME_ASSERT(hx < _width && hy < _height);

    const auto* proto = _engine->ProtoMngr.GetProtoCritter(pid);
    auto* cr = new CritterHexView(this, GetTempEntityId(), proto, props);

    cr->SetHexX(hx);
    cr->SetHexY(hy);
    cr->ChangeDirAngle(dir_angle);

    return AddCritterInternal(cr);
}

auto MapView::AddCritterInternal(CritterHexView* cr) -> CritterHexView*
{
    STACK_TRACE_ENTRY();

    if (cr->GetId()) {
        if (auto* prev_cr = GetCritter(cr->GetId()); prev_cr != nullptr) {
            cr->RestoreFading(prev_cr->StoreFading());
            DestroyCritter(prev_cr);
        }

        _crittersMap.emplace(cr->GetId(), cr);
    }

    cr->Init();

    vec_add_unique_value(_critters, cr);

    AddCritterToField(cr);

    return cr;
}

void MapView::DestroyCritter(CritterHexView* cr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(cr->GetMap() == this);

    vec_remove_unique_value(_critters, cr);

    if (cr->GetId()) {
        const auto it_map = _crittersMap.find(cr->GetId());
        RUNTIME_ASSERT(it_map != _crittersMap.end());
        _crittersMap.erase(it_map);
    }

    RemoveCritterFromField(cr);
    CleanLightSourceOffsets(cr->GetId());

    cr->DestroySelf();
}

auto MapView::GetCritters() -> const vector<CritterHexView*>&
{
    STACK_TRACE_ENTRY();

    return _critters;
}

auto MapView::GetCritters(uint16 hx, uint16 hy, CritterFindType find_type) -> vector<CritterHexView*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<CritterHexView*> critters;
    const auto& field = _hexField->GetCellForReading(hx, hy);

    if (!field.Critters.empty()) {
        for (auto* cr : field.Critters) {
            if (cr->CheckFind(find_type)) {
                critters.emplace_back(cr);
            }
        }
    }

    if (!field.MultihexCritters.empty()) {
        for (auto* cr : field.MultihexCritters) {
            if (cr->CheckFind(find_type)) {
                critters.emplace_back(cr);
            }
        }
    }

    return critters;
}

void MapView::SetCritterContour(ident_t cr_id, ContourType contour)
{
    STACK_TRACE_ENTRY();

    if (_critterContourCrId == cr_id && _critterContour == contour) {
        return;
    }

    if (cr_id != _critterContourCrId) {
        auto* cr = GetCritter(_critterContourCrId);
        if (cr != nullptr && cr->IsSpriteValid()) {
            if (!cr->IsDead() && !cr->GetIsChosen()) {
                cr->GetSprite()->SetContour(_crittersContour);
            }
            else {
                cr->GetSprite()->SetContour(ContourType::None);
            }
        }
    }

    _critterContourCrId = cr_id;
    _critterContour = contour;

    if (cr_id) {
        auto* cr = GetCritter(cr_id);
        if (cr != nullptr && cr->IsSpriteValid()) {
            cr->GetSprite()->SetContour(contour);
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
        if (!cr->GetIsChosen() && cr->IsSpriteValid() && !cr->IsDead() && cr->GetId() != _critterContourCrId) {
            cr->GetSprite()->SetContour(contour);
        }
    }
}

void MapView::MoveCritter(CritterHexView* cr, uint16 hx, uint16 hy, bool smoothly)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(cr->GetMap() == this);
    RUNTIME_ASSERT(hx < _width && hy < _height);

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

    if (cr->GetIsChosen()) {
        RebuildFog();
    }
}

void MapView::SetMultihexCritter(CritterHexView* cr, bool set)
{
    STACK_TRACE_ENTRY();

    const uint multihex = cr->GetMultihex();

    if (multihex != 0) {
        const uint16 hx = cr->GetHexX();
        const uint16 hy = cr->GetHexY();
        auto&& [sx, sy] = _engine->Geometry.GetHexOffsets((hx % 2) != 0);

        for (uint i = 0, j = GenericUtils::NumericalNumber(multihex) * GameSettings::MAP_DIR_COUNT; i < j; i++) {
            const auto cx = static_cast<int>(hx) + sx[i];
            const auto cy = static_cast<int>(hy) + sy[i];

            if (cx >= 0 && cy >= 0 && cx < _width && cy < _height) {
                auto& field = _hexField->GetCellForWriting(static_cast<uint16>(cx), static_cast<uint16>(cy));

                if (set) {
                    RUNTIME_ASSERT(std::find(field.MultihexCritters.begin(), field.MultihexCritters.end(), cr) == field.MultihexCritters.end());
                    field.MultihexCritters.emplace_back(cr);
                }
                else {
                    const auto it = std::find(field.MultihexCritters.begin(), field.MultihexCritters.end(), cr);
                    RUNTIME_ASSERT(it != field.MultihexCritters.end());
                    field.MultihexCritters.erase(it);
                }

                RecacheHexFlags(field);
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
                if (_picHexMask) {
                    const auto mask_x = std::clamp(iround((xf - x_) * GetSpritesZoom()), 0, _picHexMask->Width - 1);
                    const auto mask_y = std::clamp(iround((yf - y_) * GetSpritesZoom()), 0, _picHexMask->Height - 1);
                    const auto mask_color = _picHexMaskData[mask_y * _picHexMask->Width + mask_x];
                    const auto mask_color_r = mask_color.comp.r;

                    if (mask_color_r == 50) {
                        GeometryHelper::MoveHexByDirUnsafe(hx_, hy_, GameSettings::HEXAGONAL_GEOMETRY ? 5u : 6u);
                    }
                    else if (mask_color_r == 100) {
                        GeometryHelper::MoveHexByDirUnsafe(hx_, hy_, 0);
                    }
                    else if (mask_color_r == 150) {
                        GeometryHelper::MoveHexByDirUnsafe(hx_, hy_, GameSettings::HEXAGONAL_GEOMETRY ? 3u : 4u);
                    }
                    else if (mask_color_r == 200) {
                        GeometryHelper::MoveHexByDirUnsafe(hx_, hy_, 2u);
                    }
                }

                if (hx_ >= 0 && hy_ >= 0 && hx_ < _width && hy_ < _height) {
                    hx = static_cast<uint16>(hx_);
                    hy = static_cast<uint16>(hy_);

                    if (hex_ox != nullptr && hex_oy != nullptr) {
                        *hex_ox = iround((xf - x_) * GetSpritesZoom()) - _engine->Settings.MapHexWidth / 2;
                        *hex_oy = iround((yf - y_) * GetSpritesZoom()) - _engine->Settings.MapHexHeight / 2;
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

    for (auto* item : _nonTileItems) {
        if (!item->IsSpriteVisible() || item->IsFinishing()) {
            continue;
        }

        const auto* spr = item->Spr;
        const auto hx = item->GetHexX();
        const auto hy = item->GetHexY();
        const auto& field = _hexField->GetCellForReading(hx, hy);
        const auto l = iround(static_cast<float>(field.ScrX + item->ScrX + spr->OffsX + _engine->Settings.MapHexWidth / 2 + _engine->Settings.ScrOx - spr->Width / 2) / GetSpritesZoom()) - extra_range;
        const auto r = iround(static_cast<float>(field.ScrX + item->ScrX + spr->OffsX + _engine->Settings.MapHexWidth / 2 + _engine->Settings.ScrOx + spr->Width / 2) / GetSpritesZoom()) + extra_range;
        const auto t = iround(static_cast<float>(field.ScrY + item->ScrY + spr->OffsY + _engine->Settings.MapHexHeight / 2 + _engine->Settings.ScrOy - spr->Height) / GetSpritesZoom()) - extra_range;
        const auto b = iround(static_cast<float>(field.ScrY + item->ScrY + spr->OffsY + _engine->Settings.MapHexHeight / 2 + _engine->Settings.ScrOy) / GetSpritesZoom()) + extra_range;

        if (x < l || x > r || y < t || y > b) {
            continue;
        }

        if (item->GetSprite()->CheckHit(x - l + extra_range, y - t + extra_range, check_transparent)) {
            if (is_egg && _engine->SprMngr.CheckEggAppearence(hx, hy, item->GetEggType())) {
                pix_item_egg.emplace_back(item);
            }
            else {
                pix_item.emplace_back(item);
            }
        }
    }

    // Sorters
    struct Sorter
    {
        static auto ByTreeIndex(const ItemHexView* o1, const ItemHexView* o2) -> bool { return o1->GetSprite()->TreeIndex > o2->GetSprite()->TreeIndex; }
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
        if (!cr->IsSpriteVisible() || cr->IsFinishing()) {
            continue;
        }
        if (ignore_dead_and_chosen && (cr->IsDead() || cr->GetIsChosen())) {
            continue;
        }

        const auto rect = cr->GetViewRect();
        const auto l = iround(static_cast<float>(rect.Left + _engine->Settings.ScrOx) / GetSpritesZoom()) - extra_range;
        const auto r = iround(static_cast<float>(rect.Right + _engine->Settings.ScrOx) / GetSpritesZoom()) + extra_range;
        const auto t = iround(static_cast<float>(rect.Top + _engine->Settings.ScrOy) / GetSpritesZoom()) - extra_range;
        const auto b = iround(static_cast<float>(rect.Bottom + _engine->Settings.ScrOy) / GetSpritesZoom()) + extra_range;

        if (x >= l && x <= r && y >= t && y <= b) {
            if (check_transparent) {
                const auto rect_draw = cr->GetSprite()->GetDrawRect();
                const auto l_draw = iround(static_cast<float>(rect_draw.Left + _engine->Settings.ScrOx) / GetSpritesZoom());
                const auto t_draw = iround(static_cast<float>(rect_draw.Top + _engine->Settings.ScrOy) / GetSpritesZoom());

                if (_engine->SprMngr.SpriteHitTest(cr->Spr, x - l_draw, y - t_draw, true)) {
                    crits.emplace_back(cr);
                }
            }
            else {
                crits.emplace_back(cr);
            }
        }
    }

    if (crits.empty()) {
        return nullptr;
    }

    if (crits.size() > 1) {
        std::sort(crits.begin(), crits.end(), [](auto* cr1, auto* cr2) { return cr1->GetSprite()->TreeIndex > cr2->GetSprite()->TreeIndex; });
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
        if (item->IsTransparent() || item_egg || item->GetSprite()->TreeIndex <= cr->GetSprite()->TreeIndex) {
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

    RUNTIME_ASSERT(!cr || cr->GetMap() == this);

#define GRID_AT(x, y) _findPathGrid[((MAX_FIND_PATH + 1) + (y) - grid_oy) * (MAX_FIND_PATH * 2 + 2) + ((MAX_FIND_PATH + 1) + (x) - grid_ox)]

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
                if (nx < 0 || ny < 0 || nx >= _width || ny >= _height || GRID_AT(nx, ny)) {
                    continue;
                }

                GRID_AT(nx, ny) = -1;

                if (mh == 0) {
                    if (_hexField->GetCellForReading(static_cast<uint16>(nx), static_cast<uint16>(ny)).Flags.MoveBlocked) {
                        continue;
                    }
                }
                else {
                    // Base hex
                    auto nx_ = nx;
                    auto ny_ = ny;
                    for (uint k = 0; k < mh; k++) {
                        GeometryHelper::MoveHexByDirUnsafe(nx_, ny_, static_cast<uint8>(j));
                    }
                    if (nx_ < 0 || ny_ < 0 || nx_ >= _width || ny_ >= _height) {
                        continue;
                    }
                    if (_hexField->GetCellForReading(static_cast<uint16>(nx_), static_cast<uint16>(ny_)).Flags.MoveBlocked) {
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
                        GeometryHelper::MoveHexByDirUnsafe(nx_2, ny_2, static_cast<uint8>(dir_));
                        not_passed = _hexField->GetCellForReading(static_cast<uint16>(nx_2), static_cast<uint16>(ny_2)).Flags.MoveBlocked;
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
                        GeometryHelper::MoveHexByDirUnsafe(nx_2, ny_2, static_cast<uint8>(dir_));
                        not_passed = _hexField->GetCellForReading(static_cast<uint16>(nx_2), static_cast<uint16>(ny_2)).Flags.MoveBlocked;
                    }
                    if (not_passed) {
                        continue;
                    }
                }

                GRID_AT(nx, ny) = numindex;
                coords.emplace_back(nx, ny);

                if (cut >= 0 && GeometryHelper::CheckDist(static_cast<uint16>(nx), static_cast<uint16>(ny), end_x, end_y, cut)) {
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

    float base_angle = GeometryHelper::GetDirAngle(end_x, end_y, start_x, start_y);

    // From end
    while (numindex > 1) {
        numindex--;

        int best_step_dir = -1;
        float best_step_angle_diff = 0.0f;

        const auto check_hex = [&best_step_dir, &best_step_angle_diff, numindex, grid_ox, grid_oy, start_x, start_y, base_angle, this](int dir, int step_hx, int step_hy) {
            if (GRID_AT(step_hx, step_hy) == numindex) {
                const float angle = GeometryHelper::GetDirAngle(step_hx, step_hy, start_x, start_y);
                const float angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);
                if (best_step_dir == -1 || numindex == 0) {
                    best_step_dir = dir;
                    best_step_angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);
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
                LineTracer tracer(trace_hx, trace_hy, trace_tx, trace_ty, _width, _height, 0.0f);
                uint16 next_hx = trace_hx;
                uint16 next_hy = trace_hy;
                vector<uint8> direct_steps;
                bool failed = false;

                while (true) {
                    uint8 dir = tracer.GetNextHex(next_hx, next_hy);
                    direct_steps.emplace_back(dir);

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
                    GeometryHelper::MoveHexByDir(trace_tx, trace_ty, GeometryHelper::ReverseDir(raw_steps[i]), _width, _height);
                    continue;
                }

                for (const auto& ds : direct_steps) {
                    result.Steps.emplace_back(ds);
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
            result.Steps.emplace_back(cur_dir);

            for (size_t j = i + 1; j < raw_steps.size(); j++) {
                if (raw_steps[j] == cur_dir) {
                    result.Steps.emplace_back(cur_dir);
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

    RUNTIME_ASSERT(!cr || cr->GetMap() == this);

    return !!FindPath(cr, start_x, start_y, end_x, end_y, cut);
}

bool MapView::TraceMoveWay(uint16& hx, uint16& hy, int& ox, int& oy, vector<uint8>& steps, int quad_dir) const
{
    STACK_TRACE_ENTRY();

    ox = 0;
    oy = 0;

    const auto try_move = [this, &hx, &hy, &steps](uint8 dir) {
        auto check_hx = hx;
        auto check_hy = hy;

        if (!GeometryHelper::MoveHexByDir(check_hx, check_hy, dir, _width, _height)) {
            return false;
        }

        const auto& field = _hexField->GetCellForReading(check_hx, check_hy);
        if (field.Flags.MoveBlocked) {
            return false;
        }

        hx = check_hx;
        hy = check_hy;
        steps.emplace_back(dir);
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

void MapView::TraceBullet(uint16 hx, uint16 hy, uint16 tx, uint16 ty, uint dist, float angle, vector<CritterHexView*>* critters, CritterFindType find_type, pair<uint16, uint16>* pre_block, pair<uint16, uint16>* block, vector<pair<uint16, uint16>>* steps, bool check_shoot_blocks)
{
    STACK_TRACE_ENTRY();

    if (_isShowTrack) {
        ClearHexTrack();
    }

    const auto check_dist = dist != 0 ? dist : GeometryHelper::DistGame(hx, hy, tx, ty);
    auto cur_hx = hx;
    auto cur_hy = hy;
    auto prev_hx = cur_hx;
    auto prev_hy = cur_hy;

    LineTracer line_tracer(hx, hy, tx, ty, _width, _height, angle);

    for (uint i = 0; i < check_dist; i++) {
        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            line_tracer.GetNextHex(cur_hx, cur_hy);
        }
        else {
            line_tracer.GetNextSquare(cur_hx, cur_hy);
        }

        if (_isShowTrack) {
            GetHexTrack(cur_hx, cur_hy) = static_cast<char>(cur_hx == tx && cur_hy == ty ? 1 : 2);
        }

        if (check_shoot_blocks && _hexField->GetCellForReading(cur_hx, cur_hy).Flags.ShootBlocked) {
            break;
        }

        if (steps != nullptr) {
            steps->emplace_back(cur_hx, cur_hy);
        }

        if (critters != nullptr) {
            const auto hex_critters = GetCritters(cur_hx, cur_hy, find_type);

            for (auto* cr : hex_critters) {
                if (std::find(critters->begin(), critters->end(), cr) == critters->end()) {
                    critters->emplace_back(cr);
                }
            }
        }

        prev_hx = cur_hx;
        prev_hy = cur_hy;
    }

    if (pre_block != nullptr) {
        pre_block->first = prev_hx;
        pre_block->second = prev_hy;
    }
    if (block != nullptr) {
        block->first = cur_hx;
        block->second = cur_hy;
    }
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
            if (!GeometryHelper::MoveHexByDir(sx, sy, static_cast<uint8>(dirs[i % dirs_index]), _width, _height)) {
                break;
            }

            if (_isShowTrack) {
                GetHexTrack(sx, sy) = 1;
            }

            if (_hexField->GetCellForReading(sx, sy).Flags.ScrollBlock) {
                break;
            }

            if (_isShowTrack) {
                GetHexTrack(sx, sy) = 2;
            }
        }

        for (; i < steps; i++) {
            GeometryHelper::MoveHexByDir(hx, hy, GeometryHelper::ReverseDir(static_cast<uint8>(dirs[i % dirs_index])), _width, _height);
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

void MapView::SetShootBorders(bool enabled, uint dist)
{
    STACK_TRACE_ENTRY();

    if (_drawShootBorders != enabled) {
        _drawShootBorders = enabled;
        _shootBordersDist = dist;
        RebuildFog();
    }
}

auto MapView::GetGlobalDayTime() const noexcept -> int
{
    NO_STACK_TRACE_ENTRY();

    return _engine->GetHour() * 60 + _engine->GetMinute();
}

auto MapView::GetMapDayTime() const noexcept -> int
{
    NO_STACK_TRACE_ENTRY();

    if (_mapperMode) {
        return GetGlobalDayTime();
    }

    const auto map_time = GetCurDayTime();

    return map_time >= 0 ? map_time : GetGlobalDayTime();
}

auto MapView::GetDrawList() noexcept -> MapSpriteList&
{
    NO_STACK_TRACE_ENTRY();

    return _mapSprites;
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

void MapView::AddFastPid(hstring pid)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    _fastPids.emplace(pid);
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
    _ignorePids.emplace(pid);
}

void MapView::SwitchIgnorePid(hstring pid)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapperMode);

    if (_ignorePids.count(pid) != 0) {
        _ignorePids.erase(pid);
    }
    else {
        _ignorePids.emplace(pid);
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
        const auto adx = std::abs(dx);
        const auto ady = std::abs(dy);

        int hx;
        int hy;
        for (auto j = 1; j <= ady; j++) {
            if (dy >= 0) {
                hx = rect.Left + j / 2 + ((j % 2) != 0 ? 1 : 0);
                hy = rect.Top + (j - (hx - rect.Left - ((rect.Left % 2) != 0 ? 1 : 0)) / 2);
            }
            else {
                hx = rect.Left - j / 2 - ((j % 2) != 0 ? 1 : 0);
                hy = rect.Top - (j - (rect.Left - hx - ((rect.Left % 2) != 0 ? 0 : 1)) / 2);
            }

            for (auto i = 0; i <= adx; i++) {
                if (hx >= 0 && hy >= 0 && hx < _width && hy < _height) {
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
                    if ((i % 2) != 0) {
                        shx++;
                    }
                    else {
                        shy++;
                    }
                }
                else {
                    if ((i % 2) != 0) {
                        shy++;
                    }
                    else {
                        shx++;
                    }
                }
            }
            else {
                if (rw > 0) {
                    if ((i % 2) != 0) {
                        shy--;
                    }
                    else {
                        shx--;
                    }
                }
                else {
                    if ((i % 2) != 0) {
                        shx--;
                    }
                    else {
                        shy--;
                    }
                }
            }

            for (auto j = (i % 2) != 0 ? 1 : 0; j < hw; j += 2) {
                if (hx >= 0 && hy >= 0 && hx < _width && hy < _height) {
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

    for (uint16 hx = 0; hx < _width; hx++) {
        for (uint16 hy = 0; hy < _height; hy++) {
            const auto& field = _hexField->GetCellForReading(hx, hy);
            auto& track = GetHexTrack(hx, hy);

            track = 0;

            if (field.Flags.MoveBlocked) {
                track = 2;
            }
            if (field.Flags.ShootBlocked) {
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

auto MapView::ValidateForSave() const -> vector<string>
{
    STACK_TRACE_ENTRY();

    vector<string> errors;

    unordered_set<const CritterHexView*> cr_reported;

    for (const auto* cr : _critters) {
        for (const auto* cr2 : _critters) {
            if (cr != cr2 && cr->GetHexX() == cr2->GetHexX() && cr->GetHexY() == cr2->GetHexY() && cr_reported.count(cr) == 0 && cr_reported.count(cr2) == 0) {
                errors.emplace_back(strex("Critters have same hex coords at {} {}", cr->GetHexX(), cr->GetHexY()));
                cr_reported.emplace(cr);
                cr_reported.emplace(cr2);
            }
        }
    }

    return errors;
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

    const auto fill_critter = [&fill_section](const CritterView* cr) {
        auto kv = cr->GetProperties().SaveToText(&cr->GetProto()->GetProperties());
        kv["$Id"] = strex("{}", cr->GetId());
        kv["$Proto"] = cr->GetProto()->GetName();
        fill_section("Critter", kv);
    };

    const auto fill_item = [&fill_section](const ItemView* item) {
        auto kv = item->GetProperties().SaveToText(&item->GetProto()->GetProperties());
        kv["$Id"] = strex("{}", item->GetId());
        kv["$Proto"] = item->GetProto()->GetName();
        fill_section("Item", kv);
    };

    // Header
    fill_section("ProtoMap", GetProperties().SaveToText(nullptr));

    // Critters
    for (const auto* cr : _critters) {
        fill_critter(cr);

        for (auto* inv_item : cr->GetConstInvItems()) {
            fill_item(inv_item);
        }
    }

    // Items
    for (const auto* item : _allItems) {
        fill_item(item);

        for (auto* inner_item : item->GetConstInnerItems()) {
            fill_item(inner_item);
        }
    }

    return fomap;
}
