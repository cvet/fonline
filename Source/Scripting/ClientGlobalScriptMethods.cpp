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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
#include "ImageWriter.h"
#include "ModelAnimation.h"
#include "ModelInstance.h"
#include "ModelManager.h"
#include "ModelSprites.h"
#include "ParticleSprites.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

// Returns whether the client currently has a chosen critter.
///@ ExportMethod GlobalGetter
FO_SCRIPT_API bool Client_Game_HasChosen(ptr<ClientEngine> client)
{
    return !!client->GetChosen();
}

// Returns the chosen critter, or throws when none is currently assigned; check HasChosen first when absence is expected.
///@ ExportMethod GlobalGetter
FO_SCRIPT_API ptr<CritterView> Client_Game_Chosen(ptr<ClientEngine> client)
{
    auto chosen = client->GetChosen();

    if (!chosen) {
        throw ScriptException("No chosen critter (check HasChosen first)");
    }

    return chosen;
}

// Returns whether the client currently has a replicated Player view.
///@ ExportMethod GlobalGetter
FO_SCRIPT_API bool Client_Game_HasCurPlayer(ptr<ClientEngine> client)
{
    return !!client->GetCurPlayer();
}

// Returns the current Player view, or throws when none is available; check HasCurPlayer first when absence is expected.
///@ ExportMethod GlobalGetter
FO_SCRIPT_API ptr<PlayerView> Client_Game_CurPlayer(ptr<ClientEngine> client)
{
    auto cur_player = client->GetCurPlayer();

    if (!cur_player) {
        throw ScriptException("No current player (check HasCurPlayer first)");
    }

    return cur_player;
}

// Returns whether the client currently has a Location view.
///@ ExportMethod GlobalGetter
FO_SCRIPT_API bool Client_Game_HasCurLocation(ptr<ClientEngine> client)
{
    return !!client->GetCurLocation();
}

// Returns the current Location view, or throws when none is available; check HasCurLocation first when absence is expected.
///@ ExportMethod GlobalGetter
FO_SCRIPT_API ptr<LocationView> Client_Game_CurLocation(ptr<ClientEngine> client)
{
    auto cur_location = client->GetCurLocation();

    if (!cur_location) {
        throw ScriptException("No current location (check HasCurLocation first)");
    }

    return cur_location;
}

// Returns whether the client is currently displaying a local Map view.
///@ ExportMethod GlobalGetter
FO_SCRIPT_API bool Client_Game_HasCurMap(ptr<ClientEngine> client)
{
    return !!client->GetCurMap();
}

// Returns the current local Map view, or throws when none is loaded; check HasCurMap first when absence is expected.
///@ ExportMethod GlobalGetter
FO_SCRIPT_API ptr<MapView> Client_Game_CurMap(ptr<ClientEngine> client)
{
    auto cur_map = client->GetCurMap();

    if (!cur_map) {
        throw ScriptException("No current map (check HasCurMap first)");
    }

    return cur_map;
}

// Returns the latest mouse position processed by the client input pipeline.
///@ ExportMethod Getter
FO_SCRIPT_API ipos32 Client_Game_MousePos(ptr<ClientEngine> client)
{
    return client->MousePos;
}

// Returns whether the active input backend currently exposes mouse input.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsMouseAvailable(ptr<ClientEngine> client)
{
    return client->SprMngr.GetInput()->IsMouseAvailable();
}

// Returns a snapshot of the active input backend's gamepad state.
///@ ExportMethod
FO_SCRIPT_API GamepadState Client_Game_GetGamepadState(ptr<ClientEngine> client)
{
    return client->SprMngr.GetInput()->GetGamepadState();
}

// Returns whether the client window is currently fullscreen.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsFullscreen(ptr<ClientEngine> client)
{
    return client->SprMngr.IsFullscreen();
}

// Switches the client window between fullscreen and windowed modes.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_ToggleFullscreen(ptr<ClientEngine> client)
{
    client->SprMngr.ToggleFullscreen();
}

// Requests minimization of the client window.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_MinimizeWindow(ptr<ClientEngine> client)
{
    client->SprMngr.MinimizeWindow();
}

// Returns whether a connection attempt is currently in progress.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsConnecting(ptr<ClientEngine> client)
{
    return client->IsConnecting();
}

// Returns whether the client connection is currently established.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsConnected(ptr<ClientEngine> client)
{
    return client->IsConnected();
}

