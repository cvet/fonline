#include "StdAfx.h"
#include "HexManager.h"
#include "ResourceManager.h"
#include "3dStuff\Terrain.h"
#include "LineTracer.h"

#ifdef FONLINE_MAPPER
#include "CritterData.h"
#include "CritterManager.h"
#endif

#if defined(FONLINE_CLIENT) || defined(FONLINE_MAPPER)
#include "Script.h"
#endif

/************************************************************************/
/* FIELD                                                                */
/************************************************************************/

void Field::Clear()
{
	Crit=NULL;
	DeadCrits.clear();
	ScrX=0;
	ScrY=0;
	Tiles.clear();
	Roofs.clear();
	RoofNum=0;
	Items.clear();
	ScrollBlock=false;
	IsWall=false;
	IsWallSAI=false;
	IsWallTransp=false;
	IsScen=false;
	IsExitGrid=false;
	Corner=0;
	IsNotPassed=false;
	IsNotRaked=false;
	IsNoLight=false;
	IsMultihex=false;
}

void Field::AddItem(ItemHex* item)
{
	Items.push_back(item);
	ProcessCache();
}

void Field::EraseItem(ItemHex* item)
{
	ItemHexVecIt it=std::find(Items.begin(),Items.end(),item);
	if(it!=Items.end())
	{
		Items.erase(it);
		ProcessCache();
	}
}

void Field::ProcessCache()
{
	IsWall=false;
	IsWallSAI=false;
	IsWallTransp=false;
	IsScen=false;
	IsExitGrid=false;
	Corner=0;
	IsNotPassed=(Crit!=NULL || IsMultihex);
	IsNotRaked=false;
	IsNoLight=false;
	ScrollBlock=false;
	for(ItemHexVecIt it=Items.begin(),end=Items.end();it!=end;++it)
	{
		ItemHex* item=*it;
		WORD pid=item->GetProtoId();
		if(item->IsWall())
		{
			IsWall=true;
			IsWallTransp=item->IsLightThru();
			Corner=item->Proto->Corner;
			if(pid==SP_SCEN_IBLOCK) IsWallSAI=true;
		}
		else if(item->IsScenOrGrid())
		{
			IsScen=true;
			if(pid==SP_WALL_BLOCK || pid==SP_WALL_BLOCK_LIGHT) IsWall=true;
			else if(pid==SP_GRID_EXITGRID) IsExitGrid=true;
		}
		if(!item->IsPassed()) IsNotPassed=true;
		if(!item->IsRaked()) IsNotRaked=true;
		if(pid==SP_MISC_SCRBLOCK) ScrollBlock=true;
		if(!item->IsLightThru()) IsNoLight=true;
	}
}

void Field::AddTile(AnyFrames* anim, short ox, short oy, BYTE layer, bool is_roof)
{
	if(is_roof) Roofs.push_back(Tile());
	else Tiles.push_back(Tile());
	Tile& tile=(is_roof?Roofs.back():Tiles.back());
	tile.Anim=anim;
	tile.OffsX=ox;
	tile.OffsY=oy;
	tile.Layer=layer;
}

/************************************************************************/
/* HEX FIELD                                                            */
/************************************************************************/

HexManager::HexManager()
{
	viewField=NULL;
	reprepareTiles=false;
	tileSurf=NULL;
	tileSurfWidth=tileSurfHeight=0;
	isShowHex=false;
	roofSkip=0;
	rainCapacity=0;
	chosenId=0;
	critterContourCrId=0;
	crittersContour=0;
	critterContour=0;
	maxHexX=0;
	maxHexY=0;
	hexField=NULL;
	hexToDraw=NULL;
	hexTrack=NULL;
	hexLight=NULL;
	hTop=0;
	hBottom=0;
	wLeft=0;
	wRight=0;
	isShowCursor=false;
	drawCursorX=0;
	cursorPrePic=NULL;
	cursorPostPic=NULL;
	cursorXPic=NULL;
	cursorX=0;
	cursorY=0;
	ZeroMemory((void*)&AutoScroll,sizeof(AutoScroll));
	requestRebuildLight=false;
	lightPointsCount=0;
	SpritesCanDrawMap=false;
	dayTime[0]=300; dayTime[1]=600;  dayTime[2]=1140; dayTime[3]=1380;
	dayColor[0]=18; dayColor[1]=128; dayColor[2]=103; dayColor[3]=51;
	dayColor[4]=18; dayColor[5]=128; dayColor[6]=95;  dayColor[7]=40;
	dayColor[8]=53; dayColor[9]=128; dayColor[10]=86; dayColor[11]=29;
	picRainDrop=NULL;
	ZeroMemory(picRainDropA,sizeof(picRainDropA));
	picTrack1=picTrack2=picHexMask=NULL;
}

bool HexManager::Init()
{
	WriteLog("Hex field initialization...\n");

	if(!ItemMngr.IsInit())
	{
		WriteLog("Item manager is not init.\n");
		return false;
	}

	isShowTrack=false;
	curPidMap=0;
	curMapTime=-1;
	curHashTiles=0;
	curHashWalls=0;
	curHashScen=0;
	isShowCursor=false;
	cursorX=0;
	cursorY=0;
	chosenId=0;
	ZeroMemory((void*)&AutoScroll,sizeof(AutoScroll));
	maxHexX=0;
	maxHexY=0;

#ifdef FONLINE_MAPPER
	ClearSelTiles();
	CurProtoMap=NULL;
#endif

	WriteLog("Hex field initialization complete.\n");
	return true;
}

void HexManager::Clear()
{
	WriteLog("Hex field finish...\n");

	mainTree.Clear();
	roofRainTree.Clear();
	roofTree.Clear();

	for(DropVecIt it=rainData.begin(),end=rainData.end();it!=end;++it)
		SAFEDEL(*it);
	rainData.clear();

	for(int i=0,j=hexItems.size();i<j;i++)
		hexItems[i]->Release();
	hexItems.clear();

	for(int hx=0;hx<maxHexX;hx++)
		for(int hy=0;hy<maxHexY;hy++)
			GetField(hx,hy).Clear();

	SAFEREL(tileSurf);
	SAFEDELA(viewField);
	ResizeField(0,0);

	chosenId=0;
	WriteLog("Hex field finish complete.\n");
}

void HexManager::ReloadSprites()
{
	curDataPrefix=GameOpt.MapDataPrefix;

	// Must be valid
	picHex[0]=SprMngr.LoadAnimation((GameOpt.MapDataPrefix+"hex1.png").c_str(),PT_DATA,ANIM_USE_DUMMY);
	picHex[1]=SprMngr.LoadAnimation((GameOpt.MapDataPrefix+"hex2.png").c_str(),PT_DATA,ANIM_USE_DUMMY);
	picHex[2]=SprMngr.LoadAnimation((GameOpt.MapDataPrefix+"hex3.png").c_str(),PT_DATA,ANIM_USE_DUMMY);
	cursorPrePic=SprMngr.LoadAnimation((GameOpt.MapDataPrefix+"move_pre.png").c_str(),PT_DATA,ANIM_USE_DUMMY);
	cursorPostPic=SprMngr.LoadAnimation((GameOpt.MapDataPrefix+"move_post.png").c_str(),PT_DATA,ANIM_USE_DUMMY);
	cursorXPic=SprMngr.LoadAnimation((GameOpt.MapDataPrefix+"move_x.png").c_str(),PT_DATA,ANIM_USE_DUMMY);
	picTrack1=SprMngr.LoadAnimation((GameOpt.MapDataPrefix+"track1.png").c_str(),PT_DATA,ANIM_USE_DUMMY);
	picTrack2=SprMngr.LoadAnimation((GameOpt.MapDataPrefix+"track2.png").c_str(),PT_DATA,ANIM_USE_DUMMY);

	// May be nullsdsd
	picHexMask=SprMngr.LoadAnimation((GameOpt.MapDataPrefix+"hex_mask.png").c_str(),PT_DATA);

	// Rain
	picRainDrop=SprMngr.LoadAnimation("drop.png",PT_ART_MISC,ANIM_USE_DUMMY);
	for(int i=0;i<=6;i++)
	{
		char name[64];
		sprintf(name,"adrop%d.png",i);
		picRainDropA[i]=SprMngr.LoadAnimation(name,PT_ART_MISC,ANIM_USE_DUMMY);
	}
}

void HexManager::PlaceCarBlocks(WORD hx, WORD hy, ProtoItem* proto_item)
{
	if(!proto_item) return;

	bool raked=FLAG(proto_item->Flags,ITEM_SHOOT_THRU);
	BYTE dir;
	int steps;
	for(int i=0;i<CAR_MAX_BLOCKS;i++)
	{
		dir=proto_item->MiscEx.Car.GetBlockDir(i);
		if(dir>=DIRS_COUNT) return;

		i++;
		steps=proto_item->MiscEx.Car.GetBlockDir(i);

		for(int j=0;j<steps;j++)
		{
			MoveHexByDir(hx,hy,dir,maxHexX,maxHexY);
			GetField(hx,hy).IsNotPassed=true;
			if(!raked) GetField(hx,hy).IsNotRaked=true;
		}
	}
}

void HexManager::ReplaceCarBlocks(WORD hx, WORD hy, ProtoItem* proto_item)
{
	if(!proto_item) return;

	bool raked=FLAG(proto_item->Flags,ITEM_SHOOT_THRU);
	BYTE dir;
	int steps;
	for(int i=0;i<CAR_MAX_BLOCKS;i++)
	{
		dir=proto_item->MiscEx.Car.GetBlockDir(i);
		if(dir>=DIRS_COUNT) return;

		i++;
		steps=proto_item->MiscEx.Car.GetBlockDir(i);

		for(int j=0;j<steps;j++)
		{
			MoveHexByDir(hx,hy,dir,maxHexX,maxHexY);
			GetField(hx,hy).ProcessCache();
		}
	}
}

bool HexManager::AddItem(DWORD id, WORD pid, WORD hx, WORD hy, bool is_added, Item::ItemData* data)
{
	if(!id)
	{
		WriteLog(__FUNCTION__" - Item id is zero.\n");
		return false;
	}

	if(!IsMapLoaded())
	{
		WriteLog(__FUNCTION__" - Map is not loaded.");
		return false;
	}

	if(hx>=maxHexX || hy>=maxHexY)
	{
		WriteLog(__FUNCTION__" - Position hx<%u> or hy<%u> error value.\n",hx,hy);
		return false;
	}

	ProtoItem* proto=ItemMngr.GetProtoItem(pid);
	if(!proto)
	{
		WriteLog(__FUNCTION__" - Proto not found<%u>.\n",pid);
		return false;
	}

	// Change
	ItemHex* item_old=GetItemById(id);
	if(item_old)
	{
		if(item_old->IsFinishing()) item_old->StopFinishing();
		if(item_old->GetProtoId()==pid && item_old->GetHexX()==hx && item_old->GetHexY()==hy)
		{
			ChangeItem(id,*data);
			return true;
		}
		else DeleteItem(item_old);
	}

	// Parse
	Field& f=GetField(hx,hy);
	ItemHex* item=new ItemHex(id,proto,data,hx,hy,proto->Dir,0,0,&f.ScrX,&f.ScrY,0);
	if(is_added) item->SetShowAnim();
	PushItem(item);

	// Check ViewField size
	if(ProcessHexBorders(item->Anim->GetSprId(0),0,0))
	{
		// Resize
		hVisible=VIEW_HEIGHT+hTop+hBottom;
		wVisible=VIEW_WIDTH+wLeft+wRight;
		delete[] viewField;
		viewField=new ViewField[hVisible*wVisible];
		RefreshMap();
	}
	else
	{
		// Draw
		if(GetHexToDraw(hx,hy) && !item->IsHidden() && !item->IsFullyTransparent())
		{
			Sprite& spr=mainTree.InsertSprite(DRAW_ORDER_ITEM_AUTO(item),hx,hy+item->Proto->DrawOrderOffsetHexY,item->SpriteCut,
				f.ScrX+HEX_OX,f.ScrY+HEX_OY,0,&item->SprId,&item->ScrX,&item->ScrY,&item->Alpha,&item->SprDrawValid);
			if(!item->IsNoLightInfluence() && !(item->IsFlat() && item->IsScenOrGrid())) spr.SetLight(hexLight,maxHexX,maxHexY);
			item->SetSprite(&spr);
		}

		if(item->IsLight() || !item->IsLightThru()) RebuildLight();
	}

	return true;
}

void HexManager::ChangeItem(DWORD id, const Item::ItemData& data)
{
	ItemHex* item=GetItemById(id);
	if(!item) return;

	ProtoItem* proto=item->Proto;
	Item::ItemData old_data=item->Data;
	item->Data=data;

	if(old_data.PicMapHash!=data.PicMapHash) item->RefreshAnim();
	if(proto->IsDoor() || proto->IsContainer())
	{
		WORD old_cond=old_data.Locker.Condition;
		WORD new_cond=data.Locker.Condition;
		if(!FLAG(old_cond,LOCKER_ISOPEN) && FLAG(new_cond,LOCKER_ISOPEN)) item->SetAnimFromStart();
		if(FLAG(old_cond,LOCKER_ISOPEN) && !FLAG(new_cond,LOCKER_ISOPEN)) item->SetAnimFromEnd();
	}

	item->RefreshAlpha();
	item->SetSprite(NULL); // Refresh
	CritterCl* chosen=GetChosen();
	if(item->IsLight() || FLAG(old_data.Flags,ITEM_LIGHT_THRU)!=FLAG(data.Flags,ITEM_LIGHT_THRU)) RebuildLight();
	GetField(item->GetHexX(),item->GetHexY()).ProcessCache();
}

void HexManager::FinishItem(DWORD id, bool is_deleted)
{
	if(!id)
	{
		WriteLog(__FUNCTION__" - Item zero id.\n");
		return;
	}

	ItemHex* item=GetItemById(id);
	if(!item)
	{
		WriteLog(__FUNCTION__" - Item<%d> not found.\n",id);
		return;
	}

	item->Finish();
	if(is_deleted) item->SetHideAnim();
}

ItemHexVecIt HexManager::DeleteItem(ItemHex* item, bool with_delete /* = true */)
{
	WORD pid=item->GetProtoId();
	WORD hx=item->GetHexX();
	WORD hy=item->GetHexY();

	if(item->Proto->IsCar()) ReplaceCarBlocks(item->HexX,item->HexY,item->Proto);
	if(item->SprDrawValid) item->SprDraw->Unvalidate();

	ItemHexVecIt it=std::find(hexItems.begin(),hexItems.end(),item);
	if(it!=hexItems.end()) it=hexItems.erase(it);
	GetField(hx,hy).EraseItem(item);

	if(item->IsLight() || !item->IsLightThru()) RebuildLight();

	if(with_delete) item->Release();
	return it;
}

void HexManager::ProcessItems()
{
	for(ItemHexVecIt it=hexItems.begin();it!=hexItems.end();)
	{
		ItemHex* item=*it;
		item->Process();

		if(item->IsDynamicEffect() && !item->IsFinishing())
		{
			WordPair step=item->GetEffectStep();
			if(item->GetHexX()!=step.first || item->GetHexY()!=step.second)
			{
				WORD hx=item->GetHexX();
				WORD hy=item->GetHexY();
				int x,y;
				GetHexInterval(hx,hy,step.first,step.second,x,y);
				item->EffOffsX-=x;
				item->EffOffsY-=y;
				Field& f=GetField(hx,hy);
				Field& f_=GetField(step.first,step.second);
				f.EraseItem(item);
				f_.AddItem(item);
				item->HexX=step.first;
				item->HexY=step.second;
				if(item->SprDrawValid) item->SprDraw->Unvalidate();

				item->HexScrX=&f_.ScrX;
				item->HexScrY=&f_.ScrY;
				if(GetHexToDraw(step.first,step.second))
				{
					item->SprDraw=&mainTree.InsertSprite(DRAW_ORDER_ITEM_AUTO(item),step.first,step.second+item->Proto->DrawOrderOffsetHexY,item->SpriteCut,
						f_.ScrX+HEX_OX,f_.ScrY+HEX_OY,0,&item->SprId,&item->ScrX,&item->ScrY,&item->Alpha,&item->SprDrawValid);
					if(!item->IsNoLightInfluence() && !(item->IsFlat() && item->IsScenOrGrid())) item->SprDraw->SetLight(hexLight,maxHexX,maxHexY);
				}
				item->SetAnimOffs();
			}
		}

		if(item->IsFinish()) it=DeleteItem(item);
		else ++it;
	}
}

bool ItemCompScen(ItemHex* o1, ItemHex* o2){return o1->IsScenOrGrid() && !o2->IsScenOrGrid();}
bool ItemCompWall(ItemHex* o1, ItemHex* o2){return o1->IsWall() && !o2->IsWall();}
//bool ItemCompSort(ItemHex* o1, ItemHex* o2){return o1->IsWall() && !o2->IsWall();}
void HexManager::PushItem(ItemHex* item)
{
	hexItems.push_back(item);

	WORD hx=item->GetHexX();
	WORD hy=item->GetHexY();

	Field& f=GetField(hx,hy);
	item->HexScrX=&f.ScrX;
	item->HexScrY=&f.ScrY;
	f.AddItem(item);

	if(item->Proto->IsCar()) PlaceCarBlocks(hx,hy,item->Proto);

	// Sort
	std::sort(f.Items.begin(),f.Items.end(),ItemCompScen);
	std::sort(f.Items.begin(),f.Items.end(),ItemCompWall);
}

ItemHex* HexManager::GetItem(WORD hx, WORD hy, WORD pid)
{
	if(!IsMapLoaded() || hx>=maxHexX || hy>=maxHexY) return NULL;

	for(ItemHexVecIt it=GetField(hx,hy).Items.begin(),end=GetField(hx,hy).Items.end();it!=end;++it)
	{
		ItemHex* item=*it;
		if(item->GetProtoId()==pid) return item;
	}
	return NULL;
}

ItemHex* HexManager::GetItemById(WORD hx, WORD hy, DWORD id)
{
	if(!IsMapLoaded() || hx>=maxHexX || hy>=maxHexY) return NULL;

	for(ItemHexVecIt it=GetField(hx,hy).Items.begin(),end=GetField(hx,hy).Items.end();it!=end;++it)
	{
		ItemHex* item=*it;
		if(item->GetId()==id) return item;
	}
	return NULL;
}

ItemHex* HexManager::GetItemById(DWORD id)
{
	for(ItemHexVecIt it=hexItems.begin(),end=hexItems.end();it!=end;++it)
	{
		ItemHex* item=*it;
		if(item->GetId()==id) return item;
	}
	return NULL;
}

void HexManager::GetItems(WORD hx, WORD hy, ItemHexVec& items)
{
	if(!IsMapLoaded()) return;

	Field& f=GetField(hx,hy);
	for(int i=0,j=f.Items.size();i<j;i++)
	{
		if(std::find(items.begin(),items.end(),f.Items[i])==items.end()) items.push_back(f.Items[i]);
	}
}

