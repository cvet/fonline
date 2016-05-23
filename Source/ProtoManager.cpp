#include "ProtoManager.h"

ProtoManager ProtoMngr;

template< class T >
static void WriteProtosToBinary( UCharVec& data, const map< hash, T* >& protos )
{
    WriteData( data, (uint) protos.size() );
    for( auto& kv : protos )
    {
        hash         proto_id = kv.first;
        ProtoEntity* proto_item = kv.second;
        WriteData( data, proto_id );
        PUCharVec*   props_data;
        UIntVec*     props_data_sizes;
        proto_item->Props.StoreData( true, &props_data, &props_data_sizes );
        WriteData( data, (ushort) props_data->size() );
        for( size_t i = 0; i < props_data->size(); i++ )
        {
            uint cur_size = props_data_sizes->at( i );
            WriteData( data, cur_size );
            WriteDataArr( data, props_data->at( i ), cur_size );
        }
    }
}

template< class T >
static void ReadProtosFromBinary( UCharVec& data, uint& pos, map< hash, T* >& protos )
{
    PUCharVec props_data;
    UIntVec   props_data_sizes;
    uint      protos_count = ReadData< uint >( data, pos );
    for( uint i = 0; i < protos_count; i++ )
    {
        hash pid = ReadData< hash >( data, pos );
        T*   proto = new T( pid );
        uint data_count = ReadData< ushort >( data, pos );
        props_data.resize( data_count );
        props_data_sizes.resize( data_count );
        for( uint j = 0; j < data_count; j++ )
        {
            props_data_sizes[ j ] = ReadData< uint >( data, pos );
            props_data[ j ] = ReadDataArr< uchar >( data, props_data_sizes[ j ], pos );
        }
        proto->Props.RestoreData( props_data, props_data_sizes );
        RUNTIME_ASSERT( !protos.count( pid ) );
        protos.insert( PAIR( pid, proto ) );
    }
}

static void InsertMapValues( const StrMap& from_kv, StrMap& to_kv, bool overwrite )
{
    for( auto& kv : from_kv )
    {
        if( !kv.first.empty() && kv.first[ 0 ] != '$' )
        {
            if( overwrite )
                to_kv[ kv.first ] = kv.second;
            else
                to_kv.insert( PAIR( kv.first, kv.second ) );
        }
    }
}

