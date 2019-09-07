#include "Entity.h"
#include "Log.h"
#include "Testing.h"
#include "ProtoManager.h"
#include "Crypt.h"
#include "IniFile.h"
#include "Script.h"
#include "StringUtils.h"

#if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
# include "Map.h"
# include "Critter.h"
# include "Item.h"
#endif
#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
# include "MapView.h"
# include "CritterView.h"
# include "ItemView.h"
#endif

ProtoMap::ProtoMap( hash pid ): ProtoEntity( pid, EntityType::MapProto, ProtoMap::PropertiesRegistrator )
{
    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
    MEMORY_PROCESS( MEMORY_PROTO_MAP, sizeof( ProtoMap ) );
    #endif

    #if defined ( FONLINE_EDITOR )
    LastEntityId = 0;
    #endif
}

ProtoMap::~ProtoMap()
{
    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) sizeof( ProtoMap ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) SceneryData.capacity() );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) Tiles.capacity() * sizeof( Tile ) );

    SAFEDELA( HexFlags );

    SceneryData.clear();

    for( auto it = CrittersVec.begin(), end = CrittersVec.end(); it != end; ++it )
        SAFEREL( *it );
    CrittersVec.clear();
    for( auto it = HexItemsVec.begin(), end = HexItemsVec.end(); it != end; ++it )
        SAFEREL( *it );
    HexItemsVec.clear();
    for( auto it = ChildItemsVec.begin(), end = ChildItemsVec.end(); it != end; ++it )
        SAFEREL( *it );
    ChildItemsVec.clear();
    for( auto it = StaticItemsVec.begin(), end = StaticItemsVec.end(); it != end; ++it )
        SAFEREL( *it );
    StaticItemsVec.clear();
    for( auto it = TriggerItemsVec.begin(), end = TriggerItemsVec.end(); it != end; ++it )
        SAFEREL( *it );
    TriggerItemsVec.clear();
    #endif

    Tiles.clear();

    #if defined ( FONLINE_EDITOR )
    for( auto& entity : AllEntities )
        entity->Release();
    AllEntities.clear();
    #endif

    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
    # define CLEAN_CONTAINER( cont )    { decltype( cont ) __ ## cont; __ ## cont.swap( cont ); }
    CLEAN_CONTAINER( SceneryData );
    CLEAN_CONTAINER( CrittersVec );
    CLEAN_CONTAINER( HexItemsVec );
    CLEAN_CONTAINER( ChildItemsVec );
    CLEAN_CONTAINER( StaticItemsVec );
    CLEAN_CONTAINER( TriggerItemsVec );
    CLEAN_CONTAINER( Tiles );
    # undef CLEAN_CONTAINER
    #endif
}

template< class CritterType, EntityType CritterEntityType, class ItemType, EntityType ItemEntityType >
static void SaveTextFormat( ProtoMap& pmap, IniFile& file )
{
    // Header
    pmap.Props.SaveToText( file.SetApp( "ProtoMap" ), nullptr );

    #if defined ( FONLINE_EDITOR )
    // Critters
    for( auto& entity : pmap.AllEntities )
    {
        if( entity->Type == CritterEntityType )
        {
            StrMap& kv = file.SetApp( "Critter" );
            kv[ "$Id" ] = _str( "{}", entity->Id );
            kv[ "$Proto" ] = entity->Proto->GetName();
            entity->Props.SaveToText( kv, &entity->Proto->Props );
        }
    }

    // Items
    for( auto& entity : pmap.AllEntities )
    {
        if( entity->Type == ItemEntityType )
        {
            StrMap& kv = file.SetApp( "Item" );
            kv[ "$Id" ] = _str( "{}", entity->Id );
            kv[ "$Proto" ] = entity->Proto->GetName();
            entity->Props.SaveToText( kv, &entity->Proto->Props );
        }
    }
    #endif

    // Tiles
    for( auto& tile : pmap.Tiles )
    {
        StrMap& kv = file.SetApp( "Tile" );
        kv[ "PicMap" ] = _str().parseHash( tile.Name );
        kv[ "HexX" ] = _str( "{}", tile.HexX );
        kv[ "HexY" ] = _str( "{}", tile.HexY );
        if( tile.OffsX )
            kv[ "OffsetX" ] = _str( "{}", tile.OffsX );
        if( tile.OffsY )
            kv[ "OffsetY" ] = _str( "{}", tile.OffsY );
        if( tile.Layer )
            kv[ "Layer" ] = _str( "{}", tile.Layer );
        if( tile.IsRoof )
            kv[ "IsRoof" ] = _str( "{}", tile.IsRoof );
    }
}

