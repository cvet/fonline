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

#include "Mapper.h"
#include "3dStuff.h"
#include "DiskFileSystem.h"
#include "GenericUtils.h"
#include "Log.h"
#include "MessageBox.h"
#include "StringUtils.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

#include "sha1.h"
#include "sha2.h"

FOMapper::FOMapper(GlobalSettings& settings) : Settings {settings}, GameTime(Settings), GeomHelper(Settings), IfaceIni(""), ScriptSys(this, settings, FileMngr), Cache("Data/Cache.fobin"), EffectMngr(Settings, FileMngr, GameTime), SprMngr(Settings, FileMngr, EffectMngr, ScriptSys, GameTime), ResMngr(FileMngr, SprMngr, ScriptSys), HexMngr(true, Settings, ProtoMngr, SprMngr, EffectMngr, ResMngr, ScriptSys, GameTime), Keyb(Settings, SprMngr)
{
    Animations.resize(10000);

    // Mouse
    auto mx = 0;
    auto my = 0;
    App->Window.GetMousePosition(mx, my);
    Settings.MouseX = std::clamp(mx, 0, Settings.ScreenWidth - 1);
    Settings.MouseY = std::clamp(my, 0, Settings.ScreenHeight - 1);

    // Setup write paths
    ServerWritePath = Settings.ServerDir;
    ClientWritePath = Settings.WorkDir;

    // Resources
    FileMngr.AddDataSource("$Basic", false);
    FileMngr.AddDataSource(ClientWritePath + "Data/", false);
    ServerFileMngr.AddDataSource("$Basic", false);
    ServerFileMngr.AddDataSource(ServerWritePath, false);

    // Default effects
    EffectMngr.LoadDefaultEffects();

    // Fonts
    SprMngr.PushAtlasType(AtlasType::Static);
    auto load_fonts_ok = true;
    if (!SprMngr.LoadFontFO(FONT_FO, "OldDefault", false, true) || !SprMngr.LoadFontFO(FONT_NUM, "Numbers", true, true) || !SprMngr.LoadFontFO(FONT_BIG_NUM, "BigNumbers", true, true) || !SprMngr.LoadFontFO(FONT_SAND_NUM, "SandNumbers", false, true) || !SprMngr.LoadFontFO(FONT_SPECIAL, "Special", false, true) || !SprMngr.LoadFontFO(FONT_DEFAULT, "Default", false, true) || !SprMngr.LoadFontFO(FONT_THIN, "Thin", false, true) || !SprMngr.LoadFontFO(FONT_FAT, "Fat", false, true) || !SprMngr.LoadFontFO(FONT_BIG, "Big", false, true)) {
        load_fonts_ok = false;
    }
    RUNTIME_ASSERT(load_fonts_ok);
    SprMngr.BuildFonts();
    SprMngr.SetDefaultFont(FONT_DEFAULT, COLOR_TEXT);

    SprMngr.BeginScene(COLOR_RGB(100, 100, 100));
    SprMngr.EndScene();

    const auto init_face_ok = (InitIface() == 0);
    RUNTIME_ASSERT(init_face_ok);

    // Language Packs
    CurLang.LoadFromFiles(FileMngr, Settings.Language);

    // Prototypes
    // bool protos_ok = ProtoMngr.LoadProtosFromFiles(FileMngr);
    // RUNTIME_ASSERT(protos_ok);

    // Initialize tabs
    const auto& cr_protos = ProtoMngr.GetProtoCritters();
    for (const auto& [pid, proto] : cr_protos) {
        Tabs[INT_MODE_CRIT][DEFAULT_SUB_TAB].NpcProtos.push_back(proto);
        Tabs[INT_MODE_CRIT][proto->CollectionName].NpcProtos.push_back(proto);
    }
    for (auto& [pid, proto] : Tabs[INT_MODE_CRIT]) {
        std::sort(proto.NpcProtos.begin(), proto.NpcProtos.end(), [](ProtoCritter* a, ProtoCritter* b) { return a->GetName().compare(b->GetName()); });
    }

    const auto& item_protos = ProtoMngr.GetProtoItems();
    for (const auto& [pid, proto] : item_protos) {
        Tabs[INT_MODE_ITEM][DEFAULT_SUB_TAB].ItemProtos.push_back(proto);
        Tabs[INT_MODE_ITEM][proto->CollectionName].ItemProtos.push_back(proto);
    }
    for (auto& [pid, proto] : Tabs[INT_MODE_ITEM]) {
        std::sort(proto.ItemProtos.begin(), proto.ItemProtos.end(), [](ProtoItem* a, ProtoItem* b) { return a->GetName().compare(b->GetName()); });
    }

    for (auto i = 0; i < TAB_COUNT; i++) {
        if (Tabs[i].empty()) {
            Tabs[i][DEFAULT_SUB_TAB].Scroll = 0;
        }
        TabsActive[i] = &(*Tabs[i].begin()).second;
    }

    // Initialize tabs scroll and names
    std::memset(TabsScroll, 0, sizeof(TabsScroll));
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

    // Hex manager
    HexMngr.ReloadSprites();
    HexMngr.SwitchShowTrack();
    ChangeGameTime();

    if (!Settings.StartMap.empty()) {
        const auto map_name = Settings.StartMap;
        auto* pmap = new ProtoMap(_str(map_name).toHash());
        const bool initialized = 0;
        // pmap->EditorLoad(ServerFileMngr, ProtoMngr, SprMngr, ResMngr); // Todo: need attention!

        if (initialized && HexMngr.SetProtoMap(*pmap)) {
            auto hexX = Settings.StartHexX;
            auto hexY = Settings.StartHexY;
            if (hexX < 0 || hexX >= pmap->GetWidth()) {
                hexX = pmap->GetWorkHexX();
            }
            if (hexY < 0 || hexY >= pmap->GetHeight()) {
                hexY = pmap->GetWorkHexY();
            }
            HexMngr.FindSetCenter(hexX, hexY);

            auto* map = new MapView(0, pmap);
            ActiveMap = map;
            LoadedMaps.push_back(map);
            RunMapLoadScript(map);
        }
    }

    // Start script
    RunStartScript();

    // Refresh resources after start script executed
    for (auto tab = 0; tab < TAB_COUNT; tab++) {
        RefreshTiles(tab);
    }
    RefreshCurProtos();

    // Load console history
    const auto history_str = Cache.GetString("mapper_console.txt");
    size_t pos = 0;
    size_t prev = 0;
    size_t count = 0;
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

auto FOMapper::InitIface() -> int
{
    WriteLog("Init interface.\n");

    auto& ini = IfaceIni;

    ini = FileMngr.ReadConfigFile("mapper_default.ini");
    if (!ini) {
        WriteLog("File 'mapper_default.ini' not found.\n");
        return __LINE__;
    }

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

    ResMngr.ItemHexDefaultAnim = SprMngr.LoadAnimation(ini.GetStr("", "ItemStub", "art/items/reserved.frm"), true, false);
    ResMngr.CritterDefaultAnim = SprMngr.LoadAnimation(ini.GetStr("", "CritterStub", "art/critters/reservaa.frm"), true, false);

    // Cursor
    CurPDef = SprMngr.LoadAnimation(ini.GetStr("", "CurDefault", "actarrow.frm"), true, false);
    CurPHand = SprMngr.LoadAnimation(ini.GetStr("", "CurHand", "hand.frm"), true, false);

    // Iface
    IntMainPic = SprMngr.LoadAnimation(ini.GetStr("", "IntMainPic", "error"), true, false);
    IntPTab = SprMngr.LoadAnimation(ini.GetStr("", "IntTabPic", "error"), true, false);
    IntPSelect = SprMngr.LoadAnimation(ini.GetStr("", "IntSelectPic", "error"), true, false);
    IntPShow = SprMngr.LoadAnimation(ini.GetStr("", "IntShowPic", "error"), true, false);

    // Object
    ObjWMainPic = SprMngr.LoadAnimation(ini.GetStr("", "ObjMainPic", "error"), true, false);
    ObjPbToAllDn = SprMngr.LoadAnimation(ini.GetStr("", "ObjToAllPicDn", "error"), true, false);

    // Sub tabs
    SubTabsPic = SprMngr.LoadAnimation(ini.GetStr("", "SubTabsPic", "error"), true, false);

    // Console
    ConsolePic = SprMngr.LoadAnimation(ini.GetStr("", "ConsolePic", "error"), true, false);

    WriteLog("Init interface complete.\n");
    return 0;
}

auto FOMapper::IfaceLoadRect(Rect& comp, const char* name) -> bool
{
    const auto res = IfaceIni.GetStr("", name);
    if (res.empty()) {
        WriteLog("Signature '{}' not found.\n", name);
        return false;
    }

    if (sscanf(res.c_str(), "%d%d%d%d", &comp[0], &comp[1], &comp[2], &comp[3]) != 4) {
        comp.Clear();
        WriteLog("Unable to parse signature '{}'.\n", name);
        return false;
    }

    return true;
}

void FOMapper::ChangeGameTime()
{
    const auto color = GenericUtils::GetColorDay(HexMngr.GetMapDayTime(), HexMngr.GetMapDayColor(), HexMngr.GetMapTime(), nullptr);
    SprMngr.SetSpritesColor(COLOR_GAME_RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));
    if (HexMngr.IsMapLoaded()) {
        HexMngr.RefreshMap();
    }
}

auto FOMapper::AnimLoad(uint name_hash, AtlasType res_type) -> uint
{
    auto* anim = ResMngr.GetAnim(name_hash, res_type);
    if (anim == nullptr) {
        return 0;
    }
    auto* ianim = new IfaceAnim {anim, res_type, GameTime.GameTick()};
    if (ianim == nullptr) {
        return 0;
    }

    uint index = 1;
    for (const auto j = static_cast<uint>(Animations.size()); index < j; index++) {
        if (Animations[index] == nullptr) {
            break;
        }
    }
    if (index < static_cast<uint>(Animations.size())) {
        Animations[index] = ianim;
    }
    else {
        Animations.push_back(ianim);
    }
    return index;
}

auto FOMapper::AnimLoad(const char* fname, AtlasType res_type) -> uint
{
    auto* anim = ResMngr.GetAnim(_str(fname).toHash(), res_type);
    if (anim == nullptr) {
        return 0;
    }
    auto* ianim = new IfaceAnim {anim, res_type, GameTime.GameTick()};
    if (ianim == nullptr) {
        return 0;
    }

    uint index = 1;
    for (const auto j = static_cast<uint>(Animations.size()); index < j; index++) {
        if (Animations[index] == nullptr) {
            break;
        }
    }
    if (index < static_cast<uint>(Animations.size())) {
        Animations[index] = ianim;
    }
    else {
        Animations.push_back(ianim);
    }
    return index;
}

auto FOMapper::AnimGetCurSpr(uint anim_id) -> uint
{
    if (anim_id >= Animations.size() || (Animations[anim_id] == nullptr)) {
        return 0;
    }
    return Animations[anim_id]->Frames->Ind[Animations[anim_id]->CurSpr];
}

auto FOMapper::AnimGetCurSprCnt(uint anim_id) -> uint
{
    if (anim_id >= Animations.size() || (Animations[anim_id] == nullptr)) {
        return 0;
    }
    return Animations[anim_id]->CurSpr;
}

auto FOMapper::AnimGetSprCount(uint anim_id) -> uint
{
    if (anim_id >= Animations.size() || (Animations[anim_id] == nullptr)) {
        return 0;
    }
    return Animations[anim_id]->Frames->CntFrm;
}

auto FOMapper::AnimGetFrames(uint anim_id) -> AnyFrames*
{
    if (anim_id >= Animations.size() || (Animations[anim_id] == nullptr)) {
        return 0;
    }
    return Animations[anim_id]->Frames;
}

void FOMapper::AnimRun(uint anim_id, uint flags)
{
    if (anim_id >= Animations.size() || (Animations[anim_id] == nullptr)) {
        return;
    }

    auto* anim = Animations[anim_id];

    // Set flags
    anim->Flags = flags & 0xFFFF;
    flags >>= 16;

    // Set frm
    uchar cur_frm = flags & 0xFF;
    if (cur_frm > 0) {
        cur_frm--;
        if (cur_frm >= anim->Frames->CntFrm) {
            cur_frm = anim->Frames->CntFrm - 1;
        }
        anim->CurSpr = cur_frm;
    }
    // flags>>=8;
}

void FOMapper::AnimProcess()
{
    for (auto* anim : Animations) {
        if (anim == nullptr || anim->Flags == 0u) {
            continue;
        }

        if (IsBitSet(anim->Flags, ANIMRUN_STOP)) {
            anim->Flags = 0;
            continue;
        }

        if (IsBitSet(anim->Flags, ANIMRUN_TO_END) || IsBitSet(anim->Flags, ANIMRUN_FROM_END)) {
            const auto cur_tick = GameTime.FrameTick();
            if (cur_tick - anim->LastTick < anim->Frames->Ticks / anim->Frames->CntFrm) {
                continue;
            }

            anim->LastTick = cur_tick;
            auto end_spr = anim->Frames->CntFrm - 1;
            if (IsBitSet(anim->Flags, ANIMRUN_FROM_END)) {
                end_spr = 0;
            }

            if (anim->CurSpr < end_spr) {
                anim->CurSpr++;
            }
            else if (anim->CurSpr > end_spr) {
                anim->CurSpr--;
            }
            else {
                if (IsBitSet(anim->Flags, ANIMRUN_CYCLE)) {
                    if (IsBitSet(anim->Flags, ANIMRUN_TO_END)) {
                        anim->CurSpr = 0;
                    }
                    else {
                        anim->CurSpr = end_spr;
                    }
                }
                else {
                    anim->Flags = 0;
                }
            }
        }
    }
}

void FOMapper::AnimFree(AtlasType res_type)
{
    ResMngr.FreeResources(res_type);
    for (auto& Animation : Animations) {
        auto* anim = Animation;
        if ((anim != nullptr) && anim->ResType == res_type) {
            delete anim;
            Animation = nullptr;
        }
    }
}

void FOMapper::ProcessInputEvents()
{
    // Stop processing if window not active
    if (!SprMngr.IsWindowFocused()) {
        InputEvent event;
        while (App->Input.PollEvent(event)) {
        }

        Keyb.Lost();
        ScriptSys.InputLostEvent();
        return;
    }

    InputEvent event;
    while (App->Input.PollEvent(event)) {
        ProcessInputEvent(event);
    }
}

