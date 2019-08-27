#ifndef __CRITTER__
#define __CRITTER__

#include "Common.h"
#include "Timer.h"
#include "Networking.h"
#include "Item.h"
#include "Dialogs.h"
#include "DataMask.h"
#include "Entity.h"

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

typedef map< uint, Critter* > CritterMap;
typedef map< uint, Client* >  ClMap;
typedef map< uint, Npc* >     PcMap;

typedef vector< Critter* >    CritterVec;
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
    CLASS_PROPERTY( hash, RefMapId );
    CLASS_PROPERTY( hash, RefMapPid );
    CLASS_PROPERTY( uint, RefLocationId );
    CLASS_PROPERTY( hash, RefLocationPid );
    CLASS_PROPERTY( ushort, HexX );
    CLASS_PROPERTY( ushort, HexY );
    CLASS_PROPERTY( uchar, Dir );
    CLASS_PROPERTY( string, Password );
    CLASS_PROPERTY( int, Cond ); // enum CritterCondition
    CLASS_PROPERTY( bool, ClientToDelete );
    CLASS_PROPERTY( uint, Multihex );
    CLASS_PROPERTY( ushort, WorldX );
    CLASS_PROPERTY( ushort, WorldY );
    CLASS_PROPERTY( uint, GlobalMapLeaderId );
    CLASS_PROPERTY( uint, GlobalMapTripId );
    CLASS_PROPERTY( uint, RefGlobalMapLeaderId );
    CLASS_PROPERTY( uint, RefGlobalMapTripId );
    CLASS_PROPERTY( ushort, LastMapHexX );
    CLASS_PROPERTY( ushort, LastMapHexY );
    CLASS_PROPERTY( uint, Anim1Life );
    CLASS_PROPERTY( uint, Anim1Knockout );
    CLASS_PROPERTY( uint, Anim1Dead );
    CLASS_PROPERTY( uint, Anim2Life );
    CLASS_PROPERTY( uint, Anim2Knockout );
    CLASS_PROPERTY( uint, Anim2Dead );
    CLASS_PROPERTY( CScriptArray *, GlobalMapFog );
    CLASS_PROPERTY( CScriptArray *, TE_FuncNum );    // hash
    CLASS_PROPERTY( CScriptArray *, TE_Rate );       // uint
    CLASS_PROPERTY( CScriptArray *, TE_NextTime );   // uint
    CLASS_PROPERTY( CScriptArray *, TE_Identifier ); // int
    // ...
    CLASS_PROPERTY( uint, LookDistance );
    CLASS_PROPERTY( hash, DialogId );
    CLASS_PROPERTY( bool, IsNoTalk );
    CLASS_PROPERTY( int, MaxTalkers );   // Callback on begin dialog?
    CLASS_PROPERTY( uint, TalkDistance );
    CLASS_PROPERTY( int, CurrentHp );
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
    CLASS_PROPERTY( bool, IsNoHome );
    CLASS_PROPERTY( uint, HomeMapId );
    CLASS_PROPERTY( ushort, HomeHexX );
    CLASS_PROPERTY( ushort, HomeHexY );
    CLASS_PROPERTY( uchar, HomeDir );
    CLASS_PROPERTY( bool, IsHide );
    CLASS_PROPERTY( CScriptArray *, KnownLocations );
    CLASS_PROPERTY( CScriptArray *, ConnectionIp );
    CLASS_PROPERTY( CScriptArray *, ConnectionPort );
    CLASS_PROPERTY( uint, ShowCritterDist1 );
    CLASS_PROPERTY( uint, ShowCritterDist2 );
    CLASS_PROPERTY( uint, ShowCritterDist3 );
    CLASS_PROPERTY( hash, ScriptId );
    CLASS_PROPERTY( int, SneakCoefficient );
    // Exclude
    CLASS_PROPERTY( hash, NpcRole );            // Find Npc criteria (maybe swap to some universal prop/value array as input)
    CLASS_PROPERTY( bool, IsNoUnarmed );        // AI

protected:
    Critter( uint id, EntityType type, ProtoCritter* proto );
    ~Critter();

