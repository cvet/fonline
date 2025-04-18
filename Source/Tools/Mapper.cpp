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
#include "DiskFileSystem.h"
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"

FOMapper::FOMapper(GlobalSettings& settings, AppWindow* window) :
    FOClient(settings, window, true)
{
    STACK_TRACE_ENTRY();

    Resources.AddDataSource(strex(Settings.ResourcesDir).combinePath("FullProtos"));
    if constexpr (FO_ANGELSCRIPT_SCRIPTING) {
        Resources.AddDataSource(strex(Settings.ResourcesDir).combinePath("MapperAngelScript"));
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

    extern void Init_AngelScript_MapperScriptSystem(FOEngineBase*);
    Init_AngelScript_MapperScriptSystem(this);

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
    ScriptSys->InitModules();
    OnStart.Fire();

    if (!Settings.StartMap.empty()) {
        auto* map = LoadMap(Settings.StartMap);

        if (map != nullptr) {
            if (Settings.StartHexX > 0 && Settings.StartHexY > 0) {
                _curMap->FindSetCenter({static_cast<uint16>(Settings.StartHexX), static_cast<uint16>(Settings.StartHexY)});
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
    ConsoleHistory = strex(history_str).normalizeLineEndings().split('\n');
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

    IfaceIni = SafeAlloc::MakeUnique<ConfigFile>("mapper_default.ini", config_content);
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
    MemFill(TabIndex, 0, sizeof(TabIndex));
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

    auto spr = SprMngr.LoadSprite(fname, AtlasType::IfaceSprites);

    if (spr) {
        spr->PlayDefault();
    }

    return IfaceSpr.emplace(fname, std::move(spr)).first->second.get();
}

void FOMapper::ProcessMapperInput()
{
    STACK_TRACE_ENTRY();

    Settings.MousePos = App->Input.GetMousePosition();

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
                    if (_curMap != nullptr) {
                        _curMap->RefreshMap();
                    }
                    break;
                case KeyCode::F2:
                    Settings.ShowScen = !Settings.ShowScen;
                    if (_curMap != nullptr) {
                        _curMap->RefreshMap();
                    }
                    break;
                case KeyCode::F3:
                    Settings.ShowWall = !Settings.ShowWall;
                    if (_curMap != nullptr) {
                        _curMap->RefreshMap();
                    }
                    break;
                case KeyCode::F4:
                    Settings.ShowCrit = !Settings.ShowCrit;
                    if (_curMap != nullptr) {
                        _curMap->RefreshMap();
                    }
                    break;
                case KeyCode::F5:
                    Settings.ShowTile = !Settings.ShowTile;
                    if (_curMap != nullptr) {
                        _curMap->RefreshMap();
                    }
                    break;
                case KeyCode::F6:
                    Settings.ShowFast = !Settings.ShowFast;
                    if (_curMap != nullptr) {
                        _curMap->RefreshMap();
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
                    if (_curMap != nullptr) {
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
                    if (_curMap != nullptr && !ConsoleEdit && SelectedEntities.empty()) {
                        int day_time = GetGlobalDayTime();
                        day_time += 60;
                        while (day_time > 2880) {
                            day_time -= 1440;
                        }
                        SetGlobalDayTime(day_time);
                    }
                    break;
                case KeyCode::Subtract:
                    if (_curMap != nullptr && !ConsoleEdit && SelectedEntities.empty()) {
                        int day_time = GetGlobalDayTime();
                        day_time -= 60;
                        while (day_time < 0) {
                            day_time += 1440;
                        }
                        SetGlobalDayTime(day_time);
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
                    if (_curMap != nullptr) {
                        SaveMap(_curMap, "");
                    }
                    break;
                case KeyCode::D:
                    Settings.ScrollCheck = !Settings.ScrollCheck;
                    break;
                case KeyCode::B:
                    if (_curMap != nullptr) {
                        _curMap->MarkBlockedHexes();
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
                TabsScroll[SubTabsActiveTab] = std::max(TabsScroll[SubTabsActiveTab], 0);
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
                        if (*CurProtoScroll >= static_cast<int>(CurItemProtos->size())) {
                            *CurProtoScroll = static_cast<int>(CurItemProtos->size()) - 1;
                        }
                    }
                    else if (IsCritMode() && !CurNpcProtos->empty()) {
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
                if (ev.MouseWheel.Delta != 0 && _curMap != nullptr) {
                    _curMap->ChangeZoom(ev.MouseWheel.Delta > 0 ? -1 : 1);
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

    FrameAdvance();

    OnLoop.Fire();
    ConsoleProcess();
    ProcessMapperInput();

    if (_curMap != nullptr) {
        _curMap->Process();
    }

    {
        SprMngr.BeginScene({100, 100, 100});

        DrawIfaceLayer(0);

        if (_curMap != nullptr) {
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
    STACK_TRACE_ENTRY();

    if (!IntVisible) {
        return;
    }

    SprMngr.DrawSprite(IntMainPic.get(), {IntX, IntY}, COLOR_SPRITE);

    switch (IntMode) {
    case INT_MODE_CUSTOM0:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[0][0] + IntX, IntBCust[0][1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM1:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[1][0] + IntX, IntBCust[1][1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM2:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[2][0] + IntX, IntBCust[2][1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM3:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[3][0] + IntX, IntBCust[3][1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM4:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[4][0] + IntX, IntBCust[4][1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM5:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[5][0] + IntX, IntBCust[5][1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM6:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[6][0] + IntX, IntBCust[6][1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM7:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[7][0] + IntX, IntBCust[7][1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM8:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[8][0] + IntX, IntBCust[8][1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CUSTOM9:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCust[9][0] + IntX, IntBCust[9][1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_ITEM:
        SprMngr.DrawSprite(IntPTab.get(), {IntBItem[0] + IntX, IntBItem[1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_TILE:
        SprMngr.DrawSprite(IntPTab.get(), {IntBTile[0] + IntX, IntBTile[1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_CRIT:
        SprMngr.DrawSprite(IntPTab.get(), {IntBCrit[0] + IntX, IntBCrit[1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_FAST:
        SprMngr.DrawSprite(IntPTab.get(), {IntBFast[0] + IntX, IntBFast[1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_IGNORE:
        SprMngr.DrawSprite(IntPTab.get(), {IntBIgnore[0] + IntX, IntBIgnore[1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_INCONT:
        SprMngr.DrawSprite(IntPTab.get(), {IntBInCont[0] + IntX, IntBInCont[1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_MESS:
        SprMngr.DrawSprite(IntPTab.get(), {IntBMess[0] + IntX, IntBMess[1] + IntY}, COLOR_SPRITE);
        break;
    case INT_MODE_LIST:
        SprMngr.DrawSprite(IntPTab.get(), {IntBList[0] + IntX, IntBList[1] + IntY}, COLOR_SPRITE);
        break;
    default:
        break;
    }

    for (auto i = INT_MODE_CUSTOM0; i <= INT_MODE_CUSTOM9; i++) {
        DrawStr(IRect(IntBCust[i], IntX, IntY), TabsName[INT_MODE_CUSTOM0 + i], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    }
    DrawStr(IRect(IntBItem, IntX, IntY), TabsName[INT_MODE_ITEM], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(IRect(IntBTile, IntX, IntY), TabsName[INT_MODE_TILE], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(IRect(IntBCrit, IntX, IntY), TabsName[INT_MODE_CRIT], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(IRect(IntBFast, IntX, IntY), TabsName[INT_MODE_FAST], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(IRect(IntBIgnore, IntX, IntY), TabsName[INT_MODE_IGNORE], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(IRect(IntBInCont, IntX, IntY), TabsName[INT_MODE_INCONT], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(IRect(IntBMess, IntX, IntY), TabsName[INT_MODE_MESS], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    DrawStr(IRect(IntBList, IntX, IntY), TabsName[INT_MODE_LIST], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);

    if (Settings.ShowItem) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowItem[0] + IntX, IntBShowItem[1] + IntY}, COLOR_SPRITE);
    }
    if (Settings.ShowScen) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowScen[0] + IntX, IntBShowScen[1] + IntY}, COLOR_SPRITE);
    }
    if (Settings.ShowWall) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowWall[0] + IntX, IntBShowWall[1] + IntY}, COLOR_SPRITE);
    }
    if (Settings.ShowCrit) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowCrit[0] + IntX, IntBShowCrit[1] + IntY}, COLOR_SPRITE);
    }
    if (Settings.ShowTile) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowTile[0] + IntX, IntBShowTile[1] + IntY}, COLOR_SPRITE);
    }
    if (Settings.ShowRoof) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowRoof[0] + IntX, IntBShowRoof[1] + IntY}, COLOR_SPRITE);
    }
    if (Settings.ShowFast) {
        SprMngr.DrawSprite(IntPShow.get(), {IntBShowFast[0] + IntX, IntBShowFast[1] + IntY}, COLOR_SPRITE);
    }

    if (IsSelectItem) {
        SprMngr.DrawSprite(IntPSelect.get(), {IntBSelectItem[0] + IntX, IntBSelectItem[1] + IntY}, COLOR_SPRITE);
    }
    if (IsSelectScen) {
        SprMngr.DrawSprite(IntPSelect.get(), {IntBSelectScen[0] + IntX, IntBSelectScen[1] + IntY}, COLOR_SPRITE);
    }
    if (IsSelectWall) {
        SprMngr.DrawSprite(IntPSelect.get(), {IntBSelectWall[0] + IntX, IntBSelectWall[1] + IntY}, COLOR_SPRITE);
    }
    if (IsSelectCrit) {
        SprMngr.DrawSprite(IntPSelect.get(), {IntBSelectCrit[0] + IntX, IntBSelectCrit[1] + IntY}, COLOR_SPRITE);
    }
    if (IsSelectTile) {
        SprMngr.DrawSprite(IntPSelect.get(), {IntBSelectTile[0] + IntX, IntBSelectTile[1] + IntY}, COLOR_SPRITE);
    }
    if (IsSelectRoof) {
        SprMngr.DrawSprite(IntPSelect.get(), {IntBSelectRoof[0] + IntX, IntBSelectRoof[1] + IntY}, COLOR_SPRITE);
    }

    auto x = IntWWork[0] + IntX;
    auto y = IntWWork[1] + IntY;
    auto h = IntWWork[3] - IntWWork[1];
    int w = ProtoWidth;

    if (IsItemMode()) {
        auto i = *CurProtoScroll;
        int j = static_cast<int>(static_cast<size_t>(i) + ProtosOnScreen);
        j = std::min(j, static_cast<int>(CurItemProtos->size()));

        for (; i < j; i++, x += w) {
            const auto* proto_item = (*CurItemProtos)[i];
            auto col = (i == static_cast<int>(GetTabIndex()) ? COLOR_SPRITE_RED : COLOR_SPRITE);
            if (const auto* spr = GetIfaceSpr(proto_item->GetPicMap()); spr != nullptr) {
                SprMngr.DrawSpriteSize(spr, {x, y}, {w, h / 2}, false, true, col);
            }

            if (proto_item->GetPicInv()) {
                const auto* spr = GetIfaceSpr(proto_item->GetPicInv());
                if (spr != nullptr) {
                    SprMngr.DrawSpriteSize(spr, {x, y + h / 2}, {w, h / 2}, false, true, col);
                }
            }

            DrawStr(IRect(x, y + h - 15, x + w, y + h), proto_item->GetName(), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
        }

        if (GetTabIndex() < static_cast<uint>(CurItemProtos->size())) {
            const auto* proto_item = (*CurItemProtos)[GetTabIndex()];

            SprMngr.DrawText(irect(IntWHint.Left + IntX, IntWHint.Top + IntY, IntWHint.Width(), IntWHint.Height()), proto_item->GetName(), 0, COLOR_TEXT, FONT_DEFAULT);
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

            SprMngr.DrawSpriteSize(anim, {x, y}, {w, h / 2}, false, true, col);
            DrawStr(IRect(x, y + h - 15, x + w, y + h), proto->GetName(), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
        }

        if (GetTabIndex() < CurNpcProtos->size()) {
            const auto* proto = (*CurNpcProtos)[GetTabIndex()];
            DrawStr(IRect(IntWHint, IntX, IntY), proto->GetName(), 0, COLOR_TEXT, FONT_DEFAULT);
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

            SprMngr.DrawSpriteSize(spr, {x, y}, {w, h}, false, true, col);

            SprMngr.DrawText(irect(x, y + h - 15, w, h), strex("x{}", inner_item->GetCount()), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);

            if (inner_item->GetOwnership() == ItemOwnership::CritterInventory && inner_item->GetCritterSlot() != CritterItemSlot::Inventory) {
                SprMngr.DrawText(irect(x, y, w, h), strex("Slot {}", inner_item->GetCritterSlot()), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
            }
        }
    }
    else if (IntMode == INT_MODE_LIST) {
        auto i = ListScroll;
        auto j = static_cast<int>(LoadedMaps.size());

        for (; i < j; i++, x += w) {
            auto* map = LoadedMaps[i];
            SprMngr.DrawText(irect(x, y, w, h), strex(" '{}'", map->GetName()), 0, map == _curMap ? COLOR_SPRITE_RED : COLOR_TEXT, FONT_DEFAULT);
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
            name += strex(" ({})", count);
            DrawStr(r, name, 0, color, FONT_DEFAULT);

            posy -= line_height;
            if (posy < 0) {
                break;
            }
        }
    }

    // Map info
    if (_curMap != nullptr) {
        auto hex_thru = false;
        mpos hex;

        if (_curMap->GetHexAtScreenPos(Settings.MousePos, hex, nullptr)) {
            hex_thru = true;
        }

        auto day_time = GetGlobalDayTime();

        DrawStr(IRect(Settings.ScreenWidth - 100, 0, Settings.ScreenWidth, Settings.ScreenHeight),
            strex("Map '{}'\n"
                  "Hex {}\n"
                  "Time {} : {}\n"
                  "Fps {}\n"
                  "Tile layer {}\n"
                  "{}",
                _curMap->GetName(), hex_thru ? hex : mpos {}, day_time / 60 % 24, day_time % 60, GameTime.GetFramesPerSecond(), TileLayer, Settings.ScrollCheck ? "Scroll check" : ""),
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

    SprMngr.DrawSprite(ObjWMainPic.get(), {ObjX, ObjY}, COLOR_SPRITE);
    if (ObjToAll) {
        SprMngr.DrawSprite(ObjPbToAllDn.get(), {ObjBToAll[0] + ObjX, ObjBToAll[1] + ObjY}, COLOR_SPRITE);
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
    DrawLine("ProtoName", "", entity->GetName(), true, r);
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
        UNREACHABLE_PLACE();
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

    ucolor color = COLOR_TEXT;

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

    string str = strex("{}{}{}{}", name, !type_name.empty() ? " (" : "", !type_name.empty() ? type_name : "", !type_name.empty() ? ")" : "");
    str += "........................................................................................................";
    DrawStr(IRect(IRect(x, y, x + w / 2, y + h), 0, 0), str, FT_NOBREAK, color, FONT_DEFAULT);
    DrawStr(IRect(IRect(x + w / 2, y, x + w, y + h), 0, 0), result_text, FT_NOBREAK, color, FONT_DEFAULT);

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
            try {
                entity->GetPropertiesForEdit().ApplyPropertyFromText(prop, ObjCurLineValue);
            }
            catch (const std::exception& ex) {
                UNUSED_VARIABLE(ex);
                entity->GetPropertiesForEdit().ApplyPropertyFromText(prop, ObjCurLineInitValue);
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

void FOMapper::SelectEntityProp(int line)
{
    STACK_TRACE_ENTRY();

    constexpr auto start_line = 3;

    ObjCurLine = line;
    ObjCurLine = std::max(ObjCurLine, 0);
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
            ShowProps.push_back(prop_index != -1 ? entity->GetProperties().GetRegistrator()->GetPropertyByIndex(prop_index) : nullptr);
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

        if (_curMap == nullptr) {
            return;
        }

        if (!_curMap->GetHexAtScreenPos(Settings.MousePos, SelectHex1, nullptr)) {
            return;
        }

        SelectHex2 = SelectHex1;
        SelectPos = Settings.MousePos;

        if (CurMode == CUR_MODE_DEFAULT) {
            if (Keyb.ShiftDwn) {
                for (auto* entity : SelectedEntities) {
                    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
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
            else if (IsCritMode() && !CurNpcProtos->empty()) {
                CreateCritter((*CurNpcProtos)[GetTabIndex()]->GetProtoId(), SelectHex1);
            }
        }

        return;
    }

    // Object editor
    if (ObjVisible && !SelectedEntities.empty() && IsCurInRect(ObjWMain, ObjX, ObjY)) {
        if (IsCurInRect(ObjWWork, ObjX, ObjY)) {
            SelectEntityProp((Settings.MousePos.y - ObjY - ObjWWork[1]) / DRAW_NEXT_HEIGHT);
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
        int ind = (Settings.MousePos.x - IntX - IntWWork[0]) / ProtoWidth;

        if (IsItemMode() && !CurItemProtos->empty()) {
            ind += *CurProtoScroll;
            if (ind >= static_cast<int>(CurItemProtos->size())) {
                ind = static_cast<int>(CurItemProtos->size()) - 1;
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

                _curMap->SwitchIgnorePid(pid);
                _curMap->RefreshMap();
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
                    CreateItem(proto_item->GetProtoId(), {}, SelectedEntities[0]);
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
                        auto* owner = _curMap->GetCritter(InContItem->GetCritterId());
                        RUNTIME_ASSERT(owner);
                        owner->DeleteInvItem(InContItem, true);
                    }
                    else if (InContItem->GetOwnership() == ItemOwnership::ItemContainer) {
                        ItemView* owner = _curMap->GetItem(InContItem->GetContainerId());
                        RUNTIME_ASSERT(owner);
                        owner->DestroyInnerItem(InContItem);
                    }
                    else {
                        UNREACHABLE_PLACE();
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
                        auto to_slot = static_cast<size_t>(InContItem->GetCritterSlot()) + 1;

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

            if (ind < static_cast<int>(LoadedMaps.size()) && _curMap != LoadedMaps[ind]) {
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
            (*CurProtoScroll) -= static_cast<int>(ProtosOnScreen);
            *CurProtoScroll = std::max(*CurProtoScroll, 0);
        }
        else if (IntMode == INT_MODE_INCONT) {
            InContScroll -= static_cast<int>(ProtosOnScreen);
            InContScroll = std::max(InContScroll, 0);
        }
        else if (IntMode == INT_MODE_LIST) {
            ListScroll -= static_cast<int>(ProtosOnScreen);
            ListScroll = std::max(ListScroll, 0);
        }
    }
    else if (IsCurInRect(IntBScrFront, IntX, IntY)) {
        if (IsItemMode() && !CurItemProtos->empty()) {
            (*CurProtoScroll)++;
            if (*CurProtoScroll >= static_cast<int>(CurItemProtos->size())) {
                *CurProtoScroll = static_cast<int>(CurItemProtos->size()) - 1;
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
        if (IsItemMode() && !CurItemProtos->empty()) {
            (*CurProtoScroll) += static_cast<int>(ProtosOnScreen);
            if (*CurProtoScroll >= static_cast<int>(CurItemProtos->size())) {
                *CurProtoScroll = static_cast<int>(CurItemProtos->size()) - 1;
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
        _curMap->RefreshMap();
    }
    else if (IsCurInRect(IntBShowScen, IntX, IntY)) {
        Settings.ShowScen = !Settings.ShowScen;
        _curMap->RefreshMap();
    }
    else if (IsCurInRect(IntBShowWall, IntX, IntY)) {
        Settings.ShowWall = !Settings.ShowWall;
        _curMap->RefreshMap();
    }
    else if (IsCurInRect(IntBShowCrit, IntX, IntY)) {
        Settings.ShowCrit = !Settings.ShowCrit;
        _curMap->RefreshMap();
    }
    else if (IsCurInRect(IntBShowTile, IntX, IntY)) {
        Settings.ShowTile = !Settings.ShowTile;
        _curMap->RefreshMap();
    }
    else if (IsCurInRect(IntBShowRoof, IntX, IntY)) {
        Settings.ShowRoof = !Settings.ShowRoof;
        _curMap->RefreshMap();
    }
    else if (IsCurInRect(IntBShowFast, IntX, IntY)) {
        Settings.ShowFast = !Settings.ShowFast;
        _curMap->RefreshMap();
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
    STACK_TRACE_ENTRY();

    if (IntHold == INT_SELECT && _curMap->GetHexAtScreenPos(Settings.MousePos, SelectHex2, nullptr)) {
        if (CurMode == CUR_MODE_DEFAULT) {
            if (SelectHex1 != SelectHex2) {
                _curMap->ClearHexTrack();

                vector<mpos> hexes;

                if (SelectType == SELECT_TYPE_OLD) {
                    const int fx = std::min(SelectHex1.x, SelectHex2.x);
                    const int tx = std::max(SelectHex1.x, SelectHex2.x);
                    const int fy = std::min(SelectHex1.y, SelectHex2.y);
                    const int ty = std::max(SelectHex1.y, SelectHex2.y);

                    for (int i = fx; i <= tx; i++) {
                        for (int j = fy; j <= ty; j++) {
                            hexes.emplace_back(static_cast<uint16>(i), static_cast<uint16>(j));
                        }
                    }
                }
                else {
                    hexes = _curMap->GetHexesRect(SelectHex1, SelectHex2);
                }

                vector<ItemHexView*> items;
                vector<CritterHexView*> critters;
                for (const auto hex : hexes) {
                    // Items, critters
                    const auto& hex_items = _curMap->GetItems(hex);
                    items.insert(items.end(), hex_items.begin(), hex_items.end());

                    auto hex_critters = _curMap->GetCritters(hex, CritterFindType::Any);
                    critters.insert(critters.end(), hex_critters.begin(), hex_critters.end());

                    // Tile, roof
                    if (IsSelectTile && Settings.ShowTile) {
                        SelectAddTile(hex, false);
                    }
                    if (IsSelectRoof && Settings.ShowRoof) {
                        SelectAddTile(hex, true);
                    }
                }

                for (auto* item : items) {
                    const auto pid = item->GetProtoId();
                    if (_curMap->IsIgnorePid(pid)) {
                        continue;
                    }
                    if (!Settings.ShowFast && _curMap->IsFastPid(pid)) {
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
                    else if (Settings.ShowFast && _curMap->IsFastPid(pid)) {
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
                auto* entity = _curMap->GetEntityAtScreenPos(Settings.MousePos, 0, true);

                if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
                    if (!_curMap->IsIgnorePid(item->GetProtoId())) {
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

        _curMap->RefreshMap();
    }

    IntHold = INT_NONE;
}

void FOMapper::IntMouseMove()
{
    STACK_TRACE_ENTRY();

    if (IntHold == INT_SELECT) {
        _curMap->ClearHexTrack();
        if (!_curMap->GetHexAtScreenPos(Settings.MousePos, SelectHex2, nullptr)) {
            if (SelectHex2 != mpos {0, 0}) {
                _curMap->RefreshMap();
                SelectHex2 = {};
            }
            return;
        }

        if (CurMode == CUR_MODE_DEFAULT) {
            if (SelectHex1 != SelectHex2) {
                if (SelectType == SELECT_TYPE_OLD) {
                    const int fx = std::min(SelectHex1.x, SelectHex2.x);
                    const int tx = std::max(SelectHex1.x, SelectHex2.x);
                    const int fy = std::min(SelectHex1.y, SelectHex2.y);
                    const int ty = std::max(SelectHex1.y, SelectHex2.y);

                    for (auto i = fx; i <= tx; i++) {
                        for (auto j = fy; j <= ty; j++) {
                            _curMap->GetHexTrack({static_cast<uint16>(i), static_cast<uint16>(j)}) = 1;
                        }
                    }
                }
                else if (SelectType == SELECT_TYPE_NEW) {
                    for (const auto hex : _curMap->GetHexesRect(SelectHex1, SelectHex2)) {
                        _curMap->GetHexTrack(hex) = 1;
                    }
                }

                _curMap->RefreshMap();
            }
        }
        else if (CurMode == CUR_MODE_MOVE_SELECTION) {
            auto offs_hx = static_cast<int>(SelectHex2.x) - static_cast<int>(SelectHex1.x);
            auto offs_hy = static_cast<int>(SelectHex2.y) - static_cast<int>(SelectHex1.y);
            auto offs_x = Settings.MousePos.x - SelectPos.x;
            auto offs_y = Settings.MousePos.y - SelectPos.y;

            if (SelectMove(!Keyb.ShiftDwn, offs_hx, offs_hy, offs_x, offs_y)) {
                SelectHex1.x = static_cast<uint16>(SelectHex1.x + offs_hx);
                SelectHex1.y = static_cast<uint16>(SelectHex1.y + offs_hy);
                SelectPos.x += offs_x;
                SelectPos.y += offs_y;
                _curMap->RefreshMap();
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

    if (_curMap != nullptr) {
        // Update fast pids
        _curMap->ClearFastPids();
        for (const auto* fast_proto : TabsActive[INT_MODE_FAST]->ItemProtos) {
            _curMap->AddFastPid(fast_proto->GetProtoId());
        }

        // Update ignore pids
        _curMap->ClearIgnorePids();
        for (const auto* ignore_proto : TabsActive[INT_MODE_IGNORE]->ItemProtos) {
            _curMap->AddIgnorePid(ignore_proto->GetProtoId());
        }

        // Refresh map
        _curMap->RefreshMap();
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
            UNREACHABLE_PLACE();
        }

        SubTabsX += IntX - SubTabsRect.Width() / 2;
        SubTabsY += IntY - SubTabsRect.Height();

        SubTabsX = std::max(SubTabsX, 0);
        if (SubTabsX + SubTabsRect.Width() > Settings.ScreenWidth) {
            SubTabsX -= SubTabsX + SubTabsRect.Width() - Settings.ScreenWidth;
        }

        SubTabsY = std::max(SubTabsY, 0);
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

void FOMapper::MoveEntity(ClientEntity* entity, mpos hex)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!_curMap->GetSize().IsValidPos(hex)) {
        return;
    }

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        _curMap->MoveCritter(cr, hex, false);
    }
    else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        _curMap->MoveItem(item, hex);
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
        _curMap->DestroyCritter(cr);
    }
    else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        _curMap->DestroyItem(item);
    }
}

void FOMapper::SelectClear()
{
    STACK_TRACE_ENTRY();

    // Delete intersected tiles
    for (auto* entity : SelectedEntities) {
        if (const auto* tile = dynamic_cast<ItemHexView*>(entity); tile != nullptr && tile->GetIsTile()) {
            for (auto* sibling_tile : copy(_curMap->GetTiles(tile->GetHex(), tile->GetIsRoofTile()))) {
                const auto is_sibling_selected = std::find(SelectedEntities.begin(), SelectedEntities.end(), sibling_tile) != SelectedEntities.end();
                if (!is_sibling_selected && sibling_tile->GetTileLayer() == tile->GetTileLayer()) {
                    _curMap->DestroyItem(sibling_tile);
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

void FOMapper::SelectAddTile(mpos hex, bool is_roof)
{
    STACK_TRACE_ENTRY();

    const auto& field = _curMap->GetField(hex);
    const auto& tiles = is_roof ? field.RoofTiles : field.GroundTiles;

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
            hex_view->SetTargetAlpha(SelectAlpha);
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

    for (auto* item : _curMap->GetItems()) {
        if (_curMap->IsIgnorePid(item->GetProtoId())) {
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
        for (auto* cr : _curMap->GetCritters()) {
            SelectAddCrit(cr);
        }
    }

    _curMap->RefreshMap();
}

auto FOMapper::SelectMove(bool hex_move, int& offs_hx, int& offs_hy, int& offs_x, int& offs_y) -> bool
{
    STACK_TRACE_ENTRY();

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
            switcher = std::abs(cr->GetHex().x % 2);
        }
        else if (const auto* item = dynamic_cast<ItemHexView*>(SelectedEntities[0]); item != nullptr) {
            switcher = std::abs(item->GetHex().x % 2);
        }
    }

    // Change moving speed on zooming
    if (!hex_move) {
        static auto small_ox = 0.0f;
        static auto small_oy = 0.0f;
        const auto ox = static_cast<float>(offs_x) * _curMap->GetSpritesZoom() + small_ox;
        const auto oy = static_cast<float>(offs_y) * _curMap->GetSpritesZoom() + small_oy;

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
            ipos raw_hex;

            if (const auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
                raw_hex = ipos {cr->GetHex().x, cr->GetHex().y};
            }
            else if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
                raw_hex = ipos {item->GetHex().x, item->GetHex().y};
            }

            if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
                auto sw = switcher;
                for (auto k = 0; k < std::abs(offs_hx); k++, sw++) {
                    GeometryHelper::MoveHexByDirUnsafe(raw_hex, offs_hx > 0 ? ((sw % 2) != 0 ? 4u : 3u) : ((sw % 2) != 0 ? 0u : 1u));
                }
                for (auto k = 0; k < std::abs(offs_hy); k++) {
                    GeometryHelper::MoveHexByDirUnsafe(raw_hex, offs_hy > 0 ? 2u : 5u);
                }
            }
            else {
                raw_hex.x += offs_hx;
                raw_hex.y += offs_hy;
            }

            if (!_curMap->GetSize().IsValidPos(raw_hex)) {
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

            auto ox = item->GetOffset().x + offs_x;
            auto oy = item->GetOffset().y + offs_y;

            if (Keyb.AltDwn) {
                ox = oy = 0;
            }

            item->SetOffset({static_cast<int16>(ox), static_cast<int16>(oy)});
            item->RefreshAnim();
        }
        else {
            ipos raw_hex;

            if (const auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
                raw_hex = ipos {cr->GetHex().x, cr->GetHex().y};
            }
            else if (const auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
                raw_hex = ipos {item->GetHex().x, item->GetHex().y};
            }

            if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
                auto sw = switcher;
                for (auto k = 0, l = std::abs(offs_hx); k < l; k++, sw++) {
                    GeometryHelper::MoveHexByDirUnsafe(raw_hex, offs_hx > 0 ? ((sw % 2) != 0 ? 4 : 3) : ((sw % 2) != 0 ? 0 : 1));
                }
                for (auto k = 0, l = std::abs(offs_hy); k < l; k++) {
                    GeometryHelper::MoveHexByDirUnsafe(raw_hex, offs_hy > 0 ? 2 : 5);
                }
            }
            else {
                raw_hex.x += offs_hx;
                raw_hex.y += offs_hy;
            }

            const mpos hex = _curMap->GetSize().ClampPos(raw_hex);

            if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
                _curMap->MoveItem(item, hex);
            }
            else if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
                _curMap->MoveCritter(cr, hex, false);
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

    if (_curMap == nullptr) {
        return;
    }

    for (auto* entity : copy_hold_ref(SelectedEntities)) {
        DeleteEntity(entity);
    }

    SelectedEntities.clear();
    SelectClear();
    _curMap->RefreshMap();
    IntHold = INT_NONE;
    CurMode = CUR_MODE_DEFAULT;
}

auto FOMapper::CreateCritter(hstring pid, mpos hex) -> CritterView*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_curMap);

    if (!_curMap->GetSize().IsValidPos(hex)) {
        return nullptr;
    }

    if (_curMap->GetNonDeadCritter(hex) != nullptr) {
        return nullptr;
    }

    SelectClear();

    CritterHexView* cr = _curMap->AddMapperCritter(pid, hex, GeometryHelper::DirToAngle(NpcDir), nullptr);

    SelectAdd(cr);

    _curMap->RefreshMap();
    CurMode = CUR_MODE_DEFAULT;

    return cr;
}

auto FOMapper::CreateItem(hstring pid, mpos hex, Entity* owner) -> ItemView*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_curMap);

    // Checks
    const auto* proto = ProtoMngr.GetProtoItem(pid);

    mpos corrected_hex = hex;

    if (proto->GetIsTile()) {
        corrected_hex.x -= corrected_hex.x % Settings.MapTileStep;
        corrected_hex.y -= corrected_hex.y % Settings.MapTileStep;
    }

    if (owner == nullptr && (!_curMap->GetSize().IsValidPos(corrected_hex))) {
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
        item = _curMap->AddMapperTile(proto->GetProtoId(), corrected_hex, static_cast<uint8>(TileLayer), DrawRoof);
    }
    else {
        item = _curMap->AddMapperItem(proto->GetProtoId(), corrected_hex, nullptr);
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

    RUNTIME_ASSERT(_curMap);

    if (const auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        auto* cr_clone = _curMap->AddMapperCritter(cr->GetProtoId(), cr->GetHex(), cr->GetDirAngle(), &cr->GetProperties());

        for (const auto* inv_item : cr->GetConstInvItems()) {
            auto* inv_item_clone = cr_clone->AddMapperInvItem(_curMap->GenTempEntityId(), dynamic_cast<const ProtoItem*>(inv_item->GetProto()), inv_item->GetCritterSlot(), {});
            CloneInnerItems(inv_item_clone, inv_item);
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
    for (const auto* inner_item : from_item->GetConstInnerItems()) {
        const auto stack_id = any_t {string(inner_item->GetContainerStack())};
        auto* inner_item_clone = to_item->AddMapperInnerItem(_curMap->GenTempEntityId(), dynamic_cast<const ProtoItem*>(inner_item->GetProto()), stack_id, &from_item->GetProperties());
        CloneInnerItems(inner_item_clone, inner_item);
    }
}

void FOMapper::BufferCopy()
{
    STACK_TRACE_ENTRY();

    if (_curMap == nullptr) {
        return;
    }

    BufferRawHex = _curMap->GetScreenRawHex();

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
        entity_buf->Props = SafeAlloc::MakeRaw<Properties>(entity->GetProperties().Copy());

        for (auto* child : GetEntityInnerItems(entity)) {
            auto* child_buf = SafeAlloc::MakeRaw<EntityBuf>();
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

    if (_curMap == nullptr) {
        return;
    }

    BufferCopy();
    SelectDelete();
}

void FOMapper::BufferPaste()
{
    STACK_TRACE_ENTRY();

    if (_curMap == nullptr) {
        return;
    }

    const ipos screen_raw_hex = _curMap->GetScreenRawHex();
    const auto hx_offset = screen_raw_hex.x - BufferRawHex.x;
    const auto hy_offset = screen_raw_hex.y - BufferRawHex.y;

    SelectClear();

    for (const auto& entity_buf : EntitiesBuffer) {
        const auto raw_hx = static_cast<int>(entity_buf.Hex.x) + hx_offset;
        const auto raw_hy = static_cast<int>(entity_buf.Hex.y) + hy_offset;

        if (!_curMap->GetSize().IsValidPos(ipos {raw_hx, raw_hy})) {
            continue;
        }

        const mpos hex = _curMap->GetSize().FromRawPos(ipos {raw_hx, raw_hy});

        std::function<void(const EntityBuf*, ItemView*)> add_item_inner_items;

        add_item_inner_items = [&add_item_inner_items, this](const EntityBuf* item_entity_buf, ItemView* item) {
            for (const auto* child_buf : item_entity_buf->Children) {
                auto* inner_item = item->AddMapperInnerItem(_curMap->GenTempEntityId(), dynamic_cast<const ProtoItem*>(child_buf->Proto), {}, child_buf->Props);

                add_item_inner_items(child_buf, inner_item);
            }
        };

        if (entity_buf.IsCritter) {
            auto* cr = _curMap->AddMapperCritter(entity_buf.Proto->GetProtoId(), hex, 0, entity_buf.Props);

            for (const auto* child_buf : entity_buf.Children) {
                auto* inv_item = cr->AddMapperInvItem(_curMap->GenTempEntityId(), static_cast<const ProtoItem*>(child_buf->Proto), CritterItemSlot::Inventory, child_buf->Props);

                add_item_inner_items(child_buf, inv_item);
            }

            SelectAdd(cr);
        }
        else if (entity_buf.IsItem) {
            auto* item = _curMap->AddMapperItem(entity_buf.Proto->GetProtoId(), hex, entity_buf.Props);

            add_item_inner_items(&entity_buf, item);

            SelectAdd(item);
        }
    }
}

void FOMapper::DrawStr(const IRect& rect, string_view str, uint flags, ucolor color, int num_font)
{
    STACK_TRACE_ENTRY();

    SprMngr.DrawText({rect.Left, rect.Top, rect.Width(), rect.Height()}, str, flags, color, num_font);
}

void FOMapper::CurDraw()
{
    STACK_TRACE_ENTRY();

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
            const auto* proto_item = (*CurItemProtos)[GetTabIndex()];

            mpos hex;

            if (!_curMap->GetHexAtScreenPos(Settings.MousePos, hex, nullptr)) {
                break;
            }

            if (proto_item->GetIsTile()) {
                hex.x -= hex.x % Settings.MapTileStep;
                hex.y -= hex.y % Settings.MapTileStep;
            }

            const auto* spr = GetIfaceSpr(proto_item->GetPicMap());
            if (spr != nullptr) {
                auto x = _curMap->GetField(hex).Offset.x - (spr->Size.width / 2) + spr->Offset.x + (Settings.MapHexWidth / 2) + Settings.ScreenOffset.x + proto_item->GetOffset().x;
                auto y = _curMap->GetField(hex).Offset.y - spr->Size.height + spr->Offset.y + (Settings.MapHexHeight / 2) + Settings.ScreenOffset.y + proto_item->GetOffset().y;

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

                SprMngr.DrawSpriteSize(spr, {static_cast<int>(static_cast<float>(x) / _curMap->GetSpritesZoom()), static_cast<int>(static_cast<float>(y) / _curMap->GetSpritesZoom())}, //
                    {static_cast<int>(static_cast<float>(spr->Size.width) / _curMap->GetSpritesZoom()), static_cast<int>(static_cast<float>(spr->Size.height) / _curMap->GetSpritesZoom())}, true, false, COLOR_SPRITE);
            }
        }
        else if (IsCritMode() && !CurNpcProtos->empty()) {
            const auto model_name = (*CurNpcProtos)[GetTabIndex()]->GetModelName();
            const auto* anim = ResMngr.GetCritterPreviewSpr(model_name, CritterStateAnim::Unarmed, CritterActionAnim::Idle, NpcDir, nullptr);

            if (anim == nullptr) {
                anim = ResMngr.GetItemDefaultSpr().get();
            }

            mpos hex;

            if (!_curMap->GetHexAtScreenPos(Settings.MousePos, hex, nullptr)) {
                break;
            }

            const auto x = _curMap->GetField(hex).Offset.x - (anim->Size.width / 2) + anim->Offset.x;
            const auto y = _curMap->GetField(hex).Offset.y - anim->Size.height + anim->Offset.y;

            SprMngr.DrawSpriteSize(anim, //
                {static_cast<int>((static_cast<float>(x + Settings.ScreenOffset.x) + (static_cast<float>(Settings.MapHexWidth) / 2.0f)) / _curMap->GetSpritesZoom()), //
                    static_cast<int>((static_cast<float>(y + Settings.ScreenOffset.y) + (static_cast<float>(Settings.MapHexHeight) / 2.0f)) / _curMap->GetSpritesZoom())}, //
                {static_cast<int>(static_cast<float>(anim->Size.width) / _curMap->GetSpritesZoom()), //
                    static_cast<int>(static_cast<float>(anim->Size.height) / _curMap->GetSpritesZoom())},
                true, false, COLOR_SPRITE);
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

    return Settings.MousePos.x >= rect[0] + ax && Settings.MousePos.y >= rect[1] + ay && Settings.MousePos.x <= rect[2] + ax && Settings.MousePos.y <= rect[3] + ay;
}

auto FOMapper::IsCurInRect(const IRect& rect) const -> bool
{
    STACK_TRACE_ENTRY();

    return Settings.MousePos.x >= rect[0] && Settings.MousePos.y >= rect[1] && Settings.MousePos.x <= rect[2] && Settings.MousePos.y <= rect[3];
}

auto FOMapper::IsCurInRectNoTransp(const Sprite* spr, const IRect& rect, int ax, int ay) const -> bool
{
    STACK_TRACE_ENTRY();

    return IsCurInRect(rect, ax, ay) && SprMngr.SpriteHitTest(spr, {Settings.MousePos.x - rect.Left - ax, Settings.MousePos.y - rect.Top - ay}, false);
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

auto FOMapper::GetCurHex(mpos& hex, bool ignore_interface) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    hex = {};

    if (!ignore_interface && IsCurInInterface()) {
        return false;
    }
    return _curMap->GetHexAtScreenPos(Settings.MousePos, hex, nullptr);
}

void FOMapper::ConsoleDraw()
{
    STACK_TRACE_ENTRY();

    if (ConsoleEdit) {
        SprMngr.DrawSprite(ConsolePic.get(), {IntX + ConsolePicX, (IntVisible ? IntY : Settings.ScreenHeight) + ConsolePicY}, COLOR_SPRITE);

        auto str = ConsoleStr;
        str.insert(ConsoleCur, timespan(GameTime.GetFrameTime().duration_value()).to_ms<uint>() % 800 < 400 ? "!" : ".");
        DrawStr(IRect(IntX + ConsoleTextX, (IntVisible ? IntY : Settings.ScreenHeight) + ConsoleTextY, Settings.ScreenWidth, Settings.ScreenHeight), str, FT_NOBREAK, COLOR_TEXT, FONT_DEFAULT);
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
        ConsoleKeyTime = GameTime.GetFrameTime();
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

    if ((GameTime.GetFrameTime() - ConsoleKeyTime).to_ms<int>() >= CONSOLE_KEY_TICK - ConsoleAccelerate) {
        ConsoleKeyTime = GameTime.GetFrameTime();
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

        if (_curMap == nullptr) {
            AddMess("Map not loaded");
            return;
        }

        SaveMap(_curMap, map_name);

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

        string str = strex(command).substringAfter(' ').trim();
        if (!func(str)) {
            AddMess("Script execution fail");
            return;
        }

        AddMess(strex("Result: {}", func.GetResult()));
    }
    // Critter animations
    else if (command[0] == '@') {
        AddMess("Playing critter animations");

        if (_curMap == nullptr) {
            AddMess("Map not loaded");
            return;
        }

        vector<int> anims = strex(command.substr(1)).splitToInt(' ');
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
            for (auto* cr : _curMap->GetCritters()) {
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
            auto* pmap = SafeAlloc::MakeRaw<ProtoMap>(ToHashedString("new"), GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME));

            pmap->SetSize({MAXHEX_DEFAULT, MAXHEX_DEFAULT});

            // Morning	 5.00 -  9.59	 300 - 599
            // Day		10.00 - 18.59	 600 - 1139
            // Evening	19.00 - 22.59	1140 - 1379
            // Nigh		23.00 -  4.59	1380
            vector<int> arr = {300, 600, 1140, 1380};
            vector<uint8> arr2 = {18, 128, 103, 51, 18, 128, 95, 40, 53, 128, 86, 29};
            pmap->SetDayColorTime(arr);
            pmap->SetDayColor(arr2);

            auto* map = SafeAlloc::MakeRaw<MapView>(this, ident_t {}, pmap);

            map->EnableMapperMode();
            map->FindSetCenter({MAXHEX_DEFAULT / 2, MAXHEX_DEFAULT / 2});

            LoadedMaps.push_back(map);

            ShowMap(map);

            AddMess("Create map success");
        }
        else if (command_ext == "unload") {
            AddMess("Unload map");

            if (_curMap == nullptr) {
                AddMess("Map not loaded");
                return;
            }

            UnloadMap(_curMap);

            if (!LoadedMaps.empty()) {
                ShowMap(LoadedMaps.front());
            }
        }
        else if (command_ext == "size" && (_curMap != nullptr)) {
            AddMess("Resize map");

            if (_curMap == nullptr) {
                AddMess("Map not loaded");
                return;
            }

            int maxhx = 0;
            int maxhy = 0;
            if (!(icommand >> maxhx >> maxhy)) {
                AddMess("Invalid args");
                return;
            }

            ResizeMap(_curMap, static_cast<uint16>(maxhx), static_cast<uint16>(maxhy));
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

    auto map_data = ConfigFile(strex("{}.fomap", map_name), map_file_str, this, ConfigFileOption::ReadFirstSection);
    if (!map_data.HasSection("ProtoMap")) {
        throw MapLoaderException("Invalid map format", map_name);
    }

    auto* pmap = SafeAlloc::MakeRaw<ProtoMap>(ToHashedString(map_name), GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME));
    pmap->GetPropertiesForEdit().ApplyFromText(map_data.GetSection("ProtoMap"));

    auto new_map_holder = SafeAlloc::MakeUnique<MapView>(this, ident_t {}, pmap);
    auto* new_map = new_map_holder.get();

    new_map->EnableMapperMode();

    try {
        new_map->LoadFromFile(map_name, map_file.GetStr());
    }
    catch (const MapLoaderException& ex) {
        AddMess(strex("Map truncated: {}", ex.what()));
        return nullptr;
    }

    new_map->FindSetCenter(new_map->GetWorkHex());

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

    if (map == _curMap) {
        return;
    }

    SelectClear();

    _curMap = map;

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
            fomap_path = strex(fomap_file2.GetFullPath()).changeFileName(fomap_name);
        }
        else if (fomap_files.MoveNext()) {
            fomap_path = strex(fomap_files.GetCurFile().GetFullPath()).changeFileName(fomap_name);
        }
        else {
            fomap_path = strex("{}.fomap", fomap_path).formatPath();
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

    if (map == _curMap) {
        SelectClear();
        _curMap = nullptr;
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

    const auto corrected_width = std::clamp(width, MAXHEX_MIN, MAXHEX_MAX);
    const auto corrected_height = std::clamp(height, MAXHEX_MIN, MAXHEX_MAX);

    auto work_pos = map->GetWorkHex();
    work_pos.x = std::min(work_pos.x, static_cast<uint16>(corrected_width - 1));
    work_pos.y = std::min(work_pos.y, static_cast<uint16>(corrected_height - 1));
    map->SetWorkHex(work_pos);

    map->Resize({corrected_width, corrected_height});

    map->FindSetCenter(work_pos);

    if (map == _curMap) {
        SelectClear();
    }
}

void FOMapper::AddMess(string_view message_text)
{
    STACK_TRACE_ENTRY();

    const string str = strex("|{} - {}\n", COLOR_TEXT, message_text);
    const auto time = nanotime::now().desc();
    const string mess_time = strex("{:02}:{:02}:{:02} ", time.hour, time.minute, time.second);

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

    DrawStr(IRect(IntWWork[0] + IntX, IntWWork[1] + IntY, IntWWork[2] + IntX, IntWWork[3] + IntY), MessBoxCurText, FT_UPPER | FT_BOTTOM, COLOR_TEXT, FONT_DEFAULT);
}

void FOMapper::DrawIfaceLayer(uint layer)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(layer);

    SpritesCanDraw = true;
    OnRenderIface.Fire(); // Todo: mapper render iface layer
    SpritesCanDraw = false;
}

auto FOMapper::GetEntityInnerItems(ClientEntity* entity) const -> vector<ItemView*>
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
