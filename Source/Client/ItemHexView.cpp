#include "ItemHexView.h"
#include "EffectManager.h"
#include "GraphicStructures.h"
#include "Log.h"
#include "ResourceManager.h"
#include "SpriteManager.h"
#include "Sprites.h"
#include "Testing.h"
#include "Timer.h"

ItemHexView::ItemHexView(uint id, ProtoItem* proto, ResourceManager& res_mngr) : ItemView(id, proto), resMngr(res_mngr)
{
    const_cast<EntityType&>(Type) = EntityType::ItemHexView;

    // Hex
    HexScrX = nullptr;
    HexScrY = nullptr;

    // Offsets
    ScrX = 0;
    ScrY = 0;

    // Animation
    SprId = 0;
    curSpr = 0;
    begSpr = 0;
    endSpr = 0;
    animBegSpr = 0;
    animEndSpr = 0;
    isAnimated = false;
    animTick = 0;
    animNextTick = 0;
    SprDraw = nullptr;
    SprTemp = nullptr;
    SprDrawValid = false;

    // Alpha
    Alpha = 0;
    maxAlpha = 0xFF;

    // Finishing
    finishing = false;
    finishingTime = 0;

    // Fading
    fading = false;
    fadingTick = 0;
    fadeUp = false;

    // Effect
    isEffect = false;
    effSx = 0;
    effSy = 0;
    EffOffsX = 0;
    EffOffsY = 0;
    effStartX = 0;
    effStartY = 0;
    effDist = 0;
    effLastTick = 0;
    effCurX = 0;
    effCurY = 0;

    // Draw effect
    DrawEffect = resMngr.GetSpriteManager().GetEffectManager().Effects.Generic;
}

ItemHexView::ItemHexView(uint id, ProtoItem* proto, Properties& props, ResourceManager& res_mngr) :
    ItemHexView(id, proto, res_mngr)
{
    Props = props;
    AfterConstruction();
}

ItemHexView::ItemHexView(uint id, ProtoItem* proto, UCharVecVec* props_data, ResourceManager& res_mngr) :
    ItemHexView(id, proto, res_mngr)
{
    RUNTIME_ASSERT(props_data);
    Props.RestoreData(*props_data);
    AfterConstruction();
}

ItemHexView::ItemHexView(uint id, ProtoItem* proto, UCharVecVec* props_data, int hx, int hy, int* hex_scr_x,
    int* hex_scr_y, ResourceManager& res_mngr) :
    ItemHexView(id, proto, res_mngr)
{
    if (props_data)
        Props.RestoreData(*props_data);
    SetHexX(hx);
    SetHexY(hy);
    SetAccessory(ITEM_ACCESSORY_HEX);
    HexScrX = hex_scr_x;
    HexScrY = hex_scr_y;
    AfterConstruction();
}

void ItemHexView::AfterConstruction()
{
    RUNTIME_ASSERT(GetAccessory() == ITEM_ACCESSORY_HEX);

    // Refresh item
    RefreshAnim();
    RefreshAlpha();
    if (GetIsShowAnim())
        isAnimated = true;
    animNextTick = Timer::GameTick() + Random(GetAnimWaitRndMin() * 10, GetAnimWaitRndMax() * 10);
    SetFade(true);
}

void ItemHexView::Finish()
{
    SetFade(false);
    finishing = true;
    finishingTime = fadingTick;
    if (isEffect)
        finishingTime = Timer::GameTick();
}

bool ItemHexView::IsFinishing()
{
    return finishing;
}

bool ItemHexView::IsFinish()
{
    return finishing && Timer::GameTick() > finishingTime;
}

void ItemHexView::StopFinishing()
{
    finishing = false;
    SetFade(true);
    RefreshAnim();
}

