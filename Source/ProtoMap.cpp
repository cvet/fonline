#include "StdAfx.h"
#include "ProtoMap.h"
#include "ItemManager.h"
#include "Crypt.h"
#include "ConstantsManager.h"
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

#define FO_MAP_VERSION_TEXT1    (1)
#define FO_MAP_VERSION_TEXT2    (2)
#define FO_MAP_VERSION_TEXT3    (3)
#define FO_MAP_VERSION_TEXT4    (4)

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
	uchar MapObjType;
	bool InContainer;
	ushort ProtoId;

	ushort MapX;
	ushort MapY;

	uchar FrameNum;
	uchar Dir;

	uchar Reserved0;
	uchar Reserved1;

	uchar ItemSlot;
	uchar Reserved2;
	ushort Reserved3;
	uint Reserved4;

	short OffsetX;
	short OffsetY;

	char ScriptName[25+1];
	char FuncName[25+1];

	uint LightRGB;
	uchar Reserved5;
	uchar LightDay;
	uchar LightDirOff;
	uchar LightDistance;
	int LightIntensity;

	int BindScriptId; // For script bind in runtime

	union
	{
		// Critter
		struct
		{
			uint DialogId;
			uint AiPack;
			ushort Reserved0;
			ushort Reserved1;
			uchar Cond;
			uchar CondExt;
			uchar Reserved2; // Level
			uchar Reserved3;
			ushort BagId;
			ushort TeamId;
			int NpcRole;
			short RespawnTime;
			ushort Reserved11;
		} CRITTER;

		// Item
		struct
		{
			uint Val1;
			uchar BrokenFlags;
			uchar BrokenCount;
			ushort Deterioration;
		} ARMOR;

		struct
		{
			uint DoorId;
			ushort Condition;
			ushort Complexity;
			uchar IsOpen;
			uint Reserved;
		} CONTAINER;

		struct
		{
			uint Count;
		} DRUG;

		struct
		{
			uint Reserved;
			uchar BrokenFlags;
			uchar BrokenCount;
			ushort Deterioration;
			ushort AmmoPid;
			uint AmmoCount;
		} WEAPON;

		struct
		{
			uint Count;
		} AMMO;

		struct
		{
			uint Count;
		} MISC;

		struct
		{
			uint Reserved[4];
			int Val1;
			int Val2;
			int Val3;
			int Val4;
			int Val5;
			int Val0;
		} ITEM_VAL;

		struct
		{
			uint DoorId;
		} KEY;

		struct
		{
			uint DoorId;
			ushort Condition;
			ushort Complexity;
			uchar IsOpen;
		} DOOR;

		// Scenery
		struct
		{
			uint ElevatorType;
			uint ToMapPid;
			ushort ToMapX;
			ushort ToMapY;
			uchar ToDir;
			uchar ToEntire;
		} GRID;

		struct
		{
			uchar CanUse;
			uchar TriggerNum;
			ushort Reserved0;
			uint Reserved1[3]; // Trash
			ushort Reserved2;
			uchar Reserved3;
			uchar ParamsCount;
			int Param[5];
		} GENERIC;

		struct
		{
			uint Buffer[10];
		} ALIGN;
	};
};

struct HeaderV9
{
	uint Version;
	bool Packed;
	bool NoLogOut;
	ushort HeaderSize;
	int PlayersLimit;
	uint UnpackedDataLen;
	ushort MaxHexX;
	ushort MaxHexY;
	int Time;
	int CenterX;
	int CenterY;
	char ScriptModule[64+1];
	char ScriptFunc[64+1];
	int DayTime[4];
	uchar DayColor[12];
};

class MapObjectV9
{
public:
	uchar MapObjType;
	ushort ProtoId;
	ushort MapX;
	ushort MapY;
	short Dir;
	uint LightColor;
	uchar LightDay;
	uchar LightDirOff;
	uchar LightDistance;
	char LightIntensity;
	char ScriptName[25+1];
	char FuncName[25+1];
	uint Reserved[7];
	int UserData[10];

	union
	{
		struct
		{
			uchar Cond;
			uchar CondExt;
			short ParamIndex[15];
			int ParamValue[15];
		} MCritter;

		struct
		{
			short OffsetX;
			short OffsetY;
			uchar AnimStayBegin;
			uchar AnimStayEnd;
			ushort AnimWait;
			ushort PicMapDeprecated;
			ushort PicInvDeprecated;
			uchar InfoOffset;
			uchar Reserved[3];
			uint PicMapHash;
			uint PicInvHash;
			uint Count;
			uchar BrokenFlags;
			uchar BrokenCount;
			ushort Deterioration;
			bool InContainer;
			uchar ItemSlot;
			ushort AmmoPid;
			uint AmmoCount;
			uint LockerDoorId;
			ushort LockerCondition;
			ushort LockerComplexity;
			short TrapValue;
			int Val[10];
		} MItem;

		struct
		{
			short OffsetX;
			short OffsetY;
			uchar AnimStayBegin;
			uchar AnimStayEnd;
			ushort AnimWait;
			ushort PicMapDeprecated;
			ushort PicInvDeprecated;
			uchar InfoOffset;
			uchar Reserved[3];
			uint PicMapHash;
			uint PicInvHash;
			bool CanUse;
			bool CanTalk;
			uint TriggerNum;
			uchar ParamsCount;
			int Param[5];
			ushort ToMapPid;
			uint ToEntire;
			ushort ToMapX;
			ushort ToMapY;
			uchar ToDir;
			uchar SpriteCut;
		} MScenery;

		struct
		{
			uint Buffer[25];
		} MAlign;
	};
};

/************************************************************************/
/* ProtoMap                                                             */
/************************************************************************/

bool ProtoMap::Init(ushort pid, const char* name, int path_type)
{
	Clear();
	if(!name || !name[0]) return false;

	pmapPid=pid;
	pmapFm=new FileManager();
	pathType=path_type;
	pmapName=name;
	LastObjectUID=0;

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
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)SceneriesToSend.capacity()*sizeof(SceneryCl));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)WallsToSend.capacity()*sizeof(SceneryCl));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)mapEntires.capacity()*sizeof(MapEntire));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)CrittersVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)ItemsVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)SceneryVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)GridsVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)Header.MaxHexX*Header.MaxHexY);
	MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)Tiles.capacity()*sizeof(MapEntire));

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
	Tiles.clear();
#ifdef FONLINE_MAPPER
	TilesField.clear();
	RoofsField.clear();
#endif
	ZeroMemory(&Header,sizeof(Header));
	pmapName="";
	pathType=0;
	isInit=false;
}

bool ProtoMap::ReadHeader(int version)
{
	HeaderV9 header9;
	ZeroMemory(&header9,sizeof(header9));
	header9.DayTime[0]=300; header9.DayTime[1]=600;  header9.DayTime[2]=1140; header9.DayTime[3]=1380;
	header9.DayColor[0]=18; header9.DayColor[1]=128; header9.DayColor[2]=103; header9.DayColor[3]=51;
	header9.DayColor[4]=18; header9.DayColor[5]=128; header9.DayColor[6]=95;  header9.DayColor[7]=40;
	header9.DayColor[8]=53; header9.DayColor[9]=128; header9.DayColor[10]=86; header9.DayColor[11]=29;

	if(version>=9)
	{
		pmapFm->SetCurPos(0);
		if(!pmapFm->CopyMem(&header9,8)) return false;
		pmapFm->SetCurPos(0);
		if(!pmapFm->CopyMem(&header9,header9.HeaderSize)) return false;
	}
	else if(version>=7)
	{
		pmapFm->SetCurPos(0);
		if(!pmapFm->CopyMem(&header9,8)) return false;
		pmapFm->SetCurPos(0);
		if(!pmapFm->CopyMem(&header9,header9.HeaderSize)) return false;
	}
	else // Version < 7
	{
		pmapFm->SetCurPos(0);
		if(!pmapFm->CopyMem(&header9,96)) return false;
	}

	ZeroMemory(&Header,sizeof(Header));
	Header.Version=header9.Version;
	Header.NoLogOut=false;
	Header.Time=-1;
	Header.MaxHexX=header9.MaxHexX;
	Header.MaxHexY=header9.MaxHexY;
	Header.WorkHexX=header9.CenterX;
	Header.WorkHexY=header9.CenterY;
	memcpy(Header.DayTime,header9.DayTime,sizeof(Header.DayTime));
	memcpy(Header.DayColor,header9.DayColor,sizeof(Header.DayColor));

	Header.HeaderSize=header9.HeaderSize;
	Header.Packed=header9.Packed;
	Header.UnpackedDataLen=header9.UnpackedDataLen;
	return true;
}

