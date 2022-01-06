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
#include "MapperScripting.h"
#include "StringUtils.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

#include "sha1.h"
#include "sha2.h"

FOMapper::FOMapper(GlobalSettings& settings) : FOClient(settings, new MapperScriptSystem(this, settings)), IfaceIni("", *this)
{
    HexMngr.EnableMapperMode();

    ProtoMngr = std::make_unique<ProtoManager>(ServerFileMngr, *this);

    Animations.resize(10000);

    // Mouse
    const auto [w, h] = SprMngr.GetWindowSize();
    const auto [x, y] = SprMngr.GetMousePosition();
    Settings.MouseX = std::clamp(x, 0, w - 1);
    Settings.MouseY = std::clamp(y, 0, h - 1);

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
    CurLang.LoadFromFiles(FileMngr, *this, Settings.Language);

    // Prototypes
    // bool protos_ok = ProtoMngr.LoadProtosFromFiles(FileMngr);
    // RUNTIME_ASSERT(protos_ok);

    // Initialize tabs
    const auto& cr_protos = ProtoMngr->GetProtoCritters();
    for (const auto& [pid, proto] : cr_protos) {
        Tabs[INT_MODE_CRIT][DEFAULT_SUB_TAB].NpcProtos.push_back(proto);
        Tabs[INT_MODE_CRIT][proto->CollectionName].NpcProtos.push_back(proto);
    }
    for (auto [pid, proto] : Tabs[INT_MODE_CRIT]) {
        std::sort(proto.NpcProtos.begin(), proto.NpcProtos.end(), [](const ProtoCritter* a, const ProtoCritter* b) { return a->GetName().compare(b->GetName()); });
    }

    const auto& item_protos = ProtoMngr->GetProtoItems();
    for (const auto& [pid, proto] : item_protos) {
        Tabs[INT_MODE_ITEM][DEFAULT_SUB_TAB].ItemProtos.push_back(proto);
        Tabs[INT_MODE_ITEM][proto->CollectionName].ItemProtos.push_back(proto);
    }
    for (auto [pid, proto] : Tabs[INT_MODE_ITEM]) {
        std::sort(proto.ItemProtos.begin(), proto.ItemProtos.end(), [](const ProtoItem* a, const ProtoItem* b) { return a->GetName().compare(b->GetName()); });
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
        auto* pmap = new ProtoMap(ToHashedString(map_name), GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME));
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

            auto* map = new MapView(this, 0, pmap);
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

    // ini = FileMngr.ReadConfigFile("mapper_default.ini", *this);

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

auto FOMapper::IfaceLoadRect(IRect& comp, string_view name) -> bool
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

auto FOMapper::AnimLoad(hstring name, AtlasType res_type) -> uint
{
    auto* anim = ResMngr.GetAnim(name, res_type);
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
    for (auto*& anim : Animations) {
        if (anim != nullptr && anim->ResType == res_type) {
            delete anim;
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
        InputLostEvent.Fire();
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
            script_result = KeyDownEvent.Fire(dikdw, event_text_script);
        }
        if (dikup)
        {
            string event_text_script = event_text;
            script_result = KeyUpEvent.Fire(dikup, event_text_script);
        }

        // Disable keyboard events
        if (!script_result || Settings.DisableKeyboardEvents)
        {
            if (dikdw == KeyCode::Escape && Keyb.ShiftDwn)
                Settings.Quit = true;
            continue;
        }

        // Control keys
        if (dikdw == KeyCode::Rcontrol || dikdw == KeyCode::Lcontrol)
            Keyb.CtrlDwn = true;
        else if (dikdw == KeyCode::Lmenu || dikdw == KeyCode::Rmenu)
            Keyb.AltDwn = true;
        else if (dikdw == KeyCode::Lshift || dikdw == KeyCode::Rshift)
            Keyb.ShiftDwn = true;
        if (dikup == KeyCode::Rcontrol || dikup == KeyCode::Lcontrol)
            Keyb.CtrlDwn = false;
        else if (dikup == KeyCode::Lmenu || dikup == KeyCode::Rmenu)
            Keyb.AltDwn = false;
        else if (dikup == KeyCode::Lshift || dikup == KeyCode::Rshift)
            Keyb.ShiftDwn = false;

        // Hotkeys
        if (!Keyb.AltDwn && !Keyb.CtrlDwn && !Keyb.ShiftDwn)
        {
            switch (dikdw)
            {
            case KeyCode::F1:
                Settings.ShowItem = !Settings.ShowItem;
                HexMngr.RefreshMap();
                break;
            case KeyCode::F2:
                Settings.ShowScen = !Settings.ShowScen;
                HexMngr.RefreshMap();
                break;
            case KeyCode::F3:
                Settings.ShowWall = !Settings.ShowWall;
                HexMngr.RefreshMap();
                break;
            case KeyCode::F4:
                Settings.ShowCrit = !Settings.ShowCrit;
                HexMngr.RefreshMap();
                break;
            case KeyCode::F5:
                Settings.ShowTile = !Settings.ShowTile;
                HexMngr.RefreshMap();
                break;
            case KeyCode::F6:
                Settings.ShowFast = !Settings.ShowFast;
                HexMngr.RefreshMap();
                break;
            case KeyCode::F7:
                IntVisible = !IntVisible;
                break;
            case KeyCode::F8:
                Settings.MouseScroll = !Settings.MouseScroll;
                break;
            case KeyCode::F9:
                ObjVisible = !ObjVisible;
                break;
            case KeyCode::F10:
                HexMngr.SwitchShowHex();
                break;

            // Fullscreen
            case KeyCode::F11:
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
            case KeyCode::F12:
                SprMngr.MinimizeWindow();
                continue;

            case KeyCode::Delete:
                SelectDelete();
                break;
            case KeyCode::Add:
                if (!ConsoleEdit && SelectedEntities.empty())
                {
                    int day_time = HexMngr.GetDayTime();
                    day_time += 60;
                    Globals->SetMinute(day_time % 60);
                    Globals->SetHour(day_time / 60 % 24);
                    ChangeGameTime();
                }
                break;
            case KeyCode::Subtract:
                if (!ConsoleEdit && SelectedEntities.empty())
                {
                    int day_time = HexMngr.GetDayTime();
                    day_time -= 60;
                    Globals->SetMinute(day_time % 60);
                    Globals->SetHour(day_time / 60 % 24);
                    ChangeGameTime();
                }
                break;
            case KeyCode::Tab:
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
            case KeyCode::F7:
                IntFix = !IntFix;
                break;
            case KeyCode::F9:
                ObjFix = !ObjFix;
                break;
            case KeyCode::F10:
                HexMngr.SwitchShowRain();
                break;
            case KeyCode::F11:
                SprMngr.DumpAtlases();
                break;
            case KeyCode::Escape:
                exit(0);
                break;
            case KeyCode::Add:
                if (!ConsoleEdit && SelectedEntities.empty())
                {
                    int day_time = HexMngr.GetDayTime();
                    day_time += 1;
                    Globals->SetMinute(day_time % 60);
                    Globals->SetHour(day_time / 60 % 24);
                    ChangeGameTime();
                }
                break;
            case KeyCode::Subtract:
                if (!ConsoleEdit && SelectedEntities.empty())
                {
                    int day_time = HexMngr.GetDayTime();
                    day_time -= 60;
                    Globals->SetMinute(day_time % 60);
                    Globals->SetHour(day_time / 60 % 24);
                    ChangeGameTime();
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

        if (Keyb.CtrlDwn)
        {
            switch (dikdw)
            {
            case KeyCode::X:
                BufferCut();
                break;
            case KeyCode::C:
                BufferCopy();
                break;
            case KeyCode::V:
                BufferPaste(50, 50);
                break;
            case KeyCode::A:
                SelectAll();
                break;
            case KeyCode::S:
                if (ActiveMap)
                {
                    HexMngr.GetProtoMap(*(ProtoMap*)ActiveMap->Proto);
                    // Todo: need attention!
                    // ((ProtoMap*)ActiveMap->Proto)->EditorSave(FileMngr, "");
                    AddMess("Map saved.");
                    RunMapSaveScript(ActiveMap);
                }
                break;
            case KeyCode::D:
                Settings.ScrollCheck = !Settings.ScrollCheck;
                break;
            case KeyCode::B:
                HexMngr.MarkPassedHexes();
                break;
            case KeyCode::Q:
                Settings.ShowCorners = !Settings.ShowCorners;
                break;
            case KeyCode::E:
                Settings.ShowDrawOrder = !Settings.ShowDrawOrder;
                break;
            case KeyCode::M:
                DrawCrExtInfo++;
                if (DrawCrExtInfo > DRAW_CR_INFO_MAX)
                    DrawCrExtInfo = 0;
                break;
            case KeyCode::L:
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
        }

        // Key up
        if (dikup)
        {
            ConsoleKeyUp(dikup);

            switch (dikup)
            {
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
        InputLostEvent.Fire();
        return;
    }

    // Mouse move
    if (Settings.LastMouseX != Settings.MouseX || Settings.LastMouseY != Settings.MouseY)
    {
        int ox = Settings.MouseX - Settings.LastMouseX;
        int oy = Settings.MouseY - Settings.LastMouseY;
        Settings.LastMouseX = Settings.MouseX;
        Settings.LastMouseY = Settings.MouseY;

        MouseMoveEvent.Fire(ox, oy);

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
            script_result = MouseDownEvent.Fire(event_dy > 0 ? MOUSE_BUTTON_WHEEL_UP : MOUSE_BUTTON_WHEEL_DOWN);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON_LEFT)
            script_result = MouseDownEvent.Fire(MOUSE_BUTTON_LEFT);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON_LEFT)
            script_result = MouseUpEvent.Fire(MOUSE_BUTTON_LEFT);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON_RIGHT)
            script_result = MouseDownEvent.Fire(MOUSE_BUTTON_RIGHT);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON_RIGHT)
            script_result = MouseUpEvent.Fire(MOUSE_BUTTON_RIGHT);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON_MIDDLE)
            script_result = MouseDownEvent.Fire(MOUSE_BUTTON_MIDDLE);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON_MIDDLE)
            script_result = MouseUpEvent.Fire(MOUSE_BUTTON_MIDDLE);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON(4))
            script_result = MouseDownEvent.Fire(MOUSE_BUTTON_EXT0);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON(4))
            script_result = MouseUpEvent.Fire(MOUSE_BUTTON_EXT0);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON(5))
            script_result = MouseDownEvent.Fire(MOUSE_BUTTON_EXT1);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON(5))
            script_result = MouseUpEvent.Fire(MOUSE_BUTTON_EXT1);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON(6))
            script_result = MouseDownEvent.Fire(MOUSE_BUTTON_EXT2);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON(6))
            script_result = MouseUpEvent.Fire(MOUSE_BUTTON_EXT2);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON(7))
            script_result = MouseDownEvent.Fire(MOUSE_BUTTON_EXT3);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON(7))
            script_result = MouseUpEvent.Fire(MOUSE_BUTTON_EXT3);
        if (event == SDL_MOUSEBUTTONDOWN && event_button == SDL_BUTTON(8))
            script_result = MouseDownEvent.Fire(MOUSE_BUTTON_EXT4);
        if (event == SDL_MOUSEBUTTONUP && event_button == SDL_BUTTON(8))
            script_result = MouseUpEvent.Fire(MOUSE_BUTTON_EXT4);
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

