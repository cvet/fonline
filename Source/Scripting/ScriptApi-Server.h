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

#ifdef FO_API_SERVER_IMPL
#include "PropertiesSerializator.h"
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param count ...
 * @param stackId ...
 * @return ...
 ******************************************************************************/
FO_API_ITEM_METHOD(AddItem, FO_API_RET_OBJ(Item), FO_API_ARG(hash, pid), FO_API_ARG(uint, count), FO_API_ARG(uint, stackId))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_MARSHAL(uint, count) FO_API_ARG_MARSHAL(uint, stackId))
{
    if (!_server->ProtoMngr.GetProtoItem(pid)) {
        throw ScriptException("Invalid proto '{}' arg.", _str().parseHash(pid));
    }

    FO_API_RETURN(_server->ItemMngr.AddItemContainer(_item, pid, count > 0 ? count : 1, stackId));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param stackId ...
 * @return ...
 ******************************************************************************/
FO_API_ITEM_METHOD(GetItems, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(uint, stackId))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, stackId))
{
    ItemVec items;
    _item->ContGetItems(items, stackId);

    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param func ...
 * @return ...
 ******************************************************************************/
FO_API_ITEM_METHOD(SetScript, FO_API_RET(void), FO_API_ARG_CALLBACK(Item, func))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_CALLBACK_MARSHAL(Item, func))
{
    if (func) {
        // if (!_item->SetScript(func, true))
        //    throw ScriptException("Script function not found");
    }
    else {
        _item->SetScriptId(0);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
FO_API_ITEM_METHOD(GetMapPos, FO_API_RET_OBJ(Map), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, hx) FO_API_ARG_REF_MARSHAL(ushort, hy))
{
    Map* map = nullptr;

    switch (_item->GetAccessory()) {
    case ITEM_ACCESSORY_CRITTER: {
        auto* cr = _server->CrMngr.GetCritter(_item->GetCritId());
        if (!cr) {
            throw ScriptException("Critter accessory, critter not found");
        }

        if (!cr->GetMapId()) {
            hx = cr->GetWorldX();
            hy = cr->GetWorldY();
            FO_API_RETURN(static_cast<Map*>(nullptr));
        }

        map = _server->MapMngr.GetMap(cr->GetMapId());
        if (!map) {
            throw ScriptException("Critter accessory, map not found");
        }

        hx = cr->GetHexX();
        hy = cr->GetHexY();
    } break;
    case ITEM_ACCESSORY_HEX: {
        map = _server->MapMngr.GetMap(_item->GetMapId());
        if (!map) {
            throw ScriptException("Hex accessory, map not found");
        }

        hx = _item->GetHexX();
        hy = _item->GetHexY();
    } break;
    case ITEM_ACCESSORY_CONTAINER: {
        if (_item->GetId() == _item->GetContainerId()) {
            throw ScriptException("Container accessory, crosslink");
        }

        auto* cont = _server->ItemMngr.GetItem(_item->GetContainerId());
        if (!cont) {
            throw ScriptException("Container accessory, container not found");
        }

        // FO_API_RETURN(Item_GetMapPosition(cont, hx, hy)); // Todo: fix ITEM_ACCESSORY_CONTAINER recursion
    } break;
    default:
        throw ScriptException("Unknown accessory");
    }

    FO_API_RETURN(map);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_ITEM_METHOD(ChangeProto, FO_API_RET(void), FO_API_ARG(hash, pid))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    const auto* proto_item = _server->ProtoMngr.GetProtoItem(pid);
    if (!proto_item) {
        throw ScriptException("Proto not found");
    }

    const auto* old_proto_item = _item->GetProtoItem();
    _item->SetProto(proto_item);

    if (_item->GetAccessory() == ITEM_ACCESSORY_CRITTER) {
        auto* cr = _server->CrMngr.GetCritter(_item->GetCritId());
        if (cr) {
            _item->SetProto(old_proto_item);
            cr->Send_EraseItem(_item);
            _item->SetProto(proto_item);
            cr->Send_AddItem(_item);
            cr->SendAA_MoveItem(_item, ACTION_REFRESH, 0);
        }
    }
    else if (_item->GetAccessory() == ITEM_ACCESSORY_HEX) {
        auto* map = _server->MapMngr.GetMap(_item->GetMapId());
        if (map) {
            const auto hx = _item->GetHexX();
            const auto hy = _item->GetHexY();
            _item->SetProto(old_proto_item);
            map->EraseItem(_item->GetId());
            _item->SetProto(proto_item);
            map->AddItem(_item, hx, hy);
        }
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param fromFrm ...
 * @param toFrm ...
 ******************************************************************************/
FO_API_ITEM_METHOD(Animate, FO_API_RET(void), FO_API_ARG(uchar, fromFrm), FO_API_ARG(uchar, toFrm))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, fromFrm) FO_API_ARG_MARSHAL(uchar, toFrm))
{
    switch (_item->GetAccessory()) {
    case ITEM_ACCESSORY_CRITTER: {
        // Critter* cr=CrMngr.GetCrit(_item->ACC_CRITTER.Id);
        // if(cr) cr->Send_AnimateItem(_item,from_frm,to_frm);
    } break;
    case ITEM_ACCESSORY_HEX: {
        auto* map = _server->MapMngr.GetMap(_item->GetMapId());
        if (map) {
            map->AnimateItem(_item, fromFrm, toFrm);
        }
    } break;
    case ITEM_ACCESSORY_CONTAINER:
        break;
    default:
        throw ScriptException("Unknown accessory");
    }
}
FO_API_EPILOG()
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @param staticItem ...
 * @param param ...
 * @return ...
 ******************************************************************************/
FO_API_ITEM_METHOD(CallStaticItemFunction, FO_API_RET(bool), FO_API_ARG_OBJ(Critter, cr), FO_API_ARG_OBJ(Item, staticItem), FO_API_ARG(int, param))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr) FO_API_ARG_OBJ_MARSHAL(Item, staticItem) FO_API_ARG_MARSHAL(int, param))
{
    if (!_item->SceneryScriptFunc) {
        FO_API_RETURN(false);
    }

    FO_API_RETURN(_item->SceneryScriptFunc(cr, _item, staticItem, param) && _item->SceneryScriptFunc.GetResult());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(IsPlayer, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critter->IsPlayer());
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(IsNpc, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critter->IsNpc());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetAccess, FO_API_RET(int))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    if (!_critter->IsPlayer()) {
        throw ScriptException("Critter is not player");
    }

    FO_API_RETURN((dynamic_cast<Client*>(_critter))->Access);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param access ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(SetAccess, FO_API_RET(bool), FO_API_ARG(int, access))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, access))
{
    if (!_critter->IsPlayer()) {
        throw ScriptException("Critter is not player");
    }
    if (access < ACCESS_CLIENT || access > ACCESS_ADMIN) {
        throw ScriptException("Wrong access type");
    }

    if (access == dynamic_cast<Client*>(_critter)->Access) {
        FO_API_RETURN(true);
    }

    string pass;
    const auto allow = _server->ScriptSys.PlayerGetAccessEvent(_critter, access, pass);
    if (allow) {
        dynamic_cast<Client*>(_critter)->Access = static_cast<uchar>(access);
    }

    FO_API_RETURN(allow);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetMap, FO_API_RET_OBJ(Map))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_server->MapMngr.GetMap(_critter->GetMapId()));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param direction ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(MoveToDir, FO_API_RET(bool), FO_API_ARG(uchar, direction))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, direction))
{
    auto* map = _server->MapMngr.GetMap(_critter->GetMapId());
    if (!map) {
        throw ScriptException("Critter is on global");
    }
    if (direction >= _server->Settings.MapDirCount) {
        throw ScriptException("Invalid direction arg");
    }

    auto hx = _critter->GetHexX();
    auto hy = _critter->GetHexY();
    _server->GeomHelper.MoveHexByDir(hx, hy, direction, map->GetWidth(), map->GetHeight());

    const auto move_flags = static_cast<ushort>(direction | BIN16(00000000, 00111000));
    const auto move = _server->Act_Move(_critter, hx, hy, move_flags);
    if (move) {
        _critter->Send_Move(_critter, move_flags);
    }

    FO_API_RETURN(move);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param dir ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(TransitToHex, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uchar, dir))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uchar, dir))
{
    if (_critter->LockMapTransfers) {
        throw ScriptException("Transfers locked");
    }
    auto* map = _server->MapMngr.GetMap(_critter->GetMapId());
    if (!map) {
        throw ScriptException("Critter is on global");
    }
    if (hx >= map->GetWidth() || hy >= map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    if (hx != _critter->GetHexX() || hy != _critter->GetHexY()) {
        if (dir < _server->Settings.MapDirCount && _critter->GetDir() != dir) {
            _critter->SetDir(dir);
        }
        if (!_server->MapMngr.Transit(_critter, map, hx, hy, _critter->GetDir(), 2, 0, true)) {
            throw ScriptException("Transit fail");
        }
    }
    else if (dir < _server->Settings.MapDirCount && _critter->GetDir() != dir) {
        _critter->SetDir(dir);
        _critter->Send_Dir(_critter);
        _critter->SendA_Dir();
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param map ...
 * @param hx ...
 * @param hy ...
 * @param dir ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(TransitToMapHex, FO_API_RET(void), FO_API_ARG_OBJ(Map, map), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uchar, dir))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Map, map) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uchar, dir))
{
    if (_critter->LockMapTransfers) {
        throw ScriptException("Transfers locked");
    }
    if (map == nullptr) {
        throw ScriptException("Map arg is null");
    }

    auto dir_ = dir;
    if (dir_ >= _server->Settings.MapDirCount) {
        dir_ = 0;
    }

    if (!_server->MapMngr.Transit(_critter, map, hx, hy, dir_, 2, 0, true)) {
        throw ScriptException("Transit to map hex fail");
    }

    // Todo: need attention!
    auto* loc = map->GetLocation();
    if (loc && GenericUtils::DistSqrt(_critter->GetWorldX(), _critter->GetWorldY(), loc->GetWorldX(), loc->GetWorldY()) > loc->GetRadius()) {
        _critter->SetWorldX(loc->GetWorldX());
        _critter->SetWorldY(loc->GetWorldY());
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 ******************************************************************************/
FO_API_CRITTER_METHOD(TransitToGlobal, FO_API_RET(void))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    if (_critter->LockMapTransfers) {
        throw ScriptException("Transfers locked");
    }

    if (_critter->GetMapId() && !_server->MapMngr.TransitToGlobal(_critter, 0, true)) {
        throw ScriptException("Transit to global failed");
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param group ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(TransitToGlobalWithGroup, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Critter, group))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Critter, group))
{
    if (_critter->LockMapTransfers) {
        throw ScriptException("Transfers locked");
    }
    if (!_critter->GetMapId()) {
        throw ScriptException("Critter already on global");
    }

    if (!_server->MapMngr.TransitToGlobal(_critter, 0, true)) {
        throw ScriptException("Transit to global failed");
    }

    for (auto* cr_ : group) {
        if (cr_ != nullptr && !cr_->IsDestroyed) {
            _server->MapMngr.TransitToGlobal(cr_, _critter->GetId(), true);
        }
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param leader ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(TransitToGlobalGroup, FO_API_RET(void), FO_API_ARG_OBJ(Critter, leader))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, leader))
{
    if (_critter->LockMapTransfers) {
        throw ScriptException("Transfers locked");
    }
    if (!_critter->GetMapId()) {
        throw ScriptException("Critter already on global");
    }
    if (leader == nullptr) {
        throw ScriptException("Leader arg not found");
    }

    if (leader->GlobalMapGroup == nullptr) {
        throw ScriptException("Leader is not on global map");
    }

    if (!_server->MapMngr.TransitToGlobal(_critter, leader->GetId(), true)) {
        throw ScriptException("Transit fail");
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(IsAlive, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critter->IsAlive());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(IsKnockout, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critter->IsKnockout());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(IsDead, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critter->IsDead());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(IsFree, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critter->IsFree() && !_critter->IsWait());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(IsBusy, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critter->IsBusy() || _critter->IsWait());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param ms ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(Wait, FO_API_RET(void), FO_API_ARG(uint, ms))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, ms))
{
    _critter->SetWait(ms);

    if (_critter->IsPlayer()) {
        auto* cl = dynamic_cast<Client*>(_critter);
        cl->SetBreakTime(ms);
        cl->Send_CustomCommand(_critter, OTHER_BREAK_TIME, ms);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 ******************************************************************************/
FO_API_CRITTER_METHOD(RefreshView, FO_API_RET(void))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    _server->MapMngr.ProcessVisibleCritters(_critter);
    _server->MapMngr.ProcessVisibleItems(_critter);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param map ...
 * @param look ...
 * @param hx ...
 * @param hy ...
 * @param dir ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(ViewMap, FO_API_RET(void), FO_API_ARG_OBJ(Map, map), FO_API_ARG(uint, look), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uchar, dir))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Map, map) FO_API_ARG_MARSHAL(uint, look) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uchar, dir))
{
    if (map == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (hx >= map->GetWidth() || hy >= map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    if (!_critter->IsPlayer()) {
        FO_API_RETURN_VOID();
    }

    auto dir_ = dir;
    if (dir_ >= _server->Settings.MapDirCount) {
        dir_ = _critter->GetDir();
    }

    auto look_ = look;
    if (look_ == 0) {
        look_ = _critter->GetLookDistance();
    }

    _critter->ViewMapId = map->GetId();
    _critter->ViewMapPid = map->GetProtoId();
    _critter->ViewMapLook = static_cast<ushort>(look_);
    _critter->ViewMapHx = hx;
    _critter->ViewMapHy = hy;
    _critter->ViewMapDir = dir_;
    _critter->ViewMapLocId = 0;
    _critter->ViewMapLocEnt = 0;
    _critter->Send_LoadMap(map, _server->MapMngr);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param howSay ...
 * @param text ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(Say, FO_API_RET(void), FO_API_ARG(uchar, howSay), FO_API_ARG(string, text))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, howSay) FO_API_ARG_MARSHAL(string, text))
{
    if (howSay != SAY_FLASH_WINDOW && text.empty()) {
        FO_API_RETURN_VOID();
    }
    if (_critter->IsNpc() && !_critter->IsAlive()) {
        FO_API_RETURN_VOID();
    }

    if (howSay >= SAY_NETMSG) {
        _critter->Send_Text(_critter, howSay != SAY_FLASH_WINDOW ? text : " ", howSay);
    }
    else if (_critter->GetMapId()) {
        _critter->SendAA_Text(_critter->VisCr, text, howSay, false);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param howSay ...
 * @param textMsg ...
 * @param numStr ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(SayMsg, FO_API_RET(void), FO_API_ARG(uchar, howSay), FO_API_ARG(ushort, textMsg), FO_API_ARG(uint, numStr))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, howSay) FO_API_ARG_MARSHAL(ushort, textMsg) FO_API_ARG_MARSHAL(uint, numStr))
{
    if (_critter->IsNpc() && !_critter->IsAlive()) {
        FO_API_RETURN_VOID(); // throw ScriptException("Npc is not life");
    }

    if (howSay >= SAY_NETMSG) {
        _critter->Send_TextMsg(_critter, numStr, howSay, textMsg);
    }
    else if (_critter->GetMapId()) {
        _critter->SendAA_Msg(_critter->VisCr, numStr, howSay, textMsg);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param howSay ...
 * @param textMsg ...
 * @param numStr ...
 * @param lexems ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(SayMsgLex, FO_API_RET(void), FO_API_ARG(uchar, howSay), FO_API_ARG(ushort, textMsg), FO_API_ARG(uint, numStr), FO_API_ARG(string, lexems))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, howSay) FO_API_ARG_MARSHAL(ushort, textMsg) FO_API_ARG_MARSHAL(uint, numStr) FO_API_ARG_MARSHAL(string, lexems))
{
    if (_critter->IsNpc() && !_critter->IsAlive()) {
        FO_API_RETURN_VOID();
    }

    if (howSay >= SAY_NETMSG) {
        _critter->Send_TextMsgLex(_critter, numStr, howSay, textMsg, lexems.c_str());
    }
    else if (_critter->GetMapId()) {
        _critter->SendAA_MsgLex(_critter->VisCr, numStr, howSay, textMsg, lexems.c_str());
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param dir ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(SetDir, FO_API_RET(void), FO_API_ARG(uchar, dir))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, dir))
{
    if (dir >= _server->Settings.MapDirCount) {
        throw ScriptException("Invalid direction arg");
    }

    // Direction already set
    if (_critter->GetDir() == dir) {
        FO_API_RETURN_VOID();
    }

    _critter->SetDir(dir);

    if (_critter->GetMapId()) {
        _critter->Send_Dir(_critter);
        _critter->SendA_Dir();
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param lookOnMe ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetCritters, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(bool, lookOnMe), FO_API_ARG(uchar, findType))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, lookOnMe) FO_API_ARG_MARSHAL(uchar, findType))
{
    CritterVec critters;

    for (auto* cr : lookOnMe ? _critter->VisCr : _critter->VisCrSelf) {
        if (cr->CheckFind(findType)) {
            critters.push_back(cr);
        }
    }

    int hx = _critter->GetHexX();
    int hy = _critter->GetHexY();
    std::sort(critters.begin(), critters.end(), [_server, hx, hy](Critter* cr1, Critter* cr2) {
        const auto dist1 = _server->GeomHelper.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY());
        const auto dist2 = _server->GeomHelper.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
        return dist1 < dist2;
    });

    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetTalkedPlayers, FO_API_RET_OBJ_ARR(Critter))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    if (!_critter->IsNpc()) {
        throw ScriptException("Critter is not npc");
    }

    CritterVec players;

    for (auto* cr_ : _critter->VisCr) {
        if (cr_->IsPlayer()) {
            auto* cl = dynamic_cast<Client*>(cr_);
            if (cl->Talk.Type == TalkType::Npc && cl->Talk.TalkNpc == _critter->GetId()) {
                players.push_back(cl);
            }
        }
    }

    int hx = _critter->GetHexX();
    int hy = _critter->GetHexY();
    std::sort(players.begin(), players.end(), [_server, hx, hy](Critter* cr1, Critter* cr2) {
        const auto dist1 = _server->GeomHelper.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY());
        const auto dist2 = _server->GeomHelper.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
        return dist1 < dist2;
    });

    FO_API_RETURN(players);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(IsSeeCr, FO_API_RET(bool), FO_API_ARG_OBJ(Critter, cr))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr))
{
    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (_critter == cr) {
        FO_API_RETURN(true);
    }

    const auto& critters = (_critter->GetMapId() ? _critter->VisCrSelf : *_critter->GlobalMapGroup);
    FO_API_RETURN(std::find(critters.begin(), critters.end(), cr) != critters.end());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(IsSeenByCr, FO_API_RET(bool), FO_API_ARG_OBJ(Critter, cr))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr))
{
    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (_critter == cr) {
        FO_API_RETURN(true);
    }

    const auto& critters = (_critter->GetMapId() ? _critter->VisCr : *_critter->GlobalMapGroup);
    FO_API_RETURN(std::find(critters.begin(), critters.end(), cr) != critters.end());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param item ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(IsSeeItem, FO_API_RET(bool), FO_API_ARG_OBJ(Item, item))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Item, item))
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }

    FO_API_RETURN(_critter->CountIdVisItem(item->GetId()));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param protoId ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(CountItem, FO_API_RET(uint), FO_API_ARG(hash, protoId))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, protoId))
{
    FO_API_RETURN(_critter->CountItemPid(protoId));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param count ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(DeleteItem, FO_API_RET(bool), FO_API_ARG(hash, pid), FO_API_ARG(uint, count))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_MARSHAL(uint, count))
{
    if (pid == 0u) {
        throw ScriptException("Proto id arg is zero");
    }

    auto count_ = count;
    if (count_ == 0) {
        count_ = _critter->CountItemPid(pid);
    }

    FO_API_RETURN(_server->ItemMngr.SubItemCritter(_critter, pid, count_, nullptr));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param count ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(AddItem, FO_API_RET_OBJ(Item), FO_API_ARG(hash, pid), FO_API_ARG(uint, count))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_MARSHAL(uint, count))
{
    if (pid == 0u) {
        throw ScriptException("Proto id arg is zero");
    }
    if (!_server->ProtoMngr.GetProtoItem(pid)) {
        throw ScriptException("Invalid proto", _str().parseHash(pid));
    }

    FO_API_RETURN(_server->ItemMngr.AddItemCritter(_critter, pid, count > 0 ? count : 1));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param itemId ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetItem, FO_API_RET_OBJ(Item), FO_API_ARG(uint, itemId))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId))
{
    if (itemId == 0u) {
        FO_API_RETURN(static_cast<Item*>(nullptr));
    }

    FO_API_RETURN(_critter->GetItem(itemId, false));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param predicate ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetItemByPredicate, FO_API_RET_OBJ(Item), FO_API_ARG_PREDICATE(Item, predicate))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_PREDICATE_MARSHAL(Item, predicate))
{
    auto inv_items_copy = _critter->GetInventory(); // Make copy cuz predicate call can change inventory
    for (auto* item : inv_items_copy) {
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed) {
            FO_API_RETURN(item);
        }
    }

    FO_API_RETURN(static_cast<Item*>(nullptr));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param slot ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetItemBySlot, FO_API_RET_OBJ(Item), FO_API_ARG(int, slot))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, slot))
{
    FO_API_RETURN(_critter->GetItemSlot(slot));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param protoId ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetItemByPid, FO_API_RET_OBJ(Item), FO_API_ARG(hash, protoId))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, protoId))
{
    FO_API_RETURN(_server->CrMngr.GetItemByPidInvPriority(_critter, protoId));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetItems, FO_API_RET_OBJ_ARR(Item))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critter->GetInventory());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param slot ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetItemsBySlot, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(int, slot))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, slot))
{
    ItemVec items;
    _critter->GetItemsSlot(slot, items);
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
FO_API_CRITTER_METHOD(GetItemsByPredicate, FO_API_RET_OBJ_ARR(Item), FO_API_ARG_PREDICATE(Item, predicate))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_PREDICATE_MARSHAL(Item, predicate))
{
    auto inv_items = _critter->GetInventory();
    ItemVec items;
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

/*******************************************************************************
 * ...
 *
 * @param itemId ...
 * @param slot ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(ChangeItemSlot, FO_API_RET(void), FO_API_ARG(uint, itemId), FO_API_ARG(uchar, slot))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId) FO_API_ARG_MARSHAL(uchar, slot))
{
    if (itemId == 0u) {
        throw ScriptException("Item id arg is zero");
    }

    auto* item = _critter->GetItem(itemId, _critter->IsPlayer());
    if (!item) {
        throw ScriptException("Item not found");
    }

    // To slot arg is equal of current item slot
    if (item->GetCritSlot() == slot) {
        FO_API_RETURN_VOID();
    }

    if (slot >= _server->Settings.CritterSlotEnabled.size() || !_server->Settings.CritterSlotEnabled[slot]) {
        throw ScriptException("Slot is not allowed");
    }

    if (!_server->ScriptSys.CritterCheckMoveItemEvent(_critter, item, slot)) {
        throw ScriptException("Can't move item");
    }

    auto* item_swap = (slot ? _critter->GetItemSlot(slot) : nullptr);
    const auto from_slot = item->GetCritSlot();

    item->SetCritSlot(slot);
    if (item_swap) {
        item_swap->SetCritSlot(from_slot);
    }

    _critter->SendAA_MoveItem(item, ACTION_MOVE_ITEM, from_slot);

    if (item_swap) {
        _server->ScriptSys.CritterMoveItemEvent(_critter, item_swap, slot);
    }
    _server->ScriptSys.CritterMoveItemEvent(_critter, item, from_slot);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param cond ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(SetCondition, FO_API_RET(void), FO_API_ARG(int, cond))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, cond))
{
    const auto prev_cond = _critter->GetCond();
    if (prev_cond == cond) {
        FO_API_RETURN_VOID();
    }

    _critter->SetCond(cond);

    if (_critter->GetMapId()) {
        auto* map = _server->MapMngr.GetMap(_critter->GetMapId());
        RUNTIME_ASSERT(map);

        const auto hx = _critter->GetHexX();
        const auto hy = _critter->GetHexY();
        const auto multihex = _critter->GetMultihex();
        const auto is_dead = (cond == COND_DEAD);

        map->UnsetFlagCritter(hx, hy, multihex, !is_dead);
        map->SetFlagCritter(hx, hy, multihex, is_dead);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 ******************************************************************************/
FO_API_CRITTER_METHOD(CloseDialog, FO_API_RET(void))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    if (_critter->IsPlayer() && dynamic_cast<Client*>(_critter)->IsTalking()) {
        _server->CrMngr.CloseTalk(dynamic_cast<Client*>(_critter));
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param action ...
 * @param actionExt ...
 * @param item ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(Action, FO_API_RET(void), FO_API_ARG(int, action), FO_API_ARG(int, actionExt), FO_API_ARG_OBJ(Item, item))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, action) FO_API_ARG_MARSHAL(int, actionExt) FO_API_ARG_OBJ_MARSHAL(Item, item))
{
    _critter->SendAA_Action(action, actionExt, item);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param anim1 ...
 * @param anim2 ...
 * @param item ...
 * @param clearSequence ...
 * @param delayPlay ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(Animate, FO_API_RET(void), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2), FO_API_ARG_OBJ(Item, item), FO_API_ARG(bool, clearSequence), FO_API_ARG(bool, delayPlay))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2) FO_API_ARG_OBJ_MARSHAL(Item, item) FO_API_ARG_MARSHAL(bool, clearSequence) FO_API_ARG_MARSHAL(bool, delayPlay))
{
    _critter->SendAA_Animate(anim1, anim2, item, clearSequence, delayPlay);
}
FO_API_EPILOG()
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param cond ...
 * @param anim1 ...
 * @param anim2 ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(SetConditionAnims, FO_API_RET(void), FO_API_ARG(int, cond), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, cond) FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2))
{
    if (cond == 0 || cond == COND_ALIVE) {
        _critter->SetAnim1Life(anim1);
        _critter->SetAnim2Life(anim2);
    }
    if (cond == 0 || cond == COND_KNOCKOUT) {
        _critter->SetAnim1Knockout(anim1);
        _critter->SetAnim2Knockout(anim2);
    }
    if (cond == 0 || cond == COND_DEAD) {
        _critter->SetAnim1Dead(anim1);
        _critter->SetAnim2Dead(anim2);
    }
    _critter->SendAA_SetAnims(cond, anim1, anim2);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param soundName ...
 * @param sendSelf ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(PlaySound, FO_API_RET(void), FO_API_ARG(string, soundName), FO_API_ARG(bool, sendSelf))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, soundName) FO_API_ARG_MARSHAL(bool, sendSelf))
{
    if (sendSelf) {
        _critter->Send_PlaySound(_critter->GetId(), soundName);
    }

    for (auto it = _critter->VisCr.begin(), end = _critter->VisCr.end(); it != end; ++it) {
        auto* cr_ = *it;
        cr_->Send_PlaySound(_critter->GetId(), soundName);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param byId ...
 * @param locNum ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(IsKnownLocation, FO_API_RET(bool), FO_API_ARG(bool, byId), FO_API_ARG(uint, locNum))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, byId) FO_API_ARG_MARSHAL(uint, locNum))
{
    if (byId) {
        FO_API_RETURN(_server->MapMngr.CheckKnownLocById(_critter, locNum));
    }
    FO_API_RETURN(_server->MapMngr.CheckKnownLocByPid(_critter, locNum));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param byId ...
 * @param locNum ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(SetKnownLocation, FO_API_RET(void), FO_API_ARG(bool, byId), FO_API_ARG(uint, locNum))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, byId) FO_API_ARG_MARSHAL(uint, locNum))
{
    auto* loc = (byId ? _server->MapMngr.GetLocation(locNum) : _server->MapMngr.GetLocationByPid(locNum, 0));
    if (!loc) {
        throw ScriptException("Location not found");
    }

    _server->MapMngr.AddKnownLoc(_critter, loc->GetId());

    if (loc->IsNonEmptyAutomaps()) {
        _critter->Send_AutomapsInfo(nullptr, loc);
    }

    if (!_critter->GetMapId()) {
        _critter->Send_GlobalLocation(loc, true);
    }

    int zx = loc->GetWorldX() / _server->Settings.GlobalMapZoneLength;
    int zy = loc->GetWorldY() / _server->Settings.GlobalMapZoneLength;

    auto gmap_fog = _critter->GetGlobalMapFog();
    if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
        gmap_fog.resize(GM_ZONES_FOG_SIZE);
    }

    /*TwoBitMask gmap_mask(GM_MAXZONEX, GM_MAXZONEY, gmap_fog.data());
    if (gmap_mask.Get2Bit(zx, zy) == GM_FOG_FULL) {
        gmap_mask.Set2Bit(zx, zy, GM_FOG_HALF);
        _critter->SetGlobalMapFog(gmap_fog);
        if (!_critter->GetMapId())
            _critter->Send_GlobalMapFog(zx, zy, GM_FOG_HALF);
    }*/
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param byId ...
 * @param locNum ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(UnsetKnownLocation, FO_API_RET(void), FO_API_ARG(bool, byId), FO_API_ARG(uint, locNum))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, byId) FO_API_ARG_MARSHAL(uint, locNum))
{
    auto* loc = (byId ? _server->MapMngr.GetLocation(locNum) : _server->MapMngr.GetLocationByPid(locNum, 0));
    if (!loc) {
        throw ScriptException("Location not found");
    }
    if (!_server->MapMngr.CheckKnownLocById(_critter, loc->GetId())) {
        throw ScriptException("Player is not know this location");
    }

    _server->MapMngr.EraseKnownLoc(_critter, loc->GetId());

    if (!_critter->GetMapId()) {
        _critter->Send_GlobalLocation(loc, false);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param zoneX ...
 * @param zoneY ...
 * @param fog ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(SetFog, FO_API_RET(void), FO_API_ARG(ushort, zoneX), FO_API_ARG(ushort, zoneY), FO_API_ARG(int, fog))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, zoneX) FO_API_ARG_MARSHAL(ushort, zoneY) FO_API_ARG_MARSHAL(int, fog))
{
    if (fog < GM_FOG_FULL || fog > GM_FOG_NONE) {
        throw ScriptException("Invalid fog arg");
    }
    if (zoneX >= _server->Settings.GlobalMapWidth || zoneY >= _server->Settings.GlobalMapHeight) {
        FO_API_RETURN_VOID();
    }

    auto gmap_fog = _critter->GetGlobalMapFog();
    if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
        gmap_fog.resize(GM_ZONES_FOG_SIZE);
    }

    /*TwoBitMask gmap_mask(GM_MAXZONEX, GM_MAXZONEY, gmap_fog.data());
    if (gmap_mask.Get2Bit(zoneX, zoneY) != fog) {
        gmap_mask.Set2Bit(zoneX, zoneY, fog);
        _critter->SetGlobalMapFog(gmap_fog);
        if (!_critter->GetMapId())
            _critter->Send_GlobalMapFog(zoneX, zoneY, fog);
    }*/
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
FO_API_CRITTER_METHOD(GetFog, FO_API_RET(int), FO_API_ARG(ushort, zoneX), FO_API_ARG(ushort, zoneY))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, zoneX) FO_API_ARG_MARSHAL(ushort, zoneY))
{
    if (zoneX >= _server->Settings.GlobalMapWidth || zoneY >= _server->Settings.GlobalMapHeight) {
        FO_API_RETURN(GM_FOG_FULL);
    }

    auto gmap_fog = _critter->GetGlobalMapFog();
    if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
        gmap_fog.resize(GM_ZONES_FOG_SIZE);
    }

    /*TwoBitMask gmap_mask(GM_MAXZONEX, GM_MAXZONEY, gmap_fog.data());
    FO_API_RETURN(gmap_mask.Get2Bit(zoneX, zoneY));*/
    FO_API_RETURN(0);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param items ...
 * @param param ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(ShowItems, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Item, items), FO_API_ARG(int, param))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Item, items) FO_API_ARG_MARSHAL(int, param))
{
    // Critter is not player
    if (!_critter->IsPlayer()) {
        FO_API_RETURN_VOID();
    }

    dynamic_cast<Client*>(_critter)->Send_SomeItems(&items, param);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 ******************************************************************************/
FO_API_CRITTER_METHOD(Disconnect, FO_API_RET(void))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    if (!_critter->IsPlayer()) {
        throw ScriptException("Critter is not player");
    }

    auto* cl_ = dynamic_cast<Client*>(_critter);
    if (cl_->IsOnline()) {
        cl_->Disconnect();
    }
}
FO_API_EPILOG()
#endif
#endif

