#include "StdAfx.h"
#include "ItemManager.h"
#include "ConstantsManager.h"
#include "IniParser.h"

#ifdef FONLINE_SERVER
# include "Critter.h"
# include "Map.h"
# include "CritterManager.h"
# include "MapManager.h"
# include "Script.h"
#endif

#ifdef FONLINE_MAPPER
# include "Script.h"
#endif

ItemManager ItemMngr;

bool ItemManager::Init()
{
    WriteLog( "Item manager initialization...\n" );

    if( IsInit() )
    {
        WriteLog( "already init.\n" );
        return true;
    }

    Clear();
    ClearProtos();

    isActive = true;
    WriteLog( "Item manager initialization complete.\n" );
    return true;
}

void ItemManager::Finish()
{
    WriteLog( "Item manager finish...\n" );

    if( !isActive )
    {
        WriteLog( "already finish or not init.\n" );
        return;
    }

    #ifdef FONLINE_SERVER
    ItemGarbager();

    ItemPtrMap items = gameItems;
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = ( *it ).second;
        item->EventFinish( false );
        item->Release();
    }
    gameItems.clear();
    #endif

    Clear();
    ClearProtos();

    isActive = false;
    WriteLog( "Item manager finish complete.\n" );
}

void ItemManager::Clear()
{
    #ifdef FONLINE_SERVER
    for( auto it = gameItems.begin(), end = gameItems.end(); it != end; ++it )
        SAFEREL( ( *it ).second );
    gameItems.clear();
    radioItems.clear();
    itemToDelete.clear();
    itemToDeleteCount.clear();
    lastItemId = 0;
    #endif

    for( int i = 0; i < MAX_ITEM_PROTOTYPES; i++ )
        itemCount[ i ] = 0;
}

#if defined ( FONLINE_SERVER ) || defined ( FONLINE_OBJECT_EDITOR ) || defined ( FONLINE_MAPPER )

template< class T >
T ResolveProtoValue( const char* str )
{
    if( Str::IsNumber( str ) )
        return ( T ) Str::AtoI64( str );
    else if( Str::Substring( str, "\\" ) || Str::Substring( str, "/" ) )
        return ( T ) Str::GetHash( str );
    return ( T ) ConstantsManager::GetDefineValue( str );
}

bool ItemManager::LoadProtos()
{
    WriteLog( "Loading items prototypes...\n" );

    FileManager fm;
    if( !fm.LoadFile( "items.lst", PT_SERVER_PRO_ITEMS ) )
    {
        WriteLog( "Can't open \"items.lst\".\n" );
        return false;
    }

    char   buf[ 256 ];
    StrVec fnames;
    int    count = 0;
    while( fm.GetLine( buf, sizeof( buf ) ) )
    {
        fnames.push_back( string( buf ) );
        count++;
    }
    fm.UnloadFile();

    uint         loaded = 0;
    ProtoItemVec item_protos;
    ClearProtos();
    for( int i = 0; i < count; i++ )
    {
        char fname[ MAX_FOPATH ];
        Str::Copy( fname, FileManager::GetFullPath( fnames[ i ].c_str(), PT_SERVER_PRO_ITEMS ) );

        char collection_name[ MAX_FOPATH ];
        Str::Format( collection_name, "%03d - %s", i + 1, fnames[ i ].c_str() );
        FileManager::EraseExtension( collection_name );

        if( LoadProtos( item_protos, fname ) )
        {
            ParseProtos( item_protos, collection_name );
            loaded += (uint) item_protos.size();
        }
    }

    WriteLog( "Items prototypes successfully loaded, count<%u>.\n", loaded );
    return true;
}

