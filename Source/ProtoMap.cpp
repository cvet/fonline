#include "ProtoMap.h"
#include "ProtoManager.h"
#include "Crypt.h"
#include <strstream>
#include "IniParser.h"
#include "Script.h"

#ifdef FONLINE_SERVER
# include "Map.h"
# include "Critter.h"
#else
# include "MapCl.h"
# include "CritterCl.h"
#endif

#ifdef FONLINE_SERVER
# define MUTUAL_CRITTER         Critter
# define MUTUAL_CRITTER_TYPE    EntityType::Npc
#else
# define MUTUAL_CRITTER         CritterCl
# define MUTUAL_CRITTER_TYPE    EntityType::CritterCl
#endif

CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, ushort, Width );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, ushort, Height );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, ushort, WorkHexX );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, ushort, WorkHexY );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, int, CurDayTime );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, hash, ScriptId );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, ScriptArray *, DayTime );    // 4 int
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, ScriptArray *, DayColor );   // 12 uchar
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, bool, IsNoLogOut );

ProtoMap::ProtoMap( hash pid ): ProtoEntity( pid, Map::PropertiesRegistrator )
{
    #ifdef FONLINE_SERVER
    MEMORY_PROCESS( MEMORY_PROTO_MAP, sizeof( ProtoMap ) );
    #endif

    #ifdef FONLINE_MAPPER
    LastEntityId = 0;
    #endif
}

ProtoMap::~ProtoMap()
{
    #ifdef FONLINE_SERVER
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) sizeof( ProtoMap ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) SceneryData.capacity() );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) mapEntires.capacity() * sizeof( MapEntire ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) Tiles.capacity() * sizeof( MapEntire ) );

    SAFEDELA( HexFlags );

    SceneryData.clear();
    mapEntires.clear();

    for( auto it = CrittersVec.begin(), end = CrittersVec.end(); it != end; ++it )
        SAFEREL( *it );
    CrittersVec.clear();
    for( auto it = HexItemsVec.begin(), end = HexItemsVec.end(); it != end; ++it )
        SAFEREL( *it );
    HexItemsVec.clear();
    for( auto it = ChildItemsVec.begin(), end = ChildItemsVec.end(); it != end; ++it )
        SAFEREL( *it );
    ChildItemsVec.clear();
    for( auto it = SceneryVec.begin(), end = SceneryVec.end(); it != end; ++it )
        SAFEREL( *it );
    SceneryVec.clear();
    for( auto it = GridsVec.begin(), end = GridsVec.end(); it != end; ++it )
        SAFEREL( *it );
    GridsVec.clear();
    #endif

    Tiles.clear();
    #ifdef FONLINE_MAPPER
    for( auto& entity : AllEntities )
        entity->Release();
    AllEntities.clear();
    #endif

    #ifdef FONLINE_SERVER
    CLEAN_CONTAINER( SceneryData );
    CLEAN_CONTAINER( mapEntires );
    CLEAN_CONTAINER( CrittersVec );
    CLEAN_CONTAINER( HexItemsVec );
    CLEAN_CONTAINER( ChildItemsVec );
    CLEAN_CONTAINER( SceneryVec );
    CLEAN_CONTAINER( GridsVec );
    CLEAN_CONTAINER( Tiles );
    #endif
}

#ifdef FONLINE_MAPPER
void ProtoMap::SaveTextFormat( IniParser& file )
{
    // Header
    Props.SaveToText( file.SetApp( "ProtoMap" ), nullptr );

    // Critters
    for( auto& entity : AllEntities )
    {
        if( entity->Type == MUTUAL_CRITTER_TYPE )
        {
            StrMap& kv = file.SetApp( "Critter" );
            kv[ "$Id" ] = Str::UItoA( entity->Id );
            kv[ "$Proto" ] = entity->Proto->GetName();
            entity->Props.SaveToText( kv, &entity->Proto->Props );
        }
    }

    // Items
    for( auto& entity : AllEntities )
    {
        if( entity->Type == EntityType::Item )
        {
            StrMap& kv = file.SetApp( "Item" );
            kv[ "$Id" ] = Str::UItoA( entity->Id );
            kv[ "$Proto" ] = entity->Proto->GetName();
            entity->Props.SaveToText( kv, &entity->Proto->Props );
        }
    }

    // Tiles
    for( auto& tile : Tiles )
    {
        StrMap& kv = file.SetApp( "Tile" );
        kv[ "PicMap" ] = Str::GetName( tile.Name );
        kv[ "HexX" ] = Str::UItoA( tile.HexX );
        kv[ "HexY" ] = Str::UItoA( tile.HexY );
        if( tile.OffsX )
            kv[ "OffsetX" ] = Str::ItoA( tile.OffsX );
        if( tile.OffsY )
            kv[ "OffsetY" ] = Str::ItoA( tile.OffsY );
        if( tile.Layer )
            kv[ "Layer" ] = Str::UItoA( tile.Layer );
        if( tile.IsRoof )
            kv[ "IsRoof" ] = Str::BtoA( tile.IsRoof );
    }
}
#endif

