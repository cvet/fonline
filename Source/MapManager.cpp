#include "StdAfx.h"
#include "MapManager.h"
#include "Log.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "Script.h"
#include "LineTracer.h"

MapManager MapMngr;

/************************************************************************/
/* GlobalMapGroup                                                            */
/************************************************************************/

bool GlobalMapGroup::IsValid()
{
	return CritMove.size()!=0;
}

bool GlobalMapGroup::IsMoving()
{
	return Speed>0.0f && (CurX!=ToX || CurY!=ToY);
}

uint GlobalMapGroup::GetSize()
{
	return (uint)CritMove.size();
}

void GlobalMapGroup::Stop()
{
	Speed=0.0f;
	ToX=CurX;
	ToY=CurY;
}

void GlobalMapGroup::SyncLockGroup()
{
	CrVec critters=CritMove;
	for(CrVecIt it=critters.begin(),end=critters.end();it!=end;++it) SYNC_LOCK(*it);
}

Critter* GlobalMapGroup::GetCritter(uint crid)
{
	for(CrVecIt it=CritMove.begin(),end=CritMove.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->GetId()==crid)
		{
			SYNC_LOCK(cr);
			return cr;
		}
	}
	return NULL;
}

Item* GlobalMapGroup::GetCar()
{
	if(!CarId) return NULL;
	Item* car=ItemMngr.GetItem(CarId,true);
	if(!car) CarId=0;
	return car;
}

bool GlobalMapGroup::CheckForFollow(Critter* cr)
{
	if(IsSetMove) return false;
	if(Timer::GameTick()>=TimeCanFollow) return false;
	if(std::find(CritMove.begin(),CritMove.end(),cr)!=CritMove.end()) return false;
	if(CritMove.size()>=GM_MAX_GROUP_COUNT) return false;
	return true;
}

void GlobalMapGroup::AddCrit(Critter* cr)
{
	uint id=cr->GetId();
	if(std::find(CritMove.begin(),CritMove.end(),cr)==CritMove.end()) CritMove.push_back(cr);
}

void GlobalMapGroup::EraseCrit(Critter* cr)
{
	CrVecIt it=std::find(CritMove.begin(),CritMove.end(),cr);
	if(it!=CritMove.end()) CritMove.erase(it);
}

void GlobalMapGroup::Clear()
{
	CritMove.clear();
	CurX=CurY=ToX=ToY=0.0f;
	CarId=0;
	IsSetMove=false;
	TimeCanFollow=0;
	IsMultiply=true;
	ProcessLastTick=Timer::GameTick();
	EncounterDescriptor=0;
	EncounterTick=0;
	EncounterForce=false;
}

/************************************************************************/
/* MapMngr                                                              */
/************************************************************************/

MapManager::MapManager():lastMapId(0),lastLocId(0),runGarbager(true)
{
	MEMORY_PROCESS(MEMORY_STATIC,sizeof(MapManager));
	MEMORY_PROCESS(MEMORY_STATIC,(FPATH_MAX_PATH*2+2)*(FPATH_MAX_PATH*2+2)); // Grid, see below
}

bool MapManager::Init()
{
	WriteLog("Map manager initialization...\n");

	if(!ItemMngr.IsInit())
	{
		WriteLog("Error, Item manager not initialized.\n");
		return false;
	}

	if(!CrMngr.IsInit())
	{
		WriteLog("Error, Critter manager not initialized.\n");
		return false;
	}

	pathNumCur=0;
	for(int i=1;i<FPATH_DATA_SIZE;i++) pathesPool[i].reserve(100);

	WriteLog("Map manager initialization complete.\n");
	return true;
}

void MapManager::Finish()
{
	WriteLog("Map manager finish...\n");

	for(LocMapIt it=allLocations.begin();it!=allLocations.end();++it)
	{
		Location* loc=(*it).second;
		loc->Clear(false);
		loc->Release();
	}
	allLocations.clear();
	allMaps.clear();

	for(int i=0;i<MAX_PROTO_MAPS;i++) ProtoMaps[i].Clear();

	WriteLog("Map manager finish complete.\n");
}

void MapManager::Clear()
{
	lastMapId=0;
	lastLocId=0;
	runGarbager=false;

	for(LocMapIt it=allLocations.begin();it!=allLocations.end();++it)
		SAFEREL((*it).second);
	allLocations.clear();
	for(MapMapIt it=allMaps.begin();it!=allMaps.end();++it)
		SAFEREL((*it).second);
	allMaps.clear();
}

UIntPair EntranceParser(const char* str)
{
	int val1,val2;
	if(sscanf(str,"%d %d",&val1,&val2)!=2 || val1<0 || val1>0xFF || val2<0 || val2>0xFF) return UIntPair(-1,-1);
	return UIntPair(val1,val2);
}

bool MapManager::LoadLocationsProtos()
{
	WriteLog("Load location and map prototypes...\n");

	// Load location prototypes
	IniParser city_txt;
	if(!city_txt.LoadFile("Locations.cfg",PT_SERVER_MAPS))
	{
		WriteLog("File<%s> not found.\n",FileManager::GetFullPath("Locations.cfg",PT_SERVER_MAPS));
		return false;
	}

	city_txt.CacheApps();

	int errors=0;
	uint loaded=0;
	char res[256];
	for(int i=1;i<MAX_PROTO_LOCATIONS;i++)
	{
		ProtoLoc[i].IsInit=false;
		const char* app=Str::FormatBuf("Area %u",i);
		if(!city_txt.IsCachedApp(app) || !city_txt.GetStr(app,"map_0","error",res)) continue;

		ProtoLocation& ploc=ProtoLoc[i];
		if(!LoadLocationProto(city_txt,ploc,i))
		{
			errors++;
			WriteLog("Load location<%u> fail.\n",i);
			continue;
		}
		loaded++;
	}

	// Check for errors
	if(errors)
	{
		WriteLog("Load location and map prototypes fail, errors<%d>.\n",errors);
		return false;
	}

	WriteLog("Load location and map prototypes complete, loaded<%u> location protos.\n",loaded);
	return true;
}

bool MapManager::LoadLocationProto(IniParser& city_txt, ProtoLocation& ploc, ushort pid)
{
	char key1[256];
	char key2[256];
	char res[512];
	sprintf(key1,"Area %u",pid);

	ploc.IsInit=false;
	city_txt.GetStr(key1,"name","",res);
	ploc.Name=res;
	if(!ploc.Name.length()) ploc.Name=Str::FormatBuf("Location %u",pid);
	ploc.MaxPlayers=city_txt.GetInt(key1,"max_players",0);
	ploc.Radius=city_txt.GetInt(key1,"size",24);

	// Maps
	ploc.ProtoMapPids.clear();
	uint cur_map=0;
	while(true)
	{
		sprintf(key2,"map_%u",cur_map);
		if(!city_txt.GetStr(key1,key2,"",res)) break;

		char map_name[MAX_FOPATH];
		ushort map_pid=0;
		if(sscanf(res,"%s%u",map_name,&map_pid)!=2)
		{
			WriteLogF(_FUNC_," - Can't parse data in location<%s>, map index<%u>.\n",ploc.Name.c_str(),cur_map);
			return false;
		}

		if(!map_name[0] || !map_pid || map_pid>=MAX_PROTO_MAPS)
		{
			WriteLogF(_FUNC_," - Invalid data in location<%s>, map index<%u>.\n",ploc.Name.c_str(),cur_map);
			return false;
		}

		size_t len=Str::Length(map_name);
		bool is_automap=false;
		if(map_name[len-1]=='*')
		{
			if(len==1) break;
			is_automap=true;
			map_name[len-1]=0;
		}

		ProtoMap& pmap=ProtoMaps[map_pid];
		if(pmap.IsInit() && !Str::CompareCase(pmap.GetName(),map_name))
		{
			WriteLogF(_FUNC_," - Pid<%u> for map<%s> in location<%s>, already in use.\n",map_pid,map_name,ploc.Name.c_str());
			return false;
		}

		if(!pmap.IsInit() && !pmap.Init(map_pid,map_name,PT_SERVER_MAPS))
		{
			WriteLogF(_FUNC_," - Init proto map<%s> for location<%s> fail.\n",map_name,ploc.Name.c_str());
			return false;
		}

		ploc.ProtoMapPids.push_back(map_pid);
		if(is_automap) ploc.AutomapsPids.push_back(map_pid);
		cur_map++;
	}

	if(!cur_map)
	{
		WriteLogF(_FUNC_," - Not found no one map, location<%s>.\n",ploc.Name.c_str());
		return false;
	}

	// Entrance
	ploc.Entrance.clear();
	city_txt.GetStr(key1,"entrance","1",res);
	if(res[0]=='$')
	{
		Str::ParseLine<UIntPairVec,UIntPair(*)(const char*)>(&res[1],',',ploc.Entrance,EntranceParser);
		for(uint k=0,l=(uint)ploc.Entrance.size();k<l;k++)
		{
			uint map_num=ploc.Entrance[k].first;
			if(map_num>cur_map)
			{
				WriteLogF(_FUNC_," - Invalid map number<%u>, entrance<%s>, location<%s>.\n",map_num,res,ploc.Name.c_str());
				return false;
			}
		}
	}
	else
	{
		int val=atoi(res);
		if(val<=0 || val>(int)cur_map)
		{
			WriteLogF(_FUNC_," - Invalid entrance value<%d>, location<%s>.\n",val,ploc.Name.c_str());
			return false;
		}
		for(int k=0;k<val;k++) ploc.Entrance.push_back(UIntPairVecVal(k,0));
	}

	for(uint k=0,l=(uint)ploc.Entrance.size();k<l;k++)
	{
		uint map_num=ploc.Entrance[k].first;
		uint entire=ploc.Entrance[k].second;
		if(!GetProtoMap(ploc.ProtoMapPids[map_num])->CountEntire(entire))
		{
			WriteLogF(_FUNC_," - Entire<%u> not found on map<%u>, location<%s>.\n",entire,map_num,ploc.Name.c_str());
			return false;
		}
	}

	// Entrance function
	ploc.ScriptBindId=0;
	city_txt.GetStr(key1,"entrance_script","",res);
	if(res[0])
	{
		int bind_id=Script::Bind(res,"bool %s(Location&,Critter@[]&,uint8)",false);
		if(bind_id<=0)
		{
			WriteLogF(_FUNC_," - Function<%s> not found, location<%s>.\n",res,ploc.Name.c_str());
			return false;
		}
		ploc.ScriptBindId=bind_id;
	}

	// Other
	ploc.Visible=city_txt.GetInt(key1,"visible",0)!=0;
	ploc.GeckVisible=city_txt.GetInt(key1,"geck_visible",0)!=0;
	ploc.AutoGarbage=city_txt.GetInt(key1,"auto_garbage",1)!=0;

	// Initialize
	ploc.LocPid=pid;
	ploc.IsInit=true;
	return true;
}

