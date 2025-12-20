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
#include "LineTracer.h"
#include "MapLoader.h"

FO_BEGIN_NAMESPACE();

static constexpr int32 MAX_LIGHT_INTEN = 10000;
static constexpr int32 MAX_LIGHT_HEX = 200;
static constexpr int32 MAX_LIGHT_ALPHA = 255;

void SpritePattern::Finish()
{
    FO_STACK_TRACE_ENTRY();

    if (!Finished) {
        Sprites.reset();
        Finished = true;
    }
}

MapView::MapView(FOClient* engine, ident_t id, const ProtoMap* proto, const Properties* props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    MapProperties(GetInitRef())
{
    FO_STACK_TRACE_ENTRY();

    _name = strex("{}_{}", proto->GetName(), id);

    _maxScroll = {_engine->Settings.MapHexWidth, _engine->Settings.MapHexLineHeight * 2};
    _screenSize = {_engine->Settings.ScreenWidth, _engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight};
    _viewSize = fsize32(_screenSize);

    SetSpritesZoom(1.0f);
    SetSpritesZoomTarget(1.0f);
    RefreshMinZoom();

    if (!_engine->Settings.MapDirectDraw) {
        _rtMap = _engine->SprMngr.GetRtMngr().CreateRenderTarget(false, RenderTarget::SizeKindType::Map, {}, true);
        _rtMap->SetCustomDrawEffect(_engine->EffectMngr.Effects.FlushMap.get());
    }

    if (!_engine->Settings.DisableLighting || !_engine->Settings.DisableFog) {
        const auto tex_size = _engine->Settings.MapDirectDraw ? RenderTarget::SizeKindType::Screen : RenderTarget::SizeKindType::Map;
        _rtLight = _engine->SprMngr.GetRtMngr().CreateRenderTarget(false, tex_size, {}, true);
    }

    _picHex[0] = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "hex1.png", AtlasType::MapSprites);
    _picHex[1] = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "hex2.png", AtlasType::MapSprites);
    _picHex[2] = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "hex3.png", AtlasType::MapSprites);
    _picTrack1 = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "track1.png", AtlasType::MapSprites);
    _picTrack2 = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "track2.png", AtlasType::MapSprites);
    _picHexMask = _engine->SprMngr.LoadSprite(_engine->Settings.MapDataPrefix + "hex_mask.png", AtlasType::MapSprites);

    if (_picHexMask) {
        const auto* atlas_spr = dynamic_cast<const AtlasSprite*>(_picHexMask.get());
        FO_RUNTIME_ASSERT(atlas_spr);
        const auto mask_x = iround<int32>(numeric_cast<float32>(atlas_spr->GetAtlas()->GetTexture()->Size.width) * atlas_spr->GetAtlasRect().x);
        const auto mask_y = iround<int32>(numeric_cast<float32>(atlas_spr->GetAtlas()->GetTexture()->Size.height) * atlas_spr->GetAtlasRect().y);
        _picHexMaskData = atlas_spr->GetAtlas()->GetTexture()->GetTextureRegion({mask_x, mask_y}, atlas_spr->GetSize());
    }

    _mapSize = GetSize();

    _hexLight.resize(_mapSize.square() * 3);
    _hexField = SafeAlloc::MakeUnique<StaticTwoDimensionalGrid<Field, mpos, msize>>(_mapSize);

    _lightPoints.resize(1);
    _lightSoftPoints.resize(1);

    _screenRawHex = ConvertToScreenRawHex(ipos32(GetWorkHex()));
    InitView();
    RecacheScrollBlocks();

    _eventUnsubscriber += _engine->SprMngr.GetWindow()->OnScreenSizeChanged += [this] { OnScreenSizeChanged(); };
}

MapView::~MapView()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& cr : _critters) {
        cr->DestroySelf();
    }
    for (auto& item : _items) {
        item->DestroySelf();
    }

    _engine->SprMngr.GetRtMngr().DeleteRenderTarget(_rtMap.get());
    _engine->SprMngr.GetRtMngr().DeleteRenderTarget(_rtLight.get());
}

void MapView::OnDestroySelf()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& cr : _critters) {
        cr->DestroySelf();
    }
    for (auto& item : _items) {
        item->DestroySelf();
    }

    for (auto& pattern : _spritePatterns) {
        pattern->Finish();
    }

    _mapSprites.InvalidateAll();
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
    _items.clear();
    _staticItems.clear();
    _dynamicItems.clear();
    _processingItems.clear();
    _itemsMap.clear();
    _spritePatterns.clear();
}

void MapView::EnableMapperMode()
{
    FO_STACK_TRACE_ENTRY();

    _mapperMode = true;
    _isShowTrack = true;

    _hexTrack.resize(_mapSize.square());
}

void MapView::LoadFromFile(string_view map_name, const string& str)
{
    FO_RUNTIME_ASSERT(_mapperMode);

    _mapLoading = true;
    auto max_id = _workEntityId.underlying_value();

    MapLoader::Load(
        map_name, str, _engine->ProtoMngr, _engine->Hashes,
        [this, &max_id](ident_t id, const ProtoCritter* proto, const map<string, string>& kv) {
            FO_RUNTIME_ASSERT(id);
            FO_RUNTIME_ASSERT(_crittersMap.count(id) == 0);

            max_id = std::max(max_id, id.underlying_value());

            auto props = proto->GetProperties().Copy();
            props.ApplyFromText(kv);

            auto cr = SafeAlloc::MakeRefCounted<CritterHexView>(this, id, proto, &props);

            if (const auto hex = cr->GetHex(); !_mapSize.is_valid_pos(hex)) {
                cr->SetHex(_mapSize.clamp_pos(hex));
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
                if (const auto hex = item->GetHex(); !_mapSize.is_valid_pos(hex)) {
                    item->SetHex(_mapSize.clamp_pos(hex));
                }

                AddItemInternal(item.get());
            }
            else if (item->GetOwnership() == ItemOwnership::CritterInventory) {
                auto* cr = GetCritter(item->GetCritterId());

                if (cr == nullptr) {
                    throw GenericException("Critter not found", item->GetCritterId());
                }

                cr->AddRawInvItem(item.get());
            }
            else if (item->GetOwnership() == ItemOwnership::ItemContainer) {
                auto* cont = GetItem(item->GetContainerId());

                if (cont == nullptr) {
                    throw GenericException("Container not found", item->GetContainerId());
                }

                cont->AddRawInnerItem(item.get());
            }
            else {
                FO_UNREACHABLE_PLACE();
            }
        });

    _workEntityId = ident_t {max_id};
    _mapLoading = false;

    RebuildMapNow();
}

void MapView::LoadStaticData()
{
    FO_STACK_TRACE_ENTRY();

    const auto file = _engine->Resources.ReadFile(strex("{}.fomap-bin-client", GetProtoId()));

    if (!file) {
        throw MapViewLoadException("Map file not found", GetProtoId());
    }

    auto reader = DataReader({file.GetBuf(), file.GetSize()});

    // Hashes
    {
        const auto hashes_count = reader.Read<uint32>();

        string str;

        for (uint32 i = 0; i < hashes_count; i++) {
            const auto str_len = reader.Read<uint32>();
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

        const auto items_count = reader.Read<uint32>();

        _items.reserve(items_count);
        _staticItems.reserve(items_count);
        _processingItems.reserve(256);

        vector<uint8> props_data;

        for (uint32 i = 0; i < items_count; i++) {
            const auto static_id = ident_t {reader.Read<ident_t::underlying_type>()};
            const auto item_pid_hash = reader.Read<hstring::hash_t>();
            const auto item_pid = _engine->Hashes.ResolveHash(item_pid_hash);
            const auto* item_proto = _engine->ProtoMngr.GetProtoItem(item_pid);

            auto item_props = Properties(item_proto->GetProperties().GetRegistrator());
            const auto props_data_size = reader.Read<uint32>();
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
    const auto mark_roof_num = [this](ipos32 raw_hex, int32 num) {
        std::stack<ipos32, vector<ipos32>> next_raw_hexes;
        next_raw_hexes.push(raw_hex);

        while (!next_raw_hexes.empty()) {
            const auto next_raw_hex = next_raw_hexes.top();
            next_raw_hexes.pop();

            if (!_mapSize.is_valid_pos(next_raw_hex)) {
                continue;
            }

            const auto next_hex = _mapSize.from_raw_pos(next_raw_hex);
            const auto& field = _hexField->GetCellForReading(next_hex);

            if (field.RoofNum != 0 || field.HasRoof) {
                continue;
            }

            _hexField->GetCellForWriting(next_hex).RoofNum = num;

            next_raw_hexes.emplace(next_raw_hex.x + _engine->Settings.MapTileStep, next_raw_hex.y);
            next_raw_hexes.emplace(next_raw_hex.x - _engine->Settings.MapTileStep, next_raw_hex.y);
            next_raw_hexes.emplace(next_raw_hex.x, next_raw_hex.y + _engine->Settings.MapTileStep);
            next_raw_hexes.emplace(next_raw_hex.x, next_raw_hex.y - _engine->Settings.MapTileStep);
        }
    };

    int32 roof_num = 1;

    for (const auto hx : iterate_range(_mapSize.width)) {
        for (const auto hy : iterate_range(_mapSize.height)) {
            const auto& field = _hexField->GetCellForReading(mpos(hx, hy));

            if (field.RoofNum == 0 && field.HasRoof) {
                const auto corrected_hx = hx - hx % _engine->Settings.MapTileStep;
                const auto corrected_hy = hy - hy % _engine->Settings.MapTileStep;
                mark_roof_num(ipos32 {corrected_hx, corrected_hy}, roof_num);
                roof_num++;
            }
        }
    }

    RebuildMapNow();
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
            const auto day_color_time = IsNonEmptyDayColorTime() ? GetDayColorTime() : vector<int32> {300, 600, 1140, 1380};
            const auto day_color = IsNonEmptyDayColor() ? GetDayColor() : vector<uint8> {18, 128, 103, 51, 18, 128, 95, 40, 53, 128, 86, 29};

            _mapDayColor = GetColorDay(day_color_time, day_color, map_day_time, &_mapDayLightCapacity);
            _globalDayColor = GetColorDay(day_color_time, day_color, global_day_time, &_globalDayLightCapacity);

            if (_mapDayColor != _prevMapDayColor) {
                _prevMapDayColor = _mapDayColor;
                _needReapplyLights = true;
            }

            if (_globalDayColor != _prevGlobalDayColor) {
                _prevGlobalDayColor = _globalDayColor;
                _needReapplyLights = _globalLights != 0;
            }
        }
    }

    // Critters
    {
        vector<CritterHexView*> critter_to_delete;

        for (auto& cr : _critters) {
            cr->Process();

            if (cr->IsFinished()) {
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

        for (auto& item : _processingItems) {
            if (item->IsNeedProcess()) {
                item->Process();
            }

            if (item->IsFinished()) {
                item_to_delete.emplace_back(item.get());
            }
        }

        for (auto* item : item_to_delete) {
            DestroyItem(item);
        }
    }

    // Scroll and zoom
    {
        const auto fixed_dt = timespan(std::chrono::milliseconds(_engine->Settings.ScrollFixedDt));
        _scrollDtAccum += _engine->GameTime.GetFrameDeltaTime();

        if (_scrollDtAccum >= fixed_dt) {
            _scrollDtAccum = std::min(_scrollDtAccum, timespan(fixed_dt.value() * 10));
            _scrollDtAccum -= fixed_dt;
            ProcessZoom(fixed_dt.to_ms<float32>());
            ProcessScroll(fixed_dt.to_ms<float32>());
        }
    }
}

auto MapView::GetViewSize() const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    const auto& settings = _engine->Settings;
    const auto zoom = GetSpritesZoom();
    const auto screen_hexes_width = _screenSize.width / settings.MapHexWidth + ((_screenSize.width % settings.MapHexWidth) != 0 ? 1 : 0);
    const auto screen_hexes_height = _screenSize.height / settings.MapHexLineHeight + ((_screenSize.height % settings.MapHexLineHeight) != 0 ? 1 : 0);
    const auto view_hexes_width = is_float_equal(zoom, 1.0f) ? screen_hexes_width : iround<int32>(std::ceil(numeric_cast<float32>(screen_hexes_width) / zoom));
    const auto view_hexes_height = is_float_equal(zoom, 1.0f) ? screen_hexes_height : iround<int32>(std::ceil(numeric_cast<float32>(screen_hexes_height) / zoom));

    return {view_hexes_width, view_hexes_height};
}

void MapView::AddItemToField(ItemHexView* item)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = item->GetHex();
    auto& field = _hexField->GetCellForWriting(hex);

    vec_add_unique_value(field.OriginItems, item);
    vec_add_unique_value(field.Items, item);
    RecacheHexFlags(field);

    if (item->IsNonEmptyMultihexLines() || item->IsNonEmptyMultihexMesh()) {
        vector<mpos> multihex_entries;
        const auto multihex_lines = item->GetMultihexLines();
        const auto multihex_mesh = item->GetMultihexMesh();
        multihex_entries.reserve((1 + multihex_mesh.size()) * (1 + multihex_lines.size() / 2));

        GeometryHelper::ForEachMultihexLines(multihex_lines, hex, _mapSize, [&](mpos multihex) {
            auto& multihex_field = _hexField->GetCellForWriting(multihex);

            if (vec_safe_add_unique_value(multihex_field.Items, item)) {
                vec_add_unique_value(multihex_field.MultihexItems, pair(item, item->GetDrawMultihexLines()));
                RecacheHexFlags(multihex_field);
                multihex_entries.emplace_back(multihex);
            }
        });

        for (const auto multihex : multihex_mesh) {
            if (multihex != hex && _mapSize.is_valid_pos(multihex)) {
                auto& multihex_field = _hexField->GetCellForWriting(multihex);

                if (vec_safe_add_unique_value(multihex_field.Items, item)) {
                    vec_add_unique_value(multihex_field.MultihexItems, pair(item, item->GetDrawMultihexMesh()));
                    RecacheHexFlags(multihex_field);
                    multihex_entries.emplace_back(multihex);
                }

                if (item->IsNonEmptyMultihexLines()) {
                    GeometryHelper::ForEachMultihexLines(multihex_lines, multihex, _mapSize, [&](mpos multihex2) {
                        auto& multihex_field2 = _hexField->GetCellForWriting(multihex2);

                        if (vec_safe_add_unique_value(multihex_field2.Items, item)) {
                            vec_add_unique_value(multihex_field2.MultihexItems, pair(item, item->GetDrawMultihexLines()));
                            RecacheHexFlags(multihex_field2);
                            multihex_entries.emplace_back(multihex2);
                        }
                    });
                }
            }
        }

        if (!multihex_entries.empty()) {
            item->SetMultihexEntries(std::move(multihex_entries));
        }
    }

    if (!item->GetLightThru()) {
        UpdateHexLightSources(hex);
    }

    UpdateItemLightSource(item);

    if (!MeasureMapBorders(item->GetSprite(), item->GetSpriteOffset()) && !_mapLoading) {
        if (IsHexToDraw(hex)) {
            DrawHexItem(item, field, hex, false);
        }

        if (item->HasMultihexEntries() && (item->GetDrawMultihexLines() || item->GetDrawMultihexMesh())) {
            for (const auto multihex : item->GetMultihexEntries()) {
                if (auto& multihex_field = _hexField->GetCellForWriting(multihex); multihex_field.IsView) {
                    for (auto&& [multihex_item, drawable] : multihex_field.MultihexItems) {
                        if (drawable && multihex_item == item) {
                            DrawHexItem(item, multihex_field, multihex, true);
                        }
                    }
                }
            }
        }
    }
}

void MapView::RemoveItemFromField(ItemHexView* item)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = item->GetHex();
    auto& field = _hexField->GetCellForWriting(hex);

    vec_remove_unique_value(field.OriginItems, item);
    vec_remove_unique_value(field.Items, item);
    RecacheHexFlags(field);

    if (item->HasMultihexEntries()) {
        for (const auto multihex : item->GetMultihexEntries()) {
            auto& multihex_field = _hexField->GetCellForWriting(multihex);
            vec_remove_unique_value(multihex_field.Items, item);
            vec_remove_unique_value_if(multihex_field.MultihexItems, [item](auto&& i) { return i.first == item; });
            RecacheHexFlags(multihex_field);
        }

        item->SetMultihexEntries({});
    }

    if (!item->GetLightThru()) {
        UpdateHexLightSources(hex);
    }

    FinishLightSource(item->GetId());
    item->InvalidateSprite();
}

