#include "Common.h"
#include "BufferManager.h"
#include "NetProtocol.h"
#include "Randomizer.h"

#define NET_BUFFER_SIZE    ( 2048 )

BufferManager::BufferManager()
{
    MEMORY_PROCESS( MEMORY_NET_BUFFER, NET_BUFFER_SIZE + sizeof( BufferManager ) );
    bufLen = NET_BUFFER_SIZE;
    bufEndPos = 0;
    bufReadPos = 0;
    bufData = new char[ bufLen ];
    encryptActive = false;
    isError = false;
}

BufferManager::BufferManager( uint alen )
{
    MEMORY_PROCESS( MEMORY_NET_BUFFER, alen + sizeof( BufferManager ) );
    bufLen = alen;
    bufEndPos = 0;
    bufReadPos = 0;
    bufData = new char[ bufLen ];
    encryptActive = false;
    isError = false;
}

BufferManager& BufferManager::operator=( const BufferManager& r )
{
    MEMORY_PROCESS( MEMORY_NET_BUFFER, -(int) bufLen );
    MEMORY_PROCESS( MEMORY_NET_BUFFER, r.bufLen );
    isError = r.isError;
    bufLen = r.bufLen;
    bufEndPos = r.bufEndPos;
    bufReadPos = r.bufReadPos;
    SAFEDELA( bufData );
    bufData = new char[ bufLen ];
    memcpy( bufData, r.bufData, bufLen );
    encryptActive = r.encryptActive;
    encryptKeyPos = r.encryptKeyPos;
    memcpy( encryptKeys, r.encryptKeys, sizeof( encryptKeys ) );
    return *this;
}

BufferManager::~BufferManager()
{
    MEMORY_PROCESS( MEMORY_NET_BUFFER, -(int) ( bufLen + sizeof( BufferManager ) ) );
    SAFEDELA( bufData );
}

void BufferManager::SetEncryptKey( uint seed )
{
    if( !seed )
    {
        encryptActive = false;
        return;
    }

    Randomizer rnd( seed );
    for( int i = 0; i < CRYPT_KEYS_COUNT; i++ )
        encryptKeys[ i ] = ( rnd.Random( 0x1000, 0xFFFF ) | rnd.Random( 0x1000, 0xFFFF ) );
    encryptKeyPos = 0;
    encryptActive = true;
}

void BufferManager::Lock()
{
    bufLocker.Lock();
}

void BufferManager::Unlock()
{
    bufLocker.Unlock();
}