void MapManager::SaveAllLocationsAndMapsFile(void(*save_func)(void*,size_t))
{
	uint count=(uint)allLocations.size();
	save_func(&count,sizeof(count));

	for(LocMapIt it=allLocations.begin(),end=allLocations.end();it!=end;++it)
	{
		Location* loc=(*it).second;
		save_func(&loc->Data,sizeof(loc->Data));

		MapVec& maps=loc->GetMapsNoLock();
		uint map_count=(uint)maps.size();
		save_func(&map_count,sizeof(map_count));
		for(MapVecIt it_=maps.begin(),end_=maps.end();it_!=end_;++it_)
		{
			Map* map=*it_;
			save_func(&map->Data,sizeof(map->Data));
		}
	}
}

bool MapManager::LoadAllLocationsAndMapsFile(FILE* f)
{
	WriteLog("Load locations...\n");

	lastLocId=0;
	lastMapId=0;

	uint count;
	fread(&count,sizeof(count),1,f);
	if(!count)
	{
		WriteLog("Locations not found.\n");
		return true;
	}

	uint locaded=0;
	for(uint i=0;i<count;++i)
	{
		// Read data
		Location::LocData data;
		fread(&data,sizeof(data),1,f);
		if(data.LocId>lastLocId) lastLocId=data.LocId;

		uint map_count;
		fread(&map_count,sizeof(map_count),1,f);
		vector<Map::MapData> map_data(map_count);
		for(uint j=0;j<map_count;j++)
		{
			fread(&map_data[j],sizeof(map_data[j]),1,f);
			if(map_data[j].MapId>lastMapId) lastMapId=map_data[j].MapId;
		}

		// Check pids
		if(!IsInitProtoLocation(data.LocPid))
		{
			WriteLog("Proto location<%u> is not init. Skip location.\n",data.LocPid);
			continue;
		}
		bool map_fail=false;
		for(uint j=0;j<map_count;j++)
		{
			if(!IsInitProtoMap(map_data[j].MapPid))
			{
				WriteLog("Proto map<%u> of proto location<%u> is not init. Skip location.\n",map_data[j].MapPid,data.LocPid);
				map_fail=true;
			}
		}
		if(map_fail) continue;

		// Try create
		Location* loc=CreateLocation(data.LocPid,data.WX,data.WY,data.LocId);
		if(!loc)
		{
			WriteLog("Can't create location, pid<%u>.\n",data.LocPid);
			return false;
		}
		loc->Data=data;

		for(uint j=0;j<map_count;j++)
		{
			Map* map=CreateMap(map_data[j].MapPid,loc,map_data[j].MapId);
			if(!map)
			{
				WriteLog("Can't create map, map pid<%u>, location pid<%u>.\n",map_data[j].MapPid,data.LocPid);
				return false;
			}
			map->Data=map_data[j];
		}

		locaded++;
	}

	WriteLog("Load locations complete, count<%u>.\n",locaded);
	return true;
}

string MapManager::GetLocationsMapsStatistics()
{
	SCOPE_LOCK(mapLocker);

	static string result;
	char str[512];
	sprintf(str,"Locations count: %u\n",allLocations.size());
	result=str;
	sprintf(str,"Maps count: %u\n",allMaps.size());
	result+=str;
	result+="Location Name        Id          Pid  X     Y     Radius Color    Visible GeckVisible GeckCount AutoGarbage ToGarbage\n";
	result+="          Map Name            Id          Pid  Time Rain TbAviable TbOn   Script\n";
	for(LocMapIt it=allLocations.begin(),end=allLocations.end();it!=end;++it)
	{
		Location* loc=(*it).second;
		sprintf(str,"%-20s %-09u   %-4u %-5u %-5u %-6u %08X %-7s %-11s %-9d %-11s %-5s\n",
			loc->Proto->Name.c_str(),loc->Data.LocId,loc->Data.LocPid,loc->Data.WX,loc->Data.WY,loc->Data.Radius,loc->Data.Color,loc->Data.Visible?"true":"false",
			loc->Data.GeckVisible?"true":"false",loc->GeckCount,loc->Data.AutoGarbage?"true":"false",loc->Data.ToGarbage?"true":"false");
		result+=str;

		MapVec& maps=loc->GetMapsNoLock();
		uint map_index=0;
		for(MapVecIt it_=maps.begin(),end_=maps.end();it_!=end_;++it_)
		{
			Map* map=*it_;
			sprintf(str,"     %2u) %-20s %-09u   %-4u %-4d %-4u %-9s %-6s %-50s\n",
				map_index,map->Proto->GetName(),map->GetId(),map->GetPid(),map->GetTime(),map->GetRain(),
				map->Data.IsTurnBasedAviable?"true":"false",map->IsTurnBasedOn?"true":"false",
				map->Data.ScriptId?Script::GetScriptFuncName(map->Data.ScriptId).c_str():"");
			result+=str;
			map_index++;
		}
	}
	return result;
}

void MapManager::RunInitScriptMaps()
{
	MapMap maps=allMaps;
	for(MapMapIt it=maps.begin(),end=maps.end();it!=end;++it)
	{
		Map* map=(*it).second;
		if(map->Data.ScriptId) map->ParseScript(NULL,false);
	}
}

bool MapManager::GenerateWorld(const char* fname, int path_type)
{
	WriteLog("Generate world...\n");

	FileManager fm;
	if(!fm.LoadFile(fname,path_type))
	{
		WriteLog("Load file<%s%s> fail.\n",fm.GetFullPath(fname,path_type));
		return false;
	}

	int count_gen=0;
	char line[2048];
	while(fm.GetLine(line,2048))
	{
		istrstream str(line);

		char c;
		str >> c;
		if(str.fail() || c!='@') continue;

		ushort loc_pid,loc_wx,loc_wy;
		str >> loc_pid >> loc_wx >> loc_wy;
		if(str.fail())
		{
			WriteLog("Error, invalid data.\n");
			return false;
		}

		WriteLog("Location: pid<%u>, worldX<%u>, worldY<%u>.\n",loc_pid,loc_wx,loc_wy);

		Location* nloc=CreateLocation(loc_pid,loc_wx,loc_wy,0);
		if(!nloc)
		{
			WriteLog("Error, it was not possible to create a location.\n");
			return false;
		}

		count_gen++;
	}

	if(!count_gen)
	{
		WriteLog("Error, end of a file, but any locations is not created.\n");
		return false;
	}

	WriteLog("Generate world complete, created/generated <%u> locations.\n",count_gen);
	return true;
}

void MapManager::GetLocationAndMapIds(UIntSet& loc_ids, UIntSet& map_ids)
{
	SCOPE_LOCK(mapLocker);

	for(LocMapIt it=allLocations.begin(),end=allLocations.end();it!=end;++it)
		loc_ids.insert((*it).second->GetId());
	for(MapMapIt it=allMaps.begin(),end=allMaps.end();it!=end;++it)
		map_ids.insert((*it).second->GetId());
}

bool MapManager::IsInitProtoLocation(ushort pid_loc)
{
	if(!pid_loc || pid_loc>=MAX_PROTO_LOCATIONS) return false;
	return ProtoLoc[pid_loc].IsInit;
}

ProtoLocation* MapManager::GetProtoLocation(ushort loc_pid)
{
	if(!IsInitProtoLocation(loc_pid)) return NULL;
	return &ProtoLoc[loc_pid];
}

Location* MapManager::CreateLocation(ushort pid_loc, ushort wx, ushort wy, uint loc_id)
{
	if(!IsInitProtoLocation(pid_loc))
	{
		WriteLogF(_FUNC_," - Location proto is not init, pid<%u>.\n",pid_loc);
		return NULL;
	}

	if(!wx || !wy || wx>=GM__MAXZONEX*GameOpt.GlobalMapZoneLength || wy>=GM__MAXZONEY*GameOpt.GlobalMapZoneLength)
	{
		WriteLogF(_FUNC_," - Invalid location coordinates, pid<%u>.\n",pid_loc);
		return NULL;
	}

	Location* loc=new Location();
	if(!loc || !loc->Init(&ProtoLoc[pid_loc],wx,wy))
	{
		WriteLogF(_FUNC_," - Location init fail, pid<%u>.\n",pid_loc);
		loc->Release();
		return NULL;
	}

	if(!loc_id)
	{
		mapLocker.Lock();
		loc->SetId(lastLocId+1);
		lastLocId++;
		mapLocker.Unlock();

		for(uint i=0;i<loc->Proto->ProtoMapPids.size();++i)
		{
			ushort map_pid=loc->Proto->ProtoMapPids[i];

			Map* map=CreateMap(map_pid,loc,0);
			if(!map)
			{
				WriteLogF(_FUNC_," - Create map fail, pid<%u>.\n",map_pid);
				loc->Clear(true);
				loc->Release();
				return NULL;
			}
		}
	}
	else
	{
		SCOPE_LOCK(mapLocker);

		if(allLocations.count(loc_id))
		{
			WriteLogF(_FUNC_," - Location id<%u> is busy.\n",loc_id);
			loc->Release();
			return NULL;
		}
		loc->SetId(loc_id);
	}

	SYNC_LOCK(loc);

	mapLocker.Lock();
	allLocations.insert(LocMapVal(loc->GetId(),loc));
	mapLocker.Unlock();

	// Generate location maps
	MapVec maps=loc->GetMapsNoLock(); // Already locked
	for(MapVecIt it=maps.begin(),end=maps.end();it!=end;++it)
	{
		Map* map=*it;
		if(!map->Generate())
		{
			WriteLogF(_FUNC_," - Generate map fail.\n");
			loc->Data.ToGarbage=true;
			MapMngr.RunGarbager();
			return NULL;
		}
	}

	return loc;
}