void MapView::DrawHexItem(ItemHexView* item, Field& field, mpos hex, bool extra_draw)
{
    FO_STACK_TRACE_ENTRY();

    if (_mapperMode) {
        const auto is_fast = _fastPids.count(item->GetProtoId()) != 0;

        if (!_engine->Settings.ShowScen && !is_fast && item->GetIsScenery()) {
            return;
        }
        if (!_engine->Settings.ShowItem && !is_fast && !item->GetIsScenery() && !item->GetIsWall()) {
            return;
        }
        if (!_engine->Settings.ShowWall && !is_fast && item->GetIsWall()) {
            return;
        }
        if (!_engine->Settings.ShowTile && item->GetIsTile() && !item->GetIsRoofTile()) {
            return;
        }
        if (!_engine->Settings.ShowRoof && item->GetIsTile() && item->GetIsRoofTile()) {
            return;
        }
        if (!_engine->Settings.ShowFast && is_fast) {
            return;
        }
        if (_ignorePids.count(item->GetProtoId()) != 0) {
            return;
        }
    }
    else {
        if (item->GetAlwaysHideSprite()) {
            return;
        }
    }

    const bool is_roof = item->GetIsTile() && item->GetIsRoofTile();

    if (is_roof && _hiddenRoofNum != 0 && _hiddenRoofNum == field.RoofNum) {
        return;
    }

    DrawOrderType draw_order;

    if (item->GetIsTile()) {
        if (item->GetIsRoofTile()) {
            draw_order = static_cast<DrawOrderType>(static_cast<int32>(DrawOrderType::Roof) + item->GetTileLayer());
        }
        else {
            draw_order = static_cast<DrawOrderType>(static_cast<int32>(DrawOrderType::Tile) + item->GetTileLayer());
        }
    }
    else if (item->GetDrawFlatten()) {
        draw_order = !item->GetIsScenery() && !item->GetIsWall() ? DrawOrderType::FlatItem : DrawOrderType::FlatScenery;
    }
    else {
        draw_order = !item->GetIsScenery() && !item->GetIsWall() ? DrawOrderType::Item : DrawOrderType::Scenery;
    }

    const auto draw_hex = _mapSize.clamp_pos(hex.x, hex.y + item->GetDrawOrderOffsetHexY());
    MapSprite* mspr;

    if (!extra_draw) {
        mspr = item->AddSprite(_mapSprites, draw_order, draw_hex, &field.Offset);
    }
    else {
        mspr = item->AddExtraSprite(_mapSprites, draw_order, draw_hex, &field.Offset);
    }

    AddSpriteToChain(field, mspr);

    if (is_roof) {
        mspr->SetEggAppearence(EggAppearenceType::Always);
    }
}

auto MapView::AddReceivedItem(ident_t id, hstring pid, mpos hex, const vector<vector<uint8>>& data) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(id);
    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, id, proto);

    item->RestoreData(data);
    item->SetStatic(false);
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
    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, GenTempEntityId(), proto, props);

    item->SetHex(hex);

    return AddItemInternal(item.get());
}

auto MapView::AddMapperTile(hstring pid, mpos hex, uint8 layer, bool is_roof) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);
    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, GenTempEntityId(), proto);

    item->SetHex(hex);
    item->SetIsTile(true);
    item->SetIsRoofTile(is_roof);
    item->SetTileLayer(layer);

    return AddItemInternal(item.get());
}

auto MapView::AddLocalItem(hstring pid, mpos hex) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, ident_t {}, proto);

    item->SetStatic(false);
    item->SetHex(hex);

    return AddItemInternal(item.get());
}

auto MapView::AddItemInternal(ItemHexView* item) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(item->GetHex()));
    FO_RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::MapHex);

    if (item->GetId()) {
        if (auto* prev_item = GetItem(item->GetId()); prev_item != nullptr) {
            item->RestoreFading(prev_item->StoreFading());
            DestroyItem(prev_item);
        }
    }

    item->SetMapId(GetId());
    item->Init();

    _items.emplace_back(item);

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

    if (item->GetId()) {
        _itemsMap.emplace(item->GetId(), item);
    }

    AddItemToField(item);
    return item;
}

void MapView::RefreshItem(ItemHexView* item, bool deferred)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item->GetMap() == this);

    if (deferred) {
        _deferredRefreshItems.emplace(item);
    }
    else {
        RemoveItemFromField(item);
        AddItemToField(item);
    }
}

void MapView::DefferedRefreshItems()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& item : to_vector(_deferredRefreshItems)) {
        if (!item->IsDestroyed()) {
            RefreshItem(item.get(), false);
        }
    }

    _deferredRefreshItems.clear();
}

void MapView::MoveItem(ItemHexView* item, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item->GetMap() == this);
    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));

    RemoveItemFromField(item);
    item->SetHex(hex);
    AddItemToField(item);
}

void MapView::DestroyItem(ItemHexView* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item->GetMap() == this);

    refcount_ptr item_ref_holder = item;

    vec_remove_unique_value(_items, item);

    if (item->GetStatic()) {
        vec_remove_unique_value(_staticItems, item);
    }
    else {
        vec_remove_unique_value(_dynamicItems, item);
    }

    vec_safe_remove_unique_value(_processingItems, item);

    if (item->GetId()) {
        const auto it = _itemsMap.find(item->GetId());
        FO_RUNTIME_ASSERT(it != _itemsMap.end());
        _itemsMap.erase(it);
    }

    RemoveItemFromField(item);
    CleanLightSourceOffsets(item->GetId());
    item->DestroySelf();
}

auto MapView::GetItem(ident_t id) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = _itemsMap.find(id); it != _itemsMap.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto MapView::GetItemOnHex(mpos hex) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return nullptr;
    }

    auto& field2 = _hexField->GetCellForWriting(hex);
    return field2.Items.front().get();
}

auto MapView::GetItemOnHex(mpos hex, hstring pid) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return nullptr;
    }

    auto& field2 = _hexField->GetCellForWriting(hex);

    for (auto& item : field2.Items) {
        if (item->GetProtoId() == pid) {
            return item.get();
        }
    }

    return nullptr;
}

auto MapView::GetItemsOnHex(mpos hex) -> span<raw_ptr<ItemHexView>>
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return {};
    }

    auto& field2 = _hexField->GetCellForWriting(hex);
    return field2.Items;
}

auto MapView::GetItemsOnHex(mpos hex) const -> span<const raw_ptr<ItemHexView>>
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);
    return field.Items;
}

auto MapView::GetHexContentSize(mpos hex) -> isize32
{
    FO_STACK_TRACE_ENTRY();

    optional<irect32> result;

    if (const auto& field = _hexField->GetCellForReading(hex); field.IsView) {
        if (!field.Critters.empty()) {
            for (const auto& cr : field.Critters) {
                if (cr->IsMapSpriteVisible()) {
                    const auto rect = cr->GetViewRect();
                    result = result.has_value() ? result->expanded(rect) : rect;
                }
            }
        }

        if (!field.Items.empty()) {
            for (const auto& item : field.Items) {
                if (item->IsMapSpriteVisible()) {
                    const auto* spr = item->GetSprite();

                    if (spr != nullptr) {
                        const auto x = field.Offset.x + _engine->Settings.MapHexWidth / 2 - spr->GetOffset().x;
                        const auto y = field.Offset.y + _engine->Settings.MapHexHeight / 2 - spr->GetOffset().y;
                        const auto rect = irect32(ipos32(x, y), spr->GetSize());
                        result = result.has_value() ? result->expanded(rect) : rect;
                    }
                }
            }
        }
    }

    return result.has_value() ? result.value().size() : isize32();
}

auto MapView::RunSpritePattern(string_view name, size_t count) -> SpritePattern*
{
    FO_STACK_TRACE_ENTRY();

    auto spr = _engine->SprMngr.LoadSprite(name, AtlasType::MapSprites);

    if (!spr) {
        return nullptr;
    }

    spr->Prewarm();
    spr->PlayDefault();

    auto pattern = SafeAlloc::MakeRefCounted<SpritePattern>();

    pattern->Sprites = SafeAlloc::MakeUnique<vector<shared_ptr<Sprite>>>();
    pattern->Sprites->emplace_back(std::move(spr));

    for (size_t i = 1; i < count; i++) {
        auto next_spr = _engine->SprMngr.LoadSprite(name, AtlasType::MapSprites);

        next_spr->Prewarm();
        next_spr->PlayDefault();

        pattern->Sprites->emplace_back(std::move(next_spr));
    }

    _spritePatterns.emplace_back(pattern);
    return pattern.get();
}

void MapView::RebuildMapNow()
{
    FO_STACK_TRACE_ENTRY();

    HideHexLines(0, _hVisible);
    FO_RUNTIME_ASSERT(!_mapSprites.HasActiveSprites());
    FO_RUNTIME_ASSERT(_visibleLightSources.empty());

    InitView();

    ShowHexLines(0, _hVisible);

    _rebuildMap = false;
    _engine->SprMngr.EggNotValid();
    _needRebuildLightPrimitives = true;
    _needReapplyLights = true;
    _engine->OnRenderMap.Fire();
}

void MapView::RebuildMapOffset(ipos32 axial_hex_offset)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_viewField.empty());
    FO_RUNTIME_ASSERT(std::abs(axial_hex_offset.y) % 2 == 0);

    const auto ox = axial_hex_offset.x;
    const auto oy = axial_hex_offset.y;

    // Hide opposite lines
    HideHexLines(-ox, -oy);

    // Shift view position
    uint8 shift_ox_dir;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        shift_ox_dir = ox > 0 ? 1 : 4;
    }
    else {
        shift_ox_dir = ox > 0 ? 1 : 5;
    }

    uint8 shift_oy_dir1;
    uint8 shift_oy_dir2;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        shift_oy_dir1 = oy > 0 ? 2 : 0;
        shift_oy_dir2 = oy > 0 ? 3 : 5;
    }
    else {
        shift_oy_dir1 = oy > 0 ? 2 : 0;
        shift_oy_dir2 = oy > 0 ? 4 : 6;
    }

    if (ox != 0) {
        for (int32 i = 0; i < std::abs(ox); i++) {
            GeometryHelper::MoveHexByDirUnsafe(_screenRawHex, shift_ox_dir);
        }
    }
    if (oy != 0) {
        for (int32 i = 0; i < std::abs(oy) / 2; i++) {
            GeometryHelper::MoveHexByDirUnsafe(_screenRawHex, shift_oy_dir1);
            GeometryHelper::MoveHexByDirUnsafe(_screenRawHex, shift_oy_dir2);
        }
    }

    for (auto& vf : _viewField) {
        for (int32 i = 0; i < std::abs(ox); i++) {
            GeometryHelper::MoveHexByDirUnsafe(vf.RawHex, shift_ox_dir);
        }
        for (int32 i = 0; i < std::abs(oy) / 2; i++) {
            GeometryHelper::MoveHexByDirUnsafe(vf.RawHex, shift_oy_dir1);
            GeometryHelper::MoveHexByDirUnsafe(vf.RawHex, shift_oy_dir2);
        }

        if (_mapSize.is_valid_pos(vf.RawHex)) {
            const auto hex = _mapSize.from_raw_pos(vf.RawHex);
            auto& field = _hexField->GetCellForWriting(hex);
            field.Offset = vf.Offset;
        }
    }

    // Show new lines
    ShowHexLines(ox, oy);

    // Critters text rect
    for (auto& cr : _critters) {
        cr->RefreshOffs();
    }

    _needRebuildLightPrimitives = true;
    _engine->OnRenderMap.Fire();
}

void MapView::ShowHexLines(int ox, int oy)
{
    FO_STACK_TRACE_ENTRY();

    // Show vertical line
    if (ox != 0) {
        const auto from_x = ox > 0 ? std::max(_wVisible - ox, 0) : 0;
        const auto to_x = ox > 0 ? _wVisible : std::min(-ox, _wVisible);
        FO_RUNTIME_ASSERT(from_x < to_x);

        for (auto x = from_x; x < to_x; x++) {
            for (auto y = 0; y < _hVisible; y++) {
                ShowHex(_viewField[y * _wVisible + x]);
            }
        }
    }

    // Show horizontal line
    if (oy != 0) {
        const auto from_y = oy > 0 ? std::max(_hVisible - oy, 0) : 0;
        const auto to_y = oy > 0 ? _hVisible : std::min(-oy, _hVisible);
        FO_RUNTIME_ASSERT(from_y < to_y);

        for (auto y = from_y; y < to_y; y++) {
            for (auto x = 0; x < _wVisible; x++) {
                ShowHex(_viewField[y * _wVisible + x]);
            }
        }
    }
}

