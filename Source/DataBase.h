#ifndef _DATA_BASE_
#define _DATA_BASE_

#include "Common.h"

class DataBase
{
public:
    virtual ~DataBase() = default;
    virtual UIntVec GetAllIds() = 0;
    virtual StrMap  Get( uint id ) = 0;
    virtual bool    Insert( uint id, const StrMap& data ) = 0;
    virtual bool    Update( uint id, const StrMap& data, const StrSet* remove_fields ) = 0;
    virtual bool    Delete( uint id ) = 0;
};

DataBase* GetDataBase( const string& collection_name, const string& connection_info );

#endif // _DATA_BASE_
