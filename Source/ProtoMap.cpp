#include "StdAfx.h"
#include "ProtoMap.h"
#include "ItemManager.h"
#include "Crypt.h"
#include "Names.h"
#include "Version.h"
#include <strstream>

#ifdef FONLINE_MAPPER
#include "ResourceManager.h"
#endif

#define F1_MAP_VERSION          (0x13000000)
#define F2_MAP_VERSION          (0x14000000)
#define FO_MAP_VERSION_V1       (0xF0000000)
#define FO_MAP_VERSION_V2       (0xFA000000)
#define FO_MAP_VERSION_V3       (0xFB000000)
#define FO_MAP_VERSION_V4       (0xFC000000)
#define FO_MAP_VERSION_V5       (0xFD000000)
#define FO_MAP_VERSION_V6       (0xFE000000)
#define FO_MAP_VERSION_V7       (0xFF000000)
#define FO_MAP_VERSION_V8       (0xFF100000)
#define FO_MAP_VERSION_V9       (0xFF200000)

#define APP_HEADER              "Header"
#define APP_TILES               "Tiles"
#define APP_OBJECTS             "Objects"

// Deprecated
#define PMAP_OBJECT_SIZE_V5     (132)
#define MAPOBJ_CRITTER          (1)
#define MAPOBJ_WALL             (2) // Deprecated
#define MAPOBJ_ITEM             (3)
#define MAPOBJ_SCENERY          (4)

class MapObjectV5
{
public:
	BYTE MapObjType;
	bool InContainer;
	WORD ProtoId;

	WORD MapX;
	WORD MapY;

	BYTE FrameNum;
	BYTE Dir;

	BYTE Reserved0;
	BYTE Reserved1;

	BYTE ItemSlot;
	BYTE Reserved2;
	WORD Reserved3;
	DWORD Reserved4;

	short OffsetX;
	short OffsetY;

	char ScriptName[MAPOBJ_SCRIPT_NAME+1];
	char FuncName[MAPOBJ_SCRIPT_NAME+1];

	DWORD LightRGB;
	BYTE Reserved5;
	BYTE LightDay;
	BYTE LightDirOff;
	BYTE LightDistance;
	int LightIntensity;

	int BindScriptId; // For script bind in runtime

	union
	{
		// Critter
		struct
		{
			DWORD DialogId;
			DWORD AiPack;
			WORD Reserved0;
			WORD Reserved1;
			BYTE Cond;
			BYTE CondExt;
			BYTE Reserved2; // Level
			BYTE Reserved3;
			WORD BagId;
			WORD TeamId;
			int NpcRole;
			short RespawnTime;
			WORD Reserved11;
		} CRITTER;

		// Item
		struct
		{
			DWORD Val1;
			BYTE DeteorationFlags;
			BYTE DeteorationCount;
			WORD DeteorationValue;
		} ARMOR;

		struct
		{
			DWORD DoorId;
			WORD Condition;
			WORD Complexity;
			BYTE IsOpen;
			DWORD Reserved;
		} CONTAINER;

		struct
		{
			DWORD Count;
		} DRUG;

		struct
		{
			DWORD Reserved;
			BYTE DeteorationFlags;
			BYTE DeteorationCount;
			WORD DeteorationValue;
			WORD AmmoPid;
			DWORD AmmoCount;
		} WEAPON;

		struct
		{
			DWORD Count;
		} AMMO;

		struct
		{
			DWORD Count;
		} MISC;

		struct
		{
			DWORD Reserved[4];
			int Val1;
			int Val2;
			int Val3;
			int Val4;
			int Val5;
			int Val0;
		} ITEM_VAL;

		struct
		{
			DWORD DoorId;
		} KEY;

		struct
		{
			DWORD DoorId;
			WORD Condition;
			WORD Complexity;
			BYTE IsOpen;
		} DOOR;

		// Scenery
		struct
		{
			DWORD ElevatorType;
			DWORD ToMapPid;
			WORD ToMapX;
			WORD ToMapY;
			BYTE ToDir;
			BYTE ToEntire;
		} GRID;

		struct
		{
			BYTE CanUse;
			BYTE TriggerNum;
			WORD Reserved0;
			DWORD Reserved1[3]; // Trash
			WORD Reserved2;
			BYTE Reserved3;
			BYTE ParamsCount;
			int Param[5];
		} GENERIC;

		struct
		{
			DWORD Buffer[10];
		} ALIGN;
	};
};

/************************************************************************/
/* ProtoMap                                                             */
/************************************************************************/

bool ProtoMap::Init(WORD pid, const char* name, int path_type)
{
	Clear();
	if(!name || !name[0]) return false;

	pmapPid=pid;
	pmapFm=new FileManager();
	pathType=path_type;
	pmapName=name;

	isInit=true;
	if(!Refresh())
	{
		Clear();
		return false;
	}
	return true;
}

void ProtoMap::Clear()
{
#ifdef FONLINE_SERVER
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)SceneriesToSend.capacity()*sizeof(ScenToSend));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)WallsToSend.capacity()*sizeof(ScenToSend));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)mapEntires.capacity()*sizeof(MapEntire));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)CrittersVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)ItemsVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)SceneryVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)GridsVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)Header.MaxHexX*Header.MaxHexY);
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)GetTilesSize());

	SAFEDELA(HexFlags);

	for(MapObjectPtrVecIt it=CrittersVec.begin(),end=CrittersVec.end();it!=end;++it) SAFEREL(*it);
	CrittersVec.clear();
	for(MapObjectPtrVecIt it=ItemsVec.begin(),end=ItemsVec.end();it!=end;++it) SAFEREL(*it);
	ItemsVec.clear();
	for(MapObjectPtrVecIt it=SceneryVec.begin(),end=SceneryVec.end();it!=end;++it) SAFEREL(*it);
	SceneryVec.clear();
	for(MapObjectPtrVecIt it=GridsVec.begin(),end=GridsVec.end();it!=end;++it) SAFEREL(*it);
	GridsVec.clear();
#endif

	for(MapObjectPtrVecIt it=MObjects.begin(),end=MObjects.end();it!=end;++it)
	{
		MapObject* mobj=*it;
#ifdef FONLINE_MAPPER
		mobj->RunTime.FromMap=NULL;
		mobj->RunTime.MapObjId=0;
#endif
		SAFEREL(mobj);
	}
	MObjects.clear();
	SAFEDEL(pmapFm);
	SAFEDELA(Tiles);
	ZeroMemory(&Header,sizeof(Header));
	pmapName="";
	pathType=0;
	isInit=false;
}

bool ProtoMap::ReadHeader(int version)
{
	ZeroMemory(&Header,sizeof(Header));

	if(version>=9)
	{
		pmapFm->SetCurPos(0);
		if(!pmapFm->CopyMem(&Header,8)) return false;
		pmapFm->SetCurPos(0);
		if(!pmapFm->CopyMem(&Header,Header.HeaderSize)) return false;
	}
	else if(version>=7)
	{
		pmapFm->SetCurPos(0);
		if(!pmapFm->CopyMem(&Header,8)) return false;
		pmapFm->SetCurPos(0);
		if(!pmapFm->CopyMem(&Header,Header.HeaderSize)) return false;

		Header.DayTime[0]=300; Header.DayTime[1]=600;  Header.DayTime[2]=1140; Header.DayTime[3]=1380;
		Header.DayColor[0]=18; Header.DayColor[1]=128; Header.DayColor[2]=103; Header.DayColor[3]=51;
		Header.DayColor[4]=18; Header.DayColor[5]=128; Header.DayColor[6]=95;  Header.DayColor[7]=40;
		Header.DayColor[8]=53; Header.DayColor[9]=128; Header.DayColor[10]=86; Header.DayColor[11]=29;
	}
	else
	{
		pmapFm->SetCurPos(0);
		if(!pmapFm->CopyMem(&Header,96)) return false;
		Header.HeaderSize=96;

		if(version<6)
		{
			Header.Packed=true;
			Header.Time=-1;
		}

		Header.DayTime[0]=300; Header.DayTime[1]=600;  Header.DayTime[2]=1140; Header.DayTime[3]=1380;
		Header.DayColor[0]=18; Header.DayColor[1]=128; Header.DayColor[2]=103; Header.DayColor[3]=51;
		Header.DayColor[4]=18; Header.DayColor[5]=128; Header.DayColor[6]=95;  Header.DayColor[7]=40;
		Header.DayColor[8]=53; Header.DayColor[9]=128; Header.DayColor[10]=86; Header.DayColor[11]=29;
	}

	return true;
}

bool ProtoMap::ReadTiles(int version)
{
	Tiles=new(nothrow) DWORD[GetTilesSize()/sizeof(DWORD)];
	if(!Tiles) return false;
	pmapFm->SetCurPos(Header.Packed?0:Header.HeaderSize);

	if(version<8)
	{
		// Convert lst offsets to name hashes
		ZeroMemory(Tiles,GetTilesSize());
		DWORD size=(Header.MaxHexX/2)*(Header.MaxHexY/2)*sizeof(DWORD);
		DWORD* ptr=new(nothrow) DWORD[size/sizeof(DWORD)];
		if(!pmapFm->CopyMem(ptr,size)) return false;
		for(int x=0;x<Header.MaxHexX/2;x++)
		{
			for(int y=0;y<Header.MaxHexY/2;y++)
			{
				WORD tile=(ptr[y*(Header.MaxHexX/2)+x]>>16);
				WORD roof=(ptr[y*(Header.MaxHexX/2)+x]&0xFFFF);
				if(tile>1) Tiles[y*(Header.MaxHexX/2)*2+x*2]=Deprecated_GetPicHash(-2,0,tile);
				if(roof>1) Tiles[y*(Header.MaxHexX/2)*2+x*2+1]=Deprecated_GetPicHash(-2,0,roof);
			}
		}
		delete[] ptr;
		return true;
	}

	return pmapFm->CopyMem(Tiles,GetTilesSize());
}

