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

FO_BEGIN_NAMESPACE

///@ ExportMethod GlobalGetter
FO_SCRIPT_API bool Client_Game_HasChosen(ClientEngine* client)
{
    return client->GetChosen() != nullptr;
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API CritterView* Client_Game_Chosen(ClientEngine* client)
{
    CritterView* chosen = client->GetChosen();

    if (chosen == nullptr) {
        throw ScriptException("No chosen critter (check HasChosen first)");
    }

    return chosen;
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API bool Client_Game_HasCurPlayer(ClientEngine* client)
{
    return client->GetCurPlayer() != nullptr;
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API PlayerView* Client_Game_CurPlayer(ClientEngine* client)
{
    PlayerView* curPlayer = client->GetCurPlayer();

    if (curPlayer == nullptr) {
        throw ScriptException("No current player (check HasCurPlayer first)");
    }

    return curPlayer;
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API bool Client_Game_HasCurLocation(ClientEngine* client)
{
    return client->GetCurLocation() != nullptr;
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API LocationView* Client_Game_CurLocation(ClientEngine* client)
{
    LocationView* curLocation = client->GetCurLocation();

    if (curLocation == nullptr) {
        throw ScriptException("No current location (check HasCurLocation first)");
    }

    return curLocation;
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API bool Client_Game_HasCurMap(ClientEngine* client)
{
    return client->GetCurMap() != nullptr;
}

///@ ExportMethod GlobalGetter
FO_SCRIPT_API MapView* Client_Game_CurMap(ClientEngine* client)
{
    MapView* curMap = client->GetCurMap();

    if (curMap == nullptr) {
        throw ScriptException("No current map (check HasCurMap first)");
    }

    return curMap;
}

///@ ExportMethod Getter
FO_SCRIPT_API ipos32 Client_Game_MousePos(ClientEngine* client)
{
    return client->MousePos;
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsMouseAvailable(ClientEngine* client)
{
    return client->SprMngr.GetInput().IsMouseAvailable();
}

///@ ExportMethod
FO_SCRIPT_API GamepadState Client_Game_GetGamepadState(ClientEngine* client)
{
    return client->SprMngr.GetInput().GetGamepadState();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsFullscreen(ClientEngine* client)
{
    return client->SprMngr.IsFullscreen();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ToggleFullscreen(ClientEngine* client)
{
    client->SprMngr.ToggleFullscreen();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_MinimizeWindow(ClientEngine* client)
{
    client->SprMngr.MinimizeWindow();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsConnecting(ClientEngine* client)
{
    return client->IsConnecting();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsConnected(ClientEngine* client)
{
    return client->IsConnected();
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetDistance(ClientEngine* client, CritterView* cr1, CritterView* cr2)
{
    ignore_unused(client);

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
FO_SCRIPT_API int32_t Client_Game_GetDistance(ClientEngine* client, ItemView* item1, ItemView* item2)
{
    ignore_unused(client);

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
FO_SCRIPT_API int32_t Client_Game_GetDistance(ClientEngine* client, CritterView* cr, ItemView* item)
{
    ignore_unused(client);

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
FO_SCRIPT_API int32_t Client_Game_GetDistance(ClientEngine* client, ItemView* item, CritterView* cr)
{
    ignore_unused(client);

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
FO_SCRIPT_API int32_t Client_Game_GetDistance(ClientEngine* client, CritterView* cr, mpos hex)
{
    ignore_unused(client);

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
FO_SCRIPT_API int32_t Client_Game_GetDistance(ClientEngine* client, mpos hex, CritterView* cr)
{
    ignore_unused(client);

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
FO_SCRIPT_API int32_t Client_Game_GetDistance(ClientEngine* client, mpos hex, ItemView* item)
{
    ignore_unused(client);

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
FO_SCRIPT_API int32_t Client_Game_GetDistance(ClientEngine* client, ItemView* item, mpos hex)
{
    ignore_unused(client);

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
FO_SCRIPT_API void Client_Game_DumpAtlases(ClientEngine* client)
{
    client->SprMngr.GetAtlasMngr().DumpAtlases();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetResolution(ClientEngine* client, int32_t width, int32_t height)
{
    client->SprMngr.SetScreenSize({width, height});

    if (!client->SprMngr.GetWindow().IsVirtual()) {
        client->SprMngr.SetWindowSize({width, height});
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawMiniMap(ClientEngine* client, int32_t zoom, int32_t x, int32_t y, int32_t w, int32_t h)
{
    client->DrawMiniMap(zoom, x, y, w, h);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_RefreshAlwaysOnTop(ClientEngine* client)
{
    client->SprMngr.SetAlwaysOnTop(client->Settings.AlwaysOnTop);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_BytesSend(ClientEngine* client)
{
    return numeric_cast<uint32_t>(client->GetConnection().GetBytesSend());
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_BytesReceive(ClientEngine* client)
{
    return numeric_cast<uint32_t>(client->GetConnection().GetBytesReceived());
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE ItemView* Client_Game_GetItem(ClientEngine* client, ident_t itemId)
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
FO_SCRIPT_API FO_NULLABLE CritterView* Client_Game_GetCritter(ClientEngine* client, ident_t crId)
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
FO_SCRIPT_API vector<CritterView*> Client_Game_GetCritters(ClientEngine* client, CritterFindType findType)
{
    vector<CritterView*> critters;

    if (client->GetCurMap() != nullptr) {
        auto map_critters = client->GetCurMap()->GetCritters();
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
FO_SCRIPT_API vector<CritterView*> Client_Game_GetCritters(ClientEngine* client, hstring pid, CritterFindType findType)
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
FO_SCRIPT_API vector<CritterView*> Client_Game_GetCritters(ClientEngine* client, ProtoCritter* proto, CritterFindType findType)
{
    if (proto == nullptr) {
        throw ScriptException("Critter proto arg is null");
    }

    vector<CritterView*> critters;

    if (client->GetCurMap() != nullptr) {
        for (auto& cr : client->GetCurMap()->GetCritters()) {
            if (cr->GetProtoId() == proto->GetProtoId() && cr->CheckFind(findType)) {
                critters.emplace_back(cr.get());
            }
        }
    }
    else {
        for (auto& cr : client->GetGlobalMapCritters()) {
            if (cr->GetProtoId() == proto->GetProtoId() && cr->CheckFind(findType)) {
                critters.emplace_back(cr.get());
            }
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Game_SortCrittersByDeep(ClientEngine* client, readonly_vector<CritterView*> critters)
{
    ignore_unused(client);

    vector<CritterView*> sorted_critters = to_vector(critters);

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
FO_SCRIPT_API bool Client_Game_PlaySound(ClientEngine* client, string_view soundName)
{
    return client->SndMngr.PlaySound(client->ResMngr.GetSoundNames(), soundName);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_PlayMusic(ClientEngine* client, string_view musicName, timespan repeatTime)
{
    if (musicName.empty()) {
        client->SndMngr.StopMusic();
        return true;
    }

    return client->SndMngr.PlayMusic(musicName, repeatTime);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PlayVideo(ClientEngine* client, string_view videoName, bool canInterrupt, bool enqueue)
{
    client->PlayVideo(videoName, canInterrupt, enqueue);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsVideoPlaying(ClientEngine* client)
{
    return client->IsVideoPlaying();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API VideoPlayback* Client_Game_CreateVideoPlayback(ClientEngine* client, string_view videoName, bool looped)
{
    const auto file = client->Resources.ReadFile(videoName);

    if (!file) {
        throw ScriptException("Video file not found", videoName);
    }

    auto clip = SafeAlloc::MakeUnique<VideoClip>(file.GetData());
    auto tex = client->SprMngr.GetRender().CreateTexture(clip->GetSize(), true, false);

    clip->SetLooped(looped);

    auto video = SafeAlloc::MakeRefCounted<VideoPlayback>();

    video->Clip = std::move(clip);
    video->Tex = std::move(tex);

    video->AddRef();
    return video.get();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawVideoPlayback(ClientEngine* client, VideoPlayback* video, ipos32 pos, isize32 size)
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
FO_SCRIPT_API string Client_Game_GetText(ClientEngine* client, string_view langName, TextPackKey textKey)
{
    return client->GetLangPack(langName).GetText(textKey);
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetText(ClientEngine* client, TextPackKey textKey, int32_t skipCount = 0)
{
    if (skipCount < 0) {
        throw ScriptException("Skip count arg must not be negative", skipCount);
    }

    return client->GetCurLang().GetText(textKey, numeric_cast<size_t>(skipCount));
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetTextCount(ClientEngine* client, TextPackKey textKey)
{
    return numeric_cast<int32_t>(client->GetCurLang().GetTextCount(textKey));
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsTextPresent(ClientEngine* client, TextPackKey textKey)
{
    return client->GetCurLang().IsTextPresent(textKey);
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_ReplaceText(ClientEngine* client, string_view text, string_view from, string_view to)
{
    ignore_unused(client);

    return strex(text).replace(from, to);
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_ReplaceText(ClientEngine* client, string_view text, string_view from, int64_t to)
{
    ignore_unused(client);

    return strex(text).replace(from, strex("{}", to));
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Preload3dFiles(ClientEngine* client, readonly_vector<string> fnames)
{
#if FO_ENABLE_3D
    auto* model_spr_factory = dynamic_cast<ModelSpriteFactory*>(client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)));
    FO_VERIFY_AND_THROW(model_spr_factory, "Missing model sprite factory");

    for (const auto& fname : fnames) {
        model_spr_factory->GetModelMngr()->PreloadModel(fname);
    }

#else
    ignore_unused(client, fnames);
    throw NotEnabled3DException("3D submodule not enabled");
#endif
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_BindFont(ClientEngine* client, FontType font, string_view fontFname)
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
FO_SCRIPT_API void Client_Game_SetEffect(ClientEngine* client, EffectType effectType, int64_t effectSubtype, string_view effectPath)
{
    client->SetEffect(effectType, effectSubtype, effectPath);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetEffectScriptValue(ClientEngine* client, EffectType effectType, int64_t effectSubtype, int32_t valueIndex, float32_t value)
{
    client->SetEffectScriptValue(effectType, effectSubtype, valueIndex, value);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetEffectScriptValues(ClientEngine* client, EffectType effectType, int64_t effectSubtype, int32_t valueStartIndex, readonly_vector<float32_t> values, int32_t valuesOffset = 0, int32_t valuesCount = -1)
{
    client->SetEffectScriptValues(effectType, effectSubtype, valueStartIndex, const_span<float32_t> {values.data(), values.size()}, valuesOffset, valuesCount);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ClearEffectScriptValues(ClientEngine* client, EffectType effectType, int64_t effectSubtype)
{
    client->ClearEffectScriptValues(effectType, effectSubtype);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateMouseClick(ClientEngine* client, ipos32 pos, MouseButton button)
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
FO_SCRIPT_API void Client_Game_SimulateKeyPress(ClientEngine* client, KeyCode key, string_view text = "")
{
    if (key == KeyCode::None) {
        return;
    }

    client->ProcessInputEvent(InputEvent {InputEvent::KeyDownEvent {key, string(text)}});
    client->ProcessInputEvent(InputEvent {InputEvent::KeyUpEvent {key}});
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SimulateKeyboardPress(ClientEngine* client, KeyCode key1, KeyCode key2, string_view key1Text, string_view key2Text)
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
FO_SCRIPT_API uint32_t Client_Game_LoadSprite(ClientEngine* client, string_view sprName)
{
    return client->AnimLoad(client->Hashes.ToHashedString(sprName), AtlasType::IfaceSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadSprite(ClientEngine* client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::IfaceSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadMapSprite(ClientEngine* client, string_view sprName)
{
    return client->AnimLoad(client->Hashes.ToHashedString(sprName), AtlasType::MapSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadMapSprite(ClientEngine* client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::MapSprites);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadSeparateSprite(ClientEngine* client, string_view sprName)
{
    return client->AnimLoad(client->Hashes.ToHashedString(sprName), AtlasType::OneImage);
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Client_Game_LoadSeparateSprite(ClientEngine* client, hstring nameHash)
{
    return client->AnimLoad(nameHash, AtlasType::OneImage);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_FreeSprite(ClientEngine* client, uint32_t sprId)
{
    client->AnimFree(sprId);
}

///@ ExportMethod
FO_SCRIPT_API isize32 Client_Game_GetSpriteSize(ClientEngine* client, uint32_t sprId)
{
    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return {};
    }

    return spr->GetSize();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsSpriteHit(ClientEngine* client, uint32_t sprId, ipos32 pos)
{
    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return false;
    }

    return client->SprMngr.SpriteHitTest(spr, pos);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_StopSprite(ClientEngine* client, uint32_t sprId)
{
    auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    spr->Stop();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetSpriteTime(ClientEngine* client, uint32_t sprId, float32_t normalizedTime)
{
    auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    spr->SetTime(normalizedTime);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PlaySprite(ClientEngine* client, uint32_t sprId, hstring animName, bool looped, bool reversed)
{
    auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    spr->Play(animName, looped, reversed);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_GetTextInfo(ClientEngine* client, string_view text, isize32 size, TextFormat format, isize32& resultSize, int32_t& resultLines)
{
    if (!client->FontMngr.GetTextInfo(size, text, format, resultSize, resultLines)) {
        throw ScriptException("Can't evaluate text information", format.Font);
    }
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Game_GetTextLines(ClientEngine* client, isize32 size, FontType font)
{
    return client->FontMngr.GetLinesCount(size, "", font);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(ClientEngine* client, uint32_t sprId, ipos32 pos, ucolor color = ucolor {}, bool offs = false)
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

    client->SprMngr.DrawSprite(spr, {x, y}, color != ucolor::clear ? color : Color::Neutral);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(ClientEngine* client, uint32_t sprId, fpos32 pos, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSpriteSizeExt(spr, pos, fsize32(spr->GetSize()), false, false, true, color != ucolor::clear ? color : Color::Neutral);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(ClientEngine* client, uint32_t sprId, fpos32 pos, fsize32 size, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSpriteSizeExt(spr, pos, size, false, false, true, color != ucolor::clear ? color : Color::Neutral);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSprite(ClientEngine* client, uint32_t sprId, ipos32 pos, isize32 size, ucolor color = ucolor {}, bool fit = true, bool offs = false)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    const fpos32 draw_pos = fpos32(pos + (offs ? spr->GetOffset() : ipos32()));
    client->SprMngr.DrawSpriteSizeExt(spr, draw_pos, fsize32(size), fit, true, true, color != ucolor::clear ? color : Color::Neutral);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawSpritePattern(ClientEngine* client, uint32_t sprId, ipos32 pos, isize32 size, isize32 sprSize, ucolor color)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const auto* spr = client->AnimGetSpr(sprId);

    if (spr == nullptr) {
        return;
    }

    client->SprMngr.DrawSpritePattern(spr, pos, size, sprSize, color != ucolor::clear ? color : Color::Neutral);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawText(ClientEngine* client, string_view text, ipos32 pos, isize32 size, ucolor color, TextFormat format)
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

    client->FontMngr.DrawText(irect32 {x, y, width, height}, text, color != ucolor::clear ? color : Color::TextWhite, format);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawPrimitive(ClientEngine* client, RenderPrimitiveType primitiveType, readonly_vector<int32_t> data)
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
FO_SCRIPT_API void Client_Game_DrawCritter2d(ClientEngine* client, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim, mdir dir, int32_t l, int32_t t, int32_t r, int32_t b, bool scratch, bool center, ucolor color)
{
    const auto* frames = client->ResMngr.GetCritterAnimFrames(modelName, stateAnim, actionAnim, dir);

    if (frames != nullptr) {
        client->SprMngr.DrawSpriteSize(frames->GetCurSpr(), {l, t}, {r - l, b - t}, scratch, center, color != ucolor::clear ? color : Color::Neutral);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_DrawCritter3d(ClientEngine* client, uint32_t instance, hstring modelName, CritterStateAnim stateAnim, CritterActionAnim actionAnim, readonly_vector<int32_t> layers, readonly_vector<float32_t> position, ucolor color)
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
        client->SprMngr.PushScissor({iround<int32_t>(stl), iround<int32_t>(stt), iround<int32_t>(str) - iround<int32_t>(stl), iround<int32_t>(stb) - iround<int32_t>(stt)});
    }

    MemFill(client->DrawCritterModelLayers, 0, sizeof(client->DrawCritterModelLayers));

    for (size_t i = 0, j = layers.size(); i < j && i < MODEL_LAYERS_COUNT; i++) {
        client->DrawCritterModelLayers[i] = layers[i];
    }

    auto* model = model_spr->GetModel();

    model->SetLookDir(mdir());
    model->SetMoveDir(mdir(), false);
    model->SetRotation(rx * DEG_TO_RAD_FLOAT, ry * DEG_TO_RAD_FLOAT, rz * DEG_TO_RAD_FLOAT);
    model->SetScale(sx, sy, sz);
    model->SetSpeed(speed);
    model->PlayAnim(stateAnim, actionAnim, client->DrawCritterModelLayers, ntime, ModelAnimFlags::NoSmooth);

    if (count > 13) {
        const auto max_height = iround<int32_t>(stb - stt) * 4 / 3;
        model_spr->SetSize({iround<int32_t>(str - stl), max_height});
    }

    model_spr->DrawToAtlas();

    const auto result_x = iround<int32_t>(x) - model_spr->GetSize().width / 2 + model_spr->GetOffset().x;
    const auto result_y = iround<int32_t>(y) - model_spr->GetSize().height + model_spr->GetOffset().y;

    client->SprMngr.DrawSprite(model_spr.get(), {result_x, result_y}, color != ucolor::clear ? color : Color::Neutral);

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
FO_SCRIPT_API void Client_Game_PushDrawScissor(ClientEngine* client, ipos32 pos, isize32 size)
{
    client->SprMngr.PushScissor(irect32 {pos, size});
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PopDrawScissor(ClientEngine* client)
{
    client->SprMngr.PopScissor();
}

static auto TakeActiveOffscreenSurface(ClientEngine* client) -> raw_ptr<RenderTarget>
{
    FO_STACK_TRACE_ENTRY();

    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (client->ActiveOffscreenSurfaces.empty()) {
        throw ScriptException("No active offscreen surfaces");
    }

    raw_ptr<RenderTarget> rt = client->ActiveOffscreenSurfaces.back();
    client->ActiveOffscreenSurfaces.pop_back();
    client->OffscreenSurfaces.emplace_back(rt);

    client->SprMngr.GetRtMngr().PopRenderTarget();

    return rt;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ActivateOffscreenSurface(ClientEngine* client, bool forceClear)
{
    if (!client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    const isize32 surface_size = client->SprMngr.GetScreenSize();

    if (client->OffscreenSurfaces.empty()) {
        auto* rt = client->SprMngr.GetRtMngr().CreateRenderTarget(false, surface_size, false);

        if (rt == nullptr) {
            throw ScriptException("Can't create offscreen surface");
        }

        client->OffscreenSurfaces.emplace_back(rt);
    }

    raw_ptr<RenderTarget> rt = client->OffscreenSurfaces.back();
    client->OffscreenSurfaces.pop_back();
    client->ActiveOffscreenSurfaces.emplace_back(rt);

    if (rt->GetSize() != surface_size) {
        client->SprMngr.GetRtMngr().ResizeRenderTarget(rt.get(), surface_size);
        forceClear = true;
    }

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
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(ClientEngine* client, int32_t effectSubtype)
{
    raw_ptr<RenderTarget> rt = TakeActiveOffscreenSurface(client);
    rt->SetCustomDrawEffect(client->GetOffscreenEffect(effectSubtype));

    client->SprMngr.DrawRenderTarget(rt.get(), true);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(ClientEngine* client, int32_t effectSubtype, ipos32 pos, isize32 size)
{
    raw_ptr<RenderTarget> rt = TakeActiveOffscreenSurface(client);
    rt->SetCustomDrawEffect(client->GetOffscreenEffect(effectSubtype));

    const auto l = std::clamp(pos.x, 0, client->Settings.ScreenWidth);
    const auto t = std::clamp(pos.y, 0, client->Settings.ScreenHeight);
    const auto r = std::clamp(pos.x + size.width, 0, client->Settings.ScreenWidth);
    const auto b = std::clamp(pos.y + size.height, 0, client->Settings.ScreenHeight);
    const auto from = frect32(l, t, r - l, b - t);
    const auto to = irect32(l, t, r - l, b - t);

    client->SprMngr.DrawRenderTarget(rt.get(), true, &from, &to);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(ClientEngine* client, int32_t effectSubtype, ipos32 pos, isize32 size, float32_t scriptValue0, float32_t scriptValue1, float32_t scriptValue2, float32_t scriptValue3)
{
    raw_ptr<RenderTarget> rt = TakeActiveOffscreenSurface(client);
    RenderEffect* effect = client->GetOffscreenEffect(effectSubtype);

    if (effect->IsNeedScriptValueBuf()) {
        RenderEffect::ScriptValueBuffer& script_value_buf = client->EffectMngr.GetOrCreateScriptValueBuf(effect);

        script_value_buf.ScriptValue[0] = scriptValue0;
        script_value_buf.ScriptValue[1] = scriptValue1;
        script_value_buf.ScriptValue[2] = scriptValue2;
        script_value_buf.ScriptValue[3] = scriptValue3;
    }

    rt->SetCustomDrawEffect(effect);

    const int32_t l = std::clamp(pos.x, 0, client->Settings.ScreenWidth);
    const int32_t t = std::clamp(pos.y, 0, client->Settings.ScreenHeight);
    const int32_t r = std::clamp(pos.x + size.width, 0, client->Settings.ScreenWidth);
    const int32_t b = std::clamp(pos.y + size.height, 0, client->Settings.ScreenHeight);
    const frect32 from(l, t, r - l, b - t);
    const irect32 to(l, t, r - l, b - t);

    client->SprMngr.DrawRenderTarget(rt.get(), true, &from, &to);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_PresentOffscreenSurface(ClientEngine* client, int32_t effectSubtype, int32_t fromX, int32_t fromY, int32_t fromW, int32_t fromH, int32_t toX, int32_t toY, int32_t toW, int32_t toH)
{
    raw_ptr<RenderTarget> rt = TakeActiveOffscreenSurface(client);
    rt->SetCustomDrawEffect(client->GetOffscreenEffect(effectSubtype));

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
FO_SCRIPT_API void Client_Game_SaveScreenshot(ClientEngine* client, string_view filePath)
{
    if (filePath.empty()) {
        throw ScriptException("Screenshot file path is empty");
    }

    auto* main_rt = client->SprMngr.GetMainRenderTarget();

    if (main_rt == nullptr) {
        throw ScriptException("SpriteManager has no main render target (FO_DIRECT_SPRITES_DRAW build?)");
    }

    auto* texture = main_rt->GetTexture();
    const auto size = texture->Size;
    auto pixels = texture->GetTextureRegion({0, 0}, size);

    if (texture->FlippedHeight) {
        const auto width = numeric_cast<size_t>(size.width);
        vector<ucolor> row_buf(width);

        for (int32_t y = 0; y < size.height / 2; y++) {
            const auto top = numeric_cast<size_t>(y) * width;
            const auto bottom = numeric_cast<size_t>(size.height - 1 - y) * width;
            MemCopy(row_buf.data(), pixels.data() + top, width * sizeof(ucolor));
            MemCopy(pixels.data() + top, pixels.data() + bottom, width * sizeof(ucolor));
            MemCopy(pixels.data() + bottom, row_buf.data(), width * sizeof(ucolor));
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
FO_SCRIPT_API void Client_Game_SaveText(ClientEngine* client, string_view filePath, string_view text)
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
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    if (!file) {
        throw ScriptException("Can't write file", filePath, text.length());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetCacheData(ClientEngine* client, string_view name, readonly_vector<uint8_t> data)
{
    client->Cache.SetData(name, data);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetCacheData(ClientEngine* client, string_view name, readonly_vector<uint8_t> data, int32_t dataSize)
{
    if (dataSize < 0) {
        throw ScriptException("Negative data size", dataSize);
    }

    auto data_copy = to_vector(data);
    data_copy.resize(dataSize);
    client->Cache.SetData(name, data_copy);
}

///@ ExportMethod
FO_SCRIPT_API vector<uint8_t> Client_Game_GetCacheData(ClientEngine* client, string_view name)
{
    return client->Cache.GetData(name);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetCacheText(ClientEngine* client, string_view name, string_view str)
{
    client->Cache.SetString(name, str);
}

///@ ExportMethod
FO_SCRIPT_API string Client_Game_GetCacheText(ClientEngine* client, string_view name)
{
    return client->Cache.GetString(name);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Game_IsCacheEntry(ClientEngine* client, string_view name)
{
    return client->Cache.HasEntry(name);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_RemoveCacheEntry(ClientEngine* client, string_view name)
{
    client->Cache.RemoveEntry(name);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetUserConfig(ClientEngine* client, readonly_map<string, string> keyValues)
{
    string cfg_user;

    for (const auto& [key, value] : keyValues) {
        cfg_user += strex("{} = {}\n", key, value);
    }

    client->Cache.SetString(LOCAL_CONFIG_NAME, cfg_user);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetUserConfig(ClientEngine* client, readonly_vector<string> keyValues)
{
    string cfg_user;

    for (size_t i = 0; i + 1 < keyValues.size(); i += 2) {
        cfg_user += strex("{} = {}\n", keyValues[i], keyValues[i + 1]);
    }

    client->Cache.SetString(LOCAL_CONFIG_NAME, cfg_user);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetMousePos(ClientEngine* client, ipos32 pos)
{
    client->SprMngr.SetMousePosition(pos);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetForcedMousePos(ClientEngine* client, ipos32 pos)
{
    client->ForcedMousePos = pos;
    client->HasForcedMousePos = true;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ClearForcedMousePos(ClientEngine* client)
{
    client->HasForcedMousePos = false;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_ChangeLanguage(ClientEngine* client, string_view langName)
{
    client->ChangeLanguage(langName);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_FlashUnfocusedWindow(ClientEngine* client)
{
    if (!client->SprMngr.IsWindowFocused()) {
        client->SprMngr.BlinkWindow();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Connect(ClientEngine* client)
{
    client->Connect();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_Disconnect(ClientEngine* client)
{
    client->Disconnect();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Game_SetScreenKeyboard(ClientEngine* client, bool enabled)
{
    client->SprMngr.GetInput().SetScreenKeyboardEnabled(enabled);
}

FO_END_NAMESPACE