#ifdef FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(IsOnline, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    if (!_critter->IsPlayer()) {
        throw ScriptException("Critter is not player");
    }

    auto* cl_ = dynamic_cast<Client*>(_critter);
    FO_API_RETURN(cl_->IsOnline());
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param funcName ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(SetScript, FO_API_RET(void), FO_API_ARG(string, funcName))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, funcName))
{
    if (!funcName.empty()) {
        if (!_critter->SetScript(funcName, true)) {
            throw ScriptException("Script function not found");
        }
    }
    else {
        _critter->SetScriptId(0);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param func ...
 * @param duration ...
 * @param identifier ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(AddTimeEvent, FO_API_RET(void), FO_API_ARG_CALLBACK(Critter, func), FO_API_ARG(uint, duration), FO_API_ARG(int, identifier))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_CALLBACK_MARSHAL(Critter, func) FO_API_ARG_MARSHAL(uint, duration) FO_API_ARG_MARSHAL(int, identifier))
{
    /*hash func_num = _server->ScriptSys.BindScriptFuncNumByFunc(func);
    if (!func_num)
        throw ScriptException("Function not found");

    _critter->AddCrTimeEvent(func_num, 0, duration, identifier);*/
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param func ...
 * @param duration ...
 * @param identifier ...
 * @param rate ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(AddTimeEventWithRate, FO_API_RET(void), FO_API_ARG_CALLBACK(Critter, func), FO_API_ARG(uint, duration), FO_API_ARG(int, identifier), FO_API_ARG(uint, rate))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_CALLBACK_MARSHAL(Critter, func) FO_API_ARG_MARSHAL(uint, duration) FO_API_ARG_MARSHAL(int, identifier) FO_API_ARG_MARSHAL(uint, rate))
{
    /*hash func_num = _server->ScriptSys.BindScriptFuncNumByFunc(func);
    if (!func_num)
        throw ScriptException("Function not found");

    _critter->AddCrTimeEvent(func_num, rate, duration, identifier);*/
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param identifier ...
 * @param indexes ...
 * @param durations ...
 * @param rates ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetTimeEvents, FO_API_RET(uint), FO_API_ARG(int, identifier), FO_API_ARG_ARR_REF(int, indexes), FO_API_ARG_ARR_REF(int, durations), FO_API_ARG_ARR_REF(int, rates))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, identifier) FO_API_ARG_ARR_REF_MARSHAL(int, indexes) FO_API_ARG_ARR_REF_MARSHAL(int, durations) FO_API_ARG_ARR_REF_MARSHAL(int, rates))
{
    /*CScriptArray* te_identifier = _critter->GetTE_Identifier();
    UIntVec te_vec;
    for (uint i = 0, j = te_identifier->GetSize(); i < j; i++)
    {
        if (*(int*)te_identifier->At(i) == identifier)
            te_vec.push_back(i);
    }
    te_identifier->Release();

    uint size = (uint)te_vec.size();
    if (!size || (!indexes && !durations && !rates))
        FO_API_RETURN(size);

    CScriptArray* te_next_time = nullptr;
    CScriptArray* te_rate = nullptr;

    uint indexes_size = 0, durations_size = 0, rates_size = 0;
    if (indexes)
    {
        indexes_size = indexes->GetSize();
        indexes->Resize(indexes_size + size);
    }
    if (durations)
    {
        te_next_time = _critter->GetTE_NextTime();
        RUNTIME_ASSERT(te_next_time->GetSize() == te_identifier->GetSize());
        durations_size = durations->GetSize();
        durations->Resize(durations_size + size);
    }
    if (rates)
    {
        te_rate = _critter->GetTE_Rate();
        RUNTIME_ASSERT(te_rate->GetSize() == te_identifier->GetSize());
        rates_size = rates->GetSize();
        rates->Resize(rates_size + size);
    }

    for (uint i = 0; i < size; i++)
    {
        if (indexes)
        {
            *(uint*)indexes->At(indexes_size + i) = te_vec[i];
        }
        if (durations)
        {
            uint next_time = *(uint*)te_next_time->At(te_vec[i]);
            *(uint*)durations->At(durations_size + i) =
                (next_time > _server->Settings.FullSecond ? next_time - _server->Settings.FullSecond : 0);
        }
        if (rates)
        {
            *(uint*)rates->At(rates_size + i) = *(uint*)te_rate->At(te_vec[i]);
        }
    }

    if (te_next_time)
        te_next_time->Release();
    if (te_rate)
        te_rate->Release();

    FO_API_RETURN(size);*/
    FO_API_RETURN(0);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param findIdentifiers ...
 * @param identifiers ...
 * @param indexes ...
 * @param durations ...
 * @param rates ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetTimeEventsExt, FO_API_RET(uint), FO_API_ARG_ARR(int, findIdentifiers), FO_API_ARG_ARR(int, identifiers), FO_API_ARG_ARR_REF(int, indexes), FO_API_ARG_ARR_REF(int, durations), FO_API_ARG_ARR_REF(int, rates))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_ARR_MARSHAL(int, findIdentifiers) FO_API_ARG_ARR_MARSHAL(int, identifiers) FO_API_ARG_ARR_REF_MARSHAL(int, indexes) FO_API_ARG_ARR_REF_MARSHAL(int, durations) FO_API_ARG_ARR_REF_MARSHAL(int, rates))
{
    /*IntVec find_vec;
    _server->ScriptSys.AssignScriptArrayInVector(find_vec, findIdentifiers);

    CScriptArray* te_identifier = _critter->GetTE_Identifier();
    UIntVec te_vec;
    for (uint i = 0, j = te_identifier->GetSize(); i < j; i++)
    {
        if (std::find(find_vec.begin(), find_vec.end(), *(int*)te_identifier->At(i)) != find_vec.end())
            te_vec.push_back(i);
    }

    uint size = (uint)te_vec.size();
    if (!size || (!identifiers && !indexes && !durations && !rates))
    {
        te_identifier->Release();
        FO_API_RETURN(size);
    }

    CScriptArray* te_next_time = nullptr;
    CScriptArray* te_rate = nullptr;

    uint identifiers_size = 0, indexes_size = 0, durations_size = 0, rates_size = 0;
    if (identifiers)
    {
        identifiers_size = identifiers->GetSize();
        identifiers->Resize(identifiers_size + size);
    }
    if (indexes)
    {
        indexes_size = indexes->GetSize();
        indexes->Resize(indexes_size + size);
    }
    if (durations)
    {
        te_next_time = _critter->GetTE_NextTime();
        RUNTIME_ASSERT(te_next_time->GetSize() == te_identifier->GetSize());
        durations_size = durations->GetSize();
        durations->Resize(durations_size + size);
    }
    if (rates)
    {
        te_rate = _critter->GetTE_Rate();
        RUNTIME_ASSERT(te_rate->GetSize() == te_identifier->GetSize());
        rates_size = rates->GetSize();
        rates->Resize(rates_size + size);
    }

    for (uint i = 0; i < size; i++)
    {
        if (identifiers)
        {
            *(int*)identifiers->At(identifiers_size + i) = *(uint*)te_identifier->At(te_vec[i]);
        }
        if (indexes)
        {
            *(uint*)indexes->At(indexes_size + i) = te_vec[i];
        }
        if (durations)
        {
            uint next_time = *(uint*)te_next_time->At(te_vec[i]);
            *(uint*)durations->At(durations_size + i) =
                (next_time > _server->Settings.FullSecond ? next_time - _server->Settings.FullSecond : 0);
        }
        if (rates)
        {
            *(uint*)rates->At(rates_size + i) = *(uint*)te_rate->At(te_vec[i]);
        }
    }

    te_identifier->Release();
    if (te_next_time)
        te_next_time->Release();
    if (te_rate)
        te_rate->Release();

    FO_API_RETURN(size);*/
    FO_API_RETURN(0);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param index ...
 * @param newDuration ...
 * @param newRate ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(ChangeTimeEvent, FO_API_RET(void), FO_API_ARG(uint, index), FO_API_ARG(uint, newDuration), FO_API_ARG(uint, newRate))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, index) FO_API_ARG_MARSHAL(uint, newDuration) FO_API_ARG_MARSHAL(uint, newRate))
{
    /*CScriptArray* te_func_num = _critter->GetTE_FuncNum();
    CScriptArray* te_identifier = _critter->GetTE_Identifier();
    RUNTIME_ASSERT(te_func_num->GetSize() == te_identifier->GetSize());
    if (index >= te_func_num->GetSize())
    {
        te_func_num->Release();
        te_identifier->Release();
        throw ScriptException("Index arg is greater than maximum time events");
    }

    hash func_num = *(hash*)te_func_num->At(index);
    int identifier = *(int*)te_identifier->At(index);
    te_func_num->Release();
    te_identifier->Release();

    _critter->EraseCrTimeEvent(index);
    _critter->AddCrTimeEvent(func_num, newRate, newDuration, identifier);*/
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param index ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(EraseTimeEvent, FO_API_RET(void), FO_API_ARG(uint, index))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, index))
{
    /*CScriptArray* te_func_num = _critter->GetTE_FuncNum();
    uint size = te_func_num->GetSize();
    te_func_num->Release();
    if (index >= size)
        throw ScriptException("Index arg is greater than maximum time events");

    _critter->EraseCrTimeEvent(index);*/
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param identifier ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(EraseTimeEvents, FO_API_RET(uint), FO_API_ARG(int, identifier))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, identifier))
{
    /*CScriptArray* te_next_time = _critter->GetTE_NextTime();
    CScriptArray* te_func_num = _critter->GetTE_FuncNum();
    CScriptArray* te_rate = _critter->GetTE_Rate();
    CScriptArray* te_identifier = _critter->GetTE_Identifier();
    RUNTIME_ASSERT(te_next_time->GetSize() == te_func_num->GetSize());
    RUNTIME_ASSERT(te_func_num->GetSize() == te_rate->GetSize());
    RUNTIME_ASSERT(te_rate->GetSize() == te_identifier->GetSize());

    uint result = 0;
    for (uint i = 0; i < te_identifier->GetSize();)
    {
        if (identifier == *(int*)te_identifier->At(i))
        {
            result++;
            te_next_time->RemoveAt(i);
            te_func_num->RemoveAt(i);
            te_rate->RemoveAt(i);
            te_identifier->RemoveAt(i);
        }
        else
        {
            i++;
        }
    }

    _critter->SetTE_NextTime(te_next_time);
    _critter->SetTE_FuncNum(te_func_num);
    _critter->SetTE_Rate(te_rate);
    _critter->SetTE_Identifier(te_identifier);

    te_next_time->Release();
    te_func_num->Release();
    te_rate->Release();
    te_identifier->Release();

    FO_API_RETURN(result);*/
    FO_API_RETURN(0);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param identifiers ...
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(EraseTimeEventsExt, FO_API_RET(uint), FO_API_ARG_ARR(int, identifiers))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_ARR_MARSHAL(int, identifiers))
{
    /*IntVec identifiers_;
    _server->ScriptSys.AssignScriptArrayInVector(identifiers_, identifiers);

    CScriptArray* te_next_time = _critter->GetTE_NextTime();
    CScriptArray* te_func_num = _critter->GetTE_FuncNum();
    CScriptArray* te_rate = _critter->GetTE_Rate();
    CScriptArray* te_identifier = _critter->GetTE_Identifier();
    RUNTIME_ASSERT(te_next_time->GetSize() == te_func_num->GetSize());
    RUNTIME_ASSERT(te_func_num->GetSize() == te_rate->GetSize());
    RUNTIME_ASSERT(te_rate->GetSize() == te_identifier->GetSize());

    uint result = 0;
    for (uint i = 0; i < te_func_num->GetSize();)
    {
        if (std::find(identifiers_.begin(), identifiers_.end(), *(int*)te_identifier->At(i)) != identifiers_.end())
        {
            result++;
            te_next_time->RemoveAt(i);
            te_func_num->RemoveAt(i);
            te_rate->RemoveAt(i);
            te_identifier->RemoveAt(i);
        }
        else
        {
            i++;
        }
    }

    _critter->SetTE_NextTime(te_next_time);
    _critter->SetTE_FuncNum(te_func_num);
    _critter->SetTE_Rate(te_rate);
    _critter->SetTE_Identifier(te_identifier);

    te_next_time->Release();
    te_func_num->Release();
    te_rate->Release();
    te_identifier->Release();

    FO_API_RETURN(result);*/
    FO_API_RETURN(0);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param target ...
 * @param cut ...
 * @param isRun ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(MoveToCritter, FO_API_RET(void), FO_API_ARG_OBJ(Critter, target), FO_API_ARG(uint, cut), FO_API_ARG(bool, isRun))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, target) FO_API_ARG_MARSHAL(uint, cut) FO_API_ARG_MARSHAL(bool, isRun))
{
    if (target == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    std::memset(&_critter->Moving, 0, sizeof(_critter->Moving));

    _critter->Moving.TargId = target->GetId();
    _critter->Moving.HexX = target->GetHexX();
    _critter->Moving.HexY = target->GetHexY();
    _critter->Moving.Cut = cut;
    _critter->Moving.IsRun = isRun;
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param cut ...
 * @param isRun ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(MoveToHex, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, cut), FO_API_ARG(bool, isRun))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, cut) FO_API_ARG_MARSHAL(bool, isRun))
{
    std::memset(&_critter->Moving, 0, sizeof(_critter->Moving));

    _critter->Moving.HexX = hx;
    _critter->Moving.HexY = hy;
    _critter->Moving.Cut = cut;
    _critter->Moving.IsRun = isRun;
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(GetMovingState, FO_API_RET(int))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_critter->Moving.State);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 ******************************************************************************/
FO_API_CRITTER_METHOD(ResetMovingState, FO_API_RET(void))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    std::memset(&_critter->Moving, 0, sizeof(_critter->Moving));
    _critter->Moving.State = 1;
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetLocation, FO_API_RET_OBJ(Location))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_map->GetLocation());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param funcName ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(SetScript, FO_API_RET(void), FO_API_ARG(string, funcName))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, funcName))
{
    if (!funcName.empty()) {
        if (!_map->SetScript(funcName, true)) {
            throw ScriptException("Script function not found");
        }
    }
    else {
        _map->SetScriptId(0);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param protoId ...
 * @param count ...
 * @param props ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(AddItem, FO_API_RET_OBJ(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(hash, protoId), FO_API_ARG(uint, count), FO_API_ARG_DICT(int, int, props))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(hash, protoId) FO_API_ARG_MARSHAL(uint, count) FO_API_ARG_DICT_MARSHAL(int, int, props))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    const auto* proto = _server->ProtoMngr.GetProtoItem(protoId);
    if (!proto) {
        throw ScriptException("Invalid proto '{}' arg.", _str().parseHash(protoId));
    }
    if (!_map->IsPlaceForProtoItem(hx, hy, proto)) {
        throw ScriptException("No place for item");
    }

    if (!props.empty()) {
        Properties props_(Item::PropertiesRegistrator);
        props_ = proto->Props;

        for (const auto& [key, value] : props) {
            props_.SetValueAsIntProps(key, value);
        }

        FO_API_RETURN(_server->CreateItemOnHex(_map, hx, hy, protoId, count > 0 ? count : 1, &props_, true));
    }

    FO_API_RETURN(_server->CreateItemOnHex(_map, hx, hy, protoId, count > 0 ? count : 1, nullptr, true));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetItems, FO_API_RET_OBJ_ARR(Item))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG()
{
    ItemVec items;
    _map->GetItemsPid(0, items);
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
FO_API_MAP_METHOD(GetItemsOnHex, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    ItemVec items;
    _map->GetItemsHex(hx, hy, items);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param radius ...
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetItemsAroundHex, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, radius), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, radius) FO_API_ARG_MARSHAL(hash, pid))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    ItemVec items;
    _map->GetItemsHexEx(hx, hy, radius, pid, items);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetItemsByPid, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    ItemVec items;
    _map->GetItemsPid(pid, items);
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
FO_API_MAP_METHOD(GetItemsByPredicate, FO_API_RET_OBJ_ARR(Item), FO_API_ARG_PREDICATE(Item, predicate))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_PREDICATE_MARSHAL(Item, predicate))
{
    auto map_items = _map->GetItems();
    ItemVec items;
    items.reserve(map_items.size());
    for (auto* item : map_items) {
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed) {
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
 * @param predicate ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetItemsOnHexByPredicate, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG_PREDICATE(Item, predicate))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_PREDICATE_MARSHAL(Item, predicate))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    ItemVec map_items;
    _map->GetItemsHex(hx, hy, map_items);
    ItemVec items;
    items.reserve(map_items.size());
    for (auto* item : map_items) {
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed) {
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
 * @param radius ...
 * @param predicate ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetItemsAroundHexByPredicate, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, radius), FO_API_ARG_PREDICATE(Item, predicate))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, radius) FO_API_ARG_PREDICATE_MARSHAL(Item, predicate))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    ItemVec map_items;
    _map->GetItemsHexEx(hx, hy, radius, 0, map_items);
    ItemVec items;
    items.reserve(map_items.size());
    for (auto* item : map_items) {
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed) {
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
 * @param itemId ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetItem, FO_API_RET_OBJ(Item), FO_API_ARG(uint, itemId))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId))
{
    if (itemId == 0u) {
        throw ScriptException("Item id arg is zero");
    }

    FO_API_RETURN(_map->GetItem(itemId));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetItemOnHex, FO_API_RET_OBJ(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(hash, pid))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    FO_API_RETURN(_map->GetItemHex(hx, hy, pid, nullptr));
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
FO_API_MAP_METHOD(GetCritterOnHex, FO_API_RET_OBJ(Critter), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    auto* cr = _map->GetHexCritter(hx, hy, false);
    if (!cr) {
        cr = _map->GetHexCritter(hx, hy, true);
    }
    FO_API_RETURN(cr);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetStaticItemOnHex, FO_API_RET_OBJ(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(hash, pid))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    FO_API_RETURN(_map->GetStaticItem(hx, hy, pid));
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
FO_API_MAP_METHOD(GetStaticItemsInHex, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    ItemVec static_items;
    _map->GetStaticItemsHex(hx, hy, static_items);

    FO_API_RETURN(static_items);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param radius ...
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetStaticItemsAroundHex, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, radius), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, radius) FO_API_ARG_MARSHAL(hash, pid))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    ItemVec static_items;
    _map->GetStaticItemsHexEx(hx, hy, radius, pid, static_items);
    FO_API_RETURN(static_items);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetStaticItemsByPid, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    ItemVec static_items;
    _map->GetStaticItemsByPid(pid, static_items);
    FO_API_RETURN(static_items);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param predicate ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetStaticItemsByPredicate, FO_API_RET_OBJ_ARR(Item), FO_API_ARG_PREDICATE(Item, predicate))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_PREDICATE_MARSHAL(Item, predicate))
{
    const auto& map_static_items = _map->GetStaticMap()->StaticItemsVec;
    ItemVec items;
    items.reserve(map_static_items.size());
    for (auto* item : map_static_items) {
        if (predicate(item)) {
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
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetStaticItems, FO_API_RET_OBJ_ARR(Item))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_map->GetStaticMap()->StaticItemsVec);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param crid ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetCritter, FO_API_RET_OBJ(Critter), FO_API_ARG(uint, crid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, crid))
{
    FO_API_RETURN(_map->GetCritter(crid));
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
FO_API_MAP_METHOD(GetCrittersAroundHex, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, radius), FO_API_ARG(int, findType))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, radius) FO_API_ARG_MARSHAL(int, findType))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    CritterVec critters;
    _map->GetCrittersHex(hx, hy, radius, findType, critters);
    std::sort(critters.begin(), critters.end(), [_server, hx, hy](Critter* cr1, Critter* cr2) { return _server->GeomHelper.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY()) < _server->GeomHelper.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY()); });
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
FO_API_MAP_METHOD(GetCrittersByPids, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(hash, pid), FO_API_ARG(uchar, findType))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_MARSHAL(uchar, findType))
{
    CritterVec critters;
    if (pid == 0u) {
        auto map_critters = _map->GetCritters();
        critters.reserve(map_critters.size());
        for (auto* cr : map_critters) {
            if (cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }
    else {
        auto map_npcs = _map->GetNpcs();
        critters.reserve(map_npcs.size());
        for (auto* npc : map_npcs) {
            if (npc->GetProtoId() == pid && npc->CheckFind(findType)) {
                critters.push_back(npc);
            }
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
FO_API_MAP_METHOD(GetCrittersInPath, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(float, angle), FO_API_ARG(uint, dist), FO_API_ARG(int, findType))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist) FO_API_ARG_MARSHAL(int, findType))
{
    CritterVec critters;
    TraceData trace;
    trace.TraceMap = _map;
    trace.BeginHx = fromHx;
    trace.BeginHy = fromHy;
    trace.EndHx = toHx;
    trace.EndHy = toHy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.Critters = &critters;
    trace.FindType = findType;
    _server->MapMngr.TraceBullet(trace);

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
FO_API_MAP_METHOD(GetCrittersWithBlockInPath, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(float, angle), FO_API_ARG(uint, dist), FO_API_ARG(int, findType), FO_API_ARG_REF(ushort, preBlockHx), FO_API_ARG_REF(ushort, preBlockHy), FO_API_ARG_REF(ushort, blockHx), FO_API_ARG_REF(ushort, blockHy))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist) FO_API_ARG_MARSHAL(int, findType) FO_API_ARG_REF_MARSHAL(ushort, preBlockHx) FO_API_ARG_REF_MARSHAL(ushort, preBlockHy) FO_API_ARG_REF_MARSHAL(ushort, blockHx) FO_API_ARG_REF_MARSHAL(ushort, blockHy))
{
    CritterVec critters;
    UShortPair block;
    UShortPair pre_block;
    TraceData trace;
    trace.TraceMap = _map;
    trace.BeginHx = fromHx;
    trace.BeginHy = fromHy;
    trace.EndHx = toHx;
    trace.EndHy = toHy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.Critters = &critters;
    trace.FindType = findType;
    trace.PreBlock = &pre_block;
    trace.Block = &block;

    _server->MapMngr.TraceBullet(trace);

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
 * @param findType ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetCrittersWhoViewPath, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(uchar, findType))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(uchar, findType))
{
    CritterVec critters;
    for (auto* cr : _map->GetCritters()) {
        if (cr->CheckFind(findType) && std::find(critters.begin(), critters.end(), cr) == critters.end() && GenericUtils::IntersectCircleLine(cr->GetHexX(), cr->GetHexY(), cr->GetLookDistance(), fromHx, fromHy, toHx, toHy)) {
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
 * @param critters ...
 * @param lookOnThem ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetCrittersSeeing, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG_OBJ_ARR(Critter, critters), FO_API_ARG(bool, lookOnThem), FO_API_ARG(uchar, findType))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Critter, critters) FO_API_ARG_MARSHAL(bool, lookOnThem) FO_API_ARG_MARSHAL(uchar, findType))
{
    CritterVec result_critters;
    for (auto* cr : critters) {
        cr->GetCrFromVisCr(result_critters, findType, !lookOnThem);
    }
    FO_API_RETURN(result_critters);
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
FO_API_MAP_METHOD(GetHexInPath, FO_API_RET(void), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG_REF(ushort, toHx), FO_API_ARG_REF(ushort, toHy), FO_API_ARG(float, angle), FO_API_ARG(uint, dist))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_REF_MARSHAL(ushort, toHx) FO_API_ARG_REF_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist))
{
    UShortPair pre_block;
    UShortPair block;
    TraceData trace;
    trace.TraceMap = _map;
    trace.BeginHx = fromHx;
    trace.BeginHy = fromHy;
    trace.EndHx = toHx;
    trace.EndHy = toHy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.PreBlock = &pre_block;
    trace.Block = &block;

    _server->MapMngr.TraceBullet(trace);

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
 * @param angle ...
 * @param dist ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetWallHexInPath, FO_API_RET(void), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG_REF(ushort, toHx), FO_API_ARG_REF(ushort, toHy), FO_API_ARG(float, angle), FO_API_ARG(uint, dist))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_REF_MARSHAL(ushort, toHx) FO_API_ARG_REF_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist))
{
    UShortPair last_passed;
    TraceData trace;
    trace.TraceMap = _map;
    trace.BeginHx = fromHx;
    trace.BeginHy = fromHy;
    trace.EndHx = toHx;
    trace.EndHy = toHy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.LastPassed = &last_passed;

    _server->MapMngr.TraceBullet(trace);

    if (trace.IsHaveLastPassed) {
        toHx = last_passed.first;
        toHy = last_passed.second;
    }
    else {
        toHx = fromHx;
        toHy = fromHy;
    }
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
FO_API_MAP_METHOD(GetPathLengthToHex, FO_API_RET(uint), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(uint, cut))
{
    if (fromHx >= _map->GetWidth() || fromHy >= _map->GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= _map->GetWidth() || toHy >= _map->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    PathFindData pfd;
    pfd.MapId = _map->GetId();
    pfd.FromX = fromHx;
    pfd.FromY = fromHy;
    pfd.ToX = toHx;
    pfd.ToY = toHy;
    pfd.Cut = cut;

    const auto result = _server->MapMngr.FindPath(pfd);

    if (result != FindPathResult::Ok) {
        FO_API_RETURN(0);
    }

    auto& path = _server->MapMngr.GetPath(pfd.PathNum);
    FO_API_RETURN(static_cast<uint>(path.size()));
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
FO_API_MAP_METHOD(GetPathLengthToCritter, FO_API_RET(uint), FO_API_ARG_OBJ(Critter, cr), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(uint, cut))
{
    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (toHx >= _map->GetWidth() || toHy >= _map->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    PathFindData pfd;
    pfd.MapId = _map->GetId();
    pfd.FromCritter = cr;
    pfd.FromX = cr->GetHexX();
    pfd.FromY = cr->GetHexY();
    pfd.ToX = toHx;
    pfd.ToY = toHy;
    pfd.Multihex = cr->GetMultihex();
    pfd.Cut = cut;

    const auto result = _server->MapMngr.FindPath(pfd);

    if (result != FindPathResult::Ok) {
        FO_API_RETURN(0);
    }

    auto& path = _server->MapMngr.GetPath(pfd.PathNum);
    FO_API_RETURN(static_cast<uint>(path.size()));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param protoId ...
 * @param hx ...
 * @param hy ...
 * @param dir ...
 * @param props ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(AddNpc, FO_API_RET_OBJ(Critter), FO_API_ARG(hash, protoId), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uchar, dir), FO_API_ARG_DICT(int, int, props))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, protoId) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uchar, dir) FO_API_ARG_DICT_MARSHAL(int, int, props))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    const auto* proto = _server->ProtoMngr.GetProtoCritter(protoId);
    if (!proto) {
        throw ScriptException("Proto '{}' not found.", _str().parseHash(protoId));
    }

    Critter* npc;
    if (!props.empty()) {
        Properties props_(Critter::PropertiesRegistrator);
        props_ = proto->Props;
        for (const auto& [key, value] : props) {
            props_.SetValueAsIntProps(key, value);
        }

        npc = _server->CrMngr.CreateNpc(protoId, &props_, _map, hx, hy, dir, false);
    }
    else {
        npc = _server->CrMngr.CreateNpc(protoId, nullptr, _map, hx, hy, dir, false);
    }

    if (npc == nullptr) {
        throw ScriptException("Create npc fail");
    }

    FO_API_RETURN(npc);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param npcRole ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetNpcCount, FO_API_RET(uint), FO_API_ARG(int, npcRole), FO_API_ARG(int, findType))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, npcRole) FO_API_ARG_MARSHAL(int, findType))
{
    FO_API_RETURN(_map->GetNpcCount(npcRole, findType));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param npcRole ...
 * @param findType ...
 * @param skipCount ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(GetNpc, FO_API_RET_OBJ(Critter), FO_API_ARG(int, npcRole), FO_API_ARG(int, findType), FO_API_ARG(uint, skipCount))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, npcRole) FO_API_ARG_MARSHAL(int, findType) FO_API_ARG_MARSHAL(uint, skipCount))
{
    FO_API_RETURN(_map->GetNpc(npcRole, findType, skipCount));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hexX ...
 * @param hexY ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(IsHexPassed, FO_API_RET(bool), FO_API_ARG(ushort, hexX), FO_API_ARG(ushort, hexY))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hexX) FO_API_ARG_MARSHAL(ushort, hexY))
{
    if (hexX >= _map->GetWidth() || hexY >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    FO_API_RETURN(_map->IsHexPassed(hexX, hexY));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hexX ...
 * @param hexY ...
 * @param radius ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(IsHexesPassed, FO_API_RET(bool), FO_API_ARG(ushort, hexX), FO_API_ARG(ushort, hexY), FO_API_ARG(uint, radius))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hexX) FO_API_ARG_MARSHAL(ushort, hexY) FO_API_ARG_MARSHAL(uint, radius))
{
    if (hexX >= _map->GetWidth() || hexY >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    FO_API_RETURN(_map->IsHexesPassed(hexX, hexY, radius));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hexX ...
 * @param hexY ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(IsHexRaked, FO_API_RET(bool), FO_API_ARG(ushort, hexX), FO_API_ARG(ushort, hexY))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hexX) FO_API_ARG_MARSHAL(ushort, hexY))
{
    if (hexX >= _map->GetWidth() || hexY >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    FO_API_RETURN(_map->IsHexRaked(hexX, hexY));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hexX ...
 * @param hexY ...
 * @param color ...
 * @param text ...
 ******************************************************************************/
FO_API_MAP_METHOD(SetText, FO_API_RET(void), FO_API_ARG(ushort, hexX), FO_API_ARG(ushort, hexY), FO_API_ARG(uint, color), FO_API_ARG(string, text))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hexX) FO_API_ARG_MARSHAL(ushort, hexY) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(string, text))
{
    if (hexX >= _map->GetWidth() || hexY >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }
    _map->SetText(hexX, hexY, color, text, false);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param hexX ...
 * @param hexY ...
 * @param color ...
 * @param textMsg ...
 * @param strNum ...
 ******************************************************************************/
FO_API_MAP_METHOD(SetTextMsg, FO_API_RET(void), FO_API_ARG(ushort, hexX), FO_API_ARG(ushort, hexY), FO_API_ARG(uint, color), FO_API_ARG(ushort, textMsg), FO_API_ARG(uint, strNum))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hexX) FO_API_ARG_MARSHAL(ushort, hexY) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(ushort, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (hexX >= _map->GetWidth() || hexY >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    _map->SetTextMsg(hexX, hexY, color, textMsg, strNum);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param hexX ...
 * @param hexY ...
 * @param color ...
 * @param textMsg ...
 * @param strNum ...
 * @param lexems ...
 ******************************************************************************/
FO_API_MAP_METHOD(SetTextMsgLex, FO_API_RET(void), FO_API_ARG(ushort, hexX), FO_API_ARG(ushort, hexY), FO_API_ARG(uint, color), FO_API_ARG(ushort, textMsg), FO_API_ARG(uint, strNum), FO_API_ARG(string, lexems))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hexX) FO_API_ARG_MARSHAL(ushort, hexY) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(ushort, textMsg) FO_API_ARG_MARSHAL(uint, strNum) FO_API_ARG_MARSHAL(string, lexems))
{
    if (hexX >= _map->GetWidth() || hexY >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    _map->SetTextMsgLex(hexX, hexY, color, textMsg, strNum, lexems.c_str(), static_cast<ushort>(lexems.length()));
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param effPid ...
 * @param hx ...
 * @param hy ...
 * @param radius ...
 ******************************************************************************/
FO_API_MAP_METHOD(RunEffect, FO_API_RET(void), FO_API_ARG(hash, effPid), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, radius))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, effPid) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, radius))
{
    if (effPid == 0u) {
        throw ScriptException("Effect pid invalid arg");
    }
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    _map->SendEffect(effPid, hx, hy, static_cast<ushort>(radius));
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param effPid ...
 * @param fromCr ...
 * @param toCr ...
 * @param fromHx ...
 * @param fromHy ...
 * @param toHx ...
 * @param toHy ...
 ******************************************************************************/
FO_API_MAP_METHOD(RunFlyEffect, FO_API_RET(void), FO_API_ARG(hash, effPid), FO_API_ARG_OBJ(Critter, fromCr), FO_API_ARG_OBJ(Critter, toCr), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, effPid) FO_API_ARG_OBJ_MARSHAL(Critter, fromCr) FO_API_ARG_OBJ_MARSHAL(Critter, toCr) FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy))
{
    if (effPid == 0u) {
        throw ScriptException("Effect pid invalid arg");
    }
    if (fromHx >= _map->GetWidth() || fromHy >= _map->GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= _map->GetWidth() || toHy >= _map->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    const auto from_crid = (fromCr != nullptr ? fromCr->GetId() : 0u);
    const auto to_crid = (toCr != nullptr ? toCr->GetId() : 0u);
    _map->SendFlyEffect(effPid, from_crid, to_crid, fromHx, fromHy, toHx, toHy);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(CheckPlaceForItem, FO_API_RET(bool), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(hash, pid))
{
    const auto* proto_item = _server->ProtoMngr.GetProtoItem(pid);
    if (!proto_item) {
        throw ScriptException("Proto item not found");
    }

    FO_API_RETURN(_map->IsPlaceForProtoItem(hx, hy, proto_item));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param full ...
 ******************************************************************************/
FO_API_MAP_METHOD(BlockHex, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, full))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(bool, full))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    _map->SetHexFlag(hx, hy, FH_BLOCK_ITEM);
    if (full) {
        _map->SetHexFlag(hx, hy, FH_NRAKE_ITEM);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 ******************************************************************************/
FO_API_MAP_METHOD(UnblockHex, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    _map->UnsetHexFlag(hx, hy, FH_BLOCK_ITEM);
    _map->UnsetHexFlag(hx, hy, FH_NRAKE_ITEM);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param soundName ...
 ******************************************************************************/
FO_API_MAP_METHOD(PlaySound, FO_API_RET(void), FO_API_ARG(string, soundName))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, soundName))
{
    for (Critter* cr : _map->GetPlayers()) {
        cr->Send_PlaySound(0, soundName);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param soundName ...
 * @param hx ...
 * @param hy ...
 * @param radius ...
 ******************************************************************************/
FO_API_MAP_METHOD(PlaySoundInRadius, FO_API_RET(void), FO_API_ARG(string, soundName), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, radius))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, soundName) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, radius))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    for (Critter* cr : _map->GetPlayers()) {
        if (_server->GeomHelper.CheckDist(hx, hy, cr->GetHexX(), cr->GetHexY(), radius == 0 ? cr->LookCacheValue : radius)) {
            cr->Send_PlaySound(0, soundName);
        }
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_MAP_METHOD(Regenerate, FO_API_RET(void))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG()
{
    _server->MapMngr.RegenerateMap(_map);
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
FO_API_MAP_METHOD(MoveHexByDir, FO_API_RET(void), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy), FO_API_ARG(uchar, dir), FO_API_ARG(uint, steps))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, hx) FO_API_ARG_REF_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uchar, dir) FO_API_ARG_MARSHAL(uint, steps))
{
    if (dir >= _server->Settings.MapDirCount) {
        throw ScriptException("Invalid dir arg");
    }
    if (steps == 0u) {
        throw ScriptException("Steps arg is zero");
    }

    const auto maxhx = _map->GetWidth();
    const auto maxhy = _map->GetHeight();

    if (steps > 1) {
        for (uint i = 0; i < steps; i++) {
            _server->GeomHelper.MoveHexByDir(hx, hy, dir, maxhx, maxhy);
        }
    }
    else {
        _server->GeomHelper.MoveHexByDir(hx, hy, dir, maxhx, maxhy);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @param hx ...
 * @param hy ...
 * @param dir ...
 ******************************************************************************/
FO_API_MAP_METHOD(VerifyTrigger, FO_API_RET(void), FO_API_ARG_OBJ(Critter, cr), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uchar, dir))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uchar, dir))
{
    if (hx >= _map->GetWidth() || hy >= _map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }
    if (dir >= _server->Settings.MapDirCount) {
        throw ScriptException("Invalid dir arg");
    }

    auto from_hx = hx;
    auto from_hy = hy;
    _server->GeomHelper.MoveHexByDir(from_hx, from_hy, _server->GeomHelper.ReverseDir(dir), _map->GetWidth(), _map->GetHeight());

    _server->VerifyTrigger(_map, cr, from_hx, from_hy, hx, hy, dir);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_LOCATION_METHOD(GetMapCount, FO_API_RET(uint))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_location->GetMapsCount());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param mapPid ...
 * @return ...
 ******************************************************************************/
