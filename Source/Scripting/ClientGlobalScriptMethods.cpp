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

#include "Common.h"

#include "Client.h"
#include "FileSystem.h"
#include "Geometry.h"
#include "NetCommand.h"

FO_BEGIN_NAMESPACE();

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
FO_SCRIPT_API int32 Client_Game_GetDistance(FOClient* client, CritterView* cr1, CritterView* cr2)
{
    ignore_unused(client);

    if (cr1 == nullptr) {
        throw ScriptException("Critter 1 arg is null");
    }
    if (cr2 == nullptr) {
        throw ScriptException("Critter 2 arg is null");
    }

    const auto* hex_cr1 = dynamic_cast<CritterHexView*>(cr1);
    const auto* hex_cr2 = dynamic_cast<CritterHexView*>(cr2);

    if (hex_cr1 != nullptr && hex_cr2 != nullptr && hex_cr1->GetMapId() == hex_cr2->GetMapId()) {
        const auto dist = GeometryHelper::GetDistance(hex_cr1->GetHex(), hex_cr2->GetHex());
        const auto multihex = cr1->GetMultihex() + cr2->GetMultihex();
        return multihex < dist ? dist - multihex : 0;
    }
    else {
        throw ScriptException("Critters not on map");
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Client_Game_GetDistance(FOClient* client, ItemView* item1, ItemView* item2)
{
    ignore_unused(client);

    if (item1 == nullptr) {
        throw ScriptException("Item 1 arg is null");
    }
    if (item2 == nullptr) {
        throw ScriptException("Item 2 arg is null");
    }

    const auto* hex_item1 = dynamic_cast<ItemHexView*>(item1);
    const auto* hex_item2 = dynamic_cast<ItemHexView*>(item2);

    if (hex_item1 != nullptr && hex_item2 != nullptr && hex_item1->GetMapId() == hex_item2->GetMapId()) {
        const auto dist = GeometryHelper::GetDistance(hex_item1->GetHex(), hex_item2->GetHex());
        return dist;
    }
    else {
        throw ScriptException("Items not on map");
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Client_Game_GetDistance(FOClient* client, CritterView* cr, ItemView* item)
{
    ignore_unused(client);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }

    const auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    const auto* hex_item = dynamic_cast<ItemHexView*>(item);

    if (hex_cr != nullptr && hex_item != nullptr && hex_cr->GetMapId() == hex_item->GetMapId()) {
        const auto dist = GeometryHelper::GetDistance(hex_cr->GetHex(), hex_item->GetHex());
        const auto multihex = hex_cr->GetMultihex();
        return multihex < dist ? dist - multihex : 0;
    }
    else {
        throw ScriptException("Critter/Item not on map");
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Client_Game_GetDistance(FOClient* client, ItemView* item, CritterView* cr)
{
    ignore_unused(client);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }

    const auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    const auto* hex_item = dynamic_cast<ItemHexView*>(item);

    if (hex_cr != nullptr && hex_item != nullptr && hex_cr->GetMapId() == hex_item->GetMapId()) {
        const auto dist = GeometryHelper::GetDistance(hex_cr->GetHex(), hex_item->GetHex());
        const auto multihex = hex_cr->GetMultihex();
        return multihex < dist ? dist - multihex : 0;
    }
    else {
        throw ScriptException("Item/Critter not on map");
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Client_Game_GetDistance(FOClient* client, CritterView* cr, mpos hex)
{
    ignore_unused(client);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    const auto* hex_cr = dynamic_cast<CritterHexView*>(cr);

    if (hex_cr != nullptr) {
        const auto dist = GeometryHelper::GetDistance(hex_cr->GetHex(), hex);
        const auto multihex = hex_cr->GetMultihex();
        return multihex < dist ? dist - multihex : 0;
    }
    else {
        throw ScriptException("Critter not on map");
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Client_Game_GetDistance(FOClient* client, mpos hex, CritterView* cr)
{
    ignore_unused(client);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    const auto* hex_cr = dynamic_cast<CritterHexView*>(cr);

    if (hex_cr != nullptr) {
        const auto dist = GeometryHelper::GetDistance(hex_cr->GetHex(), hex);
        const auto multihex = hex_cr->GetMultihex();
        return multihex < dist ? dist - multihex : 0;
    }
    else {
        throw ScriptException("Critter not on map");
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Client_Game_GetDistance(FOClient* client, mpos hex, ItemView* item)
{
    ignore_unused(client);

    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }

    const auto* hex_item = dynamic_cast<ItemHexView*>(item);

    if (hex_item != nullptr) {
        const auto dist = GeometryHelper::GetDistance(hex_item->GetHex(), hex);
        return dist;
    }
    else {
        throw ScriptException("Item not on map");
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Client_Game_GetDistance(FOClient* client, ItemView* item, mpos hex)
{
    ignore_unused(client);

    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }

    const auto* hex_item = dynamic_cast<ItemHexView*>(item);

    if (hex_item != nullptr) {
        const auto dist = GeometryHelper::GetDistance(hex_item->GetHex(), hex);
        return dist;
    }
    else {
        throw ScriptException("Item not on map");
    }
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

///@ ExportMethod
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
    if (client->GetCurMap() != nullptr) {
        if (item == nullptr) {
            item = client->GetCurMap()->GetItem(itemId);
        }

        if (item == nullptr) {
            for (auto& cr : client->GetCurMap()->GetCritters()) {
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
            for (auto& cr : client->GetGlobalMapCritters()) {
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

///@ ExportMethod
FO_SCRIPT_API CritterView* Client_Game_GetCritter(FOClient* client, ident_t crId)
{
    if (!crId) {
        return nullptr;
    }

    if (client->GetCurMap() != nullptr) {
        auto* cr = client->GetCurMap()->GetCritter(crId);
        if (cr == nullptr || cr->IsDestroyed() || cr->IsDestroying()) {
            return nullptr;
        }

        return cr;
    }
    else {
        return client->GetGlobalMapCritter(crId);
    }
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Game_GetCritters(FOClient* client, CritterFindType findType)
{
    vector<CritterView*> critters;

    if (client->GetCurMap() != nullptr) {
        const auto map_critters = client->GetCurMap()->GetCritters();
        critters.reserve(map_critters.size());

        for (auto& cr : map_critters) {
            if (cr->CheckFind(findType)) {
                critters.emplace_back(cr.get());
            }
        }
    }
    else {
        auto& gmap_critters = client->GetGlobalMapCritters();
        critters.reserve(gmap_critters.size());

        for (auto& cr : gmap_critters) {
            if (cr->CheckFind(findType)) {
                critters.emplace_back(cr.get());
            }
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Game_GetCritters(FOClient* client, hstring pid, CritterFindType findType)
{
    vector<CritterView*> critters;

    if (client->GetCurMap() != nullptr) {
        if (!pid) {
            for (auto& cr : client->GetCurMap()->GetCritters()) {
                if (cr->CheckFind(findType)) {
                    critters.emplace_back(cr.get());
                }
            }
        }
        else {
            for (auto& cr : client->GetCurMap()->GetCritters()) {
                if (cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                    critters.emplace_back(cr.get());
                }
            }
        }
    }
    else {
        if (!pid) {
            for (auto& cr : client->GetGlobalMapCritters()) {
                if (cr->CheckFind(findType)) {
                    critters.emplace_back(cr.get());
                }
            }
        }
        else {
            for (auto& cr : client->GetGlobalMapCritters()) {
                if (cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                    critters.emplace_back(cr.get());
                }
            }
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Game_SortCrittersByDeep(FOClient* client, const vector<CritterView*>& critters)
{
    ignore_unused(client);

    vector<CritterView*> sorted_critters = critters;

    std::ranges::stable_sort(sorted_critters, [](const CritterView* cr1, const CritterView* cr2) {
        const auto cr1_pos = cr1->GetHex();
        const auto cr2_pos = cr2->GetHex();

        if (cr1_pos.y == cr2_pos.y) {
            if (cr1_pos.x == cr2_pos.x) {
                const auto* cr1_hex = dynamic_cast<const CritterHexView*>(cr1);
                const auto* cr2_hex = dynamic_cast<const CritterHexView*>(cr2);

                if (cr1_hex != nullptr && cr2_hex != nullptr && cr1_hex->IsMapSpriteValid() && cr2_hex->IsMapSpriteValid()) {
                    return cr1_hex->GetMapSprite()->GetSortValue() < cr2_hex->GetMapSprite()->GetSortValue();
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
FO_SCRIPT_API void Client_Game_FadeScreen(FOClient* client, ucolor fromColor, ucolor toColor, timespan duration)
{
    client->ScreenFade(duration, fromColor, toColor, false);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_FadeScreen(FOClient* client, ucolor fromColor, ucolor toColor, timespan duration, bool appendEffect)
{
    client->ScreenFade(duration, fromColor, toColor, appendEffect);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_QuakeScreen(FOClient* client, int32 noise, timespan duration)
{
    client->ScreenQuake(noise, duration);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_PlaySound(FOClient* client, string_view soundName)
{
    return client->SndMngr.PlaySound(client->ResMngr.GetSoundNames(), soundName);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_PlayMusic(FOClient* client, string_view musicName, timespan repeatTime)
{
    if (musicName.empty()) {
        client->SndMngr.StopMusic();
        return true;
    }

    return client->SndMngr.PlayMusic(musicName, repeatTime);
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

    auto clip = SafeAlloc::MakeUnique<VideoClip>(file.GetData());
    auto tex = App->Render.CreateTexture(clip->GetSize(), true, false);

    clip->SetLooped(looped);

    auto video = SafeAlloc::MakeRefCounted<VideoPlayback>();

    video->Clip = std::move(clip);
    video->Tex = std::move(tex);

    video->AddRef();
    return video.get();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawVideoPlayback(FOClient* client, VideoPlayback* video, ipos32 pos, isize32 size)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (video == nullptr || !video->Clip) {
        return;
    }

    if (size.width > 0 && size.height > 0) {
        video->Tex->UpdateTextureRegion({}, video->Tex->Size, video->Clip->RenderFrame().data());

        const auto r = irect32 {pos.x, pos.y, size.width, size.height};
        client->SprMngr.DrawTexture(video->Tex.get(), false, nullptr, &r);
    }

    if (video->Clip->IsStopped()) {
        video->Clip.reset();
        video->Tex.reset();
        video->Stopped = true;
    }
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetText(FOClient* client, TextPackName textPack, uint32 strNum)
{
    return client->GetCurLang().GetTextPack(textPack).GetStr(strNum);
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetText(FOClient* client, TextPackName textPack, uint32 strNum, int32 skipCount)
{
    return client->GetCurLang().GetTextPack(textPack).GetStr(strNum, skipCount);
}

///@ ExportMethod
FO_SCRIPT_API uint32 Client_Game_GetTextNumUpper(FOClient* client, TextPackName textPack, uint32 strNum)
{
    return client->GetCurLang().GetTextPack(textPack).GetStrNumUpper(strNum);
}

///@ ExportMethod
FO_SCRIPT_API uint32 Client_Game_GetTextNumLower(FOClient* client, TextPackName textPack, uint32 strNum)
{
    return client->GetCurLang().GetTextPack(textPack).GetStrNumLower(strNum);
}

///@ ExportMethod
FO_SCRIPT_API uint32 Client_Game_GetTextCount(FOClient* client, TextPackName textPack, uint32 strNum)
{
    return numeric_cast<uint32>(client->GetCurLang().GetTextPack(textPack).GetStrCount(strNum));
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsTextPresent(FOClient* client, TextPackName textPack, uint32 strNum)
{
    return client->GetCurLang().GetTextPack(textPack).GetStrCount(strNum) != 0;
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_ReplaceText(FOClient* client, string_view text, string_view from, string_view to)
{
    ignore_unused(client);

    return strex(text).replace(from, to);
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_ReplaceText(FOClient* client, string_view text, string_view from, int64 to)
{
    ignore_unused(client);

    return strex(text).replace(from, strex("{}", to));
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Preload3dFiles(FOClient* client, const vector<string>& fnames)
{
#if FO_ENABLE_3D
    auto* model_spr_factory = dynamic_cast<ModelSpriteFactory*>(client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)));
    FO_RUNTIME_ASSERT(model_spr_factory);

    for (const auto& fname : fnames) {
        model_spr_factory->GetModelMngr()->PreloadModel(fname);
    }

#else
    ignore_unused(client, fnames);
    throw NotEnabled3DException("3D submodule not enabled");
#endif
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_LoadFont(FOClient* client, int32 fontIndex, string_view fontFname)
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
FO_SCRIPT_API void Client_Game_SetDefaultFont(FOClient* client, int32 font)
{
    client->SprMngr.SetDefaultFont(font);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetEffect(FOClient* client, EffectType effectType, int64 effectSubtype, string_view effectPath)
{
    const auto reload_effect = [&](raw_ptr<RenderEffect> def_effect) {
        if (!effectPath.empty()) {
            auto* effect = client->EffectMngr.LoadEffect(def_effect->GetUsage(), effectPath);

            if (effect == nullptr) {
                throw ScriptException("Effect not found or have some errors, see log file");
            }

            return effect;
        }

        return def_effect.get();
    };

    const auto eff_type = static_cast<uint32>(effectType);

    if (((eff_type & static_cast<uint32>(EffectType::GenericSprite)) != 0) && effectSubtype != 0) {
        auto* item = client->GetCurMap()->GetItem(ident_t {static_cast<uint32>(effectSubtype)});

        if (item != nullptr) {
            item->SetDrawEffect(reload_effect(client->EffectMngr.Effects.Generic));
        }
    }
    if (((eff_type & static_cast<uint32>(EffectType::CritterSprite)) != 0) && effectSubtype != 0) {
        auto* cr = client->GetCurMap()->GetCritter(ident_t {static_cast<uint32>(effectSubtype)});

        if (cr != nullptr) {
            cr->SetDrawEffect(reload_effect(client->EffectMngr.Effects.Critter));
        }
    }

    if (((eff_type & static_cast<uint32>(EffectType::GenericSprite)) != 0) && effectSubtype == 0) {
        client->EffectMngr.Effects.Generic = reload_effect(client->EffectMngr.Effects.GenericDefault);
    }
    if (((eff_type & static_cast<uint32>(EffectType::CritterSprite)) != 0) && effectSubtype == 0) {
        client->EffectMngr.Effects.Critter = reload_effect(client->EffectMngr.Effects.CritterDefault);
    }
    if ((eff_type & static_cast<uint32>(EffectType::TileSprite)) != 0) {
        client->EffectMngr.Effects.Tile = reload_effect(client->EffectMngr.Effects.TileDefault);
    }
    if ((eff_type & static_cast<uint32>(EffectType::RoofSprite)) != 0) {
        client->EffectMngr.Effects.Roof = reload_effect(client->EffectMngr.Effects.RoofDefault);
    }
    if ((eff_type & static_cast<uint32>(EffectType::RainSprite)) != 0) {
        client->EffectMngr.Effects.Rain = reload_effect(client->EffectMngr.Effects.RainDefault);
    }

#if FO_ENABLE_3D
    if ((eff_type & static_cast<uint32>(EffectType::SkinnedMesh)) != 0) {
        client->EffectMngr.Effects.SkinnedModel = reload_effect(client->EffectMngr.Effects.SkinnedModelDefault);
    }
#endif

    if ((eff_type & static_cast<uint32>(EffectType::Interface)) != 0) {
        client->EffectMngr.Effects.Iface = reload_effect(client->EffectMngr.Effects.IfaceDefault);
    }

    if ((eff_type & static_cast<uint32>(EffectType::Contour)) != 0) {
        client->EffectMngr.Effects.Contour = reload_effect(client->EffectMngr.Effects.ContourDefault);
    }

    if (((eff_type & static_cast<uint32>(EffectType::Font)) != 0) && effectSubtype == -1) {
        client->EffectMngr.Effects.Font = reload_effect(client->EffectMngr.Effects.FontDefault);
    }
    if (((eff_type & static_cast<uint32>(EffectType::Font)) != 0) && effectSubtype >= 0) {
        client->SprMngr.SetFontEffect(static_cast<int32>(effectSubtype), reload_effect(client->EffectMngr.Effects.Font));
    }

    if ((eff_type & static_cast<uint32>(EffectType::Primitive)) != 0) {
        client->EffectMngr.Effects.Primitive = reload_effect(client->EffectMngr.Effects.PrimitiveDefault);
    }
    if ((eff_type & static_cast<uint32>(EffectType::Light)) != 0) {
        client->EffectMngr.Effects.Light = reload_effect(client->EffectMngr.Effects.LightDefault);
    }
    if ((eff_type & static_cast<uint32>(EffectType::Fog)) != 0) {
        client->EffectMngr.Effects.Fog = reload_effect(client->EffectMngr.Effects.FogDefault);
    }

    if ((eff_type & static_cast<uint32>(EffectType::FlushRenderTarget)) != 0) {
        client->EffectMngr.Effects.FlushRenderTarget = reload_effect(client->EffectMngr.Effects.FlushRenderTargetDefault);
    }
    if ((eff_type & static_cast<uint32>(EffectType::FlushPrimitive)) != 0) {
        client->EffectMngr.Effects.FlushPrimitive = reload_effect(client->EffectMngr.Effects.FlushPrimitiveDefault);
    }
    if ((eff_type & static_cast<uint32>(EffectType::FlushMap)) != 0) {
        client->EffectMngr.Effects.FlushMap = reload_effect(client->EffectMngr.Effects.FlushMapDefault);
    }
    if ((eff_type & static_cast<uint32>(EffectType::FlushLight)) != 0) {
        client->EffectMngr.Effects.FlushLight = reload_effect(client->EffectMngr.Effects.FlushLightDefault);
    }
    if ((eff_type & static_cast<uint32>(EffectType::FlushFog)) != 0) {
        client->EffectMngr.Effects.FlushFog = reload_effect(client->EffectMngr.Effects.FlushFogDefault);
    }

    if ((eff_type & static_cast<uint32>(EffectType::Offscreen)) != 0) {
        if (effectSubtype < 0) {
            throw ScriptException("Negative effect subtype");
        }

        client->OffscreenEffects.resize(static_cast<size_t>(effectSubtype) + 1);
        client->OffscreenEffects[static_cast<size_t>(effectSubtype)] = reload_effect(client->EffectMngr.Effects.GenericDefault);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateMouseClick(FOClient* client, ipos32 pos, MouseButton button)
{
    ignore_unused(client);
    ignore_unused(pos);
    ignore_unused(button);

    throw NotImplementedException(FO_LINE_STR);

    /*App->Input.PushEvent({InputEvent::MouseDown({(MouseButton)button})});

    IntVec prev_events = client->Settings.MainWindowMouseEvents;
    client->Settings.MainWindowMouseEvents.clear();
    int32 prev_x = client->Settings.MouseX;
    int32 prev_y = client->Settings.MouseY;
    int32 last_prev_x = client->Settings.LastMouseX;
    int32 last_prev_y = client->Settings.LastMouseY;
    client->Settings.MouseX = client->Settings.LastMouseX = x;
    client->Settings.MouseY = client->Settings.LastMouseY = y;
    client->Settings.MainWindowMouseEvents.emplace_back(SDL_MOUSEBUTTONDOWN);
    client->Settings.MainWindowMouseEvents.emplace_back(button);
    client->Settings.MainWindowMouseEvents.emplace_back(SDL_MOUSEBUTTONUP);
    client->Settings.MainWindowMouseEvents.emplace_back(button);
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

///@ ExportMethod
FO_SCRIPT_API uint32 Client_Game_LoadSprite(FOClient* client, string_view sprName)
{
    return client->AnimLoad(client->Hashes.ToHashedString(sprName), AtlasType::IfaceSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint32 Client_Game_LoadSprite(FOClient* client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::IfaceSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint32 Client_Game_LoadMapSprite(FOClient* client, string_view sprName)
{
    return client->AnimLoad(client->Hashes.ToHashedString(sprName), AtlasType::MapSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint32 Client_Game_LoadMapSprite(FOClient* client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::MapSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint32 Client_Game_LoadSeparateSprite(FOClient* client, string_view sprName)
{
    return client->AnimLoad(client->Hashes.ToHashedString(sprName), AtlasType::OneImage);
}

///@ ExportMethod
FO_SCRIPT_API uint32 Client_Game_LoadSeparateSprite(FOClient* client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::OneImage);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_FreeSprite(FOClient* client, uint32 sprId)
{
    client->AnimFree(sprId);
}

///@ ExportMethod
FO_SCRIPT_API isize32 Client_Game_GetSpriteSize(FOClient* client, uint32 sprId)
{
    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return {};
    }

    return spr->GetSize();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsSpriteHit(FOClient* client, uint32 sprId, ipos32 pos)
{
    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return false;
    }

    return client->SprMngr.SpriteHitTest(spr, pos);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_StopSprite(FOClient* client, uint32 sprId)
{
    auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    spr->Stop();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetSpriteTime(FOClient* client, uint32 sprId, float32 normalizedTime)
{
    auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    spr->SetTime(normalizedTime);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PlaySprite(FOClient* client, uint32 sprId, hstring animName, bool looped, bool reversed)
{
    auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    spr->Play(animName, looped, reversed);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_GetTextInfo(FOClient* client, string_view text, isize32 size, int32 font, uint32 flags, isize32& resultSize, int32& resultLines)
{
    if (!client->SprMngr.GetTextInfo(size, text, font, flags, resultSize, resultLines)) {
        throw ScriptException("Can't evaluate text information", font);
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Client_Game_GetTextLines(FOClient* client, isize32 size, int32 font)
{
    return client->SprMngr.GetLinesCount(size, "", font);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint32 sprId, ipos32 pos)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSprite(spr, pos, COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint32 sprId, ipos32 pos, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSprite(spr, pos, color != ucolor::clear ? color : COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint32 sprId, ipos32 pos, ucolor color, bool offs)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    auto x = pos.x;
    auto y = pos.y;

    if (offs) {
        x += -spr->GetSize().width / 2 + spr->GetOffset().x;
        y += -spr->GetSize().height + spr->GetOffset().y;
    }

    client->SprMngr.DrawSprite(spr, {x, y}, color != ucolor::clear ? color : COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint32 sprId, ipos32 pos, isize32 size)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSpriteSizeExt(spr, fpos32(pos), fsize32(size), true, true, true, COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint32 sprId, ipos32 pos, isize32 size, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSpriteSizeExt(spr, fpos32(pos), fsize32(size), true, true, true, color != ucolor::clear ? color : COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint32 sprId, fpos32 pos, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSpriteSizeExt(spr, pos, fsize32(spr->GetSize()), false, false, true, color != ucolor::clear ? color : COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint32 sprId, fpos32 pos, fsize32 size, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSpriteSizeExt(spr, pos, size, false, false, true, color != ucolor::clear ? color : COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(FOClient* client, uint32 sprId, ipos32 pos, isize32 size, ucolor color, bool fit, bool offs)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    const fpos32 draw_pos = fpos32(pos + (offs ? spr->GetOffset() : ipos32()));
    client->SprMngr.DrawSpriteSizeExt(spr, draw_pos, fsize32(size), fit, true, true, color != ucolor::clear ? color : COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSpritePattern(FOClient* client, uint32 sprId, ipos32 pos, isize32 size, isize32 sprSize, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSpritePattern(spr, pos, size, sprSize, color != ucolor::clear ? color : COLOR_SPRITE);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawText(FOClient* client, string_view text, ipos32 pos, isize32 size, ucolor color, int32 font, uint32 flags)
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
FO_SCRIPT_API void Client_Game_DrawPrimitive(FOClient* client, RenderPrimitiveType primitiveType, const vector<int32>& data)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (data.empty()) {
        return;
    }

    vector<PrimitivePoint> points;
    const auto size = data.size() / 3;
    points.reserve(size);

    for (size_t i = 0; i < size; i++) {
        points.emplace_back(ipos32 {data[i * 3], data[i * 3 + 1]}, ucolor {std::bit_cast<uint32>(data[i * 3 + 2])});
    }

    client->SprMngr.DrawPoints(points, primitiveType);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawCritter2d(FOClient* client, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim, uint8 dir, int32 l, int32 t, int32 r, int32 b, bool scratch, bool center, ucolor color)
{
    const auto* frames = client->ResMngr.GetCritterAnimFrames(modelName, stateAnim, actionAnim, dir);

    if (frames != nullptr) {
        client->SprMngr.DrawSpriteSize(frames->GetCurSpr(), {l, t}, {r - l, b - t}, scratch, center, color != ucolor::clear ? color : COLOR_SPRITE);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawCritter3d(FOClient* client, uint32 instance, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim, const vector<int32>& layers, const vector<float32>& position, ucolor color)
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
        model_spr = dynamic_ptr_cast<ModelSprite>(client->SprMngr.LoadSprite(modelName, AtlasType::IfaceSprites));

        client->DrawCritterModelCrType[instance] = modelName;
        client->DrawCritterModelFailedToLoad[instance] = false;

        if (!model_spr) {
            client->DrawCritterModelFailedToLoad[instance] = true;
            return;
        }

        auto* model = model_spr->GetModel();

        model->EnableShadow(false);
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
    const auto ntime = count > 9 ? position[9] : 0.0f;
    const auto stl = count > 10 ? position[10] : 0.0f;
    const auto stt = count > 11 ? position[11] : 0.0f;
    const auto str = count > 12 ? position[12] : 0.0f;
    const auto stb = count > 13 ? position[13] : 0.0f;

    if (count > 13) {
        client->SprMngr.PushScissor({iround<int32>(stl), iround<int32>(stt), iround<int32>(str) - iround<int32>(stl), iround<int32>(stb) - iround<int32>(stt)});
    }

    MemFill(client->DrawCritterModelLayers, 0, sizeof(client->DrawCritterModelLayers));

    for (size_t i = 0, j = layers.size(); i < j && i < MODEL_LAYERS_COUNT; i++) {
        client->DrawCritterModelLayers[i] = layers[i];
    }

    auto* model = model_spr->GetModel();

    model->SetLookDirAngle(0);
    model->SetMoveDirAngle(0, false);
    model->SetRotation(rx * DEG_TO_RAD_FLOAT, ry * DEG_TO_RAD_FLOAT, rz * DEG_TO_RAD_FLOAT);
    model->SetScale(sx, sy, sz);
    model->SetSpeed(speed);
    model->PlayAnim(stateAnim, actionAnim, client->DrawCritterModelLayers, ntime, ModelAnimFlags::NoSmooth);

    if (count > 13) {
        const auto max_height = iround<int32>(stb - stt) * 4 / 3;
        model_spr->SetSize({iround<int32>(str - stl), max_height});
    }

    model_spr->DrawToAtlas();

    const auto result_x = iround<int32>(x) - model_spr->GetSize().width / 2 + model_spr->GetOffset().x;
    const auto result_y = iround<int32>(y) - model_spr->GetSize().height + model_spr->GetOffset().y;

    client->SprMngr.DrawSprite(model_spr.get(), {result_x, result_y}, color != ucolor::clear ? color : COLOR_SPRITE);

    if (count > 13) {
        client->SprMngr.PopScissor();
    }

#else
    ignore_unused(client);
    ignore_unused(instance);
    ignore_unused(modelName);
    ignore_unused(stateAnim);
    ignore_unused(actionAnim);
    ignore_unused(layers);
    ignore_unused(position);
    ignore_unused(color);

    throw NotEnabled3DException("3D submodule not enabled");
#endif
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PushDrawScissor(FOClient* client, ipos32 pos, isize32 size)
{
    client->SprMngr.PushScissor(irect32 {pos, size});
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

        client->OffscreenSurfaces.emplace_back(rt);
    }

    auto& rt = client->OffscreenSurfaces.back();
    client->OffscreenSurfaces.pop_back();
    client->ActiveOffscreenSurfaces.emplace_back(rt);

    client->SprMngr.GetRtMngr().PushRenderTarget(rt.get());

    const auto it = std::ranges::find(client->DirtyOffscreenSurfaces, rt);

    if (it != client->DirtyOffscreenSurfaces.end() || forceClear) {
        if (it != client->DirtyOffscreenSurfaces.end()) {
            client->DirtyOffscreenSurfaces.erase(it);
        }

        client->SprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear);
    }

    if (std::ranges::find(client->PreDirtyOffscreenSurfaces, rt) == client->PreDirtyOffscreenSurfaces.end()) {
        client->PreDirtyOffscreenSurfaces.emplace_back(rt);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(FOClient* client, int32 effectSubtype)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (client->ActiveOffscreenSurfaces.empty()) {
        throw ScriptException("No active offscreen surfaces");
    }

    auto& rt = client->ActiveOffscreenSurfaces.back();
    client->ActiveOffscreenSurfaces.pop_back();
    client->OffscreenSurfaces.emplace_back(rt);

    client->SprMngr.GetRtMngr().PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= numeric_cast<int32>(client->OffscreenEffects.size()) || client->OffscreenEffects[effectSubtype] == nullptr) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->SetCustomDrawEffect(client->OffscreenEffects[effectSubtype].get());

    client->SprMngr.DrawRenderTarget(rt.get(), true);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(FOClient* client, int32 effectSubtype, ipos32 pos, isize32 size)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (client->ActiveOffscreenSurfaces.empty()) {
        throw ScriptException("No active offscreen surfaces");
    }

    auto& rt = client->ActiveOffscreenSurfaces.back();
    client->ActiveOffscreenSurfaces.pop_back();
    client->OffscreenSurfaces.emplace_back(rt);

    client->SprMngr.GetRtMngr().PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= numeric_cast<int32>(client->OffscreenEffects.size()) || client->OffscreenEffects[effectSubtype] == nullptr) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->SetCustomDrawEffect(client->OffscreenEffects[effectSubtype].get());

    const auto l = std::clamp(pos.x, 0, client->Settings.ScreenWidth);
    const auto t = std::clamp(pos.y, 0, client->Settings.ScreenHeight);
    const auto r = std::clamp(pos.x + size.width, 0, client->Settings.ScreenWidth);
    const auto b = std::clamp(pos.y + size.height, 0, client->Settings.ScreenHeight);
    const auto from = frect32(l, t, r - l, b - t);
    const auto to = irect32(l, t, r - l, b - t);

    client->SprMngr.DrawRenderTarget(rt.get(), true, &from, &to);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(FOClient* client, int32 effectSubtype, int32 fromX, int32 fromY, int32 fromW, int32 fromH, int32 toX, int32 toY, int32 toW, int32 toH)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (client->ActiveOffscreenSurfaces.empty()) {
        throw ScriptException("No active offscreen surfaces");
    }

    auto& rt = client->ActiveOffscreenSurfaces.back();
    client->ActiveOffscreenSurfaces.pop_back();
    client->OffscreenSurfaces.emplace_back(rt);

    client->SprMngr.GetRtMngr().PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= numeric_cast<int32>(client->OffscreenEffects.size()) || client->OffscreenEffects[effectSubtype] == nullptr) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->SetCustomDrawEffect(client->OffscreenEffects[effectSubtype].get());

    const auto from = frect32(std::clamp(fromX, 0, client->Settings.ScreenWidth), //
        std::clamp(fromY, 0, client->Settings.ScreenHeight), //
        std::clamp(fromW, 0, client->Settings.ScreenWidth - fromX), //
        std::clamp(fromH, 0, client->Settings.ScreenHeight - fromY));
    const auto to = irect32(std::clamp(toX, 0, client->Settings.ScreenWidth), //
        std::clamp(toY, 0, client->Settings.ScreenHeight), //
        std::clamp(toW, 0, client->Settings.ScreenWidth - toX), //
        std::clamp(toH, 0, client->Settings.ScreenHeight - toY));

    client->SprMngr.DrawRenderTarget(rt.get(), true, &from, &to);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SaveScreenshot(FOClient* client, string_view filePath)
{
    ignore_unused(client);
    ignore_unused(filePath);

    throw NotImplementedException(FO_LINE_STR);

    // client->SprMngr.SaveTexture(nullptr, strex(filePath).formatPath(), true);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SaveText(FOClient* client, string_view filePath, string_view text)
{
    ignore_unused(client);

    auto file = DiskFileSystem::OpenFile(strex(filePath).format_path(), true);

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
FO_SCRIPT_API void Client_Game_SetCacheData(FOClient* client, string_view name, const vector<uint8>& data, int32 dataSize)
{
    if (dataSize < 0) {
        throw ScriptException("Negative data size", dataSize);
    }

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
FO_SCRIPT_API void Client_Game_SetMousePos(FOClient* client, ipos32 pos)
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
    string error;

    if (!PackNetCommand(
            command, &client->GetConnection().OutBuf,
            [&error](string_view s) {
                error += s;
                error += "\n";
            },
            client->Hashes)) {
        return "Unknown command";
    }

    return strex(error).trim();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetScreenKeyboard(FOClient* client, bool enabled)
{
    ignore_unused(client, enabled);

    // Todo: improve SetScreenKeyboard
    /*if (SDL_HasScreenKeyboardSupport()) {
        bool cur = (SDL_IsTextInputActive() != SDL_FALSE);
        bool next = strex(args[1]).to_bool();
        if (cur != next) {
            if (next)
                SDL_StartTextInput();
            else
                SDL_StopTextInput();
        }
    }*/
}

FO_END_NAMESPACE();
