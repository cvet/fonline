#ifndef __CRITTER__
#define __CRITTER__

#include "Common.h"
#include "zlib/zlib.h"
#include "BufferManager.h"
#include "Item.h"
#include "Dialogs.h"
#include "AI.h"
#include "CritterData.h"
#include "DataMask.h"
#include "NetProtocol.h"
#include "ThreadSync.h"
#include "Entity.h"

#if defined ( USE_LIBEVENT )
# include "event2/event.h"
# include "event2/bufferevent.h"
# include "event2/buffer.h"
#endif

// Plane results
#define PLANE_KEEP            ( 1 )
#define PLANE_DISCARD         ( 2 )

// Client game states
#define STATE_NONE            ( 0 )
#define STATE_CONNECTED       ( 1 )
#define STATE_PLAYING         ( 2 )
#define STATE_TRANSFERRING    ( 3 )

class Critter;
class Client;
class Npc;
class Map;
class Location;

typedef Critter*              CritterPtr;
typedef Client*               ClientPtr;
typedef Npc*                  NpcPtr;

typedef map< uint, Critter* > CrMap;
typedef map< uint, Client* >  ClMap;
typedef map< uint, Npc* >     PcMap;

typedef vector< Critter* >    CrVec;
typedef vector< Client* >     ClVec;
typedef vector< Npc* >        PcVec;

