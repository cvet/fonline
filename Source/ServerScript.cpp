#include "Common.h"
#include "Server.h"
#include "AngelScript/preprocessor.h"
#include "curl/curl.h"
#include "GraphicStructures.h"
#include "SHA/sha1.h"
#include "SHA/sha2.h"
#include "PNG/png.h"
#include <time.h>

static void* ASDebugMalloc( size_t size )
{
    size += sizeof( size_t );
    MEMORY_PROCESS( MEMORY_ANGEL_SCRIPT, (int) size );
    size_t* ptr = (size_t*) malloc( size );
    *ptr = size;
    return ++ptr;
}

static void ASDebugFree( void* ptr )
{
    size_t* ptr_ = (size_t*) ptr;
    size_t  size = *( --ptr_ );
    MEMORY_PROCESS( MEMORY_ANGEL_SCRIPT, -(int) size );
    free( ptr_ );
}

static bool                 ASDbgMemoryCanWork = false;
static THREAD bool          ASDbgMemoryInUse = false;
static map< void*, string > ASDbgMemoryPtr;
static string               ASDbgMemoryBuf;
static Mutex                ASDbgMemoryLocker;

static void* ASDeepDebugMalloc( size_t size )
{
    size += sizeof( size_t );
    size_t* ptr = (size_t*) malloc( size );
    *ptr = size;

    if( ASDbgMemoryCanWork && !ASDbgMemoryInUse )
    {
        SCOPE_LOCK( ASDbgMemoryLocker );
        ASDbgMemoryInUse = true;
        string func = Script::GetActiveFuncName();
        ASDbgMemoryBuf = _str( "AS : {}", !func.empty() ? func : "<nullptr>" );
        MEMORY_PROCESS_STR( ASDbgMemoryBuf.c_str(), (int) size );
        ASDbgMemoryPtr.insert( std::make_pair( ptr, ASDbgMemoryBuf ) );
        ASDbgMemoryInUse = false;
    }
    MEMORY_PROCESS( MEMORY_ANGEL_SCRIPT, (int) size );

    return ++ptr;
}

static void ASDeepDebugFree( void* ptr )
{
    size_t* ptr_ = (size_t*) ptr;
    size_t  size = *( --ptr_ );

    if( ASDbgMemoryCanWork )
    {
        SCOPE_LOCK( ASDbgMemoryLocker );
        auto it = ASDbgMemoryPtr.find( ptr_ );
        if( it != ASDbgMemoryPtr.end() )
        {
            MEMORY_PROCESS_STR( it->second.c_str(), -(int) size );
            ASDbgMemoryPtr.erase( it );
        }
    }
    MEMORY_PROCESS( MEMORY_ANGEL_SCRIPT, -(int) size );

    free( ptr_ );
}

namespace ServerBind
{
    #undef BIND_SERVER
    #undef BIND_CLIENT
    #undef BIND_MAPPER
    #undef BIND_CLASS
    #undef BIND_ASSERT
    #undef BIND_DUMMY_DATA
    #define BIND_SERVER
    #define BIND_CLASS    FOServer::SScriptFunc::
    #define BIND_ASSERT( x )               if( ( x ) < 0 ) { WriteLog( "Bind error, line {}.\n", __LINE__ ); return false; }
    #include "ScriptBind.h"
}

bool FOServer::InitScriptSystem()
{
    WriteLog( "Script system initialization...\n" );

    // Memory debugging
    #ifdef MEMORY_DEBUG
    asThreadCleanup();
    if( MemoryDebugLevel >= 2 )
        asSetGlobalMemoryFunctions( ASDeepDebugMalloc, ASDeepDebugFree );
    else if( MemoryDebugLevel >= 1 )
        asSetGlobalMemoryFunctions( ASDebugMalloc, ASDebugFree );
    else
        asSetGlobalMemoryFunctions( malloc, free );
    #endif

    // Profiler settings
    uint sample_time = MainConfig->GetInt( "", "ProfilerSampleInterval", 0 );
    uint profiler_mode = MainConfig->GetInt( "", "ProfilerMode", 0 );
    if( !profiler_mode )
        sample_time = 0;

    // Init
    ScriptPragmaCallback* pragma_callback = new ScriptPragmaCallback( PRAGMA_SERVER );
    if( !Script::Init( pragma_callback, "SERVER", AllowServerNativeCalls,
                       sample_time, ( profiler_mode & 1 ) != 0, ( profiler_mode & 2 ) != 0 ) )
    {
        WriteLog( "Script system initialization failed.\n" );
        return false;
    }

    // Bind vars and functions, look bind.h
    asIScriptEngine*      engine = Script::GetEngine();
    PropertyRegistrator** registrators = pragma_callback->GetPropertyRegistrators();
    if( ServerBind::Bind( engine, registrators ) )
        return false;

    // Load script modules
    Script::Undef( "" );
    Script::Define( "__SERVER" );
    Script::Define( _str( "__VERSION {}", FONLINE_VERSION ) );
    if( !Script::ReloadScripts( "Server" ) )
    {
        Script::Finish();
        WriteLog( "Reload scripts fail.\n" );
        return false;
    }

    // Store property pragmas to synchronize with client
    ServerPropertyPragmas.clear();
    EngineData*    ed = (EngineData*) engine->GetUserData();
    const Pragmas& pragmas = ed->PragmaCB->GetProcessedPragmas();
    for( auto it = pragmas.begin(); it != pragmas.end(); ++it )
    {
        const Preprocessor::PragmaInstance& pragma = *it;
        if( pragma.Name == "property" )
            ServerPropertyPragmas.push_back( pragma );
    }

    // Bind game functions
    #define BIND_INTERNAL_EVENT( name )    ServerFunctions.name = Script::FindInternalEvent( "Event" # name )
    BIND_INTERNAL_EVENT( ResourcesGenerated );
    BIND_INTERNAL_EVENT( Init );
    BIND_INTERNAL_EVENT( GenerateWorld );
    BIND_INTERNAL_EVENT( Start );
    BIND_INTERNAL_EVENT( Finish );
    BIND_INTERNAL_EVENT( Loop );
    BIND_INTERNAL_EVENT( GlobalMapCritterIn );
    BIND_INTERNAL_EVENT( GlobalMapCritterOut );
    BIND_INTERNAL_EVENT( LocationInit );
    BIND_INTERNAL_EVENT( LocationFinish );
    BIND_INTERNAL_EVENT( MapInit );
    BIND_INTERNAL_EVENT( MapFinish );
    BIND_INTERNAL_EVENT( MapLoop );
    BIND_INTERNAL_EVENT( MapCritterIn );
    BIND_INTERNAL_EVENT( MapCritterOut );
    BIND_INTERNAL_EVENT( MapCheckLook );
    BIND_INTERNAL_EVENT( MapCheckTrapLook );
    BIND_INTERNAL_EVENT( CritterInit );
    BIND_INTERNAL_EVENT( CritterFinish );
    BIND_INTERNAL_EVENT( CritterIdle );
    BIND_INTERNAL_EVENT( CritterGlobalMapIdle );
    BIND_INTERNAL_EVENT( CritterCheckMoveItem );
    BIND_INTERNAL_EVENT( CritterMoveItem );
    BIND_INTERNAL_EVENT( CritterShow );
    BIND_INTERNAL_EVENT( CritterShowDist1 );
    BIND_INTERNAL_EVENT( CritterShowDist2 );
    BIND_INTERNAL_EVENT( CritterShowDist3 );
    BIND_INTERNAL_EVENT( CritterHide );
    BIND_INTERNAL_EVENT( CritterHideDist1 );
    BIND_INTERNAL_EVENT( CritterHideDist2 );
    BIND_INTERNAL_EVENT( CritterHideDist3 );
    BIND_INTERNAL_EVENT( CritterShowItemOnMap );
    BIND_INTERNAL_EVENT( CritterHideItemOnMap );
    BIND_INTERNAL_EVENT( CritterChangeItemOnMap );
    BIND_INTERNAL_EVENT( CritterMessage );
    BIND_INTERNAL_EVENT( CritterTalk );
    BIND_INTERNAL_EVENT( CritterBarter );
    BIND_INTERNAL_EVENT( CritterGetAttackDistantion );
    BIND_INTERNAL_EVENT( PlayerRegistration );
    BIND_INTERNAL_EVENT( PlayerLogin );
    BIND_INTERNAL_EVENT( PlayerGetAccess );
    BIND_INTERNAL_EVENT( PlayerAllowCommand );
    BIND_INTERNAL_EVENT( PlayerLogout );
    BIND_INTERNAL_EVENT( ItemInit );
    BIND_INTERNAL_EVENT( ItemFinish );
    BIND_INTERNAL_EVENT( ItemWalk );
    BIND_INTERNAL_EVENT( ItemCheckMove );
    BIND_INTERNAL_EVENT( StaticItemWalk );
    #undef BIND_INTERNAL_EVENT

    ASDbgMemoryCanWork = true;

    GlobalVars::SetPropertyRegistrator( registrators[ 0 ] );
    GlobalVars::PropertiesRegistrator->SetNativeSendCallback( OnSendGlobalValue );
    Globals = new GlobalVars();
    Critter::SetPropertyRegistrator( registrators[ 1 ] );
    Critter::PropertiesRegistrator->SetNativeSendCallback( OnSendCritterValue );
    Item::SetPropertyRegistrator( registrators[ 2 ] );
    Item::PropertiesRegistrator->SetNativeSendCallback( OnSendItemValue );
    Item::PropertiesRegistrator->SetNativeSetCallback( "Count", OnSetItemCount );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsHidden", OnSetItemChangeView );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsAlwaysView", OnSetItemChangeView );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsTrap", OnSetItemChangeView );
    Item::PropertiesRegistrator->SetNativeSetCallback( "TrapValue", OnSetItemChangeView );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsNoBlock", OnSetItemRecacheHex );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsShootThru", OnSetItemRecacheHex );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsGag", OnSetItemRecacheHex );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsTrigger", OnSetItemRecacheHex );
    Item::PropertiesRegistrator->SetNativeSetCallback( "BlockLines", OnSetItemBlockLines );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsGeck", OnSetItemIsGeck );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsRadio", OnSetItemIsRadio );
    Item::PropertiesRegistrator->SetNativeSetCallback( "Opened", OnSetItemOpened );
    Map::SetPropertyRegistrator( registrators[ 3 ] );
    Map::PropertiesRegistrator->SetNativeSendCallback( OnSendMapValue );
    Location::SetPropertyRegistrator( registrators[ 4 ] );
    Location::PropertiesRegistrator->SetNativeSendCallback( OnSendLocationValue );

    WriteLog( "Script system initialization complete.\n" );
    return true;
}

void FOServer::FinishScriptSystem()
{
    WriteLog( "Script system finish...\n" );
    Script::Finish();
    WriteLog( "Script system finish complete.\n" );
}

bool FOServer::DialogScriptDemand( DemandResult& demand, Critter* master, Critter* slave )
{
    int bind_id = (int) demand.ParamId;
    Script::PrepareContext( bind_id, master->GetName() );
    Script::SetArgEntity( master );
    Script::SetArgEntity( slave );
    for( int i = 0; i < demand.ValuesCount; i++ )
        Script::SetArgUInt( demand.ValueExt[ i ] );
    if( Script::RunPrepared() )
        return Script::GetReturnedBool();
    return false;
}

uint FOServer::DialogScriptResult( DemandResult& result, Critter* master, Critter* slave )
{
    int bind_id = (int) result.ParamId;
    Script::PrepareContext( bind_id, _str( "Critter '{}', func '{}'", master->GetName(), Script::GetBindFuncName( bind_id ) ) );
    Script::SetArgEntity( master );
    Script::SetArgEntity( slave );
    for( int i = 0; i < result.ValuesCount; i++ )
        Script::SetArgUInt( result.ValueExt[ i ] );
    if( Script::RunPrepared() && result.RetValue )
        return Script::GetReturnedUInt();
    return 0;
}

/************************************************************************/
/* Client script processing                                             */
/************************************************************************/

namespace ClientBind
{
    #undef BIND_SERVER
    #undef BIND_CLIENT
    #undef BIND_MAPPER
    #undef BIND_CLASS
    #undef BIND_ASSERT
    #undef BIND_DUMMY_DATA
    #define BIND_CLIENT
    #define BIND_CLASS    BindClass::
    #define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLog( "Bind error, line {}.\n", __LINE__ ); errors++; }
    #define BIND_DUMMY_DATA
    #include "ScriptBind.h"
}

