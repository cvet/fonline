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
#include "AnyData.h"
#include "ConfigFile.h"
#include "DefaultSprites.h"
#include "ImGuiStuff.h"
#include "MetadataRegistration.h"
#include "ParticleSprites.h"

FO_BEGIN_NAMESPACE

static constexpr ipos32 MAPPER_CONSOLE_WINDOW_OFFSET = {0, 6};
static constexpr string_view MAPPER_IMGUI_SETTINGS_CACHE_ENTRY = "MapperImGui.ini";
static constexpr int32 DAY_TIME_WRAP_MINUTES = 1440;
static constexpr int32 DAY_TIME_VISIBLE_UPPER_BOUND = DAY_TIME_WRAP_MINUTES * 2;

static auto MakeRectFromEdges(int32 left, int32 top, int32 right, int32 bottom) -> irect32
{
    FO_STACK_TRACE_ENTRY();

    return {left, top, right - left, bottom - top};
}

static auto ShiftDayTimeWithWrap(int32 day_time, int32 delta_minutes) -> int32
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

static auto ScaleZoomValue(float32 current_zoom, float32 factor) -> float32
{
    FO_STACK_TRACE_ENTRY();

    return std::clamp(current_zoom * factor, GameSettings::MIN_ZOOM, GameSettings::MAX_ZOOM);
}

static auto GetTileLayerFromKey(KeyCode key) -> optional<int32>
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

static auto GetNextCritterDir(uint8 dir) -> uint8
{
    FO_STACK_TRACE_ENTRY();

    return numeric_cast<uint8>((dir + 1) % GameSettings::MAP_DIR_COUNT);
}

template<size_t Size>
static auto InputBufferView(const array<char, Size>& buffer) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    const auto end = std::find(buffer.begin(), buffer.end(), '\0');
    return {buffer.data(), numeric_cast<size_t>(std::distance(buffer.begin(), end))};
}

static void AdvanceCritterDir(CritterHexView* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr);
    cr->ChangeDir(GetNextCritterDir(cr->GetDir()));
}

static void ToggleMapVisibilityFlag(raw_ptr<MapView> map, bool& value)
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

    auto lower_text = string(text);
    auto lower_filter = string(filter);

    std::ranges::transform(lower_text, lower_text.begin(), [](char c) { return numeric_cast<char>(std::tolower(numeric_cast<unsigned char>(c))); });
    std::ranges::transform(lower_filter, lower_filter.begin(), [](char c) { return numeric_cast<char>(std::tolower(numeric_cast<unsigned char>(c))); });

    return lower_text.find(lower_filter) != string::npos;
}

static auto ResolveAtlasSprite(const Sprite* sprite) -> const AtlasSprite*
{
    FO_STACK_TRACE_ENTRY();

    if (const auto* sprite_sheet = dynamic_cast<const SpriteSheet*>(sprite); sprite_sheet != nullptr) {
        sprite = sprite_sheet->GetCurSpr();
    }

    return dynamic_cast<const AtlasSprite*>(sprite);
}

static auto GetInspectorValueType(const Property* prop) -> AnyData::ValueType
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

static auto ParseInspectorValue(const Property* prop, string_view text) -> optional<AnyData::Value>
{
    FO_STACK_TRACE_ENTRY();

    try {
        return AnyData::ParseValue(string(text), false, prop->IsArray(), GetInspectorValueType(prop));
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

static auto MakeDefaultInspectorArrayElement(const Property* prop) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    switch (GetInspectorValueType(prop)) {
    case AnyData::ValueType::Int64:
        return AnyData::Value {int64 {0}};
    case AnyData::ValueType::Float64:
        return AnyData::Value {float64 {0.0}};
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

static auto GetInspectorStructLayout(const Property* prop) -> const StructLayoutDesc*
{
    FO_STACK_TRACE_ENTRY();

    const auto& base_type = prop->GetBaseType();
    if (base_type.StructLayout != nullptr && (base_type.IsComplexStruct || base_type.IsSimpleStruct) && base_type.StructLayout->Fields.size() > 1) {
        return base_type.StructLayout.get();
    }

    return nullptr;
}

static auto ReadInspectorToken(const char* str, string& result) -> const char*
{
    FO_STACK_TRACE_ENTRY();

    if (str[0] == 0) {
        return nullptr;
    }

    size_t pos = 0;
    size_t length = utf8::DecodeStrNtLen(&str[pos]);
    utf8::Decode(&str[pos], length);

    while (length == 1 && (str[pos] == ' ' || str[pos] == '\t')) {
        pos++;

        length = utf8::DecodeStrNtLen(&str[pos]);
        utf8::Decode(&str[pos], length);
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
                    length = utf8::DecodeStrNtLen(&str[pos]);
                    utf8::Decode(&str[pos], length);
                    pos += length;
                }
            }
            else if (length == 1 && str[pos] == '"') {
                break;
            }
            else {
                pos += length;
            }

            length = utf8::DecodeStrNtLen(&str[pos]);
            utf8::Decode(&str[pos], length);
        }
    }
    else {
        begin = pos;

        while (str[pos] != 0) {
            if (length == 1 && str[pos] == '\\') {
                pos++;

                length = utf8::DecodeStrNtLen(&str[pos]);
                utf8::Decode(&str[pos], length);
                pos += length;
            }
            else if (length == 1 && (str[pos] == ' ' || str[pos] == '\t')) {
                break;
            }
            else {
                pos += length;
            }

            length = utf8::DecodeStrNtLen(&str[pos]);
            utf8::Decode(&str[pos], length);
        }
    }

    result.assign(&str[begin], pos - begin);
    return str[pos] != 0 ? &str[pos + 1] : &str[pos];
}

static auto ParseInspectorStructFields(const StructLayoutDesc& layout, string_view text) -> optional<vector<string>>
{
    FO_STACK_TRACE_ENTRY();

    try {
        const auto text_str = string {text};
        const char* token_pos = text_str.c_str();
        string token;
        vector<string> fields;
        fields.reserve(layout.Fields.size());

        while ((token_pos = ReadInspectorToken(token_pos, token)) != nullptr) {
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
    string* Value {};
    bool LatinOnly {};
    bool MoveCaretToEnd {};
};

static int ImGuiInputTextStringCallback(ImGuiInputTextCallbackData* data)
{
    FO_STACK_TRACE_ENTRY();

    auto* user_data = static_cast<ImGuiInputTextStringUserData*>(data->UserData);
    IM_ASSERT(user_data != nullptr);

    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = user_data->Value;
        IM_ASSERT(str != nullptr);
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    else if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        if (user_data->LatinOnly && data->EventChar >= 128) {
            return 1;
        }
    }
    else if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
        if (user_data->MoveCaretToEnd) {
            data->CursorPos = data->BufTextLen;
            data->SelectionStart = data->BufTextLen;
            data->SelectionEnd = data->BufTextLen;
            user_data->MoveCaretToEnd = false;
        }
    }

    return 0;
}

static auto ImGuiInputTextString(const char* label, string& value, ImGuiInputTextFlags flags = 0, bool latin_only = false, bool move_caret_to_end = false) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (value.capacity() == 0) {
        value.reserve(256);
    }

    ImGuiInputTextStringUserData user_data {.Value = &value, .LatinOnly = latin_only, .MoveCaretToEnd = move_caret_to_end};
    flags |= ImGuiInputTextFlags_CallbackResize;
    if (latin_only) {
        flags |= ImGuiInputTextFlags_CallbackCharFilter;
    }
    if (move_caret_to_end) {
        flags |= ImGuiInputTextFlags_CallbackAlways;
    }

    return ImGui::InputText(label, value.data(), value.capacity() + 1, flags, ImGuiInputTextStringCallback, &user_data);
}

static auto IsInspectorValueSameAsProto(const Entity* entity, const Property* prop, string_view value_text) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto* entity_with_proto = dynamic_cast<const EntityWithProto*>(entity);
    if (entity_with_proto == nullptr) {
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

        for (const auto& [section_name, key_values] : sections) {
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

MapperEngine::MapperEngine(GlobalSettings& settings, FileSystem&& resources, AppWindow* window) :
    ClientEngine(settings, std::move(resources), window, [&] { RegisterMapperMetadata(this, &resources); })
{
    FO_STACK_TRACE_ENTRY();

    App->LoadImGuiEffect(Resources);

    MapsFileSys.AddDirSource("", false, true, true);

    for (const auto& res_pack : settings.GetResourcePacks()) {
        for (const auto& dir : res_pack.InputDirs) {
            MapsFileSys.AddDirSource(dir, false, true, true);
        }
    }

    EffectMngr.LoadDefaultEffects();

    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<DefaultSpriteFactory>(SprMngr));
    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<ParticleSpriteFactory>(SprMngr, Settings, EffectMngr, GameTime, Hashes));
#if FO_ENABLE_3D
    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<ModelSpriteFactory>(SprMngr, Settings, EffectMngr, GameTime, Hashes, *this, *this));
#endif

    ResMngr.IndexFiles();

    MapEngineType<PlayerView>(EngineMetadata::GetBaseType(PlayerView::ENTITY_TYPE_NAME));
    MapEngineType<ItemView>(EngineMetadata::GetBaseType(ItemView::ENTITY_TYPE_NAME));
    MapEngineType<ItemHexView>(EngineMetadata::GetBaseType(ItemView::ENTITY_TYPE_NAME));
    MapEngineType<CritterView>(EngineMetadata::GetBaseType(CritterView::ENTITY_TYPE_NAME));
    MapEngineType<CritterHexView>(EngineMetadata::GetBaseType(CritterView::ENTITY_TYPE_NAME));
    MapEngineType<MapView>(EngineMetadata::GetBaseType(MapView::ENTITY_TYPE_NAME));
    MapEngineType<LocationView>(EngineMetadata::GetBaseType(LocationView::ENTITY_TYPE_NAME));

    InitSubsystems(this);

    _curLang = LanguagePack {Settings.Language, *this};
    _curLang.LoadFromResources(Resources);

    // Fonts
    auto load_fonts_ok = true;

    if (!SprMngr.LoadFontFO(FONT_FO, "OldDefault", AtlasType::IfaceSprites, false, true) || //
        !SprMngr.LoadFontFO(FONT_NUM, "Numbers", AtlasType::IfaceSprites, true, true) || //
        !SprMngr.LoadFontFO(FONT_BIG_NUM, "BigNumbers", AtlasType::IfaceSprites, true, true) || //
        !SprMngr.LoadFontFO(FONT_SAND_NUM, "SandNumbers", AtlasType::IfaceSprites, false, true) || //
        !SprMngr.LoadFontFO(FONT_SPECIAL, "Special", AtlasType::IfaceSprites, false, true) || //
        !SprMngr.LoadFontFO(FONT_DEFAULT, "Default", AtlasType::IfaceSprites, false, true) || //
        !SprMngr.LoadFontFO(FONT_THIN, "Thin", AtlasType::IfaceSprites, false, true) || //
        !SprMngr.LoadFontFO(FONT_FAT, "Fat", AtlasType::IfaceSprites, false, true) || //
        !SprMngr.LoadFontFO(FONT_BIG, "Big", AtlasType::IfaceSprites, false, true)) {
        load_fonts_ok = false;
    }

    FO_RUNTIME_ASSERT(load_fonts_ok);
    SprMngr.SetDefaultFont(FONT_DEFAULT);

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

    if (!Settings.StartMap.empty()) {
        auto* map = LoadMap(Settings.StartMap);

        if (map != nullptr) {
            if (Settings.StartHexX > 0 && Settings.StartHexY > 0) {
                _curMap->InstantScrollTo(_curMap->GetSize().from_raw_pos(Settings.StartHexX, Settings.StartHexY));
            }

            ShowMap(map);
        }
    }

    // Refresh resources after start script executed
    RefreshActiveProtoLists();

    if (const auto imgui_ini = Cache.GetString(MAPPER_IMGUI_SETTINGS_CACHE_ENTRY); !imgui_ini.empty()) {
        ImGui::LoadIniSettingsFromMemory(imgui_ini.c_str(), imgui_ini.size());
        ImGui::GetIO().WantSaveIniSettings = false;
    }

    // Load console history
    const auto history_str = Cache.GetString("mapper_console.txt");
    ConsoleHistory = strex(history_str).normalize_line_endings().split('\n');

    while (numeric_cast<int32>(ConsoleHistory.size()) > Settings.ConsoleHistorySize) {
        ConsoleHistory.erase(ConsoleHistory.begin());
    }

    ConsoleHistoryCur = numeric_cast<int32>(ConsoleHistory.size());
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
    CritterDir = 3;
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

auto MapperEngine::GetPreviewSprite(hstring fname) -> Sprite*
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = PreviewSprites.find(fname); it != PreviewSprites.end()) {
        return it->second.get();
    }

    auto spr = SprMngr.LoadSprite(fname, AtlasType::IfaceSprites);

    if (spr) {
        spr->PlayDefault();
    }

    return PreviewSprites.emplace(fname, std::move(spr)).first->second.get();
}

void MapperEngine::MapperMainLoop()
{
    FO_STACK_TRACE_ENTRY();

    FrameAdvance();

    OnLoop.Fire();

    BeginMapperFrameInput();

    if (_curMap) {
        _curMap->Process();
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
        if (const auto* ini_data = ImGui::SaveIniSettingsToMemory(&ini_size); ini_data != nullptr) {
            Cache.SetString(MAPPER_IMGUI_SETTINGS_CACHE_ENTRY, string_view(ini_data, ini_size));
        }
        io.WantSaveIniSettings = false;
    }
}