// Returns edge-to-edge hex distance between two critters on the same map after subtracting both multihex radii; throws for off-map or different-map inputs.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, ptr<CritterView> cr1, ptr<CritterView> cr2)
{
    ignore_unused(client);

    auto hex_cr1 = cr1.dyn_cast<const CritterHexView>();
    auto hex_cr2 = cr2.dyn_cast<const CritterHexView>();

    if (!hex_cr1 || !hex_cr2) {
        throw ScriptException("Critters not on map");
    }

    if (hex_cr1->GetMapId() != hex_cr2->GetMapId()) {
        throw ScriptException("Critters not on map");
    }

    int32_t dist = GeometryHelper::GetDistance(hex_cr1->GetHex(), hex_cr2->GetHex());
    int32_t multihex = cr1->GetMultihex() + cr2->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

// Returns center-to-center hex distance between two items on the same map; throws for off-map or different-map inputs.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, ptr<ItemView> item1, ptr<ItemView> item2)
{
    ignore_unused(client);

    auto hex_item1 = item1.dyn_cast<const ItemHexView>();
    auto hex_item2 = item2.dyn_cast<const ItemHexView>();

    if (!hex_item1 || !hex_item2) {
        throw ScriptException("Items not on map");
    }

    if (hex_item1->GetMapId() != hex_item2->GetMapId()) {
        throw ScriptException("Items not on map");
    }

    return GeometryHelper::GetDistance(hex_item1->GetHex(), hex_item2->GetHex());
}

// Returns edge-to-center hex distance from a critter to an item on the same map after subtracting the critter multihex radius; throws for off-map or different-map inputs.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, ptr<CritterView> cr, ptr<ItemView> item)
{
    ignore_unused(client);

    auto hex_cr = cr.dyn_cast<const CritterHexView>();
    auto hex_item = item.dyn_cast<const ItemHexView>();

    if (!hex_cr || !hex_item) {
        throw ScriptException("Critter/Item not on map");
    }

    if (hex_cr->GetMapId() != hex_item->GetMapId()) {
        throw ScriptException("Critter/Item not on map");
    }

    int32_t dist = GeometryHelper::GetDistance(hex_cr->GetHex(), hex_item->GetHex());
    int32_t multihex = hex_cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

// Returns center-to-edge hex distance from an item to a critter on the same map after subtracting the critter multihex radius; throws for off-map or different-map inputs.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, ptr<ItemView> item, ptr<CritterView> cr)
{
    ignore_unused(client);

    auto hex_cr = cr.dyn_cast<const CritterHexView>();
    auto hex_item = item.dyn_cast<const ItemHexView>();

    if (!hex_cr || !hex_item) {
        throw ScriptException("Item/Critter not on map");
    }

    if (hex_cr->GetMapId() != hex_item->GetMapId()) {
        throw ScriptException("Item/Critter not on map");
    }

    int32_t dist = GeometryHelper::GetDistance(hex_cr->GetHex(), hex_item->GetHex());
    int32_t multihex = hex_cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

// Returns edge-to-center hex distance from an on-map critter to a hex after subtracting its multihex radius; throws when the critter is off map.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, ptr<CritterView> cr, mpos hex)
{
    ignore_unused(client);

    auto hex_cr = cr.dyn_cast<const CritterHexView>();

    if (!hex_cr) {
        throw ScriptException("Critter not on map");
    }

    int32_t dist = GeometryHelper::GetDistance(hex_cr->GetHex(), hex);
    int32_t multihex = hex_cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

// Returns center-to-edge hex distance from a hex to an on-map critter after subtracting its multihex radius; throws when the critter is off map.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, mpos hex, ptr<CritterView> cr)
{
    ignore_unused(client);

    auto hex_cr = cr.dyn_cast<const CritterHexView>();

    if (!hex_cr) {
        throw ScriptException("Critter not on map");
    }

    int32_t dist = GeometryHelper::GetDistance(hex_cr->GetHex(), hex);
    int32_t multihex = hex_cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

// Returns center-to-center hex distance from a hex to an on-map item; throws when the item is off map.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, mpos hex, ptr<ItemView> item)
{
    ignore_unused(client);

    auto hex_item = item.dyn_cast<const ItemHexView>();

    if (!hex_item) {
        throw ScriptException("Item not on map");
    }

    return GeometryHelper::GetDistance(hex_item->GetHex(), hex);
}

// Returns center-to-center hex distance from an on-map item to a hex; throws when the item is off map.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ptr<ClientEngine> client, ptr<ItemView> item, mpos hex)
{
    ignore_unused(client);

    auto hex_item = item.dyn_cast<const ItemHexView>();

    if (!hex_item) {
        throw ScriptException("Item not on map");
    }

    return GeometryHelper::GetDistance(hex_item->GetHex(), hex);
}

// Writes every current texture atlas to a timestamped diagnostic TGA directory whose name includes total atlas memory use.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_DumpAtlases(ptr<ClientEngine> client)
{
    client->SprMngr.GetAtlasMngr()->DumpAtlases();
}

// Changes the logical screen size and, for a non-virtual window, the native client window size to the supplied dimensions.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetResolution(ptr<ClientEngine> client, int32_t width, int32_t height)
{
    client->SprMngr.SetScreenSize({width, height});

    if (!client->SprMngr.GetWindow()->IsVirtual()) {
        client->SprMngr.SetWindowSize({width, height});
    }
}

// Prepares or refreshes the current local-map minimap for the supplied zoom and rectangle, then draws its line primitives.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawMiniMap(ptr<ClientEngine> client, int32_t zoom, int32_t x, int32_t y, int32_t w, int32_t h)
{
    client->DrawMiniMap(zoom, x, y, w, h);
}

// Reapplies the current AlwaysOnTop setting to the client window.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_RefreshAlwaysOnTop(ptr<ClientEngine> client)
{
    client->SprMngr.SetAlwaysOnTop(client->Settings->AlwaysOnTop);
}

// Returns the connection's cumulative sent-byte counter narrowed to uint32.
///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_BytesSend(ptr<ClientEngine> client)
{
    return numeric_cast<uint32_t>(client->GetConnection()->GetBytesSend());
}

// Returns the connection's cumulative received-byte counter narrowed to uint32.
///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_BytesReceive(ptr<ClientEngine> client)
{
    return numeric_cast<uint32_t>(client->GetConnection()->GetBytesReceived());
}

// Finds a live item by nonzero id, searching the chosen inventory first and then visible map/global-map items and critter inventories; returns null when absent or destroyed.
///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Game_GetItem(ptr<ClientEngine> client, ident_t itemId)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    nptr<ItemView> item;

    // On chosen
    if (auto chosen = client->GetChosen()) {
        item = chosen->GetInvItem(itemId);
    }

    // On map
    auto cur_map = client->GetCurMap();

    if (cur_map) {
        if (!item) {
            if (auto map_item = cur_map->GetItem(itemId)) {
                item = map_item;
            }
        }

        if (!item) {
            span<refcount_ptr<CritterHexView>> map_critters = cur_map->GetCritters();

            for (size_t i = 0; i < map_critters.size(); i++) {
                auto cr = map_critters[i].as_ptr();

                if (!cr->GetIsChosen()) {
                    item = cr->GetInvItem(itemId);

                    if (item) {
                        break;
                    }
                }
            }
        }
    }
    else {
        if (!item) {
            span<refcount_ptr<CritterView>> gmap_critters = client->GetGlobalMapCritters();

            for (size_t i = 0; i < gmap_critters.size(); i++) {
                auto cr = gmap_critters[i].as_ptr();

                if (!cr->GetIsChosen()) {
                    item = cr->GetInvItem(itemId);

                    if (item) {
                        break;
                    }
                }
            }
        }
    }

    if (!item) {
        return nullptr;
    }

    if (item->IsDestroyed()) {
        return nullptr;
    }

    return item;
}

// Finds a critter by id on the current local map, or among global-map critters when no local map is loaded; returns null for zero, absent, or destroying local-map entries.
///@ ExportMethod
FO_SCRIPT_API nptr<CritterView> Client_Game_GetCritter(ptr<ClientEngine> client, ident_t crId)
{
    if (!crId) {
        return nullptr;
    }

    if (auto cur_map = client->GetCurMap()) {
        auto cr = cur_map->GetCritter(crId);

        if (!cr) {
            return nullptr;
        }

        if (cr->IsDestroyed() || cr->IsDestroying()) {
            return nullptr;
        }

        return cr;
    }
    else {
        auto cr = client->GetGlobalMapCritter(crId);
        return cr;
    }
}

// Returns current local-map or global-map critters that satisfy the requested CritterFindType filter.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<CritterView>> Client_Game_GetCritters(ptr<ClientEngine> client, CritterFindType findType)
{
    vector<ptr<CritterView>> critters;

    if (auto cur_map = client->GetCurMap()) {
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

    return critters;
}

// Returns current local-map or global-map critters that match the optional prototype id and requested CritterFindType filter; an empty id matches every prototype.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<CritterView>> Client_Game_GetCritters(ptr<ClientEngine> client, hstring pid, CritterFindType findType)
{
    vector<ptr<CritterView>> critters;

    if (auto cur_map = client->GetCurMap()) {
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

    return critters;
}

// Returns current local-map or global-map critters whose prototype id matches the supplied prototype and that satisfy the requested CritterFindType filter.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<CritterView>> Client_Game_GetCritters(ptr<ClientEngine> client, ptr<ProtoCritter> proto, CritterFindType findType)
{
    vector<ptr<CritterView>> critters;

    if (auto cur_map = client->GetCurMap()) {
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

    return critters;
}

// Drops null handles and stably sorts critters by map render depth: hex Y, hex X, then sprite sort value when available, with handle order as the final tie-breaker.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<CritterView>> Client_Game_SortCrittersByDeep(ptr<ClientEngine> client, readonly_vector<nptr<CritterView>> critters)
{
    ignore_unused(client);

    vector<ptr<CritterView>> sorted_critters;
    sorted_critters.reserve(critters.size());

    for (nptr<CritterView> cr : critters) {
        if (!cr) {
            continue;
        }

        sorted_critters.emplace_back(cr);
    }

    std::ranges::stable_sort(sorted_critters, [](ptr<const CritterView> cr1, ptr<const CritterView> cr2) {
        mpos cr1_pos = cr1->GetHex();
        mpos cr2_pos = cr2->GetHex();

        if (cr1_pos.y == cr2_pos.y) {
            if (cr1_pos.x == cr2_pos.x) {
                auto cr1_hex = cr1.dyn_cast<const CritterHexView>();
                auto cr2_hex = cr2.dyn_cast<const CritterHexView>();

                if (cr1_hex && cr2_hex) {
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

    return sorted_critters;
}

// Starts a named sound through the client sound resource catalog and returns whether playback was accepted.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_PlaySound(ptr<ClientEngine> client, string_view soundName)
{
    return client->SndMngr.PlaySound(client->ResMngr.GetSoundNames(), soundName);
}

// Starts named music with the supplied repeat delay and returns whether playback was accepted; an empty name stops current music and returns true.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_PlayMusic(ptr<ClientEngine> client, string_view musicName, timespan repeatTime)
{
    if (musicName.empty()) {
        client->SndMngr.StopMusic();
        return true;
    }

    return client->SndMngr.PlayMusic(musicName, repeatTime);
}

// Starts fullscreen video playback, optionally queueing behind an active video; a nonqueued request replaces playback and clears the queue, and an empty name stops it.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_PlayVideo(ptr<ClientEngine> client, string_view videoName, bool canInterrupt, bool enqueue)
{
    client->PlayVideo(videoName, canInterrupt, enqueue);
}

// Returns whether a fullscreen video clip is currently active.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsVideoPlaying(ptr<ClientEngine> client)
{
    return client->IsVideoPlaying();
}

// Creates an independently drawable video playback object from a required resource, configures looping, and transfers ownership to the script.
///@ ExportMethod PassOwnership
FO_SCRIPT_API ptr<VideoPlayback> Client_Game_CreateVideoPlayback(ptr<ClientEngine> client, string_view videoName, bool looped)
{
    auto file = client->Resources.ReadFile(videoName);

    if (!file) {
        throw ScriptException("Video file not found", videoName);
    }

    VideoClip clip {file.GetData()};
    auto tex = client->SprMngr.GetRender().CreateTexture(clip.GetSize(), true, false);

    clip.SetLooped(looped);

    auto video = SafeAlloc::MakeRefCounted<VideoPlayback>();

    video->PlaybackResources.emplace(std::move(clip), std::move(tex));

    video->AddRef();
    return video;
}

// During RenderIface, advances and draws a non-null video playback into a positive-size rectangle and marks the object stopped when its clip ends.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawVideoPlayback(ptr<ClientEngine> client, nptr<VideoPlayback> video, ipos32 pos, isize32 size)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (!video) {
        return;
    }

    if (!video->PlaybackResources) {
        return;
    }

    auto resources = make_ptr(&*video->PlaybackResources);

    if (size.width > 0 && size.height > 0) {
        resources->Tex->UpdateTextureRegion({}, resources->Tex->Size, resources->Clip.RenderFrame());

        irect32 r = {pos.x, pos.y, size.width, size.height};
        client->SprMngr.DrawTexture(resources->Tex, false, nullptr, &r);
    }

    if (resources->Clip.IsStopped()) {
        video->PlaybackResources.reset();
        video->Stopped = true;
    }
}

// Returns the text for a key from the requested language pack, using the current pack for an empty or current language name and caching other loaded packs.
///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetText(ptr<ClientEngine> client, string_view langName, TextPackKey textKey)
{
    return string(client->GetLangPack(langName).GetText(textKey));
}

// Returns the first current-language text variant for a key.
///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetText(ptr<ClientEngine> client, TextPackKey textKey)
{
    return string(client->GetCurLang().GetText(textKey));
}

// Returns the zero-based current-language text variant for a key; an out-of-range index yields an empty string and a negative index throws.
///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetText(ptr<ClientEngine> client, TextPackKey textKey, int32_t textIndex)
{
    if (textIndex < 0) {
        throw ScriptException("Text index arg must not be negative", textIndex);
    }

    return string(client->GetCurLang().GetText(textKey, numeric_cast<size_t>(textIndex)));
}

// Returns the number of current-language text variants registered for a key.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetTextCount(ptr<ClientEngine> client, TextPackKey textKey)
{
    return numeric_cast<int32_t>(client->GetCurLang().GetTextCount(textKey));
}

// Returns whether the current language pack contains the supplied text key.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsTextPresent(ptr<ClientEngine> client, TextPackKey textKey)
{
    return client->GetCurLang().IsTextPresent(textKey);
}

// Returns a copy of text with occurrences of the supplied substring replaced by another string.
///@ ExportMethod
FO_SCRIPT_API string Client_Game_ReplaceText(ptr<ClientEngine> client, string_view text, string_view from, string_view to)
{
    ignore_unused(client);

    return strex(text).replace(from, to);
}

// Returns a copy of text with occurrences of the supplied substring replaced by the decimal representation of an integer.
///@ ExportMethod
FO_SCRIPT_API string Client_Game_ReplaceText(ptr<ClientEngine> client, string_view text, string_view from, int64_t to)
{
    ignore_unused(client);

    return strex(text).replace(from, strex("{}", to));
}

// Preloads each named model through the 3D model manager; throws when the 3D submodule or model sprite factory is unavailable.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_Preload3dFiles(ptr<ClientEngine> client, readonly_vector<string> fnames)
{
#if FO_ENABLE_3D
    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    FO_VERIFY_AND_THROW(factory, "Missing model sprite factory");

    for (const auto& fname : fnames) {
        factory->GetModelMngr()->PreloadModel(fname);
    }

#else
    ignore_unused(client, fnames);
    throw NotEnabled3DException("3D submodule not enabled");
#endif
}

// Binds a .fofnt or .fnt resource to a client font slot with the supplied default scale; throws for any other extension.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_BindFont(ptr<ClientEngine> client, FontType font, string_view fontFname, float32_t defaultScale = 1.0f)
{
    if (fontFname.ends_with(".fofnt")) {
        client->FontMngr.BindFoFont(font, fontFname, AtlasType::IfaceSprites, false, false, defaultScale);
    }
    else if (fontFname.ends_with(".fnt")) {
        client->FontMngr.BindBmfFont(font, fontFname, AtlasType::IfaceSprites, defaultScale);
    }
    else {
        throw ScriptException("Unknown font file extension", font, fontFname);
    }
}

// Rebinds effect slots selected by EffectType to an effect resource; subtype selects entity, font, or offscreen instances where that effect class requires one.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetEffect(ptr<ClientEngine> client, EffectType effectType, int64_t effectSubtype, string_view effectPath)
{
    client->SetEffect(effectType, effectSubtype, effectPath);
}

// Writes one script float at a validated index in the selected effect instance's script-value buffer.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetEffectScriptValue(ptr<ClientEngine> client, EffectType effectType, int64_t effectSubtype, int32_t valueIndex, float32_t value)
{
    client->SetEffectScriptValue(effectType, effectSubtype, valueIndex, value);
}

// Copies a validated slice of the supplied values into the selected effect instance's script-value buffer starting at valueStartIndex.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetEffectScriptValues(ptr<ClientEngine> client, EffectType effectType, int64_t effectSubtype, int32_t valueStartIndex, readonly_vector<float32_t> values, int32_t valuesOffset = 0, int32_t valuesCount = -1)
{
    const_span<float32_t> values_span {};

    if (!values.empty()) {
        values_span = {values.data(), values.size()};
    }

    client->SetEffectScriptValues(effectType, effectSubtype, valueStartIndex, values_span, valuesOffset, valuesCount);
}

// Clears all script values associated with the selected effect instance.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_ClearEffectScriptValues(ptr<ClientEngine> client, EffectType effectType, int64_t effectSubtype)
{
    client->ClearEffectScriptValues(effectType, effectSubtype);
}

// Injects a mouse-move input event when the requested position differs from the current client mouse position, including the computed delta.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateMouseMove(ptr<ClientEngine> client, ipos32 pos)
{
    ipos32 prev_pos = client->MousePos;

    if (prev_pos.x != pos.x || prev_pos.y != pos.y) {
        client->ProcessInputEvent(InputEvent {InputEvent::MouseMoveEvent {pos.x, pos.y, pos.x - prev_pos.x, pos.y - prev_pos.y}});
    }
}

// Moves the simulated mouse to the requested position when needed, then injects a button-down event.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateMouseDown(ptr<ClientEngine> client, ipos32 pos, MouseButton button)
{
    ipos32 prev_pos = client->MousePos;

    if (prev_pos.x != pos.x || prev_pos.y != pos.y) {
        client->ProcessInputEvent(InputEvent {InputEvent::MouseMoveEvent {pos.x, pos.y, pos.x - prev_pos.x, pos.y - prev_pos.y}});
    }

    client->ProcessInputEvent(InputEvent {InputEvent::MouseDownEvent {button}});
}

// Moves the simulated mouse to the requested position when needed, then injects a button-up event.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateMouseUp(ptr<ClientEngine> client, ipos32 pos, MouseButton button)
{
    ipos32 prev_pos = client->MousePos;

    if (prev_pos.x != pos.x || prev_pos.y != pos.y) {
        client->ProcessInputEvent(InputEvent {InputEvent::MouseMoveEvent {pos.x, pos.y, pos.x - prev_pos.x, pos.y - prev_pos.y}});
    }

    client->ProcessInputEvent(InputEvent {InputEvent::MouseUpEvent {button}});
}

// Moves the simulated mouse when needed and injects either a wheel step or a matching button-down/button-up pair.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateMouseClick(ptr<ClientEngine> client, ipos32 pos, MouseButton button)
{
    ipos32 prev_pos = client->MousePos;

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

// Injects a touch-down event for the supplied finger id and client position.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateTouchDown(ptr<ClientEngine> client, int64_t fingerId, ipos32 pos)
{
    client->ProcessInputEvent(InputEvent {InputEvent::TouchDownEvent {fingerId, pos.x, pos.y}});
}

// Injects a touch-move event with the supplied finger id, position, and movement offset.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateTouchMove(ptr<ClientEngine> client, int64_t fingerId, ipos32 pos, ipos32 offsetPos)
{
    client->ProcessInputEvent(InputEvent {InputEvent::TouchMoveEvent {fingerId, pos.x, pos.y, offsetPos.x, offsetPos.y}});
}

// Injects a touch-up event for the supplied finger id and client position.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateTouchUp(ptr<ClientEngine> client, int64_t fingerId, ipos32 pos)
{
    client->ProcessInputEvent(InputEvent {InputEvent::TouchUpEvent {fingerId, pos.x, pos.y}});
}

// Injects a touch-tap event at the supplied client position.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateTouchTap(ptr<ClientEngine> client, ipos32 pos)
{
    client->ProcessInputEvent(InputEvent {InputEvent::TouchTapEvent {pos.x, pos.y}});
}

// Raises the disconnect notification without touching the connection itself.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateDisconnect(ptr<ClientEngine> client)
{
    // Delivers the notification a real disconnect ends with, leaving the connection itself alone, the way
    // simulated input delivers a key without a keyboard: a test of the reaction must not end its own session
    client->OnDisconnected.Fire();
}

// Raises an engine info message with optional extra text, as the server would deliver it.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateInfoMessage(ptr<ClientEngine> client, EngineInfoMessage infoMessage, string_view extraText = "")
{
    client->OnInfoMessage.Fire(infoMessage, string(extraText));
}

// Injects a key-down event carrying optional text followed by key-up; KeyCode::None is a no-op.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateKeyPress(ptr<ClientEngine> client, KeyCode key, string_view text = "")
{
    if (key == KeyCode::None) {
        return;
    }

    client->ProcessInputEvent(InputEvent {InputEvent::KeyDownEvent {key, string(text)}});
    client->ProcessInputEvent(InputEvent {InputEvent::KeyUpEvent {key}});
}

// Injects key1 down, a complete key2 press, then key1 up, skipping either KeyCode::None value and doing nothing when both are None.
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

// Loads a named sprite into the interface atlas, starts its default animation, and returns a client-local handle, or zero when unresolved.
///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadSprite(ptr<ClientEngine> client, string_view sprName)
{
    return client->AnimLoad(client->Hashes.ToHashedString(sprName), AtlasType::IfaceSprites);
}

// Loads a hashed sprite resource into the interface atlas, starts its default animation, and returns a client-local handle, or zero when unresolved.
///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadSprite(ptr<ClientEngine> client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::IfaceSprites);
}

// Loads a named sprite into the map-sprite atlas, starts its default animation, and returns a client-local handle, or zero when unresolved.
///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadMapSprite(ptr<ClientEngine> client, string_view sprName)
{
    return client->AnimLoad(client->Hashes.ToHashedString(sprName), AtlasType::MapSprites);
}

// Loads a hashed sprite resource into the map-sprite atlas, starts its default animation, and returns a client-local handle, or zero when unresolved.
///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadMapSprite(ptr<ClientEngine> client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::MapSprites);
}

// Loads a named sprite as a separate one-image texture, starts its default animation, and returns a client-local handle, or zero when unresolved.
///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadSeparateSprite(ptr<ClientEngine> client, string_view sprName)
{
    return client->AnimLoad(client->Hashes.ToHashedString(sprName), AtlasType::OneImage);
}

// Loads a hashed sprite resource as a separate one-image texture, starts its default animation, and returns a client-local handle, or zero when unresolved.
///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadSeparateSprite(ptr<ClientEngine> client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::OneImage);
}

// Releases a client-local sprite handle, stopping its animation and retaining the resource in the reuse cache; unknown handles are ignored.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_FreeSprite(ptr<ClientEngine> client, uint32_t sprId)
{
    client->AnimFree(sprId);
}

// Returns the current sprite size for a client-local handle, or a zero size when the handle is unknown.
///@ ExportMethod
FO_SCRIPT_API isize32 Client_Game_GetSpriteSize(ptr<ClientEngine> client, uint32_t sprId)
{
    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return {};
    }

    return sprite->GetSize();
}

// Hit-tests a client position against the current sprite frame and returns false when the handle is unknown.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsSpriteHit(ptr<ClientEngine> client, uint32_t sprId, ipos32 pos)
{
    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return false;
    }

    return client->SprMngr.SpriteHitTest(sprite, pos);
}

// Stops animation for a client-local sprite handle; unknown handles are ignored.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_StopSprite(ptr<ClientEngine> client, uint32_t sprId)
{
    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return;
    }

    sprite->Stop();
}

// Sets the normalized playback time of a client-local sprite; unknown handles are ignored.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetSpriteTime(ptr<ClientEngine> client, uint32_t sprId, float32_t normalizedTime)
{
    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return;
    }

    sprite->SetTime(normalizedTime);
}

// Changes the scale of a loaded particle sprite and returns false for an unknown handle or non-particle sprite.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_SetParticleScale(ptr<ClientEngine> client, uint32_t sprId, float32_t scale)
{
    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return false;
    }

    auto particle_sprite = sprite.dyn_cast<ParticleSprite>();

    if (!particle_sprite) {
        return false;
    }

    particle_sprite->GetParticle()->SetScale(scale);
    return true;
}

// Starts the named animation on a client-local sprite with the requested looping and reverse flags; unknown handles are ignored.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_PlaySprite(ptr<ClientEngine> client, uint32_t sprId, hstring animName, bool looped, bool reversed)
{
    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return;
    }

    sprite->Play(animName, looped, reversed);
}

// Starts a loaded particle sprite with a deterministic seed and returns false for an unknown handle, non-particle sprite, or rejected playback.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_PlayParticleWithSeed(ptr<ClientEngine> client, uint32_t sprId, int32_t seed)
{
    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return false;
    }

    auto particle_sprite = sprite.dyn_cast<ParticleSprite>();

    if (!particle_sprite) {
        return false;
    }

    return particle_sprite->PlayWithSeed(seed);
}

// Advances a loaded particle sprite through its prewarm phase and returns false for an unknown handle or non-particle sprite.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_PrewarmParticle(ptr<ClientEngine> client, uint32_t sprId)
{
    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return false;
    }

    auto particle_sprite = sprite.dyn_cast<ParticleSprite>();

    if (!particle_sprite) {
        return false;
    }

    particle_sprite->Prewarm();
    return true;
}