#pragma warning( disable : 4503 )
template< class T >
static int ParseProtos( const char* ext, const char* app_name, map< hash, T* >& protos )
{
    int errors = 0;

    // Collect data
    FilesCollection                    files( ext );
    map< hash, StrMap >                files_protos;
    map< hash, map< string, StrMap > > files_texts;
    while( files.IsNextFile() )
    {
        const char*  file_name;
        FileManager& file = files.GetNextFile( &file_name );
        if( !file.IsLoaded() )
        {
            WriteLog( "Unable to open file '%s'.\n", file_name );
            errors++;
            continue;
        }

        IniParser fopro;
        fopro.AppendStr( file.GetCStr() );

        PStrMapVec protos_data;
        fopro.GetApps( app_name, protos_data );
        if( std::is_same< T, ProtoMap >::value && protos_data.empty() )
            fopro.GetApps( "Header", protos_data );
        for( auto& pkv : protos_data )
        {
            auto&       kv = *pkv;
            const char* name = ( kv.count( "$Name" ) ? kv[ "$Name" ].c_str() : file_name );
            hash        pid = Str::GetHash( name );
            if( files_protos.count( pid ) )
            {
                WriteLog( "Proto '%s' already loaded.\n", name );
                errors++;
                continue;
            }

            files_protos.insert( PAIR( pid, kv ) );

            StrSet apps;
            fopro.GetAppNames( apps );
            for( auto& app_name : apps )
            {
                if( app_name.size() == 9 && app_name.find( "Text_" ) == 0 )
                {
                    if( !files_texts.count( pid ) )
                    {
                        map< string, StrMap > texts;
                        files_texts.insert( PAIR( pid, texts ) );
                    }
                    files_texts[ pid ].insert( PAIR( app_name, fopro.GetApp( app_name.c_str() ) ) );
                }
            }
        }

        if( protos_data.empty() )
        {
            WriteLog( "File '%s' does not contain any proto.\n", file_name );
            errors++;
        }
    }
    if( errors )
        return errors;

    // Injection
    auto injection = [ &files_protos, &errors ] ( const char* key_name, bool overwrite )
    {
        for( auto& inject_kv : files_protos )
        {
            if( inject_kv.second.count( key_name ) )
            {
                StrVec inject_names;
                Str::ParseLine( inject_kv.second[ key_name ].c_str(), ' ', inject_names, Str::ParseLineDummy );
                for( auto& inject_name : inject_names )
                {
                    if( inject_name == "All" )
                    {
                        for( auto& kv : files_protos )
                            if( kv.first != inject_kv.first )
                                InsertMapValues( inject_kv.second, kv.second, overwrite );
                    }
                    else
                    {
                        hash inject_name_hash = Str::GetHash( inject_name.c_str() );
                        if( !files_protos.count( inject_name_hash ) )
                        {
                            WriteLog( "Proto '%s' not found for injection from proto '%s'.\n", inject_name.c_str(), Str::GetName( inject_kv.first ) );
                            errors++;
                            continue;
                        }
                        InsertMapValues( inject_kv.second, files_protos[ inject_name_hash ], overwrite );
                    }
                }
            }
        }
    };
    injection( "$Inject", false );
    if( errors )
        return errors;

    // Protos
    for( auto& kv : files_protos )
    {
        hash        pid = kv.first;
        const char* base_name = Str::GetName( pid );
        RUNTIME_ASSERT( protos.count( pid ) == 0 );

        // Fill content from parents
        StrMap                                      final_kv;
        std::function< bool(const char*, StrMap&) > fill_parent = [ &fill_parent, &base_name, &files_protos, &final_kv ] ( const char* name, StrMap & cur_kv )
        {
            const char* parent_name_line = ( cur_kv.count( "$Parent" ) ? cur_kv[ "$Parent" ].c_str() : "" );
            StrVec      parent_names;
            Str::ParseLine( parent_name_line, ' ', parent_names, Str::ParseLineDummy );
            for( auto& parent_name : parent_names )
            {
                hash parent_pid = Str::GetHash( parent_name.c_str() );
                auto parent = files_protos.find( parent_pid );
                if( parent == files_protos.end() )
                {
                    if( base_name == name )
                        WriteLog( "Proto '%s' fail to load parent '%s'.\n", base_name, parent_name.c_str() );
                    else
                        WriteLog( "Proto '%s' fail to load parent '%s' for proto '%s'.\n", base_name, parent_name.c_str(), name );
                    return false;
                }

                if( !fill_parent( parent_name.c_str(), parent->second ) )
                    return false;
                InsertMapValues( parent->second, final_kv, true );
            }
            return true;
        };
        if( !fill_parent( base_name, kv.second ) )
        {
            errors++;
            continue;
        }

        // Actual content
        InsertMapValues( kv.second, final_kv, true );

        // Final injection
        injection( "$InjectOverride", true );
        if( errors )
            return errors;

        // Create proto
        T* proto = new T( pid );
        if( !proto->Props.LoadFromText( final_kv ) )
        {
            WriteLog( "Proto item '%s' fail to load properties.\n", base_name );
            errors++;
            continue;
        }

        // Add to collection
        protos.insert( PAIR( pid, proto ) );
    }
    if( errors )
        return errors;

    // Texts
    for( auto& kv : files_texts )
    {
        T* proto = protos[ kv.first ];
        RUNTIME_ASSERT( proto );
        for( auto& text : kv.second )
        {
            FOMsg temp_msg;
            temp_msg.LoadFromMap( text.second );

            FOMsg* msg = new FOMsg();
            uint   str_num = 0;
            while( ( str_num = temp_msg.GetStrNumUpper( str_num ) ) )
            {
                uint count = temp_msg.Count( str_num );
                uint new_str_num = str_num;
                if( std::is_same< T, ProtoItem >::value )
                    new_str_num = ITEM_STR_ID( proto->ProtoId, str_num );
                else if( std::is_same< T, ProtoCritter >::value )
                    new_str_num = CR_STR_ID( proto->ProtoId, str_num );
                else if( std::is_same< T, ProtoLocation >::value )
                    new_str_num = LOC_STR_ID( proto->ProtoId, str_num );
                for( uint n = 0; n < count; n++ )
                    msg->AddStr( new_str_num, temp_msg.GetStr( str_num, n ) );
            }

            proto->TextsLang.push_back( *(uint*) text.first.substr( 5 ).c_str() );
            proto->Texts.push_back( msg );
        }
    }

    return errors;
}

void ProtoManager::ClearProtos()
{
    for( auto& proto : itemProtos )
        proto.second->Release();
    for( auto& proto : crProtos )
        proto.second->Release();
    for( auto& proto : mapProtos )
        proto.second->Release();
    for( auto& proto : locProtos )
        proto.second->Release();
}

