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

#ifdef FO_API_CLIENT_IMPL
#include "NetCommand.h"
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsChosen, FO_API_RET(bool))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsChosen());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsPlayer, FO_API_RET(bool))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsPlayer());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsNpc, FO_API_RET(bool))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsNpc());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsOffline, FO_API_RET(bool))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsOffline());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsLife, FO_API_RET(bool))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsLife());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsKnockout, FO_API_RET(bool))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsKnockout());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsDead, FO_API_RET(bool))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsDead());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsFree, FO_API_RET(bool))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsFree());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsBusy, FO_API_RET(bool))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(!_this->IsFree());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsAnim3d, FO_API_RET(bool))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->Anim3d != nullptr);
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
FO_API_CRITTER_VIEW_METHOD(IsAnimAviable, FO_API_RET(bool), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2))
{
    FO_API_RETURN(_this->IsAnimAviable(anim1, anim2));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(IsAnimPlaying, FO_API_RET(bool))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsAnim());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetAnim1, FO_API_RET(uint))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->GetAnim1());
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
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2))
{
    _this->Animate(anim1, anim2, nullptr);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param anim1 ...
 * @param anim2 ...
 * @param item ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(
    AnimateEx, FO_API_RET(void), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2), FO_API_ARG_OBJ(ItemView, item))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2) FO_API_ARG_OBJ_MARSHAL(ItemView, item))
{
    _this->Animate(anim1, anim2, item);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(ClearAnim, FO_API_RET(void))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    _this->ClearAnim();
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param ms ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(Wait, FO_API_RET(void), FO_API_ARG(uint, ms))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, ms))
{
    _this->TickStart(ms);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param protoId ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(CountItem, FO_API_RET(uint), FO_API_ARG(hash, protoId))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, protoId))
{
    FO_API_RETURN(_this->CountItemPid(protoId));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param itemId ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItem, FO_API_RET_OBJ(ItemView), FO_API_ARG(uint, itemId))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId))
{
    FO_API_RETURN(_this->GetItem(itemId));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param predicate ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItemPredicate, FO_API_RET_OBJ(ItemView), FO_API_ARG_PREDICATE(ItemView, predicate))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_PREDICATE_MARSHAL(ItemView, predicate))
{
    ItemViewVec inv_items = _this->InvItems;
    for (ItemView* item : inv_items)
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed)
            FO_API_RETURN(item);
    FO_API_RETURN((ItemView*)nullptr);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param slot ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItemBySlot, FO_API_RET_OBJ(ItemView), FO_API_ARG(int, slot))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, slot))
{
    FO_API_RETURN(_this->GetItemSlot(slot));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param protoId ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItemByPid, FO_API_RET_OBJ(ItemView), FO_API_ARG(hash, protoId))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, protoId))
{
    ProtoItem* proto_item = _client->ProtoMngr.GetProtoItem(protoId);
    if (!proto_item)
        FO_API_RETURN((ItemView*)nullptr);

    if (proto_item->GetStackable())
    {
        for (auto it = _this->InvItems.begin(), end = _this->InvItems.end(); it != end; ++it)
        {
            ItemView* item = *it;
            if (item->GetProtoId() == protoId)
                FO_API_RETURN(item);
        }
    }
    else
    {
        ItemView* another_slot = nullptr;
        for (auto it = _this->InvItems.begin(), end = _this->InvItems.end(); it != end; ++it)
        {
            ItemView* item = *it;
            if (item->GetProtoId() == protoId)
            {
                if (!item->GetCritSlot())
                    FO_API_RETURN(item);
                another_slot = item;
            }
        }
        FO_API_RETURN(another_slot);
    }
    FO_API_RETURN((ItemView*)nullptr);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItems, FO_API_RET_OBJ_ARR(ItemView))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->InvItems);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param slot ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItemsBySlot, FO_API_RET_OBJ_ARR(ItemView), FO_API_ARG(int, slot))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, slot))
{
    ItemViewVec items;
    _this->GetItemsSlot(slot, items);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param predicate ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetItemsPredicate, FO_API_RET_OBJ_ARR(ItemView), FO_API_ARG_PREDICATE(ItemView, predicate))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_PREDICATE_MARSHAL(ItemView, predicate))
{
    ItemViewVec inv_items = _this->InvItems;
    ItemViewVec items;
    items.reserve(inv_items.size());
    for (ItemView* item : inv_items)
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed)
            items.push_back(item);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param visible ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(SetVisible, FO_API_RET(void), FO_API_ARG(bool, visible))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, visible))
{
    _this->Visible = visible;
    _client->HexMngr.RefreshMap();
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(GetVisible, FO_API_RET(bool))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->Visible);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param value ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(set_ContourColor, FO_API_RET(void), FO_API_ARG(uint, value))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, value))
{
    if (_this->SprDrawValid)
        _this->SprDraw->SetContour(_this->SprDraw->ContourType, value);

    _this->ContourColor = value;
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(get_ContourColor, FO_API_RET(uint))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->ContourColor);
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
FO_API_CRITTER_VIEW_METHOD(GetNameTextInfo, FO_API_RET(void), FO_API_ARG_REF(bool, nameVisible), FO_API_ARG_REF(int, x),
    FO_API_ARG_REF(int, y), FO_API_ARG_REF(int, w), FO_API_ARG_REF(int, h), FO_API_ARG_REF(int, lines))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(bool, nameVisible) FO_API_ARG_REF_MARSHAL(int, x) FO_API_ARG_REF_MARSHAL(int, y)
        FO_API_ARG_REF_MARSHAL(int, w) FO_API_ARG_REF_MARSHAL(int, h) FO_API_ARG_REF_MARSHAL(int, lines))
{
    _this->GetNameTextInfo(nameVisible, x, y, w, h, lines);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param anim1 ...
 * @param anim2 ...
 * @param normalizedTime ...
 * @param animationCallback ...
 ******************************************************************************/
FO_API_CRITTER_VIEW_METHOD(AddAnimationCallback, FO_API_RET(void), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2),
    FO_API_ARG(float, normalizedTime), FO_API_ARG_CALLBACK(CritterView, animationCallback))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2) FO_API_ARG_MARSHAL(float, normalizedTime)
        FO_API_ARG_CALLBACK_MARSHAL(CritterView, animationCallback))
{
    if (!_this->Anim3d)
        throw ScriptException("Critter is not 3d");
    if (normalizedTime < 0.0f || normalizedTime > 1.0f)
        throw ScriptException("Normalized time is not in range 0..1", normalizedTime);

    _this->Anim3d->AnimationCallbacks.push_back({anim1, anim2, normalizedTime, [_this, animationCallback] {
                                                     if (!_this->IsDestroyed)
                                                         animationCallback(_this);
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
FO_API_CRITTER_VIEW_METHOD(GetBonePosition, FO_API_RET(bool), FO_API_ARG(hash, boneName), FO_API_ARG_REF(int, boneX),
    FO_API_ARG_REF(int, boneY))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, boneName) FO_API_ARG_REF_MARSHAL(int, boneX) FO_API_ARG_REF_MARSHAL(int, boneY))
{
    if (!_this->Anim3d)
        throw ScriptException("Critter is not 3d");

    if (!_this->Anim3d->GetBonePos(boneName, boneX, boneY))
        FO_API_RETURN(false);

    boneX += _this->SprOx;
    boneY += _this->SprOy;
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param count ...
 * @return ...
 ******************************************************************************/
FO_API_ITEM_VIEW_METHOD(Clone, FO_API_RET_OBJ(ItemView), FO_API_ARG(uint, count))
#ifdef FO_API_ITEM_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, count))
{
    ItemView* clone = _this->Clone();
    if (count)
        clone->SetCount(count);
    FO_API_RETURN(clone);
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
FO_API_ITEM_VIEW_METHOD(GetMapPosition, FO_API_RET(bool), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy))
#ifdef FO_API_ITEM_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, hx) FO_API_ARG_REF_MARSHAL(ushort, hy))
{
    if (!_client->HexMngr.IsMapLoaded())
        throw ScriptException("Map is not loaded");

    switch (_this->GetAccessory())
    {
    case ITEM_ACCESSORY_CRITTER: {
        CritterView* cr = _client->GetCritter(_this->GetCritId());
        if (!cr)
            throw ScriptException("CritterCl accessory, CritterCl not found");
        hx = cr->GetHexX();
        hy = cr->GetHexY();
    }
    break;
    case ITEM_ACCESSORY_HEX: {
        hx = _this->GetHexX();
        hy = _this->GetHexY();
    }
    break;
    case ITEM_ACCESSORY_CONTAINER: {
        if (_this->GetId() == _this->GetContainerId())
            throw ScriptException("Container accessory, crosslinks");
        ItemView* cont = _client->GetItem(_this->GetContainerId());
        if (!cont)
            throw ScriptException("Container accessory, container not found");
        // FO_API_RETURN(Item_GetMapPosition(cont, hx, hy)); // Recursion
    }
    break;
    default:
        throw ScriptException("Unknown accessory");
        FO_API_RETURN(false);
    }
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param fromFrame ...
 * @param toFrame ...
 ******************************************************************************/
FO_API_ITEM_VIEW_METHOD(Animate, FO_API_RET(void), FO_API_ARG(uint, fromFrame), FO_API_ARG(uint, toFrame))
#ifdef FO_API_ITEM_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, fromFrame) FO_API_ARG_MARSHAL(uint, toFrame))
{
    if (_this->Type == EntityType::ItemHexView)
    {
        ItemHexView* item_hex = (ItemHexView*)_this;
        item_hex->SetAnim(fromFrame, toFrame);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param stackId ...
 * @return ...
 ******************************************************************************/
FO_API_ITEM_VIEW_METHOD(GetItems, FO_API_RET_OBJ_ARR(ItemView), FO_API_ARG(uint, stackId))
#ifdef FO_API_ITEM_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, stackId))
{
    ItemViewVec items;
    // Todo: need attention!
    // _this->ContGetItems(items, stackId);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param command ...
 * @param separator ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(CustomCall, FO_API_RET(string), FO_API_ARG(string, command), FO_API_ARG(string, separator))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, command) FO_API_ARG_MARSHAL(string, separator))
{
    // Parse command
    vector<string> args;
    std::stringstream ss(command);
    if (separator.length() > 0)
    {
        string arg;
        while (getline(ss, arg, *separator.c_str()))
            args.push_back(arg);
    }
    else
    {
        args.push_back(command);
    }
    if (args.size() < 1)
        throw ScriptException("Empty custom call command");

    // Execute
    string cmd = args[0];
    if (cmd == "Login" && args.size() >= 3)
    {
        if (_client->InitNetReason == INIT_NET_REASON_NONE)
        {
            _client->LoginName = args[1];
            _client->LoginPassword = args[2];
            _client->InitNetReason = INIT_NET_REASON_LOGIN;
        }
    }
    else if (cmd == "Register" && args.size() >= 3)
    {
        if (_client->InitNetReason == INIT_NET_REASON_NONE)
        {
            _client->LoginName = args[1];
            _client->LoginPassword = args[2];
            _client->InitNetReason = INIT_NET_REASON_REG;
        }
    }
    else if (cmd == "CustomConnect")
    {
        if (_client->InitNetReason == INIT_NET_REASON_NONE)
        {
            _client->InitNetReason = INIT_NET_REASON_CUSTOM;
            if (!_client->UpdateFilesInProgress)
                _client->UpdateFilesStart();
        }
    }
    else if (cmd == "DumpAtlases")
    {
        _client->SprMngr.DumpAtlases();
    }
    else if (cmd == "SwitchShowTrack")
    {
        _client->HexMngr.SwitchShowTrack();
    }
    else if (cmd == "SwitchShowHex")
    {
        _client->HexMngr.SwitchShowHex();
    }
    else if (cmd == "SwitchFullscreen")
    {
        if (!_client->Settings.FullScreen)
        {
            if (_client->SprMngr.EnableFullscreen())
                _client->Settings.FullScreen = true;
        }
        else
        {
            if (_client->SprMngr.DisableFullscreen())
            {
                _client->Settings.FullScreen = false;

                if (_client->WindowResolutionDiffX || _client->WindowResolutionDiffY)
                {
                    int x, y;
                    _client->SprMngr.GetWindowPosition(x, y);
                    _client->SprMngr.SetWindowPosition(
                        x - _client->WindowResolutionDiffX, y - _client->WindowResolutionDiffY);
                    _client->WindowResolutionDiffX = _client->WindowResolutionDiffY = 0;
                }
            }
        }
    }
    else if (cmd == "MinimizeWindow")
    {
        _client->SprMngr.MinimizeWindow();
    }
    else if (cmd == "SwitchLookBorders")
    {
        // _client->DrawLookBorders = !_client->DrawLookBorders;
        // _client->RebuildLookBorders = true;
    }
    else if (cmd == "SwitchShootBorders")
    {
        // _client->DrawShootBorders = !_client->DrawShootBorders;
        // _client->RebuildLookBorders = true;
    }
    else if (cmd == "GetShootBorders")
    {
        FO_API_RETURN(_client->DrawShootBorders ? "true" : "false");
    }
    else if (cmd == "SetShootBorders" && args.size() >= 2)
    {
        bool set = (args[1] == "true");
        if (_client->DrawShootBorders != set)
        {
            _client->DrawShootBorders = set;
            _client->RebuildLookBorders = true;
        }
    }
    else if (cmd == "SetMousePos" && args.size() == 4)
    {
#ifndef FO_WEB
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
    else if (cmd == "SetCursorPos")
    {
        if (_client->HexMngr.IsMapLoaded())
            _client->HexMngr.SetCursorPos(
                _client->Settings.MouseX, _client->Settings.MouseY, _client->Keyb.CtrlDwn, true);
    }
    else if (cmd == "NetDisconnect")
    {
        _client->NetDisconnect();

        if (!_client->IsConnected && !_client->IsMainScreen(SCREEN_LOGIN))
            _client->ShowMainScreen(SCREEN_LOGIN, {});
    }
    else if (cmd == "TryExit")
    {
        _client->TryExit();
    }
    else if (cmd == "Version")
    {
        FO_API_RETURN(_str("{}", FO_VERSION));
    }
    else if (cmd == "BytesSend")
    {
        FO_API_RETURN(_str("{}", _client->BytesSend));
    }
    else if (cmd == "BytesReceive")
    {
        FO_API_RETURN(_str("{}", _client->BytesReceive));
    }
    else if (cmd == "GetLanguage")
    {
        FO_API_RETURN(_client->CurLang.NameStr);
    }
    else if (cmd == "SetLanguage" && args.size() >= 2)
    {
        if (args[1].length() == 4)
            _client->CurLang.LoadFromCache(_client->Cache, args[1]);
    }
    else if (cmd == "SetResolution" && args.size() >= 3)
    {
        int w = _str(args[1]).toInt();
        int h = _str(args[2]).toInt();
        int diff_w = w - _client->Settings.ScreenWidth;
        int diff_h = h - _client->Settings.ScreenHeight;

        _client->Settings.ScreenWidth = w;
        _client->Settings.ScreenHeight = h;
        _client->SprMngr.SetWindowSize(w, h);

        if (!_client->Settings.FullScreen)
        {
            int x, y;
            _client->SprMngr.GetWindowPosition(x, y);
            _client->SprMngr.SetWindowPosition(x - diff_w / 2, y - diff_h / 2);
        }
        else
        {
            _client->WindowResolutionDiffX += diff_w / 2;
            _client->WindowResolutionDiffY += diff_h / 2;
        }

        _client->SprMngr.OnResolutionChanged();
        if (_client->HexMngr.IsMapLoaded())
            _client->HexMngr.OnResolutionChanged();
    }
    else if (cmd == "RefreshAlwaysOnTop")
    {
        _client->SprMngr.SetAlwaysOnTop(_client->Settings.AlwaysOnTop);
    }
    else if (cmd == "Command" && args.size() >= 2)
    {
        string str;
        for (size_t i = 1; i < args.size(); i++)
            str += args[i] + " ";
        str = _str(str).trim();

        string buf;
        if (!PackNetCommand(
                str, &_client->Bout, [&buf, &separator](auto s) { buf += s + separator; }, _client->Chosen->Name))
            FO_API_RETURN("UNKNOWN");

        FO_API_RETURN(buf);
    }
    else if (cmd == "ConsoleMessage" && args.size() >= 2)
    {
        _client->Net_SendText(args[1].c_str(), SAY_NORM);
    }
    else if (cmd == "SaveLog" && args.size() == 3)
    {
        //              if( file_name == "" )
        //              {
        //                      DateTime dt;
        //                      Timer::GetCurrentDateTime(dt);
        //                      char     log_path[MAX_FOPATH];
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
    else if (cmd == "DialogAnswer" && args.size() >= 4)
    {
        bool is_npc = (args[1] == "true");
        uint talker_id = _str(args[2]).toUInt();
        uint answer_index = _str(args[3]).toUInt();
        _client->Net_SendTalk(is_npc, talker_id, answer_index);
    }
    else if (cmd == "DrawMiniMap" && args.size() >= 6)
    {
        static int zoom, x, y, x2, y2;
        zoom = _str(args[1]).toInt();
        x = _str(args[2]).toInt();
        y = _str(args[3]).toInt();
        x2 = x + _str(args[4]).toInt();
        y2 = y + _str(args[5]).toInt();

        if (zoom != _client->LmapZoom || x != _client->LmapWMap[0] || y != _client->LmapWMap[1] ||
            x2 != _client->LmapWMap[2] || y2 != _client->LmapWMap[3])
        {
            _client->LmapZoom = zoom;
            _client->LmapWMap[0] = x;
            _client->LmapWMap[1] = y;
            _client->LmapWMap[2] = x2;
            _client->LmapWMap[3] = y2;
            _client->LmapPrepareMap();
        }
        else if (Timer::FastTick() >= _client->LmapPrepareNextTick)
        {
            _client->LmapPrepareMap();
        }

        _client->SprMngr.DrawPoints(_client->LmapPrepPix, RenderPrimitiveType::LineList);
    }
    else if (cmd == "RefreshMe")
    {
        _client->Net_SendRefereshMe();
    }
    else if (cmd == "SetCrittersContour" && args.size() == 2)
    {
        int countour_type = _str(args[1]).toInt();
        _client->HexMngr.SetCrittersContour(countour_type);
    }
    else if (cmd == "DrawWait")
    {
        _client->WaitDraw();
    }
    else if (cmd == "ChangeDir" && args.size() == 2)
    {
        int dir = _str(args[1]).toInt();
        _client->Chosen->ChangeDir(dir);
        _client->Net_SendDir();
    }
    else if (cmd == "MoveItem" && args.size() == 5)
    {
        uint item_count = _str(args[1]).toUInt();
        uint item_id = _str(args[2]).toUInt();
        uint item_swap_id = _str(args[3]).toUInt();
        int to_slot = _str(args[4]).toInt();
        ItemView* item = _client->Chosen->GetItem(item_id);
        ItemView* item_swap = (item_swap_id ? _client->Chosen->GetItem(item_swap_id) : nullptr);
        ItemView* old_item = item->Clone();
        int from_slot = item->GetCritSlot();

        // Move
        bool is_light = item->GetIsLight();
        if (to_slot == -1)
        {
            _client->Chosen->Action(ACTION_DROP_ITEM, from_slot, item);
            if (item->GetStackable() && item_count < item->GetCount())
            {
                item->SetCount(item->GetCount() - item_count);
            }
            else
            {
                _client->Chosen->DeleteItem(item, true);
                item = nullptr;
            }
        }
        else
        {
            item->SetCritSlot(to_slot);
            if (item_swap)
                item_swap->SetCritSlot(from_slot);

            _client->Chosen->Action(ACTION_MOVE_ITEM, from_slot, item);
            if (item_swap)
                _client->Chosen->Action(ACTION_MOVE_ITEM_SWAP, to_slot, item_swap);
        }

        // Light
        _client->RebuildLookBorders = true;
        if (is_light && (!to_slot || (!from_slot && to_slot != -1)))
            _client->HexMngr.RebuildLight();

        // Notify scripts about item changing
        _client->OnItemInvChanged(old_item, item);
    }
    else if (cmd == "SkipRoof" && args.size() == 3)
    {
        uint hx = _str(args[1]).toUInt();
        uint hy = _str(args[2]).toUInt();
        _client->HexMngr.SetSkipRoof(hx, hy);
    }
    else if (cmd == "RebuildLookBorders")
    {
        _client->RebuildLookBorders = true;
    }
    else if (cmd == "TransitCritter" && args.size() == 5)
    {
        int hx = _str(args[1]).toInt();
        int hy = _str(args[2]).toInt();
        bool animate = _str(args[3]).toBool();
        bool force = _str(args[4]).toBool();

        _client->HexMngr.TransitCritter(_client->Chosen, hx, hy, animate, force);
    }
    else if (cmd == "SendMove")
    {
        UCharVec dirs;
        for (size_t i = 1; i < args.size(); i++)
            dirs.push_back((uchar)_str(args[i]).toInt());

        _client->Net_SendMove(dirs);

        if (dirs.size() > 1)
        {
            _client->Chosen->MoveSteps.resize(1);
        }
        else
        {
            _client->Chosen->MoveSteps.resize(0);
            if (!_client->Chosen->IsAnim())
                _client->Chosen->AnimateStay();
        }
    }
    else if (cmd == "ChosenAlpha" && args.size() == 2)
    {
        int alpha = _str(args[1]).toInt();

        _client->Chosen->Alpha = (uchar)alpha;
    }
    else if (cmd == "SetScreenKeyboard" && args.size() == 2)
    {
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
    else
    {
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    if (_client->Chosen && _client->Chosen->IsDestroyed)
        FO_API_RETURN((CritterView*)nullptr);
    FO_API_RETURN(_client->Chosen);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param itemId ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetItem, FO_API_RET_OBJ(ItemView), FO_API_ARG(uint, itemId))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId))
{
    if (!itemId)
        throw ScriptException("Item id arg is zero");

    // On map
    ItemView* item = _client->GetItem(itemId);

    // On Chosen
    if (!item && _client->Chosen)
        item = _client->Chosen->GetItem(itemId);

    // On other critters
    if (!item)
    {
        for (auto it = _client->HexMngr.GetCritters().begin(); !item && it != _client->HexMngr.GetCritters().end();
             ++it)
            if (!it->second->IsChosen())
                item = it->second->GetItem(itemId);
    }

    if (!item || item->IsDestroyed)
        FO_API_RETURN((ItemView*)nullptr);
    FO_API_RETURN(item);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetMapAllItems, FO_API_RET_OBJ_ARR(ItemView))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    ItemViewVec items;
    if (_client->HexMngr.IsMapLoaded())
    {
        ItemHexViewVec& items_ = _client->HexMngr.GetItems();
        for (auto it = items_.begin(); it != items_.end();)
            it = ((*it)->IsFinishing() ? items_.erase(it) : ++it);
        for (ItemView* item : items_)
            items.push_back(item);
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
FO_API_GLOBAL_CLIENT_FUNC(GetMapHexItems, FO_API_RET_OBJ_ARR(ItemView), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    ItemHexViewVec items;
    if (_client->HexMngr.IsMapLoaded())
    {
        _client->HexMngr.GetItems(hx, hy, items);
        for (auto it = items.begin(); it != items.end();)
            it = ((*it)->IsFinishing() ? items.erase(it) : ++it);
    }
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param cr1 ...
 * @param cr2 ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(
    GetCrittersDistantion, FO_API_RET(uint), FO_API_ARG_OBJ(CritterView, cr1), FO_API_ARG_OBJ(CritterView, cr2))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(CritterView, cr1) FO_API_ARG_OBJ_MARSHAL(CritterView, cr2))
{
    if (!_client->HexMngr.IsMapLoaded())
        throw ScriptException("Map is not loaded");
    if (!cr1)
        throw ScriptException("Critter1 arg is null");

    if (!cr2)
        throw ScriptException("Critter2 arg is null");

    FO_API_RETURN(_client->GeomHelper.DistGame(cr1->GetHexX(), cr1->GetHexY(), cr2->GetHexX(), cr2->GetHexY()));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param critterId ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCritter, FO_API_RET_OBJ(CritterView), FO_API_ARG(uint, critterId))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, critterId))
{
    if (!critterId)
        FO_API_RETURN((CritterView*)nullptr); // throw ScriptException("Critter id arg is zero");
    CritterView* cr = _client->GetCritter(critterId);
    if (!cr || cr->IsDestroyed)
        FO_API_RETURN((CritterView*)nullptr);
    FO_API_RETURN(cr);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param radius ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCritters, FO_API_RET_OBJ_ARR(CritterView), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy),
    FO_API_ARG(uint, radius), FO_API_ARG(int, findType))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, radius)
        FO_API_ARG_MARSHAL(int, findType))
{
    if (hx >= _client->HexMngr.GetWidth() || hy >= _client->HexMngr.GetHeight())
        throw ScriptException("Invalid hexes args");

    CritterViewMap& crits = _client->HexMngr.GetCritters();
    CritterViewVec critters;
    for (auto it = crits.begin(), end = crits.end(); it != end; ++it)
    {
        CritterView* cr = it->second;
        if (cr->CheckFind(findType) && _client->GeomHelper.CheckDist(hx, hy, cr->GetHexX(), cr->GetHexY(), radius))
            critters.push_back(cr);
    }

    std::sort(critters.begin(), critters.end(), [_client, &hx, &hy](CritterView* cr1, CritterView* cr2) {
        return _client->GeomHelper.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY()) <
            _client->GeomHelper.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
    });

    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(
    GetCrittersByPids, FO_API_RET_OBJ_ARR(CritterView), FO_API_ARG(hash, pid), FO_API_ARG(int, findType))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_MARSHAL(int, findType))
{
    CritterViewMap& crits = _client->HexMngr.GetCritters();
    CritterViewVec critters;
    if (!pid)
    {
        for (auto it = crits.begin(), end = crits.end(); it != end; ++it)
        {
            CritterView* cr = it->second;
            if (cr->CheckFind(findType))
                critters.push_back(cr);
        }
    }
    else
    {
        for (auto it = crits.begin(), end = crits.end(); it != end; ++it)
        {
            CritterView* cr = it->second;
            if (cr->IsNpc() && cr->GetProtoId() == pid && cr->CheckFind(findType))
                critters.push_back(cr);
        }
    }
    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

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
FO_API_GLOBAL_CLIENT_FUNC(GetCrittersInPath, FO_API_RET_OBJ_ARR(CritterView), FO_API_ARG(ushort, fromHx),
    FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(float, angle),
    FO_API_ARG(uint, dist), FO_API_ARG(int, findType))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx)
        FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist)
            FO_API_ARG_MARSHAL(int, findType))
{
    CritterViewVec critters;
    _client->HexMngr.TraceBullet(
        fromHx, fromHy, toHx, toHy, dist, angle, nullptr, false, &critters, findType, nullptr, nullptr, nullptr, true);
    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

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
FO_API_GLOBAL_CLIENT_FUNC(GetCrittersInPathBlock, FO_API_RET_OBJ_ARR(CritterView), FO_API_ARG(ushort, fromHx),
    FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(float, angle),
    FO_API_ARG(uint, dist), FO_API_ARG(int, findType), FO_API_ARG_REF(ushort, preBlockHx),
    FO_API_ARG_REF(ushort, preBlockHy), FO_API_ARG_REF(ushort, blockHx), FO_API_ARG_REF(ushort, blockHy))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx)
        FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist)
            FO_API_ARG_MARSHAL(int, findType) FO_API_ARG_REF_MARSHAL(ushort, preBlockHx) FO_API_ARG_REF_MARSHAL(
                ushort, preBlockHy) FO_API_ARG_REF_MARSHAL(ushort, blockHx) FO_API_ARG_REF_MARSHAL(ushort, blockHy))
{
    CritterViewVec critters;
    UShortPair block, pre_block;
    _client->HexMngr.TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, nullptr, false, &critters, findType, &block,
        &pre_block, nullptr, true);
    preBlockHx = pre_block.first;
    preBlockHy = pre_block.second;
    blockHx = block.first;
    blockHy = block.second;
    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

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
FO_API_GLOBAL_CLIENT_FUNC(GetHexInPath, FO_API_RET(void), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy),
    FO_API_ARG_REF(ushort, toHx), FO_API_ARG_REF(ushort, toHy), FO_API_ARG(float, angle), FO_API_ARG(uint, dist))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_REF_MARSHAL(ushort, toHx)
        FO_API_ARG_REF_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist))
{
    UShortPair pre_block, block;
    _client->HexMngr.TraceBullet(
        fromHx, fromHy, toHx, toHy, dist, angle, nullptr, false, nullptr, 0, &block, &pre_block, nullptr, true);
    toHx = pre_block.first;
    toHy = pre_block.second;
}
FO_API_EPILOG()
#endif

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
FO_API_GLOBAL_CLIENT_FUNC(GetPathHex, FO_API_RET_ARR(uchar), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy),
    FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx)
        FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(uint, cut))
{
    if (fromHx >= _client->HexMngr.GetWidth() || fromHy >= _client->HexMngr.GetHeight())
        throw ScriptException("Invalid from hexes args");
    if (toHx >= _client->HexMngr.GetWidth() || toHy >= _client->HexMngr.GetHeight())
        throw ScriptException("Invalid to hexes args");

    if (cut > 0 && !_client->HexMngr.CutPath(nullptr, fromHx, fromHy, toHx, toHy, cut))
        FO_API_RETURN(UCharVec());

    UCharVec steps;
    if (!_client->HexMngr.FindPath(nullptr, fromHx, fromHy, toHx, toHy, steps, -1))
        FO_API_RETURN(UCharVec());

    FO_API_RETURN(steps);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @param toHx ...
 * @param toHy ...
 * @param cut ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetPathCr, FO_API_RET_ARR(uchar), FO_API_ARG_OBJ(CritterView, cr), FO_API_ARG(ushort, toHx),
    FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(CritterView, cr) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy)
        FO_API_ARG_MARSHAL(uint, cut))
{
    if (toHx >= _client->HexMngr.GetWidth() || toHy >= _client->HexMngr.GetHeight())
        throw ScriptException("Invalid to hexes args");

    if (cut > 0 && !_client->HexMngr.CutPath(cr, cr->GetHexX(), cr->GetHexY(), toHx, toHy, cut))
        FO_API_RETURN(UCharVec());

    UCharVec steps;
    if (!_client->HexMngr.FindPath(cr, cr->GetHexX(), cr->GetHexY(), toHx, toHy, steps, -1))
        FO_API_RETURN(UCharVec());

    FO_API_RETURN(steps);
}
FO_API_EPILOG(0)
#endif

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
FO_API_GLOBAL_CLIENT_FUNC(GetPathLengthHex, FO_API_RET(uint), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy),
    FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx)
        FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(uint, cut))
{
    if (fromHx >= _client->HexMngr.GetWidth() || fromHy >= _client->HexMngr.GetHeight())
        throw ScriptException("Invalid from hexes args");
    if (toHx >= _client->HexMngr.GetWidth() || toHy >= _client->HexMngr.GetHeight())
        throw ScriptException("Invalid to hexes args");

    if (cut > 0 && !_client->HexMngr.CutPath(nullptr, fromHx, fromHy, toHx, toHy, cut))
        FO_API_RETURN(0);

    UCharVec steps;
    if (!_client->HexMngr.FindPath(nullptr, fromHx, fromHy, toHx, toHy, steps, -1))
        steps.clear();

    FO_API_RETURN((uint)steps.size());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @param toHx ...
 * @param toHy ...
 * @param cut ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetPathLengthCr, FO_API_RET(uint), FO_API_ARG_OBJ(CritterView, cr), FO_API_ARG(ushort, toHx),
    FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(CritterView, cr) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy)
        FO_API_ARG_MARSHAL(uint, cut))
{
    if (toHx >= _client->HexMngr.GetWidth() || toHy >= _client->HexMngr.GetHeight())
        throw ScriptException("Invalid to hexes args");

    if (cut > 0 && !_client->HexMngr.CutPath(cr, cr->GetHexX(), cr->GetHexY(), toHx, toHy, cut))
        FO_API_RETURN(0);

    UCharVec steps;
    if (!_client->HexMngr.FindPath(cr, cr->GetHexX(), cr->GetHexY(), toHx, toHy, steps, -1))
        steps.clear();

    FO_API_RETURN((uint)steps.size());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param fromColor ...
 * @param toColor ...
 * @param ms ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(
    FlushScreen, FO_API_RET(void), FO_API_ARG(uint, fromColor), FO_API_ARG(uint, toColor), FO_API_ARG(uint, ms))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
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
FO_API_GLOBAL_CLIENT_FUNC(PlaySound, FO_API_RET(bool), FO_API_ARG(string, soundName))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, soundName))
{
    FO_API_RETURN(_client->SndMngr.PlaySound(_client->ResMngr.GetSoundNames(), soundName));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param musicName ...
 * @param repeatTime ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(PlayMusic, FO_API_RET(bool), FO_API_ARG(string, musicName), FO_API_ARG(uint, repeatTime))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, musicName) FO_API_ARG_MARSHAL(uint, repeatTime))
{
    if (musicName.empty())
    {
        _client->SndMngr.StopMusic();
        FO_API_RETURN(true);
    }

    FO_API_RETURN(_client->SndMngr.PlayMusic(musicName, repeatTime));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param videoName ...
 * @param canStop ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(PlayVideo, FO_API_RET(void), FO_API_ARG(string, videoName), FO_API_ARG(bool, canStop))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, videoName) FO_API_ARG_MARSHAL(bool, canStop))
{
    _client->SndMngr.StopMusic();
    _client->AddVideo(videoName.c_str(), canStop, true);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCurrentMapPid, FO_API_RET(hash))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    if (!_client->HexMngr.IsMapLoaded())
        FO_API_RETURN(0);
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, msg))
{
    _client->AddMess(FOMB_GAME, msg, true);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param msg ...
 * @param type ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(MessageType, FO_API_RET(void), FO_API_ARG(string, msg), FO_API_ARG(int, type))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");

    _client->AddMess(FOMB_GAME, _client->CurLang.Msg[textMsg].GetStr(strNum), true);
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
FO_API_GLOBAL_CLIENT_FUNC(
    MessageMsgType, FO_API_RET(void), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum), FO_API_ARG(int, type))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum) FO_API_ARG_MARSHAL(int, type))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");

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
FO_API_GLOBAL_CLIENT_FUNC(MapMessage, FO_API_RET(void), FO_API_ARG(string, text), FO_API_ARG(ushort, hx),
    FO_API_ARG(ushort, hy), FO_API_ARG(uint, ms), FO_API_ARG(uint, color), FO_API_ARG(bool, fade), FO_API_ARG(int, ox),
    FO_API_ARG(int, oy))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy)
        FO_API_ARG_MARSHAL(uint, ms) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(bool, fade)
            FO_API_ARG_MARSHAL(int, ox) FO_API_ARG_MARSHAL(int, oy))
{
    FOClient::MapText t;
    t.HexX = hx;
    t.HexY = hy;
    t.Color = (color ? color : COLOR_TEXT);
    t.Fade = fade;
    t.StartTick = Timer::GameTick();
    t.Tick = ms;
    t.Text = text;
    t.Pos = _client->HexMngr.GetRectForText(hx, hy);
    t.EndPos = Rect(t.Pos, ox, oy);
    auto it = std::find(_client->GameMapTexts.begin(), _client->GameMapTexts.end(), t);
    if (it != _client->GameMapTexts.end())
        _client->GameMapTexts.erase(it);
    _client->GameMapTexts.push_back(t);
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");
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
FO_API_GLOBAL_CLIENT_FUNC(
    GetMsgStrSkip, FO_API_RET(string), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum), FO_API_ARG(uint, skipCount))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum) FO_API_ARG_MARSHAL(uint, skipCount))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");
    FO_API_RETURN(_client->CurLang.Msg[textMsg].Count(strNum) > 0);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param replace ...
 * @param str ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(
    ReplaceTextStr, FO_API_RET(string), FO_API_ARG(string, text), FO_API_ARG(string, replace), FO_API_ARG(string, str))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(string, replace) FO_API_ARG_MARSHAL(string, str))
{
    size_t pos = text.find(replace, 0);
    if (pos == std::string::npos)
        FO_API_RETURN(text);
    FO_API_RETURN(string(text).replace(pos, replace.length(), str));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param replace ...
 * @param i ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(
    ReplaceTextInt, FO_API_RET(string), FO_API_ARG(string, text), FO_API_ARG(string, replace), FO_API_ARG(int, i))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(string, replace) FO_API_ARG_MARSHAL(int, i))
{
    size_t pos = text.find(replace, 0);
    if (pos == std::string::npos)
        FO_API_RETURN(text);
    FO_API_RETURN(string(text).replace(pos, replace.length(), _str("{}", i)));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param lexems ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(FormatTags, FO_API_RET(string), FO_API_ARG(string, text), FO_API_ARG(string, lexems))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(string, lexems))
{
    _client->FormatTags(text, _client->Chosen, nullptr, lexems);
    FO_API_RETURN(text);
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
FO_API_GLOBAL_CLIENT_FUNC(MoveScreenToHex, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy),
    FO_API_ARG(uint, speed), FO_API_ARG(bool, canStop))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, speed)
        FO_API_ARG_MARSHAL(bool, canStop))
{
    if (!_client->HexMngr.IsMapLoaded())
        throw ScriptException("Map is not loaded");
    if (hx >= _client->HexMngr.GetWidth() || hy >= _client->HexMngr.GetHeight())
        throw ScriptException("Invalid hex args");

    if (!speed)
        _client->HexMngr.FindSetCenter(hx, hy);
    else
        _client->HexMngr.ScrollToHex(hx, hy, (float)speed / 1000.0f, canStop);
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
FO_API_GLOBAL_CLIENT_FUNC(MoveScreenOffset, FO_API_RET(void), FO_API_ARG(int, ox), FO_API_ARG(int, oy),
    FO_API_ARG(uint, speed), FO_API_ARG(bool, canStop))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, ox) FO_API_ARG_MARSHAL(int, oy) FO_API_ARG_MARSHAL(uint, speed)
        FO_API_ARG_MARSHAL(bool, canStop))
{
    if (!_client->HexMngr.IsMapLoaded())
        throw ScriptException("Map is not loaded");

    _client->HexMngr.ScrollOffset(ox, oy, (float)speed / 1000.0f, canStop);
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
FO_API_GLOBAL_CLIENT_FUNC(LockScreenScroll, FO_API_RET(void), FO_API_ARG_OBJ(CritterView, cr),
    FO_API_ARG(bool, softLock), FO_API_ARG(bool, unlockIfSame))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(
    FO_API_ARG_OBJ_MARSHAL(CritterView, cr) FO_API_ARG_MARSHAL(bool, softLock) FO_API_ARG_MARSHAL(bool, unlockIfSame))
{
    uint id = (cr ? cr->GetId() : 0);
    if (softLock)
    {
        if (unlockIfSame && id == _client->HexMngr.AutoScroll.SoftLockedCritter)
            _client->HexMngr.AutoScroll.SoftLockedCritter = 0;
        else
            _client->HexMngr.AutoScroll.SoftLockedCritter = id;

        _client->HexMngr.AutoScroll.CritterLastHexX = (cr ? cr->GetHexX() : 0);
        _client->HexMngr.AutoScroll.CritterLastHexY = (cr ? cr->GetHexY() : 0);
    }
    else
    {
        if (unlockIfSame && id == _client->HexMngr.AutoScroll.HardLockedCritter)
            _client->HexMngr.AutoScroll.HardLockedCritter = 0;
        else
            _client->HexMngr.AutoScroll.HardLockedCritter = id;
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param zoneX ...
 * @param zoneY ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetFog, FO_API_RET(int), FO_API_ARG(ushort, zoneX), FO_API_ARG(ushort, zoneY))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, zoneX) FO_API_ARG_MARSHAL(ushort, zoneY))
{
    if (zoneX >= _client->Settings.GlobalMapWidth || zoneY >= _client->Settings.GlobalMapHeight)
        throw ScriptException("Invalid world map pos arg");
    FO_API_RETURN(_client->GmapFog.Get2Bit(zoneX, zoneY));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param dayPart ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetDayTime, FO_API_RET(uint), FO_API_ARG(uint, dayPart))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, dayPart))
{
    if (dayPart >= 4)
        throw ScriptException("Invalid day part arg");

    if (_client->HexMngr.IsMapLoaded())
        FO_API_RETURN(_client->HexMngr.GetMapDayTime()[dayPart]);
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
FO_API_GLOBAL_CLIENT_FUNC(GetDayColor, FO_API_RET(void), FO_API_ARG(uint, dayPart), FO_API_ARG_REF(uchar, r),
    FO_API_ARG_REF(uchar, g), FO_API_ARG_REF(uchar, b))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, dayPart) FO_API_ARG_REF_MARSHAL(uchar, r) FO_API_ARG_REF_MARSHAL(uchar, g)
        FO_API_ARG_REF_MARSHAL(uchar, b))
{
    r = g = b = 0;
    if (dayPart >= 4)
        throw ScriptException("Invalid day part arg");

    if (_client->HexMngr.IsMapLoaded())
    {
        uchar* col = _client->HexMngr.GetMapDayColor();
        r = col[0 + dayPart];
        g = col[4 + dayPart];
        b = col[8 + dayPart];
    }
}
FO_API_EPILOG()
#endif

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
FO_API_GLOBAL_CLIENT_FUNC(GetFullSecond, FO_API_RET(uint), FO_API_ARG(ushort, year), FO_API_ARG(ushort, month),
    FO_API_ARG(ushort, day), FO_API_ARG(ushort, hour), FO_API_ARG(ushort, minute), FO_API_ARG(ushort, second))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, year) FO_API_ARG_MARSHAL(ushort, month) FO_API_ARG_MARSHAL(ushort, day)
        FO_API_ARG_MARSHAL(ushort, hour) FO_API_ARG_MARSHAL(ushort, minute) FO_API_ARG_MARSHAL(ushort, second))
{
    if (!year)
        year = _client->Globals->GetYear();
    else
        year = CLAMP(year, _client->Globals->GetYearStart(), _client->Globals->GetYearStart() + 130);
    if (!month)
        month = _client->Globals->GetMonth();
    else
        month = CLAMP(month, 1, 12);
    if (!day)
    {
        day = _client->Globals->GetDay();
    }
    else
    {
        uint month_day = _client->GameTime.GameTimeMonthDay(year, month);
        day = CLAMP(day, 1, month_day);
    }

    if (hour > 23)
        hour = 23;
    if (minute > 59)
        minute = 59;
    if (second > 59)
        second = 59;

    FO_API_RETURN(_client->GameTime.GetFullSecond(year, month, day, hour, minute, second));
}
FO_API_EPILOG(0)
#endif

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
FO_API_GLOBAL_CLIENT_FUNC(GetGameTime, FO_API_RET(void), FO_API_ARG(uint, fullSecond), FO_API_ARG_REF(ushort, year),
    FO_API_ARG_REF(ushort, month), FO_API_ARG_REF(ushort, day), FO_API_ARG_REF(ushort, dayOfWeek),
    FO_API_ARG_REF(ushort, hour), FO_API_ARG_REF(ushort, minute), FO_API_ARG_REF(ushort, second))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, fullSecond) FO_API_ARG_REF_MARSHAL(ushort, year)
        FO_API_ARG_REF_MARSHAL(ushort, month) FO_API_ARG_REF_MARSHAL(ushort, day)
            FO_API_ARG_REF_MARSHAL(ushort, dayOfWeek) FO_API_ARG_REF_MARSHAL(ushort, hour)
                FO_API_ARG_REF_MARSHAL(ushort, minute) FO_API_ARG_REF_MARSHAL(ushort, second))
{
    DateTimeStamp dt = _client->GameTime.GetGameTime(fullSecond);
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

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param dir ...
 * @param steps ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(MoveHexByDir, FO_API_RET(void), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy),
    FO_API_ARG(uchar, dir), FO_API_ARG(uint, steps))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, hx) FO_API_ARG_REF_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uchar, dir)
        FO_API_ARG_MARSHAL(uint, steps))
{
    if (!_client->HexMngr.IsMapLoaded())
        throw ScriptException("Map not loaded");
    if (dir >= _client->Settings.MapDirCount)
        throw ScriptException("Invalid dir arg");
    if (!steps)
        throw ScriptException("Steps arg is zero");

    if (steps > 1)
    {
        for (uint i = 0; i < steps; i++)
            _client->GeomHelper.MoveHexByDir(hx, hy, dir, _client->HexMngr.GetWidth(), _client->HexMngr.GetHeight());
    }
    else
    {
        _client->GeomHelper.MoveHexByDir(hx, hy, dir, _client->HexMngr.GetWidth(), _client->HexMngr.GetHeight());
    }
}
FO_API_EPILOG()
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
FO_API_GLOBAL_CLIENT_FUNC(GetTileName, FO_API_RET(hash), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy),
    FO_API_ARG(bool, roof), FO_API_ARG(int, layer))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(bool, roof)
        FO_API_ARG_MARSHAL(int, layer))
{
    if (!_client->HexMngr.IsMapLoaded())
        throw ScriptException("Map not loaded");
    if (hx >= _client->HexMngr.GetWidth())
        throw ScriptException("Invalid hex x arg");
    if (hy >= _client->HexMngr.GetHeight())
        throw ScriptException("Invalid hex y arg");

    AnyFrames* simply_tile = _client->HexMngr.GetField(hx, hy).SimplyTile[roof ? 1 : 0];
    if (simply_tile && !layer)
        FO_API_RETURN(simply_tile->NameHash);

    Field::TileVec* tiles = _client->HexMngr.GetField(hx, hy).Tiles[roof ? 1 : 0];
    if (!tiles || tiles->empty())
        FO_API_RETURN(0);

    for (auto& tile : *tiles)
    {
        if (tile.Layer == layer)
            FO_API_RETURN(tile.Anim->NameHash);
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_ARR_MARSHAL(string, fnames))
{
    for (size_t i = 0; i < fnames.size(); i++)
        _client->Preload3dFiles.push_back(fnames[i]);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(WaitPing, FO_API_RET(void))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    _client->WaitPing();
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param fontIndex ...
 * @param fontFname ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(LoadFont, FO_API_RET(bool), FO_API_ARG(int, fontIndex), FO_API_ARG(string, fontFname))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, fontIndex) FO_API_ARG_MARSHAL(string, fontFname))
{
    _client->SprMngr.PushAtlasType(AtlasType::Static);
    bool result;
    if (fontFname.length() > 0 && fontFname[0] == '*')
        result = _client->SprMngr.LoadFontFO(fontIndex, fontFname.c_str() + 1, false, false);
    else
        result = _client->SprMngr.LoadFontBMF(fontIndex, fontFname.c_str());
    if (result && !_client->SprMngr.IsAccumulateAtlasActive())
        _client->SprMngr.BuildFonts();
    _client->SprMngr.PopAtlasType();
    FO_API_RETURN(result);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param font ...
 * @param color ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SetDefaultFont, FO_API_RET(void), FO_API_ARG(int, font), FO_API_ARG(uint, color))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
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
FO_API_GLOBAL_CLIENT_FUNC(SetEffect, FO_API_RET(bool), FO_API_ARG(int, effectType), FO_API_ARG(int, effectSubtype),
    FO_API_ARG(string, effectName), FO_API_ARG(string, effectDefines))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, effectType) FO_API_ARG_MARSHAL(int, effectSubtype)
        FO_API_ARG_MARSHAL(string, effectName) FO_API_ARG_MARSHAL(string, effectDefines))
{
// Effect types
#define EFFECT_2D_GENERIC (0x00000001) // Subtype can be item id, zero for all items
#define EFFECT_2D_CRITTER (0x00000002) // Subtype can be critter id, zero for all critters
#define EFFECT_2D_TILE (0x00000004)
#define EFFECT_2D_ROOF (0x00000008)
#define EFFECT_2D_RAIN (0x00000010)
#define EFFECT_3D_SKINNED (0x00000400)
#define EFFECT_INTERFACE_BASE (0x00001000)
#define EFFECT_INTERFACE_CONTOUR (0x00002000)
#define EFFECT_FONT (0x00010000) // Subtype is FONT_*, -1 default for all fonts
#define EFFECT_PRIMITIVE_GENERIC (0x00100000)
#define EFFECT_PRIMITIVE_LIGHT (0x00200000)
#define EFFECT_PRIMITIVE_FOG (0x00400000)
#define EFFECT_FLUSH_RENDER_TARGET (0x01000000)
#define EFFECT_FLUSH_RENDER_TARGET_MS (0x02000000) // Multisample
#define EFFECT_FLUSH_PRIMITIVE (0x04000000)
#define EFFECT_FLUSH_MAP (0x08000000)
#define EFFECT_FLUSH_LIGHT (0x10000000)
#define EFFECT_FLUSH_FOG (0x20000000)
#define EFFECT_OFFSCREEN (0x40000000)

    RenderEffect* effect = nullptr;
    if (!effectName.empty())
    {
        effect = _client->EffectMngr.LoadEffect(effectName, effectDefines);
        if (!effect)
            throw ScriptException("Effect not found or have some errors, see log file");
    }

    if (effectType & EFFECT_2D_GENERIC && effectSubtype != 0)
    {
        ItemHexView* item = _client->GetItem((uint)effectSubtype);
        if (item)
            item->DrawEffect = (effect ? effect : _client->EffectMngr.Effects.Generic);
    }
    if (effectType & EFFECT_2D_CRITTER && effectSubtype != 0)
    {
        CritterView* cr = _client->GetCritter((uint)effectSubtype);
        if (cr)
            cr->DrawEffect = (effect ? effect : _client->EffectMngr.Effects.Critter);
    }

    if (effectType & EFFECT_2D_GENERIC && effectSubtype == 0)
        _client->EffectMngr.Effects.Generic = (effect ? effect : _client->EffectMngr.Effects.GenericDefault);
    if (effectType & EFFECT_2D_CRITTER && effectSubtype == 0)
        _client->EffectMngr.Effects.Critter = (effect ? effect : _client->EffectMngr.Effects.CritterDefault);
    if (effectType & EFFECT_2D_TILE)
        _client->EffectMngr.Effects.Tile = (effect ? effect : _client->EffectMngr.Effects.TileDefault);
    if (effectType & EFFECT_2D_ROOF)
        _client->EffectMngr.Effects.Roof = (effect ? effect : _client->EffectMngr.Effects.RoofDefault);
    if (effectType & EFFECT_2D_RAIN)
        _client->EffectMngr.Effects.Rain = (effect ? effect : _client->EffectMngr.Effects.RainDefault);

    if (effectType & EFFECT_3D_SKINNED)
        _client->EffectMngr.Effects.Skinned3d = (effect ? effect : _client->EffectMngr.Effects.Skinned3dDefault);

    if (effectType & EFFECT_INTERFACE_BASE)
        _client->EffectMngr.Effects.Iface = (effect ? effect : _client->EffectMngr.Effects.IfaceDefault);
    if (effectType & EFFECT_INTERFACE_CONTOUR)
        _client->EffectMngr.Effects.Contour = (effect ? effect : _client->EffectMngr.Effects.ContourDefault);

    if (effectType & EFFECT_FONT && effectSubtype == -1)
        _client->EffectMngr.Effects.Font = (effect ? effect : _client->EffectMngr.Effects.ContourDefault);
    if (effectType & EFFECT_FONT && effectSubtype >= 0)
        _client->SprMngr.SetFontEffect(effectSubtype, effect);

    if (effectType & EFFECT_PRIMITIVE_GENERIC)
        _client->EffectMngr.Effects.Primitive = (effect ? effect : _client->EffectMngr.Effects.PrimitiveDefault);
    if (effectType & EFFECT_PRIMITIVE_LIGHT)
        _client->EffectMngr.Effects.Light = (effect ? effect : _client->EffectMngr.Effects.LightDefault);
    if (effectType & EFFECT_PRIMITIVE_FOG)
        _client->EffectMngr.Effects.Fog = (effect ? effect : _client->EffectMngr.Effects.FogDefault);

    if (effectType & EFFECT_FLUSH_RENDER_TARGET)
        _client->EffectMngr.Effects.FlushRenderTarget =
            (effect ? effect : _client->EffectMngr.Effects.FlushRenderTargetDefault);
    if (effectType & EFFECT_FLUSH_RENDER_TARGET_MS)
        _client->EffectMngr.Effects.FlushRenderTargetMS =
            (effect ? effect : _client->EffectMngr.Effects.FlushRenderTargetMSDefault);
    if (effectType & EFFECT_FLUSH_PRIMITIVE)
        _client->EffectMngr.Effects.FlushPrimitive =
            (effect ? effect : _client->EffectMngr.Effects.FlushPrimitiveDefault);
    if (effectType & EFFECT_FLUSH_MAP)
        _client->EffectMngr.Effects.FlushMap = (effect ? effect : _client->EffectMngr.Effects.FlushMapDefault);
    if (effectType & EFFECT_FLUSH_LIGHT)
        _client->EffectMngr.Effects.FlushLight = (effect ? effect : _client->EffectMngr.Effects.FlushLightDefault);
    if (effectType & EFFECT_FLUSH_FOG)
        _client->EffectMngr.Effects.FlushFog = (effect ? effect : _client->EffectMngr.Effects.FlushFogDefault);

    if (effectType & EFFECT_OFFSCREEN)
    {
        if (effectSubtype < 0)
            throw ScriptException("Negative effect subtype");

        _client->OffscreenEffects.resize(effectSubtype + 1);
        _client->OffscreenEffects[effectSubtype] = effect;
    }

    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param onlyTiles ...
 * @param onlyRoof ...
 * @param onlyLight ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(
    RefreshMap, FO_API_RET(void), FO_API_ARG(bool, onlyTiles), FO_API_ARG(bool, onlyRoof), FO_API_ARG(bool, onlyLight))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(
    FO_API_ARG_MARSHAL(bool, onlyTiles) FO_API_ARG_MARSHAL(bool, onlyRoof) FO_API_ARG_MARSHAL(bool, onlyLight))
{
    if (_client->HexMngr.IsMapLoaded())
    {
        if (onlyTiles)
            _client->HexMngr.RebuildTiles();
        else if (onlyRoof)
            _client->HexMngr.RebuildRoof();
        else if (onlyLight)
            _client->HexMngr.RebuildLight();
        else
            _client->HexMngr.RefreshMap();
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
FO_API_GLOBAL_CLIENT_FUNC(MouseClick, FO_API_RET(void), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, button))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, button))
{
    /*App::Input::PushEvent({InputEvent::MouseDown({(MouseButton)button})});

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
FO_API_GLOBAL_CLIENT_FUNC(KeyboardPress, FO_API_RET(void), FO_API_ARG(uchar, key1), FO_API_ARG(uchar, key2),
    FO_API_ARG(string, key1Text), FO_API_ARG(string, key2Text))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, key1) FO_API_ARG_MARSHAL(uchar, key2) FO_API_ARG_MARSHAL(string, key1Text)
        FO_API_ARG_MARSHAL(string, key2Text))
{
    if (!key1 && !key2)
        FO_API_RETURN_VOID();

    if (key1)
        _client->ProcessInputEvent(InputEvent::KeyDown({(KeyCode)key1, key1Text}));

    if (key2)
    {
        _client->ProcessInputEvent(InputEvent::KeyDown({(KeyCode)key2, key2Text}));
        _client->ProcessInputEvent(InputEvent::KeyUp({(KeyCode)key2}));
    }

    if (key1)
        _client->ProcessInputEvent(InputEvent::KeyUp({(KeyCode)key1}));
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param fallAnimName ...
 * @param dropAnimName ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(
    SetRainAnimation, FO_API_RET(void), FO_API_ARG(string, fallAnimName), FO_API_ARG(string, dropAnimName))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, fallAnimName) FO_API_ARG_MARSHAL(string, dropAnimName))
{
    _client->HexMngr.SetRainAnimation(
        !fallAnimName.empty() ? fallAnimName.c_str() : nullptr, !dropAnimName.empty() ? dropAnimName.c_str() : nullptr);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param targetZoom ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(ChangeZoom, FO_API_RET(void), FO_API_ARG(float, targetZoom))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(float, targetZoom))
{
    if (targetZoom == _client->Settings.SpritesZoom)
        FO_API_RETURN_VOID();

    float init_zoom = _client->Settings.SpritesZoom;
    if (targetZoom == 1.0f)
    {
        _client->HexMngr.ChangeZoom(0);
    }
    else if (targetZoom > _client->Settings.SpritesZoom)
    {
        while (targetZoom > _client->Settings.SpritesZoom)
        {
            float old_zoom = _client->Settings.SpritesZoom;
            _client->HexMngr.ChangeZoom(1);
            if (_client->Settings.SpritesZoom == old_zoom)
                break;
        }
    }
    else if (targetZoom < _client->Settings.SpritesZoom)
    {
        while (targetZoom < _client->Settings.SpritesZoom)
        {
            float old_zoom = _client->Settings.SpritesZoom;
            _client->HexMngr.ChangeZoom(-1);
            if (_client->Settings.SpritesZoom == old_zoom)
                break;
        }
    }

    if (init_zoom != _client->Settings.SpritesZoom)
        _client->RebuildLookBorders = true;
}
FO_API_EPILOG()
#endif

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
FO_API_GLOBAL_CLIENT_FUNC(GetTime, FO_API_RET(void), FO_API_ARG_REF(ushort, year), FO_API_ARG_REF(ushort, month),
    FO_API_ARG_REF(ushort, day), FO_API_ARG_REF(ushort, dayOfWeek), FO_API_ARG_REF(ushort, hour),
    FO_API_ARG_REF(ushort, minute), FO_API_ARG_REF(ushort, second), FO_API_ARG_REF(ushort, milliseconds))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, year) FO_API_ARG_REF_MARSHAL(ushort, month)
        FO_API_ARG_REF_MARSHAL(ushort, day) FO_API_ARG_REF_MARSHAL(ushort, dayOfWeek)
            FO_API_ARG_REF_MARSHAL(ushort, hour) FO_API_ARG_REF_MARSHAL(ushort, minute)
                FO_API_ARG_REF_MARSHAL(ushort, second) FO_API_ARG_REF_MARSHAL(ushort, milliseconds))
{
    DateTimeStamp dt;
    Timer::GetCurrentDateTime(dt);
    year = dt.Year;
    month = dt.Month;
    dayOfWeek = dt.DayOfWeek;
    day = dt.Day;
    hour = dt.Hour;
    minute = dt.Minute;
    second = dt.Second;
    milliseconds = dt.Milliseconds;
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param index ...
 * @param enableSend ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(AllowSlot, FO_API_RET(void), FO_API_ARG(uchar, index), FO_API_ARG(bool, enableSend))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, index) FO_API_ARG_MARSHAL(bool, enableSend))
{
    CritterView::SlotEnabled[index] = true;
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param datName ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(AddDataSource, FO_API_RET(void), FO_API_ARG(string, datName))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, datName))
{
    _client->FileMngr.AddDataSource(datName, true);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param sprName ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(LoadSprite, FO_API_RET(uint), FO_API_ARG(string, sprName))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
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
FO_API_GLOBAL_CLIENT_FUNC(LoadSpriteHash, FO_API_RET(uint), FO_API_ARG(uint, nameHash))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, nameHash))
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex))
{
    AnyFrames* anim = _client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= (int)anim->CntFrm)
        FO_API_RETURN(0);
    SpriteInfo* si = _client->SprMngr.GetSpriteInfo(frameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(frameIndex));
    if (!si)
        FO_API_RETURN(0);
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex))
{
    AnyFrames* anim = _client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= (int)anim->CntFrm)
        FO_API_RETURN(0);
    SpriteInfo* si = _client->SprMngr.GetSpriteInfo(frameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(frameIndex));
    if (!si)
        FO_API_RETURN(0);
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId))
{
    AnyFrames* anim = _client->AnimGetFrames(sprId);
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId))
{
    AnyFrames* anim = _client->AnimGetFrames(sprId);
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
FO_API_GLOBAL_CLIENT_FUNC(GetPixelColor, FO_API_RET(uint), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex),
    FO_API_ARG(int, x), FO_API_ARG(int, y))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex) FO_API_ARG_MARSHAL(int, x)
        FO_API_ARG_MARSHAL(int, y))
{
    if (!sprId)
        FO_API_RETURN(0);

    AnyFrames* anim = _client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= (int)anim->CntFrm)
        FO_API_RETURN(0);

    uint spr_id_ = (frameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(frameIndex));
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
FO_API_GLOBAL_CLIENT_FUNC(GetTextInfo, FO_API_RET(void), FO_API_ARG(string, text), FO_API_ARG(int, w),
    FO_API_ARG(int, h), FO_API_ARG(int, font), FO_API_ARG(int, flags), FO_API_ARG_REF(int, tw), FO_API_ARG_REF(int, th),
    FO_API_ARG_REF(int, lines))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h)
        FO_API_ARG_MARSHAL(int, font) FO_API_ARG_MARSHAL(int, flags) FO_API_ARG_REF_MARSHAL(int, tw)
            FO_API_ARG_REF_MARSHAL(int, th) FO_API_ARG_REF_MARSHAL(int, lines))
{
    _client->SprMngr.GetTextInfo(w, h, text, font, flags, tw, th, lines);
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
FO_API_GLOBAL_CLIENT_FUNC(DrawSprite, FO_API_RET(void), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex),
    FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(uint, color), FO_API_ARG(bool, offs))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex) FO_API_ARG_MARSHAL(int, x)
        FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(bool, offs))
{
    if (!_client->CanDrawInScripts)
        throw ScriptException("You can use this function only in RenderIface event");
    if (!sprId)
        FO_API_RETURN_VOID();

    AnyFrames* anim = _client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= (int)anim->CntFrm)
        FO_API_RETURN_VOID();

    uint spr_id_ = (frameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(frameIndex));
    if (offs)
    {
        SpriteInfo* si = _client->SprMngr.GetSpriteInfo(spr_id_);
        if (!si)
            FO_API_RETURN_VOID();
        x += -si->Width / 2 + si->OffsX;
        y += -si->Height + si->OffsY;
    }

    _client->SprMngr.DrawSprite(spr_id_, x, y, COLOR_SCRIPT_SPRITE(color));
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
FO_API_GLOBAL_CLIENT_FUNC(DrawSpriteSize, FO_API_RET(void), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex),
    FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(bool, zoom),
    FO_API_ARG(uint, color), FO_API_ARG(bool, offs))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex) FO_API_ARG_MARSHAL(int, x)
        FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h) FO_API_ARG_MARSHAL(bool, zoom)
            FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(bool, offs))
{
    if (!_client->CanDrawInScripts)
        throw ScriptException("You can use this function only in RenderIface event");
    if (!sprId)
        FO_API_RETURN_VOID();

    AnyFrames* anim = _client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= (int)anim->CntFrm)
        FO_API_RETURN_VOID();

    uint spr_id_ = (frameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(frameIndex));
    if (offs)
    {
        SpriteInfo* si = _client->SprMngr.GetSpriteInfo(spr_id_);
        if (!si)
            FO_API_RETURN_VOID();
        x += si->OffsX;
        y += si->OffsY;
    }

    _client->SprMngr.DrawSpriteSizeExt(spr_id_, x, y, w, h, zoom, true, true, COLOR_SCRIPT_SPRITE(color));
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
FO_API_GLOBAL_CLIENT_FUNC(DrawSpritePattern, FO_API_RET(void), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex),
    FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(int, sprWidth),
    FO_API_ARG(int, sprHeight), FO_API_ARG(uint, color))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex) FO_API_ARG_MARSHAL(int, x)
        FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h)
            FO_API_ARG_MARSHAL(int, sprWidth) FO_API_ARG_MARSHAL(int, sprHeight) FO_API_ARG_MARSHAL(uint, color))
{
    if (!_client->CanDrawInScripts)
        throw ScriptException("You can use this function only in RenderIface event");
    if (!sprId)
        FO_API_RETURN_VOID();

    AnyFrames* anim = _client->AnimGetFrames(sprId);
    if (!anim || frameIndex >= (int)anim->CntFrm)
        FO_API_RETURN_VOID();

    _client->SprMngr.DrawSpritePattern(frameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(frameIndex), x, y, w, h,
        sprWidth, sprHeight, COLOR_SCRIPT_SPRITE(color));
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
FO_API_GLOBAL_CLIENT_FUNC(DrawText, FO_API_RET(void), FO_API_ARG(string, text), FO_API_ARG(int, x), FO_API_ARG(int, y),
    FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(uint, color), FO_API_ARG(int, font), FO_API_ARG(int, flags))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y)
        FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h) FO_API_ARG_MARSHAL(uint, color)
            FO_API_ARG_MARSHAL(int, font) FO_API_ARG_MARSHAL(int, flags))
{
    if (!_client->CanDrawInScripts)
        throw ScriptException("You can use this function only in RenderIface event");
    if (text.length() == 0)
        FO_API_RETURN_VOID();

    if (w < 0)
        w = -w, x -= w;
    if (h < 0)
        h = -h, y -= h;

    Rect r = Rect(x, y, x + w, y + h);
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, primitiveType) FO_API_ARG_ARR_MARSHAL(int, data))
{
    if (!_client->CanDrawInScripts)
        throw ScriptException("You can use this function only in RenderIface event");
    if (data.empty())
        FO_API_RETURN_VOID();

    RenderPrimitiveType prim;
    switch (primitiveType)
    {
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

    PointVec points;
    size_t size = data.size() / 3;
    points.resize(size);

    for (size_t i = 0; i < size; i++)
    {
        PrepPoint& pp = points[i];
        pp.PointX = data[i * 3];
        pp.PointY = data[i * 3 + 1];
        pp.PointColor = data[i * 3 + 2];
        pp.PointOffsX = nullptr;
        pp.PointOffsY = nullptr;
    }

    _client->SprMngr.DrawPoints(points, prim);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param mapSpr ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(DrawMapSprite, FO_API_RET(void), FO_API_ARG_OBJ(MapSprite, mapSpr))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(MapSprite, mapSpr))
{
    if (!mapSpr)
        throw ScriptException("Map sprite arg is null");

    if (!_client->HexMngr.IsMapLoaded())
        FO_API_RETURN_VOID();
    if (mapSpr->HexX >= _client->HexMngr.GetWidth() || mapSpr->HexY >= _client->HexMngr.GetHeight())
        FO_API_RETURN_VOID();
    if (!_client->HexMngr.IsHexToDraw(mapSpr->HexX, mapSpr->HexY))
        FO_API_RETURN_VOID();

    AnyFrames* anim = _client->AnimGetFrames(mapSpr->SprId);
    if (!anim || mapSpr->FrameIndex >= (int)anim->CntFrm)
        FO_API_RETURN_VOID();

    uint color = mapSpr->Color;
    bool is_flat = mapSpr->IsFlat;
    bool no_light = mapSpr->NoLight;
    int draw_order = mapSpr->DrawOrder;
    int draw_order_hy_offset = mapSpr->DrawOrderHyOffset;
    int corner = mapSpr->Corner;
    bool disable_egg = mapSpr->DisableEgg;
    uint contour_color = mapSpr->ContourColor;

    if (mapSpr->ProtoId)
    {
        ProtoItem* proto_item = _client->ProtoMngr.GetProtoItem(mapSpr->ProtoId);
        if (!proto_item)
            FO_API_RETURN_VOID();

        color = (proto_item->GetIsColorize() ? proto_item->GetLightColor() : 0);
        is_flat = proto_item->GetIsFlat();
        bool is_item = !proto_item->IsAnyScenery();
        no_light = (is_flat && !is_item);
        draw_order = (is_flat ? (is_item ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY) :
                                (is_item ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY));
        draw_order_hy_offset = proto_item->GetDrawOrderOffsetHexY();
        corner = proto_item->GetCorner();
        disable_egg = proto_item->GetDisableEgg();
        contour_color = (proto_item->GetIsBadItem() ? COLOR_RGB(255, 0, 0) : 0);
    }

    Field& f = _client->HexMngr.GetField(mapSpr->HexX, mapSpr->HexY);
    Sprites& tree = _client->HexMngr.GetDrawTree();
    Sprite& spr = tree.InsertSprite(draw_order, mapSpr->HexX, mapSpr->HexY + draw_order_hy_offset, 0,
        (_client->Settings.MapHexWidth / 2) + mapSpr->OffsX, (_client->Settings.MapHexHeight / 2) + mapSpr->OffsY,
        &f.ScrX, &f.ScrY, mapSpr->FrameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(mapSpr->FrameIndex), nullptr,
        mapSpr->IsTweakOffs ? &mapSpr->TweakOffsX : nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsY : nullptr,
        mapSpr->IsTweakAlpha ? &mapSpr->TweakAlpha : nullptr, nullptr, &mapSpr->Valid);

    spr.MapSpr = mapSpr;
    mapSpr->AddRef();

    if (!no_light)
        spr.SetLight(
            corner, _client->HexMngr.GetLightHex(0, 0), _client->HexMngr.GetWidth(), _client->HexMngr.GetHeight());

    if (!is_flat && !disable_egg)
    {
        int egg_type = 0;
        switch (corner)
        {
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

    if (color)
    {
        spr.SetColor(color & 0xFFFFFF);
        spr.SetFixedAlpha(color >> 24);
    }

    if (contour_color)
        spr.SetContour(CONTOUR_CUSTOM, contour_color);
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
FO_API_GLOBAL_CLIENT_FUNC(DrawCritter2d, FO_API_RET(void), FO_API_ARG(hash, modelName), FO_API_ARG(uint, anim1),
    FO_API_ARG(uint, anim2), FO_API_ARG(uchar, dir), FO_API_ARG(int, l), FO_API_ARG(int, t), FO_API_ARG(int, r),
    FO_API_ARG(int, b), FO_API_ARG(bool, scratch), FO_API_ARG(bool, center), FO_API_ARG(uint, color))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, modelName) FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2)
        FO_API_ARG_MARSHAL(uchar, dir) FO_API_ARG_MARSHAL(int, l) FO_API_ARG_MARSHAL(int, t) FO_API_ARG_MARSHAL(int, r)
            FO_API_ARG_MARSHAL(int, b) FO_API_ARG_MARSHAL(bool, scratch) FO_API_ARG_MARSHAL(bool, center)
                FO_API_ARG_MARSHAL(uint, color))
{
    AnyFrames* anim = _client->ResMngr.GetCrit2dAnim(modelName, anim1, anim2, dir);
    if (anim)
        _client->SprMngr.DrawSpriteSize(anim->Ind[0], l, t, r - l, b - t, scratch, center, COLOR_SCRIPT_SPRITE(color));
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
static Animation3dVec DrawCritter3dAnim;
static UIntVec DrawCritter3dCrType;
static BoolVec DrawCritter3dFailToLoad;
static int DrawCritter3dLayers[LAYERS3D_COUNT];
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
FO_API_GLOBAL_CLIENT_FUNC(DrawCritter3d, FO_API_RET(void), FO_API_ARG(uint, instance), FO_API_ARG(hash, modelName),
    FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2), FO_API_ARG_ARR(int, layers), FO_API_ARG_ARR(float, position),
    FO_API_ARG(uint, color))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, instance) FO_API_ARG_MARSHAL(hash, modelName) FO_API_ARG_MARSHAL(uint, anim1)
        FO_API_ARG_MARSHAL(uint, anim2) FO_API_ARG_ARR_MARSHAL(int, layers) FO_API_ARG_ARR_MARSHAL(float, position)
            FO_API_ARG_MARSHAL(uint, color))
{
    // x y
    // rx ry rz
    // sx sy sz
    // speed
    // scissor l t r b
    if (instance >= DrawCritter3dAnim.size())
    {
        DrawCritter3dAnim.resize(instance + 1);
        DrawCritter3dCrType.resize(instance + 1);
        DrawCritter3dFailToLoad.resize(instance + 1);
    }

    if (DrawCritter3dFailToLoad[instance] && DrawCritter3dCrType[instance] == modelName)
        FO_API_RETURN_VOID();

    Animation3d*& anim3d = DrawCritter3dAnim[instance];
    if (!anim3d || DrawCritter3dCrType[instance] != modelName)
    {
        if (anim3d)
            _client->SprMngr.FreePure3dAnimation(anim3d);
        _client->SprMngr.PushAtlasType(AtlasType::Dynamic);
        anim3d = _client->SprMngr.LoadPure3dAnimation(_str().parseHash(modelName), false);
        _client->SprMngr.PopAtlasType();
        DrawCritter3dCrType[instance] = modelName;
        DrawCritter3dFailToLoad[instance] = false;

        if (!anim3d)
        {
            DrawCritter3dFailToLoad[instance] = true;
            FO_API_RETURN_VOID();
        }
        anim3d->EnableShadow(false);
        anim3d->SetTimer(false);
    }

    uint count = (uint)position.size();
    float x = (count > 0 ? position[0] : 0.0f);
    float y = (count > 1 ? position[1] : 0.0f);
    float rx = (count > 2 ? position[2] : 0.0f);
    float ry = (count > 3 ? position[3] : 0.0f);
    float rz = (count > 4 ? position[4] : 0.0f);
    float sx = (count > 5 ? position[5] : 1.0f);
    float sy = (count > 6 ? position[6] : 1.0f);
    float sz = (count > 7 ? position[7] : 1.0f);
    float speed = (count > 8 ? position[8] : 1.0f);
    float period = (count > 9 ? position[9] : 0.0f);
    float stl = (count > 10 ? position[10] : 0.0f);
    float stt = (count > 11 ? position[11] : 0.0f);
    float str = (count > 12 ? position[12] : 0.0f);
    float stb = (count > 13 ? position[13] : 0.0f);
    if (count > 13)
        _client->SprMngr.PushScissor((int)stl, (int)stt, (int)str, (int)stb);

    memzero(DrawCritter3dLayers, sizeof(DrawCritter3dLayers));
    for (uint i = 0, j = (uint)layers.size(); i < j && i < LAYERS3D_COUNT; i++)
        DrawCritter3dLayers[i] = layers[i];

    anim3d->SetDirAngle(0);
    anim3d->SetRotation(rx * PI_VALUE / 180.0f, ry * PI_VALUE / 180.0f, rz * PI_VALUE / 180.0f);
    anim3d->SetScale(sx, sy, sz);
    anim3d->SetSpeed(speed);
    anim3d->SetAnimation(
        anim1, anim2, DrawCritter3dLayers, ANIMATION_PERIOD((int)(period * 100.0f)) | ANIMATION_NO_SMOOTH);

    _client->SprMngr.Draw3d((int)x, (int)y, anim3d, COLOR_SCRIPT_SPRITE(color));

    if (count > 13)
        _client->SprMngr.PopScissor();
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
FO_API_GLOBAL_CLIENT_FUNC(
    PushDrawScissor, FO_API_RET(void), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(
    FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h))
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, forceClear))
{
    if (!_client->CanDrawInScripts)
        throw ScriptException("You can use this function only in RenderIface event");

    if (_client->OffscreenSurfaces.empty())
    {
        RenderTarget* rt = _client->SprMngr.CreateRenderTarget(false, false, true, 0, 0, false);
        if (!rt)
            throw ScriptException("Can't create offscreen surface");

        _client->OffscreenSurfaces.push_back(rt);
    }

    RenderTarget* rt = _client->OffscreenSurfaces.back();
    _client->OffscreenSurfaces.pop_back();
    _client->ActiveOffscreenSurfaces.push_back(rt);

    _client->SprMngr.PushRenderTarget(rt);

    auto it = std::find(_client->DirtyOffscreenSurfaces.begin(), _client->DirtyOffscreenSurfaces.end(), rt);
    if (it != _client->DirtyOffscreenSurfaces.end() || forceClear)
    {
        if (it != _client->DirtyOffscreenSurfaces.end())
            _client->DirtyOffscreenSurfaces.erase(it);

        _client->SprMngr.ClearCurrentRenderTarget(0);
    }

    if (std::find(_client->PreDirtyOffscreenSurfaces.begin(), _client->PreDirtyOffscreenSurfaces.end(), rt) ==
        _client->PreDirtyOffscreenSurfaces.end())
        _client->PreDirtyOffscreenSurfaces.push_back(rt);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param effectSubtype ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(PresentOffscreenSurface, FO_API_RET(void), FO_API_ARG(int, effectSubtype))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, effectSubtype))
{
    if (!_client->CanDrawInScripts)
        throw ScriptException("You can use this function only in RenderIface event");
    if (_client->ActiveOffscreenSurfaces.empty())
        throw ScriptException("No active offscreen surfaces");

    RenderTarget* rt = _client->ActiveOffscreenSurfaces.back();
    _client->ActiveOffscreenSurfaces.pop_back();
    _client->OffscreenSurfaces.push_back(rt);

    _client->SprMngr.PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= (int)_client->OffscreenEffects.size() ||
        !_client->OffscreenEffects[effectSubtype])
        throw ScriptException("Invalid effect subtype");

    rt->DrawEffect = _client->OffscreenEffects[effectSubtype];

    _client->SprMngr.DrawRenderTarget(rt, true);
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
FO_API_GLOBAL_CLIENT_FUNC(PresentOffscreenSurfaceExt, FO_API_RET(void), FO_API_ARG(int, effectSubtype),
    FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, effectSubtype) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y)
        FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h))
{
    if (!_client->CanDrawInScripts)
        throw ScriptException("You can use this function only in RenderIface event");
    if (_client->ActiveOffscreenSurfaces.empty())
        throw ScriptException("No active offscreen surfaces");

    RenderTarget* rt = _client->ActiveOffscreenSurfaces.back();
    _client->ActiveOffscreenSurfaces.pop_back();
    _client->OffscreenSurfaces.push_back(rt);

    _client->SprMngr.PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= (int)_client->OffscreenEffects.size() ||
        !_client->OffscreenEffects[effectSubtype])
        throw ScriptException("Invalid effect subtype");

    rt->DrawEffect = _client->OffscreenEffects[effectSubtype];

    Rect from(CLAMP(x, 0, _client->Settings.ScreenWidth), CLAMP(y, 0, _client->Settings.ScreenHeight),
        CLAMP(x + w, 0, _client->Settings.ScreenWidth), CLAMP(y + h, 0, _client->Settings.ScreenHeight));
    Rect to = from;
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
FO_API_GLOBAL_CLIENT_FUNC(PresentOffscreenSurfaceExt2, FO_API_RET(void), FO_API_ARG(int, effectSubtype),
    FO_API_ARG(int, fromX), FO_API_ARG(int, fromY), FO_API_ARG(int, fromW), FO_API_ARG(int, fromH),
    FO_API_ARG(int, toX), FO_API_ARG(int, toY), FO_API_ARG(int, toW), FO_API_ARG(int, toH))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, effectSubtype) FO_API_ARG_MARSHAL(int, fromX) FO_API_ARG_MARSHAL(int, fromY)
        FO_API_ARG_MARSHAL(int, fromW) FO_API_ARG_MARSHAL(int, fromH) FO_API_ARG_MARSHAL(int, toX)
            FO_API_ARG_MARSHAL(int, toY) FO_API_ARG_MARSHAL(int, toW) FO_API_ARG_MARSHAL(int, toH))
{
    if (!_client->CanDrawInScripts)
        throw ScriptException("You can use this function only in RenderIface event");
    if (_client->ActiveOffscreenSurfaces.empty())
        throw ScriptException("No active offscreen surfaces");

    RenderTarget* rt = _client->ActiveOffscreenSurfaces.back();
    _client->ActiveOffscreenSurfaces.pop_back();
    _client->OffscreenSurfaces.push_back(rt);

    _client->SprMngr.PopRenderTarget();

    if (effectSubtype < 0 || effectSubtype >= (int)_client->OffscreenEffects.size() ||
        !_client->OffscreenEffects[effectSubtype])
        throw ScriptException("Invalid effect subtype");

    rt->DrawEffect = _client->OffscreenEffects[effectSubtype];

    Rect from(CLAMP(fromX, 0, _client->Settings.ScreenWidth), CLAMP(fromY, 0, _client->Settings.ScreenHeight),
        CLAMP(fromX + fromW, 0, _client->Settings.ScreenWidth),
        CLAMP(fromY + fromH, 0, _client->Settings.ScreenHeight));
    Rect to(CLAMP(toX, 0, _client->Settings.ScreenWidth), CLAMP(toY, 0, _client->Settings.ScreenHeight),
        CLAMP(toX + toW, 0, _client->Settings.ScreenWidth), CLAMP(toY + toH, 0, _client->Settings.ScreenHeight));
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, screen) FO_API_ARG_DICT_MARSHAL(string, int, data))
{
    if (screen >= SCREEN_LOGIN && screen <= SCREEN_WAIT)
        _client->ShowMainScreen(screen, data);
    else if (screen != SCREEN_NONE)
        _client->ShowScreen(screen, data);
    else
        _client->HideScreen(screen);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param screen ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(HideScreen, FO_API_RET(void), FO_API_ARG(int, screen))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
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
FO_API_GLOBAL_CLIENT_FUNC(GetHexPos, FO_API_RET(bool), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy),
    FO_API_ARG_REF(int, x), FO_API_ARG_REF(int, y))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_REF_MARSHAL(int, x)
        FO_API_ARG_REF_MARSHAL(int, y))
{
    x = y = 0;
    if (_client->HexMngr.IsMapLoaded() && hx < _client->HexMngr.GetWidth() && hy < _client->HexMngr.GetHeight())
    {
        _client->HexMngr.GetHexCurrentPosition(hx, hy, x, y);
        x += _client->Settings.ScrOx + (_client->Settings.MapHexWidth / 2);
        y += _client->Settings.ScrOy + (_client->Settings.MapHexHeight / 2);
        x = (int)(x / _client->Settings.SpritesZoom);
        y = (int)(y / _client->Settings.SpritesZoom);
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
FO_API_GLOBAL_CLIENT_FUNC(GetMonitorHex, FO_API_RET(bool), FO_API_ARG(int, x), FO_API_ARG(int, y),
    FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_REF_MARSHAL(ushort, hx)
        FO_API_ARG_REF_MARSHAL(ushort, hy))
{
    int old_x = _client->Settings.MouseX;
    int old_y = _client->Settings.MouseY;
    _client->Settings.MouseX = x;
    _client->Settings.MouseY = y;
    ushort hx_ = 0, hy_ = 0;
    bool result = _client->HexMngr.GetHexPixel(x, y, hx_, hy_);
    _client->Settings.MouseX = old_x;
    _client->Settings.MouseY = old_y;
    if (result)
    {
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
FO_API_GLOBAL_CLIENT_FUNC(GetMonitorItem, FO_API_RET_OBJ(ItemView), FO_API_ARG(int, x), FO_API_ARG(int, y))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
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
FO_API_GLOBAL_CLIENT_FUNC(GetMonitorCritter, FO_API_RET_OBJ(CritterView), FO_API_ARG(int, x), FO_API_ARG(int, y))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
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
FO_API_GLOBAL_CLIENT_FUNC(GetMonitorEntity, FO_API_RET_OBJ(Entity), FO_API_ARG(int, x), FO_API_ARG(int, y))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y))
{
    ItemHexView* item;
    CritterView* cr;
    _client->HexMngr.GetSmthPixel(x, y, item, cr);
    FO_API_RETURN(item ? (Entity*)item : (Entity*)cr);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetMapWidth, FO_API_RET(ushort))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    if (!_client->HexMngr.IsMapLoaded())
        throw ScriptException("Map is not loaded");

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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG()
{
    if (!_client->HexMngr.IsMapLoaded())
        throw ScriptException("Map is not loaded");

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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (!_client->HexMngr.IsMapLoaded())
        throw ScriptException("Map is not loaded");
    if (hx >= _client->HexMngr.GetWidth() || hy >= _client->HexMngr.GetHeight())
        throw ScriptException("Invalid hex args");

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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (!_client->HexMngr.IsMapLoaded())
        throw ScriptException("Map is not loaded");
    if (hx >= _client->HexMngr.GetWidth() || hy >= _client->HexMngr.GetHeight())
        throw ScriptException("Invalid hex args");

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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
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
FO_API_GLOBAL_CLIENT_FUNC(SaveText, FO_API_RET(bool), FO_API_ARG(string, filePath), FO_API_ARG(string, text))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, filePath) FO_API_ARG_MARSHAL(string, text))
{
    DiskFile f = DiskFileSystem::OpenFile(_str(filePath).formatPath(), true);
    if (!f)
        FO_API_RETURN(false);

    if (text.length() > 0)
        f.Write(text.c_str(), (uint)text.length());
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param name ...
 * @param data ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SetCacheData, FO_API_RET(void), FO_API_ARG(string, name), FO_API_ARG_ARR(uchar, data))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name) FO_API_ARG_ARR_MARSHAL(uchar, data))
{
    _client->Cache.SetCache(name, data);
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
FO_API_GLOBAL_CLIENT_FUNC(SetCacheDataSize, FO_API_RET(void), FO_API_ARG(string, name), FO_API_ARG_ARR(uchar, data),
    FO_API_ARG(uint, dataSize))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name) FO_API_ARG_ARR_MARSHAL(uchar, data) FO_API_ARG_MARSHAL(uint, dataSize))
{
    data.resize(dataSize);
    _client->Cache.SetCache(name, data);
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
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name))
{
    UCharVec data;
    if (!_client->Cache.GetCache(name, data))
        FO_API_RETURN(UCharVec());
    FO_API_RETURN(data);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param name ...
 * @param str ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SetCacheDataStr, FO_API_RET(void), FO_API_ARG(string, name), FO_API_ARG(string, str))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name) FO_API_ARG_MARSHAL(string, str))
{
    _client->Cache.SetCache(name, str);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param name ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(GetCacheDataStr, FO_API_RET(string), FO_API_ARG(string, name))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name))
{
    FO_API_RETURN(_client->Cache.GetCache(name));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param name ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(IsCacheData, FO_API_RET(bool), FO_API_ARG(string, name))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name))
{
    FO_API_RETURN(_client->Cache.IsCache(name));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param name ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(EraseCacheData, FO_API_RET(void), FO_API_ARG(string, name))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name))
{
    _client->Cache.EraseCache(name);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param keyValues ...
 ******************************************************************************/
FO_API_GLOBAL_CLIENT_FUNC(SetUserConfig, FO_API_RET(void), FO_API_ARG_DICT(string, string, keyValues))
#ifdef FO_API_GLOBAL_CLIENT_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_DICT_MARSHAL(string, string, keyValues))
{
    OutputFile cfg_user = _client->FileMngr.WriteFile(CONFIG_NAME);
    for (auto& kv : keyValues)
        cfg_user.SetStr(_str("{} = {}\n", kv.first, kv.second));
    cfg_user.Save();
}
FO_API_EPILOG()
#endif
