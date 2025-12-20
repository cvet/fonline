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

#include "Mapper.h"
#include "3dStuff.h"
#include "Baker.h"
#include "ParticleSprites.h"

FO_BEGIN_NAMESPACE();

extern void Mapper_RegisterData(EngineData*);

#if FO_ANGELSCRIPT_SCRIPTING
extern void Init_AngelScript_MapperScriptSystem(BaseEngine*);
#endif

static auto GetMapperResources(GlobalSettings& settings) -> FileSystem
{
    FO_STACK_TRACE_ENTRY();

    if (IsPackaged()) {
        FileSystem resources;
        resources.AddPacksSource(settings.ClientResources, settings.ClientResourceEntries);
        resources.AddPacksSource(settings.ClientResources, settings.MapperResourceEntries);
        return resources;
    }
    else {
        FileSystem resources;
        resources.AddCustomSource(SafeAlloc::MakeUnique<BakerDataSource>(settings));
        return resources;
    }
}

FOMapper::FOMapper(GlobalSettings& settings, AppWindow* window) :
    FOClient(settings, window, GetMapperResources(settings), [this] { Mapper_RegisterData(this); })
{
    FO_STACK_TRACE_ENTRY();

    App->LoadImGuiEffect(Resources);

    MapsFileSys.AddDirSource("", false, true, true);

    for (const auto& res_pack : settings.GetResourcePacks()) {
        for (const auto& dir : res_pack.InputDir) {
            MapsFileSys.AddDirSource(dir, false, true, true);
        }
    }

    EffectMngr.LoadDefaultEffects();

    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<DefaultSpriteFactory>(SprMngr));
    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<ParticleSpriteFactory>(SprMngr, Settings, EffectMngr, GameTime, Hashes));
#if FO_ENABLE_3D
    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<ModelSpriteFactory>(SprMngr, Settings, EffectMngr, GameTime, Hashes, *this, *this));
#endif

    SprMngr.InitializeEgg(Hashes.ToHashedString("TransparentEgg.png"), AtlasType::MapSprites);
    ResMngr.IndexFiles();

    ScriptSys.MapEngineEntityType<PlayerView>(PlayerView::ENTITY_TYPE_NAME);
    ScriptSys.MapEngineEntityType<ItemView, ItemHexView>(ItemView::ENTITY_TYPE_NAME);
    ScriptSys.MapEngineEntityType<CritterView, CritterHexView>(CritterView::ENTITY_TYPE_NAME);
    ScriptSys.MapEngineEntityType<MapView>(MapView::ENTITY_TYPE_NAME);
    ScriptSys.MapEngineEntityType<LocationView>(LocationView::ENTITY_TYPE_NAME);

#if FO_ANGELSCRIPT_SCRIPTING
    Init_AngelScript_MapperScriptSystem(this);
#endif

    ProtoMngr.LoadFromResources(Resources);

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
    const auto& cr_protos = ProtoMngr.GetProtoCritters();

    for (const auto& proto : cr_protos | std::views::values) {
        Tabs[INT_MODE_CRIT][DEFAULT_SUB_TAB].CritterProtos.emplace_back(proto);
        Tabs[INT_MODE_CRIT][proto->CollectionName].CritterProtos.emplace_back(proto);
    }
    for (auto& proto : Tabs[INT_MODE_CRIT] | std::views::values) {
        std::ranges::stable_sort(proto.CritterProtos, [](auto&& a, auto&& b) -> bool { return a->GetName() < b->GetName(); });
    }

    const auto& item_protos = ProtoMngr.GetProtoItems();

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

        TabsActive[i] = &Tabs[i].begin()->second;
    }

    // Initialize tabs scroll and names
    for (auto i = INT_MODE_CUSTOM0; i <= INT_MODE_CUSTOM9; i++) {
        TabsName[i] = "-";
    }

    TabsName[INT_MODE_ITEM] = "Item";
    TabsName[INT_MODE_TILE] = "Tile";
    TabsName[INT_MODE_CRIT] = "Crit";
    TabsName[INT_MODE_FAST] = "Fast";
    TabsName[INT_MODE_IGNORE] = "Ign";
    TabsName[INT_MODE_INCONT] = "Inv";
    TabsName[INT_MODE_MESS] = "Msg";
    TabsName[INT_MODE_LIST] = "Maps";

    // Start script
    ScriptSys.InitModules();
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
    RefreshCurProtos();

    // Load console history
    const auto history_str = Cache.GetString("mapper_console.txt");
    size_t pos = 0;
    size_t prev = 0;

    while ((pos = history_str.find('\n', prev)) != std::string::npos) {
        string history_part;
        history_part.assign(&history_str.c_str()[prev], pos - prev);
        ConsoleHistory.emplace_back(history_part);
        prev = pos + 1;
    }

    ConsoleHistory = strex(history_str).normalize_line_endings().split('\n');

    while (numeric_cast<int32>(ConsoleHistory.size()) > Settings.ConsoleHistorySize) {
        ConsoleHistory.erase(ConsoleHistory.begin());
    }

    ConsoleHistoryCur = numeric_cast<int32>(ConsoleHistory.size());
}

void FOMapper::InitIface()
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Init interface");

    const auto config_content = Resources.ReadFileText("mapper_default.ini");

    IfaceIni = SafeAlloc::MakeUnique<ConfigFile>("mapper_default.ini", config_content);
    const auto& ini = *IfaceIni;

    // Interface
    IntX = ini.GetAsInt("", "IntX", -1);
    IntY = ini.GetAsInt("", "IntY", -1);

    IfaceLoadRect(IntWMain, "IntMain");
    if (IntX == -1) {
        IntX = (Settings.ScreenWidth - IntWMain.width) / 2;
    }
    if (IntY == -1) {
        IntY = Settings.ScreenHeight - IntWMain.height;
    }

    IfaceLoadRect(IntWWork, "IntWork");
    IfaceLoadRect(IntWHint, "IntHint");

    IfaceLoadRect(IntBCust[0], "IntCustom0");
    IfaceLoadRect(IntBCust[1], "IntCustom1");
    IfaceLoadRect(IntBCust[2], "IntCustom2");
    IfaceLoadRect(IntBCust[3], "IntCustom3");
    IfaceLoadRect(IntBCust[4], "IntCustom4");
    IfaceLoadRect(IntBCust[5], "IntCustom5");
    IfaceLoadRect(IntBCust[6], "IntCustom6");
    IfaceLoadRect(IntBCust[7], "IntCustom7");
    IfaceLoadRect(IntBCust[8], "IntCustom8");
    IfaceLoadRect(IntBCust[9], "IntCustom9");
    IfaceLoadRect(IntBItem, "IntItem");
    IfaceLoadRect(IntBTile, "IntTile");
    IfaceLoadRect(IntBCrit, "IntCrit");
    IfaceLoadRect(IntBFast, "IntFast");
    IfaceLoadRect(IntBIgnore, "IntIgnore");
    IfaceLoadRect(IntBInCont, "IntInCont");
    IfaceLoadRect(IntBMess, "IntMess");
    IfaceLoadRect(IntBList, "IntList");
    IfaceLoadRect(IntBScrBack, "IntScrBack");
    IfaceLoadRect(IntBScrBackFst, "IntScrBackFst");
    IfaceLoadRect(IntBScrFront, "IntScrFront");
    IfaceLoadRect(IntBScrFrontFst, "IntScrFrontFst");

    IfaceLoadRect(IntBShowItem, "IntShowItem");
    IfaceLoadRect(IntBShowScen, "IntShowScen");
    IfaceLoadRect(IntBShowWall, "IntShowWall");
    IfaceLoadRect(IntBShowCrit, "IntShowCrit");
    IfaceLoadRect(IntBShowTile, "IntShowTile");
    IfaceLoadRect(IntBShowRoof, "IntShowRoof");
    IfaceLoadRect(IntBShowFast, "IntShowFast");

    IfaceLoadRect(IntBSelectItem, "IntSelectItem");
    IfaceLoadRect(IntBSelectScen, "IntSelectScen");
    IfaceLoadRect(IntBSelectWall, "IntSelectWall");
    IfaceLoadRect(IntBSelectCrit, "IntSelectCrit");
    IfaceLoadRect(IntBSelectTile, "IntSelectTile");
    IfaceLoadRect(IntBSelectRoof, "IntSelectRoof");

    IfaceLoadRect(SubTabsRect, "SubTabs");

    IntVisible = true;
    IntFix = true;
    IntMode = INT_MODE_MESS;
    ProtoWidth = ini.GetAsInt("", "ProtoWidth", 50);
    ProtosOnScreen = IntWWork.width / ProtoWidth;
    MemFill(TabIndex, 0, sizeof(TabIndex));
    CritterDir = 3;
    CurMode = CUR_MODE_DEFAULT;
    IsSelectItem = true;
    IsSelectScen = true;
    IsSelectWall = true;
    IsSelectCrit = true;

    // Object
    IfaceLoadRect(ObjWMain, "ObjMain");
    IfaceLoadRect(ObjWWork, "ObjWork");
    IfaceLoadRect(ObjBToAll, "ObjToAll");

    ObjVisible = true;

    // Console
    ConsolePicX = ini.GetAsInt("", "ConsolePicX", 0);
    ConsolePicY = ini.GetAsInt("", "ConsolePicY", 0);
    ConsoleTextX = ini.GetAsInt("", "ConsoleTextX", 0);
    ConsoleTextY = ini.GetAsInt("", "ConsoleTextY", 0);

    // Cursor
    CurPDef = SprMngr.LoadSprite(ini.GetAsStr("", "CurDefault", "actarrow.frm"), AtlasType::IfaceSprites);
    CurPHand = SprMngr.LoadSprite(ini.GetAsStr("", "CurHand", "hand.frm"), AtlasType::IfaceSprites);

    // Iface
    IntMainPic = SprMngr.LoadSprite(ini.GetAsStr("", "IntMainPic", "error"), AtlasType::IfaceSprites);
    IntPTab = SprMngr.LoadSprite(ini.GetAsStr("", "IntTabPic", "error"), AtlasType::IfaceSprites);
    IntPSelect = SprMngr.LoadSprite(ini.GetAsStr("", "IntSelectPic", "error"), AtlasType::IfaceSprites);
    IntPShow = SprMngr.LoadSprite(ini.GetAsStr("", "IntShowPic", "error"), AtlasType::IfaceSprites);

    // Object
    ObjWMainPic = SprMngr.LoadSprite(ini.GetAsStr("", "ObjMainPic", "error"), AtlasType::IfaceSprites);
    ObjPbToAllDn = SprMngr.LoadSprite(ini.GetAsStr("", "ObjToAllPicDn", "error"), AtlasType::IfaceSprites);

    // Sub tabs
    SubTabsPic = SprMngr.LoadSprite(ini.GetAsStr("", "SubTabsPic", "error"), AtlasType::IfaceSprites);

    // Console
    ConsolePic = SprMngr.LoadSprite(ini.GetAsStr("", "ConsolePic", "error"), AtlasType::IfaceSprites);

    WriteLog("Init interface complete");
}

auto FOMapper::IfaceLoadRect(irect32& rect, string_view name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto res = string(IfaceIni->GetAsStr("", name));

    if (res.empty()) {
        WriteLog("Signature '{}' not found", name);
        return false;
    }

    int32 comp[4] = {};

    if (auto istr = istringstream(res); !(istr >> comp[0] && istr >> comp[1] && istr >> comp[2] && istr >> comp[3])) {
        WriteLog("Unable to parse signature '{}'", name);
        rect = {};
        return false;
    }

    rect = irect32 {comp[0], comp[1], comp[2] - comp[0], comp[3] - comp[1]};
    return true;
}