// Measures formatted text inside the supplied size, writing its resulting extent and line count, and throws when the selected font cannot be evaluated.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_GetTextInfo(ptr<ClientEngine> client, string_view text, isize32 size, TextFormat format, isize32& resultSize, int32_t& resultLines)
{
    if (!client->FontMngr.GetTextInfo(size, text, format, resultSize, resultLines)) {
        throw ScriptException("Can't evaluate text information", format.Font);
    }
}

// Returns how many complete lines of the selected font fit within a positive-size rectangle, or zero for nonpositive dimensions.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetTextLines(ptr<ClientEngine> client, isize32 size, FontType font)
{
    return client->FontMngr.GetLinesCount(size, "", font);
}

// During RenderIface, draws a loaded sprite at an integer position; offs applies bottom-center anchoring plus the sprite offset, and an empty color uses neutral tint.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(ptr<ClientEngine> client, uint32_t sprId, ipos32 pos, ucolor color = ucolor {}, bool offs = false)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return;
    }

    int32_t x = pos.x;
    int32_t y = pos.y;

    if (offs) {
        x += -sprite->GetSize().width / 2 + sprite->GetOffset().x;
        y += -sprite->GetSize().height + sprite->GetOffset().y;
    }

    client->SprMngr.DrawSprite(sprite, {x, y}, color != ucolor::clear ? color : Color::Neutral);
}