bool ProtoManager::LoadProtosFromFiles()
{
    WriteLog( "Load prototypes...\n" );

    ClearProtos();

    // Load protos
    int errors = 0;
    errors += ParseProtos( "foitem", "ProtoItem", itemProtos );
    errors += ParseProtos( "focr", "ProtoCritter", crProtos );
    errors += ParseProtos( "fomap", "ProtoMap", mapProtos );
    errors += ParseProtos( "foloc", "ProtoLocation", locProtos );

    // Mapper collections
    #ifdef FONLINE_MAPPER
    for( auto& kv : itemProtos )
    {
        ProtoItem* proto_item = (ProtoItem*) kv.second;
        switch( proto_item->GetType() )
        {
        case ITEM_TYPE_ARMOR:
            proto_item->CollectionName = "armor";
            break;
        case ITEM_TYPE_DRUG:
            proto_item->CollectionName = "drug";
            break;
        case ITEM_TYPE_WEAPON:
            proto_item->CollectionName = "weapon";
            break;
        case ITEM_TYPE_AMMO:
            proto_item->CollectionName = "ammo";
            break;
        case ITEM_TYPE_MISC:
            proto_item->CollectionName = "misc";
            break;
        case ITEM_TYPE_KEY:
            proto_item->CollectionName = "key";
            break;
        case ITEM_TYPE_CONTAINER:
            proto_item->CollectionName = "container";
            break;
        case ITEM_TYPE_DOOR:
            proto_item->CollectionName = "door";
            break;
        case ITEM_TYPE_GRID:
            proto_item->CollectionName = "grid";
            break;
        case ITEM_TYPE_GENERIC:
            proto_item->CollectionName = "generic";
            break;
        case ITEM_TYPE_WALL:
            proto_item->CollectionName = "wall";
            break;
        case ITEM_TYPE_CAR:
            proto_item->CollectionName = "car";
            break;
        default:
            proto_item->CollectionName = "other";
            break;
        }
    }
    for( auto& kv : crProtos )
    {
        ProtoCritter* proto_cr = (ProtoCritter*) kv.second;
        proto_cr->CollectionName = "all";
    }
    #endif

    // Check player proto
    if( !crProtos.count( Str::GetHash( "Player" ) ) )
    {
        WriteLog( "Player proto 'Player.focr' not loaded." );
        errors++;
    }

    // Check maps for locations
    for( auto& kv : locProtos )
    {
        ScriptArray* map_pids = kv.second->GetMapProtos();
        for( uint i = 0, j = map_pids->GetSize(); i < j; i++ )
        {
            hash map_pid = *(hash*) map_pids->At( i );
            if( !mapProtos.count( map_pid ) )
            {
                WriteLog( "Proto map '%s' not found for proto location '%s'.\n", Str::GetName( map_pid ), kv.second->GetName() );
                errors++;
            }
        }
        map_pids->Release();
    }

    // Load maps data
    #ifdef FONLINE_SERVER
    for( auto& kv : mapProtos )
    {
        if( !kv.second->Load() )
        {
            WriteLog( "Load proto map '%s' fail.\n", kv.second->GetName() );
            errors++;
        }
    }
    #endif

    WriteLog( "Load prototypes complete, count %u.\n", (uint) ( itemProtos.size() + crProtos.size() + mapProtos.size() + locProtos.size() ) );
    return errors == 0;
}

void ProtoManager::GetBinaryData( UCharVec& data )
{
    data.clear();
    WriteProtosToBinary( data, itemProtos );
    WriteProtosToBinary( data, crProtos );
    WriteProtosToBinary( data, mapProtos );
    WriteProtosToBinary( data, locProtos );
    Crypt.Compress( data );
}

void ProtoManager::LoadProtosFromBinaryData( UCharVec& data )
{
    ClearProtos();

    if( data.empty() )
        return;
    if( !Crypt.Uncompress( data, 15 ) )
        return;

    uint pos = 0;
    ReadProtosFromBinary( data, pos, itemProtos );
    ReadProtosFromBinary( data, pos, crProtos );
    ReadProtosFromBinary( data, pos, mapProtos );
    ReadProtosFromBinary( data, pos, locProtos );
}

ProtoItem* ProtoManager::GetProtoItem( hash pid )
{
    auto it = itemProtos.find( pid );
    return it != itemProtos.end() ? it->second : nullptr;
}

ProtoCritter* ProtoManager::GetProtoCritter( hash pid )
{
    auto it = crProtos.find( pid );
    return it != crProtos.end() ? it->second : nullptr;
}

ProtoMap* ProtoManager::GetProtoMap( hash pid )
{
    auto it = mapProtos.find( pid );
    return it != mapProtos.end() ? it->second : nullptr;
}

ProtoLocation* ProtoManager::GetProtoLocation( hash pid )
{
    auto it = locProtos.find( pid );
    return it != locProtos.end() ? it->second : nullptr;
}
