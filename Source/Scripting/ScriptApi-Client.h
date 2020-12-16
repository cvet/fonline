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

#if FO_API_CLIENT_IMPL
#include "NetCommand.h"
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsChosen, FO_API_RET(bool))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->IsChosen());
}
FO_API_EPILOG(0)
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsPlayer, FO_API_RET(bool))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->IsPlayer());
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsNpc, FO_API_RET(bool))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->IsNpc());
}
FO_API_EPILOG(0)
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsOffline, FO_API_RET(bool))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->IsOffline());
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsAlive, FO_API_RET(bool))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->IsAlive());
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsKnockout, FO_API_RET(bool))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->IsKnockout());
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsDead, FO_API_RET(bool))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->IsDead());
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsFree, FO_API_RET(bool))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->IsFree());
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsBusy, FO_API_RET(bool))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(!_critterView->IsFree());
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsModel, FO_API_RET(bool))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->Model != nullptr);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param anim1 ...
 * @param anim2 ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsAnimAvailable, FO_API_RET(bool), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2))
{
    FO_API_RETURN(_critterView->IsAnimAvailable(anim1, anim2));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsAnimPlaying, FO_API_RET(bool))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->IsAnim());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetAnim1, FO_API_RET(uint))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->GetAnim1());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param anim1 ...
 * @param anim2 ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(Animate, FO_API_RET(void), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2))
{
    _critterView->Animate(anim1, anim2, nullptr);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param anim1 ...
 * @param anim2 ...
 * @param actionItem ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(AnimateExt, FO_API_RET(void), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2), FO_API_ARG_OBJ(ItemView, actionItem))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2) FO_API_ARG_OBJ_MARSHAL(ItemView, actionItem))
{
    _critterView->Animate(anim1, anim2, actionItem);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(StopAnim, FO_API_RET(void))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    _critterView->ClearAnim();
}
FO_API_EPILOG()
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param ms ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(Wait, FO_API_RET(void), FO_API_ARG(uint, ms))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, ms))
{
    _critterView->TickStart(ms);
}
FO_API_EPILOG()
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param protoId ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(CountItem, FO_API_RET(uint), FO_API_ARG(hash, protoId))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, protoId))
{
    FO_API_RETURN(_critterView->CountItemPid(protoId));
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param itemId ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItem, FO_API_RET_OBJ(ItemView), FO_API_ARG(uint, itemId))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId))
{
    FO_API_RETURN(_critterView->GetItem(itemId));
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param predicate ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItemByPredicate, FO_API_RET_OBJ(ItemView), FO_API_ARG_PREDICATE(ItemView, predicate))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_PREDICATE_MARSHAL(ItemView, predicate))
{
    auto inv_items = _critterView->InvItems;
    for (auto* item : inv_items) {
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed) {
            FO_API_RETURN(item);
        }
    }
    FO_API_RETURN(static_cast<ItemView*>(nullptr));
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param slot ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItemBySlot, FO_API_RET_OBJ(ItemView), FO_API_ARG(int, slot))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, slot))
{
    FO_API_RETURN(_critterView->GetItemSlot(slot));
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param protoId ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItemByPid, FO_API_RET_OBJ(ItemView), FO_API_ARG(hash, protoId))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, protoId))
{
    const auto* proto_item = _client->ProtoMngr.GetProtoItem(protoId);
    if (!proto_item) {
        FO_API_RETURN(static_cast<ItemView*>(nullptr));
    }

    if (proto_item->GetStackable()) {
        for (auto* item : _critterView->InvItems) {
            if (item->GetProtoId() == protoId) {
                FO_API_RETURN(item);
            }
        }
    }
    else {
        ItemView* another_slot = nullptr;
        for (auto* item : _critterView->InvItems) {
            if (item->GetProtoId() == protoId) {
                if (!item->GetCritSlot()) {
                    FO_API_RETURN(item);
                }
                another_slot = item;
            }
        }
        FO_API_RETURN(another_slot);
    }

    FO_API_RETURN(static_cast<ItemView*>(nullptr));
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItems, FO_API_RET_OBJ_ARR(ItemView))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->InvItems);
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param slot ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItemsBySlot, FO_API_RET_OBJ_ARR(ItemView), FO_API_ARG(int, slot))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, slot))
{
    FO_API_RETURN(_critterView->GetItemsSlot(slot));
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param predicate ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItemsByPredicate, FO_API_RET_OBJ_ARR(ItemView), FO_API_ARG_PREDICATE(ItemView, predicate))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_PREDICATE_MARSHAL(ItemView, predicate))
{
    auto inv_items = _critterView->InvItems;
    vector<ItemView*> items;
    items.reserve(inv_items.size());
    for (auto* item : inv_items) {
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed) {
            items.push_back(item);
        }
    }
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param visible ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(SetVisibility, FO_API_RET(void), FO_API_ARG(bool, visible))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, visible))
{
    _critterView->Visible = visible;
    _client->HexMngr.RefreshMap();
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetVisibility, FO_API_RET(bool))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->Visible);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param value ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(SetContourColor, FO_API_RET(void), FO_API_ARG(uint, value))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, value))
{
    if (_critterView->SprDrawValid) {
        _critterView->SprDraw->SetContour(_critterView->SprDraw->ContourType, value);
    }

    _critterView->ContourColor = value;
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetContourColor, FO_API_RET(uint))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critterView->ContourColor);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param nameVisible ...
 * @param x ...
 * @param y ...
 * @param w ...
 * @param h ...
 * @param lines ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetNameTextInfo, FO_API_RET(void), FO_API_ARG_REF(bool, nameVisible), FO_API_ARG_REF(int, x), FO_API_ARG_REF(int, y), FO_API_ARG_REF(int, w), FO_API_ARG_REF(int, h), FO_API_ARG_REF(int, lines))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(bool, nameVisible) FO_API_ARG_REF_MARSHAL(int, x) FO_API_ARG_REF_MARSHAL(int, y) FO_API_ARG_REF_MARSHAL(int, w) FO_API_ARG_REF_MARSHAL(int, h) FO_API_ARG_REF_MARSHAL(int, lines))
{
    _critterView->GetNameTextInfo(nameVisible, x, y, w, h, lines);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param anim1 ...
 * @param anim2 ...
 * @param normalizedTime ...
 * @param animCallback ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(AddAnimCallback, FO_API_RET(void), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2), FO_API_ARG(float, normalizedTime), FO_API_ARG_CALLBACK(CritterView, animCallback))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2) FO_API_ARG_MARSHAL(float, normalizedTime) FO_API_ARG_CALLBACK_MARSHAL(CritterView, animCallback))
{
    if (_critterView->Model == nullptr) {
        throw ScriptException("Critter is not 3D model");
    }
    if (normalizedTime < 0.0f || normalizedTime > 1.0f) {
        throw ScriptException("Normalized time is not in range 0..1", normalizedTime);
    }

    _critterView->Model->AnimationCallbacks.push_back({anim1, anim2, normalizedTime, [_critterView, animCallback] {
                                                           if (!_critterView->IsDestroyed) {
                                                               animCallback(_critterView);
                                                           }
                                                       }});
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param boneName ...
 * @param boneX ...
 * @param boneY ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetBonePos, FO_API_RET(bool), FO_API_ARG(hash, boneName), FO_API_ARG_REF(int, boneX), FO_API_ARG_REF(int, boneY))
