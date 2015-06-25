#ifndef __SERVER__
#define __SERVER__

#include "Common.h"
#include "Script.h"
#include "Item.h"
#include "Critter.h"
#include "Map.h"
#include "MapManager.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "Dialogs.h"
#include "CraftManager.h"
#include "ConstantsManager.h"
#include "CritterType.h"
#include "NetProtocol.h"
#include "Access.h"

#if defined ( USE_LIBEVENT )
# include "event2/event.h"
# include "event2/bufferevent.h"
# include "event2/buffer.h"
# include "event2/thread.h"
#endif

// Check buffer for error
#define CHECK_IN_BUFF_ERROR( client )    CHECK_IN_BUFF_ERROR_EX( client, 0 )
#define CHECK_IN_BUFF_ERROR_EX( client, ext )                                           \
    if( client->Bin.IsError() )                                                         \
    {                                                                                   \
        WriteLogF( _FUNC_, " - Wrong MSG data from client<%s>.\n", client->GetInfo() ); \
        ext;                                                                            \
        client->Disconnect();                                                           \
        client->Bin.LockReset();                                                        \
        return;                                                                         \
    }

class FOServer
{
public:
    FOServer();
    ~FOServer();

    // Net process
    static void Process_ParseToGame( Client* cl );
    static void Process_Move( Client* cl );
    static void Process_Update( Client* cl );
    static void Process_UpdateFile( Client* cl );
    static void Process_UpdateFileData( Client* cl );
    static void Process_CreateClient( Client* cl );
    static void Process_LogIn( ClientPtr& cl );
    static void Process_SingleplayerSaveLoad( Client* cl );
    static void Process_Dir( Client* cl );
    static void Process_ChangeItem( Client* cl );
    static void Process_UseItem( Client* cl );
    static void Process_PickItem( Client* cl );
    static void Process_PickCritter( Client* cl );
    static void Process_ContainerItem( Client* cl );
    static void Process_UseSkill( Client* cl );
    static void Process_RuleGlobal( Client* cl );
    static void Process_Text( Client* cl );
    static void Process_Command( BufferManager& buf, void ( * logcb )( const char* ), Client* cl, const char* admin_panel );
    static void Process_Command2( BufferManager& buf, void ( * logcb )( const char* ), Client* cl, const char* admin_panel );
    static void Process_Dialog( Client* cl, bool is_say );
    static void Process_Barter( Client* cl );
    static void Process_GiveMap( Client* cl );
    static void Process_SetUserHoloStr( Client* cl );
    static void Process_GetUserHoloStr( Client* cl );
    static void Process_LevelUp( Client* cl );
    static void Process_CraftAsk( Client* cl );
    static void Process_Craft( Client* cl );
    static void Process_Ping( Client* cl );
    static void Process_PlayersBarter( Client* cl );
    static void Process_ScreenAnswer( Client* cl );
    static void Process_Combat( Client* cl );
    static void Process_RunServerScript( Client* cl );
    static void Process_KarmaVoting( Client* cl );
    static void Process_Property( Client* cl, uint data_size );

    static void Send_MapData( Client* cl, ProtoMap* pmap, uchar send_info );

    // Update files
    struct UpdateFile
    {
        uint   Size;
        uchar* Data;
    };
    typedef vector< UpdateFile > UpdateFileVec;
    static UpdateFileVec UpdateFiles;
    static UCharVec      UpdateFilesList;

    static void GenerateUpdateFiles( bool first_generation = false );

    // Holodisks
    struct HoloInfo
    {
        bool   CanRewrite;
        string Title;
        string Text;
        HoloInfo( bool can_rw, const char* title, const char* text ): CanRewrite( can_rw ), Title( title ), Text( text ) {}
    };
    typedef map< uint, HoloInfo* > HoloInfoMap;
    static HoloInfoMap HolodiskInfo;
    static Mutex       HolodiskLocker;
    static uint        LastHoloId;

    static void      SaveHoloInfoFile();
    static bool      LoadHoloInfoFile( void* f, uint version );
    static HoloInfo* GetHoloInfo( uint id )
    {
        auto it = HolodiskInfo.find( id );
        return it != HolodiskInfo.end() ? ( *it ).second : NULL;
    }
    static void AddPlayerHoloInfo( Critter* cr, uint holo_num, bool send );
    static void ErasePlayerHoloInfo( Critter* cr, uint index, bool send );
    static void Send_PlayerHoloInfo( Critter* cr, uint holo_num, bool send_text );

    // Actions
    static bool Act_Move( Critter* cr, ushort hx, ushort hy, uint move_params );
    static bool Act_Attack( Critter* cr, uchar rate_weap, uint target_id );
    static bool Act_Reload( Critter* cr, uint weap_id, uint ammo_id );
    static bool Act_Use( Critter* cr, uint item_id, int skill, int target_type, uint target_id, hash target_pid, uint param );
    static bool Act_PickItem( Critter* cr, ushort hx, ushort hy, hash pid );

    static void KillCritter( Critter* cr, uint anim2, Critter* attacker );
    static void RespawnCritter( Critter* cr );
    static void KnockoutCritter( Critter* cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, ushort knock_hx, ushort knock_hy );
    static bool MoveRandom( Critter* cr );
    static bool RegenerateMap( Map* map );
    static bool VerifyTrigger( Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir );

    // Scripting
    static StrVec  ServerWrongGlobalObjects;
    static Pragmas ServerPropertyPragmas;

    // Init/Finish system
    static bool InitScriptSystem();
    static bool PostInitScriptSystem();
    static void FinishScriptSystem();
    static void ScriptSystemUpdate();

    // Dialogs demand and result
    static bool DialogScriptDemand( DemandResult& demand, Critter* master, Critter* slave );
    static uint DialogScriptResult( DemandResult& result, Critter* master, Critter* slave );

    // Client script
    static bool RequestReloadClientScripts;
    static bool ReloadClientScripts();
    static bool ReloadMapperScripts();

    // Pragma callbacks
    static bool PragmaCallbackCrData( const char* text );
    static bool PragmaCallbackCrClData( const char* text );

