#include "Common.h"
#include "ConstantsManager.h"
#include "Log.h"
#include "Text.h"
#include "FileManager.h"

const char* CollectionFiles[] =
{
    "ItemNames.lst",
    "DefineNames.lst",
    "HashNames.lst",
};

struct ConstCollection
{
    bool       Init;
    StrVec     Names;
    UIntStrMap ValueName;
    StrUIntMap NameValue;
    ConstCollection(): Init( false ) {}
};
vector< ConstCollection > ConstCollections;

void ConstantsManager::Initialize( int path_type, const char* path /* = NULL */ )
{
    ConstCollections.resize( CONSTANTS_HASH + 1 );
    for( int i = 0; i <= CONSTANTS_HASH; i++ )
    {
        if( path )
            AddCollection( i, Str::FormatBuf( "%s%s", path, CollectionFiles[ i ] ), path_type );
        else
            AddCollection( i, CollectionFiles[ i ], path_type );
    }
}

bool ConstantsManager::AddCollection( int collection, const char* fname, int path_type )
{
    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return false;

    if( collection >= (int) ConstCollections.size() )
        ConstCollections.resize( collection + 1 );

    StrVec&     names = ConstCollections[ collection ].Names;
    UIntStrMap& value_name = ConstCollections[ collection ].ValueName;
    StrUIntMap& name_value = ConstCollections[ collection ].NameValue;

    ConstCollections[ collection ].Init = true;
    names.clear();
    value_name.clear();
    name_value.clear();

    bool   revert = false;
    char   line[ MAX_FOTEXT ];
    string name;
    int    offset = 0;
    int    num;
    while( fm.GetLine( line, MAX_FOTEXT ) )
    {
        if( line[ 0 ] == '*' )
        {
            if( line[ 1 ] == '*' )
                revert = !revert;
            else
                offset = atoi( &line[ 1 ] );
        }
        else
        {
            istrstream str( line );

            if( !revert )
            {
                if( ( str >> num ).fail() )
                    continue;
                if( ( str >> name ).fail() )
                    continue;
            }
            else
            {
                if( ( str >> name ).fail() )
                    continue;
                if( ( str >> num ).fail() )
                    continue;
            }

            names.push_back( name );
            value_name.insert( PAIR( num + offset, name ) );
            name_value.insert( PAIR( name, num + offset ) );
            Str::GetHash( name.c_str() );
        }
    }

    return true;
}

void ConstantsManager::AddConstant( int collection, const char* str, int value )
{
    ConstCollections[ collection ].ValueName.insert( PAIR( value, str ) );
    ConstCollections[ collection ].NameValue.insert( PAIR( str, value ) );
    Str::GetHash( str );
}

StrVec ConstantsManager::GetCollection( int collection )
{
    return ConstCollections[ collection ].Names;
}

bool ConstantsManager::IsCollectionInit( int collection )
{
    return collection >= 0 && collection < (int) ConstCollections.size() && ConstCollections[ collection ].Init;
}

int ConstantsManager::GetValue( int collection, const char* str )
{
    auto it = ConstCollections[ collection ].NameValue.find( str );
    if( it == ConstCollections[ collection ].NameValue.end() )
        return -1;
    return ( *it ).second;
}

const char* ConstantsManager::GetName( int collection, int value )
{
    auto it = ConstCollections[ collection ].ValueName.find( value );
    if( it == ConstCollections[ collection ].ValueName.end() )
        return NULL;
    return ( *it ).second.c_str();
}

uint ConstantsManager::GetItemPid( const char* str )
{
    int value = GetValue( CONSTANTS_ITEM, str );
    return (uint) ( value < 0 ? 0 : value );
}

const char* ConstantsManager::GetItemName( ushort pid )
{
    return GetName( CONSTANTS_ITEM, pid );
}

int ConstantsManager::GetDefineValue( const char* str )
{
    if( Str::IsNumber( str ) )
        return Str::AtoI( str );

    if( Str::CompareCase( str, "true" ) )
        return 1;
    else if( Str::CompareCase( str, "false" ) )
        return 0;

    auto it = ConstCollections[ CONSTANTS_DEFINE ].NameValue.find( str );
    if( it == ConstCollections[ CONSTANTS_DEFINE ].NameValue.end() )
    {
        WriteLogF( _FUNC_, " - Define<%s> not found, taked zero by default.\n", str );
        return 0;
    }
    return ( *it ).second;
}
