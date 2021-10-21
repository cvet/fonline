#include "Common.h"
#include "CritterCl.h"
#include "ResourceManager.h"
#include "ProtoManager.h"

#ifdef FONLINE_CLIENT
# include "Client.h"
# include "SoundManager.h"
# include "Script.h"
#endif

ProtoCritter::ProtoCritter( hash pid, uint subType): ProtoEntity( pid, CritterCl::GetPropertyRegistrator(subType)) {}
CLASS_PROPERTY_ALIAS_IMPL( ProtoCritter, CritterCl, uint, Multihex );

bool CritterCl::SlotEnabled[ 0x100 ];

PROPERTIES_IMPL( CritterCl );
CLASS_PROPERTY_IMPL( CritterCl, HexX );
CLASS_PROPERTY_IMPL( CritterCl, HexY );
CLASS_PROPERTY_IMPL( CritterCl, Dir );
CLASS_PROPERTY_IMPL( CritterCl, Cond );
CLASS_PROPERTY_IMPL( CritterCl, Multihex );
CLASS_PROPERTY_IMPL( CritterCl, Anim1Life );
CLASS_PROPERTY_IMPL( CritterCl, Anim1Knockout );
CLASS_PROPERTY_IMPL( CritterCl, Anim1Dead );
CLASS_PROPERTY_IMPL( CritterCl, Anim2Life );
CLASS_PROPERTY_IMPL( CritterCl, Anim2Knockout );
CLASS_PROPERTY_IMPL( CritterCl, Anim2Dead );
CLASS_PROPERTY_IMPL( CritterCl, ModelName );
CLASS_PROPERTY_IMPL( CritterCl, ScriptId );
CLASS_PROPERTY_IMPL( CritterCl, LookDistance );
CLASS_PROPERTY_IMPL( CritterCl, Anim3dLayer );
CLASS_PROPERTY_IMPL( CritterCl, Gender );
CLASS_PROPERTY_IMPL( CritterCl, DialogId );
CLASS_PROPERTY_IMPL( CritterCl, WorldX );
CLASS_PROPERTY_IMPL( CritterCl, WorldY );
CLASS_PROPERTY_IMPL( CritterCl, GlobalMapLeaderId );
CLASS_PROPERTY_IMPL( CritterCl, TalkDistance );
CLASS_PROPERTY_IMPL( CritterCl, CurrentHp );
CLASS_PROPERTY_IMPL( CritterCl, WalkTime );
CLASS_PROPERTY_IMPL( CritterCl, RunTime );
CLASS_PROPERTY_IMPL( CritterCl, ScaleFactor );
CLASS_PROPERTY_IMPL( CritterCl, TimeoutBattle );
CLASS_PROPERTY_IMPL( CritterCl, TimeoutTransfer );
CLASS_PROPERTY_IMPL( CritterCl, TimeoutRemoveFromGame );
CLASS_PROPERTY_IMPL( CritterCl, IsNoWalk );
CLASS_PROPERTY_IMPL( CritterCl, IsNoRun );
CLASS_PROPERTY_IMPL( CritterCl, IsNoRotate );
CLASS_PROPERTY_IMPL( CritterCl, IsNoTalk );
CLASS_PROPERTY_IMPL( CritterCl, IsHide );
CLASS_PROPERTY_IMPL( CritterCl, IsNoFlatten );

CritterCl::CritterCl( uint id, ProtoCritter* proto ): Entity( id, EntityType::CritterCl, PropertiesRegistrators[0], proto )
{
    SprId = 0;
    NameColor = 0;
    ContourColor = 0;
    Flags = 0;
    curSpr = 0;
    lastEndSpr = 0;
    animStartTick = 0;
    SprOx = SprOy = 0;
    StartTick = 0;
    TickCount = 0;
    tickTextDelay = 0;
    textOnHeadColor = COLOR_TEXT;
    Alpha = 0;
    fadingEnable = false;
    FadingTick = 0;
    fadeUp = false;
    finishingTime = 0;
    staySprDir = 0;
    staySprTick = 0;
    needReSet = false;
    reSetTick = 0;
    CurMoveStep = 0;
    Visible = true;
    SprDrawValid = false;
    OxExtI = OyExtI = 0;
    OxExtF = OyExtF = 0.0f;
    OxExtSpeed = OyExtSpeed = 0;
    OffsExtNextTick = 0;
    Anim3d = Anim3dStay = nullptr;
    Name = "";
    NameOnHead = "";
    Avatar = "";
    tickFidget = Timer::GameTick() + Random( GameOpt.CritterFidgetTime, GameOpt.CritterFidgetTime * 2 );
    memzero( &stayAnim, sizeof( stayAnim ) );
    DrawEffect = Effect::Critter;
    memzero( anim3dLayers, sizeof( anim3dLayers ) );
    textOnHeadColor = COLOR_CRITTER_NAME;

    #ifdef FONLINE_CLIENT
    CScriptArray* arr = Script::CreateArray( "int[]" );
    arr->Resize( LAYERS3D_COUNT );
    SetAnim3dLayer( arr );
    arr->Release();
    #endif
}

