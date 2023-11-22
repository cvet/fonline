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

#include "Common.h"

#include "Client.h"
#include "DiskFileSystem.h"
#include "FileSystem.h"
#include "GenericUtils.h"
#include "GeometryHelper.h"
#include "StringUtils.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_IsFullscreen(FOClient* client)
{
    return client->SprMngr.IsFullscreen();
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_ToggleFullscreen(FOClient* client)
{
    client->SprMngr.ToggleFullscreen();
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_MinimizeWindow(FOClient* client)
{
    client->SprMngr.MinimizeWindow();
}

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
[[maybe_unused]] ident_t Client_Game_DeferredCall(FOClient* client, uint delay, ScriptFunc<void> func)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, false, func);
}

///@ ExportMethod
[[maybe_unused]] ident_t Client_Game_DeferredCall(FOClient* client, uint delay, ScriptFunc<void, any_t> func, any_t value)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, false, func, std::move(value));
}

///@ ExportMethod
[[maybe_unused]] ident_t Client_Game_DeferredCall(FOClient* client, uint delay, ScriptFunc<void, vector<any_t>> func, const vector<any_t>& values)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, false, func, values);
}

///@ ExportMethod
[[maybe_unused]] ident_t Client_Game_RepeatingDeferredCall(FOClient* client, uint delay, ScriptFunc<void> func)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, true, func);
}

///@ ExportMethod
[[maybe_unused]] ident_t Client_Game_RepeatingDeferredCall(FOClient* client, uint delay, ScriptFunc<void, any_t> func, any_t value)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, true, func, std::move(value));
}

///@ ExportMethod
[[maybe_unused]] ident_t Client_Game_RepeatingDeferredCall(FOClient* client, uint delay, ScriptFunc<void, vector<any_t>> func, const vector<any_t>& values)
{
    return client->ClientDeferredCalls.AddDeferredCall(delay, true, func, values);
}

///@ ExportMethod
[[maybe_unused]] bool Client_Game_IsDeferredCallPending(FOClient* client, ident_t id)
{
    return client->ClientDeferredCalls.IsDeferredCallPending(id);
}

///@ ExportMethod
[[maybe_unused]] bool Client_Game_CancelDeferredCall(FOClient* client, ident_t id)
{
    return client->ClientDeferredCalls.CancelDeferredCall(id);
}

