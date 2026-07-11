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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE

static constexpr int32_t LIGHT_INTENSITY_MAX = 100;
static constexpr int32_t LIGHT_CAPACITY_MAX = 100;
static constexpr int32_t LIGHT_RAW_INTENSITY_MAX = 10000;
static constexpr int32_t LIGHT_HEX_COLOR_MAX = 200;
static constexpr int32_t LIGHT_COLOR_CHANNEL_MAX = 255;
static constexpr float32_t MAP_DEPTH_RANGE_MARGIN = 1000.0f;

void SpritePattern::Finish()
{
    FO_STACK_TRACE_ENTRY();

    if (!Finished) {
        Sprites.clear();
        Finished = true;
    }
}

void FogLayer::Dispose() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    Disposed = true;
}

MapView::MapView(ptr<ClientEngine> engine, ident_t id, ptr<const ProtoMap> proto, isize32 screen_size, nptr<const Properties> props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props ? props : nptr<const Properties> {proto->GetProperties()}, proto->GetProperties()),
    EntityWithProto(proto),
    MapProperties(*GetInitRef())
{
    FO_STACK_TRACE_ENTRY();

    _name = strex("{}_{}", proto->GetName(), id);

    _maxScroll = {GameSettings::MAP_HEX_WIDTH, GameSettings::MAP_HEX_LINE_HEIGHT * 2};
    _screenSize = screen_size;
    _viewSize = fsize32(_screenSize);
    _scrollCheckEnabled = true;

    SetSpritesZoom(1.0f);
    SetSpritesZoomTarget(1.0f);
    RefreshMinZoom();

    SetMapDayColor(ucolor {255, 255, 255, 255});
    SetGlobalDayColor(ucolor {255, 255, 255, 255});

    const auto map_rt_size = isize32(_screenSize.width + GameSettings::MAP_HEX_WIDTH, _screenSize.height + GameSettings::MAP_HEX_LINE_HEIGHT * 2);

    if (!_engine->Settings->MapDirectDraw) {
        _rtMap = _engine->SprMngr.GetRtMngr().CreateRenderTarget(true, map_rt_size, true);
        _rtMap->SetCustomDrawEffect(_engine->EffectMngr.Effects.FlushMap);

        if (!_engine->Settings->DisableIndoorMask) {
            _rtIndoorMask = _engine->SprMngr.GetRtMngr().CreateRenderTarget(false, map_rt_size, false);
        }
    }

    if (!_engine->Settings->DisableLighting || !_engine->Settings->DisableFog) {
        const auto rt_light_size = _engine->Settings->MapDirectDraw ? GetApp()->MainWindow.GetSize() : map_rt_size;
        _rtLight = _engine->SprMngr.GetRtMngr().CreateRenderTarget(false, rt_light_size, true);
    }

    _mapSize = GetSize();
    FO_VERIFY_AND_THROW(_mapSize.width > 0 && _mapSize.height > 0, "Map size must be positive", _mapSize.width, _mapSize.height);

    {
        const ipos32 corners[] = {{0, 0}, {numeric_cast<int32_t>(_mapSize.width) - 1, 0}, //
            {0, numeric_cast<int32_t>(_mapSize.height) - 1}, //
            {numeric_cast<int32_t>(_mapSize.width) - 1, numeric_cast<int32_t>(_mapSize.height) - 1}};
        // Size the depth range from a realistic elevation bound rather than the full int16 domain (+-32767):
        // the latter reserves >90% of the depth buffer for elevations no sprite reaches, crushing precision and
        // the per-layer depth bias (matters now that 2D sprites depth-test with LessEqual). Sprites beyond the
        // bound would be depth-clipped, so it is a generous, tunable Setting.
        const float32_t elev_extent = numeric_cast<float32_t>(std::max(_engine->Settings->MapMaxElevation, 0));
        const float32_t elev_min = -elev_extent;
        const float32_t elev_max = elev_extent;
        float32_t min_depth = std::numeric_limits<float32_t>::max();
        float32_t max_depth = std::numeric_limits<float32_t>::lowest();

        for (const auto corner : corners) {
            for (const float32_t elev : {elev_min, 0.0f, elev_max}) {
                const float32_t d = GeometryHelper::ProjectWorldToMap(GeometryHelper::GetHexWorldPos(corner, ipos32 {}, elev)).z;
                min_depth = std::min(min_depth, d);
                max_depth = std::max(max_depth, d);
            }
        }

        const float32_t near_bound = min_depth - MAP_DEPTH_RANGE_MARGIN;
        const float32_t far_bound = max_depth + MAP_DEPTH_RANGE_MARGIN;
        _mapDepthHalf = std::max(std::abs(near_bound), std::abs(far_bound));
    }

    _hexLight.resize(_mapSize.square() * 3);
    _hexField.emplace(_mapSize);

    _lightPoints.resize(1);

    _screenRawHex = ConvertToScreenRawHex(ipos32(GetWorkHex()));
    InitView();
    RecacheScrollBlocks();

    _eventUnsubscriber += (*_engine->SprMngr.GetWindow()->GetOnScreenSizeChanged()) += [this]() FO_DEFERRED { OnScreenSizeChanged(); };
}

MapView::~MapView()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_CONTINUE(_critters.empty(), "Client map view has critters during destruction", GetId(), _critters.size());
    FO_VERIFY_AND_CONTINUE(_crittersMap.empty(), "Client map view has critters map entries during destruction", GetId(), _crittersMap.size());
    FO_VERIFY_AND_CONTINUE(_items.empty(), "Client map view has items during destruction", GetId(), _items.size());
    FO_VERIFY_AND_CONTINUE(_itemsMap.empty(), "Client map view has item map entries during destruction", GetId(), _itemsMap.size());
    FO_VERIFY_AND_CONTINUE(_staticItems.empty(), "Client map view has static items during destruction", GetId(), _staticItems.size());
    FO_VERIFY_AND_CONTINUE(_dynamicItems.empty(), "Client map view has dynamic items during destruction", GetId(), _dynamicItems.size());
    FO_VERIFY_AND_CONTINUE(_processingItems.empty(), "Client map view has processing items during destruction", GetId(), _processingItems.size());
    FO_VERIFY_AND_CONTINUE(_spritePatterns.empty(), "Client map view has sprite patterns during destruction", GetId(), _spritePatterns.size());
    FO_VERIFY_AND_CONTINUE(!HasFogLayers(), "Client map view has fog layers during destruction", GetId());
    FO_VERIFY_AND_CONTINUE(_visibleLightSources.empty(), "Client map view has visible light sources during destruction", GetId(), _visibleLightSources.size());
    FO_VERIFY_AND_CONTINUE(_lightSources.empty(), "Client map view has light sources during destruction", GetId(), _lightSources.size());
    FO_VERIFY_AND_CONTINUE(!_rtMap, "Client map view still has map render target during destruction", GetId());
    FO_VERIFY_AND_CONTINUE(!_rtLight, "Client map view still has light render target during destruction", GetId());
}

void MapView::OnDestroySelf()
{
    FO_STACK_TRACE_ENTRY();

    _eventUnsubscriber.Unsubscribe();

    for (auto& cr : _critters) {
        safe_call([&] { cr->DestroySelf(); });
    }
    for (auto& item : _items) {
        safe_call([&] { item->DestroySelf(); });
    }

    for (auto& pattern : _spritePatterns) {
        pattern->Finish();
    }

    for (auto& fog_slot : _fogs) {
        for (auto& fog : fog_slot) {
            fog->Disposed = true; // so a script still holding the handle recreates it on the next map
        }

        fog_slot.clear();
    }

    _mapSprites.InvalidateAll();
    _indoorMaskSprites.InvalidateAll();
    _hexField.reset();
    _viewField.clear();
    _fogs = {};
    _visibleLightSources.clear();
    _lightPoints.clear();
    _lightSources.clear();
    _critters.clear();
    _crittersMap.clear();
    _items.clear();
    _staticItems.clear();
    _dynamicItems.clear();
    _processingItems.clear();
    _itemsMap.clear();
    _spritePatterns.clear();

    if (_rtMap) {
        _engine->SprMngr.GetRtMngr().DeleteRenderTarget(_rtMap);
        _rtMap = nullptr;
    }
    if (_rtLight) {
        _engine->SprMngr.GetRtMngr().DeleteRenderTarget(_rtLight);
        _rtLight = nullptr;
    }
}

void MapView::EnableMapperMode()
{
    FO_STACK_TRACE_ENTRY();

    _mapperMode = true;
    _scrollCheckEnabled = false;

    RefreshMinZoom();
}

