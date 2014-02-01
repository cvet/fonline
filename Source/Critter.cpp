#include "StdAfx.h"
#include "Critter.h"
#include "MapManager.h"
#include "ItemManager.h"
#include "CritterType.h"
#include "Access.h"
#include "CritterManager.h"

const char* CritterEventFuncName[ CRITTER_EVENT_MAX ] =
{
    "void %s(Critter&)",                                                          // CRITTER_EVENT_IDLE
    "void %s(Critter&,bool)",                                                     // CRITTER_EVENT_FINISH
    "void %s(Critter&,Critter@)",                                                 // CRITTER_EVENT_DEAD
    "void %s(Critter&)",                                                          // CRITTER_EVENT_RESPAWN
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_SHOW_CRITTER
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_SHOW_CRITTER_1
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_SHOW_CRITTER_2
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_SHOW_CRITTER_3
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_HIDE_CRITTER
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_HIDE_CRITTER_1
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_HIDE_CRITTER_2
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_HIDE_CRITTER_3
    "void %s(Critter&,Item&,bool,Critter@)",                                      // CRITTER_EVENT_SHOW_ITEM_ON_MAP
    "void %s(Critter&,Item&)",                                                    // CRITTER_EVENT_CHANGE_ITEM_ON_MAP
    "void %s(Critter&,Item&,bool,Critter@)",                                      // CRITTER_EVENT_HIDE_ITEM_ON_MAP
    "bool %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_ATTACK
    "bool %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_ATTACKED
    "void %s(Critter&,Critter&,bool,Item&,uint)",                                 // CRITTER_EVENT_STEALING
    "void %s(Critter&,Critter&,int,int)",                                         // CRITTER_EVENT_MESSAGE
    "bool %s(Critter&,Item&,Critter@,Item@,Scenery@)",                            // CRITTER_EVENT_USE_ITEM
    "bool %s(Critter&,Critter&,Item&)",                                           // CRITTER_EVENT_USE_ITEM_ON_ME
    "bool %s(Critter&,int,Critter@,Item@,Scenery@)",                              // CRITTER_EVENT_USE_SKILL
    "bool %s(Critter&,Critter&,int)",                                             // CRITTER_EVENT_USE_SKILL_ON_ME
    "void %s(Critter&,Item&)",                                                    // CRITTER_EVENT_DROP_ITEM
    "void %s(Critter&,Item&,uint8)",                                              // CRITTER_EVENT_MOVE_ITEM
    "void %s(Critter&,uint,uint,uint,uint,uint)",                                 // CRITTER_EVENT_KNOCKOUT
    "void %s(Critter&,Critter&,Critter@)",                                        // CRITTER_EVENT_SMTH_DEAD
    "void %s(Critter&,Critter&,Critter&,bool,Item&,uint)",                        // CRITTER_EVENT_SMTH_STEALING
    "void %s(Critter&,Critter&,Critter&)",                                        // CRITTER_EVENT_SMTH_ATTACK
    "void %s(Critter&,Critter&,Critter&)",                                        // CRITTER_EVENT_SMTH_ATTACKED
    "void %s(Critter&,Critter&,Item&,Critter@,Item@,Scenery@)",                   // CRITTER_EVENT_SMTH_USE_ITEM
    "void %s(Critter&,Critter&,int,Critter@,Item@,Scenery@)",                     // CRITTER_EVENT_SMTH_USE_SKILL
    "void %s(Critter&,Critter&,Item&)",                                           // CRITTER_EVENT_SMTH_DROP_ITEM
    "void %s(Critter&,Critter&,Item&,uint8)",                                     // CRITTER_EVENT_SMTH_MOVE_ITEM
    "void %s(Critter&,Critter&,uint,uint,uint,uint,uint)",                        // CRITTER_EVENT_SMTH_KNOCKOUT
    "int %s(Critter&,NpcPlane&,int,Critter@,Item@)",                              // CRITTER_EVENT_PLANE_BEGIN
    "int %s(Critter&,NpcPlane&,int,Critter@,Item@)",                              // CRITTER_EVENT_PLANE_END
    "int %s(Critter&,NpcPlane&,int,uint&,uint&,uint&)",                           // CRITTER_EVENT_PLANE_RUN
    "bool %s(Critter&,Critter&,bool,uint)",                                       // CRITTER_EVENT_BARTER
    "bool %s(Critter&,Critter&,bool,uint)",                                       // CRITTER_EVENT_TALK
    "bool %s(Critter&,int,Item@,float&,float&,float&,float&,float&,uint&,bool&)", // CRITTER_EVENT_GLOBAL_PROCESS
    "bool %s(Critter&,Item@,uint,int,uint&,uint16&,uint16&,uint8&)",              // CRITTER_EVENT_GLOBAL_INVITE
    "void %s(Critter&,Map&,bool)",                                                // CRITTER_EVENT_TURN_BASED_PROCESS
    "void %s(Critter&,Critter&,Map&,bool)",                                       // CRITTER_EVENT_SMTH_TURN_BASED_PROCESS
};

/************************************************************************/
/*                                                                      */
/************************************************************************/

bool      Critter::ParamsRegEnabled[ MAX_PARAMS ] = { 0 };
uint      Critter::ParamsSendMsgLen = sizeof( Critter::ParamsSendCount );
ushort    Critter::ParamsSendCount = 0;
UShortVec Critter::ParamsSend;
bool      Critter::ParamsSendEnabled[ MAX_PARAMS ] = { 0 };
int       Critter::ParamsSendScript[ MAX_PARAMS ] = { 0 };
int       Critter::ParamsChangeScript[ MAX_PARAMS ] = { 0 };
int       Critter::ParamsGetScript[ MAX_PARAMS ] = { 0 };
int       Critter::ParamsDialogGetScript[ MAX_PARAMS ] = { 0 };
bool      Critter::SlotDataSendEnabled[ 0x100 ] = { 0 };
int       Critter::SlotDataSendScript[ 0x100 ] = { 0 };
uint      Critter::ParamsChosenSendMask[ MAX_PARAMS ] = { 0 };
uint      Critter::ParametersMin[ MAX_PARAMETERS_ARRAYS ] = { 0 };
uint      Critter::ParametersMax[ MAX_PARAMETERS_ARRAYS ] = { MAX_PARAMS - 1, 0 };
bool      Critter::ParametersOffset[ MAX_PARAMETERS_ARRAYS ] = { 0 };
bool      Critter::SlotEnabled[ 0x100 ] = { true, true, true, true, 0 };
Item*     Critter::SlotEnabledCacheData[ 0x100 ] = { 0 };
Item*     Critter::SlotEnabledCacheDataExt[ 0x100 ] = { 0 };

Critter::Critter(): CritterIsNpc( false ), RefCounter( 1 ), IsNotValid( false ),
                    GroupMove( NULL ), PrevHexTick( 0 ), PrevHexX( 0 ), PrevHexY( 0 ),
                    startBreakTime( 0 ), breakTime( 0 ), waitEndTick( 0 ), KnockoutAp( 0 ), CacheValuesNextTick( 0 ), IntellectCacheValue( 0 ),
                    Flags( 0 ), AccessContainerId( 0 ), ItemTransferCount( 0 ),
                    TryingGoHomeTick( 0 ), ApRegenerationTick( 0 ), GlobalIdleNextTick( 0 ), LockMapTransfers( 0 ),
                    ViewMapId( 0 ), ViewMapPid( 0 ), ViewMapLook( 0 ), ViewMapHx( 0 ), ViewMapHy( 0 ), ViewMapDir( 0 ),
                    DisableSend( 0 ), CanBeRemoved( false )
{
    memzero( &Data, sizeof( Data ) );
    DataExt = NULL;
    memzero( FuncId, sizeof( FuncId ) );
    GroupSelf = new GlobalMapGroup();
    GroupSelf->CurX = (float) Data.WorldX;
    GroupSelf->CurY = (float) Data.WorldY;
    GroupSelf->ToX = GroupSelf->CurX;
    GroupSelf->ToY = GroupSelf->CurY;
    GroupSelf->Speed = 0.0f;
    GroupSelf->Rule = this;
    memzero( ParamsIsChanged, sizeof( ParamsIsChanged ) );
    ParamsChanged.reserve( 10 );
    ParamLocked = -1;
    for( int i = 0; i < MAX_PARAMETERS_ARRAYS; i++ )
        ThisPtr[ i ] = this;
    ItemSlotMain = ItemSlotExt = defItemSlotHand = new Item();
    ItemSlotArmor = defItemSlotArmor = new Item();
}

Critter::~Critter()
{
    SAFEDEL( GroupSelf );
    SAFEREL( defItemSlotHand );
    SAFEREL( defItemSlotArmor );
}

CritDataExt* Critter::GetDataExt()
{
    if( !DataExt )
    {
        DataExt = new CritDataExt();
        memzero( DataExt, sizeof( CritDataExt ) );
        GMapFog.Create( GM__MAXZONEX, GM__MAXZONEY, DataExt->GlobalMapFog );

        #ifdef MEMORY_DEBUG
        if( IsNpc() )
        {
            MEMORY_PROCESS( MEMORY_NPC, sizeof( CritDataExt ) );
        }
        else
        {
            MEMORY_PROCESS( MEMORY_CLIENT, sizeof( CritDataExt ) );
        }
        #endif
    }
    return DataExt;
}

void Critter::FullClear()
{
    IsNotValid = true;
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        item->Accessory = 0xB0;
        ItemMngr.ItemToGarbage( item );
    }
    invItems.clear();
}

int Critter::GetLook()
{
    int look = GameOpt.LookNormal + GetParam( ST_PERCEPTION ) * 3 + GetParam( ST_BONUS_LOOK ) + GetMultihex();
    if( look < (int) GameOpt.LookMinimum )
        look = GameOpt.LookMinimum;
    return look;
}

uint Critter::GetTalkDistance( Critter* talker )
{
    int dist = Data.Params[ ST_TALK_DISTANCE ];
    if( dist <= 0 )
        dist = GameOpt.TalkDistance;
    if( talker )
        dist += talker->GetMultihex();
    return dist + GetMultihex();
}

uint Critter::GetUseApCost( Item* weap, int use )
{
    if( Script::PrepareContext( ServerFunctions.GetUseApCost, _FUNC_, GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( weap );
        Script::SetArgUChar( use );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return 1;
}

uint Critter::GetAttackDist( Item* weap, int use )
{
    if( Script::PrepareContext( ServerFunctions.GetAttackDistantion, _FUNC_, GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( weap );
        Script::SetArgUChar( use );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return 0;
}

uint Critter::GetUseDist()
{
    return 1 + GetMultihex();
}

uint Critter::GetMultihex()
{
    int mh = Data.Multihex;
    if( mh < 0 )
        mh = CritType::GetMultihex( GetCrType() );
    return CLAMP( mh, 0, MAX_HEX_OFFSET );
}

bool Critter::CheckFind( int find_type )
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
    return ( FLAG( find_type, FIND_LIFE ) && IsLife() ) ||
           ( FLAG( find_type, FIND_KO ) && IsKnockout() ) ||
           ( FLAG( find_type, FIND_DEAD ) && IsDead() );
}

void Critter::SetLexems( const char* lexems )
{
    if( lexems )
    {
        uint len = Str::Length( lexems );
        if( !len )
        {
            Data.Lexems[ 0 ] = 0;
            return;
        }

        if( len >= LEXEMS_SIZE )
            len = LEXEMS_SIZE - 1;
        memcpy( Data.Lexems, lexems, len );
        Data.Lexems[ len ] = 0;
    }
    else
    {
        Data.Lexems[ 0 ] = 0;
    }
}

int Critter::RunParamsSendScript( int bind_id, uint param_index, Critter* from_cr, Critter* to_cr )
{
    if( Script::PrepareContext( bind_id, _FUNC_, from_cr->GetInfo() ) )
    {
        Script::SetArgUInt( param_index );
        Script::SetArgObject( from_cr );
        Script::SetArgObject( to_cr );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return 0;
}

bool Critter::RunSlotDataSendScript( int bind_id, uchar slot, Item* item, Critter* from_cr, Critter* to_cr )
{
    if( Script::PrepareContext( bind_id, _FUNC_, from_cr->GetInfo() ) )
    {
        Script::SetArgUChar( slot );
        Script::SetArgObject( item );
        Script::SetArgObject( from_cr );
        Script::SetArgObject( to_cr );
        if( Script::RunPrepared() )
            return Script::GetReturnedBool();
    }
    return false;
}

void Critter::SyncLockCritters( bool self_critters, bool only_players )
{
    if( LogicMT )
    {
        CrVec& critters = ( self_critters ? VisCrSelf : VisCr );

        // Collect critters
        CrVec find_critters;
        find_critters.reserve( critters.size() );
        if( only_players )
        {
            for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
                if( ( *it )->IsPlayer() )
                    find_critters.push_back( *it );
        }
        else
        {
            find_critters = critters;
        }

        // Synchronize
        for( auto it = find_critters.begin(), end = find_critters.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck after synchronization
        CrVec find_critters2;
        find_critters2.reserve( find_critters.size() );
        if( only_players )
        {
            for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
                if( ( *it )->IsPlayer() )
                    find_critters2.push_back( *it );
        }
        else
        {
            find_critters2 = critters;
        }

        if( !CompareContainers( find_critters, find_critters2 ) )
            SyncLockCritters( self_critters, only_players );
    }
}

void Critter::ProcessVisibleCritters()
{
    if( IsNotValid )
        return;

    // Global map
    if( !GetMap() )
    {
        if( !GroupMove )
        {
            WriteLogF( _FUNC_, " - GroupMove nullptr, critter<%s>. Creating dump file.\n", GetInfo() );
            CreateDump( "ProcessVisibleCritters" );
            return;
        }

        if( IsPlayer() )
        {
            Client* cl = (Client*) this;
            for( auto it = GroupMove->CritMove.begin(), end = GroupMove->CritMove.end(); it != end; ++it )
            {
                Critter* cr = *it;
                if( this == cr )
                {
                    SETFLAG( Flags, FCRIT_CHOSEN );
                    cl->Send_AddCritter( this );
                    UNSETFLAG( Flags, FCRIT_CHOSEN );
                    cl->Send_AllParams();
                    cl->Send_AddAllItems();
                }
                else
                {
                    cl->Send_AddCritter( cr );
                }
            }
        }
        return;
    }

    // Local map
    int  vis;
    int  look_base_self = GetLook();
    int  dir_self = GetDir();
    int  dirs_count = DIRS_COUNT;
    bool show_cr1 = ( ( FuncId[ CRITTER_EVENT_SHOW_CRITTER_1 ] > 0 || FuncId[ CRITTER_EVENT_HIDE_CRITTER_1 ] > 0 ) && Data.ShowCritterDist1 > 0 );
    bool show_cr2 = ( ( FuncId[ CRITTER_EVENT_SHOW_CRITTER_2 ] > 0 || FuncId[ CRITTER_EVENT_HIDE_CRITTER_2 ] > 0 ) && Data.ShowCritterDist2 > 0 );
    bool show_cr3 = ( ( FuncId[ CRITTER_EVENT_SHOW_CRITTER_3 ] > 0 || FuncId[ CRITTER_EVENT_HIDE_CRITTER_3 ] > 0 ) && Data.ShowCritterDist3 > 0 );
    bool show_cr = ( show_cr1 || show_cr2 || show_cr3 );
    // Sneak self
    int  sneak_base_self = GetRawParam( SK_SNEAK );
    if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SNEAK_WEIGHT ) )
        sneak_base_self -= GetItemsWeight() / GameOpt.LookWeight;

    Map* map = MapMngr.GetMap( GetMap() );
    if( !map )
        return;

    CrVec critters;
    map->GetCritters( critters, true );

    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr == this || cr->IsNotValid )
            continue;

        int dist = DistGame( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY() );

        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SCRIPT ) )
        {
            bool allow_self = true;
            if( Script::PrepareContext( ServerFunctions.CheckLook, _FUNC_, GetInfo() ) )
            {
                Script::SetArgObject( map );
                Script::SetArgObject( this );
                Script::SetArgObject( cr );
                if( Script::RunPrepared() )
                    allow_self = Script::GetReturnedBool();
            }
            bool allow_opp = true;
            if( Script::PrepareContext( ServerFunctions.CheckLook, _FUNC_, GetInfo() ) )
            {
                Script::SetArgObject( map );
                Script::SetArgObject( cr );
                Script::SetArgObject( this );
                if( Script::RunPrepared() )
                    allow_opp = Script::GetReturnedBool();
            }

            if( allow_self )
            {
                if( cr->AddCrIntoVisVec( this ) )
                {
                    Send_AddCritter( cr );
                    EventShowCritter( cr );
                }
            }
            else
            {
                if( cr->DelCrFromVisVec( this ) )
                {
                    Send_RemoveCritter( cr );
                    EventHideCritter( cr );
                }
            }

            if( allow_opp )
            {
                if( AddCrIntoVisVec( cr ) )
                {
                    cr->Send_AddCritter( this );
                    cr->EventShowCritter( this );
                }
            }
            else
            {
                if( DelCrFromVisVec( cr ) )
                {
                    cr->Send_RemoveCritter( this );
                    cr->EventHideCritter( this );
                }
            }

            if( show_cr )
            {
                if( show_cr1 )
                {
                    if( (int) Data.ShowCritterDist1 >= dist )
                    {
                        if( AddCrIntoVisSet1( cr->GetId() ) )
                            EventShowCritter1( cr );
                    }
                    else
                    {
                        if( DelCrFromVisSet1( cr->GetId() ) )
                            EventHideCritter1( cr );
                    }
                }
                if( show_cr2 )
                {
                    if( (int) Data.ShowCritterDist2 >= dist )
                    {
                        if( AddCrIntoVisSet2( cr->GetId() ) )
                            EventShowCritter2( cr );
                    }
                    else
                    {
                        if( DelCrFromVisSet2( cr->GetId() ) )
                            EventHideCritter2( cr );
                    }
                }
                if( show_cr3 )
                {
                    if( (int) Data.ShowCritterDist3 >= dist )
                    {
                        if( AddCrIntoVisSet3( cr->GetId() ) )
                            EventShowCritter3( cr );
                    }
                    else
                    {
                        if( DelCrFromVisSet3( cr->GetId() ) )
                            EventHideCritter3( cr );
                    }
                }
            }

            if( ( cr->FuncId[ CRITTER_EVENT_SHOW_CRITTER_1 ] > 0 || cr->FuncId[ CRITTER_EVENT_HIDE_CRITTER_1 ] > 0 ) && cr->Data.ShowCritterDist1 > 0 )
            {
                if( (int) cr->Data.ShowCritterDist1 >= dist )
                {
                    if( cr->AddCrIntoVisSet1( GetId() ) )
                        cr->EventShowCritter1( this );
                }
                else
                {
                    if( cr->DelCrFromVisSet1( GetId() ) )
                        cr->EventHideCritter1( this );
                }
            }
            if( ( cr->FuncId[ CRITTER_EVENT_SHOW_CRITTER_2 ] > 0 || cr->FuncId[ CRITTER_EVENT_HIDE_CRITTER_2 ] > 0 ) && cr->Data.ShowCritterDist2 > 0 )
            {
                if( (int) cr->Data.ShowCritterDist2 >= dist )
                {
                    if( cr->AddCrIntoVisSet2( GetId() ) )
                        cr->EventShowCritter2( this );
                }
                else
                {
                    if( cr->DelCrFromVisSet2( GetId() ) )
                        cr->EventHideCritter2( this );
                }
            }
            if( ( cr->FuncId[ CRITTER_EVENT_SHOW_CRITTER_3 ] > 0 || cr->FuncId[ CRITTER_EVENT_HIDE_CRITTER_3 ] > 0 ) && cr->Data.ShowCritterDist3 > 0 )
            {
                if( (int) cr->Data.ShowCritterDist3 >= dist )
                {
                    if( cr->AddCrIntoVisSet3( GetId() ) )
                        cr->EventShowCritter3( this );
                }
                else
                {
                    if( cr->DelCrFromVisSet3( GetId() ) )
                        cr->EventHideCritter3( this );
                }
            }
            continue;
        }

        int look_self = look_base_self;
        int look_opp = cr->GetLook();

        // Dir modifier
        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_DIR ) )
        {
            // Self
            int real_dir = GetFarDir( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY() );
            int i = ( dir_self > real_dir ? dir_self - real_dir : real_dir - dir_self );
            if( i > dirs_count / 2 )
                i = dirs_count - i;
            look_self -= look_self * GameOpt.LookDir[ i ] / 100;
            // Opponent
            int dir_opp = cr->GetDir();
            real_dir = ( ( real_dir + dirs_count / 2 ) % dirs_count );
            i = ( dir_opp > real_dir ? dir_opp - real_dir : real_dir - dir_opp );
            if( i > dirs_count / 2 )
                i = dirs_count - i;
            look_opp -= look_opp * GameOpt.LookDir[ i ] / 100;
        }

        if( dist > look_self && dist > look_opp )
            dist = MAX_INT;

        // Trace
        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_TRACE ) && dist != MAX_INT )
        {
            TraceData trace;
            trace.TraceMap = map;
            trace.BeginHx = GetHexX();
            trace.BeginHy = GetHexY();
            trace.EndHx = cr->GetHexX();
            trace.EndHy = cr->GetHexY();
            MapMngr.TraceBullet( trace );
            if( !trace.IsFullTrace )
                dist = MAX_INT;
        }

        // Self
        if( cr->IsRawParam( MODE_HIDE ) && dist != MAX_INT )
        {
            int sneak_opp = cr->GetRawParam( SK_SNEAK );
            if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SNEAK_WEIGHT ) )
                sneak_opp -= cr->GetItemsWeight() / GameOpt.LookWeight;
            if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SNEAK_DIR ) )
            {
                int real_dir = GetFarDir( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY() );
                int i = ( dir_self > real_dir ? dir_self - real_dir : real_dir - dir_self );
                if( i > dirs_count / 2 )
                    i = dirs_count - i;
                sneak_opp -= sneak_opp * GameOpt.LookSneakDir[ i ] / 100;
            }
            sneak_opp /= (int) GameOpt.SneakDivider;
            if( sneak_opp < 0 )
                sneak_opp = 0;
            vis = look_self - sneak_opp;
        }
        else
        {
            vis = look_self;
        }
        if( vis < (int) GameOpt.LookMinimum )
            vis = GameOpt.LookMinimum;

        if( vis >= dist )
        {
            if( cr->AddCrIntoVisVec( this ) )
            {
                Send_AddCritter( cr );
                EventShowCritter( cr );
            }
        }
        else
        {
            if( cr->DelCrFromVisVec( this ) )
            {
                Send_RemoveCritter( cr );
                EventHideCritter( cr );
            }
        }

        if( show_cr1 )
        {
            if( (int) Data.ShowCritterDist1 >= dist )
            {
                if( AddCrIntoVisSet1( cr->GetId() ) )
                    EventShowCritter1( cr );
            }
            else
            {
                if( DelCrFromVisSet1( cr->GetId() ) )
                    EventHideCritter1( cr );
            }
        }
        if( show_cr2 )
        {
            if( (int) Data.ShowCritterDist2 >= dist )
            {
                if( AddCrIntoVisSet2( cr->GetId() ) )
                    EventShowCritter2( cr );
            }
            else
            {
                if( DelCrFromVisSet2( cr->GetId() ) )
                    EventHideCritter2( cr );
            }
        }
        if( show_cr3 )
        {
            if( (int) Data.ShowCritterDist3 >= dist )
            {
                if( AddCrIntoVisSet3( cr->GetId() ) )
                    EventShowCritter3( cr );
            }
            else
            {
                if( DelCrFromVisSet3( cr->GetId() ) )
                    EventHideCritter3( cr );
            }
        }

        // Opponent
        if( IsRawParam( MODE_HIDE ) && dist != MAX_INT )
        {
            int sneak_self = sneak_base_self;
            if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SNEAK_DIR ) )
            {
                int dir_opp = cr->GetDir();
                int real_dir = GetFarDir( cr->GetHexX(), cr->GetHexY(), GetHexX(), GetHexY() );
                int i = ( dir_opp > real_dir ? dir_opp - real_dir : real_dir - dir_opp );
                if( i > dirs_count / 2 )
                    i = dirs_count - i;
                sneak_self -= sneak_self * GameOpt.LookSneakDir[ i ] / 100;
            }
            sneak_self /= (int) GameOpt.SneakDivider;
            if( sneak_self < 0 )
                sneak_self = 0;
            vis = look_opp - sneak_self;
        }
        else
        {
            vis = look_opp;
        }
        if( vis < (int) GameOpt.LookMinimum )
            vis = GameOpt.LookMinimum;

        if( vis >= dist )
        {
            if( AddCrIntoVisVec( cr ) )
            {
                cr->Send_AddCritter( this );
                cr->EventShowCritter( this );
            }
        }
        else
        {
            if( DelCrFromVisVec( cr ) )
            {
                cr->Send_RemoveCritter( this );
                cr->EventHideCritter( this );
            }
        }

        if( ( cr->FuncId[ CRITTER_EVENT_SHOW_CRITTER_1 ] > 0 || cr->FuncId[ CRITTER_EVENT_HIDE_CRITTER_1 ] > 0 ) && cr->Data.ShowCritterDist1 > 0 )
        {
            if( (int) cr->Data.ShowCritterDist1 >= dist )
            {
                if( cr->AddCrIntoVisSet1( GetId() ) )
                    cr->EventShowCritter1( this );
            }
            else
            {
                if( cr->DelCrFromVisSet1( GetId() ) )
                    cr->EventHideCritter1( this );
            }
        }
        if( ( cr->FuncId[ CRITTER_EVENT_SHOW_CRITTER_2 ] > 0 || cr->FuncId[ CRITTER_EVENT_HIDE_CRITTER_2 ] > 0 ) && cr->Data.ShowCritterDist2 > 0 )
        {
            if( (int) cr->Data.ShowCritterDist2 >= dist )
            {
                if( cr->AddCrIntoVisSet2( GetId() ) )
                    cr->EventShowCritter2( this );
            }
            else
            {
                if( cr->DelCrFromVisSet2( GetId() ) )
                    cr->EventHideCritter2( this );
            }
        }
        if( ( cr->FuncId[ CRITTER_EVENT_SHOW_CRITTER_3 ] > 0 || cr->FuncId[ CRITTER_EVENT_HIDE_CRITTER_3 ] > 0 ) && cr->Data.ShowCritterDist3 > 0 )
        {
            if( (int) cr->Data.ShowCritterDist3 >= dist )
            {
                if( cr->AddCrIntoVisSet3( GetId() ) )
                    cr->EventShowCritter3( this );
            }
            else
            {
                if( cr->DelCrFromVisSet3( GetId() ) )
                    cr->EventHideCritter3( this );
            }
        }
    }
}

