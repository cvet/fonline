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
#include "NetProtocol.h"
#include "Access.h"
#include "EntityManager.h"
#include "ProtoManager.h"

#if defined ( USE_LIBEVENT )
# include "event2/event.h"
# include "event2/bufferevent.h"
# include "event2/buffer.h"
# include "event2/thread.h"
#endif

// Check buffer for error
#define CHECK_IN_BUFF_ERROR( client )    CHECK_IN_BUFF_ERROR_EXT( client, 0, return )
#define CHECK_IN_BUFF_ERROR_EXT( client, before_disconnect, after_disconnect )                                  \
    if( client->Bin.IsError() )                                                                                 \
    {                                                                                                           \
        WriteLogF( _FUNC_, " - Wrong network data from client '%s', line %u.\n", client->GetInfo(), __LINE__ ); \
        before_disconnect;                                                                                      \
        client->Disconnect();                                                                                   \
        client->Bin.LockReset();                                                                                \
        after_disconnect;                                                                                       \
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
    static void Process_Text( Client* cl );
    static void Process_Command( BufferManager& buf, void ( * logcb )( const char* ), Client* cl, const char* admin_panel );
    static void Process_Command2( BufferManager& buf, void ( * logcb )( const char* ), Client* cl, const char* admin_panel );
    static void Process_Dialog( Client* cl, bool is_say );
    static void Process_GiveMap( Client* cl );
    static void Process_SetUserHoloStr( Client* cl );
    static void Process_GetUserHoloStr( Client* cl );
    static void Process_Ping( Client* cl );
    static void Process_Property( Client* cl, uint data_size );

    static void Send_MapData( Client* cl, ProtoMap* pmap, bool send_tiles, bool send_scenery );

    // Update files
    struct UpdateFile
    {
        uint   Size;
        uchar* Data;
    };
    typedef vector< UpdateFile > UpdateFileVec;
    static UpdateFileVec UpdateFiles;
    static UCharVec      UpdateFilesList;

    static void GenerateUpdateFiles( bool first_generation = false, StrVec* resource_names = nullptr );

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

    static void      SaveHoloInfoFile( IniParser& data );
    static bool      LoadHoloInfoFile( IniParser& data );
    static HoloInfo* GetHoloInfo( uint id );
    static void      Send_PlayerHoloInfo( Critter* cr, uint holo_num, bool send_text );

    // Actions
    static bool Act_Move( Critter* cr, ushort hx, ushort hy, uint move_params );

    static void KillCritter( Critter* cr, uint anim2, Critter* attacker );
    static void RespawnCritter( Critter* cr );
    static void KnockoutCritter( Critter* cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, ushort knock_hx, ushort knock_hy );
    static bool MoveRandom( Critter* cr );
    static bool RegenerateMap( Map* map );
    static void VerifyTrigger( Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir );

    // Scripting
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

    static void OnSendGlobalValue( Entity* entity, Property* prop );
    static void OnSendCritterValue( Entity* entity, Property* prop );
    static void OnSendMapValue( Entity* entity, Property* prop );
    static void OnSendLocationValue( Entity* entity, Property* prop );

    // Critters
    static void OnSetCritterHandsItemProtoId( Entity* entity, Property* prop, void* cur_value, void* old_value );
    static void OnSetCritterHandsItemMode( Entity* entity, Property* prop, void* cur_value, void* old_value );

    // Items
    static Item* CreateItemOnHex( Map* map, ushort hx, ushort hy, hash pid, uint count, Properties* props, bool check_blocks );
    static void  OnSendItemValue( Entity* entity, Property* prop );
    static void  OnSetItemCount( Entity* entity, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemChangeView( Entity* entity, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemRecacheHex( Entity* entity, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemIsGeck( Entity* entity, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemIsRadio( Entity* entity, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemOpened( Entity* entity, Property* prop, void* cur_value, void* old_value );

    // Npc
    static void ProcessAI( Npc* npc );
    static bool AI_Stay( Npc* npc, uint ms );
    static bool AI_Move( Npc* npc, ushort hx, ushort hy, bool is_run, uint cut, uint trace );
    static bool AI_MoveToCrit( Npc* npc, uint targ_id, uint cut, uint trace, bool is_run );
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
    static void SaveGameInfoFile( IniParser& data );
    static bool LoadGameInfoFile( IniParser& data );
    static void SetGameTime( int multiplier, int year, int month, int day, int hour, int minute, int second );

    // Lang packs
    static LangPackVec LangPacks;     // Todo: synchronize
    static bool InitLangPacks( LangPackVec& lang_packs );
    static bool InitLangPacksDialogs( LangPackVec& lang_packs );
    static bool InitLangPacksLocations( LangPackVec& lang_packs );
    static bool InitLangPacksItems( LangPackVec& lang_packs );
    static void FinishLangPacks();

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
    #define WORLD_SAVE_MAX_INDEX    ( 9999 )

    struct EntityDump
    {
        bool        IsClient;
        string      TypeName;
        Properties* Props;
        Properties* ProtoProps;
        StrMap      ExtraData;
    };
    typedef vector< EntityDump* > EntityDumpVec;

    static uint          SaveWorldIndex, SaveWorldTime, SaveWorldNextTick;
    static UIntVec       SaveWorldDeleteIndexes;
    static EntityDumpVec DumpedEntities;
    static Thread        DumpThread;

    static bool SaveClient( Client* cl );
    static bool LoadClient( Client* cl );
    static bool NewWorld();
    static void SaveWorld( const char* fname );
    static bool LoadWorld( const char* fname );
    static void UnloadWorld();
    static void DumpEntity( Entity* entity );
    static void Dump_Work( void* data );

    // Access
    static void GetAccesses( StrVec& client, StrVec& tester, StrVec& moder, StrVec& admin, StrVec& admin_names );

    // Banned
    #define BANS_FNAME_ACTIVE       "Active.txt"
    #define BANS_FNAME_EXPIRED      "Expired.txt"
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

    static ClientBanned* GetBanByName( const char* name );
    static ClientBanned* GetBanByIp( uint ip );
    static uint          GetBanTime( ClientBanned& ban );
    static void          ProcessBans();
    static void          SaveBan( ClientBanned& ban, bool expired );
    static void          SaveBans();
    static void          LoadBans();

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
        bool        Valid;
        string      Name;
        Properties* CrProps;
        UCharVec    PicData;
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
        static void Synchronizer_Constructor( void* memory );
        static void Synchronizer_Destructor( void* memory );

        static AIDataPlane* NpcPlane_GetCopy( AIDataPlane* plane );
        static AIDataPlane* NpcPlane_SetChild( AIDataPlane* plane, AIDataPlane* child_plane );
        static AIDataPlane* NpcPlane_GetChild( AIDataPlane* plane, uint index );
        static bool         NpcPlane_Misc_SetScript( AIDataPlane* plane, asIScriptFunction* func );

        static Item* Item_AddItem( Item* cont, hash pid, uint count, uint stack_id );
        static uint  Item_GetItems( Item* cont, uint stack_id, ScriptArray* items );
        static bool  Item_SetScript( Item* item, asIScriptFunction* func );
        static Map*  Item_GetMapPosition( Item* item, ushort& hx, ushort& hy );
        static bool  Item_ChangeProto( Item* item, hash pid );
        static void  Item_Animate( Item* item, uchar from_frm, uchar to_frm );
        static Item* Item_GetChild( Item* item, uint child_index );
        static bool  Item_CallSceneryFunction( Item* scenery, Critter* cr, Item* item, int param );

        static bool  Crit_IsPlayer( Critter* cr );
        static bool  Crit_IsNpc( Critter* cr );
        static int   Cl_GetAccess( Critter* cl );
        static bool  Cl_SetAccess( Critter* cl, int access );
        static Map*  Crit_GetMap( Critter* cr );
        static bool  Crit_MoveRandom( Critter* cr );
        static bool  Crit_MoveToDir( Critter* cr, uchar direction );
        static bool  Crit_TransitToHex( Critter* cr, ushort hx, ushort hy, uchar dir );
        static bool  Crit_TransitToMapHex( Critter* cr, uint map_id, ushort hx, ushort hy, uchar dir );
        static bool  Crit_TransitToMapEntire( Critter* cr, uint map_id, int entire );
        static bool  Crit_TransitToGlobal( Critter* cr );
        static bool  Crit_TransitToGlobalWithGroup( Critter* cr, ScriptArray& group );
        static bool  Crit_TransitToGlobalGroup( Critter* cr, uint critter_id );
        static bool  Crit_IsLife( Critter* cr );
        static bool  Crit_IsKnockout( Critter* cr );
        static bool  Crit_IsDead( Critter* cr );
        static bool  Crit_IsFree( Critter* cr );
        static bool  Crit_IsBusy( Critter* cr );
        static void  Crit_Wait( Critter* cr, uint ms );
        static void  Crit_ToDead( Critter* cr, uint anim2, Critter* killer );
        static bool  Crit_ToLife( Critter* cr );
        static bool  Crit_ToKnockout( Critter* cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, ushort knock_hx, ushort knock_hy );
        static void  Crit_RefreshVisible( Critter* cr );
        static void  Crit_ViewMap( Critter* cr, Map* map, uint look, ushort hx, ushort hy, uchar dir );
        static void  Crit_Say( Critter* cr, uchar how_say, ScriptString& text );
        static void  Crit_SayMsg( Critter* cr, uchar how_say, ushort text_msg, uint num_str );
        static void  Crit_SayMsgLex( Critter* cr, uchar how_say, ushort text_msg, uint num_str, ScriptString& lexems );
        static void  Crit_SetDir( Critter* cr, uchar dir );
        static uint  Crit_GetCritters( Critter* cr, bool look_on_me, int find_type, ScriptArray* critters );
        static uint  Npc_GetTalkedPlayers( Critter* cr, ScriptArray* players );
        static bool  Crit_IsSeeCr( Critter* cr, Critter* cr_ );
        static bool  Crit_IsSeenByCr( Critter* cr, Critter* cr_ );
        static bool  Crit_IsSeeItem( Critter* cr, Item* item );
        static Item* Crit_AddItem( Critter* cr, hash pid, uint count );
        static bool  Crit_DeleteItem( Critter* cr, hash pid, uint count );
        static uint  Crit_CountItem( Critter* cr, hash proto_id );
        static Item* Crit_GetItem( Critter* cr, hash proto_id, int slot );
        static Item* Crit_GetItemById( Critter* cr, uint item_id );
        static uint  Crit_GetItems( Critter* cr, int slot, ScriptArray* items );
        static uint  Crit_GetItemsByType( Critter* cr, int type, ScriptArray* items );
        static Item* Crit_GetSlotItem( Critter* cr, int slot );
        static bool  Crit_MoveItem( Critter* cr, uint item_id, uint count, uchar to_slot );

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
        static void         Npc_NextPlane( Critter* npc, int reason );

        static void Crit_SendMessage( Critter* cr, int num, int val, int to );
        static void Crit_SendCombatResult( Critter* cr, ScriptArray& arr );
        static void Crit_Action( Critter* cr, int action, int action_ext, Item* item );
        static void Crit_Animate( Critter* cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play );
        static void Crit_SetAnims( Critter* cr, int cond, uint anim1, uint anim2 );
        static void Crit_PlaySound( Critter* cr, ScriptString& sound_name, bool send_self );

        static bool Crit_IsKnownLoc( Critter* cr, bool by_id, uint loc_num );
        static bool Crit_SetKnownLoc( Critter* cr, bool by_id, uint loc_num );
        static bool Crit_UnsetKnownLoc( Critter* cr, bool by_id, uint loc_num );
        static void Crit_SetFog( Critter* cr, ushort zone_x, ushort zone_y, int fog );
        static int  Crit_GetFog( Critter* cr, ushort zone_x, ushort zone_y );

        static void Cl_SendItems( Critter* cl, ScriptArray* items, int param );
        static void Cl_Disconnect( Critter* cl );

        static bool Crit_SetScript( Critter* cr, asIScriptFunction* func );

        static void Crit_AddEnemyToStack( Critter* cr, uint critter_id );
        static bool Crit_CheckEnemyInStack( Critter* cr, uint critter_id );
        static void Crit_EraseEnemyFromStack( Critter* cr, uint critter_id );
        static void Crit_ClearEnemyStack( Critter* cr );
        static void Crit_ClearEnemyStackNpc( Critter* cr );

        static bool Crit_AddTimeEvent( Critter* cr, asIScriptFunction* func, uint duration, int identifier );
        static bool Crit_AddTimeEventRate( Critter* cr, asIScriptFunction* func, uint duration, int identifier, uint rate );
        static uint Crit_GetTimeEvents( Critter* cr, int identifier, ScriptArray* indexes, ScriptArray* durations, ScriptArray* rates );
        static uint Crit_GetTimeEventsArr( Critter* cr, ScriptArray& find_identifiers, ScriptArray* identifiers, ScriptArray* indexes, ScriptArray* durations, ScriptArray* rates );
        static void Crit_ChangeTimeEvent( Critter* cr, uint index, uint new_duration, uint new_rate );
        static void Crit_EraseTimeEvent( Critter* cr, uint index );
        static uint Crit_EraseTimeEvents( Critter* cr, int identifier );
        static uint Crit_EraseTimeEventsArr( Critter* cr, ScriptArray& identifiers );

        static Location* Map_GetLocation( Map* map );
        static bool      Map_SetScript( Map* map, asIScriptFunction* func );
        static Item*     Map_AddItem( Map* map, ushort hx, ushort hy, hash proto_id, uint count, ScriptDict* props );
        static uint      Map_GetItemsHex( Map* map, ushort hx, ushort hy, ScriptArray* items );
        static uint      Map_GetItemsHexEx( Map* map, ushort hx, ushort hy, uint radius, hash pid, ScriptArray* items );
        static uint      Map_GetItemsByPid( Map* map, hash pid, ScriptArray* items );
        static uint      Map_GetItemsByType( Map* map, int type, ScriptArray* items );
        static Item*     Map_GetItem( Map* map, uint item_id );
        static Item*     Map_GetItemHex( Map* map, ushort hx, ushort hy, hash pid );
        static Item*     Map_GetDoor( Map* map, ushort hx, ushort hy );
        static Item*     Map_GetCar( Map* map, ushort hx, ushort hy );
        static Item*     Map_GetSceneryHex( Map* map, ushort hx, ushort hy, hash pid );
        static uint      Map_GetSceneriesHex( Map* map, ushort hx, ushort hy, ScriptArray* sceneries );
        static uint      Map_GetSceneriesHexEx( Map* map, ushort hx, ushort hy, uint radius, hash pid, ScriptArray* sceneries );
        static uint      Map_GetSceneriesByPid( Map* map, hash pid, ScriptArray* sceneries );
        static Critter*  Map_GetCritterHex( Map* map, ushort hx, ushort hy );
        static Critter*  Map_GetCritterById( Map* map, uint crid );
        static uint      Map_GetCritters( Map* map, ushort hx, ushort hy, uint radius, int find_type, ScriptArray* critters );
        static uint      Map_GetCrittersByPids( Map* map, hash pid, int find_type, ScriptArray* critters );
        static uint      Map_GetCrittersInPath( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type, ScriptArray* critters );
        static uint      Map_GetCrittersInPathBlock( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type, ScriptArray* critters, ushort& pre_block_hx, ushort& pre_block_hy, ushort& block_hx, ushort& block_hy );
        static uint      Map_GetCrittersWhoViewPath( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, int find_type, ScriptArray* critters );
        static uint      Map_GetCrittersSeeing( Map* map, ScriptArray& critters, bool look_on_them, int find_type, ScriptArray* result_critters );
        static void      Map_GetHexInPath( Map* map, ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist );
        static void      Map_GetHexInPathWall( Map* map, ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist );
        static uint      Map_GetPathLengthHex( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut );
        static uint      Map_GetPathLengthCr( Map* map, Critter* cr, ushort to_hx, ushort to_hy, uint cut );
        static Critter*  Map_AddNpc( Map* map, hash proto_id, ushort hx, ushort hy, uchar dir, ScriptDict* props );
        static uint      Map_GetNpcCount( Map* map, int npc_role, int find_type );
        static Critter*  Map_GetNpc( Map* map, int npc_role, int find_type, uint skip_count );
        static uint      Map_CountEntire( Map* map, int entire );
        static uint      Map_GetEntires( Map* map, int entire, ScriptArray* entires, ScriptArray* hx, ScriptArray* hy );
        static bool      Map_GetEntireCoords( Map* map, int entire, uint skip, ushort& hx, ushort& hy );
        static bool      Map_GetEntireCoordsDir( Map* map, int entire, uint skip, ushort& hx, ushort& hy, uchar& dir );
        static bool      Map_GetNearEntireCoords( Map* map, int& entire, ushort& hx, ushort& hy );
        static bool      Map_GetNearEntireCoordsDir( Map* map, int& entire, ushort& hx, ushort& hy, uchar& dir );
        static bool      Map_IsHexPassed( Map* map, ushort hex_x, ushort hex_y );
        static bool      Map_IsHexRaked( Map* map, ushort hex_x, ushort hex_y );
        static void      Map_SetText( Map* map, ushort hex_x, ushort hex_y, uint color, ScriptString& text );
        static void      Map_SetTextMsg( Map* map, ushort hex_x, ushort hex_y, uint color, ushort text_msg, uint str_num );
        static void      Map_SetTextMsgLex( Map* map, ushort hex_x, ushort hex_y, uint color, ushort text_msg, uint str_num, ScriptString& lexems );
        static void      Map_RunEffect( Map* map, hash eff_pid, ushort hx, ushort hy, uint radius );
        static void      Map_RunFlyEffect( Map* map, hash eff_pid, Critter* from_cr, Critter* to_cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy );
        static bool      Map_CheckPlaceForItem( Map* map, ushort hx, ushort hy, hash pid );
        static void      Map_BlockHex( Map* map, ushort hx, ushort hy, bool full );
        static void      Map_UnblockHex( Map* map, ushort hx, ushort hy );
        static void      Map_PlaySound( Map* map, ScriptString& sound_name );
        static void      Map_PlaySoundRadius( Map* map, ScriptString& sound_name, ushort hx, ushort hy, uint radius );
        static bool      Map_Reload( Map* map );
        static void      Map_MoveHexByDir( Map* map, ushort& hx, ushort& hy, uchar dir, uint steps );
        static void      Map_VerifyTrigger( Map* map, Critter* cr, ushort hx, ushort hy, uchar dir );

        static uint Location_GetMapCount( Location* loc );
        static Map* Location_GetMap( Location* loc, hash map_pid );
        static Map* Location_GetMapByIndex( Location* loc, uint index );
        static uint Location_GetMaps( Location* loc, ScriptArray* maps );
        static bool Location_GetEntrance( Location* loc, uint entrance, uint& map_index, hash& entire );
        static uint Location_GetEntrances( Location* loc, ScriptArray* maps_index, ScriptArray* entires );
        static bool Location_Reload( Location* loc );

        static Item*         Global_GetItem( uint item_id );
        static uint          Global_GetCrittersDistantion( Critter* cr1, Critter* cr2 );
        static void          Global_MoveItemCr( Item* item, uint count, Critter* to_cr, bool skip_checks );
        static void          Global_MoveItemMap( Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy, bool skip_checks );
        static void          Global_MoveItemCont( Item* item, uint count, Item* to_cont, uint stack_id, bool skip_checks );
        static void          Global_MoveItemsCr( ScriptArray& items, Critter* to_cr, bool skip_checks );
        static void          Global_MoveItemsMap( ScriptArray& items, Map* to_map, ushort to_hx, ushort to_hy, bool skip_checks );
        static void          Global_MoveItemsCont( ScriptArray& items, Item* to_cont, uint stack_id, bool skip_checks );
        static void          Global_DeleteItem( Item* item );
        static void          Global_DeleteItemById( uint item_id );
        static void          Global_DeleteItems( ScriptArray& items );
        static void          Global_DeleteItemsById( ScriptArray& item_ids );
        static void          Global_DeleteNpc( Critter* npc );
        static void          Global_DeleteNpcById( uint npc_id );
        static void          Global_RadioMessage( ushort channel, ScriptString& text );
        static void          Global_RadioMessageMsg( ushort channel, ushort text_msg, uint num_str );
        static void          Global_RadioMessageMsgLex( ushort channel, ushort text_msg, uint num_str, ScriptString* lexems );
        static uint          Global_GetFullSecond( ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second );
        static void          Global_GetGameTime( uint full_second, ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour, ushort& minute, ushort& second );
        static uint          Global_CreateLocation( hash loc_pid, ushort wx, ushort wy, ScriptArray* critters );
        static void          Global_DeleteLocation( Location* loc );
        static void          Global_DeleteLocationById( uint loc_id );
        static Critter*      Global_GetCritter( uint crid );
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
        static bool          Global_AddTextListener( int say_type, ScriptString& first_str, uint parameter, asIScriptFunction* func );
        static void          Global_EraseTextListener( int say_type, ScriptString& first_str, uint parameter );
        static AIDataPlane*  Global_CreatePlane();
        static bool          Global_SwapCritters( Critter* cr1, Critter* cr2, bool with_inventory );
        static uint          Global_GetAllItems( hash pid, ScriptArray* items );
        static uint          Global_GetAllPlayers( ScriptArray* players );
        static uint          Global_GetRegisteredPlayers( ScriptArray* ids, ScriptArray* names );
        static uint          Global_GetAllNpc( hash pid, ScriptArray* npc );
        static uint          Global_GetAllMaps( hash pid, ScriptArray* maps );
        static uint          Global_GetAllLocations( hash pid, ScriptArray* locations );
        static void          Global_GetTime( ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour, ushort& minute, ushort& second, ushort& milliseconds );
        static bool          Global_SetPropertyGetCallback( int prop_enum_value, void* ref, int type_id );
        static bool          Global_AddPropertySetCallback( int prop_enum_value, void* ref, int type_id, bool deferred );
        static void          Global_AllowSlot( uchar index, bool enable_send );
        static void          Global_AddRegistrationProperty( int cr_prop );
        static bool          Global_LoadDataFile( ScriptString& dat_name );
        // static uint Global_GetVersion();
        static bool Global_LoadImage( uint index, ScriptString* image_name, uint image_depth );
        static uint Global_GetImageColor( uint index, uint x, uint y );
        static void Global_Synchronize();
        static void Global_Resynchronize();
        static void Global_YieldWebRequest( ScriptString& url, ScriptDict* post, bool& success, ScriptString& result );
    } ScriptFunc;
};

#endif // __SERVER__
