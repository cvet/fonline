#include "Common.h"
#include "ProtoMap.h"
#include "ItemManager.h"
#include "CritterManager.h"
#include "Crypt.h"
#include <strstream>
#include "IniParser.h"

#ifndef FONLINE_MAPPER
# include "Script.h"
# include "Critter.h"
#else
# include "ResourceManager.h"
#endif

#define FO_MAP_VERSION_TEXT1    ( 1 )
#define FO_MAP_VERSION_TEXT2    ( 2 )
#define FO_MAP_VERSION_TEXT3    ( 3 )
#define FO_MAP_VERSION_TEXT4    ( 4 )
#define FO_MAP_VERSION_TEXT5    ( 5 )
#define FO_MAP_VERSION_TEXT6    ( 6 )
#define FO_MAP_VERSION_TEXT7    ( 7 )

#define APP_HEADER              "Header"
#define APP_TILES               "Tiles"
#define APP_OBJECTS             "Objects"

bool ProtoMap::Init( const char* name )
{
    Clear();
    if( !name || !name[ 0 ] )
        return false;

    pmapName = name;
    pmapPid = Str::GetHash( name );
    if( !pmapPid )
        return false;

    #ifdef FONLINE_MAPPER
    LastObjectUID = 0;
    #endif

    if( !Refresh() )
    {
        Clear();
        return false;
    }
    return true;
}

void ProtoMap::Clear()
{
    #ifdef FONLINE_SERVER
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) SceneriesToSend.capacity() * sizeof( SceneryCl ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) WallsToSend.capacity() * sizeof( SceneryCl ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) mapEntires.capacity() * sizeof( MapEntire ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) CrittersVec.size() * sizeof( MapObject ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) ItemsVec.size() * sizeof( MapObject ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) SceneryVec.size() * sizeof( MapObject ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) GridsVec.size() * sizeof( MapObject ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) Header.MaxHexX * Header.MaxHexY );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) Tiles.capacity() * sizeof( MapEntire ) );

    SAFEDELA( HexFlags );

    SceneriesToSend.clear();
    WallsToSend.clear();
    mapEntires.clear();

    for( auto it = CrittersVec.begin(), end = CrittersVec.end(); it != end; ++it )
        SAFEREL( *it );
    CrittersVec.clear();
    for( auto it = ItemsVec.begin(), end = ItemsVec.end(); it != end; ++it )
        SAFEREL( *it );
    ItemsVec.clear();
    for( auto it = SceneryVec.begin(), end = SceneryVec.end(); it != end; ++it )
        SAFEREL( *it );
    SceneryVec.clear();
    for( auto it = GridsVec.begin(), end = GridsVec.end(); it != end; ++it )
        SAFEREL( *it );
    GridsVec.clear();
    #endif

    Tiles.clear();
    #ifdef FONLINE_MAPPER
    for( auto it = MObjects.begin(), end = MObjects.end(); it != end; ++it )
    {
        MapObject* mobj = *it;
        mobj->RunTime.FromMap = nullptr;
        mobj->RunTime.MapObjId = 0;
        SAFEREL( mobj );
    }
    MObjects.clear();
    TilesField.clear();
    RoofsField.clear();
    #endif
    memzero( &Header, sizeof( Header ) );
    pmapName = "";
    pmapPid = 0;

    #ifdef FONLINE_SERVER
    CLEAN_CONTAINER( SceneriesToSend );
    CLEAN_CONTAINER( WallsToSend );
    CLEAN_CONTAINER( mapEntires );
    CLEAN_CONTAINER( CrittersVec );
    CLEAN_CONTAINER( ItemsVec );
    CLEAN_CONTAINER( SceneryVec );
    CLEAN_CONTAINER( GridsVec );
    CLEAN_CONTAINER( Tiles );
    #endif
}

