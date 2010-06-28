#ifndef __PROTO_MAP__
#define __PROTO_MAP__

#include "Common.h"
#include "NetProtocol.h"
#include "FileManager.h"

// Other
#define MAP_OBJECT_SIZE         (240)
#define MAPOBJ_SCRIPT_NAME      (25)
#define MAPOBJ_CRITTER_PARAMS   (15)

// Map object types
#define MAP_OBJECT_CRITTER      (0)
#define MAP_OBJECT_ITEM         (1)
#define MAP_OBJECT_SCENERY      (2)

class ProtoMap;
class MapObject
{
public:
	BYTE MapObjType;
	// BYTE
	WORD ProtoId;
	WORD MapX;
	WORD MapY;
	BYTE Dir;
	// BYTE[3]

	DWORD LightColor;
	BYTE LightDay;
	BYTE LightDirOff;
	BYTE LightDistance;
	char LightIntensity;

	char ScriptName[MAPOBJ_SCRIPT_NAME+1];
	char FuncName[MAPOBJ_SCRIPT_NAME+1];

	DWORD Reserved[7];
	int UserData[10];

	union
	{
		struct
		{
			BYTE Cond;
			BYTE CondExt;
			short ParamIndex[MAPOBJ_CRITTER_PARAMS];
			int ParamValue[MAPOBJ_CRITTER_PARAMS];
		} MCritter;

		struct
		{
			short OffsetX;
			short OffsetY;
			BYTE AnimStayBegin;
			BYTE AnimStayEnd;
			WORD AnimWait;
			WORD PicMapDeprecated;
			WORD PicInvDeprecated;
			BYTE InfoOffset;
			BYTE Reserved[3];
			DWORD PicMapHash;
			DWORD PicInvHash;

			DWORD Count;

			BYTE DeteorationFlags;
			BYTE DeteorationCount;
			WORD DeteorationValue;

			bool InContainer;
			BYTE ItemSlot;

			WORD AmmoPid;
			DWORD AmmoCount;

			DWORD LockerDoorId;
			WORD LockerCondition;
			WORD LockerComplexity;

			short TrapValue;
			// WORD

			int Val[10];
		} MItem;

		struct
		{
			short OffsetX;
			short OffsetY;
			BYTE AnimStayBegin;
			BYTE AnimStayEnd;
			WORD AnimWait;
			WORD PicMapDeprecated;
			WORD PicInvDeprecated;
			BYTE InfoOffset;
			BYTE Reserved[3];
			DWORD PicMapHash;
			DWORD PicInvHash;

			bool CanUse;
			bool CanTalk;
			// BYTE[2]
			DWORD TriggerNum;

			// BYTE[3]
			BYTE ParamsCount;
			int Param[5];

			WORD ToMapPid;
			// WORD
			DWORD ToEntire;
			WORD ToMapX;
			WORD ToMapY;
			BYTE ToDir;
			// BYTE[3]
		} MScenery;

		struct
		{
			DWORD Buffer[25];
		} MAlign;
	};

	struct _RunTime
	{
#ifdef FONLINE_MAPPER
		ProtoMap* FromMap;
		DWORD MapObjId;
		char PicMapName[64];
		char PicInvName[64];
#endif
#ifdef FONLINE_SERVER
		int BindScriptId;
#endif
		long RefCounter;
	} RunTime;

	MapObject(){ZeroMemory(this,sizeof(MapObject)); RunTime.RefCounter=1;}
	MapObject(const MapObject& r){memcpy(this,&r,sizeof(MapObject)); RunTime.RefCounter=1;}
	MapObject& operator=(const MapObject& r){memcpy(this,&r,sizeof(MapObject)); RunTime.RefCounter=1; return *this;}
	
	void AddRef(){++RunTime.RefCounter;}
	void Release(){if(!--RunTime.RefCounter) delete this;}
};
typedef vector<MapObject*> MapObjectPtrVec;
typedef vector<MapObject*>::iterator MapObjectPtrVecIt;
typedef vector<MapObject> MapObjectVec;
typedef vector<MapObject>::iterator MapObjectVecIt;

/*class Scenery
{
	WORD ProtoId;
	WORD MapX;
	WORD MapY;
	bool IsGrid;
};*/

class ProtoMap
{
public:
	struct
	{
		DWORD Version;
		bool Packed;
		bool NoLogOut;
		WORD HeaderSize;
		int PlayersLimit;
		DWORD UnpackedDataLen;
		WORD MaxHexX;
		WORD MaxHexY;
		int Time;
		int CenterX;
		int CenterY;
		char ScriptModule[MAX_SCRIPT_NAME+1];
		char ScriptFunc[MAX_SCRIPT_NAME+1];
		int DayTime[4];
		BYTE DayColor[12];
	} Header;

	MapObjectPtrVec MObjects;
	DWORD* Tiles;

	DWORD GetObjectsSize(){return MObjects.size()*MAP_OBJECT_SIZE;}
	DWORD GetTilesSize(){return (Header.MaxHexX/2)*(Header.MaxHexY/2)*sizeof(DWORD)*2;}