bool FOServer::ReloadClientScripts()
{
    WriteLog( "Reload client scripts...\n" );

    // Disable debug allocators
    #ifdef MEMORY_DEBUG
    asThreadCleanup();
    asSetGlobalMemoryFunctions( malloc, free );
    #endif

    // Swap engine
    asIScriptEngine*      old_engine = Script::GetEngine();
    ScriptPragmaCallback* pragma_callback = new ScriptPragmaCallback( PRAGMA_CLIENT );
    asIScriptEngine*      engine = Script::CreateEngine( pragma_callback, "CLIENT", AllowClientNativeCalls );
    if( engine )
        Script::SetEngine( engine );

    // Properties
    PropertyRegistrator** registrators = pragma_callback->GetPropertyRegistrators();

    // Bind vars and functions
    int bind_errors = 0;
    if( engine )
        bind_errors = ClientBind::Bind( engine, registrators );

    // Check errors
    if( !engine || bind_errors )
    {
        if( !engine )
            WriteLog( "asCreateScriptEngine fail.\n" );
        else
            WriteLog( "Bind fail, errors {}.\n", bind_errors );
        Script::FinishEngine( engine );

        #ifdef MEMORY_DEBUG
        asThreadCleanup();
        if( MemoryDebugLevel >= 2 )
            asSetGlobalMemoryFunctions( ASDeepDebugMalloc, ASDeepDebugFree );
        else if( MemoryDebugLevel >= 1 )
            asSetGlobalMemoryFunctions( ASDebugMalloc, ASDebugFree );
        else
            asSetGlobalMemoryFunctions( malloc, free );
        #endif
        return false;
    }

    // Load script modules
    Script::Undef( "__SERVER" );
    Script::Define( "__CLIENT" );
    Script::Define( _str( "__VERSION {}", FONLINE_VERSION ) );
    Script::SetLoadLibraryCompiler( true );

    FOMsg msg_script;
    int   errors = 0;
    if( Script::ReloadScripts( "Client" ) )
    {
        RUNTIME_ASSERT( engine->GetModuleCount() == 1 );
        asIScriptModule* module = engine->GetModuleByIndex( 0 );
        CBytecodeStream  binary;
        if( module->SaveByteCode( &binary ) >= 0 )
        {
            std::vector< asBYTE >&              buf = binary.GetBuf();

            UCharVec                            lnt_data;
            Preprocessor::LineNumberTranslator* lnt = (Preprocessor::LineNumberTranslator*) module->GetUserData();
            Preprocessor::StoreLineNumberTranslator( lnt, lnt_data );

            // Store data for client
            msg_script.AddBinary( STR_INTERNAL_SCRIPT_MODULE, (uchar*) &buf[ 0 ], (uint) buf.size() );
            msg_script.AddBinary( STR_INTERNAL_SCRIPT_MODULE + 1, (uchar*) &lnt_data[ 0 ], (uint) lnt_data.size() );
        }
        else
        {
            WriteLog( "Unable to save bytecode of client script.\n" );
            errors++;
        }
    }
    else
    {
        errors++;
    }

    // Add native dlls to MSG
    int         dll_num = STR_INTERNAL_SCRIPT_DLLS;
    EngineData* ed = (EngineData*) engine->GetUserData();
    for( auto it = ed->LoadedDlls.begin(), end = ed->LoadedDlls.end(); it != end; ++it )
    {
        const string& dll_name = it->first;
        const string& dll_path = it->second.first;

        // Load libraries for all platforms
        // Windows, Linux
        for( int d = 0; d < 2; d++ )
        {
            // Make file name
            const char* extensions[] = { ".dll", ".so" };
            string      fname = _str( dll_path ).eraseFileExtension() + extensions[ d ];

            // Erase first './'
            if( _str( fname ).startsWith( "./" ) )
                fname.erase( 0, 2 );

            // Load dll
            FileManager dll;
            if( !dll.LoadFile( fname ) )
            {
                if( !d )
                {
                    WriteLog( "Can't load dll '{}'.\n", dll_name );
                    errors++;
                }
                continue;
            }

            // Add dll name and binary
            msg_script.AddStr( dll_num, fname );
            msg_script.AddBinary( dll_num + 1, dll.GetBuf(), dll.GetFsize() );
            dll_num += 2;
        }
    }

    // Finish
    Pragmas pragmas = ed->PragmaCB->GetProcessedPragmas();
    Script::FinishEngine( engine );
    Script::Undef( "__CLIENT" );
    Script::Define( "__SERVER" );
    Script::SetLoadLibraryCompiler( false );

    #ifdef MEMORY_DEBUG
    asThreadCleanup();
    if( MemoryDebugLevel >= 2 )
        asSetGlobalMemoryFunctions( ASDeepDebugMalloc, ASDeepDebugFree );
    else if( MemoryDebugLevel >= 1 )
        asSetGlobalMemoryFunctions( ASDebugMalloc, ASDebugFree );
    else
        asSetGlobalMemoryFunctions( malloc, free );
    #endif
    Script::SetEngine( old_engine );

    // Add config text and pragmas
    uint pragma_index = 0;
    for( size_t i = 0; i < pragmas.size(); i++ )
    {
        if( pragmas[ i ].Name != "property" )
        {
            // All pragmas exclude 'property'
            msg_script.AddStr( STR_INTERNAL_SCRIPT_PRAGMAS + pragma_index * 3, pragmas[ i ].Name );
            msg_script.AddStr( STR_INTERNAL_SCRIPT_PRAGMAS + pragma_index * 3 + 1, pragmas[ i ].Text );
            msg_script.AddStr( STR_INTERNAL_SCRIPT_PRAGMAS + pragma_index * 3 + 2, pragmas[ i ].CurrentFile );
            pragma_index++;
        }
        else
        {
            // Verify client property, it is must present in server scripts
            bool found = false;
            for( size_t j = 0; j < ServerPropertyPragmas.size(); j++ )
            {
                if( ServerPropertyPragmas[ j ].Text == pragmas[ i ].Text )
                {
                    found = true;
                    break;
                }
            }
            if( !found )
            {
                WriteLog( "Property '{}' not registered in server scripts.\n", pragmas[ i ].Text );
                errors++;
            }
        }
    }
    for( size_t i = 0; i < ServerPropertyPragmas.size(); i++ )
    {
        // All 'property' pragmas
        msg_script.AddStr( STR_INTERNAL_SCRIPT_PRAGMAS + pragma_index * 3, ServerPropertyPragmas[ i ].Name );
        msg_script.AddStr( STR_INTERNAL_SCRIPT_PRAGMAS + pragma_index * 3 + 1, ServerPropertyPragmas[ i ].Text );
        msg_script.AddStr( STR_INTERNAL_SCRIPT_PRAGMAS + pragma_index * 3 + 2, ServerPropertyPragmas[ i ].CurrentFile );
        pragma_index++;
    }

    // Exit if have errors
    if( errors )
        return false;

    // Copy generated MSG to language packs
    for( auto it = LangPacks.begin(), end = LangPacks.end(); it != end; ++it )
    {
        LanguagePack& lang = *it;
        lang.Msg[ TEXTMSG_INTERNAL ] = msg_script;
    }

    // Regenerate update files
    GenerateUpdateFiles();

    WriteLog( "Reload client scripts complete.\n" );
    return true;
}

/************************************************************************/
/* Wrapper functions                                                    */
/************************************************************************/

static int SortCritterHx = 0;
static int SortCritterHy = 0;
static bool SortCritterByDistPred( Critter* cr1, Critter* cr2 )
{
    return DistGame( SortCritterHx, SortCritterHy, cr1->GetHexX(), cr1->GetHexY() ) < DistGame( SortCritterHx, SortCritterHy, cr2->GetHexX(), cr2->GetHexY() );
}
static void SortCritterByDist( Critter* cr, CrVec& critters )
{
    SortCritterHx = cr->GetHexX();
    SortCritterHy = cr->GetHexY();
    std::sort( critters.begin(), critters.end(), SortCritterByDistPred );
}
static void SortCritterByDist( int hx, int hy, CrVec& critters )
{
    SortCritterHx = hx;
    SortCritterHy = hy;
    std::sort( critters.begin(), critters.end(), SortCritterByDistPred );
}

Item* FOServer::SScriptFunc::Item_AddItem( Item* cont, hash pid, uint count, uint stack_id )
{
    if( cont->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !ProtoMngr.GetProtoItem( pid ) )
        SCRIPT_ERROR_R0( "Invalid proto '{}' arg.", _str().parseHash( pid ) );

    if( !count )
        count = 1;
    return ItemMngr.AddItemContainer( cont, pid, count, stack_id );
}

CScriptArray* FOServer::SScriptFunc::Item_GetItems( Item* cont, uint stack_id )
{
    if( cont->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    ItemVec items;
    cont->ContGetItems( items, stack_id );
    return Script::CreateArrayRef( "Item[]", items );
}

bool FOServer::SScriptFunc::Item_SetScript( Item* item, asIScriptFunction* func )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    if( func )
    {
        if( !item->SetScript( func, true ) )
            SCRIPT_ERROR_R0( "Script function not found." );
    }
    else
    {
        item->SetScriptId( 0 );
    }
    return true;
}

Map* FOServer::SScriptFunc::Item_GetMapPosition( Item* item, ushort& hx, ushort& hy )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    Map* map = nullptr;
    switch( item->GetAccessory() )
    {
    case ITEM_ACCESSORY_CRITTER:
    {
        Critter* cr = CrMngr.GetCritter( item->GetCritId() );
        if( !cr )
            SCRIPT_ERROR_R0( "Critter accessory, critter not found." );
        if( !cr->GetMapId() )
        {
            hx = cr->GetWorldX();
            hy = cr->GetWorldY();
            return nullptr;
        }
        map = MapMngr.GetMap( cr->GetMapId() );
        if( !map )
            SCRIPT_ERROR_R0( "Critter accessory, map not found." );
        hx = cr->GetHexX();
        hy = cr->GetHexY();
    }
    break;
    case ITEM_ACCESSORY_HEX:
    {
        map = MapMngr.GetMap( item->GetMapId() );
        if( !map )
            SCRIPT_ERROR_R0( "Hex accessory, map not found." );
        hx = item->GetHexX();
        hy = item->GetHexY();
    }
    break;
    case ITEM_ACCESSORY_CONTAINER:
    {
        if( item->GetId() == item->GetContainerId() )
            SCRIPT_ERROR_R0( "Container accessory, crosslink." );
        Item* cont = ItemMngr.GetItem( item->GetContainerId() );
        if( !cont )
            SCRIPT_ERROR_R0( "Container accessory, container not found." );
        return Item_GetMapPosition( cont, hx, hy );             // Recursion
    }
    break;
    default:
        SCRIPT_ERROR_R0( "Unknown accessory." );
        break;
    }
    return map;
}

bool FOServer::SScriptFunc::Item_ChangeProto( Item* item, hash pid )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ProtoItem* proto_item = ProtoMngr.GetProtoItem( pid );
    if( !proto_item )
        SCRIPT_ERROR_R0( "Proto item not found." );

    ProtoItem* old_proto_item = item->GetProtoItem();
    item->SetProto( proto_item );

    if( item->GetAccessory() == ITEM_ACCESSORY_CRITTER )
    {
        Critter* cr = CrMngr.GetCritter( item->GetCritId() );
        if( !cr )
            return true;
        item->SetProto( old_proto_item );
        cr->Send_EraseItem( item );
        item->SetProto( proto_item );
        cr->Send_AddItem( item );
        cr->SendAA_MoveItem( item, ACTION_REFRESH, 0 );
    }
    else if( item->GetAccessory() == ITEM_ACCESSORY_HEX )
    {
        Map* map = MapMngr.GetMap( item->GetMapId() );
        if( !map )
            return true;
        ushort hx = item->GetHexX();
        ushort hy = item->GetHexY();
        item->SetProto( old_proto_item );
        map->EraseItem( item->GetId() );
        item->SetProto( proto_item );
        map->AddItem( item, hx, hy );
    }
    return true;
}

void FOServer::SScriptFunc::Item_Animate( Item* item, uchar from_frm, uchar to_frm )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    switch( item->GetAccessory() )
    {
    case ITEM_ACCESSORY_CRITTER:
    {
        // Critter* cr=CrMngr.GetCrit(item->ACC_CRITTER.Id);
        // if(cr) cr->Send_AnimateItem(item,from_frm,to_frm);
    }
    break;
    case ITEM_ACCESSORY_HEX:
    {
        Map* map = MapMngr.GetMap( item->GetMapId() );
        if( map )
            map->AnimateItem( item, from_frm, to_frm );
    }
    break;
    case ITEM_ACCESSORY_CONTAINER:
        break;
    default:
        SCRIPT_ERROR_R( "Unknown accessory." );
    }
}

bool FOServer::SScriptFunc::Item_CallStaticItemFunction( Item* static_item, Critter* cr, Item* item, int param )
{
    if( !static_item->SceneryScriptBindId )
        return false;

    Script::PrepareContext( static_item->SceneryScriptBindId, cr->GetName() );
    Script::SetArgEntity( cr );
    Script::SetArgEntity( static_item );
    Script::SetArgEntity( item );
    Script::SetArgUInt( param );
    return Script::RunPrepared() && Script::GetReturnedBool();
}

bool FOServer::SScriptFunc::Crit_IsPlayer( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsPlayer();
}

bool FOServer::SScriptFunc::Crit_IsNpc( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsNpc();
}

int FOServer::SScriptFunc::Cl_GetAccess( Critter* cl )
{
    if( cl->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cl->IsPlayer() )
        SCRIPT_ERROR_R0( "Critter is not player." );

    return ( (Client*) cl )->Access;
}

bool FOServer::SScriptFunc::Cl_SetAccess( Critter* cl, int access )
{
    if( cl->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cl->IsPlayer() )
        SCRIPT_ERROR_R0( "Critter is not player." );
    if( access < ACCESS_CLIENT || access > ACCESS_ADMIN )
        SCRIPT_ERROR_R0( "Wrong access type." );

    if( access == ( (Client*) cl )->Access )
        return true;

    string pass;
    bool   allow = Script::RaiseInternalEvent( ServerFunctions.PlayerGetAccess, cl, access, &pass );

    if( allow )
        ( (Client*) cl )->Access = access;
    return allow;
}

Map* FOServer::SScriptFunc::Crit_GetMap( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return MapMngr.GetMap( cr->GetMapId() );
}

bool FOServer::SScriptFunc::Crit_MoveToDir( Critter* cr, uchar direction )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( !map )
        SCRIPT_ERROR_R0( "Critter is on global." );
    if( direction >= DIRS_COUNT )
        SCRIPT_ERROR_R0( "Invalid direction arg." );

    ushort hx = cr->GetHexX();
    ushort hy = cr->GetHexY();
    MoveHexByDir( hx, hy, direction, map->GetWidth(), map->GetHeight() );
    ushort move_flags = direction | BIN16( 00000000, 00111000 );
    bool   move = Act_Move( cr, hx, hy, move_flags );
    if( !move )
        SCRIPT_ERROR_R0( "Move fail." );
    cr->Send_Move( cr, move_flags );
    return true;
}

void FOServer::SScriptFunc::Crit_TransitToHex( Critter* cr, ushort hx, ushort hy, uchar dir )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->LockMapTransfers )
        SCRIPT_ERROR_R( "Transfers locked." );
    Map* map = cr->GetMap();
    if( !map )
        SCRIPT_ERROR_R( "Critter is on global." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid hexes args." );

    if( hx != cr->GetHexX() || hy != cr->GetHexY() )
    {
        if( dir < DIRS_COUNT && cr->GetDir() != dir )
            cr->SetDir( dir );
        if( !MapMngr.Transit( cr, map, hx, hy, cr->GetDir(), 2, 0, true ) )
            SCRIPT_ERROR_R( "Transit fail." );
    }
    else if( dir < DIRS_COUNT && cr->GetDir() != dir )
    {
        cr->SetDir( dir );
        cr->Send_Dir( cr );
        cr->SendA_Dir();
    }
}

void FOServer::SScriptFunc::Crit_TransitToMapHex( Critter* cr, Map* map, ushort hx, ushort hy, uchar dir )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->LockMapTransfers )
        SCRIPT_ERROR_R( "Transfers locked." );
    if( !map )
        SCRIPT_ERROR_R( "Map arg is null." );
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Map arg is destroyed." );
    if( dir >= DIRS_COUNT )
        dir = 0;

    if( !MapMngr.Transit( cr, map, hx, hy, dir, 2, 0, true ) )
        SCRIPT_ERROR_R( "Transit to map hex fail." );

    // Todo: need???
    Location* loc = map->GetLocation();
    if( loc && DistSqrt( cr->GetWorldX(), cr->GetWorldY(), loc->GetWorldX(), loc->GetWorldY() ) > loc->GetRadius() )
    {
        cr->SetWorldX( loc->GetWorldX() );
        cr->SetWorldY( loc->GetWorldY() );
    }
}

void FOServer::SScriptFunc::Crit_TransitToGlobal( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->LockMapTransfers )
        SCRIPT_ERROR_R( "Transfers locked." );

    if( cr->GetMap() && !MapMngr.TransitToGlobal( cr, 0, true ) )
        SCRIPT_ERROR_R( "Transit fail." );
}

void FOServer::SScriptFunc::Crit_TransitToGlobalWithGroup( Critter* cr, CScriptArray* group )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->LockMapTransfers )
        SCRIPT_ERROR_R( "Transfers locked." );
    if( !cr->GetMapId() )
        SCRIPT_ERROR_R( "Critter already on global." );
    if( !group )
        SCRIPT_ERROR_R( "Group arg is null." );

    if( !MapMngr.TransitToGlobal( cr, 0, true ) )
        SCRIPT_ERROR_R( "Transit fail." );

    for( int i = 0, j = group->GetSize(); i < j; i++ )
    {
        Critter* cr_ = *(Critter**) group->At( i );
        if( cr_ && !cr_->IsDestroyed )
            MapMngr.TransitToGlobal( cr_, cr->GetId(), true );
    }
}

void FOServer::SScriptFunc::Crit_TransitToGlobalGroup( Critter* cr, Critter* leader )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->LockMapTransfers )
        SCRIPT_ERROR_R( "Transfers locked." );
    if( !cr->GetMapId() )
        SCRIPT_ERROR_R( "Critter already on global." );
    if( !leader )
        SCRIPT_ERROR_R( "Leader arg not found." );
    if( leader->IsDestroyed )
        SCRIPT_ERROR_R( "Leader arg is destroyed." );
    if( !leader->GlobalMapGroup )
        SCRIPT_ERROR_R( "Leader is not on global map." );

    if( !MapMngr.TransitToGlobal( cr, leader->GetId(), true ) )
        SCRIPT_ERROR_R( "Transit fail." );
}

bool FOServer::SScriptFunc::Crit_IsLife( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsLife();
}

bool FOServer::SScriptFunc::Crit_IsKnockout( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsKnockout();
}

bool FOServer::SScriptFunc::Crit_IsDead( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsDead();
}

bool FOServer::SScriptFunc::Crit_IsFree( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsFree() && !cr->IsWait();
}