void MapView::HideHexLines(int ox, int oy)
{
    FO_STACK_TRACE_ENTRY();

    // Hide vertical line
    if (ox != 0) {
        const auto from_x = ox > 0 ? std::max(_wVisible - ox, 0) : 0;
        const auto to_x = ox > 0 ? _wVisible : std::min(-ox, _wVisible);
        FO_RUNTIME_ASSERT(from_x < to_x);

        for (auto x = from_x; x < to_x; x++) {
            for (auto y = 0; y < _hVisible; y++) {
                HideHex(_viewField[y * _wVisible + x]);
            }
        }
    }

    // Hide horizontal line
    if (oy != 0) {
        const auto from_y = oy > 0 ? std::max(_hVisible - oy, 0) : 0;
        const auto to_y = oy > 0 ? _hVisible : std::min(-oy, _hVisible);
        FO_RUNTIME_ASSERT(from_y < to_y);

        for (auto y = from_y; y < to_y; y++) {
            for (auto x = 0; x < _wVisible; x++) {
                HideHex(_viewField[y * _wVisible + x]);
            }
        }
    }
}

void MapView::ShowHex(const ViewField& vf)
{
    FO_STACK_TRACE_ENTRY();

    if (!_mapSize.is_valid_pos(vf.RawHex)) {
        return;
    }

    const auto hex = _mapSize.from_raw_pos(vf.RawHex);

    if (IsHexToDraw(hex)) {
        return;
    }

    auto& field = _hexField->GetCellForWriting(hex);
    field.IsView = true;
    field.Offset = vf.Offset;

    // Lighting
    if (!field.LightSources.empty()) {
        for (auto& ls : field.LightSources | std::views::keys) {
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
        auto* mspr = _mapSprites.AddSprite(DrawOrderType::Track, hex, //
            {_engine->Settings.MapHexWidth / 2, (_engine->Settings.MapHexHeight / 2) + (spr ? spr->GetSize().height / 2 : 0)}, &field.Offset, //
            spr.get(), nullptr, nullptr, nullptr, nullptr, nullptr);
        AddSpriteToChain(field, mspr);
    }

    // Hex lines
    if (_isShowHex) {
        const auto& spr = _picHex[0];
        auto* mspr = _mapSprites.AddSprite(DrawOrderType::HexGrid, hex, //
            {spr ? spr->GetSize().width / 2 : 0, spr ? spr->GetSize().height : 0}, &field.Offset, //
            spr.get(), nullptr, nullptr, nullptr, nullptr, nullptr);
        AddSpriteToChain(field, mspr);
    }

    // Items on hex
    if (!field.OriginItems.empty()) {
        for (auto& item : field.OriginItems) {
            DrawHexItem(item.get(), field, hex, false);
        }
    }

    // Multihex items
    if (!field.MultihexItems.empty()) {
        for (auto&& [item, drawable] : field.MultihexItems) {
            if (drawable) {
                DrawHexItem(item.get(), field, hex, true);
            }
        }
    }

    // Critters
    if (!field.OriginCritters.empty()) {
        for (auto& cr : field.OriginCritters) {
            DrawHexCritter(cr.get(), field, hex);
        }
    }

    // Patterns
    if (!_spritePatterns.empty()) {
        for (auto it = _spritePatterns.begin(); it != _spritePatterns.end();) {
            const auto& pattern = *it;

            if (pattern->Finished) {
                it = _spritePatterns.erase(it);
                continue;
            }

            ++it;

            if ((hex.x % pattern->EveryHex.x) != 0) {
                continue;
            }
            if ((hex.y % pattern->EveryHex.y) != 0) {
                continue;
            }

            if (pattern->InteractWithRoof && _hiddenRoofNum != 0 && _hiddenRoofNum == field.RoofNum) {
                continue;
            }

            if (pattern->CheckTileProperty) {
                if (!field.GroundTile) {
                    continue;
                }
                if (field.GroundTile->GetValueAsInt(static_cast<int32>(pattern->TileProperty)) != pattern->ExpectedTilePropertyValue) {
                    continue;
                }
            }
            else {
                continue;
            }

            const auto* spr = pattern->Sprites->at((hex.y * (pattern->Sprites->size() / 5) + hex.x) % pattern->Sprites->size()).get();
            auto* mspr = _mapSprites.AddSprite(pattern->InteractWithRoof && field.RoofNum != 0 ? DrawOrderType::RoofParticles : DrawOrderType::Particles, hex, //
                {_engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2 + (pattern->InteractWithRoof && field.RoofNum != 0 ? _engine->Settings.MapRoofOffsY : 0)}, &field.Offset, //
                spr, nullptr, nullptr, nullptr, nullptr, nullptr);
            AddSpriteToChain(field, mspr);
        }
    }

    // Scroll block
    if (_mapperMode) {
        const irect32 scroll_area = GetScrollAxialArea();

        if (!scroll_area.is_zero()) {
            const ipos32 axial_hex = _engine->Geometry.GetHexAxialCoord(hex);

            if (axial_hex.x == scroll_area.x || axial_hex.y == scroll_area.y || axial_hex.x == scroll_area.x + scroll_area.width || axial_hex.y == scroll_area.y + scroll_area.height) {
                const auto& spr = _picTrack1;
                auto* mspr = _mapSprites.AddSprite(DrawOrderType::Last, hex, //
                    {_engine->Settings.MapHexWidth / 2, (_engine->Settings.MapHexHeight / 2) + (spr ? spr->GetSize().height / 2 : 0)}, &field.Offset, //
                    spr.get(), nullptr, nullptr, nullptr, nullptr, nullptr);
                AddSpriteToChain(field, mspr);
            }
        }
    }
}

void MapView::HideHex(const ViewField& vf)
{
    FO_STACK_TRACE_ENTRY();

    if (!_mapSize.is_valid_pos(vf.RawHex)) {
        return;
    }

    const auto hex = _mapSize.from_raw_pos(vf.RawHex);

    if (!IsHexToDraw(hex)) {
        return;
    }

    auto& field = _hexField->GetCellForWriting(hex);
    field.IsView = false;

    InvalidateSpriteChain(field);

    // Lighting
    if (!field.LightSources.empty()) {
        for (auto* ls : field.LightSources | std::views::keys) {
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
}

void MapView::ProcessLighting()
{
    FO_STACK_TRACE_ENTRY();

    if (_engine->Settings.DisableLighting) {
        return;
    }

    if (_needReapplyLights) {
        _needReapplyLights = false;

        for (auto&& [ls, count] : copy(_visibleLightSources)) {
            ApplyLightFan(ls);
        }
    }

    vector<LightSource*> reapply_sources;
    vector<LightSource*> remove_sources;

    for (auto& ls : _visibleLightSources | std::views::keys) {
        const auto prev_intensity = ls->CurIntensity;

        if (ls->CurIntensity != ls->TargetIntensity) {
            const auto elapsed_time = (_engine->GameTime.GetFrameTime() - ls->Time).div<float32>(std::chrono::milliseconds {200});
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

    if (_needRebuildLightPrimitives) {
        _needRebuildLightPrimitives = false;

        for (auto& points : _lightPoints) {
            points.clear();
        }
        for (auto& points : _lightSoftPoints) {
            points.clear();
        }

        size_t cur_points = 0;

        for (const auto* ls : _visibleLightSources | std::views::keys) {
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
}

void MapView::UpdateCritterLightSource(const CritterHexView* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr->GetMap() == this);

    if (_engine->Settings.DisableLighting) {
        return;
    }

    bool light_added = false;

    for (const auto& item : cr->GetInvItems()) {
        if (item->GetLightSource() && item->GetCritterSlot() != CritterItemSlot::Inventory) {
            UpdateLightSource(cr->GetId(), cr->GetHex(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity(), cr->GetSpriteOffsetPtr());
            light_added = true;
            break;
        }
    }

    // Default chosen light
    if (!light_added && cr->GetIsChosen()) {
        UpdateLightSource(cr->GetId(), cr->GetHex(), _engine->Settings.ChosenLightColor, _engine->Settings.ChosenLightDistance, _engine->Settings.ChosenLightFlags, _engine->Settings.ChosenLightIntensity, cr->GetSpriteOffsetPtr());
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

    if (_engine->Settings.DisableLighting) {
        return;
    }

    if (item->GetLightSource()) {
        UpdateLightSource(item->GetId(), item->GetHex(), item->GetLightColor(), item->GetLightDistance(), item->GetLightFlags(), item->GetLightIntensity(), item->GetSpriteOffsetPtr());
    }
    else {
        FinishLightSource(item->GetId());
    }
}

void MapView::UpdateHexLightSources(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    if (_engine->Settings.DisableLighting) {
        return;
    }

    const auto& field = _hexField->GetCellForReading(hex);

    for (auto&& ls_pair : copy(field.LightSources)) {
        ApplyLightFan(ls_pair.first);
    }
}

void MapView::UpdateLightSource(ident_t id, mpos hex, ucolor color, int32 distance, uint8 flags, int32 intensity, const ipos32* offset)
{
    FO_STACK_TRACE_ENTRY();

    LightSource* ls;

    const auto it = _lightSources.find(id);

    if (it == _lightSources.end()) {
        ls = _lightSources.emplace(id, SafeAlloc::MakeUnique<LightSource>(id, hex, color, distance, flags, intensity, offset)).first->second.get();
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

    ls->TargetIntensity = std::min(std::abs(ls->Intensity), 100);

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
    ls->MarkedHexes.reserve(GeometryHelper::HexesInRadius(distance));
    ls->FanHexes.clear();
    ls->FanHexes.reserve(numeric_cast<size_t>(distance) * GameSettings::MAP_DIR_COUNT);

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
    const auto center_alpha = numeric_cast<uint8>(MAX_LIGHT_ALPHA * ls->Capacity / 100 * intensity / MAX_LIGHT_INTEN);

    ls->CenterColor = ucolor {ls->Color, center_alpha};

    MarkLight(ls, center_hex, intensity);

    ipos32 raw_traced_hex = {center_hex.x, center_hex.y};
    bool seek_start = true;
    optional<mpos> last_traced_hex;

    for (int32 i = 0, ii = GameSettings::HEXAGONAL_GEOMETRY ? 6 : 4; i < ii; i++) {
        uint8 dir;
        int32 iterations;

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            dir = numeric_cast<uint8>((i + 2) % 6);
            iterations = distance;
        }
        else {
            dir = numeric_cast<uint8>(((i + 1) * 2) % 8);
            iterations = distance * 2;
        }

        for (int32 j = 0; j < iterations; j++) {
            if (seek_start) {
                for (int32 l = 0; l < distance; l++) {
                    GeometryHelper::MoveHexByDirUnsafe(raw_traced_hex, GameSettings::HEXAGONAL_GEOMETRY ? 0 : 7);
                }

                seek_start = false;
                j = -1;
            }
            else {
                GeometryHelper::MoveHexByDirUnsafe(raw_traced_hex, dir);
            }

            auto traced_hex = _mapSize.clamp_pos(raw_traced_hex);

            if (IsBitSet(ls->Flags & LIGHT_DISABLE_DIR_MASK, 1u << i)) {
                traced_hex = center_hex;
            }
            else {
                TraceLightLine(ls, center_hex, traced_hex, distance, intensity);
            }

            if (!last_traced_hex.has_value() || traced_hex != last_traced_hex.value()) {
                uint8 traced_alpha;
                bool use_offsets = false;

                if (ipos32 {traced_hex.x, traced_hex.y} != raw_traced_hex) {
                    traced_alpha = numeric_cast<uint8>(lerp(numeric_cast<int32>(center_alpha), 0, numeric_cast<float32>(GeometryHelper::GetDistance(center_hex, traced_hex)) / numeric_cast<float32>(distance)));

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

void MapView::TraceLightLine(LightSource* ls, mpos from_hex, mpos& to_hex, int32 distance, int32 intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto [base_sx, base_sy] = GenericUtils::GetStepsCoords({from_hex.x, from_hex.y}, {to_hex.x, to_hex.y});
    const auto sx1_f = base_sx;
    const auto sy1_f = base_sy;

    auto curx1_f = numeric_cast<float32>(from_hex.x);
    auto cury1_f = numeric_cast<float32>(from_hex.y);
    auto curx1_i = numeric_cast<int32>(from_hex.x);
    auto cury1_i = numeric_cast<int32>(from_hex.y);

    auto cur_inten = intensity;
    const auto inten_sub = intensity / distance;

    const auto resolve_hex = [this](int32 hx, int32 hy) -> mpos { return _mapSize.from_raw_pos(hx, hy); };

    while (true) {
        cur_inten -= inten_sub;
        curx1_f += sx1_f;
        cury1_f += sy1_f;

        const int32 old_curx1_i = curx1_i;
        const int32 old_cury1_i = cury1_i;

        // Casts
        curx1_i = iround<int32>(curx1_f);

        if (curx1_f - numeric_cast<float32>(curx1_i) >= 0.5f) {
            curx1_i++;
        }

        cury1_i = iround<int32>(cury1_f);

        if (cury1_f - numeric_cast<float32>(cury1_i) >= 0.5f) {
            cury1_i++;
        }

        // Left&Right trace
        int32 ox = 0;
        int32 oy = 0;

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

        const auto map_width = numeric_cast<int32>(_mapSize.width);
        const auto map_height = numeric_cast<int32>(_mapSize.height);

        if (ox != 0) {
            // Left side
            ox = old_curx1_i + ox;

            if (ox < 0 || ox >= map_width || _hexField->GetCellForReading(resolve_hex(ox, old_cury1_i)).LightBlocked) {
                to_hex = resolve_hex(ox < 0 || ox >= map_width ? old_curx1_i : ox, old_cury1_i);
                MarkLightEnd(ls, resolve_hex(old_curx1_i, old_cury1_i), to_hex, cur_inten);
                break;
            }

            MarkLightStep(ls, resolve_hex(old_curx1_i, old_cury1_i), resolve_hex(ox, old_cury1_i), cur_inten);

            // Right side
            oy = old_cury1_i + oy;

            if (oy < 0 || oy >= map_height || _hexField->GetCellForReading(resolve_hex(old_curx1_i, oy)).LightBlocked) {
                to_hex = resolve_hex(old_curx1_i, oy < 0 || oy >= map_height ? old_cury1_i : oy);
                MarkLightEnd(ls, resolve_hex(old_curx1_i, old_cury1_i), to_hex, cur_inten);
                break;
            }

            MarkLightStep(ls, resolve_hex(old_curx1_i, old_cury1_i), resolve_hex(old_curx1_i, oy), cur_inten);
        }

        // Main trace
        if (curx1_i < 0 || curx1_i >= map_width || cury1_i < 0 || cury1_i >= map_height || _hexField->GetCellForReading(resolve_hex(curx1_i, cury1_i)).LightBlocked) {
            to_hex = resolve_hex(curx1_i < 0 || curx1_i >= map_width ? old_curx1_i : curx1_i, cury1_i < 0 || cury1_i >= map_height ? old_cury1_i : cury1_i);
            MarkLightEnd(ls, resolve_hex(old_curx1_i, old_cury1_i), to_hex, cur_inten);
            break;
        }

        MarkLightEnd(ls, resolve_hex(old_curx1_i, old_cury1_i), resolve_hex(curx1_i, cury1_i), cur_inten);

        if (curx1_i == numeric_cast<int32>(to_hex.x) && cury1_i == numeric_cast<int32>(to_hex.y)) {
            break;
        }
    }
}

void MapView::MarkLightStep(LightSource* ls, mpos from_hex, mpos to_hex, int32 intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(to_hex);

    if (field.HasTransparentWall) {
        const bool north_south = field.Corner == CornerType::NorthSouth || field.Corner == CornerType::North || field.Corner == CornerType::West;
        const auto dir = GeometryHelper::GetDir(from_hex, to_hex);

        if (dir == 0 || (north_south && dir == 1) || (!north_south && (dir == 4 || dir == 5))) {
            MarkLight(ls, to_hex, intensity);
        }
    }
    else {
        MarkLight(ls, to_hex, intensity);
    }
}

void MapView::MarkLightEnd(LightSource* ls, mpos from_hex, mpos to_hex, int32 intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    bool is_wall = false;
    bool north_south = false;
    const auto& field = _hexField->GetCellForReading(to_hex);

    if (field.HasWall) {
        is_wall = true;

        if (field.Corner == CornerType::NorthSouth || field.Corner == CornerType::North || field.Corner == CornerType::West) {
            north_south = true;
        }
    }

    const int32 dir = GeometryHelper::GetDir(from_hex, to_hex);

    if (dir == 0 || (north_south && dir == 1) || (!north_south && (dir == 4 || dir == 5))) {
        MarkLight(ls, to_hex, intensity);

        if (is_wall) {
            if (north_south) {
                if (to_hex.y > 0) {
                    MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x, to_hex.y - 1), true, intensity);
                }
                if (to_hex.y < _mapSize.height - 1) {
                    MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x, to_hex.y + 1), true, intensity);
                }
            }
            else {
                if (to_hex.x > 0) {
                    MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x - 1, to_hex.y), false, intensity);

                    if (to_hex.y > 0) {
                        MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x - 1, to_hex.y - 1), false, intensity);
                    }
                    if (to_hex.y < _mapSize.height - 1) {
                        MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x - 1, to_hex.y + 1), false, intensity);
                    }
                }
                if (to_hex.x < _mapSize.width - 1) {
                    MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x + 1, to_hex.y), false, intensity);

                    if (to_hex.y > 0) {
                        MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x + 1, to_hex.y - 1), false, intensity);
                    }
                    if (to_hex.y < _mapSize.height - 1) {
                        MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x + 1, to_hex.y + 1), false, intensity);
                    }
                }
            }
        }
    }
}

