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
#include "CritterHexView.h"
#include "CritterView.h"
#include "Geometry.h"
#include "MapView.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

static auto RequireHexCritter(ptr<CritterView> cr) -> ptr<CritterHexView>
{
    FO_STACK_TRACE_ENTRY();

    auto nullable_hex_cr = cr.dyn_cast<CritterHexView>();

    if (!nullable_hex_cr) {
        throw ScriptException("Critter is not on map");
    }

    return nullable_hex_cr.as_ptr();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Critter_SetName(ptr<CritterView> self, string_view name)
{
    self->SetName(name);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsOffline(ptr<CritterView> self)
{
    return self->GetControlledByPlayer() && self->GetIsPlayerOffline();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsAlive(ptr<CritterView> self)
{
    return self->IsAlive();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsKnockout(ptr<CritterView> self)
{
    return self->IsKnockout();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsDead(ptr<CritterView> self)
{
    return self->IsDead();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsOnMap(ptr<CritterView> self)
{
    auto hex_cr = self.dyn_cast<CritterHexView>();
    return static_cast<bool>(hex_cr);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsMoving(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    return hex_cr->IsMoving();
}

///@ ExportMethod
FO_SCRIPT_API nptr<MovingContext> Client_Critter_GetMovingContext(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    auto moving = hex_cr->GetMoving();
    return moving.get_no_const();
}

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

///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsVisible(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    return hex_cr->IsMapSpriteVisible();
}

///@ ExportMethod
FO_SCRIPT_API ipos32 Client_Critter_GetSpriteOffset(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    return hex_cr->GetSpriteOffset();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsAnimAvailable(ptr<CritterView> self, CritterStateAnim stateAnim, CritterActionAnim actionAnim)
{
    auto hex_cr = RequireHexCritter(self);
    return hex_cr->IsAnimAvailable(stateAnim, actionAnim);
}

///@ ExportMethod
FO_SCRIPT_API timespan Client_Critter_GetModelAnimDuration(ptr<CritterView> self, CritterStateAnim stateAnim, CritterActionAnim actionAnim)
{
#if FO_ENABLE_3D
    auto hex_cr = RequireHexCritter(self);

    if (!hex_cr->IsModel()) {
        return {};
    }

    auto nullable_model = hex_cr->GetModel();
    FO_VERIFY_AND_THROW(nullable_model, "Critter reports model but has no model instance");
    auto model = nullable_model.as_ptr();

    return model->GetAnimDuration(stateAnim, actionAnim);

#else
    ignore_unused(self);
    ignore_unused(stateAnim);
    ignore_unused(actionAnim);

    throw NotEnabled3DException("3D submodule not enabled");
#endif
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_IsAnimPlaying(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    return hex_cr->IsAnimPlaying();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Critter_Animate(ptr<CritterView> self, CritterStateAnim stateAnim, CritterActionAnim actionAnim, nptr<AbstractItem> contextItem = nullptr, bool append = false)
{
    auto hex_cr = RequireHexCritter(self);

    if (!append) {
        hex_cr->StopAnim();
    }

    nptr<Entity> context_item = contextItem;
    hex_cr->AppendAnim(stateAnim, actionAnim, context_item);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Critter_StopAnim(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    hex_cr->StopAnim();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Critter_RefreshView(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    hex_cr->RefreshView();
}

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

///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Critter_GetItem(ptr<CritterView> self, ident_t itemId)
{
    auto item = self->GetInvItem(itemId);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Critter_GetItem(ptr<CritterView> self, hstring protoId)
{
    auto nullable_proto = self->GetEngine()->GetProtoItem(protoId);

    if (!nullable_proto) {
        throw ScriptException("Invalid item proto id arg", protoId);
    }

    auto proto = nullable_proto.as_ptr();
    auto inv_items = self->GetInvItems();

    if (proto->GetStackable()) {
        for (size_t i = 0; i < inv_items.size(); i++) {
            auto item = inv_items[i].as_ptr();

            if (item->GetProtoId() == protoId) {
                return item.get_no_const();
            }
        }
    }
    else {
        nptr<ItemView> another_slot;

        for (size_t i = 0; i < inv_items.size(); i++) {
            auto item = inv_items[i].as_ptr();

            if (item->GetProtoId() == protoId) {
                if (item->GetCritterSlot() == CritterItemSlot::Inventory) {
                    return item.get_no_const();
                }

                another_slot = item;
            }
        }

        return another_slot.get_no_const();
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Critter_GetItem(ptr<CritterView> self, ptr<ProtoItem> proto)
{
    auto inv_items = self->GetInvItems();

    if (proto->GetStackable()) {
        for (size_t i = 0; i < inv_items.size(); i++) {
            auto item = inv_items[i].as_ptr();

            if (item->GetProtoId() == proto->GetProtoId()) {
                return item.get_no_const();
            }
        }
    }
    else {
        nptr<ItemView> another_slot;

        for (size_t i = 0; i < inv_items.size(); i++) {
            auto item = inv_items[i].as_ptr();

            if (item->GetProtoId() == proto->GetProtoId()) {
                if (item->GetCritterSlot() == CritterItemSlot::Inventory) {
                    return item.get_no_const();
                }

                another_slot = item;
            }
        }

        return another_slot.get_no_const();
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Critter_GetItem(ptr<CritterView> self, ItemProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<ItemView>(self->GetEngine(), property);
    auto inv_items = self->GetInvItems();

    for (size_t i = 0; i < inv_items.size(); i++) {
        auto item = inv_items[i].as_ptr();

        if (item->GetValueAsInt(prop) == propertyValue) {
            return item.get_no_const();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<ItemView*> Client_Critter_GetItems(ptr<CritterView> self)
{
    auto inv_items = self->GetInvItems();

    vector<ptr<ItemView>> items;
    items.reserve(inv_items.size());

    for (size_t i = 0; i < inv_items.size(); i++) {
        items.emplace_back(inv_items[i]);
    }

    return MakeScriptHandleVector<ItemView>(items);
}

///@ ExportMethod
FO_SCRIPT_API vector<ItemView*> Client_Critter_GetItems(ptr<CritterView> self, ItemProperty property, int32_t propertyValue)
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

    return MakeScriptHandleVector<ItemView>(items);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_GetTextPos(ptr<CritterView> self, ipos32& pos)
{
    auto hex_cr = RequireHexCritter(self);
    return hex_cr->GetNameTextPos(pos);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Critter_RunParticle(ptr<CritterView> self, string_view particleName, hstring boneName, float32_t moveX, float32_t moveY, float32_t moveZ)
{
    auto hex_cr = RequireHexCritter(self);

#if FO_ENABLE_3D
    if (nptr<ModelInstance> nullable_model = hex_cr->GetModel(); nullable_model) {
        auto model = nullable_model.as_ptr();
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

///@ ExportMethod
FO_SCRIPT_API void Client_Critter_AddAnimCallback(ptr<CritterView> self, CritterStateAnim stateAnim, CritterActionAnim actionAnim, float32_t normalizedTime, ScriptFunc<void, CritterView*> animCallback)
{
    auto hex_cr = RequireHexCritter(self);

#if FO_ENABLE_3D
    if (nptr<ModelInstance> nullable_model = hex_cr->GetModel(); nullable_model) {
        auto model = nullable_model.as_ptr();
        ModelAnimationCallback anim_callback;
        anim_callback.StateAnim = stateAnim;
        anim_callback.ActionAnim = actionAnim;
        anim_callback.NormalizedTime = std::clamp(normalizedTime, 0.0f, 1.0f);
        anim_callback.Callback = [self, animCallback = SafeAlloc::MakeShared<ScriptFunc<void, CritterView*>>(std::move(animCallback))]() mutable FO_DEFERRED {
            if (!self->IsDestroyed()) {
                animCallback->Call(self.get());
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

///@ ExportMethod
FO_SCRIPT_API bool Client_Critter_GetBonePos(ptr<CritterView> self, hstring boneName, ipos32& boneOffset)
{
#if FO_ENABLE_3D
    auto hex_cr = RequireHexCritter(self);
    boneOffset = hex_cr->GetSpriteOffset();

    if (!hex_cr->IsModel()) {
        return false;
    }

    auto nullable_model = hex_cr->GetModel();
    FO_VERIFY_AND_THROW(nullable_model, "Critter reports model but has no model instance");
    auto model = nullable_model.as_ptr();

    const auto bone_pos = model->GetBonePos(boneName);
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

///@ ExportMethod
FO_SCRIPT_API nptr<MovingContext> Client_Critter_MoveToHex(ptr<CritterView> self, mpos hex, ipos32 hexOffset, int32_t speed)
{
    auto hex_cr = RequireHexCritter(self);
    const int16_t ox = numeric_cast<int16_t>(std::clamp(hexOffset.x, -GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_WIDTH / 2));
    const int16_t oy = numeric_cast<int16_t>(std::clamp(hexOffset.y, -GameSettings::MAP_HEX_HEIGHT / 2, GameSettings::MAP_HEX_HEIGHT / 2));

    // No cut: move exactly onto the hex and stand at the requested sub-hex offset.
    auto engine_ptr = self->GetEngine();
    engine_ptr->CritterMoveTo(hex_cr, tuple {hex, ipos16 {ox, oy}, 0}, speed);
    auto moving = hex_cr->GetMoving();
    return moving.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<MovingContext> Client_Critter_MoveToHex(ptr<CritterView> self, mpos hex, int32_t cut, ipos32 hexOffset, int32_t speed)
{
    auto hex_cr = RequireHexCritter(self);
    const int16_t ox = numeric_cast<int16_t>(std::clamp(hexOffset.x, -GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_WIDTH / 2));
    const int16_t oy = numeric_cast<int16_t>(std::clamp(hexOffset.y, -GameSettings::MAP_HEX_HEIGHT / 2, GameSettings::MAP_HEX_HEIGHT / 2));

    auto engine_ptr = self->GetEngine();
    engine_ptr->CritterMoveTo(hex_cr, tuple {hex, ipos16 {ox, oy}, cut}, speed);
    auto moving = hex_cr->GetMoving();
    return moving.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Critter_MoveToDir(ptr<CritterView> self, mdir dir, int32_t speed)
{
    auto hex_cr = RequireHexCritter(self);
    auto engine_ptr = self->GetEngine();
    engine_ptr->CritterMoveTo(hex_cr, dir, speed);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Critter_StopMove(ptr<CritterView> self)
{
    auto hex_cr = RequireHexCritter(self);
    auto engine_ptr = self->GetEngine();
    engine_ptr->CritterMoveTo(hex_cr, mdir {0}, 0);
}

///@ ExportMethod
FO_SCRIPT_API int16_t Client_Critter_GetBodyAngle(ptr<CritterView> self)
{
#if FO_ENABLE_3D
    auto nullable_hex_cr = self.dyn_cast<CritterHexView>();

    if (nullable_hex_cr) {
        auto hex_cr = nullable_hex_cr.as_ptr();
        auto nullable_model = hex_cr->GetModel();

        if (nullable_model) {
            auto model = nullable_model.as_ptr();
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

///@ ExportMethod
FO_SCRIPT_API void Client_Critter_ChangeDir(ptr<CritterView> self, mdir dir)
{
    auto hex_cr = RequireHexCritter(self);
    auto engine_ptr = self->GetEngine();
    engine_ptr->CritterLookTo(hex_cr, dir);
}

///@ ExportMethod
FO_SCRIPT_API uint8_t Client_Critter_GetAlpha(ptr<CritterView> self)
{
    auto nullable_hex_cr = self.dyn_cast<CritterHexView>();

    if (!nullable_hex_cr) {
        return 0xFF;
    }

    auto hex_cr = nullable_hex_cr.as_ptr();
    return hex_cr->GetCurAlpha();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Critter_SetAlpha(ptr<CritterView> self, uint8_t alpha)
{
    auto nullable_hex_cr = self.dyn_cast<CritterHexView>();

    if (nullable_hex_cr) {
        auto hex_cr = nullable_hex_cr.as_ptr();
        hex_cr->SetTargetAlpha(alpha);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Critter_MoveItemLocally(ptr<CritterView> self, ident_t itemId, int32_t itemCount, ident_t swapItemId, CritterItemSlot toSlot)
{
    auto nullable_item = self->GetInvItem(itemId);
    nptr<ItemView> nullable_swap_item = swapItemId ? self->GetInvItem(swapItemId) : nullptr;

    if (!nullable_item) {
        throw ScriptException("Item not found");
    }
    if (swapItemId && !nullable_swap_item) {
        throw ScriptException("Swap item not found");
    }

    auto item = nullable_item.as_ptr();
    auto old_item = item->CreateRefClone();
    const CritterItemSlot from_slot = item->GetCritterSlot();
    auto nullable_map_cr = self.dyn_cast<CritterHexView>();

    if (toSlot == CritterItemSlot::Outside) {
        if (nullable_map_cr) {
            auto map_cr = nullable_map_cr.as_ptr();
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

        if (nullable_swap_item) {
            auto swap_item = nullable_swap_item.as_ptr();
            swap_item->SetCritterSlot(from_slot);
        }

        if (nullable_map_cr) {
            auto map_cr = nullable_map_cr.as_ptr();
            map_cr->Action(CritterAction::MoveItem, static_cast<int32_t>(from_slot), item, true);

            if (nullable_swap_item) {
                auto swap_item = nullable_swap_item.as_ptr();
                map_cr->Action(CritterAction::SwapItems, static_cast<int32_t>(toSlot), swap_item, true);
            }
        }
    }

    // Light
    if (nullable_map_cr) {
        auto map_cr = nullable_map_cr.as_ptr();
        map_cr->GetMap()->RebuildFog();
        map_cr->GetMap()->UpdateCritterLightSource(map_cr);
    }

    old_item->MarkAsDestroyed();
}

FO_END_NAMESPACE
