#include "MapManager.h"
#include "Critter.h"
#include "CritterManager.h"
#include "EntityManager.h"
#include "Item.h"
#include "ItemManager.h"
#include "LineTracer.h"
#include "Location.h"
#include "Log.h"
#include "Map.h"
#include "ProtoManager.h"
#include "Script.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"

MapManager::MapManager(
    ProtoManager& proto_mngr, EntityManager& entity_mngr, CritterManager& cr_mngr, ItemManager& item_mngr) :
    protoMngr(proto_mngr), entityMngr(entity_mngr), crMngr(cr_mngr), itemMngr(item_mngr)
{
    for (int i = 1; i < FPATH_DATA_SIZE; i++)
        pathesPool[i].reserve(100);
}

void MapManager::GenerateMapContent(Map* map)
{
    UIntMap id_map;

    // Generate npc
    for (Critter* base_cr : map->GetProtoMap()->CrittersVec)
    {
        // Create npc
        Npc* npc = crMngr.CreateNpc(base_cr->GetProtoId(), &base_cr->Props, map, base_cr->GetHexX(), base_cr->GetHexY(),
            base_cr->GetDir(), true);
        if (!npc)
        {
            WriteLog("Create npc '{}' on map '{}' fail, continue generate.\n", base_cr->GetName(), map->GetName());
            continue;
        }
        id_map.insert(std::make_pair(base_cr->GetId(), npc->GetId()));

        // Check condition
        if (npc->GetCond() != COND_LIFE)
        {
            if (npc->GetCond() == COND_DEAD)
            {
                npc->SetCurrentHp(GameOpt.DeadHitPoints - 1);

                uint multihex = npc->GetMultihex();
                map->UnsetFlagCritter(npc->GetHexX(), npc->GetHexY(), multihex, false);
                map->SetFlagCritter(npc->GetHexX(), npc->GetHexY(), multihex, true);
            }
        }
    }

    // Generate hex items
    for (Item* base_item : map->GetProtoMap()->HexItemsVec)
    {
        // Create item
        Item* item = itemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->Props);
        if (!item)
        {
            WriteLog("Create item '{}' on map '{}' fail, continue generate.\n", base_item->GetName(), map->GetName());
            continue;
        }
        id_map.insert(std::make_pair(base_item->GetId(), item->GetId()));

        // Other values
        if (item->GetIsCanOpen() && item->GetOpened())
            item->SetIsLightThru(true);

        if (!map->AddItem(item, item->GetHexX(), item->GetHexY()))
        {
            WriteLog("Add item '{}' to map '{}' failure, continue generate.\n", item->GetName(), map->GetName());
            itemMngr.DeleteItem(item);
        }
    }

    // Add children items
    for (Item* base_item : map->GetProtoMap()->ChildItemsVec)
    {
        // Map id
        uint parent_id = 0;
        if (base_item->GetAccessory() == ITEM_ACCESSORY_CRITTER)
            parent_id = base_item->GetCritId();
        else if (base_item->GetAccessory() == ITEM_ACCESSORY_CONTAINER)
            parent_id = base_item->GetContainerId();
        else
            UNREACHABLE_PLACE;

        if (!id_map.count(parent_id))
            continue;
        parent_id = id_map[parent_id];

        // Create item
        Item* item = itemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->Props);
        if (!item)
        {
            WriteLog("Create item '{}' on map '{}' fail, continue generate.\n", base_item->GetName(), map->GetName());
            continue;
        }

        // Add to parent
        if (base_item->GetAccessory() == ITEM_ACCESSORY_CRITTER)
        {
            Critter* cr_cont = map->GetCritter(parent_id);
            RUNTIME_ASSERT(cr_cont);
            crMngr.AddItemToCritter(cr_cont, item, false);
        }
        else if (base_item->GetAccessory() == ITEM_ACCESSORY_CONTAINER)
        {
            Item* item_cont = map->GetItem(parent_id);
            RUNTIME_ASSERT(item_cont);
            itemMngr.AddItemToContainer(item_cont, item, 0);
        }
        else
        {
            UNREACHABLE_PLACE;
        }
    }

    // Visible
    for (Npc* npc : map->GetNpcs())
    {
        ProcessVisibleCritters(npc);
        ProcessVisibleItems(npc);
    }

    // Map script
    map->SetScript(nullptr, true);
}

void MapManager::DeleteMapContent(Map* map)
{
    while (!map->mapCritters.empty() || !map->mapItems.empty())
    {
        // Transit players to global map
        KickPlayersToGlobalMap(map);

        // Delete npc
        NpcVec del_npcs = map->mapNpcs;
        for (Npc* del_npc : del_npcs)
            crMngr.DeleteNpc(del_npc);

        // Delete items
        ItemVec del_items = map->mapItems;
        for (Item* del_item : del_items)
            itemMngr.DeleteItem(del_item);
    }

    RUNTIME_ASSERT(map->mapItemsById.empty());
    RUNTIME_ASSERT(map->mapItemsByHex.empty());
    RUNTIME_ASSERT(map->mapBlockLinesByHex.empty());
}

bool MapManager::RestoreLocation(uint id, hash proto_id, const DataBase::Document& doc)
{
    ProtoLocation* proto = protoMngr.GetProtoLocation(proto_id);
    if (!proto)
    {
        WriteLog("Location proto '{}' is not loaded.\n", _str().parseHash(proto_id));
        return false;
    }

    Location* loc = new Location(id, proto);
    if (!loc->Props.LoadFromDbDocument(doc))
    {
        WriteLog("Fail to restore properties for location '{}' ({}).\n", _str().parseHash(proto_id), id);
        loc->Release();
        return false;
    }

    loc->BindScript();
    entityMngr.RegisterEntity(loc);
    return true;
}

string MapManager::GetLocationsMapsStatistics()
{
    EntityVec locations;
    entityMngr.GetEntities(EntityType::Location, locations);
    EntityVec maps;
    entityMngr.GetEntities(EntityType::Map, maps);

    string result = _str("Locations count: {}\n", (uint)locations.size());
    result += _str("Maps count: {}\n", (uint)maps.size());
    result +=
        "Location             Id           X     Y     Radius Color    Hidden  GeckVisible GeckCount AutoGarbage ToGarbage\n";
    result += "          Map                 Id          Time Rain Script\n";
    for (auto it = locations.begin(), end = locations.end(); it != end; ++it)
    {
        Location* loc = (Location*)*it;
        result += _str("{:<20} {:<10}   {:<5} {:<5} {:<6} {:08X} {:<7} {:<11} {:<9} {:<11} {:<5}\n", loc->GetName(),
            loc->GetId(), loc->GetWorldX(), loc->GetWorldY(), loc->GetRadius(), loc->GetColor(),
            loc->GetHidden() ? "true" : "false", loc->GetGeckVisible() ? "true" : "false", loc->GeckCount,
            loc->GetAutoGarbage() ? "true" : "false", loc->GetToGarbage() ? "true" : "false");

        uint map_index = 0;
        for (Map* map : loc->GetMaps())
        {
            result += _str("     {:02}) {:<20} {:<9}   {:<4} {:<4} ", map_index, map->GetName(), map->GetId(),
                map->GetCurDayTime(), map->GetRainCapacity());
            result += map->GetScriptId() ? _str().parseHash(map->GetScriptId()) : "";
            result += "\n";
            map_index++;
        }
    }
    return result;
}

Location* MapManager::CreateLocation(hash proto_id, ushort wx, ushort wy)
{
    ProtoLocation* proto = protoMngr.GetProtoLocation(proto_id);
    if (!proto)
    {
        WriteLog("Location proto '{}' is not loaded.\n", _str().parseHash(proto_id));
        return nullptr;
    }

    if (wx >= GM__MAXZONEX * GameOpt.GlobalMapZoneLength || wy >= GM__MAXZONEY * GameOpt.GlobalMapZoneLength)
    {
        WriteLog("Invalid location '{}' coordinates.\n", _str().parseHash(proto_id));
        return nullptr;
    }

    Location* loc = new Location(0, proto);
    loc->SetWorldX(wx);
    loc->SetWorldY(wy);
    CScriptArray* pids = loc->GetMapProtos();
    for (uint i = 0, j = pids->GetSize(); i < j; i++)
    {
        hash map_pid = *(hash*)pids->At(i);
        Map* map = CreateMap(map_pid, loc);
        if (!map)
        {
            WriteLog(
                "Create map '{}' for location '{}' fail.\n", _str().parseHash(map_pid), _str().parseHash(proto_id));
            for (auto& map : loc->GetMapsRaw())
                map->Release();
            loc->Release();
            pids->Release();
            return nullptr;
        }
    }
    pids->Release();
    loc->BindScript();

    entityMngr.RegisterEntity(loc);

    // Generate location maps
    MapVec maps = loc->GetMaps();
    for (Map* map : maps)
    {
        map->SetLocId(loc->GetId());
        GenerateMapContent(map);
    }

    // Init scripts
    for (Map* map : maps)
    {
        for (Item* item : map->GetItems())
            Script::RaiseInternalEvent(ServerFunctions.ItemInit, item, true);
        Script::RaiseInternalEvent(ServerFunctions.MapInit, map, true);
    }
    Script::RaiseInternalEvent(ServerFunctions.LocationInit, loc, true);

    return loc;
}