bool FOServer::SScriptFunc::Crit_IsBusy( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->IsBusy() || cr->IsWait();
}

void FOServer::SScriptFunc::Crit_Wait( Critter* cr, uint ms )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    cr->SetWait( ms );
    if( cr->IsPlayer() )
    {
        Client* cl = (Client*) cr;
        cl->SetBreakTime( ms );
        cl->Send_CustomCommand( cr, OTHER_BREAK_TIME, ms );
    }
}

void FOServer::SScriptFunc::Crit_RefreshVisible( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    cr->ProcessVisibleCritters();
    cr->ProcessVisibleItems();
}

void FOServer::SScriptFunc::Crit_ViewMap( Critter* cr, Map* map, uint look, ushort hx, ushort hy, uchar dir )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !map )
        SCRIPT_ERROR_R( "Map arg is null." );
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Map arg is destroyed." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid hexes args." );

    if( !cr->IsPlayer() )
        return;

    if( dir >= DIRS_COUNT )
        dir = cr->GetDir();
    if( !look )
        look = cr->GetLookDistance();

    cr->ViewMapId = map->GetId();
    cr->ViewMapPid = map->GetProtoId();
    cr->ViewMapLook = look;
    cr->ViewMapHx = hx;
    cr->ViewMapHy = hy;
    cr->ViewMapDir = dir;
    cr->ViewMapLocId = 0;
    cr->ViewMapLocEnt = 0;
    cr->Send_LoadMap( map );
}

void FOServer::SScriptFunc::Crit_Say( Critter* cr, uchar how_say, string text )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    if( how_say != SAY_FLASH_WINDOW && !text.length() )
        SCRIPT_ERROR_R( "Text empty." );
    if( cr->IsNpc() && !cr->IsLife() )
        return;                                  // SCRIPT_ERROR_R("Npc is not life.");

    if( how_say >= SAY_NETMSG )
        cr->Send_Text( cr, how_say != SAY_FLASH_WINDOW ? text : " ", how_say );
    else if( cr->GetMapId() )
        cr->SendAA_Text( cr->VisCr, text, how_say, false );
}

void FOServer::SScriptFunc::Crit_SayMsg( Critter* cr, uchar how_say, ushort text_msg, uint num_str )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    if( cr->IsNpc() && !cr->IsLife() )
        return;                                  // SCRIPT_ERROR_R("Npc is not life.");

    if( how_say >= SAY_NETMSG )
        cr->Send_TextMsg( cr, num_str, how_say, text_msg );
    else if( cr->GetMapId() )
        cr->SendAA_Msg( cr->VisCr, num_str, how_say, text_msg );
}

void FOServer::SScriptFunc::Crit_SayMsgLex( Critter* cr, uchar how_say, ushort text_msg, uint num_str, string lexems )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    // Npc is not life
    if( cr->IsNpc() && !cr->IsLife() )
        return;

    if( how_say >= SAY_NETMSG )
        cr->Send_TextMsgLex( cr, num_str, how_say, text_msg, lexems.c_str() );
    else if( cr->GetMapId() )
        cr->SendAA_MsgLex( cr->VisCr, num_str, how_say, text_msg, lexems.c_str() );
}

void FOServer::SScriptFunc::Crit_SetDir( Critter* cr, uchar dir )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( dir >= DIRS_COUNT )
        SCRIPT_ERROR_R( "Invalid direction arg." );

    // Direction already set
    if( cr->GetDir() == dir )
        return;

    cr->SetDir( dir );
    if( cr->GetMapId() )
    {
        cr->Send_Dir( cr );
        cr->SendA_Dir();
    }
}

CScriptArray* FOServer::SScriptFunc::Crit_GetCritters( Critter* cr, bool look_on_me, int find_type )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CrVec critters;
    for( auto it = ( look_on_me ? cr->VisCr.begin() : cr->VisCrSelf.begin() ), end = ( look_on_me ? cr->VisCr.end() : cr->VisCrSelf.end() ); it != end; ++it )
    {
        Critter* cr_ = *it;
        if( cr_->CheckFind( find_type ) )
            critters.push_back( cr_ );
    }

    SortCritterByDist( cr, critters );
    return Script::CreateArrayRef( "Critter[]", critters );
}

CScriptArray* FOServer::SScriptFunc::Npc_GetTalkedPlayers( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cr->IsNpc() )
        SCRIPT_ERROR_R0( "Critter is not npc." );

    CrVec players;
    for( auto cr_ : cr->VisCr )
    {
        if( cr_->IsPlayer() )
        {
            Client* cl = (Client*) cr_;
            if( cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == cr->GetId() )
                players.push_back( cl );
        }
    }

    SortCritterByDist( cr, players );
    return Script::CreateArrayRef( "Critter[]", players );
}

bool FOServer::SScriptFunc::Crit_IsSeeCr( Critter* cr, Critter* cr_ )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cr_ )
        SCRIPT_ERROR_R0( "Critter arg is null." );
    if( cr_->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter arg is destroyed." );

    if( cr == cr_ )
        return true;

    CrVec& critters = ( cr->GetMapId() ? cr->VisCrSelf : *cr->GlobalMapGroup );
    return std::find( critters.begin(), critters.end(), cr_ ) != critters.end();
}

bool FOServer::SScriptFunc::Crit_IsSeenByCr( Critter* cr, Critter* cr_ )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cr_ )
        SCRIPT_ERROR_R0( "Critter arg is null." );
    if( cr_->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter arg is destroyed." );

    if( cr == cr_ )
        return true;

    CrVec& critters = ( cr->GetMapId() ? cr->VisCr : *cr->GlobalMapGroup );
    return std::find( critters.begin(), critters.end(), cr_ ) != critters.end();
}

bool FOServer::SScriptFunc::Crit_IsSeeItem( Critter* cr, Item* item )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !item )
        SCRIPT_ERROR_R0( "Item arg is null." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Item arg is destroyed." );

    return cr->CountIdVisItem( item->GetId() );
}

uint FOServer::SScriptFunc::Crit_CountItem( Critter* cr, hash proto_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->CountItemPid( proto_id );
}

bool FOServer::SScriptFunc::Crit_DeleteItem( Critter* cr, hash pid, uint count )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !pid )
        SCRIPT_ERROR_R0( "Proto id arg is zero." );

    if( !count )
        count = cr->CountItemPid( pid );
    return ItemMngr.SubItemCritter( cr, pid, count );
}

Item* FOServer::SScriptFunc::Crit_AddItem( Critter* cr, hash pid, uint count )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !pid )
        SCRIPT_ERROR_R0( "Proto id arg is zero." );
    if( !ProtoMngr.GetProtoItem( pid ) )
        SCRIPT_ERROR_R0( "Invalid proto '{}'.", _str().parseHash( pid ) );

    if( !count )
        count = 1;
    return ItemMngr.AddItemCritter( cr, pid, count );
}

Item* FOServer::SScriptFunc::Crit_GetItem( Critter* cr, uint item_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->GetItem( item_id, false );
}

Item* FOServer::SScriptFunc::Crit_GetItemPredicate( Critter* cr, asIScriptFunction* predicate )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    uint bind_id = Script::BindByFunc( predicate, true );
    RUNTIME_ASSERT( bind_id );

    ItemVec inv_items = cr->GetInventory();
    for( Item* item : inv_items )
    {
        if( item->IsDestroyed )
            continue;

        Script::PrepareContext( bind_id, "Predicate" );
        Script::SetArgObject( item );
        if( !Script::RunPrepared() )
        {
            Script::PassException();
            return nullptr;
        }

        if( Script::GetReturnedBool() && !item->IsDestroyed )
            return item;
    }
    return nullptr;
}

Item* FOServer::SScriptFunc::Crit_GetItemBySlot( Critter* cr, uchar slot )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->GetItemSlot( slot );
}

Item* FOServer::SScriptFunc::Crit_GetItemByPid( Critter* cr, hash proto_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->GetItemByPidInvPriority( proto_id );
}

CScriptArray* FOServer::SScriptFunc::Crit_GetItems( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return Script::CreateArrayRef( "Item[]", cr->GetInventory() );
}

CScriptArray* FOServer::SScriptFunc::Crit_GetItemsBySlot( Critter* cr, int slot )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    ItemVec items;
    cr->GetItemsSlot( slot, items );
    return Script::CreateArrayRef( "Item[]", items );
}

CScriptArray* FOServer::SScriptFunc::Crit_GetItemsPredicate( Critter* cr, asIScriptFunction* predicate )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    uint bind_id = Script::BindByFunc( predicate, true );
    RUNTIME_ASSERT( bind_id );

    ItemVec inv_items = cr->GetInventory();
    ItemVec items;
    items.reserve( inv_items.size() );
    for( Item* item : inv_items )
    {
        if( item->IsDestroyed )
            continue;

        Script::PrepareContext( bind_id, "Predicate" );
        Script::SetArgObject( item );
        if( !Script::RunPrepared() )
        {
            Script::PassException();
            return nullptr;
        }

        if( Script::GetReturnedBool() && !item->IsDestroyed )
            items.push_back( item );
    }
    return Script::CreateArrayRef( "Item[]", items );
}

void FOServer::SScriptFunc::Crit_ChangeItemSlot( Critter* cr, uint item_id, uchar slot )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !item_id )
        SCRIPT_ERROR_R( "Item id arg is zero." );
    Item* item = cr->GetItem( item_id, cr->IsPlayer() );
    if( !item )
        SCRIPT_ERROR_R( "Item not found." );

    // To slot arg is equal of current item slot
    if( item->GetCritSlot() == slot )
        return;

    if( !Critter::SlotEnabled[ slot ] )
        SCRIPT_ERROR_R( "Slot is not allowed." );

    if( !Script::RaiseInternalEvent( ServerFunctions.CritterCheckMoveItem, cr, item, slot ) )
        SCRIPT_ERROR_R( "Can't move item" );

    Item* item_swap = ( slot ? cr->GetItemSlot( slot ) : nullptr );
    uchar from_slot = item->GetCritSlot();

    item->SetCritSlot( slot );
    if( item_swap )
        item_swap->SetCritSlot( from_slot );

    cr->SendAA_MoveItem( item, ACTION_MOVE_ITEM, from_slot );

    if( item_swap )
        Script::RaiseInternalEvent( ServerFunctions.CritterMoveItem, cr, item_swap, slot );
    Script::RaiseInternalEvent( ServerFunctions.CritterMoveItem, cr, item, from_slot );
}

void FOServer::SScriptFunc::Crit_SetCond( Critter* cr, int cond )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    int prev_cond = cr->GetCond();
    if( prev_cond == cond )
        return;
    cr->SetCond( cond );

    if( cr->GetMapId() )
    {
        Map*   map = MapMngr.GetMap( cr->GetMapId() );
        RUNTIME_ASSERT( map );
        ushort hx = cr->GetHexX();
        ushort hy = cr->GetHexY();
        uint   multihex = cr->GetMultihex();
        bool   is_dead = ( cond == COND_DEAD );
        map->UnsetFlagCritter( hx, hy, multihex, !is_dead );
        map->SetFlagCritter( hx, hy, multihex, is_dead );
    }
}

void FOServer::SScriptFunc::Crit_CloseDialog( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    if( cr->IsPlayer() && ( (Client*) cr )->IsTalking() )
        ( (Client*) cr )->CloseTalk();
}

void FOServer::SScriptFunc::Crit_SendCombatResult( Critter* cr, CScriptArray* arr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( arr->GetSize() > GameOpt.FloodSize / sizeof( uint ) )
        SCRIPT_ERROR_R( "Elements count is greater than maximum." );
    if( !arr )
        SCRIPT_ERROR_R( "Array arg is null." );

    cr->Send_CombatResult( (uint*) arr->At( 0 ), arr->GetSize() );
}

void FOServer::SScriptFunc::Crit_Action( Critter* cr, int action, int action_ext, Item* item )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( item && item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );

    cr->SendAA_Action( action, action_ext, item );
}

void FOServer::SScriptFunc::Crit_Animate( Critter* cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( item && item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );

    cr->SendAA_Animate( anim1, anim2, item, clear_sequence, delay_play );
}

void FOServer::SScriptFunc::Crit_SetAnims( Critter* cr, int cond, uint anim1, uint anim2 )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    if( cond == 0 || cond == COND_LIFE )
    {
        cr->SetAnim1Life( anim1 );
        cr->SetAnim2Life( anim2 );
    }
    if( cond == 0 || cond == COND_KNOCKOUT )
    {
        cr->SetAnim1Knockout( anim1 );
        cr->SetAnim2Knockout( anim2 );
    }
    if( cond == 0 || cond == COND_DEAD )
    {
        cr->SetAnim1Dead( anim1 );
        cr->SetAnim2Dead( anim2 );
    }
    cr->SendAA_SetAnims( cond, anim1, anim2 );
}

void FOServer::SScriptFunc::Crit_PlaySound( Critter* cr, string sound_name, bool send_self )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    if( send_self )
        cr->Send_PlaySound( cr->GetId(), sound_name );

    for( auto it = cr->VisCr.begin(), end = cr->VisCr.end(); it != end; ++it )
    {
        Critter* cr_ = *it;
        cr_->Send_PlaySound( cr->GetId(), sound_name );
    }
}

bool FOServer::SScriptFunc::Crit_IsKnownLoc( Critter* cr, bool by_id, uint loc_num )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    if( by_id )
        return cr->CheckKnownLocById( loc_num );
    return cr->CheckKnownLocByPid( loc_num );
}

bool FOServer::SScriptFunc::Crit_SetKnownLoc( Critter* cr, bool by_id, uint loc_num )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    Location* loc = ( by_id ? MapMngr.GetLocation( loc_num ) : MapMngr.GetLocationByPid( loc_num, 0 ) );
    if( !loc )
        SCRIPT_ERROR_R0( "Location not found." );

    cr->AddKnownLoc( loc->GetId() );
    if( loc->IsNonEmptyAutomaps() )
        cr->Send_AutomapsInfo( nullptr, loc );
    if( !cr->GetMapId() )
        cr->Send_GlobalLocation( loc, true );

    int           zx = GM_ZONE( loc->GetWorldX() );
    int           zy = GM_ZONE( loc->GetWorldY() );
    CScriptArray* gmap_fog = cr->GetGlobalMapFog();
    if( gmap_fog->GetSize() != GM_ZONES_FOG_SIZE )
        gmap_fog->Resize( GM_ZONES_FOG_SIZE );
    TwoBitMask gmap_mask( GM__MAXZONEX, GM__MAXZONEY, (uchar*) gmap_fog->At( 0 ) );
    if( gmap_mask.Get2Bit( zx, zy ) == GM_FOG_FULL )
    {
        gmap_mask.Set2Bit( zx, zy, GM_FOG_HALF );
        cr->SetGlobalMapFog( gmap_fog );
        if( !cr->GetMapId() )
            cr->Send_GlobalMapFog( zx, zy, GM_FOG_HALF );
    }
    gmap_fog->Release();
    return true;
}

bool FOServer::SScriptFunc::Crit_UnsetKnownLoc( Critter* cr, bool by_id, uint loc_num )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    Location* loc = ( by_id ? MapMngr.GetLocation( loc_num ) : MapMngr.GetLocationByPid( loc_num, 0 ) );
    if( !loc )
        SCRIPT_ERROR_R0( "Location not found." );
    if( !cr->CheckKnownLocById( loc->GetId() ) )
        SCRIPT_ERROR_R0( "Player is not know this location." );

    cr->EraseKnownLoc( loc->GetId() );
    if( !cr->GetMapId() )
        cr->Send_GlobalLocation( loc, false );
    return true;
}

