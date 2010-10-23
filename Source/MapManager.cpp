#include "StdAfx.h"
#include "MapManager.h"
#include "Log.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "Script.h"

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
	return SpeedX || SpeedY;
}

DWORD GlobalMapGroup::GetSize()
{
	return CritMove.size();
}

void GlobalMapGroup::SyncLockGroup()
{
	CrVec critters=CritMove;
	for(CrVecIt it=critters.begin(),end=critters.end();it!=end;++it) SYNC_LOCK(*it);
}

Critter* GlobalMapGroup::GetCritter(DWORD crid)
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
	DWORD id=cr->GetId();
	if(std::find(CritMove.begin(),CritMove.end(),cr)==CritMove.end()) CritMove.push_back(cr);
}

void GlobalMapGroup::EraseCrit(Critter* cr)
{
	CrVecIt it=std::find(CritMove.begin(),CritMove.end(),cr);
	if(it!=CritMove.end()) CritMove.erase(it);
}

void GlobalMapGroup::StartEncaunterTime(int time)
{
	NextEncaunter=Timer::GameTick()+time;
}

bool GlobalMapGroup::IsEncaunterTime()
{
	return Timer::GameTick()>=NextEncaunter;
}

void GlobalMapGroup::Clear()
{
	CritMove.clear();
	CarId=0;
	MoveX=0;
	MoveY=0;
	SpeedX=0;
	SpeedY=0;
	WXf=0;
	WYf=0;
	WXi=0;
	WYi=0;

	IsSetMove=false;
	TimeCanFollow=0;
	NextEncaunter=0;
	IsMultiply=true;

	MoveLastTick=Timer::GameTick();

	EncounterDescriptor=0;
	EncounterTick=0;
	EncounterForce=false;
}

/************************************************************************/
/* MapMngr                                                              */
/************************************************************************/

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

	pmapsLoaded.clear();
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
	pmapsLoaded.clear();
	SAFEDEL(gmMask);

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

DwordPair EntranceParser(const char* str)
{
	int val1,val2;
	if(sscanf(str,"%d %d",&val1,&val2)!=2 || val1<0 || val1>0xFF || val2<0 || val2>0xFF) return DwordPair(-1,-1);
	return DwordPair(val1,val2);
}

bool MapManager::LoadLocationsProtos()
{
	WriteLog("Load locations prototypes...\n");

	IniParser city_txt;
	if(!city_txt.LoadFile("Locations.cfg",PT_SERVER_MAPS))
	{
		WriteLog("File<%s> not found.\n",FileManager::GetFullPath("Locations.cfg",PT_SERVER_MAPS));
		return false;
	}

	int errors=0;
	DWORD loaded=0;
	char res[256];
	for(int i=1;i<MAX_PROTO_LOCATIONS;i++)
	{
		ProtoLoc[i].IsInit=false;
		if(!city_txt.GetStr(Str::Format("Area %u",i),"map_0","error",res)) continue;

		ProtoLocation& ploc=ProtoLoc[i];
		if(!LoadLocationProto(city_txt,ploc,i))
		{
			errors++;
			WriteLog("Load location<%u> fail.\n",i);
			continue;
		}
		loaded++;
	}

	if(errors)
	{
		WriteLog("Load locations prototypes fail, errors<%d>.\n",errors);
		return false;
	}

	WriteLog("Load locations prototypes success, loaded<%u> protos.\n",loaded);
	return true;
}

bool MapManager::LoadLocationProto(IniParser& city_txt, ProtoLocation& ploc, WORD pid)
{
	char key1[256];
	char key2[256];
	char res[512];
	sprintf(key1,"Area %u",pid);

	ploc.IsInit=false;
	city_txt.GetStr(key1,"name","",res);
	ploc.Name=res;
	ploc.MaxCopy=city_txt.GetInt(key1,"max_copy",0); // Max copy
	ploc.Radius=city_txt.GetInt(key1,"size",24); // Radius

	// Maps
	ploc.ProtoMapPids.clear();
	int cur_map=0;
	while(true)
	{
		sprintf(key2,"map_%u",cur_map);
		if(!city_txt.GetStr(key1,key2,"",res)) break;
		size_t len=strlen(res);
		if(!len) break;

		bool is_automap=false;
		if(res[len-1]=='*')
		{
			if(len==1) break;
			is_automap=true;
			res[len-1]=0;
		}

		StrWordMapIt it=pmapsLoaded.find(string(res));
		if(it==pmapsLoaded.end())
		{
			WriteLog(__FUNCTION__" - Proto map is not loaded, location pid<%u>, name<%s>.\n",pid,res);
			return false;
		}

		ploc.ProtoMapPids.push_back((*it).second);
		if(is_automap) ploc.AutomapsPids.push_back((*it).second);
		cur_map++;
	}

	if(!cur_map)
	{
		WriteLog(__FUNCTION__" - Not found no one map, location pid<%u>.\n",pid);
		return false;
	}

	// Entrance
	ploc.Entrance.clear();
	city_txt.GetStr(key1,"entrance","1",res);
	if(res[0]=='$')
	{
		Str::ParseLine<DwordPairVec,DwordPair(*)(const char*)>(&res[1],',',ploc.Entrance,EntranceParser);
		for(int k=0,l=ploc.Entrance.size();k<l;k++)
		{
			DWORD map_num=ploc.Entrance[k].first;
			if(map_num>cur_map)
			{
				WriteLog(__FUNCTION__" - Invalid map number<%u>, entrance<%s>, location pid<%u>.\n",map_num,res,pid);
				return false;
			}
		}
	}
	else
	{
		int val=atoi(res);
		if(val<=0 || val>cur_map)
		{
			WriteLog(__FUNCTION__" - Invalid entrance value<%d>, location pid<%u>.\n",val,pid);
			return false;
		}
		for(int k=0;k<val;k++) ploc.Entrance.push_back(DwordPairVecVal(k,0));
	}

	for(int k=0,l=ploc.Entrance.size();k<l;k++)
	{
		DWORD map_num=ploc.Entrance[k].first;
		DWORD entire=ploc.Entrance[k].second;
		if(!GetProtoMap(ploc.ProtoMapPids[map_num])->CountEntire(entire))
		{
			WriteLog(__FUNCTION__" - Entire<%u> not found on map<%u>, location pid<%u>.\n",entire,map_num,pid);
			return false;
		}
	}

	// Entrance function
	ploc.ScriptBindId=0;
	city_txt.GetStr(key1,"entrance_script","",res);
	if(res[0])
	{
		int bind_id=Script::Bind(res,"bool %s(Critter@[]&, uint8)",false);
		if(bind_id<=0)
		{
			WriteLog(__FUNCTION__" - Function<%s> not found, location pid<%u>.\n",res,pid);
			return false;
		}
		ploc.ScriptBindId=bind_id;
	}

	// Other
	ploc.Visible=city_txt.GetInt(key1,"visible",0)!=0;
	ploc.AutoGarbage=city_txt.GetInt(key1,"auto_garbage",1)!=0;
	ploc.GeckEnabled=city_txt.GetInt(key1,"geck_enabled",0)!=0;

	// Init
	ploc.LocPid=pid;
	ploc.IsInit=true;
	return true;
}