void ItemHexView::Process()
{
    // Animation
    if (begSpr != endSpr)
    {
        int anim_proc = Procent(Anim->Ticks, Timer::GameTick() - animTick);
        if (anim_proc >= 100)
        {
            begSpr = animEndSpr;
            SetSpr(endSpr);
            animNextTick =
                Timer::GameTick() + GetAnimWaitBase() * 10 + Random(GetAnimWaitRndMin() * 10, GetAnimWaitRndMax() * 10);
        }
        else
        {
            int cur_spr = begSpr + ((endSpr - begSpr + (begSpr < endSpr ? 1 : -1)) * anim_proc) / 100;
            if (curSpr != cur_spr)
                SetSpr(cur_spr);
        }
    }
    else if (isEffect && !IsFinishing())
    {
        if (IsDynamicEffect())
            SetAnimFromStart();
        else
            Finish();
    }
    else if (IsAnimated() && Timer::GameTick() - animTick >= Anim->Ticks)
    {
        if (Timer::GameTick() >= animNextTick)
            SetStayAnim();
    }

    // Effect
    if (IsDynamicEffect() && !IsFinishing())
    {
        float dt = (float)(Timer::GameTick() - effLastTick);
        if (dt > 0)
        {
            float speed = GetFlyEffectSpeed();
            if (speed == 0)
                speed = 1.0f;

            EffOffsX += effSx * dt * speed;
            EffOffsY += effSy * dt * speed;
            effCurX += effSx * dt * speed;
            effCurY += effSy * dt * speed;

            SetAnimOffs();
            effLastTick = Timer::GameTick();
            if (DistSqrt((int)effCurX, (int)effCurY, effStartX, effStartY) >= effDist)
                Finish();
        }
    }

    // Fading
    if (fading)
    {
        int fading_proc = 100 - Procent(FADING_PERIOD, fadingTick - Timer::GameTick());
        fading_proc = CLAMP(fading_proc, 0, 100);
        if (fading_proc >= 100)
        {
            fading_proc = 100;
            fading = false;
        }

        Alpha = (fadeUp == true ? (fading_proc * 0xFF) / 100 : ((100 - fading_proc) * 0xFF) / 100);
        if (Alpha > maxAlpha)
            Alpha = maxAlpha;
    }
}

void ItemHexView::SetEffect(float sx, float sy, uint dist, int dir)
{
    // Init effect
    effSx = sx;
    effSy = sy;
    effDist = dist;
    effStartX = ScrX;
    effStartY = ScrY;
    effCurX = ScrX;
    effCurY = ScrY;
    effDir = dir;
    effLastTick = Timer::GameTick();
    isEffect = true;

    // Check off fade
    fading = false;
    Alpha = maxAlpha;

    // Refresh effect animation dir
    RefreshAnim();
}

UShortPair ItemHexView::GetEffectStep()
{
    uint dist = DistSqrt((int)effCurX, (int)effCurY, effStartX, effStartY);
    if (dist > effDist)
        dist = effDist;
    int proc = Procent(effDist, dist);
    if (proc > 99)
        proc = 99;
    return EffSteps[EffSteps.size() * proc / 100];
}

void ItemHexView::SetFade(bool fade_up)
{
    uint tick = Timer::GameTick();
    fadingTick = tick + FADING_PERIOD - (fadingTick > tick ? fadingTick - tick : 0);
    fadeUp = fade_up;
    fading = true;
}

void ItemHexView::SkipFade()
{
    if (fading)
    {
        fading = false;
        Alpha = (fadeUp ? maxAlpha : 0);
    }
}

void ItemHexView::RefreshAnim()
{
    Anim = nullptr;
    hash name_hash = GetPicMap();
    if (name_hash)
        Anim = resMngr.GetItemAnim(name_hash);
    if (name_hash && !Anim)
        WriteLog("PicMap for item '{}' not found.\n", GetName());
    if (Anim && isEffect)
        Anim = Anim->GetDir(effDir);
    if (!Anim)
        Anim = resMngr.ItemHexDefaultAnim;

    SetStayAnim();
    animBegSpr = begSpr;
    animEndSpr = endSpr;

    if (GetIsCanOpen())
    {
        if (GetOpened())
            SetSprEnd();
        else
            SetSprStart();
    }
}

