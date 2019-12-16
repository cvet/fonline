#include "ProtoManager.h"
#include "Log.h"
#include "Testing.h"
#include "Crypt.h"
#include "FileUtils.h"
#include "StringUtils.h"
#include "IniFile.h"

template< class T >
static void WriteProtosToBinary( UCharVec& data, const map< hash, T* >& protos )
{
    WriteData( data, (uint) protos.size() );
    for( auto& kv : protos )
    {
        hash         proto_id = kv.first;
        ProtoEntity* proto_item = kv.second;
        WriteData( data, proto_id );

        WriteData( data, (ushort) proto_item->Components.size() );
        for( hash component : proto_item->Components )
            WriteData( data, component );

        PUCharVec* props_data;
        UIntVec*   props_data_sizes;
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
        hash   proto_id = ReadData< hash >( data, pos );
        T*     proto = new T( proto_id );

        ushort components_count = ReadData< ushort >( data, pos );
        for( ushort j = 0; j < components_count; j++ )
            proto->Components.insert( ReadData< hash >( data, pos ) );

        uint data_count = ReadData< ushort >( data, pos );
        props_data.resize( data_count );
        props_data_sizes.resize( data_count );
        for( uint j = 0; j < data_count; j++ )
        {
            props_data_sizes[ j ] = ReadData< uint >( data, pos );
            props_data[ j ] = ReadDataArr< uchar >( data, props_data_sizes[ j ], pos );
        }
        proto->Props.RestoreData( props_data, props_data_sizes );
        RUNTIME_ASSERT( !protos.count( proto_id ) );
        protos.insert( std::make_pair( proto_id, proto ) );
    }
}

static void InsertMapValues( const StrMap& from_kv, StrMap& to_kv, bool overwrite )
{
    for( auto& kv : from_kv )
    {
        RUNTIME_ASSERT( !kv.first.empty() );
        if( kv.first[ 0 ] != '$' )
        {
            if( overwrite )
                to_kv[ kv.first ] = kv.second;
            else
                to_kv.insert( std::make_pair( kv.first, kv.second ) );
        }
        else if( kv.first == "$Components" && !kv.second.empty() )
        {
            if( !to_kv.count( "$Components" ) )
                to_kv[ "$Components" ] = kv.second;
            else
                to_kv[ "$Components" ] += " " + kv.second;
        }
    }
}