bool ProtoMap::ReadObjects(int version)
{
	if(version<6)
	{
		pmapFm->SetCurPos((Header.Packed?0:Header.HeaderSize)+(Header.MaxHexX/2)*(Header.MaxHexY/2)*sizeof(DWORD));
		DWORD count=pmapFm->GetLEDWord();
		if(!count) return true;

		vector<MapObjectV5> objects_v5;
		objects_v5.resize(count);
		if(!pmapFm->CopyMem(&objects_v5[0],count*sizeof(MapObjectV5))) return false;

		for(int k=0;k<count;k++)
		{
			MapObjectV5& obj_v5=objects_v5[k];
			MapObject obj_v6;
			ZeroMemory(&obj_v6,sizeof(obj_v6));
			ProtoItem* proto=ItemMngr.GetProtoItem(obj_v5.ProtoId);

			if(version<5 && obj_v5.MapObjType==MAPOBJ_ITEM)
			{
				obj_v5.ITEM_VAL.Val0=0;
				obj_v5.ITEM_VAL.Val1=obj_v5.ITEM_VAL.Reserved[0];
				obj_v5.ITEM_VAL.Val2=obj_v5.ITEM_VAL.Reserved[1];
				obj_v5.ITEM_VAL.Val3=obj_v5.ITEM_VAL.Reserved[2];
				obj_v5.ITEM_VAL.Val4=obj_v5.ITEM_VAL.Reserved[3];
				obj_v5.ITEM_VAL.Val5=0;
			}

			if(obj_v5.MapObjType==MAPOBJ_CRITTER) obj_v6.MapObjType=MAP_OBJECT_CRITTER;
			else if(obj_v5.MapObjType==MAPOBJ_WALL) obj_v6.MapObjType=MAP_OBJECT_SCENERY;
			else if(obj_v5.MapObjType==MAPOBJ_ITEM) obj_v6.MapObjType=MAP_OBJECT_ITEM;
			else if(obj_v5.MapObjType==MAPOBJ_SCENERY) obj_v6.MapObjType=MAP_OBJECT_SCENERY;
			else continue;

			obj_v6.ProtoId=obj_v5.ProtoId;
			obj_v6.MapX=obj_v5.MapX;
			obj_v6.MapY=obj_v5.MapY;
			obj_v6.Dir=obj_v5.Dir;
			obj_v6.LightColor=obj_v5.LightRGB;
			obj_v6.LightDay=obj_v5.LightDay;
			obj_v6.LightDirOff=obj_v5.LightDirOff;
			obj_v6.LightDistance=obj_v5.LightDistance;
			obj_v6.LightIntensity=(obj_v5.LightIntensity>100?50:(obj_v5.LightIntensity<-100?-50:obj_v5.LightIntensity));
			StringCopy(obj_v6.ScriptName,obj_v5.ScriptName);
			StringCopy(obj_v6.FuncName,obj_v5.FuncName);

			if(obj_v6.MapObjType==MAP_OBJECT_CRITTER)
			{
				obj_v6.MCritter.Cond=obj_v5.CRITTER.Cond;
				obj_v6.MCritter.CondExt=obj_v5.CRITTER.CondExt;
				for(int i=0;i<MAPOBJ_CRITTER_PARAMS;i++) obj_v6.MCritter.ParamIndex[i]=-1;
				obj_v6.MCritter.ParamIndex[0]=ST_DIALOG_ID;
				obj_v6.MCritter.ParamIndex[1]=ST_AI_ID;
				obj_v6.MCritter.ParamIndex[2]=ST_BAG_ID;
				obj_v6.MCritter.ParamIndex[3]=ST_TEAM_ID;
				obj_v6.MCritter.ParamIndex[4]=ST_NPC_ROLE;
				obj_v6.MCritter.ParamIndex[5]=ST_REPLICATION_TIME;
				obj_v6.MCritter.ParamValue[0]=obj_v5.CRITTER.DialogId;
				obj_v6.MCritter.ParamValue[1]=obj_v5.CRITTER.AiPack;
				obj_v6.MCritter.ParamValue[2]=obj_v5.CRITTER.BagId;
				obj_v6.MCritter.ParamValue[3]=obj_v5.CRITTER.TeamId;
				obj_v6.MCritter.ParamValue[4]=obj_v5.CRITTER.NpcRole;
				obj_v6.MCritter.ParamValue[5]=obj_v5.CRITTER.RespawnTime;
			}
			else if(obj_v6.MapObjType==MAP_OBJECT_ITEM)
			{
				if(!proto) continue;

				obj_v6.MItem.OffsetX=obj_v5.OffsetX;
				obj_v6.MItem.OffsetY=obj_v5.OffsetY;
				obj_v6.MItem.AnimStayBegin=obj_v5.FrameNum;
				obj_v6.MItem.AnimStayEnd=obj_v5.FrameNum;
				obj_v6.MItem.InContainer=obj_v5.InContainer;
				if(obj_v6.MItem.InContainer) obj_v6.MItem.ItemSlot=obj_v5.ItemSlot;
				for(int i=0;i<10;i++) obj_v6.MItem.Val[i]=0;
				obj_v6.MItem.Val[0]=obj_v5.ITEM_VAL.Val0;
				obj_v6.MItem.Val[1]=obj_v5.ITEM_VAL.Val1;
				obj_v6.MItem.Val[2]=obj_v5.ITEM_VAL.Val2;
				obj_v6.MItem.Val[3]=obj_v5.ITEM_VAL.Val3;
				obj_v6.MItem.Val[4]=obj_v5.ITEM_VAL.Val4;
				obj_v6.MItem.Val[5]=obj_v5.ITEM_VAL.Val5;

				if(proto->GetType()==ITEM_TYPE_AMMO) obj_v6.MItem.Count=obj_v5.AMMO.Count;
				else if(proto->GetType()==ITEM_TYPE_MISC) obj_v6.MItem.Count=obj_v5.MISC.Count;
				else if(proto->GetType()==ITEM_TYPE_DRUG) obj_v6.MItem.Count=obj_v5.DRUG.Count;
				else if(proto->GetType()==ITEM_TYPE_ARMOR)
				{
					obj_v6.MItem.DeteorationFlags=obj_v5.ARMOR.DeteorationFlags;
					obj_v6.MItem.DeteorationCount=obj_v5.ARMOR.DeteorationCount;
					obj_v6.MItem.DeteorationValue=obj_v5.ARMOR.DeteorationValue;
				}
				else if(proto->GetType()==ITEM_TYPE_WEAPON)
				{
					obj_v6.MItem.DeteorationFlags=obj_v5.WEAPON.DeteorationFlags;
					obj_v6.MItem.DeteorationFlags=obj_v5.WEAPON.DeteorationFlags;
					obj_v6.MItem.DeteorationFlags=obj_v5.WEAPON.DeteorationFlags;
					obj_v6.MItem.AmmoPid=obj_v5.WEAPON.AmmoPid;
					obj_v6.MItem.AmmoCount=obj_v5.WEAPON.AmmoCount;
				}
				else if(proto->GetType()==ITEM_TYPE_KEY)
				{
					obj_v6.MItem.LockerDoorId=obj_v5.KEY.DoorId;
				}
				else if(proto->GetType()==ITEM_TYPE_CONTAINER || proto->GetType()==ITEM_TYPE_DOOR)
				{
					obj_v6.MItem.LockerDoorId=obj_v5.CONTAINER.DoorId;
					obj_v6.MItem.LockerCondition=obj_v5.CONTAINER.Condition;
					obj_v6.MItem.LockerComplexity=obj_v5.CONTAINER.Complexity;
					if(obj_v5.CONTAINER.IsOpen) SETFLAG(obj_v6.MItem.LockerCondition,LOCKER_ISOPEN);
				}
			}
			else if(obj_v6.MapObjType==MAP_OBJECT_SCENERY)
			{
				if(!proto) continue;

				obj_v6.MScenery.OffsetX=obj_v5.OffsetX;
				obj_v6.MScenery.OffsetY=obj_v5.OffsetY;

				if(proto->GetType()==ITEM_TYPE_GRID)
				{
					obj_v6.MScenery.ToEntire=obj_v5.GRID.ToEntire;
					obj_v6.MScenery.ToMapPid=obj_v5.GRID.ToMapPid;
					obj_v6.MScenery.ToMapX=obj_v5.GRID.ToMapX;
					obj_v6.MScenery.ToMapY=obj_v5.GRID.ToMapY;
					obj_v6.MScenery.ToDir=obj_v5.GRID.ToDir;
				}
				else if(proto->GetType()==ITEM_TYPE_GENERIC)
				{
					obj_v6.MScenery.CanUse=obj_v5.GENERIC.CanUse!=0;
					obj_v6.MScenery.TriggerNum=obj_v5.GENERIC.TriggerNum;
					obj_v6.MScenery.ParamsCount=obj_v5.GENERIC.ParamsCount;
					for(int i=0;i<obj_v6.MScenery.ParamsCount;i++) obj_v6.MScenery.Param[i]=obj_v5.GENERIC.Param[i];
				}
			}

			MObjects.push_back(new MapObject(obj_v6));
		}
		return true;
	}
	else if(version<8)
	{
		pmapFm->SetCurPos((Header.Packed?0:Header.HeaderSize)+(Header.MaxHexX/2)*(Header.MaxHexY/2)*sizeof(DWORD));
		DWORD count=pmapFm->GetLEDWord();
		if(!count) return true;

		for(DWORD i=0;i<count;i++)
		{
			MapObject* mobj=new MapObject();
			if(!pmapFm->CopyMem(mobj,MAP_OBJECT_SIZE)) return false;
			MObjects.push_back(mobj);
		}
		return true;
	}
	else // version 8
	{
		pmapFm->SetCurPos((Header.Packed?0:Header.HeaderSize)+GetTilesSize());
		DWORD count=pmapFm->GetLEDWord();
		if(!count) return true;

		for(DWORD i=0;i<count;i++)
		{
			MapObject* mobj=new MapObject();
			if(!pmapFm->CopyMem(mobj,MAP_OBJECT_SIZE)) return false;
			MObjects.push_back(mobj);
		}
		return true;
	}
	return false;
}