    // Text listen
    #define TEXT_LISTEN_FIRST_STR_MAX_LEN    ( 63 )
    struct TextListen
    {
        uint   FuncId;
        int    SayType;
        char   FirstStr[ TEXT_LISTEN_FIRST_STR_MAX_LEN + 1 ];
        uint   FirstStrLen;
        uint64 Parameter;
    };
    typedef vector< TextListen > TextListenVec;
    static TextListenVec TextListeners;
    static Mutex         TextListenersLocker;

//	void GlobalEventCritterUseItem(Critter* cr);
//	void GlobalEventCritterUseSkill(Critter* cr);
//	void GlobalEventCritterInit(Critter* cr, bool first_time);
//	void GlobalEventCritterFinish(Critter* cr, bool to_delete);
//	void GlobalEventCritterIdle(Critter* cr);
//	void GlobalEventCritterDead(Critter* cr);

    static void OnSendGlobalValue( void* obj, Property* prop, void* cur_value, void* old_value );
    static void OnSendCritterValue( void* obj, Property* prop, void* cur_value, void* old_value );
    static void OnSendMapValue( void* obj, Property* prop, void* cur_value, void* old_value );
    static void OnSendLocationValue( void* obj, Property* prop, void* cur_value, void* old_value );

    // Critters
    static void OnSetCritterHandsItemProtoId( void* obj, Property* prop, void* cur_value, void* old_value );
    static void OnSetCritterHandsItemMode( void* obj, Property* prop, void* cur_value, void* old_value );