public:
    // Data
    bool        CritterIsNpc;
    uint        Flags;
    string      Name;
    bool        IsRunning;
    int         LockMapTransfers;
    uint        AllowedToDownloadMap;

    static bool SlotEnabled[ 0x100 ];
    static bool SlotDataSendEnabled[ 0x100 ];

    void DeleteInventory();

    // Visible critters and items
    CritterVec VisCr;
    CritterVec VisCrSelf;
    CritterMap VisCrMap;
    CritterMap VisCrSelfMap;
    UIntSet    VisCr1, VisCr2, VisCr3;
    UIntSet    VisItem;
    Mutex      VisItemLocker;
    uint       ViewMapId;
    hash       ViewMapPid;
    ushort     ViewMapLook, ViewMapHx, ViewMapHy;
    uchar      ViewMapDir;
    uint       ViewMapLocId, ViewMapLocEnt;

    Map* GetMap();

    void ProcessVisibleCritters();
    void ProcessVisibleItems();
    void ViewMap( Map* map, int look, ushort hx, ushort hy, int dir );
    void ClearVisible();

    Critter* GetCritSelf( uint crid );
    void     GetCrFromVisCr( CritterVec& critters, int find_type, bool vis_cr_self );
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

public:
    void     AddItem( Item*& item, bool send );
    void     SetItem( Item* item );
    void     EraseItem( Item* item, bool send );
    Item*    GetItem( uint item_id, bool skip_hide );
    ItemVec& GetItemsNoLock() { return invItems; }
    Item*    GetItemByPid( hash item_pid );
    Item*    GetItemByPidInvPriority( hash item_pid );
    Item*    GetItemByPidSlot( hash item_pid, int slot );
    Item*    GetItemSlot( int slot );
    void     GetItemsSlot( int slot, ItemVec& items );
    uint     CountItemPid( hash item_pid );
    uint     RealCountItems() { return (uint) invItems.size(); }
    uint     CountItems();
    ItemVec& GetInventory();
    bool     IsHaveGeckItem();

public:
    bool SetScript( asIScriptFunction* func, bool first_time );

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

    // Moving
public:
    struct
    {
        int    State;
        uint   TargId;
        ushort HexX;
        ushort HexY;
        uint   Cut;
        bool   IsRun;
        uint   PathNum;
        uint   Iter;
        uint   Trace;
        uint   GagEntityId;
    } Moving;

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
    void Send_GlobalInfo( uchar flags );
    void Send_GlobalLocation( Location* loc, bool add );
    void Send_GlobalMapFog( ushort zx, ushort zy, uchar fog );
    void Send_CustomCommand( Critter* cr, ushort cmd, int val );
    void Send_AllProperties();
    void Send_Talk();
    void Send_GameInfo( Map* map );
    void Send_Text( Critter* from_cr, const string& text, uchar how_say );
    void Send_TextEx( uint from_id, const string& text, uchar how_say, bool unsafe_text );
    void Send_TextMsg( Critter* from_cr, uint str_num, uchar how_say, ushort num_msg );
    void Send_TextMsg( uint from_id, uint str_num, uchar how_say, ushort num_msg );
    void Send_TextMsgLex( Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems );
    void Send_TextMsgLex( uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems );
    void Send_Action( Critter* from_cr, int action, int action_ext, Item* item );
    void Send_MoveItem( Critter* from_cr, Item* item, uchar action, uchar prev_slot );
    void Send_Animate( Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play );
    void Send_SetAnims( Critter* from_cr, int cond, uint anim1, uint anim2 );
    void Send_CombatResult( uint* combat_res, uint len );
    void Send_AutomapsInfo( void* locs_vec, Location* loc );
    void Send_Effect( hash eff_pid, ushort hx, ushort hy, ushort radius );
    void Send_FlyEffect( hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy );
    void Send_PlaySound( uint crid_synchronize, const string& sound_name );

    // Send all
    void SendA_Property( NetProperty::Type type, Property* prop, Entity* entity );
    void SendA_Move( uint move_params );
    void SendA_XY();
    void SendA_Action( int action, int action_ext, Item* item );
    void SendAA_Action( int action, int action_ext, Item* item );
    void SendAA_MoveItem( Item* item, uchar action, uchar prev_slot );
    void SendAA_Animate( uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play );
    void SendAA_SetAnims( int cond, uint anim1, uint anim2 );
    void SendAA_Text( const CritterVec& to_cr, const string& text, uchar how_say, bool unsafe_text );
    void SendAA_Msg( const CritterVec& to_cr, uint num_str, uchar how_say, ushort num_msg );
    void SendAA_MsgLex( const CritterVec& to_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems );
    void SendA_Dir();
    void SendA_CustomCommand( ushort num_param, int val );

    // Chosen data
    void Send_AddAllItems();
    void Send_AllAutomapsInfo();

    bool IsPlayer() { return !CritterIsNpc; }
    bool IsNpc()    { return CritterIsNpc; }
    void RefreshName();
    void SendMessage( int num, int val, int to );
    uint GetAttackDist( Item* weap, int use );
    bool IsLife();
    bool IsDead();
    bool IsKnockout();
    bool CheckFind( int find_type );

    // Timeouts
    bool IsTransferTimeouts( bool send );

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
    CritterVec* GlobalMapGroup;
    bool        CanBeRemoved;
};

