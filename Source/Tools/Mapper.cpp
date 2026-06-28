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

#include "Mapper.h"
#include "3dStuff.h"
#include "AngelScriptScripting.h"
#include "AnyData.h"
#include "ConfigFile.h"
#include "DefaultSprites.h"
#include "ImGuiStuff.h"
#include "MetadataRegistration.h"
#include "ParticleSprites.h"

FO_BEGIN_NAMESPACE

static constexpr ipos32 MAPPER_CONSOLE_WINDOW_OFFSET = {0, 6};
static constexpr string_view MAPPER_IMGUI_SETTINGS_CACHE_ENTRY = "MapperImGui.ini";
static constexpr int32_t DAY_TIME_WRAP_MINUTES = 1440;
static constexpr int32_t DAY_TIME_VISIBLE_UPPER_BOUND = DAY_TIME_WRAP_MINUTES * 2;

static auto MakeRectFromEdges(int32_t left, int32_t top, int32_t right, int32_t bottom) -> irect32
{
    FO_STACK_TRACE_ENTRY();

    return {left, top, right - left, bottom - top};
}

static auto ShiftDayTimeWithWrap(int32_t day_time, int32_t delta_minutes) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    day_time += delta_minutes;

    while (day_time > DAY_TIME_VISIBLE_UPPER_BOUND) {
        day_time -= DAY_TIME_WRAP_MINUTES;
    }

    while (day_time < 0) {
        day_time += DAY_TIME_WRAP_MINUTES;
    }

    return day_time;
}

static auto ScaleZoomValue(float32_t current_zoom, float32_t factor) -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    return std::clamp(current_zoom * factor, GameSettings::MIN_ZOOM, GameSettings::MAX_ZOOM);
}

static auto GetTileLayerFromKey(KeyCode key) -> optional<int32_t>
{
    FO_STACK_TRACE_ENTRY();

    switch (key) {
    case KeyCode::C0:
    case KeyCode::Numpad0:
        return 0;
    case KeyCode::C1:
    case KeyCode::Numpad1:
        return 1;
    case KeyCode::C2:
    case KeyCode::Numpad2:
        return 2;
    case KeyCode::C3:
    case KeyCode::Numpad3:
        return 3;
    case KeyCode::C4:
    case KeyCode::Numpad4:
        return 4;
    default:
        return std::nullopt;
    }
}

static auto GetNextCritterDir(mdir dir) -> mdir
{
    FO_STACK_TRACE_ENTRY();

    return dir.incHex();
}

template<size_t Size>
static auto InputBufferView(const array<char, Size>& buffer) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    const auto end = std::find(buffer.begin(), buffer.end(), '\0');
    return {buffer.data(), numeric_cast<size_t>(std::distance(buffer.begin(), end))};
}

static void AdvanceCritterDir(ptr<CritterHexView> cr)
{
    FO_STACK_TRACE_ENTRY();

    cr->ChangeDir(GetNextCritterDir(cr->GetDir()));
}

static void ToggleMapVisibilityFlag(nptr<MapView> map, bool& value)
{
    FO_STACK_TRACE_ENTRY();

    value = !value;

    if (map) {
        map->RebuildMap();
    }
}

static auto ContainsCaseInsensitive(string_view text, string_view filter) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (filter.empty()) {
        return true;
    }

    const string lower_text = strex(text).lower_utf8();
    const string lower_filter = strex(filter).lower_utf8();

    return lower_text.find(lower_filter) != string::npos;
}

static auto ResolveAtlasSprite(nptr<const Sprite> sprite) -> nptr<const AtlasSprite>
{
    FO_STACK_TRACE_ENTRY();

    nptr<const Sprite> source_sprite = sprite;

    if (nptr<const SpriteSheet> nullable_sprite_sheet = source_sprite.dyn_cast<const SpriteSheet>()) {
        auto sprite_sheet = nullable_sprite_sheet.as_ptr();
        source_sprite = sprite_sheet->GetCurSpr();
    }

    return source_sprite.dyn_cast<const AtlasSprite>();
}

static auto GetInspectorValueType(ptr<const Property> prop) -> AnyData::ValueType
{
    FO_STACK_TRACE_ENTRY();

    if (prop->IsString() || prop->IsArrayOfString() || prop->IsDictOfString() || prop->IsDictOfArrayOfString() || prop->IsBaseTypeHash() || prop->IsBaseTypeEnum() || prop->IsBaseTypeComplexStruct()) {
        return AnyData::ValueType::String;
    }
    if (prop->IsBaseTypeInt()) {
        return AnyData::ValueType::Int64;
    }
    if (prop->IsBaseTypeBool()) {
        return AnyData::ValueType::Bool;
    }
    if (prop->IsBaseTypeFloat()) {
        return AnyData::ValueType::Float64;
    }

    return AnyData::ValueType::String;
}

static auto ParseInspectorValue(ptr<const Property> prop, string_view text) -> optional<AnyData::Value>
{
    FO_STACK_TRACE_ENTRY();

    try {
        return AnyData::ParseValue(string(text), false, prop->IsArray(), GetInspectorValueType(prop));
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

static auto MakeDefaultInspectorArrayElement(ptr<const Property> prop) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    switch (GetInspectorValueType(prop)) {
    case AnyData::ValueType::Int64:
        return AnyData::Value {int64_t {0}};
    case AnyData::ValueType::Float64:
        return AnyData::Value {float64_t {0.0}};
    case AnyData::ValueType::Bool:
        return AnyData::Value {false};
    case AnyData::ValueType::String:
        return AnyData::Value {string {}};
    default:
        FO_UNREACHABLE_PLACE();
    }
}

static auto SerializeInspectorArray(vector<AnyData::Value> entries) -> string
{
    FO_STACK_TRACE_ENTRY();

    AnyData::Array value_arr;
    value_arr.Reserve(entries.size());

    for (auto& entry : entries) {
        value_arr.EmplaceBack(std::move(entry));
    }

    return AnyData::ValueToString(AnyData::Value {std::move(value_arr)});
}

static auto SerializeInspectorStringArray(const vector<string>& entries) -> string
{
    FO_STACK_TRACE_ENTRY();

    vector<AnyData::Value> values;
    values.reserve(entries.size());

    for (const auto& entry : entries) {
        values.emplace_back(string {entry});
    }

    return SerializeInspectorArray(std::move(values));
}

static auto GetInspectorStructLayout(ptr<const Property> prop) -> nptr<const StructLayoutDesc>
{
    FO_STACK_TRACE_ENTRY();

    const auto& base_type = prop->GetBaseType();
    if (base_type.StructLayout && (base_type.IsComplexStruct || base_type.IsSimpleStruct) && base_type.StructLayout->Fields.size() > 1) {
        return base_type.StructLayout;
    }

    return nullptr;
}

static auto ReadInspectorToken(nptr<const char> str, string& result) -> nptr<const char>
{
    FO_STACK_TRACE_ENTRY();

    if (str[0] == 0) {
        return nullptr;
    }

    const auto decode_char = [str](size_t char_pos, size_t& char_len) {
        ptr<const char> chars = &str[char_pos];
        char_len = utf8::DecodeStrNtLen(chars.get());
        utf8::Decode(chars.get(), char_len);
    };

    size_t pos = 0;
    size_t length = 0;
    decode_char(pos, length);

    while (length == 1 && (str[pos] == ' ' || str[pos] == '\t')) {
        pos++;

        decode_char(pos, length);
    }

    if (str[pos] == 0) {
        return nullptr;
    }

    size_t begin;

    if (length == 1 && str[pos] == '"') {
        pos++;
        begin = pos;

        while (str[pos] != 0) {
            if (length == 1 && str[pos] == '\\') {
                pos++;

                if (str[pos] != 0) {
                    decode_char(pos, length);
                    pos += length;
                }
            }
            else if (length == 1 && str[pos] == '"') {
                break;
            }
            else {
                pos += length;
            }

            decode_char(pos, length);
        }
    }
    else {
        begin = pos;

        while (str[pos] != 0) {
            if (length == 1 && str[pos] == '\\') {
                pos++;

                decode_char(pos, length);
                pos += length;
            }
            else if (length == 1 && (str[pos] == ' ' || str[pos] == '\t')) {
                break;
            }
            else {
                pos += length;
            }

            decode_char(pos, length);
        }
    }

    ptr<const char> token_begin = &str[begin];
    ptr<const char> next_token = &str[pos + (str[pos] != 0 ? 1 : 0)];
    result.assign(token_begin.get(), pos - begin);
    return next_token;
}

static auto ParseInspectorStructFields(const StructLayoutDesc& layout, string_view text) -> optional<vector<string>>
{
    FO_STACK_TRACE_ENTRY();

    try {
        const auto text_str = string {text};
        nptr<const char> token_pos = text_str.c_str();
        string token;
        vector<string> fields;
        fields.reserve(layout.Fields.size());

        while ((token_pos = ReadInspectorToken(token_pos, token))) {
            fields.emplace_back(StringEscaping::DecodeString(token));
        }

        if (fields.size() != layout.Fields.size()) {
            return std::nullopt;
        }

        return fields;
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

static auto ParseInspectorStringEntries(string_view text) -> optional<vector<string>>
{
    FO_STACK_TRACE_ENTRY();

    try {
        const auto parsed = AnyData::ParseValue(string(text), false, true, AnyData::ValueType::String);
        if (parsed.Type() != AnyData::ValueType::Array) {
            return std::nullopt;
        }

        vector<string> entries;
        entries.reserve(parsed.AsArray().Size());

        for (const auto& entry : parsed.AsArray()) {
            entries.emplace_back(entry.AsString());
        }

        return entries;
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

struct ImGuiInputTextStringUserData
{
    ptr<string> Value;
    bool LatinOnly {};
    bool MoveCaretToEnd {};
};

static auto GetImGuiInputTextStringUserData(nptr<void> user_data) -> ptr<ImGuiInputTextStringUserData>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<ImGuiInputTextStringUserData> nullable_user_data = cast_from_void<ImGuiInputTextStringUserData*>(user_data.get());
    IM_ASSERT(nullable_user_data);
    return nullable_user_data.as_ptr();
}

static int ImGuiInputTextStringCallback(ImGuiInputTextCallbackData* data)
{
    FO_STACK_TRACE_ENTRY();

    IM_ASSERT(data);
    ptr<ImGuiInputTextCallbackData> callback_data = data;
    auto user_data = GetImGuiInputTextStringUserData(callback_data->UserData);

    if (callback_data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        ptr<string> str = user_data->Value;
        IM_ASSERT(callback_data->Buf == str->c_str());
        str->resize(callback_data->BufTextLen);
        callback_data->Buf = str->data();
    }
    else if (callback_data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        if (user_data->LatinOnly && callback_data->EventChar >= 128) {
            return 1;
        }
    }
    else if (callback_data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
        if (user_data->MoveCaretToEnd) {
            callback_data->CursorPos = callback_data->BufTextLen;
            callback_data->SelectionStart = callback_data->BufTextLen;
            callback_data->SelectionEnd = callback_data->BufTextLen;
            user_data->MoveCaretToEnd = false;
        }
    }

    return 0;
}

static auto ImGuiInputTextString(ptr<const char> label, string& value, ImGuiInputTextFlags flags = 0, bool latin_only = false, bool move_caret_to_end = false) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (value.capacity() == 0) {
        value.reserve(256);
    }

    ptr<string> value_ptr = &value;
    ImGuiInputTextStringUserData user_data {.Value = value_ptr, .LatinOnly = latin_only, .MoveCaretToEnd = move_caret_to_end};
    flags |= ImGuiInputTextFlags_CallbackResize;
    if (latin_only) {
        flags |= ImGuiInputTextFlags_CallbackCharFilter;
    }
    if (move_caret_to_end) {
        flags |= ImGuiInputTextFlags_CallbackAlways;
    }

    return ImGui::InputText(label.get(), value.data(), value.capacity() + 1, flags, ImGuiInputTextStringCallback, &user_data);
}

static auto IsInspectorValueSameAsProto(ptr<const Entity> entity, ptr<const Property> prop, string_view value_text) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto entity_with_proto = entity.dyn_cast<const EntityWithProto>();
    if (!entity_with_proto) {
        return true;
    }

    try {
        return entity_with_proto->GetProto()->GetProperties().SavePropertyToText(prop) == value_text;
    }
    catch (const std::exception&) {
        return true;
    }
}

static void UpdateLocalConfigValue(CacheStorage& cache, string_view key, string_view value)
{
    FO_STACK_TRACE_ENTRY();

    string cfg_user;

    if (cache.HasEntry(LOCAL_CONFIG_NAME)) {
        auto config = ConfigFile(LOCAL_CONFIG_NAME, cache.GetString(LOCAL_CONFIG_NAME));
        const auto& sections = config.GetSections();
        auto wrote_root_key = false;
        auto has_root_section = false;

        for (const auto& [section_name, key_values] : *sections) {
            if (!section_name.empty()) {
                cfg_user += strex("[{}]\n", section_name);
            }
            else {
                has_root_section = true;
            }

            for (const auto& [entry_key, entry_value] : key_values) {
                if (section_name.empty() && entry_key == key) {
                    cfg_user += strex("{} = {}\n", key, value);
                    wrote_root_key = true;
                }
                else {
                    cfg_user += strex("{} = {}\n", entry_key, entry_value);
                }
            }

            if (section_name.empty() && !wrote_root_key) {
                cfg_user += strex("{} = {}\n", key, value);
                wrote_root_key = true;
            }

            if (!section_name.empty()) {
                cfg_user += "\n";
            }
        }

        if (!has_root_section) {
            cfg_user = strex("{} = {}\n", key, value).str() + cfg_user;
        }
    }
    else {
        cfg_user = strex("{} = {}\n", key, value);
    }

    cache.SetString(LOCAL_CONFIG_NAME, cfg_user);
}

MapperEngine::MapperEngine(ptr<GlobalSettings> settings, FileSystem&& resources, ptr<IAppWindow> window) :
    ClientEngine(settings, std::move(resources), window, [&] {
        ptr<EngineMetadata> meta = this;
        ptr<const FileSystem> resources_ptr = &resources;
        RegisterMapperMetadata(meta, resources_ptr);
    })
{
    FO_STACK_TRACE_ENTRY();

    GetApp()->LoadImGuiEffect(Resources);

    MapsFileSys.AddDirSource("", false, true, true);

    for (const auto& res_pack : Settings->GetResourcePacks()) {
        for (const auto& dir : res_pack.InputDirs) {
            MapsFileSys.AddDirSource(dir, res_pack.RecursiveInput, true, true);
        }
    }

    EffectMngr.LoadDefaultEffects();

    ptr<SpriteManager> spr_mngr = &SprMngr;
    ptr<EffectManager> effect_mngr = &EffectMngr;
    ptr<GameTimer> game_time = &GameTime;
    ptr<HashResolver> hash_resolver = &Hashes;

    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<DefaultSpriteFactory>(spr_mngr));
    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<ParticleSpriteFactory>(spr_mngr, Settings, effect_mngr, game_time, hash_resolver));
#if FO_ENABLE_3D
    ptr<NameResolver> name_resolver = this;
    ptr<AnimationResolver> anim_name_resolver = this;
    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<ModelSpriteFactory>(spr_mngr, Settings, effect_mngr, game_time, hash_resolver, name_resolver, anim_name_resolver));
#endif

    ResMngr.IndexFiles();

    ptr<EngineMetadata> meta = this;
    MapScriptTypes(meta);
    MapEngineType<PlayerView>(EngineMetadata::GetBaseType(PlayerView::ENTITY_TYPE_NAME));
    MapEngineType<ItemView>(EngineMetadata::GetBaseType(ItemView::ENTITY_TYPE_NAME));
    MapEngineType<ItemHexView>(EngineMetadata::GetBaseType(ItemView::ENTITY_TYPE_NAME));
    MapEngineType<CritterView>(EngineMetadata::GetBaseType(CritterView::ENTITY_TYPE_NAME));
    MapEngineType<CritterHexView>(EngineMetadata::GetBaseType(CritterView::ENTITY_TYPE_NAME));
    MapEngineType<MapView>(EngineMetadata::GetBaseType(MapView::ENTITY_TYPE_NAME));
    MapEngineType<LocationView>(EngineMetadata::GetBaseType(LocationView::ENTITY_TYPE_NAME));

#if FO_ANGELSCRIPT_SCRIPTING
    InitAngelScriptScripting(meta, *Settings, Resources);
#endif

    _curLang = TextPack {hash_resolver};
    _curLang.LoadFromResources(Resources, Settings->Language);

    // Fonts
    FontMngr.BindFoFont(FONT_FO, "Fonts/OldDefault.fofnt", AtlasType::IfaceSprites, false, true);
    FontMngr.BindFoFont(FONT_NUM, "Fonts/Numbers.fofnt", AtlasType::IfaceSprites, true, true);
    FontMngr.BindFoFont(FONT_BIG_NUM, "Fonts/BigNumbers.fofnt", AtlasType::IfaceSprites, true, true);
    FontMngr.BindFoFont(FONT_SAND_NUM, "Fonts/SandNumbers.fofnt", AtlasType::IfaceSprites, false, true);
    FontMngr.BindFoFont(FONT_SPECIAL, "Fonts/Special.fofnt", AtlasType::IfaceSprites, false, true);
    FontMngr.BindFoFont(FONT_OLD_DEFAULT, "Fonts/Default.fofnt", AtlasType::IfaceSprites, false, true);
    FontMngr.BindFoFont(FONT_THIN, "Fonts/Thin.fofnt", AtlasType::IfaceSprites, false, true);
    FontMngr.BindFoFont(FONT_FAT, "Fonts/Fat.fofnt", AtlasType::IfaceSprites, false, true);
    FontMngr.BindFoFont(FONT_BIG, "Fonts/Big.fofnt", AtlasType::IfaceSprites, false, true);

    SprMngr.BeginScene();
    SprMngr.EndScene();

    InitIface();

    // Initialize tabs
    const auto& cr_protos = GetProtoCritters();

    for (const auto& proto : cr_protos | std::views::values) {
        Tabs[INT_MODE_CRIT][DEFAULT_SUB_TAB].CritterProtos.emplace_back(proto);
        Tabs[INT_MODE_CRIT][proto->CollectionName].CritterProtos.emplace_back(proto);
    }
    for (auto& proto : Tabs[INT_MODE_CRIT] | std::views::values) {
        std::ranges::stable_sort(proto.CritterProtos, [](auto&& a, auto&& b) -> bool { return a->GetName() < b->GetName(); });
    }

    const auto& item_protos = GetProtoItems();

    for (const auto& proto : item_protos | std::views::values) {
        Tabs[INT_MODE_ITEM][DEFAULT_SUB_TAB].ItemProtos.emplace_back(proto);
        Tabs[INT_MODE_ITEM][proto->CollectionName].ItemProtos.emplace_back(proto);
    }
    for (auto& proto : Tabs[INT_MODE_ITEM] | std::views::values) {
        std::ranges::stable_sort(proto.ItemProtos, [](auto&& a, auto&& b) -> bool { return a->GetName() < b->GetName(); });
    }

    for (auto i = 0; i < TAB_COUNT; i++) {
        if (Tabs[i].empty()) {
            Tabs[i][DEFAULT_SUB_TAB].Scroll = 0;
        }

        ActiveSubTabs[i] = &Tabs[i].begin()->second;
    }

    // Initialize tabs scroll and names
    for (auto i = INT_MODE_CUSTOM0; i <= INT_MODE_CUSTOM9; i++) {
        PanelModeNames[i] = "-";
    }

    PanelModeNames[INT_MODE_ITEM] = "Item";
    PanelModeNames[INT_MODE_TILE] = "Tile";
    PanelModeNames[INT_MODE_CRIT] = "Crit";
    PanelModeNames[INT_MODE_FAST] = "Fast";
    PanelModeNames[INT_MODE_IGNORE] = "Ign";
    PanelModeNames[INT_MODE_INCONT] = "Inv";
    PanelModeNames[INT_MODE_MESS] = "Msg";
    PanelModeNames[INT_MODE_LIST] = "Maps";

    // Start script
    InitModules();
    OnStart.Fire();

    if (!Settings->StartMap.empty()) {
        auto nullable_map = LoadMap(Settings->StartMap);

        if (nullable_map) {
            auto map = nullable_map.as_ptr();

            if (Settings->StartHexX > 0 && Settings->StartHexY > 0) {
                map->InstantScrollTo(map->GetSize().from_raw_pos(Settings->StartHexX, Settings->StartHexY));
            }

            ShowMap(map);
        }
    }

    // Refresh resources after start script executed
    RefreshActiveProtoLists();

    // The cached ImGui window layout only matters for the interactive editor UI. Skip it under the null
    // renderer (headless mapper, e.g. unit tests and batch map rendering): nothing draws those windows, and
    // feeding a stale/foreign cached ini into the headless ImGui context is a needless crash surface.
    if (!Settings->NullRenderer) {
        const auto imgui_ini = Cache.GetString(MAPPER_IMGUI_SETTINGS_CACHE_ENTRY);

        if (!imgui_ini.empty()) {
            ImGui::LoadIniSettingsFromMemory(imgui_ini.c_str(), imgui_ini.size());
            ImGui::GetIO().WantSaveIniSettings = false;
        }
    }

    // Load console history
    const auto history_str = Cache.GetString("mapper_console.txt");
    ConsoleHistory = strex(history_str).normalize_line_endings().split('\n');

    while (numeric_cast<int32_t>(ConsoleHistory.size()) > Settings->ConsoleHistorySize) {
        ConsoleHistory.erase(ConsoleHistory.begin());
    }

    ConsoleHistoryCur = numeric_cast<int32_t>(ConsoleHistory.size());
}

void MapperEngine::Shutdown()
{
    FO_STACK_TRACE_ENTRY();

    while (!LoadedMaps.empty()) {
        UnloadMap(LoadedMaps.back().get(), false);
    }

    ClientEngine::Shutdown();
}

void MapperEngine::InitIface()
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Init interface");

    // Interface
    MainPanelPos = {-1, -1};
    MainPanelWindowRect = MakeRectFromEdges(0, 0, 0, 28);
    if (MainPanelPos.x == -1) {
        MainPanelPos.x = 0;
    }
    if (MainPanelPos.y == -1) {
        MainPanelPos.y = 0;
    }

    MainPanelContentRect = MakeRectFromEdges(12, 36, 520, 120);

    WorkspaceWindowVisible = false;
    ContentWindowVisible = false;
    MapListWindowVisible = true;
    MapWindowVisible = true;
    HistoryWindowVisible = false;
    SettingsWindowVisible = false;
    ActivePanelMode = INT_MODE_ITEM;
    ProtoWidth = 50;
    ProtosOnScreen = MainPanelContentRect.width / ProtoWidth;
    MemFill(TabIndex, 0, sizeof(TabIndex));
    CritterDir = hdir::SouthWest;
    CurMode = CUR_MODE_DEFAULT;
    SelectItemsEnabled = true;
    SelectSceneryEnabled = true;
    SelectWallsEnabled = true;
    SelectCrittersEnabled = true;
    InspectorVisible = false;

    // Cursor
    CurPDef = SprMngr.LoadSprite("CurDefault.png", AtlasType::IfaceSprites);
    CurPHand = SprMngr.LoadSprite("CurHand.png", AtlasType::IfaceSprites);

    WriteLog("Init interface complete");
}

void MapperEngine::ResetImGuiSettings()
{
    FO_STACK_TRACE_ENTRY();

    Cache.SetString(MAPPER_IMGUI_SETTINGS_CACHE_ENTRY, "");
    ImGui::LoadIniSettingsFromMemory("", 0);
    ImGui::GetIO().WantSaveIniSettings = false;

    WorkspaceWindowVisible = false;
    ContentWindowVisible = false;
    CritterAnimationsWindowVisible = false;
    ScriptCallWindowVisible = false;
    MapListWindowVisible = true;
    MapWindowVisible = true;
    HistoryWindowVisible = false;
    SettingsWindowVisible = false;
    InspectorVisible = false;
    InspectorPos = {24, 24};

    AddMess("ImGui layout reset");
}

auto MapperEngine::GetPreviewSprite(hstring fname) -> nptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = PreviewSprites.find(fname); it != PreviewSprites.end()) {
        return it->second.as_nptr();
    }

    shared_ptr<Sprite> spr = SprMngr.LoadSprite(fname, AtlasType::IfaceSprites);

    if (spr) {
        spr->PlayDefault();
    }

    return PreviewSprites.emplace(fname, std::move(spr)).first->second.as_nptr();
}

void MapperEngine::SetInputLocked(bool locked) noexcept
{
    FO_STACK_TRACE_ENTRY();

    InputLocked = locked;
}

void MapperEngine::MapperMainLoop()
{
    FO_STACK_TRACE_ENTRY();

    FrameAdvance();

    OnLoop.Fire();

    BeginMapperFrameInput();

    if (_curMap) {
        auto cur_map = GetCurMapPtr();
        cur_map->Process();
        ProcessRightMouseInertia();
    }

    DrawMapperFrame();

    if (ResetImGuiSettingsRequested) {
        ResetImGuiSettingsRequested = false;
        ResetImGuiSettings();
    }

    auto& io = ImGui::GetIO();
    if (io.WantSaveIniSettings) {
        size_t ini_size = 0;
        if (nptr<const char> ini_data = ImGui::SaveIniSettingsToMemory(&ini_size); ini_data) {
            Cache.SetString(MAPPER_IMGUI_SETTINGS_CACHE_ENTRY, string_view(ini_data.get(), ini_size));
        }
        io.WantSaveIniSettings = false;
    }
}

auto MapperEngine::BeginMapperFrameInput() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (InputLocked) {
        Settings->ScrollMouseRight = false;
        Settings->ScrollMouseLeft = false;
        Settings->ScrollMouseDown = false;
        Settings->ScrollMouseUp = false;
        Settings->ScrollKeybRight = false;
        Settings->ScrollKeybLeft = false;
        Settings->ScrollKeybDown = false;
        Settings->ScrollKeybUp = false;
        MouseHoldMode = INT_NONE;
        RightMouseDragged = false;
        RightMouseInertia = {};
        RightMouseVelocityAccum = {};
        RightMouseVelocityTime = {};
    }

    if (const bool is_fullscreen = SprMngr.IsFullscreen(); (is_fullscreen && Settings->FullscreenMouseScroll) || (!is_fullscreen && Settings->WindowedMouseScroll)) {
        if (!InputLocked) {
            Settings->ScrollMouseRight = MousePos.x >= Settings->ScreenWidth - 1;
            Settings->ScrollMouseLeft = MousePos.x <= 0;
            Settings->ScrollMouseDown = MousePos.y >= Settings->ScreenHeight - 1;
            Settings->ScrollMouseUp = MousePos.y <= 0;
        }
    }

    if (!SprMngr.IsWindowFocused()) {
        OnInputLost.Fire();
        if (!PendingSelectionMoveEntries.empty()) {
            CommitPendingSelectionMove();
        }
        MouseHoldMode = INT_NONE;
        RightMouseDragged = false;
        RightMouseInertia = {};
    }

    InputEvent ev;
    while (GetApp()->Input.PollEvent(ev)) {
        ProcessInputEvent(ev);

        if (!InputLocked) {
            ProcessMapperInputEvent(ev);
        }
    }

    return true;
}

void MapperEngine::ProcessMapperInputEvent(const InputEvent& ev)
{
    FO_STACK_TRACE_ENTRY();

    const auto ev_type = ev.Type;

    if (ev_type == InputEvent::EventType::KeyDownEvent || ev_type == InputEvent::EventType::KeyUpEvent) {
        HandleMapperKeyboardEvent(ev);
    }
    else if (ev_type == InputEvent::EventType::MouseWheelEvent) {
        if (!IsImGuiMouseCaptured() && ev.MouseWheel.Delta != 0 && _curMap) {
            auto cur_map = GetCurMapPtr();
            const float32_t cur_zoom = cur_map->GetSpritesZoomTarget();
            ChangeZoom(ScaleZoomValue(cur_zoom, ev.MouseWheel.Delta > 0 ? 1.2f : 0.8f));
        }
    }
    else if (ev_type == InputEvent::EventType::MouseDownEvent) {
        if (IsImGuiMouseCaptured()) {
            return;
        }

        if (ev.MouseDown.Button == MouseButton::Middle) {
            CurMMouseDown();
            ChangeZoom(1.0f);
        }
        if (ev.MouseDown.Button == MouseButton::Left) {
            HandleLeftMouseDown();
        }
        if (ev.MouseDown.Button == MouseButton::Right) {
            MouseHoldMode = INT_PAN;
            RightMouseDragged = false;
            RightMouseInertia = {};
            RightMouseVelocityAccum = {};
            RightMouseVelocityTime = nanotime::now();
        }
    }
    else if (ev_type == InputEvent::EventType::MouseUpEvent) {
        if (IsImGuiMouseCaptured()) {
            return;
        }

        if (ev.MouseUp.Button == MouseButton::Left) {
            HandleLeftMouseUp();
        }
        if (ev.MouseUp.Button == MouseButton::Right) {
            CurRMouseUp();
        }
    }
    else if (ev_type == InputEvent::EventType::MouseMoveEvent) {
        if (_curMap && MouseHoldMode == INT_PAN) {
            auto cur_map = GetCurMapPtr();
            const float32_t zoom = cur_map->GetSpritesZoom();
            const fpos32 pan_delta {-numeric_cast<float32_t>(ev.MouseMove.DeltaX) / zoom, -numeric_cast<float32_t>(ev.MouseMove.DeltaY) / zoom};
            const fpos32 screen_pan_delta {-numeric_cast<float32_t>(ev.MouseMove.DeltaX), -numeric_cast<float32_t>(ev.MouseMove.DeltaY)};

            if (ev.MouseMove.DeltaX != 0 || ev.MouseMove.DeltaY != 0) {
                RightMouseDragged = true;
                cur_map->InstantScroll(pan_delta);

                RightMouseVelocityAccum += screen_pan_delta;

                const auto now_time = nanotime::now();
                const auto sample_ms = (now_time - RightMouseVelocityTime).to_ms<float32_t>();
                if (sample_ms >= 8.0f) {
                    RightMouseInertia = RightMouseVelocityAccum * (1000.0f / sample_ms);
                    RightMouseVelocityAccum = {};
                    RightMouseVelocityTime = now_time;
                }
            }
        }
        else {
            HandleSelectionMouseDrag();
        }
    }
}

void MapperEngine::DrawMapperFrame()
{
    FO_STACK_TRACE_ENTRY();

    EffectMngr.UpdateEffects(GameTime);
    FontMngr.FrameUpdate();

    {
        SprMngr.BeginScene();

        if (_curMap) {
            auto cur_map = GetCurMapPtr();
            cur_map->DrawMap();
        }

        SpritesCanDraw = true;
        OnRenderIface.Fire();
        SpritesCanDraw = false;

        DrawMainPanelImGui();
        DrawConsoleImGui();
        DrawInspectorImGui();
        CurDraw();

        SprMngr.EndScene();
    }
}

