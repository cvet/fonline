#include "Crypt.h"
#include "Entity.h"
#include "FileSystem.h"
#include "FileUtils.h"
#include "IniFile.h"
#include "Log.h"
#include "ProtoManager.h"
#include "Script.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
#include "Critter.h"
#include "Item.h"
#include "Map.h"
#endif
#if defined(FONLINE_EDITOR)
#include "CritterView.h"
#include "ItemView.h"
#include "MapView.h"
#endif

PROPERTIES_IMPL(ProtoMap);
CLASS_PROPERTY_IMPL(ProtoMap, FileDir);
CLASS_PROPERTY_IMPL(ProtoMap, Width);
CLASS_PROPERTY_IMPL(ProtoMap, Height);
CLASS_PROPERTY_IMPL(ProtoMap, WorkHexX);
CLASS_PROPERTY_IMPL(ProtoMap, WorkHexY);
CLASS_PROPERTY_IMPL(ProtoMap, CurDayTime);
CLASS_PROPERTY_IMPL(ProtoMap, ScriptId);
CLASS_PROPERTY_IMPL(ProtoMap, DayTime); // 4 int
CLASS_PROPERTY_IMPL(ProtoMap, DayColor); // 12 uchar
CLASS_PROPERTY_IMPL(ProtoMap, IsNoLogOut);

ProtoMap::ProtoMap(hash pid) : ProtoEntity(pid, EntityType::MapProto, ProtoMap::PropertiesRegistrator)
{
#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    MEMORY_PROCESS(MEMORY_PROTO_MAP, sizeof(ProtoMap));
#endif

#if defined(FONLINE_EDITOR)
#endif
}

ProtoMap::~ProtoMap()
{
#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    MEMORY_PROCESS(MEMORY_PROTO_MAP, -(int)sizeof(ProtoMap));
    MEMORY_PROCESS(MEMORY_PROTO_MAP, -(int)SceneryData.capacity());
    MEMORY_PROCESS(MEMORY_PROTO_MAP, -(int)Tiles.capacity() * sizeof(Tile));

    SAFEDELA(HexFlags);

    for (auto it = CrittersVec.begin(), end = CrittersVec.end(); it != end; ++it)
        SAFEREL(*it);
    for (auto it = AllItemsVec.begin(), end = AllItemsVec.end(); it != end; ++it)
        SAFEREL(*it);
    for (auto it = HexItemsVec.begin(), end = HexItemsVec.end(); it != end; ++it)
        SAFEREL(*it);
    for (auto it = ChildItemsVec.begin(), end = ChildItemsVec.end(); it != end; ++it)
        SAFEREL(*it);
    for (auto it = StaticItemsVec.begin(), end = StaticItemsVec.end(); it != end; ++it)
        SAFEREL(*it);
    for (auto it = TriggerItemsVec.begin(), end = TriggerItemsVec.end(); it != end; ++it)
        SAFEREL(*it);
#endif

#if defined(FONLINE_EDITOR)
    for (auto& entity : AllEntities)
        entity->Release();
#endif
}

