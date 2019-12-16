#include "NetBuffer.h"
#include "Debugger.h"

NetBuffer::NetBuffer()
{
    MEMORY_PROCESS( MEMORY_NET_BUFFER, DefaultBufSize + sizeof( NetBuffer ) );
    isError = false;
    bufLen = DefaultBufSize;
    bufEndPos = 0;
    bufReadPos = 0;
    bufData = new uchar[ bufLen ];
    memzero( bufData, bufLen );
    encryptActive = false;
    encryptKeyPos = 0;
    memzero( encryptKeys, sizeof( encryptKeys ) );
}

NetBuffer::~NetBuffer()
{
    MEMORY_PROCESS( MEMORY_NET_BUFFER, -(int) ( bufLen + sizeof( NetBuffer ) ) );
    SAFEDELA( bufData );
}

void NetBuffer::SetEncryptKey( uint seed )
{
    if( !seed )
    {
        encryptActive = false;
        return;
    }

    std::mt19937 rnd_generator {
        seed
    };
    std::uniform_int_distribution rnd_distr {
        1, 255
    };
    for( int i = 0; i < CryptKeysCount; i++ )
        encryptKeys[ i ] = static_cast< uchar >( rnd_distr( rnd_generator ) );
    encryptKeyPos = 0;
    encryptActive = true;
}

uchar NetBuffer::EncryptKey( int move )
{
    uchar key = 0;
    if( encryptActive )
    {
        key = encryptKeys[ encryptKeyPos ];
        encryptKeyPos += move;
        if( encryptKeyPos < 0 || encryptKeyPos >= CryptKeysCount )
        {
            encryptKeyPos %= CryptKeysCount;
            if( encryptKeyPos < 0 )
                encryptKeyPos += CryptKeysCount;
        }
    }
    return key;
}

void NetBuffer::Lock()
{
    bufLocker.Lock();
}

void NetBuffer::Unlock()
{
    bufLocker.Unlock();
}

void NetBuffer::Refresh()
{
    if( isError )
        return;
    if( bufReadPos > bufEndPos )
    {
        isError = true;
        return;
    }
    if( bufReadPos )
    {
        for( uint i = bufReadPos; i < bufEndPos; i++ )
            bufData[ i - bufReadPos ] = bufData[ i ];
        bufEndPos -= bufReadPos;
        bufReadPos = 0;
    }
}

void NetBuffer::Reset()
{
    bufEndPos = 0;
    bufReadPos = 0;
    if( bufLen > DefaultBufSize )
    {
        MEMORY_PROCESS( MEMORY_NET_BUFFER, -(int) bufLen );
        MEMORY_PROCESS( MEMORY_NET_BUFFER, DefaultBufSize );
        bufLen = DefaultBufSize;
        SAFEDELA( bufData );
        bufData = new uchar[ bufLen ];
        memzero( bufData, bufLen );
    }
}

void NetBuffer::LockReset()
{
    Lock();
    Reset();
    Unlock();
}

void NetBuffer::GrowBuf( uint len )
{
    if( bufEndPos + len < bufLen )
        return;
    MEMORY_PROCESS( MEMORY_NET_BUFFER, -(int) bufLen );
    while( bufEndPos + len >= bufLen )
        bufLen <<= 1;
    MEMORY_PROCESS( MEMORY_NET_BUFFER, bufLen );
    uchar* new_buf = new uchar[ bufLen ];
    memzero( new_buf, bufLen );
    memcpy( new_buf, bufData, bufEndPos );
    SAFEDELA( bufData );
    bufData = new_buf;
}

void NetBuffer::MoveReadPos( int val )
{
    bufReadPos += val;
    EncryptKey( val );
}

void NetBuffer::Push( const void* buf, uint len, bool no_crypt /* = false */ )
{
    if( isError || !len )
        return;
    if( bufEndPos + len >= bufLen )
        GrowBuf( len );
    CopyBuf( buf, bufData + bufEndPos, no_crypt ? 0 : EncryptKey( len ), len );
    bufEndPos += len;
}

void NetBuffer::Pop( void* buf, uint len )
{
    if( isError || !len )
        return;
    if( bufReadPos + len > bufEndPos )
    {
        isError = true;
        return;
    }
    CopyBuf( bufData + bufReadPos, buf, EncryptKey( len ), len );
    bufReadPos += len;
}

