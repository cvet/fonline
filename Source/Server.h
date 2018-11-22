#ifndef __SERVER__
#define __SERVER__

#include "Common.h"
#include "NetProtocol.h"
#include "Networking.h"
#include "Script.h"
#include "Item.h"
#include "Critter.h"
#include "Map.h"
#include "MapManager.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "Dialogs.h"
#include "Access.h"
#include "EntityManager.h"
#include "ProtoManager.h"
#include "DataBase.h"

// Check buffer for error
#define CHECK_IN_BUFF_ERROR( client )    CHECK_IN_BUFF_ERROR_EXT( client, 0, return )
#define CHECK_IN_BUFF_ERROR_EXT( client, before_disconnect, after_disconnect )                      \
    if( client->Connection->Bin.IsError() )                                                         \
    {                                                                                               \
        WriteLog( "Wrong network data from client '{}', line {}.\n", client->GetName(), __LINE__ ); \
        before_disconnect;                                                                          \
        client->Disconnect();                                                                       \
        after_disconnect;                                                                           \
    }

class FOServer
{
public:
    FOServer();
    ~FOServer();

    static void EntitySetValue( Entity* entity, Property* prop, void* cur_value, void* old_value );

    // Net process
    static void Process_ParseToGame( Client* cl );
    static void Process_Move( Client* cl );
    static void Process_Update( Client* cl );
    static void Process_UpdateFile( Client* cl );
    static void Process_UpdateFileData( Client* cl );
    static void Process_CreateClient( Client* cl );
    static void Process_LogIn( Client*& cl );
    static void Process_Dir( Client* cl );
    static void Process_Text( Client* cl );
    static void Process_Command( BufferManager& buf, LogFunc logcb, Client* cl, const string& admin_panel );
    static void Process_CommandReal( BufferManager& buf, LogFunc logcb, Client* cl, const string& admin_panel );
    static void Process_Dialog( Client* cl );
    static void Process_GiveMap( Client* cl );
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

    // Actions
    static bool Act_Move( Critter* cr, ushort hx, ushort hy, uint move_params );

    static bool RegenerateMap( Map* map );
    static void VerifyTrigger( Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir );

    // Scripting
    static Pragmas ServerPropertyPragmas;