bool ItemManager::LoadProtos( ProtoItemVec& protos, const char* fname )
{
    protos.clear();

    IniParser fopro;
    if( !fopro.LoadFile( fname, -1 ) )
    {
        WriteLogF( _FUNC_, " - File<%s> not found.\n", fname );
        return false;
    }

    ProtoItem proto_item;
    char      svalue[ MAX_FOTEXT ];
    char      name_[ MAX_FOTEXT ]; // For deprecated conversion
    while( true )
    {
        if( !fopro.GotoNextApp( "Proto" ) )
            break;

        proto_item.Clear();
        bool             deprecated = fopro.GetStr( "Proto", "Pid", "", svalue ); // Detect deprecated by 'Pid', instead new 'ProtoId'
        asIScriptEngine* engine = Script::GetEngine();
        asIObjectType*   ot = engine->GetObjectTypeById( engine->GetTypeIdByDecl( "ProtoItem" ) );

        for( int i = 0, j = ot->GetPropertyCount(); i < j; i++ )
        {
            const char* name;
            int         type_id;
            int         offset;
            ot->GetProperty( i, &name, &type_id, NULL, &offset );

            if( !fopro.GetStr( "Proto", name, "", svalue ) )
            {
                if( deprecated )
                {
                    // Convert '_' to '.' and try again
                    Str::Copy( name_, name );
                    bool  swapped = false;
                    char* str = name_;
                    while( *str )
                    {
                        if( *str == '_' )
                        {
                            *str = '.';
                            swapped = true;
                            break;
                        }
                        ++str;
                    }

                    if( !swapped || !fopro.GetStr( "Proto", name_, "", svalue ) )
                    {
                        if( Str::Compare( name, "ProtoId" ) )
                            Str::Copy( name_, "Pid" );
                        else if( Str::Compare( name, "PicInv" ) || Str::Compare( name_, "PicMap" ) )
                            Str::Append( name_, "Name" );
                        else if( Str::Compare( name, "Armor_CrTypeMale" ) )
                            Str::Copy( name_, "Armor.Anim0Male" );
                        else if( Str::Compare( name, "Armor_CrTypeFemale" ) )
                            Str::Copy( name_, "Armor.Anim0Female" );
                        else if( Str::Compare( name, "Weapon_CriticalFailture" ) )
                            Str::Copy( name_, "Weapon.CrFailture" );
                        else if( Str::Compare( name, "Weapon_MinStrength" ) )
                            Str::Copy( name_, "Weapon.MinSt" );
                        else if( Str::Compare( name, "Stackable" ) )
                            Str::Copy( name_, "Weapon.NoWear" );
                        else if( Str::Compare( name, "Weapon_ActiveUses" ) )
                            Str::Copy( name_, "Weapon.CountAttack" );
                        else if( Str::Compare( name, "Weapon_PicUse_0" ) )
                            Str::Copy( name_, "Weapon.PicName_0" );
                        else if( Str::Compare( name, "Weapon_PicUse_1" ) )
                            Str::Copy( name_, "Weapon.PicName_1" );
                        else if( Str::Compare( name, "Weapon_PicUse_2" ) )
                            Str::Copy( name_, "Weapon.PicName_2" );
                        else if( Str::Compare( name, "Weapon_ApCost_0" ) )
                            Str::Copy( name_, "Weapon.Time_0" );
                        else if( Str::Compare( name, "Weapon_ApCost_1" ) )
                            Str::Copy( name_, "Weapon.Time_1" );
                        else if( Str::Compare( name, "Weapon_ApCost_2" ) )
                            Str::Copy( name_, "Weapon.Time_2" );
                        else if( Str::Compare( name, "Car_Speed" ) )
                            Str::Copy( name_, "MiscEx.Car.Speed" );
                        else if( Str::Compare( name, "Car_Passability" ) )
                            Str::Copy( name_, "MiscEx.Car.Negotiability" );
                        else if( Str::Compare( name, "Car_DeteriorationRate" ) )
                            Str::Copy( name_, "MiscEx.Car.WearConsumption" );
                        else if( Str::Compare( name, "Car_CrittersCapacity" ) )
                            Str::Copy( name_, "MiscEx.Car.CritCapacity" );
                        else if( Str::Compare( name, "Car_TankVolume" ) )
                            Str::Copy( name_, "MiscEx.Car.FuelTank" );
                        else if( Str::Compare( name, "Car_MaxDeterioration" ) )
                            Str::Copy( name_, "MiscEx.Car.RunToBreak" );
                        else if( Str::Compare( name, "Car_FuelConsumption" ) )
                            Str::Copy( name_, "MiscEx.Car.FuelConsumption" );
                        else if( Str::Compare( name, "Car_Entrance" ) )
                            Str::Copy( name_, "MiscEx.Car.Entire" );
                        else if( Str::Compare( name, "Car_MovementType" ) )
                            Str::Copy( name_, "MiscEx.Car.WalkType" );
                        else if( Str::Compare( name, "ChildLines_0" ) )
                            Str::Copy( name_, "MiscEx.Car.Bag_0" );
                        else if( Str::Compare( name, "ChildLines_1" ) )
                            Str::Copy( name_, "MiscEx.Car.Bag_1" );
                        else if( Str::Compare( name, "BlockLines" ) )
                            Str::Copy( name_, "MiscEx.Car.Blocks" );
                        else if( Str::Compare( name, "StartValue_1" ) )
                            Str::Copy( name_, "MiscEx.StartVal1" );
                        else if( Str::Compare( name, "StartValue_2" ) )
                            Str::Copy( name_, "MiscEx.StartVal2" );
                        else if( Str::Compare( name, "StartValue_3" ) )
                            Str::Copy( name_, "MiscEx.StartVal3" );
                        else if( Str::Compare( name, "StartCount" ) )
                            Str::Copy( name_, "Ammo.StartCount" );
                        else if( Str::Compare( name, "Weapon_MaxAmmoCount" ) )
                            Str::Copy( name_, "Weapon.VolHolder" );
                        else if( Str::Compare( name, "Weapon_DefaultAmmoPid" ) )
                            Str::Copy( name_, "Weapon.DefAmmo" );
                        else if( Str::Compare( name, "Container_Volume" ) )
                            Str::Copy( name_, "Container.ContVolume" );
                        else if( Str::Compare( name, "Ammo_AcMod" ) )
                            Str::Copy( name_, "Ammo.ACMod" );
                        else if( Str::Compare( name, "Ammo_DrMod" ) )
                            Str::Copy( name_, "Ammo.DRMod" );
                        else
                            continue;

                        fopro.GetStr( "Proto", name_, "", svalue );
                    }

                    if( !svalue[ 0 ] )
                        continue;
                }
                else
                {
                    continue;
                }
            }

            // Blocks, childs
            if( Str::Compare( name, "BlockLines" ) || Str::CompareCount( name, "ChildLines_", 11 ) )
            {
                // Get lines destination
                uchar* lines;
                uint   max_count;
                if( Str::Compare( name, "BlockLines" ) )
                {
                    lines = proto_item.BlockLines;
                    max_count = sizeof( proto_item.BlockLines );
                }
                else
                {
                    int child_index = name[ 11 ] - '0';
                    if( child_index < 0 || child_index >= ITEM_MAX_CHILDS )
                        continue;
                    lines = proto_item.ChildLines[ child_index ];
                    max_count = sizeof( proto_item.ChildLines[ child_index ] );

                    // Convert dir-dir-dir to dir-1-dir-1-dir-1
                    if( deprecated )
                    {
                        char step1[] = "1";
                        for( int k = 0, l = Str::Length( svalue ); k < l; k++ )
                            Str::Insert( &svalue[ k * 2 + 1 ], step1 );
                        for( char* s = svalue; s[ 0 ] && s[ 2 ];)
                            if( s[ 0 ] == s[ 2 ] )
                            {
                                s[ 1 ]++;
                                Str::EraseInterval( s + 2, 2 );
                            }
                            else
                            {
                                s += 2;
                            }
                    }
                }

                // Parse lines
                uint svalue_len = Str::Length( svalue );
                for( uint k = 0; k / 2 < max_count && k + 1 < svalue_len; k += 2 )
                {
                    uchar dir = ( svalue[ k ] - '0' ) & 0xF;
                    uchar steps = ( svalue[ k + 1 ] - '0' ) & 0xF;
                    if( dir >= DIRS_COUNT || !steps )
                        break;
                    lines[ k / 2 ] = ( dir << 4 ) | steps;
                }
            }
            // Resolve value
            else
            {
                int size = engine->GetSizeOfPrimitiveType( type_id );
                switch( size )
                {
                case sizeof( uchar ):
                    *(uchar*) ( ( (uchar*) &proto_item ) + offset ) = ResolveProtoValue< uchar >( svalue );
                    break;
                case sizeof( ushort ):
                    *(ushort*) ( ( (uchar*) &proto_item ) + offset ) = ResolveProtoValue< ushort >( svalue );
                    break;
                case sizeof( uint ):
                    *(uint*) ( ( (uchar*) &proto_item ) + offset ) = ResolveProtoValue< uint >( svalue );
                    break;
                case sizeof( int64 ):
                    *(int64*) ( ( (uchar*) &proto_item ) + offset ) = ResolveProtoValue< int64 >( svalue );
                    break;
                default:
                    break;
                }
            }
        }

        if( deprecated )
        {
            // Car
            if( fopro.GetStr( "Proto", "MiscEx.IsCar", "", svalue ) )
            {
                proto_item.Type = ITEM_TYPE_CAR;
                proto_item.ChildPid[ 0 ] = proto_item.StartValue[ 1 ];
                proto_item.ChildPid[ 1 ] = proto_item.StartValue[ 2 ];
                proto_item.ChildPid[ 2 ] = proto_item.StartValue[ 3 ];
                proto_item.StartValue[ 1 ] = proto_item.StartValue[ 2 ] = proto_item.StartValue[ 3 ];
            }

            // Stacking, Deteriorating
            if( proto_item.Type == ITEM_TYPE_AMMO || proto_item.Type == ITEM_TYPE_DRUG ||
                proto_item.Type == ITEM_TYPE_MISC )
                proto_item.Stackable = true;
            if( proto_item.Type == ITEM_TYPE_WEAPON )
            {
                if( fopro.GetStr( "Proto", "Weapon.NoWear", "", svalue ) )
                    proto_item.Stackable = true;
                else
                    proto_item.Deteriorable = true;
            }
            if( proto_item.Type == ITEM_TYPE_ARMOR )
                proto_item.Deteriorable = true;

            // ITEM_TYPE_MISC_EX (6)
            if( proto_item.Type == 6 )
                proto_item.Type = ITEM_TYPE_MISC;

            // IsGroundLevel
            proto_item.GroundLevel = true;
            if( proto_item.Type == ITEM_TYPE_CONTAINER )
                proto_item.GroundLevel = ( proto_item.Container_MagicHandsGrnd ? true : false );
            else if( proto_item.IsDoor() || proto_item.IsCar() || proto_item.IsScen() ||
                     proto_item.IsGrid() || !proto_item.IsCanPickUp() )
                proto_item.GroundLevel = false;

            // Unarmed
            if( proto_item.Weapon_IsUnarmed )
            {
                proto_item.Stackable = false;
                proto_item.Deteriorable = false;
            }

            // No open container
            if( fopro.GetStr( "Proto", "Container.IsNoOpen", "", svalue ) )
                SETFLAG( proto_item.Locker_Condition, LOCKER_NOOPEN );
        }

        if( proto_item.ProtoId )
        {
            // Check script
            SAFEDELA( protoScript[ proto_item.ProtoId ] );
            char script_module[ MAX_SCRIPT_NAME + 1 ];
            char script_func[ MAX_SCRIPT_NAME + 1 ];
            fopro.GetStr( "Proto", "ScriptModule", "", script_module );
            fopro.GetStr( "Proto", "ScriptFunc", "", script_func );
            if( script_module[ 0 ] && script_func[ 0 ] )
                protoScript[ proto_item.ProtoId ] = Str::Duplicate( Str::FormatBuf( "%s@%s", script_module, script_func ) );

            // Add to collection
            protos.push_back( proto_item );

            //	if(FLAG(proto_item.Flags,ITEM_MULTI_HEX)) WriteLog("pid<%u> ITEM_MULTI_HEX\n",proto_item.ProtoId);
            //	if(FLAG(proto_item.Flags,ITEM_WALL_TRANS_END)) WriteLog("pid<%u> ITEM_WALL_TRANS_END\n",proto_item.ProtoId);
            //	if(FLAG(proto_item.Flags,ITEM_CACHED)) WriteLog("pid<%u> ITEM_CACHED\n",proto_item.ProtoId);
        }
    }

    if( protos.empty() )
    {
        WriteLogF( _FUNC_, " - Proto items not found<%s>.\n", fname );
        return false;
    }
    return true;
}
#endif

