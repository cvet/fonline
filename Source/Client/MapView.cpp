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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE();

static constexpr int MAX_LIGHT_INTEN = 10000;
static constexpr int MAX_LIGHT_HEX = 200;
static constexpr int MAX_LIGHT_ALPHA = 255;

static auto EvaluateItemDrawOrder(const ItemHexView* item) -> DrawOrderType
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    return cr->IsDead() && !cr->GetDeadDrawNoFlatten() ? DrawOrderType::DeadCritter : DrawOrderType::Critter;
}

MapView::MapView(FOClient* engine, ident_t id, const ProtoMap* proto, const Properties* props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    MapProperties(GetInitRef()),
    _mapSprites(engine->SprMngr)
{
    FO_STACK_TRACE_ENTRY();

    _name = strex("{}_{}", proto->GetName(), id);

    SetSpritesZoom(1.0f);

    _rtScreenOx = iround(std::ceil(static_cast<float>(_engine->Settings.MapHexWidth) / MIN_ZOOM));
    _rtScreenOy = iround(std::ceil(static_cast<float>(_engine->Settings.MapHexLineHeight * 2) / MIN_ZOOM));

    _rtLight = _engine->SprMngr.GetRtMngr().CreateRenderTarget(false, RenderTarget::SizeKindType::Map, {_rtScreenOx * 2, _rtScreenOy * 2}, false);
    _rtLight->CustomDrawEffect = _engine->EffectMngr.Effects.FlushLight;

    _rtFog = _engine->SprMngr.GetRtMngr().CreateRenderTarget(false, RenderTarget::SizeKindType::Map, {_rtScreenOx * 2, _rtScreenOy * 2}, false);
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
        FO_RUNTIME_ASSERT(atlas_spr);
        const auto mask_x = iround(static_cast<float>(atlas_spr->Atlas->MainTex->Size.width) * atlas_spr->AtlasRect.Left);
        const auto mask_y = iround(static_cast<float>(atlas_spr->Atlas->MainTex->Size.height) * atlas_spr->AtlasRect.Top);
        _picHexMaskData = atlas_spr->Atlas->MainTex->GetTextureRegion({mask_x, mask_y}, atlas_spr->Size);
    }

    _mapSize = GetSize();

    _hexLight.resize(static_cast<size_t>(_mapSize.GetSquare()) * 3);
    _hexField = SafeAlloc::MakeUnique<StaticTwoDimensionalGrid<Field, mpos, msize>>(_mapSize);

    _lightPoints.resize(1);
    _lightSoftPoints.resize(1);

    ResizeView();

    _eventUnsubscriber += _engine->SprMngr.GetWindow()->OnScreenSizeChanged += [this] { OnScreenSizeChanged(); };
}

MapView::~MapView()
{
    FO_STACK_TRACE_ENTRY();

    if (!_critters.empty() || !_crittersMap.empty() || !_allItems.empty() || !_staticItems.empty() || //
        !_dynamicItems.empty() || !_nonTileItems.empty() || !_processingItems.empty() || !_itemsMap.empty() || !_spritePatterns.empty()) {
        BreakIntoDebugger();
    }

    if (_rtMap != nullptr) {
        _engine->SprMngr.GetRtMngr().DeleteRenderTarget(_rtMap);
    }
    _engine->SprMngr.GetRtMngr().DeleteRenderTarget(_rtLight);
    _engine->SprMngr.GetRtMngr().DeleteRenderTarget(_rtFog);
}

void MapView::OnDestroySelf()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& cr : _critters) {
        cr->DestroySelf();
    }
    for (auto& item : _allItems) {
        item->DestroySelf();
    }

    for (auto& pattern : _spritePatterns) {
        pattern->Sprites.clear();
        pattern->FinishCallback = nullptr;
        pattern->Finished = true;
    }

    _mapSprites.Invalidate();
    _hexField.reset();
    _viewField.clear();
    _fogLookPoints.clear();
    _fogShootPoints.clear();
    _visibleLightSources.clear();
    _lightPoints.clear();
    _lightSoftPoints.clear();
    _lightSources.clear();
    _critters.clear();
    _crittersMap.clear();
    _allItems.clear();
    _staticItems.clear();
    _dynamicItems.clear();
    _nonTileItems.clear();
    _processingItems.clear();
    _itemsMap.clear();
}

void MapView::EnableMapperMode()
{
    FO_STACK_TRACE_ENTRY();

    _mapperMode = true;
    _isShowTrack = true;

    _hexTrack.resize(_mapSize.GetSquare());
}

void MapView::LoadFromFile(string_view map_name, const string& str)
{
    FO_RUNTIME_ASSERT(_mapperMode);

    _mapLoading = true;
    auto max_id = GetWorkEntityId().underlying_value();

    MapLoader::Load(
        map_name, str, _engine->ProtoMngr, _engine->Hashes,
        [this, &max_id](ident_t id, const ProtoCritter* proto, const map<string, string>& kv) {
            FO_RUNTIME_ASSERT(id);
            FO_RUNTIME_ASSERT(_crittersMap.count(id) == 0);

            max_id = std::max(max_id, id.underlying_value());

            auto props = proto->GetProperties().Copy();
            props.ApplyFromText(kv);

            auto cr = SafeAlloc::MakeRefCounted<CritterHexView>(this, id, proto, &props);

            if (const auto hex = cr->GetHex(); !_mapSize.IsValidPos(hex)) {
                cr->SetHex(_mapSize.ClampPos(hex));
            }

            AddCritterInternal(cr.get());
        },
        [this, &max_id](ident_t id, const ProtoItem* proto, const map<string, string>& kv) {
            FO_RUNTIME_ASSERT(id);
            FO_RUNTIME_ASSERT(_itemsMap.count(id) == 0);

            max_id = std::max(max_id, id.underlying_value());

            auto props = proto->GetProperties().Copy();
            props.ApplyFromText(kv);

            auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, id, proto, &props);

            if (item->GetOwnership() == ItemOwnership::MapHex) {
                if (const auto hex = item->GetHex(); !_mapSize.IsValidPos(hex)) {
                    item->SetHex(_mapSize.ClampPos(hex));
                }

                AddItemInternal(item.get());
            }
            else if (item->GetOwnership() == ItemOwnership::CritterInventory) {
                auto* cr = GetCritter(item->GetCritterId());

                if (cr == nullptr) {
                    throw GenericException("Critter {} not found", item->GetCritterId());
                }

                cr->AddRawInvItem(item.get());
            }
            else if (item->GetOwnership() == ItemOwnership::ItemContainer) {
                auto* cont = GetItem(item->GetContainerId());

                if (cont == nullptr) {
                    throw GenericException("Container {} not found", item->GetContainerId());
                }

                cont->AddRawInnerItem(item.get());
            }
            else {
                FO_UNREACHABLE_PLACE();
            }
        });

    SetWorkEntityId(ident_t {max_id});
    _mapLoading = false;

    ResizeView();
    RefreshMap();
}

void MapView::LoadStaticData()
{
    FO_STACK_TRACE_ENTRY();

    const auto file = _engine->Resources.ReadFile(strex("{}.fomapb-client", GetProtoId()));

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
            const auto hstr = _engine->Hashes.ToHashedString(str);
            ignore_unused(hstr);
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
            const auto static_id = ident_t {reader.Read<ident_t::underlying_type>()};
            const auto item_pid_hash = reader.Read<hstring::hash_t>();
            const auto item_pid = _engine->Hashes.ResolveHash(item_pid_hash);
            const auto* item_proto = _engine->ProtoMngr.GetProtoItem(item_pid);

            auto item_props = Properties(item_proto->GetProperties().GetRegistrator());
            const auto props_data_size = reader.Read<uint>();
            props_data.resize(props_data_size);
            reader.ReadPtr<uint8>(props_data.data(), props_data_size);
            item_props.RestoreAllData(props_data);

            auto static_item = SafeAlloc::MakeRefCounted<ItemHexView>(this, static_id, item_proto, &item_props);

            static_item->SetStatic(true);

            AddItemInternal(static_item.get());
        }
    }

    reader.VerifyEnd();

    // Index roof
    auto roof_num = 1;

    for (const auto hx : xrange(_mapSize.width)) {
        for (const auto hy : xrange(_mapSize.height)) {
            if (!_hexField->GetCellForReading({hx, hy}).RoofTiles.empty()) {
                MarkRoofNum(ipos {hx, hy}, static_cast<int16>(roof_num));
                roof_num++;
            }
        }
    }

    // Scroll blocks borders
    for (const auto hx : xrange(_mapSize.width)) {
        for (const auto hy : xrange(_mapSize.height)) {
            const auto& field = _hexField->GetCellForReading({hx, hy});

            if (field.Flags.ScrollBlock) {
                for (const auto dir : xrange(GameSettings::MAP_DIR_COUNT)) {
                    auto pos_around = mpos {hx, hy};
                    GeometryHelper::MoveHexByDir(pos_around, static_cast<uint8>(dir), _mapSize);
                    _hexField->GetCellForWriting(pos_around).Flags.MoveBlocked = true;
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
    FO_STACK_TRACE_ENTRY();

    // Map time and color
    {
        const auto global_day_time = _engine->GetGlobalDayTime();
        const auto fixed_map_day_time = GetFixedDayTime();
        const auto map_day_time = fixed_map_day_time != 0 ? fixed_map_day_time : global_day_time;

        if (map_day_time != _prevMapDayTime || global_day_time != _prevGlobalDayTime) {
            _prevMapDayTime = map_day_time;
            _prevGlobalDayTime = global_day_time;

            // Default values:
            // Morning	 5.00 -  9.59	 300 - 599
            // Day		10.00 - 18.59	 600 - 1139
            // Evening	19.00 - 22.59	1140 - 1379
            // Nigh		23.00 -  4.59	1380
            const auto day_color_time = IsNonEmptyDayColorTime() ? GetDayColorTime() : vector<int> {300, 600, 1140, 1380};
            const auto day_color = IsNonEmptyDayColor() ? GetDayColor() : vector<uint8> {18, 128, 103, 51, 18, 128, 95, 40, 53, 128, 86, 29};

            _mapDayColor = GenericUtils::GetColorDay(day_color_time, day_color, map_day_time, &_mapDayLightCapacity);
            _globalDayColor = GenericUtils::GetColorDay(day_color_time, day_color, global_day_time, &_globalDayLightCapacity);

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

        for (auto& cr : _critters) {
            cr->Process();

            if (cr->IsNeedReset()) {
                RemoveCritterFromField(cr.get());
                AddCritterToField(cr.get());
                cr->ResetOk();
            }

            if (cr->IsFinishing() && cr->IsFinished()) {
                critter_to_delete.emplace_back(cr.get());
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

auto MapView::GetViewSize() const -> isize
{
    FO_STACK_TRACE_ENTRY();

    const auto& settings = _engine->Settings;
    const auto width = iround(static_cast<float>(settings.ScreenWidth / settings.MapHexWidth + ((settings.ScreenWidth % settings.MapHexWidth) != 0 ? 1 : 0)) * GetSpritesZoom());
    const auto height = iround(static_cast<float>((settings.ScreenHeight - settings.ScreenHudHeight) / settings.MapHexLineHeight + (((settings.ScreenHeight - settings.ScreenHudHeight) % settings.MapHexLineHeight) != 0 ? 1 : 0)) * GetSpritesZoom());

    return {width, height};
}

void MapView::AddItemToField(ItemHexView* item)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = item->GetHex();
    auto& field = _hexField->GetCellForWriting(hex);

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

        std::ranges::sort(field.Items, [](const auto* i1, const auto* i2) { return i1->GetIsScenery() && !i2->GetIsScenery(); });
        std::ranges::sort(field.Items, [](const auto* i1, const auto* i2) { return i1->GetIsWall() && !i2->GetIsWall(); });

        RecacheHexFlags(field);

        if (item->IsNonEmptyBlockLines()) {
            GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hex, _mapSize, [this, item](mpos block_hex) {
                auto& block_field = _hexField->GetCellForWriting(block_hex);
                block_field.BlockLineItems.emplace_back(item);
                RecacheHexFlags(block_field);
            });
        }
    }

    if (!item->GetLightThru()) {
        UpdateHexLightSources(hex);
    }

    UpdateItemLightSource(item);
}

void MapView::RemoveItemFromField(ItemHexView* item)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = item->GetHex();
    auto& field = _hexField->GetCellForWriting(hex);

    if (item->GetIsTile()) {
        if (item->GetIsRoofTile()) {
            const auto it = std::ranges::find(field.RoofTiles, item);
            FO_RUNTIME_ASSERT(it != field.RoofTiles.end());
            field.RoofTiles.erase(it);
        }
        else {
            const auto it = std::ranges::find(field.GroundTiles, item);
            FO_RUNTIME_ASSERT(it != field.GroundTiles.end());
            field.GroundTiles.erase(it);
        }
    }
    else {
        const auto it = std::ranges::find(field.Items, item);
        FO_RUNTIME_ASSERT(it != field.Items.end());
        field.Items.erase(it);

        RecacheHexFlags(field);

        if (item->IsNonEmptyBlockLines()) {
            GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hex, _mapSize, [this, item](mpos block_hex) {
                auto& block_field = _hexField->GetCellForWriting(block_hex);
                const auto it_block = std::ranges::find(block_field.BlockLineItems, item);
                FO_RUNTIME_ASSERT(it_block != block_field.BlockLineItems.end());
                block_field.BlockLineItems.erase(it_block);
                RecacheHexFlags(block_field);
            });
        }
    }

    if (!item->GetLightThru()) {
        UpdateHexLightSources(hex);
    }

    FinishLightSource(item->GetId());
}

auto MapView::AddReceivedItem(ident_t id, hstring pid, mpos hex, const vector<vector<uint8>>& data) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(id);
    FO_RUNTIME_ASSERT(_mapSize.IsValidPos(hex));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, id, proto);

    item->RestoreData(data);
    item->SetHex(hex);

    if (!item->GetShootThru()) {
        RebuildFog();
    }

    return AddItemInternal(item.get());
}

auto MapView::AddMapperItem(hstring pid, mpos hex, const Properties* props) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);
    FO_RUNTIME_ASSERT(_mapSize.IsValidPos(hex));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, GenTempEntityId(), proto, props);

    item->SetHex(hex);

    return AddItemInternal(item.get());
}

auto MapView::AddMapperTile(hstring pid, mpos hex, uint8 layer, bool is_roof) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);
    FO_RUNTIME_ASSERT(_mapSize.IsValidPos(hex));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, GenTempEntityId(), proto);

    item->SetHex(hex);
    item->SetIsTile(true);
    item->SetIsRoofTile(is_roof);
    item->SetTileLayer(layer);

    return AddItemInternal(item.get());
}