bool ProtoMap::LoadTextFormat( const char* buf )
{
    IniParser map_ini;
    map_ini.CollectContent();
    map_ini.AppendStr( buf );

    // Header
    memzero( &Header, sizeof( Header ) );
    const char* header_str = map_ini.GetAppContent( APP_HEADER );
    if( header_str )
    {
        istrstream istr( header_str );
        string     field, value;
        int        ivalue;
        while( !istr.eof() && !istr.fail() )
        {
            istr >> field >> value;
            if( !istr.fail() )
            {
                ivalue = atoi( value.c_str() );
                if( field == "Version" )
                {
                    Header.Version = ivalue;
                    uint old_version = ( ivalue << 20 );
                    #define FO_MAP_VERSION_V6    ( 0xFE000000 )
                    #define FO_MAP_VERSION_V7    ( 0xFF000000 )
                    #define FO_MAP_VERSION_V8    ( 0xFF100000 )
                    #define FO_MAP_VERSION_V9    ( 0xFF200000 )
                    if( old_version == FO_MAP_VERSION_V6 || old_version == FO_MAP_VERSION_V7 ||
                        old_version == FO_MAP_VERSION_V8 || old_version == FO_MAP_VERSION_V9 )
                    {
                        Header.Version = FO_MAP_VERSION_TEXT1;
                        Header.DayTime[ 0 ] = 300;
                        Header.DayTime[ 1 ] = 600;
                        Header.DayTime[ 2 ] = 1140;
                        Header.DayTime[ 3 ] = 1380;
                        Header.DayColor[ 0 ] = 18;
                        Header.DayColor[ 1 ] = 128;
                        Header.DayColor[ 2 ] = 103;
                        Header.DayColor[ 3 ] = 51;
                        Header.DayColor[ 4 ] = 18;
                        Header.DayColor[ 5 ] = 128;
                        Header.DayColor[ 6 ] = 95;
                        Header.DayColor[ 7 ] = 40;
                        Header.DayColor[ 8 ] = 53;
                        Header.DayColor[ 9 ] = 128;
                        Header.DayColor[ 10 ] = 86;
                        Header.DayColor[ 11 ] = 29;
                    }
                }
                else if( field == "MaxHexX" )
                    Header.MaxHexX = ivalue;
                else if( field == "MaxHexY" )
                    Header.MaxHexY = ivalue;
                else if( field == "WorkHexX" || field == "CenterX" )
                    Header.WorkHexX = ivalue;
                else if( field == "WorkHexY" || field == "CenterY" )
                    Header.WorkHexY = ivalue;
                else if( field == "Time" )
                    Header.Time = ivalue;
                else if( field == "NoLogOut" )
                    Header.NoLogOut = ( ivalue != 0 );
                else if( ( field == "ScriptModule" || field == "ScriptName" ) && value != "-" )
                    Str::Copy( Header.ScriptModule, value.c_str() );
                else if( field == "ScriptFunc" && value != "-" )
                    Str::Copy( Header.ScriptFunc, value.c_str() );
                else if( field == "DayTime" )
                {
                    Header.DayTime[ 0 ] = ivalue;
                    istr >> Header.DayTime[ 1 ];
                    istr >> Header.DayTime[ 2 ];
                    istr >> Header.DayTime[ 3 ];
                }
                else if( field == "DayColor0" )
                {
                    Header.DayColor[ 0 ] = ivalue;
                    istr >> ivalue;
                    Header.DayColor[ 4 ] = ivalue;
                    istr >> ivalue;
                    Header.DayColor[ 8 ] = ivalue;
                }
                else if( field == "DayColor1" )
                {
                    Header.DayColor[ 1 ] = ivalue;
                    istr >> ivalue;
                    Header.DayColor[ 5 ] = ivalue;
                    istr >> ivalue;
                    Header.DayColor[ 9 ] = ivalue;
                }
                else if( field == "DayColor2" )
                {
                    Header.DayColor[ 2 ] = ivalue;
                    istr >> ivalue;
                    Header.DayColor[ 6 ] = ivalue;
                    istr >> ivalue;
                    Header.DayColor[ 10 ] = ivalue;
                }
                else if( field == "DayColor3" )
                {
                    Header.DayColor[ 3 ] = ivalue;
                    istr >> ivalue;
                    Header.DayColor[ 7 ] = ivalue;
                    istr >> ivalue;
                    Header.DayColor[ 11 ] = ivalue;
                }
            }
        }
    }
    if( ( Header.Version != FO_MAP_VERSION_TEXT1 && Header.Version != FO_MAP_VERSION_TEXT2 && Header.Version != FO_MAP_VERSION_TEXT3 &&
          Header.Version != FO_MAP_VERSION_TEXT4 && Header.Version != FO_MAP_VERSION_TEXT5 && Header.Version != FO_MAP_VERSION_TEXT6 &&
          Header.Version != FO_MAP_VERSION_TEXT7 ) ||
        Header.MaxHexX < 1 || Header.MaxHexY < 1 )
        return false;

    // Tiles
    const char* tiles_str = map_ini.GetAppContent( APP_TILES );
    if( tiles_str )
    {
        istrstream istr( tiles_str );
        string     type;
        if( Header.Version == FO_MAP_VERSION_TEXT1 )
        {
            string name;

            // Deprecated
            while( !istr.eof() && !istr.fail() )
            {
                int hx, hy;
                istr >> type >> hx >> hy >> name;
                if( !istr.fail() && hx >= 0 && hx < Header.MaxHexX && hy >= 0 && hy < Header.MaxHexY )
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
                if( !istr.fail() && hx >= 0 && hx < Header.MaxHexX && hy >= 0 && hy < Header.MaxHexY )
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
    MapObjectPtrVec mobjects;
    const char* objects_str = map_ini.GetAppContent( APP_OBJECTS );
    if( objects_str )
    {
        bool       fail = false;
        istrstream istr( objects_str );
        string     field;
        char       svalue[ MAX_FOTEXT ];
        int        ivalue;
        #ifndef FONLINE_MAPPER
        Property*  cur_prop = nullptr;
        #endif
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
                    MapObject* mobj = new MapObject();

                    mobj->MapObjType = ivalue;
                    if( ivalue == MAP_OBJECT_CRITTER )
                        mobj->MCritter.Cond = COND_LIFE;
                    else if( ivalue != MAP_OBJECT_ITEM && ivalue != MAP_OBJECT_SCENERY )
                        continue;

                    mobjects.push_back( mobj );
                }
                else if( mobjects.size() )
                {
                    MapObject& mobj = *mobjects.back();
                    // Shared
                    if( field == "ProtoId" )
                    {
                        const char* proto_name = nullptr;
                        if( mobj.MapObjType == MAP_OBJECT_ITEM || mobj.MapObjType == MAP_OBJECT_SCENERY )
                            proto_name = ConvertProtoIdByInt( ivalue );
                        else if( mobj.MapObjType == MAP_OBJECT_CRITTER )
                            proto_name = ConvertProtoCritterIdByInt( ivalue );
                        RUNTIME_ASSERT( proto_name );
                        mobj.ProtoId = ( proto_name ? Str::GetHash( proto_name ) : ivalue );
                        #ifdef FONLINE_MAPPER
                        mobj.ProtoName = ScriptString::Create( proto_name ? proto_name : Str::ItoA( ivalue ) );
                        #endif
                        if( mobj.MapObjType == MAP_OBJECT_CRITTER && !CrMngr.GetProto( mobj.ProtoId ) )
                        {
                            WriteLog( "Critter prototype '%s' (%u) not found.\n", proto_name, ivalue );
                            fail = true;
                        }
                        else if( mobj.MapObjType != MAP_OBJECT_CRITTER && !ItemMngr.GetProtoItem( mobj.ProtoId ) )
                        {
                            WriteLog( "Item prototype '%s' (%u) not found.\n", proto_name, ivalue );
                            fail = true;
                        }

                        // Fix scenery/item
                        if( !fail && ( mobj.MapObjType == MAP_OBJECT_ITEM || mobj.MapObjType == MAP_OBJECT_SCENERY ) )
                        {
                            ProtoItem* proto = ItemMngr.GetProtoItem( mobj.ProtoId );
                            if( mobj.MapObjType == MAP_OBJECT_ITEM && !proto->IsItem() )
                                mobj.MapObjType = MAP_OBJECT_SCENERY;
                            else if( mobj.MapObjType == MAP_OBJECT_SCENERY && proto->IsItem() )
                                mobj.MapObjType = MAP_OBJECT_ITEM;
                        }
                    }
                    else if( field == "ProtoName" )
                    {
                        mobj.ProtoId = Str::GetHash( svalue );
                        #ifdef FONLINE_MAPPER
                        mobj.ProtoName = ScriptString::Create( svalue );
                        #endif
                        if( mobj.MapObjType == MAP_OBJECT_CRITTER && !CrMngr.GetProto( mobj.ProtoId ) )
                        {
                            WriteLog( "Critter prototype '%s' not found.\n", svalue );
                            fail = true;
                        }
                        else if( mobj.MapObjType != MAP_OBJECT_CRITTER && !ItemMngr.GetProtoItem( mobj.ProtoId ) )
                        {
                            WriteLog( "Item prototype '%s' not found.\n", svalue );
                            fail = true;
                        }

                        // Fix scenery/item
                        if( !fail && ( mobj.MapObjType == MAP_OBJECT_ITEM || mobj.MapObjType == MAP_OBJECT_SCENERY ) )
                        {
                            ProtoItem* proto = ItemMngr.GetProtoItem( mobj.ProtoId );
                            if( mobj.MapObjType == MAP_OBJECT_ITEM && !proto->IsItem() )
                                mobj.MapObjType = MAP_OBJECT_SCENERY;
                            else if( mobj.MapObjType == MAP_OBJECT_SCENERY && proto->IsItem() )
                                mobj.MapObjType = MAP_OBJECT_ITEM;
                        }
                    }
                    else if( field == "MapX" )
                        mobj.MapX = ivalue;
                    else if( field == "MapY" )
                        mobj.MapY = ivalue;
                    else if( field == "UID" )
                        mobj.UID = ivalue;
                    else if( field == "ContainerUID" )
                        mobj.ContainerUID = ivalue;
                    else if( field == "ParentUID" )
                        mobj.ParentUID = ivalue;
                    else if( field == "ParentChildIndex" )
                        mobj.ParentChildIndex = ivalue;
                    else if( field == "LightColor" )
                        mobj.LightColor = ivalue;
                    else if( field == "LightDay" )
                        mobj.LightDay = ivalue;
                    else if( field == "LightDirOff" )
                        mobj.LightDirOff = ivalue;
                    else if( field == "LightDistance" )
                        mobj.LightDistance = ivalue;
                    else if( field == "LightIntensity" )
                        mobj.LightIntensity = ivalue;
                    else if( field == "ScriptName" )
                        Str::Copy( mobj.ScriptName, svalue );
                    else if( field == "FuncName" )
                        Str::Copy( mobj.FuncName, svalue );
                    else if( field == "UserData0" )
                        mobj.UserData[ 0 ] = ivalue;
                    else if( field == "UserData1" )
                        mobj.UserData[ 1 ] = ivalue;
                    else if( field == "UserData2" )
                        mobj.UserData[ 2 ] = ivalue;
                    else if( field == "UserData3" )
                        mobj.UserData[ 3 ] = ivalue;
                    else if( field == "UserData4" )
                        mobj.UserData[ 4 ] = ivalue;
                    else if( field == "UserData5" )
                        mobj.UserData[ 5 ] = ivalue;
                    else if( field == "UserData6" )
                        mobj.UserData[ 6 ] = ivalue;
                    else if( field == "UserData7" )
                        mobj.UserData[ 7 ] = ivalue;
                    else if( field == "UserData8" )
                        mobj.UserData[ 8 ] = ivalue;
                    else if( field == "UserData9" )
                        mobj.UserData[ 9 ] = ivalue;
                    // Critter
                    else if( mobj.MapObjType == MAP_OBJECT_CRITTER )
                    {
                        if( field == "Critter_Dir" || field == "Dir" )
                            mobj.MCritter.Dir = ivalue;
                        else if( field == "Critter_Cond" )
                            mobj.MCritter.Cond = ivalue;
                        else if( field == "Critter_Anim1" )
                            mobj.MCritter.Anim1 = ivalue;
                        else if( field == "Critter_Anim2" )
                            mobj.MCritter.Anim2 = ivalue;
                        else if( field.size() >= 18 /* "Critter_ParamIndex" or "Critter_ParamValue" */ && field.substr( 0, 13 ) == "Critter_Param" )
                        {
                            if( field.substr( 13, 5 ) == "Index" )
                            {
                                if( !mobj.Props )
                                    mobj.AllocateProps();
                                #ifndef FONLINE_MAPPER
                                cur_prop = Critter::PropertiesRegistrator->Find( svalue );
                                mobj.Props->push_back( cur_prop ? cur_prop->GetEnumValue() : -1 );
                                if( !cur_prop )
                                {
                                    WriteLog( "Critter property '%s' not found.\n", svalue );
                                    fail = true;
                                }
                                #else
                                mobj.Props->push_back( ScriptString::Create( svalue ) );
                                #endif
                            }
                            else if( mobj.Props && mobj.Props->size() % 2 == 1 )
                            {
                                #ifndef FONLINE_MAPPER
                                if( cur_prop )
                                {
                                    if( !Str::IsNumber( svalue ) )
                                        Str::GetHash( svalue );
                                    if( cur_prop->IsHash() )
                                        mobj.Props->push_back( Str::GetHash( svalue ) );
                                    else if( cur_prop->IsEnum() )
                                        mobj.Props->push_back( Script::GetEnumValue( cur_prop->GetTypeName(), svalue, fail ) );
                                    else
                                        mobj.Props->push_back( ConvertParamValue( svalue, fail ) );
                                    cur_prop = nullptr;
                                }
                                else
                                {
                                    mobj.Props->push_back( 0 );
                                }
                                #else
                                mobj.Props->push_back( ScriptString::Create( svalue ) );
                                #endif
                            }
                        }
                    }
                    // Item/Scenery
                    else if( mobj.MapObjType == MAP_OBJECT_ITEM || mobj.MapObjType == MAP_OBJECT_SCENERY )
                    {
                        // Shared parameters
                        if( field == "OffsetX" )
                            mobj.MItem.OffsetX = ivalue;
                        else if( field == "OffsetY" )
                            mobj.MItem.OffsetY = ivalue;
                        else if( field == "PicMapName" )
                        {
                            #ifdef FONLINE_MAPPER
                            Str::Copy( mobj.RunTime.PicMapName, svalue );
                            #endif
                            mobj.MItem.PicMap = Str::GetHash( svalue );
                        }
                        else if( field == "PicInvName" )
                        {
                            #ifdef FONLINE_MAPPER
                            Str::Copy( mobj.RunTime.PicInvName, svalue );
                            #endif
                            mobj.MItem.PicInv = Str::GetHash( svalue );
                        }
                        // Item
                        else if( mobj.MapObjType == MAP_OBJECT_ITEM )
                        {
                            if( field == "Item_Count" )
                                mobj.MItem.Count = ivalue;
                            else if( field == "Item_BrokenFlags" )
                                mobj.MItem.BrokenFlags = ivalue;
                            else if( field == "Item_BrokenCount" )
                                mobj.MItem.BrokenCount = ivalue;
                            else if( field == "Item_Deterioration" )
                                mobj.MItem.Deterioration = ivalue;
                            else if( field == "Item_ItemSlot" )
                                mobj.MItem.ItemSlot = ivalue;
                            else if( field == "Item_AmmoPid" )
                            #ifndef FONLINE_MAPPER
                                mobj.MItem.AmmoPid = Str::GetHash( svalue );
                            #else
                                mobj.MItem.AmmoPid = ScriptString::Create( svalue );
                            #endif
                            else if( field == "Item_AmmoCount" )
                                mobj.MItem.AmmoCount = ivalue;
                            else if( field == "Item_LockerDoorId" )
                                mobj.MItem.LockerDoorId = ivalue;
                            else if( field == "Item_LockerCondition" )
                                mobj.MItem.LockerCondition = ivalue;
                            else if( field == "Item_LockerComplexity" )
                                mobj.MItem.LockerComplexity = ivalue;
                            else if( field == "Item_TrapValue" )
                                mobj.MItem.TrapValue = ivalue;
                            else if( field == "Item_Val0" )
                                mobj.MItem.Val[ 0 ] = ivalue;
                            else if( field == "Item_Val1" )
                                mobj.MItem.Val[ 1 ] = ivalue;
                            else if( field == "Item_Val2" )
                                mobj.MItem.Val[ 2 ] = ivalue;
                            else if( field == "Item_Val3" )
                                mobj.MItem.Val[ 3 ] = ivalue;
                            else if( field == "Item_Val4" )
                                mobj.MItem.Val[ 4 ] = ivalue;
                            else if( field == "Item_Val5" )
                                mobj.MItem.Val[ 5 ] = ivalue;
                            else if( field == "Item_Val6" )
                                mobj.MItem.Val[ 6 ] = ivalue;
                            else if( field == "Item_Val7" )
                                mobj.MItem.Val[ 7 ] = ivalue;
                            else if( field == "Item_Val8" )
                                mobj.MItem.Val[ 8 ] = ivalue;
                            else if( field == "Item_Val9" )
                                mobj.MItem.Val[ 9 ] = ivalue;
                            // Deprecated
                            else if( field == "Item_DeteorationFlags" )
                                mobj.MItem.BrokenFlags = ivalue;
                            else if( field == "Item_DeteorationCount" )
                                mobj.MItem.BrokenCount = ivalue;
                            else if( field == "Item_DeteorationValue" )
                                mobj.MItem.Deterioration = ivalue;
                            else if( field == "Item_InContainer" )
                                mobj.ContainerUID = ivalue;
                        }
                        // Scenery
                        else if( mobj.MapObjType == MAP_OBJECT_SCENERY )
                        {
                            if( field == "Scenery_CanUse" )
                                mobj.MScenery.CanUse = ( ivalue != 0 );
                            else if( field == "Scenery_CanTalk" )
                                mobj.MScenery.CanTalk = ( ivalue != 0 );
                            else if( field == "Scenery_TriggerNum" )
                                mobj.MScenery.TriggerNum = ivalue;
                            else if( field == "Scenery_ParamsCount" )
                                mobj.MScenery.ParamsCount = ivalue;
                            else if( field == "Scenery_Param0" )
                                mobj.MScenery.Param[ 0 ] = ivalue;
                            else if( field == "Scenery_Param1" )
                                mobj.MScenery.Param[ 1 ] = ivalue;
                            else if( field == "Scenery_Param2" )
                                mobj.MScenery.Param[ 2 ] = ivalue;
                            else if( field == "Scenery_Param3" )
                                mobj.MScenery.Param[ 3 ] = ivalue;
                            else if( field == "Scenery_Param4" )
                                mobj.MScenery.Param[ 4 ] = ivalue;
                            else if( field == "Scenery_ToMap" || field == "Scenery_ToMapPid" )
                            #ifndef FONLINE_MAPPER
                                mobj.MScenery.ToMap = Str::GetHash( svalue );
                            #else
                                mobj.MScenery.ToMap = ScriptString::Create( svalue );
                            #endif
                            else if( field == "Scenery_ToEntire" )
                                mobj.MScenery.ToEntire = ivalue;
                            else if( field == "Scenery_ToDir" )
                                mobj.MScenery.ToDir = ivalue;
                            else if( field == "Scenery_SpriteCut" )
                                mobj.MScenery.SpriteCut = ivalue;
                        }
                    }
                }
            }
        }
        if( fail )
            return false;
    }

    // Deprecated, add UIDs
    if( Header.Version < FO_MAP_VERSION_TEXT4 )
    {
        uint uid = 0;
        for( uint i = 0, j = (uint) mobjects.size(); i < j; i++ )
        {
            MapObject* mobj = mobjects[ i ];

            // Find item in container
            if( mobj->MapObjType != MAP_OBJECT_ITEM || !mobj->ContainerUID )
                continue;

            // Find container
            for( uint k = 0, l = (uint) mobjects.size(); k < l; k++ )
            {
                MapObject* mobj_ = mobjects[ k ];
                if( mobj_->MapX != mobj->MapX || mobj_->MapY != mobj->MapY )
                    continue;
                if( mobj_->MapObjType != MAP_OBJECT_ITEM && mobj_->MapObjType != MAP_OBJECT_CRITTER )
                    continue;
                if( mobj_ == mobj )
                    continue;
                if( mobj_->MapObjType == MAP_OBJECT_ITEM )
                {
                    ProtoItem* proto_item = ItemMngr.GetProtoItem( mobj_->ProtoId );
                    if( !proto_item || proto_item->GetType() != ITEM_TYPE_CONTAINER )
                        continue;
                }
                if( !mobj_->UID )
                    mobj_->UID = ++uid;
                mobj->ContainerUID = mobj_->UID;
            }
        }
    }

    // Fix child objects positions
    for( uint i = 0, j = (uint) mobjects.size(); i < j;)
    {
        MapObject* mobj_child = mobjects[ i ];
        if( !mobj_child->ParentUID )
        {
            i++;
            continue;
        }

        bool delete_child = true;
        for( uint k = 0, l = (uint) mobjects.size(); k < l; k++ )
        {
            MapObject* mobj_parent = mobjects[ k ];
            if( !mobj_parent->UID || mobj_parent->UID != mobj_child->ParentUID || mobj_parent == mobj_child )
                continue;

            ProtoItem* proto_parent = ItemMngr.GetProtoItem( mobj_parent->ProtoId );
            if( !proto_parent || !proto_parent->GetChildPid( mobj_child->ParentChildIndex ) )
                break;

            ushort child_hx = mobj_parent->MapX, child_hy = mobj_parent->MapY;
            FOREACH_PROTO_ITEM_LINES( proto_parent->GetChildLinesStr( mobj_child->ParentChildIndex ), child_hx, child_hy, Header.MaxHexX, Header.MaxHexY );

            mobj_child->MapX = child_hx;
            mobj_child->MapY = child_hy;
            mobj_child->ProtoId = proto_parent->GetChildPid( mobj_child->ParentChildIndex );
            #ifdef FONLINE_MAPPER
            SAFEREL( mobj_child->ProtoName );
            mobj_child->ProtoName = ScriptString::Create( Str::GetName( mobj_child->ProtoId ) );
            #endif
            delete_child = false;
            break;
        }

        if( delete_child )
        {
            mobjects[ i ]->Release();
            mobjects.erase( mobjects.begin() + i );
            j = (uint) mobjects.size();
        }
        else
        {
            i++;
        }
    }

    #ifdef FONLINE_SERVER
    // Bind scripts
    if( !BindScripts( mobjects ) )
        return false;

    // Parse objects
    WallsToSend.clear();
    SceneriesToSend.clear();
    ushort maxhx = Header.MaxHexX;
    ushort maxhy = Header.MaxHexY;

    HexFlags = new uchar[ Header.MaxHexX * Header.MaxHexY ];
    if( !HexFlags )
        return false;
    memzero( HexFlags, Header.MaxHexX * Header.MaxHexY );

    for( auto it = mobjects.begin(), end = mobjects.end(); it != end; ++it )
    {
        MapObject& mobj = *( *it );

        if( mobj.MapObjType == MAP_OBJECT_CRITTER )
        {
            mobj.AddRef();
            CrittersVec.push_back( &mobj );
            continue;
        }
        else if( mobj.MapObjType == MAP_OBJECT_ITEM )
        {
            mobj.AddRef();
            ItemsVec.push_back( &mobj );
            continue;
        }

        hash   pid = mobj.ProtoId;
        ushort hx = mobj.MapX;
        ushort hy = mobj.MapY;

        ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
        if( !proto_item )
        {
            WriteLog( "Map '%s', unknown item '%s', hex x %u, hex y %u.\n", pmapName.c_str(), Str::GetName( pid ), hx, hy );
            continue;
        }

        if( hx >= maxhx || hy >= maxhy )
        {
            WriteLog( "Invalid item '%s' position on map '%s', hex x %u, hex y %u.\n", Str::GetName( pid ), pmapName.c_str(), hx, hy );
            continue;
        }

        int type = proto_item->GetType();
        switch( type )
        {
        case ITEM_TYPE_WALL:
        {
            if( !Item::PropertyIsNoBlock->GetValue< bool >( &proto_item->ItemPropsEntity ) )
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_BLOCK );
            if( !Item::PropertyIsShootThru->GetValue< bool >( &proto_item->ItemPropsEntity ) )
            {
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_BLOCK );
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_NOTRAKE );
            }
            SETFLAG( HexFlags[ hy * maxhx + hx ], FH_WALL );

            // To client
            SceneryCl cur_wall;
            memzero( &cur_wall, sizeof( SceneryCl ) );

            cur_wall.ProtoId = mobj.ProtoId;
            cur_wall.MapX = mobj.MapX;
            cur_wall.MapY = mobj.MapY;
            cur_wall.OffsetX = mobj.MScenery.OffsetX;
            cur_wall.OffsetY = mobj.MScenery.OffsetY;
            cur_wall.LightColor = mobj.LightColor;
            cur_wall.LightDistance = mobj.LightDistance;
            cur_wall.LightFlags = mobj.LightDirOff | ( ( mobj.LightDay & 3 ) << 6 );
            cur_wall.LightIntensity = mobj.LightIntensity;
            cur_wall.PicMap = mobj.MScenery.PicMap;
            cur_wall.SpriteCut = mobj.MScenery.SpriteCut;

            WallsToSend.push_back( cur_wall );
        }
        break;
        case ITEM_TYPE_GENERIC:
        case ITEM_TYPE_GRID:
        {
            if( pid == SP_GRID_ENTIRE )
            {
                mapEntires.push_back( MapEntire( hx, hy, mobj.MScenery.ToDir, mobj.MScenery.ToEntire ) );
                continue;
            }

            if( type == ITEM_TYPE_GRID )
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_SCEN_GRID );
            if( !Item::PropertyIsNoBlock->GetValue< bool >( &proto_item->ItemPropsEntity ) )
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_BLOCK );
            if( !Item::PropertyIsShootThru->GetValue< bool >( &proto_item->ItemPropsEntity ) )
            {
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_BLOCK );
                SETFLAG( HexFlags[ hy * maxhx + hx ], FH_NOTRAKE );
            }
            SETFLAG( HexFlags[ hy * maxhx + hx ], FH_SCEN );

            // To server
            if( pid == SP_MISC_SCRBLOCK )
            {
                // Block around
                for( int k = 0; k < 6; k++ )
                {
                    ushort hx_ = hx, hy_ = hy;
                    MoveHexByDir( hx_, hy_, k, Header.MaxHexX, Header.MaxHexY );
                    SETFLAG( HexFlags[ hy_ * maxhx + hx_ ], FH_BLOCK );
                }
            }
            else if( type == ITEM_TYPE_GRID )
            {
                mobj.AddRef();
                GridsVec.push_back( &mobj );
            }
            else                     // ITEM_TYPE_GENERIC
            {
                // Bind script
                if( proto_item->ScriptName.length() > 0 )
                {
                    Str::Copy( mobj.ScriptName, proto_item->ScriptName.c_str() );
                    Str::Copy( mobj.FuncName, "" );
                }

                // Add to collection
                mobj.AddRef();
                SceneryVec.push_back( &mobj );
            }

            if( pid == SP_SCEN_TRIGGER )
            {
                if( mobj.RunTime.BindScriptId )
                    SETFLAG( HexFlags[ hy * maxhx + hx ], FH_TRIGGER );
                continue;
            }

            // To client
            SceneryCl cur_scen;
            memzero( &cur_scen, sizeof( SceneryCl ) );

            // Flags
            if( type == ITEM_TYPE_GENERIC && mobj.MScenery.CanUse )
                SETFLAG( cur_scen.Flags, SCEN_CAN_USE );
            if( type == ITEM_TYPE_GENERIC && mobj.MScenery.CanTalk )
                SETFLAG( cur_scen.Flags, SCEN_CAN_TALK );
            if( type == ITEM_TYPE_GRID && proto_item->GetGrid_Type() != GRID_EXITGRID )
                SETFLAG( cur_scen.Flags, SCEN_CAN_USE );

            // Other
            cur_scen.ProtoId = mobj.ProtoId;
            cur_scen.MapX = mobj.MapX;
            cur_scen.MapY = mobj.MapY;
            cur_scen.OffsetX = mobj.MScenery.OffsetX;
            cur_scen.OffsetY = mobj.MScenery.OffsetY;
            cur_scen.LightColor = mobj.LightColor;
            cur_scen.LightDistance = mobj.LightDistance;
            cur_scen.LightFlags = mobj.LightDirOff | ( ( mobj.LightDay & 3 ) << 6 );
            cur_scen.LightIntensity = mobj.LightIntensity;
            cur_scen.PicMap = mobj.MScenery.PicMap;
            cur_scen.SpriteCut = mobj.MScenery.SpriteCut;

            SceneriesToSend.push_back( cur_scen );
        }
        break;
        default:
            break;
        }
    }

    for( auto it = mobjects.begin(), end = mobjects.end(); it != end; ++it )
        SAFEREL( *it );
    mobjects.clear();

    // Generate hashes
    HashTiles = maxhx * maxhy;
    if( Tiles.size() )
        Crypt.Crc32( (uchar*) &Tiles[ 0 ], (uint) Tiles.size() * sizeof( Tile ), HashTiles );
    HashWalls = maxhx * maxhy;
    if( WallsToSend.size() )
        Crypt.Crc32( (uchar*) &WallsToSend[ 0 ], (uint) WallsToSend.size() * sizeof( SceneryCl ), HashWalls );
    HashScen = maxhx * maxhy;
    if( SceneriesToSend.size() )
        Crypt.Crc32( (uchar*) &SceneriesToSend[ 0 ], (uint) SceneriesToSend.size() * sizeof( SceneryCl ), HashScen );

    // Shrink the vector capacities to fit their contents and reduce memory use
    SceneryClVec( SceneriesToSend ).swap( SceneriesToSend );
    SceneryClVec( WallsToSend ).swap( WallsToSend );
    EntiresVec( mapEntires ).swap( mapEntires );
    MapObjectPtrVec( CrittersVec ).swap( CrittersVec );
    MapObjectPtrVec( ItemsVec ).swap( ItemsVec );
    MapObjectPtrVec( SceneryVec ).swap( SceneryVec );
    MapObjectPtrVec( GridsVec ).swap( GridsVec );
    TileVec( Tiles ).swap( Tiles );

    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) SceneriesToSend.capacity() * sizeof( SceneryCl ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) WallsToSend.capacity() * sizeof( SceneryCl ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) mapEntires.capacity() * sizeof( MapEntire ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) CrittersVec.size() * sizeof( MapObject ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) ItemsVec.size() * sizeof( MapObject ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) SceneryVec.size() * sizeof( MapObject ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) GridsVec.size() * sizeof( MapObject ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) Header.MaxHexX * Header.MaxHexY );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) Tiles.capacity() * sizeof( Tile ) );
    #endif

    #ifdef FONLINE_MAPPER
    // Post process objects
    for( uint i = 0, j = (uint) mobjects.size(); i < j; i++ )
    {
        MapObject* mobj = mobjects[ i ];

        // Map link
        mobj->RunTime.FromMap = this;

        // Convert hashes to names
        if( mobj->MapObjType == MAP_OBJECT_ITEM || mobj->MapObjType == MAP_OBJECT_SCENERY )
        {
            if( mobj->MItem.PicMap && !mobj->RunTime.PicMapName[ 0 ] )
                Str::Copy( mobj->RunTime.PicMapName, Str::GetName( mobj->MItem.PicMap ) );
            if( mobj->MItem.PicInv && !mobj->RunTime.PicInvName[ 0 ] )
                Str::Copy( mobj->RunTime.PicInvName, Str::GetName( mobj->MItem.PicInv ) );
        }

        // Last UID
        if( mobj->UID > LastObjectUID )
            LastObjectUID = mobj->UID;
    }

    // Create cached fields
    TilesField.resize( Header.MaxHexX * Header.MaxHexY );
    RoofsField.resize( Header.MaxHexX * Header.MaxHexY );
    for( uint i = 0, j = (uint) Tiles.size(); i < j; i++ )
    {
        if( !Tiles[ i ].IsRoof )
        {
            TilesField[ Tiles[ i ].HexY * Header.MaxHexX + Tiles[ i ].HexX ].push_back( Tiles[ i ] );
            TilesField[ Tiles[ i ].HexY * Header.MaxHexX + Tiles[ i ].HexX ].back().IsSelected = false;
        }
        else
        {
            RoofsField[ Tiles[ i ].HexY * Header.MaxHexX + Tiles[ i ].HexX ].push_back( Tiles[ i ] );
            RoofsField[ Tiles[ i ].HexY * Header.MaxHexX + Tiles[ i ].HexX ].back().IsSelected = false;
        }
    }
    Tiles.clear();
    TileVec( Tiles ).swap( Tiles );

    MObjects = mobjects;
    #endif

    return true;
}

