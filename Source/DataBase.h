#ifndef _DATA_BASE_
#define _DATA_BASE_

#include "Common.h"

class DBCollection
{
public:
    virtual ~DBCollection() = default;
    virtual bool    Exists( uint id ) = 0;
    virtual StrMap  Get( uint id ) = 0;
    virtual StrMap  Get( uint id, const StrSet& specific_fields ) = 0;
    virtual UIntSet GetAllIds() = 0;
    virtual bool    Insert( uint id, const StrMap& insert ) = 0;
    virtual bool    Update( uint id, const StrMap* update, const StrSet* remove ) = 0;
    virtual bool    Delete( uint id ) = 0;
};

// DBCollection* GetDBCollection( const string& name );

#endif // _DATA_BASE_