auto FOMapper::GetIfaceSpr(hstring fname) -> Sprite*
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = IfaceSpr.find(fname); it != IfaceSpr.end()) {
        return it->second.get();
    }

    auto spr = SprMngr.LoadSprite(fname, AtlasType::IfaceSprites);

    if (spr) {
        spr->PlayDefault();
    }

    return IfaceSpr.emplace(fname, std::move(spr)).first->second.get();
}

void FOMapper::ProcessMapperInput()
{
    FO_STACK_TRACE_ENTRY();

    if (const bool is_fullscreen = SprMngr.IsFullscreen(); (is_fullscreen && Settings.FullscreenMouseScroll) || (!is_fullscreen && Settings.WindowedMouseScroll)) {
        Settings.ScrollMouseRight = Settings.MousePos.x >= Settings.ScreenWidth - 1;
        Settings.ScrollMouseLeft = Settings.MousePos.x <= 0;
        Settings.ScrollMouseDown = Settings.MousePos.y >= Settings.ScreenHeight - 1;
        Settings.ScrollMouseUp = Settings.MousePos.y <= 0;
    }

    if (!SprMngr.IsWindowFocused()) {
        Keyb.Lost();
        OnInputLost.Fire();
        IntHold = INT_NONE;
        return;
    }

    InputEvent ev;
    while (App->Input.PollEvent(ev)) {
        ProcessInputEvent(ev);

        const auto ev_type = ev.Type;

        if (ev_type == InputEvent::EventType::KeyDownEvent || ev_type == InputEvent::EventType::KeyUpEvent) {
            const auto dikdw = ev_type == InputEvent::EventType::KeyDownEvent ? ev.KeyDown.Code : KeyCode::None;
            const auto dikup = ev_type == InputEvent::EventType::KeyUpEvent ? ev.KeyUp.Code : KeyCode::None;

            // Avoid repeating
            if (dikdw != KeyCode::None && PressedKeys[static_cast<int32>(dikdw)]) {
                continue;
            }
            if (dikup != KeyCode::None && !PressedKeys[static_cast<int32>(dikup)]) {
                continue;
            }

            // Keyboard states, to know outside function
            PressedKeys[static_cast<int32>(dikup)] = false;
            PressedKeys[static_cast<int32>(dikdw)] = true;

            // Control keys
            if (dikdw == KeyCode::Rcontrol || dikdw == KeyCode::Lcontrol) {
                Keyb.CtrlDwn = true;
            }
            else if (dikdw == KeyCode::Lmenu || dikdw == KeyCode::Rmenu) {
                Keyb.AltDwn = true;
            }
            else if (dikdw == KeyCode::Lshift || dikdw == KeyCode::Rshift) {
                Keyb.ShiftDwn = true;
            }
            if (dikup == KeyCode::Rcontrol || dikup == KeyCode::Lcontrol) {
                Keyb.CtrlDwn = false;
            }
            else if (dikup == KeyCode::Lmenu || dikup == KeyCode::Rmenu) {
                Keyb.AltDwn = false;
            }
            else if (dikup == KeyCode::Lshift || dikup == KeyCode::Rshift) {
                Keyb.ShiftDwn = false;
            }

            // Hotkeys
            if (!Keyb.AltDwn && !Keyb.CtrlDwn && !Keyb.ShiftDwn) {
                switch (dikdw) {
                case KeyCode::F1:
                    Settings.ShowItem = !Settings.ShowItem;
                    if (_curMap) {
                        _curMap->RebuildMap();
                    }
                    break;
                case KeyCode::F2:
                    Settings.ShowScen = !Settings.ShowScen;
                    if (_curMap) {
                        _curMap->RebuildMap();
                    }
                    break;
                case KeyCode::F3:
                    Settings.ShowWall = !Settings.ShowWall;
                    if (_curMap) {
                        _curMap->RebuildMap();
                    }
                    break;
                case KeyCode::F4:
                    Settings.ShowCrit = !Settings.ShowCrit;
                    if (_curMap) {
                        _curMap->RebuildMap();
                    }
                    break;
                case KeyCode::F5:
                    Settings.ShowTile = !Settings.ShowTile;
                    if (_curMap) {
                        _curMap->RebuildMap();
                    }
                    break;
                case KeyCode::F6:
                    Settings.ShowFast = !Settings.ShowFast;
                    if (_curMap) {
                        _curMap->RebuildMap();
                    }
                    break;
                case KeyCode::F7:
                    IntVisible = !IntVisible;
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
                    ObjVisible = !ObjVisible;
                    break;
                case KeyCode::F10:
                    if (_curMap) {
                        _curMap->SwitchShowHex();
                    }
                    break;

                // Fullscreen
                case KeyCode::F11:
                    SprMngr.ToggleFullscreen();
                    continue;
                // Minimize
                case KeyCode::F12:
                    SprMngr.MinimizeWindow();
                    continue;

                case KeyCode::Delete:
                    SelectDelete();
                    break;
                case KeyCode::Add:
                    if (_curMap && !ConsoleEdit && SelectedEntities.empty()) {
                        int32 day_time = GetGlobalDayTime();
                        day_time += 60;

                        while (day_time > 2880) {
                            day_time -= 1440;
                        }

                        SetGlobalDayTime(day_time);
                    }
                    break;
                case KeyCode::Subtract:
                    if (_curMap && !ConsoleEdit && SelectedEntities.empty()) {
                        int32 day_time = GetGlobalDayTime();
                        day_time -= 60;

                        while (day_time < 0) {
                            day_time += 1440;
                        }

                        SetGlobalDayTime(day_time);
                    }
                    break;
                case KeyCode::Tab:
                    SelectAxialGrid = !SelectAxialGrid;
                    break;
                default:
                    break;
                }
            }

            if (Keyb.ShiftDwn) {
                switch (dikdw) {
                case KeyCode::F7:
                    IntFix = !IntFix;
                    break;
                case KeyCode::F9:
                    ObjFix = !ObjFix;
                    break;
                case KeyCode::F11:
                    SprMngr.GetAtlasMngr().DumpAtlases();
                    break;
                case KeyCode::C0:
                case KeyCode::Numpad0:
                    TileLayer = 0;
                    break;
                case KeyCode::C1:
                case KeyCode::Numpad1:
                    TileLayer = 1;
                    break;
                case KeyCode::C2:
                case KeyCode::Numpad2:
                    TileLayer = 2;
                    break;
                case KeyCode::C3:
                case KeyCode::Numpad3:
                    TileLayer = 3;
                    break;
                case KeyCode::C4:
                case KeyCode::Numpad4:
                    TileLayer = 4;
                    break;
                case KeyCode::Tab:
                    SelectEntireEntity = !SelectEntireEntity;
                    break;
                default:
                    break;
                }
            }

            if (Keyb.CtrlDwn) {
                switch (dikdw) {
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
                        SaveMap(_curMap.get(), "");
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

            // Key down
            if (dikdw != KeyCode::None) {
                if (ObjVisible && !SelectedEntities.empty()) {
                    ObjKeyDown(dikdw, ev.KeyDown.Text);
                }
                else {
                    ConsoleKeyDown(dikdw, ev.KeyDown.Text);
                }

                if (!ConsoleEdit) {
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
            }

            // Key up
            if (dikup != KeyCode::None) {
                ConsoleKeyUp(dikup);

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
        else if (ev_type == InputEvent::EventType::MouseWheelEvent) {
            if (IntVisible && SubTabsActive && IsCurInRect(SubTabsRect, SubTabsX, SubTabsY)) {
                int32 step = 4;

                if (Keyb.ShiftDwn) {
                    step = 8;
                }
                else if (Keyb.CtrlDwn) {
                    step = 20;
                }
                else if (Keyb.AltDwn) {
                    step = 50;
                }

                if (ev.MouseWheel.Delta > 0) {
                    TabsScroll[SubTabsActiveTab] += step;
                }
                else {
                    TabsScroll[SubTabsActiveTab] -= step;
                }
                TabsScroll[SubTabsActiveTab] = std::max(TabsScroll[SubTabsActiveTab], 0);
            }
            else if (IntVisible && IsCurInRect(IntWWork, IntX, IntY) && (IsItemMode() || IsCritMode())) {
                int32 step = 1;

                if (Keyb.ShiftDwn) {
                    step = numeric_cast<int32>(ProtosOnScreen);
                }
                else if (Keyb.CtrlDwn) {
                    step = 100;
                }
                else if (Keyb.AltDwn) {
                    step = 1000;
                }

                if (ev.MouseWheel.Delta > 0) {
                    if (IsItemMode() || IsCritMode()) {
                        (*CurProtoScroll) -= step;
                        *CurProtoScroll = std::max(*CurProtoScroll, 0);
                    }
                    else if (IntMode == INT_MODE_INCONT) {
                        InContScroll -= step;
                        InContScroll = std::max(InContScroll, 0);
                    }
                    else if (IntMode == INT_MODE_LIST) {
                        ListScroll -= step;
                        ListScroll = std::max(ListScroll, 0);
                    }
                }
                else {
                    if (IsItemMode() && !CurItemProtos->empty()) {
                        (*CurProtoScroll) += step;
                        if (*CurProtoScroll >= numeric_cast<int32>(CurItemProtos->size())) {
                            *CurProtoScroll = numeric_cast<int32>(CurItemProtos->size()) - 1;
                        }
                    }
                    else if (IsCritMode() && !CurCritterProtos->empty()) {
                        (*CurProtoScroll) += step;
                        if (*CurProtoScroll >= numeric_cast<int32>(CurCritterProtos->size())) {
                            *CurProtoScroll = numeric_cast<int32>(CurCritterProtos->size()) - 1;
                        }
                    }
                    else if (IntMode == INT_MODE_INCONT) {
                        InContScroll += step;
                    }
                    else if (IntMode == INT_MODE_LIST) {
                        ListScroll += step;
                    }
                }
            }
            else {
                if (ev.MouseWheel.Delta != 0 && _curMap) {
                    const auto cur_zoom = _curMap->GetSpritesZoomTarget();
                    const auto new_zoom = std::clamp(cur_zoom * (ev.MouseWheel.Delta > 0 ? 1.2f : 0.8f), GameSettings::MIN_ZOOM, GameSettings::MAX_ZOOM);
                    ChangeZoom(new_zoom);
                }
            }
        }
        else if (ev_type == InputEvent::EventType::MouseDownEvent) {
            if (ev.MouseDown.Button == MouseButton::Middle) {
                CurMMouseDown();
                ChangeZoom(1.0f);
            }
            if (ev.MouseDown.Button == MouseButton::Left) {
                IntLMouseDown();
            }
        }
        else if (ev_type == InputEvent::EventType::MouseUpEvent) {
            if (ev.MouseUp.Button == MouseButton::Left) {
                IntLMouseUp();
            }
            if (ev.MouseUp.Button == MouseButton::Right) {
                CurRMouseUp();
            }
        }
        else if (ev.Type == InputEvent::EventType::MouseMoveEvent) {
            IntMouseMove();
        }
    }
}

void FOMapper::ChangeZoom(float32 new_zoom)
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

void FOMapper::MapperMainLoop()
{
    FO_STACK_TRACE_ENTRY();

    FrameAdvance();

    OnLoop.Fire();
    ConsoleProcess();
    ProcessMapperInput();

    if (_curMap) {
        _curMap->Process();
    }

    {
        SprMngr.BeginScene();

        DrawIfaceLayer(0);

        if (_curMap) {
            _curMap->DrawMap();
        }

        // Iface
        DrawIfaceLayer(1);
        IntDraw();
        DrawIfaceLayer(2);
        ConsoleDraw();
        DrawIfaceLayer(3);
        ObjDraw();
        DrawIfaceLayer(4);
        CurDraw();
        DrawIfaceLayer(5);

        SprMngr.EndScene();
    }
}

void FOMapper::IntDraw()
{
    FO_STACK_TRACE_ENTRY();

    if (!IntVisible) {
        return;
    }

    SprMngr.DrawSprite(IntMainPic.get(), {IntX, IntY}, COLOR_SPRITE);

    switch (IntMode) {
    case INT_MODE_CUSTOM0:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[0].x + IntX, IntBCust[0].y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM1:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[1].x + IntX, IntBCust[1].y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM2:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[2].x + IntX, IntBCust[2].y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM3:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[3].x + IntX, IntBCust[3].y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM4:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[4].x + IntX, IntBCust[4].y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM5:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[5].x + IntX, IntBCust[5].y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM6:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[6].x + IntX, IntBCust[6].y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM7:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[7].x + IntX, IntBCust[7].y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM8:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[8].x + IntX, IntBCust[8].y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM9:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[9].x + IntX, IntBCust[9].y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_ITEM:
        SprMngr.DrawSprite(IntPTab.get(), {IntBItem.x + IntX, IntBItem.y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_TILE:
        SprMngr.DrawSprite(IntPTab.get(), {IntBTile.x + IntX, IntBTile.y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CRIT:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCrit.x + IntX, IntBCrit.y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_FAST:
        SprMngr.DrawSprite(IntPTab.get(), {IntBFast.x + IntX, IntBFast.y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_IGNORE:
        SprMngr.DrawSprite(IntPTab.get(), {IntBIgnore.x + IntX, IntBIgnore.y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_INCONT:
        SprMngr.DrawSprite(IntPTab.get(), {IntBInCont.x + IntX, IntBInCont.y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_MESS:
        SprMngr.DrawSprite(IntPTab.get(), {IntBMess.x + IntX, IntBMess.y + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_LIST:
        SprMngr.DrawSprite(IntPTab.get(), {IntBList.x + IntX, IntBList.y + IntY}, COLOR_SPRITE);
        break;
    default:
        break;
    }

    for (auto i = INT_MODE_CUSTOM0; i <= INT_MODE_CUSTOM9; i++) {
        DrawStr(irect32(IntBCust[i].x + IntX, IntBCust[i].y + IntY, IntBCust[i].width, IntBCust[i].height), TabsName[INT_MODE_CUSTOM0 + i], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    }
    DrawStr(irect32(IntBItem.x + IntX, IntBItem.y + IntY, IntBItem.width, IntBItem.height), TabsName[INT_MODE_ITEM], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(irect32(IntBTile.x + IntX, IntBTile.y + IntY, IntBTile.width, IntBTile.height), TabsName[INT_MODE_TILE], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(irect32(IntBCrit.x + IntX, IntBCrit.y + IntY, IntBCrit.width, IntBCrit.height), TabsName[INT_MODE_CRIT], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(irect32(IntBFast.x + IntX, IntBFast.y + IntY, IntBFast.width, IntBFast.height), TabsName[INT_MODE_FAST], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(irect32(IntBIgnore.x + IntX, IntBIgnore.y + IntY, IntBIgnore.width, IntBIgnore.height), TabsName[INT_MODE_IGNORE], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(irect32(IntBInCont.x + IntX, IntBInCont.y + IntY, IntBInCont.width, IntBInCont.height), TabsName[INT_MODE_INCONT], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(irect32(IntBMess.x + IntX, IntBMess.y + IntY, IntBMess.width, IntBMess.height), TabsName[INT_MODE_MESS], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(irect32(IntBList.x + IntX, IntBList.y + IntY, IntBList.width, IntBList.height), TabsName[INT_MODE_LIST], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);

    if (Settings.ShowItem) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowItem.x + IntX, IntBShowItem.y + IntY}, COLOR_SPRITE);
    }
    if (Settings.ShowScen) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowScen.x + IntX, IntBShowScen.y + IntY}, COLOR_SPRITE);
    }
    if (Settings.ShowWall) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowWall.x + IntX, IntBShowWall.y + IntY}, COLOR_SPRITE);
    }
    if (Settings.ShowCrit) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowCrit.x + IntX, IntBShowCrit.y + IntY}, COLOR_SPRITE);
    }
    if (Settings.ShowTile) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowTile.x + IntX, IntBShowTile.y + IntY}, COLOR_SPRITE);
    }
    if (Settings.ShowRoof) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowRoof.x + IntX, IntBShowRoof.y + IntY}, COLOR_SPRITE);
    }
    if (Settings.ShowFast) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowFast.x + IntX, IntBShowFast.y + IntY}, COLOR_SPRITE);
    }

    if (IsSelectItem) {
        SprMngr.DrawSprite(IntPSelect.get(), {IntBSelectItem.x + IntX, IntBSelectItem.y + IntY}, COLOR_SPRITE);
    }
    if (IsSelectScen) {
        SprMngr.DrawSprite(IntPSelect.get(), {IntBSelectScen.x + IntX, IntBSelectScen.y + IntY}, COLOR_SPRITE);
    }
    if (IsSelectWall) {
        SprMngr.DrawSprite(IntPSelect.get(), {IntBSelectWall.x + IntX, IntBSelectWall.y + IntY}, COLOR_SPRITE);
    }
    if (IsSelectCrit) {
        SprMngr.DrawSprite(IntPSelect.get(), {IntBSelectCrit.x + IntX, IntBSelectCrit.y + IntY}, COLOR_SPRITE);
    }
    if (IsSelectTile) {
        SprMngr.DrawSprite(IntPSelect.get(), {IntBSelectTile.x + IntX, IntBSelectTile.y + IntY}, COLOR_SPRITE);
    }
    if (IsSelectRoof) {
        SprMngr.DrawSprite(IntPSelect.get(), {IntBSelectRoof.x + IntX, IntBSelectRoof.y + IntY}, COLOR_SPRITE);
    }

    auto x = IntWWork.x + IntX;
    auto y = IntWWork.y + IntY;
    auto h = IntWWork.height;
    int32 w = ProtoWidth;

    if (IsItemMode()) {
        auto i = *CurProtoScroll;
        auto j = numeric_cast<int32>(numeric_cast<size_t>(i) + ProtosOnScreen);
        j = std::min(j, numeric_cast<int32>(CurItemProtos->size()));

        for (; i < j; i++, x += w) {
            const auto& proto = (*CurItemProtos)[i];
            auto col = i == numeric_cast<int32>(GetTabIndex()) ? COLOR_SPRITE_RED : COLOR_SPRITE;

            if (const auto* spr = GetIfaceSpr(proto->GetPicMap()); spr != nullptr) {
                SprMngr.DrawSpriteSize(spr, {x, y}, {w, h / 2}, false, true, col);
            }

            if (proto->GetPicInv()) {
                const auto* spr = GetIfaceSpr(proto->GetPicInv());

                if (spr != nullptr) {
                    SprMngr.DrawSpriteSize(spr, {x, y + h / 2}, {w, h / 2}, false, true, col);
                }
            }

            DrawStr(irect32(x, y + h - 15, w, h), proto->GetName(), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
        }

        if (GetTabIndex() < numeric_cast<int32>(CurItemProtos->size())) {
            const auto& proto = (*CurItemProtos)[GetTabIndex()];

            SprMngr.DrawText(irect32(IntWHint.x + IntX, IntWHint.y + IntY, IntWHint.width, IntWHint.height), proto->GetName(), 0, COLOR_TEXT, FONT_DEFAULT);
        }
    }
    else if (IsCritMode()) {
        auto i = *CurProtoScroll;
        auto j = i + ProtosOnScreen;
        j = std::min(j, numeric_cast<int32>(CurCritterProtos->size()));

        for (; i < j; i++, x += w) {
            const auto& proto = (*CurCritterProtos)[i];

            auto model_name = proto->GetModelName();
            const auto* anim = ResMngr.GetCritterPreviewSpr(model_name, CritterStateAnim::Unarmed, CritterActionAnim::Idle, CritterDir, nullptr); // &proto->Params[ ST_ANIM3D_LAYER_BEGIN ] );

            if (anim == nullptr) {
                continue;
            }

            auto col = COLOR_SPRITE;

            if (i == GetTabIndex()) {
                col = COLOR_SPRITE_RED;
            }

            SprMngr.DrawSpriteSize(anim, {x, y}, {w, h / 2}, false, true, col);
            DrawStr(irect32(x, y + h - 15, w, h), proto->GetName(), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
        }

        if (GetTabIndex() < numeric_cast<int32>(CurCritterProtos->size())) {
            const auto& proto = (*CurCritterProtos)[GetTabIndex()];
            DrawStr(irect32(IntWHint.x + IntX, IntWHint.y + IntY, IntWHint.width, IntWHint.height), proto->GetName(), 0, COLOR_TEXT, FONT_DEFAULT);
        }
    }
    else if (IntMode == INT_MODE_INCONT && !SelectedEntities.empty()) {
        auto& entity = SelectedEntities.front();
        auto inner_items = GetEntityInnerItems(entity.get());

        auto i = InContScroll;
        auto j = i + ProtosOnScreen;

        j = std::min(j, numeric_cast<int32>(inner_items.size()));

        for (; i < j; i++, x += w) {
            auto& inner_item = inner_items[i];
            FO_RUNTIME_ASSERT(inner_item);

            auto* spr = GetIfaceSpr(inner_item->GetPicInv());

            if (spr == nullptr) {
                continue;
            }

            auto col = COLOR_SPRITE;

            if (inner_item == InContItem) {
                col = COLOR_SPRITE_RED;
            }

            SprMngr.DrawSpriteSize(spr, {x, y}, {w, h}, false, true, col);

            SprMngr.DrawText(irect32(x, y + h - 15, w, h), strex("x{}", inner_item->GetCount()), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);

            if (inner_item->GetOwnership() == ItemOwnership::CritterInventory && inner_item->GetCritterSlot() != CritterItemSlot::Inventory) {
                SprMngr.DrawText(irect32(x, y, w, h), strex("Slot {}", inner_item->GetCritterSlot()), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
            }
        }
    }
    else if (IntMode == INT_MODE_LIST) {
        auto i = ListScroll;
        auto j = numeric_cast<int32>(LoadedMaps.size());

        for (; i < j; i++, x += w) {
            auto* map = LoadedMaps[i].get();
            SprMngr.DrawText(irect32(x, y, w, h), strex(" '{}'", map->GetName()), 0, _curMap == map ? COLOR_SPRITE_RED : COLOR_TEXT, FONT_DEFAULT);
        }
    }

    // Message box
    if (IntMode == INT_MODE_MESS) {
        MessBoxDraw();
    }

    // Sub tabs
    if (SubTabsActive) {
        SprMngr.DrawSprite(SubTabsPic.get(), {SubTabsX, SubTabsY}, COLOR_SPRITE);

        auto line_height = SprMngr.GetLineHeight(FONT_DEFAULT) + 1;
        auto posy = SubTabsRect.height - line_height - 2;
        auto i = 0;
        auto& stabs = Tabs[SubTabsActiveTab];

        for (auto&& [fst, snd] : stabs) {
            i++;

            if (i - 1 < TabsScroll[SubTabsActiveTab]) {
                continue;
            }

            auto name = fst;
            auto& stab = snd;

            auto color = TabsActive[SubTabsActiveTab] == &stab ? COLOR_TEXT_WHITE : COLOR_TEXT;
            auto r = irect32(SubTabsRect.x + SubTabsX + 5, SubTabsRect.y + SubTabsY + posy, Settings.ScreenWidth, line_height - 1);

            if (IsCurInRect(r)) {
                color = COLOR_TEXT_DWHITE;
            }

            auto count = numeric_cast<int32>(stab.CritterProtos.size());

            if (count == 0) {
                count = numeric_cast<int32>(stab.ItemProtos.size());
            }

            name += strex(" ({})", count);
            DrawStr(r, name, 0, color, FONT_DEFAULT);

            posy -= line_height;

            if (posy < 0) {
                break;
            }
        }
    }

    // Map info
    if (_curMap) {
        mpos hex;
        _curMap->GetHexAtScreen(Settings.MousePos, hex, nullptr);
        const int32 day_time = GetGlobalDayTime();

        DrawStr(irect32(Settings.ScreenWidth - 150, 20, Settings.ScreenWidth, Settings.ScreenHeight),
            strex("Map: '{}'\n"
                  "Mouse hex: {}\n"
                  "Game time: {} : {}\n"
                  "Fps: {}\n"
                  "Tile layer: {}\n"
                  "Scroll check: {}\n"
                  "Grid select: {}\n"
                  "Entity select: {}",
                _curMap->GetName(), hex, day_time / 60 % 24, day_time % 60, GameTime.GetFramesPerSecond(), TileLayer, //
                Settings.ScrollCheck ? "Yes" : "No", SelectAxialGrid ? "Axial" : "Native", SelectEntireEntity ? "All hexes" : "Single hex"),
            FT_NOBREAK_LINE | FT_BORDERED, COLOR_TEXT, FONT_DEFAULT);
    }
}

void FOMapper::ObjDraw()
{
    FO_STACK_TRACE_ENTRY();

    if (!ObjVisible) {
        return;
    }

    auto* entity = GetInspectorEntity();

    if (entity == nullptr) {
        return;
    }

    const auto* item = dynamic_cast<ItemView*>(entity);
    const auto* cr = dynamic_cast<CritterView*>(entity);
    auto r = irect32(ObjWWork.x + ObjX, ObjWWork.y + ObjY, ObjWWork.width, ObjWWork.height);
    const auto x = r.x;
    const auto y = r.y;
    const auto w = r.width;

    SprMngr.DrawSprite(ObjWMainPic.get(), {ObjX, ObjY}, COLOR_SPRITE);

    if (ObjToAll) {
        SprMngr.DrawSprite(ObjPbToAllDn.get(), {ObjBToAll.x + ObjX, ObjBToAll.y + ObjY}, COLOR_SPRITE);
    }

    if (item != nullptr) {
        const auto* spr = GetIfaceSpr(item->GetPicMap());

        if (spr == nullptr) {
            spr = ResMngr.GetItemDefaultSpr().get();
        }

        SprMngr.DrawSpriteSize(spr, {x + w - ProtoWidth, y}, {ProtoWidth, ProtoWidth}, false, true, COLOR_SPRITE);

        if (item->GetPicInv()) {
            const auto* inv_spr = GetIfaceSpr(item->GetPicInv());

            if (inv_spr != nullptr) {
                SprMngr.DrawSpriteSize(inv_spr, {x + w - ProtoWidth, y + ProtoWidth}, {ProtoWidth, ProtoWidth}, false, true, COLOR_SPRITE);
            }
        }
    }

    DrawLine("Id", "", strex("{}", entity->GetId()), true, r);
    DrawLine("ProtoId", "", dynamic_cast<EntityWithProto*>(entity)->GetProtoId(), true, r);

    if (cr != nullptr) {
        DrawLine("Type", "", "Critter", true, r);
    }
    else if (item != nullptr && !item->GetStatic()) {
        DrawLine("Type", "", "Dynamic Item", true, r);
    }
    else if (item != nullptr && item->GetStatic()) {
        DrawLine("Type", "", "Static Item", true, r);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    for (const auto& prop : ShowProps) {
        if (prop) {
            auto value = entity->GetProperties().SavePropertyToText(prop.get());
            DrawLine(prop->GetName(), prop->GetViewTypeName(), value, prop->IsCoreProperty() && !prop->IsMutable(), r);
        }
        else {
            r.y += DRAW_NEXT_HEIGHT;
        }
    }
}

void FOMapper::DrawLine(string_view name, string_view type_name, string_view text, bool is_const, irect32& r)
{
    FO_STACK_TRACE_ENTRY();

    const auto x = r.x;
    const auto y = r.y;
    const auto w = r.width;
    const auto h = r.height;

    ucolor color = COLOR_TEXT;

    if (is_const) {
        color = COLOR_TEXT_DWHITE;
    }

    auto result_text = text;

    if (ObjCurLine == (y - ObjWWork.y - ObjY) / DRAW_NEXT_HEIGHT) {
        color = COLOR_TEXT_WHITE;

        if (!is_const && ObjCurLineValue != ObjCurLineInitValue) {
            color = COLOR_TEXT_RED;
            result_text = ObjCurLineValue;
        }
    }

    string str = strex("{}{}{}{}", name, !type_name.empty() ? " (" : "", !type_name.empty() ? type_name : "", !type_name.empty() ? ")" : "");
    str += "........................................................................................................";
    DrawStr(irect32(x, y, w / 2, h), str, FT_NOBREAK, color, FONT_DEFAULT);
    DrawStr(irect32(x + w / 2, y, w / 2, h), result_text, FT_NOBREAK, color, FONT_DEFAULT);

    r.y += DRAW_NEXT_HEIGHT;
}

void FOMapper::ObjKeyDown(KeyCode dik, string_view dik_text)
{
    FO_STACK_TRACE_ENTRY();

    if (dik == KeyCode::Return || dik == KeyCode::Numpadenter) {
        if (ObjCurLineInitValue != ObjCurLineValue) {
            auto* entity = GetInspectorEntity();
            FO_RUNTIME_ASSERT(entity);
            ObjKeyDownApply(entity);

            if (ObjToAll && !SelectedEntities.empty() && SelectedEntities.front() == entity) {
                for (size_t i = 1; i < SelectedEntities.size(); i++) {
                    const auto is_same1 = dynamic_cast<CritterView*>(SelectedEntities[i].get()) != nullptr && dynamic_cast<CritterView*>(entity) != nullptr;
                    const auto is_same2 = dynamic_cast<ItemView*>(SelectedEntities[i].get()) != nullptr && dynamic_cast<ItemView*>(entity) != nullptr;

                    if (is_same1 || is_same2) {
                        ObjKeyDownApply(SelectedEntities[i].get());
                    }
                }
            }

            SelectEntityProp(ObjCurLine);
        }
    }
    else if (dik == KeyCode::Prior) {
        SelectEntityProp(ObjCurLine - 1);
    }
    else if (dik == KeyCode::Next) {
        SelectEntityProp(ObjCurLine + 1);
    }
    else if (dik == KeyCode::Escape) {
        if (ObjCurLineValue != ObjCurLineInitValue) {
            ObjCurLineValue = ObjCurLineInitValue;
        }
        else {
            SelectClear();
        }
    }
    else {
        if (!ObjCurLineIsConst) {
            Keyb.FillChar(dik, dik_text, ObjCurLineValue, nullptr, KIF_NO_SPEC_SYMBOLS);
        }
    }
}

void FOMapper::ObjKeyDownApply(Entity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    constexpr auto start_line = 3;

    if (ObjCurLine >= start_line && ObjCurLine - start_line < numeric_cast<int32>(ShowProps.size())) {
        const auto& prop = ShowProps[ObjCurLine - start_line];

        if (prop) {
            try {
                entity->GetPropertiesForEdit().ApplyPropertyFromText(prop.get(), ObjCurLineValue);

                // Modify main mesh
                if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
                    if (item->GetMultihexGeneration() == MultihexGenerationType::SameSibling) {
                        for (int32 i = 0; i < GameSettings::MAP_DIR_COUNT; i++) {
                            if (mpos hex = item->GetHex(); GeometryHelper::MoveHexByDir(hex, static_cast<uint8>(i), _curMap->GetSize())) {
                                auto main_mesh_items = vec_filter(_curMap->GetItemsOnHex(hex), [&](auto&& item2) -> bool {
                                    if (SelectedEntitiesSet.contains(item2.get())) {
                                        return false;
                                    }
                                    return item->GetProtoId() == item2->GetProtoId();
                                });
                                for (auto&& item2 : main_mesh_items) {
                                    item2->GetPropertiesForEdit().ApplyPropertyFromText(prop.get(), ObjCurLineValue);
                                }
                            }
                        }

                        // Todo: walk around multihex lines?
                    }
                }
            }
            catch (const std::exception& ex) {
                ignore_unused(ex);
                entity->GetPropertiesForEdit().ApplyPropertyFromText(prop.get(), ObjCurLineInitValue);
            }

            if (auto* hex_item = dynamic_cast<ItemHexView*>(entity); hex_item != nullptr) {
                if (prop == hex_item->GetPropertyOffset()) {
                    hex_item->RefreshOffs();
                }
                if (prop == hex_item->GetPropertyPicMap()) {
                    hex_item->RefreshAnim();
                }
            }
        }
    }
}

void FOMapper::SelectEntityProp(int32 line)
{
    FO_STACK_TRACE_ENTRY();

    constexpr auto start_line = 3;

    ObjCurLine = line;
    ObjCurLine = std::max(ObjCurLine, 0);
    ObjCurLineInitValue = ObjCurLineValue = "";
    ObjCurLineIsConst = true;

    if (const auto* entity = GetInspectorEntity(); entity != nullptr) {
        if (ObjCurLine - start_line >= numeric_cast<int32>(ShowProps.size())) {
            ObjCurLine = numeric_cast<int32>(ShowProps.size()) + start_line - 1;
        }
        if (ObjCurLine >= start_line && ObjCurLine - start_line < numeric_cast<int32>(ShowProps.size()) && (ShowProps[ObjCurLine - start_line] != nullptr)) {
            ObjCurLineInitValue = ObjCurLineValue = entity->GetProperties().SavePropertyToText(ShowProps[ObjCurLine - start_line].get());
            ObjCurLineIsConst = ShowProps[ObjCurLine - start_line]->IsCoreProperty() && !ShowProps[ObjCurLine - start_line]->IsMutable();
        }
    }
}

auto FOMapper::GetInspectorEntity() -> ClientEntity*
{
    FO_STACK_TRACE_ENTRY();

    auto* entity = IntMode == INT_MODE_INCONT && InContItem ? InContItem.get() : (!SelectedEntities.empty() ? SelectedEntities.front().get() : nullptr);

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

    SelectEntityProp(ObjCurLine);
    return entity;
}

void FOMapper::IntLMouseDown()
{
    FO_STACK_TRACE_ENTRY();

    IntHold = INT_NONE;

    // Sub tabs
    if (IntVisible && SubTabsActive) {
        if (IsCurInRect(SubTabsRect, SubTabsX, SubTabsY)) {
            const auto line_height = SprMngr.GetLineHeight(FONT_DEFAULT) + 1;
            auto posy = SubTabsRect.height - line_height - 2;
            auto i = 0;
            auto& stabs = Tabs[SubTabsActiveTab];

            for (auto& snd : stabs | std::views::values) {
                i++;
                if (i - 1 < TabsScroll[SubTabsActiveTab]) {
                    continue;
                }

                auto& stab = snd;

                auto r = irect32(SubTabsRect.x + SubTabsX + 5, SubTabsRect.y + SubTabsY + posy, SubTabsRect.width, line_height - 1);

                if (IsCurInRect(r)) {
                    TabsActive[SubTabsActiveTab] = &stab;
                    RefreshCurProtos();
                    break;
                }

                posy -= line_height;

                if (posy < 0) {
                    break;
                }
            }

            return;
        }

        if (!IsCurInRect(IntWMain, IntX, IntY)) {
            SubTabsActive = false;
            return;
        }
    }

    // Map
    if ((!IntVisible || !IsCurInRect(IntWMain, IntX, IntY)) && (!ObjVisible || SelectedEntities.empty() || !IsCurInRect(ObjWMain, ObjX, ObjY))) {
        InContItem = nullptr;

        if (!_curMap) {
            return;
        }

        if (!_curMap->GetHexAtScreen(Settings.MousePos, SelectHex1, nullptr)) {
            return;
        }

        SelectHex2 = SelectHex1;
        SelectPos = Settings.MousePos;

        if (CurMode == CUR_MODE_DEFAULT) {
            if (Keyb.ShiftDwn) {
                for (auto& entity : SelectedEntities) {
                    if (auto* cr = dynamic_cast<CritterHexView*>(entity.get()); cr != nullptr) {
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
            else if (!Keyb.CtrlDwn) {
                SelectClear();
            }

            IntHold = INT_SELECT;
        }
        else if (CurMode == CUR_MODE_MOVE_SELECTION) {
            IntHold = INT_SELECT;
        }
        else if (CurMode == CUR_MODE_PLACE_OBJECT) {
            if (IsItemMode() && !CurItemProtos->empty()) {
                CreateItem((*CurItemProtos)[GetTabIndex()]->GetProtoId(), SelectHex1, nullptr);
            }
            else if (IsCritMode() && !CurCritterProtos->empty()) {
                CreateCritter((*CurCritterProtos)[GetTabIndex()]->GetProtoId(), SelectHex1);
            }
        }

        return;
    }

    // Object editor
    if (ObjVisible && !SelectedEntities.empty() && IsCurInRect(ObjWMain, ObjX, ObjY)) {
        if (IsCurInRect(ObjWWork, ObjX, ObjY)) {
            SelectEntityProp((Settings.MousePos.y - ObjY - ObjWWork.y) / DRAW_NEXT_HEIGHT);
        }

        if (IsCurInRect(ObjBToAll, ObjX, ObjY)) {
            ObjToAll = !ObjToAll;
            IntHold = INT_BUTTON;
            return;
        }
        if (!ObjFix) {
            IntHold = INT_OBJECT;
            ItemVectX = Settings.MousePos.x - ObjX;
            ItemVectY = Settings.MousePos.y - ObjY;
        }

        return;
    }

    // Interface
    if (!IntVisible || !IsCurInRect(IntWMain, IntX, IntY)) {
        return;
    }

    if (IsCurInRect(IntWWork, IntX, IntY)) {
        int32 ind = (Settings.MousePos.x - IntX - IntWWork.x) / ProtoWidth;

        if (IsItemMode() && !CurItemProtos->empty()) {
            ind += *CurProtoScroll;
            if (ind >= numeric_cast<int32>(CurItemProtos->size())) {
                ind = numeric_cast<int32>(CurItemProtos->size()) - 1;
            }
            SetTabIndex(ind);

            // Switch ignore pid to draw
            if (Keyb.CtrlDwn) {
                const auto pid = (*CurItemProtos)[ind]->GetProtoId();

                auto& stab = Tabs[INT_MODE_IGNORE][DEFAULT_SUB_TAB];
                auto founded = false;
                for (auto it = stab.ItemProtos.begin(); it != stab.ItemProtos.end(); ++it) {
                    if ((*it)->GetProtoId() == pid) {
                        founded = true;
                        stab.ItemProtos.erase(it);
                        break;
                    }
                }
                if (!founded) {
                    stab.ItemProtos.emplace_back((*CurItemProtos)[ind].get());
                }

                _curMap->SwitchIgnorePid(pid);
                _curMap->RebuildMap();
            }
            // Add to container
            else if (Keyb.AltDwn && !SelectedEntities.empty()) {
                auto add = true;
                const auto& proto_item = (*CurItemProtos)[ind];

                if (proto_item->GetStackable()) {
                    for (const auto& child : GetEntityInnerItems(SelectedEntities.front().get())) {
                        if (proto_item->GetProtoId() == child->GetProtoId()) {
                            add = false;
                            break;
                        }
                    }
                }

                if (add) {
                    CreateItem(proto_item->GetProtoId(), {}, SelectedEntities.front().get());
                }
            }
        }
        else if (IsCritMode() && !CurCritterProtos->empty()) {
            ind += *CurProtoScroll;

            if (ind >= numeric_cast<int32>(CurCritterProtos->size())) {
                ind = numeric_cast<int32>(CurCritterProtos->size()) - 1;
            }

            SetTabIndex(ind);
        }
        else if (IntMode == INT_MODE_INCONT) {
            InContItem = nullptr;
            ind += InContScroll;

            vector<refcount_ptr<ItemView>> inner_items;

            if (!SelectedEntities.empty()) {
                inner_items = GetEntityInnerItems(SelectedEntities.front().get());
            }

            if (!inner_items.empty()) {
                if (ind < numeric_cast<int32>(inner_items.size())) {
                    InContItem = inner_items[ind];
                }

                if (Keyb.AltDwn && InContItem != nullptr) {
                    // Delete child
                    if (InContItem->GetOwnership() == ItemOwnership::CritterInventory) {
                        auto* owner = _curMap->GetCritter(InContItem->GetCritterId());
                        FO_RUNTIME_ASSERT(owner);
                        owner->DeleteInvItem(InContItem.get());
                    }
                    else if (InContItem->GetOwnership() == ItemOwnership::ItemContainer) {
                        ItemView* owner = _curMap->GetItem(InContItem->GetContainerId());
                        FO_RUNTIME_ASSERT(owner);
                        owner->DestroyInnerItem(InContItem.get());
                    }
                    else {
                        FO_UNREACHABLE_PLACE();
                    }

                    InContItem = nullptr;

                    // Reselect
                    auto* next_entity = SelectedEntities.front().get();
                    SelectClear();
                    SelectAdd(next_entity);
                }
                else if (Keyb.ShiftDwn && InContItem != nullptr) {
                    // Change child slot
                    if (auto* cr = dynamic_cast<CritterHexView*>(SelectedEntities.front().get()); cr != nullptr) {
                        auto to_slot = static_cast<size_t>(InContItem->GetCritterSlot()) + 1;

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
        else if (IntMode == INT_MODE_LIST) {
            ind += ListScroll;

            if (ind < numeric_cast<int32>(LoadedMaps.size()) && _curMap != LoadedMaps[ind].get()) {
                ShowMap(LoadedMaps[ind].get());
            }
        }
    }
    else if (IsCurInRect(IntBCust[0], IntX, IntY)) {
        IntSetMode(INT_MODE_CUSTOM0);
    }
    else if (IsCurInRect(IntBCust[1], IntX, IntY)) {
        IntSetMode(INT_MODE_CUSTOM1);
    }
    else if (IsCurInRect(IntBCust[2], IntX, IntY)) {
        IntSetMode(INT_MODE_CUSTOM2);
    }
    else if (IsCurInRect(IntBCust[3], IntX, IntY)) {
        IntSetMode(INT_MODE_CUSTOM3);
    }
    else if (IsCurInRect(IntBCust[4], IntX, IntY)) {
        IntSetMode(INT_MODE_CUSTOM4);
    }
    else if (IsCurInRect(IntBCust[5], IntX, IntY)) {
        IntSetMode(INT_MODE_CUSTOM5);
    }
    else if (IsCurInRect(IntBCust[6], IntX, IntY)) {
        IntSetMode(INT_MODE_CUSTOM6);
    }
    else if (IsCurInRect(IntBCust[7], IntX, IntY)) {
        IntSetMode(INT_MODE_CUSTOM7);
    }
    else if (IsCurInRect(IntBCust[8], IntX, IntY)) {
        IntSetMode(INT_MODE_CUSTOM8);
    }
    else if (IsCurInRect(IntBCust[9], IntX, IntY)) {
        IntSetMode(INT_MODE_CUSTOM9);
    }
    else if (IsCurInRect(IntBItem, IntX, IntY)) {
        IntSetMode(INT_MODE_ITEM);
    }
    else if (IsCurInRect(IntBTile, IntX, IntY)) {
        IntSetMode(INT_MODE_TILE);
    }
    else if (IsCurInRect(IntBCrit, IntX, IntY)) {
        IntSetMode(INT_MODE_CRIT);
    }
    else if (IsCurInRect(IntBFast, IntX, IntY)) {
        IntSetMode(INT_MODE_FAST);
    }
    else if (IsCurInRect(IntBIgnore, IntX, IntY)) {
        IntSetMode(INT_MODE_IGNORE);
    }
    else if (IsCurInRect(IntBInCont, IntX, IntY)) {
        IntSetMode(INT_MODE_INCONT);
    }
    else if (IsCurInRect(IntBMess, IntX, IntY)) {
        IntSetMode(INT_MODE_MESS);
    }
    else if (IsCurInRect(IntBList, IntX, IntY)) {
        IntSetMode(INT_MODE_LIST);
    }
    else if (IsCurInRect(IntBScrBack, IntX, IntY)) {
        if (IsItemMode() || IsCritMode()) {
            (*CurProtoScroll)--;
            *CurProtoScroll = std::max(*CurProtoScroll, 0);
        }
        else if (IntMode == INT_MODE_INCONT) {
            InContScroll--;
            InContScroll = std::max(InContScroll, 0);
        }
        else if (IntMode == INT_MODE_LIST) {
            ListScroll--;
            ListScroll = std::max(ListScroll, 0);
        }
    }
    else if (IsCurInRect(IntBScrBackFst, IntX, IntY)) {
        if (IsItemMode() || IsCritMode()) {
            (*CurProtoScroll) -= numeric_cast<int32>(ProtosOnScreen);
            *CurProtoScroll = std::max(*CurProtoScroll, 0);
        }
        else if (IntMode == INT_MODE_INCONT) {
            InContScroll -= numeric_cast<int32>(ProtosOnScreen);
            InContScroll = std::max(InContScroll, 0);
        }
        else if (IntMode == INT_MODE_LIST) {
            ListScroll -= numeric_cast<int32>(ProtosOnScreen);
            ListScroll = std::max(ListScroll, 0);
        }
    }
    else if (IsCurInRect(IntBScrFront, IntX, IntY)) {
        if (IsItemMode() && !CurItemProtos->empty()) {
            (*CurProtoScroll)++;

            if (*CurProtoScroll >= numeric_cast<int32>(CurItemProtos->size())) {
                *CurProtoScroll = numeric_cast<int32>(CurItemProtos->size()) - 1;
            }
        }
        else if (IsCritMode() && !CurCritterProtos->empty()) {
            (*CurProtoScroll)++;

            if (*CurProtoScroll >= numeric_cast<int32>(CurCritterProtos->size())) {
                *CurProtoScroll = numeric_cast<int32>(CurCritterProtos->size()) - 1;
            }
        }
        else if (IntMode == INT_MODE_INCONT) {
            InContScroll++;
        }
        else if (IntMode == INT_MODE_LIST) {
            ListScroll++;
        }
    }
    else if (IsCurInRect(IntBScrFrontFst, IntX, IntY)) {
        if (IsItemMode() && !CurItemProtos->empty()) {
            (*CurProtoScroll) += numeric_cast<int32>(ProtosOnScreen);

            if (*CurProtoScroll >= numeric_cast<int32>(CurItemProtos->size())) {
                *CurProtoScroll = numeric_cast<int32>(CurItemProtos->size()) - 1;
            }
        }
        else if (IsCritMode() && !CurCritterProtos->empty()) {
            (*CurProtoScroll) += numeric_cast<int32>(ProtosOnScreen);

            if (*CurProtoScroll >= numeric_cast<int32>(CurCritterProtos->size())) {
                *CurProtoScroll = numeric_cast<int32>(CurCritterProtos->size()) - 1;
            }
        }
        else if (IntMode == INT_MODE_INCONT) {
            InContScroll += numeric_cast<int32>(ProtosOnScreen);
        }
        else if (IntMode == INT_MODE_LIST) {
            ListScroll += numeric_cast<int32>(ProtosOnScreen);
        }
    }
    else if (IsCurInRect(IntBShowItem, IntX, IntY)) {
        Settings.ShowItem = !Settings.ShowItem;
        _curMap->RebuildMap();
    }
    else if (IsCurInRect(IntBShowScen, IntX, IntY)) {
        Settings.ShowScen = !Settings.ShowScen;
        _curMap->RebuildMap();
    }
    else if (IsCurInRect(IntBShowWall, IntX, IntY)) {
        Settings.ShowWall = !Settings.ShowWall;
        _curMap->RebuildMap();
    }
    else if (IsCurInRect(IntBShowCrit, IntX, IntY)) {
        Settings.ShowCrit = !Settings.ShowCrit;
        _curMap->RebuildMap();
    }
    else if (IsCurInRect(IntBShowTile, IntX, IntY)) {
        Settings.ShowTile = !Settings.ShowTile;
        _curMap->RebuildMap();
    }
    else if (IsCurInRect(IntBShowRoof, IntX, IntY)) {
        Settings.ShowRoof = !Settings.ShowRoof;
        _curMap->RebuildMap();
    }
    else if (IsCurInRect(IntBShowFast, IntX, IntY)) {
        Settings.ShowFast = !Settings.ShowFast;
        _curMap->RebuildMap();
    }
    else if (IsCurInRect(IntBSelectItem, IntX, IntY)) {
        IsSelectItem = !IsSelectItem;
    }
    else if (IsCurInRect(IntBSelectScen, IntX, IntY)) {
        IsSelectScen = !IsSelectScen;
    }
    else if (IsCurInRect(IntBSelectWall, IntX, IntY)) {
        IsSelectWall = !IsSelectWall;
    }
    else if (IsCurInRect(IntBSelectCrit, IntX, IntY)) {
        IsSelectCrit = !IsSelectCrit;
    }
    else if (IsCurInRect(IntBSelectTile, IntX, IntY)) {
        IsSelectTile = !IsSelectTile;
    }
    else if (IsCurInRect(IntBSelectRoof, IntX, IntY)) {
        IsSelectRoof = !IsSelectRoof;
    }
    else if (!IntFix) {
        IntHold = INT_MAIN;
        IntVectX = Settings.MousePos.x - IntX;
        IntVectY = Settings.MousePos.y - IntY;
        return;
    }
    else {
        return;
    }

    IntHold = INT_BUTTON;
}

void FOMapper::IntLMouseUp()
{
    FO_STACK_TRACE_ENTRY();

    if (IntHold == INT_SELECT && _curMap->GetHexAtScreen(Settings.MousePos, SelectHex2, nullptr)) {
        if (CurMode == CUR_MODE_DEFAULT) {
            if (SelectHex1 != SelectHex2) {
                _curMap->ClearHexTrack();

                vector<mpos> hexes;

                if (SelectAxialGrid) {
                    hexes = Geometry.GetAxialHexes(SelectHex1, SelectHex2, _curMap->GetSize());
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
                    if (item->GetIsTile() && !item->GetIsRoofTile() && IsSelectTile && Settings.ShowTile) {
                        return true;
                    }
                    else if (item->GetIsTile() && item->GetIsRoofTile() && IsSelectRoof && Settings.ShowRoof) {
                        return true;
                    }
                    else if (!item->GetIsTile() && !item->GetIsScenery() && !item->GetIsWall() && IsSelectItem && Settings.ShowItem) {
                        return true;
                    }
                    else if (!item->GetIsTile() && item->GetIsScenery() && IsSelectScen && Settings.ShowScen) {
                        return true;
                    }
                    else if (!item->GetIsTile() && item->GetIsWall() && IsSelectWall && Settings.ShowWall) {
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
                        if (IsSelectCrit && Settings.ShowCrit) {
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

            // Crits or item container
            if (!SelectedEntities.empty() && !GetEntityInnerItems(SelectedEntities.front().get()).empty()) {
                IntSetMode(INT_MODE_INCONT);
            }
        }

        _curMap->RebuildMap();
    }

    IntHold = INT_NONE;
}

void FOMapper::IntMouseMove()
{
    FO_STACK_TRACE_ENTRY();

    if (IntHold == INT_SELECT) {
        _curMap->ClearHexTrack();

        if (!_curMap->GetHexAtScreen(Settings.MousePos, SelectHex2, nullptr)) {
            if (!SelectHex2.is_zero()) {
                _curMap->RebuildMap();
                SelectHex2 = {};
            }

            return;
        }

        if (CurMode == CUR_MODE_DEFAULT) {
            if (SelectHex1 != SelectHex2) {
                if (SelectAxialGrid) {
                    for (const auto hex : Geometry.GetAxialHexes(SelectHex1, SelectHex2, _curMap->GetSize())) {
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
        else if (CurMode == CUR_MODE_MOVE_SELECTION) {
            auto offs_hx = numeric_cast<int32>(SelectHex2.x) - numeric_cast<int32>(SelectHex1.x);
            auto offs_hy = numeric_cast<int32>(SelectHex2.y) - numeric_cast<int32>(SelectHex1.y);
            auto offs_x = Settings.MousePos.x - SelectPos.x;
            auto offs_y = Settings.MousePos.y - SelectPos.y;

            if (SelectMove(!Keyb.ShiftDwn, offs_hx, offs_hy, offs_x, offs_y)) {
                SelectHex1 = _curMap->GetSize().from_raw_pos(SelectHex1.x + offs_hx, SelectHex1.y + offs_hy);
                SelectPos.x += offs_x;
                SelectPos.y += offs_y;
                _curMap->RebuildMap();
            }
        }
    }
    else if (IntHold == INT_MAIN) {
        IntX = Settings.MousePos.x - IntVectX;
        IntY = Settings.MousePos.y - IntVectY;
    }
    else if (IntHold == INT_OBJECT) {
        ObjX = Settings.MousePos.x - ItemVectX;
        ObjY = Settings.MousePos.y - ItemVectY;
    }
}

auto FOMapper::GetTabIndex() const -> int32
{
    FO_STACK_TRACE_ENTRY();

    if (IntMode < TAB_COUNT) {
        return TabsActive[IntMode]->Index;
    }
    return TabIndex[IntMode];
}

void FOMapper::SetTabIndex(int32 index)
{
    FO_STACK_TRACE_ENTRY();

    if (IntMode < TAB_COUNT) {
        TabsActive[IntMode]->Index = index;
    }
    TabIndex[IntMode] = index;
}

void FOMapper::RefreshCurProtos()
{
    FO_STACK_TRACE_ENTRY();

    // Select protos and scroll
    CurItemProtos = nullptr;
    CurProtoScroll = nullptr;
    CurCritterProtos = nullptr;
    InContItem = nullptr;

    if (IntMode >= 0 && IntMode < TAB_COUNT) {
        auto* stab = TabsActive[IntMode].get();

        if (!stab->CritterProtos.empty()) {
            CurCritterProtos = &stab->CritterProtos;
        }
        else {
            CurItemProtos = &stab->ItemProtos;
        }

        CurProtoScroll = &stab->Scroll;
    }

    if (IntMode == INT_MODE_INCONT) {
        InContScroll = 0;
    }

    if (_curMap) {
        // Update fast pids
        _curMap->ClearFastPids();
        for (const auto& fast_proto : TabsActive[INT_MODE_FAST]->ItemProtos) {
            _curMap->AddFastPid(fast_proto->GetProtoId());
        }

        // Update ignore pids
        _curMap->ClearIgnorePids();

        for (const auto& ignore_proto : TabsActive[INT_MODE_IGNORE]->ItemProtos) {
            _curMap->AddIgnorePid(ignore_proto->GetProtoId());
        }

        // Refresh map
        _curMap->RebuildMap();
    }
}

void FOMapper::IntSetMode(int32 mode)
{
    FO_STACK_TRACE_ENTRY();

    if (SubTabsActive && mode == SubTabsActiveTab) {
        SubTabsActive = false;
        return;
    }

    if (!SubTabsActive && mode == IntMode && mode >= 0 && mode < TAB_COUNT) {
        // Show sub tabs screen
        SubTabsActive = true;
        SubTabsActiveTab = mode;

        // Calculate position
        if (mode <= INT_MODE_CUSTOM9) {
            SubTabsX = IntBCust[mode - INT_MODE_CUSTOM0].x + IntBCust[mode - INT_MODE_CUSTOM0].width / 2;
            SubTabsY = IntBCust[mode - INT_MODE_CUSTOM0].y;
        }
        else if (mode == INT_MODE_ITEM) {
            SubTabsX = IntBItem.x + IntBItem.width / 2;
            SubTabsY = IntBItem.y;
        }
        else if (mode == INT_MODE_TILE) {
            SubTabsX = IntBTile.x + IntBTile.width / 2;
            SubTabsY = IntBTile.y;
        }
        else if (mode == INT_MODE_CRIT) {
            SubTabsX = IntBCrit.x + IntBCrit.width / 2;
            SubTabsY = IntBCrit.y;
        }
        else if (mode == INT_MODE_FAST) {
            SubTabsX = IntBFast.x + IntBFast.width / 2;
            SubTabsY = IntBFast.y;
        }
        else if (mode == INT_MODE_IGNORE) {
            SubTabsX = IntBIgnore.x + IntBIgnore.width / 2;
            SubTabsY = IntBIgnore.y;
        }
        else {
            FO_UNREACHABLE_PLACE();
        }

        SubTabsX += IntX - SubTabsRect.width / 2;
        SubTabsY += IntY - SubTabsRect.height;

        SubTabsX = std::max(SubTabsX, 0);
        if (SubTabsX + SubTabsRect.width > Settings.ScreenWidth) {
            SubTabsX -= SubTabsX + SubTabsRect.width - Settings.ScreenWidth;
        }

        SubTabsY = std::max(SubTabsY, 0);
        if (SubTabsY + SubTabsRect.height > Settings.ScreenHeight) {
            SubTabsY -= SubTabsY + SubTabsRect.height - Settings.ScreenHeight;
        }

        return;
    }

    IntMode = mode;
    IntHold = INT_NONE;

    RefreshCurProtos();

    if (SubTabsActive) {
        // Reinit sub tabs
        SubTabsActive = false;
        IntSetMode(IntMode);
    }
}

void FOMapper::MoveEntity(ClientEntity* entity, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (!_curMap->GetSize().is_valid_pos(hex)) {
        return;
    }

    SelectClear();

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        _curMap->MoveCritter(cr, hex, false);
    }
    else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        _curMap->MoveItem(item, hex);
    }
}

void FOMapper::DeleteEntity(ClientEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    if (SelectedEntitiesSet.erase(entity) == 0) {
        return;
    }

    vec_remove_unique_value(SelectedEntities, entity);

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        _curMap->DestroyCritter(cr);
    }
    else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        _curMap->DestroyItem(item);
    }
}

void FOMapper::SelectAdd(ClientEntity* entity, optional<mpos> hex, bool skip_refresh)
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

    // Make transparent
    if (auto* hex_view = dynamic_cast<HexView*>(corrected_entity); hex_view != nullptr) {
        hex_view->SetTargetAlpha(SelectAlpha);
    }

    if (!skip_refresh) {
        _curMap->DefferedRefreshItems();
    }
}

void FOMapper::SelectAll()
{
    FO_STACK_TRACE_ENTRY();

    SelectClear();

    for (auto& item : _curMap->GetItems()) {
        if (_curMap->IsIgnorePid(item->GetProtoId())) {
            continue;
        }

        if ((!item->GetIsScenery() && !item->GetIsWall() && IsSelectItem && Settings.ShowItem) || //
            (item->GetIsScenery() && IsSelectScen && Settings.ShowScen) || //
            (item->GetIsWall() && IsSelectWall && Settings.ShowWall) || //
            (item->GetIsTile() && !item->GetIsRoofTile() && IsSelectTile && Settings.ShowTile) || //
            (item->GetIsTile() && item->GetIsRoofTile() && IsSelectRoof && Settings.ShowRoof)) {
            SelectAdd(item.get());
        }
    }

    if (IsSelectCrit && Settings.ShowCrit) {
        for (auto& cr : _curMap->GetCritters()) {
            SelectAdd(cr.get());
        }
    }

    _curMap->RebuildMap();
}

void FOMapper::SelectRemove(ClientEntity* entity, bool skip_refresh)
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

void FOMapper::SelectClear()
{
    FO_STACK_TRACE_ENTRY();

    while (!SelectedEntities.empty()) {
        SelectRemove(SelectedEntities.back().get(), true);
    }

    _curMap->DefferedRefreshItems();
}

auto FOMapper::SelectMove(bool hex_move, int32& offs_hx, int32& offs_hy, int32& offs_x, int32& offs_y) -> bool
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

    // Change moving speed on zooming
    if (!hex_move) {
        static auto small_ox = 0.0f;
        static auto small_oy = 0.0f;
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
        // Disable moving if out of map bounds
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

    // Move map objects
    for (auto& entity : SelectedEntities) {
        if (!hex_move) {
            auto item = entity.dyn_cast<ItemHexView>();

            if (item == nullptr) {
                continue;
            }

            auto ox = item->GetOffset().x + offs_x;
            auto oy = item->GetOffset().y + offs_y;

            if (Keyb.AltDwn) {
                ox = oy = 0;
            }

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
                if (item->IsNonEmptyMultihexMesh()) {
                    auto multihex_mesh = item->GetMultihexMesh();

                    std::ranges::for_each(multihex_mesh, [&](mpos& hex2) {
                        ipos32 raw_hex2 = ipos32(hex2);
                        move_hex(raw_hex2);
                        hex2 = _curMap->GetSize().clamp_pos(raw_hex2);
                    });

                    item->SetMultihexMesh(multihex_mesh);
                }

                _curMap->MoveItem(item.get(), hex);
            }
            else if (auto cr = entity.dyn_cast<CritterHexView>()) {
                _curMap->MoveCritter(cr.get(), hex, false);
            }
        }
    }

    return true;
}

void FOMapper::SelectDelete()
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return;
    }

    for (auto& entity : copy_hold_ref(SelectedEntities)) {
        DeleteEntity(entity.get());
    }

    SelectClear();
    _curMap->RebuildMap();
    IntHold = INT_NONE;
    SetCurMode(CUR_MODE_DEFAULT);
}

auto FOMapper::CreateCritter(hstring pid, mpos hex) -> CritterView*
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

    _curMap->RebuildMap();
    SetCurMode(CUR_MODE_DEFAULT);

    return cr;
}

auto FOMapper::CreateItem(hstring pid, mpos hex, Entity* owner) -> ItemView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_curMap);

    const auto* proto = ProtoMngr.GetProtoItem(pid);
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
        item = _curMap->AddMapperTile(proto->GetProtoId(), corrected_hex, numeric_cast<uint8>(TileLayer), DrawRoof);
    }
    else {
        item = _curMap->AddMapperItem(proto->GetProtoId(), corrected_hex, nullptr);
    }

    if (owner == nullptr) {
        SelectAdd(item, corrected_hex);
        SetCurMode(CUR_MODE_DEFAULT);
    }
    else {
        IntSetMode(INT_MODE_INCONT);
        InContItem = item;
    }

    return item;
}

auto FOMapper::CloneEntity(Entity* entity) -> Entity*
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
        return cr_clone;
    }

    if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        auto* item_clone = _curMap->AddMapperItem(item->GetProtoId(), item->GetHex(), &item->GetProperties());

        CloneInnerItems(item_clone, item);

        SelectAdd(item_clone);
        return item_clone;
    }

    return nullptr;
}

void FOMapper::CloneInnerItems(ItemView* to_item, const ItemView* from_item)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& inner_item : from_item->GetInnerItems()) {
        const auto stack_id = any_t {string(inner_item->GetContainerStack())};
        auto* inner_item_clone = to_item->AddMapperInnerItem(_curMap->GenTempEntityId(), dynamic_cast<const ProtoItem*>(inner_item->GetProto()), stack_id, &from_item->GetProperties());
        CloneInnerItems(inner_item_clone, inner_item.get());
    }
}

auto FOMapper::MergeItemsToMultihexMeshes(MapView* map) -> size_t
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

auto FOMapper::TryMergeItemToMultihexMesh(MapView* map, ItemHexView* item, bool skip_selected) -> ItemHexView*
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

void FOMapper::MergeItemToMultihexMesh(MapView* map, ItemHexView* source_item, ItemHexView* target_item)
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

void FOMapper::FindMultihexMeshForItemAroundHex(MapView* map, ItemHexView* item, mpos hex, bool merge_to_it, unordered_set<ItemHexView*>& result) const
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
        if (mpos hex2 = hex; GeometryHelper::MoveHexByDir(hex2, static_cast<uint8>(i), map->GetSize())) {
            if (auto* mergable_item = find_mergable_item_on_hex(hex2); mergable_item != nullptr) {
                result.emplace(mergable_item);
            }
        }
    }
}

auto FOMapper::CompareMultihexItemForMerge(const ItemHexView* source_item, const ItemHexView* target_item, bool allow_clean_merge) const -> bool
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

auto FOMapper::BreakItemsMultihexMeshes(MapView* map) -> size_t
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

auto FOMapper::TryBreakItemFromMultihexMesh(MapView* map, ItemHexView* item, mpos hex) -> ItemHexView*
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

void FOMapper::BufferCopy()
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

void FOMapper::BufferCut()
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return;
    }

    BufferCopy();
    SelectDelete();
}

void FOMapper::BufferPaste()
{
    FO_STACK_TRACE_ENTRY();

    if (!_curMap) {
        return;
    }

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
        }
        else if (entity_buf.IsItem) {
            auto* item = _curMap->AddMapperItem(entity_buf.Proto->GetProtoId(), hex, entity_buf.Props.get());
            add_item_inner_items(&entity_buf, item);
            SelectAdd(item);
        }
    }
}