CritterCl::~CritterCl()
{
    SprMngr.FreePure3dAnimation( Anim3d );
    SprMngr.FreePure3dAnimation( Anim3dStay );
    Anim3d = Anim3dStay = nullptr;
}

void CritterCl::Init()
{
    RefreshAnim();
    AnimateStay();

    SpriteInfo* si = SprMngr.GetSpriteInfo( SprId );
    if( si )
        textRect( 0, 0, si->Width, si->Height );

    SetFade( true );
}

void CritterCl::Finish()
{
    SetFade( false );
    finishingTime = FadingTick;
}

void CritterCl::SetFade( bool fade_up )
{
    uint tick = Timer::GameTick();
    FadingTick = tick + FADING_PERIOD - ( FadingTick > tick ? FadingTick - tick : 0 );
    fadeUp = fade_up;
    fadingEnable = true;
}

uchar CritterCl::GetFadeAlpha()
{
    uint tick = Timer::GameTick();
    int  fading_proc = 100 - Procent( FADING_PERIOD, FadingTick > tick ? FadingTick - tick : 0 );
    fading_proc = CLAMP( fading_proc, 0, 100 );
    if( fading_proc >= 100 )
    {
        fading_proc = 100;
        fadingEnable = false;
    }

    return fadeUp == true ? ( fading_proc * 0xFF ) / 100 : ( ( 100 - fading_proc ) * 0xFF ) / 100;
}

void CritterCl::AddItem( Item* item )
{
    item->SetAccessory( ITEM_ACCESSORY_CRITTER );
    item->SetCritId( Id );

    InvItems.push_back( item );
    Item::SortItems( InvItems );

    if( item->GetCritSlot() && !IsAnim() )
        AnimateStay();
}

void CritterCl::DeleteItem( Item* item, bool animate )
{
    item->SetAccessory( ITEM_ACCESSORY_NONE );
    item->SetCritId( 0 );
    item->SetCritSlot( 0 );

    auto it = std::find( InvItems.begin(), InvItems.end(), item );
    RUNTIME_ASSERT( it != InvItems.end() );
    InvItems.erase( it );

    item->IsDestroyed = true;
    #ifdef FONLINE_CLIENT
    Script::RemoveEventsEntity( item );
    #endif
    item->Release();

    if( animate && !IsAnim() )
        AnimateStay();
}

void CritterCl::DeleteAllItems()
{
    while( !InvItems.empty() )
        DeleteItem( *InvItems.begin(), false );
}

Item* CritterCl::GetItem( uint item_id )
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetId() == item_id )
            return item;
    }
    return nullptr;
}

Item* CritterCl::GetItemByPid( hash item_pid )
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        if( ( *it )->GetProtoId() == item_pid )
            return *it;
    return nullptr;
}

Item* CritterCl::GetItemByPidInvPriority( hash item_pid )
{
    ProtoItem* proto_item = ProtoMngr.GetProtoItem( item_pid );
    if( !proto_item )
        return nullptr;

    if( proto_item->GetStackable() )
    {
        for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        {
            Item* item = *it;
            if( item->GetProtoId() == item_pid )
                return item;
        }
    }
    else
    {
        Item* another_slot = nullptr;
        for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        {
            Item* item = *it;
            if( item->GetProtoId() == item_pid )
            {
                if( !item->GetCritSlot() )
                    return item;
                another_slot = item;
            }
        }
        return another_slot;
    }
    return nullptr;
}

Item* CritterCl::GetItemByPidSlot( hash item_pid, int slot )
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == item_pid && item->GetCritSlot() == slot )
            return item;
    }
    return nullptr;
}

Item* CritterCl::GetItemSlot( int slot )
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        if( ( *it )->GetCritSlot() == slot )
            return *it;
    return nullptr;
}

void CritterCl::GetItemsSlot( int slot, ItemVec& items )
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( slot == -1 || item->GetCritSlot() == slot )
            items.push_back( item );
    }
}

uint CritterCl::CountItemPid( hash item_pid )
{
    uint result = 0;
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
        if( ( *it )->GetProtoId() == item_pid )
            result += ( *it )->GetCount();
    return result;
}

bool CritterCl::IsCombatMode()
{
    return IS_TIMEOUT( GetTimeoutBattle() );
}

bool CritterCl::CheckFind( int find_type )
{
    if( IsNpc() )
    {
        if( FLAG( find_type, FIND_ONLY_PLAYERS ) )
            return false;
    }
    else
    {
        if( FLAG( find_type, FIND_ONLY_NPC ) )
            return false;
    }
    return FLAG( find_type, FIND_ALL ) ||
           ( IsLife() && FLAG( find_type, FIND_LIFE ) ) ||
           ( IsKnockout() && FLAG( find_type, FIND_KO ) ) ||
           ( IsDead() && FLAG( find_type, FIND_DEAD ) );
}

uint CritterCl::GetAttackDist()
{
    uint dist = 0;
    #ifdef FONLINE_CLIENT
    Script::RaiseInternalEvent( ClientFunctions.CritterGetAttackDistantion, this, nullptr, 0, &dist );
    #endif
    return dist;
}