void Critter::ProcessVisibleItems()
{
    if( IsNotValid )
        return;

    Map* map = MapMngr.GetMap( GetMap() );
    if( !map )
        return;

    int        look = GetLook();
    ItemPtrVec items = map->GetItemsNoLock();
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;

        if( item->IsHidden() )
            continue;
        else if( item->IsAlwaysView() )
        {
            if( AddIdVisItem( item->GetId() ) )
            {
                Send_AddItemOnMap( item );
                EventShowItemOnMap( item, item->ViewPlaceOnMap, item->ViewByCritter );
            }
        }
        else
        {
            bool allowed = false;
            if( item->IsTrap() && FLAG( GameOpt.LookChecks, LOOK_CHECK_ITEM_SCRIPT ) )
            {
                if( Script::PrepareContext( ServerFunctions.CheckTrapLook, _FUNC_, GetInfo() ) )
                {
                    Script::SetArgObject( map );
                    Script::SetArgObject( this );
                    Script::SetArgObject( item );
                    if( Script::RunPrepared() )
                        allowed = Script::GetReturnedBool();
                }
            }
            else
            {
                int dist = DistGame( Data.HexX, Data.HexY, item->AccHex.HexX, item->AccHex.HexY );
                if( item->IsTrap() )
                    dist += item->TrapGetValue();
                allowed = look >= dist;
            }

            if( allowed )
            {
                if( AddIdVisItem( item->GetId() ) )
                {
                    Send_AddItemOnMap( item );
                    EventShowItemOnMap( item, item->ViewPlaceOnMap, item->ViewByCritter );
                }
            }
            else
            {
                if( DelIdVisItem( item->GetId() ) )
                {
                    Send_EraseItemFromMap( item );
                    EventHideItemOnMap( item, item->ViewPlaceOnMap, item->ViewByCritter );
                }
            }
        }
    }
}

void Critter::ViewMap( Map* map, int look, ushort hx, ushort hy, int dir )
{
    if( IsNotValid )
        return;

    Send_GameInfo( map );

    // Critters
    int   dirs_count = DIRS_COUNT;
    int   vis;
    CrVec critters;
    map->GetCritters( critters, true );

    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr == this || cr->IsNotValid )
            continue;

        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SCRIPT ) )
        {
            if( Script::PrepareContext( ServerFunctions.CheckLook, _FUNC_, GetInfo() ) )
            {
                Script::SetArgObject( map );
                Script::SetArgObject( this );
                Script::SetArgObject( cr );
                if( Script::RunPrepared() && Script::GetReturnedBool() )
                    Send_AddCritter( cr );
            }
            continue;
        }

        int dist = DistGame( hx, hy, cr->Data.HexX, cr->Data.HexY );
        int look_self = look;

        // Dir modifier
        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_DIR ) )
        {
            int real_dir = GetFarDir( hx, hy, cr->GetHexX(), cr->GetHexY() );
            int i = ( dir > real_dir ? dir - real_dir : real_dir - dir );
            if( i > dirs_count / 2 )
                i = dirs_count - i;
            look_self -= look_self * GameOpt.LookDir[ i ] / 100;
        }

        if( dist > look_self )
            continue;

        // Trace
        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_TRACE ) && dist != MAX_INT )
        {
            TraceData trace;
            trace.TraceMap = map;
            trace.BeginHx = hx;
            trace.BeginHy = hy;
            trace.EndHx = cr->GetHexX();
            trace.EndHy = cr->GetHexY();
            MapMngr.TraceBullet( trace );
            if( !trace.IsFullTrace )
                continue;
        }

        // Hide modifier
        if( cr->IsRawParam( MODE_HIDE ) )
        {
            int sneak_opp = cr->GetRawParam( SK_SNEAK );
            if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SNEAK_WEIGHT ) )
                sneak_opp -= cr->GetItemsWeight() / GameOpt.LookWeight;
            if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SNEAK_DIR ) )
            {
                int real_dir = GetFarDir( hx, hy, cr->GetHexX(), cr->GetHexY() );
                int i = ( dir > real_dir ? dir - real_dir : real_dir - dir );
                if( i > dirs_count / 2 )
                    i = dirs_count - i;
                sneak_opp -= sneak_opp * GameOpt.LookSneakDir[ i ] / 100;
            }
            sneak_opp /= (int) GameOpt.SneakDivider;
            if( sneak_opp < 0 )
                sneak_opp = 0;
            vis = look_self - sneak_opp;
        }
        else
        {
            vis = look_self;
        }
        if( vis < (int) GameOpt.LookMinimum )
            vis = GameOpt.LookMinimum;
        if( vis >= dist )
            Send_AddCritter( cr );
    }

    // Items
    ItemPtrVec& items = map->GetItemsNoLock();
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;

        if( item->IsHidden() )
            continue;
        else if( item->IsAlwaysView() )
            Send_AddItemOnMap( item );
        else
        {
            bool allowed = false;
            if( item->IsTrap() && FLAG( GameOpt.LookChecks, LOOK_CHECK_ITEM_SCRIPT ) )
            {
                if( Script::PrepareContext( ServerFunctions.CheckTrapLook, _FUNC_, GetInfo() ) )
                {
                    Script::SetArgObject( map );
                    Script::SetArgObject( this );
                    Script::SetArgObject( item );
                    if( Script::RunPrepared() )
                        allowed = Script::GetReturnedBool();
                }
            }
            else
            {
                int dist = DistGame( hx, hy, item->AccHex.HexX, item->AccHex.HexY );
                if( item->IsTrap() )
                    dist += item->TrapGetValue();
                allowed = look >= dist;
            }


            if( allowed )
                Send_AddItemOnMap( item );
        }
    }
}

void Critter::ClearVisible()
{
    SyncLockCritters( false, false );
    SyncLockCritters( true, false );

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        auto     it_ = std::find( cr->VisCrSelf.begin(), cr->VisCrSelf.end(), this );
        if( it_ != cr->VisCrSelf.end() )
        {
            cr->VisCrSelf.erase( it_ );
            cr->VisCrSelfMap.erase( this->GetId() );
        }
        cr->Send_RemoveCritter( this );
    }
    VisCr.clear();
    VisCrMap.clear();

    for( auto it = VisCrSelf.begin(), end = VisCrSelf.end(); it != end; ++it )
    {
        Critter* cr = *it;
        auto     it_ = std::find( cr->VisCr.begin(), cr->VisCr.end(), this );
        if( it_ != cr->VisCr.end() )
        {
            cr->VisCr.erase( it_ );
            cr->VisCrMap.erase( this->GetId() );
        }
    }
    VisCrSelf.clear();
    VisCrSelfMap.clear();

    VisCr1.clear();
    VisCr2.clear();
    VisCr3.clear();

    VisItemLocker.Lock();
    VisItem.clear();
    VisItemLocker.Unlock();
}

Critter* Critter::GetCritSelf( uint crid, bool sync_lock )
{
    Critter* cr = NULL;

    auto     it = VisCrSelfMap.find( crid );
    if( it != VisCrSelfMap.end() )
        cr = ( *it ).second;

    if( cr && sync_lock )
    {
        // Synchronize
        SYNC_LOCK( cr );

        // Recheck
        it = VisCrSelfMap.find( crid );
        if( it == VisCrSelfMap.end() )
            return GetCritSelf( crid, sync_lock );
    }

    return cr;
}

void Critter::GetCrFromVisCr( CrVec& critters, int find_type, bool vis_cr_self, bool sync_lock )
{
    CrVec& vis_cr = ( vis_cr_self ? VisCrSelf : VisCr );

    CrVec  find_critters;
    for( auto it = vis_cr.begin(), end = vis_cr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->CheckFind( find_type ) && std::find( critters.begin(), critters.end(), cr ) == critters.end() )
            find_critters.push_back( cr );
    }

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = find_critters.begin(), end = find_critters.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck after synchronization
        CrVec find_critters2;
        for( auto it = vis_cr.begin(), end = vis_cr.end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( cr->CheckFind( find_type ) && std::find( critters.begin(), critters.end(), cr ) == critters.end() )
                find_critters2.push_back( cr );
        }

        // Search again, if have difference
        if( !CompareContainers( find_critters, find_critters2 ) )
        {
            GetCrFromVisCr( critters, find_type, vis_cr_self, sync_lock );
            return;
        }
    }

    critters.reserve( critters.size() + find_critters.size() );
    for( auto it = find_critters.begin(), end = find_critters.end(); it != end; ++it )
        critters.push_back( *it );
}

bool Critter::AddCrIntoVisVec( Critter* add_cr )
{
    if( VisCrMap.count( add_cr->GetId() ) )
        return false;

    VisCr.push_back( add_cr );
    VisCrMap.insert( PAIR( add_cr->GetId(), add_cr ) );

    add_cr->VisCrSelf.push_back( this );
    add_cr->VisCrSelfMap.insert( PAIR( this->GetId(), this ) );
    return true;
}

bool Critter::DelCrFromVisVec( Critter* del_cr )
{
    auto it_map = VisCrMap.find( del_cr->GetId() );
    if( it_map == VisCrMap.end() )
        return false;

    VisCrMap.erase( it_map );
    VisCr.erase( std::find( VisCr.begin(), VisCr.end(), del_cr ) );

    auto it = std::find( del_cr->VisCrSelf.begin(), del_cr->VisCrSelf.end(), this );
    if( it != del_cr->VisCrSelf.end() )
    {
        del_cr->VisCrSelf.erase( it );
        del_cr->VisCrSelfMap.erase( this->GetId() );
    }
    return true;
}

bool Critter::AddCrIntoVisSet1( uint crid )
{
    return VisCr1.insert( crid ).second;
}

bool Critter::AddCrIntoVisSet2( uint crid )
{
    return VisCr2.insert( crid ).second;
}

bool Critter::AddCrIntoVisSet3( uint crid )
{
    return VisCr3.insert( crid ).second;
}

bool Critter::DelCrFromVisSet1( uint crid )
{
    return VisCr1.erase( crid ) != 0;
}

bool Critter::DelCrFromVisSet2( uint crid )
{
    return VisCr2.erase( crid ) != 0;
}

bool Critter::DelCrFromVisSet3( uint crid )
{
    return VisCr3.erase( crid ) != 0;
}

bool Critter::AddIdVisItem( uint item_id )
{
    VisItemLocker.Lock();
    bool result = VisItem.insert( item_id ).second;
    VisItemLocker.Unlock();
    return result;
}

bool Critter::DelIdVisItem( uint item_id )
{
    VisItemLocker.Lock();
    bool result = VisItem.erase( item_id ) != 0;
    VisItemLocker.Unlock();
    return result;
}

bool Critter::CountIdVisItem( uint item_id )
{
    VisItemLocker.Lock();
    bool result = VisItem.count( item_id ) != 0;
    VisItemLocker.Unlock();
    return result;
}

void Critter::SyncLockItems()
{
    ItemPtrVec inv_items = invItems;
    for( auto it = inv_items.begin(), end = inv_items.end(); it != end; ++it )
        SYNC_LOCK( *it );
}

bool Critter::SetDefaultItems( ProtoItem* proto_hand, ProtoItem* proto_armor )
{
    if( !proto_hand || !proto_armor )
        return false;

    defItemSlotHand->Init( proto_hand );
    defItemSlotArmor->Init( proto_armor );
    defItemSlotHand->Accessory = ITEM_ACCESSORY_CRITTER;
    defItemSlotArmor->Accessory = ITEM_ACCESSORY_CRITTER;
    defItemSlotHand->AccCritter.Id = GetId();
    defItemSlotArmor->AccCritter.Id = GetId();
    defItemSlotHand->AccCritter.Slot = SLOT_HAND1;
    defItemSlotArmor->AccCritter.Slot = SLOT_ARMOR;
    return true;
}