#ifdef FONLINE_MAPPER
void WriteStr( FileManager& fm, const char* format, const char* field, ScriptString* str )
{
    if( !str || str->length() == 0 )
        return;

    char buf[ MAX_FOTEXT ];
    Str::Copy( buf, str->c_str() );
    Str::Trim( buf );

    if( Str::Length( buf ) )
        fm.SetStr( format, field, buf );
}

void ProtoMap::SaveTextFormat( FileManager& fm )
{
    // Header
    fm.SetStr( "[%s]\n", APP_HEADER );
    fm.SetStr( "%-20s %d\n", "Version", Header.Version );
    fm.SetStr( "%-20s %d\n", "MaxHexX", Header.MaxHexX );
    fm.SetStr( "%-20s %d\n", "MaxHexY", Header.MaxHexY );
    fm.SetStr( "%-20s %d\n", "WorkHexX", Header.WorkHexX );
    fm.SetStr( "%-20s %d\n", "WorkHexY", Header.WorkHexY );
    fm.SetStr( "%-20s %s\n", "ScriptModule", Header.ScriptModule[ 0 ] ? Header.ScriptModule : "-" );
    fm.SetStr( "%-20s %s\n", "ScriptFunc", Header.ScriptFunc[ 0 ] ? Header.ScriptFunc : "-" );
    fm.SetStr( "%-20s %d\n", "NoLogOut", Header.NoLogOut );
    fm.SetStr( "%-20s %d\n", "Time", Header.Time );
    fm.SetStr( "%-20s %-4d %-4d %-4d %-4d\n", "DayTime", Header.DayTime[ 0 ], Header.DayTime[ 1 ], Header.DayTime[ 2 ], Header.DayTime[ 3 ] );
    fm.SetStr( "%-20s %-3d %-3d %-3d\n", "DayColor0", Header.DayColor[ 0 ], Header.DayColor[ 4 ], Header.DayColor[ 8 ] );
    fm.SetStr( "%-20s %-3d %-3d %-3d\n", "DayColor1", Header.DayColor[ 1 ], Header.DayColor[ 5 ], Header.DayColor[ 9 ] );
    fm.SetStr( "%-20s %-3d %-3d %-3d\n", "DayColor2", Header.DayColor[ 2 ], Header.DayColor[ 6 ], Header.DayColor[ 10 ] );
    fm.SetStr( "%-20s %-3d %-3d %-3d\n", "DayColor3", Header.DayColor[ 3 ], Header.DayColor[ 7 ], Header.DayColor[ 11 ] );
    fm.SetStr( "\n" );

    // Tiles
    fm.SetStr( "[%s]\n", APP_TILES );
    char tile_str[ 256 ];
    for( uint i = 0, j = (uint) Tiles.size(); i < j; i++ )
    {
        Tile&       tile = Tiles[ i ];
        const char* name = Str::GetName( tile.Name );
        if( name )
        {
            bool has_offs = ( tile.OffsX || tile.OffsY );
            bool has_layer = ( tile.Layer != 0 );

            Str::Copy( tile_str, tile.IsRoof ? "roof" : "tile" );
            if( has_offs || has_layer )
                Str::Append( tile_str, "_" );
            if( has_offs )
                Str::Append( tile_str, "o" );
            if( has_layer )
                Str::Append( tile_str, "l" );

            fm.SetStr( "%-10s %-4d %-4d ", tile_str, tile.HexX, tile.HexY );

            if( has_offs )
                fm.SetStr( "%-3d %-3d ", tile.OffsX, tile.OffsY );
            else
                fm.SetStr( "        " );

            if( has_layer )
                fm.SetStr( "%d ", tile.Layer );
            else
                fm.SetStr( "  " );

            fm.SetStr( "%s\n", name );
        }
    }
    fm.SetStr( "\n" );

    // Objects
    fm.SetStr( "[%s]\n", APP_OBJECTS );
    for( uint k = 0; k < MObjects.size(); k++ )
    {
        MapObject& mobj = *MObjects[ k ];
        // Shared
        fm.SetStr( "%-20s %d\n", "MapObjType", mobj.MapObjType );
        fm.SetStr( "%-20s %s\n", "ProtoName", mobj.ProtoName->c_str() );
        if( mobj.MapX )
            fm.SetStr( "%-20s %d\n", "MapX", mobj.MapX );
        if( mobj.MapY )
            fm.SetStr( "%-20s %d\n", "MapY", mobj.MapY );
        if( mobj.UID )
            fm.SetStr( "%-20s %d\n", "UID", mobj.UID );
        if( mobj.ContainerUID )
            fm.SetStr( "%-20s %d\n", "ContainerUID", mobj.ContainerUID );
        if( mobj.ParentUID )
            fm.SetStr( "%-20s %d\n", "ParentUID", mobj.ParentUID );
        if( mobj.ParentChildIndex )
            fm.SetStr( "%-20s %d\n", "ParentChildIndex", mobj.ParentChildIndex );
        if( mobj.LightColor )
            fm.SetStr( "%-20s %d\n", "LightColor", mobj.LightColor );
        if( mobj.LightDay )
            fm.SetStr( "%-20s %d\n", "LightDay", mobj.LightDay );
        if( mobj.LightDirOff )
            fm.SetStr( "%-20s %d\n", "LightDirOff", mobj.LightDirOff );
        if( mobj.LightDistance )
            fm.SetStr( "%-20s %d\n", "LightDistance", mobj.LightDistance );
        if( mobj.LightIntensity )
            fm.SetStr( "%-20s %d\n", "LightIntensity", mobj.LightIntensity );
        if( mobj.ScriptName[ 0 ] )
            fm.SetStr( "%-20s %s\n", "ScriptName", mobj.ScriptName );
        if( mobj.FuncName[ 0 ] )
            fm.SetStr( "%-20s %s\n", "FuncName", mobj.FuncName );
        if( mobj.UserData[ 0 ] )
            fm.SetStr( "%-20s %d\n", "UserData0", mobj.UserData[ 0 ] );
        if( mobj.UserData[ 1 ] )
            fm.SetStr( "%-20s %d\n", "UserData1", mobj.UserData[ 1 ] );
        if( mobj.UserData[ 2 ] )
            fm.SetStr( "%-20s %d\n", "UserData2", mobj.UserData[ 2 ] );
        if( mobj.UserData[ 3 ] )
            fm.SetStr( "%-20s %d\n", "UserData3", mobj.UserData[ 3 ] );
        if( mobj.UserData[ 4 ] )
            fm.SetStr( "%-20s %d\n", "UserData4", mobj.UserData[ 4 ] );
        if( mobj.UserData[ 5 ] )
            fm.SetStr( "%-20s %d\n", "UserData5", mobj.UserData[ 5 ] );
        if( mobj.UserData[ 6 ] )
            fm.SetStr( "%-20s %d\n", "UserData6", mobj.UserData[ 6 ] );
        if( mobj.UserData[ 7 ] )
            fm.SetStr( "%-20s %d\n", "UserData7", mobj.UserData[ 7 ] );
        if( mobj.UserData[ 8 ] )
            fm.SetStr( "%-20s %d\n", "UserData8", mobj.UserData[ 8 ] );
        if( mobj.UserData[ 9 ] )
            fm.SetStr( "%-20s %d\n", "UserData9", mobj.UserData[ 9 ] );
        // Critter
        if( mobj.MapObjType == MAP_OBJECT_CRITTER )
        {
            if( mobj.MCritter.Dir )
                fm.SetStr( "%-20s %d\n", "Critter_Dir", mobj.MCritter.Dir );
            if( mobj.MCritter.Cond )
                fm.SetStr( "%-20s %d\n", "Critter_Cond", mobj.MCritter.Cond );
            if( mobj.MCritter.Anim1 )
                fm.SetStr( "%-20s %d\n", "Critter_Anim1", mobj.MCritter.Anim1 );
            if( mobj.MCritter.Anim2 )
                fm.SetStr( "%-20s %d\n", "Critter_Anim2", mobj.MCritter.Anim2 );
            if( mobj.Props )
            {
                for( size_t i = 0, j = mobj.Props->size(); i < j; i += 2 )
                {
                    if( mobj.Props->at( i )->length() > 0 && mobj.Props->at( i + 1 )->length() > 0 )
                    {
                        char property_name[ MAX_FOTEXT ];
                        char property_value[ MAX_FOTEXT ];
                        Str::Copy( property_name, mobj.Props->at( i )->c_str() );
                        Str::Copy( property_value, mobj.Props->at( i + 1 )->c_str() );
                        Str::Trim( property_name );
                        Str::Trim( property_value );
                        if( Str::Length( property_name ) > 0 && Str::Length( property_value ) > 0 )
                        {
                            fm.SetStr( "%-20s %s\n", "Critter_ParamIndex", property_name );
                            fm.SetStr( "%-20s %s\n", "Critter_ParamValue", property_value );
                        }
                    }
                }
            }
        }
        // Item
        else if( mobj.MapObjType == MAP_OBJECT_ITEM || mobj.MapObjType == MAP_OBJECT_SCENERY )
        {
            if( mobj.MItem.OffsetX )
                fm.SetStr( "%-20s %d\n", "OffsetX", mobj.MItem.OffsetX );
            if( mobj.MItem.OffsetY )
                fm.SetStr( "%-20s %d\n", "OffsetY", mobj.MItem.OffsetY );
            if( mobj.RunTime.PicMapName[ 0 ] )
                fm.SetStr( "%-20s %s\n", "PicMapName", mobj.RunTime.PicMapName );
            if( mobj.RunTime.PicInvName[ 0 ] )
                fm.SetStr( "%-20s %s\n", "PicInvName", mobj.RunTime.PicInvName );
            if( mobj.MapObjType == MAP_OBJECT_ITEM )
            {
                if( mobj.MItem.Count )
                    fm.SetStr( "%-20s %d\n", "Item_Count", mobj.MItem.Count );
                if( mobj.MItem.BrokenFlags )
                    fm.SetStr( "%-20s %d\n", "Item_BrokenFlags", mobj.MItem.BrokenFlags );
                if( mobj.MItem.BrokenCount )
                    fm.SetStr( "%-20s %d\n", "Item_BrokenCount", mobj.MItem.BrokenCount );
                if( mobj.MItem.Deterioration )
                    fm.SetStr( "%-20s %d\n", "Item_Deterioration", mobj.MItem.Deterioration );
                if( mobj.MItem.ItemSlot )
                    fm.SetStr( "%-20s %d\n", "Item_ItemSlot", mobj.MItem.ItemSlot );
                if( mobj.MItem.AmmoPid )
                    WriteStr( fm, "%-20s %s\n", "Item_AmmoPid", mobj.MItem.AmmoPid );
                if( mobj.MItem.AmmoCount )
                    fm.SetStr( "%-20s %d\n", "Item_AmmoCount", mobj.MItem.AmmoCount );
                if( mobj.MItem.LockerDoorId )
                    fm.SetStr( "%-20s %d\n", "Item_LockerDoorId", mobj.MItem.LockerDoorId );
                if( mobj.MItem.LockerCondition )
                    fm.SetStr( "%-20s %d\n", "Item_LockerCondition", mobj.MItem.LockerCondition );
                if( mobj.MItem.LockerComplexity )
                    fm.SetStr( "%-20s %d\n", "Item_LockerComplexity", mobj.MItem.LockerComplexity );
                if( mobj.MItem.TrapValue )
                    fm.SetStr( "%-20s %d\n", "Item_TrapValue", mobj.MItem.TrapValue );
                if( mobj.MItem.Val[ 0 ] )
                    fm.SetStr( "%-20s %d\n", "Item_Val0", mobj.MItem.Val[ 0 ] );
                if( mobj.MItem.Val[ 1 ] )
                    fm.SetStr( "%-20s %d\n", "Item_Val1", mobj.MItem.Val[ 1 ] );
                if( mobj.MItem.Val[ 2 ] )
                    fm.SetStr( "%-20s %d\n", "Item_Val2", mobj.MItem.Val[ 2 ] );
                if( mobj.MItem.Val[ 3 ] )
                    fm.SetStr( "%-20s %d\n", "Item_Val3", mobj.MItem.Val[ 3 ] );
                if( mobj.MItem.Val[ 4 ] )
                    fm.SetStr( "%-20s %d\n", "Item_Val4", mobj.MItem.Val[ 4 ] );
                if( mobj.MItem.Val[ 5 ] )
                    fm.SetStr( "%-20s %d\n", "Item_Val5", mobj.MItem.Val[ 5 ] );
                if( mobj.MItem.Val[ 6 ] )
                    fm.SetStr( "%-20s %d\n", "Item_Val6", mobj.MItem.Val[ 6 ] );
                if( mobj.MItem.Val[ 7 ] )
                    fm.SetStr( "%-20s %d\n", "Item_Val7", mobj.MItem.Val[ 7 ] );
                if( mobj.MItem.Val[ 8 ] )
                    fm.SetStr( "%-20s %d\n", "Item_Val8", mobj.MItem.Val[ 8 ] );
                if( mobj.MItem.Val[ 9 ] )
                    fm.SetStr( "%-20s %d\n", "Item_Val9", mobj.MItem.Val[ 9 ] );
            }
            // Scenery
            else if( mobj.MapObjType == MAP_OBJECT_SCENERY )
            {
                if( mobj.MScenery.CanUse )
                    fm.SetStr( "%-20s %d\n", "Scenery_CanUse", mobj.MScenery.CanUse );
                if( mobj.MScenery.CanTalk )
                    fm.SetStr( "%-20s %d\n", "Scenery_CanTalk", mobj.MScenery.CanTalk );
                if( mobj.MScenery.TriggerNum )
                    fm.SetStr( "%-20s %d\n", "Scenery_TriggerNum", mobj.MScenery.TriggerNum );
                if( mobj.MScenery.ParamsCount )
                    fm.SetStr( "%-20s %d\n", "Scenery_ParamsCount", mobj.MScenery.ParamsCount );
                if( mobj.MScenery.Param[ 0 ] )
                    fm.SetStr( "%-20s %d\n", "Scenery_Param0", mobj.MScenery.Param[ 0 ] );
                if( mobj.MScenery.Param[ 1 ] )
                    fm.SetStr( "%-20s %d\n", "Scenery_Param1", mobj.MScenery.Param[ 1 ] );
                if( mobj.MScenery.Param[ 2 ] )
                    fm.SetStr( "%-20s %d\n", "Scenery_Param2", mobj.MScenery.Param[ 2 ] );
                if( mobj.MScenery.Param[ 3 ] )
                    fm.SetStr( "%-20s %d\n", "Scenery_Param3", mobj.MScenery.Param[ 3 ] );
                if( mobj.MScenery.Param[ 4 ] )
                    fm.SetStr( "%-20s %d\n", "Scenery_Param4", mobj.MScenery.Param[ 4 ] );
                if( mobj.MScenery.ToMap )
                    WriteStr( fm, "%-20s %s\n", "Scenery_ToMap", mobj.MScenery.ToMap );
                if( mobj.MScenery.ToEntire )
                    fm.SetStr( "%-20s %d\n", "Scenery_ToEntire", mobj.MScenery.ToEntire );
                if( mobj.MScenery.ToDir )
                    fm.SetStr( "%-20s %d\n", "Scenery_ToDir", mobj.MScenery.ToDir );
                if( mobj.MScenery.SpriteCut )
                    fm.SetStr( "%-20s %d\n", "Scenery_SpriteCut", mobj.MScenery.SpriteCut );
            }
        }
        fm.SetStr( "\n" );
    }
    fm.SetStr( "\n" );
}
#endif