void MapView::MarkLightEndNeighbor(LightSource* ls, mpos hex, bool north_south, int32 intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasWall) {
        const auto corner = field.Corner;

        if ((north_south && (corner == CornerType::NorthSouth || corner == CornerType::North || corner == CornerType::West)) || (!north_south && (corner == CornerType::EastWest || corner == CornerType::East)) || corner == CornerType::South) {
            MarkLight(ls, hex, intensity / 2);
        }
    }
}

void MapView::MarkLight(LightSource* ls, mpos hex, int32 intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto light_value = intensity * MAX_LIGHT_HEX / MAX_LIGHT_INTEN * ls->Capacity / 100;
    const auto light_value_r = numeric_cast<uint8>(light_value * ls->CenterColor.comp.r / 255);
    const auto light_value_g = numeric_cast<uint8>(light_value * ls->CenterColor.comp.g / 255);
    const auto light_value_b = numeric_cast<uint8>(light_value * ls->CenterColor.comp.b / 255);
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

    for (const auto& value : field.LightSources | std::views::values) {
        hex_light.comp.r = std::max(hex_light.comp.r, value.comp.r);
        hex_light.comp.g = std::max(hex_light.comp.g, value.comp.g);
        hex_light.comp.b = std::max(hex_light.comp.b, value.comp.b);
    }
}

void MapView::LightFanToPrimitves(const LightSource* ls, vector<PrimitivePoint>& points, vector<PrimitivePoint>& soft_points) const
{
    FO_STACK_TRACE_ENTRY();

    if (ls->FanHexes.size() <= 1) {
        return;
    }

    ipos32 center_pos = GetHexMapPos(ls->Hex);
    center_pos.x += _engine->Settings.MapHexWidth / 2;
    center_pos.y += _engine->Settings.MapHexHeight / 2;

    const auto center_point = PrimitivePoint {.PointPos = center_pos, .PointColor = ls->CenterColor, .PointOffset = ls->Offset.get()};

    const auto points_start_size = points.size();
    points.reserve(points.size() + ls->FanHexes.size() * 3);
    soft_points.reserve(soft_points.size() + ls->FanHexes.size() * 3);

    for (size_t i = 0; i < ls->FanHexes.size(); i++) {
        const auto& fan_hex = ls->FanHexes[i];
        const mpos hex = std::get<0>(fan_hex);
        const uint8 alpha = std::get<1>(fan_hex);
        const bool use_offsets = std::get<2>(fan_hex);

        const auto [ox, oy] = _engine->Geometry.GetHexOffset(ls->Hex, hex);
        const ipos32 pos = {center_pos.x + ox, center_pos.y + oy};
        const ucolor color = ucolor(ls->CenterColor, alpha);
        const ipos32* offset = use_offsets ? ls->Offset.get() : nullptr;
        const auto edge_point = PrimitivePoint {.PointPos = pos, .PointColor = color, .PointOffset = offset};

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

        if ((next.PointPos - cur.PointPos).idist() > _engine->Settings.MapHexWidth) {
            soft_points.emplace_back(PrimitivePoint {.PointPos = next.PointPos, .PointColor = next.PointColor, .PointOffset = next.PointOffset, .PPointColor = next.PPointColor});
            soft_points.emplace_back(PrimitivePoint {.PointPos = cur.PointPos, .PointColor = cur.PointColor, .PointOffset = cur.PointOffset, .PPointColor = cur.PPointColor});

            const auto dist_comp = (cur.PointPos - center_pos).idist() > (next.PointPos - center_pos).idist();
            const auto x = numeric_cast<float32>(dist_comp ? next.PointPos.x - cur.PointPos.x : cur.PointPos.x - next.PointPos.x);
            const auto y = numeric_cast<float32>(dist_comp ? next.PointPos.y - cur.PointPos.y : cur.PointPos.y - next.PointPos.y);
            const auto changed_xy = GenericUtils::ChangeStepsCoords({x, y}, dist_comp ? -2.5f : 2.5f);

            if (dist_comp) {
                soft_points.emplace_back(PrimitivePoint {.PointPos = {cur.PointPos.x + iround<int32>(changed_xy.x), cur.PointPos.y + iround<int32>(changed_xy.y)}, .PointColor = cur.PointColor, .PointOffset = cur.PointOffset, .PPointColor = cur.PPointColor});
            }
            else {
                soft_points.emplace_back(PrimitivePoint {.PointPos = {next.PointPos.x + iround<int32>(changed_xy.x), next.PointPos.y + iround<int32>(changed_xy.y)}, .PointColor = next.PointColor, .PointOffset = next.PointOffset, .PPointColor = next.PPointColor});
            }
        }
    }
}

void MapView::SetHiddenRoof(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    const auto corrected_hx = hex.x - hex.x % _engine->Settings.MapTileStep;
    const auto corrected_hy = hex.y - hex.y % _engine->Settings.MapTileStep;
    const auto corrected_hex = _mapSize.from_raw_pos(corrected_hx, corrected_hy);

    if (_hiddenRoofNum != _hexField->GetCellForReading(corrected_hex).RoofNum) {
        _hiddenRoofNum = _hexField->GetCellForReading(corrected_hex).RoofNum;
        RebuildMap();
    }
}

auto MapView::MeasureMapBorders(const Sprite* spr, ipos32 offset) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(spr);

    const auto left = std::max(spr->GetSize().width / 2 + spr->GetOffset().x + offset.x + _maxScroll.width - _wLeft * _engine->Settings.MapHexWidth, 0);
    const auto right = std::max(spr->GetSize().width / 2 - spr->GetOffset().x - offset.x + _maxScroll.width - _wRight * _engine->Settings.MapHexWidth, 0);
    const auto top = std::max(0 + spr->GetOffset().y + offset.y + _maxScroll.height - _hTop * _engine->Settings.MapHexLineHeight, 0);
    const auto bottom = std::max(spr->GetSize().height - spr->GetOffset().y - offset.y + _maxScroll.height - _hBottom * _engine->Settings.MapHexLineHeight, 0);

    if (left != 0 || right != 0 || top != 0 || bottom != 0) {
        _wLeft += left / _engine->Settings.MapHexWidth + (left % _engine->Settings.MapHexWidth != 0 ? 1 : 0);
        _wRight += right / _engine->Settings.MapHexWidth + (right % _engine->Settings.MapHexWidth != 0 ? 1 : 0);
        _hTop += top / _engine->Settings.MapHexLineHeight + (top % _engine->Settings.MapHexLineHeight != 0 ? 1 : 0);
        _hTop += _hTop % 2;
        _hBottom += bottom / _engine->Settings.MapHexLineHeight + (bottom % _engine->Settings.MapHexLineHeight != 0 ? 1 : 0);

        if (!_mapLoading) {
            RebuildMap();
        }

        return true;
    }

    return false;
}

auto MapView::MeasureMapBorders(const ItemHexView* item) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item->GetMap() == this);

    return MeasureMapBorders(item->GetSprite(), item->GetSpriteOffset());
}

void MapView::RecacheHexFlags(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));

    auto& field = _hexField->GetCellForWriting(hex);

    RecacheHexFlags(field);
}

void MapView::RecacheHexFlags(Field& field)
{
    FO_STACK_TRACE_ENTRY();

    field.HasWall = false;
    field.HasTransparentWall = false;
    field.HasScenery = false;
    field.MoveBlocked = field.ScrollBlock;
    field.ShootBlocked = false;
    field.LightBlocked = false;
    field.Corner = CornerType::NorthSouth;
    field.GroundTile.reset();
    field.HasRoof = false;

    if (_engine->Settings.CritterBlockHex && !field.Critters.empty()) {
        for (const auto& cr : field.Critters) {
            if (!field.MoveBlocked && !cr->IsDead()) {
                field.MoveBlocked = true;
            }
        }
    }

    if (!field.Items.empty()) {
        for (auto& item : field.Items) {
            if (!field.HasWall && item->GetIsWall()) {
                field.HasWall = true;
                field.HasTransparentWall = item->GetLightThru();

                field.Corner = item->GetCorner();
            }
            else if (!field.HasScenery && item->GetIsScenery()) {
                field.HasScenery = true;

                if (!field.HasWall) {
                    field.Corner = item->GetCorner();
                }
            }

            if (!field.MoveBlocked && !item->GetNoBlock()) {
                field.MoveBlocked = true;
            }
            if (!field.ShootBlocked && !item->GetShootThru()) {
                field.ShootBlocked = true;
            }
            if (!field.LightBlocked && !item->GetLightThru()) {
                field.LightBlocked = true;
            }

            if (item->GetIsTile()) {
                if (item->GetIsRoofTile()) {
                    field.HasRoof = true;
                }
                else if (!field.GroundTile) {
                    field.GroundTile = item;
                }
            }
        }
    }

    if (field.ShootBlocked) {
        field.MoveBlocked = true;
    }
}

void MapView::RecacheScrollBlocks()
{
    FO_STACK_TRACE_ENTRY();

    const irect32 scroll_area = GetScrollAxialArea();
    const int32 scroll_block_size = _engine->Settings.ScrollBlockSize;

    for (const auto hx : iterate_range(_mapSize.width)) {
        for (const auto hy : iterate_range(_mapSize.height)) {
            const mpos hex = {hx, hy};
            const auto& field = _hexField->GetCellForReading(hex);
            const ipos32 axial_hex = _engine->Geometry.GetHexAxialCoord(hex);

            bool is_on_scroll_block = false;

            if (!scroll_area.is_zero()) {
                // Is hex on scroll block line
                if ((axial_hex.x >= scroll_area.x - scroll_block_size && axial_hex.x <= scroll_area.x + scroll_block_size) || //
                    (axial_hex.x >= scroll_area.x + scroll_area.width - scroll_block_size && axial_hex.x <= scroll_area.x + scroll_area.width + scroll_block_size) || //
                    (axial_hex.y >= scroll_area.y - scroll_block_size && axial_hex.y <= scroll_area.y + scroll_block_size) || //
                    (axial_hex.y >= scroll_area.y + scroll_area.height - scroll_block_size && axial_hex.y <= scroll_area.y + scroll_area.height + scroll_block_size)) {
                    is_on_scroll_block = true;
                }
            }

            if (is_on_scroll_block != field.ScrollBlock) {
                _hexField->GetCellForWriting(hex).ScrollBlock = is_on_scroll_block;
                RecacheHexFlags(hex);
            }
        }
    }
}

void MapView::Resize(msize size)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);
    FO_RUNTIME_ASSERT(size.width >= GameSettings::MIN_MAP_SIZE);
    FO_RUNTIME_ASSERT(size.width <= GameSettings::MAX_MAP_SIZE);
    FO_RUNTIME_ASSERT(size.height >= GameSettings::MIN_MAP_SIZE);
    FO_RUNTIME_ASSERT(size.height <= GameSettings::MAX_MAP_SIZE);

    for (const auto& vf : _viewField) {
        if (_mapSize.is_valid_pos(vf.RawHex)) {
            auto& field = _hexField->GetCellForWriting(_mapSize.from_raw_pos(vf.RawHex));
            field.IsView = false;
            InvalidateSpriteChain(field);
        }
    }

    // Remove objects on shrink
    for (const auto hy : iterate_range(std::max(size.height, _mapSize.height))) {
        for (const auto hx : iterate_range(std::max(size.width, _mapSize.width))) {
            if (hx >= size.width || hy >= size.height) {
                const auto& field = _hexField->GetCellForReading({hx, hy});

                if (!field.OriginCritters.empty()) {
                    for (auto& cr : copy(field.OriginCritters)) {
                        DestroyCritter(cr.get());
                    }
                }
                if (!field.OriginItems.empty()) {
                    for (auto& item : copy(field.OriginItems)) {
                        DestroyItem(item.get());
                    }
                }

                FO_RUNTIME_ASSERT(field.OriginCritters.empty());
                FO_RUNTIME_ASSERT(field.OriginItems.empty());
                FO_RUNTIME_ASSERT(!field.SpriteChain);
            }
        }
    }

    SetSize(size);
    _mapSize = size;

    SetWorkHex(_mapSize.clamp_pos(GetWorkHex()));

    _hexTrack.resize(_mapSize.square());
    MemFill(_hexTrack.data(), 0, _hexTrack.size());
    _hexLight.resize(_mapSize.square() * 3);
    _hexField->Resize(_mapSize);

    for (const auto hy : iterate_range(_mapSize.height)) {
        for (const auto hx : iterate_range(_mapSize.width)) {
            CalculateHexLight(mpos(hx, hy), _hexField->GetCellForReading(mpos(hx, hy)));
        }
    }

    RecacheScrollBlocks();
    RebuildMapNow();
}

