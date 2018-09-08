#include "DataBase.h"
#include "IniParser.h"
#include <mongoc.h>

class DbDisk: public DataBase
{
    string dir;

public:
    DbDisk( const string& collection_name, const string& connection_info )
    {
        dir = _str( connection_info ).split( ' ' )[ 1 ] + "/" + collection_name;
    }

    virtual UIntVec GetAllIds() override
    {
        UIntVec ids;
        StrVec  paths;
        FileManager::GetFolderFileNames( dir, false, "txt", paths );
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

    virtual StrMap Get( uint id ) override
    {
        IniParser ini;
        ini.AppendFile( _str( "{}/{}.txt", dir, id ) );
        if( ini.IsLoaded() )
            return ini.GetApp( "" );
        return StrMap();
    }

    virtual bool Insert( uint id, const StrMap& data ) override
    {
        IniParser ini;
        ini.SetApp( "" ) = data;
        return ini.SaveFile( _str( "{}/{}.txt", dir, id ) );
    }

    virtual bool Update( uint id, const StrMap& data, const StrSet* remove_fields ) override
    {
        StrMap actual_data = Get( id );
        for( const auto& kv : data )
            actual_data[ kv.first ] = kv.second;

        if( remove_fields )
            for( const string& f :* remove_fields )
                actual_data.erase( f );

        IniParser ini;
        ini.SetApp( "" ) = actual_data;
        return ini.SaveFile( _str( "{}/{}.txt", dir, id ) );
    }

    virtual bool Delete( uint id ) override
    {
        return FileManager::DeleteFile( _str( "{}/{}.txt", dir, id ) );
    }
};

class DbMongo: public DataBase
{
    string               dir;

    const char*          uri_str = "mongodb://localhost:27017";
    mongoc_client_t*     client;
    mongoc_database_t*   database;
    mongoc_collection_t* collection;
    bson_t*              command, reply, * insert;
    bson_error_t         error;
    char*                str;
    bool                 retval;

    bool Init( const string& connection_info )
    {
        mongoc_init();
        client = mongoc_client_new( uri_str );
        mongoc_client_set_appname( client, "connect-example" );
        database = mongoc_client_get_database( client, "db_name" );
        collection = mongoc_client_get_collection( client, "db_name", "coll_name" );

        command = BCON_NEW( "ping", BCON_INT32( 1 ) );

        retval = mongoc_client_command_simple(
            client, "admin", command, NULL, &reply, &error );

        if( !retval )
        {
            fprintf( stderr, "%s\n", error.message );
            return EXIT_FAILURE;
        }

        str = bson_as_json( &reply, NULL );
        printf( "%s\n", str );

        insert = BCON_NEW( "hello", BCON_UTF8( "world" ) );

        // if (!mongoc_collection_insert_one(collection, insert, NULL, NULL, &error)) {
        //	fprintf(stderr, "%s\n", error.message);
        // }
    }

    virtual UIntVec GetAllIds() override
    {
        UIntVec ids;
        return ids;
    }

    virtual StrMap Get( uint id ) override
    {
        return StrMap();
    }

    virtual bool Insert( uint id, const StrMap& data ) override
    {
        return false;
    }

    virtual bool Update( uint id, const StrMap& data, const StrSet* remove_fields ) override
    {
        return false;
    }

    virtual bool Delete( uint id ) override
    {
        return false;
    }
};

DataBase* GetDataBase( const string& collection_name, const string& connection_info )
{
    return new DbDisk( collection_name, connection_info );
}