Map* MapManager::CreateMap(hash proto_id, Location* loc)
{
    ProtoMap* proto_map = protoMngr.GetProtoMap(proto_id);
    if (!proto_map)
    {
        WriteLog("Proto map '{}' is not loaded.\n", _str().parseHash(proto_id));
        return nullptr;
    }

    Map* map = new Map(0, proto_map, loc);
    MapVec& maps = loc->GetMapsRaw();
    map->SetLocId(loc->GetId());
    map->SetLocMapIndex((uint)maps.size());
    maps.push_back(map);

    entityMngr.RegisterEntity(map);
    return map;
}

bool MapManager::RestoreMap(uint id, hash proto_id, const DataBase::Document& doc)
{
    ProtoMap* proto = protoMngr.GetProtoMap(proto_id);
    if (!proto)
    {
        WriteLog("Map proto '{}' is not loaded.\n", _str().parseHash(proto_id));
        return false;
    }

    Map* map = new Map(id, proto, nullptr);
    if (!map->Props.LoadFromDbDocument(doc))
    {
        WriteLog("Fail to restore properties for map '{}' ({}).\n", _str().parseHash(proto_id), id);
        map->Release();
        return false;
    }

    entityMngr.RegisterEntity(map);
    return true;
}

void MapManager::RegenerateMap(Map* map)
{
    Script::RaiseInternalEvent(ServerFunctions.MapFinish, map);
    DeleteMapContent(map);
    GenerateMapContent(map);
    for (auto item : map->GetItems())
        Script::RaiseInternalEvent(ServerFunctions.ItemInit, item, true);
    Script::RaiseInternalEvent(ServerFunctions.MapInit, map, true);
}

Map* MapManager::GetMap(uint map_id)
{
    if (!map_id)
        return nullptr;
    return (Map*)entityMngr.GetEntity(map_id, EntityType::Map);
}

Map* MapManager::GetMapByPid(hash map_pid, uint skip_count)
{
    if (!map_pid)
        return nullptr;
    return entityMngr.GetMapByPid(map_pid, skip_count);
}

void MapManager::GetMaps(MapVec& maps)
{
    entityMngr.GetMaps(maps);
}

uint MapManager::GetMapsCount()
{
    return entityMngr.GetEntitiesCount(EntityType::Map);
}

Location* MapManager::GetLocationByMap(uint map_id)
{
    Map* map = GetMap(map_id);
    if (!map)
        return nullptr;
    return map->GetLocation();
}

Location* MapManager::GetLocation(uint loc_id)
{
    if (!loc_id)
        return nullptr;
    return (Location*)entityMngr.GetEntity(loc_id, EntityType::Location);
}

Location* MapManager::GetLocationByPid(hash loc_pid, uint skip_count)
{
    if (!loc_pid)
        return nullptr;
    return (Location*)entityMngr.GetLocationByPid(loc_pid, skip_count);
}

bool MapManager::IsIntersectZone(int wx1, int wy1, int w1_radius, int wx2, int wy2, int w2_radius, int zones)
{
    int zl = GM_ZONE_LEN;
    Rect r1((wx1 - w1_radius) / zl - zones, (wy1 - w1_radius) / zl - zones, (wx1 + w1_radius) / zl + zones,
        (wy1 + w1_radius) / zl + zones);
    Rect r2((wx2 - w2_radius) / zl, (wy2 - w2_radius) / zl, (wx2 + w2_radius) / zl, (wy2 + w2_radius) / zl);
    return r1.L <= r2.R && r2.L <= r1.R && r1.T <= r2.B && r2.T <= r1.B;
}

void MapManager::GetZoneLocations(int zx, int zy, int zone_radius, UIntVec& loc_ids)
{
    LocationVec locs;
    entityMngr.GetLocations(locs);
    int wx = zx * GM_ZONE_LEN;
    int wy = zy * GM_ZONE_LEN;
    for (auto it = locs.begin(), end = locs.end(); it != end; ++it)
    {
        Location* loc = *it;
        if (loc->IsLocVisible() &&
            IsIntersectZone(wx, wy, 0, loc->GetWorldX(), loc->GetWorldY(), loc->GetRadius(), zone_radius))
            loc_ids.push_back(loc->GetId());
    }
}

void MapManager::KickPlayersToGlobalMap(Map* map)
{
    for (Client* player : map->GetPlayers())
        TransitToGlobal(player, 0, true);
}

void MapManager::GetLocations(LocationVec& locs)
{
    entityMngr.GetLocations(locs);
}

uint MapManager::GetLocationsCount()
{
    return entityMngr.GetEntitiesCount(EntityType::Location);
}

void MapManager::LocationGarbager()
{
    if (runGarbager)
    {
        runGarbager = false;

        LocationVec locs;
        entityMngr.GetLocations(locs);

        ClientVec* gmap_players = nullptr;
        ClientVec players;
        for (auto it = locs.begin(); it != locs.end(); ++it)
        {
            Location* loc = *it;
            if (loc->GetAutoGarbage() && loc->IsCanDelete())
            {
                if (!gmap_players)
                {
                    crMngr.GetClients(players, true);
                    gmap_players = &players;
                }
                DeleteLocation(loc, gmap_players);
            }
        }
    }
}

void MapManager::DeleteLocation(Location* loc, ClientVec* gmap_players)
{
    // Start deleting
    MapVec maps = loc->GetMaps();

    // Redundant calls
    if (loc->IsDestroying || loc->IsDestroyed)
        return;

    loc->IsDestroying = true;
    for (auto it = maps.begin(); it != maps.end(); ++it)
        (*it)->IsDestroying = true;

    // Finish events
    Script::RaiseInternalEvent(ServerFunctions.LocationFinish, loc);
    for (auto it = maps.begin(); it != maps.end(); ++it)
        Script::RaiseInternalEvent(ServerFunctions.MapFinish, *it);

    // Send players on global map about this
    ClientVec players;
    if (!gmap_players)
    {
        crMngr.GetClients(players, true);
        gmap_players = &players;
    }

    for (Client* cl : *gmap_players)
        if (CheckKnownLocById(cl, loc->GetId()))
            cl->Send_GlobalLocation(loc, false);

    // Delete maps
    for (Map* map : maps)
        DeleteMapContent(map);
    loc->GetMapsRaw().clear();

    // Erase from main collections
    entityMngr.UnregisterEntity(loc);
    for (auto it = maps.begin(); it != maps.end(); ++it)
        entityMngr.UnregisterEntity(*it);

    // Invalidate for use
    loc->IsDestroyed = true;
    for (auto it = maps.begin(); it != maps.end(); ++it)
        (*it)->IsDestroyed = true;

    // Release after some time
    for (auto it = maps.begin(); it != maps.end(); ++it)
        (*it)->Release();
    loc->Release();
}