bool CompProtoByPid( ProtoItem o1, ProtoItem o2 ) { return o1.ProtoId < o2.ProtoId; }
void ItemManager::ParseProtos( ProtoItemVec& protos, const char* collection_name /* = NULL */ )
{
    if( protos.empty() )
    {
        WriteLogF( _FUNC_, " - List is empty, parsing ended.\n" );
        return;
    }

    for( uint i = 0, j = (uint) protos.size(); i < j; i++ )
    {
        ProtoItem& proto_item = protos[ i ];
        ushort     pid = proto_item.ProtoId;

        if( !pid || pid >= MAX_ITEM_PROTOTYPES )
        {
            WriteLogF( _FUNC_, " - Invalid pid<%u> of item prototype.\n", pid );
            continue;
        }

        if( IsInitProto( pid ) )
        {
            ClearProto( pid );
            WriteLogF( _FUNC_, " - Item prototype is already parsed, pid<%u>. Rewrite.\n", pid );
        }

        int type = protos[ i ].Type;
        if( type < 0 || type >= ITEM_MAX_TYPES )
            type = ITEM_TYPE_OTHER;

        #ifdef FONLINE_MAPPER
        if( collection_name )
            proto_item.CollectionName = Str::Duplicate( collection_name );
        else
            proto_item.CollectionName = Str::Duplicate( "" );
        #endif

        typeProto[ type ].push_back( proto_item );
        allProto[ pid ] = proto_item;
    }

    for( int i = 0; i < ITEM_MAX_TYPES; i++ )
    {
        protoHash[ i ] = 0;
        if( typeProto[ i ].size() )
        {
            std::sort( typeProto[ i ].begin(), typeProto[ i ].end(), CompProtoByPid );
            protoHash[ i ] = Crypt.Crc32( (uchar*) &typeProto[ i ][ 0 ], sizeof( ProtoItem ) * (uint) typeProto[ i ].size() );
        }
    }
}

ProtoItem* ItemManager::GetProtoItem( ushort pid )
{
    if( !IsInitProto( pid ) )
        return NULL;
    return &allProto[ pid ];
}

ProtoItem* ItemManager::GetAllProtos()
{
    return allProto;
}

void ItemManager::GetCopyAllProtos( ProtoItemVec& protos )
{
    for( int i = 0; i < ITEM_MAX_TYPES; i++ )
    {
        for( uint j = 0; j < typeProto[ i ].size(); j++ )
        {
            protos.push_back( typeProto[ i ][ j ] );
        }
    }
}

bool ItemManager::IsInitProto( ushort pid )
{
    if( !pid || pid >= MAX_ITEM_PROTOTYPES )
        return false;
    return allProto[ pid ].ProtoId != 0;
}

const char* ItemManager::GetProtoScript( ushort pid )
{
    if( !IsInitProto( pid ) )
        return NULL;
    return protoScript[ pid ];
}

void ItemManager::ClearProtos( int type /* = 0xFF */ )
{
    if( type == 0xFF )
    {
        for( int i = 0; i < ITEM_MAX_TYPES; i++ )
        {
            protoHash[ i ] = 0;
            typeProto[ i ].clear();
        }
        memzero( allProto, sizeof( allProto ) );
        for( int i = 0; i < MAX_ITEM_PROTOTYPES; i++ )
            SAFEDELA( protoScript[ i ] );
    }
    else if( type < ITEM_MAX_TYPES )
    {
        for( uint i = 0; i < typeProto[ type ].size(); ++i )
        {
            ushort pid = ( typeProto[ type ] )[ i ].ProtoId;
            if( IsInitProto( pid ) )
            {
                allProto[ pid ].Clear();
                SAFEDELA( protoScript[ pid ] );
            }
        }
        typeProto[ type ].clear();
        protoHash[ type ] = 0;
    }
    else
    {
        WriteLogF( _FUNC_, " - Wrong proto type<%u>.\n", type );
    }
}