bool ProtoMap::ReadTiles(int version)
{
	uint* tiles=new(nothrow) uint[((Header.MaxHexX/2)*(Header.MaxHexY/2)*sizeof(uint)*2)/sizeof(uint)];
	if(!tiles) return false;
	pmapFm->SetCurPos(Header.Packed?0:Header.HeaderSize);

	if(version<8)
	{
		// Convert lst offsets to name hashes
		ZeroMemory(tiles,(Header.MaxHexX/2)*(Header.MaxHexY/2)*sizeof(uint)*2);
		uint size=(Header.MaxHexX/2)*(Header.MaxHexY/2)*sizeof(uint);
		uint* ptr=new(nothrow) uint[size/sizeof(uint)];
		if(!pmapFm->CopyMem(ptr,size)) return false;
		for(int x=0;x<Header.MaxHexX/2;x++)
		{
			for(int y=0;y<Header.MaxHexY/2;y++)
			{
				ushort tile=(ptr[y*(Header.MaxHexX/2)+x]>>16);
				ushort roof=(ptr[y*(Header.MaxHexX/2)+x]&0xFFFF);
				if(tile>1 && (tile=Deprecated_GetPicHash(-2,0,tile))) Tiles.push_back(Tile(tile,x*2,y*2,0,0,0,false));
				if(roof>1 && (roof=Deprecated_GetPicHash(-2,0,roof))) Tiles.push_back(Tile(tile,x*2,y*2,0,0,0,true));
			}
		}
		delete[] ptr;
		delete[] tiles;
		return true;
	}

	// Version 8, 9
	if(!pmapFm->CopyMem(tiles,(Header.MaxHexX/2)*(Header.MaxHexY/2)*sizeof(uint)*2)) return false;

	for(int tx=0;tx<Header.MaxHexX/2;tx++)
	{
		for(int ty=0;ty<Header.MaxHexY/2;ty++)
		{
			uint tile=tiles[ty*(Header.MaxHexX/2)*2+tx*2];
			uint roof=tiles[ty*(Header.MaxHexX/2)*2+tx*2+1];
			if(tile) Tiles.push_back(Tile(tile,tx*2,ty*2,0,0,0,false));
			if(roof) Tiles.push_back(Tile(roof,tx*2,ty*2,0,0,0,true));
		}
	}
	delete[] tiles;
	return true;
}

