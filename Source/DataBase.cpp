#include "DataBase.h"
#include "IniParser.h"
#include "unqlite.h"
#include "mongoc.h"

DataBase* DbStorage;

class DbTxt: public DataBase
{
    string storageDir;

public:
    static DbTxt* Create( const string& storage_dir )
    {
        FileManager::CreateDirectoryTree( storage_dir + "/" );

        DbTxt* db_txt = new DbTxt();
        db_txt->storageDir = storage_dir;
        return db_txt;
    }

    virtual UIntVec GetAllIds( const string& collection_name ) override
    {
        UIntVec ids;
        StrVec  paths;
        FileManager::GetFolderFileNames( storageDir + "/" + collection_name + "/", false, "txt", paths );
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

    virtual StrMap Get( const string& collection_name, uint id ) override
    {
        IniParser ini;
        ini.AppendFile( _str( "{}/{}/{}.txt", storageDir, collection_name, id ) );
        if( ini.IsLoaded() )
            return ini.GetApp( "" );
        return StrMap();
    }

    virtual void Insert( const string& collection_name, uint id, const StrMap& data ) override
    {
        RUNTIME_ASSERT( !data.empty() );

        StrMap actual_data = Get( collection_name, id );
        RUNTIME_ASSERT( actual_data.empty() );

        IniParser ini;
        ini.SetApp( "" ) = data;
        bool      txt_saved = ini.SaveFile( _str( "{}/{}/{}.txt", storageDir, collection_name, id ) );
        RUNTIME_ASSERT( txt_saved );
    }

    virtual void Update( const string& collection_name, uint id, const StrMap& data, bool upsert /* = false */ ) override
    {
        RUNTIME_ASSERT( !data.empty() );

        StrMap actual_data = Get( collection_name, id );
        if( actual_data.empty() && !upsert )
            return;

        for( const auto& kv : data )
            actual_data[ kv.first ] = kv.second;

        IniParser ini;
        ini.SetApp( "" ) = actual_data;
        bool      txt_saved = ini.SaveFile( _str( "{}/{}/{}.txt", storageDir, collection_name, id ) );
        RUNTIME_ASSERT( txt_saved );
    }

    virtual void Delete( const string& collection_name, uint id ) override
    {
        bool txt_deleted = FileManager::DeleteFile( _str( "{}/{}/{}.txt", storageDir, collection_name, id ) );
        RUNTIME_ASSERT( txt_deleted );
    }
};

class DbUnQLite: public DataBase
{
    string                  storageDir;
    map< string, unqlite* > collections;

public:
    static DbUnQLite* Create( const string& storage_dir )
    {
        if( !storage_dir.empty() )
            FileManager::CreateDirectoryTree( storage_dir + "/" );

        unqlite* ping_db;
        string   ping_db_path = storage_dir + "/Ping.unqlite";
        int      mode = storage_dir.empty() ? UNQLITE_OPEN_IN_MEMORY : UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING;
        if( unqlite_open( &ping_db, ping_db_path.c_str(), mode ) != UNQLITE_OK )
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
        FileManager::DeleteFile( ping_db_path );

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
        RUNTIME_ASSERT( kv_cursor_first_entry == UNQLITE_OK );

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
            RUNTIME_ASSERT( kv_cursor_next_entry == UNQLITE_OK );
        }

        return ids;
    }

    virtual StrMap Get( const string& collection_name, uint id ) override
    {
        unqlite* db = GetCollection( collection_name );
        RUNTIME_ASSERT( db );

        StrMap data;
        int    kv_fetch_callback = unqlite_kv_fetch_callback( db, &id, sizeof( id ),
                                                              [] ( const void* output, unsigned int output_len, void* user_data )
                                                              {
                                                                  bson_t bson;
                                                                  bool init_static = bson_init_static( &bson, (const uint8_t*) output, output_len );
                                                                  RUNTIME_ASSERT( init_static );

                                                                  bson_iter_t iter;
                                                                  bool iter_init = bson_iter_init( &iter, &bson );
                                                                  RUNTIME_ASSERT( iter_init );

                                                                  StrMap& data = *(StrMap*) user_data;
                                                                  while( bson_iter_next( &iter ) )
                                                                  {
                                                                      string key = bson_iter_key( &iter );

                                                                      const bson_value_t* bson_value = bson_iter_value( &iter );
                                                                      RUNTIME_ASSERT( bson_value->value_type == BSON_TYPE_UTF8 );

                                                                      string value;
                                                                      value.assign( bson_value->value.v_utf8.str, bson_value->value.v_utf8.len );

                                                                      data.insert( std::make_pair( std::move( key ), std::move( value ) ) );
                                                                  }

                                                                  return UNQLITE_OK;
                                                              }, &data );
        RUNTIME_ASSERT( kv_fetch_callback == UNQLITE_OK || kv_fetch_callback == UNQLITE_NOTFOUND );

        return data;
    }