bool MapManager::LoadMapsProtos()
{
	WriteLog("Load maps prototypes...\n");

	IniParser maps_txt;
	if(!maps_txt.LoadFile("Maps.cfg",PT_SERVER_MAPS))
	{
		WriteLog("File<%s> not found.\n",FileManager::GetFullPath("Maps.cfg",PT_SERVER_MAPS));
		return false;
	}

	int errors=0;
	DWORD loaded=0;
	char res[256];
//	double wtick1=Timer::AccurateTick();
//	double wtick2=0.0;
	for(int i=1;i<MAX_PROTO_MAPS;i++)
	{
		if(maps_txt.GetStr(Str::Format("Map %u",i),"map_name","",res))
		{
//			double tick=Timer::AccurateTick();
			ProtoMap& pmap=ProtoMaps[i];
			if(!LoadMapProto(maps_txt,pmap,i))
			{
				errors++;
				WriteLog("Load proto map<%u> fail.\n",i);
				continue;
			}
			pmapsLoaded.insert(StrWordMapVal(string(pmap.GetName()),i));
			loaded++;
//			tick=Timer::AccurateTick()-tick;
//			wtick2+=tick;
//			WriteLog("map<%s><%g>\n",pmap.GetName(),tick);
		}
	}
//	wtick1=Timer::AccurateTick()-wtick1;
//	WriteLog("wtick1 %g, wtick2 %g\n",wtick1,wtick2);

	if(errors)
	{
		WriteLog("Load proto maps fail, errors<%d>.\n",errors);
		return false;
	}

	WriteLog("Load maps prototypes complete, loaded<%u> protos.\n",loaded);
	return true;
}

bool MapManager::LoadMapProto(IniParser& maps_txt, ProtoMap& pmap, WORD pid)
{
	char map_name[256];
	char key1[256];
	sprintf(key1,"Map %u",pid);
	maps_txt.GetStr(key1,"map_name","",map_name);

	if(!pmap.Init(pid,map_name,PT_SERVER_MAPS))
	{
		WriteLog(__FUNCTION__" - Proto map init fail, pid<%u>, name<%s>.\n",pid,map_name);
		return false;
	}

	if(maps_txt.IsKey(key1,"time")) pmap.Header.Time=maps_txt.GetInt(key1,"time",-1);
	if(maps_txt.IsKey(key1,"no_log_out")) pmap.Header.NoLogOut=(maps_txt.GetInt(key1,"no_log_out",0)!=0);
	if(maps_txt.IsKey(key1,"script"))
	{
		char script[MAX_SCRIPT_NAME*2+2];
		if(maps_txt.GetStr(key1,"script","",script))
		{
			if(!Script::ReparseScriptName(script,pmap.Header.ScriptModule,pmap.Header.ScriptFunc))
			{
				WriteLog(__FUNCTION__" - Proto map script name parsing fail, pid<%u>, name<%s>.\n",pid,map_name);
				return false;
			}
		}
	}
	return true;
}