bool ProtoMap::BaseLoad(ProtoManager& proto_mngr, CrLoadFunc cr_load, ItemLoadFunc item_load, TileLoadFunc tile_load)
{
    // Find file
    FileCollection maps("fomap");
    string path;
    File& map_file = maps.FindFile(GetName(), &path);
    if (!map_file.IsLoaded())
    {
        WriteLog("Map '{}' not found.\n", GetName());
        return false;
    }

    // Store path
    SetFileDir(_str(path).extractDir());

    // Load from file
    const char* buf = map_file.GetCStr();
    bool is_old_format = (strstr(buf, "[Header]") && strstr(buf, "[Tiles]") && strstr(buf, "[Objects]"));
    if (is_old_format)
    {
        WriteLog("Unable to load map '{}' from old map format.\n", GetName());
        return false;
    }

    // Header
    IniFile map_data;
    map_data.AppendStr(buf);
    if (!map_data.IsApp("ProtoMap"))
    {
        WriteLog("Invalid map format.\n");
        return false;
    }
    Props.LoadFromText(map_data.GetApp("ProtoMap"));

    // Critters
    int errors = 0;
    PStrMapVec npc_data;
    map_data.GetApps("Critter", npc_data);
    for (auto& pkv : npc_data)
    {
        auto& kv = *pkv;
        if (!kv.count("$Id") || !kv.count("$Proto"))
        {
            WriteLog("Proto critter invalid data.\n");
            errors++;
            continue;
        }

        uint id = _str(kv["$Id"]).toUInt();
        hash proto_id = _str(kv["$Proto"]).toHash();
        ProtoCritter* proto = proto_mngr.GetProtoCritter(proto_id);
        if (!proto)
        {
            WriteLog("Proto critter '{}' not found.\n", _str().parseHash(proto_id));
            errors++;
            continue;
        }

        if (!cr_load(id, proto, kv))
        {
            WriteLog("Unable to load critter '{}' properties.\n", _str().parseHash(proto_id));
            errors++;
        }
    }

    // Items
    PStrMapVec items_data;
    map_data.GetApps("Item", items_data);
    for (auto& pkv : items_data)
    {
        auto& kv = *pkv;
        if (!kv.count("$Id") || !kv.count("$Proto"))
        {
            WriteLog("Proto item invalid data.\n");
            errors++;
            continue;
        }

        uint id = _str(kv["$Id"]).toUInt();
        hash proto_id = _str(kv["$Proto"]).toHash();
        ProtoItem* proto = proto_mngr.GetProtoItem(proto_id);
        if (!proto)
        {
            WriteLog("Proto item '{}' not found.\n", _str().parseHash(proto_id));
            errors++;
            continue;
        }

        if (!item_load(id, proto, kv))
        {
            WriteLog("Unable to load item '{}' properties.\n", _str().parseHash(proto_id));
            errors++;
        }
    }

    // Tiles
    PStrMapVec tiles_data;
    map_data.GetApps("Tile", tiles_data);
    for (auto& pkv : tiles_data)
    {
        auto& kv = *pkv;
        if (!kv.count("PicMap") || !kv.count("HexX") || !kv.count("HexY"))
        {
            WriteLog("Tile invalid data.\n");
            errors++;
            continue;
        }

        hash name = _str(kv["PicMap"]).toHash();
        ushort hx = _str(kv["HexX"]).toUInt();
        ushort hy = _str(kv["HexY"]).toUInt();
        short ox = (kv.count("OffsetX") ? _str(kv["OffsetX"]).toUInt() : 0);
        short oy = (kv.count("OffsetY") ? _str(kv["OffsetY"]).toUInt() : 0);
        uchar layer = (kv.count("Layer") ? _str(kv["Layer"]).toUInt() : 0);
        bool is_roof = (kv.count("IsRoof") ? _str(kv["IsRoof"]).toBool() : false);
        if (hx < 0 || hx >= GetWidth() || hy < 0 || hy > GetHeight())
        {
            WriteLog("Tile '{}' have wrong hex position {} {}.\n", _str().parseHash(name), hx, hy);
            errors++;
            continue;
        }
        if (layer < 0 || layer > 255)
        {
            WriteLog("Tile '{}' have wrong layer value {}.\n", _str().parseHash(name), layer);
            errors++;
            continue;
        }

        tile_load({name, hx, hy, ox, oy, layer, is_roof});
    }

    return errors == 0;
}

#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
bool ProtoMap::ServerLoad(ProtoManager& proto_mngr)
{
    if (!BaseLoad(
            proto_mngr,
            [this](uint id, ProtoCritter* proto, const StrMap& kv) -> bool {
                Npc* npc = new Npc(id, proto);
                if (!npc->Props.LoadFromText(kv))
                {
                    delete npc;
                    return false;
                }

                CrittersVec.push_back(npc);
                return true;
            },
            [this](uint id, ProtoItem* proto, const StrMap& kv) -> bool {
                Item* item = new Item(id, proto);
                if (!item->Props.LoadFromText(kv))
                {
                    delete item;
                    return false;
                }

                AllItemsVec.push_back(item);
                return true;
            },
            [this](Tile&& tile) { Tiles.push_back(std::move(tile)); }))
    {
        return false;
    }

    return OnAfterLoad();
}

