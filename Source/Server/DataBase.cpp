#include "DataBase.h"
#include "Log.h"
#include "Testing.h"
#include "FileUtils.h"
#include "FileSystem.h"
#include "StringUtils.h"
#include "unqlite.h"
#include "json.hpp"

// Temporary workaround cause segfault before main on linux
#ifdef FO_LINUX
# define REMOVE_MONGO
#endif

#ifndef REMOVE_MONGO
# include "mongoc.h"
#endif
#include "bson.h"

DataBase* DbStorage;
DataBase* DbHistory;

static void ValueToBson( const string& key, const DataBase::Value& value, bson_t* bson )
{
    int value_index = value.which();
    if( value_index == DataBase::IntValue )
    {
        bool bson_ok = bson_append_int32( bson, key.c_str(), (int) key.length(), value.get< int >() );
        RUNTIME_ASSERT( bson_ok );
    }
    else if( value_index == DataBase::Int64Value )
    {
        bool bson_ok = bson_append_int64( bson, key.c_str(), (int) key.length(), value.get< int64 >() );
        RUNTIME_ASSERT( bson_ok );
    }
    else if( value_index == DataBase::DoubleValue )
    {
        bool bson_ok = bson_append_double( bson, key.c_str(), (int) key.length(), value.get< double >() );
        RUNTIME_ASSERT( bson_ok );
    }
    else if( value_index == DataBase::BoolValue )
    {
        bool bson_ok = bson_append_bool( bson, key.c_str(), (int) key.length(), value.get< bool >() );
        RUNTIME_ASSERT( bson_ok );
    }
    else if( value_index == DataBase::StringValue )
    {
        bool bson_ok = bson_append_utf8( bson, key.c_str(), (int) key.length(),
                                         value.get< string >().c_str(), (int) value.get< string >().length() );
        RUNTIME_ASSERT( bson_ok );
    }
    else if( value_index == DataBase::ArrayValue )
    {
        bson_t bson_arr;
        bool   bson_ok = bson_append_array_begin( bson, key.c_str(), (int) key.length(), &bson_arr );
        RUNTIME_ASSERT( bson_ok );

        const DataBase::Array& arr = value.get< DataBase::Array >();
        int                    arr_key_index = 0;
        for( auto& arr_value : arr )
        {
            string arr_key = _str( "{}", arr_key_index );
            arr_key_index++;

            int arr_value_index = arr_value.which();
            if( arr_value_index == DataBase::IntValue )
            {
                bool bson_ok = bson_append_int32( &bson_arr, arr_key.c_str(), (int) arr_key.length(), arr_value.get< int >() );
                RUNTIME_ASSERT( bson_ok );
            }
            else if( arr_value_index == DataBase::Int64Value )
            {
                bool bson_ok = bson_append_int64( &bson_arr, arr_key.c_str(), (int) arr_key.length(), arr_value.get< int64 >() );
                RUNTIME_ASSERT( bson_ok );
            }
            else if( arr_value_index == DataBase::DoubleValue )
            {
                bool bson_ok = bson_append_double( &bson_arr, arr_key.c_str(), (int) arr_key.length(), arr_value.get< double >() );
                RUNTIME_ASSERT( bson_ok );
            }
            else if( arr_value_index == DataBase::BoolValue )
            {
                bool bson_ok = bson_append_bool( &bson_arr, arr_key.c_str(), (int) arr_key.length(), arr_value.get< bool >() );
                RUNTIME_ASSERT( bson_ok );
            }
            else if( arr_value_index == DataBase::StringValue )
            {
                bool bson_ok = bson_append_utf8( &bson_arr, arr_key.c_str(), (int) arr_key.length(),
                                                 arr_value.get< string >().c_str(), (int) arr_value.get< string >().length() );
                RUNTIME_ASSERT( bson_ok );
            }
            else
            {
                RUNTIME_ASSERT( !"Invalid type" );
            }
        }

        bson_ok = bson_append_array_end( bson, &bson_arr );
        RUNTIME_ASSERT( bson_ok );
    }
    else if( value_index == DataBase::DictValue )
    {
        bson_t bson_doc;
        bool   bson_ok = bson_append_document_begin( bson, key.c_str(), (int) key.length(), &bson_doc );
        RUNTIME_ASSERT( bson_ok );

        const DataBase::Dict& dict = value.get< DataBase::Dict >();
        for( auto& kv : dict )
        {
            int dict_value_index = kv.second.which();
            if( dict_value_index == DataBase::IntValue )
            {
                bool bson_ok = bson_append_int32( &bson_doc, kv.first.c_str(), (int) kv.first.length(), kv.second.get< int >() );
                RUNTIME_ASSERT( bson_ok );
            }
            else if( dict_value_index == DataBase::Int64Value )
            {
                bool bson_ok = bson_append_int64( &bson_doc, kv.first.c_str(), (int) kv.first.length(), kv.second.get< int64 >() );
                RUNTIME_ASSERT( bson_ok );
            }
            else if( dict_value_index == DataBase::DoubleValue )
            {
                bool bson_ok = bson_append_double( &bson_doc, kv.first.c_str(), (int) kv.first.length(), kv.second.get< double >() );
                RUNTIME_ASSERT( bson_ok );
            }
            else if( dict_value_index == DataBase::BoolValue )
            {
                bool bson_ok = bson_append_bool( &bson_doc, kv.first.c_str(), (int) kv.first.length(), kv.second.get< bool >() );
                RUNTIME_ASSERT( bson_ok );
            }
            else if( dict_value_index == DataBase::StringValue )
            {
                bool bson_ok = bson_append_utf8( &bson_doc, kv.first.c_str(), (int) kv.first.length(),
                                                 kv.second.get< string >().c_str(), (int) kv.second.get< string >().length() );
                RUNTIME_ASSERT( bson_ok );
            }
            else if( dict_value_index == DataBase::ArrayValue )
            {
                bson_t bson_arr;
                bool   bson_ok = bson_append_array_begin( &bson_doc, kv.first.c_str(), (int) kv.first.length(), &bson_arr );
                RUNTIME_ASSERT( bson_ok );

                const DataBase::Array& arr = kv.second.get< DataBase::Array >();
                int                    arr_key_index = 0;
                for( auto& arr_value : arr )
                {
                    string arr_key = _str( "{}", arr_key_index );
                    arr_key_index++;

                    int arr_value_index = arr_value.which();
                    if( arr_value_index == DataBase::IntValue )
                    {
                        bool bson_ok = bson_append_int32( &bson_arr, arr_key.c_str(), (int) arr_key.length(), arr_value.get< int >() );
                        RUNTIME_ASSERT( bson_ok );
                    }
                    else if( arr_value_index == DataBase::Int64Value )
                    {
                        bool bson_ok = bson_append_int64( &bson_arr, arr_key.c_str(), (int) arr_key.length(), arr_value.get< int64 >() );
                        RUNTIME_ASSERT( bson_ok );
                    }
                    else if( arr_value_index == DataBase::DoubleValue )
                    {
                        bool bson_ok = bson_append_double( &bson_arr, arr_key.c_str(), (int) arr_key.length(), arr_value.get< double >() );
                        RUNTIME_ASSERT( bson_ok );
                    }
                    else if( arr_value_index == DataBase::BoolValue )
                    {
                        bool bson_ok = bson_append_bool( &bson_arr, arr_key.c_str(), (int) arr_key.length(), arr_value.get< bool >() );
                        RUNTIME_ASSERT( bson_ok );
                    }
                    else if( arr_value_index == DataBase::StringValue )
                    {
                        bool bson_ok = bson_append_utf8( &bson_arr, arr_key.c_str(), (int) arr_key.length(),
                                                         arr_value.get< string >().c_str(), (int) arr_value.get< string >().length() );
                        RUNTIME_ASSERT( bson_ok );
                    }
                    else
                    {
                        RUNTIME_ASSERT( !"Invalid type" );
                    }
                }

                bson_ok = bson_append_array_end( &bson_doc, &bson_arr );
                RUNTIME_ASSERT( bson_ok );
            }
            else
            {
                RUNTIME_ASSERT( !"Invalid type" );
            }
        }

        bson_ok = bson_append_document_end( bson, &bson_doc );
        RUNTIME_ASSERT( bson_ok );
    }
    else
    {
        RUNTIME_ASSERT( !"Invalid type" );
    }
}