#if FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, boneName) FO_API_ARG_REF_MARSHAL(int, boneX) FO_API_ARG_REF_MARSHAL(int, boneY))
{
    if (!_critterView->Model) {
        throw ScriptException("Critter is not 3d");
    }

    const auto bone_pos = _critterView->Model->GetBonePos(boneName);
    if (!bone_pos) {
        FO_API_RETURN(false);
    }

    boneX = std::get<0>(*bone_pos) + _critterView->SprOx;
    boneY = std::get<1>(*bone_pos) + _critterView->SprOy;
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
FO_API_ITEM_VIEW_METHOD(GetMapPos, FO_API_RET(bool), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy))
#if FO_API_ITEM_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, hx) FO_API_ARG_REF_MARSHAL(ushort, hy))
{
    if (!_client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }

    switch (_itemView->GetAccessory()) {
    case ITEM_ACCESSORY_CRITTER: {
        auto* const cr = _client->GetCritter(_itemView->GetCritId());
        if (!cr) {
            throw ScriptException("CritterCl accessory, CritterCl not found");
        }
        hx = cr->GetHexX();
        hy = cr->GetHexY();
    } break;
    case ITEM_ACCESSORY_HEX: {
        hx = _itemView->GetHexX();
        hy = _itemView->GetHexY();
    } break;
    case ITEM_ACCESSORY_CONTAINER: {
        if (_itemView->GetId() == _itemView->GetContainerId()) {
            throw ScriptException("Container accessory, crosslinks");
        }

        ItemView* cont = _client->GetItem(_itemView->GetContainerId());
        if (!cont) {
            throw ScriptException("Container accessory, container not found");
        }

        // FO_API_RETURN(Item_GetMapPosition(cont, hx, hy)); // Todo: solve recursion in GetMapPos
    } break;
    default:
        throw ScriptException("Unknown accessory");
    }

    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param fromFrame ...
 * @param toFrame ...
 ******************************************************************************/
FO_API_ITEM_VIEW_METHOD(Animate, FO_API_RET(void), FO_API_ARG(uint, fromFrame), FO_API_ARG(uint, toFrame))
#if FO_API_ITEM_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, fromFrame) FO_API_ARG_MARSHAL(uint, toFrame))
{
    if (_itemView->Type == EntityType::ItemHexView) {
        auto* item_hex = static_cast<ItemHexView*>(_itemView);
        item_hex->SetAnim(fromFrame, toFrame);
    }
}
FO_API_EPILOG()
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param stackId ...
 * @return ...
 ******************************************************************************/
FO_API_ITEM_VIEW_METHOD(GetItems, FO_API_RET_OBJ_ARR(ItemView), FO_API_ARG(uint, stackId))
#if FO_API_ITEM_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, stackId))
{
    vector<ItemView*> items;
    // Todo: need attention!
    // _itemView->ContGetItems(items, stackId);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param command ...
 * @param separator ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(CustomCall, FO_API_RET(string), FO_API_ARG(string, command), FO_API_ARG(string, separator))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, command) FO_API_ARG_MARSHAL(string, separator))
{
    // Parse command
    vector<string> args;
    std::stringstream ss(command);
    if (separator.length() > 0) {
        string arg;
        while (getline(ss, arg, *separator.c_str())) {
            args.push_back(arg);
        }
    }
    else {
        args.push_back(command);
    }
    if (args.empty()) {
        throw ScriptException("Empty custom call command");
    }

    // Execute
    auto cmd = args[0];
    if (cmd == "Login" && args.size() >= 3) {
        if (_client->InitNetReason == INIT_NET_REASON_NONE) {
            _client->LoginName = args[1];
            _client->LoginPassword = args[2];
            _client->InitNetReason = INIT_NET_REASON_LOGIN;
        }
    }
    else if (cmd == "Register" && args.size() >= 3) {
        if (_client->InitNetReason == INIT_NET_REASON_NONE) {
            _client->LoginName = args[1];
            _client->LoginPassword = args[2];
            _client->InitNetReason = INIT_NET_REASON_REG;
        }
    }
    else if (cmd == "CustomConnect") {
        if (_client->InitNetReason == INIT_NET_REASON_NONE) {
            _client->InitNetReason = INIT_NET_REASON_CUSTOM;
            if (!_client->Update) {
                _client->Update = FOClient::ClientUpdate();
            }
        }
    }
    else if (cmd == "DumpAtlases") {
        _client->SprMngr.DumpAtlases();
    }
    else if (cmd == "SwitchShowTrack") {
        _client->HexMngr.SwitchShowTrack();
    }
    else if (cmd == "SwitchShowHex") {
        _client->HexMngr.SwitchShowHex();
    }
    else if (cmd == "SwitchFullscreen") {
        if (!_client->Settings.FullScreen) {
            if (_client->SprMngr.EnableFullscreen()) {
                _client->Settings.FullScreen = true;
            }
        }
        else {
            if (_client->SprMngr.DisableFullscreen()) {
                _client->Settings.FullScreen = false;

                if (_client->WindowResolutionDiffX || _client->WindowResolutionDiffY) {
                    const auto [x, y] = _client->SprMngr.GetWindowPosition();
                    _client->SprMngr.SetWindowPosition(x - _client->WindowResolutionDiffX, y - _client->WindowResolutionDiffY);
                    _client->WindowResolutionDiffX = _client->WindowResolutionDiffY = 0;
                }
            }
        }
    }
    else if (cmd == "MinimizeWindow") {
        _client->SprMngr.MinimizeWindow();
    }
    else if (cmd == "SwitchLookBorders") {
        // _client->DrawLookBorders = !_client->DrawLookBorders;
        // _client->RebuildLookBorders = true;
    }
    else if (cmd == "SwitchShootBorders") {
        // _client->DrawShootBorders = !_client->DrawShootBorders;
        // _client->RebuildLookBorders = true;
    }
    else if (cmd == "GetShootBorders") {
        FO_API_RETURN(_client->DrawShootBorders ? "true" : "false");
    }
    else if (cmd == "SetShootBorders" && args.size() >= 2) {
        auto set = (args[1] == "true");
        if (_client->DrawShootBorders != set) {
            _client->DrawShootBorders = set;
            _client->RebuildLookBorders = true;
        }
    }
    else if (cmd == "SetMousePos" && args.size() == 4) {
#if !FO_WEB
        /*int x = _str(args[1]).toInt();
        int y = _str(args[2]).toInt();
        bool motion = _str(args[3]).toBool();
        if (motion)
        {
            _client->SprMngr.SetMousePosition(x, y);
        }
        else
        {
            SDL_EventState(SDL_MOUSEMOTION, SDL_DISABLE);
            _client->SprMngr.SetMousePosition(x, y);
            SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
            _client->Settings.MouseX = _client->Settings.LastMouseX = x;
            _client->Settings.MouseY = _client->Settings.LastMouseY = y;
        }*/
#endif
    }
    else if (cmd == "SetCursorPos") {
        if (_client->HexMngr.IsMapLoaded()) {
            _client->HexMngr.SetCursorPos(_client->Settings.MouseX, _client->Settings.MouseY, _client->Keyb.CtrlDwn, true);
        }
    }
    else if (cmd == "NetDisconnect") {
        _client->NetDisconnect();

        if (!_client->IsConnected && !_client->IsMainScreen(SCREEN_LOGIN)) {
            _client->ShowMainScreen(SCREEN_LOGIN, {});
        }
    }
    else if (cmd == "TryExit") {
        _client->TryExit();
    }
    else if (cmd == "Version") {
        // FO_API_RETURN(_str("{}", FO_VERSION));
        FO_API_RETURN(_str("{}", "Unsupported"));
    }
    else if (cmd == "BytesSend") {
        FO_API_RETURN(_str("{}", _client->BytesSend));
    }
    else if (cmd == "BytesReceive") {
        FO_API_RETURN(_str("{}", _client->BytesReceive));
    }
    else if (cmd == "GetLanguage") {
        FO_API_RETURN(_client->CurLang.Name);
    }
    else if (cmd == "SetLanguage" && args.size() >= 2) {
        if (args[1].length() == 4) {
            _client->CurLang.LoadFromCache(_client->Cache, args[1]);
        }
    }
    else if (cmd == "SetResolution" && args.size() >= 3) {
        auto w = _str(args[1]).toInt();
        auto h = _str(args[2]).toInt();
        auto diff_w = w - _client->Settings.ScreenWidth;
        auto diff_h = h - _client->Settings.ScreenHeight;

        _client->Settings.ScreenWidth = w;
        _client->Settings.ScreenHeight = h;
        _client->SprMngr.SetWindowSize(w, h);

        if (!_client->Settings.FullScreen) {
            const auto [x, y] = _client->SprMngr.GetWindowPosition();
            _client->SprMngr.SetWindowPosition(x - diff_w / 2, y - diff_h / 2);
        }
        else {
            _client->WindowResolutionDiffX += diff_w / 2;
            _client->WindowResolutionDiffY += diff_h / 2;
        }

        _client->SprMngr.OnResolutionChanged();
        if (_client->HexMngr.IsMapLoaded()) {
            _client->HexMngr.OnResolutionChanged();
        }
    }
    else if (cmd == "RefreshAlwaysOnTop") {
        _client->SprMngr.SetAlwaysOnTop(_client->Settings.AlwaysOnTop);
    }
    else if (cmd == "Command" && args.size() >= 2) {
        string str;
        for (size_t i = 1; i < args.size(); i++) {
            str += args[i] + " ";
        }
        str = _str(str).trim();

        string buf;
        if (!PackNetCommand(
                str, &_client->Bout, [&buf, &separator](auto s) { buf += s + separator; }, _client->Chosen->AlternateName)) {
            FO_API_RETURN("UNKNOWN");
        }

        FO_API_RETURN(buf);
    }
    else if (cmd == "ConsoleMessage" && args.size() >= 2) {
        _client->Net_SendText(args[1].c_str(), SAY_NORM);
    }
    else if (cmd == "SaveLog" && args.size() == 3) {
        //              if( file_name == "" )
        //              {
        //                      DateTime dt;
        //                      Timer::GetCurrentDateTime(dt);
        //                      char     log_path[TEMP_BUF_SIZE];
        //                      X_str(log_path, "messbox_%04d.%02d.%02d_%02d-%02d-%02d.txt",
        //                              dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);
        //              }

        //              for (uint i = 0; i < MessBox.size(); ++i)
        //              {
        //                      MessBoxMessage& m = MessBox[i];
        //                      // Skip
        //                      if (IsMainScreen(SCREEN_GAME) && std::find(MessBoxFilters.begin(), MessBoxFilters.end(),
        //                      m.Type) != MessBoxFilters.end())
        //                              continue;
        //                      // Concat
        //                      Str::Copy(cur_mess, m.Mess.c_str());
        //                      Str::EraseWords(cur_mess, '|', ' ');
        //                      fmt_log += MessBox[i].Time + string(cur_mess);
        //              }
    }
    else if (cmd == "DialogAnswer" && args.size() >= 4) {
        auto is_npc = (args[1] == "true");
        auto talker_id = _str(args[2]).toUInt();
        auto answer_index = _str(args[3]).toUInt();
        _client->Net_SendTalk(is_npc, talker_id, static_cast<uchar>(answer_index));
    }
    else if (cmd == "DrawMiniMap" && args.size() >= 6) {
        static int zoom;
        static int x;
        static int y;
        static int x2;
        static int y2;
        zoom = _str(args[1]).toInt();
        x = _str(args[2]).toInt();
        y = _str(args[3]).toInt();
        x2 = x + _str(args[4]).toInt();
        y2 = y + _str(args[5]).toInt();

        if (zoom != _client->LmapZoom || x != _client->LmapWMap[0] || y != _client->LmapWMap[1] || x2 != _client->LmapWMap[2] || y2 != _client->LmapWMap[3]) {
            _client->LmapZoom = zoom;
            _client->LmapWMap[0] = x;
            _client->LmapWMap[1] = y;
            _client->LmapWMap[2] = x2;
            _client->LmapWMap[3] = y2;
            _client->LmapPrepareMap();
        }
        else if (_client->GameTime.FrameTick() >= _client->LmapPrepareNextTick) {
            _client->LmapPrepareMap();
        }

        _client->SprMngr.DrawPoints(_client->LmapPrepPix, RenderPrimitiveType::LineList, nullptr, nullptr, nullptr);
    }
    else if (cmd == "RefreshMe") {
        _client->Net_SendRefereshMe();
    }
    else if (cmd == "SetCrittersContour" && args.size() == 2) {
        auto countour_type = _str(args[1]).toInt();
        _client->HexMngr.SetCrittersContour(countour_type);
    }
    else if (cmd == "DrawWait") {
        _client->WaitDraw();
    }
    else if (cmd == "ChangeDir" && args.size() == 2) {
        auto dir = _str(args[1]).toInt();
        _client->Chosen->ChangeDir(static_cast<uchar>(dir), true);
        _client->Net_SendDir();
    }
    else if (cmd == "MoveItem" && args.size() == 5) {
        auto item_count = _str(args[1]).toUInt();
        auto item_id = _str(args[2]).toUInt();
        auto item_swap_id = _str(args[3]).toUInt();
        auto to_slot = _str(args[4]).toInt();
        auto* item = _client->Chosen->GetItem(item_id);
        auto* item_swap = (item_swap_id ? _client->Chosen->GetItem(item_swap_id) : nullptr);
        auto* old_item = item->Clone();
        int from_slot = item->GetCritSlot();

        // Move
        auto is_light = item->GetIsLight();
        if (to_slot == -1) {
            _client->Chosen->Action(ACTION_DROP_ITEM, from_slot, item, true);
            if (item->GetStackable() && item_count < item->GetCount()) {
                item->SetCount(item->GetCount() - item_count);
            }
            else {
                _client->Chosen->DeleteItem(item, true);
                item = nullptr;
            }
        }
        else {
            item->SetCritSlot(static_cast<uchar>(to_slot));
            if (item_swap) {
                item_swap->SetCritSlot(static_cast<uchar>(from_slot));
            }

            _client->Chosen->Action(ACTION_MOVE_ITEM, from_slot, item, true);
            if (item_swap) {
                _client->Chosen->Action(ACTION_MOVE_ITEM_SWAP, to_slot, item_swap, true);
            }
        }

        // Light
        _client->RebuildLookBorders = true;
        if (is_light && (!to_slot || (!from_slot && to_slot != -1))) {
            _client->HexMngr.RebuildLight();
        }

        // Notify scripts about item changing
        _client->OnItemInvChanged(old_item, item);
    }
    else if (cmd == "SkipRoof" && args.size() == 3) {
        auto hx = _str(args[1]).toUInt();
        auto hy = _str(args[2]).toUInt();
        _client->HexMngr.SetSkipRoof(hx, hy);
    }
    else if (cmd == "RebuildLookBorders") {
        _client->RebuildLookBorders = true;
    }
    else if (cmd == "TransitCritter" && args.size() == 5) {
        auto hx = _str(args[1]).toInt();
        auto hy = _str(args[2]).toInt();
        auto animate = _str(args[3]).toBool();
        auto force = _str(args[4]).toBool();

        _client->HexMngr.TransitCritter(_client->Chosen, hx, hy, animate, force);
    }
    else if (cmd == "SendMove") {
        vector<uchar> dirs;
        for (size_t i = 1; i < args.size(); i++) {
            dirs.push_back(static_cast<uchar>(_str(args[i]).toInt()));
        }

        _client->Net_SendMove(dirs);

        if (dirs.size() > 1) {
            _client->Chosen->MoveSteps.resize(1);
        }
        else {
            _client->Chosen->MoveSteps.resize(0);
            if (!_client->Chosen->IsAnim()) {
                _client->Chosen->AnimateStay();
            }
        }
    }
    else if (cmd == "ChosenAlpha" && args.size() == 2) {
        auto alpha = _str(args[1]).toInt();

        _client->Chosen->Alpha = static_cast<uchar>(alpha);
    }
    else if (cmd == "SetScreenKeyboard" && args.size() == 2) {
        /*if (SDL_HasScreenKeyboardSupport())
        {
            bool cur = (SDL_IsTextInputActive() != SDL_FALSE);
            bool next = _str(args[1]).toBool();
            if (cur != next)
            {
                if (next)
                    SDL_StartTextInput();
                else
                    SDL_StopTextInput();
            }
        }*/
    }
    else {
        throw ScriptException("Invalid custom call command");
    }
    FO_API_RETURN("");
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetChosen, FO_API_RET_OBJ(CritterView))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    if (_client->Chosen && _client->Chosen->IsDestroyed) {
        FO_API_RETURN(static_cast<CritterView*>(nullptr));
    }
    FO_API_RETURN(_client->Chosen);
}
FO_API_EPILOG(0)
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param itemId ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetItem, FO_API_RET_OBJ(ItemView), FO_API_ARG(uint, itemId))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId))
{
    if (itemId == 0u) {
        throw ScriptException("Item id arg is zero");
    }

    // On map
    ItemView* item = _client->GetItem(itemId);

    // On Chosen
    if (!item && _client->Chosen) {
        item = _client->Chosen->GetItem(itemId);
    }

    // On other critters
    if (item == nullptr) {
        for (auto it = _client->HexMngr.GetCritters().begin(); !item && it != _client->HexMngr.GetCritters().end(); ++it) {
            if (!it->second->IsChosen()) {
                item = it->second->GetItem(itemId);
            }
        }
    }

    if (item == nullptr || item->IsDestroyed) {
        FO_API_RETURN(static_cast<ItemView*>(nullptr));
    }
    FO_API_RETURN(item);
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetVisibleItems, FO_API_RET_OBJ_ARR(ItemView))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    vector<ItemView*> items;
    if (_client->HexMngr.IsMapLoaded()) {
        auto& items_ = _client->HexMngr.GetItems();
        for (auto it = items_.begin(); it != items_.end();) {
            it = ((*it)->IsFinishing() ? items_.erase(it) : ++it);
        }
        for (ItemView* item : items_) {
            items.push_back(item);
        }
    }
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetVisibleItemsOnHex, FO_API_RET_OBJ_ARR(ItemHexView), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    vector<ItemHexView*> items;
    if (_client->HexMngr.IsMapLoaded()) {
        _client->HexMngr.GetItems(hx, hy, items);
        for (auto it = items.begin(); it != items.end();) {
            it = ((*it)->IsFinishing() ? items.erase(it) : ++it);
        }
    }
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param cr1 ...
 * @param cr2 ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCritterDistance, FO_API_RET(int), FO_API_ARG_OBJ(CritterView, cr1), FO_API_ARG_OBJ(CritterView, cr2))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(CritterView, cr1) FO_API_ARG_OBJ_MARSHAL(CritterView, cr2))
{
    if (!_client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }
    if (cr1 == nullptr) {
        throw ScriptException("Critter1 arg is null");
    }
    if (cr2 == nullptr) {
        throw ScriptException("Critter2 arg is null");
    }

    FO_API_RETURN(_client->GeomHelper.DistGame(cr1->GetHexX(), cr1->GetHexY(), cr2->GetHexX(), cr2->GetHexY()));
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param critterId ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCritter, FO_API_RET_OBJ(CritterView), FO_API_ARG(uint, critterId))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, critterId))
{
    if (critterId == 0u) {
        FO_API_RETURN(static_cast<CritterView*>(nullptr)); // throw ScriptException("Critter id arg is zero");
    }
    auto* const cr = _client->GetCritter(critterId);
    if (!cr || cr->IsDestroyed) {
        FO_API_RETURN(static_cast<CritterView*>(nullptr));
    }
    FO_API_RETURN(cr);
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param radius ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCrittersAroundHex, FO_API_RET_OBJ_ARR(CritterView), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, radius), FO_API_ARG(uchar, findType))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, radius) FO_API_ARG_MARSHAL(uchar, findType))
{
    if (hx >= _client->HexMngr.GetWidth() || hy >= _client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    auto& crits = _client->HexMngr.GetCritters();
    vector<CritterView*> critters;
    for (auto it = crits.begin(), end = crits.end(); it != end; ++it) {
        auto* cr = it->second;
        if (cr->CheckFind(findType) && _client->GeomHelper.CheckDist(hx, hy, cr->GetHexX(), cr->GetHexY(), radius)) {
            critters.push_back(cr);
        }
    }

    std::sort(critters.begin(), critters.end(), [_client, &hx, &hy](CritterView* cr1, CritterView* cr2) { return _client->GeomHelper.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY()) < _client->GeomHelper.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY()); });

    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCrittersByPids, FO_API_RET_OBJ_ARR(CritterView), FO_API_ARG(hash, pid), FO_API_ARG(uchar, findType))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_MARSHAL(uchar, findType))
{
    auto& crits = _client->HexMngr.GetCritters();
    vector<CritterView*> critters;
    if (pid == 0u) {
        for (auto it = crits.begin(), end = crits.end(); it != end; ++it) {
            auto* cr = it->second;
            if (cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }
    else {
        for (auto it = crits.begin(), end = crits.end(); it != end; ++it) {
            auto* cr = it->second;
            if (cr->IsNpc() && cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }
    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param fromHx ...
 * @param fromHy ...
 * @param toHx ...
 * @param toHy ...
 * @param angle ...
 * @param dist ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCrittersInPath, FO_API_RET_OBJ_ARR(CritterView), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(float, angle), FO_API_ARG(uint, dist), FO_API_ARG(int, findType))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist) FO_API_ARG_MARSHAL(int, findType))
{
    vector<CritterView*> critters;
    _client->HexMngr.TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, nullptr, false, &critters, findType, nullptr, nullptr, nullptr, true);
    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param fromHx ...
 * @param fromHy ...
 * @param toHx ...
 * @param toHy ...
 * @param angle ...
 * @param dist ...
 * @param findType ...
 * @param preBlockHx ...
 * @param preBlockHy ...
 * @param blockHx ...
 * @param blockHy ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCrittersWithBlockInPath, FO_API_RET_OBJ_ARR(CritterView), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(float, angle), FO_API_ARG(uint, dist), FO_API_ARG(int, findType), FO_API_ARG_REF(ushort, preBlockHx), FO_API_ARG_REF(ushort, preBlockHy), FO_API_ARG_REF(ushort, blockHx), FO_API_ARG_REF(ushort, blockHy))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist) FO_API_ARG_MARSHAL(int, findType) FO_API_ARG_REF_MARSHAL(ushort, preBlockHx) FO_API_ARG_REF_MARSHAL(ushort, preBlockHy) FO_API_ARG_REF_MARSHAL(ushort, blockHx) FO_API_ARG_REF_MARSHAL(ushort, blockHy))
{
    vector<CritterView*> critters;
    pair<ushort, ushort> block;
    pair<ushort, ushort> pre_block;
    _client->HexMngr.TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, nullptr, false, &critters, findType, &block, &pre_block, nullptr, true);
    preBlockHx = pre_block.first;
    preBlockHy = pre_block.second;
    blockHx = block.first;
    blockHy = block.second;
    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param fromHx ...
 * @param fromHy ...
 * @param toHx ...
 * @param toHy ...
 * @param angle ...
 * @param dist ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetHexInPath, FO_API_RET(void), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG_REF(ushort, toHx), FO_API_ARG_REF(ushort, toHy), FO_API_ARG(float, angle), FO_API_ARG(uint, dist))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_REF_MARSHAL(ushort, toHx) FO_API_ARG_REF_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist))
{
    pair<ushort, ushort> pre_block;
    pair<ushort, ushort> block;
    _client->HexMngr.TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, nullptr, false, nullptr, 0, &block, &pre_block, nullptr, true);
    toHx = pre_block.first;
    toHy = pre_block.second;
}
FO_API_EPILOG()
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param fromHx ...
 * @param fromHy ...
 * @param toHx ...
 * @param toHy ...
 * @param cut ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetPathToHex, FO_API_RET_ARR(uchar), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(uint, cut))
{
    if (fromHx >= _client->HexMngr.GetWidth() || fromHy >= _client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= _client->HexMngr.GetWidth() || toHy >= _client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (cut > 0 && !_client->HexMngr.CutPath(nullptr, fromHx, fromHy, to_hx, to_hy, cut)) {
        FO_API_RETURN(vector<uchar>());
    }

    vector<uchar> steps;
    if (!_client->HexMngr.FindPath(nullptr, fromHx, fromHy, to_hx, to_hy, steps, -1)) {
        FO_API_RETURN(vector<uchar>());
    }

    FO_API_RETURN(steps);
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @param toHx ...
 * @param toHy ...
 * @param cut ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetPathToCritter, FO_API_RET_ARR(uchar), FO_API_ARG_OBJ(CritterView, cr), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(CritterView, cr) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(uint, cut))
{
    if (toHx >= _client->HexMngr.GetWidth() || toHy >= _client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (cut > 0 && !_client->HexMngr.CutPath(cr, cr->GetHexX(), cr->GetHexY(), to_hx, to_hy, cut)) {
        FO_API_RETURN(vector<uchar>());
    }

    vector<uchar> steps;
    if (!_client->HexMngr.FindPath(cr, cr->GetHexX(), cr->GetHexY(), to_hx, to_hy, steps, -1)) {
        FO_API_RETURN(vector<uchar>());
    }

    FO_API_RETURN(steps);
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param fromHx ...
 * @param fromHy ...
 * @param toHx ...
 * @param toHy ...
 * @param cut ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetPathLengthToHex, FO_API_RET(uint), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(uint, cut))
{
    if (fromHx >= _client->HexMngr.GetWidth() || fromHy >= _client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= _client->HexMngr.GetWidth() || toHy >= _client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (cut > 0 && !_client->HexMngr.CutPath(nullptr, fromHx, fromHy, to_hx, to_hy, cut)) {
        FO_API_RETURN(0);
    }

    vector<uchar> steps;
    if (!_client->HexMngr.FindPath(nullptr, fromHx, fromHy, to_hx, to_hy, steps, -1)) {
        steps.clear();
    }

    FO_API_RETURN(static_cast<uint>(steps.size()));
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @param toHx ...
 * @param toHy ...
 * @param cut ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetPathLengthToCritter, FO_API_RET(uint), FO_API_ARG_OBJ(CritterView, cr), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(CritterView, cr) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(uint, cut))
{
    if (toHx >= _client->HexMngr.GetWidth() || toHy >= _client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (cut > 0 && !_client->HexMngr.CutPath(cr, cr->GetHexX(), cr->GetHexY(), to_hx, to_hy, cut)) {
        FO_API_RETURN(0);
    }

    vector<uchar> steps;
    if (!_client->HexMngr.FindPath(cr, cr->GetHexX(), cr->GetHexY(), to_hx, to_hy, steps, -1)) {
        steps.clear();
    }

    FO_API_RETURN(static_cast<uint>(steps.size()));
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param fromColor ...
 * @param toColor ...
 * @param ms ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(FlushScreen, FO_API_RET(void), FO_API_ARG(uint, fromColor), FO_API_ARG(uint, toColor), FO_API_ARG(uint, ms))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, fromColor) FO_API_ARG_MARSHAL(uint, toColor) FO_API_ARG_MARSHAL(uint, ms))
{
    _client->ScreenFade(ms, fromColor, toColor, true);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param noise ...
 * @param ms ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(QuakeScreen, FO_API_RET(void), FO_API_ARG(uint, noise), FO_API_ARG(uint, ms))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, noise) FO_API_ARG_MARSHAL(uint, ms))
{
    _client->ScreenQuake(noise, ms);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param soundName ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(PlaySound, FO_API_RET(void), FO_API_ARG(string, soundName))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, soundName))
{
    if (!_client->SndMngr.PlaySound(_client->ResMngr.GetSoundNames(), soundName)) {
        WriteLog("Sound '{}' not found.\n", soundName);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param musicName ...
 * @param repeatTime ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(PlayMusic, FO_API_RET(void), FO_API_ARG(string, musicName), FO_API_ARG(uint, repeatTime))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, musicName) FO_API_ARG_MARSHAL(uint, repeatTime))
{
    if (musicName.empty()) {
        _client->SndMngr.StopMusic();
        FO_API_RETURN_VOID();
    }

    if (!_client->SndMngr.PlayMusic(musicName, repeatTime)) {
        WriteLog("Music '{}' not found.\n", musicName);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param videoName ...
 * @param canStop ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(PlayVideo, FO_API_RET(void), FO_API_ARG(string, videoName), FO_API_ARG(bool, canStop))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, videoName) FO_API_ARG_MARSHAL(bool, canStop))
{
    // _client->SndMngr.StopMusic();
    // _client->AddVideo(videoName.c_str(), canStop, true);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCurMapPid, FO_API_RET(hash))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    if (!_client->HexMngr.IsMapLoaded()) {
        FO_API_RETURN(0);
    }
    FO_API_RETURN(_client->CurMapPid);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param msg ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(Message, FO_API_RET(void), FO_API_ARG(string, msg))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, msg))
{
    _client->AddMess(FOClient::FOMB_GAME, msg, true);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param msg ...
 * @param type ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(MessageExt, FO_API_RET(void), FO_API_ARG(string, msg), FO_API_ARG(int, type))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, msg) FO_API_ARG_MARSHAL(int, type))
{
    _client->AddMess(type, msg, true);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(MessageMsg, FO_API_RET(void), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    _client->AddMess(FOClient::FOMB_GAME, _client->CurLang.Msg[textMsg].GetStr(strNum), true);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @param type ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(MessageMsgExt, FO_API_RET(void), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum), FO_API_ARG(int, type))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum) FO_API_ARG_MARSHAL(int, type))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    _client->AddMess(type, _client->CurLang.Msg[textMsg].GetStr(strNum), true);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param hx ...
 * @param hy ...
 * @param ms ...
 * @param color ...
 * @param fade ...
 * @param ox ...
 * @param oy ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(MapMessage, FO_API_RET(void), FO_API_ARG(string, text), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, ms), FO_API_ARG(uint, color), FO_API_ARG(bool, fade), FO_API_ARG(int, ox), FO_API_ARG(int, oy))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, ms) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(bool, fade) FO_API_ARG_MARSHAL(int, ox) FO_API_ARG_MARSHAL(int, oy))
{
    FOClient::MapText map_text;
    map_text.HexX = hx;
    map_text.HexY = hy;
    map_text.Color = (color != 0u ? color : COLOR_TEXT);
    map_text.Fade = fade;
    map_text.StartTick = _client->GameTime.GameTick();
    map_text.Tick = ms;
    map_text.Text = text;
    map_text.Pos = _client->HexMngr.GetRectForText(hx, hy);
    map_text.EndPos = IRect(map_text.Pos, ox, oy);

    const auto it = std::find_if(_client->GameMapTexts.begin(), _client->GameMapTexts.end(), [&map_text](const FOClient::MapText& t) { return t.HexX == map_text.HexX && t.HexY == map_text.HexY; });
    if (it != _client->GameMapTexts.end()) {
        _client->GameMapTexts.erase(it);
    }

    _client->GameMapTexts.push_back(map_text);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetMsgStr, FO_API_RET(string), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    FO_API_RETURN(_client->CurLang.Msg[textMsg].GetStr(strNum));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @param skipCount ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetMsgStrExt, FO_API_RET(string), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum), FO_API_ARG(uint, skipCount))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum) FO_API_ARG_MARSHAL(uint, skipCount))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    FO_API_RETURN(_client->CurLang.Msg[textMsg].GetStr(strNum, skipCount));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetMsgStrNumUpper, FO_API_RET(uint), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    FO_API_RETURN(_client->CurLang.Msg[textMsg].GetStrNumUpper(strNum));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetMsgStrNumLower, FO_API_RET(uint), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    FO_API_RETURN(_client->CurLang.Msg[textMsg].GetStrNumLower(strNum));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetMsgStrCount, FO_API_RET(uint), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    FO_API_RETURN(_client->CurLang.Msg[textMsg].Count(strNum));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(IsMsgStr, FO_API_RET(bool), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }
    FO_API_RETURN(_client->CurLang.Msg[textMsg].Count(strNum) > 0);
}
FO_API_EPILOG(0)
#endif