void CritterCl::DrawStay( Rect r )
{
    if( Timer::FastTick() - staySprTick > 500 )
    {
        staySprDir++;
        if( staySprDir >= DIRS_COUNT )
            staySprDir = 0;
        staySprTick = Timer::FastTick();
    }

    int  dir = ( !IsLife() ? GetDir() : staySprDir );
    uint anim1 = GetAnim1();
    uint anim2 = GetAnim2();

    if( !Anim3d )
    {
        AnyFrames* anim = ResMngr.GetCrit2dAnim( GetModelName(), anim1, anim2, dir, this );
        if( anim )
        {
            uint spr_id = ( IsLife() ? anim->Ind[ 0 ] : anim->Ind[ anim->CntFrm - 1 ] );
            SprMngr.DrawSpriteSize( spr_id, r.L, r.T, r.W(), r.H(), false, true );
        }
    }
    else if( Anim3dStay )
    {
        Anim3dStay->SetDir( dir );
        Anim3dStay->SetAnimation( anim1, anim2, GetLayers3dData(), ANIMATION_STAY | ANIMATION_PERIOD( 100 ) | ANIMATION_NO_SMOOTH );
        SprMngr.Draw3d( r.CX(), r.B, Anim3dStay, COLOR_IFACE );
    }
}

bool CritterCl::IsLastHexes()
{
    return !LastHexX.empty() && !LastHexY.empty();
}

void CritterCl::FixLastHexes()
{
    if( IsLastHexes() && LastHexX[ LastHexX.size() - 1 ] == GetHexX() && LastHexY[ LastHexY.size() - 1 ] == GetHexY() )
        return;
    LastHexX.push_back( GetHexX() );
    LastHexY.push_back( GetHexY() );
}

ushort CritterCl::PopLastHexX()
{
    ushort hx = LastHexX[ LastHexX.size() - 1 ];
    LastHexX.pop_back();
    return hx;
}

ushort CritterCl::PopLastHexY()
{
    ushort hy = LastHexY[ LastHexY.size() - 1 ];
    LastHexY.pop_back();
    return hy;
}

void CritterCl::Move( int dir )
{
    if( dir < 0 || dir >= DIRS_COUNT || GetIsNoRotate() )
        dir = 0;

    SetDir( dir );

    uint time_move = ( IsRunning ? GetRunTime() : GetWalkTime() );

    TickStart( time_move );
    animStartTick = Timer::GameTick();

    if( !Anim3d )
    {
        if( _str().parseHash( GetModelName() ).startsWith( "art/critters/" ) )
        {
            uint       anim1 = ( IsRunning ? ANIM1_UNARMED : GetAnim1() );
            uint       anim2 = ( IsRunning ? ANIM2_RUN : ANIM2_WALK );
            AnyFrames* anim = ResMngr.GetCrit2dAnim( GetModelName(), anim1, anim2, dir, this );
            if( !anim )
                anim = ResMngr.CritterDefaultAnim;

            int step, beg_spr, end_spr;
            curSpr = lastEndSpr;

            if( !IsRunning )
            {
                int s0 = 4;
                int s1 = 8;
                int s2 = 0;
                int s3 = 0;

                if( (int) curSpr == s0 - 1 && s1 )
                {
                    beg_spr = s0;
                    end_spr = s1 - 1;
                    step = 2;
                }
                else if( (int) curSpr == s1 - 1 && s2 )
                {
                    beg_spr = s1;
                    end_spr = s2 - 1;
                    step = 3;
                }
                else if( (int) curSpr == s2 - 1 && s3 )
                {
                    beg_spr = s2;
                    end_spr = s3 - 1;
                    step = 4;
                }
                else
                {
                    beg_spr = 0;
                    end_spr = s0 - 1;
                    step = 1;
                }
            }
            else
            {
                switch( curSpr )
                {
                default:
                case 0:
                    beg_spr = 0;
                    end_spr = 1;
                    step = 1;
                    break;
                case 1:
                    beg_spr = 2;
                    end_spr = 3;
                    step = 2;
                    break;
                case 3:
                    beg_spr = 4;
                    end_spr = 6;
                    step = 3;
                    break;
                case 6:
                    beg_spr = 7;
                    end_spr = anim->CntFrm - 1;
                    step = 4;
                    break;
                }
            }

            ClearAnim();
            animSequence.push_back( CritterAnim( anim, time_move, beg_spr, end_spr, true, 0, anim1, anim2, nullptr ) );
            NextAnim( false );

            for( int i = 0; i < step; i++ )
            {
                int ox, oy;
                GetWalkHexOffsets( dir, ox, oy );
                ChangeOffs( ox, oy, true );
            }
        }
        else
        {
            uint anim1 = GetAnim1();
            uint anim2 = ( IsRunning ? ANIM2_RUN : ANIM2_WALK );
            if( GetIsHide() )
                anim2 = ( IsRunning ? ANIM2_SNEAK_RUN : ANIM2_SNEAK_WALK );

            AnyFrames* anim = ResMngr.GetCrit2dAnim( GetModelName(), anim1, anim2, dir, this );
            if( !anim )
                anim = ResMngr.CritterDefaultAnim;

            int m1 = 0;
            if( m1 <= 0 )
                m1 = 5;
            int m2 = 0;
            if( m2 <= 0 )
                m2 = 2;

            int beg_spr = lastEndSpr + 1;
            int end_spr = beg_spr + ( IsRunning ? m2 : m1 );

            ClearAnim();
            animSequence.push_back( CritterAnim( anim, time_move, beg_spr, end_spr, true, dir + 1, anim1, anim2, nullptr ) );
            NextAnim( false );

            int ox, oy;
            GetWalkHexOffsets( dir, ox, oy );
            ChangeOffs( ox, oy, true );
        }
    }
    else
    {
        uint anim1 = GetAnim1();
        uint anim2 = ( IsRunning ? ANIM2_RUN : ANIM2_WALK );
        if( GetIsHide() )
            anim2 = ( IsRunning ? ANIM2_SNEAK_RUN : ANIM2_SNEAK_WALK );

        Anim3d->CheckAnimation( anim1, anim2 );
        Anim3d->SetDir( dir );

        ClearAnim();
        animSequence.push_back( CritterAnim( nullptr, time_move, 0, 0, true, dir + 1, anim1, anim2, nullptr ) );
        NextAnim( false );

        int ox, oy;
        GetWalkHexOffsets( dir, ox, oy );
        ChangeOffs( ox, oy, true );
    }
}