#pragma warning( disable : 4503 )
template< class T >
static int ParseProtos( const string& ext, const string& app_name, map< hash, T* >& protos )
{
    int errors = 0;

    // Collect data
    FileCollection                     files( ext );
    map< hash, StrMap >                files_protos;
    map< hash, map< string, StrMap > > files_texts;
    while( files.IsNextFile() )
    {
        string proto_name;
        File&  file = files.GetNextFile( &proto_name );
        if( !file.IsLoaded() )
        {
            WriteLog( "Unable to open file '{}'.\n", proto_name );
            errors++;
            continue;
        }

        IniFile fopro;
        fopro.AppendStr( file.GetCStr() );

        PStrMapVec protos_data;
        fopro.GetApps( app_name, protos_data );
        if( std::is_same< T, ProtoMap >::value && protos_data.empty() )
            fopro.GetApps( "Header", protos_data );
        for( auto& pkv : protos_data )
        {
            auto&         kv = *pkv;
            const string& name = ( kv.count( "$Name" ) ? kv[ "$Name" ] : proto_name );
            hash          pid = _str( name ).toHash();
            if( files_protos.count( pid ) )
            {
                WriteLog( "Proto '{}' already loaded.\n", name );
                errors++;
                continue;
            }

            files_protos.insert( std::make_pair( pid, kv ) );

            StrSet apps;
            fopro.GetAppNames( apps );
            for( auto& app_name : apps )
            {
                if( app_name.size() == 9 && app_name.find( "Text_" ) == 0 )
                {
                    if( !files_texts.count( pid ) )
                    {
                        map< string, StrMap > texts;
                        files_texts.insert( std::make_pair( pid, texts ) );
                    }
                    files_texts[ pid ].insert( std::make_pair( app_name, fopro.GetApp( app_name ) ) );
                }
            }
        }

        if( protos_data.empty() )
        {
            WriteLog( "File '{}' does not contain any proto.\n", proto_name );
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
                for( auto& inject_name : _str( inject_kv.second[ key_name ] ).split( ' ' ) )
                {
                    if( inject_name == "All" )
                    {
                        for( auto& kv : files_protos )
                            if( kv.first != inject_kv.first )
                                InsertMapValues( inject_kv.second, kv.second, overwrite );
                    }
                    else
                    {
                        hash inject_name_hash = _str( inject_name ).toHash();
                        if( !files_protos.count( inject_name_hash ) )
                        {
                            WriteLog( "Proto '{}' not found for injection from proto '{}'.\n", inject_name.c_str(), _str().parseHash( inject_kv.first ) );
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
        hash   pid = kv.first;
        string base_name = _str().parseHash( pid );
        RUNTIME_ASSERT( protos.count( pid ) == 0 );

        // Fill content from parents
        StrMap                                        final_kv;
        std::function< bool(const string&, StrMap&) > fill_parent = [ &fill_parent, &base_name, &files_protos, &final_kv ] ( const string &name, StrMap & cur_kv )
        {
            const char* parent_name_line = ( cur_kv.count( "$Parent" ) ? cur_kv[ "$Parent" ].c_str() : "" );
            for( auto& parent_name : _str( parent_name_line ).split( ' ' ) )
            {
                hash parent_pid = _str( parent_name ).toHash();
                auto parent = files_protos.find( parent_pid );
                if( parent == files_protos.end() )
                {
                    if( base_name == name )
                        WriteLog( "Proto '{}' fail to load parent '{}'.\n", base_name, parent_name );
                    else
                        WriteLog( "Proto '{}' fail to load parent '{}' for proto '{}'.\n", base_name, parent_name, name );
                    return false;
                }

                if( !fill_parent( parent_name, parent->second ) )
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
            WriteLog( "Proto item '{}' fail to load properties.\n", base_name );
            errors++;
            continue;
        }

        // Components
        if( final_kv.count( "$Components" ) )
        {
            for( const string& component_name : _str( final_kv[ "$Components" ] ).split( ' ' ) )
            {
                hash component_name_hash = _str( component_name ).toHash();
                if( !proto->Props.GetRegistrator()->IsComponentRegistered( component_name_hash ) )
                {
                    WriteLog( "Proto item '{}' invalid component '{}'.\n", base_name, component_name );
                    errors++;
                    continue;
                }
                proto->Components.insert( component_name_hash );
            }
        }

        // Add to collection
        protos.insert( std::make_pair( pid, proto ) );
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
    itemProtos.clear();
    for( auto& proto : crProtos )
        proto.second->Release();
    crProtos.clear();
    for( auto& proto : mapProtos )
        proto.second->Release();
    mapProtos.clear();
    for( auto& proto : locProtos )
        proto.second->Release();
    locProtos.clear();
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
    if( errors )
        return false;

    // Mapper collections
    #ifdef FONLINE_EDITOR
    for( auto& kv : itemProtos )
    {
        ProtoItem* proto_item = (ProtoItem*) kv.second;
        if( !proto_item->Components.empty() )
            proto_item->CollectionName = _str().parseHash( *proto_item->Components.begin() ).lower();
        else
            proto_item->CollectionName = "other";
    }
    for( auto& kv : crProtos )
    {
        ProtoCritter* proto_cr = (ProtoCritter*) kv.second;
        proto_cr->CollectionName = "all";
    }
    #endif

    // Check player proto
    if( !crProtos.count( _str( "Player" ).toHash() ) )
    {
        WriteLog( "Player proto 'Player.focr' not loaded.\n" );
        errors++;
    }
    if( errors )
        return false;

    // Check maps for locations
    for( auto& kv : locProtos )
    {
        CScriptArray* map_pids = kv.second->GetMapProtos();
        for( uint i = 0, j = map_pids->GetSize(); i < j; i++ )
        {
            hash map_pid = *(hash*) map_pids->At( i );
            if( !mapProtos.count( map_pid ) )
            {
                WriteLog( "Proto map '{}' not found for proto location '{}'.\n", _str().parseHash( map_pid ), kv.second->GetName() );
                errors++;
            }
        }
        map_pids->Release();
    }
    if( errors )
        return false;

    // Load maps data
    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
    for( auto& kv : mapProtos )
    {
        if( !kv.second->ServerLoad( *this ) )
        {
            WriteLog( "Load proto map '{}' fail.\n", kv.second->GetName() );
            errors++;
        }
    }
    if( errors )
        return false;
    #endif

    WriteLog( "Load prototypes complete, count {}.\n", (uint) ( itemProtos.size() + crProtos.size() + mapProtos.size() + locProtos.size() ) );
    return true;
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

template< typename T >
static int ValidateProtoResourcesExt( map< hash, T* >& protos,  HashSet& hashes )
{
    int errors = 0;
    for( auto& kv : protos )
    {
        T*                   proto = kv.second;
        PropertyRegistrator* registrator = proto->Props.GetRegistrator();
        for( uint i = 0; i < registrator->GetCount(); i++ )
        {
            Property* prop = registrator->Get( i );
            if( prop->IsResource() )
            {
                hash h = proto->Props.template GetPropValue< hash >( prop );
                if( h && !hashes.count( h ) )
                {
                    WriteLog( "Resource '{}' not found for property '{}' in prototype '{}'.\n", _str().parseHash( h ), prop->GetName(), proto->GetName() );
                    errors++;
                }
            }
        }
    }
    return errors;
}

bool ProtoManager::ValidateProtoResources( StrVec& resource_names )
{
    HashSet hashes;
    for( auto& name : resource_names )
        hashes.insert( _str( name ).toHash() );

    int errors = 0;
    errors += ValidateProtoResourcesExt( itemProtos, hashes );
    errors += ValidateProtoResourcesExt( crProtos, hashes );
    errors += ValidateProtoResourcesExt( mapProtos, hashes );
    errors += ValidateProtoResourcesExt( locProtos, hashes );
    return errors == 0;
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