void Critter::AddItem( Item*& item, bool send )
{
    // Check
    if( !item || !item->Proto )
    {
        WriteLogF( _FUNC_, " - Item null ptr, critter<%s>.\n", GetInfo() );
        return;
    }

    // Add
    if( item->IsStackable() )
    {
        Item* item_already = GetItemByPid( item->GetProtoId() );
        if( item_already )
        {
            item_already->Count_Add( item->GetCount() );
            ItemMngr.ItemToGarbage( item );
            item = item_already;
            if( send )
                SendAA_ItemData( item );
            return;
        }
    }

    item->SetSortValue( invItems );
    SetItem( item );

    // Send
    if( send )
    {
        Send_AddItem( item );
        if( item->AccCritter.Slot != SLOT_INV )
            SendAA_MoveItem( item, ACTION_REFRESH, 0 );
    }

    // Change item
    if( Script::PrepareContext( ServerFunctions.CritterMoveItem, _FUNC_, GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( item );
        Script::SetArgUChar( SLOT_GROUND );
        Script::RunPrepared();
    }
}

void Critter::SetItem( Item* item )
{
    if( !item || !item->Proto )
    {
        WriteLogF( _FUNC_, " - Item null ptr, critter<%s>.\n", GetInfo() );
        return;
    }

    if( item->IsCar() && GroupMove && !GroupMove->CarId )
        GroupMove->CarId = item->GetId();
    invItems.push_back( item );
    item->AccCritter.Id = GetId();

    if( item->Accessory != ITEM_ACCESSORY_CRITTER )
    {
        item->Accessory = ITEM_ACCESSORY_CRITTER;
        item->AccCritter.Slot = SLOT_INV;
    }

    switch( item->AccCritter.Slot )
    {
    case SLOT_INV:
label_InvSlot:
        item->AccCritter.Slot = SLOT_INV;
        break;
    case SLOT_HAND1:
        if( ItemSlotMain->GetId() )
            goto label_InvSlot;
        if( item->IsWeapon() && !CritType::IsAnim1( GetCrType(), item->Proto->Weapon_Anim1 ) )
            goto label_InvSlot;
        ItemSlotMain = item;
        break;
    case SLOT_HAND2:
        if( ItemSlotExt->GetId() )
            goto label_InvSlot;
        ItemSlotExt = item;
        break;
    case SLOT_ARMOR:
        if( ItemSlotArmor->GetId() )
            goto label_InvSlot;
        if( !item->IsArmor() )
            goto label_InvSlot;
        if( !CritType::IsCanArmor( GetCrType() ) )
            goto label_InvSlot;
        ItemSlotArmor = item;
        break;
    default:
        break;
    }
}

void Critter::EraseItem( Item* item, bool send )
{
    if( !item )
    {
        WriteLogF( _FUNC_, " - Item null ptr, critter<%s>.\n", GetInfo() );
        return;
    }

    auto it = std::find( invItems.begin(), invItems.end(), item );
    if( it != invItems.end() )
        invItems.erase( it );
    else
        WriteLogF( _FUNC_, " - Item not found, id<%u>, pid<%u>, critter<%s>.\n", item->GetId(), item->GetProtoId(), GetInfo() );

    if( !GetMap() && GroupMove && GroupMove->CarId == item->GetId() )
    {
        GroupMove->CarId = 0;
        SendA_GlobalInfo( GroupMove, GM_INFO_GROUP_PARAM );
    }

    item->Accessory = 0xd0;
    TakeDefaultItem( item->AccCritter.Slot );
    if( send )
        Send_EraseItem( item );
    if( item->AccCritter.Slot != SLOT_INV )
        SendAA_MoveItem( item, ACTION_REFRESH, 0 );

    uchar from_slot = item->AccCritter.Slot;
    item->AccCritter.Slot = SLOT_GROUND;
    if( Script::PrepareContext( ServerFunctions.CritterMoveItem, _FUNC_, GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( item );
        Script::SetArgUChar( from_slot );
        Script::RunPrepared();
    }
}

Item* Critter::GetItem( uint item_id, bool skip_hide )
{
    if( !item_id )
        return NULL;
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetId() == item_id )
        {
            if( skip_hide && item->IsHidden() )
                return NULL;

            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

Item* Critter::GetInvItem( uint item_id, int transfer_type )
{
    if( transfer_type == TRANSFER_CRIT_LOOT && IsRawParam( MODE_NO_LOOT ) )
        return NULL;
    if( transfer_type == TRANSFER_CRIT_STEAL && IsRawParam( MODE_NO_STEAL ) )
        return NULL;

    Item* item = GetItem( item_id, true );
    if( !item || item->AccCritter.Slot != SLOT_INV ||
        ( transfer_type == TRANSFER_CRIT_LOOT && item->IsNoLoot() ) ||
        ( transfer_type == TRANSFER_CRIT_STEAL && item->IsNoSteal() ) )
        return NULL;
    return item;
}

void Critter::GetInvItems( ItemPtrVec& items, int transfer_type, bool lock )
{
    if( transfer_type == TRANSFER_CRIT_LOOT && IsRawParam( MODE_NO_LOOT ) )
        return;
    if( transfer_type == TRANSFER_CRIT_STEAL && IsRawParam( MODE_NO_STEAL ) )
        return;

    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->AccCritter.Slot == SLOT_INV && !item->IsHidden() &&
            !( transfer_type == TRANSFER_CRIT_LOOT && item->IsNoLoot() ) &&
            !( transfer_type == TRANSFER_CRIT_STEAL && item->IsNoSteal() ) )
            items.push_back( item );
    }

    if( lock )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

#pragma MESSAGE("Add explicit sync lock.")
Item* Critter::GetItemByPid( ushort item_pid )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == item_pid )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

Item* Critter::GetItemByPidSlot( ushort item_pid, int slot )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == item_pid && item->AccCritter.Slot == slot )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

Item* Critter::GetItemByPidInvPriority( ushort item_pid )
{
    ProtoItem* proto_item = ItemMngr.GetProtoItem( item_pid );
    if( !proto_item )
        return NULL;

    if( proto_item->Stackable )
    {
        for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
        {
            Item* item = *it;
            if( item->GetProtoId() == item_pid )
            {
                SYNC_LOCK( item );
                return item;
            }
        }
    }
    else
    {
        Item* another_slot = NULL;
        for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
        {
            Item* item = *it;
            if( item->GetProtoId() == item_pid )
            {
                if( item->AccCritter.Slot == SLOT_INV )
                {
                    SYNC_LOCK( item );
                    return item;
                }
                another_slot = item;
            }
        }
        if( another_slot )
            SYNC_LOCK( another_slot );
        return another_slot;
    }
    return NULL;
}

Item* Critter::GetAmmoForWeapon( Item* weap )
{
    if( !weap->IsWeapon() )
        return NULL;

    Item* ammo = GetItemByPid( weap->Data.AmmoPid );
    if( ammo )
        return ammo;
    ammo = GetItemByPid( weap->Proto->Weapon_DefaultAmmoPid );
    if( ammo )
        return ammo;
    ammo = GetAmmo( weap->Proto->Weapon_Caliber );

    // Already synchronized
    return ammo;
}

Item* Critter::GetAmmo( int caliber )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetType() == ITEM_TYPE_AMMO && item->Proto->Ammo_Caliber == caliber )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

Item* Critter::GetItemCar()
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->IsCar() )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

Item* Critter::GetItemSlot( int slot )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->AccCritter.Slot == slot )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

void Critter::GetItemsSlot( int slot, ItemPtrVec& items, bool lock )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( slot < 0 || item->AccCritter.Slot == slot )
            items.push_back( item );
    }

    if( lock )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

void Critter::GetItemsType( int type, ItemPtrVec& items, bool lock )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetType() == type )
            items.push_back( item );
    }

    if( lock )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

uint Critter::CountItemPid( ushort pid )
{
    uint res = 0;
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == pid )
            res += item->GetCount();
    }
    return res;
}

bool Critter::MoveItem( uchar from_slot, uchar to_slot, uint item_id, uint count )
{
    if( !item_id )
    {
        switch( from_slot )
        {
        case SLOT_HAND1:
            item_id = ItemSlotMain->GetId();
            break;
        case SLOT_HAND2:
            item_id = ItemSlotExt->GetId();
            break;
        case SLOT_ARMOR:
            item_id = ItemSlotArmor->GetId();
            break;
        default:
            break;
        }
    }

    if( !item_id )
    {
        WriteLogF( _FUNC_, " - Item id is zero, from slot<%u>, to slot<%u>, critter<%s>.\n", from_slot, to_slot, GetInfo() );
        return false;
    }

    Item* item = GetItem( item_id, IsPlayer() );
    if( !item )
    {
        WriteLogF( _FUNC_, " - Item not found, item id<%u>, critter<%s>.\n", item_id, GetInfo() );
        return false;
    }

    if( item->AccCritter.Slot != from_slot || from_slot == to_slot )
    {
        WriteLogF( _FUNC_, " - Wrong slots, from slot<%u>, from slot real<%u>, to slot<%u>, item id<%u>, critter<%s>.\n", from_slot, item->AccCritter.Slot, to_slot, item_id, GetInfo() );
        return false;
    }

    if( to_slot != SLOT_GROUND && !SlotEnabled[ to_slot ] )
    {
        WriteLogF( _FUNC_, " - Slot<%u> is not allowed, critter<%s>.\n", to_slot, GetInfo() );
        return false;
    }

    Item* item_swap = ( ( to_slot != SLOT_INV && to_slot != SLOT_GROUND ) ? GetItemSlot( to_slot ) : NULL );
    bool  allow = false;
    if( Script::PrepareContext( ServerFunctions.CritterCheckMoveItem, _FUNC_, GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( item );
        Script::SetArgUChar( to_slot );
        Script::SetArgObject( item_swap );
        if( Script::RunPrepared() )
            allow = Script::GetReturnedBool();
    }
    if( !allow )
    {
        if( IsPlayer() )
        {
            Send_AddItem( item );
            WriteLogF( _FUNC_, " - Can't move item with pid<%u> to slot<%u>, player<%s>.\n", item->GetProtoId(), to_slot, GetInfo() );
        }
        return false;
    }

    if( to_slot == SLOT_GROUND )
    {
        if( !count || count > item->GetCount() )
        {
            Send_AddItem( item );
            return false;
        }

        bool full_drop = ( !item->IsStackable() || count >= item->GetCount() );
        if( !full_drop )
        {
            if( GetMap() )
            {
                Item* item_ = ItemMngr.SplitItem( item, count );
                if( !item_ )
                {
                    Send_AddItem( item );
                    return false;
                }
                SendAA_ItemData( item );
                item = item_;
            }
            else
            {
                item->Count_Sub( count );
                SendAA_ItemData( item );
                item = NULL;
            }
        }
        else
        {
            EraseItem( item, false );
            if( !GetMap() )
            {
                ItemMngr.ItemToGarbage( item );
                item = NULL;
            }
        }

        if( !item )
            return true;

        Map* map = MapMngr.GetMap( GetMap() );
        if( !map )
        {
            WriteLogF( _FUNC_, " - Map not found, map id<%u>, critter<%s>.\n", GetMap(), GetInfo() );
            ItemMngr.ItemToGarbage( item );
            return true;
        }

        SendAA_Action( ACTION_DROP_ITEM, from_slot, item );
        item->ViewByCritter = this;
        map->AddItem( item, GetHexX(), GetHexY() );
        item->ViewByCritter = NULL;
        item->EventDrop( this );
        EventDropItem( item );
        return true;
    }

    TakeDefaultItem( from_slot );
    TakeDefaultItem( to_slot );
    if( to_slot == SLOT_HAND1 )
        ItemSlotMain = item;
    else if( to_slot == SLOT_HAND2 )
        ItemSlotExt = item;
    else if( to_slot == SLOT_ARMOR )
        ItemSlotArmor = item;
    if( item_swap )
    {
        if( from_slot == SLOT_HAND1 )
            ItemSlotMain = item_swap;
        else if( from_slot == SLOT_HAND2 )
            ItemSlotExt = item_swap;
        else if( from_slot == SLOT_ARMOR )
            ItemSlotArmor = item_swap;
    }

    item->AccCritter.Slot = to_slot;
    if( item_swap )
        item_swap->AccCritter.Slot = from_slot;

    SendAA_MoveItem( item, ACTION_MOVE_ITEM, from_slot );
    item->EventMove( this, from_slot );
    EventMoveItem( item, from_slot );
    if( item_swap )
    {
        SendAA_MoveItem( item, ACTION_MOVE_ITEM_SWAP, to_slot );
        item->EventMove( this, to_slot );
        EventMoveItem( item, to_slot );
    }

    return true;
}

void Critter::TakeDefaultItem( uchar slot )
{
    switch( slot )
    {
    case SLOT_HAND1:
        ItemSlotMain = defItemSlotHand;
        break;
    case SLOT_HAND2:
        ItemSlotExt = defItemSlotHand;
        break;
    case SLOT_ARMOR:
        ItemSlotArmor = defItemSlotArmor;
        break;
    default:
        break;
    }
}

uint Critter::CountItems()
{
    uint count = 0;
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
        count += ( *it )->GetCount();
    return count;
}

bool Critter::IsHaveGeckItem()
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->IsGeck() )
            return true;
    }
    return false;
}

void Critter::ToKnockout( uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, ushort knock_hx, ushort knock_hy )
{
    Data.Cond = COND_KNOCKOUT;
    Data.Anim2Knockout = anim2idle;
    Data.Anim2KnockoutEnd = anim2end;
    KnockoutAp = lost_ap;
    Send_Knockout( this, anim2begin, anim2idle, knock_hx, knock_hy );
    SendA_Knockout( anim2begin, anim2idle, knock_hx, knock_hy );
}

void Critter::TryUpOnKnockout()
{
    // Critter lie on ground
    if( GetRawParam( ST_CURRENT_HP ) <= 0 )
        return;

    // Subtract knockout ap
    if( KnockoutAp )
    {
        int cur_ap = GetRawParam( ST_CURRENT_AP );
        if( cur_ap <= 0 )
            return;
        int ap = MIN( (int) KnockoutAp, cur_ap );
        SubAp( ap );
        KnockoutAp -= ap;
        if( KnockoutAp )
            return;
    }

    // Wait when ap regenerated to zero
    if( GetRawParam( ST_CURRENT_AP ) < 0 )
        return;

    // Stand up
    Data.Cond = COND_LIFE;
    SendAA_Action( ACTION_STANDUP, Data.Anim2KnockoutEnd, NULL );
    SetBreakTime( GameOpt.Breaktime );
}

void Critter::ToDead( uint anim2, bool send_all )
{
    bool already_dead = IsDead();

    if( GetParam( ST_CURRENT_HP ) > 0 )
        Data.Params[ ST_CURRENT_HP ] = 0;
    Data.Cond = COND_DEAD;
    Data.Anim2Dead = anim2;

    Item* item = ItemSlotMain;
    if( item->GetId() )
        MoveItem( SLOT_HAND1, SLOT_INV, item->GetId(), item->GetCount() );
    item = ItemSlotExt;
    if( item->GetId() )
        MoveItem( SLOT_HAND2, SLOT_INV, item->GetId(), item->GetCount() );
    // if(ItemSlotArmor->GetId()) MoveItem(SLOT_ARMOR,SLOT_INV,ItemSlotArmor->GetId(),ItemSlotArmor->GetCount());

    if( send_all )
    {
        SendAA_Action( ACTION_DEAD, anim2, NULL );
        if( IsPlayer() )
            Send_AllParams();
    }

    if( !already_dead )
    {
        Map* map = MapMngr.GetMap( GetMap() );
        if( map )
        {
            uint multihex = GetMultihex();
            map->UnsetFlagCritter( GetHexX(), GetHexY(), multihex, false );
            map->SetFlagCritter( GetHexX(), GetHexY(), multihex, true );
        }
    }
}

bool Critter::ParseScript( const char* script, bool first_time )
{
    if( script && script[ 0 ] )
    {
        uint func_num = Script::GetScriptFuncNum( script, "void %s(Critter&,bool)" );
        if( !func_num )
        {
            WriteLogF( _FUNC_, " - Script<%s> bind fail, critter<%s>.\n", script, GetInfo() );
            return false;
        }
        Data.ScriptId = func_num;
    }

    if( Data.ScriptId && Script::PrepareContext( Script::GetScriptFuncBindId( Data.ScriptId ), _FUNC_, GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgBool( first_time );
        Script::RunPrepared();
    }
    return true;
}

bool Critter::PrepareScriptFunc( int num_scr_func )
{
    if( num_scr_func >= CRITTER_EVENT_MAX )
        return false;
    if( FuncId[ num_scr_func ] <= 0 )
        return false;
    return Script::PrepareContext( FuncId[ num_scr_func ], _FUNC_, Str::FormatBuf( "Critter<%s>, func<%s>", GetInfo(), Script::GetBindFuncName( FuncId[ num_scr_func ] ).c_str() ) );
}

void Critter::EventIdle()
{
    if( !PrepareScriptFunc( CRITTER_EVENT_IDLE ) )
        return;
    Script::SetArgObject( this );
    Script::RunPrepared();
}

void Critter::EventFinish( bool deleted )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_FINISH ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgBool( deleted );
    Script::RunPrepared();
}

void Critter::EventDead( Critter* killer )
{
    if( PrepareScriptFunc( CRITTER_EVENT_DEAD ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( killer );
        Script::RunPrepared();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_DEAD ] > 0 )
            cr->EventSmthDead( this, killer );
    }
}

void Critter::EventRespawn()
{
    if( !PrepareScriptFunc( CRITTER_EVENT_RESPAWN ) )
        return;
    Script::SetArgObject( this );
    Script::RunPrepared();
}

void Critter::EventShowCritter( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SHOW_CRITTER ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );
    Script::RunPrepared();
}

void Critter::EventShowCritter1( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SHOW_CRITTER_1 ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );
    Script::RunPrepared();
}

void Critter::EventShowCritter2( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SHOW_CRITTER_2 ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );
    Script::RunPrepared();
}

void Critter::EventShowCritter3( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SHOW_CRITTER_3 ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );
    Script::RunPrepared();
}

void Critter::EventHideCritter( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_HIDE_CRITTER ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );
    Script::RunPrepared();
}

void Critter::EventHideCritter1( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_HIDE_CRITTER_1 ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );
    Script::RunPrepared();
}

void Critter::EventHideCritter2( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_HIDE_CRITTER_2 ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );
    Script::RunPrepared();
}

void Critter::EventHideCritter3( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_HIDE_CRITTER_3 ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );
    Script::RunPrepared();
}

void Critter::EventShowItemOnMap( Item* item, bool added, Critter* dropper )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SHOW_ITEM_ON_MAP ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( item );
    Script::SetArgBool( added );
    Script::SetArgObject( dropper );
    Script::RunPrepared();
}

void Critter::EventChangeItemOnMap( Item* item )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_CHANGE_ITEM_ON_MAP ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( item );
    Script::RunPrepared();
}

void Critter::EventHideItemOnMap( Item* item, bool removed, Critter* picker )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_HIDE_ITEM_ON_MAP ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( item );
    Script::SetArgBool( removed );
    Script::SetArgObject( picker );
    Script::RunPrepared();
}

bool Critter::EventAttack( Critter* target )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_ATTACK ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( target );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_ATTACK ] > 0 )
            cr->EventSmthAttack( this, target );
    }
    return result;
}

bool Critter::EventAttacked( Critter* attacker )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_ATTACKED ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( attacker );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_ATTACKED ] > 0 )
            cr->EventSmthAttacked( this, attacker );
    }

    if( !result && attacker )
    {
        if( Script::PrepareContext( ServerFunctions.CritterAttacked, _FUNC_, GetInfo() ) )
        {
            Script::SetArgObject( this );
            Script::SetArgObject( attacker );
            Script::RunPrepared();
        }
    }
    return result;
}

bool Critter::EventStealing( Critter* thief, Item* item, uint count )
{
    bool success = false;
    if( Script::PrepareContext( ServerFunctions.CritterStealing, _FUNC_, GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( thief );
        Script::SetArgObject( item );
        Script::SetArgUInt( count );
        if( Script::RunPrepared() )
            success = Script::GetReturnedBool();
    }

    if( PrepareScriptFunc( CRITTER_EVENT_STEALING ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( thief );
        Script::SetArgBool( success );
        Script::SetArgObject( item );
        Script::SetArgUInt( count );
        Script::RunPrepared();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_STEALING ] > 0 )
            cr->EventSmthStealing( this, thief, success, item, count );
    }

    return success;
}

void Critter::EventMessage( Critter* from_cr, int num, int val )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_MESSAGE ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( from_cr );
    Script::SetArgUInt( num );
    Script::SetArgUInt( val );
    Script::RunPrepared();
}

bool Critter::EventUseItem( Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_USE_ITEM ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( item );
        Script::SetArgObject( on_critter );
        Script::SetArgObject( on_item );
        Script::SetArgObject( on_scenery );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_USE_ITEM ] > 0 )
            cr->EventSmthUseItem( this, item, on_critter, on_item, on_scenery );
    }

    return result;
}

bool Critter::EventUseItemOnMe( Critter* who_use, Item* item )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_USE_ITEM_ON_ME ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( who_use );
        Script::SetArgObject( item );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

bool Critter::EventUseSkill( int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_USE_SKILL ) )
    {
        Script::SetArgObject( this );
        Script::SetArgUInt( skill < 0 ? skill : SKILL_OFFSET( skill ) );
        Script::SetArgObject( on_critter );
        Script::SetArgObject( on_item );
        Script::SetArgObject( on_scenery );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_USE_SKILL ] > 0 )
            cr->EventSmthUseSkill( this, skill, on_critter, on_item, on_scenery );
    }

    return result;
}

bool Critter::EventUseSkillOnMe( Critter* who_use, int skill )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_USE_SKILL_ON_ME ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( who_use );
        Script::SetArgUInt( skill < 0 ? skill : SKILL_OFFSET( skill ) );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

void Critter::EventDropItem( Item* item )
{
    if( PrepareScriptFunc( CRITTER_EVENT_DROP_ITEM ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( item );
        Script::RunPrepared();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_DROP_ITEM ] > 0 )
            cr->EventSmthDropItem( this, item );
    }
}

void Critter::EventMoveItem( Item* item, uchar from_slot )
{
    if( Script::PrepareContext( ServerFunctions.CritterMoveItem, _FUNC_, GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( item );
        Script::SetArgUChar( from_slot );
        Script::RunPrepared();
    }

    if( PrepareScriptFunc( CRITTER_EVENT_MOVE_ITEM ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( item );
        Script::SetArgUChar( from_slot );
        Script::RunPrepared();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_MOVE_ITEM ] > 0 )
            cr->EventSmthMoveItem( this, item, from_slot );
    }
}