static void DocumentToBson( const DataBase::Document& doc, bson_t* bson )
{
    for( auto& entry : doc )
        ValueToBson( entry.first, entry.second, bson );
}

static DataBase::Value BsonToValue( bson_iter_t* iter )
{
    const bson_value_t* value = bson_iter_value( iter );

    if( value->value_type == BSON_TYPE_INT32 )
    {
        return value->value.v_int32;
    }
    else if( value->value_type == BSON_TYPE_INT64 )
    {
        return value->value.v_int64;
    }
    else if( value->value_type == BSON_TYPE_DOUBLE )
    {
        return value->value.v_double;
    }
    else if( value->value_type == BSON_TYPE_BOOL )
    {
        return value->value.v_bool;
    }
    else if( value->value_type == BSON_TYPE_UTF8 )
    {
        return string( value->value.v_utf8.str, value->value.v_utf8.len );
    }
    else if( value->value_type == BSON_TYPE_ARRAY )
    {
        bson_iter_t arr_iter;
        bool        ok = bson_iter_recurse( iter, &arr_iter );
        RUNTIME_ASSERT( ok );

        DataBase::Array array;
        while( bson_iter_next( &arr_iter ) )
        {
            const bson_value_t* arr_value = bson_iter_value( &arr_iter );
            if( arr_value->value_type == BSON_TYPE_INT32 )
                array.push_back( arr_value->value.v_int32 );
            else if( arr_value->value_type == BSON_TYPE_INT64 )
                array.push_back( arr_value->value.v_int64 );
            else if( arr_value->value_type == BSON_TYPE_DOUBLE )
                array.push_back( arr_value->value.v_double );
            else if( arr_value->value_type == BSON_TYPE_BOOL )
                array.push_back( arr_value->value.v_bool );
            else if( arr_value->value_type == BSON_TYPE_UTF8 )
                array.push_back( string( arr_value->value.v_utf8.str, arr_value->value.v_utf8.len ) );
            else
                RUNTIME_ASSERT( !"Invalid type" );
        }

        return array;
    }
    else if( value->value_type == BSON_TYPE_DOCUMENT )
    {
        bson_iter_t doc_iter;
        bool        ok = bson_iter_recurse( iter, &doc_iter );
        RUNTIME_ASSERT( ok );

        DataBase::Dict dict;
        while( bson_iter_next( &doc_iter ) )
        {
            const char*         key = bson_iter_key( &doc_iter );
            const bson_value_t* dict_value = bson_iter_value( &doc_iter );
            if( dict_value->value_type == BSON_TYPE_INT32 )
            {
                dict.insert( std::make_pair( string( key ), dict_value->value.v_int32 ) );
            }
            else if( dict_value->value_type == BSON_TYPE_INT64 )
            {
                dict.insert( std::make_pair( string( key ), dict_value->value.v_int64 ) );
            }
            else if( dict_value->value_type == BSON_TYPE_DOUBLE )
            {
                dict.insert( std::make_pair( string( key ), dict_value->value.v_double ) );
            }
            else if( dict_value->value_type == BSON_TYPE_BOOL )
            {
                dict.insert( std::make_pair( string( key ), dict_value->value.v_bool ) );
            }
            else if( dict_value->value_type == BSON_TYPE_UTF8 )
            {
                dict.insert( std::make_pair( string( key ), string( dict_value->value.v_utf8.str, dict_value->value.v_utf8.len ) ) );
            }
            else if( dict_value->value_type == BSON_TYPE_ARRAY )
            {
                bson_iter_t doc_arr_iter;
                bool        ok = bson_iter_recurse( &doc_iter, &doc_arr_iter );
                RUNTIME_ASSERT( ok );

                DataBase::Array dict_array;
                while( bson_iter_next( &doc_arr_iter ) )
                {
                    const bson_value_t* doc_arr_value = bson_iter_value( &doc_arr_iter );
                    if( doc_arr_value->value_type == BSON_TYPE_INT32 )
                        dict_array.push_back( doc_arr_value->value.v_int32 );
                    else if( doc_arr_value->value_type == BSON_TYPE_INT64 )
                        dict_array.push_back( doc_arr_value->value.v_int64 );
                    else if( doc_arr_value->value_type == BSON_TYPE_DOUBLE )
                        dict_array.push_back( doc_arr_value->value.v_double );
                    else if( doc_arr_value->value_type == BSON_TYPE_BOOL )
                        dict_array.push_back( doc_arr_value->value.v_bool );
                    else if( doc_arr_value->value_type == BSON_TYPE_UTF8 )
                        dict_array.push_back( string( doc_arr_value->value.v_utf8.str, doc_arr_value->value.v_utf8.len ) );
                    else
                        RUNTIME_ASSERT( !"Invalid type" );
                }

                dict.insert( std::make_pair( string( key ), dict_array ) );
            }
            else
            {
                RUNTIME_ASSERT( !"Invalid type" );
            }
        }

        return dict;
    }
    else
    {
        RUNTIME_ASSERT( !"Invalid type" );
    }

    return 0;
}