bool ProtoMap::LoadTextFormat( const char* buf )
{
    int       errors = 0;
    EntityVec entities;

    // Header
    IniParser map_data;
    map_data.AppendStr( buf );
    if( !map_data.IsApp( "ProtoMap" ) )
    {
        WriteLog( "Invalid map format.\n" );
        return false;
    }
    Props.LoadFromText( map_data.GetApp( "ProtoMap" ) );

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

        uint          id = Str::AtoUI( kv[ "$Id" ].c_str() );
        hash          proto_id = Str::GetHash( kv[ "$Proto" ].c_str() );
        ProtoCritter* proto = ProtoMngr.GetProtoCritter( proto_id );
        if( !proto )
        {
            WriteLog( "Proto critter '%s' not found.\n", Str::GetName( proto_id ) );
            errors++;
            continue;
        }

        #ifdef FONLINE_SERVER
        Npc*       npc = new Npc( id, proto );
        #else
        CritterCl* npc = new CritterCl( id, proto );
        #endif
        if( !npc->Props.LoadFromText( kv ) )
        {
            WriteLog( "Unable to load critter '%s' properties.\n", Str::GetName( proto_id ) );
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

        uint       id = Str::AtoUI( kv[ "$Id" ].c_str() );
        hash       proto_id = Str::GetHash( kv[ "$Proto" ].c_str() );
        ProtoItem* proto = ProtoMngr.GetProtoItem( proto_id );
        if( !proto )
        {
            WriteLog( "Proto item '%s' not found.\n", Str::GetName( proto_id ) );
            errors++;
            continue;
        }

        Item* item = new Item( id, proto );
        if( !item->Props.LoadFromText( kv ) )
        {
            WriteLog( "Unable to load item '%s' properties.\n", Str::GetName( proto_id ) );
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

        hash name = Str::GetHash( kv[ "PicMap" ].c_str() );
        int  hx = Str::AtoI( kv[ "HexX" ].c_str() );
        int  hy = Str::AtoI( kv[ "HexY" ].c_str() );
        int  ox = ( kv.count( "OffsetX" ) ? Str::AtoI( kv[ "OffsetX" ].c_str() ) : 0 );
        int  oy = ( kv.count( "OffsetY" ) ? Str::AtoI( kv[ "OffsetY" ].c_str() ) : 0 );
        int  layer = ( kv.count( "Layer" ) ? Str::AtoI( kv[ "Layer" ].c_str() ) : 0 );
        bool is_roof = ( kv.count( "IsRoof" ) ? Str::AtoB( kv[ "IsRoof" ].c_str() ) : false );
        if( hx < 0 || hx >= GetWidth() || hy < 0 || hy > GetHeight() )
        {
            WriteLog( "Tile '%s' have wrong hex position %d %d.\n", Str::GetName( name ), hx, hy );
            errors++;
            continue;
        }
        if( layer < 0 || layer > 255 )
        {
            WriteLog( "Tile '%s' have wrong layer value %d.\n", Str::GetName( name ), layer );
            errors++;
            continue;
        }

        Tiles.push_back( Tile( name, hx, hy, ox, oy, layer, is_roof ) );
    }

    if( errors )
        return false;
    return OnAfterLoad( entities );
}

bool ProtoMap::LoadOldTextFormat( const char* buf )
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

    IniParser map_ini;
    map_ini.CollectContent();
    map_ini.AppendStr( buf );

    // Header
    const char* header_str = map_ini.GetAppContent( "Header" );
    int         version = -1;
    if( header_str )
    {
        istrstream istr( header_str );
        string     field, value;
        int        ivalue;
        string     script_name;
        IntVec     vec = { 300, 600, 1140, 1380 };
        UCharVec   vec2 = { 18, 128, 103, 51, 18, 128, 95, 40, 53, 128, 86, 29 };
        while( !istr.eof() && !istr.fail() )
        {
            istr >> field >> value;
            if( !istr.fail() )
            {
                ivalue = atoi( value.c_str() );
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
                    SetWidth( ivalue );
                else if( field == "MaxHexY" )
                    SetHeight( ivalue );
                else if( field == "WorkHexX" || field == "CenterX" )
                    SetWorkHexX( ivalue );
                else if( field == "WorkHexY" || field == "CenterY" )
                    SetWorkHexY( ivalue );
                else if( field == "Time" )
                    SetCurDayTime( ivalue );
                else if( field == "NoLogOut" )
                    SetIsNoLogOut( ivalue != 0 );
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
            SetScriptId( Str::GetHash( script_name.c_str() ) );
        ScriptArray* arr = Script::CreateArray( "int[]" );
        Script::AppendVectorToArray( vec, arr );
        SetDayTime( arr );
        arr->Release();
        ScriptArray* arr2 = Script::CreateArray( "uint8[]" );
        Script::AppendVectorToArray( vec2, arr2 );
        SetDayColor( arr2 );
        arr2->Release();
    }
    if( ( version != FO_MAP_VERSION_TEXT1 && version != FO_MAP_VERSION_TEXT2 && version != FO_MAP_VERSION_TEXT3 &&
          version != FO_MAP_VERSION_TEXT4 && version != FO_MAP_VERSION_TEXT5 && version != FO_MAP_VERSION_TEXT6 &&
          version != FO_MAP_VERSION_TEXT7 ) ||
        GetWidth() < 1 || GetHeight() < 1 )
        return false;

    // Tiles
    const char* tiles_str = map_ini.GetAppContent( "Tiles" );
    if( tiles_str )
    {
        istrstream istr( tiles_str );
        string     type;
        if( version == FO_MAP_VERSION_TEXT1 )
        {
            string name;

            // Deprecated
            while( !istr.eof() && !istr.fail() )
            {
                int hx, hy;
                istr >> type >> hx >> hy >> name;
                if( !istr.fail() && hx >= 0 && hx < GetWidth() && hy >= 0 && hy < GetHeight() )
                {
                    hx *= 2;
                    hy *= 2;
                    if( type == "tile" )
                        Tiles.push_back( Tile( Str::GetHash( name.c_str() ), hx, hy, 0, 0, 0, false ) );
                    else if( type == "roof" )
                        Tiles.push_back( Tile( Str::GetHash( name.c_str() ), hx, hy, 0, 0, 0, true ) );
                    else if( type == "0" )
                        Tiles.push_back( Tile( Str::AtoUI( name.c_str() ), hx, hy, 0, 0, 0, false ) );
                    else if( type == "1" )
                        Tiles.push_back( Tile( Str::AtoUI( name.c_str() ), hx, hy, 0, 0, 0, true ) );
                }
            }
        }
        else
        {
            char   name[ MAX_FOTEXT ];
            int    hx, hy;
            int    ox, oy, layer;
            bool   is_roof;
            size_t len;
            bool   has_offs;
            bool   has_layer;
            while( !istr.eof() && !istr.fail() )
            {
                istr >> type >> hx >> hy;
                if( !istr.fail() && hx >= 0 && hx < GetWidth() && hy >= 0 && hy < GetHeight() )
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

                    istr.getline( name, MAX_FOTEXT );
                    Str::Trim( name );

                    Tiles.push_back( Tile( Str::GetHash( name ), hx, hy, ox, oy, layer, is_roof ) );
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
        uint ParentChildIndex = 0;
        string ScriptName;
        string FuncName;
        uint ParamsCount = 0;
        uint Params[ 5 ];
    };
    vector< AdditionalFields > entities_addon;
    EntityVec entities;
    const char* objects_str = map_ini.GetAppContent( "Objects" );
    if( objects_str )
    {
        bool       fail = false;
        istrstream istr( objects_str );
        string     field;
        char       svalue[ MAX_FOTEXT ];
        int        ivalue;
        int        is_critter = false;
        Property*  cur_prop = nullptr;
        uint       auto_id = uint( -1 );
        while( !istr.eof() && !istr.fail() )
        {
            istr >> field;
            istr.getline( svalue, MAX_FOTEXT );
            Str::Trim( svalue );

            if( !istr.fail() )
            {
                ivalue = Str::AtoI( svalue );

                if( field == "MapObjType" )
                {
                    if( ivalue == MAP_OBJECT_CRITTER )
                    {
                        is_critter = true;
                    }
                    else
                    {
                        RUNTIME_ASSERT( ivalue == MAP_OBJECT_ITEM || ivalue == MAP_OBJECT_SCENERY );
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
                            proto_id = Str::GetHash( svalue );
                            if( is_critter && !ProtoMngr.GetProtoCritter( proto_id ) )
                            {
                                WriteLog( "Critter prototype '%s' not found.\n", svalue );
                                fail = true;
                            }
                            else if( !is_critter && !ProtoMngr.GetProtoItem( proto_id ) )
                            {
                                WriteLog( "Item prototype '%s' not found.\n", svalue );
                                fail = true;
                            }
                        }
                        if( fail )
                            break;

                        if( is_critter )
                        {
                            #ifdef FONLINE_SERVER
                            Npc* cr = new Npc( auto_id--, ProtoMngr.GetProtoCritter( proto_id ) );
                            #else
                            CritterCl* cr = new CritterCl( auto_id--, ProtoMngr.GetProtoCritter( proto_id ) );
                            #endif
                            cr->SetCond( COND_LIFE );
                            entities.push_back( cr );
                            entities_addon.push_back( AdditionalFields() );
                        }
                        else
                        {
                            Item* item = new Item( auto_id--, ProtoMngr.GetProtoItem( proto_id ) );
                            entities.push_back( item );
                            entities_addon.push_back( AdditionalFields() );
                        }

                        continue;
                    }

                    #define SET_FIELD_CRITTER( field_name, prop )                         \
                        if( field == field_name && is_critter )                           \
                        {                                                                 \
                            ( (MUTUAL_CRITTER*) entities.back() )->Set ## prop( ivalue ); \
                            continue;                                                     \
                        }
                    #define SET_FIELD_ITEM( field_name, prop )                  \
                        if( field == field_name && !is_critter )                \
                        {                                                       \
                            ( (Item*) entities.back() )->Set ## prop( ivalue ); \
                            continue;                                           \
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
                    else if( field == "ParentChildIndex" )
                        entities_addon.back().ParentChildIndex = ivalue;
                    SET_FIELD_ITEM( "LightColor", LightColor );
                    if( field == "LightDay" )
                        ( (Item*) entities.back() )->SetLightFlags( ( (Item*) entities.back() )->GetLightFlags() | ( ( ivalue & 3 ) << 6 ) );
                    if( field == "LightDirOff" )
                        ( (Item*) entities.back() )->SetLightFlags( ( (Item*) entities.back() )->GetLightFlags() | ivalue );
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
                                cur_prop = MUTUAL_CRITTER::PropertiesRegistrator->Find( svalue );
                                if( !cur_prop )
                                {
                                    WriteLog( "Critter property '%s' not found.\n", svalue );
                                    fail = true;
                                }
                            }
                            else
                            {
                                if( cur_prop )
                                {
                                    if( !Str::IsNumber( svalue ) )
                                        Str::GetHash( svalue );
                                    if( cur_prop->IsHash() )
                                        cur_prop->SetPODValueAsInt( entities.back(), Str::GetHash( svalue ) );
                                    else if( cur_prop->IsEnum() )
                                        cur_prop->SetPODValueAsInt( entities.back(), Script::GetEnumValue( cur_prop->GetTypeName(), svalue, fail ) );
                                    else
                                        cur_prop->SetPODValueAsInt( entities.back(), ConvertParamValue( svalue, fail ) );
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
                            ( (Item*) entities.back() )->SetPicMap( Str::GetHash( svalue ) );
                        }
                        else if( field == "PicInvName" )
                        {
                            ( (Item*) entities.back() )->SetPicInv( Str::GetHash( svalue ) );
                        }
                        else
                        {
                            SET_FIELD_ITEM( "Item_Count", Count );
                            SET_FIELD_ITEM( "Item_ItemSlot", Slot );
                            if( field == "Item_AmmoPid" )
                                ( (Item*) entities.back() )->SetAmmoPid( Str::GetHash( svalue ) );
                            SET_FIELD_ITEM( "Item_AmmoCount", AmmoCount );
                            SET_FIELD_ITEM( "Item_TrapValue", TrapValue );
                            if( field == "Item_InContainer" )
                                entities_addon.back().ContainerUID = ivalue;

                            if( field == "Scenery_CanUse" )
                                ( (Item*) entities.back() )->SetIsCanUse( ivalue != 0 );
                            if( field == "Scenery_CanTalk" )
                                ( (Item*) entities.back() )->SetIsCanTalk( ivalue != 0 );
                            if( field == "Scenery_ParamsCount" )
                                entities_addon.back().ParamsCount = ivalue;
                            if( field == "Scenery_Param0" )
                                entities_addon.back().Params[ 0 ] = ivalue;
                            if( field == "Scenery_Param1" )
                                entities_addon.back().Params[ 1 ] = ivalue;
                            if( field == "Scenery_Param2" )
                                entities_addon.back().Params[ 2 ] = ivalue;
                            if( field == "Scenery_Param3" )
                                entities_addon.back().Params[ 3 ] = ivalue;
                            if( field == "Scenery_Param4" )
                                entities_addon.back().Params[ 4 ] = ivalue;
                            if( field == "Scenery_ToMap" || field == "Scenery_ToMapPid" )
                                ( (Item*) entities.back() )->SetGrid_ToMap( Str::GetHash( svalue ) );
                            SET_FIELD_ITEM( "Scenery_ToEntire", Grid_ToMapEntire );
                            SET_FIELD_ITEM( "Scenery_ToDir", Grid_ToMapDir );
                            SET_FIELD_ITEM( "Scenery_SpriteCut", SpriteCut );
                        }
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
            char script_name[ MAX_FOTEXT ] = { 0 };
            Str::Copy( script_name, entity_addon.ScriptName.c_str() );
            if( script_name[ 0 ] )
                Str::Append( script_name, "@" );
            Str::Append( script_name, entity_addon.FuncName.c_str() );
            if( entity->Type == MUTUAL_CRITTER_TYPE )
                ( (MUTUAL_CRITTER*) entity )->SetScriptId( Str::GetHash( script_name ) );
            else if( entity->Type == EntityType::Item )
                ( (Item*) entity )->SetScriptId( Str::GetHash( script_name ) );
        }

        if( entity_addon.ParamsCount )
        {
            RUNTIME_ASSERT( entity->Type == EntityType::Item );
            RUNTIME_ASSERT( entity_addon.ParamsCount <= 5 );
            ScriptArray* params = Script::CreateArray( "int[]" );
            for( uint i = 0; i < entity_addon.ParamsCount; i++ )
                params->InsertLast( &entity_addon.Params[ i ] );
            ( (Item*) entity )->SetSceneryParams( params );
            params->Release();
        }
    }

    // Set accessory
    for( size_t i = 0; i < entities.size(); i++ )
    {
        Entity* entity = entities[ i ];
        if( entity->Type == EntityType::Item )
            ( (Item*) entities[ i ] )->SetAccessory( ITEM_ACCESSORY_HEX );
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
            if( entity->Type != EntityType::Item )
                continue;

            // Find container
            Item* entity_item = (Item*) entities[ i ];
            ushort hx = entity_item->GetHexX();
            ushort hy = entity_item->GetHexY();
            for( size_t j = 0; j < entities.size(); j++ )
            {
                Entity* entity_cont = entities[ j ];
                AdditionalFields& entity_cont_addon = entities_addon[ j ];
                if( entity_cont == entity )
                    continue;
                if( entity_cont->Type == EntityType::Item && ( ( (Item*) entity_cont )->GetHexX() != hx || ( (Item*) entity_cont )->GetHexY() != hy ) )
                    continue;
                if( entity_cont->Type == MUTUAL_CRITTER_TYPE && ( ( (MUTUAL_CRITTER*) entity_cont )->GetHexX() != hx || ( (MUTUAL_CRITTER*) entity_cont )->GetHexY() != hy ) )
                    continue;
                if( entity_cont->Type == EntityType::Item && ( (Item*) entity_cont )->GetType() != ITEM_TYPE_CONTAINER )
                    continue;
                if( !entity_cont_addon.UID )
                    entity_cont_addon.UID = ++uid;
                entity_addon.ContainerUID = entity_cont_addon.UID;
            }
        }
    }

    // Fix children positions
    for( size_t i = 0; i < entities.size();)
    {
        Entity* entity = entities[ i ];
        AdditionalFields& entity_addon = entities_addon[ i ];
        if( !entity_addon.ParentUID )
        {
            i++;
            continue;
        }

        RUNTIME_ASSERT( entity->Type == EntityType::Item );
        Item* entity_item = (Item*) entity;
        bool delete_child = true;
        for( size_t j = 0; j < entities.size(); j++ )
        {
            Entity* entity_parent = entities[ j ];
            AdditionalFields& entity_parent_addon = entities_addon[ j ];
            if( !entity_parent_addon.UID || entity_parent_addon.UID != entity_addon.ParentUID || entity_parent == entity )
                continue;

            RUNTIME_ASSERT( entity_parent->Type == EntityType::Item );
            Item* entity_parent_item = (Item*) entity_parent;
            if( !entity_parent_item->GetChildPid( entity_addon.ParentChildIndex ) )
                break;

            ushort child_hx = entity_parent_item->GetHexX();
            ushort child_hy = entity_parent_item->GetHexY();
            FOREACH_PROTO_ITEM_LINES( entity_parent_item->GetChildLinesStr( entity_addon.ParentChildIndex ), child_hx, child_hy, GetWidth(), GetHeight() );

            entity_item->SetHexX( child_hx );
            entity_item->SetHexY( child_hy );
            RUNTIME_ASSERT( entity_item->GetProtoId() == entity_parent_item->GetChildPid( entity_addon.ParentChildIndex ) );
            delete_child = false;
            break;
        }

        if( delete_child )
        {
            entities[ i ]->Release();
            entities.erase( entities.begin() + i );
        }
        else
        {
            i++;
        }
    }

    // Fix children
    for( size_t i = 0; i < entities.size(); i++ )
    {
        Entity* entity = entities[ i ];
        AdditionalFields& entity_addon = entities_addon[ i ];
        if( !entity_addon.ContainerUID )
            continue;

        RUNTIME_ASSERT( entity->Type == EntityType::Item );
        Item* entity_item = (Item*) entity;
        entity_item->SetHexX( 0 );
        entity_item->SetHexY( 0 );

        for( size_t j = 0; j < entities.size(); j++ )
        {
            Entity* entity_parent = entities[ j ];
            AdditionalFields& entity_parent_addon = entities_addon[ j ];
            if( !entity_parent_addon.UID || entity_parent_addon.UID != entity_addon.ContainerUID || entity_parent == entity )
                continue;

            if( entity_parent->Type == EntityType::CritterCl )
            {
                entity_item->SetAccessory( ITEM_ACCESSORY_CRITTER );
                entity_item->SetCritId( entity_parent->Id );
            }
            else if( entity_parent->Type == EntityType::Item )
            {
                entity_item->SetAccessory( ITEM_ACCESSORY_CONTAINER );
                entity_item->SetContainerId( entity_parent->Id );
            }
            break;
        }
    }

    return OnAfterLoad( entities );
}

bool ProtoMap::OnAfterLoad( EntityVec& entities )
{
    #ifdef FONLINE_SERVER
    // Bind scripts
    if( !BindScripts( entities ) )
        return false;

    // Parse objects
    ushort maxhx = GetWidth();
    ushort maxhy = GetHeight();
    HexFlags = new uchar[ maxhx * maxhy ];
    memzero( HexFlags, maxhx * maxhy );

    uint scenery_count = 0;
    UCharVec scenery_data;
    for( auto& entity : entities )
    {
        if( entity->Type == MUTUAL_CRITTER_TYPE )
        {
            entity->AddRef();
            CrittersVec.push_back( (MUTUAL_CRITTER*) entity );
            continue;
        }

        RUNTIME_ASSERT( entity->Type == EntityType::Item );
        Item* item = (Item*) entity;
        int type = item->GetType();
        hash pid = item->GetProtoId();

        if( !item->IsScenery() )
        {
            entity->AddRef();
            if( item->GetAccessory() == ITEM_ACCESSORY_HEX )
                HexItemsVec.push_back( (Item*) entity );
            else
                ChildItemsVec.push_back( (Item*) entity );
            continue;
        }

        RUNTIME_ASSERT( item->GetAccessory() == ITEM_ACCESSORY_HEX );

        ushort hx = item->GetHexX();
        ushort hy = item->GetHexY();
        if( hx >= maxhx || hy >= maxhy )
        {
            WriteLog( "Invalid item '%s' position on map '%s', hex x %u, hex y %u.\n", item->GetName(), GetName(), hx, hy );
            continue;
        }

        if( pid == SP_GRID_ENTIRE )
        {
            mapEntires.push_back( MapEntire( hx, hy, item->GetGrid_ToMapDir(), item->GetGrid_ToMapEntire() ) );
            continue;
        }

        if( type == ITEM_TYPE_WALL || type == ITEM_TYPE_GENERIC || type == ITEM_TYPE_GRID )
        {
            if( type == ITEM_TYPE_GRID )
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_SCEN_GRID );
            if( !item->GetIsNoBlock() )
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_BLOCK );
            if( !item->GetIsShootThru() )
            {
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_BLOCK );
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_NOTRAKE );
            }
            if( type == ITEM_TYPE_WALL )
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_WALL );
            else
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_SCEN );

            if( pid == SP_MISC_SCRBLOCK )
            {
                // Block around
                for( int k = 0; k < 6; k++ )
                {
                    ushort hx_ = hx, hy_ = hy;
                    MoveHexByDir( hx_, hy_, k, GetWidth(), GetHeight() );
                    SETFLAG( HexFlags[ hy_ * maxhx + hx_ ], FH_BLOCK );
                }
            }
            else if( type == ITEM_TYPE_GRID )
            {
                item->AddRef();
                GridsVec.push_back( item );
            }
            else if( type == ITEM_TYPE_GENERIC )
            {
                item->AddRef();
                SceneryVec.push_back( item );
            }

            if( pid == SP_SCEN_TRIGGER )
            {
                if( item->GetScriptId() )
                    SETFLAG( HexFlags[ hy * maxhx + hx ], FH_TRIGGER );
            }

            // Data for client
            if( pid != SP_SCEN_TRIGGER )
            {
                scenery_count++;
                WriteData( scenery_data, item->Id );
                WriteData( scenery_data, item->GetProtoId() );
                PUCharVec* all_data;
                UIntVec* all_data_sizes;
                item->Props.StoreData( false, &all_data, &all_data_sizes );
                WriteData( scenery_data, (uint) all_data->size() );
                for( size_t i = 0; i < all_data->size(); i++ )
                {
                    WriteData( scenery_data, all_data_sizes->at( i ) );
                    WriteDataArr( scenery_data, all_data->at( i ), all_data_sizes->at( i ) );
                }
            }
        }
    }
    SceneryData.clear();
    WriteData( SceneryData, scenery_count );
    if( !scenery_data.empty() )
        WriteDataArr( SceneryData, &scenery_data[ 0 ], (uint) scenery_data.size() );

    for( auto& entity : entities )
        entity->Release();
    entities.clear();

    // Generate hashes
    HashTiles = maxhx * maxhy;
    if( Tiles.size() )
        HashTiles = Crypt.MurmurHash2( (uchar*) &Tiles[ 0 ], (uint) Tiles.size() * sizeof( Tile ) );
    HashScen = maxhx * maxhy;
    if( SceneryData.size() )
        HashScen = Crypt.MurmurHash2( (uchar*) &SceneryData[ 0 ], (uint) SceneryData.size() );

    // Shrink the vector capacities to fit their contents and reduce memory use
    UCharVec( SceneryData ).swap( SceneryData );
    EntiresVec( mapEntires ).swap( mapEntires );
    CrVec( CrittersVec ).swap( CrittersVec );
    ItemVec( HexItemsVec ).swap( HexItemsVec );
    ItemVec( ChildItemsVec ).swap( ChildItemsVec );
    ItemVec( SceneryVec ).swap( SceneryVec );
    ItemVec( GridsVec ).swap( GridsVec );
    TileVec( Tiles ).swap( Tiles );

    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) SceneryData.capacity() );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) mapEntires.capacity() * sizeof( MapEntire ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) GetWidth() * GetHeight() );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) Tiles.capacity() * sizeof( Tile ) );
    #endif

    #ifdef FONLINE_MAPPER
    // Get lower id
    LastEntityId = 0;
    for( auto& entity : entities )
        if( !LastEntityId || entity->Id < LastEntityId )
            LastEntityId = entity->Id;

    AllEntities = std::move( entities );
    #endif

    return true;
}