#ifdef FONLINE_SERVER
bool ProtoMap::LoadCache( FileManager& fm )
{
    // Server version
    uint version = fm.GetBEUInt();
    if( version != FONLINE_VERSION )
        return false;
    fm.GetBEUInt();
    fm.GetBEUInt();
    fm.GetBEUInt();

    // Header
    if( !fm.CopyMem( &Header, sizeof( Header ) ) )
        return false;

    // Tiles
    uint tiles_count = fm.GetBEUInt();
    if( tiles_count )
    {
        Tiles.resize( tiles_count );
        fm.CopyMem( &Tiles[ 0 ], tiles_count * sizeof( Tile ) );
    }

    // Critters
    uint count = fm.GetBEUInt();
    CrittersVec.reserve( count );
    for( uint i = 0; i < count; i++ )
    {
        MapObject* mobj = new MapObject();
        fm.CopyMem( mobj, sizeof( MapObject ) - sizeof( MapObject::_RunTime ) );
        mobj->RunTime.RefCounter = 1;
        if( mobj->Props )
        {
            mobj->AllocateProps();
            uint size = fm.GetBEUInt();
            mobj->Props = new IntVec();
            mobj->Props->resize( size );
            fm.CopyMem( &mobj->Props->at( 0 ), size * sizeof( int ) );
        }
        CrittersVec.push_back( mobj );
    }

    // Items
    count = fm.GetBEUInt();
    ItemsVec.reserve( count );
    for( uint i = 0; i < count; i++ )
    {
        MapObject* mobj = new MapObject();
        fm.CopyMem( mobj, sizeof( MapObject ) - sizeof( MapObject::_RunTime ) );
        mobj->RunTime.RefCounter = 1;
        ItemsVec.push_back( mobj );
    }

    // Scenery
    count = fm.GetBEUInt();
    SceneryVec.reserve( count );
    for( uint i = 0; i < count; i++ )
    {
        MapObject* mobj = new MapObject();
        fm.CopyMem( mobj, sizeof( MapObject ) );
        mobj->RunTime.RefCounter = 1;
        SceneryVec.push_back( mobj );
    }

    // Grids
    count = fm.GetBEUInt();
    GridsVec.reserve( count );
    for( uint i = 0; i < count; i++ )
    {
        MapObject* mobj = new MapObject();
        fm.CopyMem( mobj, sizeof( MapObject ) );
        mobj->RunTime.RefCounter = 1;
        GridsVec.push_back( mobj );
    }

    // Bind scripts
    MapObjectPtrVec mobjects;
    mobjects.insert( mobjects.end(), CrittersVec.begin(), CrittersVec.end() );
    mobjects.insert( mobjects.end(), ItemsVec.begin(), ItemsVec.end() );
    mobjects.insert( mobjects.end(), SceneryVec.begin(), SceneryVec.end() );
    mobjects.insert( mobjects.end(), GridsVec.begin(), GridsVec.end() );
    if( !BindScripts( mobjects ) )
        return false;

    // To send
    count = fm.GetBEUInt();
    if( count )
    {
        WallsToSend.resize( count );
        if( !fm.CopyMem( &WallsToSend[ 0 ], count * sizeof( SceneryCl ) ) )
            return false;
    }
    count = fm.GetBEUInt();
    if( count )
    {
        SceneriesToSend.resize( count );
        if( !fm.CopyMem( &SceneriesToSend[ 0 ], count * sizeof( SceneryCl ) ) )
            return false;
    }

    // Hashes
    HashTiles = fm.GetBEUInt();
    HashWalls = fm.GetBEUInt();
    HashScen = fm.GetBEUInt();

    // Hex flags
    HexFlags = new uchar[ Header.MaxHexX * Header.MaxHexY ];
    if( !HexFlags )
        return false;
    if( !fm.CopyMem( HexFlags, Header.MaxHexX * Header.MaxHexY ) )
        return false;

    // Entires
    count = fm.GetBEUInt();
    if( count )
    {
        mapEntires.resize( count );
        if( !fm.CopyMem( &mapEntires[ 0 ], count * sizeof( MapEntire ) ) )
            return false;
    }

    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) SceneriesToSend.capacity() * sizeof( SceneryCl ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) WallsToSend.capacity() * sizeof( SceneryCl ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) mapEntires.capacity() * sizeof( MapEntire ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) CrittersVec.size() * sizeof( MapObject ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) ItemsVec.size() * sizeof( MapObject ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) SceneryVec.size() * sizeof( MapObject ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) GridsVec.size() * sizeof( MapObject ) );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) Header.MaxHexX * Header.MaxHexY );
    MEMORY_PROCESS( MEMORY_PROTO_MAP, (int) Tiles.capacity() * sizeof( Tile ) );
    return true;
}

