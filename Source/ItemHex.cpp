#include "StdAfx.h"
#include "ItemHex.h"
#include "Common.h"
#include "ResourceManager.h"

AnyFrames* ItemHex::DefaultAnim=NULL;

ItemHex::ItemHex(DWORD id, ProtoItem* pobj, Item::ItemData* data, int hx, int hy, int dir, short scr_x, short scr_y, int* hex_scr_x, int* hex_scr_y):
HexX(hx),HexY(hy),StartScrX(scr_x),StartScrY(scr_y),ScrX(0),ScrY(0),HexScrX(hex_scr_x),HexScrY(hex_scr_y),
curSpr(0),begSpr(0),endSpr(0),animBegSpr(0),animEndSpr(0),animTick(0),animNextTick(0),
Alpha(0),maxAlpha(0xFF),Dir(dir),
finishing(false),finishingTime(0),
fading(false),fadingTick(0),fadeUp(false),
isEffect(false),effSx(0),effSy(0),EffOffsX(0),EffOffsY(0),effStartX(0),effStartY(0),effDist(0),effLastTick(0),effCurX(0),effCurY(0),
isAnimated(false),ScenFlags(0)/*,IsFocused(false)*/,
SprDrawValid(false)
{
	Init(pobj);
	Id=id;
	if(data) Data=*data;

	RefreshAnim();
	RefreshAlpha();
	if(IsShowAnim()) isAnimated=true;
	animNextTick=Timer::GameTick()+Random(Proto->AnimWaitRndMin*10,Proto->AnimWaitRndMax*10);
	SetFade(true);
}

void ItemHex::Finish()
{
	SetFade(false);
	finishing=true;
	finishingTime=fadingTick;
	if(IsEffect()) finishingTime=Timer::GameTick();
}

void ItemHex::StopFinishing()
{
	finishing=false;
	SetFade(true);
	RefreshAnim();
}

void ItemHex::Process()
{
	// Animation
	if(begSpr!=endSpr)
	{
		int anim_proc=Procent(Anim->Ticks,Timer::GameTick()-animTick);
		if(anim_proc>=100)
		{
			/*if(animBegSpr!=curSpr)
			{
				endSpr=animBegSpr;
				SetSpr(animBegSpr);
				animNextTick=Timer::FastTick()+Data.AnimWaitBase*10+Random(Proto->AnimWaitRndMin*10,Proto->AnimWaitRndMax*10);
			}*/

			begSpr=animEndSpr;
			SetSpr(endSpr);
			animNextTick=Timer::GameTick()+Data.AnimWaitBase*10+Random(Proto->AnimWaitRndMin*10,Proto->AnimWaitRndMax*10);
		}
		else
		{
			short cur_spr=begSpr+((endSpr-begSpr+(begSpr<endSpr?1:-1))*anim_proc)/100;
			if(curSpr!=cur_spr) SetSpr(cur_spr);
		}
	}
	else if(IsEffect() && !IsFinishing())
	{
		if(IsDynamicEffect())
			SetAnimFromStart();
		else
			Finish();
	}
	else if(IsAnimated() && Timer::GameTick()-animTick>=Anim->Ticks)
	{
		if(Timer::GameTick()>=animNextTick) SetStayAnim();
	}

	// Effect
	if(IsDynamicEffect() && !IsFinishing())
	{
		if(Timer::GameTick()>=effLastTick+EFFECT_0_TIME_PROC)
		{
			EffOffsX+=effSx*EFFECT_0_SPEED_MUL;
			EffOffsY+=effSy*EFFECT_0_SPEED_MUL;
			effCurX+=effSx*EFFECT_0_SPEED_MUL;
			effCurY+=effSy*EFFECT_0_SPEED_MUL;
			SetAnimOffs();
			effLastTick=Timer::GameTick();
			if(DistSqrt(effCurX,effCurY,effStartX,effStartY)>=effDist) Finish();
		}
	}

	// Fading
	if(fading)
	{
		int fading_proc=100-Procent(FADING_PERIOD,fadingTick-Timer::GameTick());
		fading_proc=CLAMP(fading_proc,0,100);
		if(fading_proc>=100)
		{
			fading_proc=100;
			fading=false;
		}

		Alpha=(fadeUp==true?(fading_proc*0xFF)/100:((100-fading_proc)*0xFF)/100);
		if(Alpha>maxAlpha) Alpha=maxAlpha;
	}
}

void ItemHex::SetEffect(float sx, float sy, int dist)
{
	// Init effect
	effSx=sx;
	effSy=sy;
	effDist=dist;
	effStartX=ScrX;
	effStartY=ScrY;
	effCurX=ScrX;
	effCurY=ScrY;
	effLastTick=Timer::GameTick();
	isEffect=true;
	// Check off fade
	fading=false;
	Alpha=maxAlpha;
}

WordPair ItemHex::GetEffectStep()
{
	int dist=DistSqrt(effCurX,effCurY,effStartX,effStartY);
	if(dist>effDist) dist=effDist;
	int proc=Procent(effDist,dist);
	if(proc>99) proc=99;
	return EffSteps[EffSteps.size()*proc/100];
}