void FOMapper::ProcessInputEvent(const InputEvent& event)
{
    // Process events
    /*for (uint i = 0; i < events.size(); i += 2)
    {
        // Event data
        int event = events[i];
        int event_key = events[i + 1];
        const char* event_text = events_text[i / 2].c_str();

        // Keys codes mapping
        uchar dikdw = 0;
        uchar dikup = 0;
        if (event == SDL_KEYDOWN)
            dikdw = Keyb.MapKey(event_key);
        else if (event == SDL_KEYUP)
            dikup = Keyb.MapKey(event_key);
        if (!dikdw && !dikup)
            continue;

        // Avoid repeating
        static bool key_pressed[0x100];
        if (dikdw && key_pressed[dikdw])
            continue;
        if (dikup && !key_pressed[dikup])
            continue;

        // Keyboard states, to know outside function
        key_pressed[dikup] = false;
        key_pressed[dikdw] = true;

        // Key script event
        bool script_result = true;
        if (dikdw)
        {
            string event_text_script = event_text;
            script_result = ScriptSys.KeyDownEvent(dikdw, event_text_script);
        }
        if (dikup)
        {
            string event_text_script = event_text;
            script_result = ScriptSys.KeyUpEvent(dikup, event_text_script);
        }

        // Disable keyboard events
        if (!script_result || Settings.DisableKeyboardEvents)
        {
            if (dikdw == KeyCode::DIK_ESCAPE && Keyb.ShiftDwn)
                Settings.Quit = true;
            continue;
        }

        // Control keys
        if (dikdw == KeyCode::DIK_RCONTROL || dikdw == KeyCode::DIK_LCONTROL)
            Keyb.CtrlDwn = true;
        else if (dikdw == KeyCode::DIK_LMENU || dikdw == KeyCode::DIK_RMENU)
            Keyb.AltDwn = true;
        else if (dikdw == KeyCode::DIK_LSHIFT || dikdw == KeyCode::DIK_RSHIFT)
            Keyb.ShiftDwn = true;
        if (dikup == KeyCode::DIK_RCONTROL || dikup == KeyCode::DIK_LCONTROL)
            Keyb.CtrlDwn = false;
        else if (dikup == KeyCode::DIK_LMENU || dikup == KeyCode::DIK_RMENU)
            Keyb.AltDwn = false;
        else if (dikup == KeyCode::DIK_LSHIFT || dikup == KeyCode::DIK_RSHIFT)
            Keyb.ShiftDwn = false;

        // Hotkeys
        if (!Keyb.AltDwn && !Keyb.CtrlDwn && !Keyb.ShiftDwn)
        {
            switch (dikdw)
            {
            case KeyCode::DIK_F1:
                Settings.ShowItem = !Settings.ShowItem;
                HexMngr.RefreshMap();
                break;
            case KeyCode::DIK_F2:
                Settings.ShowScen = !Settings.ShowScen;
                HexMngr.RefreshMap();
                break;
            case KeyCode::DIK_F3:
                Settings.ShowWall = !Settings.ShowWall;
                HexMngr.RefreshMap();
                break;
            case KeyCode::DIK_F4:
                Settings.ShowCrit = !Settings.ShowCrit;
                HexMngr.RefreshMap();
                break;
            case KeyCode::DIK_F5:
                Settings.ShowTile = !Settings.ShowTile;
                HexMngr.RefreshMap();
                break;
            case KeyCode::DIK_F6:
                Settings.ShowFast = !Settings.ShowFast;
                HexMngr.RefreshMap();
                break;
            case KeyCode::DIK_F7:
                IntVisible = !IntVisible;
                break;
            case KeyCode::DIK_F8:
                Settings.MouseScroll = !Settings.MouseScroll;
                break;
            case KeyCode::DIK_F9:
                ObjVisible = !ObjVisible;
                break;
            case KeyCode::DIK_F10:
                HexMngr.SwitchShowHex();
                break;

            // Fullscreen
            case KeyCode::DIK_F11:
                if (!Settings.FullScreen)
                {
                    if (SprMngr.EnableFullscreen())
                        Settings.FullScreen = true;
                }
                else
                {
                    if (SprMngr.DisableFullscreen())
                        Settings.FullScreen = false;
                }
                SprMngr.RefreshViewport();
                continue;
            // Minimize
            case KeyCode::DIK_F12:
                SprMngr.MinimizeWindow();
                continue;

            case KeyCode::DIK_DELETE:
                SelectDelete();
                break;
            case KeyCode::DIK_ADD:
                if (!ConsoleEdit && SelectedEntities.empty())
                {
                    int day_time = HexMngr.GetDayTime();
                    day_time += 60;
                    Globals->SetMinute(day_time % 60);
                    Globals->SetHour(day_time / 60 % 24);
                    ChangeGameTime();
                }
                break;
            case KeyCode::DIK_SUBTRACT:
                if (!ConsoleEdit && SelectedEntities.empty())
                {
                    int day_time = HexMngr.GetDayTime();
                    day_time -= 60;
                    Globals->SetMinute(day_time % 60);
                    Globals->SetHour(day_time / 60 % 24);
                    ChangeGameTime();
                }
                break;
            case KeyCode::DIK_TAB:
                SelectType = (SelectType == SELECT_TYPE_OLD ? SELECT_TYPE_NEW : SELECT_TYPE_OLD);
                break;
            default:
                break;
            }
        }

        if (Keyb.ShiftDwn)
        {
            switch (dikdw)
            {
            case KeyCode::DIK_F7:
                IntFix = !IntFix;
                break;
            case KeyCode::DIK_F9:
                ObjFix = !ObjFix;
                break;
            case KeyCode::DIK_F10:
                HexMngr.SwitchShowRain();
                break;
            case KeyCode::DIK_F11:
                SprMngr.DumpAtlases();
                break;
            case KeyCode::DIK_ESCAPE:
                exit(0);
                break;
            case KeyCode::DIK_ADD:
                if (!ConsoleEdit && SelectedEntities.empty())
                {
                    int day_time = HexMngr.GetDayTime();
                    day_time += 1;
                    Globals->SetMinute(day_time % 60);
                    Globals->SetHour(day_time / 60 % 24);
                    ChangeGameTime();
                }
                break;
            case KeyCode::DIK_SUBTRACT:
                if (!ConsoleEdit && SelectedEntities.empty())
                {
                    int day_time = HexMngr.GetDayTime();
                    day_time -= 60;
                    Globals->SetMinute(day_time % 60);
                    Globals->SetHour(day_time / 60 % 24);
                    ChangeGameTime();
                }
                break;
            case KeyCode::DIK_0:
            case KeyCode::DIK_NUMPAD0:
                TileLayer = 0;
                break;
            case KeyCode::DIK_1:
            case KeyCode::DIK_NUMPAD1:
                TileLayer = 1;
                break;
            case KeyCode::DIK_2:
            case KeyCode::DIK_NUMPAD2:
                TileLayer = 2;
                break;
            case KeyCode::DIK_3:
            case KeyCode::DIK_NUMPAD3:
                TileLayer = 3;
                break;
            case KeyCode::DIK_4:
            case KeyCode::DIK_NUMPAD4:
                TileLayer = 4;
                break;
            default:
                break;
            }
        }

        if (Keyb.CtrlDwn)
        {
            switch (dikdw)
            {
            case KeyCode::DIK_X:
                BufferCut();
                break;
            case KeyCode::DIK_C:
                BufferCopy();
                break;
            case KeyCode::DIK_V:
                BufferPaste(50, 50);
                break;
            case KeyCode::DIK_A:
                SelectAll();
                break;
            case KeyCode::DIK_S:
                if (ActiveMap)
                {
                    HexMngr.GetProtoMap(*(ProtoMap*)ActiveMap->Proto);
                    // Todo: need attention!
                    // ((ProtoMap*)ActiveMap->Proto)->EditorSave(FileMngr, "");
                    AddMess("Map saved.");
                    RunMapSaveScript(ActiveMap);
                }
                break;
            case KeyCode::DIK_D:
                Settings.ScrollCheck = !Settings.ScrollCheck;
                break;
            case KeyCode::DIK_B:
                HexMngr.MarkPassedHexes();
                break;
            case KeyCode::DIK_Q:
                Settings.ShowCorners = !Settings.ShowCorners;
                break;
            case KeyCode::DIK_W:
                Settings.ShowSpriteCuts = !Settings.ShowSpriteCuts;
                break;
            case KeyCode::DIK_E:
                Settings.ShowDrawOrder = !Settings.ShowDrawOrder;
                break;
            case KeyCode::DIK_M:
                DrawCrExtInfo++;
                if (DrawCrExtInfo > DRAW_CR_INFO_MAX)
                    DrawCrExtInfo = 0;
                break;
            case KeyCode::DIK_L:
                SaveLogFile();
                break;
            default:
                break;
            }
        }

        // Key down
        if (dikdw)
        {
            if (ObjVisible && !SelectedEntities.empty())
            {
                ObjKeyDown(dikdw, event_text);
            }
            else
            {
                ConsoleKeyDown(dikdw, event_text);
                if (!ConsoleEdit)
                {
                    switch (dikdw)
                    {
                    case KeyCode::DIK_LEFT:
                        Settings.ScrollKeybLeft = true;
                        break;
                    case KeyCode::DIK_RIGHT:
                        Settings.ScrollKeybRight = true;
                        break;
                    case KeyCode::DIK_UP:
                        Settings.ScrollKeybUp = true;
                        break;
                    case KeyCode::DIK_DOWN:
                        Settings.ScrollKeybDown = true;
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        // Key up
        if (dikup)
        {
            ConsoleKeyUp(dikup);

            switch (dikup)
            {
            case KeyCode::DIK_LEFT:
                Settings.ScrollKeybLeft = false;
                break;
            case KeyCode::DIK_RIGHT:
                Settings.ScrollKeybRight = false;
                break;
            case KeyCode::DIK_UP:
                Settings.ScrollKeybUp = false;
                break;
            case KeyCode::DIK_DOWN:
                Settings.ScrollKeybDown = false;
                break;
            default:
                break;
            }
        }
    }
}

void FOMapper::ParseMouse()
{
    // Mouse position
    int mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    Settings.MouseX = std::clamp(mx, 0, Settings.ScreenWidth - 1);
    Settings.MouseY = std::clamp(my, 0, Settings.ScreenHeight - 1);

    // Stop processing if window not active
    if (!SprMngr.IsWindowFocused())
    {
        Settings.MainWindowMouseEvents.clear();
        IntHold = INT_NONE;
        ScriptSys.InputLostEvent();
        return;
    }

    // Mouse move
    if (Settings.LastMouseX != Settings.MouseX || Settings.LastMouseY != Settings.MouseY)
    {
        int ox = Settings.MouseX - Settings.LastMouseX;
        int oy = Settings.MouseY - Settings.LastMouseY;
        Settings.LastMouseX = Settings.MouseX;
        Settings.LastMouseY = Settings.MouseY;

        ScriptSys.MouseMoveEvent(ox, oy);

        IntMouseMove();
    }

    // Mouse Scroll
    if (Settings.MouseScroll)
    {
        if (Settings.MouseX >= Settings.ScreenWidth - 1)
            Settings.ScrollMouseRight = true;
        else
            Settings.ScrollMouseRight = false;

        if (Settings.MouseX <= 0)
            Settings.ScrollMouseLeft = true;
        else
            Settings.ScrollMouseLeft = false;

        if (Settings.MouseY >= Settings.ScreenHeight - 1)
            Settings.ScrollMouseDown = true;
        else
            Settings.ScrollMouseDown = false;

        if (Settings.MouseY <= 0)
            Settings.ScrollMouseUp = true;
        else
            Settings.ScrollMouseUp = false;
    }

    // Get buffered data
    if (Settings.MainWindowMouseEvents.empty())
        return;
    IntVec events = Settings.MainWindowMouseEvents;
    Settings.MainWindowMouseEvents.clear();

    // Process events
    for (uint i = 0; i < events.size(); i += 3)
    {
        int event = events[i];
        int event_button = events[i + 1];
        int event_dy = -events[i + 2];

        // Scripts
        bool script_result = true;
        if (event == SDL_MOUSEWHEEL)
            script_result = ScriptSys.MouseDownEvent(event_dy > 0 ? MOUSE_BUTTON_WHEEL_UP : MOUSE_BUTTON_WHEEL_DOWN);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON_LEFT)
            script_result = ScriptSys.MouseDownEvent(MOUSE_BUTTON_LEFT);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON_LEFT)
            script_result = ScriptSys.MouseUpEvent(MOUSE_BUTTON_LEFT);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON_RIGHT)
            script_result = ScriptSys.MouseDownEvent(MOUSE_BUTTON_RIGHT);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON_RIGHT)
            script_result = ScriptSys.MouseUpEvent(MOUSE_BUTTON_RIGHT);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON_MIDDLE)
            script_result = ScriptSys.MouseDown, MOUSE_BUTTON_MIDDLE);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON_MIDDLE)
            script_result = ScriptSys.MouseUp, MOUSE_BUTTON_MIDDLE);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON(4))
            script_result = ScriptSys.MouseDown, MOUSE_BUTTON_EXT0);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON(4))
            script_result = ScriptSys.MouseUp, MOUSE_BUTTON_EXT0);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON(5))
            script_result = ScriptSys.MouseDown, MOUSE_BUTTON_EXT1);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON(5))
            script_result = ScriptSys.MouseUp, MOUSE_BUTTON_EXT1);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON(6))
            script_result = ScriptSys.MouseDown, MOUSE_BUTTON_EXT2);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON(6))
            script_result = ScriptSys.MouseUp, MOUSE_BUTTON_EXT2);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON(7))
            script_result = ScriptSys.MouseDown, MOUSE_BUTTON_EXT3);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON(7))
            script_result = ScriptSys.MouseUp, MOUSE_BUTTON_EXT3);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON(8))
            script_result = ScriptSys.MouseDown, MOUSE_BUTTON_EXT4);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON(8))
            script_result = ScriptSys.MouseUp, MOUSE_BUTTON_EXT4);
        if (!script_result || Settings.DisableMouseEvents)
            continue;

        // Wheel
        if (event == SDL_MOUSEWHEEL)
        {
            if (IntVisible && SubTabsActive && IsCurInRect(SubTabsRect, SubTabsX, SubTabsY))
            {
                int step = 4;
                if (Keyb.ShiftDwn)
                    step = 8;
                else if (Keyb.CtrlDwn)
                    step = 20;
                else if (Keyb.AltDwn)
                    step = 50;

                int data = event_dy;
                if (data > 0)
                    TabsScroll[SubTabsActiveTab] += step;
                else
                    TabsScroll[SubTabsActiveTab] -= step;
                if (TabsScroll[SubTabsActiveTab] < 0)
                    TabsScroll[SubTabsActiveTab] = 0;
            }
            else if (IntVisible && IsCurInRect(IntWWork, IntX, IntY) &&
                (IsObjectMode() || IsTileMode() || IsCritMode()))
            {
                int step = 1;
                if (Keyb.ShiftDwn)
                    step = ProtosOnScreen;
                else if (Keyb.CtrlDwn)
                    step = 100;
                else if (Keyb.AltDwn)
                    step = 1000;

                int data = event_dy;
                if (data > 0)
                {
                    if (IsObjectMode() || IsTileMode() || IsCritMode())
                    {
                        (*CurProtoScroll) -= step;
                        if (*CurProtoScroll < 0)
                            *CurProtoScroll = 0;
                    }
                    else if (IntMode == INT_MODE_INCONT)
                    {
                        InContScroll -= step;
                        if (InContScroll < 0)
                            InContScroll = 0;
                    }
                    else if (IntMode == INT_MODE_LIST)
                    {
                        ListScroll -= step;
                        if (ListScroll < 0)
                            ListScroll = 0;
                    }
                }
                else
                {
                    if (IsObjectMode() && (*CurItemProtos).size())
                    {
                        (*CurProtoScroll) += step;
                        if (*CurProtoScroll >= (int)(*CurItemProtos).size())
                            *CurProtoScroll = (int)(*CurItemProtos).size() - 1;
                    }
                    else if (IsTileMode() && CurTileHashes->size())
                    {
                        (*CurProtoScroll) += step;
                        if (*CurProtoScroll >= (int)CurTileHashes->size())
                            *CurProtoScroll = (int)CurTileHashes->size() - 1;
                    }
                    else if (IsCritMode() && CurNpcProtos->size())
                    {
                        (*CurProtoScroll) += step;
                        if (*CurProtoScroll >= (int)CurNpcProtos->size())
                            *CurProtoScroll = (int)CurNpcProtos->size() - 1;
                    }
                    else if (IntMode == INT_MODE_INCONT)
                        InContScroll += step;
                    else if (IntMode == INT_MODE_LIST)
                        ListScroll += step;
                }
            }
            else
            {
                if (event_dy)
                    HexMngr.ChangeZoom(event_dy > 0 ? -1 : 1);
            }
            continue;
        }

        // Middle down
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON_MIDDLE)
        {
            CurMMouseDown();
            continue;
        }

        // Left Button Down
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON_LEFT)
        {
            IntLMouseDown();
            continue;
        }

        // Left Button Up
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON_LEFT)
        {
            IntLMouseUp();
            continue;
        }

        // Right Button Up
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON_RIGHT)
        {
            CurRMouseUp();
            continue;
        }
    }*/
}

