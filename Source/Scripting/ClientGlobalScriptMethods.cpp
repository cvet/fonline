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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "NetCommand.h"
#include "StringUtils.h"

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsFullscreen(FOClient* client)
{
    return client->SprMngr.IsFullscreen();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ToggleFullscreen(FOClient* client)
{
    client->SprMngr.ToggleFullscreen();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_MinimizeWindow(FOClient* client)
{
    client->SprMngr.MinimizeWindow();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsConnecting(FOClient* client)
{
    return client->IsConnecting();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsConnected(FOClient* client)
{
    return client->IsConnected();
}

///@ ExportMethod
FO_SCRIPT_API ident_t Client_Game_DeferredCall(FOClient* client, uint delay, ScriptFunc<void> func)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, false, func);
}

///@ ExportMethod
FO_SCRIPT_API ident_t Client_Game_DeferredCall(FOClient* client, uint delay, ScriptFunc<void, any_t> func, any_t value)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, false, func, std::move(value));
}

///@ ExportMethod
FO_SCRIPT_API ident_t Client_Game_DeferredCall(FOClient* client, uint delay, ScriptFunc<void, vector<any_t>> func, const vector<any_t>& values)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, false, func, values);
}

///@ ExportMethod
FO_SCRIPT_API ident_t Client_Game_RepeatingDeferredCall(FOClient* client, uint delay, ScriptFunc<void> func)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, true, func);
}

///@ ExportMethod
FO_SCRIPT_API ident_t Client_Game_RepeatingDeferredCall(FOClient* client, uint delay, ScriptFunc<void, any_t> func, any_t value)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, true, func, std::move(value));
}