static void BsonToDocument( const bson_t* bson, DataBase::Document& doc )
{
    bson_iter_t iter;
    bool        ok = bson_iter_init( &iter, bson );
    RUNTIME_ASSERT( ok );

    while( bson_iter_next( &iter ) )
    {
        const char* key = bson_iter_key( &iter );
        if( key[ 0 ] == '_' && key[ 1 ] == 'i' && key[ 2 ] == 'd' && key[ 3 ] == 0 )
            continue;

        DataBase::Value value = BsonToValue( &iter );
        doc.insert( std::make_pair( string( key ), std::move( value ) ) );
    }
}

DataBase::Document DataBase::Get( const string& collection_name, uint id )
{
    if( deletedRecords[ collection_name ].count( id ) )
        return Document();

    if( newRecords[ collection_name ].count( id ) )
        return recordChanges[ collection_name ][ id ];

    Document doc = GetRecord( collection_name, id );

    if( recordChanges[ collection_name ].count( id ) )
    {
        for( auto& kv : recordChanges[ collection_name ][ id ] )
            doc[ kv.first ] = kv.second;
    }

    return doc;
}

void DataBase::StartChanges()
{
    RUNTIME_ASSERT( !changesStarted );
    RUNTIME_ASSERT( recordChanges.empty() );
    RUNTIME_ASSERT( newRecords.empty() );
    RUNTIME_ASSERT( deletedRecords.empty() );

    changesStarted = true;
}