void MapperEngine::ProcessRightMouseInertia()
{
    FO_STACK_TRACE_ENTRY();

    if (InputLocked || !_curMap || MouseHoldMode == INT_PAN) {
        return;
    }

    if (RightMouseInertia.dist() < 30.0f) {
        RightMouseInertia = {};
        return;
    }

    auto cur_map = GetCurMapPtr();
    const float32_t dt_ms = std::max(GameTime.GetFrameDeltaTime().to_ms<float32_t>(), 1.0f);
    const float32_t dt_sec = dt_ms / 1000.0f;
    const float32_t zoom = std::max(cur_map->GetSpritesZoom(), 0.001f);
    cur_map->InstantScroll((RightMouseInertia * dt_sec) / zoom);

    const auto damping = std::pow(0.9f, dt_ms / 16.6667f);
    RightMouseInertia *= damping;
}

void MapperEngine::ResetPendingSelectionMove()
{
    FO_STACK_TRACE_ENTRY();

    PendingSelectionMoveEntries.clear();
}

void MapperEngine::CommitPendingSelectionMove()
{
    FO_STACK_TRACE_ENTRY();

    if (PendingSelectionMoveEntries.empty() || !_curMap) {
        ResetPendingSelectionMove();
        return;
    }

    auto move_entries = std::move(PendingSelectionMoveEntries);
    ResetPendingSelectionMove();

    PushUndoOp(GetCurMap(),
        UndoOp {"Move selection",
            [move_entries](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                for (const auto& entry : move_entries) {
                    auto target = mapper->FindEntityById(*active_map, entry.EntityId);
                    if (!target) {
                        return false;
                    }

                    if (nptr<ItemHexView> nullable_item = target.dyn_cast<ItemHexView>()) {
                        auto item = nullable_item.as_ptr();
                        if (entry.HasOffset) {
                            item->SetOffset(entry.OldOffset);
                            item->RefreshAnim();
                        }
                        if (entry.HasHex) {
                            if (!entry.OldMultihexMesh.empty()) {
                                item->SetMultihexMesh(entry.OldMultihexMesh);
                            }
                            (*active_map)->MoveItem(item, entry.OldHex);
                        }
                    }
                    else if (nptr<CritterHexView> nullable_cr = target.dyn_cast<CritterHexView>(); nullable_cr && entry.HasHex) {
                        auto cr = nullable_cr.as_ptr();
                        (*active_map)->MoveCritter(cr, entry.OldHex, false);
                    }
                }

                mapper->SetMapDirty(*active_map);
                (*active_map)->RebuildMap();
                return true;
            },
            [move_entries](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                for (const auto& entry : move_entries) {
                    auto target = mapper->FindEntityById(*active_map, entry.EntityId);
                    if (!target) {
                        return false;
                    }

                    if (nptr<ItemHexView> nullable_item = target.dyn_cast<ItemHexView>()) {
                        auto item = nullable_item.as_ptr();
                        if (entry.HasOffset) {
                            item->SetOffset(entry.NewOffset);
                            item->RefreshAnim();
                        }
                        if (entry.HasHex) {
                            if (!entry.NewMultihexMesh.empty()) {
                                item->SetMultihexMesh(entry.NewMultihexMesh);
                            }
                            (*active_map)->MoveItem(item, entry.NewHex);
                        }
                    }
                    else if (nptr<CritterHexView> nullable_cr = target.dyn_cast<CritterHexView>(); nullable_cr && entry.HasHex) {
                        auto cr = nullable_cr.as_ptr();
                        (*active_map)->MoveCritter(cr, entry.NewHex, false);
                    }
                }

                mapper->SetMapDirty(*active_map);
                (*active_map)->RebuildMap();
                return true;
            }});
}

void MapperEngine::HandleMapperKeyboardEvent(const InputEvent& ev)
{
    FO_STACK_TRACE_ENTRY();

    const auto ev_type = ev.Type;
    const auto dikdw = ev_type == InputEvent::EventType::KeyDownEvent ? ev.KeyDown.Code : KeyCode::None;
    const auto dikup = ev_type == InputEvent::EventType::KeyUpEvent ? ev.KeyUp.Code : KeyCode::None;

    // Avoid repeating
    if (dikdw != KeyCode::None && PressedKeys[static_cast<int32_t>(dikdw)]) {
        return;
    }
    if (dikup != KeyCode::None && !PressedKeys[static_cast<int32_t>(dikup)]) {
        return;
    }

    // Keyboard states, to know outside function
    PressedKeys[static_cast<int32_t>(dikup)] = false;
    PressedKeys[static_cast<int32_t>(dikdw)] = true;

    const auto block_hotkeys = IsImGuiTextInputActive();
    HandlePrimaryMapperHotkeys(dikdw, block_hotkeys);

    if (!block_hotkeys && !GetApp()->Input.IsAltDown() && !GetApp()->Input.IsCtrlDown() && !GetApp()->Input.IsShiftDown() && (dikdw == KeyCode::F11 || dikdw == KeyCode::F12)) {
        return;
    }

    HandleShiftMapperHotkeys(dikdw, block_hotkeys);
    HandleCtrlMapperHotkeys(dikdw, block_hotkeys);

    if (dikdw != KeyCode::None) {
        HandleMapperConsoleKeyDown(dikdw, ev.KeyDown.Text);
    }

    UpdateArrowScrollKeys(dikdw, dikup);
}

void MapperEngine::HandlePrimaryMapperHotkeys(KeyCode dikdw, bool block_hotkeys)
{
    FO_STACK_TRACE_ENTRY();

    if (block_hotkeys || GetApp()->Input.IsAltDown() || GetApp()->Input.IsCtrlDown() || GetApp()->Input.IsShiftDown()) {
        return;
    }

    switch (dikdw) {
    case KeyCode::F1:
        ToggleMapVisibilityFlag(GetCurMap(), Settings->ShowItem);
        break;
    case KeyCode::F2:
        ToggleMapVisibilityFlag(GetCurMap(), Settings->ShowScen);
        break;
    case KeyCode::F3:
        ToggleMapVisibilityFlag(GetCurMap(), Settings->ShowWall);
        break;
    case KeyCode::F4:
        ToggleMapVisibilityFlag(GetCurMap(), Settings->ShowCrit);
        break;
    case KeyCode::F5:
        ToggleMapVisibilityFlag(GetCurMap(), Settings->ShowTile);
        break;
    case KeyCode::F6:
        ToggleMapVisibilityFlag(GetCurMap(), Settings->ShowFast);
        break;
    case KeyCode::F7:
        WorkspaceWindowVisible = !WorkspaceWindowVisible;
        break;
    case KeyCode::F8:
        if (SprMngr.IsFullscreen()) {
            Settings->FullscreenMouseScroll = !Settings->FullscreenMouseScroll;
        }
        else {
            Settings->WindowedMouseScroll = !Settings->WindowedMouseScroll;
        }
        break;
    case KeyCode::F9:
        if (InspectorVisible) {
            SelectClear();
        }
        else if (!SelectedEntities.empty() || InContItem) {
            InspectorVisible = true;
        }
        break;
    case KeyCode::F10:
        ToggleMapperHexOverlay();
        break;
    case KeyCode::F11:
        SprMngr.ToggleFullscreen();
        break;
    case KeyCode::F12:
        SprMngr.MinimizeWindow();
        break;
    case KeyCode::Delete:
        SelectDelete();
        break;
    case KeyCode::Escape:
        if (CancelInspectorPropertyEdit()) {
            break;
        }

        if (!SelectedEntities.empty() || InContItem) {
            SelectClear();
        }
        else if (CurMode == CUR_MODE_PLACE_OBJECT) {
            MouseHoldMode = INT_NONE;
            SetCurMode(CUR_MODE_DEFAULT);
        }
        break;
    case KeyCode::Add:
        if (_curMap && !ConsoleEdit && SelectedEntities.empty()) {
            SetGlobalDayTime(ShiftDayTimeWithWrap(GetGlobalDayTime(), 60));
        }
        break;
    case KeyCode::Subtract:
        if (_curMap && !ConsoleEdit && SelectedEntities.empty()) {
            SetGlobalDayTime(ShiftDayTimeWithWrap(GetGlobalDayTime(), -60));
        }
        break;
    case KeyCode::Tab:
        SelectAxialGrid = !SelectAxialGrid;
        break;
    default:
        break;
    }
}

void MapperEngine::HandleShiftMapperHotkeys(KeyCode dikdw, bool block_hotkeys)
{
    FO_STACK_TRACE_ENTRY();

    if (block_hotkeys || !GetApp()->Input.IsShiftDown()) {
        return;
    }

    switch (dikdw) {
    case KeyCode::F7:
        ContentWindowVisible = !ContentWindowVisible;
        break;
    case KeyCode::F11:
        SprMngr.GetAtlasMngr()->DumpAtlases();
        break;
    case KeyCode::C0:
    case KeyCode::Numpad0:
    case KeyCode::C1:
    case KeyCode::Numpad1:
    case KeyCode::C2:
    case KeyCode::Numpad2:
    case KeyCode::C3:
    case KeyCode::Numpad3:
    case KeyCode::C4:
    case KeyCode::Numpad4:
        TileLayer = GetTileLayerFromKey(dikdw).value_or(TileLayer);
        break;
    case KeyCode::Tab:
        SelectEntireEntity = !SelectEntireEntity;
        break;
    default:
        break;
    }
}

void MapperEngine::HandleCtrlMapperHotkeys(KeyCode dikdw, bool block_hotkeys)
{
    FO_STACK_TRACE_ENTRY();

    if (block_hotkeys || !GetApp()->Input.IsCtrlDown()) {
        return;
    }

    switch (dikdw) {
    case KeyCode::Z:
        ExecuteUndo();
        break;
    case KeyCode::Y:
        ExecuteRedo();
        break;
    case KeyCode::X:
        BufferCut();
        break;
    case KeyCode::C:
        BufferCopy();
        break;
    case KeyCode::V:
        BufferPaste();
        break;
    case KeyCode::A:
        SelectAll();
        break;
    case KeyCode::S:
        if (_curMap) {
            SaveCurrentMap();
        }
        break;
    case KeyCode::D:
        if (_curMap) {
            auto cur_map = GetCurMapPtr();
            cur_map->SetScrollCheck(!cur_map->IsScrollCheck());
        }
        break;
    case KeyCode::B:
        MarkBlockedHexes();
        break;
    default:
        break;
    }
}

void MapperEngine::UpdateArrowScrollKeys(KeyCode dikdw, KeyCode dikup)
{
    FO_STACK_TRACE_ENTRY();

    if (dikdw != KeyCode::None && !ConsoleEdit) {
        switch (dikdw) {
        case KeyCode::Left:
            Settings->ScrollKeybLeft = true;
            break;
        case KeyCode::Right:
            Settings->ScrollKeybRight = true;
            break;
        case KeyCode::Up:
            Settings->ScrollKeybUp = true;
            break;
        case KeyCode::Down:
            Settings->ScrollKeybDown = true;
            break;
        default:
            break;
        }
    }

    if (dikup != KeyCode::None) {
        switch (dikup) {
        case KeyCode::Left:
            Settings->ScrollKeybLeft = false;
            break;
        case KeyCode::Right:
            Settings->ScrollKeybRight = false;
            break;
        case KeyCode::Up:
            Settings->ScrollKeybUp = false;
            break;
        case KeyCode::Down:
            Settings->ScrollKeybDown = false;
            break;
        default:
            break;
        }
    }
}

void MapperEngine::HandleMapperConsoleKeyDown(KeyCode dikdw, string_view key_text)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(key_text);

    if (!ConsoleEdit && dikdw == KeyCode::Grave) {
        ConsoleEdit = true;
        ConsoleStr.clear();
        ConsoleHistoryCur = numeric_cast<int32_t>(ConsoleHistory.size());
    }
}

void MapperEngine::ChangeZoom(float32_t new_zoom)
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return;
    }

    auto cur_map = GetCurMapPtr();
    const fpos32 mouse_pos = fpos32(GetApp()->Input.GetMousePosition());
    const fsize32 screen_size = fsize32(cur_map->GetScreenSize());
    const float32_t mouse_x_factor = std::clamp(mouse_pos.x / screen_size.width, 0.0f, 1.0f);
    const float32_t mouse_y_factor = std::clamp(mouse_pos.y / screen_size.height, 0.0f, 1.0f);

    cur_map->ChangeZoom(new_zoom, {mouse_x_factor, mouse_y_factor});
}

auto MapperEngine::IsImGuiMouseCaptured() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return ImGui::GetIO().WantCaptureMouse;
}

auto MapperEngine::IsImGuiTextInputActive() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return ImGui::GetIO().WantTextInput;
}

MapperEngine::EntityBuf::EntityBuf(const EntityBuf& other) :
    Id(other.Id),
    Hex(other.Hex),
    Dir(other.Dir),
    IsCritter(other.IsCritter),
    IsItem(other.IsItem),
    Slot(other.Slot),
    StackId(other.StackId),
    Proto(other.Proto)
{
    FO_STACK_TRACE_ENTRY();

    if (other.Props) {
        Props.emplace(other.Props->Copy());
    }

    Children.reserve(other.Children.size());
    for (const auto& child : other.Children) {
        Children.emplace_back(SafeAlloc::MakeUnique<EntityBuf>(*child));
    }
}

auto MapperEngine::EntityBuf::operator=(const EntityBuf& other) -> EntityBuf&
{
    FO_STACK_TRACE_ENTRY();

    if (this != &other) {
        Id = other.Id;
        Hex = other.Hex;
        Dir = other.Dir;
        IsCritter = other.IsCritter;
        IsItem = other.IsItem;
        Slot = other.Slot;
        StackId = other.StackId;
        Proto = other.Proto;
        if (other.Props) {
            Props.emplace(other.Props->Copy());
        }
        else {
            Props.reset();
        }
        Children.clear();
        Children.reserve(other.Children.size());
        for (const auto& child : other.Children) {
            Children.emplace_back(SafeAlloc::MakeUnique<EntityBuf>(*child));
        }
    }

    return *this;
}

MapperEngine::UndoOp::UndoOp(string label, std::function<bool(ptr<MapperEngine>, ptr<ptr<MapView>>)> undo, std::function<bool(ptr<MapperEngine>, ptr<ptr<MapView>>)> redo, bool is_snapshot) :
    Label(std::move(label)),
    IsSnapshot(is_snapshot),
    Undo(std::move(undo)),
    Redo(std::move(redo))
{
    FO_STACK_TRACE_ENTRY();
}

auto MapperEngine::GetUndoContext(nptr<MapView> nullable_map, bool create) -> nptr<UndoContext>
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_map) {
        return nullptr;
    }

    auto map = nullable_map.as_ptr();

    if (!create) {
        if (const auto it = UndoContexts.find(map); it != UndoContexts.end()) {
            return &it->second;
        }

        return nullptr;
    }

    return &UndoContexts[map];
}

auto MapperEngine::GetUndoContext(nptr<const MapView> nullable_map, bool create) const -> nptr<const UndoContext>
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_map) {
        return nullptr;
    }

    auto map = nullable_map.as_ptr();
    ptr<MapView> map_ptr = const_cast<MapView*>(std::addressof(*map));

    if (!create) {
        if (const auto it = UndoContexts.find(map_ptr); it != UndoContexts.end()) {
            return &it->second;
        }
    }

    return nullptr;
}

void MapperEngine::ClearUndoContext(nptr<MapView> nullable_map)
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_map) {
        return;
    }

    auto map = nullable_map.as_ptr();
    UndoContexts.erase(map);
}

void MapperEngine::RemapUndoContext(nptr<MapView> nullable_old_map, nptr<MapView> nullable_new_map)
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_old_map || !nullable_new_map || nullable_old_map == nullable_new_map) {
        return;
    }

    auto old_map = nullable_old_map.as_ptr();
    auto new_map = nullable_new_map.as_ptr();

    if (const auto it = UndoContexts.find(old_map); it != UndoContexts.end()) {
        auto ctx = std::move(it->second);
        UndoContexts.erase(it);
        UndoContexts[new_map] = std::move(ctx);
    }
}

void MapperEngine::PushUndoOp(nptr<MapView> map, UndoOp op)
{
    FO_STACK_TRACE_ENTRY();

    if (UndoRedoInProgress || !map || !op.Undo || !op.Redo) {
        return;
    }

    auto nullable_ctx = GetUndoContext(map, true);
    FO_VERIFY_AND_THROW(nullable_ctx, "Missing script execution context");
    auto ctx = nullable_ctx.as_ptr();

    ctx->RedoStack.clear();
    ctx->UndoStack.emplace_back(std::move(op));

    if (ctx->UndoStack.size() > MaxUndoDepth) {
        ctx->UndoStack.erase(ctx->UndoStack.begin());

        if (ctx->CleanUndoDepth > 0) {
            ctx->CleanUndoDepth--;
        }
        else if (ctx->CleanUndoDepth == 0) {
            ctx->CleanUndoDepth = -1;
        }
    }
}

auto MapperEngine::CanUndo() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return false;
    }

    if (nptr<const UndoContext> nullable_ctx = GetUndoContext(GetCurMap(), false); nullable_ctx) {
        auto ctx = nullable_ctx.as_ptr();
        return !ctx->UndoStack.empty();
    }

    return false;
}

auto MapperEngine::CanRedo() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return false;
    }

    if (nptr<const UndoContext> nullable_ctx = GetUndoContext(GetCurMap(), false); nullable_ctx) {
        auto ctx = nullable_ctx.as_ptr();
        return !ctx->RedoStack.empty();
    }

    return false;
}

auto MapperEngine::GetUndoLabel() const -> string
{
    FO_STACK_TRACE_ENTRY();

    if (nptr<const UndoContext> nullable_ctx = GetUndoContext(GetCurMap(), false); nullable_ctx) {
        auto ctx = nullable_ctx.as_ptr();

        if (!ctx->UndoStack.empty()) {
            return ctx->UndoStack.back().Label;
        }
    }

    return {};
}

auto MapperEngine::GetRedoLabel() const -> string
{
    FO_STACK_TRACE_ENTRY();

    if (nptr<const UndoContext> nullable_ctx = GetUndoContext(GetCurMap(), false); nullable_ctx) {
        auto ctx = nullable_ctx.as_ptr();

        if (!ctx->RedoStack.empty()) {
            return ctx->RedoStack.back().Label;
        }
    }

    return {};
}

auto MapperEngine::ExecuteUndo() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return false;
    }

    auto map = GetCurMapPtr();
    auto nullable_ctx = GetUndoContext(map, false);
    if (!nullable_ctx) {
        return false;
    }

    auto ctx = nullable_ctx.as_ptr();
    if (ctx->UndoStack.empty()) {
        return false;
    }

    auto op = std::move(ctx->UndoStack.back());
    ctx->UndoStack.pop_back();

    ptr<MapView> active_map = map;
    UndoRedoInProgress = true;
    ptr<MapperEngine> mapper = this;
    const auto ok = op.Undo(mapper, &active_map);
    UndoRedoInProgress = false;

    if (!ok) {
        auto rollback_ctx = GetUndoContext(map, true).as_ptr();
        rollback_ctx->UndoStack.emplace_back(std::move(op));
        return false;
    }

    auto active_ctx = GetUndoContext(active_map, true).as_ptr();
    active_ctx->RedoStack.emplace_back(std::move(op));
    SetMapDirty(active_map, active_ctx->CleanUndoDepth < 0 || numeric_cast<int32_t>(active_ctx->UndoStack.size()) != active_ctx->CleanUndoDepth);
    return true;
}

auto MapperEngine::ExecuteRedo() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return false;
    }

    auto map = GetCurMapPtr();
    auto nullable_ctx = GetUndoContext(map, false);
    if (!nullable_ctx) {
        return false;
    }

    auto ctx = nullable_ctx.as_ptr();
    if (ctx->RedoStack.empty()) {
        return false;
    }

    auto op = std::move(ctx->RedoStack.back());
    ctx->RedoStack.pop_back();

    ptr<MapView> active_map = map;
    UndoRedoInProgress = true;
    ptr<MapperEngine> mapper = this;
    const auto ok = op.Redo(mapper, &active_map);
    UndoRedoInProgress = false;

    if (!ok) {
        auto rollback_ctx = GetUndoContext(map, true).as_ptr();
        rollback_ctx->RedoStack.emplace_back(std::move(op));
        return false;
    }

    auto active_ctx = GetUndoContext(active_map, true).as_ptr();
    active_ctx->UndoStack.emplace_back(std::move(op));
    SetMapDirty(active_map, active_ctx->CleanUndoDepth < 0 || numeric_cast<int32_t>(active_ctx->UndoStack.size()) != active_ctx->CleanUndoDepth);
    return true;
}

auto MapperEngine::CaptureMapSnapshot(nptr<const MapView> nullable_map) const -> string
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_map) {
        return {};
    }

    auto map = nullable_map.as_ptr();
    return map->SaveToText();
}

void MapperEngine::CaptureEntityBuf(EntityBuf& entity_buf, ptr<ClientEntity> entity) const
{
    FO_STACK_TRACE_ENTRY();

    auto nullable_cr = entity.dyn_cast<CritterHexView>();
    auto nullable_item_view = entity.dyn_cast<ItemView>();
    auto nullable_entity_with_proto = entity.dyn_cast<EntityWithProto>();
    FO_VERIFY_AND_THROW(nullable_entity_with_proto, "Captured entity does not have an associated prototype");
    auto entity_with_proto = nullable_entity_with_proto.as_ptr();

    entity_buf.Id = entity->GetId();
    entity_buf.IsCritter = !!nullable_cr;
    entity_buf.IsItem = !!nullable_item_view;
    entity_buf.Proto = entity_with_proto->GetProto();
    entity_buf.Props.emplace(entity->GetProperties().Copy());

    if (nullable_cr) {
        auto cr = nullable_cr.as_ptr();
        entity_buf.Hex = cr->GetHex();
        entity_buf.Dir = cr->GetDir();
    }
    else if (nptr<ItemHexView> nullable_item = entity.dyn_cast<ItemHexView>()) {
        auto item = nullable_item.as_ptr();
        entity_buf.Hex = item->GetHex();
        entity_buf.Slot = item->GetCritterSlot();
        entity_buf.StackId = any_t {string(item->GetContainerStack())};
    }
    else if (nullable_item_view) {
        auto item_view = nullable_item_view.as_ptr();
        entity_buf.Slot = item_view->GetCritterSlot();
        entity_buf.StackId = any_t {string(item_view->GetContainerStack())};
    }

    vector<refcount_ptr<ItemView>> children = GetEntityInnerItems(entity);

    for (size_t i = 0; i < children.size(); i++) {
        auto child = children[i].as_ptr();
        unique_ptr<EntityBuf> child_buf = SafeAlloc::MakeUnique<EntityBuf>();
        CaptureEntityBuf(*child_buf, child);
        entity_buf.Children.emplace_back(std::move(child_buf));
    }
}

void MapperEngine::RestoreEntityBufChildren(const EntityBuf& entity_buf, ptr<ItemView> item)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& child_buf : entity_buf.Children) {
        auto inner_item = item->AddMapperInnerItem(child_buf->Id, child_buf->Proto.dyn_cast<const ProtoItem>().as_ptr(), child_buf->StackId, child_buf->GetProps());
        RestoreEntityBufChildren(*child_buf, inner_item);
    }
}

auto MapperEngine::RestoreEntityBuf(const EntityBuf& entity_buf, nptr<Entity> owner) -> nptr<ClientEntity>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_curMap, "Mapper has no current map");
    auto cur_map = GetCurMapPtr();

    if (!owner) {
        if (entity_buf.IsCritter) {
            auto cr = cur_map->AddMapperCritter(entity_buf.Proto->GetProtoId(), entity_buf.Hex, entity_buf.Dir, entity_buf.GetProps(), entity_buf.Id);

            for (const auto& child_buf : entity_buf.Children) {
                auto inv_item = cr->AddMapperInvItem(child_buf->Id, child_buf->Proto.dyn_cast<const ProtoItem>().as_ptr(), child_buf->Slot, child_buf->GetProps());
                RestoreEntityBufChildren(*child_buf, inv_item);
            }

            return cr;
        }

        if (entity_buf.IsItem) {
            auto item = cur_map->AddMapperItem(entity_buf.Proto->GetProtoId(), entity_buf.Hex, entity_buf.GetProps(), entity_buf.Id);
            RestoreEntityBufChildren(entity_buf, item);
            return item;
        }

        return nullptr;
    }

    if (nptr<CritterView> nullable_cr = owner.dyn_cast<CritterView>()) {
        auto cr = nullable_cr.as_ptr();
        auto item = cr->AddMapperInvItem(entity_buf.Id, entity_buf.Proto.dyn_cast<const ProtoItem>().as_ptr(), entity_buf.Slot, entity_buf.GetProps());
        RestoreEntityBufChildren(entity_buf, item);
        return item;
    }

    if (nptr<ItemView> nullable_item_owner = owner.dyn_cast<ItemView>()) {
        auto item_owner = nullable_item_owner.as_ptr();
        auto item = item_owner->AddMapperInnerItem(entity_buf.Id, entity_buf.Proto.dyn_cast<const ProtoItem>().as_ptr(), entity_buf.StackId, entity_buf.GetProps());
        RestoreEntityBufChildren(entity_buf, item);
        return item;
    }

    return nullptr;
}

auto MapperEngine::FindEntityById(ptr<MapView> map, ident_t id) -> nptr<ClientEntity>
{
    FO_STACK_TRACE_ENTRY();

    if (!id) {
        return nullptr;
    }

    if (nptr<CritterHexView> cr = map->GetCritter(id)) {
        return cr;
    }
    if (nptr<ItemHexView> item = map->GetItem(id)) {
        return item;
    }

    std::function<nptr<ClientEntity>(ptr<ItemView>)> find_inner_item = [&](ptr<ItemView> owner) -> nptr<ClientEntity> {
        span<refcount_ptr<ItemView>> inner_items = owner->GetInnerItems();

        for (size_t i = 0; i < inner_items.size(); i++) {
            auto inner_item = inner_items[i].as_ptr();

            if (inner_item->GetId() == id) {
                return inner_item;
            }
            if (nptr<ClientEntity> found = find_inner_item(inner_item)) {
                return found;
            }
        }
        return nullptr;
    };

    span<refcount_ptr<CritterHexView>> critters = map->GetCritters();

    for (size_t cr_index = 0; cr_index < critters.size(); cr_index++) {
        auto cr = critters[cr_index].as_ptr();
        span<refcount_ptr<ItemView>> inv_items = cr->GetInvItems();

        for (size_t item_index = 0; item_index < inv_items.size(); item_index++) {
            auto inv_item = inv_items[item_index].as_ptr();

            if (inv_item->GetId() == id) {
                return inv_item;
            }
            if (nptr<ClientEntity> found = find_inner_item(inv_item)) {
                return found;
            }
        }
    }

    span<refcount_ptr<ItemHexView>> map_items = map->GetItems();

    for (size_t i = 0; i < map_items.size(); i++) {
        auto map_item = map_items[i].as_ptr();

        if (nptr<ClientEntity> found = find_inner_item(map_item)) {
            return found;
        }
    }

    return nullptr;
}

auto MapperEngine::RestoreMapSnapshot(ptr<ptr<MapView>> map, string_view map_name, const string& map_text) -> bool
{
    FO_STACK_TRACE_ENTRY();

    ptr<MapView> old_map = *map;
    UnloadMap(old_map, false);

    auto nullable_restored_map = LoadMapFromText(map_name, map_text);
    if (!nullable_restored_map) {
        return false;
    }

    auto restored_map = nullable_restored_map.as_ptr();
    RemapUndoContext(old_map, restored_map);
    ShowMap(restored_map);
    *map = restored_map;
    return true;
}