void ProtoMap::SaveCache( FileManager& fm )
{
    // Server version
    fm.SetBEUInt( FONLINE_VERSION );
    fm.SetBEUInt( 0 );
    fm.SetBEUInt( 0 );
    fm.SetBEUInt( 0 );

    // Header
    fm.SetData( &Header, sizeof( Header ) );

    // Tiles
    fm.SetBEUInt( (uint) Tiles.size() );
    if( Tiles.size() )
        fm.SetData( &Tiles[ 0 ], (uint) Tiles.size() * sizeof( Tile ) );

    // Critters
    fm.SetBEUInt( (uint) CrittersVec.size() );
    for( auto it = CrittersVec.begin(), end = CrittersVec.end(); it != end; ++it )
    {
        fm.SetData( *it, (uint) sizeof( MapObject ) - sizeof( MapObject::_RunTime ) );
        if( ( *it )->Props && !( *it )->Props->empty() )
        {
            uint size = ( *it )->Props->size();
            fm.SetBEUInt( size );
            fm.SetData( &( *it )->Props->at( 0 ), size * sizeof( int ) );
        }
    }

    // Items
    fm.SetBEUInt( (uint) ItemsVec.size() );
    for( auto it = ItemsVec.begin(), end = ItemsVec.end(); it != end; ++it )
        fm.SetData( *it, (uint) sizeof( MapObject ) - sizeof( MapObject::_RunTime ) );

    // Scenery
    fm.SetBEUInt( (uint) SceneryVec.size() );
    for( auto it = SceneryVec.begin(), end = SceneryVec.end(); it != end; ++it )
        fm.SetData( *it, (uint) sizeof( MapObject ) );

    // Grids
    fm.SetBEUInt( (uint) GridsVec.size() );
    for( auto it = GridsVec.begin(), end = GridsVec.end(); it != end; ++it )
        fm.SetData( *it, (uint) sizeof( MapObject ) );

    // To send
    fm.SetBEUInt( (uint) WallsToSend.size() );
    fm.SetData( &WallsToSend[ 0 ], (uint) WallsToSend.size() * sizeof( SceneryCl ) );
    fm.SetBEUInt( (uint) SceneriesToSend.size() );
    fm.SetData( &SceneriesToSend[ 0 ], (uint) SceneriesToSend.size() * sizeof( SceneryCl ) );

    // Hashes
    fm.SetBEUInt( HashTiles );
    fm.SetBEUInt( HashWalls );
    fm.SetBEUInt( HashScen );

    // Hex flags
    fm.SetData( HexFlags, Header.MaxHexX * Header.MaxHexY );

    // Entires
    fm.SetBEUInt( (uint) mapEntires.size() );
    fm.SetData( &mapEntires[ 0 ], (uint) mapEntires.size() * sizeof( MapEntire ) );

    // Save
    char fname[ MAX_FOPATH ];
    Str::Format( fname, "%s.fomapb", pmapName.c_str() );
    fm.SaveOutBufToFile( fname, PT_SERVER_CACHE );
}

