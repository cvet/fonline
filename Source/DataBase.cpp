#include "DataBase.h"
#include "IniParser.h"
#include "FileSystem.h"
#include <mongoc.h>

DataBase* DbStorage;

class DbDisk: public DataBase
{
    string dir;

public:
    static DbDisk* Create( const string& storage_dir )
    {
        DbDisk* dbDisk = new DbDisk();
        dbDisk->dir = storage_dir;
        return dbDisk;
    }

    virtual UIntVec GetAllIds( const string& collection_name ) override
    {
        UIntVec ids;
        StrVec  paths;
        FileManager::GetFolderFileNames( dir + "/" + collection_name + "/", false, "txt", paths );
        ids.reserve( paths.size() );
        for( const string& path : paths )
        {
            string id_str = _str( path ).extractFileName().eraseFileExtension();
            uint   id = _str( id_str ).toUInt();
            if( id )
                ids.push_back( id );
        }
        return ids;
    }

    virtual StrMap Get( const string& collection_name, uint id ) override
    {
        IniParser ini;
        ini.AppendFile( _str( "{}/{}/{}.txt", dir, collection_name, id ) );
        if( ini.IsLoaded() )
            return ini.GetApp( "" );
        return StrMap();
    }

    virtual bool Insert( const string& collection_name, uint id, const StrMap& data ) override
    {
        StrMap actual_data = Get( collection_name, id );
        if( !actual_data.empty() )
            return false;

        IniParser ini;
        ini.SetApp( "" ) = data;
        return ini.SaveFile( _str( "{}/{}/{}.txt", dir, collection_name, id ) );
    }

    virtual bool Update( const string& collection_name, uint id, const StrMap& data, bool upsert /* = false */ ) override
    {
        StrMap actual_data = Get( collection_name, id );
        if( actual_data.empty() && !upsert )
            return false;

        for( const auto& kv : data )
            actual_data[ kv.first ] = kv.second;

        IniParser ini;
        ini.SetApp( "" ) = actual_data;
        return ini.SaveFile( _str( "{}/{}/{}.txt", dir, collection_name, id ) );
    }

    virtual bool Delete( const string& collection_name, uint id ) override
    {
        return FileManager::DeleteFile( _str( "{}/{}/{}.txt", dir, collection_name, id ) );
    }
};


class DbDiskV2: public DataBase
{
    string dir;

public:
    static DbDiskV2* Create( const string& storage_dir )
    {
        DbDiskV2* dbDisk = new DbDiskV2();
        dbDisk->dir = storage_dir;
        return dbDisk;
    }

    virtual UIntVec GetAllIds( const string& collection_name ) override
    {
        UIntSet ids;
        StrVec  paths;
        FileManager::GetFolderFileNames( dir + "/" + collection_name + "/", false, "", paths );
        for( const string& path : paths )
        {
            string id_str = _str( path ).getFileExtension();
            uint   id = _str( id_str ).toUInt();
            if( id )
                ids.insert( id );
        }

        UIntVec ids_vec;
        ids_vec.reserve( ids.size() );
        for( uint id : ids )
            ids_vec.push_back( id );

        return ids_vec;
    }

    virtual StrMap Get( const string& collection_name, uint id ) override
    {
        StrVec paths;
        FileManager::GetFolderFileNames( dir + "/" + collection_name + "/", false, _str( "{}", id ), paths );

        StrMap data;
        for( const string& path : paths )
        {
            void* f = FileOpen( path, false );
            RUNTIME_ASSERT( f );

            uint   size = FileGetSize( f );
            string value( size, ' ' );

            if( size > 0 )
            {
                bool file_read = FileRead( f, &value[ 0 ], size );
                RUNTIME_ASSERT( file_read );
            }

            string key = _str( path ).extractFileName().eraseFileExtension();
            data.insert( std::make_pair( key, value ) );

            FileClose( f );
        }

        return data;
    }

    virtual bool Insert( const string& collection_name, uint id, const StrMap& data ) override
    {
        StrVec paths;
        FileManager::GetFolderFileNames( dir + "/" + collection_name + "/", false, _str( "{}", id ), paths );
        if( !paths.empty() )
            return false;

        for( auto& kv : data )
        {
            void* f = FileOpen( _str( "{}/{}/{}.{}", dir, collection_name, kv.first, id ), true );
            RUNTIME_ASSERT( f );

            if( !kv.second.empty() )
            {
                bool file_write = FileWrite( f, &kv.second[ 0 ], (uint) kv.second.length() );
                RUNTIME_ASSERT( file_write );
            }

            FileClose( f );
        }

        return true;
    }

    virtual bool Update( const string& collection_name, uint id, const StrMap& data, bool upsert /* = false */ ) override
    {
        StrVec paths;
        FileManager::GetFolderFileNames( dir + "/" + collection_name + "/", false, _str( "{}", id ), paths );
        if( paths.empty() && !upsert )
            return false;

        for( auto& kv : data )
        {
            void* f = FileOpen( _str( "{}/{}/{}.{}", dir, collection_name, kv.first, id ), true );
            RUNTIME_ASSERT( f );

            if( !kv.second.empty() )
            {
                bool file_write = FileWrite( f, &kv.second[ 0 ], (uint) kv.second.length() );
                RUNTIME_ASSERT( file_write );
            }

            FileClose( f );
        }

        return true;
    }

    virtual bool Delete( const string& collection_name, uint id ) override
    {
        StrVec paths;
        FileManager::GetFolderFileNames( dir + "/" + collection_name + "/", false, _str( "{}", id ), paths );
        if( paths.empty() )
            return false;

        for( const string& path : paths )
        {
            bool file_delete = FileDelete( path );
            RUNTIME_ASSERT( file_delete );
        }

        return true;
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
            WriteLog( "Mongo: failed to parse URI: {}, error: {}.\n", uri, error.message );
            return nullptr;
        }