void FOServer::SScriptFunc::Crit_SetFog( Critter* cr, ushort zone_x, ushort zone_y, int fog )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( fog < GM_FOG_FULL || fog > GM_FOG_NONE )
        SCRIPT_ERROR_R( "Invalid fog arg." );
    if( zone_x >= GameOpt.GlobalMapWidth || zone_y >= GameOpt.GlobalMapHeight )
        return;

    CScriptArray* gmap_fog = cr->GetGlobalMapFog();
    if( gmap_fog->GetSize() != GM_ZONES_FOG_SIZE )
        gmap_fog->Resize( GM_ZONES_FOG_SIZE );
    TwoBitMask gmap_mask( GM__MAXZONEX, GM__MAXZONEY, (uchar*) gmap_fog->At( 0 ) );
    if( gmap_mask.Get2Bit( zone_x, zone_y ) != fog )
    {
        gmap_mask.Set2Bit( zone_x, zone_y, fog );
        cr->SetGlobalMapFog( gmap_fog );
        if( !cr->GetMapId() )
            cr->Send_GlobalMapFog( zone_x, zone_y, fog );
    }
    gmap_fog->Release();
}

int FOServer::SScriptFunc::Crit_GetFog( Critter* cr, ushort zone_x, ushort zone_y )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( zone_x >= GameOpt.GlobalMapWidth || zone_y >= GameOpt.GlobalMapHeight )
        return GM_FOG_FULL;

    CScriptArray* gmap_fog = cr->GetGlobalMapFog();
    if( gmap_fog->GetSize() != GM_ZONES_FOG_SIZE )
        gmap_fog->Resize( GM_ZONES_FOG_SIZE );
    TwoBitMask gmap_mask( GM__MAXZONEX, GM__MAXZONEY, (uchar*) gmap_fog->At( 0 ) );
    int        result = gmap_mask.Get2Bit( zone_x, zone_y );
    gmap_fog->Release();
    return result;
}

void FOServer::SScriptFunc::Cl_SendItems( Critter* cl, CScriptArray* items, int param )
{
    if( cl->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    // Critter is not player
    if( !cl->IsPlayer() )
        return;

    ( (Client*) cl )->Send_SomeItems( items, param );
}

void FOServer::SScriptFunc::Cl_Disconnect( Critter* cl )
{
    if( cl->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !cl->IsPlayer() )
        SCRIPT_ERROR_R( "Critter is not player." );

    Client* cl_ = (Client*) cl;
    if( cl_->IsOnline() )
        cl_->Disconnect();
}

bool FOServer::SScriptFunc::Cl_IsOnline( Critter* cl )
{
    if( cl->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cl->IsPlayer() )
        SCRIPT_ERROR_R0( "Critter is not player." );

    Client* cl_ = (Client*) cl;
    return cl_->IsOnline();
}

bool FOServer::SScriptFunc::Crit_SetScript( Critter* cr, asIScriptFunction* func )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    if( func )
    {
        if( !cr->SetScript( func, true ) )
            SCRIPT_ERROR_R0( "Script function not found." );
    }
    else
    {
        cr->SetScriptId( 0 );
    }
    return true;
}

bool FOServer::SScriptFunc::Crit_AddTimeEvent( Critter* cr, asIScriptFunction* func, uint duration, int identifier )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !func )
        SCRIPT_ERROR_R0( "Func is null." );

    hash func_num = Script::BindScriptFuncNumByFunc( func );
    if( !func_num )
        SCRIPT_ERROR_R0( "Function not found." );

    cr->AddCrTimeEvent( func_num, 0, duration, identifier );
    return true;
}

bool FOServer::SScriptFunc::Crit_AddTimeEventRate( Critter* cr, asIScriptFunction* func, uint duration, int identifier, uint rate )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !func )
        SCRIPT_ERROR_R0( "Func is null." );

    hash func_num = Script::BindScriptFuncNumByFunc( func );
    if( !func_num )
        SCRIPT_ERROR_R0( "Function not found." );

    cr->AddCrTimeEvent( func_num, rate, duration, identifier );
    return true;
}

uint FOServer::SScriptFunc::Crit_GetTimeEvents( Critter* cr, int identifier, CScriptArray* indexes, CScriptArray* durations, CScriptArray* rates )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CScriptArray* te_identifier = cr->GetTE_Identifier();
    UIntVec       te_vec;
    for( uint i = 0, j = te_identifier->GetSize(); i < j; i++ )
    {
        if( *(int*) te_identifier->At( i ) == identifier )
            te_vec.push_back( i );
    }
    te_identifier->Release();

    uint size = (uint) te_vec.size();
    if( !size || ( !indexes && !durations && !rates ) )
        return size;

    CScriptArray* te_next_time = nullptr;
    CScriptArray* te_rate = nullptr;

    uint          indexes_size = 0, durations_size = 0, rates_size = 0;
    if( indexes )
    {
        indexes_size = indexes->GetSize();
        indexes->Resize( indexes_size + size );
    }
    if( durations )
    {
        te_next_time = cr->GetTE_NextTime();
        RUNTIME_ASSERT( te_next_time->GetSize() == te_identifier->GetSize() );
        durations_size = durations->GetSize();
        durations->Resize( durations_size + size );
    }
    if( rates )
    {
        te_rate = cr->GetTE_Rate();
        RUNTIME_ASSERT( te_rate->GetSize() == te_identifier->GetSize() );
        rates_size = rates->GetSize();
        rates->Resize( rates_size + size );
    }

    for( uint i = 0; i < size; i++ )
    {
        if( indexes )
        {
            *(uint*) indexes->At( indexes_size + i ) = te_vec[ i ];
        }
        if( durations )
        {
            uint next_time = *(uint*) te_next_time->At( te_vec[ i ] );
            *(uint*) durations->At( durations_size + i ) = ( next_time > GameOpt.FullSecond ? next_time - GameOpt.FullSecond : 0 );
        }
        if( rates )
        {
            *(uint*) rates->At( rates_size + i ) = *(uint*) te_rate->At( te_vec[ i ] );
        }
    }

    if( te_next_time )
        te_next_time->Release();
    if( te_rate )
        te_rate->Release();

    return size;
}

uint FOServer::SScriptFunc::Crit_GetTimeEventsArr( Critter* cr, CScriptArray* find_identifiers, CScriptArray* identifiers, CScriptArray* indexes, CScriptArray* durations, CScriptArray* rates )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    IntVec find_vec;
    Script::AssignScriptArrayInVector( find_vec, find_identifiers );

    CScriptArray* te_identifier = cr->GetTE_Identifier();
    UIntVec       te_vec;
    for( uint i = 0, j = te_identifier->GetSize(); i < j; i++ )
    {
        if( std::find( find_vec.begin(), find_vec.end(), *(int*) te_identifier->At( i ) ) != find_vec.end() )
            te_vec.push_back( i );
    }

    uint size = (uint) te_vec.size();
    if( !size || ( !identifiers && !indexes && !durations && !rates ) )
    {
        te_identifier->Release();
        return size;
    }

    CScriptArray* te_next_time = nullptr;
    CScriptArray* te_rate = nullptr;

    uint          identifiers_size = 0, indexes_size = 0, durations_size = 0, rates_size = 0;
    if( identifiers )
    {
        identifiers_size = identifiers->GetSize();
        identifiers->Resize( identifiers_size + size );
    }
    if( indexes )
    {
        indexes_size = indexes->GetSize();
        indexes->Resize( indexes_size + size );
    }
    if( durations )
    {
        te_next_time = cr->GetTE_NextTime();
        RUNTIME_ASSERT( te_next_time->GetSize() == te_identifier->GetSize() );
        durations_size = durations->GetSize();
        durations->Resize( durations_size + size );
    }
    if( rates )
    {
        te_rate = cr->GetTE_Rate();
        RUNTIME_ASSERT( te_rate->GetSize() == te_identifier->GetSize() );
        rates_size = rates->GetSize();
        rates->Resize( rates_size + size );
    }

    for( uint i = 0; i < size; i++ )
    {
        if( identifiers )
        {
            *(int*) identifiers->At( identifiers_size + i ) = *(uint*) te_identifier->At( te_vec[ i ] );
        }
        if( indexes )
        {
            *(uint*) indexes->At( indexes_size + i ) = te_vec[ i ];
        }
        if( durations )
        {
            uint next_time = *(uint*) te_next_time->At( te_vec[ i ] );
            *(uint*) durations->At( durations_size + i ) = ( next_time > GameOpt.FullSecond ? next_time - GameOpt.FullSecond : 0 );
        }
        if( rates )
        {
            *(uint*) rates->At( rates_size + i ) = *(uint*) te_rate->At( te_vec[ i ] );
        }
    }

    te_identifier->Release();
    if( te_next_time )
        te_next_time->Release();
    if( te_rate )
        te_rate->Release();

    return size;
}

void FOServer::SScriptFunc::Crit_ChangeTimeEvent( Critter* cr, uint index, uint new_duration, uint new_rate )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    CScriptArray* te_func_num = cr->GetTE_FuncNum();
    CScriptArray* te_identifier = cr->GetTE_Identifier();
    RUNTIME_ASSERT( te_func_num->GetSize() == te_identifier->GetSize() );
    if( index >= te_func_num->GetSize() )
    {
        te_func_num->Release();
        te_identifier->Release();
        SCRIPT_ERROR_R( "Index arg is greater than maximum time events." );
    }

    hash func_num = *(hash*) te_func_num->At( index );
    int  identifier = *(int*) te_identifier->At( index );
    te_func_num->Release();
    te_identifier->Release();

    cr->EraseCrTimeEvent( index );
    cr->AddCrTimeEvent( func_num, new_rate, new_duration, identifier );
}

void FOServer::SScriptFunc::Crit_EraseTimeEvent( Critter* cr, uint index )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    CScriptArray* te_func_num = cr->GetTE_FuncNum();
    uint          size = te_func_num->GetSize();
    te_func_num->Release();
    if( index >= size )
        SCRIPT_ERROR_R( "Index arg is greater than maximum time events." );

    cr->EraseCrTimeEvent( index );
}

uint FOServer::SScriptFunc::Crit_EraseTimeEvents( Critter* cr, int identifier )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CScriptArray* te_next_time = cr->GetTE_NextTime();
    CScriptArray* te_func_num = cr->GetTE_FuncNum();
    CScriptArray* te_rate = cr->GetTE_Rate();
    CScriptArray* te_identifier = cr->GetTE_Identifier();
    RUNTIME_ASSERT( te_next_time->GetSize() == te_func_num->GetSize() );
    RUNTIME_ASSERT( te_func_num->GetSize() == te_rate->GetSize() );
    RUNTIME_ASSERT( te_rate->GetSize() == te_identifier->GetSize() );

    uint result = 0;
    for( uint i = 0; i < te_identifier->GetSize();)
    {
        if( identifier == *(int*) te_identifier->At( i ) )
        {
            result++;
            te_next_time->RemoveAt( i );
            te_func_num->RemoveAt( i );
            te_rate->RemoveAt( i );
            te_identifier->RemoveAt( i );
        }
        else
        {
            i++;
        }
    }

    cr->SetTE_NextTime( te_next_time );
    cr->SetTE_FuncNum( te_func_num );
    cr->SetTE_Rate( te_rate );
    cr->SetTE_Identifier( te_identifier );

    te_next_time->Release();
    te_func_num->Release();
    te_rate->Release();
    te_identifier->Release();

    return result;
}

uint FOServer::SScriptFunc::Crit_EraseTimeEventsArr( Critter* cr, CScriptArray* identifiers )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    IntVec identifiers_;
    Script::AssignScriptArrayInVector( identifiers_, identifiers );

    CScriptArray* te_next_time = cr->GetTE_NextTime();
    CScriptArray* te_func_num = cr->GetTE_FuncNum();
    CScriptArray* te_rate = cr->GetTE_Rate();
    CScriptArray* te_identifier = cr->GetTE_Identifier();
    RUNTIME_ASSERT( te_next_time->GetSize() == te_func_num->GetSize() );
    RUNTIME_ASSERT( te_func_num->GetSize() == te_rate->GetSize() );
    RUNTIME_ASSERT( te_rate->GetSize() == te_identifier->GetSize() );

    uint result = 0;
    for( uint i = 0; i < te_func_num->GetSize();)
    {
        if( std::find( identifiers_.begin(), identifiers_.end(), *(int*) te_identifier->At( i ) ) != identifiers_.end() )
        {
            result++;
            te_next_time->RemoveAt( i );
            te_func_num->RemoveAt( i );
            te_rate->RemoveAt( i );
            te_identifier->RemoveAt( i );
        }
        else
        {
            i++;
        }
    }

    cr->SetTE_NextTime( te_next_time );
    cr->SetTE_FuncNum( te_func_num );
    cr->SetTE_Rate( te_rate );
    cr->SetTE_Identifier( te_identifier );

    te_next_time->Release();
    te_func_num->Release();
    te_rate->Release();
    te_identifier->Release();

    return result;
}

void FOServer::SScriptFunc::Crit_MoveToCritter( Critter* cr, Critter* target, uint cut, bool is_run )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !target )
        SCRIPT_ERROR_R( "Critter arg is null." );
    if( target->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );

    memzero( &cr->Moving, sizeof( cr->Moving ) );
    cr->Moving.TargId = target->GetId();
    cr->Moving.HexX = target->GetHexX();
    cr->Moving.HexY = target->GetHexY();
    cr->Moving.Cut = cut;
    cr->Moving.IsRun = is_run;
}

void FOServer::SScriptFunc::Crit_MoveToHex( Critter* cr, ushort hx, ushort hy, uint cut, bool is_run )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    memzero( &cr->Moving, sizeof( cr->Moving ) );
    cr->Moving.HexX = hx;
    cr->Moving.HexY = hy;
    cr->Moving.Cut = cut;
    cr->Moving.IsRun = is_run;
}

int FOServer::SScriptFunc::Crit_GetMovingState( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return cr->Moving.State;
}

void FOServer::SScriptFunc::Crit_ResetMovingState( Critter* cr, uint& gag_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    gag_id = cr->Moving.GagEntityId;
    memzero( &cr->Moving, sizeof( cr->Moving ) );
    cr->Moving.State = 1;
}


Location* FOServer::SScriptFunc::Map_GetLocation( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return map->GetLocation();
}

bool FOServer::SScriptFunc::Map_SetScript( Map* map, asIScriptFunction* func )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    if( func )
    {
        if( !map->SetScript( func, true ) )
            SCRIPT_ERROR_R0( "Script function not found." );
    }
    else
    {
        map->SetScriptId( 0 );
    }
    return true;
}

Item* FOServer::SScriptFunc::Map_AddItem( Map* map, ushort hx, ushort hy, hash proto_id, uint count, CScriptDict* props )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    ProtoItem* proto = ProtoMngr.GetProtoItem( proto_id );
    if( !proto )
        SCRIPT_ERROR_R0( "Invalid proto '{}' arg.", _str().parseHash( proto_id ) );
    if( !map->IsPlaceForProtoItem( hx, hy, proto ) )
        SCRIPT_ERROR_R0( "No place for item." );

    if( !count )
        count = 1;
    if( props )
    {
        Properties props_( Item::PropertiesRegistrator );
        props_ = proto->Props;
        for( uint i = 0, j = props->GetSize(); i < j; i++ )
            if( !Properties::SetValueAsIntProps( &props_, *(int*) props->GetKey( i ), *(int*) props->GetValue( i ) ) )
                return nullptr;

        return CreateItemOnHex( map, hx, hy, proto_id, count, &props_, true );
    }
    return CreateItemOnHex( map, hx, hy, proto_id, count, nullptr, true );
}

CScriptArray* FOServer::SScriptFunc::Map_GetItems( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    ItemVec items;
    map->GetItemsPid( 0, items );
    return Script::CreateArrayRef( "Item[]", items );
}

