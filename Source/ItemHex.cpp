#include "Common.h"
#include "ItemHex.h"
#include "ResourceManager.h"

ItemHex::ItemHex( uint id, ProtoItem* proto ): Item( id, proto )
{
    const_cast< EntityType& >( Type ) = EntityType::ItemHex;

    // Hex
    HexX = 0;
    HexY = 0;
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
    DrawEffect = Effect::Generic;
}

ItemHex::ItemHex( uint id, ProtoItem* proto, Properties& props ): ItemHex( id, proto )
{
    Props = props;
    AfterConstruction();
}

ItemHex::ItemHex( uint id, ProtoItem* proto, UCharVecVec* props_data ): ItemHex( id, proto )
{
    RUNTIME_ASSERT( props_data );
    Props.RestoreData( *props_data );
    AfterConstruction();
}

ItemHex::ItemHex( uint id, ProtoItem* proto, UCharVecVec* props_data, int hx, int hy, int* hex_scr_x, int* hex_scr_y ): ItemHex( id, proto )
{
    if( props_data )
        Props.RestoreData( *props_data );
    SetHexX( hx );
    SetHexY( hy );
    SetAccessory( ITEM_ACCESSORY_HEX );
    HexScrX = hex_scr_x;
    HexScrY = hex_scr_y;
    AfterConstruction();
}

void ItemHex::AfterConstruction()
{
    RUNTIME_ASSERT( GetAccessory() == ITEM_ACCESSORY_HEX );
    HexX = GetHexX();
    HexY = GetHexY();

    // Refresh item
    RefreshAnim();
    RefreshAlpha();
    if( GetIsShowAnim() )
        isAnimated = true;
    animNextTick = Timer::GameTick() + Random( GetAnimWaitRndMin() * 10, GetAnimWaitRndMax() * 10 );
    SetFade( true );
}

void ItemHex::Finish()
{
    SetFade( false );
    finishing = true;
    finishingTime = fadingTick;
    if( isEffect )
        finishingTime = Timer::GameTick();
}

void ItemHex::StopFinishing()
{
    finishing = false;
    SetFade( true );
    RefreshAnim();
}

void ItemHex::Process()
{
    // Animation
    if( begSpr != endSpr )
    {
        int anim_proc = Procent( Anim->Ticks, Timer::GameTick() - animTick );
        if( anim_proc >= 100 )
        {
            begSpr = animEndSpr;
            SetSpr( endSpr );
            animNextTick = Timer::GameTick() + GetAnimWaitBase() * 10 + Random( GetAnimWaitRndMin() * 10, GetAnimWaitRndMax() * 10 );
        }
        else
        {
            int cur_spr = begSpr + ( ( endSpr - begSpr + ( begSpr < endSpr ? 1 : -1 ) ) * anim_proc ) / 100;
            if( curSpr != cur_spr )
                SetSpr( cur_spr );
        }
    }
    else if( isEffect && !IsFinishing() )
    {
        if( IsDynamicEffect() )
            SetAnimFromStart();
        else
            Finish();
    }
    else if( IsAnimated() && Timer::GameTick() - animTick >= Anim->Ticks )
    {
        if( Timer::GameTick() >= animNextTick )
            SetStayAnim();
    }

    // Effect
    if( IsDynamicEffect() && !IsFinishing() )
    {
        if( Timer::GameTick() >= effLastTick + EFFECT_0_TIME_PROC )
        {
            EffOffsX += effSx * EFFECT_0_SPEED_MUL;
            EffOffsY += effSy * EFFECT_0_SPEED_MUL;
            effCurX += effSx * EFFECT_0_SPEED_MUL;
            effCurY += effSy * EFFECT_0_SPEED_MUL;
            SetAnimOffs();
            effLastTick = Timer::GameTick();
            if( DistSqrt( (int) effCurX, (int) effCurY, effStartX, effStartY ) >= effDist )
                Finish();
        }
    }

    // Fading
    if( fading )
    {
        int fading_proc = 100 - Procent( FADING_PERIOD, fadingTick - Timer::GameTick() );
        fading_proc = CLAMP( fading_proc, 0, 100 );
        if( fading_proc >= 100 )
        {
            fading_proc = 100;
            fading = false;
        }

        Alpha = ( fadeUp == true ? ( fading_proc * 0xFF ) / 100 : ( ( 100 - fading_proc ) * 0xFF ) / 100 );
        if( Alpha > maxAlpha )
            Alpha = maxAlpha;
    }
}

void ItemHex::SetEffect( float sx, float sy, uint dist, int dir )
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

UShortPair ItemHex::GetEffectStep()
{
    uint dist = DistSqrt( (int) effCurX, (int) effCurY, effStartX, effStartY );
    if( dist > effDist )
        dist = effDist;
    int proc = Procent( effDist, dist );
    if( proc > 99 )
        proc = 99;
    return EffSteps[ EffSteps.size() * proc / 100 ];
}