bool MapManager::IsInitProtoMap(ushort pid_map)
{
	if(pid_map>=MAX_PROTO_MAPS) return false;
	return ProtoMaps[pid_map].IsInit();
}

Map* MapManager::CreateMap(ushort pid_map, Location* loc_map, uint map_id)
{
	if(!loc_map) return NULL;

	if(!IsInitProtoMap(pid_map))
	{
		WriteLogF(_FUNC_," - Proto map<%u> is not init.\n",pid_map);
		return NULL;
	}

	Map* map=new Map();
	if(!map || !map->Init(&ProtoMaps[pid_map],loc_map))
	{
		WriteLogF(_FUNC_," - Map init fail, pid<%u>.\n",pid_map);
		delete map;
		return NULL;
	}

	if(!map_id)
	{
		mapLocker.Lock();
		map->SetId(lastMapId+1,pid_map);
		lastMapId++;
		loc_map->GetMapsNoLock().push_back(map);
		mapLocker.Unlock();
	}
	else
	{
		SCOPE_LOCK(mapLocker);

		if(allMaps.count(map_id))
		{
			WriteLogF(_FUNC_," - Map already created, id<%u>.\n",map_id);
			delete map;
			return NULL;
		}

		map->SetId(map_id,pid_map);
		loc_map->GetMapsNoLock().push_back(map);
	}

	SYNC_LOCK(map);
	Job::PushBack(JOB_MAP,map);

	mapLocker.Lock();
	allMaps.insert(MapMapVal(map->GetId(),map));
	mapLocker.Unlock();

	return map;
}

Map* MapManager::GetMap(uint map_id, bool sync_lock)
{
	if(!map_id) return NULL;

	Map* map=NULL;

	mapLocker.Lock();
	MapMapIt it=allMaps.find(map_id);
	if(it!=allMaps.end()) map=(*it).second;
	mapLocker.Unlock();

	if(map && sync_lock) SYNC_LOCK(map);
	return map;
}

Map* MapManager::GetMapByPid(ushort map_pid, uint skip_count)
{
	if(!map_pid || map_pid>=MAX_PROTO_MAPS) return NULL;

	mapLocker.Lock();
	for(MapMapIt it=allMaps.begin(),end=allMaps.end();it!=end;++it)
	{
		Map* map=(*it).second;
		if(map->GetPid()==map_pid)
		{
			if(!skip_count)
			{
				mapLocker.Unlock();

				SYNC_LOCK(map);
				return map;
			}
			else skip_count--;
		}
	}
	mapLocker.Unlock();
	return NULL;
}

void MapManager::GetMaps(MapVec& maps, bool lock)
{
	SCOPE_LOCK(mapLocker);

	maps.reserve(allMaps.size());
	for(MapMapIt it=allMaps.begin(),end=allMaps.end();it!=end;++it) maps.push_back((*it).second);

	if(lock) for(MapVecIt it=maps.begin(),end=maps.end();it!=end;++it) SYNC_LOCK(*it);
}

uint MapManager::GetMapsCount()
{
	SCOPE_LOCK(mapLocker);
	uint count=(uint)allMaps.size();
	return count;
}

ProtoMap* MapManager::GetProtoMap(ushort pid_map)
{
	if(!IsInitProtoMap(pid_map)) return NULL;
	return &ProtoMaps[pid_map];
}

bool MapManager::IsProtoMapNoLogOut(ushort pid_map)
{
	ProtoMap* pmap=GetProtoMap(pid_map);
	return pmap?pmap->Header.NoLogOut:false;
}

Location* MapManager::GetLocationByMap(uint map_id)
{
	Map* map=GetMap(map_id);
	if(!map) return NULL;
	return map->GetLocation(true);
}

Location* MapManager::GetLocation(uint loc_id)
{
	if(!loc_id) return NULL;

	mapLocker.Lock();
	LocMapIt it=allLocations.find(loc_id);
	if(it==allLocations.end())
	{
		mapLocker.Unlock();
		return NULL;
	}
	Location* loc=(*it).second;
	mapLocker.Unlock();

	SYNC_LOCK(loc);
	return loc;
}

Location* MapManager::GetLocationByPid(ushort loc_pid, uint skip_count)
{
	ProtoLocation* ploc=GetProtoLocation(loc_pid);
	if(!ploc) return NULL;

	mapLocker.Lock();
	for(LocMapIt it=allLocations.begin(),end=allLocations.end();it!=end;++it)
	{
		Location* loc=(*it).second;
		if(loc->GetPid()==loc_pid)
		{
			if(!skip_count)
			{
				mapLocker.Unlock();

				SYNC_LOCK(loc);
				return loc;
			}
			else skip_count--;
		}
	}
	mapLocker.Unlock();
	return NULL;
}

bool MapManager::IsIntersectZone(int wx1, int wy1, int w1_radius, int wx2, int wy2, int w2_radius, int zones)
{
	int zl=GM_ZONE_LEN;
	INTRECT r1((wx1-w1_radius)/zl-zones,(wy1-w1_radius)/zl-zones,(wx1+w1_radius)/zl+zones,(wy1+w1_radius)/zl+zones);
	INTRECT r2((wx2-w2_radius)/zl,(wy2-w2_radius)/zl,(wx2+w2_radius)/zl,(wy2+w2_radius)/zl);
	return r1.L<=r2.R && r2.L<=r1.R && r1.T<=r2.B && r2.T<=r1.B;
}

void MapManager::GetZoneLocations(int zx, int zy, int zone_radius, UIntVec& loc_ids)
{
	SCOPE_LOCK(mapLocker);

	int wx=zx*GM_ZONE_LEN;
	int wy=zy*GM_ZONE_LEN;
	for(LocMapIt it=allLocations.begin(),end=allLocations.end();it!=end;++it)
	{
		Location* loc=(*it).second;
		if(loc->IsVisible() && IsIntersectZone(wx,wy,0,loc->Data.WX,loc->Data.WY,loc->GetRadius(),zone_radius)) loc_ids.push_back(loc->GetId());
	}
}

void MapManager::GetLocations(LocVec& locs, bool lock)
{
	SCOPE_LOCK(mapLocker);

	locs.reserve(allLocations.size());
	for(LocMapIt it=allLocations.begin(),end=allLocations.end();it!=end;++it) locs.push_back((*it).second);

	if(lock) for(LocVecIt it=locs.begin(),end=locs.end();it!=end;++it) SYNC_LOCK(*it);
}

uint MapManager::GetLocationsCount()
{
	SCOPE_LOCK(mapLocker);
	uint count=(uint)allLocations.size();
	return count;
}

void MapManager::LocationGarbager()
{
	if(runGarbager)
	{
		ClVec players;
		CrMngr.GetCopyPlayers(players,true);

		mapLocker.Lock();
		LocMap locs=allLocations;
		mapLocker.Unlock();

		for(LocMapIt it=locs.begin(),end=locs.end();it!=end;++it)
		{
			Location* loc=(*it).second;
			if(loc->Data.ToGarbage || (loc->Data.AutoGarbage && loc->IsCanDelete()))
			{
				SYNC_LOCK(loc);
				loc->IsNotValid=true;

				// Send all active clients about this
				for(ClVecIt it_=players.begin(),end_=players.end();it_!=end_;++it_)
				{
					Client* cl=*it_;
					if(!cl->GetMap() && cl->CheckKnownLocById(loc->GetId())) cl->Send_GlobalLocation(loc,false);
				}

				// Delete
				mapLocker.Lock();
				LocMapIt it=allLocations.find(loc->GetId());
				if(it!=allLocations.end()) allLocations.erase(it);
				mapLocker.Unlock();

				// Delete maps
				MapVec maps;
				loc->GetMaps(maps,true);
				for(MapVecIt it=maps.begin(),end=maps.end();it!=end;++it)
				{
					Map* map=*it;

					// Transit players to global map
					map->KickPlayersToGlobalMap();

					// Delete from main array
					mapLocker.Lock();
					allMaps.erase(map->GetId());
					mapLocker.Unlock();
				}

				loc->Clear(true);
				Job::DeferredRelease(loc);
			}
		}
		runGarbager=false;
	}
}