void CritterCl::Action( int action, int action_ext, Item* item, bool local_call /* = true */ )
{
    #ifdef FONLINE_CLIENT
    Script::RaiseInternalEvent( ClientFunctions.CritterAction, local_call, this, action, action_ext, item );
    #endif

    switch( action )
    {
    case ACTION_KNOCKOUT:
        SetCond( COND_KNOCKOUT );
        SetAnim2Knockout( action_ext );
        break;
    case ACTION_STANDUP:
        SetCond( COND_LIFE );
        break;
    case ACTION_DEAD:
    {
        SetCond( COND_DEAD );
        SetAnim2Dead( action_ext );
        CritterAnim* anim = GetCurAnim();
        needReSet = true;
        reSetTick = Timer::GameTick() + ( anim && anim->Anim ? anim->Anim->Ticks : 1000 );
    }
    break;
    case ACTION_CONNECT:
        UNSETFLAG( Flags, FCRIT_DISCONNECT );
        break;
    case ACTION_DISCONNECT:
        SETFLAG( Flags, FCRIT_DISCONNECT );
        break;
    case ACTION_RESPAWN:
        SetCond( COND_LIFE );
        Alpha = 0;
        SetFade( true );
        AnimateStay();
        needReSet = true;
        reSetTick = Timer::GameTick();       // Fast
        break;
    case ACTION_REFRESH:
        if( Anim3d )
            Anim3d->StartMeshGeneration();
        break;
    default:
        break;
    }

    if( !IsAnim() )
        AnimateStay();
}

void CritterCl::NextAnim( bool erase_front )
{
    if( animSequence.empty() )
        return;
    if( erase_front )
    {
        SAFEREL( ( *animSequence.begin() ).ActiveItem );
        animSequence.erase( animSequence.begin() );
    }
    if( animSequence.empty() )
        return;

    CritterAnim& cr_anim = animSequence[ 0 ];
    animStartTick = Timer::GameTick();

    ProcessAnim( false, !Anim3d, cr_anim.IndAnim1, cr_anim.IndAnim2, cr_anim.ActiveItem );

    if( !Anim3d )
    {
        lastEndSpr = cr_anim.EndFrm;
        curSpr = cr_anim.BeginFrm;
		RUNTIME_ASSERT(cr_anim.Anim);
        SprId = cr_anim.Anim->GetSprId( curSpr );

        short ox = 0, oy = 0;
        for( int i = 0, j = curSpr % cr_anim.Anim->GetCnt(); i <= j; i++ )
        {
            ox += cr_anim.Anim->NextX[ i ];
            oy += cr_anim.Anim->NextY[ i ];
        }
        SetOffs( ox, oy, cr_anim.MoveText );
    }
    else
    {
        SetOffs( 0, 0, cr_anim.MoveText );
        Anim3d->SetAnimation( cr_anim.IndAnim1, cr_anim.IndAnim2, GetLayers3dData(), ( cr_anim.DirOffs ? 0 : ANIMATION_ONE_TIME ) | ( IsCombatMode() ? ANIMATION_COMBAT : 0 ) );
    }
}