#ifdef FONLINE_SERVER
bool ProtoMap::BindScripts( EntityVec& entities )
{
    int errors = 0;

    // Map script
    if( GetScriptId() )
    {
        hash func_num = Script::BindScriptFuncNumByFuncName( Str::GetName( GetScriptId() ), "void %s(Map&,bool)" );
        if( !func_num )
        {
            WriteLog( "Map '%s', can't bind map function '%s'.\n", GetName(), Str::GetName( GetScriptId() ) );
            errors++;
        }
    }

    // Entities scripts
    for( auto& entity : entities )
    {
        if( entity->Type == MUTUAL_CRITTER_TYPE && ( (MUTUAL_CRITTER*) entity )->GetScriptId() )
        {
            const char* func_name = Str::GetName( ( (MUTUAL_CRITTER*) entity )->GetScriptId() );
            hash func_num = Script::BindScriptFuncNumByFuncName( func_name, "void %s(Critter&,bool)" );
            if( !func_num )
            {
                WriteLog( "Map '%s', can't bind critter function '%s'.\n", GetName(), func_name );
                errors++;
            }
        }
        else if( entity->Type == EntityType::Item && !( (Item*) entity )->IsScenery() && ( (Item*) entity )->GetScriptId() )
        {
            const char* func_name = Str::GetName( ( (Item*) entity )->GetScriptId() );
            hash func_num = Script::BindScriptFuncNumByFuncName( func_name, "void %s(Item&,bool)" );
            if( !func_num )
            {
                WriteLog( "Map '%s', can't bind item function '%s'.\n", GetName(), func_name );
                errors++;
            }
        }
        else if( entity->Type == EntityType::Item && ( (Item*) entity )->IsScenery() && ( (Item*) entity )->GetScriptId() )
        {
            Item* item = (Item*) entity;
            const char* func_name = Str::GetName( item->GetScriptId() );
            uint bind_id = 0;
            if( item->GetProtoId() != SP_SCEN_TRIGGER )
                bind_id = Script::BindByFuncName( func_name, "bool %s(Critter&,const Item&,Item@,int)", false );
            else
                bind_id = Script::BindByFuncName( func_name, "void %s(Critter&,const Item&,bool,uint8)", false );
            if( !bind_id )
            {
                WriteLog( "Map '%s', can't bind scenery function '%s'.\n", GetName(), func_name );
                errors++;
            }
            item->SceneryScriptBindId = bind_id;
        }
    }
    return errors == 0;
}
#endif // FONLINE_SERVER

