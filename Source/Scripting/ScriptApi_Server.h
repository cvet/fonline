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

#ifdef FO_API_COMMON_IMPL
#include "PropertiesSerializator.h"

#include "curl/curl.h"
#include "minizip/zip.h"
#include "png.h"
#include "sha1.h"
#include "sha2.h"
#endif

#ifdef FO_API_ITEM_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param count ...
 * @param stackId ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_ITEM_METHOD(
    AddItem, FO_API_RET_OBJ(Item), FO_API_ARG(hash, pid), FO_API_ARG(uint, count), FO_API_ARG(uint, stackId))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_MARSHAL(uint, count) FO_API_ARG_MARSHAL(uint, stackId))
{
    if (!_server->ProtoMngr.GetProtoItem(pid))
        throw ScriptException("Invalid proto '{}' arg.", _str().parseHash(pid));

    if (!count)
        count = 1;
    FO_API_RETURN(_server->ItemMngr.AddItemContainer(_this, pid, count, stackId));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_ITEM_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param stackId ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_ITEM_METHOD(GetItems, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(uint, stackId))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, stackId))
{
    ItemVec items;
    _this->ContGetItems(items, stackId);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_ITEM_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param func ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_ITEM_METHOD(SetScript, FO_API_RET(bool), FO_API_ARG_CALLBACK(func))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_CALLBACK_MARSHAL(func))
{
    if (func)
    {
        // if (!_this->SetScript(func, true))
        //    throw ScriptException("Script function not found");
    }
    else
    {
        _this->SetScriptId(0);
    }
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_ITEM_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_ITEM_METHOD(GetMapPosition, FO_API_RET_OBJ(Map), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, hx) FO_API_ARG_REF_MARSHAL(ushort, hy))
{
    Map* map = nullptr;
    switch (_this->GetAccessory())
    {
    case ITEM_ACCESSORY_CRITTER: {
        Critter* cr = _server->CrMngr.GetCritter(_this->GetCritId());
        if (!cr)
            throw ScriptException("Critter accessory, critter not found");
        if (!cr->GetMapId())
        {
            hx = cr->GetWorldX();
            hy = cr->GetWorldY();
            FO_API_RETURN(nullptr);
        }
        map = _server->MapMngr.GetMap(cr->GetMapId());
        if (!map)
            throw ScriptException("Critter accessory, map not found");
        hx = cr->GetHexX();
        hy = cr->GetHexY();
    }
    break;
    case ITEM_ACCESSORY_HEX: {
        map = _server->MapMngr.GetMap(_this->GetMapId());
        if (!map)
            throw ScriptException("Hex accessory, map not found");
        hx = _this->GetHexX();
        hy = _this->GetHexY();
    }
    break;
    case ITEM_ACCESSORY_CONTAINER: {
        if (_this->GetId() == _this->GetContainerId())
            throw ScriptException("Container accessory, crosslink");
        Item* cont = _server->ItemMngr.GetItem(_this->GetContainerId());
        if (!cont)
            throw ScriptException("Container accessory, container not found");
        // FO_API_RETURN(Item_GetMapPosition(cont, hx, hy)); // Recursion
    }
    break;
    default:
        throw ScriptException("Unknown accessory");
        break;
    }
    FO_API_RETURN(map);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_ITEM_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_ITEM_METHOD(ChangeProto, FO_API_RET(bool), FO_API_ARG(hash, pid))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    ProtoItem* proto_item = _server->ProtoMngr.GetProtoItem(pid);
    if (!proto_item)
        throw ScriptException("Proto _this not found");

    ProtoItem* old_proto_item = _this->GetProtoItem();
    _this->SetProto(proto_item);

    if (_this->GetAccessory() == ITEM_ACCESSORY_CRITTER)
    {
        Critter* cr = _server->CrMngr.GetCritter(_this->GetCritId());
        if (!cr)
            FO_API_RETURN(true);
        _this->SetProto(old_proto_item);
        cr->Send_EraseItem(_this);
        _this->SetProto(proto_item);
        cr->Send_AddItem(_this);
        cr->SendAA_MoveItem(_this, ACTION_REFRESH, 0);
    }
    else if (_this->GetAccessory() == ITEM_ACCESSORY_HEX)
    {
        Map* map = _server->MapMngr.GetMap(_this->GetMapId());
        if (!map)
            FO_API_RETURN(true);
        ushort hx = _this->GetHexX();
        ushort hy = _this->GetHexY();
        _this->SetProto(old_proto_item);
        map->EraseItem(_this->GetId());
        _this->SetProto(proto_item);
        map->AddItem(_this, hx, hy);
    }
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_ITEM_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param fromFrm ...
 * @param toFrm ...
 ******************************************************************************/
#endif
FO_API_ITEM_METHOD(Animate, FO_API_RET(void), FO_API_ARG(uchar, fromFrm), FO_API_ARG(uchar, toFrm))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, fromFrm) FO_API_ARG_MARSHAL(uchar, toFrm))
{
    switch (_this->GetAccessory())
    {
    case ITEM_ACCESSORY_CRITTER: {
        // Critter* cr=CrMngr.GetCrit(_this->ACC_CRITTER.Id);
        // if(cr) cr->Send_AnimateItem(_this,from_frm,to_frm);
    }
    break;
    case ITEM_ACCESSORY_HEX: {
        Map* map = _server->MapMngr.GetMap(_this->GetMapId());
        if (map)
            map->AnimateItem(_this, fromFrm, toFrm);
    }
    break;
    case ITEM_ACCESSORY_CONTAINER:
        break;
    default:
        throw ScriptException("Unknown accessory");
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_ITEM_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @param item ...
 * @param param ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_ITEM_METHOD(CallStaticItemFunction, FO_API_RET(bool), FO_API_ARG_OBJ(Critter, cr), FO_API_ARG_OBJ(Item, item),
    FO_API_ARG(int, param))
#ifdef FO_API_ITEM_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr) FO_API_ARG_OBJ_MARSHAL(Item, item) FO_API_ARG_MARSHAL(int, param))
{
    if (!_this->SceneryScriptFunc)
        FO_API_RETURN(false);

    FO_API_RETURN(_this->SceneryScriptFunc(cr, _this, item, param) && _this->SceneryScriptFunc.GetResult());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(IsPlayer, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsPlayer());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(IsNpc, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsNpc());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetAccess, FO_API_RET(int))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    if (!_this->IsPlayer())
        throw ScriptException("Critter is not player");

    FO_API_RETURN(((Client*)_this)->Access);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param access ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(SetAccess, FO_API_RET(bool), FO_API_ARG(int, access))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, access))
{
    if (!_this->IsPlayer())
        throw ScriptException("Critter is not player");
    if (access < ACCESS_CLIENT || access > ACCESS_ADMIN)
        throw ScriptException("Wrong access type");

    if (access == ((Client*)_this)->Access)
        FO_API_RETURN(true);

    string pass;
    bool allow = _server->ScriptSys.PlayerGetAccessEvent(_this, access, pass);
    if (allow)
        ((Client*)_this)->Access = access;
    FO_API_RETURN(allow);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetMap, FO_API_RET_OBJ(Map))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_server->MapMngr.GetMap(_this->GetMapId()));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param direction ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(MoveToDir, FO_API_RET(bool), FO_API_ARG(uchar, direction))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, direction))
{
    Map* map = _server->MapMngr.GetMap(_this->GetMapId());
    if (!map)
        throw ScriptException("Critter is on global");
    if (direction >= _server->Settings.MapDirCount)
        throw ScriptException("Invalid direction arg");

    ushort hx = _this->GetHexX();
    ushort hy = _this->GetHexY();
    _server->GeomHelper.MoveHexByDir(hx, hy, direction, map->GetWidth(), map->GetHeight());
    ushort move_flags = direction | BIN16(00000000, 00111000);
    bool move = _server->Act_Move(_this, hx, hy, move_flags);
    if (!move)
        throw ScriptException("Move fail");
    _this->Send_Move(_this, move_flags);
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param dir ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(
    TransitToHex, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uchar, dir))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uchar, dir))
{
    if (_this->LockMapTransfers)
        throw ScriptException("Transfers locked");
    Map* map = _server->MapMngr.GetMap(_this->GetMapId());
    if (!map)
        throw ScriptException("Critter is on global");
    if (hx >= map->GetWidth() || hy >= map->GetHeight())
        throw ScriptException("Invalid hexes args");

    if (hx != _this->GetHexX() || hy != _this->GetHexY())
    {
        if (dir < _server->Settings.MapDirCount && _this->GetDir() != dir)
            _this->SetDir(dir);
        if (!_server->MapMngr.Transit(_this, map, hx, hy, _this->GetDir(), 2, 0, true))
            throw ScriptException("Transit fail");
    }
    else if (dir < _server->Settings.MapDirCount && _this->GetDir() != dir)
    {
        _this->SetDir(dir);
        _this->Send_Dir(_this);
        _this->SendA_Dir();
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 * @param hx ...
 * @param hy ...
 * @param dir ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(TransitToMapHex, FO_API_RET(void), FO_API_ARG_OBJ(Map, map), FO_API_ARG(ushort, hx),
    FO_API_ARG(ushort, hy), FO_API_ARG(uchar, dir))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Map, map) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy)
        FO_API_ARG_MARSHAL(uchar, dir))
{
    if (_this->LockMapTransfers)
        throw ScriptException("Transfers locked");
    if (!map)
        throw ScriptException("Map arg is null");

    if (dir >= _server->Settings.MapDirCount)
        dir = 0;

    if (!_server->MapMngr.Transit(_this, map, hx, hy, dir, 2, 0, true))
        throw ScriptException("Transit to map hex fail");

    // Todo: need attention!
    Location* loc = map->GetLocation();
    if (loc &&
        GenericUtils::DistSqrt(_this->GetWorldX(), _this->GetWorldY(), loc->GetWorldX(), loc->GetWorldY()) >
            loc->GetRadius())
    {
        _this->SetWorldX(loc->GetWorldX());
        _this->SetWorldY(loc->GetWorldY());
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(TransitToGlobal, FO_API_RET(void))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    if (_this->LockMapTransfers)
        throw ScriptException("Transfers locked");

    if (_this->GetMapId() && !_server->MapMngr.TransitToGlobal(_this, 0, true))
        throw ScriptException("Transit fail");
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param group ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(TransitToGlobalWithGroup, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Critter, group))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Critter, group))
{
    if (_this->LockMapTransfers)
        throw ScriptException("Transfers locked");
    if (!_this->GetMapId())
        throw ScriptException("Critter already on global");

    if (!_server->MapMngr.TransitToGlobal(_this, 0, true))
        throw ScriptException("Transit fail");

    for (Critter* cr_ : group)
    {
        if (cr_ && !cr_->IsDestroyed)
            _server->MapMngr.TransitToGlobal(cr_, _this->GetId(), true);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param leader ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(TransitToGlobalGroup, FO_API_RET(void), FO_API_ARG_OBJ(Critter, leader))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, leader))
{
    if (_this->LockMapTransfers)
        throw ScriptException("Transfers locked");
    if (!_this->GetMapId())
        throw ScriptException("Critter already on global");
    if (!leader)
        throw ScriptException("Leader arg not found");

    if (!leader->GlobalMapGroup)
        throw ScriptException("Leader is not on global map");

    if (!_server->MapMngr.TransitToGlobal(_this, leader->GetId(), true))
        throw ScriptException("Transit fail");
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(IsLife, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsLife());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(IsKnockout, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsKnockout());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(IsDead, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsDead());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(IsFree, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsFree() && !_this->IsWait());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(IsBusy, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->IsBusy() || _this->IsWait());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param ms ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(Wait, FO_API_RET(void), FO_API_ARG(uint, ms))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, ms))
{
    _this->SetWait(ms);
    if (_this->IsPlayer())
    {
        Client* cl = (Client*)_this;
        cl->SetBreakTime(ms);
        cl->Send_CustomCommand(_this, OTHER_BREAK_TIME, ms);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(RefreshVisible, FO_API_RET(void))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    _server->MapMngr.ProcessVisibleCritters(_this);
    _server->MapMngr.ProcessVisibleItems(_this);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 * @param look ...
 * @param hx ...
 * @param hy ...
 * @param dir ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(ViewMap, FO_API_RET(void), FO_API_ARG_OBJ(Map, map), FO_API_ARG(uint, look),
    FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uchar, dir))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Map, map) FO_API_ARG_MARSHAL(uint, look) FO_API_ARG_MARSHAL(ushort, hx)
        FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uchar, dir))
{
    if (!map)
        throw ScriptException("Map arg is null");

    if (hx >= map->GetWidth() || hy >= map->GetHeight())
        throw ScriptException("Invalid hexes args");

    if (!_this->IsPlayer())
        FO_API_RETURN_VOID();

    if (dir >= _server->Settings.MapDirCount)
        dir = _this->GetDir();
    if (!look)
        look = _this->GetLookDistance();

    _this->ViewMapId = map->GetId();
    _this->ViewMapPid = map->GetProtoId();
    _this->ViewMapLook = look;
    _this->ViewMapHx = hx;
    _this->ViewMapHy = hy;
    _this->ViewMapDir = dir;
    _this->ViewMapLocId = 0;
    _this->ViewMapLocEnt = 0;
    _this->Send_LoadMap(map, _server->MapMngr);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param howSay ...
 * @param text ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(Say, FO_API_RET(void), FO_API_ARG(uchar, howSay), FO_API_ARG(string, text))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, howSay) FO_API_ARG_MARSHAL(string, text))
{
    if (howSay != SAY_FLASH_WINDOW && !text.length())
        throw ScriptException("Text empty");
    if (_this->IsNpc() && !_this->IsLife())
        FO_API_RETURN_VOID(); // throw ScriptException("Npc is not life");

    if (howSay >= SAY_NETMSG)
        _this->Send_Text(_this, howSay != SAY_FLASH_WINDOW ? text : " ", howSay);
    else if (_this->GetMapId())
        _this->SendAA_Text(_this->VisCr, text, howSay, false);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param howSay ...
 * @param textMsg ...
 * @param numStr ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(
    SayMsg, FO_API_RET(void), FO_API_ARG(uchar, howSay), FO_API_ARG(ushort, textMsg), FO_API_ARG(uint, numStr))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, howSay) FO_API_ARG_MARSHAL(ushort, textMsg) FO_API_ARG_MARSHAL(uint, numStr))
{
    if (_this->IsNpc() && !_this->IsLife())
        FO_API_RETURN_VOID(); // throw ScriptException("Npc is not life");

    if (howSay >= SAY_NETMSG)
        _this->Send_TextMsg(_this, numStr, howSay, textMsg);
    else if (_this->GetMapId())
        _this->SendAA_Msg(_this->VisCr, numStr, howSay, textMsg);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param howSay ...
 * @param textMsg ...
 * @param numStr ...
 * @param lexems ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(SayMsgLex, FO_API_RET(void), FO_API_ARG(uchar, howSay), FO_API_ARG(ushort, textMsg),
    FO_API_ARG(uint, numStr), FO_API_ARG(string, lexems))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, howSay) FO_API_ARG_MARSHAL(ushort, textMsg) FO_API_ARG_MARSHAL(uint, numStr)
        FO_API_ARG_MARSHAL(string, lexems))
{
    // Npc is not life
    if (_this->IsNpc() && !_this->IsLife())
        FO_API_RETURN_VOID();

    if (howSay >= SAY_NETMSG)
        _this->Send_TextMsgLex(_this, numStr, howSay, textMsg, lexems.c_str());
    else if (_this->GetMapId())
        _this->SendAA_MsgLex(_this->VisCr, numStr, howSay, textMsg, lexems.c_str());
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param dir ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(SetDir, FO_API_RET(void), FO_API_ARG(uchar, dir))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, dir))
{
    if (dir >= _server->Settings.MapDirCount)
        throw ScriptException("Invalid direction arg");

    // Direction already set
    if (_this->GetDir() == dir)
        FO_API_RETURN_VOID();

    _this->SetDir(dir);
    if (_this->GetMapId())
    {
        _this->Send_Dir(_this);
        _this->SendA_Dir();
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param lookOnMe ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetCritters, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(bool, lookOnMe), FO_API_ARG(int, findType))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, lookOnMe) FO_API_ARG_MARSHAL(int, findType))
{
    CritterVec critters;
    for (auto it = (lookOnMe ? _this->VisCr.begin() : _this->VisCrSelf.begin()),
              end = (lookOnMe ? _this->VisCr.end() : _this->VisCrSelf.end());
         it != end; ++it)
    {
        Critter* cr_ = *it;
        if (cr_->CheckFind(findType))
            critters.push_back(cr_);
    }

    int hx = _this->GetHexX();
    int hy = _this->GetHexY();
    std::sort(critters.begin(), critters.end(), [_server, hx, hy](Critter* cr1, Critter* cr2) {
        return _server->GeomHelper.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY()) <
            _server->GeomHelper.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
    });
    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetTalkedPlayers, FO_API_RET_OBJ_ARR(Critter))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    if (!_this->IsNpc())
        throw ScriptException("Critter is not npc");

    CritterVec players;
    for (auto cr_ : _this->VisCr)
    {
        if (cr_->IsPlayer())
        {
            Client* cl = (Client*)cr_;
            if (cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == _this->GetId())
                players.push_back(cl);
        }
    }

    int hx = _this->GetHexX();
    int hy = _this->GetHexY();
    std::sort(players.begin(), players.end(), [_server, hx, hy](Critter* cr1, Critter* cr2) {
        return _server->GeomHelper.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY()) <
            _server->GeomHelper.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
    });
    FO_API_RETURN(players);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(IsSeeCr, FO_API_RET(bool), FO_API_ARG_OBJ(Critter, cr))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr))
{
    if (!cr)
        throw ScriptException("Critter arg is null");

    if (_this == cr)
        FO_API_RETURN(true);

    CritterVec& critters = (_this->GetMapId() ? _this->VisCrSelf : *_this->GlobalMapGroup);
    FO_API_RETURN(std::find(critters.begin(), critters.end(), cr) != critters.end());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(IsSeenByCr, FO_API_RET(bool), FO_API_ARG_OBJ(Critter, cr))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr))
{
    if (!cr)
        throw ScriptException("Critter arg is null");

    if (_this == cr)
        FO_API_RETURN(true);

    CritterVec& critters = (_this->GetMapId() ? _this->VisCr : *_this->GlobalMapGroup);
    FO_API_RETURN(std::find(critters.begin(), critters.end(), cr) != critters.end());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(IsSeeItem, FO_API_RET(bool), FO_API_ARG_OBJ(Item, item))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Item, item))
{
    if (!item)
        throw ScriptException("Item arg is null");

    FO_API_RETURN(_this->CountIdVisItem(item->GetId()));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param protoId ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(CountItem, FO_API_RET(uint), FO_API_ARG(hash, protoId))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, protoId))
{
    FO_API_RETURN(_this->CountItemPid(protoId));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param count ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(DeleteItem, FO_API_RET(bool), FO_API_ARG(hash, pid), FO_API_ARG(uint, count))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_MARSHAL(uint, count))
{
    if (!pid)
        throw ScriptException("Proto id arg is zero");

    if (!count)
        count = _this->CountItemPid(pid);
    FO_API_RETURN(_server->ItemMngr.SubItemCritter(_this, pid, count));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param count ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(AddItem, FO_API_RET_OBJ(Item), FO_API_ARG(hash, pid), FO_API_ARG(uint, count))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_MARSHAL(uint, count))
{
    if (!pid)
        throw ScriptException("Proto id arg is zero");
    if (!_server->ProtoMngr.GetProtoItem(pid))
        throw ScriptException("Invalid proto '{}'.", _str().parseHash(pid));

    if (!count)
        count = 1;
    FO_API_RETURN(_server->ItemMngr.AddItemCritter(_this, pid, count));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param itemId ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetItem, FO_API_RET_OBJ(Item), FO_API_ARG(uint, itemId))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId))
{
    FO_API_RETURN(_this->GetItem(itemId, false));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param predicate ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetItemPredicate, FO_API_RET_OBJ(Item), FO_API_ARG_PREDICATE(Item, predicate))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_PREDICATE_MARSHAL(Item, predicate))
{
    ItemVec inv_items = _this->GetInventory();
    for (Item* item : inv_items)
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed)
            FO_API_RETURN(item);
    FO_API_RETURN(nullptr);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param slot ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetItemBySlot, FO_API_RET_OBJ(Item), FO_API_ARG(int, slot))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, slot))
{
    FO_API_RETURN(_this->GetItemSlot(slot));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param protoId ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetItemByPid, FO_API_RET_OBJ(Item), FO_API_ARG(hash, protoId))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, protoId))
{
    FO_API_RETURN(_server->CrMngr.GetItemByPidInvPriority(_this, protoId));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetItems, FO_API_RET_OBJ_ARR(Item))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->GetInventory());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param slot ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetItemsBySlot, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(int, slot))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, slot))
{
    ItemVec items;
    _this->GetItemsSlot(slot, items);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param predicate ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetItemsPredicate, FO_API_RET_OBJ_ARR(Item), FO_API_ARG_PREDICATE(Item, predicate))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_PREDICATE_MARSHAL(Item, predicate))
{
    ItemVec inv_items = _this->GetInventory();
    ItemVec items;
    items.reserve(inv_items.size());
    for (Item* item : inv_items)
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed)
            items.push_back(item);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param itemId ...
 * @param slot ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(ChangeItemSlot, FO_API_RET(void), FO_API_ARG(uint, itemId), FO_API_ARG(int, slot))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId) FO_API_ARG_MARSHAL(int, slot))
{
    if (!itemId)
        throw ScriptException("Item id arg is zero");
    Item* item = _this->GetItem(itemId, _this->IsPlayer());
    if (!item)
        throw ScriptException("Item not found");

    // To slot arg is equal of current item slot
    if (item->GetCritSlot() == slot)
        FO_API_RETURN_VOID();

    if (!Critter::SlotEnabled[slot])
        throw ScriptException("Slot is not allowed");

    if (!_server->ScriptSys.CritterCheckMoveItemEvent(_this, item, slot))
        throw ScriptException("Can't move item");

    Item* item_swap = (slot ? _this->GetItemSlot(slot) : nullptr);
    uchar from_slot = item->GetCritSlot();

    item->SetCritSlot(slot);
    if (item_swap)
        item_swap->SetCritSlot(from_slot);

    _this->SendAA_MoveItem(item, ACTION_MOVE_ITEM, from_slot);

    if (item_swap)
        _server->ScriptSys.CritterMoveItemEvent(_this, item_swap, slot);
    _server->ScriptSys.CritterMoveItemEvent(_this, item, from_slot);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param cond ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(SetCond, FO_API_RET(void), FO_API_ARG(int, cond))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, cond))
{
    int prev_cond = _this->GetCond();
    if (prev_cond == cond)
        FO_API_RETURN_VOID();
    _this->SetCond(cond);

    if (_this->GetMapId())
    {
        Map* map = _server->MapMngr.GetMap(_this->GetMapId());
        RUNTIME_ASSERT(map);
        ushort hx = _this->GetHexX();
        ushort hy = _this->GetHexY();
        uint multihex = _this->GetMultihex();
        bool is_dead = (cond == COND_DEAD);
        map->UnsetFlagCritter(hx, hy, multihex, !is_dead);
        map->SetFlagCritter(hx, hy, multihex, is_dead);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(CloseDialog, FO_API_RET(void))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    if (_this->IsPlayer() && ((Client*)_this)->IsTalking())
        _server->CrMngr.CloseTalk((Client*)_this);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param arr ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(SendCombatResult, FO_API_RET(void), FO_API_ARG_ARR(uint, arr))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_ARR_MARSHAL(uint, arr))
{
    if (arr.size() > _server->Settings.FloodSize / sizeof(uint))
        throw ScriptException("Elements count is greater than maximum");

    _this->Send_CombatResult(arr.data(), (uint)arr.size());
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param action ...
 * @param actionExt ...
 * @param item ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(
    Action, FO_API_RET(void), FO_API_ARG(int, action), FO_API_ARG(int, actionExt), FO_API_ARG_OBJ(Item, item))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, action) FO_API_ARG_MARSHAL(int, actionExt) FO_API_ARG_OBJ_MARSHAL(Item, item))
{
    _this->SendAA_Action(action, actionExt, item);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param anim1 ...
 * @param anim2 ...
 * @param item ...
 * @param clearSequence ...
 * @param delayPlay ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(Animate, FO_API_RET(void), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2),
    FO_API_ARG_OBJ(Item, item), FO_API_ARG(bool, clearSequence), FO_API_ARG(bool, delayPlay))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2) FO_API_ARG_OBJ_MARSHAL(Item, item)
        FO_API_ARG_MARSHAL(bool, clearSequence) FO_API_ARG_MARSHAL(bool, delayPlay))
{
    _this->SendAA_Animate(anim1, anim2, item, clearSequence, delayPlay);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param cond ...
 * @param anim1 ...
 * @param anim2 ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(
    SetAnims, FO_API_RET(void), FO_API_ARG(int, cond), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, cond) FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2))
{
    if (cond == 0 || cond == COND_LIFE)
    {
        _this->SetAnim1Life(anim1);
        _this->SetAnim2Life(anim2);
    }
    if (cond == 0 || cond == COND_KNOCKOUT)
    {
        _this->SetAnim1Knockout(anim1);
        _this->SetAnim2Knockout(anim2);
    }
    if (cond == 0 || cond == COND_DEAD)
    {
        _this->SetAnim1Dead(anim1);
        _this->SetAnim2Dead(anim2);
    }
    _this->SendAA_SetAnims(cond, anim1, anim2);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param soundName ...
 * @param sendSelf ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(PlaySound, FO_API_RET(void), FO_API_ARG(string, soundName), FO_API_ARG(bool, sendSelf))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, soundName) FO_API_ARG_MARSHAL(bool, sendSelf))
{
    if (sendSelf)
        _this->Send_PlaySound(_this->GetId(), soundName);

    for (auto it = _this->VisCr.begin(), end = _this->VisCr.end(); it != end; ++it)
    {
        Critter* cr_ = *it;
        cr_->Send_PlaySound(_this->GetId(), soundName);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param byId ...
 * @param locNum ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(IsKnownLoc, FO_API_RET(bool), FO_API_ARG(bool, byId), FO_API_ARG(uint, locNum))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, byId) FO_API_ARG_MARSHAL(uint, locNum))
{
    if (byId)
        FO_API_RETURN(_server->MapMngr.CheckKnownLocById(_this, locNum));
    FO_API_RETURN(_server->MapMngr.CheckKnownLocByPid(_this, locNum));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param byId ...
 * @param locNum ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(SetKnownLoc, FO_API_RET(void), FO_API_ARG(bool, byId), FO_API_ARG(uint, locNum))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, byId) FO_API_ARG_MARSHAL(uint, locNum))
{
    Location* loc = (byId ? _server->MapMngr.GetLocation(locNum) : _server->MapMngr.GetLocationByPid(locNum, 0));
    if (!loc)
        throw ScriptException("Location not found");

    _server->MapMngr.AddKnownLoc(_this, loc->GetId());

    if (loc->IsNonEmptyAutomaps())
        _this->Send_AutomapsInfo(nullptr, loc);

    if (!_this->GetMapId())
        _this->Send_GlobalLocation(loc, true);

    int zx = loc->GetWorldX() / _server->Settings.GlobalMapZoneLength;
    int zy = loc->GetWorldY() / _server->Settings.GlobalMapZoneLength;

    UCharVec gmap_fog = _this->GetGlobalMapFog();
    if (gmap_fog.size() != GM_ZONES_FOG_SIZE)
        gmap_fog.resize(GM_ZONES_FOG_SIZE);

    TwoBitMask gmap_mask(GM__MAXZONEX, GM__MAXZONEY, gmap_fog.data());
    if (gmap_mask.Get2Bit(zx, zy) == GM_FOG_FULL)
    {
        gmap_mask.Set2Bit(zx, zy, GM_FOG_HALF);
        _this->SetGlobalMapFog(gmap_fog);
        if (!_this->GetMapId())
            _this->Send_GlobalMapFog(zx, zy, GM_FOG_HALF);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param byId ...
 * @param locNum ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(UnsetKnownLoc, FO_API_RET(void), FO_API_ARG(bool, byId), FO_API_ARG(uint, locNum))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, byId) FO_API_ARG_MARSHAL(uint, locNum))
{
    Location* loc = (byId ? _server->MapMngr.GetLocation(locNum) : _server->MapMngr.GetLocationByPid(locNum, 0));
    if (!loc)
        throw ScriptException("Location not found");
    if (!_server->MapMngr.CheckKnownLocById(_this, loc->GetId()))
        throw ScriptException("Player is not know this location");

    _server->MapMngr.EraseKnownLoc(_this, loc->GetId());

    if (!_this->GetMapId())
        _this->Send_GlobalLocation(loc, false);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param zoneX ...
 * @param zoneY ...
 * @param fog ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(
    SetFog, FO_API_RET(void), FO_API_ARG(ushort, zoneX), FO_API_ARG(ushort, zoneY), FO_API_ARG(int, fog))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, zoneX) FO_API_ARG_MARSHAL(ushort, zoneY) FO_API_ARG_MARSHAL(int, fog))
{
    if (fog < GM_FOG_FULL || fog > GM_FOG_NONE)
        throw ScriptException("Invalid fog arg");
    if (zoneX >= _server->Settings.GlobalMapWidth || zoneY >= _server->Settings.GlobalMapHeight)
        FO_API_RETURN_VOID();

    UCharVec gmap_fog = _this->GetGlobalMapFog();
    if (gmap_fog.size() != GM_ZONES_FOG_SIZE)
        gmap_fog.resize(GM_ZONES_FOG_SIZE);

    TwoBitMask gmap_mask(GM__MAXZONEX, GM__MAXZONEY, gmap_fog.data());
    if (gmap_mask.Get2Bit(zoneX, zoneY) != fog)
    {
        gmap_mask.Set2Bit(zoneX, zoneY, fog);
        _this->SetGlobalMapFog(gmap_fog);
        if (!_this->GetMapId())
            _this->Send_GlobalMapFog(zoneX, zoneY, fog);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param zoneX ...
 * @param zoneY ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetFog, FO_API_RET(int), FO_API_ARG(ushort, zoneX), FO_API_ARG(ushort, zoneY))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, zoneX) FO_API_ARG_MARSHAL(ushort, zoneY))
{
    if (zoneX >= _server->Settings.GlobalMapWidth || zoneY >= _server->Settings.GlobalMapHeight)
        FO_API_RETURN(GM_FOG_FULL);

    UCharVec gmap_fog = _this->GetGlobalMapFog();
    if (gmap_fog.size() != GM_ZONES_FOG_SIZE)
        gmap_fog.resize(GM_ZONES_FOG_SIZE);

    TwoBitMask gmap_mask(GM__MAXZONEX, GM__MAXZONEY, gmap_fog.data());
    FO_API_RETURN(gmap_mask.Get2Bit(zoneX, zoneY));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param items ...
 * @param param ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(SendItems, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Item, items), FO_API_ARG(int, param))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Item, items) FO_API_ARG_MARSHAL(int, param))
{
    // Critter is not player
    if (!_this->IsPlayer())
        FO_API_RETURN_VOID();

    ((Client*)_this)->Send_SomeItems(&items, param);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(Disconnect, FO_API_RET(void))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    if (!_this->IsPlayer())
        throw ScriptException("Critter is not player");

    Client* cl_ = (Client*)_this;
    if (cl_->IsOnline())
        cl_->Disconnect();
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(IsOnline, FO_API_RET(bool))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    if (!_this->IsPlayer())
        throw ScriptException("Critter is not player");

    Client* cl_ = (Client*)_this;
    FO_API_RETURN(cl_->IsOnline());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param func ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(SetScript, FO_API_RET(void), FO_API_ARG(string, funcName))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, funcName))
{
    if (!funcName.empty())
    {
        if (!_this->SetScript(funcName, true))
            throw ScriptException("Script function not found");
    }
    else
    {
        _this->SetScriptId(0);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param func ...
 * @param duration ...
 * @param identifier ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(
    AddTimeEvent, FO_API_RET(void), FO_API_ARG_CALLBACK(func), FO_API_ARG(uint, duration), FO_API_ARG(int, identifier))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_CALLBACK_MARSHAL(func) FO_API_ARG_MARSHAL(uint, duration) FO_API_ARG_MARSHAL(int, identifier))
{
    /*hash func_num = _server->ScriptSys.BindScriptFuncNumByFunc(func);
    if (!func_num)
        throw ScriptException("Function not found");

    _this->AddCrTimeEvent(func_num, 0, duration, identifier);*/
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param func ...
 * @param duration ...
 * @param identifier ...
 * @param rate ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(AddTimeEventRate, FO_API_RET(void), FO_API_ARG_CALLBACK(func), FO_API_ARG(uint, duration),
    FO_API_ARG(int, identifier), FO_API_ARG(uint, rate))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_CALLBACK_MARSHAL(func) FO_API_ARG_MARSHAL(uint, duration) FO_API_ARG_MARSHAL(int, identifier)
        FO_API_ARG_MARSHAL(uint, rate))
{
    /*hash func_num = _server->ScriptSys.BindScriptFuncNumByFunc(func);
    if (!func_num)
        throw ScriptException("Function not found");

    _this->AddCrTimeEvent(func_num, rate, duration, identifier);*/
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param identifier ...
 * @param indexes ...
 * @param durations ...
 * @param rates ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetTimeEvents, FO_API_RET(uint), FO_API_ARG(int, identifier), FO_API_ARG_ARR_REF(int, indexes),
    FO_API_ARG_ARR_REF(int, durations), FO_API_ARG_ARR_REF(int, rates))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, identifier) FO_API_ARG_ARR_REF_MARSHAL(int, indexes)
        FO_API_ARG_ARR_REF_MARSHAL(int, durations) FO_API_ARG_ARR_REF_MARSHAL(int, rates))
{
    /*CScriptArray* te_identifier = _this->GetTE_Identifier();
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
        te_next_time = _this->GetTE_NextTime();
        RUNTIME_ASSERT(te_next_time->GetSize() == te_identifier->GetSize());
        durations_size = durations->GetSize();
        durations->Resize(durations_size + size);
    }
    if (rates)
    {
        te_rate = _this->GetTE_Rate();
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

#ifdef FO_API_CRITTER_METHOD_DOC
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
#endif
FO_API_CRITTER_METHOD(GetTimeEventsArr, FO_API_RET(uint), FO_API_ARG_ARR(int, findIdentifiers),
    FO_API_ARG_ARR(int, identifiers), FO_API_ARG_ARR_REF(int, indexes), FO_API_ARG_ARR_REF(int, durations),
    FO_API_ARG_ARR_REF(int, rates))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_ARR_MARSHAL(int, findIdentifiers) FO_API_ARG_ARR_MARSHAL(int, identifiers)
        FO_API_ARG_ARR_REF_MARSHAL(int, indexes) FO_API_ARG_ARR_REF_MARSHAL(int, durations)
            FO_API_ARG_ARR_REF_MARSHAL(int, rates))
{
    /*IntVec find_vec;
    _server->ScriptSys.AssignScriptArrayInVector(find_vec, findIdentifiers);

    CScriptArray* te_identifier = _this->GetTE_Identifier();
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
        te_next_time = _this->GetTE_NextTime();
        RUNTIME_ASSERT(te_next_time->GetSize() == te_identifier->GetSize());
        durations_size = durations->GetSize();
        durations->Resize(durations_size + size);
    }
    if (rates)
    {
        te_rate = _this->GetTE_Rate();
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

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param index ...
 * @param newDuration ...
 * @param newRate ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(ChangeTimeEvent, FO_API_RET(void), FO_API_ARG(uint, index), FO_API_ARG(uint, newDuration),
    FO_API_ARG(uint, newRate))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, index) FO_API_ARG_MARSHAL(uint, newDuration) FO_API_ARG_MARSHAL(uint, newRate))
{
    /*CScriptArray* te_func_num = _this->GetTE_FuncNum();
    CScriptArray* te_identifier = _this->GetTE_Identifier();
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

    _this->EraseCrTimeEvent(index);
    _this->AddCrTimeEvent(func_num, newRate, newDuration, identifier);*/
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param index ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(EraseTimeEvent, FO_API_RET(void), FO_API_ARG(uint, index))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, index))
{
    /*CScriptArray* te_func_num = _this->GetTE_FuncNum();
    uint size = te_func_num->GetSize();
    te_func_num->Release();
    if (index >= size)
        throw ScriptException("Index arg is greater than maximum time events");

    _this->EraseCrTimeEvent(index);*/
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param identifier ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(EraseTimeEvents, FO_API_RET(uint), FO_API_ARG(int, identifier))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, identifier))
{
    /*CScriptArray* te_next_time = _this->GetTE_NextTime();
    CScriptArray* te_func_num = _this->GetTE_FuncNum();
    CScriptArray* te_rate = _this->GetTE_Rate();
    CScriptArray* te_identifier = _this->GetTE_Identifier();
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

    _this->SetTE_NextTime(te_next_time);
    _this->SetTE_FuncNum(te_func_num);
    _this->SetTE_Rate(te_rate);
    _this->SetTE_Identifier(te_identifier);

    te_next_time->Release();
    te_func_num->Release();
    te_rate->Release();
    te_identifier->Release();

    FO_API_RETURN(result);*/
    FO_API_RETURN(0);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param identifiers ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(EraseTimeEventsArr, FO_API_RET(uint), FO_API_ARG_ARR(int, identifiers))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_ARR_MARSHAL(int, identifiers))
{
    /*IntVec identifiers_;
    _server->ScriptSys.AssignScriptArrayInVector(identifiers_, identifiers);

    CScriptArray* te_next_time = _this->GetTE_NextTime();
    CScriptArray* te_func_num = _this->GetTE_FuncNum();
    CScriptArray* te_rate = _this->GetTE_Rate();
    CScriptArray* te_identifier = _this->GetTE_Identifier();
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

    _this->SetTE_NextTime(te_next_time);
    _this->SetTE_FuncNum(te_func_num);
    _this->SetTE_Rate(te_rate);
    _this->SetTE_Identifier(te_identifier);

    te_next_time->Release();
    te_func_num->Release();
    te_rate->Release();
    te_identifier->Release();

    FO_API_RETURN(result);*/
    FO_API_RETURN(0);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param target ...
 * @param cut ...
 * @param isRun ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(
    MoveToCritter, FO_API_RET(void), FO_API_ARG_OBJ(Critter, target), FO_API_ARG(uint, cut), FO_API_ARG(bool, isRun))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, target) FO_API_ARG_MARSHAL(uint, cut) FO_API_ARG_MARSHAL(bool, isRun))
{
    if (!target)
        throw ScriptException("Critter arg is null");

    memzero(&_this->Moving, sizeof(_this->Moving));
    _this->Moving.TargId = target->GetId();
    _this->Moving.HexX = target->GetHexX();
    _this->Moving.HexY = target->GetHexY();
    _this->Moving.Cut = cut;
    _this->Moving.IsRun = isRun;
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param cut ...
 * @param isRun ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(MoveToHex, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy),
    FO_API_ARG(uint, cut), FO_API_ARG(bool, isRun))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, cut)
        FO_API_ARG_MARSHAL(bool, isRun))
{
    memzero(&_this->Moving, sizeof(_this->Moving));
    _this->Moving.HexX = hx;
    _this->Moving.HexY = hy;
    _this->Moving.Cut = cut;
    _this->Moving.IsRun = isRun;
}
FO_API_EPILOG()
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(GetMovingState, FO_API_RET(int))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->Moving.State);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param gagId ...
 ******************************************************************************/