bool ProtoMap::BindScripts( MapObjectPtrVec& mobjs )
{
    int errors = 0;

    // Map script
    if( Header.ScriptModule[ 0 ] || Header.ScriptFunc[ 0 ] )
    {
        char script_name[ MAX_FOTEXT ];
        Str::Copy( script_name, Header.ScriptModule );
        if( script_name[ 0 ] )
            Str::Append( script_name, "@" );
        Str::Append( script_name, Header.ScriptFunc );
        if( script_name[ 0 ] )
        {
            hash func_num = Script::BindScriptFuncNumByScriptName( script_name, "void %s(Map&,bool)" );
            if( !func_num )
            {
                WriteLog( "Map '%s', can't bind map function '%s'.\n", pmapName.c_str(), script_name );
                errors++;
            }
        }
    }

    // Objects scripts
    for( auto& mobj : mobjs )
    {
        mobj->RunTime.BindScriptId = 0;

        char script_name[ MAX_FOTEXT ];
        Str::Copy( script_name, mobj->ScriptName );
        if( script_name[ 0 ] )
            Str::Append( script_name, "@" );
        Str::Append( script_name, mobj->FuncName );
        if( !script_name[ 0 ] )
            continue;

        if( mobj->MapObjType == MAP_OBJECT_CRITTER )
        {
            hash func_num = Script::BindScriptFuncNumByScriptName( script_name, "void %s(Critter&,bool)" );
            if( !func_num )
            {
                WriteLog( "Map '%s', can't bind critter function '%s'.\n", pmapName.c_str(), script_name );
                errors++;
            }
        }
        else if( mobj->MapObjType == MAP_OBJECT_ITEM )
        {
            hash func_num = Script::BindScriptFuncNumByScriptName( script_name, "void %s(Item&,bool)" );
            if( !func_num )
            {
                WriteLog( "Map '%s', can't bind item function '%s'.\n", pmapName.c_str(), script_name );
                errors++;
            }
        }
        else if( mobj->MapObjType == MAP_OBJECT_SCENERY )
        {
            # define BIND_SCENERY_FUNC( params )                                                                                                            \
                if( mobj->ProtoId != SP_SCEN_TRIGGER )                                                                                                      \
                    mobj->RunTime.BindScriptId = Script::BindByScriptName( script_name, "bool %s(Critter&,Scenery&,CritterProperty,Item@" params, false );  \
                else                                                                                                                                        \
                    mobj->RunTime.BindScriptId = Script::BindByScriptName( script_name, "void %s(Critter&,Scenery&,bool,uint8" params, false )
            switch( mobj->MScenery.ParamsCount )
            {
            case 1:
                BIND_SCENERY_FUNC( ",int)" );
                break;
            case 2:
                BIND_SCENERY_FUNC( ",int,int)" );
                break;
            case 3:
                BIND_SCENERY_FUNC( ",int,int,int)" );
                break;
            case 4:
                BIND_SCENERY_FUNC( ",int,int,int,int)" );
                break;
            case 5:
                BIND_SCENERY_FUNC( ",int,int,int,int,int)" );
                break;
            default:
                BIND_SCENERY_FUNC( ")" );
                break;
            }
            # undef BIND_SCENERY_FUNC

            if( !mobj->RunTime.BindScriptId )
            {
                WriteLog( "Map '%s', can't bind scenery function '%s'.\n", pmapName.c_str(), script_name );
                errors++;
            }
        }
    }
    return errors == 0;
}
#endif // FONLINE_SERVER