void FOMapper::DrawStr(const irect32& rect, string_view str, uint32 flags, ucolor color, int32 num_font)
{
    FO_STACK_TRACE_ENTRY();

    SprMngr.DrawText({rect.x, rect.y, rect.width, rect.height}, str, flags, color, num_font);
}

void FOMapper::CurDraw()
{
    FO_STACK_TRACE_ENTRY();

    switch (CurMode) {
    case CUR_MODE_DEFAULT:
    case CUR_MODE_MOVE_SELECTION: {
        const auto* anim = CurMode == CUR_MODE_DEFAULT ? CurPDef.get() : CurPHand.get();
        if (anim != nullptr) {
            SprMngr.DrawSprite(anim, Settings.MousePos, COLOR_SPRITE);
        }
    } break;
    case CUR_MODE_PLACE_OBJECT:
        if (IsItemMode() && !CurItemProtos->empty()) {
            const auto& proto = (*CurItemProtos)[GetTabIndex()];

            mpos hex;

            if (!_curMap->GetHexAtScreen(Settings.MousePos, hex, nullptr)) {
                break;
            }

            if (proto->GetIsTile()) {
                hex = _curMap->GetSize().from_raw_pos(hex.x - hex.x % Settings.MapTileStep, hex.y - hex.y % Settings.MapTileStep);
            }

            const auto* spr = GetIfaceSpr(proto->GetPicMap());

            if (spr != nullptr) {
                ipos32 pos = _curMap->MapToScreenPos(_curMap->GetHexMapPos(hex));
                pos += ipos32(proto->GetOffset());
                pos += spr->GetOffset();
                pos += ipos32(Settings.MapHexWidth / 2, Settings.MapHexHeight / 2);
                pos -= ipos32(spr->GetSize().width / 2, spr->GetSize().height);

                if (proto->GetIsTile()) {
                    if (DrawRoof) {
                        pos.x += Settings.MapRoofOffsX;
                        pos.y += Settings.MapRoofOffsY;
                    }
                    else {
                        pos.x += Settings.MapTileOffsX;
                        pos.y += Settings.MapTileOffsY;
                    }
                }

                const auto width = iround<int32>(numeric_cast<float32>(spr->GetSize().width) * _curMap->GetSpritesZoom());
                const auto height = iround<int32>(numeric_cast<float32>(spr->GetSize().height) * _curMap->GetSpritesZoom());
                SprMngr.DrawSpriteSize(spr, pos, {width, height}, true, false, COLOR_SPRITE);
            }
        }
        else if (IsCritMode() && !CurCritterProtos->empty()) {
            const auto model_name = (*CurCritterProtos)[GetTabIndex()]->GetModelName();
            const auto* anim = ResMngr.GetCritterPreviewSpr(model_name, CritterStateAnim::Unarmed, CritterActionAnim::Idle, CritterDir, nullptr);

            if (anim == nullptr) {
                anim = ResMngr.GetItemDefaultSpr().get();
            }

            mpos hex;

            if (!_curMap->GetHexAtScreen(Settings.MousePos, hex, nullptr)) {
                break;
            }

            ipos32 pos = _curMap->MapToScreenPos(_curMap->GetHexMapPos(hex));
            pos += anim->GetOffset();
            pos -= ipos32(anim->GetSize().width / 2, anim->GetSize().height);

            const auto width = iround<int32>(numeric_cast<float32>(anim->GetSize().width) * _curMap->GetSpritesZoom());
            const auto height = iround<int32>(numeric_cast<float32>(anim->GetSize().height) * _curMap->GetSpritesZoom());
            SprMngr.DrawSpriteSize(anim, pos, {width, height}, true, false, COLOR_SPRITE);
        }
        else {
            SetCurMode(CUR_MODE_DEFAULT);
        }
        break;
    default:
        SetCurMode(CUR_MODE_DEFAULT);
        break;
    }
}