#if FO_API_ANGELSCRIPT_ONLY
/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param replace ...
 * @param str ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(ReplaceTextStr, FO_API_RET(string), FO_API_ARG(string, text), FO_API_ARG(string, replace), FO_API_ARG(string, str))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(string, replace) FO_API_ARG_MARSHAL(string, str))
{
    const auto pos = text.find(replace, 0);
    if (pos == std::string::npos) {
        FO_API_RETURN(text);
    }
    FO_API_RETURN(string(text).replace(pos, replace.length(), str));
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_ANGELSCRIPT_ONLY
/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param replace ...
 * @param i ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(ReplaceTextInt, FO_API_RET(string), FO_API_ARG(string, text), FO_API_ARG(string, replace), FO_API_ARG(int, i))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(string, replace) FO_API_ARG_MARSHAL(int, i))
{
    const auto pos = text.find(replace, 0);
    if (pos == std::string::npos) {
        FO_API_RETURN(text);
    }
    FO_API_RETURN(string(text).replace(pos, replace.length(), _str("{}", i)));
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param lexems ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(FormatTags, FO_API_RET(string), FO_API_ARG(string, text), FO_API_ARG(string, lexems))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(string, lexems))
{
    auto text_copy = text;
    _client->FormatTags(text_copy, _client->Chosen, nullptr, lexems);
    FO_API_RETURN(text_copy);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param speed ...
 * @param canStop ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(MoveScreenToHex, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, speed), FO_API_ARG(bool, canStop))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, speed) FO_API_ARG_MARSHAL(bool, canStop))
{
    if (!_client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }
    if (hx >= _client->HexMngr.GetWidth() || hy >= _client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    if (speed == 0u) {
        _client->HexMngr.FindSetCenter(hx, hy);
    }
    else {
        _client->HexMngr.ScrollToHex(hx, hy, static_cast<float>(speed) / 1000.0f, canStop);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param ox ...
 * @param oy ...
 * @param speed ...
 * @param canStop ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(MoveScreenByOffset, FO_API_RET(void), FO_API_ARG(int, ox), FO_API_ARG(int, oy), FO_API_ARG(uint, speed), FO_API_ARG(bool, canStop))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, ox) FO_API_ARG_MARSHAL(int, oy) FO_API_ARG_MARSHAL(uint, speed) FO_API_ARG_MARSHAL(bool, canStop))
{
    if (!_client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }

    _client->HexMngr.ScrollOffset(ox, oy, static_cast<float>(speed) / 1000.0f, canStop);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @param softLock ...
 * @param unlockIfSame ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(LockScreenScroll, FO_API_RET(void), FO_API_ARG_OBJ(CritterView, cr), FO_API_ARG(bool, softLock), FO_API_ARG(bool, unlockIfSame))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(CritterView, cr) FO_API_ARG_MARSHAL(bool, softLock) FO_API_ARG_MARSHAL(bool, unlockIfSame))
{
    const auto id = (cr != nullptr ? cr->GetId() : 0);
    if (softLock) {
        if (unlockIfSame && id == _client->HexMngr.AutoScroll.SoftLockedCritter) {
            _client->HexMngr.AutoScroll.SoftLockedCritter = 0;
        }
        else {
            _client->HexMngr.AutoScroll.SoftLockedCritter = id;
        }

        _client->HexMngr.AutoScroll.CritterLastHexX = (cr ? cr->GetHexX() : 0);
        _client->HexMngr.AutoScroll.CritterLastHexY = (cr ? cr->GetHexY() : 0);
    }
    else {
        if (unlockIfSame && id == _client->HexMngr.AutoScroll.HardLockedCritter) {
            _client->HexMngr.AutoScroll.HardLockedCritter = 0;
        }
        else {
            _client->HexMngr.AutoScroll.HardLockedCritter = id;
        }
    }
}
FO_API_EPILOG()
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param zoneX ...
 * @param zoneY ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetFog, FO_API_RET(int), FO_API_ARG(ushort, zoneX), FO_API_ARG(ushort, zoneY))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, zoneX) FO_API_ARG_MARSHAL(ushort, zoneY))
{
    if (zoneX >= _client->Settings.GlobalMapWidth || zoneY >= _client->Settings.GlobalMapHeight) {
        throw ScriptException("Invalid world map pos arg");
    }
    FO_API_RETURN(_client->GmapFog.Get2Bit(zoneX, zoneY));
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param dayPart ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetDayTime, FO_API_RET(uint), FO_API_ARG(uint, dayPart))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, dayPart))
{
    if (dayPart >= 4) {
        throw ScriptException("Invalid day part arg");
    }

    if (_client->HexMngr.IsMapLoaded()) {
        FO_API_RETURN(_client->HexMngr.GetMapDayTime()[dayPart]);
    }
    FO_API_RETURN(0);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param dayPart ...
 * @param r ...
 * @param g ...
 * @param b ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetDayColor, FO_API_RET(void), FO_API_ARG(uint, dayPart), FO_API_ARG_REF(uchar, r), FO_API_ARG_REF(uchar, g), FO_API_ARG_REF(uchar, b))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, dayPart) FO_API_ARG_REF_MARSHAL(uchar, r) FO_API_ARG_REF_MARSHAL(uchar, g) FO_API_ARG_REF_MARSHAL(uchar, b))
{
    r = g = b = 0;
    if (dayPart >= 4) {
        throw ScriptException("Invalid day part arg");
    }

    if (_client->HexMngr.IsMapLoaded()) {
        auto* const col = _client->HexMngr.GetMapDayColor();
        r = col[0 + dayPart];
        g = col[4 + dayPart];
        b = col[8 + dayPart];
    }
}
FO_API_EPILOG()
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetFullSecond, FO_API_RET(uint))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_client->GameTime.GetFullSecond());
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param year ...
 * @param month ...
 * @param day ...
 * @param hour ...
 * @param minute ...
 * @param second ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(EvaluateFullSecond, FO_API_RET(uint), FO_API_ARG(ushort, year), FO_API_ARG(ushort, month), FO_API_ARG(ushort, day), FO_API_ARG(ushort, hour), FO_API_ARG(ushort, minute), FO_API_ARG(ushort, second))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, year) FO_API_ARG_MARSHAL(ushort, month) FO_API_ARG_MARSHAL(ushort, day) FO_API_ARG_MARSHAL(ushort, hour) FO_API_ARG_MARSHAL(ushort, minute) FO_API_ARG_MARSHAL(ushort, second))
{
    if (year && year < _client->Settings.StartYear) {
        throw ScriptException("Invalid year", year);
    }
    if (year && year > _client->Settings.StartYear + 100) {
        throw ScriptException("Invalid year", year);
    }
    if (month != 0u && month < 1) {
        throw ScriptException("Invalid month", month);
    }
    if (month != 0u && month > 12) {
        throw ScriptException("Invalid month", month);
    }

    auto year_ = year;
    auto month_ = month;
    auto day_ = day;

    if (year_ == 0u) {
        year_ = _client->Globals->GetYear();
    }
    if (month_ == 0u) {
        month_ = _client->Globals->GetMonth();
    }

    if (day_ != 0u) {
        const auto month_day = _client->GameTime.GameTimeMonthDay(year, month_);
        if (day_ < month_day || day_ > month_day) {
            throw ScriptException("Invalid day", day_, month_day);
        }
    }

    if (day_ == 0u) {
        day_ = _client->Globals->GetDay();
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

    FO_API_RETURN(_client->GameTime.EvaluateFullSecond(year_, month_, day_, hour, minute, second));
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param fullSecond ...
 * @param year ...
 * @param month ...
 * @param day ...
 * @param dayOfWeek ...
 * @param hour ...
 * @param minute ...
 * @param second ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetGameTime, FO_API_RET(void), FO_API_ARG(uint, fullSecond), FO_API_ARG_REF(ushort, year), FO_API_ARG_REF(ushort, month), FO_API_ARG_REF(ushort, day), FO_API_ARG_REF(ushort, dayOfWeek), FO_API_ARG_REF(ushort, hour), FO_API_ARG_REF(ushort, minute), FO_API_ARG_REF(ushort, second))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, fullSecond) FO_API_ARG_REF_MARSHAL(ushort, year) FO_API_ARG_REF_MARSHAL(ushort, month) FO_API_ARG_REF_MARSHAL(ushort, day) FO_API_ARG_REF_MARSHAL(ushort, dayOfWeek) FO_API_ARG_REF_MARSHAL(ushort, hour) FO_API_ARG_REF_MARSHAL(ushort, minute) FO_API_ARG_REF_MARSHAL(ushort, second))
{
    const auto dt = _client->GameTime.GetGameTime(fullSecond);
    year = dt.Year;
    month = dt.Month;
    dayOfWeek = dt.DayOfWeek;
    day = dt.Day;
    hour = dt.Hour;
    minute = dt.Minute;
    second = dt.Second;
}
FO_API_EPILOG()
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param dir ...
 * @param steps ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(MoveHexByDir, FO_API_RET(bool), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy), FO_API_ARG(uchar, dir), FO_API_ARG(uint, steps))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, hx) FO_API_ARG_REF_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uchar, dir) FO_API_ARG_MARSHAL(uint, steps))
{
    if (!_client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (dir >= _client->Settings.MapDirCount) {
        throw ScriptException("Invalid dir arg");
    }
    if (steps == 0u) {
        throw ScriptException("Steps arg is zero");
    }

    auto result = false;
    auto hx_ = hx;
    auto hy_ = hy;

    if (steps > 1) {
        for (uint i = 0; i < steps; i++) {
            result |= _client->GeomHelper.MoveHexByDir(hx_, hy_, dir, _client->HexMngr.GetWidth(), _client->HexMngr.GetHeight());
        }
    }
    else {
        result = _client->GeomHelper.MoveHexByDir(hx_, hy_, dir, _client->HexMngr.GetWidth(), _client->HexMngr.GetHeight());
    }

    hx = hx_;
    hy = hy_;
    FO_API_RETURN(result);
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param roof ...
 * @param layer ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetTileName, FO_API_RET(hash), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, roof), FO_API_ARG(int, layer))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(bool, roof) FO_API_ARG_MARSHAL(int, layer))
{
    if (!_client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= _client->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= _client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    const auto* simply_tile = _client->HexMngr.GetField(hx, hy).SimplyTile[roof ? 1 : 0];
    if (simply_tile && !layer) {
        FO_API_RETURN(simply_tile->NameHash);
    }

    const auto* tiles = _client->HexMngr.GetField(hx, hy).Tiles[roof ? 1 : 0];
    if (tiles == nullptr || tiles->empty()) {
        FO_API_RETURN(0);
    }

    for (const auto& tile : *tiles) {
        if (tile.Layer == layer) {
            FO_API_RETURN(tile.Anim->NameHash);
        }
    }
    FO_API_RETURN(0);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param fnames ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(Preload3dFiles, FO_API_RET(void), FO_API_ARG_ARR(string, fnames))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_ARR_MARSHAL(string, fnames))
{
    for (size_t i = 0; i < fnames.size(); i++) {
        _client->Preload3dFiles.push_back(fnames[i]);
    }
}
FO_API_EPILOG()
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(WaitPing, FO_API_RET(void))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    _client->WaitPing();
}
FO_API_EPILOG()
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param fontIndex ...
 * @param fontFname ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(LoadFont, FO_API_RET(void), FO_API_ARG(int, fontIndex), FO_API_ARG(string, fontFname))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, fontIndex) FO_API_ARG_MARSHAL(string, fontFname))
{
    _client->SprMngr.PushAtlasType(AtlasType::Static);

    bool result;
    if (fontFname.length() > 0 && fontFname[0] == '*') {
        result = _client->SprMngr.LoadFontFO(fontIndex, fontFname.c_str() + 1, false, false);
    }
    else {
        result = _client->SprMngr.LoadFontBmf(fontIndex, fontFname.c_str());
    }

    if (result && !_client->SprMngr.IsAccumulateAtlasActive()) {
        _client->SprMngr.BuildFonts();
    }
    _client->SprMngr.PopAtlasType();

    if (!result) {
        throw ScriptException("Can't load font", fontIndex, fontFname);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param font ...
 * @param color ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SetDefaultFont, FO_API_RET(void), FO_API_ARG(int, font), FO_API_ARG(uint, color))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, font) FO_API_ARG_MARSHAL(uint, color))
{
    _client->SprMngr.SetDefaultFont(font, color);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param effectType ...
 * @param effectSubtype ...
 * @param effectName ...
 * @param effectDefines ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SetEffect, FO_API_RET(void), FO_API_ARG(int, effectType), FO_API_ARG(int, effectSubtype), FO_API_ARG(string, effectName), FO_API_ARG(string, effectDefines))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, effectType) FO_API_ARG_MARSHAL(int, effectSubtype) FO_API_ARG_MARSHAL(string, effectName) FO_API_ARG_MARSHAL(string, effectDefines))
{
    RenderEffect* effect = nullptr;
    if (!effectName.empty()) {
        effect = _client->EffectMngr.LoadEffect(effectName, effectDefines, "");
        if (!effect) {
            throw ScriptException("Effect not found or have some errors, see log file");
        }
    }

    if ((effectType & EffectType_GenericSprite) && effectSubtype != 0) {
        auto* item = _client->GetItem(static_cast<uint>(effectSubtype));
        if (item) {
            item->DrawEffect = (effect ? effect : _client->EffectMngr.Effects.Generic);
        }
    }
    if ((effectType & EffectType_CritterSprite) && effectSubtype != 0) {
        auto* cr = _client->GetCritter(static_cast<uint>(effectSubtype));
        if (cr) {
            cr->DrawEffect = (effect ? effect : _client->EffectMngr.Effects.Critter);
        }
    }

    if ((effectType & EffectType_GenericSprite) && effectSubtype == 0) {
        _client->EffectMngr.Effects.Generic = (effect ? effect : _client->EffectMngr.Effects.GenericDefault);
    }
    if ((effectType & EffectType_CritterSprite) && effectSubtype == 0) {
        _client->EffectMngr.Effects.Critter = (effect ? effect : _client->EffectMngr.Effects.CritterDefault);
    }
    if (effectType & EffectType_TileSprite) {
        _client->EffectMngr.Effects.Tile = (effect ? effect : _client->EffectMngr.Effects.TileDefault);
    }
    if (effectType & EffectType_RoofSprite) {
        _client->EffectMngr.Effects.Roof = (effect ? effect : _client->EffectMngr.Effects.RoofDefault);
    }
    if (effectType & EffectType_RainSprite) {
        _client->EffectMngr.Effects.Rain = (effect ? effect : _client->EffectMngr.Effects.RainDefault);
    }

    if (effectType & EffectType_SkinnedMesh) {
        _client->EffectMngr.Effects.Skinned3d = (effect ? effect : _client->EffectMngr.Effects.Skinned3dDefault);
    }

    if (effectType & EffectType_Interface) {
        _client->EffectMngr.Effects.Iface = (effect ? effect : _client->EffectMngr.Effects.IfaceDefault);
    }
    if (effectType & EffectType_Contour) {
        _client->EffectMngr.Effects.Contour = (effect ? effect : _client->EffectMngr.Effects.ContourDefault);
    }

    if ((effectType & EffectType_Font) && effectSubtype == -1) {
        _client->EffectMngr.Effects.Font = (effect ? effect : _client->EffectMngr.Effects.ContourDefault);
    }
    if ((effectType & EffectType_Font) && effectSubtype >= 0) {
        _client->SprMngr.SetFontEffect(effectSubtype, effect);
    }

    if (effectType & EffectType_Primitive) {
        _client->EffectMngr.Effects.Primitive = (effect ? effect : _client->EffectMngr.Effects.PrimitiveDefault);
    }
    if (effectType & EffectType_Light) {
        _client->EffectMngr.Effects.Light = (effect ? effect : _client->EffectMngr.Effects.LightDefault);
    }
    if (effectType & EffectType_Fog) {
        _client->EffectMngr.Effects.Fog = (effect ? effect : _client->EffectMngr.Effects.FogDefault);
    }

    if (effectType & EffectType_FlushRenderTarget) {
        _client->EffectMngr.Effects.FlushRenderTarget = (effect ? effect : _client->EffectMngr.Effects.FlushRenderTargetDefault);
    }
    if (effectType & EffectType_FlushPrimitive) {
        _client->EffectMngr.Effects.FlushPrimitive = (effect ? effect : _client->EffectMngr.Effects.FlushPrimitiveDefault);
    }
    if (effectType & EffectType_FlushMap) {
        _client->EffectMngr.Effects.FlushMap = (effect ? effect : _client->EffectMngr.Effects.FlushMapDefault);
    }
    if (effectType & EffectType_FlushLight) {
        _client->EffectMngr.Effects.FlushLight = (effect ? effect : _client->EffectMngr.Effects.FlushLightDefault);
    }
    if (effectType & EffectType_FlushFog) {
        _client->EffectMngr.Effects.FlushFog = (effect ? effect : _client->EffectMngr.Effects.FlushFogDefault);
    }

    if (effectType & EffectType_Offscreen) {
        if (effectSubtype < 0) {
            throw ScriptException("Negative effect subtype");
        }

        _client->OffscreenEffects.resize(effectSubtype + 1);
        _client->OffscreenEffects[effectSubtype] = effect;
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param onlyTiles ...
 * @param onlyRoof ...
 * @param onlyLight ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(RedrawMap, FO_API_RET(void), FO_API_ARG(bool, onlyTiles), FO_API_ARG(bool, onlyRoof), FO_API_ARG(bool, onlyLight))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, onlyTiles) FO_API_ARG_MARSHAL(bool, onlyRoof) FO_API_ARG_MARSHAL(bool, onlyLight))
{
    if (_client->HexMngr.IsMapLoaded()) {
        if (onlyTiles) {
            _client->HexMngr.RebuildTiles();
        }
        else if (onlyRoof) {
            _client->HexMngr.RebuildRoof();
        }
        else if (onlyLight) {
            _client->HexMngr.RebuildLight();
        }
        else {
            _client->HexMngr.RefreshMap();
        }
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param x ...
 * @param y ...
 * @param button ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SimulateMouseClick, FO_API_RET(void), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, button))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, button))
{
    /*App->Input.PushEvent({InputEvent::MouseDown({(MouseButton)button})});

    IntVec prev_events = _client->Settings.MainWindowMouseEvents;
    _client->Settings.MainWindowMouseEvents.clear();
    int prev_x = _client->Settings.MouseX;
    int prev_y = _client->Settings.MouseY;
    int last_prev_x = _client->Settings.LastMouseX;
    int last_prev_y = _client->Settings.LastMouseY;
    _client->Settings.MouseX = _client->Settings.LastMouseX = x;
    _client->Settings.MouseY = _client->Settings.LastMouseY = y;
    _client->Settings.MainWindowMouseEvents.push_back(SDL_MOUSEBUTTONDOWN);
    _client->Settings.MainWindowMouseEvents.push_back(button);
    _client->Settings.MainWindowMouseEvents.push_back(SDL_MOUSEBUTTONUP);
    _client->Settings.MainWindowMouseEvents.push_back(button);
    _client->ParseMouse();
    _client->Settings.MainWindowMouseEvents = prev_events;
    _client->Settings.MouseX = prev_x;
    _client->Settings.MouseY = prev_y;
    _client->Settings.LastMouseX = last_prev_x;
    _client->Settings.LastMouseY = last_prev_y;*/
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param key1 ...
 * @param key2 ...
 * @param key1Text ...
 * @param key2Text ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SimulateKeyboardPress, FO_API_RET(void), FO_API_ARG(uchar, key1), FO_API_ARG(uchar, key2), FO_API_ARG(string, key1Text), FO_API_ARG(string, key2Text))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, key1) FO_API_ARG_MARSHAL(uchar, key2) FO_API_ARG_MARSHAL(string, key1Text) FO_API_ARG_MARSHAL(string, key2Text))
{
    if (key1 == 0u && key2 == 0u) {
        FO_API_RETURN_VOID();
    }

    if (key1 != 0u) {
        _client->ProcessInputEvent(InputEvent {InputEvent::KeyDownEvent {static_cast<KeyCode>(key1), key1Text}});
    }

    if (key2 != 0u) {
        _client->ProcessInputEvent(InputEvent {InputEvent::KeyDownEvent {static_cast<KeyCode>(key2), key2Text}});
        _client->ProcessInputEvent(InputEvent {InputEvent::KeyUpEvent {static_cast<KeyCode>(key2)}});
    }

    if (key1 != 0u) {
        _client->ProcessInputEvent(InputEvent {InputEvent::KeyUpEvent {static_cast<KeyCode>(key1)}});
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param fallAnimName ...
 * @param dropAnimName ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SetRainAnim, FO_API_RET(void), FO_API_ARG(string, fallAnimName), FO_API_ARG(string, dropAnimName))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, fallAnimName) FO_API_ARG_MARSHAL(string, dropAnimName))
{
    _client->HexMngr.SetRainAnimation(!fallAnimName.empty() ? fallAnimName.c_str() : nullptr, !dropAnimName.empty() ? dropAnimName.c_str() : nullptr);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param targetZoom ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(ChangeZoom, FO_API_RET(void), FO_API_ARG(float, targetZoom))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(float, targetZoom))
{
    if (targetZoom == _client->Settings.SpritesZoom) {
        FO_API_RETURN_VOID();
    }

    const auto init_zoom = _client->Settings.SpritesZoom;
    if (targetZoom == 1.0f) {
        _client->HexMngr.ChangeZoom(0);
    }
    else if (targetZoom > _client->Settings.SpritesZoom) {
        while (targetZoom > _client->Settings.SpritesZoom) {
            const auto old_zoom = _client->Settings.SpritesZoom;
            _client->HexMngr.ChangeZoom(1);
            if (_client->Settings.SpritesZoom == old_zoom) {
                break;
            }
        }
    }
    else if (targetZoom < _client->Settings.SpritesZoom) {
        while (targetZoom < _client->Settings.SpritesZoom) {
            const auto old_zoom = _client->Settings.SpritesZoom;
            _client->HexMngr.ChangeZoom(-1);
            if (_client->Settings.SpritesZoom == old_zoom) {
                break;
            }
        }
    }

    if (init_zoom != _client->Settings.SpritesZoom) {
        _client->RebuildLookBorders = true;
    }
}
FO_API_EPILOG()
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param year ...
 * @param month ...
 * @param day ...
 * @param dayOfWeek ...
 * @param hour ...
 * @param minute ...
 * @param second ...
 * @param milliseconds ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetTime, FO_API_RET(void), FO_API_ARG_REF(ushort, year), FO_API_ARG_REF(ushort, month), FO_API_ARG_REF(ushort, day), FO_API_ARG_REF(ushort, dayOfWeek), FO_API_ARG_REF(ushort, hour), FO_API_ARG_REF(ushort, minute), FO_API_ARG_REF(ushort, second), FO_API_ARG_REF(ushort, milliseconds))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, year) FO_API_ARG_REF_MARSHAL(ushort, month) FO_API_ARG_REF_MARSHAL(ushort, day) FO_API_ARG_REF_MARSHAL(ushort, dayOfWeek) FO_API_ARG_REF_MARSHAL(ushort, hour) FO_API_ARG_REF_MARSHAL(ushort, minute) FO_API_ARG_REF_MARSHAL(ushort, second) FO_API_ARG_REF_MARSHAL(ushort, milliseconds))
{
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
FO_API_EPILOG()
#endif
#endif

#if FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param datName ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(AddDataSource, FO_API_RET(void), FO_API_ARG(string, datName))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, datName))
{
    _client->FileMngr.AddDataSource(datName, true);
}
FO_API_EPILOG()
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param sprName ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(LoadSprite, FO_API_RET(uint), FO_API_ARG(string, sprName))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, sprName))
{
    FO_API_RETURN(_client->AnimLoad(sprName.c_str(), AtlasType::Static));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param nameHash ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(LoadSpriteByHash, FO_API_RET(uint), FO_API_ARG(hash, nameHash))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, nameHash))
{
    FO_API_RETURN(_client->AnimLoad(nameHash, AtlasType::Static));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @param frameIndex ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetSpriteWidth, FO_API_RET(int), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex))
{
    auto* anim = _client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN(0);
    }

    auto* const si = _client->SprMngr.GetSpriteInfo(frameIndex < 0 ? anim->GetCurSprId(_client->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (!si) {
        FO_API_RETURN(0);
    }

    FO_API_RETURN(si->Width);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @param frameIndex ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetSpriteHeight, FO_API_RET(int), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex))
{
    auto* anim = _client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN(0);
    }
    auto* const si = _client->SprMngr.GetSpriteInfo(frameIndex < 0 ? anim->GetCurSprId(_client->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (!si) {
        FO_API_RETURN(0);
    }

    FO_API_RETURN(si->Height);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetSpriteCount, FO_API_RET(uint), FO_API_ARG(uint, sprId))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId))
{
    auto* const anim = _client->AnimGetFrames(sprId);
    FO_API_RETURN(anim ? anim->CntFrm : 0);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetSpriteTicks, FO_API_RET(uint), FO_API_ARG(uint, sprId))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId))
{
    auto* const anim = _client->AnimGetFrames(sprId);
    FO_API_RETURN(anim ? anim->Ticks : 0);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @param frameIndex ...
 * @param x ...
 * @param y ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetSpritePixelColor, FO_API_RET(uint), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex), FO_API_ARG(int, x), FO_API_ARG(int, y))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y))
{
    if (sprId == 0u) {
        FO_API_RETURN(0);
    }

    auto* anim = _client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN(0);
    }

    const auto spr_id_ = (frameIndex < 0 ? anim->GetCurSprId(_client->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    FO_API_RETURN(_client->SprMngr.GetPixColor(spr_id_, x, y, false));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param w ...
 * @param h ...
 * @param font ...
 * @param flags ...
 * @param tw ...
 * @param th ...
 * @param lines ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetTextInfo, FO_API_RET(void), FO_API_ARG(string, text), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(int, font), FO_API_ARG(int, flags), FO_API_ARG_REF(int, tw), FO_API_ARG_REF(int, th), FO_API_ARG_REF(int, lines))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h) FO_API_ARG_MARSHAL(int, font) FO_API_ARG_MARSHAL(int, flags) FO_API_ARG_REF_MARSHAL(int, tw) FO_API_ARG_REF_MARSHAL(int, th) FO_API_ARG_REF_MARSHAL(int, lines))
{
    if (!_client->SprMngr.GetTextInfo(w, h, text, font, flags, tw, th, lines)) {
        throw ScriptException("Can't evaluate text information", font);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @param frameIndex ...
 * @param x ...
 * @param y ...
 * @param color ...
 * @param offs ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(DrawSprite, FO_API_RET(void), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(uint, color), FO_API_ARG(bool, offs))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(bool, offs))
{
    if (!_client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0u) {
        FO_API_RETURN_VOID();
    }

    auto* anim = _client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN_VOID();
    }

    auto xx = x;
    auto yy = y;

    const auto spr_id_ = (frameIndex < 0 ? anim->GetCurSprId(_client->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (offs) {
        auto* const si = _client->SprMngr.GetSpriteInfo(spr_id_);
        if (!si) {
            FO_API_RETURN_VOID();
        }

        xx += -si->Width / 2 + si->OffsX;
        yy += -si->Height + si->OffsY;
    }

    _client->SprMngr.DrawSprite(spr_id_, xx, yy, COLOR_SCRIPT_SPRITE(color));
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @param frameIndex ...
 * @param x ...
 * @param y ...
 * @param w ...
 * @param h ...
 * @param zoom ...
 * @param color ...
 * @param offs ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(DrawSpriteSized, FO_API_RET(void), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(bool, zoom), FO_API_ARG(uint, color), FO_API_ARG(bool, offs))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h) FO_API_ARG_MARSHAL(bool, zoom) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(bool, offs))
{
    if (!_client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0u) {
        FO_API_RETURN_VOID();
    }

    auto* anim = _client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN_VOID();
    }

    auto xx = x;
    auto yy = y;

    const auto spr_id_ = (frameIndex < 0 ? anim->GetCurSprId(_client->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (offs) {
        auto* const si = _client->SprMngr.GetSpriteInfo(spr_id_);
        if (!si) {
            FO_API_RETURN_VOID();
        }

        xx += si->OffsX;
        yy += si->OffsY;
    }

    _client->SprMngr.DrawSpriteSizeExt(spr_id_, xx, yy, w, h, zoom, true, true, COLOR_SCRIPT_SPRITE(color));
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @param frameIndex ...
 * @param x ...
 * @param y ...
 * @param w ...
 * @param h ...
 * @param sprWidth ...
 * @param sprHeight ...
 * @param color ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(DrawSpritePattern, FO_API_RET(void), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(int, sprWidth), FO_API_ARG(int, sprHeight), FO_API_ARG(uint, color))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h) FO_API_ARG_MARSHAL(int, sprWidth) FO_API_ARG_MARSHAL(int, sprHeight) FO_API_ARG_MARSHAL(uint, color))
{
    if (!_client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (sprId == 0u) {
        FO_API_RETURN_VOID();
    }

    auto* anim = _client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN_VOID();
    }

    _client->SprMngr.DrawSpritePattern(frameIndex < 0 ? anim->GetCurSprId(_client->GameTime.GameTick()) : anim->GetSprId(frameIndex), x, y, w, h, sprWidth, sprHeight, COLOR_SCRIPT_SPRITE(color));
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param x ...
 * @param y ...
 * @param w ...
 * @param h ...
 * @param color ...
 * @param font ...
 * @param flags ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(DrawText, FO_API_RET(void), FO_API_ARG(string, text), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(uint, color), FO_API_ARG(int, font), FO_API_ARG(int, flags))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(int, font) FO_API_ARG_MARSHAL(int, flags))
{
    if (!_client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (text.length() == 0) {
        FO_API_RETURN_VOID();
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
    _client->SprMngr.DrawStr(r, text.c_str(), flags, COLOR_SCRIPT_TEXT(color), font);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param primitiveType ...
 * @param data ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(DrawPrimitive, FO_API_RET(void), FO_API_ARG(int, primitiveType), FO_API_ARG_ARR(int, data))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, primitiveType) FO_API_ARG_ARR_MARSHAL(int, data))
{
    if (!_client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (data.empty()) {
        FO_API_RETURN_VOID();
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
    case 5:
        prim = RenderPrimitiveType::TriangleFan;
        break;
    default:
        FO_API_RETURN_VOID();
    }

    PrimitivePoints points;
    const auto size = data.size() / 3;
    points.resize(size);

    for (size_t i = 0; i < size; i++) {
        auto& pp = points[i];
        pp.PointX = data[i * 3];
        pp.PointY = data[i * 3 + 1];
        pp.PointColor = data[i * 3 + 2];
        pp.PointOffsX = nullptr;
        pp.PointOffsY = nullptr;
    }

    _client->SprMngr.DrawPoints(points, prim, nullptr, nullptr, nullptr);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param mapSpr ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(DrawMapSprite, FO_API_RET(void), FO_API_ARG_OBJ(MapSprite, mapSpr))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(MapSprite, mapSpr))
{
    if (mapSpr == nullptr) {
        throw ScriptException("Map sprite arg is null");
    }

    if (!_client->HexMngr.IsMapLoaded()) {
        FO_API_RETURN_VOID();
    }
    if (mapSpr->HexX >= _client->HexMngr.GetWidth() || mapSpr->HexY >= _client->HexMngr.GetHeight()) {
        FO_API_RETURN_VOID();
    }
    if (!_client->HexMngr.IsHexToDraw(mapSpr->HexX, mapSpr->HexY)) {
        FO_API_RETURN_VOID();
    }

    auto* anim = _client->AnimGetFrames(mapSpr->SprId);
    if (!anim || mapSpr->FrameIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN_VOID();
    }

    auto color = mapSpr->Color;
    auto is_flat = mapSpr->IsFlat;
    auto no_light = mapSpr->NoLight;
    auto draw_order = mapSpr->DrawOrder;
    auto draw_order_hy_offset = mapSpr->DrawOrderHyOffset;
    auto corner = mapSpr->Corner;
    auto disable_egg = mapSpr->DisableEgg;
    auto contour_color = mapSpr->ContourColor;

    if (mapSpr->ProtoId != 0u) {
        const auto* const proto_item = _client->ProtoMngr.GetProtoItem(mapSpr->ProtoId);
        if (!proto_item) {
            FO_API_RETURN_VOID();
        }

        color = (proto_item->GetIsColorize() ? proto_item->GetLightColor() : 0);
        is_flat = proto_item->GetIsFlat();
        const auto is_item = !proto_item->IsAnyScenery();
        no_light = (is_flat && !is_item);
        draw_order = (is_flat ? (is_item ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY) : (is_item ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY));
        draw_order_hy_offset = proto_item->GetDrawOrderOffsetHexY();
        corner = proto_item->GetCorner();
        disable_egg = proto_item->GetDisableEgg();
        contour_color = (proto_item->GetIsBadItem() ? COLOR_RGB(255, 0, 0) : 0);
    }

    auto& f = _client->HexMngr.GetField(mapSpr->HexX, mapSpr->HexY);
    auto& tree = _client->HexMngr.GetDrawTree();
    auto& spr = tree.InsertSprite(draw_order, mapSpr->HexX, mapSpr->HexY + draw_order_hy_offset, 0, (_client->Settings.MapHexWidth / 2) + mapSpr->OffsX, (_client->Settings.MapHexHeight / 2) + mapSpr->OffsY, &f.ScrX, &f.ScrY, mapSpr->FrameIndex < 0 ? anim->GetCurSprId(_client->GameTime.GameTick()) : anim->GetSprId(mapSpr->FrameIndex), nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsX : nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsY : nullptr, mapSpr->IsTweakAlpha ? &mapSpr->TweakAlpha : nullptr, nullptr, &mapSpr->Valid);

    spr.MapSpr = mapSpr;
    mapSpr->AddRef();

    if (!no_light) {
        spr.SetLight(corner, _client->HexMngr.GetLightHex(0, 0), _client->HexMngr.GetWidth(), _client->HexMngr.GetHeight());
    }

    if (!is_flat && !disable_egg) {
        int egg_type;
        switch (corner) {
        case CORNER_SOUTH:
            egg_type = EGG_X_OR_Y;
            break;
        case CORNER_NORTH:
            egg_type = EGG_X_AND_Y;
            break;
        case CORNER_EAST_WEST:
        case CORNER_WEST:
            egg_type = EGG_Y;
            break;
        default:
            egg_type = EGG_X;
            break;
        }
        spr.SetEgg(egg_type);
    }

    if (color != 0u) {
        spr.SetColor(color & 0xFFFFFF);
        spr.SetFixedAlpha(color >> 24);
    }

    if (contour_color != 0u) {
        spr.SetContour(CONTOUR_CUSTOM, contour_color);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param modelName ...
 * @param anim1 ...
 * @param anim2 ...
 * @param dir ...
 * @param l ...
 * @param t ...
 * @param r ...
 * @param b ...
 * @param scratch ...
 * @param center ...
 * @param color ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(DrawCritter2d, FO_API_RET(void), FO_API_ARG(hash, modelName), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2), FO_API_ARG(uchar, dir), FO_API_ARG(int, l), FO_API_ARG(int, t), FO_API_ARG(int, r), FO_API_ARG(int, b), FO_API_ARG(bool, scratch), FO_API_ARG(bool, center), FO_API_ARG(uint, color))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, modelName) FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2) FO_API_ARG_MARSHAL(uchar, dir) FO_API_ARG_MARSHAL(int, l) FO_API_ARG_MARSHAL(int, t) FO_API_ARG_MARSHAL(int, r) FO_API_ARG_MARSHAL(int, b) FO_API_ARG_MARSHAL(bool, scratch) FO_API_ARG_MARSHAL(bool, center) FO_API_ARG_MARSHAL(uint, color))
{
    auto* anim = _client->ResMngr.GetCritterAnim(modelName, anim1, anim2, dir);
    if (anim) {
        _client->SprMngr.DrawSpriteSize(anim->Ind[0], l, t, r - l, b - t, scratch, center, COLOR_SCRIPT_SPRITE(color));
    }
}
FO_API_EPILOG()
#endif

#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
static vector<ModelInstance*> DrawCritterModel;
static vector<uint> DrawCritterModelCrType;
static vector<bool> DrawCritterModelFailedToLoad;
static int DrawCritterModelLayers[LAYERS3D_COUNT];
#endif
/*******************************************************************************
 * ...
 *
 * @param instance ...
 * @param modelName ...
 * @param anim1 ...
 * @param anim2 ...
 * @param layers ...
 * @param position ...
 * @param color ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(DrawCritter3d, FO_API_RET(void), FO_API_ARG(uint, instance), FO_API_ARG(hash, modelName), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2), FO_API_ARG_ARR(int, layers), FO_API_ARG_ARR(float, position), FO_API_ARG(uint, color))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, instance) FO_API_ARG_MARSHAL(hash, modelName) FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2) FO_API_ARG_ARR_MARSHAL(int, layers) FO_API_ARG_ARR_MARSHAL(float, position) FO_API_ARG_MARSHAL(uint, color))
{
    // x y
    // rx ry rz
    // sx sy sz
    // speed
    // scissor l t r b
    if (instance >= DrawCritterModel.size()) {
        DrawCritterModel.resize(instance + 1);
        DrawCritterModelCrType.resize(instance + 1);
        DrawCritterModelFailedToLoad.resize(instance + 1);
    }

    if (DrawCritterModelFailedToLoad[instance] && DrawCritterModelCrType[instance] == modelName) {
        FO_API_RETURN_VOID();
    }

    auto& model = DrawCritterModel[instance];
    if (model == nullptr || DrawCritterModelCrType[instance] != modelName) {
        if (model != nullptr) {
            _client->SprMngr.FreeModel(model);
        }

        _client->SprMngr.PushAtlasType(AtlasType::Dynamic);
        model = _client->SprMngr.LoadModel(_str().parseHash(modelName), false);
        _client->SprMngr.PopAtlasType();
        DrawCritterModelCrType[instance] = modelName;
        DrawCritterModelFailedToLoad[instance] = false;

        if (model == nullptr) {
            DrawCritterModelFailedToLoad[instance] = true;
            FO_API_RETURN_VOID();
        }

        model->EnableShadow(false);
        model->SetTimer(false);
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
        _client->SprMngr.PushScissor(static_cast<int>(stl), static_cast<int>(stt), static_cast<int>(str), static_cast<int>(stb));
    }

    std::memset(DrawCritterModelLayers, 0, sizeof(DrawCritterModelLayers));
    for (uint i = 0, j = static_cast<uint>(layers.size()); i < j && i < LAYERS3D_COUNT; i++) {
        DrawCritterModelLayers[i] = layers[i];
    }

    model->SetDirAngle(0);
    model->SetRotation(rx * PI_FLOAT / 180.0f, ry * PI_FLOAT / 180.0f, rz * PI_FLOAT / 180.0f);
    model->SetScale(sx, sy, sz);
    model->SetSpeed(speed);
    model->SetAnimation(anim1, anim2, DrawCritterModelLayers, ANIMATION_PERIOD(static_cast<int>(period * 100.0f)) | ANIMATION_NO_SMOOTH);

    _client->SprMngr.Draw3d(static_cast<int>(x), static_cast<int>(y), model, COLOR_SCRIPT_SPRITE(color));

    if (count > 13) {
        _client->SprMngr.PopScissor();
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param x ...
 * @param y ...
 * @param w ...
 * @param h ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(PushDrawScissor, FO_API_RET(void), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h))
{
    _client->SprMngr.PushScissor(x, y, x + w, y + h);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(PopDrawScissor, FO_API_RET(void))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    _client->SprMngr.PopScissor();
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param forceClear ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(ActivateOffscreenSurface, FO_API_RET(void), FO_API_ARG(bool, forceClear))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, forceClear))
{
    if (!_client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    if (_client->OffscreenSurfaces.empty()) {
        auto* const rt = _client->SprMngr.CreateRenderTarget(false, true, 0, 0, false);
        if (!rt) {
            throw ScriptException("Can't create offscreen surface");
        }

        _client->OffscreenSurfaces.push_back(rt);
    }

    auto* const rt = _client->OffscreenSurfaces.back();
    _client->OffscreenSurfaces.pop_back();
    _client->ActiveOffscreenSurfaces.push_back(rt);

    _client->SprMngr.PushRenderTarget(rt);

    const auto it = std::find(_client->DirtyOffscreenSurfaces.begin(), _client->DirtyOffscreenSurfaces.end(), rt);
    if (it != _client->DirtyOffscreenSurfaces.end() || forceClear) {
        if (it != _client->DirtyOffscreenSurfaces.end()) {
            _client->DirtyOffscreenSurfaces.erase(it);
        }

        _client->SprMngr.ClearCurrentRenderTarget(0);
    }

    if (std::find(_client->PreDirtyOffscreenSurfaces.begin(), _client->PreDirtyOffscreenSurfaces.end(), rt) == _client->PreDirtyOffscreenSurfaces.end()) {
        _client->PreDirtyOffscreenSurfaces.push_back(rt);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param effectSubtype ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(PresentOffscreenSurface, FO_API_RET(void), FO_API_ARG(int, effectSubtype))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, effectSubtype))
{
    if (!_client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (_client->ActiveOffscreenSurfaces.empty()) {
        throw ScriptException("No active offscreen surfaces");
    }

    auto* const rt = _client->ActiveOffscreenSurfaces.back();
    _client->ActiveOffscreenSurfaces.pop_back();
    _client->OffscreenSurfaces.push_back(rt);

    _client->SprMngr.PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= static_cast<int>(_client->OffscreenEffects.size()) || !_client->OffscreenEffects[effectSubtype]) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->DrawEffect = _client->OffscreenEffects[effectSubtype];

    _client->SprMngr.DrawRenderTarget(rt, true, nullptr, nullptr);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param effectSubtype ...
 * @param x ...
 * @param y ...
 * @param w ...
 * @param h ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(PresentOffscreenSurfaceExt, FO_API_RET(void), FO_API_ARG(int, effectSubtype), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, effectSubtype) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h))
{
    if (!_client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (_client->ActiveOffscreenSurfaces.empty()) {
        throw ScriptException("No active offscreen surfaces");
    }

    auto* const rt = _client->ActiveOffscreenSurfaces.back();
    _client->ActiveOffscreenSurfaces.pop_back();
    _client->OffscreenSurfaces.push_back(rt);

    _client->SprMngr.PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= static_cast<int>(_client->OffscreenEffects.size()) || !_client->OffscreenEffects[effectSubtype]) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->DrawEffect = _client->OffscreenEffects[effectSubtype];

    IRect from(std::clamp(x, 0, _client->Settings.ScreenWidth), std::clamp(y, 0, _client->Settings.ScreenHeight), std::clamp(x + w, 0, _client->Settings.ScreenWidth), std::clamp(y + h, 0, _client->Settings.ScreenHeight));
    auto to = from;
    _client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param effectSubtype ...
 * @param fromX ...
 * @param fromY ...
 * @param fromW ...
 * @param fromH ...
 * @param toX ...
 * @param toY ...
 * @param toW ...
 * @param toH ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(PresentOffscreenSurfaceExt2, FO_API_RET(void), FO_API_ARG(int, effectSubtype), FO_API_ARG(int, fromX), FO_API_ARG(int, fromY), FO_API_ARG(int, fromW), FO_API_ARG(int, fromH), FO_API_ARG(int, toX), FO_API_ARG(int, toY), FO_API_ARG(int, toW), FO_API_ARG(int, toH))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, effectSubtype) FO_API_ARG_MARSHAL(int, fromX) FO_API_ARG_MARSHAL(int, fromY) FO_API_ARG_MARSHAL(int, fromW) FO_API_ARG_MARSHAL(int, fromH) FO_API_ARG_MARSHAL(int, toX) FO_API_ARG_MARSHAL(int, toY) FO_API_ARG_MARSHAL(int, toW) FO_API_ARG_MARSHAL(int, toH))
{
    if (!_client->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }
    if (_client->ActiveOffscreenSurfaces.empty()) {
        throw ScriptException("No active offscreen surfaces");
    }

    auto* const rt = _client->ActiveOffscreenSurfaces.back();
    _client->ActiveOffscreenSurfaces.pop_back();
    _client->OffscreenSurfaces.push_back(rt);

    _client->SprMngr.PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= static_cast<int>(_client->OffscreenEffects.size()) || !_client->OffscreenEffects[effectSubtype]) {
        throw ScriptException("Invalid effect subtype");
    }

    rt->DrawEffect = _client->OffscreenEffects[effectSubtype];

    IRect from(std::clamp(fromX, 0, _client->Settings.ScreenWidth), std::clamp(fromY, 0, _client->Settings.ScreenHeight), std::clamp(fromX + fromW, 0, _client->Settings.ScreenWidth), std::clamp(fromY + fromH, 0, _client->Settings.ScreenHeight));
    IRect to(std::clamp(toX, 0, _client->Settings.ScreenWidth), std::clamp(toY, 0, _client->Settings.ScreenHeight), std::clamp(toX + toW, 0, _client->Settings.ScreenWidth), std::clamp(toY + toH, 0, _client->Settings.ScreenHeight));
    _client->SprMngr.DrawRenderTarget(rt, true, &from, &to);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param screen ...
 * @param data ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(ShowScreen, FO_API_RET(void), FO_API_ARG(int, screen), FO_API_ARG_DICT(string, int, data))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, screen) FO_API_ARG_DICT_MARSHAL(string, int, data))
{
    if (screen >= SCREEN_LOGIN && screen <= SCREEN_WAIT) {
        _client->ShowMainScreen(screen, data);
    }
    else if (screen != SCREEN_NONE) {
        _client->ShowScreen(screen, data);
    }
    else {
        _client->HideScreen(screen);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param screen ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(HideScreen, FO_API_RET(void), FO_API_ARG(int, screen))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, screen))
{
    _client->HideScreen(screen);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param x ...
 * @param y ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetHexMonitorPos, FO_API_RET(bool), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG_REF(int, x), FO_API_ARG_REF(int, y))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_REF_MARSHAL(int, x) FO_API_ARG_REF_MARSHAL(int, y))
{
    x = y = 0;
    if (_client->HexMngr.IsMapLoaded() && hx < _client->HexMngr.GetWidth() && hy < _client->HexMngr.GetHeight()) {
        _client->HexMngr.GetHexCurrentPosition(hx, hy, x, y);
        x += _client->Settings.ScrOx + (_client->Settings.MapHexWidth / 2);
        y += _client->Settings.ScrOy + (_client->Settings.MapHexHeight / 2);
        x = static_cast<int>(static_cast<float>(x) / _client->Settings.SpritesZoom);
        y = static_cast<int>(static_cast<float>(y) / _client->Settings.SpritesZoom);
        FO_API_RETURN(true);
    }
    FO_API_RETURN(false);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param x ...
 * @param y ...
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetHexByMonitorPos, FO_API_RET(bool), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_REF_MARSHAL(ushort, hx) FO_API_ARG_REF_MARSHAL(ushort, hy))
{
    const auto old_x = _client->Settings.MouseX;
    const auto old_y = _client->Settings.MouseY;
    _client->Settings.MouseX = x;
    _client->Settings.MouseY = y;
    ushort hx_ = 0;
    ushort hy_ = 0;
    const auto result = _client->HexMngr.GetHexPixel(x, y, hx_, hy_);
    _client->Settings.MouseX = old_x;
    _client->Settings.MouseY = old_y;
    if (result) {
        hx = hx_;
        hy = hy_;
        FO_API_RETURN(true);
    }
    FO_API_RETURN(false);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param x ...
 * @param y ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetItemByMonitorPos, FO_API_RET_OBJ(ItemView), FO_API_ARG(int, x), FO_API_ARG(int, y))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y))
{
    bool item_egg;
    FO_API_RETURN(_client->HexMngr.GetItemPixel(x, y, item_egg));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param x ...
 * @param y ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCritterByMonitorPos, FO_API_RET_OBJ(CritterView), FO_API_ARG(int, x), FO_API_ARG(int, y))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y))
{
    FO_API_RETURN(_client->HexMngr.GetCritterPixel(x, y, false));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param x ...
 * @param y ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetEntityByMonitorPos, FO_API_RET_OBJ(Entity), FO_API_ARG(int, x), FO_API_ARG(int, y))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y))
{
    ItemHexView* item = nullptr;
    CritterView* cr = nullptr;
    _client->HexMngr.GetSmthPixel(x, y, item, cr);
    FO_API_RETURN(item ? static_cast<Entity*>(item) : static_cast<Entity*>(cr));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetMapWidth, FO_API_RET(ushort))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    if (!_client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }

    FO_API_RETURN(_client->HexMngr.GetWidth());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetMapHeight, FO_API_RET(ushort))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    if (!_client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }

    FO_API_RETURN(_client->HexMngr.GetHeight());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(IsMapHexPassed, FO_API_RET(bool), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (!_client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }
    if (hx >= _client->HexMngr.GetWidth() || hy >= _client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    FO_API_RETURN(!_client->HexMngr.GetField(hx, hy).Flags.IsNotPassed);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(IsMapHexRaked, FO_API_RET(bool), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (!_client->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }
    if (hx >= _client->HexMngr.GetWidth() || hy >= _client->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    FO_API_RETURN(!_client->HexMngr.GetField(hx, hy).Flags.IsNotRaked);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param filePath ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SaveScreenshot, FO_API_RET(void), FO_API_ARG(string, filePath))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, filePath))
{
    // _client->SprMngr.SaveTexture(nullptr, _str(filePath).formatPath(), true);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param filePath ...
 * @param text ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SaveText, FO_API_RET(void), FO_API_ARG(string, filePath), FO_API_ARG(string, text))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, filePath) FO_API_ARG_MARSHAL(string, text))
{
    auto f = DiskFileSystem::OpenFile(_str(filePath).formatPath(), true);
    if (!f) {
        throw ScriptException("Can't open file for writing", filePath);
    }

    if (text.length() > 0 && !f.Write(text.c_str(), static_cast<uint>(text.length()))) {
        throw ScriptException("Can't write file", filePath, text.length());
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param name ...
 * @param data ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SetCacheData, FO_API_RET(void), FO_API_ARG(string, name), FO_API_ARG_ARR(uchar, data))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name) FO_API_ARG_ARR_MARSHAL(uchar, data))
{
    _client->Cache.SetData(name, data);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param name ...
 * @param data ...
 * @param dataSize ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SetCacheDataExt, FO_API_RET(void), FO_API_ARG(string, name), FO_API_ARG_ARR(uchar, data), FO_API_ARG(uint, dataSize))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name) FO_API_ARG_ARR_MARSHAL(uchar, data) FO_API_ARG_MARSHAL(uint, dataSize))
{
    auto data_copy = data;
    data_copy.resize(dataSize);
    _client->Cache.SetData(name, data_copy);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param name ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCacheData, FO_API_RET_ARR(uchar), FO_API_ARG(string, name))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name))
{
    FO_API_RETURN(_client->Cache.GetData(name));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param name ...
 * @param str ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SetCacheText, FO_API_RET(void), FO_API_ARG(string, name), FO_API_ARG(string, str))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name) FO_API_ARG_MARSHAL(string, str))
{
    _client->Cache.SetString(name, str);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param name ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCacheText, FO_API_RET(string), FO_API_ARG(string, name))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name))
{
    FO_API_RETURN(_client->Cache.GetString(name));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param name ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(IsCacheEntry, FO_API_RET(bool), FO_API_ARG(string, name))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name))
{
    FO_API_RETURN(_client->Cache.HasEntry(name));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param name ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(RemoveCacheEntry, FO_API_RET(void), FO_API_ARG(string, name))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name))
{
    _client->Cache.EraseEntry(name);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param keyValues ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SetUserConfig, FO_API_RET(void), FO_API_ARG_DICT(string, string, keyValues))
#if FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_DICT_MARSHAL(string, string, keyValues))
{
    auto cfg_user = _client->FileMngr.WriteFile(CONFIG_NAME, false);
    for (const auto& [key, value] : keyValues) {
        cfg_user.SetStr(_str("{} = {}\n", key, value));
    }
    cfg_user.Save();
}
FO_API_EPILOG()
#endif