bool ProtoMap::LoadTextFormat(const char* buf)
{
	IniParser map_ini;
	map_ini.LoadFilePtr(buf,strlen(buf));

	// Header
	ZeroMemory(&Header,sizeof(Header));
	char* header_str=map_ini.GetApp(APP_HEADER);
	if(header_str)
	{
		istrstream istr(header_str);
		string field,value;
		int ivalue;
		while(!istr.eof() && !istr.fail())
		{
			istr >> field >> value;
			if(!istr.fail())
			{
				ivalue=atoi(value.c_str());
				if(field=="Version") Header.Version=(ivalue<<20);
				if(field=="MaxHexX") Header.MaxHexX=ivalue;
				if(field=="MaxHexY") Header.MaxHexY=ivalue;
				if(field=="CenterX") Header.CenterX=ivalue;
				if(field=="CenterY") Header.CenterY=ivalue;
				if(field=="PlayersLimit") Header.PlayersLimit=ivalue;
				if(field=="Time") Header.Time=ivalue;
				if(field=="NoLogOut") Header.NoLogOut=(ivalue!=0);
				if(field=="ScriptModule") StringCopy(Header.ScriptModule,value.c_str());
				if(field=="ScriptFunc") StringCopy(Header.ScriptFunc,value.c_str());
				if(field=="DayTime0") Header.DayTime[0]=ivalue;
				if(field=="DayTime1") Header.DayTime[1]=ivalue;
				if(field=="DayTime2") Header.DayTime[2]=ivalue;
				if(field=="DayTime3") Header.DayTime[3]=ivalue;
				if(field=="DayColorR0") Header.DayColor[0]=ivalue;
				if(field=="DayColorR1") Header.DayColor[1]=ivalue;
				if(field=="DayColorR2") Header.DayColor[2]=ivalue;
				if(field=="DayColorR3") Header.DayColor[3]=ivalue;
				if(field=="DayColorG0") Header.DayColor[4]=ivalue;
				if(field=="DayColorG1") Header.DayColor[5]=ivalue;
				if(field=="DayColorG2") Header.DayColor[6]=ivalue;
				if(field=="DayColorG3") Header.DayColor[7]=ivalue;
				if(field=="DayColorB0") Header.DayColor[8]=ivalue;
				if(field=="DayColorB1") Header.DayColor[9]=ivalue;
				if(field=="DayColorB2") Header.DayColor[10]=ivalue;
				if(field=="DayColorB3") Header.DayColor[11]=ivalue;
			}
		}
		delete[] header_str;
	}
	if((Header.Version!=FO_MAP_VERSION_V6 && Header.Version!=FO_MAP_VERSION_V7 && Header.Version!=FO_MAP_VERSION_V8 && Header.Version!=FO_MAP_VERSION_V9) ||
		Header.MaxHexX<1 || Header.MaxHexY<1) return false;

	// Tiles
	Tiles=new DWORD[GetTilesSize()/sizeof(DWORD)];
	if(!Tiles) return false;
	char* tiles_str=map_ini.GetApp(APP_TILES);
	if(tiles_str)
	{
		istrstream istr(tiles_str);
		string type,name;
		while(!istr.eof() && !istr.fail())
		{
			DWORD tx,ty;
			istr >> type >> tx >> ty >> name;
			if(!istr.fail() && tx>=0 && tx<Header.MaxHexX/2 && ty>=0 && ty<Header.MaxHexY/2)
			{
				if(type=="tile") Tiles[ty*(Header.MaxHexX/2)*2+tx*2]=Str::GetHash(name.c_str());
				else if(type=="roof") Tiles[ty*(Header.MaxHexX/2)*2+tx*2+1]=Str::GetHash(name.c_str());
				else if(type=="terrain")
				{
					Tiles[ty*(Header.MaxHexX/2)*2+tx*2+1]=0xAAAAAAAA;
					Tiles[ty*(Header.MaxHexX/2)*2+tx*2]=Str::GetHash(name.c_str());
				}
				else if(type=="0") Tiles[ty*(Header.MaxHexX/2)*2+tx*2]=(DWORD)_atoi64(name.c_str()); // Deprecated
				else if(type=="1") Tiles[ty*(Header.MaxHexX/2)*2+tx*2+1]=(DWORD)_atoi64(name.c_str()); // Deprecated
			}
		}
		delete[] tiles_str;
	}

	// Objects
	char* objects_str=map_ini.GetApp(APP_OBJECTS);
	if(objects_str)
	{
		istrstream istr(objects_str);
		string field,value;
		int ivalue;
		while(!istr.eof() && !istr.fail())
		{
			istr >> field >> value;
			if(!istr.fail())
			{
				if(field!="ScriptName" && field!="FuncName") ivalue=_atoi64(value.c_str());

				if(field=="MapObjType")
				{
					MapObject* mobj=new MapObject();

					mobj->MapObjType=ivalue;
					if(ivalue==MAP_OBJECT_CRITTER)
					{
						mobj->MCritter.Cond=COND_LIFE;
						mobj->MCritter.CondExt=COND_LIFE_NONE;
						for(int i=0;i<MAPOBJ_CRITTER_PARAMS;i++) mobj->MCritter.ParamIndex[i]=-1;
					}
					else if(ivalue!=MAP_OBJECT_ITEM && ivalue!=MAP_OBJECT_SCENERY)
					{
						continue;
					}

					MObjects.push_back(mobj);
				}
				else if(MObjects.size())
				{
					MapObject& mobj=*MObjects.back();
					// Shared
					if(field=="ProtoId") mobj.ProtoId=ivalue;
					else if(field=="MapX") mobj.MapX=ivalue;
					else if(field=="MapY") mobj.MapY=ivalue;
					else if(field=="Dir") mobj.Dir=ivalue;
					else if(field=="LightColor") mobj.LightColor=ivalue;
					else if(field=="LightDay") mobj.LightDay=ivalue;
					else if(field=="LightDirOff") mobj.LightDirOff=ivalue;
					else if(field=="LightDistance") mobj.LightDistance=ivalue;
					else if(field=="LightIntensity") mobj.LightIntensity=ivalue;
					else if(field=="ScriptName") StringCopy(mobj.ScriptName,value.c_str());
					else if(field=="FuncName") StringCopy(mobj.FuncName,value.c_str());
					else if(field=="UserData0") mobj.UserData[0]=ivalue;
					else if(field=="UserData1") mobj.UserData[1]=ivalue;
					else if(field=="UserData2") mobj.UserData[2]=ivalue;
					else if(field=="UserData3") mobj.UserData[3]=ivalue;
					else if(field=="UserData4") mobj.UserData[4]=ivalue;
					else if(field=="UserData5") mobj.UserData[5]=ivalue;
					else if(field=="UserData6") mobj.UserData[6]=ivalue;
					else if(field=="UserData7") mobj.UserData[7]=ivalue;
					else if(field=="UserData8") mobj.UserData[8]=ivalue;
					else if(field=="UserData9") mobj.UserData[9]=ivalue;
					// Critter
					else if(mobj.MapObjType==MAP_OBJECT_CRITTER)
					{
						if(field=="Critter_Cond") mobj.MCritter.Cond=ivalue;
						else if(field=="Critter_CondExt") mobj.MCritter.CondExt=ivalue;
						else if(field=="Critter_ParamIndex0") mobj.MCritter.ParamIndex[0]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex1") mobj.MCritter.ParamIndex[1]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex2") mobj.MCritter.ParamIndex[2]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex3") mobj.MCritter.ParamIndex[3]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex4") mobj.MCritter.ParamIndex[4]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex5") mobj.MCritter.ParamIndex[5]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex6") mobj.MCritter.ParamIndex[6]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex7") mobj.MCritter.ParamIndex[7]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex8") mobj.MCritter.ParamIndex[8]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex9") mobj.MCritter.ParamIndex[9]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex10") mobj.MCritter.ParamIndex[10]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex11") mobj.MCritter.ParamIndex[11]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex12") mobj.MCritter.ParamIndex[12]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex13") mobj.MCritter.ParamIndex[13]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamIndex14") mobj.MCritter.ParamIndex[14]=FONames::GetParamId(value.c_str());
						else if(field=="Critter_ParamValue0") mobj.MCritter.ParamValue[0]=ivalue;
						else if(field=="Critter_ParamValue1") mobj.MCritter.ParamValue[1]=ivalue;
						else if(field=="Critter_ParamValue2") mobj.MCritter.ParamValue[2]=ivalue;
						else if(field=="Critter_ParamValue3") mobj.MCritter.ParamValue[3]=ivalue;
						else if(field=="Critter_ParamValue4") mobj.MCritter.ParamValue[4]=ivalue;
						else if(field=="Critter_ParamValue5") mobj.MCritter.ParamValue[5]=ivalue;
						else if(field=="Critter_ParamValue6") mobj.MCritter.ParamValue[6]=ivalue;
						else if(field=="Critter_ParamValue7") mobj.MCritter.ParamValue[7]=ivalue;
						else if(field=="Critter_ParamValue8") mobj.MCritter.ParamValue[8]=ivalue;
						else if(field=="Critter_ParamValue9") mobj.MCritter.ParamValue[9]=ivalue;
						else if(field=="Critter_ParamValue10") mobj.MCritter.ParamValue[10]=ivalue;
						else if(field=="Critter_ParamValue11") mobj.MCritter.ParamValue[11]=ivalue;
						else if(field=="Critter_ParamValue12") mobj.MCritter.ParamValue[12]=ivalue;
						else if(field=="Critter_ParamValue13") mobj.MCritter.ParamValue[13]=ivalue;
						else if(field=="Critter_ParamValue14") mobj.MCritter.ParamValue[14]=ivalue;
					}
					// Item/Scenery
					else if(mobj.MapObjType==MAP_OBJECT_ITEM || mobj.MapObjType==MAP_OBJECT_SCENERY)
					{
						// Shared parameters
						if(field=="OffsetX") mobj.MItem.OffsetX=ivalue;
						else if(field=="OffsetY") mobj.MItem.OffsetY=ivalue;
						else if(field=="AnimStayBegin") mobj.MItem.AnimStayBegin=ivalue;
						else if(field=="AnimStayEnd") mobj.MItem.AnimStayEnd=ivalue;
						else if(field=="AnimWait") mobj.MItem.AnimWait=ivalue;
						else if(field=="PicMapName")
						{
#ifdef FONLINE_MAPPER
							StringCopy(mobj.RunTime.PicMapName,value.c_str());
#endif
							mobj.MItem.PicMapHash=Str::GetHash(value.c_str());
						}
						else if(field=="PicInvName")
						{
#ifdef FONLINE_MAPPER
							StringCopy(mobj.RunTime.PicInvName,value.c_str());
#endif
							mobj.MItem.PicInvHash=Str::GetHash(value.c_str());
						}
						else if(field=="InfoOffset") mobj.MItem.InfoOffset=ivalue;
						// Item
						else if(mobj.MapObjType==MAP_OBJECT_ITEM)
						{
							if(field=="Item_InfoOffset") mobj.MItem.InfoOffset=ivalue;
							else if(field=="Item_Count") mobj.MItem.Count=ivalue;
							else if(field=="Item_DeteorationFlags") mobj.MItem.DeteorationFlags=ivalue;
							else if(field=="Item_DeteorationCount") mobj.MItem.DeteorationCount=ivalue;
							else if(field=="Item_DeteorationValue") mobj.MItem.DeteorationValue=ivalue;
							else if(field=="Item_InContainer") mobj.MItem.InContainer=(ivalue!=0);
							else if(field=="Item_ItemSlot") mobj.MItem.ItemSlot=ivalue;
							else if(field=="Item_AmmoPid") mobj.MItem.AmmoPid=ivalue;
							else if(field=="Item_AmmoCount") mobj.MItem.AmmoCount=ivalue;
							else if(field=="Item_LockerDoorId") mobj.MItem.LockerDoorId=ivalue;
							else if(field=="Item_LockerCondition") mobj.MItem.LockerCondition=ivalue;
							else if(field=="Item_LockerComplexity") mobj.MItem.LockerComplexity=ivalue;
							else if(field=="Item_TrapValue") mobj.MItem.TrapValue=ivalue;
							else if(field=="Item_Val0") mobj.MItem.Val[0]=ivalue;
							else if(field=="Item_Val1") mobj.MItem.Val[1]=ivalue;
							else if(field=="Item_Val2") mobj.MItem.Val[2]=ivalue;
							else if(field=="Item_Val3") mobj.MItem.Val[3]=ivalue;
							else if(field=="Item_Val4") mobj.MItem.Val[4]=ivalue;
							else if(field=="Item_Val5") mobj.MItem.Val[5]=ivalue;
							else if(field=="Item_Val6") mobj.MItem.Val[6]=ivalue;
							else if(field=="Item_Val7") mobj.MItem.Val[7]=ivalue;
							else if(field=="Item_Val8") mobj.MItem.Val[8]=ivalue;
							else if(field=="Item_Val9") mobj.MItem.Val[9]=ivalue;
						}
						// Scenery
						else if(mobj.MapObjType==MAP_OBJECT_SCENERY)
						{
							if(field=="Scenery_CanUse") mobj.MScenery.CanUse=(ivalue!=0);
							else if(field=="Scenery_CanTalk") mobj.MScenery.CanTalk=(ivalue!=0);
							else if(field=="Scenery_TriggerNum") mobj.MScenery.TriggerNum=ivalue;
							else if(field=="Scenery_ParamsCount") mobj.MScenery.ParamsCount=ivalue;
							else if(field=="Scenery_Param0") mobj.MScenery.Param[0]=ivalue;
							else if(field=="Scenery_Param1") mobj.MScenery.Param[1]=ivalue;
							else if(field=="Scenery_Param2") mobj.MScenery.Param[2]=ivalue;
							else if(field=="Scenery_Param3") mobj.MScenery.Param[3]=ivalue;
							else if(field=="Scenery_Param4") mobj.MScenery.Param[4]=ivalue;
							else if(field=="Scenery_ToMapPid") mobj.MScenery.ToMapPid=ivalue;
							else if(field=="Scenery_ToEntire") mobj.MScenery.ToEntire=ivalue;
							else if(field=="Scenery_ToMapX") mobj.MScenery.ToMapX=ivalue;
							else if(field=="Scenery_ToMapY") mobj.MScenery.ToMapY=ivalue;
							else if(field=="Scenery_ToDir") mobj.MScenery.ToDir=ivalue;
							else if(field=="Scenery_SpriteCut") mobj.MScenery.SpriteCut=ivalue;
						}
					}
				}
			}
		}
		delete[] objects_str;
	}

	return true;
}