    // Init/Finish system
    static bool InitScriptSystem();
    static void FinishScriptSystem();

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
        string FirstStr;
        uint64 Parameter;
    };
    typedef vector< TextListen > TextListenVec;
    static TextListenVec TextListeners;
    static Mutex         TextListenersLocker;

    static void OnSendGlobalValue( Entity* entity, Property* prop );
    static void OnSendCritterValue( Entity* entity, Property* prop );
    static void OnSendMapValue( Entity* entity, Property* prop );
    static void OnSendLocationValue( Entity* entity, Property* prop );

    // Items
    static Item* CreateItemOnHex( Map* map, ushort hx, ushort hy, hash pid, uint count, Properties* props, bool check_blocks );
    static void  OnSendItemValue( Entity* entity, Property* prop );
    static void  OnSetItemCount( Entity* entity, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemChangeView( Entity* entity, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemRecacheHex( Entity* entity, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemBlockLines( Entity* entity, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemIsGeck( Entity* entity, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemIsRadio( Entity* entity, Property* prop, void* cur_value, void* old_value );
    static void  OnSetItemOpened( Entity* entity, Property* prop, void* cur_value, void* old_value );

    // Npc
    static void ProcessCritter( Critter* cr );
    static bool Dialog_Compile( Npc* npc, Client* cl, const Dialog& base_dlg, Dialog& compiled_dlg );
    static bool Dialog_CheckDemand( Npc* npc, Client* cl, DialogAnswer& answer, bool recheck );
    static uint Dialog_UseResult( Npc* npc, Client* cl, DialogAnswer& answer );
    static void Dialog_Begin( Client* cl, Npc* npc, hash dlg_pack_id, ushort hx, ushort hy, bool ignore_distance );

    // Main
    static int     UpdateIndex, UpdateLastIndex;
    static uint    UpdateLastTick;
    static bool    Active, ActiveInProcess, ActiveOnce;
    static UIntMap RegIp;
    static Mutex   RegIpLocker;

    static void DisconnectClient( Client* cl );
    static void RemoveClient( Client* cl );
    static void Process( Client* cl );
    static void ProcessMove( Critter* cr );

    // Log to client
    static ClVec LogClients;
    static void LogToClients( const string& str );

    // Game time
    static void SetGameTime( int multiplier, int year, int month, int day, int hour, int minute, int second );

    // Lang packs
    static LangPackVec LangPacks;     // Todo: synchronize
    static bool InitLangPacks( LangPackVec& lang_packs );
    static bool InitLangPacksDialogs( LangPackVec& lang_packs );
    static bool InitLangPacksLocations( LangPackVec& lang_packs );
    static bool InitLangPacksItems( LangPackVec& lang_packs );

    // Init/Finish
    static bool Init();
    static bool InitReal();
    static void Finish();
    static bool Starting() { return Active && ActiveInProcess; }
    static bool Started()  { return Active && !ActiveInProcess; }
    static bool Stopping() { return !Active && ActiveInProcess; }
    static bool Stopped()  { return !Active && !ActiveInProcess; }
    static void MainLoop();

    static void LogicTick();

    // Net
    static NetServerBase* TcpServer;
    static NetServerBase* WebSocketsServer;
    static ClVec          ConnectedClients;
    static Mutex          ConnectedClientsLocker;

    static void OnNewConnection( NetConnection* connection );

    // Access
    static void GetAccesses( StrVec& client, StrVec& tester, StrVec& moder, StrVec& admin, StrVec& admin_names );

    // Banned
    #define BANS_FNAME_ACTIVE                "Save/Bans/Active.txt"
    #define BANS_FNAME_EXPIRED               "Save/Bans/Expired.txt"
    struct ClientBanned
    {
        DateTimeStamp BeginTime;
        DateTimeStamp EndTime;
        uint          ClientIp;
        char          ClientName[ UTF8_BUF_SIZE( MAX_NAME ) ];
        char          BannedBy[ UTF8_BUF_SIZE( MAX_NAME ) ];
        char          BanInfo[ UTF8_BUF_SIZE( 128 ) ];
        bool operator==( const string& name ) { return _str( name ).compareIgnoreCaseUtf8( ClientName ); }
        bool operator==( const uint ip )      { return ClientIp == ip; }

        string        GetBanLexems()
        {
            return _str( "$banby{}$time{}$reason{}", BannedBy[ 0 ] ? BannedBy : "?",
                         Timer::GetTimeDifference( EndTime, BeginTime ) / 60 / 60, BanInfo[ 0 ] ? BanInfo : "just for fun" );
        }
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

    // Script functions
    struct SScriptFunc
    {
        static Item*         Item_AddItem( Item* cont, hash pid, uint count, uint stack_id );
        static CScriptArray* Item_GetItems( Item* cont, uint stack_id );
        static bool          Item_SetScript( Item* item, asIScriptFunction* func );
        static Map*          Item_GetMapPosition( Item* item, ushort& hx, ushort& hy );
        static bool          Item_ChangeProto( Item* item, hash pid );
        static void          Item_Animate( Item* item, uchar from_frm, uchar to_frm );
        static bool          Item_CallStaticItemFunction( Item* static_item, Critter* cr, Item* item, int param );

        static bool          Crit_IsPlayer( Critter* cr );
        static bool          Crit_IsNpc( Critter* cr );
        static int           Cl_GetAccess( Critter* cl );
        static bool          Cl_SetAccess( Critter* cl, int access );
        static Map*          Crit_GetMap( Critter* cr );
        static bool          Crit_MoveToDir( Critter* cr, uchar direction );
        static void          Crit_TransitToHex( Critter* cr, ushort hx, ushort hy, uchar dir );
        static void          Crit_TransitToMapHex( Critter* cr, Map* map, ushort hx, ushort hy, uchar dir );
        static void          Crit_TransitToGlobal( Critter* cr );
        static void          Crit_TransitToGlobalWithGroup( Critter* cr, CScriptArray* group );
        static void          Crit_TransitToGlobalGroup( Critter* cr, Critter* leader );
        static bool          Crit_IsLife( Critter* cr );
        static bool          Crit_IsKnockout( Critter* cr );
        static bool          Crit_IsDead( Critter* cr );
        static bool          Crit_IsFree( Critter* cr );
        static bool          Crit_IsBusy( Critter* cr );
        static void          Crit_Wait( Critter* cr, uint ms );
        static void          Crit_RefreshVisible( Critter* cr );
        static void          Crit_ViewMap( Critter* cr, Map* map, uint look, ushort hx, ushort hy, uchar dir );
        static void          Crit_Say( Critter* cr, uchar how_say, string text );
        static void          Crit_SayMsg( Critter* cr, uchar how_say, ushort text_msg, uint num_str );
        static void          Crit_SayMsgLex( Critter* cr, uchar how_say, ushort text_msg, uint num_str, string lexems );
        static void          Crit_SetDir( Critter* cr, uchar dir );
        static CScriptArray* Crit_GetCritters( Critter* cr, bool look_on_me, int find_type );
        static CScriptArray* Npc_GetTalkedPlayers( Critter* cr );
        static bool          Crit_IsSeeCr( Critter* cr, Critter* cr_ );
        static bool          Crit_IsSeenByCr( Critter* cr, Critter* cr_ );
        static bool          Crit_IsSeeItem( Critter* cr, Item* item );
        static uint          Crit_CountItem( Critter* cr, hash proto_id );
        static bool          Crit_DeleteItem( Critter* cr, hash pid, uint count );
        static Item*         Crit_AddItem( Critter* cr, hash pid, uint count );
        static Item*         Crit_GetItem( Critter* cr, uint item_id );
        static Item*         Crit_GetItemPredicate( Critter* cr, asIScriptFunction* predicate );
        static Item*         Crit_GetItemBySlot( Critter* cr, uchar slot );
        static Item*         Crit_GetItemByPid( Critter* cr, hash proto_id );
        static CScriptArray* Crit_GetItems( Critter* cr );
        static CScriptArray* Crit_GetItemsBySlot( Critter* cr, int slot );
        static CScriptArray* Crit_GetItemsPredicate( Critter* cr, asIScriptFunction* predicate );
        static void          Crit_ChangeItemSlot( Critter* cr, uint item_id, uchar slot );
        static void          Crit_SetCond( Critter* cr, int cond );
        static void          Crit_CloseDialog( Critter* cr );

        static void Crit_SendCombatResult( Critter* cr, CScriptArray* arr );
        static void Crit_Action( Critter* cr, int action, int action_ext, Item* item );
        static void Crit_Animate( Critter* cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play );
        static void Crit_SetAnims( Critter* cr, int cond, uint anim1, uint anim2 );
        static void Crit_PlaySound( Critter* cr, string sound_name, bool send_self );

        static bool Crit_IsKnownLoc( Critter* cr, bool by_id, uint loc_num );
        static bool Crit_SetKnownLoc( Critter* cr, bool by_id, uint loc_num );
        static bool Crit_UnsetKnownLoc( Critter* cr, bool by_id, uint loc_num );
        static void Crit_SetFog( Critter* cr, ushort zone_x, ushort zone_y, int fog );
        static int  Crit_GetFog( Critter* cr, ushort zone_x, ushort zone_y );

        static void Cl_SendItems( Critter* cl, CScriptArray* items, int param );
        static void Cl_Disconnect( Critter* cl );
        static bool Cl_IsOnline( Critter* cl );

        static bool Crit_SetScript( Critter* cr, asIScriptFunction* func );

        static bool Crit_AddTimeEvent( Critter* cr, asIScriptFunction* func, uint duration, int identifier );
        static bool Crit_AddTimeEventRate( Critter* cr, asIScriptFunction* func, uint duration, int identifier, uint rate );
        static uint Crit_GetTimeEvents( Critter* cr, int identifier, CScriptArray* indexes, CScriptArray* durations, CScriptArray* rates );
        static uint Crit_GetTimeEventsArr( Critter* cr, CScriptArray* find_identifiers, CScriptArray* identifiers, CScriptArray* indexes, CScriptArray* durations, CScriptArray* rates );
        static void Crit_ChangeTimeEvent( Critter* cr, uint index, uint new_duration, uint new_rate );
        static void Crit_EraseTimeEvent( Critter* cr, uint index );
        static uint Crit_EraseTimeEvents( Critter* cr, int identifier );
        static uint Crit_EraseTimeEventsArr( Critter* cr, CScriptArray* identifiers );

        static void Crit_MoveToCritter( Critter* cr, Critter* target, uint cut, bool is_run );
        static void Crit_MoveToHex( Critter* cr, ushort hx, ushort hy, uint cut, bool is_run );
        static int  Crit_GetMovingState( Critter* cr );
        static void Crit_ResetMovingState( Critter* cr, uint& gag_id );

        static Location*     Map_GetLocation( Map* map );
        static bool          Map_SetScript( Map* map, asIScriptFunction* func );
        static Item*         Map_AddItem( Map* map, ushort hx, ushort hy, hash proto_id, uint count, CScriptDict* props );
        static CScriptArray* Map_GetItems( Map* map );
        static CScriptArray* Map_GetItemsHex( Map* map, ushort hx, ushort hy );
        static CScriptArray* Map_GetItemsHexEx( Map* map, ushort hx, ushort hy, uint radius, hash pid );
        static CScriptArray* Map_GetItemsByPid( Map* map, hash pid );
        static CScriptArray* Map_GetItemsPredicate( Map* map, asIScriptFunction* predicate );
        static CScriptArray* Map_GetItemsHexPredicate( Map* map, ushort hx, ushort hy, asIScriptFunction* predicate );
        static CScriptArray* Map_GetItemsHexRadiusPredicate( Map* map, ushort hx, ushort hy, uint radius, asIScriptFunction* predicate );
        static Item*         Map_GetItem( Map* map, uint item_id );
        static Item*         Map_GetItemHex( Map* map, ushort hx, ushort hy, hash pid );
        static Item*         Map_GetStaticItem( Map* map, ushort hx, ushort hy, hash pid );
        static CScriptArray* Map_GetStaticItemsHex( Map* map, ushort hx, ushort hy );
        static CScriptArray* Map_GetStaticItemsHexEx( Map* map, ushort hx, ushort hy, uint radius, hash pid );
        static CScriptArray* Map_GetStaticItemsByPid( Map* map, hash pid );
        static CScriptArray* Map_GetStaticItemsPredicate( Map* map, asIScriptFunction* predicate );
        static CScriptArray* Map_GetStaticItemsAll( Map* map );
        static Critter*      Map_GetCritterHex( Map* map, ushort hx, ushort hy );
        static Critter*      Map_GetCritterById( Map* map, uint crid );
        static CScriptArray* Map_GetCritters( Map* map, ushort hx, ushort hy, uint radius, int find_type );
        static CScriptArray* Map_GetCrittersByPids( Map* map, hash pid, int find_type );
        static CScriptArray* Map_GetCrittersInPath( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type );
        static CScriptArray* Map_GetCrittersInPathBlock( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type, ushort& pre_block_hx, ushort& pre_block_hy, ushort& block_hx, ushort& block_hy );
        static CScriptArray* Map_GetCrittersWhoViewPath( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, int find_type );
        static CScriptArray* Map_GetCrittersSeeing( Map* map, CScriptArray* critters, bool look_on_them, int find_type );
        static void          Map_GetHexInPath( Map* map, ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist );
        static void          Map_GetHexInPathWall( Map* map, ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist );
        static uint          Map_GetPathLengthHex( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut );
        static uint          Map_GetPathLengthCr( Map* map, Critter* cr, ushort to_hx, ushort to_hy, uint cut );
        static Critter*      Map_AddNpc( Map* map, hash proto_id, ushort hx, ushort hy, uchar dir, CScriptDict* props );
        static uint          Map_GetNpcCount( Map* map, int npc_role, int find_type );
        static Critter*      Map_GetNpc( Map* map, int npc_role, int find_type, uint skip_count );
        static bool          Map_IsHexPassed( Map* map, ushort hex_x, ushort hex_y );
        static bool          Map_IsHexesPassed( Map* map, ushort hex_x, ushort hex_y, uint radius );
        static bool          Map_IsHexRaked( Map* map, ushort hex_x, ushort hex_y );
        static void          Map_SetText( Map* map, ushort hex_x, ushort hex_y, uint color, string text );
        static void          Map_SetTextMsg( Map* map, ushort hex_x, ushort hex_y, uint color, ushort text_msg, uint str_num );
        static void          Map_SetTextMsgLex( Map* map, ushort hex_x, ushort hex_y, uint color, ushort text_msg, uint str_num, string lexems );
        static void          Map_RunEffect( Map* map, hash eff_pid, ushort hx, ushort hy, uint radius );
        static void          Map_RunFlyEffect( Map* map, hash eff_pid, Critter* from_cr, Critter* to_cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy );
        static bool          Map_CheckPlaceForItem( Map* map, ushort hx, ushort hy, hash pid );
        static void          Map_BlockHex( Map* map, ushort hx, ushort hy, bool full );
        static void          Map_UnblockHex( Map* map, ushort hx, ushort hy );
        static void          Map_PlaySound( Map* map, string sound_name );
        static void          Map_PlaySoundRadius( Map* map, string sound_name, ushort hx, ushort hy, uint radius );
        static bool          Map_Reload( Map* map );
        static void          Map_MoveHexByDir( Map* map, ushort& hx, ushort& hy, uchar dir, uint steps );
        static void          Map_VerifyTrigger( Map* map, Critter* cr, ushort hx, ushort hy, uchar dir );

        static uint          Location_GetMapCount( Location* loc );
        static Map*          Location_GetMap( Location* loc, hash map_pid );
        static Map*          Location_GetMapByIndex( Location* loc, uint index );
        static CScriptArray* Location_GetMaps( Location* loc );
        static bool          Location_GetEntrance( Location* loc, uint entrance, uint& map_index, hash& entire );
        static uint          Location_GetEntrances( Location* loc, CScriptArray* maps_index, CScriptArray* entires );
        static bool          Location_Reload( Location* loc );

        static Item*         Global_GetItem( uint item_id );
        static uint          Global_GetCrittersDistantion( Critter* cr1, Critter* cr2 );
        static void          Global_MoveItemCr( Item* item, uint count, Critter* to_cr, bool skip_checks );
        static void          Global_MoveItemMap( Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy, bool skip_checks );
        static void          Global_MoveItemCont( Item* item, uint count, Item* to_cont, uint stack_id, bool skip_checks );
        static void          Global_MoveItemsCr( CScriptArray* items, Critter* to_cr, bool skip_checks );
        static void          Global_MoveItemsMap( CScriptArray* items, Map* to_map, ushort to_hx, ushort to_hy, bool skip_checks );
        static void          Global_MoveItemsCont( CScriptArray* items, Item* to_cont, uint stack_id, bool skip_checks );
        static void          Global_DeleteItem( Item* item );
        static void          Global_DeleteItemById( uint item_id );
        static void          Global_DeleteItems( CScriptArray* items );
        static void          Global_DeleteItemsById( CScriptArray* item_ids );
        static void          Global_DeleteNpc( Critter* npc );
        static void          Global_DeleteNpcById( uint npc_id );
        static void          Global_RadioMessage( ushort channel, string text );
        static void          Global_RadioMessageMsg( ushort channel, ushort text_msg, uint num_str );
        static void          Global_RadioMessageMsgLex( ushort channel, ushort text_msg, uint num_str, string lexems );
        static uint          Global_GetFullSecond( ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second );
        static void          Global_GetGameTime( uint full_second, ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour, ushort& minute, ushort& second );
        static Location*     Global_CreateLocation( hash loc_pid, ushort wx, ushort wy, CScriptArray* critters );
        static void          Global_DeleteLocation( Location* loc );
        static void          Global_DeleteLocationById( uint loc_id );
        static Critter*      Global_GetCritter( uint crid );
        static Critter*      Global_GetPlayer( string name );
        static CScriptArray* Global_GetGlobalMapCritters( ushort wx, ushort wy, uint radius, int find_type );
        static void          Global_SetTime( ushort multiplier, ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second );
        static Map*          Global_GetMap( uint map_id );
        static Map*          Global_GetMapByPid( hash map_pid, uint skip_count );
        static Location*     Global_GetLocation( uint loc_id );
        static Location*     Global_GetLocationByPid( hash loc_pid, uint skip_count );
        static CScriptArray* Global_GetLocations( ushort wx, ushort wy, uint radius );
        static CScriptArray* Global_GetVisibleLocations( ushort wx, ushort wy, uint radius, Critter* cr );
        static CScriptArray* Global_GetZoneLocationIds( ushort zx, ushort zy, uint zone_radius );
        static bool          Global_RunDialogNpc( Critter* player, Critter* npc, bool ignore_distance );
        static bool          Global_RunDialogNpcDlgPack( Critter* player, Critter* npc, uint dlg_pack, bool ignore_distance );
        static bool          Global_RunDialogHex( Critter* player, uint dlg_pack, ushort hx, ushort hy, bool ignore_distance );
        static int64         Global_WorldItemCount( hash pid );
        static bool          Global_AddTextListener( int say_type, string first_str, uint parameter, asIScriptFunction* func );
        static void          Global_EraseTextListener( int say_type, string first_str, uint parameter );
        static bool          Global_SwapCritters( Critter* cr1, Critter* cr2, bool with_inventory );
        static CScriptArray* Global_GetAllItems( hash pid );
        static CScriptArray* Global_GetOnlinePlayers();
        static CScriptArray* Global_GetRegisteredPlayerIds();
        static CScriptArray* Global_GetAllNpc( hash pid );
        static CScriptArray* Global_GetAllMaps( hash pid );
        static CScriptArray* Global_GetAllLocations( hash pid );
        static void          Global_GetTime( ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour, ushort& minute, ushort& second, ushort& milliseconds );
        static void          Global_SetPropertyGetCallback( asIScriptGeneric* gen );
        static void          Global_AddPropertySetCallback( asIScriptGeneric* gen );
        static void          Global_AllowSlot( uchar index, bool enable_send );
        static bool          Global_LoadDataFile( string dat_name );
        // static uint Global_GetVersion();
        static bool Global_LoadImage( uint index, string image_name, uint image_depth );
        static uint Global_GetImageColor( uint index, uint x, uint y );
        static void Global_YieldWebRequest( string url, CScriptDict* post, bool& success, string& result );
        static void Global_YieldWebRequestExt( string url, CScriptArray* headers, string post, bool& success, string& result );
    } ScriptFunc;
};

#endif // __SERVER__