void ItemManager::ClearProto( ushort pid )
{
    ProtoItem* proto_item = GetProtoItem( pid );
    if( !proto_item )
        return;

    int type = proto_item->Type;
    if( type < 0 || type >= ITEM_MAX_TYPES )
        type = ITEM_TYPE_OTHER;

    ProtoItemVec& protos = typeProto[ type ];
    uint&         hash = protoHash[ type ];

    allProto[ pid ].Clear();

    auto it = std::find( protos.begin(), protos.end(), pid );
    if( it != protos.end() )
        protos.erase( it );

    hash = Crypt.Crc32( (uchar*) &protos[ 0 ], sizeof( ProtoItem ) * (uint) protos.size() );
}

#ifdef FONLINE_SERVER
void ItemManager::SaveAllItemsFile( void ( * save_func )( void*, size_t ) )
{
    uint count = (uint) gameItems.size();
    save_func( &count, sizeof( count ) );
    for( auto it = gameItems.begin(), end = gameItems.end(); it != end; ++it )
    {
        Item* item = ( *it ).second;
        save_func( &item->Id, sizeof( item->Id ) );
        save_func( &item->Proto->ProtoId, sizeof( item->Proto->ProtoId ) );
        save_func( &item->Accessory, sizeof( item->Accessory ) );
        save_func( &item->AccBuffer[ 0 ], sizeof( item->AccBuffer ) );
        save_func( &item->Data, sizeof( item->Data ) );
        if( item->PLexems )
        {
            uchar lex_len = Str::Length( item->PLexems );
            save_func( &lex_len, sizeof( lex_len ) );
            save_func( item->PLexems, lex_len );
        }
        else
        {
            uchar zero = 0;
            save_func( &zero, sizeof( zero ) );
        }
    }
}

bool ItemManager::LoadAllItemsFile( void* f, int version )
{
    WriteLog( "Load items..." );

    lastItemId = 0;

    uint count;
    FileRead( f, &count, sizeof( count ) );
    if( !count )
    {
        WriteLog( "items not found.\n" );
        return true;
    }

    int errors = 0;
    for( uint i = 0; i < count; ++i )
    {
        uint   id;
        FileRead( f, &id, sizeof( id ) );
        ushort pid;
        FileRead( f, &pid, sizeof( pid ) );
        uchar  acc;
        FileRead( f, &acc, sizeof( acc ) );
        char   acc_buf[ 8 ];
        FileRead( f, &acc_buf[ 0 ], sizeof( acc_buf ) );

        Item::ItemData data;
        if( version >= WORLD_SAVE_V13 )
        {
            FileRead( f, &data, sizeof( data ) );
        }
        else
        {
            struct // 8
            {
                union
                {
                    struct
                    {
                        uchar  BrokenFlags;
                        uchar  BrokenCount;
                        ushort Deterioration;
                        ushort AmmoPid;
                        ushort AmmoCount;
                    } TechInfo;
                    struct
                    {
                        uint   DoorId;
                        ushort Condition;
                        ushort Complexity;
                    } Locker;
                    struct
                    {
                        uint   DoorId;
                        ushort Fuel;
                        ushort Deterioration;
                    } Car;
                    struct
                    {
                        uint Number;
                    } Holodisk;
                    struct
                    {
                        ushort Channel;
                        ushort Flags;
                        uchar  BroadcastSend;
                        uchar  BroadcastRecv;
                    } Radio;
                };
            } data_old;
            STATIC_ASSERT( sizeof( data_old ) == 8 );
            memzero( &data, sizeof( data ) );
            FileRead( f, &data, 84 );
            FileRead( f, &data_old, 8 );
            ProtoItem* proto = GetProtoItem( pid );
            if( proto && proto->Deteriorable )
            {
                data.BrokenFlags = data_old.TechInfo.BrokenFlags;
                data.BrokenCount = data_old.TechInfo.BrokenCount;
                data.Deterioration = data_old.TechInfo.Deterioration;
            }
            if( proto && proto->Type == ITEM_TYPE_WEAPON )
            {
                data.AmmoPid = data_old.TechInfo.AmmoPid;
                data.AmmoCount = data_old.TechInfo.AmmoCount;
            }
            if( proto && ( proto->Type == ITEM_TYPE_DOOR || proto->Type == ITEM_TYPE_CONTAINER ) )
            {
                data.LockerId = data_old.Locker.DoorId;
                data.LockerCondition = data_old.Locker.Condition;
                data.LockerComplexity = data_old.Locker.Complexity;
            }
            if( proto && proto->Type == ITEM_TYPE_KEY )
            {
                data.LockerId = data_old.Locker.DoorId;
            }
            if( proto && proto->Type == ITEM_TYPE_CAR )
            {
                data.LockerId = data_old.Car.DoorId;
                data.Charge = data_old.Car.Fuel;
                data.Deterioration = data_old.Car.Deterioration;
            }
            if( FLAG( data.Flags, ITEM_HOLODISK ) )
            {
                data.HolodiskNumber = data_old.Holodisk.Number;
            }
            if( FLAG( data.Flags, ITEM_RADIO ) )
            {
                data.RadioChannel = data_old.Radio.Channel;
                data.RadioFlags = data_old.Radio.Flags;
                data.RadioBroadcastSend = data_old.Radio.BroadcastSend;
                data.RadioBroadcastRecv = data_old.Radio.BroadcastRecv;
            }
        }

        uchar lex_len;
        char  lexems[ 1024 ] = { 0 };
        FileRead( f, &lex_len, sizeof( lex_len ) );
        if( lex_len )
        {
            FileRead( f, lexems, lex_len );
            lexems[ lex_len ] = 0;
        }

        Item* item = CreateItem( pid, 0, id );
        if( !item )
        {
            WriteLog( "Create item error id<%u>, pid<%u>.\n", id, pid );
            errors++;
            continue;
        }
        if( id > lastItemId )
            lastItemId = id;

        item->Accessory = acc;
        memcpy( item->AccBuffer, acc_buf, sizeof( acc_buf ) );
        item->Data = data;
        if( lexems[ 0 ] )
            item->SetLexems( lexems );

        AddItemStatistics( pid, item->GetCount() );

        // Radio collection
        if( item->IsRadio() )
            RadioRegister( item, true );

        // Patches
        if( version < WORLD_SAVE_V11 )
        {
            if( item->GetProtoId() == 100 /*PID_RADIO*/ && FLAG( item->Proto->Flags, ITEM_RADIO ) )
                SETFLAG( item->Data.Flags, ITEM_RADIO );
            else if( item->GetProtoId() == 58 /*PID_HOLODISK*/ && FLAG( item->Proto->Flags, ITEM_HOLODISK ) )
                SETFLAG( item->Data.Flags, ITEM_HOLODISK );
        }
    }
    if( errors )
        return false;

    WriteLog( "complete, count<%u>.\n", count );
    return true;
}

# pragma MESSAGE("Add item proto functions checker.")
bool ItemManager::CheckProtoFunctions()
{
    for( int i = 0; i < MAX_ITEM_PROTOTYPES; i++ )
    {
        const char* script = protoScript[ i ];
        // Check for items
        // Check for scenery
    }
    return true;
}