#ifdef FONLINE_MAPPER
void ProtoMap::SaveTextFormat(FileManager* fm)
{
	// Header
	fm->SetStr("[%s]\n",APP_HEADER);
	fm->SetStr("%-20s %d\n","Version",FO_MAP_VERSION_V9>>20);
	fm->SetStr("%-20s %d\n","MaxHexX",Header.MaxHexX);
	fm->SetStr("%-20s %d\n","MaxHexY",Header.MaxHexY);
	fm->SetStr("%-20s %d\n","CenterX",Header.CenterX);
	fm->SetStr("%-20s %d\n","CenterY",Header.CenterY);
	fm->SetStr("%-20s %d\n","PlayersLimit",Header.PlayersLimit);
	fm->SetStr("%-20s %d\n","Time",Header.Time);
	fm->SetStr("%-20s %d\n","DayTime0",Header.DayTime[0]);
	fm->SetStr("%-20s %d\n","DayTime1",Header.DayTime[1]);
	fm->SetStr("%-20s %d\n","DayTime2",Header.DayTime[2]);
	fm->SetStr("%-20s %d\n","DayTime3",Header.DayTime[3]);
	fm->SetStr("%-20s %d\n","DayColorR0",Header.DayColor[0]);
	fm->SetStr("%-20s %d\n","DayColorR1",Header.DayColor[1]);
	fm->SetStr("%-20s %d\n","DayColorR2",Header.DayColor[2]);
	fm->SetStr("%-20s %d\n","DayColorR3",Header.DayColor[3]);
	fm->SetStr("%-20s %d\n","DayColorG0",Header.DayColor[4]);
	fm->SetStr("%-20s %d\n","DayColorG1",Header.DayColor[5]);
	fm->SetStr("%-20s %d\n","DayColorG2",Header.DayColor[6]);
	fm->SetStr("%-20s %d\n","DayColorG3",Header.DayColor[7]);
	fm->SetStr("%-20s %d\n","DayColorB0",Header.DayColor[8]);
	fm->SetStr("%-20s %d\n","DayColorB1",Header.DayColor[9]);
	fm->SetStr("%-20s %d\n","DayColorB2",Header.DayColor[10]);
	fm->SetStr("%-20s %d\n","DayColorB3",Header.DayColor[11]);
	fm->SetStr("\n");

	// Tiles
	fm->SetStr("[%s]\n",APP_TILES);
	for(int x=0,x_=Header.MaxHexX/2;x<x_;x++)
	{
		for(int y=0,y_=Header.MaxHexY/2;y<y_;y++)
		{
			DWORD tile=GetTile(x,y);
			DWORD roof=GetRoof(x,y);

			// Terrain
			if(roof==0xAAAAAAAA)
			{
				const char* name=ResMngr.GetName(tile);
				if(name) fm->SetStr("%-10s %-4d %-4d %s\n","terrain",x,y,name);
			}
			// Simple tiles
			else
			{
				if(tile)
				{
					const char* name=ResMngr.GetName(tile);
					if(name) fm->SetStr("%-10s %-4d %-4d %s\n","tile",x,y,name);
				}
				if(roof)
				{
					const char* name=ResMngr.GetName(roof);
					if(name) fm->SetStr("%-10s %-4d %-4d %s\n","roof",x,y,name);
				}
			}
		}
	}
	fm->SetStr("\n");

	// Objects
	fm->SetStr("[%s]\n",APP_OBJECTS);
	for(size_t k=0;k<MObjects.size();k++)
	{
		MapObject& mobj=*MObjects[k];
		// Shared
		fm->SetStr("%-20s %d\n","MapObjType",mobj.MapObjType);
		fm->SetStr("%-20s %d\n","ProtoId",mobj.ProtoId);
		if(mobj.MapX) fm->SetStr("%-20s %d\n","MapX",mobj.MapX);
		if(mobj.MapY) fm->SetStr("%-20s %d\n","MapY",mobj.MapY);
		if(mobj.Dir) fm->SetStr("%-20s %d\n","Dir",mobj.Dir);
		if(mobj.LightColor) fm->SetStr("%-20s %d\n","LightColor",mobj.LightColor);
		if(mobj.LightDay) fm->SetStr("%-20s %d\n","LightDay",mobj.LightDay);
		if(mobj.LightDirOff) fm->SetStr("%-20s %d\n","LightDirOff",mobj.LightDirOff);
		if(mobj.LightDistance) fm->SetStr("%-20s %d\n","LightDistance",mobj.LightDistance);
		if(mobj.LightIntensity) fm->SetStr("%-20s %d\n","LightIntensity",mobj.LightIntensity);
		if(mobj.ScriptName[0]) fm->SetStr("%-20s %s\n","ScriptName",mobj.ScriptName);
		if(mobj.FuncName[0]) fm->SetStr("%-20s %s\n","FuncName",mobj.FuncName);
		if(mobj.UserData[0]) fm->SetStr("%-20s %d\n","UserData0",mobj.UserData[0]);
		if(mobj.UserData[1]) fm->SetStr("%-20s %d\n","UserData1",mobj.UserData[1]);
		if(mobj.UserData[2]) fm->SetStr("%-20s %d\n","UserData2",mobj.UserData[2]);
		if(mobj.UserData[3]) fm->SetStr("%-20s %d\n","UserData3",mobj.UserData[3]);
		if(mobj.UserData[4]) fm->SetStr("%-20s %d\n","UserData4",mobj.UserData[4]);
		if(mobj.UserData[5]) fm->SetStr("%-20s %d\n","UserData5",mobj.UserData[5]);
		if(mobj.UserData[6]) fm->SetStr("%-20s %d\n","UserData6",mobj.UserData[6]);
		if(mobj.UserData[7]) fm->SetStr("%-20s %d\n","UserData7",mobj.UserData[7]);
		if(mobj.UserData[8]) fm->SetStr("%-20s %d\n","UserData8",mobj.UserData[8]);
		if(mobj.UserData[9]) fm->SetStr("%-20s %d\n","UserData9",mobj.UserData[9]);
		// Critter
		if(mobj.MapObjType==MAP_OBJECT_CRITTER)
		{
			fm->SetStr("%-20s %d\n","Critter_Cond",mobj.MCritter.Cond);
			fm->SetStr("%-20s %d\n","Critter_CondExt",mobj.MCritter.CondExt);
			for(int i=0;i<MAPOBJ_CRITTER_PARAMS;i++)
			{
				if(mobj.MCritter.ParamIndex[i]>=0 && mobj.MCritter.ParamIndex[i]<MAX_PARAMS)
				{
					const char* param_name=FONames::GetParamName(mobj.MCritter.ParamIndex[i]);
					if(param_name)
					{
						char str[128];
						sprintf(str,"Critter_ParamIndex%d",i);
						fm->SetStr("%-20s %s\n",str,param_name);
						sprintf(str,"Critter_ParamValue%d",i);
						fm->SetStr("%-20s %d\n",str,mobj.MCritter.ParamValue[i]);
					}
				}
			}
		}
		// Item
		else if(mobj.MapObjType==MAP_OBJECT_ITEM || mobj.MapObjType==MAP_OBJECT_SCENERY)
		{
			if(mobj.MItem.OffsetX) fm->SetStr("%-20s %d\n","OffsetX",mobj.MItem.OffsetX);
			if(mobj.MItem.OffsetY) fm->SetStr("%-20s %d\n","OffsetY",mobj.MItem.OffsetY);
			if(mobj.MItem.AnimStayBegin) fm->SetStr("%-20s %d\n","AnimStayBegin",mobj.MItem.AnimStayBegin);
			if(mobj.MItem.AnimStayEnd) fm->SetStr("%-20s %d\n","AnimStayEnd",mobj.MItem.AnimStayEnd);
			if(mobj.MItem.AnimWait) fm->SetStr("%-20s %d\n","AnimWait",mobj.MItem.AnimWait);
			if(mobj.RunTime.PicMapName[0]) fm->SetStr("%-20s %s\n","PicMapName",mobj.RunTime.PicMapName);
			if(mobj.RunTime.PicInvName[0]) fm->SetStr("%-20s %s\n","PicInvName",mobj.RunTime.PicInvName);
			if(mobj.MItem.InfoOffset) fm->SetStr("%-20s %d\n","InfoOffset",mobj.MItem.InfoOffset);
			if(mobj.MapObjType==MAP_OBJECT_ITEM)
			{
				if(mobj.MItem.Count) fm->SetStr("%-20s %d\n","Item_Count",mobj.MItem.Count);
				if(mobj.MItem.DeteorationFlags) fm->SetStr("%-20s %d\n","Item_DeteorationFlags",mobj.MItem.DeteorationFlags);
				if(mobj.MItem.DeteorationCount) fm->SetStr("%-20s %d\n","Item_DeteorationCount",mobj.MItem.DeteorationCount);
				if(mobj.MItem.DeteorationValue) fm->SetStr("%-20s %d\n","Item_DeteorationValue",mobj.MItem.DeteorationValue);
				if(mobj.MItem.InContainer) fm->SetStr("%-20s %d\n","Item_InContainer",mobj.MItem.InContainer);
				if(mobj.MItem.ItemSlot) fm->SetStr("%-20s %d\n","Item_ItemSlot",mobj.MItem.ItemSlot);
				if(mobj.MItem.AmmoPid) fm->SetStr("%-20s %d\n","Item_AmmoPid",mobj.MItem.AmmoPid);
				if(mobj.MItem.AmmoCount) fm->SetStr("%-20s %d\n","Item_AmmoCount",mobj.MItem.AmmoCount);
				if(mobj.MItem.LockerDoorId) fm->SetStr("%-20s %d\n","Item_LockerDoorId",mobj.MItem.LockerDoorId);
				if(mobj.MItem.LockerCondition) fm->SetStr("%-20s %d\n","Item_LockerCondition",mobj.MItem.LockerCondition);
				if(mobj.MItem.LockerComplexity) fm->SetStr("%-20s %d\n","Item_LockerComplexity",mobj.MItem.LockerComplexity);
				if(mobj.MItem.TrapValue) fm->SetStr("%-20s %d\n","Item_TrapValue",mobj.MItem.TrapValue);
				if(mobj.MItem.Val[0]) fm->SetStr("%-20s %d\n","Item_Val0",mobj.MItem.Val[0]);
				if(mobj.MItem.Val[1]) fm->SetStr("%-20s %d\n","Item_Val1",mobj.MItem.Val[1]);
				if(mobj.MItem.Val[2]) fm->SetStr("%-20s %d\n","Item_Val2",mobj.MItem.Val[2]);
				if(mobj.MItem.Val[3]) fm->SetStr("%-20s %d\n","Item_Val3",mobj.MItem.Val[3]);
				if(mobj.MItem.Val[4]) fm->SetStr("%-20s %d\n","Item_Val4",mobj.MItem.Val[4]);
				if(mobj.MItem.Val[5]) fm->SetStr("%-20s %d\n","Item_Val5",mobj.MItem.Val[5]);
				if(mobj.MItem.Val[6]) fm->SetStr("%-20s %d\n","Item_Val6",mobj.MItem.Val[6]);
				if(mobj.MItem.Val[7]) fm->SetStr("%-20s %d\n","Item_Val7",mobj.MItem.Val[7]);
				if(mobj.MItem.Val[8]) fm->SetStr("%-20s %d\n","Item_Val8",mobj.MItem.Val[8]);
				if(mobj.MItem.Val[9]) fm->SetStr("%-20s %d\n","Item_Val9",mobj.MItem.Val[9]);
			}
			// Scenery
			else if(mobj.MapObjType==MAP_OBJECT_SCENERY)
			{
				if(mobj.MScenery.CanUse) fm->SetStr("%-20s %d\n","Scenery_CanUse",mobj.MScenery.CanUse);
				if(mobj.MScenery.CanTalk) fm->SetStr("%-20s %d\n","Scenery_CanTalk",mobj.MScenery.CanTalk);
				if(mobj.MScenery.TriggerNum) fm->SetStr("%-20s %d\n","Scenery_TriggerNum",mobj.MScenery.TriggerNum);
				if(mobj.MScenery.ParamsCount) fm->SetStr("%-20s %d\n","Scenery_ParamsCount",mobj.MScenery.ParamsCount);
				if(mobj.MScenery.Param[0]) fm->SetStr("%-20s %d\n","Scenery_Param0",mobj.MScenery.Param[0]);
				if(mobj.MScenery.Param[1]) fm->SetStr("%-20s %d\n","Scenery_Param1",mobj.MScenery.Param[1]);
				if(mobj.MScenery.Param[2]) fm->SetStr("%-20s %d\n","Scenery_Param2",mobj.MScenery.Param[2]);
				if(mobj.MScenery.Param[3]) fm->SetStr("%-20s %d\n","Scenery_Param3",mobj.MScenery.Param[3]);
				if(mobj.MScenery.Param[4]) fm->SetStr("%-20s %d\n","Scenery_Param4",mobj.MScenery.Param[4]);
				if(mobj.MScenery.ToMapPid) fm->SetStr("%-20s %d\n","Scenery_ToMapPid",mobj.MScenery.ToMapPid);
				if(mobj.MScenery.ToEntire) fm->SetStr("%-20s %d\n","Scenery_ToEntire",mobj.MScenery.ToEntire);
				if(mobj.MScenery.ToMapX) fm->SetStr("%-20s %d\n","Scenery_ToMapX",mobj.MScenery.ToMapX);
				if(mobj.MScenery.ToMapY) fm->SetStr("%-20s %d\n","Scenery_ToMapY",mobj.MScenery.ToMapY);
				if(mobj.MScenery.ToDir) fm->SetStr("%-20s %d\n","Scenery_ToDir",mobj.MScenery.ToDir);
				if(mobj.MScenery.SpriteCut) fm->SetStr("%-20s %d\n","SpriteCut",mobj.MScenery.SpriteCut);
			}
		}
		fm->SetStr("\n");
	}
	fm->SetStr("\n");
}
#endif

