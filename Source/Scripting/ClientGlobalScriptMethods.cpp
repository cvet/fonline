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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "DiskFileSystem.h"
#include "FileSystem.h"
#include "GenericUtils.h"
#include "GeometryHelper.h"
#include "Log.h"
#include "NetCommand.h"
#include "ProtoManager.h"
#include "StringUtils.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_IsConnecting(FOClient* client)
{
    return client->IsConnecting();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_IsConnected(FOClient* client)
{
    return client->IsConnected();
}

///@ ExportMethod
[[maybe_unused]] uint Client_Game_DeferredCall(FOClient* client, uint delay, ScriptFuncName<void> func)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, func, nullptr, nullptr, nullptr, nullptr);
}

///@ ExportMethod
[[maybe_unused]] uint Client_Game_DeferredCall(FOClient* client, uint delay, ScriptFuncName<void, int> func, int value)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, func, &value, nullptr, nullptr, nullptr);
}

///@ ExportMethod
[[maybe_unused]] uint Client_Game_DeferredCall(FOClient* client, uint delay, ScriptFuncName<void, uint> func, uint value)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, func, nullptr, nullptr, &value, nullptr);
}

///@ ExportMethod
[[maybe_unused]] uint Client_Game_DeferredCall(FOClient* client, uint delay, ScriptFuncName<void, vector<int>> func, const vector<int>& values)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, func, nullptr, &values, nullptr, nullptr);
}

///@ ExportMethod
[[maybe_unused]] uint Client_Game_DeferredCall(FOClient* client, uint delay, ScriptFuncName<void, vector<uint>> func, const vector<uint>& values)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, func, nullptr, nullptr, nullptr, &values);
}

///@ ExportMethod
[[maybe_unused]] bool Client_Game_IsDeferredCallPending(FOClient* client, uint id)
{
    return client->ClientDeferredCalls.IsDeferredCallPending(id);
}

///@ ExportMethod
[[maybe_unused]] bool Client_Game_CancelDeferredCall(FOClient* client, uint id)
{
    return client->ClientDeferredCalls.CancelDeferredCall(id);
}

///# ...
///# param pid ...
///# param props ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ProtoItem* Client_Game_GetProtoItem(FOClient* client, hstring pid)
{
    return const_cast<ProtoItem*>(client->ProtoMngr.GetProtoItem(pid));
}