CScriptArray* FOServer::SScriptFunc::Map_GetItemsHex( Map* map, ushort hx, ushort hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    ItemVec items;
    map->GetItemsHex( hx, hy, items );
    return Script::CreateArrayRef( "Item[]", items );
}

CScriptArray* FOServer::SScriptFunc::Map_GetItemsHexEx( Map* map, ushort hx, ushort hy, uint radius, hash pid )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    ItemVec items;
    map->GetItemsHexEx( hx, hy, radius, pid, items );
    return Script::CreateArrayRef( "Item[]", items );
}

CScriptArray* FOServer::SScriptFunc::Map_GetItemsByPid( Map* map, hash pid )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    ItemVec items;
    map->GetItemsPid( pid, items );
    return Script::CreateArrayRef( "Item[]", items );
}

CScriptArray* FOServer::SScriptFunc::Map_GetItemsPredicate( Map* map, asIScriptFunction* predicate )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    uint bind_id = Script::BindByFunc( predicate, true );
    RUNTIME_ASSERT( bind_id );

    ItemVec map_items = map->GetItems();
    ItemVec items;
    items.reserve( map_items.size() );
    for( Item* item : map_items )
    {
        if( item->IsDestroyed )
            continue;

        Script::PrepareContext( bind_id, "Predicate" );
        Script::SetArgObject( item );
        if( !Script::RunPrepared() )
        {
            Script::PassException();
            return nullptr;
        }

        if( Script::GetReturnedBool() && !item->IsDestroyed )
            items.push_back( item );
    }
    return Script::CreateArrayRef( "Item[]", items );
}

CScriptArray* FOServer::SScriptFunc::Map_GetItemsHexPredicate( Map* map, ushort hx, ushort hy, asIScriptFunction* predicate )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    uint bind_id = Script::BindByFunc( predicate, true );
    RUNTIME_ASSERT( bind_id );

    ItemVec map_items;
    map->GetItemsHex( hx, hy, map_items );

    ItemVec items;
    items.reserve( map_items.size() );
    for( Item* item : map_items )
    {
        if( item->IsDestroyed )
            continue;

        Script::PrepareContext( bind_id, "Predicate" );
        Script::SetArgObject( item );
        if( !Script::RunPrepared() )
        {
            Script::PassException();
            return nullptr;
        }

        if( Script::GetReturnedBool() && !item->IsDestroyed )
            items.push_back( item );
    }
    return Script::CreateArrayRef( "Item[]", items );
}

CScriptArray* FOServer::SScriptFunc::Map_GetItemsHexRadiusPredicate( Map* map, ushort hx, ushort hy, uint radius, asIScriptFunction* predicate )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    uint bind_id = Script::BindByFunc( predicate, true );
    RUNTIME_ASSERT( bind_id );

    ItemVec map_items;
    map->GetItemsHexEx( hx, hy, radius, 0, map_items );

    ItemVec items;
    items.reserve( map_items.size() );
    for( Item* item : map_items )
    {
        if( item->IsDestroyed )
            continue;

        Script::PrepareContext( bind_id, "Predicate" );
        Script::SetArgObject( item );
        if( !Script::RunPrepared() )
        {
            Script::PassException();
            return nullptr;
        }

        if( Script::GetReturnedBool() && !item->IsDestroyed )
            items.push_back( item );
    }
    return Script::CreateArrayRef( "Item[]", items );
}

Item* FOServer::SScriptFunc::Map_GetItem( Map* map, uint item_id )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !item_id )
        SCRIPT_ERROR_R0( "Item id arg is zero." );

    return map->GetItem( item_id );
}

Item* FOServer::SScriptFunc::Map_GetItemHex( Map* map, ushort hx, ushort hy, hash pid )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    return map->GetItemHex( hx, hy, pid, nullptr );
}

Critter* FOServer::SScriptFunc::Map_GetCritterHex( Map* map, ushort hx, ushort hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    Critter* cr = map->GetHexCritter( hx, hy, false );
    if( !cr )
        cr = map->GetHexCritter( hx, hy, true );
    return cr;
}

Item* FOServer::SScriptFunc::Map_GetStaticItem( Map* map, ushort hx, ushort hy, hash pid )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    return map->GetProtoMap()->GetStaticItem( hx, hy, pid );
}

CScriptArray* FOServer::SScriptFunc::Map_GetStaticItemsHex( Map* map, ushort hx, ushort hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    ItemVec static_items;
    map->GetProtoMap()->GetStaticItemsHex( hx, hy, static_items );

    return Script::CreateArrayRef( "array<const Item>", static_items );
}

CScriptArray* FOServer::SScriptFunc::Map_GetStaticItemsHexEx( Map* map, ushort hx, ushort hy, uint radius, hash pid )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    ItemVec static_items;
    map->GetProtoMap()->GetStaticItemsHexEx( hx, hy, radius, pid, static_items );
    return Script::CreateArrayRef( "array<const Item>", static_items );
}

CScriptArray* FOServer::SScriptFunc::Map_GetStaticItemsByPid( Map* map, hash pid )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    ItemVec static_items;
    map->GetProtoMap()->GetStaticItemsByPid( pid, static_items );
    return Script::CreateArrayRef( "array<const Item>", static_items );
}

CScriptArray* FOServer::SScriptFunc::Map_GetStaticItemsPredicate( Map* map, asIScriptFunction* predicate )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    uint bind_id = Script::BindByFunc( predicate, true );
    RUNTIME_ASSERT( bind_id );

    ItemVec& map_static_items = map->GetProtoMap()->StaticItemsVec;
    ItemVec  items;
    items.reserve( map_static_items.size() );
    for( Item* item : map_static_items )
    {
        RUNTIME_ASSERT( !item->IsDestroyed );

        Script::PrepareContext( bind_id, "Predicate" );
        Script::SetArgObject( item );
        if( !Script::RunPrepared() )
        {
            Script::PassException();
            return nullptr;
        }

        if( Script::GetReturnedBool() )
            items.push_back( item );
    }
    return Script::CreateArrayRef( "array<const Item>", items );
}

CScriptArray* FOServer::SScriptFunc::Map_GetStaticItemsAll( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return Script::CreateArrayRef( "array<const Item>", map->GetProtoMap()->StaticItemsVec );
}

Critter* FOServer::SScriptFunc::Map_GetCritterById( Map* map, uint crid )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return map->GetCritter( crid );
}

CScriptArray* FOServer::SScriptFunc::Map_GetCritters( Map* map, ushort hx, ushort hy, uint radius, int find_type )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    CrVec critters;
    map->GetCrittersHex( hx, hy, radius, find_type, critters );
    SortCritterByDist( hx, hy, critters );
    return Script::CreateArrayRef( "Critter[]", critters );
}

CScriptArray* FOServer::SScriptFunc::Map_GetCrittersByPids( Map* map, hash pid, int find_type )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CrVec critters;
    if( !pid )
    {
        CrVec map_critters = map->GetCritters();
        critters.reserve( map_critters.size() );
        for( Critter* cr : map_critters )
            if( cr->CheckFind( find_type ) )
                critters.push_back( cr );
    }
    else
    {
        PcVec map_npcs = map->GetNpcs();
        critters.reserve( map_npcs.size() );
        for( Npc* npc : map_npcs )
            if( npc->GetProtoId() == pid && npc->CheckFind( find_type ) )
                critters.push_back( npc );
    }

    return Script::CreateArrayRef( "Critter[]", critters );
}

CScriptArray* FOServer::SScriptFunc::Map_GetCrittersInPath( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CrVec     critters;
    TraceData trace;
    trace.TraceMap = map;
    trace.BeginHx = from_hx;
    trace.BeginHy = from_hy;
    trace.EndHx = to_hx;
    trace.EndHy = to_hy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.Critters = &critters;
    trace.FindType = find_type;
    MapMngr.TraceBullet( trace );

    return Script::CreateArrayRef( "Critter[]", critters );
}

CScriptArray* FOServer::SScriptFunc::Map_GetCrittersInPathBlock( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type, ushort& pre_block_hx, ushort& pre_block_hy, ushort& block_hx, ushort& block_hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CrVec      critters;
    UShortPair block, pre_block;
    TraceData  trace;
    trace.TraceMap = map;
    trace.BeginHx = from_hx;
    trace.BeginHy = from_hy;
    trace.EndHx = to_hx;
    trace.EndHy = to_hy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.Critters = &critters;
    trace.FindType = find_type;
    trace.PreBlock = &pre_block;
    trace.Block = &block;
    MapMngr.TraceBullet( trace );

    pre_block_hx = pre_block.first;
    pre_block_hy = pre_block.second;
    block_hx = block.first;
    block_hy = block.second;
    return Script::CreateArrayRef( "Critter[]", critters );
}

CScriptArray* FOServer::SScriptFunc::Map_GetCrittersWhoViewPath( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, int find_type )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CrVec critters;
    for( Critter* cr : map->GetCritters() )
    {
        if( cr->CheckFind( find_type ) &&
            std::find( critters.begin(), critters.end(), cr ) == critters.end() &&
            IntersectCircleLine( cr->GetHexX(), cr->GetHexY(), cr->GetLookDistance(), from_hx, from_hy, to_hx, to_hy ) )
            critters.push_back( cr );
    }

    return Script::CreateArrayRef( "Critter[]", critters );
}

CScriptArray* FOServer::SScriptFunc::Map_GetCrittersSeeing( Map* map, CScriptArray* critters, bool look_on_them, int find_type )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !critters )
        SCRIPT_ERROR_R0( "Critters arg is null." );

    CrVec result_critters;
    for( int i = 0, j = critters->GetSize(); i < j; i++ )
    {
        Critter* cr = *(Critter**) critters->At( i );
        cr->GetCrFromVisCr( result_critters, find_type, !look_on_them );
    }

    return Script::CreateArrayRef( "Critter[]", result_critters );
}

void FOServer::SScriptFunc::Map_GetHexInPath( Map* map, ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    UShortPair pre_block, block;
    TraceData  trace;
    trace.TraceMap = map;
    trace.BeginHx = from_hx;
    trace.BeginHy = from_hy;
    trace.EndHx = to_hx;
    trace.EndHy = to_hy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.PreBlock = &pre_block;
    trace.Block = &block;
    MapMngr.TraceBullet( trace );
    to_hx = pre_block.first;
    to_hy = pre_block.second;
}

void FOServer::SScriptFunc::Map_GetHexInPathWall( Map* map, ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    UShortPair last_passed;
    TraceData  trace;
    trace.TraceMap = map;
    trace.BeginHx = from_hx;
    trace.BeginHy = from_hy;
    trace.EndHx = to_hx;
    trace.EndHy = to_hy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.LastPassed = &last_passed;
    MapMngr.TraceBullet( trace );
    if( trace.IsHaveLastPassed )
    {
        to_hx = last_passed.first;
        to_hy = last_passed.second;
    }
    else
    {
        to_hx = from_hx;
        to_hy = from_hy;
    }
}

uint FOServer::SScriptFunc::Map_GetPathLengthHex( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( from_hx >= map->GetWidth() || from_hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid from hexes args." );
    if( to_hx >= map->GetWidth() || to_hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid to hexes args." );

    PathFindData pfd;
    pfd.Clear();
    pfd.MapId = map->GetId();
    pfd.FromX = from_hx;
    pfd.FromY = from_hy;
    pfd.ToX = to_hx;
    pfd.ToY = to_hy;
    pfd.Cut = cut;
    uint result = MapMngr.FindPath( pfd );
    if( result != FPATH_OK )
        return 0;
    PathStepVec& path = MapMngr.GetPath( pfd.PathNum );
    return (uint) path.size();
}

uint FOServer::SScriptFunc::Map_GetPathLengthCr( Map* map, Critter* cr, ushort to_hx, ushort to_hy, uint cut )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cr )
        SCRIPT_ERROR_R0( "Critter arg is null." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter arg is destroyed." );
    if( to_hx >= map->GetWidth() || to_hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid to hexes args." );

    PathFindData pfd;
    pfd.Clear();
    pfd.MapId = map->GetId();
    pfd.FromCritter = cr;
    pfd.FromX = cr->GetHexX();
    pfd.FromY = cr->GetHexY();
    pfd.ToX = to_hx;
    pfd.ToY = to_hy;
    pfd.Multihex = cr->GetMultihex();
    pfd.Cut = cut;
    uint result = MapMngr.FindPath( pfd );
    if( result != FPATH_OK )
        return 0;
    PathStepVec& path = MapMngr.GetPath( pfd.PathNum );
    return (uint) path.size();
}

Critter* FOServer::SScriptFunc::Map_AddNpc( Map* map, hash proto_id, ushort hx, ushort hy, uchar dir, CScriptDict* props )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    ProtoCritter* proto = ProtoMngr.GetProtoCritter( proto_id );
    if( !proto )
        SCRIPT_ERROR_R0( "Proto '{}' not found.", _str().parseHash( proto_id ) );

    Critter* npc = nullptr;
    if( props )
    {
        Properties props_( Critter::PropertiesRegistrator );
        props_ = proto->Props;
        for( uint i = 0, j = props->GetSize(); i < j; i++ )
            if( !Properties::SetValueAsIntProps( &props_, *(int*) props->GetKey( i ), *(int*) props->GetValue( i ) ) )
                return nullptr;

        npc = CrMngr.CreateNpc( proto_id, &props_, map, hx, hy, dir, false );
    }
    else
    {
        npc = CrMngr.CreateNpc( proto_id, nullptr, map, hx, hy, dir, false );
    }

    if( !npc )
        SCRIPT_ERROR_R0( "Create npc fail." );
    return npc;
}

uint FOServer::SScriptFunc::Map_GetNpcCount( Map* map, int npc_role, int find_type )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return map->GetNpcCount( npc_role, find_type );
}

Critter* FOServer::SScriptFunc::Map_GetNpc( Map* map, int npc_role, int find_type, uint skip_count )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return map->GetNpc( npc_role, find_type, skip_count );
}

bool FOServer::SScriptFunc::Map_IsHexPassed( Map* map, ushort hex_x, ushort hex_y )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hex_x >= map->GetWidth() || hex_y >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    return map->IsHexPassed( hex_x, hex_y );
}

bool FOServer::SScriptFunc::Map_IsHexesPassed( Map* map, ushort hex_x, ushort hex_y, uint radius )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hex_x >= map->GetWidth() || hex_y >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    return map->IsHexesPassed( hex_x, hex_y, radius );
}

bool FOServer::SScriptFunc::Map_IsHexRaked( Map* map, ushort hex_x, ushort hex_y )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hex_x >= map->GetWidth() || hex_y >= map->GetHeight() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    return map->IsHexRaked( hex_x, hex_y );
}

void FOServer::SScriptFunc::Map_SetText( Map* map, ushort hex_x, ushort hex_y, uint color, string text )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hex_x >= map->GetWidth() || hex_y >= map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid hexes args." );
    map->SetText( hex_x, hex_y, color, text, false );
}

void FOServer::SScriptFunc::Map_SetTextMsg( Map* map, ushort hex_x, ushort hex_y, uint color, ushort text_msg, uint str_num )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hex_x >= map->GetWidth() || hex_y >= map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid hexes args." );

    map->SetTextMsg( hex_x, hex_y, color, text_msg, str_num );
}

void FOServer::SScriptFunc::Map_SetTextMsgLex( Map* map, ushort hex_x, ushort hex_y, uint color, ushort text_msg, uint str_num, string lexems )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hex_x >= map->GetWidth() || hex_y >= map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid hexes args." );

    map->SetTextMsgLex( hex_x, hex_y, color, text_msg, str_num, lexems.c_str(), (ushort) lexems.length() );
}

void FOServer::SScriptFunc::Map_RunEffect( Map* map, hash eff_pid, ushort hx, ushort hy, uint radius )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !eff_pid )
        SCRIPT_ERROR_R( "Effect pid invalid arg." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid hexes args." );

    map->SendEffect( eff_pid, hx, hy, radius );
}

