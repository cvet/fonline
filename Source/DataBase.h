#ifndef _DATA_BASE_
#define _DATA_BASE_

#include "Common.h"

class DataBase
{
public:
    virtual ~DataBase() = default;
    virtual UIntVec GetAllIds( const string& collection_name ) = 0;
    virtual StrMap  Get( const string& collection_name, uint id ) = 0;
    virtual bool    Insert( const string& collection_name, uint id, const StrMap& data ) = 0;
    virtual bool    Update( const string& collection_name, uint id, const StrMap& data ) = 0;
    virtual bool    Delete( const string& collection_name, uint id ) = 0;
};

extern DataBase* DbStorage;

DataBase* GetDataBase( const string& connection_info );

#endif // _DATA_BASE_