void FOMapper::CurRMouseUp()
{
    FO_STACK_TRACE_ENTRY();

    if (IntHold == INT_NONE) {
        if (CurMode == CUR_MODE_MOVE_SELECTION) {
            SetCurMode(CUR_MODE_DEFAULT);
        }
        else if (CurMode == CUR_MODE_PLACE_OBJECT) {
            SetCurMode(CUR_MODE_DEFAULT);
        }
        else if (CurMode == CUR_MODE_DEFAULT) {
            if (!SelectedEntities.empty()) {
                SetCurMode(CUR_MODE_MOVE_SELECTION);
            }
            else if (IsItemMode() || IsCritMode()) {
                SetCurMode(CUR_MODE_PLACE_OBJECT);
            }
        }
    }
}

void FOMapper::CurMMouseDown()
{
    FO_STACK_TRACE_ENTRY();

    if (SelectedEntities.empty()) {
        CritterDir++;

        if (CritterDir >= GameSettings::MAP_DIR_COUNT) {
            CritterDir = 0;
        }

        DrawRoof = !DrawRoof;
    }
    else {
        for (auto& entity : SelectedEntities) {
            if (auto cr = entity.dyn_cast<CritterHexView>()) {
                const auto dir = cr->GetDir() + 1;
                cr->ChangeDir(numeric_cast<uint8>(dir % GameSettings::MAP_DIR_COUNT));
            }
        }
    }
}