void DataBase::Insert( const string& collection_name, uint id, const Document& doc )
{
    RUNTIME_ASSERT( changesStarted );
    RUNTIME_ASSERT( !newRecords[ collection_name ].count( id ) );
    RUNTIME_ASSERT( !deletedRecords[ collection_name ].count( id ) );

    newRecords[ collection_name ].insert( id );
    for( auto& kv : doc )
        recordChanges[ collection_name ][ id ][ kv.first ] = kv.second;
}

void DataBase::Update( const string& collection_name, uint id, const string& key, const Value& value )
{
    RUNTIME_ASSERT( changesStarted );
    RUNTIME_ASSERT( !deletedRecords[ collection_name ].count( id ) );

    recordChanges[ collection_name ][ id ][ key ] = value;
}

void DataBase::Delete( const string& collection_name, uint id )
{
    RUNTIME_ASSERT( changesStarted );
    RUNTIME_ASSERT( !deletedRecords[ collection_name ].count( id ) );

    deletedRecords[ collection_name ].insert( id );
    newRecords[ collection_name ].erase( id );
    recordChanges[ collection_name ].erase( id );
}

void DataBase::CommitChanges()
{
    RUNTIME_ASSERT( changesStarted );

    changesStarted = false;

    for( auto& collection : recordChanges )
    {
        for( auto& data : collection.second )
        {
            auto it = newRecords.find( collection.first );
            if( it != newRecords.end() && it->second.count( data.first ) )
                InsertRecord( collection.first, data.first, data.second );
            else
                UpdateRecord( collection.first, data.first, data.second );
        }
    }

    for( auto& collection : deletedRecords )
        for( auto & id : collection.second )
            DeleteRecord( collection.first, id );

    recordChanges.clear();
    newRecords.clear();
    deletedRecords.clear();

    CommitRecords();
}

class DbJson: public DataBase
{
    string storageDir;

public:
    static DbJson* Create( const string& storage_dir )
    {
        File::CreateDirectoryTree( storage_dir + "/" );

        DbJson* db_json = new DbJson();
        db_json->storageDir = storage_dir;
        return db_json;
    }

    virtual UIntVec GetAllIds( const string& collection_name ) override
    {
        UIntVec ids;
        StrVec  paths;
        File::GetFolderFileNames( storageDir + "/" + collection_name + "/", false, "json", paths );
        ids.reserve( paths.size() );
        for( const string& path : paths )
        {
            string id_str = _str( path ).extractFileName().eraseFileExtension();
            uint   id = _str( id_str ).toUInt();
            RUNTIME_ASSERT( id );
            ids.push_back( id );
        }
        return ids;
    }

protected:
    virtual Document GetRecord( const string& collection_name, uint id ) override
    {
        string path = File::GetWritePath( _str( "{}/{}/{}.json", storageDir, collection_name, id ) );
        void*  f = FileOpen( path.c_str(), false );
        if( !f )
            return Document();

        uint  length = FileGetSize( f );
        char* json = new char[ length ];
        bool  read_ok = FileRead( f, json, length );
        RUNTIME_ASSERT( read_ok );

        FileClose( f );

        bson_t       bson;
        bson_error_t error;
        bool         parse_json_ok = bson_init_from_json( &bson, json, length, &error );
        RUNTIME_ASSERT( parse_json_ok );

        delete[] json;

        Document doc;
        BsonToDocument( &bson, doc );

        bson_destroy( &bson );
        return doc;
    }

    virtual void InsertRecord( const string& collection_name, uint id, const Document& doc ) override
    {
        RUNTIME_ASSERT( !doc.empty() );

        string path = File::GetWritePath( _str( "{}/{}/{}.json", storageDir, collection_name, id ) );
        void*  f_check = FileOpen( path.c_str(), false );
        RUNTIME_ASSERT( !f_check );

        bson_t bson;
        bson_init( &bson );
        DocumentToBson( doc, &bson );

        size_t length;
        char*  json = bson_as_canonical_extended_json( &bson, &length );
        RUNTIME_ASSERT( json );
        bson_destroy( &bson );

        nlohmann::json pretty_json = nlohmann::json::parse( json );
        string         pretty_json_dump = pretty_json.dump( 4 );
        bson_free( json );

        void* f = FileOpen( path.c_str(), true );
        RUNTIME_ASSERT( f );

        bool write_ok = FileWrite( f, pretty_json_dump.c_str(), (uint) pretty_json_dump.length() );
        RUNTIME_ASSERT( write_ok );

        FileClose( f );
    }

