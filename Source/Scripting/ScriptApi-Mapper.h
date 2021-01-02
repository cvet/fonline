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

#if FO_API_MAPPER_IMPL
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(AddItem, FO_API_RET_OBJ(ItemView), FO_API_ARG(hash, pid), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _mapper->HexMngr.GetWidth() || hy >= _mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    const auto* proto = _mapper->ProtoMngr.GetProtoItem(pid);
    if (!proto) {
        throw ScriptException("Invalid item prototype");
    }

    FO_API_RETURN(_mapper->AddItem(pid, hx, hy, nullptr));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param hx ...
 * @param hy ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(AddCritter, FO_API_RET_OBJ(CritterView), FO_API_ARG(hash, pid), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    if (hx >= _mapper->HexMngr.GetWidth() || hy >= _mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    const auto* proto = _mapper->ProtoMngr.GetProtoCritter(pid);
    if (!proto) {
        throw ScriptException("Invalid critter prototype");
    }

    FO_API_RETURN(_mapper->AddCritter(pid, hx, hy));
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
FO_API_GLOBAL_MAPPER_FUNC(GetItemByHex, FO_API_RET_OBJ(ItemView), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    FO_API_RETURN(_mapper->HexMngr.GetItem(hx, hy, 0));
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
FO_API_GLOBAL_MAPPER_FUNC(GetItemsByHex, FO_API_RET_OBJ_ARR(ItemHexView), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    vector<ItemHexView*> items;
    _mapper->HexMngr.GetItems(hx, hy, items);
    FO_API_RETURN(items);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(GetCritterByHex, FO_API_RET_OBJ(CritterView), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(int, findType))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(int, findType))
{
    vector<CritterView*> critters_;
    _mapper->HexMngr.GetCritters(hx, hy, critters_, findType);
    FO_API_RETURN(!critters_.empty() ? critters_[0] : nullptr);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param findType ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(GetCrittersByHex, FO_API_RET_OBJ_ARR(CritterView), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(int, findType))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(int, findType))
{
    vector<CritterView*> critters;
    _mapper->HexMngr.GetCritters(hx, hy, critters, findType);
    FO_API_RETURN(critters);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param entity ...
 * @param hx ...
 * @param hy ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(MoveEntity, FO_API_RET(void), FO_API_ARG_OBJ(Entity, entity), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Entity, entity) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy))
{
    auto hx_ = hx;
    auto hy_ = hy;

    if (hx_ >= _mapper->HexMngr.GetWidth()) {
        hx_ = _mapper->HexMngr.GetWidth() - 1;
    }
    if (hy_ >= _mapper->HexMngr.GetHeight()) {
        hy_ = _mapper->HexMngr.GetHeight() - 1;
    }

    _mapper->MoveEntity(entity, hx_, hy_);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param entity ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(DeleteEntity, FO_API_RET(void), FO_API_ARG_OBJ(Entity, entity))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Entity, entity))
{
    _mapper->DeleteEntity(entity);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param entities ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(DeleteEntities, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Entity, entities))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Entity, entities))
{
    for (auto* entity : entities) {
        if (entity != nullptr && !entity->IsDestroyed) {
            _mapper->DeleteEntity(entity);
        }
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param entity ...
 * @param set ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(SelectEntity, FO_API_RET(void), FO_API_ARG_OBJ(Entity, entity), FO_API_ARG(bool, set))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(Entity, entity) FO_API_ARG_MARSHAL(bool, set))
{
    if (entity == nullptr) {
        throw ScriptException("Entity arg is null");
    }

    if (set) {
        _mapper->SelectAdd(entity);
    }
    else {
        _mapper->SelectErase(entity);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param entities ...
 * @param set ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(SelectEntities, FO_API_RET(void), FO_API_ARG_OBJ_ARR(Entity, entities), FO_API_ARG(bool, set))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_ARR_MARSHAL(Entity, entities) FO_API_ARG_MARSHAL(bool, set))
{
    for (auto* entity : entities) {
        if (entity != nullptr) {
            if (set) {
                _mapper->SelectAdd(entity);
            }
            else {
                _mapper->SelectErase(entity);
            }
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
FO_API_GLOBAL_MAPPER_FUNC(GetSelectedEntity, FO_API_RET_OBJ(Entity))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG()
{
    auto* entity = (!_mapper->SelectedEntities.empty() ? _mapper->SelectedEntities[0] : nullptr);
    FO_API_RETURN(entity);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(GetSelectedEntities, FO_API_RET_OBJ_ARR(Entity))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG()
{
    vector<Entity*> entities;
    entities.reserve(_mapper->SelectedEntities.size());

    for (auto* entity : _mapper->SelectedEntities) {
        entities.push_back(entity);
    }

    FO_API_RETURN(entities);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param roof ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(GetTilesCount, FO_API_RET(uint), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, roof))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(bool, roof))
{
    if (!_mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= _mapper->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= _mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    const auto& tiles = _mapper->HexMngr.GetTiles(hx, hy, roof);
    FO_API_RETURN(static_cast<uint>(tiles.size()));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param roof ...
 * @param layer ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(DeleteTile, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, roof), FO_API_ARG(int, layer))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(bool, roof) FO_API_ARG_MARSHAL(int, layer))
{
    if (!_mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= _mapper->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= _mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    auto deleted = false;
    auto& tiles = _mapper->HexMngr.GetTiles(hx, hy, roof);
    auto& f = _mapper->HexMngr.GetField(hx, hy);
    if (layer < 0) {
        deleted = !tiles.empty();
        tiles.clear();
        while (f.GetTilesCount(roof)) {
            f.EraseTile(0, roof);
        }
    }
    else {
        for (size_t i = 0, j = tiles.size(); i < j; i++) {
            if (tiles[i].Layer == layer) {
                tiles.erase(tiles.begin() + i);
                f.EraseTile(static_cast<uint>(i), roof);
                deleted = true;
                break;
            }
        }
    }

    if (deleted) {
        if (roof) {
            _mapper->HexMngr.RebuildRoof();
        }
        else {
            _mapper->HexMngr.RebuildTiles();
        }
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
FO_API_GLOBAL_MAPPER_FUNC(GetTileHash, FO_API_RET(hash), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, roof), FO_API_ARG(int, layer))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(bool, roof) FO_API_ARG_MARSHAL(int, layer))
{
    if (!_mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= _mapper->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= _mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    const auto& tiles = _mapper->HexMngr.GetTiles(hx, hy, roof);
    for (const auto i : xrange(tiles)) {
        if (tiles[i].Layer == layer) {
            FO_API_RETURN(tiles[i].Name);
        }
    }

    FO_API_RETURN(0);
}
FO_API_EPILOG(0)
#endif

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
FO_API_GLOBAL_MAPPER_FUNC(AddTileHash, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(int, ox), FO_API_ARG(int, oy), FO_API_ARG(int, layer), FO_API_ARG(bool, roof), FO_API_ARG(hash, picHash))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(int, ox) FO_API_ARG_MARSHAL(int, oy) FO_API_ARG_MARSHAL(int, layer) FO_API_ARG_MARSHAL(bool, roof) FO_API_ARG_MARSHAL(hash, picHash))
{
    if (!_mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= _mapper->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= _mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    if (picHash == 0u) {
        FO_API_RETURN_VOID();
    }

    auto layer_ = layer;
    layer_ = std::clamp(layer_, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END);

    _mapper->HexMngr.SetTile(picHash, hx, hy, static_cast<short>(ox), static_cast<short>(oy), static_cast<uchar>(layer_), roof, false);
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
FO_API_GLOBAL_MAPPER_FUNC(GetTileName, FO_API_RET(string), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(bool, roof), FO_API_ARG(int, layer))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(bool, roof) FO_API_ARG_MARSHAL(int, layer))
{
    if (!_mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= _mapper->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= _mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    const auto& tiles = _mapper->HexMngr.GetTiles(hx, hy, roof);
    for (const auto i : xrange(tiles)) {
        if (tiles[i].Layer == layer) {
            FO_API_RETURN(_str().parseHash(tiles[i].Name).str());
        }
    }

    FO_API_RETURN(string(""));
}
FO_API_EPILOG(0)
#endif

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
FO_API_GLOBAL_MAPPER_FUNC(AddTileName, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(int, ox), FO_API_ARG(int, oy), FO_API_ARG(int, layer), FO_API_ARG(bool, roof), FO_API_ARG(string, picName))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(int, ox) FO_API_ARG_MARSHAL(int, oy) FO_API_ARG_MARSHAL(int, layer) FO_API_ARG_MARSHAL(bool, roof) FO_API_ARG_MARSHAL(string, picName))
{
    if (!_mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= _mapper->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= _mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    if (picName.empty()) {
        FO_API_RETURN_VOID();
    }

    auto layer_ = layer;
    layer_ = std::clamp(layer_, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END);

    const auto pic_hash = _str(picName).toHash();
    _mapper->HexMngr.SetTile(pic_hash, hx, hy, static_cast<short>(ox), static_cast<short>(oy), static_cast<uchar>(layer_), roof, false);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param fileName ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(LoadMap, FO_API_RET_OBJ(MapView), FO_API_ARG(string, fileName))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, fileName))
{
    auto* pmap = new ProtoMap(_str(fileName).toHash());
    // Todo: need attention!
    // if (!pmap->EditorLoad(_mapper->ServerFileMngr, _mapper->ProtoMngr, _mapper->SprMngr, _mapper->ResMngr))
    //     FO_API_RETURN(nullptr);

    auto* map = new MapView(0, pmap);
    _mapper->LoadedMaps.push_back(map);
    _mapper->RunMapLoadScript(map);

    FO_API_RETURN(map);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param map ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(UnloadMap, FO_API_RET(void), FO_API_ARG_OBJ(MapView, map))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(MapView, map))
{
    if (map == nullptr) {
        throw ScriptException("Proto map arg nullptr");
    }

    const auto it = std::find(_mapper->LoadedMaps.begin(), _mapper->LoadedMaps.end(), map);
    if (it != _mapper->LoadedMaps.end()) {
        _mapper->LoadedMaps.erase(it);
    }

    if (map == _mapper->ActiveMap) {
        _mapper->HexMngr.UnloadMap();
        _mapper->SelectedEntities.clear();
        _mapper->ActiveMap = nullptr;
    }

    map->Proto->Release();
    map->Release();
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param map ...
 * @param customName ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(SaveMap, FO_API_RET(bool), FO_API_ARG_OBJ(MapView, map), FO_API_ARG(string, customName))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(MapView, map) FO_API_ARG_MARSHAL(string, customName))
{
    if (map == nullptr) {
        throw ScriptException("Proto map arg nullptr");
    }

    // Todo: need attention!
    //((ProtoMap*)map->Proto)->EditorSave(_mapper->ServerFileMngr, customName);
    _mapper->RunMapSaveScript(map);
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param map ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(ShowMap, FO_API_RET(bool), FO_API_ARG_OBJ(MapView, map))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(MapView, map))
{
    if (map == nullptr) {
        throw ScriptException("Proto map arg nullptr");
    }

    if (_mapper->ActiveMap == map) {
        FO_API_RETURN(true);
    }

    _mapper->SelectClear();
    if (!_mapper->HexMngr.SetProtoMap(*(ProtoMap*)map->Proto)) {
        FO_API_RETURN(false);
    }

    _mapper->HexMngr.FindSetCenter(map->GetWorkHexX(), map->GetWorkHexY());
    _mapper->ActiveMap = map;

    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param index ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(GetLoadedMaps, FO_API_RET_OBJ_ARR(MapView), FO_API_ARG_REF(int, index))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(int, index))
{
    index = -1;
    for (auto i = 0, j = static_cast<int>(_mapper->LoadedMaps.size()); i < j; i++) {
        const auto* map = _mapper->LoadedMaps[i];
        if (map == _mapper->ActiveMap) {
            index = i;
        }
    }
    FO_API_RETURN(_mapper->LoadedMaps);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param dir ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(GetMapFileNames, FO_API_RET_ARR(string), FO_API_ARG(string, dir))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, dir))
{
    vector<string> names;

    auto map_files = _mapper->ServerFileMngr.FilterFiles("fomap", dir, false);
    while (map_files.MoveNext()) {
        auto file_header = map_files.GetCurFileHeader();
        names.push_back(file_header.GetName());
    }

    FO_API_RETURN(names);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param width ...
 * @param height ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(ResizeMap, FO_API_RET(void), FO_API_ARG(ushort, width), FO_API_ARG(ushort, height))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, width) FO_API_ARG_MARSHAL(ushort, height))
{
    if (!_mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }

    RUNTIME_ASSERT(_mapper->ActiveMap);
    auto* pmap = (ProtoMap*)_mapper->ActiveMap->Proto;

    // Unload current
    _mapper->HexMngr.GetProtoMap(*pmap);
    _mapper->SelectClear();
    _mapper->HexMngr.UnloadMap();

    // Check size
    auto maxhx = std::clamp(width, MAXHEX_MIN, MAXHEX_MAX);
    auto maxhy = std::clamp(height, MAXHEX_MIN, MAXHEX_MAX);
    const auto old_maxhx = pmap->GetWidth();
    const auto old_maxhy = pmap->GetHeight();

    maxhx = std::clamp(maxhx, MAXHEX_MIN, MAXHEX_MAX);
    maxhy = std::clamp(maxhy, MAXHEX_MIN, MAXHEX_MAX);

    if (pmap->GetWorkHexX() >= maxhx) {
        pmap->SetWorkHexX(maxhx - 1);
    }
    if (pmap->GetWorkHexY() >= maxhy) {
        pmap->SetWorkHexY(maxhy - 1);
    }

    pmap->SetWidth(maxhx);
    pmap->SetHeight(maxhy);

    // Delete truncated entities
    if (maxhx < old_maxhx || maxhy < old_maxhy) {
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
    if (maxhx < old_maxhx || maxhy < old_maxhy) {
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

/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(TabGetTileDirs, FO_API_RET_ARR(string), FO_API_ARG(int, tab))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab))
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    auto& ttab = _mapper->TabsTiles[tab];
    // FO_API_RETURN(ttab.TileSubDirs);
    FO_API_RETURN(vector<string>());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param subTab ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(TabGetItemPids, FO_API_RET_ARR(hash), FO_API_ARG(int, tab), FO_API_ARG(string, subTab))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab) FO_API_ARG_MARSHAL(string, subTab))
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    if (!subTab.empty() && !_mapper->Tabs[tab].count(subTab)) {
        FO_API_RETURN(vector<hash>());
    }

    auto& stab = _mapper->Tabs[tab][!subTab.empty() ? subTab : FOMapper::DEFAULT_SUB_TAB];
    // FO_API_RETURN(stab.ItemProtos);
    FO_API_RETURN(vector<hash>());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param subTab ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(TabGetCritterPids, FO_API_RET_ARR(hash), FO_API_ARG(int, tab), FO_API_ARG(string, subTab))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab) FO_API_ARG_MARSHAL(string, subTab))
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    if (!subTab.empty() && !_mapper->Tabs[tab].count(subTab)) {
        FO_API_RETURN(vector<hash>());
    }

    auto& stab = _mapper->Tabs[tab][!subTab.empty() ? subTab : FOMapper::DEFAULT_SUB_TAB];
    // FO_API_RETURN(stab.NpcProtos);
    FO_API_RETURN(vector<hash>());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param dirNames ...
 * @param includeSubdirs ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(TabSetTileDirs, FO_API_RET(void), FO_API_ARG(int, tab), FO_API_ARG_ARR(string, dirNames), FO_API_ARG_ARR(bool, includeSubdirs))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab) FO_API_ARG_ARR_MARSHAL(string, dirNames) FO_API_ARG_ARR_MARSHAL(bool, includeSubdirs))
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    /*if (dirNames && includeSubdirs && dirNames->GetSize() != includeSubdirs->GetSize())
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

    _mapper->RefreshTiles(tab);*/
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param subTab ...
 * @param itemPids ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(TabSetItemPids, FO_API_RET(void), FO_API_ARG(int, tab), FO_API_ARG(string, subTab), FO_API_ARG_ARR(hash, itemPids))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab) FO_API_ARG_MARSHAL(string, subTab) FO_API_ARG_ARR_MARSHAL(hash, itemPids))
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    /*if (subTab.empty() || subTab == DEFAULT_SUB_TAB)
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
    _mapper->RefreshCurProtos();*/
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param subTab ...
 * @param critterPids ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(TabSetCritterPids, FO_API_RET(void), FO_API_ARG(int, tab), FO_API_ARG(string, subTab), FO_API_ARG_ARR(hash, critterPids))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab) FO_API_ARG_MARSHAL(string, subTab) FO_API_ARG_ARR_MARSHAL(hash, critterPids))
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    /*if (subTab.empty() || subTab == DEFAULT_SUB_TAB)
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
    _mapper->RefreshCurProtos();*/
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param tab ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(TabDelete, FO_API_RET(void), FO_API_ARG(int, tab))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab))
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    /*_mapper->Tabs[tab].clear();
    SubTab& stab_default = _mapper->Tabs[tab][DEFAULT_SUB_TAB];
    _mapper->TabsActive[tab] = &stab_default;*/
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param subTab ...
 * @param show ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(TabSelect, FO_API_RET(void), FO_API_ARG(int, tab), FO_API_ARG(string, subTab), FO_API_ARG(bool, show))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab) FO_API_ARG_MARSHAL(string, subTab) FO_API_ARG_MARSHAL(bool, show))
{
    if (tab < 0 || tab >= FOMapper::INT_MODE_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    if (show) {
        _mapper->IntSetMode(tab);
    }

    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        FO_API_RETURN_VOID();
    }

    auto it = _mapper->Tabs[tab].find(!subTab.empty() ? subTab : FOMapper::DEFAULT_SUB_TAB);
    if (it != _mapper->Tabs[tab].end()) {
        _mapper->TabsActive[tab] = &it->second;
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param tab ...
 * @param tabName ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(TabSetName, FO_API_RET(void), FO_API_ARG(int, tab), FO_API_ARG(string, tabName))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, tab) FO_API_ARG_MARSHAL(string, tabName))
{
    if (tab < 0 || tab >= FOMapper::INT_MODE_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    _mapper->TabsName[tab] = tabName;
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param hx ...
 * @param hy ...
 * @param speed ...
 * @param canStop ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(MoveScreenToHex, FO_API_RET(void), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, speed), FO_API_ARG(bool, canStop))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, speed) FO_API_ARG_MARSHAL(bool, canStop))
{
    if (hx >= _mapper->HexMngr.GetWidth() || hy >= _mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex args");
    }
    if (!_mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }

    if (speed == 0u) {
        _mapper->HexMngr.FindSetCenter(hx, hy);
    }
    else {
        _mapper->HexMngr.ScrollToHex(hx, hy, static_cast<float>(speed) / 1000.0f, canStop);
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
FO_API_GLOBAL_MAPPER_FUNC(MoveScreenOffset, FO_API_RET(void), FO_API_ARG(int, ox), FO_API_ARG(int, oy), FO_API_ARG(uint, speed), FO_API_ARG(bool, canStop))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, ox) FO_API_ARG_MARSHAL(int, oy) FO_API_ARG_MARSHAL(uint, speed) FO_API_ARG_MARSHAL(bool, canStop))
{
    if (!_mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }

    _mapper->HexMngr.ScrollOffset(ox, oy, static_cast<float>(speed) / 1000.0f, canStop);
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
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(MoveHexByDir, FO_API_RET(bool), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy), FO_API_ARG(uchar, dir), FO_API_ARG(uint, steps))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_REF_MARSHAL(ushort, hx) FO_API_ARG_REF_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uchar, dir) FO_API_ARG_MARSHAL(uint, steps))
{
    if (!_mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (dir >= _mapper->Settings.MapDirCount) {
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
            result |= _mapper->GeomHelper.MoveHexByDir(hx_, hy_, dir, _mapper->HexMngr.GetWidth(), _mapper->HexMngr.GetHeight());
        }
    }
    else {
        result |= _mapper->GeomHelper.MoveHexByDir(hx_, hy_, dir, _mapper->HexMngr.GetWidth(), _mapper->HexMngr.GetHeight());
    }

    hx = static_cast<ushort>(hx_);
    hy = static_cast<ushort>(hy_);
    FO_API_RETURN(result);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param key ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(GetIfaceIniStr, FO_API_RET(string), FO_API_ARG(string, key))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, key))
{
    FO_API_RETURN(_mapper->IfaceIni.GetStr("", key, ""));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param msg ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(Message, FO_API_RET(void), FO_API_ARG(string, msg))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, msg))
{
    _mapper->AddMess(msg.c_str());
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param textMsg ...
 * @param strNum ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(MessageMsg, FO_API_RET(void), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    _mapper->AddMess(_mapper->CurLang.Msg[textMsg].GetStr(strNum).c_str());
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
FO_API_GLOBAL_MAPPER_FUNC(MapMessage, FO_API_RET(void), FO_API_ARG(string, text), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG(uint, ms), FO_API_ARG(uint, color), FO_API_ARG(bool, fade), FO_API_ARG(int, ox), FO_API_ARG(int, oy))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(uint, ms) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(bool, fade) FO_API_ARG_MARSHAL(int, ox) FO_API_ARG_MARSHAL(int, oy))
{
    FOMapper::MapText map_text;
    map_text.HexX = hx;
    map_text.HexY = hy;
    map_text.Color = (color != 0u ? color : COLOR_TEXT);
    map_text.Fade = fade;
    map_text.StartTick = _mapper->GameTime.FrameTick();
    map_text.Tick = ms;
    map_text.Text = text;
    map_text.Pos = _mapper->HexMngr.GetRectForText(hx, hy);
    map_text.EndPos = IRect(map_text.Pos, ox, oy);

    const auto it = std::find_if(_mapper->GameMapTexts.begin(), _mapper->GameMapTexts.end(), [&map_text](const FOMapper::MapText& t) { return t.HexX == map_text.HexX && t.HexY == map_text.HexY; });
    if (it != _mapper->GameMapTexts.end()) {
        _mapper->GameMapTexts.erase(it);
    }

    _mapper->GameMapTexts.push_back(map_text);
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
FO_API_GLOBAL_MAPPER_FUNC(GetMsgStr, FO_API_RET(string), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    FO_API_RETURN(_mapper->CurLang.Msg[textMsg].GetStr(strNum));
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
FO_API_GLOBAL_MAPPER_FUNC(GetMsgStrSkip, FO_API_RET(string), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum), FO_API_ARG(uint, skipCount))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum) FO_API_ARG_MARSHAL(uint, skipCount))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    FO_API_RETURN(_mapper->CurLang.Msg[textMsg].GetStr(strNum, skipCount));
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
FO_API_GLOBAL_MAPPER_FUNC(GetMsgStrNumUpper, FO_API_RET(uint), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    FO_API_RETURN(_mapper->CurLang.Msg[textMsg].GetStrNumUpper(strNum));
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
FO_API_GLOBAL_MAPPER_FUNC(GetMsgStrNumLower, FO_API_RET(uint), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    FO_API_RETURN(_mapper->CurLang.Msg[textMsg].GetStrNumLower(strNum));
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
FO_API_GLOBAL_MAPPER_FUNC(GetMsgStrCount, FO_API_RET(uint), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    FO_API_RETURN(_mapper->CurLang.Msg[textMsg].Count(strNum));
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
FO_API_GLOBAL_MAPPER_FUNC(IsMsgStr, FO_API_RET(bool), FO_API_ARG(int, textMsg), FO_API_ARG(uint, strNum))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, textMsg) FO_API_ARG_MARSHAL(uint, strNum))
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    FO_API_RETURN(_mapper->CurLang.Msg[textMsg].Count(strNum) > 0);
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
FO_API_GLOBAL_MAPPER_FUNC(ReplaceTextStr, FO_API_RET(string), FO_API_ARG(string, text), FO_API_ARG(string, replace), FO_API_ARG(string, str))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(string, replace) FO_API_ARG_MARSHAL(string, str))
{
    const auto pos = text.find(replace, 0);
    if (pos == std::string::npos) {
        FO_API_RETURN(text);
    }

    FO_API_RETURN(string(text).replace(pos, replace.length(), text));
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
FO_API_GLOBAL_MAPPER_FUNC(ReplaceTextInt, FO_API_RET(string), FO_API_ARG(string, text), FO_API_ARG(string, replace), FO_API_ARG(int, i))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
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
FO_API_GLOBAL_MAPPER_FUNC(GetHexInPath, FO_API_RET(void), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG_REF(ushort, toHx), FO_API_ARG_REF(ushort, toHy), FO_API_ARG(float, angle), FO_API_ARG(uint, dist))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_REF_MARSHAL(ushort, toHx) FO_API_ARG_REF_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, angle) FO_API_ARG_MARSHAL(uint, dist))
{
    pair<ushort, ushort> pre_block;
    pair<ushort, ushort> block;
    _mapper->HexMngr.TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, nullptr, false, nullptr, 0, &block, &pre_block, nullptr, true);
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
FO_API_GLOBAL_MAPPER_FUNC(GetPathLengthHex, FO_API_RET(uint), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(uint, cut))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(uint, cut))
{
    if (fromHx >= _mapper->HexMngr.GetWidth() || fromHy >= _mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= _mapper->HexMngr.GetWidth() || toHy >= _mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (cut > 0 && !_mapper->HexMngr.CutPath(nullptr, fromHx, fromHy, to_hx, to_hy, cut)) {
        FO_API_RETURN(0);
    }

    vector<uchar> steps;
    if (!_mapper->HexMngr.FindPath(nullptr, fromHx, fromHy, to_hx, to_hy, steps, -1)) {
        FO_API_RETURN(0);
    }

    FO_API_RETURN(static_cast<uint>(steps.size()));
}
FO_API_EPILOG(0)
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
FO_API_GLOBAL_MAPPER_FUNC(GetHexPos, FO_API_RET(bool), FO_API_ARG(ushort, hx), FO_API_ARG(ushort, hy), FO_API_ARG_REF(int, x), FO_API_ARG_REF(int, y))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx) FO_API_ARG_MARSHAL(ushort, hy) FO_API_ARG_REF_MARSHAL(int, x) FO_API_ARG_REF_MARSHAL(int, y))
{
    x = y = 0;

    if (_mapper->HexMngr.IsMapLoaded() && hx < _mapper->HexMngr.GetWidth() && hy < _mapper->HexMngr.GetHeight()) {
        _mapper->HexMngr.GetHexCurrentPosition(hx, hy, x, y);

        x += _mapper->Settings.ScrOx + (_mapper->Settings.MapHexWidth / 2);
        y += _mapper->Settings.ScrOy + (_mapper->Settings.MapHexHeight / 2);

        x = static_cast<int>(static_cast<float>(x) / _mapper->Settings.SpritesZoom);
        y = static_cast<int>(static_cast<float>(y) / _mapper->Settings.SpritesZoom);

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
 * @param ignoreInterface ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(GetMonitorHex, FO_API_RET(bool), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG_REF(ushort, hx), FO_API_ARG_REF(ushort, hy), FO_API_ARG(bool, ignoreInterface))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_REF_MARSHAL(ushort, hx) FO_API_ARG_REF_MARSHAL(ushort, hy) FO_API_ARG_MARSHAL(bool, ignoreInterface))
{
    const auto old_x = _mapper->Settings.MouseX;
    const auto old_y = _mapper->Settings.MouseY;

    _mapper->Settings.MouseX = x;
    _mapper->Settings.MouseY = y;

    ushort hx_;
    ushort hy_;
    const auto result = _mapper->GetCurHex(hx_, hy_, ignoreInterface);

    _mapper->Settings.MouseX = old_x;
    _mapper->Settings.MouseY = old_y;

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
 * @param ignoreInterface ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(GetMonitorObject, FO_API_RET_OBJ(Entity), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(bool, ignoreInterface))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(bool, ignoreInterface))
{
    if (!_mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }

    if (!ignoreInterface && _mapper->IsCurInInterface()) {
        FO_API_RETURN(static_cast<Entity*>(nullptr));
    }

    ItemHexView* item = nullptr;
    CritterView* cr = nullptr;
    _mapper->HexMngr.GetSmthPixel(_mapper->Settings.MouseX, _mapper->Settings.MouseY, item, cr);

    Entity* mobj = nullptr;
    if (item != nullptr) {
        mobj = item;
    }
    else if (cr != nullptr) {
        mobj = cr;
    }

    FO_API_RETURN(mobj);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param datName ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(AddDataSource, FO_API_RET(void), FO_API_ARG(string, datName))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, datName))
{
    _mapper->FileMngr.AddDataSource(datName, false);

    for (auto tab = 0; tab < FOMapper::TAB_COUNT; tab++) {
        _mapper->RefreshTiles(tab);
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param fontIndex ...
 * @param fontFname ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(LoadFont, FO_API_RET(void), FO_API_ARG(int, fontIndex), FO_API_ARG(string, fontFname))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, fontIndex) FO_API_ARG_MARSHAL(string, fontFname))
{
    bool result;
    if (fontFname.length() > 0 && fontFname[0] == '*') {
        result = _mapper->SprMngr.LoadFontFO(fontIndex, fontFname.substr(1), false, false);
    }
    else {
        result = _mapper->SprMngr.LoadFontBmf(fontIndex, fontFname);
    }

    if (result) {
        _mapper->SprMngr.BuildFonts();
    }
    else {
        throw ScriptException("Unable to load font", fontIndex, fontFname);
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
FO_API_GLOBAL_MAPPER_FUNC(SetDefaultFont, FO_API_RET(void), FO_API_ARG(int, font), FO_API_ARG(uint, color))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, font) FO_API_ARG_MARSHAL(uint, color))
{
    _mapper->SprMngr.SetDefaultFont(font, color);
}
FO_API_EPILOG()
#endif