void FOMapper::SetCurMode(int cur_mode)
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

auto FOMapper::IsCurInRect(const irect32& rect, int32 ax, int32 ay) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return Settings.MousePos.x >= rect.x + ax && Settings.MousePos.y >= rect.y + ay && Settings.MousePos.x < rect.x + rect.width + ax && Settings.MousePos.y < rect.y + rect.height + ay;
}

auto FOMapper::IsCurInRect(const irect32& rect) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return Settings.MousePos.x >= rect.x && Settings.MousePos.y >= rect.y && Settings.MousePos.x < rect.x + rect.width && Settings.MousePos.y < rect.y + rect.height;
}

auto FOMapper::IsCurInRectNoTransp(const Sprite* spr, const irect32& rect, int32 ax, int32 ay) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return IsCurInRect(rect, ax, ay) && SprMngr.SpriteHitTest(spr, {Settings.MousePos.x - rect.x - ax, Settings.MousePos.y - rect.y - ay});
}

auto FOMapper::IsCurInInterface() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (IntVisible && SubTabsActive && IsCurInRectNoTransp(SubTabsPic.get(), SubTabsRect, SubTabsX, SubTabsY)) {
        return true;
    }
    if (IntVisible && IsCurInRectNoTransp(IntMainPic.get(), IntWMain, IntX, IntY)) {
        return true;
    }
    if (ObjVisible && !SelectedEntities.empty() && IsCurInRectNoTransp(ObjWMainPic.get(), ObjWMain, ObjX, ObjY)) {
        return true;
    }
    return false;
}