auto MapView::AddItemInternal(ItemHexView* item) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = item->GetHex();

    FO_RUNTIME_ASSERT(_mapSize.IsValidPos(hex));
    FO_RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::MapHex);

    if (item->GetId()) {
        if (auto* prev_item = GetItem(item->GetId()); prev_item != nullptr) {
            item->RestoreFading(prev_item->StoreFading());
            DestroyItem(prev_item);
        }
    }

    item->SetMapId(GetId());
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

    if (!MeasureMapBorders(item->Spr, item->SprOffset)) {
        if (!_mapLoading && IsHexToDraw(hex) && (_mapperMode || !item->GetAlwaysHideSprite())) {
            auto& field = _hexField->GetCellForWriting(hex);
            const auto hex_y_with_offset = static_cast<uint16>(std::clamp(static_cast<int>(hex.y) + item->GetDrawOrderOffsetHexY(), 0, _mapSize.height - 1));
            auto* spr = item->InsertSprite(_mapSprites, EvaluateItemDrawOrder(item), mpos {hex.x, hex_y_with_offset}, &field.Offset);

            AddSpriteToChain(field, spr);
        }
    }

    return item;
}

void MapView::MoveItem(ItemHexView* item, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item->GetMap() == this);
    FO_RUNTIME_ASSERT(_mapSize.IsValidPos(hex));

    RemoveItemFromField(item);
    item->SetHex(hex);
    AddItemToField(item);

    if (item->IsSpriteValid()) {
        item->InvalidateSprite();
    }

    if (IsHexToDraw(hex) && (_mapperMode || !item->GetAlwaysHideSprite())) {
        auto& field = _hexField->GetCellForWriting(hex);
        const auto hex_y_with_offset = static_cast<uint16>(std::clamp(static_cast<int>(hex.y) + item->GetDrawOrderOffsetHexY(), 0, _mapSize.height - 1));
        auto* spr = item->InsertSprite(_mapSprites, EvaluateItemDrawOrder(item), {hex.x, hex_y_with_offset}, &field.Offset);

        AddSpriteToChain(field, spr);
    }
}

void MapView::DestroyItem(ItemHexView* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item->GetMap() == this);

    if (item->IsSpriteValid()) {
        item->InvalidateSprite();
    }

    {
        const auto it = std::ranges::find(_allItems, item);
        FO_RUNTIME_ASSERT(it != _allItems.end());
        _allItems.erase(it);
    }

    if (item->GetStatic()) {
        const auto it = std::ranges::find(_staticItems, item);
        FO_RUNTIME_ASSERT(it != _staticItems.end());
        _staticItems.erase(it);
    }
    else {
        const auto it = std::ranges::find(_dynamicItems, item);
        FO_RUNTIME_ASSERT(it != _dynamicItems.end());
        _dynamicItems.erase(it);
    }

    if (!item->GetIsTile()) {
        const auto it = std::ranges::find(_nonTileItems, item);
        FO_RUNTIME_ASSERT(it != _nonTileItems.end());
        _nonTileItems.erase(it);
    }

    if (const auto it = std::ranges::find(_processingItems, item); it != _processingItems.end()) {
        _processingItems.erase(it);
    }

    if (item->GetId()) {
        const auto it = _itemsMap.find(item->GetId());
        FO_RUNTIME_ASSERT(it != _itemsMap.end());
        _itemsMap.erase(it);
    }

    RemoveItemFromField(item);
    CleanLightSourceOffsets(item->GetId());

    item->DestroySelf();
}

auto MapView::GetItem(mpos hex, hstring pid) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    if (!_mapSize.IsValidPos(hex) || _hexField->GetCellForReading(hex).Items.empty()) {
        return nullptr;
    }

    for (auto* item : _hexField->GetCellForReading(hex).Items) {
        if (item->GetProtoId() == pid) {
            return item;
        }
    }

    return nullptr;
}

auto MapView::GetItem(mpos hex, ident_t id) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    if (!_mapSize.IsValidPos(hex) || _hexField->GetCellForReading(hex).Items.empty()) {
        return nullptr;
    }

    for (auto* item : _hexField->GetCellForReading(hex).Items) {
        if (item->GetId() == id) {
            return item;
        }
    }

    return nullptr;
}

auto MapView::GetItem(ident_t id) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = _itemsMap.find(id); it != _itemsMap.end()) {
        return it->second;
    }

    return nullptr;
}

auto MapView::GetItems(mpos hex) -> const vector<ItemHexView*>&
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    return field.Items;
}

auto MapView::GetTile(mpos hex, bool is_roof, int layer) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);
    const auto& field_tiles = is_roof ? field.RoofTiles : field.GroundTiles;

    for (auto* tile : field_tiles) {
        if (layer < 0 || tile->GetTileLayer() == layer) {
            return tile;
        }
    }

    return nullptr;
}

auto MapView::GetTiles(mpos hex, bool is_roof) -> const vector<ItemHexView*>&
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);
    const auto& field_tiles = is_roof ? field.RoofTiles : field.GroundTiles;

    return field_tiles;
}

auto MapView::GetHexContentSize(mpos hex) -> isize
{
    FO_STACK_TRACE_ENTRY();

    auto result = IRect();

    if (const auto& field = _hexField->GetCellForReading(hex); field.IsView) {
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
                        const auto l = field.Offset.x + _engine->Settings.MapHexWidth / 2 - spr->Offset.x;
                        const auto t = field.Offset.y + _engine->Settings.MapHexHeight / 2 - spr->Offset.y;
                        const auto r = l + spr->Size.width;
                        const auto b = t + spr->Size.height;

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

    return isize {result.Width(), result.Height()};
}

void MapView::RunEffectItem(hstring eff_pid, mpos from_hex, mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapSize.IsValidPos(from_hex));
    FO_RUNTIME_ASSERT(_mapSize.IsValidPos(to_hex));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(eff_pid);
    auto effect_item = SafeAlloc::MakeRefCounted<ItemHexView>(this, ident_t {}, proto);

    effect_item->SetHex(from_hex);
    effect_item->SetEffect(to_hex);

    AddItemInternal(effect_item.get());
}

auto MapView::RunSpritePattern(string_view name, uint count) -> SpritePattern*
{
    FO_STACK_TRACE_ENTRY();

    auto spr = _engine->SprMngr.LoadSprite(name, AtlasType::MapSprites);

    if (!spr) {
        return nullptr;
    }

    spr->Prewarm();
    spr->PlayDefault();

    auto pattern = SafeAlloc::MakeRefCounted<SpritePattern>();

    pattern->SprName = name;
    pattern->SprCount = count;
    pattern->Sprites.emplace_back(std::move(spr));

    for (uint i = 1; i < count; i++) {
        auto next_spr = _engine->SprMngr.LoadSprite(name, AtlasType::MapSprites);

        next_spr->Prewarm();
        next_spr->PlayDefault();

        pattern->Sprites.emplace_back(std::move(next_spr));
    }

    pattern->FinishCallback = [this, ppattern = pattern.get()]() {
        const auto it = std::ranges::find_if(_spritePatterns, [ppattern](auto&& p) { return p.get() == ppattern; });
        FO_RUNTIME_ASSERT(it != _spritePatterns.end());
        it->get()->Sprites.clear();
        _spritePatterns.erase(it);
        ppattern->FinishCallback = nullptr;
        ppattern->Finished = true;
    };

    _spritePatterns.emplace_back(std::move(pattern));
    return _spritePatterns.back().get();
}