auto MapperEngine::ApplyEntityPropertyText(ptr<Entity> entity, ptr<const Property> prop, string_view value_text) -> bool
{
    FO_STACK_TRACE_ENTRY();

    try {
        entity->GetPropertiesForEdit()->ApplyPropertyFromText(prop, value_text);
        SetMapDirty(GetCurMap());

        if (nptr<ItemHexView> nullable_item = entity.dyn_cast<ItemHexView>()) {
            auto item = nullable_item.as_ptr();
            if (item->GetMultihexGeneration() == MultihexGenerationType::SameSibling) {
                auto cur_map = GetCurMapPtr();

                for (int32_t i = 0; i < GameSettings::MAP_DIR_COUNT; i++) {
                    if (mpos hex = item->GetHex(); GeometryHelper::MoveHexByDir(hex, hdir(i), cur_map->GetSize())) {
                        vector<ptr<ItemHexView>> main_mesh_items = vec_filter(cur_map->GetItemsOnHex(hex), [&](ptr<ItemHexView> item2) -> bool {
                            if (SelectedEntitiesSet.contains(item2.as_ptr())) {
                                return false;
                            }
                            return item->GetProtoId() == item2->GetProtoId();
                        });
                        for (ptr<ItemHexView> item2 : main_mesh_items) {
                            item2->GetPropertiesForEdit()->ApplyPropertyFromText(prop, value_text);
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception&) {
        return false;
    }

    if (nptr<ItemHexView> nullable_hex_item = entity.dyn_cast<ItemHexView>()) {
        auto hex_item = nullable_hex_item.as_ptr();
        ptr<const Property> offset_prop = hex_item->GetPropertyOffset();
        ptr<const Property> pic_map_prop = hex_item->GetPropertyPicMap();

        if (prop == offset_prop) {
            hex_item->RefreshOffs();
        }
        if (prop == pic_map_prop) {
            hex_item->RefreshAnim();
        }
    }

    return true;
}

void MapperEngine::DrawMainPanelImGui()
{
    FO_STACK_TRACE_ENTRY();

    if (ImGui::BeginMainMenuBar()) {
        const auto pos = ImGui::GetWindowPos();
        const auto size = ImGui::GetWindowSize();

        MainPanelPos = {iround<int32_t>(pos.x), iround<int32_t>(pos.y)};
        MainPanelWindowRect = {0, 0, iround<int32_t>(size.x), iround<int32_t>(size.y)};
        MainPanelContentRect = {12, MainPanelWindowRect.height + 8, 520, 120};
        ProtosOnScreen = std::max(1, ProtoWidth > 0 ? MainPanelContentRect.width / ProtoWidth : 1);

        const auto run_menu_action = [](bool triggered, auto&& action) {
            if (triggered) {
                action();
            }
        };

        const auto run_menu_action_with_message = [&](bool triggered, auto&& action, string_view message) {
            run_menu_action(triggered, [&] {
                action();
                AddMess(message);
            });
        };

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save current", "Ctrl+S", false, static_cast<bool>(_curMap))) {
                SaveCurrentMap();
            }
            if (ImGui::MenuItem("Reset changes", nullptr, false, _curMap && IsMapDirty(GetCurMap()))) {
                ResetCurrentMapChanges();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                GetApp()->RequestQuit();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows")) {
            ImGui::MenuItem("Workspace", "F7", &WorkspaceWindowVisible);
            ImGui::MenuItem("Content", "Shift+F7", &ContentWindowVisible);

            if (ImGui::MenuItem("Console", "~", ConsoleEdit)) {
                ConsoleEdit = !ConsoleEdit;
                if (ConsoleEdit) {
                    ConsoleHistoryCur = numeric_cast<int32_t>(ConsoleHistory.size());
                }
            }

            ImGui::MenuItem("Critter animations", nullptr, &CritterAnimationsWindowVisible);
            ImGui::MenuItem("Script call", nullptr, &ScriptCallWindowVisible);
            ImGui::MenuItem("Map browser", nullptr, &MapListWindowVisible);
            ImGui::MenuItem("Controls", nullptr, &MapWindowVisible, static_cast<bool>(_curMap));
            ImGui::MenuItem("History", nullptr, &HistoryWindowVisible, static_cast<bool>(_curMap));
            ImGui::MenuItem("Settings", nullptr, &SettingsWindowVisible);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            const auto undo_label = GetUndoLabel();
            const auto redo_label = GetRedoLabel();

            if (ImGui::MenuItem(undo_label.empty() ? "Undo" : strex("Undo {}", undo_label).c_str(), "Ctrl+Z", false, CanUndo())) {
                ExecuteUndo();
            }
            if (ImGui::MenuItem(redo_label.empty() ? "Redo" : strex("Redo {}", redo_label).c_str(), "Ctrl+Y", false, CanRedo())) {
                ExecuteRedo();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Select all", "Ctrl+A")) {
                SelectAll();
            }
            if (ImGui::MenuItem("Clear selection")) {
                SelectClear();
            }
            if (ImGui::MenuItem("Delete selected", "Del")) {
                SelectDelete();
            }

            ImGui::Separator();
            run_menu_action_with_message(ImGui::MenuItem("Copy", "Ctrl+C"), [&] { BufferCopy(); }, "Copied selection");
            run_menu_action_with_message(ImGui::MenuItem("Cut", "Ctrl+X"), [&] { BufferCut(); }, "Cut selection");
            run_menu_action_with_message(ImGui::MenuItem("Paste", "Ctrl+V"), [&] { BufferPaste(); }, "Pasted selection");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            bool view_layer_changed = false;

            view_layer_changed |= ImGui::MenuItem("Items", nullptr, &Settings->ShowItem);
            view_layer_changed |= ImGui::MenuItem("Scenery", nullptr, &Settings->ShowScen);
            view_layer_changed |= ImGui::MenuItem("Walls", nullptr, &Settings->ShowWall);
            view_layer_changed |= ImGui::MenuItem("Critters", nullptr, &Settings->ShowCrit);
            view_layer_changed |= ImGui::MenuItem("Tiles", nullptr, &Settings->ShowTile);
            view_layer_changed |= ImGui::MenuItem("Roof", nullptr, &Settings->ShowRoof);
            view_layer_changed |= ImGui::MenuItem("Fast", nullptr, &Settings->ShowFast);

            if (view_layer_changed && _curMap) {
                _curMap->RebuildMap();
            }

            ImGui::Separator();
            ImGui::MenuItem("Axial grid selection", nullptr, &SelectAxialGrid);
            ImGui::MenuItem("Select entire entity", nullptr, &SelectEntireEntity);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            run_menu_action_with_message(
                ImGui::MenuItem("Rebuild map", nullptr, false, static_cast<bool>(_curMap)),
                [&] {
                    auto cur_map = GetCurMapPtr();
                    cur_map->RebuildMap();
                },
                "Map rebuilt");
            run_menu_action_with_message(ImGui::MenuItem("Mark blocked hexes", nullptr, false, static_cast<bool>(_curMap)), [&] { MarkBlockedHexes(); }, "Blocked hexes marked");
            run_menu_action_with_message(ImGui::MenuItem("Reverse lights", nullptr, false, static_cast<bool>(_curMap)), [&] { ParseCommand("* reverse-light"); }, "Reverse lights done");
            run_menu_action_with_message(ImGui::MenuItem("Merge by command", nullptr, false, static_cast<bool>(_curMap)), [&] { ParseCommand("* merge-items"); }, "Merge items command done");
            run_menu_action_with_message(ImGui::MenuItem("Break by command", nullptr, false, static_cast<bool>(_curMap)), [&] { ParseCommand("* break-items"); }, "Break items command done");

            ImGui::Separator();
            if (ImGui::MenuItem("Merge multihex items", nullptr, false, static_cast<bool>(_curMap))) {
                auto cur_map = GetCurMapPtr();
                const auto merged = MergeItemsToMultihexMeshes(cur_map);
                AddMess(strex("Merged items: {}", merged));
            }
            if (ImGui::MenuItem("Break multihex items", nullptr, false, static_cast<bool>(_curMap))) {
                auto cur_map = GetCurMapPtr();
                const auto broken = BreakItemsMultihexMeshes(cur_map);
                AddMess(strex("Broken items: {}", broken));
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("System")) {
            if (ImGui::MenuItem("Toggle fullscreen", "F11")) {
                SprMngr.ToggleFullscreen();
            }
            if (ImGui::MenuItem("Minimize", "F12")) {
                SprMngr.MinimizeWindow();
            }
            if (ImGui::MenuItem("Dump atlases")) {
                SprMngr.GetAtlasMngr()->DumpAtlases();
            }

            ImGui::Separator();
            if (SprMngr.IsFullscreen()) {
                ImGui::MenuItem("Fullscreen mouse scroll", nullptr, &Settings->FullscreenMouseScroll);
            }
            else {
                ImGui::MenuItem("Windowed mouse scroll", nullptr, &Settings->WindowedMouseScroll);
            }
            ImGui::EndMenu();
        }

        if (_curMap && IsMapDirty(GetCurMap())) {
            constexpr string_view dirty_label = "*** Save ***";
            const auto label_width = ImGui::CalcTextSize(dirty_label.data()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            const auto right_x = numeric_cast<float32_t>(MainPanelWindowRect.width) - label_width - ImGui::GetStyle().ItemSpacing.x * 2.0f;

            if (ImGui::GetCursorPosX() < right_x) {
                ImGui::SetCursorPosX(right_x);
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.45f, 0.10f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.86f, 0.56f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.60f, 0.36f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));

            if (ImGui::Button(dirty_label.data())) {
                SaveCurrentMap();
            }

            ImGui::PopStyleColor(4);
        }

        ImGui::EndMainMenuBar();
    }

    DrawWorkspaceWindowImGui();
    DrawContentWindowImGui();
    DrawCritterAnimationsWindowImGui();
    DrawScriptCallWindowImGui();
    DrawMapListWindowImGui();
    DrawMapWindowImGui();
    DrawHistoryWindowImGui();
    DrawSettingsWindowImGui();
}

void MapperEngine::DrawWorkspaceWindowImGui()
{
    FO_STACK_TRACE_ENTRY();

    if (!WorkspaceWindowVisible) {
        return;
    }

    ImGui::SetNextWindowPos(
        {
            numeric_cast<float32_t>(MainPanelPos.x + MainPanelContentRect.x),
            numeric_cast<float32_t>(MainPanelPos.y + MainPanelContentRect.y),
        },
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({500.0f, 600.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Workspace", &WorkspaceWindowVisible, 0)) {
        ImGui::End();
        return;
    }

    const auto toggle_visibility = [&](string_view label, string_view tooltip, bool& value) {
        if (value) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }

        const auto clicked = ImGui::Button(label.data(), {32.0f, 0.0f});

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", tooltip.data());
        }

        if (value) {
            ImGui::PopStyleColor(2);
        }

        if (clicked) {
            value = !value;
        }

        return clicked;
    };

    auto visibility_changed = false;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.0f, ImGui::GetStyle().ItemSpacing.y});
    visibility_changed |= toggle_visibility("Items", "Items", Settings->ShowItem);
    ImGui::SameLine();
    visibility_changed |= toggle_visibility("Scenery", "Scenery", Settings->ShowScen);
    ImGui::SameLine();
    visibility_changed |= toggle_visibility("Walls", "Walls", Settings->ShowWall);
    ImGui::SameLine();
    visibility_changed |= toggle_visibility("Critters", "Critters", Settings->ShowCrit);
    ImGui::SameLine();
    visibility_changed |= toggle_visibility("Tiles", "Tiles", Settings->ShowTile);
    ImGui::SameLine();
    visibility_changed |= toggle_visibility("Roof", "Roof", Settings->ShowRoof);
    ImGui::SameLine();
    visibility_changed |= toggle_visibility("Fast", "Fast", Settings->ShowFast);
    ImGui::PopStyleVar();

    if (visibility_changed && _curMap) {
        auto cur_map = GetCurMapPtr();
        cur_map->RebuildMap();
    }

    ImGui::Separator();

    auto& workspace_filter_buf = WorkspaceFilterBuf;
    constexpr array tab_order {
        INT_MODE_CUSTOM0,
        INT_MODE_CUSTOM1,
        INT_MODE_CUSTOM2,
        INT_MODE_CUSTOM3,
        INT_MODE_CUSTOM4,
        INT_MODE_CUSTOM5,
        INT_MODE_CUSTOM6,
        INT_MODE_CUSTOM7,
        INT_MODE_CUSTOM8,
        INT_MODE_CUSTOM9,
        INT_MODE_ITEM,
        INT_MODE_TILE,
        INT_MODE_CRIT,
        INT_MODE_FAST,
        INT_MODE_IGNORE,
    };

    constexpr auto workspace_table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("##WorkspaceTable", 3, workspace_table_flags)) {
        ImGui::TableSetupColumn("Tabs", ImGuiTableColumnFlags_WidthStretch, 0.9f);
        ImGui::TableSetupColumn("SubTabs", ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch, 4.0f);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::Text("Tabs");
        ImGui::Separator();
        if (ImGui::BeginChild("##WorkspaceTabs", {0.0f, 0.0f}, false)) {
            for (const auto mode : tab_order) {
                if (Tabs[mode].empty()) {
                    continue;
                }

                const auto selected = ActivePanelMode == mode;
                const auto name = !PanelModeNames[mode].empty() ? PanelModeNames[mode] : strex("Tab {}", mode).str();
                if (ImGui::Selectable(name.c_str(), selected)) {
                    SetActivePanelMode(mode);
                }
            }
        }
        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::Text("SubTabs");
        ImGui::Separator();
        if (ImGui::BeginChild("##WorkspaceSubTabs", {0.0f, 0.0f}, false)) {
            if (nptr<const SubTab> active_subtab = GetActiveSubTab(); active_subtab) {
                for (auto& [name, subtab] : Tabs[ActivePanelMode]) {
                    const auto selected = active_subtab == &subtab;
                    if (ImGui::Selectable(name.c_str(), selected)) {
                        ActiveSubTabs[ActivePanelMode] = &subtab;
                        RefreshActiveProtoLists();
                    }
                }
            }
            else {
                ImGui::TextDisabled("No subtabs");
            }
        }
        ImGui::EndChild();

        ImGui::TableNextColumn();
        if (IsItemMode() || IsCritMode()) {
            ImGui::InputTextWithHint("##WorkspaceFilter", "Filter prototypes...", workspace_filter_buf.data(), workspace_filter_buf.size());
            const auto filter = InputBufferView(workspace_filter_buf);
            ImGui::Separator();

            if (ImGui::BeginChild("##WorkspaceProtoList", {0.0f, 0.0f}, false)) {
                const auto draw_proto_entry = [&](int32_t index, string_view label, nptr<const Sprite> sprite, auto&& on_select) {
                    ImGui::PushID(index);
                    const auto selected = index == GetActiveProtoIndex();
                    const auto row_height = 72.0f;
                    const auto row_width = ImGui::GetContentRegionAvail().x;
                    const auto cursor = ImGui::GetCursorScreenPos();
                    const auto clicked = ImGui::Selectable("##ProtoRow", selected, 0, {row_width, row_height});
                    const auto rect_min = ImGui::GetItemRectMin();
                    const auto rect_max = ImGui::GetItemRectMax();
                    ptr<ImDrawList> draw_list = ImGui::GetWindowDrawList();
                    draw_list->AddRectFilled(rect_min, rect_max, ImGui::ColorConvertFloat4ToU32(selected ? ImVec4(0.24f, 0.32f, 0.18f, 0.65f) : ImVec4(0.08f, 0.08f, 0.08f, 0.35f)), 4.0f);
                    draw_list->AddRect(rect_min, rect_max, ImGui::GetColorU32(ImGuiCol_Border), 4.0f);

                    const ImVec2 preview_min {cursor.x + 8.0f, cursor.y + 8.0f};
                    const ImVec2 preview_max {cursor.x + 64.0f, cursor.y + 64.0f};
                    draw_list->AddRectFilled(preview_min, preview_max, ImGui::ColorConvertFloat4ToU32({0.12f, 0.12f, 0.12f, 1.0f}), 4.0f);

                    if (nptr<const AtlasSprite> nullable_atlas_sprite = ResolveAtlasSprite(sprite); nullable_atlas_sprite) {
                        auto atlas_sprite = nullable_atlas_sprite.as_ptr();

                        if (nptr<const RenderTexture> nullable_texture = atlas_sprite->GetBatchTexture(); nullable_texture) {
                            auto texture = nullable_texture.as_ptr();
                            const auto uv = atlas_sprite->GetAtlasRect();
                            const auto sprite_size = sprite->GetSize();
                            const auto scale = std::min(56.0f / std::max(1, sprite_size.width), 56.0f / std::max(1, sprite_size.height));
                            const auto draw_width = numeric_cast<float32_t>(sprite_size.width) * scale;
                            const auto draw_height = numeric_cast<float32_t>(sprite_size.height) * scale;
                            const ImVec2 image_min {preview_min.x + (56.0f - draw_width) * 0.5f, preview_min.y + (56.0f - draw_height) * 0.5f};
                            const ImVec2 image_max {image_min.x + draw_width, image_min.y + draw_height};
                            draw_list->AddImage(cast_to_void(texture.get()), image_min, image_max, {uv.x, uv.y}, {uv.x + uv.width, uv.y + uv.height});
                        }
                    }

                    draw_list->AddText({cursor.x + 76.0f, cursor.y + 10.0f}, ImGui::GetColorU32(ImGuiCol_Text), string(label).c_str());

                    if (clicked) {
                        on_select();
                    }

                    if (ImGui::IsItemHovered() && sprite) {
                        if (ImGui::BeginTooltip()) {
                            if (nptr<const AtlasSprite> nullable_atlas_sprite = ResolveAtlasSprite(sprite); nullable_atlas_sprite) {
                                auto atlas_sprite = nullable_atlas_sprite.as_ptr();

                                if (nptr<const RenderTexture> nullable_texture = atlas_sprite->GetBatchTexture(); nullable_texture) {
                                    auto texture = nullable_texture.as_ptr();
                                    const auto uv = atlas_sprite->GetAtlasRect();
                                    const auto sprite_size = sprite->GetSize();
                                    ImGui::Image(cast_to_void(texture.get()), {numeric_cast<float32_t>(std::max(1, sprite_size.width)), numeric_cast<float32_t>(std::max(1, sprite_size.height))}, {uv.x, uv.y}, {uv.x + uv.width, uv.y + uv.height});
                                }
                                else {
                                    ImGui::TextDisabled("No texture");
                                }
                            }
                            else {
                                ImGui::TextDisabled("No preview");
                            }

                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::PopID();
                };

                if (IsItemMode() && ActiveItemProtos) {
                    vector<int32_t> visible_indices;
                    visible_indices.reserve(ActiveItemProtos->size());

                    for (int32_t i = 0; i < numeric_cast<int32_t>(ActiveItemProtos->size()); i++) {
                        const auto& proto = (*ActiveItemProtos)[i];

                        if (ContainsCaseInsensitive(proto->GetName(), filter)) {
                            visible_indices.emplace_back(i);
                        }
                    }

                    ImGuiListClipper clipper;
                    clipper.Begin(numeric_cast<int32_t>(visible_indices.size()), 72.0f);
                    while (clipper.Step()) {
                        for (int32_t row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                            const auto i = visible_indices[row];
                            const auto& proto = (*ActiveItemProtos)[i];
                            const auto label = string(proto->GetName());

                            draw_proto_entry(i, label, GetPreviewSprite(proto->GetPicMap()), [&] {
                                SetActiveProtoIndex(i);

                                if (_curMap && ImGui::GetIO().KeyCtrl) {
                                    const auto pid = proto->GetProtoId();
                                    auto& stab = Tabs[INT_MODE_IGNORE][DEFAULT_SUB_TAB];
                                    auto found = false;

                                    for (auto it = stab.ItemProtos.begin(); it != stab.ItemProtos.end(); ++it) {
                                        if ((*it)->GetProtoId() == pid) {
                                            found = true;
                                            stab.ItemProtos.erase(it);
                                            break;
                                        }
                                    }

                                    if (!found) {
                                        stab.ItemProtos.emplace_back(proto);
                                    }

                                    auto cur_map = GetCurMapPtr();
                                    cur_map->SwitchIgnorePid(pid);
                                    cur_map->RebuildMap();
                                }
                                else if (ImGui::GetIO().KeyAlt && !SelectedEntities.empty()) {
                                    auto add = true;

                                    if (proto->GetStackable()) {
                                        vector<refcount_ptr<ItemView>> children = GetEntityInnerItems(SelectedEntities.front().as_ptr());

                                        for (size_t child_index = 0; child_index < children.size(); child_index++) {
                                            auto child = children[child_index].as_ptr();

                                            if (proto->GetProtoId() == child->GetProtoId()) {
                                                add = false;
                                                break;
                                            }
                                        }
                                    }

                                    if (add) {
                                        CreateItem(proto->GetProtoId(), {}, SelectedEntities.front().as_ptr());
                                    }
                                }
                                else {
                                    SetCurMode(CUR_MODE_PLACE_OBJECT);
                                }
                            });
                        }
                    }
                }
                else if (IsCritMode() && ActiveCritterProtos) {
                    vector<int32_t> visible_indices;
                    visible_indices.reserve(ActiveCritterProtos->size());

                    for (int32_t i = 0; i < numeric_cast<int32_t>(ActiveCritterProtos->size()); i++) {
                        const auto& proto = (*ActiveCritterProtos)[i];

                        if (ContainsCaseInsensitive(proto->GetName(), filter)) {
                            visible_indices.emplace_back(i);
                        }
                    }

                    ImGuiListClipper clipper;
                    clipper.Begin(numeric_cast<int32_t>(visible_indices.size()), 72.0f);
                    while (clipper.Step()) {
                        for (int32_t row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                            const auto i = visible_indices[row];
                            const auto& proto = (*ActiveCritterProtos)[i];
                            const auto label = string(proto->GetName());
                            auto preview_sprite = ResMngr.GetCritterPreviewSpr(proto->GetModelName(), CritterStateAnim::Unarmed, CritterActionAnim::Idle, CritterDir, nullptr);
                            draw_proto_entry(i, label, preview_sprite, [&] {
                                SetActiveProtoIndex(i);
                                SetCurMode(CUR_MODE_PLACE_OBJECT);
                            });
                        }
                    }
                }
            }
            ImGui::EndChild();
        }
        else {
            ImGui::Separator();
            ImGui::TextDisabled("This mode uses the Content window.");
            if (ImGui::Button("Open Content")) {
                ContentWindowVisible = true;
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

void MapperEngine::DrawContentWindowImGui()
{
    FO_STACK_TRACE_ENTRY();

    if (!ContentWindowVisible) {
        return;
    }

    ImGui::SetNextWindowPos(
        {
            numeric_cast<float32_t>(MainPanelPos.x + MainPanelContentRect.x),
            numeric_cast<float32_t>(MainPanelPos.y + MainPanelContentRect.y + 162),
        },
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({520.0f, 540.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Content", &ContentWindowVisible, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    auto& map_name_buf = ContentMapNameBuf;
    auto& map_filter_buf = ContentMapFilterBuf;
    int32_t& resize_w = ContentResizeW;
    int32_t& resize_h = ContentResizeH;

    const auto run_button_action = [](bool triggered, auto&& action) {
        if (triggered) {
            action();
        }
    };

    const auto run_button_action_with_message = [&](bool triggered, auto&& action, string_view message) {
        run_button_action(triggered, [&] {
            action();
            AddMess(message);
        });
    };

    const auto get_map_name_input = [&] { return strvex(InputBufferView(map_name_buf)).trim().str(); };

    if (ActivePanelMode == INT_MODE_INCONT) {
        if (!SelectedEntities.empty()) {
            vector<refcount_ptr<ItemView>> inner_items = GetEntityInnerItems(SelectedEntities.front().as_ptr());

            ImGui::Text("Container items: %d", numeric_cast<int32_t>(inner_items.size()));

            if (ImGui::BeginChild("##ContainerItems", {0.0f, -ImGui::GetFrameHeightWithSpacing() * 2.0f}, true)) {
                for (size_t i = 0; i < inner_items.size(); i++) {
                    auto inner_item = inner_items[i].as_ptr();
                    auto label = strex("{} x{}", inner_item->GetName(), inner_item->GetCount());
                    const auto selected_item = InContItem.as_nptr();
                    const auto selected = selected_item == inner_item.as_nptr();

                    if (ImGui::Selectable(label.c_str(), selected)) {
                        InContItem = inner_items[i];
                        InspectorVisible = true;
                    }
                }
            }
            ImGui::EndChild();

            if (InContItem && ImGui::Button("Remove item")) {
                auto in_cont_item = InContItem.as_ptr();

                if (in_cont_item->GetOwnership() == ItemOwnership::CritterInventory) {
                    if (_curMap) {
                        auto cur_map = GetCurMapPtr();

                        if (nptr<CritterHexView> nullable_owner = cur_map->GetCritter(in_cont_item->GetCritterId())) {
                            auto owner = nullable_owner.as_ptr();
                            owner->DeleteInvItem(in_cont_item);
                        }
                    }
                }
                else if (in_cont_item->GetOwnership() == ItemOwnership::ItemContainer) {
                    if (_curMap) {
                        auto cur_map = GetCurMapPtr();

                        if (nptr<ItemHexView> nullable_owner = cur_map->GetItem(in_cont_item->GetContainerId())) {
                            auto owner = nullable_owner.as_ptr();
                            owner->DestroyInnerItem(in_cont_item);
                        }
                    }
                }

                InContItem.reset();

                auto nullable_next_entity = SelectedEntities.empty() ? nullptr : SelectedEntities.front().as_nptr();
                if (nullable_next_entity) {
                    SelectClear();
                    auto next_entity = nullable_next_entity.as_ptr();
                    SelectAdd(next_entity);
                }
            }

            if (InContItem && !SelectedEntities.empty()) {
                if (refcount_nptr<CritterHexView> nullable_cr = SelectedEntities.front().dyn_cast<CritterHexView>()) {
                    auto cr = nullable_cr.as_ptr();
                    auto in_cont_item = InContItem.as_ptr();
                    ImGui::SameLine();

                    if (ImGui::Button("Next slot")) {
                        size_t to_slot = static_cast<size_t>(in_cont_item->GetCritterSlot()) + 1;

                        while (numeric_cast<size_t>(to_slot) >= Settings->CritterSlotEnabled.size() || !Settings->CritterSlotEnabled[to_slot % 256]) {
                            to_slot++;
                        }

                        to_slot %= 256;

                        span<refcount_ptr<ItemView>> inv_items = cr->GetInvItems();

                        for (size_t i = 0; i < inv_items.size(); i++) {
                            auto item = inv_items[i].as_ptr();

                            if (item->GetCritterSlot() == static_cast<CritterItemSlot>(to_slot)) {
                                item->SetCritterSlot(CritterItemSlot::Inventory);
                            }
                        }

                        in_cont_item->SetCritterSlot(static_cast<CritterItemSlot>(to_slot));
                        cr->RefreshView();
                    }
                }
            }
        }
        else {
            ImGui::TextDisabled("Select an entity with inventory or container items.");
        }
    }
    else if (ActivePanelMode == INT_MODE_LIST) {
        if (_curMap && (resize_w <= 0 || resize_h <= 0)) {
            auto cur_map = GetCurMapPtr();
            const msize map_size = cur_map->GetSize();
            resize_w = map_size.width;
            resize_h = map_size.height;
        }

        ImGui::InputTextWithHint("##MapFilter", "Filter maps...", map_filter_buf.data(), map_filter_buf.size());
        const auto map_filter = InputBufferView(map_filter_buf);

        if (ImGui::BeginChild("##LoadedMaps", {0.0f, 180.0f}, true)) {
            for (auto& map : LoadedMaps) {
                auto label = string(map->GetName());

                if (!ContainsCaseInsensitive(label, map_filter)) {
                    continue;
                }

                auto loaded_map = map.as_nptr();
                const bool is_current = GetCurMap() == loaded_map;

                if (is_current) {
                    label = strex("* {}", label);
                }

                if (ImGui::Selectable(label.c_str(), is_current) && !is_current) {
                    ShowMap(map.as_ptr());
                }
            }
        }
        ImGui::EndChild();

        ImGui::SeparatorText("Map commands");
        ImGui::InputText("Map name", map_name_buf.data(), map_name_buf.size());

        run_button_action(ImGui::Button("New map"), [&] { ParseCommand("* new"); });
        ImGui::SameLine();
        run_button_action(ImGui::Button("Unload current") && _curMap, [&] { ParseCommand("* unload"); });
        ImGui::SameLine();
        run_button_action(ImGui::Button("Resave all"), [&] { ParseCommand("* resave"); });

        if (ImGui::Button("Load")) {
            const auto map_name = get_map_name_input();
            if (!map_name.empty()) {
                if (nptr<MapView> nullable_map = LoadMap(map_name); nullable_map) {
                    auto map = nullable_map.as_ptr();
                    ShowMap(map);
                    AddMess("Load map success");
                }
                else {
                    AddMess("Load map failed");
                }
            }
        }
        ImGui::SameLine();
        run_button_action_with_message(
            ImGui::Button("Save current") && _curMap,
            [&] {
                auto cur_map = GetCurMapPtr();
                SaveMap(cur_map, "");
            },
            "Save map success");
        ImGui::SameLine();
        if (ImGui::Button("Save As") && _curMap) {
            const auto map_name = get_map_name_input();
            if (!map_name.empty()) {
                auto cur_map = GetCurMapPtr();
                SaveMap(cur_map, map_name);
                AddMess("Save map success");
            }
        }

        ImGui::InputInt("Resize width", &resize_w);
        ImGui::InputInt("Resize height", &resize_h);

        if (ImGui::Button("Resize current") && _curMap) {
            auto cur_map = GetCurMapPtr();
            ResizeMap(cur_map, resize_w, resize_h);
            AddMess(strex("Resize map to {}x{}", resize_w, resize_h));
        }
    }
    else if (ActivePanelMode == INT_MODE_MESS) {
        if (ImGui::Button("Clear")) {
            MessBox.clear();
            MessBoxCurText.clear();
            MessBoxScroll = 0;
        }

        ImGui::SameLine();
        if (ImGui::Button("Copy all")) {
            string all_messages;
            for (const auto& entry : MessBox) {
                all_messages += entry.Time;
                all_messages += entry.Mess;
            }

            GetApp()->Input.SetClipboardText(all_messages);
        }

        ImGui::Separator();

        if (ImGui::BeginChild("##Messages", {0.0f, 0.0f}, true)) {
            for (auto it = MessBox.rbegin(); it != MessBox.rend(); ++it) {
                ImGui::TextUnformatted(strex("{}{}", it->Time, it->Mess).c_str());
            }
        }
        ImGui::EndChild();
    }
    else {
        ImGui::TextDisabled("No content window for the current mode.");
    }

    ImGui::End();
}

void MapperEngine::DrawCritterAnimationsWindowImGui()
{
    FO_STACK_TRACE_ENTRY();

    if (!CritterAnimationsWindowVisible) {
        return;
    }

    ImGui::SetNextWindowPos(
        {
            numeric_cast<float32_t>(MainPanelPos.x + MainPanelContentRect.x + MainPanelContentRect.width + 24),
            numeric_cast<float32_t>(MainPanelPos.y + MainPanelContentRect.y + 132),
        },
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({360.0f, 160.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Critter Animations", &CritterAnimationsWindowVisible, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    int32_t& anim_state = CritterAnimState;
    int32_t& anim_action = CritterAnimAction;
    auto& anim_sequence_buf = CritterAnimSequenceBuf;

    ImGui::InputInt("State", &anim_state);
    ImGui::InputInt("Action", &anim_action);
    if (ImGui::Button("Play pair")) {
        ParseCommand(strex("@ {} {}", anim_state, anim_action));
    }

    ImGui::InputTextWithHint("##AnimSequence", "Sequence: state action [state action]...", anim_sequence_buf.data(), anim_sequence_buf.size());
    if (ImGui::Button("Play sequence")) {
        const auto sequence = strvex(InputBufferView(anim_sequence_buf)).trim().str();
        if (!sequence.empty()) {
            ParseCommand(strex("@ {}", sequence));
        }
    }

    ImGui::End();
}

void MapperEngine::DrawScriptCallWindowImGui()
{
    FO_STACK_TRACE_ENTRY();

    if (!ScriptCallWindowVisible) {
        return;
    }

    ImGui::SetNextWindowPos(
        {
            numeric_cast<float32_t>(MainPanelPos.x + MainPanelContentRect.x + MainPanelContentRect.width + 24),
            numeric_cast<float32_t>(MainPanelPos.y + MainPanelContentRect.y + 304),
        },
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({380.0f, 140.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Script Call", &ScriptCallWindowVisible, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    auto& script_func_buf = ScriptCallFuncBuf;
    auto& script_args_buf = ScriptCallArgsBuf;

    ImGui::InputTextWithHint("##ScriptFunc", "Function name", script_func_buf.data(), script_func_buf.size());
    ImGui::InputTextWithHint("##ScriptArgs", "Arguments", script_args_buf.data(), script_args_buf.size());

    if (ImGui::Button("Run script")) {
        const auto func_name = strvex(InputBufferView(script_func_buf)).trim().str();
        const auto args = strvex(InputBufferView(script_args_buf)).trim().str();

        if (!func_name.empty()) {
            if (!args.empty()) {
                ParseCommand(strex("#{} {}", func_name, args));
            }
            else {
                ParseCommand(strex("#{}", func_name));
            }
        }
    }

    ImGui::End();
}

void MapperEngine::DrawMapListWindowImGui()
{
    FO_STACK_TRACE_ENTRY();

    if (!MapListWindowVisible) {
        return;
    }

    ptr<const ImGuiViewport> viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        {
            viewport->GetCenter().x,
            viewport->GetCenter().y,
        },
        ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({380.0f, 420.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({380.0f, 420.0f}, {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()});

    if (!ImGui::Begin("Map Browser", &MapListWindowVisible, 0)) {
        ImGui::End();
        return;
    }

    auto& map_filter_buf = MapBrowserFilterBuf;
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::InputTextWithHint("##AllMapsFilter", "Search maps...", map_filter_buf.data(), map_filter_buf.size());
    const auto map_filter = InputBufferView(map_filter_buf);

    vector<string> map_names;
    const auto map_files = MapsFileSys.FilterFiles("fomap");
    map_names.reserve(map_files.GetFilesCount());

    for (const auto& map_file : map_files) {
        const auto map_name = map_file.GetNameNoExt();

        if (!ContainsCaseInsensitive(map_name, map_filter)) {
            continue;
        }

        map_names.emplace_back(map_name);
    }

    std::ranges::sort(map_names);

    ImGui::Text("Maps: %d", numeric_cast<int32_t>(map_names.size()));
    ImGui::Separator();

    if (ImGui::BeginChild("##AllMapsList", {0.0f, 0.0f}, true)) {
        for (const auto& map_name : map_names) {
            const auto loaded_it = std::ranges::find_if(LoadedMaps, [&](const auto& map) { return string(map->GetName()) == map_name; });
            const auto is_loaded = loaded_it != LoadedMaps.end();
            const nptr<MapView> loaded_map = is_loaded ? loaded_it->as_nptr() : nullptr;
            const auto is_current = is_loaded && GetCurMap() == loaded_map;

            auto label = map_name;
            if (is_current) {
                label = strex("* {}", label).str();
            }
            else if (is_loaded) {
                label = strex("+ {}", label).str();
            }

            if (ImGui::Selectable(label.c_str(), is_current)) {
                if (is_loaded) {
                    ShowMap(loaded_it->as_ptr());
                }
                else if (nptr<MapView> nullable_map = LoadMap(map_name); nullable_map) {
                    auto map = nullable_map.as_ptr();
                    ShowMap(map);
                    AddMess(strex("Load map success: {}", map_name));
                }
                else {
                    AddMess(strex("Load map failed: {}", map_name));
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

void MapperEngine::DrawMapWindowImGui()
{
    FO_STACK_TRACE_ENTRY();

    if (!MapWindowVisible || !_curMap) {
        return;
    }

    ptr<const ImGuiViewport> viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        {
            viewport->WorkPos.x + viewport->WorkSize.x - 16.0f,
            viewport->WorkPos.y + MainPanelWindowRect.height + 8.0f,
        },
        ImGuiCond_FirstUseEver, {1.0f, 0.0f});

    if (!ImGui::Begin("Controls", &MapWindowVisible, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    auto cur_map = GetCurMapPtr();
    mpos hex;
    cur_map->GetHexAtScreen(MousePos, hex, nullptr);
    const int32_t day_time = GetGlobalDayTime();
    const string map_name = string(cur_map->GetName());
    const auto rotate_selected_critters = [&] {
        for (size_t i = 0; i < SelectedEntities.size(); i++) {
            auto entity = SelectedEntities[i].as_ptr();

            if (nptr<CritterHexView> cr = entity.dyn_cast<CritterHexView>()) {
                AdvanceCritterDir(cr.as_ptr());
            }
        }
    };

    if (ImGui::BeginTable("##ControlsSummary", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        const auto draw_summary_row = [](string_view label, auto&& draw_value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGuiTextUnformatted(label);
            ImGui::TableNextColumn();
            draw_value();
        };

        draw_summary_row("Map", [&] { ImGui::TextUnformatted(map_name.c_str()); });
        draw_summary_row("Mouse hex", [&] { ImGui::Text("%d, %d", hex.x, hex.y); });
        draw_summary_row("Time", [&] { ImGui::Text("%02d:%02d", day_time / 60 % 24, day_time % 60); });
        draw_summary_row("FPS", [&] { ImGui::Text("%d", GameTime.GetFramesPerSecond()); });
        draw_summary_row("Tile layer", [&] { ImGui::Text("%d", TileLayer); });

        ImGui::EndTable();
    }
    ImGui::SeparatorText("Map");

    if (ImGui::Button("Time +1h")) {
        SetGlobalDayTime(ShiftDayTimeWithWrap(day_time, 60));
    }

    ImGui::SameLine();
    if (ImGui::Button("Time -1h")) {
        SetGlobalDayTime(ShiftDayTimeWithWrap(day_time, -60));
    }

    ImGui::SameLine();
    if (ImGui::Button("Toggle hex")) {
        ToggleMapperHexOverlay();
    }

    const float32_t current_zoom = cur_map->GetSpritesZoomTarget();
    ImGui::Text("Zoom: %.2f", current_zoom);
    if (ImGui::Button("Zoom in")) {
        ChangeZoom(ScaleZoomValue(current_zoom, 1.2f));
    }
    ImGui::SameLine();
    if (ImGui::Button("Zoom out")) {
        ChangeZoom(ScaleZoomValue(current_zoom, 0.8f));
    }
    ImGui::SameLine();
    if (ImGui::Button("Zoom reset")) {
        ChangeZoom(1.0f);
    }

    auto tile_layer = TileLayer;
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::SliderInt("##TileLayer", &tile_layer, 0, 4)) {
        TileLayer = tile_layer;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Tile layer");

    bool scroll_check_enabled = cur_map->IsScrollCheck();

    if (ImGui::Checkbox("Scroll check", &scroll_check_enabled)) {
        cur_map->SetScrollCheck(scroll_check_enabled);
    }

    ImGui::Checkbox("Roof preview", &PreviewRoofTiles);

    const auto rotate_preview_dir_label = strex("Rotate preview dir ({})", CritterDir).str();
    if (ImGui::Button(rotate_preview_dir_label.c_str())) {
        CritterDir = GetNextCritterDir(CritterDir);
    }

    if (ImGui::Button("Rotate selected critters")) {
        rotate_selected_critters();
    }

    ImGui::Checkbox("Axial grid selection", &SelectAxialGrid);
    ImGui::Checkbox("Select entire entity", &SelectEntireEntity);

    const auto visibility_before = array {
        Settings->ShowItem,
        Settings->ShowScen,
        Settings->ShowWall,
        Settings->ShowCrit,
        Settings->ShowTile,
        Settings->ShowRoof,
        Settings->ShowFast,
    };

    const auto draw_checkbox_group = [](auto&& entries) {
        for (const auto& [label, value] : entries) {
            ImGui::Checkbox(label, value);
        }
    };

    if (ImGui::CollapsingHeader("Visibility")) {
        draw_checkbox_group(array {
            std::pair {"Items", &Settings->ShowItem},
            std::pair {"Scenery", &Settings->ShowScen},
            std::pair {"Walls", &Settings->ShowWall},
            std::pair {"Critters", &Settings->ShowCrit},
            std::pair {"Tiles", &Settings->ShowTile},
            std::pair {"Roof", &Settings->ShowRoof},
            std::pair {"Fast", &Settings->ShowFast},
        });
    }

    const auto visibility_after = array {
        Settings->ShowItem,
        Settings->ShowScen,
        Settings->ShowWall,
        Settings->ShowCrit,
        Settings->ShowTile,
        Settings->ShowRoof,
        Settings->ShowFast,
    };

    const auto visibility_changed = visibility_before != visibility_after;

    if (visibility_changed) {
        cur_map->RebuildMap();
    }

    if (ImGui::CollapsingHeader("Selection")) {
        draw_checkbox_group(array {
            std::pair {"Select items", &SelectItemsEnabled},
            std::pair {"Select scenery", &SelectSceneryEnabled},
            std::pair {"Select walls", &SelectWallsEnabled},
            std::pair {"Select critters", &SelectCrittersEnabled},
            std::pair {"Select tiles", &SelectTilesEnabled},
            std::pair {"Select roof", &SelectRoofTilesEnabled},
        });
    }

    ImGui::End();
}

void MapperEngine::DrawInspectorImGui()
{
    FO_STACK_TRACE_ENTRY();

    if (!InspectorVisible) {
        return;
    }

    auto nullable_entity = GetInspectorEntity();
    if (!nullable_entity) {
        return;
    }

    auto item = nullable_entity.dyn_cast<ItemView>();
    auto cr = nullable_entity.dyn_cast<CritterView>();
    auto entity_with_proto = nullable_entity.dyn_cast<EntityWithProto>();
    FO_VERIFY_AND_THROW(entity_with_proto, "Inspected entity does not have an associated prototype");

    const string_view type_name = cr ? "Critter" : (item && !item->GetStatic() ? "Dynamic Item" : "Static Item");

    ImGui::SetNextWindowPos({numeric_cast<float32_t>(InspectorPos.x), numeric_cast<float32_t>(InspectorPos.y)}, ImGuiCond_Once);
    ImGui::SetNextWindowSize({380.0f, 520.0f}, ImGuiCond_Once);

    auto keep_open = InspectorVisible;
    const auto flags = ImGuiWindowFlags_AlwaysAutoResize;
    if (!ImGui::Begin("Inspector", &keep_open, flags)) {
        ImGui::End();
        if (!keep_open) {
            SelectClear();
        }
        return;
    }

    const auto pos = ImGui::GetWindowPos();
    InspectorPos = {iround<int32_t>(pos.x), iround<int32_t>(pos.y)};

    auto compatible_candidates = 0;
    const auto is_same_inspector_entity_type = [&](ptr<const ClientEntity> selected_entity) {
        const auto selected_cr = selected_entity.dyn_cast<CritterView>();
        const auto selected_item = selected_entity.dyn_cast<ItemView>();
        const auto same_cr = selected_cr && cr;
        const auto same_item = selected_item && item;
        return same_cr || same_item;
    };
    const auto is_inspector_front_entity = [&]() -> bool {
        if (SelectedEntities.empty()) {
            return false;
        }

        auto front_entity = SelectedEntities.front().as_nptr();
        nptr<const ClientEntity> inspected_entity = nullable_entity;
        return front_entity == inspected_entity;
    };

    if (SelectedEntities.size() > 1 && is_inspector_front_entity()) {
        for (size_t i = 0; i < SelectedEntities.size(); i++) {
            auto selected_entity = SelectedEntities[i].as_ptr();

            if (is_same_inspector_entity_type(selected_entity)) {
                compatible_candidates++;
            }
        }
    }

    const auto can_apply_to_all = compatible_candidates > 1;
    if (!can_apply_to_all) {
        InspectorApplyToAll = false;
    }

    if (can_apply_to_all) {
        const auto apply_to_all_label = strex("Apply to all ({})", compatible_candidates).str();
        ImGui::Checkbox(apply_to_all_label.c_str(), &InspectorApplyToAll);
    }

    constexpr int32_t START_LINE = 3;
    string& edit_buf = InspectorEditBuf;
    int32_t& edit_line = InspectorEditLine;
    int32_t& pending_focus_line = InspectorPendingFocusLine;
    int32_t& pending_focus_array_index = InspectorPendingFocusArrayIndex;
    int32_t& pending_caret_reset_line = InspectorPendingCaretResetLine;
    int32_t& pending_caret_reset_array_index = InspectorPendingCaretResetArrayIndex;
    int32_t& pending_caret_reset_frames = InspectorPendingCaretResetFrames;
    bool& last_edit_cell_rect_valid = InspectorLastEditCellRectValid;
    float& last_edit_cell_min_x = InspectorLastEditCellMinX;
    float& last_edit_cell_min_y = InspectorLastEditCellMinY;
    float& last_edit_cell_max_x = InspectorLastEditCellMaxX;
    float& last_edit_cell_max_y = InspectorLastEditCellMaxY;
    const auto left_column_bg = ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 1.0f, 0.10f});
    const auto readonly_value_bg = ImGui::ColorConvertFloat4ToU32({0.72f, 0.78f, 0.88f, 0.22f});
    const auto same_as_proto_bg = ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 1.0f, 0.10f});
    const auto changed_from_proto_bg = ImGui::ColorConvertFloat4ToU32({0.55f, 0.55f, 0.55f, 0.22f});

    const auto sync_edit_buf = [&]() {
        edit_buf = InspectorSelectedLineValue;
        if (edit_buf.capacity() < 4096) {
            edit_buf.reserve(4096);
        }
    };

    const auto clear_edit_state = [&]() { ResetInspectorPropertyEditState(); };

    const auto begin_edit_state = [&](int32_t line, int32_t array_index = 0) {
        edit_line = line;
        sync_edit_buf();
        pending_focus_line = line;
        pending_focus_array_index = array_index;
        pending_caret_reset_line = line;
        pending_caret_reset_array_index = array_index;
        pending_caret_reset_frames = 2;
    };

    const auto reset_selected_line_state = [&](int32_t selected_line) {
        SelectInspectorPropertyLine(selected_line);
        sync_edit_buf();
        clear_edit_state();
    };

    const auto keep_selected_line_edit_state = [&](int32_t selected_line) {
        SelectInspectorPropertyLine(selected_line);
        sync_edit_buf();
        edit_line = selected_line;
    };

    const auto restore_selected_line_initial_value = [&] { CancelInspectorPropertyEdit(); };

    const auto apply_to_compatible_selected_entities = [&](auto&& action) {
        if (!(InspectorApplyToAll && can_apply_to_all && is_inspector_front_entity())) {
            return;
        }

        for (size_t j = 1; j < SelectedEntities.size(); j++) {
            auto selected_entity = SelectedEntities[j].as_ptr();
            if (is_same_inspector_entity_type(selected_entity)) {
                action(selected_entity);
            }
        }
    };

    if (edit_line != -1 && last_edit_cell_rect_valid && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const auto mouse_pos = ImGui::GetMousePos();
        const auto click_inside_edit_cell = mouse_pos.x >= last_edit_cell_min_x && mouse_pos.x <= last_edit_cell_max_x && mouse_pos.y >= last_edit_cell_min_y && mouse_pos.y <= last_edit_cell_max_y;

        if (!click_inside_edit_cell) {
            CancelInspectorPropertyEdit();
        }
    }

    ImGui::Separator();

    // Preserve inspector keyboard navigation when the window has focus.
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive()) {
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
            reset_selected_line_state(InspectorSelectedLine - 1);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
            reset_selected_line_state(InspectorSelectedLine + 1);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            restore_selected_line_initial_value();
        }
    }

    auto apply_value = [&](ptr<Entity> target_entity) { ApplyInspectorPropertyEdit(target_entity); };

    const auto apply_selected_value = [&](int32_t selected_line) {
        auto entity = nullable_entity.as_ptr();
        apply_value(entity);
        apply_to_compatible_selected_entities(apply_value);
        reset_selected_line_state(selected_line);
    };

    const auto apply_selected_value_keep_edit = [&](int32_t selected_line) {
        auto entity = nullable_entity.as_ptr();
        apply_value(entity);
        apply_to_compatible_selected_entities(apply_value);
        keep_selected_line_edit_state(selected_line);
    };

    const auto reset_selected_value = [&](int32_t selected_line) {
        if (selected_line < START_LINE || selected_line - START_LINE >= numeric_cast<int32_t>(ShowProps.size())) {
            return;
        }

        nptr<const Property> nullable_selected_prop = ShowProps[selected_line - START_LINE];
        if (!nullable_selected_prop) {
            return;
        }

        try {
            auto selected_prop = nullable_selected_prop.as_ptr();
            InspectorSelectedLineValue = entity_with_proto->GetProto()->GetProperties().SavePropertyToText(selected_prop);
            apply_value(nullable_entity.as_ptr());
        }
        catch (const std::exception&) {
        }

        reset_selected_line_state(selected_line);
    };

    if (ImGui::BeginChild("##InspectorProps", {0.0f, 0.0f}, false)) {
        if (ImGui::BeginTable("##InspectorGrid", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 170.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            auto focus_edit_line = pending_focus_line;
            const auto focus_edit_array_index = pending_focus_array_index >= 0 ? pending_focus_array_index : 0;
            const auto caret_reset_array_index = pending_caret_reset_array_index >= 0 ? pending_caret_reset_array_index : 0;

            const auto draw_summary_row = [&](string_view label, string_view value) {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeightWithSpacing() + 2.0f);
                ImGui::TableNextColumn();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, left_column_bg);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(string(label).c_str());
                ImGui::TableNextColumn();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, readonly_value_bg);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(string(value).c_str());
            };

            draw_summary_row("Type", type_name);
            draw_summary_row("Id", strex("{}", nullable_entity->GetId()).str());
            draw_summary_row("ProtoId", entity_with_proto->GetProtoId());

            for (size_t i = 0; i < ShowProps.size(); i++) {
                const auto line = START_LINE + numeric_cast<int32_t>(i);
                nptr<const Property> nullable_prop = ShowProps[i];

                ImGui::PushID(line);

                if (!nullable_prop) {
                    ImGui::TableNextRow(ImGuiTableRowFlags_None, 1.0f);
                    ImGui::TableNextColumn();
                    ImGui::Separator();
                    ImGui::TableNextColumn();
                    ImGui::Separator();
                    ImGui::PopID();
                    continue;
                }

                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeightWithSpacing() + 2.0f);

                auto prop = nullable_prop.as_ptr();
                auto value = nullable_entity->GetProperties().SavePropertyToText(prop);
                const auto is_const = prop->IsCoreProperty() && !prop->IsMutable();
                const auto selected = InspectorSelectedLine == line;
                auto entity = nullable_entity.as_ptr();
                const auto same_as_proto = IsInspectorValueSameAsProto(entity, prop, value);
                const auto label = strex("{} ({})", prop->GetName(), prop->GetViewTypeName()).str();

                ImGui::TableNextColumn();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, left_column_bg);
                if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                    reset_selected_line_state(line);
                }

                ImGui::TableNextColumn();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, is_const ? readonly_value_bg : (same_as_proto ? same_as_proto_bg : changed_from_proto_bg));
                const auto value_cell_start = ImGui::GetCursorScreenPos();
                const auto value_cell_width = ImGui::GetContentRegionAvail().x;
                const auto request_focus = focus_edit_line == line;
                const auto request_caret_reset = pending_caret_reset_line == line && pending_caret_reset_frames > 0;
                const auto select_value_line = [&] {
                    SelectInspectorPropertyLine(line);
                    if (!is_const) {
                        begin_edit_state(line);
                    }
                    else {
                        reset_selected_line_state(line);
                    }
                };

                if (selected && !is_const && edit_line == line) {
                    if (pending_focus_line == line) {
                        focus_edit_line = line;
                    }

                    auto struct_layout = GetInspectorStructLayout(prop);

                    auto commit_requested = false;
                    auto cancel_requested = false;
                    auto finish_edit_requested = false;
                    auto focus_consumed = false;
                    const auto edit_text_value = [&](float32_t item_width = -58.0f) {
                        ImGui::SetNextItemWidth(item_width);
                        if (request_focus) {
                            ImGui::SetKeyboardFocusHere();
                            focus_consumed = true;
                        }

                        const auto value_submitted = ImGuiInputTextString("##value", edit_buf, ImGuiInputTextFlags_EnterReturnsTrue, true, request_caret_reset);
                        const auto value_deactivated = ImGui::IsItemDeactivatedAfterEdit();
                        InspectorSelectedLineValue = edit_buf;
                        commit_requested = value_submitted || value_deactivated;
                        finish_edit_requested = value_submitted;
                        cancel_requested = ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape);
                    };

                    const auto edit_struct_fields = [&](vector<string>& field_values, int32_t struct_id, bool request_focus, bool request_caret_reset) {
                        auto struct_changed = false;

                        for (size_t field_index = 0; field_index < field_values.size(); field_index++) {
                            const auto& field = struct_layout->Fields[field_index];
                            ImGui::PushID(strex("{}:{}", struct_id, field.Name).str().c_str());

                            ImGui::AlignTextToFramePadding();
                            ImGui::TextUnformatted(field.Name.c_str());
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(-1.0f);

                            if (request_focus && field_index == 0) {
                                ImGui::SetKeyboardFocusHere();
                                focus_consumed = true;
                            }

                            if (field.Type.IsBool) {
                                auto current_value = false;

                                try {
                                    current_value = AnyData::ParseValue(field_values[field_index], false, false, AnyData::ValueType::Bool).AsBool();
                                }
                                catch (const std::exception&) {
                                }

                                if (ImGui::Checkbox("##field", &current_value)) {
                                    field_values[field_index] = AnyData::ValueToString(AnyData::Value {current_value});
                                    struct_changed = true;
                                    commit_requested = true;
                                }
                            }
                            else if (field.Type.IsInt) {
                                auto field_buf = field_values[field_index];
                                if (field_buf.capacity() < 512) {
                                    field_buf.reserve(512);
                                }

                                const auto value_submitted = ImGuiInputTextString("##field", field_buf, ImGuiInputTextFlags_EnterReturnsTrue, true, request_caret_reset && field_index == 0);
                                const auto value_deactivated = ImGui::IsItemDeactivatedAfterEdit();
                                field_values[field_index] = std::move(field_buf);
                                struct_changed |= value_submitted || value_deactivated;
                                commit_requested |= value_submitted || value_deactivated;
                                finish_edit_requested |= value_submitted;
                                cancel_requested |= ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape);
                            }
                            else if (field.Type.IsFloat) {
                                auto field_buf = field_values[field_index];
                                if (field_buf.capacity() < 512) {
                                    field_buf.reserve(512);
                                }

                                const auto value_submitted = ImGuiInputTextString("##field", field_buf, ImGuiInputTextFlags_EnterReturnsTrue, true, request_caret_reset && field_index == 0);
                                const auto value_deactivated = ImGui::IsItemDeactivatedAfterEdit();
                                field_values[field_index] = std::move(field_buf);
                                struct_changed |= value_submitted || value_deactivated;
                                commit_requested |= value_submitted || value_deactivated;
                                finish_edit_requested |= value_submitted;
                                cancel_requested |= ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape);
                            }
                            else {
                                auto field_buf = field_values[field_index];
                                if (field_buf.capacity() < 512) {
                                    field_buf.reserve(512);
                                }

                                const auto value_submitted = ImGuiInputTextString("##field", field_buf, ImGuiInputTextFlags_EnterReturnsTrue, true, request_caret_reset && field_index == 0);
                                const auto value_deactivated = ImGui::IsItemDeactivatedAfterEdit();
                                field_values[field_index] = std::move(field_buf);
                                struct_changed |= value_submitted || value_deactivated;
                                commit_requested |= value_submitted || value_deactivated;
                                finish_edit_requested |= value_submitted;
                                cancel_requested |= ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape);
                            }

                            ImGui::PopID();
                        }

                        return struct_changed;
                    };

                    if (prop->IsArray() && struct_layout) {
                        if (const auto parsed_entries = ParseInspectorStringEntries(InspectorSelectedLineValue); parsed_entries.has_value()) {
                            auto struct_entries = *parsed_entries;
                            auto array_changed = false;
                            std::optional<size_t> remove_index {};

                            for (size_t entry_index = 0; entry_index < struct_entries.size(); entry_index++) {
                                auto field_values = ParseInspectorStructFields(*struct_layout, struct_entries[entry_index]).value_or(vector<string>(struct_layout->Fields.size()));

                                ImGui::PushID(numeric_cast<int32_t>(entry_index));
                                ImGui::SeparatorText(strex("Item {}", entry_index).str().c_str());
                                array_changed |= edit_struct_fields(field_values, numeric_cast<int32_t>(entry_index), focus_edit_line == line && entry_index == numeric_cast<size_t>(focus_edit_array_index), pending_caret_reset_line == line && pending_caret_reset_frames > 0 && entry_index == numeric_cast<size_t>(caret_reset_array_index));
                                struct_entries[entry_index] = SerializeInspectorStringArray(field_values);
                                if (ImGui::SmallButton("Remove")) {
                                    remove_index = entry_index;
                                    commit_requested = true;
                                }
                                ImGui::PopID();
                            }

                            if (remove_index.has_value()) {
                                struct_entries.erase(struct_entries.begin() + numeric_cast<ptrdiff_t>(*remove_index));
                                array_changed = true;
                            }

                            if (ImGui::Button("Add element")) {
                                struct_entries.emplace_back(SerializeInspectorStringArray(vector<string>(struct_layout->Fields.size())));
                                array_changed = true;
                                pending_focus_line = line;
                                pending_focus_array_index = numeric_cast<int32_t>(struct_entries.size()) - 1;
                                pending_caret_reset_line = line;
                                pending_caret_reset_array_index = pending_focus_array_index;
                                pending_caret_reset_frames = 2;
                            }

                            if (array_changed) {
                                InspectorSelectedLineValue = SerializeInspectorStringArray(struct_entries);
                            }
                        }
                    }
                    else if (prop->IsArray()) {
                        if (const auto parsed_value = ParseInspectorValue(prop, InspectorSelectedLineValue); parsed_value.has_value() && parsed_value->Type() == AnyData::ValueType::Array) {
                            vector<AnyData::Value> entries;
                            entries.reserve(parsed_value->AsArray().Size());

                            for (const auto& entry : parsed_value->AsArray()) {
                                entries.emplace_back(entry.Copy());
                            }

                            auto array_changed = false;
                            std::optional<size_t> remove_index {};

                            for (size_t entry_index = 0; entry_index < entries.size(); entry_index++) {
                                ImGui::PushID(numeric_cast<int32_t>(entry_index));

                                if (focus_edit_line == line && entry_index == numeric_cast<size_t>(focus_edit_array_index)) {
                                    ImGui::SetKeyboardFocusHere();
                                    focus_consumed = true;
                                }

                                switch (GetInspectorValueType(prop)) {
                                case AnyData::ValueType::Int64: {
                                    auto value_buf = AnyData::ValueToString(entries[entry_index]);
                                    if (value_buf.capacity() < 512) {
                                        value_buf.reserve(512);
                                    }
                                    ImGui::SetNextItemWidth(-34.0f);
                                    const auto value_submitted = ImGuiInputTextString("##ArrayValue", value_buf, ImGuiInputTextFlags_EnterReturnsTrue, true, pending_caret_reset_line == line && pending_caret_reset_frames > 0 && entry_index == numeric_cast<size_t>(caret_reset_array_index));
                                    const auto value_deactivated = ImGui::IsItemDeactivatedAfterEdit();
                                    if (value_submitted || value_deactivated) {
                                        try {
                                            auto entry_value = AnyData::ParseValue(value_buf, false, false, AnyData::ValueType::Int64).AsInt64();
                                            if (!prop->IsBaseTypeSignedInt() && entry_value < 0) {
                                                entry_value = 0;
                                            }
                                            entries[entry_index] = AnyData::Value {entry_value};
                                            array_changed = true;
                                        }
                                        catch (const std::exception&) {
                                        }
                                    }
                                    commit_requested |= value_submitted || value_deactivated;
                                    finish_edit_requested |= value_submitted;
                                    cancel_requested |= ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape);
                                    break;
                                }
                                case AnyData::ValueType::Float64: {
                                    auto value_buf = AnyData::ValueToString(entries[entry_index]);
                                    if (value_buf.capacity() < 512) {
                                        value_buf.reserve(512);
                                    }
                                    ImGui::SetNextItemWidth(-34.0f);
                                    const auto value_submitted = ImGuiInputTextString("##ArrayValue", value_buf, ImGuiInputTextFlags_EnterReturnsTrue, true, pending_caret_reset_line == line && pending_caret_reset_frames > 0 && entry_index == numeric_cast<size_t>(caret_reset_array_index));
                                    const auto value_deactivated = ImGui::IsItemDeactivatedAfterEdit();
                                    if (value_submitted || value_deactivated) {
                                        try {
                                            entries[entry_index] = AnyData::Value {AnyData::ParseValue(value_buf, false, false, AnyData::ValueType::Float64).AsDouble()};
                                            array_changed = true;
                                        }
                                        catch (const std::exception&) {
                                        }
                                    }
                                    commit_requested |= value_submitted || value_deactivated;
                                    finish_edit_requested |= value_submitted;
                                    cancel_requested |= ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape);
                                    break;
                                }
                                case AnyData::ValueType::Bool: {
                                    auto entry_value = entries[entry_index].AsBool();
                                    if (ImGui::Checkbox("##ArrayValue", &entry_value)) {
                                        entries[entry_index] = AnyData::Value {entry_value};
                                        array_changed = true;
                                        commit_requested = true;
                                    }
                                    break;
                                }
                                case AnyData::ValueType::String: {
                                    auto string_buf = string(entries[entry_index].AsString());
                                    if (string_buf.capacity() < 512) {
                                        string_buf.reserve(512);
                                    }
                                    ImGui::SetNextItemWidth(-34.0f);
                                    const auto value_submitted = ImGuiInputTextString("##ArrayValue", string_buf, ImGuiInputTextFlags_EnterReturnsTrue, true, pending_caret_reset_line == line && pending_caret_reset_frames > 0 && entry_index == numeric_cast<size_t>(caret_reset_array_index));
                                    const auto value_deactivated = ImGui::IsItemDeactivatedAfterEdit();
                                    if (value_submitted || value_deactivated) {
                                        entries[entry_index] = AnyData::Value {std::move(string_buf)};
                                        array_changed = true;
                                    }
                                    commit_requested |= value_submitted || value_deactivated;
                                    finish_edit_requested |= value_submitted;
                                    cancel_requested |= ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape);
                                    break;
                                }
                                default:
                                    FO_UNREACHABLE_PLACE();
                                }

                                ImGui::SameLine();
                                if (ImGui::SmallButton("X")) {
                                    remove_index = entry_index;
                                    commit_requested = true;
                                }

                                ImGui::PopID();
                            }

                            if (remove_index.has_value()) {
                                entries.erase(entries.begin() + numeric_cast<ptrdiff_t>(*remove_index));
                                array_changed = true;
                            }

                            if (ImGui::Button("Add element")) {
                                entries.emplace_back(MakeDefaultInspectorArrayElement(prop));
                                array_changed = true;
                                pending_focus_line = line;
                                pending_focus_array_index = numeric_cast<int32_t>(entries.size()) - 1;
                                pending_caret_reset_line = line;
                                pending_caret_reset_array_index = pending_focus_array_index;
                                pending_caret_reset_frames = 2;
                            }

                            if (array_changed) {
                                InspectorSelectedLineValue = SerializeInspectorArray(std::move(entries));
                            }
                        }
                        else {
                            edit_text_value();
                        }
                    }
                    else if (struct_layout) {
                        auto field_values = ParseInspectorStructFields(*struct_layout, InspectorSelectedLineValue).value_or(vector<string>(struct_layout->Fields.size()));
                        if (edit_struct_fields(field_values, -1, focus_edit_line == line, pending_caret_reset_line == line && pending_caret_reset_frames > 0)) {
                            InspectorSelectedLineValue = SerializeInspectorStringArray(field_values);
                        }
                    }
                    else if (prop->IsBaseTypeBool()) {
                        auto current_value = false;
                        if (const auto parsed_value = ParseInspectorValue(prop, InspectorSelectedLineValue); parsed_value.has_value() && parsed_value->Type() == AnyData::ValueType::Bool) {
                            current_value = parsed_value->AsBool();
                        }

                        if (focus_edit_line == line) {
                            ImGui::SetKeyboardFocusHere();
                            focus_consumed = true;
                        }
                        if (ImGui::Checkbox("##value", &current_value)) {
                            InspectorSelectedLineValue = AnyData::ValueToString(AnyData::Value {current_value});
                            commit_requested = true;
                        }
                    }
                    else if (prop->IsBaseTypeInt() && !prop->IsBaseTypeSimpleStruct()) {
                        edit_text_value();
                    }
                    else if (prop->IsBaseTypeFloat() && !prop->IsBaseTypeSimpleStruct()) {
                        edit_text_value();
                    }
                    else {
                        edit_text_value();
                    }

                    cancel_requested |= ImGui::IsKeyPressed(ImGuiKey_Escape, false);

                    last_edit_cell_min_x = value_cell_start.x;
                    last_edit_cell_min_y = value_cell_start.y;
                    last_edit_cell_max_x = value_cell_start.x + value_cell_width;
                    last_edit_cell_max_y = std::max(ImGui::GetCursorScreenPos().y, value_cell_start.y + ImGui::GetFrameHeightWithSpacing());
                    last_edit_cell_rect_valid = true;

                    if (pending_caret_reset_line == line && pending_caret_reset_frames > 0) {
                        pending_caret_reset_frames--;
                        if (pending_caret_reset_frames == 0) {
                            pending_caret_reset_line = -1;
                            pending_caret_reset_array_index = -1;
                        }
                    }

                    if (cancel_requested) {
                        restore_selected_line_initial_value();
                    }
                    else if (commit_requested) {
                        if (finish_edit_requested) {
                            apply_selected_value(line);
                        }
                        else {
                            apply_selected_value_keep_edit(line);
                        }
                    }

                    if (focus_consumed && pending_focus_line == line) {
                        pending_focus_line = -1;
                        pending_focus_array_index = -1;
                    }
                }
                else {
                    if (selected) {
                        if (!is_const) {
                            const auto reset_label_width = ImGui::CalcTextSize("Reset").x + ImGui::GetStyle().FramePadding.x * 2.0f;
                            const auto reset_enabled = !same_as_proto;
                            const auto value_button_width = reset_enabled ? std::max(1.0f, ImGui::GetContentRegionAvail().x - reset_label_width - ImGui::GetStyle().ItemSpacing.x) : ImGui::GetContentRegionAvail().x;

                            if (ImGui::Selectable(strex("{}##value_display", value).str().c_str(), false, ImGuiSelectableFlags_AllowOverlap, {value_button_width, 0.0f})) {
                                begin_edit_state(line);
                            }

                            if (reset_enabled) {
                                ImGui::SameLine();
                                if (ImGui::SmallButton("Reset")) {
                                    reset_selected_value(line);
                                }
                            }
                        }
                        else {
                            ImGui::AlignTextToFramePadding();
                            ImGui::TextUnformatted(value.c_str());
                        }
                    }
                    else {
                        if (ImGui::Selectable(strex("{}##value_select", value).str().c_str(), false, ImGuiSelectableFlags_AllowOverlap, {ImGui::GetContentRegionAvail().x, 0.0f})) {
                            select_value_line();
                        }
                    }
                }

                ImGui::PopID();
            }

            if (edit_line == -1) {
                last_edit_cell_rect_valid = false;
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::End();

    if (!keep_open) {
        SelectClear();
    }
}

void MapperEngine::ApplyInspectorPropertyEdit(ptr<Entity> entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    constexpr auto start_line = 3;

    if (InspectorSelectedLine >= start_line && InspectorSelectedLine - start_line < numeric_cast<int32_t>(ShowProps.size())) {
        const auto& nullable_prop = ShowProps[InspectorSelectedLine - start_line];

        if (nullable_prop) {
            const auto entity_id = entity->GetId();
            const auto old_value = InspectorSelectedLineInitialValue;
            const auto new_value = InspectorSelectedLineValue;

            auto prop = nullable_prop.as_ptr();

            if (!ApplyEntityPropertyText(entity, prop, new_value)) {
                ApplyEntityPropertyText(entity, prop, old_value);
                return;
            }

            const auto prop_name = string(prop->GetName());
            PushUndoOp(GetCurMap(),
                UndoOp {strex("Edit {}", prop_name),
                    [entity_id, prop_name, old_value](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                        auto nullable_target = mapper->FindEntityById(*active_map, entity_id);
                        if (!nullable_target) {
                            return false;
                        }

                        auto nullable_apply_prop = nullable_target->GetProperties().GetRegistrator()->FindProperty(prop_name);
                        if (!nullable_apply_prop) {
                            return false;
                        }

                        auto target = nullable_target.as_ptr();
                        auto apply_prop = nullable_apply_prop.as_ptr();
                        return mapper->ApplyEntityPropertyText(target, apply_prop, old_value);
                    },
                    [entity_id, prop_name, new_value](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                        auto nullable_target = mapper->FindEntityById(*active_map, entity_id);
                        if (!nullable_target) {
                            return false;
                        }

                        auto nullable_apply_prop = nullable_target->GetProperties().GetRegistrator()->FindProperty(prop_name);
                        if (!nullable_apply_prop) {
                            return false;
                        }

                        auto target = nullable_target.as_ptr();
                        auto apply_prop = nullable_apply_prop.as_ptr();
                        return mapper->ApplyEntityPropertyText(target, apply_prop, new_value);
                    }});
        }
    }
}

void MapperEngine::SelectInspectorPropertyLine(int32_t line)
{
    FO_STACK_TRACE_ENTRY();

    constexpr auto start_line = 3;

    InspectorSelectedLine = line;
    InspectorSelectedLine = std::max(InspectorSelectedLine, 0);
    InspectorSelectedLineInitialValue = InspectorSelectedLineValue = "";

    if (nptr<const ClientEntity> entity = GetInspectorEntity(); entity) {
        if (InspectorSelectedLine - start_line >= numeric_cast<int32_t>(ShowProps.size())) {
            InspectorSelectedLine = numeric_cast<int32_t>(ShowProps.size()) + start_line - 1;
        }
        if (InspectorSelectedLine >= start_line && InspectorSelectedLine - start_line < numeric_cast<int32_t>(ShowProps.size()) && ShowProps[InspectorSelectedLine - start_line]) {
            auto selected_prop = ShowProps[InspectorSelectedLine - start_line].as_ptr();
            InspectorSelectedLineInitialValue = InspectorSelectedLineValue = entity->GetProperties().SavePropertyToText(selected_prop);
        }
    }
}

void MapperEngine::ResetInspectorPropertyEditState()
{
    FO_STACK_TRACE_ENTRY();

    InspectorEditLine = -1;
    InspectorPendingFocusLine = -1;
    InspectorPendingFocusArrayIndex = -1;
    InspectorPendingCaretResetLine = -1;
    InspectorPendingCaretResetArrayIndex = -1;
    InspectorPendingCaretResetFrames = 0;
    InspectorLastEditCellRectValid = false;
}

auto MapperEngine::CancelInspectorPropertyEdit() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (InspectorEditLine == -1) {
        return false;
    }

    InspectorSelectedLineValue = InspectorSelectedLineInitialValue;
    InspectorEditBuf = InspectorSelectedLineValue;
    if (InspectorEditBuf.capacity() < 4096) {
        InspectorEditBuf.reserve(4096);
    }

    ResetInspectorPropertyEditState();
    return true;
}

auto MapperEngine::GetInspectorEntity() -> nptr<ClientEntity>
{
    FO_STACK_TRACE_ENTRY();

    nptr<ClientEntity> entity = nullptr;

    if (ActivePanelMode == INT_MODE_INCONT && InContItem) {
        entity = InContItem.as_nptr();
    }
    else if (!SelectedEntities.empty()) {
        entity = SelectedEntities.front().as_nptr();
    }

    if (entity == InspectorEntity) {
        return entity;
    }

    InspectorEntity = entity;
    ShowProps.clear();

    if (entity) {
        vector<int32_t> prop_indices;
        OnInspectorProperties.Fire(entity.as_ptr(), prop_indices);

        for (const auto prop_index : prop_indices) {
            ShowProps.emplace_back(prop_index != -1 ? entity->GetProperties().GetRegistrator()->GetPropertyByIndex(prop_index) : nullptr);
        }
    }

    SelectInspectorPropertyLine(InspectorSelectedLine);
    return entity;
}

void MapperEngine::HandleLeftMouseDown()
{
    FO_STACK_TRACE_ENTRY();

    MouseHoldMode = INT_NONE;

    if (HandleMapLeftMouseDown()) {
        return;
    }
}

auto MapperEngine::HandleMapLeftMouseDown() -> bool
{
    FO_STACK_TRACE_ENTRY();

    InContItem.reset();

    if (!_curMap) {
        return true;
    }

    auto cur_map = GetCurMapPtr();

    if (!cur_map->GetHexAtScreen(MousePos, SelectHex1, nullptr)) {
        return true;
    }

    SelectHex2 = SelectHex1;
    SelectPos = MousePos;

    if (CurMode == CUR_MODE_PLACE_OBJECT) {
        return true;
    }

    auto entity = cur_map->GetEntityAtScreen(MousePos, 0, true);
    nptr<ClientEntity> clicked_entity = entity.first;
    const auto clicked_selected = clicked_entity && SelectedEntitiesSet.contains(clicked_entity.as_ptr());

    if (clicked_selected) {
        MouseHoldMode = INT_MOVE_SELECTION;
        SetCurMode(CUR_MODE_MOVE_SELECTION);
        return true;
    }

    if (GetApp()->Input.IsShiftDown()) {
        for (size_t i = 0; i < SelectedEntities.size(); i++) {
            auto selected_entity = SelectedEntities[i].as_ptr();

            if (nptr<CritterHexView> nullable_cr = selected_entity.dyn_cast<CritterHexView>()) {
                auto cr = nullable_cr.as_ptr();
                auto hex = cr->GetHex();

                if (const auto find_path = cur_map->FindPath(nullptr, hex, SelectHex1, -1)) {
                    for (const auto dir : find_path->DirSteps) {
                        if (GeometryHelper::MoveHexByDir(hex, dir, cur_map->GetSize())) {
                            cur_map->MoveCritter(cr, hex, true);
                        }
                        else {
                            break;
                        }
                    }
                }

                break;
            }
        }
    }
    else if (!GetApp()->Input.IsCtrlDown()) {
        SelectClear();
    }

    MouseHoldMode = INT_SELECT;

    return true;
}

void MapperEngine::HandleLeftMouseUp()
{
    FO_STACK_TRACE_ENTRY();

    if (CurMode == CUR_MODE_PLACE_OBJECT) {
        auto cur_map = GetCurMapPtr();

        if (cur_map->GetHexAtScreen(MousePos, SelectHex2, nullptr)) {
            if (IsItemMode() && ActiveItemProtos && !ActiveItemProtos->empty()) {
                CreateItem((*ActiveItemProtos)[GetActiveProtoIndex()]->GetProtoId(), SelectHex2, nullptr);
            }
            else if (IsCritMode() && ActiveCritterProtos && !ActiveCritterProtos->empty()) {
                CreateCritter((*ActiveCritterProtos)[GetActiveProtoIndex()]->GetProtoId(), SelectHex2);
            }

            SetCurMode(CUR_MODE_DEFAULT);
        }

        MouseHoldMode = INT_NONE;
        return;
    }

    if (MouseHoldMode == INT_MOVE_SELECTION) {
        CommitPendingSelectionMove();
        MouseHoldMode = INT_NONE;
        SetCurMode(CUR_MODE_DEFAULT);
        return;
    }

    if (MouseHoldMode == INT_SELECT) {
        auto cur_map = GetCurMapPtr();

        if (cur_map->GetHexAtScreen(MousePos, SelectHex2, nullptr)) {
            if (SelectHex1 != SelectHex2) {
                ClearMapperTrackOverlay();

                vector<mpos> hexes;

                if (SelectAxialGrid) {
                    hexes = GeometryHelper::GetAxialHexes(SelectHex1, SelectHex2, cur_map->GetSize());
                }
                else {
                    const msize map_size = cur_map->GetSize();
                    const int32_t fx = std::min(SelectHex1.x, SelectHex2.x);
                    const int32_t tx = std::max(SelectHex1.x, SelectHex2.x);
                    const int32_t fy = std::min(SelectHex1.y, SelectHex2.y);
                    const int32_t ty = std::max(SelectHex1.y, SelectHex2.y);

                    for (int32_t i = fx; i <= tx; i++) {
                        for (int32_t j = fy; j <= ty; j++) {
                            hexes.emplace_back(map_size.from_raw_pos(i, j));
                        }
                    }
                }

                const auto check_item_to_add = [&](ptr<const ItemHexView> item) -> bool {
                    if (cur_map->IsIgnorePid(item->GetProtoId())) {
                        return false;
                    }
                    if (item->GetIsTile() && !item->GetIsRoofTile() && SelectTilesEnabled && Settings->ShowTile) {
                        return true;
                    }
                    else if (item->GetIsTile() && item->GetIsRoofTile() && SelectRoofTilesEnabled && Settings->ShowRoof) {
                        return true;
                    }
                    else if (!item->GetIsTile() && !item->GetIsScenery() && !item->GetIsWall() && SelectItemsEnabled && Settings->ShowItem) {
                        return true;
                    }
                    else if (!item->GetIsTile() && item->GetIsScenery() && SelectSceneryEnabled && Settings->ShowScen) {
                        return true;
                    }
                    else if (!item->GetIsTile() && item->GetIsWall() && SelectWallsEnabled && Settings->ShowWall) {
                        return true;
                    }
                    else if (Settings->ShowFast && cur_map->IsFastPid(item->GetProtoId())) {
                        return true;
                    }
                    else {
                        return false;
                    }
                };

                for (const auto hex : hexes) {
                    for (ptr<ItemHexView> hex_item : copy_hold_ref(cur_map->GetItemsOnHex(hex))) {
                        if (check_item_to_add(hex_item)) {
                            SelectAdd(hex_item, hex, true);
                        }
                    }
                    for (ptr<CritterHexView> hex_cr : copy_hold_ref(cur_map->GetCrittersOnHex(hex, CritterFindType::Any))) {
                        if (SelectCrittersEnabled && Settings->ShowCrit) {
                            SelectAdd(hex_cr, hex);
                        }
                    }
                }

                cur_map->DefferedRefreshItems();
            }
            else {
                auto entity = cur_map->GetEntityAtScreen(MousePos, 0, true);

                if (nptr<ItemHexView> nullable_item = entity.first.dyn_cast<ItemHexView>()) {
                    auto item = nullable_item.as_ptr();
                    if (!item->GetIsTile()) {
                        SelectAdd(item, entity.second->GetHex());
                    }
                }
                else if (nptr<CritterHexView> nullable_cr = entity.first.dyn_cast<CritterHexView>()) {
                    auto cr = nullable_cr.as_ptr();
                    SelectAdd(cr, entity.second->GetHex());
                }
            }

            if (!SelectedEntities.empty() && !GetEntityInnerItems(SelectedEntities.front().as_ptr()).empty()) {
                SetActivePanelMode(INT_MODE_INCONT);
            }

            cur_map->RebuildMap();
        }
    }

    MouseHoldMode = INT_NONE;
    SetCurMode(CUR_MODE_DEFAULT);
}

void MapperEngine::HandleSelectionMouseDrag()
{
    FO_STACK_TRACE_ENTRY();

    if (IsImGuiMouseCaptured() || (MouseHoldMode != INT_SELECT && MouseHoldMode != INT_MOVE_SELECTION)) {
        return;
    }

    auto cur_map = GetCurMapPtr();

    const bool had_track_overlay = !MapperTrackOverlayHexes.empty();
    ClearMapperTrackOverlay();

    if (!cur_map->GetHexAtScreen(MousePos, SelectHex2, nullptr)) {
        if (!SelectHex2.is_zero() || had_track_overlay) {
            cur_map->RebuildMap();
            SelectHex2 = {};
        }

        return;
    }

    if (MouseHoldMode == INT_SELECT) {
        if (SelectHex1 != SelectHex2) {
            if (SelectAxialGrid) {
                for (const auto hex : GeometryHelper::GetAxialHexes(SelectHex1, SelectHex2, cur_map->GetSize())) {
                    AddMapperTrackOverlayHex(hex, 1);
                }
            }
            else {
                const msize map_size = cur_map->GetSize();
                const int32_t fx = std::min(SelectHex1.x, SelectHex2.x);
                const int32_t tx = std::max(SelectHex1.x, SelectHex2.x);
                const int32_t fy = std::min(SelectHex1.y, SelectHex2.y);
                const int32_t ty = std::max(SelectHex1.y, SelectHex2.y);

                for (auto i = fx; i <= tx; i++) {
                    for (auto j = fy; j <= ty; j++) {
                        AddMapperTrackOverlayHex(map_size.from_raw_pos(i, j), 1);
                    }
                }
            }

            cur_map->RebuildMap();
        }
        else if (had_track_overlay) {
            cur_map->RebuildMap();
        }
    }
    else if (MouseHoldMode == INT_MOVE_SELECTION) {
        auto offs_hx = numeric_cast<int32_t>(SelectHex2.x) - numeric_cast<int32_t>(SelectHex1.x);
        auto offs_hy = numeric_cast<int32_t>(SelectHex2.y) - numeric_cast<int32_t>(SelectHex1.y);
        auto offs_x = MousePos.x - SelectPos.x;
        auto offs_y = MousePos.y - SelectPos.y;

        if (SelectMove(!GetApp()->Input.IsShiftDown(), offs_hx, offs_hy, offs_x, offs_y)) {
            SelectHex1 = cur_map->GetSize().from_raw_pos(SelectHex1.x + offs_hx, SelectHex1.y + offs_hy);
            SelectPos.x += offs_x;
            SelectPos.y += offs_y;
            cur_map->RebuildMap();
        }
    }
}

void MapperEngine::SetMapperHexOverlayVisible(bool visible)
{
    FO_STACK_TRACE_ENTRY();

    if (MapperHexOverlayVisible == visible) {
        return;
    }

    MapperHexOverlayVisible = visible;

    if (_curMap != nullptr) {
        _curMap->RebuildMap();
    }
}

void MapperEngine::ToggleMapperHexOverlay()
{
    FO_STACK_TRACE_ENTRY();

    SetMapperHexOverlayVisible(!MapperHexOverlayVisible);
}

void MapperEngine::ClearMapperTrackOverlay()
{
    FO_STACK_TRACE_ENTRY();

    MapperTrackOverlayHexes.clear();
    MapperTrackOverlayKinds.clear();
}

void MapperEngine::AddMapperTrackOverlayHex(mpos hex, int32_t kind)
{
    FO_STACK_TRACE_ENTRY();

    if (_curMap != nullptr && !_curMap->GetSize().is_valid_pos(hex)) {
        return;
    }

    const int32_t normalized_kind = kind == 2 ? 2 : 1;

    MapperTrackOverlayHexes.emplace_back(hex);
    MapperTrackOverlayKinds.emplace_back(normalized_kind);
}

void MapperEngine::MarkBlockedHexes()
{
    FO_STACK_TRACE_ENTRY();

    ClearMapperTrackOverlay();

    if (!_curMap) {
        return;
    }

    const msize map_size = _curMap->GetSize();

    for (int32_t hx = 0; hx < map_size.width; hx++) {
        for (int32_t hy = 0; hy < map_size.height; hy++) {
            const mpos hex = map_size.from_raw_pos(hx, hy);
            const MapView::Field& field = _curMap->GetField(hex);
            int32_t kind = 0;

            if (field.MoveBlocked) {
                kind = 2;
            }
            if (field.ShootBlocked) {
                kind = 1;
            }
            if (kind != 0) {
                AddMapperTrackOverlayHex(hex, kind);
            }
        }
    }

    _curMap->RebuildMap();
}

auto MapperEngine::GetActiveSubTab() -> nptr<SubTab>
{
    FO_STACK_TRACE_ENTRY();

    if (ActivePanelMode < 0 || ActivePanelMode >= TAB_COUNT) {
        return nullptr;
    }

    return ActiveSubTabs[ActivePanelMode];
}

auto MapperEngine::GetActiveSubTab() const -> nptr<const SubTab>
{
    FO_STACK_TRACE_ENTRY();

    if (ActivePanelMode < 0 || ActivePanelMode >= TAB_COUNT) {
        return nullptr;
    }

    return ActiveSubTabs[ActivePanelMode];
}

auto MapperEngine::GetActiveProtoIndex() const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (nptr<const SubTab> nullable_active_subtab = GetActiveSubTab(); nullable_active_subtab) {
        auto active_subtab = nullable_active_subtab.as_ptr();
        return active_subtab->Index;
    }

    if (ActivePanelMode >= 0 && ActivePanelMode < INT_MODE_COUNT) {
        return TabIndex[ActivePanelMode];
    }

    return 0;
}

void MapperEngine::SetActiveProtoIndex(int32_t index)
{
    FO_STACK_TRACE_ENTRY();

    if (nptr<SubTab> nullable_active_subtab = GetActiveSubTab(); nullable_active_subtab) {
        auto active_subtab = nullable_active_subtab.as_ptr();
        active_subtab->Index = index;
    }

    if (ActivePanelMode >= 0 && ActivePanelMode < INT_MODE_COUNT) {
        TabIndex[ActivePanelMode] = index;
    }
}

void MapperEngine::RefreshActiveProtoLists()
{
    FO_STACK_TRACE_ENTRY();

    // Select protos and scroll
    ActiveItemProtos = nullptr;
    ActiveProtoScroll = nullptr;
    ActiveCritterProtos = nullptr;
    InContItem.reset();

    if (nptr<SubTab> nullable_stab = GetActiveSubTab(); nullable_stab) {
        auto stab = nullable_stab.as_ptr();

        if (!stab->CritterProtos.empty()) {
            ActiveCritterProtos = &stab->CritterProtos;
        }
        else {
            ActiveItemProtos = &stab->ItemProtos;
        }

        ActiveProtoScroll = &stab->Scroll;
    }

    if (_curMap) {
        auto cur_map = GetCurMapPtr();

        // Update fast pids
        cur_map->ClearFastPids();
        for (const auto& fast_proto : ActiveSubTabs[INT_MODE_FAST]->ItemProtos) {
            cur_map->AddFastPid(fast_proto->GetProtoId());
        }

        // Update ignore pids
        cur_map->ClearIgnorePids();

        for (const auto& ignore_proto : ActiveSubTabs[INT_MODE_IGNORE]->ItemProtos) {
            cur_map->AddIgnorePid(ignore_proto->GetProtoId());
        }

        // Refresh map
        cur_map->RebuildMap();
    }
}

void MapperEngine::SetActivePanelMode(int32_t mode)
{
    FO_STACK_TRACE_ENTRY();

    ActivePanelMode = mode;
    MouseHoldMode = INT_NONE;

    RefreshActiveProtoLists();
}

void MapperEngine::MoveEntity(ptr<ClientEntity> entity, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto cur_map = GetCurMapPtr();

    if (!cur_map->GetSize().is_valid_pos(hex)) {
        return;
    }

    mpos old_hex {};
    if (nptr<CritterHexView> nullable_cr = entity.dyn_cast<CritterHexView>()) {
        auto cr = nullable_cr.as_ptr();
        old_hex = cr->GetHex();
    }
    else if (nptr<ItemHexView> nullable_item = entity.dyn_cast<ItemHexView>()) {
        auto item = nullable_item.as_ptr();
        old_hex = item->GetHex();
    }

    if (old_hex == hex) {
        return;
    }

    const auto entity_id = entity->GetId();

    SelectClear();

    if (nptr<CritterHexView> nullable_cr = entity.dyn_cast<CritterHexView>()) {
        auto cr = nullable_cr.as_ptr();
        cur_map->MoveCritter(cr, hex, false);
    }
    else if (nptr<ItemHexView> nullable_item = entity.dyn_cast<ItemHexView>()) {
        auto item = nullable_item.as_ptr();
        cur_map->MoveItem(item, hex);
    }

    SetMapDirty(GetCurMap());

    PushUndoOp(GetCurMap(),
        UndoOp {"Move entity",
            [entity_id, old_hex](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                auto nullable_target = mapper->FindEntityById(*active_map, entity_id);
                if (!nullable_target) {
                    return false;
                }
                auto target = nullable_target.as_ptr();
                mapper->MoveEntity(target, old_hex);
                return true;
            },
            [entity_id, hex](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                auto nullable_target = mapper->FindEntityById(*active_map, entity_id);
                if (!nullable_target) {
                    return false;
                }
                auto target = nullable_target.as_ptr();
                mapper->MoveEntity(target, hex);
                return true;
            }});
}

void MapperEngine::DeleteEntity(ptr<ClientEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    auto nullable_map = GetCurMap();
    EntityBuf entity_buf;
    CaptureEntityBuf(entity_buf, entity);
    auto item_ownership = ItemOwnership::MapHex;
    ident_t owner_id {};

    if (nptr<ItemView> nullable_item = entity.dyn_cast<ItemView>()) {
        auto item = nullable_item.as_ptr();
        item_ownership = item->GetOwnership();
        if (item_ownership == ItemOwnership::CritterInventory) {
            owner_id = item->GetCritterId();
        }
        else if (item_ownership == ItemOwnership::ItemContainer) {
            owner_id = item->GetContainerId();
        }
    }

    SelectedEntitiesSet.erase(entity);

    vec_remove_unique_value(SelectedEntities, entity.hold_ref());

    auto cur_map = nullable_map.as_ptr();

    if (nptr<CritterHexView> nullable_cr = entity.dyn_cast<CritterHexView>()) {
        auto cr = nullable_cr.as_ptr();
        cur_map->DestroyCritter(cr);
    }
    else if (nptr<ItemHexView> nullable_item = entity.dyn_cast<ItemHexView>()) {
        auto item = nullable_item.as_ptr();
        cur_map->DestroyItem(item);
    }
    else if (nptr<ItemView> nullable_inner_item = entity.dyn_cast<ItemView>()) {
        auto inner_item = nullable_inner_item.as_ptr();
        if (inner_item->GetOwnership() == ItemOwnership::CritterInventory) {
            if (nptr<CritterHexView> nullable_owner = cur_map->GetCritter(inner_item->GetCritterId())) {
                auto owner = nullable_owner.as_ptr();
                owner->DeleteInvItem(inner_item);
            }
        }
        else if (inner_item->GetOwnership() == ItemOwnership::ItemContainer) {
            if (nptr<ItemHexView> nullable_owner = cur_map->GetItem(inner_item->GetContainerId())) {
                auto owner = nullable_owner.as_ptr();
                owner->DestroyInnerItem(inner_item);
            }
        }
    }

    SetMapDirty(GetCurMap());

    PushUndoOp(nullable_map,
        UndoOp {"Delete entity",
            [entity_buf, item_ownership, owner_id](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                nptr<ClientEntity> owner;

                if (item_ownership == ItemOwnership::CritterInventory || item_ownership == ItemOwnership::ItemContainer) {
                    owner = mapper->FindEntityById(*active_map, owner_id);
                    if (!owner) {
                        return false;
                    }
                }

                return !!mapper->RestoreEntityBuf(entity_buf, owner);
            },
            [entity_id = entity_buf.Id](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                if (nptr<ClientEntity> nullable_target = mapper->FindEntityById(*active_map, entity_id)) {
                    auto target = nullable_target.as_ptr();
                    mapper->DeleteEntity(target);
                    return true;
                }
                return false;
            }});
}

static void SetSelectionContour(ptr<ClientEntity> entity, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    // Selection outline goes through the same script contour pipeline as the client (ContourPipeline.fos):
    // write the entity's Contour property; the script's property-setter caches it and OnRenderMap_AfterSprites
    // draws it. Mapper-only callers, but the Contour property exists on every Item/Critter.
    nptr<const Property> prop = entity->GetProperties().GetRegistrator()->FindProperty("Contour");

    if (prop != nullptr) {
        entity->GetPropertiesForEdit()->SetValue<ucolor>(prop.as_ptr(), color);
    }
}

void MapperEngine::SelectAdd(ptr<ClientEntity> entity, optional<mpos> hex, bool skip_refresh)
{
    FO_STACK_TRACE_ENTRY();

    if (entity->IsDestroyed()) {
        return;
    }
    if (SelectedEntitiesSet.contains(entity)) {
        return;
    }

    ptr<ClientEntity> corrected_entity = entity;
    auto cur_map = GetCurMapPtr();

    // Break from merged mesh
    if (!SelectEntireEntity && hex.has_value()) {
        if (nptr<ItemHexView> nullable_item = corrected_entity.dyn_cast<ItemHexView>()) {
            auto item = nullable_item.as_ptr();
            auto broken_item = TryBreakItemFromMultihexMesh(cur_map, item, hex.value());

            if (!broken_item) {
                return;
            }

            corrected_entity = broken_item.as_ptr();
        }
    }

    vec_add_unique_value(SelectedEntities, corrected_entity.hold_ref());
    SelectedEntitiesSet.emplace(corrected_entity);
    InspectorVisible = true;

    // Make transparent + outline the selection (contour via the script pipeline)
    if (nptr<HexView> nullable_hex_view = corrected_entity.dyn_cast<HexView>()) {
        auto hex_view = nullable_hex_view.as_ptr();
        hex_view->SetTargetAlpha(SelectAlpha);
        SetSelectionContour(corrected_entity, SelectContourColor);
    }

    if (!skip_refresh) {
        cur_map->DefferedRefreshItems();
    }
}

void MapperEngine::SelectAll()
{
    FO_STACK_TRACE_ENTRY();

    SelectClear();

    auto cur_map = GetCurMapPtr();
    span<refcount_ptr<ItemHexView>> items = cur_map->GetItems();

    for (size_t i = 0; i < items.size(); i++) {
        auto item = items[i].as_ptr();

        if (cur_map->IsIgnorePid(item->GetProtoId())) {
            continue;
        }

        if ((!item->GetIsScenery() && !item->GetIsWall() && SelectItemsEnabled && Settings->ShowItem) || //
            (item->GetIsScenery() && SelectSceneryEnabled && Settings->ShowScen) || //
            (item->GetIsWall() && SelectWallsEnabled && Settings->ShowWall) || //
            (item->GetIsTile() && !item->GetIsRoofTile() && SelectTilesEnabled && Settings->ShowTile) || //
            (item->GetIsTile() && item->GetIsRoofTile() && SelectRoofTilesEnabled && Settings->ShowRoof)) {
            SelectAdd(item);
        }
    }

    if (SelectCrittersEnabled && Settings->ShowCrit) {
        span<refcount_ptr<CritterHexView>> critters = cur_map->GetCritters();

        for (size_t i = 0; i < critters.size(); i++) {
            auto cr = critters[i].as_ptr();
            SelectAdd(cr);
        }
    }

    cur_map->RebuildMap();
}

void MapperEngine::SelectRemove(ptr<ClientEntity> entity, bool skip_refresh)
{
    FO_STACK_TRACE_ENTRY();

    if (SelectedEntitiesSet.erase(entity) == 0) {
        return;
    }

    auto entity_holder = entity.hold_ref();
    vec_remove_unique_value(SelectedEntities, entity_holder);

    auto cur_map = GetCurMapPtr();

    // Delete intersected tiles
    if (!SelectEntireEntity && !entity->IsDestroyed()) {
        if (nptr<ItemHexView> nullable_tile = entity.dyn_cast<ItemHexView>()) {
            auto tile = nullable_tile.as_ptr();

            if (tile->GetIsTile()) {
                vector<ptr<ItemHexView>> same_siblings;

                for (ptr<ItemHexView> item : cur_map->GetItemsOnHex(tile->GetHex())) {
                    if (item != tile && item->GetIsTile() == tile->GetIsTile() && item->GetIsRoofTile() == tile->GetIsRoofTile() && //
                        item->GetTileLayer() == tile->GetTileLayer() && !SelectedEntitiesSet.contains(item.as_ptr())) {
                        same_siblings.emplace_back(item.as_ptr());
                    }
                }

                for (ptr<ItemHexView> same_sibling : copy_hold_ref(same_siblings)) {
                    if (!same_sibling->IsDestroyed()) {
                        auto nullable_broken_item = TryBreakItemFromMultihexMesh(cur_map, same_sibling, tile->GetHex());

                        if (nullable_broken_item) {
                            auto broken_item = nullable_broken_item.as_ptr();
                            cur_map->DestroyItem(broken_item);
                        }
                    }
                }
            }
        }
    }

    // Restore alpha + drop the selection outline
    if (!entity->IsDestroyed()) {
        if (nptr<HexView> nullable_hex_view = entity.dyn_cast<HexView>()) {
            auto hex_view = nullable_hex_view.as_ptr();
            hex_view->RestoreAlpha();
            SetSelectionContour(entity, ucolor::clear);
        }
    }

    // Merge multihex items
    if (!entity->IsDestroyed()) {
        if (nptr<ItemHexView> nullable_item = entity.dyn_cast<ItemHexView>()) {
            auto item = nullable_item.as_ptr();

            if (item->GetMultihexGeneration() != MultihexGenerationType::None) {
                CoalesceItemMultihexMesh(cur_map, item, true);
            }
        }
    }

    if (!skip_refresh) {
        cur_map->DefferedRefreshItems();
    }
}

void MapperEngine::SelectClear()
{
    FO_STACK_TRACE_ENTRY();

    while (!SelectedEntities.empty()) {
        SelectRemove(SelectedEntities.back().as_ptr(), true);
    }

    InContItem.reset();
    InspectorVisible = false;
    InspectorEntity = nullptr;
    ShowProps.clear();
    InspectorSelectedLine = 0;
    InspectorSelectedLineInitialValue.clear();
    InspectorSelectedLineValue.clear();
    InspectorEditBuf.clear();
    ResetInspectorPropertyEditState();

    auto cur_map = GetCurMapPtr();
    cur_map->DefferedRefreshItems();
}

auto MapperEngine::SelectMove(bool hex_move, int32_t& offs_hx, int32_t& offs_hy, int32_t& offs_x, int32_t& offs_y) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!hex_move && offs_x == 0 && offs_y == 0) {
        return false;
    }
    if (hex_move && offs_hx == 0 && offs_hy == 0) {
        return false;
    }

    // Tile step
    bool have_tiles = false;

    for (size_t i = 0; i != SelectedEntities.size(); i++) {
        auto entity = SelectedEntities[i].as_ptr();
        auto nullable_item = entity.dyn_cast<ItemHexView>();
        if (!nullable_item) {
            continue;
        }

        auto item = nullable_item.as_ptr();
        if (item->GetIsTile()) {
            have_tiles = true;
            break;
        }
    }

    if (hex_move && have_tiles) {
        if (std::abs(offs_hx) < Settings->MapTileStep && std::abs(offs_hy) < Settings->MapTileStep) {
            return false;
        }

        offs_hx -= offs_hx % Settings->MapTileStep;
        offs_hy -= offs_hy % Settings->MapTileStep;
    }

    // Setup hex moving switcher
    int32_t switcher = 0;

    if (!SelectedEntities.empty()) {
        if (refcount_nptr<CritterHexView> nullable_cr = SelectedEntities.front().dyn_cast<CritterHexView>()) {
            auto cr = nullable_cr.as_ptr();
            switcher = std::abs(cr->GetHex().x % 2);
        }
        else if (refcount_nptr<ItemHexView> nullable_item = SelectedEntities.front().dyn_cast<ItemHexView>()) {
            auto item = nullable_item.as_ptr();
            switcher = std::abs(item->GetHex().x % 2);
        }
    }

    const auto move_hex = [&](ipos32& raw_hex) {
        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            int32_t sw = switcher;

            for (int32_t k = 0; k < std::abs(offs_hx); k++, sw++) {
                GeometryHelper::MoveHexByDirUnsafe(raw_hex, offs_hx > 0 ? ((sw % 2) != 0 ? hdir::West : hdir::SouthWest) : ((sw % 2) != 0 ? hdir::NorthEast : hdir::East));
            }
            for (int32_t k = 0; k < std::abs(offs_hy); k++) {
                GeometryHelper::MoveHexByDirUnsafe(raw_hex, offs_hy > 0 ? hdir::SouthEast : hdir::NorthWest);
            }
        }
        else {
            raw_hex.x += offs_hx;
            raw_hex.y += offs_hy;
        }
    };

    auto cur_map = GetCurMapPtr();

    if (!hex_move) {
        float32_t& small_ox = SelectionSmallOffsetX;
        float32_t& small_oy = SelectionSmallOffsetY;
        const auto ox = numeric_cast<float32_t>(offs_x) / cur_map->GetSpritesZoom() + small_ox;
        const auto oy = numeric_cast<float32_t>(offs_y) / cur_map->GetSpritesZoom() + small_oy;

        if (offs_x != 0 && std::fabs(ox) < 1.0f) {
            small_ox = ox;
        }
        else {
            small_ox = 0.0f;
        }
        if (offs_y != 0 && std::fabs(oy) < 1.0f) {
            small_oy = oy;
        }
        else {
            small_oy = 0.0f;
        }

        offs_x = iround<int32_t>(ox);
        offs_y = iround<int32_t>(oy);
    }
    else {
        for (size_t i = 0; i < SelectedEntities.size(); i++) {
            auto entity = SelectedEntities[i].as_ptr();
            ipos32 raw_hex;

            if (nptr<CritterHexView> nullable_cr = entity.dyn_cast<CritterHexView>()) {
                auto cr = nullable_cr.as_ptr();
                raw_hex = ipos32 {cr->GetHex().x, cr->GetHex().y};
            }
            else if (nptr<ItemHexView> nullable_item = entity.dyn_cast<ItemHexView>()) {
                auto item = nullable_item.as_ptr();
                raw_hex = ipos32 {item->GetHex().x, item->GetHex().y};
            }

            move_hex(raw_hex);

            if (!cur_map->GetSize().is_valid_pos(raw_hex)) {
                return false;
            }

            if (nptr<ItemHexView> nullable_item = entity.dyn_cast<ItemHexView>()) {
                auto item = nullable_item.as_ptr();
                if (item->HasMultihexEntries()) {
                    auto multihex_entries = item->GetMultihexEntries().as_ptr();

                    for (const auto hex2 : *multihex_entries) {
                        ipos32 raw_hex2 = ipos32 {hex2.x, hex2.y};
                        move_hex(raw_hex2);

                        if (!cur_map->GetSize().is_valid_pos(raw_hex2)) {
                            return false;
                        }
                    }
                }
            }
        }
    }

    vector<MoveCommandEntry> move_entries;

    for (size_t i = 0; i < SelectedEntities.size(); i++) {
        auto entity = SelectedEntities[i].as_ptr();

        if (!hex_move) {
            auto nullable_item = entity.dyn_cast<ItemHexView>();

            if (!nullable_item) {
                continue;
            }

            auto item = nullable_item.as_ptr();
            auto ox = item->GetOffset().x + offs_x;
            auto oy = item->GetOffset().y + offs_y;

            if (GetApp()->Input.IsAltDown()) {
                ox = oy = 0;
            }

            MoveCommandEntry entry;
            entry.EntityId = item->GetId();
            entry.HasOffset = true;
            entry.OldOffset = item->GetOffset();
            entry.NewOffset = {numeric_cast<int16_t>(ox), numeric_cast<int16_t>(oy)};
            if (entry.OldOffset == entry.NewOffset) {
                continue;
            }
            move_entries.emplace_back(std::move(entry));

            item->SetOffset({numeric_cast<int16_t>(ox), numeric_cast<int16_t>(oy)});
            item->RefreshAnim();
        }
        else {
            ipos32 raw_hex;

            if (nptr<CritterHexView> nullable_cr = entity.dyn_cast<CritterHexView>()) {
                auto cr = nullable_cr.as_ptr();
                raw_hex = ipos32 {cr->GetHex().x, cr->GetHex().y};
            }
            else if (nptr<ItemHexView> nullable_item = entity.dyn_cast<ItemHexView>()) {
                auto item = nullable_item.as_ptr();
                raw_hex = ipos32 {item->GetHex().x, item->GetHex().y};
            }

            move_hex(raw_hex);
            const mpos hex = cur_map->GetSize().clamp_pos(raw_hex);

            if (nptr<ItemHexView> nullable_item = entity.dyn_cast<ItemHexView>()) {
                auto item = nullable_item.as_ptr();
                MoveCommandEntry entry;
                entry.EntityId = item->GetId();
                entry.HasHex = true;
                entry.OldHex = item->GetHex();
                entry.NewHex = hex;

                if (item->IsNonEmptyMultihexMesh()) {
                    entry.OldMultihexMesh = item->GetMultihexMesh();
                    auto multihex_mesh = entry.OldMultihexMesh;

                    std::ranges::for_each(multihex_mesh, [&](mpos& hex2) {
                        ipos32 raw_hex2 = ipos32(hex2);
                        move_hex(raw_hex2);
                        hex2 = cur_map->GetSize().clamp_pos(raw_hex2);
                    });

                    entry.NewMultihexMesh = multihex_mesh;
                    item->SetMultihexMesh(multihex_mesh);
                }

                if (entry.OldHex == entry.NewHex && entry.OldMultihexMesh == entry.NewMultihexMesh) {
                    continue;
                }
                move_entries.emplace_back(std::move(entry));

                cur_map->MoveItem(item, hex);
            }
            else if (nptr<CritterHexView> nullable_cr = entity.dyn_cast<CritterHexView>()) {
                auto cr = nullable_cr.as_ptr();
                MoveCommandEntry entry;
                entry.EntityId = cr->GetId();
                entry.HasHex = true;
                entry.OldHex = cr->GetHex();
                entry.NewHex = hex;
                if (entry.OldHex == entry.NewHex) {
                    continue;
                }
                move_entries.emplace_back(std::move(entry));

                cur_map->MoveCritter(cr, hex, false);
            }
        }
    }

    if (move_entries.empty()) {
        return false;
    }

    SetMapDirty(GetCurMap());
    cur_map->DefferedRefreshItems();

    for (const auto& entry : move_entries) {
        auto it = std::ranges::find_if(PendingSelectionMoveEntries, [&](const MoveCommandEntry& pending_entry) { return pending_entry.EntityId == entry.EntityId; });

        if (it == PendingSelectionMoveEntries.end()) {
            PendingSelectionMoveEntries.emplace_back(entry);
            continue;
        }

        if (entry.HasOffset) {
            it->HasOffset = true;
            it->NewOffset = entry.NewOffset;
        }
        if (entry.HasHex) {
            it->HasHex = true;
            it->NewHex = entry.NewHex;
            it->NewMultihexMesh = entry.NewMultihexMesh;
        }
    }

    return true;
}

void MapperEngine::SelectDelete()
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return;
    }

    auto cur_map = GetCurMapPtr();

    if (!UndoRedoInProgress && SelectedEntities.size() > 1) {
        struct DeleteCommandEntry
        {
            EntityBuf Entity {};
            ItemOwnership Ownership {ItemOwnership::MapHex};
            ident_t OwnerId {};
        };

        vector<DeleteCommandEntry> delete_entries;
        delete_entries.reserve(SelectedEntities.size());

        for (ptr<ClientEntity> entity : copy_hold_ref(SelectedEntities)) {
            DeleteCommandEntry entry;
            CaptureEntityBuf(entry.Entity, entity);

            if (nptr<ItemView> nullable_item = entity.dyn_cast<ItemView>()) {
                auto item = nullable_item.as_ptr();
                entry.Ownership = item->GetOwnership();
                if (entry.Ownership == ItemOwnership::CritterInventory) {
                    entry.OwnerId = item->GetCritterId();
                }
                else if (entry.Ownership == ItemOwnership::ItemContainer) {
                    entry.OwnerId = item->GetContainerId();
                }
            }

            delete_entries.emplace_back(std::move(entry));
        }

        UndoRedoInProgress = true;
        for (ptr<ClientEntity> entity : copy_hold_ref(SelectedEntities)) {
            DeleteEntity(entity);
        }
        UndoRedoInProgress = false;

        SelectClear();
        cur_map->RebuildMap();
        MouseHoldMode = INT_NONE;
        SetCurMode(CUR_MODE_DEFAULT);

        PushUndoOp(GetCurMap(),
            UndoOp {"Delete selection",
                [delete_entries](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                    for (const auto& entry : delete_entries) {
                        nptr<ClientEntity> owner;

                        if (entry.Ownership == ItemOwnership::CritterInventory || entry.Ownership == ItemOwnership::ItemContainer) {
                            owner = mapper->FindEntityById(*active_map, entry.OwnerId);
                            if (!owner) {
                                return false;
                            }
                        }

                        if (!mapper->RestoreEntityBuf(entry.Entity, owner)) {
                            return false;
                        }
                    }

                    mapper->SetMapDirty(*active_map);
                    return true;
                },
                [delete_entries](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                    for (auto it = delete_entries.rbegin(); it != delete_entries.rend(); ++it) {
                        if (nptr<ClientEntity> nullable_target = mapper->FindEntityById(*active_map, it->Entity.Id)) {
                            auto target = nullable_target.as_ptr();
                            mapper->DeleteEntity(target);
                        }
                    }

                    return true;
                }});
        return;
    }

    for (ptr<ClientEntity> entity : copy_hold_ref(SelectedEntities)) {
        DeleteEntity(entity);
    }

    SelectClear();
    cur_map->RebuildMap();
    MouseHoldMode = INT_NONE;
    SetCurMode(CUR_MODE_DEFAULT);
}

auto MapperEngine::CreateCritter(hstring pid, mpos hex) -> ptr<CritterView>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_curMap, "Mapper has no current map");
    auto cur_map = GetCurMapPtr();

    if (!cur_map->GetSize().is_valid_pos(hex)) {
        throw GenericException("Invalid hex for critter spawn", pid, hex.x, hex.y);
    }

    if (cur_map->GetNonDeadCritter(hex)) {
        throw GenericException("Hex is already occupied by a non-dead critter", pid, hex.x, hex.y);
    }

    SelectClear();

    auto cr = cur_map->AddMapperCritter(pid, hex, CritterDir, nullptr);

    SelectAdd(cr, hex);

    SetMapDirty(GetCurMap());
    cur_map->RebuildMap();
    SetCurMode(CUR_MODE_DEFAULT);

    EntityBuf entity_buf;
    CaptureEntityBuf(entity_buf, cr);
    PushUndoOp(GetCurMap(),
        UndoOp {"Create critter",
            [entity_id = entity_buf.Id](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                if (nptr<ClientEntity> nullable_target = mapper->FindEntityById(*active_map, entity_id)) {
                    auto target = nullable_target.as_ptr();
                    mapper->DeleteEntity(target);
                    return true;
                }
                return false;
            },
            [entity_buf](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                ignore_unused(active_map);
                return !!mapper->RestoreEntityBuf(entity_buf);
            }});

    return cr;
}

auto MapperEngine::CreateItem(hstring pid, mpos hex, nptr<Entity> owner) -> ptr<ItemView>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_curMap, "Mapper has no current map");
    auto cur_map = GetCurMapPtr();

    auto nullable_proto = GetProtoItem(pid);

    if (!nullable_proto) {
        throw GenericException("Invalid item proto", pid);
    }

    auto proto = nullable_proto.as_ptr();
    mpos corrected_hex = hex;

    if (proto->GetIsTile()) {
        corrected_hex = cur_map->GetSize().from_raw_pos(corrected_hex.x - corrected_hex.x % Settings->MapTileStep, corrected_hex.y - corrected_hex.y % Settings->MapTileStep);
    }

    if (!owner && (!cur_map->GetSize().is_valid_pos(corrected_hex))) {
        throw GenericException("Invalid hex for item spawn", pid, hex.x, hex.y);
    }

    if (!owner) {
        SelectClear();
    }

    nptr<ItemView> nullable_item;

    if (owner) {
        if (nptr<CritterHexView> nullable_cr = owner.dyn_cast<CritterHexView>()) {
            auto cr = nullable_cr.as_ptr();
            nullable_item = cr->AddMapperInvItem(cr->GetMap()->GenTempEntityId(), proto, CritterItemSlot::Inventory, {});
        }
        if (nptr<ItemHexView> nullable_cont = owner.dyn_cast<ItemHexView>()) {
            auto cont = nullable_cont.as_ptr();
            nullable_item = cont->AddMapperInnerItem(cont->GetMap()->GenTempEntityId(), proto, {}, nullptr);
        }
    }
    else if (proto->GetIsTile()) {
        nullable_item = cur_map->AddMapperTile(proto->GetProtoId(), corrected_hex, numeric_cast<uint8_t>(TileLayer), PreviewRoofTiles);
    }
    else {
        nullable_item = cur_map->AddMapperItem(proto->GetProtoId(), corrected_hex, nullptr);
    }

    if (!nullable_item) {
        throw GenericException("Failed to create item", pid);
    }

    auto created_item = nullable_item.as_ptr();

    if (!owner) {
        SelectAdd(created_item, corrected_hex);
        SetCurMode(CUR_MODE_DEFAULT);
    }
    else {
        SetActivePanelMode(INT_MODE_INCONT);
        InContItem = created_item.hold_ref();
    }

    SetMapDirty(GetCurMap());

    {
        EntityBuf entity_buf;
        CaptureEntityBuf(entity_buf, created_item);
        auto item_ownership = created_item->GetOwnership();
        ident_t owner_id {};

        if (item_ownership == ItemOwnership::CritterInventory) {
            owner_id = created_item->GetCritterId();
        }
        else if (item_ownership == ItemOwnership::ItemContainer) {
            owner_id = created_item->GetContainerId();
        }

        PushUndoOp(GetCurMap(),
            UndoOp {"Create item",
                [entity_id = entity_buf.Id](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                    if (nptr<ClientEntity> nullable_target = mapper->FindEntityById(*active_map, entity_id)) {
                        auto target = nullable_target.as_ptr();
                        mapper->DeleteEntity(target);
                        return true;
                    }
                    return false;
                },
                [entity_buf, item_ownership, owner_id](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                    nptr<ClientEntity> restore_owner;

                    if (item_ownership == ItemOwnership::CritterInventory || item_ownership == ItemOwnership::ItemContainer) {
                        restore_owner = mapper->FindEntityById(*active_map, owner_id);
                        if (!restore_owner) {
                            return false;
                        }
                    }

                    return !!mapper->RestoreEntityBuf(entity_buf, restore_owner);
                }});
    }

    return created_item;
}