#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
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
/*******************************************************************************
 * ...
 *
 * @param x ...
 * @param y ...
 * @param button ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(MouseClick, FO_API_RET(void), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, button))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, button))
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

/*******************************************************************************
 * ...
 *
 * @param key1 ...
 * @param key2 ...
 * @param key1Text ...
 * @param key2Text ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(KeyboardPress, FO_API_RET(void), FO_API_ARG(uchar, key1), FO_API_ARG(uchar, key2), FO_API_ARG(string, key1Text), FO_API_ARG(string, key2Text))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uchar, key1) FO_API_ARG_MARSHAL(uchar, key2) FO_API_ARG_MARSHAL(string, key1Text) FO_API_ARG_MARSHAL(string, key2Text))
{
    if (key1 == 0u && key2 == 0u) {
        FO_API_RETURN_VOID();
    }

    if (key1 != 0u) {
        _mapper->ProcessInputEvent(InputEvent {InputEvent::KeyDownEvent {static_cast<KeyCode>(key1), key1Text}});
    }

    if (key2 != 0u) {
        _mapper->ProcessInputEvent(InputEvent {InputEvent::KeyDownEvent {static_cast<KeyCode>(key2), key2Text}});
        _mapper->ProcessInputEvent(InputEvent {InputEvent::KeyUpEvent {static_cast<KeyCode>(key2)}});
    }

    if (key1 != 0u) {
        _mapper->ProcessInputEvent(InputEvent {InputEvent::KeyUpEvent {static_cast<KeyCode>(key1)}});
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
FO_API_GLOBAL_MAPPER_FUNC(SetRainAnimation, FO_API_RET(void), FO_API_ARG(string, fallAnimName), FO_API_ARG(string, dropAnimName))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, fallAnimName) FO_API_ARG_MARSHAL(string, dropAnimName))
{
    _mapper->HexMngr.SetRainAnimation(!fallAnimName.empty() ? fallAnimName.c_str() : nullptr, !dropAnimName.empty() ? dropAnimName.c_str() : nullptr);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param targetZoom ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(ChangeZoom, FO_API_RET(void), FO_API_ARG(float, targetZoom))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(float, targetZoom))
{
    if (Math::FloatCompare(targetZoom, _mapper->Settings.SpritesZoom)) {
        FO_API_RETURN_VOID();
    }

    if (targetZoom == 1.0f) {
        _mapper->HexMngr.ChangeZoom(0);
    }
    else if (targetZoom > _mapper->Settings.SpritesZoom) {
        while (targetZoom > _mapper->Settings.SpritesZoom) {
            const auto old_zoom = _mapper->Settings.SpritesZoom;

            _mapper->HexMngr.ChangeZoom(1);

            if (Math::FloatCompare(_mapper->Settings.SpritesZoom, old_zoom)) {
                break;
            }
        }
    }
    else if (targetZoom < _mapper->Settings.SpritesZoom) {
        while (targetZoom < _mapper->Settings.SpritesZoom) {
            const auto old_zoom = _mapper->Settings.SpritesZoom;

            _mapper->HexMngr.ChangeZoom(-1);

            if (Math::FloatCompare(_mapper->Settings.SpritesZoom, old_zoom)) {
                break;
            }
        }
    }
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param sprName ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(LoadSprite, FO_API_RET(uint), FO_API_ARG(string, sprName))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, sprName))
{
    FO_API_RETURN(_mapper->AnimLoad(sprName.c_str(), AtlasType::Static));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param nameHash ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(LoadSpriteHash, FO_API_RET(uint), FO_API_ARG(uint, nameHash))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, nameHash))
{
    FO_API_RETURN(_mapper->AnimLoad(nameHash, AtlasType::Static));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param sprId ...
 * @param sprIndex ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(GetSpriteWidth, FO_API_RET(int), FO_API_ARG(uint, sprId), FO_API_ARG(int, sprIndex))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, sprIndex))
{
    auto* anim = _mapper->AnimGetFrames(sprId);
    if (!anim || sprIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN(0);
    }

    const auto* si = _mapper->SprMngr.GetSpriteInfo(sprIndex < 0 ? anim->GetCurSprId(_mapper->GameTime.GameTick()) : anim->GetSprId(sprIndex));
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
 * @param sprIndex ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(GetSpriteHeight, FO_API_RET(int), FO_API_ARG(uint, sprId), FO_API_ARG(int, sprIndex))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, sprIndex))
{
    auto* anim = _mapper->AnimGetFrames(sprId);
    if (!anim || sprIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN(0);
    }

    const auto* si = _mapper->SprMngr.GetSpriteInfo(sprIndex < 0 ? anim->GetCurSprId(_mapper->GameTime.GameTick()) : anim->GetSprId(sprIndex));
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
FO_API_GLOBAL_MAPPER_FUNC(GetSpriteCount, FO_API_RET(uint), FO_API_ARG(uint, sprId))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId))
{
    auto* anim = _mapper->AnimGetFrames(sprId);
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
FO_API_GLOBAL_MAPPER_FUNC(GetSpriteTicks, FO_API_RET(uint), FO_API_ARG(uint, sprId))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId))
{
    auto* anim = _mapper->AnimGetFrames(sprId);
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
FO_API_GLOBAL_MAPPER_FUNC(GetPixelColor, FO_API_RET(uint), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex), FO_API_ARG(int, x), FO_API_ARG(int, y))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y))
{
    if (sprId == 0u) {
        FO_API_RETURN(0);
    }

    auto* anim = _mapper->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN(0);
    }

    const auto spr_id = (frameIndex < 0 ? anim->GetCurSprId(_mapper->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    FO_API_RETURN(_mapper->SprMngr.GetPixColor(spr_id, x, y, false));
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
FO_API_GLOBAL_MAPPER_FUNC(GetTextInfo, FO_API_RET(void), FO_API_ARG(string, text), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(int, font), FO_API_ARG(int, flags), FO_API_ARG_REF(int, tw), FO_API_ARG_REF(int, th), FO_API_ARG_REF(int, lines))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h) FO_API_ARG_MARSHAL(int, font) FO_API_ARG_MARSHAL(int, flags) FO_API_ARG_REF_MARSHAL(int, tw) FO_API_ARG_REF_MARSHAL(int, th) FO_API_ARG_REF_MARSHAL(int, lines))
{
    if (!_mapper->SprMngr.GetTextInfo(w, h, text, font, flags, tw, th, lines)) {
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
FO_API_GLOBAL_MAPPER_FUNC(DrawSprite, FO_API_RET(void), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(uint, color), FO_API_ARG(bool, offs))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(bool, offs))
{
    if (!_mapper->SpritesCanDraw || !sprId) {
        FO_API_RETURN_VOID();
    }

    auto* anim = _mapper->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN_VOID();
    }

    auto x_ = x;
    auto y_ = y;

    const auto spr_id = (frameIndex < 0 ? anim->GetCurSprId(_mapper->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (offs) {
        const auto* si = _mapper->SprMngr.GetSpriteInfo(spr_id);
        if (!si) {
            FO_API_RETURN_VOID();
        }

        x_ += -si->Width / 2 + si->OffsX;
        y_ += -si->Height + si->OffsY;
    }

    _mapper->SprMngr.DrawSprite(spr_id, x_, y_, COLOR_SCRIPT_SPRITE(color));
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
FO_API_GLOBAL_MAPPER_FUNC(DrawSpriteSize, FO_API_RET(void), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(bool, zoom), FO_API_ARG(uint, color), FO_API_ARG(bool, offs))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h) FO_API_ARG_MARSHAL(bool, zoom) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(bool, offs))
{
    if (!_mapper->SpritesCanDraw || !sprId) {
        FO_API_RETURN_VOID();
    }

    auto* anim = _mapper->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN_VOID();
    }

    auto x_ = x;
    auto y_ = y;

    const auto spr_id = (frameIndex < 0 ? anim->GetCurSprId(_mapper->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (offs) {
        const auto* si = _mapper->SprMngr.GetSpriteInfo(spr_id);
        if (!si) {
            FO_API_RETURN_VOID();
        }

        x_ += si->OffsX;
        y_ += si->OffsY;
    }

    _mapper->SprMngr.DrawSpriteSizeExt(spr_id, x_, y_, w, h, zoom, true, true, COLOR_SCRIPT_SPRITE(color));
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
FO_API_GLOBAL_MAPPER_FUNC(DrawSpritePattern, FO_API_RET(void), FO_API_ARG(uint, sprId), FO_API_ARG(int, frameIndex), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(int, sprWidth), FO_API_ARG(int, sprHeight), FO_API_ARG(uint, color))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, sprId) FO_API_ARG_MARSHAL(int, frameIndex) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h) FO_API_ARG_MARSHAL(int, sprWidth) FO_API_ARG_MARSHAL(int, sprHeight) FO_API_ARG_MARSHAL(uint, color))
{
    if (!_mapper->SpritesCanDraw || !sprId) {
        FO_API_RETURN_VOID();
    }

    auto* anim = _mapper->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        FO_API_RETURN_VOID();
    }

    _mapper->SprMngr.DrawSpritePattern(frameIndex < 0 ? anim->GetCurSprId(_mapper->GameTime.GameTick()) : anim->GetSprId(frameIndex), x, y, w, h, sprWidth, sprHeight, COLOR_SCRIPT_SPRITE(color));
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
FO_API_GLOBAL_MAPPER_FUNC(DrawText, FO_API_RET(void), FO_API_ARG(string, text), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h), FO_API_ARG(uint, color), FO_API_ARG(int, font), FO_API_ARG(int, flags))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h) FO_API_ARG_MARSHAL(uint, color) FO_API_ARG_MARSHAL(int, font) FO_API_ARG_MARSHAL(int, flags))
{
    if (!_mapper->SpritesCanDraw) {
        FO_API_RETURN_VOID();
    }
    if (text.length() == 0) {
        FO_API_RETURN_VOID();
    }

    auto x_ = x;
    auto y_ = y;
    auto w_ = w;
    auto h_ = h;

    if (w_ < 0) {
        w_ = -w_;
        x_ -= w_;
    }
    if (h_ < 0) {
        h_ = -h_;
        y_ -= h_;
    }

    _mapper->SprMngr.DrawStr(IRect(x_, y_, x_ + w_, y_ + h_), text, flags, COLOR_SCRIPT_TEXT(color), font);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param primitiveType ...
 * @param data ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(DrawPrimitive, FO_API_RET(void), FO_API_ARG(int, primitiveType), FO_API_ARG_ARR(int, data))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, primitiveType) FO_API_ARG_ARR_MARSHAL(int, data))
{
    if (!_mapper->SpritesCanDraw || data.empty()) {
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

    const auto size = data.size() / 3;
    PrimitivePoints points;
    points.resize(size);

    for (const auto i : xrange(size)) {
        auto& pp = points[i];
        pp.PointX = data[i * 3];
        pp.PointY = data[i * 3 + 1];
        pp.PointColor = data[i * 3 + 2];
        pp.PointOffsX = nullptr;
        pp.PointOffsY = nullptr;
    }

    _mapper->SprMngr.DrawPoints(points, prim, nullptr, nullptr, nullptr);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param mapSpr ...
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(DrawMapSprite, FO_API_RET(void), FO_API_ARG_OBJ(MapSprite, mapSpr))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_OBJ_MARSHAL(MapSprite, mapSpr))
{
    if (mapSpr == nullptr) {
        throw ScriptException("Map sprite arg is null");
    }

    if (!_mapper->HexMngr.IsMapLoaded()) {
        FO_API_RETURN_VOID();
    }
    if (mapSpr->HexX >= _mapper->HexMngr.GetWidth() || mapSpr->HexY >= _mapper->HexMngr.GetHeight()) {
        FO_API_RETURN_VOID();
    }
    if (!_mapper->HexMngr.IsHexToDraw(mapSpr->HexX, mapSpr->HexY)) {
        FO_API_RETURN_VOID();
    }

    auto* anim = _mapper->AnimGetFrames(mapSpr->SprId);
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
        const auto* proto_item = _mapper->ProtoMngr.GetProtoItem(mapSpr->ProtoId);
        if (!proto_item) {
            FO_API_RETURN_VOID();
        }

        color = (proto_item->GetIsColorize() ? proto_item->GetLightColor() : 0);
        is_flat = proto_item->GetIsFlat();
        const auto is_item = !proto_item->IsAnyScenery();
        no_light = (is_flat && !is_item);
        draw_order = (is_flat ? (is_item ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY) : (is_item ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY));
        draw_order_hy_offset = static_cast<int>(proto_item->GetDrawOrderOffsetHexY());
        corner = proto_item->GetCorner();
        disable_egg = proto_item->GetDisableEgg();
        contour_color = (proto_item->GetIsBadItem() ? COLOR_RGB(255, 0, 0) : 0);
    }

    auto& f = _mapper->HexMngr.GetField(mapSpr->HexX, mapSpr->HexY);
    auto& tree = _mapper->HexMngr.GetDrawTree();
    auto& spr = tree.InsertSprite(draw_order, mapSpr->HexX, mapSpr->HexY + draw_order_hy_offset, (_mapper->Settings.MapHexWidth / 2) + mapSpr->OffsX, (_mapper->Settings.MapHexHeight / 2) + mapSpr->OffsY, &f.ScrX, &f.ScrY, mapSpr->FrameIndex < 0 ? anim->GetCurSprId(_mapper->GameTime.GameTick()) : anim->GetSprId(mapSpr->FrameIndex), nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsX : nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsY : nullptr, mapSpr->IsTweakAlpha ? &mapSpr->TweakAlpha : nullptr, nullptr, nullptr);

    spr.MapSpr = mapSpr;
    mapSpr->AddRef();

    if (!no_light) {
        spr.SetLight(corner, _mapper->HexMngr.GetLightHex(0, 0), _mapper->HexMngr.GetWidth(), _mapper->HexMngr.GetHeight());
    }

    if (!is_flat && !disable_egg) {
        int egg_type = 0;
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
FO_API_GLOBAL_MAPPER_FUNC(DrawCritter2d, FO_API_RET(void), FO_API_ARG(hash, modelName), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2), FO_API_ARG(uchar, dir), FO_API_ARG(int, l), FO_API_ARG(int, t), FO_API_ARG(int, r), FO_API_ARG(int, b), FO_API_ARG(bool, scratch), FO_API_ARG(bool, center), FO_API_ARG(uint, color))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, modelName) FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2) FO_API_ARG_MARSHAL(uchar, dir) FO_API_ARG_MARSHAL(int, l) FO_API_ARG_MARSHAL(int, t) FO_API_ARG_MARSHAL(int, r) FO_API_ARG_MARSHAL(int, b) FO_API_ARG_MARSHAL(bool, scratch) FO_API_ARG_MARSHAL(bool, center) FO_API_ARG_MARSHAL(uint, color))
{
    auto* anim = _mapper->ResMngr.GetCritterAnim(modelName, anim1, anim2, dir);
    if (anim) {
        _mapper->SprMngr.DrawSpriteSize(anim->Ind[0], l, t, r - l, b - t, scratch, center, COLOR_SCRIPT_SPRITE(color));
    }
}
FO_API_EPILOG()
#endif

#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
static vector<ModelInstance*> MapperDrawCritterModel;
static vector<uint> MapperDrawCritterModelCrType;
static vector<bool> MapperDrawCritterModelFailedToLoad;
static int MapperDrawCritterModelLayers[LAYERS3D_COUNT];
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
FO_API_GLOBAL_MAPPER_FUNC(DrawCritter3d, FO_API_RET(void), FO_API_ARG(uint, instance), FO_API_ARG(hash, modelName), FO_API_ARG(uint, anim1), FO_API_ARG(uint, anim2), FO_API_ARG_ARR(int, layers), FO_API_ARG_ARR(float, position), FO_API_ARG(uint, color))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, instance) FO_API_ARG_MARSHAL(hash, modelName) FO_API_ARG_MARSHAL(uint, anim1) FO_API_ARG_MARSHAL(uint, anim2) FO_API_ARG_ARR_MARSHAL(int, layers) FO_API_ARG_ARR_MARSHAL(float, position) FO_API_ARG_MARSHAL(uint, color))
{
    // x y
    // rx ry rz
    // sx sy sz
    // speed
    // scissor l t r b

    if (instance >= MapperDrawCritterModel.size()) {
        MapperDrawCritterModel.resize(instance + 1);
        MapperDrawCritterModelCrType.resize(instance + 1);
        MapperDrawCritterModelFailedToLoad.resize(instance + 1);
    }

    if (MapperDrawCritterModelFailedToLoad[instance] && MapperDrawCritterModelCrType[instance] == modelName) {
        FO_API_RETURN_VOID();
    }

    auto*& model = MapperDrawCritterModel[instance];
    if (model == nullptr || MapperDrawCritterModelCrType[instance] != modelName) {
        if (model != nullptr) {
            _mapper->SprMngr.FreeModel(model);
        }

        model = _mapper->SprMngr.LoadModel(_str().parseHash(modelName), false);

        MapperDrawCritterModelCrType[instance] = modelName;
        MapperDrawCritterModelFailedToLoad[instance] = false;

        if (model == nullptr) {
            MapperDrawCritterModelFailedToLoad[instance] = true;
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
        _mapper->SprMngr.PushScissor(static_cast<int>(stl), static_cast<int>(stt), static_cast<int>(str), static_cast<int>(stb));
    }

    std::memset(MapperDrawCritterModelLayers, 0, sizeof(MapperDrawCritterModelLayers));
    for (uint i = 0, j = static_cast<uint>(layers.size()); i < j && i < LAYERS3D_COUNT; i++) {
        MapperDrawCritterModelLayers[i] = layers[i];
    }

    model->SetDirAngle(0);
    model->SetRotation(rx * PI_FLOAT / 180.0f, ry * PI_FLOAT / 180.0f, rz * PI_FLOAT / 180.0f);
    model->SetScale(sx, sy, sz);
    model->SetSpeed(speed);
    model->SetAnimation(anim1, anim2, MapperDrawCritterModelLayers, ANIMATION_PERIOD(static_cast<int>(period * 100.0f)) | ANIMATION_NO_SMOOTH);

    _mapper->SprMngr.Draw3d(static_cast<int>(x), static_cast<int>(y), model, COLOR_SCRIPT_SPRITE(color));

    if (count > 13) {
        _mapper->SprMngr.PopScissor();
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
FO_API_GLOBAL_MAPPER_FUNC(PushDrawScissor, FO_API_RET(void), FO_API_ARG(int, x), FO_API_ARG(int, y), FO_API_ARG(int, w), FO_API_ARG(int, h))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, x) FO_API_ARG_MARSHAL(int, y) FO_API_ARG_MARSHAL(int, w) FO_API_ARG_MARSHAL(int, h))
{
    _mapper->SprMngr.PushScissor(x, y, x + w, y + h);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 ******************************************************************************/
FO_API_GLOBAL_MAPPER_FUNC(PopDrawScissor, FO_API_RET(void))
#if FO_API_GLOBAL_MAPPER_FUNC_IMPL
FO_API_PROLOG()
{
    _mapper->SprMngr.PopScissor();
}
FO_API_EPILOG()
#endif
