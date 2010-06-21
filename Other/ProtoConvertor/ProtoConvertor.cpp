// ProtoConvertor.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <ObjMngr.h>
#include <Log.h>

CFileMngr fm;
CObjectMngr objm;
CObjectMngr objm2;

PObjVec f2items;
PObjVec f2scen;
PObjVec f2misc;
PObjVec f2wall;
PObjVec f2tile;
PObjVec foproto;

PObjVec newproto;
PObjVec newproto2;

int ConvertTrans()
{
/*	if(!objm.LoadProtosOldFormat(f2items,"items.lst",PT_PRO_ITEMS)) return 0;
	if(!objm.LoadProtosOldFormat(f2scen,"scenery.lst",PT_PRO_SCENERY)) return 0;
	if(!objm.LoadProtosOldFormat(f2misc,"misc.lst",PT_PRO_MISC)) return 0;
	if(!objm.LoadProtosOldFormat(f2wall,"walls.lst",PT_PRO_WALLS)) return 0;
	if(!objm.LoadProtosOldFormat(f2tile,"tiles.lst",PT_PRO_TILES)) return 0;
	if(!objm.LoadProtosOldFormat(foproto,"objects.lst",PT_PRO_ITEMS)) return 0;

	objm.ParseProtos(f2items);
	objm.ParseProtos(f2scen);
	objm.ParseProtos(f2misc);
	objm.ParseProtos(f2wall);
	objm.ParseProtos(f2tile);
	objm.ParseProtos(foproto);

	if(!objm2.Init(&fm))
	{
		WriteLog("Object Manager2 Init False\n");
		return 0;
	}

	if(!objm2.LoadProtosNewFormat(newproto,"proto.fopro",PT_PRO_FOOBJ)) return 0;
	objm2.ParseProtos(newproto);

	int succ=0;

	int i;

#define CONVERT_PROTO_TRANS(vec) \
	for(i=0;i<objm2.Proto##vec##.size();i++)\
	{\
		CProtoObject* pobj2=&objm2.Proto##vec##[i];\
		CProtoObject* pobj1=objm.GetProtoObject(pobj2->GetPid());\
		if(!pobj1) continue;\
		if(pobj1->TransType!=TRANS_RED) continue;\
		WriteLog("<%u>\n",pobj2->GetPid());\
		pobj2->TransType=TRANS_RED;\
		succ++;\
	}

#define CUR_CONVERT(vec) CONVERT_PROTO_TRANS(vec)

	PObjVec ProtoArmor;
	PObjVec ProtoDrug;
	PObjVec ProtoWeapon;
	PObjVec ProtoAmmo;
	PObjVec ProtoMisc;
	PObjVec ProtoMisc2;
	PObjVec ProtoKey;
	PObjVec ProtoContainer;
	PObjVec ProtoDoor;
	PObjVec ProtoGrid;
	PObjVec ProtoGeneric;
	PObjVec ProtoWall;
	PObjVec ProtoTile;

	CUR_CONVERT(Armor);
	CUR_CONVERT(Drug);
	CUR_CONVERT(Weapon);
	CUR_CONVERT(Ammo);
	CUR_CONVERT(Misc);
	CUR_CONVERT(Misc2);
	CUR_CONVERT(Key);
	CUR_CONVERT(Container);
	CUR_CONVERT(Door);
	CUR_CONVERT(Grid);
	CUR_CONVERT(Generic);
	CUR_CONVERT(Wall);
	CUR_CONVERT(Tile);

	objm2.SaveProtosNewFormat("proto2.fopro",PT_PRO_FOOBJ);

	WriteLog("Success<%u>\n",succ);
*/
	return 0;
}