auto MapperEngine::CloneEntity(ptr<Entity> entity) -> nptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_curMap, "Mapper has no current map");
    auto cur_map = GetCurMapPtr();

    if (nptr<CritterHexView> nullable_cr = entity.dyn_cast<CritterHexView>()) {
        auto cr = nullable_cr.as_ptr();
        auto cr_clone = cur_map->AddMapperCritter(cr->GetProtoId(), cr->GetHex(), cr->GetDir(), &cr->GetProperties());

        const_span<refcount_ptr<ItemView>> inv_items = cr->GetInvItems();

        for (size_t i = 0; i < inv_items.size(); i++) {
            auto inv_item = inv_items[i].as_ptr();
            auto nullable_inv_item_proto = inv_item->GetProto().dyn_cast<const ProtoItem>();
            FO_VERIFY_AND_THROW(nullable_inv_item_proto, "Inventory item prototype is not an item prototype");
            auto inv_item_proto = nullable_inv_item_proto.as_ptr();

            auto inv_item_clone = cr_clone->AddMapperInvItem(cur_map->GenTempEntityId(), inv_item_proto, inv_item->GetCritterSlot(), {});
            CloneInnerItems(cur_map, inv_item_clone, inv_item);
        }

        SelectAdd(cr_clone);
        SetMapDirty(GetCurMap());

        EntityBuf entity_buf;
        CaptureEntityBuf(entity_buf, cr_clone);
        PushUndoOp(GetCurMap(),
            UndoOp {"Clone entity",
                [entity_id = entity_buf.Id](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                    if (nptr<ClientEntity> nullable_target = mapper->FindEntityById(*active_map, entity_id)) {
                        auto target = nullable_target.as_ptr();
                        mapper->DeleteEntity(target);
                        return true;
                    }
                    return false;
                },
                [entity_buf](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                    ignore_unused(active_map);
                    return !!mapper->RestoreEntityBuf(entity_buf);
                }});

        return cr_clone;
    }

    if (nptr<ItemHexView> nullable_item = entity.dyn_cast<ItemHexView>()) {
        auto item = nullable_item.as_ptr();
        auto item_clone = cur_map->AddMapperItem(item->GetProtoId(), item->GetHex(), &item->GetProperties());

        CloneInnerItems(cur_map, item_clone, item);

        SelectAdd(item_clone);
        SetMapDirty(GetCurMap());

        EntityBuf entity_buf;
        CaptureEntityBuf(entity_buf, item_clone);
        PushUndoOp(GetCurMap(),
            UndoOp {"Clone entity",
                [entity_id = entity_buf.Id](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                    if (nptr<ClientEntity> nullable_target = mapper->FindEntityById(*active_map, entity_id)) {
                        auto target = nullable_target.as_ptr();
                        mapper->DeleteEntity(target);
                        return true;
                    }
                    return false;
                },
                [entity_buf](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                    ignore_unused(active_map);
                    return !!mapper->RestoreEntityBuf(entity_buf);
                }});

        return item_clone;
    }

    return nullptr;
}