bool ProtoMap::ReadObjects(int version)
{
	if(version<6)
	{
		pmapFm->SetCurPos((Header.Packed?0:Header.HeaderSize)+(Header.MaxHexX/2)*(Header.MaxHexY/2)*sizeof(uint));
		uint count=pmapFm->GetLEUInt();
		if(!count) return true;

		vector<MapObjectV5> objects_v5;
		objects_v5.resize(count);
		if(!pmapFm->CopyMem(&objects_v5[0],count*sizeof(MapObjectV5))) return false;

		for(uint k=0;k<count;k++)
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
				Deprecated_CondExtToAnim2(obj_v5.CRITTER.Cond,obj_v5.CRITTER.CondExt,obj_v6.MCritter.Anim2,obj_v6.MCritter.Anim2);
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
				obj_v6.ContainerUID=obj_v5.InContainer;
				if(obj_v6.ContainerUID) obj_v6.MItem.ItemSlot=obj_v5.ItemSlot;
				for(int i=0;i<10;i++) obj_v6.MItem.Val[i]=0;
				obj_v6.MItem.Val[0]=obj_v5.ITEM_VAL.Val0;
				obj_v6.MItem.Val[1]=obj_v5.ITEM_VAL.Val1;
				obj_v6.MItem.Val[2]=obj_v5.ITEM_VAL.Val2;
				obj_v6.MItem.Val[3]=obj_v5.ITEM_VAL.Val3;
				obj_v6.MItem.Val[4]=obj_v5.ITEM_VAL.Val4;
				obj_v6.MItem.Val[5]=obj_v5.ITEM_VAL.Val5;

				if(proto->Type==ITEM_TYPE_AMMO) obj_v6.MItem.Count=obj_v5.AMMO.Count;
				else if(proto->Type==ITEM_TYPE_MISC) obj_v6.MItem.Count=obj_v5.MISC.Count;
				else if(proto->Type==ITEM_TYPE_DRUG) obj_v6.MItem.Count=obj_v5.DRUG.Count;
				else if(proto->Type==ITEM_TYPE_ARMOR)
				{
					obj_v6.MItem.BrokenFlags=obj_v5.ARMOR.BrokenFlags;
					obj_v6.MItem.BrokenCount=obj_v5.ARMOR.BrokenCount;
					obj_v6.MItem.Deterioration=obj_v5.ARMOR.Deterioration;
				}
				else if(proto->Type==ITEM_TYPE_WEAPON)
				{
					obj_v6.MItem.BrokenFlags=obj_v5.WEAPON.BrokenFlags;
					obj_v6.MItem.BrokenCount=obj_v5.WEAPON.BrokenCount;
					obj_v6.MItem.Deterioration=obj_v5.WEAPON.Deterioration;
					obj_v6.MItem.AmmoPid=obj_v5.WEAPON.AmmoPid;
					obj_v6.MItem.AmmoCount=obj_v5.WEAPON.AmmoCount;
				}
				else if(proto->Type==ITEM_TYPE_KEY)
				{
					obj_v6.MItem.LockerDoorId=obj_v5.KEY.DoorId;
				}
				else if(proto->Type==ITEM_TYPE_CONTAINER || proto->Type==ITEM_TYPE_DOOR)
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

				if(proto->Type==ITEM_TYPE_GRID)
				{
					obj_v6.MScenery.ToEntire=obj_v5.GRID.ToEntire;
					obj_v6.MScenery.ToMapPid=obj_v5.GRID.ToMapPid;
					obj_v6.MScenery.ToDir=obj_v5.GRID.ToDir;
				}
				else if(proto->Type==ITEM_TYPE_GENERIC)
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
	else // Version 6, 7, 8, 9
	{
		pmapFm->SetCurPos((Header.Packed?0:Header.HeaderSize)+
			((Header.MaxHexX/2)*(Header.MaxHexY/2)*sizeof(uint)*(version<8?1:2)));
		uint count=pmapFm->GetLEUInt();
		if(!count) return true;

		for(uint i=0;i<count;i++)
		{
			MapObjectV9 mobj_v9;
			if(!pmapFm->CopyMem(&mobj_v9,240)) return false;

			MapObject* mobj=new MapObject();
			mobj->MapObjType=mobj_v9.MapObjType;
			mobj->ProtoId=mobj_v9.ProtoId;
			mobj->MapX=mobj_v9.MapX;
			mobj->MapY=mobj_v9.MapY;
			mobj->Dir=mobj_v9.Dir;
			mobj->LightColor=mobj_v9.LightColor;
			mobj->LightDay=mobj_v9.LightDay;
			mobj->LightDirOff=mobj_v9.LightDirOff;
			mobj->LightDistance=mobj_v9.LightDistance;
			mobj->LightIntensity=mobj_v9.LightIntensity;
			StringCopy(mobj->ScriptName,mobj_v9.ScriptName);
			StringCopy(mobj->FuncName,mobj_v9.FuncName);
			for(int i=0;i<10;i++) mobj->UserData[i]=mobj_v9.UserData[i];
			mobj->MapObjType=mobj_v9.MapObjType;

			if(mobj->MapObjType==MAP_OBJECT_CRITTER)
			{
				mobj->MCritter.Cond=mobj_v9.MCritter.Cond;
				Deprecated_CondExtToAnim2(mobj_v9.MCritter.Cond,mobj_v9.MCritter.CondExt,mobj->MCritter.Anim2,mobj->MCritter.Anim2);
				for(int i=0;i<15;i++) mobj->MCritter.ParamIndex[i]=mobj_v9.MCritter.ParamIndex[i];
				for(int i=0;i<15;i++) mobj->MCritter.ParamValue[i]=mobj_v9.MCritter.ParamValue[i];
			}
			else if(mobj->MapObjType==MAP_OBJECT_ITEM)
			{
				mobj->MItem.OffsetX=mobj_v9.MItem.OffsetX;
				mobj->MItem.OffsetY=mobj_v9.MItem.OffsetY;
				mobj->MItem.AnimStayBegin=mobj_v9.MItem.AnimStayBegin;
				mobj->MItem.AnimStayEnd=mobj_v9.MItem.AnimStayEnd;
				mobj->MItem.AnimWait=mobj_v9.MItem.AnimWait;
				mobj->MItem.InfoOffset=mobj_v9.MItem.InfoOffset;
				mobj->MItem.PicMapHash=mobj_v9.MItem.PicMapHash;
				mobj->MItem.PicInvHash=mobj_v9.MItem.PicInvHash;
				mobj->MItem.Count=mobj_v9.MItem.Count;
				mobj->MItem.BrokenFlags=mobj_v9.MItem.BrokenFlags;
				mobj->MItem.BrokenCount=mobj_v9.MItem.BrokenCount;
				mobj->MItem.Deterioration=mobj_v9.MItem.Deterioration;
				mobj->ContainerUID=mobj_v9.MItem.InContainer;
				mobj->MItem.ItemSlot=mobj_v9.MItem.ItemSlot;
				mobj->MItem.AmmoPid=mobj_v9.MItem.AmmoPid;
				mobj->MItem.AmmoCount=mobj_v9.MItem.AmmoCount;
				mobj->MItem.LockerDoorId=mobj_v9.MItem.LockerDoorId;
				mobj->MItem.LockerCondition=mobj_v9.MItem.LockerCondition;
				mobj->MItem.LockerComplexity=mobj_v9.MItem.LockerComplexity;
				mobj->MItem.TrapValue=mobj_v9.MItem.TrapValue;
				for(int i=0;i<10;i++) mobj->MItem.Val[i]=mobj_v9.MItem.Val[i];
			}
			else if(mobj->MapObjType==MAP_OBJECT_SCENERY)
			{
				mobj->MScenery.OffsetX=mobj_v9.MScenery.OffsetX;
				mobj->MScenery.OffsetY=mobj_v9.MScenery.OffsetY;
				mobj->MScenery.AnimStayBegin=mobj_v9.MScenery.AnimStayBegin;
				mobj->MScenery.AnimStayEnd=mobj_v9.MScenery.AnimStayEnd;
				mobj->MScenery.AnimWait=mobj_v9.MScenery.AnimWait;
				mobj->MScenery.InfoOffset=mobj_v9.MScenery.InfoOffset;
				mobj->MScenery.PicMapHash=mobj_v9.MScenery.PicMapHash;
				mobj->MScenery.PicInvHash=mobj_v9.MScenery.PicInvHash;
				mobj->MScenery.CanUse=mobj_v9.MScenery.CanUse;
				mobj->MScenery.CanTalk=mobj_v9.MScenery.CanTalk;
				mobj->MScenery.TriggerNum=mobj_v9.MScenery.TriggerNum;
				mobj->MScenery.ParamsCount=mobj_v9.MScenery.ParamsCount;
				for(int i=0;i<5;i++) mobj->MScenery.Param[i]=mobj_v9.MScenery.Param[i];
				mobj->MScenery.ToMapPid=mobj_v9.MScenery.ToMapPid;
				mobj->MScenery.ToEntire=mobj_v9.MScenery.ToEntire;
				mobj->MScenery.ToDir=mobj_v9.MScenery.ToDir;
				mobj->MScenery.SpriteCut=mobj_v9.MScenery.SpriteCut;
			}

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
				if(field=="Version")
				{
					Header.Version=ivalue;
					uint old_version=(ivalue<<20);
					if(old_version==FO_MAP_VERSION_V6 || old_version==FO_MAP_VERSION_V7 ||
						old_version==FO_MAP_VERSION_V8 || old_version==FO_MAP_VERSION_V9)
					{
						Header.Version=FO_MAP_VERSION_TEXT1;
						Header.DayTime[0]=300; Header.DayTime[1]=600;  Header.DayTime[2]=1140; Header.DayTime[3]=1380;
						Header.DayColor[0]=18; Header.DayColor[1]=128; Header.DayColor[2]=103; Header.DayColor[3]=51;
						Header.DayColor[4]=18; Header.DayColor[5]=128; Header.DayColor[6]=95;  Header.DayColor[7]=40;
						Header.DayColor[8]=53; Header.DayColor[9]=128; Header.DayColor[10]=86; Header.DayColor[11]=29;
					}
				}
				if(field=="MaxHexX") Header.MaxHexX=ivalue;
				if(field=="MaxHexY") Header.MaxHexY=ivalue;
				if(field=="WorkHexX" || field=="CenterX") Header.WorkHexX=ivalue;
				if(field=="WorkHexY" || field=="CenterY") Header.WorkHexY=ivalue;
				if(field=="Time") Header.Time=ivalue;
				if(field=="NoLogOut") Header.NoLogOut=(ivalue!=0);
				if(field=="ScriptModule" && value!="-") StringCopy(Header.ScriptModule,value.c_str());
				if(field=="ScriptFunc" && value!="-") StringCopy(Header.ScriptFunc,value.c_str());
				if(field=="DayTime")
				{
					Header.DayTime[0]=ivalue;
					istr >> Header.DayTime[1];
					istr >> Header.DayTime[2];
					istr >> Header.DayTime[3];
				}
				if(field=="DayColor0")
				{
					Header.DayColor[0]=ivalue;
					istr >> ivalue; Header.DayColor[4]=ivalue;
					istr >> ivalue; Header.DayColor[8]=ivalue;
				}
				if(field=="DayColor1")
				{
					Header.DayColor[1]=ivalue;
					istr >> ivalue; Header.DayColor[5]=ivalue;
					istr >> ivalue; Header.DayColor[9]=ivalue;
				}
				if(field=="DayColor2")
				{
					Header.DayColor[2]=ivalue;
					istr >> ivalue; Header.DayColor[6]=ivalue;
					istr >> ivalue; Header.DayColor[10]=ivalue;
				}
				if(field=="DayColor3")
				{
					Header.DayColor[3]=ivalue;
					istr >> ivalue; Header.DayColor[7]=ivalue;
					istr >> ivalue; Header.DayColor[11]=ivalue;
				}
			}
		}
		delete[] header_str;
	}
	if((Header.Version!=FO_MAP_VERSION_TEXT1 && Header.Version!=FO_MAP_VERSION_TEXT2 &&
		Header.Version!=FO_MAP_VERSION_TEXT3 && Header.Version!=FO_MAP_VERSION_TEXT4) ||
		Header.MaxHexX<1 || Header.MaxHexY<1) return false;

	// Tiles
	char* tiles_str=map_ini.GetApp(APP_TILES);
	if(tiles_str)
	{
		istrstream istr(tiles_str);
		string type;
		if(Header.Version==FO_MAP_VERSION_TEXT1)
		{
			string name;

			// Deprecated
			while(!istr.eof() && !istr.fail())
			{
				int hx,hy;
				istr >> type >> hx >> hy >> name;
				if(!istr.fail() && hx>=0 && hx<Header.MaxHexX && hy>=0 && hy<Header.MaxHexY)
				{
					hx*=2;
					hy*=2;
					if(type=="tile") Tiles.push_back(Tile(Str::GetHash(name.c_str()),hx,hy,0,0,0,false));
					else if(type=="roof") Tiles.push_back(Tile(Str::GetHash(name.c_str()),hx,hy,0,0,0,true));
					//else if(type=="terr" || type=="terrain") Tiles.push_back(Tile(Str::GetHash(name.c_str()),hx,hy,0,0,0,));
					else if(type=="0") Tiles.push_back(Tile((uint)_atoi64(name.c_str()),hx,hy,0,0,0,false));
					else if(type=="1") Tiles.push_back(Tile((uint)_atoi64(name.c_str()),hx,hy,0,0,0,true));
				}
			}
		}
		else
		{
			char name[MAX_FOTEXT];
			int hx,hy;
			int ox,oy,layer;
			bool is_roof;
			size_t len;
			bool has_offs;
			bool has_layer;
			while(!istr.eof() && !istr.fail())
			{
				istr >> type >> hx >> hy;
				if(!istr.fail() && hx>=0 && hx<Header.MaxHexX && hy>=0 && hy<Header.MaxHexY)
				{
					if(!type.compare(0,4,"tile")) is_roof=false;
					else if(!type.compare(0,4,"roof")) is_roof=true;
					else continue;

					len=type.length();
					has_offs=false;
					has_layer=false;
					if(len>5)
					{
						while(--len>=5)
						{
							switch(type[len])
							{
							case 'o': has_offs=true; break;
							case 'l': has_layer=true; break;
							default: break;
							}
						}
					}

					if(has_offs) istr >> ox >> oy;
					else ox=oy=0;
					if(has_layer) istr >> layer;
					else layer=0;

					istr.getline(name,MAX_FOTEXT);
					Str::EraseFrontBackSpecificChars(name);

					Tiles.push_back(Tile(Str::GetHash(name),hx,hy,ox,oy,layer,is_roof));
				}
			}
		}
		delete[] tiles_str;
	}

	// Objects
	char* objects_str=map_ini.GetApp(APP_OBJECTS);
	if(objects_str)
	{
		istrstream istr(objects_str);
		string field;
		char svalue[MAX_FOTEXT];
		int ivalue;
		while(!istr.eof() && !istr.fail())
		{
			istr >> field;
			istr.getline(svalue,MAX_FOTEXT);

			if(!istr.fail())
			{
				ivalue=(int)_atoi64(svalue);

				if(field=="MapObjType")
				{
					MapObject* mobj=new MapObject();

					mobj->MapObjType=ivalue;
					if(ivalue==MAP_OBJECT_CRITTER)
					{
						mobj->MCritter.Cond=COND_LIFE;
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
					else if(field=="UID") mobj.UID=ivalue;
					else if(field=="ContainerUID") mobj.ContainerUID=ivalue;
					else if(field=="ParentUID") mobj.ParentUID=ivalue;
					else if(field=="ParentChildIndex") mobj.ParentChildIndex=ivalue;
					else if(field=="LightColor") mobj.LightColor=ivalue;
					else if(field=="LightDay") mobj.LightDay=ivalue;
					else if(field=="LightDirOff") mobj.LightDirOff=ivalue;
					else if(field=="LightDistance") mobj.LightDistance=ivalue;
					else if(field=="LightIntensity") mobj.LightIntensity=ivalue;
					else if(field=="ScriptName") StringCopy(mobj.ScriptName,Str::EraseFrontBackSpecificChars(svalue));
					else if(field=="FuncName") StringCopy(mobj.FuncName,Str::EraseFrontBackSpecificChars(svalue));
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
						else if(field=="Critter_Anim1") mobj.MCritter.Anim1=ivalue;
						else if(field=="Critter_Anim2") mobj.MCritter.Anim2=ivalue;
						else if(field=="Critter_ParamIndex0") mobj.MCritter.ParamIndex[0]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex1") mobj.MCritter.ParamIndex[1]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex2") mobj.MCritter.ParamIndex[2]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex3") mobj.MCritter.ParamIndex[3]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex4") mobj.MCritter.ParamIndex[4]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex5") mobj.MCritter.ParamIndex[5]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex6") mobj.MCritter.ParamIndex[6]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex7") mobj.MCritter.ParamIndex[7]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex8") mobj.MCritter.ParamIndex[8]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex9") mobj.MCritter.ParamIndex[9]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex10") mobj.MCritter.ParamIndex[10]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex11") mobj.MCritter.ParamIndex[11]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex12") mobj.MCritter.ParamIndex[12]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex13") mobj.MCritter.ParamIndex[13]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
						else if(field=="Critter_ParamIndex14") mobj.MCritter.ParamIndex[14]=ConstantsManager::GetParamId(Str::EraseFrontBackSpecificChars(svalue));
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
						// Deprecated
						else if(field=="Critter_CondExt") Deprecated_CondExtToAnim2(mobj.MCritter.Cond,ivalue,mobj.MCritter.Anim2,mobj.MCritter.Anim2);
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
							StringCopy(mobj.RunTime.PicMapName,Str::EraseFrontBackSpecificChars(svalue));