void CritterCl::Animate( uint anim1, uint anim2, Item* item )
{
    uchar dir = GetDir();
    if( !anim1 )
        anim1 = GetAnim1();
    if( item )
        item = item->Clone();

    if( !Anim3d )
    {
        AnyFrames* anim = ResMngr.GetCrit2dAnim( GetModelName(), anim1, anim2, dir, this );
        if( !anim )
        {
            if( !IsAnim() )
                AnimateStay();
            return;
        }

        #pragma MESSAGE( "Migrate critter on head text moving in scripts." )
        bool move_text = true;
//			(Cond==COND_DEAD || Cond==COND_KNOCKOUT ||
//			(anim2!=ANIM2_SHOW_WEAPON && anim2!=ANIM2_HIDE_WEAPON && anim2!=ANIM2_PREPARE_WEAPON && anim2!=ANIM2_TURNOFF_WEAPON &&
//			anim2!=ANIM2_DODGE_FRONT && anim2!=ANIM2_DODGE_BACK && anim2!=ANIM2_USE && anim2!=ANIM2_PICKUP &&
//			anim2!=ANIM2_DAMAGE_FRONT && anim2!=ANIM2_DAMAGE_BACK && anim2!=ANIM2_IDLE && anim2!=ANIM2_IDLE_COMBAT));

        animSequence.push_back( CritterAnim( anim, anim->Ticks, 0, anim->GetCnt() - 1, move_text, 0, anim->Anim1, anim->Anim2, item ) );
    }
    else
    {
        if( !Anim3d->CheckAnimation( anim1, anim2 ) )
        {
            if( !IsAnim() )
                AnimateStay();
            return;
        }

        animSequence.push_back( CritterAnim( nullptr, 0, 0, 0, true, 0, anim1, anim2, item ) );
    }

    if( animSequence.size() == 1 )
        NextAnim( false );
}

void CritterCl::AnimateStay()
{
    uint anim1 = GetAnim1();
    uint anim2 = GetAnim2();

    if( !Anim3d )
    {
        AnyFrames* anim = ResMngr.GetCrit2dAnim( GetModelName(), anim1, anim2, GetDir(), this );
        if( !anim )
            anim = ResMngr.CritterDefaultAnim;

        if( stayAnim.Anim != anim )
        {
            ProcessAnim( true, true, anim1, anim2, nullptr );

            stayAnim.Anim = anim;
            stayAnim.AnimTick = anim->Ticks;
            stayAnim.BeginFrm = 0;
            stayAnim.EndFrm = anim->GetCnt() - 1;
            if( GetCond() == COND_DEAD )
                stayAnim.BeginFrm = stayAnim.EndFrm;
            curSpr = stayAnim.BeginFrm;
        }

        SprId = anim->GetSprId( curSpr );

        SetOffs( 0, 0, true );
        short ox = 0, oy = 0;
        for( uint i = 0, j = curSpr % anim->GetCnt(); i <= j; i++ )
        {
            ox += anim->NextX[ i ];
            oy += anim->NextY[ i ];
        }
        ChangeOffs( ox, oy, false );
    }
    else
    {
        Anim3d->SetDir( GetDir() );

        int scale_factor = GetScaleFactor();
        if( scale_factor != 0 )
        {
            float scale = (float) scale_factor / 1000.0f;
            Anim3d->SetScale( scale, scale, scale );
        }

        Anim3d->CheckAnimation( anim1, anim2 );

        ProcessAnim( true, false, anim1, anim2, nullptr );
        SetOffs( 0, 0, true );

        if( GetCond() == COND_LIFE || GetCond() == COND_KNOCKOUT )
            Anim3d->SetAnimation( anim1, anim2, GetLayers3dData(), IsCombatMode() ? ANIMATION_COMBAT : 0 );
        else
            Anim3d->SetAnimation( anim1, anim2, GetLayers3dData(), ( ANIMATION_STAY | ANIMATION_PERIOD( 100 ) ) | ( IsCombatMode() ? ANIMATION_COMBAT : 0 ) );
    }
}

bool CritterCl::IsWalkAnim()
{
    if( animSequence.size() )
    {
        int anim2 = animSequence[ 0 ].IndAnim2;
        return anim2 == ANIM2_WALK || anim2 == ANIM2_RUN || anim2 == ANIM2_LIMP || anim2 == ANIM2_PANIC_RUN;
    }
    return false;
}

void CritterCl::ClearAnim()
{
    for( uint i = 0, j = (uint) animSequence.size(); i < j; i++ )
        SAFEREL( animSequence[ i ].ActiveItem );
    animSequence.clear();
}

void CritterCl::EraseAnim()
{

}

bool CritterCl::IsHaveLightSources()
{
    for( auto it = InvItems.begin(), end = InvItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetIsLight() )
            return true;
    }
    return false;
}

void CritterCl::TickStart( uint ms )
{
    TickCount = ms;
    StartTick = Timer::GameTick();
}

void CritterCl::TickNull()
{
    TickCount = 0;
}

bool CritterCl::IsFree()
{
    return Timer::GameTick() - StartTick >= TickCount;
}

uint CritterCl::GetAnim1()
{
    switch( GetCond() )
    {
    case COND_LIFE:
        return GetAnim1Life() ? GetAnim1Life() : ANIM1_UNARMED;
    case COND_KNOCKOUT:
        return GetAnim1Knockout() ? GetAnim1Knockout() : ANIM1_UNARMED;
    case COND_DEAD:
        return GetAnim1Dead() ? GetAnim1Dead() : ANIM1_UNARMED;
    default:
        break;
    }
    return ANIM1_UNARMED;
}