class Critter: public Entity
{
public:
    // Properties
    PROPERTIES_HEADER();
    // Core
    // CritData
    CLASS_PROPERTY( hash, ModelName );
    CLASS_PROPERTY( uint, MapId );
    CLASS_PROPERTY( hash, MapPid );
    CLASS_PROPERTY( ushort, HexX );
    CLASS_PROPERTY( ushort, HexY );
    CLASS_PROPERTY( uchar, Dir );
    CLASS_PROPERTY( ScriptString *, PassHash );
    CLASS_PROPERTY( int, Cond ); // enum CritterCondition
    CLASS_PROPERTY( bool, ClientToDelete );
    CLASS_PROPERTY( uint, Multihex );
    CLASS_PROPERTY( ushort, WorldX );
    CLASS_PROPERTY( ushort, WorldY );
    CLASS_PROPERTY( uint, GlobalMapLeaderId );
    CLASS_PROPERTY( uint, GlobalMapTripId );
    CLASS_PROPERTY( ushort, LastMapHexX );
    CLASS_PROPERTY( ushort, LastMapHexY );
    CLASS_PROPERTY( uint, Anim1Life );
    CLASS_PROPERTY( uint, Anim1Knockout );
    CLASS_PROPERTY( uint, Anim1Dead );
    CLASS_PROPERTY( uint, Anim2Life );
    CLASS_PROPERTY( uint, Anim2Knockout );
    CLASS_PROPERTY( uint, Anim2Dead );
    CLASS_PROPERTY( uint, Anim2KnockoutEnd );
    CLASS_PROPERTY( ScriptArray *, GlobalMapFog );
    CLASS_PROPERTY( ScriptArray *, TE_FuncNum );    // hash
    CLASS_PROPERTY( ScriptArray *, TE_Rate );       // uint
    CLASS_PROPERTY( ScriptArray *, TE_NextTime );   // uint
    CLASS_PROPERTY( ScriptArray *, TE_Identifier ); // int
    // ...
    CLASS_PROPERTY( uint, LookDistance );
    CLASS_PROPERTY( hash, DialogId );
    CLASS_PROPERTY( bool, IsNoTalk );
    CLASS_PROPERTY( bool, IsNoBarter );
    CLASS_PROPERTY( int, MaxTalkers );   // Callback on begin dialog?
    CLASS_PROPERTY( uint, TalkDistance );
    CLASS_PROPERTY( int, CurrentHp );
    // CLASS_PROPERTY( int, MaxHp ); Need?
    CLASS_PROPERTY( int, CurrentAp );
    CLASS_PROPERTY( uint, ApRegenerationTime );
    CLASS_PROPERTY( int, ActionPoints ); // Rename MaxAp
    CLASS_PROPERTY( bool, IsNoWalk );
    CLASS_PROPERTY( bool, IsNoRun );
    CLASS_PROPERTY( bool, IsNoRotate );
    CLASS_PROPERTY( uint, WalkTime );
    CLASS_PROPERTY( uint, RunTime );
    CLASS_PROPERTY( int, ScaleFactor );
    CLASS_PROPERTY( uint, TimeoutBattle );
    CLASS_PROPERTY( uint, TimeoutTransfer );
    CLASS_PROPERTY( uint, TimeoutRemoveFromGame );
    CLASS_PROPERTY( bool, IsGeck );    // Rename
    CLASS_PROPERTY( bool, IsNoLoot );  // ?
    CLASS_PROPERTY( bool, IsNoSteal ); // ?
    CLASS_PROPERTY( bool, IsNoHome );
    CLASS_PROPERTY( uint, HomeMapId );
    CLASS_PROPERTY( ushort, HomeHexX );
    CLASS_PROPERTY( ushort, HomeHexY );
    CLASS_PROPERTY( uchar, HomeDir );
    CLASS_PROPERTY( bool, IsHide );
    CLASS_PROPERTY( bool, IsEndCombat );           // ?
    CLASS_PROPERTY( hash, HandsItemProtoId );
    CLASS_PROPERTY( uchar, HandsItemMode );
    CLASS_PROPERTY( ScriptArray *, KnownLocations );
    CLASS_PROPERTY( ScriptArray *, ConnectionIp );
    CLASS_PROPERTY( ScriptArray *, ConnectionPort );
    CLASS_PROPERTY( uint, ShowCritterDist1 );
    CLASS_PROPERTY( uint, ShowCritterDist2 );
    CLASS_PROPERTY( uint, ShowCritterDist3 );
    CLASS_PROPERTY( hash, ScriptId );
    CLASS_PROPERTY( ScriptArray *, EnemyStack );
    CLASS_PROPERTY( ScriptArray *, InternalBagItemPid );
    CLASS_PROPERTY( ScriptArray *, InternalBagItemCount );
    CLASS_PROPERTY( ScriptArray *, ExternalBagCurrentSet );
    CLASS_PROPERTY( ScriptArray *, FavoriteItemPid );
    CLASS_PROPERTY( int, SneakCoefficient );
    CLASS_PROPERTY( int, BarterCoefficient );
    // Exclude
    CLASS_PROPERTY( hash, BagId );              // Bags (migrate bags to scripts)
    CLASS_PROPERTY( uint, LastWeaponId );       // Bags
    CLASS_PROPERTY( bool, IsNoItemGarbager );   // Bags
    CLASS_PROPERTY( hash, NpcRole );            // Find Npc criteria (maybe swap to some universal prop/value array as input)
    CLASS_PROPERTY( hash, TeamId );             // Trace check criteria (maybe swap to some universal prop/value array)
    CLASS_PROPERTY( uint, FreeBarterPlayer );   // Used for barter coef
    CLASS_PROPERTY( int, CarryWeight );         // Overweight checking
    // CLASS_PROPERTY( bool, IsInvulnerable ); // Resolve in scripts
    CLASS_PROPERTY( bool, IsUnlimitedAmmo );    // AI, check shoot possibility
    CLASS_PROPERTY( bool, IsNoUnarmed );        // AI
    CLASS_PROPERTY( bool, IsNoFavoriteItem );   // AI
    CLASS_PROPERTY( bool, IsNoPush );           // Can puck checks
    CLASS_PROPERTY( bool, IsNoEnemyStack );     // Migrate enemy stack to scripts
    CLASS_PROPERTY( bool, IsNoFlatten );        // Draw order (migrate to critter type option)
    CLASS_PROPERTY( bool, IsDamagedEye );
    CLASS_PROPERTY( bool, IsDamagedRightArm );
    CLASS_PROPERTY( bool, IsDamagedLeftArm );
    CLASS_PROPERTY( bool, IsDamagedRightLeg );
    CLASS_PROPERTY( bool, IsDamagedLeftLeg );

protected:
    Critter( uint id, EntityType type, ProtoCritter* proto );
    ~Critter();

public:
    // Data
    SyncObject    Sync;
    bool          CritterIsNpc;
    uint          Flags;
    ScriptString* NameStr;
    bool          IsRunning;
    int           LockMapTransfers;
    uint          AllowedToDownloadMap;

