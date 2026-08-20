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
#include "CritterHexView.h"
#include "CritterView.h"
#include "Geometry.h"
#include "MapView.h"
#include "ModelAnimation.h"
#include "ModelInstance.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

static auto RequireHexCritter(ptr<CritterView> cr) -> ptr<CritterHexView>
{
    FO_STACK_TRACE_ENTRY();

    auto hex_cr = cr.dyn_cast<CritterHexView>();

    if (!hex_cr) {
        throw ScriptException("Critter is not on map");
    }

    return hex_cr;
}

// Replaces the display name stored in this client-side critter view without updating authoritative server state.
///@ ExportMethod
FO_SCRIPT_API void Client_Critter_SetName(ptr<CritterView> self, string_view name)
{
    self->SetName(name);
}

// Reports whether this critter is player-controlled and its replicated player-offline flag is set.
///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsOffline(ptr<CritterView> self)
{
    return self->GetControlledByPlayer() && self->GetIsPlayerOffline();
}

// Reports whether the critter's replicated condition is alive.
///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsAlive(ptr<CritterView> self)
{
    return self->IsAlive();
}

// Reports whether the critter's replicated condition is knockout.
///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsKnockout(ptr<CritterView> self)
{
    return self->IsKnockout();
}

// Reports whether the critter's replicated condition is dead.
///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsDead(ptr<CritterView> self)
{
    return self->IsDead();
}

// Reports whether this client-side critter view is currently represented on the loaded map.
///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsOnMap(ptr<CritterView> self)
{
    auto hex_cr = self.dyn_cast<CritterHexView>();
    return static_cast<bool>(hex_cr);
}

// Reports whether this map critter is undergoing client-side movement; throws when the critter is not on the loaded map.
///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsMoving(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    return hex_cr->IsMoving();
}

// Returns the current client-side movement context for this map critter, or null when stationary; throws when the critter is not on the loaded map.
///@ ExportMethod
FO_SCRIPT_API nptr<MovingContext> Client_Critter_GetMovingContext(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    auto moving = hex_cr->GetMoving();
    return moving;
}

// Reports whether this map critter uses a loaded 3D model; returns false when 3D support is disabled and throws when the critter is not on the loaded map.
///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsModel(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);

#if FO_ENABLE_3D
    return hex_cr->IsModel();
#else
    return false;
#endif
}

// Returns whether this map critter's sprite currently participates in rendering; throws when the critter is not on the loaded map.
///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsVisible(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    return hex_cr->IsMapSpriteVisible();
}

// Returns the current rendered pixel offset of this map critter's sprite; throws when the critter is not on the loaded map.
///@ ExportMethod
FO_SCRIPT_API ipos32 Client_Critter_GetSpriteOffset(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    return hex_cr->GetSpriteOffset();
}

// Reports whether the current map presentation can resolve the requested state and action animation tuple; throws when the critter is not on the loaded map.
///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsAnimAvailable(ptr<CritterView> self, CritterStateAnim stateAnim, CritterActionAnim actionAnim)
{
    auto hex_cr = RequireHexCritter(self);
    return hex_cr->IsAnimAvailable(stateAnim, actionAnim);
}

// Returns the resolved runtime duration of an animation on the critter's loaded 3D model, or zero for a non-model or unresolved tuple; throws off-map or when 3D support is disabled.
///@ ExportMethod
FO_SCRIPT_API timespan Client_Critter_GetModelAnimDuration(ptr<CritterView> self, CritterStateAnim stateAnim, CritterActionAnim actionAnim)
{
#if FO_ENABLE_3D
    auto hex_cr = RequireHexCritter(self);

    if (!hex_cr->IsModel()) {
        return {};
    }

    auto model = hex_cr->GetModel();
    FO_VERIFY_AND_THROW(model, "Critter reports model but has no model instance");

    return model->GetAnimDuration(stateAnim, actionAnim);

#else
    ignore_unused(self);
    ignore_unused(stateAnim);
    ignore_unused(actionAnim);

    throw NotEnabled3DException("3D submodule not enabled");
#endif
}

