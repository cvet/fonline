#ifndef _DATA_BASE_
#define _DATA_BASE_

#include "Common.h"

class DBCollection
{
public:
    virtual ~DBCollection() = default;
    virtual StrMap Get( int64 id ) = 0;
    virtual void   Insert( int64 id, const StrMap& insert ) = 0;
    virtual void   Update( int64 id, const StrMap* update, const StrSet* remove ) = 0;
    virtual void   Delete( int64 id ) = 0;
};

// DBCollection* GetDBCollection( const string& name );

#endif // _DATA_BASE_