void ItemManager::RunInitScriptItems()
{
    ItemPtrMap items = gameItems;
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = ( *it ).second;
        if( item->Data.ScriptId )
            item->ParseScript( NULL, false );
    }
}

void ItemManager::GetGameItems( ItemPtrVec& items )
{
    SCOPE_LOCK( itemLocker );
    items.reserve( gameItems.size() );
    for( auto it = gameItems.begin(), end = gameItems.end(); it != end; ++it )
    {
        Item* item = ( *it ).second;
        items.push_back( item );
    }
}

uint ItemManager::GetItemsCount()
{
    SCOPE_LOCK( itemLocker );
    uint count = (uint) gameItems.size();
    return count;
}

void ItemManager::SetCritterItems( Critter* cr )
{
    ItemPtrVec items;
    uint       crid = cr->GetId();

    itemLocker.Lock();
    for( auto it = gameItems.begin(), end = gameItems.end(); it != end; ++it )
    {
        Item* item = ( *it ).second;
        if( item->Accessory == ITEM_ACCESSORY_CRITTER && item->AccCritter.Id == crid )
            items.push_back( item );
    }
    itemLocker.Unlock();

    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        SYNC_LOCK( item );

        if( item->Accessory == ITEM_ACCESSORY_CRITTER && item->AccCritter.Id == crid )
        {
            cr->SetItem( item );
            if( item->IsRadio() )
                RadioRegister( item, true );
        }
    }
}

void ItemManager::GetItemIds( UIntSet& item_ids )
{
    SCOPE_LOCK( itemLocker );

    for( auto it = gameItems.begin(), end = gameItems.end(); it != end; ++it )
        item_ids.insert( ( *it ).second->GetId() );
}

Item* ItemManager::CreateItem( ushort pid, uint count, uint item_id /* = 0 */ )
{
    if( !IsInitProto( pid ) )
        return NULL;

    Item* item = new Item();
    if( !item )
    {
        WriteLogF( _FUNC_, " - Allocation fail.\n" );
        return NULL;
    }

    item->Init( &allProto[ pid ] );
    if( item_id )
    {
        SCOPE_LOCK( itemLocker );

        if( gameItems.count( item_id ) )
        {
            WriteLogF( _FUNC_, " - Item already created, id<%u>.\n", item_id );
            delete item;
            return NULL;
        }

        item->Id = item_id;
    }
    else
    {
        SCOPE_LOCK( itemLocker );

        item->Id = lastItemId + 1;
        lastItemId++;
    }

    if( item->IsStackable() )
        item->Count_Set( count );
    else
        AddItemStatistics( pid, 1 );

    SYNC_LOCK( item );

    // Main collection
    itemLocker.Lock();
    gameItems.insert( PAIR( item->Id, item ) );
    itemLocker.Unlock();

    // Radio collection
    if( item->IsRadio() )
        RadioRegister( item, true );

    // Prototype script
    if( !item_id && count && protoScript[ pid ] ) // Only for new items
    {
        item->ParseScript( protoScript[ pid ], true );
        if( item->IsNotValid )
            return NULL;
    }
    return item;
}

Item* ItemManager::SplitItem( Item* item, uint count )
{
    if( !item->IsStackable() )
    {
        WriteLogF( _FUNC_, " - Splitted item is not stackable, id<%u>, pid<%u>.\n", item->GetId(), item->GetProtoId() );
        return NULL;
    }

    uint item_count = item->GetCount();
    if( !count || count >= item_count )
    {
        WriteLogF( _FUNC_, " - Invalid count, id<%u>, pid<%u>, count<%u>, split count<%u>.\n", item->GetId(), item->GetProtoId(), item_count, count );
        return NULL;
    }

    Item* new_item = CreateItem( item->GetProtoId(), 0 ); // Ignore init script
    if( !new_item )
    {
        WriteLogF( _FUNC_, " - Create item fail, pid<%u>, count<%u>.\n", item->GetProtoId(), count );
        return NULL;
    }

    item->Count_Sub( count );
    new_item->Data = item->Data;
    new_item->Data.Count = 0;
    new_item->Count_Add( count );
    if( item->PLexems )
        new_item->SetLexems( item->PLexems );

    // Radio collection
    if( new_item->IsRadio() )
        RadioRegister( new_item, true );

    return new_item;
}

Item* ItemManager::GetItem( uint item_id, bool sync_lock )
{
    Item* item = NULL;

    itemLocker.Lock();
    auto it = gameItems.find( item_id );
    if( it != gameItems.end() )
        item = ( *it ).second;
    itemLocker.Unlock();

    if( item && sync_lock )
        SYNC_LOCK( item );
    return item;
}

void ItemManager::ItemToGarbage( Item* item )
{
    SCOPE_LOCK( itemLocker );
    itemToDelete.push_back( item->GetId() );
    itemToDeleteCount.push_back( item->GetCount() );
}

void ItemManager::ItemGarbager()
{
    if( !itemToDelete.empty() )
    {
        itemLocker.Lock();
        UIntVec to_del = itemToDelete;
        UIntVec to_del_count = itemToDeleteCount;
        itemToDelete.clear();
        itemToDeleteCount.clear();
        itemLocker.Unlock();

        for( uint i = 0, j = (uint) to_del.size(); i < j; i++ )
        {
            uint id = to_del[ i ];
            uint count = to_del_count[ i ];

            // Erase from main collection
            itemLocker.Lock();
            auto it = gameItems.find( id );
            if( it == gameItems.end() )
            {
                itemLocker.Unlock();
                continue;
            }
            Item* item = ( *it ).second;
            gameItems.erase( it );
            itemLocker.Unlock();

            // Synchronize
            SYNC_LOCK( item );

            // Maybe some items added
            if( item->IsStackable() && item->GetCount() > count )
            {
                itemLocker.Lock();
                gameItems.insert( PAIR( item->Id, item ) );
                itemLocker.Unlock();

                item->Count_Sub( count );
                NotifyChangeItem( item );
                continue;
            }

            // Call finish event
            if( item->IsValidAccessory() )
                EraseItemHolder( item );
            if( !item->IsNotValid && item->FuncId[ ITEM_EVENT_FINISH ] > 0 )
                item->EventFinish( true );

            item->IsNotValid = true;
            if( item->IsValidAccessory() )
                EraseItemHolder( item );

            // Erase from statistics
            if( item->IsStackable() )
                item->Count_Set( 0 );
            else
                SubItemStatistics( item->GetProtoId(), 1 );

            // Erase from radio collection
            if( item->IsRadio() )
                RadioRegister( item, false );

            // Clear, release
            item->FullClear();
            Job::DeferredRelease( item );
        }
    }
}