#define BIN_BEGIN( cl_ )     cl_->Connection->Bin.Lock()
#define BIN_END( cl_ )       cl_->Connection->Bin.Unlock()
#define BOUT_BEGIN( cl_ )    cl_->Connection->Bout.Lock()
#define BOUT_END( cl_ )      cl_->Connection->Bout.Unlock(); cl_->Connection->Dispatch()

class Client: public Critter
{
public:
    Client( NetConnection* conn, ProtoCritter* proto );
    ~Client();

    NetConnection* Connection;
    uchar          Access;
    uint           LanguageMsg;
    int            GameState;
    uint           LastActivityTime;
    uint           LastSendedMapTick;
    char           LastSay[ UTF8_BUF_SIZE( MAX_CHAT_MESSAGE ) ];
    uint           LastSayEqualCount;
    uint           RadioMessageSended;
    int            UpdateFileIndex;
    uint           UpdateFilePortion;

public:
    uint        GetIp();
    const char* GetIpStr();
    ushort      GetPort();
    bool        IsOnline();
    bool        IsOffline();
    void        Disconnect();
    void        RemoveFromGame();
    uint        GetOfflineTime();

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
    void Send_SomeItems( CScriptArray* items_arr, int param );
    void Send_GlobalInfo( uchar flags );
    void Send_GlobalLocation( Location* loc, bool add );
    void Send_GlobalMapFog( ushort zx, ushort zy, uchar fog );
    void Send_CustomCommand( Critter* cr, ushort cmd, int val );
    void Send_AllProperties();
    void Send_Talk();
    void Send_GameInfo( Map* map );
    void Send_Text( Critter* from_cr, const string& text, uchar how_say );
    void Send_TextEx( uint from_id, const string& text, uchar how_say, bool unsafe_text );
    void Send_TextMsg( Critter* from_cr, uint str_num, uchar how_say, ushort num_msg );
    void Send_TextMsg( uint from_id, uint str_num, uchar how_say, ushort num_msg );
    void Send_TextMsgLex( Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems );
    void Send_TextMsgLex( uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems );
    void Send_Action( Critter* from_cr, int action, int action_ext, Item* item );
    void Send_MoveItem( Critter* from_cr, Item* item, uchar action, uchar prev_slot );
    void Send_Animate( Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play );
    void Send_SetAnims( Critter* from_cr, int cond, uint anim1, uint anim2 );
    void Send_CombatResult( uint* combat_res, uint len );
    void Send_AutomapsInfo( void* locs_vec, Location* loc );
    void Send_Effect( hash eff_pid, ushort hx, ushort hy, ushort radius );
    void Send_FlyEffect( hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy );
    void Send_PlaySound( uint crid_synchronize, const string& sound_name );
    void Send_MapText( ushort hx, ushort hy, uint color, const string& text, bool unsafe_text );
    void Send_MapTextMsg( ushort hx, ushort hy, uint color, ushort num_msg, uint num_str );
    void Send_MapTextMsgLex( ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, const char* lexems, ushort lexems_len );
    void Send_ViewMap();
    void Send_SomeItem( Item* item );                                     // Without checks
    void Send_CustomMessage( uint msg );
    void Send_CustomMessage( uint msg, uchar* data, uint data_size );

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

    // Dialogs
public:
    uint GetTalkedPlayers();
    bool IsTalkedPlayers();
    uint GetBarterPlayers();
    bool IsFreeToTalk();
};

#endif // __CRITTER__