bool ProtoMap::Load()
{
    // Find file
    FilesCollection maps( "fomap" );
    const char* path;
    FileManager& map_file = maps.FindFile( GetName(), &path );
    if( !map_file.IsLoaded() )
    {
        WriteLog( "Map '%s' not found.\n", GetName() );
        return false;
    }

    // Store path
    char dir[ MAX_FOPATH ];
    FileManager::ExtractDir( path, dir );
    pmapDir = dir;

    // Load from file
    const char* data = map_file.GetCStr();
    bool is_old_format = ( Str::Substring( data, "[Header]" ) && Str::Substring( data, "[Tiles]" ) && Str::Substring( data, "[Objects]" ) );
    if( is_old_format && !LoadOldTextFormat( data ) )
    {
        WriteLog( "Unable to load map '%s' from old map format.\n", GetName() );
        return false;
    }
    else if( !is_old_format && !LoadTextFormat( data ) )
    {
        WriteLog( "Unable to load map '%s'.\n", GetName() );
        return false;
    }

    return true;
}

#ifdef FONLINE_MAPPER
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
    ScriptArray* arr = Script::CreateArray( "int[]" );
    Script::AppendVectorToArray( vec, arr );
    SetDayTime( arr );
    arr->Release();
    ScriptArray* arr2 = Script::CreateArray( "uint8[]" );
    Script::AppendVectorToArray( vec2, arr2 );
    SetDayColor( arr2 );
    arr2->Release();
}