INTRECT HexManager::GetRectForText(WORD hx, WORD hy)
{
	if(!IsMapLoaded()) return INTRECT();
	Field& f=GetField(hx,hy);

	// Critters first
	if(f.Crit) return f.Crit->GetTextRect();
	else if(f.DeadCrits.size()) return f.DeadCrits[0]->GetTextRect();

	// Items
	INTRECT r(0,0,0,0);
	for(int i=0,j=f.Items.size();i<j;i++)
	{
		SpriteInfo* si=SprMngr.GetSpriteInfo(f.Items[i]->SprId);
		if(si)
		{
			int w=si->Width-si->OffsX;
			int h=si->Height-si->OffsY;
			if(w>r.L) r.L=w;
			if(h>r.B) r.B=h;
		}
	}
	return r;
}

bool HexManager::RunEffect(WORD eff_pid, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy)
{
	if(!IsMapLoaded()) return false;
	if(from_hx>=maxHexX || from_hy>=maxHexY || to_hx>=maxHexX || to_hy>=maxHexY)
	{
		WriteLog(__FUNCTION__" - Incorrect value of from_x<%d> or from_y<%d> or to_x<%d> or to_y<%d>.\n",from_hx,from_hy,to_hx,to_hy);
		return false;
	}

	ProtoItem* proto=ItemMngr.GetProtoItem(eff_pid);
	if(!proto)
	{
		WriteLog(__FUNCTION__" - Proto not found, pid<%d>.\n",eff_pid);
		return false;
	}

	Field& f=GetField(from_hx,from_hy);
	ItemHex* item=new ItemHex(0,proto,NULL,from_hx,from_hy,GetFarDir(from_hx,from_hy,to_hx,to_hy),0,0,&f.ScrX,&f.ScrY,0);
	item->SetAnimFromStart();

	float sx=0;
	float sy=0;
	DWORD dist=0;

	if(from_hx!=to_hx || from_hy!=to_hy)
	{
		item->EffSteps.push_back(WordPairVecVal(from_hx,from_hy));
		TraceBullet(from_hx,from_hy,to_hx,to_hy,0,0.0f,NULL,false,NULL,0,NULL,NULL,&item->EffSteps,false);
		int x,y;
		GetHexInterval(from_hx,from_hy,to_hx,to_hy,x,y);
		y+=Random(5,25); // Center of body
		GetStepsXY(sx,sy,0,0,x,y);
		dist=DistSqrt(0,0,x,y);
	}

	item->SetEffect(sx,sy,dist);

	f.AddItem(item);
	hexItems.push_back(item);

	if(GetHexToDraw(from_hx,from_hy))
	{
		item->SprDraw=&mainTree.InsertSprite(DRAW_ORDER_ITEM_AUTO(item),from_hx,from_hy+item->Proto->DrawOrderOffsetHexY,item->SpriteCut,
			f.ScrX+HEX_OX,f.ScrY+HEX_OY,0,&item->SprId,&item->ScrX,&item->ScrY,&item->Alpha,&item->SprDrawValid);
		if(!item->IsNoLightInfluence() && !(item->IsFlat() && item->IsScenOrGrid())) item->SprDraw->SetLight(hexLight,maxHexX,maxHexY);
	}

	return true;
}

void HexManager::ProcessRain()
{
	if(!rainCapacity) return;

	static DWORD last_tick=Timer::GameTick();
	DWORD delta=Timer::GameTick()-last_tick;
	if(delta<=RAIN_TICK) return;

	for(DropVecIt it=rainData.begin(),end=rainData.end();it!=end;++it)
	{
		Drop* cur_drop=*it;

		if(cur_drop->DropCnt==-1)
		{
			if((cur_drop->OffsY+=RAIN_SPEED)>=cur_drop->GroundOffsY)
			{
				cur_drop->CurSprId=picRainDropA[cur_drop->DropCnt]->GetCurSprId();
				cur_drop->DropCnt=0;
			}
		}
		else
		{
			cur_drop->CurSprId=picRainDropA[cur_drop->DropCnt++]->GetCurSprId();

			if(cur_drop->DropCnt>6)
			{
				cur_drop->CurSprId=picRainDrop->GetCurSprId();
				cur_drop->DropCnt=-1;
				cur_drop->OffsX=Random(-10,10);
				cur_drop->OffsY=-100-Random(0,100);
			}
		}
	}
	
	last_tick=Timer::GameTick();
}

void HexManager::SetCursorPos(int x, int y, bool show_steps, bool refresh)
{
	WORD hx,hy;
	if(GetHexPixel(x,y,hx,hy))
	{
		Field& f=GetField(hx,hy);
		cursorX=f.ScrX+1-1;
		cursorY=f.ScrY-1-1;

		CritterCl* chosen=GetChosen();
		if(!chosen)
		{
			drawCursorX=-1;
			return;
		}

		int cx=chosen->GetHexX();
		int cy=chosen->GetHexY();
		DWORD mh=chosen->GetMultihex();

		if((cx==hx && cy==hy) || (f.IsNotPassed && (!mh || !CheckDist(cx,cy,hx,hy,mh))))
		{
			drawCursorX=-1;
		}
		else
		{
			static int last_cur_x=0;
			static WORD last_hx=0,last_hy=0;
			if(refresh || hx!=last_hx || hy!=last_hy)
			{
				bool is_tb=chosen->IsTurnBased();
				if(chosen->IsLife() && (!is_tb || chosen->GetAllAp()>0))
				{
					ByteVec steps;
					if(!FindPath(chosen,cx,cy,hx,hy,steps,-1)) drawCursorX=-1;
					else if(!is_tb) drawCursorX=(show_steps?steps.size():0);
					else
					{
						drawCursorX=steps.size();
						if(!show_steps && drawCursorX>chosen->GetAllAp()) drawCursorX=-1;
					} 
				}
				else
				{
					drawCursorX=-1;
				}

				last_hx=hx;
				last_hy=hy;
				last_cur_x=drawCursorX;
			}
			else
			{
				drawCursorX=last_cur_x;
			}
		}
	}
}

void HexManager::DrawCursor(DWORD spr_id)
{
	if(GameOpt.HideCursor || !isShowCursor) return;
	SpriteInfo* si=SprMngr.GetSpriteInfo(spr_id);
	if(!si) return;
	SprMngr.DrawSpriteSize(spr_id,(cursorX+GameOpt.ScrOx)/GameOpt.SpritesZoom,(cursorY+GameOpt.ScrOy)/GameOpt.SpritesZoom,si->Width/GameOpt.SpritesZoom,si->Height/GameOpt.SpritesZoom,true,false);
}

void HexManager::DrawCursor(const char* text)
{
	if(GameOpt.HideCursor || !isShowCursor) return;
	int x=(cursorX+GameOpt.ScrOx)/GameOpt.SpritesZoom;
	int y=(cursorY+GameOpt.ScrOy)/GameOpt.SpritesZoom;
	SprMngr.DrawStr(INTRECT(x,y,x+HEX_W/GameOpt.SpritesZoom,y+HEX_REAL_H/GameOpt.SpritesZoom),text,FT_CENTERX|FT_CENTERY,COLOR_TEXT_WHITE);
}

void HexManager::RebuildMap(int rx, int ry)
{
	if(!viewField) return;
	if(rx<0 || ry<0 || rx>=maxHexX || ry>=maxHexY) return;

	InitView(rx,ry);

	// Set to draw hexes
	ClearHexToDraw();

	int ty;
	int y2=0;
	int vpos;
	for(ty=0;ty<hVisible;ty++)
	{
		for(int tx=0;tx<wVisible;tx++)
		{
			vpos=y2+tx;

			int hx=viewField[vpos].HexX;
			int hy=viewField[vpos].HexY;

			if(hx<0 || hy<0 || hx>=maxHexX || hy>=maxHexY) continue;

			GetHexToDraw(hx,hy)=true;
			Field& f=GetField(hx,hy);
			f.ScrX=viewField[vpos].ScrX;
			f.ScrY=viewField[vpos].ScrY;
		}
		y2+=wVisible;
	}

	// Light
	RealRebuildLight();
	requestRebuildLight=false;

	// Tiles, roof
	RebuildTiles();
	RebuildRoof();

	// Erase old sprites
	mainTree.Unvalidate();
	roofRainTree.Unvalidate();

	for(DropVecIt it=rainData.begin(),end=rainData.end();it!=end;++it)
		delete (*it);
	rainData.clear();

	SprMngr.EggNotValid();

	// Begin generate new sprites
	y2=0;
	for(ty=0;ty<hVisible;ty++)
	{
		for(int x=0;x<wVisible;x++)
		{
			vpos=y2+x;
			int ny=viewField[vpos].HexY;
			int nx=viewField[vpos].HexX;
			if(ny<0 || nx<0 || nx>=maxHexX || ny>=maxHexY) continue;

			Field& f=GetField(nx,ny);

			// Track
			if(isShowTrack && GetHexTrack(nx,ny))
			{
				DWORD spr_id=(GetHexTrack(nx,ny)==1?picTrack1->GetCurSprId():picTrack2->GetCurSprId());
				SpriteInfo* si=SprMngr.GetSpriteInfo(spr_id);
				mainTree.AddSprite(DRAW_ORDER_TRACK,nx,ny,0,f.ScrX+HEX_OX,f.ScrY+HEX_OY+(si?si->Height/2:0),spr_id,NULL,NULL,NULL,NULL,NULL);
			}

			// Hex Lines
			if(isShowHex)
			{
				int lt_pos=hTop*wVisible+wRight+VIEW_WIDTH;
				int lb_pos=(hTop+VIEW_HEIGHT-1)*wVisible+wRight+VIEW_WIDTH;
				int rb_pos=(hTop+VIEW_HEIGHT-1)*wVisible+wRight+1;
				int rt_pos=hTop*wVisible+wRight+1;

				int lt_pos2=(hTop+1)*wVisible+wRight+VIEW_WIDTH;
				int lb_pos2=(hTop+VIEW_HEIGHT-1-1)*wVisible+wRight+VIEW_WIDTH;
				int rb_pos2=(hTop+VIEW_HEIGHT-1-1)*wVisible+wRight+1;
				int rt_pos2=(hTop+1)*wVisible+wRight+1;
				bool thru=(vpos==lt_pos || vpos==lb_pos || vpos==rb_pos || vpos==rt_pos ||
					vpos==lt_pos2 || vpos==lb_pos2 || vpos==rb_pos2 || vpos==rt_pos2);

				DWORD spr_id=(thru?picHex[1]->GetCurSprId():picHex[0]->GetCurSprId());
				SpriteInfo* si=SprMngr.GetSpriteInfo(spr_id);
				mainTree.AddSprite(DRAW_ORDER_HEX_GRID,nx,ny,0,f.ScrX+(si?si->Width/2:0),f.ScrY+(si?si->Height:0),spr_id,NULL,NULL,NULL,NULL,NULL);
			}

			// Rain
			if(rainCapacity)
			{
				if(rainCapacity>=Random(0,255) && x>=wRight-1 && x<=(wVisible-wLeft+1) && ty>=hTop-2 && ty<=hVisible)
				{
					int rofx=nx;
					int rofy=ny;
					if(rofx&1) rofx--;
					if(rofy&1) rofy--;

					if(GetField(rofx,rofy).Roofs.empty())
					{
						Drop* new_drop=new Drop(picRainDrop->GetCurSprId(),Random(-10,10),-Random(0,200),0);
						rainData.push_back(new_drop);

						mainTree.AddSprite(DRAW_ORDER_RAIN,nx,ny,0,f.ScrX+HEX_OX,f.ScrY+HEX_OY,0,&new_drop->CurSprId,
							&new_drop->OffsX,&new_drop->OffsY,NULL,NULL).SetLight(hexLight,maxHexX,maxHexY);
					}
					else if(!roofSkip || roofSkip!=GetField(rofx,rofy).RoofNum)
					{
						Drop* new_drop=new Drop(picRainDrop->GetCurSprId(),Random(-10,10),-100-Random(0,100),-100);
						rainData.push_back(new_drop);

						roofRainTree.AddSprite(DRAW_ORDER_RAIN,nx,ny,0,f.ScrX+HEX_OX,f.ScrY+HEX_OY,0,&new_drop->CurSprId,
							&new_drop->OffsX,&new_drop->OffsY,NULL,NULL).SetLight(hexLight,maxHexX,maxHexY);;
					}
				}
			}

			// Items on hex
			if(!f.Items.empty())
			{
				for(ItemHexVecIt it=f.Items.begin(),end=f.Items.end();it!=end;++it)
				{
					ItemHex* item=*it;

#ifdef FONLINE_CLIENT
					if(item->IsHidden() || item->IsFullyTransparent()) continue;
					if(item->IsScenOrGrid() && !GameOpt.ShowScen) continue;
					if(item->IsItem() && !GameOpt.ShowItem) continue;
					if(item->IsWall() && !GameOpt.ShowWall) continue;
#else
					bool is_fast=fastPids.count(item->GetProtoId())!=0;
					if(item->IsScenOrGrid() && !GameOpt.ShowScen && !is_fast) continue;
					if(item->IsItem() && !GameOpt.ShowItem && !is_fast) continue;
					if(item->IsWall() && !GameOpt.ShowWall && !is_fast) continue;
					if(!GameOpt.ShowFast && is_fast) continue;
					if(ignorePids.count(item->GetProtoId())) continue;
#endif

					Sprite& spr=mainTree.AddSprite(DRAW_ORDER_ITEM_AUTO(item),nx,ny+item->Proto->DrawOrderOffsetHexY,item->SpriteCut,
						f.ScrX+HEX_OX,f.ScrY+HEX_OY,0,&item->SprId,&item->ScrX,&item->ScrY,&item->Alpha,&item->SprDrawValid);
					if(!item->IsNoLightInfluence() && !(item->IsFlat() && item->IsScenOrGrid())) spr.SetLight(hexLight,maxHexX,maxHexY);
					item->SetSprite(&spr);
				}
			}

			// Critters
			CritterCl* cr=f.Crit;
			if(cr && GameOpt.ShowCrit && cr->Visible)
			{
				Sprite& spr=mainTree.AddSprite(DRAW_ORDER_CRIT_AUTO(cr),nx,ny,0,
					f.ScrX+HEX_OX,f.ScrY+HEX_OY,0,&cr->SprId,&cr->SprOx,&cr->SprOy,
					&cr->Alpha,&cr->SprDrawValid);
				spr.SetLight(hexLight,maxHexX,maxHexY);
				cr->SprDraw=&spr;

				cr->SetSprRect();

				int contour=0;
				if(cr->GetId()==critterContourCrId) contour=critterContour;
				else if(!cr->IsChosen()) contour=crittersContour;
				spr.SetContour(contour,cr->ContourColor);
			}

			// Dead critters
			if(!f.DeadCrits.empty() && GameOpt.ShowCrit)
			{
				for(CritVecIt it=f.DeadCrits.begin(),end=f.DeadCrits.end();it!=end;++it)
				{
					CritterCl* cr=*it;
					if(!cr->Visible) continue;

					Sprite& spr=mainTree.AddSprite(DRAW_ORDER_CRIT_AUTO(cr),nx,ny,0,
						f.ScrX+HEX_OX,f.ScrY+HEX_OY,0,&cr->SprId,&cr->SprOx,&cr->SprOy,
						&cr->Alpha,&cr->SprDrawValid);
					spr.SetLight(hexLight,maxHexX,maxHexY);
					cr->SprDraw=&spr;

					cr->SetSprRect();
				}
			}
		}
		y2+=wVisible;
	}
	mainTree.SortBySurfaces();
	mainTree.SortByMapPos();

#ifdef FONLINE_MAPPER
	if(MapperFunctions.RenderMap && Script::PrepareContext(MapperFunctions.RenderMap,CALL_FUNC_STR,"Game"))
	{
		SpritesCanDrawMap=true;
		Script::RunPrepared();
		SpritesCanDrawMap=false;
	}
#endif
#ifdef FONLINE_CLIENT
	if(Script::PrepareContext(ClientFunctions.RenderMap,CALL_FUNC_STR,"Game"))
	{
		SpritesCanDrawMap=true;
		Script::RunPrepared();
		SpritesCanDrawMap=false;
	}
#endif

	screenHexX=rx;
	screenHexY=ry;

#ifdef FONLINE_MAPPER
	if(CurProtoMap)
	{
		CurProtoMap->Header.WorkHexX=rx;
		CurProtoMap->Header.WorkHexY=ry;
	}
#endif
}

/************************************************************************/
/* Light                                                                */
/************************************************************************/

#define MAX_LIGHT_VALUE      (10000)
#define MAX_LIGHT_HEX        (200)
#define MAX_LIGHT_ALPHA      (100)
#define LIGHT_SOFT_LENGTH    (HEX_W)
int LightCapacity=0;
int LightMinHx=0;
int LightMaxHx=0;
int LightMinHy=0;
int LightMaxHy=0;
int LightProcentR=0;
int LightProcentG=0;
int LightProcentB=0;

void HexManager::MarkLight(WORD hx, WORD hy, DWORD inten)
{
	int light=inten*MAX_LIGHT_HEX/MAX_LIGHT_VALUE*LightCapacity/100;
	int lr=light*LightProcentR/100;
	int lg=light*LightProcentG/100;
	int lb=light*LightProcentB/100;
	BYTE* p=GetLightHex(hx,hy);
	if(lr>*p) *p=lr;
	if(lg>*(p+1)) *(p+1)=lg;
	if(lb>*(p+2)) *(p+2)=lb;
}

void HexManager::MarkLightEndNeighbor(WORD hx, WORD hy, bool north_south, DWORD inten)
{
	Field& f=GetField(hx,hy);
	if(f.IsWall)
	{
		int lt=f.Corner;
		if((north_south && (lt==CORNER_NORTH_SOUTH || lt==CORNER_NORTH || lt==CORNER_WEST)) ||
			(!north_south && (lt==CORNER_EAST_WEST || lt==CORNER_EAST)) ||
			lt==CORNER_SOUTH)
		{
			BYTE* p=GetLightHex(hx,hy);
			int light_full=inten*MAX_LIGHT_HEX/MAX_LIGHT_VALUE*LightCapacity/100;
			int light_self=(inten/2)*MAX_LIGHT_HEX/MAX_LIGHT_VALUE*LightCapacity/100;
			int lr_full=light_full*LightProcentR/100;
			int lg_full=light_full*LightProcentG/100;
			int lb_full=light_full*LightProcentB/100;
			int lr_self=int(*p)+light_self*LightProcentR/100;
			int lg_self=int(*(p+1))+light_self*LightProcentG/100;
			int lb_self=int(*(p+2))+light_self*LightProcentB/100;
			if(lr_self>lr_full) lr_self=lr_full;
			if(lg_self>lg_full) lg_self=lg_full;
			if(lb_self>lb_full) lb_self=lb_full;
			if(lr_self>*p) *p=lr_self;
			if(lg_self>*(p+1)) *(p+1)=lg_self;
			if(lb_self>*(p+2)) *(p+2)=lb_self;
		}
	}
}