#if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
template< class CritterType, EntityType CritterEntityType, class ItemType, EntityType ItemEntityType >
static bool BindScripts( ProtoMap& pmap, EntityVec& entities )
{
    int errors = 0;

    // Map script
    if( pmap.GetScriptId() )
    {
        hash func_num = Script::BindScriptFuncNumByFuncName( _str().parseHash( pmap.GetScriptId() ), "void %s(Map, bool)" );
        if( !func_num )
        {
            WriteLog( "Map '{}', can't bind map function '{}'.\n", pmap.GetName(), _str().parseHash( pmap.GetScriptId() ) );
            errors++;
        }
    }

    // Entities scripts
    for( auto& entity : entities )
    {
        if( entity->Type == CritterEntityType && ( (CritterType*) entity )->GetScriptId() )
        {
            string func_name = _str().parseHash( ( (CritterType*) entity )->GetScriptId() );
            hash   func_num = Script::BindScriptFuncNumByFuncName( func_name, "void %s(Critter, bool)" );
            if( !func_num )
            {
                WriteLog( "Map '{}', can't bind critter function '{}'.\n", pmap.GetName(), func_name );
                errors++;
            }
        }
        else if( entity->Type == ItemEntityType && !( (ItemType*) entity )->IsStatic() && ( (ItemType*) entity )->GetScriptId() )
        {
            string func_name = _str().parseHash( ( (ItemType*) entity )->GetScriptId() );
            hash   func_num = Script::BindScriptFuncNumByFuncName( func_name, "void %s(Item, bool)" );
            if( !func_num )
            {
                WriteLog( "Map '{}', can't bind item function '{}'.\n", pmap.GetName(), func_name );
                errors++;
            }
        }
        else if( entity->Type == ItemEntityType && ( (ItemType*) entity )->IsStatic() && ( (ItemType*) entity )->GetScriptId() )
        {
            Item*  item = (ItemType*) entity;
            string func_name = _str().parseHash( item->GetScriptId() );
            uint   bind_id = 0;
            if( item->GetIsTrigger() || item->GetIsTrap() )
                bind_id = Script::BindByFuncName( func_name, "void %s(Critter, const Item, bool, uint8)", false );
            else
                bind_id = Script::BindByFuncName( func_name, "bool %s(Critter, const Item, Item, int)", false );
            if( !bind_id )
            {
                WriteLog( "Map '{}', can't bind static item function '{}'.\n", pmap.GetName(), func_name );
                errors++;
            }
            item->SceneryScriptBindId = bind_id;
        }
    }
    return errors == 0;
}

template< class CritterType, EntityType CritterEntityType, class ItemType, EntityType ItemEntityType >
static bool OnAfterLoad( ProtoMap& pmap, EntityVec& entities )
{
    // Bind scripts
    if( !BindScripts< CritterType, CritterEntityType, ItemType, ItemEntityType >( pmap, entities ) )
        return false;

    // Parse objects
    ushort maxhx = pmap.GetWidth();
    ushort maxhy = pmap.GetHeight();
    pmap.HexFlags = new uchar[ maxhx * maxhy ];
    memzero( pmap.HexFlags, maxhx * maxhy );

    uint     scenery_count = 0;
    UCharVec scenery_data;
    for( auto& entity : entities )
    {
        if( entity->Type == CritterEntityType )
        {
            entity->AddRef();
            pmap.CrittersVec.push_back( (CritterType*) entity );
            continue;
        }

        RUNTIME_ASSERT( entity->Type == ItemEntityType );
        ItemType* item = (ItemType*) entity;
        hash      pid = item->GetProtoId();

        if( !item->IsStatic() )
        {
            entity->AddRef();
            if( item->GetAccessory() == ITEM_ACCESSORY_HEX )
                pmap.HexItemsVec.push_back( (ItemType*) entity );
            else
                pmap.ChildItemsVec.push_back( (ItemType*) entity );
            continue;
        }

        RUNTIME_ASSERT( item->GetAccessory() == ITEM_ACCESSORY_HEX );

        ushort hx = item->GetHexX();
        ushort hy = item->GetHexY();
        if( hx >= maxhx || hy >= maxhy )
        {
            WriteLog( "Invalid item '{}' position on map '{}', hex x {}, hex y {}.\n", item->GetName(), pmap.GetName(), hx, hy );
            continue;
        }

        if( !item->GetIsHiddenInStatic() )
        {
            item->AddRef();
            pmap.StaticItemsVec.push_back( item );
        }

        if( !item->GetIsNoBlock() )
            SETFLAG( pmap.HexFlags[ hy * maxhx + hx ], FH_BLOCK );

        if( !item->GetIsShootThru() )
        {
            SETFLAG( pmap.HexFlags[ hy * maxhx + hx ], FH_BLOCK );
            SETFLAG( pmap.HexFlags[ hy * maxhx + hx ], FH_NOTRAKE );
        }

        if( item->GetIsScrollBlock() )
        {
            // Block around
            for( int k = 0; k < 6; k++ )
            {
                ushort hx_ = hx, hy_ = hy;
                MoveHexByDir( hx_, hy_, k, maxhx, maxhy );
                SETFLAG( pmap.HexFlags[ hy_ * maxhx + hx_ ], FH_BLOCK );
            }
        }

        if( item->GetIsTrigger() || item->GetIsTrap() )
        {
            item->AddRef();
            pmap.TriggerItemsVec.push_back( item );

            SETFLAG( pmap.HexFlags[ hy * maxhx + hx ], FH_STATIC_TRIGGER );
        }

        if( item->IsNonEmptyBlockLines() )
        {
            ushort hx_ = hx, hy_ = hy;
            bool   raked = item->GetIsShootThru();
            FOREACH_PROTO_ITEM_LINES( item->GetBlockLines(), hx_, hy_, maxhx, maxhy,
                                      SETFLAG( pmap.HexFlags[ hy_ * maxhx + hx_ ], FH_BLOCK );
                                      if( !raked )
                                          SETFLAG( pmap.HexFlags[ hy_ * maxhx + hx_ ], FH_NOTRAKE );
                                      );
        }

        // Data for client
        if( !item->GetIsHidden() )
        {
            scenery_count++;
            WriteData( scenery_data, item->Id );
            WriteData( scenery_data, item->GetProtoId() );
            PUCharVec* all_data;
            UIntVec*   all_data_sizes;
            item->Props.StoreData( false, &all_data, &all_data_sizes );
            WriteData( scenery_data, (uint) all_data->size() );
            for( size_t i = 0; i < all_data->size(); i++ )
            {
                WriteData( scenery_data, all_data_sizes->at( i ) );
                WriteDataArr( scenery_data, all_data->at( i ), all_data_sizes->at( i ) );
            }
        }
    }

    pmap.SceneryData.clear();
    WriteData( pmap.SceneryData, scenery_count );
    if( !scenery_data.empty() )
        WriteDataArr( pmap.SceneryData, &scenery_data[ 0 ], (uint) scenery_data.size() );

    for( auto& entity : entities )
        entity->Release();
    entities.clear();

    // Generate hashes
    pmap.HashTiles = maxhx * maxhy;
    if( pmap.Tiles.size() )
        pmap.HashTiles = Crypt.MurmurHash2( (uchar*) &pmap.Tiles[ 0 ], (uint) pmap.Tiles.size() * sizeof( ProtoMap::Tile ) );
    pmap.HashScen = maxhx * maxhy;
    if( pmap.SceneryData.size() )
        pmap.HashScen = Crypt.MurmurHash2( (uchar*) &pmap.SceneryData[ 0 ], (uint) pmap.SceneryData.size() );

    // Shrink the vector capacities to fit their contents and reduce memory use
    UCharVec( pmap.SceneryData ).swap( pmap.SceneryData );
    CritterVec( pmap.CrittersVec ).swap( pmap.CrittersVec );
    ItemVec( pmap.HexItemsVec ).swap( pmap.HexItemsVec );
    ItemVec( pmap.ChildItemsVec ).swap( pmap.ChildItemsVec );
    ItemVec( pmap.StaticItemsVec ).swap( pmap.StaticItemsVec );
    ItemVec( pmap.TriggerItemsVec ).swap( pmap.TriggerItemsVec );
    ProtoMap::TileVec( pmap.Tiles ).swap( pmap.Tiles );

    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) pmap.SceneryData.capacity() );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) pmap.GetWidth() * pmap.GetHeight() );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) pmap.Tiles.capacity() * sizeof( ProtoMap::Tile ) );

    # if defined ( FONLINE_EDITOR )
    // Get lower id
    pmap.LastEntityId = 0;
    for( auto& entity : entities )
        if( !pmap.LastEntityId || entity->Id < pmap.LastEntityId )
            pmap.LastEntityId = entity->Id;

    pmap.AllEntities = std::move( entities );
    # endif

    return true;
}
#endif