///# ...
///# param cr1 ...
///# param cr2 ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Game_GetDistance(FOClient* client, CritterView* cr1, CritterView* cr2)
{
    UNUSED_VARIABLE(client);

    if (cr1 == nullptr) {
        throw ScriptException("Critter 1 arg is null");
    }
    if (cr2 == nullptr) {
        throw ScriptException("Critter 2 arg is null");
    }

    const auto* hex_cr1 = dynamic_cast<CritterHexView*>(cr1);
    if (hex_cr1 == nullptr) {
        throw ScriptException("Critter 1 is not on map");
    }

    const auto* hex_cr2 = dynamic_cast<CritterHexView*>(cr2);
    if (hex_cr2 == nullptr) {
        throw ScriptException("Critter 2 is not on map");
    }

    if (hex_cr1->GetMap()->GetId() != hex_cr2->GetMap()->GetId()) {
        throw ScriptException("Critters different maps");
    }

    return GeometryHelper::DistGame(hex_cr1->GetHexX(), hex_cr1->GetHexY(), hex_cr2->GetHexX(), hex_cr2->GetHexY());
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_GetTick(FOClient* client)
{
    return time_duration_to_ms<uint>(client->GameTime.FrameTime().time_since_epoch());
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
    return client->GetChosen();
}

///# ...
///# param itemId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Game_GetItem(FOClient* client, ident_t itemId)
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
                if (!cr->IsChosen()) {
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
            for (auto* cr : client->GetWorldmapCritters()) {
                if (!cr->IsChosen()) {
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

///# ...
///# param critterId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] CritterView* Client_Game_GetCritter(FOClient* client, ident_t critterId)
{
    if (!critterId) {
        return nullptr;
    }

    if (client->CurMap != nullptr) {
        auto* cr = client->CurMap->GetCritter(critterId);
        if (cr == nullptr || cr->IsDestroyed() || cr->IsDestroying()) {
            return nullptr;
        }

        return cr;
    }
    else {
        return client->GetWorldmapCritter(critterId);
    }
}

///# ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Game_GetCritters(FOClient* client, CritterFindType findType)
{
    vector<CritterView*> critters;

    if (client->CurMap != nullptr) {
        for (auto* cr : client->CurMap->GetCritters()) {
            if (cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }
    else {
        critters = client->GetWorldmapCritters();
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
                if (cr->IsNpc() && cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                    critters.push_back(cr);
                }
            }
        }
    }
    else {
        if (!pid) {
            for (auto* cr : client->GetWorldmapCritters()) {
                if (cr->CheckFind(findType)) {
                    critters.push_back(cr);
                }
            }
        }
        else {
            for (auto* cr : client->GetWorldmapCritters()) {
                if (cr->IsNpc() && cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                    critters.push_back(cr);
                }
            }
        }
    }

    return critters;
}

///# ...
///# param fromColor ...
///# param toColor ...
///# param duration ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_FlushScreen(FOClient* client, ucolor fromColor, ucolor toColor, tick_t duration)
{
    client->ScreenFade(std::chrono::milliseconds {duration.underlying_value()}, fromColor, toColor, true);
}

///# ...
///# param noise ...
///# param ms ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_QuakeScreen(FOClient* client, int noise, tick_t duration)
{
    client->ScreenQuake(noise, std::chrono::milliseconds {duration.underlying_value()});
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
[[maybe_unused]] bool Client_Game_PlayMusic(FOClient* client, string_view musicName, tick_t repeatTime)
{
    if (musicName.empty()) {
        client->SndMngr.StopMusic();
        return true;
    }

    return client->SndMngr.PlayMusic(musicName, std::chrono::milliseconds {repeatTime.underlying_value()});
}

///# ...
///# param videoName ...
///# param canInterrupt ...
///# param enqueue
///@ ExportMethod
[[maybe_unused]] void Client_Game_PlayVideo(FOClient* client, string_view videoName, bool canInterrupt, bool enqueue)
{
    client->PlayVideo(videoName, canInterrupt, enqueue);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_IsVideoPlaying(FOClient* client)
{
    return client->IsVideoPlaying();
}

///# ...
///# param videoName ...
///# param looped ...
///# return ...
///@ ExportMethod PassOwnership
[[maybe_unused]] VideoPlayback* Client_Game_CreateVideoPlayback(FOClient* client, string_view videoName, bool looped)
{
    const auto file = client->Resources.ReadFile(videoName);
    if (!file) {
        throw ScriptException("Video file not found", videoName);
    }

    auto&& clip = std::make_unique<VideoClip>(file.GetData());
    auto&& tex = unique_ptr<RenderTexture> {App->Render.CreateTexture(std::get<0>(clip->GetSize()), std::get<1>(clip->GetSize()), true, false)};

    clip->SetLooped(looped);

    auto* video = new VideoPlayback();
    video->Clip = std::move(clip);
    video->Tex = std::move(tex);
    return video;
}

///# ...
///# param video ...
///# param x ...
///# param y ...
///# param width ...
///# param height ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawVideoPlayback(FOClient* client, VideoPlayback* video, int x, int y, int width, int height)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (video == nullptr || !video->Clip) {
        return;
    }

    if (width > 0 && height > 0) {
        video->Tex->UpdateTextureRegion({0, 0, video->Tex->Width, video->Tex->Height}, video->Clip->RenderFrame().data());

        const auto r = IRect {x, y, x + width, y + height};
        client->SprMngr.DrawTexture(video->Tex.get(), false, nullptr, &r);
    }

    if (video->Clip->IsStopped()) {
        video->Clip.reset();
        video->Tex.reset();
        video->Stopped = true;
    }
}

///# ...
///# param msg ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_ConsoleMessage(FOClient* client, string_view msg)
{
    client->ConsoleMessage(msg);
}

///# ...
///# param msg ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_Message(FOClient* client, string_view msg)
{
    client->AddMessage(FOMB_GAME, msg);
}

///# ...
///# param type ...
///# param msg ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_Message(FOClient* client, int type, string_view msg)
{
    client->AddMessage(type, msg);
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

    client->AddMessage(FOMB_GAME, client->GetCurLang().Msg[textMsg].GetStr(strNum));
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

    client->AddMessage(type, client->GetCurLang().Msg[textMsg].GetStr(strNum));
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
    UNUSED_VARIABLE(client);

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
///# param zoneX ...
///# param zoneY ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] int Client_Game_GetFog(FOClient* client, uint16 zoneX, uint16 zoneY)
{
    if (zoneX >= client->Settings.GlobalMapWidth || zoneY >= client->Settings.GlobalMapHeight) {
        throw ScriptException("Invalid world map pos arg");
    }
    return client->GetWorldmapFog().Get2Bit(zoneX, zoneY);
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] tick_t Client_Game_GetFullSecond(FOClient* client)
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
[[maybe_unused]] tick_t Client_Game_EvaluateFullSecond(FOClient* client, uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)
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
        const auto month_day = client->GameTime.GameTimeMonthDays(year, month_);
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
[[maybe_unused]] void Client_Game_EvaluateGameTime(FOClient* client, tick_t fullSecond, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second)
{
    const auto dt = client->GameTime.EvaluateGameTime(fullSecond);
    year = dt.Year;
    month = dt.Month;
    dayOfWeek = dt.DayOfWeek;
    day = dt.Day;
    hour = dt.Hour;
    minute = dt.Minute;
    second = dt.Second;
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

///# ...
///# param font ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SetDefaultFont(FOClient* client, int font)
{
    client->SprMngr.SetDefaultFont(font);
}

///# ...
///# param effectType ...
///# param effectSubtype ...
///# param effectPath ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SetEffect(FOClient* client, EffectType effectType, int64 effectSubtype, string_view effectPath)
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

///# ...
///# param x ...
///# param y ...
///# param button ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SimulateMouseClick(FOClient* client, int x, int y, MouseButton button)
{
    UNUSED_VARIABLE(client);
    UNUSED_VARIABLE(x);
    UNUSED_VARIABLE(y);
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
///# param year ...
///# param month ...
///# param day ...
///# param dayOfWeek ...
///# param hour ...
///# param minute ...
///# param second ...
///# param milliseconds ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Client_Game_GetTime(FOClient* client, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second, uint16& milliseconds)
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

///# ...
///# param sprName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_LoadSprite(FOClient* client, string_view sprName)
{
    return client->AnimLoad(client->ToHashedString(sprName), AtlasType::IfaceSprites);
}

///# ...
///# param nameHash ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_LoadSprite(FOClient* client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::IfaceSprites);
}

///# ...
///# param sprName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_LoadMapSprite(FOClient* client, string_view sprName)
{
    return client->AnimLoad(client->ToHashedString(sprName), AtlasType::MapSprites);
}

///# ...
///# param nameHash ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Game_LoadMapSprite(FOClient* client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::MapSprites);
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
///# return ...
///@ ExportMethod
[[maybe_unused]] int Client_Game_GetSpriteWidth(FOClient* client, uint sprId)
{
    const auto* spr = client->AnimGetSpr(sprId);
    if (spr == nullptr) {
        return 0;
    }

    return spr->Width;
}

///# ...
///# param sprId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Client_Game_GetSpriteHeight(FOClient* client, uint sprId)
{
    const auto* spr = client->AnimGetSpr(sprId);
    if (spr == nullptr) {
        return 0;
    }

    return spr->Height;
}

///# ...
///# param sprId ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Game_IsSpriteHit(FOClient* client, uint sprId, int x, int y)
{
    if (sprId == 0) {
        return false;
    }

    const auto* spr = client->AnimGetSpr(sprId);
    if (spr == nullptr) {
        return false;
    }

    return client->SprMngr.SpriteHitTest(spr, x, y, false);
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_StopSprite(FOClient* client, uint sprId)
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

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SetSpriteTime(FOClient* client, uint sprId, float normalizedTime)
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

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_PlaySprite(FOClient* client, uint sprId, hstring animName, bool looped, bool reversed)
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
///# param x ...
///# param y ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawSprite(FOClient* client, uint sprId, int x, int y)
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

    client->SprMngr.DrawSprite(spr, x, y, COLOR_SPRITE);
}

///# ...
///# param sprId ...
///# param x ...
///# param y ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawSprite(FOClient* client, uint sprId, int x, int y, ucolor color)
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

    client->SprMngr.DrawSprite(spr, x, y, color != ucolor::clear ? color : COLOR_SPRITE);
}

///# ...
///# param sprId ...
///# param x ...
///# param y ...
///# param color ...
///# param offs ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawSprite(FOClient* client, uint sprId, int x, int y, ucolor color, bool offs)
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

    auto xx = x;
    auto yy = y;

    if (offs) {
        xx += -spr->Width / 2 + spr->OffsX;
        yy += -spr->Height + spr->OffsY;
    }

    client->SprMngr.DrawSprite(spr, xx, yy, color != ucolor::clear ? color : COLOR_SPRITE);
}

///# ...
///# param sprId ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawSprite(FOClient* client, uint sprId, int x, int y, int w, int h)
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

    client->SprMngr.DrawSpriteSizeExt(spr, x, y, w, h, true, true, true, COLOR_SPRITE);
}

///# ...
///# param sprId ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawSprite(FOClient* client, uint sprId, int x, int y, int w, int h, ucolor color)
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

    client->SprMngr.DrawSpriteSizeExt(spr, x, y, w, h, true, true, true, color != ucolor::clear ? color : COLOR_SPRITE);
}

///# ...
///# param sprId ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///# param color ...
///# param zoom ...
///# param offs ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawSprite(FOClient* client, uint sprId, int x, int y, int w, int h, ucolor color, bool zoom, bool offs)
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

    auto xx = x;
    auto yy = y;

    if (offs) {
        xx += spr->OffsX;
        yy += spr->OffsY;
    }

    client->SprMngr.DrawSpriteSizeExt(spr, xx, yy, w, h, zoom, true, true, color != ucolor::clear ? color : COLOR_SPRITE);
}

///# ...
///# param sprId ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///# param sprWidth ...
///# param sprHeight ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawSpritePattern(FOClient* client, uint sprId, int x, int y, int w, int h, int sprWidth, int sprHeight, ucolor color)
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

    client->SprMngr.DrawSpritePattern(spr, x, y, w, h, sprWidth, sprHeight, color != ucolor::clear ? color : COLOR_SPRITE);
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
[[maybe_unused]] void Client_Game_DrawText(FOClient* client, string_view text, int x, int y, int w, int h, ucolor color, int font, int flags)
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
    client->SprMngr.DrawStr(r, text, flags, color != ucolor::clear ? color : COLOR_TEXT, font);
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
    default:
        throw ScriptException("Invalid primitive type");
    }

    vector<PrimitivePoint> points;
    const auto size = data.size() / 3;
    points.resize(size);

    for (size_t i = 0; i < size; i++) {
        auto& pp = points[i];
        pp.PointX = data[i * 3];
        pp.PointY = data[i * 3 + 1];
        pp.PointColor = ucolor {static_cast<uint>(data[i * 3 + 2])};
        pp.PointOffsX = nullptr;
        pp.PointOffsY = nullptr;
    }

    client->SprMngr.DrawPoints(points, prim);
}