#endif
							mobj.MItem.PicMapHash=Str::GetHash(Str::EraseFrontBackSpecificChars(svalue));
						}
						else if(field=="PicInvName")
						{
#ifdef FONLINE_MAPPER
							StringCopy(mobj.RunTime.PicInvName,Str::EraseFrontBackSpecificChars(svalue));
#endif
							mobj.MItem.PicInvHash=Str::GetHash(Str::EraseFrontBackSpecificChars(svalue));
						}
						else if(field=="InfoOffset") mobj.MItem.InfoOffset=ivalue;
						// Item
						else if(mobj.MapObjType==MAP_OBJECT_ITEM)
						{
							if(field=="Item_InfoOffset") mobj.MItem.InfoOffset=ivalue;
							else if(field=="Item_Count") mobj.MItem.Count=ivalue;
							else if(field=="Item_BrokenFlags") mobj.MItem.BrokenFlags=ivalue;
							else if(field=="Item_BrokenCount") mobj.MItem.BrokenCount=ivalue;
							else if(field=="Item_Deterioration") mobj.MItem.Deterioration=ivalue;
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
							// Deprecated
							else if(field=="Item_DeteorationFlags") mobj.MItem.BrokenFlags=ivalue;
							else if(field=="Item_DeteorationCount") mobj.MItem.BrokenCount=ivalue;
							else if(field=="Item_DeteorationValue") mobj.MItem.Deterioration=ivalue;
							else if(field=="Item_InContainer") mobj.ContainerUID=ivalue;
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
	fm->SetStr("%-20s %d\n","Version",Header.Version);
	fm->SetStr("%-20s %d\n","MaxHexX",Header.MaxHexX);
	fm->SetStr("%-20s %d\n","MaxHexY",Header.MaxHexY);
	fm->SetStr("%-20s %d\n","WorkHexX",Header.WorkHexX);
	fm->SetStr("%-20s %d\n","WorkHexY",Header.WorkHexY);
	fm->SetStr("%-20s %s\n","ScriptModule",Header.ScriptModule[0]?Header.ScriptModule:"-");
	fm->SetStr("%-20s %s\n","ScriptFunc",Header.ScriptFunc[0]?Header.ScriptFunc:"-");
	fm->SetStr("%-20s %d\n","NoLogOut",Header.NoLogOut);
	fm->SetStr("%-20s %d\n","Time",Header.Time);
	fm->SetStr("%-20s %-4d %-4d %-4d %-4d\n","DayTime",Header.DayTime[0],Header.DayTime[1],Header.DayTime[2],Header.DayTime[3]);
	fm->SetStr("%-20s %-3d %-3d %-3d\n","DayColor0",Header.DayColor[0],Header.DayColor[4],Header.DayColor[8]);
	fm->SetStr("%-20s %-3d %-3d %-3d\n","DayColor1",Header.DayColor[1],Header.DayColor[5],Header.DayColor[9]);
	fm->SetStr("%-20s %-3d %-3d %-3d\n","DayColor2",Header.DayColor[2],Header.DayColor[6],Header.DayColor[10]);
	fm->SetStr("%-20s %-3d %-3d %-3d\n","DayColor3",Header.DayColor[3],Header.DayColor[7],Header.DayColor[11]);
	fm->SetStr("\n");

	// Tiles
	fm->SetStr("[%s]\n",APP_TILES);
	char tile_str[256];
	for(uint i=0,j=Tiles.size();i<j;i++)
	{
		Tile& tile=Tiles[i];
		const char* name=Str::GetName(tile.NameHash);
		if(name)
		{
			bool has_offs=(tile.OffsX || tile.OffsY);
			bool has_layer=(tile.Layer!=0);

			StringCopy(tile_str,tile.IsRoof?"roof":"tile");
			if(has_offs || has_layer) StringAppend(tile_str,"_");
			if(has_offs)  StringAppend(tile_str,"o");
			if(has_layer)  StringAppend(tile_str,"l");

			fm->SetStr("%-10s %-4d %-4d ",tile_str,tile.HexX,tile.HexY);

			if(has_offs)
				fm->SetStr("%-3d %-3d ",tile.OffsX,tile.OffsY);
			else
				fm->SetStr("        ");

			if(has_layer)
				fm->SetStr("%d ",tile.Layer);
			else
				fm->SetStr("  ");

			fm->SetStr("%s\n",name);
		}
	}
	fm->SetStr("\n");

	// Objects
	fm->SetStr("[%s]\n",APP_OBJECTS);
	for(uint k=0;k<MObjects.size();k++)
	{
		MapObject& mobj=*MObjects[k];
		// Shared
		fm->SetStr("%-20s %d\n","MapObjType",mobj.MapObjType);
		fm->SetStr("%-20s %d\n","ProtoId",mobj.ProtoId);
		if(mobj.MapX) fm->SetStr("%-20s %d\n","MapX",mobj.MapX);
		if(mobj.MapY) fm->SetStr("%-20s %d\n","MapY",mobj.MapY);
		if(mobj.Dir) fm->SetStr("%-20s %d\n","Dir",mobj.Dir);
		if(mobj.UID) fm->SetStr("%-20s %d\n","UID",mobj.UID);
		if(mobj.ContainerUID) fm->SetStr("%-20s %d\n","ContainerUID",mobj.ContainerUID);
		if(mobj.ParentUID) fm->SetStr("%-20s %d\n","ParentUID",mobj.ParentUID);
		if(mobj.ParentChildIndex) fm->SetStr("%-20s %d\n","ParentChildIndex",mobj.ParentChildIndex);
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
			fm->SetStr("%-20s %d\n","Critter_Anim1",mobj.MCritter.Anim1);
			fm->SetStr("%-20s %d\n","Critter_Anim2",mobj.MCritter.Anim2);
			for(int i=0;i<MAPOBJ_CRITTER_PARAMS;i++)
			{
				if(mobj.MCritter.ParamIndex[i]>=0 && mobj.MCritter.ParamIndex[i]<MAX_PARAMS)
				{
					const char* param_name=ConstantsManager::GetParamName(mobj.MCritter.ParamIndex[i]);
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
				if(mobj.MItem.BrokenFlags) fm->SetStr("%-20s %d\n","Item_BrokenFlags",mobj.MItem.BrokenFlags);
				if(mobj.MItem.BrokenCount) fm->SetStr("%-20s %d\n","Item_BrokenCount",mobj.MItem.BrokenCount);
				if(mobj.MItem.Deterioration) fm->SetStr("%-20s %d\n","Item_Deterioration",mobj.MItem.Deterioration);
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
	uint version=fm->GetBEUInt();
	if(version!=SERVER_VERSION) return false;
	fm->GetBEUInt();
	fm->GetBEUInt();
	fm->GetBEUInt();

	// Header
	if(!fm->CopyMem(&Header,sizeof(Header))) return false;

	// Tiles
	uint tiles_count=fm->GetBEUInt();
	if(tiles_count)
	{
		Tiles.resize(tiles_count);
		fm->CopyMem(&Tiles[0],tiles_count*sizeof(Tile));
	}

	// Critters
	uint count=fm->GetBEUInt();
	CrittersVec.reserve(count);
	for(uint i=0;i<count;i++)
	{
		MapObject* mobj=new MapObject();
		fm->CopyMem(mobj,sizeof(MapObject)-sizeof(MapObject::_RunTime));
		CrittersVec.push_back(mobj);
	}

	// Items
	count=fm->GetBEUInt();
	ItemsVec.reserve(count);
	for(uint i=0;i<count;i++)
	{
		MapObject* mobj=new MapObject();
		fm->CopyMem(mobj,sizeof(MapObject)-sizeof(MapObject::_RunTime));
		ItemsVec.push_back(mobj);
	}

	// Scenery
	count=fm->GetBEUInt();
	SceneryVec.reserve(count);
	for(uint i=0;i<count;i++)
	{
		MapObject* mobj=new MapObject();
		fm->CopyMem(mobj,sizeof(MapObject));
		SceneryVec.push_back(mobj);
		mobj->RunTime.RefCounter=1;
		mobj->RunTime.BindScriptId=0;
		if(mobj->ScriptName[0] && mobj->FuncName[0]) BindSceneryScript(mobj);
	}

	// Grids
	count=fm->GetBEUInt();
	GridsVec.reserve(count);
	for(uint i=0;i<count;i++)
	{
		MapObject* mobj=new MapObject();
		fm->CopyMem(mobj,sizeof(MapObject));
		GridsVec.push_back(mobj);
		mobj->RunTime.RefCounter=1;
		mobj->RunTime.BindScriptId=0;
	}

	// To send
	count=fm->GetBEUInt();
	if(count)
	{
		WallsToSend.resize(count);
		if(!fm->CopyMem(&WallsToSend[0],count*sizeof(SceneryCl))) return false;
	}
	count=fm->GetBEUInt();
	if(count)
	{
		SceneriesToSend.resize(count);
		if(!fm->CopyMem(&SceneriesToSend[0],count*sizeof(SceneryCl))) return false;
	}

	// Hashes
	HashTiles=fm->GetBEUInt();
	HashWalls=fm->GetBEUInt();
	HashScen=fm->GetBEUInt();

	// Hex flags
	HexFlags=new uchar[Header.MaxHexX*Header.MaxHexY];
	if(!HexFlags) return false;
	if(!fm->CopyMem(HexFlags,Header.MaxHexX*Header.MaxHexY)) return false;

	// Entires
	count=fm->GetBEUInt();
	if(count)
	{
		mapEntires.resize(count);
		if(!fm->CopyMem(&mapEntires[0],count*sizeof(MapEntire))) return false;
	}

	MEMORY_PROCESS(MEMORY_PROTO_MAP,SceneriesToSend.capacity()*sizeof(SceneryCl));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,WallsToSend.capacity()*sizeof(SceneryCl));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,mapEntires.capacity()*sizeof(MapEntire));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,CrittersVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,ItemsVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,SceneryVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,GridsVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,Header.MaxHexX*Header.MaxHexY);
	MEMORY_PROCESS(MEMORY_PROTO_MAP,Tiles.capacity()*sizeof(Tile));
	return true;
}