void MapManager::TraceBullet(TraceData& trace)
{
    Map* map = trace.TraceMap;
    ushort maxhx = map->GetWidth();
    ushort maxhy = map->GetHeight();
    ushort hx = trace.BeginHx;
    ushort hy = trace.BeginHy;
    ushort tx = trace.EndHx;
    ushort ty = trace.EndHy;

    uint dist = trace.Dist;
    if (!dist)
        dist = DistGame(hx, hy, tx, ty);

    ushort cx = hx;
    ushort cy = hy;
    ushort old_cx = cx;
    ushort old_cy = cy;
    uchar dir;

    LineTracer line_tracer(hx, hy, tx, ty, maxhx, maxhy, trace.Angle, !GameOpt.MapHexagonal);

    trace.IsFullTrace = false;
    trace.IsCritterFounded = false;
    trace.IsHaveLastPassed = false;
    bool last_passed_ok = false;
    for (uint i = 0;; i++)
    {
        if (i >= dist)
        {
            trace.IsFullTrace = true;
            break;
        }

        if (GameOpt.MapHexagonal)
        {
            dir = line_tracer.GetNextHex(cx, cy);
        }
        else
        {
            line_tracer.GetNextSquare(cx, cy);
            dir = GetNearDir(old_cx, old_cy, cx, cy);
        }

        if (trace.HexCallback)
        {
            trace.HexCallback(map, trace.FindCr, old_cx, old_cy, cx, cy, dir);
            old_cx = cx;
            old_cy = cy;
            continue;
        }

        if (trace.LastPassed && !last_passed_ok)
        {
            if (map->IsHexPassed(cx, cy))
            {
                (*trace.LastPassed).first = cx;
                (*trace.LastPassed).second = cy;
                trace.IsHaveLastPassed = true;
            }
            else if (!map->IsHexCritter(cx, cy) || !trace.LastPassedSkipCritters)
            {
                last_passed_ok = true;
            }
        }

        if (!map->IsHexRaked(cx, cy))
            break;

        if (trace.Critters != nullptr && map->IsHexCritter(cx, cy))
            map->GetCrittersHex(cx, cy, 0, trace.FindType, *trace.Critters);

        if (trace.FindCr && map->IsFlagCritter(cx, cy, false))
        {
            Critter* cr = map->GetHexCritter(cx, cy, false);
            if (cr == trace.FindCr)
            {
                trace.IsCritterFounded = true;
                break;
            }
        }

        old_cx = cx;
        old_cy = cy;
    }

    if (trace.PreBlock)
    {
        (*trace.PreBlock).first = old_cx;
        (*trace.PreBlock).second = old_cy;
    }
    if (trace.Block)
    {
        (*trace.Block).first = cx;
        (*trace.Block).second = cy;
    }
}

int thread_local MapGridOffsX = 0;
int thread_local MapGridOffsY = 0;
static thread_local short* Grid = nullptr;
#define GRID(x, y) \
    Grid[((FPATH_MAX_PATH + 1) + (y)-MapGridOffsY) * (FPATH_MAX_PATH * 2 + 2) + \
        ((FPATH_MAX_PATH + 1) + (x)-MapGridOffsX)]
int MapManager::FindPath(PathFindData& pfd)
{
    // Allocate temporary grid
    if (!Grid)
        Grid = new short[(FPATH_MAX_PATH * 2 + 2) * (FPATH_MAX_PATH * 2 + 2)];

    // Data
    uint map_id = pfd.MapId;
    ushort from_hx = pfd.FromX;
    ushort from_hy = pfd.FromY;
    ushort to_hx = pfd.ToX;
    ushort to_hy = pfd.ToY;
    uint multihex = pfd.Multihex;
    uint cut = pfd.Cut;
    uint trace = pfd.Trace;
    bool is_run = pfd.IsRun;
    bool check_cr = pfd.CheckCrit;
    bool check_gag_items = pfd.CheckGagItems;
    int dirs_count = DIRS_COUNT;

    // Checks
    if (trace && !pfd.TraceCr)
        return FPATH_TRACE_TARG_NULL_PTR;

    Map* map = GetMap(map_id);
    if (!map)
        return FPATH_MAP_NOT_FOUND;
    ushort maxhx = map->GetWidth();
    ushort maxhy = map->GetHeight();

    if (from_hx >= maxhx || from_hy >= maxhy || to_hx >= maxhx || to_hy >= maxhy)
        return FPATH_INVALID_HEXES;

    if (CheckDist(from_hx, from_hy, to_hx, to_hy, cut))
        return FPATH_ALREADY_HERE;
    if (!cut && FLAG(map->GetHexFlags(to_hx, to_hy), FH_NOWAY))
        return FPATH_HEX_BUSY;

    // Ring check
    if (cut <= 1 && !multihex)
    {
        short *rsx, *rsy;
        GetHexOffsets(to_hx & 1, rsx, rsy);

        int i = 0;
        for (; i < dirs_count; i++, rsx++, rsy++)
        {
            short xx = to_hx + *rsx;
            short yy = to_hy + *rsy;
            if (xx >= 0 && xx < maxhx && yy >= 0 && yy < maxhy)
            {
                ushort flags = map->GetHexFlags(xx, yy);
                if (FLAG(flags, FH_GAG_ITEM << 8))
                    break;
                if (!FLAG(flags, FH_NOWAY))
                    break;
            }
        }
        if (i == dirs_count)
            return FPATH_HEX_BUSY_RING;
    }

    // Parse previous move params
    /*UShortPairVec first_steps;
       uchar first_dir=pfd.MoveParams&7;
       if(first_dir<DIRS_COUNT)
       {
            ushort hx_=from_hx;
            ushort hy_=from_hy;
            MoveHexByDir(hx_,hy_,first_dir);
            if(map->IsHexPassed(hx_,hy_))
            {
                    first_steps.push_back(PAIR(hx_,hy_));
            }
       }
       for(int i=0;i<4;i++)
       {

       }*/

    // Prepare
    int numindex = 1;
    memzero(Grid, (FPATH_MAX_PATH * 2 + 2) * (FPATH_MAX_PATH * 2 + 2) * sizeof(short));
    MapGridOffsX = from_hx;
    MapGridOffsY = from_hy;
    GRID(from_hx, from_hy) = numindex;

    UShortPairVec coords, cr_coords, gag_coords;
    coords.reserve(10000);
    cr_coords.reserve(100);
    gag_coords.reserve(100);

    // First point
    coords.push_back(std::make_pair(from_hx, from_hy));

    // Begin search
    int p = 0, p_togo = 1;
    ushort cx, cy;
    while (true)
    {
        for (int i = 0; i < p_togo; i++, p++)
        {
            cx = coords[p].first;
            cy = coords[p].second;
            numindex = GRID(cx, cy);

            if (CheckDist(cx, cy, to_hx, to_hy, cut))
                goto label_FindOk;
            if (++numindex > FPATH_MAX_PATH)
                return FPATH_TOOFAR;

            short *sx, *sy;
            GetHexOffsets(cx & 1, sx, sy);

            for (int j = 0; j < dirs_count; j++)
            {
                short nx = (short)cx + sx[j];
                short ny = (short)cy + sy[j];
                if (nx < 0 || ny < 0 || nx >= maxhx || ny >= maxhy)
                    continue;

                short& g = GRID(nx, ny);
                if (g)
                    continue;

                if (!multihex)
                {
                    ushort flags = map->GetHexFlags(nx, ny);
                    if (!FLAG(flags, FH_NOWAY))
                    {
                        coords.push_back(std::make_pair(nx, ny));
                        g = numindex;
                    }
                    else if (check_gag_items && FLAG(flags, FH_GAG_ITEM << 8))
                    {
                        gag_coords.push_back(std::make_pair(nx, ny));
                        g = numindex | 0x4000;
                    }
                    else if (check_cr && FLAG(flags, FH_CRITTER << 8))
                    {
                        cr_coords.push_back(std::make_pair(nx, ny));
                        g = numindex | 0x8000;
                    }
                    else
                    {
                        g = -1;
                    }
                }
                else
                {
                    /*
                       // Multihex
                       // Base hex
                       int hx_=nx,hy_=ny;
                       for(uint k=0;k<multihex;k++) MoveHexByDirUnsafe(hx_,hy_,j);
                       if(hx_<0 || hy_<0 || hx_>=maxhx || hy_>=maxhy) continue;
                       //if(!IsHexPassed(hx_,hy_)) return false;

                       // Clock wise hexes
                       bool is_square_corner=(!GameOpt.MapHexagonal && IS_DIR_CORNER(j));
                       uint steps_count=(is_square_corner?multihex*2:multihex);
                       int dir_=(GameOpt.MapHexagonal?((j+2)%6):((j+2)%8));
                       if(is_square_corner) dir_=(dir_+1)%8;
                       int hx__=hx_,hy__=hy_;
                       for(uint k=0;k<steps_count;k++)
                       {
                            MoveHexByDirUnsafe(hx__,hy__,dir_);
                            //if(!IsHexPassed(hx__,hy__)) return false;
                       }

                       // Counter clock wise hexes
                       dir_=(GameOpt.MapHexagonal?((j+4)%6):((j+6)%8));
                       if(is_square_corner) dir_=(dir_+7)%8;
                       hx__=hx_,hy__=hy_;
                       for(uint k=0;k<steps_count;k++)
                       {
                            MoveHexByDirUnsafe(hx__,hy__,dir_);
                            //if(!IsHexPassed(hx__,hy__)) return false;
                       }
                     */

                    if (map->IsMovePassed(nx, ny, j, multihex))
                    {
                        coords.push_back(std::make_pair(nx, ny));
                        g = numindex;
                    }
                    else
                    {
                        g = -1;
                    }
                }
            }
        }

        // Add gag hex after some distance
        if (gag_coords.size())
        {
            short last_index = GRID(coords.back().first, coords.back().second);
            UShortPair& xy = gag_coords.front();
            short gag_index = GRID(xy.first, xy.second) ^ 0x4000;
            if (gag_index + 10 <
                last_index) // Todo: if path finding not be reworked than migrate magic number to scripts
            {
                GRID(xy.first, xy.second) = gag_index;
                coords.push_back(xy);
                gag_coords.erase(gag_coords.begin());
            }
        }

        // Add gag and critters hexes
        p_togo = (int)coords.size() - p;
        if (!p_togo)
        {
            if (gag_coords.size())
            {
                UShortPair& xy = gag_coords.front();
                GRID(xy.first, xy.second) ^= 0x4000;
                coords.push_back(xy);
                gag_coords.erase(gag_coords.begin());
                p_togo++;
            }
            else if (cr_coords.size())
            {
                UShortPair& xy = cr_coords.front();
                GRID(xy.first, xy.second) ^= 0x8000;
                coords.push_back(xy);
                cr_coords.erase(cr_coords.begin());
                p_togo++;
            }
        }

        if (!p_togo)
            return FPATH_DEADLOCK;
    }

label_FindOk:
    if (++pathNumCur >= FPATH_DATA_SIZE)
        pathNumCur = 1;
    PathStepVec& path = pathesPool[pathNumCur];
    path.resize(numindex - 1);

    // Smooth data
    if (!GameOpt.MapSmoothPath)
        smoothSwitcher = false;

    int smooth_count = 0, smooth_iteration = 0;
    if (GameOpt.MapSmoothPath && !GameOpt.MapHexagonal)
    {
        int x1 = cx, y1 = cy;
        int x2 = from_hx, y2 = from_hy;
        int dx = abs(x1 - x2);
        int dy = abs(y1 - y2);
        int d = MAX(dx, dy);
        int h1 = abs(dx - dy);
        int h2 = d - h1;
        if (dy < dx)
            std::swap(h1, h2);
        smooth_count = ((h1 && h2) ? h1 / h2 + 1 : 3);
        if (smooth_count < 3)
            smooth_count = 3;

        smooth_count = ((h1 && h2) ? MAX(h1, h2) / MIN(h1, h2) + 1 : 0);
        if (h1 && h2 && smooth_count < 2)
            smooth_count = 2;
        smooth_iteration = ((h1 && h2) ? MIN(h1, h2) % MAX(h1, h2) : 0);
    }

    while (numindex > 1)
    {
        if (GameOpt.MapSmoothPath)
        {
            if (GameOpt.MapHexagonal)
            {
                if (numindex & 1)
                    smoothSwitcher = !smoothSwitcher;
            }
            else
            {
                smoothSwitcher = (smooth_count < 2 || smooth_iteration % smooth_count);
            }
        }

        numindex--;
        PathStep& ps = path[numindex - 1];
        ps.HexX = cx;
        ps.HexY = cy;
        int dir = FindPathGrid(cx, cy, numindex, smoothSwitcher);
        if (dir == -1)
            return FPATH_ERROR;
        ps.Dir = dir;

        smooth_iteration++;
    }

    // Check for closed door and critter
    if (check_cr || check_gag_items)
    {
        for (int i = 0, j = (int)path.size(); i < j; i++)
        {
            PathStep& ps = path[i];
            if (map->IsHexPassed(ps.HexX, ps.HexY))
                continue;

            if (check_gag_items && map->IsHexGag(ps.HexX, ps.HexY))
            {
                Item* item = map->GetItemGag(ps.HexX, ps.HexY);
                if (!item)
                    continue;
                pfd.GagItem = item;
                path.resize(i);
                break;
            }

            if (check_cr && map->IsFlagCritter(ps.HexX, ps.HexY, false))
            {
                Critter* cr = map->GetHexCritter(ps.HexX, ps.HexY, false);
                if (!cr || cr == pfd.FromCritter)
                    continue;
                pfd.GagCritter = cr;
                path.resize(i);
                break;
            }
        }
    }

    // Trace
    if (trace)
    {
        IntVec trace_seq;
        ushort targ_hx = pfd.TraceCr->GetHexX();
        ushort targ_hy = pfd.TraceCr->GetHexY();
        bool trace_ok = false;

        trace_seq.resize(path.size() + 4);
        for (int i = 0, j = (int)path.size(); i < j; i++)
        {
            PathStep& ps = path[i];
            if (map->IsHexGag(ps.HexX, ps.HexY))
            {
                trace_seq[i + 2 - 2] += 1;
                trace_seq[i + 2 - 1] += 2;
                trace_seq[i + 2 - 0] += 3;
                trace_seq[i + 2 + 1] += 2;
                trace_seq[i + 2 + 2] += 1;
            }
        }

        TraceData trace_;
        trace_.TraceMap = map;
        trace_.EndHx = targ_hx;
        trace_.EndHy = targ_hy;
        trace_.FindCr = pfd.TraceCr;
        for (int k = 0; k < 5; k++)
        {
            for (int i = 0, j = (int)path.size(); i < j; i++)
            {
                if (k < 4 && trace_seq[i + 2] != k)
                    continue;
                if (k == 4 && trace_seq[i + 2] < 4)
                    continue;

                PathStep& ps = path[i];

                if (!CheckDist(ps.HexX, ps.HexY, targ_hx, targ_hy, trace))
                    continue;

                trace_.BeginHx = ps.HexX;
                trace_.BeginHy = ps.HexY;
                TraceBullet(trace_);
                if (trace_.IsCritterFounded)
                {
                    trace_ok = true;
                    path.resize(i + 1);
                    goto label_TraceOk;
                }
            }
        }

        if (!trace_ok && !pfd.GagItem && !pfd.GagCritter)
            return FPATH_TRACE_FAIL;
    label_TraceOk:
        if (trace_ok)
        {
            pfd.GagItem = nullptr;
            pfd.GagCritter = nullptr;
        }
    }

    // Parse move params
    PathSetMoveParams(path, is_run);

    // Number of path
    if (path.empty())
        return FPATH_ALREADY_HERE;
    pfd.PathNum = pathNumCur;

    // New X,Y
    PathStep& ps = path[path.size() - 1];
    pfd.NewToX = ps.HexX;
    pfd.NewToY = ps.HexY;
    return FPATH_OK;
}