uint CritterCl::GetAnim2()
{
    switch( GetCond() )
    {
    case COND_LIFE:
        return GetAnim2Life() ? GetAnim2Life() : ( ( IsCombatMode() && GameOpt.Anim2CombatIdle ) ? GameOpt.Anim2CombatIdle : ANIM2_IDLE );
    case COND_KNOCKOUT:
        return GetAnim2Knockout() ? GetAnim2Knockout() : ANIM2_IDLE_PRONE_FRONT;
    case COND_DEAD:
        return GetAnim2Dead() ? GetAnim2Dead() : ANIM2_DEAD_FRONT;
    default:
        break;
    }
    return ANIM2_IDLE;
}

void CritterCl::ProcessAnim( bool animate_stay, bool is2d, uint anim1, uint anim2, Item* item )
{
    #ifdef FONLINE_CLIENT
    Script::RaiseInternalEvent( is2d ? ClientFunctions.Animation2dProcess : ClientFunctions.Animation3dProcess, animate_stay, this, anim1, anim2, item );
    #endif
}

int* CritterCl::GetLayers3dData()
{
    CScriptArray* layers = GetAnim3dLayer();
    if( layers->GetSize() == LAYERS3D_COUNT )
        memcpy( anim3dLayers, layers->At( 0 ), sizeof( anim3dLayers ) );
    else
        memzero( anim3dLayers, sizeof( anim3dLayers ) );
    SAFEREL( layers );
    return anim3dLayers;
}

bool CritterCl::IsAnimAviable( uint anim1, uint anim2 )
{
    if( !anim1 )
        anim1 = GetAnim1();
    // 3d
    if( Anim3d )
        return Anim3d->IsAnimation( anim1, anim2 );
    // 2d
    return ResMngr.GetCrit2dAnim( GetModelName(), anim1, anim2, GetDir(), this ) != nullptr;
}

void CritterCl::RefreshAnim()
{
    // Release previous
    SprMngr.FreePure3dAnimation( Anim3d );
    SprMngr.FreePure3dAnimation( Anim3dStay );
    Anim3d = nullptr;
    Anim3dStay = nullptr;

    // Check 3d availability
    string model_name = _str().parseHash( GetModelName() );
    string ext = _str( model_name ).getFileExtension();
    if( !Is3dExtensionSupported( ext ) )
        return;

    // Try load
    SprMngr.PushAtlasType( RES_ATLAS_DYNAMIC );
    Animation3d* anim3d = SprMngr.LoadPure3dAnimation( model_name, true );
    if( anim3d )
    {
        Anim3d = anim3d;
        Anim3dStay = SprMngr.LoadPure3dAnimation( model_name, false );

        Anim3d->SetDir( GetDir() );
        SprId = Anim3d->SprId;

        Anim3d->SetAnimation( ANIM1_UNARMED, ANIM2_IDLE, GetLayers3dData(), 0 );

        // Start mesh generation for Mapper
        #ifdef FONLINE_MAPPER
        Anim3d->StartMeshGeneration();
        Anim3dStay->StartMeshGeneration();
        #endif
    }
    SprMngr.PopAtlasType();
}

void CritterCl::ChangeDir( uchar dir, bool animate /* = true */ )
{
    if( dir >= DIRS_COUNT || GetIsNoRotate() )
        dir = 0;
    if( GetDir() == dir )
        return;
    SetDir( dir );
    if( Anim3d )
        Anim3d->SetDir( dir );
    if( animate && !IsAnim() )
        AnimateStay();
}