void ItemManager::NotifyChangeItem( Item* item )
{
    switch( item->Accessory )
    {
    case ITEM_ACCESSORY_CRITTER:
    {
        Critter* cr = CrMngr.GetCritter( item->AccCritter.Id, false );
        if( cr )
            cr->SendAA_ItemData( item );
    }
    break;
    case ITEM_ACCESSORY_HEX:
    {
        Map* map = MapMngr.GetMap( item->AccHex.MapId, false );
        if( map )
            map->ChangeDataItem( item );
    }
    break;
    default:
        break;
    }

	item->NotifyChangeLexems();
}

void ItemManager::EraseItemHolder( Item* item )
{
    switch( item->Accessory )
    {
    case ITEM_ACCESSORY_CRITTER:
    {
        Critter* cr = CrMngr.GetCritter( item->AccCritter.Id, true );
        if( cr )
            cr->EraseItem( item, true );
        else if( item->IsRadio() )
            ItemMngr.RadioRegister( item, true );
    }
    break;
    case ITEM_ACCESSORY_HEX:
    {
        Map* map = MapMngr.GetMap( item->AccHex.MapId, true );
        if( map )
            map->EraseItem( item->GetId() );
    }
    break;
    case ITEM_ACCESSORY_CONTAINER:
    {
        Item* cont = ItemMngr.GetItem( item->AccContainer.ContainerId, true );
        if( cont )
            cont->ContEraseItem( item );
    }
    break;
    default:
        break;
    }
    item->Accessory = 45;
}

void ItemManager::MoveItem( Item* item, uint count, Critter* to_cr )
{
    if( item->Accessory == ITEM_ACCESSORY_CRITTER && item->AccCritter.Id == to_cr->GetId() )
        return;

    if( count >= item->GetCount() || !item->IsStackable() )
    {
        EraseItemHolder( item );
        to_cr->AddItem( item, true );
    }
    else
    {
        Item* item_ = ItemMngr.SplitItem( item, count );
        if( item_ )
        {
            NotifyChangeItem( item );
            to_cr->AddItem( item_, true );
        }
    }
}

void ItemManager::MoveItem( Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy )
{
    if( item->Accessory == ITEM_ACCESSORY_HEX && item->AccHex.MapId == to_map->GetId() && item->AccHex.HexX == to_hx && item->AccHex.HexY == to_hy )
        return;

    if( count >= item->GetCount() || !item->IsStackable() )
    {
        EraseItemHolder( item );
        to_map->AddItem( item, to_hx, to_hy );
    }
    else
    {
        Item* item_ = ItemMngr.SplitItem( item, count );
        if( item_ )
        {
            NotifyChangeItem( item );
            to_map->AddItem( item_, to_hx, to_hy );
        }
    }
}

void ItemManager::MoveItem( Item* item, uint count, Item* to_cont, uint stack_id )
{
    if( item->Accessory == ITEM_ACCESSORY_CONTAINER && item->AccContainer.ContainerId == to_cont->GetId() && item->AccContainer.StackId == stack_id )
        return;

    if( count >= item->GetCount() || !item->IsStackable() )
    {
        EraseItemHolder( item );
        to_cont->ContAddItem( item, stack_id );
    }
    else
    {
        Item* item_ = ItemMngr.SplitItem( item, count );
        if( item_ )
        {
            NotifyChangeItem( item );
            to_cont->ContAddItem( item_, stack_id );
        }
    }
}

Item* ItemManager::AddItemContainer( Item* cont, ushort pid, uint count, uint stack_id )
{
    if( !cont || !cont->IsContainer() )
        return NULL;

    Item* item = cont->ContGetItemByPid( pid, stack_id );
    Item* result = NULL;

    if( item )
    {
        if( item->IsStackable() )
        {
            item->Count_Add( count );
            result = item;
        }
        else
        {
            if( count > MAX_ADDED_NOGROUP_ITEMS )
                count = MAX_ADDED_NOGROUP_ITEMS;
            for( uint i = 0; i < count; ++i )
            {
                item = ItemMngr.CreateItem( pid, 1 );
                if( !item )
                    continue;
                cont->ContAddItem( item, stack_id );
                result = item;
            }
        }
    }
    else
    {
        ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
        if( !proto_item )
            return result;

        if( proto_item->Stackable )
        {
            item = ItemMngr.CreateItem( pid, count );
            if( !item )
                return result;
            cont->ContAddItem( item, stack_id );
            result = item;
        }
        else
        {
            if( count > MAX_ADDED_NOGROUP_ITEMS )
                count = MAX_ADDED_NOGROUP_ITEMS;
            for( uint i = 0; i < count; ++i )
            {
                item = ItemMngr.CreateItem( pid, 1 );
                if( !item )
                    continue;
                cont->ContAddItem( item, stack_id );
                result = item;
            }
        }
    }

    return result;
}

Item* ItemManager::AddItemCritter( Critter* cr, ushort pid, uint count )
{
    if( !count )
        return NULL;

    Item* item = cr->GetItemByPid( pid );
    Item* result = NULL;

    if( item )
    {
        if( item->IsStackable() )
        {
            item->Count_Add( count );
            cr->SendAA_ItemData( item );
            result = item;
        }
        else
        {
            if( count > MAX_ADDED_NOGROUP_ITEMS )
                count = MAX_ADDED_NOGROUP_ITEMS;
            for( uint i = 0; i < count; ++i )
            {
                item = ItemMngr.CreateItem( pid, 1 );
                if( !item )
                    break;
                cr->AddItem( item, true );
                result = item;
            }
        }
    }
    else
    {
        ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
        if( !proto_item )
            return result;

        if( proto_item->Stackable )
        {
            item = ItemMngr.CreateItem( pid, count );
            if( !item )
                return result;
            cr->AddItem( item, true );
            result = item;
        }
        else
        {
            if( count > MAX_ADDED_NOGROUP_ITEMS )
                count = MAX_ADDED_NOGROUP_ITEMS;
            for( uint i = 0; i < count; ++i )
            {
                item = ItemMngr.CreateItem( pid, 1 );
                if( !item )
                    break;
                cr->AddItem( item, true );
                result = item;
            }
        }
    }

    return result;
}

bool ItemManager::SubItemCritter( Critter* cr, ushort pid, uint count, ItemPtrVec* erased_items )
{
    if( !count )
        return true;

    Item* item = cr->GetItemByPidInvPriority( pid );
    if( !item )
        return true;

    if( item->IsStackable() )
    {
        if( count >= item->GetCount() )
        {
            cr->EraseItem( item, true );
            if( !erased_items )
                ItemMngr.ItemToGarbage( item );
            else
                erased_items->push_back( item );
        }
        else
        {
            if( erased_items )
            {
                Item* item_ = ItemMngr.SplitItem( item, count );
                if( item_ )
                    erased_items->push_back( item_ );
            }
            else
            {
                item->Count_Sub( count );
            }
            cr->SendAA_ItemData( item );
        }
    }
    else
    {
        for( uint i = 0; i < count; ++i )
        {
            cr->EraseItem( item, true );
            if( !erased_items )
                ItemMngr.ItemToGarbage( item );
            else
                erased_items->push_back( item );
            item = cr->GetItemByPidInvPriority( pid );
            if( !item )
                return true;
        }
    }

    return true;
}