void MapView::LoadFromFile(string_view map_name, const string& str)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper mode is not selected");

    _mapLoading = true;
    auto reset_loading = scope_exit([&]() noexcept { _mapLoading = false; });
    auto max_id = _workEntityId.underlying_value();

    MapLoader::Load(
        map_name, str, *_engine, _engine->Hashes,
        [this, &max_id, map_name](ident_t id, ptr<const ProtoCritter> proto, ptr<const map<string_view, string_view>> kv) {
            FO_VERIFY_AND_THROW(id, "Critter id is empty");
            FO_VERIFY_AND_THROW(_crittersMap.count(id) == 0, "Map file contains duplicate critter id", map_name, id, proto->GetProtoId());

            max_id = std::max(max_id, id.underlying_value());

            auto props = proto->GetProperties()->Copy();
            props.ApplyFromText(*kv);

            auto props_ptr = make_nptr(&props);
            auto cr = SafeAlloc::MakeRefCounted<CritterHexView>(this, id, proto, props_ptr);

            if (const auto hex = cr->GetHex(); !_mapSize.is_valid_pos(hex)) {
                cr->SetHex(_mapSize.clamp_pos(hex));
            }

            AddCritterInternal(cr);
        },
        [this, &max_id, map_name](ident_t id, ptr<const ProtoItem> proto, ptr<const map<string_view, string_view>> kv) {
            FO_VERIFY_AND_THROW(id, "Item id is empty");
            FO_VERIFY_AND_THROW(_itemsMap.count(id) == 0, "Map file contains duplicate item id", map_name, id, proto->GetProtoId());

            max_id = std::max(max_id, id.underlying_value());

            auto props = proto->GetProperties()->Copy();
            props.ApplyFromText(*kv);

            auto props_ptr = make_nptr(&props);
            auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, id, proto, props_ptr);

            if (item->GetOwnership() == ItemOwnership::MapHex) {
                if (const auto hex = item->GetHex(); !_mapSize.is_valid_pos(hex)) {
                    item->SetHex(_mapSize.clamp_pos(hex));
                }

                AddItemInternal(item);
            }
            else if (item->GetOwnership() == ItemOwnership::CritterInventory) {
                auto cr = GetCritter(item->GetCritterId());

                if (!cr) {
                    throw GenericException("Critter not found", item->GetCritterId());
                }

                cr->AddRawInvItem(item);
            }
            else if (item->GetOwnership() == ItemOwnership::ItemContainer) {
                auto cont = GetItem(item->GetContainerId());

                if (!cont) {
                    throw GenericException("Container not found", item->GetContainerId());
                }

                cont->AddRawInnerItem(item);
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

    auto reader = DataReader(file.GetDataSpan());

    // Hashes
    {
        const auto hashes_count = reader.Read<uint32_t>();

        string str;

        for (uint32_t i = 0; i < hashes_count; i++) {
            const auto str_len = reader.Read<uint32_t>();
            str.resize(str_len);
            reader.ReadStringBytes(str);
            const auto hstr = _engine->Hashes.ToHashedString(str);
            ignore_unused(hstr);
        }
    }

    // Read static items
    {
        _mapLoading = true;
        auto reset_loading = scope_exit([this]() noexcept { _mapLoading = false; });

        const auto items_count = reader.Read<uint32_t>();

        _items.reserve(items_count);
        _staticItems.reserve(items_count);
        _processingItems.reserve(256);

        vector<uint8_t> props_data;

        for (uint32_t i = 0; i < items_count; i++) {
            const auto static_id = ident_t {reader.Read<ident_t::underlying_type>()};
            const auto item_pid_hash = reader.Read<hstring::hash_t>();
            const auto item_pid = _engine->Hashes.ResolveHash(item_pid_hash);
            auto item_proto = _engine->GetProtoItem(item_pid);
            FO_VERIFY_AND_THROW(item_proto, "Missing required item prototype");

            auto item_props = Properties(item_proto->GetProperties()->GetRegistrator());
            const auto props_data_size = reader.Read<uint32_t>();
            props_data.resize(props_data_size);
            span<uint8_t> props_data_span = props_data;
            reader.ReadBytes(props_data_span);
            item_props.RestoreAllData(props_data);

            auto item_props_ptr = make_nptr(&item_props);
            auto static_item = SafeAlloc::MakeRefCounted<ItemHexView>(this, static_id, item_proto.as_ptr(), item_props_ptr);
            static_item->SetStatic(true);
            AddItemInternal(static_item);
        }
    }

    reader.VerifyEnd();

    // Index roof
    const auto mark_roof_num = [this](ipos32 raw_hex, int32_t num) {
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

            if (field.RoofNum != 0 || !field.HasRoof) {
                continue;
            }

            _hexField->GetCellForWriting(next_hex)->RoofNum = num;

            next_raw_hexes.emplace(next_raw_hex.x + _engine->Settings->MapTileStep, next_raw_hex.y);
            next_raw_hexes.emplace(next_raw_hex.x - _engine->Settings->MapTileStep, next_raw_hex.y);
            next_raw_hexes.emplace(next_raw_hex.x, next_raw_hex.y + _engine->Settings->MapTileStep);
            next_raw_hexes.emplace(next_raw_hex.x, next_raw_hex.y - _engine->Settings->MapTileStep);
        }
    };

    int32_t roof_num = 1;

    for (const auto hx : iterate_range(_mapSize.width)) {
        for (const auto hy : iterate_range(_mapSize.height)) {
            const auto& field = _hexField->GetCellForReading(mpos(hx, hy));

            if (field.RoofNum == 0 && field.HasRoof) {
                const auto corrected_hx = hx - hx % _engine->Settings->MapTileStep;
                const auto corrected_hy = hy - hy % _engine->Settings->MapTileStep;
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

    // Critters
    {
        _critterToDeleteScratch.clear();

        for (size_t i = 0; i < _critters.size(); i++) {
            auto cr = _critters[i].as_ptr();

            cr->Process();

            if (cr->IsFinished()) {
                _critterToDeleteScratch.emplace_back(cr);
            }
        }

        for (ptr<CritterHexView> cr : _critterToDeleteScratch) {
            DestroyCritter(cr);
        }

        _critterToDeleteScratch.clear();
    }

    // Items
    {
        _itemToDeleteScratch.clear();

        for (ptr<ItemHexView> item : _processingItems) {
            if (item->IsNeedProcess()) {
                item->Process();
            }

            if (item->IsFinished()) {
                _itemToDeleteScratch.emplace_back(item);
            }
        }

        for (ptr<ItemHexView> item : _itemToDeleteScratch) {
            DestroyItem(item);
        }

        _itemToDeleteScratch.clear();
    }

    // Scroll and zoom
    {
        const auto fixed_dt = timespan(std::chrono::milliseconds(_engine->Settings->ScrollFixedDt));
        _scrollDtAccum += _engine->GameTime.GetFrameDeltaTime();

        if (_scrollDtAccum >= fixed_dt) {
            _scrollDtAccum = std::min(_scrollDtAccum, timespan(fixed_dt.value() * 10));
            _scrollDtAccum -= fixed_dt;
            ProcessZoom(fixed_dt.to_ms<float32_t>());
            ProcessScroll(fixed_dt.to_ms<float32_t>());
        }
    }
}

auto MapView::GetViewSize() const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    const auto zoom = GetSpritesZoom();
    const auto screen_hexes_width = _screenSize.width / GameSettings::MAP_HEX_WIDTH + ((_screenSize.width % GameSettings::MAP_HEX_WIDTH) != 0 ? 1 : 0);
    const auto screen_hexes_height = _screenSize.height / GameSettings::MAP_HEX_LINE_HEIGHT + ((_screenSize.height % GameSettings::MAP_HEX_LINE_HEIGHT) != 0 ? 1 : 0);
    const auto view_hexes_width = is_float_equal(zoom, 1.0f) ? screen_hexes_width : iround<int32_t>(std::ceil(numeric_cast<float32_t>(screen_hexes_width) / zoom));
    const auto view_hexes_height = is_float_equal(zoom, 1.0f) ? screen_hexes_height : iround<int32_t>(std::ceil(numeric_cast<float32_t>(screen_hexes_height) / zoom));

    return {view_hexes_width, view_hexes_height};
}

void MapView::AddItemToField(ptr<ItemHexView> item)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = item->GetHex();
    auto field = _hexField->GetCellForWriting(hex);

    vec_add_unique_value(field->OriginItems, item);
    vec_add_unique_value(field->Items, item);
    RecacheHexFlags(field);

    if (item->IsNonEmptyMultihexLines() || item->IsNonEmptyMultihexMesh()) {
        vector<mpos> multihex_entries;
        const auto multihex_lines = item->GetMultihexLines();
        const auto multihex_mesh = item->GetMultihexMesh();
        multihex_entries.reserve((1 + multihex_mesh.size()) * (1 + multihex_lines.size() / 2));

        GeometryHelper::ForEachMultihexLines(multihex_lines, hex, _mapSize, [&](mpos multihex) {
            auto multihex_field = _hexField->GetCellForWriting(multihex);

            if (vec_safe_add_unique_value(multihex_field->Items, item)) {
                vec_add_unique_value(multihex_field->MultihexItems, pair(item, item->GetDrawMultihexLines()));
                RecacheHexFlags(multihex_field);
                multihex_entries.emplace_back(multihex);
            }
        });

        for (const auto multihex : multihex_mesh) {
            if (multihex != hex && _mapSize.is_valid_pos(multihex)) {
                auto multihex_field = _hexField->GetCellForWriting(multihex);

                if (vec_safe_add_unique_value(multihex_field->Items, item)) {
                    vec_add_unique_value(multihex_field->MultihexItems, pair(item, item->GetDrawMultihexMesh()));
                    RecacheHexFlags(multihex_field);
                    multihex_entries.emplace_back(multihex);
                }

                if (item->IsNonEmptyMultihexLines()) {
                    GeometryHelper::ForEachMultihexLines(multihex_lines, multihex, _mapSize, [&](mpos multihex2) {
                        auto multihex_field2 = _hexField->GetCellForWriting(multihex2);

                        if (vec_safe_add_unique_value(multihex_field2->Items, item)) {
                            vec_add_unique_value(multihex_field2->MultihexItems, pair(item, item->GetDrawMultihexLines()));
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

    auto item_spr = item->GetSprite();
    FO_VERIFY_AND_THROW(item_spr, "Item is missing its sprite");
    if (!MeasureMapBorders(item_spr, item->GetSpriteOffset()) && !_mapLoading) {
        if (IsHexToDraw(hex)) {
            DrawHexItem(item, field, hex, false);
        }

        if (item->HasMultihexEntries() && (item->GetDrawMultihexLines() || item->GetDrawMultihexMesh())) {
            auto multihex_entries = item->GetMultihexEntries();
            FO_VERIFY_AND_THROW(multihex_entries, "Multihex entries collection is null");

            for (const auto multihex : *multihex_entries) {
                if (auto multihex_field = _hexField->GetCellForWriting(multihex); multihex_field->IsView) {
                    for (auto&& [multihex_item, drawable] : multihex_field->MultihexItems) {
                        if (drawable && multihex_item == item) {
                            DrawHexItem(item, multihex_field, multihex, true);
                        }
                    }
                }
            }
        }
    }
}

void MapView::RemoveItemFromField(ptr<ItemHexView> item)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = item->GetHex();
    auto field = _hexField->GetCellForWriting(hex);

    vec_remove_unique_value(field->OriginItems, item);
    vec_remove_unique_value(field->Items, item);
    RecacheHexFlags(field);

    if (item->HasMultihexEntries()) {
        auto multihex_entries = item->GetMultihexEntries();
        FO_VERIFY_AND_THROW(multihex_entries, "Multihex entries collection is null");

        for (const auto multihex : *multihex_entries) {
            auto multihex_field = _hexField->GetCellForWriting(multihex);
            vec_remove_unique_value(multihex_field->Items, item);
            vec_remove_unique_value_if(multihex_field->MultihexItems, [item](auto&& i) { return i.first == item; });
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

void MapView::DrawHexItem(ptr<ItemHexView> item, ptr<Field> field, mpos hex, bool extra_draw)
{
    FO_STACK_TRACE_ENTRY();

    if (_mapperMode) {
        const auto is_fast = _fastPids.count(item->GetProtoId()) != 0;

        if (!_isShowMapperHiddenSprites && item->GetAlwaysHideSprite()) {
            return;
        }
        if (!_engine->Settings->ShowScen && !is_fast && item->GetIsScenery()) {
            return;
        }
        if (!_engine->Settings->ShowItem && !is_fast && !item->GetIsScenery() && !item->GetIsWall()) {
            return;
        }
        if (!_engine->Settings->ShowWall && !is_fast && item->GetIsWall()) {
            return;
        }
        if (!_engine->Settings->ShowTile && item->GetIsTile() && !item->GetIsRoofTile()) {
            return;
        }
        if (!_engine->Settings->ShowRoof && item->GetIsTile() && item->GetIsRoofTile()) {
            return;
        }
        if (!_engine->Settings->ShowFast && is_fast) {
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
    const bool hidden_roof_for_indoor_mask = is_roof && _hiddenRoofNum != 0 && _hiddenRoofNum == field->RoofNum;
    MapSpriteList& target_sprites = hidden_roof_for_indoor_mask ? _indoorMaskSprites : _mapSprites;

    DrawOrderType draw_order;

    if (item->GetIsTile()) {
        if (item->GetIsRoofTile()) {
            draw_order = static_cast<DrawOrderType>(static_cast<int32_t>(DrawOrderType::Roof) + item->GetTileLayer());
        }
        else {
            draw_order = static_cast<DrawOrderType>(static_cast<int32_t>(DrawOrderType::Tile) + item->GetTileLayer());
        }
    }
    else {
        if (item->GetDrawFlatten()) {
            draw_order = item->GetStatic() ? DrawOrderType::FlatItemPreLight : DrawOrderType::FlatItemAfterLight;
        }
        else {
            draw_order = DrawOrderType::Item;
        }
    }

    const auto draw_hex = _mapSize.clamp_pos(hex.x, hex.y + item->GetDrawOrderOffsetHexY());
    auto mspr = !extra_draw ? item->AddSprite(target_sprites, draw_order, draw_hex, &field->Offset) : item->AddExtraSprite(target_sprites, draw_order, draw_hex, &field->Offset);

    AddSpriteToChain(field, mspr);

    if (is_roof) {
        mspr->SetEggAppearence(EggAppearenceType::Always);
    }
}

auto MapView::AddReceivedItem(ident_t id, hstring pid, mpos hex, const vector<vector<uint8_t>>& data, bool fade_in) -> ptr<ItemHexView>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(id, "Received client map item has an empty entity id", GetId(), pid, hex);
    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Received client map item targets a hex outside map bounds", GetId(), id, pid, hex, _mapSize);

    auto proto = _engine->GetProtoItem(pid);
    FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

    auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, id, proto.as_ptr());

    item->RestoreData(data);
    item->SetStatic(false);
    item->SetHex(hex);

    if (!item->GetShootThru()) {
        RebuildFog();
    }

    const bool was_present = !!GetItem(id);

    auto added = AddItemInternal(item);

    if (fade_in && !was_present) {
        added->FadeUp();
    }

    return added;
}

auto MapView::AddMapperItem(hstring pid, mpos hex, nptr<const Properties> props, ident_t id) -> ptr<ItemHexView>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper item creation was requested outside mapper mode", GetId(), pid, hex);
    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Mapper item creation targets a hex outside map bounds", GetId(), pid, hex, _mapSize);

    auto proto = _engine->GetProtoItem(pid);
    FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

    auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, id ? id : GenTempEntityId(), proto.as_ptr(), props);

    item->SetHex(hex);

    return AddItemInternal(item);
}

auto MapView::AddMapperTile(hstring pid, mpos hex, uint8_t layer, bool is_roof) -> ptr<ItemHexView>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper tile creation was requested outside mapper mode", GetId(), pid, hex, layer, is_roof);
    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Mapper tile creation targets a hex outside map bounds", GetId(), pid, hex, layer, is_roof, _mapSize);

    auto proto = _engine->GetProtoItem(pid);
    FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

    auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, GenTempEntityId(), proto.as_ptr());

    item->SetHex(hex);
    item->SetIsTile(true);
    item->SetIsRoofTile(is_roof);
    item->SetTileLayer(layer);

    return AddItemInternal(item);
}

auto MapView::AddLocalItem(hstring pid, mpos hex) -> ptr<ItemHexView>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Local client map item creation targets a hex outside map bounds", GetId(), pid, hex, _mapSize);

    auto proto = _engine->GetProtoItem(pid);
    FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

    auto item = SafeAlloc::MakeRefCounted<ItemHexView>(this, ident_t {}, proto.as_ptr());

    item->SetStatic(false);
    item->SetHex(hex);

    return AddItemInternal(item);
}

auto MapView::AddItemInternal(ptr<ItemHexView> item) -> ptr<ItemHexView>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(item->GetHex()), "Item hex is outside client map bounds while adding item", item->GetId(), item->GetHex(), _mapSize);
    FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::MapHex, "Client map item has an unexpected ownership mode while being added to a map field", GetId(), item->GetId(), item->GetProtoId(), item->GetOwnership());

    if (item->GetId()) {
        if (auto prev_item = GetItem(item->GetId())) {
            item->InheritAlphaFrom(prev_item);
            DestroyItem(prev_item);
        }
    }

    item->SetMapId(GetId());
    item->Init();

    _items.emplace_back(item.hold_ref());

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

void MapView::RefreshItem(ptr<ItemHexView> item, bool deferred)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(item->GetMap() == this, "Item refresh requested for an item attached to a different client map", item->GetId(), item->GetMap()->GetId(), GetId());

    if (deferred) {
        _deferredRefreshItems.emplace(item.hold_ref());
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
            RefreshItem(item, false);
        }
    }

    _deferredRefreshItems.clear();
}

void MapView::MoveItem(ptr<ItemHexView> item, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(item->GetMap() == this, "Item move requested for an item attached to a different client map", item->GetId(), item->GetMap()->GetId(), GetId());
    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Item move target hex is outside client map bounds", item->GetId(), hex, _mapSize);

    RemoveItemFromField(item);
    item->SetHex(hex);
    AddItemToField(item);
}

void MapView::DestroyItem(ptr<ItemHexView> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(item->GetMap() == this, "Item destroy requested for an item attached to a different client map", item->GetId(), item->GetMap()->GetId(), GetId());

    auto item_ref_holder = item.hold_ref();

    vec_remove_unique_value(_items, item_ref_holder);

    if (item->GetStatic()) {
        vec_remove_unique_value(_staticItems, item);
    }
    else {
        vec_remove_unique_value(_dynamicItems, item);
    }

    vec_safe_remove_unique_value(_processingItems, item);

    if (item->GetId()) {
        const auto it = _itemsMap.find(item->GetId());
        FO_VERIFY_AND_THROW(it != _itemsMap.end(), "Client map item lookup is missing during item destruction", GetId(), item->GetId(), _itemsMap.size());
        _itemsMap.erase(it);
    }

    RemoveItemFromField(item);
    CleanLightSourceOffsets(item->GetId());
    item->DestroySelf();
}

void MapView::DestroyItems(const_span<ptr<ItemHexView>> items)
{
    FO_STACK_TRACE_ENTRY();

    if (items.empty()) {
        return;
    }

    unordered_set<ptr<const ItemHexView>> to_destroy;
    vector<ptr<ItemHexView>> unique_items;
    to_destroy.reserve(items.size());
    unique_items.reserve(items.size());

    for (ptr<ItemHexView> item : items) {
        FO_VERIFY_AND_THROW(item->GetMap() == this, "Item destroy requested for an item attached to a different client map", item->GetId(), item->GetMap()->GetId(), GetId());

        if (to_destroy.emplace(item).second) {
            unique_items.emplace_back(item);
        }
    }

    vector<refcount_ptr<ItemHexView>> ref_holders;
    ref_holders.reserve(unique_items.size());

    for (ptr<ItemHexView> item : unique_items) {
        ref_holders.emplace_back(item.hold_ref());
    }

    std::erase_if(_items, [&](const refcount_ptr<ItemHexView>& i) { return to_destroy.contains(i.get()); });
    std::erase_if(_staticItems, [&](const ptr<ItemHexView>& i) { return to_destroy.contains(i.get()); });
    std::erase_if(_dynamicItems, [&](const ptr<ItemHexView>& i) { return to_destroy.contains(i.get()); });
    std::erase_if(_processingItems, [&](const ptr<ItemHexView>& i) { return to_destroy.contains(i.get()); });

    for (ptr<ItemHexView> item : unique_items) {
        if (item->GetId()) {
            const auto it = _itemsMap.find(item->GetId());
            FO_VERIFY_AND_THROW(it != _itemsMap.end(), "Client map item lookup is missing during item destruction", GetId(), item->GetId(), _itemsMap.size());
            _itemsMap.erase(it);
        }

        RemoveItemFromField(item);
        CleanLightSourceOffsets(item->GetId());
        item->DestroySelf();
    }
}

auto MapView::GetItem(ident_t id) -> nptr<ItemHexView>
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = _itemsMap.find(id); it != _itemsMap.end()) {
        return it->second;
    }

    return nullptr;
}

auto MapView::GetItemOnHex(mpos hex) -> nptr<ItemHexView>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Client map item lookup requested a hex outside map bounds", GetId(), hex, _mapSize);
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return nullptr;
    }

    auto field2 = _hexField->GetCellForWriting(hex);
    return field2->Items.front();
}

auto MapView::GetItemOnHex(mpos hex, hstring pid) -> nptr<ItemHexView>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Client map item lookup by proto requested a hex outside map bounds", GetId(), pid, hex, _mapSize);
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return nullptr;
    }

    auto field2 = _hexField->GetCellForWriting(hex);

    for (ptr<ItemHexView> item : field2->Items) {
        if (item->GetProtoId() == pid) {
            return item;
        }
    }

    return nullptr;
}

auto MapView::GetItemsOnHex(mpos hex) -> span<ptr<ItemHexView>>
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return {};
    }

    auto field2 = _hexField->GetCellForWriting(hex);
    return field2->Items;
}

auto MapView::GetItemsOnHex(mpos hex) const -> span<const ptr<ItemHexView>>
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
            for (ptr<const CritterHexView> cr : field.Critters) {
                if (cr->IsMapSpriteVisible()) {
                    const auto rect = cr->GetViewRect();
                    result = result.has_value() ? result->expanded(rect) : rect;
                }
            }
        }

        if (!field.Items.empty()) {
            for (ptr<const ItemHexView> item : field.Items) {
                if (item->IsMapSpriteVisible()) {
                    auto spr = item->GetSprite();

                    if (spr) {
                        const auto x = field.Offset.x + GameSettings::MAP_HEX_WIDTH / 2 - spr->GetOffset().x;
                        const auto y = field.Offset.y + GameSettings::MAP_HEX_HEIGHT / 2 - spr->GetOffset().y;
                        const auto rect = irect32(ipos32(x, y), spr->GetSize());
                        result = result.has_value() ? result->expanded(rect) : rect;
                    }
                }
            }
        }
    }

    return result.has_value() ? result.value().size() : isize32();
}

auto MapView::RunSpritePattern(string_view name, size_t count) -> nptr<SpritePattern>
{
    FO_STACK_TRACE_ENTRY();

    auto spr = _engine->SprMngr.LoadSprite(name, AtlasType::MapSprites);

    if (!spr) {
        return nullptr;
    }

    spr->Prewarm();
    spr->PlayDefault();

    auto pattern = SafeAlloc::MakeRefCounted<SpritePattern>();

    pattern->Sprites.emplace_back(std::move(spr));

    for (size_t i = 1; i < count; i++) {
        auto next_spr = _engine->SprMngr.LoadSprite(name, AtlasType::MapSprites);

        next_spr->Prewarm();
        next_spr->PlayDefault();

        pattern->Sprites.emplace_back(std::move(next_spr));
    }

    _spritePatterns.emplace_back(pattern);
    return pattern;
}

void MapView::RebuildMapNow()
{
    FO_STACK_TRACE_ENTRY();

    HideHexLines(0, _hVisible);
    FO_VERIFY_AND_THROW(!_mapSprites.HasActiveSprites(), "Map sprites must be cleared before reinitializing the view");
    FO_VERIFY_AND_THROW(!_indoorMaskSprites.HasActiveSprites(), "Indoor mask sprites must be cleared before reinitializing the view");
    FO_VERIFY_AND_THROW(_visibleLightSources.empty(), "Visible light sources must be empty before this operation");

    InitView();

    ShowHexLines(0, _hVisible);

    _rebuildMap = false;
    _engine->SprMngr.InvalidateEgg();
    _needRebuildLightPrimitives = true;
    _needReapplyLights = true;
    _engine->OnRenderMap_Rebuild.Fire(this);
}

void MapView::RebuildMapOffset(ipos32 axial_hex_offset)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_viewField.empty(), "Client map view-field is empty while rebuilding map offset", GetId(), _mapSize, _wVisible, _hVisible);
    FO_VERIFY_AND_THROW(std::abs(axial_hex_offset.y) % 2 == 0, "Client map axial vertical offset must stay aligned to two raw rows", GetId(), axial_hex_offset, _screenRawHex);

    const auto ox = axial_hex_offset.x;
    const auto oy = axial_hex_offset.y;

    // Hide opposite lines
    HideHexLines(-ox, -oy);

    // Shift view position
    mdir shift_ox_dir;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        shift_ox_dir = ox > 0 ? hdir::East : hdir::West;
    }
    else {
        shift_ox_dir = ox > 0 ? hdir::East : hdir::NorthWest;
    }

    mdir shift_oy_dir1;
    mdir shift_oy_dir2;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        shift_oy_dir1 = oy > 0 ? hdir::SouthEast : hdir::NorthEast;
        shift_oy_dir2 = oy > 0 ? hdir::SouthWest : hdir::NorthWest;
    }
    else {
        shift_oy_dir1 = oy > 0 ? hdir::SouthEast : hdir::NorthEast;
        shift_oy_dir2 = oy > 0 ? hdir::West : hdir::NorthWest;
    }

    if (ox != 0) {
        for (int32_t i = 0; i < std::abs(ox); i++) {
            GeometryHelper::MoveHexByDirUnsafe(_screenRawHex, shift_ox_dir);
        }
    }
    if (oy != 0) {
        for (int32_t i = 0; i < std::abs(oy) / 2; i++) {
            GeometryHelper::MoveHexByDirUnsafe(_screenRawHex, shift_oy_dir1);
            GeometryHelper::MoveHexByDirUnsafe(_screenRawHex, shift_oy_dir2);
        }
    }

    for (auto& vf : _viewField) {
        for (int32_t i = 0; i < std::abs(ox); i++) {
            GeometryHelper::MoveHexByDirUnsafe(vf.RawHex, shift_ox_dir);
        }
        for (int32_t i = 0; i < std::abs(oy) / 2; i++) {
            GeometryHelper::MoveHexByDirUnsafe(vf.RawHex, shift_oy_dir1);
            GeometryHelper::MoveHexByDirUnsafe(vf.RawHex, shift_oy_dir2);
        }

        if (_mapSize.is_valid_pos(vf.RawHex)) {
            const auto hex = _mapSize.from_raw_pos(vf.RawHex);
            auto field = _hexField->GetCellForWriting(hex);
            field->Offset = vf.Offset;
        }
    }

    // Show new lines
    ShowHexLines(ox, oy);

    // Critters text rect
    for (size_t i = 0; i < _critters.size(); i++) {
        _critters[i]->RefreshOffs();
    }

    _needRebuildLightPrimitives = true;
    _engine->OnRenderMap_Rebuild.Fire(this);
}