auto MapperEngine::BeginMapperFrameInput() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (const bool is_fullscreen = SprMngr.IsFullscreen(); (is_fullscreen && Settings.FullscreenMouseScroll) || (!is_fullscreen && Settings.WindowedMouseScroll)) {
        Settings.ScrollMouseRight = Settings.MousePos.x >= Settings.ScreenWidth - 1;
        Settings.ScrollMouseLeft = Settings.MousePos.x <= 0;
        Settings.ScrollMouseDown = Settings.MousePos.y >= Settings.ScreenHeight - 1;
        Settings.ScrollMouseUp = Settings.MousePos.y <= 0;
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
    while (App->Input.PollEvent(ev)) {
        ProcessInputEvent(ev);
        ProcessMapperInputEvent(ev);
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
            const auto cur_zoom = _curMap->GetSpritesZoomTarget();
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
        if (_curMap != nullptr && MouseHoldMode == INT_PAN) {
            const auto zoom = _curMap->GetSpritesZoom();
            const fpos32 pan_delta {-numeric_cast<float32>(ev.MouseMove.DeltaX) / zoom, -numeric_cast<float32>(ev.MouseMove.DeltaY) / zoom};
            const fpos32 screen_pan_delta {-numeric_cast<float32>(ev.MouseMove.DeltaX), -numeric_cast<float32>(ev.MouseMove.DeltaY)};

            if (ev.MouseMove.DeltaX != 0 || ev.MouseMove.DeltaY != 0) {
                RightMouseDragged = true;
                _curMap->InstantScroll(pan_delta);

                RightMouseVelocityAccum += screen_pan_delta;

                const auto now_time = nanotime::now();
                const auto sample_ms = (now_time - RightMouseVelocityTime).to_ms<float32>();
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

    {
        SprMngr.BeginScene();

        DrawIfaceLayer(0);

        if (_curMap) {
            _curMap->DrawMap();
        }

        DrawIfaceLayer(1);
        DrawMainPanelImGui();
        DrawConsoleImGui();
        DrawInspectorImGui();
        DrawIfaceLayer(2);

        DrawIfaceLayer(4);
        CurDraw();
        DrawIfaceLayer(5);

        SprMngr.EndScene();
    }
}

void MapperEngine::ProcessRightMouseInertia()
{
    FO_STACK_TRACE_ENTRY();

    if (_curMap == nullptr || MouseHoldMode == INT_PAN) {
        return;
    }

    if (RightMouseInertia.dist() < 30.0f) {
        RightMouseInertia = {};
        return;
    }

    const auto dt_ms = std::max(GameTime.GetFrameDeltaTime().to_ms<float32>(), 1.0f);
    const auto dt_sec = dt_ms / 1000.0f;
    const auto zoom = std::max(_curMap->GetSpritesZoom(), 0.001f);
    _curMap->InstantScroll((RightMouseInertia * dt_sec) / zoom);

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

    if (PendingSelectionMoveEntries.empty() || _curMap == nullptr) {
        ResetPendingSelectionMove();
        return;
    }

    auto move_entries = std::move(PendingSelectionMoveEntries);
    ResetPendingSelectionMove();

    PushUndoOp(_curMap.get(),
        UndoOp {"Move selection",
            [move_entries](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                for (const auto& entry : move_entries) {
                    auto* target = mapper.FindEntityById(active_map, entry.EntityId);
                    if (target == nullptr) {
                        return false;
                    }

                    if (auto* item = dynamic_cast<ItemHexView*>(target); item != nullptr) {
                        if (entry.HasOffset) {
                            item->SetOffset(entry.OldOffset);
                            item->RefreshAnim();
                        }
                        if (entry.HasHex) {
                            if (!entry.OldMultihexMesh.empty()) {
                                item->SetMultihexMesh(entry.OldMultihexMesh);
                            }
                            active_map->MoveItem(item, entry.OldHex);
                        }
                    }
                    else if (auto* cr = dynamic_cast<CritterHexView*>(target); cr != nullptr && entry.HasHex) {
                        active_map->MoveCritter(cr, entry.OldHex, false);
                    }
                }

                mapper.SetMapDirty(active_map.get());
                active_map->RebuildMap();
                return true;
            },
            [move_entries](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                for (const auto& entry : move_entries) {
                    auto* target = mapper.FindEntityById(active_map, entry.EntityId);
                    if (target == nullptr) {
                        return false;
                    }

                    if (auto* item = dynamic_cast<ItemHexView*>(target); item != nullptr) {
                        if (entry.HasOffset) {
                            item->SetOffset(entry.NewOffset);
                            item->RefreshAnim();
                        }
                        if (entry.HasHex) {
                            if (!entry.NewMultihexMesh.empty()) {
                                item->SetMultihexMesh(entry.NewMultihexMesh);
                            }
                            active_map->MoveItem(item, entry.NewHex);
                        }
                    }
                    else if (auto* cr = dynamic_cast<CritterHexView*>(target); cr != nullptr && entry.HasHex) {
                        active_map->MoveCritter(cr, entry.NewHex, false);
                    }
                }

                mapper.SetMapDirty(active_map.get());
                active_map->RebuildMap();
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
    if (dikdw != KeyCode::None && PressedKeys[static_cast<int32>(dikdw)]) {
        return;
    }
    if (dikup != KeyCode::None && !PressedKeys[static_cast<int32>(dikup)]) {
        return;
    }

    // Keyboard states, to know outside function
    PressedKeys[static_cast<int32>(dikup)] = false;
    PressedKeys[static_cast<int32>(dikdw)] = true;

    const auto block_hotkeys = IsImGuiTextInputActive();
    HandlePrimaryMapperHotkeys(dikdw, block_hotkeys);

    if (!block_hotkeys && !App->Input.IsAltDown() && !App->Input.IsCtrlDown() && !App->Input.IsShiftDown() && (dikdw == KeyCode::F11 || dikdw == KeyCode::F12)) {
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

    if (block_hotkeys || App->Input.IsAltDown() || App->Input.IsCtrlDown() || App->Input.IsShiftDown()) {
        return;
    }

    switch (dikdw) {
    case KeyCode::F1:
        ToggleMapVisibilityFlag(_curMap.get(), Settings.ShowItem);
        break;
    case KeyCode::F2:
        ToggleMapVisibilityFlag(_curMap.get(), Settings.ShowScen);
        break;
    case KeyCode::F3:
        ToggleMapVisibilityFlag(_curMap.get(), Settings.ShowWall);
        break;
    case KeyCode::F4:
        ToggleMapVisibilityFlag(_curMap.get(), Settings.ShowCrit);
        break;
    case KeyCode::F5:
        ToggleMapVisibilityFlag(_curMap.get(), Settings.ShowTile);
        break;
    case KeyCode::F6:
        ToggleMapVisibilityFlag(_curMap.get(), Settings.ShowFast);
        break;
    case KeyCode::F7:
        WorkspaceWindowVisible = !WorkspaceWindowVisible;
        break;
    case KeyCode::F8:
        if (SprMngr.IsFullscreen()) {
            Settings.FullscreenMouseScroll = !Settings.FullscreenMouseScroll;
        }
        else {
            Settings.WindowedMouseScroll = !Settings.WindowedMouseScroll;
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
        if (_curMap) {
            _curMap->SwitchShowHex();
        }
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

        if (!SelectedEntities.empty() || InContItem != nullptr) {
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

    if (block_hotkeys || !App->Input.IsShiftDown()) {
        return;
    }

    switch (dikdw) {
    case KeyCode::F7:
        ContentWindowVisible = !ContentWindowVisible;
        break;
    case KeyCode::F11:
        SprMngr.GetAtlasMngr().DumpAtlases();
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

    if (block_hotkeys || !App->Input.IsCtrlDown()) {
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
        Settings.ScrollCheck = !Settings.ScrollCheck;
        break;
    case KeyCode::B:
        if (_curMap) {
            _curMap->MarkBlockedHexes();
        }
        break;
    case KeyCode::Q:
        Settings.ShowCorners = !Settings.ShowCorners;
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
            Settings.ScrollKeybLeft = true;
            break;
        case KeyCode::Right:
            Settings.ScrollKeybRight = true;
            break;
        case KeyCode::Up:
            Settings.ScrollKeybUp = true;
            break;
        case KeyCode::Down:
            Settings.ScrollKeybDown = true;
            break;
        default:
            break;
        }
    }

    if (dikup != KeyCode::None) {
        switch (dikup) {
        case KeyCode::Left:
            Settings.ScrollKeybLeft = false;
            break;
        case KeyCode::Right:
            Settings.ScrollKeybRight = false;
            break;
        case KeyCode::Up:
            Settings.ScrollKeybUp = false;
            break;
        case KeyCode::Down:
            Settings.ScrollKeybDown = false;
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
        ConsoleHistoryCur = numeric_cast<int32>(ConsoleHistory.size());
    }
}

void MapperEngine::ChangeZoom(float32 new_zoom)
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return;
    }

    const fpos32 mouse_pos = fpos32(App->Input.GetMousePosition());
    const fsize32 screen_size = fsize32(_curMap->GetScreenSize());
    const float32 mouse_x_factor = std::clamp(mouse_pos.x / screen_size.width, 0.0f, 1.0f);
    const float32 mouse_y_factor = std::clamp(mouse_pos.y / screen_size.height, 0.0f, 1.0f);

    _curMap->ChangeZoom(new_zoom, {mouse_x_factor, mouse_y_factor});
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
    DirAngle(other.DirAngle),
    IsCritter(other.IsCritter),
    IsItem(other.IsItem),
    Slot(other.Slot),
    StackId(other.StackId),
    Proto(other.Proto),
    Props(other.Props ?
            [&]() {
                auto props = other.Props->Copy();
                return SafeAlloc::MakeUnique<Properties>(std::move(props));
            }() :
            nullptr)
{
    FO_STACK_TRACE_ENTRY();

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
        DirAngle = other.DirAngle;
        IsCritter = other.IsCritter;
        IsItem = other.IsItem;
        Slot = other.Slot;
        StackId = other.StackId;
        Proto = other.Proto;
        Props = other.Props ?
            [&]() {
                auto props = other.Props->Copy();
                return SafeAlloc::MakeUnique<Properties>(std::move(props));
            }() :
            nullptr;
        Children.clear();
        Children.reserve(other.Children.size());
        for (const auto& child : other.Children) {
            Children.emplace_back(SafeAlloc::MakeUnique<EntityBuf>(*child));
        }
    }

    return *this;
}

MapperEngine::UndoOp::UndoOp(string label, std::function<bool(MapperEngine&, raw_ptr<MapView>&)> undo, std::function<bool(MapperEngine&, raw_ptr<MapView>&)> redo, bool is_snapshot) :
    Label(std::move(label)),
    IsSnapshot(is_snapshot),
    Undo(std::move(undo)),
    Redo(std::move(redo))
{
    FO_STACK_TRACE_ENTRY();
}

auto MapperEngine::GetUndoContext(MapView* map, bool create) -> UndoContext*
{
    FO_STACK_TRACE_ENTRY();

    if (map == nullptr) {
        return nullptr;
    }

    if (!create) {
        if (const auto it = UndoContexts.find(map); it != UndoContexts.end()) {
            return &it->second;
        }

        return nullptr;
    }

    return &UndoContexts[map];
}

auto MapperEngine::GetUndoContext(const MapView* map, bool create) const -> const UndoContext*
{
    FO_STACK_TRACE_ENTRY();

    if (map == nullptr) {
        return nullptr;
    }

    if (!create) {
        if (const auto it = UndoContexts.find(const_cast<MapView*>(map)); it != UndoContexts.end()) {
            return &it->second;
        }
    }

    return nullptr;
}

void MapperEngine::ClearUndoContext(MapView* map)
{
    FO_STACK_TRACE_ENTRY();

    if (map == nullptr) {
        return;
    }

    UndoContexts.erase(map);
}

void MapperEngine::RemapUndoContext(MapView* old_map, MapView* new_map)
{
    FO_STACK_TRACE_ENTRY();

    if (old_map == nullptr || new_map == nullptr || old_map == new_map) {
        return;
    }

    if (const auto it = UndoContexts.find(old_map); it != UndoContexts.end()) {
        auto ctx = std::move(it->second);
        UndoContexts.erase(it);
        UndoContexts[new_map] = std::move(ctx);
    }
}

void MapperEngine::PushUndoOp(MapView* map, UndoOp op)
{
    FO_STACK_TRACE_ENTRY();

    if (UndoRedoInProgress || map == nullptr || !op.Undo || !op.Redo) {
        return;
    }

    auto* ctx = GetUndoContext(map, true);
    FO_RUNTIME_ASSERT(ctx);

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

    if (_curMap == nullptr) {
        return false;
    }

    if (const auto* ctx = GetUndoContext(_curMap.get(), false); ctx != nullptr) {
        return !ctx->UndoStack.empty();
    }

    return false;
}

auto MapperEngine::CanRedo() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_curMap == nullptr) {
        return false;
    }

    if (const auto* ctx = GetUndoContext(_curMap.get(), false); ctx != nullptr) {
        return !ctx->RedoStack.empty();
    }

    return false;
}

auto MapperEngine::GetUndoLabel() const -> string
{
    FO_STACK_TRACE_ENTRY();

    if (const auto* ctx = _curMap ? GetUndoContext(_curMap.get(), false) : nullptr; ctx != nullptr && !ctx->UndoStack.empty()) {
        return ctx->UndoStack.back().Label;
    }

    return {};
}

auto MapperEngine::GetRedoLabel() const -> string
{
    FO_STACK_TRACE_ENTRY();

    if (const auto* ctx = _curMap ? GetUndoContext(_curMap.get(), false) : nullptr; ctx != nullptr && !ctx->RedoStack.empty()) {
        return ctx->RedoStack.back().Label;
    }

    return {};
}

auto MapperEngine::ExecuteUndo() -> bool
{
    FO_STACK_TRACE_ENTRY();

    raw_ptr<MapView> map = _curMap.get();
    auto* ctx = GetUndoContext(map.get(), false);
    if (ctx == nullptr || ctx->UndoStack.empty()) {
        return false;
    }

    auto op = std::move(ctx->UndoStack.back());
    ctx->UndoStack.pop_back();

    raw_ptr<MapView> active_map = map;
    UndoRedoInProgress = true;
    const auto ok = op.Undo(*this, active_map);
    UndoRedoInProgress = false;

    if (!ok) {
        ctx = GetUndoContext(map.get(), true);
        ctx->UndoStack.emplace_back(std::move(op));
        return false;
    }

    ctx = GetUndoContext(active_map.get(), true);
    ctx->RedoStack.emplace_back(std::move(op));
    SetMapDirty(active_map.get(), ctx->CleanUndoDepth < 0 || numeric_cast<int32>(ctx->UndoStack.size()) != ctx->CleanUndoDepth);
    return true;
}

auto MapperEngine::ExecuteRedo() -> bool
{
    FO_STACK_TRACE_ENTRY();

    raw_ptr<MapView> map = _curMap.get();
    auto* ctx = GetUndoContext(map.get(), false);
    if (ctx == nullptr || ctx->RedoStack.empty()) {
        return false;
    }

    auto op = std::move(ctx->RedoStack.back());
    ctx->RedoStack.pop_back();

    raw_ptr<MapView> active_map = map;
    UndoRedoInProgress = true;
    const auto ok = op.Redo(*this, active_map);
    UndoRedoInProgress = false;

    if (!ok) {
        ctx = GetUndoContext(map.get(), true);
        ctx->RedoStack.emplace_back(std::move(op));
        return false;
    }

    ctx = GetUndoContext(active_map.get(), true);
    ctx->UndoStack.emplace_back(std::move(op));
    SetMapDirty(active_map.get(), ctx->CleanUndoDepth < 0 || numeric_cast<int32>(ctx->UndoStack.size()) != ctx->CleanUndoDepth);
    return true;
}

auto MapperEngine::CaptureMapSnapshot(MapView* map) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return map != nullptr ? map->SaveToText() : string {};
}

void MapperEngine::CaptureEntityBuf(EntityBuf& entity_buf, ClientEntity* entity) const
{
    FO_STACK_TRACE_ENTRY();

    entity_buf.Id = entity->GetId();
    entity_buf.IsCritter = dynamic_cast<CritterHexView*>(entity) != nullptr;
    entity_buf.IsItem = dynamic_cast<ItemView*>(entity) != nullptr;
    entity_buf.Proto = dynamic_cast<EntityWithProto*>(entity)->GetProto();
    entity_buf.Props = SafeAlloc::MakeUnique<Properties>(entity->GetProperties().Copy());

    if (const auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        entity_buf.Hex = cr->GetHex();
        entity_buf.DirAngle = numeric_cast<int16>(cr->GetDirAngle());
    }
    else if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        entity_buf.Hex = item->GetHex();
        entity_buf.Slot = item->GetCritterSlot();
        entity_buf.StackId = any_t {string(item->GetContainerStack())};
    }
    else if (const auto* inner_item = dynamic_cast<ItemView*>(entity); inner_item != nullptr) {
        entity_buf.Slot = inner_item->GetCritterSlot();
        entity_buf.StackId = any_t {string(inner_item->GetContainerStack())};
    }

    for (auto& child : GetEntityInnerItems(entity)) {
        auto child_buf = SafeAlloc::MakeUnique<EntityBuf>();
        CaptureEntityBuf(*child_buf, child.get());
        entity_buf.Children.emplace_back(std::move(child_buf));
    }
}

void MapperEngine::RestoreEntityBufChildren(const EntityBuf& entity_buf, ItemView* item)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& child_buf : entity_buf.Children) {
        auto* inner_item = item->AddMapperInnerItem(child_buf->Id, child_buf->Proto.dyn_cast<const ProtoItem>().get(), child_buf->StackId, child_buf->Props.get());
        RestoreEntityBufChildren(*child_buf, inner_item);
    }
}

auto MapperEngine::RestoreEntityBuf(const EntityBuf& entity_buf, Entity* owner) -> ClientEntity*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_curMap);

    if (owner == nullptr) {
        if (entity_buf.IsCritter) {
            auto* cr = _curMap->AddMapperCritter(entity_buf.Proto->GetProtoId(), entity_buf.Hex, entity_buf.DirAngle, entity_buf.Props.get(), entity_buf.Id);

            for (const auto& child_buf : entity_buf.Children) {
                auto* inv_item = cr->AddMapperInvItem(child_buf->Id, child_buf->Proto.dyn_cast<const ProtoItem>().get(), child_buf->Slot, child_buf->Props.get());
                RestoreEntityBufChildren(*child_buf, inv_item);
            }

            return cr;
        }

        if (entity_buf.IsItem) {
            auto* item = _curMap->AddMapperItem(entity_buf.Proto->GetProtoId(), entity_buf.Hex, entity_buf.Props.get(), entity_buf.Id);
            RestoreEntityBufChildren(entity_buf, item);
            return item;
        }

        return nullptr;
    }

    if (auto* cr = dynamic_cast<CritterView*>(owner); cr != nullptr) {
        auto* item = cr->AddMapperInvItem(entity_buf.Id, entity_buf.Proto.dyn_cast<const ProtoItem>().get(), entity_buf.Slot, entity_buf.Props.get());
        RestoreEntityBufChildren(entity_buf, item);
        return item;
    }

    if (auto* item_owner = dynamic_cast<ItemView*>(owner); item_owner != nullptr) {
        auto* item = item_owner->AddMapperInnerItem(entity_buf.Id, entity_buf.Proto.dyn_cast<const ProtoItem>().get(), entity_buf.StackId, entity_buf.Props.get());
        RestoreEntityBufChildren(entity_buf, item);
        return item;
    }

    return nullptr;
}

auto MapperEngine::FindEntityById(raw_ptr<MapView> map, ident_t id) -> ClientEntity*
{
    FO_STACK_TRACE_ENTRY();

    if (map == nullptr || !id) {
        return nullptr;
    }

    if (auto* cr = map->GetCritter(id); cr != nullptr) {
        return cr;
    }
    if (auto* item = map->GetItem(id); item != nullptr) {
        return item;
    }

    std::function<ClientEntity*(ItemView*)> find_inner_item = [&](ItemView* owner) -> ClientEntity* {
        for (auto& inner_item : owner->GetInnerItems()) {
            if (inner_item->GetId() == id) {
                return inner_item.get();
            }
            if (auto* found = find_inner_item(inner_item.get()); found != nullptr) {
                return found;
            }
        }
        return nullptr;
    };

    for (auto& cr : map->GetCritters()) {
        for (auto& inv_item : cr->GetInvItems()) {
            if (inv_item->GetId() == id) {
                return inv_item.get();
            }
            if (auto* found = find_inner_item(inv_item.get()); found != nullptr) {
                return found;
            }
        }
    }

    for (auto& map_item : map->GetItems()) {
        if (auto* found = find_inner_item(map_item.get()); found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

auto MapperEngine::RestoreMapSnapshot(raw_ptr<MapView>& map, string_view map_name, const string& map_text) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (map == nullptr) {
        return false;
    }

    auto* old_map = map.get();
    UnloadMap(old_map, false);

    auto* restored_map = LoadMapFromText(map_name, map_text);
    if (restored_map == nullptr) {
        return false;
    }

    RemapUndoContext(old_map, restored_map);
    ShowMap(restored_map);
    map = restored_map;
    return true;
}

auto MapperEngine::ApplyEntityPropertyText(Entity* entity, const Property* prop, string_view value_text) -> bool
{
    FO_STACK_TRACE_ENTRY();

    try {
        entity->GetPropertiesForEdit().ApplyPropertyFromText(prop, value_text);
        SetMapDirty(_curMap.get());

        if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
            if (item->GetMultihexGeneration() == MultihexGenerationType::SameSibling) {
                for (int32 i = 0; i < GameSettings::MAP_DIR_COUNT; i++) {
                    if (mpos hex = item->GetHex(); GeometryHelper::MoveHexByDir(hex, numeric_cast<uint8>(i), _curMap->GetSize())) {
                        auto main_mesh_items = vec_filter(_curMap->GetItemsOnHex(hex), [&](auto&& item2) -> bool {
                            if (SelectedEntitiesSet.contains(item2.get())) {
                                return false;
                            }
                            return item->GetProtoId() == item2->GetProtoId();
                        });
                        for (auto&& item2 : main_mesh_items) {
                            item2->GetPropertiesForEdit().ApplyPropertyFromText(prop, value_text);
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception&) {
        return false;
    }

    if (auto* hex_item = dynamic_cast<ItemHexView*>(entity); hex_item != nullptr) {
        if (prop == hex_item->GetPropertyOffset()) {
            hex_item->RefreshOffs();
        }
        if (prop == hex_item->GetPropertyPicMap()) {
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

        MainPanelPos = {iround<int32>(pos.x), iround<int32>(pos.y)};
        MainPanelWindowRect = {0, 0, iround<int32>(size.x), iround<int32>(size.y)};
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
            if (ImGui::MenuItem("Save current", "Ctrl+S", false, _curMap != nullptr)) {
                SaveCurrentMap();
            }
            if (ImGui::MenuItem("Reset changes", nullptr, false, _curMap != nullptr && IsMapDirty(_curMap.get()))) {
                ResetCurrentMapChanges();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                App->RequestQuit();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows")) {
            ImGui::MenuItem("Workspace", "F7", &WorkspaceWindowVisible);
            ImGui::MenuItem("Content", "Shift+F7", &ContentWindowVisible);

            if (ImGui::MenuItem("Console", "~", ConsoleEdit)) {
                ConsoleEdit = !ConsoleEdit;
                if (ConsoleEdit) {
                    ConsoleHistoryCur = numeric_cast<int32>(ConsoleHistory.size());
                }
            }

            ImGui::MenuItem("Critter animations", nullptr, &CritterAnimationsWindowVisible);
            ImGui::MenuItem("Script call", nullptr, &ScriptCallWindowVisible);
            ImGui::MenuItem("Map browser", nullptr, &MapListWindowVisible);
            ImGui::MenuItem("Controls", nullptr, &MapWindowVisible, _curMap != nullptr);
            ImGui::MenuItem("History", nullptr, &HistoryWindowVisible, _curMap != nullptr);
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
            ImGui::MenuItem("Items", nullptr, &Settings.ShowItem);
            ImGui::MenuItem("Scenery", nullptr, &Settings.ShowScen);
            ImGui::MenuItem("Walls", nullptr, &Settings.ShowWall);
            ImGui::MenuItem("Critters", nullptr, &Settings.ShowCrit);
            ImGui::MenuItem("Tiles", nullptr, &Settings.ShowTile);
            ImGui::MenuItem("Roof", nullptr, &Settings.ShowRoof);
            ImGui::MenuItem("Fast", nullptr, &Settings.ShowFast);

            ImGui::Separator();
            ImGui::MenuItem("Axial grid selection", nullptr, &SelectAxialGrid);
            ImGui::MenuItem("Select entire entity", nullptr, &SelectEntireEntity);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            run_menu_action_with_message(ImGui::MenuItem("Rebuild map", nullptr, false, _curMap != nullptr), [&] { _curMap->RebuildMap(); }, "Map rebuilt");
            run_menu_action_with_message(ImGui::MenuItem("Mark blocked hexes", nullptr, false, _curMap != nullptr), [&] { _curMap->MarkBlockedHexes(); }, "Blocked hexes marked");
            run_menu_action_with_message(ImGui::MenuItem("Reverse lights", nullptr, false, _curMap != nullptr), [&] { ParseCommand("* reverse-light"); }, "Reverse lights done");
            run_menu_action_with_message(ImGui::MenuItem("Merge by command", nullptr, false, _curMap != nullptr), [&] { ParseCommand("* merge-items"); }, "Merge items command done");
            run_menu_action_with_message(ImGui::MenuItem("Break by command", nullptr, false, _curMap != nullptr), [&] { ParseCommand("* break-items"); }, "Break items command done");

            ImGui::Separator();
            if (ImGui::MenuItem("Merge multihex items", nullptr, false, _curMap != nullptr)) {
                const auto merged = MergeItemsToMultihexMeshes(_curMap.get());
                AddMess(strex("Merged items: {}", merged));
            }
            if (ImGui::MenuItem("Break multihex items", nullptr, false, _curMap != nullptr)) {
                const auto broken = BreakItemsMultihexMeshes(_curMap.get());
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
                SprMngr.GetAtlasMngr().DumpAtlases();
            }

            ImGui::Separator();
            if (SprMngr.IsFullscreen()) {
                ImGui::MenuItem("Fullscreen mouse scroll", nullptr, &Settings.FullscreenMouseScroll);
            }
            else {
                ImGui::MenuItem("Windowed mouse scroll", nullptr, &Settings.WindowedMouseScroll);
            }
            ImGui::EndMenu();
        }

        if (_curMap != nullptr && IsMapDirty(_curMap.get())) {
            constexpr const char* dirty_label = "*** Save ***";
            const auto label_width = ImGui::CalcTextSize(dirty_label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            const auto right_x = numeric_cast<float32>(MainPanelWindowRect.width) - label_width - ImGui::GetStyle().ItemSpacing.x * 2.0f;

            if (ImGui::GetCursorPosX() < right_x) {
                ImGui::SetCursorPosX(right_x);
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.45f, 0.10f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.86f, 0.56f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.60f, 0.36f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));

            if (ImGui::Button(dirty_label)) {
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
            numeric_cast<float32>(MainPanelPos.x + MainPanelContentRect.x),
            numeric_cast<float32>(MainPanelPos.y + MainPanelContentRect.y),
        },
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({500.0f, 600.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Workspace", &WorkspaceWindowVisible, 0)) {
        ImGui::End();
        return;
    }

    const auto toggle_visibility = [&](const char* label, const char* tooltip, bool& value) {
        if (value) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }

        const auto clicked = ImGui::Button(label, {32.0f, 0.0f});

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", tooltip);
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
    visibility_changed |= toggle_visibility("Items", "Items", Settings.ShowItem);
    ImGui::SameLine();
    visibility_changed |= toggle_visibility("Scenery", "Scenery", Settings.ShowScen);
    ImGui::SameLine();
    visibility_changed |= toggle_visibility("Walls", "Walls", Settings.ShowWall);
    ImGui::SameLine();
    visibility_changed |= toggle_visibility("Critters", "Critters", Settings.ShowCrit);
    ImGui::SameLine();
    visibility_changed |= toggle_visibility("Tiles", "Tiles", Settings.ShowTile);
    ImGui::SameLine();
    visibility_changed |= toggle_visibility("Roof", "Roof", Settings.ShowRoof);
    ImGui::SameLine();
    visibility_changed |= toggle_visibility("Fast", "Fast", Settings.ShowFast);
    ImGui::PopStyleVar();

    if (visibility_changed && _curMap) {
        _curMap->RebuildMap();
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
            if (const auto* active_subtab = GetActiveSubTab(); active_subtab != nullptr) {
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
                const auto draw_proto_entry = [&](int32 index, string_view label, const Sprite* sprite, auto&& on_select) {
                    ImGui::PushID(index);
                    const auto selected = index == GetActiveProtoIndex();
                    const auto row_height = 72.0f;
                    const auto row_width = ImGui::GetContentRegionAvail().x;
                    const auto cursor = ImGui::GetCursorScreenPos();
                    const auto clicked = ImGui::Selectable("##ProtoRow", selected, 0, {row_width, row_height});
                    const auto rect_min = ImGui::GetItemRectMin();
                    const auto rect_max = ImGui::GetItemRectMax();
                    auto* draw_list = ImGui::GetWindowDrawList();
                    draw_list->AddRectFilled(rect_min, rect_max, ImGui::ColorConvertFloat4ToU32(selected ? ImVec4(0.24f, 0.32f, 0.18f, 0.65f) : ImVec4(0.08f, 0.08f, 0.08f, 0.35f)), 4.0f);
                    draw_list->AddRect(rect_min, rect_max, ImGui::GetColorU32(ImGuiCol_Border), 4.0f);

                    const ImVec2 preview_min {cursor.x + 8.0f, cursor.y + 8.0f};
                    const ImVec2 preview_max {cursor.x + 64.0f, cursor.y + 64.0f};
                    draw_list->AddRectFilled(preview_min, preview_max, ImGui::ColorConvertFloat4ToU32({0.12f, 0.12f, 0.12f, 1.0f}), 4.0f);

                    if (const auto* atlas_sprite = ResolveAtlasSprite(sprite); atlas_sprite != nullptr) {
                        if (const auto* texture = atlas_sprite->GetBatchTexture(); texture != nullptr) {
                            const auto uv = atlas_sprite->GetAtlasRect();
                            const auto sprite_size = sprite->GetSize();
                            const auto scale = std::min(56.0f / std::max(1, sprite_size.width), 56.0f / std::max(1, sprite_size.height));
                            const auto draw_width = numeric_cast<float32>(sprite_size.width) * scale;
                            const auto draw_height = numeric_cast<float32>(sprite_size.height) * scale;
                            const ImVec2 image_min {preview_min.x + (56.0f - draw_width) * 0.5f, preview_min.y + (56.0f - draw_height) * 0.5f};
                            const ImVec2 image_max {image_min.x + draw_width, image_min.y + draw_height};
                            draw_list->AddImage(const_cast<RenderTexture*>(texture), image_min, image_max, {uv.x, uv.y}, {uv.x + uv.width, uv.y + uv.height});
                        }
                    }

                    draw_list->AddText({cursor.x + 76.0f, cursor.y + 10.0f}, ImGui::GetColorU32(ImGuiCol_Text), string(label).c_str());

                    if (clicked) {
                        on_select();
                    }

                    if (ImGui::IsItemHovered() && sprite != nullptr) {
                        if (ImGui::BeginTooltip()) {
                            if (const auto* atlas_sprite = ResolveAtlasSprite(sprite); atlas_sprite != nullptr) {
                                if (const auto* texture = atlas_sprite->GetBatchTexture(); texture != nullptr) {
                                    const auto uv = atlas_sprite->GetAtlasRect();
                                    const auto sprite_size = sprite->GetSize();
                                    ImGui::Image(const_cast<RenderTexture*>(texture), {numeric_cast<float32>(std::max(1, sprite_size.width)), numeric_cast<float32>(std::max(1, sprite_size.height))}, {uv.x, uv.y}, {uv.x + uv.width, uv.y + uv.height});
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

                if (IsItemMode() && ActiveItemProtos != nullptr) {
                    vector<int32> visible_indices;
                    visible_indices.reserve(ActiveItemProtos->size());

                    for (int32 i = 0; i < numeric_cast<int32>(ActiveItemProtos->size()); i++) {
                        const auto& proto = (*ActiveItemProtos)[i];

                        if (ContainsCaseInsensitive(proto->GetName(), filter)) {
                            visible_indices.emplace_back(i);
                        }
                    }

                    ImGuiListClipper clipper;
                    clipper.Begin(numeric_cast<int32>(visible_indices.size()), 72.0f);
                    while (clipper.Step()) {
                        for (int32 row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
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
                                        stab.ItemProtos.emplace_back(proto.get());
                                    }

                                    _curMap->SwitchIgnorePid(pid);
                                    _curMap->RebuildMap();
                                }
                                else if (ImGui::GetIO().KeyAlt && !SelectedEntities.empty()) {
                                    auto add = true;

                                    if (proto->GetStackable()) {
                                        for (const auto& child : GetEntityInnerItems(SelectedEntities.front().get())) {
                                            if (proto->GetProtoId() == child->GetProtoId()) {
                                                add = false;
                                                break;
                                            }
                                        }
                                    }

                                    if (add) {
                                        CreateItem(proto->GetProtoId(), {}, SelectedEntities.front().get());
                                    }
                                }
                                else {
                                    SetCurMode(CUR_MODE_PLACE_OBJECT);
                                }
                            });
                        }
                    }
                }
                else if (IsCritMode() && ActiveCritterProtos != nullptr) {
                    vector<int32> visible_indices;
                    visible_indices.reserve(ActiveCritterProtos->size());

                    for (int32 i = 0; i < numeric_cast<int32>(ActiveCritterProtos->size()); i++) {
                        const auto& proto = (*ActiveCritterProtos)[i];

                        if (ContainsCaseInsensitive(proto->GetName(), filter)) {
                            visible_indices.emplace_back(i);
                        }
                    }

                    ImGuiListClipper clipper;
                    clipper.Begin(numeric_cast<int32>(visible_indices.size()), 72.0f);
                    while (clipper.Step()) {
                        for (int32 row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                            const auto i = visible_indices[row];
                            const auto& proto = (*ActiveCritterProtos)[i];
                            const auto label = string(proto->GetName());
                            const auto* preview_sprite = ResMngr.GetCritterPreviewSpr(proto->GetModelName(), CritterStateAnim::Unarmed, CritterActionAnim::Idle, CritterDir, nullptr);
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
            numeric_cast<float32>(MainPanelPos.x + MainPanelContentRect.x),
            numeric_cast<float32>(MainPanelPos.y + MainPanelContentRect.y + 162),
        },
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({520.0f, 540.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Content", &ContentWindowVisible, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    auto& map_name_buf = ContentMapNameBuf;
    auto& map_filter_buf = ContentMapFilterBuf;
    int32& resize_w = ContentResizeW;
    int32& resize_h = ContentResizeH;

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
            auto inner_items = GetEntityInnerItems(SelectedEntities.front().get());

            ImGui::Text("Container items: %d", numeric_cast<int32>(inner_items.size()));

            if (ImGui::BeginChild("##ContainerItems", {0.0f, -ImGui::GetFrameHeightWithSpacing() * 2.0f}, true)) {
                for (const auto& inner_item : inner_items) {
                    auto label = strex("{} x{}", inner_item->GetName(), inner_item->GetCount());
                    const auto selected = InContItem == inner_item;

                    if (ImGui::Selectable(label.c_str(), selected)) {
                        InContItem = inner_item;
                        InspectorVisible = true;
                    }
                }
            }
            ImGui::EndChild();

            if (InContItem && ImGui::Button("Remove item")) {
                if (InContItem->GetOwnership() == ItemOwnership::CritterInventory) {
                    auto* owner = _curMap ? _curMap->GetCritter(InContItem->GetCritterId()) : nullptr;
                    if (owner) {
                        owner->DeleteInvItem(InContItem.get());
                    }
                }
                else if (InContItem->GetOwnership() == ItemOwnership::ItemContainer) {
                    auto* owner = _curMap ? _curMap->GetItem(InContItem->GetContainerId()) : nullptr;
                    if (owner) {
                        owner->DestroyInnerItem(InContItem.get());
                    }
                }

                InContItem = nullptr;

                auto* next_entity = SelectedEntities.empty() ? nullptr : SelectedEntities.front().get();
                if (next_entity != nullptr) {
                    SelectClear();
                    SelectAdd(next_entity);
                }
            }

            if (InContItem) {
                if (auto* cr = SelectedEntities.empty() ? nullptr : dynamic_cast<CritterHexView*>(SelectedEntities.front().get()); cr != nullptr) {
                    ImGui::SameLine();

                    if (ImGui::Button("Next slot")) {
                        size_t to_slot = static_cast<size_t>(InContItem->GetCritterSlot()) + 1;

                        while (numeric_cast<size_t>(to_slot) >= Settings.CritterSlotEnabled.size() || !Settings.CritterSlotEnabled[to_slot % 256]) {
                            to_slot++;
                        }

                        to_slot %= 256;

                        for (auto& item : cr->GetInvItems()) {
                            if (item->GetCritterSlot() == static_cast<CritterItemSlot>(to_slot)) {
                                item->SetCritterSlot(CritterItemSlot::Inventory);
                            }
                        }

                        InContItem->SetCritterSlot(static_cast<CritterItemSlot>(to_slot));
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
            resize_w = _curMap->GetSize().width;
            resize_h = _curMap->GetSize().height;
        }

        ImGui::InputTextWithHint("##MapFilter", "Filter maps...", map_filter_buf.data(), map_filter_buf.size());
        const auto map_filter = InputBufferView(map_filter_buf);

        if (ImGui::BeginChild("##LoadedMaps", {0.0f, 180.0f}, true)) {
            for (auto& map : LoadedMaps) {
                auto label = string(map->GetName());

                if (!ContainsCaseInsensitive(label, map_filter)) {
                    continue;
                }

                if (_curMap == map.get()) {
                    label = strex("* {}", label);
                }

                if (ImGui::Selectable(label.c_str(), _curMap == map.get()) && _curMap != map.get()) {
                    ShowMap(map.get());
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
                if (auto* map = LoadMap(map_name); map != nullptr) {
                    ShowMap(map);
                    AddMess("Load map success");
                }
                else {
                    AddMess("Load map failed");
                }
            }
        }
        ImGui::SameLine();
        run_button_action_with_message(ImGui::Button("Save current") && _curMap, [&] { SaveMap(_curMap.get(), ""); }, "Save map success");
        ImGui::SameLine();
        if (ImGui::Button("Save As") && _curMap) {
            const auto map_name = get_map_name_input();
            if (!map_name.empty()) {
                SaveMap(_curMap.get(), map_name);
                AddMess("Save map success");
            }
        }

        ImGui::InputInt("Resize width", &resize_w);
        ImGui::InputInt("Resize height", &resize_h);

        if (ImGui::Button("Resize current") && _curMap) {
            ResizeMap(_curMap.get(), resize_w, resize_h);
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

            App->Input.SetClipboardText(all_messages);
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
            numeric_cast<float32>(MainPanelPos.x + MainPanelContentRect.x + MainPanelContentRect.width + 24),
            numeric_cast<float32>(MainPanelPos.y + MainPanelContentRect.y + 132),
        },
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({360.0f, 160.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Critter Animations", &CritterAnimationsWindowVisible, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    int32& anim_state = CritterAnimState;
    int32& anim_action = CritterAnimAction;
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
            numeric_cast<float32>(MainPanelPos.x + MainPanelContentRect.x + MainPanelContentRect.width + 24),
            numeric_cast<float32>(MainPanelPos.y + MainPanelContentRect.y + 304),
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

    const auto* viewport = ImGui::GetMainViewport();
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

    ImGui::Text("Maps: %d", numeric_cast<int32>(map_names.size()));
    ImGui::Separator();

    if (ImGui::BeginChild("##AllMapsList", {0.0f, 0.0f}, true)) {
        for (const auto& map_name : map_names) {
            const auto loaded_it = std::ranges::find_if(LoadedMaps, [&](const auto& map) { return string(map->GetName()) == map_name; });
            const auto is_loaded = loaded_it != LoadedMaps.end();
            const auto is_current = is_loaded && _curMap == loaded_it->get();

            auto label = map_name;
            if (is_current) {
                label = strex("* {}", label).str();
            }
            else if (is_loaded) {
                label = strex("+ {}", label).str();
            }

            if (ImGui::Selectable(label.c_str(), is_current)) {
                if (is_loaded) {
                    ShowMap(loaded_it->get());
                }
                else if (auto* map = LoadMap(map_name); map != nullptr) {
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

    const auto* viewport = ImGui::GetMainViewport();
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

    mpos hex;
    _curMap->GetHexAtScreen(Settings.MousePos, hex, nullptr);
    const int32 day_time = GetGlobalDayTime();
    const auto map_name = string(_curMap->GetName());
    const auto rotate_selected_critters = [&] {
        for (auto& entity : SelectedEntities) {
            if (auto cr = entity.dyn_cast<CritterHexView>()) {
                AdvanceCritterDir(cr.get());
            }
        }
    };

    if (ImGui::BeginTable("##ControlsSummary", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        const auto draw_summary_row = [](string_view label, auto&& draw_value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label.data(), label.data() + label.size());
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
        _curMap->SwitchShowHex();
    }

    const auto current_zoom = _curMap->GetSpritesZoomTarget();
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

    ImGui::Checkbox("Scroll check", &Settings.ScrollCheck);
    ImGui::Checkbox("Show corners", &Settings.ShowCorners);
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
        Settings.ShowItem,
        Settings.ShowScen,
        Settings.ShowWall,
        Settings.ShowCrit,
        Settings.ShowTile,
        Settings.ShowRoof,
        Settings.ShowFast,
    };

    const auto draw_checkbox_group = [](auto&& entries) {
        for (const auto& [label, value] : entries) {
            ImGui::Checkbox(label, value);
        }
    };

    if (ImGui::CollapsingHeader("Visibility")) {
        draw_checkbox_group(array {
            std::pair {"Items", &Settings.ShowItem},
            std::pair {"Scenery", &Settings.ShowScen},
            std::pair {"Walls", &Settings.ShowWall},
            std::pair {"Critters", &Settings.ShowCrit},
            std::pair {"Tiles", &Settings.ShowTile},
            std::pair {"Roof", &Settings.ShowRoof},
            std::pair {"Fast", &Settings.ShowFast},
        });
    }

    const auto visibility_after = array {
        Settings.ShowItem,
        Settings.ShowScen,
        Settings.ShowWall,
        Settings.ShowCrit,
        Settings.ShowTile,
        Settings.ShowRoof,
        Settings.ShowFast,
    };

    const auto visibility_changed = visibility_before != visibility_after;

    if (visibility_changed && _curMap) {
        _curMap->RebuildMap();
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

    auto* entity = GetInspectorEntity();
    if (entity == nullptr) {
        return;
    }

    const auto* item = dynamic_cast<ItemView*>(entity);
    const auto* cr = dynamic_cast<CritterView*>(entity);
    const char* type_name = cr != nullptr ? "Critter" : (item != nullptr && !item->GetStatic() ? "Dynamic Item" : "Static Item");

    ImGui::SetNextWindowPos({numeric_cast<float32>(InspectorPos.x), numeric_cast<float32>(InspectorPos.y)}, ImGuiCond_Once);
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
    InspectorPos = {iround<int32>(pos.x), iround<int32>(pos.y)};

    auto compatible_candidates = 0;
    const auto is_same_inspector_entity_type = [&](const ClientEntity* selected_entity) {
        const auto same_cr = dynamic_cast<const CritterView*>(selected_entity) != nullptr && cr != nullptr;
        const auto same_item = dynamic_cast<const ItemView*>(selected_entity) != nullptr && item != nullptr;
        return same_cr || same_item;
    };

    if (SelectedEntities.size() > 1 && !SelectedEntities.empty() && SelectedEntities.front() == entity) {
        for (const auto& selected_entity : SelectedEntities) {
            if (is_same_inspector_entity_type(selected_entity.get())) {
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

    constexpr int32 START_LINE = 3;
    string& edit_buf = InspectorEditBuf;
    int32& edit_line = InspectorEditLine;
    int32& pending_focus_line = InspectorPendingFocusLine;
    int32& pending_focus_array_index = InspectorPendingFocusArrayIndex;
    int32& pending_caret_reset_line = InspectorPendingCaretResetLine;
    int32& pending_caret_reset_array_index = InspectorPendingCaretResetArrayIndex;
    int32& pending_caret_reset_frames = InspectorPendingCaretResetFrames;
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

    const auto begin_edit_state = [&](int32 line, int32 array_index = 0) {
        edit_line = line;
        sync_edit_buf();
        pending_focus_line = line;
        pending_focus_array_index = array_index;
        pending_caret_reset_line = line;
        pending_caret_reset_array_index = array_index;
        pending_caret_reset_frames = 2;
    };

    const auto reset_selected_line_state = [&](int32 selected_line) {
        SelectInspectorPropertyLine(selected_line);
        sync_edit_buf();
        clear_edit_state();
    };

    const auto keep_selected_line_edit_state = [&](int32 selected_line) {
        SelectInspectorPropertyLine(selected_line);
        sync_edit_buf();
        edit_line = selected_line;
    };

    const auto restore_selected_line_initial_value = [&] { CancelInspectorPropertyEdit(); };

    const auto apply_to_compatible_selected_entities = [&](auto&& action) {
        if (!(InspectorApplyToAll && can_apply_to_all && !SelectedEntities.empty() && SelectedEntities.front() == entity)) {
            return;
        }

        for (size_t j = 1; j < SelectedEntities.size(); j++) {
            auto* selected_entity = SelectedEntities[j].get();
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

    auto apply_value = [&](Entity* target_entity) { ApplyInspectorPropertyEdit(target_entity); };

    const auto apply_selected_value = [&](int32 selected_line) {
        apply_value(entity);
        apply_to_compatible_selected_entities(apply_value);
        reset_selected_line_state(selected_line);
    };

    const auto apply_selected_value_keep_edit = [&](int32 selected_line) {
        apply_value(entity);
        apply_to_compatible_selected_entities(apply_value);
        keep_selected_line_edit_state(selected_line);
    };

    const auto reset_selected_value = [&](int32 selected_line) {
        if (selected_line < START_LINE || selected_line - START_LINE >= numeric_cast<int32>(ShowProps.size())) {
            return;
        }

        const auto* selected_prop = ShowProps[selected_line - START_LINE].get();
        const auto* entity_with_proto = dynamic_cast<EntityWithProto*>(entity);
        if (selected_prop == nullptr || entity_with_proto == nullptr) {
            return;
        }

        try {
            InspectorSelectedLineValue = entity_with_proto->GetProto()->GetProperties().SavePropertyToText(selected_prop);
            apply_value(entity);
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
            draw_summary_row("Id", strex("{}", entity->GetId()).str());
            draw_summary_row("ProtoId", dynamic_cast<EntityWithProto*>(entity)->GetProtoId());

            for (size_t i = 0; i < ShowProps.size(); i++) {
                const auto line = START_LINE + numeric_cast<int32>(i);
                const auto* prop = ShowProps[i].get();

                ImGui::PushID(line);

                if (prop == nullptr) {
                    ImGui::TableNextRow(ImGuiTableRowFlags_None, 1.0f);
                    ImGui::TableNextColumn();
                    ImGui::Separator();
                    ImGui::TableNextColumn();
                    ImGui::Separator();
                    ImGui::PopID();
                    continue;
                }

                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeightWithSpacing() + 2.0f);

                auto value = entity->GetProperties().SavePropertyToText(prop);
                const auto is_const = prop->IsCoreProperty() && !prop->IsMutable();
                const auto selected = InspectorSelectedLine == line;
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

                    const auto* struct_layout = GetInspectorStructLayout(prop);

                    auto commit_requested = false;
                    auto cancel_requested = false;
                    auto finish_edit_requested = false;
                    auto focus_consumed = false;
                    const auto edit_text_value = [&](float32 item_width = -58.0f) {
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

                    const auto edit_struct_fields = [&](vector<string>& field_values, int32 struct_id, bool request_focus, bool request_caret_reset) {
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

                    if (prop->IsArray() && struct_layout != nullptr) {
                        if (const auto parsed_entries = ParseInspectorStringEntries(InspectorSelectedLineValue); parsed_entries.has_value()) {
                            auto struct_entries = *parsed_entries;
                            auto array_changed = false;
                            std::optional<size_t> remove_index {};

                            for (size_t entry_index = 0; entry_index < struct_entries.size(); entry_index++) {
                                auto field_values = ParseInspectorStructFields(*struct_layout, struct_entries[entry_index]).value_or(vector<string>(struct_layout->Fields.size()));

                                ImGui::PushID(numeric_cast<int32>(entry_index));
                                ImGui::SeparatorText(strex("Item {}", entry_index).str().c_str());
                                array_changed |= edit_struct_fields(field_values, numeric_cast<int32>(entry_index), focus_edit_line == line && entry_index == numeric_cast<size_t>(focus_edit_array_index), pending_caret_reset_line == line && pending_caret_reset_frames > 0 && entry_index == numeric_cast<size_t>(caret_reset_array_index));
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
                                pending_focus_array_index = numeric_cast<int32>(struct_entries.size()) - 1;
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
                                ImGui::PushID(numeric_cast<int32>(entry_index));

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
                                    auto string_buf = entries[entry_index].AsString();
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
                                pending_focus_array_index = numeric_cast<int32>(entries.size()) - 1;
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
                    else if (struct_layout != nullptr) {
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

void MapperEngine::ApplyInspectorPropertyEdit(Entity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    constexpr auto start_line = 3;

    if (InspectorSelectedLine >= start_line && InspectorSelectedLine - start_line < numeric_cast<int32>(ShowProps.size())) {
        const auto& prop = ShowProps[InspectorSelectedLine - start_line];

        if (prop) {
            const auto entity_id = entity->GetId();
            const auto old_value = InspectorSelectedLineInitialValue;
            const auto new_value = InspectorSelectedLineValue;

            if (!ApplyEntityPropertyText(entity, prop.get(), new_value)) {
                ignore_unused(ApplyEntityPropertyText(entity, prop.get(), old_value));
                return;
            }

            const auto prop_name = string(prop->GetName());
            PushUndoOp(_curMap.get(),
                UndoOp {strex("Edit {}", prop_name),
                    [entity_id, prop_name, old_value](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                        auto* target = mapper.FindEntityById(active_map, entity_id);
                        if (target == nullptr) {
                            return false;
                        }

                        const auto* apply_prop = target->GetProperties().GetRegistrator()->FindProperty(prop_name);
                        return apply_prop != nullptr && mapper.ApplyEntityPropertyText(target, apply_prop, old_value);
                    },
                    [entity_id, prop_name, new_value](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                        auto* target = mapper.FindEntityById(active_map, entity_id);
                        if (target == nullptr) {
                            return false;
                        }

                        const auto* apply_prop = target->GetProperties().GetRegistrator()->FindProperty(prop_name);
                        return apply_prop != nullptr && mapper.ApplyEntityPropertyText(target, apply_prop, new_value);
                    }});
        }
    }
}

void MapperEngine::SelectInspectorPropertyLine(int32 line)
{
    FO_STACK_TRACE_ENTRY();

    constexpr auto start_line = 3;

    InspectorSelectedLine = line;
    InspectorSelectedLine = std::max(InspectorSelectedLine, 0);
    InspectorSelectedLineInitialValue = InspectorSelectedLineValue = "";

    if (const auto* entity = GetInspectorEntity(); entity != nullptr) {
        if (InspectorSelectedLine - start_line >= numeric_cast<int32>(ShowProps.size())) {
            InspectorSelectedLine = numeric_cast<int32>(ShowProps.size()) + start_line - 1;
        }
        if (InspectorSelectedLine >= start_line && InspectorSelectedLine - start_line < numeric_cast<int32>(ShowProps.size()) && (ShowProps[InspectorSelectedLine - start_line] != nullptr)) {
            InspectorSelectedLineInitialValue = InspectorSelectedLineValue = entity->GetProperties().SavePropertyToText(ShowProps[InspectorSelectedLine - start_line].get());
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

auto MapperEngine::GetInspectorEntity() -> ClientEntity*
{
    FO_STACK_TRACE_ENTRY();

    auto* entity = ActivePanelMode == INT_MODE_INCONT && InContItem ? InContItem.get() : (!SelectedEntities.empty() ? SelectedEntities.front().get() : nullptr);

    if (entity == InspectorEntity) {
        return entity;
    }

    InspectorEntity = entity;
    ShowProps.clear();

    if (entity != nullptr) {
        vector<int32> prop_indices;
        OnInspectorProperties.Fire(entity, prop_indices);

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

    InContItem = nullptr;

    if (!_curMap) {
        return true;
    }

    if (!_curMap->GetHexAtScreen(Settings.MousePos, SelectHex1, nullptr)) {
        return true;
    }

    SelectHex2 = SelectHex1;
    SelectPos = Settings.MousePos;

    if (CurMode == CUR_MODE_PLACE_OBJECT) {
        return true;
    }

    const auto entity = _curMap->GetEntityAtScreen(Settings.MousePos, 0, true);
    const auto clicked_entity = entity.first;
    const auto clicked_selected = clicked_entity != nullptr && SelectedEntitiesSet.contains(clicked_entity);

    if (clicked_selected) {
        MouseHoldMode = INT_MOVE_SELECTION;
        SetCurMode(CUR_MODE_MOVE_SELECTION);
        return true;
    }

    if (App->Input.IsShiftDown()) {
        for (auto& selected_entity : SelectedEntities) {
            if (auto* cr = dynamic_cast<CritterHexView*>(selected_entity.get()); cr != nullptr) {
                auto hex = cr->GetHex();

                if (const auto find_path = _curMap->FindPath(nullptr, hex, SelectHex1, -1)) {
                    for (const auto dir : find_path->DirSteps) {
                        if (GeometryHelper::MoveHexByDir(hex, dir, _curMap->GetSize())) {
                            _curMap->MoveCritter(cr, hex, true);
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
    else if (!App->Input.IsCtrlDown()) {
        SelectClear();
    }

    MouseHoldMode = INT_SELECT;

    return true;
}

void MapperEngine::HandleLeftMouseUp()
{
    FO_STACK_TRACE_ENTRY();

    if (CurMode == CUR_MODE_PLACE_OBJECT) {
        if (_curMap->GetHexAtScreen(Settings.MousePos, SelectHex2, nullptr)) {
            if (IsItemMode() && ActiveItemProtos != nullptr && !ActiveItemProtos->empty()) {
                CreateItem((*ActiveItemProtos)[GetActiveProtoIndex()]->GetProtoId(), SelectHex2, nullptr);
            }
            else if (IsCritMode() && ActiveCritterProtos != nullptr && !ActiveCritterProtos->empty()) {
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
        if (_curMap->GetHexAtScreen(Settings.MousePos, SelectHex2, nullptr)) {
            if (SelectHex1 != SelectHex2) {
                _curMap->ClearHexTrack();

                vector<mpos> hexes;

                if (SelectAxialGrid) {
                    hexes = GeometryHelper::GetAxialHexes(SelectHex1, SelectHex2, _curMap->GetSize());
                }
                else {
                    const auto map_size = _curMap->GetSize();
                    const int32 fx = std::min(SelectHex1.x, SelectHex2.x);
                    const int32 tx = std::max(SelectHex1.x, SelectHex2.x);
                    const int32 fy = std::min(SelectHex1.y, SelectHex2.y);
                    const int32 ty = std::max(SelectHex1.y, SelectHex2.y);

                    for (int32 i = fx; i <= tx; i++) {
                        for (int32 j = fy; j <= ty; j++) {
                            hexes.emplace_back(map_size.from_raw_pos(i, j));
                        }
                    }
                }

                const auto check_item_to_add = [&](const ItemHexView* item) -> bool {
                    if (_curMap->IsIgnorePid(item->GetProtoId())) {
                        return false;
                    }
                    if (item->GetIsTile() && !item->GetIsRoofTile() && SelectTilesEnabled && Settings.ShowTile) {
                        return true;
                    }
                    else if (item->GetIsTile() && item->GetIsRoofTile() && SelectRoofTilesEnabled && Settings.ShowRoof) {
                        return true;
                    }
                    else if (!item->GetIsTile() && !item->GetIsScenery() && !item->GetIsWall() && SelectItemsEnabled && Settings.ShowItem) {
                        return true;
                    }
                    else if (!item->GetIsTile() && item->GetIsScenery() && SelectSceneryEnabled && Settings.ShowScen) {
                        return true;
                    }
                    else if (!item->GetIsTile() && item->GetIsWall() && SelectWallsEnabled && Settings.ShowWall) {
                        return true;
                    }
                    else if (Settings.ShowFast && _curMap->IsFastPid(item->GetProtoId())) {
                        return true;
                    }
                    else {
                        return false;
                    }
                };

                for (const auto hex : hexes) {
                    for (auto* hex_item : copy_hold_ref(_curMap->GetItemsOnHex(hex))) {
                        if (check_item_to_add(hex_item)) {
                            SelectAdd(hex_item, hex, true);
                        }
                    }
                    for (auto* hex_cr : copy_hold_ref(_curMap->GetCrittersOnHex(hex, CritterFindType::Any))) {
                        if (SelectCrittersEnabled && Settings.ShowCrit) {
                            SelectAdd(hex_cr, hex);
                        }
                    }
                }

                _curMap->DefferedRefreshItems();
            }
            else {
                const auto entity = _curMap->GetEntityAtScreen(Settings.MousePos, 0, true);

                if (auto* item = dynamic_cast<ItemHexView*>(entity.first); item != nullptr) {
                    if (!item->GetIsTile()) {
                        SelectAdd(item, entity.second->GetHex());
                    }
                }
                else if (auto* cr = dynamic_cast<CritterHexView*>(entity.first); cr != nullptr) {
                    SelectAdd(cr, entity.second->GetHex());
                }
            }

            if (!SelectedEntities.empty() && !GetEntityInnerItems(SelectedEntities.front().get()).empty()) {
                SetActivePanelMode(INT_MODE_INCONT);
            }

            _curMap->RebuildMap();
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

    _curMap->ClearHexTrack();

    if (!_curMap->GetHexAtScreen(Settings.MousePos, SelectHex2, nullptr)) {
        if (!SelectHex2.is_zero()) {
            _curMap->RebuildMap();
            SelectHex2 = {};
        }

        return;
    }

    if (MouseHoldMode == INT_SELECT) {
        if (SelectHex1 != SelectHex2) {
            if (SelectAxialGrid) {
                for (const auto hex : GeometryHelper::GetAxialHexes(SelectHex1, SelectHex2, _curMap->GetSize())) {
                    _curMap->GetHexTrack(hex) = 1;
                }
            }
            else {
                const auto map_size = _curMap->GetSize();
                const int32 fx = std::min(SelectHex1.x, SelectHex2.x);
                const int32 tx = std::max(SelectHex1.x, SelectHex2.x);
                const int32 fy = std::min(SelectHex1.y, SelectHex2.y);
                const int32 ty = std::max(SelectHex1.y, SelectHex2.y);

                for (auto i = fx; i <= tx; i++) {
                    for (auto j = fy; j <= ty; j++) {
                        _curMap->GetHexTrack(map_size.from_raw_pos(i, j)) = 1;
                    }
                }
            }

            _curMap->RebuildMap();
        }
    }
    else if (MouseHoldMode == INT_MOVE_SELECTION) {
        auto offs_hx = numeric_cast<int32>(SelectHex2.x) - numeric_cast<int32>(SelectHex1.x);
        auto offs_hy = numeric_cast<int32>(SelectHex2.y) - numeric_cast<int32>(SelectHex1.y);
        auto offs_x = Settings.MousePos.x - SelectPos.x;
        auto offs_y = Settings.MousePos.y - SelectPos.y;

        if (SelectMove(!App->Input.IsShiftDown(), offs_hx, offs_hy, offs_x, offs_y)) {
            SelectHex1 = _curMap->GetSize().from_raw_pos(SelectHex1.x + offs_hx, SelectHex1.y + offs_hy);
            SelectPos.x += offs_x;
            SelectPos.y += offs_y;
            _curMap->RebuildMap();
        }
    }
}

auto MapperEngine::GetActiveSubTab() -> SubTab*
{
    FO_STACK_TRACE_ENTRY();

    if (ActivePanelMode < 0 || ActivePanelMode >= TAB_COUNT) {
        return nullptr;
    }

    return ActiveSubTabs[ActivePanelMode].get();
}

auto MapperEngine::GetActiveSubTab() const -> const SubTab*
{
    FO_STACK_TRACE_ENTRY();

    if (ActivePanelMode < 0 || ActivePanelMode >= TAB_COUNT) {
        return nullptr;
    }

    return ActiveSubTabs[ActivePanelMode].get();
}

auto MapperEngine::GetActiveProtoIndex() const -> int32
{
    FO_STACK_TRACE_ENTRY();

    if (const auto* active_subtab = GetActiveSubTab(); active_subtab != nullptr) {
        return active_subtab->Index;
    }

    if (ActivePanelMode >= 0 && ActivePanelMode < INT_MODE_COUNT) {
        return TabIndex[ActivePanelMode];
    }

    return 0;
}

void MapperEngine::SetActiveProtoIndex(int32 index)
{
    FO_STACK_TRACE_ENTRY();

    if (auto* active_subtab = GetActiveSubTab(); active_subtab != nullptr) {
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
    InContItem = nullptr;

    if (auto* stab = GetActiveSubTab(); stab != nullptr) {
        if (!stab->CritterProtos.empty()) {
            ActiveCritterProtos = &stab->CritterProtos;
        }
        else {
            ActiveItemProtos = &stab->ItemProtos;
        }

        ActiveProtoScroll = &stab->Scroll;
    }

    if (_curMap) {
        // Update fast pids
        _curMap->ClearFastPids();
        for (const auto& fast_proto : ActiveSubTabs[INT_MODE_FAST]->ItemProtos) {
            _curMap->AddFastPid(fast_proto->GetProtoId());
        }

        // Update ignore pids
        _curMap->ClearIgnorePids();

        for (const auto& ignore_proto : ActiveSubTabs[INT_MODE_IGNORE]->ItemProtos) {
            _curMap->AddIgnorePid(ignore_proto->GetProtoId());
        }

        // Refresh map
        _curMap->RebuildMap();
    }
}

void MapperEngine::SetActivePanelMode(int32 mode)
{
    FO_STACK_TRACE_ENTRY();

    ActivePanelMode = mode;
    MouseHoldMode = INT_NONE;

    RefreshActiveProtoLists();
}

void MapperEngine::MoveEntity(ClientEntity* entity, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (!_curMap->GetSize().is_valid_pos(hex)) {
        return;
    }

    mpos old_hex {};
    if (const auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        old_hex = cr->GetHex();
    }
    else if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        old_hex = item->GetHex();
    }

    if (old_hex == hex) {
        return;
    }

    const auto entity_id = entity->GetId();

    SelectClear();

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        _curMap->MoveCritter(cr, hex, false);
    }
    else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        _curMap->MoveItem(item, hex);
    }

    SetMapDirty(_curMap.get());

    PushUndoOp(_curMap.get(),
        UndoOp {"Move entity",
            [entity_id, old_hex](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                auto* target = mapper.FindEntityById(active_map, entity_id);
                if (target == nullptr) {
                    return false;
                }
                mapper.MoveEntity(target, old_hex);
                return true;
            },
            [entity_id, hex](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                auto* target = mapper.FindEntityById(active_map, entity_id);
                if (target == nullptr) {
                    return false;
                }
                mapper.MoveEntity(target, hex);
                return true;
            }});
}

void MapperEngine::DeleteEntity(ClientEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    const auto map = _curMap.get();
    EntityBuf entity_buf;
    CaptureEntityBuf(entity_buf, entity);
    auto item_ownership = ItemOwnership::MapHex;
    ident_t owner_id {};

    if (const auto* item = dynamic_cast<ItemView*>(entity); item != nullptr) {
        item_ownership = item->GetOwnership();
        if (item_ownership == ItemOwnership::CritterInventory) {
            owner_id = item->GetCritterId();
        }
        else if (item_ownership == ItemOwnership::ItemContainer) {
            owner_id = item->GetContainerId();
        }
    }

    SelectedEntitiesSet.erase(entity);

    vec_remove_unique_value(SelectedEntities, entity);

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        _curMap->DestroyCritter(cr);
    }
    else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        _curMap->DestroyItem(item);
    }
    else if (auto* inner_item = dynamic_cast<ItemView*>(entity); inner_item != nullptr) {
        if (inner_item->GetOwnership() == ItemOwnership::CritterInventory) {
            if (auto* owner = _curMap->GetCritter(inner_item->GetCritterId()); owner != nullptr) {
                owner->DeleteInvItem(inner_item);
            }
        }
        else if (inner_item->GetOwnership() == ItemOwnership::ItemContainer) {
            if (auto* owner = _curMap->GetItem(inner_item->GetContainerId()); owner != nullptr) {
                owner->DestroyInnerItem(inner_item);
            }
        }
    }

    SetMapDirty(_curMap.get());

    PushUndoOp(map,
        UndoOp {"Delete entity",
            [entity_buf, item_ownership, owner_id](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                Entity* owner = nullptr;

                if (item_ownership == ItemOwnership::CritterInventory || item_ownership == ItemOwnership::ItemContainer) {
                    owner = mapper.FindEntityById(active_map, owner_id);
                    if (owner == nullptr) {
                        return false;
                    }
                }

                return mapper.RestoreEntityBuf(entity_buf, owner) != nullptr;
            },
            [entity_id = entity_buf.Id](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                if (auto* target = mapper.FindEntityById(active_map, entity_id); target != nullptr) {
                    mapper.DeleteEntity(target);
                    return true;
                }
                return false;
            }});
}

void MapperEngine::SelectAdd(ClientEntity* entity, optional<mpos> hex, bool skip_refresh)
{
    FO_STACK_TRACE_ENTRY();

    if (entity->IsDestroyed()) {
        return;
    }
    if (SelectedEntitiesSet.contains(entity)) {
        return;
    }

    ClientEntity* corrected_entity = entity;

    // Break from merged mesh
    if (!SelectEntireEntity && hex.has_value()) {
        if (auto* item = dynamic_cast<ItemHexView*>(corrected_entity); item != nullptr) {
            corrected_entity = TryBreakItemFromMultihexMesh(_curMap.get(), item, hex.value());

            if (corrected_entity == nullptr) {
                return;
            }
        }
    }

    vec_add_unique_value(SelectedEntities, corrected_entity);
    SelectedEntitiesSet.emplace(corrected_entity);
    InspectorVisible = true;

    // Make transparent
    if (auto* hex_view = dynamic_cast<HexView*>(corrected_entity); hex_view != nullptr) {
        hex_view->SetTargetAlpha(SelectAlpha);
    }

    if (!skip_refresh) {
        _curMap->DefferedRefreshItems();
    }
}

void MapperEngine::SelectAll()
{
    FO_STACK_TRACE_ENTRY();

    SelectClear();

    for (auto& item : _curMap->GetItems()) {
        if (_curMap->IsIgnorePid(item->GetProtoId())) {
            continue;
        }

        if ((!item->GetIsScenery() && !item->GetIsWall() && SelectItemsEnabled && Settings.ShowItem) || //
            (item->GetIsScenery() && SelectSceneryEnabled && Settings.ShowScen) || //
            (item->GetIsWall() && SelectWallsEnabled && Settings.ShowWall) || //
            (item->GetIsTile() && !item->GetIsRoofTile() && SelectTilesEnabled && Settings.ShowTile) || //
            (item->GetIsTile() && item->GetIsRoofTile() && SelectRoofTilesEnabled && Settings.ShowRoof)) {
            SelectAdd(item.get());
        }
    }

    if (SelectCrittersEnabled && Settings.ShowCrit) {
        for (auto& cr : _curMap->GetCritters()) {
            SelectAdd(cr.get());
        }
    }

    _curMap->RebuildMap();
}

void MapperEngine::SelectRemove(ClientEntity* entity, bool skip_refresh)
{
    FO_STACK_TRACE_ENTRY();

    if (SelectedEntitiesSet.erase(entity) == 0) {
        return;
    }

    refcount_ptr entity_holder = entity;
    vec_remove_unique_value(SelectedEntities, entity);

    // Delete intersected tiles
    if (!SelectEntireEntity && !entity->IsDestroyed()) {
        if (const auto* tile = dynamic_cast<ItemHexView*>(entity); tile != nullptr && tile->GetIsTile()) {
            vector<ItemHexView*> same_siblings;

            for (auto& item : _curMap->GetItemsOnHex(tile->GetHex())) {
                if (item != tile && item->GetIsTile() == tile->GetIsTile() && item->GetIsRoofTile() == tile->GetIsRoofTile() && //
                    item->GetTileLayer() == tile->GetTileLayer() && !SelectedEntitiesSet.contains(static_cast<ClientEntity*>(item.get()))) {
                    same_siblings.emplace_back(item.get());
                }
            }

            for (auto* same_sibling : copy_hold_ref(same_siblings)) {
                if (!same_sibling->IsDestroyed()) {
                    same_sibling = TryBreakItemFromMultihexMesh(_curMap.get(), same_sibling, tile->GetHex());

                    if (same_sibling != nullptr) {
                        _curMap->DestroyItem(same_sibling);
                    }
                }
            }
        }
    }

    // Restore alpha
    if (!entity->IsDestroyed()) {
        if (auto* hex_view = dynamic_cast<HexView*>(entity); hex_view != nullptr) {
            hex_view->RestoreAlpha();
        }
    }

    // Merge multihex items
    if (!entity->IsDestroyed()) {
        if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr && item->GetMultihexGeneration() != MultihexGenerationType::None) {
            while (item != nullptr) {
                item = TryMergeItemToMultihexMesh(_curMap.get(), item, true);
            }
        }
    }

    if (!skip_refresh) {
        _curMap->DefferedRefreshItems();
    }
}

void MapperEngine::SelectClear()
{
    FO_STACK_TRACE_ENTRY();

    while (!SelectedEntities.empty()) {
        SelectRemove(SelectedEntities.back().get(), true);
    }

    InContItem = nullptr;
    InspectorVisible = false;
    InspectorEntity = nullptr;
    ShowProps.clear();
    InspectorSelectedLine = 0;
    InspectorSelectedLineInitialValue.clear();
    InspectorSelectedLineValue.clear();
    InspectorEditBuf.clear();
    ResetInspectorPropertyEditState();

    _curMap->DefferedRefreshItems();
}

auto MapperEngine::SelectMove(bool hex_move, int32& offs_hx, int32& offs_hy, int32& offs_x, int32& offs_y) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!hex_move && offs_x == 0 && offs_y == 0) {
        return false;
    }
    if (hex_move && offs_hx == 0 && offs_hy == 0) {
        return false;
    }

    // Tile step
    const auto have_tiles = std::ranges::find_if(SelectedEntities, [](auto&& entity) {
        const auto item = entity.template dyn_cast<ItemHexView>();
        return item && item->GetIsTile();
    }) != SelectedEntities.end();

    if (hex_move && have_tiles) {
        if (std::abs(offs_hx) < Settings.MapTileStep && std::abs(offs_hy) < Settings.MapTileStep) {
            return false;
        }

        offs_hx -= offs_hx % Settings.MapTileStep;
        offs_hy -= offs_hy % Settings.MapTileStep;
    }

    // Setup hex moving switcher
    int32 switcher = 0;

    if (!SelectedEntities.empty()) {
        if (auto cr = SelectedEntities.front().dyn_cast<CritterHexView>()) {
            switcher = std::abs(cr->GetHex().x % 2);
        }
        else if (auto item = SelectedEntities.front().dyn_cast<ItemHexView>()) {
            switcher = std::abs(item->GetHex().x % 2);
        }
    }

    const auto move_hex = [&](ipos32& raw_hex) {
        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            int32 sw = switcher;

            for (int32 k = 0; k < std::abs(offs_hx); k++, sw++) {
                GeometryHelper::MoveHexByDirUnsafe(raw_hex, offs_hx > 0 ? ((sw % 2) != 0 ? 4 : 3) : ((sw % 2) != 0 ? 0 : 1));
            }
            for (int32 k = 0; k < std::abs(offs_hy); k++) {
                GeometryHelper::MoveHexByDirUnsafe(raw_hex, offs_hy > 0 ? 2 : 5);
            }
        }
        else {
            raw_hex.x += offs_hx;
            raw_hex.y += offs_hy;
        }
    };

    if (!hex_move) {
        float32& small_ox = SelectionSmallOffsetX;
        float32& small_oy = SelectionSmallOffsetY;
        const auto ox = numeric_cast<float32>(offs_x) / _curMap->GetSpritesZoom() + small_ox;
        const auto oy = numeric_cast<float32>(offs_y) / _curMap->GetSpritesZoom() + small_oy;

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

        offs_x = iround<int32>(ox);
        offs_y = iround<int32>(oy);
    }
    else {
        for (auto& entity : SelectedEntities) {
            ipos32 raw_hex;

            if (auto cr = entity.dyn_cast<CritterHexView>()) {
                raw_hex = ipos32 {cr->GetHex().x, cr->GetHex().y};
            }
            else if (auto item = entity.dyn_cast<ItemHexView>()) {
                raw_hex = ipos32 {item->GetHex().x, item->GetHex().y};
            }

            move_hex(raw_hex);

            if (!_curMap->GetSize().is_valid_pos(raw_hex)) {
                return false;
            }

            if (auto item = entity.dyn_cast<ItemHexView>(); item && item->HasMultihexEntries()) {
                for (const auto hex2 : item->GetMultihexEntries()) {
                    ipos32 raw_hex2 = ipos32 {hex2.x, hex2.y};
                    move_hex(raw_hex2);

                    if (!_curMap->GetSize().is_valid_pos(raw_hex2)) {
                        return false;
                    }
                }
            }
        }
    }

    vector<MoveCommandEntry> move_entries;

    for (auto& entity : SelectedEntities) {
        if (!hex_move) {
            auto item = entity.dyn_cast<ItemHexView>();

            if (item == nullptr) {
                continue;
            }

            auto ox = item->GetOffset().x + offs_x;
            auto oy = item->GetOffset().y + offs_y;

            if (App->Input.IsAltDown()) {
                ox = oy = 0;
            }

            MoveCommandEntry entry;
            entry.EntityId = item->GetId();
            entry.HasOffset = true;
            entry.OldOffset = item->GetOffset();
            entry.NewOffset = {numeric_cast<int16>(ox), numeric_cast<int16>(oy)};
            if (entry.OldOffset == entry.NewOffset) {
                continue;
            }
            move_entries.emplace_back(std::move(entry));

            item->SetOffset({numeric_cast<int16>(ox), numeric_cast<int16>(oy)});
            item->RefreshAnim();
        }
        else {
            ipos32 raw_hex;

            if (const auto cr = entity.dyn_cast<CritterHexView>()) {
                raw_hex = ipos32 {cr->GetHex().x, cr->GetHex().y};
            }
            else if (const auto item = entity.dyn_cast<ItemHexView>()) {
                raw_hex = ipos32 {item->GetHex().x, item->GetHex().y};
            }

            move_hex(raw_hex);
            const mpos hex = _curMap->GetSize().clamp_pos(raw_hex);

            if (auto item = entity.dyn_cast<ItemHexView>()) {
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
                        hex2 = _curMap->GetSize().clamp_pos(raw_hex2);
                    });

                    entry.NewMultihexMesh = multihex_mesh;
                    item->SetMultihexMesh(multihex_mesh);
                }

                if (entry.OldHex == entry.NewHex && entry.OldMultihexMesh == entry.NewMultihexMesh) {
                    continue;
                }
                move_entries.emplace_back(std::move(entry));

                _curMap->MoveItem(item.get(), hex);
            }
            else if (auto cr = entity.dyn_cast<CritterHexView>()) {
                MoveCommandEntry entry;
                entry.EntityId = cr->GetId();
                entry.HasHex = true;
                entry.OldHex = cr->GetHex();
                entry.NewHex = hex;
                if (entry.OldHex == entry.NewHex) {
                    continue;
                }
                move_entries.emplace_back(std::move(entry));

                _curMap->MoveCritter(cr.get(), hex, false);
            }
        }
    }

    if (move_entries.empty()) {
        return false;
    }

    SetMapDirty(_curMap.get());
    _curMap->DefferedRefreshItems();

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

    if (!UndoRedoInProgress && SelectedEntities.size() > 1) {
        struct DeleteCommandEntry
        {
            EntityBuf Entity {};
            ItemOwnership Ownership {ItemOwnership::MapHex};
            ident_t OwnerId {};
        };

        vector<DeleteCommandEntry> delete_entries;
        delete_entries.reserve(SelectedEntities.size());

        for (auto* entity : copy_hold_ref(SelectedEntities)) {
            DeleteCommandEntry entry;
            CaptureEntityBuf(entry.Entity, entity);

            if (const auto* item = dynamic_cast<ItemView*>(entity); item != nullptr) {
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
        for (auto* entity : copy_hold_ref(SelectedEntities)) {
            DeleteEntity(entity);
        }
        UndoRedoInProgress = false;

        SelectClear();
        _curMap->RebuildMap();
        MouseHoldMode = INT_NONE;
        SetCurMode(CUR_MODE_DEFAULT);

        PushUndoOp(_curMap.get(),
            UndoOp {"Delete selection",
                [delete_entries](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                    for (const auto& entry : delete_entries) {
                        Entity* owner = nullptr;

                        if (entry.Ownership == ItemOwnership::CritterInventory || entry.Ownership == ItemOwnership::ItemContainer) {
                            owner = mapper.FindEntityById(active_map, entry.OwnerId);
                            if (owner == nullptr) {
                                return false;
                            }
                        }

                        if (mapper.RestoreEntityBuf(entry.Entity, owner) == nullptr) {
                            return false;
                        }
                    }

                    mapper.SetMapDirty(active_map.get());
                    return true;
                },
                [delete_entries](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                    for (auto it = delete_entries.rbegin(); it != delete_entries.rend(); ++it) {
                        if (auto* target = mapper.FindEntityById(active_map, it->Entity.Id); target != nullptr) {
                            mapper.DeleteEntity(target);
                        }
                    }

                    return true;
                }});
        return;
    }

    for (auto* entity : copy_hold_ref(SelectedEntities)) {
        DeleteEntity(entity);
    }

    SelectClear();
    _curMap->RebuildMap();
    MouseHoldMode = INT_NONE;
    SetCurMode(CUR_MODE_DEFAULT);
}

auto MapperEngine::CreateCritter(hstring pid, mpos hex) -> CritterView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_curMap);

    if (!_curMap->GetSize().is_valid_pos(hex)) {
        return nullptr;
    }

    if (_curMap->GetNonDeadCritter(hex) != nullptr) {
        return nullptr;
    }

    SelectClear();

    CritterHexView* cr = _curMap->AddMapperCritter(pid, hex, GeometryHelper::DirToAngle(CritterDir), nullptr);

    SelectAdd(cr, hex);

    SetMapDirty(_curMap.get());
    _curMap->RebuildMap();
    SetCurMode(CUR_MODE_DEFAULT);

    EntityBuf entity_buf;
    CaptureEntityBuf(entity_buf, cr);
    PushUndoOp(_curMap.get(),
        UndoOp {"Create critter",
            [entity_id = entity_buf.Id](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                if (auto* target = mapper.FindEntityById(active_map, entity_id); target != nullptr) {
                    mapper.DeleteEntity(target);
                    return true;
                }
                return false;
            },
            [entity_buf](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                ignore_unused(active_map);
                return mapper.RestoreEntityBuf(entity_buf) != nullptr;
            }});

    return cr;
}

auto MapperEngine::CreateItem(hstring pid, mpos hex, Entity* owner) -> ItemView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_curMap);

    const auto* proto = GetProtoItem(pid);

    if (proto == nullptr) {
        return nullptr;
    }

    mpos corrected_hex = hex;

    if (proto->GetIsTile()) {
        corrected_hex = _curMap->GetSize().from_raw_pos(corrected_hex.x - corrected_hex.x % Settings.MapTileStep, corrected_hex.y - corrected_hex.y % Settings.MapTileStep);
    }

    if (owner == nullptr && (!_curMap->GetSize().is_valid_pos(corrected_hex))) {
        return nullptr;
    }

    if (owner == nullptr) {
        SelectClear();
    }

    ItemView* item = nullptr;

    if (owner != nullptr) {
        if (auto* cr = dynamic_cast<CritterHexView*>(owner); cr != nullptr) {
            item = cr->AddMapperInvItem(cr->GetMap()->GenTempEntityId(), proto, CritterItemSlot::Inventory, {});
        }
        if (auto* cont = dynamic_cast<ItemHexView*>(owner); cont != nullptr) {
            item = cont->AddMapperInnerItem(cont->GetMap()->GenTempEntityId(), proto, {}, nullptr);
        }
    }
    else if (proto->GetIsTile()) {
        item = _curMap->AddMapperTile(proto->GetProtoId(), corrected_hex, numeric_cast<uint8>(TileLayer), PreviewRoofTiles);
    }
    else {
        item = _curMap->AddMapperItem(proto->GetProtoId(), corrected_hex, nullptr);
    }

    if (owner == nullptr) {
        SelectAdd(item, corrected_hex);
        SetCurMode(CUR_MODE_DEFAULT);
    }
    else {
        SetActivePanelMode(INT_MODE_INCONT);
        InContItem = item;
    }

    SetMapDirty(_curMap.get());

    if (item != nullptr) {
        EntityBuf entity_buf;
        CaptureEntityBuf(entity_buf, item);
        auto item_ownership = item->GetOwnership();
        ident_t owner_id {};
        if (item_ownership == ItemOwnership::CritterInventory) {
            owner_id = item->GetCritterId();
        }
        else if (item_ownership == ItemOwnership::ItemContainer) {
            owner_id = item->GetContainerId();
        }

        PushUndoOp(_curMap.get(),
            UndoOp {"Create item",
                [entity_id = entity_buf.Id](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                    if (auto* target = mapper.FindEntityById(active_map, entity_id); target != nullptr) {
                        mapper.DeleteEntity(target);
                        return true;
                    }
                    return false;
                },
                [entity_buf, item_ownership, owner_id](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                    Entity* restore_owner = nullptr;

                    if (item_ownership == ItemOwnership::CritterInventory || item_ownership == ItemOwnership::ItemContainer) {
                        restore_owner = mapper.FindEntityById(active_map, owner_id);
                        if (restore_owner == nullptr) {
                            return false;
                        }
                    }

                    return mapper.RestoreEntityBuf(entity_buf, restore_owner) != nullptr;
                }});
    }

    return item;
}

auto MapperEngine::CloneEntity(Entity* entity) -> Entity*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_curMap);

    if (const auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        auto* cr_clone = _curMap->AddMapperCritter(cr->GetProtoId(), cr->GetHex(), cr->GetDirAngle(), &cr->GetProperties());

        for (const auto& inv_item : cr->GetInvItems()) {
            auto* inv_item_clone = cr_clone->AddMapperInvItem(_curMap->GenTempEntityId(), dynamic_cast<const ProtoItem*>(inv_item->GetProto()), inv_item->GetCritterSlot(), {});
            CloneInnerItems(inv_item_clone, inv_item.get());
        }

        SelectAdd(cr_clone);
        SetMapDirty(_curMap.get());

        EntityBuf entity_buf;
        CaptureEntityBuf(entity_buf, cr_clone);
        PushUndoOp(_curMap.get(),
            UndoOp {"Clone entity",
                [entity_id = entity_buf.Id](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                    if (auto* target = mapper.FindEntityById(active_map, entity_id); target != nullptr) {
                        mapper.DeleteEntity(target);
                        return true;
                    }
                    return false;
                },
                [entity_buf](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                    ignore_unused(active_map);
                    return mapper.RestoreEntityBuf(entity_buf) != nullptr;
                }});

        return cr_clone;
    }

    if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        auto* item_clone = _curMap->AddMapperItem(item->GetProtoId(), item->GetHex(), &item->GetProperties());

        CloneInnerItems(item_clone, item);

        SelectAdd(item_clone);
        SetMapDirty(_curMap.get());

        EntityBuf entity_buf;
        CaptureEntityBuf(entity_buf, item_clone);
        PushUndoOp(_curMap.get(),
            UndoOp {"Clone entity",
                [entity_id = entity_buf.Id](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                    if (auto* target = mapper.FindEntityById(active_map, entity_id); target != nullptr) {
                        mapper.DeleteEntity(target);
                        return true;
                    }
                    return false;
                },
                [entity_buf](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                    ignore_unused(active_map);
                    return mapper.RestoreEntityBuf(entity_buf) != nullptr;
                }});

        return item_clone;
    }

    return nullptr;
}

void MapperEngine::CloneInnerItems(ItemView* to_item, const ItemView* from_item)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& inner_item : from_item->GetInnerItems()) {
        const auto stack_id = any_t {string(inner_item->GetContainerStack())};
        auto* inner_item_clone = to_item->AddMapperInnerItem(_curMap->GenTempEntityId(), dynamic_cast<const ProtoItem*>(inner_item->GetProto()), stack_id, &from_item->GetProperties());
        CloneInnerItems(inner_item_clone, inner_item.get());
    }
}

auto MapperEngine::MergeItemsToMultihexMeshes(MapView* map) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    size_t merges = 0;

    // First merge to modified items
    for (auto* item : copy_hold_ref(map->GetItems())) {
        if (!item->IsDestroyed() && item->GetMultihexGeneration() != MultihexGenerationType::None) {
            auto ignore_props = vector<const Property*> {item->GetPropertyHex(), item->GetPropertyMultihexMesh()};

            if (!item->GetProperties().CompareData(item->GetProto()->GetProperties(), ignore_props, true)) {
                while ((item = TryMergeItemToMultihexMesh(map, item, false)) != nullptr) {
                    merges++;
                }
            }
        }
    }

    // Rest merge clean items
    for (auto* item : copy_hold_ref(map->GetItems())) {
        if (!item->IsDestroyed() && item->GetMultihexGeneration() != MultihexGenerationType::None) {
            while ((item = TryMergeItemToMultihexMesh(map, item, false)) != nullptr) {
                merges++;
            }
        }
    }

    // Normalize mutihex meshes (sort and move origin to first entry)
    const auto hex_less = [](auto&& hex1, auto&& hex2) { return hex1.y == hex2.y ? hex1.x < hex2.x : hex1.y < hex2.y; };

    for (auto& item : map->GetItems()) {
        if (item->IsNonEmptyMultihexMesh()) {
            auto multihex_mesh = item->GetMultihexMesh();
            std::ranges::sort(multihex_mesh, hex_less);

            if (hex_less(multihex_mesh.front(), item->GetHex())) {
                const auto hex = multihex_mesh.front();
                multihex_mesh.front() = item->GetHex();
                std::ranges::sort(multihex_mesh, hex_less);
                item->SetMultihexMesh(multihex_mesh);
                map->MoveItem(item.get(), hex);
            }
            else {
                item->SetMultihexMesh(multihex_mesh);
            }
        }
    }

    map->DefferedRefreshItems();
    return merges;
}

auto MapperEngine::TryMergeItemToMultihexMesh(MapView* map, ItemHexView* item, bool skip_selected) -> ItemHexView*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (item->GetMultihexGeneration() == MultihexGenerationType::None) {
        return nullptr;
    }

    ItemHexView* source_item = nullptr;
    ItemHexView* target_item = nullptr;

    // Find mergable item by selected strategy
    // Always prefer merge from higher ids to lower
    if (item->GetMultihexGeneration() == MultihexGenerationType::SameSibling) {
        unordered_set<ItemHexView*> target_items;
        unordered_set<ItemHexView*> source_items;
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

        auto sorted_target_items = vec_sorted(target_items, [](auto&& i1, auto&& i2) { return i1->GetId() < i2->GetId(); });
        auto sorted_source_items = vec_sorted(source_items, [](auto&& i1, auto&& i2) { return i1->GetId() > i2->GetId(); });

        if (skip_selected) {
            sorted_target_items = vec_filter(sorted_target_items, [&](auto&& i) { return !SelectedEntitiesSet.contains(i); });
            sorted_source_items = vec_filter(sorted_source_items, [&](auto&& i) { return !SelectedEntitiesSet.contains(i); });
        }

        auto* best_target_item = !sorted_target_items.empty() ? sorted_target_items.front() : nullptr;
        auto* best_source_item = !sorted_source_items.empty() ? sorted_source_items.front() : nullptr;

        if (best_target_item != nullptr && (best_source_item == nullptr || best_target_item->GetId() < item->GetId())) {
            source_item = item;
            target_item = best_target_item;
        }
        else if (best_source_item != nullptr) {
            source_item = best_source_item;
            target_item = item;
        }
    }
    else if (item->GetMultihexGeneration() == MultihexGenerationType::AnyUnique) {
        for (auto& check_item : map->GetItems() | std::views::reverse) {
            if (check_item->GetProtoId() != item->GetProtoId()) {
                continue;
            }
            if (skip_selected && SelectedEntitiesSet.contains(check_item.get())) {
                continue;
            }

            if (CompareMultihexItemForMerge(check_item.get(), item, false)) {
                source_item = check_item->GetId() > item->GetId() ? check_item.get() : item;
                target_item = check_item->GetId() > item->GetId() ? item : check_item.get();
                break;
            }
        }
    }

    // Merge item and his mesh to another item mesh
    if (source_item != nullptr && target_item != nullptr) {
        MergeItemToMultihexMesh(map, source_item, target_item);
        return target_item;
    }

    return nullptr;
}

void MapperEngine::MergeItemToMultihexMesh(MapView* map, ItemHexView* source_item, ItemHexView* target_item)
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

void MapperEngine::FindMultihexMeshForItemAroundHex(MapView* map, ItemHexView* item, mpos hex, bool merge_to_it, unordered_set<ItemHexView*>& result) const
{
    FO_STACK_TRACE_ENTRY();

    const auto find_mergable_item_on_hex = [&](mpos check_hex) -> ItemHexView* {
        if (!map->GetSize().is_valid_pos(check_hex)) {
            return nullptr;
        }

        for (auto& check_item : map->GetItemsOnHex(check_hex)) {
            if (result.contains(check_item.get())) {
                continue;
            }
            if (check_item->GetProtoId() != item->GetProtoId()) {
                continue;
            }

            if (CompareMultihexItemForMerge(merge_to_it ? check_item.get() : item, merge_to_it ? item : check_item.get(), true)) {
                return check_item.get();
            }
        }

        return nullptr;
    };

    // At same hex
    if (auto* mergable_item = find_mergable_item_on_hex(hex); mergable_item != nullptr) {
        result.emplace(mergable_item);
    }

    // Neighbor hexes
    for (int32 i = 0; i < GameSettings::MAP_DIR_COUNT; i++) {
        if (mpos hex2 = hex; GeometryHelper::MoveHexByDir(hex2, numeric_cast<uint8>(i), map->GetSize())) {
            if (auto* mergable_item = find_mergable_item_on_hex(hex2); mergable_item != nullptr) {
                result.emplace(mergable_item);
            }
        }
    }
}

auto MapperEngine::CompareMultihexItemForMerge(const ItemHexView* source_item, const ItemHexView* target_item, bool allow_clean_merge) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (source_item->GetId() == target_item->GetId()) {
        return false;
    }
    if (source_item->GetProtoId() != target_item->GetProtoId()) {
        return false;
    }

    // Our data is not modified (same as in proto)
    auto ignore_props = vector<const Property*> {source_item->GetPropertyHex(), source_item->GetPropertyMultihexMesh()};

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

auto MapperEngine::BreakItemsMultihexMeshes(MapView* map) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    size_t breaks = 0;

    for (auto& item : to_vector(map->GetItems())) {
        if (!item->IsNonEmptyMultihexMesh()) {
            continue;
        }

        const auto multihex_mesh = item->GetMultihexMesh();
        item->SetMultihexMesh({});

        for (const auto multihex : multihex_mesh) {
            map->AddMapperItem(item->GetProtoId(), multihex, &item->GetProperties());
            breaks++;
        }

        map->RefreshItem(item.get(), true);
    }

    map->DefferedRefreshItems();
    return breaks;
}

auto MapperEngine::TryBreakItemFromMultihexMesh(MapView* map, ItemHexView* item, mpos hex) -> ItemHexView*
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
        auto* separated_item = map->AddMapperItem(item->GetProtoId(), item->GetHex(), &item->GetProperties());

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
                auto* separated_item = map->AddMapperItem(item->GetProtoId(), multihex, &item->GetProperties());

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

    BufferRawHex = _curMap->GetScreenRawHex();
    EntitiesBuffer.clear();

    // Add entities to buffer
    function<void(EntityBuf*, ClientEntity*)> add_entity;
    add_entity = [&add_entity, this](EntityBuf* entity_buf, ClientEntity* entity) {
        mpos hex;

        if (const auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
            hex = cr->GetHex();
        }
        else if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
            hex = item->GetHex();
        }

        entity_buf->Hex = hex;
        entity_buf->IsCritter = dynamic_cast<CritterHexView*>(entity) != nullptr;
        entity_buf->IsItem = dynamic_cast<ItemHexView*>(entity) != nullptr;
        entity_buf->Proto = dynamic_cast<EntityWithProto*>(entity)->GetProto();
        entity_buf->Props = SafeAlloc::MakeUnique<Properties>(entity->GetProperties().Copy());

        for (auto& child : GetEntityInnerItems(entity)) {
            auto child_buf = SafeAlloc::MakeUnique<EntityBuf>();
            add_entity(child_buf.get(), child.get());
            entity_buf->Children.emplace_back(std::move(child_buf));
        }
    };

    for (auto& entity : SelectedEntities) {
        EntitiesBuffer.emplace_back();
        add_entity(&EntitiesBuffer.back(), entity.get());
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

    vector<EntityBuf> pasted_entities;

    const ipos32 screen_raw_hex = _curMap->GetScreenRawHex();
    const auto hx_offset = screen_raw_hex.x - BufferRawHex.x;
    const auto hy_offset = screen_raw_hex.y - BufferRawHex.y;

    SelectClear();

    for (const auto& entity_buf : EntitiesBuffer) {
        const auto raw_hx = numeric_cast<int32>(entity_buf.Hex.x) + hx_offset;
        const auto raw_hy = numeric_cast<int32>(entity_buf.Hex.y) + hy_offset;

        if (!_curMap->GetSize().is_valid_pos(raw_hx, raw_hy)) {
            continue;
        }

        const mpos hex = _curMap->GetSize().from_raw_pos(raw_hx, raw_hy);

        function<void(const EntityBuf*, ItemView*)> add_item_inner_items;

        add_item_inner_items = [&add_item_inner_items, this](const EntityBuf* item_entity_buf, ItemView* item) {
            for (const auto& child_buf : item_entity_buf->Children) {
                auto* inner_item = item->AddMapperInnerItem(_curMap->GenTempEntityId(), child_buf->Proto.dyn_cast<const ProtoItem>().get(), {}, child_buf->Props.get());
                add_item_inner_items(child_buf.get(), inner_item);
            }
        };

        if (entity_buf.IsCritter) {
            auto* cr = _curMap->AddMapperCritter(entity_buf.Proto->GetProtoId(), hex, 0, entity_buf.Props.get());

            for (const auto& child_buf : entity_buf.Children) {
                auto* inv_item = cr->AddMapperInvItem(_curMap->GenTempEntityId(), child_buf->Proto.dyn_cast<const ProtoItem>().get(), CritterItemSlot::Inventory, child_buf->Props.get());
                add_item_inner_items(child_buf.get(), inv_item);
            }

            SelectAdd(cr);

            pasted_entities.emplace_back();
            CaptureEntityBuf(pasted_entities.back(), cr);
        }
        else if (entity_buf.IsItem) {
            auto* item = _curMap->AddMapperItem(entity_buf.Proto->GetProtoId(), hex, entity_buf.Props.get());
            add_item_inner_items(&entity_buf, item);
            SelectAdd(item);

            pasted_entities.emplace_back();
            CaptureEntityBuf(pasted_entities.back(), item);
        }
    }

    if (!pasted_entities.empty()) {
        PushUndoOp(_curMap.get(),
            UndoOp {"Paste selection",
                [pasted_entities](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                    for (auto it = pasted_entities.rbegin(); it != pasted_entities.rend(); ++it) {
                        if (auto* target = mapper.FindEntityById(active_map, it->Id); target != nullptr) {
                            mapper.DeleteEntity(target);
                        }
                    }

                    return true;
                },
                [pasted_entities](MapperEngine& mapper, raw_ptr<MapView>& active_map) {
                    for (const auto& entity_buf : pasted_entities) {
                        if (mapper.RestoreEntityBuf(entity_buf) == nullptr) {
                            return false;
                        }
                    }

                    mapper.SetMapDirty(active_map.get());
                    return true;
                }});
    }
}

auto MapperEngine::JumpHistoryToIndex(int32 target_index) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_curMap == nullptr) {
        return false;
    }

    const auto* ctx = GetUndoContext(_curMap.get(), false);
    const auto total_count = ctx != nullptr ? numeric_cast<int32>(ctx->UndoStack.size() + ctx->RedoStack.size()) : 0;
    target_index = std::clamp(target_index, 0, total_count);

    while (true) {
        const auto* cur_ctx = GetUndoContext(_curMap.get(), false);
        const auto applied_count = cur_ctx != nullptr ? numeric_cast<int32>(cur_ctx->UndoStack.size()) : 0;

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

    if (_curMap == nullptr) {
        ImGui::TextUnformatted("No map loaded");
        ImGui::End();
        return;
    }

    const auto* ctx = GetUndoContext(_curMap.get(), false);
    const auto undo_count = ctx != nullptr ? numeric_cast<int32>(ctx->UndoStack.size()) : 0;
    const auto redo_count = ctx != nullptr ? numeric_cast<int32>(ctx->RedoStack.size()) : 0;
    const auto total_count = undo_count + redo_count;

    auto& last_history_map = LastHistoryMap;
    int32& last_history_undo_count = LastHistoryUndoCount;
    const auto scroll_to_current = ImGui::IsWindowAppearing() || last_history_map != _curMap.get() || last_history_undo_count != undo_count;

    last_history_map = _curMap.get();
    last_history_undo_count = undo_count;

    ImGui::Text("Applied: %d / %d", undo_count, total_count);
    ImGui::Separator();

    const auto draw_history_button = [&](string_view label, int32 target_index, bool is_current) {
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

        if (ctx != nullptr && undo_count > 0) {
            ImGui::SeparatorText("Undo");

            for (int32 index = 0; index < undo_count; index++) {
                const auto label = strex("{}. {}", index + 1, ctx->UndoStack[numeric_cast<size_t>(index)].Label).str();
                draw_history_button(label, index + 1, false);
            }
        }

        ImGui::SeparatorText("Current position");
        ImGui::TextDisabled("Applied commands: %d", undo_count);
        if (scroll_to_current) {
            ImGui::SetScrollHereY(0.5f);
        }

        if (ctx != nullptr && redo_count > 0) {
            ImGui::SeparatorText("Redo");

            for (int32 index = 0; index < redo_count; index++) {
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

void MapperEngine::DrawStr(const irect32& rect, string_view str, uint32 flags, ucolor color, int32 num_font)
{
    FO_STACK_TRACE_ENTRY();

    SprMngr.DrawText({rect.x, rect.y, rect.width, rect.height}, str, flags, color, num_font);
}

void MapperEngine::CurDraw()
{
    FO_STACK_TRACE_ENTRY();

    if (_curMap == nullptr) {
        return;
    }

    const auto can_place_object = CurMode == CUR_MODE_PLACE_OBJECT && ((IsItemMode() && ActiveItemProtos != nullptr && !ActiveItemProtos->empty()) || (IsCritMode() && ActiveCritterProtos != nullptr && !ActiveCritterProtos->empty()));

    if (!can_place_object) {
        return;
    }

    if (IsItemMode() && ActiveItemProtos != nullptr && !ActiveItemProtos->empty()) {
        const auto& proto = (*ActiveItemProtos)[GetActiveProtoIndex()];

        mpos hex;

        if (!_curMap->GetHexAtScreen(Settings.MousePos, hex, nullptr)) {
            return;
        }

        if (proto->GetIsTile()) {
            hex = _curMap->GetSize().from_raw_pos(hex.x - hex.x % Settings.MapTileStep, hex.y - hex.y % Settings.MapTileStep);
        }

        const auto* spr = GetPreviewSprite(proto->GetPicMap());

        if (spr != nullptr) {
            const auto zoom = _curMap->GetSpritesZoom();
            ipos32 pos = _curMap->MapToScreenPos(_curMap->GetHexMapPos(hex));
            pos += ipos32(iround<int32>(numeric_cast<float32>(proto->GetOffset().x) * zoom), iround<int32>(numeric_cast<float32>(proto->GetOffset().y) * zoom));
            pos += ipos32(iround<int32>(numeric_cast<float32>(spr->GetOffset().x) * zoom), iround<int32>(numeric_cast<float32>(spr->GetOffset().y) * zoom));
            pos += ipos32(iround<int32>(numeric_cast<float32>(Settings.MapHexWidth / 2) * zoom), iround<int32>(numeric_cast<float32>(Settings.MapHexHeight) * zoom));
            pos -= ipos32(iround<int32>(numeric_cast<float32>(spr->GetSize().width / 2) * zoom), iround<int32>(numeric_cast<float32>(spr->GetSize().height) * zoom));

            if (proto->GetIsTile()) {
                if (PreviewRoofTiles) {
                    pos.x += iround<int32>(numeric_cast<float32>(Settings.MapRoofOffsX) * zoom);
                    pos.y += iround<int32>(numeric_cast<float32>(Settings.MapRoofOffsY) * zoom);
                }
                else {
                    pos.x += iround<int32>(numeric_cast<float32>(Settings.MapTileOffsX) * zoom);
                    pos.y += iround<int32>(numeric_cast<float32>(Settings.MapTileOffsY) * zoom);
                }
            }

            const auto width = iround<int32>(numeric_cast<float32>(spr->GetSize().width) * zoom);
            const auto height = iround<int32>(numeric_cast<float32>(spr->GetSize().height) * zoom);
            SprMngr.DrawSpriteSize(spr, pos, {width, height}, true, false, COLOR_SPRITE);
        }
        return;
    }

    if (IsCritMode() && ActiveCritterProtos != nullptr && !ActiveCritterProtos->empty()) {
        const auto model_name = (*ActiveCritterProtos)[GetActiveProtoIndex()]->GetModelName();
        const auto* anim = ResMngr.GetCritterPreviewSpr(model_name, CritterStateAnim::Unarmed, CritterActionAnim::Idle, CritterDir, nullptr);

        if (anim == nullptr) {
            anim = ResMngr.GetItemDefaultSpr().get();
        }

        mpos hex;

        if (!_curMap->GetHexAtScreen(Settings.MousePos, hex, nullptr)) {
            return;
        }

        const auto zoom = _curMap->GetSpritesZoom();
        ipos32 pos = _curMap->MapToScreenPos(_curMap->GetHexMapPos(hex));
        pos += ipos32(iround<int32>(numeric_cast<float32>(anim->GetOffset().x) * zoom), iround<int32>(numeric_cast<float32>(anim->GetOffset().y) * zoom));
        pos -= ipos32(iround<int32>(numeric_cast<float32>(anim->GetSize().width / 2) * zoom), iround<int32>(numeric_cast<float32>(anim->GetSize().height) * zoom));

        const auto width = iround<int32>(numeric_cast<float32>(anim->GetSize().width) * zoom);
        const auto height = iround<int32>(numeric_cast<float32>(anim->GetSize().height) * zoom);
        SprMngr.DrawSpriteSize(anim, pos, {width, height}, true, false, COLOR_SPRITE);
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
        if (Settings.ScreenWidth == resolution.width && Settings.ScreenHeight == resolution.height) {
            return;
        }

        SprMngr.SetScreenSize(resolution);
        SprMngr.SetWindowSize(resolution);
        UpdateLocalConfigValue(Cache, "ScreenWidth", strex("{}", resolution.width));
        UpdateLocalConfigValue(Cache, "ScreenHeight", strex("{}", resolution.height));
        AddMess(strex("Resolution changed to {}x{}", resolution.width, resolution.height));
    };

    ImGui::Text("Current resolution: %d x %d", Settings.ScreenWidth, Settings.ScreenHeight);
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
            const auto is_current = Settings.ScreenWidth == res.width && Settings.ScreenHeight == res.height;
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
            const auto sample_ms = (now_time - RightMouseVelocityTime).to_ms<float32>();

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
        for (auto& entity : SelectedEntities) {
            if (auto cr = entity.dyn_cast<CritterHexView>()) {
                AdvanceCritterDir(cr.get());
            }
        }
    }
}

void MapperEngine::SetCurMode(int cur_mode)
{
    FO_STACK_TRACE_ENTRY();

    CurMode = cur_mode;

    // Restore alpha
    for (auto& entity : SelectedEntities) {
        if (auto* hex_view = dynamic_cast<HexView*>(entity.get()); hex_view != nullptr) {
            if (CurMode == CUR_MODE_MOVE_SELECTION) {
                hex_view->RestoreAlpha();
            }
            else {
                hex_view->SetTargetAlpha(SelectAlpha);
            }
        }
    }
}

auto MapperEngine::IsCurInRect(const irect32& rect, int32 ax, int32 ay) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return Settings.MousePos.x >= rect.x + ax && Settings.MousePos.y >= rect.y + ay && Settings.MousePos.x < rect.x + rect.width + ax && Settings.MousePos.y < rect.y + rect.height + ay;
}

auto MapperEngine::IsCurInRect(const irect32& rect) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return Settings.MousePos.x >= rect.x && Settings.MousePos.y >= rect.y && Settings.MousePos.x < rect.x + rect.width && Settings.MousePos.y < rect.y + rect.height;
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

    return _curMap->GetHexAtScreen(Settings.MousePos, hex, nullptr);
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

    const auto base_y = numeric_cast<float32>(MainPanelPos.y + MainPanelWindowRect.height + MAPPER_CONSOLE_WINDOW_OFFSET.y);
    const auto base_x = numeric_cast<float32>(MainPanelPos.x + MAPPER_CONSOLE_WINDOW_OFFSET.x);

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
                if (ConsoleHistoryCur + 1 >= numeric_cast<int32>(ConsoleHistory.size())) {
                    ConsoleHistoryCur = numeric_cast<int32>(ConsoleHistory.size());
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

    for (int32 i = 0; i < numeric_cast<int32>(ConsoleHistory.size()) - 1; i++) {
        if (ConsoleHistory[i] == ConsoleHistory[ConsoleHistory.size() - 1]) {
            ConsoleHistory.erase(ConsoleHistory.begin() + i);
            i = -1;
        }
    }

    while (numeric_cast<int32>(ConsoleHistory.size()) > Settings.ConsoleHistorySize) {
        ConsoleHistory.erase(ConsoleHistory.begin());
    }

    ConsoleHistoryCur = numeric_cast<int32>(ConsoleHistory.size());

    string history_str;
    for (const auto& str : ConsoleHistory) {
        history_str += str + "\n";
    }
    Cache.SetString("mapper_console.txt", history_str);

    const auto process_command = OnMapperMessage.Fire(ConsoleStr);
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

        if (auto* map = LoadMap(map_name); map != nullptr) {
            AddMess("Load map success");
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

        SaveMap(_curMap.get(), map_name);

        AddMess("Save map success");
    }
    // Run script
    else if (command[0] == '#') {
        const auto before_snapshot = _curMap && !UndoRedoInProgress ? CaptureMapSnapshot(_curMap.get()) : string {};
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
            const auto after_snapshot = CaptureMapSnapshot(_curMap.get());
            if (before_snapshot != after_snapshot) {
                PushUndoOp(_curMap.get(), UndoOp {strex("Script {}", func_name), [map_name = string(_curMap->GetName()), before_snapshot](MapperEngine& mapper, raw_ptr<MapView>& active_map) { return mapper.RestoreMapSnapshot(active_map, map_name, before_snapshot); }, [map_name = string(_curMap->GetName()), after_snapshot](MapperEngine& mapper, raw_ptr<MapView>& active_map) { return mapper.RestoreMapSnapshot(active_map, map_name, after_snapshot); }, true});
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

        vector<int32> anims = strvex(command.substr(1)).split_to_int32(' ');

        if (anims.empty()) {
            return;
        }

        if (!SelectedEntities.empty()) {
            for (auto& entity : SelectedEntities) {
                if (auto cr = entity.dyn_cast<CritterHexView>()) {
                    cr->StopAnim();

                    for (size_t j = 0; j < anims.size() / 2; j++) {
                        cr->AppendAnim(static_cast<CritterStateAnim>(anims[numeric_cast<size_t>(j) * 2]), static_cast<CritterActionAnim>(anims[j * 2 + 1]), nullptr);
                    }
                }
            }
        }
        else {
            for (auto& cr : _curMap->GetCritters()) {
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
            auto pmap = SafeAlloc::MakeRefCounted<ProtoMap>(Hashes.ToHashedString("new"), GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME));
            pmap->AddRef(); // Todo: fix memleak
            pmap->SetSize({GameSettings::DEFAULT_MAP_SIZE, GameSettings::DEFAULT_MAP_SIZE});

            auto map = SafeAlloc::MakeRefCounted<MapView>(this, ident_t {}, pmap.get());
            map->EnableMapperMode();
            map->InstantScrollTo({GameSettings::DEFAULT_MAP_SIZE / 2, GameSettings::DEFAULT_MAP_SIZE / 2});

            LoadedMaps.emplace_back(std::move(map));
            ShowMap(LoadedMaps.back().get());
            SetMapDirty(LoadedMaps.back().get());
            AddMess("Create map success");
        }
        else if (command_ext == "unload" && _curMap) {
            AddMess("Unload map");

            UnloadMap(_curMap.get());

            if (!LoadedMaps.empty()) {
                ShowMap(LoadedMaps.front().get());
            }
        }
        else if (command_ext == "size" && _curMap) {
            AddMess("Resize map");

            int32 maxhx = 0;
            int32 maxhy = 0;

            if (!(icommand >> maxhx >> maxhy)) {
                AddMess("Invalid args");
                return;
            }

            ResizeMap(_curMap.get(), maxhx, maxhy);
        }
        else if (command_ext == "resave") {
            AddMess("Resave maps");

            auto map_files = MapsFileSys.FilterFiles("fomap");

            for (const auto& map_file_header : map_files) {
                const auto map_name = map_file_header.GetNameNoExt();

                if (auto* map = LoadMap(map_name); map != nullptr) {
                    SaveMap(map, map_name);
                    AddMess(strex("Resave map: {}", map_name));
                }
                else {
                    AddMess(strex("Failed to load map: {}", map_name));
                }
            }
        }
        else if (command_ext == "reverse-light" && _curMap) {
            const auto before_snapshot = !UndoRedoInProgress ? CaptureMapSnapshot(_curMap.get()) : string {};

            for (auto& item : _curMap->GetItems()) {
                auto rgba = item->GetLightColor().rgba;
                rgba = (rgba & 0xFF000000) | ((rgba & 0xFF) << 16) | (rgba & 0xFF00) | ((rgba & 0xFF0000) >> 16);
                item->SetLightColor(ucolor(rgba));
            }

            _curMap->RebuildMap();
            SetMapDirty(_curMap.get());

            if (!before_snapshot.empty()) {
                const auto after_snapshot = CaptureMapSnapshot(_curMap.get());
                if (before_snapshot != after_snapshot) {
                    PushUndoOp(_curMap.get(), UndoOp {"Reverse lights", [map_name = string(_curMap->GetName()), before_snapshot](MapperEngine& mapper, raw_ptr<MapView>& active_map) { return mapper.RestoreMapSnapshot(active_map, map_name, before_snapshot); }, [map_name = string(_curMap->GetName()), after_snapshot](MapperEngine& mapper, raw_ptr<MapView>& active_map) { return mapper.RestoreMapSnapshot(active_map, map_name, after_snapshot); }, true});
                }
            }
        }
        else if (command_ext == "merge-items" && _curMap) {
            const auto before_snapshot = !UndoRedoInProgress ? CaptureMapSnapshot(_curMap.get()) : string {};
            MergeItemsToMultihexMeshes(_curMap.get());
            FO_RUNTIME_ASSERT(MergeItemsToMultihexMeshes(_curMap.get()) == 0);
            SetMapDirty(_curMap.get());

            if (!before_snapshot.empty()) {
                const auto after_snapshot = CaptureMapSnapshot(_curMap.get());
                if (before_snapshot != after_snapshot) {
                    PushUndoOp(_curMap.get(), UndoOp {"Merge items", [map_name = string(_curMap->GetName()), before_snapshot](MapperEngine& mapper, raw_ptr<MapView>& active_map) { return mapper.RestoreMapSnapshot(active_map, map_name, before_snapshot); }, [map_name = string(_curMap->GetName()), after_snapshot](MapperEngine& mapper, raw_ptr<MapView>& active_map) { return mapper.RestoreMapSnapshot(active_map, map_name, after_snapshot); }, true});
                }
            }
        }
        else if (command_ext == "break-items" && _curMap) {
            const auto before_snapshot = !UndoRedoInProgress ? CaptureMapSnapshot(_curMap.get()) : string {};
            BreakItemsMultihexMeshes(_curMap.get());
            FO_RUNTIME_ASSERT(BreakItemsMultihexMeshes(_curMap.get()) == 0);
            SetMapDirty(_curMap.get());

            if (!before_snapshot.empty()) {
                const auto after_snapshot = CaptureMapSnapshot(_curMap.get());
                if (before_snapshot != after_snapshot) {
                    PushUndoOp(_curMap.get(), UndoOp {"Break items", [map_name = string(_curMap->GetName()), before_snapshot](MapperEngine& mapper, raw_ptr<MapView>& active_map) { return mapper.RestoreMapSnapshot(active_map, map_name, before_snapshot); }, [map_name = string(_curMap->GetName()), after_snapshot](MapperEngine& mapper, raw_ptr<MapView>& active_map) { return mapper.RestoreMapSnapshot(active_map, map_name, after_snapshot); }, true});
                }
            }
        }
    }
    else {
        AddMess("Unknown command");
    }
}

auto MapperEngine::LoadMapFromText(string_view map_name, const string& map_text) -> MapView*
{
    FO_STACK_TRACE_ENTRY();

    const auto map_data = ConfigFile(strex("{}.fomap", map_name), map_text, &Hashes, ConfigFileOption::ReadFirstSection);

    if (!map_data.HasSection("ProtoMap")) {
        throw MapLoaderException("Invalid map format", map_name);
    }

    auto pmap = SafeAlloc::MakeRefCounted<ProtoMap>(Hashes.ToHashedString(map_name), GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME));
    pmap->AddRef(); // Todo: fix memleak
    pmap->GetPropertiesForEdit().ApplyFromText(map_data.GetSection("ProtoMap"));

    auto new_map = SafeAlloc::MakeRefCounted<MapView>(this, ident_t {}, pmap.get());
    new_map->EnableMapperMode();

    try {
        new_map->LoadFromFile(map_name, map_text);
    }
    catch (const MapLoaderException& ex) {
        AddMess(strex("Map truncated: {}", ex.what()));
        return nullptr;
    }

    MergeItemsToMultihexMeshes(new_map.get());
    FO_RUNTIME_ASSERT(MergeItemsToMultihexMeshes(new_map.get()) == 0);

    new_map->InstantScrollTo(new_map->GetWorkHex());
    OnEditMapLoad.Fire(new_map.get());
    LoadedMaps.emplace_back(std::move(new_map));
    GetUndoContext(LoadedMaps.back().get(), true)->CleanUndoDepth = 0;
    SetMapDirty(LoadedMaps.back().get(), false);

    return LoadedMaps.back().get();
}

auto MapperEngine::LoadMap(string_view map_name) -> MapView*
{
    FO_STACK_TRACE_ENTRY();

    const auto map_files = MapsFileSys.FilterFiles("fomap");
    const auto map_file = map_files.FindFileByName(map_name);

    if (!map_file) {
        AddMess("Map file not found");
        return nullptr;
    }

    return LoadMapFromText(map_name, map_file.GetStr());
}

void MapperEngine::ShowMap(MapView* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!map->IsDestroyed());

    const auto it = std::ranges::find(LoadedMaps, map);
    FO_RUNTIME_ASSERT(it != LoadedMaps.end());

    WorkspaceWindowVisible = true;
    MapListWindowVisible = false;

    if (_curMap != map) {
        if (_curMap) {
            SelectClear();
        }

        _curMap = map;
        RefreshActiveProtoLists();
    }
}

auto MapperEngine::IsMapDirty(const MapView* map) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return map != nullptr && DirtyMaps.contains(const_cast<MapView*>(map));
}

void MapperEngine::SetMapDirty(MapView* map, bool dirty)
{
    FO_STACK_TRACE_ENTRY();

    if (map == nullptr) {
        return;
    }

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

    if (_curMap == nullptr) {
        return;
    }

    SaveMap(_curMap.get(), "");
    AddMess(strex("Saved map: {}", _curMap->GetName()));
}

void MapperEngine::ResetCurrentMapChanges()
{
    FO_STACK_TRACE_ENTRY();

    if (_curMap == nullptr) {
        return;
    }

    const auto map_name = string(_curMap->GetName());
    ClearUndoContext(_curMap.get());
    UnloadMap(_curMap.get());

    if (auto* map = LoadMap(map_name); map != nullptr) {
        ShowMap(map);
        AddMess(strex("Reset changes: {}", map_name));
    }
    else {
        AddMess(strex("Reset changes failed: {}", map_name));
    }
}

void MapperEngine::SaveMap(MapView* map, string_view custom_name)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!map->IsDestroyed());

    MergeItemsToMultihexMeshes(map);
    FO_RUNTIME_ASSERT(MergeItemsToMultihexMeshes(map) == 0);

    const auto it = std::ranges::find(LoadedMaps, map);
    FO_RUNTIME_ASSERT(it != LoadedMaps.end());

    const auto fomap_content = map->SaveToText();

    const auto fomap_name = !custom_name.empty() ? custom_name : map->GetProto()->GetName();
    FO_RUNTIME_ASSERT(!fomap_name.empty());

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
        FO_RUNTIME_ASSERT(dir_ok);
    }

    std::ofstream fomap_file {std::filesystem::path {fs_make_path(fomap_path)}, std::ios::binary | std::ios::trunc};
    FO_RUNTIME_ASSERT(fomap_file);
    fomap_file.write(fomap_content.data(), static_cast<std::streamsize>(fomap_content.size()));
    FO_RUNTIME_ASSERT(fomap_file);

    OnEditMapSave.Fire(map);
    auto* ctx = GetUndoContext(map, true);
    ctx->CleanUndoDepth = numeric_cast<int32>(ctx->UndoStack.size());
    SetMapDirty(map, false);
}

void MapperEngine::UnloadMap(MapView* map, bool clear_undo)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!map->IsDestroyed());

    if (_curMap == map) {
        SelectClear();
        _curMap = nullptr;
    }

    const auto it = std::ranges::find(LoadedMaps, map);
    FO_RUNTIME_ASSERT(it != LoadedMaps.end());

    SetMapDirty(map, false);
    if (clear_undo) {
        ClearUndoContext(map);
    }
    map->MarkAsDestroyed();
    LoadedMaps.erase(it);
}

void MapperEngine::ResizeMap(MapView* map, int32 width, int32 height)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!map->IsDestroyed());

    const auto before_snapshot = !UndoRedoInProgress ? CaptureMapSnapshot(map) : string {};

    const auto corrected_width = std::clamp(width, GameSettings::MIN_MAP_SIZE, GameSettings::MAX_MAP_SIZE);
    const auto corrected_height = std::clamp(height, GameSettings::MIN_MAP_SIZE, GameSettings::MAX_MAP_SIZE);

    map->Resize(msize(numeric_cast<int16>(corrected_width), numeric_cast<int16>(corrected_height)));
    map->InstantScrollTo(map->GetWorkHex());

    if (_curMap == map) {
        SelectClear();
    }

    SetMapDirty(map);

    const auto after_snapshot = CaptureMapSnapshot(map);
    if (!before_snapshot.empty() && before_snapshot != after_snapshot) {
        PushUndoOp(map, UndoOp {"Resize map", [map_name = string(map->GetName()), before_snapshot](MapperEngine& mapper, raw_ptr<MapView>& active_map) { return mapper.RestoreMapSnapshot(active_map, map_name, before_snapshot); }, [map_name = string(map->GetName()), after_snapshot](MapperEngine& mapper, raw_ptr<MapView>& active_map) { return mapper.RestoreMapSnapshot(active_map, map_name, after_snapshot); }, true});
    }
}

void MapperEngine::AddMess(string_view message_text)
{
    FO_STACK_TRACE_ENTRY();

    const string str = strex("|{} - {}\n", COLOR_TEXT, message_text);
    const auto time = nanotime::now().desc(true);
    const string mess_time = strex("{:02}:{:02}:{:02} ", time.hour, time.minute, time.second);

    MessBox.emplace_back(MessBoxMessage {.Type = 0, .Mess = str, .Time = mess_time});
    MessBoxScroll = 0;
    MessBoxCurText = "";

    const irect32 ir(MainPanelContentRect.x + MainPanelPos.x, MainPanelContentRect.y + MainPanelPos.y, MainPanelContentRect.width, MainPanelContentRect.height);
    int32 max_lines = ir.height / 10;

    if (ir == irect32()) {
        max_lines = 20;
    }

    int32 cur_mess = numeric_cast<int32>(MessBox.size()) - 1;

    for (int32 i = 0, j = 0; cur_mess >= 0; cur_mess--) {
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

    DrawStr(irect32(MainPanelContentRect.x + MainPanelPos.x, MainPanelContentRect.y + MainPanelPos.y, MainPanelContentRect.width, MainPanelContentRect.height), MessBoxCurText, FT_UPPER | FT_BOTTOM, COLOR_TEXT, FONT_DEFAULT);
}

void MapperEngine::DrawIfaceLayer(int32 layer)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(layer);

    SpritesCanDraw = true;
    OnRenderIface.Fire(); // Todo: mapper render iface layer
    SpritesCanDraw = false;
}

auto MapperEngine::GetEntityInnerItems(ClientEntity* entity) const -> vector<refcount_ptr<ItemView>>
{
    FO_STACK_TRACE_ENTRY();

    if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
        return cr->GetInvItems();
    }
    if (auto* item = dynamic_cast<ItemView*>(entity); item != nullptr) {
        return item->GetInnerItems();
    }

    return {};
}

FO_END_NAMESPACE
