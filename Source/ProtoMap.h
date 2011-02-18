#ifndef __PROTO_MAP__
#define __PROTO_MAP__

#include "Common.h"
#include "NetProtocol.h"
#include "FileManager.h"

// Generic
#define MAPOBJ_SCRIPT_NAME      (25)
#define MAPOBJ_CRITTER_PARAMS   (15)

// Map object types
#define MAP_OBJECT_CRITTER      (0)
#define MAP_OBJECT_ITEM         (1)
#define MAP_OBJECT_SCENERY      (2)

class ProtoMap;
class MapObject // Available in fonline.h
{
public:
	BYTE MapObjType;
	WORD ProtoId;
	WORD MapX;
	WORD MapY;
	short Dir;

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
			DWORD Anim1;
			DWORD Anim2;
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
			BYTE InfoOffset;
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

			int Val[10];
		} MItem;

		struct
		{
			short OffsetX;
			short OffsetY;
			BYTE AnimStayBegin;
			BYTE AnimStayEnd;
			WORD AnimWait;
			BYTE InfoOffset;
			DWORD PicMapHash;
			DWORD PicInvHash;

			bool CanUse;
			bool CanTalk;
			DWORD TriggerNum;

			BYTE ParamsCount;
			int Param[5];

			WORD ToMapPid;
			DWORD ToEntire;
			BYTE ToDir;

			BYTE SpriteCut;
		} MScenery;
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

struct SceneryCl
{
	WORD ProtoId;
	BYTE Flags;
	BYTE SpriteCut;
	WORD MapX;
	WORD MapY;
	short OffsetX;
	short OffsetY;
	DWORD LightColor;
	BYTE LightDistance;
	BYTE LightFlags;
	char LightIntensity;
	BYTE InfoOffset;
	BYTE AnimStayBegin;
	BYTE AnimStayEnd;
	WORD AnimWait;
	DWORD PicMapHash;
	short Dir;
	WORD Reserved1;
};
typedef vector<SceneryCl> SceneryClVec;

class ProtoMap
{
public:
	// Header
	struct
	{
		DWORD Version;
		WORD MaxHexX,MaxHexY;
		int WorkHexX,WorkHexY;
		char ScriptModule[MAX_SCRIPT_NAME+1];
		char ScriptFunc[MAX_SCRIPT_NAME+1];
		int Time;
		bool NoLogOut;
		int DayTime[4];
		BYTE DayColor[12];

		// Deprecated
		WORD HeaderSize;
		bool Packed;
		DWORD UnpackedDataLen;
	} Header;

	// Objects
	MapObjectPtrVec MObjects;

	// Tiles
	struct Tile // 12 bytes
	{
		DWORD NameHash;
		WORD HexX,HexY;
		char OffsX,OffsY;
		BYTE Layer;
		bool IsRoof;
#ifdef FONLINE_MAPPER
		bool IsSelected;
#endif

		Tile(){}
		Tile(DWORD name, WORD hx, WORD hy, char ox, char oy, BYTE layer, bool is_roof):
			NameHash(name),HexX(hx),HexY(hy),OffsX(ox),OffsY(oy),Layer(layer),IsRoof(is_roof){}
	};
	typedef vector<Tile> TileVec;
	TileVec Tiles;
#ifdef FONLINE_MAPPER
	// For fast access
	typedef vector<TileVec> TileVecVec;
	TileVecVec TilesField;
	TileVecVec RoofsField;
	TileVec& GetTiles(WORD hx, WORD hy, bool is_roof){return is_roof?RoofsField[hy*Header.MaxHexX+hx]:TilesField[hy*Header.MaxHexX+hx];}
#endif

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
	SceneryClVec WallsToSend;
	SceneryClVec SceneriesToSend;
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
	bool Save(const char* f_name, int path_type);
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
	ProtoMap():pmapFm(NULL),isInit(false),pathType(0),HexFlags(NULL){MEMORY_PROCESS(MEMORY_PROTO_MAP,sizeof(ProtoMap));}
	ProtoMap(const ProtoMap& r){*this=r;MEMORY_PROCESS(MEMORY_PROTO_MAP,sizeof(ProtoMap));}
	~ProtoMap(){pmapFm=NULL;isInit=false;HexFlags=NULL;MEMORY_PROCESS(MEMORY_PROTO_MAP,-(int)sizeof(ProtoMap));}
#else
	ProtoMap():pmapFm(NULL),isInit(false),pathType(0),RefCounter(1){}
	~ProtoMap(){pmapFm=NULL;isInit=false;}
#endif
};
typedef vector<ProtoMap> ProtoMapVec;
typedef vector<ProtoMap>::iterator ProtoMapVecIt;
typedef vector<ProtoMap>::value_type ProtoMapVecVal;
typedef vector<ProtoMap*> ProtoMapPtrVec;
typedef vector<ProtoMap*>::iterator ProtoMapPtrVecIt;
typedef vector<ProtoMap*>::value_type ProtoMapPtrVecVal;

#endif // __PROTO_MAP__