bool ProtoMap::Refresh()
{
    // Load text or binary
    FilesCollection maps( "fomap" );
    const char* path;
    FileManager& map_file = maps.FindFile( pmapName.c_str(), &path );
    if( !map_file.IsLoaded() )
    {
        WriteLog( "Map '%s' not found.\n", pmapName.c_str() );
        return false;
    }

    // Store path
    char dir[ MAX_FOPATH ];
    FileManager::ExtractDir( path, dir );
    pmapDir = dir;

    // Load from cache
    #ifdef FONLINE_SERVER
    FileManager cached;
    if( cached.LoadFile( ( pmapName + ".fomapb" ).c_str(), PT_SERVER_CACHE ) )
    {
        uint64 last_write = map_file.GetWriteTime();
        uint64 last_write_cache = cached.GetWriteTime();
        if( last_write_cache >= last_write )
        {
            if( LoadCache( cached ) )
                return true;
        }
    }
    #endif

    // Load from file
    if( !LoadTextFormat( (const char*) map_file.GetBuf() ) )
    {
        WriteLog( "Unable to load map '%s'.\n", pmapName.c_str() );
        return false;
    }

    #ifdef FONLINE_SERVER
    SaveCache( map_file );
    #endif
    return true;
}

#ifdef FONLINE_MAPPER
void ProtoMap::GenNew()
{
    Clear();
    Header.Version = FO_MAP_VERSION_TEXT7;
    Header.MaxHexX = MAXHEX_DEF;
    Header.MaxHexY = MAXHEX_DEF;
    pmapName = "";
    pmapPid = 0;

    // Morning	 5.00 -  9.59	 300 - 599
    // Day		10.00 - 18.59	 600 - 1139
    // Evening	19.00 - 22.59	1140 - 1379
    // Nigh		23.00 -  4.59	1380
    Header.DayTime[ 0 ] = 300;
    Header.DayTime[ 1 ] = 600;
    Header.DayTime[ 2 ] = 1140;
    Header.DayTime[ 3 ] = 1380;
    Header.DayColor[ 0 ] = 18;
    Header.DayColor[ 1 ] = 128;
    Header.DayColor[ 2 ] = 103;
    Header.DayColor[ 3 ] = 51;
    Header.DayColor[ 4 ] = 18;
    Header.DayColor[ 5 ] = 128;
    Header.DayColor[ 6 ] = 95;
    Header.DayColor[ 7 ] = 40;
    Header.DayColor[ 8 ] = 53;
    Header.DayColor[ 9 ] = 128;
    Header.DayColor[ 10 ] = 86;
    Header.DayColor[ 11 ] = 29;

    # ifdef FONLINE_MAPPER
    TilesField.resize( Header.MaxHexX * Header.MaxHexY );
    RoofsField.resize( Header.MaxHexX * Header.MaxHexY );
    # endif
}