void Critter::EventKnockout( uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, uint knock_dist )
{
    if( PrepareScriptFunc( CRITTER_EVENT_KNOCKOUT ) )
    {
        Script::SetArgObject( this );
        Script::SetArgUInt( anim2begin );
        Script::SetArgUInt( anim2idle );
        Script::SetArgUInt( anim2end );
        Script::SetArgUInt( lost_ap );
        Script::SetArgUInt( knock_dist );
        Script::RunPrepared();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_KNOCKOUT ] > 0 )
            cr->EventSmthKnockout( this, anim2begin, anim2idle, anim2end, lost_ap, knock_dist );
    }
}

void Critter::EventSmthDead( Critter* from_cr, Critter* killer )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_DEAD ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( from_cr );
    Script::SetArgObject( killer );
    Script::RunPrepared();
}

void Critter::EventSmthStealing( Critter* from_cr, Critter* thief, bool success, Item* item, uint count )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_STEALING ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( from_cr );
    Script::SetArgObject( thief );
    Script::SetArgBool( success );
    Script::SetArgObject( item );
    Script::SetArgUInt( count );
    Script::RunPrepared();
}

void Critter::EventSmthAttack( Critter* from_cr, Critter* target )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_ATTACK ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( from_cr );
    Script::SetArgObject( target );
    Script::RunPrepared();
}

void Critter::EventSmthAttacked( Critter* from_cr, Critter* attacker )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_ATTACKED ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( from_cr );
    Script::SetArgObject( attacker );
    Script::RunPrepared();
}

void Critter::EventSmthUseItem( Critter* from_cr, Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_USE_ITEM ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( from_cr );
    Script::SetArgObject( item );
    Script::SetArgObject( on_critter );
    Script::SetArgObject( on_item );
    Script::SetArgObject( on_scenery );
    Script::RunPrepared();
}

void Critter::EventSmthUseSkill( Critter* from_cr, int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_USE_SKILL ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( from_cr );
    Script::SetArgUInt( skill < 0 ? skill : SKILL_OFFSET( skill ) );
    Script::SetArgObject( on_critter );
    Script::SetArgObject( on_item );
    Script::SetArgObject( on_scenery );
    Script::RunPrepared();
}

void Critter::EventSmthDropItem( Critter* from_cr, Item* item )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_DROP_ITEM ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( from_cr );
    Script::SetArgObject( item );
    Script::RunPrepared();
}

void Critter::EventSmthMoveItem( Critter* from_cr, Item* item, uchar from_slot )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_MOVE_ITEM ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( from_cr );
    Script::SetArgObject( item );
    Script::SetArgUChar( from_slot );
    Script::RunPrepared();
}

void Critter::EventSmthKnockout( Critter* from_cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, uint knock_dist )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_KNOCKOUT ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( from_cr );
    Script::SetArgUInt( anim2begin );
    Script::SetArgUInt( anim2idle );
    Script::SetArgUInt( anim2end );
    Script::SetArgUInt( lost_ap );
    Script::SetArgUInt( knock_dist );
    Script::RunPrepared();
}

int Critter::EventPlaneBegin( AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item )
{
    if( PrepareScriptFunc( CRITTER_EVENT_PLANE_BEGIN ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( plane );
        Script::SetArgUInt( reason );
        Script::SetArgObject( some_cr );
        Script::SetArgObject( some_item );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return PLANE_RUN_GLOBAL;
}

int Critter::EventPlaneEnd( AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item )
{
    if( PrepareScriptFunc( CRITTER_EVENT_PLANE_END ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( plane );
        Script::SetArgUInt( reason );
        Script::SetArgObject( some_cr );
        Script::SetArgObject( some_item );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return PLANE_RUN_GLOBAL;
}

int Critter::EventPlaneRun( AIDataPlane* plane, int reason, uint& p0, uint& p1, uint& p2 )
{
    if( PrepareScriptFunc( CRITTER_EVENT_PLANE_RUN ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( plane );
        Script::SetArgUInt( reason );
        Script::SetArgAddress( &p0 );
        Script::SetArgAddress( &p1 );
        Script::SetArgAddress( &p2 );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return PLANE_RUN_GLOBAL;
}

bool Critter::EventBarter( Critter* cr_barter, bool attach, uint barter_count )
{
    if( FuncId[ CRITTER_EVENT_BARTER ] <= 0 )
        return true;
    if( !PrepareScriptFunc( CRITTER_EVENT_BARTER ) )
        return false;
    Script::SetArgObject( this );
    Script::SetArgObject( cr_barter );
    Script::SetArgBool( attach );
    Script::SetArgUInt( barter_count );
    if( Script::RunPrepared() )
        return Script::GetReturnedBool();
    return false;
}

bool Critter::EventTalk( Critter* cr_talk, bool attach, uint talk_count )
{
    if( FuncId[ CRITTER_EVENT_TALK ] <= 0 )
        return true;
    if( !PrepareScriptFunc( CRITTER_EVENT_TALK ) )
        return false;
    Script::SetArgObject( this );
    Script::SetArgObject( cr_talk );
    Script::SetArgBool( attach );
    Script::SetArgUInt( talk_count );
    if( Script::RunPrepared() )
        return Script::GetReturnedBool();
    return false;
}

bool Critter::EventGlobalProcess( int type, Item* car, float& x, float& y, float& to_x, float& to_y, float& speed, uint& encounter_descriptor, bool& wait_for_answer )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_GLOBAL_PROCESS ) )
    {
        Script::SetArgObject( this );
        Script::SetArgUInt( type );
        Script::SetArgObject( car );
        Script::SetArgAddress( &x );
        Script::SetArgAddress( &y );
        Script::SetArgAddress( &to_x );
        Script::SetArgAddress( &to_y );
        Script::SetArgAddress( &speed );
        Script::SetArgAddress( &encounter_descriptor );
        Script::SetArgAddress( &wait_for_answer );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

bool Critter::EventGlobalInvite( Item* car, uint encounter_descriptor, int combat_mode, uint& map_id, ushort& hx, ushort& hy, uchar& dir )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_GLOBAL_INVITE ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( car );
        Script::SetArgUInt( encounter_descriptor );
        Script::SetArgUInt( combat_mode );
        Script::SetArgAddress( &map_id );
        Script::SetArgAddress( &hx );
        Script::SetArgAddress( &hy );
        Script::SetArgAddress( &dir );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

void Critter::EventTurnBasedProcess( Map* map, bool begin_turn )
{
    if( PrepareScriptFunc( CRITTER_EVENT_TURN_BASED_PROCESS ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( map );
        Script::SetArgBool( begin_turn );
        Script::RunPrepared();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_TURN_BASED_PROCESS ] > 0 )
            cr->EventSmthTurnBasedProcess( this, map, begin_turn );
    }
}

void Critter::EventSmthTurnBasedProcess( Critter* from_cr, Map* map, bool begin_turn )
{
    if( PrepareScriptFunc( CRITTER_EVENT_SMTH_TURN_BASED_PROCESS ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( from_cr );
        Script::SetArgObject( map );
        Script::SetArgBool( begin_turn );
        Script::RunPrepared();
    }
}

void Critter::Send_Move( Critter* from_cr, uint move_params )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Move( from_cr, move_params );
}
void Critter::Send_Dir( Critter* from_cr )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Dir( from_cr );
}
void Critter::Send_AddCritter( Critter* cr )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AddCritter( cr );
}
void Critter::Send_RemoveCritter( Critter* cr )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_RemoveCritter( cr );
}
void Critter::Send_LoadMap( Map* map )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_LoadMap( map );
}
void Critter::Send_XY( Critter* cr )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_XY( cr );
}
void Critter::Send_AddItemOnMap( Item* item )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AddItemOnMap( item );
}
void Critter::Send_ChangeItemOnMap( Item* item )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_ChangeItemOnMap( item );
}
void Critter::Send_EraseItemFromMap( Item* item )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_EraseItemFromMap( item );
}
void Critter::Send_AnimateItem( Item* item, uchar from_frm, uchar to_frm )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AnimateItem( item, from_frm, to_frm );
}
void Critter::Send_AddItem( Item* item )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AddItem( item );
}
void Critter::Send_EraseItem( Item* item )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_EraseItem( item );
}
void Critter::Send_ContainerInfo()
{
    if( IsPlayer() )
        ( (Client*) this )->Send_ContainerInfo();
}
void Critter::Send_ContainerInfo( Item* item_cont, uchar transfer_type, bool open_screen )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_ContainerInfo( item_cont, transfer_type, open_screen );
}
void Critter::Send_ContainerInfo( Critter* cr_cont, uchar transfer_type, bool open_screen )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_ContainerInfo( cr_cont, transfer_type, open_screen );
}
void Critter::Send_GlobalInfo( uchar flags )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_GlobalInfo( flags );
}
void Critter::Send_GlobalLocation( Location* loc, bool add )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_GlobalLocation( loc, add );
}
void Critter::Send_GlobalMapFog( ushort zx, ushort zy, uchar fog )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_GlobalMapFog( zx, zy, fog );
}
void Critter::Send_AllParams()
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AllParams();
}
void Critter::Send_Param( ushort num_param )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Param( num_param );
}
void Critter::Send_ParamOther( ushort num_param, int val )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_ParamOther( num_param, val );
}
void Critter::Send_CritterParam( Critter* cr, ushort num_param, int val )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_CritterParam( cr, num_param, val );
}
void Critter::Send_Talk()
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Talk();
}
void Critter::Send_GameInfo( Map* map )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_GameInfo( map );
}
void Critter::Send_Text( Critter* from_cr, const char* s_str, uchar how_say )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Text( from_cr, s_str, how_say );
}
void Critter::Send_TextEx( uint from_id, const char* s_str, ushort str_len, uchar how_say, ushort intellect, bool unsafe_text )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_TextEx( from_id, s_str, str_len, how_say, intellect, unsafe_text );
}
void Critter::Send_TextMsg( Critter* from_cr, uint str_num, uchar how_say, ushort num_msg )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_TextMsg( from_cr, str_num, how_say, num_msg );
}
void Critter::Send_TextMsg( uint from_id, uint str_num, uchar how_say, ushort num_msg )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_TextMsg( from_id, str_num, how_say, num_msg );
}
void Critter::Send_TextMsgLex( Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_TextMsgLex( from_cr, num_str, how_say, num_msg, lexems );
}
void Critter::Send_TextMsgLex( uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_TextMsgLex( from_id, num_str, how_say, num_msg, lexems );
}
void Critter::Send_Action( Critter* from_cr, int action, int action_ext, Item* item )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Action( from_cr, action, action_ext, item );
}
void Critter::Send_Knockout( Critter* from_cr, uint anim2begin, uint anim2idle, ushort knock_hx, ushort knock_hy )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Knockout( from_cr, anim2begin, anim2idle, knock_hx, knock_hy );
}
void Critter::Send_MoveItem( Critter* from_cr, Item* item, uchar action, uchar prev_slot )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_MoveItem( from_cr, item, action, prev_slot );
}
void Critter::Send_ItemData( Critter* from_cr, uchar slot, Item* item, bool ext_data )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_ItemData( from_cr, slot, item, ext_data );
}
void Critter::Send_Animate( Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Animate( from_cr, anim1, anim2, item, clear_sequence, delay_play );
}
void Critter::Send_SetAnims( Critter* from_cr, int cond, uint anim1, uint anim2 )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_SetAnims( from_cr, cond, anim1, anim2 );
}
void Critter::Send_CombatResult( uint* combat_res, uint len )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_CombatResult( combat_res, len );
}
void Critter::Send_Quest( uint num )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Quest( num );
}
void Critter::Send_Quests( UIntVec& nums )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Quests( nums );
}
void Critter::Send_HoloInfo( bool clear, ushort offset, ushort count )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_HoloInfo( clear, offset, count );
}
void Critter::Send_AutomapsInfo( void* locs_vec, Location* loc )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AutomapsInfo( locs_vec, loc );
}
void Critter::Send_Follow( uint rule, uchar follow_type, ushort map_pid, uint follow_wait )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Follow( rule, follow_type, map_pid, follow_wait );
}
void Critter::Send_Effect( ushort eff_pid, ushort hx, ushort hy, ushort radius )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Effect( eff_pid, hx, hy, radius );
}
void Critter::Send_FlyEffect( ushort eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_FlyEffect( eff_pid, from_crid, to_crid, from_hx, from_hy, to_hx, to_hy );
}
void Critter::Send_PlaySound( uint crid_synchronize, const char* sound_name )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_PlaySound( crid_synchronize, sound_name );
}
void Critter::Send_PlaySoundType( uint crid_synchronize, uchar sound_type, uchar sound_type_ext, uchar sound_id, uchar sound_id_ext )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_PlaySoundType( crid_synchronize, sound_type, sound_type_ext, sound_id, sound_id_ext );
}
void Critter::Send_CritterLexems( Critter* cr )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_CritterLexems( cr );
}

void Critter::SendA_Move( uint move_params )
{
    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Move( this, move_params );
    }
}

void Critter::SendA_XY()
{
    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_XY( this );
    }
}

void Critter::SendA_Action( int action, int action_ext, Item* item )
{
    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Action( this, action, action_ext, item );
    }
}

void Critter::SendAA_Action( int action, int action_ext, Item* item )
{
    if( IsPlayer() )
        Send_Action( this, action, action_ext, item );

    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Action( this, action, action_ext, item );
    }
}

void Critter::SendA_Knockout( uint anim2begin, uint anim2idle, ushort knock_hx, ushort knock_hy )
{
    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Knockout( this, anim2begin, anim2idle, knock_hx, knock_hy );
    }
}

void Critter::SendAA_MoveItem( Item* item, uchar action, uchar prev_slot )
{
    if( IsPlayer() )
        Send_MoveItem( this, item, action, prev_slot );

    if( VisCr.empty() )
        return;
    SyncLockCritters( false, true );

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_MoveItem( this, item, action, prev_slot );
    }
}

void Critter::SendAA_ItemData( Item* item )
{
    if( IsPlayer() )
        Send_AddItem( item );

    uchar slot = item->AccCritter.Slot;
    if( !VisCr.empty() && slot != SLOT_INV && SlotEnabled[ slot ] )
    {
        if( SlotDataSendEnabled[ slot ] )
        {
            int script = SlotDataSendScript[ slot ];
            if( !script )
            {
                // Send all data
                for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
                {
                    Critter* cr = *it;
                    if( cr->IsPlayer() )
                        cr->Send_ItemData( this, slot, item, true );
                }
            }
            else
            {
                SyncLockCritters( false, true );

                // Send all data if condition passably, else send half
                for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
                {
                    Critter* cr = *it;
                    if( cr->IsPlayer() )
                    {
                        if( RunSlotDataSendScript( script, slot, item, this, cr ) )
                            cr->Send_ItemData( this, slot, item, true );
                        else
                            cr->Send_ItemData( this, slot, item, false );
                    }
                }
            }
        }
        else
        {
            // Send half data
            for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
            {
                Critter* cr = *it;
                if( cr->IsPlayer() )
                    cr->Send_ItemData( this, slot, item, false );
            }
        }
    }
}

void Critter::SendAA_Animate( uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play )
{
    if( IsPlayer() )
        Send_Animate( this, anim1, anim2, item, clear_sequence, delay_play );

    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Animate( this, anim1, anim2, item, clear_sequence, delay_play );
    }
}

void Critter::SendAA_SetAnims( int cond, uint anim1, uint anim2 )
{
    if( IsPlayer() )
        Send_SetAnims( this, cond, anim1, anim2 );

    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_SetAnims( this, cond, anim1, anim2 );
    }
}

void Critter::SendA_GlobalInfo( GlobalMapGroup* group, uchar info_flags )
{
    if( !group )
        return;

    CrVec cr_group = group->CritMove;
    for( auto it = cr_group.begin(), end = cr_group.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
        {
            SYNC_LOCK( cr );
            cr->Send_GlobalInfo( info_flags );
        }
    }
}

void Critter::SendAA_Text( CrVec& to_cr, const char* str, uchar how_say, bool unsafe_text )
{
    if( !str || !str[ 0 ] )
        return;

    ushort str_len = Str::Length( str );
    uint   from_id = GetId();
    ushort intellect = ( how_say >= SAY_NORM && how_say <= SAY_RADIO ? IntellectCacheValue : 0 );

    if( IsPlayer() )
        Send_TextEx( from_id, str, str_len, how_say, intellect, unsafe_text );

    if( to_cr.empty() )
        return;

    int dist = -1;
    if( how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD )
        dist = GameOpt.ShoutDist + GetMultihex();
    else if( how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD )
        dist = GameOpt.WhisperDist + GetMultihex();

    for( auto it = to_cr.begin(), end = to_cr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr == this || !cr->IsPlayer() )
            continue;

        // SYNC_LOCK(cr);

        if( dist == -1 )
            cr->Send_TextEx( from_id, str, str_len, how_say, intellect, unsafe_text );
        else if( CheckDist( Data.HexX, Data.HexY, cr->Data.HexX, cr->Data.HexY, dist + cr->GetMultihex() ) )
            cr->Send_TextEx( from_id, str, str_len, how_say, intellect, unsafe_text );
    }
}

void Critter::SendAA_Msg( CrVec& to_cr, uint num_str, uchar how_say, ushort num_msg )
{
    if( IsPlayer() )
        Send_TextMsg( this, num_str, how_say, num_msg );

    if( to_cr.empty() )
        return;

    int dist = -1;
    if( how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD )
        dist = GameOpt.ShoutDist + GetMultihex();
    else if( how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD )
        dist = GameOpt.WhisperDist + GetMultihex();

    for( auto it = to_cr.begin(), end = to_cr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr == this )
            continue;
        if( !cr->IsPlayer() )
            continue;

        // SYNC_LOCK(cr);

        if( dist == -1 )
            cr->Send_TextMsg( this, num_str, how_say, num_msg );
        else if( CheckDist( Data.HexX, Data.HexY, cr->Data.HexX, cr->Data.HexY, dist + cr->GetMultihex() ) )
            cr->Send_TextMsg( this, num_str, how_say, num_msg );
    }
}

void Critter::SendAA_MsgLex( CrVec& to_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems )
{
    if( IsPlayer() )
        Send_TextMsgLex( this, num_str, how_say, num_msg, lexems );

    if( to_cr.empty() )
        return;

    int dist = -1;
    if( how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD )
        dist = GameOpt.ShoutDist + GetMultihex();
    else if( how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD )
        dist = GameOpt.WhisperDist + GetMultihex();

    for( auto it = to_cr.begin(), end = to_cr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr == this )
            continue;
        if( !cr->IsPlayer() )
            continue;

        // SYNC_LOCK(cr);

        if( dist == -1 )
            cr->Send_TextMsgLex( this, num_str, how_say, num_msg, lexems );
        else if( CheckDist( Data.HexX, Data.HexY, cr->Data.HexX, cr->Data.HexY, dist + cr->GetMultihex() ) )
            cr->Send_TextMsgLex( this, num_str, how_say, num_msg, lexems );
    }
}

void Critter::SendA_Dir()
{
    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Dir( this );
    }
}

void Critter::SendA_Follow( uchar follow_type, ushort map_pid, uint follow_wait )
{
    if( VisCr.empty() )
        return;
    SyncLockCritters( false, true );

    int dist = FOLLOW_DIST + GetMultihex();
    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( !cr->IsPlayer() )
            continue;
        if( cr->GetFollowCrId() != GetId() )
            continue;
        if( cr->IsDead() || cr->IsKnockout() )
            continue;
        if( !CheckDist( Data.LastHexX, Data.LastHexY, cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex() ) )
            continue;
        cr->Send_Follow( GetId(), follow_type, map_pid, follow_wait );
    }
}

void Critter::SendA_ParamOther( ushort num_param, int val )
{
    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_CritterParam( this, num_param, val );
    }
}

void Critter::SendA_ParamCheck( ushort num_param )
{
    if( VisCr.empty() )
        return;

    int script = ParamsSendScript[ num_param ];
    if( !script )
    {
        int val = Data.Params[ num_param ];
        for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( cr->IsPlayer() )
                cr->Send_CritterParam( this, num_param, val );
        }
    }
    else
    {
        SyncLockCritters( false, true );

        for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( cr->IsPlayer() )
                cr->Send_CritterParam( this, num_param, RunParamsSendScript( script, num_param, this, cr ) );
        }
    }
}

void Critter::Send_AddAllItems()
{
    if( !IsPlayer() )
        return;

    SyncLockItems();

    Client* cl = (Client*) this;
    BOUT_BEGIN( cl );
    cl->Bout << NETMSG_CLEAR_ITEMS;
    BOUT_END( cl );

    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
        Send_AddItem( *it );

    BOUT_BEGIN( cl );
    cl->Bout << NETMSG_ALL_ITEMS_SEND;
    BOUT_END( cl );
}