void MapView::SetCursorPos(CritterHexView* cr, ipos pos, bool show_steps, bool refresh)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!cr || cr->GetMap() == this);

    mpos hex;

    if (GetHexAtScreenPos(pos, hex, nullptr)) {
        const auto& field = _hexField->GetCellForReading(hex);

        _cursorX = field.Offset.x + 1 - 1;
        _cursorY = field.Offset.y - 1 - 1;

        if (cr == nullptr) {
            _drawCursorX = -1;
            return;
        }

        const auto cr_hex = cr->GetHex();
        const auto multihex = cr->GetMultihex();

        if (cr_hex == hex || (field.Flags.MoveBlocked && (multihex == 0 || !GeometryHelper::CheckDist(cr_hex, hex, multihex)))) {
            _drawCursorX = -1;
        }
        else {
            if (refresh || hex != _lastCurPos) {
                if (cr->IsAlive()) {
                    const auto find_path = FindPath(cr, cr_hex, hex, -1);
                    if (!find_path) {
                        _drawCursorX = -1;
                    }
                    else {
                        _drawCursorX = static_cast<int>(show_steps ? find_path->DirSteps.size() : 0);
                    }
                }
                else {
                    _drawCursorX = -1;
                }

                _lastCurPos = hex;
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
    FO_STACK_TRACE_ENTRY();

    if (_engine->Settings.HideCursor || !_engine->Settings.ShowMoveCursor) {
        return;
    }

    const auto spr_zoom = GetSpritesZoom();
    const int x = iround(static_cast<float>(_cursorX + _engine->Settings.ScreenOffset.x) / spr_zoom);
    const int y = iround(static_cast<float>(_cursorY + _engine->Settings.ScreenOffset.y) / spr_zoom);
    const int w = iround(static_cast<float>(spr->Size.width) / spr_zoom);
    const int h = iround(static_cast<float>(spr->Size.height) / spr_zoom);

    _engine->SprMngr.DrawSpriteSize(spr, {x, y}, {w, h}, true, false, COLOR_SPRITE);
}

void MapView::DrawCursor(string_view text)
{
    FO_STACK_TRACE_ENTRY();

    if (_engine->Settings.HideCursor || !_engine->Settings.ShowMoveCursor) {
        return;
    }

    const auto spr_zoom = GetSpritesZoom();
    const auto x = iround(static_cast<float>(_cursorX + _engine->Settings.ScreenOffset.x) / spr_zoom);
    const auto y = iround(static_cast<float>(_cursorY + _engine->Settings.ScreenOffset.y) / spr_zoom);
    const auto width = iround(static_cast<float>(_engine->Settings.MapHexWidth) / spr_zoom);
    const auto height = iround(static_cast<float>(_engine->Settings.MapHexHeight) / spr_zoom);

    _engine->SprMngr.DrawText({x, y, width, height}, text, FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, -1);
}

void MapView::RebuildMap(ipos screen_raw_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_viewField.empty());

    for (int i = 0, j = _hVisible * _wVisible; i < j; i++) {
        const auto raw_hex = _viewField[i].RawHex;

        if (!_mapSize.IsValidPos(raw_hex)) {
            continue;
        }

        const auto hex = _mapSize.FromRawPos(raw_hex);
        auto& field = _hexField->GetCellForWriting(hex);

        field.IsView = false;
        InvalidateSpriteChain(field);
    }

    InitView(screen_raw_hex);

    // Invalidation
    _mapSprites.Invalidate();
    _engine->SprMngr.EggNotValid();
    _visibleLightSources.clear();
    _needReapplyLights = true;

    // Begin generate new sprites
    for (const auto i : xrange(_hVisible * _wVisible)) {
        const auto& vf = _viewField[i];

        if (!_mapSize.IsValidPos(vf.RawHex)) {
            continue;
        }

        const auto hex = _mapSize.FromRawPos(vf.RawHex);
        auto& field = _hexField->GetCellForWriting(hex);
        FO_RUNTIME_ASSERT(!field.IsView);

        field.IsView = true;
        field.Offset = vf.Offset;

        // Lighting
        if (!field.LightSources.empty()) {
            for (auto&& [ls, color] : field.LightSources) {
                _visibleLightSources[ls]++;
            }
        }

        // Track
        if (_isShowTrack && GetHexTrack(hex) != 0) {
            const auto& spr = GetHexTrack(hex) == 1 ? _picTrack1 : _picTrack2;
            const auto hex_offset = ipos {_engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2 + (spr ? spr->Size.height / 2 : 0)};
            auto& mspr = _mapSprites.AddSprite(DrawOrderType::Track, hex, hex_offset, &field.Offset, //
                spr.get(), nullptr, nullptr, nullptr, nullptr, nullptr);

            AddSpriteToChain(field, &mspr);
        }

        // Hex lines
        if (_isShowHex) {
            const auto& spr = _picHex[0];
            const auto hex_offset = ipos {spr ? spr->Size.width / 2 : 0, spr ? spr->Size.height : 0};
            auto& mspr = _mapSprites.AddSprite(DrawOrderType::HexGrid, hex, hex_offset, &field.Offset, //
                spr.get(), nullptr, nullptr, nullptr, nullptr, nullptr);

            AddSpriteToChain(field, &mspr);
        }

        // Tiles
        if (!field.GroundTiles.empty() && _engine->Settings.ShowTile) {
            for (auto* tile : field.GroundTiles) {
                if (!_mapperMode && tile->GetAlwaysHideSprite()) {
                    continue;
                }

                const auto hex_y_with_offset = static_cast<uint16>(std::clamp(static_cast<int>(hex.y) + tile->GetDrawOrderOffsetHexY(), 0, _mapSize.height - 1));
                auto* mspr = tile->AddSprite(_mapSprites, EvaluateItemDrawOrder(tile), {hex.x, hex_y_with_offset}, &field.Offset);

                AddSpriteToChain(field, mspr);
            }
        }

        // Roof
        if (!field.RoofTiles.empty() && (_roofSkip == 0 || _roofSkip != field.RoofNum) && _engine->Settings.ShowRoof) {
            for (auto* tile : field.RoofTiles) {
                if (!_mapperMode && tile->GetAlwaysHideSprite()) {
                    continue;
                }

                const auto hex_y_with_offset = static_cast<uint16>(std::clamp(static_cast<int>(hex.y) + tile->GetDrawOrderOffsetHexY(), 0, _mapSize.height - 1));
                auto* mspr = tile->AddSprite(_mapSprites, EvaluateItemDrawOrder(tile), {hex.x, hex_y_with_offset}, &field.Offset);

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

                const auto hex_y_with_offset = static_cast<uint16>(std::clamp(static_cast<int>(hex.y) + item->GetDrawOrderOffsetHexY(), 0, _mapSize.height - 1));
                auto* mspr = item->AddSprite(_mapSprites, EvaluateItemDrawOrder(item), {hex.x, hex_y_with_offset}, &field.Offset);

                AddSpriteToChain(field, mspr);
            }
        }

        // Critters
        if (!field.Critters.empty() && _engine->Settings.ShowCrit) {
            for (auto* cr : field.Critters) {
                auto* mspr = cr->AddSprite(_mapSprites, EvaluateCritterDrawOrder(cr), hex, &field.Offset);

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
            for (const auto& pattern : _spritePatterns) {
                if ((hex.x % pattern->EveryHex.x) != 0) {
                    continue;
                }
                if ((hex.y % pattern->EveryHex.y) != 0) {
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

                const auto* spr = pattern->Sprites[(hex.y * (pattern->Sprites.size() / 5) + hex.x) % pattern->Sprites.size()].get();
                auto& mspr = _mapSprites.AddSprite(pattern->InteractWithRoof && field.RoofNum != 0 ? DrawOrderType::RoofParticles : DrawOrderType::Particles, hex, //
                    {_engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2 + (pattern->InteractWithRoof && field.RoofNum != 0 ? _engine->Settings.MapRoofOffsY : 0)}, &field.Offset, //
                    spr, nullptr, nullptr, nullptr, nullptr, nullptr);

                AddSpriteToChain(field, &mspr);
            }
        }
    }

    _mapSprites.Sort();

    _screenRawHex = screen_raw_hex;

    _needRebuildLightPrimitives = true;

    _engine->OnRenderMap.Fire();
}

void MapView::RebuildMapOffset(ipos hex_offset)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_viewField.empty());
    FO_RUNTIME_ASSERT(hex_offset.x == 0 || hex_offset.x == -1 || hex_offset.x == 1);
    FO_RUNTIME_ASSERT(hex_offset.y == 0 || hex_offset.y == -2 || hex_offset.y == 2);

    const auto ox = hex_offset.x;
    const auto oy = hex_offset.y;

    auto hide_hex = [this](const ViewField& vf) {
        if (!_mapSize.IsValidPos(vf.RawHex)) {
            return;
        }

        const auto hex = _mapSize.FromRawPos(vf.RawHex);

        if (!IsHexToDraw(hex)) {
            return;
        }

        auto& field = _hexField->GetCellForWriting(hex);
        field.IsView = false;

        InvalidateSpriteChain(field);

        // Lighting
        if (!field.LightSources.empty()) {
            for (auto&& [ls, color] : field.LightSources) {
                const auto it = _visibleLightSources.find(ls);

                FO_RUNTIME_ASSERT(it != _visibleLightSources.end());
                FO_RUNTIME_ASSERT(it->second > 0);

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

    _screenRawHex.x += _viewField[vpos2].RawHex.x - _viewField[vpos1].RawHex.x;
    _screenRawHex.y += _viewField[vpos2].RawHex.y - _viewField[vpos1].RawHex.y;

    for (const auto i : xrange(_wVisible * _hVisible)) {
        auto& vf = _viewField[i];

        if (ox < 0) {
            vf.RawHex.x--;
            if ((vf.RawHex.x % 2) != 0) {
                vf.RawHex.y++;
            }
        }
        else if (ox > 0) {
            vf.RawHex.x++;
            if ((vf.RawHex.x % 2) == 0) {
                vf.RawHex.y--;
            }
        }

        if (oy < 0) {
            vf.RawHex.x--;
            vf.RawHex.y--;
            if ((vf.RawHex.x % 2) == 0) {
                vf.RawHex.y--;
            }
        }
        else if (oy > 0) {
            vf.RawHex.x++;
            vf.RawHex.y++;
            if ((vf.RawHex.x % 2) != 0) {
                vf.RawHex.y++;
            }
        }

        if (_mapSize.IsValidPos(vf.RawHex)) {
            const auto hex = _mapSize.FromRawPos(vf.RawHex);
            auto& field = _hexField->GetCellForWriting(hex);
            field.Offset = vf.Offset;
        }
    }

    auto show_hex = [this](const ViewField& vf) {
        if (!_mapSize.IsValidPos(vf.RawHex)) {
            return;
        }

        const auto hex = _mapSize.FromRawPos(vf.RawHex);

        if (IsHexToDraw(hex)) {
            return;
        }

        auto& field = _hexField->GetCellForWriting(hex);

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
        if (_isShowTrack && GetHexTrack(hex) != 0) {
            const auto& spr = GetHexTrack(hex) == 1 ? _picTrack1 : _picTrack2;
            auto& mspr = _mapSprites.InsertSprite(DrawOrderType::Track, hex, //
                {_engine->Settings.MapHexWidth / 2, (_engine->Settings.MapHexHeight / 2) + (spr ? spr->Size.height / 2 : 0)}, &field.Offset, //
                spr.get(), nullptr, nullptr, nullptr, nullptr, nullptr);

            AddSpriteToChain(field, &mspr);
        }

        // Hex lines
        if (_isShowHex) {
            const auto& spr = _picHex[0];
            auto& mspr = _mapSprites.InsertSprite(DrawOrderType::HexGrid, hex, //
                {spr ? spr->Size.width / 2 : 0, spr ? spr->Size.height : 0}, &field.Offset, //
                spr.get(), nullptr, nullptr, nullptr, nullptr, nullptr);

            AddSpriteToChain(field, &mspr);
        }

        // Tiles
        if (!field.GroundTiles.empty() && _engine->Settings.ShowTile) {
            for (auto* tile : field.GroundTiles) {
                if (!_mapperMode && tile->GetAlwaysHideSprite()) {
                    continue;
                }

                const auto hex_y_with_offset = static_cast<uint16>(std::clamp(static_cast<int>(hex.y) + tile->GetDrawOrderOffsetHexY(), 0, _mapSize.height - 1));
                auto* mspr = tile->InsertSprite(_mapSprites, EvaluateItemDrawOrder(tile), {hex.x, hex_y_with_offset}, &field.Offset);

                AddSpriteToChain(field, mspr);
            }
        }

        // Roof
        if (!field.RoofTiles.empty() && _engine->Settings.ShowRoof && (_roofSkip == 0 || _roofSkip != field.RoofNum)) {
            for (auto* tile : field.RoofTiles) {
                if (!_mapperMode && tile->GetAlwaysHideSprite()) {
                    continue;
                }

                const auto hex_y_with_offset = static_cast<uint16>(std::clamp(static_cast<int>(hex.y) + tile->GetDrawOrderOffsetHexY(), 0, _mapSize.height - 1));
                auto* mspr = tile->InsertSprite(_mapSprites, EvaluateItemDrawOrder(tile), {hex.x, hex_y_with_offset}, &field.Offset);

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

                const auto hex_y_with_offset = static_cast<uint16>(std::clamp(static_cast<int>(hex.y) + item->GetDrawOrderOffsetHexY(), 0, _mapSize.height - 1));
                auto* mspr = item->InsertSprite(_mapSprites, EvaluateItemDrawOrder(item), {hex.x, hex_y_with_offset}, &field.Offset);

                AddSpriteToChain(field, mspr);
            }
        }

        // Critters
        if (!field.Critters.empty() && _engine->Settings.ShowCrit) {
            for (auto* cr : field.Critters) {
                auto* mspr = cr->InsertSprite(_mapSprites, EvaluateCritterDrawOrder(cr), hex, &field.Offset);

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
            for (const auto& pattern : _spritePatterns) {
                if ((hex.x % pattern->EveryHex.x) != 0) {
                    continue;
                }
                if ((hex.y % pattern->EveryHex.y) != 0) {
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

                const auto* spr = pattern->Sprites[(hex.y * (pattern->Sprites.size() / 5) + hex.x) % pattern->Sprites.size()].get();
                auto& mspr = _mapSprites.InsertSprite(pattern->InteractWithRoof && field.RoofNum != 0 ? DrawOrderType::RoofParticles : DrawOrderType::Particles, hex, //
                    {_engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2 + (pattern->InteractWithRoof && field.RoofNum != 0 ? _engine->Settings.MapRoofOffsY : 0)}, &field.Offset, //
                    spr, nullptr, nullptr, nullptr, nullptr, nullptr);

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
    for (auto& cr : _critters) {
        cr->RefreshOffs();
    }

    _needRebuildLightPrimitives = true;

    _engine->OnRenderMap.Fire();
}

void MapView::ProcessLighting()
{
    FO_STACK_TRACE_ENTRY();

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
            const auto elapsed_time = (_engine->GameTime.GetFrameTime() - ls->Time).div<float>(std::chrono::milliseconds {200});

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
        FO_RUNTIME_ASSERT(_lightSources.count(ls->Id) != 0);

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
            FO_RUNTIME_ASSERT(ls->Applied);

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
            if (ls->Offset != nullptr && *ls->Offset != ls->LastOffset) {
                ls->LastOffset = *ls->Offset;
                need_render_light = true;
            }
        }
    }

    // Prerender light
    if (need_render_light && _rtLight != nullptr) {
        _engine->SprMngr.GetRtMngr().PushRenderTarget(_rtLight);
        _engine->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);

        const auto zoom = GetSpritesZoom();
        const auto offset = fpos {static_cast<float>(_rtScreenOx), static_cast<float>(_rtScreenOy)};

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
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr->GetMap() == this);

    bool light_added = false;

    for (const auto& item : cr->GetInvItems()) {
        if (item->GetLightSource() && item->GetCritterSlot() != CritterItemSlot::Inventory) {
            UpdateLightSource(cr->GetId(), cr->GetHex(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity(), &cr->SprOffset);
            light_added = true;
            break;
        }
    }

    // Default chosen light
    if (!light_added && cr->GetIsChosen()) {
        UpdateLightSource(cr->GetId(), cr->GetHex(), _engine->Settings.ChosenLightColor, _engine->Settings.ChosenLightDistance, _engine->Settings.ChosenLightFlags, _engine->Settings.ChosenLightIntensity, &cr->SprOffset);
        light_added = true;
    }

    if (!light_added) {
        FinishLightSource(cr->GetId());
    }
}

void MapView::UpdateItemLightSource(const ItemHexView* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item->GetMap() == this);

    if (item->GetLightSource()) {
        UpdateLightSource(item->GetId(), item->GetHex(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity(), &item->SprOffset);
    }
    else {
        FinishLightSource(item->GetId());
    }
}

void MapView::UpdateHexLightSources(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    for (auto&& ls_pair : copy(field.LightSources)) {
        ApplyLightFan(ls_pair.first);
    }
}

void MapView::UpdateLightSource(ident_t id, mpos hex, ucolor color, uint distance, uint8 flags, int intensity, const ipos* offset)
{
    FO_STACK_TRACE_ENTRY();

    LightSource* ls;

    const auto it = _lightSources.find(id);

    if (it == _lightSources.end()) {
        ls = _lightSources.emplace(id, SafeAlloc::MakeUnique<LightSource>(LightSource {id, hex, color, distance, flags, intensity, offset})).first->second.get();
    }
    else {
        ls = it->second.get();

        // Ignore redundant updates
        if (!ls->Finishing && ls->Hex == hex && ls->Color == color && ls->Distance == distance && ls->Flags == flags && ls->Intensity == intensity) {
            return;
        }

        CleanLightFan(ls);

        ls->Finishing = false;
        ls->Hex = hex;
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
            ls->Time = _engine->GameTime.GetFrameTime();
        }
    }

    ApplyLightFan(ls);
}

void MapView::FinishLightSource(ident_t id)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _lightSources.find(id);

    if (it != _lightSources.end()) {
        auto& ls = it->second;

        if (!ls->Finishing) {
            ls->Finishing = true;
            ls->StartIntensity = ls->CurIntensity;
            ls->TargetIntensity = 0;
            ls->Time = _engine->GameTime.GetFrameTime();
        }
    }
}

void MapView::CleanLightSourceOffsets(ident_t id)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _lightSources.find(id);

    if (it != _lightSources.end()) {
        auto& ls = it->second;

        ls->Offset = nullptr;
    }
}

void MapView::ApplyLightFan(LightSource* ls)
{
    FO_STACK_TRACE_ENTRY();

    if (ls->Applied) {
        CleanLightFan(ls);
    }

    FO_RUNTIME_ASSERT(!ls->Applied);

    ls->Applied = true;
    ls->NeedReapply = false;

    const auto center_hex = ls->Hex;
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

    MarkLight(ls, center_hex, intensity);

    ipos raw_traced_hex = {center_hex.x, center_hex.y};
    bool seek_start = true;
    mpos last_traced_hex = {static_cast<uint16>(-1), static_cast<uint16>(-1)};

    for (int i = 0, ii = (GameSettings::HEXAGONAL_GEOMETRY ? 6 : 4); i < ii; i++) {
        const auto dir = static_cast<uint8>(GameSettings::HEXAGONAL_GEOMETRY ? (i + 2) % 6 : ((i + 1) * 2) % 8);

        for (int j = 0, jj = static_cast<int>(GameSettings::HEXAGONAL_GEOMETRY ? distance : distance * 2); j < jj; j++) {
            if (seek_start) {
                for (uint l = 0; l < distance; l++) {
                    GeometryHelper::MoveHexByDirUnsafe(raw_traced_hex, GameSettings::HEXAGONAL_GEOMETRY ? 0 : 7);
                }

                seek_start = false;
                j = -1;
            }
            else {
                GeometryHelper::MoveHexByDirUnsafe(raw_traced_hex, dir);
            }

            auto traced_hex = _mapSize.ClampPos(raw_traced_hex);

            if (IsBitSet(ls->Flags, LIGHT_DISABLE_DIR(i))) {
                traced_hex = center_hex;
            }
            else {
                TraceLightLine(ls, center_hex, traced_hex, distance, intensity);
            }

            if (traced_hex != last_traced_hex) {
                uint8 traced_alpha;
                bool use_offsets = false;

                if (ipos {traced_hex.x, traced_hex.y} != raw_traced_hex) {
                    traced_alpha = static_cast<uint8>(lerp(static_cast<int>(center_alpha), 0, static_cast<float>(GeometryHelper::DistGame(center_hex, traced_hex)) / static_cast<float>(distance)));

                    if (traced_hex == center_hex) {
                        use_offsets = true;
                    }
                }
                else {
                    traced_alpha = 0;
                    use_offsets = true;
                }

                ls->FanHexes.emplace_back(traced_hex, traced_alpha, use_offsets);

                last_traced_hex = traced_hex;
            }
        }
    }

    if (!ls->FanHexes.empty()) {
        _needRebuildLightPrimitives = true;
    }
}

void MapView::CleanLightFan(LightSource* ls)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(ls->Applied);

    ls->Applied = false;

    if (IsBitSet(ls->Flags, LIGHT_GLOBAL)) {
        FO_RUNTIME_ASSERT(_globalLights > 0);

        _globalLights--;
    }

    for (const auto& hex : ls->MarkedHexes) {
        auto& field = _hexField->GetCellForWriting(hex);

        field.LightSources.erase(ls);
        CalculateHexLight(hex, field);

        if constexpr (FO_DEBUG) {
            if (field.IsView) {
                FO_RUNTIME_ASSERT(_visibleLightSources[ls] > 0);

                _visibleLightSources[ls]--;
            }
        }
    }

    if (!ls->FanHexes.empty()) {
        _needRebuildLightPrimitives = true;
    }

    if constexpr (FO_DEBUG) {
        FO_RUNTIME_ASSERT(_visibleLightSources.count(ls) == 0 || _visibleLightSources[ls] == 0);
    }

    _visibleLightSources.erase(ls);
}

void MapView::TraceLightLine(LightSource* ls, mpos from_hex, mpos& to_hex, uint distance, uint intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto [base_sx, base_sy] = GenericUtils::GetStepsCoords({from_hex.x, from_hex.y}, {to_hex.x, to_hex.y});
    const auto sx1_f = base_sx;
    const auto sy1_f = base_sy;

    auto curx1_f = static_cast<float>(from_hex.x);
    auto cury1_f = static_cast<float>(from_hex.y);
    auto curx1_i = static_cast<int>(from_hex.x);
    auto cury1_i = static_cast<int>(from_hex.y);

    auto cur_inten = intensity;
    const auto inten_sub = intensity / distance;

    for (;;) {
        cur_inten -= inten_sub;
        curx1_f += sx1_f;
        cury1_f += sy1_f;

        const int old_curx1_i = curx1_i;
        const int old_cury1_i = cury1_i;

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
        int ox = 0;
        int oy = 0;

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

            if (ox < 0 || ox >= _mapSize.width || _hexField->GetCellForReading({static_cast<uint16>(ox), static_cast<uint16>(old_cury1_i)}).Flags.LightBlocked) {
                to_hex.x = static_cast<uint16>(ox < 0 || ox >= _mapSize.width ? old_curx1_i : ox);
                to_hex.y = static_cast<uint16>(old_cury1_i);

                MarkLightEnd(ls, {static_cast<uint16>(old_curx1_i), static_cast<uint16>(old_cury1_i)}, to_hex, cur_inten);
                break;
            }

            MarkLightStep(ls, {static_cast<uint16>(old_curx1_i), static_cast<uint16>(old_cury1_i)}, {static_cast<uint16>(ox), static_cast<uint16>(old_cury1_i)}, cur_inten);

            // Right side
            oy = old_cury1_i + oy;

            if (oy < 0 || oy >= _mapSize.height || _hexField->GetCellForReading({static_cast<uint16>(old_curx1_i), static_cast<uint16>(oy)}).Flags.LightBlocked) {
                to_hex.x = static_cast<uint16>(old_curx1_i);
                to_hex.y = static_cast<uint16>(oy < 0 || oy >= _mapSize.height ? old_cury1_i : oy);

                MarkLightEnd(ls, {static_cast<uint16>(old_curx1_i), static_cast<uint16>(old_cury1_i)}, to_hex, cur_inten);
                break;
            }

            MarkLightStep(ls, {static_cast<uint16>(old_curx1_i), static_cast<uint16>(old_cury1_i)}, {static_cast<uint16>(old_curx1_i), static_cast<uint16>(oy)}, cur_inten);
        }

        // Main trace
        if (curx1_i < 0 || curx1_i >= _mapSize.width || cury1_i < 0 || cury1_i >= _mapSize.height || _hexField->GetCellForReading({static_cast<uint16>(curx1_i), static_cast<uint16>(cury1_i)}).Flags.LightBlocked) {
            to_hex.x = static_cast<uint16>(curx1_i < 0 || curx1_i >= _mapSize.width ? old_curx1_i : curx1_i);
            to_hex.y = static_cast<uint16>(cury1_i < 0 || cury1_i >= _mapSize.height ? old_cury1_i : cury1_i);

            MarkLightEnd(ls, {static_cast<uint16>(old_curx1_i), static_cast<uint16>(old_cury1_i)}, to_hex, cur_inten);
            break;
        }

        MarkLightEnd(ls, {static_cast<uint16>(old_curx1_i), static_cast<uint16>(old_cury1_i)}, {static_cast<uint16>(curx1_i), static_cast<uint16>(cury1_i)}, cur_inten);

        if (curx1_i == to_hex.x && cury1_i == to_hex.y) {
            break;
        }
    }
}

void MapView::MarkLightStep(LightSource* ls, mpos from_hex, mpos to_hex, uint intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(to_hex);

    if (field.Flags.HasTransparentWall) {
        const bool north_south = field.Corner == CornerType::NorthSouth || field.Corner == CornerType::North || field.Corner == CornerType::West;
        const auto dir = GeometryHelper::GetFarDir(from_hex, to_hex);

        if (dir == 0 || (north_south && dir == 1) || (!north_south && (dir == 4 || dir == 5))) {
            MarkLight(ls, to_hex, intensity);
        }
    }
    else {
        MarkLight(ls, to_hex, intensity);
    }
}

void MapView::MarkLightEnd(LightSource* ls, mpos from_hex, mpos to_hex, uint intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    bool is_wall = false;
    bool north_south = false;
    const auto& field = _hexField->GetCellForReading(to_hex);

    if (field.Flags.HasWall) {
        is_wall = true;

        if (field.Corner == CornerType::NorthSouth || field.Corner == CornerType::North || field.Corner == CornerType::West) {
            north_south = true;
        }
    }

    const int dir = GeometryHelper::GetFarDir(from_hex, to_hex);

    if (dir == 0 || (north_south && dir == 1) || (!north_south && (dir == 4 || dir == 5))) {
        MarkLight(ls, to_hex, intensity);

        if (is_wall) {
            if (north_south) {
                if (to_hex.y > 0) {
                    MarkLightEndNeighbor(ls, _mapSize.FromRawPos(ipos {to_hex.x, to_hex.y - 1}), true, intensity);
                }
                if (to_hex.y < _mapSize.height - 1) {
                    MarkLightEndNeighbor(ls, _mapSize.FromRawPos(ipos {to_hex.x, to_hex.y + 1}), true, intensity);
                }
            }
            else {
                if (to_hex.x > 0) {
                    MarkLightEndNeighbor(ls, _mapSize.FromRawPos(ipos {to_hex.x - 1, to_hex.y}), false, intensity);

                    if (to_hex.y > 0) {
                        MarkLightEndNeighbor(ls, _mapSize.FromRawPos(ipos {to_hex.x - 1, to_hex.y - 1}), false, intensity);
                    }
                    if (to_hex.y < _mapSize.height - 1) {
                        MarkLightEndNeighbor(ls, _mapSize.FromRawPos(ipos {to_hex.x - 1, to_hex.y + 1}), false, intensity);
                    }
                }
                if (to_hex.x < _mapSize.width - 1) {
                    MarkLightEndNeighbor(ls, _mapSize.FromRawPos(ipos {to_hex.x + 1, to_hex.y}), false, intensity);

                    if (to_hex.y > 0) {
                        MarkLightEndNeighbor(ls, _mapSize.FromRawPos(ipos {to_hex.x + 1, to_hex.y - 1}), false, intensity);
                    }
                    if (to_hex.y < _mapSize.height - 1) {
                        MarkLightEndNeighbor(ls, _mapSize.FromRawPos(ipos {to_hex.x + 1, to_hex.y + 1}), false, intensity);
                    }
                }
            }
        }
    }
}

void MapView::MarkLightEndNeighbor(LightSource* ls, mpos hex, bool north_south, uint intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Flags.HasWall) {
        const auto corner = field.Corner;

        if ((north_south && (corner == CornerType::NorthSouth || corner == CornerType::North || corner == CornerType::West)) || (!north_south && (corner == CornerType::EastWest || corner == CornerType::East)) || corner == CornerType::South) {
            MarkLight(ls, hex, intensity / 2);
        }
    }
}

void MapView::MarkLight(LightSource* ls, mpos hex, uint intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto light_value = static_cast<int>(intensity) * MAX_LIGHT_HEX / MAX_LIGHT_INTEN * ls->Capacity / 100;
    const auto light_value_r = static_cast<uint8>(light_value * ls->CenterColor.comp.r / 255);
    const auto light_value_g = static_cast<uint8>(light_value * ls->CenterColor.comp.g / 255);
    const auto light_value_b = static_cast<uint8>(light_value * ls->CenterColor.comp.b / 255);
    const auto light_color = ucolor {light_value_r, light_value_g, light_value_b, 0};

    auto& field = _hexField->GetCellForWriting(hex);
    const auto it = field.LightSources.find(ls);

    if (it == field.LightSources.end()) {
        field.LightSources.emplace(ls, light_color);
        ls->MarkedHexes.emplace_back(hex);
        CalculateHexLight(hex, field);

        if (field.IsView) {
            _visibleLightSources[ls]++;
        }
    }
    else {
        auto& cur_color = it->second;

        if (light_color.comp.r > cur_color.comp.r || light_color.comp.g > cur_color.comp.g || light_color.comp.b > cur_color.comp.b) {
            cur_color = light_color;
            CalculateHexLight(hex, field);
        }
    }
}

void MapView::CalculateHexLight(mpos hex, const Field& field)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto& hex_light = _hexLight[hex.y * _mapSize.width + hex.x];

    hex_light = {};

    for (auto&& ls_pair : field.LightSources) {
        hex_light.comp.r = std::max(hex_light.comp.r, ls_pair.second.comp.r);
        hex_light.comp.g = std::max(hex_light.comp.g, ls_pair.second.comp.g);
        hex_light.comp.b = std::max(hex_light.comp.b, ls_pair.second.comp.b);
    }
}