    static bool   SlotEnabled[ 0x100 ];
    static bool   SlotDataSendEnabled[ 0x100 ];
    static IntSet RegProperties;

    void DeleteInventory();
    hash GetFavoriteItemPid( uchar slot );
    void SetFavoriteItemPid( uchar slot, hash pid );

    // Visible critters and items
    CrVec         VisCr;
    CrVec         VisCrSelf;
    CrMap         VisCrMap;
    CrMap         VisCrSelfMap;
    UIntSet       VisCr1, VisCr2, VisCr3;
    UIntSet       VisItem;
    MutexSpinlock VisItemLocker;
    uint          ViewMapId;
    hash          ViewMapPid;
    ushort        ViewMapLook, ViewMapHx, ViewMapHy;
    uchar         ViewMapDir;
    uint          ViewMapLocId, ViewMapLocEnt;

    void SyncLockCritters( bool self_critters, bool only_players );
    void ProcessVisibleCritters();
    void ProcessVisibleItems();
    void ViewMap( Map* map, int look, ushort hx, ushort hy, int dir );
    void ClearVisible();

    Critter* GetCritSelf( uint crid, bool sync_lock );
    void     GetCrFromVisCr( CrVec& critters, int find_type, bool vis_cr_self, bool sync_lock );
    Critter* GetGlobalMapCritter( uint cr_id );

    bool AddCrIntoVisVec( Critter* add_cr );
    bool DelCrFromVisVec( Critter* del_cr );

    bool AddCrIntoVisSet1( uint crid );
    bool AddCrIntoVisSet2( uint crid );
    bool AddCrIntoVisSet3( uint crid );
    bool DelCrFromVisSet1( uint crid );
    bool DelCrFromVisSet2( uint crid );
    bool DelCrFromVisSet3( uint crid );

    bool AddIdVisItem( uint item_id );
    bool DelIdVisItem( uint item_id );
    bool CountIdVisItem( uint item_id );

    // Items
protected:
    ItemVec invItems;
    Item*   defItemSlotHand;
    Item*   defItemSlotArmor;

    void TakeDefaultItem( uchar slot );

public:
    Item* ItemSlotMain;
    Item* ItemSlotExt;
    Item* ItemSlotArmor;

    void     SyncLockItems();
    Item*    GetHandsItem() { return defItemSlotHand; }
    void     AddItem( Item*& item, bool send );
    void     SetItem( Item* item );
    void     EraseItem( Item* item, bool send );
    Item*    GetItem( uint item_id, bool skip_hide );
    Item*    GetInvItem( uint item_id, int transfer_type );
    ItemVec& GetItemsNoLock() { return invItems; }
    void     GetInvItems( ItemVec& items, int transfer_type, bool lock );
    Item*    GetItemByPid( hash item_pid );
    Item*    GetItemByPidInvPriority( hash item_pid );
    Item*    GetItemByPidSlot( hash item_pid, int slot );
    Item*    GetAmmoForWeapon( Item* weap );
    Item*    GetAmmo( int caliber );
    Item*    GetItemCar();
    Item*    GetItemSlot( int slot );
    void     GetItemsSlot( int slot, ItemVec& items, bool lock );
    void     GetItemsType( int type, ItemVec& items, bool lock );
    uint     CountItemPid( hash item_pid );
    bool     MoveItem( uchar from_slot, uchar to_slot, uint item_id, uint count );
    uint     RealCountItems() { return (uint) invItems.size(); }
    uint     CountItems();
    ItemVec& GetInventory();
    bool     IsHaveGeckItem();

public:
    bool SetScript( asIScriptFunction* func, bool first_time );
    // Knockout
    uint KnockoutAp;

    void ToKnockout( uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, ushort knock_hx, ushort knock_hy );
    void TryUpOnKnockout();

    // Respawn, Dead
    void ToDead( uint anim2, bool send_all );

    // Cached values to avoid synchronization
    uint CacheValuesNextTick;
    uint LookCacheValue;        // Critter::GetLook()