void MapManager::GM_GroupMove(GlobalMapGroup* group)
{
	uint tick=Timer::GameTick();
	Critter* rule=group->Rule;

	// Encounter
	if(group->EncounterDescriptor)
	{
		if(tick<group->EncounterTick) return;

		// Force invite
		if(group->EncounterForce)
		{
			group->SyncLockGroup();
			GM_GlobalInvite(group,rule->Data.Params[MODE_DEFAULT_COMBAT]);
			return;
		}

		// Continue walk
		group->EncounterDescriptor=0;
		rule->SendA_GlobalInfo(group,GM_INFO_GROUP_PARAM);
		group->ProcessLastTick=tick;
	}

	// Start callback
	if(!group->IsSetMove && tick>=group->TimeCanFollow)
	{
		group->SyncLockGroup();
		group->IsSetMove=true;
		GM_GlobalProcess(rule,group,GLOBAL_PROCESS_START);
		return;
	}

	// Move
	if(group->IsMoving())
	{
		uint dtime=tick-group->ProcessLastTick;
		if(dtime>=GameOpt.GlobalMapMoveTime)
		{
			if(dtime>=GameOpt.GlobalMapMoveTime*2) dtime=0;
			group->ProcessLastTick=tick-dtime%GameOpt.GlobalMapMoveTime;

			group->SyncLockGroup();

			if(rule->IsPlayer() && ((Client*)rule)->IsOffline())
			{
				group->Stop();
				GM_GlobalProcess(rule,group,GLOBAL_PROCESS_STOPPED);
				return;
			}

			GM_GlobalProcess(rule,group,GLOBAL_PROCESS_MOVE);
		}
	}
}

void MapManager::GM_GlobalProcess(Critter* cr, GlobalMapGroup* group, int type)
{
	static THREAD int recursion_depth=0;
	if(++recursion_depth>100)
	{
		WriteLogF(_FUNC_," - Recursion depth is greater than 100, abort. Critter<%s>.\n",cr->GetInfo());
		recursion_depth--;
		return;
	}

	// Catchers
	uint encounter_descriptor=0;
	bool wait_for_answer=false;
	Critter* rule=group->Rule;
	float cur_wx=group->CurX;
	float cur_wy=group->CurY;
	float to_wx=group->ToX;
	float to_wy=group->ToY;
	float speed=group->Speed;
	float base_speed=group->Speed;
	bool global_process=true;

	if(cr->FuncId[CRITTER_EVENT_GLOBAL_PROCESS]>0)
	{
		if(cr->EventGlobalProcess(type,group->GetCar(),cur_wx,cur_wy,to_wx,to_wy,speed,encounter_descriptor,wait_for_answer))
		{
			global_process=false;
		}
		else
		{
			encounter_descriptor=0;
			wait_for_answer=false;
			cur_wx=group->CurX;
			cur_wy=group->CurY;
			to_wx=group->ToX;
			to_wy=group->ToY;
			speed=group->Speed;
		}
	}

	if(!group->IsValid())
	{
		recursion_depth--;
		return;
	}

	if(global_process && Script::PrepareContext(ServerFunctions.GlobalProcess,_FUNC_,rule->GetInfo()))
	{
		Script::SetArgUInt(type);
		Script::SetArgObject(cr);
		Script::SetArgObject(group->GetCar());
		Script::SetArgAddress(&cur_wx);
		Script::SetArgAddress(&cur_wy);
		Script::SetArgAddress(&to_wx);
		Script::SetArgAddress(&to_wy);
		Script::SetArgAddress(&speed);
		Script::SetArgAddress(&encounter_descriptor);
		Script::SetArgAddress(&wait_for_answer);
		Script::RunPrepared();
	}

	if(!group->IsValid())
	{
		recursion_depth--;
		return;
	}

	// Check ranges
	float max_wx=(float)GM_MAXX;
	float max_wy=(float)GM_MAXY;
	if(cur_wx<0.0f) cur_wx=0.0f;
	if(cur_wy<0.0f) cur_wy=0.0f;
	if(cur_wx>=max_wx) cur_wx=max_wx-1;
	if(cur_wy>=max_wy) cur_wy=max_wy-1;
	if(to_wx<0.0f) to_wx=0.0f;
	if(to_wy<0.0f) to_wy=0.0f;
	if(to_wx>=max_wx) to_wx=max_wx-1;
	if(to_wy>=max_wy) to_wy=max_wy-1;
	if(speed<0.0f) speed=0.0f;
	if(cur_wx==to_wx && cur_wy==to_wy) speed=0.0f;
	if(speed==0.0f) to_wx=cur_wx,to_wy=cur_wy;

	// Current position
	if(cur_wx!=group->CurX || cur_wy!=group->CurY || speed!=group->Speed)
	{
		group->CurX=cur_wx;
		group->CurY=cur_wy;
		group->Speed=speed;

		int cur_wxi=(int)cur_wx;
		int cur_wyi=(int)cur_wy;
		for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it)
		{
			Critter* cr=*it;
			if(cur_wxi!=cr->Data.WorldX || cur_wyi!=cr->Data.WorldY)
			{
				cr->Data.WorldX=cur_wxi;
				cr->Data.WorldY=cur_wyi;
				cr->Send_GlobalInfo(GM_INFO_GROUP_PARAM);
			}
		}
	}

	// New target
	if(type==GLOBAL_PROCESS_SET_MOVE || to_wx!=group->ToX || to_wy!=group->ToY)
	{
		GM_GroupSetMove(group,to_wx,to_wy,speed);
		recursion_depth--;
		return;
	}

	// Stop
	if(base_speed>0.0f && speed<=0.0f)
	{
		group->Stop();
		GM_GlobalProcess(rule,group,GLOBAL_PROCESS_STOPPED);
		recursion_depth--;
		return;
	}
	if(type==GLOBAL_PROCESS_STOPPED) cr->SendA_GlobalInfo(group,GM_INFO_GROUP_PARAM);

	// Encounter
	if(encounter_descriptor)
	{
		group->EncounterDescriptor=encounter_descriptor;
		if(type==GLOBAL_PROCESS_ENTER)
		{
			GM_GlobalInvite(group,rule->Data.Params[MODE_DEFAULT_COMBAT]);
		}
		else
		{
			if(wait_for_answer)
			{
				group->EncounterTick=Timer::GameTick()+GM_ANSWER_WAIT_TIME;
				group->EncounterForce=false;
			}
			else
			{
				group->EncounterTick=Timer::GameTick()+GM_LIGHT_TIME;
				group->EncounterForce=true;
			}

			rule->SendA_GlobalInfo(group,GM_INFO_GROUP_PARAM);
		}
	}

	recursion_depth--;
}

void MapManager::GM_GlobalInvite(GlobalMapGroup* group, int combat_mode)
{
	uint encounter_descriptor=group->EncounterDescriptor;
	group->EncounterDescriptor=0;
	uint map_id=0;
	ushort hx=0,hy=0;
	uchar dir=0;
	Critter* rule=group->Rule;
	bool global_invite=true;

	if(rule->FuncId[CRITTER_EVENT_GLOBAL_INVITE]>0)
	{
		if(rule->EventGlobalInvite(group->GetCar(),encounter_descriptor,combat_mode,map_id,hx,hy,dir))
		{
			global_invite=false;
		}
		else
		{
			map_id=0;
			hx=0;
			hy=0;
			dir=0;
		}
	}

	if(global_invite && Script::PrepareContext(ServerFunctions.GlobalInvite,_FUNC_,rule->GetInfo()))
	{
		Script::SetArgObject(rule);
		Script::SetArgObject(group->GetCar());
		Script::SetArgUInt(encounter_descriptor);
		Script::SetArgUInt(combat_mode);
		Script::SetArgAddress(&map_id);
		Script::SetArgAddress(&hx);
		Script::SetArgAddress(&hy);
		Script::SetArgAddress(&dir);
		Script::RunPrepared();
	}

	if(!group->IsValid()) return;

	if(map_id)
	{
		Map* map=GetMap(map_id);
		if(map) GM_GroupToMap(group,map,0,hx,hy,dir<DIRS_COUNT?dir:rule->GetDir());
	}
	else
	{
		rule->SendA_GlobalInfo(group,GM_INFO_GROUP_PARAM);
	}
}

bool MapManager::GM_CheckEntrance(Location* loc, CScriptArray* arr, uchar entrance)
{
	if(!loc->Proto->ScriptBindId) return true;

	if(Script::PrepareContext(loc->Proto->ScriptBindId,_FUNC_,(*(Critter**)arr->At(0))->GetInfo()))
	{
		Script::SetArgObject(loc);
		Script::SetArgObject(arr);
		Script::SetArgUChar(entrance);
		if(Script::RunPrepared()) return Script::GetReturnedBool();
	}
	return false;
}

CScriptArray* MapManager::GM_CreateGroupArray(GlobalMapGroup* group)
{
	CScriptArray* arr=Script::CreateArray("Critter@[]");
	if(!arr)
	{
		WriteLogF(_FUNC_," - Create script array fail.\n");
		return NULL;
	}

	group->SyncLockGroup();

	arr->Resize((asUINT)group->CritMove.size());
	Critter** p_=(Critter**)arr->At(0);
	*p_=group->Rule;
	group->Rule->AddRef();
	int ind=1;
	for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr==group->Rule) continue;
		Critter** p=(Critter**)arr->At(ind);
		if(!p)
		{
			WriteLogF(_FUNC_," - Critical bug, rule critter<%s>, not valid<%d>.\n",group->Rule->GetInfo(),group->Rule->IsNotValid);
			return NULL;
		}
		*p=cr;
		cr->AddRef();
		ind++;
	}
	return arr;
}

void MapManager::GM_GroupStartMove(Critter* cr)
{
	if(cr->GetMap())
	{
		WriteLogF(_FUNC_," - Critter<%s> is on map.\n",cr->GetInfo());
		TransitToGlobal(cr,0,0,false);
		return;
	}

	cr->Data.HexX=0;
	cr->Data.HexY=0;
	cr->GroupMove=cr->GroupSelf;
	GlobalMapGroup* group=cr->GroupMove;
	group->Clear();
	group->CurX=(float)cr->Data.WorldX;
	group->CurY=(float)cr->Data.WorldY;
	group->ToX=group->CurX;
	group->ToY=group->CurY;
	group->Speed=0.0f;
	cr->Data.GlobalGroupUid++;

	Item* car=cr->GetItemCar();
	if(car) group->CarId=car->GetId();

	group->TimeCanFollow=Timer::GameTick()+TIME_CAN_FOLLOW_GM;
	SETFLAG(cr->Flags,FCRIT_RULEGROUP);

	group->AddCrit(cr);
	cr->Send_GlobalInfo(GM_INFO_ALL);
	GM_GlobalProcess(cr,group,GLOBAL_PROCESS_START_FAST);
}