bool ItemManager::SetItemCritter( Critter* cr, ushort pid, uint count )
{
    uint cur_count = cr->CountItemPid( pid );
    if( cur_count > count )
        return SubItemCritter( cr, pid, cur_count - count );
    else if( cur_count < count )
        return AddItemCritter( cr, pid, count - cur_count ) != NULL;
    return true;
}

bool ItemManager::MoveItemCritters( Critter* from_cr, Critter* to_cr, uint item_id, uint count )
{
    Item* item = from_cr->GetItem( item_id, false );
    if( !item )
        return false;

    if( !count || count > item->GetCount() )
        count = item->GetCount();

    if( item->IsStackable() && item->GetCount() > count )
    {
        Item* item_ = to_cr->GetItemByPid( item->GetProtoId() );
        if( !item_ )
        {
            item_ = ItemMngr.CreateItem( item->GetProtoId(), count );
            if( !item_ )
            {
                WriteLogF( _FUNC_, " - Create item fail, pid<%u>.\n", item->GetProtoId() );
                return false;
            }

            to_cr->AddItem( item_, true );
        }
        else
        {
            item_->Count_Add( count );
            to_cr->SendAA_ItemData( item_ );
        }

        item->Count_Sub( count );
        from_cr->SendAA_ItemData( item );
    }
    else
    {
        from_cr->EraseItem( item, true );
        to_cr->AddItem( item, true );
    }

    return true;
}

bool ItemManager::MoveItemCritterToCont( Critter* from_cr, Item* to_cont, uint item_id, uint count, uint stack_id )
{
    if( !to_cont->IsContainer() )
        return false;

    Item* item = from_cr->GetItem( item_id, false );
    if( !item )
        return false;

    if( !count || count > item->GetCount() )
        count = item->GetCount();

    if( item->IsStackable() && item->GetCount() > count )
    {
        Item* item_ = to_cont->ContGetItemByPid( item->GetProtoId(), stack_id );
        if( !item_ )
        {
            item_ = ItemMngr.CreateItem( item->GetProtoId(), count );
            if( !item_ )
            {
                WriteLogF( _FUNC_, " - Create item fail, pid<%u>.\n", item->GetProtoId() );
                return false;
            }

            item_->AccContainer.StackId = stack_id;
            to_cont->ContSetItem( item_ );
        }
        else
        {
            item_->Count_Add( count );
        }

        item->Count_Sub( count );
        from_cr->SendAA_ItemData( item );
    }
    else
    {
        from_cr->EraseItem( item, true );
        to_cont->ContAddItem( item, stack_id );
    }

    return true;
}

bool ItemManager::MoveItemCritterFromCont( Item* from_cont, Critter* to_cr, uint item_id, uint count )
{
    if( !from_cont->IsContainer() )
        return false;

    Item* item = from_cont->ContGetItem( item_id, false );
    if( !item )
        return false;

    if( !count || count > item->GetCount() )
        count = item->GetCount();

    if( item->IsStackable() && item->GetCount() > count )
    {
        Item* item_ = to_cr->GetItemByPid( item->GetProtoId() );
        if( !item_ )
        {
            item_ = ItemMngr.CreateItem( item->GetProtoId(), count );
            if( !item_ )
            {
                WriteLogF( _FUNC_, " - Create item fail, pid<%u>.\n", item->GetProtoId() );
                return false;
            }

            to_cr->AddItem( item_, true );
        }
        else
        {
            item_->Count_Add( count );
            to_cr->SendAA_ItemData( item_ );
        }

        item->Count_Sub( count );
    }
    else
    {
        from_cont->ContEraseItem( item );
        to_cr->AddItem( item, true );
    }

    return true;
}

bool ItemManager::MoveItemsContainers( Item* from_cont, Item* to_cont, uint from_stack_id, uint to_stack_id )
{
    if( !from_cont->IsContainer() || !to_cont->IsContainer() )
        return false;
    if( !from_cont->ContIsItems() )
        return true;

    ItemPtrVec items;
    from_cont->ContGetItems( items, from_stack_id, true );
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        from_cont->ContEraseItem( item );
        to_cont->ContAddItem( item, to_stack_id );
    }

    return true;
}

bool ItemManager::MoveItemsContToCritter( Item* from_cont, Critter* to_cr, uint stack_id )
{
    if( !from_cont->IsContainer() )
        return false;
    if( !from_cont->ContIsItems() )
        return true;

    ItemPtrVec items;
    from_cont->ContGetItems( items, stack_id, true );
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        from_cont->ContEraseItem( item );
        to_cr->AddItem( item, true );
    }

    return true;
}

void ItemManager::RadioRegister( Item* radio, bool add )
{
    SCOPE_LOCK( radioItemsLocker );

    auto it = std::find( radioItems.begin(), radioItems.end(), radio );

    if( add )
    {
        if( it == radioItems.end() )
            radioItems.push_back( radio );
    }
    else
    {
        if( it != radioItems.end() )
            radioItems.erase( it );
    }
}

void ItemManager::RadioSendText( Critter* cr, const char* text, ushort text_len, bool unsafe_text, ushort text_msg, uint num_str, UShortVec& channels )
{
    ItemPtrVec radios;
    ItemPtrVec items = cr->GetItemsNoLock();
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->IsRadio() && item->RadioIsSendActive() &&
            std::find( channels.begin(), channels.end(), item->Data.RadioChannel ) == channels.end() )
        {
            channels.push_back( item->Data.RadioChannel );
            radios.push_back( item );

            if( radios.size() > 100 )
                break;
        }
    }

    for( uint i = 0, j = (uint) radios.size(); i < j; i++ )
    {
        RadioSendTextEx( channels[ i ],
                         radios[ i ]->Data.RadioBroadcastSend, cr->GetMap(), cr->Data.WorldX, cr->Data.WorldY,
                         text, text_len, cr->IntellectCacheValue, unsafe_text, text_msg, num_str, NULL );
    }
}