void MapManager::SaveAllLocationsAndMapsFile(void(*save_func)(void*,size_t))
{
	DWORD count=allLocations.size();
	save_func(&count,sizeof(count));

	for(LocMapIt it=allLocations.begin(),end=allLocations.end();it!=end;++it)
	{
		Location* loc=(*it).second;
		save_func(&loc->Data,sizeof(loc->Data));

		MapVec& maps=loc->GetMapsNoLock();
		DWORD map_count=maps.size();
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

	DWORD count;
	fread(&count,sizeof(count),1,f);
	if(!count)
	{
		WriteLog("Locations not found.\n");
		return true;
	}

	DWORD locaded=0;
	for(DWORD i=0;i<count;++i)
	{
		// Read data
		Location::LocData data;
		fread(&data,sizeof(data),1,f);
		if(data.LocId>lastLocId) lastLocId=data.LocId;

		DWORD map_count;
		fread(&map_count,sizeof(map_count),1,f);
		vector<Map::MapData> map_data(map_count);
		for(int j=0;j<map_count;j++)
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
		for(int j=0;j<map_count;j++)
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

		for(int j=0;j<map_count;j++)
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
	result+="Location Name        Id          Pid  X     Y     Radius Color    Visible GeckEnabled GeckCount AutoGarbage ToGarbage\n";
	result+="          Map Name            Id          Pid  Time Rain TbAviable TbOn   Script\n";
	for(LocMapIt it=allLocations.begin(),end=allLocations.end();it!=end;++it)
	{
		Location* loc=(*it).second;
		sprintf(str,"%-20s %-09u   %-4u %-5u %-5u %-6u %08X %-7s %-11s %-9d %-11s %-5s\n",
			loc->Proto->Name.c_str(),loc->Data.LocId,loc->Data.LocPid,loc->Data.WX,loc->Data.WY,loc->Data.Radius,loc->Data.Color,loc->Data.Visible?"true":"false",
			loc->Data.GeckEnabled?"true":"false",loc->GeckCount,loc->Data.AutoGarbage?"true":"false",loc->Data.ToGarbage?"true":"false");
		result+=str;

		MapVec& maps=loc->GetMapsNoLock();
		DWORD map_index=0;
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

		WORD loc_pid,loc_wx,loc_wy;
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

void MapManager::GetLocationAndMapIds(DwordSet& loc_ids, DwordSet& map_ids)
{
	SCOPE_LOCK(mapLocker);

	for(LocMapIt it=allLocations.begin(),end=allLocations.end();it!=end;++it)
		loc_ids.insert((*it).second->GetId());
	for(MapMapIt it=allMaps.begin(),end=allMaps.end();it!=end;++it)
		map_ids.insert((*it).second->GetId());
}

bool MapManager::IsInitProtoLocation(WORD pid_loc)
{
	if(!pid_loc || pid_loc>=MAX_PROTO_LOCATIONS) return false;
	return ProtoLoc[pid_loc].IsInit;
}

ProtoLocation* MapManager::GetProtoLocation(WORD loc_pid)
{
	if(!IsInitProtoLocation(loc_pid)) return NULL;
	return &ProtoLoc[loc_pid];
}

Location* MapManager::CreateLocation(WORD pid_loc, WORD wx, WORD wy, DWORD loc_id)
{
	if(!IsInitProtoLocation(pid_loc))
	{
		WriteLog(__FUNCTION__" - Location proto is not init, pid<%u>.\n",pid_loc);
		return NULL;
	}

	if(!wx || !wy || wx>=GM__MAXZONEX*GameOpt.GlobalMapZoneLength || wy>=GM__MAXZONEY*GameOpt.GlobalMapZoneLength)
	{
		WriteLog(__FUNCTION__" - Invalid location coordinates, pid<%u>.\n",pid_loc);
		return NULL;
	}

	Location* loc=new Location();
	if(!loc || !loc->Init(&ProtoLoc[pid_loc],wx,wy))
	{
		WriteLog(__FUNCTION__" - Location init fail, pid<%u>.\n",pid_loc);
		loc->Release();
		return NULL;
	}

	if(!loc_id)
	{
		mapLocker.Lock();
		loc->SetId(lastLocId+1);
		lastLocId++;
		mapLocker.Unlock();

		for(int i=0;i<loc->Proto->ProtoMapPids.size();++i)
		{
			WORD map_pid=loc->Proto->ProtoMapPids[i];

			Map* map=CreateMap(map_pid,loc,0);
			if(!map)
			{
				WriteLog(__FUNCTION__" - Create map fail, pid<%u>.\n",map_pid);
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
			WriteLog(__FUNCTION__" - Location id<%u> is busy.\n",loc_id);
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
			WriteLog(__FUNCTION__" - Generate map fail.\n");
			loc->Data.ToGarbage=true;
			MapMngr.RunGarbager();
			return NULL;
		}
	}

	return loc;
}

bool MapManager::IsInitProtoMap(WORD pid_map)
{
	if(pid_map>=MAX_PROTO_MAPS) return false;
	return ProtoMaps[pid_map].IsInit();
}

Map* MapManager::CreateMap(WORD pid_map, Location* loc_map, DWORD map_id)
{
	if(!loc_map) return NULL;

	if(!IsInitProtoMap(pid_map))
	{
		WriteLog(__FUNCTION__" - Proto map<%u> is not init.\n",pid_map);
		return NULL;
	}

	Map* map=new Map();
	if(!map || !map->Init(&ProtoMaps[pid_map],loc_map))
	{
		WriteLog(__FUNCTION__" - Map init fail, pid<%u>.\n",pid_map);
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
			WriteLog(__FUNCTION__" - Map already created, id<%u>.\n",map_id);
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

Map* MapManager::GetMap(DWORD map_id, bool sync_lock)
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

Map* MapManager::GetMapByPid(WORD map_pid, DWORD skip_count)
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

DWORD MapManager::GetMapsCount()
{
	SCOPE_LOCK(mapLocker);
	DWORD count=allMaps.size();
	return count;
}

ProtoMap* MapManager::GetProtoMap(WORD pid_map)
{
	if(!IsInitProtoMap(pid_map)) return NULL;
	return &ProtoMaps[pid_map];
}

bool MapManager::IsProtoMapNoLogOut(WORD pid_map)
{
	ProtoMap* pmap=GetProtoMap(pid_map);
	return pmap?pmap->Header.NoLogOut:false;
}

Location* MapManager::GetLocationByMap(DWORD map_id)
{
	Map* map=GetMap(map_id);
	if(!map) return NULL;
	return map->GetLocation(true);
}

Location* MapManager::GetLocation(DWORD loc_id)
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

Location* MapManager::GetLocationByPid(WORD loc_pid, DWORD skip_count)
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

bool MapManager::IsLocationOnCoord(int wx, int wy)
{
	SCOPE_LOCK(mapLocker);

	for(LocMapIt it=allLocations.begin(),end=allLocations.end();it!=end;++it)
	{
		Location* loc=(*it).second;
		if(loc->IsVisible() && DistSqrt(wx,wy,loc->Data.WX,loc->Data.WY)<=loc->GetRadius()) return true;
	}
	return false;
}

bool MapManager::IsIntersectZone(int wx1, int wy1, int w1_radius, int wx2, int wy2, int w2_radius, int zones)
{
	int zl=GM_ZONE_LEN;
	INTRECT r1((wx1-w1_radius)/zl-zones,(wy1-w1_radius)/zl-zones,(wx1+w1_radius)/zl+zones,(wy1+w1_radius)/zl+zones);
	INTRECT r2((wx2-w2_radius)/zl,(wy2-w2_radius)/zl,(wx2+w2_radius)/zl,(wy2+w2_radius)/zl);
	return r1.L<=r2.R && r2.L<=r1.R && r1.T<=r2.B && r2.T<=r1.B;
}

void MapManager::GetZoneLocations(int zx, int zy, int zone_radius, DwordVec& loc_ids)
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

DWORD MapManager::GetLocationsCount()
{
	SCOPE_LOCK(mapLocker);
	DWORD count=allLocations.size();
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
			if(loc->IsToGarbage() && (loc->Data.ToGarbage || loc->IsCanDelete()))
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

bool MapManager::RefreshGmMask(const char* mask_path)
{
	WriteLog("Refresh GM Mask<%s>...",mask_path);

	FileManager fm;
	if(!fm.LoadFile(mask_path,PT_SERVER_MAPS)) WriteLog("Global map mask file not found.\n");
	else if(fm.GetLEWord()!=0x4D42) WriteLog("Invalid file format of global map mask.\n");
	else
	{
		fm.SetCurPos(28);
		if(fm.GetLEWord()!=4) WriteLog("Invalid bit per pixel format of global map mask.\n");
		else
		{
			fm.SetCurPos(18);
			WORD mask_w=fm.GetLEDWord();
			WORD mask_h=fm.GetLEDWord();
			fm.SetCurPos(118);
			gmMask=new CByteMask(mask_w,mask_h,0xFF);
			int padd_len=mask_w/2+(mask_w/2)%4;
			for(int x,y=0,w=0;y<mask_h;)
			{
				BYTE b=fm.GetByte();
				x=w*2;
				gmMask->SetByte(x,y,b>>4);
				gmMask->SetByte(x+1,y,b&0xF);
				w++;
				if(w>=mask_w/2)
				{
					w=0;
					y++;
					fm.SetCurPos(118+y*padd_len);
				}
			}
			WriteLog("size<%u><%u>. Success.\n",mask_w,mask_h);
			return true;
		}
	}
	return false;
}

void MapManager::GM_GroupMove(GlobalMapGroup* group)
{
	DWORD tick=Timer::GameTick();
	DWORD dtime=tick-group->MoveLastTick;
	if(dtime<GM_MOVE_PROC_TIME) return;
	group->MoveLastTick=tick;
	Critter* rule=group->Rule;

	if(group->EncounterDescriptor)
	{
		if(tick<group->EncounterTick) return;

		// Force invite
		if(group->EncounterForce)
		{
			GM_GlobalInvite(group,rule->Data.Params[MODE_DEFAULT_COMBAT]);
			return;
		}

		// Continue walk
		group->StartEncaunterTime(ENCOUNTERS_TIME);
		group->EncounterDescriptor=0;
		rule->SendA_GlobalInfo(group,GM_INFO_GROUP_PARAM);
	}

	if(!group->IsSetMove && tick>=group->TimeCanFollow)
	{
		group->IsSetMove=true;
		GM_GlobalProcess(rule,group,GLOBAL_PROCESS_START);
		return;
	}

	if(!group->IsMoving()) return;

	group->SyncLockGroup();

	int& xi=group->WXi;
	int& yi=group->WYi;
	float& xf=group->WXf;
	float& yf=group->WYf;
	int& mxi=group->MoveX;
	int& myi=group->MoveY;
	float& sxf=group->SpeedX;
	float& syf=group->SpeedY;
	Item* car=group->GetCar();
	int walk_type=(car?car->Proto->MiscEx.Car.WalkType:GM_WALK_GROUND);

	// Car
	if(car)
	{
		int fuel=car->CarGetFuel();
		int wear=car->CarGetWear();
		if(!fuel || wear>=car->CarGetRunToBreak())
		{
			DWORD str=(!fuel?STR_CAR_FUEL_EMPTY:STR_CAR_BROKEN);
			for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it) (*it)->Send_TextMsg(*it,str,SAY_NETMSG,TEXTMSG_GAME);
			goto label_GMStopMove;
		}
		fuel-=car->CarGetFuelConsumption();
		wear+=car->CarGetWearConsumption();
		if(fuel<0) fuel=0;
		if(wear>car->CarGetRunToBreak()) wear=car->CarGetRunToBreak();
		car->Data.Car.Fuel=fuel;
		car->Data.Car.Deteoration=wear;
	}

	// Process
	int cur_dist;
	int last_dist=DistSqrt(xi,yi,mxi,myi);

	float kr=1.0f;
	if(walk_type==GM_WALK_GROUND) kr=GlobalMapKRelief[GetGmRelief(xi,yi)];
	if(car && kr!=1.0f)
	{
		float n=car->Proto->MiscEx.Car.Negotiability;
		if(n>100 && kr<1.0f) kr+=(1.0f-kr)*(n-100.0f)/100.0f;
		else if(n>100 && kr>1.0f) kr-=(kr-1.0f)*(n-100.0f)/100.0f;
		else if(n<100 && kr<1.0f) kr-=(1.0f-kr)*(100.0f-n)/100.0f;
		else if(n<100 && kr>1.0f) kr+=(kr-1.0f)*(100.0f-n)/100.0f;
	}

	xf+=sxf*kr*((float)(dtime)/GM_MOVE_PROC_TIME);
	yf+=syf*kr*((float)(dtime)/GM_MOVE_PROC_TIME);

	int old_x=xi;
	int old_y=yi;
	int new_x=(int)xf; // Cast
	int new_y=(int)yf; // Cast

	if(new_x>=GM_MAXX || new_y>=GM_MAXY || new_x<0 || new_y<0)
	{
		if(new_x>=GM_MAXX) new_x=GM_MAXX-1;
		if(new_x<0) new_x=0;
		if(new_y>=GM_MAXY) new_y=GM_MAXY-1;
		if(new_y<0) new_y=0;
		goto label_GMStopMove;
	}

	// New position
	if(old_x!=new_x || old_y!=new_y)
	{
		// Check relief
		if((walk_type==GM_WALK_GROUND && GetGmRelief(old_x,old_y)!=0xF && GetGmRelief(new_x,new_y)==0xF ) ||
			(walk_type==GM_WALK_WATER && GetGmRelief(new_x,new_y)!=0xF))
		{
			// Move from old to new and find last correct position
			int steps=max(abs(new_x-old_x),abs(new_y-old_y));
			float xx=(float)old_x;
			float yy=(float)old_y;
			float oxx=(float)(new_x-old_x)/steps;
			float oyy=(float)(new_y-old_y)/steps;
			int new_x_=old_x;
			int new_y_=old_y;

			for(int i=0;i<steps;i++)
			{
				xx+=oxx;
				yy+=oyy;
				int xxi=(int)xx;
				int yyi=(int)yy;

				if((walk_type==GM_WALK_GROUND && GetGmRelief(xxi,yyi)==0xF) ||
					(walk_type==GM_WALK_WATER && GetGmRelief(xxi,yyi)!=0xF)) break;

				new_x_=xxi;
				new_y_=yyi;
			}
			if(new_x_==old_x && new_y_==old_y) goto label_GMStopMove;

			new_x=new_x_;
			new_y=new_y_;
		}

		// Set new position	
		xi=new_x;
		yi=new_y;

		// Add Score
//		if(car && group->GetSize()>1 && rule->IsPlayer()) SetScore(SCORE_DRIVER,(Client*)rule,group->GetSize()-1);

		for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it)
		{
			Critter* cr=*it;
//			if(cr->IsPlayer()) SetScore(SCORE_SCAUT,(Client*)cr,1);
			cr->Data.WorldX=xi;
			cr->Data.WorldY=yi;
		}

		// Zone
		int old_zone_x=GM_ZONE(old_x);
		int old_zone_y=GM_ZONE(old_y);
		int cur_zone_x=GM_ZONE(new_x);
		int cur_zone_y=GM_ZONE(new_y);

		// Change zone
		if(old_zone_x!=cur_zone_x || old_zone_y!=cur_zone_y) GM_GroupScanZone(group,cur_zone_x,cur_zone_y);
		//GM_GlobalProcess(group,GLOBAL_PROCESS_NEW_ZONE);
	}

	// Dist
	cur_dist=DistSqrt(xi,yi,mxi,myi);
	if(!cur_dist || cur_dist>last_dist)
	{
		xi=mxi;
		yi=myi;
		for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it)
		{
			Critter* cr=*it;
			cr->Data.WorldX=xi;
			cr->Data.WorldY=yi;
		}
		goto label_GMStopMove;
	}

	if(group->IsEncaunterTime())
	{
		group->EncounterDescriptor=0;
		group->StartEncaunterTime(ENCOUNTERS_TIME);
		if(rule->IsPlayer() && ((Client*)rule)->IsOffline()) goto label_GMStopMove;
		if(walk_type!=GM_WALK_GROUND || IsLocationOnCoord(xi,yi)) return;
		GM_GlobalProcess(rule,group,GLOBAL_PROCESS_MOVE);
	}

	return;

label_GMStopMove:

	sxf=0.0f;
	syf=0.0f;
	mxi=xi;
	myi=yi;
	xf=xi;
	yf=yi;
	rule->SendA_GlobalInfo(group,GM_INFO_GROUP_PARAM);
	GM_GlobalProcess(rule,group,GLOBAL_PROCESS_STOPPED);
}

void MapManager::GM_GlobalProcess(Critter* cr, GlobalMapGroup* group, int type)
{
	DWORD encounter_descriptor=0;
	bool wait_for_answer=false;
	Critter* rule=group->Rule;
	DWORD cur_wx=group->WXi;
	DWORD cur_wy=group->WYi;
	DWORD to_wx=group->MoveX;
	DWORD to_wy=group->MoveY;
	DWORD speed=0;
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
			cur_wx=group->WXi;
			cur_wy=group->WYi;
			to_wx=group->MoveX;
			to_wy=group->MoveY;
			speed=0;
		}
	}

	if(global_process && Script::PrepareContext(ServerFunctions.GlobalProcess,CALL_FUNC_STR,rule->GetInfo()))
	{
		Script::SetArgDword(type);
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

	if(!group->IsValid()) return;

	if(type==GLOBAL_PROCESS_SET_MOVE || cur_wx!=group->WXi || cur_wy!=group->WYi || to_wx!=group->MoveX || to_wy!=group->MoveY)
	{
		if(cur_wx>=GM_MAXX) cur_wx=GM_MAXX-1;
		if(cur_wy>=GM_MAXY) cur_wy=GM_MAXY-1;
		if(to_wx>=GM_MAXX) to_wx=GM_MAXX-1;
		if(to_wy>=GM_MAXY) to_wy=GM_MAXY-1;
		group->WXi=cur_wx;
		group->WYi=cur_wy;
		group->MoveX=to_wx;
		group->MoveY=to_wy;
		GM_GroupSetMove(group,to_wx,to_wy,speed);
	}

	if(speed==DWORD(-1) && group->IsValid())
	{
		group->MoveX=group->WXi;
		group->MoveY=group->WYi;
		group->WXf=group->WXi;
		group->WYf=group->WYi;
		group->SpeedX=0.0f;
		group->SpeedY=0.0f;
		GM_GlobalProcess(rule,group,GLOBAL_PROCESS_STOPPED);
	}

	if(encounter_descriptor && group->IsValid())
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
}

void MapManager::GM_GlobalInvite(GlobalMapGroup* group, int combat_mode)
{
	DWORD encounter_descriptor=group->EncounterDescriptor;
	group->EncounterDescriptor=0;
	DWORD map_id=0;
	WORD hx=0,hy=0;
	BYTE dir=0;
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

	if(global_invite && Script::PrepareContext(ServerFunctions.GlobalInvite,CALL_FUNC_STR,rule->GetInfo()))
	{
		Script::SetArgObject(rule);
		Script::SetArgObject(group->GetCar());
		Script::SetArgDword(encounter_descriptor);
		Script::SetArgDword(combat_mode);
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
		if(map) GM_GroupToMap(group,map,0,hx,hy,dir<6?dir:rule->GetDir());
	}
	else
	{
		group->StartEncaunterTime(ENCOUNTERS_TIME);
		rule->SendA_GlobalInfo(group,GM_INFO_GROUP_PARAM);
	}
}

void MapManager::GM_GroupScanZone(GlobalMapGroup* group, int zx, int zy)
{
	group->SyncLockGroup();

	DwordVec loc_ids1,loc_ids2;
	bool loc_ids2_founded=false;
	MapMngr.GetZoneLocations(zx,zy,1,loc_ids1);

	for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(!cr->IsPlayer()) continue;
		Client* cl=(Client*)cr;

		int look_len=(cr->IsPerk(PE_SCOUT)?2:1);

		if(look_len==2 && !loc_ids2_founded)
		{
			MapMngr.GetZoneLocations(zx,zy,2,loc_ids2);
			loc_ids2_founded=true;
		}

		for(int x=-look_len;x<=look_len;x++)
		{
			for(int y=-look_len;y<=look_len;y++)
			{
				int zx_=zx+x;
				int zy_=zy+y;
				if(zx_>=0 && zx_<GameOpt.GlobalMapWidth && zy_>=0 && zy_<GameOpt.GlobalMapHeight)
				{
					// Open fog
					int fog=(zx==zx_ && zy==zy_?GM_FOG_NONE:GM_FOG_SELF);
					cl->GetDataExt(); // Generate ext data
					if(cl->GMapFog.Get2Bit(zx_,zy_)<fog)
					{
						cl->GMapFog.Set2Bit(zx_,zy_,fog);
						cl->Send_GlobalMapFog(zx_,zy_,fog);
					}
				}
			}
		}

		// Find new locations
		DwordVec& loc_ids=(look_len==1?loc_ids1:loc_ids2);
		for(DwordVecIt it_=loc_ids.begin(),end_=loc_ids.end();it_!=end_;++it_)
		{
			DWORD loc_id=*it_;
			if(!cl->CheckKnownLocById(loc_id))
			{
				Location* loc=GetLocation(loc_id);
				if(loc)
				{
					cl->AddKnownLoc(loc_id);
					if(loc->IsAutomaps()) cl->Send_AutomapsInfo(NULL,loc);
					cl->Send_GlobalLocation(loc,true);
				}
			}
		}
	}
}

bool MapManager::GM_CheckEntrance(int bind_id, asIScriptArray* arr, BYTE entrance)
{
	if(Script::PrepareContext(bind_id,CALL_FUNC_STR,(*(Critter**)arr->GetElementPointer(0))->GetInfo()))
	{
		Script::SetArgObject(arr);
		Script::SetArgByte(entrance);
		if(Script::RunPrepared()) return Script::GetReturnedBool();
	}
	return false;
}

asIScriptArray* MapManager::GM_CreateGroupArray(GlobalMapGroup* group)
{
	asIScriptArray* arr=Script::CreateArray("Critter@[]");
	if(!arr)
	{
		WriteLog(__FUNCTION__" - Create script array fail.\n");
		return NULL;
	}

	group->SyncLockGroup();

	arr->Resize(group->CritMove.size());
	Critter** p_=(Critter**)arr->GetElementPointer(0);
	*p_=group->Rule;
	group->Rule->AddRef();
	int ind=1;
	for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr==group->Rule) continue;
		Critter** p=(Critter**)arr->GetElementPointer(ind);
		if(!p)
		{
			WriteLog(__FUNCTION__" - Critical bug, rule critter<%s>, not valid<%d>.\n",group->Rule->GetInfo(),group->Rule->IsNotValid);
			return NULL;
		}
		*p=cr;
		cr->AddRef();
		ind++;
	}
	return arr;
}

void MapManager::GM_GroupStartMove(Critter* cr, bool send)
{
	if(cr->GetMap())
	{
		WriteLog(__FUNCTION__" - Critter<%s> is on map.\n",cr->GetInfo());
		TransitToGlobal(cr,0,0,false);
		return;
	}

	cr->Data.HexX=0;
	cr->Data.HexY=0;
	cr->GroupMove=cr->GroupSelf;
	GlobalMapGroup* group=cr->GroupMove;
	group->Clear();
	group->WXi=cr->Data.WorldX;
	group->WYi=cr->Data.WorldY;
	group->WXf=cr->Data.WorldX;
	group->WYf=cr->Data.WorldY;
	group->MoveX=cr->Data.WorldX;
	group->MoveY=cr->Data.WorldY;
	group->SpeedX=0;
	group->SpeedY=0;
	cr->Data.GlobalGroupUid++;

	Item* car=cr->GetItemCar();
	if(car) group->CarId=car->GetId();

	group->StartEncaunterTime(ENCOUNTERS_TIME);
	group->TimeCanFollow=Timer::GameTick()+TIME_CAN_FOLLOW_GM;
	SETFLAG(cr->Flags,FCRIT_RULEGROUP);

	group->AddCrit(cr);
	GM_GroupScanZone(cr->GroupMove,GM_ZONE(group->WXi),GM_ZONE(group->WYi));

	if(send) cr->Send_GlobalInfo(GM_INFO_ALL);
	GM_GlobalProcess(cr,group,GLOBAL_PROCESS_START_FAST);
}

void MapManager::GM_AddCritToGroup(Critter* cr, DWORD rule_id)
{
	if(!cr)
	{
		WriteLog(__FUNCTION__" - Critter null ptr.");
		return;
	}

	if(!rule_id)
	{
		GM_GroupStartMove(cr,false);
		return;
	}

	Critter* rule=CrMngr.GetCritter(rule_id,true);
	if(!rule || rule->GetMap() || !rule->GroupMove || rule!=rule->GroupMove->Rule)
	{
		if(cr->IsNpc()) WriteLog(__FUNCTION__" - Invalid rule on global map. Start move alone.\n");
		GM_GroupStartMove(cr,false);
		return;
	}

	GlobalMapGroup* group=rule->GroupMove;
	group->SyncLockGroup();

	UNSETFLAG(cr->Flags,FCRIT_RULEGROUP);
	cr->Data.WorldX=group->WXi;
	cr->Data.WorldY=group->WYi;
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
			MapMngr.GM_GroupSetMove(group,group->MoveX,group->MoveY,0);
		}

		GM_GroupStartMove(cr,true);
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

	cr->GroupMove->MoveX=cr->GroupMove->WXi;
	cr->GroupMove->MoveY=cr->GroupMove->WYi;
	cr->GroupMove->WXf=cr->GroupMove->WXi;
	cr->GroupMove->WYf=cr->GroupMove->WYi;
	cr->GroupMove->SpeedX=0.0f;
	cr->GroupMove->SpeedY=0.0f;

	cr->SendA_GlobalInfo(cr->GroupMove,GM_INFO_GROUP_PARAM);
	GM_GlobalProcess(cr,cr->GroupMove,GLOBAL_PROCESS_STOPPED);
}