void NetBuffer::Cut( uint len )
{
    if( isError || !len )
        return;
    if( bufReadPos + len > bufEndPos )
    {
        isError = true;
        return;
    }
    uchar* buf = bufData + bufReadPos;
    for( uint i = 0; i + bufReadPos + len < bufEndPos; i++ )
        buf[ i ] = buf[ i + len ];
    bufEndPos -= len;
}

void NetBuffer::CopyBuf( const void* from, void* to, uchar crypt_key, uint len )
{
    const uchar* from_ = (const uchar*) from;
    uchar*       to_ = (uchar*) to;
    for( uint i = 0; i < len; i++, to_++, from_++ )
        *to_ = *from_ ^ crypt_key;
}

bool NetBuffer::NeedProcess()
{
    uint msg = 0;
    if( bufReadPos + sizeof( msg ) > bufEndPos )
        return false;

    CopyBuf( bufData + bufReadPos, &msg, EncryptKey( 0 ), sizeof( msg ) );

    // Known size
    switch( msg )
    {
    case 0xFFFFFFFF:
        return true;                                                                 // Ping
    case NETMSG_DISCONNECT:
        return ( NETMSG_DISCONNECT_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_LOGIN:
        return ( NETMSG_LOGIN_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_WRONG_NET_PROTO:
        return ( NETMSG_WRONG_NET_PROTO_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_REGISTER_SUCCESS:
        return ( NETMSG_REGISTER_SUCCESS_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_PING:
        return ( NETMSG_PING_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_END_PARSE_TO_GAME:
        return ( NETMSG_END_PARSE_TO_GAME_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_UPDATE:
        return ( NETMSG_UPDATE_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_GET_UPDATE_FILE:
        return ( NETMSG_GET_UPDATE_FILE_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_GET_UPDATE_FILE_DATA:
        return ( NETMSG_GET_UPDATE_FILE_DATA_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_UPDATE_FILE_DATA:
        return ( NETMSG_UPDATE_FILE_DATA_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_REMOVE_CRITTER:
        return ( NETMSG_REMOVE_CRITTER_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_MSG:
        return ( NETMSG_MSG_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_MAP_TEXT_MSG:
        return ( NETMSG_MAP_TEXT_MSG_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_DIR:
        return ( NETMSG_DIR_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_CRITTER_DIR:
        return ( NETMSG_CRITTER_DIR_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_MOVE_WALK:
        return ( NETMSG_SEND_MOVE_WALK_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_MOVE_RUN:
        return ( NETMSG_SEND_MOVE_RUN_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_CRITTER_MOVE:
        return ( NETMSG_CRITTER_MOVE_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_CRITTER_XY:
        return ( NETMSG_CRITTER_XY_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_CUSTOM_COMMAND:
        return ( NETMSG_CUSTOM_COMMAND_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_CLEAR_ITEMS:
        return ( NETMSG_CLEAR_ITEMS_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_REMOVE_ITEM:
        return ( NETMSG_REMOVE_ITEM_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_ALL_ITEMS_SEND:
        return ( NETMSG_ALL_ITEMS_SEND_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_ERASE_ITEM_FROM_MAP:
        return ( NETMSG_ERASE_ITEM_FROM_MAP_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_ANIMATE_ITEM:
        return ( NETMSG_ANIMATE_ITEM_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_CRITTER_ACTION:
        return ( NETMSG_CRITTER_ACTION_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_CRITTER_ANIMATE:
        return ( NETMSG_CRITTER_ANIMATE_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_CRITTER_SET_ANIMS:
        return ( NETMSG_CRITTER_SET_ANIMS_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_EFFECT:
        return ( NETMSG_EFFECT_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_FLY_EFFECT:
        return ( NETMSG_FLY_EFFECT_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_TALK_NPC:
        return ( NETMSG_SEND_TALK_NPC_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_GET_INFO:
        return ( NETMSG_SEND_GET_TIME_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_GAME_INFO:
        return ( NETMSG_GAME_INFO_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_GIVE_MAP:
        return ( NETMSG_SEND_GIVE_MAP_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_LOAD_MAP_OK:
        return ( NETMSG_SEND_LOAD_MAP_OK_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_REFRESH_ME:
        return ( NETMSG_SEND_REFRESH_ME_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_VIEW_MAP:
        return ( NETMSG_VIEW_MAP_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_POD_PROPERTY( 1, 0 ):
        return ( NETMSG_SEND_POD_PROPERTY_SIZE( 1, 0  ) + bufReadPos <= bufEndPos );
    case NETMSG_SEND_POD_PROPERTY( 2, 0  ):
        return ( NETMSG_SEND_POD_PROPERTY_SIZE( 2, 0  ) + bufReadPos <= bufEndPos );
    case NETMSG_SEND_POD_PROPERTY( 4, 0  ):
        return ( NETMSG_SEND_POD_PROPERTY_SIZE( 4, 0  ) + bufReadPos <= bufEndPos );
    case NETMSG_SEND_POD_PROPERTY( 8, 0  ):
        return ( NETMSG_SEND_POD_PROPERTY_SIZE( 8, 0  ) + bufReadPos <= bufEndPos );
    case NETMSG_SEND_POD_PROPERTY( 1, 1 ):
        return ( NETMSG_SEND_POD_PROPERTY_SIZE( 1, 1  ) + bufReadPos <= bufEndPos );
    case NETMSG_SEND_POD_PROPERTY( 2, 1  ):
        return ( NETMSG_SEND_POD_PROPERTY_SIZE( 2, 1  ) + bufReadPos <= bufEndPos );
    case NETMSG_SEND_POD_PROPERTY( 4, 1  ):
        return ( NETMSG_SEND_POD_PROPERTY_SIZE( 4, 1  ) + bufReadPos <= bufEndPos );
    case NETMSG_SEND_POD_PROPERTY( 8, 1  ):
        return ( NETMSG_SEND_POD_PROPERTY_SIZE( 8, 1  ) + bufReadPos <= bufEndPos );
    case NETMSG_SEND_POD_PROPERTY( 1, 2 ):
        return ( NETMSG_SEND_POD_PROPERTY_SIZE( 1, 2  ) + bufReadPos <= bufEndPos );
    case NETMSG_SEND_POD_PROPERTY( 2, 2  ):
        return ( NETMSG_SEND_POD_PROPERTY_SIZE( 2, 2  ) + bufReadPos <= bufEndPos );
    case NETMSG_SEND_POD_PROPERTY( 4, 2  ):
        return ( NETMSG_SEND_POD_PROPERTY_SIZE( 4, 2  ) + bufReadPos <= bufEndPos );
    case NETMSG_SEND_POD_PROPERTY( 8, 2  ):
        return ( NETMSG_SEND_POD_PROPERTY_SIZE( 8, 2  ) + bufReadPos <= bufEndPos );
    case NETMSG_POD_PROPERTY( 1, 0 ):
        return ( NETMSG_POD_PROPERTY_SIZE( 1, 0  ) + bufReadPos <= bufEndPos );
    case NETMSG_POD_PROPERTY( 2, 0  ):
        return ( NETMSG_POD_PROPERTY_SIZE( 2, 0  ) + bufReadPos <= bufEndPos );
    case NETMSG_POD_PROPERTY( 4, 0  ):
        return ( NETMSG_POD_PROPERTY_SIZE( 4, 0  ) + bufReadPos <= bufEndPos );
    case NETMSG_POD_PROPERTY( 8, 0  ):
        return ( NETMSG_POD_PROPERTY_SIZE( 8, 0  ) + bufReadPos <= bufEndPos );
    case NETMSG_POD_PROPERTY( 1, 1 ):
        return ( NETMSG_POD_PROPERTY_SIZE( 1, 1  ) + bufReadPos <= bufEndPos );
    case NETMSG_POD_PROPERTY( 2, 1  ):
        return ( NETMSG_POD_PROPERTY_SIZE( 2, 1  ) + bufReadPos <= bufEndPos );
    case NETMSG_POD_PROPERTY( 4, 1  ):
        return ( NETMSG_POD_PROPERTY_SIZE( 4, 1  ) + bufReadPos <= bufEndPos );
    case NETMSG_POD_PROPERTY( 8, 1  ):
        return ( NETMSG_POD_PROPERTY_SIZE( 8, 1  ) + bufReadPos <= bufEndPos );
    case NETMSG_POD_PROPERTY( 1, 2 ):
        return ( NETMSG_POD_PROPERTY_SIZE( 1, 2  ) + bufReadPos <= bufEndPos );
    case NETMSG_POD_PROPERTY( 2, 2  ):
        return ( NETMSG_POD_PROPERTY_SIZE( 2, 2  ) + bufReadPos <= bufEndPos );
    case NETMSG_POD_PROPERTY( 4, 2  ):
        return ( NETMSG_POD_PROPERTY_SIZE( 4, 2  ) + bufReadPos <= bufEndPos );
    case NETMSG_POD_PROPERTY( 8, 2  ):
        return ( NETMSG_POD_PROPERTY_SIZE( 8, 2  ) + bufReadPos <= bufEndPos );
    default:
        break;
    }

    // Changeable size
    uint msg_len = 0;
    if( bufReadPos + sizeof( msg ) + sizeof( msg_len ) > bufEndPos )
        return false;

    EncryptKey( sizeof( msg ) );
    CopyBuf( bufData + bufReadPos + sizeof( msg ), &msg_len, EncryptKey( -(int) sizeof( msg ) ), sizeof( msg_len ) );

    switch( msg )
    {
    case NETMSG_LOGIN_SUCCESS:
    case NETMSG_LOADMAP:
    case NETMSG_CREATE_CLIENT:
    case NETMSG_UPDATE_FILES_LIST:
    case NETMSG_ADD_PLAYER:
    case NETMSG_ADD_NPC:
    case NETMSG_SEND_COMMAND:
    case NETMSG_SEND_TEXT:
    case NETMSG_CRITTER_TEXT:
    case NETMSG_MSG_LEX:
    case NETMSG_MAP_TEXT:
    case NETMSG_MAP_TEXT_MSG_LEX:
    case NETMSG_ADD_ITEM:
    case NETMSG_ADD_ITEM_ON_MAP:
    case NETMSG_SOME_ITEM:
    case NETMSG_SOME_ITEMS:
    case NETMSG_CRITTER_MOVE_ITEM:
    case NETMSG_COMBAT_RESULTS:
    case NETMSG_PLAY_SOUND:
    case NETMSG_TALK_NPC:
    case NETMSG_MAP:
    case NETMSG_RPC:
    case NETMSG_GLOBAL_INFO:
    case NETMSG_AUTOMAPS_INFO:
    case NETMSG_COMPLEX_PROPERTY:
    case NETMSG_SEND_COMPLEX_PROPERTY:
    case NETMSG_ALL_PROPERTIES:
        return bufReadPos + msg_len <= bufEndPos;
    default:
        // Unknown message
        Reset();
        isError = true;
        return false;
    }

    return false;
}

void NetBuffer::SkipMsg( uint msg )
{
    bufReadPos -= sizeof( msg );
    EncryptKey( -(int) sizeof( msg ) );

    // Known size
    uint size = 0;
    switch( msg )
    {
    case 0xFFFFFFFF:
        size = 16;
        break;
    case NETMSG_DISCONNECT:
        size = NETMSG_DISCONNECT_SIZE;
        break;
    case NETMSG_LOGIN:
        size = NETMSG_LOGIN_SIZE;
        break;
    case NETMSG_WRONG_NET_PROTO:
        size = NETMSG_WRONG_NET_PROTO_SIZE;
        break;
    case NETMSG_REGISTER_SUCCESS:
        size = NETMSG_REGISTER_SUCCESS_SIZE;
        break;
    case NETMSG_PING:
        size = NETMSG_PING_SIZE;
        break;
    case NETMSG_END_PARSE_TO_GAME:
        size = NETMSG_END_PARSE_TO_GAME_SIZE;
        break;
    case NETMSG_UPDATE:
        size = NETMSG_UPDATE_SIZE;
        break;
    case NETMSG_GET_UPDATE_FILE:
        size = NETMSG_GET_UPDATE_FILE_SIZE;
        break;
    case NETMSG_GET_UPDATE_FILE_DATA:
        size = NETMSG_GET_UPDATE_FILE_DATA_SIZE;
        break;
    case NETMSG_UPDATE_FILE_DATA:
        size = NETMSG_UPDATE_FILE_DATA_SIZE;
        break;
    case NETMSG_REMOVE_CRITTER:
        size = NETMSG_REMOVE_CRITTER_SIZE;
        break;
    case NETMSG_MSG:
        size = NETMSG_MSG_SIZE;
        break;
    case NETMSG_MAP_TEXT_MSG:
        size = NETMSG_MAP_TEXT_MSG_SIZE;
        break;
    case NETMSG_DIR:
        size = NETMSG_DIR_SIZE;
        break;
    case NETMSG_CRITTER_DIR:
        size = NETMSG_CRITTER_DIR_SIZE;
        break;
    case NETMSG_SEND_MOVE_WALK:
        size = NETMSG_SEND_MOVE_WALK_SIZE;
        break;
    case NETMSG_SEND_MOVE_RUN:
        size = NETMSG_SEND_MOVE_RUN_SIZE;
        break;
    case NETMSG_CRITTER_MOVE:
        size = NETMSG_CRITTER_MOVE_SIZE;
        break;
    case NETMSG_CRITTER_XY:
        size = NETMSG_CRITTER_XY_SIZE;
        break;
    case NETMSG_CUSTOM_COMMAND:
        size = NETMSG_CUSTOM_COMMAND_SIZE;
        break;
    case NETMSG_CLEAR_ITEMS:
        size = NETMSG_CLEAR_ITEMS_SIZE;
        break;
    case NETMSG_REMOVE_ITEM:
        size = NETMSG_REMOVE_ITEM_SIZE;
        break;
    case NETMSG_ALL_ITEMS_SEND:
        size = NETMSG_ALL_ITEMS_SEND_SIZE;
        break;
    case NETMSG_ERASE_ITEM_FROM_MAP:
        size = NETMSG_ERASE_ITEM_FROM_MAP_SIZE;
        break;
    case NETMSG_ANIMATE_ITEM:
        size = NETMSG_ANIMATE_ITEM_SIZE;
        break;
    case NETMSG_CRITTER_ACTION:
        size = NETMSG_CRITTER_ACTION_SIZE;
        break;
    case NETMSG_CRITTER_ANIMATE:
        size = NETMSG_CRITTER_ANIMATE_SIZE;
        break;
    case NETMSG_CRITTER_SET_ANIMS:
        size = NETMSG_CRITTER_SET_ANIMS_SIZE;
        break;
    case NETMSG_EFFECT:
        size = NETMSG_EFFECT_SIZE;
        break;
    case NETMSG_FLY_EFFECT:
        size = NETMSG_FLY_EFFECT_SIZE;
        break;
    case NETMSG_SEND_TALK_NPC:
        size = NETMSG_SEND_TALK_NPC_SIZE;
        break;
    case NETMSG_SEND_GET_INFO:
        size = NETMSG_SEND_GET_TIME_SIZE;
        break;
    case NETMSG_GAME_INFO:
        size = NETMSG_GAME_INFO_SIZE;
        break;
    case NETMSG_SEND_GIVE_MAP:
        size = NETMSG_SEND_GIVE_MAP_SIZE;
        break;
    case NETMSG_SEND_LOAD_MAP_OK:
        size = NETMSG_SEND_LOAD_MAP_OK_SIZE;
        break;
    case NETMSG_SEND_REFRESH_ME:
        size = NETMSG_SEND_REFRESH_ME_SIZE;
        break;
    case NETMSG_VIEW_MAP:
        size = NETMSG_VIEW_MAP_SIZE;
        break;
    case NETMSG_SEND_POD_PROPERTY( 1, 0 ):
        size = NETMSG_SEND_POD_PROPERTY_SIZE( 1, 0 );
        break;
    case NETMSG_SEND_POD_PROPERTY( 2, 0 ):
        size = NETMSG_SEND_POD_PROPERTY_SIZE( 2, 0 );
        break;
    case NETMSG_SEND_POD_PROPERTY( 4, 0 ):
        size = NETMSG_SEND_POD_PROPERTY_SIZE( 4, 0 );
        break;
    case NETMSG_SEND_POD_PROPERTY( 8, 0 ):
        size = NETMSG_SEND_POD_PROPERTY_SIZE( 8, 0 );
        break;
    case NETMSG_SEND_POD_PROPERTY( 1, 1 ):
        size = NETMSG_SEND_POD_PROPERTY_SIZE( 1, 1 );
        break;
    case NETMSG_SEND_POD_PROPERTY( 2, 1 ):
        size = NETMSG_SEND_POD_PROPERTY_SIZE( 2, 1 );
        break;
    case NETMSG_SEND_POD_PROPERTY( 4, 1 ):
        size = NETMSG_SEND_POD_PROPERTY_SIZE( 4, 1 );
        break;
    case NETMSG_SEND_POD_PROPERTY( 8, 1 ):
        size = NETMSG_SEND_POD_PROPERTY_SIZE( 8, 1 );
        break;
    case NETMSG_SEND_POD_PROPERTY( 1, 2 ):
        size = NETMSG_SEND_POD_PROPERTY_SIZE( 1, 2 );
        break;
    case NETMSG_SEND_POD_PROPERTY( 2, 2 ):
        size = NETMSG_SEND_POD_PROPERTY_SIZE( 2, 2 );
        break;
    case NETMSG_SEND_POD_PROPERTY( 4, 2 ):
        size = NETMSG_SEND_POD_PROPERTY_SIZE( 4, 2 );
        break;
    case NETMSG_SEND_POD_PROPERTY( 8, 2 ):
        size = NETMSG_SEND_POD_PROPERTY_SIZE( 8, 2 );
        break;
    case NETMSG_POD_PROPERTY( 1, 0 ):
        size = NETMSG_POD_PROPERTY_SIZE( 1, 0 );
        break;
    case NETMSG_POD_PROPERTY( 2, 0 ):
        size = NETMSG_POD_PROPERTY_SIZE( 2, 0 );
        break;
    case NETMSG_POD_PROPERTY( 4, 0 ):
        size = NETMSG_POD_PROPERTY_SIZE( 4, 0 );
        break;
    case NETMSG_POD_PROPERTY( 8, 0 ):
        size = NETMSG_POD_PROPERTY_SIZE( 8, 0 );
        break;
    case NETMSG_POD_PROPERTY( 1, 1 ):
        size = NETMSG_POD_PROPERTY_SIZE( 1, 1 );
        break;
    case NETMSG_POD_PROPERTY( 2, 1 ):
        size = NETMSG_POD_PROPERTY_SIZE( 2, 1 );
        break;
    case NETMSG_POD_PROPERTY( 4, 1 ):
        size = NETMSG_POD_PROPERTY_SIZE( 4, 1 );
        break;
    case NETMSG_POD_PROPERTY( 8, 1 ):
        size = NETMSG_POD_PROPERTY_SIZE( 8, 1 );
        break;
    case NETMSG_POD_PROPERTY( 1, 2 ):
        size = NETMSG_POD_PROPERTY_SIZE( 1, 2 );
        break;
    case NETMSG_POD_PROPERTY( 2, 2 ):
        size = NETMSG_POD_PROPERTY_SIZE( 2, 2 );
        break;
    case NETMSG_POD_PROPERTY( 4, 2 ):
        size = NETMSG_POD_PROPERTY_SIZE( 4, 2 );
        break;
    case NETMSG_POD_PROPERTY( 8, 2 ):
        size = NETMSG_POD_PROPERTY_SIZE( 8, 2 );
        break;

    case NETMSG_LOGIN_SUCCESS:
    case NETMSG_LOADMAP:
    case NETMSG_CREATE_CLIENT:
    case NETMSG_UPDATE_FILES_LIST:
    case NETMSG_ADD_PLAYER:
    case NETMSG_ADD_NPC:
    case NETMSG_SEND_COMMAND:
    case NETMSG_SEND_TEXT:
    case NETMSG_CRITTER_TEXT:
    case NETMSG_MSG_LEX:
    case NETMSG_MAP_TEXT:
    case NETMSG_MAP_TEXT_MSG_LEX:
    case NETMSG_SOME_ITEMS:
    case NETMSG_CRITTER_MOVE_ITEM:
    case NETMSG_COMBAT_RESULTS:
    case NETMSG_PLAY_SOUND:
    case NETMSG_TALK_NPC:
    case NETMSG_MAP:
    case NETMSG_RPC:
    case NETMSG_GLOBAL_INFO:
    case NETMSG_AUTOMAPS_INFO:
    case NETMSG_ADD_ITEM:
    case NETMSG_ADD_ITEM_ON_MAP:
    case NETMSG_SOME_ITEM:
    case NETMSG_COMPLEX_PROPERTY:
    case NETMSG_SEND_COMPLEX_PROPERTY:
    case NETMSG_ALL_PROPERTIES:
    {
        // Changeable size
        uint msg_len = 0;
        EncryptKey( sizeof( msg ) );
        CopyBuf( bufData + bufReadPos + sizeof( msg ), &msg_len, EncryptKey( -(int) sizeof( msg ) ), sizeof( msg_len ) );
        size = msg_len;
    }
    break;
    default:
        Reset();
        return;
    }

    bufReadPos += size;
    EncryptKey( size );
}