// During RenderIface, draws a loaded sprite at a floating-point position using its intrinsic size; an empty color uses neutral tint.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(ptr<ClientEngine> client, uint32_t sprId, fpos32 pos, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return;
    }

    client->SprMngr.DrawSpriteSizeExt(sprite, pos, fsize32(sprite->GetSize()), false, false, true, color != ucolor::clear ? color : Color::Neutral);
}

// During RenderIface, draws a loaded sprite at a floating-point position scaled to the supplied size; an empty color uses neutral tint.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(ptr<ClientEngine> client, uint32_t sprId, fpos32 pos, fsize32 size, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return;
    }

    client->SprMngr.DrawSpriteSizeExt(sprite, pos, size, false, false, true, color != ucolor::clear ? color : Color::Neutral);
}

// During RenderIface, draws a loaded sprite into an integer rectangle with optional fitting and sprite-offset application; an empty color uses neutral tint.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(ptr<ClientEngine> client, uint32_t sprId, ipos32 pos, isize32 size, ucolor color = ucolor {}, bool fit = true, bool offs = false)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return;
    }

    fpos32 draw_pos = fpos32(pos + (offs ? sprite->GetOffset() : ipos32()));
    client->SprMngr.DrawSpriteSizeExt(sprite, draw_pos, fsize32(size), fit, true, true, color != ucolor::clear ? color : Color::Neutral);
}