void MapView::LightFanToPrimitves(const LightSource* ls, vector<PrimitivePoint>& points, vector<PrimitivePoint>& soft_points) const
{
    FO_NO_STACK_TRACE_ENTRY();

    if (ls->FanHexes.size() <= 1) {
        return;
    }

    ipos center_pos = GetHexPosition(ls->Hex);
    center_pos.x += _engine->Settings.MapHexWidth / 2;
    center_pos.y += _engine->Settings.MapHexHeight / 2;

    const auto center_point = PrimitivePoint {center_pos, ls->CenterColor, ls->Offset};

    const auto points_start_size = points.size();
    points.reserve(points.size() + ls->FanHexes.size() * 3);
    soft_points.reserve(soft_points.size() + ls->FanHexes.size() * 3);

    for (size_t i = 0; i < ls->FanHexes.size(); i++) {
        const auto& fan_hex = ls->FanHexes[i];
        const mpos hex = std::get<0>(fan_hex);
        const uint8 alpha = std::get<1>(fan_hex);
        const bool use_offsets = std::get<2>(fan_hex);

        const auto [ox, oy] = _engine->Geometry.GetHexInterval(ls->Hex, hex);
        const auto edge_point = PrimitivePoint {{center_pos.x + ox, center_pos.y + oy}, ucolor {ls->CenterColor, alpha}, use_offsets ? ls->Offset : nullptr};

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

    FO_RUNTIME_ASSERT(points.size() % 3 == 0);

    for (size_t i = points_start_size; i < points.size(); i += 3) {
        const auto& cur = points[i];
        const auto& next = points[i + 1];

        if (GenericUtils::DistSqrt(cur.PointPos, next.PointPos) > static_cast<uint>(_engine->Settings.MapHexWidth)) {
            soft_points.emplace_back(PrimitivePoint {next.PointPos, next.PointColor, next.PointOffset, next.PPointColor});
            soft_points.emplace_back(PrimitivePoint {cur.PointPos, cur.PointColor, cur.PointOffset, cur.PPointColor});

            const auto dist_comp = GenericUtils::DistSqrt(center_pos, cur.PointPos) > GenericUtils::DistSqrt(center_pos, next.PointPos);
            const auto x = static_cast<float>(dist_comp ? next.PointPos.x - cur.PointPos.x : cur.PointPos.x - next.PointPos.x);
            const auto y = static_cast<float>(dist_comp ? next.PointPos.y - cur.PointPos.y : cur.PointPos.y - next.PointPos.y);
            const auto changed_xy = GenericUtils::ChangeStepsCoords({x, y}, dist_comp ? -2.5f : 2.5f);

            if (dist_comp) {
                soft_points.emplace_back(PrimitivePoint {{cur.PointPos.x + iround(changed_xy.x), cur.PointPos.y + iround(changed_xy.y)}, cur.PointColor, cur.PointOffset, cur.PPointColor});
            }
            else {
                soft_points.emplace_back(PrimitivePoint {{next.PointPos.x + iround(changed_xy.x), next.PointPos.y + iround(changed_xy.y)}, next.PointColor, next.PointOffset, next.PPointColor});
            }
        }
    }
}

void MapView::SetSkipRoof(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    if (_roofSkip != _hexField->GetCellForReading(hex).RoofNum) {
        _roofSkip = _hexField->GetCellForReading(hex).RoofNum;
        RefreshMap();
    }
}

void MapView::MarkRoofNum(ipos raw_hex, int16 num)
{
    FO_STACK_TRACE_ENTRY();

    if (!_mapSize.IsValidPos(raw_hex)) {
        return;
    }

    const auto hex = _mapSize.FromRawPos(raw_hex);

    if (_hexField->GetCellForReading(hex).RoofTiles.empty()) {
        return;
    }
    if (_hexField->GetCellForReading(hex).RoofNum != 0) {
        return;
    }

    for (auto x = 0; x < _engine->Settings.MapTileStep; x++) {
        for (auto y = 0; y < _engine->Settings.MapTileStep; y++) {
            if (_mapSize.IsValidPos(ipos {hex.x + x, hex.y + y})) {
                _hexField->GetCellForWriting(_mapSize.FromRawPos(ipos {hex.x + x, hex.y + y})).RoofNum = num;
            }
        }
    }

    MarkRoofNum({hex.x + _engine->Settings.MapTileStep, hex.y}, num);
    MarkRoofNum({hex.x - _engine->Settings.MapTileStep, hex.y}, num);
    MarkRoofNum({hex.x, hex.y + _engine->Settings.MapTileStep}, num);
    MarkRoofNum({hex.x, hex.y - _engine->Settings.MapTileStep}, num);
}

auto MapView::IsVisible(const Sprite* spr, ipos offset) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(spr);

    const auto top = offset.y + spr->Offset.x - spr->Size.height - _engine->Settings.MapHexLineHeight * 2;
    const auto bottom = offset.y + spr->Offset.y + _engine->Settings.MapHexLineHeight * 2;
    const auto left = offset.x + spr->Offset.x - spr->Size.width / 2 - _engine->Settings.MapHexWidth;
    const auto right = offset.x + spr->Offset.x + spr->Size.width / 2 + _engine->Settings.MapHexWidth;
    const auto zoomed_screen_height = iround(std::ceil(static_cast<float>(_engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight) * GetSpritesZoom()));
    const auto zoomed_screen_width = iround(std::ceil(static_cast<float>(_engine->Settings.ScreenWidth) * GetSpritesZoom()));

    return top <= zoomed_screen_height && bottom >= 0 && left <= zoomed_screen_width && right >= 0;
}

auto MapView::MeasureMapBorders(const Sprite* spr, ipos offset) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(spr);

    const auto top = std::max(spr->Offset.y + offset.y - _hTop * _engine->Settings.MapHexLineHeight + _engine->Settings.MapHexLineHeight * 2, 0);
    const auto bottom = std::max(spr->Size.height - spr->Offset.y - offset.y - _hBottom * _engine->Settings.MapHexLineHeight + _engine->Settings.MapHexLineHeight * 2, 0);
    const auto left = std::max(spr->Size.width / 2 + spr->Offset.x + offset.x - _wLeft * _engine->Settings.MapHexWidth + _engine->Settings.MapHexWidth, 0);
    const auto right = std::max(spr->Size.width / 2 - spr->Offset.x - offset.x - _wRight * _engine->Settings.MapHexWidth + _engine->Settings.MapHexWidth, 0);

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
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item->GetMap() == this);

    return MeasureMapBorders(item->Spr, item->SprOffset);
}