void MapView::SwitchShowHex()
{
    FO_STACK_TRACE_ENTRY();

    _isShowHex = !_isShowHex;

    RebuildMap();
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

    RebuildMap();
}

auto MapView::ScreenToMapPos(ipos32 screen_pos) const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    return (fpos32(screen_pos) / GetSpritesZoom() + _scrollOffset).round<int32>();
}

auto MapView::MapToScreenPos(ipos32 map_pos) const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    return ((fpos32(map_pos) - _scrollOffset) * GetSpritesZoom()).round<int32>();
}

void MapView::InitView()
{
    FO_STACK_TRACE_ENTRY();

    const isize32 view_size = GetViewSize();
    _wVisible = view_size.width + _wLeft + _wRight;
    _hVisible = view_size.height + _hTop + _hBottom;
    _viewField.resize(numeric_cast<size_t>(_hVisible) * _wVisible);

    // From screen left top to view left top
    auto row_hex = _screenRawHex;

    for (int32 i = 0; i < _wLeft; i++) {
        constexpr auto dir = static_cast<uint8>(GameSettings::HEXAGONAL_GEOMETRY ? 4 : 5);
        GeometryHelper::MoveHexByDirUnsafe(row_hex, dir);
    }
    for (int32 i = 0; i < _hTop / 2; i++) {
        constexpr auto dir1 = static_cast<uint8>(GameSettings::HEXAGONAL_GEOMETRY ? 5 : 6);
        constexpr auto dir2 = static_cast<uint8>(0);
        GeometryHelper::MoveHexByDirUnsafe(row_hex, dir1);
        GeometryHelper::MoveHexByDirUnsafe(row_hex, dir2);
    }

    // Assign view coords
    size_t vpos = 0;
    auto oy = -_engine->Settings.MapHexLineHeight * _hTop;

    for (auto yv = 0; yv < _hVisible; yv++) {
        auto cur_hex = row_hex;
        auto ox = -(_wLeft * _engine->Settings.MapHexWidth) + (yv % 2 != 0 ? _engine->Settings.MapHexWidth / 2 : 0);

        for (auto xv = 0; xv < _wVisible; xv++) {
            auto& vf = _viewField[vpos];
            ++vpos;

            vf.Offset = {ox, oy};
            vf.RawHex = cur_hex;

            ox += _engine->Settings.MapHexWidth;
            GeometryHelper::MoveHexByDirUnsafe(cur_hex, 1);
        }

        oy += _engine->Settings.MapHexLineHeight;
        GeometryHelper::MoveHexByDirUnsafe(row_hex, yv % 2 == 0 ? 2 : 3);
    }
}

void MapView::AddSpriteToChain(Field& field, MapSprite* mspr)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (!field.SpriteChain) {
        field.SpriteChain = mspr;
        mspr->CreateExtraChain(field.SpriteChain.get_pp());
    }
    else {
        field.SpriteChain->AddToExtraChain(mspr);
    }
}

void MapView::InvalidateSpriteChain(Field& field)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (field.SpriteChain) {
        FO_RUNTIME_ASSERT(field.SpriteChain->IsValid());
        while (field.SpriteChain != nullptr) {
            FO_RUNTIME_ASSERT(field.SpriteChain->IsValid());
            field.SpriteChain->Invalidate();
        }
    }
}

auto MapView::GetScreenRawHex() const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    return _screenRawHex;
}

auto MapView::GetCenterRawHex() const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    const ipos32 lt_pos = _engine->Geometry.GetHexPos(_screenRawHex);
    const ipos32 center_offset = ipos32(iround<int32>(_viewSize.width) / 2, iround<int32>(_viewSize.height) / 2);
    return _engine->Geometry.GetHexPosCoord(lt_pos + center_offset);
}

auto MapView::ConvertToScreenRawHex(ipos32 center_raw_hex) const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    const ipos32 center_pos = _engine->Geometry.GetHexPos(center_raw_hex);
    const ipos32 center_offset = ipos32(iround<int32>(_viewSize.width) / 2, iround<int32>(_viewSize.height) / 2);
    return _engine->Geometry.GetHexPosCoord(center_pos - center_offset);
}

auto MapView::GetHexMapPos(mpos hex) const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    const auto hex_offset = _engine->Geometry.GetHexOffset(_screenRawHex, ipos32(hex));
    return {hex_offset.x, hex_offset.y};
}

void MapView::DrawMap()
{
    FO_STACK_TRACE_ENTRY();

    if (_rebuildMap) {
        RebuildMapNow();
    }

    ProcessLighting();
    PrepareFogToDraw();
    _mapSprites.SortIfNeeded();

    // Draw by parts if view size too big
    const fsize32 screen_size = fsize32(_screenSize);
    const fpos32 draw_scale = {screen_size.width / _viewSize.width, screen_size.height / _viewSize.height};
    const int32 steps_width = iround<int32>(std::ceil(1.0f / draw_scale.x));
    const int32 steps_height = iround<int32>(std::ceil(1.0f / draw_scale.y));
    const bool direct_draw = _engine->Settings.MapDirectDraw;

    for (int32 step_x = 0; step_x < steps_width; step_x++) {
        for (int32 step_y = 0; step_y < steps_height; step_y++) {
            irect32 draw_area;

            if (direct_draw) {
                const int32 draw_x = iround<int32>(_scrollOffset.x);
                const int32 draw_y = iround<int32>(_scrollOffset.y);
                draw_area = {draw_x, draw_y, _screenSize.width, _screenSize.height};
            }
            else {
                FO_RUNTIME_ASSERT(_rtMap);
                _engine->SprMngr.GetRtMngr().PushRenderTarget(_rtMap.get());
                _engine->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);

                const int32 draw_x = iround<int32>(std::floor(_scrollOffset.x)) + step_x * _screenSize.width;
                const int32 draw_y = iround<int32>(std::floor(_scrollOffset.y)) + step_y * _screenSize.height;
                const int32 draw_width = std::min(iround<int32>(std::ceil(_viewSize.width)) - step_x * _screenSize.width, _screenSize.width);
                const int32 draw_height = std::min(iround<int32>(std::ceil(_viewSize.height)) - step_y * _screenSize.height, _screenSize.height);
                draw_area = {draw_x, draw_y, draw_width, draw_height};
            }

            const float32 step_xf = numeric_cast<float32>(step_x);
            const float32 step_yf = numeric_cast<float32>(step_y);
            const float32 source_x = std::fmod(_scrollOffset.x, 1.0f);
            const float32 source_y = std::fmod(_scrollOffset.y, 1.0f);
            const float32 source_width = std::min(_viewSize.width - step_xf * screen_size.width, screen_size.width);
            const float32 source_height = std::min(_viewSize.height - step_yf * screen_size.height, screen_size.height);
            const frect32 source_rect = {source_x, source_y, source_width, source_height};
            const int32 target_x = iround<int32>(std::floor(step_xf * screen_size.width * draw_scale.x));
            const int32 target_y = iround<int32>(std::floor(step_yf * screen_size.height * draw_scale.y));
            const int32 target_width = iround<int32>(std::ceil(source_width * draw_scale.x));
            const int32 target_height = iround<int32>(std::ceil(source_height * draw_scale.y));
            const irect32 target_rect = {target_x, target_y, target_width, target_height};

            // Tiles
            _engine->SprMngr.DrawSprites(_mapSprites, draw_area, false, false, DrawOrderType::Tile, DrawOrderType::Tile4, _mapDayColor);

            // Flat sprites
            _engine->SprMngr.DrawSprites(_mapSprites, draw_area, true, false, DrawOrderType::HexGrid, DrawOrderType::FlatScenery, _mapDayColor);

            // Lighting
            if (!_engine->Settings.DisableLighting) {
                _engine->SprMngr.GetRtMngr().PushRenderTarget(_rtLight.get());
                _engine->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);

                for (auto& points : _lightPoints) {
                    _engine->SprMngr.DrawPoints(points, RenderPrimitiveType::TriangleList, &draw_area, _engine->EffectMngr.Effects.Light.get());
                }
                for (auto& points : _lightSoftPoints) {
                    _engine->SprMngr.DrawPoints(points, RenderPrimitiveType::TriangleList, &draw_area, _engine->EffectMngr.Effects.Light.get());
                }

                _engine->SprMngr.GetRtMngr().PopRenderTarget();

                _rtLight->SetCustomDrawEffect(_engine->EffectMngr.Effects.FlushLight.get());
                _engine->SprMngr.DrawRenderTarget(_rtLight.get(), true);
            }

            // Other sprites
            _engine->SprMngr.DrawSprites(_mapSprites, draw_area, true, true, DrawOrderType::Ligth, DrawOrderType::Last, _mapDayColor);

            // Collected contours
            _engine->SprMngr.DrawContours();

            // Fog
            if (!_engine->Settings.DisableFog && !_mapperMode) {
                _engine->SprMngr.GetRtMngr().PushRenderTarget(_rtLight.get());
                _engine->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);

                _engine->SprMngr.DrawPoints(_fogLookPoints, RenderPrimitiveType::TriangleStrip, &draw_area, _engine->EffectMngr.Effects.Fog.get());
                _engine->SprMngr.DrawPoints(_fogShootPoints, RenderPrimitiveType::TriangleStrip, &draw_area, _engine->EffectMngr.Effects.Fog.get());

                _engine->SprMngr.GetRtMngr().PopRenderTarget();

                _rtLight->SetCustomDrawEffect(_engine->EffectMngr.Effects.FlushFog.get());
                _engine->SprMngr.DrawRenderTarget(_rtLight.get(), true);
            }

            // Draw streched render target
            if (!direct_draw) {
                _engine->SprMngr.GetRtMngr().PopRenderTarget();
                _engine->SprMngr.DrawRenderTarget(_rtMap.get(), false, &source_rect, &target_rect);
            }
        }
    }
}

void MapView::PrepareFogToDraw()
{
    FO_STACK_TRACE_ENTRY();

    if (_engine->Settings.DisableFog) {
        return;
    }
    if (_mapperMode) {
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
            const int32 chosen_dir = chosen->GetDir();
            const auto dist_shoot = _shootBordersDist;
            const auto half_hw = _engine->Settings.MapHexWidth / 2;
            const auto half_hh = _engine->Settings.MapHexHeight / 2;

            const ipos32 base_pos = GetHexMapPos(base_hex);
            const auto center_look_point = PrimitivePoint {.PointPos = {base_pos.x + half_hw, base_pos.y + half_hh}, .PointColor = ucolor {0, 0, 0, 0}, .PointOffset = chosen->GetSpriteOffsetPtr()};
            const auto center_shoot_point = PrimitivePoint {.PointPos = {base_pos.x + half_hw, base_pos.y + half_hh}, .PointColor = ucolor {0, 0, 0, 255}, .PointOffset = chosen->GetSpriteOffsetPtr()};

            auto target_raw_hex = ipos32 {base_hex.x, base_hex.y};

            size_t look_points_added = 0;
            size_t shoot_points_added = 0;

            bool seek_start = true;

            for (auto i = 0; i < (GameSettings::HEXAGONAL_GEOMETRY ? 6 : 4); i++) {
                uint8 dir;
                int32 iterations;

                if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
                    dir = numeric_cast<uint8>((i + 2) % 6);
                    iterations = dist;
                }
                else {
                    dir = numeric_cast<uint8>(((i + 1) * 2) % 8);
                    iterations = dist * 2;
                }

                for (int32 j = 0; j < iterations; j++) {
                    if (seek_start) {
                        // Move to start position
                        for (int32 l = 0; l < dist; l++) {
                            GeometryHelper::MoveHexByDirUnsafe(target_raw_hex, GameSettings::HEXAGONAL_GEOMETRY ? 0 : 7);
                        }
                        seek_start = false;
                        j = -1;
                    }
                    else {
                        // Move to next hex
                        GeometryHelper::MoveHexByDirUnsafe(target_raw_hex, numeric_cast<uint8>(dir));
                    }

                    auto target_hex = _mapSize.clamp_pos(target_raw_hex);

                    if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
                        const int32 dir_ = GeometryHelper::GetDir(base_hex, target_hex);
                        auto ii = (chosen_dir > dir_ ? chosen_dir - dir_ : dir_ - chosen_dir);

                        if (ii > GameSettings::MAP_DIR_COUNT / 2) {
                            ii = GameSettings::MAP_DIR_COUNT - ii;
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

                    auto dist_look = GeometryHelper::GetDistance(base_hex, target_hex);

                    if (_drawLookBorders) {
                        const auto hex_pos = GetHexMapPos(target_hex);
                        const auto color = ucolor {255, numeric_cast<uint8>(dist_look * 255 / dist), 0, 0};
                        const auto* offset = dist_look == dist ? chosen->GetSpriteOffsetPtr() : nullptr;

                        _fogLookPoints.emplace_back(PrimitivePoint {.PointPos = {hex_pos.x + half_hw, hex_pos.y + half_hh}, .PointColor = color, .PointOffset = offset});

                        if (++look_points_added % 2 == 0) {
                            _fogLookPoints.emplace_back(center_look_point);
                        }
                    }

                    if (_drawShootBorders) {
                        mpos block_hex;
                        const auto max_shoot_dist = std::max(std::min(dist_look, dist_shoot), 0) + 1;

                        TraceBullet(base_hex, target_hex, max_shoot_dist, 0.0f, nullptr, CritterFindType::Any, nullptr, &block_hex, nullptr, true);

                        const auto block_hex_pos = GetHexMapPos(block_hex);
                        const auto result_shoot_dist = GeometryHelper::GetDistance(base_hex, block_hex);
                        const auto color = ucolor {255, numeric_cast<uint8>(result_shoot_dist * 255 / max_shoot_dist), 0, 255};
                        const auto* offset = result_shoot_dist == max_shoot_dist ? chosen->GetSpriteOffsetPtr() : nullptr;

                        _fogShootPoints.emplace_back(PrimitivePoint {.PointPos = {block_hex_pos.x + half_hw, block_hex_pos.y + half_hh}, .PointColor = color, .PointOffset = offset});

                        if (++shoot_points_added % 2 == 0) {
                            _fogShootPoints.emplace_back(center_shoot_point);
                        }
                    }
                }
            }
        }
    }
}