bool ProtoMap::OnAfterLoad()
{
    // Bind scripts
    if (!BindScripts())
        return false;

    // Parse objects
    ushort maxhx = GetWidth();
    ushort maxhy = GetHeight();
    HexFlags = new uchar[maxhx * maxhy];
    memzero(HexFlags, maxhx * maxhy);

    uint scenery_count = 0;
    UCharVec scenery_data;
    for (auto& item : AllItemsVec)
    {
        if (!item->IsStatic())
        {
            item->AddRef();
            if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
                HexItemsVec.push_back(item);
            else
                ChildItemsVec.push_back(item);
            continue;
        }

        RUNTIME_ASSERT(item->GetAccessory() == ITEM_ACCESSORY_HEX);

        ushort hx = item->GetHexX();
        ushort hy = item->GetHexY();
        if (hx >= maxhx || hy >= maxhy)
        {
            WriteLog(
                "Invalid item '{}' position on map '{}', hex x {}, hex y {}.\n", item->GetName(), GetName(), hx, hy);
            continue;
        }

        if (!item->GetIsHiddenInStatic())
        {
            item->AddRef();
            StaticItemsVec.push_back(item);
        }

        if (!item->GetIsNoBlock())
            SETFLAG(HexFlags[hy * maxhx + hx], FH_BLOCK);

        if (!item->GetIsShootThru())
        {
            SETFLAG(HexFlags[hy * maxhx + hx], FH_BLOCK);
            SETFLAG(HexFlags[hy * maxhx + hx], FH_NOTRAKE);
        }

        if (item->GetIsScrollBlock())
        {
            // Block around
            for (int k = 0; k < 6; k++)
            {
                ushort hx_ = hx, hy_ = hy;
                MoveHexByDir(hx_, hy_, k, maxhx, maxhy);
                SETFLAG(HexFlags[hy_ * maxhx + hx_], FH_BLOCK);
            }
        }

        if (item->GetIsTrigger() || item->GetIsTrap())
        {
            item->AddRef();
            TriggerItemsVec.push_back(item);

            SETFLAG(HexFlags[hy * maxhx + hx], FH_STATIC_TRIGGER);
        }

        if (item->IsNonEmptyBlockLines())
        {
            ushort hx_ = hx, hy_ = hy;
            bool raked = item->GetIsShootThru();
            FOREACH_PROTO_ITEM_LINES(
                item->GetBlockLines(), hx_, hy_, maxhx, maxhy, SETFLAG(HexFlags[hy_ * maxhx + hx_], FH_BLOCK);
                if (!raked) SETFLAG(HexFlags[hy_ * maxhx + hx_], FH_NOTRAKE););
        }

        // Data for client
        if (!item->GetIsHidden())
        {
            scenery_count++;
            WriteData(scenery_data, item->Id);
            WriteData(scenery_data, item->GetProtoId());
            PUCharVec* all_data;
            UIntVec* all_data_sizes;
            item->Props.StoreData(false, &all_data, &all_data_sizes);
            WriteData(scenery_data, (uint)all_data->size());
            for (size_t i = 0; i < all_data->size(); i++)
            {
                WriteData(scenery_data, all_data_sizes->at(i));
                WriteDataArr(scenery_data, all_data->at(i), all_data_sizes->at(i));
            }
        }
    }

    SceneryData.clear();
    WriteData(SceneryData, scenery_count);
    if (!scenery_data.empty())
        WriteDataArr(SceneryData, &scenery_data[0], (uint)scenery_data.size());

    // Generate hashes
    HashTiles = maxhx * maxhy;
    if (Tiles.size())
        HashTiles = Crypt.MurmurHash2((uchar*)&Tiles[0], (uint)Tiles.size() * sizeof(ProtoMap::Tile));
    HashScen = maxhx * maxhy;
    if (SceneryData.size())
        HashScen = Crypt.MurmurHash2((uchar*)&SceneryData[0], (uint)SceneryData.size());

    // Shrink the vector capacities to fit their contents and reduce memory use
    UCharVec(SceneryData).swap(SceneryData);
    CritterVec(CrittersVec).swap(CrittersVec);
    ItemVec(AllItemsVec).swap(AllItemsVec);
    ItemVec(HexItemsVec).swap(HexItemsVec);
    ItemVec(ChildItemsVec).swap(ChildItemsVec);
    ItemVec(StaticItemsVec).swap(StaticItemsVec);
    ItemVec(TriggerItemsVec).swap(TriggerItemsVec);
    ProtoMap::TileVec(Tiles).swap(Tiles);

    MEMORY_PROCESS(MEMORY_PROTO_MAP, (int)SceneryData.capacity());
    MEMORY_PROCESS(MEMORY_PROTO_MAP, (int)GetWidth() * GetHeight());
    MEMORY_PROCESS(MEMORY_PROTO_MAP, (int)Tiles.capacity() * sizeof(ProtoMap::Tile));

    return true;
}