    // Break time
private:
    uint startBreakTime;
    uint breakTime;
    uint waitEndTick;

public:
    bool IsFree() { return Timer::GameTick() - startBreakTime >= breakTime; }
    bool IsBusy() { return !IsFree(); }
    void SetBreakTime( uint ms );
    void SetBreakTimeDelta( uint ms );
    void SetWait( uint ms ) { waitEndTick = Timer::GameTick() + ms; }
    bool IsWait()           { return Timer::GameTick() < waitEndTick; }

    // Send
    volatile int DisableSend;
    bool IsSendDisabled() { return DisableSend > 0; }
    void Send_Property( NetProperty::Type type, Property* prop, Entity* entity );
    void Send_Move( Critter* from_cr, uint move_params );
    void Send_Dir( Critter* from_cr );
    void Send_AddCritter( Critter* cr );
    void Send_RemoveCritter( Critter* cr );
    void Send_LoadMap( Map* map );
    void Send_XY( Critter* cr );
    void Send_AddItemOnMap( Item* item );
    void Send_EraseItemFromMap( Item* item );
    void Send_AnimateItem( Item* item, uchar from_frm, uchar to_frm );
    void Send_AddItem( Item* item );
    void Send_EraseItem( Item* item );
    void Send_ContainerInfo();
    void Send_ContainerInfo( Item* item_cont, uchar transfer_type, bool open_screen );
    void Send_ContainerInfo( Critter* cr_cont, uchar transfer_type, bool open_screen );
    void Send_GlobalInfo( uchar flags );
    void Send_GlobalLocation( Location* loc, bool add );
    void Send_GlobalMapFog( ushort zx, ushort zy, uchar fog );
    void Send_CustomCommand( Critter* cr, ushort cmd, int val );
    void Send_AllProperties();
    void Send_Talk();
    void Send_GameInfo( Map* map );
    void Send_Text( Critter* from_cr, const char* s_str, uchar how_say );
    void Send_TextEx( uint from_id, const char* s_str, ushort str_len, uchar how_say, bool unsafe_text );
    void Send_TextMsg( Critter* from_cr, uint str_num, uchar how_say, ushort num_msg );
    void Send_TextMsg( uint from_id, uint str_num, uchar how_say, ushort num_msg );
    void Send_TextMsgLex( Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems );
    void Send_TextMsgLex( uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems );
    void Send_Action( Critter* from_cr, int action, int action_ext, Item* item );
    void Send_Knockout( Critter* from_cr, uint anim2begin, uint anim2idle, ushort knock_hx, ushort knock_hy );
    void Send_MoveItem( Critter* from_cr, Item* item, uchar action, uchar prev_slot );
    void Send_Animate( Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play );
    void Send_SetAnims( Critter* from_cr, int cond, uint anim1, uint anim2 );
    void Send_CombatResult( uint* combat_res, uint len );
    void Send_AutomapsInfo( void* locs_vec, Location* loc );
    void Send_Effect( hash eff_pid, ushort hx, ushort hy, ushort radius );
    void Send_FlyEffect( hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy );
    void Send_PlaySound( uint crid_synchronize, const char* sound_name );
    void Send_PlaySoundType( uint crid_synchronize, uchar sound_type, uchar sound_type_ext, uchar sound_id, uchar sound_id_ext );

    // Send all
    void SendA_Property( NetProperty::Type type, Property* prop, Entity* entity );
    void SendA_Move( uint move_params );
    void SendA_XY();
    void SendA_Action( int action, int action_ext, Item* item );
    void SendAA_Action( int action, int action_ext, Item* item );
    void SendA_Knockout( uint anim2begin, uint anim2idle, ushort knock_hx, ushort knock_hy );
    void SendAA_MoveItem( Item* item, uchar action, uchar prev_slot );
    void SendAA_Animate( uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play );
    void SendAA_SetAnims( int cond, uint anim1, uint anim2 );
    void SendAA_Text( CrVec& to_cr, const char* str, uchar how_say, bool unsafe_text );
    void SendAA_Msg( CrVec& to_cr, uint num_str, uchar how_say, ushort num_msg );
    void SendAA_MsgLex( CrVec& to_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems );
    void SendA_Dir();
    void SendA_CustomCommand( ushort num_param, int val );

    // Chosen data
    void Send_AddAllItems();
    void Send_AllAutomapsInfo();