auto MapView::IsOutsideArea(mpos hex) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const irect32 scroll_area = GetScrollAxialArea();

    if (!scroll_area.is_zero()) {
        const ipos32 axial_hex = _engine->Geometry.GetHexAxialCoord(hex);

        if (axial_hex.x < scroll_area.x || axial_hex.x > scroll_area.x + scroll_area.width || //
            axial_hex.y < scroll_area.y || axial_hex.y > scroll_area.y + scroll_area.height) {
            return true;
        }
    }

    return false;
}

auto MapView::IsManualScrolling() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _engine->Settings.ScrollMouseLeft || _engine->Settings.ScrollKeybLeft || _engine->Settings.ScrollMouseRight || //
        _engine->Settings.ScrollKeybRight || _engine->Settings.ScrollMouseUp || _engine->Settings.ScrollKeybUp || //
        _engine->Settings.ScrollMouseDown || _engine->Settings.ScrollKeybDown;
}

void MapView::ProcessScroll(float32 dt)
{
    FO_STACK_TRACE_ENTRY();

    const bool is_manual_scrolling = IsManualScrolling();

    if (is_manual_scrolling && _autoScrollCanStop) {
        _autoScrollActive = false;
    }

    if (_autoScrollHardLockedCritter) {
        const auto* cr = GetCritter(_autoScrollHardLockedCritter);

        if (cr != nullptr && ipos32(cr->GetHex()) != GetCenterRawHex()) {
            ScrollToHex(cr->GetHex(), cr->GetHexOffset(), _autoScrollLockSpeed, false);
        }
    }

    if (_autoScrollSoftLockedCritter && !is_manual_scrolling) {
        const auto* cr = GetCritter(_autoScrollSoftLockedCritter);

        if (cr != nullptr && cr->GetHex() != _autoScrollCritterLastHex) {
            const auto hex_offset = _engine->Geometry.GetHexOffset(_autoScrollCritterLastHex, cr->GetHex());
            ApplyScrollOffset(hex_offset - ipos32(_autoScrollCritterLastHexOffset) + ipos32(cr->GetHexOffset()), _autoScrollLockSpeed, true);
            _autoScrollCritterLastHex = cr->GetHex();
            _autoScrollCritterLastHexOffset = cr->GetHexOffset();
        }
    }

    fpos32 scroll;

    if (_autoScrollActive) {
        const auto scroll_step = numeric_cast<float32>(_autoScrollSpeed) / 10000.0f * dt;
        scroll.x = lerp(0.0f, _autoScrollOffset.x, scroll_step);
        scroll.y = lerp(0.0f, _autoScrollOffset.y, scroll_step);

        _autoScrollOffset -= scroll;

        if (_autoScrollOffset.dist() < 0.001f) {
            _autoScrollActive = false;
        }
    }
    else {
        if (!is_manual_scrolling) {
            return;
        }

        if (_engine->Settings.ScrollMouseLeft || _engine->Settings.ScrollKeybLeft) {
            scroll.x -= 1.0f;
        }
        if (_engine->Settings.ScrollMouseRight || _engine->Settings.ScrollKeybRight) {
            scroll.x += 1.0f;
        }
        if (_engine->Settings.ScrollMouseUp || _engine->Settings.ScrollKeybUp) {
            scroll.y -= 1.0f;
        }
        if (_engine->Settings.ScrollMouseDown || _engine->Settings.ScrollKeybDown) {
            scroll.y += 1.0f;
        }

        if (std::abs(scroll.x) < 0.1f && std::abs(scroll.y) < 0.1f) {
            return;
        }

        const auto zoom = GetSpritesZoom();
        const auto scroll_step = numeric_cast<float32>(_engine->Settings.ScrollSpeed) / 1000.f / zoom * dt;
        scroll.x *= scroll_step;
        scroll.y *= scroll_step;
    }

    InstantScroll(scroll);
}

void MapView::ChangeZoom(float32 new_zoom, fpos32 anchor)
{
    FO_STACK_TRACE_ENTRY();

    if (!_engine->Settings.MapZoomEnabled) {
        return;
    }

    _zoomAnchor = anchor;
    SetSpritesZoomTarget(new_zoom);
}

void MapView::ProcessZoom(float32 dt)
{
    FO_STACK_TRACE_ENTRY();

    if (!_engine->Settings.MapZoomEnabled) {
        return;
    }

    const float32 init_zoom = GetSpritesZoom();
    const float32 target_zoom = GetSpritesZoomTarget();

    if (is_float_equal(init_zoom, target_zoom)) {
        return;
    }

    const float32 min_zoom = _engine->Settings.ScrollCheck ? _minZoomScroll : GameSettings::MIN_ZOOM;
    constexpr float32 max_zoom = GameSettings::MAX_ZOOM;
    const float32 clamped_target_zoom = std::clamp(target_zoom, min_zoom, max_zoom);

    if (!is_float_equal(target_zoom, clamped_target_zoom)) {
        SetSpritesZoomTarget(clamped_target_zoom);
    }

    if (is_float_equal(init_zoom, clamped_target_zoom)) {
        return;
    }

    if (init_zoom >= min_zoom && init_zoom <= max_zoom) {
        const int32 zoom_speed = _engine->Settings.ZoomSpeed;
        constexpr float32 zoom_stop_bias = 0.001f;

        float32 new_zoom = lerp(init_zoom, clamped_target_zoom, dt * numeric_cast<float32>(zoom_speed) / 10000.0f);

        if (std::abs(new_zoom - clamped_target_zoom) < zoom_stop_bias) {
            new_zoom = clamped_target_zoom;
        }

        InstantZoom(new_zoom, _zoomAnchor);
    }
    else {
        InstantZoom(clamped_target_zoom, _zoomAnchor);
    }
}

void MapView::InstantZoom(float32 new_zoom, fpos32 anchor)
{
    FO_STACK_TRACE_ENTRY();

    if (!_engine->Settings.MapZoomEnabled) {
        return;
    }

    FO_RUNTIME_ASSERT(new_zoom >= GameSettings::MIN_ZOOM);
    FO_RUNTIME_ASSERT(new_zoom <= GameSettings::MAX_ZOOM);

    const float32 init_zoom = GetSpritesZoom();
    const isize32 init_size = GetViewSize();

    SetSpritesZoom(new_zoom);

    const isize32 new_size = GetViewSize();
    const bool size_changed = init_size != new_size;

    _viewSize.width = numeric_cast<float32>(_screenSize.width) / new_zoom;
    _viewSize.height = numeric_cast<float32>(_screenSize.height) / new_zoom;

    if (size_changed) {
        const int32 size_diff_width = new_size.width - init_size.width;
        const int32 size_diff_height = new_size.height - init_size.height;

        if (size_diff_width < 0) {
            HideHexLines(-size_diff_width, 0);
        }
        if (size_diff_height < 0) {
            HideHexLines(0, -size_diff_height);
        }

        InitView();

        if (size_diff_width > 0) {
            ShowHexLines(size_diff_width, 0);
        }
        if (size_diff_height > 0) {
            ShowHexLines(0, size_diff_height);
        }

        _rebuildFog = true;
        _engine->OnScreenScroll.Fire();
    }

    const float32 screen_width = numeric_cast<float32>(_screenSize.width);
    const float32 screen_height = numeric_cast<float32>(_screenSize.height);
    const float32 changed_width = screen_width / new_zoom - screen_width / init_zoom;
    const float32 changed_height = screen_height / new_zoom - screen_height / init_zoom;

    InstantScroll(-fpos32(changed_width * anchor.x, changed_height * anchor.y));
}

void MapView::InstantScroll(fpos32 scroll)
{
    FO_STACK_TRACE_ENTRY();

    _scrollOffset += scroll;

    if (_engine->Settings.ScrollCheck) {
        const irect32 scroll_area = GetScrollAxialArea();

        if (!scroll_area.is_zero()) {
            const fpos32 screen_pos = fpos32(_engine->Geometry.GetHexPos(_screenRawHex));
            const ipos32 half_hex = {_engine->Settings.MapHexWidth / 2, _engine->Settings.MapHexHeight / 2};
            const float32 zoom = GetSpritesZoom();
            const fpos32 view_size = fpos32(numeric_cast<float32>(_screenSize.width), numeric_cast<float32>(_screenSize.height)) / zoom;
            const fpos32 lt_pos = screen_pos + _scrollOffset;
            const fpos32 rb_pos = screen_pos + view_size + _scrollOffset;
            const float32 area_l = numeric_cast<float32>(scroll_area.x * _engine->Settings.MapHexWidth / 2 + half_hex.x);
            const float32 area_t = numeric_cast<float32>(scroll_area.y * _engine->Settings.MapHexLineHeight + half_hex.y);
            const float32 area_r = numeric_cast<float32>((scroll_area.x + scroll_area.width) * _engine->Settings.MapHexWidth / 2 + half_hex.x);
            const float32 area_b = numeric_cast<float32>((scroll_area.y + scroll_area.height) * _engine->Settings.MapHexLineHeight + half_hex.y);

            if (lt_pos.x - area_l < 0.0f) {
                _scrollOffset.x -= lt_pos.x - area_l;
            }
            if (lt_pos.y - area_t < 0.0f) {
                _scrollOffset.y -= lt_pos.y - area_t;
            }
            if (rb_pos.x - area_r >= 0.0f) {
                _scrollOffset.x -= rb_pos.x - area_r + 0.001f;
            }
            if (rb_pos.y - area_b >= 0.0f) {
                _scrollOffset.y -= rb_pos.y - area_b + 0.001f;
            }
        }
    }

    const float32 max_scroll_width = numeric_cast<float32>(_maxScroll.width);
    const float32 max_scroll_height = numeric_cast<float32>(_maxScroll.height);
    int32 xmove = 0;
    int32 ymove = 0;

    while (_scrollOffset.x < 0.0f) {
        xmove += -1;
        _scrollOffset.x += max_scroll_width;
    }
    while (_scrollOffset.x >= max_scroll_width) {
        xmove += 1;
        _scrollOffset.x -= max_scroll_width;
    }
    while (_scrollOffset.y < 0.0f) {
        ymove += -2;
        _scrollOffset.y += max_scroll_height;
    }
    while (_scrollOffset.y >= max_scroll_height) {
        ymove += 2;
        _scrollOffset.y -= max_scroll_height;
    }

    SetScrollOffset(_scrollOffset.round<int32>());

    if (xmove != 0 || ymove != 0) {
        RebuildMapOffset({xmove, ymove});
        _rebuildFog = true;
        _engine->OnScreenScroll.Fire();
    }
}

void MapView::ScrollToHex(mpos hex, ipos16 hex_offset, int32 speed, bool can_stop)
{
    FO_STACK_TRACE_ENTRY();

    const ipos32 hex_pos = _engine->Geometry.GetHexOffset(GetCenterRawHex(), ipos32(hex));
    _autoScrollActive = false;
    ApplyScrollOffset(hex_pos - ipos32(hex_offset) - GetScrollOffset(), speed, can_stop);
}

void MapView::ApplyScrollOffset(ipos32 offset, int32 speed, bool can_stop)
{
    FO_STACK_TRACE_ENTRY();

    if (!_autoScrollActive) {
        _autoScrollActive = true;
        _autoScrollOffset = {};
    }

    _autoScrollCanStop = can_stop;
    _autoScrollSpeed = speed;
    _autoScrollOffset += fpos32(offset);
}

void MapView::LockScreenScroll(CritterView* cr, int32 speed, bool soft_lock, bool unlock_if_same)
{
    FO_STACK_TRACE_ENTRY();

    const auto id = cr != nullptr ? cr->GetId() : ident_t();

    if (soft_lock) {
        if (unlock_if_same && id == _autoScrollSoftLockedCritter) {
            _autoScrollSoftLockedCritter = ident_t();
        }
        else {
            _autoScrollSoftLockedCritter = id;
        }

        _autoScrollCritterLastHex = cr != nullptr ? cr->GetHex() : mpos();
        _autoScrollCritterLastHexOffset = cr != nullptr ? cr->GetHexOffset() : ipos16();
    }
    else {
        if (unlock_if_same && id == _autoScrollHardLockedCritter) {
            _autoScrollHardLockedCritter = ident_t();
        }
        else {
            _autoScrollHardLockedCritter = id;
        }
    }

    _autoScrollLockSpeed = speed;
}

void MapView::SetExtraScrollOffset(fpos32 offset)
{
    FO_STACK_TRACE_ENTRY();

    InstantScroll(-_extraScrollOffset);
    InstantScroll(offset);
    _extraScrollOffset = offset;
}

void MapView::RefreshMinZoom()
{
    FO_STACK_TRACE_ENTRY();

    if (const irect32 scroll_area = GetScrollAxialArea(); !scroll_area.is_zero()) {
        constexpr float32 min_zoom_bias = 1.1f;
        const float32 min_zoom_x = numeric_cast<float32>(_screenSize.width) / numeric_cast<float32>(scroll_area.width * (_engine->Settings.MapHexWidth / 2)) * min_zoom_bias;
        const float32 min_zoom_y = numeric_cast<float32>(_screenSize.height) / numeric_cast<float32>(scroll_area.height * _engine->Settings.MapHexLineHeight) * min_zoom_bias;
        _minZoomScroll = std::max(min_zoom_x, min_zoom_y);
    }
    else {
        _minZoomScroll = GameSettings::MIN_ZOOM;
    }

    SetSpritesZoom(std::max(_minZoomScroll, GetSpritesZoom()));
    SetSpritesZoomTarget(std::max(_minZoomScroll, GetSpritesZoomTarget()));
}

void MapView::AddCritterToField(CritterHexView* cr)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = cr->GetHex();
    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));
    auto& field = _hexField->GetCellForWriting(hex);

    vec_add_unique_value(field.OriginCritters, cr);
    vec_add_unique_value(field.Critters, cr);
    RecacheHexFlags(field);
    SetMultihexCritter(cr, true);
    UpdateCritterLightSource(cr);

    if (!_mapLoading && IsHexToDraw(hex)) {
        DrawHexCritter(cr, field, hex);
    }
}