#ifdef FONLINE_SERVER
bool ProtoMap::LoadCache(FileManager* fm)
{
	// Server version
	DWORD version=fm->GetBEDWord();
	if(version!=SERVER_VERSION) return false;
	fm->GetBEDWord();
	fm->GetBEDWord();
	fm->GetBEDWord();

	// Header
	if(!fm->CopyMem(&Header,sizeof(Header))) return false;

	// Tiles
	Tiles=new DWORD[GetTilesSize()/sizeof(DWORD)];
	if(!Tiles) return false;
	fm->CopyMem(Tiles,GetTilesSize());

	// Critters
	DWORD count=fm->GetBEDWord();
	CrittersVec.reserve(count);
	for(DWORD i=0;i<count;i++)
	{
		MapObject* mobj=new MapObject();
		fm->CopyMem(mobj,sizeof(MapObject)-sizeof(MapObject::_RunTime));
		CrittersVec.push_back(mobj);
	}

	// Items
	count=fm->GetBEDWord();
	ItemsVec.reserve(count);
	for(DWORD i=0;i<count;i++)
	{
		MapObject* mobj=new MapObject();
		fm->CopyMem(mobj,sizeof(MapObject)-sizeof(MapObject::_RunTime));
		ItemsVec.push_back(mobj);
	}

	// Scenery
	count=fm->GetBEDWord();
	SceneryVec.reserve(count);
	for(DWORD i=0;i<count;i++)
	{
		MapObject* mobj=new MapObject();
		fm->CopyMem(mobj,sizeof(MapObject));
		SceneryVec.push_back(mobj);
		mobj->RunTime.RefCounter=1;
		mobj->RunTime.BindScriptId=0;
		if(mobj->ScriptName[0] && mobj->FuncName[0]) BindSceneryScript(mobj);
	}

	// Grids
	count=fm->GetBEDWord();
	GridsVec.reserve(count);
	for(DWORD i=0;i<count;i++)
	{
		MapObject* mobj=new MapObject();
		fm->CopyMem(mobj,sizeof(MapObject));
		GridsVec.push_back(mobj);
		mobj->RunTime.RefCounter=1;
		mobj->RunTime.BindScriptId=0;
	}

	// To send
	count=fm->GetBEDWord();
	if(count)
	{
		WallsToSend.resize(count);
		if(!fm->CopyMem(&WallsToSend[0],count*sizeof(ScenToSend))) return false;
	}
	count=fm->GetBEDWord();
	if(count)
	{
		SceneriesToSend.resize(count);
		if(!fm->CopyMem(&SceneriesToSend[0],count*sizeof(ScenToSend))) return false;
	}

	// Hashes
	HashTiles=fm->GetBEDWord();
	HashWalls=fm->GetBEDWord();
	HashScen=fm->GetBEDWord();

	// Hex flags
	HexFlags=new BYTE[Header.MaxHexX*Header.MaxHexY];
	if(!HexFlags) return false;
	if(!fm->CopyMem(HexFlags,Header.MaxHexX*Header.MaxHexY)) return false;

	// Entires
	count=fm->GetBEDWord();
	if(count)
	{
		mapEntires.resize(count);
		if(!fm->CopyMem(&mapEntires[0],count*sizeof(MapEntire))) return false;
	}

	MEMORY_PROCESS(MEMORY_PROTO_MAP,SceneriesToSend.capacity()*sizeof(ScenToSend));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,WallsToSend.capacity()*sizeof(ScenToSend));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,mapEntires.capacity()*sizeof(MapEntire));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,CrittersVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,ItemsVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,SceneryVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,GridsVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,Header.MaxHexX*Header.MaxHexY);
	MEMORY_PROCESS(MEMORY_PROTO_MAP,GetTilesSize());
	return true;
}