template< class CritterType, EntityType CritterEntityType, class ItemType, EntityType ItemEntityType >
static bool LoadTextFormat( ProtoMap& pmap, const char* buf, EntityVec& entities )
{
    int errors = 0;

    // Header
    IniFile map_data;
    map_data.AppendStr( buf );
    if( !map_data.IsApp( "ProtoMap" ) )
    {
        WriteLog( "Invalid map format.\n" );
        return false;
    }
    pmap.Props.LoadFromText( map_data.GetApp( "ProtoMap" ) );

    // Critters
    PStrMapVec npc_data;
    map_data.GetApps( "Critter", npc_data );
    for( auto& pkv : npc_data )
    {
        auto& kv = *pkv;
        if( !kv.count( "$Id" ) || !kv.count( "$Proto" ) )
        {
            WriteLog( "Proto critter invalid data.\n" );
            errors++;
            continue;
        }

        uint          id = _str( kv[ "$Id" ] ).toUInt();
        hash          proto_id = _str( kv[ "$Proto" ] ).toHash();
        ProtoCritter* proto = ProtoMngr.GetProtoCritter( proto_id );
        if( !proto )
        {
            WriteLog( "Proto critter '{}' not found.\n", _str().parseHash( proto_id ) );
            errors++;
            continue;
        }

        CritterType* npc = new CritterType( id, proto );
        if( !npc->Props.LoadFromText( kv ) )
        {
            WriteLog( "Unable to load critter '{}' properties.\n", _str().parseHash( proto_id ) );
            errors++;
            continue;
        }

        entities.push_back( npc );
    }

    // Items
    PStrMapVec items_data;
    map_data.GetApps( "Item", items_data );
    for( auto& pkv : items_data )
    {
        auto& kv = *pkv;
        if( !kv.count( "$Id" ) || !kv.count( "$Proto" ) )
        {
            WriteLog( "Proto item invalid data.\n" );
            errors++;
            continue;
        }

        uint       id = _str( kv[ "$Id" ] ).toUInt();
        hash       proto_id = _str( kv[ "$Proto" ] ).toHash();
        ProtoItem* proto = ProtoMngr.GetProtoItem( proto_id );
        if( !proto )
        {
            WriteLog( "Proto item '{}' not found.\n", _str().parseHash( proto_id ) );
            errors++;
            continue;
        }

        ItemType* item = new ItemType( id, proto );
        if( !item->Props.LoadFromText( kv ) )
        {
            WriteLog( "Unable to load item '{}' properties.\n", _str().parseHash( proto_id ) );
            errors++;
            continue;
        }

        entities.push_back( item );
    }

    // Tiles
    PStrMapVec tiles_data;
    map_data.GetApps( "Tile", tiles_data );
    for( auto& pkv : tiles_data )
    {
        auto& kv = *pkv;
        if( !kv.count( "PicMap" ) || !kv.count( "HexX" ) || !kv.count( "HexY" ) )
        {
            WriteLog( "Tile invalid data.\n" );
            errors++;
            continue;
        }

        hash name = _str( kv[ "PicMap" ] ).toHash();
        int  hx = _str( kv[ "HexX" ] ).toUInt();
        int  hy = _str( kv[ "HexY" ] ).toUInt();
        int  ox = ( kv.count( "OffsetX" ) ? _str( kv[ "OffsetX" ] ).toUInt() : 0 );
        int  oy = ( kv.count( "OffsetY" ) ? _str( kv[ "OffsetY" ] ).toUInt() : 0 );
        int  layer = ( kv.count( "Layer" ) ? _str( kv[ "Layer" ] ).toUInt() : 0 );
        bool is_roof = ( kv.count( "IsRoof" ) ? _str( kv[ "IsRoof" ] ).toBool() : false );
        if( hx < 0 || hx >= pmap.GetWidth() || hy < 0 || hy > pmap.GetHeight() )
        {
            WriteLog( "Tile '{}' have wrong hex position {} {}.\n", _str().parseHash( name ), hx, hy );
            errors++;
            continue;
        }
        if( layer < 0 || layer > 255 )
        {
            WriteLog( "Tile '{}' have wrong layer value {}.\n", _str().parseHash( name ), layer );
            errors++;
            continue;
        }

        pmap.Tiles.push_back( ProtoMap::Tile( name, hx, hy, ox, oy, layer, is_roof ) );
    }

    return errors == 0;
}