void MapView::RemoveCritterFromField(CritterHexView* cr)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = cr->GetHex();
    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));
    auto& field = _hexField->GetCellForWriting(hex);

    vec_remove_unique_value(field.OriginCritters, cr);
    vec_remove_unique_value(field.Critters, cr);
    RecacheHexFlags(field);
    SetMultihexCritter(cr, false);
    FinishLightSource(cr->GetId());
    cr->InvalidateSprite();
}

auto MapView::GetCritter(ident_t id) -> CritterHexView*
{
    FO_STACK_TRACE_ENTRY();

    if (!id) {
        return nullptr;
    }

    const auto it = _crittersMap.find(id);
    return it != _crittersMap.end() ? it->second.get() : nullptr;
}

auto MapView::GetNonDeadCritter(mpos hex) -> CritterHexView*
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Critters.empty()) {
        return nullptr;
    }

    auto& field2 = _hexField->GetCellForWriting(hex);

    for (auto& cr : field2.Critters) {
        if (!cr->IsDead()) {
            return cr.get();
        }
    }

    return nullptr;
}

auto MapView::AddReceivedCritter(ident_t id, hstring pid, mpos hex, int16 dir_angle, const vector<vector<uint8>>& data) -> CritterHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(id);
    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));

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
    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));

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

    refcount_ptr cr_ref_holder = cr;
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

auto MapView::GetCrittersOnHex(mpos hex, CritterFindType find_type) -> vector<CritterHexView*>
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Critters.empty()) {
        return {};
    }

    auto& field2 = _hexField->GetCellForWriting(hex);
    vector<CritterHexView*> critters;
    critters.reserve(field2.Critters.size());

    for (auto& cr : field2.Critters) {
        if (cr->CheckFind(find_type)) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

auto MapView::GetCrittersOnHex(mpos hex, CritterFindType find_type) const -> vector<const CritterHexView*>
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Critters.empty()) {
        return {};
    }

    vector<const CritterHexView*> critters;
    critters.reserve(field.Critters.size());

    for (const auto& cr : field.Critters) {
        if (cr->CheckFind(find_type)) {
            critters.emplace_back(cr.get());
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
        if (cr != nullptr && cr->IsMapSpriteValid()) {
            if (!cr->IsDead() && !cr->GetIsChosen()) {
                cr->GetMapSprite()->SetContour(_crittersContour);
            }
            else {
                cr->GetMapSprite()->SetContour(ContourType::None);
            }
        }
    }

    _critterContourCrId = cr_id;
    _critterContour = contour;

    if (cr_id) {
        auto* cr = GetCritter(cr_id);
        if (cr != nullptr && cr->IsMapSpriteValid()) {
            cr->GetMapSprite()->SetContour(contour);
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
        if (!cr->GetIsChosen() && cr->IsMapSpriteValid() && !cr->IsDead() && cr->GetId() != _critterContourCrId) {
            cr->GetMapSprite()->SetContour(contour);
        }
    }
}

void MapView::MoveCritter(CritterHexView* cr, mpos to_hex, bool smoothly)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr->GetMap() == this);
    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(to_hex));

    const auto cur_hex = cr->GetHex();

    if (cur_hex == to_hex) {
        return;
    }

    RemoveCritterFromField(cr);

    cr->SetHex(to_hex);

    if (smoothly) {
        const auto hex_offset = _engine->Geometry.GetHexOffset(to_hex, cur_hex);

        cr->AddExtraOffs(hex_offset);
    }

    AddCritterToField(cr);

    if (cr->GetIsChosen()) {
        RebuildFog();
    }
}

void MapView::ReapplyCritterView(CritterHexView* cr)
{
    FO_STACK_TRACE_ENTRY();

    RemoveCritterFromField(cr);
    AddCritterToField(cr);
}

void MapView::SetMultihexCritter(CritterHexView* cr, bool set)
{
    FO_STACK_TRACE_ENTRY();

    const auto multihex = cr->GetMultihex();

    if (multihex != 0) {
        const auto hex = cr->GetHex();
        const auto hexes_around = GeometryHelper::HexesInRadius(multihex);

        for (int32 i = 1; i < hexes_around; i++) {
            if (auto multihex_hex = hex; GeometryHelper::MoveHexAroundAway(multihex_hex, i, _mapSize)) {
                auto& field = _hexField->GetCellForWriting(multihex_hex);

                if (set) {
                    vec_add_unique_value(field.Critters, cr);
                }
                else {
                    vec_remove_unique_value(field.Critters, cr);
                }

                RecacheHexFlags(field);
            }
        }
    }
}

void MapView::DrawHexCritter(CritterHexView* cr, Field& field, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    if (_mapperMode && !_engine->Settings.ShowCrit) {
        return;
    }

    ContourType contour = ContourType::None;

    if (cr->GetId() == _critterContourCrId) {
        contour = _critterContour;
    }
    else if (!cr->IsDead() && !cr->GetIsChosen()) {
        contour = _crittersContour;
    }

    const auto draw_order = cr->IsDead() && !cr->GetDeadDrawNoFlatten() ? DrawOrderType::DeadCritter : DrawOrderType::Critter;
    auto* mspr = cr->AddSprite(_mapSprites, draw_order, hex, &field.Offset);
    mspr->SetContour(contour, cr->GetContourColor());
    AddSpriteToChain(field, mspr);
}

auto MapView::GetHexAtScreen(ipos32 screen_pos, mpos& hex, ipos32* hex_offset) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const ipos32 pos = ScreenToMapPos(screen_pos);
    const ipos32 screen_offset = _engine->Geometry.GetHexPos(_screenRawHex);
    ipos32 offset;
    ipos32 raw_hex = _engine->Geometry.GetHexPosCoord(screen_offset + pos, &offset);

    // Correct with hex color mask
    if (_picHexMask) {
        const int32 mask_x = std::clamp(offset.x, 0, _picHexMask->GetSize().width - 1);
        const int32 mask_y = std::clamp(offset.y, 0, _picHexMask->GetSize().height - 1);
        const ucolor mask_color = _picHexMaskData[mask_y * _picHexMask->GetSize().width + mask_x];
        const uint8 mask_color_r = mask_color.comp.r;

        if (mask_color_r == 50) {
            GeometryHelper::MoveHexByDirUnsafe(raw_hex, GameSettings::HEXAGONAL_GEOMETRY ? 5 : 6);
        }
        else if (mask_color_r == 100) {
            GeometryHelper::MoveHexByDirUnsafe(raw_hex, 0);
        }
        else if (mask_color_r == 150) {
            GeometryHelper::MoveHexByDirUnsafe(raw_hex, GameSettings::HEXAGONAL_GEOMETRY ? 3 : 4);
        }
        else if (mask_color_r == 200) {
            GeometryHelper::MoveHexByDirUnsafe(raw_hex, 2u);
        }
    }

    if (_mapSize.is_valid_pos(raw_hex)) {
        hex = _mapSize.from_raw_pos(raw_hex);

        if (hex_offset != nullptr) {
            *hex_offset = offset;
        }

        return true;
    }

    return false;
}

auto MapView::GetItemAtScreen(ipos32 screen_pos, bool& item_egg, int32 extra_range, bool check_transparent) -> pair<ItemHexView*, const MapSprite*>
{
    FO_STACK_TRACE_ENTRY();

    vector<pair<ItemHexView*, const MapSprite*>> pix_item;
    vector<pair<ItemHexView*, const MapSprite*>> pix_item_egg;

    const auto pos = ScreenToMapPos(screen_pos);
    const auto is_egg = _engine->SprMngr.IsEggTransp(pos);

    const auto process_sprite = [&](ItemHexView* item, const MapSprite* mspr) {
        const irect32 rect = mspr->GetDrawRect();
        irect32 check_rect = rect;
        check_rect.x -= extra_range;
        check_rect.y -= extra_range;
        check_rect.width += extra_range * 2;
        check_rect.height += extra_range * 2;

        if (pos.x < check_rect.x || pos.x > check_rect.x + check_rect.width || pos.y < check_rect.y || pos.y > check_rect.y + check_rect.height) {
            return;
        }

        if (check_transparent) {
            const Sprite* spr = mspr->GetSprite();
            const ipos32 check_pos = {pos.x - rect.x, pos.y - rect.y};

            if (!_engine->SprMngr.SpriteHitTest(spr, check_pos)) {
                return;
            }
        }

        if (is_egg && _engine->SprMngr.CheckEggAppearence(mspr->GetHex(), mspr->GetEggAppearence())) {
            pix_item_egg.emplace_back(item, mspr);
        }
        else {
            pix_item.emplace_back(item, mspr);
        }
    };

    for (const auto& vf : _viewField) {
        if (!_mapSize.is_valid_pos(vf.RawHex)) {
            continue;
        }

        const auto hex = _mapSize.from_raw_pos(vf.RawHex);
        const auto& field = _hexField->GetCellForReading(hex);

        if (field.Items.empty()) {
            continue;
        }

        auto& field2 = _hexField->GetCellForWriting(hex);

        for (auto& item : field2.OriginItems) {
            if (item->IsMapSpriteVisible()) {
                process_sprite(item.get(), item->GetMapSprite());
            }
        }

        for (auto&& [item, drawable] : field2.MultihexItems) {
            if (drawable && item->HasExtraMapSprites()) {
                for (const auto& extra_mspr_entry : item->GetExtraMapSprites()) {
                    if (extra_mspr_entry.second && extra_mspr_entry.first->GetHex() == hex) {
                        process_sprite(item.get(), extra_mspr_entry.first.get());
                    }
                }
            }
        }
    }

    if (pix_item.empty()) {
        if (pix_item_egg.empty()) {
            return {};
        }

        std::ranges::stable_sort(pix_item_egg, [](auto&& i1, auto&& i2) -> bool { return i1.second->GetSortValue() > i2.second->GetSortValue(); });
        item_egg = true;
        return pix_item_egg.front();
    }
    else {
        std::ranges::stable_sort(pix_item, [](auto&& i1, auto&& i2) -> bool { return i1.second->GetSortValue() > i2.second->GetSortValue(); });
        item_egg = false;
        return pix_item.front();
    }
}

auto MapView::GetCritterAtScreen(ipos32 screen_pos, bool ignore_dead_and_chosen, int32 extra_range, bool check_transparent) -> pair<CritterHexView*, const MapSprite*>
{
    FO_STACK_TRACE_ENTRY();

    vector<CritterHexView*> critters;
    const auto pos = ScreenToMapPos(screen_pos);

    for (auto& cr : _critters) {
        if (!cr->IsMapSpriteVisible() || cr->IsFinishing()) {
            continue;
        }
        if (ignore_dead_and_chosen && (cr->IsDead() || cr->GetIsChosen())) {
            continue;
        }

        const auto* mspr = cr->GetMapSprite();
        const auto view_rect = mspr->GetViewRect();
        const auto l = view_rect.x - extra_range;
        const auto t = view_rect.y - extra_range;
        const auto r = view_rect.x + view_rect.width + extra_range;
        const auto b = view_rect.y + view_rect.height + extra_range;

        if (pos.x >= l && pos.x <= r && pos.y >= t && pos.y <= b) {
            if (check_transparent) {
                const auto draw_rect = mspr->GetDrawRect();

                if (_engine->SprMngr.SpriteHitTest(cr->GetSprite(), {pos.x - draw_rect.x, pos.y - draw_rect.y})) {
                    critters.emplace_back(cr.get());
                }
            }
            else {
                critters.emplace_back(cr.get());
            }
        }
    }

    if (critters.empty()) {
        return {};
    }
    if (critters.size() > 1) {
        std::ranges::stable_sort(critters, [](auto&& cr1, auto&& cr2) -> bool { return cr1->GetMapSprite()->GetSortValue() > cr2->GetMapSprite()->GetSortValue(); });
    }
    return pair(critters.front(), critters.front()->GetMapSprite());
}

auto MapView::GetEntityAtScreen(ipos32 screen_pos, int32 extra_range, bool check_transparent) -> pair<ClientEntity*, const MapSprite*>
{
    FO_STACK_TRACE_ENTRY();

    bool item_egg = false;
    auto item_hit = GetItemAtScreen(screen_pos, item_egg, extra_range, check_transparent);
    auto cr_hit = GetCritterAtScreen(screen_pos, false, extra_range, check_transparent);

    if (cr_hit.first != nullptr && item_hit.first != nullptr) {
        if (item_hit.first->IsTransparent() || item_egg || item_hit.second->GetSortValue() <= cr_hit.second->GetSortValue()) {
            item_hit.first = nullptr;
        }
        else {
            cr_hit.first = nullptr;
        }
    }

    if (cr_hit.first != nullptr) {
        return pair(static_cast<ClientEntity*>(cr_hit.first), cr_hit.second);
    }
    else {
        return pair(static_cast<ClientEntity*>(item_hit.first), item_hit.second);
    }
}