    bool        IsPlayer() { return !CritterIsNpc; }
    bool        IsNpc()    { return CritterIsNpc; }
    void        RefreshName();
    const char* GetInfo();
    uint        GetItemsWeight();
    uint        GetItemsVolume();
    bool        IsOverweight();
    int         GetFreeWeight();
    int         GetFreeVolume();
    void        SendMessage( int num, int val, int to );
    uint        GetUseApCost( Item* weap, int use );
    uint        GetAttackDist( Item* weap, int use );
    uint        GetUseDist();
    bool        IsLife();
    bool        IsDead();
    bool        IsKnockout();
    bool        CheckFind( int find_type );
    int         GetRealAp();
    int         GetAllAp();
    void        SubAp( int val );
    bool        IsDmgLeg();
    bool        IsDmgTwoLeg();
    bool        IsDmgArm();
    bool        IsDmgTwoArm();

    int GetApCostCritterMove( bool is_run );
    int GetApCostMoveItemContainer();
    int GetApCostMoveItemInventory();
    int GetApCostPickItem();
    int GetApCostDropItem();
    int GetApCostPickCritter();
    int GetApCostUseSkill();

    // Timeouts
    bool IsTransferTimeouts( bool send );

    // Current container
    uint AccessContainerId;
    uint ItemTransferCount;

    // Home
    uint TryingGoHomeTick;

    void SetHome( uint map_id, ushort hx, ushort hy, uchar dir );
    bool IsInHome();

    // Enemy stack
    void     AddEnemyToStack( uint crid );
    bool     CheckEnemyInStack( uint crid );
    void     EraseEnemyInStack( uint crid );
    Critter* ScanEnemyStack();

    // Locations
    bool CheckKnownLocById( uint loc_id );
    bool CheckKnownLocByPid( hash loc_pid );
    void AddKnownLoc( uint loc_id );
    void EraseKnownLoc( uint loc_id );

    // Time events
    void AddCrTimeEvent( hash func_num, uint rate, uint duration, int identifier );
    void EraseCrTimeEvent( int index );
    void ContinueTimeEvents( int offs_time );

    // Other
    CrVec* GlobalMapGroup;
    uint   IdleNextTick;
    uint   ApRegenerationTick;

    bool   CanBeRemoved;
};

class Client: public Critter
{
public:
    Client( ProtoCritter* proto );
    ~Client();

    char          Name[ UTF8_BUF_SIZE( MAX_NAME ) ];
    uchar         Access;
    uint          LanguageMsg;
    uint          UID[ 5 ];
    SOCKET        Sock;
    sockaddr_in   From;
    BufferManager Bin, Bout;
    UCharVec      NetIOBuffer;
    int           GameState;
    bool          IsDisconnected;
    uint          DisconnectTick;
    bool          DisableZlib;
    z_stream      Zstrm;
    bool          ZstrmInit;
    uint          LastActivityTime;
    uint          LastSendedMapTick;
    char          LastSay[ UTF8_BUF_SIZE( MAX_CHAT_MESSAGE ) ];
    uint          LastSayEqualCount;
    uint          RadioMessageSended;
    int           UpdateFileIndex;
    uint          UpdateFilePortion;

