#ifndef __CONSTANTS_MANAGER__
#define __CONSTANTS_MANAGER__

#define CONSTANTS_DEFINE    ( 0 )
#define CONSTANTS_HASH      ( 1 )

namespace ConstantsManager
{
    void   Initialize( int path_type, const char* path = NULL );
    bool   AddCollection( int collection, const char* fname, int path_type );
    void   AddConstant( int collection, const char* str, int value );
    StrVec GetCollection( int collection );
    bool   IsCollectionInit( int collection );

    int         GetValue( int collection, const char* str );
    const char* GetName( int collection, int value );
    int         GetDefineValue( const char* str );
};

#endif // __CONSTANTS_MANAGER__