FO_API_LOCATION_METHOD(GetMap, FO_API_RET_OBJ(Map), FO_API_ARG(hash, mapPid))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, mapPid))
{
    for (auto* map : _location->GetMaps()) {
        if (map->GetProtoId() == mapPid) {
            FO_API_RETURN(map);
        }
    }
    FO_API_RETURN(static_cast<Map*>(nullptr));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param index ...
 * @return ...
 ******************************************************************************/
FO_API_LOCATION_METHOD(GetMapByIndex, FO_API_RET_OBJ(Map), FO_API_ARG(uint, index))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, index))
{
    auto maps = _location->GetMaps();
    if (index >= maps.size()) {
        throw ScriptException("Invalid index arg");
    }

    FO_API_RETURN(maps[index]);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_LOCATION_METHOD(GetMaps, FO_API_RET_OBJ_ARR(Map))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_location->GetMaps());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param entranceIndex ...
 * @param mapIndex ...
 * @param entrance ...
 * @return ...
 ******************************************************************************/
FO_API_LOCATION_METHOD(GetEntrance, FO_API_RET(void), FO_API_ARG(uint, entranceIndex), FO_API_ARG_REF(uint, mapIndex), FO_API_ARG_REF(hash, entrance))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, entranceIndex) FO_API_ARG_REF_MARSHAL(uint, mapIndex) FO_API_ARG_REF_MARSHAL(hash, entrance))
{
    const auto map_entrances = _location->GetMapEntrances();
    const auto count = static_cast<uint>(map_entrances.size()) / 2u;
    if (entranceIndex >= count) {
        throw ScriptException("Invalid entrance");
    }

    mapIndex = _location->GetMapIndex(map_entrances[entranceIndex * 2]);
    entrance = map_entrances[entranceIndex * 2 + 1];
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param mapsIndex ...
 * @param entrances ...
 * @return ...
 ******************************************************************************/
FO_API_LOCATION_METHOD(GetEntrances, FO_API_RET(uint), FO_API_ARG_ARR_REF(int, mapsIndex), FO_API_ARG_ARR_REF(hash, entrances))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_ARR_REF_MARSHAL(int, mapsIndex) FO_API_ARG_ARR_REF_MARSHAL(hash, entrances))
{
    auto map_entrances = _location->GetMapEntrances();
    const auto count = static_cast<uint>(map_entrances.size()) / 2u;

    for (const auto i : xrange(count)) {
        int index = _location->GetMapIndex(map_entrances[i * 2]);
        mapsIndex.push_back(index);
        auto entrance = map_entrances[i * 2 + 1];
        entrances.push_back(entrance);
    }

    FO_API_RETURN(count);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_LOCATION_METHOD(Regenerate, FO_API_RET(void))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG()
{
    for (auto* map : _location->GetMaps()) {
        _server->MapMngr.RegenerateMap(map);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param cr1 ...
 * @param cr2 ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetCritterDistance, FO_API_RET(int), FO_API_ARG_OBJ(Critter, cr1), FO_API_ARG_OBJ(Critter, cr2))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr1) FO_API_ARG_OBJ_MARSHAL(Critter, cr2))
{
    if (cr1 == nullptr) {
        throw ScriptException("Critter1 arg is null");
    }
    if (cr2 == nullptr) {
        throw ScriptException("Critter2 arg is null");
    }
    if (cr1->GetMapId() != cr2->GetMapId()) {
        throw ScriptException("Differernt maps");
    }

    const auto dist = _server->GeomHelper.DistGame(cr1->GetHexX(), cr1->GetHexY(), cr2->GetHexX(), cr2->GetHexY());
    FO_API_RETURN(static_cast<int>(dist));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param itemId ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetItem, FO_API_RET_OBJ(Item), FO_API_ARG(uint, itemId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId))
{
    if (itemId == 0u) {
        throw ScriptException("Item id arg is zero");
    }

    auto* item = _server->ItemMngr.GetItem(itemId);
    if (!item || item->IsDestroyed) {
        FO_API_RETURN(static_cast<Item*>(nullptr));
    }

    FO_API_RETURN(item);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param item ...
 * @param count ...
 * @param toCr ...
 * @param skipChecks ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(MoveItemToCritter, FO_API_RET(void), FO_API_ARG_OBJ(Item, item), FO_API_ARG(uint, count), FO_API_ARG_OBJ(Critter, toCr), FO_API_ARG(bool, skipChecks))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Item, item) FO_API_ARG_MARSHAL(uint, count) FO_API_ARG_OBJ_MARSHAL(Critter, toCr) FO_API_ARG_MARSHAL(bool, skipChecks))
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    auto count_ = count;
    if (count_ == 0u) {
        count_ = item->GetCount();
    }
    if (count_ > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    _server->ItemMngr.MoveItem(item, count_, toCr, skipChecks);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param item ...
 * @param count ...
 * @param toMap ...
 * @param toHx ...
 * @param toHy ...
 * @param skipChecks ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(MoveItemToMap, FO_API_RET(void), FO_API_ARG_OBJ(Item, item), FO_API_ARG(uint, count), FO_API_ARG_OBJ(Map, toMap), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(bool, skipChecks))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Item, item) FO_API_ARG_MARSHAL(uint, count) FO_API_ARG_OBJ_MARSHAL(Map, toMap) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(bool, skipChecks))
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (toHx >= toMap->GetWidth() || toHy >= toMap->GetHeight()) {
        throw ScriptException("Invalid hexex args");
    }

    auto count_ = count;
    if (count_ == 0u) {
        count_ = item->GetCount();
    }
    if (count_ > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    _server->ItemMngr.MoveItem(item, count_, toMap, toHx, toHy, skipChecks);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param item ...
 * @param count ...
 * @param toCont ...
 * @param stackId ...
 * @param skipChecks ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(MoveItemToContainer, FO_API_RET(void), FO_API_ARG_OBJ(Item, item), FO_API_ARG(uint, count), FO_API_ARG_OBJ(Item, toCont), FO_API_ARG(uint, stackId), FO_API_ARG(bool, skipChecks))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Item, item) FO_API_ARG_MARSHAL(uint, count) FO_API_ARG_OBJ_MARSHAL(Item, toCont) FO_API_ARG_MARSHAL(uint, stackId) FO_API_ARG_MARSHAL(bool, skipChecks))
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    auto count_ = count;
    if (count_ == 0u) {
        count_ = item->GetCount();
    }
    if (count_ > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    _server->ItemMngr.MoveItem(item, count_, toCont, stackId, skipChecks);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param items ...
 * @param toCr ...
 * @param skipChecks ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(MoveItemsToCritter, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Item, items), FO_API_ARG_OBJ(Critter, toCr), FO_API_ARG(bool, skipChecks))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Item, items) FO_API_ARG_OBJ_MARSHAL(Critter, toCr) FO_API_ARG_MARSHAL(bool, skipChecks))
{
    if (toCr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed) {
            continue;
        }

        _server->ItemMngr.MoveItem(item, item->GetCount(), toCr, skipChecks);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param items ...
 * @param toMap ...
 * @param toHx ...
 * @param toHy ...
 * @param skipChecks ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(MoveItemsToMap, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Item, items), FO_API_ARG_OBJ(Map, toMap), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(bool, skipChecks))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Item, items) FO_API_ARG_OBJ_MARSHAL(Map, toMap) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(bool, skipChecks))
{
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (toHx >= toMap->GetWidth() || toHy >= toMap->GetHeight()) {
        throw ScriptException("Invalid hexex args");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed) {
            continue;
        }

        _server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHx, toHy, skipChecks);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param items ...
 * @param toCont ...
 * @param stackId ...
 * @param skipChecks ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(MoveItemsToContainer, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Item, items), FO_API_ARG_OBJ(Item, toCont), FO_API_ARG(uint, stackId), FO_API_ARG(bool, skipChecks))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Item, items) FO_API_ARG_OBJ_MARSHAL(Item, toCont) FO_API_ARG_MARSHAL(uint, stackId) FO_API_ARG_MARSHAL(bool, skipChecks))
{
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed) {
            continue;
        }

        _server->ItemMngr.MoveItem(item, item->GetCount(), toCont, stackId, skipChecks);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param item ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(DeleteItem, FO_API_RET(void), FO_API_ARG_OBJ(Item, item))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Item, item))
{
    if (item != nullptr) {
        _server->ItemMngr.DeleteItem(item);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param itemId ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(DeleteItemById, FO_API_RET(void), FO_API_ARG(uint, itemId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId))
{
    auto* item = _server->ItemMngr.GetItem(itemId);
    if (item) {
        _server->ItemMngr.DeleteItem(item);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param items ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(DeleteItems, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Item, items))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Item, items))
{
    for (auto* item : items) {
        if (item != nullptr) {
            _server->ItemMngr.DeleteItem(item);
        }
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param itemIds ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(DeleteItemsById, FO_API_RET(void), FO_API_ARG_ARR(uint, itemIds))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_ARR_MARSHAL(uint, itemIds))
{
    ItemVec items_to_delete;
    for (auto item_id : itemIds) {
        if (item_id != 0u) {
            auto* item = _server->ItemMngr.GetItem(item_id);
            if (item != nullptr) {
                _server->ItemMngr.DeleteItem(item);
            }
        }
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param npc ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(DeleteNpc, FO_API_RET(void), FO_API_ARG_OBJ(Critter, npc))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, npc))
{
    if (npc != nullptr) {
        _server->CrMngr.DeleteNpc(npc);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param npcId ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(DeleteNpcById, FO_API_RET(void), FO_API_ARG(uint, npcId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, npcId))
{
    Critter* npc = _server->CrMngr.GetNpc(npcId);
    if (npc != nullptr) {
        _server->CrMngr.DeleteNpc(npc);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param channel ...
 * @param text ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(RadioMessage, FO_API_RET(void), FO_API_ARG(ushort, channel), FO_API_ARG(string, text))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, channel) FO_API_ARG_MARSHAL(string, text))
{
    if (!text.empty()) {
        _server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, text, false, 0, 0, nullptr);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param channel ...
 * @param textMsg ...
 * @param numStr ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(RadioMessageMsg, FO_API_RET(void), FO_API_ARG(ushort, channel), FO_API_ARG(ushort, textMsg), FO_API_ARG(uint, numStr))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, channel) FO_API_ARG_MARSHAL(ushort, textMsg) FO_API_ARG_MARSHAL(uint, numStr))
{
    _server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, "", false, textMsg, numStr, nullptr);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param channel ...
 * @param textMsg ...
 * @param numStr ...
 * @param lexems ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(RadioMessageMsgLex, FO_API_RET(void), FO_API_ARG(ushort, channel), FO_API_ARG(ushort, textMsg), FO_API_ARG(uint, numStr), FO_API_ARG(string, lexems))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, channel) FO_API_ARG_MARSHAL(ushort, textMsg) FO_API_ARG_MARSHAL(uint, numStr) FO_API_ARG_MARSHAL(string, lexems))
{
    _server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, "", false, textMsg, numStr, !lexems.empty() ? lexems.c_str() : nullptr);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetFullSecond, FO_API_RET(uint))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_server->GameTime.GetFullSecond());
}
FO_API_EPILOG(0)
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
FO_API_GLOBAL_SERVER_FUNC(EvaluateFullSecond, FO_API_RET(uint), FO_API_ARG(ushort, year), FO_API_ARG(ushort, month), FO_API_ARG(ushort, day), FO_API_ARG(ushort, hour), FO_API_ARG(ushort, minute), FO_API_ARG(ushort, second))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, year) FO_API_ARG_MARSHAL(ushort, month) FO_API_ARG_MARSHAL(ushort, day) FO_API_ARG_MARSHAL(ushort, hour) FO_API_ARG_MARSHAL(ushort, minute) FO_API_ARG_MARSHAL(ushort, second))
{
    if (year && year < _server->Settings.StartYear) {
        throw ScriptException("Invalid year", year);
    }
    if (year && year > _server->Settings.StartYear + 100) {
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
        year_ = _server->Globals->GetYear();
    }
    if (month_ == 0u) {
        month_ = _server->Globals->GetMonth();
    }

    if (day_ != 0u) {
        const auto month_day = _server->GameTime.GameTimeMonthDay(year, month_);
        if (day_ < month_day || day_ > month_day) {
            throw ScriptException("Invalid day", day_, month_day);
        }
    }

    if (day_ == 0u) {
        day_ = _server->Globals->GetDay();
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

    FO_API_RETURN(_server->GameTime.EvaluateFullSecond(year_, month_, day_, hour, minute, second));
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
FO_API_GLOBAL_SERVER_FUNC(GetGameTime, FO_API_RET(void), FO_API_ARG(uint, fullSecond), FO_API_ARG_REF(ushort, year), FO_API_ARG_REF(ushort, month), FO_API_ARG_REF(ushort, day), FO_API_ARG_REF(ushort, dayOfWeek), FO_API_ARG_REF(ushort, hour), FO_API_ARG_REF(ushort, minute), FO_API_ARG_REF(ushort, second))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, fullSecond) FO_API_ARG_REF_MARSHAL(ushort, year) FO_API_ARG_REF_MARSHAL(ushort, month) FO_API_ARG_REF_MARSHAL(ushort, day) FO_API_ARG_REF_MARSHAL(ushort, dayOfWeek) FO_API_ARG_REF_MARSHAL(ushort, hour) FO_API_ARG_REF_MARSHAL(ushort, minute) FO_API_ARG_REF_MARSHAL(ushort, second))
{
    const auto dt = _server->GameTime.GetGameTime(fullSecond);

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
 * @param locPid ...
 * @param wx ...
 * @param wy ...
 * @param critters ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(CreateLocation, FO_API_RET_OBJ(Location), FO_API_ARG(hash, locPid), FO_API_ARG(ushort, wx), FO_API_ARG(ushort, wy), FO_API_ARG_OBJ_ARR(Critter, critters))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, locPid) FO_API_ARG_MARSHAL(ushort, wx) FO_API_ARG_MARSHAL(ushort, wy) FO_API_ARG_OBJ_ARR_MARSHAL(Critter, critters))
{
    // Create and generate location
    auto* loc = _server->MapMngr.CreateLocation(locPid, wx, wy);
    if (!loc) {
        throw ScriptException("Unable to create location '{}'.", _str().parseHash(locPid));
    }

    // Add known locations to critters
    for (auto* cr : critters) {
        _server->MapMngr.AddKnownLoc(cr, loc->GetId());

        if (cr->GetMapId() == 0u) {
            cr->Send_GlobalLocation(loc, true);
        }
        if (loc->IsNonEmptyAutomaps()) {
            cr->Send_AutomapsInfo(nullptr, loc);
        }

        ushort zx = loc->GetWorldX() / _server->Settings.GlobalMapZoneLength;
        ushort zy = loc->GetWorldY() / _server->Settings.GlobalMapZoneLength;

        auto gmap_fog = cr->GetGlobalMapFog();
        if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
            gmap_fog.resize(GM_ZONES_FOG_SIZE);
        }

        /*TwoBitMask gmap_mask(GM_MAXZONEX, GM_MAXZONEY, gmap_fog.data());
        if (gmap_mask.Get2Bit(zx, zy) == GM_FOG_FULL) {
            gmap_mask.Set2Bit(zx, zy, GM_FOG_HALF);
            cr->SetGlobalMapFog(gmap_fog);
            if (!cr->GetMapId())
                cr->Send_GlobalMapFog(zx, zy, GM_FOG_HALF);
        }*/
    }

    FO_API_RETURN(loc);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param loc ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(DeleteLocation, FO_API_RET(void), FO_API_ARG_OBJ(Location, loc))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Location, loc))
{
    if (loc != nullptr) {
        _server->MapMngr.DeleteLocation(loc, nullptr);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param locId ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(DeleteLocationById, FO_API_RET(void), FO_API_ARG(uint, locId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, locId))
{
    auto* loc = _server->MapMngr.GetLocation(locId);
    if (loc) {
        _server->MapMngr.DeleteLocation(loc, nullptr);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param crId ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetCritter, FO_API_RET_OBJ(Critter), FO_API_ARG(uint, crId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, crId))
{
    if (crId == 0u) {
        FO_API_RETURN(static_cast<Critter*>(nullptr));
    }

    FO_API_RETURN(_server->CrMngr.GetCritter(crId));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @param name ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetPlayer, FO_API_RET_OBJ(Critter), FO_API_ARG(string, name))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name))
{
    // Check existence
    const auto id = MAKE_CLIENT_ID(name);
    const auto doc = _server->DbStorage->Get("Players", id);
    if (doc.empty()) {
        FO_API_RETURN(static_cast<Critter*>(nullptr));
    }

    // Find online
    auto* cl = _server->CrMngr.GetPlayer(id);
    if (cl) {
        cl->AddRef();
        FO_API_RETURN(cl);
    }

    // Load from db
    const auto* cl_proto = _server->ProtoMngr.GetProtoCritter(_str("Player").toHash());
    RUNTIME_ASSERT(cl_proto);

    cl = new Client(nullptr, cl_proto, _server->Settings, _server->ScriptSys, _server->GameTime);
    cl->SetId(id);
    cl->Name = name;

    if (!PropertiesSerializator::LoadFromDbDocument(&cl->Props, doc, _server->ScriptSys)) {
        throw ScriptException("Client data db read failed");
    }

    FO_API_RETURN(cl);
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param wx ...
 * @param wy ...
 * @param radius ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetGlobalMapCritters, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(ushort, wx), FO_API_ARG(ushort, wy), FO_API_ARG(uint, radius), FO_API_ARG(int, findType))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, wx) FO_API_ARG_MARSHAL(ushort, wy) FO_API_ARG_MARSHAL(uint, radius) FO_API_ARG_MARSHAL(int, findType))
{
    CritterVec critters;
    _server->CrMngr.GetGlobalMapCritters(wx, wy, radius, findType, critters);
    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param mapId ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetMap, FO_API_RET_OBJ(Map), FO_API_ARG(uint, mapId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, mapId))
{
    if (mapId == 0u) {
        throw ScriptException("Map id arg is zero");
    }

    FO_API_RETURN(_server->MapMngr.GetMap(mapId));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param mapPid ...
 * @param skipCount ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetMapByPid, FO_API_RET_OBJ(Map), FO_API_ARG(hash, mapPid), FO_API_ARG(uint, skipCount))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, mapPid) FO_API_ARG_MARSHAL(uint, skipCount))
{
    if (mapPid == 0u) {
        throw ScriptException("Invalid zero map proto id arg");
    }

    FO_API_RETURN(_server->MapMngr.GetMapByPid(mapPid, skipCount));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param locId ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetLocation, FO_API_RET_OBJ(Location), FO_API_ARG(uint, locId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, locId))
{
    if (locId == 0u) {
        throw ScriptException("Location id arg is zero");
    }

    FO_API_RETURN(_server->MapMngr.GetLocation(locId));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param locPid ...
 * @param skipCount ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetLocationByPid, FO_API_RET_OBJ(Location), FO_API_ARG(hash, locPid), FO_API_ARG(uint, skipCount))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, locPid) FO_API_ARG_MARSHAL(uint, skipCount))
{
    if (locPid == 0u) {
        throw ScriptException("Invalid zero location proto id arg");
    }

    FO_API_RETURN(_server->MapMngr.GetLocationByPid(locPid, skipCount));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param wx ...
 * @param wy ...
 * @param radius ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetLocationsAroundPos, FO_API_RET_OBJ_ARR(Location), FO_API_ARG(ushort, wx), FO_API_ARG(ushort, wy), FO_API_ARG(uint, radius))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, wx) FO_API_ARG_MARSHAL(ushort, wy) FO_API_ARG_MARSHAL(uint, radius))
{
    LocationVec locations;
    LocationVec all_locations;
    _server->MapMngr.GetLocations(all_locations);
    for (auto* loc : all_locations) {
        if (GenericUtils::DistSqrt(wx, wy, loc->GetWorldX(), loc->GetWorldY()) <= radius + loc->GetRadius()) {
            locations.push_back(loc);
        }
    }

    FO_API_RETURN(locations);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param wx ...
 * @param wy ...
 * @param radius ...
 * @param cr ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetVisibleLocationsAroundPos, FO_API_RET_OBJ_ARR(Location), FO_API_ARG(ushort, wx), FO_API_ARG(ushort, wy), FO_API_ARG(uint, radius), FO_API_ARG_OBJ(Critter, cr))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, wx) FO_API_ARG_MARSHAL(ushort, wy) FO_API_ARG_MARSHAL(uint, radius) FO_API_ARG_OBJ_MARSHAL(Critter, cr))
{
    LocationVec locations;
    LocationVec all_locations;
    _server->MapMngr.GetLocations(all_locations);
    for (auto* loc : all_locations) {
        if (GenericUtils::DistSqrt(wx, wy, loc->GetWorldX(), loc->GetWorldY()) <= radius + loc->GetRadius() && (loc->IsLocVisible() || (cr && cr->IsPlayer() && _server->MapMngr.CheckKnownLocById(cr, loc->GetId())))) {
            locations.push_back(loc);
        }
    }

    FO_API_RETURN(locations);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param zx ...
 * @param zy ...
 * @param zoneRadius ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetZoneLocationIds, FO_API_RET_ARR(uint), FO_API_ARG(ushort, zx), FO_API_ARG(ushort, zy), FO_API_ARG(uint, zoneRadius))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, zx) FO_API_ARG_MARSHAL(ushort, zy) FO_API_ARG_MARSHAL(uint, zoneRadius))
{
    UIntVec loc_ids;
    _server->MapMngr.GetZoneLocations(zx, zy, zoneRadius, loc_ids);
    FO_API_RETURN(loc_ids);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param player ...
 * @param npc ...
 * @param ignoreDistance ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(RunNpcDialog, FO_API_RET(bool), FO_API_ARG_OBJ(Critter, player), FO_API_ARG_OBJ(Critter, npc), FO_API_ARG(bool, ignoreDistance))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, player) FO_API_ARG_OBJ_MARSHAL(Critter, npc) FO_API_ARG_MARSHAL(bool, ignoreDistance))
{
    if (player == nullptr) {
        throw ScriptException("Player arg is null");
    }
    if (!player->IsPlayer()) {
        throw ScriptException("Player arg is not player");
    }
    if (npc == nullptr) {
        throw ScriptException("Npc arg is null");
    }
    if (!npc->IsNpc()) {
        throw ScriptException("Npc arg is not npc");
    }

    auto* cl = dynamic_cast<Client*>(player);
    if (cl->Talk.Locked) {
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");
    }

    _server->Dialog_Begin(cl, dynamic_cast<Npc*>(npc), 0, 0, 0, ignoreDistance);

    FO_API_RETURN(cl->Talk.Type == TalkType::Npc && cl->Talk.TalkNpc == npc->GetId());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param player ...
 * @param npc ...
 * @param dlgPack ...
 * @param ignoreDistance ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(RunCustomNpcDialog, FO_API_RET(bool), FO_API_ARG_OBJ(Critter, player), FO_API_ARG_OBJ(Critter, npc), FO_API_ARG(uint, dlgPack), FO_API_ARG(bool, ignoreDistance))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, player) FO_API_ARG_OBJ_MARSHAL(Critter, npc) FO_API_ARG_MARSHAL(uint, dlgPack) FO_API_ARG_MARSHAL(bool, ignoreDistance))
{
    if (player == nullptr) {
        throw ScriptException("Player arg is null");
    }
    if (!player->IsPlayer()) {
        throw ScriptException("Player arg is not player");
    }
    if (npc == nullptr) {
        throw ScriptException("Npc arg is null");
    }
    if (!npc->IsNpc()) {
        throw ScriptException("Npc arg is not npc");
    }

    auto* cl = dynamic_cast<Client*>(player);
    if (cl->Talk.Locked) {
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");
    }

    _server->Dialog_Begin(cl, dynamic_cast<Npc*>(npc), dlgPack, 0, 0, ignoreDistance);

    FO_API_RETURN(cl->Talk.Type == TalkType::Npc && cl->Talk.TalkNpc == npc->GetId());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param player ...
 * @param dlgPack ...
 * @param hx ...
 * @param hy ...
 * @param ignoreDistance ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(RunCustomDialogOnHex, FO_API_RET(bool), FO_API_ARG_OBJ(Critter, player), FO_API_ARG(uint, dlgPack), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, ignoreDistance))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, player) FO_API_ARG_MARSHAL(uint, dlgPack) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(bool, ignoreDistance))
{
    if (player == nullptr) {
        throw ScriptException("Player arg is null");
    }
    if (!player->IsPlayer()) {
        throw ScriptException("Player arg is not player");
    }
    if (!_server->DlgMngr.GetDialog(dlgPack)) {
        throw ScriptException("Dialog not found");
    }

    auto* cl = dynamic_cast<Client*>(player);
    if (cl->Talk.Locked) {
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");
    }

    _server->Dialog_Begin(cl, nullptr, dlgPack, hx, hy, ignoreDistance);

    FO_API_RETURN(cl->Talk.Type == TalkType::Hex && cl->Talk.TalkHexX == hx && cl->Talk.TalkHexY == hy);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetWorldItemCount, FO_API_RET(int64), FO_API_ARG(hash, pid))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    if (!_server->ProtoMngr.GetProtoItem(pid)) {
        throw ScriptException("Invalid protoId arg");
    }

    FO_API_RETURN(_server->ItemMngr.GetItemStatistics(pid));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param sayType ...
 * @param firstStr ...
 * @param parameter ...
 * @param func ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(AddTextListener, FO_API_RET(void), FO_API_ARG(int, sayType), FO_API_ARG(string, firstStr), FO_API_ARG(uint, parameter), FO_API_ARG_CALLBACK(Entity, func))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, sayType) FO_API_ARG_MARSHAL(string, firstStr) FO_API_ARG_MARSHAL(uint, parameter) FO_API_ARG_CALLBACK_MARSHAL(Entity, func))
{
    /*if (firstStr.length() > TEXT_LISTEN_FIRST_STR_MAX_LEN)
        throw ScriptException("First string arg length greater than maximum");

    uint func_id = _server->ScriptSys.BindByFunc(func, false);
    if (!func_id)
        throw ScriptException("Unable to bind script function");

    TextListen tl;
    tl.FuncId = func_id;
    tl.SayType = sayType;
    tl.FirstStr = firstStr;
    tl.Parameter = parameter;

    SCOPE_LOCK(_server->TextListenersLocker);

    _server->TextListeners.push_back(tl);*/
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param sayType ...
 * @param firstStr ...
 * @param parameter ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(EraseTextListener, FO_API_RET(void), FO_API_ARG(int, sayType), FO_API_ARG(string, firstStr), FO_API_ARG(uint, parameter))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, sayType) FO_API_ARG_MARSHAL(string, firstStr) FO_API_ARG_MARSHAL(uint, parameter))
{
    /*SCOPE_LOCK(_server->TextListenersLocker);

    for (auto it = _server->TextListeners.begin(), end = _server->TextListeners.end(); it != end; ++it)
    {
        TextListen& tl = *it;
        if (sayType == tl.SayType && _str(firstStr).compareIgnoreCaseUtf8(tl.FirstStr) && tl.Parameter == parameter)
        {
            _server->TextListeners.erase(it);
            FO_API_RETURN_VOID();
        }
    }*/
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
static void SwapCrittersRefreshNpc(Npc* npc)
{
    UnsetBit(npc->Flags, FCRIT_PLAYER);
    SetBit(npc->Flags, FCRIT_NPC);
}
#endif
/*******************************************************************************
 * ...
 *
 * @param cr1 ...
 * @param cr2 ...
 * @param withInventory ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(SwapCritters, FO_API_RET(void), FO_API_ARG_OBJ(Critter, cr1), FO_API_ARG_OBJ(Critter, cr2), FO_API_ARG(bool, withInventory))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr1) FO_API_ARG_OBJ_MARSHAL(Critter, cr2) FO_API_ARG_MARSHAL(bool, withInventory))
{
    // Check
    if (cr1 == nullptr) {
        throw ScriptException("Critter1 arg is null");
    }
    if (cr2 == nullptr) {
        throw ScriptException("Critter2 arg is null");
    }
    if (cr1 == cr2) {
        throw ScriptException("Critter1 is equal to Critter2");
    }
    if (cr1->GetMapId() == 0u) {
        throw ScriptException("Critter1 is on global map");
    }
    if (cr2->GetMapId() == 0u) {
        throw ScriptException("Critter2 is on global map");
    }

    // Swap positions
    auto* map1 = _server->MapMngr.GetMap(cr1->GetMapId());
    if (!map1) {
        throw ScriptException("Map of Critter1 not found");
    }
    auto* map2 = _server->MapMngr.GetMap(cr2->GetMapId());
    if (!map2) {
        throw ScriptException("Map of Critter2 not found");
    }

    auto& cr_map1 = map1->GetCrittersRaw();
    auto& cl_map1 = map1->GetPlayersRaw();
    auto& npc_map1 = map1->GetNpcsRaw();
    auto it_cr = std::find(cr_map1.begin(), cr_map1.end(), cr1);
    if (it_cr != cr_map1.end()) {
        cr_map1.erase(it_cr);
    }
    auto it_cl = std::find(cl_map1.begin(), cl_map1.end(), dynamic_cast<Client*>(cr1));
    if (it_cl != cl_map1.end()) {
        cl_map1.erase(it_cl);
    }
    auto it_pc = std::find(npc_map1.begin(), npc_map1.end(), dynamic_cast<Npc*>(cr1));
    if (it_pc != npc_map1.end()) {
        npc_map1.erase(it_pc);
    }

    auto& cr_map2 = map2->GetCrittersRaw();
    auto& cl_map2 = map2->GetPlayersRaw();
    auto& npc_map2 = map2->GetNpcsRaw();
    it_cr = std::find(cr_map2.begin(), cr_map2.end(), cr1);
    if (it_cr != cr_map2.end()) {
        cr_map2.erase(it_cr);
    }
    it_cl = std::find(cl_map2.begin(), cl_map2.end(), dynamic_cast<Client*>(cr1));
    if (it_cl != cl_map2.end()) {
        cl_map2.erase(it_cl);
    }
    it_pc = std::find(npc_map2.begin(), npc_map2.end(), dynamic_cast<Npc*>(cr1));
    if (it_pc != npc_map2.end()) {
        npc_map2.erase(it_pc);
    }

    cr_map2.push_back(cr1);
    if (cr1->IsNpc()) {
        npc_map2.push_back(dynamic_cast<Npc*>(cr1));
    }
    else {
        cl_map2.push_back(dynamic_cast<Client*>(cr1));
    }

    cr_map1.push_back(cr2);

    if (cr2->IsNpc()) {
        npc_map1.push_back(dynamic_cast<Npc*>(cr2));
    }
    else {
        cl_map1.push_back(dynamic_cast<Client*>(cr2));
    }

    // Swap data
    // std::swap(cr1->Props, cr2->Props);
    std::swap(cr1->Flags, cr2->Flags);

    cr1->SetBreakTime(0);
    cr2->SetBreakTime(0);

    // Swap inventory
    if (withInventory) {
        auto items1 = cr1->GetInventory();
        auto items2 = cr2->GetInventory();
        for (auto it = items1.begin(), end = items1.end(); it != end; ++it) {
            _server->CrMngr.EraseItemFromCritter(cr1, *it, false);
        }
        for (auto it = items2.begin(), end = items2.end(); it != end; ++it) {
            _server->CrMngr.EraseItemFromCritter(cr2, *it, false);
        }
        for (auto it = items1.begin(), end = items1.end(); it != end; ++it) {
            _server->CrMngr.AddItemToCritter(cr2, *it, false);
        }
        for (auto it = items2.begin(), end = items2.end(); it != end; ++it) {
            _server->CrMngr.AddItemToCritter(cr1, *it, false);
        }
    }

    // Refresh
    cr1->ClearVisible();
    cr2->ClearVisible();

    auto swap_critters_refresh_client = [_server](Client* cl, Map* map, Map* prev_map) {
        UnsetBit(cl->Flags, FCRIT_NPC);
        SetBit(cl->Flags, FCRIT_PLAYER);

        if (cl->Talk.Type != TalkType::None) {
            _server->CrMngr.CloseTalk(cl);
        }

        if (map != prev_map) {
            cl->Send_LoadMap(nullptr, _server->MapMngr);
        }
        else {
            cl->Send_AllProperties();
            cl->Send_AddAllItems();
            cl->Send_AllAutomapsInfo(_server->MapMngr);
        }
    };

    if (cr1->IsNpc()) {
        SwapCrittersRefreshNpc(dynamic_cast<Npc*>(cr1));
    }
    else {
        swap_critters_refresh_client(dynamic_cast<Client*>(cr1), map2, map1);
    }

    if (cr2->IsNpc()) {
        SwapCrittersRefreshNpc(dynamic_cast<Npc*>(cr2));
    }
    else {
        swap_critters_refresh_client(dynamic_cast<Client*>(cr2), map1, map2);
    }

    if (map1 == map2) {
        cr1->Send_CustomCommand(cr1, OTHER_CLEAR_MAP, 0);
        cr2->Send_CustomCommand(cr2, OTHER_CLEAR_MAP, 0);
        cr1->Send_Dir(cr1);
        cr2->Send_Dir(cr2);
        cr1->Send_CustomCommand(cr1, OTHER_TELEPORT, (cr1->GetHexX() << 16) | (cr1->GetHexY()));
        cr2->Send_CustomCommand(cr2, OTHER_TELEPORT, (cr2->GetHexX() << 16) | (cr2->GetHexY()));
        _server->MapMngr.ProcessVisibleCritters(cr1);
        _server->MapMngr.ProcessVisibleCritters(cr2);
        _server->MapMngr.ProcessVisibleItems(cr1);
        _server->MapMngr.ProcessVisibleItems(cr2);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetAllItems, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(hash, pid))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    ItemVec items;
    ItemVec all_items;
    _server->ItemMngr.GetGameItems(all_items);
    for (auto* item : all_items) {
        if (!item->IsDestroyed && (pid == 0u || pid == item->GetProtoId())) {
            items.push_back(item);
        }
    }

    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetOnlinePlayers, FO_API_RET_OBJ_ARR(Critter))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG()
{
    ClientVec all_players;
    _server->CrMngr.GetClients(all_players, false);

    CritterVec players;
    for (auto* player_ : all_players) {
        if (!player_->IsDestroyed && player_->IsPlayer()) {
            players.push_back(player_);
        }
    }

    FO_API_RETURN(players);
}
FO_API_EPILOG(0)
#endif
#endif

#ifdef FO_API_MULTIPLAYER_ONLY
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetRegisteredPlayerIds, FO_API_RET_ARR(uint))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_server->DbStorage->GetAllIds("Players"));
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetAllNpc, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(hash, pid))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    CritterVec npcs;
    NpcVec all_npcs;
    _server->CrMngr.GetNpcs(all_npcs);
    for (auto* npc_ : all_npcs) {
        if (!npc_->IsDestroyed && (pid == 0u || pid == npc_->GetProtoId())) {
            npcs.push_back(npc_);
        }
    }

    FO_API_RETURN(npcs);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetAllMaps, FO_API_RET_OBJ_ARR(Map), FO_API_ARG(hash, pid))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    MapVec maps;
    MapVec all_maps;
    _server->MapMngr.GetMaps(all_maps);
    for (auto* map : all_maps) {
        if (pid == 0u || pid == map->GetProtoId()) {
            maps.push_back(map);
        }
    }

    FO_API_RETURN(maps);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(GetAllLocations, FO_API_RET_OBJ_ARR(Location), FO_API_ARG(hash, pid))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    LocationVec locations;
    LocationVec all_locations;
    _server->MapMngr.GetLocations(all_locations);
    for (auto* loc : all_locations) {
        if (pid == 0u || pid == loc->GetProtoId()) {
            locations.push_back(loc);
        }
    }

    FO_API_RETURN(locations);
}
FO_API_EPILOG(0)
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
FO_API_GLOBAL_SERVER_FUNC(GetTime, FO_API_RET(void), FO_API_ARG_REF(ushort, year), FO_API_ARG_REF(ushort, month), FO_API_ARG_REF(ushort, day), FO_API_ARG_REF(ushort, dayOfWeek), FO_API_ARG_REF(ushort, hour), FO_API_ARG_REF(ushort, minute), FO_API_ARG_REF(ushort, second), FO_API_ARG_REF(ushort, milliseconds))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
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

/*******************************************************************************
 * ...
 *
 * @param multiplier ...
 * @param year ...
 * @param month ...
 * @param day ...
 * @param hour ...
 * @param minute ...
 * @param second ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(SetTime, FO_API_RET(void), FO_API_ARG(ushort, multiplier), FO_API_ARG(ushort, year), FO_API_ARG(ushort, month), FO_API_ARG(ushort, day), FO_API_ARG(ushort, hour), FO_API_ARG(ushort, minute), FO_API_ARG(ushort, second))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, multiplier) FO_API_ARG_MARSHAL(ushort, year) FO_API_ARG_MARSHAL(ushort, month) FO_API_ARG_MARSHAL(ushort, day) FO_API_ARG_MARSHAL(ushort, hour) FO_API_ARG_MARSHAL(ushort, minute) FO_API_ARG_MARSHAL(ushort, second))
{
    _server->SetGameTime(multiplier, year, month, day, hour, minute, second);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param datName ...
 ******************************************************************************/
FO_API_GLOBAL_SERVER_FUNC(AddDataSource, FO_API_RET(void), FO_API_ARG(string, datName))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, datName))
{
    _server->FileMngr.AddDataSource(datName, false);
}
FO_API_EPILOG()
#endif