void HexManager::MarkLightEnd(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, DWORD inten)
{
	bool is_wall=false;
	bool north_south=false;
	Field& f=GetField(to_hx,to_hy);
	if(f.IsWall)
	{
		is_wall=true;
		if(f.Corner==CORNER_NORTH_SOUTH || f.Corner==CORNER_NORTH || f.Corner==CORNER_WEST) north_south=true;
	}

	int dir=GetFarDir(from_hx,from_hy,to_hx,to_hy);
	if(dir==0 || (north_south && dir==1) || (!north_south && (dir==4 || dir==5)))
	{
		MarkLight(to_hx,to_hy,inten);
		if(is_wall)
		{
			if(north_south)
			{
				if(to_hy>0) MarkLightEndNeighbor(to_hx,to_hy-1,true,inten);
				if(to_hy<maxHexY-1) MarkLightEndNeighbor(to_hx,to_hy+1,true,inten);
			}
			else
			{
				if(to_hx>0)
				{
					MarkLightEndNeighbor(to_hx-1,to_hy,false,inten);
					if(to_hy>0) MarkLightEndNeighbor(to_hx-1,to_hy-1,false,inten);
					if(to_hy<maxHexY-1) MarkLightEndNeighbor(to_hx-1,to_hy+1,false,inten);
				}
				if(to_hx<maxHexX-1)
				{
					MarkLightEndNeighbor(to_hx+1,to_hy,false,inten);
					if(to_hy>0) MarkLightEndNeighbor(to_hx+1,to_hy-1,false,inten);
					if(to_hy<maxHexY-1) MarkLightEndNeighbor(to_hx+1,to_hy+1,false,inten);
				}
			}
		}
	}
}

void HexManager::MarkLightStep(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, DWORD inten)
{
	Field& f=GetField(to_hx,to_hy);
	if(f.IsWallTransp)
	{
		bool north_south=(f.Corner==CORNER_NORTH_SOUTH || f.Corner==CORNER_NORTH || f.Corner==CORNER_WEST);
		int dir=GetFarDir(from_hx,from_hy,to_hx,to_hy);
		if(dir==0 || (north_south && dir==1) || (!north_south && (dir==4 || dir==5))) MarkLight(to_hx,to_hy,inten);
	}
	else
	{
		MarkLight(to_hx,to_hy,inten);
	}
}

void HexManager::TraceLight(WORD from_hx, WORD from_hy, WORD& hx, WORD& hy, int dist, DWORD inten)
{
	float base_sx,base_sy;
	GetStepsXY(base_sx,base_sy,from_hx,from_hy,hx,hy);
	float sx1f=base_sx;
	float sy1f=base_sy;	
	float curx1f=(float)from_hx;
	float cury1f=(float)from_hy;
	int curx1i=from_hx;
	int cury1i=from_hy;
	int old_curx1i=curx1i;
	int old_cury1i=cury1i;
	bool right_barr=false;
	bool left_barr=false;
	DWORD inten_sub=inten/dist;

	for(;;)
	{
		inten-=inten_sub;
		curx1f+=sx1f;
		cury1f+=sy1f;
		old_curx1i=curx1i;
		old_cury1i=cury1i;

		// Casts
		curx1i=(int)curx1f;
		if(curx1f-(float)curx1i>=0.5f) curx1i++;
		cury1i=(int)cury1f;
		if(cury1f-(float)cury1i>=0.5f) cury1i++;
		bool can_mark=(curx1i>=LightMinHx && curx1i<=LightMaxHx && cury1i>=LightMinHy && cury1i<=LightMaxHy);

		// Left&Right trace
		int ox=0;
		int oy=0;

		if(old_curx1i&1)
		{
			if(old_curx1i+1==curx1i && old_cury1i+1==cury1i) {ox= 1; oy= 1;}
			if(old_curx1i-1==curx1i && old_cury1i+1==cury1i) {ox=-1; oy= 1;}
		}
		else
		{
			if(old_curx1i-1==curx1i && old_cury1i-1==cury1i) {ox=-1; oy=-1;}
			if(old_curx1i+1==curx1i && old_cury1i-1==cury1i) {ox= 1; oy=-1;}
		}

		if(ox)
		{
			// Left side
			ox=old_curx1i+ox;
			if(ox<0 || ox>=maxHexX || GetField(ox,old_cury1i).IsNoLight)
			{
				hx=(ox<0 || ox>=maxHexX?old_curx1i:ox);
				hy=old_cury1i;
				if(can_mark) MarkLightEnd(old_curx1i,old_cury1i,hx,hy,inten);
				break;
			}
			if(can_mark) MarkLightStep(old_curx1i,old_cury1i,ox,old_cury1i,inten);

			// Right side
			oy=old_cury1i+oy;
			if(oy<0 || oy>=maxHexY || GetField(old_curx1i,oy).IsNoLight)
			{
				hx=old_curx1i;
				hy=(oy<0 || oy>=maxHexY?old_cury1i:oy);
				if(can_mark) MarkLightEnd(old_curx1i,old_cury1i,hx,hy,inten);
				break;
			}
			if(can_mark) MarkLightStep(old_curx1i,old_cury1i,old_curx1i,oy,inten);
		}

		// Main trace
		if(curx1i<0 || curx1i>=maxHexX || cury1i<0 || cury1i>=maxHexY || GetField(curx1i,cury1i).IsNoLight)
		{
			hx=(curx1i<0 || curx1i>=maxHexX?old_curx1i:curx1i);
			hy=(cury1i<0 || cury1i>=maxHexY?old_cury1i:cury1i);
			if(can_mark) MarkLightEnd(old_curx1i,old_cury1i,hx,hy,inten);
			break;
		}
		if(can_mark) MarkLightEnd(old_curx1i,old_cury1i,curx1i,cury1i,inten);
		if(curx1i==hx && cury1i==hy) break;
	}
}

void HexManager::ParseLightTriangleFan(LightSource& ls)
{
	WORD hx=ls.HexX;
	WORD hy=ls.HexY;
	// Distance
	int dist=ls.Distance;
	if(dist<1) dist=1;
	// Intensity
	int inten=abs(ls.Intensity);
	if(inten>100) inten=50;
	inten*=100;
	if(FLAG(ls.Flags,LIGHT_GLOBAL)) GetColorDay(GetMapDayTime(),GetMapDayColor(),GetDayTime(),&LightCapacity);
	else if(ls.Intensity>=0) GetColorDay(GetMapDayTime(),GetMapDayColor(),GetMapTime(),&LightCapacity);
	else LightCapacity=100;
	if(FLAG(ls.Flags,LIGHT_INVERSE)) LightCapacity=100-LightCapacity;
	// Color
	DWORD color=ls.ColorRGB;
	if(color==0) color=0xFFFFFF;
	int alpha=MAX_LIGHT_ALPHA*LightCapacity/100*inten/MAX_LIGHT_VALUE;
	color=D3DCOLOR_ARGB(alpha,(color>>16)&0xFF,(color>>8)&0xFF,color&0xFF);
	LightProcentR=((color>>16)&0xFF)*100/0xFF;
	LightProcentG=((color>>8)&0xFF)*100/0xFF;
	LightProcentB=(color&0xFF)*100/0xFF;

	// Begin
	MarkLight(hx,hy,inten);
	int base_x,base_y;
	GetHexCurrentPosition(hx,hy,base_x,base_y);
	base_x+=HEX_OX;
	base_y+=HEX_OY;

	lightPointsCount++;
	if(lightPoints.size()<lightPointsCount) lightPoints.push_back(PointVec());
	PointVec& points=lightPoints[lightPointsCount-1];
	points.clear();
	points.reserve(3+dist*DIRS_COUNT);
	points.push_back(PrepPoint(base_x,base_y,color,(short*)&GameOpt.ScrOx,(short*)&GameOpt.ScrOy)); // Center of light
	color=D3DCOLOR_ARGB(0,(color>>16)&0xFF,(color>>8)&0xFF,color&0xFF);

	int hx_far=hx,hy_far=hy;
	bool seek_start=true;
	WORD last_hx=-1,last_hy=-1;

	for(int i=0,ii=(GameOpt.MapHexagonal?6:4);i<ii;i++)
	{
		int dir=(GameOpt.MapHexagonal?(i+2)%6:((i+1)*2)%8);

		for(int j=0,jj=(GameOpt.MapHexagonal?dist:dist*2);j<jj;j++)
		{
			if(seek_start)
			{
				// Move to start position
				for(int l=0;l<dist;l++) MoveHexByDirUnsafe(hx_far,hy_far,GameOpt.MapHexagonal?0:7);
				seek_start=false;
				j=-1;
			}
			else
			{
				// Move to next hex
				MoveHexByDirUnsafe(hx_far,hy_far,dir);
			}

			WORD hx_=CLAMP(hx_far,0,maxHexX-1);
			WORD hy_=CLAMP(hy_far,0,maxHexY-1);
			if(j>=0 && FLAG(ls.Flags,LIGHT_DISABLE_DIR(i)))
			{
				hx_=hx;
				hy_=hy;
			}
			else
			{
				TraceLight(hx,hy,hx_,hy_,dist,inten);
			}

			if(hx_!=last_hx || hy_!=last_hy)
			{
				if((int)hx_!=hx_far || (int)hy_!=hy_far)
				{
					int a=alpha-DistGame(hx,hy,hx_,hy_)*alpha/dist;
					a=CLAMP(a,0,alpha);
					color=D3DCOLOR_ARGB(a,(color>>16)&0xFF,(color>>8)&0xFF,color&0xFF);
				}
				else color=D3DCOLOR_ARGB(0,(color>>16)&0xFF,(color>>8)&0xFF,color&0xFF);
				int x,y;
				GetHexInterval(hx,hy,hx_,hy_,x,y);
				points.push_back(PrepPoint(base_x+x,base_y+y,color,(short*)&GameOpt.ScrOx,(short*)&GameOpt.ScrOy));
				last_hx=hx_;
				last_hy=hy_;
			}
		}
	}

	for(int i=1,j=points.size();i<j;i++)
	{
		PrepPoint& cur=points[i];
		PrepPoint& next=points[i>=points.size()-1?1:i+1];
		if(DistSqrt(cur.PointX,cur.PointY,next.PointX,next.PointY)>LIGHT_SOFT_LENGTH)
		{
			bool dist_comp=(DistSqrt(base_x,base_y,cur.PointX,cur.PointY)>DistSqrt(base_x,base_y,next.PointX,next.PointY));
			lightSoftPoints.push_back(PrepPoint(next.PointX,next.PointY,next.PointColor,(short*)&GameOpt.ScrOx,(short*)&GameOpt.ScrOy));
			lightSoftPoints.push_back(PrepPoint(cur.PointX,cur.PointY,cur.PointColor,(short*)&GameOpt.ScrOx,(short*)&GameOpt.ScrOy));
			float x=(dist_comp?next.PointX-cur.PointX:cur.PointX-next.PointX);
			float y=(dist_comp?next.PointY-cur.PointY:cur.PointY-next.PointY);
			ChangeStepsXY(x,y,dist_comp?-2.5f:2.5f);
			if(dist_comp) lightSoftPoints.push_back(PrepPoint(cur.PointX+int(x),cur.PointY+int(y),cur.PointColor,(short*)&GameOpt.ScrOx,(short*)&GameOpt.ScrOy));
			else lightSoftPoints.push_back(PrepPoint(next.PointX+int(x),next.PointY+int(y),next.PointColor,(short*)&GameOpt.ScrOx,(short*)&GameOpt.ScrOy));
		}
	}
}

void HexManager::RealRebuildLight()
{
	lightPointsCount=0;
	lightSoftPoints.clear();
	if(!viewField) return;

	ClearHexLight();
	CollectLightSources();

	LightMinHx=viewField[0].HexX;
	LightMaxHx=viewField[hVisible*wVisible-1].HexX;
	LightMinHy=viewField[wVisible-1].HexY;
	LightMaxHy=viewField[hVisible*wVisible-wVisible].HexY;

	for(LightSourceVecIt it=lightSources.begin(),end=lightSources.end();it!=end;++it)
	{
		LightSource& ls=(*it);
	//	if( (int)ls.HexX<LightMinHx-(int)ls.Distance || (int)ls.HexX>LightMaxHx+(int)ls.Distance ||
	//		(int)ls.HexY<LightMinHy-(int)ls.Distance || (int)ls.HexY>LightMaxHy+(int)ls.Distance) continue;
		ParseLightTriangleFan(ls);
	}
}

void HexManager::CollectLightSources()
{
	lightSources.clear();

#ifdef FONLINE_MAPPER
	if(!CurProtoMap) return;

	for(MapObjectPtrVecIt it=CurProtoMap->MObjects.begin(),end=CurProtoMap->MObjects.end();it!=end;++it)
	{
		MapObject* o=*it;
		if(o->MapObjType==MAP_OBJECT_CRITTER || !o->LightIntensity) continue;

		bool allow_light=false;
		if(o->MapObjType==MAP_OBJECT_ITEM && o->MItem.InContainer && o->MItem.ItemSlot!=SLOT_INV)
		{
			for(MapObjectPtrVecIt it_=CurProtoMap->MObjects.begin(),end_=CurProtoMap->MObjects.end();it_!=end_;++it_)
			{
				MapObject* oo=*it_;
				if(oo->MapObjType==MAP_OBJECT_CRITTER && oo->MapX==o->MapX && oo->MapY==o->MapY)
				{
					allow_light=true;
					break;
				}
			}
		}
		else if(o->LightIntensity && !(o->MapObjType==MAP_OBJECT_ITEM && o->MItem.InContainer))
		{
			allow_light=true;
		}

		if(allow_light)
			lightSources.push_back(LightSource(o->MapX,o->MapY,o->LightColor,o->LightDistance,o->LightIntensity,o->LightDirOff|((o->LightDay&3)<<6)));
	}
#else
	if(!IsMapLoaded()) return;

	// Scenery
	lightSources=lightSourcesScen;

	// Items on ground
	for(ItemHexVecIt it=hexItems.begin(),end=hexItems.end();it!=end;++it)
	{
		ItemHex* item=(*it);
		if(item->IsItem() && item->IsLight()) lightSources.push_back(LightSource(item->GetHexX(),item->GetHexY(),item->LightGetColor(),item->LightGetDistance(),item->LightGetIntensity(),item->LightGetFlags()));
	}

	// Items in critters slots
	for(CritMapIt it=allCritters.begin(),end=allCritters.end();it!=end;++it)
	{
		CritterCl* cr=(*it).second;
		bool added=false;
		for(ItemPtrVecIt it_=cr->InvItems.begin(),end_=cr->InvItems.end();it_!=end_;++it_)
		{
			Item* item=*it_;
			if(item->IsLight() && item->ACC_CRITTER.Slot!=SLOT_INV)
			{
				lightSources.push_back(LightSource(cr->GetHexX(),cr->GetHexY(),item->LightGetColor(),item->LightGetDistance(),item->LightGetIntensity(),item->LightGetFlags()));
				added=true;
			}
		}

		// Default chosen light
		if(cr->IsChosen() && !added)
			lightSources.push_back(LightSource(cr->GetHexX(),cr->GetHexY(),0,4,MAX_LIGHT_VALUE/4,0));
	}
#endif
}

/************************************************************************/
/* Tiles                                                                */
/************************************************************************/

bool HexManager::CheckTilesBorder(Field::Tile& tile, bool is_roof)
{
	int ox=(is_roof?ROOF_OX:TILE_OX)+tile.OffsX;
	int oy=(is_roof?ROOF_OY:TILE_OY)+tile.OffsY;
	return ProcessHexBorders(tile.Anim->GetSprId(0),ox,oy);
}

bool HexManager::AddTerrain(DWORD name_hash, int hx, int hy)
{
	Terrain* terrain=new(nothrow) Terrain(SprMngr.GetDevice(),hx,hy,Str::GetName(name_hash));
	if(terrain && !terrain->IsError())
	{
		tilesTerrain.push_back(terrain);
		return true;
	}
	delete terrain;
	return false;
}

bool HexManager::InitTilesSurf()
{
	int w=MODE_WIDTH+((SCROLL_OX*2)/MIN_ZOOM+1);
	int h=MODE_HEIGHT+((SCROLL_OY*2)/MIN_ZOOM+1);

	if(tileSurf && tileSurfWidth==w && tileSurfHeight==h) return true;

	SAFEREL(tileSurf);
	tileSurfWidth=0;
	tileSurfHeight=0;

	if(!SprMngr.CreateRenderTarget(tileSurf,w,h))
	{
		WriteLog("Can't create tiles surface, width<%d>, height<%d>.\n",w,h);
		return false;
	}

	tileSurfWidth=w;
	tileSurfHeight=h;
	return true;
}

void HexManager::RebuildTiles()
{
	if(!GameOpt.ShowTile) return;

	tilesTree.Unvalidate();

	int vpos;
	int y2=0;
	for(int ty=0;ty<hVisible;ty++)
	{
		for(int x=0;x<wVisible;x++)
		{
			vpos=y2+x;
			if(vpos<0 || vpos>=wVisible*hVisible) continue;
			int hx=viewField[vpos].HexX;
			int hy=viewField[vpos].HexY;

			if(hy<0 || hx<0 || hy>=maxHexY || hx>=maxHexX) continue;

			Field& f=GetField(hx,hy);
			if(f.Tiles.empty()) continue;

			for(size_t i=0,j=f.Tiles.size();i<j;i++)
			{
				Field::Tile& tile=f.Tiles[i];
				DWORD spr_id=tile.Anim->GetSprId(0);
				int ox=f.ScrX+tile.OffsX+TILE_OX;
				int oy=f.ScrY+tile.OffsY+TILE_OY;

				if(IsVisible(spr_id,ox,oy))
#ifdef FONLINE_MAPPER
				{
					ProtoMap::TileVec& tiles=CurProtoMap->GetTiles(hx,hy,false);
					tilesTree.AddSprite(DRAW_ORDER_TILE+tile.Layer,hx,hy,0,ox,oy,spr_id,NULL,NULL,NULL,tiles[i].IsSelected?(BYTE*)&SELECT_ALPHA:NULL,NULL);
				}
#else
					tilesTree.AddSprite(DRAW_ORDER_TILE+tile.Layer,hx,hy,0,ox,oy,spr_id,NULL,NULL,NULL,NULL,NULL);
#endif
			}
		}
		y2+=wVisible;
	}

	// Sort
	tilesTree.SortBySurfaces();
	tilesTree.SortByMapPos();
	reprepareTiles=true;
}