bool ProtoMap::Save( const char* custom_name /* = NULL */ )
{
    // Fill data
    IniParser file;
    SaveTextFormat( file );

    // Save
    string save_fname = pmapDir + ( custom_name && *custom_name ? string( custom_name ) : string( GetName() ) ) + ".fomap";
    if( !file.SaveFile( save_fname.c_str() ) )
    {
        WriteLog( "Unable write file '%s' in modules.\n", save_fname.c_str() );
        return false;
    }
    return true;
}

bool ProtoMap::IsMapFile( const char* fname )
{
    const char* ext = FileManager::GetExtension( fname );
    if( !ext )
        return false;

    if( Str::CompareCase( ext, "fomap" ) )
    {
        // Check text format
        IniParser txt;
        if( !txt.AppendFile( fname ) )
            return false;
        return txt.IsApp( "Header" ) && txt.IsApp( "Tiles" ) && txt.IsApp( "Objects" );
    }

    return false;
}
#endif // FONLINE_MAPPER

#ifdef FONLINE_SERVER
uint ProtoMap::CountEntire( uint num )
{
    if( num == uint( -1 ) )
        return (uint) mapEntires.size();

    uint result = 0;
    for( uint i = 0, j = (uint) mapEntires.size(); i < j; i++ )
    {
        if( mapEntires[ i ].Number == num )
            result++;
    }
    return result;
}

