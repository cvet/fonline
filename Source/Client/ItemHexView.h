#pragma once

#include "Common.h"
#include "ItemView.h"
#include "SpriteManager.h"

class ResourceManager;
struct AnyFrames;

class ItemHexView: public ItemView
{
private:
    ItemHexView( uint id, ProtoItem* proto, ResourceManager& res_mngr );
    void AfterConstruction();

    ResourceManager& resMngr;

public:
    ItemHexView( uint id, ProtoItem* proto, Properties& props, ResourceManager& res_mngr );
    ItemHexView( uint id, ProtoItem* proto, UCharVecVec* props_data, ResourceManager& res_mngr );
    ItemHexView( uint id, ProtoItem* proto, UCharVecVec* props_data, int hx, int hy, int* hex_scr_x, int* hex_scr_y, ResourceManager& res_mngr );

public:
    uint       SprId;
    short      ScrX, ScrY;
    int*       HexScrX, * HexScrY;
    uchar      Alpha;
    AnyFrames* Anim;
    bool       SprDrawValid;
    Sprite*    SprDraw, * SprTemp;
    Effect*    DrawEffect;

private:
    int   curSpr, begSpr, endSpr;
    uint  animBegSpr, animEndSpr;
    uint  animTick;
    uchar maxAlpha;
    bool  isAnimated;
    uint  animNextTick;

public:
    bool IsAnimated()         { return isAnimated; }
    bool IsDrawContour()      { return /*IsFocused && */ !IsAnyScenery() && !GetIsNoHighlight() && !GetIsBadItem(); }
    bool IsTransparent()      { return maxAlpha < 0xFF; }
    bool IsFullyTransparent() { return maxAlpha == 0; }
    void RefreshAnim();
    void RestoreAlpha() { Alpha = maxAlpha; }
    void RefreshAlpha() { maxAlpha = ( IsColorize() ? GetAlpha() : 0xFF ); }
    void SetSprite( Sprite* spr );
    int  GetEggType();

    // Finish
private:
    bool finishing;
    uint finishingTime;

public:
    void Finish();
    bool IsFinishing() { return finishing; }
    bool IsFinish()    { return ( finishing && Timer::GameTick() > finishingTime ); }
    void StopFinishing();

    // Process
public:
    void Process();

    // Effect
private:
    bool  isEffect;
    float effSx, effSy;
    int   effStartX, effStartY;
    float effCurX, effCurY;
    uint  effDist;
    uint  effLastTick;
    int   effDir;

public:
    float EffOffsX, EffOffsY;

    bool       IsDynamicEffect() { return isEffect && ( effSx || effSy ); }
    void       SetEffect( float sx, float sy, uint dist, int dir );
    UShortPair GetEffectStep();

    // Fade
private:
    bool fading;
    uint fadingTick;
    bool fadeUp;

    void SetFade( bool fade_up );

public:
    void SkipFade();

    // Animation
public:
    void StartAnimate();
    void StopAnimate();
    void SetAnimFromEnd();
    void SetAnimFromStart();
    void SetAnim( uint beg, uint end );
    void SetSprStart();
    void SetSprEnd();
    void SetSpr( uint num_spr );
    void SetAnimOffs();
    void SetStayAnim();
    void SetShowAnim();
    void SetHideAnim();

public: // Move some specific types to end
    UShortPairVec EffSteps;
};