int ConvertNoBlock()
{
	if(!objm2.Init(&fm))
	{
		WriteLog("Object Manager2 Init False\n");
		return 0;
	}

	if(!objm2.LoadProtosNewFormat(newproto,"proto.fopro",PT_PRO_FOOBJ)) return 0;
	objm2.ParseProtos(newproto);

	/*for(int i=0;i<MAX_PROTOTYPES;i++)
	{
		CProtoObject* pobj=objm2.GetProtoObject(i);
		if(!pobj) continue;
		if(!pobj->IsItem()) continue;
		pobj->NoBlock=TRUE;
		WriteLog("%u\n",i);
	}*/

	objm2.SaveProtosNewFormat("proto2.fopro",PT_PRO_FOOBJ);
	return 0;
}

int main(int argc, char* argv[])
{
	LogToFile("Convertor.log");
	WriteLog("Start Convert\n\n");

	if(!fm.Init(".\\"))
	{
		WriteLog("File Manager Init False\n");
		return 0;
	}

	if(!objm.Init(&fm)) return 0;
	if(!objm.LoadProtosNewFormat(newproto,"proto.fopro",PT_PRO_FOOBJ)) return 0;
	objm.ParseProtos(newproto);
	if(!objm2.Init(&fm)) return 0;
	if(!objm2.LoadProtosNewFormat(newproto2,"tiles.fopro",PT_PRO_FOOBJ)) return 0;
	objm2.ParseProtos(newproto2);

	objm.ClearProtos(OBJ_TYPE_TILE);
	objm.ParseProtos(objm2.GetProtos(OBJ_TYPE_TILE));
	objm.SaveProtosNewFormat("proto2.fopro",PT_PRO_FOOBJ);
	return 0;
	

	return ConvertNoBlock();

	if(!objm.Init(&fm))
	{
		WriteLog("Object Manager Init False\n");
		return 0;
	}
	
	return ConvertTrans();

	MsgF2Item.LoadMsgFile(string(fm.GetDataPath())+string(fm.GetPath(PT_TXT_GAME))+string("PROITEMS.MSG"),false);
	MsgF2Scenery.LoadMsgFile(string(fm.GetDataPath())+string(fm.GetPath(PT_TXT_GAME))+string("PROSCENERY.MSG"),false);
	MsgF2Misc.LoadMsgFile(string(fm.GetDataPath())+string(fm.GetPath(PT_TXT_GAME))+string("PROMISC.MSG"),false);
	MsgF2Wall.LoadMsgFile(string(fm.GetDataPath())+string(fm.GetPath(PT_TXT_GAME))+string("PROWALL.MSG"),false);
	MsgF2Tile.LoadMsgFile(string(fm.GetDataPath())+string(fm.GetPath(PT_TXT_GAME))+string("PROTILE.MSG"),false);

	if(!objm.LoadProtosOldFormat(f2items,"items.lst",PT_PRO_ITEMS)) return 0;
	if(!objm.LoadProtosOldFormat(f2scen,"scenery.lst",PT_PRO_SCENERY)) return 0;
	if(!objm.LoadProtosOldFormat(f2misc,"misc.lst",PT_PRO_MISC)) return 0;
	if(!objm.LoadProtosOldFormat(f2wall,"walls.lst",PT_PRO_WALLS)) return 0;
	if(!objm.LoadProtosOldFormat(f2tile,"tiles.lst",PT_PRO_TILES)) return 0;
	if(!objm.LoadProtosOldFormat(foproto,"objects.lst",PT_PRO_ITEMS)) return 0;

	objm.ParseProtos(f2items);
	objm.ParseProtos(f2scen);
	objm.ParseProtos(f2misc);
	objm.ParseProtos(f2wall);
	objm.ParseProtos(f2tile);
	objm.ParseProtos(foproto);

	objm.PrintProtosHash();

	objm.SaveProtosNewFormat(POBJ_FILENAME,PT_PRO_FOOBJ);

	MsgFOObj.SaveMsgFile(string(fm.GetDataPath())+string(fm.GetPath(PT_TXT_GAME))+string("FOOBJ.MSG"),false);

	objm.ClearProtos();

	LogFinish();
	
	WriteLog("\nEnd Convert - Exit\n");

	return 0;
}