// During RenderIface, tiles a loaded sprite over the supplied rectangle using sprSize as the tile dimensions; an empty color uses neutral tint.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSpritePattern(ptr<ClientEngine> client, uint32_t sprId, ipos32 pos, isize32 size, isize32 sprSize, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return;
    }

    client->SprMngr.DrawSpritePattern(sprite, pos, size, sprSize, color != ucolor::clear ? color : Color::Neutral);
}

// During RenderIface, draws a UV region of a loaded sprite into the supplied rectangle and returns the renderer result, or false for an unknown handle.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_DrawSpriteRegion(ptr<ClientEngine> client, uint32_t sprId, fpos32 uv0, fpos32 uv1, ipos32 pos, isize32 size, ucolor color = ucolor {})
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    auto sprite = client->AnimGetSpr(sprId);

    if (!sprite) {
        return false;
    }

    return client->SprMngr.DrawSpriteRegion(sprite, uv0, uv1, fpos32(pos), fsize32(size), color != ucolor::clear ? color : Color::Neutral);
}

// During RenderIface, draws nonempty formatted text in the supplied rectangle; negative dimensions extend left or up, and an empty color uses text white.
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

// During RenderIface, draws primitive points decoded from complete x, y, packed-color triplets; empty data is ignored and trailing incomplete values are dropped.
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
    auto size = data.size() / 3;
    points.reserve(size);

    for (size_t i = 0; i < size; i++) {
        points.emplace_back(ipos32 {data[i * 3], data[i * 3 + 1]}, ucolor {std::bit_cast<uint32_t>(data[i * 3 + 2])});
    }

    client->SprMngr.DrawPoints(points, primitiveType);
}