auto MapView::FindPath(CritterHexView* cr, mpos start_hex, mpos& target_hex, int32 cut) -> optional<FindPathResult>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!cr || cr->GetMap() == this);

    if (start_hex == target_hex) {
        return FindPathResult();
    }

    const auto max_path_find_len = _engine->Settings.MaxPathFindLength;
    _findPathGrid.resize((numeric_cast<size_t>(max_path_find_len) * 2 + 2) * (max_path_find_len * 2 + 2));
    MemFill(_findPathGrid.data(), 0, _findPathGrid.size() * sizeof(int16));

    const auto grid_offset = start_hex;
    const auto grid_at = [&](mpos hex) -> int16& { return _findPathGrid[((max_path_find_len + 1) + hex.y - grid_offset.y) * (max_path_find_len * 2 + 2) + ((max_path_find_len + 1) + hex.x - grid_offset.x)]; };

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

        auto p_togo = numeric_cast<int32>(coords.size()) - p;

        if (p_togo == 0) {
            return std::nullopt;
        }

        for (auto i = 0; i < p_togo && !find_ok; ++i, ++p) {
            auto hex = coords[p];

            for (const auto j : iterate_range(GameSettings::MAP_DIR_COUNT)) {
                auto raw_next_hex = ipos32 {hex.x, hex.y};
                GeometryHelper::MoveHexByDirUnsafe(raw_next_hex, static_cast<uint8>(j));

                if (!_mapSize.is_valid_pos(raw_next_hex)) {
                    continue;
                }

                const auto next_hex = _mapSize.from_raw_pos(raw_next_hex);

                if (grid_at(next_hex) != 0) {
                    continue;
                }

                if (multihex == 0) {
                    if (_hexField->GetCellForReading(next_hex).MoveBlocked) {
                        continue;
                    }
                }
                else {
                    // Base hex
                    auto raw_next_hex2 = raw_next_hex;

                    for (int32 k = 0; k < multihex; k++) {
                        GeometryHelper::MoveHexByDirUnsafe(raw_next_hex2, numeric_cast<uint8>(j));
                    }

                    if (!_mapSize.is_valid_pos(raw_next_hex2)) {
                        continue;
                    }
                    if (_hexField->GetCellForReading(_mapSize.from_raw_pos(raw_next_hex2)).MoveBlocked) {
                        continue;
                    }

                    // Clock wise hexes
                    const bool is_square_corner = (j % 2) != 0 && !GameSettings::HEXAGONAL_GEOMETRY;
                    const int32 steps_count = is_square_corner ? multihex * 2 : multihex;
                    bool is_move_blocked = false;

                    {
                        uint8 dir_;

                        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
                            dir_ = (j + 4) % 6;
                        }
                        else {
                            dir_ = (j + 6) % 8;
                        }

                        if (is_square_corner) {
                            dir_ = (dir_ + 1) % 8;
                        }

                        auto raw_next_hex3 = raw_next_hex2;

                        for (int32 k = 0; k < steps_count && !is_move_blocked; k++) {
                            GeometryHelper::MoveHexByDirUnsafe(raw_next_hex3, dir_);
                            FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(raw_next_hex3));
                            is_move_blocked = _hexField->GetCellForReading(_mapSize.from_raw_pos(raw_next_hex3)).MoveBlocked;
                        }
                    }

                    if (is_move_blocked) {
                        continue;
                    }

                    // Counter clock wise hexes
                    {
                        uint8 dir_;

                        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
                            dir_ = (j + 4) % 6;
                        }
                        else {
                            dir_ = (j + 6) % 8;
                        }

                        if (is_square_corner) {
                            dir_ = (dir_ + 7) % 8;
                        }

                        auto raw_next_hex3 = raw_next_hex2;

                        for (int32 k = 0; k < steps_count && !is_move_blocked; k++) {
                            GeometryHelper::MoveHexByDirUnsafe(raw_next_hex3, dir_);
                            FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(raw_next_hex3));
                            is_move_blocked = _hexField->GetCellForReading(_mapSize.from_raw_pos(raw_next_hex3)).MoveBlocked;
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

    int32 x1 = coords.back().x;
    int32 y1 = coords.back().y;

    vector<uint8> raw_steps;
    raw_steps.resize(numindex - 1);

    float32 base_angle = GeometryHelper::GetDirAngle(target_hex, start_hex);

    // From end
    while (numindex > 1) {
        numindex--;

        int32 best_step_dir = -1;
        float32 best_step_angle_diff = 0.0f;

        const auto check_hex = [&best_step_dir, &best_step_angle_diff, numindex, &grid_at, start_hex, base_angle, this](uint8 dir, ipos32 raw_step_hex) {
            if (_mapSize.is_valid_pos(raw_step_hex)) {
                const auto step_hex = _mapSize.from_raw_pos(raw_step_hex);

                if (grid_at(step_hex) == numindex) {
                    const float32 angle = GeometryHelper::GetDirAngle(step_hex, start_hex);
                    const float32 angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);

                    if (best_step_dir == -1 || numindex == 0) {
                        best_step_dir = dir;
                        best_step_angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);
                    }
                    else if (angle_diff < best_step_angle_diff) {
                        best_step_dir = dir;
                        best_step_angle_diff = angle_diff;
                    }
                }
            }
        };

        if ((x1 % 2) != 0) {
            check_hex(3, ipos32 {x1 - 1, y1 - 1});
            check_hex(2, ipos32 {x1, y1 - 1});
            check_hex(5, ipos32 {x1, y1 + 1});
            check_hex(0, ipos32 {x1 + 1, y1});
            check_hex(4, ipos32 {x1 - 1, y1});
            check_hex(1, ipos32 {x1 + 1, y1 - 1});

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
            check_hex(3, ipos32 {x1 - 1, y1});
            check_hex(2, ipos32 {x1, y1 - 1});
            check_hex(5, ipos32 {x1, y1 + 1});
            check_hex(0, ipos32 {x1 + 1, y1 + 1});
            check_hex(4, ipos32 {x1 - 1, y1 + 1});
            check_hex(1, ipos32 {x1 + 1, y1});

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

            for (auto i = numeric_cast<int32>(raw_steps.size()) - 1; i >= 0; i--) {
                LineTracer tracer(trace_hex, trace_target_hex, 0.0f, _mapSize);
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

                result.ControlSteps.emplace_back(numeric_cast<uint16>(result.DirSteps.size()));

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

            result.ControlSteps.emplace_back(numeric_cast<uint16>(result.DirSteps.size()));
        }
    }

    FO_RUNTIME_ASSERT(!result.DirSteps.empty());
    FO_RUNTIME_ASSERT(!result.ControlSteps.empty());

    return {result};
}

bool MapView::CutPath(CritterHexView* cr, mpos start_hex, mpos& target_hex, int32 cut)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!cr || cr->GetMap() == this);

    return !!FindPath(cr, start_hex, target_hex, cut);
}

bool MapView::TraceMoveWay(mpos& start_hex, ipos16& hex_offset, vector<uint8>& dir_steps, int32 quad_dir) const
{
    FO_STACK_TRACE_ENTRY();

    hex_offset = {};

    const auto try_move = [this, &start_hex, &dir_steps](uint8 dir) {
        auto check_hex = start_hex;

        if (!GeometryHelper::MoveHexByDir(check_hex, dir, _mapSize)) {
            return false;
        }

        const auto& field = _hexField->GetCellForReading(check_hex);

        if (field.MoveBlocked) {
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

void MapView::TraceBullet(mpos start_hex, mpos target_hex, int32 dist, float32 angle, vector<CritterHexView*>* critters, CritterFindType find_type, mpos* pre_block_hex, mpos* block_hex, vector<mpos>* hex_steps, bool check_shoot_blocks)
{
    FO_STACK_TRACE_ENTRY();

    if (_isShowTrack) {
        ClearHexTrack();
    }

    const auto check_dist = dist != 0 ? dist : GeometryHelper::GetDistance(start_hex, target_hex);
    auto next_hex = start_hex;
    auto prev_hex = next_hex;

    LineTracer tracer(start_hex, target_hex, angle, _mapSize);

    for (int32 i = 0; i < check_dist; i++) {
        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            tracer.GetNextHex(next_hex);
        }
        else {
            tracer.GetNextSquare(next_hex);
        }

        if (_isShowTrack) {
            GetHexTrack(next_hex) = numeric_cast<int8>(next_hex == target_hex ? 1 : 2);
        }

        if (check_shoot_blocks && _hexField->GetCellForReading(next_hex).ShootBlocked) {
            break;
        }

        if (hex_steps != nullptr) {
            hex_steps->emplace_back(next_hex);
        }

        if (critters != nullptr) {
            auto hex_critters = GetCrittersOnHex(next_hex, find_type);
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

void MapView::InstantScrollTo(mpos center_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_viewField.empty());

    const ipos32 new_screen_hex = ConvertToScreenRawHex(ipos32(center_hex));
    const ipos32 offset_to_new_pos = _engine->Geometry.GetHexOffset(_screenRawHex, new_screen_hex);
    InstantScroll(fpos32(offset_to_new_pos));
}

void MapView::SetShootBorders(bool enabled, int32 dist)
{
    FO_STACK_TRACE_ENTRY();

    if (_drawShootBorders != enabled) {
        _drawShootBorders = enabled;
        _shootBordersDist = dist;
        RebuildFog();
    }
}

auto MapView::AddMapSprite(const Sprite* spr, mpos hex, DrawOrderType draw_order, int32 draw_order_hy_offset, ipos32 offset, const ipos32* poffset, const uint8* palpha, bool* callback) -> MapSprite*
{
    FO_STACK_TRACE_ENTRY();

    auto& field = _hexField->GetCellForWriting(hex);
    auto* mspr = _mapSprites.AddSprite(draw_order, _mapSize.clamp_pos(hex.x, hex.y + draw_order_hy_offset), //
        {(_engine->Settings.MapHexWidth / 2) + offset.x, (_engine->Settings.MapHexHeight / 2) + offset.y}, &field.Offset, spr, nullptr, //
        poffset, palpha, nullptr, callback);
    AddSpriteToChain(field, mspr);
    return mspr;
}

void MapView::OnScreenSizeChanged()
{
    FO_STACK_TRACE_ENTRY();

    _screenSize = {_engine->Settings.ScreenWidth, _engine->Settings.ScreenHeight - _engine->Settings.ScreenHudHeight};
    _viewSize = fsize32(_screenSize) / GetSpritesZoom();

    RebuildMapNow();
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

void MapView::MarkBlockedHexes()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    for (const auto hx : iterate_range(_mapSize.width)) {
        for (const auto hy : iterate_range(_mapSize.height)) {
            const auto& field = _hexField->GetCellForReading({hx, hy});
            auto& track = GetHexTrack({hx, hy});

            track = 0;

            if (field.MoveBlocked) {
                track = 2;
            }
            if (field.ShootBlocked) {
                track = 1;
            }
        }
    }

    RebuildMap();
}

auto MapView::GenTempEntityId() -> ident_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapperMode);

    auto next_id = ident_t {(_workEntityId.underlying_value() + 1)};

    while (_itemsMap.count(next_id) != 0 || _crittersMap.count(next_id) != 0) {
        next_id = ident_t {next_id.underlying_value() + 1};
    }

    _workEntityId = next_id;
    return next_id;
}

auto MapView::SaveToText() const -> string
{
    FO_STACK_TRACE_ENTRY();

    string fomap;
    fomap.reserve(0x1000000); // 1mb

    const auto fill_generic_section = [&fomap](string_view name, const map<string, string>& section) {
        fomap.append("[").append(name).append("]").append("\n");

        for (auto&& [key, value] : section) {
            fomap.append(key).append(" = ").append(value).append("\n");
        }

        fomap.append("\n");
    };

    const auto fill_item_section = [&fomap](string_view name, const map<string, string>& section) {
        fomap.append("[").append(name).append("]").append("\n");

        for (auto&& [key, value] : section) {
            if (key == "MultihexMesh") {
                const auto hex_str = strex(value).split(' ');
                FO_RUNTIME_ASSERT(hex_str.size() % 2 == 0);
                fomap.append(key).append(" = ");

                for (size_t i = 0; i < hex_str.size(); i += 2) {
                    fomap.append(i == 0 ? "\\\n" : " \\\n");
                    fomap.append(" ").append(hex_str[i]).append(" ").append(hex_str[i + 1]);
                }
            }
            else {
                fomap.append(key).append(" = ").append(value);
            }

            fomap.append("\n");
        }

        fomap.append("\n");
    };

    const auto fill_critter = [&fill_generic_section](const CritterView* cr) {
        auto kv = cr->GetProperties().SaveToText(&cr->GetProto()->GetProperties());
        kv["$Id"] = strex("{}", cr->GetId());
        kv["$Proto"] = cr->GetProto()->GetName();
        fill_generic_section("Critter", kv);
    };

    const auto fill_item = [&fill_item_section](const ItemView* item) {
        auto kv = item->GetProperties().SaveToText(&item->GetProto()->GetProperties());
        kv["$Id"] = strex("{}", item->GetId());
        kv["$Proto"] = item->GetProto()->GetName();
        fill_item_section("Item", kv);
    };

    // Header
    fill_generic_section("ProtoMap", GetProperties().SaveToText(nullptr));

    // Critters
    for (const auto& cr : _critters) {
        fill_critter(cr.get());

        for (const auto& inv_item : cr->GetInvItems()) {
            fill_item(inv_item.get());
        }
    }

    // Items
    for (const auto& item : _items) {
        fill_item(item.get());

        for (const auto& inner_item : item->GetInnerItems()) {
            fill_item(inner_item.get());
        }
    }

    return fomap;
}

auto MapView::GetColorDay(const vector<int32>& day_time, const vector<uint8>& colors, int32 game_time, int32* light) -> ucolor
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(day_time.size() == 4);
    FO_RUNTIME_ASSERT(colors.size() == 12);

    uint8 result[3];
    const int32 color_r[4] = {colors[0], colors[1], colors[2], colors[3]};
    const int32 color_g[4] = {colors[4], colors[5], colors[6], colors[7]};
    const int32 color_b[4] = {colors[8], colors[9], colors[10], colors[11]};

    game_time %= 1440;
    int32 time;
    int32 duration;
    if (game_time >= day_time[0] && game_time < day_time[1]) {
        time = 0;
        game_time -= day_time[0];
        duration = day_time[1] - day_time[0];
    }
    else if (game_time >= day_time[1] && game_time < day_time[2]) {
        time = 1;
        game_time -= day_time[1];
        duration = day_time[2] - day_time[1];
    }
    else if (game_time >= day_time[2] && game_time < day_time[3]) {
        time = 2;
        game_time -= day_time[2];
        duration = day_time[3] - day_time[2];
    }
    else {
        time = 3;
        if (game_time >= day_time[3]) {
            game_time -= day_time[3];
        }
        else {
            game_time += 1440 - day_time[3];
        }
        duration = 1440 - day_time[3] + day_time[0];
    }

    if (duration == 0) {
        duration = 1;
    }

    result[0] = numeric_cast<uint8>(color_r[time] + (color_r[time < 3 ? time + 1 : 0] - color_r[time]) * game_time / duration);
    result[1] = numeric_cast<uint8>(color_g[time] + (color_g[time < 3 ? time + 1 : 0] - color_g[time]) * game_time / duration);
    result[2] = numeric_cast<uint8>(color_b[time] + (color_b[time < 3 ? time + 1 : 0] - color_b[time]) * game_time / duration);

    if (light != nullptr) {
        const auto max_light = (std::max({color_r[0], color_r[1], color_r[2], color_r[3]}) + std::max({color_g[0], color_g[1], color_g[2], color_g[3]}) + std::max({color_b[0], color_b[1], color_b[2], color_b[3]})) / 3;
        const auto min_light = (std::min({color_r[0], color_r[1], color_r[2], color_r[3]}) + std::min({color_g[0], color_g[1], color_g[2], color_g[3]}) + std::min({color_b[0], color_b[1], color_b[2], color_b[3]})) / 3;
        const auto cur_light = (result[0] + result[1] + result[2]) / 3;

        *light = GenericUtils::Percent(max_light - min_light, max_light - cur_light);
        *light = std::clamp(*light, 0, 100);
    }

    return ucolor {result[0], result[1], result[2], 255};
}

FO_END_NAMESPACE();