auto FOMapper::GetCurHex(mpos& hex, bool ignore_interface) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    hex = {};

    if (!ignore_interface && IsCurInInterface()) {
        return false;
    }

    return _curMap->GetHexAtScreen(Settings.MousePos, hex, nullptr);
}

void FOMapper::ConsoleDraw()
{
    FO_STACK_TRACE_ENTRY();

    if (ConsoleEdit) {
        SprMngr.DrawSprite(ConsolePic.get(), {IntX + ConsolePicX, (IntVisible ? IntY : Settings.ScreenHeight) + ConsolePicY}, COLOR_SPRITE);

        auto str = ConsoleStr;
        str.insert(ConsoleCur, timespan(GameTime.GetFrameTime().duration_value()).to_ms<int32>() % 800 < 400 ? "!" : ".");
        DrawStr(irect32(IntX + ConsoleTextX, (IntVisible ? IntY : Settings.ScreenHeight) + ConsoleTextY, Settings.ScreenWidth, Settings.ScreenHeight), str, FT_NOBREAK, COLOR_TEXT, FONT_DEFAULT);
    }
}

void FOMapper::ConsoleKeyDown(KeyCode dik, string_view dik_text)
{
    FO_STACK_TRACE_ENTRY();

    if (dik == KeyCode::Return || dik == KeyCode::Numpadenter) {
        if (ConsoleEdit) {
            if (ConsoleStr.empty()) {
                ConsoleEdit = false;
            }
            else {
                // Modify console history
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

                // Save console history
                string history_str;

                for (const auto& str : ConsoleHistory) {
                    history_str += str + "\n";
                }

                Cache.SetString("mapper_console.txt", history_str);

                // Process command
                const auto process_command = OnMapperMessage.Fire(ConsoleStr);
                AddMess(ConsoleStr);

                if (process_command) {
                    ParseCommand(ConsoleStr);
                }

                ConsoleStr = "";
                ConsoleCur = 0;
            }
        }
        else {
            ConsoleEdit = true;
            ConsoleStr = "";
            ConsoleCur = 0;
            ConsoleHistoryCur = numeric_cast<int32>(ConsoleHistory.size());
        }
    }
    else {
        switch (dik) {
        case KeyCode::Up: {
            if (ConsoleHistoryCur > 0) {
                ConsoleHistoryCur--;
                ConsoleStr = ConsoleHistory[ConsoleHistoryCur];
                ConsoleCur = numeric_cast<int32>(ConsoleStr.length());
            }
        } break;
        case KeyCode::Down: {
            if (ConsoleHistoryCur + 1 >= numeric_cast<int32>(ConsoleHistory.size())) {
                ConsoleHistoryCur = numeric_cast<int32>(ConsoleHistory.size());
                ConsoleStr = "";
                ConsoleCur = 0;
            }
            else {
                ConsoleHistoryCur++;
                ConsoleStr = ConsoleHistory[ConsoleHistoryCur];
                ConsoleCur = numeric_cast<int32>(ConsoleStr.length());
            }
        } break;
        default: {
            Keyb.FillChar(dik, dik_text, ConsoleStr, &ConsoleCur, KIF_NO_SPEC_SYMBOLS);
            ConsoleLastKey = dik;
            ConsoleLastKeyText = dik_text;
            ConsoleKeyTime = GameTime.GetFrameTime();
            ConsoleAccelerate = 1;
        } break;
        }
    }
}