void MapView::ShowHexLines(int ox, int oy)
{
    FO_STACK_TRACE_ENTRY();

    // Show vertical line
    if (ox != 0) {
        const auto from_x = ox > 0 ? std::max(_wVisible - ox, 0) : 0;
        const auto to_x = ox > 0 ? _wVisible : std::min(-ox, _wVisible);
        FO_VERIFY_AND_THROW(from_x < to_x, "Client map show-column range is empty after scroll offset calculation", GetId(), ox, _wVisible, from_x, to_x);

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
        FO_VERIFY_AND_THROW(from_y < to_y, "Client map show-row range is empty after scroll offset calculation", GetId(), oy, _hVisible, from_y, to_y);

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
        FO_VERIFY_AND_THROW(from_x < to_x, "Client map hide-column range is empty after scroll offset calculation", GetId(), ox, _wVisible, from_x, to_x);

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
        FO_VERIFY_AND_THROW(from_y < to_y, "Client map hide-row range is empty after scroll offset calculation", GetId(), oy, _hVisible, from_y, to_y);

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

    auto field = _hexField->GetCellForWriting(hex);
    field->IsView = true;
    field->Offset = vf.Offset;

    // Lighting
    if (!field->LightSources.empty()) {
        for (ptr<LightSource> ls : field->LightSources | std::views::keys) {
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

    // Items on hex
    if (!field->OriginItems.empty()) {
        for (ptr<ItemHexView> item : field->OriginItems) {
            DrawHexItem(item, field, hex, false);
        }
    }

    // Multihex items
    if (!field->MultihexItems.empty()) {
        for (auto&& [item, drawable] : field->MultihexItems) {
            if (drawable) {
                DrawHexItem(item, field, hex, true);
            }
        }
    }

    // Critters
    if (!field->OriginCritters.empty()) {
        for (ptr<CritterHexView> cr : field->OriginCritters) {
            DrawHexCritter(cr, field, hex);
        }
    }

    // Patterns
    if (!_spritePatterns.empty()) {
        for (auto it = _spritePatterns.begin(); it != _spritePatterns.end();) {
            auto pattern = it->as_ptr();

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

            if (pattern->InteractWithRoof && _hiddenRoofNum != 0 && _hiddenRoofNum == field->RoofNum) {
                continue;
            }

            if (pattern->CheckTileProperty) {
                if (!field->GroundTile) {
                    continue;
                }
                if (field->GroundTile->GetValueAsInt(static_cast<int32_t>(pattern->TileProperty)) != pattern->ExpectedTilePropertyValue) {
                    continue;
                }
            }
            else {
                continue;
            }

            auto spr = pattern->Sprites.at((hex.y * (pattern->Sprites.size() / 5) + hex.x) % pattern->Sprites.size()).as_nptr();
            const bool on_roof = pattern->InteractWithRoof && field->RoofNum != 0;
            ptr<MapSprite> mspr = _mapSprites.AddSprite(on_roof ? DrawOrderType::RoofParticles : DrawOrderType::Particles, hex, //
                {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2}, &field->Offset, //
                spr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

            if (on_roof) {
                mspr->SetElevation(numeric_cast<int16_t>(_engine->Settings->MapRoofElevation));
            }

            AddSpriteToChain(field, mspr);
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

    auto field = _hexField->GetCellForWriting(hex);
    field->IsView = false;

    InvalidateSpriteChain(field);

    // Lighting
    if (!field->LightSources.empty()) {
        for (ptr<LightSource> ls : field->LightSources | std::views::keys) {
            const auto it = _visibleLightSources.find(ls);

            FO_VERIFY_AND_THROW(it != _visibleLightSources.end(), "Lookup failed in visible light sources");
            FO_VERIFY_AND_THROW(it->second > 0, "Visible light source reference count must be positive");

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

    if (_engine->Settings->DisableLighting) {
        return;
    }

    if (_needReapplyLights) {
        _needReapplyLights = false;

        for (auto&& [ls, count] : copy(_visibleLightSources)) {
            ApplyLightFan(ls);
        }
    }

    _reapplyLightSourcesScratch.clear();
    _removeLightSourcesScratch.clear();

    for (ptr<LightSource> ls : _visibleLightSources | std::views::keys) {
        const int32_t prev_intensity = ls->CurIntensity;

        if (ls->CurIntensity != ls->TargetIntensity) {
            const auto elapsed_time = (_engine->GameTime.GetFrameTime() - ls->Time).div<float32_t>(std::chrono::milliseconds {200});
            ls->CurIntensity = lerp(ls->StartIntensity, ls->TargetIntensity, std::clamp(elapsed_time, 0.0f, 1.0f));
        }

        if (ls->Finishing && ls->CurIntensity == 0) {
            _removeLightSourcesScratch.emplace_back(ls);
        }
        else if (ls->CurIntensity != prev_intensity || ls->NeedReapply) {
            _reapplyLightSourcesScratch.emplace_back(ls);
        }
    }

    for (ptr<LightSource> ls : _reapplyLightSourcesScratch) {
        ApplyLightFan(ls);
    }

    for (ptr<LightSource> ls : _removeLightSourcesScratch) {
        FO_VERIFY_AND_THROW(_lightSources.count(ls->Id) != 0, "Client map light source removal is missing the source in the active light map", GetId(), ls->Id, _lightSources.size(), _removeLightSourcesScratch.size());

        CleanLightFan(ls);
        _lightSources.erase(ls->Id);
    }

    _reapplyLightSourcesScratch.clear();
    _removeLightSourcesScratch.clear();

    if (_needRebuildLightPrimitives) {
        _needRebuildLightPrimitives = false;

        for (auto& points : _lightPoints) {
            points.clear();
        }

        size_t cur_points = 0;

        for (ptr<const LightSource> ls : _visibleLightSources | std::views::keys) {
            FO_VERIFY_AND_THROW(ls->Applied, "Light source is not applied to the map view");

            // Split large entries to fit into 16-bit index
            if constexpr (sizeof(vindex_t) == 2) {
                while (_lightPoints[cur_points].size() > 0x7FFF) {
                    cur_points++;

                    if (cur_points >= _lightPoints.size()) {
                        _lightPoints.emplace_back();
                        break;
                    }
                }
            }

            LightFanToPrimitves(ls, _lightPoints[cur_points]);
        }
    }
}

void MapView::UpdateCritterLightSource(ptr<const CritterHexView> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(cr->GetMap() == this, "Critter light update requested for a critter attached to a different client map", cr->GetId(), cr->GetMap()->GetId(), GetId());

    if (_engine->Settings->DisableLighting) {
        return;
    }

    if (cr->GetLightSource()) {
        UpdateLightSource(cr->GetId(), cr->GetHex(), cr->GetLightColor(), cr->GetLightDistance(), static_cast<LightFlag>(cr->GetLightFlags()), cr->GetLightIntensity(), cr->GetSpriteOffsetPtr());
    }
    else {
        FinishLightSource(cr->GetId());
    }
}

void MapView::UpdateItemLightSource(ptr<const ItemHexView> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(item->GetMap() == this, "Item light update requested for an item attached to a different client map", item->GetId(), item->GetMap()->GetId(), GetId());

    if (_engine->Settings->DisableLighting) {
        return;
    }

    if (item->GetLightSource()) {
        UpdateLightSource(item->GetId(), item->GetHex(), item->GetLightColor(), item->GetLightDistance(), static_cast<LightFlag>(item->GetLightFlags()), item->GetLightIntensity(), item->GetSpriteOffsetPtr());
    }
    else {
        FinishLightSource(item->GetId());
    }
}

void MapView::UpdateHexLightSources(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    if (_engine->Settings->DisableLighting) {
        return;
    }

    const auto& field = _hexField->GetCellForReading(hex);

    for (auto&& ls_pair : copy(field.LightSources)) {
        ApplyLightFan(ls_pair.first);
    }
}

void MapView::UpdateLightSource(ident_t id, mpos hex, ucolor color, int32_t distance, LightFlag flags, int32_t intensity, nptr<const ipos32> offset)
{
    FO_STACK_TRACE_ENTRY();

    const auto apply_updated_light_source = [this](ptr<LightSource> ls) {
        ls->TargetIntensity = std::clamp(std::abs(ls->Intensity), 0, LIGHT_INTENSITY_MAX);

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
    };

    const auto it = _lightSources.find(id);

    if (it == _lightSources.end()) {
        ptr<LightSource> ls = _lightSources.emplace(id, SafeAlloc::MakeUnique<LightSource>(id, hex, color, distance, flags, intensity, offset)).first->second;

        apply_updated_light_source(ls);
    }
    else {
        auto ls = it->second.as_ptr();

        // Ignore redundant updates
        if (!ls->Finishing && ls->Hex == hex && ls->Color == color && ls->Distance == distance && ls->Flags == flags && ls->Intensity == intensity && ls->Offset == offset) {
            return;
        }

        CleanLightFan(ls);

        ls->Finishing = false;
        ls->Hex = hex;
        ls->Color = color;
        ls->Distance = distance;
        ls->Flags = flags;
        ls->Intensity = intensity;
        ls->Offset = offset;

        apply_updated_light_source(ls);
    }
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

        if (ls->Offset) {
            // Already-built light primitives hold this raw offset pointer (PointOffset), which points into
            // the entity view that is being destroyed right now; null them in place so even the current
            // frame cannot dereference the freed storage. A rebuild is not needed: the offset participates
            // in primitives only through PointOffset.
            for (auto& points : _lightPoints) {
                for (auto& point : points) {
                    if (point.PointOffset == ls->Offset) {
                        point.PointOffset = nullptr;
                    }
                }
            }

            ls->Offset = nullptr;
        }
    }
}

void MapView::ApplyLightFan(ptr<LightSource> ls)
{
    FO_STACK_TRACE_ENTRY();

    if (ls->Applied) {
        CleanLightFan(ls);
    }

    FO_VERIFY_AND_THROW(!ls->Applied, "Light source is already applied to the map view");

    ls->Applied = true;
    ls->NeedReapply = false;

    const auto center_hex = ls->Hex;
    const auto distance = std::max(1, ls->Distance);
    const auto prev_fan_hexes = std::move(ls->FanHexes);

    ls->MarkedHexes.clear();
    ls->MarkedHexes.reserve(GeometryHelper::HexesInRadius(distance));
    ls->FanHexes.clear();
    ls->FanHexes.reserve(numeric_cast<size_t>(distance) * GameSettings::MAP_DIR_COUNT);

    if (IsEnumSet(ls->Flags, LightFlag::Global)) {
        _globalLights++;
    }

    if (IsEnumSet(ls->Flags, LightFlag::Global)) {
        ls->Capacity = GetGlobalDayLightCapacity();
    }
    else if (ls->Intensity >= 0) {
        ls->Capacity = GetMapDayLightCapacity();
    }
    else {
        ls->Capacity = LIGHT_CAPACITY_MAX;
    }

    if (IsEnumSet(ls->Flags, LightFlag::Inverse)) {
        ls->Capacity = LIGHT_CAPACITY_MAX - ls->Capacity;
    }

    const int32_t raw_intensity = std::clamp(ls->CurIntensity, 0, LIGHT_INTENSITY_MAX) * LIGHT_RAW_INTENSITY_MAX / LIGHT_INTENSITY_MAX;
    const int32_t clamped_capacity = std::clamp(ls->Capacity, 0, LIGHT_CAPACITY_MAX);
    const int64_t scaled_center_alpha = numeric_cast<int64_t>(raw_intensity) * LIGHT_COLOR_CHANNEL_MAX * clamped_capacity;
    const uint8_t center_alpha = numeric_cast<uint8_t>(scaled_center_alpha / LIGHT_RAW_INTENSITY_MAX / LIGHT_CAPACITY_MAX);

    ls->CenterColor = ucolor {ls->Color, center_alpha};

    MarkLight(ls, center_hex, raw_intensity);

    ipos32 raw_traced_hex = {center_hex.x, center_hex.y};
    bool seek_start = true;
    optional<mpos> last_traced_hex;

    // One spoke per hex direction. MAP_DIR_COUNT is 6 (hexagonal) or 8 (square);
    // LightFlag::StopDir0..StopDir7 carry the per-spoke blocked bits inside
    // ls->Flags — (StopDir0 << i) gates spoke i.
    for (int32_t i = 0, ii = GameSettings::MAP_DIR_COUNT; i < ii; i++) {
        const mdir dir = hdir((i + 2) % GameSettings::MAP_DIR_COUNT);

        for (int32_t j = 0; j < distance; j++) {
            if (seek_start) {
                for (int32_t l = 0; l < distance; l++) {
#if FO_GEOMETRY == 1
                    GeometryHelper::MoveHexByDirUnsafe(raw_traced_hex, hdir::NorthEast);
#elif FO_GEOMETRY == 2
                    GeometryHelper::MoveHexByDirUnsafe(raw_traced_hex, hdir::North);
#endif
                }

                seek_start = false;
                j = -1;
            }
            else {
                GeometryHelper::MoveHexByDirUnsafe(raw_traced_hex, dir);
            }

            auto traced_hex = _mapSize.clamp_pos(raw_traced_hex);

            const bool dir_disabled = IsEnumSet(ls->Flags, static_cast<LightFlag>(static_cast<uint16_t>(LightFlag::StopDir0) << i));
            if (dir_disabled) {
                traced_hex = center_hex;
            }
            else {
                TraceLightLine(ls, center_hex, traced_hex, distance, raw_intensity);
            }

            if (!last_traced_hex.has_value() || traced_hex != last_traced_hex.value()) {
                uint8_t traced_alpha;
                bool use_offsets = false;

                if (ipos32 {traced_hex.x, traced_hex.y} != raw_traced_hex) {
                    traced_alpha = numeric_cast<uint8_t>(lerp(numeric_cast<int32_t>(center_alpha), 0, numeric_cast<float32_t>(GeometryHelper::GetDistance(center_hex, traced_hex)) / numeric_cast<float32_t>(distance)));

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

void MapView::CleanLightFan(ptr<LightSource> ls)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ls->Applied, "Light source is not applied to the map view");

    ls->Applied = false;

    if (IsEnumSet(ls->Flags, LightFlag::Global)) {
        FO_VERIFY_AND_THROW(_globalLights > 0, "Global light counter underflowed while cleaning a light fan");
        _globalLights--;
    }

    for (const auto& hex : ls->MarkedHexes) {
        auto field = _hexField->GetCellForWriting(hex);

        field->LightSources.erase(ls);
        CalculateHexLight(hex, field);

        if constexpr (FO_DEBUG) {
            if (field->IsView) {
                FO_VERIFY_AND_THROW(_visibleLightSources[ls] > 0, "Visible light source reference count underflowed while cleaning a light fan");
                _visibleLightSources[ls]--;
            }
        }
    }

    if (!ls->FanHexes.empty()) {
        _needRebuildLightPrimitives = true;
    }

    if constexpr (FO_DEBUG) {
        FO_VERIFY_AND_THROW(_visibleLightSources.count(ls) == 0 || _visibleLightSources[ls] == 0, "Visible light source counter is already active before add");
    }

    _visibleLightSources.erase(ls);
}

void MapView::TraceLightLine(ptr<LightSource> ls, mpos from_hex, mpos& to_hex, int32_t distance, int32_t raw_intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto [base_sx, base_sy] = GeometryHelper::GetStepsCoords({from_hex.x, from_hex.y}, {to_hex.x, to_hex.y});
    const auto sx1_f = base_sx;
    const auto sy1_f = base_sy;

    auto curx1_f = numeric_cast<float32_t>(from_hex.x);
    auto cury1_f = numeric_cast<float32_t>(from_hex.y);
    auto curx1_i = numeric_cast<int32_t>(from_hex.x);
    auto cury1_i = numeric_cast<int32_t>(from_hex.y);

    int32_t cur_raw_intensity = raw_intensity;
    const int32_t raw_intensity_sub = raw_intensity / distance;

    const auto resolve_hex = [this](int32_t hx, int32_t hy) -> mpos { return _mapSize.from_raw_pos(hx, hy); };

    while (true) {
        cur_raw_intensity -= raw_intensity_sub;
        curx1_f += sx1_f;
        cury1_f += sy1_f;

        const int32_t old_curx1_i = curx1_i;
        const int32_t old_cury1_i = cury1_i;

        // Casts
        curx1_i = iround<int32_t>(curx1_f);

        if (curx1_f - numeric_cast<float32_t>(curx1_i) >= 0.5f) {
            curx1_i++;
        }

        cury1_i = iround<int32_t>(cury1_f);

        if (cury1_f - numeric_cast<float32_t>(cury1_i) >= 0.5f) {
            cury1_i++;
        }

        // Left&Right trace
        int32_t ox = 0;
        int32_t oy = 0;

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

        const auto map_width = numeric_cast<int32_t>(_mapSize.width);
        const auto map_height = numeric_cast<int32_t>(_mapSize.height);

        if (ox != 0) {
            // Left side
            ox = old_curx1_i + ox;

            if (ox < 0 || ox >= map_width || _hexField->GetCellForReading(resolve_hex(ox, old_cury1_i)).LightBlocked) {
                to_hex = resolve_hex(ox < 0 || ox >= map_width ? old_curx1_i : ox, old_cury1_i);
                MarkLightEnd(ls, resolve_hex(old_curx1_i, old_cury1_i), to_hex, cur_raw_intensity);
                break;
            }

            MarkLightStep(ls, resolve_hex(old_curx1_i, old_cury1_i), resolve_hex(ox, old_cury1_i), cur_raw_intensity);

            // Right side
            oy = old_cury1_i + oy;

            if (oy < 0 || oy >= map_height || _hexField->GetCellForReading(resolve_hex(old_curx1_i, oy)).LightBlocked) {
                to_hex = resolve_hex(old_curx1_i, oy < 0 || oy >= map_height ? old_cury1_i : oy);
                MarkLightEnd(ls, resolve_hex(old_curx1_i, old_cury1_i), to_hex, cur_raw_intensity);
                break;
            }

            MarkLightStep(ls, resolve_hex(old_curx1_i, old_cury1_i), resolve_hex(old_curx1_i, oy), cur_raw_intensity);
        }

        // Main trace
        if (curx1_i < 0 || curx1_i >= map_width || cury1_i < 0 || cury1_i >= map_height || _hexField->GetCellForReading(resolve_hex(curx1_i, cury1_i)).LightBlocked) {
            to_hex = resolve_hex(curx1_i < 0 || curx1_i >= map_width ? old_curx1_i : curx1_i, cury1_i < 0 || cury1_i >= map_height ? old_cury1_i : cury1_i);
            MarkLightEnd(ls, resolve_hex(old_curx1_i, old_cury1_i), to_hex, cur_raw_intensity);
            break;
        }

        MarkLightEnd(ls, resolve_hex(old_curx1_i, old_cury1_i), resolve_hex(curx1_i, cury1_i), cur_raw_intensity);

        if (curx1_i == numeric_cast<int32_t>(to_hex.x) && cury1_i == numeric_cast<int32_t>(to_hex.y)) {
            break;
        }
    }
}

void MapView::MarkLightStep(ptr<LightSource> ls, mpos from_hex, mpos to_hex, int32_t raw_intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(to_hex);

    if (field.HasTransparentWall) {
        const bool north_south = field.Corner == CornerType::NorthSouth || field.Corner == CornerType::North || field.Corner == CornerType::West;
        const auto dir = GeometryHelper::GetHexDir(from_hex, to_hex);

        if (dir == hdir::NorthEast || (north_south && dir == hdir::East) || (!north_south && (dir == hdir::West || dir == hdir::NorthWest))) {
            MarkLight(ls, to_hex, raw_intensity);
        }
    }
    else {
        MarkLight(ls, to_hex, raw_intensity);
    }
}

void MapView::MarkLightEnd(ptr<LightSource> ls, mpos from_hex, mpos to_hex, int32_t raw_intensity)
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

    const auto dir = GeometryHelper::GetHexDir(from_hex, to_hex);

    if (dir == hdir::NorthEast || (north_south && dir == hdir::East) || (!north_south && (dir == hdir::West || dir == hdir::NorthWest))) {
        MarkLight(ls, to_hex, raw_intensity);

        if (is_wall) {
            if (north_south) {
                if (to_hex.y > 0) {
                    MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x, to_hex.y - 1), true, raw_intensity);
                }
                if (to_hex.y < _mapSize.height - 1) {
                    MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x, to_hex.y + 1), true, raw_intensity);
                }
            }
            else {
                if (to_hex.x > 0) {
                    MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x - 1, to_hex.y), false, raw_intensity);

                    if (to_hex.y > 0) {
                        MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x - 1, to_hex.y - 1), false, raw_intensity);
                    }
                    if (to_hex.y < _mapSize.height - 1) {
                        MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x - 1, to_hex.y + 1), false, raw_intensity);
                    }
                }
                if (to_hex.x < _mapSize.width - 1) {
                    MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x + 1, to_hex.y), false, raw_intensity);

                    if (to_hex.y > 0) {
                        MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x + 1, to_hex.y - 1), false, raw_intensity);
                    }
                    if (to_hex.y < _mapSize.height - 1) {
                        MarkLightEndNeighbor(ls, _mapSize.from_raw_pos(to_hex.x + 1, to_hex.y + 1), false, raw_intensity);
                    }
                }
            }
        }
    }
}