void Critter::Send_AllQuests()
{
    if( !IsPlayer() )
        return;

    UIntVec quests;
    VarMngr.GetQuestVars( GetId(), quests );
    Send_Quests( quests );
}

void Critter::Send_AllAutomapsInfo()
{
    if( !IsPlayer() )
        return;

    CritDataExt* data_ext = GetDataExt();
    if( !data_ext )
        return;

    LocVec locs;
    for( ushort i = 0; i < data_ext->LocationsCount; i++ )
    {
        uint      loc_id = data_ext->LocationsId[ i ];
        Location* loc = MapMngr.GetLocation( loc_id );
        if( loc && loc->IsAutomaps() )
            locs.push_back( loc );
    }

    Send_AutomapsInfo( &locs, NULL );
}

void Critter::SendMessage( int num, int val, int to )
{
    switch( to )
    {
    case MESSAGE_TO_VISIBLE_ME:
    {
        CrVec critters = VisCr;
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            SYNC_LOCK( cr );
            cr->EventMessage( this, num, val );
        }
    }
    break;
    case MESSAGE_TO_IAM_VISIBLE:
    {
        CrVec critters = VisCrSelf;
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            SYNC_LOCK( cr );
            cr->EventMessage( this, num, val );
        }
    }
    break;
    case MESSAGE_TO_ALL_ON_MAP:
    {
        Map* map = MapMngr.GetMap( GetMap() );
        if( !map )
            return;

        CrVec critters;
        map->GetCritters( critters, true );
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            cr->EventMessage( this, num, val );
        }
    }
    break;
    default:
        break;
    }
}

void Critter::RefreshName()
{
    if( IsPlayer() )
    {
        NameStr = ( (Client*) this )->Name;
    }
    else
    {
        uint        dlg_pack_id = Data.Params[ ST_DIALOG_ID ];
        DialogPack* dlg_pack = ( dlg_pack_id > 0 ? DlgMngr.GetDialogPack( dlg_pack_id ) : NULL );
        char        buf[ MAX_FOTEXT ];
        if( dlg_pack )
            NameStr = Str::Format( buf, "Npc (%s, %u)", dlg_pack->PackName.c_str(), GetId() );
        else
            NameStr = Str::Format( buf, "Npc (%u)", GetId() );
    }
}

const char* Critter::GetInfo()
{
//	static char buf[1024];
//	sprintf(buf,"Name<%s>, Id<%u>, MapPid<%u>, HexX<%u>, HexY<%u>",GetName(),GetId(),GetProtoMap(),GetHexX(),GetHexY());
//	return buf;
    return GetName();
}

int Critter::GetParam( uint index )
{
    if( ParamsGetScript[ index ] && Script::PrepareContext( ParamsGetScript[ index ], _FUNC_, "" ) )
    {
        Script::SetArgObject( this );
        Script::SetArgUInt( index - ( ParametersOffset[ index ] ? ParametersMin[ index ] : 0 ) );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return Data.Params[ index ];
}

void Critter::ChangeParam( uint index )
{
    if( !ParamsIsChanged[ index ] && ParamLocked != (int) index )
    {
        ParamsChanged.push_back( index );
        ParamsChanged.push_back( Data.Params[ index ] );
        ParamsIsChanged[ index ] = true;
    }
}

IntVec CallChange;
void Critter::ProcessChangedParams()
{
    if( ParamsChanged.size() )
    {
        if( IsNotValid )
        {
            ParamsChanged.clear();
            return;
        }

        CallChange.clear();
        for( uint i = 0, j = (uint) ParamsChanged.size(); i < j; i += 2 )
        {
            int index = ParamsChanged[ i ];
            int old_val = ParamsChanged[ i + 1 ];
            if( Data.Params[ index ] != old_val )
            {
                if( ParamsChangeScript[ index ] )
                {
                    CallChange.push_back( ParamsChangeScript[ index ] );
                    CallChange.push_back( index );
                    CallChange.push_back( old_val );
                }
                else
                {
                    Send_Param( index );
                    if( ParamsSendEnabled[ index ] )
                        SendA_ParamCheck( index );
                }
            }
            ParamsIsChanged[ index ] = false;
        }
        ParamsChanged.clear();

        if( CallChange.size() )
        {
            for( uint i = 0, j = (uint) CallChange.size(); i < j; i += 3 )
            {
                uint index = CallChange[ i + 1 ];
                ParamLocked = index;
                if( Script::PrepareContext( CallChange[ i ], _FUNC_, GetInfo() ) )
                {
                    Script::SetArgObject( this );
                    Script::SetArgUInt( index - ( ParametersOffset[ index ] ? ParametersMin[ index ] : 0 ) );
                    Script::SetArgUInt( CallChange[ i + 2 ] );
                    Script::RunPrepared();
                }
                ParamLocked = -1;
                Send_Param( index );
                if( ParamsSendEnabled[ index ] )
                    SendA_ParamCheck( index );
            }
        }
    }
}

bool Critter::IsCanWalk()
{
    return CritType::IsCanWalk( GetCrType() ) && !IsRawParam( MODE_NO_WALK );
}

bool Critter::IsCanRun()
{
    return CritType::IsCanRun( GetCrType() ) && !IsRawParam( MODE_NO_RUN );
}

uint Critter::GetTimeWalk()
{
    int walk_time = GetRawParam( ST_WALK_TIME );
    if( walk_time <= 0 )
        walk_time = CritType::GetTimeWalk( GetCrType() );
    return walk_time > 0 ? walk_time : 400;
}

uint Critter::GetTimeRun()
{
    int walk_time = GetRawParam( ST_RUN_TIME );
    if( walk_time <= 0 )
        walk_time = CritType::GetTimeRun( GetCrType() );
    return walk_time > 0 ? walk_time : 200;
}

uint Critter::GetItemsWeight()
{
    uint res = 0;
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( !item->IsHidden() )
            res += item->GetWeight();
    }
    return res;
}

uint Critter::GetItemsVolume()
{
    uint res = 0;
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( !item->IsHidden() )
            res += item->GetVolume();
    }
    return res;
}

int Critter::GetFreeWeight()
{
    int cur_weight = GetItemsWeight();
    int max_weight = GetParam( ST_CARRY_WEIGHT );
    return max_weight - cur_weight;
}

int Critter::GetFreeVolume()
{
    int cur_volume = GetItemsVolume();
    int max_volume = CRITTER_INV_VOLUME;
    return max_volume - cur_volume;
}

/*
   void Critter::SetPerkSafe(uint index, int val)
   {
        if(Data.Pe[num]==val) return;

        if(num>=0 && num<PERK_COUNT)
        {
                // Up
                if(Data.Pe[num]<val)
                {
                        val=val-Data.Pe[num];
                        while(val)
                        {
                                Perks::Up(this,num,PERK_ALL,true);
                                val--;
                        }
                }
                // Down
                else
                {
                        val=Data.Pe[num]-val;
                        while(val)
                        {
                                Perks::Down(this,num);
                                val--;
                        }
                }
        }
        else if(num>=DAMAGE_BEGIN && num<=DAMAGE_END)
        {
                // Injure, uninjure
                Data.Pe[num]=val;
                SendPerk(num);

                if(num==PE_DAMAGE_EYE) ProcessVisibleCritters();
        }
        else if(num>=MODE_BEGIN && num<=MODE_END)
        {
                // Modes
                Data.Pe[num]=val;
                SendPerk(num);

                if(num==MODE_HIDE && GetMap()) ProcessVisibleCritters();
        }
        else if(num>=TRAIT_BEGIN && num<=TRAIT_END)
        {
                // Trait
                Data.Pe[num]=val;
                SendPerk(num);
        }
   }*/


bool Critter::CheckMyTurn( Map* map )
{
    if( !IsTurnBased() )
        return true;
    if( !map )
        map = MapMngr.GetMap( GetMap() );
    if( !map || !map->IsTurnBasedOn || map->IsCritterTurn( this ) )
        return true;
    return false;
}

/************************************************************************/
/* Timeouts                                                             */
/************************************************************************/

void Critter::SetTimeout( int timeout, uint game_seconds )
{
    ChangeParam( timeout );
    if( game_seconds )
        Data.Params[ timeout ] = GameOpt.FullSecond + game_seconds;
    else
        Data.Params[ timeout ] = 0;
}

bool Critter::IsTransferTimeouts( bool send )
{
    if( GetParam( TO_TRANSFER ) )
    {
        if( send )
            Send_TextMsg( this, STR_TIMEOUT_TRANSFER_WAIT, SAY_NETMSG, TEXTMSG_GAME );
        return true;
    }
    if( GetParam( TO_BATTLE ) )
    {
        if( send )
            Send_TextMsg( this, STR_TIMEOUT_BATTLE_WAIT, SAY_NETMSG, TEXTMSG_GAME );
        return true;
    }
    return false;
}

/************************************************************************/
/* Enemy stack                                                          */
/************************************************************************/

void Critter::AddEnemyInStack( uint crid )
{
    // Find already
    int stack_count = MIN( Data.EnemyStackCount, MAX_ENEMY_STACK );
    for( int i = 0; i < stack_count; i++ )
    {
        if( Data.EnemyStack[ i ] == crid )
        {
            for( int j = i; j < stack_count - 1; j++ )
            {
                Data.EnemyStack[ j ] = Data.EnemyStack[ j + 1 ];
            }
            Data.EnemyStack[ stack_count - 1 ] = crid;
            return;
        }
    }
    // Add
    for( int i = 0; i < stack_count - 1; i++ )
    {
        Data.EnemyStack[ i ] = Data.EnemyStack[ i + 1 ];
    }
    Data.EnemyStack[ stack_count - 1 ] = crid;
}

bool Critter::CheckEnemyInStack( uint crid )
{
    if( Data.Params[ MODE_NO_ENEMY_STACK ] )
        return false;

    int stack_count = MIN( Data.EnemyStackCount, MAX_ENEMY_STACK );
    for( int i = 0; i < stack_count; i++ )
    {
        if( Data.EnemyStack[ i ] == crid )
            return true;
    }
    return false;
}

void Critter::EraseEnemyInStack( uint crid )
{
    int stack_count = MIN( Data.EnemyStackCount, MAX_ENEMY_STACK );
    for( int i = 0; i < stack_count; i++ )
    {
        if( Data.EnemyStack[ i ] == crid )
        {
            for( int j = i; j > 0; j-- )
            {
                Data.EnemyStack[ j ] = Data.EnemyStack[ j - 1 ];
            }
            Data.EnemyStack[ 0 ] = 0;
            break;
        }
    }
}

Critter* Critter::ScanEnemyStack()
{
    if( Data.Params[ MODE_NO_ENEMY_STACK ] )
        return NULL;

    int stack_count = MIN( Data.EnemyStackCount, MAX_ENEMY_STACK );
    for( int i = stack_count - 1; i >= 0; i-- )
    {
        uint crid = Data.EnemyStack[ i ];
        if( crid )
        {
            Critter* enemy = GetCritSelf( crid, true );
            if( enemy && !enemy->IsDead() )
                return enemy;
        }
    }
    return NULL;
}

/************************************************************************/
/* Misc events                                                          */
/************************************************************************/

void Critter::AddCrTimeEvent( uint func_num, uint rate, uint duration, int identifier )
{
    if( duration )
        duration += GameOpt.FullSecond;
    auto it = CrTimeEvents.begin(), end = CrTimeEvents.end();
    for( ; it != end; ++it )
    {
        CrTimeEvent& cte = *it;
        if( duration < cte.NextTime )
            break;
    }

    CrTimeEvent cte;
    cte.FuncNum = func_num;
    cte.Rate = rate;
    cte.NextTime = duration;
    cte.Identifier = identifier;
    CrTimeEvents.insert( it, cte );
}

void Critter::EraseCrTimeEvent( int index )
{
    if( index >= (int) CrTimeEvents.size() )
        return;
    CrTimeEvents.erase( CrTimeEvents.begin() + index );
}

void Critter::ContinueTimeEvents( int offs_time )
{
    for( auto it = CrTimeEvents.begin(), end = CrTimeEvents.end(); it != end; ++it )
    {
        CrTimeEvent& cte = *it;
        if( cte.NextTime )
            cte.NextTime += offs_time;
    }
}

void Critter::Delete()
{
    if( IsPlayer() )
        delete (Client*) this;
    else
        delete (Npc*) this;
}

/************************************************************************/
/* Client                                                               */
/************************************************************************/

#if !defined ( USE_LIBEVENT ) || defined ( LIBEVENT_TIMEOUTS_WORKAROUND )
Client::SendCallback Client::SendData = NULL;
#endif

Client::Client(): ZstrmInit( false ), Access( ACCESS_DEFAULT ), pingOk( true ), LanguageMsg( 0 ),
                  GameState( STATE_NONE ), IsDisconnected( false ), DisconnectTick( 0 ), DisableZlib( false ),
                  LastSendScoresTick( 0 ), LastSendCraftTick( 0 ), LastSendEntrancesTick( 0 ), LastSendEntrancesLocId( 0 ),
                  ScreenCallbackBindId( 0 ), LastSendedMapTick( 0 ), RadioMessageSended( 0 ),
                  UpdateFileIndex( -1 ), UpdateFilePortion( 0 )
{
    CritterIsNpc = false;
    MEMORY_PROCESS( MEMORY_CLIENT, sizeof( Client ) + sizeof( GlobalMapGroup ) + 40 + sizeof( Item ) * 2 );

    SETFLAG( Flags, FCRIT_PLAYER );
    Sock = INVALID_SOCKET;
    memzero( Name, sizeof( Name ) );
    memzero( PassHash, sizeof( PassHash ) );
    Str::Copy( Name, "err" );
    pingNextTick = Timer::FastTick() + PING_CLIENT_LIFE_TIME;
    Talk.Clear();
    talkNextTick = Timer::GameTick() + PROCESS_TALK_TICK;
    LastActivityTime = Timer::FastTick();
    LastSay[ 0 ] = 0;
    LastSayEqualCount = 0;
    memzero( UID, sizeof( UID ) );

    #if defined ( USE_LIBEVENT )
    NetIOArgPtr = NULL;
    #else // IOCP
    MEMORY_PROCESS( MEMORY_CLIENT, WSA_BUF_SIZE * 2 );
    NetIOIn = new NetIOArg();
    memzero( &NetIOIn->OV, sizeof( NetIOIn->OV ) );
    NetIOIn->PClient = this;
    NetIOIn->Buffer.buf = new char[ WSA_BUF_SIZE ];
    NetIOIn->Buffer.len = WSA_BUF_SIZE;
    NetIOIn->Operation = WSAOP_RECV;
    NetIOIn->Flags = 0;
    NetIOIn->Bytes = 0;
    NetIOOut = new NetIOArg();
    memzero( &NetIOOut->OV, sizeof( NetIOOut->OV ) );
    NetIOOut->PClient = this;
    NetIOOut->Buffer.buf = new char[ WSA_BUF_SIZE ];
    NetIOOut->Buffer.len = WSA_BUF_SIZE;
    NetIOOut->Operation = WSAOP_FREE;
    NetIOOut->Flags = 0;
    NetIOOut->Bytes = 0;
    #endif
}

Client::~Client()
{
    MEMORY_PROCESS( MEMORY_CLIENT, -(int) ( sizeof( Client ) + sizeof( GlobalMapGroup ) + 40 + sizeof( Item ) * 2 ) );
    if( DataExt )
    {
        MEMORY_PROCESS( MEMORY_CLIENT, -(int) sizeof( CritDataExt ) );
        SAFEDEL( DataExt );
    }
    if( ZstrmInit )
    {
        deflateEnd( &Zstrm );
        ZstrmInit = false;
    }

    #if defined ( USE_LIBEVENT )
    SAFEDEL( NetIOArgPtr );
    #else // IOCP
    MEMORY_PROCESS( MEMORY_CLIENT, -(int) ( WSA_BUF_SIZE * 2 ) );
    if( NetIOIn )
    {
        SAFEDELA( NetIOIn->Buffer.buf );
        SAFEDEL( NetIOIn );
    }
    if( NetIOOut )
    {
        SAFEDELA( NetIOOut->Buffer.buf );
        SAFEDEL( NetIOOut );
    }
    #endif
}

void Client::Shutdown()
{
    if( Sock == INVALID_SOCKET )
        return;

    #if defined ( USE_LIBEVENT )
    # if defined ( LIBEVENT_TIMEOUTS_WORKAROUND )
    NetIOArgPtr->BEVLocker.Lock();
    bufferevent_free( NetIOArgPtr->BEV );
    NetIOArgPtr->BEV = NULL;
    NetIOArgPtr->BEVLocker.Unlock();
    # else
    bufferevent_free( NetIOArgPtr->BEV );
    # endif

    shutdown( Sock, SD_BOTH );
    closesocket( Sock );
    Sock = INVALID_SOCKET;

    Release();
    #else
    NetIOIn->Locker.Lock();
    NetIOOut->Locker.Lock();
    shutdown( Sock, SD_BOTH );
    closesocket( Sock );
    Sock = INVALID_SOCKET;
    NetIOOut->Locker.Unlock();
    NetIOIn->Locker.Unlock();
    #endif
}

uint Client::GetIp()
{
    return From.sin_addr.s_addr;
}

const char* Client::GetIpStr()
{
    return inet_ntoa( From.sin_addr );
}

ushort Client::GetPort()
{
    return From.sin_port;
}

void Client::PingClient()
{
    if( !pingOk )
    {
        Disconnect();
        Bout.LockReset();
        return;
    }

    BOUT_BEGIN( this );
    Bout << NETMSG_PING;
    Bout << (uchar) PING_CLIENT;
    BOUT_END( this );

    pingNextTick = Timer::FastTick() + PING_CLIENT_LIFE_TIME;
    pingOk = false;
}

void Client::Send_AddCritter( Critter* cr )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    bool is_npc = cr->IsNpc();
    uint msg = ( is_npc ? NETMSG_ADD_NPC : NETMSG_ADD_PLAYER );
    uint msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( uint ) + sizeof( uint ) + sizeof( ushort ) * 2 +
                   sizeof( uchar ) + sizeof( uchar ) + sizeof( uint ) * 6 + sizeof( uint ) + sizeof( short ) +
                   ( is_npc ? sizeof( ushort ) + sizeof( uint ) : UTF8_BUF_SIZE( MAX_NAME ) ) + ParamsSendMsgLen;
    int dialog_id = ( is_npc ? cr->Data.Params[ ST_DIALOG_ID ] : 0 );

    BOUT_BEGIN( this );
    Bout << msg;
    Bout << msg_len;
    Bout << cr->GetId();
    Bout << cr->Data.BaseType;
    Bout << cr->GetHexX();
    Bout << cr->GetHexY();
    Bout << cr->GetDir();
    Bout << cr->Data.Cond;
    Bout << cr->Data.Anim1Life;
    Bout << cr->Data.Anim1Knockout;
    Bout << cr->Data.Anim1Dead;
    Bout << cr->Data.Anim2Life;
    Bout << cr->Data.Anim2Knockout;
    Bout << cr->Data.Anim2Dead;
    Bout << cr->Flags;
    Bout << cr->Data.Multihex;

    if( is_npc )
    {
        Npc* npc = (Npc*) cr;
        Bout << npc->GetProtoId();
        Bout << dialog_id;
    }
    else
    {
        Client* cl = (Client*) cr;
        Bout.Push( cl->Name, UTF8_BUF_SIZE( MAX_NAME ) );
    }

    Bout << ParamsSendCount;
    for( auto it = ParamsSend.begin(), end = ParamsSend.end(); it != end; ++it )
    {
        ushort index = *it;
        Bout << index;

        int script = ParamsSendScript[ index ];
        if( !script )
            Bout << cr->Data.Params[ index ];
        else
            Bout << RunParamsSendScript( script, index, cr, this );
        #pragma MESSAGE("RunParamsSendScript call before BOUT_END, may be unsafe.")
    }
    BOUT_END( this );

    if( cr != this )
        Send_MoveItem( cr, NULL, ACTION_REFRESH, 0 );
    if( cr->IsLexems() )
        Send_CritterLexems( cr );
}

void Client::Send_RemoveCritter( Critter* cr )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_REMOVE_CRITTER;
    Bout << cr->GetId();
    BOUT_END( this );
}