void ProtoMap::SaveCache(FileManager* fm)
{
	// Server version
	fm->SetBEDWord(SERVER_VERSION);
	fm->SetBEDWord(0);
	fm->SetBEDWord(0);
	fm->SetBEDWord(0);

	// Header
	fm->SetData(&Header,sizeof(Header));

	// Tiles
	fm->SetData(Tiles,GetTilesSize());

	// Critters
	fm->SetBEDWord(CrittersVec.size());
	for(MapObjectPtrVecIt it=CrittersVec.begin(),end=CrittersVec.end();it!=end;++it)
		fm->SetData(*it,sizeof(MapObject)-sizeof(MapObject::_RunTime));

	// Items
	fm->SetBEDWord(ItemsVec.size());
	for(MapObjectPtrVecIt it=ItemsVec.begin(),end=ItemsVec.end();it!=end;++it)
		fm->SetData(*it,sizeof(MapObject)-sizeof(MapObject::_RunTime));

	// Scenery
	fm->SetBEDWord(SceneryVec.size());
	for(MapObjectPtrVecIt it=SceneryVec.begin(),end=SceneryVec.end();it!=end;++it)
		fm->SetData(*it,sizeof(MapObject));

	// Grids
	fm->SetBEDWord(GridsVec.size());
	for(MapObjectPtrVecIt it=GridsVec.begin(),end=GridsVec.end();it!=end;++it)
		fm->SetData(*it,sizeof(MapObject));

	// To send
	fm->SetBEDWord(WallsToSend.size());
	fm->SetData(&WallsToSend[0],WallsToSend.size()*sizeof(ScenToSend));
	fm->SetBEDWord(SceneriesToSend.size());
	fm->SetData(&SceneriesToSend[0],SceneriesToSend.size()*sizeof(ScenToSend));

	// Hashes
	fm->SetBEDWord(HashTiles);
	fm->SetBEDWord(HashWalls);
	fm->SetBEDWord(HashScen);

	// Hex flags
	fm->SetData(HexFlags,Header.MaxHexX*Header.MaxHexY);

	// Entires
	fm->SetBEDWord(mapEntires.size());
	fm->SetData(&mapEntires[0],mapEntires.size()*sizeof(MapEntire));

	// Save
	char fname[MAX_PATH];
	sprintf(fname,"%s.mapb",pmapName.c_str());
	fm->SaveOutBufToFile(fname,pathType);
}

void ProtoMap::BindSceneryScript(MapObject* mobj)
{
//============================================================
#define BIND_SCENERY_FUNC(params) \
	if(mobj->ProtoId!=SP_SCEN_TRIGGER) \
		mobj->RunTime.BindScriptId=Script::Bind(mobj->ScriptName,mobj->FuncName,"bool %s(Critter&,Scenery&,int,Item@"params,false); \
	else \
		mobj->RunTime.BindScriptId=Script::Bind(mobj->ScriptName,mobj->FuncName,"void %s(Critter&,Scenery&,bool,uint8"params,false)
//============================================================

	switch(mobj->MScenery.ParamsCount)
	{
	case 1: BIND_SCENERY_FUNC(",int)"); break;
	case 2: BIND_SCENERY_FUNC(",int,int)"); break;
	case 3: BIND_SCENERY_FUNC(",int,int,int)"); break;
	case 4: BIND_SCENERY_FUNC(",int,int,int,int)"); break;
	case 5: BIND_SCENERY_FUNC(",int,int,int,int,int)"); break;
	default: BIND_SCENERY_FUNC(")"); break;
	}

	if(mobj->RunTime.BindScriptId<=0)
	{
		char map_info[128];
		sprintf(map_info,"pid<%u>, name<%s>",GetPid(),pmapName.c_str());
		WriteLog(__FUNCTION__" - Map<%s>, Can't bind scenery function<%s> in module<%s>. Scenery hexX<%u>, hexY<%u>.\n",map_info,
			mobj->FuncName,mobj->ScriptName,mobj->MapX,mobj->MapY);
		mobj->RunTime.BindScriptId=0;
	}
}
#endif // FONLINE_SERVER