// Reports whether this map critter currently has an active visual animation; throws when the critter is not on the loaded map.
///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsAnimPlaying(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    return hex_cr->IsAnimPlaying();
}

// Queues the requested visual animation with an optional item context, clearing the existing sequence first unless append is true; throws when the critter is off-map.
///@ ExportMethod
FO_SCRIPT_API void Client_Critter_Animate(ptr<CritterView> self, CritterStateAnim stateAnim, CritterActionAnim actionAnim, nptr<AbstractItem> contextItem = nullptr, bool append = false)
{
    auto hex_cr = RequireHexCritter(self);

    if (!append) {
        hex_cr->StopAnim();
    }

    auto context_item = contextItem;
    hex_cr->AppendAnim(stateAnim, actionAnim, context_item);
}

// Clears the active and queued visual animations of this map critter; throws when the critter is not on the loaded map.
///@ ExportMethod
FO_SCRIPT_API void Client_Critter_StopAnim(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    hex_cr->StopAnim();
}

// Reapplies the map critter's scale and movement presentation and refreshes its base visual animation when no explicit animation is active; throws off-map.
///@ ExportMethod
FO_SCRIPT_API void Client_Critter_RefreshView(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    hex_cr->RefreshView();
}

// Sums stack counts for visible inventory items with the requested prototype id, or for every item when the id is empty.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Critter_CountItem(ptr<CritterView> self, hstring protoId)
{
    auto inv_items = self->GetInvItems();
    int32_t result = 0;

    for (size_t i = 0; i < inv_items.size(); i++) {
        auto item = inv_items[i].as_ptr();

        if (!protoId || item->GetProtoId() == protoId) {
            result += item->GetCount();
        }
    }

    return result;
}

// Sums stack counts for visible inventory items whose prototype matches the supplied prototype.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Critter_CountItem(ptr<CritterView> self, ptr<ProtoItem> proto)
{
    auto inv_items = self->GetInvItems();
    int32_t result = 0;

    for (size_t i = 0; i < inv_items.size(); i++) {
        auto item = inv_items[i].as_ptr();

        if (item->GetProtoId() == proto->GetProtoId()) {
            result += item->GetCount();
        }
    }

    return result;
}

// Returns the visible inventory item with this entity id, or null when it is absent.
///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Critter_GetItem(ptr<CritterView> self, ident_t itemId)
{
    auto item = self->GetInvItem(itemId);
    return item;
}