void Client::Send_LoadMap( Map* map )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    ushort pid_map = 0;
    int    map_time = -1;
    uchar  map_rain = 0;
    uint   hash_tiles = 0;
    uint   hash_walls = 0;
    uint   hash_scen = 0;

    if( !map )
        map = MapMngr.GetMap( GetMap(), false );
    if( map )
    {
        pid_map = map->GetPid();
        map_time = map->GetTime();
        map_rain = map->GetRain();
        hash_tiles = map->Proto->HashTiles;
        hash_walls = map->Proto->HashWalls;
        hash_scen = map->Proto->HashScen;
    }

    BOUT_BEGIN( this );
    Bout << NETMSG_LOADMAP;
    Bout << pid_map;
    Bout << map_time;
    Bout << map_rain;
    Bout << hash_tiles;
    Bout << hash_walls;
    Bout << hash_scen;
    BOUT_END( this );

    GameState = STATE_TRANSFERRING;
}

void Client::Send_Move( Critter* from_cr, uint move_params )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_MOVE;
    Bout << from_cr->GetId();
    Bout << move_params;
    Bout << from_cr->GetHexX();
    Bout << from_cr->GetHexY();
    BOUT_END( this );
}

void Client::Send_Dir( Critter* from_cr )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_DIR;
    Bout << from_cr->GetId();
    Bout << from_cr->GetDir();
    BOUT_END( this );
}

void Client::Send_Action( Critter* from_cr, int action, int action_ext, Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    Send_XY( from_cr );
    if( item )
        Send_SomeItem( item );

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_ACTION;
    Bout << from_cr->GetId();
    Bout << action;
    Bout << action_ext;
    Bout << (bool) ( item ? true : false );
    BOUT_END( this );
}

void Client::Send_Knockout( Critter* from_cr, uint anim2begin, uint anim2idle, ushort knock_hx, ushort knock_hy )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    Send_XY( from_cr );

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_KNOCKOUT;
    Bout << from_cr->GetId();
    Bout << anim2begin;
    Bout << anim2idle;
    Bout << knock_hx;
    Bout << knock_hy;
    BOUT_END( this );
}

void Client::Send_MoveItem( Critter* from_cr, Item* item, uchar action, uchar prev_slot )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    Send_XY( from_cr );
    if( item )
        Send_SomeItem( item );

    uint        msg = NETMSG_CRITTER_MOVE_ITEM;
    uint        msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( uint ) + sizeof( action ) + sizeof( prev_slot ) + sizeof( bool );

    uchar       slots_data_count = 0;
    uchar       slots_data_ext_count = 0;
    ItemPtrVec& inv = from_cr->GetInventory();
    for( auto it = inv.begin(), end = inv.end(); it != end; ++it )
    {
        Item* item_ = *it;
        uchar slot = item_->AccCritter.Slot;
        if( slot != SLOT_INV && SlotEnabled[ slot ] )
        {
            int script = SlotDataSendScript[ slot ];
            if( SlotDataSendEnabled[ slot ] && ( !script || RunSlotDataSendScript( script, slot, item_, from_cr, this ) ) )
            {
                SlotEnabledCacheDataExt[ slots_data_ext_count ] = item_;
                slots_data_ext_count++;
            }
            else
            {
                SlotEnabledCacheData[ slots_data_count ] = item_;
                slots_data_count++;
            }
        }
    }
    msg_len += sizeof( uchar ) + ( sizeof( uchar ) + sizeof( uint ) + sizeof( ushort ) + sizeof( Item::ItemData ) ) * ( slots_data_count + slots_data_ext_count );

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_MOVE_ITEM;
    Bout << msg_len;
    Bout << from_cr->GetId();
    Bout << action;
    Bout << prev_slot;
    Bout << (bool) ( item ? true : false );

    // Slots
    Bout << (uchar) ( slots_data_count + slots_data_ext_count );
    for( uchar i = 0; i < slots_data_count; i++ )
    {
        Item* item_ = SlotEnabledCacheData[ i ];
        Bout << item_->AccCritter.Slot;
        Bout << item_->GetId();
        Bout << item_->GetProtoId();
        Bout.Push( (char*) &item_->Data, Item::ItemData::SendMask[ ITEM_DATA_MASK_CRITTER ], sizeof( item_->Data ) );
    }
    for( uchar i = 0; i < slots_data_ext_count; i++ )
    {
        Item* item_ = SlotEnabledCacheDataExt[ i ];
        Bout << item_->AccCritter.Slot;
        Bout << item_->GetId();
        Bout << item_->GetProtoId();
        Bout.Push( (char*) &item_->Data, Item::ItemData::SendMask[ ITEM_DATA_MASK_CRITTER_EXT ], sizeof( item_->Data ) );
    }
    BOUT_END( this );

    // Lexems
    for( uchar i = 0; i < slots_data_count; i++ )
        if( SlotEnabledCacheData[ i ]->PLexems )
            Send_ItemLexems( SlotEnabledCacheData[ i ] );
    for( uchar i = 0; i < slots_data_ext_count; i++ )
        if( SlotEnabledCacheDataExt[ i ]->PLexems )
            Send_ItemLexems( SlotEnabledCacheDataExt[ i ] );
}

void Client::Send_ItemData( Critter* from_cr, uchar slot, Item* item, bool ext_data )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_ITEM_DATA;
    Bout << from_cr->GetId();
    Bout << slot;
    Bout.Push( (char*) &item->Data, Item::ItemData::SendMask[ ext_data ? ITEM_DATA_MASK_CRITTER_EXT : ITEM_DATA_MASK_CRITTER ], sizeof( item->Data ) );
    BOUT_END( this );
}

void Client::Send_Animate( Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( clear_sequence )
        Send_XY( from_cr );
    if( item )
        Send_SomeItem( item );

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_ANIMATE;
    Bout << from_cr->GetId();
    Bout << anim1;
    Bout << anim2;
    Bout << (bool) ( item ? true : false );
    Bout << clear_sequence;
    Bout << delay_play;
    BOUT_END( this );
}

void Client::Send_SetAnims( Critter* from_cr, int cond, uint anim1, uint anim2 )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_SET_ANIMS;
    Bout << from_cr->GetId();
    Bout << cond;
    Bout << anim1;
    Bout << anim2;
    BOUT_END( this );
}

void Client::Send_AddItemOnMap( Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uchar is_added = item->ViewPlaceOnMap;

    BOUT_BEGIN( this );
    Bout << NETMSG_ADD_ITEM_ON_MAP;
    Bout << item->GetId();
    Bout << item->GetProtoId();
    Bout << item->AccHex.HexX;
    Bout << item->AccHex.HexY;
    Bout << is_added;
    Bout.Push( (char*) &item->Data, Item::ItemData::SendMask[ ITEM_DATA_MASK_MAP ], sizeof( item->Data ) );
    BOUT_END( this );

    if( item->PLexems )
        Send_ItemLexems( item );
}

void Client::Send_ChangeItemOnMap( Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_CHANGE_ITEM_ON_MAP;
    Bout << item->GetId();
    Bout.Push( (char*) &item->Data, Item::ItemData::SendMask[ ITEM_DATA_MASK_MAP ], sizeof( item->Data ) );
    BOUT_END( this );

    if( item->PLexems )
        Send_ItemLexems( item );
}

void Client::Send_EraseItemFromMap( Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_ERASE_ITEM_FROM_MAP;
    Bout << item->GetId();
    Bout << item->ViewPlaceOnMap;
    BOUT_END( this );
}

void Client::Send_AnimateItem( Item* item, uchar from_frm, uchar to_frm )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_ANIMATE_ITEM;
    Bout << item->GetId();
    Bout << from_frm;
    Bout << to_frm;
    BOUT_END( this );
}

void Client::Send_AddItem( Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( item->IsHidden() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_ADD_ITEM;
    Bout << item->GetId();
    Bout << item->GetProtoId();
    Bout << item->AccCritter.Slot;
    Bout.Push( (char*) &item->Data, Item::ItemData::SendMask[ ITEM_DATA_MASK_CHOSEN ], sizeof( item->Data ) );
    BOUT_END( this );

    if( item->PLexems )
        Send_ItemLexems( item );
}

void Client::Send_EraseItem( Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_REMOVE_ITEM;
    Bout << item->GetId();
    BOUT_END( this );
}

void Client::Send_ContainerInfo()
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uchar transfer_type = TRANSFER_CLOSE;
    uint  msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( transfer_type );

    BOUT_BEGIN( this );
    Bout << NETMSG_CONTAINER_INFO;
    Bout << msg_len;
    Bout << transfer_type;
    BOUT_END( this );

    AccessContainerId = 0;
}

void Client::Send_ContainerInfo( Item* item_cont, uchar transfer_type, bool open_screen )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( item_cont->GetType() != ITEM_TYPE_CONTAINER )
        return;

    uint       msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uchar ) + sizeof( uint ) + sizeof( uint ) + sizeof( ushort ) + sizeof( uint );
    ItemPtrVec items;
    item_cont->ContGetAllItems( items, true, true );
    if( items.size() )
        msg_len += (uint) items.size() * ( sizeof( uint ) + sizeof( ushort ) + sizeof( Item::ItemData ) );
    if( open_screen )
        SETFLAG( transfer_type, 0x80 );

    BOUT_BEGIN( this );
    Bout << NETMSG_CONTAINER_INFO;
    Bout << msg_len;
    Bout << transfer_type;
    Bout << uint( 0 );
    Bout << item_cont->GetId();
    Bout << item_cont->GetProtoId();
    Bout << uint( items.size() );

    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        Bout << item->GetId();
        Bout << item->GetProtoId();
        Bout.Push( (char*) &item->Data, Item::ItemData::SendMask[ ITEM_DATA_MASK_CONTAINER ], sizeof( item->Data ) );
    }
    BOUT_END( this );

    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->PLexems )
            Send_ItemLexems( item );
    }

    AccessContainerId = item_cont->GetId();
}

void Client::Send_ContainerInfo( Critter* cr_cont, uchar transfer_type, bool open_screen )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint       msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uchar ) + sizeof( uint ) + sizeof( uint ) + sizeof( ushort ) + sizeof( uint );
    ItemPtrVec items;
    cr_cont->GetInvItems( items, transfer_type, true );
    ushort     barter_k = 0;
    if( transfer_type == TRANSFER_CRIT_BARTER )
    {
        if( cr_cont->GetRawParam( SK_BARTER ) > GetRawParam( SK_BARTER ) )
            barter_k = cr_cont->GetRawParam( SK_BARTER ) - GetRawParam( SK_BARTER );
        barter_k = CLAMP( barter_k, 5, 95 );
        if( cr_cont->Data.Params[ ST_FREE_BARTER_PLAYER ] == (int) GetId() )
            barter_k = 0;
    }

    if( open_screen )
        SETFLAG( transfer_type, 0x80 );
    msg_len += (uint) items.size() * ( sizeof( uint ) + sizeof( ushort ) + sizeof( Item::ItemData ) );

    BOUT_BEGIN( this );
    Bout << NETMSG_CONTAINER_INFO;
    Bout << msg_len;
    Bout << transfer_type;
    Bout << (uint) Talk.TalkTime;
    Bout << cr_cont->GetId();
    Bout << barter_k;
    Bout << (uint) items.size();

    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        Bout << item->GetId();
        Bout << item->GetProtoId();
        Bout.Push( (char*) &item->Data, Item::ItemData::SendMask[ ITEM_DATA_MASK_CONTAINER ], sizeof( item->Data ) );
    }
    BOUT_END( this );

    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->PLexems )
            Send_ItemLexems( item );
    }

    AccessContainerId = cr_cont->GetId();
}

#define SEND_LOCATION_SIZE    ( 16 )
void Client::Send_GlobalInfo( uchar info_flags )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( LockMapTransfers )
        return;

    CritDataExt* data_ext = GetDataExt();
    if( !data_ext )
        return;

    Item* car = ( FLAG( info_flags, GM_INFO_GROUP_PARAM ) ? GroupMove->GetCar() : NULL );

    // Calculate length of message
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( info_flags );

    ushort loc_count = data_ext->LocationsCount;
    if( FLAG( info_flags, GM_INFO_LOCATIONS ) )
        msg_len += sizeof( loc_count ) + SEND_LOCATION_SIZE * loc_count;

    if( FLAG( info_flags, GM_INFO_GROUP_PARAM ) )
        msg_len += sizeof( ushort ) * 4 + sizeof( uint ) + sizeof( bool ) + sizeof( uint );

    if( FLAG( info_flags, GM_INFO_ZONES_FOG ) )
        msg_len += GM_ZONES_FOG_SIZE;

    // Parse message
    BOUT_BEGIN( this );
    Bout << NETMSG_GLOBAL_INFO;
    Bout << msg_len;
    Bout << info_flags;

    if( FLAG( info_flags, GM_INFO_LOCATIONS ) )
    {
        Bout << loc_count;

        char empty_loc[ SEND_LOCATION_SIZE ] = { 0, 0, 0, 0 };
        for( int i = 0; i < data_ext->LocationsCount;)
        {
            uint      loc_id = data_ext->LocationsId[ i ];
            Location* loc = MapMngr.GetLocation( loc_id );
            if( loc && !loc->Data.ToGarbage )
            {
                i++;
                Bout << loc_id;
                Bout << loc->GetPid();
                Bout << loc->Data.WX;
                Bout << loc->Data.WY;
                Bout << loc->Data.Radius;
                Bout << loc->Data.Color;
            }
            else
            {
                EraseKnownLoc( loc_id );
                Bout.Push( empty_loc, sizeof( empty_loc ) );
            }
        }
    }

    if( FLAG( info_flags, GM_INFO_GROUP_PARAM ) )
    {
        ushort cur_x = (ushort) GroupMove->CurX;
        ushort cur_y = (ushort) GroupMove->CurY;
        ushort to_x = (ushort) GroupMove->ToX;
        ushort to_y = (ushort) GroupMove->ToY;
        uint   speed = (uint) ( GroupMove->Speed * 1000000.0f );
        bool   wait = ( GroupMove->EncounterDescriptor ? true : false );

        Bout << cur_x;
        Bout << cur_y;
        Bout << to_x;
        Bout << to_y;
        Bout << speed;
        Bout << wait;

        if( car )
            Bout << car->AccCritter.Id;
        else
            Bout << (uint) 0;
    }

    if( FLAG( info_flags, GM_INFO_ZONES_FOG ) )
    {
        Bout.Push( (char*) data_ext->GlobalMapFog, GM_ZONES_FOG_SIZE );
    }
    BOUT_END( this );

    if( FLAG( info_flags, GM_INFO_CRITTERS ) )
        ProcessVisibleCritters();
    if( FLAG( info_flags, GM_INFO_GROUP_PARAM ) && car )
        Send_AddItemOnMap( car );
}

void Client::Send_GlobalLocation( Location* loc, bool add )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uchar info_flags = GM_INFO_LOCATION;
    uint  msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( info_flags ) + sizeof( add ) + SEND_LOCATION_SIZE;

    BOUT_BEGIN( this );
    Bout << NETMSG_GLOBAL_INFO;
    Bout << msg_len;
    Bout << info_flags;
    Bout << add;
    Bout << loc->GetId();
    Bout << loc->GetPid();
    Bout << loc->Data.WX;
    Bout << loc->Data.WY;
    Bout << loc->Data.Radius;
    Bout << loc->Data.Color;
    BOUT_END( this );
}

void Client::Send_GlobalMapFog( ushort zx, ushort zy, uchar fog )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uchar info_flags = GM_INFO_FOG;
    uint  msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( info_flags ) + sizeof( zx ) + sizeof( zy ) + sizeof( fog );

    BOUT_BEGIN( this );
    Bout << NETMSG_GLOBAL_INFO;
    Bout << msg_len;
    Bout << info_flags;
    Bout << zx;
    Bout << zy;
    Bout << fog;
    BOUT_END( this );
}

void Client::Send_XY( Critter* cr )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_XY;
    Bout << cr->GetId();
    Bout << cr->GetHexX();
    Bout << cr->GetHexY();
    Bout << cr->GetDir();
    BOUT_END( this );
}

void Client::Send_AllParams()
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_ALL_PARAMS;
    Bout.Push( (char*) Data.Params, (char*) &ParamsChosenSendMask[ 0 ], sizeof( Data.Params ) );
    BOUT_END( this );
}

void Client::Send_Param( ushort num_param )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !ParamsChosenSendMask[ num_param ] )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_PARAM;
    Bout << num_param;
    Bout << Data.Params[ num_param ];
    BOUT_END( this );
}

void Client::Send_ParamOther( ushort num_param, int val )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_PARAM;
    Bout << num_param;
    Bout << val;
    BOUT_END( this );
}

void Client::Send_CritterParam( Critter* cr, ushort num_param, int val )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_PARAM;
    Bout << cr->GetId();
    Bout << num_param;
    Bout << val;
    BOUT_END( this );
}

void Client::Send_Talk()
{
    if( IsSendDisabled() || IsOffline() )
        return;

    bool  close = ( Talk.TalkType == TALK_NONE );
    uchar is_npc = ( Talk.TalkType == TALK_WITH_NPC );
    uint  talk_id = ( is_npc ? Talk.TalkNpc : Talk.DialogPackId );
    uint  msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( is_npc ) + sizeof( talk_id ) + sizeof( uchar );

    BOUT_BEGIN( this );
    Bout << NETMSG_TALK_NPC;

    if( close )
    {
        Bout << msg_len;
        Bout << is_npc;
        Bout << talk_id;
        Bout << uchar( 0 );
    }
    else
    {
        uchar all_answers = (uchar) Talk.CurDialog.Answers.size();
        msg_len += sizeof( uint ) + sizeof( uint ) * all_answers + sizeof( uint ) + sizeof( ushort ) + (uint) Talk.Lexems.length();

        Bout << msg_len;
        Bout << is_npc;
        Bout << talk_id;
        Bout << all_answers;
        Bout << (ushort) Talk.Lexems.length();                                  // Lexems length
        if( Talk.Lexems.length() )
            Bout.Push( Talk.Lexems.c_str(), (uint) Talk.Lexems.length() );      // Lexems string
        Bout << Talk.CurDialog.TextId;                                          // Main text_id
        for( auto it = Talk.CurDialog.Answers.begin(), end = Talk.CurDialog.Answers.end(); it != end; ++it )
            Bout << ( *it ).TextId;                                             // Answers text_id
        Bout << uint( Talk.TalkTime - ( Timer::GameTick() - Talk.StartTick ) ); // Talk time
    }
    BOUT_END( this );
}

void Client::Send_GameInfo( Map* map )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    if( map )
        map->Lock();
    int                time = ( map ? map->GetTime() : -1 );
    uchar              rain = ( map ? map->GetRain() : 0 );
    bool               turn_based = ( map ? map->IsTurnBasedOn : false );
    bool               no_log_out = ( map ? map->IsNoLogOut() : true );
    static const int   day_time_dummy[ 4 ] = { 0 };
    static const uchar day_color_dummy[ 12 ] = { 0 };
    const int*         day_time = ( map ? map->Data.MapDayTime : day_time_dummy );
    const uchar*       day_color = ( map ? map->Data.MapDayColor : day_color_dummy );
    if( map )
        map->Unlock();

    BOUT_BEGIN( this );
    Bout << NETMSG_GAME_INFO;
    Bout << GameOpt.YearStart;
    Bout << GameOpt.Year;
    Bout << GameOpt.Month;
    Bout << GameOpt.Day;
    Bout << GameOpt.Hour;
    Bout << GameOpt.Minute;
    Bout << GameOpt.Second;
    Bout << GameOpt.TimeMultiplier;
    Bout << time;
    Bout << rain;
    Bout << turn_based;
    Bout << no_log_out;
    Bout.Push( (const char*) day_time, sizeof( day_time_dummy ) );
    Bout.Push( (const char*) day_color, sizeof( day_color_dummy ) );
    BOUT_END( this );
}

void Client::Send_Text( Critter* from_cr, const char* s_str, uchar how_say )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !s_str || !s_str[ 0 ] )
        return;
    ushort s_len = Str::Length( s_str );
    uint   from_id = ( from_cr ? from_cr->GetId() : 0 );
    ushort intellect = ( from_cr && how_say >= SAY_NORM && how_say <= SAY_RADIO ? from_cr->IntellectCacheValue : 0 );
    Send_TextEx( from_id, s_str, s_len, how_say, intellect, false );
}

