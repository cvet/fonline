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

#ifdef FO_API_MAPPER_IMPL
#include "Mapper.h"
#endif

#ifdef FO_API_ITEM_VIEW_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_ITEM_VIEW_METHOD(AddChild, FO_API_RET_OBJ(ItemView), FO_API_ARG(hash, pid))
#ifdef FO_API_ITEM_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    ProtoItem* proto_item = _mapper->ProtoMngr.GetProtoItem(pid);
    if (!proto_item || proto_item->IsStatic())
        throw ScriptException("Added child is not item");

    FO_API_RETURN(_mapper->AddItem(pid, 0, 0, _this));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_VIEW_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_VIEW_METHOD(AddChild, FO_API_RET_OBJ(ItemView), FO_API_ARG(hash, pid))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid))
{
    ProtoItem* proto_item = _mapper->ProtoMngr.GetProtoItem(pid);
    if (!proto_item || proto_item->IsStatic())
        throw ScriptException("Added child is not item");

    FO_API_RETURN(_mapper->AddItem(pid, 0, 0, _this));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_ITEM_VIEW_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_ITEM_VIEW_METHOD(GetChildren, FO_API_RET_OBJ_ARR(ItemView))
#ifdef FO_API_ITEM_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    ItemViewVec children;
    // Todo: need attention!
    // _this->ContGetItems(children, 0);
    FO_API_RETURN(children);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_CRITTER_VIEW_METHOD_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_CRITTER_VIEW_METHOD(GetChildren, FO_API_RET_OBJ_ARR(ItemView))
#ifdef FO_API_CRITTER_VIEW_METHOD_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_this->InvItems);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(AddItem, FO_API_RET_OBJ(ItemView), FO_API_ARG(hash, pid), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid), FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _mapper->HexMngr.GetWidth() || hy >= _mapper->HexMngr.GetHeight())
        throw ScriptException("Invalid hex args");
    ProtoItem* proto = _mapper->ProtoMngr.GetProtoItem(pid);
    if (!proto)
        throw ScriptException("Invalid item prototype");

    FO_API_RETURN(_mapper->AddItem(pid, hx, hy, nullptr));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(AddCritter, FO_API_RET_OBJ(CritterView), FO_API_ARG(hash, pid), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid), FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _mapper->HexMngr.GetWidth() || hy >= _mapper->HexMngr.GetHeight())
        throw ScriptException("Invalid hex args");
    ProtoCritter* proto = _mapper->ProtoMngr.GetProtoCritter(pid);
    if (!proto)
        throw ScriptException("Invalid critter prototype");

    FO_API_RETURN(_mapper->AddCritter(pid, hx, hy));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetItemByHex, FO_API_RET_OBJ(ItemView), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy))
{
    FO_API_RETURN(_mapper->HexMngr.GetItem(hx, hy, 0));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetItemsByHex, FO_API_RET_OBJ_ARR(ItemView), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy))
{
    ItemHexViewVec items;
    _mapper->HexMngr.GetItems(hx, hy, items);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetCritterByHex, FO_API_RET_OBJ(CritterView), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(int, findType))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy), FO_API_ARG_MARSHAL(int, findType))
{
    CritterViewVec critters_;
    _mapper->HexMngr.GetCritters(hx, hy, critters_, findType);
    FO_API_RETURN(!critters_.empty() ? critters_[0] : nullptr);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetCrittersByHex, FO_API_RET_OBJ_ARR(CritterView), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(int, findType))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy), FO_API_ARG_MARSHAL(int, findType))
{
    CritterViewVec critters;
    _mapper->HexMngr.GetCritters(hx, hy, critters, findType);
    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param entity ...
 * @param hx ...
 * @param hy ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(MoveEntity, FO_API_RET(void), FO_API_ARG_OBJ(Entity, entity), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Entity, entity), FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _mapper->HexMngr.GetWidth())
        hx = _mapper->HexMngr.GetWidth() - 1;
    if (hy >= _mapper->HexMngr.GetHeight())
        hy = _mapper->HexMngr.GetHeight() - 1;
    _mapper->MoveEntity(entity, hx, hy);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param entity ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(DeleteEntity, FO_API_RET(void), FO_API_ARG_OBJ(Entity, entity))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Entity, entity))
{
    _mapper->DeleteEntity(entity);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param entities ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(DeleteEntities, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Entity, entities))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Entity, entities))
{
    for (int i = 0, j = entities->GetSize(); i < j; i++)
    {
        Entity* entity = *(Entity**)entities->At(i);
        if (entity && !entity->IsDestroyed)
            _mapper->DeleteEntity(entity);
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param entity ...
 * @param set ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(SelectEntity, FO_API_RET(void), FO_API_ARG_OBJ(Entity, entity), FO_API_ARG(bool, set))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Entity, entity), FO_API_ARG_MARSHAL(bool, set))
{
    if (!entity)
        throw ScriptException("Entity arg is null");

    if (set)
        _mapper->SelectAdd(entity);
    else
        _mapper->SelectErase(entity);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param entities ...
 * @param set ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(SelectEntities, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Entity, entities), FO_API_ARG(bool, set))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Entity, entities), FO_API_ARG_MARSHAL(bool, set))
{
    for (int i = 0, j = entities->GetSize(); i < j; i++)
    {
        Entity* entity = *(Entity**)entities->At(i);
        if (entity)
        {
            if (set)
                _mapper->SelectAdd(entity);
            else
                _mapper->SelectErase(entity);
        }
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetSelectedEntity, FO_API_RET_OBJ(Entity))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_mapper->SelectedEntities.size() ? _mapper->SelectedEntities[0] : nullptr);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetSelectedEntities, FO_API_RET_OBJ_ARR(Entity))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG()
{
    EntityVec entities;
    entities.reserve(_mapper->SelectedEntities.size());
    for (uint i = 0, j = (uint)_mapper->SelectedEntities.size(); i < j; i++)
        entities.push_back(_mapper->SelectedEntities[i]);
    FO_API_RETURN(_mapper->ScriptSys.CreateArrayRef("Entity[]", entities));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param roof ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetTilesCount, FO_API_RET(uint), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, roof))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy), FO_API_ARG_MARSHAL(bool, roof))
{
    if (!_mapper->HexMngr.IsMapLoaded())
        throw ScriptException("Map not loaded");
    if (hx >= _mapper->HexMngr.GetWidth())
        throw ScriptException("Invalid hex x arg");
    if (hy >= _mapper->HexMngr.GetHeight())
        throw ScriptException("Invalid hex y arg");

    MapTileVec& tiles = _mapper->HexMngr.GetTiles(hx, hy, roof);
    FO_API_RETURN((uint)tiles.size());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param roof ...
 * @param layer ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(DeleteTile, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, roof), FO_API_ARG(int, layer))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy), FO_API_ARG_MARSHAL(bool, roof), FO_API_ARG_MARSHAL(int, layer))
{
    if (!_mapper->HexMngr.IsMapLoaded())
        throw ScriptException("Map not loaded");
    if (hx >= _mapper->HexMngr.GetWidth())
        throw ScriptException("Invalid hex x arg");
    if (hy >= _mapper->HexMngr.GetHeight())
        throw ScriptException("Invalid hex y arg");

    bool deleted = false;
    MapTileVec& tiles = _mapper->HexMngr.GetTiles(hx, hy, roof);
    Field& f = _mapper->HexMngr.GetField(hx, hy);
    if (layer < 0)
    {
        deleted = !tiles.empty();
        tiles.clear();
        while (f.GetTilesCount(roof))
            f.EraseTile(0, roof);
    }
    else
    {
        for (size_t i = 0, j = tiles.size(); i < j; i++)
        {
            if (tiles[i].Layer == layer)
            {
                tiles.erase(tiles.begin() + i);
                f.EraseTile((uint)i, roof);
                deleted = true;
                break;
            }
        }
    }

    if (deleted)
    {
        if (roof)
            _mapper->HexMngr.RebuildRoof();
        else
            _mapper->HexMngr.RebuildTiles();
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param roof ...
 * @param layer ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetTileHash, FO_API_RET(hash), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, roof), FO_API_ARG(int, layer))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy), FO_API_ARG_MARSHAL(bool, roof), FO_API_ARG_MARSHAL(int, layer))
{
    if (!_mapper->HexMngr.IsMapLoaded())
        throw ScriptException("Map not loaded");
    if (hx >= _mapper->HexMngr.GetWidth())
        throw ScriptException("Invalid hex x arg");
    if (hy >= _mapper->HexMngr.GetHeight())
        throw ScriptException("Invalid hex y arg");

    MapTileVec& tiles = _mapper->HexMngr.GetTiles(hx, hy, roof);
    for (size_t i = 0, j = tiles.size(); i < j; i++)
    {
        if (tiles[i].Layer == layer)
            FO_API_RETURN(tiles[i].Name);
    }
    FO_API_RETURN(0);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param ox ...
 * @param oy ...
 * @param layer ...
 * @param roof ...
 * @param picHash ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(AddTileHash, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(int, ox), FO_API_ARG(int, oy), FO_API_ARG(int, layer), FO_API_ARG(bool, roof), FO_API_ARG(hash, picHash))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy), FO_API_ARG_MARSHAL(int, ox), FO_API_ARG_MARSHAL(int, oy), FO_API_ARG_MARSHAL(int, layer), FO_API_ARG_MARSHAL(bool, roof), FO_API_ARG_MARSHAL(hash, picHash))
{
    if (!_mapper->HexMngr.IsMapLoaded())
        throw ScriptException("Map not loaded");
    if (hx >= _mapper->HexMngr.GetWidth())
        throw ScriptException("Invalid hex x arg");
    if (hy >= _mapper->HexMngr.GetHeight())
        throw ScriptException("Invalid hex y arg");

    if (!picHash)
        FO_API_RETURN_VOID();

    layer = CLAMP(layer, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END);

    _mapper->HexMngr.SetTile(picHash, hx, hy, ox, oy, layer, roof, false);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param roof ...
 * @param layer ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetTileName, FO_API_RET(string), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, roof), FO_API_ARG(int, layer))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy), FO_API_ARG_MARSHAL(bool, roof), FO_API_ARG_MARSHAL(int, layer))
{
    if (!_mapper->HexMngr.IsMapLoaded())
        throw ScriptException("Map not loaded");
    if (hx >= _mapper->HexMngr.GetWidth())
        throw ScriptException("Invalid hex x arg");
    if (hy >= _mapper->HexMngr.GetHeight())
        throw ScriptException("Invalid hex y arg");

    MapTileVec& tiles = _mapper->HexMngr.GetTiles(hx, hy, roof);
    for (size_t i = 0, j = tiles.size(); i < j; i++)
    {
        if (tiles[i].Layer == layer)
            FO_API_RETURN(_str().parseHash(tiles[i].Name));
    }
    FO_API_RETURN("");
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param ox ...
 * @param oy ...
 * @param layer ...
 * @param roof ...
 * @param picName ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(AddTileName, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(int, ox), FO_API_ARG(int, oy), FO_API_ARG(int, layer), FO_API_ARG(bool, roof), FO_API_ARG(string, picName))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy), FO_API_ARG_MARSHAL(int, ox), FO_API_ARG_MARSHAL(int, oy), FO_API_ARG_MARSHAL(int, layer), FO_API_ARG_MARSHAL(bool, roof), FO_API_ARG_MARSHAL(string, picName))
{
    if (!_mapper->HexMngr.IsMapLoaded())
        throw ScriptException("Map not loaded");
    if (hx >= _mapper->HexMngr.GetWidth())
        throw ScriptException("Invalid hex x arg");
    if (hy >= _mapper->HexMngr.GetHeight())
        throw ScriptException("Invalid hex y arg");

    if (picName.empty())
        FO_API_RETURN_VOID();

    layer = CLAMP(layer, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END);

    hash pic_hash = _str(picName).toHash();
    _mapper->HexMngr.SetTile(pic_hash, hx, hy, ox, oy, layer, roof, false);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param index ...
 * @param enableSend ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(AllowSlot, FO_API_RET(void), FO_API_ARG(uchar, index), FO_API_ARG(bool, enableSend))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, index), FO_API_ARG_MARSHAL(bool, enableSend))
{
    //
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param gen ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(SetPropertyGetCallback, FO_API_RET(void), FO_API_ARG_OBJ(asIScriptGeneric, gen))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(asIScriptGeneric, gen))
{
    int prop_enum_value = gen->GetArgDWord(0);
    void* ref = gen->GetArgAddress(1);
    gen->SetReturnByte(0);
    RUNTIME_ASSERT(ref);

    Property* prop = GlobalVars::PropertiesRegistrator->FindByEnum(prop_enum_value);
    prop = (prop ? prop : CritterView::PropertiesRegistrator->FindByEnum(prop_enum_value));
    prop = (prop ? prop : ItemView::PropertiesRegistrator->FindByEnum(prop_enum_value));
    prop = (prop ? prop : MapView::PropertiesRegistrator->FindByEnum(prop_enum_value));
    prop = (prop ? prop : LocationView::PropertiesRegistrator->FindByEnum(prop_enum_value));
    prop = (prop ? prop : GlobalVars::PropertiesRegistrator->FindByEnum(prop_enum_value));
    if (!prop)
        throw ScriptException("Property '{}' not found.", _str().parseHash(prop_enum_value));

    string result = prop->SetGetCallback(*(asIScriptFunction**)ref);
    if (result != "")
        throw ScriptException(result.c_str());

    gen->SetReturnByte(1);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param fileName ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(LoadMap, FO_API_RET_OBJ(MapView), FO_API_ARG(string, fileName))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, fileName))
{
    ProtoMap* pmap = new ProtoMap(_str(fileName).toHash());
    // Todo: need attention!
    // if (!pmap->EditorLoad(_mapper->ServerFileMngr, _mapper->ProtoMngr, _mapper->SprMngr, _mapper->ResMngr))
    //     FO_API_RETURN(nullptr);

    MapView* map = new MapView(0, pmap);
    _mapper->LoadedMaps.push_back(map);
    _mapper->RunMapLoadScript(map);
    FO_API_RETURN(map);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(UnloadMap, FO_API_RET(void), FO_API_ARG_OBJ(MapView, map))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(MapView, map))
{
    if (!map)
        throw ScriptException("Proto map arg nullptr");

    auto it = std::find(_mapper->LoadedMaps.begin(), _mapper->LoadedMaps.end(), map);
    if (it != _mapper->LoadedMaps.end())
        _mapper->LoadedMaps.erase(it);

    if (map == _mapper->ActiveMap)
    {
        _mapper->HexMngr.UnloadMap();
        _mapper->SelectedEntities.clear();
        _mapper->ActiveMap = nullptr;
    }

    map->Proto->Release();
    map->Release();
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 * @param customName ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(SaveMap, FO_API_RET(bool), FO_API_ARG_OBJ(MapView, map), FO_API_ARG(string, customName))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(MapView, map), FO_API_ARG_MARSHAL(string, customName))
{
    if (!map)
        throw ScriptException("Proto map arg nullptr");

    // Todo: need attention!
    //((ProtoMap*)map->Proto)->EditorSave(_mapper->ServerFileMngr, customName);
    _mapper->RunMapSaveScript(map);
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(ShowMap, FO_API_RET(bool), FO_API_ARG_OBJ(MapView, map))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(MapView, map))
{
    if (!map)
        throw ScriptException("Proto map arg nullptr");

    if (_mapper->ActiveMap == map)
        FO_API_RETURN(true);

    _mapper->SelectClear();
    if (!_mapper->HexMngr.SetProtoMap(*(ProtoMap*)map->Proto))
        FO_API_RETURN(false);
    _mapper->HexMngr.FindSetCenter(map->GetWorkHexX(), map->GetWorkHexY());
    _mapper->ActiveMap = map;
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param index ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetLoadedMaps, FO_API_RET_OBJ_ARR(MapView), FO_API_ARG_REF(int, index))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(int, index))
{
    index = -1;
    for (int i = 0, j = (int)_mapper->LoadedMaps.size(); i < j; i++)
    {
        MapView* map = _mapper->LoadedMaps[i];
        if (map == _mapper->ActiveMap)
            index = i;
    }
    FO_API_RETURN(_mapper->LoadedMaps);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param dir ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetMapFileNames, FO_API_RET_ARR(string), FO_API_ARG(string, dir))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, dir))
{
    CScriptArray* names = _mapper->ScriptSys.CreateArray("string[]");
    FileCollection map_files = _mapper->ServerFileMngr.FilterFiles("fomap", dir);
    while (map_files.MoveNext())
    {
        FileHeader file_header = map_files.GetCurFileHeader();
        string fname = file_header.GetName();
        names->InsertLast(&fname);
    }
    FO_API_RETURN(names);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param width ...
 * @param height ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(ResizeMap, FO_API_RET(void), FO_API_ARG(ushort, width), FO_API_ARG(ushort, height))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, width), FO_API_ARG_MARSHAL(ushort, height))
{
    if (!_mapper->HexMngr.IsMapLoaded())
        throw ScriptException("Map not loaded");

    RUNTIME_ASSERT(_mapper->ActiveMap);
    ProtoMap* pmap = (ProtoMap*)_mapper->ActiveMap->Proto;

    // Unload current
    _mapper->HexMngr.GetProtoMap(*pmap);
    _mapper->SelectClear();
    _mapper->HexMngr.UnloadMap();

    // Check size
    int maxhx = CLAMP(width, MAXHEX_MIN, MAXHEX_MAX);
    int maxhy = CLAMP(height, MAXHEX_MIN, MAXHEX_MAX);
    int old_maxhx = pmap->GetWidth();
    int old_maxhy = pmap->GetHeight();
    maxhx = CLAMP(maxhx, MAXHEX_MIN, MAXHEX_MAX);
    maxhy = CLAMP(maxhy, MAXHEX_MIN, MAXHEX_MAX);
    if (pmap->GetWorkHexX() >= maxhx)
        pmap->SetWorkHexX(maxhx - 1);
    if (pmap->GetWorkHexY() >= maxhy)
        pmap->SetWorkHexY(maxhy - 1);
    pmap->SetWidth(maxhx);
    pmap->SetHeight(maxhy);

    // Delete truncated entities
    if (maxhx < old_maxhx || maxhy < old_maxhy)
    {
        // Todo: need attention!
        /*for (auto it = pmap->AllEntities.begin(); it != pmap->AllEntities.end();)
        {
            Entity* entity = *it;
            int hx = (entity->Type == EntityType::CritterView ? ((CritterView*)entity)->GetHexX() :
                                                                ((ItemHexView*)entity)->GetHexX());
            int hy = (entity->Type == EntityType::CritterView ? ((CritterView*)entity)->GetHexY() :
                                                                ((ItemHexView*)entity)->GetHexY());
            if (hx >= maxhx || hy >= maxhy)
            {
                entity->Release();
                it = pmap->AllEntities.erase(it);
            }
            else
            {
                ++it;
            }
        }*/
    }

    // Delete truncated tiles
    if (maxhx < old_maxhx || maxhy < old_maxhy)
    {
        // Todo: need attention!
        /*for (auto it = pmap->Tiles.begin(); it != pmap->Tiles.end();)
        {
            MapTile& tile = *it;
            if (tile.HexX >= maxhx || tile.HexY >= maxhy)
                it = pmap->Tiles.erase(it);
            else
                ++it;
        }*/
    }

    // Update visibility
    _mapper->HexMngr.SetProtoMap(*pmap);
    _mapper->HexMngr.FindSetCenter(pmap->GetWorkHexX(), pmap->GetWorkHexY());
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param dirNames ...
 * @param includeSubdirs ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(TabGetTileDirs, FO_API_RET(uint), FO_API_ARG(int, tab), FO_API_ARG_ARR_REF(string, dirNames), FO_API_ARG_ARR_REF(bool, includeSubdirs))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab), FO_API_ARG_ARR_REF_MARSHAL(string, dirNames), FO_API_ARG_ARR_REF_MARSHAL(bool, includeSubdirs))
{
    if (tab < 0 || tab >= TAB_COUNT)
        throw ScriptException("Wrong tab arg");

    TileTab& ttab = _mapper->TabsTiles[tab];
    if (dirNames)
    {
        asUINT i = dirNames->GetSize();
        dirNames->Resize(dirNames->GetSize() + (uint)ttab.TileDirs.size());
        for (uint k = 0, l = (uint)ttab.TileDirs.size(); k < l; k++, i++)
        {
            string& p = *(string*)dirNames->At(i);
            p = ttab.TileDirs[k];
        }
    }
    if (includeSubdirs)
        _mapper->ScriptSys.AppendVectorToArray(ttab.TileSubDirs, includeSubdirs);
    FO_API_RETURN((uint)ttab.TileDirs.size());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param subTab ...
 * @param itemPids ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(TabGetItemPids, FO_API_RET(uint), FO_API_ARG(int, tab), FO_API_ARG(string, subTab), FO_API_ARG_ARR_REF(hash, itemPids))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab), FO_API_ARG_MARSHAL(string, subTab), FO_API_ARG_ARR_REF_MARSHAL(hash, itemPids))
{
    if (tab < 0 || tab >= TAB_COUNT)
        throw ScriptException("Wrong tab arg");
    if (!subTab.empty() && !_mapper->Tabs[tab].count(subTab))
        FO_API_RETURN(0);

    SubTab& stab = _mapper->Tabs[tab][!subTab.empty() ? subTab : DEFAULT_SUB_TAB];
    if (itemPids)
        _mapper->ScriptSys.AppendVectorToArray(stab.ItemProtos, itemPids);
    FO_API_RETURN((uint)stab.ItemProtos.size());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param subTab ...
 * @param critterPids ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(TabGetCritterPids, FO_API_RET(uint), FO_API_ARG(int, tab), FO_API_ARG(string, subTab), FO_API_ARG_ARR_REF(hash, critterPids))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab), FO_API_ARG_MARSHAL(string, subTab), FO_API_ARG_ARR_REF_MARSHAL(hash, critterPids))
{
    if (tab < 0 || tab >= TAB_COUNT)
        throw ScriptException("Wrong tab arg");
    if (!subTab.empty() && !_mapper->Tabs[tab].count(subTab))
        FO_API_RETURN(0);

    SubTab& stab = _mapper->Tabs[tab][!subTab.empty() ? subTab : DEFAULT_SUB_TAB];
    if (critterPids)
        _mapper->ScriptSys.AppendVectorToArray(stab.NpcProtos, critterPids);
    FO_API_RETURN((uint)stab.NpcProtos.size());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param dirNames ...
 * @param includeSubdirs ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(TabSetTileDirs, FO_API_RET(void), FO_API_ARG(int, tab), FO_API_ARG_ARR(string, dirNames), FO_API_ARG_ARR(bool, includeSubdirs))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab), FO_API_ARG_ARR_MARSHAL(string, dirNames), FO_API_ARG_ARR_MARSHAL(bool, includeSubdirs))
{
    if (tab < 0 || tab >= TAB_COUNT)
        throw ScriptException("Wrong tab arg");
    if (dirNames && includeSubdirs && dirNames->GetSize() != includeSubdirs->GetSize())
        FO_API_RETURN_VOID();

    TileTab& ttab = _mapper->TabsTiles[tab];
    ttab.TileDirs.clear();
    ttab.TileSubDirs.clear();

    if (dirNames)
    {
        for (uint i = 0, j = dirNames->GetSize(); i < j; i++)
        {
            string& name = *(string*)dirNames->At(i);
            if (!name.empty())
            {
                ttab.TileDirs.push_back(name);
                ttab.TileSubDirs.push_back(includeSubdirs ? *(bool*)includeSubdirs->At(i) : false);
            }
        }
    }

    _mapper->RefreshTiles(tab);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param subTab ...
 * @param itemPids ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(TabSetItemPids, FO_API_RET(void), FO_API_ARG(int, tab), FO_API_ARG(string, subTab), FO_API_ARG_ARR(hash, itemPids))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab), FO_API_ARG_MARSHAL(string, subTab), FO_API_ARG_ARR_MARSHAL(hash, itemPids))
{
    if (tab < 0 || tab >= TAB_COUNT)
        throw ScriptException("Wrong tab arg");
    if (subTab.empty() || subTab == DEFAULT_SUB_TAB)
        FO_API_RETURN_VOID();

    // Add protos to sub tab
    if (itemPids && itemPids->GetSize())
    {
        ProtoItemVec proto_items;
        for (int i = 0, j = itemPids->GetSize(); i < j; i++)
        {
            hash pid = *(hash*)itemPids->At(i);
            ProtoItem* proto_item = _mapper->ProtoMngr.GetProtoItem(pid);
            if (proto_item)
                proto_items.push_back(proto_item);
        }

        if (proto_items.size())
        {
            SubTab& stab = _mapper->Tabs[tab][subTab];
            stab.ItemProtos = proto_items;
        }
    }
    // Delete sub tab
    else
    {
        auto it = _mapper->Tabs[tab].find(subTab);
        if (it != _mapper->Tabs[tab].end())
        {
            if (_mapper->TabsActive[tab] == &it->second)
                _mapper->TabsActive[tab] = nullptr;
            _mapper->Tabs[tab].erase(it);
        }
    }

    // Recalculate whole pids
    SubTab& stab_default = _mapper->Tabs[tab][DEFAULT_SUB_TAB];
    stab_default.ItemProtos.clear();
    for (auto it = _mapper->Tabs[tab].begin(), end = _mapper->Tabs[tab].end(); it != end; ++it)
    {
        SubTab& stab = it->second;
        if (&stab == &stab_default)
            continue;
        for (uint i = 0, j = (uint)stab.ItemProtos.size(); i < j; i++)
            stab_default.ItemProtos.push_back(stab.ItemProtos[i]);
    }
    if (!_mapper->TabsActive[tab])
        _mapper->TabsActive[tab] = &stab_default;

    // Refresh
    _mapper->RefreshCurProtos();
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param subTab ...
 * @param critterPids ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(TabSetCritterPids, FO_API_RET(void), FO_API_ARG(int, tab), FO_API_ARG(string, subTab), FO_API_ARG_ARR(hash, critterPids))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab), FO_API_ARG_MARSHAL(string, subTab), FO_API_ARG_ARR_MARSHAL(hash, critterPids))
{
    if (tab < 0 || tab >= TAB_COUNT)
        throw ScriptException("Wrong tab arg");
    if (subTab.empty() || subTab == DEFAULT_SUB_TAB)
        FO_API_RETURN_VOID();

    // Add protos to sub tab
    if (critterPids && critterPids->GetSize())
    {
        ProtoCritterVec cr_protos;
        for (int i = 0, j = critterPids->GetSize(); i < j; i++)
        {
            hash pid = *(hash*)critterPids->At(i);
            ProtoCritter* cr_data = _mapper->ProtoMngr.GetProtoCritter(pid);
            if (cr_data)
                cr_protos.push_back(cr_data);
        }

        if (cr_protos.size())
        {
            SubTab& stab = _mapper->Tabs[tab][subTab];
            stab.NpcProtos = cr_protos;
        }
    }
    // Delete sub tab
    else
    {
        auto it = _mapper->Tabs[tab].find(subTab);
        if (it != _mapper->Tabs[tab].end())
        {
            if (_mapper->TabsActive[tab] == &it->second)
                _mapper->TabsActive[tab] = nullptr;
            _mapper->Tabs[tab].erase(it);
        }
    }

    // Recalculate whole pids
    SubTab& stab_default = _mapper->Tabs[tab][DEFAULT_SUB_TAB];
    stab_default.NpcProtos.clear();
    for (auto it = _mapper->Tabs[tab].begin(), end = _mapper->Tabs[tab].end(); it != end; ++it)
    {
        SubTab& stab = it->second;
        if (&stab == &stab_default)
            continue;
        for (uint i = 0, j = (uint)stab.NpcProtos.size(); i < j; i++)
            stab_default.NpcProtos.push_back(stab.NpcProtos[i]);
    }
    if (!_mapper->TabsActive[tab])
        _mapper->TabsActive[tab] = &stab_default;

    // Refresh
    _mapper->RefreshCurProtos();
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param tab ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(TabDelete, FO_API_RET(void), FO_API_ARG(int, tab))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab))
{
    if (tab < 0 || tab >= TAB_COUNT)
        throw ScriptException("Wrong tab arg");

    _mapper->Tabs[tab].clear();
    SubTab& stab_default = _mapper->Tabs[tab][DEFAULT_SUB_TAB];
    _mapper->TabsActive[tab] = &stab_default;
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param subTab ...
 * @param show ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(TabSelect, FO_API_RET(void), FO_API_ARG(int, tab), FO_API_ARG(string, subTab), FO_API_ARG(bool, show))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab), FO_API_ARG_MARSHAL(string, subTab), FO_API_ARG_MARSHAL(bool, show))
{
    if (tab < 0 || tab >= INT_MODE_COUNT)
        throw ScriptException("Wrong tab arg");

    if (show)
        _mapper->IntSetMode(tab);

    if (tab < 0 || tab >= TAB_COUNT)
        FO_API_RETURN_VOID();

    auto it = _mapper->Tabs[tab].find(!subTab.empty() ? subTab : DEFAULT_SUB_TAB);
    if (it != _mapper->Tabs[tab].end())
        _mapper->TabsActive[tab] = &it->second;
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param tabName ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(TabSetName, FO_API_RET(void), FO_API_ARG(int, tab), FO_API_ARG(string, tabName))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab), FO_API_ARG_MARSHAL(string, tabName))
{
    if (tab < 0 || tab >= INT_MODE_COUNT)
        throw ScriptException("Wrong tab arg");

    _mapper->TabsName[tab] = tabName;
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param speed ...
 * @param canStop ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(MoveScreenToHex, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, speed), FO_API_ARG(bool, canStop))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy), FO_API_ARG_MARSHAL(uint, speed), FO_API_ARG_MARSHAL(bool, canStop))
{
    if (hx >= _mapper->HexMngr.GetWidth() || hy >= _mapper->HexMngr.GetHeight())
        throw ScriptException("Invalid hex args");
    if (!_mapper->HexMngr.IsMapLoaded())
        throw ScriptException("Map is not loaded");

    if (!speed)
        _mapper->HexMngr.FindSetCenter(hx, hy);
    else
        _mapper->HexMngr.ScrollToHex(hx, hy, (float)speed / 1000.0f, canStop);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param ox ...
 * @param oy ...
 * @param speed ...
 * @param canStop ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(MoveScreenOffset, FO_API_RET(void), FO_API_ARG(int, ox), FO_API_ARG(int, oy), FO_API_ARG(uint, speed), FO_API_ARG(bool, canStop))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, ox), FO_API_ARG_MARSHAL(int, oy), FO_API_ARG_MARSHAL(uint, speed), FO_API_ARG_MARSHAL(bool, canStop))
{
    if (!_mapper->HexMngr.IsMapLoaded())
        throw ScriptException("Map is not loaded");

    _mapper->HexMngr.ScrollOffset(ox, oy, (float)speed / 1000.0f, canStop);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param dir ...
 * @param steps ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(MoveHexByDir, FO_API_RET(void), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy), FO_API_ARG(uchar, dir), FO_API_ARG(uint, steps))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, hx), FO_API_ARG_REF_MARSHAL(ushort, hy), FO_API_ARG_MARSHAL(uchar, dir), FO_API_ARG_MARSHAL(uint, steps))
{
    if (!_mapper->HexMngr.IsMapLoaded())
        throw ScriptException("Map not loaded");
    if (dir >= _mapper->Settings.MapDirCount)
        throw ScriptException("Invalid dir arg");
    if (!steps)
        throw ScriptException("Steps arg is zero");
    int hx_ = hx, hy_ = hy;
    if (steps > 1)
    {
        for (uint i = 0; i < steps; i++)
            _mapper->GeomHelper.MoveHexByDirUnsafe(hx_, hy_, dir);
    }
    else
    {
        _mapper->GeomHelper.MoveHexByDirUnsafe(hx_, hy_, dir);
    }
    if (hx_ < 0)
        hx_ = 0;
    if (hy_ < 0)
        hy_ = 0;
    hx = hx_;
    hy = hy_;
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param key ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetIfaceIniStr, FO_API_RET(string), FO_API_ARG(string, key))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, key))
{
    FO_API_RETURN(_mapper->IfaceIni.GetStr("", key, ""));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param msg ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(Message, FO_API_RET(void), FO_API_ARG(string, msg))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, msg))
{
    _mapper->AddMess(msg.c_str());
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(MessageMsg, FO_API_RET(void), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg), FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");
    _mapper->AddMess(_mapper->CurLang.Msg[textMsg].GetStr(strNum).c_str());
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
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
#endif
FO_API_GLOBAL_MAPPER_FUNC(MapMessage, FO_API_RET(void), FO_API_ARG(string, text), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, ms), FO_API_ARG(uint, color), FO_API_ARG(bool, fade), FO_API_ARG(int, ox), FO_API_ARG(int, oy))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text), FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy), FO_API_ARG_MARSHAL(uint, ms), FO_API_ARG_MARSHAL(uint, color), FO_API_ARG_MARSHAL(bool, fade), FO_API_ARG_MARSHAL(int, ox), FO_API_ARG_MARSHAL(int, oy))
{
    FOMapper::MapText t;
    t.HexX = hx;
    t.HexY = hy;
    t.Color = (color ? color : COLOR_TEXT);
    t.Fade = fade;
    t.StartTick = Timer::FastTick();
    t.Tick = ms;
    t.Text = text;
    t.Pos = _mapper->HexMngr.GetRectForText(hx, hy);
    t.EndPos = Rect(t.Pos, ox, oy);
    auto it = std::find(_mapper->GameMapTexts.begin(), _mapper->GameMapTexts.end(), t);
    if (it != _mapper->GameMapTexts.end())
        _mapper->GameMapTexts.erase(it);
    _mapper->GameMapTexts.push_back(t);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetMsgStr, FO_API_RET(string), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg), FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");

    FO_API_RETURN(_mapper->CurLang.Msg[textMsg].GetStr(strNum));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @param skipCount ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetMsgStrSkip, FO_API_RET(string), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum), FO_API_ARG(uint, skipCount))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg), FO_API_ARG_MARSHAL(uint, strNum), FO_API_ARG_MARSHAL(uint, skipCount))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");

    FO_API_RETURN(_mapper->CurLang.Msg[textMsg].GetStr(strNum, skipCount));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetMsgStrNumUpper, FO_API_RET(uint), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg), FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");

    FO_API_RETURN(_mapper->CurLang.Msg[textMsg].GetStrNumUpper(strNum));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetMsgStrNumLower, FO_API_RET(uint), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg), FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");

    FO_API_RETURN(_mapper->CurLang.Msg[textMsg].GetStrNumLower(strNum));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetMsgStrCount, FO_API_RET(uint), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg), FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");

    FO_API_RETURN(_mapper->CurLang.Msg[textMsg].Count(strNum));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(IsMsgStr, FO_API_RET(bool), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg), FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT)
        throw ScriptException("Invalid text msg arg");

    FO_API_RETURN(_mapper->CurLang.Msg[textMsg].Count(strNum) > 0);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param replace ...
 * @param str ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(ReplaceTextStr, FO_API_RET(string), FO_API_ARG(string, text), FO_API_ARG(string, replace), FO_API_ARG(string, str))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text), FO_API_ARG_MARSHAL(string, replace), FO_API_ARG_MARSHAL(string, str))
{
    size_t pos = text.find(replace, 0);
    if (pos == std::string::npos)
        FO_API_RETURN(text);
    FO_API_RETURN(string(text).replace(pos, replace.length(), text));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param replace ...
 * @param i ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(ReplaceTextInt, FO_API_RET(string), FO_API_ARG(string, text), FO_API_ARG(string, replace), FO_API_ARG(int, i))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text), FO_API_ARG_MARSHAL(string, replace), FO_API_ARG_MARSHAL(int, i))
{
    size_t pos = text.find(replace, 0);
    if (pos == std::string::npos)
        FO_API_RETURN(text);
    FO_API_RETURN(string(text).replace(pos, replace.length(), _str("{}", i)));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
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
FO_API_GLOBAL_MAPPER_FUNC(GetHexInPath, FO_API_RET(void), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG_REF(ushort, toHx), FO_API_ARG_REF(ushort, toHy), FO_API_ARG(float, angle), FO_API_ARG(uint, dist))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx), FO_API_ARG_MARSHAL(ushort, fromHy), FO_API_ARG_REF_MARSHAL(ushort, toHx), FO_API_ARG_REF_MARSHAL(ushort, toHy), FO_API_ARG_MARSHAL(float, angle), FO_API_ARG_MARSHAL(uint, dist))
{
    UShortPair pre_block, block;
    _mapper->HexMngr.TraceBullet(
        fromHx, fromHy, toHx, toHy, dist, angle, nullptr, false, nullptr, 0, &block, &pre_block, nullptr, true);
    toHx = pre_block.first;
    toHy = pre_block.second;
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
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
FO_API_GLOBAL_MAPPER_FUNC(GetPathLengthHex, FO_API_RET(uint), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx), FO_API_ARG_MARSHAL(ushort, fromHy), FO_API_ARG_MARSHAL(ushort, toHx), FO_API_ARG_MARSHAL(ushort, toHy), FO_API_ARG_MARSHAL(uint, cut))
{
    if (fromHx >= _mapper->HexMngr.GetWidth() || fromHy >= _mapper->HexMngr.GetHeight())
        throw ScriptException("Invalid from hexes args");
    if (toHx >= _mapper->HexMngr.GetWidth() || toHy >= _mapper->HexMngr.GetHeight())
        throw ScriptException("Invalid to hexes args");

    if (cut > 0 && !_mapper->HexMngr.CutPath(nullptr, fromHx, fromHy, toHx, toHy, cut))
        FO_API_RETURN(0);
    UCharVec steps;
    if (!_mapper->HexMngr.FindPath(nullptr, fromHx, fromHy, toHx, toHy, steps, -1))
        FO_API_RETURN(0);
    FO_API_RETURN((uint)steps.size());
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param x ...
 * @param y ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetHexPos, FO_API_RET(bool), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG_REF(int, x), FO_API_ARG_REF(int, y))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx), FO_API_ARG_MARSHAL(ushort, hy), FO_API_ARG_REF_MARSHAL(int, x), FO_API_ARG_REF_MARSHAL(int, y))
{
    x = y = 0;
    if (_mapper->HexMngr.IsMapLoaded() && hx < _mapper->HexMngr.GetWidth() && hy < _mapper->HexMngr.GetHeight())
    {
        _mapper->HexMngr.GetHexCurrentPosition(hx, hy, x, y);
        x += _mapper->Settings.ScrOx + (_mapper->Settings.MapHexWidth / 2);
        y += _mapper->Settings.ScrOy + (_mapper->Settings.MapHexHeight / 2);
        x = (int)(x / _mapper->Settings.SpritesZoom);
        y = (int)(y / _mapper->Settings.SpritesZoom);
        FO_API_RETURN(true);
    }
    FO_API_RETURN(false);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param x ...
 * @param y ...
 * @param hx ...
 * @param hy ...
 * @param ignoreInterface ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetMonitorHex, FO_API_RET(bool), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy), FO_API_ARG(bool, ignoreInterface))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x), FO_API_ARG_MARSHAL(int, y), FO_API_ARG_REF_MARSHAL(ushort, hx), FO_API_ARG_REF_MARSHAL(ushort, hy), FO_API_ARG_MARSHAL(bool, ignoreInterface))
{
    ushort hx_, hy_;
    int old_x = _mapper->Settings.MouseX;
    int old_y = _mapper->Settings.MouseY;
    _mapper->Settings.MouseX = x;
    _mapper->Settings.MouseY = y;
    bool result = _mapper->GetCurHex(hx_, hy_, ignoreInterface);
    _mapper->Settings.MouseX = old_x;
    _mapper->Settings.MouseY = old_y;
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

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param x ...
 * @param y ...
 * @param ignoreInterface ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetMonitorObject, FO_API_RET_OBJ(Entity), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(bool, ignoreInterface))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x), FO_API_ARG_MARSHAL(int, y), FO_API_ARG_MARSHAL(bool, ignoreInterface))
{
    if (!_mapper->HexMngr.IsMapLoaded())
        throw ScriptException("Map not loaded");

    if (!ignoreInterface && _mapper->IsCurInInterface())
        FO_API_RETURN(nullptr);

    ItemHexView* item;
    CritterView* cr;
    _mapper->HexMngr.GetSmthPixel(_mapper->Settings.MouseX, _mapper->Settings.MouseY, item, cr);

    Entity* mobj = nullptr;
    if (item)
        mobj = item;
    else if (cr)
        mobj = cr;
    FO_API_RETURN(mobj);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param datName ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(AddDataSource, FO_API_RET(void), FO_API_ARG(string, datName))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, datName))
{
    _mapper->FileMngr.AddDataSource(datName, false);

    for (int tab = 0; tab < TAB_COUNT; tab++)
        _mapper->RefreshTiles(tab);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param fontIndex ...
 * @param fontFname ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(LoadFont, FO_API_RET(bool), FO_API_ARG(int, fontIndex), FO_API_ARG(string, fontFname))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, fontIndex), FO_API_ARG_MARSHAL(string, fontFname))
{
    bool result;
    if (fontFname.length() > 0 && font_fname[0] == '*')
        result = _mapper->SprMngr.LoadFontFO(fontIndex, fontFname.c_str() + 1, false);
    else
        result = _mapper->SprMngr.LoadFontBMF(fontIndex, fontFname.c_str());
    if (result)
        _mapper->SprMngr.BuildFonts();
    FO_API_RETURN(result);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param font ...
 * @param color ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(SetDefaultFont, FO_API_RET(void), FO_API_ARG(int, font), FO_API_ARG(uint, color))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, font), FO_API_ARG_MARSHAL(uint, color))
{
    _mapper->SprMngr.SetDefaultFont(font, color);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
/*static int MouseButtonToSdlButton(int button)
{
    if (button == MOUSE_BUTTON_LEFT)
        return SDL_BUTTON_LEFT;
    if (button == MOUSE_BUTTON_RIGHT)
        return SDL_BUTTON_RIGHT;
    if (button == MOUSE_BUTTON_MIDDLE)
        return SDL_BUTTON_MIDDLE;
    if (button == MOUSE_BUTTON_EXT0)
        return SDL_BUTTON(4);
    if (button == MOUSE_BUTTON_EXT1)
        return SDL_BUTTON(5);
    if (button == MOUSE_BUTTON_EXT2)
        return SDL_BUTTON(6);
    if (button == MOUSE_BUTTON_EXT3)
        return SDL_BUTTON(7);
    if (button == MOUSE_BUTTON_EXT4)
        return SDL_BUTTON(8);
    return -1;
}*/
#endif
#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param x ...
 * @param y ...
 * @param button ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(MouseClick, FO_API_RET(void), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, button))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x), FO_API_ARG_MARSHAL(int, y), FO_API_ARG_MARSHAL(int, button))
{
    /*IntVec prev_events = _mapper->Settings.MainWindowMouseEvents;
    _mapper->Settings.MainWindowMouseEvents.clear();
    int prev_x = _mapper->Settings.MouseX;
    int prev_y = _mapper->Settings.MouseY;
    int last_prev_x = _mapper->Settings.LastMouseX;
    int last_prev_y = _mapper->Settings.LastMouseY;
    int prev_cursor = _mapper->CurMode;
    _mapper->Settings.MouseX = _mapper->Settings.LastMouseX = x;
    _mapper->Settings.MouseY = _mapper->Settings.LastMouseY = y;
    _mapper->Settings.MainWindowMouseEvents.push_back(SDL_MOUSEBUTTONDOWN);
    _mapper->Settings.MainWindowMouseEvents.push_back(MouseButtonToSdlButton(button));
    _mapper->Settings.MainWindowMouseEvents.push_back(SDL_MOUSEBUTTONUP);
    _mapper->Settings.MainWindowMouseEvents.push_back(MouseButtonToSdlButton(button));
    _mapper->ParseMouse();
    _mapper->Settings.MainWindowMouseEvents = prev_events;
    _mapper->Settings.MouseX = prev_x;
    _mapper->Settings.MouseY = prev_y;
    _mapper->Settings.LastMouseX = last_prev_x;
    _mapper->Settings.LastMouseY = last_prev_y;*/
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param key1 ...
 * @param key2 ...
 * @param key1Text ...
 * @param key2Text ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(KeyboardPress, FO_API_RET(void), FO_API_ARG(uchar, key1), FO_API_ARG(uchar, key2), FO_API_ARG(string, key1Text), FO_API_ARG(string, key2Text))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, key1), FO_API_ARG_MARSHAL(uchar, key2), FO_API_ARG_MARSHAL(string, key1Text), FO_API_ARG_MARSHAL(string, key2Text))
{
    if (!key1 && !key2)
        FO_API_RETURN_VOID();

    if (key1)
        _mapper->ProcessInputEvent({InputEvent::KeyDown({(KeyCode)key1, key1_text})});

    if (key2)
    {
        _mapper->ProcessInputEvent({InputEvent::KeyDown({(KeyCode)key2, key2_text})});
        _mapper->ProcessInputEvent({InputEvent::KeyUp({(KeyCode)key2})});
    }

    if (key1)
        _mapper->ProcessInputEvent({InputEvent::KeyUp({(KeyCode)key1})});
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param fallAnimName ...
 * @param dropAnimName ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(SetRainAnimation, FO_API_RET(void), FO_API_ARG(string, fallAnimName), FO_API_ARG(string, dropAnimName))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, fallAnimName), FO_API_ARG_MARSHAL(string, dropAnimName))
{
    _mapper->HexMngr.SetRainAnimation(!fallAnimName.empty() ? fallAnimName.c_str() : nullptr,
        !dropAnimName.empty() ? dropAnimName.c_str() : nullptr);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param targetZoom ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(ChangeZoom, FO_API_RET(void), FO_API_ARG(float, targetZoom))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(float, targetZoom))
{
    if (targetZoom == _mapper->Settings.SpritesZoom)
        FO_API_RETURN_VOID();

    if (targetZoom == 1.0f)
    {
        _mapper->HexMngr.ChangeZoom(0);
    }
    else if (targetZoom > _mapper->Settings.SpritesZoom)
    {
        while (targetZoom > _mapper->Settings.SpritesZoom)
        {
            float old_zoom = _mapper->Settings.SpritesZoom;
            _mapper->HexMngr.ChangeZoom(1);
            if (_mapper->Settings.SpritesZoom == old_zoom)
                break;
        }
    }
    else if (targetZoom < _mapper->Settings.SpritesZoom)
    {
        while (targetZoom < _mapper->Settings.SpritesZoom)
        {
            float old_zoom = _mapper->Settings.SpritesZoom;
            _mapper->HexMngr.ChangeZoom(-1);
            if (_mapper->Settings.SpritesZoom == old_zoom)
                break;
        }
    }
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param sprName ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(LoadSprite, FO_API_RET(uint), FO_API_ARG(string, sprName))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, sprName))
{
    FO_API_RETURN(_mapper->AnimLoad(sprName.c_str(), AtlasType::Static));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param nameHash ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(LoadSpriteHash, FO_API_RET(uint), FO_API_ARG(uint, nameHash))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, nameHash))
{
    FO_API_RETURN(_mapper->AnimLoad(nameHash, AtlasType::Static));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @param sprIndex ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetSpriteWidth, FO_API_RET(int), FO_API_ARG(uint, sprId), FO_API_ARG(int, sprIndex))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId), FO_API_ARG_MARSHAL(int, sprIndex))
{
    AnyFrames* anim = _mapper->AnimGetFrames(sprId);
    if (!anim || sprIndex >= (int)anim->CntFrm)
        FO_API_RETURN(0);
    SpriteInfo* si = _mapper->SprMngr.GetSpriteInfo(sprIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(sprIndex));
    if (!si)
        FO_API_RETURN(0);
    FO_API_RETURN(si->Width);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @param sprIndex ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetSpriteHeight, FO_API_RET(int), FO_API_ARG(uint, sprId), FO_API_ARG(int, sprIndex))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId), FO_API_ARG_MARSHAL(int, sprIndex))
{
    AnyFrames* anim = _mapper->AnimGetFrames(sprId);
    if (!anim || sprIndex >= (int)anim->CntFrm)
        FO_API_RETURN(0);
    SpriteInfo* si = _mapper->SprMngr.GetSpriteInfo(sprIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(sprIndex));
    if (!si)
        FO_API_RETURN(0);
    FO_API_RETURN(si->Height);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetSpriteCount, FO_API_RET(uint), FO_API_ARG(uint, sprId))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId))
{
    AnyFrames* anim = _mapper->AnimGetFrames(sprId);
    FO_API_RETURN(anim ? anim->CntFrm : 0);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetSpriteTicks, FO_API_RET(uint), FO_API_ARG(uint, sprId))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId))
{
    AnyFrames* anim = _mapper->AnimGetFrames(sprId);
    FO_API_RETURN(anim ? anim->Ticks : 0);
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @param frameIndex ...
 * @param x ...
 * @param y ...
 * @return ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetPixelColor, FO_API_RET(uint), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex), FO_API_ARG(int, x), FO_API_ARG(int, y))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId), FO_API_ARG_MARSHAL(int, frameIndex), FO_API_ARG_MARSHAL(int, x), FO_API_ARG_MARSHAL(int, y))
{
    if (!sprId)
        FO_API_RETURN(0);

    AnyFrames* anim = _mapper->AnimGetFrames(sprId);
    if (!anim || frameIndex >= (int)anim->CntFrm)
        FO_API_RETURN(0);

    uint spr_id_ = (frameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(frameIndex));
    FO_API_RETURN(_mapper->SprMngr.GetPixColor(spr_id_, x, y, false));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
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
#endif
FO_API_GLOBAL_MAPPER_FUNC(GetTextInfo, FO_API_RET(void), FO_API_ARG(string, text), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(int, font), FO_API_ARG(int, flags), FO_API_ARG_REF(int, tw), FO_API_ARG_REF(int, th), FO_API_ARG_REF(int, lines))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text), FO_API_ARG_MARSHAL(int, w), FO_API_ARG_MARSHAL(int, h), FO_API_ARG_MARSHAL(int, font), FO_API_ARG_MARSHAL(int, flags), FO_API_ARG_REF_MARSHAL(int, tw), FO_API_ARG_REF_MARSHAL(int, th), FO_API_ARG_REF_MARSHAL(int, lines))
{
    _mapper->SprMngr.GetTextInfo(w, h, !text.empty() ? text.c_str() : nullptr, font, flags, tw, th, lines);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
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
#endif
FO_API_GLOBAL_MAPPER_FUNC(DrawSprite, FO_API_RET(void), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(uint, color), FO_API_ARG(bool, offs))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId), FO_API_ARG_MARSHAL(int, frameIndex), FO_API_ARG_MARSHAL(int, x), FO_API_ARG_MARSHAL(int, y), FO_API_ARG_MARSHAL(uint, color), FO_API_ARG_MARSHAL(bool, offs))
{
    if (!SpritesCanDraw || !sprId)
        FO_API_RETURN_VOID();
    AnyFrames* anim = _mapper->AnimGetFrames(sprId);
    if (!anim || frameIndex >= (int)anim->CntFrm)
        FO_API_RETURN_VOID();
    uint spr_id_ = (frameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(frameIndex));
    if (offs)
    {
        SpriteInfo* si = _mapper->SprMngr.GetSpriteInfo(spr_id_);
        if (!si)
            FO_API_RETURN_VOID();
        x += -si->Width / 2 + si->OffsX;
        y += -si->Height + si->OffsY;
    }
    _mapper->SprMngr.DrawSprite(spr_id_, x, y, COLOR_SCRIPT_SPRITE(color));
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
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
#endif
FO_API_GLOBAL_MAPPER_FUNC(DrawSpriteSize, FO_API_RET(void), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(bool, zoom), FO_API_ARG(uint, color), FO_API_ARG(bool, offs))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId), FO_API_ARG_MARSHAL(int, frameIndex), FO_API_ARG_MARSHAL(int, x), FO_API_ARG_MARSHAL(int, y), FO_API_ARG_MARSHAL(int, w), FO_API_ARG_MARSHAL(int, h), FO_API_ARG_MARSHAL(bool, zoom), FO_API_ARG_MARSHAL(uint, color), FO_API_ARG_MARSHAL(bool, offs))
{
    if (!SpritesCanDraw || !sprId)
        FO_API_RETURN_VOID();
    AnyFrames* anim = _mapper->AnimGetFrames(sprId);
    if (!anim || frameIndex >= (int)anim->CntFrm)
        FO_API_RETURN_VOID();
    uint spr_id_ = (frameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(frameIndex));
    if (offs)
    {
        SpriteInfo* si = _mapper->SprMngr.GetSpriteInfo(spr_id_);
        if (!si)
            FO_API_RETURN_VOID();
        x += si->OffsX;
        y += si->OffsY;
    }
    _mapper->SprMngr.DrawSpriteSizeExt(spr_id_, x, y, w, h, zoom, true, true, COLOR_SCRIPT_SPRITE(color));
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
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
#endif
FO_API_GLOBAL_MAPPER_FUNC(DrawSpritePattern, FO_API_RET(void), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(int, sprWidth), FO_API_ARG(int, sprHeight), FO_API_ARG(uint, color))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId), FO_API_ARG_MARSHAL(int, frameIndex), FO_API_ARG_MARSHAL(int, x), FO_API_ARG_MARSHAL(int, y), FO_API_ARG_MARSHAL(int, w), FO_API_ARG_MARSHAL(int, h), FO_API_ARG_MARSHAL(int, sprWidth), FO_API_ARG_MARSHAL(int, sprHeight), FO_API_ARG_MARSHAL(uint, color))
{
    if (!SpritesCanDraw || !sprId)
        FO_API_RETURN_VOID();
    AnyFrames* anim = _mapper->AnimGetFrames(sprId);
    if (!anim || frameIndex >= (int)anim->CntFrm)
        FO_API_RETURN_VOID();
    _mapper->SprMngr.DrawSpritePattern(frameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(frameIndex), x, y, w, h,
        sprWidth, sprHeight, COLOR_SCRIPT_SPRITE(color));
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
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
#endif
FO_API_GLOBAL_MAPPER_FUNC(DrawText, FO_API_RET(void), FO_API_ARG(string, text), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(uint, color), FO_API_ARG(int, font), FO_API_ARG(int, flags))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text), FO_API_ARG_MARSHAL(int, x), FO_API_ARG_MARSHAL(int, y), FO_API_ARG_MARSHAL(int, w), FO_API_ARG_MARSHAL(int, h), FO_API_ARG_MARSHAL(uint, color), FO_API_ARG_MARSHAL(int, font), FO_API_ARG_MARSHAL(int, flags))
{
    if (!SpritesCanDraw)
        FO_API_RETURN_VOID();
    if (text.length() == 0)
        FO_API_RETURN_VOID();
    if (w < 0)
        w = -w, x -= w;
    if (h < 0)
        h = -h, y -= h;
    _mapper->SprMngr.DrawStr(Rect(x, y, x + w, y + h), text.c_str(), flags, COLOR_SCRIPT_TEXT(color), font);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param primitiveType ...
 * @param data ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(DrawPrimitive, FO_API_RET(void), FO_API_ARG(int, primitiveType), FO_API_ARG_ARR(int, data))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, primitiveType), FO_API_ARG_ARR_MARSHAL(int, data))
{
    if (!SpritesCanDraw || data->GetSize() == 0)
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

    static PointVec points;
    int size = data->GetSize() / 3;
    points.resize(size);

    for (int i = 0; i < size; i++)
    {
        PrepPoint& pp = points[i];
        pp.PointX = *(int*)data->At(i * 3);
        pp.PointY = *(int*)data->At(i * 3 + 1);
        pp.PointColor = *(int*)data->At(i * 3 + 2);
        pp.PointOffsX = nullptr;
        pp.PointOffsY = nullptr;
    }

    _mapper->SprMngr.DrawPoints(points, prim);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param mapSpr ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(DrawMapSprite, FO_API_RET(void), FO_API_ARG_OBJ(MapSprite, mapSpr))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(MapSprite, mapSpr))
{
    if (!mapSpr)
        throw ScriptException("Map sprite arg is null");

    if (!_mapper->HexMngr.IsMapLoaded())
        FO_API_RETURN_VOID();
    if (mapSpr->HexX >= _mapper->HexMngr.GetWidth() || mapSpr->HexY >= _mapper->HexMngr.GetHeight())
        FO_API_RETURN_VOID();
    if (!_mapper->HexMngr.IsHexToDraw(mapSpr->HexX, mapSpr->HexY))
        FO_API_RETURN_VOID();

    AnyFrames* anim = _mapper->AnimGetFrames(mapSpr->SprId);
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
        ProtoItem* proto_item = _mapper->ProtoMngr.GetProtoItem(mapSpr->ProtoId);
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

    Field& f = _mapper->HexMngr.GetField(mapSpr->HexX, mapSpr->HexY);
    Sprites& tree = _mapper->HexMngr.GetDrawTree();
    Sprite& spr = tree.InsertSprite(draw_order, mapSpr->HexX, mapSpr->HexY + draw_order_hy_offset, 0,
        (_mapper->Settings.MapHexWidth / 2) + mapSpr->OffsX, (_mapper->Settings.MapHexHeight / 2) + mapSpr->OffsY, &f.ScrX,
        &f.ScrY, mapSpr->FrameIndex < 0 ? anim->GetCurSprId() : anim->GetSprId(mapSpr->FrameIndex), nullptr,
        mapSpr->IsTweakOffs ? &mapSpr->TweakOffsX : nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsY : nullptr,
        mapSpr->IsTweakAlpha ? &mapSpr->TweakAlpha : nullptr, nullptr, nullptr);

    spr.MapSpr = mapSpr;
    mapSpr->AddRef();

    if (!no_light)
        spr.SetLight(corner, _mapper->HexMngr.GetLightHex(0, 0), _mapper->HexMngr.GetWidth(), _mapper->HexMngr.GetHeight());

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

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
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
#endif
FO_API_GLOBAL_MAPPER_FUNC(DrawCritter2d, FO_API_RET(void), FO_API_ARG(hash, modelName), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2), FO_API_ARG(uchar, dir), FO_API_ARG(int, l), FO_API_ARG(int, t), FO_API_ARG(int, r), FO_API_ARG(int, b), FO_API_ARG(bool, scratch), FO_API_ARG(bool, center), FO_API_ARG(uint, color))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, modelName), FO_API_ARG_MARSHAL(uint, anim1), FO_API_ARG_MARSHAL(uint, anim2), FO_API_ARG_MARSHAL(uchar, dir), FO_API_ARG_MARSHAL(int, l), FO_API_ARG_MARSHAL(int, t), FO_API_ARG_MARSHAL(int, r), FO_API_ARG_MARSHAL(int, b), FO_API_ARG_MARSHAL(bool, scratch), FO_API_ARG_MARSHAL(bool, center), FO_API_ARG_MARSHAL(uint, color))
{
    AnyFrames* anim = _mapper->ResMngr.GetCrit2dAnim(modelName, anim1, anim2, dir);
    if (anim)
        _mapper->SprMngr.DrawSpriteSize(anim->Ind[0], l, t, r - l, b - t, scratch, center, COLOR_SCRIPT_SPRITE(color));
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
static Animation3dVec DrawCritter3dAnim;
static UIntVec DrawCritter3dCrType;
static BoolVec DrawCritter3dFailToLoad;
static int DrawCritter3dLayers[LAYERS3D_COUNT];
#endif
#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
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
#endif
FO_API_GLOBAL_MAPPER_FUNC(DrawCritter3d, FO_API_RET(void), FO_API_ARG(uint, instance), FO_API_ARG(hash, modelName), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2), FO_API_ARG_ARR(int, layers), FO_API_ARG_ARR(float, position), FO_API_ARG(uint, color))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, instance), FO_API_ARG_MARSHAL(hash, modelName), FO_API_ARG_MARSHAL(uint, anim1), FO_API_ARG_MARSHAL(uint, anim2), FO_API_ARG_ARR_MARSHAL(int, layers), FO_API_ARG_ARR_MARSHAL(float, position), FO_API_ARG_MARSHAL(uint, color))
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
            _mapper->SprMngr.FreePure3dAnimation(anim3d);
        anim3d = _mapper->SprMngr.LoadPure3dAnimation(_str().parseHash(modelName).c_str(), false);
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

    uint count = (position ? position->GetSize() : 0);
    float x = (count > 0 ? *(float*)position->At(0) : 0.0f);
    float y = (count > 1 ? *(float*)position->At(1) : 0.0f);
    float rx = (count > 2 ? *(float*)position->At(2) : 0.0f);
    float ry = (count > 3 ? *(float*)position->At(3) : 0.0f);
    float rz = (count > 4 ? *(float*)position->At(4) : 0.0f);
    float sx = (count > 5 ? *(float*)position->At(5) : 1.0f);
    float sy = (count > 6 ? *(float*)position->At(6) : 1.0f);
    float sz = (count > 7 ? *(float*)position->At(7) : 1.0f);
    float speed = (count > 8 ? *(float*)position->At(8) : 1.0f);
    float period = (count > 9 ? *(float*)position->At(9) : 0.0f);
    float stl = (count > 10 ? *(float*)position->At(10) : 0.0f);
    float stt = (count > 11 ? *(float*)position->At(11) : 0.0f);
    float str = (count > 12 ? *(float*)position->At(12) : 0.0f);
    float stb = (count > 13 ? *(float*)position->At(13) : 0.0f);
    if (count > 13)
        _mapper->SprMngr.PushScissor((int)stl, (int)stt, (int)str, (int)stb);

    memzero(DrawCritter3dLayers, sizeof(DrawCritter3dLayers));
    for (uint i = 0, j = (layers ? layers->GetSize() : 0); i < j && i < LAYERS3D_COUNT; i++)
        DrawCritter3dLayers[i] = *(int*)layers->At(i);

    anim3d->SetDirAngle(0);
    anim3d->SetRotation(rx * PI_VALUE / 180.0f, ry * PI_VALUE / 180.0f, rz * PI_VALUE / 180.0f);
    anim3d->SetScale(sx, sy, sz);
    anim3d->SetSpeed(speed);
    anim3d->SetAnimation(
        anim1, anim2, DrawCritter3dLayers, ANIMATION_PERIOD((int)(period * 100.0f)) | ANIMATION_NO_SMOOTH);

    _mapper->SprMngr.Draw3d((int)x, (int)y, anim3d, COLOR_SCRIPT_SPRITE(color));

    if (count > 13)
        _mapper->SprMngr.PopScissor();
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 * @param x ...
 * @param y ...
 * @param w ...
 * @param h ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(PushDrawScissor, FO_API_RET(void), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x), FO_API_ARG_MARSHAL(int, y), FO_API_ARG_MARSHAL(int, w), FO_API_ARG_MARSHAL(int, h))
{
    _mapper->SprMngr.PushScissor(x, y, x + w, y + h);
}
FO_API_EPILOG()
#endif

#ifdef FO_API_GLOBAL_MAPPER_FUNC_DOC
/*******************************************************************************
 * ...
 *
 ******************************************************************************/
#endif
FO_API_GLOBAL_MAPPER_FUNC(PopDrawScissor, FO_API_RET(void))
#ifdef FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG()
{
    _mapper->SprMngr.PopScissor();
}
FO_API_EPILOG()
#endif