///@ ExportMethod
FO_SCRIPT_API ident_t Client_Game_RepeatingDeferredCall(FOClient* client, uint delay, ScriptFunc<void, vector<any_t>> func, const vector<any_t>& values)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, true, func, values);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsDeferredCallPending(FOClient* client, ident_t id)
{
    return client->ClientDeferredCalls.IsDeferredCallPending(id);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_CancelDeferredCall(FOClient* client, ident_t id)
{
    return client->ClientDeferredCalls.CancelDeferredCall(id);
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API uint Client_Game_GetDistance(FOClient* client, CritterView* cr1, CritterView* cr2)
{
    UNUSED_VARIABLE(client);

    if (cr1 == nullptr) {
        throw ScriptException("Critter 1 arg is null");
    }
    if (cr2 == nullptr) {
        throw ScriptException("Critter 2 arg is null");
    }

    const auto* hex_cr1 = dynamic_cast<CritterHexView*>(cr1);
    const auto* hex_cr2 = dynamic_cast<CritterHexView*>(cr2);

    if (hex_cr1 != nullptr && hex_cr2 != nullptr && hex_cr1->GetMapId() == hex_cr2->GetMapId()) {
        const auto dist = GeometryHelper::DistGame(hex_cr1->GetHex(), hex_cr2->GetHex());
        const auto multihex = cr1->GetMultihex() + cr2->GetMultihex();
        return multihex < dist ? dist - multihex : 0;
    }
    else {
        throw ScriptException("Critters not on map");
    }
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API uint Client_Game_GetDistance(FOClient* client, ItemView* item1, ItemView* item2)
{
    UNUSED_VARIABLE(client);

    if (item1 == nullptr) {
        throw ScriptException("Item 1 arg is null");
    }
    if (item2 == nullptr) {
        throw ScriptException("Item 2 arg is null");
    }

    const auto* hex_item1 = dynamic_cast<ItemHexView*>(item1);
    const auto* hex_item2 = dynamic_cast<ItemHexView*>(item2);

    if (hex_item1 != nullptr && hex_item2 != nullptr && hex_item1->GetMapId() == hex_item2->GetMapId()) {
        const auto dist = GeometryHelper::DistGame(hex_item1->GetHex(), hex_item2->GetHex());
        return dist;
    }
    else {
        throw ScriptException("Items not on map");
    }
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API uint Client_Game_GetDistance(FOClient* client, CritterView* cr, ItemView* item)
{
    UNUSED_VARIABLE(client);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }

    const auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    const auto* hex_item = dynamic_cast<ItemHexView*>(item);

    if (hex_cr != nullptr && hex_item != nullptr && hex_cr->GetMapId() == hex_item->GetMapId()) {
        const auto dist = GeometryHelper::DistGame(hex_cr->GetHex(), hex_item->GetHex());
        const auto multihex = hex_cr->GetMultihex();
        return multihex < dist ? dist - multihex : 0;
    }
    else {
        throw ScriptException("Critter/Item not on map");
    }
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API uint Client_Game_GetDistance(FOClient* client, ItemView* item, CritterView* cr)
{
    UNUSED_VARIABLE(client);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }

    const auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    const auto* hex_item = dynamic_cast<ItemHexView*>(item);

    if (hex_cr != nullptr && hex_item != nullptr && hex_cr->GetMapId() == hex_item->GetMapId()) {
        const auto dist = GeometryHelper::DistGame(hex_cr->GetHex(), hex_item->GetHex());
        const auto multihex = hex_cr->GetMultihex();
        return multihex < dist ? dist - multihex : 0;
    }
    else {
        throw ScriptException("Item/Critter not on map");
    }
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API uint Client_Game_GetDistance(FOClient* client, CritterView* cr, mpos hex)
{
    UNUSED_VARIABLE(client);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    const auto* hex_cr = dynamic_cast<CritterHexView*>(cr);

    if (hex_cr != nullptr) {
        const auto dist = GeometryHelper::DistGame(hex_cr->GetHex(), hex);
        const auto multihex = hex_cr->GetMultihex();
        return multihex < dist ? dist - multihex : 0;
    }
    else {
        throw ScriptException("Critter not on map");
    }
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API uint Client_Game_GetDistance(FOClient* client, mpos hex, CritterView* cr)
{
    UNUSED_VARIABLE(client);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    const auto* hex_cr = dynamic_cast<CritterHexView*>(cr);

    if (hex_cr != nullptr) {
        const auto dist = GeometryHelper::DistGame(hex_cr->GetHex(), hex);
        const auto multihex = hex_cr->GetMultihex();
        return multihex < dist ? dist - multihex : 0;
    }
    else {
        throw ScriptException("Critter not on map");
    }
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API uint Client_Game_GetDistance(FOClient* client, mpos hex, ItemView* item)
{
    UNUSED_VARIABLE(client);

    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }

    const auto* hex_item = dynamic_cast<ItemHexView*>(item);

    if (hex_item != nullptr) {
        const auto dist = GeometryHelper::DistGame(hex_item->GetHex(), hex);
        return dist;
    }
    else {
        throw ScriptException("Item not on map");
    }
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API uint Client_Game_GetDistance(FOClient* client, ItemView* item, mpos hex)
{
    UNUSED_VARIABLE(client);

    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }

    const auto* hex_item = dynamic_cast<ItemHexView*>(item);

    if (hex_item != nullptr) {
        const auto dist = GeometryHelper::DistGame(hex_item->GetHex(), hex);
        return dist;
    }
    else {
        throw ScriptException("Item not on map");
    }
}

///@ ExportMethod
FO_SCRIPT_API uint Client_Game_GetTick(FOClient* client)
{
    return time_duration_to_ms<uint>(client->GameTime.FrameTime().time_since_epoch());
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_CustomCall(FOClient* client, string_view command)
{
    return client->CustomCall(command, " ");
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_CustomCall(FOClient* client, string_view command, string_view separator)
{
    return client->CustomCall(command, separator);
}

///@ ExportMethod
FO_SCRIPT_API CritterView* Client_Game_GetChosen(FOClient* client)
{
    return client->GetChosen();
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API ItemView* Client_Game_GetItem(FOClient* client, ident_t itemId)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    ItemView* item = nullptr;

    // On chosen
    if (auto* chosen = client->GetChosen(); chosen != nullptr) {
        item = chosen->GetInvItem(itemId);
    }

    // On map
    if (client->CurMap != nullptr) {
        if (item == nullptr) {
            item = client->CurMap->GetItem(itemId);
        }

        if (item == nullptr) {
            for (auto* cr : client->CurMap->GetCritters()) {
                if (!cr->GetIsChosen()) {
                    item = cr->GetInvItem(itemId);

                    if (item != nullptr) {
                        break;
                    }
                }
            }
        }
    }
    else {
        if (item == nullptr) {
            for (auto* cr : client->GetGlobalMapCritters()) {
                if (!cr->GetIsChosen()) {
                    item = cr->GetInvItem(itemId);

                    if (item != nullptr) {
                        break;
                    }
                }
            }
        }
    }

    return item != nullptr && !item->IsDestroyed() ? item : nullptr;
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API CritterView* Client_Game_GetCritter(FOClient* client, ident_t crId)
{
    if (!crId) {
        return nullptr;
    }

    if (client->CurMap != nullptr) {
        auto* cr = client->CurMap->GetCritter(crId);
        if (cr == nullptr || cr->IsDestroyed() || cr->IsDestroying()) {
            return nullptr;
        }

        return cr;
    }
    else {
        return client->GetGlobalMapCritter(crId);
    }
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API vector<CritterView*> Client_Game_GetCritters(FOClient* client, CritterFindType findType)
{
    vector<CritterView*> critters;

    if (client->CurMap != nullptr) {
        auto&& map_critters = client->CurMap->GetCritters();
        critters.reserve(map_critters.size());

        for (auto* cr : map_critters) {
            if (cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }
    else {
        critters = client->GetGlobalMapCritters();
    }

    return critters;
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API vector<CritterView*> Client_Game_GetCritters(FOClient* client, hstring pid, CritterFindType findType)
{
    vector<CritterView*> critters;

    if (client->CurMap != nullptr) {
        if (!pid) {
            for (auto* cr : client->CurMap->GetCritters()) {
                if (cr->CheckFind(findType)) {
                    critters.push_back(cr);
                }
            }
        }
        else {
            for (auto* cr : client->CurMap->GetCritters()) {
                if (cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                    critters.push_back(cr);
                }
            }
        }
    }
    else {
        if (!pid) {
            for (auto* cr : client->GetGlobalMapCritters()) {
                if (cr->CheckFind(findType)) {
                    critters.push_back(cr);
                }
            }
        }
        else {
            for (auto* cr : client->GetGlobalMapCritters()) {
                if (cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                    critters.push_back(cr);
                }
            }
        }
    }

    return critters;
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API vector<CritterView*> Client_Game_SortCrittersByDeep(FOClient* client, const vector<CritterView*>& critters)
{
    UNUSED_VARIABLE(client);

    vector<CritterView*> sorted_critters = critters;

    std::sort(sorted_critters.begin(), sorted_critters.end(), [](const CritterView* cr1, const CritterView* cr2) {
        const auto cr1_pos = cr1->GetHex();
        const auto cr2_pos = cr2->GetHex();

        if (cr1_pos.y == cr2_pos.y) {
            if (cr1_pos.x == cr2_pos.x) {
                const auto* cr1_hex = dynamic_cast<const CritterHexView*>(cr1);
                const auto* cr2_hex = dynamic_cast<const CritterHexView*>(cr2);

                if (cr1_hex != nullptr && cr2_hex != nullptr && cr1_hex->IsSpriteValid() && cr2_hex->IsSpriteValid()) {
                    return cr1_hex->GetSprite()->TreeIndex < cr2_hex->GetSprite()->TreeIndex;
                }

                return cr1 < cr2;
            }

            return cr1_pos.x < cr2_pos.x;
        }

        return cr1_pos.y < cr2_pos.y;
    });

    return sorted_critters;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_FlushScreen(FOClient* client, ucolor fromColor, ucolor toColor, tick_t duration)
{
    client->ScreenFade(std::chrono::milliseconds {duration.underlying_value()}, fromColor, toColor, true);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_QuakeScreen(FOClient* client, int noise, tick_t duration)
{
    client->ScreenQuake(noise, std::chrono::milliseconds {duration.underlying_value()});
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_PlaySound(FOClient* client, string_view soundName)
{
    return client->SndMngr.PlaySound(client->ResMngr.GetSoundNames(), soundName);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_PlayMusic(FOClient* client, string_view musicName, tick_t repeatTime)
{
    if (musicName.empty()) {
        client->SndMngr.StopMusic();
        return true;
    }

    return client->SndMngr.PlayMusic(musicName, std::chrono::milliseconds {repeatTime.underlying_value()});
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PlayVideo(FOClient* client, string_view videoName, bool canInterrupt, bool enqueue)
{
    client->PlayVideo(videoName, canInterrupt, enqueue);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsVideoPlaying(FOClient* client)
{
    return client->IsVideoPlaying();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API VideoPlayback* Client_Game_CreateVideoPlayback(FOClient* client, string_view videoName, bool looped)
{
    const auto file = client->Resources.ReadFile(videoName);
    if (!file) {
        throw ScriptException("Video file not found", videoName);
    }

    auto&& clip = std::make_unique<VideoClip>(file.GetData());
    auto&& tex = unique_ptr<RenderTexture> {App->Render.CreateTexture(clip->GetSize(), true, false)};

    clip->SetLooped(looped);

    auto* video = new VideoPlayback();
    video->Clip = std::move(clip);
    video->Tex = std::move(tex);

    return video;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawVideoPlayback(FOClient* client, VideoPlayback* video, ipos pos, isize size)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (video == nullptr || !video->Clip) {
        return;
    }

    if (size.width > 0 && size.height > 0) {
        video->Tex->UpdateTextureRegion({}, video->Tex->Size, video->Clip->RenderFrame().data());

        const auto r = IRect {pos.x, pos.y, pos.x + size.width, pos.y + size.height};
        client->SprMngr.DrawTexture(video->Tex.get(), false, nullptr, &r);
    }

    if (video->Clip->IsStopped()) {
        video->Clip.reset();
        video->Tex.reset();
        video->Stopped = true;
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ConsoleMessage(FOClient* client, string_view msg)
{
    client->ConsoleMessage(msg);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Message(FOClient* client, string_view msg)
{
    client->AddMessage(FOMB_GAME, msg);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Message(FOClient* client, int type, string_view msg)
{
    client->AddMessage(type, msg);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Message(FOClient* client, TextPackName textPack, uint strNum)
{
    client->AddMessage(FOMB_GAME, client->GetCurLang().GetTextPack(textPack).GetStr(strNum));
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Message(FOClient* client, int type, TextPackName textPack, uint strNum)
{
    client->AddMessage(type, client->GetCurLang().GetTextPack(textPack).GetStr(strNum));
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetText(FOClient* client, TextPackName textPack, uint strNum)
{
    return client->GetCurLang().GetTextPack(textPack).GetStr(strNum);
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetText(FOClient* client, TextPackName textPack, uint strNum, uint skipCount)
{
    return client->GetCurLang().GetTextPack(textPack).GetStr(strNum, skipCount);
}

///@ ExportMethod
FO_SCRIPT_API uint Client_Game_GetTextNumUpper(FOClient* client, TextPackName textPack, uint strNum)
{
    return client->GetCurLang().GetTextPack(textPack).GetStrNumUpper(strNum);
}

///@ ExportMethod
FO_SCRIPT_API uint Client_Game_GetTextNumLower(FOClient* client, TextPackName textPack, uint strNum)
{
    return client->GetCurLang().GetTextPack(textPack).GetStrNumLower(strNum);
}

///@ ExportMethod
FO_SCRIPT_API uint Client_Game_GetTextCount(FOClient* client, TextPackName textPack, uint strNum)
{
    return static_cast<uint>(client->GetCurLang().GetTextPack(textPack).GetStrCount(strNum));
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsTextPresent(FOClient* client, TextPackName textPack, uint strNum)
{
    return client->GetCurLang().GetTextPack(textPack).GetStrCount(strNum) != 0;
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_ReplaceText(FOClient* client, string_view text, string_view from, string_view to)
{
    UNUSED_VARIABLE(client);

    return strex(text).replace(from, to);
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_ReplaceText(FOClient* client, string_view text, string_view from, int64 to)
{
    UNUSED_VARIABLE(client);

    return strex(text).replace(from, strex("{}", to));
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_FormatTags(FOClient* client, string_view text, string_view lexems)
{
    auto text_copy = string(text);
    client->FormatTags(text_copy, client->GetChosen(), nullptr, lexems);
    return text_copy;
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API int Client_Game_GetFog(FOClient* client, uint16 zoneX, uint16 zoneY)
{
    if (zoneX >= client->Settings.GlobalMapWidth || zoneY >= client->Settings.GlobalMapHeight) {
        throw ScriptException("Invalid world map pos arg");
    }

    return client->GetGlobalMapFog().Get2Bit(zoneX, zoneY);
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API tick_t Client_Game_GetFullSecond(FOClient* client)
{
    return client->GameTime.GetServerTime();
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API tick_t Client_Game_EvaluateFullSecond(FOClient* client, uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)
{
    if (year != 0 && year < client->Settings.StartYear) {
        throw ScriptException("Invalid year", year);
    }
    if (year != 0 && year > client->Settings.StartYear + 100) {
        throw ScriptException("Invalid year", year);
    }
    if (month != 0 && month < 1) {
        throw ScriptException("Invalid month", month);
    }
    if (month != 0 && month > 12) {
        throw ScriptException("Invalid month", month);
    }

    auto year_ = year;
    auto month_ = month;
    auto day_ = day;

    if (year_ == 0) {
        year_ = client->GetYear();
    }
    if (month_ == 0) {
        month_ = client->GetMonth();
    }

    if (day_ != 0) {
        const auto month_day = Timer::GameTimeMonthDays(year, month_);

        if (day_ < month_day || day_ > month_day) {
            throw ScriptException("Invalid day", day_, month_day);
        }
    }

    if (day_ == 0) {
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

    return client->GameTime.DateToServerTime(year_, month_, day_, hour, minute, second);
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API void Client_Game_EvaluateGameTime(FOClient* client, tick_t serverTime, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second)
{
    const auto dt = client->GameTime.ServerTimeToDate(serverTime);
    year = dt.Year;
    month = dt.Month;
    dayOfWeek = dt.DayOfWeek;
    day = dt.Day;
    hour = dt.Hour;
    minute = dt.Minute;
    second = dt.Second;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Preload3dFiles(FOClient* client, const vector<string>& fnames)
{
    for (size_t i = 0; i < fnames.size(); i++) {
        client->Preload3dFiles.push_back(fnames[i]);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_LoadFont(FOClient* client, int fontIndex, string_view fontFname)
{
    bool result;

    if (!fontFname.empty() && fontFname[0] == '*') {
        result = client->SprMngr.LoadFontFO(fontIndex, fontFname.substr(1), AtlasType::IfaceSprites, false, false);
    }
    else {
        result = client->SprMngr.LoadFontBmf(fontIndex, fontFname, AtlasType::IfaceSprites);
    }

    if (!result) {
        throw ScriptException("Can't load font", fontIndex, fontFname);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetDefaultFont(FOClient* client, int font)
{
    client->SprMngr.SetDefaultFont(font);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetEffect(FOClient* client, EffectType effectType, int64 effectSubtype, string_view effectPath)
{
    const auto reload_effect = [&](RenderEffect* def_effect) {
        if (!effectPath.empty()) {
            auto* effect = client->EffectMngr.LoadEffect(def_effect->Usage, effectPath);
            if (effect == nullptr) {
                throw ScriptException("Effect not found or have some errors, see log file");
            }
            return effect;
        }
        return def_effect;
    };

    const auto eff_type = static_cast<uint>(effectType);

    if (((eff_type & static_cast<uint>(EffectType::GenericSprite)) != 0) && effectSubtype != 0) {
        auto* item = client->CurMap->GetItem(ident_t {static_cast<uint>(effectSubtype)});
        if (item != nullptr) {
            item->DrawEffect = reload_effect(client->EffectMngr.Effects.Generic);
        }
    }
    if (((eff_type & static_cast<uint>(EffectType::CritterSprite)) != 0) && effectSubtype != 0) {
        auto* cr = client->CurMap->GetCritter(ident_t {static_cast<uint>(effectSubtype)});
        if (cr != nullptr) {
            cr->DrawEffect = reload_effect(client->EffectMngr.Effects.Critter);
        }
    }

    if (((eff_type & static_cast<uint>(EffectType::GenericSprite)) != 0) && effectSubtype == 0) {
        client->EffectMngr.Effects.Generic = reload_effect(client->EffectMngr.Effects.GenericDefault);
    }
    if (((eff_type & static_cast<uint>(EffectType::CritterSprite)) != 0) && effectSubtype == 0) {
        client->EffectMngr.Effects.Critter = reload_effect(client->EffectMngr.Effects.CritterDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::TileSprite)) != 0) {
        client->EffectMngr.Effects.Tile = reload_effect(client->EffectMngr.Effects.TileDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::RoofSprite)) != 0) {
        client->EffectMngr.Effects.Roof = reload_effect(client->EffectMngr.Effects.RoofDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::RainSprite)) != 0) {
        client->EffectMngr.Effects.Rain = reload_effect(client->EffectMngr.Effects.RainDefault);
    }

#if FO_ENABLE_3D
    if ((eff_type & static_cast<uint>(EffectType::SkinnedMesh)) != 0) {
        client->EffectMngr.Effects.SkinnedModel = reload_effect(client->EffectMngr.Effects.SkinnedModelDefault);
    }
#endif

    if ((eff_type & static_cast<uint>(EffectType::Interface)) != 0) {
        client->EffectMngr.Effects.Iface = reload_effect(client->EffectMngr.Effects.IfaceDefault);
    }

    if ((eff_type & static_cast<uint>(EffectType::ContourStrict)) != 0) {
        client->EffectMngr.Effects.ContourStrictSprite = reload_effect(client->EffectMngr.Effects.ContourStrictSpriteDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::ContourDynamic)) != 0) {
        client->EffectMngr.Effects.ContourDynamicSprite = reload_effect(client->EffectMngr.Effects.ContourDynamicSpriteDefault);
    }

    if (((eff_type & static_cast<uint>(EffectType::Font)) != 0) && effectSubtype == -1) {
        client->EffectMngr.Effects.Font = reload_effect(client->EffectMngr.Effects.FontDefault);
    }
    if (((eff_type & static_cast<uint>(EffectType::Font)) != 0) && effectSubtype >= 0) {
        client->SprMngr.SetFontEffect(static_cast<int>(effectSubtype), reload_effect(client->EffectMngr.Effects.Font));
    }

    if ((eff_type & static_cast<uint>(EffectType::Primitive)) != 0) {
        client->EffectMngr.Effects.Primitive = reload_effect(client->EffectMngr.Effects.PrimitiveDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::Light)) != 0) {
        client->EffectMngr.Effects.Light = reload_effect(client->EffectMngr.Effects.LightDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::Fog)) != 0) {
        client->EffectMngr.Effects.Fog = reload_effect(client->EffectMngr.Effects.FogDefault);
    }

    if ((eff_type & static_cast<uint>(EffectType::FlushRenderTarget)) != 0) {
        client->EffectMngr.Effects.FlushRenderTarget = reload_effect(client->EffectMngr.Effects.FlushRenderTargetDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::FlushPrimitive)) != 0) {
        client->EffectMngr.Effects.FlushPrimitive = reload_effect(client->EffectMngr.Effects.FlushPrimitiveDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::FlushMap)) != 0) {
        client->EffectMngr.Effects.FlushMap = reload_effect(client->EffectMngr.Effects.FlushMapDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::FlushLight)) != 0) {
        client->EffectMngr.Effects.FlushLight = reload_effect(client->EffectMngr.Effects.FlushLightDefault);
    }
    if ((eff_type & static_cast<uint>(EffectType::FlushFog)) != 0) {
        client->EffectMngr.Effects.FlushFog = reload_effect(client->EffectMngr.Effects.FlushFogDefault);
    }

    if ((eff_type & static_cast<uint>(EffectType::Offscreen)) != 0) {
        if (effectSubtype < 0) {
            throw ScriptException("Negative effect subtype");
        }

        client->OffscreenEffects.resize(static_cast<size_t>(effectSubtype) + 1);
        client->OffscreenEffects[static_cast<size_t>(effectSubtype)] = reload_effect(client->EffectMngr.Effects.GenericDefault);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateMouseClick(FOClient* client, ipos pos, MouseButton button)
{
    UNUSED_VARIABLE(client);
    UNUSED_VARIABLE(pos);
    UNUSED_VARIABLE(button);

    throw NotImplementedException(LINE_STR);

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

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateKeyboardPress(FOClient* client, KeyCode key1, KeyCode key2, string_view key1Text, string_view key2Text)
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

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API void Client_Game_GetTime(FOClient* client, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second, uint16& milliseconds)
{
    UNUSED_VARIABLE(client);

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

///@ ExportMethod
FO_SCRIPT_API uint Client_Game_LoadSprite(FOClient* client, string_view sprName)
{
    return client->AnimLoad(client->ToHashedString(sprName), AtlasType::IfaceSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint Client_Game_LoadSprite(FOClient* client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::IfaceSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint Client_Game_LoadMapSprite(FOClient* client, string_view sprName)
{
    return client->AnimLoad(client->ToHashedString(sprName), AtlasType::MapSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint Client_Game_LoadMapSprite(FOClient* client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::MapSprites);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_FreeSprite(FOClient* client, uint sprId)
{
    client->AnimFree(sprId);
}

///@ ExportMethod
FO_SCRIPT_API isize Client_Game_GetSpriteSize(FOClient* client, uint sprId)
{
    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return {};
    }

    return spr->Size;
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsSpriteHit(FOClient* client, uint sprId, ipos pos)
{
    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return 0;
    }

    return client->SprMngr.SpriteHitTest(spr, pos, false);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_StopSprite(FOClient* client, uint sprId)
{
    if (sprId == 0) {
        return;
    }

    auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    spr->Stop();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetSpriteTime(FOClient* client, uint sprId, float normalizedTime)
{
    if (sprId == 0) {
        return;
    }

    auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    spr->SetTime(normalizedTime);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PlaySprite(FOClient* client, uint sprId, hstring animName, bool looped, bool reversed)
{
    if (sprId == 0) {
        return;
    }

    auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    spr->Play(animName, looped, reversed);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_GetTextInfo(FOClient* client, string_view text, isize size, int font, uint flags, isize& resultSize, int& resultLines)
{
    if (!client->SprMngr.GetTextInfo(size, text, font, flags, resultSize, resultLines)) {
        throw ScriptException("Can't evaluate text information", font);
    }
}

///@ ExportMethod
FO_SCRIPT_API int Client_Game_GetTextLines(FOClient* client, isize size, int font)
{
    return client->SprMngr.GetLinesCount(size, "", font);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint sprId, ipos pos)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0) {
        return;
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSprite(spr, pos, COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint sprId, ipos pos, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0) {
        return;
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSprite(spr, pos, color != ucolor::clear ? color : COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint sprId, ipos pos, ucolor color, bool offs)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0) {
        return;
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    auto x = pos.x;
    auto y = pos.y;

    if (offs) {
        x += -spr->Size.width / 2 + spr->Offset.x;
        y += -spr->Size.height + spr->Offset.y;
    }

    client->SprMngr.DrawSprite(spr, {x, y}, color != ucolor::clear ? color : COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint sprId, ipos pos, isize size)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0) {
        return;
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSpriteSizeExt(spr, pos, size, true, true, true, COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint sprId, ipos pos, isize size, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0) {
        return;
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSpriteSizeExt(spr, pos, size, true, true, true, color != ucolor::clear ? color : COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint sprId, ipos pos, isize size, ucolor color, bool zoom, bool offs)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0) {
        return;
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    auto x = pos.x;
    auto y = pos.y;

    if (offs) {
        x += spr->Offset.x;
        y += spr->Offset.y;
    }

    client->SprMngr.DrawSpriteSizeExt(spr, {x, y}, size, zoom, true, true, color != ucolor::clear ? color : COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSpritePattern(FOClient* client, uint sprId, ipos pos, isize size, isize sprSize, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0) {
        return;
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSpritePattern(spr, pos, size, sprSize, color != ucolor::clear ? color : COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawText(FOClient* client, string_view text, ipos pos, isize size, ucolor color, int font, uint flags)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (text.empty()) {
        return;
    }

    auto x = pos.x;
    auto y = pos.y;
    auto width = size.width;
    auto height = size.height;

    if (width < 0) {
        width = -width;
        x -= width;
    }
    if (height < 0) {
        height = -height;
        y -= height;
    }

    client->SprMngr.DrawText({x, y, width, height}, text, flags, color != ucolor::clear ? color : COLOR_TEXT, font);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawPrimitive(FOClient* client, int primitiveType, const vector<int>& data)
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
    default:
        throw ScriptException("Invalid primitive type");
    }

    vector<PrimitivePoint> points;
    const auto size = data.size() / 3;
    points.resize(size);

    for (size_t i = 0; i < size; i++) {
        auto& pp = points[i];

        pp.PointPos = {data[i * 3], data[i * 3 + 1]};
        pp.PointColor = ucolor {static_cast<uint>(data[i * 3 + 2])};
        pp.PointOffset = nullptr;
        pp.PPointColor = nullptr;
    }

    client->SprMngr.DrawPoints(points, prim);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawCritter2d(FOClient* client, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim, uint8 dir, int l, int t, int r, int b, bool scratch, bool center, ucolor color)
{
    const auto* frames = client->ResMngr.GetCritterAnimFrames(modelName, stateAnim, actionAnim, dir);

    if (frames != nullptr) {
        client->SprMngr.DrawSpriteSize(frames->GetCurSpr(), {l, t}, {r - l, b - t}, scratch, center, color != ucolor::clear ? color : COLOR_SPRITE);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawCritter3d(FOClient* client, uint instance, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim, const vector<int>& layers, const vector<float>& position, ucolor color)
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

    auto& model_spr = client->DrawCritterModel[instance];

    if (!model_spr || client->DrawCritterModelCrType[instance] != modelName) {
        model_spr = dynamic_pointer_cast<ModelSprite>(client->SprMngr.LoadSprite(modelName, AtlasType::IfaceSprites));

        client->DrawCritterModelCrType[instance] = modelName;
        client->DrawCritterModelFailedToLoad[instance] = false;

        if (!model_spr) {
            client->DrawCritterModelFailedToLoad[instance] = true;
            return;
        }

        auto* model = model_spr->GetModel();

        model->EnableShadow(false);
        model->SetTimer(false);
        model->StartMeshGeneration();
    }

    const auto count = position.size();
    const auto x = count > 0 ? position[0] : 0.0f;
    const auto y = count > 1 ? position[1] : 0.0f;
    const auto rx = count > 2 ? position[2] : 0.0f;
    const auto ry = count > 3 ? position[3] : 0.0f;
    const auto rz = count > 4 ? position[4] : 0.0f;
    const auto sx = count > 5 ? position[5] : 1.0f;
    const auto sy = count > 6 ? position[6] : 1.0f;
    const auto sz = count > 7 ? position[7] : 1.0f;
    const auto speed = count > 8 ? position[8] : 1.0f;
    const auto period = count > 9 ? position[9] : 0.0f;
    const auto stl = count > 10 ? position[10] : 0.0f;
    const auto stt = count > 11 ? position[11] : 0.0f;
    const auto str = count > 12 ? position[12] : 0.0f;
    const auto stb = count > 13 ? position[13] : 0.0f;

    if (count > 13) {
        client->SprMngr.PushScissor(iround(stl), iround(stt), iround(str), iround(stb));
    }
    auto pop_scissor = ScopeCallback([&]() noexcept {
        if (count > 13) {
            safe_call([&] { client->SprMngr.PopScissor(); });
        }
    });

    std::memset(client->DrawCritterModelLayers, 0, sizeof(client->DrawCritterModelLayers));
    for (size_t i = 0, j = layers.size(); i < j && i < MODEL_LAYERS_COUNT; i++) {
        client->DrawCritterModelLayers[i] = layers[i];
    }

    auto* model = model_spr->GetModel();

    model->SetLookDirAngle(0);
    model->SetMoveDirAngle(0, false);
    model->SetRotation(rx * PI_FLOAT / 180.0f, ry * PI_FLOAT / 180.0f, rz * PI_FLOAT / 180.0f);
    model->SetScale(sx, sy, sz);
    model->SetSpeed(speed);
    model->SetAnimation(stateAnim, actionAnim, client->DrawCritterModelLayers, ANIMATION_PERIOD(iround(period * 100.0f)) | ANIMATION_NO_SMOOTH);

    if (count > 13) {
        const auto max_height = iround(stb - stt) * 4 / 3;
        model_spr->SetSize({iround(str - stl), max_height});
    }

    model_spr->DrawToAtlas();

    const auto result_x = iround(x) - model_spr->Size.width / 2 + model_spr->Offset.x;
    const auto result_y = iround(y) - model_spr->Size.height + model_spr->Offset.y;

    client->SprMngr.DrawSprite(model_spr.get(), {result_x, result_y}, color != ucolor::clear ? color : COLOR_SPRITE);

#else
    UNUSED_VARIABLE(client);
    UNUSED_VARIABLE(instance);
    UNUSED_VARIABLE(modelName);
    UNUSED_VARIABLE(stateAnim);
    UNUSED_VARIABLE(actionAnim);
    UNUSED_VARIABLE(layers);
    UNUSED_VARIABLE(position);
    UNUSED_VARIABLE(color);

    throw NotEnabled3DException("3D submodule not enabled");
#endif
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PushDrawScissor(FOClient* client, ipos pos, isize size)
{
    client->SprMngr.PushScissor(pos.x, pos.y, pos.x + size.width, pos.y + size.height);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PopDrawScissor(FOClient* client)
{
    client->SprMngr.PopScissor();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ActivateOffscreenSurface(FOClient* client, bool forceClear)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (client->OffscreenSurfaces.empty()) {
        auto* rt = client->SprMngr.GetRtMngr().CreateRenderTarget(false, RenderTarget::SizeKindType::Screen, {0, 0}, false);
        if (rt == nullptr) {
            throw ScriptException("Can't create offscreen surface");
        }

        client->OffscreenSurfaces.push_back(rt);
    }

    auto* rt = client->OffscreenSurfaces.back();
    client->OffscreenSurfaces.pop_back();
    client->ActiveOffscreenSurfaces.push_back(rt);

    client->SprMngr.GetRtMngr().PushRenderTarget(rt);

    const auto it = std::find(client->DirtyOffscreenSurfaces.begin(), client->DirtyOffscreenSurfaces.end(), rt);
    if (it != client->DirtyOffscreenSurfaces.end() || forceClear) {
        if (it != client->DirtyOffscreenSurfaces.end()) {
            client->DirtyOffscreenSurfaces.erase(it);
        }

        client->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);
    }

    if (std::find(client->PreDirtyOffscreenSurfaces.begin(), client->PreDirtyOffscreenSurfaces.end(), rt) == client->PreDirtyOffscreenSurfaces.end()) {
        client->PreDirtyOffscreenSurfaces.push_back(rt);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(FOClient* client, int effectSubtype)
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

    client->SprMngr.GetRtMngr().PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= static_cast<int>(client->OffscreenEffects.size()) || client->OffscreenEffects[effectSubtype] == nullptr) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->CustomDrawEffect = client->OffscreenEffects[effectSubtype];

    client->SprMngr.DrawRenderTarget(rt, true);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(FOClient* client, int effectSubtype, ipos pos, isize size)
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

    client->SprMngr.GetRtMngr().PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= static_cast<int>(client->OffscreenEffects.size()) || client->OffscreenEffects[effectSubtype] == nullptr) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->CustomDrawEffect = client->OffscreenEffects[effectSubtype];

    const auto l = std::clamp(pos.x, 0, client->Settings.ScreenWidth);
    const auto t = std::clamp(pos.y, 0, client->Settings.ScreenHeight);
    const auto r = std::clamp(pos.x + size.width, 0, client->Settings.ScreenWidth);
    const auto b = std::clamp(pos.y + size.height, 0, client->Settings.ScreenHeight);
    const auto from = IRect(l, t, r, b);
    const auto to = IRect(from);

    client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(FOClient* client, int effectSubtype, int fromX, int fromY, int fromW, int fromH, int toX, int toY, int toW, int toH)
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

    client->SprMngr.GetRtMngr().PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= static_cast<int>(client->OffscreenEffects.size()) || client->OffscreenEffects[effectSubtype] == nullptr) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->CustomDrawEffect = client->OffscreenEffects[effectSubtype];

    const auto from = IRect(std::clamp(fromX, 0, client->Settings.ScreenWidth), //
        std::clamp(fromY, 0, client->Settings.ScreenHeight), //
        std::clamp(fromX + fromW, 0, client->Settings.ScreenWidth), //
        std::clamp(fromY + fromH, 0, client->Settings.ScreenHeight));
    const auto to = IRect(std::clamp(toX, 0, client->Settings.ScreenWidth), //
        std::clamp(toY, 0, client->Settings.ScreenHeight), //
        std::clamp(toX + toW, 0, client->Settings.ScreenWidth), //
        std::clamp(toY + toH, 0, client->Settings.ScreenHeight));

    client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ShowScreen(FOClient* client, int screen)
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

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ShowScreen(FOClient* client, int screen, const map<string, any_t>& data)
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

///@ ExportMethod
FO_SCRIPT_API void Client_Game_HideScreen(FOClient* client)
{
    client->HideScreen(SCREEN_NONE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_HideScreen(FOClient* client, int screen)
{
    client->HideScreen(screen);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SaveScreenshot(FOClient* client, string_view filePath)
{
    UNUSED_VARIABLE(client);
    UNUSED_VARIABLE(filePath);

    throw NotImplementedException(LINE_STR);

    // client->SprMngr.SaveTexture(nullptr, strex(filePath).formatPath(), true);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SaveText(FOClient* client, string_view filePath, string_view text)
{
    UNUSED_VARIABLE(client);

    auto file = DiskFileSystem::OpenFile(strex(filePath).formatPath(), true);
    if (!file) {
        throw ScriptException("Can't open file for writing", filePath);
    }

    if (!file.Write(text)) {
        throw ScriptException("Can't write file", filePath, text.length());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetCacheData(FOClient* client, string_view name, const vector<uint8>& data)
{
    client->Cache.SetData(name, data);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetCacheData(FOClient* client, string_view name, const vector<uint8>& data, uint dataSize)
{
    auto data_copy = data;
    data_copy.resize(dataSize);
    client->Cache.SetData(name, data_copy);
}

///@ ExportMethod
FO_SCRIPT_API vector<uint8> Client_Game_GetCacheData(FOClient* client, string_view name)
{
    return client->Cache.GetData(name);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetCacheText(FOClient* client, string_view name, string_view str)
{
    client->Cache.SetString(name, str);
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetCacheText(FOClient* client, string_view name)
{
    return client->Cache.GetString(name);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsCacheEntry(FOClient* client, string_view name)
{
    return client->Cache.HasEntry(name);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_RemoveCacheEntry(FOClient* client, string_view name)
{
    client->Cache.RemoveEntry(name);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetUserConfig(FOClient* client, const map<string, string>& keyValues)
{
    string cfg_user;

    for (const auto& [key, value] : keyValues) {
        cfg_user += strex("{} = {}\n", key, value);
    }

    client->Cache.SetString(LOCAL_CONFIG_NAME, cfg_user);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetUserConfig(FOClient* client, const vector<string>& keyValues)
{
    string cfg_user;

    for (size_t i = 0; i + 1 < keyValues.size(); i += 2) {
        cfg_user += strex("{} = {}\n", keyValues[i], keyValues[i + 1]);
    }

    client->Cache.SetString(LOCAL_CONFIG_NAME, cfg_user);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetMousePos(FOClient* client, ipos pos)
{
    client->SprMngr.SetMousePosition(pos);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ChangeLanguage(FOClient* client, string_view langName)
{
    client->ChangeLanguage(langName);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_FlashUnfocusedWindow(FOClient* client)
{
    if (!client->SprMngr.IsWindowFocused()) {
        client->SprMngr.BlinkWindow();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Login(FOClient* client, string_view login, string_view password)
{
    client->Connect(login, password, INIT_NET_REASON_LOGIN);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Register(FOClient* client, string_view login, string_view password)
{
    client->Connect(login, password, INIT_NET_REASON_REG);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Connect(FOClient* client)
{
    client->Connect("", "", INIT_NET_REASON_CUSTOM);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Disconnect(FOClient* client)
{
    client->Disconnect();
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_BuiltInCommand(FOClient* client, string_view command)
{
    string result;

    if (!PackNetCommand(
            command, &client->GetConnection().OutBuf,
            [&result](string_view s) {
                result += s;
                result += "\n";
            },
            *client)) {
        return "Unknown command";
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetScreenKeyboard(FOClient* client, bool enabled)
{
    UNUSED_VARIABLE(client, enabled);

    // Todo: improve SetScreenKeyboard
    /*if (SDL_HasScreenKeyboardSupport()) {
        bool cur = (SDL_IsTextInputActive() != SDL_FALSE);
        bool next = strex(args[1]).toBool();
        if (cur != next) {
            if (next)
                SDL_StartTextInput();
            else
                SDL_StopTextInput();
        }
    }*/
}
