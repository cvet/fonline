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
#include "CritterHexView.h"
#include "CritterView.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# param name ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_SetName(CritterView* self, string_view name)
{
    self->SetName(name);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_IsChosen(CritterView* self)
{
    return self->IsChosen();
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsOwnedByPlayer(CritterView* self)
{
    return self->IsOwnedByPlayer();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_IsNpc(CritterView* self)
{
    return self->IsNpc();
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsOffline(CritterView* self)
{
    return self->IsOwnedByPlayer() && self->IsPlayerOffline();
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsAlive(CritterView* self)
{
    return self->IsAlive();
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsKnockout(CritterView* self)
{
    return self->IsKnockout();
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsDead(CritterView* self)
{
    return self->IsDead();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_IsOnMap(CritterView* self)
{
    return dynamic_cast<CritterHexView*>(self) != nullptr;
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsMoving(CritterView* self)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    return hex_cr->IsMoving();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_IsModel(CritterView* self)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

#if FO_ENABLE_3D
    return hex_cr->IsModel();
#else
    return false;
#endif
}

///# ...
///# param anim1 ...
///# param anim2 ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_IsAnimAvailable(CritterView* self, uint anim1, uint anim2)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    return hex_cr->IsAnimAvailable(anim1, anim2);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_IsAnimPlaying(CritterView* self)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    return hex_cr->IsAnim();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Critter_GetAnim1(CritterView* self)
{
    return self->GetAnim1();
}

///# ...
///# param anim1 ...
///# param anim2 ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_Animate(CritterView* self, uint anim1, uint anim2)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    hex_cr->Animate(anim1, anim2, nullptr);
}

///# ...
///# param anim1 ...
///# param anim2 ...
///# param actionItem ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_Animate(CritterView* self, uint anim1, uint anim2, AbstractItem* actionItem)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    hex_cr->Animate(anim1, anim2, actionItem);
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_StopAnim(CritterView* self)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    hex_cr->ClearAnim();
}

///# ...
///# param protoId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Critter_CountItem(CritterView* self, hstring protoId)
{
    uint result = 0;
    for (const auto* item : self->GetInvItems()) {
        if (!protoId || item->GetProtoId() == protoId) {
            result += item->GetCount();
        }
    }

    return result;
}

///# ...
///# param itemId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Critter_GetItem(CritterView* self, ident_t itemId)
{
    return self->GetInvItem(itemId);
}

///# ...
///# param protoId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Critter_GetItem(CritterView* self, hstring protoId)
{
    const auto* proto_item = self->GetEngine()->ProtoMngr.GetProtoItem(protoId);
    if (proto_item == nullptr) {
        throw ScriptException("Invalid proto id", protoId);
    }

    if (proto_item->GetStackable()) {
        for (auto* item : self->GetInvItems()) {
            if (item->GetProtoId() == protoId) {
                return item;
            }
        }
    }
    else {
        ItemView* another_slot = nullptr;
        for (auto* item : self->GetInvItems()) {
            if (item->GetProtoId() == protoId) {
                if (item->GetCritterSlot() == 0) {
                    return item;
                }
                another_slot = item;
            }
        }
        return another_slot;
    }

    return nullptr;
}

///# ...
///# param component ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Critter_GetItem(CritterView* self, ItemComponent component)
{
    for (auto* item : self->GetInvItems()) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return item;
        }
    }

    return nullptr;
}

///# ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Critter_GetItem(CritterView* self, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<ItemView>(self->GetEngine(), property);

    for (auto* item : self->GetInvItems()) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item;
        }
    }

    return nullptr;
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<ItemView*> Client_Critter_GetItems(CritterView* self)
{
    return self->GetInvItems();
}

///# ...
///# param component ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<ItemView*> Client_Critter_GetItems(CritterView* self, ItemComponent component)
{
    vector<ItemView*> items;
    items.reserve(self->GetInvItems().size());

    for (auto* item : self->GetInvItems()) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<ItemView*> Client_Critter_GetItems(CritterView* self, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<ItemView>(self->GetEngine(), property);

    vector<ItemView*> items;
    items.reserve(self->GetInvItems().size());

    for (auto* item : self->GetInvItems()) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# param visible ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_SetVisibility(CritterView* self, bool visible)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    hex_cr->Visible = visible;
    hex_cr->GetEngine()->CurMap->RefreshMap();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_GetVisibility(CritterView* self)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    return hex_cr->Visible;
}

///# ...
///# param nameVisible ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///# param lines ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_GetNameTextInfo(CritterView* self, bool& nameVisible, int& x, int& y, int& w, int& h, int& lines)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    hex_cr->GetNameTextInfo(nameVisible, x, y, w, h, lines);
}

///# ...
///# param x ...
///# param y ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_GetTextPos(CritterView* self, int& x, int& y)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    hex_cr->GetNameTextPos(x, y);
}

///# ...
///# param particleName ...
///# param boneName ...
///# param moveX ...
///# param moveY ...
///# param moveZ ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_RunParticle(CritterView* self, string_view particleName, hstring boneName, float moveX, float moveY, float moveZ)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

#if FO_ENABLE_3D
    if (hex_cr->IsModel()) {
        hex_cr->GetModel()->RunParticle(particleName, boneName, vec3(moveX, moveY, moveZ));
    }
    else
#endif
    {
        // Todo: improve run particles for 2D animations
        UNUSED_VARIABLE(particleName);
        UNUSED_VARIABLE(boneName);
        UNUSED_VARIABLE(moveX);
        UNUSED_VARIABLE(moveY);
        UNUSED_VARIABLE(moveZ);
    }
}