void MapperEngine::CloneInnerItems(ptr<MapView> map, ptr<ItemView> to_item, ptr<const ItemView> from_item)
{
    FO_STACK_TRACE_ENTRY();

    const_span<refcount_ptr<ItemView>> inner_items = from_item->GetInnerItems();

    for (size_t i = 0; i < inner_items.size(); i++) {
        auto inner_item = inner_items[i].as_ptr();
        const auto stack_id = any_t {string(inner_item->GetContainerStack())};
        auto inner_item_proto = inner_item->GetProto().dyn_cast<const ProtoItem>();
        FO_VERIFY_AND_THROW(inner_item_proto, "Inner item prototype is not an item prototype");

        auto inner_item_clone = to_item->AddMapperInnerItem(map->GenTempEntityId(), inner_item_proto.as_ptr(), stack_id, &from_item->GetProperties());
        CloneInnerItems(map, inner_item_clone, inner_item);
    }
}

auto MapperEngine::MergeItemsToMultihexMeshes(ptr<MapView> map) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    size_t merges = 0;

    // AnyUnique items merge by (proto, data), independent of position, into the lowest-id group member. Doing
    // that with the generic per-step loop is O(N^2) (a full-map rescan per merge - the dominant cost of loading
    // a large tiled map). Coalesce them all in one O(N*groups) grouping pass; SameSibling and AnyUnique never
    // interact (MultihexGeneration is a proto property), so the result is identical to the interleaved loops.
    merges += CoalesceAnyUniqueItems(map, false);

    // SameSibling items merge spatially; the per-step best-by-id selection is path-dependent, so keep the exact
    // two-phase order (modified items first, then the rest) and the incremental driver.

    // First merge to modified items
    for (ptr<ItemHexView> item : copy_hold_ref(map->GetItems())) {
        if (!item->IsDestroyed() && item->GetMultihexGeneration() == MultihexGenerationType::SameSibling) {
            auto ignore_props = vector<ptr<const Property>> {item->GetPropertyHex(), item->GetPropertyMultihexMesh()};

            if (!item->GetProperties().CompareData(item->GetProto()->GetProperties(), ignore_props, true)) {
                merges += CoalesceItemMultihexMesh(map, item, false);
            }
        }
    }

    // Rest merge clean items
    for (ptr<ItemHexView> item : copy_hold_ref(map->GetItems())) {
        if (!item->IsDestroyed() && item->GetMultihexGeneration() == MultihexGenerationType::SameSibling) {
            merges += CoalesceItemMultihexMesh(map, item, false);
        }
    }

    // Normalize mutihex meshes (sort and move origin to first entry)
    const auto hex_less = [](auto&& hex1, auto&& hex2) { return hex1.y == hex2.y ? hex1.x < hex2.x : hex1.y < hex2.y; };

    span<refcount_ptr<ItemHexView>> map_items = map->GetItems();

    for (size_t i = 0; i < map_items.size(); i++) {
        auto item = map_items[i].as_ptr();

        if (item->IsNonEmptyMultihexMesh()) {
            auto multihex_mesh = item->GetMultihexMesh();
            std::ranges::sort(multihex_mesh, hex_less);

            if (hex_less(multihex_mesh.front(), item->GetHex())) {
                const auto hex = multihex_mesh.front();
                multihex_mesh.front() = item->GetHex();
                std::ranges::sort(multihex_mesh, hex_less);
                item->SetMultihexMesh(multihex_mesh);
                map->MoveItem(item, hex);
            }
            else {
                item->SetMultihexMesh(multihex_mesh);
            }
        }
    }

    map->DefferedRefreshItems();
    return merges;
}