///# ...
///# param modelName ...
///# param stateAnim ...
///# param actionAnim ...
///# param dir ...
///# param l ...
///# param t ...
///# param r ...
///# param b ...
///# param scratch ...
///# param center ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawCritter2d(FOClient* client, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim, uint8 dir, int l, int t, int r, int b, bool scratch, bool center, ucolor color)
{
    const auto* frames = client->ResMngr.GetCritterAnimFrames(modelName, stateAnim, actionAnim, dir);
    if (frames != nullptr) {
        client->SprMngr.DrawSpriteSize(frames->GetCurSpr(), l, t, r - l, b - t, scratch, center, color != ucolor::clear ? color : COLOR_SPRITE);
    }
}

///# ...
///# param instance ...
///# param modelName ...
///# param stateAnim ...
///# param actionAnim ...
///# param layers ...
///# param position ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_DrawCritter3d(FOClient* client, uint instance, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim, const vector<int>& layers, const vector<float>& position, ucolor color)
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
    for (size_t i = 0, j = layers.size(); i < j && i < MODEL_LAYERS_COUNT; i++) {
        client->DrawCritterModelLayers[i] = layers[i];
    }

    auto* model = model_spr->GetModel();

    model->SetLookDirAngle(0);
    model->SetMoveDirAngle(0, false);
    model->SetRotation(rx * PI_FLOAT / 180.0f, ry * PI_FLOAT / 180.0f, rz * PI_FLOAT / 180.0f);
    model->SetScale(sx, sy, sz);
    model->SetSpeed(speed);
    model->SetAnimation(stateAnim, actionAnim, client->DrawCritterModelLayers, ANIMATION_PERIOD(static_cast<int>(period * 100.0f)) | ANIMATION_NO_SMOOTH);

    model_spr->DrawToAtlas();

    client->SprMngr.DrawSprite(model_spr.get(), static_cast<int>(x) - model_spr->Width / 2 + model_spr->OffsX, static_cast<int>(y) - model_spr->Height + model_spr->OffsY, color != ucolor::clear ? color : COLOR_SPRITE);

    if (count > 13) {
        client->SprMngr.PopScissor();
    }

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
        auto* rt = client->SprMngr.GetRtMngr().CreateRenderTarget(false, RenderTarget::SizeType::Screen, 0, 0, false);
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

    client->SprMngr.GetRtMngr().PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= static_cast<int>(client->OffscreenEffects.size()) || client->OffscreenEffects[effectSubtype] == nullptr) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->CustomDrawEffect = client->OffscreenEffects[effectSubtype];

    client->SprMngr.DrawRenderTarget(rt, true);
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

    client->SprMngr.GetRtMngr().PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= static_cast<int>(client->OffscreenEffects.size()) || client->OffscreenEffects[effectSubtype] == nullptr) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->CustomDrawEffect = client->OffscreenEffects[effectSubtype];

    const auto from = IRect(std::clamp(x, 0, client->Settings.ScreenWidth), std::clamp(y, 0, client->Settings.ScreenHeight), std::clamp(x + w, 0, client->Settings.ScreenWidth), std::clamp(y + h, 0, client->Settings.ScreenHeight));
    const auto to = IRect(from);
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

    client->SprMngr.GetRtMngr().PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= static_cast<int>(client->OffscreenEffects.size()) || client->OffscreenEffects[effectSubtype] == nullptr) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->CustomDrawEffect = client->OffscreenEffects[effectSubtype];

    const auto from = IRect(std::clamp(fromX, 0, client->Settings.ScreenWidth), std::clamp(fromY, 0, client->Settings.ScreenHeight), std::clamp(fromX + fromW, 0, client->Settings.ScreenWidth), std::clamp(fromY + fromH, 0, client->Settings.ScreenHeight));
    const auto to = IRect(std::clamp(toX, 0, client->Settings.ScreenWidth), std::clamp(toY, 0, client->Settings.ScreenHeight), std::clamp(toX + toW, 0, client->Settings.ScreenWidth), std::clamp(toY + toH, 0, client->Settings.ScreenHeight));
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
[[maybe_unused]] void Client_Game_ShowScreen(FOClient* client, int screen, const map<string, any_t>& data)
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
///# param filePath ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SaveScreenshot(FOClient* client, string_view filePath)
{
    UNUSED_VARIABLE(client);
    UNUSED_VARIABLE(filePath);

    throw NotImplementedException(LINE_STR);

    // client->SprMngr.SaveTexture(nullptr, _str(filePath).formatPath(), true);
}