bool ProtoMap::BindScripts()
{
    int errors = 0;

    if (GetScriptId())
    {
        hash func_num = Script::BindScriptFuncNumByFuncName(_str().parseHash(GetScriptId()), "void %s(Map, bool)");
        if (!func_num)
        {
            WriteLog("Map '{}', can't bind map function '{}'.\n", GetName(), _str().parseHash(GetScriptId()));
            errors++;
        }
    }

    for (auto& cr : CrittersVec)
    {
        if (cr->GetScriptId())
        {
            string func_name = _str().parseHash(cr->GetScriptId());
            hash func_num = Script::BindScriptFuncNumByFuncName(func_name, "void %s(Critter, bool)");
            if (!func_num)
            {
                WriteLog("Map '{}', can't bind critter function '{}'.\n", GetName(), func_name);
                errors++;
            }
        }
    }

    for (auto& item : AllItemsVec)
    {
        if (!item->IsStatic() && item->GetScriptId())
        {
            string func_name = _str().parseHash(item->GetScriptId());
            hash func_num = Script::BindScriptFuncNumByFuncName(func_name, "void %s(Item, bool)");
            if (!func_num)
            {
                WriteLog("Map '{}', can't bind item function '{}'.\n", GetName(), func_name);
                errors++;
            }
        }
        else if (item->IsStatic() && item->GetScriptId())
        {
            string func_name = _str().parseHash(item->GetScriptId());
            uint bind_id = 0;
            if (item->GetIsTrigger() || item->GetIsTrap())
                bind_id = Script::BindByFuncName(func_name, "void %s(Critter, const Item, bool, uint8)", false);
            else
                bind_id = Script::BindByFuncName(func_name, "bool %s(Critter, const Item, Item, int)", false);

            if (!bind_id)
            {
                WriteLog("Map '{}', can't bind static item function '{}'.\n", GetName(), func_name);
                errors++;
            }

            item->SceneryScriptBindId = bind_id;
        }
    }

    return errors == 0;
}

void ProtoMap::GetStaticItemTriggers(ushort hx, ushort hy, ItemVec& triggers)
{
    for (auto& item : TriggerItemsVec)
        if (item->GetHexX() == hx && item->GetHexY() == hy)
            triggers.push_back(item);
}

Item* ProtoMap::GetStaticItem(ushort hx, ushort hy, hash pid)
{
    for (auto& item : StaticItemsVec)
        if ((!pid || item->GetProtoId() == pid) && item->GetHexX() == hx && item->GetHexY() == hy)
            return item;
    return nullptr;
}

void ProtoMap::GetStaticItemsHex(ushort hx, ushort hy, ItemVec& items)
{
    for (auto& item : StaticItemsVec)
        if (item->GetHexX() == hx && item->GetHexY() == hy)
            items.push_back(item);
}

void ProtoMap::GetStaticItemsHexEx(ushort hx, ushort hy, uint radius, hash pid, ItemVec& items)
{
    for (auto& item : StaticItemsVec)
        if ((!pid || item->GetProtoId() == pid) && DistGame(item->GetHexX(), item->GetHexY(), hx, hy) <= radius)
            items.push_back(item);
}

void ProtoMap::GetStaticItemsByPid(hash pid, ItemVec& items)
{
    for (auto& item : StaticItemsVec)
        if (!pid || item->GetProtoId() == pid)
            items.push_back(item);
}
#endif

