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

CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, string, FileDir );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, ushort, Width );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, ushort, Height );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, ushort, WorkHexX );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, ushort, WorkHexY );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, int, CurDayTime );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, hash, ScriptId );
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, CScriptArray *, DayTime );    // 4 int
CLASS_PROPERTY_ALIAS_IMPL( ProtoMap, Map, CScriptArray *, DayColor );   // 12 uchar
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
    #ifdef FONLINE_MAPPER
    for( auto& entity : AllEntities )
        entity->Release();
    AllEntities.clear();
    #endif

    #ifdef FONLINE_SERVER
    CLEAN_CONTAINER( SceneryData );
    CLEAN_CONTAINER( CrittersVec );
    CLEAN_CONTAINER( HexItemsVec );
    CLEAN_CONTAINER( ChildItemsVec );
    CLEAN_CONTAINER( StaticItemsVec );
    CLEAN_CONTAINER( TriggerItemsVec );
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
            kv[ "$Id" ] = _str( "{}", entity->Id );
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
            kv[ "$Id" ] = _str( "{}", entity->Id );
            kv[ "$Proto" ] = entity->Proto->GetName();
            entity->Props.SaveToText( kv, &entity->Proto->Props );
        }
    }

    // Tiles
    for( auto& tile : Tiles )
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

        uint          id = _str( kv[ "$Id" ] ).toUInt();
        hash          proto_id = _str( kv[ "$Proto" ] ).toHash();
        ProtoCritter* proto = ProtoMngr.GetProtoCritter( proto_id );
        if( !proto )
        {
            WriteLog( "Proto critter '{}' not found.\n", _str().parseHash( proto_id ) );
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

        Item* item = new Item( id, proto );
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
        if( hx < 0 || hx >= GetWidth() || hy < 0 || hy > GetHeight() )
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
            SetScriptId( _str( script_name ).toHash() );
        CScriptArray* arr = Script::CreateArray( "int[]" );
        Script::AppendVectorToArray( vec, arr );
        SetDayTime( arr );
        arr->Release();
        CScriptArray* arr2 = Script::CreateArray( "uint8[]" );
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
                if( !istr.fail() && hx >= 0 && hx < GetWidth() && hy >= 0 && hy < GetHeight() )
                {
                    hx *= 2;
                    hy *= 2;
                    if( type == "tile" )
                        Tiles.push_back( Tile( _str( name ).toHash(), hx, hy, 0, 0, 0, false ) );
                    else if( type == "roof" )
                        Tiles.push_back( Tile( _str( name ).toHash(), hx, hy, 0, 0, 0, true ) );
                    else if( type == "0" )
                        Tiles.push_back( Tile( _str( name ).toUInt(), hx, hy, 0, 0, 0, false ) );
                    else if( type == "1" )
                        Tiles.push_back( Tile( _str( name ).toUInt(), hx, hy, 0, 0, 0, true ) );
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

                    string name;
                    std::getline( istr, name, '\n' );
                    name = _str( name ).trim();

                    Tiles.push_back( Tile( _str( name ).toHash(), hx, hy, ox, oy, layer, is_roof ) );
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
    EntityVec entities;
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
                            cur_prop = MUTUAL_CRITTER::PropertiesRegistrator->Find( svalue.c_str() );
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
                        ( (Item*) entities.back() )->SetPicMap( _str( svalue ).toHash() );
                    }
                    else if( field == "PicInvName" )
                    {
                        ( (Item*) entities.back() )->SetPicInv( _str( svalue ).toHash() );
                    }
                    else
                    {
                        SET_FIELD_ITEM( "Item_Count", Count );
                        SET_FIELD_ITEM( "Item_ItemSlot", Slot );
                        SET_FIELD_ITEM( "Item_TrapValue", TrapValue );
                        if( field == "Item_InContainer" )
                            entities_addon.back().ContainerUID = ivalue;

                        if( field == "Scenery_CanTalk" )
                            ( (Item*) entities.back() )->SetIsCanTalk( ivalue != 0 );
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

            if( entity->Type == MUTUAL_CRITTER_TYPE )
                ( (MUTUAL_CRITTER*) entity )->SetScriptId( _str( script_name ).toHash() );
            else if( entity->Type == EntityType::Item )
                ( (Item*) entity )->SetScriptId( _str( script_name ).toHash() );
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
        hash pid = item->GetProtoId();

        if( !item->IsStatic() )
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
            WriteLog( "Invalid item '{}' position on map '{}', hex x {}, hex y {}.\n", item->GetName(), GetName(), hx, hy );
            continue;
        }

        if( !item->GetIsHiddenInStatic() )
        {
            item->AddRef();
            StaticItemsVec.push_back( item );
        }

        if( !item->GetIsNoBlock() )
            SETFLAG( HexFlags[ hy * maxhx + hx ], FH_BLOCK );

        if( !item->GetIsShootThru() )
        {
            SETFLAG( HexFlags[ hy * maxhx + hx ], FH_BLOCK );
            SETFLAG( HexFlags[ hy * maxhx + hx ], FH_NOTRAKE );
        }

        if( item->GetIsScrollBlock() )
        {
            // Block around
            for( int k = 0; k < 6; k++ )
            {
                ushort hx_ = hx, hy_ = hy;
                MoveHexByDir( hx_, hy_, k, maxhx, maxhy );
                SETFLAG( HexFlags[ hy_ * maxhx + hx_ ], FH_BLOCK );
            }
        }

        if( item->GetIsTrigger() || item->GetIsTrap() )
        {
            item->AddRef();
            TriggerItemsVec.push_back( item );

            SETFLAG( HexFlags[ hy * maxhx + hx ], FH_STATIC_TRIGGER );
        }

        if( item->IsNonEmptyBlockLines() )
        {
            ushort hx_ = hx, hy_ = hy;
            bool raked = item->GetIsShootThru();
            FOREACH_PROTO_ITEM_LINES( item->GetBlockLines(), hx_, hy_, maxhx, maxhy,
                                      SETFLAG( HexFlags[ hy_ * maxhx + hx_ ], FH_BLOCK );
                                      if( !raked )
                                          SETFLAG( HexFlags[ hy_ * maxhx + hx_ ], FH_NOTRAKE );
                                      );
        }

        // Data for client
        if( !item->GetIsHidden() )
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
    CrVec( CrittersVec ).swap( CrittersVec );
    ItemVec( HexItemsVec ).swap( HexItemsVec );
    ItemVec( ChildItemsVec ).swap( ChildItemsVec );
    ItemVec( StaticItemsVec ).swap( StaticItemsVec );
    ItemVec( TriggerItemsVec ).swap( TriggerItemsVec );
    TileVec( Tiles ).swap( Tiles );

    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) SceneryData.capacity() );
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
        hash func_num = Script::BindScriptFuncNumByFuncName( _str().parseHash( GetScriptId() ), "void %s(Map, bool)" );
        if( !func_num )
        {
            WriteLog( "Map '{}', can't bind map function '{}'.\n", GetName(), _str().parseHash( GetScriptId() ) );
            errors++;
        }
    }

    // Entities scripts
    for( auto& entity : entities )
    {
        if( entity->Type == MUTUAL_CRITTER_TYPE && ( (MUTUAL_CRITTER*) entity )->GetScriptId() )
        {
            string func_name = _str().parseHash( ( (MUTUAL_CRITTER*) entity )->GetScriptId() );
            hash func_num = Script::BindScriptFuncNumByFuncName( func_name, "void %s(Critter, bool)" );
            if( !func_num )
            {
                WriteLog( "Map '{}', can't bind critter function '{}'.\n", GetName(), func_name );
                errors++;
            }
        }
        else if( entity->Type == EntityType::Item && !( (Item*) entity )->IsStatic() && ( (Item*) entity )->GetScriptId() )
        {
            string func_name = _str().parseHash( ( (Item*) entity )->GetScriptId() );
            hash func_num = Script::BindScriptFuncNumByFuncName( func_name, "void %s(Item, bool)" );
            if( !func_num )
            {
                WriteLog( "Map '{}', can't bind item function '{}'.\n", GetName(), func_name );
                errors++;
            }
        }
        else if( entity->Type == EntityType::Item && ( (Item*) entity )->IsStatic() && ( (Item*) entity )->GetScriptId() )
        {
            Item* item = (Item*) entity;
            string func_name = _str().parseHash( item->GetScriptId() );
            uint bind_id = 0;
            if( item->GetIsTrigger() || item->GetIsTrap() )
                bind_id = Script::BindByFuncName( func_name, "void %s(Critter, const Item, bool, uint8)", false );
            else
                bind_id = Script::BindByFuncName( func_name, "bool %s(Critter, const Item, Item, int)", false );
            if( !bind_id )
            {
                WriteLog( "Map '{}', can't bind static item function '{}'.\n", GetName(), func_name );
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
    string path;
    FileManager& map_file = maps.FindFile( GetName(), &path );
    if( !map_file.IsLoaded() )
    {
        WriteLog( "Map '{}' not found.\n", GetName() );
        return false;
    }

    // Store path
    SetFileDir( _str( path ).extractDir() );

    // Load from file
    const char* data = map_file.GetCStr();
    bool is_old_format = ( strstr( data, "[Header]" ) && strstr( data, "[Tiles]" ) && strstr( data, "[Objects]" ) );
    if( is_old_format && !LoadOldTextFormat( data ) )
    {
        WriteLog( "Unable to load map '{}' from old map format.\n", GetName() );
        return false;
    }
    else if( !is_old_format && !LoadTextFormat( data ) )
    {
        WriteLog( "Unable to load map '{}'.\n", GetName() );
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
    CScriptArray* arr = Script::CreateArray( "int[]" );
    Script::AppendVectorToArray( vec, arr );
    SetDayTime( arr );
    arr->Release();
    CScriptArray* arr2 = Script::CreateArray( "uint8[]" );
    Script::AppendVectorToArray( vec2, arr2 );
    SetDayColor( arr2 );
    arr2->Release();
}

bool ProtoMap::Save( const string& custom_name )
{
    // Fill data
    IniParser file;
    SaveTextFormat( file );

    // Save
    string save_fname = GetFileDir() + ( !custom_name.empty() ? custom_name : GetName() ) + ".fomap";
    if( !file.SaveFile( save_fname ) )
    {
        WriteLog( "Unable write file '{}' in modules.\n", save_fname );
        return false;
    }
    return true;
}

bool ProtoMap::IsMapFile( const string& fname )
{
    string ext = _str( fname ).getFileExtension();
    if( ext.empty() )
        return false;

    if( ext == "fomap" )
    {
        // Check text format
        IniParser txt;
        if( !txt.AppendFile( fname ) )
            return false;

        return txt.IsApp( "ProtoMap" ) || ( txt.IsApp( "Header" ) && txt.IsApp( "Tiles" ) && txt.IsApp( "Objects" ) );
    }

    return false;
}
#endif // FONLINE_MAPPER

#ifdef FONLINE_SERVER
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
#endif // FONLINE_SERVER

CLASS_PROPERTY_ALIAS_IMPL( ProtoLocation, Location, CScriptArray *, MapProtos );

ProtoLocation::ProtoLocation( hash pid ): ProtoEntity( pid, Location::PropertiesRegistrator )
{
    //
}