    virtual void UpdateRecord( const string& collection_name, uint id, const Document& doc ) override
    {
        RUNTIME_ASSERT( !doc.empty() );

        string path = File::GetWritePath( _str( "{}/{}/{}.json", storageDir, collection_name, id ) );
        void*  f_read = FileOpen( path.c_str(), false );
        RUNTIME_ASSERT( f_read );

        uint  length = FileGetSize( f_read );
        char* json = new char[ length ];
        bool  read_ok = FileRead( f_read, json, length );
        RUNTIME_ASSERT( read_ok );

        FileClose( f_read );

        bson_t       bson;
        bson_error_t error;
        bool         parse_json_ok = bson_init_from_json( &bson, json, length, &error );
        RUNTIME_ASSERT( parse_json_ok );

        delete[] json;

        DocumentToBson( doc, &bson );

        size_t new_length;
        char*  new_json = bson_as_canonical_extended_json( &bson, &new_length );
        RUNTIME_ASSERT( new_json );
        bson_destroy( &bson );

        nlohmann::json pretty_json = nlohmann::json::parse( new_json );
        string         pretty_json_dump = pretty_json.dump( 4 );
        bson_free( new_json );

        void* f_write = FileOpen( path.c_str(), true );
        RUNTIME_ASSERT( f_write );

        bool write_ok = FileWrite( f_write, pretty_json_dump.c_str(), (uint) pretty_json_dump.length() );
        RUNTIME_ASSERT( write_ok );

        FileClose( f_write );
    }

    virtual void DeleteRecord( const string& collection_name, uint id ) override
    {
        string path = File::GetWritePath( _str( "{}/{}/{}.json", storageDir, collection_name, id ) );
        bool   file_deleted = FileDelete( path );
        RUNTIME_ASSERT( file_deleted );
    }

    virtual void CommitRecords() override
    {
        // Nothing
    }
};