void Client::Send_TextEx( uint from_id, const char* s_str, ushort str_len, uchar how_say, ushort intellect, bool unsafe_text )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( from_id ) + sizeof( how_say ) +
                   sizeof( intellect ) + sizeof( unsafe_text ) + sizeof( str_len ) + str_len;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_TEXT;
    Bout << msg_len;
    Bout << from_id;
    Bout << how_say;
    Bout << intellect;
    Bout << unsafe_text;
    Bout << str_len;
    Bout.Push( s_str, str_len );
    BOUT_END( this );
}

void Client::Send_TextMsg( Critter* from_cr, uint num_str, uchar how_say, ushort num_msg )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !num_str )
        return;
    uint from_id = ( from_cr ? from_cr->GetId() : 0 );

    BOUT_BEGIN( this );
    Bout << NETMSG_MSG;
    Bout << from_id;
    Bout << how_say;
    Bout << num_msg;
    Bout << num_str;
    BOUT_END( this );
}

void Client::Send_TextMsg( uint from_id, uint num_str, uchar how_say, ushort num_msg )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !num_str )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_MSG;
    Bout << from_id;
    Bout << how_say;
    Bout << num_msg;
    Bout << num_str;
    BOUT_END( this );
}

void Client::Send_TextMsgLex( Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !num_str )
        return;

    ushort lex_len = Str::Length( lexems );
    if( !lex_len || lex_len > MAX_DLG_LEXEMS_TEXT )
    {
        Send_TextMsg( from_cr, num_str, how_say, num_msg );
        return;
    }

    uint from_id = ( from_cr ? from_cr->GetId() : 0 );
    uint msg_len = NETMSG_MSG_SIZE + sizeof( lex_len ) + lex_len;

    BOUT_BEGIN( this );
    Bout << NETMSG_MSG_LEX;
    Bout << msg_len;
    Bout << from_id;
    Bout << how_say;
    Bout << num_msg;
    Bout << num_str;
    Bout << lex_len;
    Bout.Push( lexems, lex_len );
    BOUT_END( this );
}

void Client::Send_TextMsgLex( uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !num_str )
        return;

    ushort lex_len = Str::Length( lexems );
    if( !lex_len || lex_len > MAX_DLG_LEXEMS_TEXT )
    {
        Send_TextMsg( from_id, num_str, how_say, num_msg );
        return;
    }

    uint msg_len = NETMSG_MSG_SIZE + sizeof( lex_len ) + lex_len;

    BOUT_BEGIN( this );
    Bout << NETMSG_MSG_LEX;
    Bout << msg_len;
    Bout << from_id;
    Bout << how_say;
    Bout << num_msg;
    Bout << num_str;
    Bout << lex_len;
    Bout.Push( lexems, lex_len );
    BOUT_END( this );
}

void Client::Send_MapText( ushort hx, ushort hy, uint color, const char* text, ushort text_len, ushort intellect, bool unsafe_text )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( hx ) + sizeof( hy ) + sizeof( color ) +
                   sizeof( text_len ) + text_len + sizeof( intellect ) + sizeof( unsafe_text );

    BOUT_BEGIN( this );
    Bout << NETMSG_MAP_TEXT;
    Bout << msg_len;
    Bout << hx;
    Bout << hy;
    Bout << color;
    Bout << text_len;
    Bout.Push( text, text_len );
    Bout << intellect;
    Bout << unsafe_text;
    BOUT_END( this );
}

void Client::Send_MapTextMsg( ushort hx, ushort hy, uint color, ushort num_msg, uint num_str )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_MAP_TEXT_MSG;
    Bout << hx;
    Bout << hy;
    Bout << color;
    Bout << num_msg;
    Bout << num_str;
    BOUT_END( this );
}

void Client::Send_MapTextMsgLex( ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, const char* lexems, ushort lexems_len )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( ushort ) * 2 + sizeof( uint ) + sizeof( ushort ) + sizeof( uint ) + sizeof( lexems_len ) + lexems_len;

    BOUT_BEGIN( this );
    Bout << NETMSG_MAP_TEXT_MSG_LEX;
    Bout << msg_len;
    Bout << hx;
    Bout << hy;
    Bout << color;
    Bout << num_msg;
    Bout << num_str;
    Bout << lexems_len;
    if( lexems_len )
        Bout.Push( lexems, lexems_len );
    BOUT_END( this );
}

void Client::Send_CombatResult( uint* combat_res, uint len )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !combat_res || len > GameOpt.FloodSize / sizeof( uint ) )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( len ) + len * sizeof( uint );

    BOUT_BEGIN( this );
    Bout << NETMSG_COMBAT_RESULTS;
    Bout << msg_len;
    Bout << len;
    if( len )
        Bout.Push( (char*) combat_res, len * sizeof( uint ) );
    BOUT_END( this );
}

void Client::Send_Quest( uint num )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_QUEST;
    Bout << num;
    BOUT_END( this );
}

void Client::Send_Quests( UIntVec& nums )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uint ) + sizeof( uint ) * (uint) nums.size();

    BOUT_BEGIN( this );
    Bout << NETMSG_QUESTS;
    Bout << msg_len;
    Bout << (uint) nums.size();
    if( nums.size() )
        Bout.Push( (char*) &nums[ 0 ], (uint) nums.size() * sizeof( uint ) );
    BOUT_END( this );
}

void Client::Send_HoloInfo( bool clear, ushort offset, ushort count )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( offset >= MAX_HOLO_INFO || count > MAX_HOLO_INFO || offset + count > MAX_HOLO_INFO )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( clear ) + sizeof( offset ) + sizeof( count ) + count * sizeof( uint );

    BOUT_BEGIN( this );
    Bout << NETMSG_HOLO_INFO;
    Bout << msg_len;
    Bout << clear;
    Bout << offset;
    Bout << count;
    if( count )
        Bout.Push( (char*) &Data.HoloInfo[ offset ], count * sizeof( uint ) );
    BOUT_END( this );
}

void Client::Send_UserHoloStr( uint str_num, const char* text, ushort text_len )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( str_num ) + sizeof( text_len ) + text_len;

    BOUT_BEGIN( this );
    Bout << NETMSG_USER_HOLO_STR;
    Bout << msg_len;
    Bout << str_num;
    Bout << text_len;
    Bout.Push( text, text_len );
    BOUT_END( this );
}

void Client::Send_AutomapsInfo( void* locs_vec, Location* loc )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    if( locs_vec )
    {
        LocVec* locs = (LocVec*) locs_vec;
        uint    msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( bool ) + sizeof( ushort );
        for( uint i = 0, j = (uint) locs->size(); i < j; i++ )
        {
            Location* loc_ = ( *locs )[ i ];
            msg_len += sizeof( uint ) + sizeof( ushort ) + sizeof( ushort ) + sizeof( ushort ) * (uint) loc_->GetAutomaps().size();
        }

        BOUT_BEGIN( this );
        Bout << NETMSG_AUTOMAPS_INFO;
        Bout << msg_len;
        Bout << (bool) true;        // Clear list
        Bout << (ushort) locs->size();
        for( uint i = 0, j = (uint) locs->size(); i < j; i++ )
        {
            Location*  loc_ = ( *locs )[ i ];
            UShortVec& automaps = loc_->GetAutomaps();
            Bout << loc_->GetId();
            Bout << loc_->GetPid();
            Bout << (ushort) automaps.size();
            for( uint k = 0, l = (uint) automaps.size(); k < l; k++ )
                Bout << automaps[ k ];
        }
        BOUT_END( this );
    }

    if( loc )
    {
        UShortVec& automaps = loc->GetAutomaps();
        uint       msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( bool ) + sizeof( ushort ) +
                             sizeof( uint ) + sizeof( ushort ) + sizeof( ushort ) + sizeof( ushort ) * (uint) automaps.size();

        BOUT_BEGIN( this );
        Bout << NETMSG_AUTOMAPS_INFO;
        Bout << msg_len;
        Bout << (bool) false;        // Append this information
        Bout << (ushort) 1;
        Bout << loc->GetId();
        Bout << loc->GetPid();
        Bout << (ushort) automaps.size();
        for( uint i = 0, j = (uint) automaps.size(); i < j; i++ )
            Bout << automaps[ i ];
        BOUT_END( this );
    }
}

void Client::Send_Follow( uint rule, uchar follow_type, ushort map_pid, uint follow_wait )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_FOLLOW;
    Bout << rule;
    Bout << follow_type;
    Bout << map_pid;
    Bout << follow_wait;
    BOUT_END( this );
}

void Client::Send_Effect( ushort eff_pid, ushort hx, ushort hy, ushort radius )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_EFFECT;
    Bout << eff_pid;
    Bout << hx;
    Bout << hy;
    Bout << radius;
    BOUT_END( this );
}

void Client::Send_FlyEffect( ushort eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_FLY_EFFECT;
    Bout << eff_pid;
    Bout << from_crid;
    Bout << to_crid;
    Bout << from_hx;
    Bout << from_hy;
    Bout << to_hx;
    Bout << to_hy;
    BOUT_END( this );
}

void Client::Send_PlaySound( uint crid_synchronize, const char* sound_name )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_PLAY_SOUND;
    Bout << crid_synchronize;
    Bout.Push( sound_name, 100 );
    BOUT_END( this );
}

void Client::Send_PlaySoundType( uint crid_synchronize, uchar sound_type, uchar sound_type_ext, uchar sound_id, uchar sound_id_ext )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_PLAY_SOUND_TYPE;
    Bout << crid_synchronize;
    Bout << sound_type;
    Bout << sound_type_ext;
    Bout << sound_id;
    Bout << sound_id_ext;
    BOUT_END( this );
}

void Client::Send_CritterLexems( Critter* cr )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint   critter_id = cr->GetId();
    ushort lexems_len = Str::Length( cr->Data.Lexems );
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( critter_id ) + sizeof( lexems_len ) + lexems_len;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_LEXEMS;
    Bout << msg_len;
    Bout << critter_id;
    Bout << lexems_len;
    Bout.Push( cr->Data.Lexems, lexems_len );
    BOUT_END( this );
}

void Client::Send_PlayersBarter( uchar barter, uint param, uint param_ext )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_PLAYERS_BARTER;
    Bout << barter;
    Bout << param;
    Bout << param_ext;
    BOUT_END( this );
}

void Client::Send_PlayersBarterSetHide( Item* item, uint count )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_PLAYERS_BARTER_SET_HIDE;
    Bout << item->GetId();
    Bout << item->GetProtoId();
    Bout << count;
    Bout.Push( (char*) &item->Data, Item::ItemData::SendMask[ ITEM_DATA_MASK_CONTAINER ], sizeof( item->Data ) );
    BOUT_END( this );

    if( item->PLexems )
        Send_ItemLexems( item );
}

void Client::Send_ShowScreen( int screen_type, uint param, bool need_answer )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_SHOW_SCREEN;
    Bout << screen_type;
    Bout << param;
    Bout << need_answer;
    BOUT_END( this );
}

void Client::Send_RunClientScript( const char* func_name, int p0, int p1, int p2, const char* p3, UIntVec& p4 )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    ushort func_name_len = Str::Length( func_name );
    ushort p3len = ( p3 ? Str::Length( p3 ) : 0 );
    ushort p4size = (uint) p4.size();
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( func_name_len ) + func_name_len + sizeof( p0 ) + sizeof( p1 ) + sizeof( p2 ) + sizeof( p3len ) + p3len + sizeof( p4size ) + p4size * sizeof( uint );

    BOUT_BEGIN( this );
    Bout << NETMSG_RUN_CLIENT_SCRIPT;
    Bout << msg_len;
    Bout << func_name_len;
    Bout.Push( func_name, func_name_len );
    Bout << p0;
    Bout << p1;
    Bout << p2;
    Bout << p3len;
    if( p3len )
        Bout.Push( p3, p3len );
    Bout << p4size;
    if( p4size )
        Bout.Push( (char*) &p4[ 0 ], p4size * sizeof( uint ) );
    BOUT_END( this );
}

void Client::Send_DropTimers()
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_DROP_TIMERS;
    BOUT_END( this );
}

void Client::Send_ViewMap()
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_VIEW_MAP;
    Bout << ViewMapHx;
    Bout << ViewMapHy;
    Bout << ViewMapLocId;
    Bout << ViewMapLocEnt;
    BOUT_END( this );
}

void Client::Send_ItemLexems( Item* item ) // Already checks for client offline and lexems valid
{
    if( IsSendDisabled() )
        return;

    uint   item_id = item->GetId();
    ushort lexems_len = Str::Length( item->PLexems );
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( item_id ) + sizeof( lexems_len ) + lexems_len;

    BOUT_BEGIN( this );
    Bout << NETMSG_ITEM_LEXEMS;
    Bout << msg_len;
    Bout << item_id;
    Bout << lexems_len;
    Bout.Push( item->PLexems, lexems_len );
    BOUT_END( this );
}

void Client::Send_ItemLexemsNull( Item* item ) // Already checks for client offline and lexems valid
{
    if( IsSendDisabled() )
        return;

    uint   item_id = item->GetId();
    ushort lexems_len = 0;
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( item_id ) + sizeof( lexems_len );

    BOUT_BEGIN( this );
    Bout << NETMSG_ITEM_LEXEMS;
    Bout << msg_len;
    Bout << item_id;
    Bout << lexems_len;
    BOUT_END( this );
}

void Client::Send_CheckUIDS()
{
    if( IsSendDisabled() )
        return;

    const uint uid_msg[ 5 ] = { NETMSG_CHECK_UID0, NETMSG_CHECK_UID1, NETMSG_CHECK_UID2, NETMSG_CHECK_UID3, NETMSG_CHECK_UID4 };
    uint       msg = uid_msg[ Random( 0, 4 ) ];
    uchar      rnd_count = Random( 1, 21 );
    uchar      rnd_count2 = Random( 2, 12 );
    uint       uidxor[ 5 ] = { (uint) Random( 1, 0x6FFFFFFF ), (uint) Random( 1, 0x6FFFFFFF ), (uint) Random( 1, 0x6FFFFFFF ), (uint) Random( 1, 0x6FFFFFFF ), (uint) Random( 1, 0x6FFFFFFF ) };
    uint       uid[ 5 ] = { UID[ 0 ] ^ uidxor[ 0 ], UID[ 1 ] ^ uidxor[ 1 ], UID[ 2 ] ^ uidxor[ 2 ], UID[ 3 ] ^ uidxor[ 3 ], UID[ 4 ] ^ uidxor[ 4 ] };
    uint       msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( uid ) + sizeof( uidxor ) + sizeof( rnd_count ) * 2 + rnd_count + rnd_count2;

    BOUT_BEGIN( this );
    Bout << msg;
    Bout << msg_len;
    Bout << uid[ 3 ];
    Bout << uidxor[ 0 ];
    Bout << rnd_count;
    Bout << uid[ 1 ];
    Bout << uidxor[ 2 ];
    for( int i = 0; i < rnd_count; i++ )
        Bout << (uchar) Random( 0, 255 );
    Bout << uid[ 2 ];
    Bout << uidxor[ 1 ];
    Bout << uid[ 4 ];
    Bout << rnd_count2;
    Bout << uidxor[ 3 ];
    Bout << uidxor[ 4 ];
    Bout << uid[ 0 ];
    for( int i = 0; i < rnd_count2; i++ )
        Bout << (uchar) Random( 0, 255 );
    BOUT_END( this );
}

void Client::Send_SomeItem( Item* item )
{
    BOUT_BEGIN( this );
    Bout << NETMSG_SOME_ITEM;
    Bout << item->GetId();
    Bout << item->GetProtoId();
    Bout << item->AccCritter.Slot;
    Bout.Push( (char*) &item->Data, Item::ItemData::SendMask[ ITEM_DATA_MASK_CRITTER ], sizeof( item->Data ) );
    BOUT_END( this );
}

void Client::Send_CustomMessage( uint msg )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << msg;
    BOUT_END( this );
}

void Client::Send_CustomMessage( uint msg, uchar* data, uint data_size )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( msg ) + sizeof( msg_len ) + data_size;

    BOUT_BEGIN( this );
    Bout << msg;
    Bout << msg_len;
    if( data_size )
        Bout.Push( (char*) data, data_size );
    BOUT_END( this );
}

/************************************************************************/
/* Locations                                                            */
/************************************************************************/

bool Client::CheckKnownLocById( uint loc_id )
{
    if( !loc_id )
        return false;
    CritDataExt* data_ext = GetDataExt();
    if( !data_ext )
        return false;

    for( int i = 0; i < data_ext->LocationsCount; i++ )
        if( data_ext->LocationsId[ i ] == loc_id )
            return true;
    return false;
}

bool Client::CheckKnownLocByPid( ushort loc_pid )
{
    if( !loc_pid )
        return false;
    CritDataExt* data_ext = GetDataExt();
    if( !data_ext )
        return false;

    for( int i = 0; i < data_ext->LocationsCount; i++ )
    {
        Location* loc = MapMngr.GetLocation( data_ext->LocationsId[ i ] );
        if( loc && loc->GetPid() == loc_pid )
            return true;
    }
    return false;
}

void Client::AddKnownLoc( uint loc_id )
{
    CritDataExt* data_ext = GetDataExt();
    if( !data_ext )
        return;

    if( data_ext->LocationsCount >= MAX_STORED_LOCATIONS )
    {
        // Erase 100..199
        memcpy( &data_ext->LocationsId[ 100 ], &data_ext->LocationsId[ 200 ], MAX_STORED_LOCATIONS - 100 );
        data_ext->LocationsCount = MAX_STORED_LOCATIONS - 100;
    }

    for( int i = 0; i < data_ext->LocationsCount; i++ )
        if( data_ext->LocationsId[ i ] == loc_id )
            return;

    data_ext->LocationsId[ data_ext->LocationsCount ] = loc_id;
    data_ext->LocationsCount++;
}

void Client::EraseKnownLoc( uint loc_id )
{
    if( !loc_id )
        return;
    CritDataExt* data_ext = GetDataExt();
    if( !data_ext )
        return;

    for( int i = 0; i < data_ext->LocationsCount; i++ )
    {
        uint& cur_loc_id = data_ext->LocationsId[ i ];
        if( cur_loc_id == loc_id )
        {
            // Swap
            uint& last_loc_id = data_ext->LocationsId[ data_ext->LocationsCount - 1 ];
            if( i != data_ext->LocationsCount - 1 )
                cur_loc_id = last_loc_id;
            data_ext->LocationsCount--;
            return;
        }
    }
}

/************************************************************************/
/* Players barter                                                       */
/************************************************************************/

Client* Client::BarterGetOpponent( uint opponent_id )
{
    if( !GetMap() && !GroupMove )
    {
        BarterEnd();
        return NULL;
    }

    if( BarterOpponent && BarterOpponent != opponent_id )
    {
        Critter* cr = ( GetMap() ? GetCritSelf( BarterOpponent, true ) : GroupMove->GetCritter( BarterOpponent ) );
        if( cr && cr->IsPlayer() )
            ( (Client*) cr )->BarterEnd();
        BarterEnd();
    }

    Critter* cr = ( GetMap() ? GetCritSelf( opponent_id, true ) : GroupMove->GetCritter( opponent_id ) );
    if( !cr || !cr->IsPlayer() || !cr->IsLife() || cr->IsBusy() || cr->GetParam( TO_BATTLE ) || ( (Client*) cr )->IsOffline() ||
        ( GetMap() && !CheckDist( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), BARTER_DIST + GetMultihex() + cr->GetMultihex() ) ) )
    {
        if( cr && cr->IsPlayer() && BarterOpponent == cr->GetId() )
            ( (Client*) cr )->BarterEnd();
        BarterEnd();
        return NULL;
    }

    BarterOpponent = cr->GetId();
    return (Client*) cr;
}

void Client::BarterEnd()
{
    BarterOpponent = 0;
    Send_PlayersBarter( BARTER_END, 0, 0 );
}

void Client::BarterRefresh( Client* opponent )
{
    Send_PlayersBarter( BARTER_REFRESH, opponent->GetId(), opponent->BarterHide );
    Send_PlayersBarter( BARTER_OFFER, BarterOffer, false );
    Send_PlayersBarter( BARTER_OFFER, opponent->BarterOffer, true );
    if( !opponent->BarterHide )
        Send_ContainerInfo( opponent, TRANSFER_CRIT_BARTER, false );
    for( auto it = BarterItems.begin(); it != BarterItems.end();)
    {
        BarterItem& barter_item = *it;
        if( !GetItem( barter_item.Id, true ) )
            it = BarterItems.erase( it );
        else
        {
            Send_PlayersBarter( BARTER_SET_SELF, barter_item.Id, barter_item.Count );
            ++it;
        }
    }
    for( auto it = opponent->BarterItems.begin(); it != opponent->BarterItems.end();)
    {
        BarterItem& barter_item = *it;
        if( !opponent->GetItem( barter_item.Id, true ) )
            it = opponent->BarterItems.erase( it );
        else
        {
            if( opponent->BarterHide )
                Send_PlayersBarterSetHide( opponent->GetItem( barter_item.Id, true ), barter_item.Count );
            else
                Send_PlayersBarter( BARTER_SET_OPPONENT, barter_item.Id, barter_item.Count );
            ++it;
        }
    }
}