void BufferManager::Refresh()
{
    if( isError )
        return;
    if( bufReadPos > bufEndPos )
    {
        isError = true;
        WriteLogF( _FUNC_, " - Error!\n" );
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

void BufferManager::Reset()
{
    bufEndPos = 0;
    bufReadPos = 0;
    if( bufLen > NET_BUFFER_SIZE )
    {
        MEMORY_PROCESS( MEMORY_NET_BUFFER, -(int) bufLen );
        MEMORY_PROCESS( MEMORY_NET_BUFFER, NET_BUFFER_SIZE );
        bufLen = NET_BUFFER_SIZE;
        SAFEDELA( bufData );
        bufData = new char[ bufLen ];
    }
}

void BufferManager::LockReset()
{
    Lock();
    Reset();
    Unlock();
}

void BufferManager::GrowBuf( uint len )
{
    if( bufEndPos + len < bufLen )
        return;
    MEMORY_PROCESS( MEMORY_NET_BUFFER, -(int) bufLen );
    while( bufEndPos + len >= bufLen )
        bufLen <<= 1;
    MEMORY_PROCESS( MEMORY_NET_BUFFER, bufLen );
    char* new_buf = new char[ bufLen ];
    memcpy( new_buf, bufData, bufEndPos );
    SAFEDELA( bufData );
    bufData = new_buf;
}

void BufferManager::MoveReadPos( int val )
{
    bufReadPos += val;
    EncryptKey( val );
}

void BufferManager::Push( const char* buf, uint len, bool no_crypt /* = false */ )
{
    if( isError || !len )
        return;
    if( bufEndPos + len >= bufLen )
        GrowBuf( len );
    CopyBuf( buf, bufData + bufEndPos, nullptr, no_crypt ? 0 : EncryptKey( len ), len );
    bufEndPos += len;
}

void BufferManager::Push( const char* buf, const char* mask, uint len )
{
    if( isError || !len || !mask )
        return;
    if( bufEndPos + len >= bufLen )
        GrowBuf( len );
    CopyBuf( buf, bufData + bufEndPos, mask, EncryptKey( len ), len );
    bufEndPos += len;
}

void BufferManager::Pop( char* buf, uint len )
{
    if( isError || !len )
        return;
    if( bufReadPos + len > bufEndPos )
    {
        isError = true;
        WriteLogF( _FUNC_, " - Error!\n" );
        return;
    }
    CopyBuf( bufData + bufReadPos, buf, nullptr, EncryptKey( len ), len );
    bufReadPos += len;
}

void BufferManager::Cut( uint len )
{
    if( isError || !len )
        return;
    if( bufReadPos + len > bufEndPos )
    {
        isError = true;
        WriteLogF( _FUNC_, " - Error!\n" );
        return;
    }
    char* buf = bufData + bufReadPos;
    for( uint i = 0; i + bufReadPos + len < bufEndPos; i++ )
        buf[ i ] = buf[ i + len ];
    bufEndPos -= len;
}

void BufferManager::CopyBuf( const char* from, char* to, const char* mask, uint crypt_key, uint len )
{
    if( mask )
    {
        if( len % sizeof( uint ) )
            for( uint i = 0; i < len; i++, to++, from++, mask++ )
                *to = ( *from & *mask ) ^ crypt_key;
        else
            for( uint i = 0, j = sizeof( uint ); i < len; i += j, to += j, from += j, mask += j )
                *(uint*) to = ( *(uint*) from & *(uint*) mask ) ^ crypt_key;
    }
    else
    {
        if( len % sizeof( uint ) )
            for( uint i = 0; i < len; i++, to++, from++ )
                *to = *from ^ crypt_key;
        else
            for( uint i = 0, j = sizeof( uint ); i < len; i += j, to += j, from += j )
                *(uint*) to = *(uint*) from ^ crypt_key;
    }
}

bool BufferManager::NeedProcess()
{
    if( bufReadPos + sizeof( uint ) > bufEndPos )
        return false;
    uint msg = *(uint*) ( bufData + bufReadPos ) ^ EncryptKey( 0 );

    // Known size
    switch( msg )
    {
    case 0xFFFFFFFF:
        return true;                                                                 // Ping
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
    case NETMSG_SEND_CHANGE_ITEM:
        return ( NETMSG_SEND_CHANGE_ITEM_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_PICK_ITEM:
        return ( NETMSG_SEND_PICK_ITEM_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_ITEM_CONT:
        return ( NETMSG_SEND_ITEM_CONT_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_USE_ITEM:
        return ( NETMSG_SEND_USE_ITEM_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_USE_SKILL:
        return ( NETMSG_SEND_USE_SKILL_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_PICK_CRITTER:
        return ( NETMSG_SEND_PICK_CRITTER_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_CRITTER_ACTION:
        return ( NETMSG_CRITTER_ACTION_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_CRITTER_KNOCKOUT:
        return ( NETMSG_CRITTER_KNOCKOUT_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_CRITTER_ANIMATE:
        return ( NETMSG_CRITTER_ANIMATE_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_CRITTER_SET_ANIMS:
        return ( NETMSG_CRITTER_SET_ANIMS_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_EFFECT:
        return ( NETMSG_EFFECT_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_FLY_EFFECT:
        return ( NETMSG_FLY_EFFECT_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_PLAY_SOUND:
        return ( NETMSG_PLAY_SOUND_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_PLAY_SOUND_TYPE:
        return ( NETMSG_PLAY_SOUND_TYPE_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_KARMA_VOTING:
        return ( NETMSG_SEND_KARMA_VOTING_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_TALK_NPC:
        return ( NETMSG_SEND_TALK_NPC_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_SAY_NPC:
        return ( NETMSG_SEND_SAY_NPC_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_PLAYERS_BARTER:
        return ( NETMSG_PLAYERS_BARTER_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_GET_INFO:
        return ( NETMSG_SEND_GET_TIME_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_GAME_INFO:
        return ( NETMSG_GAME_INFO_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_COMBAT:
        return ( NETMSG_SEND_COMBAT_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_GIVE_MAP:
        return ( NETMSG_SEND_GIVE_MAP_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_LOAD_MAP_OK:
        return ( NETMSG_SEND_LOAD_MAP_OK_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_DROP_TIMERS:
        return ( NETMSG_DROP_TIMERS_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_REFRESH_ME:
        return ( NETMSG_SEND_REFRESH_ME_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_VIEW_MAP:
        return ( NETMSG_VIEW_MAP_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_RULE_GLOBAL:
        return ( NETMSG_SEND_RULE_GLOBAL_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_FOLLOW:
        return ( NETMSG_FOLLOW_SIZE + bufReadPos <= bufEndPos );
    case NETMSG_SEND_GET_USER_HOLO_STR:
        return ( NETMSG_SEND_GET_USER_HOLO_STR_SIZE + bufReadPos <= bufEndPos );
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
    if( bufReadPos + sizeof( uint ) + sizeof( uint ) > bufEndPos )
        return false;

    EncryptKey( sizeof( uint ) );
    uint msg_len = *(uint*) ( bufData + bufReadPos + sizeof( uint ) ) ^ EncryptKey( -(int) sizeof( uint ) );

    switch( msg )
    {
    case NETMSG_CHECK_UID0:
    case NETMSG_CHECK_UID1:
    case NETMSG_CHECK_UID2:
    case NETMSG_CHECK_UID3:
    case NETMSG_CHECK_UID4:
    case NETMSG_LOGIN_SUCCESS:
    case NETMSG_LOADMAP:
    case NETMSG_CREATE_CLIENT:
    case NETMSG_SINGLEPLAYER_SAVE_LOAD:
    case NETMSG_UPDATE_FILES_LIST:
    case NETMSG_ADD_PLAYER:
    case NETMSG_ADD_NPC:
    case NETMSG_SEND_COMMAND:
    case NETMSG_SEND_TEXT:
    case NETMSG_CRITTER_TEXT:
    case NETMSG_MSG_LEX:
    case NETMSG_MAP_TEXT:
    case NETMSG_MAP_TEXT_MSG_LEX:
    case NETMSG_SEND_LEVELUP:
    case NETMSG_ADD_ITEM:
    case NETMSG_ADD_ITEM_ON_MAP:
    case NETMSG_SOME_ITEM:
    case NETMSG_PLAYERS_BARTER_SET_HIDE:
    case NETMSG_CONTAINER_INFO:
    case NETMSG_CRITTER_MOVE_ITEM:
    case NETMSG_COMBAT_RESULTS:
    case NETMSG_TALK_NPC:
    case NETMSG_SEND_BARTER:
    case NETMSG_MAP:
    case NETMSG_RUN_CLIENT_SCRIPT:
    case NETMSG_SEND_RUN_SERVER_SCRIPT:
    case NETMSG_GLOBAL_INFO:
    case NETMSG_GLOBAL_ENTRANCES:
    case NETMSG_HOLO_INFO:
    case NETMSG_SEND_SET_USER_HOLO_STR:
    case NETMSG_USER_HOLO_STR:
    case NETMSG_AUTOMAPS_INFO:
    case NETMSG_COMPLEX_PROPERTY:
    case NETMSG_SEND_COMPLEX_PROPERTY:
    case NETMSG_ALL_PROPERTIES:
        return ( msg_len + bufReadPos <= bufEndPos );
    default:
        // Unknown message
        Reset();
        isError = true;
        return false;
    }

    return false;
}

void BufferManager::SkipMsg( uint msg )
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
    case NETMSG_SEND_CHANGE_ITEM:
        size = NETMSG_SEND_CHANGE_ITEM_SIZE;
        break;
    case NETMSG_SEND_PICK_ITEM:
        size = NETMSG_SEND_PICK_ITEM_SIZE;
        break;
    case NETMSG_SEND_ITEM_CONT:
        size = NETMSG_SEND_ITEM_CONT_SIZE;
        break;
    case NETMSG_SEND_USE_ITEM:
        size = NETMSG_SEND_USE_ITEM_SIZE;
        break;
    case NETMSG_SEND_USE_SKILL:
        size = NETMSG_SEND_USE_SKILL_SIZE;
        break;
    case NETMSG_SEND_PICK_CRITTER:
        size = NETMSG_SEND_PICK_CRITTER_SIZE;
        break;
    case NETMSG_CRITTER_ACTION:
        size = NETMSG_CRITTER_ACTION_SIZE;
        break;
    case NETMSG_CRITTER_KNOCKOUT:
        size = NETMSG_CRITTER_KNOCKOUT_SIZE;
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
    case NETMSG_PLAY_SOUND:
        size = NETMSG_PLAY_SOUND_SIZE;
        break;
    case NETMSG_PLAY_SOUND_TYPE:
        size = NETMSG_PLAY_SOUND_TYPE_SIZE;
        break;
    case NETMSG_SEND_KARMA_VOTING:
        size = NETMSG_SEND_KARMA_VOTING_SIZE;
        break;
    case NETMSG_SEND_TALK_NPC:
        size = NETMSG_SEND_TALK_NPC_SIZE;
        break;
    case NETMSG_SEND_SAY_NPC:
        size = NETMSG_SEND_SAY_NPC_SIZE;
        break;
    case NETMSG_PLAYERS_BARTER:
        size = NETMSG_PLAYERS_BARTER_SIZE;
        break;
    case NETMSG_SEND_GET_INFO:
        size = NETMSG_SEND_GET_TIME_SIZE;
        break;
    case NETMSG_GAME_INFO:
        size = NETMSG_GAME_INFO_SIZE;
        break;
    case NETMSG_SEND_COMBAT:
        size = NETMSG_SEND_COMBAT_SIZE;
        break;
    case NETMSG_SEND_GIVE_MAP:
        size = NETMSG_SEND_GIVE_MAP_SIZE;
        break;
    case NETMSG_SEND_LOAD_MAP_OK:
        size = NETMSG_SEND_LOAD_MAP_OK_SIZE;
        break;
    case NETMSG_DROP_TIMERS:
        size = NETMSG_DROP_TIMERS_SIZE;
        break;
    case NETMSG_SEND_REFRESH_ME:
        size = NETMSG_SEND_REFRESH_ME_SIZE;
        break;
    case NETMSG_VIEW_MAP:
        size = NETMSG_VIEW_MAP_SIZE;
        break;
    case NETMSG_SEND_RULE_GLOBAL:
        size = NETMSG_SEND_RULE_GLOBAL_SIZE;
        break;
    case NETMSG_FOLLOW:
        size = NETMSG_FOLLOW_SIZE;
        break;
    case NETMSG_SEND_GET_USER_HOLO_STR:
        size = NETMSG_SEND_GET_USER_HOLO_STR_SIZE;
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

    case NETMSG_CHECK_UID0:
    case NETMSG_CHECK_UID1:
    case NETMSG_CHECK_UID2:
    case NETMSG_CHECK_UID3:
    case NETMSG_CHECK_UID4:
    case NETMSG_LOGIN_SUCCESS:
    case NETMSG_LOADMAP:
    case NETMSG_CREATE_CLIENT:
    case NETMSG_SINGLEPLAYER_SAVE_LOAD:
    case NETMSG_UPDATE_FILES_LIST:
    case NETMSG_ADD_PLAYER:
    case NETMSG_ADD_NPC:
    case NETMSG_SEND_COMMAND:
    case NETMSG_SEND_TEXT:
    case NETMSG_CRITTER_TEXT:
    case NETMSG_MSG_LEX:
    case NETMSG_MAP_TEXT:
    case NETMSG_MAP_TEXT_MSG_LEX:
    case NETMSG_SEND_LEVELUP:
    case NETMSG_CONTAINER_INFO:
    case NETMSG_CRITTER_MOVE_ITEM:
    case NETMSG_COMBAT_RESULTS:
    case NETMSG_TALK_NPC:
    case NETMSG_SEND_BARTER:
    case NETMSG_MAP:
    case NETMSG_RUN_CLIENT_SCRIPT:
    case NETMSG_SEND_RUN_SERVER_SCRIPT:
    case NETMSG_GLOBAL_INFO:
    case NETMSG_GLOBAL_ENTRANCES:
    case NETMSG_HOLO_INFO:
    case NETMSG_SEND_SET_USER_HOLO_STR:
    case NETMSG_USER_HOLO_STR:
    case NETMSG_AUTOMAPS_INFO:
    case NETMSG_ADD_ITEM:
    case NETMSG_ADD_ITEM_ON_MAP:
    case NETMSG_SOME_ITEM:
    case NETMSG_PLAYERS_BARTER_SET_HIDE:
    case NETMSG_COMPLEX_PROPERTY:
    case NETMSG_SEND_COMPLEX_PROPERTY:
    case NETMSG_ALL_PROPERTIES:
    {
        // Changeable size
        EncryptKey( sizeof( msg ) );
        uint msg_len = *(uint*) ( bufData + bufReadPos + sizeof( msg ) ) ^ EncryptKey( -(int) sizeof( msg ) );
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
