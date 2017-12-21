#include "DataBase.h"
#include "IniParser.h"

class DbDisk: public DataBase
{
    string dir;

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
        ini.SetApp( "" ) = data;
        return ini.SaveFile( _str( "{}/{}.txt", dir, id ) );
    }

    virtual bool Delete( uint id ) override
    {
        return FileManager::DeleteFile( _str( "{}/{}.txt", dir, id ) );
    }
};

DataBase* GetDataBase( const string& collection_name, const string& connection_info )
{
    return nullptr;
}