void MapView::RecacheHexFlags(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapSize.IsValidPos(hex));

    auto& field = _hexField->GetCellForWriting(hex);

    RecacheHexFlags(field);
}

void MapView::RecacheHexFlags(Field& field)
{
    FO_STACK_TRACE_ENTRY();

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

void MapView::Resize(msize size)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    size.width = std::clamp(size.width, MAXHEX_MIN, MAXHEX_MAX);
    size.height = std::clamp(size.height, MAXHEX_MIN, MAXHEX_MAX);

    for (int i = 0, j = _hVisible * _wVisible; i < j; i++) {
        const auto& vf = _viewField[i];

        if (_mapSize.IsValidPos(vf.RawHex)) {
            auto& field = _hexField->GetCellForWriting(_mapSize.FromRawPos(vf.RawHex));
            field.IsView = false;
            InvalidateSpriteChain(field);
        }
    }

    // Remove objects on shrink
    for (uint16 hy = 0; hy < std::max(size.height, _mapSize.height); hy++) {
        for (uint16 hx = 0; hx < std::max(size.width, _mapSize.width); hx++) {
            if (hx >= size.width || hy >= size.height) {
                const auto& field = _hexField->GetCellForReading({hx, hy});

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

                FO_RUNTIME_ASSERT(field.Critters.empty());
                FO_RUNTIME_ASSERT(field.Items.empty());
                FO_RUNTIME_ASSERT(field.GroundTiles.empty());
                FO_RUNTIME_ASSERT(field.RoofTiles.empty());
                FO_RUNTIME_ASSERT(!field.SpriteChain);
            }
        }
    }

    SetSize(size);
    _mapSize = size;

    _hexTrack.resize(_mapSize.GetSquare());
    MemFill(_hexTrack.data(), 0, _hexTrack.size());
    _hexLight.resize(static_cast<size_t>(_mapSize.GetSquare()) * 3);
    _hexField->Resize(_mapSize);

    for (uint16 hy = 0; hy < _mapSize.height; hy++) {
        for (uint16 hx = 0; hx < _mapSize.width; hx++) {
            CalculateHexLight({hx, hy}, _hexField->GetCellForReading({hx, hy}));
        }
    }

    RefreshMap();
}

void MapView::SwitchShowHex()
{
    FO_STACK_TRACE_ENTRY();

    _isShowHex = !_isShowHex;

    RefreshMap();
}

void MapView::ClearHexTrack()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    MemFill(_hexTrack.data(), 0, _hexTrack.size() * sizeof(char));
}

void MapView::SwitchShowTrack()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    _isShowTrack = !_isShowTrack;

    if (!_isShowTrack) {
        ClearHexTrack();
    }

    RefreshMap();
}