        mongoc_client_t* client = mongoc_client_new_from_uri( mongo_uri );
        if( !client )
        {
            WriteLog( "Mongo: can't create client.\n" );
            return nullptr;
        }

        mongoc_uri_destroy( mongo_uri );
        mongoc_client_set_appname( client, "fonline" );

        mongoc_database_t* database = mongoc_client_get_database( client, db_name.c_str() );
        if( !database )
        {
            WriteLog( "Mongo: can't get database '{}'.\n", db_name );
            return nullptr;
        }

        // Ping
        bson_t* ping = BCON_NEW( "ping", BCON_INT32( 1 ) );
        bson_t  reply;
        if( !mongoc_client_command_simple( client, "admin", ping, nullptr, &reply, &error ) )
        {
            WriteLog( "Mongo: can't ping database, error: {}.\n", error.message );
            return nullptr;
        }

        DbMongo* dbMongo = new DbMongo();
        dbMongo->client = client;
        dbMongo->database = database;
        dbMongo->databaseName = db_name;
        return dbMongo;
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
        if( !collection )
            return UIntVec();

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

            if( bson_iter_type( &iter ) != BSON_TYPE_INT32 )
            {
                WriteLog( "Mongo: non int32 '_id' in collection '{}' during get all ids.\n", collection_name );
                mongoc_cursor_destroy( cursor );
                bson_destroy( &query );
                bson_destroy( &fields );
                return UIntVec();
            }

            uint id = bson_iter_int32( &iter );
            ids.push_back( id );
        }

        bson_error_t error;
        if( mongoc_cursor_error( cursor, &error ) )
        {
            WriteLog( "Mongo: an error occurred during get all ids: {}.\n", error.message );
            mongoc_cursor_destroy( cursor );
            bson_destroy( &query );
            bson_destroy( &fields );
            return UIntVec();
        }

        mongoc_cursor_destroy( cursor );
        bson_destroy( &query );
        bson_destroy( &fields );
        return ids;
    }

    virtual StrMap Get( const string& collection_name, uint id ) override
    {
        mongoc_collection_t* collection = GetCollection( collection_name );
        if( !collection )
            return StrMap();

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
                if( bson_iter_type( &iter ) != BSON_TYPE_INT32 )
                {
                    WriteLog( "Mongo: non int32 '_id' in collection '{}' during get document.\n", collection_name );
                    mongoc_cursor_destroy( cursor );
                    bson_destroy( &query );
                    return StrMap();
                }

                uint document_id = bson_iter_int32( &iter );
                RUNTIME_ASSERT( id == document_id );
            }
            else
            {
                if( bson_iter_type( &iter ) != BSON_TYPE_UTF8 )
                {
                    WriteLog( "Mongo: non string value in collection '{}' during get document.\n", collection_name );
                    mongoc_cursor_destroy( cursor );
                    bson_destroy( &query );
                    return StrMap();
                }

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

    virtual bool Insert( const string& collection_name, uint id, const StrMap& data ) override
    {
        mongoc_collection_t* collection = GetCollection( collection_name );
        if( !collection )
            return false;

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
        if( !mongoc_collection_insert( collection, MONGOC_INSERT_NONE, &insert, nullptr, &error ) )
        {
            WriteLog( "Mongo: can't insert document in '{}', error: {}.\n", collection_name, error.message );
            bson_destroy( &insert );
            return false;
        }

        bson_destroy( &insert );
        return true;
    }

    virtual bool Update( const string& collection_name, uint id, const StrMap& data, bool upsert /* = false */ ) override
    {
        mongoc_collection_t* collection = GetCollection( collection_name );
        if( !collection )
            return false;

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
        if( !mongoc_collection_update( collection, upsert ? MONGOC_UPDATE_UPSERT : MONGOC_UPDATE_NONE, &selector, &update, nullptr, &error ) )
        {
            WriteLog( "Mongo: can't update document in '{}'.\n", collection_name );
            bson_destroy( &selector );
            bson_destroy( &update );
            bson_destroy( &update_set );
            return false;
        }

        bson_destroy( &selector );
        bson_destroy( &update );
        bson_destroy( &update_set );
        return true;
    }

    virtual bool Delete( const string& collection_name, uint id ) override
    {
        mongoc_collection_t* collection = GetCollection( collection_name );
        if( !collection )
            return false;

        bson_t selector;
        bson_init( &selector );

        bool append_int32 = bson_append_int32( &selector, "_id", 3, id );
        RUNTIME_ASSERT( append_int32 );

        bson_error_t error;
        if( !mongoc_collection_remove( collection, MONGOC_REMOVE_SINGLE_REMOVE, &selector, nullptr, &error ) )
        {
            WriteLog( "Mongo: can't delete document in '{}'.\n", collection_name );
            bson_destroy( &selector );
            return false;
        }

        bson_destroy( &selector );
        return true;
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
                WriteLog( "Mongo: can't get collection '{}'.\n", collection_name );
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
    auto entries = _str( connection_info ).split( ' ' );
    if( entries[ 0 ] == "Disk" )
    {
        if( entries.size() != 2 )
        {
            WriteLog( "Wrong disk settings '{}'.\n", connection_info );
            return nullptr;
        }

        return DbDisk::Create( entries[ 1 ] );
    }
    else if( entries[ 0 ] == "Mongo" )
    {
        if( entries.size() != 3 )
        {
            WriteLog( "Wrong mongo settings '{}'.\n", connection_info );
            return nullptr;
        }

        return DbMongo::Create( entries[ 1 ], entries[ 2 ] );
    }

    WriteLog( "Wrong data base connection info '{}'.\n", connection_info );
    return nullptr;
}