void HexManager::RebuildRoof()
{
	if(!GameOpt.ShowRoof) return;

	roofTree.Unvalidate();

	int vpos;
	int y2=0;
	for(int ty=0;ty<hVisible;ty++)
	{
		for(int x=0;x<wVisible;x++)
		{
			vpos=y2+x;
			int hx=viewField[vpos].HexX;
			int hy=viewField[vpos].HexY;

			if(hy<0 || hx<0 || hy>=maxHexY || hx>=maxHexX) continue;

			Field& f=GetField(hx,hy);
			if(f.Roofs.empty()) continue;

			if(!roofSkip || roofSkip!=f.RoofNum)
			{
				for(size_t i=0,j=f.Roofs.size();i<j;i++)
				{
					Field::Tile& roof=f.Roofs[i];
					DWORD spr_id=roof.Anim->GetSprId(0);
					int ox=f.ScrX+roof.OffsX+ROOF_OX;
					int oy=f.ScrY+roof.OffsY+ROOF_OY;

					if(IsVisible(spr_id,ox,oy))
#ifdef FONLINE_MAPPER
					{
						ProtoMap::TileVec& roofs=CurProtoMap->GetTiles(hx,hy,true);
						roofTree.AddSprite(DRAW_ORDER_TILE+roof.Layer,hx,hy,0,ox,oy,spr_id,NULL,NULL,NULL,roofs[i].IsSelected?(BYTE*)&SELECT_ALPHA:&GameOpt.RoofAlpha,NULL).SetEgg(EGG_ALWAYS);
					}
#else
						roofTree.AddSprite(DRAW_ORDER_TILE+roof.Layer,hx,hy,0,ox,oy,spr_id,NULL,NULL,NULL,&GameOpt.RoofAlpha,NULL).SetEgg(EGG_ALWAYS);
#endif
				}
			}
		}
		y2+=wVisible;
	}

	// Sort
	roofTree.SortBySurfaces();
	roofTree.SortByMapPos();
}

void HexManager::SetSkipRoof(int hx, int hy)
{
	if(roofSkip!=GetField(hx,hy).RoofNum)
	{
		roofSkip=GetField(hx,hy).RoofNum;
		if(rainCapacity) RefreshMap();
		else RebuildRoof();
	}
}

void HexManager::MarkRoofNum(int hx, int hy, int num)
{
	if(hx<0 || hx>=maxHexX || hy<0 || hy>=maxHexY) return;
	if(GetField(hx,hy).Roofs.empty()) return;
	if(GetField(hx,hy).RoofNum) return;

	for(int x=0;x<GameOpt.MapRoofSkipSize;x++)
		for(int y=0;y<GameOpt.MapRoofSkipSize;y++)
			if(hx+x>=0 && hx+x<maxHexX && hy+y>=0 && hy+y<maxHexY)
				GetField(hx+x,hy+y).RoofNum=num;

	MarkRoofNum(hx+GameOpt.MapRoofSkipSize,hy,num);
	MarkRoofNum(hx-GameOpt.MapRoofSkipSize,hy,num);
	MarkRoofNum(hx,hy+GameOpt.MapRoofSkipSize,num);
	MarkRoofNum(hx,hy-GameOpt.MapRoofSkipSize,num);
}

bool HexManager::IsVisible(DWORD spr_id, int ox, int oy)
{
	if(!spr_id) return false;
	SpriteInfo* si=SprMngr.GetSpriteInfo(spr_id);
	if(!si) return false;
	if(si->Anim3d) return true;

	int top=oy+si->OffsY-si->Height-SCROLL_OY;
	int bottom=oy+si->OffsY+SCROLL_OY;
	int left=ox+si->OffsX-si->Width/2-SCROLL_OX;
	int right=ox+si->OffsX+si->Width/2+SCROLL_OX;
	return !(top>MODE_HEIGHT*GameOpt.SpritesZoom || bottom<0 || left>MODE_WIDTH*GameOpt.SpritesZoom || right<0);
}

bool HexManager::ProcessHexBorders(DWORD spr_id, int ox, int oy)
{
	SpriteInfo* si=SprMngr.GetSpriteInfo(spr_id);
	if(!si) return false;

	int top=si->OffsY+oy-hTop*HEX_LINE_H+SCROLL_OY;
	if(top<0) top=0;
	int bottom=si->Height-si->OffsY-oy-hBottom*HEX_LINE_H+SCROLL_OY;
	if(bottom<0) bottom=0;
	int left=si->Width/2+si->OffsX+ox-wLeft*HEX_W+SCROLL_OX;
	if(left<0) left=0;
	int right=si->Width/2-si->OffsX-ox-wRight*HEX_W+SCROLL_OX;
	if(right<0) right=0;

	if(top || bottom || left || right)
	{
		// Resize
		hTop+=top/HEX_LINE_H+((top%HEX_LINE_H)?1:0);
		hBottom+=bottom/HEX_LINE_H+((bottom%HEX_LINE_H)?1:0);
		wLeft+=left/HEX_W+((left%HEX_W)?1:0);
		wRight+=right/HEX_W+((right%HEX_W)?1:0);
		return true;
	}
	return false;
}

void HexManager::SwitchShowHex()
{
	isShowHex=!isShowHex;
	RefreshMap();
}

void HexManager::SwitchShowRain()
{
	rainCapacity=(rainCapacity?0:255);
	RefreshMap();
}

void HexManager::SetWeather(int time, BYTE rain)
{
	curMapTime=time;
	rainCapacity=rain;
}

bool HexManager::ResizeField(WORD w, WORD h)
{
	GameOpt.ClientMap=NULL;
	GameOpt.ClientMapWidth=0;
	GameOpt.ClientMapHeight=0;

	maxHexX=w;
	maxHexY=h;
	SAFEDELA(hexField);
	SAFEDELA(hexToDraw);
	SAFEDELA(hexTrack);
	SAFEDELA(hexLight);
	if(!w || !h) return true;

	hexField=new Field[w*h];
	if(!hexField) return false;
	hexToDraw=new bool[w*h];
	if(!hexToDraw) return false;
	ZeroMemory(hexToDraw,w*h*sizeof(bool));
	hexTrack=new char[w*h];
	if(!hexTrack) return false;
	ZeroMemory(hexTrack,w*h*sizeof(char));
	hexLight=new BYTE[w*h*3];
	if(!hexLight) return false;
	ZeroMemory(hexLight,w*h*3*sizeof(BYTE));

	GameOpt.ClientMap=hexField;
	GameOpt.ClientMapWidth=w;
	GameOpt.ClientMapHeight=h;
	return true;
}

void HexManager::SwitchShowTrack()
{
	isShowTrack=!isShowTrack;
	if(!isShowTrack) ClearHexTrack();
	RefreshMap();
}

void HexManager::InitView(int cx, int cy)
{
	if(GameOpt.MapHexagonal)
	{
		// Get center offset
		int hw=VIEW_WIDTH/2+wRight;
		int hv=VIEW_HEIGHT/2+hTop;
		int vw=hv/2+(hv&1)+1;
		int vh=hv-vw/2-1;
 		for(int i=0;i<hw;i++)
 		{
 			if(vw&1) vh--;
 			vw++;
 		}

		// Subtract offset
		cx-=abs(vw);
		cy-=abs(vh);

		int x;
		int xa=-(wRight*HEX_W);
		int xb=-(HEX_W/2)-(wRight*HEX_W);
		int y=-HEX_LINE_H*hTop;
		int y2=0;
		int vpos;
		int hx,hy;
		int wx=MODE_WIDTH*GameOpt.SpritesZoom;

		for(int j=0;j<hVisible;j++)
		{
			hx=cx+j/2+(j&1);
			hy=cy+(j-(hx-cx-(cx&1))/2);
			x=((j&1)?xa:xb);

			for(int i=0;i<wVisible;i++)
			{
				vpos=y2+i;
				viewField[vpos].ScrX=wx-x;
				viewField[vpos].ScrY=y;
				viewField[vpos].ScrXf=(float)viewField[vpos].ScrX;
				viewField[vpos].ScrYf=(float)viewField[vpos].ScrY;
				viewField[vpos].HexX=hx;
				viewField[vpos].HexY=hy;

				if(hx&1) hy--;
				hx++;
				x+=HEX_W;
			}
			y+=HEX_LINE_H;
			y2+=wVisible;
		}
	}
	else
	{
		// Calculate data
		int halfw=VIEW_WIDTH/2+wRight;
		int halfh=VIEW_HEIGHT/2+hTop;
		int basehx=cx-halfh/2-halfw;
		int basehy=cy-halfh/2+halfw;
		int y2=0;
		int vpos;
		int x;
		int xa=-HEX_W*wRight;
		int xb=-HEX_W*wRight-HEX_W/2;
		int y=-HEX_LINE_H*hTop;
		int wx=MODE_WIDTH*GameOpt.SpritesZoom;
		int hx,hy;

		// Initialize field
		for(int j=0;j<hVisible;j++)
		{
			x=((j&1)?xa:xb);
			hx=basehx;
			hy=basehy;

			for(int i=0;i<wVisible;i++)
			{
				vpos=y2+i;
				viewField[vpos].ScrX=wx-x;
				viewField[vpos].ScrY=y;
				viewField[vpos].ScrXf=(float)viewField[vpos].ScrX;
				viewField[vpos].ScrYf=(float)viewField[vpos].ScrY;
				viewField[vpos].HexX=hx;
				viewField[vpos].HexY=hy;

				hx++;
				hy--;
				x+=HEX_W;
			}

			if(j&1) basehy++;
			else basehx++;

			y+=HEX_LINE_H;
			y2+=wVisible;
		}
	}
}

void HexManager::ChangeZoom(int zoom)
{
	if(!IsMapLoaded()) return;
	if(GameOpt.SpritesZoomMin==GameOpt.SpritesZoomMax) return;
	if(!zoom && GameOpt.SpritesZoom==1.0f) return;
	if(zoom>0 && GameOpt.SpritesZoom>=min(GameOpt.SpritesZoomMax,MAX_ZOOM)) return;
	if(zoom<0 && GameOpt.SpritesZoom<=max(GameOpt.SpritesZoomMin,MIN_ZOOM)) return;

	// Check screen blockers
	if(GameOpt.ScrollCheck && (zoom>0 || (zoom==0 && GameOpt.SpritesZoom<1.0f)))
	{
		for(int x=-1;x<=1;x++)
		{
			for(int y=-1;y<=1;y++)
			{
				if((x || y) && ScrollCheck(x,y)) return;
			}
		}
	}

	if(zoom || GameOpt.SpritesZoom<1.0f)
	{
		float old_zoom=GameOpt.SpritesZoom;
		float w=MODE_WIDTH/HEX_W+((MODE_WIDTH%HEX_W)?1:0);
		GameOpt.SpritesZoom=(w*GameOpt.SpritesZoom+(zoom>=0?2.0f:-2.0f))/w;

		if(GameOpt.SpritesZoom<max(GameOpt.SpritesZoomMin,MIN_ZOOM) || GameOpt.SpritesZoom>min(GameOpt.SpritesZoomMax,MAX_ZOOM))
		{
			GameOpt.SpritesZoom=old_zoom;
			return;
		}
	}
	else
	{
		GameOpt.SpritesZoom=1.0f;
	}

	wVisible=VIEW_WIDTH+wLeft+wRight;
	hVisible=VIEW_HEIGHT+hTop+hBottom;
	delete[] viewField;
	viewField=new ViewField[hVisible*wVisible];
	RefreshMap();

	if(zoom==0 && GameOpt.SpritesZoom!=1.0f) ChangeZoom(0);
}

void HexManager::GetHexCurrentPosition(WORD hx, WORD hy, int& x, int& y)
{
	ViewField& center_hex=viewField[hVisible/2*wVisible+wVisible/2];
	int center_hx=center_hex.HexX;
	int center_hy=center_hex.HexY;

	GetHexInterval(center_hx,center_hy,hx,hy,x,y);
	x+=center_hex.ScrX;
	y+=center_hex.ScrY;

//	x+=center_hex.ScrX+GameOpt.ScrOx;
//	y+=center_hex.ScrY+GameOpt.ScrOy;
//	x/=GameOpt.SpritesZoom;
//	y/=GameOpt.SpritesZoom;
}

void HexManager::DrawMap()
{
	// Rebuild light
	if(requestRebuildLight)
	{
		RealRebuildLight();
		requestRebuildLight=false;
	}

	// Tiles
	if(GameOpt.ShowTile)
	{
		if(reprepareTiles)
		{
			// Clear
			if(GameOpt.ScreenClear) SprMngr.ClearRenderTarget(tileSurf,D3DCOLOR_XRGB(100,100,100));

			// Draw terrain
			for(TerrainVecIt it=tilesTerrain.begin(),end=tilesTerrain.end();it!=end;++it)
			{
				Terrain* terrain=*it;
				int x,y;
				GetHexCurrentPosition(terrain->GetHexX(),terrain->GetHexY(),x,y);
				terrain->Draw(tileSurf,NULL,x+33,y+20,GameOpt.SpritesZoom);
			}

			// Draw simple tiles
			SprMngr.SetCurEffect2D(DEFAULT_EFFECT_TILE);
			SprMngr.PrepareBuffer(tilesTree,tileSurf,SCROLL_OX,SCROLL_OY,TILE_ALPHA);

			// Done
			reprepareTiles=false;
		}

		SprMngr.DrawPrepared(tileSurf,SCROLL_OX,SCROLL_OY);
	}

	// Flat sprites
	SprMngr.SetCurEffect2D(DEFAULT_EFFECT_GENERIC);
	SprMngr.DrawSprites(mainTree,true,false,DRAW_ORDER_FLAT,DRAW_ORDER_LIGHT-1);

	// Light
	for(int i=0;i<lightPointsCount;i++) SprMngr.DrawPoints(lightPoints[i],D3DPT_TRIANGLEFAN,&GameOpt.SpritesZoom);
	SprMngr.DrawPoints(lightSoftPoints,D3DPT_TRIANGLELIST,&GameOpt.SpritesZoom);

	// Cursor flat
	DrawCursor(cursorPrePic->GetCurSprId());

	// Sprites
	SprMngr.SetCurEffect2D(DEFAULT_EFFECT_GENERIC);
	SprMngr.DrawSprites(mainTree,true,true,DRAW_ORDER_LIGHT,DRAW_ORDER_LAST);

	// Roof
	if(GameOpt.ShowRoof)
	{
		LPDIRECT3DDEVICE device=SprMngr.GetDevice();
		device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
		device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);
		SprMngr.SetCurEffect2D(DEFAULT_EFFECT_ROOF);
		SprMngr.DrawSprites(roofTree,false,true,0,0);
		device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
		device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);

		SprMngr.SetCurEffect2D(DEFAULT_EFFECT_GENERIC);
		if(rainCapacity) SprMngr.DrawSprites(roofRainTree,false,false,0,0);
	}

	// Contours
	SprMngr.DrawContours();

	// Cursor
	DrawCursor(cursorPostPic->GetCurSprId());
	if(drawCursorX<0) DrawCursor(cursorXPic->GetCurSprId());
	else if(drawCursorX>0) DrawCursor(Str::Format("%u",drawCursorX));

	SprMngr.Flush();
}

bool HexManager::Scroll()
{
	if(!IsMapLoaded()) return false;

	static DWORD last_call=Timer::AccurateTick();
	if(Timer::AccurateTick()-last_call<GameOpt.ScrollDelay) return false;
	else last_call=Timer::AccurateTick();

	bool is_scroll=(GameOpt.ScrollMouseLeft || GameOpt.ScrollKeybLeft || GameOpt.ScrollMouseRight || GameOpt.ScrollKeybRight || GameOpt.ScrollMouseUp || GameOpt.ScrollKeybUp || GameOpt.ScrollMouseDown || GameOpt.ScrollKeybDown);
	int scr_ox=GameOpt.ScrOx;
	int scr_oy=GameOpt.ScrOy;

	if(is_scroll && AutoScroll.CanStop) AutoScroll.Active=false;

	// Check critter scroll lock
	if(AutoScroll.LockedCritter && !is_scroll)
	{
		CritterCl* cr=GetCritter(AutoScroll.LockedCritter);
		if(cr && (cr->GetHexX()!=screenHexX || cr->GetHexY()!=screenHexY)) ScrollToHex(cr->GetHexX(),cr->GetHexY(),0.02,true);
		//if(cr && DistSqrt(cr->GetHexX(),cr->GetHexY(),screenHexX,screenHexY)>4) ScrollToHex(cr->GetHexX(),cr->GetHexY(),0.5,true);
	}

	if(AutoScroll.Active)
	{
		AutoScroll.OffsXStep+=AutoScroll.OffsX*AutoScroll.Speed;
		AutoScroll.OffsYStep+=AutoScroll.OffsY*AutoScroll.Speed;
		int xscroll=(int)AutoScroll.OffsXStep;
		int yscroll=(int)AutoScroll.OffsYStep;
		if(xscroll>SCROLL_OX)
		{
			xscroll=SCROLL_OX;
			AutoScroll.OffsXStep=(double)SCROLL_OX;
		}
		if(xscroll<-SCROLL_OX)
		{
			xscroll=-SCROLL_OX;
			AutoScroll.OffsXStep=-(double)SCROLL_OX;
		}
		if(yscroll>SCROLL_OY)
		{
			yscroll=SCROLL_OY;
			AutoScroll.OffsYStep=(double)SCROLL_OY;
		}
		if(yscroll<-SCROLL_OY)
		{
			yscroll=-SCROLL_OY;
			AutoScroll.OffsYStep=-(double)SCROLL_OY;
		}

		AutoScroll.OffsX-=xscroll;
		AutoScroll.OffsY-=yscroll;
		AutoScroll.OffsXStep-=xscroll;
		AutoScroll.OffsYStep-=yscroll;
		if(!xscroll && !yscroll) return false;
		if(!DistSqrt(0,0,AutoScroll.OffsX,AutoScroll.OffsY)) AutoScroll.Active=false;

		scr_ox+=xscroll;
		scr_oy+=yscroll;
	}
	else
	{
		if(!is_scroll) return false;

		int xscroll=0;
		int yscroll=0;

		if(GameOpt.ScrollMouseLeft || GameOpt.ScrollKeybLeft) xscroll+=1;
		if(GameOpt.ScrollMouseRight || GameOpt.ScrollKeybRight) xscroll-=1;
		if(GameOpt.ScrollMouseUp || GameOpt.ScrollKeybUp) yscroll+=1;
		if(GameOpt.ScrollMouseDown || GameOpt.ScrollKeybDown) yscroll-=1;
		if(!xscroll && !yscroll) return false;

		scr_ox+=xscroll*GameOpt.ScrollStep*GameOpt.SpritesZoom;
		scr_oy+=yscroll*(GameOpt.ScrollStep*SCROLL_OY/SCROLL_OX)*GameOpt.SpritesZoom;
	}

	if(GameOpt.ScrollCheck)
	{
		int xmod=0;
		int ymod=0;
		if(scr_ox-GameOpt.ScrOx>0) xmod=1;
		if(scr_ox-GameOpt.ScrOx<0) xmod=-1;
		if(scr_oy-GameOpt.ScrOy>0) ymod=-1;
		if(scr_oy-GameOpt.ScrOy<0) ymod=1;
		if((xmod || ymod) && ScrollCheck(xmod,ymod))
		{
			if(xmod && ymod && !ScrollCheck(0,ymod)) scr_ox=0;
			else if(xmod && ymod && !ScrollCheck(xmod,0)) scr_oy=0;
			else
			{
				if(xmod) scr_ox=0;
				if(ymod) scr_oy=0;
			}
		}
	}

	int xmod=0;
	int ymod=0;
	if(scr_ox>=SCROLL_OX)
	{
		xmod=1;
		scr_ox-=SCROLL_OX;
		if(scr_ox>SCROLL_OX) scr_ox=SCROLL_OX;
	}
	else if(scr_ox<=-SCROLL_OX)
	{
		xmod=-1;
		scr_ox+=SCROLL_OX;
		if(scr_ox<-SCROLL_OX) scr_ox=-SCROLL_OX;
	}
	if(scr_oy>=SCROLL_OY)
	{
		ymod=-2;
		scr_oy-=SCROLL_OY;
		if(scr_oy>SCROLL_OY) scr_oy=SCROLL_OY;
	}
	else if(scr_oy<=-SCROLL_OY)
	{
		ymod=2;
		scr_oy+=SCROLL_OY;
		if(scr_oy<-SCROLL_OY) scr_oy=-SCROLL_OY;
	}

	GameOpt.ScrOx=scr_ox;
	GameOpt.ScrOy=scr_oy;

	if(xmod || ymod)
	{
		int vpos1=5*wVisible+4;
		int vpos2=(5+ymod)*wVisible+4+xmod;
		int hx=screenHexX+(viewField[vpos2].HexX-viewField[vpos1].HexX);
		int hy=screenHexY+(viewField[vpos2].HexY-viewField[vpos1].HexY);
		RebuildMap(hx,hy);

		if(GameOpt.ScrollCheck)
		{
			int old_ox=GameOpt.ScrOx;
			int old_oy=GameOpt.ScrOy;
			if(GameOpt.ScrOx>0 && ScrollCheck(1,0)) GameOpt.ScrOx=0;
			else if(GameOpt.ScrOx<0 && ScrollCheck(-1,0)) GameOpt.ScrOx=0;
			if(GameOpt.ScrOy>0 && ScrollCheck(0,-1)) GameOpt.ScrOy=0;
			else if(GameOpt.ScrOy<0 && ScrollCheck(0,1)) GameOpt.ScrOy=0;
		}
	}
	else
	{
		return false;
	}
	
	return true;
}