void FOMapper::MainLoop()
{
    GameTime.FrameAdvance();

    // Fixed FPS
    const auto start_loop = Timer::RealtimeTick();

    // FPS counter
    static auto last_call = GameTime.FrameTick();
    static uint call_counter = 0;
    if ((GameTime.FrameTick() - last_call) >= 1000) {
        Settings.FPS = call_counter;
        call_counter = 0;
        last_call = GameTime.FrameTick();
    }
    else {
        call_counter++;
    }

    // Input events
    /*SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_MOUSEMOTION)
        {
            int sw = 0, sh = 0;
            SprMngr.GetWindowSize(sw, sh);
            int x = (int)(event.motion.x / (float)sw * (float)Settings.ScreenWidth);
            int y = (int)(event.motion.y / (float)sh * (float)Settings.ScreenHeight);
            Settings.MouseX = std::clamp(x, 0, Settings.ScreenWidth - 1);
            Settings.MouseY = std::clamp(y, 0, Settings.ScreenHeight - 1);
        }
        else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
        {
            Settings.MainWindowKeyboardEvents.push_back(event.type);
            Settings.MainWindowKeyboardEvents.push_back(event.key.keysym.scancode);
            Settings.MainWindowKeyboardEventsText.push_back("");
        }
        else if (event.type == SDL_TEXTINPUT)
        {
            Settings.MainWindowKeyboardEvents.push_back(SDL_KEYDOWN);
            Settings.MainWindowKeyboardEvents.push_back(510);
            Settings.MainWindowKeyboardEventsText.push_back(event.text.text);
            Settings.MainWindowKeyboardEvents.push_back(SDL_KEYUP);
            Settings.MainWindowKeyboardEvents.push_back(510);
            Settings.MainWindowKeyboardEventsText.push_back(event.text.text);
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
        {
            Settings.MainWindowMouseEvents.push_back(event.type);
            Settings.MainWindowMouseEvents.push_back(event.button.button);
            Settings.MainWindowMouseEvents.push_back(0);
        }
        else if (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERUP)
        {
            Settings.MainWindowMouseEvents.push_back(
                event.type == SDL_FINGERDOWN ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP);
            Settings.MainWindowMouseEvents.push_back(SDL_BUTTON_LEFT);
            Settings.MainWindowMouseEvents.push_back(0);
            Settings.MouseX = (int)(event.tfinger.x * (float)Settings.ScreenWidth);
            Settings.MouseY = (int)(event.tfinger.y * (float)Settings.ScreenHeight);
        }
        else if (event.type == SDL_MOUSEWHEEL)
        {
            Settings.MainWindowMouseEvents.push_back(event.type);
            Settings.MainWindowMouseEvents.push_back(SDL_BUTTON_MIDDLE);
            Settings.MainWindowMouseEvents.push_back(-event.wheel.y);
        }
        else if (event.type == SDL_QUIT)
        {
            Settings.Quit = true;
        }
    }*/

    // Script loop
    ScriptSys.LoopEvent();

    // Input
    ConsoleProcess();
    ProcessInputEvents();

    // Process
    AnimProcess();

    if (HexMngr.IsMapLoaded()) {
        for (auto it = HexMngr.GetCritters().begin(), end = HexMngr.GetCritters().end(); it != end; ++it) {
            auto* cr = it->second;
            cr->Process();

            if (cr->IsNeedMove()) {
                const auto err_move = ((!cr->IsRunning && cr->GetIsNoWalk()) || (cr->IsRunning && cr->GetIsNoRun()));
                auto old_hx = cr->GetHexX();
                auto old_hy = cr->GetHexY();
                if (!err_move && HexMngr.TransitCritter(cr, cr->MoveSteps[0].first, cr->MoveSteps[0].second, true, false)) {
                    cr->MoveSteps.erase(cr->MoveSteps.begin());
                }
                else {
                    cr->MoveSteps.clear();
                }
                HexMngr.RebuildLight();
            }
        }

        HexMngr.Scroll();
        HexMngr.ProcessItems();
        HexMngr.ProcessRain();
    }

    // Start render
    SprMngr.BeginScene(COLOR_RGB(100, 100, 100));

    DrawIfaceLayer(0);
    if (HexMngr.IsMapLoaded()) {
        HexMngr.DrawMap();

        // Texts on heads
        if (DrawCrExtInfo != 0) {
            for (auto& it : HexMngr.GetCritters()) {
                auto* cr = it.second;
                if (cr->SprDrawValid) {
                    if (DrawCrExtInfo == 1) {
                        cr->SetText(_str("|0xffaabbcc ProtoId...{}\n|0xffff1122 DialogId...{}\n", cr->GetName(), cr->GetDialogId()).c_str(), COLOR_TEXT_WHITE, 60000000);
                    }
                    else {
                        cr->SetText("", COLOR_TEXT_WHITE, 60000000);
                    }
                    cr->DrawTextOnHead();
                }
            }
        }

        // Texts on map
        const auto tick = GameTime.FrameTick();
        for (auto it = GameMapTexts.begin(); it != GameMapTexts.end();) {
            auto& t = (*it);
            if (tick >= t.StartTick + t.Tick) {
                it = GameMapTexts.erase(it);
            }
            else {
                const auto procent = GenericUtils::Percent(t.Tick, tick - t.StartTick);
                const auto r = t.Pos.Interpolate(t.EndPos, procent);
                auto& f = HexMngr.GetField(t.HexX, t.HexY);
                const auto x = static_cast<int>((f.ScrX + Settings.MapHexWidth / 2 + Settings.ScrOx) / Settings.SpritesZoom - 100.0f - static_cast<float>(t.Pos.L - r.L));
                const auto y = static_cast<int>((f.ScrY + Settings.MapHexLineHeight / 2 - t.Pos.Height() - (t.Pos.T - r.T) + Settings.ScrOy) / Settings.SpritesZoom - 70.0f);
                auto color = t.Color;
                if (t.Fade) {
                    color = (color ^ 0xFF000000) | ((0xFF * (100 - procent) / 100) << 24);
                }
                SprMngr.DrawStr(Rect(x, y, x + 200, y + 70), t.Text, FT_CENTERX | FT_BOTTOM | FT_BORDERED, color, FONT_DEFAULT);
                it++;
            }
        }
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

    // Fixed FPS
    if (!Settings.VSync && (Settings.FixedFPS != 0)) {
        if (Settings.FixedFPS > 0) {
            static auto balance = 0.0;
            const auto elapsed = Timer::RealtimeTick() - start_loop;
            const auto need_elapsed = 1000.0 / static_cast<double>(Settings.FixedFPS);
            if (need_elapsed > elapsed) {
                const auto sleep = need_elapsed - elapsed + balance;
                balance = fmod(sleep, 1.0);
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(sleep)));
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(-Settings.FixedFPS));
        }
    }
}

void FOMapper::RefreshTiles(int tab)
{
    static const string formats[] = {"frm", "fofrm", "bmp", "dds", "dib", "hdr", "jpg", "jpeg", "pfm", "png", "tga", "spr", "til", "zar", "art"};

    // Clear old tile names
    for (auto it = Tabs[tab].begin(); it != Tabs[tab].end();) {
        auto& stab = it->second;
        if (!stab.TileNames.empty()) {
            if (TabsActive[tab] == &stab) {
                TabsActive[tab] = nullptr;
            }
            Tabs[tab].erase(it++);
        }
        else {
            ++it;
        }
    }

    // Find names
    auto& ttab = TabsTiles[tab];
    if (ttab.TileDirs.empty()) {
        return;
    }

    Tabs[tab].clear();
    Tabs[tab][DEFAULT_SUB_TAB].Index = 0; // Add default

    StrUIntMap PathIndex;

    for (uint t = 0, tt = static_cast<uint>(ttab.TileDirs.size()); t < tt; t++) {
        auto& path = ttab.TileDirs[t];
        bool include_subdirs = ttab.TileSubDirs[t];

        StrVec tiles;
        auto tile_files = FileMngr.FilterFiles("", path, include_subdirs);
        while (tile_files.MoveNext()) {
            tiles.push_back(tile_files.GetCurFileHeader().GetPath());
        }

        std::sort(tiles.begin(), tiles.end(), [](const string& left, const string& right) {
            for (auto lit = left.begin(), rit = right.begin(); lit != left.end() && rit != right.end(); ++lit, ++rit) {
                const auto lc = tolower(*lit);
                const auto rc = tolower(*rit);
                if (lc < rc) {
                    return true;
                }
                if (lc > rc) {
                    return false;
                }
            }
            return left.size() < right.size();
        });

        for (auto& fname : tiles) {
            string ext = _str(fname).getFileExtension();
            if (ext.empty()) {
                continue;
            }

            // Check format availability
            auto format_aviable = false;
            for (const auto& format : formats) {
                if (format == ext) {
                    format_aviable = true;
                    break;
                }
            }
            if (!format_aviable) {
                format_aviable = Is3dExtensionSupported(ext);
            }

            if (format_aviable) {
                // Make primary collection name
                string dir = _str(fname).extractDir();
                if (dir.empty()) {
                    dir = "root";
                }
                auto path_index = PathIndex[dir];
                if (path_index == 0u) {
                    path_index = static_cast<uint>(PathIndex.size());
                    PathIndex[dir] = path_index;
                }
                string collection_name = _str("{:03} - {}", path_index, dir);

                // Make secondary collection name
                string collection_name_ex;
                if (Settings.SplitTilesCollection) {
                    auto pos = fname.find_last_of('/');
                    if (pos == string::npos) {
                        pos = 0;
                    }
                    else {
                        pos++;
                    }
                    for (auto i = static_cast<uint>(pos), j = static_cast<uint>(fname.size()); i < j; i++) {
                        if (fname[i] >= '0' && fname[i] <= '9') {
                            if ((i - pos) != 0u) {
                                collection_name_ex += collection_name;
                                collection_name_ex += fname.substr(pos, i - pos);
                            }
                            break;
                        }
                    }
                    if (collection_name_ex.length() == 0u) {
                        collection_name_ex += collection_name;
                        collection_name_ex += "<other>";
                    }
                }

                // Write tile
                auto hash = _str(fname).toHash();
                Tabs[tab][DEFAULT_SUB_TAB].TileHashes.push_back(hash);
                Tabs[tab][DEFAULT_SUB_TAB].TileNames.push_back(fname);
                Tabs[tab][collection_name].TileHashes.push_back(hash);
                Tabs[tab][collection_name].TileNames.push_back(fname);
                Tabs[tab][collection_name_ex].TileHashes.push_back(hash);
                Tabs[tab][collection_name_ex].TileNames.push_back(fname);
            }
        }
    }

    // Set default active tab
    TabsActive[tab] = &(*Tabs[tab].begin()).second;
}

auto FOMapper::GetProtoItemCurSprId(ProtoItem* proto_item) -> uint
{
    auto* anim = ResMngr.GetItemAnim(proto_item->GetPicMap());
    if (anim == nullptr) {
        return 0;
    }

    uint beg = 0;
    uint end = 0;
    if (proto_item->GetIsShowAnim()) {
        beg = 0;
        end = anim->CntFrm - 1;
    }
    if (proto_item->GetIsShowAnimExt()) {
        beg = proto_item->GetAnimStay0();
        end = proto_item->GetAnimStay1();
    }
    if (beg >= anim->CntFrm) {
        beg = anim->CntFrm - 1;
    }
    if (end >= anim->CntFrm) {
        end = anim->CntFrm - 1;
    }
    if (beg > end) {
        std::swap(beg, end);
    }

    const auto count = end - beg + 1;
    const auto ticks = anim->Ticks / anim->CntFrm * count;
    return anim->Ind[beg + ((GameTime.FrameTick() % ticks) * 100 / ticks) * count / 100];
}