#if defined(FONLINE_EDITOR)
bool ProtoMap::IsMapFile(const string& fname)
{
    string ext = _str(fname).getFileExtension();
    if (ext.empty())
        return false;

    if (ext == "fomap")
    {
        IniFile txt;
        if (!txt.AppendFile(fname))
            return false;

        return txt.IsApp("ProtoMap") || (txt.IsApp("Header") && txt.IsApp("Tiles") && txt.IsApp("Objects"));
    }

    return false;
}

void ProtoMap::GenNew()
{
    SetWidth(MAXHEX_DEF);
    SetHeight(MAXHEX_DEF);

    // Morning	 5.00 -  9.59	 300 - 599
    // Day		10.00 - 18.59	 600 - 1139
    // Evening	19.00 - 22.59	1140 - 1379
    // Nigh		23.00 -  4.59	1380
    IntVec vec = {300, 600, 1140, 1380};
    UCharVec vec2 = {18, 128, 103, 51, 18, 128, 95, 40, 53, 128, 86, 29};
    CScriptArray* arr = Script::CreateArray("int[]");
    Script::AppendVectorToArray(vec, arr);
    SetDayTime(arr);
    arr->Release();
    CScriptArray* arr2 = Script::CreateArray("uint8[]");
    Script::AppendVectorToArray(vec2, arr2);
    SetDayColor(arr2);
    arr2->Release();
}

bool ProtoMap::EditorLoad(ProtoManager& proto_mngr, SpriteManager& spr_mngr, ResourceManager& res_mngr)
{
    if (!BaseLoad(
            proto_mngr,
            [this, &spr_mngr, &res_mngr](uint id, ProtoCritter* proto, const StrMap& kv) -> bool {
                CritterView* npc = new CritterView(id, proto, spr_mngr, res_mngr);
                if (!npc->Props.LoadFromText(kv))
                {
                    delete npc;
                    return false;
                }

                AllEntities.push_back(npc);
                return true;
            },
            [this](uint id, ProtoItem* proto, const StrMap& kv) -> bool {
                ItemView* item = new ItemView(id, proto);
                if (!item->Props.LoadFromText(kv))
                {
                    delete item;
                    return false;
                }

                AllEntities.push_back(item);
                return true;
            },
            [this](Tile&& tile) { Tiles.push_back(std::move(tile)); }))
    {
        return false;
    }

    LastEntityId = 0;
    for (auto& entity : AllEntities)
    {
        if (!LastEntityId || LastEntityId < entity->Id)
            LastEntityId = entity->Id;
    }

    return true;
}

bool ProtoMap::EditorSave(const string& custom_name)
{
    IniFile file;
    Props.SaveToText(file.SetApp("ProtoMap"), nullptr);

    for (auto& entity : AllEntities)
    {
        if (entity->Type == EntityType::CritterView)
        {
            StrMap& kv = file.SetApp("Critter");
            kv["$Id"] = _str("{}", entity->Id);
            kv["$Proto"] = entity->Proto->GetName();
            entity->Props.SaveToText(kv, &entity->Proto->Props);
        }
    }

    for (auto& entity : AllEntities)
    {
        if (entity->Type == EntityType::ItemView)
        {
            StrMap& kv = file.SetApp("Item");
            kv["$Id"] = _str("{}", entity->Id);
            kv["$Proto"] = entity->Proto->GetName();
            entity->Props.SaveToText(kv, &entity->Proto->Props);
        }
    }

    for (auto& tile : Tiles)
    {
        StrMap& kv = file.SetApp("Tile");
        kv["PicMap"] = _str().parseHash(tile.Name);
        kv["HexX"] = _str("{}", tile.HexX);
        kv["HexY"] = _str("{}", tile.HexY);
        if (tile.OffsX)
            kv["OffsetX"] = _str("{}", tile.OffsX);
        if (tile.OffsY)
            kv["OffsetY"] = _str("{}", tile.OffsY);
        if (tile.Layer)
            kv["Layer"] = _str("{}", tile.Layer);
        if (tile.IsRoof)
            kv["IsRoof"] = _str("{}", tile.IsRoof);
    }

    string save_fname = GetFileDir() + (!custom_name.empty() ? custom_name : GetName()) + ".fomap";
    if (!file.SaveFile(save_fname))
    {
        WriteLog("Unable write file '{}' in modules.\n", save_fname);
        return false;
    }

    return true;
}
#endif