void MapView::InitView(ipos screen_raw_hex)
{
    FO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        // Get center offset
        const auto view_size = GetViewSize();
        const auto hw = view_size.width / 2 + _wRight;
        const auto hv = view_size.height / 2 + _hTop;
        auto vw = hv / 2 + std::abs(hv % 2) + 1;
        auto vh = hv - vw / 2 - 1;

        for (auto i = 0; i < hw; i++) {
            if ((vw % 2) != 0) {
                vh--;
            }
            vw++;
        }

        screen_raw_hex.x -= std::abs(vw);
        screen_raw_hex.y -= std::abs(vh);

        const auto xa = -(_wRight * _engine->Settings.MapHexWidth);
        const auto xb = -(_engine->Settings.MapHexWidth / 2) - (_wRight * _engine->Settings.MapHexWidth);
        auto oy = -_engine->Settings.MapHexLineHeight * _hTop;
        const auto wx = iround(static_cast<float>(_engine->Settings.ScreenWidth) * GetSpritesZoom());

        for (auto yv = 0; yv < _hVisible; yv++) {
            auto hx = screen_raw_hex.x + yv / 2 + std::abs(yv % 2);
            auto hy = screen_raw_hex.y + (yv - (hx - screen_raw_hex.x - std::abs(screen_raw_hex.x % 2)) / 2);
            auto ox = (yv % 2) != 0 ? xa : xb;

            if (yv == 0 && (screen_raw_hex.x % 2) != 0) {
                hy++;
            }

            for (auto xv = 0; xv < _wVisible; xv++) {
                auto& vf = _viewField[yv * _wVisible + xv];
                vf.Offset = {wx - ox, oy};
                vf.Offsetf = {static_cast<float>(vf.Offset.x), static_cast<float>(vf.Offset.y)};
                vf.RawHex = {hx, hy};

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
        const auto view_size = GetViewSize();
        const auto halfw = view_size.width / 2 + _wRight;
        const auto halfh = view_size.height / 2 + _hTop;
        auto basehx = screen_raw_hex.x - halfh / 2 - halfw;
        auto basehy = screen_raw_hex.y - halfh / 2 + halfw;
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
                _viewField[vpos].Offset = {wx - x, y};
                _viewField[vpos].Offsetf = {static_cast<float>(_viewField[vpos].Offset.x), static_cast<float>(_viewField[vpos].Offset.y)};
                _viewField[vpos].RawHex = {hx, hy};

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
    FO_STACK_TRACE_ENTRY();

    if (!_viewField.empty()) {
        for (int i = 0, j = _hVisible * _wVisible; i < j; i++) {
            const auto& vf = _viewField[i];

            if (_mapSize.IsValidPos(vf.RawHex)) {
                auto& field = _hexField->GetCellForWriting(_mapSize.FromRawPos(vf.RawHex));
                field.IsView = false;
                InvalidateSpriteChain(field);
            }
        }
    }

    const auto view_size = GetViewSize();

    _wVisible = view_size.width + _wLeft + _wRight;
    _hVisible = view_size.height + _hTop + _hBottom;

    _viewField.resize(static_cast<size_t>(_hVisible) * _wVisible);
}

void MapView::AddSpriteToChain(Field& field, MapSprite* mspr)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

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
    FO_NO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    // SpriteChain changed outside loop
    if (field.SpriteChain != nullptr) {
        FO_RUNTIME_ASSERT(field.SpriteChain->Valid);
        while (field.SpriteChain != nullptr) {
            FO_RUNTIME_ASSERT(field.SpriteChain->Valid);
            field.SpriteChain->Invalidate();
        }
    }
}

void MapView::ChangeZoom(int zoom)
{
    FO_STACK_TRACE_ENTRY();

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
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                if ((x != 0 || y != 0) && ScrollCheck(x, y)) {
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

auto MapView::GetScreenRawHex() const -> ipos
{
    FO_STACK_TRACE_ENTRY();

    return _screenRawHex;
}

auto MapView::GetHexPosition(mpos hex) const -> ipos
{
    FO_STACK_TRACE_ENTRY();

    const auto& center_field = _viewField[_hVisible / 2 * _wVisible + _wVisible / 2];
    const auto center_hex = center_field.RawHex;
    const auto hex_offset = _engine->Geometry.GetHexInterval(center_hex, ipos {hex.x, hex.y});

    return {center_field.Offset.x + hex_offset.x, center_field.Offset.y + hex_offset.y};
}

void MapView::DrawMap()
{
    FO_STACK_TRACE_ENTRY();

    _engine->SprMngr.SetSpritesZoom(GetSpritesZoom());
    auto reset_spr_mngr_zoom = ScopeCallback([this]() noexcept { _engine->SprMngr.SetSpritesZoom(1.0f); });

    // Prepare light
    ProcessLighting();

    // Prepare fog
    PrepareFogToDraw();

    // Prerendered offsets
    const auto ox = _rtScreenOx - iround(static_cast<float>(_engine->Settings.ScreenOffset.x) / GetSpritesZoom());
    const auto oy = _rtScreenOy - iround(static_cast<float>(_engine->Settings.ScreenOffset.y) / GetSpritesZoom());
    const auto prerendered_rect = IRect(ox, oy, ox + _engine->Settings.ScreenWidth, oy + (_engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight));

    // Separate render target
    if (_engine->EffectMngr.Effects.FlushMap != nullptr) {
        if (_rtMap == nullptr) {
            _rtMap = _engine->SprMngr.GetRtMngr().CreateRenderTarget(false, RenderTarget::SizeKindType::Map, {}, false);
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

void MapView::PrepareFogToDraw()
{
    FO_STACK_TRACE_ENTRY();

    if (_mapperMode || _rtFog == nullptr) {
        return;
    }

    if (_rebuildFog) {
        _rebuildFog = false;

        _fogLookPoints.clear();
        _fogShootPoints.clear();

        CritterHexView* chosen;

        if (const auto it = std::ranges::find_if(_critters, [](auto&& cr) { return cr->GetIsChosen(); }); it != _critters.end()) {
            chosen = it->get();
        }
        else {
            chosen = nullptr;
        }

        if (chosen != nullptr && (_drawLookBorders || _drawShootBorders)) {
            const auto dist = chosen->GetLookDistance() + _engine->Settings.FogExtraLength;
            const auto base_hex = chosen->GetHex();
            const int chosen_dir = chosen->GetDir();
            const auto dist_shoot = _shootBordersDist;
            const auto half_hw = _engine->Settings.MapHexWidth / 2;
            const auto half_hh = _engine->Settings.MapHexHeight / 2;

            const ipos base_pos = GetHexPosition(base_hex);
            const auto center_look_point = PrimitivePoint {{base_pos.x + half_hw, base_pos.y + half_hh}, ucolor {0, 0, 0, 0}, &chosen->SprOffset};
            const auto center_shoot_point = PrimitivePoint {{base_pos.x + half_hw, base_pos.y + half_hh}, ucolor {0, 0, 0, 255}, &chosen->SprOffset};

            auto target_raw_hex = ipos {base_hex.x, base_hex.y};

            size_t look_points_added = 0;
            size_t shoot_points_added = 0;

            auto seek_start = true;
            for (auto i = 0; i < (GameSettings::HEXAGONAL_GEOMETRY ? 6 : 4); i++) {
                // ReSharper disable once CppUnreachableCode
                const auto dir = (GameSettings::HEXAGONAL_GEOMETRY ? (i + 2) % 6 : ((i + 1) * 2) % 8);

                // ReSharper disable once CppUnreachableCode
                for (int j = 0, jj = static_cast<int>(GameSettings::HEXAGONAL_GEOMETRY ? dist : dist * 2); j < jj; j++) {
                    if (seek_start) {
                        // Move to start position
                        for (uint l = 0; l < dist; l++) {
                            GeometryHelper::MoveHexByDirUnsafe(target_raw_hex, GameSettings::HEXAGONAL_GEOMETRY ? 0 : 7);
                        }
                        seek_start = false;
                        j = -1;
                    }
                    else {
                        // Move to next hex
                        GeometryHelper::MoveHexByDirUnsafe(target_raw_hex, static_cast<uint8>(dir));
                    }

                    auto target_hex = _mapSize.ClampPos(target_raw_hex);

                    if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
                        const int dir_ = GeometryHelper::GetFarDir(base_hex, target_hex);
                        auto ii = (chosen_dir > dir_ ? chosen_dir - dir_ : dir_ - chosen_dir);
                        if (ii > static_cast<int>(GameSettings::MAP_DIR_COUNT / 2)) {
                            ii = static_cast<int>(GameSettings::MAP_DIR_COUNT - ii);
                        }

                        const auto look_dist = dist - dist * _engine->Settings.LookDir[ii] / 100;

                        mpos block;
                        TraceBullet(base_hex, target_hex, look_dist, 0.0f, nullptr, CritterFindType::Any, nullptr, &block, nullptr, false);
                        target_hex = block;
                    }

                    if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_TRACE_CLIENT)) {
                        mpos block;
                        TraceBullet(base_hex, target_hex, 0, 0.0f, nullptr, CritterFindType::Any, nullptr, &block, nullptr, true);
                        target_hex = block;
                    }

                    auto dist_look = GeometryHelper::DistGame(base_hex, target_hex);

                    if (_drawLookBorders) {
                        const auto hex_pos = GetHexPosition(target_hex);
                        const auto color = ucolor {255, static_cast<uint8>(dist_look * 255 / dist), 0, 0};
                        const auto* offset = dist_look == dist ? &chosen->SprOffset : nullptr;

                        _fogLookPoints.emplace_back(PrimitivePoint {{hex_pos.x + half_hw, hex_pos.y + half_hh}, color, offset});

                        if (++look_points_added % 2 == 0) {
                            _fogLookPoints.emplace_back(center_look_point);
                        }
                    }

                    if (_drawShootBorders) {
                        mpos block_hex;
                        const auto max_shoot_dist = std::max(std::min(dist_look, dist_shoot), 0u) + 1u;

                        TraceBullet(base_hex, target_hex, max_shoot_dist, 0.0f, nullptr, CritterFindType::Any, nullptr, &block_hex, nullptr, true);

                        const auto block_hex_pos = GetHexPosition(block_hex);
                        const auto result_shoot_dist = GeometryHelper::DistGame(base_hex, block_hex);
                        const auto color = ucolor {255, static_cast<uint8>(result_shoot_dist * 255 / max_shoot_dist), 0, 255};
                        const auto* offset = result_shoot_dist == max_shoot_dist ? &chosen->SprOffset : nullptr;

                        _fogShootPoints.emplace_back(PrimitivePoint {{block_hex_pos.x + half_hw, block_hex_pos.y + half_hh}, color, offset});

                        if (++shoot_points_added % 2 == 0) {
                            _fogShootPoints.emplace_back(center_shoot_point);
                        }
                    }
                }
            }

            _fogOffset = chosen != nullptr ? &chosen->SprOffset : nullptr;
            _fogLastOffset = _fogOffset != nullptr ? *_fogOffset : ipos {};
            _fogForceRerender = true;
        }
    }

    if (_fogForceRerender || _fogOffset == nullptr || *_fogOffset != _fogLastOffset) {
        _fogForceRerender = false;

        if (_fogOffset != nullptr) {
            _fogLastOffset = *_fogOffset;
        }

        const float zoom = GetSpritesZoom();
        const auto offset = fpos {static_cast<float>(_rtScreenOx), static_cast<float>(_rtScreenOy)};

        _engine->SprMngr.GetRtMngr().PushRenderTarget(_rtFog);
        _engine->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);

        _engine->SprMngr.DrawPoints(_fogLookPoints, RenderPrimitiveType::TriangleStrip, &zoom, &offset, _engine->EffectMngr.Effects.Fog);
        _engine->SprMngr.DrawPoints(_fogShootPoints, RenderPrimitiveType::TriangleStrip, &zoom, &offset, _engine->EffectMngr.Effects.Fog);

        _engine->SprMngr.GetRtMngr().PopRenderTarget();
    }
}

auto MapView::IsScrollEnabled() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _engine->Settings.ScrollMouseLeft || _engine->Settings.ScrollKeybLeft || _engine->Settings.ScrollMouseRight || //
        _engine->Settings.ScrollKeybRight || _engine->Settings.ScrollMouseUp || _engine->Settings.ScrollKeybUp || //
        _engine->Settings.ScrollMouseDown || _engine->Settings.ScrollKeybDown;
}

auto MapView::Scroll() -> bool
{
    FO_STACK_TRACE_ENTRY();

    // Scroll delay
    float time_k = 1.0f;

    if (_engine->Settings.ScrollDelay != 0) {
        const auto time = _engine->GameTime.GetFrameTime();

        if (time - _scrollLastTime < std::chrono::milliseconds {_engine->Settings.ScrollDelay / 2}) {
            return false;
        }

        time_k = (time - _scrollLastTime).to_ms<float>() / static_cast<float>(_engine->Settings.ScrollDelay);
        _scrollLastTime = time;
    }

    const bool is_scroll = IsScrollEnabled();
    int scr_ox = _engine->Settings.ScreenOffset.x;
    int scr_oy = _engine->Settings.ScreenOffset.y;
    const int prev_scr_ox = scr_ox;
    const int prev_scr_oy = scr_oy;

    if (is_scroll && AutoScroll.CanStop) {
        AutoScroll.Active = false;
    }

    // Check critter scroll lock
    if (AutoScroll.HardLockedCritter && !is_scroll) {
        const auto* cr = GetCritter(AutoScroll.HardLockedCritter);

        if (cr != nullptr && ipos {cr->GetHex().x, cr->GetHex().y} != _screenRawHex) {
            ScrollToHex(cr->GetHex(), 0.02f, true);
        }
    }

    if (AutoScroll.SoftLockedCritter && !is_scroll) {
        const auto* cr = GetCritter(AutoScroll.SoftLockedCritter);

        if (cr != nullptr && cr->GetHex() != AutoScroll.CritterLastHex) {
            const auto hex_offset = _engine->Geometry.GetHexInterval(AutoScroll.CritterLastHex, cr->GetHex());
            ScrollOffset(hex_offset, 0.02f, true);
            AutoScroll.CritterLastHex = cr->GetHex();
        }
    }

    int xscroll = 0;
    int yscroll = 0;

    if (AutoScroll.Active) {
        AutoScroll.OffsetStep.x += AutoScroll.Offset.x * AutoScroll.Speed * time_k;
        AutoScroll.OffsetStep.y += AutoScroll.Offset.y * AutoScroll.Speed * time_k;

        xscroll = iround(AutoScroll.OffsetStep.x);
        yscroll = iround(AutoScroll.OffsetStep.y);

        if (xscroll > _engine->Settings.MapHexWidth) {
            xscroll = _engine->Settings.MapHexWidth;
            AutoScroll.OffsetStep.x = static_cast<float>(_engine->Settings.MapHexWidth);
        }
        if (xscroll < -_engine->Settings.MapHexWidth) {
            xscroll = -_engine->Settings.MapHexWidth;
            AutoScroll.OffsetStep.x = -static_cast<float>(_engine->Settings.MapHexWidth);
        }
        if (yscroll > _engine->Settings.MapHexLineHeight * 2) {
            yscroll = _engine->Settings.MapHexLineHeight * 2;
            AutoScroll.OffsetStep.y = static_cast<float>(_engine->Settings.MapHexLineHeight * 2);
        }
        if (yscroll < -_engine->Settings.MapHexLineHeight * 2) {
            yscroll = -_engine->Settings.MapHexLineHeight * 2;
            AutoScroll.OffsetStep.y = -static_cast<float>(_engine->Settings.MapHexLineHeight * 2);
        }

        AutoScroll.Offset.x -= static_cast<float>(xscroll);
        AutoScroll.Offset.y -= static_cast<float>(yscroll);
        AutoScroll.OffsetStep.x -= static_cast<float>(xscroll);
        AutoScroll.OffsetStep.y -= static_cast<float>(yscroll);

        if (xscroll == 0 && yscroll == 0) {
            return false;
        }

        if (GenericUtils::DistSqrt({0, 0}, {iround(AutoScroll.Offset.x), iround(AutoScroll.Offset.y)}) == 0) {
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

        if (xscroll == 0 && yscroll == 0) {
            return false;
        }

        xscroll = iround(static_cast<float>(xscroll * _engine->Settings.ScrollStep) * GetSpritesZoom() * time_k);
        yscroll = iround(static_cast<float>(yscroll * (_engine->Settings.ScrollStep * (_engine->Settings.MapHexLineHeight * 2) / _engine->Settings.MapHexWidth)) * GetSpritesZoom() * time_k);
    }

    scr_ox += xscroll;
    scr_oy += yscroll;

    if (_engine->Settings.ScrollCheck) {
        int xmod = 0;
        int ymod = 0;

        if (scr_ox - _engine->Settings.ScreenOffset.x > 0) {
            xmod = 1;
        }
        if (scr_ox - _engine->Settings.ScreenOffset.x < 0) {
            xmod = -1;
        }
        if (scr_oy - _engine->Settings.ScreenOffset.y > 0) {
            ymod = -1;
        }
        if (scr_oy - _engine->Settings.ScreenOffset.y < 0) {
            ymod = 1;
        }

        if ((xmod != 0 || ymod != 0) && ScrollCheck(xmod, ymod)) {
            if (xmod != 0 && ymod != 0 && !ScrollCheck(0, ymod)) {
                scr_ox = 0;
            }
            else if (xmod != 0 && ymod != 0 && !ScrollCheck(xmod, 0)) {
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

    int xmod = 0;
    int ymod = 0;

    if (scr_ox >= _engine->Settings.MapHexWidth) {
        xmod = 1;
        scr_ox -= _engine->Settings.MapHexWidth;
        scr_ox = std::min(scr_ox, _engine->Settings.MapHexWidth);
    }
    else if (scr_ox <= -_engine->Settings.MapHexWidth) {
        xmod = -1;
        scr_ox += _engine->Settings.MapHexWidth;
        scr_ox = std::max(scr_ox, -_engine->Settings.MapHexWidth);
    }
    if (scr_oy >= (_engine->Settings.MapHexLineHeight * 2)) {
        ymod = -2;
        scr_oy -= _engine->Settings.MapHexLineHeight * 2;
        scr_oy = std::min(scr_oy, _engine->Settings.MapHexLineHeight * 2);
    }
    else if (scr_oy <= -(_engine->Settings.MapHexLineHeight * 2)) {
        ymod = 2;
        scr_oy += _engine->Settings.MapHexLineHeight * 2;
        scr_oy = std::max(scr_oy, -(_engine->Settings.MapHexLineHeight * 2));
    }

    _engine->Settings.ScreenOffset.x = scr_ox;
    _engine->Settings.ScreenOffset.y = scr_oy;

    if (xmod != 0 || ymod != 0) {
        RebuildMapOffset({xmod, ymod});

        if (_engine->Settings.ScrollCheck) {
            if (_engine->Settings.ScreenOffset.x > 0 && ScrollCheck(1, 0)) {
                _engine->Settings.ScreenOffset.x = 0;
            }
            else if (_engine->Settings.ScreenOffset.x < 0 && ScrollCheck(-1, 0)) {
                _engine->Settings.ScreenOffset.x = 0;
            }
            if (_engine->Settings.ScreenOffset.y > 0 && ScrollCheck(0, -1)) {
                _engine->Settings.ScreenOffset.y = 0;
            }
            else if (_engine->Settings.ScreenOffset.y < 0 && ScrollCheck(0, 1)) {
                _engine->Settings.ScreenOffset.y = 0;
            }
        }
    }

    if (!_mapperMode) {
        const auto final_scr_ox = _engine->Settings.ScreenOffset.x - prev_scr_ox + xmod * _engine->Settings.MapHexWidth;
        const auto final_scr_oy = _engine->Settings.ScreenOffset.y - prev_scr_oy + (-ymod / 2) * (_engine->Settings.MapHexLineHeight * 2);

        if (final_scr_ox != 0 || final_scr_oy != 0) {
            _engine->OnScreenScroll.Fire({final_scr_ox, final_scr_oy});
        }
    }

    return xmod != 0 || ymod != 0;
}

auto MapView::ScrollCheckPos(int (&view_fields_to_check)[4], uint8 dir1, optional<uint8> dir2) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto max_vf = _wVisible * _hVisible;

    for (const auto vf_index : view_fields_to_check) {
        if (vf_index < 0 || vf_index >= max_vf) {
            return true;
        }

        if (!_mapSize.IsValidPos(_viewField[vf_index].RawHex)) {
            return true;
        }

        auto hex = _mapSize.FromRawPos(_viewField[vf_index].RawHex);

        GeometryHelper::MoveHexByDir(hex, dir1, _mapSize);

        if (_hexField->GetCellForReading(hex).Flags.ScrollBlock) {
            return true;
        }

        if (dir2.has_value()) {
            GeometryHelper::MoveHexByDir(hex, dir2.value(), _mapSize);

            if (_hexField->GetCellForReading(hex).Flags.ScrollBlock) {
                return true;
            }
        }
    }

    return false;
}

auto MapView::ScrollCheck(int xmod, int ymod) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto view_size = GetViewSize();

    int positions_left[4] = {
        _hTop * _wVisible + _wRight + view_size.width, // Left top
        (_hTop + view_size.height - 1) * _wVisible + _wRight + view_size.width, // Left bottom
        (_hTop + 1) * _wVisible + _wRight + view_size.width, // Left top 2
        (_hTop + view_size.height - 1 - 1) * _wVisible + _wRight + view_size.width, // Left bottom 2
    };
    int positions_right[4] = {
        (_hTop + view_size.height - 1) * _wVisible + _wRight + 1, // Right bottom
        _hTop * _wVisible + _wRight + 1, // Right top
        (_hTop + view_size.height - 1 - 1) * _wVisible + _wRight + 1, // Right bottom 2
        (_hTop + 1) * _wVisible + _wRight + 1, // Right top 2
    };

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        if (ymod < 0 && (ScrollCheckPos(positions_left, 0, 5) || ScrollCheckPos(positions_right, 5, 0))) {
            return true; // Up
        }
        if (ymod > 0 && (ScrollCheckPos(positions_left, 2, 3) || ScrollCheckPos(positions_right, 3, 2))) {
            return true; // Down
        }
        if (xmod > 0 && (ScrollCheckPos(positions_left, 4, std::nullopt) || ScrollCheckPos(positions_right, 4, std::nullopt))) {
            return true; // Left
        }
        if (xmod < 0 && (ScrollCheckPos(positions_right, 1, std::nullopt) || ScrollCheckPos(positions_left, 1, std::nullopt))) {
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
            if (xmod > 0 && (ScrollCheckPos(positions_left, 4, std::nullopt) || ScrollCheckPos(positions_right, 4, std::nullopt))) {
                return true; // Left
            }
            if (xmod < 0 && (ScrollCheckPos(positions_right, 1, std::nullopt) || ScrollCheckPos(positions_left, 1, std::nullopt))) {
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

void MapView::ScrollToHex(mpos hex, float speed, bool can_stop)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex_offset = _engine->Geometry.GetHexInterval(_screenRawHex, ipos {hex.x, hex.y});

    AutoScroll.Active = false;
    ScrollOffset(hex_offset, speed, can_stop);
}

void MapView::ScrollOffset(ipos offset, float speed, bool can_stop)
{
    FO_STACK_TRACE_ENTRY();

    if (!AutoScroll.Active) {
        AutoScroll.Active = true;
        AutoScroll.Offset = {};
        AutoScroll.OffsetStep = {};
    }

    AutoScroll.CanStop = can_stop;
    AutoScroll.Speed = speed;
    AutoScroll.Offset.x += -static_cast<float>(offset.x);
    AutoScroll.Offset.y += -static_cast<float>(offset.y);
}

void MapView::AddCritterToField(CritterHexView* cr)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = cr->GetHex();
    FO_RUNTIME_ASSERT(_mapSize.IsValidPos(hex));

    auto& field = _hexField->GetCellForWriting(hex);

    vec_add_unique_value(field.Critters, cr);

    RecacheHexFlags(field);
    SetMultihexCritter(cr, true);
    UpdateCritterLightSource(cr);

    if (!_mapLoading && IsHexToDraw(hex)) {
        auto* spr = cr->InsertSprite(_mapSprites, EvaluateCritterDrawOrder(cr), hex, &field.Offset);

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
    FO_STACK_TRACE_ENTRY();

    const auto hex = cr->GetHex();
    auto& field = _hexField->GetCellForWriting(hex);

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
    FO_STACK_TRACE_ENTRY();

    if (!id) {
        return nullptr;
    }

    const auto it = _crittersMap.find(id);
    return it != _crittersMap.end() ? it->second : nullptr;
}

auto MapView::GetNonDeadCritter(mpos hex) -> CritterHexView*
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

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

auto MapView::AddReceivedCritter(ident_t id, hstring pid, mpos hex, int16 dir_angle, const vector<vector<uint8>>& data) -> CritterHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(id);
    FO_RUNTIME_ASSERT(_mapSize.IsValidPos(hex));

    const auto* proto = _engine->ProtoMngr.GetProtoCritter(pid);
    auto cr = SafeAlloc::MakeRefCounted<CritterHexView>(this, id, proto);

    cr->RestoreData(data);
    cr->SetHex(hex);
    cr->ChangeDirAngle(dir_angle);

    return AddCritterInternal(cr.get());
}

auto MapView::AddMapperCritter(hstring pid, mpos hex, int16 dir_angle, const Properties* props) -> CritterHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);
    FO_RUNTIME_ASSERT(_mapSize.IsValidPos(hex));

    const auto* proto = _engine->ProtoMngr.GetProtoCritter(pid);
    auto cr = SafeAlloc::MakeRefCounted<CritterHexView>(this, GenTempEntityId(), proto, props);

    cr->SetHex(hex);
    cr->ChangeDirAngle(dir_angle);

    return AddCritterInternal(cr.get());
}

auto MapView::AddCritterInternal(CritterHexView* cr) -> CritterHexView*
{
    FO_STACK_TRACE_ENTRY();

    if (cr->GetId()) {
        if (auto* prev_cr = GetCritter(cr->GetId()); prev_cr != nullptr) {
            cr->RestoreFading(prev_cr->StoreFading());
            DestroyCritter(prev_cr);
        }

        _crittersMap.emplace(cr->GetId(), cr);
    }

    cr->SetMapId(GetId());
    cr->Init();

    vec_add_unique_value(_critters, cr);

    AddCritterToField(cr);

    return cr;
}

void MapView::DestroyCritter(CritterHexView* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr->GetMap() == this);

    vec_remove_unique_value(_critters, cr);

    if (cr->GetId()) {
        const auto it_map = _crittersMap.find(cr->GetId());
        FO_RUNTIME_ASSERT(it_map != _crittersMap.end());
        _crittersMap.erase(it_map);
    }

    RemoveCritterFromField(cr);

    CleanLightSourceOffsets(cr->GetId());

    cr->DestroySelf();
}

auto MapView::GetCritters(mpos hex, CritterFindType find_type) -> vector<CritterHexView*>
{
    FO_STACK_TRACE_ENTRY();

    vector<CritterHexView*> critters;
    const auto& field = _hexField->GetCellForReading(hex);

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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    if (_crittersContour == contour) {
        return;
    }

    _crittersContour = contour;

    for (auto& cr : _critters) {
        if (!cr->GetIsChosen() && cr->IsSpriteValid() && !cr->IsDead() && cr->GetId() != _critterContourCrId) {
            cr->GetSprite()->SetContour(contour);
        }
    }
}

void MapView::MoveCritter(CritterHexView* cr, mpos to_hex, bool smoothly)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr->GetMap() == this);
    FO_RUNTIME_ASSERT(_mapSize.IsValidPos(to_hex));

    const auto cur_hex = cr->GetHex();

    if (cur_hex == to_hex) {
        return;
    }

    RemoveCritterFromField(cr);

    cr->SetHex(to_hex);

    if (smoothly) {
        const auto hex_offset = _engine->Geometry.GetHexInterval(to_hex, cur_hex);

        cr->AddExtraOffs(hex_offset);
    }

    AddCritterToField(cr);

    if (cr->GetIsChosen()) {
        RebuildFog();
    }
}