void ItemHexView::SetSprite(Sprite* spr)
{
    if (spr)
        SprDraw = spr;
    if (SprDrawValid)
    {
        SprDraw->SetColor(IsColorize() ? GetColor() : 0);
        SprDraw->SetEgg(GetEggType());
        if (GetIsBadItem())
            SprDraw->SetContour(CONTOUR_RED);
    }
}

int ItemHexView::GetEggType()
{
    if (GetDisableEgg() || GetIsFlat())
        return 0;

    switch (GetCorner())
    {
    case CORNER_SOUTH:
        return EGG_X_OR_Y;
    case CORNER_NORTH:
        return EGG_X_AND_Y;
    case CORNER_EAST_WEST:
    case CORNER_WEST:
        return EGG_Y;
    default:
        return EGG_X; // CORNER_NORTH_SOUTH, CORNER_EAST
    }
    return 0;
}

void ItemHexView::StartAnimate()
{
    SetStayAnim();
    animNextTick =
        Timer::GameTick() + GetAnimWaitBase() * 10 + Random(GetAnimWaitRndMin() * 10, GetAnimWaitRndMax() * 10);
    isAnimated = true;
}

void ItemHexView::StopAnimate()
{
    SetSpr(animBegSpr);
    begSpr = animBegSpr;
    endSpr = animBegSpr;
    isAnimated = false;
}

void ItemHexView::SetAnimFromEnd()
{
    begSpr = animEndSpr;
    endSpr = animBegSpr;
    SetSpr(begSpr);
    animTick = Timer::GameTick();
}

void ItemHexView::SetAnimFromStart()
{
    begSpr = animBegSpr;
    endSpr = animEndSpr;
    SetSpr(begSpr);
    animTick = Timer::GameTick();
}

void ItemHexView::SetAnim(uint beg, uint end)
{
    if (beg > Anim->CntFrm - 1)
        beg = Anim->CntFrm - 1;
    if (end > Anim->CntFrm - 1)
        end = Anim->CntFrm - 1;
    begSpr = beg;
    endSpr = end;
    SetSpr(begSpr);
    animTick = Timer::GameTick();
}

void ItemHexView::SetSprStart()
{
    SetSpr(animBegSpr);
    begSpr = curSpr;
    endSpr = curSpr;
}

void ItemHexView::SetSprEnd()
{
    SetSpr(animEndSpr);
    begSpr = curSpr;
    endSpr = curSpr;
}

void ItemHexView::SetSpr(uint num_spr)
{
    curSpr = num_spr;
    SprId = Anim->GetSprId(curSpr);
    SetAnimOffs();
}

void ItemHexView::SetAnimOffs()
{
    ScrX = GetOffsetX();
    ScrY = GetOffsetY();
    for (int i = 1; i <= curSpr; i++)
    {
        ScrX += Anim->NextX[i];
        ScrY += Anim->NextY[i];
    }
    if (IsDynamicEffect())
    {
        ScrX += (int)EffOffsX;
        ScrY += (int)EffOffsY;
    }
}

void ItemHexView::SetStayAnim()
{
    if (GetIsShowAnimExt())
        SetAnim(GetAnimStay0(), GetAnimStay1());
    else
        SetAnim(0, Anim->CntFrm - 1);
}

void ItemHexView::SetShowAnim()
{
    if (GetIsShowAnimExt())
        SetAnim(GetAnimShow0(), GetAnimShow1());
    else
        SetAnim(0, Anim->CntFrm - 1);
}

void ItemHexView::SetHideAnim()
{
    if (GetIsShowAnimExt())
    {
        SetAnim(GetAnimHide0(), GetAnimHide1());
        animBegSpr = (GetAnimHide1());
        animEndSpr = (GetAnimHide1());
    }
    else
    {
        SetAnim(0, Anim->CntFrm - 1);
        animBegSpr = Anim->CntFrm - 1;
        animEndSpr = Anim->CntFrm - 1;
    }
}
