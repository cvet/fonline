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

#include "Common.h"

#include "Client.h"
#include "ClientScripting.h"
#include "FileSystem.h"
#include "GeometryHelper.h"
#include "Log.h"
#include "NetCommand.h"
#include "StringUtils.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# param hx1 ...
///# param hy1 ...
///# param hx2 ...
///# param hy2 ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Client_Global_GetHexDistance([[maybe_unused]] FOClient* client, ushort hx1, ushort hy1, ushort hx2, ushort hy2)
{
    return client->GeomHelper.DistGame(hx1, hy1, hx2, hy2);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uchar Client_Global_GetHexDir([[maybe_unused]] FOClient* client, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy)
{
    return client->GeomHelper.GetFarDir(fromHx, fromHy, toHx, toHy);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param offset ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uchar Client_Global_GetHexDirWithOffset([[maybe_unused]] FOClient* client, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, float offset)
{
    return client->GeomHelper.GetFarDir(fromHx, fromHy, toHx, toHy, offset);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Global_GetTick([[maybe_unused]] FOClient* client)
{
    return client->GameTime.FrameTick();
}

///# ...
///# param command ...
///# param separator ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Global_CustomCall([[maybe_unused]] FOClient* client, string_view command, string_view separator)
{
    // Parse command
    vector<string> args;
    const auto command_str = string(command);
    std::stringstream ss(command_str);
    if (separator.length() > 0) {
        string arg;
        const auto sep = *separator.data();
        while (std::getline(ss, arg, sep)) {
            args.push_back(arg);
        }
    }
    else {
        args.push_back(string(command));
    }
    if (args.empty()) {
        throw ScriptException("Empty custom call command");
    }

    // Execute
    const auto cmd = args[0];
    if (cmd == "Login" && args.size() >= 3) {
        if (client->InitNetReason == INIT_NET_REASON_NONE) {
            client->LoginName = args[1];
            client->LoginPassword = args[2];
            client->InitNetReason = INIT_NET_REASON_LOGIN;
        }
    }
    else if (cmd == "Register" && args.size() >= 3) {
        if (client->InitNetReason == INIT_NET_REASON_NONE) {
            client->LoginName = args[1];
            client->LoginPassword = args[2];
            client->InitNetReason = INIT_NET_REASON_REG;
        }
    }
    else if (cmd == "CustomConnect") {
        if (client->InitNetReason == INIT_NET_REASON_NONE) {
            client->InitNetReason = INIT_NET_REASON_CUSTOM;
            if (!client->Update) {
                client->Update = FOClient::ClientUpdate();
            }
        }
    }
    else if (cmd == "DumpAtlases") {
        client->SprMngr.DumpAtlases();
    }
    else if (cmd == "SwitchShowTrack") {
        client->HexMngr.SwitchShowTrack();
    }
    else if (cmd == "SwitchShowHex") {
        client->HexMngr.SwitchShowHex();
    }
    else if (cmd == "SwitchFullscreen") {
        if (!client->Settings.FullScreen) {
            if (client->SprMngr.EnableFullscreen()) {
                client->Settings.FullScreen = true;
            }
        }
        else {
            if (client->SprMngr.DisableFullscreen()) {
                client->Settings.FullScreen = false;

                if (client->WindowResolutionDiffX || client->WindowResolutionDiffY) {
                    const auto [x, y] = client->SprMngr.GetWindowPosition();
                    client->SprMngr.SetWindowPosition(x - client->WindowResolutionDiffX, y - client->WindowResolutionDiffY);
                    client->WindowResolutionDiffX = client->WindowResolutionDiffY = 0;
                }
            }
        }
    }
    else if (cmd == "MinimizeWindow") {
        client->SprMngr.MinimizeWindow();
    }
    else if (cmd == "SwitchLookBorders") {
        // client->DrawLookBorders = !client->DrawLookBorders;
        // client->RebuildLookBorders = true;
    }
    else if (cmd == "SwitchShootBorders") {
        // client->DrawShootBorders = !client->DrawShootBorders;
        // client->RebuildLookBorders = true;
    }
    else if (cmd == "GetShootBorders") {
        return client->DrawShootBorders ? "true" : "false";
    }
    else if (cmd == "SetShootBorders" && args.size() >= 2) {
        auto set = (args[1] == "true");
        if (client->DrawShootBorders != set) {
            client->DrawShootBorders = set;
            client->RebuildLookBorders = true;
        }
    }
    else if (cmd == "SetMousePos" && args.size() == 4) {
#if !FO_WEB
        /*int x = _str(args[1]).toInt();
        int y = _str(args[2]).toInt();
        bool motion = _str(args[3]).toBool();
        if (motion)
        {
            client->SprMngr.SetMousePosition(x, y);
        }
        else
        {
            SDL_EventState(SDL_MOUSEMOTION, SDL_DISABLE);
            client->SprMngr.SetMousePosition(x, y);
            SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
            client->Settings.MouseX = client->Settings.LastMouseX = x;
            client->Settings.MouseY = client->Settings.LastMouseY = y;
        }*/
#endif
    }
    else if (cmd == "SetCursorPos") {
        if (client->HexMngr.IsMapLoaded()) {
            client->HexMngr.SetCursorPos(client->Settings.MouseX, client->Settings.MouseY, client->Keyb.CtrlDwn, true);
        }
    }
    else if (cmd == "NetDisconnect") {
        client->NetDisconnect();

        if (!client->IsConnected && !client->IsMainScreen(SCREEN_LOGIN)) {
            client->ShowMainScreen(SCREEN_LOGIN, {});
        }
    }
    else if (cmd == "TryExit") {
        client->TryExit();
    }
    else if (cmd == "Version") {
        // return _str("{}", FO_VERSION);
        return _str("{}", "Unsupported");
    }
    else if (cmd == "BytesSend") {
        return _str("{}", client->BytesSend);
    }
    else if (cmd == "BytesReceive") {
        return _str("{}", client->BytesReceive);
    }
    else if (cmd == "GetLanguage") {
        return client->CurLang.Name;
    }
    else if (cmd == "SetLanguage" && args.size() >= 2) {
        if (args[1].length() == 4) {
            client->CurLang.LoadFromCache(client->Cache, args[1]);
        }
    }
    else if (cmd == "SetResolution" && args.size() >= 3) {
        auto w = _str(args[1]).toInt();
        auto h = _str(args[2]).toInt();
        auto diff_w = w - client->Settings.ScreenWidth;
        auto diff_h = h - client->Settings.ScreenHeight;

        client->Settings.ScreenWidth = w;
        client->Settings.ScreenHeight = h;
        client->SprMngr.SetWindowSize(w, h);

        if (!client->Settings.FullScreen) {
            const auto [x, y] = client->SprMngr.GetWindowPosition();
            client->SprMngr.SetWindowPosition(x - diff_w / 2, y - diff_h / 2);
        }
        else {
            client->WindowResolutionDiffX += diff_w / 2;
            client->WindowResolutionDiffY += diff_h / 2;
        }

        client->SprMngr.OnResolutionChanged();
        if (client->HexMngr.IsMapLoaded()) {
            client->HexMngr.OnResolutionChanged();
        }
    }
    else if (cmd == "RefreshAlwaysOnTop") {
        client->SprMngr.SetAlwaysOnTop(client->Settings.AlwaysOnTop);
    }
    else if (cmd == "Command" && args.size() >= 2) {
        string str;
        for (size_t i = 1; i < args.size(); i++) {
            str += args[i] + " ";
        }
        str = _str(str).trim();

        string buf;
        if (!PackNetCommand(
                str, &client->Bout,
                [&buf, &separator](auto s) {
                    buf += s;
                    buf += separator;
                },
                client->Chosen->AlternateName)) {
            return "UNKNOWN";
        }

        return buf;
    }
    else if (cmd == "ConsoleMessage" && args.size() >= 2) {
        client->Net_SendText(args[1], SAY_NORM);
    }
    else if (cmd == "SaveLog" && args.size() == 3) {
        //              if( file_name == "" )
        //              {
        //                      DateTime dt;
        //                      Timer::GetCurrentDateTime(dt);
        //                      char     log_path[TEMP_BUF_SIZE];
        //                      X_str(log_path, "messbox_%04d.%02d.%02d_%02d-%02d-%02d.txt",
        //                              dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);
        //              }

        //              for (uint i = 0; i < MessBox.size(); ++i)
        //              {
        //                      MessBoxMessage& m = MessBox[i];
        //                      // Skip
        //                      if (IsMainScreen(SCREEN_GAME) && std::find(MessBoxFilters.begin(), MessBoxFilters.end(),
        //                      m.Type) != MessBoxFilters.end())
        //                              continue;
        //                      // Concat
        //                      Str::Copy(cur_mess, m.Mess);
        //                      Str::EraseWords(cur_mess, '|', ' ');
        //                      fmt_log += MessBox[i].Time + string(cur_mess);
        //              }
    }
    else if (cmd == "DialogAnswer" && args.size() >= 4) {
        auto is_npc = (args[1] == "true");
        auto talker_id = _str(args[2]).toUInt();
        auto answer_index = _str(args[3]).toUInt();
        client->Net_SendTalk(is_npc, talker_id, static_cast<uchar>(answer_index));
    }
    else if (cmd == "DrawMiniMap" && args.size() >= 6) {
        static int zoom;
        static int x;
        static int y;
        static int x2;
        static int y2;
        zoom = _str(args[1]).toInt();
        x = _str(args[2]).toInt();
        y = _str(args[3]).toInt();
        x2 = x + _str(args[4]).toInt();
        y2 = y + _str(args[5]).toInt();

        if (zoom != client->LmapZoom || x != client->LmapWMap[0] || y != client->LmapWMap[1] || x2 != client->LmapWMap[2] || y2 != client->LmapWMap[3]) {
            client->LmapZoom = zoom;
            client->LmapWMap[0] = x;
            client->LmapWMap[1] = y;
            client->LmapWMap[2] = x2;
            client->LmapWMap[3] = y2;
            client->LmapPrepareMap();
        }
        else if (client->GameTime.FrameTick() >= client->LmapPrepareNextTick) {
            client->LmapPrepareMap();
        }

        client->SprMngr.DrawPoints(client->LmapPrepPix, RenderPrimitiveType::LineList, nullptr, nullptr, nullptr);
    }
    else if (cmd == "RefreshMe") {
        client->Net_SendRefereshMe();
    }
    else if (cmd == "SetCrittersContour" && args.size() == 2) {
        auto countour_type = _str(args[1]).toInt();
        client->HexMngr.SetCrittersContour(countour_type);
    }
    else if (cmd == "DrawWait") {
        client->WaitDraw();
    }
    else if (cmd == "ChangeDir" && args.size() == 2) {
        auto dir = _str(args[1]).toInt();
        client->Chosen->ChangeDir(static_cast<uchar>(dir), true);
        client->Net_SendDir();
    }
    else if (cmd == "MoveItem" && args.size() == 5) {
        auto item_count = _str(args[1]).toUInt();
        auto item_id = _str(args[2]).toUInt();
        auto item_swap_id = _str(args[3]).toUInt();
        auto to_slot = _str(args[4]).toInt();
        auto* item = client->Chosen->GetItem(item_id);
        auto* item_swap = (item_swap_id ? client->Chosen->GetItem(item_swap_id) : nullptr);
        auto* old_item = item->Clone();
        int from_slot = item->GetCritSlot();

        // Move
        auto is_light = item->GetIsLight();
        if (to_slot == -1) {
            client->Chosen->Action(ACTION_DROP_ITEM, from_slot, item, true);
            if (item->GetStackable() && item_count < item->GetCount()) {
                item->SetCount(item->GetCount() - item_count);
            }
            else {
                client->Chosen->DeleteItem(item, true);
                item = nullptr;
            }
        }
        else {
            item->SetCritSlot(static_cast<uchar>(to_slot));
            if (item_swap) {
                item_swap->SetCritSlot(static_cast<uchar>(from_slot));
            }

            client->Chosen->Action(ACTION_MOVE_ITEM, from_slot, item, true);
            if (item_swap) {
                client->Chosen->Action(ACTION_MOVE_ITEM_SWAP, to_slot, item_swap, true);
            }
        }

        // Light
        client->RebuildLookBorders = true;
        if (is_light && (!to_slot || (!from_slot && to_slot != -1))) {
            client->HexMngr.RebuildLight();
        }

        // Notify scripts about item changing
        client->OnItemInvChanged(old_item, item);
    }
    else if (cmd == "SkipRoof" && args.size() == 3) {
        auto hx = _str(args[1]).toUInt();
        auto hy = _str(args[2]).toUInt();
        client->HexMngr.SetSkipRoof(hx, hy);
    }
    else if (cmd == "RebuildLookBorders") {
        client->RebuildLookBorders = true;
    }
    else if (cmd == "TransitCritter" && args.size() == 5) {
        auto hx = _str(args[1]).toInt();
        auto hy = _str(args[2]).toInt();
        auto animate = _str(args[3]).toBool();
        auto force = _str(args[4]).toBool();

        client->HexMngr.TransitCritter(client->Chosen, hx, hy, animate, force);
    }
    else if (cmd == "SendMove") {
        vector<uchar> dirs;
        for (size_t i = 1; i < args.size(); i++) {
            dirs.push_back(static_cast<uchar>(_str(args[i]).toInt()));
        }

        client->Net_SendMove(dirs);

        if (dirs.size() > 1) {
            client->Chosen->MoveSteps.resize(1);
        }
        else {
            client->Chosen->MoveSteps.resize(0);
            if (!client->Chosen->IsAnim()) {
                client->Chosen->AnimateStay();
            }
        }
    }
    else if (cmd == "ChosenAlpha" && args.size() == 2) {
        auto alpha = _str(args[1]).toInt();

        client->Chosen->Alpha = static_cast<uchar>(alpha);
    }
    else if (cmd == "SetScreenKeyboard" && args.size() == 2) {
        /*if (SDL_HasScreenKeyboardSupport())
        {
            bool cur = (SDL_IsTextInputActive() != SDL_FALSE);
            bool next = _str(args[1]).toBool();
            if (cur != next)
            {
                if (next)
                    SDL_StartTextInput();
                else
                    SDL_StopTextInput();
            }
        }*/
    }
    else {
        throw ScriptException("Invalid custom call command");
    }
    return "";
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Client_Global_GetChosen([[maybe_unused]] FOClient* client)
{
    if (client->Chosen && client->Chosen->IsDestroyed) {
        return static_cast<CritterView*>(nullptr);
    }
    return client->Chosen;
}

///# ...
///# param itemId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Global_GetItem([[maybe_unused]] FOClient* client, uint itemId)
{
    if (itemId == 0u) {
        throw ScriptException("Item id arg is zero");
    }

    // On map
    ItemView* item = client->GetItem(itemId);

    // On Chosen
    if (!item && client->Chosen) {
        item = client->Chosen->GetItem(itemId);
    }

    // On other critters
    if (item == nullptr) {
        for (auto it = client->HexMngr.GetCritters().begin(); !item && it != client->HexMngr.GetCritters().end(); ++it) {
            if (!it->second->IsChosen()) {
                item = it->second->GetItem(itemId);
            }
        }
    }

    if (item == nullptr || item->IsDestroyed) {
        return static_cast<ItemView*>(nullptr);
    }
    return item;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ItemView*> Client_Global_GetVisibleItems([[maybe_unused]] FOClient* client)
{
    vector<ItemView*> items;
    if (client->HexMngr.IsMapLoaded()) {
        auto& items_ = client->HexMngr.GetItems();
        for (auto it = items_.begin(); it != items_.end();) {
            it = ((*it)->IsFinishing() ? items_.erase(it) : ++it);
        }
        for (ItemView* item : items_) {
            items.push_back(item);
        }
    }
    return items;
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ItemView*> Client_Global_GetVisibleItemsOnHex([[maybe_unused]] FOClient* client, ushort hx, ushort hy)
{
    vector<ItemHexView*> hex_items;
    if (client->HexMngr.IsMapLoaded()) {
        client->HexMngr.GetItems(hx, hy, hex_items);
        for (auto it = hex_items.begin(); it != hex_items.end();) {
            it = ((*it)->IsFinishing() ? hex_items.erase(it) : ++it);
        }
    }

    vector<ItemView*> items;
    items.reserve(hex_items.size());
    for (auto* item : hex_items) {
        items.push_back(item);
    }

    return items;
}

///# ...
///# param cr1 ...
///# param cr2 ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] int Client_Global_GetCritterDistance([[maybe_unused]] FOClient* client, CritterView* cr1, CritterView* cr2)
{
    if (!client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }
    if (cr1 == nullptr) {
        throw ScriptException("Critter1 arg is null");
    }
    if (cr2 == nullptr) {
        throw ScriptException("Critter2 arg is null");
    }

    return client->GeomHelper.DistGame(cr1->GetHexX(), cr1->GetHexY(), cr2->GetHexX(), cr2->GetHexY());
}

///# ...
///# param critterId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] CritterView* Client_Global_GetCritter([[maybe_unused]] FOClient* client, uint critterId)
{
    if (critterId == 0u) {
        return static_cast<CritterView*>(nullptr); // throw ScriptException("Critter id arg is zero";
    }
    auto* const cr = client->GetCritter(critterId);
    if (!cr || cr->IsDestroyed) {
        return static_cast<CritterView*>(nullptr);
    }
    return cr;
}

///# ...
///# param hx ...
///# param hy ...
///# param radius ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Global_GetCrittersAroundHex([[maybe_unused]] FOClient* client, ushort hx, ushort hy, uint radius, uchar findType)
{
    if (hx >= client->HexMngr.GetWidth() || hy >= client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    auto& crits = client->HexMngr.GetCritters();
    vector<CritterView*> critters;
    for (auto it = crits.begin(), end = crits.end(); it != end; ++it) {
        auto* cr = it->second;
        if (cr->CheckFind(findType) && client->GeomHelper.CheckDist(hx, hy, cr->GetHexX(), cr->GetHexY(), radius)) {
            critters.push_back(cr);
        }
    }

    std::sort(critters.begin(), critters.end(), [client, &hx, &hy](CritterView* cr1, CritterView* cr2) { return client->GeomHelper.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY()) < client->GeomHelper.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY()); });

    return critters;
}

///# ...
///# param pid ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Global_GetCrittersByPids([[maybe_unused]] FOClient* client, hash pid, uchar findType)
{
    auto& crits = client->HexMngr.GetCritters();
    vector<CritterView*> critters;
    if (pid == 0u) {
        for (auto it = crits.begin(), end = crits.end(); it != end; ++it) {
            auto* cr = it->second;
            if (cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }
    else {
        for (auto it = crits.begin(), end = crits.end(); it != end; ++it) {
            auto* cr = it->second;
            if (cr->IsNpc() && cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }
    return critters;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param angle ...
///# param dist ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Global_GetCrittersInPath([[maybe_unused]] FOClient* client, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, float angle, uint dist, int findType)
{
    vector<CritterView*> critters;
    client->HexMngr.TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, nullptr, false, &critters, findType, nullptr, nullptr, nullptr, true);
    return critters;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param angle ...
///# param dist ...
///# param findType ...
///# param preBlockHx ...
///# param preBlockHy ...
///# param blockHx ...
///# param blockHy ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Global_GetCrittersWithBlockInPath([[maybe_unused]] FOClient* client, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, float angle, uint dist, int findType, ushort& preBlockHx, ushort& preBlockHy, ushort& blockHx, ushort& blockHy)
{
    vector<CritterView*> critters;
    pair<ushort, ushort> block;
    pair<ushort, ushort> pre_block;
    client->HexMngr.TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, nullptr, false, &critters, findType, &block, &pre_block, nullptr, true);
    preBlockHx = pre_block.first;
    preBlockHy = pre_block.second;
    blockHx = block.first;
    blockHy = block.second;
    return critters;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param angle ...
///# param dist ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Client_Global_GetHexInPath([[maybe_unused]] FOClient* client, ushort fromHx, ushort fromHy, ushort& toHx, ushort& toHy, float angle, uint dist)
{
    pair<ushort, ushort> pre_block;
    pair<ushort, ushort> block;
    client->HexMngr.TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, nullptr, false, nullptr, 0, &block, &pre_block, nullptr, true);
    toHx = pre_block.first;
    toHy = pre_block.second;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<uchar> Client_Global_GetPathToHex([[maybe_unused]] FOClient* client, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, uint cut)
{
    if (fromHx >= client->HexMngr.GetWidth() || fromHy >= client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= client->HexMngr.GetWidth() || toHy >= client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (cut > 0 && !client->HexMngr.CutPath(nullptr, fromHx, fromHy, to_hx, to_hy, cut)) {
        return vector<uchar>();
    }

    vector<uchar> steps;
    if (!client->HexMngr.FindPath(nullptr, fromHx, fromHy, to_hx, to_hy, steps, -1)) {
        return vector<uchar>();
    }

    return steps;
}

///# ...
///# param cr ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<uchar> Client_Global_GetPathToCritter([[maybe_unused]] FOClient* client, CritterView* cr, ushort toHx, ushort toHy, uint cut)
{
    if (toHx >= client->HexMngr.GetWidth() || toHy >= client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (cut > 0 && !client->HexMngr.CutPath(cr, cr->GetHexX(), cr->GetHexY(), to_hx, to_hy, cut)) {
        return vector<uchar>();
    }

    vector<uchar> steps;
    if (!client->HexMngr.FindPath(cr, cr->GetHexX(), cr->GetHexY(), to_hx, to_hy, steps, -1)) {
        return vector<uchar>();
    }

    return steps;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Global_GetPathLengthToHex([[maybe_unused]] FOClient* client, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, uint cut)
{
    if (fromHx >= client->HexMngr.GetWidth() || fromHy >= client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= client->HexMngr.GetWidth() || toHy >= client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (cut > 0 && !client->HexMngr.CutPath(nullptr, fromHx, fromHy, to_hx, to_hy, cut)) {
        return 0;
    }

    vector<uchar> steps;
    if (!client->HexMngr.FindPath(nullptr, fromHx, fromHy, to_hx, to_hy, steps, -1)) {
        steps.clear();
    }

    return static_cast<uint>(steps.size());
}

///# ...
///# param cr ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Global_GetPathLengthToCritter([[maybe_unused]] FOClient* client, CritterView* cr, ushort toHx, ushort toHy, uint cut)
{
    if (toHx >= client->HexMngr.GetWidth() || toHy >= client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (cut > 0 && !client->HexMngr.CutPath(cr, cr->GetHexX(), cr->GetHexY(), to_hx, to_hy, cut)) {
        return 0;
    }

    vector<uchar> steps;
    if (!client->HexMngr.FindPath(cr, cr->GetHexX(), cr->GetHexY(), to_hx, to_hy, steps, -1)) {
        steps.clear();
    }

    return static_cast<uint>(steps.size());
}

///# ...
///# param fromColor ...
///# param toColor ...
///# param ms ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_FlushScreen([[maybe_unused]] FOClient* client, uint fromColor, uint toColor, uint ms)
{
    client->ScreenFade(ms, fromColor, toColor, true);
}

///# ...
///# param noise ...
///# param ms ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_QuakeScreen([[maybe_unused]] FOClient* client, uint noise, uint ms)
{
    client->ScreenQuake(noise, ms);
}

///# ...
///# param soundName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_PlaySound([[maybe_unused]] FOClient* client, string_view soundName)
{
    if (!client->SndMngr.PlaySound(client->ResMngr.GetSoundNames(), soundName)) {
        WriteLog("Sound '{}' not found.\n", soundName);
    }
}

///# ...
///# param musicName ...
///# param repeatTime ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_PlayMusic([[maybe_unused]] FOClient* client, string_view musicName, uint repeatTime)
{
    if (musicName.empty()) {
        client->SndMngr.StopMusic();
        return;
    }

    if (!client->SndMngr.PlayMusic(musicName, repeatTime)) {
        WriteLog("Music '{}' not found.\n", musicName);
    }
}

///# ...
///# param videoName ...
///# param canStop ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_PlayVideo([[maybe_unused]] FOClient* client, string_view videoName, bool canStop)
{
    // client->SndMngr.StopMusic();
    // client->AddVideo(videoName, canStop, true);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] hash Client_Global_GetCurMapPid([[maybe_unused]] FOClient* client)
{
    if (!client->HexMngr.IsMapLoaded()) {
        return 0;
    }
    return client->CurMapPid;
}

///# ...
///# param msg ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_Message([[maybe_unused]] FOClient* client, string_view msg)
{
    client->AddMess(SAY_NETMSG, msg, true);
}

///# ...
///# param msg ...
///# param type ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_MessageExt([[maybe_unused]] FOClient* client, string_view msg, int type)
{
    client->AddMess(type, msg, true);
}

///# ...
///# param textMsg ...
///# param strNum ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_MessageMsg([[maybe_unused]] FOClient* client, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    client->AddMess(SAY_NETMSG, client->CurLang.Msg[textMsg].GetStr(strNum), true);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# param type ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_MessageMsgExt([[maybe_unused]] FOClient* client, int textMsg, uint strNum, int type)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    client->AddMess(type, client->CurLang.Msg[textMsg].GetStr(strNum), true);
}

///# ...
///# param text ...
///# param hx ...
///# param hy ...
///# param ms ...
///# param color ...
///# param fade ...
///# param ox ...
///# param oy ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_MapMessage([[maybe_unused]] FOClient* client, string_view text, ushort hx, ushort hy, uint ms, uint color, bool fade, int ox, int oy)
{
    FOClient::MapText map_text;
    map_text.HexX = hx;
    map_text.HexY = hy;
    map_text.Color = (color != 0u ? color : COLOR_TEXT);
    map_text.Fade = fade;
    map_text.StartTick = client->GameTime.GameTick();
    map_text.Tick = ms;
    map_text.Text = text;
    map_text.Pos = client->HexMngr.GetRectForText(hx, hy);
    map_text.EndPos = IRect(map_text.Pos, ox, oy);

    const auto it = std::find_if(client->GameMapTexts.begin(), client->GameMapTexts.end(), [&map_text](const FOClient::MapText& t) { return t.HexX == map_text.HexX && t.HexY == map_text.HexY; });
    if (it != client->GameMapTexts.end()) {
        client->GameMapTexts.erase(it);
    }

    client->GameMapTexts.push_back(map_text);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Global_GetMsgStr([[maybe_unused]] FOClient* client, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    return client->CurLang.Msg[textMsg].GetStr(strNum);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# param skipCount ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Global_GetMsgStrExt([[maybe_unused]] FOClient* client, int textMsg, uint strNum, uint skipCount)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    return client->CurLang.Msg[textMsg].GetStr(strNum, skipCount);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Global_GetMsgStrNumUpper([[maybe_unused]] FOClient* client, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    return client->CurLang.Msg[textMsg].GetStrNumUpper(strNum);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Global_GetMsgStrNumLower([[maybe_unused]] FOClient* client, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    return client->CurLang.Msg[textMsg].GetStrNumLower(strNum);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Global_GetMsgStrCount([[maybe_unused]] FOClient* client, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    return client->CurLang.Msg[textMsg].Count(strNum);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Global_IsMsgStr([[maybe_unused]] FOClient* client, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    return client->CurLang.Msg[textMsg].Count(strNum) > 0;
}

///# ...
///# param text ...
///# param replace ...
///# param str ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Global_ReplaceTextStr([[maybe_unused]] FOClient* client, string_view text, string_view replace, string_view str)
{
    const auto pos = text.find(replace, 0);
    if (pos == std::string::npos) {
        return string(text);
    }
    return string(text).replace(pos, replace.length(), str);
}

///# ...
///# param text ...
///# param replace ...
///# param i ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Global_ReplaceTextInt([[maybe_unused]] FOClient* client, string_view text, string_view replace, int i)
{
    const auto pos = text.find(replace, 0);
    if (pos == std::string::npos) {
        return string(text);
    }
    return string(text).replace(pos, replace.length(), _str("{}", i).strv());
}

///# ...
///# param text ...
///# param lexems ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Global_FormatTags([[maybe_unused]] FOClient* client, string_view text, string_view lexems)
{
    auto text_copy = string(text);
    client->FormatTags(text_copy, client->Chosen, nullptr, lexems);
    return text_copy;
}

///# ...
///# param hx ...
///# param hy ...
///# param speed ...
///# param canStop ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_MoveScreenToHex([[maybe_unused]] FOClient* client, ushort hx, ushort hy, uint speed, bool canStop)
{
    if (!client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }
    if (hx >= client->HexMngr.GetWidth() || hy >= client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    if (speed == 0u) {
        client->HexMngr.FindSetCenter(hx, hy);
    }
    else {
        client->HexMngr.ScrollToHex(hx, hy, static_cast<float>(speed) / 1000.0f, canStop);
    }
}

///# ...
///# param ox ...
///# param oy ...
///# param speed ...
///# param canStop ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_MoveScreenByOffset([[maybe_unused]] FOClient* client, int ox, int oy, uint speed, bool canStop)
{
    if (!client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }

    client->HexMngr.ScrollOffset(ox, oy, static_cast<float>(speed) / 1000.0f, canStop);
}

///# ...
///# param cr ...
///# param softLock ...
///# param unlockIfSame ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_LockScreenScroll([[maybe_unused]] FOClient* client, CritterView* cr, bool softLock, bool unlockIfSame)
{
    const auto id = (cr != nullptr ? cr->GetId() : 0);
    if (softLock) {
        if (unlockIfSame && id == client->HexMngr.AutoScroll.SoftLockedCritter) {
            client->HexMngr.AutoScroll.SoftLockedCritter = 0;
        }
        else {
            client->HexMngr.AutoScroll.SoftLockedCritter = id;
        }

        client->HexMngr.AutoScroll.CritterLastHexX = (cr ? cr->GetHexX() : 0);
        client->HexMngr.AutoScroll.CritterLastHexY = (cr ? cr->GetHexY() : 0);
    }
    else {
        if (unlockIfSame && id == client->HexMngr.AutoScroll.HardLockedCritter) {
            client->HexMngr.AutoScroll.HardLockedCritter = 0;
        }
        else {
            client->HexMngr.AutoScroll.HardLockedCritter = id;
        }
    }
}

///# ...
///# param zoneX ...
///# param zoneY ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] int Client_Global_GetFog([[maybe_unused]] FOClient* client, ushort zoneX, ushort zoneY)
{
    if (zoneX >= client->Settings.GlobalMapWidth || zoneY >= client->Settings.GlobalMapHeight) {
        throw ScriptException("Invalid world map pos arg");
    }
    return client->GmapFog.Get2Bit(zoneX, zoneY);
}

///# ...
///# param dayPart ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Global_GetDayTime([[maybe_unused]] FOClient* client, uint dayPart)
{
    if (dayPart >= 4) {
        throw ScriptException("Invalid day part arg");
    }

    if (client->HexMngr.IsMapLoaded()) {
        return client->HexMngr.GetMapDayTime()[dayPart];
    }
    return 0;
}

///# ...
///# param dayPart ...
///# param r ...
///# param g ...
///# param b ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_GetDayColor([[maybe_unused]] FOClient* client, uint dayPart, uchar& r, uchar& g, uchar& b)
{
    r = g = b = 0;
    if (dayPart >= 4) {
        throw ScriptException("Invalid day part arg");
    }

    if (client->HexMngr.IsMapLoaded()) {
        auto* const col = client->HexMngr.GetMapDayColor();
        r = col[0 + dayPart];
        g = col[4 + dayPart];
        b = col[8 + dayPart];
    }
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Global_GetFullSecond([[maybe_unused]] FOClient* client)
{
    return client->GameTime.GetFullSecond();
}

///# ...
///# param year ...
///# param month ...
///# param day ...
///# param hour ...
///# param minute ...
///# param second ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Global_EvaluateFullSecond([[maybe_unused]] FOClient* client, ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second)
{
    if (year && year < client->Settings.StartYear) {
        throw ScriptException("Invalid year", year);
    }
    if (year && year > client->Settings.StartYear + 100) {
        throw ScriptException("Invalid year", year);
    }
    if (month != 0u && month < 1) {
        throw ScriptException("Invalid month", month);
    }
    if (month != 0u && month > 12) {
        throw ScriptException("Invalid month", month);
    }

    auto year_ = year;
    auto month_ = month;
    auto day_ = day;

    if (year_ == 0u) {
        year_ = client->Globals->GetYear();
    }
    if (month_ == 0u) {
        month_ = client->Globals->GetMonth();
    }

    if (day_ != 0u) {
        const auto month_day = client->GameTime.GameTimeMonthDay(year, month_);
        if (day_ < month_day || day_ > month_day) {
            throw ScriptException("Invalid day", day_, month_day);
        }
    }

    if (day_ == 0u) {
        day_ = client->Globals->GetDay();
    }

    if (hour > 23) {
        throw ScriptException("Invalid hour", hour);
    }
    if (minute > 59) {
        throw ScriptException("Invalid minute", minute);
    }
    if (second > 59) {
        throw ScriptException("Invalid second", second);
    }

    return client->GameTime.EvaluateFullSecond(year_, month_, day_, hour, minute, second);
}

///# ...
///# param fullSecond ...
///# param year ...
///# param month ...
///# param day ...
///# param dayOfWeek ...
///# param hour ...
///# param minute ...
///# param second ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Client_Global_GetGameTime([[maybe_unused]] FOClient* client, uint fullSecond, ushort& year, ushort& month, ushort& day, ushort& dayOfWeek, ushort& hour, ushort& minute, ushort& second)
{
    const auto dt = client->GameTime.GetGameTime(fullSecond);
    year = dt.Year;
    month = dt.Month;
    dayOfWeek = dt.DayOfWeek;
    day = dt.Day;
    hour = dt.Hour;
    minute = dt.Minute;
    second = dt.Second;
}

///# ...
///# param hx ...
///# param hy ...
///# param dir ...
///# param steps ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Global_MoveHexByDir([[maybe_unused]] FOClient* client, ushort& hx, ushort& hy, uchar dir, uint steps)
{
    if (!client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (dir >= client->Settings.MapDirCount) {
        throw ScriptException("Invalid dir arg");
    }
    if (steps == 0u) {
        throw ScriptException("Steps arg is zero");
    }

    auto result = false;
    auto hx_ = hx;
    auto hy_ = hy;

    if (steps > 1) {
        for (uint i = 0; i < steps; i++) {
            result |= client->GeomHelper.MoveHexByDir(hx_, hy_, dir, client->HexMngr.GetWidth(), client->HexMngr.GetHeight());
        }
    }
    else {
        result = client->GeomHelper.MoveHexByDir(hx_, hy_, dir, client->HexMngr.GetWidth(), client->HexMngr.GetHeight());
    }

    hx = hx_;
    hy = hy_;
    return result;
}

///# ...
///# param hx ...
///# param hy ...
///# param roof ...
///# param layer ...
///# return ...
///@ ExportMethod
[[maybe_unused]] hash Client_Global_GetTileName([[maybe_unused]] FOClient* client, ushort hx, ushort hy, bool roof, int layer)
{
    if (!client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= client->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    const auto* simply_tile = client->HexMngr.GetField(hx, hy).SimplyTile[roof ? 1 : 0];
    if (simply_tile && !layer) {
        return simply_tile->NameHash;
    }

    const auto* tiles = client->HexMngr.GetField(hx, hy).Tiles[roof ? 1 : 0];
    if (tiles == nullptr || tiles->empty()) {
        return 0;
    }

    for (const auto& tile : *tiles) {
        if (tile.Layer == layer) {
            return tile.Anim->NameHash;
        }
    }
    return 0;
}

///# ...
///# param fnames ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_Preload3dFiles([[maybe_unused]] FOClient* client, const vector<string>& fnames)
{
    for (size_t i = 0; i < fnames.size(); i++) {
        client->Preload3dFiles.push_back(fnames[i]);
    }
}

///# ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Client_Global_WaitPing([[maybe_unused]] FOClient* client)
{
    client->WaitPing();
}

///# ...
///# param fontIndex ...
///# param fontFname ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_LoadFont([[maybe_unused]] FOClient* client, int fontIndex, string_view fontFname)
{
    client->SprMngr.PushAtlasType(AtlasType::Static);

    bool result;
    if (fontFname.length() > 0 && fontFname[0] == '*') {
        result = client->SprMngr.LoadFontFO(fontIndex, fontFname.substr(1), false, false);
    }
    else {
        result = client->SprMngr.LoadFontBmf(fontIndex, fontFname);
    }

    if (result && !client->SprMngr.IsAccumulateAtlasActive()) {
        client->SprMngr.BuildFonts();
    }
    client->SprMngr.PopAtlasType();

    if (!result) {
        throw ScriptException("Can't load font", fontIndex, fontFname);
    }
}

///# ...
///# param font ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_SetDefaultFont([[maybe_unused]] FOClient* client, int font, uint color)
{
    client->SprMngr.SetDefaultFont(font, color);
}

///# ...
///# param effectType ...
///# param effectSubtype ...
///# param effectName ...
///# param effectDefines ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_SetEffect([[maybe_unused]] FOClient* client, EffectType effectType, int effectSubtype, string_view effectName, string_view effectDefines)
{
    RenderEffect* effect = nullptr;
    if (!effectName.empty()) {
        effect = client->EffectMngr.LoadEffect(effectName, effectDefines, "");
        if (!effect) {
            throw ScriptException("Effect not found or have some errors, see log file");
        }
    }

    const auto eff_type = static_cast<uint>(effectType);

    if ((eff_type & static_cast<uint>(EffectType::GenericSprite)) && effectSubtype != 0) {
        auto* item = client->GetItem(static_cast<uint>(effectSubtype));
        if (item) {
            item->DrawEffect = (effect ? effect : client->EffectMngr.Effects.Generic);
        }
    }
    if ((eff_type & static_cast<uint>(EffectType::CritterSprite)) && effectSubtype != 0) {
        auto* cr = client->GetCritter(static_cast<uint>(effectSubtype));
        if (cr) {
            cr->DrawEffect = (effect ? effect : client->EffectMngr.Effects.Critter);
        }
    }

    if ((eff_type & static_cast<uint>(EffectType::GenericSprite)) && effectSubtype == 0) {
        client->EffectMngr.Effects.Generic = (effect ? effect : client->EffectMngr.Effects.GenericDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::CritterSprite)) && effectSubtype == 0) {
        client->EffectMngr.Effects.Critter = (effect ? effect : client->EffectMngr.Effects.CritterDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::TileSprite)) {
        client->EffectMngr.Effects.Tile = (effect ? effect : client->EffectMngr.Effects.TileDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::RoofSprite)) {
        client->EffectMngr.Effects.Roof = (effect ? effect : client->EffectMngr.Effects.RoofDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::RainSprite)) {
        client->EffectMngr.Effects.Rain = (effect ? effect : client->EffectMngr.Effects.RainDefault);
    }

    if (eff_type & static_cast<uint>(EffectType::SkinnedMesh)) {
        client->EffectMngr.Effects.Skinned3d = (effect ? effect : client->EffectMngr.Effects.Skinned3dDefault);
    }

    if (eff_type & static_cast<uint>(EffectType::Interface)) {
        client->EffectMngr.Effects.Iface = (effect ? effect : client->EffectMngr.Effects.IfaceDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::Contour)) {
        client->EffectMngr.Effects.Contour = (effect ? effect : client->EffectMngr.Effects.ContourDefault);
    }

    if ((eff_type & static_cast<uint>(EffectType::Font)) && effectSubtype == -1) {
        client->EffectMngr.Effects.Font = (effect ? effect : client->EffectMngr.Effects.ContourDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::Font)) && effectSubtype >= 0) {
        client->SprMngr.SetFontEffect(effectSubtype, effect);
    }

    if (eff_type & static_cast<uint>(EffectType::Primitive)) {
        client->EffectMngr.Effects.Primitive = (effect ? effect : client->EffectMngr.Effects.PrimitiveDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::Light)) {
        client->EffectMngr.Effects.Light = (effect ? effect : client->EffectMngr.Effects.LightDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::Fog)) {
        client->EffectMngr.Effects.Fog = (effect ? effect : client->EffectMngr.Effects.FogDefault);
    }

    if (eff_type & static_cast<uint>(EffectType::FlushRenderTarget)) {
        client->EffectMngr.Effects.FlushRenderTarget = (effect ? effect : client->EffectMngr.Effects.FlushRenderTargetDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::FlushPrimitive)) {
        client->EffectMngr.Effects.FlushPrimitive = (effect ? effect : client->EffectMngr.Effects.FlushPrimitiveDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::FlushMap)) {
        client->EffectMngr.Effects.FlushMap = (effect ? effect : client->EffectMngr.Effects.FlushMapDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::FlushLight)) {
        client->EffectMngr.Effects.FlushLight = (effect ? effect : client->EffectMngr.Effects.FlushLightDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::FlushFog)) {
        client->EffectMngr.Effects.FlushFog = (effect ? effect : client->EffectMngr.Effects.FlushFogDefault);
    }

    if (eff_type & static_cast<uint>(EffectType::Offscreen)) {
        if (effectSubtype < 0) {
            throw ScriptException("Negative effect subtype");
        }

        client->OffscreenEffects.resize(effectSubtype + 1);
        client->OffscreenEffects[effectSubtype] = effect;
    }
}

///# ...
///# param onlyTiles ...
///# param onlyRoof ...
///# param onlyLight ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_RedrawMap([[maybe_unused]] FOClient* client, bool onlyTiles, bool onlyRoof, bool onlyLight)
{
    if (client->HexMngr.IsMapLoaded()) {
        if (onlyTiles) {
            client->HexMngr.RebuildTiles();
        }
        else if (onlyRoof) {
            client->HexMngr.RebuildRoof();
        }
        else if (onlyLight) {
            client->HexMngr.RebuildLight();
        }
        else {
            client->HexMngr.RefreshMap();
        }
    }
}

///# ...
///# param x ...
///# param y ...
///# param button ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_SimulateMouseClick([[maybe_unused]] FOClient* client, int x, int y, int button)
{
    /*App->Input.PushEvent({InputEvent::MouseDown({(MouseButton)button})});

    IntVec prev_events = client->Settings.MainWindowMouseEvents;
    client->Settings.MainWindowMouseEvents.clear();
    int prev_x = client->Settings.MouseX;
    int prev_y = client->Settings.MouseY;
    int last_prev_x = client->Settings.LastMouseX;
    int last_prev_y = client->Settings.LastMouseY;
    client->Settings.MouseX = client->Settings.LastMouseX = x;
    client->Settings.MouseY = client->Settings.LastMouseY = y;
    client->Settings.MainWindowMouseEvents.push_back(SDL_MOUSEBUTTONDOWN);
    client->Settings.MainWindowMouseEvents.push_back(button);
    client->Settings.MainWindowMouseEvents.push_back(SDL_MOUSEBUTTONUP);
    client->Settings.MainWindowMouseEvents.push_back(button);
    client->ParseMouse();
    client->Settings.MainWindowMouseEvents = prev_events;
    client->Settings.MouseX = prev_x;
    client->Settings.MouseY = prev_y;
    client->Settings.LastMouseX = last_prev_x;
    client->Settings.LastMouseY = last_prev_y;*/
}

///# ...
///# param key1 ...
///# param key2 ...
///# param key1Text ...
///# param key2Text ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_SimulateKeyboardPress([[maybe_unused]] FOClient* client, KeyCode key1, KeyCode key2, string_view key1Text, string_view key2Text)
{
    if (key1 == KeyCode::None && key2 == KeyCode::None) {
        return;
    }

    if (key1 != KeyCode::None) {
        client->ProcessInputEvent(InputEvent {InputEvent::KeyDownEvent {key1, string(key1Text)}});
    }

    if (key2 != KeyCode::None) {
        client->ProcessInputEvent(InputEvent {InputEvent::KeyDownEvent {key2, string(key2Text)}});
        client->ProcessInputEvent(InputEvent {InputEvent::KeyUpEvent {key2}});
    }

    if (key1 != KeyCode::None) {
        client->ProcessInputEvent(InputEvent {InputEvent::KeyUpEvent {key1}});
    }
}

///# ...
///# param fallAnimName ...
///# param dropAnimName ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_SetRainAnim([[maybe_unused]] FOClient* client, string_view fallAnimName, string_view dropAnimName)
{
    client->HexMngr.SetRainAnimation(fallAnimName, dropAnimName);
}

///# ...
///# param targetZoom ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_ChangeZoom([[maybe_unused]] FOClient* client, float targetZoom)
{
    if (targetZoom == client->Settings.SpritesZoom) {
        return;
    }

    const auto init_zoom = client->Settings.SpritesZoom;
    if (targetZoom == 1.0f) {
        client->HexMngr.ChangeZoom(0);
    }
    else if (targetZoom > client->Settings.SpritesZoom) {
        while (targetZoom > client->Settings.SpritesZoom) {
            const auto old_zoom = client->Settings.SpritesZoom;
            client->HexMngr.ChangeZoom(1);
            if (client->Settings.SpritesZoom == old_zoom) {
                break;
            }
        }
    }
    else if (targetZoom < client->Settings.SpritesZoom) {
        while (targetZoom < client->Settings.SpritesZoom) {
            const auto old_zoom = client->Settings.SpritesZoom;
            client->HexMngr.ChangeZoom(-1);
            if (client->Settings.SpritesZoom == old_zoom) {
                break;
            }
        }
    }

    if (init_zoom != client->Settings.SpritesZoom) {
        client->RebuildLookBorders = true;
    }
}

///# ...
///# param year ...
///# param month ...
///# param day ...
///# param dayOfWeek ...
///# param hour ...
///# param minute ...
///# param second ...
///# param milliseconds ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Client_Global_GetTime([[maybe_unused]] FOClient* client, ushort& year, ushort& month, ushort& day, ushort& dayOfWeek, ushort& hour, ushort& minute, ushort& second, ushort& milliseconds)
{
    const auto cur_time = Timer::GetCurrentDateTime();
    year = cur_time.Year;
    month = cur_time.Month;
    dayOfWeek = cur_time.DayOfWeek;
    day = cur_time.Day;
    hour = cur_time.Hour;
    minute = cur_time.Minute;
    second = cur_time.Second;
    milliseconds = cur_time.Milliseconds;
}

///# ...
///# param datName ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Client_Global_AddDataSource([[maybe_unused]] FOClient* client, string_view datName)
{
    client->FileMngr.AddDataSource(datName, true);
}

///# ...
///# param sprName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Global_LoadSprite([[maybe_unused]] FOClient* client, string_view sprName)
{
    return client->AnimLoad(sprName, AtlasType::Static);
}

///# ...
///# param nameHash ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Global_LoadSpriteByHash([[maybe_unused]] FOClient* client, hash nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::Static);
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Client_Global_GetSpriteWidth([[maybe_unused]] FOClient* client, uint sprId, int frameIndex)
{
    auto* anim = client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return 0;
    }

    const auto* si = client->SprMngr.GetSpriteInfo(frameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (!si) {
        return 0;
    }

    return si->Width;
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Client_Global_GetSpriteHeight([[maybe_unused]] FOClient* client, uint sprId, int frameIndex)
{
    auto* anim = client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return 0;
    }
    const auto* si = client->SprMngr.GetSpriteInfo(frameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (!si) {
        return 0;
    }

    return si->Height;
}

///# ...
///# param sprId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Global_GetSpriteCount([[maybe_unused]] FOClient* client, uint sprId)
{
    auto* const anim = client->AnimGetFrames(sprId);
    return anim ? anim->CntFrm : 0;
}

///# ...
///# param sprId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Global_GetSpriteTicks([[maybe_unused]] FOClient* client, uint sprId)
{
    auto* const anim = client->AnimGetFrames(sprId);
    return anim ? anim->Ticks : 0;
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Global_GetSpritePixelColor([[maybe_unused]] FOClient* client, uint sprId, int frameIndex, int x, int y)
{
    if (sprId == 0u) {
        return 0;
    }

    auto* anim = client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return 0;
    }

    const auto spr_id_ = (frameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    return client->SprMngr.GetPixColor(spr_id_, x, y, false);
}

///# ...
///# param text ...
///# param w ...
///# param h ...
///# param font ...
///# param flags ...
///# param tw ...
///# param th ...
///# param lines ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_GetTextInfo([[maybe_unused]] FOClient* client, string_view text, int w, int h, int font, int flags, int& tw, int& th, int& lines)
{
    if (!client->SprMngr.GetTextInfo(w, h, text, font, flags, tw, th, lines)) {
        throw ScriptException("Can't evaluate text information", font);
    }
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# param x ...
///# param y ...
///# param color ...
///# param offs ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_DrawSprite([[maybe_unused]] FOClient* client, uint sprId, int frameIndex, int x, int y, uint color, bool offs)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0u) {
        return;
    }

    auto* anim = client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return;
    }

    auto xx = x;
    auto yy = y;

    const auto spr_id_ = (frameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (offs) {
        const auto* si = client->SprMngr.GetSpriteInfo(spr_id_);
        if (!si) {
            return;
        }

        xx += -si->Width / 2 + si->OffsX;
        yy += -si->Height + si->OffsY;
    }

    client->SprMngr.DrawSprite(spr_id_, xx, yy, COLOR_SCRIPT_SPRITE(color));
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///# param zoom ...
///# param color ...
///# param offs ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_DrawSpriteSized([[maybe_unused]] FOClient* client, uint sprId, int frameIndex, int x, int y, int w, int h, bool zoom, uint color, bool offs)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0u) {
        return;
    }

    auto* anim = client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return;
    }

    auto xx = x;
    auto yy = y;

    const auto spr_id_ = (frameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (offs) {
        const auto* si = client->SprMngr.GetSpriteInfo(spr_id_);
        if (!si) {
            return;
        }

        xx += si->OffsX;
        yy += si->OffsY;
    }

    client->SprMngr.DrawSpriteSizeExt(spr_id_, xx, yy, w, h, zoom, true, true, COLOR_SCRIPT_SPRITE(color));
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///# param sprWidth ...
///# param sprHeight ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_DrawSpritePattern([[maybe_unused]] FOClient* client, uint sprId, int frameIndex, int x, int y, int w, int h, int sprWidth, int sprHeight, uint color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0u) {
        return;
    }

    auto* anim = client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return;
    }

    client->SprMngr.DrawSpritePattern(frameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(frameIndex), x, y, w, h, sprWidth, sprHeight, COLOR_SCRIPT_SPRITE(color));
}

///# ...
///# param text ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///# param color ...
///# param font ...
///# param flags ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_DrawText([[maybe_unused]] FOClient* client, string_view text, int x, int y, int w, int h, uint color, int font, int flags)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (text.length() == 0) {
        return;
    }

    auto ww = w;
    auto hh = h;
    auto xx = x;
    auto yy = y;

    if (ww < 0) {
        ww = -ww;
        xx -= ww;
    }
    if (hh < 0) {
        hh = -hh;
        yy -= hh;
    }

    const auto r = IRect(xx, yy, xx + ww, yy + hh);
    client->SprMngr.DrawStr(r, text, flags, COLOR_SCRIPT_TEXT(color), font);
}

///# ...
///# param primitiveType ...
///# param data ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_DrawPrimitive([[maybe_unused]] FOClient* client, int primitiveType, const vector<int>& data)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (data.empty()) {
        return;
    }

    RenderPrimitiveType prim;
    switch (primitiveType) {
    case 0:
        prim = RenderPrimitiveType::PointList;
        break;
    case 1:
        prim = RenderPrimitiveType::LineList;
        break;
    case 2:
        prim = RenderPrimitiveType::LineStrip;
        break;
    case 3:
        prim = RenderPrimitiveType::TriangleList;
        break;
    case 4:
        prim = RenderPrimitiveType::TriangleStrip;
        break;
    case 5:
        prim = RenderPrimitiveType::TriangleFan;
        break;
    default:
        return;
    }

    PrimitivePoints points;
    const auto size = data.size() / 3;
    points.resize(size);

    for (size_t i = 0; i < size; i++) {
        auto& pp = points[i];
        pp.PointX = data[i * 3];
        pp.PointY = data[i * 3 + 1];
        pp.PointColor = data[i * 3 + 2];
        pp.PointOffsX = nullptr;
        pp.PointOffsY = nullptr;
    }

    client->SprMngr.DrawPoints(points, prim, nullptr, nullptr, nullptr);
}

///# ...
///# param mapSpr ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_DrawMapSprite([[maybe_unused]] FOClient* client, MapSprite* mapSpr)
{
    if (mapSpr == nullptr) {
        throw ScriptException("Map sprite arg is null");
    }

    if (!client->HexMngr.IsMapLoaded()) {
        return;
    }
    if (mapSpr->HexX >= client->HexMngr.GetWidth() || mapSpr->HexY >= client->HexMngr.GetHeight()) {
        return;
    }
    if (!client->HexMngr.IsHexToDraw(mapSpr->HexX, mapSpr->HexY)) {
        return;
    }

    auto* anim = client->AnimGetFrames(mapSpr->SprId);
    if (!anim || mapSpr->FrameIndex >= static_cast<int>(anim->CntFrm)) {
        return;
    }

    auto color = mapSpr->Color;
    auto is_flat = mapSpr->IsFlat;
    auto no_light = mapSpr->NoLight;
    auto draw_order = mapSpr->DrawOrder;
    auto draw_order_hy_offset = mapSpr->DrawOrderHyOffset;
    auto corner = mapSpr->Corner;
    auto disable_egg = mapSpr->DisableEgg;
    auto contour_color = mapSpr->ContourColor;

    if (mapSpr->ProtoId != 0u) {
        const auto* const proto_item = client->ProtoMngr.GetProtoItem(mapSpr->ProtoId);
        if (!proto_item) {
            return;
        }

        color = (proto_item->GetIsColorize() ? proto_item->GetLightColor() : 0);
        is_flat = proto_item->GetIsFlat();
        const auto is_item = !proto_item->IsAnyScenery();
        no_light = (is_flat && !is_item);
        draw_order = (is_flat ? (is_item ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY) : (is_item ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY));
        draw_order_hy_offset = proto_item->GetDrawOrderOffsetHexY();
        corner = proto_item->GetCorner();
        disable_egg = proto_item->GetDisableEgg();
        contour_color = (proto_item->GetIsBadItem() ? COLOR_RGB(255, 0, 0) : 0);
    }

    auto& f = client->HexMngr.GetField(mapSpr->HexX, mapSpr->HexY);
    auto& tree = client->HexMngr.GetDrawTree();
    auto& spr = tree.InsertSprite(draw_order, mapSpr->HexX, mapSpr->HexY + draw_order_hy_offset, (client->Settings.MapHexWidth / 2) + mapSpr->OffsX, (client->Settings.MapHexHeight / 2) + mapSpr->OffsY, &f.ScrX, &f.ScrY, mapSpr->FrameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(mapSpr->FrameIndex), nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsX : nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsY : nullptr, mapSpr->IsTweakAlpha ? &mapSpr->TweakAlpha : nullptr, nullptr, &mapSpr->Valid);

    spr.MapSpr = mapSpr;
    mapSpr->AddRef();

    if (!no_light) {
        spr.SetLight(corner, client->HexMngr.GetLightHex(0, 0), client->HexMngr.GetWidth(), client->HexMngr.GetHeight());
    }

    if (!is_flat && !disable_egg) {
        int egg_type;
        switch (corner) {
        case CORNER_SOUTH:
            egg_type = EGG_X_OR_Y;
            break;
        case CORNER_NORTH:
            egg_type = EGG_X_AND_Y;
            break;
        case CORNER_EAST_WEST:
        case CORNER_WEST:
            egg_type = EGG_Y;
            break;
        default:
            egg_type = EGG_X;
            break;
        }
        spr.SetEgg(egg_type);
    }

    if (color != 0u) {
        spr.SetColor(color & 0xFFFFFF);
        spr.SetFixedAlpha(color >> 24);
    }

    if (contour_color != 0u) {
        spr.SetContour(CONTOUR_CUSTOM, contour_color);
    }
}

///# ...
///# param modelName ...
///# param anim1 ...
///# param anim2 ...
///# param dir ...
///# param l ...
///# param t ...
///# param r ...
///# param b ...
///# param scratch ...
///# param center ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_DrawCritter2d([[maybe_unused]] FOClient* client, hash modelName, uint anim1, uint anim2, uchar dir, int l, int t, int r, int b, bool scratch, bool center, uint color)
{
    auto* anim = client->ResMngr.GetCritterAnim(modelName, anim1, anim2, dir);
    if (anim) {
        client->SprMngr.DrawSpriteSize(anim->Ind[0], l, t, r - l, b - t, scratch, center, COLOR_SCRIPT_SPRITE(color));
    }
}

///# ...
///# param instance ...
///# param modelName ...
///# param anim1 ...
///# param anim2 ...
///# param layers ...
///# param position ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_DrawCritter3d([[maybe_unused]] FOClient* client, uint instance, hash modelName, uint anim1, uint anim2, const vector<int>& layers, const vector<float>& position, uint color)
{
    // x y
    // rx ry rz
    // sx sy sz
    // speed
    // scissor l t r b
    if (instance >= client->DrawCritterModel.size()) {
        client->DrawCritterModel.resize(instance + 1);
        client->DrawCritterModelCrType.resize(instance + 1);
        client->DrawCritterModelFailedToLoad.resize(instance + 1);
    }

    if (client->DrawCritterModelFailedToLoad[instance] && client->DrawCritterModelCrType[instance] == modelName) {
        return;
    }

    auto& model = client->DrawCritterModel[instance];
    if (model == nullptr || client->DrawCritterModelCrType[instance] != modelName) {
        if (model != nullptr) {
            client->SprMngr.FreeModel(model);
        }

        client->SprMngr.PushAtlasType(AtlasType::Dynamic);
        model = client->SprMngr.LoadModel(_str().parseHash(modelName), false);
        client->SprMngr.PopAtlasType();
        client->DrawCritterModelCrType[instance] = modelName;
        client->DrawCritterModelFailedToLoad[instance] = false;

        if (model == nullptr) {
            client->DrawCritterModelFailedToLoad[instance] = true;
            return;
        }

        model->EnableShadow(false);
        model->SetTimer(false);
    }

    const auto count = static_cast<uint>(position.size());
    const auto x = (count > 0 ? position[0] : 0.0f);
    const auto y = (count > 1 ? position[1] : 0.0f);
    const auto rx = (count > 2 ? position[2] : 0.0f);
    const auto ry = (count > 3 ? position[3] : 0.0f);
    const auto rz = (count > 4 ? position[4] : 0.0f);
    const auto sx = (count > 5 ? position[5] : 1.0f);
    const auto sy = (count > 6 ? position[6] : 1.0f);
    const auto sz = (count > 7 ? position[7] : 1.0f);
    const auto speed = (count > 8 ? position[8] : 1.0f);
    const auto period = (count > 9 ? position[9] : 0.0f);
    const auto stl = (count > 10 ? position[10] : 0.0f);
    const auto stt = (count > 11 ? position[11] : 0.0f);
    const auto str = (count > 12 ? position[12] : 0.0f);
    const auto stb = (count > 13 ? position[13] : 0.0f);
    if (count > 13) {
        client->SprMngr.PushScissor(static_cast<int>(stl), static_cast<int>(stt), static_cast<int>(str), static_cast<int>(stb));
    }

    std::memset(client->DrawCritterModelLayers, 0, sizeof(client->DrawCritterModelLayers));
    for (uint i = 0, j = static_cast<uint>(layers.size()); i < j && i < LAYERS3D_COUNT; i++) {
        client->DrawCritterModelLayers[i] = layers[i];
    }

    model->SetDirAngle(0);
    model->SetRotation(rx * PI_FLOAT / 180.0f, ry * PI_FLOAT / 180.0f, rz * PI_FLOAT / 180.0f);
    model->SetScale(sx, sy, sz);
    model->SetSpeed(speed);
    model->SetAnimation(anim1, anim2, client->DrawCritterModelLayers, ANIMATION_PERIOD(static_cast<int>(period * 100.0f)) | ANIMATION_NO_SMOOTH);

    client->SprMngr.Draw3d(static_cast<int>(x), static_cast<int>(y), model, COLOR_SCRIPT_SPRITE(color));

    if (count > 13) {
        client->SprMngr.PopScissor();
    }
}

///# ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_PushDrawScissor([[maybe_unused]] FOClient* client, int x, int y, int w, int h)
{
    client->SprMngr.PushScissor(x, y, x + w, y + h);
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_PopDrawScissor([[maybe_unused]] FOClient* client)
{
    client->SprMngr.PopScissor();
}

///# ...
///# param forceClear ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_ActivateOffscreenSurface([[maybe_unused]] FOClient* client, bool forceClear)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (client->OffscreenSurfaces.empty()) {
        auto* const rt = client->SprMngr.CreateRenderTarget(false, true, 0, 0, false);
        if (!rt) {
            throw ScriptException("Can't create offscreen surface");
        }

        client->OffscreenSurfaces.push_back(rt);
    }

    auto* const rt = client->OffscreenSurfaces.back();
    client->OffscreenSurfaces.pop_back();
    client->ActiveOffscreenSurfaces.push_back(rt);

    client->SprMngr.PushRenderTarget(rt);

    const auto it = std::find(client->DirtyOffscreenSurfaces.begin(), client->DirtyOffscreenSurfaces.end(), rt);
    if (it != client->DirtyOffscreenSurfaces.end() || forceClear) {
        if (it != client->DirtyOffscreenSurfaces.end()) {
            client->DirtyOffscreenSurfaces.erase(it);
        }

        client->SprMngr.ClearCurrentRenderTarget(0);
    }

    if (std::find(client->PreDirtyOffscreenSurfaces.begin(), client->PreDirtyOffscreenSurfaces.end(), rt) == client->PreDirtyOffscreenSurfaces.end()) {
        client->PreDirtyOffscreenSurfaces.push_back(rt);
    }
}

///# ...
///# param effectSubtype ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_PresentOffscreenSurface([[maybe_unused]] FOClient* client, int effectSubtype)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (client->ActiveOffscreenSurfaces.empty()) {
        throw ScriptException("No active offscreen surfaces");
    }

    auto* const rt = client->ActiveOffscreenSurfaces.back();
    client->ActiveOffscreenSurfaces.pop_back();
    client->OffscreenSurfaces.push_back(rt);

    client->SprMngr.PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= static_cast<int>(client->OffscreenEffects.size()) || !client->OffscreenEffects[effectSubtype]) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->DrawEffect = client->OffscreenEffects[effectSubtype];

    client->SprMngr.DrawRenderTarget(rt, true, nullptr, nullptr);
}

///# ...
///# param effectSubtype ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_PresentOffscreenSurfaceExt([[maybe_unused]] FOClient* client, int effectSubtype, int x, int y, int w, int h)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (client->ActiveOffscreenSurfaces.empty()) {
        throw ScriptException("No active offscreen surfaces");
    }

    auto* const rt = client->ActiveOffscreenSurfaces.back();
    client->ActiveOffscreenSurfaces.pop_back();
    client->OffscreenSurfaces.push_back(rt);

    client->SprMngr.PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= static_cast<int>(client->OffscreenEffects.size()) || !client->OffscreenEffects[effectSubtype]) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->DrawEffect = client->OffscreenEffects[effectSubtype];

    IRect from(std::clamp(x, 0, client->Settings.ScreenWidth), std::clamp(y, 0, client->Settings.ScreenHeight), std::clamp(x + w, 0, client->Settings.ScreenWidth), std::clamp(y + h, 0, client->Settings.ScreenHeight));
    auto to = from;
    client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}

///# ...
///# param effectSubtype ...
///# param fromX ...
///# param fromY ...
///# param fromW ...
///# param fromH ...
///# param toX ...
///# param toY ...
///# param toW ...
///# param toH ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_PresentOffscreenSurfaceExt2([[maybe_unused]] FOClient* client, int effectSubtype, int fromX, int fromY, int fromW, int fromH, int toX, int toY, int toW, int toH)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (client->ActiveOffscreenSurfaces.empty()) {
        throw ScriptException("No active offscreen surfaces");
    }

    auto* const rt = client->ActiveOffscreenSurfaces.back();
    client->ActiveOffscreenSurfaces.pop_back();
    client->OffscreenSurfaces.push_back(rt);

    client->SprMngr.PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= static_cast<int>(client->OffscreenEffects.size()) || !client->OffscreenEffects[effectSubtype]) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->DrawEffect = client->OffscreenEffects[effectSubtype];

    IRect from(std::clamp(fromX, 0, client->Settings.ScreenWidth), std::clamp(fromY, 0, client->Settings.ScreenHeight), std::clamp(fromX + fromW, 0, client->Settings.ScreenWidth), std::clamp(fromY + fromH, 0, client->Settings.ScreenHeight));
    IRect to(std::clamp(toX, 0, client->Settings.ScreenWidth), std::clamp(toY, 0, client->Settings.ScreenHeight), std::clamp(toX + toW, 0, client->Settings.ScreenWidth), std::clamp(toY + toH, 0, client->Settings.ScreenHeight));
    client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}

///# ...
///# param screen ...
///# param data ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_ShowScreen([[maybe_unused]] FOClient* client, int screen, const map<string, int>& data)
{
    if (screen >= SCREEN_LOGIN && screen <= SCREEN_WAIT) {
        client->ShowMainScreen(screen, data);
    }
    else if (screen != SCREEN_NONE) {
        client->ShowScreen(screen, data);
    }
    else {
        client->HideScreen(screen);
    }
}

///# ...
///# param screen ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_HideScreen([[maybe_unused]] FOClient* client, int screen)
{
    client->HideScreen(screen);
}

///# ...
///# param hx ...
///# param hy ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Global_GetHexMonitorPos([[maybe_unused]] FOClient* client, ushort hx, ushort hy, int& x, int& y)
{
    x = y = 0;
    if (client->HexMngr.IsMapLoaded() && hx < client->HexMngr.GetWidth() && hy < client->HexMngr.GetHeight()) {
        client->HexMngr.GetHexCurrentPosition(hx, hy, x, y);
        x += client->Settings.ScrOx + (client->Settings.MapHexWidth / 2);
        y += client->Settings.ScrOy + (client->Settings.MapHexHeight / 2);
        x = static_cast<int>(static_cast<float>(x) / client->Settings.SpritesZoom);
        y = static_cast<int>(static_cast<float>(y) / client->Settings.SpritesZoom);
        return true;
    }
    return false;
}

///# ...
///# param x ...
///# param y ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Global_GetHexByMonitorPos([[maybe_unused]] FOClient* client, int x, int y, ushort& hx, ushort& hy)
{
    const auto old_x = client->Settings.MouseX;
    const auto old_y = client->Settings.MouseY;
    client->Settings.MouseX = x;
    client->Settings.MouseY = y;
    ushort hx_ = 0;
    ushort hy_ = 0;
    const auto result = client->HexMngr.GetHexPixel(x, y, hx_, hy_);
    client->Settings.MouseX = old_x;
    client->Settings.MouseY = old_y;
    if (result) {
        hx = hx_;
        hy = hy_;
        return true;
    }
    return false;
}

///# ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Client_Global_GetItemByMonitorPos([[maybe_unused]] FOClient* client, int x, int y)
{
    bool item_egg;
    return client->HexMngr.GetItemPixel(x, y, item_egg);
}

///# ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Client_Global_GetCritterByMonitorPos([[maybe_unused]] FOClient* client, int x, int y)
{
    return client->HexMngr.GetCritterPixel(x, y, false);
}

///# ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Entity* Client_Global_GetEntityByMonitorPos([[maybe_unused]] FOClient* client, int x, int y)
{
    ItemHexView* item = nullptr;
    CritterView* cr = nullptr;
    client->HexMngr.GetSmthPixel(x, y, item, cr);
    return item ? static_cast<Entity*>(item) : static_cast<Entity*>(cr);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ushort Client_Global_GetMapWidth([[maybe_unused]] FOClient* client)
{
    if (!client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }

    return client->HexMngr.GetWidth();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ushort Client_Global_GetMapHeight([[maybe_unused]] FOClient* client)
{
    if (!client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }

    return client->HexMngr.GetHeight();
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Global_IsMapHexPassed([[maybe_unused]] FOClient* client, ushort hx, ushort hy)
{
    if (!client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }
    if (hx >= client->HexMngr.GetWidth() || hy >= client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    return !client->HexMngr.GetField(hx, hy).Flags.IsNotPassed;
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Global_IsMapHexRaked([[maybe_unused]] FOClient* client, ushort hx, ushort hy)
{
    if (!client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }
    if (hx >= client->HexMngr.GetWidth() || hy >= client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    return !client->HexMngr.GetField(hx, hy).Flags.IsNotRaked;
}

///# ...
///# param filePath ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_SaveScreenshot([[maybe_unused]] FOClient* client, string_view filePath)
{
    // client->SprMngr.SaveTexture(nullptr, _str(filePath).formatPath(), true);
}

///# ...
///# param filePath ...
///# param text ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_SaveText([[maybe_unused]] FOClient* client, string_view filePath, string_view text)
{
    auto f = DiskFileSystem::OpenFile(_str(filePath).formatPath(), true);
    if (!f) {
        throw ScriptException("Can't open file for writing", filePath);
    }

    if (text.length() > 0 && !f.Write(text.data(), static_cast<uint>(text.length()))) {
        throw ScriptException("Can't write file", filePath, text.length());
    }
}

///# ...
///# param name ...
///# param data ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_SetCacheData([[maybe_unused]] FOClient* client, string_view name, const vector<uchar>& data)
{
    client->Cache.SetData(name, data);
}

///# ...
///# param name ...
///# param data ...
///# param dataSize ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_SetCacheDataExt([[maybe_unused]] FOClient* client, string_view name, const vector<uchar>& data, uint dataSize)
{
    auto data_copy = data;
    data_copy.resize(dataSize);
    client->Cache.SetData(name, data_copy);
}

///# ...
///# param name ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<uchar> Client_Global_GetCacheData([[maybe_unused]] FOClient* client, string_view name)
{
    return client->Cache.GetData(name);
}

///# ...
///# param name ...
///# param str ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_SetCacheText([[maybe_unused]] FOClient* client, string_view name, string_view str)
{
    client->Cache.SetString(name, str);
}

///# ...
///# param name ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Global_GetCacheText([[maybe_unused]] FOClient* client, string_view name)
{
    return client->Cache.GetString(name);
}

///# ...
///# param name ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Global_IsCacheEntry([[maybe_unused]] FOClient* client, string_view name)
{
    return client->Cache.HasEntry(name);
}

///# ...
///# param name ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_RemoveCacheEntry([[maybe_unused]] FOClient* client, string_view name)
{
    client->Cache.EraseEntry(name);
}

///# ...
///# param keyValues ...
///@ ExportMethod
[[maybe_unused]] void Client_Global_SetUserConfig([[maybe_unused]] FOClient* client, const map<string, string>& keyValues)
{
    auto cfg_user = client->FileMngr.WriteFile(CONFIG_NAME, false);
    for (const auto& [key, value] : keyValues) {
        cfg_user.SetStr(_str("{} = {}\n", key, value));
    }
    cfg_user.Save();
}