void FOMapper::IntDraw()
{
    if (!IntVisible) {
        return;
    }

    SprMngr.DrawSprite(IntMainPic, IntX, IntY, 0);

    switch (IntMode) {
    case INT_MODE_CUSTOM0:
        SprMngr.DrawSprite(IntPTab, IntBCust[0][0] + IntX, IntBCust[0][1] + IntY, 0);
        break;
    case INT_MODE_CUSTOM1:
        SprMngr.DrawSprite(IntPTab, IntBCust[1][0] + IntX, IntBCust[1][1] + IntY, 0);
        break;
    case INT_MODE_CUSTOM2:
        SprMngr.DrawSprite(IntPTab, IntBCust[2][0] + IntX, IntBCust[2][1] + IntY, 0);
        break;
    case INT_MODE_CUSTOM3:
        SprMngr.DrawSprite(IntPTab, IntBCust[3][0] + IntX, IntBCust[3][1] + IntY, 0);
        break;
    case INT_MODE_CUSTOM4:
        SprMngr.DrawSprite(IntPTab, IntBCust[4][0] + IntX, IntBCust[4][1] + IntY, 0);
        break;
    case INT_MODE_CUSTOM5:
        SprMngr.DrawSprite(IntPTab, IntBCust[5][0] + IntX, IntBCust[5][1] + IntY, 0);
        break;
    case INT_MODE_CUSTOM6:
        SprMngr.DrawSprite(IntPTab, IntBCust[6][0] + IntX, IntBCust[6][1] + IntY, 0);
        break;
    case INT_MODE_CUSTOM7:
        SprMngr.DrawSprite(IntPTab, IntBCust[7][0] + IntX, IntBCust[7][1] + IntY, 0);
        break;
    case INT_MODE_CUSTOM8:
        SprMngr.DrawSprite(IntPTab, IntBCust[8][0] + IntX, IntBCust[8][1] + IntY, 0);
        break;
    case INT_MODE_CUSTOM9:
        SprMngr.DrawSprite(IntPTab, IntBCust[9][0] + IntX, IntBCust[9][1] + IntY, 0);
        break;
    case INT_MODE_ITEM:
        SprMngr.DrawSprite(IntPTab, IntBItem[0] + IntX, IntBItem[1] + IntY, 0);
        break;
    case INT_MODE_TILE:
        SprMngr.DrawSprite(IntPTab, IntBTile[0] + IntX, IntBTile[1] + IntY, 0);
        break;
    case INT_MODE_CRIT:
        SprMngr.DrawSprite(IntPTab, IntBCrit[0] + IntX, IntBCrit[1] + IntY, 0);
        break;
    case INT_MODE_FAST:
        SprMngr.DrawSprite(IntPTab, IntBFast[0] + IntX, IntBFast[1] + IntY, 0);
        break;
    case INT_MODE_IGNORE:
        SprMngr.DrawSprite(IntPTab, IntBIgnore[0] + IntX, IntBIgnore[1] + IntY, 0);
        break;
    case INT_MODE_INCONT:
        SprMngr.DrawSprite(IntPTab, IntBInCont[0] + IntX, IntBInCont[1] + IntY, 0);
        break;
    case INT_MODE_MESS:
        SprMngr.DrawSprite(IntPTab, IntBMess[0] + IntX, IntBMess[1] + IntY, 0);
        break;
    case INT_MODE_LIST:
        SprMngr.DrawSprite(IntPTab, IntBList[0] + IntX, IntBList[1] + IntY, 0);
        break;
    default:
        break;
    }

    for (auto i = INT_MODE_CUSTOM0; i <= INT_MODE_CUSTOM9; i++) {
        SprMngr.DrawStr(Rect(IntBCust[i], IntX, IntY), TabsName[INT_MODE_CUSTOM0 + i], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    }
    SprMngr.DrawStr(Rect(IntBItem, IntX, IntY), TabsName[INT_MODE_ITEM], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(Rect(IntBTile, IntX, IntY), TabsName[INT_MODE_TILE], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(Rect(IntBCrit, IntX, IntY), TabsName[INT_MODE_CRIT], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(Rect(IntBFast, IntX, IntY), TabsName[INT_MODE_FAST], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(Rect(IntBIgnore, IntX, IntY), TabsName[INT_MODE_IGNORE], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(Rect(IntBInCont, IntX, IntY), TabsName[INT_MODE_INCONT], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(Rect(IntBMess, IntX, IntY), TabsName[INT_MODE_MESS], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.DrawStr(Rect(IntBList, IntX, IntY), TabsName[INT_MODE_LIST], FT_NOBREAK | FT_CENTERX | FT_CENTERY, COLOR_TEXT_WHITE, FONT_DEFAULT);

    if (Settings.ShowItem) {
        SprMngr.DrawSprite(IntPShow, IntBShowItem[0] + IntX, IntBShowItem[1] + IntY, 0);
    }
    if (Settings.ShowScen) {
        SprMngr.DrawSprite(IntPShow, IntBShowScen[0] + IntX, IntBShowScen[1] + IntY, 0);
    }
    if (Settings.ShowWall) {
        SprMngr.DrawSprite(IntPShow, IntBShowWall[0] + IntX, IntBShowWall[1] + IntY, 0);
    }
    if (Settings.ShowCrit) {
        SprMngr.DrawSprite(IntPShow, IntBShowCrit[0] + IntX, IntBShowCrit[1] + IntY, 0);
    }
    if (Settings.ShowTile) {
        SprMngr.DrawSprite(IntPShow, IntBShowTile[0] + IntX, IntBShowTile[1] + IntY, 0);
    }
    if (Settings.ShowRoof) {
        SprMngr.DrawSprite(IntPShow, IntBShowRoof[0] + IntX, IntBShowRoof[1] + IntY, 0);
    }
    if (Settings.ShowFast) {
        SprMngr.DrawSprite(IntPShow, IntBShowFast[0] + IntX, IntBShowFast[1] + IntY, 0);
    }

    if (IsSelectItem) {
        SprMngr.DrawSprite(IntPSelect, IntBSelectItem[0] + IntX, IntBSelectItem[1] + IntY, 0);
    }
    if (IsSelectScen) {
        SprMngr.DrawSprite(IntPSelect, IntBSelectScen[0] + IntX, IntBSelectScen[1] + IntY, 0);
    }
    if (IsSelectWall) {
        SprMngr.DrawSprite(IntPSelect, IntBSelectWall[0] + IntX, IntBSelectWall[1] + IntY, 0);
    }
    if (IsSelectCrit) {
        SprMngr.DrawSprite(IntPSelect, IntBSelectCrit[0] + IntX, IntBSelectCrit[1] + IntY, 0);
    }
    if (IsSelectTile) {
        SprMngr.DrawSprite(IntPSelect, IntBSelectTile[0] + IntX, IntBSelectTile[1] + IntY, 0);
    }
    if (IsSelectRoof) {
        SprMngr.DrawSprite(IntPSelect, IntBSelectRoof[0] + IntX, IntBSelectRoof[1] + IntY, 0);
    }

    auto x = IntWWork[0] + IntX;
    auto y = IntWWork[1] + IntY;
    auto h = IntWWork[3] - IntWWork[1];
    int w = ProtoWidth;

    if (IsObjectMode()) {
        auto i = *CurProtoScroll;
        int j = i + ProtosOnScreen;
        if (j > static_cast<int>((*CurItemProtos).size())) {
            j = static_cast<int>((*CurItemProtos).size());
        }

        for (; i < j; i++, x += w) {
            auto* proto_item = (*CurItemProtos)[i];
            auto col = (i == static_cast<int>(GetTabIndex()) ? COLOR_IFACE_RED : COLOR_IFACE);
            SprMngr.DrawSpriteSize(GetProtoItemCurSprId(proto_item), x, y, w, h / 2, false, true, col);

            if (proto_item->GetPicInv() != 0u) {
                auto* anim = ResMngr.GetInvAnim(proto_item->GetPicInv());
                if (anim != nullptr) {
                    SprMngr.DrawSpriteSize(anim->GetCurSprId(GameTime.GameTick()), x, y + h / 2, w, h / 2, false, true, col);
                }
            }

            SprMngr.DrawStr(Rect(x, y + h - 15, x + w, y + h), proto_item->GetName(), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
        }

        if (GetTabIndex() < static_cast<uint>((*CurItemProtos).size())) {
            auto* proto_item = (*CurItemProtos)[GetTabIndex()];
            auto it = std::find(proto_item->TextsLang.begin(), proto_item->TextsLang.end(), CurLang.NameCode);
            if (it != proto_item->TextsLang.end()) {
                auto index = static_cast<uint>(std::distance(proto_item->TextsLang.begin(), it));
                auto info = proto_item->Texts[0]->GetStr(ITEM_STR_ID(proto_item->ProtoId, 1));
                info += " - ";
                info += proto_item->Texts[0]->GetStr(ITEM_STR_ID(proto_item->ProtoId, 2));
                SprMngr.DrawStr(Rect(IntWHint, IntX, IntY), info, 0, FONT_DEFAULT, FONT_DEFAULT);
            }
        }
    }
    else if (IsTileMode()) {
        auto i = *CurProtoScroll;
        int j = i + ProtosOnScreen;
        if (j > static_cast<int>(CurTileHashes->size())) {
            j = static_cast<int>(CurTileHashes->size());
        }

        for (; i < j; i++, x += w) {
            auto* anim = ResMngr.GetItemAnim((*CurTileHashes)[i]);
            if (anim == nullptr) {
                anim = ResMngr.ItemHexDefaultAnim;
            }

            auto col = (i == static_cast<int>(GetTabIndex()) ? COLOR_IFACE_RED : COLOR_IFACE);
            SprMngr.DrawSpriteSize(anim->GetCurSprId(GameTime.GameTick()), x, y, w, h / 2, false, true, col);

            auto& name = (*CurTileNames)[i];
            auto pos = name.find_last_of('/');
            if (pos != string::npos) {
                SprMngr.DrawStr(Rect(x, y + h - 15, x + w, y + h), name.substr(pos + 1), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
            }
            else {
                SprMngr.DrawStr(Rect(x, y + h - 15, x + w, y + h), name, FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
            }
        }

        if (GetTabIndex() < CurTileNames->size()) {
            SprMngr.DrawStr(Rect(IntWHint, IntX, IntY), (*CurTileNames)[GetTabIndex()], 0, FONT_DEFAULT, FONT_DEFAULT);
        }
    }
    else if (IsCritMode()) {
        uint i = *CurProtoScroll;
        auto j = i + ProtosOnScreen;
        if (j > CurNpcProtos->size()) {
            j = static_cast<uint>(CurNpcProtos->size());
        }

        for (; i < j; i++, x += w) {
            auto* proto = (*CurNpcProtos)[i];

            auto model_name = proto->Props.GetValue<hash>(CritterView::PropertyModelName);
            auto spr_id = ResMngr.GetCritSprId(model_name, 1, 1, NpcDir, nullptr); // &proto->Params[ ST_ANIM3D_LAYER_BEGIN ] );
            if (spr_id == 0u) {
                continue;
            }

            auto col = COLOR_IFACE;
            if (i == GetTabIndex()) {
                col = COLOR_IFACE_RED;
            }

            SprMngr.DrawSpriteSize(spr_id, x, y, w, h / 2, false, true, col);
            SprMngr.DrawStr(Rect(x, y + h - 15, x + w, y + h), proto->GetName(), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
        }

        if (GetTabIndex() < CurNpcProtos->size()) {
            auto* proto = (*CurNpcProtos)[GetTabIndex()];
            SprMngr.DrawStr(Rect(IntWHint, IntX, IntY), proto->GetName(), 0, FONT_DEFAULT, FONT_DEFAULT);
        }
    }
    else if (IntMode == INT_MODE_INCONT && !SelectedEntities.empty()) {
        auto* entity = SelectedEntities[0];
        EntityVec children; // Todo: need attention!
        // = entity->GetChildren();
        uint i = InContScroll;
        auto j = i + ProtosOnScreen;
        if (j > children.size()) {
            j = static_cast<uint>(children.size());
        }

        for (; i < j; i++, x += w) {
            RUNTIME_ASSERT(children[i]->Type == EntityType::ItemView);
            auto* child = dynamic_cast<ItemView*>(children[i]);

            auto* anim = ResMngr.GetInvAnim(child->GetPicInv());
            if (anim == nullptr) {
                continue;
            }

            auto col = COLOR_IFACE;
            if (child == InContItem) {
                col = COLOR_IFACE_RED;
            }

            SprMngr.DrawSpriteSize(anim->GetCurSprId(GameTime.GameTick()), x, y, w, h, false, true, col);

            SprMngr.DrawStr(Rect(x, y + h - 15, x + w, y + h), _str("x{}", child->GetCount()), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
            if (child->GetAccessory() == ITEM_ACCESSORY_CRITTER && (child->GetCritSlot() != 0u)) {
                SprMngr.DrawStr(Rect(x, y, x + w, y + h), _str("Slot {}", child->GetCritSlot()), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
            }
        }
    }
    else if (IntMode == INT_MODE_LIST) {
        auto i = ListScroll;
        auto j = static_cast<int>(LoadedMaps.size());

        for (; i < j; i++, x += w) {
            auto* map = LoadedMaps[i];
            SprMngr.DrawStr(Rect(x, y, x + w, y + h), _str(" '{}'", map->GetName()), 0, map == ActiveMap ? COLOR_IFACE_RED : COLOR_TEXT, FONT_DEFAULT);
        }
    }

    // Message box
    if (IntMode == INT_MODE_MESS) {
        MessBoxDraw();
    }

    // Sub tabs
    if (SubTabsActive) {
        SprMngr.DrawSprite(SubTabsPic, SubTabsX, SubTabsY, 0);

        auto line_height = SprMngr.GetLineHeight(FONT_DEFAULT) + 1;
        auto posy = SubTabsRect.Height() - line_height - 2;
        auto i = 0;
        auto& stabs = Tabs[SubTabsActiveTab];
        for (auto& it : stabs) {
            i++;
            if (i - 1 < TabsScroll[SubTabsActiveTab]) {
                continue;
            }

            auto name = it.first;
            auto& stab = it.second;

            auto color = (TabsActive[SubTabsActiveTab] == &stab ? COLOR_TEXT_WHITE : COLOR_TEXT);
            auto r = Rect(SubTabsRect.L + SubTabsX + 5, SubTabsRect.T + SubTabsY + posy, SubTabsRect.L + SubTabsX + 5 + Settings.ScreenWidth, SubTabsRect.T + SubTabsY + posy + line_height - 1);
            if (IsCurInRect(r)) {
                color = COLOR_TEXT_DWHITE;
            }

            auto count = static_cast<uint>(stab.TileNames.size());
            if (count == 0u) {
                count = static_cast<uint>(stab.NpcProtos.size());
            }
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
    if (HexMngr.IsMapLoaded()) {
        auto hex_thru = false;
        ushort hx = 0;
        ushort hy = 0;
        if (HexMngr.GetHexPixel(Settings.MouseX, Settings.MouseY, hx, hy)) {
            hex_thru = true;
        }
        auto day_time = HexMngr.GetDayTime();
        SprMngr.DrawStr(Rect(Settings.ScreenWidth - 100, 0, Settings.ScreenWidth, Settings.ScreenHeight),
            _str("Map '{}'\n"
                 "Hex {} {}\n"
                 "Time {} : {}\n"
                 "Fps {}\n"
                 "Tile layer {}\n"
                 "{}",
                ActiveMap->GetName(), hex_thru ? hx : -1, hex_thru ? hy : -1, day_time / 60 % 24, day_time % 60, Settings.FPS, TileLayer, Settings.ScrollCheck ? "Scroll check" : "")
                .c_str(),
            FT_NOBREAK_LINE, 0, FONT_DEFAULT);
    }
}

void FOMapper::ObjDraw()
{
    if (!ObjVisible) {
        return;
    }

    auto* entity = GetInspectorEntity();
    if (entity == nullptr) {
        return;
    }

    auto* item = (entity->Type == EntityType::ItemView || entity->Type == EntityType::ItemHexView ? dynamic_cast<ItemView*>(entity) : nullptr);
    auto* cr = (entity->Type == EntityType::CritterView ? dynamic_cast<CritterView*>(entity) : nullptr);
    auto r = Rect(ObjWWork, ObjX, ObjY);
    const auto x = r.L;
    const auto y = r.T;
    const auto w = r.Width();
    auto h = r.Height();

    SprMngr.DrawSprite(ObjWMainPic, ObjX, ObjY, 0);
    if (ObjToAll) {
        SprMngr.DrawSprite(ObjPbToAllDn, ObjBToAll[0] + ObjX, ObjBToAll[1] + ObjY, 0);
    }

    if (item != nullptr) {
        auto* anim = ResMngr.GetItemAnim(item->GetPicMap());
        if (anim == nullptr) {
            anim = ResMngr.ItemHexDefaultAnim;
        }
        SprMngr.DrawSpriteSize(anim->GetCurSprId(GameTime.GameTick()), x + w - ProtoWidth, y, ProtoWidth, ProtoWidth, false, true, 0);

        if (item->GetPicInv() != 0u) {
            auto* anim = ResMngr.GetInvAnim(item->GetPicInv());
            if (anim != nullptr) {
                SprMngr.DrawSpriteSize(anim->GetCurSprId(GameTime.GameTick()), x + w - ProtoWidth, y + ProtoWidth, ProtoWidth, ProtoWidth, false, true, 0);
            }
        }
    }

    DrawLine("Id", "", _str("{} ({})", entity->Id, static_cast<int>(entity->Id)), true, r);
    DrawLine("ProtoName", "", _str().parseHash(entity->GetProtoId()), true, r);
    if (cr != nullptr) {
        DrawLine("Type", "", "Critter", true, r);
    }
    else if ((item != nullptr) && !item->IsStatic()) {
        DrawLine("Type", "", "Item", true, r);
    }
    else if ((item != nullptr) && item->IsStatic()) {
        DrawLine("Type", "", "Static Item", true, r);
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }

    for (auto& prop : ShowProps) {
        if (prop != nullptr) {
            auto value = entity->Props.SavePropertyToText(prop);
            DrawLine(prop->GetName(), prop->GetTypeName(), value, prop->IsConst(), r);
        }
        else {
            r.T += DRAW_NEXT_HEIGHT;
            r.B += DRAW_NEXT_HEIGHT;
        }
    }
}

void FOMapper::DrawLine(const string& name, const string& type_name, const string& text, bool is_const, Rect& r)
{
    auto col = COLOR_TEXT;
    const auto x = r.L;
    const auto y = r.T;
    const auto w = r.Width();
    const auto h = r.Height();
    col = COLOR_TEXT;
    if (is_const) {
        col = COLOR_TEXT_DWHITE;
    }

    auto result_text = text;
    if (ObjCurLine == (y - ObjWWork[1] - ObjY) / DRAW_NEXT_HEIGHT) {
        col = COLOR_TEXT_WHITE;
        if (!is_const && ObjCurLineValue != ObjCurLineInitValue) {
            col = COLOR_TEXT_RED;
            result_text = ObjCurLineValue;
        }
    }

    string str = _str("{}{}{}{}", name, !type_name.empty() ? " (" : "", !type_name.empty() ? type_name : "", !type_name.empty() ? ")" : "");
    str += "........................................................................................................";
    SprMngr.DrawStr(Rect(Rect(x, y, x + w / 2, y + h), 0, 0), str, FT_NOBREAK, col, 0);
    SprMngr.DrawStr(Rect(Rect(x + w / 2, y, x + w, y + h), 0, 0), result_text, FT_NOBREAK, col, 0);
    r.T += DRAW_NEXT_HEIGHT;
    r.B += DRAW_NEXT_HEIGHT;
}

void FOMapper::ObjKeyDown(KeyCode dik, const char* dik_text)
{
    if (dik == KeyCode::DIK_RETURN || dik == KeyCode::DIK_NUMPADENTER) {
        if (ObjCurLineInitValue != ObjCurLineValue) {
            auto* entity = GetInspectorEntity();
            RUNTIME_ASSERT(entity);
            ObjKeyDownApply(entity);

            if (!SelectedEntities.empty() && SelectedEntities[0] == entity && ObjToAll) {
                for (size_t i = 1; i < SelectedEntities.size(); i++) {
                    if (SelectedEntities[i]->Type == entity->Type) {
                        ObjKeyDownApply(SelectedEntities[i]);
                    }
                }
            }

            SelectEntityProp(ObjCurLine);
            HexMngr.RebuildLight();
        }
    }
    else if (dik == KeyCode::DIK_UP) {
        SelectEntityProp(ObjCurLine - 1);
    }
    else if (dik == KeyCode::DIK_DOWN) {
        SelectEntityProp(ObjCurLine + 1);
    }
    else if (dik == KeyCode::DIK_ESCAPE) {
        ObjCurLineValue = ObjCurLineInitValue;
    }
    else {
        if (!ObjCurLineIsConst) {
            Keyb.GetChar(dik, dik_text, ObjCurLineValue, nullptr, MAX_FOTEXT, KIF_NO_SPEC_SYMBOLS);
        }
    }
}

void FOMapper::ObjKeyDownApply(Entity* entity)
{
    const auto start_line = 3;
    RUNTIME_ASSERT((entity->Type == EntityType::CritterView || entity->Type == EntityType::Item || entity->Type == EntityType::ItemHexView));
    if (ObjCurLine >= start_line && ObjCurLine - start_line < static_cast<int>(ShowProps.size())) {
        auto* prop = ShowProps[ObjCurLine - start_line];
        if (prop != nullptr) {
            if (entity->Props.LoadPropertyFromText(prop, ObjCurLineValue)) {
                if (entity->Type == EntityType::ItemHexView && (prop == ItemHexView::PropertyOffsetX || prop == ItemHexView::PropertyOffsetY)) {
                    (dynamic_cast<ItemHexView*>(entity))->SetAnimOffs();
                }
                if (entity->Type == EntityType::ItemHexView && prop == ItemHexView::PropertyPicMap) {
                    (dynamic_cast<ItemHexView*>(entity))->RefreshAnim();
                }
            }
            else {
                entity->Props.LoadPropertyFromText(prop, ObjCurLineInitValue);
            }
        }
    }
}

void FOMapper::SelectEntityProp(int line)
{
    const auto start_line = 3;
    ObjCurLine = line;
    if (ObjCurLine < 0) {
        ObjCurLine = 0;
    }
    ObjCurLineInitValue = ObjCurLineValue = "";
    ObjCurLineIsConst = true;

    auto* entity = GetInspectorEntity();
    if (entity != nullptr) {
        RUNTIME_ASSERT((entity->Type == EntityType::CritterView || entity->Type == EntityType::Item || entity->Type == EntityType::ItemHexView));
        if (ObjCurLine - start_line >= static_cast<int>(ShowProps.size())) {
            ObjCurLine = static_cast<int>(ShowProps.size()) + start_line - 1;
        }
        if (ObjCurLine >= start_line && ObjCurLine - start_line < static_cast<int>(ShowProps.size()) && (ShowProps[ObjCurLine - start_line] != nullptr)) {
            ObjCurLineInitValue = ObjCurLineValue = entity->Props.SavePropertyToText(ShowProps[ObjCurLine - start_line]);
            ObjCurLineIsConst = ShowProps[ObjCurLine - start_line]->IsConst();
        }
    }
}

auto FOMapper::GetInspectorEntity() -> Entity*
{
    auto* entity = (IntMode == INT_MODE_INCONT && (InContItem != nullptr) ? InContItem : (!SelectedEntities.empty() ? SelectedEntities[0] : nullptr));
    if (entity == InspectorEntity) {
        return entity;
    }

    InspectorEntity = entity;
    ShowProps.clear();

    if (entity != nullptr) {
        IntVec enum_values;
        ScriptSys.InspectorPropertiesEvent(entity, enum_values);
        for (auto enum_value : enum_values) {
            ShowProps.push_back(enum_value != 0 ? entity->Props.FindByEnum(enum_value) : nullptr);
        }
    }

    SelectEntityProp(ObjCurLine);
    return entity;
}

void FOMapper::IntLMouseDown()
{
    IntHold = INT_NONE;

    // Sub tabs
    if (IntVisible && SubTabsActive) {
        if (IsCurInRect(SubTabsRect, SubTabsX, SubTabsY)) {
            const auto line_height = SprMngr.GetLineHeight(FONT_DEFAULT) + 1;
            auto posy = SubTabsRect.Height() - line_height - 2;
            auto i = 0;
            auto& stabs = Tabs[SubTabsActiveTab];
            for (auto& it : stabs) {
                i++;
                if (i - 1 < TabsScroll[SubTabsActiveTab]) {
                    continue;
                }

                const auto& name = it.first;
                auto& stab = it.second;

                auto r = Rect(SubTabsRect.L + SubTabsX + 5, SubTabsRect.T + SubTabsY + posy, SubTabsRect.L + SubTabsX + 5 + SubTabsRect.Width(), SubTabsRect.T + SubTabsY + posy + line_height - 1);
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

        if (!HexMngr.GetHexPixel(Settings.MouseX, Settings.MouseY, SelectHX1, SelectHY1)) {
            return;
        }
        SelectHX2 = SelectHX1;
        SelectHY2 = SelectHY1;
        SelectX = Settings.MouseX;
        SelectY = Settings.MouseY;

        if (CurMode == CUR_MODE_DEFAULT) {
            if (Keyb.ShiftDwn) {
                for (auto& entity : SelectedEntities) {
                    if (entity->Type == EntityType::CritterView) {
                        auto* cr = dynamic_cast<CritterView*>(entity);
                        const auto is_run = (!cr->MoveSteps.empty() && cr->MoveSteps[cr->MoveSteps.size() - 1].first == SelectHX1 && cr->MoveSteps[cr->MoveSteps.size() - 1].second == SelectHY1);

                        cr->MoveSteps.clear();
                        if (!is_run && cr->GetIsNoWalk()) {
                            break;
                        }

                        auto hx = cr->GetHexX();
                        auto hy = cr->GetHexY();
                        UCharVec steps;
                        if (HexMngr.FindPath(nullptr, hx, hy, SelectHX1, SelectHY1, steps, -1)) {
                            for (auto step : steps) {
                                GeomHelper.MoveHexByDir(hx, hy, step, HexMngr.GetWidth(), HexMngr.GetHeight());
                                cr->MoveSteps.push_back(UShortPair(hx, hy));
                            }
                            cr->IsRunning = is_run;
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
            if (IsObjectMode() && !(*CurItemProtos).empty() != 0u) {
                AddItem((*CurItemProtos)[GetTabIndex()]->ProtoId, SelectHX1, SelectHY1, nullptr);
            }
            else if (IsTileMode() && !CurTileHashes->empty()) {
                AddTile((*CurTileHashes)[GetTabIndex()], SelectHX1, SelectHY1, 0, 0, TileLayer, DrawRoof);
            }
            else if (IsCritMode() && !CurNpcProtos->empty()) {
                AddCritter((*CurNpcProtos)[GetTabIndex()]->ProtoId, SelectHX1, SelectHY1);
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

        if (IsObjectMode() && !(*CurItemProtos).empty()) {
            ind += *CurProtoScroll;
            if (ind >= static_cast<int>((*CurItemProtos).size())) {
                ind = static_cast<int>((*CurItemProtos).size()) - 1;
            }
            SetTabIndex(ind);

            // Switch ignore pid to draw
            if (Keyb.CtrlDwn) {
                const auto pid = (*CurItemProtos)[ind]->ProtoId;

                auto& stab = Tabs[INT_MODE_IGNORE][DEFAULT_SUB_TAB];
                auto founded = false;
                for (auto it = stab.ItemProtos.begin(); it != stab.ItemProtos.end(); ++it) {
                    if ((*it)->ProtoId == pid) {
                        founded = true;
                        stab.ItemProtos.erase(it);
                        break;
                    }
                }
                if (!founded) {
                    stab.ItemProtos.push_back((*CurItemProtos)[ind]);
                }

                HexMngr.SwitchIgnorePid(pid);
                HexMngr.RefreshMap();
            }
            // Add to container
            else if (Keyb.AltDwn && !SelectedEntities.empty()) {
                const auto add = true;
                auto* proto_item = (*CurItemProtos)[ind];

                if (proto_item->GetStackable()) {
                    // Todo: need attention!
                    /*for (auto& child : SelectedEntities[0]->GetChildren())
                    {
                        if (proto_item->ProtoId == child->GetProtoId())
                        {
                            add = false;
                            break;
                        }
                    }*/
                }

                if (add) {
                    AddItem(proto_item->ProtoId, 0, 0, SelectedEntities[0]);
                }
            }
        }
        else if (IsTileMode() && !CurTileHashes->empty()) {
            ind += *CurProtoScroll;
            if (ind >= static_cast<int>(CurTileHashes->size())) {
                ind = static_cast<int>(CurTileHashes->size()) - 1;
            }
            SetTabIndex(ind);
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
            EntityVec children;
            // Todo: need attention!
            // if (!SelectedEntities.empty())
            //    children = SelectedEntities[0]->GetChildren();

            if (!children.empty()) {
                if (ind < static_cast<int>(children.size())) {
                    InContItem = dynamic_cast<ItemView*>(children[ind]);
                }

                // Delete child
                if (Keyb.AltDwn && (InContItem != nullptr)) {
                    if (InContItem->GetAccessory() == ITEM_ACCESSORY_CRITTER) {
                        auto* owner = HexMngr.GetCritter(InContItem->GetCritId());
                        RUNTIME_ASSERT(owner);
                        owner->DeleteItem(InContItem, true);
                    }
                    else if (InContItem->GetAccessory() == ITEM_ACCESSORY_CONTAINER) {
                        ItemView* owner = HexMngr.GetItemById(InContItem->GetContainerId());
                        RUNTIME_ASSERT(owner);
                        // owner->ContEraseItem(InContItem); // Todo: need attention!
                        InContItem->Release();
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
                // Change child slot
                else if (Keyb.ShiftDwn && (InContItem != nullptr) && SelectedEntities[0]->Type == EntityType::CritterView) {
                    auto* cr = dynamic_cast<CritterView*>(SelectedEntities[0]);

                    auto to_slot = InContItem->GetCritSlot() + 1;
                    while (to_slot >= Settings.CritterSlotEnabled.size() || !Settings.CritterSlotEnabled[to_slot % 256]) {
                        to_slot++;
                    }
                    to_slot %= 256;

                    // Todo: need attention!
                    // for (auto& child : cr->GetChildren())
                    //    if (((ItemView*)child)->GetCritSlot() == to_slot)
                    //        ((ItemView*)child)->SetCritSlot(0);

                    InContItem->SetCritSlot(to_slot);

                    cr->AnimateStay();
                }
                HexMngr.RebuildLight();
            }
        }
        else if (IntMode == INT_MODE_LIST) {
            ind += ListScroll;

            if (ind < static_cast<int>(LoadedMaps.size()) && ActiveMap != LoadedMaps[ind]) {
                SelectClear();

                // Todo: need attention!
                /*if (ActiveMap != nullptr) {
                    HexMngr.GetProtoMap(*dynamic_cast<ProtoMap*>(ActiveMap->Proto));
                }
                if (HexMngr.SetProtoMap(*dynamic_cast<ProtoMap*>(LoadedMaps[ind]->Proto))) {
                    ActiveMap = LoadedMaps[ind];
                    HexMngr.FindSetCenter(ActiveMap->GetWorkHexX(), ActiveMap->GetWorkHexY());
                }*/
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
        if (IsObjectMode() || IsTileMode() || IsCritMode()) {
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
        if (IsObjectMode() || IsTileMode() || IsCritMode()) {
            (*CurProtoScroll) -= ProtosOnScreen;
            if (*CurProtoScroll < 0) {
                *CurProtoScroll = 0;
            }
        }
        else if (IntMode == INT_MODE_INCONT) {
            InContScroll -= ProtosOnScreen;
            if (InContScroll < 0) {
                InContScroll = 0;
            }
        }
        else if (IntMode == INT_MODE_LIST) {
            ListScroll -= ProtosOnScreen;
            if (ListScroll < 0) {
                ListScroll = 0;
            }
        }
    }
    else if (IsCurInRect(IntBScrFront, IntX, IntY)) {
        if (IsObjectMode() && !(*CurItemProtos).empty()) {
            (*CurProtoScroll)++;
            if (*CurProtoScroll >= static_cast<int>((*CurItemProtos).size())) {
                *CurProtoScroll = static_cast<int>((*CurItemProtos).size()) - 1;
            }
        }
        else if (IsTileMode() && !CurTileHashes->empty()) {
            (*CurProtoScroll)++;
            if (*CurProtoScroll >= static_cast<int>(CurTileHashes->size())) {
                *CurProtoScroll = static_cast<int>(CurTileHashes->size()) - 1;
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
        if (IsObjectMode() && !(*CurItemProtos).empty()) {
            (*CurProtoScroll) += ProtosOnScreen;
            if (*CurProtoScroll >= static_cast<int>((*CurItemProtos).size())) {
                *CurProtoScroll = static_cast<int>((*CurItemProtos).size()) - 1;
            }
        }
        else if (IsTileMode() && !CurTileHashes->empty()) {
            (*CurProtoScroll) += ProtosOnScreen;
            if (*CurProtoScroll >= static_cast<int>(CurTileHashes->size())) {
                *CurProtoScroll = static_cast<int>(CurTileHashes->size()) - 1;
            }
        }
        else if (IsCritMode() && !CurNpcProtos->empty()) {
            (*CurProtoScroll) += ProtosOnScreen;
            if (*CurProtoScroll >= static_cast<int>(CurNpcProtos->size())) {
                *CurProtoScroll = static_cast<int>(CurNpcProtos->size()) - 1;
            }
        }
        else if (IntMode == INT_MODE_INCONT) {
            InContScroll += ProtosOnScreen;
        }
        else if (IntMode == INT_MODE_LIST) {
            ListScroll += ProtosOnScreen;
        }
    }
    else if (IsCurInRect(IntBShowItem, IntX, IntY)) {
        Settings.ShowItem = !Settings.ShowItem;
        HexMngr.RefreshMap();
    }
    else if (IsCurInRect(IntBShowScen, IntX, IntY)) {
        Settings.ShowScen = !Settings.ShowScen;
        HexMngr.RefreshMap();
    }
    else if (IsCurInRect(IntBShowWall, IntX, IntY)) {
        Settings.ShowWall = !Settings.ShowWall;
        HexMngr.RefreshMap();
    }
    else if (IsCurInRect(IntBShowCrit, IntX, IntY)) {
        Settings.ShowCrit = !Settings.ShowCrit;
        HexMngr.RefreshMap();
    }
    else if (IsCurInRect(IntBShowTile, IntX, IntY)) {
        Settings.ShowTile = !Settings.ShowTile;
        HexMngr.RefreshMap();
    }
    else if (IsCurInRect(IntBShowRoof, IntX, IntY)) {
        Settings.ShowRoof = !Settings.ShowRoof;
        HexMngr.RefreshMap();
    }
    else if (IsCurInRect(IntBShowFast, IntX, IntY)) {
        Settings.ShowFast = !Settings.ShowFast;
        HexMngr.RefreshMap();
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
    if (IntHold == INT_SELECT && HexMngr.GetHexPixel(Settings.MouseX, Settings.MouseY, SelectHX2, SelectHY2)) {
        if (CurMode == CUR_MODE_DEFAULT) {
            if (SelectHX1 != SelectHX2 || SelectHY1 != SelectHY2) {
                HexMngr.ClearHexTrack();
                UShortPairVec h;

                if (SelectType == SELECT_TYPE_OLD) {
                    const int fx = std::min(SelectHX1, SelectHX2);
                    const int tx = std::max(SelectHX1, SelectHX2);
                    const int fy = std::min(SelectHY1, SelectHY2);
                    const int ty = std::max(SelectHY1, SelectHY2);

                    for (auto i = fx; i <= tx; i++) {
                        for (auto j = fy; j <= ty; j++) {
                            h.push_back(std::make_pair(i, j));
                        }
                    }
                }
                else // SELECT_TYPE_NEW
                {
                    HexMngr.GetHexesRect(Rect(SelectHX1, SelectHY1, SelectHX2, SelectHY2), h);
                }

                ItemHexViewVec items;
                CritterViewVec critters;
                for (auto& i : h) {
                    const auto hx = i.first;
                    const auto hy = i.second;

                    // Items, critters
                    HexMngr.GetItems(hx, hy, items);
                    HexMngr.GetCritters(hx, hy, critters, FIND_ALL);

                    // Tile, roof
                    if (IsSelectTile && Settings.ShowTile) {
                        SelectAddTile(hx, hy, false);
                    }
                    if (IsSelectRoof && Settings.ShowRoof) {
                        SelectAddTile(hx, hy, true);
                    }
                }

                for (auto& item : items) {
                    const auto pid = item->GetProtoId();
                    if (HexMngr.IsIgnorePid(pid)) {
                        continue;
                    }
                    if (!Settings.ShowFast && HexMngr.IsFastPid(pid)) {
                        continue;
                    }

                    if (!item->IsAnyScenery() && IsSelectItem && Settings.ShowItem) {
                        SelectAddItem(item);
                    }
                    else if (item->IsScenery() && IsSelectScen && Settings.ShowScen) {
                        SelectAddItem(item);
                    }
                    else if (item->IsWall() && IsSelectWall && Settings.ShowWall) {
                        SelectAddItem(item);
                    }
                    else if (Settings.ShowFast && HexMngr.IsFastPid(pid)) {
                        SelectAddItem(item);
                    }
                }

                for (auto& critter : critters) {
                    if (IsSelectCrit && Settings.ShowCrit) {
                        SelectAddCrit(critter);
                    }
                }
            }
            else {
                ItemHexView* item = nullptr;
                CritterView* cr = nullptr;
                HexMngr.GetSmthPixel(Settings.MouseX, Settings.MouseY, item, cr);

                if (item != nullptr) {
                    if (!HexMngr.IsIgnorePid(item->GetProtoId())) {
                        SelectAddItem(item);
                    }
                }
                else if (cr != nullptr) {
                    SelectAddCrit(cr);
                }
            }

            // Crits or item container
            // Todo: need attention!
            // if (!SelectedEntities.empty() && !SelectedEntities[0]->GetChildren().empty())
            //    IntSetMode(INT_MODE_INCONT);
        }

        HexMngr.RefreshMap();
    }

    IntHold = INT_NONE;
}

void FOMapper::IntMouseMove()
{
    if (IntHold == INT_SELECT) {
        HexMngr.ClearHexTrack();
        if (!HexMngr.GetHexPixel(Settings.MouseX, Settings.MouseY, SelectHX2, SelectHY2)) {
            if ((SelectHX2 != 0u) || (SelectHY2 != 0u)) {
                HexMngr.RefreshMap();
                SelectHX2 = SelectHY2 = 0;
            }
            return;
        }

        if (CurMode == CUR_MODE_DEFAULT) {
            if (SelectHX1 != SelectHX2 || SelectHY1 != SelectHY2) {
                if (SelectType == SELECT_TYPE_OLD) {
                    const int fx = std::min(SelectHX1, SelectHX2);
                    const int tx = std::max(SelectHX1, SelectHX2);
                    const int fy = std::min(SelectHY1, SelectHY2);
                    const int ty = std::max(SelectHY1, SelectHY2);

                    for (auto i = fx; i <= tx; i++) {
                        for (auto j = fy; j <= ty; j++) {
                            HexMngr.GetHexTrack(i, j) = 1;
                        }
                    }
                }
                else if (SelectType == SELECT_TYPE_NEW) {
                    UShortPairVec h;
                    HexMngr.GetHexesRect(Rect(SelectHX1, SelectHY1, SelectHX2, SelectHY2), h);

                    for (auto& i : h) {
                        HexMngr.GetHexTrack(i.first, i.second) = 1;
                    }
                }

                HexMngr.RefreshMap();
            }
        }
        else if (CurMode == CUR_MODE_MOVE_SELECTION) {
            auto offs_hx = static_cast<int>(SelectHX2) - static_cast<int>(SelectHX1);
            auto offs_hy = static_cast<int>(SelectHY2) - static_cast<int>(SelectHY1);
            auto offs_x = Settings.MouseX - SelectX;
            auto offs_y = Settings.MouseY - SelectY;
            if (SelectMove(!Keyb.ShiftDwn, offs_hx, offs_hy, offs_x, offs_y)) {
                SelectHX1 += offs_hx;
                SelectHY1 += offs_hy;
                SelectX += offs_x;
                SelectY += offs_y;
                HexMngr.RefreshMap();
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

auto FOMapper::GetTabIndex() -> uint
{
    if (IntMode < TAB_COUNT) {
        return TabsActive[IntMode]->Index;
    }
    return TabIndex[IntMode];
}

void FOMapper::SetTabIndex(uint index)
{
    if (IntMode < TAB_COUNT) {
        TabsActive[IntMode]->Index = index;
    }
    TabIndex[IntMode] = index;
}

void FOMapper::RefreshCurProtos()
{
    // Select protos and scroll
    CurItemProtos = nullptr;
    CurProtoScroll = nullptr;
    CurTileHashes = nullptr;
    CurTileNames = nullptr;
    CurNpcProtos = nullptr;
    InContItem = nullptr;

    if (IntMode >= 0 && IntMode < TAB_COUNT) {
        auto* stab = TabsActive[IntMode];
        if (!stab->TileNames.empty()) {
            CurTileNames = &stab->TileNames;
            CurTileHashes = &stab->TileHashes;
        }
        else if (!stab->NpcProtos.empty()) {
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

    // Update fast pids
    HexMngr.ClearFastPids();
    auto& fast_pids = TabsActive[INT_MODE_FAST]->ItemProtos;
    for (auto& fast_pid : fast_pids) {
        HexMngr.AddFastPid(fast_pid->ProtoId);
    }

    // Update ignore pids
    HexMngr.ClearIgnorePids();
    auto& ignore_pids = TabsActive[INT_MODE_IGNORE]->ItemProtos;
    for (auto& ignore_pid : ignore_pids) {
        HexMngr.AddIgnorePid(ignore_pid->ProtoId);
    }

    // Refresh map
    if (HexMngr.IsMapLoaded()) {
        HexMngr.RefreshMap();
    }
}

void FOMapper::IntSetMode(int mode)
{
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
            SubTabsX = IntBCust[mode - INT_MODE_CUSTOM0].CenterX(), SubTabsY = IntBCust[mode - INT_MODE_CUSTOM0].T;
        }
        else if (mode == INT_MODE_ITEM) {
            SubTabsX = IntBItem.CenterX(), SubTabsY = IntBItem.T;
        }
        else if (mode == INT_MODE_TILE) {
            SubTabsX = IntBTile.CenterX(), SubTabsY = IntBTile.T;
        }
        else if (mode == INT_MODE_CRIT) {
            SubTabsX = IntBCrit.CenterX(), SubTabsY = IntBCrit.T;
        }
        else if (mode == INT_MODE_FAST) {
            SubTabsX = IntBFast.CenterX(), SubTabsY = IntBFast.T;
        }
        else if (mode == INT_MODE_IGNORE) {
            SubTabsX = IntBIgnore.CenterX(), SubTabsY = IntBIgnore.T;
        }
        else {
            SubTabsX = SubTabsY = 0;
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

void FOMapper::MoveEntity(Entity* entity, ushort hx, ushort hy)
{
    if (hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight()) {
        return;
    }

    if (entity->Type == EntityType::CritterView) {
        auto* cr = dynamic_cast<CritterView*>(entity);
        if (cr->IsDead() || (HexMngr.GetField(hx, hy).Crit == nullptr)) {
            HexMngr.RemoveCritter(cr);
            cr->SetHexX(hx);
            cr->SetHexY(hy);
            HexMngr.SetCritter(cr);
        }
    }
    else if (entity->Type == EntityType::ItemHexView) {
        auto* item = dynamic_cast<ItemHexView*>(entity);
        HexMngr.DeleteItem(item, false, nullptr);
        item->SetHexX(hx);
        item->SetHexY(hy);
        HexMngr.PushItem(item);
    }
}

void FOMapper::DeleteEntity(Entity* entity)
{
    const auto it = std::find(SelectedEntities.begin(), SelectedEntities.end(), entity);
    if (it != SelectedEntities.end()) {
        SelectedEntities.erase(it);
    }

    if (entity->Type == EntityType::CritterView) {
        HexMngr.DeleteCritter(entity->Id);
    }
    else if (entity->Type == EntityType::ItemHexView) {
        HexMngr.FinishItem(entity->Id, false);
    }
}

void FOMapper::SelectClear()
{
    // Clear map objects
    for (auto& entity : SelectedEntities) {
        if (entity->Type == EntityType::ItemHexView) {
            (dynamic_cast<ItemHexView*>(entity))->RestoreAlpha();
        }
        else if (entity->Type == EntityType::CritterView) {
            (dynamic_cast<CritterView*>(entity))->Alpha = 0xFF;
        }
    }
    SelectedEntities.clear();

    // Clear tiles
    if (!SelectedTile.empty()) {
        HexMngr.ParseSelTiles();
    }
    SelectedTile.clear();
}

void FOMapper::SelectAddItem(ItemHexView* item)
{
    RUNTIME_ASSERT(item);
    SelectAdd(item);
}

void FOMapper::SelectAddCrit(CritterView* npc)
{
    RUNTIME_ASSERT(npc);
    SelectAdd(npc);
}

void FOMapper::SelectAddTile(ushort hx, ushort hy, bool is_roof)
{
    auto& f = HexMngr.GetField(hx, hy);
    if (!is_roof && (f.GetTilesCount(false) == 0u)) {
        return;
    }
    if (is_roof && (f.GetTilesCount(true) == 0u)) {
        return;
    }

    // Helper
    for (auto& stile : SelectedTile) {
        if (stile.HexX == hx && stile.HexY == hy && stile.IsRoof == is_roof) {
            return;
        }
    }
    SelectedTile.push_back(SelMapTile(hx, hy, is_roof));

    // Select
    auto& tiles = HexMngr.GetTiles(hx, hy, is_roof);
    for (auto& tile : tiles) {
        tile.IsSelected = true;
    }
}

void FOMapper::SelectAdd(Entity* entity)
{
    const auto it = std::find(SelectedEntities.begin(), SelectedEntities.end(), entity);
    if (it == SelectedEntities.end()) {
        SelectedEntities.push_back(entity);

        if (entity->Type == EntityType::CritterView) {
            (dynamic_cast<CritterView*>(entity))->Alpha = HexMngr.SelectAlpha;
        }
        if (entity->Type == EntityType::ItemHexView) {
            (dynamic_cast<ItemHexView*>(entity))->Alpha = HexMngr.SelectAlpha;
        }
    }
}

void FOMapper::SelectErase(Entity* entity)
{
    const auto it = std::find(SelectedEntities.begin(), SelectedEntities.end(), entity);
    if (it != SelectedEntities.end()) {
        SelectedEntities.erase(it);

        if (entity->Type == EntityType::CritterView) {
            (dynamic_cast<CritterView*>(entity))->Alpha = 0xFF;
        }
        if (entity->Type == EntityType::ItemHexView) {
            (dynamic_cast<ItemHexView*>(entity))->RestoreAlpha();
        }
    }
}

void FOMapper::SelectAll()
{
    SelectClear();

    for (uint i = 0; i < HexMngr.GetWidth(); i++) {
        for (uint j = 0; j < HexMngr.GetHeight(); j++) {
            if (IsSelectTile && Settings.ShowTile) {
                SelectAddTile(i, j, false);
            }
            if (IsSelectRoof && Settings.ShowRoof) {
                SelectAddTile(i, j, true);
            }
        }
    }

    auto& items = HexMngr.GetItems();
    for (auto& item : items) {
        if (HexMngr.IsIgnorePid(item->GetProtoId())) {
            continue;
        }

        if (!item->IsAnyScenery() && IsSelectItem && Settings.ShowItem) {
            SelectAddItem(item);
        }
        else if (item->IsScenery() && IsSelectScen && Settings.ShowScen) {
            SelectAddItem(item);
        }
        else if (item->IsWall() && IsSelectWall && Settings.ShowWall) {
            SelectAddItem(item);
        }
    }

    if (IsSelectCrit && Settings.ShowCrit) {
        auto& crits = HexMngr.GetCritters();
        for (auto& crit : crits) {
            SelectAddCrit(crit.second);
        }
    }

    HexMngr.RefreshMap();
}

struct TileToMove
{
    Field* field {};
    Field::Tile tile;
    MapTileVec* ptiles {};
    MapTile ptile;
    bool roof {};
    TileToMove() { }
    TileToMove(Field* f, Field::Tile& t, MapTileVec* pts, MapTile& pt, bool r) : field(f), tile(t), ptiles(pts), ptile(pt), roof(r) { }
};

auto FOMapper::SelectMove(bool hex_move, int& offs_hx, int& offs_hy, int& offs_x, int& offs_y) -> bool
{
    if (!hex_move && ((offs_x == 0) && (offs_y == 0))) {
        return false;
    }
    if (hex_move && ((offs_hx == 0) && (offs_hy == 0))) {
        return false;
    }

    // Tile step
    if (hex_move && !SelectedTile.empty()) {
        if (abs(offs_hx) < Settings.MapTileStep && abs(offs_hy) < Settings.MapTileStep) {
            return false;
        }
        offs_hx -= offs_hx % Settings.MapTileStep;
        offs_hy -= offs_hy % Settings.MapTileStep;
    }

    // Setup hex moving switcher
    auto switcher = 0;
    if (!SelectedEntities.empty()) {
        switcher = (SelectedEntities[0]->Type == EntityType::CritterView ? (dynamic_cast<CritterView*>(SelectedEntities[0]))->GetHexX() : (dynamic_cast<ItemHexView*>(SelectedEntities[0]))->GetHexX()) % 2;
    }
    else if (!SelectedTile.empty()) {
        switcher = SelectedTile[0].HexX % 2;
    }

    // Change moving speed on zooming
    if (!hex_move) {
        static auto small_ox = 0.0f;
        static auto small_oy = 0.0f;
        auto ox = static_cast<float>(offs_x) * Settings.SpritesZoom + small_ox;
        auto oy = static_cast<float>(offs_y) * Settings.SpritesZoom + small_oy;
        if ((offs_x != 0) && fabs(ox) < 1.0f) {
            small_ox = ox;
        }
        else {
            small_ox = 0.0f;
        }
        if ((offs_y != 0) && fabs(oy) < 1.0f) {
            small_oy = oy;
        }
        else {
            small_oy = 0.0f;
        }
        offs_x = static_cast<int>(ox);
        offs_y = static_cast<int>(oy);
    }
    // Check borders
    else {
        // Objects
        for (auto& entity : SelectedEntities) {
            int hx = (entity->Type == EntityType::CritterView ? (dynamic_cast<CritterView*>(entity))->GetHexX() : (dynamic_cast<ItemHexView*>(entity))->GetHexX());
            int hy = (entity->Type == EntityType::CritterView ? (dynamic_cast<CritterView*>(entity))->GetHexY() : (dynamic_cast<ItemHexView*>(entity))->GetHexY());
            if (Settings.MapHexagonal) {
                auto sw = switcher;
                for (auto k = 0, l = abs(offs_hx); k < l; k++, sw++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hx > 0 ? ((sw & 1) != 0 ? 4 : 3) : ((sw & 1) != 0 ? 0 : 1));
                }
                for (auto k = 0, l = abs(offs_hy); k < l; k++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hy > 0 ? 2 : 5);
                }
            }
            else {
                hx += offs_hx;
                hy += offs_hy;
            }
            if (hx < 0 || hy < 0 || hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight()) {
                return false; // Disable moving
            }
        }

        // Tiles
        for (auto& stile : SelectedTile) {
            int hx = stile.HexX;
            int hy = stile.HexY;
            if (Settings.MapHexagonal) {
                auto sw = switcher;
                for (auto k = 0, l = abs(offs_hx); k < l; k++, sw++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hx > 0 ? ((sw & 1) != 0 ? 4 : 3) : ((sw & 1) != 0 ? 0 : 1));
                }
                for (auto k = 0, l = abs(offs_hy); k < l; k++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hy > 0 ? 2 : 5);
                }
            }
            else {
                hx += offs_hx;
                hy += offs_hy;
            }
            if (hx < 0 || hy < 0 || hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight()) {
                return false; // Disable moving
            }
        }
    }

    // Move map objects
    for (auto& entity : SelectedEntities) {
        if (!hex_move) {
            if (entity->Type != EntityType::ItemHexView) {
                continue;
            }

            auto* item = dynamic_cast<ItemHexView*>(entity);
            auto ox = item->GetOffsetX() + offs_x;
            auto oy = item->GetOffsetY() + offs_y;
            if (Keyb.AltDwn) {
                ox = oy = 0;
            }

            item->SetOffsetX(ox);
            item->SetOffsetY(oy);
            item->RefreshAnim();
        }
        else {
            int hx = (entity->Type == EntityType::CritterView ? (dynamic_cast<CritterView*>(entity))->GetHexX() : (dynamic_cast<ItemHexView*>(entity))->GetHexX());
            int hy = (entity->Type == EntityType::CritterView ? (dynamic_cast<CritterView*>(entity))->GetHexY() : (dynamic_cast<ItemHexView*>(entity))->GetHexY());
            if (Settings.MapHexagonal) {
                auto sw = switcher;
                for (auto k = 0, l = abs(offs_hx); k < l; k++, sw++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hx > 0 ? ((sw & 1) != 0 ? 4 : 3) : ((sw & 1) != 0 ? 0 : 1));
                }
                for (auto k = 0, l = abs(offs_hy); k < l; k++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hy > 0 ? 2 : 5);
                }
            }
            else {
                hx += offs_hx;
                hy += offs_hy;
            }
            hx = std::clamp(hx, 0, HexMngr.GetWidth() - 1);
            hy = std::clamp(hy, 0, HexMngr.GetHeight() - 1);

            if (entity->Type == EntityType::ItemHexView) {
                auto* item = dynamic_cast<ItemHexView*>(entity);
                HexMngr.DeleteItem(item, false, nullptr);
                item->SetHexX(hx);
                item->SetHexY(hy);
                HexMngr.PushItem(item);
            }
            else if (entity->Type == EntityType::CritterView) {
                auto* cr = dynamic_cast<CritterView*>(entity);
                HexMngr.RemoveCritter(cr);
                cr->SetHexX(hx);
                cr->SetHexY(hy);
                HexMngr.SetCritter(cr);
            }
        }
    }

    // Move tiles
    vector<TileToMove> tiles_to_move;
    tiles_to_move.reserve(1000);

    for (auto& stile : SelectedTile) {
        if (!hex_move) {
            auto& f = HexMngr.GetField(stile.HexX, stile.HexY);
            auto& tiles = HexMngr.GetTiles(stile.HexX, stile.HexY, stile.IsRoof);

            for (uint k = 0, l = static_cast<uint>(tiles.size()); k < l; k++) {
                if (tiles[k].IsSelected) {
                    auto ox = tiles[k].OffsX + offs_x;
                    auto oy = tiles[k].OffsY + offs_y;
                    if (Keyb.AltDwn) {
                        ox = oy = 0;
                    }

                    tiles[k].OffsX = ox;
                    tiles[k].OffsY = oy;
                    auto ftile = f.GetTile(k, stile.IsRoof);
                    f.EraseTile(k, stile.IsRoof);
                    f.AddTile(ftile.Anim, ox, oy, ftile.Layer, stile.IsRoof);
                }
            }
        }
        else {
            int hx = stile.HexX;
            int hy = stile.HexY;
            if (Settings.MapHexagonal) {
                auto sw = switcher;
                for (auto k = 0, l = abs(offs_hx); k < l; k++, sw++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hx > 0 ? ((sw & 1) != 0 ? 4 : 3) : ((sw & 1) != 0 ? 0 : 1));
                }
                for (auto k = 0, l = abs(offs_hy); k < l; k++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hy > 0 ? 2 : 5);
                }
            }
            else {
                hx += offs_hx;
                hy += offs_hy;
            }
            hx = std::clamp(hx, 0, HexMngr.GetWidth() - 1);
            hy = std::clamp(hy, 0, HexMngr.GetHeight() - 1);
            if (stile.HexX == hx && stile.HexY == hy) {
                continue;
            }

            auto& f = HexMngr.GetField(stile.HexX, stile.HexY);
            auto& tiles = HexMngr.GetTiles(stile.HexX, stile.HexY, stile.IsRoof);

            for (uint k = 0; k < tiles.size();) {
                if (tiles[k].IsSelected) {
                    tiles[k].HexX = hx;
                    tiles[k].HexY = hy;
                    auto& ftile = f.GetTile(k, stile.IsRoof);
                    tiles_to_move.push_back(TileToMove(&HexMngr.GetField(hx, hy), ftile, &HexMngr.GetTiles(hx, hy, stile.IsRoof), tiles[k], stile.IsRoof));
                    tiles.erase(tiles.begin() + k);
                    f.EraseTile(k, stile.IsRoof);
                }
                else {
                    k++;
                }
            }
            stile.HexX = hx;
            stile.HexY = hy;
        }
    }

    for (auto& ttm : tiles_to_move) {
        ttm.field->AddTile(ttm.tile.Anim, ttm.tile.OffsX, ttm.tile.OffsY, ttm.tile.Layer, ttm.roof);
        ttm.ptiles->push_back(ttm.ptile);
    }
    return true;
}

void FOMapper::SelectDelete()
{
    auto entities = SelectedEntities;
    for (auto& entity : entities) {
        DeleteEntity(entity);
    }

    for (auto& stile : SelectedTile) {
        auto& f = HexMngr.GetField(stile.HexX, stile.HexY);
        auto& tiles = HexMngr.GetTiles(stile.HexX, stile.HexY, stile.IsRoof);

        for (uint k = 0; k < tiles.size();) {
            if (tiles[k].IsSelected) {
                tiles.erase(tiles.begin() + k);
                f.EraseTile(k, stile.IsRoof);
            }
            else {
                k++;
            }
        }
    }

    SelectedEntities.clear();
    SelectedTile.clear();
    HexMngr.ClearSelTiles();
    SelectClear();
    HexMngr.RefreshMap();
    IntHold = INT_NONE;
    CurMode = CUR_MODE_DEFAULT;
}

auto FOMapper::AddCritter(hash pid, ushort hx, ushort hy) -> CritterView*
{
    RUNTIME_ASSERT(ActiveMap);

    const auto* proto = ProtoMngr.GetProtoCritter(pid);
    if (proto == nullptr) {
        return nullptr;
    }

    if (hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight()) {
        return nullptr;
    }
    if (HexMngr.GetField(hx, hy).Crit != nullptr) {
        return nullptr;
    }

    SelectClear();

    CritterView* cr = 0; // Todo: need attention!
    // new CritterView(--((ProtoMap*)ActiveMap->Proto)->LastEntityId, proto, Settings, SprMngr, ResMngr);
    cr->SetHexX(hx);
    cr->SetHexY(hy);
    cr->SetDir(NpcDir);
    cr->SetCond(COND_ALIVE);
    cr->Init();

    HexMngr.AddCritter(cr);
    SelectAdd(cr);

    HexMngr.RefreshMap();
    CurMode = CUR_MODE_DEFAULT;

    return cr;
}

auto FOMapper::AddItem(hash pid, ushort hx, ushort hy, Entity* owner) -> ItemView*
{
    RUNTIME_ASSERT(ActiveMap);

    // Checks
    const auto* proto_item = ProtoMngr.GetProtoItem(pid);
    if (proto_item == nullptr) {
        return nullptr;
    }
    if ((owner == nullptr) && (hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight())) {
        return nullptr;
    }
    if (proto_item->IsStatic() && (owner != nullptr)) {
        return nullptr;
    }

    // Clear selection
    if (owner == nullptr) {
        SelectClear();
    }

    // Create
    ItemView* item = nullptr;
    if (owner != nullptr) {
        // Todo: need attention!
        /*item = new ItemView(--((ProtoMap*)ActiveMap->Proto)->LastEntityId, proto_item);
        if (owner->Type == EntityType::CritterView)
            ((CritterView*)owner)->AddItem(item);
        else if (owner->Type == EntityType::Item || owner->Type == EntityType::ItemHexView)
            ((ItemView*)owner)->ContSetItem(item);*/
    }
    else {
        // Todo: need attention!
        // uint id = HexMngr.AddItem(--((ProtoMap*)ActiveMap->Proto)->LastEntityId, pid, hx, hy, 0, nullptr);
        // item = HexMngr.GetItemById(id);
    }

    // Select
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

void FOMapper::AddTile(hash name, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof)
{
    RUNTIME_ASSERT(ActiveMap);

    hx -= hx % Settings.MapTileStep;
    hy -= hy % Settings.MapTileStep;

    if (hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight()) {
        return;
    }

    SelectClear();

    HexMngr.SetTile(name, hx, hy, ox, oy, layer, is_roof, false);
    CurMode = CUR_MODE_DEFAULT;
}

auto FOMapper::CloneEntity(Entity* entity) -> Entity*
{
    RUNTIME_ASSERT(ActiveMap);

    int hx = (entity->Type == EntityType::CritterView ? (dynamic_cast<CritterView*>(entity))->GetHexX() : (dynamic_cast<ItemHexView*>(entity))->GetHexX());
    int hy = (entity->Type == EntityType::CritterView ? (dynamic_cast<CritterView*>(entity))->GetHexY() : (dynamic_cast<ItemHexView*>(entity))->GetHexY());
    if (hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight()) {
        return nullptr;
    }

    Entity* owner = nullptr;
    if (entity->Type == EntityType::CritterView) {
        if (HexMngr.GetField(hx, hy).Crit != nullptr) {
            auto place_founded = false;
            for (auto d = 0; d < 6; d++) {
                ushort hx_ = hx;
                ushort hy_ = hy;
                GeomHelper.MoveHexByDir(hx_, hy_, d, HexMngr.GetWidth(), HexMngr.GetHeight());
                if (HexMngr.GetField(hx_, hy_).Crit == nullptr) {
                    hx = hx_;
                    hy = hy_;
                    place_founded = true;
                    break;
                }
            }
            if (!place_founded) {
                return nullptr;
            }
        }

        CritterView* cr = 0; // Todo: need attention!
                             // new CritterView(
                             //--((ProtoMap*)ActiveMap->Proto)->LastEntityId, (ProtoCritter*)entity->Proto, Settings,
                             // SprMngr, ResMngr);
        cr->Props = (dynamic_cast<CritterView*>(entity))->Props;
        cr->SetHexX(hx);
        cr->SetHexY(hy);
        cr->Init();
        HexMngr.AddCritter(cr);
        SelectAdd(cr);
        owner = cr;
    }
    else if (entity->Type == EntityType::ItemHexView) {
        const uint id = 0; // Todo: need attention!
        // HexMngr.AddItem(
        //  --((ProtoMap*)ActiveMap->Proto)->LastEntityId, entity->GetProtoId(), hx, hy, false, nullptr);
        auto* item = HexMngr.GetItemById(id);
        SelectAdd(item);
        owner = item;
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }

    auto* pmap = (ProtoMap*)ActiveMap->Proto;
    std::function<void(Entity*, Entity*)> add_entity_children = [&add_entity_children, &pmap](Entity* from, Entity* to) {
        // Todo: need attention!
        /*for (auto& from_child : from->GetChildren())
        {
            RUNTIME_ASSERT(from_child->Type == EntityType::Item);
            ItemView* to_child = new ItemView(--pmap->LastEntityId, (ProtoItem*)from_child->Proto);
            to_child->Props = from_child->Props;
            if (to->Type == EntityType::CritterView)
                ((CritterView*)to)->AddItem(to_child);
            else
                ((ItemView*)to)->ContSetItem(to_child);
            add_entity_children(from_child, to_child);
        }*/
    };
    add_entity_children(entity, owner);

    return owner;
}

void FOMapper::BufferCopy()
{
    // Clear buffers
    std::function<void(EntityBuf*)> free_entity = [&free_entity](EntityBuf* entity_buf) {
        delete entity_buf->Props;
        for (auto& child : entity_buf->Children) {
            free_entity(child);
            delete child;
        }
    };
    for (auto& entity_buf : EntitiesBuffer) {
        free_entity(&entity_buf);
    }
    EntitiesBuffer.clear();
    TilesBuffer.clear();

    // Add entities to buffer
    std::function<void(EntityBuf*, Entity*)> add_entity = [&add_entity](EntityBuf* entity_buf, Entity* entity) {
        entity_buf->HexX = (entity->Type == EntityType::CritterView ? (dynamic_cast<CritterView*>(entity))->GetHexX() : (dynamic_cast<ItemHexView*>(entity))->GetHexX());
        entity_buf->HexY = (entity->Type == EntityType::CritterView ? (dynamic_cast<CritterView*>(entity))->GetHexY() : (dynamic_cast<ItemHexView*>(entity))->GetHexY());
        entity_buf->Type = entity->Type;
        entity_buf->Proto = entity->Proto;
        entity_buf->Props = new Properties(entity->Props);
        // Todo: need attention!
        /*for (auto& child : entity->GetChildren())
        {
            EntityBuf* child_buf = new EntityBuf();
            add_entity(child_buf, child);
            entity_buf->Children.push_back(child_buf);
        }*/
    };
    for (auto& entity : SelectedEntities) {
        EntitiesBuffer.push_back(EntityBuf());
        add_entity(&EntitiesBuffer.back(), entity);
    }

    // Add tiles to buffer
    for (auto& tile : SelectedTile) {
        auto& hex_tiles = HexMngr.GetTiles(tile.HexX, tile.HexY, tile.IsRoof);
        for (auto& hex_tile : hex_tiles) {
            if (hex_tile.IsSelected) {
                TileBuf tb {};
                tb.Name = hex_tile.Name;
                tb.HexX = tile.HexX;
                tb.HexY = tile.HexY;
                tb.OffsX = hex_tile.OffsX;
                tb.OffsY = hex_tile.OffsY;
                tb.Layer = hex_tile.Layer;
                tb.IsRoof = tile.IsRoof;
                TilesBuffer.push_back(tb);
            }
        }
    }
}

void FOMapper::BufferCut()
{
    BufferCopy();
    SelectDelete();
}

void FOMapper::BufferPaste(int, int)
{
    if (ActiveMap == nullptr) {
        return;
    }

    SelectClear();

    // Paste map objects
    for (auto& entity_buf : EntitiesBuffer) {
        if (entity_buf.HexX >= HexMngr.GetWidth() || entity_buf.HexY >= HexMngr.GetHeight()) {
            continue;
        }

        auto hx = entity_buf.HexX;
        auto hy = entity_buf.HexY;

        Entity* owner = nullptr;
        if (entity_buf.Type == EntityType::CritterView) {
            if (HexMngr.GetField(hx, hy).Crit != nullptr) {
                auto place_founded = false;
                for (int d = 0; d < 6; d++) {
                    ushort hx_ = entity_buf.HexX;
                    ushort hy_ = entity_buf.HexY;
                    GeomHelper.MoveHexByDir(hx_, hy_, d, HexMngr.GetWidth(), HexMngr.GetHeight());
                    if (HexMngr.GetField(hx_, hy_).Crit == nullptr) {
                        hx = hx_;
                        hy = hy_;
                        place_founded = true;
                        break;
                    }
                }
                if (!place_founded) {
                    continue;
                }
            }

            CritterView* cr = 0; // Todo: need attention!
            // new CritterView(--((ProtoMap*)ActiveMap->Proto)->LastEntityId,
            //  (ProtoCritter*)entity_buf.Proto, Settings, SprMngr, ResMngr);
            cr->Props = *entity_buf.Props;
            cr->SetHexX(hx);
            cr->SetHexY(hy);
            cr->Init();
            HexMngr.AddCritter(cr);
            SelectAdd(cr);
            owner = cr;
        }
        else if (entity_buf.Type == EntityType::ItemHexView) {
            const uint id = 0; // Todo: need attention!
            // HexMngr.AddItem(
            //  --((ProtoMap*)ActiveMap->Proto)->LastEntityId, entity_buf.Proto->ProtoId, hx, hy, false, nullptr);
            ItemHexView* item = HexMngr.GetItemById(id);
            item->Props = *entity_buf.Props;
            SelectAdd(item);
            owner = item;
        }

        // Todo: need attention!
        /*auto* pmap = dynamic_cast<ProtoMap*>(ActiveMap->Proto);
        std::function<void(EntityBuf*, Entity*)> add_entity_children = [&add_entity_children, &pmap](EntityBuf* entity_buf) {
            for (auto& child_buf : entity_buf->Children) {
                RUNTIME_ASSERT(child_buf->Type == EntityType::Item);
                ItemView* child = new ItemView(--pmap->LastEntityId, (ProtoItem*)child_buf->Proto);
                child->Props = *child_buf->Props;
                if (entity->Type == EntityType::CritterView)
                    ((CritterView*)entity)->AddItem(child);
                else
                    ((ItemView*)entity)->ContSetItem(child);
                add_entity_children(child_buf, child);
            }
        };
        add_entity_children(&entity_buf, owner);*/
    }

    // Paste tiles
    for (auto& tile_buf : TilesBuffer) {
        if (tile_buf.HexX < HexMngr.GetWidth() && tile_buf.HexY < HexMngr.GetHeight()) {
            // Create
            HexMngr.SetTile(tile_buf.Name, tile_buf.HexX, tile_buf.HexY, tile_buf.OffsX, tile_buf.OffsY, tile_buf.Layer, tile_buf.IsRoof, true);

            // Select helper
            bool sel_added = false;
            for (auto& stile : SelectedTile) {
                if (stile.HexX == tile_buf.HexX && stile.HexY == tile_buf.HexY && stile.IsRoof == tile_buf.IsRoof) {
                    sel_added = true;
                    break;
                }
            }
            if (!sel_added) {
                SelectedTile.push_back(SelMapTile(tile_buf.HexX, tile_buf.HexY, tile_buf.IsRoof));
            }
        }
    }
}

void FOMapper::CurDraw()
{
    switch (CurMode) {
    case CUR_MODE_DEFAULT:
    case CUR_MODE_MOVE_SELECTION: {
        AnyFrames* anim = (CurMode == CUR_MODE_DEFAULT ? CurPDef : CurPHand);
        if (anim != nullptr) {
            SpriteInfo* si = SprMngr.GetSpriteInfo(anim->GetCurSprId(GameTime.GameTick()));
            if (si != nullptr) {
                SprMngr.DrawSprite(anim, Settings.MouseX, Settings.MouseY, COLOR_IFACE);
            }
        }
    } break;
    case CUR_MODE_PLACE_OBJECT:
        if (IsObjectMode() && !(*CurItemProtos).empty()) {
            ProtoItem* proto_item = (*CurItemProtos)[GetTabIndex()];

            ushort hx = 0;
            ushort hy = 0;
            if (!HexMngr.GetHexPixel(Settings.MouseX, Settings.MouseY, hx, hy)) {
                break;
            }

            const uint spr_id = GetProtoItemCurSprId(proto_item);
            SpriteInfo* si = SprMngr.GetSpriteInfo(spr_id);
            if (si != nullptr) {
                const int x = HexMngr.GetField(hx, hy).ScrX - (si->Width / 2) + si->OffsX + (Settings.MapHexWidth / 2) + Settings.ScrOx + proto_item->GetOffsetX();
                const int y = HexMngr.GetField(hx, hy).ScrY - si->Height + si->OffsY + (Settings.MapHexHeight / 2) + Settings.ScrOy + proto_item->GetOffsetY();
                SprMngr.DrawSpriteSize(spr_id, static_cast<int>(x / Settings.SpritesZoom), static_cast<int>(y / Settings.SpritesZoom), static_cast<int>(si->Width / Settings.SpritesZoom), static_cast<int>(si->Height / Settings.SpritesZoom), true, false, 0);
            }
        }
        else if (IsTileMode() && !CurTileHashes->empty()) {
            AnyFrames* anim = ResMngr.GetItemAnim((*CurTileHashes)[GetTabIndex()]);
            if (anim == nullptr) {
                anim = ResMngr.ItemHexDefaultAnim;
            }

            ushort hx = 0;
            ushort hy = 0;
            if (!HexMngr.GetHexPixel(Settings.MouseX, Settings.MouseY, hx, hy)) {
                break;
            }

            SpriteInfo* si = SprMngr.GetSpriteInfo(anim->GetCurSprId(GameTime.GameTick()));
            if (si != nullptr) {
                hx -= hx % Settings.MapTileStep;
                hy -= hy % Settings.MapTileStep;
                int x = HexMngr.GetField(hx, hy).ScrX - (si->Width / 2) + si->OffsX;
                int y = HexMngr.GetField(hx, hy).ScrY - si->Height + si->OffsY;
                if (!DrawRoof) {
                    x += Settings.MapTileOffsX;
                    y += Settings.MapTileOffsY;
                }
                else {
                    x += Settings.MapRoofOffsX;
                    y += Settings.MapRoofOffsY;
                }

                SprMngr.DrawSpriteSize(anim, static_cast<int>((x + Settings.ScrOx) / Settings.SpritesZoom), static_cast<int>((y + Settings.ScrOy) / Settings.SpritesZoom), static_cast<int>(si->Width / Settings.SpritesZoom), static_cast<int>(si->Height / Settings.SpritesZoom), true, false, 0);
            }
        }
        else if (IsCritMode() && !CurNpcProtos->empty()) {
            const hash model_name = (*CurNpcProtos)[GetTabIndex()]->Props.GetValue<hash>(CritterView::PropertyModelName);
            uint spr_id = ResMngr.GetCritSprId(model_name, 1, 1, NpcDir, nullptr);
            if (spr_id == 0u) {
                spr_id = ResMngr.ItemHexDefaultAnim->GetSprId(0);
            }

            ushort hx = 0;
            ushort hy = 0;
            if (!HexMngr.GetHexPixel(Settings.MouseX, Settings.MouseY, hx, hy)) {
                break;
            }

            SpriteInfo* si = SprMngr.GetSpriteInfo(spr_id);
            if (si != nullptr) {
                const int x = HexMngr.GetField(hx, hy).ScrX - (si->Width / 2) + si->OffsX;
                const int y = HexMngr.GetField(hx, hy).ScrY - si->Height + si->OffsY;

                SprMngr.DrawSpriteSize(spr_id, static_cast<int>((x + Settings.ScrOx + (Settings.MapHexWidth / 2)) / Settings.SpritesZoom), static_cast<int>((y + Settings.ScrOy + (Settings.MapHexHeight / 2)) / Settings.SpritesZoom), static_cast<int>(si->Width / Settings.SpritesZoom), static_cast<int>(si->Height / Settings.SpritesZoom), true, false, 0);
            }
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
    if (IntHold == INT_NONE) {
        if (CurMode == CUR_MODE_MOVE_SELECTION) {
            CurMode = CUR_MODE_DEFAULT;
        }
        else if (CurMode == CUR_MODE_PLACE_OBJECT) {
            CurMode = CUR_MODE_DEFAULT;
        }
        else if (CurMode == CUR_MODE_DEFAULT) {
            if (!SelectedEntities.empty() || !SelectedTile.empty()) {
                CurMode = CUR_MODE_MOVE_SELECTION;
            }
            else if (IsObjectMode() || IsTileMode() || IsCritMode()) {
                CurMode = CUR_MODE_PLACE_OBJECT;
            }
        }
    }
}

void FOMapper::CurMMouseDown()
{
    if (SelectedEntities.empty()) {
        NpcDir++;
        if (NpcDir >= Settings.MapDirCount) {
            NpcDir = 0;
        }

        DrawRoof = !DrawRoof;
    }
    else {
        for (auto& entity : SelectedEntities) {
            if (entity->Type == EntityType::CritterView) {
                CritterView* cr = dynamic_cast<CritterView*>(entity);
                auto dir = cr->GetDir() + 1;
                if (dir >= Settings.MapDirCount) {
                    dir = 0;
                }
                cr->ChangeDir(dir, true);
            }
        }
    }
}

auto FOMapper::IsCurInRect(Rect& rect, int ax, int ay) -> bool
{
    return (Settings.MouseX >= rect[0] + ax && Settings.MouseY >= rect[1] + ay && Settings.MouseX <= rect[2] + ax && Settings.MouseY <= rect[3] + ay);
}

auto FOMapper::IsCurInRect(Rect& rect) -> bool
{
    return (Settings.MouseX >= rect[0] && Settings.MouseY >= rect[1] && Settings.MouseX <= rect[2] && Settings.MouseY <= rect[3]);
}

auto FOMapper::IsCurInRectNoTransp(uint spr_id, Rect& rect, int ax, int ay) -> bool
{
    return IsCurInRect(rect, ax, ay) && SprMngr.IsPixNoTransp(spr_id, Settings.MouseX - rect.L - ax, Settings.MouseY - rect.T - ay, false);
}

auto FOMapper::IsCurInInterface() -> bool
{
    if (IntVisible && SubTabsActive && IsCurInRectNoTransp(SubTabsPic->GetCurSprId(GameTime.GameTick()), SubTabsRect, SubTabsX, SubTabsY)) {
        return true;
    }
    if (IntVisible && IsCurInRectNoTransp(IntMainPic->GetCurSprId(GameTime.GameTick()), IntWMain, IntX, IntY)) {
        return true;
    }
    if (ObjVisible && !SelectedEntities.empty() && IsCurInRectNoTransp(ObjWMainPic->GetCurSprId(GameTime.GameTick()), ObjWMain, ObjX, ObjY)) {
        return true;
    }
    return false;
}

auto FOMapper::GetCurHex(ushort& hx, ushort& hy, bool ignore_interface) -> bool
{
    hx = hy = 0;
    if (!ignore_interface && IsCurInInterface()) {
        return false;
    }
    return HexMngr.GetHexPixel(Settings.MouseX, Settings.MouseY, hx, hy);
}

void FOMapper::ConsoleDraw()
{
    if (ConsoleEdit) {
        SprMngr.DrawSprite(ConsolePic, IntX + ConsolePicX, (IntVisible ? IntY : Settings.ScreenHeight) + ConsolePicY, 0);
    }

    if (ConsoleEdit) {
        string buf = ConsoleStr;
        buf.insert(ConsoleCur, GameTime.FrameTick() % 800 < 400 ? "!" : ".");
        SprMngr.DrawStr(Rect(IntX + ConsoleTextX, (IntVisible ? IntY : Settings.ScreenHeight) + ConsoleTextY, Settings.ScreenWidth, Settings.ScreenHeight), buf, FT_NOBREAK, 0, FONT_DEFAULT);
    }
}

void FOMapper::ConsoleKeyDown(KeyCode dik, const char* dik_text)
{
    if (dik == KeyCode::DIK_RETURN || dik == KeyCode::DIK_NUMPADENTER) {
        if (ConsoleEdit) {
            if (ConsoleStr.empty()) {
                ConsoleEdit = false;
            }
            else {
                // Modify console history
                ConsoleHistory.push_back(ConsoleStr);
                for (uint i = 0; i < ConsoleHistory.size() - 1; i++) {
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
                for (auto& i : ConsoleHistory) {
                    history_str += i + "\n";
                }
                Cache.SetString("mapper_console.txt", history_str);

                // Process command
                bool process_command = true;
                process_command = ScriptSys.ConsoleMessageEvent(ConsoleStr);

                AddMess(ConsoleStr.c_str());
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
    case KeyCode::DIK_UP:
        if (ConsoleHistoryCur - 1 < 0) {
            return;
        }
        ConsoleHistoryCur--;
        ConsoleStr = ConsoleHistory[ConsoleHistoryCur];
        ConsoleCur = static_cast<uint>(ConsoleStr.length());
        return;
    case KeyCode::DIK_DOWN:
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
        Keyb.GetChar(dik, dik_text, ConsoleStr, &ConsoleCur, MAX_CHAT_MESSAGE, KIF_NO_SPEC_SYMBOLS);
        ConsoleLastKey = dik;
        ConsoleLastKeyText = dik_text;
        ConsoleKeyTick = GameTime.FrameTick();
        ConsoleAccelerate = 1;
        return;
    }
}

void FOMapper::ConsoleKeyUp(KeyCode /*key*/)
{
    ConsoleLastKey = KeyCode::DIK_NONE;
    ConsoleLastKeyText = "";
}

void FOMapper::ConsoleProcess()
{
    if (ConsoleLastKey == KeyCode::DIK_NONE) {
        return;
    }

    if (static_cast<int>(GameTime.FrameTick() - ConsoleKeyTick) >= CONSOLE_KEY_TICK - ConsoleAccelerate) {
        ConsoleKeyTick = GameTime.FrameTick();
        ConsoleAccelerate = CONSOLE_MAX_ACCELERATE;
        Keyb.GetChar(ConsoleLastKey, ConsoleLastKeyText, ConsoleStr, &ConsoleCur, MAX_CHAT_MESSAGE, KIF_NO_SPEC_SYMBOLS);
    }
}

void FOMapper::ParseCommand(const string& command)
{
    if (command.empty()) {
        return;
    }

    // Load map
    if (command[0] == '~') {
        string map_name = _str(command.substr(1)).trim();
        if (map_name.empty()) {
            AddMess("Error parse map name.");
            return;
        }

        ProtoMap* pmap = new ProtoMap(_str(map_name).toHash());
        // Todo: need attention!
        /*if (!pmap->EditorLoad(ServerFileMngr, ProtoMngr, SprMngr, ResMngr))
        {
            AddMess("File not found or truncated.");
            return;
        }*/

        SelectClear();

        // Todo: need attention!
        /*if (ActiveMap != nullptr) {
            HexMngr.GetProtoMap(*dynamic_cast<ProtoMap*>(ActiveMap->Proto));
        }
        if (!HexMngr.SetProtoMap(*pmap)) {
            AddMess("Load map fail.");
            return;
        }*/

        HexMngr.FindSetCenter(pmap->GetWorkHexX(), pmap->GetWorkHexY());

        MapView* map = new MapView(0, pmap);
        ActiveMap = map;
        LoadedMaps.push_back(map);

        AddMess("Load map complete.");

        RunMapLoadScript(map);
    }
    // Save map
    else if (command[0] == '^') {
        string map_name = _str(command.substr(1)).trim();
        if (map_name.empty()) {
            AddMess("Error parse map name.");
            return;
        }

        if (ActiveMap == nullptr) {
            AddMess("Map not loaded.");
            return;
        }

        // Todo: need attention!
        // HexMngr.GetProtoMap(*dynamic_cast<ProtoMap*>(ActiveMap->Proto));
        // ((ProtoMap*)ActiveMap->Proto)->EditorSave(ServerFileMngr, map_name);

        AddMess("Save map success.");
        RunMapSaveScript(ActiveMap);
    }
    // Run script
    else if (command[0] == '#') {
        istringstream icmd(command.substr(1));
        string func_name;
        if (!(icmd >> func_name)) {
            AddMess("Function name not typed.");
            return;
        }

        // Reparse module
        /*uint bind_id = ScriptSys.BindByFuncName(func_name, "string %s(string)", true);
        if (bind_id)
        {
            string str = _str(command).substringAfter(' ').trim();
            ScriptSys.PrepareContext(bind_id, "Mapper");
            ScriptSys.SetArgObject(&str);
            if (ScriptSys.RunPrepared())
            {
                string result = *(string*)ScriptSys.GetReturnedRawAddress();
                AddMess(_str("Result: {}", result).c_str());
            }
            else
            {
                AddMess("Script execution fail.");
            }
        }
        else
        {
            AddMess("Function not found.");
            return;
        }*/
    }
    // Critter animations
    else if (command[0] == '@') {
        AddMess("Playing critter animations.");

        if (ActiveMap == nullptr) {
            AddMess("Map not loaded.");
            return;
        }

        IntVec anims = _str(command.substr(1)).splitToInt(' ');
        if (anims.empty()) {
            return;
        }

        if (!SelectedEntities.empty()) {
            for (auto& entity : SelectedEntities) {
                if (entity->Type == EntityType::CritterView) {
                    CritterView* cr = dynamic_cast<CritterView*>(entity);
                    cr->ClearAnim();
                    for (uint j = 0; j < anims.size() / 2; j++) {
                        cr->Animate(anims[j * 2], anims[j * 2 + 1], nullptr);
                    }
                }
            }
        }
        else {
            for (auto& cr_kv : HexMngr.GetCritters()) {
                CritterView* cr = cr_kv.second;
                cr->ClearAnim();
                for (uint j = 0; j < anims.size() / 2; j++) {
                    cr->Animate(anims[j * 2], anims[j * 2 + 1], nullptr);
                }
            }
        }
    }
    // Other
    else if (command[0] == '*') {
        istringstream icommand(command.substr(1));
        string command_ext;
        if (!(icommand >> command_ext)) {
            return;
        }

        if (command_ext == "new") {
            ProtoMap* pmap = new ProtoMap(_str("new").toHash());

            pmap->SetWidth(MAXHEX_DEFAULT);
            pmap->SetHeight(MAXHEX_DEFAULT);

            // Morning	 5.00 -  9.59	 300 - 599
            // Day		10.00 - 18.59	 600 - 1139
            // Evening	19.00 - 22.59	1140 - 1379
            // Nigh		23.00 -  4.59	1380
            IntVec arr = {300, 600, 1140, 1380};
            UCharVec arr2 = {18, 128, 103, 51, 18, 128, 95, 40, 53, 128, 86, 29};
            pmap->SetDayTime(arr);
            pmap->SetDayColor(arr2);

            // Todo: need attention!
            /*if (ActiveMap != nullptr) {
                HexMngr.GetProtoMap(*dynamic_cast<ProtoMap*>(ActiveMap->Proto));
            }

            if (!HexMngr.SetProtoMap(*pmap)) {
                AddMess("Create map fail, see log.");
                return;
            }*/

            AddMess("Create map success.");
            HexMngr.FindSetCenter(150, 150);

            MapView* map = new MapView(0, pmap);
            ActiveMap = map;
            LoadedMaps.push_back(map);
        }
        else if (command_ext == "unload") {
            AddMess("Unload map.");

            auto it = std::find(LoadedMaps.begin(), LoadedMaps.end(), ActiveMap);
            if (it == LoadedMaps.end()) {
                return;
            }

            LoadedMaps.erase(it);
            SelectedEntities.clear();
            ActiveMap->Proto->Release();
            ActiveMap->Release();
            ActiveMap = nullptr;

            if (LoadedMaps.empty()) {
                HexMngr.UnloadMap();
                return;
            }

            // Todo: need attention!
            /*if (HexMngr.SetProtoMap(*dynamic_cast<ProtoMap*>(LoadedMaps[0]->Proto))) {
                ActiveMap = LoadedMaps[0];
                HexMngr.FindSetCenter(ActiveMap->GetWorkHexX(), ActiveMap->GetWorkHexY());
                return;
            }*/
        }
        else if (command_ext == "size" && (ActiveMap != nullptr)) {
            AddMess("Resize map.");

            int maxhx = 0;
            int maxhy = 0;
            if (!(icommand >> maxhx >> maxhy)) {
                AddMess("Invalid args.");
                return;
            }

            // FOMapper::SScriptFunc::Global_ResizeMap(maxhx, maxhy);
        }
    }
    else {
        AddMess("Unknown command.");
    }
}

void FOMapper::AddMess(const char* message_text)
{
    const string str = _str("|{} - {}\n", COLOR_TEXT, message_text);

    const auto dt = Timer::GetCurrentDateTime();
    const string mess_time = _str("{:02}:{:02}:{:02} ", dt.Hour, dt.Minute, dt.Second);

    MessBox.push_back(MessBoxMessage(0, str.c_str(), mess_time.c_str()));
    MessBoxScroll = 0;
    MessBoxGenerate();
}

void FOMapper::MessBoxGenerate()
{
    MessBoxCurText = "";
    if (MessBox.empty()) {
        return;
    }

    const Rect ir(IntWWork[0] + IntX, IntWWork[1] + IntY, IntWWork[2] + IntX, IntWWork[3] + IntY);
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
    if (!IntVisible) {
        return;
    }
    if (MessBoxCurText.empty()) {
        return;
    }

    const uint flags = FT_UPPER | FT_BOTTOM;
    SprMngr.DrawStr(Rect(IntWWork[0] + IntX, IntWWork[1] + IntY, IntWWork[2] + IntX, IntWWork[3] + IntY), MessBoxCurText, flags, 0, FONT_DEFAULT);
}

auto FOMapper::SaveLogFile() -> bool
{
    if (MessBox.empty()) {
        return false;
    }

    const auto dt = Timer::GetCurrentDateTime();
    const string log_path = _str("mapper_messbox_{:02}-{:02}-{}_{:02}-{:02}-{:02}.txt", dt.Day, dt.Month, dt.Year, dt.Hour, dt.Minute, dt.Second);

    DiskFileSystem::ResetCurDir();
    auto f = DiskFileSystem::OpenFile(log_path, true);
    if (!f) {
        return false;
    }

    string cur_mess;
    string fmt_log;
    for (auto& i : MessBox) {
        cur_mess = _str(i.Mess).erase('|', ' ');
        fmt_log += i.Time + string(cur_mess);
    }

    f.Write(fmt_log.c_str(), static_cast<uint>(fmt_log.length()));
    return true;
}

void FOMapper::RunStartScript()
{
    ScriptSys.StartEvent();
}

void FOMapper::RunMapLoadScript(MapView* map)
{
    RUNTIME_ASSERT(map);
    ScriptSys.MapLoadEvent(map);
}

void FOMapper::RunMapSaveScript(MapView* map)
{
    RUNTIME_ASSERT(map);
    ScriptSys.MapSaveEvent(map);
}

void FOMapper::DrawIfaceLayer(uint /*layer*/)
{
    SpritesCanDraw = true;
    ScriptSys.RenderIfaceEvent(); // Todo: mapper render iface layer
    SpritesCanDraw = false;
}

void FOMapper::OnSetItemFlags(Entity* entity, Property* prop, void* /*cur_value*/, void* /*old_value*/)
{
    // IsColorize, IsBadItem, IsShootThru, IsLightThru, IsNoBlock

    ItemView* item = dynamic_cast<ItemView*>(entity);
    if (item->GetAccessory() == ITEM_ACCESSORY_HEX && HexMngr.IsMapLoaded()) {
        ItemHexView* hex_item = dynamic_cast<ItemHexView*>(item);
        bool rebuild_cache = false;
        if (prop == ItemView::PropertyIsColorize) {
            hex_item->RefreshAlpha();
        }
        else if (prop == ItemView::PropertyIsBadItem) {
            hex_item->SetSprite(nullptr);
        }
        else if (prop == ItemView::PropertyIsShootThru) {
            rebuild_cache = true;
        }
        else if (prop == ItemView::PropertyIsLightThru) {
            HexMngr.RebuildLight(), rebuild_cache = true;
        }
        else if (prop == ItemView::PropertyIsNoBlock) {
            rebuild_cache = true;
        }
        if (rebuild_cache) {
            HexMngr.GetField(hex_item->GetHexX(), hex_item->GetHexY()).ProcessCache();
        }
    }
}

void FOMapper::OnSetItemSomeLight(Entity* /*entity*/, Property* /*prop*/, void* /*cur_value*/, void* /*old_value*/)
{
    // IsLight, LightIntensity, LightDistance, LightFlags, LightColor

    if (HexMngr.IsMapLoaded()) {
        HexMngr.RebuildLight();
    }
}

void FOMapper::OnSetItemPicMap(Entity* entity, Property* /*prop*/, void* /*cur_value*/, void* /*old_value*/)
{
    ItemView* item = dynamic_cast<ItemView*>(entity);

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX) {
        ItemHexView* hex_item = dynamic_cast<ItemHexView*>(item);
        hex_item->RefreshAnim();
    }
}

void FOMapper::OnSetItemOffsetXY(Entity* entity, Property* /*prop*/, void* /*cur_value*/, void* /*old_value*/)
{
    // OffsetX, OffsetY

    ItemView* item = dynamic_cast<ItemView*>(entity);

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX && HexMngr.IsMapLoaded()) {
        ItemHexView* hex_item = dynamic_cast<ItemHexView*>(item);
        hex_item->SetAnimOffs();
        HexMngr.ProcessHexBorders(hex_item);
    }
}

void FOMapper::OnSetItemOpened(Entity* entity, Property* /*prop*/, void* cur_value, void* old_value)
{
    ItemView* item = dynamic_cast<ItemView*>(entity);
    const bool cur = *static_cast<bool*>(cur_value);
    const bool old = *static_cast<bool*>(old_value);

    if (item->GetIsCanOpen()) {
        ItemHexView* hex_item = dynamic_cast<ItemHexView*>(item);
        if (!old && cur) {
            hex_item->SetAnimFromStart();
        }
        if (old && !cur) {
            hex_item->SetAnimFromEnd();
        }
    }
}