bool ProtoMap::Refresh()
{
	if(!IsInit()) return false;

	char map_info[128];
	sprintf(map_info,"pid<%u>, name<%s>",GetPid(),pmapName.c_str());

	// Read
	string fname_txt=pmapName+".txt";
	string fname_map=pmapName+".map";
	string fname_bin=pmapName+".mapb";

#ifdef FONLINE_SERVER
	// Cached binary
	FileManager cached;
	cached.LoadFile(fname_bin.c_str(),pathType);

	// Load binary or text
	bool text=false;
	if(!pmapFm->LoadFile(fname_map.c_str(),pathType))
	{
		text=true;
		if(!pmapFm->LoadFile(fname_txt.c_str(),pathType) && !cached.IsLoaded())
		{
			WriteLog(__FUNCTION__" - Load file fail, file name<%s>, folder<%s>.\n",pmapName.c_str(),pmapFm->GetPath(pathType));
			return false;
		}
	}

	// Process cache
	if(cached.IsLoaded())
	{
		bool load_cache=true;
		if(pmapFm->IsLoaded())
		{
			FILETIME last_write,last_write_cache;
			pmapFm->GetTime(NULL,NULL,&last_write);
			cached.GetTime(NULL,NULL,&last_write_cache);
			if(CompareFileTime(&last_write,&last_write_cache)>0) load_cache=false;
		}

		if(load_cache)
		{
			if(LoadCache(&cached)) return true;

			if(!pmapFm->IsLoaded())
			{
				WriteLog(__FUNCTION__" - Map<%s>. Can't read cached map file.\n",map_info);
				return false;
			}
		}
	}
#endif // FONLINE_SERVER
#ifdef FONLINE_MAPPER
	// Load binary or text
	bool text=false;
	if(!pmapFm->LoadFile(fname_map.c_str(),pathType))
	{
		text=true;
		if(!pmapFm->LoadFile(fname_txt.c_str(),pathType))
		{
			WriteLog(__FUNCTION__" - Load file fail, file name<%s>, folder<%s>.\n",pmapName.c_str(),pmapFm->GetPath(pathType));
			return false;
		}
	}
#endif // FONLINE_MAPPER

	// Load
	if(text)
	{
		if(!LoadTextFormat((const char*)pmapFm->GetBuf()))
		{
			WriteLog(__FUNCTION__" - Map<%s>. Can't read text map format.\n",map_info);
			return false;
		}
	}
	else
	{
		// Check map format version
		// Read version
		DWORD version_full=pmapFm->GetLEDWord();

		// Check version
		if(version_full==F1_MAP_VERSION)
		{
			WriteLog(__FUNCTION__" - Map<%s>. FOnline not support F1 map format.\n",map_info);
			return false;
		}

		if(version_full==F2_MAP_VERSION)
		{
			WriteLog(__FUNCTION__" - Map<%s>. FOnline not support F2 map format.\n",map_info);
			return false;
		}

		int version=9; // Last version
		if(version_full==FO_MAP_VERSION_V1) version=-1; // Deprecated
		else if(version_full==FO_MAP_VERSION_V2) version=-2; // Deprecated
		else if(version_full==FO_MAP_VERSION_V3) version=-3; // Deprecated
		else if(version_full==FO_MAP_VERSION_V4) version=4;
		else if(version_full==FO_MAP_VERSION_V5) version=5;
		else if(version_full==FO_MAP_VERSION_V6) version=6;
		else if(version_full==FO_MAP_VERSION_V7) version=7;
		else if(version_full==FO_MAP_VERSION_V8) version=8;
		else if(version_full!=FO_MAP_VERSION_V9)
		{
			WriteLog(__FUNCTION__" - Map<%s>. Unknown map format<%u>.\n",map_info,version_full);
			return false;
		}

		if(version<0)
		{
			WriteLog(__FUNCTION__" - Map<%s>. Map format not supproted, resave in Mapper v.1.17.2, than open again.\n",map_info);
			return false;
		}

		// Read Header
		if(!ReadHeader(version))
		{
			WriteLog(__FUNCTION__" - Map<%s>. Can't read Header.\n",map_info);
			return false;
		}

		// Unpack
		if(Header.Packed)
		{
			pmapFm->SetCurPos(Header.HeaderSize);
			DWORD pack_len=pmapFm->GetFsize()-Header.HeaderSize;
			DWORD unpack_len=Header.UnpackedDataLen;
			BYTE* data=Crypt.Uncompress(pmapFm->GetCurBuf(),pack_len,unpack_len/pack_len+1);
			if(!data)
			{
				WriteLog(__FUNCTION__" - Map<%s>. Unable unpack data.\n",map_info);
				return false;
			}
			pmapFm->LoadStream(data,pack_len);
			delete[] data;
		}

		// Read Tiles
		if(!ReadTiles(version))
		{
			WriteLog(__FUNCTION__" - Map<%s>. Can't read Tiles.\n",map_info);
			return false;
		}

		// Read Objects
		if(!ReadObjects(version))	
		{
			WriteLog(__FUNCTION__" - Map<%s>. Can't read Objects.\n",map_info);
			return false;
		}
	}
	pmapFm->UnloadFile();

#ifdef FONLINE_SERVER
	// Parse objects
	WallsToSend.clear();
	SceneriesToSend.clear();
	WORD maxhx=Header.MaxHexX;
	WORD maxhy=Header.MaxHexY;

	HexFlags=new BYTE[Header.MaxHexX*Header.MaxHexY];
	if(!HexFlags) return false;
	ZeroMemory(HexFlags,Header.MaxHexX*Header.MaxHexY);

	for(MapObjectPtrVecIt it=MObjects.begin(),end=MObjects.end();it!=end;++it)
	{
		MapObject& mobj=*(*it);

		if(mobj.MapObjType==MAP_OBJECT_CRITTER)
		{
			mobj.AddRef();
			CrittersVec.push_back(&mobj);
			continue;
		}
		else if(mobj.MapObjType==MAP_OBJECT_ITEM)
		{
			mobj.AddRef();
			ItemsVec.push_back(&mobj);
			continue;
		}

		WORD pid=mobj.ProtoId;
		WORD hx=mobj.MapX;
		WORD hy=mobj.MapY;

		ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
		if(!proto_item)
		{
			WriteLog(__FUNCTION__" - Map<%s>, Unknown prototype<%u>, hexX<%u>, hexY<%u>.\n",map_info,pid,hx,hy);
			continue;
		}

		if(hx>=maxhx || hy>=maxhy)
		{
			WriteLog(__FUNCTION__" - Invalid object position on map<%s>, pid<%u>, hexX<%u>, hexY<%u>.\n",map_info,pid,hx,hy);
			continue;
		}

		WORD type=proto_item->GetType();
		switch(type)
		{
		case ITEM_TYPE_WALL:
			{
				if(!FLAG(proto_item->Flags,ITEM_NO_BLOCK)) SETFLAG(HexFlags[hy*maxhx+hx],FH_BLOCK);
				if(!FLAG(proto_item->Flags,ITEM_SHOOT_THRU))
				{
					SETFLAG(HexFlags[hy*maxhx+hx],FH_BLOCK);
					SETFLAG(HexFlags[hy*maxhx+hx],FH_NOTRAKE);
				}

				SETFLAG(HexFlags[hy*maxhx+hx],FH_WALL);

				// To client
				ScenToSend cur_wall;
				ZeroMemory(&cur_wall,sizeof(ScenToSend));

				cur_wall.ProtoId=mobj.ProtoId;
				cur_wall.MapX=mobj.MapX;
				cur_wall.MapY=mobj.MapY;
				cur_wall.Dir=mobj.Dir;
				cur_wall.OffsetX=mobj.MScenery.OffsetX;
				cur_wall.OffsetY=mobj.MScenery.OffsetY;
				cur_wall.LightColor=mobj.LightColor;
				cur_wall.LightDistance=mobj.LightDistance;
				cur_wall.LightFlags=mobj.LightDirOff|((mobj.LightDay&3)<<6);
				cur_wall.LightIntensity=mobj.LightIntensity;
				cur_wall.InfoOffset=mobj.MScenery.InfoOffset;
				cur_wall.AnimStayBegin=mobj.MScenery.AnimStayBegin;
				cur_wall.AnimStayEnd=mobj.MScenery.AnimStayEnd;
				cur_wall.AnimWait=mobj.MScenery.AnimWait;
				cur_wall.PicMapHash=mobj.MScenery.PicMapHash;
				cur_wall.SpriteCut=mobj.MScenery.SpriteCut;

				WallsToSend.push_back(cur_wall);
			}
			break;
		case ITEM_TYPE_GENERIC:
		case ITEM_TYPE_GRID:
			{
				if(pid==SP_GRID_ENTIRE)
				{
					mapEntires.push_back(MapEntire(hx,hy,mobj.MScenery.ToDir,mobj.MScenery.ToEntire));
					continue;
				}

				if(type==ITEM_TYPE_GRID) SETFLAG(HexFlags[hy*maxhx+hx],FH_SCEN_GRID);
				if(!FLAG(proto_item->Flags,ITEM_NO_BLOCK)) SETFLAG(HexFlags[hy*maxhx+hx],FH_BLOCK);
				if(!FLAG(proto_item->Flags,ITEM_SHOOT_THRU))
				{
					SETFLAG(HexFlags[hy*maxhx+hx],FH_BLOCK);
					SETFLAG(HexFlags[hy*maxhx+hx],FH_NOTRAKE);
				}
				SETFLAG(HexFlags[hy*maxhx+hx],FH_SCEN);

				// To server
				if(pid==SP_MISC_SCRBLOCK)
				{
					// Block around
					for(int k=0;k<6;k++)
					{
						WORD hx_=hx,hy_=hy;
						MoveHexByDir(hx_,hy_,k,Header.MaxHexX,Header.MaxHexY);
						SETFLAG(HexFlags[hy_*maxhx+hx_],FH_BLOCK);
					}
				}
				else if(type==ITEM_TYPE_GRID)
				{
					mobj.AddRef();
					GridsVec.push_back(&mobj);
				}
				else // ITEM_TYPE_GENERIC
				{
					// Bind script
					const char* script=ItemMngr.GetProtoScript(pid);
					if(script)
					{
						char script_module[MAX_SCRIPT_NAME+1];
						char script_func[MAX_SCRIPT_NAME+1];
						if(Script::ReparseScriptName(script,script_module,script_func))
						{
							StringCopy(mobj.ScriptName,script_module);
							StringCopy(mobj.FuncName,script_func);
						}
					}

					mobj.RunTime.BindScriptId=0;
					if(mobj.ScriptName[0] && mobj.FuncName[0]) BindSceneryScript(&mobj);

					// Add to collection
					mobj.AddRef();
					SceneryVec.push_back(&mobj);
				}

				if(pid==SP_SCEN_TRIGGER)
				{
					if(mobj.RunTime.BindScriptId) SETFLAG(HexFlags[hy*maxhx+hx],FH_TRIGGER);
					continue;
				}

				// To client
				ScenToSend cur_scen;
				ZeroMemory(&cur_scen,sizeof(ScenToSend));

				// Flags
				if(type==ITEM_TYPE_GENERIC && mobj.MScenery.CanUse) SETFLAG(cur_scen.Flags,SCEN_CAN_USE);
				if(type==ITEM_TYPE_GENERIC && mobj.MScenery.CanTalk) SETFLAG(cur_scen.Flags,SCEN_CAN_TALK);
				if(type==ITEM_TYPE_GRID && proto_item->Grid.Type!=GRID_EXITGRID) SETFLAG(cur_scen.Flags,SCEN_CAN_USE);

				// Other
				cur_scen.ProtoId=mobj.ProtoId;
				cur_scen.MapX=mobj.MapX;
				cur_scen.MapY=mobj.MapY;
				cur_scen.Dir=mobj.Dir;
				cur_scen.OffsetX=mobj.MScenery.OffsetX;
				cur_scen.OffsetY=mobj.MScenery.OffsetY;
				cur_scen.LightColor=mobj.LightColor;
				cur_scen.LightDistance=mobj.LightDistance;
				cur_scen.LightFlags=mobj.LightDirOff|((mobj.LightDay&3)<<6);
				cur_scen.LightIntensity=mobj.LightIntensity;
				cur_scen.InfoOffset=mobj.MScenery.InfoOffset;
				cur_scen.AnimStayBegin=mobj.MScenery.AnimStayBegin;
				cur_scen.AnimStayEnd=mobj.MScenery.AnimStayEnd;
				cur_scen.AnimWait=mobj.MScenery.AnimWait;
				cur_scen.PicMapHash=mobj.MScenery.PicMapHash;
				cur_scen.SpriteCut=mobj.MScenery.SpriteCut;

				SceneriesToSend.push_back(cur_scen);
			}
			break;
		default:
			break;
		}
	}

	for(MapObjectPtrVecIt it=MObjects.begin(),end=MObjects.end();it!=end;++it) SAFEREL(*it);
	MObjects.clear();

	// Generate hashes
	HashTiles=maxhx*maxhy;
	Crypt.Crc32((BYTE*)Tiles,GetTilesSize(),HashTiles);
	HashWalls=maxhx*maxhy;
	if(WallsToSend.size()) Crypt.Crc32((BYTE*)&WallsToSend[0],WallsToSend.size()*sizeof(ScenToSend),HashWalls);
	HashScen=maxhx*maxhy;
	if(SceneriesToSend.size()) Crypt.Crc32((BYTE*)&SceneriesToSend[0],SceneriesToSend.size()*sizeof(ScenToSend),HashScen);

	MEMORY_PROCESS(MEMORY_PROTO_MAP,SceneriesToSend.capacity()*sizeof(ScenToSend));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,WallsToSend.capacity()*sizeof(ScenToSend));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,mapEntires.capacity()*sizeof(MapEntire));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,CrittersVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,ItemsVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,SceneryVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,GridsVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,Header.MaxHexX*Header.MaxHexY);
	MEMORY_PROCESS(MEMORY_PROTO_MAP,GetTilesSize());

	SaveCache(pmapFm);