#endif
FO_API_CRITTER_METHOD(ResetMovingState, FO_API_RET(void), FO_API_ARG_REF(uint, gagId))
#ifdef FO_API_CRITTER_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(uint, gagId))
{
    gagId = _this->Moving.GagEntityId;
    memzero(&_this->Moving, sizeof(_this->Moving));
    _this->Moving.State = 1;
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetLocation, FO_API_RET_OBJ(Location))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->GetLocation());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param func ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(SetScript, FO_API_RET(void), FO_API_ARG(string, funcName))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, funcName))
{
    if (!funcName.empty())
    {
        if (!_this->SetScript(funcName, true))
            throw ScriptException("Script function not found");
    }
    else
    {
        _this->SetScriptId(0);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
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
#endif
FO_API_MAP_METHOD(AddItem, FO_API_RET_OBJ(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy),
    FO_API_ARG(hash, protoId), FO_API_ARG(uint, count), FO_API_ARG_DICT(int, int, props))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(hash, protoId)
        FO_API_ARG_MARSHAL(uint, count) FO_API_ARG_DICT_MARSHAL(int, int, props))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    ProtoItem* proto = _server->ProtoMngr.GetProtoItem(protoId);
    if (!proto)
        throw ScriptException("Invalid proto '{}' arg.", _str().parseHash(protoId));
    if (!_this->IsPlaceForProtoItem(hx, hy, proto))
        throw ScriptException("No place for item");

    if (!count)
        count = 1;

    if (!props.empty())
    {
        Properties props_(Item::PropertiesRegistrator);
        props_ = proto->Props;
        for (auto& kv : props)
            if (!Properties::SetValueAsIntProps(&props_, kv.first, kv.second))
                FO_API_RETURN(nullptr);
        FO_API_RETURN(_server->CreateItemOnHex(_this, hx, hy, protoId, count, &props_, true));
    }
    FO_API_RETURN(_server->CreateItemOnHex(_this, hx, hy, protoId, count, nullptr, true));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetItems, FO_API_RET_OBJ_ARR(Item))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG()
{
    ItemVec items;
    _this->GetItemsPid(0, items);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetItemsHex, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    ItemVec items;
    _this->GetItemsHex(hx, hy, items);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param radius ...
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetItemsHexEx, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy),
    FO_API_ARG(uint, radius), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, radius)
        FO_API_ARG_MARSHAL(hash, pid))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    ItemVec items;
    _this->GetItemsHexEx(hx, hy, radius, pid, items);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetItemsByPid, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    ItemVec items;
    _this->GetItemsPid(pid, items);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param predicate ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetItemsPredicate, FO_API_RET_OBJ_ARR(Item), FO_API_ARG_PREDICATE(Item, predicate))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_PREDICATE_MARSHAL(Item, predicate))
{
    ItemVec map_items = _this->GetItems();
    ItemVec items;
    items.reserve(map_items.size());
    for (Item* item : map_items)
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed)
            items.push_back(item);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param predicate ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetItemsHexPredicate, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy),
    FO_API_ARG_PREDICATE(Item, predicate))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(
    FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_PREDICATE_MARSHAL(Item, predicate))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    ItemVec map_items;
    _this->GetItemsHex(hx, hy, map_items);
    ItemVec items;
    items.reserve(map_items.size());
    for (Item* item : map_items)
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed)
            items.push_back(item);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param radius ...
 * @param predicate ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetItemsHexRadiusPredicate, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy),
    FO_API_ARG(uint, radius), FO_API_ARG_PREDICATE(Item, predicate))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, radius)
        FO_API_ARG_PREDICATE_MARSHAL(Item, predicate))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    ItemVec map_items;
    _this->GetItemsHexEx(hx, hy, radius, 0, map_items);
    ItemVec items;
    items.reserve(map_items.size());
    for (Item* item : map_items)
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed)
            items.push_back(item);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param itemId ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetItem, FO_API_RET_OBJ(Item), FO_API_ARG(uint, itemId))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId))
{
    if (!itemId)
        throw ScriptException("Item id arg is zero");

    FO_API_RETURN(_this->GetItem(itemId));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(
    GetItemHex, FO_API_RET_OBJ(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(hash, pid))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    FO_API_RETURN(_this->GetItemHex(hx, hy, pid, nullptr));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetCritterHex, FO_API_RET_OBJ(Critter), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    Critter* cr = _this->GetHexCritter(hx, hy, false);
    if (!cr)
        cr = _this->GetHexCritter(hx, hy, true);
    FO_API_RETURN(cr);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(
    GetStaticItem, FO_API_RET_OBJ(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(hash, pid))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    FO_API_RETURN(_this->GetStaticItem(hx, hy, pid));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetStaticItemsHex, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    ItemVec static_items;
    _this->GetStaticItemsHex(hx, hy, static_items);

    FO_API_RETURN(static_items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param radius ...
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetStaticItemsHexEx, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy),
    FO_API_ARG(uint, radius), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, radius)
        FO_API_ARG_MARSHAL(hash, pid))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    ItemVec static_items;
    _this->GetStaticItemsHexEx(hx, hy, radius, pid, static_items);
    FO_API_RETURN(static_items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetStaticItemsByPid, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    ItemVec static_items;
    _this->GetStaticItemsByPid(pid, static_items);
    FO_API_RETURN(static_items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param predicate ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetStaticItemsPredicate, FO_API_RET_OBJ_ARR(Item), FO_API_ARG_PREDICATE(Item, predicate))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_PREDICATE_MARSHAL(Item, predicate))
{
    ItemVec& map_static_items = _this->GetStaticMap()->StaticItemsVec;
    ItemVec items;
    items.reserve(map_static_items.size());
    for (Item* item : map_static_items)
        if (predicate(item))
            items.push_back(item);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetStaticItemsAll, FO_API_RET_OBJ_ARR(Item))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->GetStaticMap()->StaticItemsVec);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param crid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetCritterById, FO_API_RET_OBJ(Critter), FO_API_ARG(uint, crid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, crid))
{
    FO_API_RETURN(_this->GetCritter(crid));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param radius ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetCritters, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy),
    FO_API_ARG(uint, radius), FO_API_ARG(int, findType))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, radius)
        FO_API_ARG_MARSHAL(int, findType))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    CritterVec critters;
    _this->GetCrittersHex(hx, hy, radius, findType, critters);
    std::sort(critters.begin(), critters.end(), [_server, hx, hy](Critter* cr1, Critter* cr2) {
        return _server->GeomHelper.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY()) <
            _server->GeomHelper.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
    });
    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetCrittersByPids, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(hash, pid), FO_API_ARG(int, findType))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_MARSHAL(int, findType))
{
    CritterVec critters;
    if (!pid)
    {
        CritterVec map_critters = _this->GetCritters();
        critters.reserve(map_critters.size());
        for (Critter* cr : map_critters)
            if (cr->CheckFind(findType))
                critters.push_back(cr);
    }
    else
    {
        NpcVec map_npcs = _this->GetNpcs();
        critters.reserve(map_npcs.size());
        for (Npc* npc : map_npcs)
            if (npc->GetProtoId() == pid && npc->CheckFind(findType))
                critters.push_back(npc);
    }

    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
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
#endif
FO_API_MAP_METHOD(GetCrittersInPath, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(ushort, fromHx),
    FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(float, angle),
    FO_API_ARG(uint, dist), FO_API_ARG(int, findType))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx)
        FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist)
            FO_API_ARG_MARSHAL(int, findType))
{
    CritterVec critters;
    TraceData trace;
    trace.TraceMap = _this;
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

#ifdef FO_API_MAP_METHOD_DOC
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
#endif
FO_API_MAP_METHOD(GetCrittersInPathBlock, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(ushort, fromHx),
    FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(float, angle),
    FO_API_ARG(uint, dist), FO_API_ARG(int, findType), FO_API_ARG_REF(ushort, preBlockHx),
    FO_API_ARG_REF(ushort, preBlockHy), FO_API_ARG_REF(ushort, blockHx), FO_API_ARG_REF(ushort, blockHy))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx)
        FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist)
            FO_API_ARG_MARSHAL(int, findType) FO_API_ARG_REF_MARSHAL(ushort, preBlockHx) FO_API_ARG_REF_MARSHAL(
                ushort, preBlockHy) FO_API_ARG_REF_MARSHAL(ushort, blockHx) FO_API_ARG_REF_MARSHAL(ushort, blockHy))
{
    CritterVec critters;
    UShortPair block, pre_block;
    TraceData trace;
    trace.TraceMap = _this;
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

#ifdef FO_API_MAP_METHOD_DOC
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
#endif
FO_API_MAP_METHOD(GetCrittersWhoViewPath, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(ushort, fromHx),
    FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(int, findType))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx)
        FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(int, findType))
{
    CritterVec critters;
    for (Critter* cr : _this->GetCritters())
    {
        if (cr->CheckFind(findType) && std::find(critters.begin(), critters.end(), cr) == critters.end() &&
            GenericUtils::IntersectCircleLine(
                cr->GetHexX(), cr->GetHexY(), cr->GetLookDistance(), fromHx, fromHy, toHx, toHy))
            critters.push_back(cr);
    }

    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param critters ...
 * @param lookOnThem ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetCrittersSeeing, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG_OBJ_ARR(Critter, critters),
    FO_API_ARG(bool, lookOnThem), FO_API_ARG(int, findType))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Critter, critters) FO_API_ARG_MARSHAL(bool, lookOnThem)
        FO_API_ARG_MARSHAL(int, findType))
{
    CritterVec result_critters;
    for (Critter* cr : critters)
        cr->GetCrFromVisCr(result_critters, findType, !lookOnThem);
    FO_API_RETURN(result_critters);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
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
#endif
FO_API_MAP_METHOD(GetHexInPath, FO_API_RET(void), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy),
    FO_API_ARG_REF(ushort, toHx), FO_API_ARG_REF(ushort, toHy), FO_API_ARG(float, angle), FO_API_ARG(uint, dist))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_REF_MARSHAL(ushort, toHx)
        FO_API_ARG_REF_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist))
{
    UShortPair pre_block, block;
    TraceData trace;
    trace.TraceMap = _this;
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

#ifdef FO_API_MAP_METHOD_DOC
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
#endif
FO_API_MAP_METHOD(GetHexInPathWall, FO_API_RET(void), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy),
    FO_API_ARG_REF(ushort, toHx), FO_API_ARG_REF(ushort, toHy), FO_API_ARG(float, angle), FO_API_ARG(uint, dist))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_REF_MARSHAL(ushort, toHx)
        FO_API_ARG_REF_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist))
{
    UShortPair last_passed;
    TraceData trace;
    trace.TraceMap = _this;
    trace.BeginHx = fromHx;
    trace.BeginHy = fromHy;
    trace.EndHx = toHx;
    trace.EndHy = toHy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.LastPassed = &last_passed;
    _server->MapMngr.TraceBullet(trace);
    if (trace.IsHaveLastPassed)
    {
        toHx = last_passed.first;
        toHy = last_passed.second;
    }
    else
    {
        toHx = fromHx;
        toHy = fromHy;
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
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
#endif
FO_API_MAP_METHOD(GetPathLengthHex, FO_API_RET(uint), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy),
    FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx)
        FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(uint, cut))
{
    if (fromHx >= _this->GetWidth() || fromHy >= _this->GetHeight())
        throw ScriptException("Invalid from hexes args");
    if (toHx >= _this->GetWidth() || toHy >= _this->GetHeight())
        throw ScriptException("Invalid to hexes args");

    PathFindData pfd;
    pfd.MapId = _this->GetId();
    pfd.FromX = fromHx;
    pfd.FromY = fromHy;
    pfd.ToX = toHx;
    pfd.ToY = toHy;
    pfd.Cut = cut;
    uint result = _server->MapMngr.FindPath(pfd);
    if (result != FPATH_OK)
        FO_API_RETURN(0);
    PathStepVec& path = _server->MapMngr.GetPath(pfd.PathNum);
    FO_API_RETURN((uint)path.size());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @param toHx ...
 * @param toHy ...
 * @param cut ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetPathLengthCr, FO_API_RET(uint), FO_API_ARG_OBJ(Critter, cr), FO_API_ARG(ushort, toHx),
    FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy)
        FO_API_ARG_MARSHAL(uint, cut))
{
    if (!cr)
        throw ScriptException("Critter arg is null");

    if (toHx >= _this->GetWidth() || toHy >= _this->GetHeight())
        throw ScriptException("Invalid to hexes args");

    PathFindData pfd;
    pfd.MapId = _this->GetId();
    pfd.FromCritter = cr;
    pfd.FromX = cr->GetHexX();
    pfd.FromY = cr->GetHexY();
    pfd.ToX = toHx;
    pfd.ToY = toHy;
    pfd.Multihex = cr->GetMultihex();
    pfd.Cut = cut;
    uint result = _server->MapMngr.FindPath(pfd);
    if (result != FPATH_OK)
        FO_API_RETURN(0);
    PathStepVec& path = _server->MapMngr.GetPath(pfd.PathNum);
    FO_API_RETURN((uint)path.size());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
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
#endif
FO_API_MAP_METHOD(AddNpc, FO_API_RET_OBJ(Critter), FO_API_ARG(hash, protoId), FO_API_ARG(ushort, hx),
    FO_API_ARG(ushort, hy), FO_API_ARG(uchar, dir), FO_API_ARG_DICT(int, int, props))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, protoId) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy)
        FO_API_ARG_MARSHAL(uchar, dir) FO_API_ARG_DICT_MARSHAL(int, int, props))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");
    ProtoCritter* proto = _server->ProtoMngr.GetProtoCritter(protoId);
    if (!proto)
        throw ScriptException("Proto '{}' not found.", _str().parseHash(protoId));

    Critter* npc = nullptr;
    if (!props.empty())
    {
        Properties props_(Critter::PropertiesRegistrator);
        props_ = proto->Props;
        for (auto& kv : props)
            if (!Properties::SetValueAsIntProps(&props_, kv.first, kv.second))
                FO_API_RETURN(nullptr);

        npc = _server->CrMngr.CreateNpc(protoId, &props_, _this, hx, hy, dir, false);
    }
    else
    {
        npc = _server->CrMngr.CreateNpc(protoId, nullptr, _this, hx, hy, dir, false);
    }

    if (!npc)
        throw ScriptException("Create npc fail");
    FO_API_RETURN(npc);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param npcRole ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(GetNpcCount, FO_API_RET(uint), FO_API_ARG(int, npcRole), FO_API_ARG(int, findType))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, npcRole) FO_API_ARG_MARSHAL(int, findType))
{
    FO_API_RETURN(_this->GetNpcCount(npcRole, findType));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param npcRole ...
 * @param findType ...
 * @param skipCount ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(
    GetNpc, FO_API_RET_OBJ(Critter), FO_API_ARG(int, npcRole), FO_API_ARG(int, findType), FO_API_ARG(uint, skipCount))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, npcRole) FO_API_ARG_MARSHAL(int, findType) FO_API_ARG_MARSHAL(uint, skipCount))
{
    FO_API_RETURN(_this->GetNpc(npcRole, findType, skipCount));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hexX ...
 * @param hexY ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(IsHexPassed, FO_API_RET(bool), FO_API_ARG(ushort, hexX), FO_API_ARG(ushort, hexY))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hexX) FO_API_ARG_MARSHAL(ushort, hexY))
{
    if (hexX >= _this->GetWidth() || hexY >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    FO_API_RETURN(_this->IsHexPassed(hexX, hexY));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hexX ...
 * @param hexY ...
 * @param radius ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(
    IsHexesPassed, FO_API_RET(bool), FO_API_ARG(ushort, hexX), FO_API_ARG(ushort, hexY), FO_API_ARG(uint, radius))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hexX) FO_API_ARG_MARSHAL(ushort, hexY) FO_API_ARG_MARSHAL(uint, radius))
{
    if (hexX >= _this->GetWidth() || hexY >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    FO_API_RETURN(_this->IsHexesPassed(hexX, hexY, radius));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hexX ...
 * @param hexY ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(IsHexRaked, FO_API_RET(bool), FO_API_ARG(ushort, hexX), FO_API_ARG(ushort, hexY))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hexX) FO_API_ARG_MARSHAL(ushort, hexY))
{
    if (hexX >= _this->GetWidth() || hexY >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    FO_API_RETURN(_this->IsHexRaked(hexX, hexY));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hexX ...
 * @param hexY ...
 * @param color ...
 * @param text ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(SetText, FO_API_RET(void), FO_API_ARG(ushort, hexX), FO_API_ARG(ushort, hexY),
    FO_API_ARG(uint, color), FO_API_ARG(string, text))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hexX) FO_API_ARG_MARSHAL(ushort, hexY) FO_API_ARG_MARSHAL(uint, color)
        FO_API_ARG_MARSHAL(string, text))
{
    if (hexX >= _this->GetWidth() || hexY >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");
    _this->SetText(hexX, hexY, color, text, false);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hexX ...
 * @param hexY ...
 * @param color ...
 * @param textMsg ...
 * @param strNum ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(SetTextMsg, FO_API_RET(void), FO_API_ARG(ushort, hexX), FO_API_ARG(ushort, hexY),
    FO_API_ARG(uint, color), FO_API_ARG(ushort, textMsg), FO_API_ARG(uint, strNum))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hexX) FO_API_ARG_MARSHAL(ushort, hexY) FO_API_ARG_MARSHAL(uint, color)
        FO_API_ARG_MARSHAL(ushort, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (hexX >= _this->GetWidth() || hexY >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    _this->SetTextMsg(hexX, hexY, color, textMsg, strNum);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
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
#endif
FO_API_MAP_METHOD(SetTextMsgLex, FO_API_RET(void), FO_API_ARG(ushort, hexX), FO_API_ARG(ushort, hexY),
    FO_API_ARG(uint, color), FO_API_ARG(ushort, textMsg), FO_API_ARG(uint, strNum), FO_API_ARG(string, lexems))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hexX) FO_API_ARG_MARSHAL(ushort, hexY) FO_API_ARG_MARSHAL(uint, color)
        FO_API_ARG_MARSHAL(ushort, textMsg) FO_API_ARG_MARSHAL(uint, strNum) FO_API_ARG_MARSHAL(string, lexems))
{
    if (hexX >= _this->GetWidth() || hexY >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    _this->SetTextMsgLex(hexX, hexY, color, textMsg, strNum, lexems.c_str(), (ushort)lexems.length());
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param effPid ...
 * @param hx ...
 * @param hy ...
 * @param radius ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(RunEffect, FO_API_RET(void), FO_API_ARG(hash, effPid), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy),
    FO_API_ARG(uint, radius))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, effPid) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy)
        FO_API_ARG_MARSHAL(uint, radius))
{
    if (!effPid)
        throw ScriptException("Effect pid invalid arg");
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    _this->SendEffect(effPid, hx, hy, radius);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
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
#endif
FO_API_MAP_METHOD(RunFlyEffect, FO_API_RET(void), FO_API_ARG(hash, effPid), FO_API_ARG_OBJ(Critter, fromCr),
    FO_API_ARG_OBJ(Critter, toCr), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx),
    FO_API_ARG(ushort, toHy))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, effPid) FO_API_ARG_OBJ_MARSHAL(Critter, fromCr)
        FO_API_ARG_OBJ_MARSHAL(Critter, toCr) FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy)
            FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy))
{
    if (!effPid)
        throw ScriptException("Effect pid invalid arg");
    if (fromHx >= _this->GetWidth() || fromHy >= _this->GetHeight())
        throw ScriptException("Invalid from hexes args");
    if (toHx >= _this->GetWidth() || toHy >= _this->GetHeight())
        throw ScriptException("Invalid to hexes args");

    uint from_crid = (fromCr ? fromCr->GetId() : 0);
    uint to_crid = (toCr ? toCr->GetId() : 0);
    _this->SendFlyEffect(effPid, from_crid, to_crid, fromHx, fromHy, toHx, toHy);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(
    CheckPlaceForItem, FO_API_RET(bool), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(hash, pid))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(hash, pid))
{
    ProtoItem* proto_item = _server->ProtoMngr.GetProtoItem(pid);
    if (!proto_item)
        throw ScriptException("Proto item not found");

    FO_API_RETURN(_this->IsPlaceForProtoItem(hx, hy, proto_item));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param full ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(BlockHex, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, full))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(bool, full))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    _this->SetHexFlag(hx, hy, FH_BLOCK_ITEM);
    if (full)
        _this->SetHexFlag(hx, hy, FH_NRAKE_ITEM);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(UnblockHex, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    _this->UnsetHexFlag(hx, hy, FH_BLOCK_ITEM);
    _this->UnsetHexFlag(hx, hy, FH_NRAKE_ITEM);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param soundName ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(PlaySound, FO_API_RET(void), FO_API_ARG(string, soundName))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, soundName))
{
    for (Critter* cr : _this->GetPlayers())
        cr->Send_PlaySound(0, soundName);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param soundName ...
 * @param hx ...
 * @param hy ...
 * @param radius ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(PlaySoundRadius, FO_API_RET(void), FO_API_ARG(string, soundName), FO_API_ARG(ushort, hx),
    FO_API_ARG(ushort, hy), FO_API_ARG(uint, radius))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, soundName) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy)
        FO_API_ARG_MARSHAL(uint, radius))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");

    for (Critter* cr : _this->GetPlayers())
        if (_server->GeomHelper.CheckDist(
                hx, hy, cr->GetHexX(), cr->GetHexY(), radius == 0 ? cr->LookCacheValue : radius))
            cr->Send_PlaySound(0, soundName);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(Reload, FO_API_RET(bool))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG()
{
    _server->MapMngr.RegenerateMap(_this);
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param dir ...
 * @param steps ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(MoveHexByDir, FO_API_RET(void), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy),
    FO_API_ARG(uchar, dir), FO_API_ARG(uint, steps))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, hx) FO_API_ARG_REF_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uchar, dir)
        FO_API_ARG_MARSHAL(uint, steps))
{
    if (dir >= _server->Settings.MapDirCount)
        throw ScriptException("Invalid dir arg");
    if (!steps)
        throw ScriptException("Steps arg is zero");

    ushort maxhx = _this->GetWidth();
    ushort maxhy = _this->GetHeight();
    if (steps > 1)
    {
        for (uint i = 0; i < steps; i++)
            _server->GeomHelper.MoveHexByDir(hx, hy, dir, maxhx, maxhy);
    }
    else
    {
        _server->GeomHelper.MoveHexByDir(hx, hy, dir, maxhx, maxhy);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_MAP_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @param hx ...
 * @param hy ...
 * @param dir ...
 ******************************************************************************/
#endif
FO_API_MAP_METHOD(VerifyTrigger, FO_API_RET(void), FO_API_ARG_OBJ(Critter, cr), FO_API_ARG(ushort, hx),
    FO_API_ARG(ushort, hy), FO_API_ARG(uchar, dir))
#ifdef FO_API_MAP_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy)
        FO_API_ARG_MARSHAL(uchar, dir))
{
    if (hx >= _this->GetWidth() || hy >= _this->GetHeight())
        throw ScriptException("Invalid hexes args");
    if (dir >= _server->Settings.MapDirCount)
        throw ScriptException("Invalid dir arg");

    ushort from_hx = hx, from_hy = hy;
    _server->GeomHelper.MoveHexByDir(
        from_hx, from_hy, _server->GeomHelper.ReverseDir(dir), _this->GetWidth(), _this->GetHeight());
    _server->VerifyTrigger(_this, cr, from_hx, from_hy, hx, hy, dir);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_LOCATION_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_LOCATION_METHOD(GetMapCount, FO_API_RET(uint))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->GetMapsCount());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_LOCATION_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param mapPid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_LOCATION_METHOD(GetMap, FO_API_RET_OBJ(Map), FO_API_ARG(hash, mapPid))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, mapPid))
{
    for (Map* map : _this->GetMaps())
        if (map->GetProtoId() == mapPid)
            FO_API_RETURN(map);
    FO_API_RETURN(nullptr);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_LOCATION_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param index ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_LOCATION_METHOD(GetMapByIndex, FO_API_RET_OBJ(Map), FO_API_ARG(uint, index))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, index))
{
    MapVec maps = _this->GetMaps();
    if (index >= maps.size())
        throw ScriptException("Invalid index arg");

    FO_API_RETURN(maps[index]);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_LOCATION_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_LOCATION_METHOD(GetMaps, FO_API_RET_OBJ_ARR(Map))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->GetMaps());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_LOCATION_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param entrance ...
 * @param mapIndex ...
 * @param entire ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_LOCATION_METHOD(GetEntrance, FO_API_RET(void), FO_API_ARG(uint, entrance), FO_API_ARG_REF(uint, mapIndex),
    FO_API_ARG_REF(hash, entire))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG(
    FO_API_ARG_MARSHAL(uint, entrance) FO_API_ARG_REF_MARSHAL(uint, mapIndex) FO_API_ARG_REF_MARSHAL(hash, entire))
{
    HashVec map_entrances = _this->GetMapEntrances();
    uint count = (uint)map_entrances.size() / 2;
    if (entrance >= count)
        throw ScriptException("Invalid entrance");

    mapIndex = _this->GetMapIndex(map_entrances[entrance * 2]);
    entire = map_entrances[entrance * 2 + 1];
}
FO_API_EPILOG()
#endif