void ItemHex::SetFade(bool fade_up)
{
	int tick=Timer::GameTick();
	fadingTick=tick+FADING_PERIOD-(fadingTick>tick?fadingTick-tick:0);
	fadeUp=fade_up;
	fading=true;
}

void ItemHex::RefreshAnim()
{
	int dir=GetDir();
	DWORD name_hash=Proto->PicMapHash;
	if(Data.PicMapHash) name_hash=Data.PicMapHash;
	Anim=NULL;
	if(name_hash) Anim=ResMngr.GetAnim(name_hash,dir);
	if(name_hash && !Anim && dir) Anim=ResMngr.GetAnim(name_hash,0);
	if(!Anim) Anim=DefaultAnim;

	SetStayAnim();
	animBegSpr=begSpr;
	animEndSpr=endSpr;

	if(Proto->GetType()==ITEM_TYPE_CONTAINER || Proto->GetType()==ITEM_TYPE_DOOR)
	{
		if(FLAG(Data.Locker.Condition,LOCKER_ISOPEN)) SetSprEnd();
		else SetSprStart();
	}
}

void ItemHex::SetSprite(Sprite* spr)
{
	if(spr) SprDraw=spr;
	if(SprDrawValid)
	{
		SprDraw->Color=(IsColorize()?GetColor():0);
		SprDraw->Egg=GetEggType();
		if(IsBadItem()) SprDraw->Contour=Sprite::ContourRed;
	}
}

Sprite::EggType ItemHex::GetEggType()
{
	if(Proto->DisableEgg || IsFlat()) return Sprite::EggNone;
	switch(Proto->Corner)
	{
	case CORNER_SOUTH: return Sprite::EggXorY;
	case CORNER_NORTH: return Sprite::EggXandY;
	case CORNER_EAST_WEST:
	case CORNER_WEST: return Sprite::EggY;
	default: return Sprite::EggX; // CORNER_NORTH_SOUTH, CORNER_EAST
	}
	return Sprite::EggNone;
}

void ItemHex::StartAnimate()
{
	SetStayAnim();
	animNextTick=Timer::GameTick()+Data.AnimWaitBase*10+Random(Proto->AnimWaitRndMin*10,Proto->AnimWaitRndMax*10);
	isAnimated=true;
}

void ItemHex::StopAnimate()
{
	SetSpr(animBegSpr);
	begSpr=animBegSpr;
	endSpr=animBegSpr;
	isAnimated=false;
}

void ItemHex::SetAnimFromEnd()
{
	begSpr=animEndSpr;
	endSpr=animBegSpr;
	SetSpr(begSpr);
	animTick=Timer::GameTick();
}

void ItemHex::SetAnimFromStart()
{
	begSpr=animBegSpr;
	endSpr=animEndSpr;
	SetSpr(begSpr);
	animTick=Timer::GameTick();
}

void ItemHex::SetAnim(short beg, short end)
{
	if(beg>Anim->CntFrm-1) beg=Anim->CntFrm-1;
	if(end>Anim->CntFrm-1) end=Anim->CntFrm-1;
	begSpr=beg;
	endSpr=end;
	SetSpr(begSpr);
	animTick=Timer::GameTick();
}

void ItemHex::SetSprStart()
{
	SetSpr(animBegSpr);
	begSpr=curSpr;
	endSpr=curSpr;
}

void ItemHex::SetSprEnd()
{
	SetSpr(animEndSpr);
	begSpr=curSpr;
	endSpr=curSpr;
}
void ItemHex::SetSpr(short num_spr)
{
	curSpr=num_spr;
	SprId=Anim->GetSprId(curSpr);
	SetAnimOffs();
}

void ItemHex::SetAnimOffs()
{
	ScrX=StartScrX;
	ScrY=StartScrY;
	for(int i=1;i<=curSpr;i++)
	{
		ScrX+=Anim->NextX[i];
		ScrY+=Anim->NextY[i];
	}
	if(IsDynamicEffect())
	{
		ScrX+=(int)EffOffsX;
		ScrY+=(int)EffOffsY;
	}
}

void ItemHex::SetStayAnim()
{
	if(IsShowAnimExt())
		SetAnim(Data.AnimStay[0],Data.AnimStay[1]);
	else
		SetAnim(0,Anim->CntFrm-1);
}

void ItemHex::SetShowAnim()
{
	if(IsShowAnimExt())
		SetAnim(Data.AnimShow[0],Data.AnimShow[1]);
	else
		SetAnim(0,Anim->CntFrm-1);
}

void ItemHex::SetHideAnim()
{
	if(IsShowAnimExt())
	{
		SetAnim(Data.AnimHide[0],Data.AnimHide[1]);
		animBegSpr=(Data.AnimHide[1]);
		animEndSpr=(Data.AnimHide[1]);
	}
	else
	{
		SetAnim(0,Anim->CntFrm-1);
		animBegSpr=Anim->CntFrm-1;
		animEndSpr=Anim->CntFrm-1;
	}
}