class DbUnQLite: public DataBase
{
    string                  storageDir;
    map< string, unqlite* > collections;

public:
    static DbUnQLite* Create( const string& storage_dir )
    {
        File::CreateDirectoryTree( storage_dir + "/" );

        unqlite* ping_db;
        string   ping_db_path = storage_dir + "/Ping.unqlite";
        if( unqlite_open( &ping_db, ping_db_path.c_str(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING ) != UNQLITE_OK )
        {
            WriteLog( "UnQLite : Can't open db at '{}'.\n", ping_db_path );
            return nullptr;
        }

        uint ping = 42;
        if( unqlite_kv_store( ping_db, &ping, sizeof( ping ), &ping, sizeof( ping ) ) != UNQLITE_OK )
        {
            WriteLog( "UnQLite : Can't write to db at '{}'.\n", ping_db_path );
            unqlite_close( ping_db );
            return nullptr;
        }

        unqlite_close( ping_db );
        File::DeleteFile( ping_db_path );

        DbUnQLite* db_unqlite = new DbUnQLite();
        db_unqlite->storageDir = storage_dir;
        return db_unqlite;
    }

    ~DbUnQLite()
    {
        for( auto& kv : collections )
            unqlite_close( kv.second );
        collections.clear();
    }

    virtual UIntVec GetAllIds( const string& collection_name ) override
    {
        unqlite* db = GetCollection( collection_name );
        RUNTIME_ASSERT( db );

        unqlite_kv_cursor* cursor;
        int                kv_cursor_init = unqlite_kv_cursor_init( db, &cursor );
        RUNTIME_ASSERT( kv_cursor_init == UNQLITE_OK );

        int kv_cursor_first_entry = unqlite_kv_cursor_first_entry( cursor );
        RUNTIME_ASSERT( ( kv_cursor_first_entry == UNQLITE_OK || kv_cursor_first_entry == UNQLITE_DONE ) );

        UIntVec ids;
        while( unqlite_kv_cursor_valid_entry( cursor ) )
        {
            uint id;
            int  kv_cursor_key_callback = unqlite_kv_cursor_key_callback( cursor,
                                                                          [] ( const void* output, unsigned int output_len, void* user_data )
                                                                          {
                                                                              RUNTIME_ASSERT( output_len == sizeof( uint ) );
                                                                              *(uint*) user_data = *(uint*) output;
                                                                              return UNQLITE_OK;
                                                                          }, &id );
            RUNTIME_ASSERT( kv_cursor_key_callback == UNQLITE_OK );
            RUNTIME_ASSERT( id != 0 );

            ids.push_back( id );

            int kv_cursor_next_entry = unqlite_kv_cursor_next_entry( cursor );
            RUNTIME_ASSERT( ( kv_cursor_next_entry == UNQLITE_OK || kv_cursor_next_entry == UNQLITE_DONE ) );
        }

        return ids;
    }

protected:
    virtual Document GetRecord( const string& collection_name, uint id ) override
    {
        unqlite* db = GetCollection( collection_name );
        RUNTIME_ASSERT( db );

        Document doc;
        int      kv_fetch_callback = unqlite_kv_fetch_callback( db, &id, sizeof( id ),
                                                                [] ( const void* output, unsigned int output_len, void* user_data )
                                                                {
                                                                    bson_t bson;
                                                                    bool init_static = bson_init_static( &bson, (const uint8_t*) output, output_len );
                                                                    RUNTIME_ASSERT( init_static );

                                                                    Document& doc = *(Document*) user_data;
                                                                    BsonToDocument( &bson, doc );

                                                                    return UNQLITE_OK;
                                                                }, &doc );
        RUNTIME_ASSERT( ( kv_fetch_callback == UNQLITE_OK || kv_fetch_callback == UNQLITE_NOTFOUND ) );

        return doc;
    }

    virtual void InsertRecord( const string& collection_name, uint id, const Document& doc ) override
    {
        RUNTIME_ASSERT( !doc.empty() );

        unqlite* db = GetCollection( collection_name );
        RUNTIME_ASSERT( db );

        int kv_fetch_callback = unqlite_kv_fetch_callback( db, &id, sizeof( id ),
                                                           [] ( const void*, unsigned int, void* )
                                                           {
                                                               return UNQLITE_OK;
                                                           }, nullptr );
        RUNTIME_ASSERT( kv_fetch_callback == UNQLITE_NOTFOUND );

        bson_t bson;
        bson_init( &bson );
        DocumentToBson( doc, &bson );

        const uint8_t* bson_data = bson_get_data( &bson );
        RUNTIME_ASSERT( bson_data );

        int kv_store = unqlite_kv_store( db, &id, sizeof( id ), bson_data, bson.len );
        RUNTIME_ASSERT( kv_store == UNQLITE_OK );

        bson_destroy( &bson );
    }

    virtual void UpdateRecord( const string& collection_name, uint id, const Document& doc ) override
    {
        RUNTIME_ASSERT( !doc.empty() );

        unqlite* db = GetCollection( collection_name );
        RUNTIME_ASSERT( db );

        Document actual_doc = Get( collection_name, id );
        RUNTIME_ASSERT( !actual_doc.empty() );

        for( auto& kv : doc )
            actual_doc[ kv.first ] = kv.second;

        bson_t bson;
        bson_init( &bson );
        DocumentToBson( actual_doc, &bson );

        const uint8_t* bson_data = bson_get_data( &bson );
        RUNTIME_ASSERT( bson_data );

        int kv_store = unqlite_kv_store( db, &id, sizeof( id ), bson_data, bson.len );
        RUNTIME_ASSERT( kv_store == UNQLITE_OK );

        bson_destroy( &bson );
    }

    virtual void DeleteRecord( const string& collection_name, uint id ) override
    {
        unqlite* db = GetCollection( collection_name );
        RUNTIME_ASSERT( db );

        int kv_delete = unqlite_kv_delete( db, &id, sizeof( id ) );
        RUNTIME_ASSERT( kv_delete == UNQLITE_OK );
    }

    virtual void CommitRecords() override
    {
        for( auto collection : collections )
        {
            int commit = unqlite_commit( collection.second );
            RUNTIME_ASSERT( commit == UNQLITE_OK );
        }
    }

private:
    unqlite* GetCollection( const string& collection_name )
    {
        unqlite* db;
        auto     it = collections.find( collection_name );
        if( it == collections.end() )
        {
            string db_path = _str( "{}/{}.unqlite", storageDir, collection_name );
            int    r = unqlite_open( &db, db_path.c_str(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING );
            if( r != UNQLITE_OK )
            {
                WriteLog( "UnQLite : Can't open db at '{}'.\n", collection_name );
                return nullptr;
            }

            collections.insert( std::make_pair( collection_name, db ) );
        }
        else
        {
            db = it->second;
        }
        return db;
    }
};

#ifndef REMOVE_MONGO
class DbMongo: public DataBase
{
    mongoc_client_t*                    client;
    mongoc_database_t*                  database;
    string                              databaseName;
    map< string, mongoc_collection_t* > collections;

public:
    static DbMongo* Create( const string& uri, const string& db_name )
    {
        mongoc_init();

        bson_error_t  error;
        mongoc_uri_t* mongo_uri = mongoc_uri_new_with_error( uri.c_str(), &error );
        if( !mongo_uri )
        {
            WriteLog( "Mongo : Failed to parse URI: {}, error: {}.\n", uri, error.message );
            return nullptr;
        }

        mongoc_client_t* client = mongoc_client_new_from_uri( mongo_uri );
        if( !client )
        {
            WriteLog( "Mongo : Can't create client.\n" );
            return nullptr;
        }

        mongoc_uri_destroy( mongo_uri );
        mongoc_client_set_appname( client, "fonline" );

        mongoc_database_t* database = mongoc_client_get_database( client, db_name.c_str() );
        if( !database )
        {
            WriteLog( "Mongo : Can't get database '{}'.\n", db_name );
            return nullptr;
        }

        // Ping
        bson_t* ping = BCON_NEW( "ping", BCON_INT32( 1 ) );
        bson_t  reply;
        if( !mongoc_client_command_simple( client, "admin", ping, nullptr, &reply, &error ) )
        {
            WriteLog( "Mongo : Can't ping database, error: {}.\n", error.message );
            return nullptr;
        }

        DbMongo* db_mongo = new DbMongo();
        db_mongo->client = client;
        db_mongo->database = database;
        db_mongo->databaseName = db_name;
        return db_mongo;
    }

    ~DbMongo()
    {
        for( auto& kv : collections )
            mongoc_collection_destroy( kv.second );
        collections.clear();

        mongoc_database_destroy( database );
        mongoc_client_destroy( client );
        mongoc_cleanup();
    }

    virtual UIntVec GetAllIds( const string& collection_name ) override
    {
        mongoc_collection_t* collection = GetCollection( collection_name );
        RUNTIME_ASSERT( collection );

        bson_t fields;
        bson_init( &fields );

        bool append_int32 = bson_append_int32( &fields, "_id", 3, 1 );
        RUNTIME_ASSERT( append_int32 );

        bson_t query;
        bson_init( &query );

        mongoc_cursor_t* cursor = mongoc_collection_find( collection, MONGOC_QUERY_NONE, 0, 0, uint( -1 ), &query, &fields, nullptr );
        RUNTIME_ASSERT( cursor );

        UIntVec       ids;
        const bson_t* document;
        while( mongoc_cursor_next( cursor, &document ) )
        {
            bson_iter_t iter;
            bool        iter_init = bson_iter_init( &iter, document );
            RUNTIME_ASSERT( iter_init );
            bool        iter_next = bson_iter_next( &iter );
            RUNTIME_ASSERT( iter_next );
            RUNTIME_ASSERT( bson_iter_type( &iter ) == BSON_TYPE_INT32 );

            uint id = bson_iter_int32( &iter );
            ids.push_back( id );
        }

        bson_error_t error;
        RUNTIME_ASSERT_STR( !mongoc_cursor_error( cursor, &error ), error.message );

        mongoc_cursor_destroy( cursor );
        bson_destroy( &query );
        bson_destroy( &fields );
        return ids;
    }

protected:
    virtual Document GetRecord( const string& collection_name, uint id ) override
    {
        mongoc_collection_t* collection = GetCollection( collection_name );
        RUNTIME_ASSERT( collection );

        bson_t query;
        bson_init( &query );

        bool append_int32 = bson_append_int32( &query, "_id", 3, id );
        RUNTIME_ASSERT( append_int32 );

        mongoc_cursor_t* cursor = mongoc_collection_find( collection, MONGOC_QUERY_NONE, 0, 1, 0, &query, nullptr, nullptr );
        RUNTIME_ASSERT( cursor );

        const bson_t* bson;
        if( !mongoc_cursor_next( cursor, &bson ) )
        {
            mongoc_cursor_destroy( cursor );
            bson_destroy( &query );
            return Document();
        }

        Document doc;
        BsonToDocument( bson, doc );

        mongoc_cursor_destroy( cursor );
        bson_destroy( &query );
        return doc;
    }

    virtual void InsertRecord( const string& collection_name, uint id, const Document& doc ) override
    {
        RUNTIME_ASSERT( !doc.empty() );

        mongoc_collection_t* collection = GetCollection( collection_name );
        RUNTIME_ASSERT( collection );

        bson_t insert;
        bson_init( &insert );

        bool append_int32 = bson_append_int32( &insert, "_id", 3, id );
        RUNTIME_ASSERT( append_int32 );

        DocumentToBson( doc, &insert );

        bson_error_t error;
        bool         collection_insert = mongoc_collection_insert( collection, MONGOC_INSERT_NONE, &insert, nullptr, &error );
        RUNTIME_ASSERT_STR( collection_insert, error.message );

        bson_destroy( &insert );
    }

    virtual void UpdateRecord( const string& collection_name, uint id, const Document& doc ) override
    {
        RUNTIME_ASSERT( !doc.empty() );

        mongoc_collection_t* collection = GetCollection( collection_name );
        RUNTIME_ASSERT( collection );

        bson_t selector;
        bson_init( &selector );

        bool append_int32 = bson_append_int32( &selector, "_id", 3, id );
        RUNTIME_ASSERT( append_int32 );

        bson_t update;
        bson_init( &update );

        bson_t update_set;
        bool   append_document_begin = bson_append_document_begin( &update, "$set", 4, &update_set );
        RUNTIME_ASSERT( append_document_begin );

        DocumentToBson( doc, &update_set );

        bool append_document_end = bson_append_document_end( &update, &update_set );
        RUNTIME_ASSERT( append_document_end );

        bson_error_t error;
        bool         collection_update = mongoc_collection_update( collection, MONGOC_UPDATE_NONE, &selector, &update, nullptr, &error );
        RUNTIME_ASSERT_STR( collection_update, error.message );

        bson_destroy( &selector );
        bson_destroy( &update );
    }

    virtual void DeleteRecord( const string& collection_name, uint id ) override
    {
        mongoc_collection_t* collection = GetCollection( collection_name );
        RUNTIME_ASSERT( collection );

        bson_t selector;
        bson_init( &selector );

        bool append_int32 = bson_append_int32( &selector, "_id", 3, id );
        RUNTIME_ASSERT( append_int32 );

        bson_error_t error;
        bool         collection_remove = mongoc_collection_remove( collection, MONGOC_REMOVE_SINGLE_REMOVE, &selector, nullptr, &error );
        RUNTIME_ASSERT_STR( collection_remove, error.message );

        bson_destroy( &selector );
    }

    virtual void CommitRecords() override
    {
        // Nothing
    }

private:
    mongoc_collection_t* GetCollection( const string& collection_name )
    {
        mongoc_collection_t* collection;
        auto                 it = collections.find( collection_name );
        if( it == collections.end() )
        {
            collection = mongoc_client_get_collection( client, databaseName.c_str(), collection_name.c_str() );
            if( !collection )
            {
                WriteLog( "Mongo : Can't get collection '{}'.\n", collection_name );
                return nullptr;
            }

            collections.insert( std::make_pair( collection_name, collection ) );
        }
        else
        {
            collection = it->second;
        }
        return collection;
    }
};
#endif

class DbMemory: public DataBase
{
    Collections collections;

public:
    static DbMemory* Create()
    {
        return new DbMemory();
    }

    virtual UIntVec GetAllIds( const string& collection_name ) override
    {
        Collection& collection = collections[ collection_name ];

        UIntVec     ids;
        ids.reserve( collection.size() );
        for( auto& kv : collection )
            ids.push_back( kv.first );

        return ids;
    }

protected:
    virtual Document GetRecord( const string& collection_name, uint id ) override
    {
        Collection& collection = collections[ collection_name ];

        auto        it = collection.find( id );
        return it != collection.end() ? it->second : Document();
    }

    virtual void InsertRecord( const string& collection_name, uint id, const Document& doc ) override
    {
        RUNTIME_ASSERT( !doc.empty() );

        Collection& collection = collections[ collection_name ];
        RUNTIME_ASSERT( !collection.count( id ) );

        collection.insert( std::make_pair( id, doc ) );
    }

    virtual void UpdateRecord( const string& collection_name, uint id, const Document& doc ) override
    {
        RUNTIME_ASSERT( !doc.empty() );

        Collection& collection = collections[ collection_name ];

        auto        it = collection.find( id );
        RUNTIME_ASSERT( it != collection.end() );

        for( auto& kv : doc )
            it->second[ kv.first ] = kv.second;
    }

    virtual void DeleteRecord( const string& collection_name, uint id ) override
    {
        Collection& collection = collections[ collection_name ];

        auto        it = collection.find( id );
        RUNTIME_ASSERT( it != collection.end() );

        collection.erase( it );
    }

    virtual void CommitRecords() override
    {
        // Nothing
    }
};

DataBase* GetDataBase( const string& connection_info )
{
    auto options = _str( connection_info ).split( ' ' );
    if( options[ 0 ] == "JSON" && options.size() == 2 )
        return DbJson::Create( options[ 1 ] );
    else if( options[ 0 ] == "UnQLite" && options.size() == 2 )
        return DbUnQLite::Create( options[ 1 ] );
    #ifndef REMOVE_MONGO
    else if( options[ 0 ] == "Mongo" && options.size() == 3 )
        return DbMongo::Create( options[ 1 ], options[ 2 ] );
    #endif
    else if( options[ 0 ] == "Memory" && options.size() == 1 )
        return DbMemory::Create();

    WriteLog( "Wrong storage options.\n" );
    return nullptr;
}