void ItemManager::RadioSendTextEx( ushort channel, int broadcast_type, uint from_map_id, ushort from_wx, ushort from_wy,
                                   const char* text, ushort text_len, ushort intellect, bool unsafe_text,
                                   ushort text_msg, uint num_str, const char* lexems )
{
    // Broadcast
    if( broadcast_type != RADIO_BROADCAST_FORCE_ALL && broadcast_type != RADIO_BROADCAST_WORLD &&
        broadcast_type != RADIO_BROADCAST_MAP && broadcast_type != RADIO_BROADCAST_LOCATION &&
        !( broadcast_type >= 101 && broadcast_type <= 200 ) /*RADIO_BROADCAST_ZONE*/ )
        return;
    if( ( broadcast_type == RADIO_BROADCAST_MAP || broadcast_type == RADIO_BROADCAST_LOCATION ) && !from_map_id )
        return;

    int  broadcast = 0;
    uint broadcast_map_id = 0;
    uint broadcast_loc_id = 0;

    // Get copy of all radios
    radioItemsLocker.Lock();
    ItemPtrVec radio_items = radioItems;
    radioItemsLocker.Unlock();

    // Multiple sending controlling
    // Not thread safe, but this not so important in this case
    static uint msg_count = 0;
    msg_count++;

    // Send
    for( auto it = radio_items.begin(), end = radio_items.end(); it != end; ++it )
    {
        Item* radio = *it;

        if( radio->Data.RadioChannel == channel && radio->RadioIsRecvActive() )
        {
            if( broadcast_type != RADIO_BROADCAST_FORCE_ALL && radio->Data.RadioBroadcastRecv != RADIO_BROADCAST_FORCE_ALL )
            {
                if( broadcast_type == RADIO_BROADCAST_WORLD )
                    broadcast = radio->Data.RadioBroadcastRecv;
                else if( radio->Data.RadioBroadcastRecv == RADIO_BROADCAST_WORLD )
                    broadcast = broadcast_type;
                else
                    broadcast = MIN( broadcast_type, radio->Data.RadioBroadcastRecv );

                if( broadcast == RADIO_BROADCAST_WORLD )
                    broadcast = RADIO_BROADCAST_FORCE_ALL;
                else if( broadcast == RADIO_BROADCAST_MAP || broadcast == RADIO_BROADCAST_LOCATION )
                {
                    if( !broadcast_map_id )
                    {
                        Map* map = MapMngr.GetMap( from_map_id, false );
                        if( !map )
                            continue;
                        broadcast_map_id = map->GetId();
                        broadcast_loc_id = map->GetLocation( false )->GetId();
                    }
                }
                else if( !( broadcast >= 101 && broadcast <= 200 ) /*RADIO_BROADCAST_ZONE*/ )
                    continue;
            }
            else
            {
                broadcast = RADIO_BROADCAST_FORCE_ALL;
            }

            if( radio->Accessory == ITEM_ACCESSORY_CRITTER )
            {
                Client* cl = CrMngr.GetPlayer( radio->AccCritter.Id, false );
                if( cl && cl->RadioMessageSended != msg_count )
                {
                    if( broadcast != RADIO_BROADCAST_FORCE_ALL )
                    {
                        if( broadcast == RADIO_BROADCAST_MAP )
                        {
                            if( broadcast_map_id != cl->GetMap() )
                                continue;
                        }
                        else if( broadcast == RADIO_BROADCAST_LOCATION )
                        {
                            Map* map = MapMngr.GetMap( cl->GetMap(), false );
                            if( !map || broadcast_loc_id != map->GetLocation( false )->GetId() )
                                continue;
                        }
                        else if( broadcast >= 101 && broadcast <= 200 )                   // RADIO_BROADCAST_ZONE
                        {
                            if( !MapMngr.IsIntersectZone( from_wx, from_wy, 0, cl->Data.WorldX, cl->Data.WorldY, 0, broadcast - 101 ) )
                                continue;
                        }
                        else
                            continue;
                    }

                    if( text )
                        cl->Send_TextEx( radio->GetId(), text, text_len, SAY_RADIO, intellect, unsafe_text );
                    else if( lexems )
                        cl->Send_TextMsgLex( radio->GetId(), num_str, SAY_RADIO, text_msg, lexems );
                    else
                        cl->Send_TextMsg( radio->GetId(), num_str, SAY_RADIO, text_msg );

                    cl->RadioMessageSended = msg_count;
                }
            }
            else if( radio->Accessory == ITEM_ACCESSORY_HEX )
            {
                if( broadcast == RADIO_BROADCAST_MAP && broadcast_map_id != radio->AccHex.MapId )
                    continue;

                Map* map = MapMngr.GetMap( radio->AccHex.MapId, false );
                if( map )
                {
                    if( broadcast != RADIO_BROADCAST_FORCE_ALL && broadcast != RADIO_BROADCAST_MAP )
                    {
                        if( broadcast == RADIO_BROADCAST_LOCATION )
                        {
                            Location* loc = map->GetLocation( false );
                            if( broadcast_loc_id != loc->GetId() )
                                continue;
                        }
                        else if( broadcast >= 101 && broadcast <= 200 )                   // RADIO_BROADCAST_ZONE
                        {
                            Location* loc = map->GetLocation( false );
                            if( !MapMngr.IsIntersectZone( from_wx, from_wy, 0, loc->Data.WX, loc->Data.WY, loc->GetRadius(), broadcast - 101 ) )
                                continue;
                        }
                        else
                            continue;
                    }

                    if( text )
                        map->SetText( radio->AccHex.HexX, radio->AccHex.HexY, 0xFFFFFFFE, text, text_len, intellect, unsafe_text );
                    else if( lexems )
                        map->SetTextMsgLex( radio->AccHex.HexX, radio->AccHex.HexY, 0xFFFFFFFE, text_msg, num_str, lexems, Str::Length( lexems ) );
                    else
                        map->SetTextMsg( radio->AccHex.HexX, radio->AccHex.HexY, 0xFFFFFFFE, text_msg, num_str );
                }
            }
        }
    }
}
#endif // FONLINE_SERVER

void ItemManager::AddItemStatistics( ushort pid, uint val )
{
    if( !IsInitProto( pid ) )
        return;

    SCOPE_LOCK( itemCountLocker );

    itemCount[ pid ] += (int64) val;
}

void ItemManager::SubItemStatistics( ushort pid, uint val )
{
    if( !IsInitProto( pid ) )
        return;

    SCOPE_LOCK( itemCountLocker );

    itemCount[ pid ] -= (int64) val;
}

int64 ItemManager::GetItemStatistics( ushort pid )
{
    SCOPE_LOCK( itemCountLocker );

    int64 count = itemCount[ pid ];
    return count;
}

string ItemManager::GetItemsStatistics()
{
    SCOPE_LOCK( itemCountLocker );

    string result = "Pid    Name                                     Count\n";
    if( IsInit() )
    {
        char str[ 512 ];
        for( int i = 0; i < MAX_ITEM_PROTOTYPES; i++ )
        {
            ProtoItem* proto_item = GetProtoItem( i );
            if( proto_item && proto_item->IsItem() )
            {
                char* s = (char*) ConstantsManager::GetItemName( i );
                Str::Format( str, "%-6u %-40s %-20s\n", i, s ? s : Str::ItoA( i ), Str::I64toA( itemCount[ i ] ) );
                result += str;
            }
        }
    }
    return result;
}

// ==============================================================================================================================
// ******************************************************************************************************************************
// ==============================================================================================================================