bool HexManager::ScrollCheckPos(int (&positions)[4], int dir1, int dir2)
{
	int max_pos=wVisible*hVisible;
	for(int i=0;i<4;i++)
	{
		int pos=positions[i];
		if(pos<0 || pos>=max_pos) return true;

		WORD hx=viewField[pos].HexX;
		WORD hy=viewField[pos].HexY;
		if(hx>=maxHexX || hy>=maxHexY) return true;

		MoveHexByDir(hx,hy,dir1,maxHexX,maxHexY);
		if(GetField(hx,hy).ScrollBlock) return true;
		if(dir2>=0)
		{
			MoveHexByDir(hx,hy,dir2,maxHexX,maxHexY);
			if(GetField(hx,hy).ScrollBlock) return true;
		}
	}
	return false;
}

bool HexManager::ScrollCheck(int xmod, int ymod)
{
	int positions_left[4]={
		hTop*wVisible+wRight+VIEW_WIDTH, // Left top
		(hTop+VIEW_HEIGHT-1)*wVisible+wRight+VIEW_WIDTH, // Left bottom
		(hTop+1)*wVisible+wRight+VIEW_WIDTH, // Left top 2
		(hTop+VIEW_HEIGHT-1-1)*wVisible+wRight+VIEW_WIDTH, // Left bottom 2
	};
	int positions_right[4]={
		(hTop+VIEW_HEIGHT-1)*wVisible+wRight+1, // Right bottom
		hTop*wVisible+wRight+1, // Right top
		(hTop+VIEW_HEIGHT-1-1)*wVisible+wRight+1, // Right bottom 2
		(hTop+1)*wVisible+wRight+1, // Right top 2
	};

	int dirs[8]={0,5,2,3,4,-1,1,-1}; // Hexagonal
	if(!GameOpt.MapHexagonal) dirs[0]=7,dirs[1]=-1,dirs[2]=3,dirs[3]=-1,
		dirs[4]=5,dirs[5]=-1,dirs[6]=1,dirs[7]=-1; // Square

	if(GameOpt.MapHexagonal)
	{
		if(ymod<0 && (ScrollCheckPos(positions_left,0,5) || ScrollCheckPos(positions_right,5,0))) return true; // Up
		else if(ymod>0 && (ScrollCheckPos(positions_left,2,3) || ScrollCheckPos(positions_right,3,2))) return true; // Down
		if(xmod>0 && (ScrollCheckPos(positions_left,4,-1) || ScrollCheckPos(positions_right,4,-1))) return true; // Left
		else if(xmod<0 && (ScrollCheckPos(positions_right,1,-1) || ScrollCheckPos(positions_left,1,-1))) return true; // Right
	}
	else
	{
		if(ymod<0 && (ScrollCheckPos(positions_left,0,6) || ScrollCheckPos(positions_right,6,0))) return true; // Up
		else if(ymod>0 && (ScrollCheckPos(positions_left,2,4) || ScrollCheckPos(positions_right,4,2))) return true; // Down
		if(xmod>0 && (ScrollCheckPos(positions_left,6,4) || ScrollCheckPos(positions_right,4,6))) return true; // Left
		else if(xmod<0 && (ScrollCheckPos(positions_right,0,2) || ScrollCheckPos(positions_left,2,0))) return true; // Right
	}

	// Add precise for zooming infelicity
	if(GameOpt.SpritesZoom!=1.0f)
	{
		for(int i=0;i<4;i++) positions_left[i]--;
		for(int i=0;i<4;i++) positions_right[i]++;

		if(GameOpt.MapHexagonal)
		{
			if(ymod<0 && (ScrollCheckPos(positions_left,0,5) || ScrollCheckPos(positions_right,5,0))) return true; // Up
			else if(ymod>0 && (ScrollCheckPos(positions_left,2,3) || ScrollCheckPos(positions_right,3,2))) return true; // Down
			if(xmod>0 && (ScrollCheckPos(positions_left,4,-1) || ScrollCheckPos(positions_right,4,-1))) return true; // Left
			else if(xmod<0 && (ScrollCheckPos(positions_right,1,-1) || ScrollCheckPos(positions_left,1,-1))) return true; // Right
		}
		else
		{
			if(ymod<0 && (ScrollCheckPos(positions_left,0,6) || ScrollCheckPos(positions_right,6,0))) return true; // Up
			else if(ymod>0 && (ScrollCheckPos(positions_left,2,4) || ScrollCheckPos(positions_right,4,2))) return true; // Down
			if(xmod>0 && (ScrollCheckPos(positions_left,6,4) || ScrollCheckPos(positions_right,4,6))) return true; // Left
			else if(xmod<0 && (ScrollCheckPos(positions_right,0,2) || ScrollCheckPos(positions_left,2,0))) return true; // Right
		}
	}
	return false;
}

void HexManager::ScrollToHex(int hx, int hy, double speed, bool can_stop)
{
	if(!IsMapLoaded()) return;

	int sx,sy;
	GetScreenHexes(sx,sy);
	int x,y;
	GetHexInterval(sx,sy,hx,hy,x,y);
	AutoScroll.Active=true;
	AutoScroll.CanStop=can_stop;
	AutoScroll.OffsX=-x;
	AutoScroll.OffsY=-y;
	AutoScroll.OffsXStep=0.0;
	AutoScroll.OffsYStep=0.0;
	AutoScroll.Speed=speed;
}

void HexManager::PreRestore()
{
	SAFEREL(tileSurf);
	for(TerrainVecIt it=tilesTerrain.begin(),end=tilesTerrain.end();it!=end;++it) (*it)->PreRestore();
}

void HexManager::PostRestore()
{
	InitTilesSurf();
	for(TerrainVecIt it=tilesTerrain.begin(),end=tilesTerrain.end();it!=end;++it) (*it)->PostRestore();
	RefreshMap();
}

void HexManager::SetCrit(CritterCl* cr)
{
	if(!IsMapLoaded()) return;

	WORD hx=cr->GetHexX();
	WORD hy=cr->GetHexY();
	Field& f=GetField(hx,hy);

	if(cr->IsDead())
	{
		if(std::find(f.DeadCrits.begin(),f.DeadCrits.end(),cr)==f.DeadCrits.end()) f.DeadCrits.push_back(cr);
	}
	else
	{
		if(f.Crit && f.Crit!=cr)
		{
			WriteLog(__FUNCTION__" - Hex<%u><%u> busy, critter old<%u>, new<%u>.\n",hx,hy,f.Crit->GetId(),cr->GetId());
			return;
		}

		f.Crit=cr;
		SetMultihex(cr->GetHexX(),cr->GetHexY(),cr->GetMultihex(),true);
	}

	if(GetHexToDraw(hx,hy) && cr->Visible)
	{
		Sprite& spr=mainTree.InsertSprite(DRAW_ORDER_CRIT_AUTO(cr),hx,hy,0,
			f.ScrX+HEX_OX,f.ScrY+HEX_OY,0,&cr->SprId,&cr->SprOx,&cr->SprOy,&cr->Alpha,&cr->SprDrawValid);
		spr.SetLight(hexLight,maxHexX,maxHexY);
		cr->SprDraw=&spr;

		cr->SetSprRect();
		cr->FixLastHexes();

		int contour=0;
		if(cr->GetId()==critterContourCrId) contour=critterContour;
		else if(!cr->IsDead() && !cr->IsChosen()) contour=crittersContour;
		spr.SetContour(contour,cr->ContourColor);
	}

	f.ProcessCache();
}

void HexManager::RemoveCrit(CritterCl* cr)
{
	if(!IsMapLoaded()) return;

	WORD hx=cr->GetHexX();
	WORD hy=cr->GetHexY();

	Field& f=GetField(hx,hy);
	if(f.Crit==cr)
	{
		f.Crit=NULL;
		SetMultihex(cr->GetHexX(),cr->GetHexY(),cr->GetMultihex(),false);
	}
	else
	{
		CritVecIt it=std::find(f.DeadCrits.begin(),f.DeadCrits.end(),cr);
		if(it!=f.DeadCrits.end()) f.DeadCrits.erase(it);
	}

	if(cr->IsChosen() || cr->IsHaveLightSources()) RebuildLight();
	if(cr->SprDrawValid) cr->SprDraw->Unvalidate();
	f.ProcessCache();
}

void HexManager::AddCrit(CritterCl* cr)
{
	if(allCritters.count(cr->GetId())) return;
	allCritters.insert(CritMapVal(cr->GetId(),cr));
	if(cr->IsChosen()) chosenId=cr->GetId();
	SetCrit(cr);
}

void HexManager::EraseCrit(DWORD crid)
{
	CritMapIt it=allCritters.find(crid);
	if(it==allCritters.end()) return;
	CritterCl* cr=(*it).second;
	if(cr->IsChosen()) chosenId=0;
	RemoveCrit(cr);
	cr->EraseAllItems();
	cr->IsNotValid=true;
	cr->Release();
	allCritters.erase(it);
}

void HexManager::ClearCritters()
{
	for(CritMapIt it=allCritters.begin(),end=allCritters.end();it!=end;++it)
	{
		CritterCl* cr=(*it).second;
		RemoveCrit(cr);
		cr->EraseAllItems();
		cr->IsNotValid=true;
		cr->Release();
	}
	allCritters.clear();
	chosenId=0;
}

void HexManager::GetCritters(WORD hx, WORD hy, CritVec& crits, int find_type)
{
	Field* f=&GetField(hx,hy);
	if(f->Crit && f->Crit->CheckFind(find_type)) crits.push_back(f->Crit);
	for(CritVecIt it=f->DeadCrits.begin(),end=f->DeadCrits.end();it!=end;++it) if((*it)->CheckFind(find_type)) crits.push_back(*it);
}

void HexManager::SetCritterContour(DWORD crid, int contour)
{
	if(critterContourCrId)
	{
		CritterCl* cr=GetCritter(critterContourCrId);
		if(cr && cr->SprDrawValid)
		{
			if(!cr->IsDead() && !cr->IsChosen())
				cr->SprDraw->SetContour(crittersContour);
			else
				cr->SprDraw->SetContour(0);
		}
	}
	critterContourCrId=crid;
	critterContour=contour;
	if(crid)
	{
		CritterCl* cr=GetCritter(crid);
		if(cr && cr->SprDrawValid) cr->SprDraw->SetContour(contour);
	}
}

void HexManager::SetCrittersContour(int contour)
{
	if(crittersContour==contour) return;
	crittersContour=contour;
	for(CritMapIt it=allCritters.begin(),end=allCritters.end();it!=end;it++)
	{
		CritterCl* cr=(*it).second;
		if(!cr->IsChosen() && cr->SprDrawValid && !cr->IsDead() && cr->GetId()!=critterContourCrId) cr->SprDraw->SetContour(contour);
	}
}

bool HexManager::TransitCritter(CritterCl* cr, int hx, int hy, bool animate, bool force)
{
	if(!IsMapLoaded() || hx<0 || hx>=maxHexX || hy<0 || hy>=maxHexY) return false;
	if(cr->GetHexX()==hx && cr->GetHexY()==hy) return true;

	// Dead transit
	if(cr->IsDead())
	{
		RemoveCrit(cr);
		cr->HexX=hx;
		cr->HexY=hy;
		SetCrit(cr);

		if(cr->IsChosen() || cr->IsHaveLightSources()) RebuildLight();
		return true;
	}

	// Not dead transit
	Field& f=GetField(hx,hy);

	if(f.Crit!=NULL) // Hex busy
	{
		// Try move critter on busy hex in previous position
		if(force && f.Crit->IsLastHexes()) TransitCritter(f.Crit,f.Crit->PopLastHexX(),f.Crit->PopLastHexY(),false,true);
		if(f.Crit!=NULL)
		{
			// Try move in next game cycle
			return false;
		}
	}

	RemoveCrit(cr);

	int old_hx=cr->GetHexX();
	int old_hy=cr->GetHexY();
	cr->HexX=hx;
	cr->HexY=hy;
	int dir=GetFarDir(old_hx,old_hy,hx,hy);

	if(animate)
	{
		cr->Move(dir);
		if(DistGame(old_hx,old_hy,hx,hy)>1)
		{
			WORD hx_=hx;
			WORD hy_=hy;
			MoveHexByDir(hx_,hy_,ReverseDir(dir),maxHexX,maxHexY);
			int ox,oy;
			GetHexInterval(hx_,hy_,old_hx,old_hy,ox,oy);
			cr->AddOffsExt(ox,oy);
		}
	}
	else
	{
		int ox,oy;
		GetHexInterval(hx,hy,old_hx,old_hy,ox,oy);
		cr->AddOffsExt(ox,oy);
	}

	SetCrit(cr);
	return true;
}

void HexManager::SetMultihex(WORD hx, WORD hy, DWORD multihex, bool set)
{
	if(IsMapLoaded() && multihex)
	{
		short* sx,*sy;
		GetHexOffsets(hx&1,sx,sy);
		for(int i=0,j=NumericalNumber(multihex)*DIRS_COUNT;i<j;i++)
		{
			short cx=(short)hx+sx[i];
			short cy=(short)hy+sy[i];
			if(cx>=0 && cy>=0 && cx<maxHexX && cy<maxHexY)
			{
				Field& neighbor=GetField(cx,cy);
				neighbor.IsMultihex=set;
				neighbor.ProcessCache();
			}
		}
	}
}

bool HexManager::GetHexPixel(int x, int y, WORD& hx, WORD& hy)
{
	if(!IsMapLoaded()) return false;

	float xf=(float)x-(float)GameOpt.ScrOx/GameOpt.SpritesZoom;
	float yf=(float)y-(float)GameOpt.ScrOy/GameOpt.SpritesZoom;
	float ox=(float)HEX_W/GameOpt.SpritesZoom;
	float oy=(float)HEX_REAL_H/GameOpt.SpritesZoom;
	int y2=0;
	int vpos=0;

	for(int ty=0;ty<hVisible;ty++)
	{
		for(int tx=0;tx<wVisible;tx++)
		{
			vpos=y2+tx;

			float x_=viewField[vpos].ScrXf/GameOpt.SpritesZoom;
			float y_=viewField[vpos].ScrYf/GameOpt.SpritesZoom;

			if(xf>=x_ && xf<x_+ox && yf>=y_ && yf<y_+oy)
			{
				int hx_=viewField[vpos].HexX;
				int hy_=viewField[vpos].HexY;

				// Correct with hex color mask
				if(picHexMask)
				{
					DWORD r=(SprMngr.GetPixColor(picHexMask->Ind[0],xf-x_,yf-y_)&0x00FF0000)>>16;
					if(r==50) MoveHexByDirUnsafe(hx_,hy_,GameOpt.MapHexagonal?5:6);
					else if(r==100) MoveHexByDirUnsafe(hx_,hy_,GameOpt.MapHexagonal?0:0);
					else if(r==150) MoveHexByDirUnsafe(hx_,hy_,GameOpt.MapHexagonal?3:4);
					else if(r==200) MoveHexByDirUnsafe(hx_,hy_,GameOpt.MapHexagonal?2:2);
				}

				if(hx_>=0 && hy_>=0 && hx_<maxHexX && hy_<maxHexY)
				{
					hx=hx_;
					hy=hy_;
					return true;
				}
			}
		}
		y2+=wVisible;
	}

	return false;
}