	DWORD GetTile(WORD tx, WORD ty){return Tiles[ty*(Header.MaxHexX/2)*2+tx*2];}
	DWORD GetRoof(WORD tx, WORD ty){return Tiles[ty*(Header.MaxHexX/2)*2+tx*2+1];}
	void SetTile(WORD tx, WORD ty, DWORD pic_hash){Tiles[ty*(Header.MaxHexX/2)*2+tx*2]=pic_hash;}
	void SetRoof(WORD tx, WORD ty, DWORD pic_hash){Tiles[ty*(Header.MaxHexX/2)*2+tx*2+1]=pic_hash;}

private:
	bool ReadHeader(int version);
	bool ReadTiles(int version);
	bool ReadObjects(int version);
	bool LoadTextFormat(const char* buf);
#ifdef FONLINE_MAPPER
	void SaveTextFormat(FileManager* fm);
#endif

#ifdef FONLINE_SERVER
public:
	// To Client
	ScenToSendVec WallsToSend;
	ScenToSendVec SceneriesToSend;
	DWORD HashTiles;
	DWORD HashWalls;
	DWORD HashScen;

	MapObjectPtrVec CrittersVec;
	MapObjectPtrVec ItemsVec;
	MapObjectPtrVec SceneryVec;
	MapObjectPtrVec GridsVec;
	BYTE* HexFlags;

private:
	bool LoadCache(FileManager* fm);
	void SaveCache(FileManager* fm);
	void BindSceneryScript(MapObject* mobj);
#endif

public:
	// Entires
	struct MapEntire 
	{
		DWORD Number;
		WORD HexX;
		WORD HexY;
		BYTE Dir;

		MapEntire(){ZeroMemory(this,sizeof(MapEntire));}
		MapEntire(WORD hx, WORD hy, BYTE dir, DWORD type):HexX(hx),HexY(hy),Dir(dir),Number(type){}
	};
typedef vector<MapEntire> EntiresVec;

private:
	EntiresVec mapEntires;

public:
	DWORD CountEntire(DWORD num);
	MapEntire* GetEntire(DWORD num, DWORD skip);
	MapEntire* GetEntireRandom(DWORD num);
	MapEntire* GetEntireNear(DWORD num, WORD hx, WORD hy);
	MapEntire* GetEntireNear(DWORD num, DWORD num_ext, WORD hx, WORD hy);
	void GetEntires(DWORD num, EntiresVec& entires);

private:
	int pathType;
	string pmapName;
	WORD pmapPid;

	bool isInit;
	FileManager* pmapFm;

public:
	bool Init(WORD pid, const char* name, int path_type);
	void Clear();
	bool Refresh();

#ifdef FONLINE_MAPPER
	void GenNew(FileManager* fm);
	bool Save(const char* f_name, int path_type, bool text, bool pack);
	static bool IsMapFile(const char* fname);
#endif

	bool IsInit(){return isInit;}
	WORD GetPid(){return isInit?pmapPid:0;}
	const char* GetName(){return pmapName.c_str();}

	long RefCounter;
	void AddRef(){++RefCounter;}
	void Release(){if(!--RefCounter) delete this;}

#ifdef FONLINE_SERVER
	MapObject* GetMapScenery(WORD hx, WORD hy, WORD pid);
	void GetMapSceneriesHex(WORD hx, WORD hy, MapObjectPtrVec& mobjs);
	void GetMapSceneriesHexEx(WORD hx, WORD hy, DWORD radius, WORD pid, MapObjectPtrVec& mobjs);
	void GetMapSceneriesByPid(WORD pid, MapObjectPtrVec& mobjs);
	MapObject* GetMapGrid(WORD hx, WORD hy);
	ProtoMap():pmapFm(NULL),isInit(false),pathType(0),HexFlags(NULL),Tiles(NULL){MEMORY_PROCESS(MEMORY_PROTO_MAP,sizeof(ProtoMap));}
	ProtoMap(const ProtoMap& r){*this=r;MEMORY_PROCESS(MEMORY_PROTO_MAP,sizeof(ProtoMap));}
	~ProtoMap(){pmapFm=NULL;isInit=false;HexFlags=NULL;Tiles=NULL;MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)sizeof(ProtoMap));}
#else
	ProtoMap():pmapFm(NULL),isInit(false),pathType(0),RefCounter(1){Tiles=NULL;}
	~ProtoMap(){pmapFm=NULL;isInit=false;Tiles=NULL;}
#endif
};
typedef vector<ProtoMap> ProtoMapVec;
typedef vector<ProtoMap>::iterator ProtoMapVecIt;
typedef vector<ProtoMap>::value_type ProtoMapVecVal;
typedef vector<ProtoMap*> ProtoMapPtrVec;
typedef vector<ProtoMap*>::iterator ProtoMapPtrVecIt;
typedef vector<ProtoMap*>::value_type ProtoMapPtrVecVal;

#endif // __PROTO_MAP__