    // Items
    static Item* CreateItemOnHex( Map* map, ushort hx, ushort hy, hash pid, uint count, bool check_blocks = true );
    static bool  TransferAllItems();
    static void  OnSendItemValue( void* obj, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemCount( void* obj, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemChangeView( void* obj, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemRecacheHex( void* obj, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemIsGeck( void* obj, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemIsRadio( void* obj, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemTrapValue( void* obj, Property* prop, void* cur_value, void* old_value );

    // Npc
    static void ProcessAI( Npc* npc );
    static bool AI_Stay( Npc* npc, uint ms );
    static bool AI_Move( Npc* npc, ushort hx, ushort hy, bool is_run, uint cut, uint trace );
    static bool AI_MoveToCrit( Npc* npc, uint targ_id, uint cut, uint trace, bool is_run );
    static bool AI_MoveItem( Npc* npc, Map* map, uchar from_slot, uchar to_slot, uint item_id, uint count );
    static bool AI_Attack( Npc* npc, Map* map, uchar mode, uint targ_id );
    static bool AI_PickItem( Npc* npc, Map* map, ushort hx, ushort hy, hash pid, uint use_item_id );
    static bool AI_ReloadWeapon( Npc* npc, Map* map, Item* weap, uint ammo_id );
    static bool TransferAllNpc();
    static void ProcessCritter( Critter* cr );
    static bool Dialog_Compile( Npc* npc, Client* cl, const Dialog& base_dlg, Dialog& compiled_dlg );
    static bool Dialog_CheckDemand( Npc* npc, Client* cl, DialogAnswer& answer, bool recheck );
    static uint Dialog_UseResult( Npc* npc, Client* cl, DialogAnswer& answer );
    static void Dialog_Begin( Client* cl, Npc* npc, hash dlg_pack_id, ushort hx, ushort hy, bool ignore_distance );

    // Main
    static uint    CpuCount;
    static int     UpdateIndex, UpdateLastIndex;
    static uint    UpdateLastTick;
    static bool    Active, ActiveInProcess, ActiveOnce;
    static ClVec   SaveClients;
    static Mutex   SaveClientsLocker;
    static UIntMap RegIp;
    static Mutex   RegIpLocker;

    static void DisconnectClient( Client* cl );
    static void RemoveClient( Client* cl );
    static void DeleteClientFile( const char* client_name );
    static void AddSaveClient( Client* cl );
    static void EraseSaveClient( uint crid );
    static void Process( ClientPtr& cl );

    // Log to client
    static ClVec LogClients;
    static void LogToClients( const char* str );

    // Game time
    static void SaveGameInfoFile();
    static bool LoadGameInfoFile( void* f, uint version );
    static void InitGameTime();
    static void SetGameTime( int multiplier, int year, int month, int day, int hour, int minute, int second );

    // Lang packs
    static LangPackVec LangPacks;     // Todo: synchronize
    static bool InitCrafts( LangPackVec& lang_packs );
    static bool InitLangPacks( LangPackVec& lang_packs );
    static bool InitLangPacksDialogs( LangPackVec& lang_packs );
    static bool InitLangPacksLocations( LangPackVec& lang_packs );
    static bool InitLangPacksItems( LangPackVec& lang_packs );
    static void FinishLangPacks();
    static bool InitLangCrTypes( LangPackVec& lang_packs );

    // Init/Finish
    static bool Init();
    static bool InitReal();
    static void Finish();
    static bool Starting() { return Active && ActiveInProcess; }
    static bool Started()  { return Active && !ActiveInProcess; }
    static bool Stopping() { return !Active && ActiveInProcess; }
    static bool Stopped()  { return !Active && !ActiveInProcess; }
    static void MainLoop();

    static Thread*           LogicThreads;
    static uint              LogicThreadCount;
    static bool              LogicThreadSetAffinity;
    static MutexSynchronizer LogicThreadSync;
    static void SynchronizeLogicThreads();
    static void ResynchronizeLogicThreads();
    static void Logic_Work( void* data );

    // Net IO
    static ClVec  ConnectedClients;
    static Mutex  ConnectedClientsLocker;
    static SOCKET ListenSock;
    static Thread ListenThread;

    static void Net_Listen( void* );

    #if defined ( USE_LIBEVENT )
    static event_base* NetIOEventHandler;
    static Thread      NetIOThread;
    static uint        NetIOThreadsCount;

    static void NetIO_Loop( void* );
    static void NetIO_Event( bufferevent* bev, short what, void* arg );
    static void NetIO_Input( bufferevent* bev, void* arg );
    static void NetIO_Output( bufferevent* bev, void* arg );
    #else // IOCP
    static HANDLE  NetIOCompletionPort;
    static Thread* NetIOThreads;
    static uint    NetIOThreadsCount;

    static void NetIO_Work( void* );
    static void NetIO_Input( Client::NetIOArg* io );
    static void NetIO_Output( Client::NetIOArg* io );
    #endif

    // Dump save/load
    struct ClientSaveData
    {
        char                    Name[ UTF8_BUF_SIZE( MAX_NAME ) ];
        char                    PasswordHash[ PASS_HASH_SIZE ];
        CritData                Data;
        Properties*             Props;
        Critter::CrTimeEventVec TimeEvents;

        ClientSaveData()
        {
            Props = NULL;
            Clear();
        }

        void Clear()
        {
            memzero( Name, sizeof( Name ) );
            memzero( PasswordHash, sizeof( PasswordHash ) );
            memzero( &Data, sizeof( Data ) );
            TimeEvents.clear();
        }
    };
    typedef vector< ClientSaveData > ClientSaveDataVec;
    static ClientSaveDataVec ClientsSaveData;
    static size_t            ClientsSaveDataCount;

    #define WORLD_SAVE_MAX_INDEX           ( 9999 )
    #define WORLD_SAVE_DATA_BUFFER_SIZE    ( 10000000 )  // 10mb
    static PUCharVec  WorldSaveData;
    static size_t     WorldSaveDataBufCount, WorldSaveDataBufFreeSize;

    static uint       SaveWorldIndex, SaveWorldTime, SaveWorldNextTick;
    static UIntVec    SaveWorldDeleteIndexes;
    static void*      DumpFile;
    static MutexEvent DumpBeginEvent, DumpEndEvent;
    static Thread     DumpThread;

    static bool SaveClient( Client* cl, bool deferred );
    static bool LoadClient( Client* cl );
    static bool NewWorld();
    static void SaveWorld( const char* fname );
    static bool LoadWorld( const char* fname );
    static void UnloadWorld();
    static void AddWorldSaveData( void* data, size_t size );
    static void AddClientSaveData( Client* cl );
    static void Dump_Work( void* data );

    // Access
    static void GetAccesses( StrVec& client, StrVec& tester, StrVec& moder, StrVec& admin, StrVec& admin_names );

    // Banned
    #define BANS_FNAME_ACTIVE              "Active.txt"
    #define BANS_FNAME_EXPIRED             "Expired.txt"
    struct ClientBanned
    {
        DateTimeStamp BeginTime;
        DateTimeStamp EndTime;
        uint          ClientIp;
        char          ClientName[ UTF8_BUF_SIZE( MAX_NAME ) ];
        char          BannedBy[ UTF8_BUF_SIZE( MAX_NAME ) ];
        char          BanInfo[ UTF8_BUF_SIZE( 128 ) ];
        bool operator==( const char* name ) { return Str::CompareCaseUTF8( name, ClientName ); }
        bool operator==( const uint ip )    { return ClientIp == ip; }

        const char*   GetBanLexems() { return Str::FormatBuf( "$banby%s$time%d$reason%s", BannedBy[ 0 ] ? BannedBy : "?", Timer::GetTimeDifference( EndTime, BeginTime ) / 60 / 60, BanInfo[ 0 ] ? BanInfo : "just for fun" ); }
    };
    typedef vector< ClientBanned > ClientBannedVec;
    static ClientBannedVec Banned;
    static Mutex           BannedLocker;

    static ClientBanned* GetBanByName( const char* name )
    {
        auto it = std::find( Banned.begin(), Banned.end(), name );
        return it != Banned.end() ? &( *it ) : NULL;
    }
    static ClientBanned* GetBanByIp( uint ip )
    {
        auto it = std::find( Banned.begin(), Banned.end(), ip );
        return it != Banned.end() ? &( *it ) : NULL;
    }
    static uint GetBanTime( ClientBanned& ban );
    static void ProcessBans();
    static void SaveBan( ClientBanned& ban, bool expired );
    static void SaveBans();
    static void LoadBans();

    // Clients data
    struct ClientData
    {
        char ClientName[ UTF8_BUF_SIZE( MAX_NAME ) ];
        char ClientPassHash[ PASS_HASH_SIZE ];
        uint SaveIndex;
        uint UID[ 5 ];
        uint UIDEndTick;
    };
    typedef map< uint, ClientData* > ClientDataMap;
    static ClientDataMap ClientsData;
    static Mutex         ClientsDataLocker;

    static bool        LoadClientsData();
    static ClientData* GetClientData( uint id );

    // Statistics
    struct Statistics_
    {
        uint  ServerStartTick;
        uint  Uptime;
        int64 BytesSend;
        int64 BytesRecv;
        int64 DataReal;
        int64 DataCompressed;
        float CompressRatio;
        uint  MaxOnline;
        uint  CurOnline;

        uint  CycleTime;
        uint  FPS;
        uint  LoopTime;
        uint  LoopCycles;
        uint  LoopMin;
        uint  LoopMax;
        uint  LagsCount;
    } static Statistics;

    static string GetIngamePlayersStatistics();

    // Singleplayer save
    struct SingleplayerSave_
    {
        bool           Valid;
        ClientSaveData CrData;
        UCharVec       PicData;
    } static SingleplayerSave;

    // Brute force prevention
    static UIntUIntPairMap BruteForceIps;   // Ip -> Time / Fail count
    static StrUIntMap      BruteForceNames; // Name -> Time
    static bool CheckBruteForceIp( uint ip );
    static bool CheckBruteForceName( const char* name );
    static void ClearBruteForceEntire( uint ip, const char* name );

    // Script functions
    struct SScriptFunc
    {
        static ScriptString* ProtoItem_GetScriptName( ProtoItem* proto );

        static void Synchronizer_Constructor( void* memory );
        static void Synchronizer_Destructor( void* memory );

        static AIDataPlane* NpcPlane_GetCopy( AIDataPlane* plane );
        static AIDataPlane* NpcPlane_SetChild( AIDataPlane* plane, AIDataPlane* child_plane );
        static AIDataPlane* NpcPlane_GetChild( AIDataPlane* plane, uint index );
        static bool         NpcPlane_Misc_SetScript( AIDataPlane* plane, ScriptString& func_name );

        static Item* Container_AddItem( Item* cont, hash pid, uint count, uint stack_id );
        static uint  Container_GetItems( Item* cont, uint stack_id, ScriptArray* items );
        static Item* Container_GetItem( Item* cont, hash pid, uint stack_id );

        static bool  Item_IsStackable( Item* item );
        static bool  Item_IsDeteriorable( Item* item );
        static bool  Item_SetScript( Item* item, ScriptString* script );
        static hash  Item_GetScriptId( Item* item );
        static bool  Item_SetEvent( Item* item, int event_type, ScriptString* func_name );
        static hash  Item_get_ProtoId( Item* item );
        static int   Item_get_Type( Item* item );
        static uint  Item_GetCount( Item* item );
        static void  Item_SetCount( Item* item, uint count );
        static uint  Item_GetCost( Item* item );
        static Map*  Item_GetMapPosition( Item* item, ushort& hx, ushort& hy );
        static bool  Item_ChangeProto( Item* item, hash pid );
        static void  Item_Animate( Item* item, uchar from_frm, uchar to_frm );
        static Item* Item_GetChild( Item* item, uint child_index );
        static bool  Item_LockerOpen( Item* item );
        static bool  Item_LockerClose( Item* item );

        static void Item_EventFinish( Item* item, bool deleted );
        static bool Item_EventAttack( Item* item, Critter* attacker, Critter* target );
        static bool Item_EventUse( Item* item, Critter* cr, Critter* on_critter, Item* on_item, MapObject* on_scenery );
        static bool Item_EventUseOnMe( Item* item, Critter* cr, Item* used_item );
        static bool Item_EventSkill( Item* item, Critter* cr, int skill );
        static void Item_EventDrop( Item* item, Critter* cr );
        static void Item_EventMove( Item* item, Critter* cr, uchar from_slot );
        static void Item_EventWalk( Item* item, Critter* cr, bool entered, uchar dir );

        static uint CraftItem_GetShowParams( CraftItem* craft, ScriptArray* nums, ScriptArray* vals, ScriptArray* ors );
        static uint CraftItem_GetNeedParams( CraftItem* craft, ScriptArray* nums, ScriptArray* vals, ScriptArray* ors );
        static uint CraftItem_GetNeedTools( CraftItem* craft, ScriptArray* pids, ScriptArray* vals, ScriptArray* ors );
        static uint CraftItem_GetNeedItems( CraftItem* craft, ScriptArray* pids, ScriptArray* vals, ScriptArray* ors );
        static uint CraftItem_GetOutItems( CraftItem* craft, ScriptArray* pids, ScriptArray* vals );

        static bool Scen_CallSceneryFunction( MapObject* scenery, Critter* cr, int skill, Item* item );

        static bool         Crit_IsPlayer( Critter* cr );
        static bool         Crit_IsNpc( Critter* cr );
        static bool         Crit_IsCanWalk( Critter* cr );
        static bool         Crit_IsCanRun( Critter* cr );
        static bool         Crit_IsCanRotate( Critter* cr );
        static bool         Crit_IsCanAim( Critter* cr );
        static bool         Crit_IsAnim1( Critter* cr, uint index );
        static int          Cl_GetAccess( Critter* cl );
        static bool         Cl_SetAccess( Critter* cl, int access );
        static bool         Crit_SetEvent( Critter* cr, int event_type, ScriptString* func_name );
        static Map*         Crit_GetMap( Critter* cr );
        static uint         Crit_GetMapId( Critter* cr );
        static hash         Crit_GetMapProtoId( Critter* cr );
        static bool         Crit_ChangeCrType( Critter* cr, uint new_type );
        static void         Cl_DropTimers( Critter* cl );
        static bool         Crit_MoveRandom( Critter* cr );
        static bool         Crit_MoveToDir( Critter* cr, uchar direction );
        static bool         Crit_TransitToHex( Critter* cr, ushort hx, ushort hy, uchar dir );
        static bool         Crit_TransitToMapHex( Critter* cr, uint map_id, ushort hx, ushort hy, uchar dir, bool with_group );
        static bool         Crit_TransitToMapEntire( Critter* cr, uint map_id, int entire, bool with_group );
        static bool         Crit_TransitToGlobal( Critter* cr, bool request_group );
        static bool         Crit_TransitToGlobalWithGroup( Critter* cr, ScriptArray& group );
        static bool         Crit_TransitToGlobalGroup( Critter* cr, uint critter_id );
        static bool         Crit_IsLife( Critter* cr );
        static bool         Crit_IsKnockout( Critter* cr );
        static bool         Crit_IsDead( Critter* cr );
        static bool         Crit_IsFree( Critter* cr );
        static bool         Crit_IsBusy( Critter* cr );
        static void         Crit_Wait( Critter* cr, uint ms );
        static void         Crit_ToDead( Critter* cr, uint anim2, Critter* killer );
        static bool         Crit_ToLife( Critter* cr );
        static bool         Crit_ToKnockout( Critter* cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, ushort knock_hx, ushort knock_hy );
        static void         Crit_RefreshVisible( Critter* cr );
        static void         Crit_ViewMap( Critter* cr, Map* map, uint look, ushort hx, ushort hy, uchar dir );
        static void         Crit_AddHolodiskInfo( Critter* cr, uint holodisk_num );
        static void         Crit_EraseHolodiskInfo( Critter* cr, uint holodisk_num );
        static bool         Crit_IsHolodiskInfo( Critter* cr, uint holodisk_num );
        static void         Crit_Say( Critter* cr, uchar how_say, ScriptString& text );
        static void         Crit_SayMsg( Critter* cr, uchar how_say, ushort text_msg, uint num_str );
        static void         Crit_SayMsgLex( Critter* cr, uchar how_say, ushort text_msg, uint num_str, ScriptString& lexems );
        static void         Crit_SetDir( Critter* cr, uchar dir );
        static bool         Crit_PickItem( Critter* cr, ushort hx, ushort hy, hash pid );
        static void         Crit_SetFavoriteItem( Critter* cr, int slot, hash pid );
        static hash         Crit_GetFavoriteItem( Critter* cr, int slot );
        static uint         Crit_GetCritters( Critter* cr, bool look_on_me, int find_type, ScriptArray* critters );
        static uint         Crit_GetFollowGroup( Critter* cr, int find_type, ScriptArray* critters );
        static Critter*     Crit_GetFollowLeader( Critter* cr );
        static ScriptArray* Crit_GetGlobalGroup( Critter* cr );
        static bool         Crit_IsGlobalGroupLeader( Critter* cr );
        static void         Crit_LeaveGlobalGroup( Critter* cr );
        static void         Crit_GiveGlobalGroupLead( Critter* cr, Critter* to_cr );
        static uint         Npc_GetTalkedPlayers( Critter* cr, ScriptArray* players );
        static bool         Crit_IsSeeCr( Critter* cr, Critter* cr_ );
        static bool         Crit_IsSeenByCr( Critter* cr, Critter* cr_ );
        static bool         Crit_IsSeeItem( Critter* cr, Item* item );
        static Item*        Crit_AddItem( Critter* cr, hash pid, uint count );
        static bool         Crit_DeleteItem( Critter* cr, hash pid, uint count );
        static uint         Crit_ItemsCount( Critter* cr );
        static uint         Crit_ItemsWeight( Critter* cr );
        static uint         Crit_ItemsVolume( Critter* cr );
        static uint         Crit_CountItem( Critter* cr, hash proto_id );
        static Item*        Crit_GetItem( Critter* cr, hash proto_id, int slot );
        static Item*        Crit_GetItemById( Critter* cr, uint item_id );
        static uint         Crit_GetItems( Critter* cr, int slot, ScriptArray* items );
        static uint         Crit_GetItemsByType( Critter* cr, int type, ScriptArray* items );
        static ProtoItem*   Crit_GetSlotProto( Critter* cr, int slot, uchar& mode );
        static bool         Crit_MoveItem( Critter* cr, uint item_id, uint count, uchar to_slot );

        static uint         Npc_ErasePlane( Critter* npc, int plane_type, bool all );
        static bool         Npc_ErasePlaneIndex( Critter* npc, uint index );
        static void         Npc_DropPlanes( Critter* npc );
        static bool         Npc_IsNoPlanes( Critter* npc );
        static bool         Npc_IsCurPlane( Critter* npc, int plane_type );
        static AIDataPlane* Npc_GetCurPlane( Critter* npc );
        static uint         Npc_GetPlanes( Critter* npc, ScriptArray* arr );
        static uint         Npc_GetPlanesIdentifier( Critter* npc, int identifier, ScriptArray* arr );
        static uint         Npc_GetPlanesIdentifier2( Critter* npc, int identifier, uint identifier_ext, ScriptArray* arr );
        static bool         Npc_AddPlane( Critter* npc, AIDataPlane& plane );

        static void Crit_SendMessage( Critter* cr, int num, int val, int to );
        static void Crit_SendCombatResult( Critter* cr, ScriptArray& arr );
        static void Crit_Action( Critter* cr, int action, int action_ext, Item* item );
        static void Crit_Animate( Critter* cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play );
        static void Crit_SetAnims( Critter* cr, int cond, uint anim1, uint anim2 );
        static void Crit_PlaySound( Critter* cr, ScriptString& sound_name, bool send_self );
        static void Crit_PlaySoundType( Critter* cr, uchar sound_type, uchar sound_type_ext, uchar sound_id, uchar sound_id_ext, bool send_self );

        static bool Crit_IsKnownLoc( Critter* cr, bool by_id, uint loc_num );
        static bool Crit_SetKnownLoc( Critter* cr, bool by_id, uint loc_num );
        static bool Crit_UnsetKnownLoc( Critter* cr, bool by_id, uint loc_num );
        static void Crit_SetFog( Critter* cr, ushort zone_x, ushort zone_y, int fog );
        static int  Crit_GetFog( Critter* cr, ushort zone_x, ushort zone_y );

        static void Cl_ShowContainer( Critter* cl, Critter* cr_cont, Item* item_cont, uchar transfer_type );
        static void Cl_ShowScreen( Critter* cl, int screen_type, uint param, ScriptString* func_name );
        static void Cl_RunClientScript( Critter* cl, ScriptString& func_name, int p0, int p1, int p2, ScriptString* p3, ScriptArray* p4 );
        static void Cl_Disconnect( Critter* cl );

        static bool Crit_SetScript( Critter* cr, ScriptString* script );
        static hash Crit_get_ProtoId( Critter* cr );
        static uint Crit_GetMultihex( Critter* cr );
        static void Crit_SetMultihex( Critter* cr, int value );

        static void Crit_AddEnemyToStack( Critter* cr, uint critter_id );
        static bool Crit_CheckEnemyInStack( Critter* cr, uint critter_id );
        static void Crit_EraseEnemyFromStack( Critter* cr, uint critter_id );
        static void Crit_ChangeEnemyStackSize( Critter* cr, uint new_size );
        static void Crit_GetEnemyStack( Critter* cr, ScriptArray& arr );
        static void Crit_ClearEnemyStack( Critter* cr );
        static void Crit_ClearEnemyStackNpc( Critter* cr );

        static bool Crit_AddTimeEvent( Critter* cr, ScriptString& func_name, uint duration, int identifier );
        static bool Crit_AddTimeEventRate( Critter* cr, ScriptString& func_name, uint duration, int identifier, uint rate );
        static uint Crit_GetTimeEvents( Critter* cr, int identifier, ScriptArray* indexes, ScriptArray* durations, ScriptArray* rates );
        static uint Crit_GetTimeEventsArr( Critter* cr, ScriptArray& find_identifiers, ScriptArray* identifiers, ScriptArray* indexes, ScriptArray* durations, ScriptArray* rates );
        static void Crit_ChangeTimeEvent( Critter* cr, uint index, uint new_duration, uint new_rate );
        static void Crit_EraseTimeEvent( Critter* cr, uint index );
        static uint Crit_EraseTimeEvents( Critter* cr, int identifier );
        static uint Crit_EraseTimeEventsArr( Critter* cr, ScriptArray& identifiers );

        static void Crit_EventIdle( Critter* cr );
        static void Crit_EventFinish( Critter* cr, bool deleted );
        static void Crit_EventDead( Critter* cr, Critter* killer );
        static void Crit_EventRespawn( Critter* cr );
        static void Crit_EventShowCritter( Critter* cr, Critter* show_cr );
        static void Crit_EventShowCritter1( Critter* cr, Critter* show_cr );
        static void Crit_EventShowCritter2( Critter* cr, Critter* show_cr );
        static void Crit_EventShowCritter3( Critter* cr, Critter* show_cr );
        static void Crit_EventHideCritter( Critter* cr, Critter* hide_cr );
        static void Crit_EventHideCritter1( Critter* cr, Critter* hide_cr );
        static void Crit_EventHideCritter2( Critter* cr, Critter* hide_cr );
        static void Crit_EventHideCritter3( Critter* cr, Critter* hide_cr );
        static void Crit_EventShowItemOnMap( Critter* cr, Item* show_item, bool added, Critter* dropper );
        static void Crit_EventChangeItemOnMap( Critter* cr, Item* item );
        static void Crit_EventHideItemOnMap( Critter* cr, Item* hide_item, bool removed, Critter* picker );
        static bool Crit_EventAttack( Critter* cr, Critter* target );
        static bool Crit_EventAttacked( Critter* cr, Critter* attacker );
        static bool Crit_EventStealing( Critter* cr, Critter* thief, Item* item, uint count );
        static void Crit_EventMessage( Critter* cr, Critter* from_cr, int message, int value );
        static bool Crit_EventUseItem( Critter* cr, Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery );
        static bool Crit_EventUseItemOnMe( Critter* cr, Critter* who_use, Item* item );
        static bool Crit_EventUseSkill( Critter* cr, int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery );
        static bool Crit_EventUseSkillOnMe( Critter* cr, Critter* who_use, int skill );
        static void Crit_EventDropItem( Critter* cr, Item* item );
        static void Crit_EventMoveItem( Critter* cr, Item* item, uchar from_slot );
        static void Crit_EventKnockout( Critter* cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, uint knock_dist );
        static void Crit_EventSmthDead( Critter* cr, Critter* from_cr, Critter* killer );
        static void Crit_EventSmthStealing( Critter* cr, Critter* from_cr, Critter* thief, bool success, Item* item, uint count );
        static void Crit_EventSmthAttack( Critter* cr, Critter* from_cr, Critter* target );
        static void Crit_EventSmthAttacked( Critter* cr, Critter* from_cr, Critter* attacker );
        static void Crit_EventSmthUseItem( Critter* cr, Critter* from_cr, Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery );
        static void Crit_EventSmthUseSkill( Critter* cr, Critter* from_cr, int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery );
        static void Crit_EventSmthDropItem( Critter* cr, Critter* from_cr, Item* item );
        static void Crit_EventSmthMoveItem( Critter* cr, Critter* from_cr, Item* item, uchar from_slot );
        static void Crit_EventSmthKnockout( Critter* cr, Critter* from_cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, uint knock_dist );
        static int  Crit_EventPlaneBegin( Critter* cr, AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item );
        static int  Crit_EventPlaneEnd( Critter* cr, AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item );
        static int  Crit_EventPlaneRun( Critter* cr, AIDataPlane* plane, int reason, uint& p0, uint& p1, uint& p2 );
        static bool Crit_EventBarter( Critter* cr, Critter* cr_barter, bool attach, uint barter_count );
        static bool Crit_EventTalk( Critter* cr, Critter* cr_talk, bool attach, uint talk_count );
        static bool Crit_EventGlobalProcess( Critter* cr, int type, Item* car, float& x, float& y, float& to_x, float& to_y, float& speed, uint& encounter_descriptor, bool& wait_for_answer );
        static bool Crit_EventGlobalInvite( Critter* cr, Item* car, uint encounter_descriptor, int combat_mode, uint& map_id, ushort& hx, ushort& hy, uchar& dir );
        static void Crit_EventTurnBasedProcess( Critter* cr, Map* map, bool begin_turn );
        static void Crit_EventSmthTurnBasedProcess( Critter* cr, Critter* from_cr, Map* map, bool begin_turn );

        static uint       Map_GetId( Map* map );
        static hash       Map_get_ProtoId( Map* map );
        static Location*  Map_GetLocation( Map* map );
        static bool       Map_SetScript( Map* map, ScriptString* script );
        static hash       Map_GetScriptId( Map* map );
        static bool       Map_SetEvent( Map* map, int event_type, ScriptString* func_name );
        static void       Map_SetLoopTime( Map* map, uint loop_num, uint ms );
        static uchar      Map_GetRain( Map* map );
        static void       Map_SetRain( Map* map, uchar capacity );
        static int        Map_GetTime( Map* map );
        static void       Map_SetTime( Map* map, int time );
        static uint       Map_GetDayTime( Map* map, uint day_part );
        static void       Map_SetDayTime( Map* map, uint day_part, uint time );
        static void       Map_GetDayColor( Map* map, uint day_part, uchar& r, uchar& g, uchar& b );
        static void       Map_SetDayColor( Map* map, uint day_part, uchar r, uchar g, uchar b );
        static void       Map_SetTurnBasedAvailability( Map* map, bool value );
        static bool       Map_IsTurnBasedAvailability( Map* map );
        static void       Map_BeginTurnBased( Map* map, Critter* first_turn_crit );
        static bool       Map_IsTurnBased( Map* map );
        static void       Map_EndTurnBased( Map* map );
        static int        Map_GetTurnBasedSequence( Map* map, ScriptArray& critters_ids );
        static void       Map_SetData( Map* map, uint index, int value );
        static int        Map_GetData( Map* map, uint index );
        static Item*      Map_AddItem( Map* map, ushort hx, ushort hy, hash proto_id, uint count );
        static uint       Map_GetItemsHex( Map* map, ushort hx, ushort hy, ScriptArray* items );
        static uint       Map_GetItemsHexEx( Map* map, ushort hx, ushort hy, uint radius, hash pid, ScriptArray* items );
        static uint       Map_GetItemsByPid( Map* map, hash pid, ScriptArray* items );
        static uint       Map_GetItemsByType( Map* map, int type, ScriptArray* items );
        static Item*      Map_GetItem( Map* map, uint item_id );
        static Item*      Map_GetItemHex( Map* map, ushort hx, ushort hy, hash pid );
        static Item*      Map_GetDoor( Map* map, ushort hx, ushort hy );
        static Item*      Map_GetCar( Map* map, ushort hx, ushort hy );
        static MapObject* Map_GetSceneryHex( Map* map, ushort hx, ushort hy, hash pid );
        static uint       Map_GetSceneriesHex( Map* map, ushort hx, ushort hy, ScriptArray* sceneries );
        static uint       Map_GetSceneriesHexEx( Map* map, ushort hx, ushort hy, uint radius, hash pid, ScriptArray* sceneries );
        static uint       Map_GetSceneriesByPid( Map* map, hash pid, ScriptArray* sceneries );
        static Critter*   Map_GetCritterHex( Map* map, ushort hx, ushort hy );
        static Critter*   Map_GetCritterById( Map* map, uint crid );
        static uint       Map_GetCritters( Map* map, ushort hx, ushort hy, uint radius, int find_type, ScriptArray* critters );
        static uint       Map_GetCrittersByPids( Map* map, hash pid, int find_type, ScriptArray* critters );
        static uint       Map_GetCrittersInPath( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type, ScriptArray* critters );
        static uint       Map_GetCrittersInPathBlock( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type, ScriptArray* critters, ushort& pre_block_hx, ushort& pre_block_hy, ushort& block_hx, ushort& block_hy );
        static uint       Map_GetCrittersWhoViewPath( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, int find_type, ScriptArray* critters );
        static uint       Map_GetCrittersSeeing( Map* map, ScriptArray& critters, bool look_on_them, int find_type, ScriptArray* result_critters );
        static void       Map_GetHexInPath( Map* map, ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist );
        static void       Map_GetHexInPathWall( Map* map, ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist );
        static uint       Map_GetPathLengthHex( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut );
        static uint       Map_GetPathLengthCr( Map* map, Critter* cr, ushort to_hx, ushort to_hy, uint cut );
        static Critter*   Map_AddNpc( Map* map, hash proto_id, ushort hx, ushort hy, uchar dir, ScriptArray* params, ScriptArray* items, ScriptString* script );
        static uint       Map_GetNpcCount( Map* map, int npc_role, int find_type );
        static Critter*   Map_GetNpc( Map* map, int npc_role, int find_type, uint skip_count );
        static uint       Map_CountEntire( Map* map, int entire );
        static uint       Map_GetEntires( Map* map, int entire, ScriptArray* entires, ScriptArray* hx, ScriptArray* hy );
        static bool       Map_GetEntireCoords( Map* map, int entire, uint skip, ushort& hx, ushort& hy );
        static bool       Map_GetEntireCoordsDir( Map* map, int entire, uint skip, ushort& hx, ushort& hy, uchar& dir );
        static bool       Map_GetNearEntireCoords( Map* map, int& entire, ushort& hx, ushort& hy );
        static bool       Map_GetNearEntireCoordsDir( Map* map, int& entire, ushort& hx, ushort& hy, uchar& dir );
        static bool       Map_IsHexPassed( Map* map, ushort hex_x, ushort hex_y );
        static bool       Map_IsHexRaked( Map* map, ushort hex_x, ushort hex_y );
        static void       Map_SetText( Map* map, ushort hex_x, ushort hex_y, uint color, ScriptString& text );
        static void       Map_SetTextMsg( Map* map, ushort hex_x, ushort hex_y, uint color, ushort text_msg, uint str_num );
        static void       Map_SetTextMsgLex( Map* map, ushort hex_x, ushort hex_y, uint color, ushort text_msg, uint str_num, ScriptString& lexems );
        static void       Map_RunEffect( Map* map, hash eff_pid, ushort hx, ushort hy, uint radius );
        static void       Map_RunFlyEffect( Map* map, hash eff_pid, Critter* from_cr, Critter* to_cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy );
        static bool       Map_CheckPlaceForItem( Map* map, ushort hx, ushort hy, hash pid );
        static void       Map_BlockHex( Map* map, ushort hx, ushort hy, bool full );
        static void       Map_UnblockHex( Map* map, ushort hx, ushort hy );
        static void       Map_PlaySound( Map* map, ScriptString& sound_name );
        static void       Map_PlaySoundRadius( Map* map, ScriptString& sound_name, ushort hx, ushort hy, uint radius );
        static bool       Map_Reload( Map* map );
        static ushort     Map_GetWidth( Map* map );
        static ushort     Map_GetHeight( Map* map );
        static void       Map_MoveHexByDir( Map* map, ushort& hx, ushort& hy, uchar dir, uint steps );
        static bool       Map_VerifyTrigger( Map* map, Critter* cr, ushort hx, ushort hy, uchar dir );

        static void Map_EventFinish( Map* map, bool deleted );
        static void Map_EventLoop0( Map* map );
        static void Map_EventLoop1( Map* map );
        static void Map_EventLoop2( Map* map );
        static void Map_EventLoop3( Map* map );
        static void Map_EventLoop4( Map* map );
        static void Map_EventInCritter( Map* map, Critter* cr );
        static void Map_EventOutCritter( Map* map, Critter* cr );
        static void Map_EventCritterDead( Map* map, Critter* cr, Critter* killer );
        static void Map_EventTurnBasedBegin( Map* map );
        static void Map_EventTurnBasedEnd( Map* map );
        static void Map_EventTurnBasedProcess( Map* map, Critter* cr, bool begin_turn );

        static hash Location_get_ProtoId( Location* loc );
        static bool Location_SetEvent( Location* loc, int event_type, ScriptString* func_name );
        static uint Location_GetMapCount( Location* loc );
        static Map* Location_GetMap( Location* loc, hash map_pid );
        static Map* Location_GetMapByIndex( Location* loc, uint index );
        static uint Location_GetMaps( Location* loc, ScriptArray* maps );
        static bool Location_GetEntrance( Location* loc, uint entrance, uint& map_index, uint& entire );
        static uint Location_GetEntrances( Location* loc, ScriptArray* maps_index, ScriptArray* entires );
        static bool Location_Reload( Location* loc );
        static void Location_Update( Location* loc );

        static void Location_EventFinish( Location* loc, bool deleted );
        static bool Location_EventEnter( Location* loc, ScriptArray& group, uchar entrance );

        static ProtoItem* Global_GetProtoItem( hash pid );
        static Item*      Global_GetItem( uint item_id );
        static uint       Global_GetCrittersDistantion( Critter* cr1, Critter* cr2 );
        static void       Global_MoveItemCr( Item* item, uint count, Critter* to_cr );
        static void       Global_MoveItemMap( Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy );
        static void       Global_MoveItemCont( Item* item, uint count, Item* to_cont, uint stack_id );
        static void       Global_MoveItemsCr( ScriptArray& items, Critter* to_cr );
        static void       Global_MoveItemsMap( ScriptArray& items, Map* to_map, ushort to_hx, ushort to_hy );
        static void       Global_MoveItemsCont( ScriptArray& items, Item* to_cont, uint stack_id );
        static void       Global_DeleteItem( Item* item );
        static void       Global_DeleteItems( ScriptArray& items );
        static void       Global_DeleteNpc( Critter* npc );
        static void       Global_DeleteNpcForce( Critter* npc );
        static void       Global_RadioMessage( ushort channel, ScriptString& text );
        static void       Global_RadioMessageMsg( ushort channel, ushort text_msg, uint num_str );
        static void       Global_RadioMessageMsgLex( ushort channel, ushort text_msg, uint num_str, ScriptString* lexems );
        static uint       Global_GetFullSecond( ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second );
        static void       Global_GetGameTime( uint full_second, ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour, ushort& minute, ushort& second );
        static uint       Global_CreateLocation( hash loc_pid, ushort wx, ushort wy, ScriptArray* critters );
        static void       Global_DeleteLocation( uint loc_id );
        // static void*          Global_GetProtoCritter( hash proto_id );
        static Critter*      Global_GetCritter( uint crid );
        static CraftItem*    Global_GetCraftItem( uint num );
        static Critter*      Global_GetPlayer( ScriptString& name );
        static uint          Global_GetPlayerId( ScriptString& name );
        static ScriptString* Global_GetPlayerName( uint id );
        static uint          Global_GetGlobalMapCritters( ushort wx, ushort wy, uint radius, int find_type, ScriptArray* critters );
        static void          Global_SetTime( ushort multiplier, ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second );
        static Map*          Global_GetMap( uint map_id );
        static Map*          Global_GetMapByPid( hash map_pid, uint skip_count );
        static Location*     Global_GetLocation( uint loc_id );
        static Location*     Global_GetLocationByPid( hash loc_pid, uint skip_count );
        static uint          Global_GetLocations( ushort wx, ushort wy, uint radius, ScriptArray* locations );
        static uint          Global_GetVisibleLocations( ushort wx, ushort wy, uint radius, Critter* cr, ScriptArray* locations );
        static uint          Global_GetZoneLocationIds( ushort zx, ushort zy, uint zone_radius, ScriptArray* locations );
        static bool          Global_RunDialogNpc( Critter* player, Critter* npc, bool ignore_distance );
        static bool          Global_RunDialogNpcDlgPack( Critter* player, Critter* npc, uint dlg_pack, bool ignore_distance );
        static bool          Global_RunDialogHex( Critter* player, uint dlg_pack, ushort hx, ushort hy, bool ignore_distance );
        static int64         Global_WorldItemCount( hash pid );
        static bool          Global_AddTextListener( int say_type, ScriptString& first_str, uint parameter, ScriptString& script_name );
        static void          Global_EraseTextListener( int say_type, ScriptString& first_str, uint parameter );
        static AIDataPlane*  Global_CreatePlane();
        static uint          Global_GetBagItems( uint bag_id, ScriptArray* pids, ScriptArray* min_counts, ScriptArray* max_counts, ScriptArray* slots );
        static bool          Global_SwapCritters( Critter* cr1, Critter* cr2, bool with_inventory );
        static uint          Global_GetAllItems( hash pid, ScriptArray* items );
        static uint          Global_GetAllPlayers( ScriptArray* players );
        static uint          Global_GetRegisteredPlayers( ScriptArray* ids, ScriptArray* names );
        static uint          Global_GetAllNpc( hash pid, ScriptArray* npc );
        static uint          Global_GetAllMaps( hash pid, ScriptArray* maps );
        static uint          Global_GetAllLocations( hash pid, ScriptArray* locations );
        static hash          Global_GetScriptId( ScriptString& script_name, ScriptString& func_decl );
        static ScriptString* Global_GetScriptName( hash script_id );
        static void          Global_GetTime( ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour, ushort& minute, ushort& second, ushort& milliseconds );
        static bool          Global_SetPropertyGetCallback( int prop_enum_value, ScriptString& script_func );
        static bool          Global_AddPropertySetCallback( int prop_enum_value, ScriptString& script_func );
        static void          Global_AllowSlot( uchar index, bool enable_send );
        static void          Global_AddRegistrationProperty( int cr_prop );
        static bool          Global_LoadDataFile( ScriptString& dat_name );
        static int           Global_GetConstantValue( int const_collection, ScriptString* name );
        static ScriptString* Global_GetConstantName( int const_collection, int value );
        static void          Global_AddConstant( int const_collection, ScriptString* name, int value );
        static bool          Global_LoadConstants( int const_collection, ScriptString* file_name, int path_type );
        // static uint Global_GetVersion();
        static bool          Global_IsCritterCanWalk( uint cr_type );
        static bool          Global_IsCritterCanRun( uint cr_type );
        static bool          Global_IsCritterCanRotate( uint cr_type );
        static bool          Global_IsCritterCanAim( uint cr_type );
        static bool          Global_IsCritterCanArmor( uint cr_type );
        static bool          Global_IsCritterAnim1( uint cr_type, uint index );
        static int           Global_GetCritterAnimType( uint cr_type );
        static uint          Global_GetCritterAlias( uint cr_type );
        static ScriptString* Global_GetCritterTypeName( uint cr_type );
        static ScriptString* Global_GetCritterSoundName( uint cr_type );
        static bool          Global_LoadImage( uint index, ScriptString* image_name, uint image_depth, int path_type );
        static uint          Global_GetImageColor( uint index, uint x, uint y );
        static void          Global_Synchronize();
        static void          Global_Resynchronize();
    } ScriptFunc;
};

#endif // __SERVER__