void CritterCl::Process()
{
    // Fading
    if( fadingEnable == true )
        Alpha = GetFadeAlpha();

    // Extra offsets
    if( OffsExtNextTick && Timer::GameTick() >= OffsExtNextTick )
    {
        OffsExtNextTick = Timer::GameTick() + 30;
        uint dist = DistSqrt( 0, 0, OxExtI, OyExtI );
        SprOx -= OxExtI;
        SprOy -= OyExtI;
        float mul = (float) ( dist / 10 );
        if( mul < 1.0f )
            mul = 1.0f;
        OxExtF += OxExtSpeed * mul;
        OyExtF += OyExtSpeed * mul;
        OxExtI = (int) OxExtF;
        OyExtI = (int) OyExtF;
        if( DistSqrt( 0, 0, OxExtI, OyExtI ) > dist ) // End of work
        {
            OffsExtNextTick = 0;
            OxExtI = 0;
            OyExtI = 0;
        }
        SetOffs( SprOx, SprOy, true );
    }

    // Animation
    CritterAnim& cr_anim = ( animSequence.size() ? animSequence[ 0 ] : stayAnim );
    int          anim_proc = ( Timer::GameTick() - animStartTick ) * 100 / ( cr_anim.AnimTick ? cr_anim.AnimTick : 1 );
    if( anim_proc >= 100 )
    {
        if( animSequence.size() )
            anim_proc = 100;
        else
            anim_proc %= 100;
    }

    // Change frames
    if( !Anim3d && anim_proc < 100 )
    {
        uint cur_spr = cr_anim.BeginFrm + ( ( cr_anim.EndFrm - cr_anim.BeginFrm + 1 ) * anim_proc ) / 100;
        if( cur_spr != curSpr )
        {
            // Change frame
            uint old_spr = curSpr;
            curSpr = cur_spr;
            SprId = cr_anim.Anim->GetSprId( curSpr );

            // Change offsets
            short ox = 0, oy = 0;
            for( uint i = 0, j = old_spr % cr_anim.Anim->GetCnt(); i <= j; i++ )
            {
                ox -= cr_anim.Anim->NextX[ i ];
                oy -= cr_anim.Anim->NextY[ i ];
            }
            for( uint i = 0, j = cur_spr % cr_anim.Anim->GetCnt(); i <= j; i++ )
            {
                ox += cr_anim.Anim->NextX[ i ];
                oy += cr_anim.Anim->NextY[ i ];
            }
            ChangeOffs( ox, oy, cr_anim.MoveText );
        }
    }

    if( animSequence.size() )
    {
        // Move offsets
        if( cr_anim.DirOffs )
        {
            int ox, oy;
            GetWalkHexOffsets( cr_anim.DirOffs - 1, ox, oy );
            SetOffs( ox - ( ox * anim_proc / 100 ), oy - ( oy * anim_proc / 100 ), true );

            if( anim_proc >= 100 )
            {
                NextAnim( true );
                if( MoveSteps.size() )
                    return;
            }
        }
        else
        {
            if( !Anim3d )
            {
                if( anim_proc >= 100 )
                    NextAnim( true );
            }
            else
            {
                if( !Anim3d->IsAnimationPlaying() )
                    NextAnim( true );
            }
        }

        if( MoveSteps.size() )
            return;
        if( !animSequence.size() )
            AnimateStay();
    }

    // Battle 3d mode
    // Todo: do same for 2d animations
    if( Anim3d && GameOpt.Anim2CombatIdle && !animSequence.size() && GetCond() == COND_LIFE && !GetAnim2Life() )
    {
        if( GameOpt.Anim2CombatBegin && IsCombatMode() && Anim3d->GetAnim2() != (int) GameOpt.Anim2CombatIdle )
            Animate( 0, GameOpt.Anim2CombatBegin, nullptr );
        else if( GameOpt.Anim2CombatEnd && !IsCombatMode() && Anim3d->GetAnim2() == (int) GameOpt.Anim2CombatIdle )
            Animate( 0, GameOpt.Anim2CombatEnd, nullptr );
    }

    // Fidget animation
    if( Timer::GameTick() >= tickFidget )
    {
        if( !animSequence.size() && GetCond() == COND_LIFE && IsFree() && !MoveSteps.size() && !IsCombatMode() )
            Action( ACTION_FIDGET, 0, nullptr );
        tickFidget = Timer::GameTick() + Random( GameOpt.CritterFidgetTime, GameOpt.CritterFidgetTime * 2 );
    }
}

void CritterCl::ChangeOffs( short change_ox, short change_oy, bool move_text )
{
    SetOffs( SprOx - OxExtI + change_ox, SprOy - OyExtI + change_oy, move_text );
}

void CritterCl::SetOffs( short set_ox, short set_oy, bool move_text )
{
    SprOx = set_ox + OxExtI;
    SprOy = set_oy + OyExtI;
    if( SprDrawValid )
    {
        SprMngr.GetDrawRect( SprDraw, DRect );
        if( move_text )
        {
            textRect = DRect;
            if( Anim3d )
                textRect.T += SprMngr.GetSpriteInfo( SprId )->Height / 6;
        }
        if( IsChosen() )
            SprMngr.SetEgg( GetHexX(), GetHexY(), SprDraw );
    }
}

void CritterCl::SetSprRect()
{
    if( SprDrawValid )
    {
        Rect old = DRect;
        SprMngr.GetDrawRect( SprDraw, DRect );
        textRect.L += DRect.L - old.L;
        textRect.R += DRect.L - old.L;
        textRect.T += DRect.T - old.T;
        textRect.B += DRect.T - old.T;

        if( IsChosen() )
            SprMngr.SetEgg( GetHexX(), GetHexY(), SprDraw );
    }
}

Rect CritterCl::GetTextRect()
{
    if( SprDrawValid )
        return textRect;
    return Rect();
}

void CritterCl::AddOffsExt( short ox, short oy )
{
    SprOx -= OxExtI;
    SprOy -= OyExtI;
    ox += OxExtI;
    oy += OyExtI;
    OxExtI = ox;
    OyExtI = oy;
    OxExtF = (float) ox;
    OyExtF = (float) oy;
    GetStepsXY( OxExtSpeed, OyExtSpeed, 0, 0, ox, oy );
    OxExtSpeed = -OxExtSpeed;
    OyExtSpeed = -OyExtSpeed;
    OffsExtNextTick = Timer::GameTick() + 30;
    SetOffs( SprOx, SprOy, true );
}

void CritterCl::GetWalkHexOffsets( int dir, int& ox, int& oy )
{
    int hx = 1, hy = 1;
    MoveHexByDirUnsafe( hx, hy, dir );
    GetHexInterval( hx, hy, 1, 1, ox, oy );
}