#ifdef FO_API_LOCATION_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param mapsIndex ...
 * @param entires ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_LOCATION_METHOD(
    GetEntrances, FO_API_RET(uint), FO_API_ARG_ARR_REF(int, mapsIndex), FO_API_ARG_ARR_REF(hash, entires))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_ARR_REF_MARSHAL(int, mapsIndex) FO_API_ARG_ARR_REF_MARSHAL(hash, entires))
{
    HashVec map_entrances = _this->GetMapEntrances();
    uint count = (uint)map_entrances.size() / 2;

    for (uint e = 0; e < count; e++)
    {
        int index = _this->GetMapIndex(map_entrances[e * 2]);
        mapsIndex.push_back(index);
        hash entire = map_entrances[e * 2 + 1];
        entires.push_back(entire);
    }

    FO_API_RETURN(count);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_LOCATION_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_LOCATION_METHOD(Reload, FO_API_RET(bool))
#ifdef FO_API_LOCATION_METHOD_IMPL
FO_API_PROLOG()
{
    for (Map* map : _this->GetMaps())
        _server->MapMngr.RegenerateMap(map);

    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param cr1 ...
 * @param cr2 ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(
    GetCrittersDistantion, FO_API_RET(uint), FO_API_ARG_OBJ(Critter, cr1), FO_API_ARG_OBJ(Critter, cr2))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, cr1) FO_API_ARG_OBJ_MARSHAL(Critter, cr2))
{
    if (!cr1)
        throw ScriptException("Critter1 arg is null");

    if (!cr2)
        throw ScriptException("Critter2 arg is null");

    if (cr1->GetMapId() != cr2->GetMapId())
        throw ScriptException("Differernt maps");

    FO_API_RETURN(_server->GeomHelper.DistGame(cr1->GetHexX(), cr1->GetHexY(), cr2->GetHexX(), cr2->GetHexY()));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param itemId ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetItem, FO_API_RET_OBJ(Item), FO_API_ARG(uint, itemId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId))
{
    if (!itemId)
        throw ScriptException("Item id arg is zero");

    Item* item = _server->ItemMngr.GetItem(itemId);
    if (!item || item->IsDestroyed)
        FO_API_RETURN(nullptr);
    FO_API_RETURN(item);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 * @param count ...
 * @param toCr ...
 * @param skipChecks ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(MoveItemCr, FO_API_RET(void), FO_API_ARG_OBJ(Item, item), FO_API_ARG(uint, count),
    FO_API_ARG_OBJ(Critter, toCr), FO_API_ARG(bool, skipChecks))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Item, item) FO_API_ARG_MARSHAL(uint, count) FO_API_ARG_OBJ_MARSHAL(Critter, toCr)
        FO_API_ARG_MARSHAL(bool, skipChecks))
{
    if (!item)
        throw ScriptException("Item arg is null");

    if (!toCr)
        throw ScriptException("Critter arg is null");

    if (!count)
        count = item->GetCount();
    if (count > item->GetCount())
        throw ScriptException("Count arg is greater than maximum");

    _server->ItemMngr.MoveItem(item, count, toCr, skipChecks);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
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
#endif
FO_API_GLOBAL_SERVER_FUNC(MoveItemMap, FO_API_RET(void), FO_API_ARG_OBJ(Item, item), FO_API_ARG(uint, count),
    FO_API_ARG_OBJ(Map, toMap), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(bool, skipChecks))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Item, item) FO_API_ARG_MARSHAL(uint, count) FO_API_ARG_OBJ_MARSHAL(Map, toMap)
        FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(bool, skipChecks))
{
    if (!item)
        throw ScriptException("Item arg is null");

    if (!toMap)
        throw ScriptException("Map arg is null");

    if (toHx >= toMap->GetWidth() || toHy >= toMap->GetHeight())
        throw ScriptException("Invalid hexex args");

    if (!count)
        count = item->GetCount();
    if (count > item->GetCount())
        throw ScriptException("Count arg is greater than maximum");

    _server->ItemMngr.MoveItem(item, count, toMap, toHx, toHy, skipChecks);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 * @param count ...
 * @param toCont ...
 * @param stackId ...
 * @param skipChecks ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(MoveItemCont, FO_API_RET(void), FO_API_ARG_OBJ(Item, item), FO_API_ARG(uint, count),
    FO_API_ARG_OBJ(Item, toCont), FO_API_ARG(uint, stackId), FO_API_ARG(bool, skipChecks))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Item, item) FO_API_ARG_MARSHAL(uint, count) FO_API_ARG_OBJ_MARSHAL(Item, toCont)
        FO_API_ARG_MARSHAL(uint, stackId) FO_API_ARG_MARSHAL(bool, skipChecks))
{
    if (!item)
        throw ScriptException("Item arg is null");

    if (!toCont)
        throw ScriptException("Container arg is null");

    if (!count)
        count = item->GetCount();
    if (count > item->GetCount())
        throw ScriptException("Count arg is greater than maximum");

    _server->ItemMngr.MoveItem(item, count, toCont, stackId, skipChecks);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param items ...
 * @param toCr ...
 * @param skipChecks ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(MoveItemsCr, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Item, items), FO_API_ARG_OBJ(Critter, toCr),
    FO_API_ARG(bool, skipChecks))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(
    FO_API_ARG_OBJ_ARR_MARSHAL(Item, items) FO_API_ARG_OBJ_MARSHAL(Critter, toCr) FO_API_ARG_MARSHAL(bool, skipChecks))
{
    if (!toCr)
        throw ScriptException("Critter arg is null");

    for (Item* item : items)
    {
        if (!item || item->IsDestroyed)
            continue;

        _server->ItemMngr.MoveItem(item, item->GetCount(), toCr, skipChecks);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param items ...
 * @param toMap ...
 * @param toHx ...
 * @param toHy ...
 * @param skipChecks ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(MoveItemsMap, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Item, items), FO_API_ARG_OBJ(Map, toMap),
    FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(bool, skipChecks))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Item, items) FO_API_ARG_OBJ_MARSHAL(Map, toMap)
        FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(bool, skipChecks))
{
    if (!toMap)
        throw ScriptException("Map arg is null");
    if (toHx >= toMap->GetWidth() || toHy >= toMap->GetHeight())
        throw ScriptException("Invalid hexex args");

    for (Item* item : items)
    {
        if (!item || item->IsDestroyed)
            continue;

        _server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHx, toHy, skipChecks);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param items ...
 * @param toCont ...
 * @param stackId ...
 * @param skipChecks ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(MoveItemsCont, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Item, items),
    FO_API_ARG_OBJ(Item, toCont), FO_API_ARG(uint, stackId), FO_API_ARG(bool, skipChecks))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Item, items) FO_API_ARG_OBJ_MARSHAL(Item, toCont)
        FO_API_ARG_MARSHAL(uint, stackId) FO_API_ARG_MARSHAL(bool, skipChecks))
{
    if (!toCont)
        throw ScriptException("Container arg is null");

    for (Item* item : items)
    {
        if (!item || item->IsDestroyed)
            continue;

        _server->ItemMngr.MoveItem(item, item->GetCount(), toCont, stackId, skipChecks);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(DeleteItem, FO_API_RET(void), FO_API_ARG_OBJ(Item, item))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Item, item))
{
    if (item)
        _server->ItemMngr.DeleteItem(item);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param itemId ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(DeleteItemById, FO_API_RET(void), FO_API_ARG(uint, itemId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, itemId))
{
    Item* item = _server->ItemMngr.GetItem(itemId);
    if (item)
        _server->ItemMngr.DeleteItem(item);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param items ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(DeleteItems, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Item, items))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Item, items))
{
    for (Item* item : items)
        if (item)
            _server->ItemMngr.DeleteItem(item);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param items ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(DeleteItemsById, FO_API_RET(void), FO_API_ARG_ARR(uint, itemIds))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_ARR_MARSHAL(uint, itemIds))
{
    ItemVec items_to_delete;
    for (uint item_id : itemIds)
    {
        if (item_id)
        {
            Item* item = _server->ItemMngr.GetItem(item_id);
            if (item)
                _server->ItemMngr.DeleteItem(item);
        }
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param npc ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(DeleteNpc, FO_API_RET(void), FO_API_ARG_OBJ(Critter, npc))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, npc))
{
    if (npc)
        _server->CrMngr.DeleteNpc(npc);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param npcId ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(DeleteNpcById, FO_API_RET(void), FO_API_ARG(uint, npcId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, npcId))
{
    Critter* npc = _server->CrMngr.GetNpc(npcId);
    if (npc)
        _server->CrMngr.DeleteNpc(npc);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param channel ...
 * @param text ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(RadioMessage, FO_API_RET(void), FO_API_ARG(ushort, channel), FO_API_ARG(string, text))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, channel) FO_API_ARG_MARSHAL(string, text))
{
    if (!text.empty())
        _server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, text, false, 0, 0, nullptr);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param channel ...
 * @param textMsg ...
 * @param numStr ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(RadioMessageMsg, FO_API_RET(void), FO_API_ARG(ushort, channel), FO_API_ARG(ushort, textMsg),
    FO_API_ARG(uint, numStr))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, channel) FO_API_ARG_MARSHAL(ushort, textMsg) FO_API_ARG_MARSHAL(uint, numStr))
{
    _server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, "", false, textMsg, numStr, nullptr);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param channel ...
 * @param textMsg ...
 * @param numStr ...
 * @param lexems ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(RadioMessageMsgLex, FO_API_RET(void), FO_API_ARG(ushort, channel),
    FO_API_ARG(ushort, textMsg), FO_API_ARG(uint, numStr), FO_API_ARG(string, lexems))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, channel) FO_API_ARG_MARSHAL(ushort, textMsg) FO_API_ARG_MARSHAL(uint, numStr)
        FO_API_ARG_MARSHAL(string, lexems))
{
    _server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, "", false, textMsg, numStr,
        !lexems.empty() ? lexems.c_str() : nullptr);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
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
#endif
FO_API_GLOBAL_SERVER_FUNC(GetFullSecond, FO_API_RET(uint), FO_API_ARG(ushort, year), FO_API_ARG(ushort, month),
    FO_API_ARG(ushort, day), FO_API_ARG(ushort, hour), FO_API_ARG(ushort, minute), FO_API_ARG(ushort, second))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, year) FO_API_ARG_MARSHAL(ushort, month) FO_API_ARG_MARSHAL(ushort, day)
        FO_API_ARG_MARSHAL(ushort, hour) FO_API_ARG_MARSHAL(ushort, minute) FO_API_ARG_MARSHAL(ushort, second))
{
    if (!year)
        year = _server->Globals->GetYear();
    else
        year = CLAMP(year, _server->Globals->GetYearStart(), _server->Globals->GetYearStart() + 130);
    if (!month)
        month = _server->Globals->GetMonth();
    else
        month = CLAMP(month, 1, 12);
    if (!day)
    {
        day = _server->Globals->GetDay();
    }
    else
    {
        uint month_day = _server->GameTime.GameTimeMonthDay(year, month);
        day = CLAMP(day, 1, month_day);
    }
    if (hour > 23)
        hour = 23;
    if (minute > 59)
        minute = 59;
    if (second > 59)
        second = 59;
    FO_API_RETURN(_server->GameTime.GetFullSecond(year, month, day, hour, minute, second));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
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
#endif
FO_API_GLOBAL_SERVER_FUNC(GetGameTime, FO_API_RET(void), FO_API_ARG(uint, fullSecond), FO_API_ARG_REF(ushort, year),
    FO_API_ARG_REF(ushort, month), FO_API_ARG_REF(ushort, day), FO_API_ARG_REF(ushort, dayOfWeek),
    FO_API_ARG_REF(ushort, hour), FO_API_ARG_REF(ushort, minute), FO_API_ARG_REF(ushort, second))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, fullSecond) FO_API_ARG_REF_MARSHAL(ushort, year)
        FO_API_ARG_REF_MARSHAL(ushort, month) FO_API_ARG_REF_MARSHAL(ushort, day)
            FO_API_ARG_REF_MARSHAL(ushort, dayOfWeek) FO_API_ARG_REF_MARSHAL(ushort, hour)
                FO_API_ARG_REF_MARSHAL(ushort, minute) FO_API_ARG_REF_MARSHAL(ushort, second))
{
    DateTimeStamp dt = _server->GameTime.GetGameTime(fullSecond);
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

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param locPid ...
 * @param wx ...
 * @param wy ...
 * @param critters ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(CreateLocation, FO_API_RET_OBJ(Location), FO_API_ARG(hash, locPid), FO_API_ARG(ushort, wx),
    FO_API_ARG(ushort, wy), FO_API_ARG_OBJ_ARR(Critter, critters))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, locPid) FO_API_ARG_MARSHAL(ushort, wx) FO_API_ARG_MARSHAL(ushort, wy)
        FO_API_ARG_OBJ_ARR_MARSHAL(Critter, critters))
{
    // Create and generate location
    Location* loc = _server->MapMngr.CreateLocation(locPid, wx, wy);
    if (!loc)
        throw ScriptException("Unable to create location '{}'.", _str().parseHash(locPid));

    // Add known locations to critters
    for (Critter* cr : critters)
    {
        _server->MapMngr.AddKnownLoc(cr, loc->GetId());
        if (!cr->GetMapId())
            cr->Send_GlobalLocation(loc, true);
        if (loc->IsNonEmptyAutomaps())
            cr->Send_AutomapsInfo(nullptr, loc);

        ushort zx = loc->GetWorldX() / _server->Settings.GlobalMapZoneLength;
        ushort zy = loc->GetWorldY() / _server->Settings.GlobalMapZoneLength;
        UCharVec gmap_fog = cr->GetGlobalMapFog();
        if (gmap_fog.size() != GM_ZONES_FOG_SIZE)
            gmap_fog.resize(GM_ZONES_FOG_SIZE);
        TwoBitMask gmap_mask(GM__MAXZONEX, GM__MAXZONEY, gmap_fog.data());
        if (gmap_mask.Get2Bit(zx, zy) == GM_FOG_FULL)
        {
            gmap_mask.Set2Bit(zx, zy, GM_FOG_HALF);
            cr->SetGlobalMapFog(gmap_fog);
            if (!cr->GetMapId())
                cr->Send_GlobalMapFog(zx, zy, GM_FOG_HALF);
        }
    }

    FO_API_RETURN(loc);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param loc ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(DeleteLocation, FO_API_RET(void), FO_API_ARG_OBJ(Location, loc))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Location, loc))
{
    if (loc)
        _server->MapMngr.DeleteLocation(loc, nullptr);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param locId ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(DeleteLocationById, FO_API_RET(void), FO_API_ARG(uint, locId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, locId))
{
    Location* loc = _server->MapMngr.GetLocation(locId);
    if (loc)
        _server->MapMngr.DeleteLocation(loc, nullptr);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param crid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetCritter, FO_API_RET_OBJ(Critter), FO_API_ARG(uint, crid))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, crid))
{
    if (!crid)
        FO_API_RETURN(nullptr); // throw ScriptException("Critter id arg is zero");
    FO_API_RETURN(_server->CrMngr.GetCritter(crid));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param name ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetPlayer, FO_API_RET_OBJ(Critter), FO_API_ARG(string, name))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, name))
{
    // Check existence
    uint id = MAKE_CLIENT_ID(name);
    DataBase::Document doc = DbStorage->Get("Players", id);
    if (doc.empty())
        FO_API_RETURN(nullptr);

    // Find online
    Client* cl = _server->CrMngr.GetPlayer(id);
    if (cl)
    {
        cl->AddRef();
        FO_API_RETURN(cl);
    }

    // Load from db
    ProtoCritter* cl_proto = _server->ProtoMngr.GetProtoCritter(_str("Player").toHash());
    RUNTIME_ASSERT(cl_proto);
    cl = new Client(nullptr, cl_proto, _server->Settings, _server->ScriptSys);
    cl->SetId(id);
    cl->Name = name;
    if (!PropertiesSerializator::LoadFromDbDocument(&cl->Props, doc, _server->ScriptSys))
        throw ScriptException("Client data db read failed");
    FO_API_RETURN(cl);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param wx ...
 * @param wy ...
 * @param radius ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetGlobalMapCritters, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(ushort, wx),
    FO_API_ARG(ushort, wy), FO_API_ARG(uint, radius), FO_API_ARG(int, findType))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, wx) FO_API_ARG_MARSHAL(ushort, wy) FO_API_ARG_MARSHAL(uint, radius)
        FO_API_ARG_MARSHAL(int, findType))
{
    CritterVec critters;
    _server->CrMngr.GetGlobalMapCritters(wx, wy, radius, findType, critters);
    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param mapId ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetMap, FO_API_RET_OBJ(Map), FO_API_ARG(uint, mapId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, mapId))
{
    if (!mapId)
        throw ScriptException("Map id arg is zero");

    FO_API_RETURN(_server->MapMngr.GetMap(mapId));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param mapPid ...
 * @param skipCount ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetMapByPid, FO_API_RET_OBJ(Map), FO_API_ARG(hash, mapPid), FO_API_ARG(uint, skipCount))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, mapPid) FO_API_ARG_MARSHAL(uint, skipCount))
{
    if (!mapPid)
        throw ScriptException("Invalid zero map proto id arg");

    FO_API_RETURN(_server->MapMngr.GetMapByPid(mapPid, skipCount));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param locId ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetLocation, FO_API_RET_OBJ(Location), FO_API_ARG(uint, locId))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, locId))
{
    if (!locId)
        throw ScriptException("Location id arg is zero");

    FO_API_RETURN(_server->MapMngr.GetLocation(locId));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param locPid ...
 * @param skipCount ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(
    GetLocationByPid, FO_API_RET_OBJ(Location), FO_API_ARG(hash, locPid), FO_API_ARG(uint, skipCount))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, locPid) FO_API_ARG_MARSHAL(uint, skipCount))
{
    if (!locPid)
        throw ScriptException("Invalid zero location proto id arg");

    FO_API_RETURN(_server->MapMngr.GetLocationByPid(locPid, skipCount));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param wx ...
 * @param wy ...
 * @param radius ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetLocations, FO_API_RET_OBJ_ARR(Location), FO_API_ARG(ushort, wx), FO_API_ARG(ushort, wy),
    FO_API_ARG(uint, radius))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, wx) FO_API_ARG_MARSHAL(ushort, wy) FO_API_ARG_MARSHAL(uint, radius))
{
    LocationVec locations;
    LocationVec all_locations;
    _server->MapMngr.GetLocations(all_locations);
    for (auto it = all_locations.begin(), end = all_locations.end(); it != end; ++it)
    {
        Location* loc = *it;
        if (GenericUtils::DistSqrt(wx, wy, loc->GetWorldX(), loc->GetWorldY()) <= radius + loc->GetRadius())
            locations.push_back(loc);
    }

    FO_API_RETURN(locations);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param wx ...
 * @param wy ...
 * @param radius ...
 * @param cr ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetVisibleLocations, FO_API_RET_OBJ_ARR(Location), FO_API_ARG(ushort, wx),
    FO_API_ARG(ushort, wy), FO_API_ARG(uint, radius), FO_API_ARG_OBJ(Critter, cr))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, wx) FO_API_ARG_MARSHAL(ushort, wy) FO_API_ARG_MARSHAL(uint, radius)
        FO_API_ARG_OBJ_MARSHAL(Critter, cr))
{
    LocationVec locations;
    LocationVec all_locations;
    _server->MapMngr.GetLocations(all_locations);
    for (auto it = all_locations.begin(), end = all_locations.end(); it != end; ++it)
    {
        Location* loc = *it;
        if (GenericUtils::DistSqrt(wx, wy, loc->GetWorldX(), loc->GetWorldY()) <= radius + loc->GetRadius() &&
            (loc->IsLocVisible() || (cr && cr->IsPlayer() && _server->MapMngr.CheckKnownLocById(cr, loc->GetId()))))
            locations.push_back(loc);
    }

    FO_API_RETURN(locations);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param zx ...
 * @param zy ...
 * @param zoneRadius ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetZoneLocationIds, FO_API_RET_ARR(uint), FO_API_ARG(ushort, zx), FO_API_ARG(ushort, zy),
    FO_API_ARG(uint, zoneRadius))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, zx) FO_API_ARG_MARSHAL(ushort, zy) FO_API_ARG_MARSHAL(uint, zoneRadius))
{
    UIntVec loc_ids;
    _server->MapMngr.GetZoneLocations(zx, zy, zoneRadius, loc_ids);
    FO_API_RETURN(loc_ids);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param player ...
 * @param npc ...
 * @param ignoreDistance ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(RunDialogNpc, FO_API_RET(bool), FO_API_ARG_OBJ(Critter, player), FO_API_ARG_OBJ(Critter, npc),
    FO_API_ARG(bool, ignoreDistance))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, player) FO_API_ARG_OBJ_MARSHAL(Critter, npc)
        FO_API_ARG_MARSHAL(bool, ignoreDistance))
{
    if (!player)
        throw ScriptException("Player arg is null");

    if (!player->IsPlayer())
        throw ScriptException("Player arg is not player");
    if (!npc)
        throw ScriptException("Npc arg is null");

    if (!npc->IsNpc())
        throw ScriptException("Npc arg is not npc");
    Client* cl = (Client*)player;
    if (cl->Talk.Locked)
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");

    _server->Dialog_Begin(cl, (Npc*)npc, 0, 0, 0, ignoreDistance);
    FO_API_RETURN(cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == npc->GetId());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param player ...
 * @param npc ...
 * @param dlgPack ...
 * @param ignoreDistance ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(RunDialogNpcDlgPack, FO_API_RET(bool), FO_API_ARG_OBJ(Critter, player),
    FO_API_ARG_OBJ(Critter, npc), FO_API_ARG(uint, dlgPack), FO_API_ARG(bool, ignoreDistance))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, player) FO_API_ARG_OBJ_MARSHAL(Critter, npc)
        FO_API_ARG_MARSHAL(uint, dlgPack) FO_API_ARG_MARSHAL(bool, ignoreDistance))
{
    if (!player)
        throw ScriptException("Player arg is null");

    if (!player->IsPlayer())
        throw ScriptException("Player arg is not player");
    if (!npc)
        throw ScriptException("Npc arg is null");

    if (!npc->IsNpc())
        throw ScriptException("Npc arg is not npc");
    Client* cl = (Client*)player;
    if (cl->Talk.Locked)
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");

    _server->Dialog_Begin(cl, (Npc*)npc, dlgPack, 0, 0, ignoreDistance);
    FO_API_RETURN(cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == npc->GetId());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
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
#endif
FO_API_GLOBAL_SERVER_FUNC(RunDialogHex, FO_API_RET(bool), FO_API_ARG_OBJ(Critter, player), FO_API_ARG(uint, dlgPack),
    FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, ignoreDistance))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Critter, player) FO_API_ARG_MARSHAL(uint, dlgPack) FO_API_ARG_MARSHAL(ushort, hx)
        FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(bool, ignoreDistance))
{
    if (!player)
        throw ScriptException("Player arg is null");

    if (!player->IsPlayer())
        throw ScriptException("Player arg is not player");
    if (!_server->DlgMngr.GetDialog(dlgPack))
        throw ScriptException("Dialog not found");
    Client* cl = (Client*)player;
    if (cl->Talk.Locked)
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");

    _server->Dialog_Begin(cl, nullptr, dlgPack, hx, hy, ignoreDistance);
    FO_API_RETURN(cl->Talk.TalkType == TALK_WITH_HEX && cl->Talk.TalkHexX == hx && cl->Talk.TalkHexY == hy);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(WorldItemCount, FO_API_RET(int64), FO_API_ARG(hash, pid))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    if (!_server->ProtoMngr.GetProtoItem(pid))
        throw ScriptException("Invalid protoId arg");

    FO_API_RETURN(_server->ItemMngr.GetItemStatistics(pid));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param sayType ...
 * @param firstStr ...
 * @param parameter ...
 * @param func ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(AddTextListener, FO_API_RET(void), FO_API_ARG(int, sayType), FO_API_ARG(string, firstStr),
    FO_API_ARG(uint, parameter), FO_API_ARG_CALLBACK(func))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, sayType) FO_API_ARG_MARSHAL(string, firstStr) FO_API_ARG_MARSHAL(uint, parameter)
        FO_API_ARG_CALLBACK_MARSHAL(func))
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

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param sayType ...
 * @param firstStr ...
 * @param parameter ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(EraseTextListener, FO_API_RET(void), FO_API_ARG(int, sayType), FO_API_ARG(string, firstStr),
    FO_API_ARG(uint, parameter))
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
    UNSETFLAG(npc->Flags, FCRIT_PLAYER);
    SETFLAG(npc->Flags, FCRIT_NPC);
}
#endif
#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param cr1 ...
 * @param cr2 ...
 * @param withInventory ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(SwapCritters, FO_API_RET(void), FO_API_ARG_OBJ(Critter, cr1), FO_API_ARG_OBJ(Critter, cr2),
    FO_API_ARG(bool, withInventory))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(
    FO_API_ARG_OBJ_MARSHAL(Critter, cr1) FO_API_ARG_OBJ_MARSHAL(Critter, cr2) FO_API_ARG_MARSHAL(bool, withInventory))
{
    // Check
    if (!cr1)
        throw ScriptException("Critter1 arg is null");
    if (!cr2)
        throw ScriptException("Critter2 arg is null");

    if (cr1 == cr2)
        throw ScriptException("Critter1 is equal to Critter2");
    if (!cr1->GetMapId())
        throw ScriptException("Critter1 is on global map");
    if (!cr2->GetMapId())
        throw ScriptException("Critter2 is on global map");

    // Swap positions
    Map* map1 = _server->MapMngr.GetMap(cr1->GetMapId());
    if (!map1)
        throw ScriptException("Map of Critter1 not found");
    Map* map2 = _server->MapMngr.GetMap(cr2->GetMapId());
    if (!map2)
        throw ScriptException("Map of Critter2 not found");

    CritterVec& cr_map1 = map1->GetCrittersRaw();
    ClientVec& cl_map1 = map1->GetPlayersRaw();
    NpcVec& npc_map1 = map1->GetNpcsRaw();
    auto it_cr = std::find(cr_map1.begin(), cr_map1.end(), cr1);
    if (it_cr != cr_map1.end())
        cr_map1.erase(it_cr);
    auto it_cl = std::find(cl_map1.begin(), cl_map1.end(), (Client*)cr1);
    if (it_cl != cl_map1.end())
        cl_map1.erase(it_cl);
    auto it_pc = std::find(npc_map1.begin(), npc_map1.end(), (Npc*)cr1);
    if (it_pc != npc_map1.end())
        npc_map1.erase(it_pc);

    CritterVec& cr_map2 = map2->GetCrittersRaw();
    ClientVec& cl_map2 = map2->GetPlayersRaw();
    NpcVec& npc_map2 = map2->GetNpcsRaw();
    it_cr = std::find(cr_map2.begin(), cr_map2.end(), cr1);
    if (it_cr != cr_map2.end())
        cr_map2.erase(it_cr);
    it_cl = std::find(cl_map2.begin(), cl_map2.end(), (Client*)cr1);
    if (it_cl != cl_map2.end())
        cl_map2.erase(it_cl);
    it_pc = std::find(npc_map2.begin(), npc_map2.end(), (Npc*)cr1);
    if (it_pc != npc_map2.end())
        npc_map2.erase(it_pc);

    cr_map2.push_back(cr1);
    if (cr1->IsNpc())
        npc_map2.push_back((Npc*)cr1);
    else
        cl_map2.push_back((Client*)cr1);
    cr_map1.push_back(cr2);
    if (cr2->IsNpc())
        npc_map1.push_back((Npc*)cr2);
    else
        cl_map1.push_back((Client*)cr2);

    // Swap data
    std::swap(cr1->Props, cr2->Props);
    std::swap(cr1->Flags, cr2->Flags);
    cr1->SetBreakTime(0);
    cr2->SetBreakTime(0);

    // Swap inventory
    if (withInventory)
    {
        ItemVec items1 = cr1->GetInventory();
        ItemVec items2 = cr2->GetInventory();
        for (auto it = items1.begin(), end = items1.end(); it != end; ++it)
            _server->CrMngr.EraseItemFromCritter(cr1, *it, false);
        for (auto it = items2.begin(), end = items2.end(); it != end; ++it)
            _server->CrMngr.EraseItemFromCritter(cr2, *it, false);
        for (auto it = items1.begin(), end = items1.end(); it != end; ++it)
            _server->CrMngr.AddItemToCritter(cr2, *it, false);
        for (auto it = items2.begin(), end = items2.end(); it != end; ++it)
            _server->CrMngr.AddItemToCritter(cr1, *it, false);
    }

    // Refresh
    cr1->ClearVisible();
    cr2->ClearVisible();

    auto swap_critters_refresh_client = [_server](Client* cl, Map* map, Map* prev_map) {
        UNSETFLAG(cl->Flags, FCRIT_NPC);
        SETFLAG(cl->Flags, FCRIT_PLAYER);

        if (cl->Talk.TalkType != TALK_NONE)
            _server->CrMngr.CloseTalk(cl);

        if (map != prev_map)
        {
            cl->Send_LoadMap(nullptr, _server->MapMngr);
        }
        else
        {
            cl->Send_AllProperties();
            cl->Send_AddAllItems();
            cl->Send_AllAutomapsInfo(_server->MapMngr);
        }
    };

    if (cr1->IsNpc())
        SwapCrittersRefreshNpc((Npc*)cr1);
    else
        swap_critters_refresh_client((Client*)cr1, map2, map1);

    if (cr2->IsNpc())
        SwapCrittersRefreshNpc((Npc*)cr2);
    else
        swap_critters_refresh_client((Client*)cr2, map1, map2);

    if (map1 == map2)
    {
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

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetAllItems, FO_API_RET_OBJ_ARR(Item), FO_API_ARG(hash, pid))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    ItemVec items;
    ItemVec all_items;
    _server->ItemMngr.GetGameItems(all_items);
    for (auto it = all_items.begin(), end = all_items.end(); it != end; ++it)
    {
        Item* item = *it;
        if (!item->IsDestroyed && (!pid || pid == item->GetProtoId()))
            items.push_back(item);
    }

    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetOnlinePlayers, FO_API_RET_OBJ_ARR(Critter))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG()
{
    CritterVec players;
    ClientVec all_players;
    _server->CrMngr.GetClients(all_players);
    for (auto it = all_players.begin(), end = all_players.end(); it != end; ++it)
    {
        Critter* player_ = *it;
        if (!player_->IsDestroyed && player_->IsPlayer())
            players.push_back(player_);
    }

    FO_API_RETURN(players);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetRegisteredPlayerIds, FO_API_RET_ARR(uint))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(DbStorage->GetAllIds("Players"));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetAllNpc, FO_API_RET_OBJ_ARR(Critter), FO_API_ARG(hash, pid))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    CritterVec npcs;
    NpcVec all_npcs;
    _server->CrMngr.GetNpcs(all_npcs);
    for (auto it = all_npcs.begin(), end = all_npcs.end(); it != end; ++it)
    {
        Npc* npc_ = *it;
        if (!npc_->IsDestroyed && (!pid || pid == npc_->GetProtoId()))
            npcs.push_back(npc_);
    }

    FO_API_RETURN(npcs);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetAllMaps, FO_API_RET_OBJ_ARR(Map), FO_API_ARG(hash, pid))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    MapVec maps;
    MapVec all_maps;
    _server->MapMngr.GetMaps(all_maps);
    for (auto it = all_maps.begin(), end = all_maps.end(); it != end; ++it)
    {
        Map* map = *it;
        if (!pid || pid == map->GetProtoId())
            maps.push_back(map);
    }

    FO_API_RETURN(maps);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(GetAllLocations, FO_API_RET_OBJ_ARR(Location), FO_API_ARG(hash, pid))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    LocationVec locations;
    LocationVec all_locations;
    _server->MapMngr.GetLocations(all_locations);
    for (auto it = all_locations.begin(), end = all_locations.end(); it != end; ++it)
    {
        Location* loc = *it;
        if (!pid || pid == loc->GetProtoId())
            locations.push_back(loc);
    }

    FO_API_RETURN(locations);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
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
#endif
FO_API_GLOBAL_SERVER_FUNC(GetTime, FO_API_RET(void), FO_API_ARG_REF(ushort, year), FO_API_ARG_REF(ushort, month),
    FO_API_ARG_REF(ushort, day), FO_API_ARG_REF(ushort, dayOfWeek), FO_API_ARG_REF(ushort, hour),
    FO_API_ARG_REF(ushort, minute), FO_API_ARG_REF(ushort, second), FO_API_ARG_REF(ushort, milliseconds))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, year) FO_API_ARG_REF_MARSHAL(ushort, month)
        FO_API_ARG_REF_MARSHAL(ushort, day) FO_API_ARG_REF_MARSHAL(ushort, dayOfWeek)
            FO_API_ARG_REF_MARSHAL(ushort, hour) FO_API_ARG_REF_MARSHAL(ushort, minute)
                FO_API_ARG_REF_MARSHAL(ushort, second) FO_API_ARG_REF_MARSHAL(ushort, milliseconds))
{
    DateTimeStamp cur_time;
    Timer::GetCurrentDateTime(cur_time);
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

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
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
#endif
FO_API_GLOBAL_SERVER_FUNC(SetTime, FO_API_RET(void), FO_API_ARG(ushort, multiplier), FO_API_ARG(ushort, year),
    FO_API_ARG(ushort, month), FO_API_ARG(ushort, day), FO_API_ARG(ushort, hour), FO_API_ARG(ushort, minute),
    FO_API_ARG(ushort, second))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, multiplier) FO_API_ARG_MARSHAL(ushort, year) FO_API_ARG_MARSHAL(ushort, month)
        FO_API_ARG_MARSHAL(ushort, day) FO_API_ARG_MARSHAL(ushort, hour) FO_API_ARG_MARSHAL(ushort, minute)
            FO_API_ARG_MARSHAL(ushort, second))
{
    _server->SetGameTime(multiplier, year, month, day, hour, minute, second);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param index ...
 * @param enableSend ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(AllowSlot, FO_API_RET(void), FO_API_ARG(uchar, index), FO_API_ARG(bool, enableSend))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, index) FO_API_ARG_MARSHAL(bool, enableSend))
{
    Critter::SlotEnabled[index] = true;
    Critter::SlotDataSendEnabled[index] = enableSend;
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param datName ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(AddDataSource, FO_API_RET(void), FO_API_ARG(string, datName))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, datName))
{
    _server->FileMngr.AddDataSource(datName, false);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
struct ServerImage
{
    UCharVec Data;
    uint Width;
    uint Height;
    uint Depth;
};
static vector<ServerImage*> ServerImages;
#endif
#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param index ...
 * @param imageName ...
 * @param imageDepth ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(
    LoadImage, FO_API_RET(bool), FO_API_ARG(uint, index), FO_API_ARG(string, imageName), FO_API_ARG(uint, imageDepth))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(
    FO_API_ARG_MARSHAL(uint, index) FO_API_ARG_MARSHAL(string, imageName) FO_API_ARG_MARSHAL(uint, imageDepth))
{
    // Delete old
    if (index >= ServerImages.size())
        ServerImages.resize(index + 1);
    if (ServerImages[index])
    {
        delete ServerImages[index];
        ServerImages[index] = nullptr;
    }
    if (imageName.empty())
        FO_API_RETURN(true);

    // Check depth
    static uint image_depth_;
    image_depth_ = imageDepth; // Avoid GCC warning "argument 'image_depth' might be clobbered by 'longjmp' or 'vfork'"
    if (imageDepth < 1 || imageDepth > 4)
        throw ScriptException("Wrong image depth arg");

    // Check extension
    string ext = _str(imageName).getFileExtension();
    if (ext != "png")
        throw ScriptException("Wrong extension. Allowed only PNG");

    // Load file to memory
    FileCollection images = _server->FileMngr.FilterFiles("png");
    File file = images.FindFile(imageName.substr(0, imageName.find_last_of('.')));
    if (!file)
        throw ScriptException("File '{}' not found.", imageName);

    // Load PNG from memory
    png_structp pp = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = nullptr;
    if (pp)
        info = png_create_info_struct(pp);
    if (!pp || !info)
    {
        if (pp)
            png_destroy_read_struct(&pp, nullptr, nullptr);
        throw ScriptException("Cannot allocate memory to read PNG data");
    }

    if (setjmp(png_jmpbuf(pp)))
    {
        png_destroy_read_struct(&pp, &info, nullptr);
        throw ScriptException("PNG data contains errors");
    }

    struct png_mem_data_
    {
        png_structp pp;
        const unsigned char* current;
        const unsigned char* last;

        static void png_read_data_from_mem(png_structp png_ptr, png_bytep data, png_size_t length)
        {
            png_mem_data_* png_mem_data = (png_mem_data_*)png_get_io_ptr(png_ptr);
            if (png_mem_data->current + length > png_mem_data->last)
            {
                png_error(png_mem_data->pp, "Invalid attempt to read row data.");
                FO_API_RETURN_VOID();
            }
            memcpy(data, png_mem_data->current, length);
            png_mem_data->current += length;
        }
    } png_mem_data;
    png_mem_data.current = file.GetBuf();
    png_mem_data.last = file.GetBuf() + file.GetFsize();
    png_mem_data.pp = pp;
    png_set_read_fn(pp, (png_voidp)&png_mem_data, png_mem_data.png_read_data_from_mem);

    png_read_info(pp, info);

    if (png_get_color_type(pp, info) == PNG_COLOR_TYPE_PALETTE)
        png_set_expand(pp);

    int channels = 1;
    if (png_get_color_type(pp, info) & PNG_COLOR_MASK_COLOR)
        channels = 3;

    int num_trans = 0;
    png_get_tRNS(pp, info, 0, &num_trans, 0);
    if ((png_get_color_type(pp, info) & PNG_COLOR_MASK_ALPHA) || (num_trans != 0))
        channels++;

    int w = (int)png_get_image_width(pp, info);
    int h = (int)png_get_image_height(pp, info);
    int d = channels;

    if (png_get_bit_depth(pp, info) < 8)
    {
        png_set_packing(pp);
        png_set_expand(pp);
    }
    else if (png_get_bit_depth(pp, info) == 16)
        png_set_strip_16(pp);

    if (png_get_valid(pp, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(pp);

    uchar* data = new uchar[w * h * d];
    png_bytep* rows = new png_bytep[h];

    for (int i = 0; i < h; i++)
        rows[i] = (png_bytep)(data + i * w * d);

    for (int i = png_set_interlace_handling(pp); i > 0; i--)
        png_read_rows(pp, rows, nullptr, h);

    delete[] rows;
    png_read_end(pp, info);
    png_destroy_read_struct(&pp, &info, nullptr);

    // Copy data
    ServerImage* simg = new ServerImage();
    simg->Width = w;
    simg->Height = h;
    simg->Depth = image_depth_;
    simg->Data.resize(simg->Width * simg->Height * simg->Depth + 3); // +3 padding

    const uint argb_offs[4] = {2, 1, 0, 3};
    uint min_depth = MIN((uint)d, simg->Depth);
    uint data_index = 0;
    uint png_data_index = 0;
    for (uint y = 0; y < simg->Height; y++)
    {
        for (uint x = 0; x < simg->Width; x++)
        {
            memzero(&simg->Data[data_index], simg->Depth);
            for (uint j = 0; j < min_depth; j++)
                simg->Data[data_index + j] = *(data + png_data_index + argb_offs[j]);
            png_data_index += d;
            data_index += simg->Depth;
        }
    }
    delete[] data;

    ServerImages[index] = simg;
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param index ...
 * @param x ...
 * @param y ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(
    GetImageColor, FO_API_RET(uint), FO_API_ARG(uint, index), FO_API_ARG(uint, x), FO_API_ARG(uint, y))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, index) FO_API_ARG_MARSHAL(uint, x) FO_API_ARG_MARSHAL(uint, y))
{
    if (index >= ServerImages.size() || !ServerImages[index])
        throw ScriptException("Image not loaded");
    ServerImage* simg = ServerImages[index];
    if (x >= simg->Width || y >= simg->Height)
        throw ScriptException("Invalid coords arg");

    uint* data = (uint*)(&simg->Data[0] + y * simg->Width * simg->Depth + x * simg->Depth);
    uint result = *data;
    switch (simg->Depth)
    {
    case 1:
        result &= 0xFF;
        break;
    case 2:
        result &= 0xFFFF;
        break;
    case 3:
        result &= 0xFFFFFF;
        break;
    default:
        break;
    }
    FO_API_RETURN(result);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
static size_t WriteMemoryCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    string& result = *(string*)userdata;
    size_t len = size * nmemb;
    if (len)
    {
        result.resize(result.size() + len);
        memcpy(&result[result.size() - len], ptr, len);
    }
    return len;
}

static void YieldWebRequestInternal(FOServer* server, string url, vector<string> headers, map<string, string> post1,
    string post2, bool& success, string& result)
{
    success = false;
    result = "";

    /*asIScriptContext* ctx = server->ScriptSys.SuspendCurrentContext(uint(-1));
    if (!ctx)
        return;

    struct RequestData
    {
        asIScriptContext* Context;
        string Url;
        CScriptArray* Headers;
        CScriptDict* Post1;
        string Post2;
        bool* Success;
        string* Result;
    };

    RequestData* request_data = new RequestData();
    request_data->Context = ctx;
    request_data->Url = url;
    request_data->Headers = headers;
    request_data->Post1 = post1;
    request_data->Post2 = post2;
    request_data->Success = &success;
    request_data->Result = &result;

    if (headers)
        headers->AddRef();
    if (post1)
        post1->AddRef();

    auto request_func = [](void* data) {
        static bool curl_inited = false;
        if (!curl_inited)
        {
            curl_inited = true;
            curl_global_init(CURL_GLOBAL_ALL);
        }

        RequestData* request_data = (RequestData*)data;

        bool success = false;
        string result;

        CURL* curl = curl_easy_init();
        if (curl)
        {
            curl_easy_setopt(curl, CURLOPT_URL, request_data->Url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

            curl_slist* header_list = nullptr;
            if (request_data->Headers && request_data->Headers->GetSize())
            {
                for (uint i = 0, j = request_data->Headers->GetSize(); i < j; i++)
                    header_list = curl_slist_append(header_list, (*(string*)request_data->Headers->At(i)).c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
            }

            string post;
            if (request_data->Post1)
            {
                for (uint i = 0, j = request_data->Post1->GetSize(); i < j; i++)
                {
                    string& key = *(string*)request_data->Post1->GetKey(i);
                    string& value = *(string*)request_data->Post1->GetValue(i);
                    char* escaped_key = curl_easy_escape(curl, key.c_str(), (int)key.length());
                    char* escaped_value = curl_easy_escape(curl, value.c_str(), (int)value.length());
                    if (i > 0)
                        post += "&";
                    post += escaped_key;
                    post += "=";
                    post += escaped_value;
                    curl_free(escaped_key);
                    curl_free(escaped_value);
                }
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.c_str());
            }
            else if (!request_data->Post2.empty())
            {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_data->Post2.c_str());
            }

            CURLcode curl_res = curl_easy_perform(curl);
            if (curl_res == CURLE_OK)
            {
                success = true;
            }
            else
            {
                result = "curl_easy_perform() failed: ";
                result += curl_easy_strerror(curl_res);
            }

            curl_easy_cleanup(curl);
            if (header_list)
                curl_slist_free_all(header_list);
        }
        else
        {
            result = "curl_easy_init fail";
        }

        *request_data->Success = success;
        *request_data->Result = result;
        FOServer::Self->ScriptSys.ResumeContext(request_data->Context);
        if (request_data->Headers)
            request_data->Headers->Release();
        if (request_data->Post1)
            request_data->Post1->Release();
        delete request_data;
    };
    std::thread(request_func, request_data).detach();*/
}
#endif
#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param url ...
 * @param post ...
 * @param success ...
 * @param result ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(YieldWebRequest, FO_API_RET(void), FO_API_ARG(string, url),
    FO_API_ARG_DICT(string, string, post), FO_API_ARG_REF(bool, success), FO_API_ARG_REF(string, result))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, url) FO_API_ARG_DICT_MARSHAL(string, string, post)
        FO_API_ARG_REF_MARSHAL(bool, success) FO_API_ARG_REF_MARSHAL(string, result))
{
    // YieldWebRequestInternal(_server, url, nullptr, post, "", success, result);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_SERVER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param url ...
 * @param headers ...
 * @param post ...
 * @param success ...
 * @param result ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_SERVER_FUNC(YieldWebRequestExt, FO_API_RET(void), FO_API_ARG(string, url),
    FO_API_ARG_ARR(string, headers), FO_API_ARG(string, post), FO_API_ARG_REF(bool, success),
    FO_API_ARG_REF(string, result))
#ifdef FO_API_GLOBAL_SERVER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, url) FO_API_ARG_ARR_MARSHAL(string, headers) FO_API_ARG_MARSHAL(string, post)
        FO_API_ARG_REF_MARSHAL(bool, success) FO_API_ARG_REF_MARSHAL(string, result))
{
    // YieldWebRequestInternal(_server, url, headers, nullptr, post, success, result);
}
FO_API_EPILOG()
#endif