template< class CritterType, EntityType CritterEntityType, class ItemType, EntityType ItemEntityType >
static bool LoadOldTextFormat( ProtoMap& pmap, const char* buf, EntityVec& entities )
{
    #define MAP_OBJECT_CRITTER                   ( 0 )
    #define MAP_OBJECT_ITEM                      ( 1 )
    #define MAP_OBJECT_SCENERY                   ( 2 )
    #define FO_MAP_VERSION_TEXT1                 ( 1 )
    #define FO_MAP_VERSION_TEXT2                 ( 2 )
    #define FO_MAP_VERSION_TEXT3                 ( 3 )
    #define FO_MAP_VERSION_TEXT4                 ( 4 )
    #define FO_MAP_VERSION_TEXT5                 ( 5 )
    #define FO_MAP_VERSION_TEXT6                 ( 6 )
    #define FO_MAP_VERSION_TEXT7                 ( 7 )

    IniFile map_ini;
    map_ini.CollectContent();
    map_ini.AppendStr( buf );

    // Header
    string header_str = map_ini.GetAppContent( "Header" );
    int    version = -1;
    if( !header_str.empty() )
    {
        istringstream istr( header_str );
        string        field, value;
        int           ivalue;
        string        script_name;
        IntVec        vec = { 300, 600, 1140, 1380 };
        UCharVec      vec2 = { 18, 128, 103, 51, 18, 128, 95, 40, 53, 128, 86, 29 };
        while( !istr.eof() && !istr.fail() )
        {
            istr >> field >> value;
            if( !istr.fail() )
            {
                ivalue = _str( value ).toInt();
                if( field == "Version" )
                {
                    version = ivalue;
                    uint old_version = ( ivalue << 20 );
                    #define FO_MAP_VERSION_V6    ( 0xFE000000 )
                    #define FO_MAP_VERSION_V7    ( 0xFF000000 )
                    #define FO_MAP_VERSION_V8    ( 0xFF100000 )
                    #define FO_MAP_VERSION_V9    ( 0xFF200000 )
                    if( old_version == FO_MAP_VERSION_V6 || old_version == FO_MAP_VERSION_V7 ||
                        old_version == FO_MAP_VERSION_V8 || old_version == FO_MAP_VERSION_V9 )
                        version = FO_MAP_VERSION_TEXT1;
                }
                else if( field == "MaxHexX" )
                    pmap.SetWidth( ivalue );
                else if( field == "MaxHexY" )
                    pmap.SetHeight( ivalue );
                else if( field == "WorkHexX" || field == "CenterX" )
                    pmap.SetWorkHexX( ivalue );
                else if( field == "WorkHexY" || field == "CenterY" )
                    pmap.SetWorkHexY( ivalue );
                else if( field == "Time" )
                    pmap.SetCurDayTime( ivalue );
                else if( field == "NoLogOut" )
                    pmap.SetIsNoLogOut( ivalue != 0 );
                else if( ( field == "ScriptModule" || field == "ScriptName" ) && value != "-" )
                    script_name += value;
                else if( field == "ScriptFunc" && value != "-" )
                    script_name.append( "@" ).append( value );
                else if( field == "DayTime" )
                {
                    vec[ 0 ] = ivalue;
                    istr >> vec[ 1 ];
                    istr >> vec[ 2 ];
                    istr >> vec[ 3 ];
                }
                else if( field == "DayColor0" )
                {
                    vec2[ 0 ] = ivalue;
                    istr >> ivalue;
                    vec2[ 4 ] = ivalue;
                    istr >> ivalue;
                    vec2[ 8 ] = ivalue;
                }
                else if( field == "DayColor1" )
                {
                    vec2[ 1 ] = ivalue;
                    istr >> ivalue;
                    vec2[ 5 ] = ivalue;
                    istr >> ivalue;
                    vec2[ 9 ] = ivalue;
                }
                else if( field == "DayColor2" )
                {
                    vec2[ 2 ] = ivalue;
                    istr >> ivalue;
                    vec2[ 6 ] = ivalue;
                    istr >> ivalue;
                    vec2[ 10 ] = ivalue;
                }
                else if( field == "DayColor3" )
                {
                    vec2[ 3 ] = ivalue;
                    istr >> ivalue;
                    vec2[ 7 ] = ivalue;
                    istr >> ivalue;
                    vec2[ 11 ] = ivalue;
                }
            }
        }
        if( !script_name.empty() )
            pmap.SetScriptId( _str( script_name ).toHash() );
        CScriptArray* arr = Script::CreateArray( "int[]" );
        Script::AppendVectorToArray( vec, arr );
        pmap.SetDayTime( arr );
        arr->Release();
        CScriptArray* arr2 = Script::CreateArray( "uint8[]" );
        Script::AppendVectorToArray( vec2, arr2 );
        pmap.SetDayColor( arr2 );
        arr2->Release();
    }
    if( ( version != FO_MAP_VERSION_TEXT1 && version != FO_MAP_VERSION_TEXT2 && version != FO_MAP_VERSION_TEXT3 &&
          version != FO_MAP_VERSION_TEXT4 && version != FO_MAP_VERSION_TEXT5 && version != FO_MAP_VERSION_TEXT6 &&
          version != FO_MAP_VERSION_TEXT7 ) ||
        pmap.GetWidth() < 1 || pmap.GetHeight() < 1 )
        return false;

    // Tiles
    string tiles_str = map_ini.GetAppContent( "Tiles" );
    if( !tiles_str.empty() )
    {
        istringstream istr( tiles_str );
        string     type;
        if( version == FO_MAP_VERSION_TEXT1 )
        {
            string name;

            // Deprecated
            while( !istr.eof() && !istr.fail() )
            {
                int hx, hy;
                istr >> type >> hx >> hy >> name;
                if( !istr.fail() && hx >= 0 && hx < pmap.GetWidth() && hy >= 0 && hy < pmap.GetHeight() )
                {
                    hx *= 2;
                    hy *= 2;
                    if( type == "tile" )
                        pmap.Tiles.push_back( ProtoMap::Tile( _str( name ).toHash(), hx, hy, 0, 0, 0, false ) );
                    else if( type == "roof" )
                        pmap.Tiles.push_back( ProtoMap::Tile( _str( name ).toHash(), hx, hy, 0, 0, 0, true ) );
                    else if( type == "0" )
                        pmap.Tiles.push_back( ProtoMap::Tile( _str( name ).toUInt(), hx, hy, 0, 0, 0, false ) );
                    else if( type == "1" )
                        pmap.Tiles.push_back( ProtoMap::Tile( _str( name ).toUInt(), hx, hy, 0, 0, 0, true ) );
                }
            }
        }
        else
        {
            int hx, hy;
            int    ox, oy, layer;
            bool   is_roof;
            size_t len;
            bool   has_offs;
            bool   has_layer;
            while( !istr.eof() && !istr.fail() )
            {
                istr >> type >> hx >> hy;
                if( !istr.fail() && hx >= 0 && hx < pmap.GetWidth() && hy >= 0 && hy < pmap.GetHeight() )
                {
                    if( !type.compare( 0, 4, "tile" ) )
                        is_roof = false;
                    else if( !type.compare( 0, 4, "roof" ) )
                        is_roof = true;
                    else
                        continue;

                    len = type.length();
                    has_offs = false;
                    has_layer = false;
                    if( len > 5 )
                    {
                        while( --len >= 5 )
                        {
                            switch( type[ len ] )
                            {
                            case 'o':
                                has_offs = true;
                                break;
                            case 'l':
                                has_layer = true;
                                break;
                            default:
                                break;
                            }
                        }
                    }

                    if( has_offs )
                        istr >> ox >> oy;
                    else
                        ox = oy = 0;
                    if( has_layer )
                        istr >> layer;
                    else
                        layer = 0;

                    string name;
                    std::getline( istr, name, '\n' );
                    name = _str( name ).trim();

                    pmap.Tiles.push_back( ProtoMap::Tile( _str( name ).toHash(), hx, hy, ox, oy, layer, is_roof ) );
                }
            }
        }
    }

    // Objects
    struct AdditionalFields
    {
        uint UID = 0;
        uint ContainerUID = 0;
        uint ParentUID = 0;
        string ScriptName;
        string FuncName;
    };
    vector< AdditionalFields > entities_addon;
    string objects_str = map_ini.GetAppContent( "Objects" );
    if( !objects_str.empty() )
    {
        bool       fail = false;
        istringstream istr( objects_str );
        int        is_critter = false;
        Property*  cur_prop = nullptr;
        uint       auto_id = uint( -1 );
        while( !istr.eof() && !istr.fail() )
        {
            string     field;
            istr >> field;

            string svalue;
            std::getline( istr, svalue, '\n' );
            svalue = _str( svalue ).trim();

            if( istr.fail() )
                continue;
            int ivalue = _str( svalue ).toInt();

            if( field == "MapObjType" )
            {
                if( ivalue == MAP_OBJECT_CRITTER )
                {
                    is_critter = true;
                }
                else
                {
                    RUNTIME_ASSERT( ( ivalue == MAP_OBJECT_ITEM || ivalue == MAP_OBJECT_SCENERY ) );
                    is_critter = false;
                }
            }
            else
            {
                if( field == "ProtoId" || field == "ProtoName" )
                {
                    hash proto_id = 0;
                    if( field == "ProtoName" )
                    {
                        proto_id = _str( svalue ).toHash();
                        if( is_critter && !ProtoMngr.GetProtoCritter( proto_id ) )
                        {
                            WriteLog( "Critter prototype '{}' not found.\n", svalue );
                            fail = true;
                        }
                        else if( !is_critter && !ProtoMngr.GetProtoItem( proto_id ) )
                        {
                            WriteLog( "Item prototype '{}' not found.\n", svalue );
                            fail = true;
                        }
                    }
                    if( fail )
                        break;

                    if( is_critter )
                    {
                        CritterType* cr = new CritterType( auto_id--, ProtoMngr.GetProtoCritter( proto_id ) );
                        cr->SetCond( COND_LIFE );
                        entities.push_back( cr );
                        entities_addon.push_back( AdditionalFields() );
                    }
                    else
                    {
                        ItemType* item = new ItemType( auto_id--, ProtoMngr.GetProtoItem( proto_id ) );
                        entities.push_back( item );
                        entities_addon.push_back( AdditionalFields() );
                    }

                    continue;
                }

                #define SET_FIELD_CRITTER( field_name, prop )                      \
                    if( field == field_name && is_critter )                        \
                    {                                                              \
                        ( (CritterType*) entities.back() )->Set ## prop( ivalue ); \
                        continue;                                                  \
                    }
                #define SET_FIELD_ITEM( field_name, prop )                      \
                    if( field == field_name && !is_critter )                    \
                    {                                                           \
                        ( (ItemType*) entities.back() )->Set ## prop( ivalue ); \
                        continue;                                               \
                    }
                SET_FIELD_CRITTER( "MapX", HexX );
                SET_FIELD_CRITTER( "MapY", HexY );
                SET_FIELD_ITEM( "MapX", HexX );
                SET_FIELD_ITEM( "MapY", HexY );
                if( field == "UID" )
                    entities_addon.back().UID = ivalue;
                else if( field == "ContainerUID" )
                    entities_addon.back().ContainerUID = ivalue;
                else if( field == "ParentUID" )
                    entities_addon.back().ParentUID = ivalue;
                SET_FIELD_ITEM( "LightColor", LightColor );
                if( field == "LightDay" )
                    ( (ItemType*) entities.back() )->SetLightFlags( ( (ItemType*) entities.back() )->GetLightFlags() | ( ( ivalue & 3 ) << 6 ) );
                if( field == "LightDirOff" )
                    ( (ItemType*) entities.back() )->SetLightFlags( ( (ItemType*) entities.back() )->GetLightFlags() | ivalue );
                SET_FIELD_ITEM( "LightDistance", LightDistance );
                SET_FIELD_ITEM( "LightIntensity", LightIntensity );
                if( field == "ScriptName" )
                    entities_addon.back().ScriptName = svalue;
                if( field == "FuncName" )
                    entities_addon.back().FuncName = svalue;

                if( is_critter )
                {
                    SET_FIELD_CRITTER( "Critter_Dir", Dir );
                    SET_FIELD_CRITTER( "Dir", Dir );
                    SET_FIELD_CRITTER( "Critter_Cond", Cond );
                    SET_FIELD_CRITTER( "Critter_Anim1", Anim1Life );
                    SET_FIELD_CRITTER( "Critter_Anim2", Anim2Life );

                    if( field.size() >= 18 /*"Critter_ParamIndex" or "Critter_ParamValue" */ && field.substr( 0, 13 ) == "Critter_Param" )
                    {
                        if( field.substr( 13, 5 ) == "Index" )
                        {
                            cur_prop = CritterType::PropertiesRegistrator->Find( svalue.c_str() );
                            if( !cur_prop )
                            {
                                WriteLog( "Critter property '{}' not found.\n", svalue );
                                fail = true;
                            }
                        }
                        else
                        {
                            if( cur_prop )
                            {
                                if( !_str( svalue ).isNumber() )
                                    _str( svalue ).toHash();
                                if( cur_prop->IsHash() )
                                    cur_prop->SetPODValueAsInt( entities.back(), _str( svalue ).toHash() );
                                else if( cur_prop->IsEnum() )
                                    cur_prop->SetPODValueAsInt( entities.back(), Script::GetEnumValue( cur_prop->GetTypeName(), svalue.c_str(), fail ) );
                                else
                                    cur_prop->SetPODValueAsInt( entities.back(), ConvertParamValue( svalue.c_str(), fail ) );
                                cur_prop = nullptr;
                            }
                        }
                    }
                }
                else
                {
                    SET_FIELD_ITEM( "OffsetX", OffsetX );
                    SET_FIELD_ITEM( "OffsetY", OffsetY );
                    SET_FIELD_ITEM( "PicMapName", PicMap );
                    SET_FIELD_ITEM( "PicInvName", PicInv );

                    if( field == "PicMapName" )
                    {
                        ( (ItemType*) entities.back() )->SetPicMap( _str( svalue ).toHash() );
                    }
                    else if( field == "PicInvName" )
                    {
                        ( (ItemType*) entities.back() )->SetPicInv( _str( svalue ).toHash() );
                    }
                    else
                    {
                        SET_FIELD_ITEM( "Item_Count", Count );
                        SET_FIELD_ITEM( "Item_ItemSlot", Slot );
                        SET_FIELD_ITEM( "Item_TrapValue", TrapValue );
                        if( field == "Item_InContainer" )
                            entities_addon.back().ContainerUID = ivalue;

                        if( field == "Scenery_CanTalk" )
                            ( (ItemType*) entities.back() )->SetIsCanTalk( ivalue != 0 );
                        SET_FIELD_ITEM( "Scenery_SpriteCut", SpriteCut );
                    }
                }
            }
        }
        if( fail )
            return false;
    }

    // Fix script name and params
    for( size_t i = 0; i < entities.size(); i++ )
    {
        Entity* entity = entities[ i ];
        AdditionalFields& entity_addon = entities_addon[ i ];
        if( !entity_addon.ScriptName.empty() || !entity_addon.FuncName.empty() )
        {
            string script_name = entity_addon.ScriptName;
            if( !script_name.empty() )
                script_name += "@";
            script_name += entity_addon.FuncName;

            if( entity->Type == CritterEntityType )
                ( (CritterType*) entity )->SetScriptId( _str( script_name ).toHash() );
            else if( entity->Type == ItemEntityType )
                ( (ItemType*) entity )->SetScriptId( _str( script_name ).toHash() );
        }
    }

    // Set accessory
    for( size_t i = 0; i < entities.size(); i++ )
    {
        Entity* entity = entities[ i ];
        if( entity->Type == ItemEntityType )
            ( (ItemType*) entities[ i ] )->SetAccessory( ITEM_ACCESSORY_HEX );
    }

    // Deprecated, add UIDs
    if( version < FO_MAP_VERSION_TEXT4 )
    {
        uint uid = 0;
        for( size_t i = 0; i < entities.size(); i++ )
        {
            Entity* entity = entities[ i ];
            AdditionalFields& entity_addon = entities_addon[ i ];

            // Find item in container
            if( entity->Type != ItemEntityType )
                continue;

            // Find container
            ItemType* entity_item = (ItemType*) entities[ i ];
            ushort hx = entity_item->GetHexX();
            ushort hy = entity_item->GetHexY();
            for( size_t j = 0; j < entities.size(); j++ )
            {
                Entity* entity_cont = entities[ j ];
                AdditionalFields& entity_cont_addon = entities_addon[ j ];
                if( entity_cont == entity )
                    continue;
                if( entity_cont->Type == ItemEntityType && ( ( (ItemType*) entity_cont )->GetHexX() != hx || ( (ItemType*) entity_cont )->GetHexY() != hy ) )
                    continue;
                if( entity_cont->Type == CritterEntityType && ( ( (CritterType*) entity_cont )->GetHexX() != hx || ( (CritterType*) entity_cont )->GetHexY() != hy ) )
                    continue;
                if( !entity_cont_addon.UID )
                    entity_cont_addon.UID = ++uid;
                entity_addon.ContainerUID = entity_cont_addon.UID;
            }
        }
    }

    // Fix children
    for( size_t i = 0; i < entities.size(); i++ )
    {
        Entity* entity = entities[ i ];
        AdditionalFields& entity_addon = entities_addon[ i ];
        if( !entity_addon.ContainerUID )
            continue;

        RUNTIME_ASSERT( entity->Type == ItemEntityType );
        ItemType* entity_item = (ItemType*) entity;
        entity_item->SetHexX( 0 );
        entity_item->SetHexY( 0 );

        for( size_t j = 0; j < entities.size(); j++ )
        {
            Entity* entity_parent = entities[ j ];
            AdditionalFields& entity_parent_addon = entities_addon[ j ];
            if( !entity_parent_addon.UID || entity_parent_addon.UID != entity_addon.ContainerUID || entity_parent == entity )
                continue;

            if( entity_parent->Type == CritterEntityType )
            {
                entity_item->SetAccessory( ITEM_ACCESSORY_CRITTER );
                entity_item->SetCritId( entity_parent->Id );
            }
            else if( entity_parent->Type == ItemEntityType )
            {
                entity_item->SetAccessory( ITEM_ACCESSORY_CONTAINER );
                entity_item->SetContainerId( entity_parent->Id );
            }
            break;
        }
    }

    return true;
}