void FOMapper::MapperMainLoop()
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
    LoopEvent.Fire();

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
            for (auto [id, cr] : HexMngr.GetCritters()) {
                if (cr->SprDrawValid) {
                    if (DrawCrExtInfo == 1) {
                        cr->SetText(_str("|0xffaabbcc ProtoId...{}\n|0xffff1122 DialogId...{}\n", cr->GetName(), cr->GetDialogId()), COLOR_TEXT_WHITE, 60000000);
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
            auto& map_text = *it;

            if (tick >= map_text.StartTick + map_text.Tick) {
                it = GameMapTexts.erase(it);
            }
            else {
                const auto percent = GenericUtils::Percent(map_text.Tick, tick - map_text.StartTick);
                const auto text_pos = map_text.Pos.Interpolate(map_text.EndPos, percent);
                const auto& field = HexMngr.GetField(map_text.HexX, map_text.HexY);

                const auto x = static_cast<int>((field.ScrX + Settings.MapHexWidth / 2 + Settings.ScrOx) / Settings.SpritesZoom - 100.0f - static_cast<float>(map_text.Pos.Left - text_pos.Left));
                const auto y = static_cast<int>((field.ScrY + Settings.MapHexLineHeight / 2 - map_text.Pos.Height() - (map_text.Pos.Top - text_pos.Top) + Settings.ScrOy) / Settings.SpritesZoom - 70.0f);

                auto color = map_text.Color;
                if (map_text.Fade) {
                    color = (color ^ 0xFF000000) | ((0xFF * (100 - percent) / 100) << 24);
                }

                SprMngr.DrawStr(IRect(x, y, x + 200, y + 70), map_text.Text, FT_CENTERX | FT_BOTTOM | FT_BORDERED, color, FONT_DEFAULT);

                ++it;
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

    map<string, uint> PathIndex;

    for (uint t = 0, tt = static_cast<uint>(ttab.TileDirs.size()); t < tt; t++) {
        auto& path = ttab.TileDirs[t];
        bool include_subdirs = ttab.TileSubDirs[t];

        vector<string> tiles;
        auto tile_files = FileMngr.FilterFiles("", path, include_subdirs);
        while (tile_files.MoveNext()) {
            tiles.emplace_back(tile_files.GetCurFileHeader().GetPath());
        }

        std::sort(tiles.begin(), tiles.end(), [](string_view left, string_view right) {
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
                format_aviable = (ext == "fo3d" || ext == "fbx" || ext == "dae" || ext == "obj");
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
                const auto hash = ToHashedString(fname);
                Tabs[tab][DEFAULT_SUB_TAB].TileNames.push_back(hash);
                Tabs[tab][collection_name].TileNames.push_back(hash);
                Tabs[tab][collection_name_ex].TileNames.push_back(hash);
            }
        }
    }

    // Set default active tab
    TabsActive[tab] = &(*Tabs[tab].begin()).second;
}

auto FOMapper::GetProtoItemCurSprId(const ProtoItem* proto_item) -> uint
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

            if (proto_item->GetPicInv()) {
                auto* anim = ResMngr.GetInvAnim(proto_item->GetPicInv());
                if (anim != nullptr) {
                    SprMngr.DrawSpriteSize(anim->GetCurSprId(GameTime.GameTick()), x, y + h / 2, w, h / 2, false, true, col);
                }
            }

            SprMngr.DrawStr(IRect(x, y + h - 15, x + w, y + h), proto_item->GetName(), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
        }

        if (GetTabIndex() < static_cast<uint>((*CurItemProtos).size())) {
            auto* proto_item = (*CurItemProtos)[GetTabIndex()];
            auto it = std::find(proto_item->TextsLang.begin(), proto_item->TextsLang.end(), CurLang.NameCode);
            if (it != proto_item->TextsLang.end()) {
                auto index = static_cast<uint>(std::distance(proto_item->TextsLang.begin(), it));
                auto info = proto_item->Texts[0]->GetStr(ITEM_STR_ID(proto_item->GetProtoId().as_uint(), 1));
                info += " - ";
                info += proto_item->Texts[0]->GetStr(ITEM_STR_ID(proto_item->GetProtoId().as_uint(), 2));
                SprMngr.DrawStr(IRect(IntWHint, IntX, IntY), info, 0, FONT_DEFAULT, FONT_DEFAULT);
            }
        }
    }
    else if (IsTileMode()) {
        auto i = *CurProtoScroll;
        int j = i + ProtosOnScreen;
        if (j > static_cast<int>(CurTileNames->size())) {
            j = static_cast<int>(CurTileNames->size());
        }

        for (; i < j; i++, x += w) {
            auto* anim = ResMngr.GetItemAnim((*CurTileNames)[i]);
            if (anim == nullptr) {
                anim = ResMngr.ItemHexDefaultAnim;
            }

            auto col = (i == static_cast<int>(GetTabIndex()) ? COLOR_IFACE_RED : COLOR_IFACE);
            SprMngr.DrawSpriteSize(anim->GetCurSprId(GameTime.GameTick()), x, y, w, h / 2, false, true, col);

            auto& name = (*CurTileNames)[i];
            auto pos = _str(name).str().find_last_of('/');
            if (pos != string::npos) {
                SprMngr.DrawStr(IRect(x, y + h - 15, x + w, y + h), _str(name).str().substr(pos + 1), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
            }
            else {
                SprMngr.DrawStr(IRect(x, y + h - 15, x + w, y + h), name, FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
            }
        }

        if (GetTabIndex() < CurTileNames->size()) {
            SprMngr.DrawStr(IRect(IntWHint, IntX, IntY), (*CurTileNames)[GetTabIndex()], 0, FONT_DEFAULT, FONT_DEFAULT);
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

            auto model_name = proto->GetModelName();
            auto spr_id = ResMngr.GetCritterSprId(model_name, 1, 1, NpcDir, nullptr); // &proto->Params[ ST_ANIM3D_LAYER_BEGIN ] );
            if (spr_id == 0u) {
                continue;
            }

            auto col = COLOR_IFACE;
            if (i == GetTabIndex()) {
                col = COLOR_IFACE_RED;
            }

            SprMngr.DrawSpriteSize(spr_id, x, y, w, h / 2, false, true, col);
            SprMngr.DrawStr(IRect(x, y + h - 15, x + w, y + h), proto->GetName(), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
        }

        if (GetTabIndex() < CurNpcProtos->size()) {
            auto* proto = (*CurNpcProtos)[GetTabIndex()];
            SprMngr.DrawStr(IRect(IntWHint, IntX, IntY), proto->GetName(), 0, FONT_DEFAULT, FONT_DEFAULT);
        }
    }
    else if (IntMode == INT_MODE_INCONT && !SelectedEntities.empty()) {
        auto* entity = SelectedEntities[0];
        vector<Entity*> children; // Todo: need attention!
        // = entity->GetChildren();
        uint i = InContScroll;
        auto j = i + ProtosOnScreen;
        if (j > children.size()) {
            j = static_cast<uint>(children.size());
        }

        for (; i < j; i++, x += w) {
            auto* child = dynamic_cast<ItemView*>(children[i]);
            RUNTIME_ASSERT(child);

            auto* anim = ResMngr.GetInvAnim(child->GetPicInv());
            if (anim == nullptr) {
                continue;
            }

            auto col = COLOR_IFACE;
            if (child == InContItem) {
                col = COLOR_IFACE_RED;
            }

            SprMngr.DrawSpriteSize(anim->GetCurSprId(GameTime.GameTick()), x, y, w, h, false, true, col);

            SprMngr.DrawStr(IRect(x, y + h - 15, x + w, y + h), _str("x{}", child->GetCount()), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
            if (child->GetOwnership() == ItemOwnership::CritterInventory && (child->GetCritSlot() != 0u)) {
                SprMngr.DrawStr(IRect(x, y, x + w, y + h), _str("Slot {}", child->GetCritSlot()), FT_NOBREAK, COLOR_TEXT_WHITE, FONT_DEFAULT);
            }
        }
    }
    else if (IntMode == INT_MODE_LIST) {
        auto i = ListScroll;
        auto j = static_cast<int>(LoadedMaps.size());

        for (; i < j; i++, x += w) {
            auto* map = LoadedMaps[i];
            SprMngr.DrawStr(IRect(x, y, x + w, y + h), _str(" '{}'", map->GetName()), 0, map == ActiveMap ? COLOR_IFACE_RED : COLOR_TEXT, FONT_DEFAULT);
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
        for (auto [fst, snd] : stabs) {
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
        SprMngr.DrawStr(IRect(Settings.ScreenWidth - 100, 0, Settings.ScreenWidth, Settings.ScreenHeight),
            _str("Map '{}'\n"
                 "Hex {} {}\n"
                 "Time {} : {}\n"
                 "Fps {}\n"
                 "Tile layer {}\n"
                 "{}",
                ActiveMap->GetName(), hex_thru ? hx : -1, hex_thru ? hy : -1, day_time / 60 % 24, day_time % 60, Settings.FPS, TileLayer, Settings.ScrollCheck ? "Scroll check" : ""),
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

    auto* item = dynamic_cast<ItemView*>(entity);
    auto* cr = dynamic_cast<CritterView*>(entity);
    auto r = IRect(ObjWWork, ObjX, ObjY);
    const auto x = r.Left;
    const auto y = r.Top;
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

        if (item->GetPicInv()) {
            auto* anim = ResMngr.GetInvAnim(item->GetPicInv());
            if (anim != nullptr) {
                SprMngr.DrawSpriteSize(anim->GetCurSprId(GameTime.GameTick()), x + w - ProtoWidth, y + ProtoWidth, ProtoWidth, ProtoWidth, false, true, 0);
            }
        }
    }

    DrawLine("Id", "", _str("{} ({})", entity->GetId(), static_cast<int>(entity->GetId())), true, r);
    DrawLine("ProtoName", "", entity->GetName(), true, r);
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
    SprMngr.DrawStr(IRect(IRect(x, y, x + w / 2, y + h), 0, 0), str, FT_NOBREAK, color, 0);
    SprMngr.DrawStr(IRect(IRect(x + w / 2, y, x + w, y + h), 0, 0), result_text, FT_NOBREAK, color, 0);

    r.Top += DRAW_NEXT_HEIGHT;
    r.Bottom += DRAW_NEXT_HEIGHT;
}

void FOMapper::ObjKeyDown(KeyCode dik, string_view dik_text)
{
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
            HexMngr.RebuildLight();
        }
    }
    else if (dik == KeyCode::Up) {
        SelectEntityProp(ObjCurLine - 1);
    }
    else if (dik == KeyCode::Down) {
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
    const auto start_line = 3;
    if (ObjCurLine >= start_line && ObjCurLine - start_line < static_cast<int>(ShowProps.size())) {
        const auto* prop = ShowProps[ObjCurLine - start_line];
        if (prop != nullptr) {
            if (entity->GetPropertiesForEdit().LoadPropertyFromText(prop, ObjCurLineValue)) {
                if (auto* hex_item = dynamic_cast<ItemHexView*>(entity); hex_item != nullptr) {
                    if (prop == hex_item->GetPropertyOffsetX() || prop == hex_item->GetPropertyOffsetY()) {
                        hex_item->SetAnimOffs();
                    }
                    if (prop == hex_item->GetPropertyPicMap()) {
                        hex_item->RefreshAnim();
                    }
                }
            }
            else {
                const auto r = entity->GetPropertiesForEdit().LoadPropertyFromText(prop, ObjCurLineInitValue);
                UNUSED_VARIABLE(r);
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

    if (auto* entity = GetInspectorEntity(); entity != nullptr) {
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
    auto* entity = (IntMode == INT_MODE_INCONT && (InContItem != nullptr) ? InContItem : (!SelectedEntities.empty() ? SelectedEntities[0] : nullptr));
    if (entity == InspectorEntity) {
        return entity;
    }

    InspectorEntity = entity;
    ShowProps.clear();

    if (entity != nullptr) {
        vector<int> prop_indices;
        InspectorPropertiesEvent.Fire(entity, prop_indices);
        for (const auto prop_index : prop_indices) {
            ShowProps.push_back(prop_index != -1 ? entity->GetProperties().GetRegistrator()->GetByIndex(prop_index) : nullptr);
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
            for (auto [fst, snd] : stabs) {
                i++;
                if (i - 1 < TabsScroll[SubTabsActiveTab]) {
                    continue;
                }

                const auto& name = fst;
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

        if (!HexMngr.GetHexPixel(Settings.MouseX, Settings.MouseY, SelectHX1, SelectHY1)) {
            return;
        }
        SelectHX2 = SelectHX1;
        SelectHY2 = SelectHY1;
        SelectX = Settings.MouseX;
        SelectY = Settings.MouseY;

        if (CurMode == CUR_MODE_DEFAULT) {
            if (Keyb.ShiftDwn) {
                for (auto* entity : SelectedEntities) {
                    if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
                        const auto is_run = (!cr->MoveSteps.empty() && cr->MoveSteps[cr->MoveSteps.size() - 1].first == SelectHX1 && cr->MoveSteps[cr->MoveSteps.size() - 1].second == SelectHY1);

                        cr->MoveSteps.clear();
                        if (!is_run && cr->GetIsNoWalk()) {
                            break;
                        }

                        auto hx = cr->GetHexX();
                        auto hy = cr->GetHexY();
                        vector<uchar> steps;
                        if (HexMngr.FindPath(nullptr, hx, hy, SelectHX1, SelectHY1, steps, -1)) {
                            for (const auto step : steps) {
                                if (GeomHelper.MoveHexByDir(hx, hy, step, HexMngr.GetWidth(), HexMngr.GetHeight())) {
                                    cr->MoveSteps.push_back(pair<ushort, ushort>(hx, hy));
                                }
                                else {
                                    break;
                                }
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
                AddItem((*CurItemProtos)[GetTabIndex()]->GetProtoId(), SelectHX1, SelectHY1, nullptr);
            }
            else if (IsTileMode() && !CurTileNames->empty()) {
                AddTile((*CurTileNames)[GetTabIndex()], SelectHX1, SelectHY1, 0, 0, TileLayer, DrawRoof);
            }
            else if (IsCritMode() && !CurNpcProtos->empty()) {
                AddCritter((*CurNpcProtos)[GetTabIndex()]->GetProtoId(), SelectHX1, SelectHY1);
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

                HexMngr.SwitchIgnorePid(pid);
                HexMngr.RefreshMap();
            }
            // Add to container
            else if (Keyb.AltDwn && !SelectedEntities.empty()) {
                const auto add = true;
                auto* proto_item = (*CurItemProtos)[ind];

                if (proto_item->GetStackable()) {
                    // Todo: need attention!
                    /*for (auto* child : SelectedEntities[0]->GetChildren())
                    {
                        if (proto_item->ProtoId == child->GetProtoId())
                        {
                            add = false;
                            break;
                        }
                    }*/
                }

                if (add) {
                    AddItem(proto_item->GetProtoId(), 0, 0, SelectedEntities[0]);
                }
            }
        }
        else if (IsTileMode() && !CurTileNames->empty()) {
            ind += *CurProtoScroll;
            if (ind >= static_cast<int>(CurTileNames->size())) {
                ind = static_cast<int>(CurTileNames->size()) - 1;
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
            vector<Entity*> children;
            // Todo: need attention!
            // if (!SelectedEntities.empty())
            //    children = SelectedEntities[0]->GetChildren();

            if (!children.empty()) {
                if (ind < static_cast<int>(children.size())) {
                    InContItem = dynamic_cast<ItemView*>(children[ind]);
                }

                // Delete child
                if (Keyb.AltDwn && InContItem != nullptr) {
                    if (InContItem->GetOwnership() == ItemOwnership::CritterInventory) {
                        auto* owner = HexMngr.GetCritter(InContItem->GetCritId());
                        RUNTIME_ASSERT(owner);
                        owner->DeleteItem(InContItem, true);
                    }
                    else if (InContItem->GetOwnership() == ItemOwnership::ItemContainer) {
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
                else if (Keyb.ShiftDwn && InContItem != nullptr) {
                    if (auto* cr = dynamic_cast<CritterView*>(SelectedEntities[0]); cr != nullptr) {
                        auto to_slot = InContItem->GetCritSlot() + 1;
                        // while (to_slot >= Settings.CritterSlotEnabled.size() || !Settings.CritterSlotEnabled[to_slot % 256]) {
                        //    to_slot++;
                        //}
                        to_slot %= 256;

                        // Todo: need attention!
                        // for (auto* child : cr->GetChildren())
                        //    if (((ItemView*)child)->GetCritSlot() == to_slot)
                        //        ((ItemView*)child)->SetCritSlot(0);

                        InContItem->SetCritSlot(to_slot);

                        cr->AnimateStay();
                    }
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
        else if (IsTileMode() && !CurTileNames->empty()) {
            (*CurProtoScroll)++;
            if (*CurProtoScroll >= static_cast<int>(CurTileNames->size())) {
                *CurProtoScroll = static_cast<int>(CurTileNames->size()) - 1;
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
        else if (IsTileMode() && !CurTileNames->empty()) {
            (*CurProtoScroll) += ProtosOnScreen;
            if (*CurProtoScroll >= static_cast<int>(CurTileNames->size())) {
                *CurProtoScroll = static_cast<int>(CurTileNames->size()) - 1;
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
                vector<pair<ushort, ushort>> hexes;

                if (SelectType == SELECT_TYPE_OLD) {
                    const int fx = std::min(SelectHX1, SelectHX2);
                    const int tx = std::max(SelectHX1, SelectHX2);
                    const int fy = std::min(SelectHY1, SelectHY2);
                    const int ty = std::max(SelectHY1, SelectHY2);

                    for (auto i = fx; i <= tx; i++) {
                        for (auto j = fy; j <= ty; j++) {
                            hexes.push_back(std::make_pair(i, j));
                        }
                    }
                }
                else {
                    hexes = HexMngr.GetHexesRect(IRect(SelectHX1, SelectHY1, SelectHX2, SelectHY2));
                }

                vector<ItemHexView*> items;
                vector<CritterView*> critters;
                for (auto [hx, hy] : hexes) {
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

                for (auto* item : items) {
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

                for (auto* cr : critters) {
                    if (IsSelectCrit && Settings.ShowCrit) {
                        SelectAddCrit(cr);
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
                            HexMngr.GetHexTrack(static_cast<ushort>(i), static_cast<ushort>(j)) = 1;
                        }
                    }
                }
                else if (SelectType == SELECT_TYPE_NEW) {
                    for (auto [hx, hy] : HexMngr.GetHexesRect(IRect(SelectHX1, SelectHY1, SelectHX2, SelectHY2))) {
                        HexMngr.GetHexTrack(hx, hy) = 1;
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
    CurTileNames = nullptr;
    CurNpcProtos = nullptr;
    InContItem = nullptr;

    if (IntMode >= 0 && IntMode < TAB_COUNT) {
        auto* stab = TabsActive[IntMode];
        if (!stab->TileNames.empty()) {
            CurTileNames = &stab->TileNames;
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
    for (const auto* fast_proto : TabsActive[INT_MODE_FAST]->ItemProtos) {
        HexMngr.AddFastPid(fast_proto->GetProtoId());
    }

    // Update ignore pids
    HexMngr.ClearIgnorePids();
    for (const auto* ignore_proto : TabsActive[INT_MODE_IGNORE]->ItemProtos) {
        HexMngr.AddIgnorePid(ignore_proto->GetProtoId());
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

void FOMapper::MoveEntity(ClientEntity* entity, ushort hx, ushort hy)
{
    if (hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight()) {
        return;
    }

    if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
        if (cr->IsDead() || (HexMngr.GetField(hx, hy).Crit == nullptr)) {
            HexMngr.RemoveCritter(cr);
            cr->SetHexX(hx);
            cr->SetHexY(hy);
            HexMngr.SetCritter(cr);
        }
    }
    else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        HexMngr.DeleteItem(item, false, nullptr);
        item->SetHexX(hx);
        item->SetHexY(hy);
        HexMngr.PushItem(item);
    }
}

void FOMapper::DeleteEntity(ClientEntity* entity)
{
    const auto it = std::find(SelectedEntities.begin(), SelectedEntities.end(), entity);
    if (it != SelectedEntities.end()) {
        SelectedEntities.erase(it);
    }

    if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
        HexMngr.DeleteCritter(cr->GetId());
    }
    else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        HexMngr.FinishItem(item->GetId(), false);
    }
}

void FOMapper::SelectClear()
{
    // Clear map objects
    for (auto* entity : SelectedEntities) {
        if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
            item->RestoreAlpha();
        }
        else if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
            cr->Alpha = 0xFF;
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
    SelectedTile.push_back({hx, hy, is_roof});

    // Select
    auto& tiles = HexMngr.GetTiles(hx, hy, is_roof);
    for (auto& tile : tiles) {
        tile.IsSelected = true;
    }
}

void FOMapper::SelectAdd(ClientEntity* entity)
{
    const auto it = std::find(SelectedEntities.begin(), SelectedEntities.end(), entity);
    if (it == SelectedEntities.end()) {
        SelectedEntities.push_back(entity);

        if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
            cr->Alpha = HexMngr.SelectAlpha;
        }
        else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
            item->Alpha = HexMngr.SelectAlpha;
        }
    }
}

void FOMapper::SelectErase(ClientEntity* entity)
{
    const auto it = std::find(SelectedEntities.begin(), SelectedEntities.end(), entity);
    if (it != SelectedEntities.end()) {
        SelectedEntities.erase(it);

        if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
            cr->Alpha = 0xFF;
        }
        else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
            item->RestoreAlpha();
        }
    }
}

void FOMapper::SelectAll()
{
    SelectClear();

    for (const auto hx : xrange(HexMngr.GetWidth())) {
        for (const auto hy : xrange(HexMngr.GetHeight())) {
            if (IsSelectTile && Settings.ShowTile) {
                SelectAddTile(hx, hy, false);
            }
            if (IsSelectRoof && Settings.ShowRoof) {
                SelectAddTile(hx, hy, true);
            }
        }
    }

    for (auto* item : HexMngr.GetItems()) {
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
        for (auto [id, cr] : HexMngr.GetCritters()) {
            SelectAddCrit(cr);
        }
    }

    HexMngr.RefreshMap();
}

struct TileToMove
{
    Field* MoveField {};
    Field::Tile MoveTile {};
    vector<MapTile>* MapTilesToMove {};
    MapTile MapTimeToMove {};
    bool IsRoofMoved {};
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
        if (std::abs(offs_hx) < Settings.MapTileStep && std::abs(offs_hy) < Settings.MapTileStep) {
            return false;
        }
        offs_hx -= offs_hx % Settings.MapTileStep;
        offs_hy -= offs_hy % Settings.MapTileStep;
    }

    // Setup hex moving switcher
    auto switcher = 0;
    if (!SelectedEntities.empty()) {
        if (auto* cr = dynamic_cast<CritterView*>(SelectedEntities[0]); cr != nullptr) {
            switcher = cr->GetHexX() % 2;
        }
        else if (auto* item = dynamic_cast<ItemHexView*>(SelectedEntities[0]); item != nullptr) {
            switcher = item->GetHexX() % 2;
        }
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
    // Check borders
    else {
        // Objects
        for (auto* entity : SelectedEntities) {
            int hx;
            int hy;
            if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
                hx = cr->GetHexX();
                hy = cr->GetHexY();
            }
            else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
                hx = item->GetHexX();
                hy = item->GetHexY();
            }

            if (Settings.MapHexagonal) {
                auto sw = switcher;
                for (auto k = 0, l = std::abs(offs_hx); k < l; k++, sw++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hx > 0 ? ((sw % 2) != 0 ? 4u : 3u) : ((sw % 2) != 0 ? 0u : 1u));
                }
                for (auto k = 0, l = std::abs(offs_hy); k < l; k++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hy > 0 ? 2u : 5u);
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
                for (auto k = 0, l = std::abs(offs_hx); k < l; k++, sw++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hx > 0 ? ((sw & 1) != 0 ? 4 : 3) : ((sw & 1) != 0 ? 0 : 1));
                }
                for (auto k = 0, l = std::abs(offs_hy); k < l; k++) {
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

            item->SetOffsetX(static_cast<short>(ox));
            item->SetOffsetY(static_cast<short>(oy));
            item->RefreshAnim();
        }
        else {
            int hx;
            int hy;
            if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
                hx = cr->GetHexX();
                hy = cr->GetHexY();
            }
            else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
                hx = item->GetHexX();
                hy = item->GetHexY();
            }

            if (Settings.MapHexagonal) {
                auto sw = switcher;
                for (auto k = 0, l = std::abs(offs_hx); k < l; k++, sw++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hx > 0 ? ((sw & 1) != 0 ? 4 : 3) : ((sw & 1) != 0 ? 0 : 1));
                }
                for (auto k = 0, l = std::abs(offs_hy); k < l; k++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hy > 0 ? 2 : 5);
                }
            }
            else {
                hx += offs_hx;
                hy += offs_hy;
            }

            hx = std::clamp(hx, 0, HexMngr.GetWidth() - 1);
            hy = std::clamp(hy, 0, HexMngr.GetHeight() - 1);

            if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
                HexMngr.DeleteItem(item, false, nullptr);
                item->SetHexX(static_cast<ushort>(hx));
                item->SetHexY(static_cast<ushort>(hy));
                HexMngr.PushItem(item);
            }
            else if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
                HexMngr.RemoveCritter(cr);
                cr->SetHexX(static_cast<ushort>(hx));
                cr->SetHexY(static_cast<ushort>(hy));
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
                for (auto k = 0, l = std::abs(offs_hx); k < l; k++, sw++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hx > 0 ? ((sw % 2) != 0 ? 4u : 3u) : ((sw % 2) != 0 ? 0u : 1u));
                }
                for (auto k = 0, l = std::abs(offs_hy); k < l; k++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, offs_hy > 0 ? 2u : 5u);
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
                    tiles_to_move.push_back({&HexMngr.GetField(hx, hy), ftile, &HexMngr.GetTiles(hx, hy, stile.IsRoof), tiles[k], stile.IsRoof});
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
        ttm.MoveField->AddTile(ttm.MoveTile.Anim, ttm.MoveTile.OffsX, ttm.MoveTile.OffsY, ttm.MoveTile.Layer, ttm.IsRoofMoved);
        ttm.MapTilesToMove->push_back(ttm.MapTimeToMove);
    }
    return true;
}

void FOMapper::SelectDelete()
{
    auto entities = SelectedEntities;
    for (auto* entity : entities) {
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

auto FOMapper::AddCritter(hstring pid, ushort hx, ushort hy) -> CritterView*
{
    RUNTIME_ASSERT(ActiveMap);

    const auto* proto = ProtoMngr->GetProtoCritter(pid);
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
    cr->Init();

    HexMngr.AddCritter(cr);
    SelectAdd(cr);

    HexMngr.RefreshMap();
    CurMode = CUR_MODE_DEFAULT;

    return cr;
}

auto FOMapper::AddItem(hstring pid, ushort hx, ushort hy, Entity* owner) -> ItemView*
{
    RUNTIME_ASSERT(ActiveMap);

    // Checks
    const auto* proto_item = ProtoMngr->GetProtoItem(pid);
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

void FOMapper::AddTile(hstring name, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof)
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

    int hx;
    int hy;
    if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
        hx = cr->GetHexX();
        hy = cr->GetHexY();
    }
    else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        hx = item->GetHexX();
        hy = item->GetHexY();
    }

    if (hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight()) {
        return nullptr;
    }

    Entity* owner = nullptr;
    if (dynamic_cast<CritterView*>(entity) != nullptr) {
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
        cr->SetProperties(entity->GetProperties());
        cr->SetHexX(hx);
        cr->SetHexY(hy);
        cr->Init();
        HexMngr.AddCritter(cr);
        SelectAdd(cr);
        owner = cr;
    }
    else if (dynamic_cast<ItemHexView*>(entity) != nullptr) {
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

    auto* pmap = static_cast<const ProtoMap*>(ActiveMap->GetProto());
    std::function<void(Entity*, Entity*)> add_entity_children = [&add_entity_children, &pmap](Entity* from, Entity* to) {
        // Todo: need attention!
        /*for (auto* from_child : from->GetChildren())
        {
            RUNTIME_ASSERT(from_child->Type == EntityType::Item);
            ItemView* to_child = new ItemView(--pmap->LastEntityId, (ProtoItem*)from_child->Proto);
            to_child->_props = from_child->_props;
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
        for (auto* child : entity_buf->Children) {
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
        ushort hx;
        ushort hy;
        if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
            hx = cr->GetHexX();
            hy = cr->GetHexY();
        }
        else if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
            hx = item->GetHexX();
            hy = item->GetHexY();
        }

        entity_buf->HexX = hx;
        entity_buf->HexY = hy;
        entity_buf->IsCritter = (dynamic_cast<CritterView*>(entity) != nullptr);
        entity_buf->IsItem = (dynamic_cast<ItemHexView*>(entity) != nullptr);
        entity_buf->Proto = (dynamic_cast<EntityWithProto*>(entity) != nullptr ? dynamic_cast<EntityWithProto*>(entity)->GetProto() : nullptr);
        entity_buf->Props = new Properties(entity->GetProperties());
        // Todo: need attention!
        /*for (auto* child : entity->GetChildren())
        {
            EntityBuf* child_buf = new EntityBuf();
            add_entity(child_buf, child);
            entity_buf->Children.push_back(child_buf);
        }*/
    };
    for (auto* entity : SelectedEntities) {
        EntitiesBuffer.push_back(EntityBuf());
        add_entity(&EntitiesBuffer.back(), entity);
    }

    // Add tiles to buffer
    for (const auto& tile : SelectedTile) {
        auto& hex_tiles = HexMngr.GetTiles(tile.HexX, tile.HexY, tile.IsRoof);
        for (const auto& hex_tile : hex_tiles) {
            if (hex_tile.IsSelected) {
                TileBuf tb;
                tb.Name = ResolveHash(hex_tile.NameHash);
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
    for (const auto& entity_buf : EntitiesBuffer) {
        if (entity_buf.HexX >= HexMngr.GetWidth() || entity_buf.HexY >= HexMngr.GetHeight()) {
            continue;
        }

        auto hx = entity_buf.HexX;
        auto hy = entity_buf.HexY;

        const Entity* owner;
        if (entity_buf.IsCritter) {
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
            cr->SetProperties(*entity_buf.Props);
            cr->SetHexX(hx);
            cr->SetHexY(hy);
            cr->Init();
            HexMngr.AddCritter(cr);
            SelectAdd(cr);
            owner = cr;
        }
        else if (entity_buf.IsItem) {
            const uint id = 0; // Todo: need attention!
            // HexMngr.AddItem(
            //  --((ProtoMap*)ActiveMap->Proto)->LastEntityId, entity_buf.Proto->ProtoId, hx, hy, false, nullptr);
            ItemHexView* item = HexMngr.GetItemById(id);
            item->SetProperties(*entity_buf.Props);
            SelectAdd(item);
            owner = item;
        }

        // Todo: need attention!
        /*auto* pmap = dynamic_cast<ProtoMap*>(ActiveMap->Proto);
        std::function<void(EntityBuf*, Entity*)> add_entity_children = [&add_entity_children, &pmap](EntityBuf* entity_buf) {
            for (auto& child_buf : entity_buf->Children) {
                RUNTIME_ASSERT(child_buf->Type == EntityType::Item);
                ItemView* child = new ItemView(--pmap->LastEntityId, (ProtoItem*)child_buf->Proto);
                child->_props = *child_buf->_props;
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
                SelectedTile.push_back({tile_buf.HexX, tile_buf.HexY, tile_buf.IsRoof});
            }
        }
    }
}

void FOMapper::CurDraw()
{
    switch (CurMode) {
    case CUR_MODE_DEFAULT:
    case CUR_MODE_MOVE_SELECTION: {
        auto* anim = (CurMode == CUR_MODE_DEFAULT ? CurPDef : CurPHand);
        if (anim != nullptr) {
            const auto* si = SprMngr.GetSpriteInfo(anim->GetCurSprId(GameTime.GameTick()));
            if (si != nullptr) {
                SprMngr.DrawSprite(anim, Settings.MouseX, Settings.MouseY, COLOR_IFACE);
            }
        }
    } break;
    case CUR_MODE_PLACE_OBJECT:
        if (IsObjectMode() && !(*CurItemProtos).empty()) {
            const auto* proto_item = (*CurItemProtos)[GetTabIndex()];

            ushort hx = 0;
            ushort hy = 0;
            if (!HexMngr.GetHexPixel(Settings.MouseX, Settings.MouseY, hx, hy)) {
                break;
            }

            const auto spr_id = GetProtoItemCurSprId(proto_item);
            const auto* si = SprMngr.GetSpriteInfo(spr_id);
            if (si != nullptr) {
                const auto x = HexMngr.GetField(hx, hy).ScrX - (si->Width / 2) + si->OffsX + (Settings.MapHexWidth / 2) + Settings.ScrOx + proto_item->GetOffsetX();
                const auto y = HexMngr.GetField(hx, hy).ScrY - si->Height + si->OffsY + (Settings.MapHexHeight / 2) + Settings.ScrOy + proto_item->GetOffsetY();
                SprMngr.DrawSpriteSize(spr_id, static_cast<int>(x / Settings.SpritesZoom), static_cast<int>(y / Settings.SpritesZoom), static_cast<int>(si->Width / Settings.SpritesZoom), static_cast<int>(si->Height / Settings.SpritesZoom), true, false, 0);
            }
        }
        else if (IsTileMode() && !CurTileNames->empty()) {
            auto* anim = ResMngr.GetItemAnim((*CurTileNames)[GetTabIndex()]);
            if (anim == nullptr) {
                anim = ResMngr.ItemHexDefaultAnim;
            }

            ushort hx = 0;
            ushort hy = 0;
            if (!HexMngr.GetHexPixel(Settings.MouseX, Settings.MouseY, hx, hy)) {
                break;
            }

            const auto* si = SprMngr.GetSpriteInfo(anim->GetCurSprId(GameTime.GameTick()));
            if (si != nullptr) {
                hx -= hx % Settings.MapTileStep;
                hy -= hy % Settings.MapTileStep;
                auto x = HexMngr.GetField(hx, hy).ScrX - (si->Width / 2) + si->OffsX;
                auto y = HexMngr.GetField(hx, hy).ScrY - si->Height + si->OffsY;
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
            const auto model_name = (*CurNpcProtos)[GetTabIndex()]->GetModelName();
            auto spr_id = ResMngr.GetCritterSprId(model_name, 1, 1, NpcDir, nullptr);
            if (spr_id == 0u) {
                spr_id = ResMngr.ItemHexDefaultAnim->GetSprId(0);
            }

            ushort hx = 0;
            ushort hy = 0;
            if (!HexMngr.GetHexPixel(Settings.MouseX, Settings.MouseY, hx, hy)) {
                break;
            }

            const auto* si = SprMngr.GetSpriteInfo(spr_id);
            if (si != nullptr) {
                const auto x = HexMngr.GetField(hx, hy).ScrX - (si->Width / 2) + si->OffsX;
                const auto y = HexMngr.GetField(hx, hy).ScrY - si->Height + si->OffsY;

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
        for (auto* entity : SelectedEntities) {
            if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
                auto dir = cr->GetDir() + 1;
                if (dir >= Settings.MapDirCount) {
                    dir = 0;
                }
                cr->ChangeDir(dir, true);
            }
        }
    }
}

auto FOMapper::IsCurInRect(const IRect& rect, int ax, int ay) const -> bool
{
    return Settings.MouseX >= rect[0] + ax && Settings.MouseY >= rect[1] + ay && Settings.MouseX <= rect[2] + ax && Settings.MouseY <= rect[3] + ay;
}

auto FOMapper::IsCurInRect(const IRect& rect) const -> bool
{
    return Settings.MouseX >= rect[0] && Settings.MouseY >= rect[1] && Settings.MouseX <= rect[2] && Settings.MouseY <= rect[3];
}

auto FOMapper::IsCurInRectNoTransp(uint spr_id, const IRect& rect, int ax, int ay) const -> bool
{
    return IsCurInRect(rect, ax, ay) && SprMngr.IsPixNoTransp(spr_id, Settings.MouseX - rect.Left - ax, Settings.MouseY - rect.Top - ay, false);
}

auto FOMapper::IsCurInInterface() const -> bool
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

        auto str = ConsoleStr;
        str.insert(ConsoleCur, GameTime.FrameTick() % 800 < 400 ? "!" : ".");
        SprMngr.DrawStr(IRect(IntX + ConsoleTextX, (IntVisible ? IntY : Settings.ScreenHeight) + ConsoleTextY, Settings.ScreenWidth, Settings.ScreenHeight), str, FT_NOBREAK, 0, FONT_DEFAULT);
    }
}

void FOMapper::ConsoleKeyDown(KeyCode dik, string_view dik_text)
{
    if (dik == KeyCode::Return || dik == KeyCode::Numpadenter) {
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
                for (const auto& str : ConsoleHistory) {
                    history_str += str + "\n";
                }
                Cache.SetString("mapper_console.txt", history_str);

                // Process command
                const auto process_command = ConsoleMessageEvent.Fire(ConsoleStr);
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
        ConsoleKeyTick = GameTime.FrameTick();
        ConsoleAccelerate = 1;
        return;
    }
}

void FOMapper::ConsoleKeyUp(KeyCode /*key*/)
{
    ConsoleLastKey = KeyCode::None;
    ConsoleLastKeyText = "";
}

void FOMapper::ConsoleProcess()
{
    if (ConsoleLastKey == KeyCode::None) {
        return;
    }

    if (static_cast<int>(GameTime.FrameTick() - ConsoleKeyTick) >= CONSOLE_KEY_TICK - ConsoleAccelerate) {
        ConsoleKeyTick = GameTime.FrameTick();
        ConsoleAccelerate = CONSOLE_MAX_ACCELERATE;
        Keyb.FillChar(ConsoleLastKey, ConsoleLastKeyText, ConsoleStr, &ConsoleCur, KIF_NO_SPEC_SYMBOLS);
    }
}

void FOMapper::ParseCommand(string_view command)
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

        auto* pmap = new ProtoMap(ToHashedString(map_name), GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME));
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

        auto* map = new MapView(this, 0, pmap);
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
        const auto command_str = string(command.substr(1));
        istringstream icmd(command_str);
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
                AddMess(_str("Result: {}", result));
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

        vector<int> anims = _str(command.substr(1)).splitToInt(' ');
        if (anims.empty()) {
            return;
        }

        if (!SelectedEntities.empty()) {
            for (auto* entity : SelectedEntities) {
                if (auto* cr = dynamic_cast<CritterView*>(entity); cr != nullptr) {
                    cr->ClearAnim();
                    for (uint j = 0; j < anims.size() / 2; j++) {
                        cr->Animate(anims[j * 2], anims[j * 2 + 1], nullptr);
                    }
                }
            }
        }
        else {
            for (auto [id, cr] : HexMngr.GetCritters()) {
                cr->ClearAnim();
                for (uint j = 0; j < anims.size() / 2; j++) {
                    cr->Animate(anims[j * 2], anims[j * 2 + 1], nullptr);
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
            auto* pmap = new ProtoMap(ToHashedString("new"), GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME));

            pmap->SetWidth(MAXHEX_DEFAULT);
            pmap->SetHeight(MAXHEX_DEFAULT);

            // Morning	 5.00 -  9.59	 300 - 599
            // Day		10.00 - 18.59	 600 - 1139
            // Evening	19.00 - 22.59	1140 - 1379
            // Nigh		23.00 -  4.59	1380
            vector<int> arr = {300, 600, 1140, 1380};
            vector<uchar> arr2 = {18, 128, 103, 51, 18, 128, 95, 40, 53, 128, 86, 29};
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

            auto* map = new MapView(this, 0, pmap);
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
            ActiveMap->GetProto()->Release();
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

void FOMapper::AddMess(string_view message_text)
{
    const string str = _str("|{} - {}\n", COLOR_TEXT, message_text);

    const auto dt = Timer::GetCurrentDateTime();
    const string mess_time = _str("{:02}:{:02}:{:02} ", dt.Hour, dt.Minute, dt.Second);

    MessBox.push_back({0, str, mess_time});
    MessBoxScroll = 0;
    MessBoxGenerate();
}

void FOMapper::MessBoxGenerate()
{
    MessBoxCurText = "";
    if (MessBox.empty()) {
        return;
    }

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
    if (!IntVisible) {
        return;
    }
    if (MessBoxCurText.empty()) {
        return;
    }

    const uint flags = FT_UPPER | FT_BOTTOM;
    SprMngr.DrawStr(IRect(IntWWork[0] + IntX, IntWWork[1] + IntY, IntWWork[2] + IntX, IntWWork[3] + IntY), MessBoxCurText, flags, 0, FONT_DEFAULT);
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
    StartEvent.Fire();
}

void FOMapper::RunMapLoadScript(MapView* map)
{
    RUNTIME_ASSERT(map);
    EditMapLoadEvent.Fire(map);
}

void FOMapper::RunMapSaveScript(MapView* map)
{
    RUNTIME_ASSERT(map);
    EditMapSaveEvent.Fire(map);
}

void FOMapper::DrawIfaceLayer(uint /*layer*/)
{
    SpritesCanDraw = true;
    RenderIfaceEvent.Fire(); // Todo: mapper render iface layer
    SpritesCanDraw = false;
}

void FOMapper::OnSetItemFlags(Entity* entity, const Property* prop, void* /*cur_value*/, void* /*old_value*/)
{
    // IsColorize, IsBadItem, IsShootThru, IsLightThru, IsNoBlock

    auto* item = dynamic_cast<ItemView*>(entity);

    if (item->GetOwnership() == ItemOwnership::MapHex && HexMngr.IsMapLoaded()) {
        auto* hex_item = dynamic_cast<ItemHexView*>(item);
        bool rebuild_cache = false;
        if (prop == hex_item->GetPropertyIsColorize()) {
            hex_item->RefreshAlpha();
        }
        else if (prop == hex_item->GetPropertyIsBadItem()) {
            hex_item->SetSprite(nullptr);
        }
        else if (prop == hex_item->GetPropertyIsShootThru()) {
            rebuild_cache = true;
        }
        else if (prop == hex_item->GetPropertyIsLightThru()) {
            HexMngr.RebuildLight();
            rebuild_cache = true;
        }
        else if (prop == hex_item->GetPropertyIsNoBlock()) {
            rebuild_cache = true;
        }
        if (rebuild_cache) {
            HexMngr.GetField(hex_item->GetHexX(), hex_item->GetHexY()).ProcessCache();
        }
    }
}

void FOMapper::OnSetItemSomeLight(Entity* /*entity*/, const Property* /*prop*/, void* /*cur_value*/, void* /*old_value*/)
{
    // IsLight, LightIntensity, LightDistance, LightFlags, LightColor

    if (HexMngr.IsMapLoaded()) {
        HexMngr.RebuildLight();
    }
}

void FOMapper::OnSetItemPicMap(Entity* entity, const Property* /*prop*/, void* /*cur_value*/, void* /*old_value*/)
{
    auto* item = dynamic_cast<ItemView*>(entity);

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto* hex_item = dynamic_cast<ItemHexView*>(item);
        hex_item->RefreshAnim();
    }
}

void FOMapper::OnSetItemOffsetXY(Entity* entity, const Property* /*prop*/, void* /*cur_value*/, void* /*old_value*/)
{
    // OffsetX, OffsetY

    auto* item = dynamic_cast<ItemView*>(entity);

    if (item->GetOwnership() == ItemOwnership::MapHex && HexMngr.IsMapLoaded()) {
        auto* hex_item = dynamic_cast<ItemHexView*>(item);
        hex_item->SetAnimOffs();
        HexMngr.ProcessHexBorders(hex_item);
    }
}

void FOMapper::OnSetItemOpened(Entity* entity, const Property* /*prop*/, void* cur_value, void* old_value)
{
    auto* item = dynamic_cast<ItemView*>(entity);
    const bool cur = *static_cast<bool*>(cur_value);
    const bool old = *static_cast<bool*>(old_value);

    if (item->GetIsCanOpen()) {
        auto* hex_item = dynamic_cast<ItemHexView*>(item);
        if (!old && cur) {
            hex_item->SetAnimFromStart();
        }
        if (old && !cur) {
            hex_item->SetAnimFromEnd();
        }
    }
}