void MapManager::GM_AddCritToGroup(Critter* cr, uint rule_id)
{
	if(!cr)
	{
		WriteLogF(_FUNC_," - Critter null ptr.");
		return;
	}

	if(!rule_id)
	{
		GM_GroupStartMove(cr);
		return;
	}

	Critter* rule=CrMngr.GetCritter(rule_id,true);
	if(!rule || rule->GetMap() || !rule->GroupMove || rule!=rule->GroupMove->Rule)
	{
		if(cr->IsNpc()) WriteLogF(_FUNC_," - Invalid rule on global map. Start move alone.\n");
		GM_GroupStartMove(cr);
		return;
	}

	GlobalMapGroup* group=rule->GroupMove;
	group->SyncLockGroup();

	UNSETFLAG(cr->Flags,FCRIT_RULEGROUP);
	cr->Data.WorldX=(uint)group->CurX;
	cr->Data.WorldY=(uint)group->CurY;
	cr->Data.HexX=(rule_id>>16)&0xFFFF;
	cr->Data.HexY=rule_id&0xFFFF;

	for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it) (*it)->Send_AddCritter(cr);
	group->AddCrit(cr);
	cr->GroupMove=group;
	cr->Data.GlobalGroupUid=rule->Data.GlobalGroupUid;
}

void MapManager::GM_LeaveGroup(Critter* cr)
{
	if(cr->GetMap() || !cr->GroupMove || cr->GroupMove->GetSize()<2) return;

	GlobalMapGroup* group=cr->GroupMove;
	group->SyncLockGroup();

	if(cr!=cr->GroupMove->Rule)
	{
		group->EraseCrit(cr);
		for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it) (*it)->Send_RemoveCritter(cr);

		Item* car=cr->GetItemCar();
		if(car && car->GetId()==group->CarId)
		{
			group->CarId=0;
			MapMngr.GM_GroupSetMove(group,group->ToX,group->ToY,0.0f);
		}

		GM_GroupStartMove(cr);
	}
	else
	{
		// Give rule to critter with highest charisma
		Critter* new_rule=NULL;
		int max_charisma=0;
		for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it)
		{
			Critter* cr=*it;
			if(cr==group->Rule) continue;
			int charisma=cr->GetParam(ST_CHARISMA);
			if(!max_charisma || charisma>max_charisma)
			{
				new_rule=cr;
				max_charisma=charisma;
			}
		}
		GM_GiveRule(cr,new_rule);

		// Call again
		if(cr!=cr->GroupMove->Rule) GM_LeaveGroup(cr);
	}
}

void MapManager::GM_GiveRule(Critter* cr, Critter* new_rule)
{
	if(cr->GetMap() || !cr->GroupMove || cr->GroupMove->GetSize()<2 || cr->GroupMove->Rule!=cr || cr==new_rule) return;

	cr->GroupSelf->SyncLockGroup();

	*new_rule->GroupSelf=*cr->GroupSelf;
	new_rule->GroupSelf->Rule=new_rule;
	cr->GroupSelf->Clear();
	new_rule->GroupMove=new_rule->GroupSelf;
	UNSETFLAG(cr->Flags,FCRIT_RULEGROUP);
	SETFLAG(new_rule->Flags,FCRIT_RULEGROUP);

	for(CrVecIt it=new_rule->GroupSelf->CritMove.begin(),end=new_rule->GroupSelf->CritMove.end();it!=end;++it)
	{
		Critter* cr_=*it;
		cr_->GroupMove=new_rule->GroupSelf;
		cr_->Send_CritterParam(cr,OTHER_FLAGS,cr->Flags);
		cr_->Send_CritterParam(new_rule,OTHER_FLAGS,new_rule->Flags);
	}
}

void MapManager::GM_StopGroup(Critter* cr)
{
	if(cr->GetMap() || !cr->GroupMove) return;

	cr->GroupMove->ToX=cr->GroupMove->CurX;
	cr->GroupMove->ToY=cr->GroupMove->CurY;
	cr->GroupMove->Speed=0.0f;

	GM_GlobalProcess(cr,cr->GroupMove,GLOBAL_PROCESS_STOPPED);
}

bool MapManager::GM_GroupToMap(GlobalMapGroup* group, Map* map, uint entire, ushort mx, ushort my, uchar mdir)
{
	if(!map || !map->GetId())
	{
		WriteLogF(_FUNC_," - Map null ptr or zero id, pointer<%p>.\n",map);
		return false;
	}

	Critter* rule=group->Rule;
	ushort hx,hy;
	uchar dir;
	ushort car_hx,car_hy;
	Item* car=group->GetCar();
	Critter* car_owner=NULL;

	if(car)
	{
		car_owner=group->GetCritter(car->ACC_CRITTER.Id);
		if(!car_owner)
		{
			WriteLogF(_FUNC_," - Car owner not found, rule<%s>.\n",rule->GetInfo());
			car=NULL;
		}
	}

	if(mx>=map->GetMaxHexX() || my>=map->GetMaxHexY() || mdir>=DIRS_COUNT)
	{
		if(car)
		{
			if(!map->GetStartCoordCar(car_hx,car_hy,car->Proto))
			{
				rule->Send_TextMsg(rule,STR_GLOBAL_CAR_PLACE_NOT_FOUND,SAY_NETMSG,TEXTMSG_GAME);
				return false;
			}
			hx=car_hx;
			hy=car_hy;
			if(mdir<DIRS_COUNT) dir=mdir;
		}
		else
		{
			if(!map->GetStartCoord(hx,hy,dir,entire))
			{
				rule->Send_TextMsg(rule,STR_GLOBAL_PLACE_NOT_FOUND,SAY_NETMSG,TEXTMSG_GAME);
				return false;
			}
		}
	}
	else
	{
		hx=mx;
		hy=my;
		dir=mdir;
		if(car && !map->GetStartCoordCar(car_hx,car_hy,car->Proto))
		{
			rule->Send_TextMsg(rule,STR_GLOBAL_CAR_PLACE_NOT_FOUND,SAY_NETMSG,TEXTMSG_GAME);
			return false;
		}
	}

	// Transit
	group->SyncLockGroup();
	CrVec transit_cr=group->CritMove;

	// Transit rule
	rule->Data.WorldX=(uint)rule->GroupSelf->CurX;
	rule->Data.WorldY=(uint)rule->GroupSelf->CurY;
	if(car) map->SetCritterCar(car_hx,car_hy,car_owner,car);
	if(!Transit(rule,map,hx,hy,dir,car?3:2,true)) return false;

	// Transit other
	for(CrVecIt it=transit_cr.begin(),end=transit_cr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr==rule) continue;

		cr->Data.WorldX=(uint)group->CurX;
		cr->Data.WorldY=(uint)group->CurY;

		if(!Transit(cr,map,hx,hy,dir,car?3:2,true))
		{
			GM_GroupStartMove(cr);
			continue;
		}
	}

	group->Clear();
	return true;
}

bool MapManager::GM_GroupToLoc(Critter* rule, uint loc_id, uchar entrance, bool force /* = false */)
{
	if(rule->GetMap()) return false;
	if(!rule->GroupMove) return false;
	if(rule->IsPlayer() && ((Client*)rule)->IsOffline()) return false; // Offline on encounter
	if(!loc_id) return false;

	if(rule!=rule->GroupMove->Rule)
	{
		WriteLogF(_FUNC_," - Critter<%s> is not rule.\n",rule->GetInfo());
		return false;
	}

	if(!force && rule->IsPlayer() && !((Client*)rule)->CheckKnownLocById(loc_id))
	{
		WriteLogF(_FUNC_," - Critter<%s> is not known location.\n",rule->GetInfo());
		return false;
	}

	Location* loc=GetLocation(loc_id);
	if(!loc)
	{
		if(rule->IsPlayer()) ((Client*)rule)->EraseKnownLoc(loc_id);
		rule->Send_GlobalInfo(GM_INFO_LOCATIONS);
		rule->Send_TextMsg(rule,STR_GLOBAL_LOCATION_NOT_FOUND,SAY_NETMSG,TEXTMSG_GAME);
		return false;
	}

	if(!force && DistSqrt((uint)rule->GroupSelf->CurX,(uint)rule->GroupSelf->CurY,loc->Data.WX,loc->Data.WY)>loc->GetRadius())
	{
		if(rule->IsPlayer()) rule->Send_GlobalLocation(loc,true);
		rule->Send_TextMsg(rule,STR_GLOBAL_LOCATION_REMOVED,SAY_NETMSG,TEXTMSG_GAME);
		return false;
	}

	if(!force && !loc->IsCanEnter(rule->GroupSelf->GetSize()))
	{
		rule->Send_TextMsg(rule,STR_GLOBAL_PLAYERS_OVERFLOW,SAY_NETMSG,TEXTMSG_GAME);
		return false;
	}

	if(!loc->GetMapsCount())
	{
		if(rule->IsPlayer()) ((Client*)rule)->EraseKnownLoc(loc_id);
		WriteLogF(_FUNC_," - Location is empty, critter<%s>.\n",rule->GetInfo());
		return false;
	}

	if(entrance>=loc->Proto->Entrance.size())
	{
		WriteLogF(_FUNC_," - Invalid entrance, critter<%s>.\n",rule->GetInfo());
		return false;
	}

	if(loc->Proto->ScriptBindId)
	{
		CScriptArray* arr=GM_CreateGroupArray(rule->GroupMove);
		if(!arr) return false;
		bool result=GM_CheckEntrance(loc,arr,entrance);
		arr->Release();
		if(!result)
		{
			WriteLogF(_FUNC_," - Can't enter in entrance, critter<%s>.\n",rule->GetInfo());
			return false;
		}
	}

	uint count=loc->Proto->Entrance[entrance].first;
	uint entire=loc->Proto->Entrance[entrance].second;

	Map* map=loc->GetMap(count);
	if(!map)
	{
		if(rule->IsPlayer()) ((Client*)rule)->EraseKnownLoc(loc_id);
		WriteLogF(_FUNC_," - Map not found in location, critter<%s>.\n",rule->GetInfo());
		return false;
	}

	return GM_GroupToMap(rule->GroupMove,map,entire,-1,-1,-1);
}