template< class CritterType, EntityType CritterEntityType, class ItemType, EntityType ItemEntityType >
static bool LoadProtoMap( ProtoMap& pmap, EntityVec& entities )
{
    // Find file
    FileCollection maps( "fomap" );
    string path;
    File& map_file = maps.FindFile( pmap.GetName(), &path );
    if( !map_file.IsLoaded() )
    {
        WriteLog( "Map '{}' not found.\n", pmap.GetName() );
        return false;
    }

    // Store path
    pmap.SetFileDir( _str( path ).extractDir() );

    // Load from file
    const char* data = map_file.GetCStr();
    bool is_old_format = ( strstr( data, "[Header]" ) && strstr( data, "[Tiles]" ) && strstr( data, "[Objects]" ) );
    if( is_old_format && !LoadOldTextFormat< CritterType, CritterEntityType, ItemType, ItemEntityType >( pmap, data, entities ) )
    {
        WriteLog( "Unable to load map '{}' from old map format.\n", pmap.GetName() );
        return false;
    }
    else if( !is_old_format && !LoadTextFormat< CritterType, CritterEntityType, ItemType, ItemEntityType >( pmap, data, entities ) )
    {
        WriteLog( "Unable to load map '{}'.\n", pmap.GetName() );
        return false;
    }

    return true;
}