int MapManager::FindPathGrid(ushort& hx, ushort& hy, int index, bool smooth_switcher)
{
    // Hexagonal
    if (GameOpt.MapHexagonal)
    {
        if (smooth_switcher)
        {
            if (hx & 1)
            {
                if (GRID(hx - 1, hy - 1) == index)
                {
                    hx--;
                    hy--;
                    return 3;
                } // 0
                if (GRID(hx, hy - 1) == index)
                {
                    hy--;
                    return 2;
                } // 5
                if (GRID(hx, hy + 1) == index)
                {
                    hy++;
                    return 5;
                } // 2
                if (GRID(hx + 1, hy) == index)
                {
                    hx++;
                    return 0;
                } // 3
                if (GRID(hx - 1, hy) == index)
                {
                    hx--;
                    return 4;
                } // 1
                if (GRID(hx + 1, hy - 1) == index)
                {
                    hx++;
                    hy--;
                    return 1;
                } // 4
            }
            else
            {
                if (GRID(hx - 1, hy) == index)
                {
                    hx--;
                    return 3;
                } // 0
                if (GRID(hx, hy - 1) == index)
                {
                    hy--;
                    return 2;
                } // 5
                if (GRID(hx, hy + 1) == index)
                {
                    hy++;
                    return 5;
                } // 2
                if (GRID(hx + 1, hy + 1) == index)
                {
                    hx++;
                    hy++;
                    return 0;
                } // 3
                if (GRID(hx - 1, hy + 1) == index)
                {
                    hx--;
                    hy++;
                    return 4;
                } // 1
                if (GRID(hx + 1, hy) == index)
                {
                    hx++;
                    return 1;
                } // 4
            }
        }
        else
        {
            if (hx & 1)
            {
                if (GRID(hx - 1, hy) == index)
                {
                    hx--;
                    return 4;
                } // 1
                if (GRID(hx + 1, hy - 1) == index)
                {
                    hx++;
                    hy--;
                    return 1;
                } // 4
                if (GRID(hx, hy - 1) == index)
                {
                    hy--;
                    return 2;
                } // 5
                if (GRID(hx - 1, hy - 1) == index)
                {
                    hx--;
                    hy--;
                    return 3;
                } // 0
                if (GRID(hx + 1, hy) == index)
                {
                    hx++;
                    return 0;
                } // 3
                if (GRID(hx, hy + 1) == index)
                {
                    hy++;
                    return 5;
                } // 2
            }
            else
            {
                if (GRID(hx - 1, hy + 1) == index)
                {
                    hx--;
                    hy++;
                    return 4;
                } // 1
                if (GRID(hx + 1, hy) == index)
                {
                    hx++;
                    return 1;
                } // 4
                if (GRID(hx, hy - 1) == index)
                {
                    hy--;
                    return 2;
                } // 5
                if (GRID(hx - 1, hy) == index)
                {
                    hx--;
                    return 3;
                } // 0
                if (GRID(hx + 1, hy + 1) == index)
                {
                    hx++;
                    hy++;
                    return 0;
                } // 3
                if (GRID(hx, hy + 1) == index)
                {
                    hy++;
                    return 5;
                } // 2
            }
        }
    }
    // Square
    else
    {
        // Without smoothing
        if (!GameOpt.MapSmoothPath)
        {
            if (GRID(hx - 1, hy) == index)
            {
                hx--;
                return 0;
            } // 0
            if (GRID(hx, hy - 1) == index)
            {
                hy--;
                return 6;
            } // 6
            if (GRID(hx, hy + 1) == index)
            {
                hy++;
                return 2;
            } // 2
            if (GRID(hx + 1, hy) == index)
            {
                hx++;
                return 4;
            } // 4
            if (GRID(hx - 1, hy + 1) == index)
            {
                hx--;
                hy++;
                return 1;
            } // 1
            if (GRID(hx + 1, hy - 1) == index)
            {
                hx++;
                hy--;
                return 5;
            } // 5
            if (GRID(hx + 1, hy + 1) == index)
            {
                hx++;
                hy++;
                return 3;
            } // 3
            if (GRID(hx - 1, hy - 1) == index)
            {
                hx--;
                hy--;
                return 7;
            } // 7
        }
        // With smoothing
        else
        {
            if (smooth_switcher)
            {
                if (GRID(hx - 1, hy) == index)
                {
                    hx--;
                    return 0;
                } // 0
                if (GRID(hx, hy + 1) == index)
                {
                    hy++;
                    return 2;
                } // 2
                if (GRID(hx + 1, hy) == index)
                {
                    hx++;
                    return 4;
                } // 4
                if (GRID(hx, hy - 1) == index)
                {
                    hy--;
                    return 6;
                } // 6
                if (GRID(hx + 1, hy + 1) == index)
                {
                    hx++;
                    hy++;
                    return 3;
                } // 3
                if (GRID(hx - 1, hy - 1) == index)
                {
                    hx--;
                    hy--;
                    return 7;
                } // 7
                if (GRID(hx - 1, hy + 1) == index)
                {
                    hx--;
                    hy++;
                    return 1;
                } // 1
                if (GRID(hx + 1, hy - 1) == index)
                {
                    hx++;
                    hy--;
                    return 5;
                } // 5
            }
            else
            {
                if (GRID(hx + 1, hy + 1) == index)
                {
                    hx++;
                    hy++;
                    return 3;
                } // 3
                if (GRID(hx - 1, hy - 1) == index)
                {
                    hx--;
                    hy--;
                    return 7;
                } // 7
                if (GRID(hx - 1, hy) == index)
                {
                    hx--;
                    return 0;
                } // 0
                if (GRID(hx, hy + 1) == index)
                {
                    hy++;
                    return 2;
                } // 2
                if (GRID(hx + 1, hy) == index)
                {
                    hx++;
                    return 4;
                } // 4
                if (GRID(hx, hy - 1) == index)
                {
                    hy--;
                    return 6;
                } // 6
                if (GRID(hx - 1, hy + 1) == index)
                {
                    hx--;
                    hy++;
                    return 1;
                } // 1
                if (GRID(hx + 1, hy - 1) == index)
                {
                    hx++;
                    hy--;
                    return 5;
                } // 5
            }
        }
    }

    return -1;
}