void ProtoMap::SaveCache(FileManager* fm)
{
	// Server version
	fm->SetBEUInt(SERVER_VERSION);
	fm->SetBEUInt(0);
	fm->SetBEUInt(0);
	fm->SetBEUInt(0);

	// Header
	fm->SetData(&Header,sizeof(Header));

	// Tiles
	fm->SetBEUInt(Tiles.size());
	if(Tiles.size()) fm->SetData(&Tiles[0],Tiles.size()*sizeof(Tile));

	// Critters
	fm->SetBEUInt(CrittersVec.size());
	for(MapObjectPtrVecIt it=CrittersVec.begin(),end=CrittersVec.end();it!=end;++it)
		fm->SetData(*it,sizeof(MapObject)-sizeof(MapObject::_RunTime));

	// Items
	fm->SetBEUInt(ItemsVec.size());
	for(MapObjectPtrVecIt it=ItemsVec.begin(),end=ItemsVec.end();it!=end;++it)
		fm->SetData(*it,sizeof(MapObject)-sizeof(MapObject::_RunTime));

	// Scenery
	fm->SetBEUInt(SceneryVec.size());
	for(MapObjectPtrVecIt it=SceneryVec.begin(),end=SceneryVec.end();it!=end;++it)
		fm->SetData(*it,sizeof(MapObject));

	// Grids
	fm->SetBEUInt(GridsVec.size());
	for(MapObjectPtrVecIt it=GridsVec.begin(),end=GridsVec.end();it!=end;++it)
		fm->SetData(*it,sizeof(MapObject));

	// To send
	fm->SetBEUInt(WallsToSend.size());
	fm->SetData(&WallsToSend[0],WallsToSend.size()*sizeof(SceneryCl));
	fm->SetBEUInt(SceneriesToSend.size());
	fm->SetData(&SceneriesToSend[0],SceneriesToSend.size()*sizeof(SceneryCl));

	// Hashes
	fm->SetBEUInt(HashTiles);
	fm->SetBEUInt(HashWalls);
	fm->SetBEUInt(HashScen);

	// Hex flags
	fm->SetData(HexFlags,Header.MaxHexX*Header.MaxHexY);

	// Entires
	fm->SetBEUInt(mapEntires.size());
	fm->SetData(&mapEntires[0],mapEntires.size()*sizeof(MapEntire));

	// Save
	char fname[MAX_PATH];
	sprintf(fname,"%s%sb",pmapName.c_str(),MAP_PROTO_EXT);
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
		WriteLog(_FUNC_," - Map<%s>, Can't bind scenery function<%s> in module<%s>. Scenery hexX<%u>, hexY<%u>.\n",map_info,
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
	string fname_txt=pmapName+MAP_PROTO_EXT;
	string fname_bin=pmapName+string(MAP_PROTO_EXT)+"b";
	string fname_map=pmapName+".map"; // Deprecated

#ifdef FONLINE_SERVER
	// Cached binary
	FileManager cached;
	cached.LoadFile(fname_bin.c_str(),pathType);

	// Load text or binary
	bool text=true;
	if(!pmapFm->LoadFile(fname_txt.c_str(),pathType))
	{
		text=false;
		if(!pmapFm->LoadFile(fname_map.c_str(),pathType) && !cached.IsLoaded())
		{
			WriteLog(_FUNC_," - Load file fail, file name<%s>, folder<%s>.\n",pmapName.c_str(),pmapFm->GetPath(pathType));
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
				WriteLog(_FUNC_," - Map<%s>. Can't read cached map file.\n",map_info);
				return false;
			}
		}
	}
#endif // FONLINE_SERVER
#ifdef FONLINE_MAPPER
	// Load binary or text
	bool text=true;
	if(!pmapFm->LoadFile(fname_txt.c_str(),pathType))
	{
		text=false;
		if(!pmapFm->LoadFile(fname_map.c_str(),pathType))
		{
			WriteLog(_FUNC_," - Load file fail, file name<%s>, folder<%s>.\n",pmapName.c_str(),pmapFm->GetPath(pathType));
			return false;
		}
	}
#endif // FONLINE_MAPPER

	// Load
	if(text)
	{
		if(!LoadTextFormat((const char*)pmapFm->GetBuf()))
		{
			WriteLog(_FUNC_," - Map<%s>. Can't read text map format.\n",map_info);
			return false;
		}
	}
	else
	{
		// Deprecated
		// Check map format version
		// Read version
		uint version_full=pmapFm->GetLEUInt();

		// Check version
		if(version_full==F1_MAP_VERSION)
		{
			WriteLog(_FUNC_," - Map<%s>. FOnline not support F1 map format.\n",map_info);
			return false;
		}

		if(version_full==F2_MAP_VERSION)
		{
			WriteLog(_FUNC_," - Map<%s>. FOnline not support F2 map format.\n",map_info);
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
			WriteLog(_FUNC_," - Map<%s>. Unknown map format<%u>.\n",map_info,version_full);
			return false;
		}

		if(version<0)
		{
			WriteLog(_FUNC_," - Map<%s>. Map format not supproted, resave in Mapper v.1.17.2, than open again.\n",map_info);
			return false;
		}

		// Read Header
		if(!ReadHeader(version))
		{
			WriteLog(_FUNC_," - Map<%s>. Can't read Header.\n",map_info);
			return false;
		}

		// Unpack
		if(Header.Packed)
		{
			pmapFm->SetCurPos(Header.HeaderSize);
			uint pack_len=pmapFm->GetFsize()-Header.HeaderSize;
			uint unpack_len=Header.UnpackedDataLen;
			uchar* data=Crypt.Uncompress(pmapFm->GetCurBuf(),pack_len,unpack_len/pack_len+1);
			if(!data)
			{
				WriteLog(_FUNC_," - Map<%s>. Unable unpack data.\n",map_info);
				return false;
			}
			pmapFm->LoadStream(data,pack_len);
			delete[] data;
		}

		// Read Tiles
		if(!ReadTiles(version))
		{
			WriteLog(_FUNC_," - Map<%s>. Can't read Tiles.\n",map_info);
			return false;
		}

		// Read Objects
		if(!ReadObjects(version))
		{
			WriteLog(_FUNC_," - Map<%s>. Can't read Objects.\n",map_info);
			return false;
		}

		Header.Version=FO_MAP_VERSION_TEXT1;
	}
	pmapFm->UnloadFile();

	// Deprecated, add UIDs
	if(Header.Version<FO_MAP_VERSION_TEXT4)
	{
		uint uid=0;
		for(uint i=0,j=MObjects.size();i<j;i++)
		{
			MapObject* mobj=MObjects[i];

			// Find item in container
			if(mobj->MapObjType!=MAP_OBJECT_ITEM || !mobj->ContainerUID) continue;

			// Find container
			for(uint k=0,l=MObjects.size();k<l;k++)
			{
				MapObject* mobj_=MObjects[k];
				if(mobj_->MapX!=mobj->MapX || mobj_->MapY!=mobj->MapY) continue;
				if(mobj_->MapObjType!=MAP_OBJECT_ITEM && mobj_->MapObjType!=MAP_OBJECT_CRITTER) continue;
				if(mobj_==mobj) continue;
				if(mobj_->MapObjType!=MAP_OBJECT_ITEM)
				{
					ProtoItem* proto_item=ItemMngr.GetProtoItem(mobj_->ProtoId);
					if(!proto_item || proto_item->Type!=ITEM_TYPE_CONTAINER) continue;
				}
				if(!mobj_->UID) mobj_->UID=++uid;
				mobj->ContainerUID=mobj_->UID;
			}
		}
	}

	// Fix child objects positions
	for(uint i=0,j=MObjects.size();i<j;)
	{
		MapObject* mobj_child=MObjects[i];
		if(!mobj_child->ParentUID)
		{
			i++;
			continue;
		}

		bool delete_child=true;
		for(uint k=0,l=MObjects.size();k<l;k++)
		{
			MapObject* mobj_parent=MObjects[k];
			if(!mobj_parent->UID || mobj_parent->UID!=mobj_child->ParentUID || mobj_parent==mobj_child) continue;

			ProtoItem* proto_parent=ItemMngr.GetProtoItem(mobj_parent->ProtoId);
			if(!proto_parent || !proto_parent->ChildPid[mobj_child->ParentChildIndex]) break;

			ushort child_hx=mobj_parent->MapX,child_hy=mobj_parent->MapY;
			FOREACH_PROTO_ITEM_LINES(proto_parent->ChildLines[mobj_child->ParentChildIndex],child_hx,child_hy,Header.MaxHexX,Header.MaxHexY,;);

			mobj_child->MapX=child_hx;
			mobj_child->MapY=child_hy;
			mobj_child->ProtoId=proto_parent->ChildPid[mobj_child->ParentChildIndex];
			delete_child=false;
			break;
		}

		if(delete_child)
		{
			MObjects[i]->Release();
			MObjects.erase(MObjects.begin()+i);
			j=MObjects.size();
		}
		else
		{
			i++;
		}
	}

#ifdef FONLINE_SERVER
	// Parse objects
	WallsToSend.clear();
	SceneriesToSend.clear();
	ushort maxhx=Header.MaxHexX;
	ushort maxhy=Header.MaxHexY;

	HexFlags=new uchar[Header.MaxHexX*Header.MaxHexY];
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

		ushort pid=mobj.ProtoId;
		ushort hx=mobj.MapX;
		ushort hy=mobj.MapY;

		ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
		if(!proto_item)
		{
			WriteLog(_FUNC_," - Map<%s>, Unknown prototype<%u>, hexX<%u>, hexY<%u>.\n",map_info,pid,hx,hy);
			continue;
		}

		if(hx>=maxhx || hy>=maxhy)
		{
			WriteLog(_FUNC_," - Invalid object position on map<%s>, pid<%u>, hexX<%u>, hexY<%u>.\n",map_info,pid,hx,hy);
			continue;
		}

		int type=proto_item->Type;
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
				SceneryCl cur_wall;
				ZeroMemory(&cur_wall,sizeof(SceneryCl));

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
						ushort hx_=hx,hy_=hy;
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
				SceneryCl cur_scen;
				ZeroMemory(&cur_scen,sizeof(SceneryCl));

				// Flags
				if(type==ITEM_TYPE_GENERIC && mobj.MScenery.CanUse) SETFLAG(cur_scen.Flags,SCEN_CAN_USE);
				if(type==ITEM_TYPE_GENERIC && mobj.MScenery.CanTalk) SETFLAG(cur_scen.Flags,SCEN_CAN_TALK);
				if(type==ITEM_TYPE_GRID && proto_item->Grid_Type!=GRID_EXITGRID) SETFLAG(cur_scen.Flags,SCEN_CAN_USE);

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
	if(Tiles.size()) Crypt.Crc32((uchar*)&Tiles[0],Tiles.size()*sizeof(Tile),HashTiles);
	HashWalls=maxhx*maxhy;
	if(WallsToSend.size()) Crypt.Crc32((uchar*)&WallsToSend[0],WallsToSend.size()*sizeof(SceneryCl),HashWalls);
	HashScen=maxhx*maxhy;
	if(SceneriesToSend.size()) Crypt.Crc32((uchar*)&SceneriesToSend[0],SceneriesToSend.size()*sizeof(SceneryCl),HashScen);

	MEMORY_PROCESS(MEMORY_PROTO_MAP,SceneriesToSend.capacity()*sizeof(SceneryCl));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,WallsToSend.capacity()*sizeof(SceneryCl));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,mapEntires.capacity()*sizeof(MapEntire));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,CrittersVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,ItemsVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,SceneryVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,GridsVec.size()*sizeof(MapObject));
	MEMORY_PROCESS(MEMORY_PROTO_MAP,Header.MaxHexX*Header.MaxHexY);
	MEMORY_PROCESS(MEMORY_PROTO_MAP,Tiles.capacity()*sizeof(Tile));

	SaveCache(pmapFm);