void FOMapper::ConsoleKeyUp(KeyCode /*key*/)
{
    FO_STACK_TRACE_ENTRY();

    ConsoleLastKey = KeyCode::None;
    ConsoleLastKeyText = "";
}

void FOMapper::ConsoleProcess()
{
    FO_STACK_TRACE_ENTRY();

    if (ConsoleLastKey == KeyCode::None) {
        return;
    }

    if ((GameTime.GetFrameTime() - ConsoleKeyTime).to_ms<int32>() >= CONSOLE_KEY_TICK - ConsoleAccelerate) {
        ConsoleKeyTime = GameTime.GetFrameTime();
        ConsoleAccelerate = CONSOLE_MAX_ACCELERATE;
        Keyb.FillChar(ConsoleLastKey, ConsoleLastKeyText, ConsoleStr, &ConsoleCur, KIF_NO_SPEC_SYMBOLS);
    }
}

void FOMapper::ParseCommand(string_view command)
{
    FO_STACK_TRACE_ENTRY();

    if (command.empty()) {
        return;
    }

    // Load map
    if (command[0] == '~') {
        string map_name = strex(command.substr(1)).trim();

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
        string map_name = strex(command.substr(1)).trim();

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
        const auto command_str = string(command.substr(1));
        istringstream icmd(command_str);
        string func_name;

        if (!(icmd >> func_name)) {
            AddMess("Function name not typed");
            return;
        }

        auto func = ScriptSys.FindFunc<string, string>(Hashes.ToHashedString(func_name));

        if (!func) {
            AddMess("Function not found");
            return;
        }

        string str = strex(command).substring_after(' ').trim();

        if (!func(str)) {
            AddMess("Script execution fail");
            return;
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

        vector<int32> anims = strex(command.substr(1)).split_to_int32(' ');

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
            for (auto& item : _curMap->GetItems()) {
                auto rgba = item->GetLightColor().rgba;
                rgba = (rgba & 0xFF000000) | ((rgba & 0xFF) << 16) | (rgba & 0xFF00) | ((rgba & 0xFF0000) >> 16);
                item->SetLightColor(ucolor(rgba));
            }

            _curMap->RebuildMap();
        }
        else if (command_ext == "merge-items" && _curMap) {
            MergeItemsToMultihexMeshes(_curMap.get());
            FO_RUNTIME_ASSERT(MergeItemsToMultihexMeshes(_curMap.get()) == 0);
        }
        else if (command_ext == "break-items" && _curMap) {
            BreakItemsMultihexMeshes(_curMap.get());
            FO_RUNTIME_ASSERT(BreakItemsMultihexMeshes(_curMap.get()) == 0);
        }
    }
    else {
        AddMess("Unknown command");
    }
}