void MapView::SetMultihexCritter(CritterHexView* cr, bool set)
{
    FO_STACK_TRACE_ENTRY();

    const uint multihex = cr->GetMultihex();

    if (multihex != 0) {
        const auto hex = cr->GetHex();
        auto&& [sx, sy] = _engine->Geometry.GetHexOffsets(hex);

        for (uint i = 0, j = GenericUtils::NumericalNumber(multihex) * GameSettings::MAP_DIR_COUNT; i < j; i++) {
            const auto multihex_raw_hex = ipos {static_cast<int>(hex.x) + sx[i], static_cast<int>(hex.y) + sy[i]};

            if (_mapSize.IsValidPos(multihex_raw_hex)) {
                auto& field = _hexField->GetCellForWriting(_mapSize.FromRawPos(multihex_raw_hex));

                if (set) {
                    FO_RUNTIME_ASSERT(std::ranges::find(field.MultihexCritters,, cr) == field.MultihexCritters.end());
                    field.MultihexCritters.emplace_back(cr);
                }
                else {
                    const auto it = std::ranges::find(field.MultihexCritters, cr);
                    FO_RUNTIME_ASSERT(it != field.MultihexCritters.end());
                    field.MultihexCritters.erase(it);
                }

                RecacheHexFlags(field);
            }
        }
    }
}

auto MapView::GetHexAtScreenPos(ipos pos, mpos& hex, ipos* hex_offset) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const float spr_zoom = GetSpritesZoom();
    const float xf = static_cast<float>(pos.x) - static_cast<float>(_engine->Settings.ScreenOffset.x) / spr_zoom;
    const float yf = static_cast<float>(pos.y) - static_cast<float>(_engine->Settings.ScreenOffset.y) / spr_zoom;
    const float ox = static_cast<float>(_engine->Settings.MapHexWidth) / spr_zoom;
    const float oy = static_cast<float>(_engine->Settings.MapHexHeight) / spr_zoom;
    int y2 = 0;

    for (int vf_y = 0; vf_y < _hVisible; vf_y++) {
        for (int vf_x = 0; vf_x < _wVisible; vf_x++) {
            const int vf_index = y2 + vf_x;
            const float vf_ox = _viewField[vf_index].Offsetf.x / spr_zoom;
            const float vf_oy = _viewField[vf_index].Offsetf.y / spr_zoom;

            if (xf >= vf_ox && xf < vf_ox + ox && yf >= vf_oy && yf < vf_oy + oy) {
                ipos raw_hex = _viewField[vf_index].RawHex;

                // Correct with hex color mask
                if (_picHexMask) {
                    const int mask_x = std::clamp(iround((xf - vf_ox) * spr_zoom), 0, _picHexMask->Size.width - 1);
                    const int mask_y = std::clamp(iround((yf - vf_oy) * spr_zoom), 0, _picHexMask->Size.height - 1);
                    const ucolor mask_color = _picHexMaskData[mask_y * _picHexMask->Size.width + mask_x];
                    const uint8 mask_color_r = mask_color.comp.r;

                    if (mask_color_r == 50) {
                        GeometryHelper::MoveHexByDirUnsafe(raw_hex, GameSettings::HEXAGONAL_GEOMETRY ? 5u : 6u);
                    }
                    else if (mask_color_r == 100) {
                        GeometryHelper::MoveHexByDirUnsafe(raw_hex, 0);
                    }
                    else if (mask_color_r == 150) {
                        GeometryHelper::MoveHexByDirUnsafe(raw_hex, GameSettings::HEXAGONAL_GEOMETRY ? 3u : 4u);
                    }
                    else if (mask_color_r == 200) {
                        GeometryHelper::MoveHexByDirUnsafe(raw_hex, 2u);
                    }
                }

                if (_mapSize.IsValidPos(raw_hex)) {
                    hex = _mapSize.FromRawPos(raw_hex);

                    if (hex_offset != nullptr) {
                        *hex_offset = {iround((xf - vf_ox) * spr_zoom) - _engine->Settings.MapHexWidth / 2, iround((yf - vf_oy) * spr_zoom) - _engine->Settings.MapHexHeight / 2};
                    }

                    return true;
                }
            }
        }

        y2 += _wVisible;
    }

    return false;
}

auto MapView::GetItemAtScreenPos(ipos pos, bool& item_egg, int extra_range, bool check_transparent) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    vector<ItemHexView*> pix_item;
    vector<ItemHexView*> pix_item_egg;
    const auto is_egg = _engine->SprMngr.IsEggTransp(pos);

    for (auto* item : _nonTileItems) {
        if (!item->IsSpriteVisible() || item->IsFinishing()) {
            continue;
        }

        const auto* spr = item->Spr;
        const auto hex = item->GetHex();
        const auto& field = _hexField->GetCellForReading(hex);
        const auto spr_zoom = GetSpritesZoom();
        const auto l = iround(static_cast<float>(field.Offset.x + item->SprOffset.x + spr->Offset.x + _engine->Settings.MapHexWidth / 2 + _engine->Settings.ScreenOffset.x - spr->Size.width / 2) / spr_zoom) - extra_range;
        const auto r = iround(static_cast<float>(field.Offset.x + item->SprOffset.x + spr->Offset.x + _engine->Settings.MapHexWidth / 2 + _engine->Settings.ScreenOffset.x + spr->Size.width / 2) / spr_zoom) + extra_range;
        const auto t = iround(static_cast<float>(field.Offset.y + item->SprOffset.y + spr->Offset.y + _engine->Settings.MapHexHeight / 2 + _engine->Settings.ScreenOffset.y - spr->Size.height) / spr_zoom) - extra_range;
        const auto b = iround(static_cast<float>(field.Offset.y + item->SprOffset.y + spr->Offset.y + _engine->Settings.MapHexHeight / 2 + _engine->Settings.ScreenOffset.y) / spr_zoom) + extra_range;

        if (pos.x < l || pos.x > r || pos.y < t || pos.y > b) {
            continue;
        }

        if (item->GetSprite()->CheckHit({pos.x - l + extra_range, pos.y - t + extra_range}, check_transparent)) {
            if (is_egg && _engine->SprMngr.CheckEggAppearence(hex, item->GetEggType())) {
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
            std::ranges::sort(pix_item_egg, Sorter::ByTreeIndex);
            std::ranges::sort(pix_item_egg, Sorter::ByTransparent);
        }
        item_egg = true;
        return pix_item_egg[0];
    }

    // Visible items
    if (pix_item.size() > 1) {
        std::ranges::sort(pix_item, Sorter::ByTreeIndex);
        std::ranges::sort(pix_item, Sorter::ByTransparent);
    }

    item_egg = false;
    return pix_item[0];
}

auto MapView::GetCritterAtScreenPos(ipos pos, bool ignore_dead_and_chosen, int extra_range, bool check_transparent) -> CritterHexView*
{
    FO_STACK_TRACE_ENTRY();

    if (!_engine->Settings.ShowCrit) {
        return nullptr;
    }

    vector<CritterHexView*> critters;

    for (auto& cr : _critters) {
        if (!cr->IsSpriteVisible() || cr->IsFinishing()) {
            continue;
        }
        if (ignore_dead_and_chosen && (cr->IsDead() || cr->GetIsChosen())) {
            continue;
        }

        const auto rect = cr->GetViewRect();
        const auto l = iround(static_cast<float>(rect.Left + _engine->Settings.ScreenOffset.x) / GetSpritesZoom()) - extra_range;
        const auto r = iround(static_cast<float>(rect.Right + _engine->Settings.ScreenOffset.x) / GetSpritesZoom()) + extra_range;
        const auto t = iround(static_cast<float>(rect.Top + _engine->Settings.ScreenOffset.y) / GetSpritesZoom()) - extra_range;
        const auto b = iround(static_cast<float>(rect.Bottom + _engine->Settings.ScreenOffset.y) / GetSpritesZoom()) + extra_range;

        if (pos.x >= l && pos.x <= r && pos.y >= t && pos.y <= b) {
            if (check_transparent) {
                const auto rect_draw = cr->GetSprite()->GetDrawRect();
                const auto l_draw = iround(static_cast<float>(rect_draw.Left + _engine->Settings.ScreenOffset.x) / GetSpritesZoom());
                const auto t_draw = iround(static_cast<float>(rect_draw.Top + _engine->Settings.ScreenOffset.y) / GetSpritesZoom());

                if (_engine->SprMngr.SpriteHitTest(cr->Spr, {pos.x - l_draw, pos.y - t_draw}, true)) {
                    critters.emplace_back(cr.get());
                }
            }
            else {
                critters.emplace_back(cr.get());
            }
        }
    }

    if (critters.empty()) {
        return nullptr;
    }

    if (critters.size() > 1) {
        std::ranges::sort(critters, [](auto* cr1, auto* cr2) { return cr1->GetSprite()->TreeIndex > cr2->GetSprite()->TreeIndex; });
    }
    return critters.front();
}

auto MapView::GetEntityAtScreenPos(ipos pos, int extra_range, bool check_transparent) -> ClientEntity*
{
    FO_STACK_TRACE_ENTRY();

    bool item_egg = false;
    auto* item = GetItemAtScreenPos(pos, item_egg, extra_range, check_transparent);
    auto* cr = GetCritterAtScreenPos(pos, false, extra_range, check_transparent);

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

auto MapView::FindPath(CritterHexView* cr, mpos start_hex, mpos& target_hex, int cut) -> optional<FindPathResult>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!cr || cr->GetMap() == this);

    if (start_hex == target_hex) {
        return FindPathResult();
    }

    const auto max_path_find_len = _engine->Settings.MaxPathFindLength;
    _findPathGrid.resize((static_cast<size_t>(max_path_find_len) * 2 + 2) * (max_path_find_len * 2 + 2));
    MemFill(_findPathGrid.data(), 0, _findPathGrid.size() * sizeof(int16));

    const auto grid_offset = start_hex;
    const auto grid_at = [&](mpos hex) -> short& { return _findPathGrid[((max_path_find_len + 1) + (hex.y) - grid_offset.y) * (max_path_find_len * 2 + 2) + ((max_path_find_len + 1) + (hex.x) - grid_offset.x)]; };

    int16 numindex = 1;
    grid_at(start_hex) = numindex;

    vector<mpos> coords;
    coords.reserve(max_path_find_len);
    coords.emplace_back(start_hex);

    const auto multihex = cr != nullptr ? cr->GetMultihex() : 0;
    auto p = 0;
    auto find_ok = false;

    while (!find_ok) {
        if (++numindex > max_path_find_len) {
            return std::nullopt;
        }

        auto p_togo = static_cast<int>(coords.size()) - p;

        if (p_togo == 0) {
            return std::nullopt;
        }

        for (auto i = 0; i < p_togo && !find_ok; ++i, ++p) {
            auto hex = coords[p];

            const auto [sx, sy] = _engine->Geometry.GetHexOffsets(hex);

            for (const auto j : xrange(GameSettings::MAP_DIR_COUNT)) {
                const auto raw_next_hex = ipos {hex.x + sx[j], hex.y + sy[j]};

                if (!_mapSize.IsValidPos(raw_next_hex)) {
                    continue;
                }

                const auto next_hex = _mapSize.FromRawPos(raw_next_hex);

                if (grid_at(next_hex) != 0) {
                    continue;
                }

                if (multihex == 0) {
                    if (_hexField->GetCellForReading(next_hex).Flags.MoveBlocked) {
                        continue;
                    }
                }
                else {
                    // Base hex
                    auto raw_next_hex2 = raw_next_hex;

                    for (uint k = 0; k < multihex; k++) {
                        GeometryHelper::MoveHexByDirUnsafe(raw_next_hex2, static_cast<uint8>(j));
                    }

                    if (!_mapSize.IsValidPos(raw_next_hex2)) {
                        continue;
                    }
                    if (_hexField->GetCellForReading(_mapSize.FromRawPos(raw_next_hex2)).Flags.MoveBlocked) {
                        continue;
                    }

                    // Clock wise hexes
                    // ReSharper disable once CppVariableCanBeMadeConstexpr
                    // ReSharper disable once CppUnreachableCode
                    const bool is_square_corner = !GameSettings::HEXAGONAL_GEOMETRY && (j % 2) != 0;
                    // ReSharper disable once CppUnreachableCode
                    const uint steps_count = is_square_corner ? multihex * 2 : multihex;
                    bool is_move_blocked = false;

                    {
                        // ReSharper disable once CppUnreachableCode
                        uint8 dir_ = GameSettings::HEXAGONAL_GEOMETRY ? ((j + 2) % 6) : ((j + 2) % 8);
                        // ReSharper disable once CppUnreachableCode
                        if (is_square_corner) {
                            dir_ = (dir_ + 1) % 8;
                        }

                        auto raw_next_hex3 = raw_next_hex2;

                        for (uint k = 0; k < steps_count && !is_move_blocked; k++) {
                            GeometryHelper::MoveHexByDirUnsafe(raw_next_hex3, dir_);
                            FO_RUNTIME_ASSERT(_mapSize.IsValidPos(raw_next_hex3));
                            is_move_blocked = _hexField->GetCellForReading(_mapSize.FromRawPos(raw_next_hex3)).Flags.MoveBlocked;
                        }
                    }

                    if (is_move_blocked) {
                        continue;
                    }

                    // Counter clock wise hexes
                    {
                        // ReSharper disable once CppUnreachableCode
                        uint8 dir_ = GameSettings::HEXAGONAL_GEOMETRY ? (j + 4) % 6 : (j + 6) % 8;
                        // ReSharper disable once CppUnreachableCode
                        if (is_square_corner) {
                            dir_ = (dir_ + 7) % 8;
                        }

                        auto raw_next_hex3 = raw_next_hex2;

                        for (uint k = 0; k < steps_count && !is_move_blocked; k++) {
                            GeometryHelper::MoveHexByDirUnsafe(raw_next_hex3, dir_);
                            FO_RUNTIME_ASSERT(_mapSize.IsValidPos(raw_next_hex3));
                            is_move_blocked = _hexField->GetCellForReading(_mapSize.FromRawPos(raw_next_hex3)).Flags.MoveBlocked;
                        }
                    }

                    if (is_move_blocked) {
                        continue;
                    }
                }

                grid_at(next_hex) = numindex;
                coords.emplace_back(next_hex);

                if (cut >= 0 && GeometryHelper::CheckDist(next_hex, target_hex, cut)) {
                    target_hex = next_hex;
                    return FindPathResult();
                }

                if (cut < 0 && next_hex == target_hex) {
                    find_ok = true;
                    break;
                }
            }
        }
    }
    if (cut >= 0) {
        return std::nullopt;
    }

    int x1 = coords.back().x;
    int y1 = coords.back().y;

    vector<uint8> raw_steps;
    raw_steps.resize(numindex - 1);

    float base_angle = GeometryHelper::GetDirAngle(target_hex, start_hex);

    // From end
    while (numindex > 1) {
        numindex--;

        int best_step_dir = -1;
        float best_step_angle_diff = 0.0f;

        const auto check_hex = [&best_step_dir, &best_step_angle_diff, numindex, &grid_at, start_hex, base_angle, this](uint8 dir, ipos raw_step_hex) {
            if (_mapSize.IsValidPos(raw_step_hex)) {
                const auto step_hex = _mapSize.FromRawPos(raw_step_hex);

                if (grid_at(step_hex) == numindex) {
                    const float angle = GeometryHelper::GetDirAngle(step_hex, start_hex);
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
            }
        };

        if ((x1 % 2) != 0) {
            check_hex(3, ipos {x1 - 1, y1 - 1});
            check_hex(2, ipos {x1, y1 - 1});
            check_hex(5, ipos {x1, y1 + 1});
            check_hex(0, ipos {x1 + 1, y1});
            check_hex(4, ipos {x1 - 1, y1});
            check_hex(1, ipos {x1 + 1, y1 - 1});

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
            check_hex(3, ipos {x1 - 1, y1});
            check_hex(2, ipos {x1, y1 - 1});
            check_hex(5, ipos {x1, y1 + 1});
            check_hex(0, ipos {x1 + 1, y1 + 1});
            check_hex(4, ipos {x1 - 1, y1 + 1});
            check_hex(1, ipos {x1 + 1, y1});

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
        auto trace_hex = start_hex;

        while (true) {
            auto trace_target_hex = target_hex;

            for (auto i = static_cast<int>(raw_steps.size()) - 1; i >= 0; i--) {
                LineTracer tracer(trace_hex, trace_target_hex, _mapSize, 0.0f);
                auto next_hex = trace_hex;
                vector<uint8> direct_steps;
                bool failed = false;

                while (true) {
                    uint8 dir = tracer.GetNextHex(next_hex);
                    direct_steps.emplace_back(dir);

                    if (next_hex == trace_target_hex) {
                        break;
                    }

                    if (grid_at(next_hex) <= 0) {
                        failed = true;
                        break;
                    }
                }

                if (failed) {
                    FO_RUNTIME_ASSERT(i > 0);
                    GeometryHelper::MoveHexByDir(trace_target_hex, GeometryHelper::ReverseDir(raw_steps[i]), _mapSize);
                    continue;
                }

                for (const auto& ds : direct_steps) {
                    result.DirSteps.emplace_back(ds);
                }

                result.ControlSteps.emplace_back(static_cast<uint16>(result.DirSteps.size()));

                trace_hex = trace_target_hex;
                break;
            }

            if (trace_target_hex == target_hex) {
                break;
            }
        }
    }
    else {
        for (size_t i = 0; i < raw_steps.size(); i++) {
            const auto cur_dir = raw_steps[i];
            result.DirSteps.emplace_back(cur_dir);

            for (size_t j = i + 1; j < raw_steps.size(); j++) {
                if (raw_steps[j] == cur_dir) {
                    result.DirSteps.emplace_back(cur_dir);
                    i++;
                }
                else {
                    break;
                }
            }

            result.ControlSteps.emplace_back(static_cast<uint16>(result.DirSteps.size()));
        }
    }

    FO_RUNTIME_ASSERT(!result.DirSteps.empty());
    FO_RUNTIME_ASSERT(!result.ControlSteps.empty());

    return {result};
}

bool MapView::CutPath(CritterHexView* cr, mpos start_hex, mpos& target_hex, int cut)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!cr || cr->GetMap() == this);

    return !!FindPath(cr, start_hex, target_hex, cut);
}