void ItemHex::SetFade( bool fade_up )
{
    uint tick = Timer::GameTick();
    fadingTick = tick + FADING_PERIOD - ( fadingTick > tick ? fadingTick - tick : 0 );
    fadeUp = fade_up;
    fading = true;
}

void ItemHex::RefreshAnim()
{
    Anim = nullptr;
    hash name_hash = GetPicMap();
    if( name_hash )
        Anim = ResMngr.GetItemAnim( name_hash );
    if( name_hash && !Anim )
        WriteLog( "PicMap for item '{}' not found.\n", GetName() );
    if( Anim && isEffect )
        Anim = Anim->GetDir( effDir );
    if( !Anim )
        Anim = ResMngr.ItemHexDefaultAnim;

    SetStayAnim();
    animBegSpr = begSpr;
    animEndSpr = endSpr;

    if( GetType() == ITEM_TYPE_CONTAINER || GetType() == ITEM_TYPE_DOOR )
    {
        if( GetOpened() )
            SetSprEnd();
        else
            SetSprStart();
    }
}

void ItemHex::SetSprite( Sprite* spr )
{
    if( spr )
        SprDraw = spr;
    if( SprDrawValid )
    {
        SprDraw->SetColor( IsColorize() ? GetColor() : 0 );
        SprDraw->SetEgg( GetEggType() );
        if( GetIsBadItem() )
            SprDraw->SetContour( CONTOUR_RED );
    }
}

int ItemHex::GetEggType()
{
    if( GetDisableEgg() || GetIsFlat() )
        return 0;

    switch( GetCorner() )
    {
    case CORNER_SOUTH:
        return EGG_X_OR_Y;
    case CORNER_NORTH:
        return EGG_X_AND_Y;
    case CORNER_EAST_WEST:
    case CORNER_WEST:
        return EGG_Y;
    default:
        return EGG_X;          // CORNER_NORTH_SOUTH, CORNER_EAST
    }
    return 0;
}

void ItemHex::StartAnimate()
{
    SetStayAnim();
    animNextTick = Timer::GameTick() + GetAnimWaitBase() * 10 + Random( GetAnimWaitRndMin() * 10, GetAnimWaitRndMax() * 10 );
    isAnimated = true;
}

void ItemHex::StopAnimate()
{
    SetSpr( animBegSpr );
    begSpr = animBegSpr;
    endSpr = animBegSpr;
    isAnimated = false;
}

void ItemHex::SetAnimFromEnd()
{
    begSpr = animEndSpr;
    endSpr = animBegSpr;
    SetSpr( begSpr );
    animTick = Timer::GameTick();
}

void ItemHex::SetAnimFromStart()
{
    begSpr = animBegSpr;
    endSpr = animEndSpr;
    SetSpr( begSpr );
    animTick = Timer::GameTick();
}

void ItemHex::SetAnim( uint beg, uint end )
{
    if( beg > Anim->CntFrm - 1 )
        beg = Anim->CntFrm - 1;
    if( end > Anim->CntFrm - 1 )
        end = Anim->CntFrm - 1;
    begSpr = beg;
    endSpr = end;
    SetSpr( begSpr );
    animTick = Timer::GameTick();
}

void ItemHex::SetSprStart()
{
    SetSpr( animBegSpr );
    begSpr = curSpr;
    endSpr = curSpr;
}

void ItemHex::SetSprEnd()
{
    SetSpr( animEndSpr );
    begSpr = curSpr;
    endSpr = curSpr;
}

void ItemHex::SetSpr( uint num_spr )
{
    curSpr = num_spr;
    SprId = Anim->GetSprId( curSpr );
    SetAnimOffs();
}

void ItemHex::SetAnimOffs()
{
    ScrX = GetOffsetX();
    ScrY = GetOffsetY();
    for( int i = 1; i <= curSpr; i++ )
    {
        ScrX += Anim->NextX[ i ];
        ScrY += Anim->NextY[ i ];
    }
    if( IsDynamicEffect() )
    {
        ScrX += (int) EffOffsX;
        ScrY += (int) EffOffsY;
    }
}

void ItemHex::SetStayAnim()
{
    if( GetIsShowAnimExt() )
        SetAnim( GetAnimStay_0(), GetAnimStay_1() );
    else
        SetAnim( 0, Anim->CntFrm - 1 );
}

void ItemHex::SetShowAnim()
{
    if( GetIsShowAnimExt() )
        SetAnim( GetAnimShow_0(), GetAnimShow_1() );
    else
        SetAnim( 0, Anim->CntFrm - 1 );
}

void ItemHex::SetHideAnim()
{
    if( GetIsShowAnimExt() )
    {
        SetAnim( GetAnimHide_0(), GetAnimHide_1() );
        animBegSpr = ( GetAnimHide_1() );
        animEndSpr = ( GetAnimHide_1() );
    }
    else
    {
        SetAnim( 0, Anim->CntFrm - 1 );
        animBegSpr = Anim->CntFrm - 1;
        animEndSpr = Anim->CntFrm - 1;
    }
}