#endif

#ifdef FONLINE_MAPPER
	for(int i=0,j=MObjects.size();i<j;i++)
	{
		MapObject* mobj=MObjects[i];
		mobj->RunTime.FromMap=this;
		if(mobj->MapObjType==MAP_OBJECT_ITEM || mobj->MapObjType==MAP_OBJECT_SCENERY)
		{
			if(mobj->MItem.PicMapHash && !mobj->RunTime.PicMapName[0]) StringCopy(mobj->RunTime.PicMapName,ResMngr.GetName(mobj->MItem.PicMapHash));
			if(mobj->MItem.PicInvHash && !mobj->RunTime.PicInvName[0]) StringCopy(mobj->RunTime.PicInvName,ResMngr.GetName(mobj->MItem.PicInvHash));
		}
	}
#endif
	return true;
}

#ifdef FONLINE_MAPPER
void ProtoMap::GenNew(FileManager* fm)
{
	Clear();
	Header.PlayersLimit=30;
	Header.Version=FO_MAP_VERSION_V5;
	Header.MaxHexX=MAXHEX_DEF;
	Header.MaxHexY=MAXHEX_DEF;
	pmapPid=0xFFFF;
	pmapFm=fm;
	pathType=PT_MAPS;
	static int number=0;
	Tiles=new DWORD[GetTilesSize()/sizeof(DWORD)];
	ZeroMemory(Tiles,GetTilesSize());

	// Morning	 5.00 -  9.59	 300 - 599
	// Day		10.00 - 18.59	 600 - 1139
	// Evening	19.00 - 22.59	1140 - 1379
	// Nigh		23.00 -  4.59	1380
	Header.DayTime[0]=300; Header.DayTime[1]=600;  Header.DayTime[2]=1140; Header.DayTime[3]=1380;
	Header.DayColor[0]=18; Header.DayColor[1]=128; Header.DayColor[2]=103; Header.DayColor[3]=51;
	Header.DayColor[4]=18; Header.DayColor[5]=128; Header.DayColor[6]=95;  Header.DayColor[7]=40;
	Header.DayColor[8]=53; Header.DayColor[9]=128; Header.DayColor[10]=86; Header.DayColor[11]=29;

	isInit=true;
}

bool ProtoMap::Save(const char* f_name, int path_type, bool text, bool pack)
{
	if(f_name && *f_name) pmapName=f_name;
	if(path_type>=0) pathType=path_type;

	string fname=pmapName;
	Header.Version=FO_MAP_VERSION_V9;
	pmapFm->ClearOutBuf();

	if(text)
	{
		SaveTextFormat(pmapFm);
		fname+=".txt";
	}
	else
	{
		Header.HeaderSize=sizeof(Header);
		Header.Packed=pack;
		Header.UnpackedDataLen=GetTilesSize()+sizeof(DWORD)+GetObjectsSize();

		DWORD data_len=Header.UnpackedDataLen;
		BYTE* data=new BYTE[data_len];
		memcpy(data,Tiles,GetTilesSize());
		*(DWORD*)(data+GetTilesSize())=MObjects.size();
		for(size_t i=0,j=MObjects.size();i<j;i++) memcpy(data+GetTilesSize()+sizeof(DWORD)+i*MAP_OBJECT_SIZE,MObjects[i],MAP_OBJECT_SIZE);
		if(pack)
		{
			BYTE* data_=Crypt.Compress(data,data_len);
			delete[] data;
			if(!data_)
			{
				WriteLog(__FUNCTION__" - Unable to pack data.\n");
				return false;
			}
			data=data_;
		}

		pmapFm->SetData(&Header,sizeof(Header));
		pmapFm->SetData(data,data_len);
		fname+=".map";
	}

	if(!pmapFm->SaveOutBufToFile(fname.c_str(),pathType))
	{
		WriteLog(__FUNCTION__" - Unable write file.\n");
		pmapFm->ClearOutBuf();
		return false;
	}

	pmapFm->ClearOutBuf();
	return true;
}

bool ProtoMap::IsMapFile(const char* fname)
{
	char* ext=(char*)FileManager::GetExtension(fname);
	if(!ext) return false;
	ext--;

	if(!_stricmp(ext,MAP_PROTO_EXT))
	{
		// Check binary format
		FileManager file;
		if(!file.LoadFile(fname,PT_ROOT)) return false;
		DWORD version=file.GetLEDWord();
		return version==FO_MAP_VERSION_V4 || version==FO_MAP_VERSION_V5 ||
			version==FO_MAP_VERSION_V6 || version==FO_MAP_VERSION_V7 ||
			version==FO_MAP_VERSION_V8 || version==FO_MAP_VERSION_V9;
	}
	else if(!_stricmp(ext,".txt"))
	{
		// Check text format
		IniParser txt;
		if(!txt.LoadFile(fname,PT_MAPS)) return false;
		return txt.IsApp(APP_HEADER) && txt.IsApp(APP_TILES) && txt.IsApp(APP_OBJECTS);
	}
	return false;
}
#endif // FONLINE_MAPPER

#ifdef FONLINE_SERVER
DWORD ProtoMap::CountEntire(DWORD num)
{
	if(num==-1) return mapEntires.size();

	DWORD result=0;
	for(int i=0,j=mapEntires.size();i<j;i++)
	{
		if(mapEntires[i].Number==num) result++;
	}
	return result;
}

ProtoMap::MapEntire* ProtoMap::GetEntire(DWORD num, DWORD skip)
{
	for(int i=0,j=mapEntires.size();i<j;i++)
	{
		if(num==-1 || mapEntires[i].Number==num)
		{
			if(!skip) return &mapEntires[i];
			else skip--;
		}
	}

	return NULL;
}

ProtoMap::MapEntire* ProtoMap::GetEntireRandom(DWORD num)
{
	vector<MapEntire*> entires;
	for(int i=0,j=mapEntires.size();i<j;i++)
	{
		if(num==-1 || mapEntires[i].Number==num) entires.push_back(&mapEntires[i]);
	}

	if(entires.empty()) return NULL;
	return entires[Random(0,entires.size()-1)];
}

ProtoMap::MapEntire* ProtoMap::GetEntireNear(DWORD num, WORD hx, WORD hy)
{
	MapEntire* near_entire=NULL;
	int last_dist;
	for(int i=0,j=mapEntires.size();i<j;i++)
	{
		MapEntire& entire=mapEntires[i];
		if(num==-1 || entire.Number==num)
		{
			int dist=DistGame(hx,hy,entire.HexX,entire.HexY);
			if(!near_entire || dist<last_dist)
			{
				near_entire=&entire;
				last_dist=dist;
			}
		}
	}
	return near_entire;
}

ProtoMap::MapEntire* ProtoMap::GetEntireNear(DWORD num, DWORD num_ext, WORD hx, WORD hy)
{
	MapEntire* near_entire=NULL;
	int last_dist;
	for(int i=0,j=mapEntires.size();i<j;i++)
	{
		MapEntire& entire=mapEntires[i];
		if(num==-1 || num_ext==-1 || entire.Number==num || entire.Number==num_ext)
		{
			int dist=DistGame(hx,hy,entire.HexX,entire.HexY);
			if(!near_entire || dist<last_dist)
			{
				near_entire=&entire;
				last_dist=dist;
			}
		}
	}
	return near_entire;
}

void ProtoMap::GetEntires(DWORD num, EntiresVec& entires)
{
	for(int i=0,j=mapEntires.size();i<j;i++)
	{
		MapEntire& entire=mapEntires[i];
		if(num==-1 || entire.Number==num) entires.push_back(entire);
	}
}

MapObject* ProtoMap::GetMapScenery(WORD hx, WORD hy, WORD pid)
{
	for(MapObjectPtrVecIt it=SceneryVec.begin(),end=SceneryVec.end();it!=end;++it)
	{
		MapObject* mobj=*it;
		if((!pid || mobj->ProtoId==pid) && mobj->MapX==hx && mobj->MapY==hy) return mobj;
	}
	return NULL;
}

void ProtoMap::GetMapSceneriesHex(WORD hx, WORD hy, MapObjectPtrVec& mobjs)
{
	for(MapObjectPtrVecIt it=SceneryVec.begin(),end=SceneryVec.end();it!=end;++it)
	{
		MapObject* mobj=*it;
		if(mobj->MapX==hx && mobj->MapY==hy) mobjs.push_back(mobj);
	}
}

void ProtoMap::GetMapSceneriesHexEx(WORD hx, WORD hy, DWORD radius, WORD pid, MapObjectPtrVec& mobjs)
{
	for(MapObjectPtrVecIt it=SceneryVec.begin(),end=SceneryVec.end();it!=end;++it)
	{
		MapObject* mobj=*it;
		if((!pid || mobj->ProtoId==pid) && DistGame(mobj->MapX,mobj->MapY,hx,hy)<=radius) mobjs.push_back(mobj);
	}
}

void ProtoMap::GetMapSceneriesByPid(WORD pid, MapObjectPtrVec& mobjs)
{
	for(MapObjectPtrVecIt it=SceneryVec.begin(),end=SceneryVec.end();it!=end;++it)
	{
		MapObject* mobj=*it;
		if(!pid || mobj->ProtoId==pid) mobjs.push_back(mobj);
	}
}

MapObject* ProtoMap::GetMapGrid(WORD hx, WORD hy)
{
	for(MapObjectPtrVecIt it=GridsVec.begin(),end=GridsVec.end();it!=end;++it)
	{
		MapObject* mobj=*it;
		if(mobj->MapX==hx && mobj->MapY==hy) return mobj;
	}
	return NULL;
}

#endif // FONLINE_SERVER