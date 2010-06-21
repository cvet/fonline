#ifndef __ITEM_HEX__
#define __ITEM_HEX__

#include "Defines.h"
#include "Item.h"
#include "SpriteManager.h"

#define EFFECT_0_TIME_PROC      (10)
#define EFFECT_0_SPEED_MUL      (10.0f)

struct AnyFrames;

class ItemHex : public Item
{
public:
	ItemHex(DWORD id, ProtoItem* pobj, Item::ItemData* data, int hx, int hy, int dir, short scr_x, short scr_y, int* hex_scr_x, int* hex_scr_y);
	bool operator==(const WORD& _right){return (GetProtoId()==_right);}

public:
	DWORD SprId;
	int HexX,HexY,Dir;
	short StartScrX,StartScrY;
	short ScrX,ScrY;
	int* HexScrX;
	int* HexScrY;
	BYTE Alpha;
	AnyFrames* Anim;
	static AnyFrames* DefaultAnim;
	BYTE ScenFlags;
	BYTE InfoOffset;
	bool SprDrawValid;
	Sprite* SprDraw;

private:
	short curSpr,begSpr,endSpr;
	short animBegSpr,animEndSpr;
	DWORD animTick;
	BYTE maxAlpha;
	bool isAnimated;
	DWORD animNextTick;

public:
	void RestoreAlpha(){Alpha=maxAlpha;}
	bool IsScenOrGrid(){return Proto->IsScen() || Proto->IsGrid();}
	bool IsItem(){return Proto->IsItem();}
	bool IsWall(){return Proto->IsWall();}
	WORD GetHexX(){return HexX;}
	WORD GetHexY(){return HexY;}
	DWORD GetPos(){return HEX_POS(HexX,HexY+Proto->DrawPosOffsY);}
	bool IsAnimated(){return isAnimated;}
	bool IsCanLook(){return !(Proto->IsGrid() && Proto->Grid.Type==GRID_EXITGRID);} // Proto->CanLook
	bool IsCanUse(){return IsItem() || (IsScenOrGrid() && FLAG(ScenFlags,SCEN_CAN_USE));}
	bool IsDrawContour(){return /*IsFocused && */IsItem() && !Proto->IsDoor() && !IsNoHighlight() && !IsBadItem();}
	bool IsTransparent(){int t=Proto->TransType; return t==TRANS_GLASS || t==TRANS_STEAM || t==TRANS_ENERGY || t==TRANS_RED;}
	void RefreshAnim();
	void SetEffects(Sprite* prep);
	Sprite::EggType GetEggType();

	// Finish
private:
	bool finishing;
	DWORD finishingTime;

public:
	void Finish();
	bool IsFinishing(){return finishing;}
	bool IsFinish(){return (finishing && Timer::FastTick()>finishingTime);}
	void StopFinishing();

	// Process
public:
	void Process();

	// Effect
private:
	bool isEffect;
	float effSx,effSy;
	int effStartX,effStartY;
	float effCurX,effCurY;
	int effDist;
	DWORD effLastTick;

public:
	float EffOffsX,EffOffsY;

	bool IsEffect(){return isEffect;}
	bool IsDynamicEffect(){return IsEffect() && (effSx || effSy);}
	void SetEffect(float sx, float sy, int dist);
	WordPair GetEffectStep();

	// Fade
private:
	bool fading;
	DWORD fadingTick;
	bool fadeUp;

	void SetFade(bool fade_up);

	// Animation
public:
	void StartAnimate();
	void StopAnimate();
	void SetAnimFromEnd();
	void SetAnimFromStart();
	void SetAnim(short beg, short end);
	void SetSprStart();
	void SetSprEnd();
	void SetSpr(short num_spr);
	void SetAnimOffs();
	void SetStayAnim();
	void SetShowAnim();
	void SetHideAnim();

public: // Move some specific types to end
	WordPairVec EffSteps;
};

typedef vector<ItemHex*> ItemHexVec;
typedef vector<ItemHex*>::iterator ItemHexVecIt;
typedef vector<ItemHex*>::value_type ItemHexVecVal;

#endif // __ITEM_HEX__