void MapManager::GM_GroupSetMove(GlobalMapGroup* group, float to_x, float to_y, float speed)
{
	int dist=DistSqrt((int)to_x,(int)to_y,(int)group->CurX,(int)group->CurY);

	if(speed<=0.0f || !dist)
	{
		group->Speed=0.0f;
		group->ToX=group->CurX;
		group->ToY=group->CurY;
	}
	else
	{
		group->Speed=speed;
		group->ToX=to_x;
		group->ToY=to_y;
	}

	group->Rule->SendA_GlobalInfo(group,GM_INFO_GROUP_PARAM);
	if(Timer::GameTick()-group->ProcessLastTick>GameOpt.GlobalMapMoveTime) group->ProcessLastTick=Timer::GameTick();
	if(!group->IsSetMove)
	{
		group->IsSetMove=true;
		GM_GlobalProcess(group->Rule,group,GLOBAL_PROCESS_START);
	}
}

void MapManager::TraceBullet(TraceData& trace)
{
	Map* map=trace.TraceMap;
	ushort maxhx=map->GetMaxHexX();
	ushort maxhy=map->GetMaxHexY();
	ushort hx=trace.BeginHx;
	ushort hy=trace.BeginHy;
	ushort tx=trace.EndHx;
	ushort ty=trace.EndHy;

	uint dist=trace.Dist;
	if(!dist) dist=DistGame(hx,hy,tx,ty);

	ushort cx=hx;
	ushort cy=hy;
	ushort old_cx=cx;
	ushort old_cy=cy;
	uchar dir;

	LineTracer line_tracer(hx,hy,tx,ty,maxhx,maxhy,trace.Angle,!GameOpt.MapHexagonal);

	trace.IsFullTrace=false;
	trace.IsCritterFounded=false;
	trace.IsHaveLastPassed=false;
	trace.IsTeammateFounded=false;
	bool last_passed_ok=false;
	for(uint i=0;;i++)
	{
		if(i>=dist)
		{
			trace.IsFullTrace=true;
			break;
		}

		if(GameOpt.MapHexagonal)
		{
			dir=line_tracer.GetNextHex(cx,cy);
		}
		else
		{
			line_tracer.GetNextSquare(cx,cy);
			dir=GetNearDir(old_cx,old_cy,cx,cy);
		}

		if(trace.HexCallback)
		{
			trace.HexCallback(map,trace.FindCr,old_cx,old_cy,cx,cy,dir);
			old_cx=cx;
			old_cy=cy;
			continue;
		}

		if(trace.LastPassed && !last_passed_ok)
		{
			if(map->IsHexPassed(cx,cy))
			{
				(*trace.LastPassed).first=cx;
				(*trace.LastPassed).second=cy;
				trace.IsHaveLastPassed=true;
			}
			else if(!map->IsHexCritter(cx,cy) || !trace.LastPassedSkipCritters) last_passed_ok=true;
		}

		if(!map->IsHexRaked(cx,cy)) break;
		if(trace.Critters!=NULL && map->IsHexCritter(cx,cy)) map->GetCrittersHex(cx,cy,0,trace.FindType,*trace.Critters,false);
		if((trace.FindCr || trace.IsCheckTeam) && map->IsFlagCritter(cx,cy,false))
		{
			Critter* cr=map->GetHexCritter(cx,cy,false,false);
			if(cr)
			{
				if(cr==trace.FindCr)
				{
					trace.IsCritterFounded=true;
					break;
				}
				if(trace.IsCheckTeam && cr->Data.Params[ST_TEAM_ID]==(int)trace.BaseCrTeamId)
				{
					trace.IsTeammateFounded=true;
					break;
				}
			}
		}

		old_cx=cx;
		old_cy=cy;
	}

	if(trace.PreBlock)
	{
		(*trace.PreBlock).first=old_cx;
		(*trace.PreBlock).second=old_cy;
	}
	if(trace.Block)
	{
		(*trace.Block).first=cx;
		(*trace.Block).second=cy;
	}
}