// Draws the resolved current 2D critter frame into the supplied bounds, forwarding scratch and center scaling modes; unresolved animation tuples are ignored.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawCritter2d(ptr<ClientEngine> client, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim, mdir dir, int32_t l, int32_t t, int32_t r, int32_t b, bool scratch, bool center, ucolor color)
{
    auto frames = client->ResMngr.GetCritterAnimFrames(modelName, stateAnim, actionAnim, dir);

    if (frames) {
        client->SprMngr.DrawSpriteSize(frames->GetCurSpr(), {l, t}, {r - l, b - t}, scratch, center, color != ucolor::clear ? color : Color::Neutral);
    }
}

// Draws a cached 3D critter instance using layers and optional position values x, y, rotations, scales, speed, normalized time, and scissor bounds; throws without 3D support.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawCritter3d(ptr<ClientEngine> client, uint32_t instance, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim, readonly_vector<int32_t> layers, readonly_vector<float32_t> position, ucolor color)
{
#if FO_ENABLE_3D
    size_t instance_index = numeric_cast<size_t>(instance);

    // Layout: xy, rotation xyz, scale xyz, speed, scissor ltrb
    if (instance_index >= client->DrawCritterModel.size()) {
        client->DrawCritterModel.resize(instance_index + 1);
        client->DrawCritterModelCrType.resize(instance_index + 1);
        client->DrawCritterModelFailedToLoad.resize(instance_index + 1);
    }

    if (client->DrawCritterModelFailedToLoad[instance_index] && client->DrawCritterModelCrType[instance_index] == modelName) {
        return;
    }

    auto& model_spr = client->DrawCritterModel[instance_index];

    if (!model_spr || client->DrawCritterModelCrType[instance_index] != modelName) {
        model_spr = client->SprMngr.LoadSprite(modelName, AtlasType::IfaceSprites).dyn_cast<ModelSprite>();

        client->DrawCritterModelCrType[instance_index] = modelName;
        client->DrawCritterModelFailedToLoad[instance_index] = false;

        if (!model_spr) {
            client->DrawCritterModelFailedToLoad[instance_index] = true;
            return;
        }

        auto model = model_spr->GetModel();

        model->EnableShadow(false);
        model->StartMeshGeneration();
    }

    size_t count = position.size();
    float32_t x = count > 0 ? position[0] : 0.0f;
    float32_t y = count > 1 ? position[1] : 0.0f;
    float32_t rx = count > 2 ? position[2] : 0.0f;
    float32_t ry = count > 3 ? position[3] : 0.0f;
    float32_t rz = count > 4 ? position[4] : 0.0f;
    float32_t sx = count > 5 ? position[5] : 1.0f;
    float32_t sy = count > 6 ? position[6] : 1.0f;
    float32_t sz = count > 7 ? position[7] : 1.0f;
    float32_t speed = count > 8 ? position[8] : 1.0f;
    float32_t ntime = count > 9 ? position[9] : 0.0f;
    float32_t stl = count > 10 ? position[10] : 0.0f;
    float32_t stt = count > 11 ? position[11] : 0.0f;
    float32_t str = count > 12 ? position[12] : 0.0f;
    float32_t stb = count > 13 ? position[13] : 0.0f;

    if (count > 13) {
        client->SprMngr.PushScissor({iround<int32_t>(stl), iround<int32_t>(stt), iround<int32_t>(str) - iround<int32_t>(stl), iround<int32_t>(stb) - iround<int32_t>(stt)});
    }

    auto scissor_guard = scope_fail([&]() noexcept {
        if (count > 13) {
            client->SprMngr.PopScissor();
        }
    });

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
        int32_t max_height = iround<int32_t>(stb - stt) * 4 / 3;
        model_spr->SetSize({iround<int32_t>(str - stl), max_height});
    }

    model_spr->DrawToAtlas();

    int32_t result_x = iround<int32_t>(x) - model_spr->GetSize().width / 2 + model_spr->GetOffset().x;
    int32_t result_y = iround<int32_t>(y) - model_spr->GetSize().height + model_spr->GetOffset().y;

    client->SprMngr.DrawSprite(model_spr, {result_x, result_y}, color != ucolor::clear ? color : Color::Neutral);

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