void MapView::MarkLightEndNeighbor(ptr<LightSource> ls, mpos hex, bool north_south, int32_t raw_intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasWall) {
        const auto corner = field.Corner;

        if ((north_south && (corner == CornerType::NorthSouth || corner == CornerType::North || corner == CornerType::West)) || (!north_south && (corner == CornerType::EastWest || corner == CornerType::East)) || corner == CornerType::South) {
            MarkLight(ls, hex, raw_intensity / 2);
        }
    }
}

void MapView::MarkLight(ptr<LightSource> ls, mpos hex, int32_t raw_intensity)
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32_t clamped_raw_intensity = std::clamp(raw_intensity, 0, LIGHT_RAW_INTENSITY_MAX);
    const int32_t clamped_capacity = std::clamp(ls->Capacity, 0, LIGHT_CAPACITY_MAX);
    const int64_t scaled_light_value = numeric_cast<int64_t>(clamped_raw_intensity) * LIGHT_HEX_COLOR_MAX * clamped_capacity;
    const int32_t light_value = numeric_cast<int32_t>(scaled_light_value / LIGHT_RAW_INTENSITY_MAX / LIGHT_CAPACITY_MAX);
    const int32_t scaled_light_value_r = light_value * numeric_cast<int32_t>(ls->CenterColor.comp.r) / LIGHT_COLOR_CHANNEL_MAX;
    const int32_t scaled_light_value_g = light_value * numeric_cast<int32_t>(ls->CenterColor.comp.g) / LIGHT_COLOR_CHANNEL_MAX;
    const int32_t scaled_light_value_b = light_value * numeric_cast<int32_t>(ls->CenterColor.comp.b) / LIGHT_COLOR_CHANNEL_MAX;
    const uint8_t light_value_r = numeric_cast<uint8_t>(std::clamp(scaled_light_value_r, 0, LIGHT_COLOR_CHANNEL_MAX));
    const uint8_t light_value_g = numeric_cast<uint8_t>(std::clamp(scaled_light_value_g, 0, LIGHT_COLOR_CHANNEL_MAX));
    const uint8_t light_value_b = numeric_cast<uint8_t>(std::clamp(scaled_light_value_b, 0, LIGHT_COLOR_CHANNEL_MAX));
    const ucolor light_color = ucolor {light_value_r, light_value_g, light_value_b, 0};

    auto field = _hexField->GetCellForWriting(hex);
    const auto it = field->LightSources.find(ls);

    if (it == field->LightSources.end()) {
        field->LightSources.emplace(ls, light_color);
        ls->MarkedHexes.emplace_back(hex);
        CalculateHexLight(hex, field);

        if (field->IsView) {
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

void MapView::CalculateHexLight(mpos hex, ptr<const Field> field)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto& hex_light = _hexLight[hex.y * _mapSize.width + hex.x];

    hex_light = {};

    for (const auto& value : field->LightSources | std::views::values) {
        hex_light.comp.r = std::max(hex_light.comp.r, value.comp.r);
        hex_light.comp.g = std::max(hex_light.comp.g, value.comp.g);
        hex_light.comp.b = std::max(hex_light.comp.b, value.comp.b);
    }
}

void MapView::LightFanToPrimitves(ptr<const LightSource> ls, vector<PrimitivePoint>& points) const
{
    FO_STACK_TRACE_ENTRY();

    if (ls->FanHexes.size() <= 1) {
        return;
    }

    ipos32 center_pos = GetHexMapPos(ls->Hex);
    center_pos.x += GameSettings::MAP_HEX_WIDTH / 2;
    center_pos.y += GameSettings::MAP_HEX_HEIGHT / 2;

    // Per-light ovalization metadata.
    const auto [screen_anchor_ox, screen_anchor_oy] = GeometryHelper::GetHexOffset(ipos32 {0, 0}, _screenRawHex);
    const fpos32 screen_anchor_tex = {numeric_cast<float32_t>(screen_anchor_ox), numeric_cast<float32_t>(screen_anchor_oy)};
    const auto natural_radius = std::max(1, ls->Distance);
    const auto fan_radius = numeric_cast<float32_t>(natural_radius);
    const auto center_alpha_norm = numeric_cast<float32_t>(ls->CenterColor.comp.a) / 255.0f;
    const fpos32 light_egg_data = {fan_radius, center_alpha_norm};
    const auto semi_x_px = fan_radius * numeric_cast<float32_t>(GameSettings::MAP_HEX_WIDTH);
    const auto semi_y_px = fan_radius * numeric_cast<float32_t>(GameSettings::MAP_HEX_LINE_HEIGHT);

    const auto rim_dell = [semi_x_px, semi_y_px](float32_t ox, float32_t oy) noexcept -> float32_t {
        const float32_t nx = ox / semi_x_px;
        const float32_t ny = oy / semi_y_px;
        return std::sqrt(nx * nx + ny * ny);
    };

    const auto center_point = PrimitivePoint {.PointPos = center_pos, .PointColor = ls->CenterColor, .PointOffset = ls->Offset, .TexUV = screen_anchor_tex, .EggData = light_egg_data, .PointPosZ = 0.0f};

    const auto points_start_size = points.size();
    points.reserve(points.size() + ls->FanHexes.size() * 3);

    for (size_t i = 0; i < ls->FanHexes.size(); i++) {
        const auto& fan_hex = ls->FanHexes[i];
        const mpos hex = std::get<0>(fan_hex);
        const uint8_t alpha = std::get<1>(fan_hex);
        const bool use_offsets = std::get<2>(fan_hex);

        const auto [ox_int, oy_int] = GeometryHelper::GetHexOffset(ls->Hex, hex);
        const float32_t ox = numeric_cast<float32_t>(ox_int);
        const float32_t oy = numeric_cast<float32_t>(oy_int);
        const ipos32 pos = {center_pos.x + iround<int32_t>(ox), center_pos.y + iround<int32_t>(oy)};
        const ucolor color = ucolor(ls->CenterColor, alpha);
        const nptr<const ipos32> offset = use_offsets ? ls->Offset : nullptr;
        const auto edge_point = PrimitivePoint {.PointPos = pos, .PointColor = color, .PointOffset = offset, .TexUV = screen_anchor_tex, .EggData = light_egg_data, .PointPosZ = rim_dell(ox, oy)};

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

    FO_VERIFY_AND_THROW(points.size() % 3 == 0, "Client map border polygon points must be grouped into render triangles", GetId(), points.size(), 3);
}

void MapView::SetHiddenRoof(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    const auto corrected_hx = hex.x - hex.x % _engine->Settings->MapTileStep;
    const auto corrected_hy = hex.y - hex.y % _engine->Settings->MapTileStep;
    const auto corrected_hex = _mapSize.from_raw_pos(corrected_hx, corrected_hy);
    const auto roof_num = _hexField->GetCellForReading(corrected_hex).RoofNum;

    if (_hiddenRoofNum != roof_num) {
        _hiddenRoofNum = roof_num;
        RebuildMap();
    }
}

auto MapView::MeasureMapBorders(ptr<const Sprite> spr, ipos32 offset) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto left = std::max(spr->GetSize().width / 2 + spr->GetOffset().x + offset.x + _maxScroll.width - _wLeft * GameSettings::MAP_HEX_WIDTH, 0);
    const auto right = std::max(spr->GetSize().width / 2 - spr->GetOffset().x - offset.x + _maxScroll.width - _wRight * GameSettings::MAP_HEX_WIDTH, 0);
    const auto top = std::max(0 + spr->GetOffset().y + offset.y + _maxScroll.height - _hTop * GameSettings::MAP_HEX_LINE_HEIGHT, 0);
    const auto bottom = std::max(spr->GetSize().height - spr->GetOffset().y - offset.y + _maxScroll.height - _hBottom * GameSettings::MAP_HEX_LINE_HEIGHT, 0);

    if (left != 0 || right != 0 || top != 0 || bottom != 0) {
        _wLeft += left / GameSettings::MAP_HEX_WIDTH + (left % GameSettings::MAP_HEX_WIDTH != 0 ? 1 : 0);
        _wRight += right / GameSettings::MAP_HEX_WIDTH + (right % GameSettings::MAP_HEX_WIDTH != 0 ? 1 : 0);
        _hTop += top / GameSettings::MAP_HEX_LINE_HEIGHT + (top % GameSettings::MAP_HEX_LINE_HEIGHT != 0 ? 1 : 0);
        _hTop += _hTop % 2;
        _hBottom += bottom / GameSettings::MAP_HEX_LINE_HEIGHT + (bottom % GameSettings::MAP_HEX_LINE_HEIGHT != 0 ? 1 : 0);

        if (!_mapLoading) {
            RebuildMap();
        }

        return true;
    }

    return false;
}

auto MapView::MeasureMapBorders(ptr<const ItemHexView> item) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(item->GetMap() == this, "Map border measurement requested for an item attached to a different client map", item->GetId(), item->GetMap()->GetId(), GetId());

    auto item_spr = item->GetSprite();
    FO_VERIFY_AND_THROW(item_spr, "Item is missing its sprite");
    return MeasureMapBorders(item_spr.as_ptr(), item->GetSpriteOffset());
}

void MapView::RecacheHexFlags(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Client map cannot recache flags for a hex outside map bounds", GetId(), hex, _mapSize);

    auto field = _hexField->GetCellForWriting(hex);

    RecacheHexFlags(field);
}

void MapView::RecacheHexFlags(ptr<Field> field)
{
    FO_STACK_TRACE_ENTRY();

    field->HasWall = false;
    field->HasTransparentWall = false;
    field->HasScenery = false;
    field->MoveBlocked = field->ScrollBlock;
    field->ShootBlocked = false;
    field->LightBlocked = false;
    field->Corner = CornerType::NorthSouth;
    field->GroundTile.reset();
    field->HasRoof = false;

    if (!field->Items.empty()) {
        for (ptr<ItemHexView> item : field->Items) {
            if (!field->HasWall && item->GetIsWall()) {
                field->HasWall = true;
                field->HasTransparentWall = item->GetLightThru();

                field->Corner = item->GetCorner();
            }
            else if (!field->HasScenery && item->GetIsScenery()) {
                field->HasScenery = true;

                if (!field->HasWall) {
                    field->Corner = item->GetCorner();
                }
            }

            if (!field->MoveBlocked && !item->GetNoBlock()) {
                field->MoveBlocked = true;
            }
            if (!field->ShootBlocked && !item->GetShootThru()) {
                field->ShootBlocked = true;
            }
            if (!field->LightBlocked && !item->GetLightThru()) {
                field->LightBlocked = true;
            }

            if (item->GetIsTile()) {
                if (item->GetIsRoofTile()) {
                    field->HasRoof = true;
                }
                else if (!field->GroundTile) {
                    field->GroundTile = item;
                }
            }
        }
    }

    if (field->ShootBlocked) {
        field->MoveBlocked = true;
    }
}

void MapView::RecacheScrollBlocks()
{
    FO_STACK_TRACE_ENTRY();

    const irect32 scroll_area = GetScrollAxialArea();
    const int32_t scroll_block_size = _engine->Settings->ScrollBlockSize;

    for (const auto hx : iterate_range(_mapSize.width)) {
        for (const auto hy : iterate_range(_mapSize.height)) {
            const mpos hex = {hx, hy};
            const auto& field = _hexField->GetCellForReading(hex);
            const ipos32 axial_hex = GeometryHelper::GetHexAxialCoord(hex);

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
                _hexField->GetCellForWriting(hex)->ScrollBlock = is_on_scroll_block;
                RecacheHexFlags(hex);
            }
        }
    }
}

void MapView::Resize(msize size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper mode is not selected");
    FO_VERIFY_AND_THROW(size.width >= GameSettings::MIN_MAP_SIZE, "Map resize width is smaller than the engine minimum", size.width, GameSettings::MIN_MAP_SIZE);
    FO_VERIFY_AND_THROW(size.width <= GameSettings::MAX_MAP_SIZE, "Map resize width is larger than the engine maximum", size.width, GameSettings::MAX_MAP_SIZE);
    FO_VERIFY_AND_THROW(size.height >= GameSettings::MIN_MAP_SIZE, "Map resize height is smaller than the engine minimum", size.height, GameSettings::MIN_MAP_SIZE);
    FO_VERIFY_AND_THROW(size.height <= GameSettings::MAX_MAP_SIZE, "Map resize height is larger than the engine maximum", size.height, GameSettings::MAX_MAP_SIZE);

    for (const auto& vf : _viewField) {
        if (_mapSize.is_valid_pos(vf.RawHex)) {
            auto field = _hexField->GetCellForWriting(_mapSize.from_raw_pos(vf.RawHex));
            field->IsView = false;
            InvalidateSpriteChain(field);
        }
    }

    // Remove objects on shrink
    for (const auto hy : iterate_range(std::max(size.height, _mapSize.height))) {
        for (const auto hx : iterate_range(std::max(size.width, _mapSize.width))) {
            if (hx >= size.width || hy >= size.height) {
                const auto& field = _hexField->GetCellForReading({hx, hy});

                if (!field.OriginCritters.empty()) {
                    for (ptr<CritterHexView> cr : copy(field.OriginCritters)) {
                        DestroyCritter(cr);
                    }
                }
                if (!field.OriginItems.empty()) {
                    for (ptr<ItemHexView> item : copy(field.OriginItems)) {
                        DestroyItem(item);
                    }
                }

                FO_VERIFY_AND_THROW(field.OriginCritters.empty(), "Field origin critters must be empty before this operation");
                FO_VERIFY_AND_THROW(field.OriginItems.empty(), "Field origin items must be empty before this operation");
                FO_VERIFY_AND_THROW(!field.SpriteChain, "Field sprite chain is already set");
            }
        }
    }

    SetSize(size);
    _mapSize = size;

    SetWorkHex(_mapSize.clamp_pos(GetWorkHex()));

    _hexLight.resize(_mapSize.square() * 3);
    _hexField->Resize(_mapSize);

    for (const auto hy : iterate_range(_mapSize.height)) {
        for (const auto hx : iterate_range(_mapSize.width)) {
            CalculateHexLight(mpos(hx, hy), &_hexField->GetCellForReading(mpos(hx, hy)));
        }
    }

    RecacheScrollBlocks();
    RebuildMapNow();
}