bool MapManager::GM_GroupToMap(GlobalMapGroup* group, Map* map, DWORD entire, WORD mx, WORD my, BYTE mdir)
{
	if(!map || !map->GetId())
	{
		WriteLog(__FUNCTION__" - Map null ptr or zero id, pointer<%p>.\n",map);
		return false;
	}

	Critter* rule=group->Rule;
	WORD hx,hy;
	BYTE dir;
	WORD car_hx,car_hy;
	Item* car=group->GetCar();
	Critter* car_owner=NULL;

	if(car)
	{
		car_owner=group->GetCritter(car->ACC_CRITTER.Id);
		if(!car_owner)
		{
			WriteLog(__FUNCTION__" - Car owner not found, rule<%s>.\n",rule->GetInfo());
			car=NULL;
		}
	}

	if(mx>=map->GetMaxHexX() || my>=map->GetMaxHexY() || mdir>5)
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
			if(mdir<6) dir=mdir;
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
	rule->Data.WorldX=rule->GroupSelf->WXi;
	rule->Data.WorldY=rule->GroupSelf->WYi;
	if(car) map->SetCritterCar(car_hx,car_hy,car_owner,car);
	if(!Transit(rule,map,hx,hy,dir,car?3:2,true)) return false;

	// Transit other
	for(CrVecIt it=transit_cr.begin(),end=transit_cr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr==rule) continue;

		cr->Data.WorldX=group->WXi;
		cr->Data.WorldY=group->WYi;

		if(!Transit(cr,map,hx,hy,dir,car?3:2,true))
		{
			GM_GroupStartMove(cr,true);
			continue;
		}
	}

	group->Clear();
	return true;
}