#endif

#ifdef FONLINE_MAPPER
	// Post process objects
	for(uint i=0,j=MObjects.size();i<j;i++)
	{
		MapObject* mobj=MObjects[i];

		// Map link
		mobj->RunTime.FromMap=this;

		// Convert hashes to names
		if(mobj->MapObjType==MAP_OBJECT_ITEM || mobj->MapObjType==MAP_OBJECT_SCENERY)
		{
			if(mobj->MItem.PicMapHash && !mobj->RunTime.PicMapName[0]) StringCopy(mobj->RunTime.PicMapName,Str::GetName(mobj->MItem.PicMapHash));
			if(mobj->MItem.PicInvHash && !mobj->RunTime.PicInvName[0]) StringCopy(mobj->RunTime.PicInvName,Str::GetName(mobj->MItem.PicInvHash));
		}

		// Last UID
		if(mobj->UID>LastObjectUID) LastObjectUID=mobj->UID;
	}

	// Create cached fields
	TilesField.resize(Header.MaxHexX*Header.MaxHexY);
	RoofsField.resize(Header.MaxHexX*Header.MaxHexY);
	for(uint i=0,j=Tiles.size();i<j;i++)
	{
		if(!Tiles[i].IsRoof)
		{
			TilesField[Tiles[i].HexY*Header.MaxHexX+Tiles[i].HexX].push_back(Tiles[i]);
			TilesField[Tiles[i].HexY*Header.MaxHexX+Tiles[i].HexX].back().IsSelected=false;
		}
		else
		{
			RoofsField[Tiles[i].HexY*Header.MaxHexX+Tiles[i].HexX].push_back(Tiles[i]);
			RoofsField[Tiles[i].HexY*Header.MaxHexX+Tiles[i].HexX].back().IsSelected=false;
		}
	}
	Tiles.clear();