///# ...
///# param anim1 ...
///# param anim2 ...
///# param normalizedTime ...
///# param animCallback ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_AddAnimCallback(CritterView* self, uint anim1, uint anim2, float normalizedTime, CallbackFunc<CritterView*> animCallback)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

#if FO_ENABLE_3D
    if (hex_cr->IsModel()) {
        hex_cr->GetModel()->AnimationCallbacks.push_back({anim1, anim2, std::clamp(normalizedTime, 0.0f, 1.0f), [self, animCallback] {
                                                              if (!self->IsDestroyed()) {
                                                                  const auto func_name = static_cast<hstring>(animCallback);
                                                                  const auto result = self->GetEngine()->ScriptSys->CallFunc<void, CritterView*>(func_name, self);
                                                                  UNUSED_VARIABLE(result);
                                                              }
                                                          }});
    }
    else
#endif
    {
        // Todo: improve animation callbacks for 2D animations
        UNUSED_VARIABLE(anim1);
        UNUSED_VARIABLE(anim2);
        UNUSED_VARIABLE(normalizedTime);
        UNUSED_VARIABLE(animCallback);
    }
}

///# ...
///# param boneName ...
///# param boneX ...
///# param boneY ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_GetBonePos(CritterView* self, hstring boneName, int& boneX, int& boneY)
{
#if FO_ENABLE_3D
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    if (!hex_cr->IsModel()) {
        return false;
    }

    const auto bone_pos = hex_cr->GetModel()->GetBonePos(boneName);
    if (!bone_pos) {
        return false;
    }

    boneX = std::get<0>(*bone_pos) + hex_cr->ScrX;
    boneY = std::get<1>(*bone_pos) + hex_cr->ScrY;
    return true;

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(boneName);
    UNUSED_VARIABLE(boneX);
    UNUSED_VARIABLE(boneY);

    throw NotEnabled3DException("3D submodule not enabled");
#endif
}

///@ ExportMethod
[[maybe_unused]] void Client_Critter_MoveToHex(CritterView* self, uint16 hx, uint16 hy, int ox, int oy, uint speed)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    self->GetEngine()->CritterMoveTo(hex_cr, tuple {hx, hy, ox, oy}, speed);
}

///@ ExportMethod
[[maybe_unused]] void Client_Critter_MoveToDir(CritterView* self, int dir, uint speed)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    self->GetEngine()->CritterMoveTo(hex_cr, dir, speed);
}

///@ ExportMethod
[[maybe_unused]] void Client_Critter_StopMove(CritterView* self)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    self->GetEngine()->CritterMoveTo(hex_cr, -1, 0);
}

///@ ExportMethod
[[maybe_unused]] void Client_Critter_ChangeDir(CritterView* self, uint8 dir)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    self->GetEngine()->CritterLookTo(hex_cr, dir);
}

///@ ExportMethod
[[maybe_unused]] void Client_Critter_ChangeDirAngle(CritterView* self, int16 dirAngle)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    self->GetEngine()->CritterLookTo(hex_cr, dirAngle);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint8 Client_Critter_GetAlpha(CritterView* self)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    return hex_cr != nullptr ? hex_cr->Alpha : 0xFF;
}