bool ProtoMap::Save( const char* custom_name /* = NULL */ )
{
    // Fill tiles from cached fields
    TilesField.resize( Header.MaxHexX * Header.MaxHexY );
    RoofsField.resize( Header.MaxHexX * Header.MaxHexY );
    for( ushort hy = 0; hy < Header.MaxHexY; hy++ )
    {
        for( ushort hx = 0; hx < Header.MaxHexX; hx++ )
        {
            TileVec& tiles = TilesField[ hy * Header.MaxHexX + hx ];
            for( uint i = 0, j = (uint) tiles.size(); i < j; i++ )
                Tiles.push_back( tiles[ i ] );
            TileVec& roofs = RoofsField[ hy * Header.MaxHexX + hx ];
            for( uint i = 0, j = (uint) roofs.size(); i < j; i++ )
                Tiles.push_back( roofs[ i ] );
        }
    }

    // Delete non used UIDs
    for( uint i = 0, j = (uint) MObjects.size(); i < j; i++ )
    {
        MapObject* mobj = MObjects[ i ];
        if( !mobj->UID )
            continue;

        bool founded = false;
        for( uint k = 0, l = (uint) MObjects.size(); k < l; k++ )
        {
            MapObject* mobj_ = MObjects[ k ];
            if( mobj_->ContainerUID == mobj->UID || mobj_->ParentUID == mobj->UID )
            {
                founded = true;
                break;
            }
        }
        if( !founded )
            mobj->UID = 0;
    }

    // Fill data
    FileManager fm;
    Header.Version = FO_MAP_VERSION_TEXT7;
    SaveTextFormat( fm );
    Tiles.clear();

    // Save
    string save_fname = pmapDir + ( custom_name && *custom_name ? string( custom_name ) : pmapName ) + ".fomap";
    if( !fm.SaveOutBufToFile( save_fname.c_str(), PT_SERVER_MODULES ) )
    {
        WriteLog( "Unable write file.\n" );
        fm.ClearOutBuf();
        return false;
    }

    fm.ClearOutBuf();
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
        if( !txt.AppendFile( fname, PT_ROOT ) )
            return false;
        return txt.IsApp( APP_HEADER ) && txt.IsApp( APP_TILES ) && txt.IsApp( APP_OBJECTS );
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
    for( uint i = 0, j = (uint) mapEntires.size(); i < j; i++ )
    {
        MapEntire& entire = mapEntires[ i ];
        if( num == uint( -1 ) || entire.Number == num )
            entires.push_back( entire );
    }
}

MapObject* ProtoMap::GetMapScenery( ushort hx, ushort hy, hash pid )
{
    for( auto it = SceneryVec.begin(), end = SceneryVec.end(); it != end; ++it )
    {
        MapObject* mobj = *it;
        if( ( !pid || mobj->ProtoId == pid ) && mobj->MapX == hx && mobj->MapY == hy )
            return mobj;
    }
    return nullptr;
}

void ProtoMap::GetMapSceneriesHex( ushort hx, ushort hy, MapObjectPtrVec& mobjs )
{
    for( auto it = SceneryVec.begin(), end = SceneryVec.end(); it != end; ++it )
    {
        MapObject* mobj = *it;
        if( mobj->MapX == hx && mobj->MapY == hy )
            mobjs.push_back( mobj );
    }
}

void ProtoMap::GetMapSceneriesHexEx( ushort hx, ushort hy, uint radius, hash pid, MapObjectPtrVec& mobjs )
{
    for( auto it = SceneryVec.begin(), end = SceneryVec.end(); it != end; ++it )
    {
        MapObject* mobj = *it;
        if( ( !pid || mobj->ProtoId == pid ) && DistGame( mobj->MapX, mobj->MapY, hx, hy ) <= radius )
            mobjs.push_back( mobj );
    }
}

void ProtoMap::GetMapSceneriesByPid( hash pid, MapObjectPtrVec& mobjs )
{
    for( auto it = SceneryVec.begin(), end = SceneryVec.end(); it != end; ++it )
    {
        MapObject* mobj = *it;
        if( !pid || mobj->ProtoId == pid )
            mobjs.push_back( mobj );
    }
}

MapObject* ProtoMap::GetMapGrid( ushort hx, ushort hy )
{
    for( auto it = GridsVec.begin(), end = GridsVec.end(); it != end; ++it )
    {
        MapObject* mobj = *it;
        if( mobj->MapX == hx && mobj->MapY == hy )
            return mobj;
    }
    return nullptr;
}

#endif // FONLINE_SERVER