ProtoMap::MapEntire* ProtoMap::GetEntire( uint num, uint skip )
{
    for( uint i = 0, j = (uint) mapEntires.size(); i < j; i++ )
    {
        if( num == uint( -1 ) || mapEntires[ i ].Number == num )
        {
            if( !skip )
                return &mapEntires[ i ];
            else
                skip--;
        }
    }

    return nullptr;
}

ProtoMap::MapEntire* ProtoMap::GetEntireRandom( uint num )
{
    vector< MapEntire* > entires;
    for( uint i = 0, j = (uint) mapEntires.size(); i < j; i++ )
    {
        if( num == uint( -1 ) || mapEntires[ i ].Number == num )
            entires.push_back( &mapEntires[ i ] );
    }

    if( entires.empty() )
        return nullptr;
    return entires[ Random( 0, (uint) entires.size() - 1 ) ];
}

ProtoMap::MapEntire* ProtoMap::GetEntireNear( uint num, ushort hx, ushort hy )
{
    MapEntire* near_entire = nullptr;
    uint       last_dist = 0;
    for( uint i = 0, j = (uint) mapEntires.size(); i < j; i++ )
    {
        MapEntire& entire = mapEntires[ i ];
        if( num == uint( -1 ) || entire.Number == num )
        {
            uint dist = DistGame( hx, hy, entire.HexX, entire.HexY );
            if( !near_entire || dist < last_dist )
            {
                near_entire = &entire;
                last_dist = dist;
            }
        }
    }
    return near_entire;
}