void CritterCl::SetText( const char* str, uint color, uint text_delay )
{
    tickStartText = Timer::GameTick();
    strTextOnHead = str;
    tickTextDelay = text_delay;
    textOnHeadColor = color;
}

void CritterCl::GetNameTextInfo( bool& nameVisible, int& x, int& y, int& w, int& h, int& lines )
{
    nameVisible = false;

    string str;
    if( strTextOnHead.empty() )
    {
        if( IsPlayer() && !GameOpt.ShowPlayerNames )
            return;
        if( IsNpc() && !GameOpt.ShowNpcNames )
            return;

        nameVisible = true;

        str = ( NameOnHead.empty() ? Name : NameOnHead );
        if( GameOpt.ShowCritId )
            str += _str( "  {}", GetId() );
        if( FLAG( Flags, FCRIT_DISCONNECT ) )
            str += GameOpt.PlayerOffAppendix;
    }
    else
    {
        str = strTextOnHead;
    }

    Rect tr = GetTextRect();
    x = (int) ( (float) ( tr.L + tr.W() / 2 + GameOpt.ScrOx ) / GameOpt.SpritesZoom - 100.0f );
    y = (int) ( (float) ( tr.T + GameOpt.ScrOy ) / GameOpt.SpritesZoom - 70.0f );

    SprMngr.GetTextInfo( 200, 70, str.c_str(), -1, FT_CENTERX | FT_BOTTOM | FT_BORDERED, w, h, lines );
    x += 100 - ( w / 2 );
    y += 70 - h;
}

void CritterCl::DrawTextOnHead()
{
    if( SprDrawValid )
    {
		bool nameVis;
		int x, y, w, h, lines;
		GetNameTextInfo(nameVis, x, y, w, h, lines);
        Rect   r( x, y, w + x, h + y );
		
        string str = "";
        uint   color;
        
		uint draw_count = 0;

		uint alpha = 0;
		if (!strTextOnHead.empty())
		{
			color = textOnHeadColor;

			if (tickTextDelay > 500)
			{
				uint dt = Timer::GameTick() - tickStartText;
				uint hide = tickTextDelay - 200;
				if (dt >= hide)
				{
					uint alpha = 0xFF * (100 - Procent(tickTextDelay - hide, dt - hide)) / 100;
					color = (alpha << 24) | (color & 0xFFFFFF);
				}
			}
			if (fadingEnable)
			{
				draw_count++;
				alpha = GetFadeAlpha();
				SprMngr.DrawStr(r, strTextOnHead, FT_CENTERX | FT_BOTTOM | FT_BORDERED, (alpha << 24) | (color & 0xFFFFFF));
			}
			else if (!IsFinishing())
			{
				draw_count++;
				SprMngr.DrawStr(r, strTextOnHead, FT_CENTERX | FT_BOTTOM | FT_BORDERED, color);
			}

			r[1] -= h/lines;
			r[3] = r[1] + h / lines;
			r[2] += 50;
			r[0] -= 50;
		}

		{
			bool isDraw = true;
			if (IsPlayer() && !GameOpt.ShowPlayerNames)
				isDraw = false;
			if (IsNpc() && !GameOpt.ShowNpcNames)
				isDraw = false;
			if (isDraw)
			{
				str = (NameOnHead.empty() ? Name : NameOnHead);
				if (GameOpt.ShowCritId)
					str += _str(" ({})", GetId());
				if (FLAG(Flags, FCRIT_DISCONNECT))
					str += GameOpt.PlayerOffAppendix;
				color = (NameColor ? NameColor : COLOR_CRITTER_NAME);
			}

			if (!str.empty())
			{
				if (fadingEnable)
				{
					draw_count++;
					alpha = GetFadeAlpha();
					SprMngr.DrawStr(r, str, FT_CENTERX | FT_BOTTOM | FT_BORDERED, (alpha << 24) | (color & 0xFFFFFF));
				}
				else if (!IsFinishing())
				{
					draw_count++;
					SprMngr.DrawStr(r, str, FT_CENTERX | FT_BOTTOM | FT_BORDERED, color);
				}
			}
		}
		
#ifdef FONLINE_CLIENT
		if (!draw_count)
		{
			Rect   tr = GetTextRect();
			int    x = (int)((float)(tr.L + tr.W() / 2 + GameOpt.ScrOx) / GameOpt.SpritesZoom - 100.0f);
			int    y = (int)((float)(tr.T + GameOpt.ScrOy) / GameOpt.SpritesZoom - 70.0f);
			r = Rect(x, y, x + 200, y + 70);
		}
		bool old = FOClient::Self->CanDrawInScripts;
		FOClient::Self->CanDrawInScripts = true;
		Script::RaiseInternalEvent( ClientFunctions.CritterNameRender, this, r[0], r[1], r[2], r[3], alpha );
		FOClient::Self->CanDrawInScripts = old;
#endif
    }

    if( Timer::GameTick() - tickStartText >= tickTextDelay && !strTextOnHead.empty() )
        strTextOnHead = "";
}