void FOServer::SScriptFunc::Map_RunFlyEffect( Map* map, hash eff_pid, Critter* from_cr, Critter* to_cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !eff_pid )
        SCRIPT_ERROR_R( "Effect pid invalid arg." );
    if( from_hx >= map->GetWidth() || from_hy >= map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid from hexes args." );
    if( to_hx >= map->GetWidth() || to_hy >= map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid to hexes args." );
    if( from_cr && from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter is destroyed." );
    if( to_cr && to_cr->IsDestroyed )
        SCRIPT_ERROR_R( "To critter is destroyed." );

    uint from_crid = ( from_cr ? from_cr->GetId() : 0 );
    uint to_crid = ( to_cr ? to_cr->GetId() : 0 );
    map->SendFlyEffect( eff_pid, from_crid, to_crid, from_hx, from_hy, to_hx, to_hy );
}

bool FOServer::SScriptFunc::Map_CheckPlaceForItem( Map* map, ushort hx, ushort hy, hash pid )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ProtoItem* proto_item = ProtoMngr.GetProtoItem( pid );
    if( !proto_item )
        SCRIPT_ERROR_R0( "Proto item not found." );

    return map->IsPlaceForProtoItem( hx, hy, proto_item );
}

void FOServer::SScriptFunc::Map_BlockHex( Map* map, ushort hx, ushort hy, bool full )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid hexes args." );

    map->SetHexFlag( hx, hy, FH_BLOCK_ITEM );
    if( full )
        map->SetHexFlag( hx, hy, FH_NRAKE_ITEM );
}

void FOServer::SScriptFunc::Map_UnblockHex( Map* map, ushort hx, ushort hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid hexes args." );

    map->UnsetHexFlag( hx, hy, FH_BLOCK_ITEM );
    map->UnsetHexFlag( hx, hy, FH_NRAKE_ITEM );
}

void FOServer::SScriptFunc::Map_PlaySound( Map* map, string sound_name )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    for( Critter* cr : map->GetPlayers() )
        cr->Send_PlaySound( 0, sound_name );
}

void FOServer::SScriptFunc::Map_PlaySoundRadius( Map* map, string sound_name, ushort hx, ushort hy, uint radius )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid hexes args." );

    for( Critter* cr : map->GetPlayers() )
        if( CheckDist( hx, hy, cr->GetHexX(), cr->GetHexY(), radius == 0 ? cr->LookCacheValue : radius ) )
            cr->Send_PlaySound( 0, sound_name );
}

bool FOServer::SScriptFunc::Map_Reload( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !RegenerateMap( map ) )
        SCRIPT_ERROR_R0( "Reload map fail." );

    return true;
}

void FOServer::SScriptFunc::Map_MoveHexByDir( Map* map, ushort& hx, ushort& hy, uchar dir, uint steps )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( dir >= DIRS_COUNT )
        SCRIPT_ERROR_R( "Invalid dir arg." );
    if( !steps )
        SCRIPT_ERROR_R( "Steps arg is zero." );

    ushort maxhx = map->GetWidth();
    ushort maxhy = map->GetHeight();
    if( steps > 1 )
    {
        for( uint i = 0; i < steps; i++ )
            MoveHexByDir( hx, hy, dir, maxhx, maxhy );
    }
    else
    {
        MoveHexByDir( hx, hy, dir, maxhx, maxhy );
    }
}

void FOServer::SScriptFunc::Map_VerifyTrigger( Map* map, Critter* cr, ushort hx, ushort hy, uchar dir )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );
    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid hexes args." );
    if( dir >= DIRS_COUNT )
        SCRIPT_ERROR_R( "Invalid dir arg." );

    ushort from_hx = hx, from_hy = hy;
    MoveHexByDir( from_hx, from_hy, ReverseDir( dir ), map->GetWidth(), map->GetHeight() );
    VerifyTrigger( map, cr, from_hx, from_hy, hx, hy, dir );
}

uint FOServer::SScriptFunc::Location_GetMapCount( Location* loc )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return loc->GetMapsCount();
}

Map* FOServer::SScriptFunc::Location_GetMap( Location* loc, hash map_pid )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    for( Map* map : loc->GetMaps() )
        if( map->GetProtoId() == map_pid )
            return map;
    return nullptr;
}

Map* FOServer::SScriptFunc::Location_GetMapByIndex( Location* loc, uint index )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    MapVec maps = loc->GetMaps();
    if( index >= maps.size() )
        SCRIPT_ERROR_R0( "Invalid index arg." );

    return maps[ index ];
}

CScriptArray* FOServer::SScriptFunc::Location_GetMaps( Location* loc )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    return Script::CreateArrayRef( "Map[]", loc->GetMaps() );
}

bool FOServer::SScriptFunc::Location_GetEntrance( Location* loc, uint entrance, uint& map_index, hash& entire )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CScriptArray* map_entrances = loc->GetMapEntrances();
    uint          count = map_entrances->GetSize() / 2;
    if( entrance >= count )
    {
        map_entrances->Release();
        SCRIPT_ERROR_R0( "Invalid entrance." );
    }
    hash entrance_map = *(hash*) map_entrances->At( entrance * 2 );
    hash entrance_entire = *(hash*) map_entrances->At( entrance * 2 + 1 );
    map_entrances->Release();

    map_index = loc->GetMapIndex( entrance_map );
    entire = entrance_entire;
    return true;
}

uint FOServer::SScriptFunc::Location_GetEntrances( Location* loc, CScriptArray* maps_index, CScriptArray* entires )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CScriptArray* map_entrances = loc->GetMapEntrances();
    uint          count = map_entrances->GetSize() / 2;

    if( maps_index || entires )
    {
        for( uint e = 0; e < count; e++ )
        {
            if( maps_index )
            {
                uint index = loc->GetMapIndex( *(hash*) map_entrances->At( e * 2 ) );
                maps_index->InsertLast( &index );
            }
            if( entires )
            {
                hash entire = *(hash*) map_entrances->At( e * 2 + 1 );
                entires->InsertLast( &entire );
            }
        }
    }

    map_entrances->Release();
    return count;
}

bool FOServer::SScriptFunc::Location_Reload( Location* loc )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    for( Map* map : loc->GetMaps() )
        if( !RegenerateMap( map ) )
            SCRIPT_ERROR_R0( "Reload map in location fail." );
    return true;
}