bool MapView::TraceMoveWay(mpos& start_hex, ipos16& hex_offset, vector<uint8>& dir_steps, int quad_dir) const
{
    FO_STACK_TRACE_ENTRY();

    hex_offset = {};

    const auto try_move = [this, &start_hex, &dir_steps](uint8 dir) {
        auto check_hex = start_hex;

        if (!GeometryHelper::MoveHexByDir(check_hex, dir, _mapSize)) {
            return false;
        }

        const auto& field = _hexField->GetCellForReading(check_hex);

        if (field.Flags.MoveBlocked) {
            return false;
        }

        start_hex = check_hex;
        dir_steps.emplace_back(dir);
        return true;
    };

    const auto try_move2 = [&try_move, &hex_offset](uint8 dir1, uint8 dir2) {
        if (try_move(dir1)) {
            if (!try_move(dir2)) {
                hex_offset.x = dir1 == 0 ? -16 : 16;
                return false;
            }
            return true;
        }

        if (try_move(dir2)) {
            if (!try_move(dir1)) {
                hex_offset.x = dir2 == 5 ? 16 : -16;
                return false;
            }
            return true;
        }

        return false;
    };

    constexpr auto some_big_path_len = 200;

    for (auto i = 0, j = (quad_dir == 3 || quad_dir == 7 ? some_big_path_len / 2 : some_big_path_len); i < j; i++) {
        if ((quad_dir == 0 && !try_move(start_hex.x % 2 == 0 ? 0 : 1)) || //
            (quad_dir == 1 && !try_move(1)) || //
            (quad_dir == 2 && !try_move(2)) || //
            (quad_dir == 3 && !try_move2(3, 2)) || //
            (quad_dir == 4 && !try_move(start_hex.x % 2 == 0 ? 4 : 3)) || //
            (quad_dir == 5 && !try_move(4)) || //
            (quad_dir == 6 && !try_move(5)) || //
            (quad_dir == 7 && !try_move2(0, 5))) {
            return i > 0;
        }
    }

    return true;
}

void MapView::TraceBullet(mpos start_hex, mpos target_hex, uint dist, float angle, vector<CritterHexView*>* critters, CritterFindType find_type, mpos* pre_block_hex, mpos* block_hex, vector<mpos>* hex_steps, bool check_shoot_blocks)
{
    FO_STACK_TRACE_ENTRY();

    if (_isShowTrack) {
        ClearHexTrack();
    }

    const auto check_dist = dist != 0 ? dist : GeometryHelper::DistGame(start_hex, target_hex);
    auto next_hex = start_hex;
    auto prev_hex = next_hex;

    LineTracer line_tracer(start_hex, target_hex, _mapSize, angle);

    for (uint i = 0; i < check_dist; i++) {
        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            line_tracer.GetNextHex(next_hex);
        }
        else {
            line_tracer.GetNextSquare(next_hex);
        }

        if (_isShowTrack) {
            GetHexTrack(next_hex) = static_cast<char>(next_hex == target_hex ? 1 : 2);
        }

        if (check_shoot_blocks && _hexField->GetCellForReading(next_hex).Flags.ShootBlocked) {
            break;
        }

        if (hex_steps != nullptr) {
            hex_steps->emplace_back(next_hex);
        }

        if (critters != nullptr) {
            auto hex_critters = GetCritters(next_hex, find_type);
            critters->insert(critters->end(), hex_critters.begin(), hex_critters.end());
        }

        prev_hex = next_hex;
    }

    if (pre_block_hex != nullptr) {
        *pre_block_hex = prev_hex;
    }
    if (block_hex != nullptr) {
        *block_hex = next_hex;
    }
}

void MapView::FindSetCenter(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_viewField.empty());

    RebuildMap(ipos {hex.x, hex.y});

    if (_mapperMode) {
        return;
    }

    mpos corrected_hex = hex;

    const auto find_set_center_dir = [this, &corrected_hex](const array<int, 2>& dirs, size_t steps) {
        auto moved_hex = corrected_hex;
        const auto dirs_index = dirs[1] == -1 ? 1 : 2;

        size_t i = 0;

        for (; i < steps; i++) {
            if (!GeometryHelper::MoveHexByDir(moved_hex, static_cast<uint8>(dirs[i % dirs_index]), _mapSize)) {
                break;
            }
            if (_hexField->GetCellForReading(moved_hex).Flags.ScrollBlock) {
                break;
            }
        }

        for (; i < steps; i++) {
            GeometryHelper::MoveHexByDir(corrected_hex, GeometryHelper::ReverseDir(static_cast<uint8>(dirs[i % dirs_index])), _mapSize);
        }
    };

    const auto view_size = GetViewSize();
    const auto iw = view_size.width / 2 + 2;
    const auto ih = view_size.height / 2 + 2;

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

    RebuildMap(ipos {corrected_hex.x, corrected_hex.y});
}

void MapView::SetShootBorders(bool enabled, uint dist)
{
    FO_STACK_TRACE_ENTRY();

    if (_drawShootBorders != enabled) {
        _drawShootBorders = enabled;
        _shootBordersDist = dist;
        RebuildFog();
    }
}

auto MapView::GetDrawList() noexcept -> MapSpriteList&
{
    FO_NO_STACK_TRACE_ENTRY();

    return _mapSprites;
}

void MapView::OnScreenSizeChanged()
{
    FO_STACK_TRACE_ENTRY();

    if (_viewField.empty()) {
        return;
    }

    _fogForceRerender = true;

    ResizeView();
    RefreshMap();
}

void MapView::AddFastPid(hstring pid)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    _fastPids.emplace(pid);
}

auto MapView::IsFastPid(hstring pid) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    return _fastPids.count(pid) != 0;
}

void MapView::ClearFastPids()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    _fastPids.clear();
}

void MapView::AddIgnorePid(hstring pid)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);
    _ignorePids.emplace(pid);
}

void MapView::SwitchIgnorePid(hstring pid)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    if (_ignorePids.count(pid) != 0) {
        _ignorePids.erase(pid);
    }
    else {
        _ignorePids.emplace(pid);
    }
}

auto MapView::IsIgnorePid(hstring pid) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    return _ignorePids.count(pid) != 0;
}

void MapView::ClearIgnorePids()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    _ignorePids.clear();
}

auto MapView::GetHexesRect(mpos from_hex, mpos to_hex) const -> vector<mpos>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    vector<mpos> hexes;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        auto [x, y] = _engine->Geometry.GetHexInterval(from_hex, to_hex);
        x = -x;

        const auto dx = x / _engine->Settings.MapHexWidth;
        const auto dy = y / _engine->Settings.MapHexLineHeight;
        const auto adx = std::abs(dx);
        const auto ady = std::abs(dy);

        int hx;
        int hy;

        for (auto j = 1; j <= ady; j++) {
            if (dy >= 0) {
                hx = from_hex.x + j / 2 + ((j % 2) != 0 ? 1 : 0);
                hy = from_hex.y + (j - (hx - from_hex.x - ((from_hex.x % 2) != 0 ? 1 : 0)) / 2);
            }
            else {
                hx = from_hex.x - j / 2 - ((j % 2) != 0 ? 1 : 0);
                hy = from_hex.y - (j - (from_hex.x - hx - ((from_hex.x % 2) != 0 ? 0 : 1)) / 2);
            }

            for (auto i = 0; i <= adx; i++) {
                if (_mapSize.IsValidPos(ipos {hx, hy})) {
                    hexes.emplace_back(static_cast<uint16>(hx), static_cast<uint16>(hy));
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
        auto [rw, rh] = _engine->Geometry.GetHexInterval(from_hex, to_hex);
        if (rw == 0) {
            rw = 1;
        }
        if (rh == 0) {
            rh = 1;
        }

        const auto hw = std::abs(rw / (_engine->Settings.MapHexWidth / 2)) + ((rw % (_engine->Settings.MapHexWidth / 2)) != 0 ? 1 : 0) + (std::abs(rw) >= _engine->Settings.MapHexWidth / 2 ? 1 : 0); // Hexes width
        const auto hh = std::abs(rh / _engine->Settings.MapHexLineHeight) + ((rh % _engine->Settings.MapHexLineHeight) != 0 ? 1 : 0) + (std::abs(rh) >= _engine->Settings.MapHexLineHeight ? 1 : 0); // Hexes height
        auto shx = static_cast<int>(from_hex.x);
        auto shy = static_cast<int>(from_hex.y);

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
                if (_mapSize.IsValidPos(ipos {hx, hy})) {
                    hexes.emplace_back(_mapSize.FromRawPos(ipos {hx, hy}));
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
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    for (uint16 hx = 0; hx < _mapSize.width; hx++) {
        for (uint16 hy = 0; hy < _mapSize.height; hy++) {
            const auto& field = _hexField->GetCellForReading({hx, hy});
            auto& track = GetHexTrack({hx, hy});

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

auto MapView::GenTempEntityId() -> ident_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    auto next_id = ident_t {(GetWorkEntityId().underlying_value() + 1)};

    while (_itemsMap.count(next_id) != 0 || _crittersMap.count(next_id) != 0) {
        next_id = ident_t {next_id.underlying_value() + 1};
    }

    SetWorkEntityId(next_id);

    return next_id;
}

auto MapView::ValidateForSave() const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> errors;

    unordered_set<const CritterHexView*> cr_reported;

    for (const auto& cr : _critters) {
        for (const auto& cr2 : _critters) {
            if (cr != cr2 && cr->GetHex() == cr2->GetHex() && cr_reported.count(cr.get()) == 0 && cr_reported.count(cr2.get()) == 0) {
                errors.emplace_back(strex("Critters have same hex coords at {}", cr->GetHex()));
                cr_reported.emplace(cr.get());
                cr_reported.emplace(cr2.get());
            }
        }
    }

    return errors;
}

auto MapView::SaveToText() const -> string
{
    FO_STACK_TRACE_ENTRY();

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
    for (const auto& cr : _critters) {
        fill_critter(cr.get());

        for (const auto& inv_item : cr->GetInvItems()) {
            fill_item(inv_item.get());
        }
    }

    // Items
    for (const auto& item : _allItems) {
        fill_item(item.get());

        for (const auto& inner_item : item->GetInnerItems()) {
            fill_item(inner_item.get());
        }
    }

    return fomap;
}

FO_END_NAMESPACE();