ProtoMap::MapEntire* ProtoMap::GetEntireNear( uint num, uint num_ext, ushort hx, ushort hy )
{
    MapEntire* near_entire = nullptr;
    uint       last_dist = 0;
    for( uint i = 0, j = (uint) mapEntires.size(); i < j; i++ )
    {
        MapEntire& entire = mapEntires[ i ];
        if( num == uint( -1 ) || num_ext == uint( -1 ) || entire.Number == num || entire.Number == num_ext )
        {
            uint dist = DistGame( hx, hy, entire.HexX, entire.HexY );
            if( !near_entire || dist < last_dist )
            {
                near_entire = &entire;
                last_dist = dist;
            }
        }
    }
    return near_entire;
}

void ProtoMap::GetEntires( uint num, EntiresVec& entires )
{
    for( auto& entire : mapEntires )
        if( num == uint( -1 ) || entire.Number == num )
            entires.push_back( entire );
}

Item* ProtoMap::GetMapScenery( ushort hx, ushort hy, hash pid )
{
    for( auto& item : SceneryVec )
        if( ( !pid || item->GetProtoId() == pid ) && item->GetHexX() == hx && item->GetHexY() == hy )
            return item;
    return nullptr;
}

void ProtoMap::GetMapSceneriesHex( ushort hx, ushort hy, ItemVec& items )
{
    for( auto& item : SceneryVec )
        if( item->GetHexX() == hx && item->GetHexY() == hy )
            items.push_back( item );
}

void ProtoMap::GetMapSceneriesHexEx( ushort hx, ushort hy, uint radius, hash pid, ItemVec& items )
{
    for( auto& item : SceneryVec )
        if( ( !pid || item->GetProtoId() == pid ) && DistGame( item->GetHexX(), item->GetHexY(), hx, hy ) <= radius )
            items.push_back( item );
}

void ProtoMap::GetMapSceneriesByPid( hash pid, ItemVec& items )
{
    for( auto& item : SceneryVec )
        if( !pid || item->GetProtoId()  == pid )
            items.push_back( item );
}

Item* ProtoMap::GetMapGrid( ushort hx, ushort hy )
{
    for( auto& item : GridsVec )
        if( item->GetHexX() == hx && item->GetHexY() == hy )
            return item;
    return nullptr;
}

#endif // FONLINE_SERVER

CLASS_PROPERTY_ALIAS_IMPL( ProtoLocation, Location, ScriptArray *, MapProtos );

ProtoLocation::ProtoLocation( hash pid ): ProtoEntity( pid, Location::PropertiesRegistrator )
{
    //
}