    #if defined ( USE_LIBEVENT )
    struct NetIOArg
    {
        Mutex         Locker;
        Client*       PClient;
        bufferevent*  BEV;
        # if defined ( LIBEVENT_TIMEOUTS_WORKAROUND )
        MutexSpinlock BEVLocker;
        # endif
    }* NetIOArgPtr;
    # define BIN_BEGIN( cl_ )     cl_->Bin.Lock()
    # define BIN_END( cl_ )       cl_->Bin.Unlock()
    # define BOUT_BEGIN( cl_ )    cl_->Bout.Lock()
    # if defined ( LIBEVENT_TIMEOUTS_WORKAROUND )
    typedef void ( *SendCallback )( bufferevent*, void* );
    static SendCallback SendData;
    #  define BOUT_END( cl_ )                                                 \
        cl_->Bout.Unlock();                                                   \
        cl_->NetIOArgPtr->BEVLocker.Lock();                                   \
        if( cl_->NetIOArgPtr->BEV )                                           \
        {                                                                     \
            bufferevent_lock( cl_->NetIOArgPtr->BEV );                        \
            cl_->NetIOArgPtr->BEVLocker.Unlock();                             \
            ( *Client::SendData )( cl_->NetIOArgPtr->BEV, cl_->NetIOArgPtr ); \
            bufferevent_unlock( cl_->NetIOArgPtr->BEV );                      \
        }                                                                     \
        else                                                                  \
        {                                                                     \
            cl_->NetIOArgPtr->BEVLocker.Unlock();                             \
        }
    # else
    #  define BOUT_END( cl_ )     cl_->Bout.Unlock();
    # endif
    #else // IOCP
    struct NetIOArg
    {
        WSAOVERLAPPED OV;
        Mutex         Locker;
        void*         PClient;
        WSABUF        Buffer;
        long          Operation;
        DWORD         Flags;
        DWORD         Bytes;
    }* NetIOIn, * NetIOOut;
    # define WSA_BUF_SIZE    ( 4096 )
    # define WSAOP_FREE      ( 0 )
    # define WSAOP_SEND      ( 1 )
    # define WSAOP_RECV      ( 2 )
    typedef void ( *SendCallback )( NetIOArg* );
    static SendCallback SendData;
    # define BIN_BEGIN( cl_ )     cl_->Bin.Lock()
    # define BIN_END( cl_ )       cl_->Bin.Unlock()
    # define BOUT_BEGIN( cl_ )    cl_->Bout.Lock()
    # define BOUT_END( cl_ )                                                                                                                         \
        cl_->Bout.Unlock(); if( !cl_->IsOffline() && InterlockedCompareExchange( &cl_->NetIOOut->Operation, WSAOP_SEND, WSAOP_FREE ) == WSAOP_FREE ) \
            ( *Client::SendData )( cl_->NetIOOut )
    #endif
    void Shutdown();

public:
    uint        GetIp();
    const char* GetIpStr();
    ushort      GetPort();
    bool        IsOnline();
    bool        IsOffline();
    void        Disconnect();
    void        RemoveFromGame();
    uint        GetOfflineTime();
    const char* GetBinPassHash();
    void        SetBinPassHash( const char* pass_hash );

    // Ping
private:
    uint pingNextTick;
    bool pingOk;

public:
    bool IsToPing();
    void PingClient();
    void PingOk( uint next_ping );

    // Sends
public:
    void Send_Property( NetProperty::Type type, Property* prop, Entity* entity );
    void Send_Move( Critter* from_cr, uint move_params );
    void Send_Dir( Critter* from_cr );
    void Send_AddCritter( Critter* cr );
    void Send_RemoveCritter( Critter* cr );
    void Send_LoadMap( Map* map );
    void Send_XY( Critter* cr );
    void Send_AddItemOnMap( Item* item );
    void Send_EraseItemFromMap( Item* item );
    void Send_AnimateItem( Item* item, uchar from_frm, uchar to_frm );
    void Send_AddItem( Item* item );
    void Send_EraseItem( Item* item );
    void Send_ContainerInfo();
    void Send_ContainerInfo( Item* item_cont, uchar transfer_type, bool open_screen );
    void Send_ContainerInfo( Critter* cr_cont, uchar transfer_type, bool open_screen );
    void Send_GlobalInfo( uchar flags );
    void Send_GlobalLocation( Location* loc, bool add );
    void Send_GlobalMapFog( ushort zx, ushort zy, uchar fog );
    void Send_CustomCommand( Critter* cr, ushort cmd, int val );
    void Send_AllProperties();
    void Send_Talk();
    void Send_GameInfo( Map* map );
    void Send_Text( Critter* from_cr, const char* s_str, uchar how_say );
    void Send_TextEx( uint from_id, const char* s_str, ushort str_len, uchar how_say, bool unsafe_text );
    void Send_TextMsg( Critter* from_cr, uint str_num, uchar how_say, ushort num_msg );
    void Send_TextMsg( uint from_id, uint str_num, uchar how_say, ushort num_msg );
    void Send_TextMsgLex( Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems );
    void Send_TextMsgLex( uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems );
    void Send_Action( Critter* from_cr, int action, int action_ext, Item* item );
    void Send_Knockout( Critter* from_cr, uint anim2begin, uint anim2idle, ushort knock_hx, ushort knock_hy );
    void Send_MoveItem( Critter* from_cr, Item* item, uchar action, uchar prev_slot );
    void Send_Animate( Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play );
    void Send_SetAnims( Critter* from_cr, int cond, uint anim1, uint anim2 );
    void Send_CombatResult( uint* combat_res, uint len );
    void Send_AutomapsInfo( void* locs_vec, Location* loc );
    void Send_Effect( hash eff_pid, ushort hx, ushort hy, ushort radius );
    void Send_FlyEffect( hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy );
    void Send_PlaySound( uint crid_synchronize, const char* sound_name );
    void Send_PlaySoundType( uint crid_synchronize, uchar sound_type, uchar sound_type_ext, uchar sound_id, uchar sound_id_ext );
    void Send_MapText( ushort hx, ushort hy, uint color, const char* text, ushort text_len, bool unsafe_text );
    void Send_MapTextMsg( ushort hx, ushort hy, uint color, ushort num_msg, uint num_str );
    void Send_MapTextMsgLex( ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, const char* lexems, ushort lexems_len );
    void Send_UserHoloStr( uint str_num, const char* text, ushort text_len );
    void Send_PlayersBarter( uchar barter, uint param, uint param_ext );
    void Send_PlayersBarterSetHide( Item* item, uint count );
    void Send_RunClientScript( const char* func_name, int p0, int p1, int p2, const char* p3, UIntVec& p4 );
    void Send_ViewMap();
    void Send_CheckUIDS();
    void Send_SomeItem( Item* item );                                     // Without checks
    void Send_CustomMessage( uint msg );
    void Send_CustomMessage( uint msg, uchar* data, uint data_size );