template< class CritterType, EntityType CritterEntityType, class ItemType, EntityType ItemEntityType >
static bool SaveProtoMap( ProtoMap& pmap, const string& custom_name )
{
    // Fill data
    IniFile file;
    SaveTextFormat< CritterType, CritterEntityType, ItemType, ItemEntityType >( pmap, file );

    // Save
    string save_fname = pmap.GetFileDir() + ( !custom_name.empty() ? custom_name : pmap.GetName() ) + ".fomap";
    if( !file.SaveFile( save_fname ) )
    {
        WriteLog( "Unable write file '{}' in modules.\n", save_fname );
        return false;
    }
    return true;
}

#if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
bool ProtoMap::Load_Server()
{
    EntityVec entities;
    if( !LoadProtoMap< Npc, EntityType::Npc, Item, EntityType::Item >( *this, entities ) )
        return false;
    return OnAfterLoad< Npc, EntityType::Npc, Item, EntityType::Item >( *this, entities );
}
#endif

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
bool ProtoMap::Load_Client()
{
    EntityVec entities;
    return LoadProtoMap< CritterView, EntityType::CritterView, ItemView, EntityType::ItemView >( *this, entities );
}

bool ProtoMap::Save_Client( const string& custom_name )
{
    return SaveProtoMap< CritterView, EntityType::CritterView, ItemView, EntityType::ItemView >( *this, custom_name );
}
#endif