uint FOServer::SScriptFunc::Global_GetCrittersDistantion( Critter* cr1, Critter* cr2 )
{
    if( !cr1 )
        SCRIPT_ERROR_R0( "Critter1 arg is null." );
    if( cr1->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter1 arg is destroyed." );
    if( !cr2 )
        SCRIPT_ERROR_R0( "Critter2 arg is null." );
    if( cr2->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter2 arg is destroyed." );
    if( cr1->GetMapId() != cr2->GetMapId() )
        SCRIPT_ERROR_R0( "Differernt maps." );

    return DistGame( cr1->GetHexX(), cr1->GetHexY(), cr2->GetHexX(), cr2->GetHexY() );
}

Item* FOServer::SScriptFunc::Global_GetItem( uint item_id )
{
    if( !item_id )
        SCRIPT_ERROR_R0( "Item id arg is zero." );

    Item* item = ItemMngr.GetItem( item_id );
    if( !item || item->IsDestroyed )
        return nullptr;
    return item;
}

void FOServer::SScriptFunc::Global_MoveItemCr( Item* item, uint count, Critter* to_cr, bool skip_checks )
{
    if( !item )
        SCRIPT_ERROR_R( "Item arg is null." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    if( !to_cr )
        SCRIPT_ERROR_R( "Critter arg is null." );
    if( to_cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );

    if( !count )
        count = item->GetCount();
    if( count > item->GetCount() )
        SCRIPT_ERROR_R( "Count arg is greater than maximum." );

    ItemMngr.MoveItem( item, count, to_cr, skip_checks );
}

void FOServer::SScriptFunc::Global_MoveItemMap( Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy, bool skip_checks )
{
    if( !item )
        SCRIPT_ERROR_R( "Item arg is null." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    if( !to_map )
        SCRIPT_ERROR_R( "Map arg is null." );
    if( to_map->IsDestroyed )
        SCRIPT_ERROR_R( "Map arg is destroyed." );
    if( to_hx >= to_map->GetWidth() || to_hy >= to_map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid hexex args." );

    if( !count )
        count = item->GetCount();
    if( count > item->GetCount() )
        SCRIPT_ERROR_R( "Count arg is greater than maximum." );

    ItemMngr.MoveItem( item, count, to_map, to_hx, to_hy, skip_checks );
}

void FOServer::SScriptFunc::Global_MoveItemCont( Item* item, uint count, Item* to_cont, uint stack_id, bool skip_checks )
{
    if( !item )
        SCRIPT_ERROR_R( "Item arg is null." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    if( !to_cont )
        SCRIPT_ERROR_R( "Container arg is null." );
    if( to_cont->IsDestroyed )
        SCRIPT_ERROR_R( "Container arg is destroyed." );

    if( !count )
        count = item->GetCount();
    if( count > item->GetCount() )
        SCRIPT_ERROR_R( "Count arg is greater than maximum." );

    ItemMngr.MoveItem( item, count, to_cont, stack_id, skip_checks );
}

void FOServer::SScriptFunc::Global_MoveItemsCr( CScriptArray* items, Critter* to_cr, bool skip_checks )
{
    if( !items )
        SCRIPT_ERROR_R( "Items arg is null." );
    if( !to_cr )
        SCRIPT_ERROR_R( "Critter arg is null." );
    if( to_cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );

    for( int i = 0, j = items->GetSize(); i < j; i++ )
    {
        Item* item = *(Item**) items->At( i );
        if( !item || item->IsDestroyed )
            continue;

        ItemMngr.MoveItem( item, item->GetCount(), to_cr, skip_checks );
    }
}

void FOServer::SScriptFunc::Global_MoveItemsMap( CScriptArray* items, Map* to_map, ushort to_hx, ushort to_hy, bool skip_checks )
{
    if( !items )
        SCRIPT_ERROR_R( "Items arg is null." );
    if( !to_map )
        SCRIPT_ERROR_R( "Map arg is null." );
    if( to_map->IsDestroyed )
        SCRIPT_ERROR_R( "Map arg is destroyed." );
    if( to_hx >= to_map->GetWidth() || to_hy >= to_map->GetHeight() )
        SCRIPT_ERROR_R( "Invalid hexex args." );

    for( int i = 0, j = items->GetSize(); i < j; i++ )
    {
        Item* item = *(Item**) items->At( i );
        if( !item || item->IsDestroyed )
            continue;

        ItemMngr.MoveItem( item, item->GetCount(), to_map, to_hx, to_hy, skip_checks );
    }
}

void FOServer::SScriptFunc::Global_MoveItemsCont( CScriptArray* items, Item* to_cont, uint stack_id, bool skip_checks )
{
    if( !items )
        SCRIPT_ERROR_R( "Items arg is null." );
    if( !to_cont )
        SCRIPT_ERROR_R( "Container arg is null." );
    if( to_cont->IsDestroyed )
        SCRIPT_ERROR_R( "Container arg is destroyed." );

    for( int i = 0, j = items->GetSize(); i < j; i++ )
    {
        Item* item = *(Item**) items->At( i );
        if( !item || item->IsDestroyed )
            continue;

        ItemMngr.MoveItem( item, item->GetCount(), to_cont, stack_id, skip_checks );
    }
}

void FOServer::SScriptFunc::Global_DeleteItem( Item* item )
{
    if( item )
        ItemMngr.DeleteItem( item );
}

void FOServer::SScriptFunc::Global_DeleteItemById( uint item_id )
{
    Item* item = ItemMngr.GetItem( item_id );
    if( item )
        ItemMngr.DeleteItem( item );
}

void FOServer::SScriptFunc::Global_DeleteItems( CScriptArray* items )
{
    for( int i = 0, j = items->GetSize(); i < j; i++ )
    {
        Item* item = *(Item**) items->At( i );
        if( item )
            ItemMngr.DeleteItem( item );
    }
}

void FOServer::SScriptFunc::Global_DeleteItemsById( CScriptArray* items )
{
    if( !items )
        SCRIPT_ERROR_R( "Items arg is null." );

    ItemVec items_to_delete;
    for( int i = 0, j = items->GetSize(); i < j; i++ )
    {
        uint item_id = *(uint*) items->At( i );
        if( item_id )
        {
            Item* item = ItemMngr.GetItem( item_id );
            if( item )
                ItemMngr.DeleteItem( item );
        }
    }
}

void FOServer::SScriptFunc::Global_DeleteNpc( Critter* npc )
{
    if( npc )
        CrMngr.DeleteNpc( npc );
}

void FOServer::SScriptFunc::Global_DeleteNpcById( uint npc_id )
{
    Critter* npc = CrMngr.GetNpc( npc_id );
    if( npc )
        CrMngr.DeleteNpc( npc );
}

void FOServer::SScriptFunc::Global_RadioMessage( ushort channel, string text )
{
    if( !text.empty() )
        ItemMngr.RadioSendTextEx( channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, text, false, 0, 0, nullptr );
}

void FOServer::SScriptFunc::Global_RadioMessageMsg( ushort channel, ushort text_msg, uint num_str )
{
    ItemMngr.RadioSendTextEx( channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, "", false, text_msg, num_str, nullptr );
}

void FOServer::SScriptFunc::Global_RadioMessageMsgLex( ushort channel, ushort text_msg, uint num_str, string lexems )
{
    ItemMngr.RadioSendTextEx( channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, "", false, text_msg, num_str, !lexems.empty() ? lexems.c_str() : nullptr );
}

uint FOServer::SScriptFunc::Global_GetFullSecond( ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second )
{
    if( !year )
        year = Globals->GetYear();
    else
        year = CLAMP( year, Globals->GetYearStart(), Globals->GetYearStart() + 130 );
    if( !month )
        month = Globals->GetMonth();
    else
        month = CLAMP( month, 1, 12 );
    if( !day )
    {
        day = Globals->GetDay();
    }
    else
    {
        uint month_day = Timer::GameTimeMonthDay( year, month );
        day = CLAMP( day, 1, month_day );
    }
    if( hour > 23 )
        hour = 23;
    if( minute > 59 )
        minute = 59;
    if( second > 59 )
        second = 59;
    return Timer::GetFullSecond( year, month, day, hour, minute, second );
}

void FOServer::SScriptFunc::Global_GetGameTime( uint full_second, ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour, ushort& minute, ushort& second )
{
    DateTimeStamp dt = Timer::GetGameTime( full_second );
    year = dt.Year;
    month = dt.Month;
    day_of_week = dt.DayOfWeek;
    day = dt.Day;
    hour = dt.Hour;
    minute = dt.Minute;
    second = dt.Second;
}

Location* FOServer::SScriptFunc::Global_CreateLocation( hash loc_pid, ushort wx, ushort wy, CScriptArray* critters )
{
    // Create and generate location
    Location* loc = MapMngr.CreateLocation( loc_pid, wx, wy );
    if( !loc )
        SCRIPT_ERROR_R0( "Unable to create location '{}'.", _str().parseHash( loc_pid ) );

    // Add known locations to critters
    if( critters )
    {
        for( uint i = 0, j = critters->GetSize(); i < j; i++ )
        {
            Critter* cr = *(Critter**) critters->At( i );

            cr->AddKnownLoc( loc->GetId() );
            if( !cr->GetMapId() )
                cr->Send_GlobalLocation( loc, true );
            if( loc->IsNonEmptyAutomaps() )
                cr->Send_AutomapsInfo( nullptr, loc );

            ushort        zx = GM_ZONE( loc->GetWorldX() );
            ushort        zy = GM_ZONE( loc->GetWorldY() );
            CScriptArray* gmap_fog = cr->GetGlobalMapFog();
            if( gmap_fog->GetSize() != GM_ZONES_FOG_SIZE )
                gmap_fog->Resize( GM_ZONES_FOG_SIZE );
            TwoBitMask gmap_mask( GM__MAXZONEX, GM__MAXZONEY, (uchar*) gmap_fog->At( 0 ) );
            if( gmap_mask.Get2Bit( zx, zy ) == GM_FOG_FULL )
            {
                gmap_mask.Set2Bit( zx, zy, GM_FOG_HALF );
                cr->SetGlobalMapFog( gmap_fog );
                if( !cr->GetMapId() )
                    cr->Send_GlobalMapFog( zx, zy, GM_FOG_HALF );
            }
            gmap_fog->Release();
        }
    }
    return loc;
}

void FOServer::SScriptFunc::Global_DeleteLocation( Location* loc )
{
    if( loc )
        MapMngr.DeleteLocation( loc, nullptr );
}

void FOServer::SScriptFunc::Global_DeleteLocationById( uint loc_id )
{
    Location* loc = MapMngr.GetLocation( loc_id );
    if( loc )
        MapMngr.DeleteLocation( loc, nullptr );
}

Critter* FOServer::SScriptFunc::Global_GetCritter( uint crid )
{
    if( !crid )
        return nullptr;          // SCRIPT_ERROR_R0("Critter id arg is zero.");
    return CrMngr.GetCritter( crid );
}

Critter* FOServer::SScriptFunc::Global_GetPlayer( string name )
{
    // Check existence
    uint               id = MAKE_CLIENT_ID( name );
    DataBase::Document doc = DbStorage->Get( "Players", id );
    if( doc.empty() )
        return nullptr;

    // Find online
    Client* cl = CrMngr.GetPlayer( id );
    if( cl )
    {
        cl->AddRef();
        return cl;
    }

    // Load from db
    ProtoCritter* cl_proto = ProtoMngr.GetProtoCritter( _str( "Player" ).toHash() );
    RUNTIME_ASSERT( cl_proto );
    cl = new Client( nullptr, cl_proto );
    cl->SetId( id );
    cl->Name = name;
    if( !cl->Props.LoadFromDbDocument( doc ) )
        SCRIPT_ERROR_R0( "Client data db read failed." );
    return cl;
}

CScriptArray* FOServer::SScriptFunc::Global_GetGlobalMapCritters( ushort wx, ushort wy, uint radius, int find_type )
{
    CrVec critters;
    CrMngr.GetGlobalMapCritters( wx, wy, radius, find_type, critters );
    return Script::CreateArrayRef( "Critter[]", critters );
}

Map* FOServer::SScriptFunc::Global_GetMap( uint map_id )
{
    if( !map_id )
        SCRIPT_ERROR_R0( "Map id arg is zero." );

    return MapMngr.GetMap( map_id );
}

Map* FOServer::SScriptFunc::Global_GetMapByPid( hash map_pid, uint skip_count )
{
    if( !map_pid )
        SCRIPT_ERROR_R0( "Invalid zero map proto id arg." );

    return MapMngr.GetMapByPid( map_pid, skip_count );
}

Location* FOServer::SScriptFunc::Global_GetLocation( uint loc_id )
{
    if( !loc_id )
        SCRIPT_ERROR_R0( "Location id arg is zero." );

    return MapMngr.GetLocation( loc_id );
}

Location* FOServer::SScriptFunc::Global_GetLocationByPid( hash loc_pid, uint skip_count )
{
    if( !loc_pid )
        SCRIPT_ERROR_R0( "Invalid zero location proto id arg." );

    return MapMngr.GetLocationByPid( loc_pid, skip_count );
}

CScriptArray* FOServer::SScriptFunc::Global_GetLocations( ushort wx, ushort wy, uint radius )
{
    LocVec locations;
    LocVec all_locations;
    MapMngr.GetLocations( all_locations );
    for( auto it = all_locations.begin(), end = all_locations.end(); it != end; ++it )
    {
        Location* loc = *it;
        if( DistSqrt( wx, wy, loc->GetWorldX(), loc->GetWorldY() ) <= radius + loc->GetRadius() )
            locations.push_back( loc );
    }

    return Script::CreateArrayRef( "Location[]", locations );
}

CScriptArray* FOServer::SScriptFunc::Global_GetVisibleLocations( ushort wx, ushort wy, uint radius, Critter* cr )
{
    LocVec locations;
    LocVec all_locations;
    MapMngr.GetLocations( all_locations );
    for( auto it = all_locations.begin(), end = all_locations.end(); it != end; ++it )
    {
        Location* loc = *it;
        if( DistSqrt( wx, wy, loc->GetWorldX(), loc->GetWorldY() ) <= radius + loc->GetRadius() &&
            ( loc->IsLocVisible() || ( cr && cr->IsPlayer() && ( (Client*) cr )->CheckKnownLocById( loc->GetId() ) ) ) )
            locations.push_back( loc );
    }

    return Script::CreateArrayRef( "Location[]", locations );
}

CScriptArray* FOServer::SScriptFunc::Global_GetZoneLocationIds( ushort zx, ushort zy, uint zone_radius )
{
    UIntVec loc_ids;
    MapMngr.GetZoneLocations( zx, zy, zone_radius, loc_ids );

    CScriptArray* ids = Script::CreateArray( "uint[]" );
    Script::AppendVectorToArray< uint >( loc_ids, ids );
    return ids;
}

bool FOServer::SScriptFunc::Global_RunDialogNpc( Critter* player, Critter* npc, bool ignore_distance )
{
    if( !player )
        SCRIPT_ERROR_R0( "Player arg is null." );
    if( player->IsDestroyed )
        SCRIPT_ERROR_R0( "Player arg is destroyed." );
    if( !player->IsPlayer() )
        SCRIPT_ERROR_R0( "Player arg is not player." );
    if( !npc )
        SCRIPT_ERROR_R0( "Npc arg is null." );
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Npc arg is destroyed." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Npc arg is not npc." );
    Client* cl = (Client*) player;
    if( cl->Talk.Locked )
        SCRIPT_ERROR_R0( "Can't open new dialog from demand, result or dialog functions." );

    Dialog_Begin( cl, (Npc*) npc, 0, 0, 0, ignore_distance );
    return cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == npc->GetId();
}

bool FOServer::SScriptFunc::Global_RunDialogNpcDlgPack( Critter* player, Critter* npc, uint dlg_pack, bool ignore_distance )
{
    if( !player )
        SCRIPT_ERROR_R0( "Player arg is null." );
    if( player->IsDestroyed )
        SCRIPT_ERROR_R0( "Player arg is destroyed." );
    if( !player->IsPlayer() )
        SCRIPT_ERROR_R0( "Player arg is not player." );
    if( !npc )
        SCRIPT_ERROR_R0( "Npc arg is null." );
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Npc arg is destroyed." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Npc arg is not npc." );
    Client* cl = (Client*) player;
    if( cl->Talk.Locked )
        SCRIPT_ERROR_R0( "Can't open new dialog from demand, result or dialog functions." );

    Dialog_Begin( cl, (Npc*) npc, dlg_pack, 0, 0, ignore_distance );
    return cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == npc->GetId();
}

bool FOServer::SScriptFunc::Global_RunDialogHex( Critter* player, uint dlg_pack, ushort hx, ushort hy, bool ignore_distance )
{
    if( !player )
        SCRIPT_ERROR_R0( "Player arg is null." );
    if( player->IsDestroyed )
        SCRIPT_ERROR_R0( "Player arg is destroyed." );
    if( !player->IsPlayer() )
        SCRIPT_ERROR_R0( "Player arg is not player." );
    if( !DlgMngr.GetDialog( dlg_pack ) )
        SCRIPT_ERROR_R0( "Dialog not found." );
    Client* cl = (Client*) player;
    if( cl->Talk.Locked )
        SCRIPT_ERROR_R0( "Can't open new dialog from demand, result or dialog functions." );

    Dialog_Begin( cl, nullptr, dlg_pack, hx, hy, ignore_distance );
    return cl->Talk.TalkType == TALK_WITH_HEX && cl->Talk.TalkHexX == hx && cl->Talk.TalkHexY == hy;
}

int64 FOServer::SScriptFunc::Global_WorldItemCount( hash pid )
{
    if( !ProtoMngr.GetProtoItem( pid ) )
        SCRIPT_ERROR_R0( "Invalid protoId arg." );

    return ItemMngr.GetItemStatistics( pid );
}

bool FOServer::SScriptFunc::Global_AddTextListener( int say_type, string first_str, uint parameter, asIScriptFunction* func )
{
    if( first_str.length() > TEXT_LISTEN_FIRST_STR_MAX_LEN )
        SCRIPT_ERROR_R0( "First string arg length greater than maximum." );

    uint func_id = Script::BindByFunc( func, false );
    if( !func_id )
        SCRIPT_ERROR_R0( "Unable to bind script function." );

    TextListen tl;
    tl.FuncId = func_id;
    tl.SayType = say_type;
    tl.FirstStr = first_str;
    tl.Parameter = parameter;

    SCOPE_LOCK( TextListenersLocker );

    TextListeners.push_back( tl );
    return true;
}

void FOServer::SScriptFunc::Global_EraseTextListener( int say_type, string first_str, uint parameter )
{
    SCOPE_LOCK( TextListenersLocker );

    for( auto it = TextListeners.begin(), end = TextListeners.end(); it != end; ++it )
    {
        TextListen& tl = *it;
        if( say_type == tl.SayType && _str( first_str ).compareIgnoreCaseUtf8( tl.FirstStr ) && tl.Parameter == parameter )
        {
            TextListeners.erase( it );
            return;
        }
    }
}

static void SwapCrittersRefreshNpc( Npc* npc )
{
    UNSETFLAG( npc->Flags, FCRIT_PLAYER );
    SETFLAG( npc->Flags, FCRIT_NPC );
}

static void SwapCrittersRefreshClient( Client* cl, Map* map, Map* prev_map )
{
    UNSETFLAG( cl->Flags, FCRIT_NPC );
    SETFLAG( cl->Flags, FCRIT_PLAYER );

    if( cl->Talk.TalkType != TALK_NONE )
        cl->CloseTalk();

    if( map != prev_map )
    {
        cl->Send_LoadMap( nullptr );
    }
    else
    {
        cl->Send_AllProperties();
        cl->Send_AddAllItems();
        cl->Send_AllAutomapsInfo();
    }
}

bool FOServer::SScriptFunc::Global_SwapCritters( Critter* cr1, Critter* cr2, bool with_inventory )
{
    // Check
    if( !cr1 )
        SCRIPT_ERROR_R0( "Critter1 arg is null." );
    if( !cr2 )
        SCRIPT_ERROR_R0( "Critter2 arg is null." );
    if( cr1->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter1 is destroyed." );
    if( cr2->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter2 is destroyed." );
    if( cr1 == cr2 )
        SCRIPT_ERROR_R0( "Critter1 is equal to Critter2." );
    if( !cr1->GetMapId() )
        SCRIPT_ERROR_R0( "Critter1 is on global map." );
    if( !cr2->GetMapId() )
        SCRIPT_ERROR_R0( "Critter2 is on global map." );

    // Swap positions
    Map* map1 = MapMngr.GetMap( cr1->GetMapId() );
    if( !map1 )
        SCRIPT_ERROR_R0( "Map of Critter1 not found." );
    Map* map2 = MapMngr.GetMap( cr2->GetMapId() );
    if( !map2 )
        SCRIPT_ERROR_R0( "Map of Critter2 not found." );

    CrVec& cr_map1 = map1->GetCrittersRaw();
    ClVec& cl_map1 = map1->GetPlayersRaw();
    PcVec& npc_map1 = map1->GetNpcsRaw();
    auto   it_cr = std::find( cr_map1.begin(), cr_map1.end(), cr1 );
    if( it_cr != cr_map1.end() )
        cr_map1.erase( it_cr );
    auto it_cl = std::find( cl_map1.begin(), cl_map1.end(), (Client*) cr1 );
    if( it_cl != cl_map1.end() )
        cl_map1.erase( it_cl );
    auto it_pc = std::find( npc_map1.begin(), npc_map1.end(), (Npc*) cr1 );
    if( it_pc != npc_map1.end() )
        npc_map1.erase( it_pc );

    CrVec& cr_map2 = map2->GetCrittersRaw();
    ClVec& cl_map2 = map2->GetPlayersRaw();
    PcVec& npc_map2 = map2->GetNpcsRaw();
    it_cr = std::find( cr_map2.begin(), cr_map2.end(), cr1 );
    if( it_cr != cr_map2.end() )
        cr_map2.erase( it_cr );
    it_cl = std::find( cl_map2.begin(), cl_map2.end(), (Client*) cr1 );
    if( it_cl != cl_map2.end() )
        cl_map2.erase( it_cl );
    it_pc = std::find( npc_map2.begin(), npc_map2.end(), (Npc*) cr1 );
    if( it_pc != npc_map2.end() )
        npc_map2.erase( it_pc );

    cr_map2.push_back( cr1 );
    if( cr1->IsNpc() )
        npc_map2.push_back( (Npc*) cr1 );
    else
        cl_map2.push_back( (Client*) cr1 );
    cr_map1.push_back( cr2 );
    if( cr2->IsNpc() )
        npc_map1.push_back( (Npc*) cr2 );
    else
        cl_map1.push_back( (Client*) cr2 );

    // Swap data
    std::swap( cr1->Props, cr2->Props );
    std::swap( cr1->Flags, cr2->Flags );
    cr1->SetBreakTime( 0 );
    cr2->SetBreakTime( 0 );

    // Swap inventory
    if( with_inventory )
    {
        ItemVec items1 = cr1->GetInventory();
        ItemVec items2 = cr2->GetInventory();
        for( auto it = items1.begin(), end = items1.end(); it != end; ++it )
            cr1->EraseItem( *it, false );
        for( auto it = items2.begin(), end = items2.end(); it != end; ++it )
            cr2->EraseItem( *it, false );
        for( auto it = items1.begin(), end = items1.end(); it != end; ++it )
            cr2->AddItem( *it, false );
        for( auto it = items2.begin(), end = items2.end(); it != end; ++it )
            cr1->AddItem( *it, false );
    }

    // Refresh
    cr1->ClearVisible();
    cr2->ClearVisible();

    if( cr1->IsNpc() )
        SwapCrittersRefreshNpc( (Npc*) cr1 );
    else
        SwapCrittersRefreshClient( (Client*) cr1, map2, map1 );
    if( cr2->IsNpc() )
        SwapCrittersRefreshNpc( (Npc*) cr2 );
    else
        SwapCrittersRefreshClient( (Client*) cr2, map1, map2 );
    if( map1 == map2 )
    {
        cr1->Send_CustomCommand( cr1, OTHER_CLEAR_MAP, 0 );
        cr2->Send_CustomCommand( cr2, OTHER_CLEAR_MAP, 0 );
        cr1->Send_Dir( cr1 );
        cr2->Send_Dir( cr2 );
        cr1->Send_CustomCommand( cr1, OTHER_TELEPORT, ( cr1->GetHexX() << 16 ) | ( cr1->GetHexY() ) );
        cr2->Send_CustomCommand( cr2, OTHER_TELEPORT, ( cr2->GetHexX() << 16 ) | ( cr2->GetHexY() ) );
        cr1->ProcessVisibleCritters();
        cr2->ProcessVisibleCritters();
        cr1->ProcessVisibleItems();
        cr2->ProcessVisibleItems();
    }
    return true;
}

CScriptArray* FOServer::SScriptFunc::Global_GetAllItems( hash pid )
{
    ItemVec items;
    ItemVec all_items;
    ItemMngr.GetGameItems( all_items );
    for( auto it = all_items.begin(), end = all_items.end(); it != end; ++it )
    {
        Item* item = *it;
        if( !item->IsDestroyed && ( !pid || pid == item->GetProtoId() ) )
            items.push_back( item );
    }

    return Script::CreateArrayRef( "Item[]", items );
}

CScriptArray* FOServer::SScriptFunc::Global_GetOnlinePlayers()
{
    CrVec players;
    ClVec all_players;
    CrMngr.GetClients( all_players );
    for( auto it = all_players.begin(), end = all_players.end(); it != end; ++it )
    {
        Critter* player_ = *it;
        if( !player_->IsDestroyed && player_->IsPlayer() )
            players.push_back( player_ );
    }

    return Script::CreateArrayRef( "Critter[]", players );
}

CScriptArray* FOServer::SScriptFunc::Global_GetRegisteredPlayerIds()
{
    UIntVec       ids = DbStorage->GetAllIds( "Players" );
    CScriptArray* result = Script::CreateArray( "uint[]" );
    Script::AppendVectorToArray< uint >( ids, result );
    return result;
}

CScriptArray* FOServer::SScriptFunc::Global_GetAllNpc( hash pid )
{
    CrVec npcs;
    PcVec all_npcs;
    CrMngr.GetNpcs( all_npcs );
    for( auto it = all_npcs.begin(), end = all_npcs.end(); it != end; ++it )
    {
        Npc* npc_ = *it;
        if( !npc_->IsDestroyed && ( !pid || pid == npc_->GetProtoId() ) )
            npcs.push_back( npc_ );
    }

    return Script::CreateArrayRef( "Critter[]", npcs );
}

CScriptArray* FOServer::SScriptFunc::Global_GetAllMaps( hash pid )
{
    MapVec maps;
    MapVec all_maps;
    MapMngr.GetMaps( all_maps );
    for( auto it = all_maps.begin(), end = all_maps.end(); it != end; ++it )
    {
        Map* map = *it;
        if( !pid || pid == map->GetProtoId() )
            maps.push_back( map );
    }

    return Script::CreateArrayRef( "Map[]", maps );
}

CScriptArray* FOServer::SScriptFunc::Global_GetAllLocations( hash pid )
{
    LocVec locations;
    LocVec all_locations;
    MapMngr.GetLocations( all_locations );
    for( auto it = all_locations.begin(), end = all_locations.end(); it != end; ++it )
    {
        Location* loc = *it;
        if( !pid || pid == loc->GetProtoId() )
            locations.push_back( loc );
    }

    return Script::CreateArrayRef( "Location[]", locations );
}

void FOServer::SScriptFunc::Global_GetTime( ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour, ushort& minute, ushort& second, ushort& milliseconds )
{
    DateTimeStamp cur_time;
    Timer::GetCurrentDateTime( cur_time );
    year = cur_time.Year;
    month = cur_time.Month;
    day_of_week = cur_time.DayOfWeek;
    day = cur_time.Day;
    hour = cur_time.Hour;
    minute = cur_time.Minute;
    second = cur_time.Second;
    milliseconds = cur_time.Milliseconds;
}

void FOServer::SScriptFunc::Global_SetTime( ushort multiplier, ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second )
{
    SetGameTime( multiplier, year, month, day, hour, minute, second );
}

void FOServer::SScriptFunc::Global_SetPropertyGetCallback( asIScriptGeneric* gen )
{
    int   prop_enum_value = gen->GetArgDWord( 0 );
    void* ref = gen->GetArgAddress( 1 );
    gen->SetReturnByte( 0 );
    RUNTIME_ASSERT( ref );

    Property* prop = GlobalVars::PropertiesRegistrator->FindByEnum( prop_enum_value );
    prop = ( prop ? prop : Critter::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : Item::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : Map::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : Location::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : GlobalVars::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    if( !prop )
        SCRIPT_ERROR_R( "Property '{}' not found.", _str().parseHash( prop_enum_value ) );

    string result = prop->SetGetCallback( *(asIScriptFunction**) ref );
    if( result != "" )
        SCRIPT_ERROR_R( result );

    gen->SetReturnByte( 1 );
}

void FOServer::SScriptFunc::Global_AddPropertySetCallback( asIScriptGeneric* gen )
{
    int   prop_enum_value = gen->GetArgDWord( 0 );
    void* ref = gen->GetArgAddress( 1 );
    bool  deferred = gen->GetArgByte( 2 ) != 0;
    gen->SetReturnByte( 0 );
    RUNTIME_ASSERT( ref );

    Property* prop = Critter::PropertiesRegistrator->FindByEnum( prop_enum_value );
    prop = ( prop ? prop : Item::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : Map::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : Location::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : GlobalVars::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    if( !prop )
        SCRIPT_ERROR_R( "Property '{}' not found.", _str().parseHash( prop_enum_value ) );

    string result = prop->AddSetCallback( *(asIScriptFunction**) ref, deferred );
    if( result != "" )
        SCRIPT_ERROR_R( result );

    gen->SetReturnByte( 1 );
}

void FOServer::SScriptFunc::Global_AllowSlot( uchar index, bool enable_send )
{
    Critter::SlotEnabled[ index ] = true;
    Critter::SlotDataSendEnabled[ index ] = enable_send;
}

bool FOServer::SScriptFunc::Global_LoadDataFile( string dat_name )
{
    return FileManager::LoadDataFile( dat_name );
}

struct ServerImage
{
    UCharVec Data;
    uint     Width;
    uint     Height;
    uint     Depth;
};
static vector< ServerImage* > ServerImages;

bool FOServer::SScriptFunc::Global_LoadImage( uint index, string image_name, uint image_depth )
{
    // Delete old
    if( index >= ServerImages.size() )
        ServerImages.resize( index + 1 );
    if( ServerImages[ index ] )
    {
        MEMORY_PROCESS( MEMORY_IMAGE, -(int) ServerImages[ index ]->Data.capacity() );
        delete ServerImages[ index ];
        ServerImages[ index ] = nullptr;
    }
    if( image_name.empty() )
        return true;

    // Check depth
    static uint image_depth_;
    image_depth_ = image_depth; // Avoid GCC warning "argument 'image_depth' might be clobbered by 'longjmp' or 'vfork'"
    if( image_depth < 1 || image_depth > 4 )
        SCRIPT_ERROR_R0( "Wrong image depth arg." );

    // Check extension
    string ext = _str( image_name ).getFileExtension();
    if( ext != "png" )
        SCRIPT_ERROR_R0( "Wrong extension. Allowed only PNG." );

    // Load file to memory
    FilesCollection images( "png" );
    FileManager&    fm = images.FindFile( image_name.substr( 0, image_name.find_last_of( '.' ) ) );
    if( !fm.IsLoaded() )
        SCRIPT_ERROR_R0( "File '{}' not found.", image_name );

    // Load PNG from memory
    png_structp pp = png_create_read_struct( PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr );
    png_infop   info = nullptr;
    if( pp )
        info = png_create_info_struct( pp );
    if( !pp || !info )
    {
        if( pp )
            png_destroy_read_struct( &pp, nullptr, nullptr );
        SCRIPT_ERROR_R0( "Cannot allocate memory to read PNG data." );
    }

    if( setjmp( png_jmpbuf( pp ) ) )
    {
        png_destroy_read_struct( &pp, &info, nullptr );
        SCRIPT_ERROR_R0( "PNG data contains errors." );
    }

    struct png_mem_data_
    {
        png_structp          pp;
        const unsigned char* current;
        const unsigned char* last;

        static void          png_read_data_from_mem( png_structp png_ptr, png_bytep data, png_size_t length )
        {
            png_mem_data_* png_mem_data = (png_mem_data_*) png_get_io_ptr( png_ptr );
            if( png_mem_data->current + length > png_mem_data->last )
            {
                png_error( png_mem_data->pp, "Invalid attempt to read row data." );
                return;
            }
            memcpy( data, png_mem_data->current, length );
            png_mem_data->current += length;
        }
    } png_mem_data;
    png_mem_data.current = fm.GetBuf();
    png_mem_data.last = fm.GetBuf() + fm.GetFsize();
    png_mem_data.pp = pp;
    png_set_read_fn( pp, ( png_voidp ) & png_mem_data, png_mem_data.png_read_data_from_mem );

    png_read_info( pp, info );

    if( png_get_color_type( pp, info ) == PNG_COLOR_TYPE_PALETTE )
        png_set_expand( pp );

    int channels = 1;
    if( png_get_color_type( pp, info ) & PNG_COLOR_MASK_COLOR )
        channels = 3;

    int num_trans = 0;
    png_get_tRNS( pp, info, 0, &num_trans, 0 );
    if( ( png_get_color_type( pp, info ) & PNG_COLOR_MASK_ALPHA ) || ( num_trans != 0 ) )
        channels++;

    int w = (int) png_get_image_width( pp, info );
    int h = (int) png_get_image_height( pp, info );
    int d = channels;

    if( png_get_bit_depth( pp, info ) < 8 )
    {
        png_set_packing( pp );
        png_set_expand( pp );
    }
    else if( png_get_bit_depth( pp, info ) == 16 )
        png_set_strip_16( pp );

    if( png_get_valid( pp, info, PNG_INFO_tRNS ) )
        png_set_tRNS_to_alpha( pp );

    uchar*     data = new uchar[ w * h * d ];
    png_bytep* rows = new png_bytep[ h ];

    for( int i = 0; i < h; i++ )
        rows[ i ] = (png_bytep) ( data + i * w * d );

    for( int i = png_set_interlace_handling( pp ); i > 0; i-- )
        png_read_rows( pp, rows, nullptr, h );

    delete[] rows;
    png_read_end( pp, info );
    png_destroy_read_struct( &pp, &info, nullptr );

    // Copy data
    ServerImage* simg = new ServerImage();
    simg->Width = w;
    simg->Height = h;
    simg->Depth = image_depth_;
    simg->Data.resize( simg->Width * simg->Height * simg->Depth + 3 ); // +3 padding

    const uint argb_offs[ 4 ] = { 2, 1, 0, 3 };
    uint       min_depth = MIN( (uint) d, simg->Depth );
    uint       data_index = 0;
    uint       png_data_index = 0;
    for( uint y = 0; y < simg->Height; y++ )
    {
        for( uint x = 0; x < simg->Width; x++ )
        {
            memzero( &simg->Data[ data_index ], simg->Depth );
            for( uint j = 0; j < min_depth; j++ )
                simg->Data[ data_index + j ] = *( data + png_data_index + argb_offs[ j ] );
            png_data_index += d;
            data_index += simg->Depth;
        }
    }
    delete[] data;

    ServerImages[ index ] = simg;
    MEMORY_PROCESS( MEMORY_IMAGE, (int) ServerImages[ index ]->Data.capacity() );
    return true;
}

uint FOServer::SScriptFunc::Global_GetImageColor( uint index, uint x, uint y )
{
    if( index >= ServerImages.size() || !ServerImages[ index ] )
        SCRIPT_ERROR_R0( "Image not loaded." );
    ServerImage* simg = ServerImages[ index ];
    if( x >= simg->Width || y >= simg->Height )
        SCRIPT_ERROR_R0( "Invalid coords arg." );

    uint* data = (uint*) ( &simg->Data[ 0 ] + y * simg->Width * simg->Depth + x * simg->Depth );
    uint  result = *data;
    switch( simg->Depth )
    {
    case 1:
        result &= 0xFF;
        break;
    case 2:
        result &= 0xFFFF;
        break;
    case 3:
        result &= 0xFFFFFF;
        break;
    default:
        break;
    }
    return result;
}

static size_t WriteMemoryCallback( char* ptr, size_t size, size_t nmemb, void* userdata )
{
    string& result = *(string*) userdata;
    size_t  len = size * nmemb;
    if( len )
    {
        result.resize( result.size() + len );
        memcpy( &result[ result.size() - len ], ptr, len );
    }
    return len;
}

static void YieldWebRequest( const string& url, CScriptArray* headers, CScriptDict* post1, const string& post2, bool& success, string& result )
{
    success = false;
    result = "";

    asIScriptContext* ctx = Script::SuspendCurrentContext( uint( -1 ) );
    if( !ctx )
        return;

    struct RequestData
    {
        asIScriptContext* Context;
        Thread*           WorkThread;
        string            Url;
        CScriptArray*     Headers;
        CScriptDict*      Post1;
        string            Post2;
        bool*             Success;
        string*           Result;
    };

    RequestData* request_data = new RequestData();
    request_data->Context = ctx;
    request_data->WorkThread = new Thread();
    request_data->Url = url;
    request_data->Headers = headers;
    request_data->Post1 = post1;
    request_data->Post2 = post2;
    request_data->Success = &success;
    request_data->Result = &result;

    if( headers )
        headers->AddRef();
    if( post1 )
        post1->AddRef();

    auto request_func = [] (void* data)
    {
        static bool curl_inited = false;
        if( !curl_inited )
        {
            curl_inited = true;
            curl_global_init( CURL_GLOBAL_ALL );
        }

        RequestData* request_data = (RequestData*) data;

        bool         success = false;
        string       result;

        CURL*        curl = curl_easy_init();
        if( curl )
        {
            curl_easy_setopt( curl, CURLOPT_URL, request_data->Url.c_str() );
            curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback );
            curl_easy_setopt( curl, CURLOPT_WRITEDATA, &result );
            curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 0 );

            curl_slist* header_list = nullptr;
            if( request_data->Headers && request_data->Headers->GetSize() )
            {
                for( uint i = 0, j = request_data->Headers->GetSize(); i < j; i++ )
                    header_list = curl_slist_append( header_list, ( *(string*) request_data->Headers->At( i ) ).c_str() );
                curl_easy_setopt( curl, CURLOPT_HTTPHEADER, header_list );
            }

            string post;
            if( request_data->Post1 )
            {
                for( uint i = 0, j = request_data->Post1->GetSize(); i < j; i++ )
                {
                    string& key = *(string*) request_data->Post1->GetKey( i );
                    string& value = *(string*) request_data->Post1->GetValue( i );
                    char*   escaped_key = curl_easy_escape( curl, key.c_str(), (int) key.length() );
                    char*   escaped_value = curl_easy_escape( curl, value.c_str(), (int) value.length() );
                    if( i > 0 )
                        post += "&";
                    post += escaped_key;
                    post += "=";
                    post += escaped_value;
                    curl_free( escaped_key );
                    curl_free( escaped_value );
                }
                curl_easy_setopt( curl, CURLOPT_POSTFIELDS, post.c_str() );
            }
            else if( !request_data->Post2.empty() )
            {
                curl_easy_setopt( curl, CURLOPT_POSTFIELDS, request_data->Post2.c_str() );
            }

            CURLcode curl_res = curl_easy_perform( curl );
            if( curl_res == CURLE_OK )
            {
                success = true;
            }
            else
            {
                result = "curl_easy_perform() failed: ";
                result += curl_easy_strerror( curl_res );
            }

            curl_easy_cleanup( curl );
            if( header_list )
                curl_slist_free_all( header_list );
        }
        else
        {
            result = "curl_easy_init fail";
        }

        *request_data->Success = success;
        *request_data->Result = result;
        Script::ResumeContext( request_data->Context );
        delete request_data->WorkThread;
        if( request_data->Headers )
            request_data->Headers->Release();
        if( request_data->Post1 )
            request_data->Post1->Release();
        delete request_data;
    };
    request_data->WorkThread->Start( request_func, "WebRequest", request_data );
}

void FOServer::SScriptFunc::Global_YieldWebRequest( string url, CScriptDict* post, bool& success, string& result )
{
    YieldWebRequest( url, nullptr, post, "", success, result );
}

void FOServer::SScriptFunc::Global_YieldWebRequestExt( string url, CScriptArray* headers, string post, bool& success, string& result )
{
    YieldWebRequest( url, headers, nullptr, post, success, result );
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