    virtual void Insert( const string& collection_name, uint id, const StrMap& data ) override
    {
        RUNTIME_ASSERT( !data.empty() );

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

        for( auto& kv : data )
        {
            bool append_utf8 = bson_append_utf8( &bson, kv.first.c_str(),
                                                 (int) kv.first.length(), kv.second.c_str(), (int) kv.second.length() );
            RUNTIME_ASSERT( append_utf8 );
        }

        const uint8_t* bson_data = bson_get_data( &bson );
        RUNTIME_ASSERT( bson_data );

        int kv_store = unqlite_kv_store( db, &id, sizeof( id ), bson_data, bson.len );
        RUNTIME_ASSERT( kv_store == UNQLITE_OK );

        int commit = unqlite_commit( db );
        RUNTIME_ASSERT( commit == UNQLITE_OK );

        bson_destroy( &bson );
    }

    virtual void Update( const string& collection_name, uint id, const StrMap& data, bool upsert /* = false */ ) override
    {
        RUNTIME_ASSERT( !data.empty() );

        unqlite* db = GetCollection( collection_name );
        RUNTIME_ASSERT( db );

        StrMap actual_data = Get( collection_name, id );
        RUNTIME_ASSERT( !actual_data.empty() || upsert );

        bson_t bson;
        bson_init( &bson );

        for( auto& kv : data )
            actual_data[ kv.first ] = kv.second;

        for( auto& kv : actual_data )
        {
            bool append_utf8 = bson_append_utf8( &bson, kv.first.c_str(),
                                                 (int) kv.first.length(), kv.second.c_str(), (int) kv.second.length() );
            RUNTIME_ASSERT( append_utf8 );
        }

        const uint8_t* bson_data = bson_get_data( &bson );
        RUNTIME_ASSERT( bson_data );

        int kv_store = unqlite_kv_store( db, &id, sizeof( id ), bson_data, bson.len );
        RUNTIME_ASSERT( kv_store == UNQLITE_OK );

        int commit = unqlite_commit( db );
        RUNTIME_ASSERT( commit == UNQLITE_OK );

        bson_destroy( &bson );
    }