auto MapperEngine::CoalesceAnyUniqueItems(ptr<MapView> map, bool skip_selected) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    // O(N*groups) bulk coalescence of every AnyUnique item on the map. AnyUnique merges any same-proto items
    // that share per-item data (ignoring Hex and MultihexMesh) into a single mesh, regardless of position,
    // collapsing each (proto, data) group into its lowest-id member - exactly what the original per-step loop
    // (TryMergeItemToMultihexMesh's AnyUnique branch + the MergeItemsToMultihexMeshes while-loops) converges to,
    // but without the O(N^2) full-map rescan per merge. The per-group merge sequence is irrelevant to the final
    // stored mesh because MergeItemsToMultihexMeshes normalizes (sorts + origin-to-smallest) afterward.
    size_t merges = 0;

    // One (proto, data) group: its current lowest-id survivor plus the members still to be merged into it.
    struct UniqueGroup
    {
        ptr<ItemHexView> survivor;
        vector<ptr<ItemHexView>> to_merge;
    };

    // Group representatives are bucketed by proto id so data comparison is only ever done within a single
    // proto. Within a proto the number of distinct data signatures is small in practice (clean tiles plus a
    // few authored variants), so the linear "first matching group" assignment stays effectively O(N).
    unordered_map<hstring, vector<UniqueGroup>> groups_by_proto;

    for (ptr<ItemHexView> item : copy_hold_ref(map->GetItems())) {
        if (item->IsDestroyed() || item->GetMultihexGeneration() != MultihexGenerationType::AnyUnique) {
            continue;
        }
        if (skip_selected && SelectedEntitiesSet.contains(item)) {
            continue;
        }

        auto ignore_props = vector<ptr<const Property>> {item->GetPropertyHex(), item->GetPropertyMultihexMesh()};
        auto& proto_groups = groups_by_proto[item->GetProtoId()];
        UniqueGroup* match = nullptr;

        for (auto& group : proto_groups) {
            if (group.survivor->GetProperties().CompareData(item->GetProperties(), ignore_props, true)) {
                match = &group;
                break;
            }
        }

        if (match == nullptr) {
            proto_groups.emplace_back(UniqueGroup {item, {}});
        }
        else if (item->GetId() < match->survivor->GetId()) {
            // Keep the lowest id as the survivor (the original always merges into the lower id).
            match->to_merge.emplace_back(match->survivor);
            match->survivor = item;
        }
        else {
            match->to_merge.emplace_back(item);
        }
    }

    // Merge each group's members into its survivor. Rather than calling MergeItemToMultihexMesh once per
    // member (each of which rescans the survivor's growing mesh - O(group^2), the second quadratic of a large
    // tiled map), gather the whole group's hexes once into a dedup set and assign the survivor's mesh a single
    // time. The resulting hex set is identical; MergeItemsToMultihexMeshes normalizes order afterward.
    unordered_set<mpos> mesh_hexes;
    vector<mpos> mesh_vec;

    // All merged-away sources are destroyed in one bulk pass at the end - destroying them individually is the
    // third O(N^2) on a large tiled map (each DestroyItem linearly compacts the membership vectors).
    vector<ptr<ItemHexView>> sources_to_destroy;

    for (auto& [proto_id, proto_groups] : groups_by_proto) {
        for (auto& group : proto_groups) {
            if (group.to_merge.empty()) {
                continue;
            }

            mesh_hexes.clear();
            mesh_vec.clear();

            const auto add_item_hexes = [&](ptr<const ItemHexView> it) {
                if (it->GetHex() != group.survivor->GetHex() && mesh_hexes.emplace(it->GetHex()).second) {
                    mesh_vec.emplace_back(it->GetHex());
                }

                if (it->IsNonEmptyMultihexMesh()) {
                    for (const auto multihex : it->GetMultihexMesh()) {
                        if (multihex != group.survivor->GetHex() && mesh_hexes.emplace(multihex).second) {
                            mesh_vec.emplace_back(multihex);
                        }
                    }
                }
            };

            add_item_hexes(group.survivor);

            for (ptr<ItemHexView> source : group.to_merge) {
                add_item_hexes(source);
            }

            group.survivor->SetMultihexMesh(mesh_vec);
            map->RefreshItem(group.survivor, true);

            for (ptr<ItemHexView> source : group.to_merge) {
                sources_to_destroy.emplace_back(source);
                merges++;
            }
        }
    }

    map->DestroyItems(sources_to_destroy);

    return merges;
}