// Returns positive draw and view bounds for a previously loaded 3D critter instance, false when unavailable, and throws without 3D support.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_GetDrawCritter3dBounds(ptr<ClientEngine> client, uint32_t instance, irect32& drawRect, irect32& viewRect)
{
#if FO_ENABLE_3D
    size_t instance_index = numeric_cast<size_t>(instance);

    if (instance_index >= client->DrawCritterModel.size()) {
        return false;
    }

    shared_ptr<ModelSprite> model_spr = client->DrawCritterModel[instance_index];

    if (!model_spr) {
        return false;
    }

    irect32 draw_rect = model_spr->GetModel()->GetDrawRect();
    irect32 view_rect = model_spr->GetModel()->GetViewRect();

    if (draw_rect.width <= 0 || draw_rect.height <= 0 || view_rect.width <= 0 || view_rect.height <= 0) {
        return false;
    }

    drawRect = draw_rect;
    viewRect = view_rect;
    return true;

#else
    ignore_unused(client);
    ignore_unused(instance);
    ignore_unused(drawRect);
    ignore_unused(viewRect);

    throw NotEnabled3DException("3D submodule not enabled");
#endif
}

// Pushes a drawing scissor rectangle onto the client renderer stack.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_PushDrawScissor(ptr<ClientEngine> client, ipos32 pos, isize32 size)
{
    client->SprMngr.PushScissor(irect32 {pos, size});
}

// Pops the top drawing scissor rectangle from the client renderer stack.
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

    auto rt = client->ActiveOffscreenSurfaces.back();

    client->SprMngr.GetRtMngr().PopRenderTarget();

    client->ActiveOffscreenSurfaces.pop_back();
    client->OffscreenSurfaces.emplace_back(rt);

    return rt;
}

// During RenderIface, pushes a pooled screen-sized offscreen render target, clearing it when forced, dirty, or resized; presentations must consume active surfaces in stack order.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_ActivateOffscreenSurface(ptr<ClientEngine> client, bool forceClear)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    isize32 surface_size = client->SprMngr.GetScreenSize();

    if (client->OffscreenSurfaces.empty()) {
        auto rt = client->SprMngr.GetRtMngr().CreateRenderTarget(false, surface_size, false);

        client->OffscreenSurfaces.emplace_back(rt);
    }

    auto rt = client->OffscreenSurfaces.back();

    if (rt->GetSize() != surface_size) {
        client->SprMngr.GetRtMngr().ResizeRenderTarget(rt, surface_size);
        forceClear = true;
    }

    client->SprMngr.GetRtMngr().PushRenderTarget(rt);

    client->OffscreenSurfaces.pop_back();
    client->ActiveOffscreenSurfaces.emplace_back(rt);

    auto it = std::ranges::find(client->DirtyOffscreenSurfaces, rt);

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

// During RenderIface, consumes the active offscreen surface and draws it over the screen with the selected offscreen effect.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(ptr<ClientEngine> client, int32_t effectSubtype)
{
    auto rt = TakeActiveOffscreenSurface(client);
    rt->SetCustomDrawEffect(client->GetOffscreenEffect(effectSubtype));

    client->SprMngr.DrawRenderTarget(rt, true);
}

// During RenderIface, consumes the active offscreen surface and draws the clamped same-position source and destination rectangle with the selected effect.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(ptr<ClientEngine> client, int32_t effectSubtype, ipos32 pos, isize32 size)
{
    auto rt = TakeActiveOffscreenSurface(client);
    rt->SetCustomDrawEffect(client->GetOffscreenEffect(effectSubtype));

    int32_t l = std::clamp(pos.x, 0, client->Settings->ScreenWidth);
    int32_t t = std::clamp(pos.y, 0, client->Settings->ScreenHeight);
    int32_t r = std::clamp(pos.x + size.width, 0, client->Settings->ScreenWidth);
    int32_t b = std::clamp(pos.y + size.height, 0, client->Settings->ScreenHeight);
    frect32 from(l, t, r - l, b - t);
    irect32 to(l, t, r - l, b - t);

    client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}

// During RenderIface, sets the first four script values when supported, then consumes and draws the active offscreen surface in the clamped rectangle.
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

    int32_t l = std::clamp(pos.x, 0, client->Settings->ScreenWidth);
    int32_t t = std::clamp(pos.y, 0, client->Settings->ScreenHeight);
    int32_t r = std::clamp(pos.x + size.width, 0, client->Settings->ScreenWidth);
    int32_t b = std::clamp(pos.y + size.height, 0, client->Settings->ScreenHeight);
    frect32 from(l, t, r - l, b - t);
    irect32 to(l, t, r - l, b - t);

    client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}

// During RenderIface, consumes the active offscreen surface and draws independently clamped source and destination rectangles with the selected effect.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(ptr<ClientEngine> client, int32_t effectSubtype, int32_t fromX, int32_t fromY, int32_t fromW, int32_t fromH, int32_t toX, int32_t toY, int32_t toW, int32_t toH)
{
    auto rt = TakeActiveOffscreenSurface(client);
    rt->SetCustomDrawEffect(client->GetOffscreenEffect(effectSubtype));

    frect32 from = frect32(std::clamp(fromX, 0, client->Settings->ScreenWidth), //
        std::clamp(fromY, 0, client->Settings->ScreenHeight), //
        std::clamp(fromW, 0, client->Settings->ScreenWidth - fromX), //
        std::clamp(fromH, 0, client->Settings->ScreenHeight - fromY));
    irect32 to = irect32(std::clamp(toX, 0, client->Settings->ScreenWidth), //
        std::clamp(toY, 0, client->Settings->ScreenHeight), //
        std::clamp(toW, 0, client->Settings->ScreenWidth - toX), //
        std::clamp(toH, 0, client->Settings->ScreenHeight - toY));

    client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}