void MapView::SetShowMapperOverlay(bool show)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper mode is not selected");

    if (_isShowMapperOverlay == show) {
        return;
    }

    _isShowMapperOverlay = show;
    RebuildMap();
}

void MapView::SetShowMapperHiddenSprites(bool show)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper mode is not selected");

    if (_isShowMapperHiddenSprites == show) {
        return;
    }

    _isShowMapperHiddenSprites = show;
    RebuildMap();
}

auto MapView::ScreenToMapPos(ipos32 screen_pos) const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    return (fpos32(screen_pos) / GetSpritesZoom() + _scrollOffset).round<int32_t>();
}

auto MapView::MapToScreenPos(ipos32 map_pos) const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    return ((fpos32(map_pos) - _scrollOffset) * GetSpritesZoom()).round<int32_t>();
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

    for (int32_t i = 0; i < _wLeft; i++) {
        const auto dir = hdir::West;
        GeometryHelper::MoveHexByDirUnsafe(row_hex, dir);
    }
    for (int32_t i = 0; i < _hTop / 2; i++) {
        const auto dir1 = hdir::NorthWest;
        const auto dir2 = hdir::NorthEast;
        GeometryHelper::MoveHexByDirUnsafe(row_hex, dir1);
        GeometryHelper::MoveHexByDirUnsafe(row_hex, dir2);
    }

    // Assign view coords
    size_t vpos = 0;
    auto oy = -GameSettings::MAP_HEX_LINE_HEIGHT * _hTop;

    for (auto yv = 0; yv < _hVisible; yv++) {
        auto cur_hex = row_hex;
        auto ox = -(_wLeft * GameSettings::MAP_HEX_WIDTH) + (yv % 2 != 0 ? GameSettings::MAP_HEX_WIDTH / 2 : 0);

        for (auto xv = 0; xv < _wVisible; xv++) {
            auto& vf = _viewField[vpos];
            ++vpos;

            vf.Offset = {ox, oy};
            vf.RawHex = cur_hex;

            ox += GameSettings::MAP_HEX_WIDTH;
            GeometryHelper::MoveHexByDirUnsafe(cur_hex, hdir::East);
        }

        oy += GameSettings::MAP_HEX_LINE_HEIGHT;
        GeometryHelper::MoveHexByDirUnsafe(row_hex, yv % 2 == 0 ? hdir::SouthEast : hdir::SouthWest);
    }
}

void MapView::AddSpriteToChain(ptr<Field> field, ptr<MapSprite> mspr)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!field->SpriteChain) {
        field->SpriteChain = mspr;
        mspr->CreateExtraChain(field->SpriteChain.get_pp());
    }
    else {
        field->SpriteChain->AddToExtraChain(mspr);
    }
}

void MapView::InvalidateSpriteChain(ptr<Field> field)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (field->SpriteChain) {
        FO_VERIFY_AND_THROW(field->SpriteChain->IsValid(), "Map field sprite chain is invalid");
        while (field->SpriteChain) {
            FO_VERIFY_AND_THROW(field->SpriteChain->IsValid(), "Map field sprite chain is invalid");
            field->SpriteChain->Invalidate();
        }
    }
}

auto MapView::GetScreenRawHex() const -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return _screenRawHex;
}

auto MapView::GetCenterRawHex() const -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    const ipos32 lt_pos = GeometryHelper::GetHexPos(_screenRawHex);
    const ipos32 center_offset = ipos32(iround<int32_t>(_viewSize.width) / 2, iround<int32_t>(_viewSize.height) / 2);
    return GeometryHelper::GetHexPosCoord(lt_pos + center_offset);
}

auto MapView::ConvertToScreenRawHex(ipos32 center_raw_hex) const -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    const ipos32 center_pos = GeometryHelper::GetHexPos(center_raw_hex);
    const ipos32 center_offset = ipos32(iround<int32_t>(_viewSize.width) / 2, iround<int32_t>(_viewSize.height) / 2);
    return GeometryHelper::GetHexPosCoord(center_pos - center_offset);
}

auto MapView::GetHexMapPos(mpos hex) const -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto hex_offset = GeometryHelper::GetHexOffset(_screenRawHex, ipos32(hex));
    return {hex_offset.x, hex_offset.y};
}

void MapView::SetTransparentEgg(TransparentEggSlot slot, mpos hex, ipos32 hex_offset, isize32 egg_size, bool apply_size_ext)
{
    FO_STACK_TRACE_ENTRY();

    if (!_mapSize.is_valid_pos(hex)) {
        ClearTransparentEgg(slot);
        return;
    }

    auto& egg = _transparentEggs[static_cast<size_t>(slot)];
    egg.Hex = hex;
    egg.HexOffset = hex_offset;
    egg.Size = egg_size;
    egg.ApplySizeExt = apply_size_ext;
    egg.Valid = true;

    UpdateTransparentEgg(slot);
}

void MapView::ClearTransparentEgg(TransparentEggSlot slot)
{
    FO_STACK_TRACE_ENTRY();

    _transparentEggs[static_cast<size_t>(slot)] = {};
    _engine->SprMngr.InvalidateEgg(slot);
}

void MapView::UpdateTransparentEgg(TransparentEggSlot slot)
{
    FO_STACK_TRACE_ENTRY();

    const auto& egg = _transparentEggs[static_cast<size_t>(slot)];

    if (!egg.Valid) {
        _engine->SprMngr.InvalidateEgg(slot);
        return;
    }
    if (!_mapSize.is_valid_pos(egg.Hex)) {
        _engine->SprMngr.InvalidateEgg(slot);
        return;
    }

    // egg.HexOffset is hex-center-relative; GetHexMapPos is the cell top-left, so add half a hex.
    const auto hex_pos = GetHexMapPos(egg.Hex);
    const auto center_x = hex_pos.x + GameSettings::MAP_HEX_WIDTH / 2 + egg.HexOffset.x;
    const auto center_y = hex_pos.y + GameSettings::MAP_HEX_HEIGHT / 2 + egg.HexOffset.y;
    const auto egg_width_ext = egg.ApplySizeExt ? numeric_cast<float32_t>(_engine->Settings->EggEllipseWidthExt) : 0.0f;
    const auto egg_height_ext = egg.ApplySizeExt ? numeric_cast<float32_t>(_engine->Settings->EggEllipseHeightExt) : 0.0f;
    float32_t radius_w = std::max((numeric_cast<float32_t>(egg.Size.width) + egg_width_ext) * 0.5f, 1.0f);
    float32_t radius_h = std::max((numeric_cast<float32_t>(egg.Size.height) + egg_height_ext) * 0.5f, 1.0f);
    _engine->SprMngr.SetEgg(slot, egg.Hex, {numeric_cast<float32_t>(center_x), numeric_cast<float32_t>(center_y)}, {radius_w, radius_h});
}

void MapView::UpdateTransparentEggs()
{
    FO_STACK_TRACE_ENTRY();

    UpdateTransparentEgg(TransparentEggSlot::Primary);
    UpdateTransparentEgg(TransparentEggSlot::Secondary);
}

void MapView::DrawMap()
{
    FO_STACK_TRACE_ENTRY();

    if (_rebuildMap) {
        RebuildMapNow();
    }

    UpdateTransparentEggs();

    _engine->SprMngr.GetRender().SetOrthoDepthRange(-_mapDepthHalf, _mapDepthHalf);
    const auto restore_depth_range = scope_exit([this]() noexcept { _engine->SprMngr.GetRender().SetOrthoDepthRange(ORTHO_DEPTH_DEFAULT_NEAR, ORTHO_DEPTH_DEFAULT_FAR); });

    ProcessLighting();
    PrepareFogToDraw();
    _mapSprites.SortIfNeeded();
    _indoorMaskSprites.SortIfNeeded();

    // Draw by parts if view size too big
    const fsize32 screen_size = fsize32(_screenSize);
    const fpos32 draw_scale = {screen_size.width / _viewSize.width, screen_size.height / _viewSize.height};
    const int32_t steps_width = iround<int32_t>(std::ceil(1.0f / draw_scale.x));
    const int32_t steps_height = iround<int32_t>(std::ceil(1.0f / draw_scale.y));
    const bool direct_draw = _engine->Settings->MapDirectDraw;

    for (int32_t step_x = 0; step_x < steps_width; step_x++) {
        for (int32_t step_y = 0; step_y < steps_height; step_y++) {
            irect32 draw_area;

            if (direct_draw) {
                const int32_t draw_x = iround<int32_t>(_scrollOffset.x);
                const int32_t draw_y = iround<int32_t>(_scrollOffset.y);
                draw_area = {draw_x, draw_y, _screenSize.width, _screenSize.height};
            }
            else {
                FO_VERIFY_AND_THROW(_rtMap, "Map render target is null");
                _engine->SprMngr.GetRtMngr().PushRenderTarget(_rtMap);
                _engine->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear, true);

                const int32_t draw_x = iround<int32_t>(std::floor(_scrollOffset.x)) + step_x * _screenSize.width;
                const int32_t draw_y = iround<int32_t>(std::floor(_scrollOffset.y)) + step_y * _screenSize.height;
                const int32_t draw_width = std::min(iround<int32_t>(std::ceil(_viewSize.width)) - step_x * _screenSize.width, _screenSize.width);
                const int32_t draw_height = std::min(iround<int32_t>(std::ceil(_viewSize.height)) - step_y * _screenSize.height, _screenSize.height);
                draw_area = {draw_x, draw_y, draw_width, draw_height};
            }

            const float32_t step_xf = numeric_cast<float32_t>(step_x);
            const float32_t step_yf = numeric_cast<float32_t>(step_y);
            const float32_t source_x = std::fmod(_scrollOffset.x, 1.0f);
            const float32_t source_y = std::fmod(_scrollOffset.y, 1.0f);
            const float32_t source_width = std::min(_viewSize.width - step_xf * screen_size.width, screen_size.width);
            const float32_t source_height = std::min(_viewSize.height - step_yf * screen_size.height, screen_size.height);
            const frect32 source_rect = {source_x, source_y, source_width, source_height};
            const int32_t target_x = iround<int32_t>(std::floor(step_xf * screen_size.width * draw_scale.x));
            const int32_t target_y = iround<int32_t>(std::floor(step_yf * screen_size.height * draw_scale.y));
            const int32_t target_width = iround<int32_t>(std::ceil(source_width * draw_scale.x));
            const int32_t target_height = iround<int32_t>(std::ceil(source_height * draw_scale.y));
            const irect32 target_rect = {target_x, target_y, target_width, target_height};

            _currentRenderDrawArea = draw_area;
            const auto reset_render_draw_area = scope_exit([this]() noexcept { _currentRenderDrawArea.reset(); });

            // Tiles
            _engine->OnRenderMap_BeforeTiles.Fire(this, draw_area);
            FO_VERIFY_AND_THROW(_engine->EffectMngr.Effects.Tile, "Tile effect is null");
            _engine->SprMngr.DrawSprites(_mapSprites, draw_area, false, DrawOrderType::Tile, DrawOrderType::PreLight, GetMapDayColor(), _engine->EffectMngr.Effects.Tile);
            _engine->OnRenderMap_AfterTiles.Fire(this, draw_area);

            // Lighting
            if (!_engine->Settings->DisableLighting) {
                _engine->OnRenderMap_BeforeLighting.Fire(this, draw_area);
                FO_VERIFY_AND_THROW(_rtLight, "Lighting render target is not allocated");
                _engine->SprMngr.GetRtMngr().PushRenderTarget(_rtLight);
                _engine->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);

                for (auto& points : _lightPoints) {
                    _engine->SprMngr.DrawPoints(points, RenderPrimitiveType::TriangleList, &draw_area, _engine->EffectMngr.Effects.Light);
                }

                _engine->SprMngr.GetRtMngr().PopRenderTarget();

                auto flush_light = _engine->EffectMngr.Effects.FlushLight;
                FO_VERIFY_AND_THROW(flush_light, "Flush light effect is null");

                if (flush_light->IsNeedCameraBuf()) {
                    const ipos32 anchor_hex_pos = GetHexMapPos(mpos(0, 0));
                    const ipos32 hex_center = {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
                    const fpos32 anchor_world = fpos32(anchor_hex_pos + hex_center) - _scrollOffset;
                    const auto rt_size = _rtLight->GetTexture()->Size;
                    const float32_t sw_f = numeric_cast<float32_t>(_screenSize.width);
                    const float32_t sh_f = numeric_cast<float32_t>(_screenSize.height);
                    auto& cam_buf = flush_light->CameraBuf = RenderEffect::CameraBuffer();
                    cam_buf->MapAnchorScreenPos[0] = step_xf - (anchor_world.x + source_x) / sw_f;
                    cam_buf->MapAnchorScreenPos[1] = step_yf - (anchor_world.y + source_y) / sh_f;
                    cam_buf->MapAnchorScreenPos[2] = numeric_cast<float32_t>(rt_size.width) / sw_f;
                    cam_buf->MapAnchorScreenPos[3] = numeric_cast<float32_t>(rt_size.height) / sh_f;
                    cam_buf->ChunkScreenAnchor[0] = (step_xf - source_x / sw_f) * draw_scale.x;
                    cam_buf->ChunkScreenAnchor[1] = (step_yf - source_y / sh_f) * draw_scale.y;
                    cam_buf->ChunkScreenAnchor[2] = cam_buf->MapAnchorScreenPos[2] * draw_scale.x;
                    cam_buf->ChunkScreenAnchor[3] = cam_buf->MapAnchorScreenPos[3] * draw_scale.y;
                }

                _rtLight->SetCustomDrawEffect(flush_light);
                _engine->SprMngr.DrawRenderTarget(_rtLight, true);
                _engine->OnRenderMap_AfterLighting.Fire(this, draw_area);
            }

            // Flat sprites
            _engine->OnRenderMap_BeforeFlatSprites.Fire(this, draw_area);
            FO_VERIFY_AND_THROW(_engine->EffectMngr.Effects.Flat, "Flat effect is null");
            _engine->SprMngr.DrawSprites(_mapSprites, draw_area, false, DrawOrderType::AfterLight, DrawOrderType::FlatEnd, GetMapDayColor(), _engine->EffectMngr.Effects.Flat);
            _engine->OnRenderMap_AfterFlatSprites.Fire(this, draw_area);

            // Other sprites, with fog layers interleaved by their draw order
            _engine->OnRenderMap_BeforeSprites.Fire(this, draw_area);
            DrawSpritesWithFog(draw_area);
            _engine->OnRenderMap_AfterSprites.Fire(this, draw_area);

            // Fog layers drawn on top of all sprites (DrawOrderType::Last)
            if (!_engine->Settings->DisableFog && !_mapperMode && HasFogLayers()) {
                _engine->OnRenderMap_BeforeFog.Fire(this, draw_area);
                DrawFogSlot(draw_area, DrawOrderType::Last);
                _engine->OnRenderMap_AfterFog.Fire(this, draw_area);
            }

            _engine->OnRenderMap_AfterSpritesAndFog.Fire(this, draw_area);

            // Draw streched render target
            if (!direct_draw) {
                _engine->OnRenderMap_BeforeFlushMap.Fire(this, draw_area);
                _engine->SprMngr.GetRtMngr().PopRenderTarget();
                auto flush_map = _engine->EffectMngr.Effects.FlushMap;
                FO_VERIFY_AND_THROW(flush_map, "Flush map effect is null");
                _rtMap->SetCustomDrawEffect(flush_map);

                if (flush_map->IsNeedIndoorMaskTex()) {
                    if (_rtIndoorMask && !_engine->Settings->DisableIndoorMask) {
                        _engine->SprMngr.GetRtMngr().PushRenderTarget(_rtIndoorMask);
                        _engine->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);
                        FO_VERIFY_AND_THROW(_engine->EffectMngr.Effects.Roof, "Roof effect is null");
                        _engine->SprMngr.DrawSprites(_indoorMaskSprites, draw_area, false, DrawOrderType::Roof, DrawOrderType::Roof4, GetMapDayColor(), _engine->EffectMngr.Effects.Roof);
                        _engine->SprMngr.GetRtMngr().PopRenderTarget();
                        flush_map->IndoorMaskTex = _rtIndoorMask->GetTexture();
                    }
                    else {
                        flush_map->IndoorMaskTex = nullptr;
                    }
                }

                if (flush_map->IsNeedCameraBuf()) {
                    // World-anchored UV affine basis: world_uv = .xy + TexCoord * .zw, where
                    //   .xy = world UV at TexCoord(0,0) for this chunk
                    //   .zw = world UV per TexCoord unit
                    // The shader's TexCoord does NOT span [0,1] across the chunk — `_rtMap` is
                    // sized (screen_w + MAP_HEX_WIDTH, screen_h + 2*MAP_HEX_LINE_HEIGHT) so
                    // sprites that overlap the screen edge render fully, while the FlushMap
                    // source_rect samples only the screen_w × screen_h content region. So
                    // TexCoord ranges roughly [source_x/rt_w, (source_x+screen_w)/rt_w], with
                    // max ~screen_w/rt_w (~0.95 with default hex padding), not 1.0. An affine
                    // basis `anchor + TexCoord * (rt_size/screen_size)` cancels the rt/screen
                    // and sub-pixel discrepancies, so adjacent chunks emit a continuous ps at
                    // the seam (older `ps = TexCoord - anchor` form had a hex-sized gap there).
                    // Pre-zoom anchor (no GetSpritesZoom() multiply) keeps ps zoom-invariant: a
                    // fixed world position produces the same ps at any zoom, so noise patterns
                    // stay pinned to the world during zoom in/out instead of drifting.
                    const ipos32 anchor_hex_pos = GetHexMapPos(mpos(0, 0));
                    const ipos32 hex_center = {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
                    const fpos32 anchor_world = fpos32(anchor_hex_pos + hex_center) - _scrollOffset;
                    const auto rt_size = _rtMap->GetTexture()->Size;
                    const float32_t sw_f = numeric_cast<float32_t>(_screenSize.width);
                    const float32_t sh_f = numeric_cast<float32_t>(_screenSize.height);
                    auto& cam_buf = flush_map->CameraBuf = RenderEffect::CameraBuffer();
                    cam_buf->MapAnchorScreenPos[0] = step_xf - (anchor_world.x + source_x) / sw_f;
                    cam_buf->MapAnchorScreenPos[1] = step_yf - (anchor_world.y + source_y) / sh_f;
                    cam_buf->MapAnchorScreenPos[2] = numeric_cast<float32_t>(rt_size.width) / sw_f;
                    cam_buf->MapAnchorScreenPos[3] = numeric_cast<float32_t>(rt_size.height) / sh_f;
                    // Screen-anchored basis: screen_uv = .xy + TexCoord * .zw — UV continuous
                    // across the full screen (regardless of chunking) for effects that must
                    // stay attached to the screen frame (vignette, sun bleach, grain). Same
                    // rt-vs-screen scaling correction as MapAnchorScreenPos: TexCoord max is
                    // ~sw/rt_w not 1.0, so the per-TexCoord screen UV step is
                    // `(rt_w/sw) * draw_scale`, and the chunk anchor backs out source_x.
                    cam_buf->ChunkScreenAnchor[0] = (step_xf - source_x / sw_f) * draw_scale.x;
                    cam_buf->ChunkScreenAnchor[1] = (step_yf - source_y / sh_f) * draw_scale.y;
                    cam_buf->ChunkScreenAnchor[2] = cam_buf->MapAnchorScreenPos[2] * draw_scale.x;
                    cam_buf->ChunkScreenAnchor[3] = cam_buf->MapAnchorScreenPos[3] * draw_scale.y;
                }

                FO_VERIFY_AND_THROW(_rtMap, "Map render target is not allocated");
                _engine->SprMngr.DrawRenderTarget(_rtMap, false, &source_rect, &target_rect);
                _engine->OnRenderMap_AfterFlushMap.Fire(this, draw_area);
            }
        }
    }
}