///# ...
///# param cr1 ...
///# param cr2 ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Game_GetDistance(FOClient* client, CritterView* cr1, CritterView* cr2)
{
    if (cr1 == nullptr) {
        throw ScriptException("Critter1 arg is null");
    }
    if (cr2 == nullptr) {
        throw ScriptException("Critter2 arg is null");
    }
    if (cr1->GetMapId() != cr2->GetMapId()) {
        throw ScriptException("Critters different maps");
    }

    return client->Geometry.DistGame(cr1->GetHexX(), cr1->GetHexY(), cr2->GetHexX(), cr2->GetHexY());
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_GetTick(FOClient* client)
{
    return client->GameTime.FrameTick();
}

///# ...
///# param command ...
///# param separator ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Game_CustomCall(FOClient* client, string_view command)
{
    return client->CustomCall(command, " ");
}

///# ...
///# param command ...
///# param separator ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Game_CustomCall(FOClient* client, string_view command, string_view separator)
{
    return client->CustomCall(command, separator);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Client_Game_GetChosen(FOClient* client)
{
    if (client->GetChosen() && client->GetChosen()->IsDestroyed()) {
        return nullptr;
    }
    return client->GetChosen();
}

///# ...
///# param itemId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Game_GetItem(FOClient* client, uint itemId)
{
    if (itemId == 0u) {
        throw ScriptException("Item id arg is zero");
    }

    // On map
    ItemView* item = client->CurMap->GetItem(itemId);

    // On Chosen
    if (item == nullptr && client->GetChosen() != nullptr) {
        item = client->GetChosen()->GetItem(itemId);
    }

    // On other critters
    if (item == nullptr) {
        for (auto* cr : client->CurMap->GetCritters()) {
            if (!cr->IsChosen()) {
                item = cr->GetItem(itemId);
            }
        }
    }

    if (item == nullptr || item->IsDestroyed()) {
        return static_cast<ItemView*>(nullptr);
    }
    return item;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ItemView*> Client_Game_GetVisibleItems(FOClient* client)
{
    vector<ItemView*> items;
    if (client->CurMap != nullptr) {
        const auto items_ = client->CurMap->GetItems();
        items.reserve(items_.size());
        for (auto* item : items_) {
            if (!item->IsFinishing()) {
                items.emplace_back(item);
            }
        }
    }
    return items;
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ItemView*> Client_Game_GetVisibleItemsOnHex(FOClient* client, ushort hx, ushort hy)
{
    vector<ItemHexView*> hex_items;
    if (client->CurMap != nullptr) {
        hex_items = client->CurMap->GetItems(hx, hy);
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
///# param critterId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] CritterView* Client_Game_GetCritter(FOClient* client, uint critterId)
{
    if (critterId == 0u) {
        return nullptr;
    }

    auto* cr = client->CurMap->GetCritter(critterId);
    if (!cr || cr->IsDestroyed() || cr->IsDestroying()) {
        return nullptr;
    }

    return cr;
}

///# ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Game_GetCritters(FOClient* client, CritterFindType findType)
{
    vector<CritterView*> critters;

    for (auto* cr : client->CurMap->GetCritters()) {
        if (cr->CheckFind(findType)) {
            critters.push_back(cr);
        }
    }

    return critters;
}

///# ...
///# param pid ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Game_GetCritters(FOClient* client, hstring pid, CritterFindType findType)
{
    vector<CritterView*> critters;

    if (!pid) {
        for (auto* cr : client->CurMap->GetCritters()) {
            if (cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }
    else {
        for (auto* cr : client->CurMap->GetCritters()) {
            if (cr->IsNpc() && cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }

    return critters;
}

///# ...
///# param hx ...
///# param hy ...
///# param radius ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Game_GetCritters(FOClient* client, ushort hx, ushort hy, uint radius, CritterFindType findType)
{
    if (hx >= client->CurMap->GetWidth() || hy >= client->CurMap->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    vector<CritterView*> critters;

    for (auto* cr : client->CurMap->GetCritters()) {
        if (cr->CheckFind(findType) && client->Geometry.CheckDist(hx, hy, cr->GetHexX(), cr->GetHexY(), radius)) {
            critters.push_back(cr);
        }
    }

    std::sort(critters.begin(), critters.end(), [client, &hx, &hy](CritterView* cr1, CritterView* cr2) { return client->Geometry.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY()) < client->Geometry.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY()); });

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
[[maybe_unused]] vector<CritterView*> Client_Game_GetCrittersInPath(FOClient* client, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, float angle, uint dist, CritterFindType findType)
{
    vector<CritterHexView*> critters;
    client->CurMap->TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, &critters, findType, nullptr, nullptr, nullptr, true);
    return vec_downcast<CritterView*>(critters);
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
[[maybe_unused]] vector<CritterView*> Client_Game_GetCrittersWithBlockInPath(FOClient* client, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, float angle, uint dist, CritterFindType findType, ushort& preBlockHx, ushort& preBlockHy, ushort& blockHx, ushort& blockHy)
{
    vector<CritterHexView*> critters;
    pair<ushort, ushort> block = {};
    pair<ushort, ushort> pre_block = {};
    client->CurMap->TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, &critters, findType, &block, &pre_block, nullptr, true);
    preBlockHx = pre_block.first;
    preBlockHy = pre_block.second;
    blockHx = block.first;
    blockHy = block.second;
    return vec_downcast<CritterView*>(critters);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param angle ...
///# param dist ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Client_Game_GetHexInPath(FOClient* client, ushort fromHx, ushort fromHy, ushort& toHx, ushort& toHy, float angle, uint dist)
{
    pair<ushort, ushort> pre_block = {};
    pair<ushort, ushort> block = {};
    client->CurMap->TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, nullptr, CritterFindType::Any, &block, &pre_block, nullptr, true);
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
[[maybe_unused]] vector<uchar> Client_Game_GetPath(FOClient* client, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, uint cut)
{
    if (fromHx >= client->CurMap->GetWidth() || fromHy >= client->CurMap->GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= client->CurMap->GetWidth() || toHy >= client->CurMap->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (client->Geometry.DistGame(fromHx, fromHy, to_hx, to_hy) <= 1) {
        if (client->Geometry.DistGame(fromHx, fromHy, to_hx, to_hy) > 0 && cut == 0) {
            return {client->Geometry.GetFarDir(fromHx, fromHy, to_hx, to_hy)};
        }
        return {};
    }

    const auto init_to_hx = to_hx;
    const auto init_to_hy = to_hy;

    if (cut > 0 && !client->CurMap->CutPath(nullptr, fromHx, fromHy, to_hx, to_hy, cut)) {
        return {};
    }

    if (cut > 0 && client->Geometry.DistGame(fromHx, fromHy, init_to_hx, init_to_hy) <= cut && client->Geometry.DistGame(fromHx, fromHy, to_hx, to_hy) <= 1) {
        return {};
    }

    auto result = client->CurMap->FindPath(nullptr, fromHx, fromHy, to_hx, to_hy, -1);
    if (!result) {
        return {};
    }

    return result->Steps;
}

///# ...
///# param cr ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<uchar> Client_Game_GetPath(FOClient* client, CritterView* cr, ushort toHx, ushort toHy, uint cut)
{
    if (toHx >= client->CurMap->GetWidth() || toHy >= client->CurMap->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (client->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) <= 1 + cr->GetMultihex()) {
        if (client->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) > cr->GetMultihex() && cut == 0) {
            return {client->Geometry.GetFarDir(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy)};
        }
        return {};
    }

    const auto init_to_hx = to_hx;
    const auto init_to_hy = to_hy;

    if (cut > 0 && !client->CurMap->CutPath(hex_cr, hex_cr->GetHexX(), hex_cr->GetHexY(), to_hx, to_hy, cut)) {
        return {};
    }

    if (cut > 0 && client->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), init_to_hx, init_to_hy) <= cut + cr->GetMultihex() && client->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) <= 1 + cr->GetMultihex()) {
        return {};
    }

    auto result = client->CurMap->FindPath(hex_cr, hex_cr->GetHexX(), hex_cr->GetHexY(), to_hx, to_hy, -1);
    if (!result) {
        return {};
    }

    return result->Steps;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Game_GetPathLength(FOClient* client, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, uint cut)
{
    if (fromHx >= client->CurMap->GetWidth() || fromHy >= client->CurMap->GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= client->CurMap->GetWidth() || toHy >= client->CurMap->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (client->Geometry.DistGame(fromHx, fromHy, to_hx, to_hy) <= 1) {
        return cut > 0 ? 0 : 1;
    }

    const auto init_to_hx = to_hx;
    const auto init_to_hy = to_hy;

    if (cut > 0 && !client->CurMap->CutPath(nullptr, fromHx, fromHy, to_hx, to_hy, cut)) {
        return 0;
    }

    if (cut > 0 && client->Geometry.DistGame(fromHx, fromHy, init_to_hx, init_to_hy) <= cut && client->Geometry.DistGame(fromHx, fromHy, to_hx, to_hy) <= 1) {
        return 0;
    }

    const auto result = client->CurMap->FindPath(nullptr, fromHx, fromHy, to_hx, to_hy, -1);
    if (!result) {
        return 0;
    }

    return static_cast<uint>(result->Steps.size());
}

///# ...
///# param cr ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Game_GetPathLength(FOClient* client, CritterView* cr, ushort toHx, ushort toHy, uint cut)
{
    if (toHx >= client->CurMap->GetWidth() || toHy >= client->CurMap->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (client->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) <= 1 + cr->GetMultihex()) {
        return cut > 0 ? 0 : 1;
    }

    const auto init_to_hx = to_hx;
    const auto init_to_hy = to_hy;

    if (cut > 0 && !client->CurMap->CutPath(hex_cr, hex_cr->GetHexX(), hex_cr->GetHexY(), to_hx, to_hy, cut)) {
        return 0;
    }

    if (cut > 0 && client->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), init_to_hx, init_to_hy) <= cut + cr->GetMultihex() && client->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) <= 1 + cr->GetMultihex()) {
        return 0;
    }

    const auto result = client->CurMap->FindPath(hex_cr, hex_cr->GetHexX(), hex_cr->GetHexY(), to_hx, to_hy, -1);
    if (!result) {
        return 0;
    }

    return static_cast<uint>(result->Steps.size());
}

///# ...
///# param fromColor ...
///# param toColor ...
///# param ms ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_FlushScreen(FOClient* client, uint fromColor, uint toColor, uint ms)
{
    client->ScreenFade(ms, fromColor, toColor, true);
}

///# ...
///# param noise ...
///# param ms ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_QuakeScreen(FOClient* client, uint noise, uint ms)
{
    client->ScreenQuake(noise, ms);
}

///# ...
///# param soundName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_PlaySound(FOClient* client, string_view soundName)
{
    return client->SndMngr.PlaySound(client->ResMngr.GetSoundNames(), soundName);
}

///# ...
///# param musicName ...
///# param repeatTime ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_PlayMusic(FOClient* client, string_view musicName, uint repeatTime)
{
    if (musicName.empty()) {
        client->SndMngr.StopMusic();
        return true;
    }

    return client->SndMngr.PlayMusic(musicName, repeatTime);
}

///# ...
///# param videoName ...
///# param canStop ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_PlayVideo(FOClient* client, string_view videoName, bool canStop)
{
    // client->SndMngr.StopMusic();
    // client->AddVideo(videoName, canStop, true);
    return false;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] hstring Client_Game_GetCurMapPid(FOClient* client)
{
    if (client->CurMap == nullptr) {
        return {};
    }
    return client->CurMapPid;
}

///# ...
///# param msg ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_Message(FOClient* client, string_view msg)
{
    client->AddMessage(0, msg);
}

///# ...
///# param type ...
///# param msg ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_Message(FOClient* client, int type, string_view msg)
{
    client->AddMessage(static_cast<uchar>(type), msg);
}

///# ...
///# param textMsg ...
///# param strNum ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_Message(FOClient* client, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    client->AddMessage(0, client->GetCurLang().Msg[textMsg].GetStr(strNum));
}

///# ...
///# param type ...
///# param textMsg ...
///# param strNum ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_Message(FOClient* client, int type, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    client->AddMessage(static_cast<uchar>(type), client->GetCurLang().Msg[textMsg].GetStr(strNum));
}

///# ...
///# param text ...
///# param hx ...
///# param hy ...
///# param showTime ...
///# param color ...
///# param fade ...
///# param endOx ...
///# param endOy ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_MapMessage(FOClient* client, string_view text, ushort hx, ushort hy, uint showTime, uint color, bool fade, int endOx, int endOy)
{
    client->CurMap->AddMapText(text, hx, hy, color, showTime, fade, endOx, endOy);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Game_GetMsgStr(FOClient* client, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    return client->GetCurLang().Msg[textMsg].GetStr(strNum);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# param skipCount ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Game_GetMsgStr(FOClient* client, int textMsg, uint strNum, uint skipCount)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    return client->GetCurLang().Msg[textMsg].GetStr(strNum, skipCount);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_GetMsgStrNumUpper(FOClient* client, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    return client->GetCurLang().Msg[textMsg].GetStrNumUpper(strNum);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_GetMsgStrNumLower(FOClient* client, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    return client->GetCurLang().Msg[textMsg].GetStrNumLower(strNum);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_GetMsgStrCount(FOClient* client, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    return client->GetCurLang().Msg[textMsg].Count(strNum);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_IsMsgStr(FOClient* client, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    return client->GetCurLang().Msg[textMsg].Count(strNum) > 0;
}

///# ...
///# param text ...
///# param replace ...
///# param str ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Game_ReplaceText(FOClient* client, string_view text, string_view replace, ObjInfo<1> obj1)
{
    const auto pos = text.find(replace, 0);
    if (pos == std::string::npos) {
        return string(text);
    }
    return string(text).replace(pos, replace.length(), obj1);
}

///# ...
///# param text ...
///# param lexems ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Game_FormatTags(FOClient* client, string_view text, string_view lexems)
{
    auto text_copy = string(text);
    client->FormatTags(text_copy, client->GetChosen(), nullptr, lexems);
    return text_copy;
}

///# ...
///# param hx ...
///# param hy ...
///# param speed ...
///# param canStop ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_MoveScreenToHex(FOClient* client, ushort hx, ushort hy, uint speed, bool canStop)
{
    if (client->CurMap == nullptr) {
        throw ScriptException("Map is not loaded");
    }
    if (hx >= client->CurMap->GetWidth() || hy >= client->CurMap->GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    if (speed == 0u) {
        client->CurMap->FindSetCenter(hx, hy);
    }
    else {
        client->CurMap->ScrollToHex(hx, hy, static_cast<float>(speed) / 1000.0f, canStop);
    }
}

///# ...
///# param ox ...
///# param oy ...
///# param speed ...
///# param canStop ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_MoveScreenOffset(FOClient* client, int ox, int oy, uint speed, bool canStop)
{
    if (client->CurMap == nullptr) {
        throw ScriptException("Map is not loaded");
    }

    client->CurMap->ScrollOffset(ox, oy, static_cast<float>(speed) / 1000.0f, canStop);
}

///# ...
///# param cr ...
///# param softLock ...
///# param unlockIfSame ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_LockScreenScroll(FOClient* client, CritterView* cr, bool softLock, bool unlockIfSame)
{
    const auto id = (cr != nullptr ? cr->GetId() : 0);
    if (softLock) {
        if (unlockIfSame && id == client->CurMap->AutoScroll.SoftLockedCritter) {
            client->CurMap->AutoScroll.SoftLockedCritter = 0;
        }
        else {
            client->CurMap->AutoScroll.SoftLockedCritter = id;
        }

        client->CurMap->AutoScroll.CritterLastHexX = (cr ? cr->GetHexX() : 0);
        client->CurMap->AutoScroll.CritterLastHexY = (cr ? cr->GetHexY() : 0);
    }
    else {
        if (unlockIfSame && id == client->CurMap->AutoScroll.HardLockedCritter) {
            client->CurMap->AutoScroll.HardLockedCritter = 0;
        }
        else {
            client->CurMap->AutoScroll.HardLockedCritter = id;
        }
    }
}

///# ...
///# param zoneX ...
///# param zoneY ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] int Client_Game_GetFog(FOClient* client, ushort zoneX, ushort zoneY)
{
    if (zoneX >= client->Settings.GlobalMapWidth || zoneY >= client->Settings.GlobalMapHeight) {
        throw ScriptException("Invalid world map pos arg");
    }
    return client->GetWorldmapFog().Get2Bit(zoneX, zoneY);
}

///# ...
///# param dayPart ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_GetDayTime(FOClient* client, uint dayPart)
{
    if (dayPart >= 4) {
        throw ScriptException("Invalid day part arg");
    }

    if (client->CurMap != nullptr) {
        return client->CurMap->GetMapDayTime()[dayPart];
    }
    return 0;
}

///# ...
///# param dayPart ...
///# param r ...
///# param g ...
///# param b ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_GetDayColor(FOClient* client, uint dayPart, uchar& r, uchar& g, uchar& b)
{
    r = g = b = 0;
    if (dayPart >= 4) {
        throw ScriptException("Invalid day part arg");
    }

    if (client->CurMap != nullptr) {
        auto* const col = client->CurMap->GetMapDayColor();
        r = col[0 + dayPart];
        g = col[4 + dayPart];
        b = col[8 + dayPart];
    }
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Game_GetFullSecond(FOClient* client)
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
[[maybe_unused]] uint Client_Game_EvaluateFullSecond(FOClient* client, ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second)
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
        year_ = client->GetYear();
    }
    if (month_ == 0u) {
        month_ = client->GetMonth();
    }

    if (day_ != 0u) {
        const auto month_day = client->GameTime.GameTimeMonthDay(year, month_);
        if (day_ < month_day || day_ > month_day) {
            throw ScriptException("Invalid day", day_, month_day);
        }
    }

    if (day_ == 0u) {
        day_ = client->GetDay();
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
[[maybe_unused]] void Client_Game_GetGameTime(FOClient* client, uint fullSecond, ushort& year, ushort& month, ushort& day, ushort& dayOfWeek, ushort& hour, ushort& minute, ushort& second)
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
[[maybe_unused]] bool Client_Game_MoveHexByDir(FOClient* client, ushort& hx, ushort& hy, uchar dir, uint steps)
{
    if (client->CurMap == nullptr) {
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
            result |= client->Geometry.MoveHexByDir(hx_, hy_, dir, client->CurMap->GetWidth(), client->CurMap->GetHeight());
        }
    }
    else {
        result = client->Geometry.MoveHexByDir(hx_, hy_, dir, client->CurMap->GetWidth(), client->CurMap->GetHeight());
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
[[maybe_unused]] hstring Client_Game_GetTileName(FOClient* client, ushort hx, ushort hy, bool roof, int layer)
{
    if (client->CurMap == nullptr) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= client->CurMap->GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= client->CurMap->GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    const auto* simply_tile = client->CurMap->GetField(hx, hy).SimplyTile[roof ? 1 : 0];
    if (simply_tile != nullptr && layer == 0) {
        return simply_tile->Name;
    }

    const auto* tiles = client->CurMap->GetField(hx, hy).Tiles[roof ? 1 : 0];
    if (tiles == nullptr || tiles->empty()) {
        return {};
    }

    for (const auto& tile : *tiles) {
        if (tile.Layer == layer) {
            return tile.Anim->Name;
        }
    }
    return {};
}

///# ...
///# param fnames ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_Preload3dFiles(FOClient* client, const vector<string>& fnames)
{
    for (size_t i = 0; i < fnames.size(); i++) {
        client->Preload3dFiles.push_back(fnames[i]);
    }
}

///# ...
///# param fontIndex ...
///# param fontFname ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_LoadFont(FOClient* client, int fontIndex, string_view fontFname)
{
    client->SprMngr.PushAtlasType(AtlasType::Static);

    bool result;
    if (!fontFname.empty() && fontFname[0] == '*') {
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
[[maybe_unused]] void Client_Game_SetDefaultFont(FOClient* client, int font, uint color)
{
    client->SprMngr.SetDefaultFont(font, color);
}

///# ...
///# param effectType ...
///# param effectSubtype ...
///# param effectName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SetEffect(FOClient* client, EffectType effectType, int effectSubtype, string_view effectName)
{
    const auto reload_effect = [&](RenderEffect* def_effect) {
        if (!effectName.empty()) {
            auto* effect = client->EffectMngr.LoadEffect(def_effect->Usage, effectName, "");
            if (!effect) {
                throw ScriptException("Effect not found or have some errors, see log file");
            }
            return effect;
        }
        return def_effect;
    };

    const auto eff_type = static_cast<uint>(effectType);

    if ((eff_type & static_cast<uint>(EffectType::GenericSprite)) && effectSubtype != 0) {
        auto* item = client->CurMap->GetItem(static_cast<uint>(effectSubtype));
        if (item) {
            item->DrawEffect = reload_effect(client->EffectMngr.Effects.Generic);
        }
    }
    if ((eff_type & static_cast<uint>(EffectType::CritterSprite)) && effectSubtype != 0) {
        auto* cr = client->CurMap->GetCritter(static_cast<uint>(effectSubtype));
        if (cr) {
            cr->DrawEffect = reload_effect(client->EffectMngr.Effects.Critter);
        }
    }

    if ((eff_type & static_cast<uint>(EffectType::GenericSprite)) && effectSubtype == 0) {
        client->EffectMngr.Effects.Generic = reload_effect(client->EffectMngr.Effects.GenericDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::CritterSprite)) && effectSubtype == 0) {
        client->EffectMngr.Effects.Critter = reload_effect(client->EffectMngr.Effects.CritterDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::TileSprite)) {
        client->EffectMngr.Effects.Tile = reload_effect(client->EffectMngr.Effects.TileDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::RoofSprite)) {
        client->EffectMngr.Effects.Roof = reload_effect(client->EffectMngr.Effects.RoofDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::RainSprite)) {
        client->EffectMngr.Effects.Rain = reload_effect(client->EffectMngr.Effects.RainDefault);
    }

#if FO_ENABLE_3D
    if (eff_type & static_cast<uint>(EffectType::SkinnedMesh)) {
        client->EffectMngr.Effects.Skinned3d = reload_effect(client->EffectMngr.Effects.Skinned3dDefault);
    }
#endif

    if (eff_type & static_cast<uint>(EffectType::Interface)) {
        client->EffectMngr.Effects.Iface = reload_effect(client->EffectMngr.Effects.IfaceDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::Contour)) {
        client->EffectMngr.Effects.Contour = reload_effect(client->EffectMngr.Effects.ContourDefault);
    }

    if ((eff_type & static_cast<uint>(EffectType::Font)) && effectSubtype == -1) {
        client->EffectMngr.Effects.Font = reload_effect(client->EffectMngr.Effects.FontDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::Font)) && effectSubtype >= 0) {
        client->SprMngr.SetFontEffect(effectSubtype, reload_effect(client->EffectMngr.Effects.Font));
    }

    if (eff_type & static_cast<uint>(EffectType::Primitive)) {
        client->EffectMngr.Effects.Primitive = reload_effect(client->EffectMngr.Effects.PrimitiveDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::Light)) {
        client->EffectMngr.Effects.Light = reload_effect(client->EffectMngr.Effects.LightDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::Fog)) {
        client->EffectMngr.Effects.Fog = reload_effect(client->EffectMngr.Effects.FogDefault);
    }

    if (eff_type & static_cast<uint>(EffectType::FlushRenderTarget)) {
        client->EffectMngr.Effects.FlushRenderTarget = reload_effect(client->EffectMngr.Effects.FlushRenderTargetDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::FlushPrimitive)) {
        client->EffectMngr.Effects.FlushPrimitive = reload_effect(client->EffectMngr.Effects.FlushPrimitiveDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::FlushMap)) {
        client->EffectMngr.Effects.FlushMap = reload_effect(client->EffectMngr.Effects.FlushMapDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::FlushLight)) {
        client->EffectMngr.Effects.FlushLight = reload_effect(client->EffectMngr.Effects.FlushLightDefault);
    }
    if (eff_type & static_cast<uint>(EffectType::FlushFog)) {
        client->EffectMngr.Effects.FlushFog = reload_effect(client->EffectMngr.Effects.FlushFogDefault);
    }

    if (eff_type & static_cast<uint>(EffectType::Offscreen)) {
        if (effectSubtype < 0) {
            throw ScriptException("Negative effect subtype");
        }

        client->OffscreenEffects.resize(effectSubtype + 1);
        client->OffscreenEffects[effectSubtype] = reload_effect(client->EffectMngr.Effects.GenericDefault);
    }
}

///# ...
///# param onlyTiles ...
///# param onlyRoof ...
///# param onlyLight ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_RedrawMap(FOClient* client, bool onlyTiles, bool onlyRoof, bool onlyLight)
{
    if (client->CurMap != nullptr) {
        if (onlyTiles) {
            client->CurMap->RebuildTiles();
        }
        else if (onlyRoof) {
            client->CurMap->RebuildRoof();
        }
        else if (onlyLight) {
            client->CurMap->RebuildLight();
        }
        else {
            client->CurMap->RefreshMap();
        }
    }
}

///# ...
///# param x ...
///# param y ...
///# param button ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SimulateMouseClick(FOClient* client, int x, int y, MouseButton button)
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
[[maybe_unused]] void Client_Game_SimulateKeyboardPress(FOClient* client, KeyCode key1, KeyCode key2, string_view key1Text, string_view key2Text)
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
///# param targetZoom ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_ChangeZoom(FOClient* client, float targetZoom)
{
    if (Math::FloatCompare(targetZoom, client->Settings.SpritesZoom)) {
        return;
    }

    const auto init_zoom = client->Settings.SpritesZoom;
    if (targetZoom == 1.0f) {
        client->CurMap->ChangeZoom(0);
    }
    else if (targetZoom > client->Settings.SpritesZoom) {
        while (targetZoom > client->Settings.SpritesZoom) {
            const auto old_zoom = client->Settings.SpritesZoom;
            client->CurMap->ChangeZoom(1);
            if (client->Settings.SpritesZoom == old_zoom) {
                break;
            }
        }
    }
    else if (targetZoom < client->Settings.SpritesZoom) {
        while (targetZoom < client->Settings.SpritesZoom) {
            const auto old_zoom = client->Settings.SpritesZoom;
            client->CurMap->ChangeZoom(-1);
            if (client->Settings.SpritesZoom == old_zoom) {
                break;
            }
        }
    }

    if (init_zoom != client->Settings.SpritesZoom) {
        client->CurMap->RebuildFog();
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
[[maybe_unused]] void Client_Game_GetTime(FOClient* client, ushort& year, ushort& month, ushort& day, ushort& dayOfWeek, ushort& hour, ushort& minute, ushort& second, ushort& milliseconds)
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
///# param sprName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_LoadSprite(FOClient* client, string_view sprName)
{
    return client->AnimLoad(client->ToHashedString(sprName), AtlasType::Static);
}

///# ...
///# param nameHash ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_LoadSprite(FOClient* client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::Static);
}

///# ...
///# param sprId ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_FreeSprite(FOClient* client, uint sprId)
{
    client->AnimFree(sprId);
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Client_Game_GetSpriteWidth(FOClient* client, uint sprId, int frameIndex)
{
    const auto* anim = client->AnimGetFrames(sprId);
    if (anim == nullptr || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return 0;
    }

    const auto* si = client->SprMngr.GetSpriteInfo(frameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (si == nullptr) {
        return 0;
    }

    return si->Width;
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Client_Game_GetSpriteHeight(FOClient* client, uint sprId, int frameIndex)
{
    const auto* anim = client->AnimGetFrames(sprId);
    if (anim == nullptr || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return 0;
    }

    const auto* si = client->SprMngr.GetSpriteInfo(frameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (si == nullptr) {
        return 0;
    }

    return si->Height;
}

///# ...
///# param sprId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_GetSpriteCount(FOClient* client, uint sprId)
{
    const auto* anim = client->AnimGetFrames(sprId);
    return anim != nullptr ? anim->CntFrm : 0;
}

///# ...
///# param sprId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_GetSpriteTicks(FOClient* client, uint sprId)
{
    const auto* anim = client->AnimGetFrames(sprId);
    return anim != nullptr ? anim->Ticks : 0;
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_GetSpritePixelColor(FOClient* client, uint sprId, int frameIndex, int x, int y)
{
    if (sprId == 0u) {
        return 0;
    }

    const auto* anim = client->AnimGetFrames(sprId);
    if (anim == nullptr || frameIndex >= static_cast<int>(anim->CntFrm)) {
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
[[maybe_unused]] void Client_Game_GetTextInfo(FOClient* client, string_view text, int w, int h, int font, int flags, int& tw, int& th, int& lines)
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
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawSprite(FOClient* client, uint sprId, int frameIndex, int x, int y)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0u) {
        return;
    }

    const auto* anim = client->AnimGetFrames(sprId);
    if (anim == nullptr || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return;
    }

    const auto spr_id = frameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(frameIndex);
    client->SprMngr.DrawSprite(spr_id, x, y, COLOR_SCRIPT_SPRITE(0));
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# param x ...
///# param y ...
///# param color ...
///# param offs ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawSprite(FOClient* client, uint sprId, int frameIndex, int x, int y, uint color, bool offs)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0u) {
        return;
    }

    const auto* anim = client->AnimGetFrames(sprId);
    if (anim == nullptr || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return;
    }

    auto xx = x;
    auto yy = y;

    const auto spr_id = frameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(frameIndex);
    if (offs) {
        const auto* si = client->SprMngr.GetSpriteInfo(spr_id);
        if (si == nullptr) {
            return;
        }

        xx += -si->Width / 2 + si->OffsX;
        yy += -si->Height + si->OffsY;
    }

    client->SprMngr.DrawSprite(spr_id, xx, yy, COLOR_SCRIPT_SPRITE(color));
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawSprite(FOClient* client, uint sprId, int frameIndex, int x, int y, int w, int h)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0u) {
        return;
    }

    const auto* anim = client->AnimGetFrames(sprId);
    if (anim == nullptr || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return;
    }

    const auto spr_id = frameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(frameIndex);
    client->SprMngr.DrawSpriteSizeExt(spr_id, x, y, w, h, true, true, true, COLOR_SCRIPT_SPRITE(0));
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
[[maybe_unused]] void Client_Game_DrawSprite(FOClient* client, uint sprId, int frameIndex, int x, int y, int w, int h, bool zoom, uint color, bool offs)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0u) {
        return;
    }

    const auto* anim = client->AnimGetFrames(sprId);
    if (anim == nullptr || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return;
    }

    auto xx = x;
    auto yy = y;

    const auto spr_id = frameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(frameIndex);
    if (offs) {
        const auto* si = client->SprMngr.GetSpriteInfo(spr_id);
        if (si == nullptr) {
            return;
        }

        xx += si->OffsX;
        yy += si->OffsY;
    }

    client->SprMngr.DrawSpriteSizeExt(spr_id, xx, yy, w, h, zoom, true, true, COLOR_SCRIPT_SPRITE(color));
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
[[maybe_unused]] void Client_Game_DrawSpritePattern(FOClient* client, uint sprId, int frameIndex, int x, int y, int w, int h, int sprWidth, int sprHeight, uint color)
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
[[maybe_unused]] void Client_Game_DrawText(FOClient* client, string_view text, int x, int y, int w, int h, uint color, int font, int flags)
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
[[maybe_unused]] void Client_Game_DrawPrimitive(FOClient* client, int primitiveType, const vector<int>& data)
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

    vector<PrimitivePoint> points;
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

    client->SprMngr.DrawPoints(points, prim);
}

///# ...
///# param mapSpr ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawMapSprite(FOClient* client, MapSprite* mapSpr)
{
    if (mapSpr == nullptr) {
        throw ScriptException("Map sprite arg is null");
    }

    if (client->CurMap == nullptr) {
        return;
    }
    if (mapSpr->HexX >= client->CurMap->GetWidth() || mapSpr->HexY >= client->CurMap->GetHeight()) {
        return;
    }
    if (!client->CurMap->IsHexToDraw(mapSpr->HexX, mapSpr->HexY)) {
        return;
    }

    const auto* anim = client->AnimGetFrames(mapSpr->SprId);
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

    if (mapSpr->ProtoId) {
        const auto* const proto_item = client->ProtoMngr.GetProtoItem(mapSpr->ProtoId);
        if (!proto_item) {
            return;
        }

        color = (proto_item->GetIsColorize() ? proto_item->GetLightColor() : 0);
        is_flat = proto_item->GetIsFlat();
        const auto is_item = !proto_item->IsAnyScenery();
        no_light = (is_flat && !is_item);
        draw_order = (is_flat ? (is_item ? DrawOrderType::FlatItem : DrawOrderType::FlatScenery) : (is_item ? DrawOrderType::Item : DrawOrderType::Scenery));
        draw_order_hy_offset = proto_item->GetDrawOrderOffsetHexY();
        corner = proto_item->GetCorner();
        disable_egg = proto_item->GetDisableEgg();
        contour_color = (proto_item->GetIsBadItem() ? COLOR_RGB(255, 0, 0) : 0);
    }

    auto& field = client->CurMap->GetField(mapSpr->HexX, mapSpr->HexY);
    auto& tree = client->CurMap->GetDrawTree();
    auto& spr = tree.InsertSprite(draw_order, mapSpr->HexX, mapSpr->HexY + draw_order_hy_offset, (client->Settings.MapHexWidth / 2) + mapSpr->OffsX, (client->Settings.MapHexHeight / 2) + mapSpr->OffsY, &field.ScrX, &field.ScrY, mapSpr->FrameIndex < 0 ? anim->GetCurSprId(client->GameTime.GameTick()) : anim->GetSprId(mapSpr->FrameIndex), nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsX : nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsY : nullptr, mapSpr->IsTweakAlpha ? &mapSpr->TweakAlpha : nullptr, nullptr, &mapSpr->Valid);

    spr.MapSpr = mapSpr;
    mapSpr->AddRef();

    if (!no_light) {
        spr.SetLight(corner, client->CurMap->GetLightHex(0, 0), client->CurMap->GetWidth(), client->CurMap->GetHeight());
    }

    if (!is_flat && !disable_egg) {
        EggAppearenceType egg_appearence;
        switch (corner) {
        case CornerType::South:
            egg_appearence = EggAppearenceType::ByXOrY;
            break;
        case CornerType::North:
            egg_appearence = EggAppearenceType::ByXAndY;
            break;
        case CornerType::EastWest:
        case CornerType::West:
            egg_appearence = EggAppearenceType::ByY;
            break;
        default:
            egg_appearence = EggAppearenceType::ByX;
            break;
        }
        spr.SetEggAppearence(egg_appearence);
    }

    if (color != 0u) {
        spr.SetColor(color & 0xFFFFFF);
        spr.SetFixedAlpha(color >> 24);
    }

    if (contour_color != 0u) {
        spr.SetContour(ContourType::Custom, contour_color);
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
[[maybe_unused]] void Client_Game_DrawCritter2d(FOClient* client, hstring modelName, uint anim1, uint anim2, uchar dir, int l, int t, int r, int b, bool scratch, bool center, uint color)
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
[[maybe_unused]] void Client_Game_DrawCritter3d(FOClient* client, uint instance, hstring modelName, uint anim1, uint anim2, const vector<int>& layers, const vector<float>& position, uint color)
{
#if FO_ENABLE_3D
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

    auto*& model = client->DrawCritterModel[instance];
    if (model == nullptr || client->DrawCritterModelCrType[instance] != modelName) {
        if (model != nullptr) {
            client->SprMngr.FreeModel(model);
        }

        client->SprMngr.PushAtlasType(AtlasType::Dynamic);
        model = client->SprMngr.LoadModel(modelName, false);
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

    model->SetLookDirAngle(0);
    model->SetMoveDirAngle(0, false);
    model->SetRotation(rx * PI_FLOAT / 180.0f, ry * PI_FLOAT / 180.0f, rz * PI_FLOAT / 180.0f);
    model->SetScale(sx, sy, sz);
    model->SetSpeed(speed);
    model->SetAnimation(anim1, anim2, client->DrawCritterModelLayers, ANIMATION_PERIOD(static_cast<int>(period * 100.0f)) | ANIMATION_NO_SMOOTH);

    client->SprMngr.Draw3d(static_cast<int>(x), static_cast<int>(y), model, COLOR_SCRIPT_SPRITE(color));

    if (count > 13) {
        client->SprMngr.PopScissor();
    }

#else
    throw NotEnabled3DException("3D submodule not enabled");
#endif
}

///# ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_PushDrawScissor(FOClient* client, int x, int y, int w, int h)
{
    client->SprMngr.PushScissor(x, y, x + w, y + h);
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_PopDrawScissor(FOClient* client)
{
    client->SprMngr.PopScissor();
}

///# ...
///# param forceClear ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_ActivateOffscreenSurface(FOClient* client, bool forceClear)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (client->OffscreenSurfaces.empty()) {
        auto* const rt = client->SprMngr.CreateRenderTarget(false, RenderTarget::SizeType::Screen, 0, 0, false);
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
[[maybe_unused]] void Client_Game_PresentOffscreenSurface(FOClient* client, int effectSubtype)
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

    rt->CustomDrawEffect = client->OffscreenEffects[effectSubtype];

    client->SprMngr.DrawRenderTarget(rt, true, nullptr, nullptr);
}

///# ...
///# param effectSubtype ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_PresentOffscreenSurface(FOClient* client, int effectSubtype, int x, int y, int w, int h)
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

    rt->CustomDrawEffect = client->OffscreenEffects[effectSubtype];

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
[[maybe_unused]] void Client_Game_PresentOffscreenSurface(FOClient* client, int effectSubtype, int fromX, int fromY, int fromW, int fromH, int toX, int toY, int toW, int toH)
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

    rt->CustomDrawEffect = client->OffscreenEffects[effectSubtype];

    IRect from(std::clamp(fromX, 0, client->Settings.ScreenWidth), std::clamp(fromY, 0, client->Settings.ScreenHeight), std::clamp(fromX + fromW, 0, client->Settings.ScreenWidth), std::clamp(fromY + fromH, 0, client->Settings.ScreenHeight));
    IRect to(std::clamp(toX, 0, client->Settings.ScreenWidth), std::clamp(toY, 0, client->Settings.ScreenHeight), std::clamp(toX + toW, 0, client->Settings.ScreenWidth), std::clamp(toY + toH, 0, client->Settings.ScreenHeight));
    client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}

///# ...
///# param screen ...
///# param data ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_ShowScreen(FOClient* client, int screen)
{
    if (screen >= SCREEN_LOGIN && screen <= SCREEN_WAIT) {
        client->ShowMainScreen(screen, {});
    }
    else if (screen != SCREEN_NONE) {
        client->ShowScreen(screen, {});
    }
    else {
        client->HideScreen(screen);
    }
}

///# ...
///# param screen ...
///# param data ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_ShowScreen(FOClient* client, int screen, const map<string, string>& data)
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
///@ ExportMethod
[[maybe_unused]] void Client_Game_HideScreen(FOClient* client)
{
    client->HideScreen(SCREEN_NONE);
}

///# ...
///# param screen ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_HideScreen(FOClient* client, int screen)
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
[[maybe_unused]] bool Client_Game_GetHexScreenPos(FOClient* client, ushort hx, ushort hy, int& x, int& y)
{
    x = 0;
    y = 0;

    if (client->CurMap != nullptr && hx < client->CurMap->GetWidth() && hy < client->CurMap->GetHeight()) {
        client->CurMap->GetHexCurrentPosition(hx, hy, x, y);
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
[[maybe_unused]] bool Client_Game_GetHexAtScreenPos(FOClient* client, int x, int y, ushort& hx, ushort& hy)
{
    if (client->CurMap == nullptr) {
        return false;
    }

    const auto old_x = client->Settings.MouseX;
    const auto old_y = client->Settings.MouseY;
    client->Settings.MouseX = x;
    client->Settings.MouseY = y;
    ushort hx_ = 0;
    ushort hy_ = 0;
    const auto result = client->CurMap->GetHexAtScreenPos(x, y, hx_, hy_, nullptr, nullptr);
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
///# param hx ...
///# param hy ...
///# param ox ...
///# param oy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_GetHexAtScreenPos(FOClient* client, int x, int y, ushort& hx, ushort& hy, int& ox, int& oy)
{
    if (client->CurMap == nullptr) {
        return false;
    }

    const auto old_x = client->Settings.MouseX;
    const auto old_y = client->Settings.MouseY;
    client->Settings.MouseX = x;
    client->Settings.MouseY = y;
    ushort hx_ = 0;
    ushort hy_ = 0;
    const auto result = client->CurMap->GetHexAtScreenPos(x, y, hx_, hy_, &ox, &oy);
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
[[maybe_unused]] ItemView* Client_Game_GetItemAtScreenPos(FOClient* client, int x, int y)
{
    if (client->CurMap == nullptr) {
        return nullptr;
    }

    bool item_egg;
    return client->CurMap->GetItemAtScreenPos(x, y, item_egg);
}

///# ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Client_Game_GetCritterAtScreenPos(FOClient* client, int x, int y)
{
    if (client->CurMap == nullptr) {
        return nullptr;
    }

    return client->CurMap->GetCritterAtScreenPos(x, y, false, false);
}

///# ...
///# param x ...
///# param y ...
///# param wideRange
///# return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Client_Game_GetCritterAtScreenPos(FOClient* client, int x, int y, bool wideRange)
{
    if (client->CurMap == nullptr) {
        return nullptr;
    }

    auto* cr = client->CurMap->GetCritterAtScreenPos(x, y, false, false);
    if (cr == nullptr && wideRange) {
        cr = client->CurMap->GetCritterAtScreenPos(x, y, false, true);
    }
    return cr;
}

///# ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ClientEntity* Client_Game_GetEntityAtScreenPos(FOClient* client, int x, int y)
{
    if (client->CurMap == nullptr) {
        return nullptr;
    }

    return client->CurMap->GetEntityAtScreenPos(x, y);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ushort Client_Game_GetMapWidth(FOClient* client)
{
    if (client->CurMap == nullptr) {
        throw ScriptException("Map is not loaded");
    }

    return client->CurMap->GetWidth();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ushort Client_Game_GetMapHeight(FOClient* client)
{
    if (client->CurMap == nullptr) {
        throw ScriptException("Map is not loaded");
    }

    return client->CurMap->GetHeight();
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_IsMapHexPassed(FOClient* client, ushort hx, ushort hy)
{
    if (client->CurMap == nullptr) {
        throw ScriptException("Map is not loaded");
    }
    if (hx >= client->CurMap->GetWidth() || hy >= client->CurMap->GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    return !client->CurMap->GetField(hx, hy).Flags.IsNotPassed;
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_IsMapHexRaked(FOClient* client, ushort hx, ushort hy)
{
    if (client->CurMap == nullptr) {
        throw ScriptException("Map is not loaded");
    }
    if (hx >= client->CurMap->GetWidth() || hy >= client->CurMap->GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    return !client->CurMap->GetField(hx, hy).Flags.IsNotRaked;
}

///# ...
///# param filePath ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SaveScreenshot(FOClient* client, string_view filePath)
{
    // client->SprMngr.SaveTexture(nullptr, _str(filePath).formatPath(), true);
}

///# ...
///# param filePath ...
///# param text ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SaveText(FOClient* client, string_view filePath, string_view text)
{
    auto file = DiskFileSystem::OpenFile(_str(filePath).formatPath(), true);
    if (!file) {
        throw ScriptException("Can't open file for writing", filePath);
    }

    if (!file.Write(text)) {
        throw ScriptException("Can't write file", filePath, text.length());
    }
}

///# ...
///# param name ...
///# param data ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SetCacheData(FOClient* client, string_view name, const vector<uchar>& data)
{
    client->Cache.SetData(name, data);
}

///# ...
///# param name ...
///# param data ...
///# param dataSize ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SetCacheData(FOClient* client, string_view name, const vector<uchar>& data, uint dataSize)
{
    auto data_copy = data;
    data_copy.resize(dataSize);
    client->Cache.SetData(name, data_copy);
}

///# ...
///# param name ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<uchar> Client_Game_GetCacheData(FOClient* client, string_view name)
{
    return client->Cache.GetData(name);
}

///# ...
///# param name ...
///# param str ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SetCacheText(FOClient* client, string_view name, string_view str)
{
    client->Cache.SetString(name, str);
}

///# ...
///# param name ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Game_GetCacheText(FOClient* client, string_view name)
{
    return client->Cache.GetString(name);
}

///# ...
///# param name ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_IsCacheEntry(FOClient* client, string_view name)
{
    return client->Cache.HasEntry(name);
}

///# ...
///# param name ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_RemoveCacheEntry(FOClient* client, string_view name)
{
    client->Cache.EraseEntry(name);
}

///# ...
///# param keyValues ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SetUserConfig(FOClient* client, const map<string, string>& keyValues)
{
    string cfg_user;
    for (const auto& [key, value] : keyValues) {
        cfg_user += _str("{} = {}\n", key, value).str();
    }

    client->Cache.SetString(CONFIG_NAME, cfg_user);
}