void MapManager::PathSetMoveParams(PathStepVec& path, bool is_run)
{
    uint move_params = 0; // Base parameters
    for (int i = (int)path.size() - 1; i >= 0; i--) // From end to beginning
    {
        PathStep& ps = path[i];

        // Walk flags
        if (is_run)
            SETFLAG(move_params, MOVE_PARAM_RUN);
        else
            UNSETFLAG(move_params, MOVE_PARAM_RUN);

        // Store
        ps.MoveParams = move_params;

        // Add dir to sequence
        move_params = (move_params << MOVE_PARAM_STEP_BITS) | ps.Dir | MOVE_PARAM_STEP_ALLOW;
    }
}

bool MapManager::TransitToGlobal(Critter* cr, uint leader_id, bool force)
{
    if (cr->LockMapTransfers)
    {
        WriteLog("Transfers locked, critter '{}'.\n", cr->GetName());
        return false;
    }

    return Transit(cr, nullptr, 0, 0, 0, 0, leader_id, force);
}

bool MapManager::Transit(
    Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint radius, uint leader_id, bool force)
{
    // Check location deletion
    Location* loc = (map ? map->GetLocation() : nullptr);
    if (loc && loc->GetToGarbage())
    {
        WriteLog("Transfer to deleted location, critter '{}'.\n", cr->GetName());
        return false;
    }

    // Maybe critter already in transfer
    if (cr->LockMapTransfers)
    {
        WriteLog("Transfers locked, critter '{}'.\n", cr->GetName());
        return false;
    }

    // Check force
    if (!force)
    {
        if (IS_TIMEOUT(cr->GetTimeoutTransfer()) || IS_TIMEOUT(cr->GetTimeoutBattle()))
            return false;
        if (cr->IsDead())
            return false;
        if (cr->IsKnockout())
            return false;
        if (loc && !loc->IsCanEnter(1))
            return false;
    }

    uint map_id = (map ? map->GetId() : 0);
    uint old_map_id = cr->GetMapId();
    Map* old_map = GetMap(old_map_id);

    // Recheck after synchronization
    if (cr->GetMapId() != old_map_id)
        return false;

    if (old_map_id == map_id)
    {
        // One map
        if (!map_id)
        {
            // Todo: check group
            return true;
        }

        if (!map || hx >= map->GetWidth() || hy >= map->GetHeight())
            return false;

        uint multihex = cr->GetMultihex();
        if (!map->FindStartHex(hx, hy, multihex, radius, true) && !map->FindStartHex(hx, hy, multihex, radius, false))
            return false;

        cr->LockMapTransfers++;

        cr->SetDir(dir >= DIRS_COUNT ? 0 : dir);
        map->UnsetFlagCritter(cr->GetHexX(), cr->GetHexY(), multihex, cr->IsDead());
        cr->SetHexX(hx);
        cr->SetHexY(hy);
        map->SetFlagCritter(hx, hy, multihex, cr->IsDead());
        cr->SetBreakTime(0);
        cr->Send_CustomCommand(cr, OTHER_TELEPORT, (cr->GetHexX() << 16) | (cr->GetHexY()));
        cr->ClearVisible();
        ProcessVisibleCritters(cr);
        ProcessVisibleItems(cr);
        cr->Send_XY(cr);

        cr->LockMapTransfers--;
    }
    else
    {
        // Different maps
        if (map)
        {
            uint multihex = cr->GetMultihex();
            if (!map->FindStartHex(hx, hy, multihex, radius, true) &&
                !map->FindStartHex(hx, hy, multihex, radius, false))
                return false;
        }
        if (!CanAddCrToMap(cr, map, hx, hy, leader_id))
            return false;

        cr->LockMapTransfers++;

        if (!old_map_id || old_map)
            EraseCrFromMap(cr, old_map);

        cr->SetLastMapHexX(cr->GetHexX());
        cr->SetLastMapHexY(cr->GetHexY());
        cr->SetBreakTime(0);

        AddCrToMap(cr, map, hx, hy, dir, leader_id);

        cr->Send_LoadMap(nullptr, *this);

        // Visible critters / items
        cr->DisableSend++;
        ProcessVisibleCritters(cr);
        ProcessVisibleItems(cr);
        cr->DisableSend--;

        cr->LockMapTransfers--;
    }
    return true;
}

bool MapManager::FindPlaceOnMap(Critter* cr, Map* map, ushort& hx, ushort& hy, uint radius)
{
    uint multihex = cr->GetMultihex();
    if (!map->FindStartHex(hx, hy, multihex, radius, true) && !map->FindStartHex(hx, hy, multihex, radius, false))
        return false;
    return true;
}

