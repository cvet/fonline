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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "DiskFileSystem.h"
#include "GenericUtils.h"
#include "Log.h"
#include "MapperScripting.h"
#include "StringUtils.h"

FOMapper::FOMapper(GlobalSettings& settings, AppWindow* window) :
    FOClient(settings, window, true)
{
    STACK_TRACE_ENTRY();

    Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath("FullProtos"));
    if constexpr (FO_ANGELSCRIPT_SCRIPTING) {
        Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath("MapperAngelScript"));
    }

    for (const auto& dir : settings.BakeContentEntries) {
        ContentFileSys.AddDataSource(dir, DataSourceType::NonCachedDirRoot);
    }

    if (settings.BakeContentEntries.empty() && !Settings.MapsDir.empty()) {
        if (!DiskFileSystem::IsDir(Settings.MapsDir)) {
            throw MapperException("Directory with maps not found", Settings.MapsDir);
        }

        ContentFileSys.AddDataSource(Settings.MapsDir, DataSourceType::NonCachedDirRoot);
    }

    extern void Mapper_RegisterData(FOEngineBase*);
    Mapper_RegisterData(this);
    ScriptSys = new MapperScriptSystem(this);
    ScriptSys->InitSubsystems();

    _curLang = LanguagePack {Settings.Language, *this};
    _curLang.LoadTexts(Resources);

    ProtoMngr.LoadFromResources();

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
    RUNTIME_ASSERT(load_fonts_ok);
    SprMngr.SetDefaultFont(FONT_DEFAULT);

    SprMngr.BeginScene({100, 100, 100});
    SprMngr.EndScene();

    InitIface();

    // Initialize tabs
    const auto& cr_protos = ProtoMngr.GetProtoCritters();
    for (auto&& [pid, proto] : cr_protos) {
        Tabs[INT_MODE_CRIT][DEFAULT_SUB_TAB].NpcProtos.push_back(proto);
        Tabs[INT_MODE_CRIT][proto->CollectionName].NpcProtos.push_back(proto);
    }
    for (auto&& [pid, proto] : Tabs[INT_MODE_CRIT]) {
        std::sort(proto.NpcProtos.begin(), proto.NpcProtos.end(), [](const ProtoCritter* a, const ProtoCritter* b) -> bool { return a->GetName() < b->GetName(); });
    }

    const auto& item_protos = ProtoMngr.GetProtoItems();
    for (auto&& [pid, proto] : item_protos) {
        Tabs[INT_MODE_ITEM][DEFAULT_SUB_TAB].ItemProtos.push_back(proto);
        Tabs[INT_MODE_ITEM][proto->CollectionName].ItemProtos.push_back(proto);
    }
    for (auto&& [pid, proto] : Tabs[INT_MODE_ITEM]) {
        std::sort(proto.ItemProtos.begin(), proto.ItemProtos.end(), [](const ProtoItem* a, const ProtoItem* b) -> bool { return a->GetName() < b->GetName(); });
    }

    for (auto i = 0; i < TAB_COUNT; i++) {
        if (Tabs[i].empty()) {
            Tabs[i][DEFAULT_SUB_TAB].Scroll = 0;
        }
        TabsActive[i] = &(*Tabs[i].begin()).second;
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
    ScriptSys->InitModules();
    OnStart.Fire();

    if (!Settings.StartMap.empty()) {
        auto* map = LoadMap(Settings.StartMap);

        if (map != nullptr) {
            if (Settings.StartHexX > 0 && Settings.StartHexY > 0) {
                CurMap->FindSetCenter(Settings.StartHexX, Settings.StartHexY);
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
        ConsoleHistory.push_back(history_part);
        prev = pos + 1;
    }
    ConsoleHistory = _str(history_str).normalizeLineEndings().split('\n');
    while (ConsoleHistory.size() > Settings.ConsoleHistorySize) {
        ConsoleHistory.erase(ConsoleHistory.begin());
    }
    ConsoleHistoryCur = static_cast<int>(ConsoleHistory.size());
}

void FOMapper::InitIface()
{
    STACK_TRACE_ENTRY();

    WriteLog("Init interface");

    const auto config_content = Resources.ReadFileText("mapper_default.ini");

    IfaceIni.reset(new ConfigFile("mapper_default.ini", config_content));
    const auto& ini = *IfaceIni;

    // Interface
    IntX = ini.GetInt("", "IntX", -1);
    IntY = ini.GetInt("", "IntY", -1);

    IfaceLoadRect(IntWMain, "IntMain");
    if (IntX == -1) {
        IntX = (Settings.ScreenWidth - IntWMain.Width()) / 2;
    }
    if (IntY == -1) {
        IntY = Settings.ScreenHeight - IntWMain.Height();
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
    SelectType = SELECT_TYPE_NEW;
    ProtoWidth = ini.GetInt("", "ProtoWidth", 50);
    ProtosOnScreen = (IntWWork[2] - IntWWork[0]) / ProtoWidth;
    std::memset(TabIndex, 0, sizeof(TabIndex));
    NpcDir = 3;
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
    ConsolePicX = ini.GetInt("", "ConsolePicX", 0);
    ConsolePicY = ini.GetInt("", "ConsolePicY", 0);
    ConsoleTextX = ini.GetInt("", "ConsoleTextX", 0);
    ConsoleTextY = ini.GetInt("", "ConsoleTextY", 0);

    // Cursor
    CurPDef = SprMngr.LoadSprite(ini.GetStr("", "CurDefault", "actarrow.frm"), AtlasType::IfaceSprites);
    CurPHand = SprMngr.LoadSprite(ini.GetStr("", "CurHand", "hand.frm"), AtlasType::IfaceSprites);

    // Iface
    IntMainPic = SprMngr.LoadSprite(ini.GetStr("", "IntMainPic", "error"), AtlasType::IfaceSprites);
    IntPTab = SprMngr.LoadSprite(ini.GetStr("", "IntTabPic", "error"), AtlasType::IfaceSprites);
    IntPSelect = SprMngr.LoadSprite(ini.GetStr("", "IntSelectPic", "error"), AtlasType::IfaceSprites);
    IntPShow = SprMngr.LoadSprite(ini.GetStr("", "IntShowPic", "error"), AtlasType::IfaceSprites);

    // Object
    ObjWMainPic = SprMngr.LoadSprite(ini.GetStr("", "ObjMainPic", "error"), AtlasType::IfaceSprites);
    ObjPbToAllDn = SprMngr.LoadSprite(ini.GetStr("", "ObjToAllPicDn", "error"), AtlasType::IfaceSprites);

    // Sub tabs
    SubTabsPic = SprMngr.LoadSprite(ini.GetStr("", "SubTabsPic", "error"), AtlasType::IfaceSprites);

    // Console
    ConsolePic = SprMngr.LoadSprite(ini.GetStr("", "ConsolePic", "error"), AtlasType::IfaceSprites);

    WriteLog("Init interface complete");
}

auto FOMapper::IfaceLoadRect(IRect& comp, string_view name) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto res = IfaceIni->GetStr("", name);
    if (res.empty()) {
        WriteLog("Signature '{}' not found", name);
        return false;
    }

    if (auto istr = istringstream(res); !(istr >> comp[0] && istr >> comp[1] && istr >> comp[2] && istr >> comp[3])) {
        comp.Clear();
        WriteLog("Unable to parse signature '{}'", name);
        return false;
    }

    return true;
}

auto FOMapper::GetIfaceSpr(hstring fname) -> Sprite*
{
    STACK_TRACE_ENTRY();

    if (const auto it = IfaceSpr.find(fname); it != IfaceSpr.end()) {
        return it->second.get();
    }

    auto&& spr = SprMngr.LoadSprite(fname, AtlasType::IfaceSprites);

    if (spr) {
        spr->PlayDefault();
    }

    return IfaceSpr.emplace(fname, std::move(spr)).first->second.get();
}

void FOMapper::ProcessMapperInput()
{
    STACK_TRACE_ENTRY();

    std::tie(Settings.MouseX, Settings.MouseY) = App->Input.GetMousePosition();

    if (const bool is_fullscreen = SprMngr.IsFullscreen(); (is_fullscreen && Settings.FullscreenMouseScroll) || (!is_fullscreen && Settings.WindowedMouseScroll)) {
        Settings.ScrollMouseRight = Settings.MouseX >= Settings.ScreenWidth - 1;
        Settings.ScrollMouseLeft = Settings.MouseX <= 0;
        Settings.ScrollMouseDown = Settings.MouseY >= Settings.ScreenHeight - 1;
        Settings.ScrollMouseUp = Settings.MouseY <= 0;
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
            if (dikdw != KeyCode::None && PressedKeys[static_cast<int>(dikdw)]) {
                continue;
            }
            if (dikup != KeyCode::None && !PressedKeys[static_cast<int>(dikup)]) {
                continue;
            }

            // Keyboard states, to know outside function
            PressedKeys[static_cast<int>(dikup)] = false;
            PressedKeys[static_cast<int>(dikdw)] = true;

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
                    if (CurMap != nullptr) {
                        CurMap->RefreshMap();
                    }
                    break;
                case KeyCode::F2:
                    Settings.ShowScen = !Settings.ShowScen;
                    if (CurMap != nullptr) {
                        CurMap->RefreshMap();
                    }
                    break;
                case KeyCode::F3:
                    Settings.ShowWall = !Settings.ShowWall;
                    if (CurMap != nullptr) {
                        CurMap->RefreshMap();
                    }
                    break;
                case KeyCode::F4:
                    Settings.ShowCrit = !Settings.ShowCrit;
                    if (CurMap != nullptr) {
                        CurMap->RefreshMap();
                    }
                    break;
                case KeyCode::F5:
                    Settings.ShowTile = !Settings.ShowTile;
                    if (CurMap != nullptr) {
                        CurMap->RefreshMap();
                    }
                    break;
                case KeyCode::F6:
                    Settings.ShowFast = !Settings.ShowFast;
                    if (CurMap != nullptr) {
                        CurMap->RefreshMap();
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
                    if (CurMap != nullptr) {
                        CurMap->SwitchShowHex();
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
                    if (CurMap != nullptr && !ConsoleEdit && SelectedEntities.empty()) {
                        int day_time = CurMap->GetGlobalDayTime();
                        day_time += 60;
                        SetMinute(day_time % 60);
                        SetHour(day_time / 60 % 24);
                    }
                    break;
                case KeyCode::Subtract:
                    if (CurMap != nullptr && !ConsoleEdit && SelectedEntities.empty()) {
                        int day_time = CurMap->GetGlobalDayTime();
                        day_time -= 60;
                        SetMinute(day_time % 60);
                        SetHour(day_time / 60 % 24);
                    }
                    break;
                case KeyCode::Tab:
                    SelectType = (SelectType == SELECT_TYPE_OLD ? SELECT_TYPE_NEW : SELECT_TYPE_OLD);
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
                case KeyCode::F10:
                    if (CurMap != nullptr) {
                        // CurMap->SwitchShowRain();
                    }
                    break;
                case KeyCode::F11:
                    SprMngr.GetAtlasMngr().DumpAtlases();
                    break;
                case KeyCode::Add:
                    if (CurMap != nullptr && !ConsoleEdit && SelectedEntities.empty()) {
                        int day_time = CurMap->GetGlobalDayTime();
                        day_time += 1;
                        SetMinute(day_time % 60);
                        SetHour(day_time / 60 % 24);
                    }
                    break;
                case KeyCode::Subtract:
                    if (CurMap != nullptr && !ConsoleEdit && SelectedEntities.empty()) {
                        int day_time = CurMap->GetGlobalDayTime();
                        day_time -= 60;
                        SetMinute(day_time % 60);
                        SetHour(day_time / 60 % 24);
                    }
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
                    if (CurMap != nullptr) {
                        SaveMap(CurMap, "");
                    }
                    break;
                case KeyCode::D:
                    Settings.ScrollCheck = !Settings.ScrollCheck;
                    break;
                case KeyCode::B:
                    if (CurMap != nullptr) {
                        CurMap->MarkBlockedHexes();
                    }
                    break;
                case KeyCode::Q:
                    Settings.ShowCorners = !Settings.ShowCorners;
                    break;
                case KeyCode::E:
                    Settings.ShowDrawOrder = !Settings.ShowDrawOrder;
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
                int step = 4;
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
                if (TabsScroll[SubTabsActiveTab] < 0) {
                    TabsScroll[SubTabsActiveTab] = 0;
                }
            }
            else if (IntVisible && IsCurInRect(IntWWork, IntX, IntY) && (IsItemMode() || IsCritMode())) {
                int step = 1;
                if (Keyb.ShiftDwn) {
                    step = static_cast<int>(ProtosOnScreen);
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
                        if (*CurProtoScroll < 0) {
                            *CurProtoScroll = 0;
                        }
                    }
                    else if (IntMode == INT_MODE_INCONT) {
                        InContScroll -= step;
                        if (InContScroll < 0) {
                            InContScroll = 0;
                        }
                    }
                    else if (IntMode == INT_MODE_LIST) {
                        ListScroll -= step;
                        if (ListScroll < 0) {
                            ListScroll = 0;
                        }
                    }
                }
                else {
                    if (IsItemMode() && (*CurItemProtos).size()) {
                        (*CurProtoScroll) += step;
                        if (*CurProtoScroll >= static_cast<int>((*CurItemProtos).size())) {
                            *CurProtoScroll = static_cast<int>((*CurItemProtos).size()) - 1;
                        }
                    }
                    else if (IsCritMode() && CurNpcProtos->size()) {
                        (*CurProtoScroll) += step;
                        if (*CurProtoScroll >= static_cast<int>(CurNpcProtos->size())) {
                            *CurProtoScroll = static_cast<int>(CurNpcProtos->size()) - 1;
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
                if (ev.MouseWheel.Delta != 0 && CurMap != nullptr) {
                    CurMap->ChangeZoom(ev.MouseWheel.Delta > 0 ? -1 : 1);
                }
            }
        }
        else if (ev_type == InputEvent::EventType::MouseDownEvent) {
            if (ev.MouseDown.Button == MouseButton::Middle) {
                CurMMouseDown();
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

void FOMapper::MapperMainLoop()
{
    STACK_TRACE_ENTRY();

    GameTime.FrameAdvance();

    // FPS counter
    if (GameTime.FrameTime() - _fpsTime >= std::chrono::milliseconds {1000}) {
        Settings.FPS = _fpsCounter;
        _fpsCounter = 0;
        _fpsTime = GameTime.FrameTime();
    }
    else {
        _fpsCounter++;
    }

    OnLoop.Fire();
    ConsoleProcess();
    ProcessMapperInput();

    if (CurMap != nullptr) {
        CurMap->Process();
    }

    SprMngr.BeginScene({100, 100, 100});
    {
        DrawIfaceLayer(0);
        if (CurMap != nullptr) {
            CurMap->DrawMap();
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
    }
    SprMngr.EndScene();
}

void FOMapper::IntDraw()
{
    STACK_TRACE_ENTRY();

    if (!IntVisible) {
        return;
    }

    SprMngr.DrawSprite(IntMainPic.get(), IntX, IntY, COLOR_SPRITE);

    switch (IntMode) {
    case INT_MODE_CUSTOM0:
        SprMngr.DrawSprite(IntPTab.get(), IntBCust[0][0] + IntX, IntBCust[0][1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM1:
        SprMngr.DrawSprite(IntPTab.get(), IntBCust[1][0] + IntX, IntBCust[1][1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM2:
        SprMngr.DrawSprite(IntPTab.get(), IntBCust[2][0] + IntX, IntBCust[2][1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM3:
        SprMngr.DrawSprite(IntPTab.get(), IntBCust[3][0] + IntX, IntBCust[3][1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM4:
        SprMngr.DrawSprite(IntPTab.get(), IntBCust[4][0] + IntX, IntBCust[4][1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM5:
        SprMngr.DrawSprite(IntPTab.get(), IntBCust[5][0] + IntX, IntBCust[5][1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM6:
        SprMngr.DrawSprite(IntPTab.get(), IntBCust[6][0] + IntX, IntBCust[6][1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM7:
        SprMngr.DrawSprite(IntPTab.get(), IntBCust[7][0] + IntX, IntBCust[7][1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM8:
        SprMngr.DrawSprite(IntPTab.get(), IntBCust[8][0] + IntX, IntBCust[8][1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM9:
        SprMngr.DrawSprite(IntPTab.get(), IntBCust[9][0] + IntX, IntBCust[9][1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_ITEM:
        SprMngr.DrawSprite(IntPTab.get(), IntBItem[0] + IntX, IntBItem[1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_TILE:
        SprMngr.DrawSprite(IntPTab.get(), IntBTile[0] + IntX, IntBTile[1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_CRIT:
        SprMngr.DrawSprite(IntPTab.get(), IntBCrit[0] + IntX, IntBCrit[1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_FAST:
        SprMngr.DrawSprite(IntPTab.get(), IntBFast[0] + IntX, IntBFast[1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_IGNORE:
        SprMngr.DrawSprite(IntPTab.get(), IntBIgnore[0] + IntX, IntBIgnore[1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_INCONT:
        SprMngr.DrawSprite(IntPTab.get(), IntBInCont[0] + IntX, IntBInCont[1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_MESS:
        SprMngr.DrawSprite(IntPTab.get(), IntBMess[0] + IntX, IntBMess[1] + IntY, COLOR_SPRITE);
        break;
    case INT_MODE_LIST:
        SprMngr.DrawSprite(IntPTab.get(), IntBList[0] + IntX, IntBList[1] + IntY, COLOR_SPRITE);
        break;
    default:
        break;
    }

    for (auto i = INT_MODE_CUSTOM0; i <= INT_MODE_CUSTOM9; i++) {
        SprMngr.DrawStr(IRect(IntBCust[i], IntX, IntY), TabsName[INT_MODE_CUSTOM0 + i], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    }
    SprMngr.DrawStr(IRect(IntBItem, IntX, IntY), TabsName[INT_MODE_ITEM], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(IRect(IntBTile, IntX, IntY), TabsName[INT_MODE_TILE], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(IRect(IntBCrit, IntX, IntY), TabsName[INT_MODE_CRIT], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(IRect(IntBFast, IntX, IntY), TabsName[INT_MODE_FAST], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(IRect(IntBIgnore, IntX, IntY), TabsName[INT_MODE_IGNORE], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(IRect(IntBInCont, IntX, IntY), TabsName[INT_MODE_INCONT], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(IRect(IntBMess, IntX, IntY), TabsName[INT_MODE_MESS], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(IRect(IntBList, IntX, IntY), TabsName[INT_MODE_LIST], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);

    if (Settings.ShowItem) {
        SprMngr.DrawSprite(IntPShow.get(), IntBShowItem[0] + IntX, IntBShowItem[1] + IntY, COLOR_SPRITE);
    }
    if (Settings.ShowScen) {
        SprMngr.DrawSprite(IntPShow.get(), IntBShowScen[0] + IntX, IntBShowScen[1] + IntY, COLOR_SPRITE);
    }
    if (Settings.ShowWall) {
        SprMngr.DrawSprite(IntPShow.get(), IntBShowWall[0] + IntX, IntBShowWall[1] + IntY, COLOR_SPRITE);
    }
    if (Settings.ShowCrit) {
        SprMngr.DrawSprite(IntPShow.get(), IntBShowCrit[0] + IntX, IntBShowCrit[1] + IntY, COLOR_SPRITE);
    }
    if (Settings.ShowTile) {
        SprMngr.DrawSprite(IntPShow.get(), IntBShowTile[0] + IntX, IntBShowTile[1] + IntY, COLOR_SPRITE);
    }
    if (Settings.ShowRoof) {
        SprMngr.DrawSprite(IntPShow.get(), IntBShowRoof[0] + IntX, IntBShowRoof[1] + IntY, COLOR_SPRITE);
    }
    if (Settings.ShowFast) {
        SprMngr.DrawSprite(IntPShow.get(), IntBShowFast[0] + IntX, IntBShowFast[1] + IntY, COLOR_SPRITE);
    }

    if (IsSelectItem) {
        SprMngr.DrawSprite(IntPSelect.get(), IntBSelectItem[0] + IntX, IntBSelectItem[1] + IntY, COLOR_SPRITE);
    }
    if (IsSelectScen) {
        SprMngr.DrawSprite(IntPSelect.get(), IntBSelectScen[0] + IntX, IntBSelectScen[1] + IntY, COLOR_SPRITE);
    }
    if (IsSelectWall) {
        SprMngr.DrawSprite(IntPSelect.get(), IntBSelectWall[0] + IntX, IntBSelectWall[1] + IntY, COLOR_SPRITE);
    }
    if (IsSelectCrit) {
        SprMngr.DrawSprite(IntPSelect.get(), IntBSelectCrit[0] + IntX, IntBSelectCrit[1] + IntY, COLOR_SPRITE);
    }
    if (IsSelectTile) {
        SprMngr.DrawSprite(IntPSelect.get(), IntBSelectTile[0] + IntX, IntBSelectTile[1] + IntY, COLOR_SPRITE);
    }
    if (IsSelectRoof) {
        SprMngr.DrawSprite(IntPSelect.get(), IntBSelectRoof[0] + IntX, IntBSelectRoof[1] + IntY, COLOR_SPRITE);
    }

    auto x = IntWWork[0] + IntX;
    auto y = IntWWork[1] + IntY;
    auto h = IntWWork[3] - IntWWork[1];
    int w = ProtoWidth;

    if (IsItemMode()) {
        auto i = *CurProtoScroll;
        int j = static_cast<int>(static_cast<size_t>(i) + ProtosOnScreen);
        if (j > static_cast<int>((*CurItemProtos).size())) {
            j = static_cast<int>((*CurItemProtos).size());
        }

        for (; i < j; i++, x += w) {
            const auto* proto_item = (*CurItemProtos)[i];
            auto col = (i == static_cast<int>(GetTabIndex()) ? COLOR_SPRITE_RED : COLOR_SPRITE);
            if (const auto* spr = GetIfaceSpr(proto_item->GetPicMap()); spr != nullptr) {
                SprMngr.DrawSpriteSize(spr, x, y, w, h / 2, false, true, col);
            }

            if (proto_item->GetPicInv()) {
                const auto* spr = GetIfaceSpr(proto_item->GetPicInv());
                if (spr != nullptr) {
                    SprMngr.DrawSpriteSize(spr, x, y + h / 2, w, h / 2, false, true, col);
                }
            }

            SprMngr.DrawStr(IRect(x, y + h - 15, x + w, y + h), proto_item->GetName(), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
        }

        if (GetTabIndex() < static_cast<uint>((*CurItemProtos).size())) {
            const auto* proto_item = (*CurItemProtos)[GetTabIndex()];

            SprMngr.DrawStr(IRect(IntWHint, IntX, IntY), proto_item->GetName(), 0, COLOR_TEXT, FONT_DEFAULT);
        }
    }
    else if (IsCritMode()) {
        uint i = *CurProtoScroll;
        auto j = i + ProtosOnScreen;
        if (j > CurNpcProtos->size()) {
            j = static_cast<uint>(CurNpcProtos->size());
        }

        for (; i < j; i++, x += w) {
            const auto* proto = (*CurNpcProtos)[i];

            auto model_name = proto->GetModelName();
            const auto* anim = ResMngr.GetCritterPreviewSpr(model_name, CritterStateAnim::Unarmed, CritterActionAnim::Idle, NpcDir, nullptr); // &proto->Params[ ST_ANIM3D_LAYER_BEGIN ] );
            if (anim == nullptr) {
                continue;
            }

            auto col = COLOR_SPRITE;
            if (i == GetTabIndex()) {
                col = COLOR_SPRITE_RED;
            }

            SprMngr.DrawSpriteSize(anim, x, y, w, h / 2, false, true, col);
            SprMngr.DrawStr(IRect(x, y + h - 15, x + w, y + h), proto->GetName(), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
        }

        if (GetTabIndex() < CurNpcProtos->size()) {
            const auto* proto = (*CurNpcProtos)[GetTabIndex()];
            SprMngr.DrawStr(IRect(IntWHint, IntX, IntY), proto->GetName(), 0, COLOR_TEXT, FONT_DEFAULT);
        }
    }
    else if (IntMode == INT_MODE_INCONT && !SelectedEntities.empty()) {
        auto* entity = SelectedEntities[0];
        vector<ItemView*> inner_items = GetEntityInnerItems(entity);

        uint i = InContScroll;
        auto j = i + ProtosOnScreen;
        if (j > inner_items.size()) {
            j = static_cast<uint>(inner_items.size());
        }

        for (; i < j; i++, x += w) {
            auto* inner_item = inner_items[i];
            RUNTIME_ASSERT(inner_item);

            auto* spr = GetIfaceSpr(inner_item->GetPicInv());
            if (spr == nullptr) {
                continue;
            }

            auto col = COLOR_SPRITE;
            if (inner_item == InContItem) {
                col = COLOR_SPRITE_RED;
            }

            SprMngr.DrawSpriteSize(spr, x, y, w, h, false, true, col);

            SprMngr.DrawStr(IRect(x, y + h - 15, x + w, y + h), _str("x{}", inner_item->GetCount()), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
            if (inner_item->GetOwnership() == ItemOwnership::CritterInventory && inner_item->GetCritterSlot() != CritterItemSlot::Inventory) {
                SprMngr.DrawStr(IRect(x, y, x + w, y + h), _str("Slot {}", inner_item->GetCritterSlot()), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
            }
        }
    }
    else if (IntMode == INT_MODE_LIST) {
        auto i = ListScroll;
        auto j = static_cast<int>(LoadedMaps.size());

        for (; i < j; i++, x += w) {
            auto* map = LoadedMaps[i];
            SprMngr.DrawStr(IRect(x, y, x + w, y + h), _str(" '{}'", map->GetName()), 0, map == CurMap ? COLOR_SPRITE_RED : COLOR_TEXT, FONT_DEFAULT);
        }
    }

    // Message box
    if (IntMode == INT_MODE_MESS) {
        MessBoxDraw();
    }

    // Sub tabs
    if (SubTabsActive) {
        SprMngr.DrawSprite(SubTabsPic.get(), SubTabsX, SubTabsY, COLOR_SPRITE);

        auto line_height = SprMngr.GetLineHeight(FONT_DEFAULT) + 1;
        auto posy = SubTabsRect.Height() - line_height - 2;
        auto i = 0;
        auto& stabs = Tabs[SubTabsActiveTab];
        for (auto&& [fst, snd] : stabs) {
            i++;
            if (i - 1 < TabsScroll[SubTabsActiveTab]) {
                continue;
            }

            auto name = fst;
            auto& stab = snd;

            auto color = (TabsActive[SubTabsActiveTab] == &stab ? COLOR_TEXT_WHITE : COLOR_TEXT);
            auto r = IRect(SubTabsRect.Left + SubTabsX + 5, SubTabsRect.Top + SubTabsY + posy, SubTabsRect.Left + SubTabsX + 5 + Settings.ScreenWidth, SubTabsRect.Top + SubTabsY + posy + line_height - 1);
            if (IsCurInRect(r)) {
                color = COLOR_TEXT_DWHITE;
            }

            auto count = static_cast<uint>(stab.NpcProtos.size());
            if (count == 0u) {
                count = static_cast<uint>(stab.ItemProtos.size());
            }
            name += _str(" ({})", count);
            SprMngr.DrawStr(r, name, 0, color, FONT_DEFAULT);

            posy -= line_height;
            if (posy < 0) {
                break;
            }
        }
    }

    // Map info
    if (CurMap != nullptr) {
        auto hex_thru = false;
        uint16 hx = 0;
        uint16 hy = 0;
        if (CurMap->GetHexAtScreenPos(Settings.MouseX, Settings.MouseY, hx, hy, nullptr, nullptr)) {
            hex_thru = true;
        }
        auto day_time = CurMap->GetGlobalDayTime();
        SprMngr.DrawStr(IRect(Settings.ScreenWidth - 100, 0, Settings.ScreenWidth, Settings.ScreenHeight),
            _str("Map '{}'\n"
                 "Hex {} {}\n"
                 "Time {} : {}\n"
                 "Fps {}\n"
                 "Tile layer {}\n"
                 "{}",
                CurMap->GetName(), hex_thru ? hx : -1, hex_thru ? hy : -1, day_time / 60 % 24, day_time % 60, Settings.FPS, TileLayer, Settings.ScrollCheck ? "Scroll check" : ""),
            FT_NOBREAK_LINE, COLOR_TEXT, FONT_DEFAULT);
    }
}

void FOMapper::ObjDraw()
{
    STACK_TRACE_ENTRY();

    if (!ObjVisible) {
        return;
    }

    auto* entity = GetInspectorEntity();
    if (entity == nullptr) {
        return;
    }

    const auto* item = dynamic_cast<ItemView*>(entity);
    const auto* cr = dynamic_cast<CritterView*>(entity);
    auto r = IRect(ObjWWork, ObjX, ObjY);
    const auto x = r.Left;
    const auto y = r.Top;
    const auto w = r.Width();

    SprMngr.DrawSprite(ObjWMainPic.get(), ObjX, ObjY, COLOR_SPRITE);
    if (ObjToAll) {
        SprMngr.DrawSprite(ObjPbToAllDn.get(), ObjBToAll[0] + ObjX, ObjBToAll[1] + ObjY, COLOR_SPRITE);
    }

    if (item != nullptr) {
        const auto* spr = GetIfaceSpr(item->GetPicMap());
        if (spr == nullptr) {
            spr = ResMngr.GetItemDefaultSpr().get();
        }

        SprMngr.DrawSpriteSize(spr, x + w - ProtoWidth, y, ProtoWidth, ProtoWidth, false, true, COLOR_SPRITE);

        if (item->GetPicInv()) {
            const auto* inv_spr = GetIfaceSpr(item->GetPicInv());
            if (inv_spr != nullptr) {
                SprMngr.DrawSpriteSize(inv_spr, x + w - ProtoWidth, y + ProtoWidth, ProtoWidth, ProtoWidth, false, true, COLOR_SPRITE);
            }
        }
    }

    DrawLine("Id", "", _str("{}", entity->GetId()), true, r);
    DrawLine("ProtoName", "", entity->GetName(), true, r);
    if (cr != nullptr) {
        DrawLine("Type", "", "Critter", true, r);
    }
    else if (item != nullptr && !item->GetIsStatic()) {
        DrawLine("Type", "", "Dynamic Item", true, r);
    }
    else if (item != nullptr && item->GetIsStatic()) {
        DrawLine("Type", "", "Static Item", true, r);
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }

    for (const auto* prop : ShowProps) {
        if (prop != nullptr) {
            auto value = entity->GetProperties().SavePropertyToText(prop);
            DrawLine(prop->GetName(), prop->GetFullTypeName(), value, prop->IsReadOnly(), r);
        }
        else {
            r.Top += DRAW_NEXT_HEIGHT;
            r.Bottom += DRAW_NEXT_HEIGHT;
        }
    }
}

void FOMapper::DrawLine(string_view name, string_view type_name, string_view text, bool is_const, IRect& r)
{
    STACK_TRACE_ENTRY();

    const auto x = r.Left;
    const auto y = r.Top;
    const auto w = r.Width();
    const auto h = r.Height();

    auto color = COLOR_TEXT;
    if (is_const) {
        color = COLOR_TEXT_DWHITE;
    }

    auto result_text = text;
    if (ObjCurLine == (y - ObjWWork[1] - ObjY) / DRAW_NEXT_HEIGHT) {
        color = COLOR_TEXT_WHITE;
        if (!is_const && ObjCurLineValue != ObjCurLineInitValue) {
            color = COLOR_TEXT_RED;
            result_text = ObjCurLineValue;
        }
    }

    string str = _str("{}{}{}{}", name, !type_name.empty() ? " (" : "", !type_name.empty() ? type_name : "", !type_name.empty() ? ")" : "");
    str += "........................................................................................................";
    SprMngr.DrawStr(IRect(IRect(x, y, x + w / 2, y + h), 0, 0), str, FT_NOBREAK, color, FONT_DEFAULT);
    SprMngr.DrawStr(IRect(IRect(x + w / 2, y, x + w, y + h), 0, 0), result_text, FT_NOBREAK, color, FONT_DEFAULT);

    r.Top += DRAW_NEXT_HEIGHT;
    r.Bottom += DRAW_NEXT_HEIGHT;
}

void FOMapper::ObjKeyDown(KeyCode dik, string_view dik_text)
{
    STACK_TRACE_ENTRY();

    if (dik == KeyCode::Return || dik == KeyCode::Numpadenter) {
        if (ObjCurLineInitValue != ObjCurLineValue) {
            auto* entity = GetInspectorEntity();
            RUNTIME_ASSERT(entity);
            ObjKeyDownApply(entity);

            if (!SelectedEntities.empty() && SelectedEntities[0] == entity && ObjToAll) {
                for (size_t i = 1; i < SelectedEntities.size(); i++) {
                    const auto is_same1 = dynamic_cast<CritterView*>(SelectedEntities[i]) != nullptr && dynamic_cast<CritterView*>(entity) != nullptr;
                    const auto is_same2 = dynamic_cast<ItemView*>(SelectedEntities[i]) != nullptr && dynamic_cast<ItemView*>(entity) != nullptr;
                    if (is_same1 || is_same2) {
                        ObjKeyDownApply(SelectedEntities[i]);
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
        ObjCurLineValue = ObjCurLineInitValue;
    }
    else {
        if (!ObjCurLineIsConst) {
            Keyb.FillChar(dik, dik_text, ObjCurLineValue, nullptr, KIF_NO_SPEC_SYMBOLS);
        }
    }
}

void FOMapper::ObjKeyDownApply(Entity* entity)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    constexpr auto start_line = 3;

    if (ObjCurLine >= start_line && ObjCurLine - start_line < static_cast<int>(ShowProps.size())) {
        const auto* prop = ShowProps[ObjCurLine - start_line];
        if (prop != nullptr) {
            if (entity->GetPropertiesForEdit().ApplyPropertyFromText(prop, ObjCurLineValue)) {
                if (auto* hex_item = dynamic_cast<ItemHexView*>(entity); hex_item != nullptr) {
                    if (prop == hex_item->GetPropertyOffsetX() || prop == hex_item->GetPropertyOffsetY()) {
                        hex_item->RefreshOffs();
                    }
                    if (prop == hex_item->GetPropertyPicMap()) {
                        hex_item->RefreshAnim();
                    }
                }
            }
            else {
                const auto r = entity->GetPropertiesForEdit().ApplyPropertyFromText(prop, ObjCurLineInitValue);
                UNUSED_VARIABLE(r);
            }
        }
    }
}

void FOMapper::SelectEntityProp(int line)
{
    STACK_TRACE_ENTRY();

    constexpr auto start_line = 3;

    ObjCurLine = line;
    if (ObjCurLine < 0) {
        ObjCurLine = 0;
    }
    ObjCurLineInitValue = ObjCurLineValue = "";
    ObjCurLineIsConst = true;

    if (const auto* entity = GetInspectorEntity(); entity != nullptr) {
        if (ObjCurLine - start_line >= static_cast<int>(ShowProps.size())) {
            ObjCurLine = static_cast<int>(ShowProps.size()) + start_line - 1;
        }
        if (ObjCurLine >= start_line && ObjCurLine - start_line < static_cast<int>(ShowProps.size()) && (ShowProps[ObjCurLine - start_line] != nullptr)) {
            ObjCurLineInitValue = ObjCurLineValue = entity->GetProperties().SavePropertyToText(ShowProps[ObjCurLine - start_line]);
            ObjCurLineIsConst = ShowProps[ObjCurLine - start_line]->IsReadOnly();
        }
    }
}

auto FOMapper::GetInspectorEntity() -> ClientEntity*
{
    STACK_TRACE_ENTRY();

    auto* entity = (IntMode == INT_MODE_INCONT && (InContItem != nullptr) ? InContItem : (!SelectedEntities.empty() ? SelectedEntities[0] : nullptr));
    if (entity == InspectorEntity) {
        return entity;
    }

    InspectorEntity = entity;
    ShowProps.clear();

    if (entity != nullptr) {
        vector<int> prop_indices;
        OnInspectorProperties.Fire(entity, prop_indices);
        for (const auto prop_index : prop_indices) {
            ShowProps.push_back(prop_index != -1 ? entity->GetProperties().GetRegistrator()->GetByIndex(prop_index) : nullptr);
        }
    }

    SelectEntityProp(ObjCurLine);
    return entity;
}

void FOMapper::IntLMouseDown()
{
    STACK_TRACE_ENTRY();

    IntHold = INT_NONE;

    // Sub tabs
    if (IntVisible && SubTabsActive) {
        if (IsCurInRect(SubTabsRect, SubTabsX, SubTabsY)) {
            const auto line_height = SprMngr.GetLineHeight(FONT_DEFAULT) + 1;
            auto posy = SubTabsRect.Height() - line_height - 2;
            auto i = 0;
            auto& stabs = Tabs[SubTabsActiveTab];
            for (auto&& [fst, snd] : stabs) {
                i++;
                if (i - 1 < TabsScroll[SubTabsActiveTab]) {
                    continue;
                }

                auto& stab = snd;

                auto r = IRect(SubTabsRect.Left + SubTabsX + 5, SubTabsRect.Top + SubTabsY + posy, SubTabsRect.Left + SubTabsX + 5 + SubTabsRect.Width(), SubTabsRect.Top + SubTabsY + posy + line_height - 1);
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

        if (CurMap == nullptr) {
            return;
        }

        if (!CurMap->GetHexAtScreenPos(Settings.MouseX, Settings.MouseY, SelectHexX1, SelectHexY1, nullptr, nullptr)) {
            return;
        }

        SelectHexX2 = SelectHexX1;
        SelectHexY2 = SelectHexY1;
        SelectX = Settings.MouseX;
        SelectY = Settings.MouseY;

        if (CurMode == CUR_MODE_DEFAULT) {
            if (Keyb.ShiftDwn) {
                for (auto* entity : SelectedEntities) {
                    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
                        auto hx = cr->GetHexX();
                        auto hy = cr->GetHexY();

                        if (const auto find_path = CurMap->FindPath(nullptr, hx, hy, SelectHexX1, SelectHexY1, -1)) {
                            for (const auto step : find_path->Steps) {
                                if (GeometryHelper::MoveHexByDir(hx, hy, step, CurMap->GetWidth(), CurMap->GetHeight())) {
                                    CurMap->MoveCritter(cr, hx, hy, true);
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
                CreateItem((*CurItemProtos)[GetTabIndex()]->GetProtoId(), SelectHexX1, SelectHexY1, nullptr);
            }
            else if (IsCritMode() && !CurNpcProtos->empty()) {
                CreateCritter((*CurNpcProtos)[GetTabIndex()]->GetProtoId(), SelectHexX1, SelectHexY1);
            }
        }

        return;
    }

    // Object editor
    if (ObjVisible && !SelectedEntities.empty() && IsCurInRect(ObjWMain, ObjX, ObjY)) {
        if (IsCurInRect(ObjWWork, ObjX, ObjY)) {
            SelectEntityProp((Settings.MouseY - ObjY - ObjWWork[1]) / DRAW_NEXT_HEIGHT);
        }

        if (IsCurInRect(ObjBToAll, ObjX, ObjY)) {
            ObjToAll = !ObjToAll;
            IntHold = INT_BUTTON;
            return;
        }
        if (!ObjFix) {
            IntHold = INT_OBJECT;
            ItemVectX = Settings.MouseX - ObjX;
            ItemVectY = Settings.MouseY - ObjY;
        }

        return;
    }

    // Interface
    if (!IntVisible || !IsCurInRect(IntWMain, IntX, IntY)) {
        return;
    }

    if (IsCurInRect(IntWWork, IntX, IntY)) {
        int ind = (Settings.MouseX - IntX - IntWWork[0]) / ProtoWidth;

        if (IsItemMode() && !CurItemProtos->empty()) {
            ind += *CurProtoScroll;
            if (ind >= static_cast<int>((*CurItemProtos).size())) {
                ind = static_cast<int>((*CurItemProtos).size()) - 1;
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
                    stab.ItemProtos.push_back((*CurItemProtos)[ind]);
                }

                CurMap->SwitchIgnorePid(pid);
                CurMap->RefreshMap();
            }
            // Add to container
            else if (Keyb.AltDwn && !SelectedEntities.empty()) {
                auto add = true;
                const auto* proto_item = (*CurItemProtos)[ind];

                if (proto_item->GetStackable()) {
                    for (const auto* child : GetEntityInnerItems(SelectedEntities[0])) {
                        if (proto_item->GetProtoId() == child->GetProtoId()) {
                            add = false;
                            break;
                        }
                    }
                }

                if (add) {
                    CreateItem(proto_item->GetProtoId(), 0, 0, SelectedEntities[0]);
                }
            }
        }
        else if (IsCritMode() && !CurNpcProtos->empty()) {
            ind += *CurProtoScroll;
            if (ind >= static_cast<int>(CurNpcProtos->size())) {
                ind = static_cast<int>(CurNpcProtos->size()) - 1;
            }
            SetTabIndex(ind);
        }
        else if (IntMode == INT_MODE_INCONT) {
            InContItem = nullptr;
            ind += InContScroll;

            vector<ItemView*> inner_items;
            if (!SelectedEntities.empty()) {
                inner_items = GetEntityInnerItems(SelectedEntities[0]);
            }

            if (!inner_items.empty()) {
                if (ind < static_cast<int>(inner_items.size())) {
                    InContItem = inner_items[ind];
                }

                if (Keyb.AltDwn && InContItem != nullptr) {
                    // Delete child
                    if (InContItem->GetOwnership() == ItemOwnership::CritterInventory) {
                        auto* owner = CurMap->GetCritter(InContItem->GetCritterId());
                        RUNTIME_ASSERT(owner);
                        owner->DeleteInvItem(InContItem, true);
                    }
                    else if (InContItem->GetOwnership() == ItemOwnership::ItemContainer) {
                        ItemView* owner = CurMap->GetItem(InContItem->GetContainerId());
                        RUNTIME_ASSERT(owner);
                        owner->DestroyInnerItem(InContItem);
                    }
                    else {
                        throw UnreachablePlaceException(LINE_STR);
                    }
                    InContItem = nullptr;

                    // Reselect
                    auto* tmp = SelectedEntities[0];
                    SelectClear();
                    SelectAdd(tmp);
                }
                else if (Keyb.ShiftDwn && InContItem != nullptr) {
                    // Change child slot
                    if (auto* cr = dynamic_cast<CritterHexView*>(SelectedEntities[0]); cr != nullptr) {
                        auto to_slot = static_cast<int>(InContItem->GetCritterSlot()) + 1;
                        while (static_cast<size_t>(to_slot) >= Settings.CritterSlotEnabled.size() || !Settings.CritterSlotEnabled[to_slot % 256]) {
                            to_slot++;
                        }
                        to_slot %= 256;

                        for (auto* item : cr->GetInvItems()) {
                            if (item->GetCritterSlot() == static_cast<CritterItemSlot>(to_slot)) {
                                item->SetCritterSlot(CritterItemSlot::Inventory);
                            }
                        }

                        InContItem->SetCritterSlot(static_cast<CritterItemSlot>(to_slot));

                        cr->AnimateStay();
                    }
                }
            }
        }
        else if (IntMode == INT_MODE_LIST) {
            ind += ListScroll;

            if (ind < static_cast<int>(LoadedMaps.size()) && CurMap != LoadedMaps[ind]) {
                ShowMap(LoadedMaps[ind]);
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
            if (*CurProtoScroll < 0) {
                *CurProtoScroll = 0;
            }
        }
        else if (IntMode == INT_MODE_INCONT) {
            InContScroll--;
            if (InContScroll < 0) {
                InContScroll = 0;
            }
        }
        else if (IntMode == INT_MODE_LIST) {
            ListScroll--;
            if (ListScroll < 0) {
                ListScroll = 0;
            }
        }
    }
    else if (IsCurInRect(IntBScrBackFst, IntX, IntY)) {
        if (IsItemMode() || IsCritMode()) {
            (*CurProtoScroll) -= static_cast<int>(ProtosOnScreen);
            if (*CurProtoScroll < 0) {
                *CurProtoScroll = 0;
            }
        }
        else if (IntMode == INT_MODE_INCONT) {
            InContScroll -= static_cast<int>(ProtosOnScreen);
            if (InContScroll < 0) {
                InContScroll = 0;
            }
        }
        else if (IntMode == INT_MODE_LIST) {
            ListScroll -= static_cast<int>(ProtosOnScreen);
            if (ListScroll < 0) {
                ListScroll = 0;
            }
        }
    }
    else if (IsCurInRect(IntBScrFront, IntX, IntY)) {
        if (IsItemMode() && !(*CurItemProtos).empty()) {
            (*CurProtoScroll)++;
            if (*CurProtoScroll >= static_cast<int>((*CurItemProtos).size())) {
                *CurProtoScroll = static_cast<int>((*CurItemProtos).size()) - 1;
            }
        }
        else if (IsCritMode() && !CurNpcProtos->empty()) {
            (*CurProtoScroll)++;
            if (*CurProtoScroll >= static_cast<int>(CurNpcProtos->size())) {
                *CurProtoScroll = static_cast<int>(CurNpcProtos->size()) - 1;
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
        if (IsItemMode() && !(*CurItemProtos).empty()) {
            (*CurProtoScroll) += static_cast<int>(ProtosOnScreen);
            if (*CurProtoScroll >= static_cast<int>((*CurItemProtos).size())) {
                *CurProtoScroll = static_cast<int>((*CurItemProtos).size()) - 1;
            }
        }
        else if (IsCritMode() && !CurNpcProtos->empty()) {
            (*CurProtoScroll) += static_cast<int>(ProtosOnScreen);
            if (*CurProtoScroll >= static_cast<int>(CurNpcProtos->size())) {
                *CurProtoScroll = static_cast<int>(CurNpcProtos->size()) - 1;
            }
        }
        else if (IntMode == INT_MODE_INCONT) {
            InContScroll += static_cast<int>(ProtosOnScreen);
        }
        else if (IntMode == INT_MODE_LIST) {
            ListScroll += static_cast<int>(ProtosOnScreen);
        }
    }
    else if (IsCurInRect(IntBShowItem, IntX, IntY)) {
        Settings.ShowItem = !Settings.ShowItem;
        CurMap->RefreshMap();
    }
    else if (IsCurInRect(IntBShowScen, IntX, IntY)) {
        Settings.ShowScen = !Settings.ShowScen;
        CurMap->RefreshMap();
    }
    else if (IsCurInRect(IntBShowWall, IntX, IntY)) {
        Settings.ShowWall = !Settings.ShowWall;
        CurMap->RefreshMap();
    }
    else if (IsCurInRect(IntBShowCrit, IntX, IntY)) {
        Settings.ShowCrit = !Settings.ShowCrit;
        CurMap->RefreshMap();
    }
    else if (IsCurInRect(IntBShowTile, IntX, IntY)) {
        Settings.ShowTile = !Settings.ShowTile;
        CurMap->RefreshMap();
    }
    else if (IsCurInRect(IntBShowRoof, IntX, IntY)) {
        Settings.ShowRoof = !Settings.ShowRoof;
        CurMap->RefreshMap();
    }
    else if (IsCurInRect(IntBShowFast, IntX, IntY)) {
        Settings.ShowFast = !Settings.ShowFast;
        CurMap->RefreshMap();
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
        IntVectX = Settings.MouseX - IntX;
        IntVectY = Settings.MouseY - IntY;
        return;
    }
    else {
        return;
    }

    IntHold = INT_BUTTON;
}

void FOMapper::IntLMouseUp()
{
    STACK_TRACE_ENTRY();

    if (IntHold == INT_SELECT && CurMap->GetHexAtScreenPos(Settings.MouseX, Settings.MouseY, SelectHexX2, SelectHexY2, nullptr, nullptr)) {
        if (CurMode == CUR_MODE_DEFAULT) {
            if (SelectHexX1 != SelectHexX2 || SelectHexY1 != SelectHexY2) {
                CurMap->ClearHexTrack();
                vector<pair<uint16, uint16>> hexes;

                if (SelectType == SELECT_TYPE_OLD) {
                    const int fx = std::min(SelectHexX1, SelectHexX2);
                    const int tx = std::max(SelectHexX1, SelectHexX2);
                    const int fy = std::min(SelectHexY1, SelectHexY2);
                    const int ty = std::max(SelectHexY1, SelectHexY2);

                    for (auto i = fx; i <= tx; i++) {
                        for (auto j = fy; j <= ty; j++) {
                            hexes.emplace_back(i, j);
                        }
                    }
                }
                else {
                    hexes = CurMap->GetHexesRect(IRect(SelectHexX1, SelectHexY1, SelectHexX2, SelectHexY2));
                }

                vector<ItemHexView*> items;
                vector<CritterHexView*> critters;
                for (auto&& [hx, hy] : hexes) {
                    // Items, critters
                    auto&& hex_items = CurMap->GetItems(hx, hy);
                    items.insert(items.end(), hex_items.begin(), hex_items.end());

                    auto&& hex_critters = CurMap->GetCritters(hx, hy, CritterFindType::Any);
                    critters.insert(critters.end(), hex_critters.begin(), hex_critters.end());

                    // Tile, roof
                    if (IsSelectTile && Settings.ShowTile) {
                        SelectAddTile(hx, hy, false);
                    }
                    if (IsSelectRoof && Settings.ShowRoof) {
                        SelectAddTile(hx, hy, true);
                    }
                }

                for (auto* item : items) {
                    const auto pid = item->GetProtoId();
                    if (CurMap->IsIgnorePid(pid)) {
                        continue;
                    }
                    if (!Settings.ShowFast && CurMap->IsFastPid(pid)) {
                        continue;
                    }

                    if (!item->GetIsScenery() && !item->GetIsWall() && IsSelectItem && Settings.ShowItem) {
                        SelectAddItem(item);
                    }
                    else if (item->GetIsScenery() && IsSelectScen && Settings.ShowScen) {
                        SelectAddItem(item);
                    }
                    else if (item->GetIsWall() && IsSelectWall && Settings.ShowWall) {
                        SelectAddItem(item);
                    }
                    else if (Settings.ShowFast && CurMap->IsFastPid(pid)) {
                        SelectAddItem(item);
                    }
                }

                for (auto* cr : critters) {
                    if (IsSelectCrit && Settings.ShowCrit) {
                        SelectAddCrit(cr);
                    }
                }
            }
            else {
                auto* entity = CurMap->GetEntityAtScreenPos(Settings.MouseX, Settings.MouseY, 0, true);

                if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
                    if (!CurMap->IsIgnorePid(item->GetProtoId())) {
                        SelectAddItem(item);
                    }
                }
                else if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
                    SelectAddCrit(cr);
                }
            }

            // Crits or item container
            if (!SelectedEntities.empty() && !GetEntityInnerItems(SelectedEntities[0]).empty()) {
                IntSetMode(INT_MODE_INCONT);
            }
        }

        CurMap->RefreshMap();
    }

    IntHold = INT_NONE;
}

void FOMapper::IntMouseMove()
{
    STACK_TRACE_ENTRY();

    if (IntHold == INT_SELECT) {
        CurMap->ClearHexTrack();
        if (!CurMap->GetHexAtScreenPos(Settings.MouseX, Settings.MouseY, SelectHexX2, SelectHexY2, nullptr, nullptr)) {
            if (SelectHexX2 != 0u || SelectHexY2 != 0u) {
                CurMap->RefreshMap();
                SelectHexX2 = SelectHexY2 = 0;
            }
            return;
        }

        if (CurMode == CUR_MODE_DEFAULT) {
            if (SelectHexX1 != SelectHexX2 || SelectHexY1 != SelectHexY2) {
                if (SelectType == SELECT_TYPE_OLD) {
                    const int fx = std::min(SelectHexX1, SelectHexX2);
                    const int tx = std::max(SelectHexX1, SelectHexX2);
                    const int fy = std::min(SelectHexY1, SelectHexY2);
                    const int ty = std::max(SelectHexY1, SelectHexY2);

                    for (auto i = fx; i <= tx; i++) {
                        for (auto j = fy; j <= ty; j++) {
                            CurMap->GetHexTrack(static_cast<uint16>(i), static_cast<uint16>(j)) = 1;
                        }
                    }
                }
                else if (SelectType == SELECT_TYPE_NEW) {
                    for (auto&& [hx, hy] : CurMap->GetHexesRect(IRect(SelectHexX1, SelectHexY1, SelectHexX2, SelectHexY2))) {
                        CurMap->GetHexTrack(hx, hy) = 1;
                    }
                }

                CurMap->RefreshMap();
            }
        }
        else if (CurMode == CUR_MODE_MOVE_SELECTION) {
            auto offs_hx = static_cast<int>(SelectHexX2) - static_cast<int>(SelectHexX1);
            auto offs_hy = static_cast<int>(SelectHexY2) - static_cast<int>(SelectHexY1);
            auto offs_x = Settings.MouseX - SelectX;
            auto offs_y = Settings.MouseY - SelectY;
            if (SelectMove(!Keyb.ShiftDwn, offs_hx, offs_hy, offs_x, offs_y)) {
                SelectHexX1 = static_cast<uint16>(SelectHexX1 + offs_hx);
                SelectHexY1 = static_cast<uint16>(SelectHexY1 + offs_hy);
                SelectX += offs_x;
                SelectY += offs_y;
                CurMap->RefreshMap();
            }
        }
    }
    else if (IntHold == INT_MAIN) {
        IntX = Settings.MouseX - IntVectX;
        IntY = Settings.MouseY - IntVectY;
    }
    else if (IntHold == INT_OBJECT) {
        ObjX = Settings.MouseX - ItemVectX;
        ObjY = Settings.MouseY - ItemVectY;
    }
}

auto FOMapper::GetTabIndex() const -> uint
{
    STACK_TRACE_ENTRY();

    if (IntMode < TAB_COUNT) {
        return TabsActive[IntMode]->Index;
    }
    return TabIndex[IntMode];
}

void FOMapper::SetTabIndex(uint index)
{
    STACK_TRACE_ENTRY();

    if (IntMode < TAB_COUNT) {
        TabsActive[IntMode]->Index = index;
    }
    TabIndex[IntMode] = index;
}

void FOMapper::RefreshCurProtos()
{
    STACK_TRACE_ENTRY();

    // Select protos and scroll
    CurItemProtos = nullptr;
    CurProtoScroll = nullptr;
    CurNpcProtos = nullptr;
    InContItem = nullptr;

    if (IntMode >= 0 && IntMode < TAB_COUNT) {
        auto* stab = TabsActive[IntMode];
        if (!stab->NpcProtos.empty()) {
            CurNpcProtos = &stab->NpcProtos;
        }
        else {
            CurItemProtos = &stab->ItemProtos;
        }
        CurProtoScroll = &stab->Scroll;
    }

    if (IntMode == INT_MODE_INCONT) {
        InContScroll = 0;
    }

    if (CurMap != nullptr) {
        // Update fast pids
        CurMap->ClearFastPids();
        for (const auto* fast_proto : TabsActive[INT_MODE_FAST]->ItemProtos) {
            CurMap->AddFastPid(fast_proto->GetProtoId());
        }

        // Update ignore pids
        CurMap->ClearIgnorePids();
        for (const auto* ignore_proto : TabsActive[INT_MODE_IGNORE]->ItemProtos) {
            CurMap->AddIgnorePid(ignore_proto->GetProtoId());
        }

        // Refresh map
        CurMap->RefreshMap();
    }
}

void FOMapper::IntSetMode(int mode)
{
    STACK_TRACE_ENTRY();

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
            SubTabsX = IntBCust[mode - INT_MODE_CUSTOM0].CenterX();
            SubTabsY = IntBCust[mode - INT_MODE_CUSTOM0].Top;
        }
        else if (mode == INT_MODE_ITEM) {
            SubTabsX = IntBItem.CenterX();
            SubTabsY = IntBItem.Top;
        }
        else if (mode == INT_MODE_TILE) {
            SubTabsX = IntBTile.CenterX();
            SubTabsY = IntBTile.Top;
        }
        else if (mode == INT_MODE_CRIT) {
            SubTabsX = IntBCrit.CenterX();
            SubTabsY = IntBCrit.Top;
        }
        else if (mode == INT_MODE_FAST) {
            SubTabsX = IntBFast.CenterX();
            SubTabsY = IntBFast.Top;
        }
        else if (mode == INT_MODE_IGNORE) {
            SubTabsX = IntBIgnore.CenterX();
            SubTabsY = IntBIgnore.Top;
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }

        SubTabsX += IntX - SubTabsRect.Width() / 2;
        SubTabsY += IntY - SubTabsRect.Height();

        if (SubTabsX < 0) {
            SubTabsX = 0;
        }
        if (SubTabsX + SubTabsRect.Width() > Settings.ScreenWidth) {
            SubTabsX -= SubTabsX + SubTabsRect.Width() - Settings.ScreenWidth;
        }

        if (SubTabsY < 0) {
            SubTabsY = 0;
        }
        if (SubTabsY + SubTabsRect.Height() > Settings.ScreenHeight) {
            SubTabsY -= SubTabsY + SubTabsRect.Height() - Settings.ScreenHeight;
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

void FOMapper::MoveEntity(ClientEntity* entity, uint16 hx, uint16 hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (hx >= CurMap->GetWidth() || hy >= CurMap->GetHeight()) {
        return;
    }

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        CurMap->MoveCritter(cr, hx, hy, false);
    }
    else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        CurMap->MoveItem(item, hx, hy);
    }
}

void FOMapper::DeleteEntity(ClientEntity* entity)
{
    STACK_TRACE_ENTRY();

    const auto it = std::find(SelectedEntities.begin(), SelectedEntities.end(), entity);
    if (it != SelectedEntities.end()) {
        SelectedEntities.erase(it);
    }

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        CurMap->DestroyCritter(cr);
    }
    else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        CurMap->DestroyItem(item);
    }
}

void FOMapper::SelectClear()
{
    STACK_TRACE_ENTRY();

    // Delete intersected tiles
    for (auto* entity : SelectedEntities) {
        if (const auto* tile = dynamic_cast<ItemHexView*>(entity); tile != nullptr && tile->GetIsTile()) {
            for (auto* sibling_tile : copy(CurMap->GetTiles(tile->GetHexX(), tile->GetHexY(), tile->GetIsRoofTile()))) {
                const auto is_sibling_selected = std::find(SelectedEntities.begin(), SelectedEntities.end(), sibling_tile) != SelectedEntities.end();
                if (!is_sibling_selected && sibling_tile->GetTileLayer() == tile->GetTileLayer()) {
                    CurMap->DestroyItem(sibling_tile);
                }
            }
        }
    }

    // Restore alpha
    for (auto* entity : SelectedEntities) {
        if (auto* hex_view = dynamic_cast<HexView*>(entity); hex_view != nullptr) {
            hex_view->RestoreAlpha();
        }
    }

    SelectedEntities.clear();
}

void FOMapper::SelectAddItem(ItemHexView* item)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item);
    SelectAdd(item);
}

void FOMapper::SelectAddCrit(CritterView* npc)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(npc);
    SelectAdd(npc);
}

void FOMapper::SelectAddTile(uint16 hx, uint16 hy, bool is_roof)
{
    STACK_TRACE_ENTRY();

    auto&& field = CurMap->GetField(hx, hy);
    auto&& tiles = is_roof ? field.RoofTiles : field.GroundTiles;

    for (auto* tile : tiles) {
        SelectAdd(tile);
    }
}

void FOMapper::SelectAdd(ClientEntity* entity)
{
    STACK_TRACE_ENTRY();

    const auto it = std::find(SelectedEntities.begin(), SelectedEntities.end(), entity);
    if (it == SelectedEntities.end()) {
        SelectedEntities.push_back(entity);

        if (auto* hex_view = dynamic_cast<HexView*>(entity); hex_view != nullptr) {
            hex_view->Alpha = SelectAlpha;
        }
    }
}

void FOMapper::SelectErase(ClientEntity* entity)
{
    STACK_TRACE_ENTRY();

    const auto it = std::find(SelectedEntities.begin(), SelectedEntities.end(), entity);
    if (it != SelectedEntities.end()) {
        SelectedEntities.erase(it);

        if (auto* hex_view = dynamic_cast<HexView*>(entity); hex_view != nullptr) {
            hex_view->RestoreAlpha();
        }
    }
}

void FOMapper::SelectAll()
{
    STACK_TRACE_ENTRY();

    SelectClear();

    for (auto* item : CurMap->GetItems()) {
        if (CurMap->IsIgnorePid(item->GetProtoId())) {
            continue;
        }

        if (!item->GetIsScenery() && !item->GetIsWall() && IsSelectItem && Settings.ShowItem) {
            SelectAddItem(item);
        }
        else if (item->GetIsScenery() && IsSelectScen && Settings.ShowScen) {
            SelectAddItem(item);
        }
        else if (item->GetIsWall() && IsSelectWall && Settings.ShowWall) {
            SelectAddItem(item);
        }
        else if (item->GetIsTile() && !item->GetIsRoofTile() && IsSelectTile && Settings.ShowTile) {
            SelectAddItem(item);
        }
        else if (item->GetIsTile() && item->GetIsRoofTile() && IsSelectRoof && Settings.ShowRoof) {
            SelectAddItem(item);
        }
    }

    if (IsSelectCrit && Settings.ShowCrit) {
        for (auto* cr : CurMap->GetCritters()) {
            SelectAddCrit(cr);
        }
    }

    CurMap->RefreshMap();
}

auto FOMapper::SelectMove(bool hex_move, int& offs_hx, int& offs_hy, int& offs_x, int& offs_y) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!hex_move && offs_x == 0 && offs_y == 0) {
        return false;
    }
    if (hex_move && offs_hx == 0 && offs_hy == 0) {
        return false;
    }

    // Tile step
    const auto have_tiles = std::find_if(SelectedEntities.begin(), SelectedEntities.end(), [](auto&& entity) {
        const auto* item = dynamic_cast<ItemHexView*>(entity);
        return item != nullptr && item->GetIsTile();
    }) != SelectedEntities.end();

    if (hex_move && have_tiles) {
        if (std::abs(offs_hx) < Settings.MapTileStep && std::abs(offs_hy) < Settings.MapTileStep) {
            return false;
        }

        offs_hx -= offs_hx % Settings.MapTileStep;
        offs_hy -= offs_hy % Settings.MapTileStep;
    }

    // Setup hex moving switcher
    int switcher = 0;

    if (!SelectedEntities.empty()) {
        if (const auto* cr = dynamic_cast<CritterHexView*>(SelectedEntities[0]); cr != nullptr) {
            switcher = std::abs(cr->GetHexX() % 2);
        }
        else if (const auto* item = dynamic_cast<ItemHexView*>(SelectedEntities[0]); item != nullptr) {
            switcher = std::abs(item->GetHexX() % 2);
        }
    }

    // Change moving speed on zooming
    if (!hex_move) {
        static auto small_ox = 0.0f;
        static auto small_oy = 0.0f;
        const auto ox = static_cast<float>(offs_x) * CurMap->GetSpritesZoom() + small_ox;
        const auto oy = static_cast<float>(offs_y) * CurMap->GetSpritesZoom() + small_oy;

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

        offs_x = static_cast<int>(ox);
        offs_y = static_cast<int>(oy);
    }
    else {
        for (auto* entity : SelectedEntities) {
            int hx = -1;
            int hy = -1;

            if (const auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
                hx = cr->GetHexX();
                hy = cr->GetHexY();
            }
            else if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
                hx = item->GetHexX();
                hy = item->GetHexY();
            }

            if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
                auto sw = switcher;
                for (auto k = 0; k < std::abs(offs_hx); k++, sw++) {
                    GeometryHelper::MoveHexByDirUnsafe(hx, hy, offs_hx > 0 ? ((sw % 2) != 0 ? 4u : 3u) : ((sw % 2) != 0 ? 0u : 1u));
                }
                for (auto k = 0; k < std::abs(offs_hy); k++) {
                    GeometryHelper::MoveHexByDirUnsafe(hx, hy, offs_hy > 0 ? 2u : 5u);
                }
            }
            else {
                hx += offs_hx;
                hy += offs_hy;
            }

            if (hx < 0 || hy < 0 || hx >= CurMap->GetWidth() || hy >= CurMap->GetHeight()) {
                return false; // Disable moving
            }
        }
    }

    // Move map objects
    for (auto* entity : SelectedEntities) {
        if (!hex_move) {
            auto* item = dynamic_cast<ItemHexView*>(entity);
            if (item == nullptr) {
                continue;
            }

            auto ox = item->GetOffsetX() + offs_x;
            auto oy = item->GetOffsetY() + offs_y;
            if (Keyb.AltDwn) {
                ox = oy = 0;
            }

            item->SetOffsetX(static_cast<int16>(ox));
            item->SetOffsetY(static_cast<int16>(oy));
            item->RefreshAnim();
        }
        else {
            int hx;
            int hy;

            if (const auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
                hx = cr->GetHexX();
                hy = cr->GetHexY();
            }
            else if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
                hx = item->GetHexX();
                hy = item->GetHexY();
            }

            if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
                auto sw = switcher;
                for (auto k = 0, l = std::abs(offs_hx); k < l; k++, sw++) {
                    GeometryHelper::MoveHexByDirUnsafe(hx, hy, offs_hx > 0 ? ((sw % 2) != 0 ? 4 : 3) : ((sw % 2) != 0 ? 0 : 1));
                }
                for (auto k = 0, l = std::abs(offs_hy); k < l; k++) {
                    GeometryHelper::MoveHexByDirUnsafe(hx, hy, offs_hy > 0 ? 2 : 5);
                }
            }
            else {
                hx += offs_hx;
                hy += offs_hy;
            }

            hx = std::clamp(hx, 0, CurMap->GetWidth() - 1);
            hy = std::clamp(hy, 0, CurMap->GetHeight() - 1);

            if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
                CurMap->MoveItem(item, static_cast<uint16>(hx), static_cast<uint16>(hy));
            }
            else if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
                CurMap->MoveCritter(cr, static_cast<uint16>(hx), static_cast<uint16>(hy), false);
            }
        }
    }

    // Restore alpha
    for (auto* entity : SelectedEntities) {
        if (auto* hex_view = dynamic_cast<HexView*>(entity); hex_view != nullptr) {
            hex_view->RestoreAlpha();
        }
    }

    return true;
}

void FOMapper::SelectDelete()
{
    STACK_TRACE_ENTRY();

    for (auto* entity : copy_hold_ref(SelectedEntities)) {
        DeleteEntity(entity);
    }

    SelectedEntities.clear();
    SelectClear();
    CurMap->RefreshMap();
    IntHold = INT_NONE;
    CurMode = CUR_MODE_DEFAULT;
}

auto FOMapper::CreateCritter(hstring pid, uint16 hx, uint16 hy) -> CritterView*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(CurMap);

    if (hx >= CurMap->GetWidth() || hy >= CurMap->GetHeight()) {
        return nullptr;
    }
    if (CurMap->GetNonDeadCritter(hx, hy) != nullptr) {
        return nullptr;
    }

    SelectClear();

    CritterHexView* cr = CurMap->AddMapperCritter(pid, hx, hy, GeometryHelper::DirToAngle(NpcDir), nullptr);

    SelectAdd(cr);

    CurMap->RefreshMap();
    CurMode = CUR_MODE_DEFAULT;

    return cr;
}

auto FOMapper::CreateItem(hstring pid, uint16 hx, uint16 hy, Entity* owner) -> ItemView*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(CurMap);

    // Checks
    const auto* proto = ProtoMngr.GetProtoItem(pid);

    if (owner == nullptr && (hx >= CurMap->GetWidth() || hy >= CurMap->GetHeight())) {
        return nullptr;
    }

    if (owner == nullptr) {
        SelectClear();
    }

    if (proto->GetIsTile()) {
        hx -= hx % Settings.MapTileStep;
        hy -= hy % Settings.MapTileStep;
    }

    ItemView* item = nullptr;

    if (owner != nullptr) {
        if (auto* cr = dynamic_cast<CritterHexView*>(owner); cr != nullptr) {
            item = cr->AddInvItem(cr->GetMap()->GetTempEntityId(), proto, CritterItemSlot::Inventory, {});
        }
        if (auto* cont = dynamic_cast<ItemHexView*>(owner); cont != nullptr) {
            item = cont->AddInnerItem(cont->GetMap()->GetTempEntityId(), proto, ContainerItemStack::Root, nullptr);
        }
    }
    else if (proto->GetIsTile()) {
        item = CurMap->AddMapperTile(proto->GetProtoId(), hx, hy, static_cast<uint8>(TileLayer), DrawRoof);
    }
    else {
        item = CurMap->AddMapperItem(proto->GetProtoId(), hx, hy, nullptr);
    }

    if (owner == nullptr) {
        SelectAdd(item);
        CurMode = CUR_MODE_DEFAULT;
    }
    else {
        IntSetMode(INT_MODE_INCONT);
        InContItem = item;
    }

    return item;
}

auto FOMapper::CloneEntity(Entity* entity) -> Entity*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(CurMap);

    if (const auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        auto* cr_clone = CurMap->AddMapperCritter(cr->GetProtoId(), cr->GetHexX(), cr->GetHexY(), cr->GetDirAngle(), &cr->GetProperties());

        for (const auto* inv_item : cr->GetConstInvItems()) {
            auto* inv_item_clone = cr_clone->AddInvItem(CurMap->GetTempEntityId(), static_cast<const ProtoItem*>(inv_item->GetProto()), inv_item->GetCritterSlot(), {});
            CloneInnerItems(inv_item_clone, inv_item);
        }

        SelectAdd(cr_clone);
        return cr_clone;
    }

    if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        auto* item_clone = CurMap->AddMapperItem(item->GetProtoId(), item->GetHexX(), item->GetHexY(), &item->GetProperties());

        CloneInnerItems(item_clone, item);

        SelectAdd(item_clone);
        return item_clone;
    }

    return nullptr;
}

void FOMapper::CloneInnerItems(ItemView* to_item, const ItemView* from_item)
{
    for (const auto* inner_item : from_item->GetConstInnerItems()) {
        auto* inner_item_clone = to_item->AddInnerItem(CurMap->GetTempEntityId(), static_cast<const ProtoItem*>(inner_item->GetProto()), inner_item->GetContainerStack(), &from_item->GetProperties());
        CloneInnerItems(inner_item_clone, inner_item);
    }
}

void FOMapper::BufferCopy()
{
    STACK_TRACE_ENTRY();

    if (CurMap == nullptr) {
        return;
    }

    std::tie(BefferHexX, BefferHexY) = CurMap->GetScreenHexes();

    // Clear buffers
    std::function<void(EntityBuf*)> free_entity = [&free_entity](EntityBuf* entity_buf) {
        delete entity_buf->Props;
        for (auto* child : entity_buf->Children) {
            free_entity(child);
            delete child;
        }
    };
    for (auto& entity_buf : EntitiesBuffer) {
        free_entity(&entity_buf);
    }
    EntitiesBuffer.clear();

    // Add entities to buffer
    std::function<void(EntityBuf*, ClientEntity*)> add_entity;
    add_entity = [&add_entity, this](EntityBuf* entity_buf, ClientEntity* entity) {
        uint16 hx = 0u;
        uint16 hy = 0u;
        if (const auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
            hx = cr->GetHexX();
            hy = cr->GetHexY();
        }
        else if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
            hx = item->GetHexX();
            hy = item->GetHexY();
        }

        entity_buf->HexX = hx;
        entity_buf->HexY = hy;
        entity_buf->IsCritter = dynamic_cast<CritterHexView*>(entity) != nullptr;
        entity_buf->IsItem = dynamic_cast<ItemHexView*>(entity) != nullptr;
        entity_buf->Proto = dynamic_cast<EntityWithProto*>(entity)->GetProto();
        entity_buf->Props = new Properties(entity->GetProperties());

        for (auto* child : GetEntityInnerItems(entity)) {
            auto* child_buf = new EntityBuf();
            add_entity(child_buf, child);
            entity_buf->Children.push_back(child_buf);
        }
    };
    for (auto* entity : SelectedEntities) {
        EntitiesBuffer.emplace_back();
        add_entity(&EntitiesBuffer.back(), entity);
    }
}

void FOMapper::BufferCut()
{
    STACK_TRACE_ENTRY();

    if (CurMap == nullptr) {
        return;
    }

    BufferCopy();
    SelectDelete();
}

void FOMapper::BufferPaste()
{
    STACK_TRACE_ENTRY();

    if (CurMap == nullptr) {
        return;
    }

    auto&& [sx, sy] = CurMap->GetScreenHexes();
    const auto hx_offset = sx - BefferHexX;
    const auto hy_offset = sy - BefferHexY;

    SelectClear();

    for (const auto& entity_buf : EntitiesBuffer) {
        const auto hx = static_cast<int>(entity_buf.HexX) + hx_offset;
        const auto hy = static_cast<int>(entity_buf.HexY) + hy_offset;

        if (hx < 0 || hx >= CurMap->GetWidth() || hy < 0 || hy >= CurMap->GetHeight()) {
            continue;
        }

        std::function<void(const EntityBuf*, ItemView*)> add_item_inner_items;

        add_item_inner_items = [&add_item_inner_items, this](const EntityBuf* item_entity_buf, ItemView* item) {
            for (const auto* child_buf : item_entity_buf->Children) {
                auto* inner_item = item->AddInnerItem(CurMap->GetTempEntityId(), static_cast<const ProtoItem*>(child_buf->Proto), ContainerItemStack::Root, child_buf->Props);

                add_item_inner_items(child_buf, inner_item);
            }
        };

        if (entity_buf.IsCritter) {
            auto* cr = CurMap->AddMapperCritter(entity_buf.Proto->GetProtoId(), static_cast<uint16>(hx), static_cast<uint16>(hy), 0, entity_buf.Props);

            for (const auto* child_buf : entity_buf.Children) {
                auto* inv_item = cr->AddInvItem(CurMap->GetTempEntityId(), static_cast<const ProtoItem*>(child_buf->Proto), CritterItemSlot::Inventory, child_buf->Props);

                add_item_inner_items(child_buf, inv_item);
            }

            SelectAdd(cr);
        }
        else if (entity_buf.IsItem) {
            auto* item = CurMap->AddMapperItem(entity_buf.Proto->GetProtoId(), static_cast<uint16>(hx), static_cast<uint16>(hy), entity_buf.Props);

            add_item_inner_items(&entity_buf, item);

            SelectAdd(item);
        }
    }
}

void FOMapper::CurDraw()
{
    STACK_TRACE_ENTRY();

    switch (CurMode) {
    case CUR_MODE_DEFAULT:
    case CUR_MODE_MOVE_SELECTION: {
        const auto* anim = CurMode == CUR_MODE_DEFAULT ? CurPDef.get() : CurPHand.get();
        if (anim != nullptr) {
            SprMngr.DrawSprite(anim, Settings.MouseX, Settings.MouseY, COLOR_SPRITE);
        }
    } break;
    case CUR_MODE_PLACE_OBJECT:
        if (IsItemMode() && !(*CurItemProtos).empty()) {
            const auto* proto_item = (*CurItemProtos)[GetTabIndex()];

            uint16 hx = 0;
            uint16 hy = 0;
            if (!CurMap->GetHexAtScreenPos(Settings.MouseX, Settings.MouseY, hx, hy, nullptr, nullptr)) {
                break;
            }

            if (proto_item->GetIsTile()) {
                hx -= hx % Settings.MapTileStep;
                hy -= hy % Settings.MapTileStep;
            }

            const auto* spr = GetIfaceSpr(proto_item->GetPicMap());
            if (spr != nullptr) {
                auto x = CurMap->GetField(hx, hy).ScrX - (spr->Width / 2) + spr->OffsX + (Settings.MapHexWidth / 2) + Settings.ScrOx + proto_item->GetOffsetX();
                auto y = CurMap->GetField(hx, hy).ScrY - spr->Height + spr->OffsY + (Settings.MapHexHeight / 2) + Settings.ScrOy + proto_item->GetOffsetY();

                if (proto_item->GetIsTile()) {
                    if (DrawRoof) {
                        x += Settings.MapRoofOffsX;
                        y += Settings.MapRoofOffsY;
                    }
                    else {
                        x += Settings.MapTileOffsX;
                        y += Settings.MapTileOffsY;
                    }
                }

                SprMngr.DrawSpriteSize(spr, static_cast<int>(static_cast<float>(x) / CurMap->GetSpritesZoom()), static_cast<int>(static_cast<float>(y) / CurMap->GetSpritesZoom()), //
                    static_cast<int>(static_cast<float>(spr->Width) / CurMap->GetSpritesZoom()), static_cast<int>(static_cast<float>(spr->Height) / CurMap->GetSpritesZoom()), true, false, COLOR_SPRITE);
            }
        }
        else if (IsCritMode() && !CurNpcProtos->empty()) {
            const auto model_name = (*CurNpcProtos)[GetTabIndex()]->GetModelName();
            const auto* anim = ResMngr.GetCritterPreviewSpr(model_name, CritterStateAnim::Unarmed, CritterActionAnim::Idle, NpcDir, nullptr);
            if (anim == nullptr) {
                anim = ResMngr.GetItemDefaultSpr().get();
            }

            uint16 hx = 0;
            uint16 hy = 0;
            if (!CurMap->GetHexAtScreenPos(Settings.MouseX, Settings.MouseY, hx, hy, nullptr, nullptr)) {
                break;
            }

            const auto x = CurMap->GetField(hx, hy).ScrX - (anim->Width / 2) + anim->OffsX;
            const auto y = CurMap->GetField(hx, hy).ScrY - anim->Height + anim->OffsY;

            SprMngr.DrawSpriteSize(anim, //
                static_cast<int>((static_cast<float>(x + Settings.ScrOx) + (static_cast<float>(Settings.MapHexWidth) / 2.0f)) / CurMap->GetSpritesZoom()), //
                static_cast<int>((static_cast<float>(y + Settings.ScrOy) + (static_cast<float>(Settings.MapHexHeight) / 2.0f)) / CurMap->GetSpritesZoom()), //
                static_cast<int>(static_cast<float>(anim->Width) / CurMap->GetSpritesZoom()), //
                static_cast<int>(static_cast<float>(anim->Height) / CurMap->GetSpritesZoom()), true, false, COLOR_SPRITE);
        }
        else {
            CurMode = CUR_MODE_DEFAULT;
        }
        break;
    default:
        CurMode = CUR_MODE_DEFAULT;
        break;
    }
}

void FOMapper::CurRMouseUp()
{
    STACK_TRACE_ENTRY();

    if (IntHold == INT_NONE) {
        if (CurMode == CUR_MODE_MOVE_SELECTION) {
            CurMode = CUR_MODE_DEFAULT;
        }
        else if (CurMode == CUR_MODE_PLACE_OBJECT) {
            CurMode = CUR_MODE_DEFAULT;
        }
        else if (CurMode == CUR_MODE_DEFAULT) {
            if (!SelectedEntities.empty()) {
                CurMode = CUR_MODE_MOVE_SELECTION;
            }
            else if (IsItemMode() || IsCritMode()) {
                CurMode = CUR_MODE_PLACE_OBJECT;
            }
        }
    }
}

void FOMapper::CurMMouseDown()
{
    STACK_TRACE_ENTRY();

    if (SelectedEntities.empty()) {
        NpcDir++;
        if (NpcDir >= GameSettings::MAP_DIR_COUNT) {
            NpcDir = 0;
        }

        DrawRoof = !DrawRoof;
    }
    else {
        for (auto* entity : SelectedEntities) {
            if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
                uint dir = cr->GetDir() + 1u;
                if (dir >= GameSettings::MAP_DIR_COUNT) {
                    dir = 0u;
                }
                cr->ChangeDir(static_cast<uint8>(dir));
            }
        }
    }
}

auto FOMapper::IsCurInRect(const IRect& rect, int ax, int ay) const -> bool
{
    STACK_TRACE_ENTRY();

    return Settings.MouseX >= rect[0] + ax && Settings.MouseY >= rect[1] + ay && Settings.MouseX <= rect[2] + ax && Settings.MouseY <= rect[3] + ay;
}

auto FOMapper::IsCurInRect(const IRect& rect) const -> bool
{
    STACK_TRACE_ENTRY();

    return Settings.MouseX >= rect[0] && Settings.MouseY >= rect[1] && Settings.MouseX <= rect[2] && Settings.MouseY <= rect[3];
}

auto FOMapper::IsCurInRectNoTransp(const Sprite* spr, const IRect& rect, int ax, int ay) const -> bool
{
    STACK_TRACE_ENTRY();

    return IsCurInRect(rect, ax, ay) && SprMngr.SpriteHitTest(spr, Settings.MouseX - rect.Left - ax, Settings.MouseY - rect.Top - ay, false);
}

auto FOMapper::IsCurInInterface() const -> bool
{
    STACK_TRACE_ENTRY();

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

auto FOMapper::GetCurHex(uint16& hx, uint16& hy, bool ignore_interface) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    hx = hy = 0;
    if (!ignore_interface && IsCurInInterface()) {
        return false;
    }
    return CurMap->GetHexAtScreenPos(Settings.MouseX, Settings.MouseY, hx, hy, nullptr, nullptr);
}

void FOMapper::ConsoleDraw()
{
    STACK_TRACE_ENTRY();

    if (ConsoleEdit) {
        SprMngr.DrawSprite(ConsolePic.get(), IntX + ConsolePicX, (IntVisible ? IntY : Settings.ScreenHeight) + ConsolePicY, COLOR_SPRITE);

        auto str = ConsoleStr;
        str.insert(ConsoleCur, time_duration_to_ms<uint>(GameTime.FrameTime().time_since_epoch()) % 800 < 400 ? "!" : ".");
        SprMngr.DrawStr(IRect(IntX + ConsoleTextX, (IntVisible ? IntY : Settings.ScreenHeight) + ConsoleTextY, Settings.ScreenWidth, Settings.ScreenHeight), str, FT_NOBREAK, COLOR_TEXT, FONT_DEFAULT);
    }
}

void FOMapper::ConsoleKeyDown(KeyCode dik, string_view dik_text)
{
    STACK_TRACE_ENTRY();

    if (dik == KeyCode::Return || dik == KeyCode::Numpadenter) {
        if (ConsoleEdit) {
            if (ConsoleStr.empty()) {
                ConsoleEdit = false;
            }
            else {
                // Modify console history
                ConsoleHistory.push_back(ConsoleStr);
                for (int i = 0; i < static_cast<int>(ConsoleHistory.size()) - 1; i++) {
                    if (ConsoleHistory[i] == ConsoleHistory[ConsoleHistory.size() - 1]) {
                        ConsoleHistory.erase(ConsoleHistory.begin() + i);
                        i = -1;
                    }
                }
                while (ConsoleHistory.size() > Settings.ConsoleHistorySize) {
                    ConsoleHistory.erase(ConsoleHistory.begin());
                }
                ConsoleHistoryCur = static_cast<int>(ConsoleHistory.size());

                // Save console history
                string history_str;
                for (const auto& str : ConsoleHistory) {
                    history_str += str + "\n";
                }
                Cache.SetString("mapper_console.txt", history_str);

                // Process command
                const auto process_command = OnConsoleMessage.Fire(ConsoleStr);
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
            ConsoleHistoryCur = static_cast<int>(ConsoleHistory.size());
        }

        return;
    }

    switch (dik) {
    case KeyCode::Up:
        if (ConsoleHistoryCur - 1 < 0) {
            return;
        }
        ConsoleHistoryCur--;
        ConsoleStr = ConsoleHistory[ConsoleHistoryCur];
        ConsoleCur = static_cast<uint>(ConsoleStr.length());
        return;
    case KeyCode::Down:
        if (ConsoleHistoryCur + 1 >= static_cast<int>(ConsoleHistory.size())) {
            ConsoleHistoryCur = static_cast<int>(ConsoleHistory.size());
            ConsoleStr = "";
            ConsoleCur = 0;
            return;
        }
        ConsoleHistoryCur++;
        ConsoleStr = ConsoleHistory[ConsoleHistoryCur];
        ConsoleCur = static_cast<uint>(ConsoleStr.length());
        return;
    default:
        Keyb.FillChar(dik, dik_text, ConsoleStr, &ConsoleCur, KIF_NO_SPEC_SYMBOLS);
        ConsoleLastKey = dik;
        ConsoleLastKeyText = dik_text;
        ConsoleKeyTime = GameTime.FrameTime();
        ConsoleAccelerate = 1;
        return;
    }
}

void FOMapper::ConsoleKeyUp(KeyCode /*key*/)
{
    STACK_TRACE_ENTRY();

    ConsoleLastKey = KeyCode::None;
    ConsoleLastKeyText = "";
}

void FOMapper::ConsoleProcess()
{
    STACK_TRACE_ENTRY();

    if (ConsoleLastKey == KeyCode::None) {
        return;
    }

    if (time_duration_to_ms<int>(GameTime.FrameTime() - ConsoleKeyTime) >= CONSOLE_KEY_TICK - ConsoleAccelerate) {
        ConsoleKeyTime = GameTime.FrameTime();
        ConsoleAccelerate = CONSOLE_MAX_ACCELERATE;
        Keyb.FillChar(ConsoleLastKey, ConsoleLastKeyText, ConsoleStr, &ConsoleCur, KIF_NO_SPEC_SYMBOLS);
    }
}

void FOMapper::ParseCommand(string_view command)
{
    STACK_TRACE_ENTRY();

    if (command.empty()) {
        return;
    }

    // Load map
    if (command[0] == '~') {
        string map_name = _str(command.substr(1)).trim();
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
        string map_name = _str(command.substr(1)).trim();
        if (map_name.empty()) {
            AddMess("Error parse map name");
            return;
        }

        if (CurMap == nullptr) {
            AddMess("Map not loaded");
            return;
        }

        SaveMap(CurMap, map_name);

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

        auto func = ScriptSys->FindFunc<string, string>(ToHashedString(func_name));
        if (!func) {
            AddMess("Function not found");
            return;
        }

        string str = _str(command).substringAfter(' ').trim();
        if (!func(str)) {
            AddMess("Script execution fail");
            return;
        }

        AddMess(_str("Result: {}", func.GetResult()));
    }
    // Critter animations
    else if (command[0] == '@') {
        AddMess("Playing critter animations");

        if (CurMap == nullptr) {
            AddMess("Map not loaded");
            return;
        }

        vector<int> anims = _str(command.substr(1)).splitToInt(' ');
        if (anims.empty()) {
            return;
        }

        if (!SelectedEntities.empty()) {
            for (auto* entity : SelectedEntities) {
                if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
                    cr->ClearAnim();
                    for (uint j = 0; j < anims.size() / 2; j++) {
                        cr->Animate(static_cast<CritterStateAnim>(anims[static_cast<size_t>(j) * 2]), static_cast<CritterActionAnim>(anims[j * 2 + 1]), nullptr);
                    }
                }
            }
        }
        else {
            for (auto* cr : CurMap->GetCritters()) {
                cr->ClearAnim();
                for (uint j = 0; j < anims.size() / 2; j++) {
                    cr->Animate(static_cast<CritterStateAnim>(anims[static_cast<size_t>(j) * 2]), static_cast<CritterActionAnim>(anims[j * 2 + 1]), nullptr);
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
            auto* pmap = new ProtoMap(ToHashedString("new"), GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME));

            pmap->SetWidth(MAXHEX_DEFAULT);
            pmap->SetHeight(MAXHEX_DEFAULT);

            // Morning	 5.00 -  9.59	 300 - 599
            // Day		10.00 - 18.59	 600 - 1139
            // Evening	19.00 - 22.59	1140 - 1379
            // Nigh		23.00 -  4.59	1380
            vector<int> arr = {300, 600, 1140, 1380};
            vector<uint8> arr2 = {18, 128, 103, 51, 18, 128, 95, 40, 53, 128, 86, 29};
            pmap->SetDayTime(arr);
            pmap->SetDayColor(arr2);

            auto* map = new MapView(this, ident_t {}, pmap);

            map->EnableMapperMode();
            map->FindSetCenter(MAXHEX_DEFAULT / 2, MAXHEX_DEFAULT / 2);

            LoadedMaps.push_back(map);

            ShowMap(map);

            AddMess("Create map success");
        }
        else if (command_ext == "unload") {
            AddMess("Unload map");

            if (CurMap == nullptr) {
                AddMess("Map not loaded");
                return;
            }

            UnloadMap(CurMap);

            if (!LoadedMaps.empty()) {
                ShowMap(LoadedMaps.front());
            }
        }
        else if (command_ext == "size" && (CurMap != nullptr)) {
            AddMess("Resize map");

            if (CurMap == nullptr) {
                AddMess("Map not loaded");
                return;
            }

            int maxhx = 0;
            int maxhy = 0;
            if (!(icommand >> maxhx >> maxhy)) {
                AddMess("Invalid args");
                return;
            }

            ResizeMap(CurMap, static_cast<uint16>(maxhx), static_cast<uint16>(maxhy));
        }
    }
    else {
        AddMess("Unknown command");
    }
}

auto FOMapper::LoadMap(string_view map_name) -> MapView*
{
    STACK_TRACE_ENTRY();

    const auto map_files = ContentFileSys.FilterFiles("fomap");
    const auto map_file = map_files.FindFileByName(map_name);
    if (!map_file) {
        AddMess("Map file not found");
        return nullptr;
    }

    const auto map_file_str = map_file.GetStr();

    auto map_data = ConfigFile(_str("{}.fomap", map_name), map_file_str, this, ConfigFileOption::ReadFirstSection);
    if (!map_data.HasSection("ProtoMap")) {
        throw MapLoaderException("Invalid map format", map_name);
    }

    auto* pmap = new ProtoMap(ToHashedString(map_name), GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME));
    if (!pmap->GetPropertiesForEdit().ApplyFromText(map_data.GetSection("ProtoMap"))) {
        throw MapLoaderException("Invalid map header", map_name);
    }

    auto&& new_map_holder = std::make_unique<MapView>(this, ident_t {}, pmap);
    auto* new_map = new_map_holder.get();

    new_map->EnableMapperMode();

    try {
        new_map->LoadFromFile(map_name, map_file.GetStr());
    }
    catch (const MapLoaderException& ex) {
        AddMess(_str("Map truncated: {}", ex.what()));
        return nullptr;
    }

    new_map->FindSetCenter(new_map->GetWorkHexX(), new_map->GetWorkHexY());

    OnEditMapLoad.Fire(new_map);

    LoadedMaps.push_back(new_map);

    return new_map_holder.release();
}

void FOMapper::ShowMap(MapView* map)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!map->IsDestroyed());

    const auto it = std::find(LoadedMaps.begin(), LoadedMaps.end(), map);
    RUNTIME_ASSERT(it != LoadedMaps.end());

    if (map == CurMap) {
        return;
    }

    SelectClear();

    CurMap = map;

    if (const auto day_time = CurMap->GetCurDayTime(); day_time >= 0) {
        SetHour(day_time / 60 % 24);
        SetMinute(day_time % 60);
    }
    else {
        SetHour(12);
        SetMinute(0);
    }

    RefreshCurProtos();
}

void FOMapper::SaveMap(MapView* map, string_view custom_name)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!map->IsDestroyed());

    const auto it = std::find(LoadedMaps.begin(), LoadedMaps.end(), map);
    RUNTIME_ASSERT(it != LoadedMaps.end());

    const auto map_errors = map->ValidateForSave();
    if (!map_errors.empty()) {
        for (const auto& error : map_errors) {
            AddMess(error);
        }

        return;
    }

    const auto fomap_content = map->SaveToText();

    const auto fomap_name = !custom_name.empty() ? custom_name : map->GetProto()->GetName();
    RUNTIME_ASSERT(!fomap_name.empty());

    string fomap_path;
    {
        auto fomap_files = ContentFileSys.FilterFiles("fomap");

        if (const auto fomap_file = fomap_files.FindFileByName(fomap_name)) {
            fomap_path = fomap_file.GetFullPath();
        }
        else if (const auto fomap_file2 = fomap_files.FindFileByName(map->GetProto()->GetName())) {
            fomap_path = _str(fomap_file2.GetFullPath()).changeFileName(fomap_name);
        }
        else if (fomap_files.MoveNext()) {
            fomap_path = _str(fomap_files.GetCurFile().GetFullPath()).changeFileName(fomap_name);
        }
        else {
            fomap_path = _str("{}.fomap", fomap_path).formatPath();
        }
    }

    auto fomap_file = DiskFileSystem::OpenFile(fomap_path, true);
    RUNTIME_ASSERT(fomap_file);
    const auto fomap_file_ok = fomap_file.Write(fomap_content);
    RUNTIME_ASSERT(fomap_file_ok);

    OnEditMapSave.Fire(map);
}

void FOMapper::UnloadMap(MapView* map)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!map->IsDestroyed());

    if (map == CurMap) {
        SelectClear();
        CurMap = nullptr;
    }

    const auto it = std::find(LoadedMaps.begin(), LoadedMaps.end(), map);
    RUNTIME_ASSERT(it != LoadedMaps.end());
    LoadedMaps.erase(it);

    map->MarkAsDestroyed();
    map->Release();
}

void FOMapper::ResizeMap(MapView* map, uint16 width, uint16 height)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!map->IsDestroyed());

    const auto fixed_width = std::clamp(width, MAXHEX_MIN, MAXHEX_MAX);
    const auto fixed_height = std::clamp(height, MAXHEX_MIN, MAXHEX_MAX);

    if (map->GetWorkHexX() >= fixed_width) {
        map->SetWorkHexX(fixed_width - 1);
    }
    if (map->GetWorkHexY() >= fixed_height) {
        map->SetWorkHexY(fixed_height - 1);
    }

    map->Resize(fixed_width, fixed_height);

    map->FindSetCenter(map->GetWorkHexX(), map->GetWorkHexY());

    if (map == CurMap) {
        SelectClear();
    }
}

void FOMapper::AddMess(string_view message_text)
{
    STACK_TRACE_ENTRY();

    const string str = _str("|{} - {}\n", COLOR_TEXT, message_text);

    const auto dt = Timer::GetCurrentDateTime();
    const string mess_time = _str("{:02}:{:02}:{:02} ", dt.Hour, dt.Minute, dt.Second);

    MessBox.push_back({0, str, mess_time});
    MessBoxScroll = 0;

    MessBoxCurText = "";

    const IRect ir(IntWWork[0] + IntX, IntWWork[1] + IntY, IntWWork[2] + IntX, IntWWork[3] + IntY);
    int max_lines = ir.Height() / 10;
    if (ir.IsZero()) {
        max_lines = 20;
    }

    int cur_mess = static_cast<int>(MessBox.size()) - 1;
    for (int i = 0, j = 0; cur_mess >= 0; cur_mess--) {
        MessBoxMessage& m = MessBox[cur_mess];
        // Scroll
        j++;
        if (j <= MessBoxScroll) {
            continue;
        }
        // Add to message box
        MessBoxCurText = m.Mess + MessBoxCurText;
        i++;
        if (i >= max_lines) {
            break;
        }
    }
}

void FOMapper::MessBoxDraw()
{
    STACK_TRACE_ENTRY();

    if (!IntVisible) {
        return;
    }
    if (MessBoxCurText.empty()) {
        return;
    }

    SprMngr.DrawStr(IRect(IntWWork[0] + IntX, IntWWork[1] + IntY, IntWWork[2] + IntX, IntWWork[3] + IntY), MessBoxCurText, FT_UPPER | FT_BOTTOM, COLOR_TEXT, FONT_DEFAULT);
}

void FOMapper::DrawIfaceLayer(uint layer)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(layer);

    SpritesCanDraw = true;
    OnRenderIface.Fire(); // Todo: mapper render iface layer
    SpritesCanDraw = false;
}

auto FOMapper::GetEntityInnerItems(ClientEntity* entity) -> vector<ItemView*>
{
    STACK_TRACE_ENTRY();

    if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
        return cr->GetInvItems();
    }
    if (auto* item = dynamic_cast<ItemView*>(entity); item != nullptr) {
        return item->GetInnerItems();
    }
    return {};
}