ItemHex* HexManager::GetItemPixel(int x, int y, bool& item_egg)
{
	if(!IsMapLoaded()) return NULL;

	ItemHexVec pix_item;
	ItemHexVec pix_item_egg;
	bool is_egg=SprMngr.IsEggTransp(x,y);

	for(ItemHexVecIt it=hexItems.begin(),end=hexItems.end();it!=end;++it)
	{
		ItemHex* item=(*it);
		WORD hx=item->GetHexX();
		WORD hy=item->GetHexY();

		if(item->IsFinishing() || !item->SprDrawValid) continue;

#ifdef FONLINE_CLIENT
		if(item->IsHidden() || item->IsFullyTransparent()) continue;
		if(item->IsScenOrGrid() && !GameOpt.ShowScen) continue;
		if(item->IsItem() && !GameOpt.ShowItem) continue;
		if(item->IsWall() && !GameOpt.ShowWall) continue;
#else // FONLINE_MAPPER
		bool is_fast=fastPids.count(item->GetProtoId())!=0;
		if(item->IsScenOrGrid() && !GameOpt.ShowScen && !is_fast) continue;
		if(item->IsItem() && !GameOpt.ShowItem && !is_fast) continue;
		if(item->IsWall() && !GameOpt.ShowWall && !is_fast) continue;
		if(!GameOpt.ShowFast && is_fast) continue;
		if(ignorePids.count(item->GetProtoId())) continue;
#endif

		SpriteInfo* si=SprMngr.GetSpriteInfo(item->SprId);
		if(!si) continue;

		if(si->Anim3d)
		{
			if(si->Anim3d->IsIntersect(x,y)) pix_item.push_back(item);
			continue;
		}

		int l=(*item->HexScrX+item->ScrX+si->OffsX+HEX_OX+GameOpt.ScrOx-si->Width/2)/GameOpt.SpritesZoom;
		int r=(*item->HexScrX+item->ScrX+si->OffsX+HEX_OX+GameOpt.ScrOx+si->Width/2)/GameOpt.SpritesZoom;
		int t=(*item->HexScrY+item->ScrY+si->OffsY+HEX_OY+GameOpt.ScrOy-si->Height)/GameOpt.SpritesZoom;
		int b=(*item->HexScrY+item->ScrY+si->OffsY+HEX_OY+GameOpt.ScrOy)/GameOpt.SpritesZoom;

		if(x>=l && x<=r && y>=t && y<=b)
		{
			Sprite* spr=item->SprDraw->GetIntersected(x-l,y-t);
			if(spr)
			{
				item->SprTemp=spr;
				if(is_egg && SprMngr.CompareHexEgg(hx,hy,item->GetEggType()))
					pix_item_egg.push_back(item);
				else
					pix_item.push_back(item);
			}
		}
	}

	// Sorters
	struct Sorter
	{
		static bool ByTreeIndex(ItemHex* o1, ItemHex* o2){return o1->SprTemp->TreeIndex>o2->SprTemp->TreeIndex;}
		static bool ByTransparent(ItemHex* o1, ItemHex* o2){return !o1->IsTransparent() && o2->IsTransparent();}
	};

	// Egg items
	if(pix_item.empty())
	{
		if(pix_item_egg.empty()) return NULL;
		if(pix_item_egg.size()>1)
		{
			std::sort(pix_item_egg.begin(),pix_item_egg.end(),Sorter::ByTreeIndex);
			std::sort(pix_item_egg.begin(),pix_item_egg.end(),Sorter::ByTransparent);
		}
		item_egg=true;
		return pix_item_egg[0];
	}

	// Visible items
	if(pix_item.size()>1)
	{
		std::sort(pix_item.begin(),pix_item.end(),Sorter::ByTreeIndex);
		std::sort(pix_item.begin(),pix_item.end(),Sorter::ByTransparent);
	}
	item_egg=false;
	return pix_item[0];
}

CritterCl* HexManager::GetCritterPixel(int x, int y, bool ignore_dead_and_chosen)
{
	if(!IsMapLoaded() || !GameOpt.ShowCrit) return NULL;

	CritVec crits;
	for(CritMapIt it=allCritters.begin();it!=allCritters.end();it++)
	{
		CritterCl* cr=(*it).second;
		if(!cr->Visible || cr->IsFinishing() || !cr->SprDrawValid) continue;
		if(ignore_dead_and_chosen && (cr->IsDead() || cr->IsChosen())) continue;

		if(cr->Anim3d)
		{
			if(cr->Anim3d->IsIntersect(x,y)) crits.push_back(cr);
			continue;
		}

		if(x>=(cr->DRect.L+GameOpt.ScrOx)/GameOpt.SpritesZoom && x<=(cr->DRect.R+GameOpt.ScrOx)/GameOpt.SpritesZoom &&
			y>=(cr->DRect.T+GameOpt.ScrOy)/GameOpt.SpritesZoom && y<=(cr->DRect.B+GameOpt.ScrOy)/GameOpt.SpritesZoom &&
			SprMngr.IsPixNoTransp(cr->SprId,x-(cr->DRect.L+GameOpt.ScrOx)/GameOpt.SpritesZoom,y-(cr->DRect.T+GameOpt.ScrOy)/GameOpt.SpritesZoom))
		{
			crits.push_back(cr);
		}
	}

	if(crits.empty()) return NULL;
	struct Sorter{static bool ByTreeIndex(CritterCl* cr1, CritterCl* cr2){return cr1->SprDraw->TreeIndex>cr2->SprDraw->TreeIndex;}};
	if(crits.size()>1) std::sort(crits.begin(),crits.end(),Sorter::ByTreeIndex);
	return crits[0];
}

void HexManager::GetSmthPixel(int pix_x, int pix_y, ItemHex*& item, CritterCl*& cr)
{
	bool item_egg;
	item=GetItemPixel(pix_x,pix_y,item_egg);
	cr=GetCritterPixel(pix_x,pix_y,false);

	if(cr && item)
	{
		if(item->IsTransparent() || item_egg) item=NULL;
		else
		{
			if(item->SprDraw->TreeIndex>cr->SprDraw->TreeIndex) cr=NULL;
			else item=NULL;
		}
	}
}

bool HexManager::FindPath(CritterCl* cr, WORD start_x, WORD start_y, WORD& end_x, WORD& end_y, ByteVec& steps, int cut)
{
	// Static data
#define GRID(x,y) grid[((MAX_FIND_PATH+1)+(y)-grid_oy)*(MAX_FIND_PATH*2+2)+((MAX_FIND_PATH+1)+(x)-grid_ox)]
	static int grid_ox=0,grid_oy=0;
	static short* grid=NULL;
	static WordPairVec coords;

	// Allocate temporary grid
	if(!grid)
	{
		grid=new(nothrow) short[(MAX_FIND_PATH*2+2)*(MAX_FIND_PATH*2+2)];
		if(!grid) return false;
	}

	if(!IsMapLoaded()) return false;
	if(start_x==end_x && start_y==end_y) return true;

	short numindex=1;
	ZeroMemory(grid,(MAX_FIND_PATH*2+2)*(MAX_FIND_PATH*2+2)*sizeof(short));
	grid_ox=start_x;
	grid_oy=start_y;
	GRID(start_x,start_y)=numindex;
	coords.clear();
	coords.push_back(WordPairVecVal(start_x,start_y));

	DWORD mh=(cr?cr->GetMultihex():0);
	int p=0;
	while(true)
	{
		if(++numindex>MAX_FIND_PATH) return false;

		int p_togo=coords.size()-p;
		if(!p_togo) return false;

		for(int i=0;i<p_togo;++i,++p)
		{
			int hx=coords[p].first;
			int hy=coords[p].second;

			short* sx,*sy;
			GetHexOffsets(hx&1,sx,sy);

			for(int j=0,jj=DIRS_COUNT;j<jj;j++)
			{
				int nx=hx+sx[j];
				int ny=hy+sy[j];
				if(nx<0 || ny<0 || nx>=maxHexX || ny>=maxHexY || GRID(nx,ny)) continue;
				GRID(nx,ny)=-1;

				if(!mh)
				{
					if(GetField(nx,ny).IsNotPassed) continue;
				}
				else
				{
					// Base hex
					int nx_=nx,ny_=ny;
					for(DWORD k=0;k<mh;k++) MoveHexByDirUnsafe(nx_,ny_,j);
					if(nx_<0 || ny_<0 || nx_>=maxHexX || ny_>=maxHexY) continue;
					if(GetField(nx_,ny_).IsNotPassed) continue;

					// Clock wise hexes
					bool is_square_corner=(!GameOpt.MapHexagonal && IS_DIR_CORNER(j));
					DWORD steps_count=(is_square_corner?mh*2:mh);
					bool not_passed=false;
					int dir_=(GameOpt.MapHexagonal?((j+2)%6):((j+2)%8));
					if(is_square_corner) dir_=(dir_+1)%8;
					int nx__=nx_,ny__=ny_;
					for(DWORD k=0;k<steps_count && !not_passed;k++)
					{
						MoveHexByDirUnsafe(nx__,ny__,dir_);
						not_passed=GetField(nx__,ny__).IsNotPassed;
					}
					if(not_passed) continue;

					// Counter clock wise hexes
					dir_=(GameOpt.MapHexagonal?((j+4)%6):((j+6)%8));
					if(is_square_corner) dir_=(dir_+7)%8;
					nx__=nx_,ny__=ny_;
					for(DWORD k=0;k<steps_count && !not_passed;k++)
					{
						MoveHexByDirUnsafe(nx__,ny__,dir_);
						not_passed=GetField(nx__,ny__).IsNotPassed;
					}
					if(not_passed) continue;
				}

				GRID(nx,ny)=numindex;
				coords.push_back(WordPairVecVal(nx,ny));

				if(cut>=0 && CheckDist(nx,ny,end_x,end_y,cut))
				{
					end_x=nx;
					end_y=ny;
					return true;
				}

				if(cut<0 && nx==end_x && ny==end_y) goto label_FindOk;
			}
		}
	}
	if(cut>=0) return false;

label_FindOk:
	int x1=coords.back().first;
	int y1=coords.back().second;
	steps.resize(numindex-1);

	// From end
	if(GameOpt.MapHexagonal)
	{
		static bool switcher=false;
		if(!GameOpt.MapSmoothPath) switcher=false;

		while(numindex>1)
		{
			if(GameOpt.MapSmoothPath && numindex&1) switcher=!switcher;
			numindex--;

			if(switcher)
			{
				if(x1&1)
				{
					if(GRID(x1-1,y1-1)==numindex) { steps[numindex-1]=3; x1--; y1--; continue; } // 0
					if(GRID(x1  ,y1-1)==numindex) { steps[numindex-1]=2; y1--;       continue; } // 5
					if(GRID(x1  ,y1+1)==numindex) { steps[numindex-1]=5; y1++;       continue; } // 2
					if(GRID(x1+1,y1  )==numindex) { steps[numindex-1]=0; x1++;       continue; } // 3
					if(GRID(x1-1,y1  )==numindex) { steps[numindex-1]=4; x1--;       continue; } // 1
					if(GRID(x1+1,y1-1)==numindex) { steps[numindex-1]=1; x1++; y1--; continue; } // 4
				}
				else
				{
					if(GRID(x1-1,y1  )==numindex) { steps[numindex-1]=3; x1--;       continue; } // 0
					if(GRID(x1  ,y1-1)==numindex) { steps[numindex-1]=2; y1--;       continue; } // 5
					if(GRID(x1  ,y1+1)==numindex) { steps[numindex-1]=5; y1++;       continue; } // 2
					if(GRID(x1+1,y1+1)==numindex) { steps[numindex-1]=0; x1++; y1++; continue; } // 3
					if(GRID(x1-1,y1+1)==numindex) { steps[numindex-1]=4; x1--; y1++; continue; } // 1
					if(GRID(x1+1,y1  )==numindex) { steps[numindex-1]=1; x1++;       continue; } // 4
				}
			}
			else
			{
				if(x1&1)
				{
					if(GRID(x1-1,y1  )==numindex) { steps[numindex-1]=4; x1--;       continue; } // 1
					if(GRID(x1+1,y1-1)==numindex) { steps[numindex-1]=1; x1++; y1--; continue; } // 4
					if(GRID(x1  ,y1-1)==numindex) { steps[numindex-1]=2; y1--;       continue; } // 5
					if(GRID(x1-1,y1-1)==numindex) { steps[numindex-1]=3; x1--; y1--; continue; } // 0
					if(GRID(x1+1,y1  )==numindex) { steps[numindex-1]=0; x1++;       continue; } // 3
					if(GRID(x1  ,y1+1)==numindex) { steps[numindex-1]=5; y1++;       continue; } // 2
				}
				else
				{
					if(GRID(x1-1,y1+1)==numindex) { steps[numindex-1]=4; x1--; y1++; continue; } // 1
					if(GRID(x1+1,y1  )==numindex) { steps[numindex-1]=1; x1++;       continue; } // 4
					if(GRID(x1  ,y1-1)==numindex) { steps[numindex-1]=2; y1--;       continue; } // 5
					if(GRID(x1-1,y1  )==numindex) { steps[numindex-1]=3; x1--;       continue; } // 0
					if(GRID(x1+1,y1+1)==numindex) { steps[numindex-1]=0; x1++; y1++; continue; } // 3
					if(GRID(x1  ,y1+1)==numindex) { steps[numindex-1]=5; y1++;       continue; } // 2
				}
			}
			return false;
		}
	}
	else
	{
		// Smooth data
		int switch_count,switch_begin;
		if(GameOpt.MapSmoothPath)
		{
			int x2=start_x,y2=start_y;
			int dx=abs(x1-x2);
			int dy=abs(y1-y2);
			int d=max(dx,dy);
			int h1=abs(dx-dy);
			int h2=d-h1;
			switch_count=((h1 && h2)?max(h1,h2)/min(h1,h2)+1:0);
			if(h1 && h2 && switch_count<2) switch_count=2;
			switch_begin=((h1 && h2)?min(h1,h2)%max(h1,h2):0);
		}

		for(int i=switch_begin;numindex>1;i++)
		{
			numindex--;

			// Without smoothing
			if(!GameOpt.MapSmoothPath)
			{
				if(GRID(x1-1,y1  )==numindex) { steps[numindex-1]=4; x1--;       continue; } // 0
				if(GRID(x1  ,y1-1)==numindex) { steps[numindex-1]=2;       y1--; continue; } // 6
				if(GRID(x1  ,y1+1)==numindex) { steps[numindex-1]=6;       y1++; continue; } // 2
				if(GRID(x1+1,y1  )==numindex) { steps[numindex-1]=0; x1++;       continue; } // 4
				if(GRID(x1-1,y1+1)==numindex) { steps[numindex-1]=5; x1--; y1++; continue; } // 1
				if(GRID(x1+1,y1-1)==numindex) { steps[numindex-1]=1; x1++; y1--; continue; } // 5
				if(GRID(x1+1,y1+1)==numindex) { steps[numindex-1]=7; x1++; y1++; continue; } // 3
				if(GRID(x1-1,y1-1)==numindex) { steps[numindex-1]=3; x1--; y1--; continue; } // 7
			}
			// With smoothing
			else
			{
				if(switch_count<2 || i%switch_count)
				{
					if(GRID(x1-1,y1  )==numindex) { steps[numindex-1]=4; x1--;       continue; } // 0
					if(GRID(x1  ,y1+1)==numindex) { steps[numindex-1]=6;       y1++; continue; } // 2
					if(GRID(x1+1,y1  )==numindex) { steps[numindex-1]=0; x1++;       continue; } // 4
					if(GRID(x1  ,y1-1)==numindex) { steps[numindex-1]=2;       y1--; continue; } // 6
					if(GRID(x1+1,y1+1)==numindex) { steps[numindex-1]=7; x1++; y1++; continue; } // 3
					if(GRID(x1-1,y1-1)==numindex) { steps[numindex-1]=3; x1--; y1--; continue; } // 7
					if(GRID(x1-1,y1+1)==numindex) { steps[numindex-1]=5; x1--; y1++; continue; } // 1
					if(GRID(x1+1,y1-1)==numindex) { steps[numindex-1]=1; x1++; y1--; continue; } // 5
				}
				else
				{
					if(GRID(x1+1,y1+1)==numindex) { steps[numindex-1]=7; x1++; y1++; continue; } // 3
					if(GRID(x1-1,y1-1)==numindex) { steps[numindex-1]=3; x1--; y1--; continue; } // 7
					if(GRID(x1-1,y1  )==numindex) { steps[numindex-1]=4; x1--;       continue; } // 0
					if(GRID(x1  ,y1+1)==numindex) { steps[numindex-1]=6;       y1++; continue; } // 2
					if(GRID(x1+1,y1  )==numindex) { steps[numindex-1]=0; x1++;       continue; } // 4
					if(GRID(x1  ,y1-1)==numindex) { steps[numindex-1]=2;       y1--; continue; } // 6
					if(GRID(x1-1,y1+1)==numindex) { steps[numindex-1]=5; x1--; y1++; continue; } // 1
					if(GRID(x1+1,y1-1)==numindex) { steps[numindex-1]=1; x1++; y1--; continue; } // 5
				}
			}
			return false;
		}
	}
	return true;
}

bool HexManager::CutPath(CritterCl* cr, WORD start_x, WORD start_y, WORD& end_x, WORD& end_y, int cut)
{
	static ByteVec dummy;
	return FindPath(cr,start_x,start_y,end_x,end_y,dummy,cut);
}

bool HexManager::TraceBullet(WORD hx, WORD hy, WORD tx, WORD ty, DWORD dist, float angle, CritterCl* find_cr, bool find_cr_safe,
							 CritVec* critters, int find_type, WordPair* pre_block, WordPair* block, WordPairVec* steps, bool check_passed)
{
	if(IsShowTrack()) ClearHexTrack();

	if(!dist) dist=DistGame(hx,hy,tx,ty);

	WORD cx=hx;
	WORD cy=hy;
	WORD old_cx=cx;
	WORD old_cy=cy;
	BYTE dir;

	LineTracer line_tracer(hx,hy,tx,ty,maxHexX,maxHexY,angle,!GameOpt.MapHexagonal);

	for(DWORD i=0;i<dist;i++)
	{
		if(GameOpt.MapHexagonal)
		{
			dir=line_tracer.GetNextHex(cx,cy);
			WriteLog("%u) dir<%u>\n",i,dir);
		}
		else
		{
			line_tracer.GetNextSquare(cx,cy);
			dir=GetNearDir(old_cx,old_cy,cx,cy);
		}

		if(IsShowTrack()) GetHexTrack(cx,cy)=(cx==tx && cy==ty?1:2);

		if(steps)
		{
			steps->push_back(WordPairVecVal(cx,cy));
			continue;
		}

		Field& f=GetField(cx,cy);
		if(check_passed && f.IsNotRaked) break;
		if(critters!=NULL) GetCritters(cx,cy,*critters,find_type);
		if(find_cr!=NULL && f.Crit)
		{
			CritterCl* cr=f.Crit;
			if(cr && cr==find_cr) return true;
			if(find_cr_safe) return false;
		}

		old_cx=cx;
		old_cy=cy;
	}

	if(pre_block)
	{
		(*pre_block).first=old_cx;
		(*pre_block).second=old_cy;
	}
	if(block)
	{
		(*block).first=cx;
		(*block).second=cy;
	}
	return false;
}