#if defined ( FONLINE_EDITOR )
void ProtoMap::GenNew()
{
    SetWidth( MAXHEX_DEF );
    SetHeight( MAXHEX_DEF );

    // Morning	 5.00 -  9.59	 300 - 599
    // Day		10.00 - 18.59	 600 - 1139
    // Evening	19.00 - 22.59	1140 - 1379
    // Nigh		23.00 -  4.59	1380
    IntVec vec = { 300, 600, 1140, 1380 };
    UCharVec vec2 = { 18, 128, 103, 51, 18, 128, 95, 40, 53, 128, 86, 29 };
    CScriptArray* arr = Script::CreateArray( "int[]" );
    Script::AppendVectorToArray( vec, arr );
    SetDayTime( arr );
    arr->Release();
    CScriptArray* arr2 = Script::CreateArray( "uint8[]" );
    Script::AppendVectorToArray( vec2, arr2 );
    SetDayColor( arr2 );
    arr2->Release();
}

bool ProtoMap::IsMapFile( const string& fname )
{
    string ext = _str( fname ).getFileExtension();
    if( ext.empty() )
        return false;

    if( ext == "fomap" )
    {
        // Check text format
        IniFile txt;
        if( !txt.AppendFile( fname ) )
            return false;

        return txt.IsApp( "ProtoMap" ) || ( txt.IsApp( "Header" ) && txt.IsApp( "Tiles" ) && txt.IsApp( "Objects" ) );
    }

    return false;
}
#endif