bool MapManager::CanAddCrToMap(Critter* cr, Map* map, ushort hx, ushort hy, uint leader_id)
{
    if (map)
    {
        if (hx >= map->GetWidth() || hy >= map->GetHeight())
            return false;
        if (!map->IsHexesPassed(hx, hy, cr->GetMultihex()))
            return false;
    }
    else
    {
        if (leader_id && leader_id != cr->GetId())
        {
            Critter* leader = crMngr.GetCritter(leader_id);
            if (!leader || leader->GetMapId())
                return false;
        }
    }
    return true;
}

void MapManager::AddCrToMap(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint leader_id)
{
    cr->LockMapTransfers++;

    cr->SetTimeoutBattle(0);
    cr->SetTimeoutTransfer(GameOpt.FullSecond + GameOpt.TimeoutTransfer);

    if (map)
    {
        RUNTIME_ASSERT((hx < map->GetWidth() && hy < map->GetHeight()));

        cr->SetMapId(map->GetId());
        cr->SetRefMapId(map->GetId());
        cr->SetRefMapPid(map->GetProtoId());
        cr->SetRefLocationId(map->GetLocation() ? map->GetLocation()->GetId() : 0);
        cr->SetRefLocationPid(map->GetLocation() ? map->GetLocation()->GetProtoId() : 0);
        cr->SetHexX(hx);
        cr->SetHexY(hy);
        cr->SetDir(dir);

        map->AddCritter(cr);

        Script::RaiseInternalEvent(ServerFunctions.MapCritterIn, map, cr);
    }
    else
    {
        RUNTIME_ASSERT(!cr->GlobalMapGroup);

        if (!leader_id || leader_id == cr->GetId())
        {
            cr->SetGlobalMapLeaderId(cr->GetId());
            cr->SetRefGlobalMapLeaderId(cr->GetId());
            cr->SetGlobalMapTripId(cr->GetGlobalMapTripId() + 1);
            cr->SetRefGlobalMapTripId(cr->GetGlobalMapTripId());

            cr->GlobalMapGroup = new CritterVec();
            cr->GlobalMapGroup->push_back(cr);
        }
        else
        {
            Critter* leader = crMngr.GetCritter(leader_id);
            RUNTIME_ASSERT(leader);
            RUNTIME_ASSERT(!leader->GetMapId());

            cr->SetWorldX(leader->GetWorldX());
            cr->SetWorldY(leader->GetWorldY());
            cr->SetGlobalMapLeaderId(leader_id);
            cr->SetRefGlobalMapLeaderId(leader_id);
            cr->SetGlobalMapTripId(leader->GetGlobalMapTripId());

            for (auto it = leader->GlobalMapGroup->begin(), end = leader->GlobalMapGroup->end(); it != end; ++it)
                (*it)->Send_AddCritter(cr);

            cr->GlobalMapGroup = leader->GlobalMapGroup;
            cr->GlobalMapGroup->push_back(cr);
        }

        Script::RaiseInternalEvent(ServerFunctions.GlobalMapCritterIn, cr);
    }

    cr->LockMapTransfers--;
}

void MapManager::EraseCrFromMap(Critter* cr, Map* map)
{
    cr->LockMapTransfers++;

    if (map)
    {
        Script::RaiseInternalEvent(ServerFunctions.MapCritterOut, map, cr);

        CritterVec critters = cr->VisCr;
        for (auto it = critters.begin(), end = critters.end(); it != end; ++it)
            Script::RaiseInternalEvent(ServerFunctions.CritterHide, *it, cr);

        cr->ClearVisible();
        map->EraseCritter(cr);
        map->UnsetFlagCritter(cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), cr->IsDead());

        cr->SetMapId(0);

        runGarbager = true;
    }
    else
    {
        RUNTIME_ASSERT(cr->GlobalMapGroup);

        Script::RaiseInternalEvent(ServerFunctions.GlobalMapCritterOut, cr);

        auto it = std::find(cr->GlobalMapGroup->begin(), cr->GlobalMapGroup->end(), cr);
        cr->GlobalMapGroup->erase(it);

        if (!cr->GlobalMapGroup->empty())
        {
            Critter* new_leader = *cr->GlobalMapGroup->begin();
            for (auto group_cr : *cr->GlobalMapGroup)
            {
                group_cr->SetGlobalMapLeaderId(new_leader->Id);
                group_cr->SetRefGlobalMapLeaderId(new_leader->Id);
                group_cr->Send_RemoveCritter(cr);
            }
            cr->GlobalMapGroup = nullptr;
        }
        else
        {
            SAFEDEL(cr->GlobalMapGroup);
        }

        cr->SetGlobalMapLeaderId(0);
    }

    cr->LockMapTransfers--;
}