auto MapperEngine::CoalesceItemMultihexMesh(ptr<MapView> map, ptr<ItemHexView> item, bool skip_selected) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    // Incremental driver for the merge loop that used to be `while ((item = TryMergeItemToMultihexMesh(...))
    // != nullptr) merges++;`. It produces the exact same sequence of merges (same per-step best-by-id target
    // and source, same merge direction, same survivor) as repeatedly calling TryMergeItemToMultihexMesh, but
    // collects the merge candidates ONCE per mesh hex instead of rescanning the whole growing mesh on every
    // step. That turns each coalesced region from O(N^2) to O(N) (the dominant cost of the map-load merge).
    //
    // Only the SameSibling strategy grows a mesh hex by hex (so only it is quadratic); AnyUnique merges by a
    // full-map scan per step and is left on the original per-step path.
    if (item->GetMultihexGeneration() != MultihexGenerationType::SameSibling) {
        size_t merges = 0;
        nptr<ItemHexView> nullable_merge_item = item;

        while (nullable_merge_item) {
            auto merge_item = nullable_merge_item.as_ptr();
            nullable_merge_item = TryMergeItemToMultihexMesh(map, merge_item, skip_selected);
            if (!nullable_merge_item) {
                break;
            }
            merges++;
        }

        return merges;
    }

    size_t merges = 0;

    // Candidate sets maintained across the whole loop, mirroring TryMergeItemToMultihexMesh's per-call sets:
    // an item eligible to be merged INTO (target) and an item eligible to be merged FROM (source), each found
    // around the survivor's mesh hexes via FindMultihexMeshForItemAroundHex. `scanned` records which hexes
    // already contributed their neighborhood so each hex is scanned exactly once.
    unordered_set<ptr<ItemHexView>> target_items;
    unordered_set<ptr<ItemHexView>> source_items;
    unordered_set<mpos> scanned;

    const auto scan_hex = [&](mpos hex) {
        // Mirror find_include_multihex_lines from the original per-step function. multihex_lines is re-read
        // from the current survivor (proto-derived, so constant within a region) to stay faithful if the
        // survivor object changes.
        const auto multihex_lines = item->GetMultihexLines();

        FindMultihexMeshForItemAroundHex(map, item, hex, true, target_items);
        FindMultihexMeshForItemAroundHex(map, item, hex, false, source_items);

        if (!multihex_lines.empty()) {
            GeometryHelper::ForEachMultihexLines(multihex_lines, hex, map->GetSize(), [&](mpos hex2) {
                FindMultihexMeshForItemAroundHex(map, item, hex2, true, target_items);
                FindMultihexMeshForItemAroundHex(map, item, hex2, false, source_items);
            });
        }
    };

    // Scan one hex's neighborhood exactly once. Cheap no-op if the hex was already scanned. This is what keeps
    // the whole region O(N): every hex contributes its neighborhood to the candidate sets a single time.
    const auto scan_hex_once = [&](mpos hex) {
        if (scanned.emplace(hex).second) {
            scan_hex(hex);
        }
    };

    // Collect a snapshot of every hex an item covers (origin + mesh). Used to fold the hexes a just-merged
    // party brings into the survivor, scanning ONLY those new hexes instead of re-walking the whole mesh.
    const auto collect_item_hexes = [](ptr<const ItemHexView> it, vector<mpos>& out) {
        out.clear();
        out.emplace_back(it->GetHex());

        if (it->IsNonEmptyMultihexMesh()) {
            const auto mesh = it->GetMultihexMesh();
            out.insert(out.end(), mesh.begin(), mesh.end());
        }
    };

    // Full rebuild of the candidate sets against the current survivor. Used once at the start, and again only
    // when a merge changes the survivor's DATA (the X->clean transition), because that flips which neighbors
    // are eligible. Within a constant-data regime the incremental scan below is sufficient.
    vector<mpos> hex_scratch;

    const auto rebuild = [&]() {
        target_items.clear();
        source_items.clear();
        scanned.clear();
        collect_item_hexes(item, hex_scratch);
        for (const auto hex : hex_scratch) {
            scan_hex_once(hex);
        }
    };

    auto ignore_props = vector<ptr<const Property>> {item->GetPropertyHex(), item->GetPropertyMultihexMesh()};

    rebuild();

    for (;;) {
        // The survivor never merges into itself; FindMultihexMeshForItemAroundHex would never collect it
        // (CompareMultihexItemForMerge rejects equal ids), but a prior step may have collected it before it
        // became the survivor, so drop it explicitly.
        target_items.erase(item);
        source_items.erase(item);

        vector<ptr<ItemHexView>> sorted_target_items = vec_sorted(target_items, [](ptr<ItemHexView> i1, ptr<ItemHexView> i2) { return i1->GetId() < i2->GetId(); });
        vector<ptr<ItemHexView>> sorted_source_items = vec_sorted(source_items, [](ptr<ItemHexView> i1, ptr<ItemHexView> i2) { return i2->GetId() < i1->GetId(); });

        if (skip_selected) {
            sorted_target_items = vec_filter(sorted_target_items, [&](ptr<ItemHexView> i) { return !SelectedEntitiesSet.contains(i); });
            sorted_source_items = vec_filter(sorted_source_items, [&](ptr<ItemHexView> i) { return !SelectedEntitiesSet.contains(i); });
        }

        nptr<ItemHexView> best_target_item;
        nptr<ItemHexView> best_source_item;

        if (!sorted_target_items.empty()) {
            best_target_item = sorted_target_items.front();
        }
        if (!sorted_source_items.empty()) {
            best_source_item = sorted_source_items.front();
        }

        nptr<ItemHexView> nullable_source_item;
        nptr<ItemHexView> nullable_target_item;

        if (best_target_item && (!best_source_item || best_target_item->GetId() < item->GetId())) {
            nullable_source_item = item;
            nullable_target_item = best_target_item;
        }
        else if (best_source_item) {
            nullable_source_item = best_source_item;
            nullable_target_item = item;
        }

        if (!nullable_source_item || !nullable_target_item) {
            break;
        }

        auto source_item = nullable_source_item.as_ptr();
        auto target_item = nullable_target_item.as_ptr();

        ptr<ItemHexView> old_survivor = item;

        // All of the previous survivor's hexes are already scanned; the only hexes new to the merged survivor
        // come from the OTHER party. Snapshot them before the merge destroys the source.
        ptr<ItemHexView> incoming_item = target_item == item ? source_item : target_item;
        collect_item_hexes(incoming_item, hex_scratch);

        MergeItemToMultihexMesh(map, source_item, target_item);
        merges++;

        // The merged-away source is destroyed; it must never leak back as a candidate.
        target_items.erase(source_item);
        source_items.erase(source_item);

        item = target_item;

        // The survivor's data changes only when it merges INTO a clean target whose (clean) data differs from
        // the old survivor's data; that flips neighbor eligibility, so rebuild from scratch. This happens at
        // most once per region (clean data is absorbing), keeping the whole region O(N).
        const bool data_changed = item != old_survivor && !old_survivor->GetProperties().CompareData(item->GetProperties(), ignore_props, true);

        if (data_changed) {
            rebuild();
        }
        else {
            for (const auto hex : hex_scratch) {
                scan_hex_once(hex);
            }
        }
    }

    return merges;
}

auto MapperEngine::TryMergeItemToMultihexMesh(ptr<MapView> map, ptr<ItemHexView> item, bool skip_selected) -> nptr<ItemHexView>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (item->GetMultihexGeneration() == MultihexGenerationType::None) {
        return nullptr;
    }

    nptr<ItemHexView> nullable_source_item;
    nptr<ItemHexView> nullable_target_item;

    // Find mergable item by selected strategy
    // Always prefer merge from higher ids to lower
    if (item->GetMultihexGeneration() == MultihexGenerationType::SameSibling) {
        unordered_set<ptr<ItemHexView>> target_items;
        unordered_set<ptr<ItemHexView>> source_items;
        const auto multihex_lines = item->GetMultihexLines();

        const auto find_include_multihex_lines = [&](mpos hex) {
            FindMultihexMeshForItemAroundHex(map, item, hex, true, target_items);
            FindMultihexMeshForItemAroundHex(map, item, hex, false, source_items);

            if (!multihex_lines.empty()) {
                GeometryHelper::ForEachMultihexLines(multihex_lines, hex, map->GetSize(), [&](mpos hex2) {
                    FindMultihexMeshForItemAroundHex(map, item, hex2, true, target_items);
                    FindMultihexMeshForItemAroundHex(map, item, hex2, false, source_items);
                });
            }
        };

        find_include_multihex_lines(item->GetHex());

        if (item->IsNonEmptyMultihexMesh()) {
            for (const auto multihex : item->GetMultihexMesh()) {
                find_include_multihex_lines(multihex);
            }
        }

        vector<ptr<ItemHexView>> sorted_target_items = vec_sorted(target_items, [](ptr<ItemHexView> i1, ptr<ItemHexView> i2) { return i1->GetId() < i2->GetId(); });
        vector<ptr<ItemHexView>> sorted_source_items = vec_sorted(source_items, [](ptr<ItemHexView> i1, ptr<ItemHexView> i2) { return i2->GetId() < i1->GetId(); });

        if (skip_selected) {
            sorted_target_items = vec_filter(sorted_target_items, [&](ptr<ItemHexView> i) { return !SelectedEntitiesSet.contains(i); });
            sorted_source_items = vec_filter(sorted_source_items, [&](ptr<ItemHexView> i) { return !SelectedEntitiesSet.contains(i); });
        }

        nptr<ItemHexView> best_target_item;
        nptr<ItemHexView> best_source_item;

        if (!sorted_target_items.empty()) {
            best_target_item = sorted_target_items.front();
        }
        if (!sorted_source_items.empty()) {
            best_source_item = sorted_source_items.front();
        }

        if (best_target_item && (!best_source_item || best_target_item->GetId() < item->GetId())) {
            nullable_source_item = item;
            nullable_target_item = best_target_item;
        }
        else if (best_source_item) {
            nullable_source_item = best_source_item;
            nullable_target_item = item;
        }
    }
    else if (item->GetMultihexGeneration() == MultihexGenerationType::AnyUnique) {
        span<refcount_ptr<ItemHexView>> map_items = map->GetItems();

        for (size_t check_index = map_items.size(); check_index > 0; check_index--) {
            auto check_item = map_items[check_index - 1].as_ptr();

            if (check_item->GetProtoId() != item->GetProtoId()) {
                continue;
            }
            if (skip_selected && SelectedEntitiesSet.contains(check_item)) {
                continue;
            }

            if (CompareMultihexItemForMerge(check_item, item, false)) {
                if (item->GetId() < check_item->GetId()) {
                    nullable_source_item = check_item;
                    nullable_target_item = item;
                }
                else {
                    nullable_source_item = item;
                    nullable_target_item = check_item;
                }
                break;
            }
        }
    }

    // Merge item and his mesh to another item mesh
    if (nullable_source_item && nullable_target_item) {
        auto source_item = nullable_source_item.as_ptr();
        auto target_item = nullable_target_item.as_ptr();
        MergeItemToMultihexMesh(map, source_item, target_item);
        return nullable_target_item;
    }

    return nullptr;
}

void MapperEngine::MergeItemToMultihexMesh(ptr<MapView> map, ptr<ItemHexView> source_item, ptr<ItemHexView> target_item)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto multihex_mesh = target_item->GetMultihexMesh();
    bool some_hex_added = false;

    if (source_item->GetHex() != target_item->GetHex() && vec_safe_add_unique_value(multihex_mesh, source_item->GetHex())) {
        some_hex_added = true;
    }

    if (source_item->IsNonEmptyMultihexMesh()) {
        const auto source_multihex_mesh = source_item->GetMultihexMesh();
        multihex_mesh.reserve(multihex_mesh.size() + source_multihex_mesh.size());

        for (const auto multihex : source_multihex_mesh) {
            if (multihex != target_item->GetHex() && vec_safe_add_unique_value(multihex_mesh, multihex)) {
                some_hex_added = true;
            }
        }
    }

    if (some_hex_added) {
        target_item->SetMultihexMesh(multihex_mesh);
        map->RefreshItem(target_item, true);
    }

    map->DestroyItem(source_item);
}

void MapperEngine::FindMultihexMeshForItemAroundHex(ptr<MapView> map, ptr<ItemHexView> item, mpos hex, bool merge_to_it, unordered_set<ptr<ItemHexView>>& result) const
{
    FO_STACK_TRACE_ENTRY();

    const auto find_mergable_item_on_hex = [&](mpos check_hex) -> nptr<ItemHexView> {
        if (!map->GetSize().is_valid_pos(check_hex)) {
            return nullptr;
        }

        for (ptr<ItemHexView> check_item : map->GetItemsOnHex(check_hex)) {
            if (result.contains(check_item.as_ptr())) {
                continue;
            }
            if (check_item->GetProtoId() != item->GetProtoId()) {
                continue;
            }

            if (CompareMultihexItemForMerge(merge_to_it ? check_item.as_ptr() : item, merge_to_it ? item : check_item.as_ptr(), true)) {
                return check_item.as_nptr();
            }
        }

        return nullptr;
    };

    // At same hex
    if (nptr<ItemHexView> nullable_mergable_item = find_mergable_item_on_hex(hex); nullable_mergable_item) {
        auto mergable_item = nullable_mergable_item.as_ptr();
        result.emplace(mergable_item);
    }

    // Neighbor hexes
    for (int32_t i = 0; i < GameSettings::MAP_DIR_COUNT; i++) {
        if (mpos hex2 = hex; GeometryHelper::MoveHexByDir(hex2, hdir(i), map->GetSize())) {
            if (nptr<ItemHexView> nullable_mergable_item = find_mergable_item_on_hex(hex2); nullable_mergable_item) {
                auto mergable_item = nullable_mergable_item.as_ptr();
                result.emplace(mergable_item);
            }
        }
    }
}

auto MapperEngine::CompareMultihexItemForMerge(ptr<const ItemHexView> source_item, ptr<const ItemHexView> target_item, bool allow_clean_merge) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (source_item->GetId() == target_item->GetId()) {
        return false;
    }
    if (source_item->GetProtoId() != target_item->GetProtoId()) {
        return false;
    }

    // Our data is not modified (same as in proto)
    auto ignore_props = vector<ptr<const Property>> {source_item->GetPropertyHex(), source_item->GetPropertyMultihexMesh()};

    if (allow_clean_merge) {
        if (source_item->GetProperties().CompareData(source_item->GetProto()->GetProperties(), ignore_props, true)) {
            return true;
        }
    }

    // Our data is same as checked item
    if (source_item->GetProperties().CompareData(target_item->GetProperties(), ignore_props, true)) {
        return true;
    }

    return false;
}

auto MapperEngine::BreakItemsMultihexMeshes(ptr<MapView> map) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    size_t breaks = 0;

    vector<refcount_ptr<ItemHexView>> items = to_vector(map->GetItems());

    for (size_t i = 0; i < items.size(); i++) {
        auto item = items[i].as_ptr();

        if (!item->IsNonEmptyMultihexMesh()) {
            continue;
        }

        const auto multihex_mesh = item->GetMultihexMesh();
        item->SetMultihexMesh({});

        for (const auto multihex : multihex_mesh) {
            map->AddMapperItem(item->GetProtoId(), multihex, &item->GetProperties());
            breaks++;
        }

        map->RefreshItem(item, true);
    }

    map->DefferedRefreshItems();
    return breaks;
}

auto MapperEngine::TryBreakItemFromMultihexMesh(ptr<MapView> map, ptr<ItemHexView> item, mpos hex) -> nptr<ItemHexView>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (!item->IsNonEmptyMultihexMesh()) {
        return item;
    }

    const auto multihex_lines = item->GetMultihexLines();

    const auto check_hex_for_hit = [&](mpos item_hex) {
        if (item_hex == hex) {
            return true;
        }

        if (!multihex_lines.empty()) {
            bool found = false;
            GeometryHelper::ForEachMultihexLines(multihex_lines, item_hex, map->GetSize(), [&](mpos line_hex) { found = found || line_hex == hex; });
            return found;
        }

        return false;
    };

    auto multihex_mesh = item->GetMultihexMesh();

    if (check_hex_for_hit(item->GetHex())) {
        item->SetMultihexMesh({});
        auto separated_item = map->AddMapperItem(item->GetProtoId(), item->GetHex(), &item->GetProperties());

        const auto new_mesh_holder_hex = multihex_mesh.front();
        vec_remove_unique_value(multihex_mesh, new_mesh_holder_hex);
        item->SetMultihexMesh(multihex_mesh);
        map->MoveItem(item, new_mesh_holder_hex);

        return separated_item;
    }
    else {
        for (const auto multihex : multihex_mesh) {
            if (check_hex_for_hit(multihex)) {
                item->SetMultihexMesh({});
                auto separated_item = map->AddMapperItem(item->GetProtoId(), multihex, &item->GetProperties());

                vec_remove_unique_value(multihex_mesh, multihex);
                item->SetMultihexMesh(multihex_mesh);
                map->RefreshItem(item, true);

                return separated_item;
            }
        }

        return nullptr;
    }
}

void MapperEngine::BufferCopy()
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return;
    }

    auto cur_map = GetCurMapPtr();
    BufferRawHex = cur_map->GetScreenRawHex();
    EntitiesBuffer.clear();

    // Add entities to buffer
    function<void(EntityBuf&, ptr<ClientEntity>)> add_entity;
    add_entity = [&add_entity, this](EntityBuf& entity_buf, ptr<ClientEntity> entity) {
        mpos hex;

        auto cr = entity.dyn_cast<CritterHexView>();
        auto item = entity.dyn_cast<ItemHexView>();
        auto entity_with_proto = entity.dyn_cast<EntityWithProto>();
        FO_VERIFY_AND_THROW(entity_with_proto, "Buffered entity does not have an associated prototype");

        if (cr) {
            hex = cr->GetHex();
        }
        else if (item) {
            hex = item->GetHex();
        }

        entity_buf.Hex = hex;
        entity_buf.IsCritter = !!cr;
        entity_buf.IsItem = !!item;
        entity_buf.Proto = entity_with_proto->GetProto();
        entity_buf.Props.emplace(entity->GetProperties().Copy());

        vector<refcount_ptr<ItemView>> children = GetEntityInnerItems(entity);

        for (size_t i = 0; i < children.size(); i++) {
            auto child = children[i].as_ptr();
            unique_ptr<EntityBuf> child_buf = SafeAlloc::MakeUnique<EntityBuf>();
            add_entity(*child_buf, child);
            entity_buf.Children.emplace_back(std::move(child_buf));
        }
    };

    for (size_t i = 0; i < SelectedEntities.size(); i++) {
        auto entity = SelectedEntities[i].as_ptr();
        EntitiesBuffer.emplace_back();
        add_entity(EntitiesBuffer.back(), entity);
    }
}

void MapperEngine::BufferCut()
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return;
    }

    BufferCopy();
    SelectDelete();
}

void MapperEngine::BufferPaste()
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return;
    }

    auto cur_map = GetCurMapPtr();
    vector<EntityBuf> pasted_entities;

    const ipos32 screen_raw_hex = cur_map->GetScreenRawHex();
    const auto hx_offset = screen_raw_hex.x - BufferRawHex.x;
    const auto hy_offset = screen_raw_hex.y - BufferRawHex.y;

    SelectClear();

    for (const auto& entity_buf : EntitiesBuffer) {
        const auto raw_hx = numeric_cast<int32_t>(entity_buf.Hex.x) + hx_offset;
        const auto raw_hy = numeric_cast<int32_t>(entity_buf.Hex.y) + hy_offset;

        if (!cur_map->GetSize().is_valid_pos(raw_hx, raw_hy)) {
            continue;
        }

        const mpos hex = cur_map->GetSize().from_raw_pos(raw_hx, raw_hy);

        function<void(const EntityBuf&, ptr<ItemView>)> add_item_inner_items;

        add_item_inner_items = [&add_item_inner_items, &cur_map](const EntityBuf& item_entity_buf, ptr<ItemView> item) {
            for (const auto& child_buf : item_entity_buf.Children) {
                auto inner_item = item->AddMapperInnerItem(cur_map->GenTempEntityId(), child_buf->Proto.dyn_cast<const ProtoItem>().as_ptr(), {}, child_buf->GetProps());
                add_item_inner_items(*child_buf, inner_item);
            }
        };

        if (entity_buf.IsCritter) {
            auto cr = cur_map->AddMapperCritter(entity_buf.Proto->GetProtoId(), hex, mdir(), entity_buf.GetProps());

            for (const auto& child_buf : entity_buf.Children) {
                auto inv_item = cr->AddMapperInvItem(cur_map->GenTempEntityId(), child_buf->Proto.dyn_cast<const ProtoItem>().as_ptr(), CritterItemSlot::Inventory, child_buf->GetProps());
                add_item_inner_items(*child_buf, inv_item);
            }

            SelectAdd(cr);

            pasted_entities.emplace_back();
            CaptureEntityBuf(pasted_entities.back(), cr);
        }
        else if (entity_buf.IsItem) {
            auto item = cur_map->AddMapperItem(entity_buf.Proto->GetProtoId(), hex, entity_buf.GetProps());
            add_item_inner_items(entity_buf, item);
            SelectAdd(item);

            pasted_entities.emplace_back();
            CaptureEntityBuf(pasted_entities.back(), item);
        }
    }

    if (!pasted_entities.empty()) {
        PushUndoOp(GetCurMap(),
            UndoOp {"Paste selection",
                [pasted_entities](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                    for (auto it = pasted_entities.rbegin(); it != pasted_entities.rend(); ++it) {
                        if (nptr<ClientEntity> nullable_target = mapper->FindEntityById(*active_map, it->Id)) {
                            auto target = nullable_target.as_ptr();
                            mapper->DeleteEntity(target);
                        }
                    }

                    return true;
                },
                [pasted_entities](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) {
                    for (const auto& entity_buf : pasted_entities) {
                        if (!mapper->RestoreEntityBuf(entity_buf)) {
                            return false;
                        }
                    }

                    mapper->SetMapDirty(*active_map);
                    return true;
                }});
    }
}

auto MapperEngine::JumpHistoryToIndex(int32_t target_index) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return false;
    }

    auto ctx = GetUndoContext(GetCurMap(), false);
    const auto total_count = ctx ? numeric_cast<int32_t>(ctx->UndoStack.size() + ctx->RedoStack.size()) : 0;
    target_index = std::clamp(target_index, 0, total_count);

    while (true) {
        auto cur_ctx = GetUndoContext(GetCurMap(), false);
        const auto applied_count = cur_ctx ? numeric_cast<int32_t>(cur_ctx->UndoStack.size()) : 0;

        if (applied_count == target_index) {
            return true;
        }

        if (applied_count > target_index) {
            if (!ExecuteUndo()) {
                return false;
            }
        }
        else {
            if (!ExecuteRedo()) {
                return false;
            }
        }
    }
}