    virtual void Delete( const string& collection_name, uint id ) override
    {
        unqlite* db = GetCollection( collection_name );
        RUNTIME_ASSERT( db );

        int kv_delete = unqlite_kv_delete( db, &id, sizeof( id ) );
        RUNTIME_ASSERT( kv_delete == UNQLITE_OK );

        int commit = unqlite_commit( db );
        RUNTIME_ASSERT( commit == UNQLITE_OK );
    }

private:
    unqlite* GetCollection( const string& collection_name )
    {
        unqlite* db;
        auto     it = collections.find( collection_name );
        if( it == collections.end() )
        {
            string db_path = _str( "{}{}{}.unqlite", storageDir, storageDir.empty() ? "" : "/", collection_name );
            int    mode = storageDir.empty() ? UNQLITE_OPEN_IN_MEMORY : UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING;
            int    r = unqlite_open( &db, db_path.c_str(), mode );
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

    virtual StrMap Get( const string& collection_name, uint id ) override
    {
        mongoc_collection_t* collection = GetCollection( collection_name );
        RUNTIME_ASSERT( collection );

        bson_t query;
        bson_init( &query );

        bool append_int32 = bson_append_int32( &query, "_id", 3, id );
        RUNTIME_ASSERT( append_int32 );

        mongoc_cursor_t* cursor = mongoc_collection_find( collection, MONGOC_QUERY_NONE, 0, 1, 0, &query, nullptr, nullptr );
        RUNTIME_ASSERT( cursor );

        const bson_t* document;
        if( !mongoc_cursor_next( cursor, &document ) )
            return StrMap();

        StrMap      data;
        bson_iter_t iter;
        bool        iter_init = bson_iter_init( &iter, document );
        RUNTIME_ASSERT( iter_init );

        while( bson_iter_next( &iter ) )
        {
            const char* key = bson_iter_key( &iter );
            RUNTIME_ASSERT( key );

            if( key[ 0 ] == '_' && key[ 1 ] == 'i' && key[ 2 ] == 'd' )
            {
                RUNTIME_ASSERT( bson_iter_type( &iter ) == BSON_TYPE_INT32 );

                uint document_id = bson_iter_int32( &iter );
                RUNTIME_ASSERT( id == document_id );
            }
            else
            {
                RUNTIME_ASSERT( bson_iter_type( &iter ) == BSON_TYPE_UTF8 );

                uint32_t    length;
                const char* str = bson_iter_utf8( &iter, &length );
                RUNTIME_ASSERT( str );

                string value;
                value.assign( str, length );

                data.insert( std::make_pair( string( key ), value ) );
            }
        }

        mongoc_cursor_destroy( cursor );
        bson_destroy( &query );
        return data;
    }

    virtual void Insert( const string& collection_name, uint id, const StrMap& data ) override
    {
        RUNTIME_ASSERT( !data.empty() );

        mongoc_collection_t* collection = GetCollection( collection_name );
        RUNTIME_ASSERT( collection );

        bson_t insert;
        bson_init( &insert );

        bool append_int32 = bson_append_int32( &insert, "_id", 3, id );
        RUNTIME_ASSERT( append_int32 );

        for( auto& kv : data )
        {
            bool append_utf8 = bson_append_utf8( &insert, kv.first.c_str(),
                                                 (int) kv.first.length(), kv.second.c_str(), (int) kv.second.length() );
            RUNTIME_ASSERT( append_utf8 );
        }

        bson_error_t error;
        bool         collection_insert = mongoc_collection_insert( collection, MONGOC_INSERT_NONE, &insert, nullptr, &error );
        RUNTIME_ASSERT_STR( collection_insert, error.message );

        bson_destroy( &insert );
    }

    virtual void Update( const string& collection_name, uint id, const StrMap& data, bool upsert /* = false */ ) override
    {
        RUNTIME_ASSERT( !data.empty() );

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

        for( auto& kv : data )
        {
            bool append_utf8 = bson_append_utf8( &update_set, kv.first.c_str(),
                                                 (int) kv.first.length(), kv.second.c_str(), (int) kv.second.length() );
            RUNTIME_ASSERT( append_utf8 );
        }

        bool append_document_end = bson_append_document_end( &update, &update_set );
        RUNTIME_ASSERT( append_document_end );

        bson_error_t error;
        bool         collection_update = mongoc_collection_update( collection, upsert ? MONGOC_UPDATE_UPSERT : MONGOC_UPDATE_NONE, &selector, &update, nullptr, &error );
        RUNTIME_ASSERT_STR( collection_update, error.message );

        bson_destroy( &selector );
        bson_destroy( &update );
        bson_destroy( &update_set );
    }

    virtual void Delete( const string& collection_name, uint id ) override
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

DataBase* GetDataBase( const string& connection_info )
{
    auto options = _str( connection_info ).split( ' ' );
    if( options[ 0 ] == "Txt" && options.size() == 2 )
        return DbTxt::Create( options[ 1 ] );
    else if( options[ 0 ] == "UnQLite" && options.size() == 2 )
        return DbUnQLite::Create( options[ 1 ] );
    else if( options[ 0 ] == "Mongo" && options.size() == 3 )
        return DbMongo::Create( options[ 1 ], options[ 2 ] );
    else if( options[ 0 ] == "Memory" && options.size() == 1 )
        return DbUnQLite::Create( "" );

    WriteLog( "Storage : Wrong data base connection info '{}'.\n", connection_info );
    return nullptr;
}