int THREAD MapGridOffsX=0;
int THREAD MapGridOffsY=0;
static THREAD short* Grid=NULL;
#define GRID(x,y) Grid[((FPATH_MAX_PATH+1)+(y)-MapGridOffsY)*(FPATH_MAX_PATH*2+2)+((FPATH_MAX_PATH+1)+(x)-MapGridOffsX)]
int MapManager::FindPath(PathFindData& pfd)
{
	// Allocate temporary grid
	if(!Grid)
	{
		Grid=new(nothrow) short[(FPATH_MAX_PATH*2+2)*(FPATH_MAX_PATH*2+2)];
		if(!Grid) return FPATH_ALLOC_FAIL;
	}

	// Data
	uint map_id=pfd.MapId;
	ushort from_hx=pfd.FromX;
	ushort from_hy=pfd.FromY;
	ushort to_hx=pfd.ToX;
	ushort to_hy=pfd.ToY;
	uint multihex=pfd.Multihex;
	uint cut=pfd.Cut;
	uint trace=pfd.Trace;
	bool is_run=pfd.IsRun;
	bool check_cr=pfd.CheckCrit;
	bool check_gag_items=pfd.CheckGagItems;
	int dirs_count=DIRS_COUNT;

	// Checks
	if(trace && !pfd.TraceCr) return FPATH_TRACE_TARG_NULL_PTR;

	Map* map=GetMap(map_id);
	if(!map) return FPATH_MAP_NOT_FOUND;
	ushort maxhx=map->GetMaxHexX();
	ushort maxhy=map->GetMaxHexY();

	if(from_hx>=maxhx || from_hy>=maxhy || to_hx>=maxhx || to_hy>=maxhy) return FPATH_INVALID_HEXES;

	if(CheckDist(from_hx,from_hy,to_hx,to_hy,cut)) return FPATH_ALREADY_HERE;
	if(!cut && FLAG(map->GetHexFlags(to_hx,to_hy),FH_NOWAY)) return FPATH_HEX_BUSY;

	// Ring check
	if(cut<=1 && !multihex)
	{
		short* rsx,*rsy;
		GetHexOffsets(to_hx&1,rsx,rsy);

		int i=0;
		for(;i<dirs_count;i++,rsx++,rsy++)
		{
			short xx=to_hx+*rsx;
			short yy=to_hy+*rsy;
			if(xx>=0 && xx<maxhx && yy>=0 && yy<maxhy)
			{
				ushort flags=map->GetHexFlags(xx,yy);
				if(FLAG(flags,FH_GAG_ITEM<<8)) break;
				if(!FLAG(flags,FH_NOWAY)) break;
			}
		}
		if(i==dirs_count) return FPATH_HEX_BUSY_RING;
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
			first_steps.push_back(UShortPairVecVal(hx_,hy_));
		}
	}
	for(int i=0;i<4;i++)
	{

	}*/

	// Prepare
	int numindex=1;
	memzero(Grid,(FPATH_MAX_PATH*2+2)*(FPATH_MAX_PATH*2+2)*sizeof(short));
	MapGridOffsX=from_hx;
	MapGridOffsY=from_hy;
	GRID(from_hx,from_hy)=numindex;

	UShortPairVec coords,cr_coords,gag_coords;
	coords.reserve(10000);
	cr_coords.reserve(100);
	gag_coords.reserve(100);

	// First point
	coords.push_back(UShortPairVecVal(from_hx,from_hy));

	// Begin search
	int p=0,p_togo=1;
	ushort cx,cy;
	while(true)
	{
		for(int i=0;i<p_togo;i++,p++)
		{
			cx=coords[p].first;
			cy=coords[p].second;
			numindex=GRID(cx,cy);

			if(CheckDist(cx,cy,to_hx,to_hy,cut)) goto label_FindOk;
			if(++numindex>FPATH_MAX_PATH) return FPATH_TOOFAR;

			short* sx,*sy;
			GetHexOffsets(cx&1,sx,sy);

			for(int j=0;j<dirs_count;j++)
			{
				short nx=(short)cx+sx[j];
				short ny=(short)cy+sy[j];
				if(nx<0 || ny<0 || nx>=maxhx || ny>=maxhy) continue;

				short& g=GRID(nx,ny);
				if(g) continue;

				if(!multihex)
				{
					ushort flags=map->GetHexFlags(nx,ny);
					if(!FLAG(flags,FH_NOWAY))
					{
						coords.push_back(UShortPairVecVal(nx,ny));
						g=numindex;
					}
					else if(check_gag_items && FLAG(flags,FH_GAG_ITEM<<8))
					{
						gag_coords.push_back(UShortPairVecVal(nx,ny));
						g=numindex|0x4000;
					}
					else if(check_cr && FLAG(flags,FH_CRITTER<<8))
					{
						cr_coords.push_back(UShortPairVecVal(nx,ny));
						g=numindex|0x8000;
					}
					else
					{
						g=-1;
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

					if(map->IsMovePassed(nx,ny,j,multihex))
					{
						coords.push_back(UShortPairVecVal(nx,ny));
						g=numindex;
					}
					else
					{
						g=-1;
					}
				}
			}
		}

		// Add gag hex after some distance
		if(gag_coords.size())
		{
			short last_index=GRID(coords.back().first,coords.back().second);
			UShortPair& xy=gag_coords.front();
			short gag_index=GRID(xy.first,xy.second)^0x4000;
			if(gag_index+10<last_index) // Todo: if path finding not be reworked than migrate magic number to scripts
			{
				GRID(xy.first,xy.second)=gag_index;
				coords.push_back(xy);
				gag_coords.erase(gag_coords.begin());
			}
		}

		// Add gag and critters hexes
		p_togo=(int)coords.size()-p;
		if(!p_togo)
		{
			if(gag_coords.size())
			{
				UShortPair& xy=gag_coords.front();
				GRID(xy.first,xy.second)^=0x4000;
				coords.push_back(xy);
				gag_coords.erase(gag_coords.begin());
				p_togo++;
			}
			else if(cr_coords.size())
			{
				UShortPair& xy=cr_coords.front();
				GRID(xy.first,xy.second)^=0x8000;
				coords.push_back(xy);
				cr_coords.erase(cr_coords.begin());
				p_togo++;
			}
		}

		if(!p_togo) return FPATH_DEADLOCK;
	}

label_FindOk:
	if(++pathNumCur>=FPATH_DATA_SIZE) pathNumCur=1;
	PathStepVec& path=pathesPool[pathNumCur];
	path.resize(numindex-1);

	// Smooth data
	static THREAD bool smooth_switcher=false;
	if(!GameOpt.MapSmoothPath) smooth_switcher=false;

	int smooth_count=0,smooth_iteration=0;
	if(GameOpt.MapSmoothPath && !GameOpt.MapHexagonal)
	{
		int x1=cx,y1=cy;
		int x2=from_hx,y2=from_hy;
		int dx=abs(x1-x2);
		int dy=abs(y1-y2);
		int d=MAX(dx,dy);
		int h1=abs(dx-dy);
		int h2=d-h1;
		if(dy<dx) std::swap(h1,h2);
		smooth_count=((h1 && h2)?h1/h2+1:3);
		if(smooth_count<3) smooth_count=3;

		smooth_count=((h1 && h2)?MAX(h1,h2)/MIN(h1,h2)+1:0);
		if(h1 && h2 && smooth_count<2) smooth_count=2;
		smooth_iteration=((h1 && h2)?MIN(h1,h2)%MAX(h1,h2):0);
	}

	while(numindex>1)
	{
		if(GameOpt.MapSmoothPath)
		{
			if(GameOpt.MapHexagonal)
			{
				if(numindex&1) smooth_switcher=!smooth_switcher;
			}
			else
			{
				smooth_switcher=(smooth_count<2 || smooth_iteration%smooth_count);
			}
		}

		numindex--;
		PathStep& ps=path[numindex-1];
		ps.HexX=cx;
		ps.HexY=cy;
		int dir=FindPathGrid(cx,cy,numindex,smooth_switcher);
		if(dir==-1) return FPATH_ERROR;
		ps.Dir=dir;

		smooth_iteration++;
	}

	// Check for closed door and critter
	if(check_cr || check_gag_items)
	{
		for(int i=0,j=(int)path.size();i<j;i++)
		{
			PathStep& ps=path[i];
			if(map->IsHexPassed(ps.HexX,ps.HexY)) continue;

			if(check_gag_items && map->IsHexGag(ps.HexX,ps.HexY))
			{
				Item* item=map->GetItemGag(ps.HexX,ps.HexY);
				if(!item) continue;
				pfd.GagItem=item;
				path.resize(i);
				break;
			}

			if(check_cr && map->IsFlagCritter(ps.HexX,ps.HexY,false))
			{
				Critter* cr=map->GetHexCritter(ps.HexX,ps.HexY,false,false);
				if(!cr || cr==pfd.FromCritter) continue;
				pfd.GagCritter=cr;
				path.resize(i);
				break;
			}
		}
	}

	// Trace
	if(trace)
	{
		IntVec trace_seq;
		ushort targ_hx=pfd.TraceCr->GetHexX();
		ushort targ_hy=pfd.TraceCr->GetHexY();
		bool trace_ok=false;

		trace_seq.resize(path.size()+4);
		for(int i=0,j=(int)path.size();i<j;i++)
		{
			PathStep& ps=path[i];
			if(map->IsHexGag(ps.HexX,ps.HexY))
			{
				trace_seq[i+2-2]+=1;
				trace_seq[i+2-1]+=2;
				trace_seq[i+2-0]+=3;
				trace_seq[i+2+1]+=2;
				trace_seq[i+2+2]+=1;
			}
		}

		TraceData trace_;
		trace_.TraceMap=map;
		trace_.EndHx=targ_hx;
		trace_.EndHy=targ_hy;
		trace_.FindCr=pfd.TraceCr;
		for(int k=0;k<5;k++)
		{
			for(int i=0,j=(int)path.size();i<j;i++)
			{
				if(k<4 && trace_seq[i+2]!=k) continue;
				if(k==4 && trace_seq[i+2]<4) continue;

				PathStep& ps=path[i];

				if(!CheckDist(ps.HexX,ps.HexY,targ_hx,targ_hy,trace)) continue;

				trace_.BeginHx=ps.HexX;
				trace_.BeginHy=ps.HexY;
				TraceBullet(trace_);
				if(trace_.IsCritterFounded)
				{
					trace_ok=true;
					path.resize(i+1);
					goto label_TraceOk;
				}
			}
		}

		if(!trace_ok && !pfd.GagItem && !pfd.GagCritter) return FPATH_TRACE_FAIL;
label_TraceOk:
		if(trace_ok)
		{
			pfd.GagItem=NULL;
			pfd.GagCritter=NULL;
		}
	}

	// Parse move params
	PathSetMoveParams(path,is_run);

	// Number of path
	if(path.empty()) return FPATH_ALREADY_HERE;
	pfd.PathNum=pathNumCur;

	// New X,Y
	PathStep& ps=path[path.size()-1];
	pfd.NewToX=ps.HexX;
	pfd.NewToY=ps.HexY;
	return FPATH_OK;
}

int MapManager::FindPathGrid(ushort& hx, ushort& hy, int index, bool smooth_switcher)
{
	// Hexagonal
	if(GameOpt.MapHexagonal)
	{
		if(smooth_switcher)
		{
			if(hx&1)
			{
				if(GRID(hx-1,hy-1)==index) { hx--; hy--; return 3; } // 0
				if(GRID(hx  ,hy-1)==index) {       hy--; return 2; } // 5
				if(GRID(hx  ,hy+1)==index) {       hy++; return 5; } // 2
				if(GRID(hx+1,hy  )==index) { hx++;       return 0; } // 3
				if(GRID(hx-1,hy  )==index) { hx--;       return 4; } // 1
				if(GRID(hx+1,hy-1)==index) { hx++; hy--; return 1; } // 4
			}
			else
			{
				if(GRID(hx-1,hy  )==index) { hx--;       return 3; } // 0
				if(GRID(hx  ,hy-1)==index) {       hy--; return 2; } // 5
				if(GRID(hx  ,hy+1)==index) {       hy++; return 5; } // 2
				if(GRID(hx+1,hy+1)==index) { hx++; hy++; return 0; } // 3
				if(GRID(hx-1,hy+1)==index) { hx--; hy++; return 4; } // 1
				if(GRID(hx+1,hy  )==index) { hx++;       return 1; } // 4
			}
		}
		else
		{
			if(hx&1)
			{
				if(GRID(hx-1,hy  )==index) { hx--;       return 4; } // 1
				if(GRID(hx+1,hy-1)==index) { hx++; hy--; return 1; } // 4
				if(GRID(hx  ,hy-1)==index) {       hy--; return 2; } // 5
				if(GRID(hx-1,hy-1)==index) { hx--; hy--; return 3; } // 0
				if(GRID(hx+1,hy  )==index) { hx++;       return 0; } // 3
				if(GRID(hx  ,hy+1)==index) {       hy++; return 5; } // 2
			}
			else
			{
				if(GRID(hx-1,hy+1)==index) { hx--; hy++; return 4; } // 1
				if(GRID(hx+1,hy  )==index) { hx++;       return 1; } // 4
				if(GRID(hx  ,hy-1)==index) {       hy--; return 2; } // 5
				if(GRID(hx-1,hy  )==index) { hx--;       return 3; } // 0
				if(GRID(hx+1,hy+1)==index) { hx++; hy++; return 0; } // 3
				if(GRID(hx  ,hy+1)==index) {       hy++; return 5; } // 2
			}
		}
	}
	// Square
	else
	{
		// Without smoothing
		if(!GameOpt.MapSmoothPath)
		{
			if(GRID(hx-1,hy  )==index) { hx--;       return 0; } // 0
			if(GRID(hx  ,hy-1)==index) {       hy--; return 6; } // 6
			if(GRID(hx  ,hy+1)==index) {       hy++; return 2; } // 2
			if(GRID(hx+1,hy  )==index) { hx++;       return 4; } // 4
			if(GRID(hx-1,hy+1)==index) { hx--; hy++; return 1; } // 1
			if(GRID(hx+1,hy-1)==index) { hx++; hy--; return 5; } // 5
			if(GRID(hx+1,hy+1)==index) { hx++; hy++; return 3; } // 3
			if(GRID(hx-1,hy-1)==index) { hx--; hy--; return 7; } // 7
		}
		// With smoothing
		else
		{
			if(smooth_switcher)
			{
				if(GRID(hx-1,hy  )==index) { hx--;       return 0; } // 0
				if(GRID(hx  ,hy+1)==index) {       hy++; return 2; } // 2
				if(GRID(hx+1,hy  )==index) { hx++;       return 4; } // 4
				if(GRID(hx  ,hy-1)==index) {       hy--; return 6; } // 6
				if(GRID(hx+1,hy+1)==index) { hx++; hy++; return 3; } // 3
				if(GRID(hx-1,hy-1)==index) { hx--; hy--; return 7; } // 7
				if(GRID(hx-1,hy+1)==index) { hx--; hy++; return 1; } // 1
				if(GRID(hx+1,hy-1)==index) { hx++; hy--; return 5; } // 5
			}
			else
			{
				if(GRID(hx+1,hy+1)==index) { hx++; hy++; return 3; } // 3
				if(GRID(hx-1,hy-1)==index) { hx--; hy--; return 7; } // 7
				if(GRID(hx-1,hy  )==index) { hx--;       return 0; } // 0
				if(GRID(hx  ,hy+1)==index) {       hy++; return 2; } // 2
				if(GRID(hx+1,hy  )==index) { hx++;       return 4; } // 4
				if(GRID(hx  ,hy-1)==index) {       hy--; return 6; } // 6
				if(GRID(hx-1,hy+1)==index) { hx--; hy++; return 1; } // 1
				if(GRID(hx+1,hy-1)==index) { hx++; hy--; return 5; } // 5
			}
		}
	}

	return -1;
}