    // Players barter
    bool BarterOffer;
    bool BarterHide;
    uint BarterOpponent;
    struct BarterItem
    {
        uint Id;
        uint Count;
        bool operator==( uint id ) { return Id == id; }
        BarterItem( uint id, uint count ): Id( id ), Count( count ) {}
    };
    typedef vector< BarterItem > BarterItemVec;
    BarterItemVec BarterItems;

    Client*     BarterGetOpponent( uint opponent_id );
    void        BarterEnd();
    void        BarterRefresh( Client* opponent );
    BarterItem* BarterGetItem( uint item_id );
    void        BarterEraseItem( uint item_id );

    // Dialogs
private:
    uint talkNextTick;

public:
    Talking Talk;
    bool IsTalking() { return Talk.TalkType != TALK_NONE; }
    void ProcessTalk( bool force );
    void CloseTalk();
};

class Npc: public Critter
{
public:
    Npc( uint id, ProtoCritter* proto );
    ~Npc();

    // Bags
public:
    uint NextRefreshBagTick;
    bool IsNeedRefreshBag() { return IsLife() && Timer::GameTick() > NextRefreshBagTick && IsNoPlanes(); }
    void RefreshBag();

    // AI
private:
    AIDataPlaneVec aiPlanes;

public:
    bool            AddPlane( int reason, AIDataPlane* plane, bool is_child, Critter* some_cr = nullptr, Item* some_item = nullptr );
    void            NextPlane( int reason, Critter* some_cr = nullptr, Item* some_item = nullptr );
    bool            RunPlane( int reason, uint& r0, uint& r1, uint& r2 );
    bool            IsPlaneAviable( int plane_type );
    bool            IsCurPlane( int plane_type );
    AIDataPlane*    GetCurPlane() { return aiPlanes.size() ? aiPlanes[ 0 ]->GetCurPlane() : nullptr; }
    AIDataPlaneVec& GetPlanes()   { return aiPlanes; }
    void            DropPlanes();
    void            SetBestCurPlane();
    bool            IsNoPlanes() { return aiPlanes.empty(); }

    // Dialogs
public:
    uint GetTalkedPlayers();
    bool IsTalkedPlayers();
    uint GetBarterPlayers();
    bool IsFreeToTalk();
    bool IsPlaneNoTalk();

    // Target
public:
    void MoveToHex( int reason, ushort hx, ushort hy, uchar ori, bool is_run, uchar cut );
    void SetTarget( int reason, Critter* target, int min_hp, bool is_gag );
};

#endif // __CRITTER__