// Returns a visible inventory item with this prototype id, preferring the Inventory slot for non-stackable items; throws when the prototype id is invalid.
///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Critter_GetItem(ptr<CritterView> self, hstring protoId)
{
    auto proto = self->GetEngine()->GetProtoItem(protoId);

    if (!proto) {
        throw ScriptException("Invalid item proto id arg", protoId);
    }

    auto inv_items = self->GetInvItems();

    if (proto->GetStackable()) {
        for (size_t i = 0; i < inv_items.size(); i++) {
            auto item = inv_items[i].as_ptr();

            if (item->GetProtoId() == protoId) {
                return item;
            }
        }
    }
    else {
        nptr<ItemView> another_slot;

        for (size_t i = 0; i < inv_items.size(); i++) {
            auto item = inv_items[i].as_ptr();

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

// Returns a visible inventory item matching the supplied prototype, preferring the Inventory slot for non-stackable items, or null when absent.
///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Critter_GetItem(ptr<CritterView> self, ptr<ProtoItem> proto)
{
    auto inv_items = self->GetInvItems();

    if (proto->GetStackable()) {
        for (size_t i = 0; i < inv_items.size(); i++) {
            auto item = inv_items[i].as_ptr();

            if (item->GetProtoId() == proto->GetProtoId()) {
                return item;
            }
        }
    }
    else {
        nptr<ItemView> another_slot;

        for (size_t i = 0; i < inv_items.size(); i++) {
            auto item = inv_items[i].as_ptr();

            if (item->GetProtoId() == proto->GetProtoId()) {
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

// Returns the first visible inventory item whose integer-convertible property equals the requested value, or null when none match.
///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Critter_GetItem(ptr<CritterView> self, ItemProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<ItemView>(self->GetEngine(), property);
    auto inv_items = self->GetInvItems();

    for (size_t i = 0; i < inv_items.size(); i++) {
        auto item = inv_items[i].as_ptr();

        if (item->GetValueAsInt(prop) == propertyValue) {
            return item;
        }
    }

    return nullptr;
}

// Returns a snapshot of handles to every item in this client-side inventory view.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<ItemView>> Client_Critter_GetItems(ptr<CritterView> self)
{
    auto inv_items = self->GetInvItems();

    vector<ptr<ItemView>> items;
    items.reserve(inv_items.size());

    for (size_t i = 0; i < inv_items.size(); i++) {
        items.emplace_back(inv_items[i]);
    }

    return items;
}

// Returns a snapshot of visible inventory items whose integer-convertible property equals the requested value.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<ItemView>> Client_Critter_GetItems(ptr<CritterView> self, ItemProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<ItemView>(self->GetEngine(), property);
    auto inv_items = self->GetInvItems();

    vector<ptr<ItemView>> items;
    items.reserve(inv_items.size());

    for (size_t i = 0; i < inv_items.size(); i++) {
        auto item = inv_items[i].as_ptr();

        if (item->GetValueAsInt(prop) == propertyValue) {
            items.emplace_back(item);
        }
    }

    return items;
}

// Writes the screen position for name text above a valid map sprite and returns true, or false when no valid sprite is available; throws when the critter is off-map.
///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_GetTextPos(ptr<CritterView> self, ipos32& pos)
{
    auto hex_cr = RequireHexCritter(self);
    return hex_cr->GetNameTextPos(pos);
}

// Starts a named particle on a bone of the critter's loaded 3D model with the supplied offset; does nothing for 2D or non-3D builds and throws off-map.
///@ ExportMethod
FO_SCRIPT_API void Client_Critter_RunParticle(ptr<CritterView> self, string_view particleName, hstring boneName, float32_t moveX, float32_t moveY, float32_t moveZ)
{
    auto hex_cr = RequireHexCritter(self);

#if FO_ENABLE_3D
    if (auto model = hex_cr->GetModel(); model) {
        model->RunParticle(particleName, boneName, vec3(moveX, moveY, moveZ));
    }
    else
#endif
    {
        ignore_unused(hex_cr);
        ignore_unused(particleName);
        ignore_unused(boneName);
        ignore_unused(moveX);
        ignore_unused(moveY);
        ignore_unused(moveZ);
    }
}

// Registers a deferred callback at the clamped normalized time of the selected 3D animation tuple; does nothing without a model, skips destroyed critters, and throws off-map.
///@ ExportMethod
FO_SCRIPT_API void Client_Critter_AddAnimCallback(ptr<CritterView> self, CritterStateAnim stateAnim, CritterActionAnim actionAnim, float32_t normalizedTime, ScriptFunc<void, ptr<CritterView>> animCallback)
{
    auto hex_cr = RequireHexCritter(self);

#if FO_ENABLE_3D
    if (auto model = hex_cr->GetModel(); model) {
        ModelAnimationCallback anim_callback;
        anim_callback.StateAnim = stateAnim;
        anim_callback.ActionAnim = actionAnim;
        anim_callback.NormalizedTime = std::clamp(normalizedTime, 0.0f, 1.0f);
        anim_callback.Callback = [self, animCallback = SafeAlloc::MakeShared<ScriptFunc<void, ptr<CritterView>>>(std::move(animCallback))]() mutable FO_DEFERRED {
            if (!self->IsDestroyed()) {
                animCallback->Call(self);
            }
        };

        model->AddAnimationCallback(std::move(anim_callback));
    }
    else
#endif
    {
        ignore_unused(hex_cr);
        ignore_unused(stateAnim);
        ignore_unused(actionAnim);
        ignore_unused(normalizedTime);
        ignore_unused(animCallback);
    }
}

// Writes the screen-space position of a named bone and returns true; returns false for a 2D critter or missing bone and throws when 3D support is disabled.
///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_GetBonePos(ptr<CritterView> self, hstring boneName, ipos32& boneOffset)
{
#if FO_ENABLE_3D
    auto hex_cr = RequireHexCritter(self);
    boneOffset = hex_cr->GetSpriteOffset();

    if (!hex_cr->IsModel()) {
        return false;
    }

    auto model = hex_cr->GetModel();
    FO_VERIFY_AND_THROW(model, "Critter reports model but has no model instance");

    auto bone_pos = model->GetBonePos(boneName);
    if (!bone_pos.has_value()) {
        return false;
    }

    boneOffset = boneOffset + bone_pos.value();

    return true;

#else
    ignore_unused(self);
    ignore_unused(boneName);
    ignore_unused(boneOffset);

    throw NotEnabled3DException("3D submodule not enabled");
#endif
}

// Starts local movement exactly to the target hex and clamped sub-hex offset, returning the movement context or null when no movement remains; throws off-map.
///@ ExportMethod
FO_SCRIPT_API nptr<MovingContext> Client_Critter_MoveToHex(ptr<CritterView> self, mpos hex, ipos32 hexOffset, int32_t speed)
{
    auto hex_cr = RequireHexCritter(self);
    int16_t ox = numeric_cast<int16_t>(std::clamp(hexOffset.x, -GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_WIDTH / 2));
    int16_t oy = numeric_cast<int16_t>(std::clamp(hexOffset.y, -GameSettings::MAP_HEX_HEIGHT / 2, GameSettings::MAP_HEX_HEIGHT / 2));

    // No cut: move exactly onto the hex and stand at the requested sub-hex offset
    auto engine = self->GetEngine();
    engine->CritterMoveTo(hex_cr, tuple {hex, ipos16 {ox, oy}, 0}, speed);
    auto moving = hex_cr->GetMoving();
    return moving;
}

// Starts local movement toward the target while retaining the requested cut distance and clamped sub-hex offset, returning the resulting context; throws off-map.
///@ ExportMethod
FO_SCRIPT_API nptr<MovingContext> Client_Critter_MoveToHex(ptr<CritterView> self, mpos hex, int32_t cut, ipos32 hexOffset, int32_t speed)
{
    auto hex_cr = RequireHexCritter(self);
    int16_t ox = numeric_cast<int16_t>(std::clamp(hexOffset.x, -GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_WIDTH / 2));
    int16_t oy = numeric_cast<int16_t>(std::clamp(hexOffset.y, -GameSettings::MAP_HEX_HEIGHT / 2, GameSettings::MAP_HEX_HEIGHT / 2));

    auto engine = self->GetEngine();
    engine->CritterMoveTo(hex_cr, tuple {hex, ipos16 {ox, oy}, cut}, speed);
    auto moving = hex_cr->GetMoving();
    return moving;
}

// Starts client-side movement of this map critter in the requested direction and at the requested speed; throws when the critter is off-map.
///@ ExportMethod
FO_SCRIPT_API void Client_Critter_MoveToDir(ptr<CritterView> self, mdir dir, int32_t speed)
{
    auto hex_cr = RequireHexCritter(self);
    auto engine = self->GetEngine();
    engine->CritterMoveTo(hex_cr, dir, speed);
}

// Stops the current client-side movement of this map critter; throws when the critter is not on the loaded map.
///@ ExportMethod
FO_SCRIPT_API void Client_Critter_StopMove(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    auto engine = self->GetEngine();
    engine->CritterMoveTo(hex_cr, mdir {0}, 0);
}

// Returns the rendered 3D body angle in degrees when a model is available, otherwise the critter's discrete direction angle.
///@ ExportMethod
FO_SCRIPT_API int16_t Client_Critter_GetBodyAngle(ptr<CritterView> self)
{
#if FO_ENABLE_3D
    auto hex_cr = self.dyn_cast<CritterHexView>();

    if (hex_cr) {
        auto model = hex_cr->GetModel();

        if (model) {
            float32_t a = 180.0f - model->GetMoveDirAngle();
            a = std::fmod(a, 360.0f);

            if (a < 0.0f) {
                a += 360.0f;
            }

            return iround<int16_t>(a);
        }
    }
#endif

    return self->GetDir().angle();
}

// Changes the local facing direction of this map critter through the normal client presentation path; throws when the critter is off-map.
///@ ExportMethod
FO_SCRIPT_API void Client_Critter_ChangeDir(ptr<CritterView> self, mdir dir)
{
    auto hex_cr = RequireHexCritter(self);
    auto engine = self->GetEngine();
    engine->CritterLookTo(hex_cr, dir);
}

// Returns the map sprite's current rendered alpha, or 255 when the critter is outside the map view.
///@ ExportMethod
FO_SCRIPT_API uint8_t Client_Critter_GetAlpha(ptr<CritterView> self)
{
    auto hex_cr = self.dyn_cast<CritterHexView>();

    if (!hex_cr) {
        return 0xFF;
    }

    return hex_cr->GetCurAlpha();
}

// Fades a map critter's sprite toward the requested target alpha; does nothing when the critter is outside the map view.
///@ ExportMethod
FO_SCRIPT_API void Client_Critter_SetAlpha(ptr<CritterView> self, uint8_t alpha)
{
    auto hex_cr = self.dyn_cast<CritterHexView>();

    if (hex_cr) {
        hex_cr->SetTargetAlpha(alpha);
    }
}

// Applies a local predicted drop, slot move, or optional slot swap and refreshes map action and lighting visuals; it does not send an authoritative server request.
///@ ExportMethod
FO_SCRIPT_API void Client_Critter_MoveItemLocally(ptr<CritterView> self, ident_t itemId, int32_t itemCount, ident_t swapItemId, CritterItemSlot toSlot)
{
    auto item = self->GetInvItem(itemId);
    auto swap_item = swapItemId ? self->GetInvItem(swapItemId) : nullptr;

    if (!item) {
        throw ScriptException("Item not found");
    }
    if (swapItemId && !swap_item) {
        throw ScriptException("Swap item not found");
    }

    auto old_item = item->CreateRefClone();
    CritterItemSlot from_slot = item->GetCritterSlot();
    auto map_cr = self.dyn_cast<CritterHexView>();

    if (toSlot == CritterItemSlot::Outside) {
        if (map_cr) {
            map_cr->Action(CritterAction::DropItem, static_cast<int32_t>(from_slot), item, true);
        }

        if (item->GetStackable() && itemCount < item->GetCount()) {
            item->SetCount(item->GetCount() - itemCount);
        }
        else {
            self->DeleteInvItem(item);
        }
    }
    else {
        item->SetCritterSlot(toSlot);

        if (swap_item) {
            swap_item->SetCritterSlot(from_slot);
        }

        if (map_cr) {
            map_cr->Action(CritterAction::MoveItem, static_cast<int32_t>(from_slot), item, true);

            if (swap_item) {
                map_cr->Action(CritterAction::SwapItems, static_cast<int32_t>(toSlot), swap_item, true);
            }
        }
    }

    // Light
    if (map_cr) {
        map_cr->GetMap()->RebuildFog();
        map_cr->GetMap()->UpdateCritterLightSource(map_cr);
    }

    old_item->MarkAsDestroyed();
}

FO_END_NAMESPACE