void MapManager::PathSetMoveParams(PathStepVec& path, bool is_run)
{
	uint move_params=0; // Base parameters
	for(int i=(int)path.size()-1;i>=0;i--) // From end to beginning
	{
		PathStep& ps=path[i];

		// Walk flags
		if(is_run) SETFLAG(move_params,MOVE_PARAM_RUN);
		else UNSETFLAG(move_params,MOVE_PARAM_RUN);

		// Store
		ps.MoveParams=move_params;

		// Add dir to sequence
		move_params=(move_params<<MOVE_PARAM_STEP_BITS)|ps.Dir|MOVE_PARAM_STEP_ALLOW;
	}
}

bool MapManager::TryTransitCrGrid(Critter* cr, Map* map, ushort hx, ushort hy, bool force)
{
	if(cr->LockMapTransfers)
	{
		WriteLogF(_FUNC_," - Transfers locked, critter<%s>.\n",cr->GetInfo());
		return false;
	}

	if(!cr->IsPlayer() || !cr->IsLife()) return false;
	if(!map || !FLAG(map->GetHexFlags(hx,hy),FH_SCEN_GRID)) return false;
	if(!force && !map->IsTurnBasedOn && cr->IsTransferTimeouts(true)) return false;

	Location* loc=map->GetLocation(true);
	uint id_map=0;
	uchar dir=0;

	if(!loc->GetTransit(map,id_map,hx,hy,dir)) return false;
	if(loc->IsVisible() && cr->IsPlayer())
	{
		((Client*)cr)->AddKnownLoc(loc->GetId());
		if(loc->IsAutomaps()) cr->Send_AutomapsInfo(NULL,loc);
	}
	cr->SetTimeout(TO_TRANSFER,0);
	cr->SetTimeout(TO_BATTLE,0);

	// To global
	if(!id_map)
	{
		if(TransitToGlobal(cr,0,FOLLOW_PREP,force)) return true;
	}
	// To local
	else
	{
		Map* to_map=MapMngr.GetMap(id_map);
		if(to_map && Transit(cr,to_map,hx,hy,dir,2,force)) return true;
	}

	return false;
}

bool MapManager::TransitToGlobal(Critter* cr, uint rule, uchar follow_type, bool force)
{
	if(cr->LockMapTransfers)
	{
		WriteLogF(_FUNC_," - Transfers locked, critter<%s>.\n",cr->GetInfo());
		return false;
	}

	return Transit(cr,NULL,rule>>16,rule&0xFFFF,follow_type,0,force);
}

bool MapManager::Transit(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint radius, bool force)
{
	// Check location deletion
	Location* loc=(map?map->GetLocation(true):NULL);
	if(loc && loc->Data.ToGarbage)
	{
		WriteLogF(_FUNC_," - Transfer to deleted location, critter<%s>.\n",cr->GetInfo());
		return false;
	}

	// Maybe critter already in transfer
	if(cr->LockMapTransfers)
	{
		WriteLogF(_FUNC_," - Transfers locked, critter<%s>.\n",cr->GetInfo());
		return false;
	}

	// Check force
	if(!force)
	{
		if(cr->GetParam(TO_TRANSFER) || cr->GetParam(TO_BATTLE)) return false;
		if(cr->IsDead()) return false;
		if(cr->IsKnockout()) return false;
		if(loc && !loc->IsCanEnter(1)) return false;
	}

	uint map_id=(map?map->GetId():0);
	uint old_map_id=cr->GetMap();
	Map* old_map=MapMngr.GetMap(old_map_id,true);
	ushort old_hx=cr->GetHexX();
	ushort old_hy=cr->GetHexY();

	// Recheck after synchronization
	if(cr->GetMap()!=old_map_id) return false;

	// One map
	if(old_map_id==map_id)
	{
		// Global
		if(!map_id) return true;

		// Local
		if(!map || hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) return false;

		uint multihex=cr->GetMultihex();
		if(!map->FindStartHex(hx,hy,multihex,radius,true) && !map->FindStartHex(hx,hy,multihex,radius,false)) return false;

		cr->LockMapTransfers++;

		cr->Data.Dir=(dir>=DIRS_COUNT?0:dir);
		bool is_dead=cr->IsDead();
		map->UnsetFlagCritter(old_hx,old_hy,multihex,is_dead);
		cr->Data.HexX=hx;
		cr->Data.HexY=hy;
		map->SetFlagCritter(hx,hy,multihex,is_dead);
		cr->SetBreakTime(0);
		cr->Send_ParamOther(OTHER_TELEPORT,(cr->GetHexX()<<16)|(cr->GetHexY()));
		cr->ClearVisible();
		cr->ProcessVisibleCritters();
		cr->ProcessVisibleItems();
		cr->Send_XY(cr);

		cr->LockMapTransfers--;
		return true;
	}
	// Different maps
	else
	{
		cr->LockMapTransfers++;

		if(!AddCrToMap(cr,map,hx,hy,radius))
		{
			cr->LockMapTransfers--;
			return false;
		}

		// Global
		if(!map)
		{
			// to_ori == follow_type
			if(dir==FOLLOW_PREP) cr->SendA_Follow(dir,0,TIME_CAN_FOLLOW_GM);
		}
		// Local
		else
		{
			cr->Data.Dir=(dir>=DIRS_COUNT?0:dir);
		}

		if(!old_map_id || old_map) EraseCrFromMap(cr,old_map,old_hx,old_hy);

		cr->SetBreakTime(0);
		cr->Send_LoadMap(NULL);

		// Map out / in events
		if(old_map) old_map->EraseCritterEvents(cr);
		if(map) map->AddCritterEvents(cr);

		// Visible critters / items
		cr->DisableSend++;
		cr->ProcessVisibleCritters();
		cr->ProcessVisibleItems();
		cr->DisableSend--;

		cr->LockMapTransfers--;
	}
	return true;
}

bool MapManager::AddCrToMap(Critter* cr, Map* map, ushort tx, ushort ty, uint radius)
{
	// Global map
	if(!map)
	{
		cr->SetMaps(0,0);
		cr->SetTimeout(TO_BATTLE,0);
		cr->SetTimeout(TO_TRANSFER,GameOpt.TimeoutTransfer);

		cr->LockMapTransfers++;
		cr->Data.LastHexX=cr->Data.HexX;
		cr->Data.LastHexY=cr->Data.HexY;
		// tx,ty == rule_id
		uint to_group=(tx<<16)|ty;
		if(!to_group) GM_GroupStartMove(cr);
		else GM_AddCritToGroup(cr,to_group);
		cr->LockMapTransfers--;
	}
	// Local map
	else
	{
		if(tx>=map->GetMaxHexX() || ty>=map->GetMaxHexY()) return false;

		uint multihex=cr->GetMultihex();
		if(!map->FindStartHex(tx,ty,multihex,radius,true) && !map->FindStartHex(tx,ty,multihex,radius,false)) return false;

		cr->LockMapTransfers++;
		cr->SetTimeout(TO_BATTLE,0);
		cr->SetTimeout(TO_TRANSFER,GameOpt.TimeoutTransfer);
		cr->SetMaps(map->GetId(),map->GetPid());
		cr->Data.LastHexX=cr->Data.HexX;
		cr->Data.LastHexY=cr->Data.HexY;
		cr->Data.HexX=tx;
		cr->Data.HexY=ty;
		map->AddCritter(cr);
		cr->LockMapTransfers--;
	}
	return true;
}

void MapManager::EraseCrFromMap(Critter* cr, Map* map, ushort hex_x, ushort hex_y)
{
	// Global map
	if(!map)
	{
		if(cr->GroupMove)
		{
			cr->GroupMove->EraseCrit(cr);
			for(CrVecIt it=cr->GroupMove->CritMove.begin(),end=cr->GroupMove->CritMove.end();it!=end;++it)
			{
				Critter* cr_=*it;
				cr_->Send_RemoveCritter(cr);
				//cr_->EventHideCritter(cr);
			}
			cr->GroupMove=NULL;
		}
	}
	// Local map
	else
	{
		cr->LockMapTransfers++;

		cr->SyncLockCritters(false,false);
		CrVec critters=cr->VisCr;

		for(CrVecIt it=critters.begin(),end=critters.end();it!=end;++it)
		{
			Critter* cr_=*it;
			cr_->EventHideCritter(cr);
		}

		cr->ClearVisible();
		map->EraseCritter(cr);
		map->UnsetFlagCritter(hex_x,hex_y,cr->GetMultihex(),cr->IsDead());

		cr->LockMapTransfers--;
	}
}