void MapperEngine::DrawHistoryWindowImGui()
{
    FO_STACK_TRACE_ENTRY();

    if (!HistoryWindowVisible) {
        return;
    }

    ImGui::SetNextWindowPos({48.0f, 48.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({640.0f, 760.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("History", &HistoryWindowVisible, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    if (!_curMap) {
        ImGui::TextUnformatted("No map loaded");
        ImGui::End();
        return;
    }

    auto ctx = GetUndoContext(GetCurMap(), false);
    const auto undo_count = ctx ? numeric_cast<int32_t>(ctx->UndoStack.size()) : 0;
    const auto redo_count = ctx ? numeric_cast<int32_t>(ctx->RedoStack.size()) : 0;
    const auto total_count = undo_count + redo_count;

    auto& last_history_map = LastHistoryMap;
    int32_t& last_history_undo_count = LastHistoryUndoCount;
    const auto scroll_to_current = ImGui::IsWindowAppearing() || !(last_history_map == GetCurMap()) || last_history_undo_count != undo_count;

    last_history_map = GetCurMap();
    last_history_undo_count = undo_count;

    ImGui::Text("Applied: %d / %d", undo_count, total_count);
    ImGui::Separator();

    const auto draw_history_button = [&](string_view label, int32_t target_index, bool is_current) {
        const auto button_label = is_current ? strex("%s  [current]", label).str() : string(label);
        if (is_current) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }

        const auto clicked = ImGui::Button(button_label.c_str(), {-FLT_MIN, 0.0f});

        if (is_current) {
            ImGui::PopStyleColor(2);
        }

        if (clicked && !is_current) {
            JumpHistoryToIndex(target_index);
        }
    };

    if (ImGui::BeginChild("HistoryEntries", {0.0f, 0.0f}, false)) {
        if (undo_count == 0) {
            draw_history_button("Initial state", 0, true);
        }
        else {
            draw_history_button("Initial state", 0, false);
        }

        if (ctx && undo_count > 0) {
            ImGui::SeparatorText("Undo");

            for (int32_t index = 0; index < undo_count; index++) {
                const auto label = strex("{}. {}", index + 1, ctx->UndoStack[numeric_cast<size_t>(index)].Label).str();
                draw_history_button(label, index + 1, false);
            }
        }

        ImGui::SeparatorText("Current position");
        ImGui::TextDisabled("Applied commands: %d", undo_count);
        if (scroll_to_current) {
            ImGui::SetScrollHereY(0.5f);
        }

        if (ctx && redo_count > 0) {
            ImGui::SeparatorText("Redo");

            for (int32_t index = 0; index < redo_count; index++) {
                const auto redo_index = redo_count - 1 - index;
                const auto target_index = undo_count + index + 1;
                const auto label = strex("{}. {}", target_index, ctx->RedoStack[numeric_cast<size_t>(redo_index)].Label).str();
                draw_history_button(label, target_index, false);
            }
        }

        ImGui::EndChild();
    }

    ImGui::End();
}

void MapperEngine::DrawStr(const irect32& rect, string_view str, ucolor color, TextFormat format)
{
    FO_STACK_TRACE_ENTRY();

    FontMngr.DrawText(rect, str, color, format);
}

void MapperEngine::CurDraw()
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return;
    }

    const auto can_place_object = CurMode == CUR_MODE_PLACE_OBJECT && ((IsItemMode() && ActiveItemProtos && !ActiveItemProtos->empty()) || (IsCritMode() && ActiveCritterProtos && !ActiveCritterProtos->empty()));

    if (!can_place_object) {
        return;
    }

    auto cur_map = GetCurMapPtr();

    if (IsItemMode() && ActiveItemProtos && !ActiveItemProtos->empty()) {
        const auto& proto = (*ActiveItemProtos)[GetActiveProtoIndex()];

        mpos hex;

        if (!cur_map->GetHexAtScreen(MousePos, hex, nullptr)) {
            return;
        }

        if (proto->GetIsTile()) {
            hex = cur_map->GetSize().from_raw_pos(hex.x - hex.x % Settings->MapTileStep, hex.y - hex.y % Settings->MapTileStep);
        }

        auto nullable_spr = GetPreviewSprite(proto->GetPicMap());

        if (nullable_spr) {
            const auto zoom = cur_map->GetSpritesZoom();
            ipos32 pos = cur_map->MapToScreenPos(cur_map->GetHexMapPos(hex));
            pos += ipos32(iround<int32_t>(numeric_cast<float32_t>(proto->GetOffset().x) * zoom), iround<int32_t>(numeric_cast<float32_t>(proto->GetOffset().y) * zoom));
            pos += ipos32(iround<int32_t>(numeric_cast<float32_t>(nullable_spr->GetOffset().x) * zoom), iround<int32_t>(numeric_cast<float32_t>(nullable_spr->GetOffset().y) * zoom));
            pos += ipos32(iround<int32_t>(numeric_cast<float32_t>(Settings->MapHexWidth / 2) * zoom), iround<int32_t>(numeric_cast<float32_t>(Settings->MapHexHeight) * zoom));
            pos -= ipos32(iround<int32_t>(numeric_cast<float32_t>(nullable_spr->GetSize().width / 2) * zoom), iround<int32_t>(numeric_cast<float32_t>(nullable_spr->GetSize().height) * zoom));

            if (proto->GetIsTile() && PreviewRoofTiles) {
                // The flat tile/roof XY offset already came from the prototype Offset above; a roof preview rides the
                // same 3D elevation as a placed roof tile, so raise it on screen by that elevation's projection.
                const auto elev_y = GeometryHelper::ProjectWorldToMap(vec3 {0.0F, numeric_cast<float32_t>(Settings->MapRoofElevation), 0.0F}).y;
                pos.y += iround<int32_t>(elev_y * zoom);
            }

            const auto width = iround<int32_t>(numeric_cast<float32_t>(nullable_spr->GetSize().width) * zoom);
            const auto height = iround<int32_t>(numeric_cast<float32_t>(nullable_spr->GetSize().height) * zoom);
            auto preview_spr = nullable_spr.as_ptr();
            SprMngr.DrawSpriteSize(preview_spr, pos, {width, height}, true, false, Color::Neutral);
        }
        return;
    }

    if (IsCritMode() && ActiveCritterProtos && !ActiveCritterProtos->empty()) {
        const auto model_name = (*ActiveCritterProtos)[GetActiveProtoIndex()]->GetModelName();
        auto anim = ResMngr.GetCritterPreviewSpr(model_name, CritterStateAnim::Unarmed, CritterActionAnim::Idle, CritterDir, nullptr);

        mpos hex;

        if (!cur_map->GetHexAtScreen(MousePos, hex, nullptr)) {
            return;
        }

        const auto zoom = cur_map->GetSpritesZoom();
        ipos32 pos = cur_map->MapToScreenPos(cur_map->GetHexMapPos(hex));
        pos += ipos32(iround<int32_t>(numeric_cast<float32_t>(anim->GetOffset().x) * zoom), iround<int32_t>(numeric_cast<float32_t>(anim->GetOffset().y) * zoom));
        pos -= ipos32(iround<int32_t>(numeric_cast<float32_t>(anim->GetSize().width / 2) * zoom), iround<int32_t>(numeric_cast<float32_t>(anim->GetSize().height) * zoom));

        const auto width = iround<int32_t>(numeric_cast<float32_t>(anim->GetSize().width) * zoom);
        const auto height = iround<int32_t>(numeric_cast<float32_t>(anim->GetSize().height) * zoom);
        SprMngr.DrawSpriteSize(anim, pos, {width, height}, true, false, Color::Neutral);
    }
}

void MapperEngine::DrawSettingsWindowImGui()
{
    FO_STACK_TRACE_ENTRY();

    if (!SettingsWindowVisible) {
        return;
    }

    ImGui::SetNextWindowPos({96.0f, 96.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({420.0f, 320.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Settings", &SettingsWindowVisible, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    const auto apply_resolution = [&](isize32 resolution) {
        if (Settings->ScreenWidth == resolution.width && Settings->ScreenHeight == resolution.height) {
            return;
        }

        SprMngr.SetScreenSize(resolution);
        SprMngr.SetWindowSize(resolution);
        UpdateLocalConfigValue(Cache, "ScreenWidth", strex("{}", resolution.width));
        UpdateLocalConfigValue(Cache, "ScreenHeight", strex("{}", resolution.height));
        AddMess(strex("Resolution changed to {}x{}", resolution.width, resolution.height));
    };

    ImGui::Text("Current resolution: %d x %d", Settings->ScreenWidth, Settings->ScreenHeight);
    auto fullscreen = SprMngr.IsFullscreen();
    if (ImGui::Checkbox("Fullscreen", &fullscreen)) {
        SprMngr.ToggleFullscreen();
    }
    ImGui::Separator();
    if (ImGui::Button("Reset layout")) {
        ResetImGuiSettingsRequested = true;
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Popular resolutions");

    constexpr array<isize32, 10> popular_resolutions {{
        {1024, 768},
        {1280, 720},
        {1280, 800},
        {1366, 768},
        {1440, 900},
        {1600, 900},
        {1680, 1050},
        {1920, 1080},
        {2560, 1440},
        {3840, 2160},
    }};

    if (ImGui::BeginChild("##SettingsResolutions", {0.0f, 0.0f}, false)) {
        for (const auto& res : popular_resolutions) {
            const auto is_current = Settings->ScreenWidth == res.width && Settings->ScreenHeight == res.height;
            if (ImGui::Selectable(strex("{} x {}", res.width, res.height).c_str(), is_current)) {
                apply_resolution(res);
            }
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

void MapperEngine::CurRMouseUp()
{
    FO_STACK_TRACE_ENTRY();

    if (MouseHoldMode == INT_PAN) {
        if (RightMouseDragged) {
            const auto now_time = nanotime::now();
            const auto sample_ms = (now_time - RightMouseVelocityTime).to_ms<float32_t>();

            if ((RightMouseVelocityAccum.x != 0.0f || RightMouseVelocityAccum.y != 0.0f) && sample_ms > 0.0f) {
                RightMouseInertia = RightMouseVelocityAccum * (1000.0f / sample_ms);
            }

            RightMouseVelocityAccum = {};
            RightMouseVelocityTime = {};
        }
        else {
            RightMouseInertia = {};
            RightMouseVelocityAccum = {};
            RightMouseVelocityTime = {};

            if (CurMode == CUR_MODE_PLACE_OBJECT) {
                SetCurMode(CUR_MODE_DEFAULT);
            }

            if (!SelectedEntities.empty()) {
                SelectClear();
            }
        }

        MouseHoldMode = INT_NONE;
        RightMouseDragged = false;
    }
}

void MapperEngine::CurMMouseDown()
{
    FO_STACK_TRACE_ENTRY();

    if (SelectedEntities.empty()) {
        CritterDir = GetNextCritterDir(CritterDir);

        PreviewRoofTiles = !PreviewRoofTiles;
    }
    else {
        for (size_t i = 0; i < SelectedEntities.size(); i++) {
            auto entity = SelectedEntities[i].as_ptr();

            if (nptr<CritterHexView> nullable_cr = entity.dyn_cast<CritterHexView>()) {
                auto cr = nullable_cr.as_ptr();
                AdvanceCritterDir(cr);
            }
        }
    }
}

void MapperEngine::SetCurMode(int cur_mode)
{
    FO_STACK_TRACE_ENTRY();

    CurMode = cur_mode;

    // Restore alpha
    for (size_t i = 0; i < SelectedEntities.size(); i++) {
        auto entity = SelectedEntities[i].as_ptr();

        if (nptr<HexView> nullable_hex_view = entity.dyn_cast<HexView>()) {
            auto hex_view = nullable_hex_view.as_ptr();
            if (CurMode == CUR_MODE_MOVE_SELECTION) {
                hex_view->RestoreAlpha();
            }
            else {
                hex_view->SetTargetAlpha(SelectAlpha);
            }
        }
    }
}

auto MapperEngine::IsCurInRect(const irect32& rect, int32_t ax, int32_t ay) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return MousePos.x >= rect.x + ax && MousePos.y >= rect.y + ay && MousePos.x < rect.x + rect.width + ax && MousePos.y < rect.y + rect.height + ay;
}

auto MapperEngine::IsCurInRect(const irect32& rect) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return MousePos.x >= rect.x && MousePos.y >= rect.y && MousePos.x < rect.x + rect.width && MousePos.y < rect.y + rect.height;
}

auto MapperEngine::IsCurInInterface() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return IsImGuiMouseCaptured();
}

auto MapperEngine::GetCurHex(mpos& hex, bool ignore_interface) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    hex = {};

    if (!ignore_interface && IsCurInInterface()) {
        return false;
    }

    auto cur_map = GetCurMapPtr();
    return cur_map->GetHexAtScreen(MousePos, hex, nullptr);
}

void MapperEngine::DrawConsoleImGui()
{
    FO_STACK_TRACE_ENTRY();

    if (!ConsoleEdit) {
        return;
    }

    auto& console_buf = ConsoleBuf;
    bool& sync_from_state = ConsoleSyncFromState;

    auto sync_buffer = [&]() {
        std::fill(console_buf.begin(), console_buf.end(), '\0');
        const auto copy_len = std::min(ConsoleStr.size(), console_buf.size() - 1);
        std::copy_n(ConsoleStr.data(), copy_len, console_buf.data());
    };

    const auto base_y = numeric_cast<float32_t>(MainPanelPos.y + MainPanelWindowRect.height + MAPPER_CONSOLE_WINDOW_OFFSET.y);
    const auto base_x = numeric_cast<float32_t>(MainPanelPos.x + MAPPER_CONSOLE_WINDOW_OFFSET.x);

    ImGui::SetNextWindowPos({base_x, base_y}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    auto keep_open = ConsoleEdit;
    const auto window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Mapper Console", &keep_open, window_flags)) {
        if (sync_from_state || ImGui::IsWindowAppearing()) {
            sync_buffer();
            sync_from_state = false;
        }

        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputText("##MapperConsoleInput", console_buf.data(), console_buf.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
            ConsoleStr = console_buf.data();
            ConsoleSubmitCommand();
            sync_from_state = true;
        }

        ConsoleStr = console_buf.data();

        if (ImGui::IsItemActive()) {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                if (ConsoleHistoryCur > 0) {
                    ConsoleHistoryCur--;
                    ConsoleStr = ConsoleHistory[ConsoleHistoryCur];
                    sync_from_state = true;
                }
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                if (ConsoleHistoryCur + 1 >= numeric_cast<int32_t>(ConsoleHistory.size())) {
                    ConsoleHistoryCur = numeric_cast<int32_t>(ConsoleHistory.size());
                    ConsoleStr.clear();
                }
                else {
                    ConsoleHistoryCur++;
                    ConsoleStr = ConsoleHistory[ConsoleHistoryCur];
                }

                sync_from_state = true;
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ConsoleEdit = false;
            ConsoleStr.clear();
            sync_from_state = true;
        }
    }
    ImGui::End();

    if (!keep_open) {
        ConsoleEdit = false;
        ConsoleStr.clear();
        sync_from_state = true;
    }
}

void MapperEngine::ConsoleSubmitCommand()
{
    FO_STACK_TRACE_ENTRY();

    if (ConsoleStr.empty()) {
        ConsoleEdit = false;
        return;
    }

    // Keep history unique and bounded.
    ConsoleHistory.emplace_back(ConsoleStr);

    for (int32_t i = 0; i < numeric_cast<int32_t>(ConsoleHistory.size()) - 1; i++) {
        if (ConsoleHistory[i] == ConsoleHistory[ConsoleHistory.size() - 1]) {
            ConsoleHistory.erase(ConsoleHistory.begin() + i);
            i = -1;
        }
    }

    while (numeric_cast<int32_t>(ConsoleHistory.size()) > Settings->ConsoleHistorySize) {
        ConsoleHistory.erase(ConsoleHistory.begin());
    }

    ConsoleHistoryCur = numeric_cast<int32_t>(ConsoleHistory.size());

    string history_str;
    for (const auto& str : ConsoleHistory) {
        history_str += str + "\n";
    }
    Cache.SetString("mapper_console.txt", history_str);

    const auto process_command = OnMapperMessage.Fire(ConsoleStr) == EventResult::ContinueChain;
    AddMess(ConsoleStr);

    if (process_command) {
        ParseCommand(ConsoleStr);
    }

    ConsoleEdit = false;
    ConsoleStr = "";
}

void MapperEngine::ParseCommand(string_view command)
{
    FO_STACK_TRACE_ENTRY();

    if (command.empty()) {
        return;
    }

    // Load map
    if (command[0] == '~') {
        const string_view map_name = strvex(command.substr(1)).trim();

        if (map_name.empty()) {
            AddMess("Error parse map name");
            return;
        }

        if (nptr<MapView> nullable_map = LoadMap(map_name); nullable_map) {
            AddMess("Load map success");
            auto map = nullable_map.as_ptr();
            ShowMap(map);
        }
        else {
            AddMess("Load map failed");
        }
    }
    // Save map
    else if (command[0] == '^') {
        const string_view map_name = strvex(command.substr(1)).trim();

        if (map_name.empty()) {
            AddMess("Error parse map name");
            return;
        }

        if (!_curMap) {
            AddMess("Map not loaded");
            return;
        }

        auto cur_map = GetCurMapPtr();
        SaveMap(cur_map, map_name);

        AddMess("Save map success");
    }
    // Run script
    else if (command[0] == '#') {
        const auto before_snapshot = _curMap && !UndoRedoInProgress ? CaptureMapSnapshot(GetCurMap()) : string {};
        const auto command_str = string(command.substr(1));
        istringstream icmd(command_str);
        string func_name;

        if (!(icmd >> func_name)) {
            AddMess("Function name not typed");
            return;
        }

        auto func = FindFunc<string, string>(Hashes.ToHashedString(func_name));

        if (!func) {
            AddMess("Function not found");
            return;
        }

        string str = strvex(command).substring_after(' ').trim().str();

        if (!func.Call(str)) {
            AddMess("Script execution fail");
            return;
        }

        if (!before_snapshot.empty()) {
            const auto after_snapshot = CaptureMapSnapshot(GetCurMap());
            if (before_snapshot != after_snapshot) {
                auto cur_map = GetCurMapPtr();
                const string map_name = string(cur_map->GetName());
                PushUndoOp(GetCurMap(), UndoOp {strex("Script {}", func_name), [map_name, before_snapshot](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) { return mapper->RestoreMapSnapshot(active_map, map_name, before_snapshot); }, [map_name, after_snapshot](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) { return mapper->RestoreMapSnapshot(active_map, map_name, after_snapshot); }, true});
            }
        }

        AddMess(strex("Result: {}", func.GetResult()));
    }
    // Critter animations
    else if (command[0] == '@') {
        AddMess("Playing critter animations");

        if (!_curMap) {
            AddMess("Map not loaded");
            return;
        }

        vector<int32_t> anims = strvex(command.substr(1)).split_to_int32(' ');

        if (anims.empty()) {
            return;
        }

        auto cur_map = GetCurMapPtr();

        if (!SelectedEntities.empty()) {
            for (size_t i = 0; i < SelectedEntities.size(); i++) {
                auto entity = SelectedEntities[i].as_ptr();

                if (nptr<CritterHexView> nullable_cr = entity.dyn_cast<CritterHexView>()) {
                    auto cr = nullable_cr.as_ptr();
                    cr->StopAnim();

                    for (size_t j = 0; j < anims.size() / 2; j++) {
                        cr->AppendAnim(static_cast<CritterStateAnim>(anims[numeric_cast<size_t>(j) * 2]), static_cast<CritterActionAnim>(anims[j * 2 + 1]), nullptr);
                    }
                }
            }
        }
        else {
            span<refcount_ptr<CritterHexView>> critters = cur_map->GetCritters();

            for (size_t i = 0; i < critters.size(); i++) {
                auto cr = critters[i].as_ptr();

                cr->StopAnim();

                for (size_t j = 0; j < anims.size() / 2; j++) {
                    cr->AppendAnim(static_cast<CritterStateAnim>(anims[numeric_cast<size_t>(j) * 2]), static_cast<CritterActionAnim>(anims[j * 2 + 1]), nullptr);
                }
            }
        }
    }
    // Other
    else if (command[0] == '*') {
        const auto icommand_str = string(command.substr(1));
        istringstream icommand(icommand_str);
        string command_ext;

        if (!(icommand >> command_ext)) {
            return;
        }

        if (command_ext == "new") {
            auto nullable_registrator = GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME);
            FO_VERIFY_AND_THROW(nullable_registrator, "Map property registrator is not available");
            auto registrator = nullable_registrator.as_ptr();

            refcount_ptr<ProtoMap> pmap = SafeAlloc::MakeRefCounted<ProtoMap>(Hashes.ToHashedString("new"), registrator);
            pmap->SetSize({GameSettings::DEFAULT_MAP_SIZE, GameSettings::DEFAULT_MAP_SIZE});

            ptr<ClientEngine> engine = this;
            auto map = SafeAlloc::MakeRefCounted<MapView>(engine, ident_t {}, pmap.as_ptr(), GetApp()->MainWindow.GetSize());
            map->EnableMapperMode();
            map->SetScrollCheck(false);
            map->InstantScrollTo({GameSettings::DEFAULT_MAP_SIZE / 2, GameSettings::DEFAULT_MAP_SIZE / 2});

            LoadedMaps.emplace_back(std::move(map));
            ShowMap(LoadedMaps.back().as_ptr());
            SetMapDirty(LoadedMaps.back().as_ptr());
            AddMess("Create map success");
        }
        else if (command_ext == "unload" && _curMap) {
            AddMess("Unload map");

            auto cur_map = GetCurMapPtr();
            UnloadMap(cur_map);

            if (!LoadedMaps.empty()) {
                ShowMap(LoadedMaps.front().as_ptr());
            }
        }
        else if (command_ext == "size" && _curMap) {
            AddMess("Resize map");

            int32_t maxhx = 0;
            int32_t maxhy = 0;

            if (!(icommand >> maxhx >> maxhy)) {
                AddMess("Invalid args");
                return;
            }

            auto cur_map = GetCurMapPtr();
            ResizeMap(cur_map, maxhx, maxhy);
        }
        else if (command_ext == "resave") {
            AddMess("Resave maps");

            auto map_files = MapsFileSys.FilterFiles("fomap");

            for (const auto& map_file_header : map_files) {
                const auto map_name = map_file_header.GetNameNoExt();

                if (nptr<MapView> nullable_map = LoadMap(map_name); nullable_map) {
                    auto map = nullable_map.as_ptr();
                    SaveMap(map, map_name);
                    AddMess(strex("Resave map: {}", map_name));
                }
                else {
                    AddMess(strex("Failed to load map: {}", map_name));
                }
            }
        }
        else if (command_ext == "reverse-light" && _curMap) {
            const auto before_snapshot = !UndoRedoInProgress ? CaptureMapSnapshot(GetCurMap()) : string {};
            auto cur_map = GetCurMapPtr();

            span<refcount_ptr<ItemHexView>> items = cur_map->GetItems();

            for (size_t i = 0; i < items.size(); i++) {
                auto item = items[i].as_ptr();
                auto rgba = item->GetLightColor().rgba;
                rgba = (rgba & 0xFF000000) | ((rgba & 0xFF) << 16) | (rgba & 0xFF00) | ((rgba & 0xFF0000) >> 16);
                item->SetLightColor(ucolor(rgba));
            }

            cur_map->RebuildMap();
            SetMapDirty(GetCurMap());

            if (!before_snapshot.empty()) {
                const auto after_snapshot = CaptureMapSnapshot(GetCurMap());
                if (before_snapshot != after_snapshot) {
                    const string map_name = string(cur_map->GetName());
                    PushUndoOp(GetCurMap(), UndoOp {"Reverse lights", [map_name, before_snapshot](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) { return mapper->RestoreMapSnapshot(active_map, map_name, before_snapshot); }, [map_name, after_snapshot](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) { return mapper->RestoreMapSnapshot(active_map, map_name, after_snapshot); }, true});
                }
            }
        }
        else if (command_ext == "merge-items" && _curMap) {
            const auto before_snapshot = !UndoRedoInProgress ? CaptureMapSnapshot(GetCurMap()) : string {};
            auto cur_map = GetCurMapPtr();
            MergeItemsToMultihexMeshes(cur_map);
            const auto merge_items_repeat_count = MergeItemsToMultihexMeshes(cur_map);
            FO_VERIFY_AND_THROW(merge_items_repeat_count == 0, "Mapper merge-items command is not idempotent for current map", cur_map->GetName(), merge_items_repeat_count);
            SetMapDirty(GetCurMap());

            if (!before_snapshot.empty()) {
                const auto after_snapshot = CaptureMapSnapshot(GetCurMap());
                if (before_snapshot != after_snapshot) {
                    const string map_name = string(cur_map->GetName());
                    PushUndoOp(GetCurMap(), UndoOp {"Merge items", [map_name, before_snapshot](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) { return mapper->RestoreMapSnapshot(active_map, map_name, before_snapshot); }, [map_name, after_snapshot](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) { return mapper->RestoreMapSnapshot(active_map, map_name, after_snapshot); }, true});
                }
            }
        }
        else if (command_ext == "break-items" && _curMap) {
            const auto before_snapshot = !UndoRedoInProgress ? CaptureMapSnapshot(GetCurMap()) : string {};
            auto cur_map = GetCurMapPtr();
            BreakItemsMultihexMeshes(cur_map);
            const auto break_items_repeat_count = BreakItemsMultihexMeshes(cur_map);
            FO_VERIFY_AND_THROW(break_items_repeat_count == 0, "Mapper break-items command is not idempotent for current map", cur_map->GetName(), break_items_repeat_count);
            SetMapDirty(GetCurMap());

            if (!before_snapshot.empty()) {
                const auto after_snapshot = CaptureMapSnapshot(GetCurMap());
                if (before_snapshot != after_snapshot) {
                    const string map_name = string(cur_map->GetName());
                    PushUndoOp(GetCurMap(), UndoOp {"Break items", [map_name, before_snapshot](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) { return mapper->RestoreMapSnapshot(active_map, map_name, before_snapshot); }, [map_name, after_snapshot](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) { return mapper->RestoreMapSnapshot(active_map, map_name, after_snapshot); }, true});
                }
            }
        }
    }
    else {
        AddMess("Unknown command");
    }
}

auto MapperEngine::LoadMapFromText(string_view map_name, const string& map_text) -> nptr<MapView>
{
    FO_STACK_TRACE_ENTRY();

    const auto map_data = ConfigFile(strex("{}.fomap", map_name), map_text, ConfigFileOption::ReadFirstSection);

    if (!map_data.HasSection("ProtoMap")) {
        throw MapLoaderException("Invalid map format", map_name);
    }

    const auto& proto_map_section = map_data.GetSection("ProtoMap");
    map<string, string> proto_map_header_extra_fields;

    for (const auto& [key, value] : proto_map_section) {
        if (key.starts_with("$Text")) {
            proto_map_header_extra_fields.emplace(string {key}, string {value});
        }
    }

    auto nullable_registrator = GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME);
    FO_VERIFY_AND_THROW(nullable_registrator, "Map property registrator is not available");
    auto registrator = nullable_registrator.as_ptr();

    refcount_ptr<ProtoMap> pmap = SafeAlloc::MakeRefCounted<ProtoMap>(Hashes.ToHashedString(map_name), registrator);
    pmap->GetPropertiesForEdit()->ApplyFromText(proto_map_section);

    ptr<ClientEngine> engine = this;
    auto new_map = SafeAlloc::MakeRefCounted<MapView>(engine, ident_t {}, pmap.as_ptr(), GetApp()->MainWindow.GetSize());
    new_map->SetHeaderExtraFields(std::move(proto_map_header_extra_fields));
    new_map->EnableMapperMode();
    new_map->SetScrollCheck(false);

    try {
        new_map->LoadFromFile(map_name, map_text);
    }
    catch (const MapLoaderException& ex) {
        AddMess(strex("Map truncated: {}", ex.what()));
        return nullptr;
    }

    auto new_map_ptr = new_map.as_ptr();
    MergeItemsToMultihexMeshes(new_map_ptr);
    const auto load_merge_repeat_count = MergeItemsToMultihexMeshes(new_map_ptr);
    FO_VERIFY_AND_THROW(load_merge_repeat_count == 0, "Loaded map merge-items normalization is not idempotent", map_name, load_merge_repeat_count);

    new_map->InstantScrollTo(new_map->GetWorkHex());
    OnEditMapLoad.Fire(new_map.as_ptr());
    LoadedMaps.emplace_back(std::move(new_map));
    auto loaded_map = LoadedMaps.back().as_nptr();

    auto undo_context = GetUndoContext(loaded_map, true).as_ptr();
    undo_context->CleanUndoDepth = 0;
    SetMapDirty(loaded_map, false);

    return loaded_map;
}

auto MapperEngine::LoadMap(string_view map_name) -> nptr<MapView>
{
    FO_STACK_TRACE_ENTRY();

    const auto map_files = MapsFileSys.FilterFiles("fomap");
    File map_file = map_files.FindFileByName(map_name);

    if (!map_file) {
        string map_path = string(map_name);

        if (!strvex(map_path).ends_with(".fomap")) {
            map_path = strex("{}.fomap", map_path).str();
        }

        map_path = strex(map_path).format_path().str();
        map_file = map_files.FindFileByPath(map_path);
    }

    if (!map_file) {
        AddMess("Map file not found");
        return nullptr;
    }

    return LoadMapFromText(map_name, map_file.GetStr());
}

void MapperEngine::ShowMap(ptr<MapView> map)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Mapper cannot show a destroyed map", map->GetName(), LoadedMaps.size());

    const auto it = std::ranges::find_if(LoadedMaps, [map](const refcount_ptr<MapView>& loaded_map) noexcept {
        auto loaded_map_view = loaded_map.as_nptr();
        nptr<const MapView> target_map = map;
        return loaded_map_view == target_map;
    });
    FO_VERIFY_AND_THROW(it != LoadedMaps.end(), "Mapper show requested for a map that is not tracked as loaded", map->GetName(), LoadedMaps.size());

    WorkspaceWindowVisible = true;
    MapListWindowVisible = false;

    nptr<MapView> map_view = map;

    if (!(GetCurMap() == map_view)) {
        if (_curMap) {
            SelectClear();
        }

        _curMap = map.hold_ref();
        ClearMapperTrackOverlay();
        RefreshActiveProtoLists();
    }
}

auto MapperEngine::IsMapDirty(nptr<MapView> nullable_map) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_map) {
        return false;
    }

    auto map = nullable_map.as_ptr();
    return DirtyMaps.contains(map);
}

void MapperEngine::SetMapDirty(nptr<MapView> nullable_map, bool dirty)
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_map) {
        return;
    }

    auto map = nullable_map.as_ptr();

    if (dirty) {
        DirtyMaps.emplace(map);
    }
    else {
        DirtyMaps.erase(map);
    }
}

void MapperEngine::SaveCurrentMap()
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return;
    }

    auto cur_map = GetCurMapPtr();
    SaveMap(cur_map, "");
    AddMess(strex("Saved map: {}", cur_map->GetName()));
}

void MapperEngine::ResetCurrentMapChanges()
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return;
    }

    auto cur_map = GetCurMapPtr();
    const string map_name = string(cur_map->GetName());
    ClearUndoContext(cur_map);
    UnloadMap(cur_map);

    if (nptr<MapView> nullable_map = LoadMap(map_name); nullable_map) {
        auto map = nullable_map.as_ptr();
        ShowMap(map);
        AddMess(strex("Reset changes: {}", map_name));
    }
    else {
        AddMess(strex("Reset changes failed: {}", map_name));
    }
}

void MapperEngine::SaveMap(ptr<MapView> map, string_view custom_name)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Mapper cannot save a destroyed map", map->GetName(), custom_name);

    MergeItemsToMultihexMeshes(map);
    const auto save_merge_repeat_count = MergeItemsToMultihexMeshes(map);
    FO_VERIFY_AND_THROW(save_merge_repeat_count == 0, "Map save merge-items normalization is not idempotent", map->GetName(), custom_name, save_merge_repeat_count);

    const auto it = std::ranges::find_if(LoadedMaps, [map](const refcount_ptr<MapView>& loaded_map) noexcept {
        auto loaded_map_view = loaded_map.as_nptr();
        nptr<const MapView> target_map = map;
        return loaded_map_view == target_map;
    });
    FO_VERIFY_AND_THROW(it != LoadedMaps.end(), "Mapper save requested for a map that is not tracked as loaded", map->GetName(), custom_name, LoadedMaps.size());

    const auto fomap_content = map->SaveToText();

    const auto fomap_name = !custom_name.empty() ? custom_name : map->GetProto()->GetName();
    FO_VERIFY_AND_THROW(!fomap_name.empty(), "Mapper cannot determine a .fomap file name for saving", map->GetName(), custom_name, map->GetProto()->GetName());

    string fomap_path;
    const auto fomap_files = MapsFileSys.FilterFiles("fomap");

    if (const auto fomap_file = fomap_files.FindFileByName(fomap_name)) {
        fomap_path = fomap_file.GetDiskPath();
    }
    else if (const auto fomap_file2 = fomap_files.FindFileByName(map->GetProto()->GetName())) {
        fomap_path = strex(fomap_file2.GetDiskPath()).change_file_name(fomap_name);
    }
    else if (fomap_files.GetFilesCount() != 0) {
        fomap_path = strex(fomap_files.GetFileByIndex(0).GetDiskPath()).change_file_name(fomap_name);
    }
    else {
        fomap_path = strex("{}.fomap", fomap_path).format_path();
    }

    const auto dir = strex(fomap_path).extract_dir().str();

    if (!dir.empty()) {
        const auto dir_ok = fs_create_directories(dir);
        FO_VERIFY_AND_THROW(dir_ok, "Mapper failed to create .fomap output directory", dir, fomap_path, fomap_name);
    }

    std::ofstream fomap_file {std::filesystem::path {fs_make_path(fomap_path)}, std::ios::binary | std::ios::trunc};
    FO_VERIFY_AND_THROW(fomap_file, "Mapper failed to open .fomap file for writing", fomap_path, fomap_name, fomap_content.size());
    if (!fomap_content.empty()) {
        ptr<const char> fomap_data = fomap_content.data();
        fomap_file.write(fomap_data.get(), static_cast<std::streamsize>(fomap_content.size()));
    }
    FO_VERIFY_AND_THROW(fomap_file, "Mapper failed to write .fomap content", fomap_path, fomap_name, fomap_content.size());

    OnEditMapSave.Fire(map);
    auto ctx = GetUndoContext(map, true).as_ptr();
    ctx->CleanUndoDepth = numeric_cast<int32_t>(ctx->UndoStack.size());
    SetMapDirty(map, false);
}

void MapperEngine::SaveMapToDir(ptr<MapView> map, string_view sub_dir, string_view name)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Cannot save a destroyed map");
    FO_VERIFY_AND_THROW(!name.empty(), "Map save name is empty");

    MergeItemsToMultihexMeshes(map);
    FO_VERIFY_AND_THROW(MergeItemsToMultihexMeshes(map) == 0, "Failed to merge items to multihex meshes before save");

    const auto it = std::ranges::find_if(LoadedMaps, [map](const refcount_ptr<MapView>& loaded_map) noexcept {
        auto loaded_map_view = loaded_map.as_nptr();
        nptr<const MapView> target_map = map;
        return loaded_map_view == target_map;
    });
    FO_VERIFY_AND_THROW(it != LoadedMaps.end(), "Map to save is not in the loaded maps list");

    const auto fomap_content = map->SaveToText();

    // Resolve the on-disk Maps root from an existing fomap disk path, then write the new map
    // under <MapsRoot>/<sub_dir>/<name>.fomap. The AI authoring loop targets a checked-in
    // Maps/Generated/ area, so this avoids SaveMap's "first file's directory" fallback that
    // could scatter generated maps next to unrelated content.
    const auto fomap_files = MapsFileSys.FilterFiles("fomap");
    FO_VERIFY_AND_THROW(fomap_files.GetFilesCount() != 0, "No fomap file found to resolve the Maps root directory");

    string maps_root = strex(fomap_files.GetFileByIndex(0).GetDiskPath()).extract_dir().str();
    std::ranges::replace(maps_root, '\\', '/');

    if (const auto pos = maps_root.rfind("/Maps/"); pos != string::npos) {
        maps_root = maps_root.substr(0, pos + 5); // keep ".../Maps"
    }
    // else: best-effort fallback keeps the reference file's directory (incl. a path already ending in /Maps)

    const string target_dir = !sub_dir.empty() ? strex("{}/{}", maps_root, sub_dir).str() : maps_root;
    const string fomap_path = strex("{}/{}.fomap", target_dir, name).format_path().str();

    const auto dir = strex(fomap_path).extract_dir().str();

    if (!dir.empty()) {
        const auto dir_ok = fs_create_directories(dir);
        FO_VERIFY_AND_THROW(dir_ok, "Unable to create the target map directory", dir);
    }

    std::ofstream fomap_file {std::filesystem::path {fs_make_path(fomap_path)}, std::ios::binary | std::ios::trunc};
    FO_VERIFY_AND_THROW(fomap_file, "Unable to open the fomap file for writing", fomap_path);

    if (!fomap_content.empty()) {
        ptr<const char> fomap_data = fomap_content.data();
        fomap_file.write(fomap_data.get(), static_cast<std::streamsize>(fomap_content.size()));
    }

    FO_VERIFY_AND_THROW(fomap_file, "Unable to write the fomap file", fomap_path);

    OnEditMapSave.Fire(map);
    auto ctx = GetUndoContext(map, true).as_ptr();
    ctx->CleanUndoDepth = numeric_cast<int32_t>(ctx->UndoStack.size());
    SetMapDirty(map, false);
}

void MapperEngine::UnloadMap(ptr<MapView> map, bool clear_undo)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Mapper cannot unload a destroyed map", map->GetName(), clear_undo);

    nptr<MapView> map_view = map;

    if (GetCurMap() == map_view) {
        SelectClear();
        ClearMapperTrackOverlay();
        _curMap.reset();
    }

    const auto it = std::ranges::find_if(LoadedMaps, [map](const refcount_ptr<MapView>& loaded_map) noexcept {
        auto loaded_map_view = loaded_map.as_nptr();
        nptr<const MapView> target_map = map;
        return loaded_map_view == target_map;
    });
    FO_VERIFY_AND_THROW(it != LoadedMaps.end(), "Mapper unload requested for a map that is not tracked as loaded", map->GetName(), LoadedMaps.size(), clear_undo);

    SetMapDirty(map, false);

    if (clear_undo) {
        ClearUndoContext(map);
    }

    map->DestroySelf();
    LoadedMaps.erase(it);
}

void MapperEngine::ResizeMap(ptr<MapView> map, int32_t width, int32_t height)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

    const auto before_snapshot = !UndoRedoInProgress ? CaptureMapSnapshot(map) : string {};

    const auto corrected_width = std::clamp(width, GameSettings::MIN_MAP_SIZE, GameSettings::MAX_MAP_SIZE);
    const auto corrected_height = std::clamp(height, GameSettings::MIN_MAP_SIZE, GameSettings::MAX_MAP_SIZE);

    map->Resize(msize(numeric_cast<int16_t>(corrected_width), numeric_cast<int16_t>(corrected_height)));
    map->InstantScrollTo(map->GetWorkHex());

    nptr<MapView> map_view = map;

    if (GetCurMap() == map_view) {
        SelectClear();
        ClearMapperTrackOverlay();
    }

    SetMapDirty(map);

    const auto after_snapshot = CaptureMapSnapshot(map);

    if (!before_snapshot.empty() && before_snapshot != after_snapshot) {
        PushUndoOp(map, UndoOp {"Resize map", [map_name = string(map->GetName()), before_snapshot](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) { return mapper->RestoreMapSnapshot(active_map, map_name, before_snapshot); }, [map_name = string(map->GetName()), after_snapshot](ptr<MapperEngine> mapper, ptr<ptr<MapView>> active_map) { return mapper->RestoreMapSnapshot(active_map, map_name, after_snapshot); }, true});
    }
}

void MapperEngine::AddMess(string_view message_text)
{
    FO_STACK_TRACE_ENTRY();

    const string str = strex("- {}\n", message_text);
    const auto time = nanotime::now().desc(true);
    const string mess_time = strex("{:02}:{:02}:{:02} ", time.hour, time.minute, time.second);

    MessBox.emplace_back(MessBoxMessage {.Type = 0, .Mess = str, .Time = mess_time});
    MessBoxScroll = 0;
    MessBoxCurText = "";

    const irect32 ir(MainPanelContentRect.x + MainPanelPos.x, MainPanelContentRect.y + MainPanelPos.y, MainPanelContentRect.width, MainPanelContentRect.height);
    int32_t max_lines = ir.height / 10;

    if (ir == irect32()) {
        max_lines = 20;
    }

    int32_t cur_mess = numeric_cast<int32_t>(MessBox.size()) - 1;

    for (int32_t i = 0, j = 0; cur_mess >= 0; cur_mess--) {
        MessBoxMessage& m = MessBox[cur_mess];

        // Scroll
        if (++j <= MessBoxScroll) {
            continue;
        }

        // Add to message box
        MessBoxCurText = m.Mess + MessBoxCurText;

        if (++i >= max_lines) {
            break;
        }
    }
}

void MapperEngine::MessBoxDraw()
{
    FO_STACK_TRACE_ENTRY();

    if (MessBoxCurText.empty()) {
        return;
    }

    DrawStr(irect32(MainPanelContentRect.x + MainPanelPos.x, MainPanelContentRect.y + MainPanelPos.y, MainPanelContentRect.width, MainPanelContentRect.height), MessBoxCurText, Color::TextWhite, TextFormat {.Font = FONT_OLD_DEFAULT, .Flags = CombineEnum(FontFlag::KeepTail, FontFlag::AlignBottom)});
}

auto MapperEngine::GetEntityInnerItems(ptr<ClientEntity> entity) const -> vector<refcount_ptr<ItemView>>
{
    FO_STACK_TRACE_ENTRY();

    if (nptr<CritterView> nullable_cr = entity.dyn_cast<CritterView>()) {
        auto cr = nullable_cr.as_ptr();
        span<refcount_ptr<ItemView>> items = cr->GetInvItems();
        return {items.begin(), items.end()};
    }
    if (nptr<ItemView> nullable_item = entity.dyn_cast<ItemView>()) {
        auto item = nullable_item.as_ptr();
        span<refcount_ptr<ItemView>> items = item->GetInnerItems();
        return {items.begin(), items.end()};
    }

    return {};
}

FO_END_NAMESPACE
