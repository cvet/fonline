//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// ReSharper disable CppInconsistentNaming

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
[[maybe_unused]] bool Client_Critter_IsPlayer(CritterView* self)
{
    return self->IsPlayer();
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
    return self->IsOffline();
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
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsFree(CritterView* self)
{
    return self->IsFree();
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Critter_IsBusy(CritterView* self)
{
    return !self->IsFree();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_IsModel(CritterView* self)
{
    return self->IsModel();
}

///# ...
///# param anim1 ...
///# param anim2 ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_IsAnimAvailable(CritterView* self, uint anim1, uint anim2)
{
    return self->IsAnimAvailable(anim1, anim2);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_IsAnimPlaying(CritterView* self)
{
    return self->IsAnim();
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
    self->Animate(anim1, anim2, nullptr);
}

///# ...
///# param anim1 ...
///# param anim2 ...
///# param actionItem ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_Animate(CritterView* self, uint anim1, uint anim2, ItemView* actionItem)
{
    self->Animate(anim1, anim2, actionItem);
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_StopAnim(CritterView* self)
{
    self->ClearAnim();
}

///# ...
///# param ms ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Client_Critter_Wait(CritterView* self, uint ms)
{
    self->TickStart(ms);
}

///# ...
///# param protoId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Critter_CountItem(CritterView* self, hstring protoId)
{
    return self->CountItemPid(protoId);
}

///# ...
///# param itemId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Critter_GetItem(CritterView* self, uint itemId)
{
    return self->GetItem(itemId);
}

///# ...
///# param protoId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Critter_GetItem(CritterView* self, hstring protoId)
{
    const auto* proto_item = self->GetEngine()->ProtoMngr->GetProtoItem(protoId);
    if (proto_item == nullptr) {
        throw ScriptException("Invalid proto id", protoId);
    }

    if (proto_item->GetStackable()) {
        for (auto* item : self->InvItems) {
            if (item->GetProtoId() == protoId) {
                return item;
            }
        }
    }
    else {
        ItemView* another_slot = nullptr;
        for (auto* item : self->InvItems) {
            if (item->GetProtoId() == protoId) {
                if (item->GetCritSlot() == 0) {
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
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Critter_GetItem(CritterView* self, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<ItemView>(self->GetEngine(), property);

    for (auto* item : self->InvItems) {
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
    return self->InvItems;
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
    items.reserve(self->InvItems.size());

    for (auto* item : self->InvItems) {
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
    self->Visible = visible;
    self->GetEngine()->HexMngr.RefreshMap();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_GetVisibility(CritterView* self)
{
    return self->Visible;
}

///# ...
///# param value ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_SetContourColor(CritterView* self, uint value)
{
    if (self->SprDrawValid) {
        self->SprDraw->SetContour(self->SprDraw->ContourType, value);
    }

    self->ContourColor = value;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Client_Critter_GetContourColor(CritterView* self)
{
    return self->ContourColor;
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
    self->GetNameTextInfo(nameVisible, x, y, w, h, lines);
}

///# ...
///# param anim1 ...
///# param anim2 ...
///# param normalizedTime ...
///# param animCallback ...
///@ ExportMethod
[[maybe_unused]] void Client_Critter_AddAnimCallback(CritterView* self, uint anim1, uint anim2, float normalizedTime, const std::function<void(CritterView*)>& animCallback)
{
    if (!self->IsModel()) {
        throw ScriptException("Critter is not 3D model");
    }
    if (normalizedTime < 0.0f || normalizedTime > 1.0f) {
        throw ScriptException("Normalized time is not in range 0..1", normalizedTime);
    }

    self->GetModel()->AnimationCallbacks.push_back({anim1, anim2, normalizedTime, [self, animCallback] {
                                                        if (!self->IsDestroyed()) {
                                                            animCallback(self);
                                                        }
                                                    }});
}

///# ...
///# param boneName ...
///# param boneX ...
///# param boneY ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Critter_GetBonePos(CritterView* self, hstring boneName, int& boneX, int& boneY)
{
    if (!self->IsModel()) {
        throw ScriptException("Critter is not 3d");
    }

    const auto bone_pos = self->GetModel()->GetBonePos(boneName);
    if (!bone_pos) {
        return false;
    }

    boneX = std::get<0>(*bone_pos) + self->SprOx;
    boneY = std::get<1>(*bone_pos) + self->SprOy;
    return true;
}