///# ...
///# param filePath ...
///# param text ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SaveText(FOClient* client, string_view filePath, string_view text)
{
    UNUSED_VARIABLE(client);

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
[[maybe_unused]] void Client_Game_SetCacheData(FOClient* client, string_view name, const vector<uint8>& data)
{
    client->Cache.SetData(name, data);
}

///# ...
///# param name ...
///# param data ...
///# param dataSize ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SetCacheData(FOClient* client, string_view name, const vector<uint8>& data, uint dataSize)
{
    auto data_copy = data;
    data_copy.resize(dataSize);
    client->Cache.SetData(name, data_copy);
}

///# ...
///# param name ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<uint8> Client_Game_GetCacheData(FOClient* client, string_view name)
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

    client->Cache.SetString(LOCAL_CONFIG_NAME, cfg_user);
}

///# ...
///# param keyValues ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SetUserConfig(FOClient* client, const vector<string>& keyValues)
{
    string cfg_user;
    for (size_t i = 0; i + 1 < keyValues.size(); i += 2) {
        cfg_user += _str("{} = {}\n", keyValues[i], keyValues[i + 1]).str();
    }

    client->Cache.SetString(LOCAL_CONFIG_NAME, cfg_user);
}

///# ...
///# param x ...
///# param y ...
///@ ExportMethod
[[maybe_unused]] void Client_Game_SetMousePos(FOClient* client, int x, int y)
{
    client->SprMngr.SetMousePosition(x, y);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Client_Game_GetCurLang(FOClient* client)
{
    return client->GetCurLang().Name;
}