void HexManager::FindSetCenter(int x, int y)
{
	if(!viewField) return;
	RebuildMap(x,y);

#ifdef FONLINE_CLIENT
	int iw=VIEW_WIDTH/2+2;
	int ih=VIEW_HEIGHT/2+2;
	WORD hx=x;
	WORD hy=y;
	ByteVec dirs;

	// Up
	dirs.clear();
	dirs.push_back(0);
	dirs.push_back(5);
	FindSetCenterDir(hx,hy,dirs,ih);
	// Down
	dirs.clear();
	dirs.push_back(3);
	dirs.push_back(2);
	FindSetCenterDir(hx,hy,dirs,ih);
	// Left
	dirs.clear();
	dirs.push_back(1);
	FindSetCenterDir(hx,hy,dirs,iw);
	// Right
	dirs.clear();
	dirs.push_back(4);
	FindSetCenterDir(hx,hy,dirs,iw);

	// Up-Right
	dirs.clear();
	dirs.push_back(0);
	FindSetCenterDir(hx,hy,dirs,ih);
	// Down-Left
	dirs.clear();
	dirs.push_back(3);
	FindSetCenterDir(hx,hy,dirs,ih);
	// Up-Left
	dirs.clear();
	dirs.push_back(5);
	FindSetCenterDir(hx,hy,dirs,ih);
	// Down-Right
	dirs.clear();
	dirs.push_back(2);
	FindSetCenterDir(hx,hy,dirs,ih);

	RebuildMap(hx,hy);
#endif //FONLINE_CLIENT
}

void HexManager::FindSetCenterDir(WORD& x, WORD& y, ByteVec& dirs, int steps)
{
	if(dirs.empty()) return;

	WORD sx=x;
	WORD sy=y;
	int cur_dir=0;

	int i;
	for(i=0;i<steps;i++)
	{
		MoveHexByDir(sx,sy,dirs[cur_dir],maxHexX,maxHexY);
		cur_dir++;
		if(cur_dir>=dirs.size()) cur_dir=0;

		GetHexTrack(sx,sy)=1;
		if(GetField(sx,sy).ScrollBlock) break;
		GetHexTrack(sx,sy)=2;
	}

	for(;i<steps;i++)
	{
		MoveHexByDir(x,y,ReverseDir(dirs[cur_dir]),maxHexX,maxHexY);
		cur_dir++;
		if(cur_dir>=dirs.size()) cur_dir=0;
	}
}

bool HexManager::LoadMap(WORD map_pid)
{
	WriteLog("Load map...");

	// Unload previous
	UnloadMap();

	// Check data sprites reloading
	if(curDataPrefix!=GameOpt.MapDataPrefix) ReloadSprites();

	// Tiles surface
	InitTilesSurf();

	// Make name
	char map_name[256];
	sprintf(map_name,"map%u",map_pid);

	// Find in cache
	DWORD cache_len;
	BYTE* cache=Crypt.GetCache(map_name,cache_len);
	if(!cache)
	{
		WriteLog("Load map<%s> from cache fail.\n",map_name);
		return false;
	}

	FileManager fm;
	if(!fm.LoadStream(cache,cache_len))
	{
		WriteLog("Load map<%s> from stream fail.\n",map_name);
		delete[] cache;
		return false;
	}
	delete[] cache;

	// Header
	DWORD buf_len=fm.GetFsize();
	BYTE* buf=Crypt.Uncompress(fm.GetBuf(),buf_len,50);
	if(!buf)
	{
		WriteLog("Uncompress map fail.\n");
		return false;
	}

	fm.UnloadFile();
	fm.LoadStream(buf,buf_len);
	delete[] buf;

	if(fm.GetBEDWord()!=CLIENT_MAP_FORMAT_VER)
	{
		WriteLog("Map Format is not supported.\n");
		return false;
	}

	if(fm.GetBEDWord()!=map_pid)
	{
		WriteLog("Data truncated.\n");
		return false;
	}

	// Reserved
	WORD maxhx=fm.GetBEWord();
	if(maxhx==0xAABB) maxhx=400;
	WORD maxhy=fm.GetBEWord();
	if(maxhy==0xCCDD) maxhy=400;
	fm.GetBEDWord();
	fm.GetBEDWord();

	DWORD tiles_count=fm.GetBEDWord();
	DWORD walls_count=fm.GetBEDWord();
	DWORD scen_count=fm.GetBEDWord();
	DWORD tiles_len=fm.GetBEDWord();
	DWORD walls_len=fm.GetBEDWord();
	DWORD scen_len=fm.GetBEDWord();

	if(tiles_count*sizeof(ProtoMap::Tile)!=tiles_len)
	{
		WriteLog("Tiles data truncated.\n");
		return false;
	}

	if(walls_count*sizeof(SceneryCl)!=walls_len)
	{
		WriteLog("Walls data truncated.\n");
		return false;
	}

	if(scen_count*sizeof(SceneryCl)!=scen_len)
	{
		WriteLog("Scenery data truncated.\n");
		return false;
	}

	// Create field
	if(!ResizeField(maxhx,maxhy))
	{
		WriteLog("Buffer allocation fail.\n");
		return false;
	}

	// Tiles
	fm.SetCurPos(0x2C);
	curHashTiles=maxhx*maxhy;
	Crypt.Crc32(fm.GetCurBuf(),tiles_len,curHashTiles);

	for(DWORD i=0;i<tiles_count;i++)
	{
		ProtoMap::Tile tile;
		if(!fm.CopyMem(&tile,sizeof(tile)))
		{
			WriteLog("Unable to read tile.\n");
			continue;
		}
		if(tile.HexX>=maxHexX || tile.HexY>=maxHexY) continue;

		Field& f=GetField(tile.HexX,tile.HexY);
		AnyFrames* anim=ResMngr.GetItemAnim(tile.NameHash);
		if(anim)
		{
			if(!tile.IsRoof)
				f.AddTile(anim,tile.OffsX,tile.OffsY,tile.Layer,false);
			else
				f.AddTile(anim,tile.OffsX,tile.OffsY,tile.Layer,true);

			CheckTilesBorder(tile.IsRoof?f.Roofs.back():f.Tiles.back(),tile.IsRoof);
		}
	}

	// Roof indexes
	int roof_num=1;
	for(int hx=0;hx<maxHexX;hx++)
	{
		for(int hy=0;hy<maxHexY;hy++)
		{
			if(GetField(hx,hy).Roofs.size())
			{
				MarkRoofNum(hx,hy,roof_num);
				roof_num++;
			}
		}
	}

	// Walls
	fm.SetCurPos(0x2C+tiles_len);
	curHashWalls=maxhx*maxhy;
	Crypt.Crc32(fm.GetCurBuf(),walls_len,curHashWalls);

	for(DWORD i=0;i<walls_count;i++)
	{
		SceneryCl cur_wall;
		ZeroMemory(&cur_wall,sizeof(cur_wall));

		if(!fm.CopyMem(&cur_wall,sizeof(cur_wall)))
		{
			WriteLog("Unable to read wall item.\n");
			continue;
		}

		if(!ParseScenery(cur_wall))
		{
			WriteLog("Unable to parse wall item.\n");
			continue;
		}
	}

	// Scenery
	fm.SetCurPos(0x2C+tiles_len+walls_len);
	curHashScen=maxhx*maxhy;
	Crypt.Crc32(fm.GetCurBuf(),scen_len,curHashScen);

	for(DWORD i=0;i<scen_count;i++)
	{
		SceneryCl cur_scen;
		ZeroMemory(&cur_scen,sizeof(cur_scen));

		if(!fm.CopyMem(&cur_scen,sizeof(cur_scen)))
		{
			WriteLog("Unable to read scenery item.\n");
			continue;
		}

		if(!ParseScenery(cur_scen))
		{
			WriteLog("Unable to parse scenery item<%d>.\n",cur_scen.ProtoId);
			continue;
		}
	}

	// Scroll blocks borders
	for(int hx=0;hx<maxHexX;hx++)
	{
		for(int hy=0;hy<maxHexY;hy++)
		{
			Field& f=GetField(hx,hy);
			if(f.ScrollBlock)
			{
				for(int i=0;i<6;i++)
				{
					WORD hx_=hx,hy_=hy;
					MoveHexByDir(hx_,hy_,i,maxHexX,maxHexY);
					GetField(hx_,hy_).IsNotPassed=true;
				}
			}
		}
	}

	// Light
	CollectLightSources();
	lightPoints.clear();
	lightPointsCount=0;

	// Visible
	hVisible=VIEW_HEIGHT+hTop+hBottom;
	wVisible=VIEW_WIDTH+wLeft+wRight;
	SAFEDELA(viewField);
	viewField=new ViewField[hVisible*wVisible];

	// Finish
	curPidMap=map_pid;
	curMapTime=-1;
	AutoScroll.Active=false;
	WriteLog("Load map success.\n");
	return true;
}

void HexManager::UnloadMap()
{
	if(!IsMapLoaded()) return;

	SAFEDELA(viewField);

	hTop=0;
	hBottom=0;
	wLeft=0;
	wRight=0;
	hVisible=0;
	wVisible=0;
	screenHexX=0;
	screenHexY=0;

	lightSources.clear();
	lightSourcesScen.clear();

	mainTree.Unvalidate();
	roofTree.Unvalidate();
	roofRainTree.Unvalidate();

	for(DropVecIt it=rainData.begin(),end=rainData.end();it!=end;++it)
		delete *it;
	rainData.clear();

	for(int i=0,j=hexItems.size();i<j;i++)
		hexItems[i]->Release();
	hexItems.clear();

	ResizeField(0,0);
	ClearCritters();

	curPidMap=0;
	curMapTime=-1;
	curHashTiles=0;
	curHashWalls=0;
	curHashScen=0;

	crittersContour=0;
	critterContour=0;

	for(TerrainVecIt it=tilesTerrain.begin(),end=tilesTerrain.end();it!=end;++it) delete *it;
	tilesTerrain.clear();
}

void HexManager::GetMapHash(WORD map_pid, DWORD& hash_tiles, DWORD& hash_walls, DWORD& hash_scen)
{
	WriteLog("Get hash of map, pid<%u>...",map_pid);

	hash_tiles=0;
	hash_walls=0;
	hash_scen=0;

	if(map_pid==curPidMap)
	{
		hash_tiles=curHashTiles;
		hash_walls=curHashWalls;
		hash_scen=curHashScen;

		WriteLog("Hashes of loaded map: tiles<%u>, walls<%u>, scenery<%u>.\n",hash_tiles,hash_walls,hash_scen);
		return;
	}

	char map_name[256];
	sprintf(map_name,"map%u",map_pid);

	DWORD cache_len;
	BYTE* cache=Crypt.GetCache(map_name,cache_len);
	if(!cache)
	{
		WriteLog("Load map<%s> from cache fail.\n",map_name);
		return;
	}

	FileManager fm;
	if(!fm.LoadStream(cache,cache_len))
	{
		WriteLog("Load map<%s> from stream fail.\n",map_name);
		delete[] cache;
		return;
	}
	delete[] cache;

	DWORD buf_len=fm.GetFsize();
	BYTE* buf=Crypt.Uncompress(fm.GetBuf(),buf_len,50);
	if(!buf)
	{
		WriteLog("Uncompress map fail.\n");
		return;
	}

	fm.LoadStream(buf,buf_len);
	delete[] buf;

	if(fm.GetBEDWord()!=CLIENT_MAP_FORMAT_VER)
	{
		WriteLog("Map format is not supported.\n");
		return;
	}

	if(fm.GetBEDWord()!=map_pid)
	{
		WriteLog("Invalid proto number.\n");
		return;
	}

	// Reserved
	WORD maxhx=fm.GetBEWord();
	if(maxhx==0xAABB) maxhx=400;
	WORD maxhy=fm.GetBEWord();
	if(maxhy==0xCCDD) maxhy=400;
	fm.GetBEDWord();
	fm.GetBEDWord();
	DWORD tiles_count=fm.GetBEDWord();
	DWORD walls_count=fm.GetBEDWord();
	DWORD scen_count=fm.GetBEDWord();
	DWORD tiles_len=fm.GetBEDWord();
	DWORD walls_len=fm.GetBEDWord();
	DWORD scen_len=fm.GetBEDWord();

	// Data Check Sum
	if(tiles_count*sizeof(ProtoMap::Tile)!=tiles_len)
	{
		WriteLog("Invalid check sum tiles.\n");
		return;
	}
	
	if(walls_count*sizeof(SceneryCl)!=walls_len)
	{
		WriteLog("Invalid check sum walls.\n");
		return;
	}
	
	if(scen_count*sizeof(SceneryCl)!=scen_len)
	{
		WriteLog("Invalid check sum scenery.\n");
		return;
	}

	fm.SetCurPos(0x2C);
	hash_tiles=maxhx*maxhy;
	Crypt.Crc32(fm.GetCurBuf(),tiles_len,hash_tiles);
	fm.SetCurPos(0x2C+tiles_len);
	hash_walls=maxhx*maxhy;
	Crypt.Crc32(fm.GetCurBuf(),walls_len,hash_walls);
	fm.SetCurPos(0x2C+tiles_len+walls_len);
	hash_scen=maxhx*maxhy;
	Crypt.Crc32(fm.GetCurBuf(),scen_len,hash_scen);

	WriteLog("complete.\n");
}

bool HexManager::GetMapData(WORD map_pid, ItemVec& items, WORD& maxhx, WORD& maxhy)
{
	char map_name[256];
	sprintf(map_name,"map%u",map_pid);

	DWORD cache_len;
	BYTE* cache=Crypt.GetCache(map_name,cache_len);
	if(!cache) return false;

	FileManager fm;
	if(!fm.LoadStream(cache,cache_len))
	{
		delete[] cache;
		return false;
	}
	delete[] cache;

	DWORD buf_len=fm.GetFsize();
	BYTE* buf=Crypt.Uncompress(fm.GetBuf(),buf_len,50);
	if(!buf) return false;

	if(!fm.LoadStream(buf,buf_len))
	{
		delete[] buf;
		return false;
	}
	delete[] buf;

	if(fm.GetBEDWord()!=CLIENT_MAP_FORMAT_VER) return false;

	fm.GetBEDWord();
	maxhx=fm.GetBEWord();
	maxhy=fm.GetBEWord();
	fm.GetBEDWord();
	fm.GetBEDWord();
	fm.GetBEDWord();
	DWORD walls_count=fm.GetBEDWord();
	DWORD scen_count=fm.GetBEDWord();
	DWORD tiles_len=fm.GetBEDWord();
	DWORD walls_len=fm.GetBEDWord();

	items.reserve(walls_count+scen_count);

	// Walls
	fm.SetCurPos(0x2C+tiles_len);
	for(DWORD i=0;i<walls_count+scen_count;i++)
	{
		if(i==walls_count) fm.SetCurPos(0x2C+tiles_len+walls_len);
		SceneryCl scenwall;
		if(!fm.CopyMem(&scenwall,sizeof(scenwall))) return false;

		ProtoItem* proto_item=ItemMngr.GetProtoItem(scenwall.ProtoId);
		if(proto_item)
		{
			Item item;
			item.Init(proto_item);
			item.Accessory=ITEM_ACCESSORY_NONE;
			item.ACC_HEX.HexX=scenwall.MapX;
			item.ACC_HEX.HexY=scenwall.MapY;
			items.push_back(item);
		}
	}

	return true;
}

bool HexManager::ParseScenery(SceneryCl& scen)
{
	WORD pid=scen.ProtoId;
	WORD hx=scen.MapX;
	WORD hy=scen.MapY;

	if(hx>=maxHexX || hy>=maxHexY)
	{
		WriteLog(__FUNCTION__" - Invalid coord<%d,%d>.\n",hx,hy);
		return false;
	}

	ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
	if(!proto_item)
	{
		WriteLog(__FUNCTION__" - Proto item not found<%d>.\n",pid);
		return false;
	}

	static DWORD scen_id=0;
	scen_id--;

	ItemHex* scenery=new ItemHex(scen_id,proto_item,NULL,hx,hy,scen.Dir,
		scen.OffsetX,scen.OffsetY,&GetField(hx,hy).ScrX,&GetField(hx,hy).ScrY,
		scen.SpriteCut);
	scenery->ScenFlags=scen.Flags;

	// Mapper additional parameters
	bool refresh_anim=false;
	scenery->Data.LightIntensity=scen.LightIntensity;
	scenery->Data.LightDistance=scen.LightDistance;
	scenery->Data.LightColor=scen.LightColor;
	scenery->Data.LightFlags=scen.LightFlags;
	if(scen.InfoOffset) scenery->Data.Info=scen.InfoOffset;
	if(scen.AnimStayBegin || scen.AnimStayEnd)
	{
		SETFLAG(scenery->Data.Flags,ITEM_SHOW_ANIM);
		SETFLAG(scenery->Data.Flags,ITEM_SHOW_ANIM_EXT);
		scenery->Data.AnimShow[0]=scen.AnimStayBegin;
		scenery->Data.AnimShow[1]=scen.AnimStayEnd;
		scenery->Data.AnimStay[0]=scen.AnimStayBegin;
		scenery->Data.AnimStay[1]=scen.AnimStayEnd;
		scenery->Data.AnimHide[0]=scen.AnimStayBegin;
		scenery->Data.AnimHide[1]=scen.AnimStayEnd;
		refresh_anim=true;
	}
	if(scen.AnimWait) scenery->Data.AnimWaitBase=scen.AnimWait;
	if(scen.PicMapHash)
	{
		scenery->Data.PicMapHash=scen.PicMapHash;
		refresh_anim=true;
	}

	if(scenery->IsLight() || scen.LightIntensity)
		lightSourcesScen.push_back(LightSource(hx,hy,scenery->LightGetColor(),scenery->LightGetDistance(),scenery->LightGetIntensity(),scenery->LightGetFlags()));

	scenery->RefreshAlpha();
	if(refresh_anim) scenery->RefreshAnim();
	PushItem(scenery);
	if(!scenery->IsHidden() && !scenery->IsFullyTransparent()) ProcessHexBorders(scenery->Anim->GetSprId(0),scenery->StartScrX,scenery->StartScrY);
	return true;
}

int HexManager::GetDayTime()
{
	return (GameOpt.Hour*60+GameOpt.Minute);
}

int HexManager::GetMapTime()
{
	if(curMapTime<0) return GetDayTime();
	return curMapTime;
}

int* HexManager::GetMapDayTime()
{
	return dayTime;
}

BYTE* HexManager::GetMapDayColor()
{
	return dayColor;
}