// Reads the main render target, corrects backend vertical flipping, creates parent directories, and writes a TGA screenshot to a required nonempty path.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SaveScreenshot(ptr<ClientEngine> client, string_view filePath)
{
    if (filePath.empty()) {
        throw ScriptException("Screenshot file path is empty");
    }

    auto main_rt = client->SprMngr.GetMainRenderTarget();

    if (!main_rt) {
        throw ScriptException("SpriteManager has no main render target (FO_DIRECT_SPRITES_DRAW build?)");
    }

    auto texture = main_rt->GetTexture();
    isize32 size = texture->Size;
    auto pixels = texture->GetTextureRegion({0, 0}, size);

    if (texture->FlippedHeight) {
        auto width = numeric_cast<size_t>(size.width);

        if (width != 0) {
            vector<ucolor> row_buf(width);
            size_t row_bytes = width * sizeof(ucolor);

            auto row_buf_data = make_nptr(row_buf.data());
            FO_VERIFY_AND_THROW(row_buf_data, "Row buffer data is null");
            auto pixels_data = make_nptr(pixels.data());
            FO_VERIFY_AND_THROW(pixels_data, "Pixel data is null");

            for (int32_t y = 0; y < size.height / 2; y++) {
                auto top = numeric_cast<size_t>(y) * width;
                auto bottom = numeric_cast<size_t>(size.height - 1 - y) * width;
                MemCopy(row_buf_data, pixels_data.get() + top, row_bytes);
                MemCopy(pixels_data.get() + top, pixels_data.get() + bottom, row_bytes);
                MemCopy(pixels_data.get() + bottom, row_buf_data, row_bytes);
            }
        }
    }

    string path = strex(filePath).format_path().str();
    string dir = strex(path).extract_dir().str();

    if (!dir.empty()) {
        if (!fs_create_directories(dir)) {
            throw ScriptException("Can't create directory for screenshot", filePath);
        }
    }

    ImageWriter::WriteSimplePng(path, size, pixels);
}

// Creates parent directories and truncates the target file before writing the supplied text bytes; write and directory failures throw.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SaveText(ptr<ClientEngine> client, string_view filePath, string_view text)
{
    ignore_unused(client);

    string path = strex(filePath).format_path().str();
    string dir = strex(path).extract_dir().str();

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
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    if (!file) {
        throw ScriptException("Can't write file", filePath, text.length());
    }
}

// Stores the complete byte array under a client cache entry name.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetCacheData(ptr<ClientEngine> client, string_view name, readonly_vector<uint8_t> data)
{
    client->Cache.SetData(name, data);
}

// Stores a resized copy of the byte array under a client cache entry, truncating or zero-extending it to a required nonnegative dataSize.
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

// Returns the bytes stored under a client cache entry name, or an empty array when the entry cannot be read.
///@ ExportMethod
FO_SCRIPT_API vector<uint8_t> Client_Game_GetCacheData(ptr<ClientEngine> client, string_view name)
{
    return client->Cache.GetData(name);
}

// Stores a string under a client cache entry name.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetCacheText(ptr<ClientEngine> client, string_view name, string_view str)
{
    client->Cache.SetString(name, str);
}

// Returns the string stored under a client cache entry name, or an empty string when the entry cannot be read.
///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetCacheText(ptr<ClientEngine> client, string_view name)
{
    return client->Cache.GetString(name);
}

// Returns whether the named client cache entry exists.
///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsCacheEntry(ptr<ClientEngine> client, string_view name)
{
    return client->Cache.HasEntry(name);
}

// Removes the named client cache entry when present.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_RemoveCacheEntry(ptr<ClientEngine> client, string_view name)
{
    client->Cache.RemoveEntry(name);
}

// Replaces the cached user configuration with key/value lines serialized as `key = value`.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetUserConfig(ptr<ClientEngine> client, readonly_map<string, string> keyValues)
{
    string cfg_user;

    for (const auto& [key, value] : keyValues) {
        cfg_user += strex("{} = {}\n", key, value);
    }

    client->Cache.SetString(LOCAL_CONFIG_NAME, cfg_user);
}

// Replaces the cached user configuration with consecutive key/value pairs serialized as `key = value`, ignoring an unmatched final element.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetUserConfig(ptr<ClientEngine> client, readonly_vector<string> keyValues)
{
    string cfg_user;

    for (size_t i = 0; i + 1 < keyValues.size(); i += 2) {
        cfg_user += strex("{} = {}\n", keyValues[i], keyValues[i + 1]);
    }

    client->Cache.SetString(LOCAL_CONFIG_NAME, cfg_user);
}

// Moves the native input backend's mouse cursor to the supplied client position.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetMousePos(ptr<ClientEngine> client, ipos32 pos)
{
    client->SprMngr.SetMousePosition(pos);
}

// Forces the client's logical mouse position to the supplied value after each input poll without moving the native cursor.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetForcedMousePos(ptr<ClientEngine> client, ipos32 pos)
{
    client->ForcedMousePos = pos;
    client->HasForcedMousePos = true;
}

// Stops overriding the client's logical mouse position after input polling.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_ClearForcedMousePos(ptr<ClientEngine> client)
{
    client->HasForcedMousePos = false;
}

// Loads the named language pack, makes it current, and updates the client Language setting.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_ChangeLanguage(ptr<ClientEngine> client, string_view langName)
{
    client->ChangeLanguage(langName);
}

// Requests a taskbar or window-manager attention flash only when the client window is unfocused.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_FlashUnfocusedWindow(ptr<ClientEngine> client)
{
    if (!client->SprMngr.IsWindowFocused()) {
        client->SprMngr.BlinkWindow();
    }
}

// Queues a client connection request for processing by the main loop.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_Connect(ptr<ClientEngine> client)
{
    client->Connect();
}

// Cancels any pending connection request and disconnects the active transport.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_Disconnect(ptr<ClientEngine> client)
{
    client->Disconnect();
}

// Enables or disables the active input backend's on-screen keyboard.
///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetScreenKeyboard(ptr<ClientEngine> client, bool enabled)
{
    client->SprMngr.GetInput()->SetScreenKeyboardEnabled(enabled);
}

FO_END_NAMESPACE
