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
#include "CritterHexView.h"
#include "CritterView.h"

///@ ExportMethod
[[maybe_unused]] void Client_Critter_SetName(CritterView* self, string_view name)
{
    self->SetName(name);
}

///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsOffline(CritterView* self)
{
    return self->GetControlledByPlayer() && self->GetIsPlayerOffline();
}

///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsAlive(CritterView* self)
{
    return self->IsAlive();
}

///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsKnockout(CritterView* self)
{
    return self->IsKnockout();
}

///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsDead(CritterView* self)
{
    return self->IsDead();
}

///@ ExportMethod
[[maybe_unused]] bool Client_Critter_IsOnMap(CritterView* self)
{
    return dynamic_cast<CritterHexView*>(self) != nullptr;
}

///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsMoving(CritterView* self)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    return hex_cr->IsMoving();
}

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

///@ ExportMethod
[[maybe_unused]] bool Client_Critter_IsAnimAvailable(CritterView* self, CritterStateAnim stateAnim, CritterActionAnim actionAnim)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    return hex_cr->IsAnimAvailable(stateAnim, actionAnim);
}

///@ ExportMethod
[[maybe_unused]] bool Client_Critter_IsAnimPlaying(CritterView* self)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    return hex_cr->IsAnim();
}

///@ ExportMethod
[[maybe_unused]] CritterStateAnim Client_Critter_GetStateAnim(CritterView* self)
{
    return self->GetStateAnim();
}

///@ ExportMethod
[[maybe_unused]] void Client_Critter_Animate(CritterView* self, CritterStateAnim stateAnim, CritterActionAnim actionAnim)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    hex_cr->Animate(stateAnim, actionAnim, nullptr);
}

///@ ExportMethod
[[maybe_unused]] void Client_Critter_Animate(CritterView* self, CritterStateAnim stateAnim, CritterActionAnim actionAnim, AbstractItem* contextItem)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    hex_cr->Animate(stateAnim, actionAnim, contextItem);
}

///@ ExportMethod
[[maybe_unused]] void Client_Critter_StopAnim(CritterView* self)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    hex_cr->ClearAnim();
}

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

///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Critter_GetItem(CritterView* self, ident_t itemId)
{
    return self->GetInvItem(itemId);
}

///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Critter_GetItem(CritterView* self, hstring protoId)
{
    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoItem(protoId);

    if (proto->GetStackable()) {
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
                if (item->GetCritterSlot() == CritterItemSlot::Inventory) {
                    return item;
                }

                another_slot = item;
            }
        }

        return another_slot;
    }

    return nullptr;
}

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

///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<ItemView*> Client_Critter_GetItems(CritterView* self)
{
    return self->GetInvItems();
}

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

///@ ExportMethod
[[maybe_unused]] bool Client_Critter_GetTextPos(CritterView* self, int& x, int& y)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    return hex_cr->GetNameTextPos(x, y);
}

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

///@ ExportMethod
[[maybe_unused]] void Client_Critter_AddAnimCallback(CritterView* self, CritterStateAnim stateAnim, CritterActionAnim actionAnim, float normalizedTime, CallbackFunc<CritterView*> animCallback)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

#if FO_ENABLE_3D
    if (hex_cr->IsModel()) {
        hex_cr->GetModel()->AnimationCallbacks.push_back({stateAnim, actionAnim, std::clamp(normalizedTime, 0.0f, 1.0f), [self, animCallback] {
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
        UNUSED_VARIABLE(stateAnim);
        UNUSED_VARIABLE(actionAnim);
        UNUSED_VARIABLE(normalizedTime);
        UNUSED_VARIABLE(animCallback);
    }
}

///@ ExportMethod
[[maybe_unused]] bool Client_Critter_GetBonePos(CritterView* self, hstring boneName, int& boneX, int& boneY)
{
#if FO_ENABLE_3D
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    boneX = hex_cr->ScrX;
    boneY = hex_cr->ScrY;

    if (!hex_cr->IsModel()) {
        return false;
    }

    const auto bone_pos = hex_cr->GetModel()->GetBonePos(boneName);
    if (!bone_pos) {
        return false;
    }

    boneX += std::get<0>(*bone_pos);
    boneY += std::get<1>(*bone_pos);

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

///@ ExportMethod
[[maybe_unused]] uint8 Client_Critter_GetAlpha(CritterView* self)
{
    const auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    return hex_cr != nullptr ? hex_cr->GetCurAlpha() : 0xFF;
}

///@ ExportMethod
[[maybe_unused]] void Client_Critter_SetAlpha(CritterView* self, uint8 alpha)
{
    if (auto* hex_cr = dynamic_cast<CritterHexView*>(self); hex_cr != nullptr) {
        hex_cr->SetTargetAlpha(alpha);
    }
}

///@ ExportMethod
[[maybe_unused]] void Client_Critter_SetContour(CritterView* self, ContourType contour)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(self);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    hex_cr->GetMap()->SetCritterContour(self->GetId(), contour);
}

///@ ExportMethod
[[maybe_unused]] void Client_Critter_MoveItemLocally(CritterView* self, ident_t itemId, uint itemCount, ident_t swapItemId, CritterItemSlot toSlot)
{
    auto* item = self->GetInvItem(itemId);
    auto* swap_item = swapItemId ? self->GetInvItem(swapItemId) : nullptr;

    if (item == nullptr) {
        throw ScriptException("Item not found");
    }
    if (swapItemId && swap_item == nullptr) {
        throw ScriptException("Swap item not found");
    }

    auto* old_item = item->CreateRefClone();
    const auto from_slot = item->GetCritterSlot();
    auto* map_cr = dynamic_cast<CritterHexView*>(self);

    if (toSlot == CritterItemSlot::Outside) {
        if (map_cr != nullptr) {
            map_cr->Action(CritterAction::DropItem, static_cast<int>(from_slot), item, true);
        }

        if (item->GetStackable() && itemCount < item->GetCount()) {
            item->SetCount(item->GetCount() - itemCount);
        }
        else {
            self->DeleteInvItem(item, true);
            item = nullptr;
        }
    }
    else {
        item->SetCritterSlot(toSlot);

        if (swap_item != nullptr) {
            swap_item->SetCritterSlot(from_slot);
        }

        if (map_cr != nullptr) {
            map_cr->Action(CritterAction::MoveItem, static_cast<int>(from_slot), item, true);

            if (swap_item != nullptr) {
                map_cr->Action(CritterAction::SwapItems, static_cast<int>(toSlot), swap_item, true);
            }
        }
    }

    // Light
    if (map_cr != nullptr) {
        map_cr->GetMap()->RebuildFog();
        map_cr->GetMap()->UpdateCritterLightSource(map_cr);
    }

    // Notify scripts about item changing
    self->GetEngine()->OnItemInvChanged.Fire(item, old_item);

    old_item->Release();
}