#endif
	return true;
}

#ifdef FONLINE_MAPPER
void ProtoMap::GenNew(FileManager* fm)
{
	Clear();
	Header.Version=FO_MAP_VERSION_V5;
	Header.MaxHexX=MAXHEX_DEF;
	Header.MaxHexY=MAXHEX_DEF;
	pmapPid=0xFFFF;
	pmapFm=fm;
	pathType=PT_MAPS;

	// Morning	 5.00 -  9.59	 300 - 599
	// Day		10.00 - 18.59	 600 - 1139
	// Evening	19.00 - 22.59	1140 - 1379
	// Nigh		23.00 -  4.59	1380
	Header.DayTime[0]=300; Header.DayTime[1]=600;  Header.DayTime[2]=1140; Header.DayTime[3]=1380;
	Header.DayColor[0]=18; Header.DayColor[1]=128; Header.DayColor[2]=103; Header.DayColor[3]=51;
	Header.DayColor[4]=18; Header.DayColor[5]=128; Header.DayColor[6]=95;  Header.DayColor[7]=40;
	Header.DayColor[8]=53; Header.DayColor[9]=128; Header.DayColor[10]=86; Header.DayColor[11]=29;

#ifdef FONLINE_MAPPER
	TilesField.resize(Header.MaxHexX*Header.MaxHexY);
	RoofsField.resize(Header.MaxHexX*Header.MaxHexY);
#endif

	isInit=true;
}

