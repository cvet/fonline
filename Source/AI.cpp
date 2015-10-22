#include "Common.h"
#include "AI.h"
#include "Text.h"
#include "FileManager.h"
#include "Item.h"
#include "IniParser.h"

NpcAIMngr AIMngr;

/************************************************************************/
/* Parsers                                                              */
/************************************************************************/

string ParseBagComb( const char* str )
{
    while( *str == ' ' )
        str++;
    return string( str );
}

NpcBagItems ParseBagItems( const char* str )
{
    NpcBagItems items;

label_ParseNext:
    NpcBagItem i;
    while( *str == ' ' )
        str++;
    char  buf[ MAX_FOTEXT ];
    char* pbuf = buf;
    // Parse pid
    for( ; *str != ':' && *str != '+' && *str != '^' && *str; str++, pbuf++ )
        *pbuf = *str;
    *pbuf = 0;
    const char* proto_name = ConvertProtoIdByStr( buf );
    hash        pid = ( proto_name ? Str::GetHash( proto_name ) : 0 );
    if( !pid )
        return items;
    i.ItemPid = pid;
    // Parse place
    if( *str == '^' )
    {
        str++;
        if( *str == 'm' )
            i.ItemSlot = SLOT_HAND1;
        else if( *str == 'e' )
            i.ItemSlot = SLOT_HAND2;
        else if( *str == 'a' )
            i.ItemSlot = SLOT_ARMOR;
        if( *str )
            str++;
    }
    // Parse min count
    if( *str == ':' )
    {
        str++;
        pbuf = buf;
        for( ; *str != '-' && *str != '+' && *str; str++, pbuf++ )
            *pbuf = *str;
        *pbuf = 0;
        i.MinCnt = Str::AtoI( buf );
    }
    else
    {
        i.MinCnt = 1;
    }
    // Parse max count
    if( *str == '-' )
    {
        str++;
        pbuf = buf;
        for( ; *str != '+' && *str; str++, pbuf++ )
            *pbuf = *str;
        *pbuf = 0;
        i.MaxCnt = Str::AtoI( buf );
    }
    else
    {
        i.MaxCnt = i.MinCnt;
    }
    // Parse alias
    if( *str == '+' )
    {
        items.push_back( i );
        str++;
        goto label_ParseNext;
    }

    items.push_back( i );
    return items;
}

/************************************************************************/
/* Init/Finish                                                          */
/************************************************************************/

bool NpcAIMngr::Init()
{
    if( !LoadNpcBags() )
        return false;
    return true;
}

void NpcAIMngr::Finish()
{
    npcBags.clear();
}

/************************************************************************/
/* Get                                                                  */
/************************************************************************/

NpcBag& NpcAIMngr::GetBag( uint num )
{
    return num < npcBags.size() ? npcBags[ num ] : npcBags[ 0 ];
}

/************************************************************************/
/* Load                                                                 */
/************************************************************************/

bool NpcAIMngr::LoadNpcBags()
{
    WriteLog( "Load NPC bags...\n" );

    IniParser bags_txt;
    if( !bags_txt.AppendFile( BAGS_FILE_NAME, PT_SERVER_CONFIGS ) )
    {
        WriteLog( "File '%s' not found.\n", FileManager::GetDataPath( BAGS_FILE_NAME, PT_SERVER_CONFIGS ) );
        npcBags.resize( 1 );
        return true;
    }

    int                 end_bag = bags_txt.GetInt( "", "end_bag", -1 );
    int                 bag_count = 0;
    StringNpcBagCombMap loaded_comb;

    npcBags.resize( end_bag + 1 );

    for( int i = 0; i <= end_bag; i++ )
    {
        const char* bag_str = bags_txt.GetStr( "", Str::FormatBuf( "bag_%d", i ), "" );
        NpcBag&     cur_bag = npcBags[ i ];

        StrVec      comb;
        Str::ParseLine< StrVec, string ( * )( const char* ) >( bag_str, ' ', comb, ParseBagComb );

        for( uint j = 0; j < comb.size(); j++ )
        {
            string& c = comb[ j ];
            auto    it = loaded_comb.find( c );
            if( it == loaded_comb.end() )
            {
                // Get combination line
                if( !( bag_str = bags_txt.GetStr( "", c.c_str(), "" ) ) )
                {
                    WriteLog( "Items combination '%s' not found.\n", c.c_str() );
                    return false;
                }

                // Parse
                NpcBagCombination items_comb;
                Str::ParseLine< NpcBagCombination, NpcBagItems ( * )( const char* ) >( bag_str, ' ', items_comb, ParseBagItems );

                // Check
                for( uint l = 0; l < items_comb.size(); l++ )
                {
                    NpcBagItems& items = items_comb[ l ];
                    for( uint k = 0; k < items.size(); k++ )
                    {
                        NpcBagItem& b = items[ k ];
                        if( b.MinCnt > b.MaxCnt )
                        {
                            WriteLog( "Invalid items combination '%s', item combination %d, number %d.\n", c.c_str(), l, k );
                            return false;
                        }
                    }
                }

                loaded_comb.insert( PAIR( c, items_comb ) );
                it = loaded_comb.find( c );
            }

            cur_bag.push_back( it->second );
        }

        bag_count++;
    }

    WriteLog( "Load NPC bags complete, count %d.\n", bag_count );
    return true;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
