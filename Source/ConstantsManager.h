#ifndef __CONSTANTS_MANAGER__
#define __CONSTANTS_MANAGER__

#define CONSTANTS_PARAM      ( 0 )
#define CONSTANTS_ITEM       ( 1 )
#define CONSTANTS_DEFINE     ( 2 )
#define CONSTANTS_PICTURE    ( 3 )
#define CONSTANTS_HASH       ( 4 )

namespace ConstantsManager
{
    void   Initialize( int path_type, const char* path = NULL );
    bool   AddCollection( int collection, const char* fname, int path_type );
    void   AddConstant( int collection, const char* str, int value );
    StrVec GetCollection( int collection );
    bool   IsCollectionInit( int collection );

    int         GetValue( int collection, const char* str );
    const char* GetName( int collection, int value );
    int         GetParamId( const char* str );
    const char* GetParamName( uint index );
    uint        GetItemPid( const char* str );
    const char* GetItemName( ushort pid );
    int         GetDefineValue( const char* str );
    const char* GetPictureName( uint index );
};

#endif // __CONSTANTS_MANAGER__