void MapManager::ProcessVisibleCritters(Critter* view_cr)
{
    if (view_cr->IsDestroyed)
        return;

    // Global map
    if (!view_cr->GetMapId())
    {
        RUNTIME_ASSERT(view_cr->GlobalMapGroup);

        if (view_cr->IsPlayer())
        {
            Client* cl = (Client*)view_cr;
            for (auto it = view_cr->GlobalMapGroup->begin(), end = view_cr->GlobalMapGroup->end(); it != end; ++it)
            {
                Critter* cr = *it;
                if (view_cr == cr)
                {
                    SETFLAG(view_cr->Flags, FCRIT_CHOSEN);
                    cl->Send_AddCritter(view_cr);
                    UNSETFLAG(view_cr->Flags, FCRIT_CHOSEN);
                    cl->Send_AddAllItems();
                }
                else
                {
                    cl->Send_AddCritter(cr);
                }
            }
        }

        return;
    }

    // Local map
    Map* map = GetMap(view_cr->GetMapId());
    RUNTIME_ASSERT(map);

    int vis;
    int look_base_self = view_cr->GetLookDistance();
    int dir_self = view_cr->GetDir();
    int dirs_count = DIRS_COUNT;
    uint show_cr_dist1 = view_cr->GetShowCritterDist1();
    uint show_cr_dist2 = view_cr->GetShowCritterDist2();
    uint show_cr_dist3 = view_cr->GetShowCritterDist3();
    bool show_cr1 = (show_cr_dist1 > 0);
    bool show_cr2 = (show_cr_dist2 > 0);
    bool show_cr3 = (show_cr_dist3 > 0);

    bool show_cr = (show_cr1 || show_cr2 || show_cr3);
    // Sneak self
    int sneak_base_self = view_cr->GetSneakCoefficient();

    for (Critter* cr : map->GetCritters())
    {
        if (cr == view_cr || cr->IsDestroyed)
            continue;

        int dist = DistGame(view_cr->GetHexX(), view_cr->GetHexY(), cr->GetHexX(), cr->GetHexY());

        if (FLAG(GameOpt.LookChecks, LOOK_CHECK_SCRIPT))
        {
            bool allow_self = Script::RaiseInternalEvent(ServerFunctions.MapCheckLook, map, view_cr, cr);
            bool allow_opp = Script::RaiseInternalEvent(ServerFunctions.MapCheckLook, map, cr, view_cr);

            if (allow_self)
            {
                if (cr->AddCrIntoVisVec(view_cr))
                {
                    view_cr->Send_AddCritter(cr);
                    Script::RaiseInternalEvent(ServerFunctions.CritterShow, view_cr, cr);
                }
            }
            else
            {
                if (cr->DelCrFromVisVec(view_cr))
                {
                    view_cr->Send_RemoveCritter(cr);
                    Script::RaiseInternalEvent(ServerFunctions.CritterHide, view_cr, cr);
                }
            }

            if (allow_opp)
            {
                if (view_cr->AddCrIntoVisVec(cr))
                {
                    cr->Send_AddCritter(view_cr);
                    Script::RaiseInternalEvent(ServerFunctions.CritterShow, cr, view_cr);
                }
            }
            else
            {
                if (view_cr->DelCrFromVisVec(cr))
                {
                    cr->Send_RemoveCritter(view_cr);
                    Script::RaiseInternalEvent(ServerFunctions.CritterHide, cr, view_cr);
                }
            }

            if (show_cr)
            {
                if (show_cr1)
                {
                    if ((int)show_cr_dist1 >= dist)
                    {
                        if (view_cr->AddCrIntoVisSet1(cr->GetId()))
                            Script::RaiseInternalEvent(ServerFunctions.CritterShowDist1, view_cr, cr);
                    }
                    else
                    {
                        if (view_cr->DelCrFromVisSet1(cr->GetId()))
                            Script::RaiseInternalEvent(ServerFunctions.CritterHideDist1, view_cr, cr);
                    }
                }
                if (show_cr2)
                {
                    if ((int)show_cr_dist2 >= dist)
                    {
                        if (view_cr->AddCrIntoVisSet2(cr->GetId()))
                            Script::RaiseInternalEvent(ServerFunctions.CritterShowDist2, view_cr, cr);
                    }
                    else
                    {
                        if (view_cr->DelCrFromVisSet2(cr->GetId()))
                            Script::RaiseInternalEvent(ServerFunctions.CritterHideDist2, view_cr, cr);
                    }
                }
                if (show_cr3)
                {
                    if ((int)show_cr_dist3 >= dist)
                    {
                        if (view_cr->AddCrIntoVisSet3(cr->GetId()))
                            Script::RaiseInternalEvent(ServerFunctions.CritterShowDist3, view_cr, cr);
                    }
                    else
                    {
                        if (view_cr->DelCrFromVisSet3(cr->GetId()))
                            Script::RaiseInternalEvent(ServerFunctions.CritterHideDist3, view_cr, cr);
                    }
                }
            }

            uint cr_dist = cr->GetShowCritterDist1();
            if (cr_dist)
            {
                if ((int)cr_dist >= dist)
                {
                    if (cr->AddCrIntoVisSet1(view_cr->GetId()))
                        Script::RaiseInternalEvent(ServerFunctions.CritterShowDist1, cr, view_cr);
                }
                else
                {
                    if (cr->DelCrFromVisSet1(view_cr->GetId()))
                        Script::RaiseInternalEvent(ServerFunctions.CritterHideDist1, cr, view_cr);
                }
            }
            cr_dist = cr->GetShowCritterDist2();
            if (cr_dist)
            {
                if ((int)cr_dist >= dist)
                {
                    if (cr->AddCrIntoVisSet2(view_cr->GetId()))
                        Script::RaiseInternalEvent(ServerFunctions.CritterShowDist2, cr, view_cr);
                }
                else
                {
                    if (cr->DelCrFromVisSet2(view_cr->GetId()))
                        Script::RaiseInternalEvent(ServerFunctions.CritterHideDist2, cr, view_cr);
                }
            }
            cr_dist = cr->GetShowCritterDist3();
            if (cr_dist)
            {
                if ((int)cr_dist >= dist)
                {
                    if (cr->AddCrIntoVisSet3(view_cr->GetId()))
                        Script::RaiseInternalEvent(ServerFunctions.CritterShowDist1, cr, view_cr);
                }
                else
                {
                    if (cr->DelCrFromVisSet3(view_cr->GetId()))
                        Script::RaiseInternalEvent(ServerFunctions.CritterHideDist3, cr, view_cr);
                }
            }
            continue;
        }

        int look_self = look_base_self;
        int look_opp = cr->GetLookDistance();

        // Dir modifier
        if (FLAG(GameOpt.LookChecks, LOOK_CHECK_DIR))
        {
            // Self
            int real_dir = GetFarDir(view_cr->GetHexX(), view_cr->GetHexY(), cr->GetHexX(), cr->GetHexY());
            int i = (dir_self > real_dir ? dir_self - real_dir : real_dir - dir_self);
            if (i > dirs_count / 2)
                i = dirs_count - i;
            look_self -= look_self * GameOpt.LookDir[i] / 100;
            // Opponent
            int dir_opp = cr->GetDir();
            real_dir = ((real_dir + dirs_count / 2) % dirs_count);
            i = (dir_opp > real_dir ? dir_opp - real_dir : real_dir - dir_opp);
            if (i > dirs_count / 2)
                i = dirs_count - i;
            look_opp -= look_opp * GameOpt.LookDir[i] / 100;
        }

        if (dist > look_self && dist > look_opp)
            dist = MAX_INT;

        // Trace
        if (FLAG(GameOpt.LookChecks, LOOK_CHECK_TRACE) && dist != MAX_INT)
        {
            TraceData trace;
            trace.TraceMap = map;
            trace.BeginHx = view_cr->GetHexX();
            trace.BeginHy = view_cr->GetHexY();
            trace.EndHx = cr->GetHexX();
            trace.EndHy = cr->GetHexY();
            TraceBullet(trace);
            if (!trace.IsFullTrace)
                dist = MAX_INT;
        }

        // Self
        if (cr->GetIsHide() && dist != MAX_INT)
        {
            int sneak_opp = cr->GetSneakCoefficient();
            if (FLAG(GameOpt.LookChecks, LOOK_CHECK_SNEAK_DIR))
            {
                int real_dir = GetFarDir(view_cr->GetHexX(), view_cr->GetHexY(), cr->GetHexX(), cr->GetHexY());
                int i = (dir_self > real_dir ? dir_self - real_dir : real_dir - dir_self);
                if (i > dirs_count / 2)
                    i = dirs_count - i;
                sneak_opp -= sneak_opp * GameOpt.LookSneakDir[i] / 100;
            }
            sneak_opp /= (int)GameOpt.SneakDivider;
            if (sneak_opp < 0)
                sneak_opp = 0;
            vis = look_self - sneak_opp;
        }
        else
        {
            vis = look_self;
        }
        if (vis < (int)GameOpt.LookMinimum)
            vis = GameOpt.LookMinimum;

        if (vis >= dist)
        {
            if (cr->AddCrIntoVisVec(view_cr))
            {
                view_cr->Send_AddCritter(cr);
                Script::RaiseInternalEvent(ServerFunctions.CritterShow, view_cr, cr);
            }
        }
        else
        {
            if (cr->DelCrFromVisVec(view_cr))
            {
                view_cr->Send_RemoveCritter(cr);
                Script::RaiseInternalEvent(ServerFunctions.CritterHide, view_cr, cr);
            }
        }

        if (show_cr1)
        {
            if ((int)show_cr_dist1 >= dist)
            {
                if (view_cr->AddCrIntoVisSet1(cr->GetId()))
                    Script::RaiseInternalEvent(ServerFunctions.CritterShowDist1, view_cr, cr);
            }
            else
            {
                if (view_cr->DelCrFromVisSet1(cr->GetId()))
                    Script::RaiseInternalEvent(ServerFunctions.CritterHideDist1, view_cr, cr);
            }
        }
        if (show_cr2)
        {
            if ((int)show_cr_dist2 >= dist)
            {
                if (view_cr->AddCrIntoVisSet2(cr->GetId()))
                    Script::RaiseInternalEvent(ServerFunctions.CritterShowDist2, view_cr, cr);
            }
            else
            {
                if (view_cr->DelCrFromVisSet2(cr->GetId()))
                    Script::RaiseInternalEvent(ServerFunctions.CritterHideDist2, view_cr, cr);
            }
        }
        if (show_cr3)
        {
            if ((int)show_cr_dist3 >= dist)
            {
                if (view_cr->AddCrIntoVisSet3(cr->GetId()))
                    Script::RaiseInternalEvent(ServerFunctions.CritterShowDist3, view_cr, cr);
            }
            else
            {
                if (view_cr->DelCrFromVisSet3(cr->GetId()))
                    Script::RaiseInternalEvent(ServerFunctions.CritterHideDist3, view_cr, cr);
            }
        }

        // Opponent
        if (view_cr->GetIsHide() && dist != MAX_INT)
        {
            int sneak_self = sneak_base_self;
            if (FLAG(GameOpt.LookChecks, LOOK_CHECK_SNEAK_DIR))
            {
                int dir_opp = cr->GetDir();
                int real_dir = GetFarDir(cr->GetHexX(), cr->GetHexY(), view_cr->GetHexX(), view_cr->GetHexY());
                int i = (dir_opp > real_dir ? dir_opp - real_dir : real_dir - dir_opp);
                if (i > dirs_count / 2)
                    i = dirs_count - i;
                sneak_self -= sneak_self * GameOpt.LookSneakDir[i] / 100;
            }
            sneak_self /= (int)GameOpt.SneakDivider;
            if (sneak_self < 0)
                sneak_self = 0;
            vis = look_opp - sneak_self;
        }
        else
        {
            vis = look_opp;
        }
        if (vis < (int)GameOpt.LookMinimum)
            vis = GameOpt.LookMinimum;

        if (vis >= dist)
        {
            if (view_cr->AddCrIntoVisVec(cr))
            {
                cr->Send_AddCritter(view_cr);
                Script::RaiseInternalEvent(ServerFunctions.CritterShow, cr, view_cr);
            }
        }
        else
        {
            if (view_cr->DelCrFromVisVec(cr))
            {
                cr->Send_RemoveCritter(view_cr);
                Script::RaiseInternalEvent(ServerFunctions.CritterHide, cr, view_cr);
            }
        }

        uint cr_dist = cr->GetShowCritterDist1();
        if (cr_dist)
        {
            if ((int)cr_dist >= dist)
            {
                if (cr->AddCrIntoVisSet1(view_cr->GetId()))
                    Script::RaiseInternalEvent(ServerFunctions.CritterShowDist1, cr, view_cr);
            }
            else
            {
                if (cr->DelCrFromVisSet1(view_cr->GetId()))
                    Script::RaiseInternalEvent(ServerFunctions.CritterHideDist1, cr, view_cr);
            }
        }
        cr_dist = cr->GetShowCritterDist2();
        if (cr_dist)
        {
            if ((int)cr_dist >= dist)
            {
                if (cr->AddCrIntoVisSet2(view_cr->GetId()))
                    Script::RaiseInternalEvent(ServerFunctions.CritterShowDist2, cr, view_cr);
            }
            else
            {
                if (cr->DelCrFromVisSet2(view_cr->GetId()))
                    Script::RaiseInternalEvent(ServerFunctions.CritterHideDist2, cr, view_cr);
            }
        }
        cr_dist = cr->GetShowCritterDist3();
        if (cr_dist)
        {
            if ((int)cr_dist >= dist)
            {
                if (cr->AddCrIntoVisSet3(view_cr->GetId()))
                    Script::RaiseInternalEvent(ServerFunctions.CritterShowDist3, cr, view_cr);
            }
            else
            {
                if (cr->DelCrFromVisSet3(view_cr->GetId()))
                    Script::RaiseInternalEvent(ServerFunctions.CritterHideDist3, cr, view_cr);
            }
        }
    }
}