#if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
void ProtoMap::GetStaticItemTriggers( ushort hx, ushort hy, ItemVec& triggers )
{
    for( auto& item : TriggerItemsVec )
        if( item->GetHexX() == hx && item->GetHexY() == hy )
            triggers.push_back( item );
}

Item* ProtoMap::GetStaticItem( ushort hx, ushort hy, hash pid )
{
    for( auto& item : StaticItemsVec )
        if( ( !pid || item->GetProtoId() == pid ) && item->GetHexX() == hx && item->GetHexY() == hy )
            return item;
    return nullptr;
}

void ProtoMap::GetStaticItemsHex( ushort hx, ushort hy, ItemVec& items )
{
    for( auto& item : StaticItemsVec )
        if( item->GetHexX() == hx && item->GetHexY() == hy )
            items.push_back( item );
}

void ProtoMap::GetStaticItemsHexEx( ushort hx, ushort hy, uint radius, hash pid, ItemVec& items )
{
    for( auto& item : StaticItemsVec )
        if( ( !pid || item->GetProtoId() == pid ) && DistGame( item->GetHexX(), item->GetHexY(), hx, hy ) <= radius )
            items.push_back( item );
}

void ProtoMap::GetStaticItemsByPid( hash pid, ItemVec& items )
{
    for( auto& item : StaticItemsVec )
        if( !pid || item->GetProtoId() == pid )
            items.push_back( item );
}
#endif