void MapView::DrawSpritesWithFog(const irect32& draw_area)
{
    FO_STACK_TRACE_ENTRY();

    const ucolor day_color = GetMapDayColor();
    FO_VERIFY_AND_THROW(_engine->EffectMngr.Effects.Flat, "Flat effect is null");
    FO_VERIFY_AND_THROW(_engine->EffectMngr.Effects.Generic, "Generic effect is null");
    FO_VERIFY_AND_THROW(_engine->EffectMngr.Effects.Roof, "Roof effect is null");

    struct SpriteDrawSegment
    {
        DrawOrderType From {};
        DrawOrderType To {};
        ptr<RenderEffect> Effect;
    };

    const SpriteDrawSegment sprite_draw_segments[] = {
        {DrawOrderType::AfterLight, DrawOrderType::FlatEnd, _engine->EffectMngr.Effects.Flat},
        {DrawOrderType::NormalBegin, DrawOrderType::NormalEnd, _engine->EffectMngr.Effects.Generic},
        {DrawOrderType::Roof, DrawOrderType::Last, _engine->EffectMngr.Effects.Roof},
    };

    if (_engine->Settings->DisableFog || _mapperMode || !HasFogLayers()) {
        for (const SpriteDrawSegment& segment : sprite_draw_segments) {
            _engine->SprMngr.DrawSprites(_mapSprites, draw_area, true, segment.From, segment.To, day_color, segment.Effect);
        }

        return;
    }

    const size_t last_sprite_order = static_cast<size_t>(DrawOrderType::Last);

    // Fog slots below the main sprite pass (draw order < Light) blit first, at ground level.
    for (size_t order = 0; order < static_cast<size_t>(DrawOrderType::Light); order++) {
        DrawFogSlot(draw_area, static_cast<DrawOrderType>(order));
    }

    for (const SpriteDrawSegment& segment : sprite_draw_segments) {
        const size_t segment_begin = static_cast<size_t>(segment.From);
        const size_t segment_end = static_cast<size_t>(segment.To);
        FO_VERIFY_AND_THROW(segment_begin <= segment_end, "Requested fogged draw-order segment has inverted boundaries", segment.From, segment.To);
        size_t sprite_segment_begin = segment_begin;

        for (size_t order = segment_begin; order <= segment_end && order < last_sprite_order; order++) {
            if (_fogs[order].empty()) {
                continue;
            }

            const DrawOrderType boundary = static_cast<DrawOrderType>(order);
            _engine->SprMngr.DrawSprites(_mapSprites, draw_area, true, static_cast<DrawOrderType>(sprite_segment_begin), boundary, day_color, segment.Effect);
            DrawFogSlot(draw_area, boundary);

            sprite_segment_begin = order + 1;
        }

        if (sprite_segment_begin <= segment_end) {
            _engine->SprMngr.DrawSprites(_mapSprites, draw_area, true, static_cast<DrawOrderType>(sprite_segment_begin), segment.To, day_color, segment.Effect);
        }
    }
}

void MapView::DrawFogSlot(const irect32& draw_area, DrawOrderType draw_order)
{
    FO_STACK_TRACE_ENTRY();

    for (auto& fog : _fogs[static_cast<size_t>(draw_order)]) {
        const_span<PrimitivePoint> fog_points = fog->Shape.GetPoints();

        if (fog_points.empty()) {
            continue;
        }

        auto fog_effect = fog->CustomFogEffect ? fog->CustomFogEffect : _engine->EffectMngr.Effects.Fog;
        FO_VERIFY_AND_THROW(fog_effect, "Fog effect is null");

        if (fog->CustomFlushEffect) {
            // Custom flush (e.g. base-look fog): rasterize the honest hexagon profile into the light
            // target, then composite it onto the scene with the custom effect. The effect shapes the
            // analytic oval, cold tint, drifting mist edge, and distance depth from the fog's own tunable
            // fields, passed below as script values (fog center + semi-axes in light-target UV, plus knobs).
            FO_VERIFY_AND_THROW(_rtLight, "Lighting render target is not allocated");
            _engine->SprMngr.GetRtMngr().PushRenderTarget(_rtLight);
            _engine->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);

            _engine->SprMngr.DrawPoints(fog_points, RenderPrimitiveType::TriangleStrip, &draw_area, fog_effect);

            _engine->SprMngr.GetRtMngr().PopRenderTarget();

            auto flush_effect = fog->CustomFlushEffect;
            FO_VERIFY_AND_THROW(flush_effect, "Flush effect is null");

            if (flush_effect->IsNeedScriptValueBuf() && fog_points.size() >= 3) {
                const auto to_target = [&draw_area](const PrimitivePoint& p) -> ipos32 {
                    ipos32 pos = p.PointPos;
                    if (p.PointOffset) {
                        pos += *p.PointOffset;
                    }
                    return pos - ipos32 {draw_area.x, draw_area.y};
                };

                // Index 2 is the first inserted center point of the triangle strip; the ring points fan
                // around it. Semi-axes are the farthest extent from the center along each axis.
                const ipos32 center = to_target(fog_points[2]);
                int32_t semi_x = 0;
                int32_t semi_y = 0;

                for (const auto& p : fog_points) {
                    const ipos32 s = to_target(p);
                    semi_x = std::max(semi_x, std::abs(s.x - center.x));
                    semi_y = std::max(semi_y, std::abs(s.y - center.y));
                }

                auto rt_tex = _rtLight->GetTexture();
                const auto rt_w = numeric_cast<float32_t>(rt_tex->Size.width);
                const auto rt_h = numeric_cast<float32_t>(rt_tex->Size.height);
                float32_t center_v = numeric_cast<float32_t>(center.y) / rt_h;

                if (rt_tex->FlippedHeight) {
                    center_v = 1.0f - center_v;
                }

                // World-anchored noise offset: the origin's absolute world pos in light-target UV. Added to
                // (TexCoord - center) in the shader it yields each pixel's world position, so the rim noise
                // is fixed to the world — stable under camera scroll, streaming as the fog moves through it.
                const float32_t world_x = numeric_cast<float32_t>(fog->OriginWorldPos.x) / rt_w;
                const float32_t world_y = numeric_cast<float32_t>(fog->OriginWorldPos.y) / rt_h;

                const float32_t values[14] = {numeric_cast<float32_t>(center.x) / rt_w, center_v, //
                    numeric_cast<float32_t>(semi_x) / rt_w, numeric_cast<float32_t>(semi_y) / rt_h, //
                    fog->OvalRoundness, fog->EdgeNoise, fog->Depth, fog->ClearRadius, //
                    numeric_cast<float32_t>(fog->TintColor.comp.r) / 255.0f, numeric_cast<float32_t>(fog->TintColor.comp.g) / 255.0f, numeric_cast<float32_t>(fog->TintColor.comp.b) / 255.0f, //
                    0.0f, // [2].w reserved
                    world_x, world_y}; // [3].xy world-anchored noise offset
                _engine->EffectMngr.SetEffectScriptValues(flush_effect, 0, const_span<float32_t> {values, 14});
            }

            _rtLight->SetCustomDrawEffect(flush_effect);
            _engine->SprMngr.DrawRenderTarget(_rtLight, true);
        }
        else {
            // Without a custom flush effect: draw the fog points straight into the scene
            _engine->SprMngr.DrawPoints(fog_points, RenderPrimitiveType::TriangleStrip, &draw_area, fog_effect);
        }
    }
}

auto MapView::DrawEntitySprite(ptr<ClientEntity> entity, ptr<RenderEffect> effect, ucolor color, int32_t padding) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(padding >= 0, "Sprite draw padding must not be negative");

    if (!_currentRenderDrawArea.has_value()) {
        throw ScriptException("Map sprite draw is available only inside map render stage events");
    }

    nptr<const MapSprite> mspr;

    if (auto cr = entity.dyn_cast<CritterHexView>()) {
        if (cr->GetMap() != this || !cr->IsMapSpriteValid()) {
            return false;
        }

        mspr = cr->GetMapSprite();
    }
    else if (auto item = entity.dyn_cast<ItemHexView>(); item) {
        if (item->GetMap() != this || !item->IsMapSpriteValid()) {
            return false;
        }

        mspr = item->GetMapSprite();
    }
    else {
        throw ScriptException("Entity has no map sprite", entity->GetId());
    }

    FO_VERIFY_AND_THROW(mspr, "Entity has no map sprite to draw");

    if (mspr->IsHidden()) {
        return false;
    }

    auto spr = mspr->GetSprite();

    if (!spr) {
        return false;
    }

    irect32 mspr_rect = mspr->GetDrawRect();
    mspr_rect.x -= _currentRenderDrawArea->x;
    mspr_rect.y -= _currentRenderDrawArea->y;

    if (mspr_rect.x > _currentRenderDrawArea->width || mspr_rect.x + mspr_rect.width < 0 || mspr_rect.y > _currentRenderDrawArea->height || mspr_rect.y + mspr_rect.height < 0) {
        return false;
    }

    _engine->SprMngr.DrawSpriteWithEffect(spr, mspr_rect.pos(), color, effect, padding);
    return true;
}