#ifdef FONLINE_MAPPER
bool HexManager::SetProtoMap(ProtoMap& pmap)
{
	WriteLog("Create map from prototype.\n");

	UnloadMap();
	CurProtoMap=NULL;

	if(curDataPrefix!=GameOpt.MapDataPrefix) ReloadSprites();

	InitTilesSurf();

	if(!ResizeField(pmap.Header.MaxHexX,pmap.Header.MaxHexY))
	{
		WriteLog("Buffer allocation fail.\n");
		return false;
	}

	for(int i=0;i<4;i++) dayTime[i]=pmap.Header.DayTime[i];
	for(int i=0;i<12;i++) dayColor[i]=pmap.Header.DayColor[i];

	// Tiles
	for(WORD hy=0;hy<pmap.Header.MaxHexY;hy++)
	{
		for(WORD hx=0;hx<pmap.Header.MaxHexX;hx++)
		{
			Field& f=GetField(hx,hy);

			for(int r=0;r<=1;r++) // Tile/roof
			{
				ProtoMap::TileVec& tiles=pmap.GetTiles(hx,hy,r!=0);

				for(size_t i=0,j=tiles.size();i<j;i++)
				{
					ProtoMap::Tile& tile=tiles[i];
					AnyFrames* anim=ResMngr.GetItemAnim(tile.NameHash);
					if(anim)
					{
						Field::Tile ftile;
						ftile.Anim=anim;
						ftile.OffsX=tile.OffsX;
						ftile.OffsY=tile.OffsY;
						ftile.Layer=tile.Layer;

						if(!tile.IsRoof)
							f.Tiles.push_back(ftile);
						else
							f.Roofs.push_back(ftile);

						CheckTilesBorder(ftile,tile.IsRoof);
					}
				}
			}
		}
	}

	// Objects
	DWORD cur_id=0;
	for(int i=0,j=pmap.MObjects.size();i<j;i++)
	{
		MapObject* o=pmap.MObjects[i];
		if(o->MapX>=maxHexX || o->MapY>=maxHexY)
		{
			WriteLog("Invalid position of map object. Skip.\n");
			continue;
		}

		if(o->MapObjType==MAP_OBJECT_SCENERY)
		{
			SceneryCl s;
			ZeroMemory(&s,sizeof(s));
			s.ProtoId=o->ProtoId;
			s.MapX=o->MapX;
			s.MapY=o->MapY;
			s.OffsetX=o->MScenery.OffsetX;
			s.OffsetY=o->MScenery.OffsetY;
			s.LightColor=o->LightColor;
			s.LightDistance=o->LightDistance;
			s.LightFlags=o->LightDirOff|((o->LightDay&3)<<6);
			s.LightIntensity=o->LightIntensity;
			s.Dir=o->Dir;
			s.SpriteCut=o->MScenery.SpriteCut;
			s.PicMapHash=o->MScenery.PicMapHash;

			if(!ParseScenery(s))
			{
				WriteLog("Unable to parse scen or wall object.\n");
				continue;
			}
			ItemHex* item=hexItems.back();
			AffectItem(o,item);
			o->RunTime.MapObjId=item->GetId();
		}
		else if(o->MapObjType==MAP_OBJECT_ITEM)
		{
			if(o->MItem.InContainer) continue;

			ProtoItem* proto=ItemMngr.GetProtoItem(o->ProtoId);
			if(!proto) continue;

			Field& f=GetField(o->MapX,o->MapY);
			ItemHex* item=new ItemHex(++cur_id,proto,NULL,o->MapX,o->MapY,proto->Dir,0,0,&f.ScrX,&f.ScrY,0);
			PushItem(item);
			AffectItem(o,item);
			o->RunTime.MapObjId=cur_id;

			ProcessHexBorders(item->SprId,item->StartScrX,item->StartScrY);
		}
		else if(o->MapObjType==MAP_OBJECT_CRITTER)
		{
			CritData* pnpc=CrMngr.GetProto(o->ProtoId);
			if(!pnpc)
			{
				WriteLog("Proto<%u> npc not found.\n",o->ProtoId);
				continue;
			}

			ProtoItem* pitem_main=NULL;
			ProtoItem* pitem_ext=NULL;
			ProtoItem* pitem_armor=NULL;
			for(int k=0,l=pmap.MObjects.size();k<l;k++)
			{
				MapObject* child=pmap.MObjects[k];
				if(child->MapObjType==MAP_OBJECT_ITEM && child->MItem.InContainer && child->MapX==o->MapX && child->MapY==o->MapY)
				{
					if(child->MItem.ItemSlot==SLOT_HAND1) pitem_main=ItemMngr.GetProtoItem(child->ProtoId);
					else if(child->MItem.ItemSlot==SLOT_HAND2) pitem_ext=ItemMngr.GetProtoItem(child->ProtoId);
					else if(child->MItem.ItemSlot==SLOT_ARMOR) pitem_armor=ItemMngr.GetProtoItem(child->ProtoId);
				}
			}

			CritterCl* cr=new CritterCl();
			cr->SetBaseType(pnpc->BaseType);
			cr->DefItemSlotHand->Init(pitem_main?pitem_main:ItemMngr.GetProtoItem(ITEM_DEF_SLOT));
			cr->DefItemSlotArmor->Init(pitem_armor?pitem_armor:ItemMngr.GetProtoItem(ITEM_DEF_ARMOR));
			memcpy(cr->Params,pnpc->Params,sizeof(pnpc->Params));
			cr->HexX=o->MapX;
			cr->HexY=o->MapY;
			cr->Flags=o->ProtoId;
			cr->SetDir(o->Dir);
			cr->Id=++cur_id;
			cr->Init();
			AffectCritter(o,cr);
			AddCrit(cr);
			o->RunTime.MapObjId=cur_id;
		}
	}

	hVisible=VIEW_HEIGHT+hTop+hBottom;
	wVisible=VIEW_WIDTH+wLeft+wRight;
	SAFEDELA(viewField);
	viewField=new ViewField[hVisible*wVisible];

	curPidMap=0xFFFF;
//	curMapTime=pmap.Time;
	curHashTiles=0xFFFF;
	curHashWalls=0xFFFF;
	curHashScen=0xFFFF;
	CurProtoMap=&pmap;
	WriteLog("Create map from prototype complete.\n");
	return true;
}

void HexManager::ClearSelTiles()
{
	if(!CurProtoMap) return;

	for(int hx=0;hx<maxHexX;hx++)
	{
		for(int hy=0;hy<maxHexY;hy++)
		{
			Field& f=GetField(hx,hy);
			if(f.Tiles.size())
			{
				ProtoMap::TileVec& tiles=CurProtoMap->GetTiles(hx,hy,false);
				for(size_t i=0;i<tiles.size();)
				{
					if(tiles[i].IsSelected)
					{
						tiles.erase(tiles.begin()+i);
						f.Tiles.erase(f.Tiles.begin()+i);
					}
					else i++;
				}
			}
			if(f.Roofs.size())
			{
				ProtoMap::TileVec& roofs=CurProtoMap->GetTiles(hx,hy,true);
				for(size_t i=0;i<roofs.size();)
				{
					if(roofs[i].IsSelected)
					{
						roofs.erase(roofs.begin()+i);
						f.Roofs.erase(f.Roofs.begin()+i);
					}
					else i++;
				}
			}
		}
	}
}

void HexManager::ParseSelTiles()
{
	if(!CurProtoMap) return;

	bool borders_changed=false;
	for(int hx=0;hx<maxHexX;hx++)
	{
		for(int hy=0;hy<maxHexY;hy++)
		{
			Field& f=GetField(hx,hy);
			if(f.Tiles.size())
			{
				ProtoMap::TileVec& tiles=CurProtoMap->GetTiles(hx,hy,false);
				for(size_t i=0,j=tiles.size();i<j;i++)
					if(tiles[i].IsSelected) tiles[i].IsSelected=false;
			}
			if(f.Roofs.size())
			{
				ProtoMap::TileVec& roofs=CurProtoMap->GetTiles(hx,hy,true);
				for(size_t i=0,j=roofs.size();i<j;i++)
					if(roofs[i].IsSelected) roofs[i].IsSelected=false;
			}
		}
	}
}

void HexManager::SetTile(DWORD name_hash, WORD hx, WORD hy, short ox, short oy, BYTE layer, bool is_roof, bool select)
{
	Field& f=GetField(hx,hy);

	AnyFrames* anim=ResMngr.GetItemAnim(name_hash);
	if(!anim) return;

	if(is_roof)
	{
		f.AddTile(anim,0,0,layer,true);
		ProtoMap::TileVec& roofs=CurProtoMap->GetTiles(hx,hy,true);
		roofs.push_back(ProtoMap::Tile(name_hash,hx,hy,ox,oy,layer,true));
		roofs.back().IsSelected=select;
	}
	else
	{
		f.AddTile(anim,0,0,layer,false);
		ProtoMap::TileVec& tiles=CurProtoMap->GetTiles(hx,hy,false);
		tiles.push_back(ProtoMap::Tile(name_hash,hx,hy,ox,oy,layer,false));
		tiles.back().IsSelected=select;
	}

	if(CheckTilesBorder(is_roof?f.Roofs.back():f.Tiles.back(),is_roof))
	{
		hVisible=VIEW_HEIGHT+hTop+hBottom;
		wVisible=VIEW_WIDTH+wLeft+wRight;
		delete[] viewField;
		viewField=new ViewField[hVisible*wVisible];
		RefreshMap();
	}
	else
	{
		if(is_roof)
			RebuildRoof();
		else
			RebuildTiles();
	}
}

/*void HexManager::SetTerrain(WORD hx, WORD hy, DWORD name_hash)
{
	Field& f=GetField(hx,hy);
	f.TerrainId=name_hash;
	CurProtoMap->SetRoof(hx/2,hy/2,name_hash?0xAAAAAAAA:0);
	CurProtoMap->SetTile(hx/2,hy/2,name_hash);
	RebuildTerrain();
	RebuildTiles();
	RebuildRoof();
}

void HexManager::RebuildTerrain()
{
	for(TerrainVecIt it=tilesTerrain.begin(),end=tilesTerrain.end();it!=end;++it) delete *it;
	tilesTerrain.clear();

	for(int tx=0;tx<maxHexX/2;tx++)
	{
		for(int ty=0;ty<maxHexY/2;ty++)
		{
			Field& f=GetField(tx*2,ty*2);
			if(f.TerrainId) AddTerrain(f.TerrainId,tx*2,ty*2);
			if(f.SelTerrain) AddTerrain(f.SelTerrain,tx*2,ty*2);
		}
	}
	reprepareTiles=true;
}*/

void HexManager::AddFastPid(WORD pid)
{
	fastPids.insert(pid);
}

bool HexManager::IsFastPid(WORD pid)
{
	return fastPids.count(pid)!=0;
}

void HexManager::ClearFastPids()
{
	fastPids.clear();
}

void HexManager::SwitchIgnorePid(WORD pid)
{
	if(ignorePids.count(pid)) ignorePids.erase(pid);
	else ignorePids.insert(pid);
}

bool HexManager::IsIgnorePid(WORD pid)
{
	return ignorePids.count(pid)!=0;
}

void HexManager::GetHexesRect(INTRECT& rect, WordPairVec& hexes)
{
	hexes.clear();

	if(GameOpt.MapHexagonal)
	{
		int x,y;
		GetHexInterval(rect[0],rect[1],rect[2],rect[3],x,y);
		x=-x;

		int dx=x/HEX_W;
		int dy=y/HEX_LINE_H;
		int adx=abs(dx);
		int ady=abs(dy);

		int hx,hy;
		for(int j=0;j<=ady;j++)
		{
			if(dy>=0)
			{
				hx=rect[0]+j/2+(j&1);
				hy=rect[1]+(j-(hx-rect[0]-((rect[0]&1)?1:0))/2);
			}
			else
			{
				hx=rect[0]-j/2-(j&1);
				hy=rect[1]-(j-(rect[0]-hx-((rect[0]&1)?0:1))/2);
			}

			for(int i=0;i<=adx;i++)
			{
				if(hx>=0 && hy>=0 && hx<maxHexX && hy<maxHexY)
					hexes.push_back(WordPairVecVal(hx,hy));

				if(dx>=0)
				{
					if(hx&1) hy--;
					hx++;
				}
				else
				{
					hx--;
					if(hx&1) hy++;
				}
			}
		}
	}
	else
	{
		int rw,rh; // Rect width/height
		GetHexInterval(rect[0],rect[1],rect[2],rect[3],rw,rh);
		if(!rw) rw=1;
		if(!rh) rh=1;

		int hw=abs(rw/(HEX_W/2))+((rw%(HEX_W/2))?1:0)+(abs(rw)>=HEX_W/2?1:0); // Hexes width
		int hh=abs(rh/HEX_LINE_H)+((rh%HEX_LINE_H)?1:0)+(abs(rh)>=HEX_LINE_H?1:0); // Hexes height
		int shx=rect[0];
		int shy=rect[1];

		for(int i=0;i<hh;i++)
		{
			int hx=shx;
			int hy=shy;

			if(rh>0)
			{
				if(rw>0)
				{
					if(i&1) shx++;
					else shy++;
				}
				else
				{
					if(i&1) shy++;
					else shx++;
				}
			}
			else
			{
				if(rw>0)
				{
					if(i&1) shy--;
					else shx--;
				}
				else
				{
					if(i&1) shx--;
					else shy--;
				}
			}

			for(int j=(i&1)?1:0;j<hw;j+=2)
			{
				if(hx>=0 && hy>=0 && hx<maxHexX && hy<maxHexY)
					hexes.push_back(WordPairVecVal(hx,hy));

				if(rw>0) hx--,hy++;
				else hx++,hy--;
			}
		}
	}
}

void HexManager::MarkPassedHexes()
{
	for(int hx=0;hx<maxHexX;hx++)
	{
		for(int hy=0;hy<maxHexY;hy++)
		{
			Field& f=GetField(hx,hy);
			char& track=GetHexTrack(hx,hy);
			track=0;
			if(f.IsNotPassed) track=2;
			if(f.IsNotRaked) track=1;
		}
	}
	RefreshMap();
}

void HexManager::AffectItem(MapObject* mobj, ItemHex* item)
{
	DWORD old_spr_id=item->SprId;
	short old_ox=item->StartScrX;
	short old_oy=item->StartScrY;

	mobj->LightIntensity=CLAMP(mobj->LightIntensity,-100,100);

	if(mobj->MItem.AnimStayBegin || mobj->MItem.AnimStayEnd)
	{
		SETFLAG(item->Data.Flags,ITEM_SHOW_ANIM);
		SETFLAG(item->Data.Flags,ITEM_SHOW_ANIM_EXT);
		item->Data.AnimShow[0]=mobj->MItem.AnimStayBegin;
		item->Data.AnimShow[1]=mobj->MItem.AnimStayEnd;
		item->Data.AnimStay[0]=mobj->MItem.AnimStayBegin;
		item->Data.AnimStay[1]=mobj->MItem.AnimStayEnd;
		item->Data.AnimHide[0]=mobj->MItem.AnimStayBegin;
		item->Data.AnimHide[1]=mobj->MItem.AnimStayEnd;
	}
	else
	{
		UNSETFLAG(item->Data.Flags,ITEM_SHOW_ANIM);
		UNSETFLAG(item->Data.Flags,ITEM_SHOW_ANIM_EXT);
		SETFLAG(item->Data.Flags,item->Proto->Flags&ITEM_SHOW_ANIM);
		SETFLAG(item->Data.Flags,item->Proto->Flags&ITEM_SHOW_ANIM_EXT);
		item->Data.AnimShow[0]=item->Proto->AnimShow[0];
		item->Data.AnimShow[1]=item->Proto->AnimShow[1];
		item->Data.AnimStay[0]=item->Proto->AnimStay[0];
		item->Data.AnimStay[1]=item->Proto->AnimStay[1];
		item->Data.AnimHide[0]=item->Proto->AnimHide[0];
		item->Data.AnimHide[1]=item->Proto->AnimHide[1];
	}

	if(mobj->MItem.AnimWait) item->Data.AnimWaitBase=mobj->MItem.AnimWait;
	else item->Data.AnimWaitBase=item->Proto->AnimWaitBase;

	item->Data.LightIntensity=mobj->LightIntensity;
	item->Data.LightColor=mobj->LightColor;
	item->Data.LightFlags=(mobj->LightDirOff|((mobj->LightDay&3)<<6));
	item->Data.LightDistance=mobj->LightDistance;

	mobj->MItem.PicMapHash=(mobj->RunTime.PicMapName[0]?Str::GetHash(mobj->RunTime.PicMapName):0);
	mobj->MItem.PicInvHash=(mobj->RunTime.PicInvName[0]?Str::GetHash(mobj->RunTime.PicInvName):0);
	item->Data.PicMapHash=mobj->MItem.PicMapHash;
	item->Data.PicInvHash=mobj->MItem.PicInvHash;
	item->Dir=mobj->Dir;

	item->Data.Info=mobj->MItem.InfoOffset;
	item->StartScrX=mobj->MItem.OffsetX;
	item->StartScrY=mobj->MItem.OffsetY;

	if(item->IsHasLocker()) item->Data.Locker.Condition=mobj->MItem.LockerCondition;

	int cut=(mobj->MScenery.SpriteCut?mobj->MScenery.SpriteCut:item->Proto->SpriteCut);
	if(mobj->MapObjType==MAP_OBJECT_SCENERY && item->SpriteCut!=cut)
	{
		item->SpriteCut=cut;
		RefreshMap();
	}

	item->RefreshAnim();

	if((item->SprId!=old_spr_id || item->StartScrX!=old_ox || item->StartScrX!=old_oy) &&
		ProcessHexBorders(item->SprId,item->StartScrX,item->StartScrY))
	{
		// Resize field
		hVisible=VIEW_HEIGHT+hTop+hBottom;
		wVisible=VIEW_WIDTH+wLeft+wRight;
		delete[] viewField;
		viewField=new ViewField[hVisible*wVisible];
		RefreshMap();
	}
}

void HexManager::AffectCritter(MapObject* mobj, CritterCl* cr)
{
	if(mobj->MCritter.Cond<COND_LIFE || mobj->MCritter.Cond>COND_DEAD)
		mobj->MCritter.Cond=COND_LIFE;

	bool refresh=false;
	if(cr->Cond!=mobj->MCritter.Cond)
	{
		cr->Cond=mobj->MCritter.Cond;
		cr->Anim1Life=0;
		cr->Anim1Knockout=0;
		cr->Anim1Dead=0;
		cr->Anim2Life=0;
		cr->Anim2Knockout=0;
		cr->Anim2Dead=0;
		refresh=true;
	}

	DWORD& anim1=(cr->Cond==COND_LIFE?cr->Anim1Life:(cr->Cond==COND_KNOCKOUT?cr->Anim1Knockout:cr->Anim1Dead));
	DWORD& anim2=(cr->Cond==COND_LIFE?cr->Anim2Life:(cr->Cond==COND_KNOCKOUT?cr->Anim2Knockout:cr->Anim2Dead));
	if(anim1!=mobj->MCritter.Anim1 || anim2!=mobj->MCritter.Anim2) refresh=true;
	anim1=mobj->MCritter.Anim1;
	anim2=mobj->MCritter.Anim2;

	for(int i=0;i<MAPOBJ_CRITTER_PARAMS;i++)
	{
		if(mobj->MCritter.ParamIndex[i]>=0 && mobj->MCritter.ParamIndex[i]<MAX_PARAMS)
			cr->Params[mobj->MCritter.ParamIndex[i]]=mobj->MCritter.ParamValue[i];
	}

	if(refresh) cr->AnimateStay();
}

#endif // FONLINE_MAPPER