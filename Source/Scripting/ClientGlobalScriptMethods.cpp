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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "ImGuiStuff.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

///@ ExportMethod GlobalGetter
FO_SCRIPT_API bool Client_Game_HasChosen(ptr<ClientEngine> client)
{
    return !!client->GetChosen();
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API ptr<CritterView> Client_Game_Chosen(ptr<ClientEngine> client)
{
    auto chosen = client->GetChosen();

    if (!chosen) {
        throw ScriptException("No chosen critter (check HasChosen first)");
    }

    return chosen.get_no_const();
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API bool Client_Game_HasCurPlayer(ptr<ClientEngine> client)
{
    return !!client->GetCurPlayer();
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API ptr<PlayerView> Client_Game_CurPlayer(ptr<ClientEngine> client)
{
    auto cur_player = client->GetCurPlayer();

    if (!cur_player) {
        throw ScriptException("No current player (check HasCurPlayer first)");
    }

    return cur_player.get_no_const();
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API bool Client_Game_HasCurLocation(ptr<ClientEngine> client)
{
    return !!client->GetCurLocation();
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API ptr<LocationView> Client_Game_CurLocation(ptr<ClientEngine> client)
{
    auto cur_location = client->GetCurLocation();

    if (!cur_location) {
        throw ScriptException("No current location (check HasCurLocation first)");
    }

    return cur_location.get_no_const();
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API bool Client_Game_HasCurMap(ptr<ClientEngine> client)
{
    return !!client->GetCurMap();
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API ptr<MapView> Client_Game_CurMap(ptr<ClientEngine> client)
{
    auto cur_map = client->GetCurMap();

    if (!cur_map) {
        throw ScriptException("No current map (check HasCurMap first)");
    }

    return cur_map.get_no_const();
}

///@ ExportMethod Getter
FO_SCRIPT_API ipos32 Client_Game_MousePos(ptr<ClientEngine> client)
{
    return client->MousePos;
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsMouseAvailable(ptr<ClientEngine> client)
{
    return client->SprMngr.GetInput()->IsMouseAvailable();
}

///@ ExportMethod
FO_SCRIPT_API GamepadState Client_Game_GetGamepadState(ptr<ClientEngine> client)
{
    return client->SprMngr.GetInput()->GetGamepadState();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsFullscreen(ptr<ClientEngine> client)
{
    return client->SprMngr.IsFullscreen();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ToggleFullscreen(ptr<ClientEngine> client)
{
    client->SprMngr.ToggleFullscreen();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_MinimizeWindow(ptr<ClientEngine> client)
{
    client->SprMngr.MinimizeWindow();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsConnecting(ptr<ClientEngine> client)
{
    return client->IsConnecting();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsConnected(ptr<ClientEngine> client)
{
    return client->IsConnected();
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, ptr<CritterView> cr1, ptr<CritterView> cr2)
{
    ignore_unused(client);

    auto nullable_hex_cr1 = cr1.dyn_cast<const CritterHexView>();
    auto nullable_hex_cr2 = cr2.dyn_cast<const CritterHexView>();

    if (!nullable_hex_cr1 || !nullable_hex_cr2) {
        throw ScriptException("Critters not on map");
    }
    auto hex_cr1 = nullable_hex_cr1.as_ptr();
    auto hex_cr2 = nullable_hex_cr2.as_ptr();

    if (hex_cr1->GetMapId() != hex_cr2->GetMapId()) {
        throw ScriptException("Critters not on map");
    }

    const auto dist = GeometryHelper::GetDistance(hex_cr1->GetHex(), hex_cr2->GetHex());
    const auto multihex = cr1->GetMultihex() + cr2->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, ptr<ItemView> item1, ptr<ItemView> item2)
{
    ignore_unused(client);

    auto nullable_hex_item1 = item1.dyn_cast<const ItemHexView>();
    auto nullable_hex_item2 = item2.dyn_cast<const ItemHexView>();

    if (!nullable_hex_item1 || !nullable_hex_item2) {
        throw ScriptException("Items not on map");
    }
    auto hex_item1 = nullable_hex_item1.as_ptr();
    auto hex_item2 = nullable_hex_item2.as_ptr();

    if (hex_item1->GetMapId() != hex_item2->GetMapId()) {
        throw ScriptException("Items not on map");
    }

    return GeometryHelper::GetDistance(hex_item1->GetHex(), hex_item2->GetHex());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, ptr<CritterView> cr, ptr<ItemView> item)
{
    ignore_unused(client);

    auto nullable_hex_cr = cr.dyn_cast<const CritterHexView>();
    auto nullable_hex_item = item.dyn_cast<const ItemHexView>();

    if (!nullable_hex_cr || !nullable_hex_item) {
        throw ScriptException("Critter/Item not on map");
    }
    auto hex_cr = nullable_hex_cr.as_ptr();
    auto hex_item = nullable_hex_item.as_ptr();

    if (hex_cr->GetMapId() != hex_item->GetMapId()) {
        throw ScriptException("Critter/Item not on map");
    }

    const auto dist = GeometryHelper::GetDistance(hex_cr->GetHex(), hex_item->GetHex());
    const auto multihex = hex_cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, ptr<ItemView> item, ptr<CritterView> cr)
{
    ignore_unused(client);

    auto nullable_hex_cr = cr.dyn_cast<const CritterHexView>();
    auto nullable_hex_item = item.dyn_cast<const ItemHexView>();

    if (!nullable_hex_cr || !nullable_hex_item) {
        throw ScriptException("Item/Critter not on map");
    }
    auto hex_cr = nullable_hex_cr.as_ptr();
    auto hex_item = nullable_hex_item.as_ptr();

    if (hex_cr->GetMapId() != hex_item->GetMapId()) {
        throw ScriptException("Item/Critter not on map");
    }

    const auto dist = GeometryHelper::GetDistance(hex_cr->GetHex(), hex_item->GetHex());
    const auto multihex = hex_cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, ptr<CritterView> cr, mpos hex)
{
    ignore_unused(client);

    auto nullable_hex_cr = cr.dyn_cast<const CritterHexView>();

    if (!nullable_hex_cr) {
        throw ScriptException("Critter not on map");
    }
    auto hex_cr = nullable_hex_cr.as_ptr();

    const auto dist = GeometryHelper::GetDistance(hex_cr->GetHex(), hex);
    const auto multihex = hex_cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, mpos hex, ptr<CritterView> cr)
{
    ignore_unused(client);

    auto nullable_hex_cr = cr.dyn_cast<const CritterHexView>();

    if (!nullable_hex_cr) {
        throw ScriptException("Critter not on map");
    }
    auto hex_cr = nullable_hex_cr.as_ptr();

    const auto dist = GeometryHelper::GetDistance(hex_cr->GetHex(), hex);
    const auto multihex = hex_cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, mpos hex, ptr<ItemView> item)
{
    ignore_unused(client);

    auto nullable_hex_item = item.dyn_cast<const ItemHexView>();

    if (!nullable_hex_item) {
        throw ScriptException("Item not on map");
    }
    auto hex_item = nullable_hex_item.as_ptr();

    return GeometryHelper::GetDistance(hex_item->GetHex(), hex);
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, ptr<ItemView> item, mpos hex)
{
    ignore_unused(client);

    auto nullable_hex_item = item.dyn_cast<const ItemHexView>();

    if (!nullable_hex_item) {
        throw ScriptException("Item not on map");
    }
    auto hex_item = nullable_hex_item.as_ptr();

    return GeometryHelper::GetDistance(hex_item->GetHex(), hex);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DumpAtlases(ptr<ClientEngine> client)
{
    client->SprMngr.GetAtlasMngr()->DumpAtlases();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetResolution(ptr<ClientEngine> client, int32_t width, int32_t height)
{
    client->SprMngr.SetScreenSize({width, height});

    if (!client->SprMngr.GetWindow()->IsVirtual()) {
        client->SprMngr.SetWindowSize({width, height});
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawMiniMap(ptr<ClientEngine> client, int32_t zoom, int32_t x, int32_t y, int32_t w, int32_t h)
{
    client->DrawMiniMap(zoom, x, y, w, h);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_RefreshAlwaysOnTop(ptr<ClientEngine> client)
{
    client->SprMngr.SetAlwaysOnTop(client->Settings->AlwaysOnTop);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_BytesSend(ptr<ClientEngine> client)
{
    return numeric_cast<uint32_t>(client->GetConnection()->GetBytesSend());
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_BytesReceive(ptr<ClientEngine> client)
{
    return numeric_cast<uint32_t>(client->GetConnection()->GetBytesReceived());
}

///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Game_GetItem(ptr<ClientEngine> client, ident_t itemId)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    nptr<ItemView> nullable_item;

    // On chosen
    if (nptr<CritterView> nullable_chosen = client->GetChosen()) {
        auto chosen = nullable_chosen.as_ptr();
        nullable_item = chosen->GetInvItem(itemId);
    }

    // On map
    auto nullable_cur_map = client->GetCurMap();

    if (nullable_cur_map) {
        auto cur_map = nullable_cur_map.as_ptr();

        if (!nullable_item) {
            if (nptr<ItemHexView> map_item = cur_map->GetItem(itemId)) {
                nullable_item = map_item;
            }
        }

        if (!nullable_item) {
            span<refcount_ptr<CritterHexView>> map_critters = cur_map->GetCritters();

            for (size_t i = 0; i < map_critters.size(); i++) {
                auto cr = map_critters[i].as_ptr();

                if (!cr->GetIsChosen()) {
                    nullable_item = cr->GetInvItem(itemId);

                    if (nullable_item) {
                        break;
                    }
                }
            }
        }
    }
    else {
        if (!nullable_item) {
            span<refcount_ptr<CritterView>> gmap_critters = client->GetGlobalMapCritters();

            for (size_t i = 0; i < gmap_critters.size(); i++) {
                auto cr = gmap_critters[i].as_ptr();

                if (!cr->GetIsChosen()) {
                    nullable_item = cr->GetInvItem(itemId);

                    if (nullable_item) {
                        break;
                    }
                }
            }
        }
    }

    if (!nullable_item) {
        return nullptr;
    }

    auto item = nullable_item.as_ptr();

    if (item->IsDestroyed()) {
        return nullptr;
    }

    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<CritterView> Client_Game_GetCritter(ptr<ClientEngine> client, ident_t crId)
{
    if (!crId) {
        return nullptr;
    }

    if (nptr<MapView> nullable_cur_map = client->GetCurMap()) {
        auto cur_map = nullable_cur_map.as_ptr();
        auto nullable_cr = cur_map->GetCritter(crId);
        if (!nullable_cr) {
            return nullptr;
        }

        auto cr = nullable_cr.as_ptr();
        if (cr->IsDestroyed() || cr->IsDestroying()) {
            return nullptr;
        }

        return cr.get_no_const();
    }
    else {
        auto cr = client->GetGlobalMapCritter(crId);
        return cr.get_no_const();
    }
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Game_GetCritters(ptr<ClientEngine> client, CritterFindType findType)
{
    vector<ptr<CritterView>> critters;

    if (nptr<MapView> nullable_cur_map = client->GetCurMap()) {
        auto cur_map = nullable_cur_map.as_ptr();
        span<refcount_ptr<CritterHexView>> map_critters = cur_map->GetCritters();
        critters.reserve(map_critters.size());

        for (size_t i = 0; i < map_critters.size(); i++) {
            auto cr = map_critters[i].as_ptr();

            if (cr->CheckFind(findType)) {
                critters.emplace_back(cr);
            }
        }
    }
    else {
        span<refcount_ptr<CritterView>> gmap_critters = client->GetGlobalMapCritters();
        critters.reserve(gmap_critters.size());

        for (size_t i = 0; i < gmap_critters.size(); i++) {
            auto cr = gmap_critters[i].as_ptr();

            if (cr->CheckFind(findType)) {
                critters.emplace_back(cr);
            }
        }
    }

    return MakeScriptHandleVector<CritterView>(critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Game_GetCritters(ptr<ClientEngine> client, hstring pid, CritterFindType findType)
{
    vector<ptr<CritterView>> critters;

    if (nptr<MapView> nullable_cur_map = client->GetCurMap()) {
        auto cur_map = nullable_cur_map.as_ptr();
        span<refcount_ptr<CritterHexView>> map_critters = cur_map->GetCritters();

        if (!pid) {
            for (size_t i = 0; i < map_critters.size(); i++) {
                auto cr = map_critters[i].as_ptr();

                if (cr->CheckFind(findType)) {
                    critters.emplace_back(cr);
                }
            }
        }
        else {
            for (size_t i = 0; i < map_critters.size(); i++) {
                auto cr = map_critters[i].as_ptr();

                if (cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                    critters.emplace_back(cr);
                }
            }
        }
    }
    else {
        span<refcount_ptr<CritterView>> gmap_critters = client->GetGlobalMapCritters();

        if (!pid) {
            for (size_t i = 0; i < gmap_critters.size(); i++) {
                auto cr = gmap_critters[i].as_ptr();

                if (cr->CheckFind(findType)) {
                    critters.emplace_back(cr);
                }
            }
        }
        else {
            for (size_t i = 0; i < gmap_critters.size(); i++) {
                auto cr = gmap_critters[i].as_ptr();

                if (cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                    critters.emplace_back(cr);
                }
            }
        }
    }

    return MakeScriptHandleVector<CritterView>(critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Game_GetCritters(ptr<ClientEngine> client, ptr<ProtoCritter> proto, CritterFindType findType)
{
    vector<ptr<CritterView>> critters;

    if (nptr<MapView> nullable_cur_map = client->GetCurMap()) {
        auto cur_map = nullable_cur_map.as_ptr();
        span<refcount_ptr<CritterHexView>> map_critters = cur_map->GetCritters();

        for (size_t i = 0; i < map_critters.size(); i++) {
            auto cr = map_critters[i].as_ptr();

            if (cr->GetProtoId() == proto->GetProtoId() && cr->CheckFind(findType)) {
                critters.emplace_back(cr);
            }
        }
    }
    else {
        span<refcount_ptr<CritterView>> gmap_critters = client->GetGlobalMapCritters();

        for (size_t i = 0; i < gmap_critters.size(); i++) {
            auto cr = gmap_critters[i].as_ptr();

            if (cr->GetProtoId() == proto->GetProtoId() && cr->CheckFind(findType)) {
                critters.emplace_back(cr);
            }
        }
    }

    return MakeScriptHandleVector<CritterView>(critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Game_SortCrittersByDeep(ptr<ClientEngine> client, readonly_vector<CritterView*> critters)
{
    ignore_unused(client);

    vector<ptr<CritterView>> sorted_critters;
    sorted_critters.reserve(critters.size());

    for (nptr<CritterView> nullable_cr : critters) {
        if (!nullable_cr) {
            continue;
        }

        auto cr = nullable_cr.as_ptr();
        sorted_critters.emplace_back(cr);
    }

    std::ranges::stable_sort(sorted_critters, [](ptr<const CritterView> cr1, ptr<const CritterView> cr2) {
        const mpos cr1_pos = cr1->GetHex();
        const mpos cr2_pos = cr2->GetHex();

        if (cr1_pos.y == cr2_pos.y) {
            if (cr1_pos.x == cr2_pos.x) {
                auto nullable_cr1_hex = cr1.dyn_cast<const CritterHexView>();
                auto nullable_cr2_hex = cr2.dyn_cast<const CritterHexView>();

                if (nullable_cr1_hex && nullable_cr2_hex) {
                    auto cr1_hex = nullable_cr1_hex.as_ptr();
                    auto cr2_hex = nullable_cr2_hex.as_ptr();

                    if (cr1_hex->IsMapSpriteValid() && cr2_hex->IsMapSpriteValid()) {
                        return cr1_hex->GetMapSprite()->GetSortValue() < cr2_hex->GetMapSprite()->GetSortValue();
                    }
                }

                return cr1 < cr2;
            }

            return cr1_pos.x < cr2_pos.x;
        }

        return cr1_pos.y < cr2_pos.y;
    });

    return MakeScriptHandleVector<CritterView>(sorted_critters);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_PlaySound(ptr<ClientEngine> client, string_view soundName)
{
    return client->SndMngr.PlaySound(client->ResMngr.GetSoundNames(), soundName);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_PlayMusic(ptr<ClientEngine> client, string_view musicName, timespan repeatTime)
{
    if (musicName.empty()) {
        client->SndMngr.StopMusic();
        return true;
    }

    return client->SndMngr.PlayMusic(musicName, repeatTime);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PlayVideo(ptr<ClientEngine> client, string_view videoName, bool canInterrupt, bool enqueue)
{
    client->PlayVideo(videoName, canInterrupt, enqueue);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsVideoPlaying(ptr<ClientEngine> client)
{
    return client->IsVideoPlaying();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API ptr<VideoPlayback> Client_Game_CreateVideoPlayback(ptr<ClientEngine> client, string_view videoName, bool looped)
{
    const auto file = client->Resources.ReadFile(videoName);

    if (!file) {
        throw ScriptException("Video file not found", videoName);
    }

    VideoClip clip {file.GetData()};
    auto tex = client->SprMngr.GetRender().CreateTexture(clip.GetSize(), true, false);

    clip.SetLooped(looped);

    refcount_ptr<VideoPlayback> video = SafeAlloc::MakeRefCounted<VideoPlayback>();

    video->PlaybackResources.emplace(std::move(clip), std::move(tex));

    video->AddRef();
    return video.as_ptr().get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawVideoPlayback(ptr<ClientEngine> client, nptr<VideoPlayback> video, ipos32 pos, isize32 size)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (!video) {
        return;
    }
    auto video_ref = video.as_ptr();

    if (!video_ref->PlaybackResources) {
        return;
    }

    ptr<VideoPlaybackResources> resources = &*video_ref->PlaybackResources;

    if (size.width > 0 && size.height > 0) {
        resources->Tex->UpdateTextureRegion({}, resources->Tex->Size, resources->Clip.RenderFrame());

        const irect32 r = {pos.x, pos.y, size.width, size.height};
        auto video_tex = resources->Tex.as_ptr();
        client->SprMngr.DrawTexture(video_tex, false, nullptr, &r);
    }

    if (resources->Clip.IsStopped()) {
        video_ref->PlaybackResources.reset();
        video_ref->Stopped = true;
    }
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetText(ptr<ClientEngine> client, string_view langName, TextPackKey textKey)
{
    return string(client->GetLangPack(langName).GetText(textKey));
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetText(ptr<ClientEngine> client, TextPackKey textKey, int32_t skipCount = 0)
{
    if (skipCount < 0) {
        throw ScriptException("Skip count arg must not be negative", skipCount);
    }

    return string(client->GetCurLang().GetText(textKey, numeric_cast<size_t>(skipCount)));
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetTextCount(ptr<ClientEngine> client, TextPackKey textKey)
{
    return numeric_cast<int32_t>(client->GetCurLang().GetTextCount(textKey));
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsTextPresent(ptr<ClientEngine> client, TextPackKey textKey)
{
    return client->GetCurLang().IsTextPresent(textKey);
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_ReplaceText(ptr<ClientEngine> client, string_view text, string_view from, string_view to)
{
    ignore_unused(client);

    return strex(text).replace(from, to);
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_ReplaceText(ptr<ClientEngine> client, string_view text, string_view from, int64_t to)
{
    ignore_unused(client);

    return strex(text).replace(from, strex("{}", to));
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Preload3dFiles(ptr<ClientEngine> client, readonly_vector<string> fnames)
{
#if FO_ENABLE_3D
    auto nullable_factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    FO_VERIFY_AND_THROW(nullable_factory, "Missing model sprite factory");
    auto factory = nullable_factory.as_ptr();

    for (const auto& fname : fnames) {
        factory->GetModelMngr()->PreloadModel(fname);
    }

#else
    ignore_unused(client, fnames);
    throw NotEnabled3DException("3D submodule not enabled");
#endif
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_BindFont(ptr<ClientEngine> client, FontType font, string_view fontFname)
{
    if (fontFname.ends_with(".fofnt")) {
        client->FontMngr.BindFoFont(font, fontFname, AtlasType::IfaceSprites, false, false);
    }
    else if (fontFname.ends_with(".fnt")) {
        client->FontMngr.BindBmfFont(font, fontFname, AtlasType::IfaceSprites);
    }
    else {
        throw ScriptException("Unknown font file extension", font, fontFname);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetEffect(ptr<ClientEngine> client, EffectType effectType, int64_t effectSubtype, string_view effectPath)
{
    client->SetEffect(effectType, effectSubtype, effectPath);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetEffectScriptValue(ptr<ClientEngine> client, EffectType effectType, int64_t effectSubtype, int32_t valueIndex, float32_t value)
{
    client->SetEffectScriptValue(effectType, effectSubtype, valueIndex, value);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetEffectScriptValues(ptr<ClientEngine> client, EffectType effectType, int64_t effectSubtype, int32_t valueStartIndex, readonly_vector<float32_t> values, int32_t valuesOffset = 0, int32_t valuesCount = -1)
{
    const_span<float32_t> values_span {};

    if (!values.empty()) {
        ptr<const float32_t> values_data = values.data();
        values_span = {values_data.get(), values.size()};
    }

    client->SetEffectScriptValues(effectType, effectSubtype, valueStartIndex, values_span, valuesOffset, valuesCount);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ClearEffectScriptValues(ptr<ClientEngine> client, EffectType effectType, int64_t effectSubtype)
{
    client->ClearEffectScriptValues(effectType, effectSubtype);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateMouseClick(ptr<ClientEngine> client, ipos32 pos, MouseButton button)
{
    const ipos32 prev_pos = client->MousePos;

    if (prev_pos.x != pos.x || prev_pos.y != pos.y) {
        client->ProcessInputEvent(InputEvent {InputEvent::MouseMoveEvent {pos.x, pos.y, pos.x - prev_pos.x, pos.y - prev_pos.y}});
    }

    if (button == MouseButton::WheelUp) {
        client->ProcessInputEvent(InputEvent {InputEvent::MouseWheelEvent {1}});
    }
    else if (button == MouseButton::WheelDown) {
        client->ProcessInputEvent(InputEvent {InputEvent::MouseWheelEvent {-1}});
    }
    else {
        client->ProcessInputEvent(InputEvent {InputEvent::MouseDownEvent {button}});
        client->ProcessInputEvent(InputEvent {InputEvent::MouseUpEvent {button}});
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateTouchDown(ptr<ClientEngine> client, int64_t fingerId, ipos32 pos)
{
    client->ProcessInputEvent(InputEvent {InputEvent::TouchDownEvent {fingerId, pos.x, pos.y}});
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateTouchMove(ptr<ClientEngine> client, int64_t fingerId, ipos32 pos, ipos32 offsetPos)
{
    client->ProcessInputEvent(InputEvent {InputEvent::TouchMoveEvent {fingerId, pos.x, pos.y, offsetPos.x, offsetPos.y}});
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateTouchUp(ptr<ClientEngine> client, int64_t fingerId, ipos32 pos)
{
    client->ProcessInputEvent(InputEvent {InputEvent::TouchUpEvent {fingerId, pos.x, pos.y}});
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateKeyPress(ptr<ClientEngine> client, KeyCode key, string_view text = "")
{
    if (key == KeyCode::None) {
        return;
    }

    client->ProcessInputEvent(InputEvent {InputEvent::KeyDownEvent {key, string(text)}});
    client->ProcessInputEvent(InputEvent {InputEvent::KeyUpEvent {key}});
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateKeyboardPress(ptr<ClientEngine> client, KeyCode key1, KeyCode key2, string_view key1Text, string_view key2Text)
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
FO_SCRIPT_API uint32_t Client_Game_LoadSprite(ptr<ClientEngine> client, string_view sprName)
{
    return client->AnimLoad(client->Hashes.ToHashedString(sprName), AtlasType::IfaceSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadSprite(ptr<ClientEngine> client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::IfaceSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadMapSprite(ptr<ClientEngine> client, string_view sprName)
{
    return client->AnimLoad(client->Hashes.ToHashedString(sprName), AtlasType::MapSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadMapSprite(ptr<ClientEngine> client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::MapSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadSeparateSprite(ptr<ClientEngine> client, string_view sprName)
{
    return client->AnimLoad(client->Hashes.ToHashedString(sprName), AtlasType::OneImage);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadSeparateSprite(ptr<ClientEngine> client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::OneImage);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_FreeSprite(ptr<ClientEngine> client, uint32_t sprId)
{
    client->AnimFree(sprId);
}

///@ ExportMethod
FO_SCRIPT_API isize32 Client_Game_GetSpriteSize(ptr<ClientEngine> client, uint32_t sprId)
{
    auto nullable_sprite = client->AnimGetSpr(sprId);

    if (!nullable_sprite) {
        return {};
    }

    auto sprite = nullable_sprite.as_ptr();
    return sprite->GetSize();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsSpriteHit(ptr<ClientEngine> client, uint32_t sprId, ipos32 pos)
{
    auto nullable_sprite = client->AnimGetSpr(sprId);

    if (!nullable_sprite) {
        return false;
    }
    auto sprite = nullable_sprite.as_ptr();

    return client->SprMngr.SpriteHitTest(sprite, pos);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_StopSprite(ptr<ClientEngine> client, uint32_t sprId)
{
    auto nullable_sprite = client->AnimGetSpr(sprId);

    if (!nullable_sprite) {
        return;
    }

    auto sprite = nullable_sprite.as_ptr();
    sprite->Stop();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetSpriteTime(ptr<ClientEngine> client, uint32_t sprId, float32_t normalizedTime)
{
    auto nullable_sprite = client->AnimGetSpr(sprId);

    if (!nullable_sprite) {
        return;
    }

    auto sprite = nullable_sprite.as_ptr();
    sprite->SetTime(normalizedTime);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PlaySprite(ptr<ClientEngine> client, uint32_t sprId, hstring animName, bool looped, bool reversed)
{
    auto nullable_sprite = client->AnimGetSpr(sprId);

    if (!nullable_sprite) {
        return;
    }

    auto sprite = nullable_sprite.as_ptr();
    sprite->Play(animName, looped, reversed);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_GetTextInfo(ptr<ClientEngine> client, string_view text, isize32 size, TextFormat format, isize32& resultSize, int32_t& resultLines)
{
    if (!client->FontMngr.GetTextInfo(size, text, format, resultSize, resultLines)) {
        throw ScriptException("Can't evaluate text information", format.Font);
    }
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetTextLines(ptr<ClientEngine> client, isize32 size, FontType font)
{
    return client->FontMngr.GetLinesCount(size, "", font);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(ptr<ClientEngine> client, uint32_t sprId, ipos32 pos, ucolor color = ucolor {}, bool offs = false)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    auto nullable_sprite = client->AnimGetSpr(sprId);

    if (!nullable_sprite) {
        return;
    }
    auto sprite = nullable_sprite.as_ptr();

    int32_t x = pos.x;
    int32_t y = pos.y;

    if (offs) {
        x += -sprite->GetSize().width / 2 + sprite->GetOffset().x;
        y += -sprite->GetSize().height + sprite->GetOffset().y;
    }

    client->SprMngr.DrawSprite(sprite, {x, y}, color != ucolor::clear ? color : Color::Neutral);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(ptr<ClientEngine> client, uint32_t sprId, fpos32 pos, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    auto nullable_sprite = client->AnimGetSpr(sprId);

    if (!nullable_sprite) {
        return;
    }
    auto sprite = nullable_sprite.as_ptr();

    client->SprMngr.DrawSpriteSizeExt(sprite, pos, fsize32(sprite->GetSize()), false, false, true, color != ucolor::clear ? color : Color::Neutral);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(ptr<ClientEngine> client, uint32_t sprId, fpos32 pos, fsize32 size, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    auto nullable_sprite = client->AnimGetSpr(sprId);

    if (!nullable_sprite) {
        return;
    }
    auto sprite = nullable_sprite.as_ptr();

    client->SprMngr.DrawSpriteSizeExt(sprite, pos, size, false, false, true, color != ucolor::clear ? color : Color::Neutral);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(ptr<ClientEngine> client, uint32_t sprId, ipos32 pos, isize32 size, ucolor color = ucolor {}, bool fit = true, bool offs = false)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    auto nullable_sprite = client->AnimGetSpr(sprId);

    if (!nullable_sprite) {
        return;
    }
    auto sprite = nullable_sprite.as_ptr();

    const fpos32 draw_pos = fpos32(pos + (offs ? sprite->GetOffset() : ipos32()));
    client->SprMngr.DrawSpriteSizeExt(sprite, draw_pos, fsize32(size), fit, true, true, color != ucolor::clear ? color : Color::Neutral);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSpritePattern(ptr<ClientEngine> client, uint32_t sprId, ipos32 pos, isize32 size, isize32 sprSize, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    auto nullable_sprite = client->AnimGetSpr(sprId);

    if (!nullable_sprite) {
        return;
    }
    auto sprite = nullable_sprite.as_ptr();

    client->SprMngr.DrawSpritePattern(sprite, pos, size, sprSize, color != ucolor::clear ? color : Color::Neutral);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_DrawSpriteRegion(ptr<ClientEngine> client, uint32_t sprId, fpos32 uv0, fpos32 uv1, ipos32 pos, isize32 size, ucolor color = ucolor {})
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    auto nullable_sprite = client->AnimGetSpr(sprId);

    if (!nullable_sprite) {
        return false;
    }
    auto sprite = nullable_sprite.as_ptr();

    return client->SprMngr.DrawSpriteRegion(sprite, uv0, uv1, fpos32(pos), fsize32(size), color != ucolor::clear ? color : Color::Neutral);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawText(ptr<ClientEngine> client, string_view text, ipos32 pos, isize32 size, ucolor color, TextFormat format)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (text.empty()) {
        return;
    }

    int32_t x = pos.x;
    int32_t y = pos.y;
    int32_t width = size.width;
    int32_t height = size.height;

    if (width < 0) {
        width = -width;
        x -= width;
    }
    if (height < 0) {
        height = -height;
        y -= height;
    }

    client->FontMngr.DrawText(irect32 {x, y, width, height}, text, color != ucolor::clear ? color : Color::TextWhite, format);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawPrimitive(ptr<ClientEngine> client, RenderPrimitiveType primitiveType, readonly_vector<int32_t> data)
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
        points.emplace_back(ipos32 {data[i * 3], data[i * 3 + 1]}, ucolor {std::bit_cast<uint32_t>(data[i * 3 + 2])});
    }

    client->SprMngr.DrawPoints(points, primitiveType);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawCritter2d(ptr<ClientEngine> client, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim, mdir dir, int32_t l, int32_t t, int32_t r, int32_t b, bool scratch, bool center, ucolor color)
{
    auto nullable_frames = client->ResMngr.GetCritterAnimFrames(modelName, stateAnim, actionAnim, dir);

    if (nullable_frames) {
        auto frames = nullable_frames.as_ptr();
        client->SprMngr.DrawSpriteSize(frames->GetCurSpr(), {l, t}, {r - l, b - t}, scratch, center, color != ucolor::clear ? color : Color::Neutral);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawCritter3d(ptr<ClientEngine> client, uint32_t instance, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim, readonly_vector<int32_t> layers, readonly_vector<float32_t> position, ucolor color)
{
#if FO_ENABLE_3D
    const size_t instance_index = numeric_cast<size_t>(instance);

    // x y
    // rx ry rz
    // sx sy sz
    // speed
    // scissor l t r b
    if (instance_index >= client->DrawCritterModel.size()) {
        client->DrawCritterModel.resize(instance_index + 1);
        client->DrawCritterModelCrType.resize(instance_index + 1);
        client->DrawCritterModelFailedToLoad.resize(instance_index + 1);
    }

    if (client->DrawCritterModelFailedToLoad[instance_index] && client->DrawCritterModelCrType[instance_index] == modelName) {
        return;
    }

    shared_ptr<ModelSprite>& nullable_model_spr = client->DrawCritterModel[instance_index];

    if (!nullable_model_spr || client->DrawCritterModelCrType[instance_index] != modelName) {
        nullable_model_spr = dynamic_ptr_cast<ModelSprite>(client->SprMngr.LoadSprite(modelName, AtlasType::IfaceSprites));

        client->DrawCritterModelCrType[instance_index] = modelName;
        client->DrawCritterModelFailedToLoad[instance_index] = false;

        if (!nullable_model_spr) {
            client->DrawCritterModelFailedToLoad[instance_index] = true;
            return;
        }

        auto model_spr = nullable_model_spr.as_ptr();
        auto model = model_spr->GetModel();

        model->EnableShadow(false);
        model->StartMeshGeneration();
    }

    auto model_spr = nullable_model_spr.as_ptr();

    const size_t count = position.size();
    const float32_t x = count > 0 ? position[0] : 0.0f;
    const float32_t y = count > 1 ? position[1] : 0.0f;
    const float32_t rx = count > 2 ? position[2] : 0.0f;
    const float32_t ry = count > 3 ? position[3] : 0.0f;
    const float32_t rz = count > 4 ? position[4] : 0.0f;
    const float32_t sx = count > 5 ? position[5] : 1.0f;
    const float32_t sy = count > 6 ? position[6] : 1.0f;
    const float32_t sz = count > 7 ? position[7] : 1.0f;
    const float32_t speed = count > 8 ? position[8] : 1.0f;
    const float32_t ntime = count > 9 ? position[9] : 0.0f;
    const float32_t stl = count > 10 ? position[10] : 0.0f;
    const float32_t stt = count > 11 ? position[11] : 0.0f;
    const float32_t str = count > 12 ? position[12] : 0.0f;
    const float32_t stb = count > 13 ? position[13] : 0.0f;

    if (count > 13) {
        client->SprMngr.PushScissor({iround<int32_t>(stl), iround<int32_t>(stt), iround<int32_t>(str) - iround<int32_t>(stl), iround<int32_t>(stb) - iround<int32_t>(stt)});
    }

    MemFill(client->DrawCritterModelLayers, 0, sizeof(client->DrawCritterModelLayers));

    for (size_t i = 0, j = layers.size(); i < j && i < MODEL_LAYERS_COUNT; i++) {
        client->DrawCritterModelLayers[i] = layers[i];
    }

    auto model = model_spr->GetModel();

    model->SetLookDir(mdir());
    model->SetMoveDir(mdir(), false);
    model->SetRotation(rx * DEG_TO_RAD_FLOAT, ry * DEG_TO_RAD_FLOAT, rz * DEG_TO_RAD_FLOAT);
    model->SetScale(sx, sy, sz);
    model->SetSpeed(speed);
    model->PlayAnim(stateAnim, actionAnim, client->DrawCritterModelLayers, ntime, ModelAnimFlags::NoSmooth);

    if (count > 13) {
        const int32_t max_height = iround<int32_t>(stb - stt) * 4 / 3;
        model_spr->SetSize({iround<int32_t>(str - stl), max_height});
    }

    model_spr->DrawToAtlas();

    const int32_t result_x = iround<int32_t>(x) - model_spr->GetSize().width / 2 + model_spr->GetOffset().x;
    const int32_t result_y = iround<int32_t>(y) - model_spr->GetSize().height + model_spr->GetOffset().y;

    client->SprMngr.DrawSprite(model_spr, {result_x, result_y}, color != ucolor::clear ? color : Color::Neutral);

    if (count > 13) {
        client->SprMngr.PopScissor();
    }

#else
    ptr<ClientEngine> client = client;
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
FO_SCRIPT_API void Client_Game_PushDrawScissor(ptr<ClientEngine> client, ipos32 pos, isize32 size)
{
    client->SprMngr.PushScissor(irect32 {pos, size});
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PopDrawScissor(ptr<ClientEngine> client)
{
    client->SprMngr.PopScissor();
}

static auto TakeActiveOffscreenSurface(ptr<ClientEngine> client) -> ptr<RenderTarget>
{
    FO_STACK_TRACE_ENTRY();

    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (client->ActiveOffscreenSurfaces.empty()) {
        throw ScriptException("No active offscreen surfaces");
    }

    ptr<RenderTarget> rt = client->ActiveOffscreenSurfaces.back();
    client->ActiveOffscreenSurfaces.pop_back();
    client->OffscreenSurfaces.emplace_back(rt);

    client->SprMngr.GetRtMngr().PopRenderTarget();

    return rt;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ActivateOffscreenSurface(ptr<ClientEngine> client, bool forceClear)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const isize32 surface_size = client->SprMngr.GetScreenSize();

    if (client->OffscreenSurfaces.empty()) {
        auto rt = client->SprMngr.GetRtMngr().CreateRenderTarget(false, surface_size, false);

        client->OffscreenSurfaces.emplace_back(rt);
    }

    ptr<RenderTarget> rt = client->OffscreenSurfaces.back();
    client->OffscreenSurfaces.pop_back();
    client->ActiveOffscreenSurfaces.emplace_back(rt);

    if (rt->GetSize() != surface_size) {
        client->SprMngr.GetRtMngr().ResizeRenderTarget(rt, surface_size);
        forceClear = true;
    }

    client->SprMngr.GetRtMngr().PushRenderTarget(rt);

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
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(ptr<ClientEngine> client, int32_t effectSubtype)
{
    auto rt = TakeActiveOffscreenSurface(client);
    rt->SetCustomDrawEffect(client->GetOffscreenEffect(effectSubtype));

    client->SprMngr.DrawRenderTarget(rt, true);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(ptr<ClientEngine> client, int32_t effectSubtype, ipos32 pos, isize32 size)
{
    auto rt = TakeActiveOffscreenSurface(client);
    rt->SetCustomDrawEffect(client->GetOffscreenEffect(effectSubtype));

    const int32_t l = std::clamp(pos.x, 0, client->Settings->ScreenWidth);
    const int32_t t = std::clamp(pos.y, 0, client->Settings->ScreenHeight);
    const int32_t r = std::clamp(pos.x + size.width, 0, client->Settings->ScreenWidth);
    const int32_t b = std::clamp(pos.y + size.height, 0, client->Settings->ScreenHeight);
    const frect32 from(l, t, r - l, b - t);
    const irect32 to(l, t, r - l, b - t);

    client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(ptr<ClientEngine> client, int32_t effectSubtype, ipos32 pos, isize32 size, float32_t scriptValue0, float32_t scriptValue1, float32_t scriptValue2, float32_t scriptValue3)
{
    auto rt = TakeActiveOffscreenSurface(client);
    auto effect = client->GetOffscreenEffect(effectSubtype);

    if (effect->IsNeedScriptValueBuf()) {
        auto script_value_buf = client->EffectMngr.GetOrCreateScriptValueBuf(effect);

        script_value_buf->ScriptValue[0] = scriptValue0;
        script_value_buf->ScriptValue[1] = scriptValue1;
        script_value_buf->ScriptValue[2] = scriptValue2;
        script_value_buf->ScriptValue[3] = scriptValue3;
    }

    rt->SetCustomDrawEffect(effect);

    const int32_t l = std::clamp(pos.x, 0, client->Settings->ScreenWidth);
    const int32_t t = std::clamp(pos.y, 0, client->Settings->ScreenHeight);
    const int32_t r = std::clamp(pos.x + size.width, 0, client->Settings->ScreenWidth);
    const int32_t b = std::clamp(pos.y + size.height, 0, client->Settings->ScreenHeight);
    const frect32 from(l, t, r - l, b - t);
    const irect32 to(l, t, r - l, b - t);

    client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(ptr<ClientEngine> client, int32_t effectSubtype, int32_t fromX, int32_t fromY, int32_t fromW, int32_t fromH, int32_t toX, int32_t toY, int32_t toW, int32_t toH)
{
    auto rt = TakeActiveOffscreenSurface(client);
    rt->SetCustomDrawEffect(client->GetOffscreenEffect(effectSubtype));

    const frect32 from = frect32(std::clamp(fromX, 0, client->Settings->ScreenWidth), //
        std::clamp(fromY, 0, client->Settings->ScreenHeight), //
        std::clamp(fromW, 0, client->Settings->ScreenWidth - fromX), //
        std::clamp(fromH, 0, client->Settings->ScreenHeight - fromY));
    const irect32 to = irect32(std::clamp(toX, 0, client->Settings->ScreenWidth), //
        std::clamp(toY, 0, client->Settings->ScreenHeight), //
        std::clamp(toW, 0, client->Settings->ScreenWidth - toX), //
        std::clamp(toH, 0, client->Settings->ScreenHeight - toY));

    client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SaveScreenshot(ptr<ClientEngine> client, string_view filePath)
{
    if (filePath.empty()) {
        throw ScriptException("Screenshot file path is empty");
    }

    auto nullable_main_rt = client->SprMngr.GetMainRenderTarget();

    if (!nullable_main_rt) {
        throw ScriptException("SpriteManager has no main render target (FO_DIRECT_SPRITES_DRAW build?)");
    }
    auto main_rt = nullable_main_rt.as_ptr();

    auto texture = main_rt->GetTexture();
    const auto size = texture->Size;
    auto pixels = texture->GetTextureRegion({0, 0}, size);

    if (texture->FlippedHeight) {
        const auto width = numeric_cast<size_t>(size.width);

        if (width != 0) {
            vector<ucolor> row_buf(width);
            const size_t row_bytes = width * sizeof(ucolor);

            auto row_buf_data = nptr<ucolor>(row_buf.data()).as_ptr();
            auto pixels_data = nptr<ucolor>(pixels.data()).as_ptr();

            for (int32_t y = 0; y < size.height / 2; y++) {
                const auto top = numeric_cast<size_t>(y) * width;
                const auto bottom = numeric_cast<size_t>(size.height - 1 - y) * width;
                MemCopy(row_buf_data.get(), pixels_data.get() + top, row_bytes);
                MemCopy(pixels_data.get() + top, pixels_data.get() + bottom, row_bytes);
                MemCopy(pixels_data.get() + bottom, row_buf_data.get(), row_bytes);
            }
        }
    }

    const auto path = strex(filePath).format_path().str();
    const auto dir = strex(path).extract_dir().str();

    if (!dir.empty()) {
        if (!fs_create_directories(dir)) {
            throw ScriptException("Can't create directory for screenshot", filePath);
        }
    }

    WriteSimpleTga(path, size, std::move(pixels));
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SaveText(ptr<ClientEngine> client, string_view filePath, string_view text)
{
    ignore_unused(client);

    const auto path = strex(filePath).format_path().str();
    const auto dir = strex(path).extract_dir().str();

    if (!dir.empty()) {
        if (!fs_create_directories(dir)) {
            throw ScriptException("Can't open file for writing", filePath);
        }
    }

    std::ofstream file {std::filesystem::path {fs_make_path(path)}, std::ios::binary | std::ios::trunc};

    if (!file) {
        throw ScriptException("Can't open file for writing", filePath);
    }

    if (!text.empty()) {
        ptr<const char> text_data = text.data();
        file.write(text_data.get(), static_cast<std::streamsize>(text.size()));
    }

    if (!file) {
        throw ScriptException("Can't write file", filePath, text.length());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetCacheData(ptr<ClientEngine> client, string_view name, readonly_vector<uint8_t> data)
{
    client->Cache.SetData(name, data);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetCacheData(ptr<ClientEngine> client, string_view name, readonly_vector<uint8_t> data, int32_t dataSize)
{
    if (dataSize < 0) {
        throw ScriptException("Negative data size", dataSize);
    }

    vector<uint8_t> data_copy = to_vector(data);
    data_copy.resize(dataSize);
    client->Cache.SetData(name, data_copy);
}

///@ ExportMethod
FO_SCRIPT_API vector<uint8_t> Client_Game_GetCacheData(ptr<ClientEngine> client, string_view name)
{
    return client->Cache.GetData(name);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetCacheText(ptr<ClientEngine> client, string_view name, string_view str)
{
    client->Cache.SetString(name, str);
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetCacheText(ptr<ClientEngine> client, string_view name)
{
    return client->Cache.GetString(name);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsCacheEntry(ptr<ClientEngine> client, string_view name)
{
    return client->Cache.HasEntry(name);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_RemoveCacheEntry(ptr<ClientEngine> client, string_view name)
{
    client->Cache.RemoveEntry(name);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetUserConfig(ptr<ClientEngine> client, readonly_map<string, string> keyValues)
{
    string cfg_user;

    for (const auto& [key, value] : keyValues) {
        cfg_user += strex("{} = {}\n", key, value);
    }

    client->Cache.SetString(LOCAL_CONFIG_NAME, cfg_user);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetUserConfig(ptr<ClientEngine> client, readonly_vector<string> keyValues)
{
    string cfg_user;

    for (size_t i = 0; i + 1 < keyValues.size(); i += 2) {
        cfg_user += strex("{} = {}\n", keyValues[i], keyValues[i + 1]);
    }

    client->Cache.SetString(LOCAL_CONFIG_NAME, cfg_user);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetMousePos(ptr<ClientEngine> client, ipos32 pos)
{
    client->SprMngr.SetMousePosition(pos);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetForcedMousePos(ptr<ClientEngine> client, ipos32 pos)
{
    client->ForcedMousePos = pos;
    client->HasForcedMousePos = true;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ClearForcedMousePos(ptr<ClientEngine> client)
{
    client->HasForcedMousePos = false;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ChangeLanguage(ptr<ClientEngine> client, string_view langName)
{
    client->ChangeLanguage(langName);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_FlashUnfocusedWindow(ptr<ClientEngine> client)
{
    if (!client->SprMngr.IsWindowFocused()) {
        client->SprMngr.BlinkWindow();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Connect(ptr<ClientEngine> client)
{
    client->Connect();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Disconnect(ptr<ClientEngine> client)
{
    client->Disconnect();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetScreenKeyboard(ptr<ClientEngine> client, bool enabled)
{
    client->SprMngr.GetInput()->SetScreenKeyboardEnabled(enabled);
}

FO_END_NAMESPACE