Client::BarterItem* Client::BarterGetItem( uint item_id )
{
    auto it = std::find( BarterItems.begin(), BarterItems.end(), item_id );
    return it != BarterItems.end() ? &( *it ) : NULL;
}

void Client::BarterEraseItem( uint item_id )
{
    auto it = std::find( BarterItems.begin(), BarterItems.end(), item_id );
    if( it != BarterItems.end() )
        BarterItems.erase( it );
}

/************************************************************************/
/* Timers                                                               */
/************************************************************************/

void Client::DropTimers( bool send )
{
    LastSendScoresTick = 0;
    LastSendCraftTick = 0;
    LastSendEntrancesTick = 0;
    LastSendEntrancesLocId = 0;
    if( send )
        Send_DropTimers();
}

/************************************************************************/
/* Dialogs                                                              */
/************************************************************************/

void Client::ProcessTalk( bool force )
{
    if( !force && Timer::GameTick() < talkNextTick )
        return;
    talkNextTick = Timer::GameTick() + PROCESS_TALK_TICK;
    if( Talk.TalkType == TALK_NONE )
        return;

    // Check time of talk
    if( Timer::GameTick() - Talk.StartTick > Talk.TalkTime )
    {
        CloseTalk();
        return;
    }

    // Check npc
    Npc* npc = NULL;
    if( Talk.TalkType == TALK_WITH_NPC )
    {
        npc = CrMngr.GetNpc( Talk.TalkNpc, true );
        if( !npc )
        {
            CloseTalk();
            return;
        }

        if( !npc->IsLife() )
        {
            Send_TextMsg( this, STR_DIALOG_NPC_NOT_LIFE, SAY_NETMSG, TEXTMSG_GAME );
            CloseTalk();
            return;
        }

        if( npc->IsPlaneNoTalk() )
        {
            Send_TextMsg( this, STR_DIALOG_NPC_BUSY, SAY_NETMSG, TEXTMSG_GAME );
            CloseTalk();
            return;
        }
    }

    // Check distance
    if( !Talk.IgnoreDistance )
    {
        uint   map_id = 0;
        ushort hx = 0, hy = 0;
        uint   talk_distance = 0;
        if( Talk.TalkType == TALK_WITH_NPC )
        {
            map_id = npc->GetMap();
            hx = npc->GetHexX();
            hy = npc->GetHexY();
            talk_distance = npc->GetTalkDistance( this );
        }
        else if( Talk.TalkType == TALK_WITH_HEX )
        {
            map_id = Talk.TalkHexMap;
            hx = Talk.TalkHexX;
            hy = Talk.TalkHexY;
            talk_distance = GameOpt.TalkDistance + GetMultihex();
        }

        if( GetMap() != map_id || !CheckDist( GetHexX(), GetHexY(), hx, hy, talk_distance ) )
        {
            Send_TextMsg( this, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME );
            CloseTalk();
        }
    }
}

void Client::CloseTalk()
{
    if( Talk.TalkType != TALK_NONE )
    {
        Npc* npc = NULL;
        if( Talk.TalkType == TALK_WITH_NPC )
        {
            Talk.TalkType = TALK_NONE;
            npc = CrMngr.GetNpc( Talk.TalkNpc, true );
            if( npc )
            {
                if( Talk.Barter )
                    npc->EventBarter( this, false, npc->GetBarterPlayers() );
                npc->EventTalk( this, false, npc->GetTalkedPlayers() );
            }
        }

        int func_id = Talk.CurDialog.DlgScript;
        switch( func_id )
        {
        case NOT_ANSWER_CLOSE_DIALOG:
            break;
        case NOT_ANSWER_BEGIN_BATTLE:
        {
            if( !npc )
                break;
            npc->SetTarget( REASON_FROM_DIALOG, this, GameOpt.DeadHitPoints, false );
        }
        break;
        default:
            if( func_id <= 0 )
                break;
            if( !Script::PrepareContext( func_id, _FUNC_, GetInfo() ) )
                break;
            Script::SetArgObject( this );
            Script::SetArgObject( npc );
            Script::SetArgObject( NULL );
            Talk.Locked = true;
            Script::RunPrepared();
            Talk.Locked = false;
            break;
        }
    }

    Talk.Clear();
    Send_Talk();
}

/************************************************************************/
/* NPC                                                                  */
/************************************************************************/

Npc::Npc(): NextRefreshBagTick( 0 )
{
    CritterIsNpc = true;
    MEMORY_PROCESS( MEMORY_NPC, sizeof( Npc ) + sizeof( GlobalMapGroup ) + 40 + sizeof( Item ) * 2 );
    SETFLAG( Flags, FCRIT_NPC );
}

Npc::~Npc()
{
    MEMORY_PROCESS( MEMORY_NPC, -(int) ( sizeof( Npc ) + sizeof( GlobalMapGroup ) + 40 + sizeof( Item ) * 2 ) );
    if( DataExt )
    {
        MEMORY_PROCESS( MEMORY_NPC, -(int) sizeof( CritDataExt ) );
        SAFEDEL( DataExt );
    }
    DropPlanes();
}

void Npc::RefreshBag()
{
    if( Data.BagRefreshTime < 0 )
        return;
    NextRefreshBagTick = Timer::GameTick() + ( Data.BagRefreshTime ? Data.BagRefreshTime : GameOpt.BagRefreshTime ) * 60 * 1000;

    SyncLockItems();

    // Collect pids and count
    static THREAD uint pids[ MAX_ITEM_PROTOTYPES ];
    memzero( &pids, sizeof( pids ) );

    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        pids[ item->GetProtoId() ] += item->GetCount();

        // Repair/reload item in slots
        if( item->AccCritter.Slot != SLOT_INV )
        {
            if( item->IsDeteriorable() && item->IsBroken() )
                item->Repair();
            if( item->IsWeapon() && item->WeapGetMaxAmmoCount() && item->WeapIsEmpty() )
                item->WeapLoadHolder();
        }
    }

    // Item garbager
    if( !IsRawParam( MODE_NO_ITEM_GARBAGER ) )
    {
        // Erase not grouped items
        uint need_count = Random( 2, 4 );
        for( uint i = 0; i < MAX_ITEM_PROTOTYPES; i++ )
        {
            if( pids[ i ] <= need_count )
                continue;
            ProtoItem* proto_item = ItemMngr.GetProtoItem( i );
            if( !proto_item || proto_item->Stackable )
                continue;
            ItemMngr.SetItemCritter( this, i, need_count );
            pids[ i ] = need_count;
            need_count = Random( 2, 4 );
        }
    }

    // Refresh bag
    bool drop_last_weapon = false;

    // Internal bag
    if( !Data.Params[ ST_BAG_ID ] )
    {
        if( !Data.BagSize )
            return;

        // Update cur items
        for( int k = 0; k < Data.BagSize; k++ )
        {
            NpcBagItem& item = Data.Bag[ k ];
            uint        count = item.MinCnt;
            if( pids[ item.ItemPid ] > count )
                continue;
            if( item.MinCnt != item.MaxCnt )
                count = Random( item.MinCnt, item.MaxCnt );
            ItemMngr.SetItemCritter( this, item.ItemPid, count );
            drop_last_weapon = true;
        }
    }
    // External bags
    else
    {
        NpcBag& bag = AIMngr.GetBag( Data.Params[ ST_BAG_ID ] );
        if( bag.empty() )
            return;

        // Check combinations, update or create new
        for( uint i = 0; i < bag.size(); i++ )
        {
            NpcBagCombination& comb = bag[ i ];
            bool               create = true;
            if( i < MAX_NPC_BAGS_PACKS && Data.BagCurrentSet[ i ] < comb.size() )
            {
                NpcBagItems& items = comb[ Data.BagCurrentSet[ i ] ];
                for( uint l = 0; l < items.size(); l++ )
                {
                    NpcBagItem& item = items[ l ];
                    if( pids[ item.ItemPid ] > 0 )
                    {
                        // Update cur items
                        for( uint k = 0; k < items.size(); k++ )
                        {
                            NpcBagItem& item_ = items[ k ];
                            uint        count = item_.MinCnt;
                            if( pids[ item_.ItemPid ] > count )
                                continue;
                            if( item_.MinCnt != item_.MaxCnt )
                                count = Random( item_.MinCnt, item_.MaxCnt );
                            ItemMngr.SetItemCritter( this, item_.ItemPid, count );
                            drop_last_weapon = true;
                        }

                        create = false;
                        goto label_EndCycles;                         // Force end of cycle
                    }
                }
            }

label_EndCycles:
            // Create new combination
            if( create && comb.size() )
            {
                int          rnd = Random( 0, (int) comb.size() - 1 );
                Data.BagCurrentSet[ i ] = rnd;
                NpcBagItems& items = comb[ rnd ];
                for( uint k = 0; k < items.size(); k++ )
                {
                    NpcBagItem& bag_item = items[ k ];
                    uint        count = bag_item.MinCnt;
                    if( bag_item.MinCnt != bag_item.MaxCnt )
                        count = Random( bag_item.MinCnt, bag_item.MaxCnt );
                    // if(pids[bag_item.ItemPid]<count) TODO: need???
                    if( ItemMngr.SetItemCritter( this, bag_item.ItemPid, count ) )
                    {
                        // Move bag_item
                        if( bag_item.ItemSlot != SLOT_INV )
                        {
                            if( !GetItemSlot( bag_item.ItemSlot ) )
                            {
                                Item* item = GetItemByPid( bag_item.ItemPid );
                                if( item && MoveItem( SLOT_INV, bag_item.ItemSlot, item->GetId(), item->GetCount() ) )
                                    Data.FavoriteItemPid[ bag_item.ItemSlot ] = bag_item.ItemPid;
                            }
                        }
                        drop_last_weapon = true;
                    }
                }
            }
        }
    }

    if( drop_last_weapon && Data.Params[ ST_LAST_WEAPON_ID ] )
    {
        ChangeParam( ST_LAST_WEAPON_ID );
        Data.Params[ ST_LAST_WEAPON_ID ] = 0;
    }
}

bool Npc::AddPlane( int reason, AIDataPlane* plane, bool is_child, Critter* some_cr, Item* some_item )
{
    if( is_child && aiPlanes.empty() )
    {
        plane->Assigned = false;
        plane->Release();
        return false;
    }

    if( plane->Type == AI_PLANE_ATTACK )
    {
        for( auto it = aiPlanes.begin(), end = aiPlanes.end(); it != end; ++it )
        {
            AIDataPlane* p = *it;
            if( p->Type == AI_PLANE_ATTACK && p->Attack.TargId == plane->Attack.TargId )
            {
                p->Assigned = false;
                p->Release();
                aiPlanes.erase( it );
                break;
            }
        }
    }

    int result = EventPlaneBegin( plane, reason, some_cr, some_item );
    if( result == PLANE_RUN_GLOBAL && Script::PrepareContext( ServerFunctions.NpcPlaneBegin, _FUNC_, GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( plane );
        Script::SetArgUInt( reason );
        Script::SetArgObject( some_cr );
        Script::SetArgObject( some_item );
        if( Script::RunPrepared() )
            result = ( Script::GetReturnedBool() ? PLANE_KEEP : PLANE_DISCARD );
    }

    if( result == PLANE_DISCARD )
    {
        plane->Release();
        return false;
    }

    // Add
    plane->Assigned = true;
    if( is_child )
    {
        AIDataPlane* parent = aiPlanes[ 0 ]->GetCurPlane();
        parent->ChildPlane = plane;
        plane->Priority = parent->Priority;
    }
    else
    {
        if( !plane->Priority )
        {
            aiPlanes.push_back( plane );
        }
        else
        {
            uint i = 0;
            for( ; i < aiPlanes.size(); i++ )
                if( plane->Priority > aiPlanes[ i ]->Priority )
                    break;
            aiPlanes.insert( aiPlanes.begin() + i, plane );
            if( i && plane->Priority == aiPlanes[ 0 ]->Priority )
                SetBestCurPlane();
        }
    }
    return true;
}

void Npc::NextPlane( int reason, Critter* some_cr, Item* some_item )
{
    if( aiPlanes.empty() )
        return;
    SendA_XY();

    AIDataPlane* cur = aiPlanes[ 0 ];
    AIDataPlane* last = aiPlanes[ 0 ]->GetCurPlane();

    aiPlanes.erase( aiPlanes.begin() );
    SetBestCurPlane();

    int result = EventPlaneEnd( last, reason, some_cr, some_item );
    if( result == PLANE_RUN_GLOBAL && Script::PrepareContext( ServerFunctions.NpcPlaneEnd, _FUNC_, GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( last );
        Script::SetArgUInt( reason );
        Script::SetArgObject( some_cr );
        Script::SetArgObject( some_item );
        if( Script::RunPrepared() )
            result = ( Script::GetReturnedBool() ? PLANE_DISCARD : PLANE_KEEP );
    }

    if( result == PLANE_KEEP )
    {
        uint place = 0;
        if( aiPlanes.size() )
        {
            place = 1;
            for( uint i = (uint) aiPlanes.size(); place < i; place++ )
                if( cur->Priority > aiPlanes[ place ]->Priority )
                    break;
        }
        aiPlanes.insert( aiPlanes.begin() + place, cur );
    }
    else
    {
        if( cur != last )     // Child
        {
            cur->DeleteLast();
            aiPlanes.insert( aiPlanes.begin(), cur );
        }
        else         // Main
        {
            cur->Assigned = false;
            cur->Release();
        }
    }
}

bool Npc::RunPlane( int reason, uint& r0, uint& r1, uint& r2 )
{
    AIDataPlane* cur = aiPlanes[ 0 ];
    AIDataPlane* last = aiPlanes[ 0 ]->GetCurPlane();
//	aiPlanes.erase(aiPlanes.begin());

    int result = EventPlaneRun( last, reason, r0, r1, r2 );
    ;
    if( result == PLANE_RUN_GLOBAL && Script::PrepareContext( ServerFunctions.NpcPlaneRun, _FUNC_, GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( last );
        Script::SetArgUInt( reason );
        Script::SetArgAddress( &r0 );
        Script::SetArgAddress( &r1 );
        Script::SetArgAddress( &r2 );
        if( Script::RunPrepared() )
            result = ( Script::GetReturnedBool() ? PLANE_KEEP : PLANE_DISCARD );
    }

    return result == PLANE_KEEP;

    /*if(result==PLANE_DISCARD)
       {
            if(cur!=last) // Child
            {
                    cur->DeleteLast();
                    aiPlanes.insert(aiPlanes.begin(),cur);
            }
            else // Main
            {
                    cur->Assigned=false;
                    cur->Release();
            }
            return false;
       }

       aiPlanes.insert(aiPlanes.begin(),cur);
       return true;*/
}

bool Npc::IsPlaneAviable( int plane_type )
{
    for( uint i = 0, j = (uint) aiPlanes.size(); i < j; i++ )
        if( aiPlanes[ i ]->IsSelfOrHas( plane_type ) )
            return true;
    return false;
}

void Npc::DropPlanes()
{
    for( uint i = 0, j = (uint) aiPlanes.size(); i < j; i++ )
    {
        aiPlanes[ i ]->Assigned = false;
        aiPlanes[ i ]->Release();
    }
    aiPlanes.clear();
}

void Npc::SetBestCurPlane()
{
    if( aiPlanes.size() < 2 )
        return;

    uint type = aiPlanes[ 0 ]->Type;
    uint priority = aiPlanes[ 0 ]->Priority;

    if( type != AI_PLANE_ATTACK )
        return;

    int sort_cnt = 1;
    for( uint i = 1, j = (uint) aiPlanes.size(); i < j; i++ )
    {
        if( aiPlanes[ i ]->Priority == priority )
            sort_cnt++;
        else
            break;
    }

    if( sort_cnt < 2 )
        return;

    // Find critter with smallest dist
    int hx = GetHexX();
    int hy = GetHexY();
    int best_plane = -1;
    int best_dist = -1;
    for( int i = 0; i < sort_cnt; i++ )
    {
        Critter* cr = GetCritSelf( aiPlanes[ i ]->Attack.TargId, false );
        if( !cr )
            continue;

        int dist = DistGame( hx, hy, cr->GetHexX(), cr->GetHexY() );
        if( best_dist >= 0 && dist >= best_dist )
            continue;
        else if( dist == best_dist )
        {
            Critter* cr2 = GetCritSelf( aiPlanes[ best_plane ]->Attack.TargId, false );
            if( !cr || cr->Data.Params[ ST_CURRENT_HP ] >= cr2->Data.Params[ ST_CURRENT_HP ] )
                continue;
        }

        best_dist = dist;
        best_plane = i;
    }

    if( best_plane > 0 )
        std::swap( aiPlanes[ 0 ], aiPlanes[ best_plane ] );
}

uint Npc::GetTalkedPlayers()
{
    uint talk = 0;
    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( !cr->IsPlayer() )
            continue;
        Client* cl = (Client*) cr;
        if( cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == GetId() )
            talk++;
    }
    return talk;
}

bool Npc::IsTalkedPlayers()
{
    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( !cr->IsPlayer() )
            continue;
        Client* cl = (Client*) cr;
        if( cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == GetId() )
            return true;
    }
    return false;
}

uint Npc::GetBarterPlayers()
{
    uint barter = 0;
    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( !cr->IsPlayer() )
            continue;
        Client* cl = (Client*) cr;
        if( cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == GetId() && cl->Talk.Barter )
            barter++;
    }
    return barter;
}

bool Npc::IsFreeToTalk()
{
    int max_talkers = GetParam( ST_MAX_TALKERS );
    if( max_talkers < 0 )
        return false;
    else if( !max_talkers )
        max_talkers = (uint) GameOpt.NpcMaxTalkers;
    return GetTalkedPlayers() < (uint) max_talkers;
}

bool Npc::IsPlaneNoTalk()
{
    AIDataPlane* p = GetCurPlane();
    if( !p )
        return false;

    switch( p->Type )
    {
    case AI_PLANE_WALK:
    case AI_PLANE_ATTACK:
    case AI_PLANE_PICK:
        return true;
    default:
        break;
    }

    return false;
}

void Npc::MoveToHex( int reason, ushort hx, ushort hy, uchar ori, bool is_run, uchar cut )
{
    AIDataPlane* plane = new AIDataPlane( AI_PLANE_WALK, AI_PLANE_WALK_PRIORITY );
    if( !plane )
        return;
    plane->Walk.HexX = hx;
    plane->Walk.HexY = hy;
    plane->Walk.Dir = ori;
    plane->Walk.IsRun = is_run;
    plane->Walk.Cut = cut;
    AddPlane( reason, plane, false );
}

void Npc::SetTarget( int reason, Critter* target, int min_hp, bool is_gag )
{
    for( uint i = 0, j = (uint) aiPlanes.size(); i < j; i++ )
    {
        AIDataPlane* pp = aiPlanes[ i ];
        if( pp->Type == AI_PLANE_ATTACK && pp->Attack.TargId == target->GetId() )
        {
            if( i && pp->Priority == aiPlanes[ 0 ]->Priority )
                SetBestCurPlane();
            return;
        }
    }

    AIDataPlane* plane = new AIDataPlane( AI_PLANE_ATTACK, AI_PLANE_ATTACK_PRIORITY );
    if( !plane )
        return;
    plane->Attack.TargId = target->GetId();
    plane->Attack.MinHp = min_hp;
    plane->Attack.LastHexX = ( target->GetMap() ? target->GetHexX() : 0 );
    plane->Attack.LastHexY = ( target->GetMap() ? target->GetHexY() : 0 );
    plane->Attack.IsGag = is_gag;
    plane->Attack.GagHexX = target->GetHexX();
    plane->Attack.GagHexY = target->GetHexY();
    plane->Attack.IsRun = ( IsTurnBased() ? GameOpt.TbAlwaysRun : GameOpt.RtAlwaysRun );
    AddPlane( reason, plane, false, target, NULL );
}