void MapView::PrepareFogToDraw()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& fog_slot : _fogs) {
        for (auto it = fog_slot.begin(); it != fog_slot.end();) {
            if ((*it)->Disposed || (*it)->GetRefCount() == 1) {
                it = fog_slot.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    if (_engine->Settings->DisableFog) {
        return;
    }
    if (_mapperMode) {
        return;
    }

    FogShape::Input input;
    input.MapHexWidth = _engine->Settings->MapHexWidth;
    input.MapHexHeight = _engine->Settings->MapHexHeight;
    input.MapSize = _mapSize;
    input.FrameTime = _engine->GameTime.GetFrameTime();
    input.TraceBulletToBlock = [this](mpos start_hex, mpos target_hex, int32_t dist, bool check_shoot_blocks) {
        mpos block_hex;
        TraceBullet(start_hex, target_hex, dist, 0.0f, nullptr, CritterFindType::Any, nullptr, &block_hex, nullptr, check_shoot_blocks);
        return block_hex;
    };

    for (auto& fog : _fogs | std::views::join) {
        auto fog_input = input;
        fog_input.FogExtraLength = fog->ExtraLength;
        fog_input.FogTransitionDuration = fog->TransitionDuration;
        fog_input.Enabled = fog->Enabled;
        fog_input.Distance = fog->Distance;
        fog_input.Radius = fog->Radius;
        fog_input.OverlayColor = fog->OverlayColor;
        fog_input.CenterColor = fog->CenterColor;
        fog_input.TraceMode = fog->Traced ? FogShape::TraceModeType::Overlay : FogShape::TraceModeType::None;
        fog_input.CheckShootBlocks = fog->CheckShootBlocks;

        ipos32 base_draw_offset {};
        ipos32 draw_offset {};

        if (fog->FollowCritter) {
            auto cr = GetCritter(fog->OriginCritterId);
            if (!cr) {
                fog->Shape.Clear();
                continue;
            }

            fog_input.FogOrigin.Valid = true;
            fog_input.FogOrigin.BaseHex = cr->GetHex();
            fog_input.FogOrigin.LookDistance = cr->GetLookDistance();

            base_draw_offset = GeometryHelper::GetHexOffset(_screenRawHex, ipos32(fog_input.FogOrigin.BaseHex));
            draw_offset = base_draw_offset + *cr->GetSpriteOffsetPtr();
            // Absolute world-pixel position of the origin (camera-independent), incl. the sub-hex sprite
            // offset for smooth flow; the shader anchors the rim noise to it so it streams as the fog moves.
            fog->OriginWorldPos = GeometryHelper::GetHexOffset(ipos32 {}, ipos32(fog_input.FogOrigin.BaseHex)) + *cr->GetSpriteOffsetPtr();
        }
        else {
            fog_input.FogOrigin.Valid = true;
            fog_input.FogOrigin.BaseHex = fog->OriginHex;
            fog_input.FogOrigin.LookDistance = fog->Radius;

            base_draw_offset = GeometryHelper::GetHexOffset(_screenRawHex, ipos32(fog->OriginHex));
            draw_offset = base_draw_offset;
            fog->OriginWorldPos = GeometryHelper::GetHexOffset(ipos32 {}, ipos32(fog->OriginHex));
        }

        fog->Shape.SetBaseDrawOffset(base_draw_offset);
        fog->Shape.SetDrawOffset(draw_offset);
        fog->Shape.Prepare(fog_input);
    }
}

auto MapView::IsOutsideArea(mpos hex) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const irect32 scroll_area = GetScrollAxialArea();

    if (!scroll_area.is_zero()) {
        const ipos32 axial_hex = GeometryHelper::GetHexAxialCoord(hex);

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

    return _engine->Settings->ScrollMouseLeft || _engine->Settings->ScrollKeybLeft || _engine->Settings->ScrollMouseRight || //
        _engine->Settings->ScrollKeybRight || _engine->Settings->ScrollMouseUp || _engine->Settings->ScrollKeybUp || //
        _engine->Settings->ScrollMouseDown || _engine->Settings->ScrollKeybDown;
}

void MapView::ProcessScroll(float32_t dt)
{
    FO_STACK_TRACE_ENTRY();

    const bool is_manual_scrolling = IsManualScrolling();

    if (is_manual_scrolling && _autoScrollCanStop) {
        _autoScrollActive = false;
    }

    fpos32 scroll;

    if (_autoScrollActive) {
        const auto scroll_step = numeric_cast<float32_t>(_autoScrollSpeed) / 10000.0f * dt;
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

        if (_engine->Settings->ScrollMouseLeft || _engine->Settings->ScrollKeybLeft) {
            scroll.x -= 1.0f;
        }
        if (_engine->Settings->ScrollMouseRight || _engine->Settings->ScrollKeybRight) {
            scroll.x += 1.0f;
        }
        if (_engine->Settings->ScrollMouseUp || _engine->Settings->ScrollKeybUp) {
            scroll.y -= 1.0f;
        }
        if (_engine->Settings->ScrollMouseDown || _engine->Settings->ScrollKeybDown) {
            scroll.y += 1.0f;
        }

        if (std::abs(scroll.x) < 0.1f && std::abs(scroll.y) < 0.1f) {
            return;
        }

        const auto zoom = GetSpritesZoom();
        const auto scroll_step = numeric_cast<float32_t>(_engine->Settings->ScrollSpeed) / 1000.f / zoom * dt;
        scroll.x *= scroll_step;
        scroll.y *= scroll_step;
    }

    InstantScroll(scroll);
}

void MapView::ChangeZoom(float32_t new_zoom, fpos32 anchor)
{
    FO_STACK_TRACE_ENTRY();

    if (!_engine->Settings->MapZoomEnabled) {
        return;
    }

    _zoomAnchor = anchor;
    SetSpritesZoomTarget(new_zoom);
}

void MapView::ProcessZoom(float32_t dt)
{
    FO_STACK_TRACE_ENTRY();

    if (!_engine->Settings->MapZoomEnabled) {
        return;
    }

    const float32_t init_zoom = GetSpritesZoom();
    const float32_t target_zoom = GetSpritesZoomTarget();

    if (is_float_equal(init_zoom, target_zoom)) {
        return;
    }

    const float32_t min_zoom = _scrollCheckEnabled ? _minZoomScroll : GameSettings::MIN_ZOOM;
    constexpr float32_t max_zoom = GameSettings::MAX_ZOOM;
    const float32_t clamped_target_zoom = std::clamp(target_zoom, min_zoom, max_zoom);

    if (!is_float_equal(target_zoom, clamped_target_zoom)) {
        SetSpritesZoomTarget(clamped_target_zoom);
    }

    if (is_float_equal(init_zoom, clamped_target_zoom)) {
        return;
    }

    if (init_zoom >= min_zoom && init_zoom <= max_zoom) {
        const int32_t zoom_speed = _engine->Settings->ZoomSpeed;
        constexpr float32_t zoom_stop_bias = 0.001f;

        float32_t new_zoom = lerp(init_zoom, clamped_target_zoom, dt * numeric_cast<float32_t>(zoom_speed) / 10000.0f);

        if (std::abs(new_zoom - clamped_target_zoom) < zoom_stop_bias) {
            new_zoom = clamped_target_zoom;
        }

        InstantZoom(new_zoom, _zoomAnchor);
    }
    else {
        InstantZoom(clamped_target_zoom, _zoomAnchor);
    }
}

void MapView::InstantZoom(float32_t new_zoom, fpos32 anchor)
{
    FO_STACK_TRACE_ENTRY();

    if (!_engine->Settings->MapZoomEnabled) {
        return;
    }

    FO_VERIFY_AND_THROW(new_zoom >= GameSettings::MIN_ZOOM, "Requested map zoom is smaller than the engine minimum", new_zoom, GameSettings::MIN_ZOOM);
    FO_VERIFY_AND_THROW(new_zoom <= GameSettings::MAX_ZOOM, "Requested map zoom is larger than the engine maximum", new_zoom, GameSettings::MAX_ZOOM);

    const float32_t init_zoom = GetSpritesZoom();
    const isize32 init_size = GetViewSize();

    SetSpritesZoom(new_zoom);

    const isize32 new_size = GetViewSize();
    const bool size_changed = init_size != new_size;

    _viewSize.width = numeric_cast<float32_t>(_screenSize.width) / new_zoom;
    _viewSize.height = numeric_cast<float32_t>(_screenSize.height) / new_zoom;

    if (size_changed) {
        const int32_t size_diff_width = new_size.width - init_size.width;
        const int32_t size_diff_height = new_size.height - init_size.height;

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

        RebuildFog();
        _engine->OnScreenScroll.Fire();
    }

    const float32_t screen_width = numeric_cast<float32_t>(_screenSize.width);
    const float32_t screen_height = numeric_cast<float32_t>(_screenSize.height);
    const float32_t changed_width = screen_width / new_zoom - screen_width / init_zoom;
    const float32_t changed_height = screen_height / new_zoom - screen_height / init_zoom;

    InstantScroll(-fpos32(changed_width * anchor.x, changed_height * anchor.y));
}

void MapView::InstantScroll(fpos32 scroll)
{
    FO_STACK_TRACE_ENTRY();

    _scrollOffset += scroll;

    if (_scrollCheckEnabled) {
        const irect32 scroll_area = GetScrollAxialArea();

        if (!scroll_area.is_zero()) {
            const fpos32 screen_pos = fpos32(GeometryHelper::GetHexPos(_screenRawHex));
            constexpr ipos32 half_hex = {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
            const float32_t zoom = GetSpritesZoom();
            const fpos32 view_size = fpos32(numeric_cast<float32_t>(_screenSize.width), numeric_cast<float32_t>(_screenSize.height)) / zoom;
            const fpos32 lt_pos = screen_pos + _scrollOffset;
            const fpos32 rb_pos = screen_pos + view_size + _scrollOffset;
            const float32_t area_l = numeric_cast<float32_t>(scroll_area.x * GameSettings::MAP_HEX_WIDTH / 2 + half_hex.x);
            const float32_t area_t = numeric_cast<float32_t>(scroll_area.y * GameSettings::MAP_HEX_LINE_HEIGHT + half_hex.y);
            const float32_t area_r = numeric_cast<float32_t>((scroll_area.x + scroll_area.width) * GameSettings::MAP_HEX_WIDTH / 2 + half_hex.x);
            const float32_t area_b = numeric_cast<float32_t>((scroll_area.y + scroll_area.height) * GameSettings::MAP_HEX_LINE_HEIGHT + half_hex.y);

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

    const float32_t max_scroll_width = numeric_cast<float32_t>(_maxScroll.width);
    const float32_t max_scroll_height = numeric_cast<float32_t>(_maxScroll.height);
    int32_t xmove = 0;
    int32_t ymove = 0;

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

    SetScrollOffset(_scrollOffset.round<int32_t>());

    if (xmove != 0 || ymove != 0) {
        RebuildMapOffset({xmove, ymove});
        _engine->OnScreenScroll.Fire();
    }
}

void MapView::ScrollToHex(mpos hex, ipos16 hex_offset, int32_t speed, bool can_stop)
{
    FO_STACK_TRACE_ENTRY();

    const ipos32 hex_pos = GeometryHelper::GetHexOffset(GetCenterRawHex(), ipos32(hex));
    _autoScrollActive = false;
    ApplyScrollOffset(hex_pos - ipos32(hex_offset) - GetScrollOffset().round<int32_t>(), speed, can_stop);
}

void MapView::ApplyScrollOffset(ipos32 offset, int32_t speed, bool can_stop)
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
        constexpr float32_t min_zoom_bias = 1.1f;
        const float32_t min_zoom_x = numeric_cast<float32_t>(_screenSize.width) / numeric_cast<float32_t>(scroll_area.width * (GameSettings::MAP_HEX_WIDTH / 2)) * min_zoom_bias;
        const float32_t min_zoom_y = numeric_cast<float32_t>(_screenSize.height) / numeric_cast<float32_t>(scroll_area.height * GameSettings::MAP_HEX_LINE_HEIGHT) * min_zoom_bias;

        _minZoomScroll = std::max(min_zoom_x, min_zoom_y);

        if (!_mapperMode && _minZoomScroll > 1.0f) {
            _minZoomScroll = GameSettings::MIN_ZOOM;
        }

        _minZoomScroll = std::min(_minZoomScroll, GameSettings::MAX_ZOOM);
    }
    else {
        _minZoomScroll = GameSettings::MIN_ZOOM;
    }

    if (_scrollCheckEnabled) {
        SetSpritesZoomTarget(std::max(_minZoomScroll, GetSpritesZoomTarget()));
    }
}

void MapView::SetScrollCheck(bool enabled)
{
    FO_STACK_TRACE_ENTRY();

    if (_scrollCheckEnabled == enabled) {
        return;
    }

    _scrollCheckEnabled = enabled;

    if (enabled) {
        RefreshMinZoom();

        const float32_t target_zoom = GetSpritesZoomTarget();

        if (!is_float_equal(GetSpritesZoom(), target_zoom)) {
            InstantZoom(target_zoom, {0.5f, 0.5f});
        }

        InstantScroll({});
    }
}

void MapView::AddCritterToField(ptr<CritterHexView> cr)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = cr->GetHex();
    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Client map cannot add critter to a field outside map bounds", GetId(), cr->GetId(), hex, _mapSize);
    auto field = _hexField->GetCellForWriting(hex);

    vec_add_unique_value(field->OriginCritters, cr);
    vec_add_unique_value(field->Critters, cr);
    RecacheHexFlags(field);
    SetMultihexCritter(cr, true);
    UpdateCritterLightSource(cr);

    if (!_mapLoading && IsHexToDraw(hex)) {
        DrawHexCritter(cr, field, hex);
    }
}

void MapView::RemoveCritterFromField(ptr<CritterHexView> cr)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = cr->GetHex();
    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Client map cannot remove critter from a field outside map bounds", GetId(), cr->GetId(), hex, _mapSize);
    auto field = _hexField->GetCellForWriting(hex);

    vec_remove_unique_value(field->OriginCritters, cr);
    vec_remove_unique_value(field->Critters, cr);
    RecacheHexFlags(field);
    SetMultihexCritter(cr, false);
    FinishLightSource(cr->GetId());
    cr->InvalidateSprite();
}

auto MapView::GetCritter(ident_t id) -> nptr<CritterHexView>
{
    FO_STACK_TRACE_ENTRY();

    if (!id) {
        return nullptr;
    }

    const auto it = _crittersMap.find(id);
    if (it == _crittersMap.end()) {
        return nullptr;
    }

    return it->second;
}

auto MapView::GetNonDeadCritter(mpos hex) -> nptr<CritterHexView>
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Critters.empty()) {
        return nullptr;
    }

    auto field2 = _hexField->GetCellForWriting(hex);

    for (ptr<CritterHexView> cr : field2->Critters) {
        if (!cr->IsDead()) {
            return cr;
        }
    }

    return nullptr;
}

auto MapView::AddReceivedCritter(ident_t id, hstring pid, mpos hex, mdir dir, const vector<vector<uint8_t>>& data, bool fade_in) -> ptr<CritterHexView>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(id, "Received client map critter has an empty entity id", GetId(), pid, hex);
    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Received client map critter targets a hex outside map bounds", GetId(), id, pid, hex, _mapSize);

    auto proto = _engine->GetProtoCritter(pid);
    FO_VERIFY_AND_THROW(proto, "Critter prototype is missing");
    auto cr = SafeAlloc::MakeRefCounted<CritterHexView>(this, id, proto.as_ptr());

    cr->RestoreData(data);
    cr->SetHex(hex);
    cr->ChangeDir(dir);

    // Detect re-addition: if the previous view is still around (fading out), the new view inherits its
    // alpha in AddCritterInternal, so we must skip the FadeUp() reset to keep the transition smooth.
    const bool was_present = !!GetCritter(id);

    auto added = AddCritterInternal(cr);

    if (fade_in && !was_present) {
        added->FadeUp();
    }

    return added;
}

auto MapView::AddMapperCritter(hstring pid, mpos hex, mdir dir, nptr<const Properties> props, ident_t id) -> ptr<CritterHexView>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper critter creation was requested outside mapper mode", GetId(), pid, hex, dir);
    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Mapper critter creation targets a hex outside map bounds", GetId(), pid, hex, _mapSize);

    auto proto = _engine->GetProtoCritter(pid);
    FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

    auto cr = SafeAlloc::MakeRefCounted<CritterHexView>(this, id ? id : GenTempEntityId(), proto.as_ptr(), props);

    cr->SetHex(hex);
    cr->ChangeDir(dir);

    return AddCritterInternal(cr);
}

auto MapView::AddCritterInternal(ptr<CritterHexView> cr) -> ptr<CritterHexView>
{
    FO_STACK_TRACE_ENTRY();

    if (cr->GetId()) {
        if (auto prev_cr = GetCritter(cr->GetId())) {
            cr->InheritAlphaFrom(prev_cr);
            DestroyCritter(prev_cr);
        }

        _crittersMap.emplace(cr->GetId(), cr);
    }

    cr->SetMapId(GetId());
    cr->Init();

    vec_add_unique_value(_critters, cr.hold_ref());
    AddCritterToField(cr);
    return cr;
}

void MapView::DestroyCritter(ptr<CritterHexView> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(cr->GetMap() == this, "Critter destroy requested for a critter attached to a different client map", cr->GetId(), cr->GetMap()->GetId(), GetId());

    auto cr_ref_holder = cr.hold_ref();
    vec_remove_unique_value(_critters, cr_ref_holder);

    if (cr->GetId()) {
        const auto it_map = _crittersMap.find(cr->GetId());
        FO_VERIFY_AND_THROW(it_map != _crittersMap.end(), "Lookup failed in critters map");
        _crittersMap.erase(it_map);
    }

    RemoveCritterFromField(cr);
    CleanLightSourceOffsets(cr->GetId());
    cr->DestroySelf();
}

auto MapView::GetCrittersOnHex(mpos hex, CritterFindType find_type) -> vector<ptr<CritterHexView>>
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Critters.empty()) {
        return {};
    }

    auto field2 = _hexField->GetCellForWriting(hex);
    vector<ptr<CritterHexView>> critters;
    critters.reserve(field2->Critters.size());

    for (ptr<CritterHexView> cr : field2->Critters) {
        if (cr->CheckFind(find_type)) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

auto MapView::GetCrittersOnHex(mpos hex, CritterFindType find_type) const -> vector<ptr<const CritterHexView>>
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Critters.empty()) {
        return {};
    }

    vector<ptr<const CritterHexView>> critters;
    critters.reserve(field.Critters.size());

    for (ptr<const CritterHexView> cr : field.Critters) {
        if (cr->CheckFind(find_type)) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

auto MapView::GetCrittersInRadius(mpos hex, int32_t radius, CritterFindType find_type) -> vector<ptr<CritterHexView>>
{
    FO_STACK_TRACE_ENTRY();

    vector<ptr<CritterHexView>> critters;

    const int32_t hexes_in_radius = GeometryHelper::HexesInRadius(radius);
    unordered_set<ptr<const CritterHexView>> seen;
    seen.reserve(numeric_cast<size_t>(hexes_in_radius));

    for (int32_t i = 0; i < hexes_in_radius; i++) {
        if (mpos cur_hex = hex; GeometryHelper::MoveHexAroundAway(cur_hex, i, _mapSize)) {
            const auto& field = _hexField->GetCellForReading(cur_hex);

            if (field.Critters.empty()) {
                continue;
            }

            auto field2 = _hexField->GetCellForWriting(cur_hex);

            for (ptr<CritterHexView> cr : field2->Critters) {
                if (seen.insert(cr).second && cr->CheckFind(find_type)) {
                    critters.emplace_back(cr);
                }
            }
        }
    }

    return critters;
}

void MapView::MoveCritter(ptr<CritterHexView> cr, mpos to_hex, bool smoothly)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(cr->GetMap() == this, "Critter move requested for a critter attached to a different client map", cr->GetId(), cr->GetMap()->GetId(), GetId());
    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(to_hex), "Critter move target hex is outside client map bounds", cr->GetId(), to_hex, _mapSize);

    const auto cur_hex = cr->GetHex();

    if (cur_hex == to_hex) {
        return;
    }

    RemoveCritterFromField(cr);

    cr->SetHex(to_hex);

    if (smoothly) {
        const auto hex_offset = GeometryHelper::GetHexOffset(to_hex, cur_hex);

        cr->AddExtraOffs(hex_offset);
    }

    AddCritterToField(cr);
}

void MapView::ReapplyCritterView(ptr<CritterHexView> cr)
{
    FO_STACK_TRACE_ENTRY();

    RemoveCritterFromField(cr);
    AddCritterToField(cr);
}

void MapView::SetMultihexCritter(ptr<CritterHexView> cr, bool set)
{
    FO_STACK_TRACE_ENTRY();

    const auto multihex = cr->GetMultihex();

    if (multihex != 0) {
        const auto hex = cr->GetHex();
        const auto hexes_around = GeometryHelper::HexesInRadius(multihex);

        for (int32_t i = 1; i < hexes_around; i++) {
            if (auto multihex_hex = hex; GeometryHelper::MoveHexAroundAway(multihex_hex, i, _mapSize)) {
                auto field = _hexField->GetCellForWriting(multihex_hex);

                if (set) {
                    vec_add_unique_value(field->Critters, cr);
                }
                else {
                    vec_remove_unique_value(field->Critters, cr);
                }

                RecacheHexFlags(field);
            }
        }
    }
}

void MapView::DrawHexCritter(ptr<CritterHexView> cr, ptr<Field> field, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    if (_mapperMode && !_engine->Settings->ShowCrit) {
        return;
    }

    const auto draw_order = cr->IsDead() && !cr->GetDeadDrawNoFlatten() ? DrawOrderType::DeadCritter : DrawOrderType::Critter;
    auto mspr = cr->AddSprite(_mapSprites, draw_order, hex, &field->Offset);
    AddSpriteToChain(field, mspr);
}

auto MapView::GetHexAtScreen(ipos32 screen_pos, mpos& hex, nptr<ipos32> hex_offset) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const ipos32 pos = ScreenToMapPos(screen_pos);
    const ipos32 screen_offset = GeometryHelper::GetHexPos(_screenRawHex);

    // GetHexPos/GetHexPosCoord work from the hex draw origin (cell top-left), but sprites (critters,
    // items) and GetHexScreenPos anchor at the hex visual center = origin + {MAP_HEX_WIDTH/2, MAP_HEX_HEIGHT/2}.
    // Bias the lookup point by -half a hex so we resolve the hex whose visual center is nearest and the
    // returned offset is measured relative to that center (matching the critter HexOffset convention, and
    // staying within +-{MAP_HEX_WIDTH/2, MAP_HEX_HEIGHT/2} without clamping).
    const ipos32 hex_center = {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
    ipos32 offset;
    const ipos32 raw_hex = GeometryHelper::GetHexPosCoord(screen_offset + pos - hex_center, &offset);

    if (_mapSize.is_valid_pos(raw_hex)) {
        hex = _mapSize.from_raw_pos(raw_hex);

        if (hex_offset) {
            *hex_offset = offset;
        }

        return true;
    }

    return false;
}