bool ProtoMap::Save(const char* f_name, int path_type)
{
	if(f_name && *f_name) pmapName=f_name;
	if(path_type>=0) pathType=path_type;

	// Fill tiles from cached fields
	TilesField.resize(Header.MaxHexX*Header.MaxHexY);
	RoofsField.resize(Header.MaxHexX*Header.MaxHexY);
	for(ushort hy=0;hy<Header.MaxHexY;hy++)
	{
		for(ushort hx=0;hx<Header.MaxHexX;hx++)
		{
			TileVec& tiles=TilesField[hy*Header.MaxHexX+hx];
			for(uint i=0,j=tiles.size();i<j;i++) Tiles.push_back(tiles[i]);
			TileVec& roofs=RoofsField[hy*Header.MaxHexX+hx];
			for(uint i=0,j=roofs.size();i<j;i++) Tiles.push_back(roofs[i]);
		}
	}

	// Delete non used UIDs
	for(uint i=0,j=MObjects.size();i<j;i++)
	{
		MapObject* mobj=MObjects[i];
		if(!mobj->UID) continue;

		bool founded=false;
		for(uint k=0,l=MObjects.size();k<l;k++)
		{
			MapObject* mobj_=MObjects[k];
			if(mobj_->ContainerUID==mobj->UID || mobj_->ParentUID==mobj->UID)
			{
				founded=true;
				break;
			}
		}
		if(!founded) mobj->UID=0;
	}

	// Save
	pmapFm->ClearOutBuf();
	Header.Version=FO_MAP_VERSION_TEXT4;
	SaveTextFormat(pmapFm);
	Tiles.clear();

	string fname=pmapName+MAP_PROTO_EXT;
	if(!pmapFm->SaveOutBufToFile(fname.c_str(),pathType))
	{
		WriteLog(_FUNC_," - Unable write file.\n");
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
		// Check text format
		IniParser txt;
		if(!txt.LoadFile(fname,PT_ROOT)) return false;
		return txt.IsApp(APP_HEADER) && txt.IsApp(APP_TILES) && txt.IsApp(APP_OBJECTS);
	}
	else if(!_stricmp(ext,".map"))
	{
		// Check binary format
		FileManager file;
		if(!file.LoadFile(fname,PT_ROOT)) return false;
		uint version=file.GetLEUInt();
		return version==FO_MAP_VERSION_V4 || version==FO_MAP_VERSION_V5 ||
			version==FO_MAP_VERSION_V6 || version==FO_MAP_VERSION_V7 ||
			version==FO_MAP_VERSION_V8 || version==FO_MAP_VERSION_V9;
	}

	return false;
}
#endif // FONLINE_MAPPER

#ifdef FONLINE_SERVER
uint ProtoMap::CountEntire(uint num)
{
	if(num==uint(-1)) return mapEntires.size();

	uint result=0;
	for(int i=0,j=mapEntires.size();i<j;i++)
	{
		if(mapEntires[i].Number==num) result++;
	}
	return result;
}

ProtoMap::MapEntire* ProtoMap::GetEntire(uint num, uint skip)
{
	for(uint i=0,j=mapEntires.size();i<j;i++)
	{
		if(num==uint(-1) || mapEntires[i].Number==num)
		{
			if(!skip) return &mapEntires[i];
			else skip--;
		}
	}

	return NULL;
}

ProtoMap::MapEntire* ProtoMap::GetEntireRandom(uint num)
{
	vector<MapEntire*> entires;
	for(uint i=0,j=mapEntires.size();i<j;i++)
	{
		if(num==uint(-1) || mapEntires[i].Number==num) entires.push_back(&mapEntires[i]);
	}

	if(entires.empty()) return NULL;
	return entires[Random(0,entires.size()-1)];
}

ProtoMap::MapEntire* ProtoMap::GetEntireNear(uint num, ushort hx, ushort hy)
{
	MapEntire* near_entire=NULL;
	uint last_dist=0;
	for(uint i=0,j=mapEntires.size();i<j;i++)
	{
		MapEntire& entire=mapEntires[i];
		if(num==uint(-1) || entire.Number==num)
		{
			uint dist=DistGame(hx,hy,entire.HexX,entire.HexY);
			if(!near_entire || dist<last_dist)
			{
				near_entire=&entire;
				last_dist=dist;
			}
		}
	}
	return near_entire;
}

ProtoMap::MapEntire* ProtoMap::GetEntireNear(uint num, uint num_ext, ushort hx, ushort hy)
{
	MapEntire* near_entire=NULL;
	uint last_dist=0;
	for(uint i=0,j=mapEntires.size();i<j;i++)
	{
		MapEntire& entire=mapEntires[i];
		if(num==uint(-1) || num_ext==uint(-1) || entire.Number==num || entire.Number==num_ext)
		{
			uint dist=DistGame(hx,hy,entire.HexX,entire.HexY);
			if(!near_entire || dist<last_dist)
			{
				near_entire=&entire;
				last_dist=dist;
			}
		}
	}
	return near_entire;
}

void ProtoMap::GetEntires(uint num, EntiresVec& entires)
{
	for(uint i=0,j=mapEntires.size();i<j;i++)
	{
		MapEntire& entire=mapEntires[i];
		if(num==uint(-1) || entire.Number==num) entires.push_back(entire);
	}
}

MapObject* ProtoMap::GetMapScenery(ushort hx, ushort hy, ushort pid)
{
	for(MapObjectPtrVecIt it=SceneryVec.begin(),end=SceneryVec.end();it!=end;++it)
	{
		MapObject* mobj=*it;
		if((!pid || mobj->ProtoId==pid) && mobj->MapX==hx && mobj->MapY==hy) return mobj;
	}
	return NULL;
}

void ProtoMap::GetMapSceneriesHex(ushort hx, ushort hy, MapObjectPtrVec& mobjs)
{
	for(MapObjectPtrVecIt it=SceneryVec.begin(),end=SceneryVec.end();it!=end;++it)
	{
		MapObject* mobj=*it;
		if(mobj->MapX==hx && mobj->MapY==hy) mobjs.push_back(mobj);
	}
}

void ProtoMap::GetMapSceneriesHexEx(ushort hx, ushort hy, uint radius, ushort pid, MapObjectPtrVec& mobjs)
{
	for(MapObjectPtrVecIt it=SceneryVec.begin(),end=SceneryVec.end();it!=end;++it)
	{
		MapObject* mobj=*it;
		if((!pid || mobj->ProtoId==pid) && DistGame(mobj->MapX,mobj->MapY,hx,hy)<=radius) mobjs.push_back(mobj);
	}
}

void ProtoMap::GetMapSceneriesByPid(ushort pid, MapObjectPtrVec& mobjs)
{
	for(MapObjectPtrVecIt it=SceneryVec.begin(),end=SceneryVec.end();it!=end;++it)
	{
		MapObject* mobj=*it;
		if(!pid || mobj->ProtoId==pid) mobjs.push_back(mobj);
	}
}

MapObject* ProtoMap::GetMapGrid(ushort hx, ushort hy)
{
	for(MapObjectPtrVecIt it=GridsVec.begin(),end=GridsVec.end();it!=end;++it)
	{
		MapObject* mobj=*it;
		if(mobj->MapX==hx && mobj->MapY==hy) return mobj;
	}
	return NULL;
}

#endif // FONLINE_SERVER