auto FOMapper::LoadMap(string_view map_name) -> MapView*
{
    FO_STACK_TRACE_ENTRY();

    const auto map_files = MapsFileSys.FilterFiles("fomap");
    const auto map_file = map_files.FindFileByName(map_name);

    if (!map_file) {
        AddMess("Map file not found");
        return nullptr;
    }

    const auto map_file_str = map_file.GetStr();
    const auto map_data = ConfigFile(strex("{}.fomap", map_name), map_file_str, &Hashes, ConfigFileOption::ReadFirstSection);

    if (!map_data.HasSection("ProtoMap")) {
        throw MapLoaderException("Invalid map format", map_name);
    }

    auto pmap = SafeAlloc::MakeRefCounted<ProtoMap>(Hashes.ToHashedString(map_name), GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME));
    pmap->AddRef(); // Todo: fix memleak
    pmap->GetPropertiesForEdit().ApplyFromText(map_data.GetSection("ProtoMap"));

    auto new_map = SafeAlloc::MakeRefCounted<MapView>(this, ident_t {}, pmap.get());
    new_map->EnableMapperMode();

    try {
        new_map->LoadFromFile(map_name, map_file.GetStr());
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

    return LoadedMaps.back().get();
}

void FOMapper::ShowMap(MapView* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!map->IsDestroyed());

    const auto it = std::ranges::find(LoadedMaps, map);
    FO_RUNTIME_ASSERT(it != LoadedMaps.end());

    if (_curMap != map) {
        if (_curMap) {
            SelectClear();
        }

        _curMap = map;
        RefreshCurProtos();
    }
}

void FOMapper::SaveMap(MapView* map, string_view custom_name)
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

    auto fomap_file = DiskFileSystem::OpenFile(fomap_path, true);
    FO_RUNTIME_ASSERT(fomap_file);
    const auto fomap_file_ok = fomap_file.Write(fomap_content);
    FO_RUNTIME_ASSERT(fomap_file_ok);

    OnEditMapSave.Fire(map);
}

void FOMapper::UnloadMap(MapView* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!map->IsDestroyed());

    if (_curMap == map) {
        SelectClear();
        _curMap = nullptr;
    }

    const auto it = std::ranges::find(LoadedMaps, map);
    FO_RUNTIME_ASSERT(it != LoadedMaps.end());

    map->MarkAsDestroyed();
    LoadedMaps.erase(it);
}

void FOMapper::ResizeMap(MapView* map, int32 width, int32 height)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!map->IsDestroyed());

    const auto corrected_width = std::clamp(width, GameSettings::MIN_MAP_SIZE, GameSettings::MAX_MAP_SIZE);
    const auto corrected_height = std::clamp(height, GameSettings::MIN_MAP_SIZE, GameSettings::MAX_MAP_SIZE);

    map->Resize(msize(numeric_cast<int16>(corrected_width), numeric_cast<int16>(corrected_height)));
    map->InstantScrollTo(map->GetWorkHex());

    if (_curMap == map) {
        SelectClear();
    }
}

void FOMapper::AddMess(string_view message_text)
{
    FO_STACK_TRACE_ENTRY();

    const string str = strex("|{} - {}\n", COLOR_TEXT, message_text);
    const auto time = nanotime::now().desc(true);
    const string mess_time = strex("{:02}:{:02}:{:02} ", time.hour, time.minute, time.second);

    MessBox.emplace_back(MessBoxMessage {.Type = 0, .Mess = str, .Time = mess_time});
    MessBoxScroll = 0;
    MessBoxCurText = "";

    const irect32 ir(IntWWork.x + IntX, IntWWork.y + IntY, IntWWork.width, IntWWork.height);
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

void FOMapper::MessBoxDraw()
{
    FO_STACK_TRACE_ENTRY();

    if (!IntVisible) {
        return;
    }
    if (MessBoxCurText.empty()) {
        return;
    }

    DrawStr(irect32(IntWWork.x + IntX, IntWWork.y + IntY, IntWWork.width, IntWWork.height), MessBoxCurText, FT_UPPER | FT_BOTTOM, COLOR_TEXT, FONT_DEFAULT);
}

void FOMapper::DrawIfaceLayer(int32 layer)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(layer);

    SpritesCanDraw = true;
    OnRenderIface.Fire(); // Todo: mapper render iface layer
    SpritesCanDraw = false;
}

auto FOMapper::GetEntityInnerItems(ClientEntity* entity) const -> vector<refcount_ptr<ItemView>>
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

FO_END_NAMESPACE();