void MapManager::ProcessVisibleItems(Critter* view_cr)
{
    if (view_cr->IsDestroyed)
        return;

    uint map_id = view_cr->GetMapId();
    if (!map_id)
        return;

    Map* map = GetMap(map_id);
    RUNTIME_ASSERT(map);

    int look = view_cr->GetLookDistance();
    for (Item* item : map->GetItems())
    {
        if (item->GetIsHidden())
        {
            continue;
        }
        else if (item->GetIsAlwaysView())
        {
            if (view_cr->AddIdVisItem(item->GetId()))
            {
                view_cr->Send_AddItemOnMap(item);
                Script::RaiseInternalEvent(
                    ServerFunctions.CritterShowItemOnMap, view_cr, item, item->ViewPlaceOnMap, item->ViewByCritter);
            }
        }
        else
        {
            bool allowed = false;
            if (item->GetIsTrap() && FLAG(GameOpt.LookChecks, LOOK_CHECK_ITEM_SCRIPT))
            {
                allowed = Script::RaiseInternalEvent(ServerFunctions.MapCheckTrapLook, map, view_cr, item);
            }
            else
            {
                int dist = DistGame(view_cr->GetHexX(), view_cr->GetHexY(), item->GetHexX(), item->GetHexY());
                if (item->GetIsTrap())
                    dist += item->GetTrapValue();
                allowed = look >= dist;
            }

            if (allowed)
            {
                if (view_cr->AddIdVisItem(item->GetId()))
                {
                    view_cr->Send_AddItemOnMap(item);
                    Script::RaiseInternalEvent(
                        ServerFunctions.CritterShowItemOnMap, view_cr, item, item->ViewPlaceOnMap, item->ViewByCritter);
                }
            }
            else
            {
                if (view_cr->DelIdVisItem(item->GetId()))
                {
                    view_cr->Send_EraseItemFromMap(item);
                    Script::RaiseInternalEvent(
                        ServerFunctions.CritterHideItemOnMap, view_cr, item, item->ViewPlaceOnMap, item->ViewByCritter);
                }
            }
        }
    }
}

void MapManager::ViewMap(Critter* view_cr, Map* map, int look, ushort hx, ushort hy, int dir)
{
    if (view_cr->IsDestroyed)
        return;

    view_cr->Send_GameInfo(map);

    // Critters
    int dirs_count = DIRS_COUNT;
    int vis;
    for (Critter* cr : map->GetCritters())
    {
        if (cr == view_cr || cr->IsDestroyed)
            continue;

        if (FLAG(GameOpt.LookChecks, LOOK_CHECK_SCRIPT))
        {
            if (Script::RaiseInternalEvent(ServerFunctions.MapCheckLook, map, view_cr, cr))
                view_cr->Send_AddCritter(cr);
            continue;
        }

        int dist = DistGame(hx, hy, cr->GetHexX(), cr->GetHexY());
        int look_self = look;

        // Dir modifier
        if (FLAG(GameOpt.LookChecks, LOOK_CHECK_DIR))
        {
            int real_dir = GetFarDir(hx, hy, cr->GetHexX(), cr->GetHexY());
            int i = (dir > real_dir ? dir - real_dir : real_dir - dir);
            if (i > dirs_count / 2)
                i = dirs_count - i;
            look_self -= look_self * GameOpt.LookDir[i] / 100;
        }

        if (dist > look_self)
            continue;

        // Trace
        if (FLAG(GameOpt.LookChecks, LOOK_CHECK_TRACE) && dist != MAX_INT)
        {
            TraceData trace;
            trace.TraceMap = map;
            trace.BeginHx = hx;
            trace.BeginHy = hy;
            trace.EndHx = cr->GetHexX();
            trace.EndHy = cr->GetHexY();
            TraceBullet(trace);
            if (!trace.IsFullTrace)
                continue;
        }

        // Hide modifier
        if (cr->GetIsHide())
        {
            int sneak_opp = cr->GetSneakCoefficient();
            if (FLAG(GameOpt.LookChecks, LOOK_CHECK_SNEAK_DIR))
            {
                int real_dir = GetFarDir(hx, hy, cr->GetHexX(), cr->GetHexY());
                int i = (dir > real_dir ? dir - real_dir : real_dir - dir);
                if (i > dirs_count / 2)
                    i = dirs_count - i;
                sneak_opp -= sneak_opp * GameOpt.LookSneakDir[i] / 100;
            }
            sneak_opp /= (int)GameOpt.SneakDivider;
            if (sneak_opp < 0)
                sneak_opp = 0;
            vis = look_self - sneak_opp;
        }
        else
        {
            vis = look_self;
        }
        if (vis < (int)GameOpt.LookMinimum)
            vis = GameOpt.LookMinimum;
        if (vis >= dist)
            view_cr->Send_AddCritter(cr);
    }

    // Items
    for (Item* item : map->GetItems())
    {
        if (item->GetIsHidden())
        {
            continue;
        }
        else if (item->GetIsAlwaysView())
        {
            view_cr->Send_AddItemOnMap(item);
        }
        else
        {
            bool allowed = false;
            if (item->GetIsTrap() && FLAG(GameOpt.LookChecks, LOOK_CHECK_ITEM_SCRIPT))
            {
                allowed = Script::RaiseInternalEvent(ServerFunctions.MapCheckTrapLook, map, view_cr, item);
            }
            else
            {
                int dist = DistGame(hx, hy, item->GetHexX(), item->GetHexY());
                if (item->GetIsTrap())
                    dist += item->GetTrapValue();
                allowed = look >= dist;
            }

            if (allowed)
                view_cr->Send_AddItemOnMap(item);
        }
    }
}

bool MapManager::CheckKnownLocById(Critter* cr, uint loc_id)
{
    CScriptArray* known_locs = cr->GetKnownLocations();
    for (uint i = 0, j = known_locs->GetSize(); i < j; i++)
    {
        if (*(uint*)known_locs->At(i) == loc_id)
        {
            SAFEREL(known_locs);
            return true;
        }
    }
    SAFEREL(known_locs);
    return false;
}

bool MapManager::CheckKnownLocByPid(Critter* cr, hash loc_pid)
{
    CScriptArray* known_locs = cr->GetKnownLocations();
    SAFEREL(known_locs);
    for (uint i = 0, j = known_locs->GetSize(); i < j; i++)
    {
        Location* loc = GetLocation(*(uint*)known_locs->At(i));
        if (loc && loc->GetProtoId() == loc_pid)
        {
            SAFEREL(known_locs);
            return true;
        }
    }
    SAFEREL(known_locs);
    return false;
}

void MapManager::AddKnownLoc(Critter* cr, uint loc_id)
{
    if (CheckKnownLocById(cr, loc_id))
        return;

    CScriptArray* known_locs = cr->GetKnownLocations();
    known_locs->InsertLast(&loc_id);
    cr->SetKnownLocations(known_locs);
    SAFEREL(known_locs);
}

void MapManager::EraseKnownLoc(Critter* cr, uint loc_id)
{
    CScriptArray* known_locs = cr->GetKnownLocations();
    for (uint i = 0, j = known_locs->GetSize(); i < j; i++)
    {
        if (*(uint*)known_locs->At(i) == loc_id)
        {
            known_locs->RemoveAt(i);
            cr->SetKnownLocations(known_locs);
            break;
        }
    }
    SAFEREL(known_locs);
}