auto MapView::GetItemAtScreen(ipos32 screen_pos, bool& item_egg, int32_t extra_range, bool check_transparent) -> pair<nptr<ItemHexView>, nptr<const MapSprite>>
{
    FO_STACK_TRACE_ENTRY();

    const auto pos = ScreenToMapPos(screen_pos);

    pair<nptr<ItemHexView>, nptr<const MapSprite>> best {};
    pair<nptr<ItemHexView>, nptr<const MapSprite>> best_egg {};
    int32_t best_sort = std::numeric_limits<int32_t>::min();
    int32_t best_egg_sort = std::numeric_limits<int32_t>::min();

    const auto process_sprite = [&](ptr<ItemHexView> item, ptr<const MapSprite> mspr) {
        const int32_t sort_value = mspr->GetSortValue();

        if (sort_value <= best_sort && sort_value <= best_egg_sort) {
            return;
        }

        const irect32 rect = mspr->GetDrawRect();
        irect32 check_rect = rect;
        check_rect.x -= extra_range;
        check_rect.y -= extra_range;
        check_rect.width += extra_range * 2;
        check_rect.height += extra_range * 2;

        if (pos.x < check_rect.x || pos.x > check_rect.x + check_rect.width || pos.y < check_rect.y || pos.y > check_rect.y + check_rect.height) {
            return;
        }

        const bool potentially_egg = _engine->SprMngr.IsEggTransp(pos, mspr->GetHex(), mspr->GetEggAppearence());

        if (potentially_egg ? sort_value <= best_egg_sort : sort_value <= best_sort) {
            return;
        }

        if (check_transparent) {
            auto spr = mspr->GetSprite();

            if (!spr) {
                return;
            }

            const ipos32 check_pos = {pos.x - rect.x, pos.y - rect.y};

            if (!_engine->SprMngr.SpriteHitTest(spr, check_pos)) {
                return;
            }
        }

        if (potentially_egg) {
            best_egg_sort = sort_value;
            best_egg = {item, mspr};
        }
        else {
            best_sort = sort_value;
            best = {item, mspr};
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

        auto field2 = _hexField->GetCellForWriting(hex);

        for (auto& item : field2->OriginItems) {
            if (item->IsMapSpriteVisible()) {
                process_sprite(item, item->GetMapSprite());
            }
        }

        for (auto&& [item, drawable] : field2->MultihexItems) {
            if (drawable && item->HasExtraMapSprites()) {
                auto extra_map_sprites = item->GetExtraMapSprites();
                FO_VERIFY_AND_THROW(extra_map_sprites, "Extra map sprites collection is null");

                for (const auto& extra_mspr_entry : *extra_map_sprites) {
                    if (extra_mspr_entry.second && extra_mspr_entry.first && extra_mspr_entry.first->GetHex() == hex) {
                        process_sprite(item, extra_mspr_entry.first);
                    }
                }
            }
        }
    }

    if (best.first) {
        item_egg = false;
        return best;
    }

    if (best_egg.first) {
        item_egg = true;
        return best_egg;
    }

    return {};
}

auto MapView::GetCritterAtScreen(ipos32 screen_pos, bool ignore_dead_and_chosen, int32_t extra_range, bool check_transparent) -> pair<nptr<CritterHexView>, nptr<const MapSprite>>
{
    FO_STACK_TRACE_ENTRY();

    const auto pos = ScreenToMapPos(screen_pos);

    nptr<CritterHexView> best = nullptr;
    nptr<const MapSprite> best_mspr = nullptr;
    int32_t best_sort = std::numeric_limits<int32_t>::min();

    for (size_t i = 0; i < _critters.size(); i++) {
        auto cr = _critters[i].as_ptr();

        if (!cr->IsMapSpriteVisible() || cr->IsFinishing()) {
            continue;
        }
        if (ignore_dead_and_chosen && (cr->IsDead() || cr->GetIsChosen())) {
            continue;
        }

        auto mspr = cr->GetMapSprite();
        const int32_t sort_value = mspr->GetSortValue();

        if (sort_value <= best_sort) {
            continue;
        }

        const auto bound_rect = check_transparent ? mspr->GetDrawRect() : mspr->GetViewRect();
        const auto l = bound_rect.x - extra_range;
        const auto t = bound_rect.y - extra_range;
        const auto r = bound_rect.x + bound_rect.width + extra_range;
        const auto b = bound_rect.y + bound_rect.height + extra_range;

        if (pos.x < l || pos.x > r || pos.y < t || pos.y > b) {
            continue;
        }

        if (check_transparent) {
            auto spr = cr->GetSprite();

            if (!spr) {
                continue;
            }

            if (!_engine->SprMngr.SpriteHitTest(spr, {pos.x - bound_rect.x, pos.y - bound_rect.y})) {
                continue;
            }
        }

        best_sort = sort_value;
        best = cr;
        best_mspr = mspr;
    }

    if (!best) {
        return {};
    }
    return {best, best_mspr};
}

auto MapView::GetEntityAtScreen(ipos32 screen_pos, int32_t extra_range, bool check_transparent) -> pair<nptr<ClientEntity>, nptr<const MapSprite>>
{
    FO_STACK_TRACE_ENTRY();

    bool item_egg = false;
    auto item_hit = GetItemAtScreen(screen_pos, item_egg, extra_range, check_transparent);
    auto cr_hit = GetCritterAtScreen(screen_pos, false, extra_range, check_transparent);

    if (cr_hit.first && item_hit.first) {
        if (item_hit.first->IsTransparent() || item_egg || item_hit.second->GetSortValue() <= cr_hit.second->GetSortValue()) {
            item_hit.first = nullptr;
        }
        else {
            cr_hit.first = nullptr;
        }
    }

    if (cr_hit.first) {
        return pair(nptr<ClientEntity>(cr_hit.first), cr_hit.second);
    }
    else {
        return pair(nptr<ClientEntity>(item_hit.first), item_hit.second);
    }
}

auto MapView::FindPath(nptr<CritterHexView> find_cr, mpos start_hex, mpos& target_hex, int32_t cut, ipos16 target_hex_offset) -> optional<FindPathResult>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!find_cr || find_cr->GetMap() == this, "Critter belongs to a different map view");

    if (start_hex == target_hex) {
        return FindPathResult();
    }

    int32_t multihex {};
    ipos16 from_hex_offset {};

    if (find_cr) {
        multihex = find_cr->GetMultihex();
        from_hex_offset = find_cr->GetHexOffset();
    }

    FindPathInput input;
    input.FromHex = start_hex;
    input.FromHexOffset = from_hex_offset;
    input.ToHex = target_hex;
    input.ToHexOffset = target_hex_offset;
    input.MapSize = _mapSize;
    input.MaxLength = _engine->Settings->MaxPathFindLength;
    input.Cut = cut < 0 ? 0 : cut;
    input.Multihex = multihex;
    input.FreeMovement = _engine->Settings->MapFreeMovement;

    input.CheckHex = [&](mpos hex) -> HexBlockResult {
        const auto& cell = _hexField->GetCellForReading(hex);

        if (cell.MoveBlocked) {
            return HexBlockResult::Blocked;
        }

        for (auto cr : cell.Critters) {
            if (!cr->IsDead() && find_cr != cr) {
                return HexBlockResult::DeferCritter;
            }
        }

        return HexBlockResult::Passable;
    };

    auto output = PathFinding::FindPath(input);

    if (output.Result == FindPathOutput::ResultType::Ok) {
        target_hex = output.NewToHex;

        FindPathResult result;
        result.DirSteps = std::move(output.Steps);
        result.ControlSteps = std::move(output.ControlSteps);
        result.EndHexOffset = output.EndHexOffset;
        return result;
    }

    if (output.Result == FindPathOutput::ResultType::AlreadyHere) {
        target_hex = output.NewToHex;
        return FindPathResult();
    }

    return std::nullopt;
}

bool MapView::CutPath(nptr<CritterHexView> cr, mpos start_hex, mpos& target_hex, int32_t cut)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!cr || cr->GetMap() == this, "Critter belongs to a different map view");

    return !!FindPath(cr, start_hex, target_hex, cut);
}

bool MapView::TraceMoveWay(mpos& start_hex, ipos16& hex_offset, vector<mdir>& dir_steps, mdir dir, int32_t multihex) const
{
    FO_STACK_TRACE_ENTRY();

    hex_offset = {};

    constexpr int32_t some_big_path_len = 30;
    LineTracer tracer(start_hex, numeric_cast<float32_t>(dir.angle()), some_big_path_len, _mapSize);
    auto next_hex = start_hex;

    for (int32_t i = 0; i < some_big_path_len; i++) {
        const auto next_dir = tracer.GetNextHex(next_hex);

        if (!next_dir.has_value()) {
            break;
        }

        const auto result = PathFinding::CheckHexWithMultihex(next_hex, next_dir.value(), multihex, _mapSize, [this](mpos h) { //
            return _hexField->GetCellForReading(h).MoveBlocked ? HexBlockResult::Blocked : HexBlockResult::Passable;
        });

        if (result == HexBlockResult::Passable) {
            dir_steps.emplace_back(next_dir.value());
        }
        else {
            break;
        }
    }

    return !dir_steps.empty();
}

void MapView::TraceBullet(mpos start_hex, mpos target_hex, int32_t dist, float32_t angle, nptr<vector<ptr<CritterHexView>>> critters, CritterFindType find_type, nptr<mpos> pre_block_hex, nptr<mpos> block_hex, nptr<vector<mpos>> hex_steps, bool check_shoot_blocks)
{
    FO_STACK_TRACE_ENTRY();

    const auto check_dist = dist != 0 ? dist : GeometryHelper::GetDistance(start_hex, target_hex);
    auto next_hex = start_hex;
    auto prev_hex = next_hex;

    LineTracer tracer(start_hex, target_hex, angle, _mapSize);

    for (int32_t i = 0; i < check_dist; i++) {
        if (!tracer.GetNextHex(next_hex).has_value()) {
            break;
        }

        if (check_shoot_blocks && _hexField->GetCellForReading(next_hex).ShootBlocked) {
            break;
        }

        if (hex_steps) {
            hex_steps->emplace_back(next_hex);
        }

        if (critters) {
            auto hex_critters = GetCrittersOnHex(next_hex, find_type);
            for (ptr<CritterHexView> cr : hex_critters) {
                critters->emplace_back(cr);
            }
        }

        prev_hex = next_hex;
    }

    if (pre_block_hex) {
        *pre_block_hex = prev_hex;
    }
    if (block_hex) {
        *block_hex = next_hex;
    }
}

void MapView::InstantScrollTo(mpos center_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_viewField.empty(), "Client map view-field is empty while scrolling to a center hex", GetId(), center_hex, _mapSize, _wVisible, _hVisible);

    const ipos32 new_screen_hex = ConvertToScreenRawHex(ipos32(center_hex));
    const ipos32 offset_to_new_pos = GeometryHelper::GetHexOffset(_screenRawHex, new_screen_hex);
    InstantScroll(fpos32(offset_to_new_pos));
}

void MapView::RebuildMap()
{
    FO_STACK_TRACE_ENTRY();

    _rebuildMap = true;
}

void MapView::RebuildFog()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& fog : _fogs | std::views::join) {
        fog->Shape.RequestRebuild();
    }
}

auto MapView::HasFogLayers() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::ranges::any_of(_fogs, [](const auto& fog_slot) { return !fog_slot.empty(); });
}

auto MapView::AddFog(nptr<CritterView> cr, DrawOrderType draw_order, nptr<RenderEffect> custom_flush_effect) -> ptr<FogLayer>
{
    FO_STACK_TRACE_ENTRY();

    auto hex_cr = cr.dyn_cast<CritterHexView>();
    if (!hex_cr || hex_cr->GetMap() != this) {
        throw ScriptException("Fog critter is not a hex critter on this map");
    }

    auto fog = SafeAlloc::MakeRefCounted<FogLayer>();
    fog->DrawOrder = draw_order;
    fog->FollowCritter = true;
    fog->OriginCritterId = hex_cr->GetId();
    fog->CustomFlushEffect = custom_flush_effect;

    _fogs[static_cast<size_t>(draw_order)].emplace_back(fog);
    return fog;
}

auto MapView::AddFog(mpos hex, DrawOrderType draw_order, nptr<RenderEffect> custom_flush_effect) -> ptr<FogLayer>
{
    FO_STACK_TRACE_ENTRY();

    auto fog = SafeAlloc::MakeRefCounted<FogLayer>();
    fog->DrawOrder = draw_order;
    fog->FollowCritter = false;
    fog->OriginHex = hex;
    fog->CustomFlushEffect = custom_flush_effect;

    _fogs[static_cast<size_t>(draw_order)].emplace_back(fog);
    return fog;
}

auto MapView::AddMapSprite(ptr<const Sprite> spr, mpos hex, DrawOrderType draw_order, int32_t draw_order_hy_offset, ipos32 offset, nptr<const ipos32> poffset, nptr<const uint8_t> palpha, nptr<bool> callback) -> ptr<MapSprite>
{
    FO_STACK_TRACE_ENTRY();

    auto field = _hexField->GetCellForWriting(hex);
    ptr<MapSprite> mspr = _mapSprites.AddSprite(draw_order, _mapSize.clamp_pos(hex.x, hex.y + draw_order_hy_offset), //
        {(GameSettings::MAP_HEX_WIDTH / 2) + offset.x, (GameSettings::MAP_HEX_HEIGHT / 2) + offset.y}, &field->Offset, spr, nullptr, //
        poffset, nullptr, palpha, nullptr, callback);
    AddSpriteToChain(field, mspr);
    return mspr;
}

void MapView::OnScreenSizeChanged()
{
    FO_STACK_TRACE_ENTRY();

    if (_engine->Settings->MapDirectDraw) {
        const auto window_size = GetApp()->MainWindow.GetSize();

        SetScreenSize(window_size);

        if (_rtLight) {
            _engine->SprMngr.GetRtMngr().ResizeRenderTarget(_rtLight, window_size);
        }
    }

    RebuildMapNow();
}

void MapView::SetScreenSize(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    if (_screenSize == size) {
        return;
    }

    _screenSize = size;
    _viewSize = fsize32(_screenSize) / GetSpritesZoom();

    auto& rt_mngr = _engine->SprMngr.GetRtMngr();
    const auto map_rt_size = isize32(_screenSize.width + GameSettings::MAP_HEX_WIDTH, _screenSize.height + GameSettings::MAP_HEX_LINE_HEIGHT * 2);

    if (_rtMap) {
        rt_mngr.ResizeRenderTarget(_rtMap, map_rt_size);
    }
    if (_rtLight && !_engine->Settings->MapDirectDraw) {
        rt_mngr.ResizeRenderTarget(_rtLight, map_rt_size);
    }

    if (!_viewField.empty()) {
        auto chosen = _engine->GetMapChosen();

        if (chosen && chosen->GetMap() == this) {
            InstantScrollTo(chosen->GetHex());
        }
    }

    RebuildMapNow();
}

void MapView::AddFastPid(hstring pid)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper mode is not selected");

    _fastPids.emplace(pid);
}

auto MapView::IsFastPid(hstring pid) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper mode is not selected");

    return _fastPids.count(pid) != 0;
}

void MapView::ClearFastPids()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper mode is not selected");

    _fastPids.clear();
}

void MapView::AddIgnorePid(hstring pid)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper mode is not selected");
    _ignorePids.emplace(pid);
}

void MapView::SwitchIgnorePid(hstring pid)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper mode is not selected");

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

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper mode is not selected");

    return _ignorePids.count(pid) != 0;
}

void MapView::ClearIgnorePids()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper mode is not selected");

    _ignorePids.clear();
}

auto MapView::GenTempEntityId() -> ident_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapperMode, "Mapper mode is not selected");

    auto next_id = ident_t {(_workEntityId.underlying_value() + 1)};

    while (_itemsMap.count(next_id) != 0 || _crittersMap.count(next_id) != 0) {
        next_id = ident_t {next_id.underlying_value() + 1};
    }

    _workEntityId = next_id;
    return next_id;
}

void MapView::SetHeaderExtraFields(map<string, string> fields)
{
    FO_STACK_TRACE_ENTRY();

    _headerExtraFields = std::move(fields);
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
                FO_VERIFY_AND_THROW(hex_str.size() % 2 == 0, "Mapper item MultihexMesh field must contain x/y coordinate pairs", name, key, value, hex_str.size());
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

    const auto fill_critter = [&fill_generic_section](ptr<const CritterView> cr) {
        auto kv = cr->GetProperties()->SaveToText(cr->GetProto()->GetProperties());
        kv["$Id"] = strex("{}", cr->GetId());
        kv["$Proto"] = cr->GetProto()->GetName();
        fill_generic_section("Critter", kv);
    };

    const auto fill_item = [&fill_item_section](ptr<const ItemView> item) {
        auto kv = item->GetProperties()->SaveToText(item->GetProto()->GetProperties());
        kv["$Id"] = strex("{}", item->GetId());
        kv["$Proto"] = item->GetProto()->GetName();
        fill_item_section("Item", kv);
    };

    // Header
    auto proto_map_kv = GetProperties()->SaveToText(nullptr);

    for (const auto& [key, value] : _headerExtraFields) {
        proto_map_kv.emplace(key, value);
    }

    fill_generic_section("ProtoMap", proto_map_kv);

    // Critters
    for (size_t i = 0; i < _critters.size(); i++) {
        auto cr = _critters[i].as_ptr();
        fill_critter(cr);

        const_span<refcount_ptr<ItemView>> inv_items = cr->GetInvItems();

        for (size_t j = 0; j < inv_items.size(); j++) {
            fill_item(inv_items[j]);
        }
    }

    // Items
    for (size_t i = 0; i < _items.size(); i++) {
        auto item = _items[i].as_ptr();
        fill_item(item);

        const_span<refcount_ptr<ItemView>> inner_items = item->GetInnerItems();

        for (size_t j = 0; j < inner_items.size(); j++) {
            fill_item(inner_items[j]);
        }
    }

    return fomap;
}

void MapView::SetDayColors(ucolor map_color, int32_t map_light_capacity, ucolor global_color, int32_t global_light_capacity)
{
    FO_STACK_TRACE_ENTRY();

    const int32_t clamped_map_light_capacity = std::clamp(map_light_capacity, 0, LIGHT_CAPACITY_MAX);
    const int32_t clamped_global_light_capacity = std::clamp(global_light_capacity, 0, LIGHT_CAPACITY_MAX);

    if (GetMapDayColor() != map_color) {
        SetMapDayColor(map_color);
        _needReapplyLights = true;
    }

    if (GetGlobalDayColor() != global_color) {
        SetGlobalDayColor(global_color);
        _needReapplyLights = _needReapplyLights || _globalLights != 0;
    }

    if (GetMapDayLightCapacity() != clamped_map_light_capacity) {
        SetMapDayLightCapacity(clamped_map_light_capacity);
        _needReapplyLights = true;
    }

    if (GetGlobalDayLightCapacity() != clamped_global_light_capacity) {
        SetGlobalDayLightCapacity(clamped_global_light_capacity);
        _needReapplyLights = _needReapplyLights || _globalLights != 0;
    }
}

FO_END_NAMESPACE