bool MapManager::GM_GroupToLoc(Critter* rule, DWORD loc_id, BYTE entrance, bool force /* = false */)
{
	if(rule->GetMap()) return false;
	if(!rule->GroupMove) return false;
	if(rule->IsPlayer() && ((Client*)rule)->IsOffline()) return false; // Offline on encounter
	if(!loc_id) return false;

	if(rule!=rule->GroupMove->Rule)
	{
		WriteLog(__FUNCTION__" - Critter<%s> is not rule.\n",rule->GetInfo());
		return false;
	}

	if(!force && rule->IsPlayer() && !((Client*)rule)->CheckKnownLocById(loc_id))
	{
		WriteLog(__FUNCTION__" - Critter<%s> is not known location.\n",rule->GetInfo());
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

	if(!force && DistSqrt(rule->GroupSelf->WXi,rule->GroupSelf->WYi,loc->Data.WX,loc->Data.WY)>loc->GetRadius())
	{
		if(rule->IsPlayer()) rule->Send_GlobalLocation(loc,true);
		rule->Send_TextMsg(rule,STR_GLOBAL_LOCATION_REMOVED,SAY_NETMSG,TEXTMSG_GAME);
		return false;
	}

	if(!loc->GetMapsCount())
	{
		if(rule->IsPlayer()) ((Client*)rule)->EraseKnownLoc(loc_id);
		WriteLog(__FUNCTION__" - Location is empty, critter<%s>.\n",rule->GetInfo());
		return false;
	}

	if(entrance>=loc->Proto->Entrance.size())
	{
		WriteLog(__FUNCTION__" - Invalid entrance, critter<%s>.\n",rule->GetInfo());
		return false;
	}

	if(loc->Proto->ScriptBindId)
	{
		asIScriptArray* arr=GM_CreateGroupArray(rule->GroupMove);
		if(!arr) return false;
		bool result=GM_CheckEntrance(loc->Proto->ScriptBindId,arr,entrance);
		arr->Release();
		if(!result)
		{
			WriteLog(__FUNCTION__" - Can't enter in entrance, critter<%s>.\n",rule->GetInfo());
			return false;
		}
	}

	DWORD count=loc->Proto->Entrance[entrance].first;
	DWORD entire=loc->Proto->Entrance[entrance].second;

	Map* map=loc->GetMap(count);
	if(!map)
	{
		if(rule->IsPlayer()) ((Client*)rule)->EraseKnownLoc(loc_id);
		WriteLog(__FUNCTION__" - Map not found in location, critter<%s>.\n",rule->GetInfo());
		return false;
	}

	return GM_GroupToMap(rule->GroupMove,map,entire,-1,-1,-1);
}

void MapManager::GM_GroupSetMove(GlobalMapGroup* group, int gx, int gy, DWORD speed)
{
	group->MoveX=gx;
	group->MoveY=gy;
	int dist=DistSqrt(group->MoveX,group->MoveY,group->WXi,group->WYi);

	if(!speed || !dist) // Stop
	{
		group->MoveX=group->WXi;
		group->MoveY=group->WYi;
		group->SpeedX=0.0f;
		group->SpeedY=0.0f;
	}
	else
	{
		float k_speed=1000.0f/float(speed);
		float time=(k_speed*1000.0f*float(dist))/float(GM_MOVE_PROC_TIME);
		group->SpeedX=float(group->MoveX-group->WXi)/time;
		group->SpeedY=float(group->MoveY-group->WYi)/time;
	}

	group->Rule->SendA_GlobalInfo(group,GM_INFO_GROUP_PARAM);
	if(Timer::GameTick()-group->MoveLastTick>GM_MOVE_PROC_TIME) group->MoveLastTick=Timer::GameTick();
	if(group->IsEncaunterTime()) group->StartEncaunterTime(Random(1000,ENCOUNTERS_TIME));
	if(!group->IsSetMove)
	{
		group->IsSetMove=true;
		GM_GlobalProcess(group->Rule,group,GLOBAL_PROCESS_START);
	}
}

void MapManager::TraceBullet(TraceData& trace)
{
	Map* map=trace.TraceMap;
	WORD maxhx=map->GetMaxHexX();
	WORD maxhy=map->GetMaxHexY();
	WORD hx=trace.BeginHx;
	WORD hy=trace.BeginHy;
	WORD tx=trace.EndHx;
	WORD ty=trace.EndHy;

	DWORD dist=trace.Dist;
	if(!dist) dist=DistGame(hx,hy,tx,ty);

	float nx=3.0f*(float(tx)-float(hx));
	float ny=(float(ty)-float(hy))*SQRT3T2_FLOAT-(float(tx&1)-float(hx&1))*SQRT3_FLOAT;
	float dir=180.0f+RAD2DEG*atan2f(ny,nx);

	if(trace.Angle!=0.0f)
	{
		dir+=trace.Angle;
		if(dir<0.0f) dir+=360.0f;
		else if(dir>360.0f) dir-=360.0f;
	}

	BYTE dir1,dir2;
	if(dir>=30.0f && dir<90.0f) { dir1=5; dir2=0; }
	else if(dir>=90.0f && dir<150.0f) { dir1=4; dir2=5; }
	else if(dir>=150.0f && dir<210.0f) { dir1=3; dir2=4; }
	else if(dir>=210.0f && dir<270.0f) { dir1=2; dir2=3; }
	else if(dir>=270.0f && dir<330.0f) { dir1=1; dir2=2; }
	else { dir1=0; dir2=1; }

	WORD cx=hx;
	WORD cy=hy;
	WORD old_cx=cx;
	WORD old_cy=cy;
	WORD t1x,t1y,t2x,t2y;

	float x1=3.0f*float(hx)+BIAS_FLOAT;
	float y1=SQRT3T2_FLOAT*float(hy)-SQRT3_FLOAT*(float(hx&1))+BIAS_FLOAT;
	float x2=3.0f*float(tx)+BIAS_FLOAT+BIAS_FLOAT;
	float y2=SQRT3T2_FLOAT*float(ty)-SQRT3_FLOAT*(float(tx&1))+BIAS_FLOAT;
	if(trace.Angle!=0.0f)
	{
		x2-=x1;
		y2-=y1;
		float xp=cos(trace.Angle/RAD2DEG)*x2-sin(trace.Angle/RAD2DEG)*y2;
		float yp=sin(trace.Angle/RAD2DEG)*x2+cos(trace.Angle/RAD2DEG)*y2;
		x2=x1+xp;
		y2=y1+yp;
	}
	float dx=x2-x1;
	float dy=y2-y1;

	float c1x,c1y,c2x,c2y;
	float dist1,dist2;

	trace.IsFullTrace=false;
	trace.IsCritterFounded=false;
	trace.IsHaveLastPassed=false;
	trace.IsTeammateFounded=false;
	bool last_passed_ok=false;
	for(DWORD i=0;;i++)
	{
		if(i>=dist)
		{
			trace.IsFullTrace=true;
			break;
		}

		t1x=cx;
		t2x=cx;
		t1y=cy;
		t2y=cy;
		MoveHexByDir(t1x,t1y,dir1,maxhx,maxhy);
		MoveHexByDir(t2x,t2y,dir2,maxhx,maxhy);
		c1x=3*float(t1x);
		c1y=SQRT3T2_FLOAT*float(t1y)-(float(t1x&1))*SQRT3_FLOAT;
		c2x=3*float(t2x);
		c2y=SQRT3T2_FLOAT*float(t2y)-(float(t2x&1))*SQRT3_FLOAT;
		dist1=dx*(y1-c1y)-dy*(x1-c1x);
		dist2=dx*(y1-c2y)-dy*(x1-c2x);
		dist1=(dist1>0?dist1:-dist1);
		dist2=(dist2>0?dist2:-dist2);
		if(dist1<=dist2) // Left hand biased
		{
			cx=t1x;
			cy=t1y;
		}
		else
		{
			cx=t2x;
			cy=t2y;
		}

		if(trace.HexCallback)
		{
			trace.HexCallback(map,trace.FindCr,old_cx,old_cy,cx,cy,GetFarDir(old_cx,old_cy,cx,cy));
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
				if(trace.IsCheckTeam && cr->Data.Params[ST_TEAM_ID]==trace.BaseCrTeamId)
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
	DWORD map_id=pfd.MapId;
	WORD from_hx=pfd.FromX;
	WORD from_hy=pfd.FromY;
	WORD to_hx=pfd.ToX;
	WORD to_hy=pfd.ToY;
	DWORD multihex=pfd.Multihex;
	DWORD cut=pfd.Cut;
	DWORD trace=pfd.Trace;
	bool is_run=pfd.IsRun;
	bool check_cr=pfd.CheckCrit;
	bool check_gag_items=pfd.CheckGagItems;

	// Checks
	if(trace && !pfd.TraceCr) return FPATH_TRACE_TARG_NULL_PTR;

	Map* map=GetMap(map_id);
	if(!map) return FPATH_MAP_NOT_FOUND;
	WORD maxhx=map->GetMaxHexX();
	WORD maxhy=map->GetMaxHexY();

	if(from_hx>=maxhx || from_hy>=maxhy || to_hx>=maxhx || to_hy>=maxhy) return FPATH_INVALID_HEXES;

	if(CheckDist(from_hx,from_hy,to_hx,to_hy,cut)) return FPATH_ALREADY_HERE;
	if(!cut && FLAG(map->GetHexFlags(to_hx,to_hy),FH_NOWAY)) return FPATH_HEX_BUSY;

	// Ring check
	if(cut<=1 && !multihex)
	{
		bool odd=(to_hx&1)!=0;
		short* rsx=(odd?SXOdd:SXEven);
		short* rsy=(odd?SYOdd:SYEven);

		int i;
		for(i=0;i<6;i++,rsx++,rsy++)
		{
			short xx=to_hx+*rsx;
			short yy=to_hy+*rsy;
			if(xx>=0 && xx<maxhx && yy>=0 && yy<maxhy && !FLAG(map->GetHexFlags(xx,yy),FH_NOWAY)) break;
		}

		if(i==6) return FPATH_HEX_BUSY_RING;
	}

	// Parse previous move params
	/*WordPairVec first_steps;
	BYTE first_dir=pfd.MoveParams&7;
	if(first_dir<6)
	{
		WORD hx_=from_hx;
		WORD hy_=from_hy;
		MoveHexByDir(hx_,hy_,first_dir);
		if(map->IsHexPassed(hx_,hy_))
		{
			first_steps.push_back(WordPairVecVal(hx_,hy_));
		}
	}
	for(int i=0;i<4;i++)
	{

	}*/

	// Prepare
	int numindex=1;
	ZeroMemory(Grid,(FPATH_MAX_PATH*2+2)*(FPATH_MAX_PATH*2+2)*sizeof(short));
	MapGridOffsX=from_hx;
	MapGridOffsY=from_hy;
	GRID(from_hx,from_hy)=numindex;

	WordPairVec coords,cr_coords,gag_coords;
	coords.reserve(10000);
	cr_coords.reserve(100);
	gag_coords.reserve(100);

	// First point
	coords.push_back(WordPairVecVal(from_hx,from_hy));

	// Begin search
	int p=0,p_togo=1;
	WORD cx,cy;
	while(true)
	{
		for(int i=0;i<p_togo;i++,p++)
		{
			cx=coords[p].first;
			cy=coords[p].second;
			numindex=GRID(cx,cy);

			if(CheckDist(cx,cy,to_hx,to_hy,cut)) goto label_FindOk;
			if(++numindex>FPATH_MAX_PATH) return FPATH_TOOFAR;

			bool odd=(cx&1)!=0;
			for(int j=0;j<6;j++)
			{
				short nx=(short)cx+(odd?SXOdd[j]:SXEven[j]);
				short ny=(short)cy+(odd?SYOdd[j]:SYEven[j]);
				if(nx<0 || ny<0 || nx>=maxhx || ny>=maxhy) continue;

				short& g=GRID(nx,ny);
				if(g) continue;

				if(!multihex)
				{
					WORD flags=map->GetHexFlags(nx,ny);
					if(!FLAG(flags,FH_NOWAY))
					{
						coords.push_back(WordPairVecVal(nx,ny));
						g=numindex;
					}
					else if(check_gag_items && FLAG(flags,FH_GAG_ITEM<<8))
					{
						gag_coords.push_back(WordPairVecVal(nx,ny));
						g=numindex|0x4000;
					}
					else if(check_cr && FLAG(flags,FH_CRITTER<<8))
					{
						cr_coords.push_back(WordPairVecVal(nx,ny));
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
					for(DWORD k=0;k<multihex;k++) MoveHexByDirUnsafe(hx_,hy_,j);
					if(hx_<0 || hy_<0 || hx_>=maxhx || hy_>=maxhy) continue;
					//if(!IsHexPassed(hx_,hy_)) return false;

					// Clock wise hexes
					int dir_=(j+2)%6;
					int hx__=hx_,hy__=hy_;
					for(DWORD k=0;k<multihex;k++)
					{
						MoveHexByDirUnsafe(hx__,hy__,dir_);
						//if(!IsHexPassed(hx__,hy__)) return false;
					}

					// Counter clock wise hexes
					dir_=(j+4)%6;
					hx__=hx_,hy__=hy_;
					for(DWORD k=0;k<multihex;k++)
					{
						MoveHexByDirUnsafe(hx__,hy__,dir_);
						//if(!IsHexPassed(hx__,hy__)) return false;
					}
					*/

					if(map->IsMovePassed(nx,ny,j,multihex))
					{
						coords.push_back(WordPairVecVal(nx,ny));
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
			WordPair& xy=gag_coords.front();
			short gag_index=GRID(xy.first,xy.second)^0x4000;
			if(gag_index+10<last_index) // Todo: if path finding not be reworked than migrate magic number to scripts
			{
				GRID(xy.first,xy.second)=gag_index;
				coords.push_back(xy);
				gag_coords.erase(gag_coords.begin());
			}
		}

		// Add gag and critters hexes
		p_togo=coords.size()-p;
		if(!p_togo)
		{
			if(gag_coords.size())
			{
				WordPair& xy=gag_coords.front();
				GRID(xy.first,xy.second)^=0x4000;
				coords.push_back(xy);
				gag_coords.erase(gag_coords.begin());
				p_togo++;
			}
			else if(cr_coords.size())
			{
				WordPair& xy=cr_coords.front();
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

	// Parse full path
	static THREAD bool switcher=false;
	while(numindex>1)
	{
		if(numindex&1) switcher=!switcher;
		numindex--;
		PathStep& ps=path[numindex-1];
		ps.HexX=cx;
		ps.HexY=cy;
		int dir=FindPathGrid(cx,cy,numindex,switcher);
		if(dir==-1) return FPATH_ERROR;
		ps.Dir=dir;
	}

	// Check for closed door and critter
	if(check_cr || check_gag_items)
	{
		for(int i=0,j=path.size();i<j;i++)
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
		WORD targ_hx=pfd.TraceCr->GetHexX();
		WORD targ_hy=pfd.TraceCr->GetHexY();
		bool trace_ok=false;

		trace_seq.resize(path.size()+4);
		for(int i=0,j=path.size();i<j;i++)
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
			for(int i=0,j=path.size();i<j;i++)
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

int MapManager::FindPathGrid(WORD& hx, WORD& hy, int index, bool switcher)
{
	if(switcher)
	{
		if(hx&1)
		{
			if(GRID(hx-1,hy-1)==index) { hx--; hy--; return GetDir(hx,hy,hx+1,hy+1); } // 0
			if(GRID(hx,hy-1)==index)   { hy--; return GetDir(hx,hy,hx,hy+1); } // 5
			if(GRID(hx,hy+1)==index)   { hy++; return GetDir(hx,hy,hx,hy-1); } // 2
			if(GRID(hx+1,hy)==index)   { hx++; return GetDir(hx,hy,hx-1,hy); } // 3
			if(GRID(hx-1,hy)==index)   { hx--; return GetDir(hx,hy,hx+1,hy); } // 1
			if(GRID(hx+1,hy-1)==index) { hx++; hy--; return GetDir(hx,hy,hx-1,hy+1); } // 4
		}
		else
		{
			if(GRID(hx-1,hy)==index)   { hx--; return GetDir(hx,hy,hx+1,hy); } // 0
			if(GRID(hx,hy-1)==index)   { hy--; return GetDir(hx,hy,hx,hy+1); } // 5
			if(GRID(hx,hy+1)==index)   { hy++; return GetDir(hx,hy,hx,hy-1); } // 2
			if(GRID(hx+1,hy+1)==index) { hx++; hy++; return GetDir(hx,hy,hx-1,hy-1); } // 3
			if(GRID(hx-1,hy+1)==index) { hx--; hy++; return GetDir(hx,hy,hx+1,hy-1); } // 1
			if(GRID(hx+1,hy)==index)   { hx++; return GetDir(hx,hy,hx-1,hy); } // 4
		}
	}
	else
	{
		if(hx&1)
		{
			if(GRID(hx-1,hy)==index)   { hx--; return GetDir(hx,hy,hx+1,hy); } // 1
			if(GRID(hx+1,hy-1)==index) { hx++; hy--; return GetDir(hx,hy,hx-1,hy+1); } // 4
			if(GRID(hx,hy-1)==index)   { hy--; return GetDir(hx,hy,hx,hy+1); } // 5
			if(GRID(hx-1,hy-1)==index) { hx--; hy--; return GetDir(hx,hy,hx+1,hy+1); } // 0
			if(GRID(hx+1,hy)==index)   { hx++; return GetDir(hx,hy,hx-1,hy); } // 3
			if(GRID(hx,hy+1)==index)   { hy++; return GetDir(hx,hy,hx,hy-1); } // 2
		}
		else
		{
			if(GRID(hx-1,hy+1)==index) { hx--; hy++; return GetDir(hx,hy,hx+1,hy-1); } // 1
			if(GRID(hx+1,hy)==index)   { hx++; return GetDir(hx,hy,hx-1,hy); } // 4
			if(GRID(hx,hy-1)==index)   { hy--; return GetDir(hx,hy,hx,hy+1); } // 5
			if(GRID(hx-1,hy)==index)   { hx--; return GetDir(hx,hy,hx+1,hy); } // 0
			if(GRID(hx+1,hy+1)==index) { hx++; hy++; return GetDir(hx,hy,hx-1,hy-1); } // 3
			if(GRID(hx,hy+1)==index)   { hy++; return GetDir(hx,hy,hx,hy-1); } // 2
		}
	}

	return -1;
}

void MapManager::PathSetMoveParams(PathStepVec& path, bool is_run)
{
	WORD move_params=0xFFFF;
	for(int i=(int)path.size()-1;i>=0;i--)
	{
		PathStep& ps=path[i];
		move_params=(move_params<<3)|ps.Dir;
		if(is_run) SETFLAG(move_params,0x8000);
		else UNSETFLAG(move_params,0x8000);
		ps.MoveParams=move_params;
	}
}

bool MapManager::TryTransitCrGrid(Critter* cr, Map* map, WORD hx, WORD hy, bool force)
{
	if(cr->LockMapTransfers)
	{
		WriteLog(__FUNCTION__" - Transfers locked, critter<%s>.\n",cr->GetInfo());
		return false;
	}

	if(!cr->IsPlayer() || !cr->IsLife()) return false;
	if(!map || !FLAG(map->GetHexFlags(hx,hy),FH_SCEN_GRID)) return false;
	if(!force && !map->IsTurnBasedOn && cr->IsTransferTimeouts(true)) return false;

	Location* loc=map->GetLocation(true);
	DWORD id_map=0;
	BYTE dir=0;

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

bool MapManager::TransitToGlobal(Critter* cr, DWORD rule, BYTE follow_type, bool force)
{
	if(cr->LockMapTransfers)
	{
		WriteLog(__FUNCTION__" - Transfers locked, critter<%s>.\n",cr->GetInfo());
		return false;
	}

	return Transit(cr,NULL,rule>>16,rule&0xFFFF,follow_type,0,force);
}

bool MapManager::Transit(Critter* cr, Map* map, WORD hx, WORD hy, BYTE dir, DWORD radius, bool force)
{
	// Check location deletion
	if(map && map->GetLocation(true)->Data.ToGarbage)
	{
		WriteLog(__FUNCTION__" - Transfer to deleted location, critter<%s>.\n",cr->GetInfo());
		return false;
	}

	// Maybe critter already in transfer
	if(cr->LockMapTransfers)
	{
		WriteLog(__FUNCTION__" - Transfers locked, critter<%s>.\n",cr->GetInfo());
		return false;
	}

	// Check force
	if(!force)
	{
		if(cr->GetTimeout(TO_TRANSFER) || cr->GetTimeout(TO_BATTLE)) return false;
		if(cr->IsDead()) return false;
		if(cr->IsKnockout()) return false;
	}

	DWORD map_id=(map?map->GetId():0);
	DWORD old_map_id=cr->GetMap();
	Map* old_map=MapMngr.GetMap(old_map_id,true);
	WORD old_hx=cr->GetHexX();
	WORD old_hy=cr->GetHexY();

	// Recheck after synchronization
	if(cr->GetMap()!=old_map_id) return false;

	// One map
	if(old_map_id==map_id)
	{
		// Global
		if(!map_id) return true;

		// Local
		if(!map || hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) return false;

		DWORD multihex=cr->GetMultihex();
		if(!map->FindStartHex(hx,hy,multihex,radius,true) && !map->FindStartHex(hx,hy,multihex,radius,false)) return false;

		cr->LockMapTransfers++; // Transfer begin critical section
		cr->Data.Dir=(dir>5?0:dir);
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
		cr->LockMapTransfers--; // Transfer end critical section
		return true;
	}
	// Different maps
	else
	{
		if(!AddCrToMap(cr,map,hx,hy,radius)) return false;

		// Global
		if(!map)
		{
			// to_ori == follow_type
			if(dir==FOLLOW_PREP) cr->SendA_Follow(dir,0,TIME_CAN_FOLLOW_GM);
		}
		// Local
		else
		{
			cr->Data.Dir=(dir>5?0:dir);
		}

		if(!old_map_id || old_map) EraseCrFromMap(cr,old_map,old_hx,old_hy);

		cr->SetBreakTime(0);
		cr->Send_LoadMap(NULL);

		cr->LockMapTransfers++; // Transfer begin critical section
		cr->DisableSend++;
		cr->ProcessVisibleCritters();
		cr->ProcessVisibleItems();
		cr->DisableSend--;
		cr->LockMapTransfers--; // Transfer end critical section
	}
	return true;
}

bool MapManager::AddCrToMap(Critter* cr, Map* map, WORD tx, WORD ty, DWORD radius)
{
	if(cr->LockMapTransfers)
	{
		WriteLog(__FUNCTION__" - Transfers locked, critter<%s>.\n",cr->GetInfo());
		return false;
	}

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
		DWORD to_group=(tx<<16)|ty;
		if(!to_group) GM_GroupStartMove(cr,false);
		else GM_AddCritToGroup(cr,to_group);
		cr->LockMapTransfers--;
	}
	// Local map
	else
	{
		if(tx>=map->GetMaxHexX() || ty>=map->GetMaxHexY()) return false;

		DWORD multihex=cr->GetMultihex();
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
		map->AddCritterEvents(cr);
	}
	return true;
}

void MapManager::EraseCrFromMap(Critter* cr, Map* map, WORD hex_x, WORD hex_y)
{
	if(cr->LockMapTransfers)
	{
		WriteLog(__FUNCTION__" - Transfers locked, critter<%s>.\n",cr->GetInfo());
		return;
	}

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

MapManager::MapManager():lastMapId(0),lastLocId(0),gmMask(NULL),runGarbager(true)
{
	MEMORY_PROCESS(MEMORY_STATIC,sizeof(MapManager));
	MEMORY_PROCESS(MEMORY_STATIC,sizeof(Grid));
}






