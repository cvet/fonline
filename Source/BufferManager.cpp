#include "StdAfx.h"
#include "BufferManager.h"
#include "NetProtocol.h"

#define NET_BUFFER_SIZE             (2048)

BufferManager::BufferManager()
{
	MEMORY_PROCESS(MEMORY_NET_BUFFER,NET_BUFFER_SIZE+sizeof(BufferManager));
	InitializeCriticalSection(&cs);
	bufLen=NET_BUFFER_SIZE;
	bufEndPos=0;
	bufReadPos=0;
	bufData=new char[bufLen];
	isError=false;
}

BufferManager::BufferManager(DWORD alen)
{
	MEMORY_PROCESS(MEMORY_NET_BUFFER,alen+sizeof(BufferManager));
	InitializeCriticalSection(&cs);
	bufLen=alen;
	bufEndPos=0;
	bufReadPos=0;
	bufData=new char[bufLen];
	isError=false;
}

BufferManager& BufferManager::operator=(const BufferManager& r)
{
	MEMORY_PROCESS(MEMORY_NET_BUFFER,-(int)bufLen);
	MEMORY_PROCESS(MEMORY_NET_BUFFER,r.bufLen);
	isError=r.isError;
	bufLen=r.bufLen;
	bufEndPos=r.bufEndPos;
	bufReadPos=r.bufReadPos;
	SAFEDELA(bufData);
	bufData=new char[bufLen];
	memcpy(bufData,r.bufData,bufLen);
	return *this;
}

BufferManager::~BufferManager()
{
	MEMORY_PROCESS(MEMORY_NET_BUFFER,-(int)(bufLen+sizeof(BufferManager)));
	DeleteCriticalSection(&cs);
	SAFEDELA(bufData);
}

void BufferManager::Lock()
{
	EnterCriticalSection(&cs);
}

void BufferManager::Unlock()
{
	LeaveCriticalSection(&cs);
}

void BufferManager::Refresh()
{
	if(isError) return;
	if(bufReadPos>bufEndPos) { isError=true; WriteLog(__FUNCTION__" - Error!\n"); return; }
	if(bufReadPos)
	{
		for(DWORD i=bufReadPos;i<bufEndPos;i++) bufData[i-bufReadPos]=bufData[i];
		bufEndPos-=bufReadPos;
		bufReadPos=0;
	}
}

void BufferManager::Reset()
{
	bufEndPos=0;
	bufReadPos=0;
	if(bufLen>NET_BUFFER_SIZE)
	{
		MEMORY_PROCESS(MEMORY_NET_BUFFER,-(int)bufLen);
		MEMORY_PROCESS(MEMORY_NET_BUFFER,NET_BUFFER_SIZE);
		bufLen=NET_BUFFER_SIZE;
		SAFEDELA(bufData);
		bufData=new char[bufLen];
	}
}

void BufferManager::LockReset()
{
	Lock();
	Reset();
	Unlock();
}

void BufferManager::GrowBuf(DWORD len)
{
	MEMORY_PROCESS(MEMORY_NET_BUFFER,-(int)bufLen);
	while(bufEndPos+len>=bufLen) bufLen<<=1;
	MEMORY_PROCESS(MEMORY_NET_BUFFER,bufLen);
	char* new_buf=new char[bufLen];
	memcpy(new_buf,bufData,bufEndPos);
	SAFEDELA(bufData);
	bufData=new_buf;
}

void BufferManager::Push(const char* buf, DWORD len)
{
	if(isError || !len) return;
	if(bufEndPos+len>=bufLen) GrowBuf(len);
	memcpy(bufData+bufEndPos,buf,len);
	bufEndPos+=len;
}

void BufferManager::Pop(char* buf, DWORD len)
{
	if(isError || !len) return;
	if(bufReadPos+len>bufEndPos) { isError=true; WriteLog(__FUNCTION__" - Error!\n"); return; }
	memcpy(buf,bufData+bufReadPos,len);
	bufReadPos+=len;
}

void BufferManager::Pop(DWORD len)
{
	if(isError || !len) return;
	if(bufReadPos+len>bufEndPos) { isError=true; WriteLog(__FUNCTION__" - Error!\n"); return; }
	char* buf=bufData+bufReadPos;
	for(DWORD i=0;i+bufReadPos+len<bufEndPos;i++) buf[i]=buf[i+len];
	bufEndPos-=len;
}

BufferManager& BufferManager::operator<<(int i)
{
	if(isError) return *this;
	if(bufEndPos+4>=bufLen) GrowBuf(4);
	*(int*)(bufData+bufEndPos)=i;
	bufEndPos+=4;
	return *this;
}

BufferManager& BufferManager::operator>>(int& i)
{
	if(isError) return *this;
	if(bufReadPos+4>bufEndPos) { isError=true; WriteLog(__FUNCTION__" - Error!\n"); return *this; }
	i=*(int*)(bufData+bufReadPos);
	bufReadPos+=4;
	return *this;
}

BufferManager& BufferManager::operator<<(DWORD i)
{
	if(isError) return *this;
	if(bufEndPos+4>=bufLen) GrowBuf(4);
	*(DWORD*)(bufData+bufEndPos)=i;
	bufEndPos+=4;
	return *this;
}

BufferManager& BufferManager::operator>>(DWORD& i)
{
	if(isError) return *this;
	if(bufReadPos+4>bufEndPos) { isError=true; WriteLog(__FUNCTION__" - Error!\n"); return *this; }
	i=*(DWORD*)(bufData+bufReadPos);
	bufReadPos+=4;
	return *this;
}

BufferManager& BufferManager::operator<<(WORD i)
{
	if(isError) return *this;
	if(bufEndPos+2>=bufLen) GrowBuf(2);
	*(WORD*)(bufData+bufEndPos)=i;
	bufEndPos+=2;
	return *this;
}

BufferManager& BufferManager::operator>>(WORD& i)
{
	if(isError) return *this;
	if(bufReadPos+2>bufEndPos) { isError=true; WriteLog(__FUNCTION__" - Error!\n"); return *this; }
	i=*(WORD*)(bufData+bufReadPos);
	bufReadPos+=2;
	return *this;
}

BufferManager& BufferManager::operator<<(short i)
{
	if(isError) return *this;
	if(bufEndPos+2>=bufLen) GrowBuf(2);
	*(short*)(bufData+bufEndPos)=i;
	bufEndPos+=2;
	return *this;
}

BufferManager& BufferManager::operator>>(short& i)
{
	if(isError) return *this;
	if(bufReadPos+2>bufEndPos) { isError=true; WriteLog(__FUNCTION__" - Error!\n"); return *this; }
	i=*(short*)(bufData+bufReadPos);
	bufReadPos+=2;
	return *this;
}

BufferManager& BufferManager::operator<<(BYTE i)
{
	if(isError) return *this;
	if(bufEndPos+1>=bufLen) GrowBuf(1);
	*(BYTE*)(bufData+bufEndPos)=i;
	bufEndPos+=1;
	return *this;
}

BufferManager& BufferManager::operator>>(BYTE& i)
{
	if(isError) return *this;
	if(bufReadPos+1>bufEndPos) { isError=true; WriteLog(__FUNCTION__" - Error!\n"); return *this; }
	i=*(BYTE*)(bufData+bufReadPos);
	bufReadPos+=1;
	return *this;
}

BufferManager& BufferManager::operator<<(bool i)
{
	if(isError) return *this;
	if(bufEndPos+1>=bufLen) GrowBuf(1);
	*(BYTE*)(bufData+bufEndPos)=(i?1:0);
	bufEndPos+=1;
	return *this;
}

BufferManager& BufferManager::operator>>(bool& i)
{
	if(isError) return *this;
	if(bufReadPos+1>bufEndPos) { isError=true; WriteLog(__FUNCTION__" - Error!\n"); return *this; }
	i=((*(BYTE*)(bufData+bufReadPos))?true:false);
	bufReadPos+=1;
	return *this;
}

#if (defined(FONLINE_SERVER)) || (defined(FONLINE_CLIENT))
bool BufferManager::NeedProcess()
{
	if(bufReadPos+sizeof(MSGTYPE)>bufEndPos) return false;
	MSGTYPE msg=*(MSGTYPE*)(bufData+bufReadPos);

	//TODO: #ifdef FONLINE_SERVER, FONLINE_CLIENT

	// Known size
	switch(msg)
	{
	case 0xFFFFFFFF:						return true;
	case NETMSG_LOGIN:						return (NETMSG_LOGIN_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_LOGIN_SUCCESS:				return (NETMSG_LOGIN_SUCCESS_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_REGISTER_SUCCESS:			return (NETMSG_REGISTER_SUCCESS_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_PING:						return (NETMSG_PING_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_REMOVE_CRITTER:				return (NETMSG_REMOVE_CRITTER_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_MSG:						return (NETMSG_MSG_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_MAP_TEXT_MSG:				return (NETMSG_MAP_TEXT_MSG_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_DIR:						return (NETMSG_DIR_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_CRITTER_DIR:				return (NETMSG_CRITTER_DIR_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_MOVE_WALK:				return (NETMSG_SEND_MOVE_WALK_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_MOVE_RUN:				return (NETMSG_SEND_MOVE_RUN_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_CRITTER_MOVE:				return (NETMSG_CRITTER_MOVE_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_CRITTER_XY:					return (NETMSG_CRITTER_XY_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_ALL_PARAMS:					return (NETMSG_ALL_PARAMS_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_PARAM:						return (NETMSG_PARAM_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_CRITTER_PARAM:				return (NETMSG_CRITTER_PARAM_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_CRAFT:					return (NETMSG_SEND_CRAFT_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_CRAFT_RESULT:				return (NETMSG_CRAFT_RESULT_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_CLEAR_ITEMS:				return (NETMSG_CLEAR_ITEMS_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_ADD_ITEM:					return (NETMSG_ADD_ITEM_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_REMOVE_ITEM:				return (NETMSG_REMOVE_ITEM_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_SORT_VALUE_ITEM:		return (NETMSG_SEND_SORT_VALUE_ITEM_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_ADD_ITEM_ON_MAP:			return (NETMSG_ADD_ITEM_ON_MAP_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_CHANGE_ITEM_ON_MAP:			return (NETMSG_CHANGE_ITEM_ON_MAP_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_ERASE_ITEM_FROM_MAP:		return (NETMSG_ERASE_ITEM_FROM_MAP_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_ANIMATE_ITEM:				return (NETMSG_ANIMATE_ITEM_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_RATE_ITEM:				return (NETMSG_SEND_RATE_ITEM_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_CHANGE_ITEM:			return (NETMSG_SEND_CHANGE_ITEM_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_PICK_ITEM:				return (NETMSG_SEND_PICK_ITEM_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_ITEM_CONT:				return (NETMSG_SEND_ITEM_CONT_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_USE_ITEM:				return (NETMSG_SEND_USE_ITEM_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_USE_SKILL:				return (NETMSG_SEND_USE_SKILL_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_PICK_CRITTER:			return (NETMSG_SEND_PICK_CRITTER_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SOME_ITEM:					return (NETMSG_SOME_ITEM_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_CRITTER_ACTION:				return (NETMSG_CRITTER_ACTION_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_CRITTER_KNOCKOUT:			return (NETMSG_CRITTER_KNOCKOUT_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_CRITTER_ANIMATE:			return (NETMSG_CRITTER_ANIMATE_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_EFFECT:						return (NETMSG_EFFECT_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_FLY_EFFECT:					return (NETMSG_FLY_EFFECT_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_PLAY_SOUND:					return (NETMSG_PLAY_SOUND_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_PLAY_SOUND_TYPE:			return (NETMSG_PLAY_SOUND_TYPE_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_KARMA_VOTING:			return (NETMSG_SEND_KARMA_VOTING_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_TALK_NPC:				return (NETMSG_SEND_TALK_NPC_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_SAY_NPC:				return (NETMSG_SEND_SAY_NPC_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_PLAYERS_BARTER:				return (NETMSG_PLAYERS_BARTER_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_PLAYERS_BARTER_SET_HIDE:	return (NETMSG_PLAYERS_BARTER_SET_HIDE_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_GET_INFO:				return (NETMSG_SEND_GET_TIME_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_GAME_INFO:					return (NETMSG_GAME_INFO_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_COMBAT:				return (NETMSG_SEND_COMBAT_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_LOADMAP:					return (NETMSG_LOADMAP_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_GIVE_ME_MAP:			return (NETMSG_SEND_GIVE_ME_MAP_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_LOAD_MAP_OK:			return (NETMSG_SEND_LOAD_MAP_OK_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SHOW_SCREEN:				return (NETMSG_SHOW_SCREEN_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_SCREEN_ANSWER:			return (NETMSG_SEND_SCREEN_ANSWER_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_DROP_TIMERS:				return (NETMSG_DROP_TIMERS_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_REFRESH_ME:			return (NETMSG_SEND_REFRESH_ME_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_VIEW_MAP:					return (NETMSG_VIEW_MAP_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_GIVE_GLOBAL_INFO:		return (NETMSG_SEND_GIVE_GLOBAL_INFO_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_RULE_GLOBAL:			return (NETMSG_SEND_RULE_GLOBAL_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_FOLLOW:						return (NETMSG_FOLLOW_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_RADIO:						return (NETMSG_RADIO_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_QUEST:						return (NETMSG_QUEST_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_GET_USER_HOLO_STR:		return (NETMSG_SEND_GET_USER_HOLO_STR_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SEND_GET_SCORES:			return (NETMSG_SEND_GET_SCORES_SIZE+bufReadPos<=bufEndPos);
	case NETMSG_SCORES:						return (NETMSG_SCORES_SIZE+bufReadPos<=bufEndPos);
	}

	// Changeable Size
	if(bufReadPos+sizeof(MSGTYPE)+sizeof(DWORD)>bufEndPos) return false;
	DWORD msg_len=*(DWORD*)(bufData+bufReadPos+sizeof(MSGTYPE));

	switch(msg)
	{
	case NETMSG_CHECK_UID0:
	case NETMSG_CHECK_UID1:
	case NETMSG_CHECK_UID2:
	case NETMSG_CHECK_UID3:
	case NETMSG_CHECK_UID4:
	case NETMSG_CREATE_CLIENT:
	case NETMSG_ADD_PLAYER:
	case NETMSG_ADD_NPC:
	case NETMSG_CRITTER_LEXEMS:
	case NETMSG_ITEM_LEXEMS:
	case NETMSG_SEND_COMMAND:
	case NETMSG_SEND_TEXT:
	case NETMSG_CRITTER_TEXT:
	case NETMSG_MSG_LEX:
	case NETMSG_MAP_TEXT:
	case NETMSG_MAP_TEXT_MSG_LEX:
	case NETMSG_SEND_LEVELUP:
	case NETMSG_CRAFT_ASK:
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
	case NETMSG_MSG_DATA:
	case NETMSG_ITEM_PROTOS:
	case NETMSG_QUESTS:
	case NETMSG_HOLO_INFO:
	case NETMSG_SEND_SET_USER_HOLO_STR:
	case NETMSG_USER_HOLO_STR:
		return (msg_len+bufReadPos<=bufEndPos);
	default:
		// Unknown message
#ifdef FONLINE_CLIENT
		WriteLog(__FUNCTION__" - Unknown message<%u> in buffer, try find valid.\n",(msg>>8)&0xFF);
		SeekValidMsg();
		return NeedProcess();
#else
		//WriteLog(__FUNCTION__" - Unknown message<%u> in buffer, reset.\n",msg);
		Reset();
#endif
		return false;
	}
	
	return false;
}

void BufferManager::SkipMsg(MSGTYPE msg)
{
	bufReadPos-=sizeof(msg);

	// Known Size
	switch(msg)
	{
	case 0xFFFFFFFF:						bufReadPos+=16; return;
	case NETMSG_LOGIN:						bufReadPos+=NETMSG_LOGIN_SIZE; return;
	case NETMSG_LOGIN_SUCCESS:				bufReadPos+=NETMSG_LOGIN_SUCCESS_SIZE; return;
	case NETMSG_REGISTER_SUCCESS:			bufReadPos+=NETMSG_REGISTER_SUCCESS_SIZE; return;
	case NETMSG_PING:						bufReadPos+=NETMSG_PING_SIZE; return;
	case NETMSG_REMOVE_CRITTER:				bufReadPos+=NETMSG_REMOVE_CRITTER_SIZE; return;
	case NETMSG_MSG:						bufReadPos+=NETMSG_MSG_SIZE; return;
	case NETMSG_MAP_TEXT_MSG:				bufReadPos+=NETMSG_MAP_TEXT_MSG_SIZE; return;
	case NETMSG_DIR:						bufReadPos+=NETMSG_DIR_SIZE; return;
	case NETMSG_CRITTER_DIR:				bufReadPos+=NETMSG_CRITTER_DIR_SIZE; return;
	case NETMSG_SEND_MOVE_WALK:				bufReadPos+=NETMSG_SEND_MOVE_WALK_SIZE; return;
	case NETMSG_SEND_MOVE_RUN:				bufReadPos+=NETMSG_SEND_MOVE_RUN_SIZE; return;
	case NETMSG_CRITTER_MOVE:				bufReadPos+=NETMSG_CRITTER_MOVE_SIZE; return;
	case NETMSG_CRITTER_XY:					bufReadPos+=NETMSG_CRITTER_XY_SIZE; return;
	case NETMSG_ALL_PARAMS:					bufReadPos+=NETMSG_ALL_PARAMS_SIZE; return;
	case NETMSG_PARAM:						bufReadPos+=NETMSG_PARAM_SIZE; return;
	case NETMSG_CRITTER_PARAM:				bufReadPos+=NETMSG_CRITTER_PARAM_SIZE; return;
	case NETMSG_SEND_CRAFT:					bufReadPos+=NETMSG_SEND_CRAFT_SIZE; return;
	case NETMSG_CRAFT_RESULT:				bufReadPos+=NETMSG_CRAFT_RESULT_SIZE; return;	
	case NETMSG_CLEAR_ITEMS:				bufReadPos+=NETMSG_CLEAR_ITEMS_SIZE; return;
	case NETMSG_ADD_ITEM:					bufReadPos+=NETMSG_ADD_ITEM_SIZE; return;
	case NETMSG_REMOVE_ITEM:				bufReadPos+=NETMSG_REMOVE_ITEM_SIZE; return;
	case NETMSG_SEND_SORT_VALUE_ITEM:		bufReadPos+=NETMSG_SEND_SORT_VALUE_ITEM_SIZE; return;
	case NETMSG_ADD_ITEM_ON_MAP:			bufReadPos+=NETMSG_ADD_ITEM_ON_MAP_SIZE; return;
	case NETMSG_CHANGE_ITEM_ON_MAP:			bufReadPos+=NETMSG_CHANGE_ITEM_ON_MAP_SIZE; return;
	case NETMSG_ERASE_ITEM_FROM_MAP:		bufReadPos+=NETMSG_ERASE_ITEM_FROM_MAP_SIZE; return;
	case NETMSG_ANIMATE_ITEM:				bufReadPos+=NETMSG_ANIMATE_ITEM_SIZE; return;
	case NETMSG_SEND_RATE_ITEM:				bufReadPos+=NETMSG_SEND_RATE_ITEM_SIZE; return;
	case NETMSG_SEND_CHANGE_ITEM:			bufReadPos+=NETMSG_SEND_CHANGE_ITEM_SIZE; return;
	case NETMSG_SEND_PICK_ITEM:				bufReadPos+=NETMSG_SEND_PICK_ITEM_SIZE; return;
	case NETMSG_SEND_ITEM_CONT:				bufReadPos+=NETMSG_SEND_ITEM_CONT_SIZE; return;
	case NETMSG_SEND_USE_ITEM:				bufReadPos+=NETMSG_SEND_USE_ITEM_SIZE; return;
	case NETMSG_SEND_USE_SKILL:				bufReadPos+=NETMSG_SEND_USE_SKILL_SIZE; return;
	case NETMSG_SEND_PICK_CRITTER:			bufReadPos+=NETMSG_SEND_PICK_CRITTER_SIZE; return;
	case NETMSG_SOME_ITEM:					bufReadPos+=NETMSG_SOME_ITEM_SIZE; return;
	case NETMSG_CRITTER_ACTION:				bufReadPos+=NETMSG_CRITTER_ACTION_SIZE; return;
	case NETMSG_CRITTER_KNOCKOUT:			bufReadPos+=NETMSG_CRITTER_KNOCKOUT_SIZE; return;
	case NETMSG_CRITTER_ANIMATE:			bufReadPos+=NETMSG_CRITTER_ANIMATE_SIZE; return;
	case NETMSG_EFFECT:						bufReadPos+=NETMSG_EFFECT_SIZE; return;
	case NETMSG_FLY_EFFECT:					bufReadPos+=NETMSG_FLY_EFFECT_SIZE; return;
	case NETMSG_PLAY_SOUND:					bufReadPos+=NETMSG_PLAY_SOUND_SIZE; return;
	case NETMSG_PLAY_SOUND_TYPE:			bufReadPos+=NETMSG_PLAY_SOUND_TYPE_SIZE; return;
	case NETMSG_SEND_KARMA_VOTING:			bufReadPos+=NETMSG_SEND_KARMA_VOTING_SIZE; return;
	case NETMSG_SEND_TALK_NPC:				bufReadPos+=NETMSG_SEND_TALK_NPC_SIZE; return;
	case NETMSG_SEND_SAY_NPC:				bufReadPos+=NETMSG_SEND_SAY_NPC_SIZE; return;
	case NETMSG_PLAYERS_BARTER:				bufReadPos+=NETMSG_PLAYERS_BARTER_SIZE; return;
	case NETMSG_PLAYERS_BARTER_SET_HIDE:	bufReadPos+=NETMSG_PLAYERS_BARTER_SET_HIDE_SIZE; return;
	case NETMSG_SEND_GET_INFO:				bufReadPos+=NETMSG_SEND_GET_TIME_SIZE; return;
	case NETMSG_GAME_INFO:					bufReadPos+=NETMSG_GAME_INFO_SIZE; return;
	case NETMSG_SEND_COMBAT:				bufReadPos+=NETMSG_SEND_COMBAT_SIZE; return;
	case NETMSG_LOADMAP:					bufReadPos+=NETMSG_LOADMAP_SIZE; return;
	case NETMSG_SEND_GIVE_ME_MAP:			bufReadPos+=NETMSG_SEND_GIVE_ME_MAP_SIZE; return;
	case NETMSG_SEND_LOAD_MAP_OK:			bufReadPos+=NETMSG_SEND_LOAD_MAP_OK_SIZE; return;
	case NETMSG_SHOW_SCREEN:				bufReadPos+=NETMSG_SHOW_SCREEN_SIZE; return;
	case NETMSG_SEND_SCREEN_ANSWER:			bufReadPos+=NETMSG_SEND_SCREEN_ANSWER_SIZE; return;
	case NETMSG_DROP_TIMERS:				bufReadPos+=NETMSG_DROP_TIMERS_SIZE; return;
	case NETMSG_SEND_REFRESH_ME:			bufReadPos+=NETMSG_SEND_REFRESH_ME_SIZE; return;
	case NETMSG_VIEW_MAP:					bufReadPos+=NETMSG_VIEW_MAP_SIZE; return;
	case NETMSG_SEND_GIVE_GLOBAL_INFO:		bufReadPos+=NETMSG_SEND_GIVE_GLOBAL_INFO_SIZE; return;
	case NETMSG_SEND_RULE_GLOBAL:			bufReadPos+=NETMSG_SEND_RULE_GLOBAL_SIZE; return;
	case NETMSG_FOLLOW:						bufReadPos+=NETMSG_FOLLOW_SIZE; return;
	case NETMSG_RADIO:						bufReadPos+=NETMSG_RADIO_SIZE; return;
	case NETMSG_QUEST:						bufReadPos+=NETMSG_QUEST_SIZE; return;
	case NETMSG_SEND_GET_USER_HOLO_STR:		bufReadPos+=NETMSG_SEND_GET_USER_HOLO_STR_SIZE; return;
	case NETMSG_SEND_GET_SCORES:			bufReadPos+=NETMSG_SEND_GET_SCORES_SIZE; return;
	case NETMSG_SCORES:						bufReadPos+=NETMSG_SCORES_SIZE; return;

	case NETMSG_CHECK_UID0:
	case NETMSG_CHECK_UID1:
	case NETMSG_CHECK_UID2:
	case NETMSG_CHECK_UID3:
	case NETMSG_CHECK_UID4:
	case NETMSG_CREATE_CLIENT:
	case NETMSG_ADD_PLAYER:
	case NETMSG_ADD_NPC:
	case NETMSG_CRITTER_LEXEMS:
	case NETMSG_ITEM_LEXEMS:
	case NETMSG_SEND_COMMAND:
	case NETMSG_SEND_TEXT:
	case NETMSG_CRITTER_TEXT:
	case NETMSG_MSG_LEX:
	case NETMSG_MAP_TEXT:
	case NETMSG_MAP_TEXT_MSG_LEX:
	case NETMSG_SEND_LEVELUP:
	case NETMSG_CRAFT_ASK:
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
	case NETMSG_MSG_DATA:
	case NETMSG_ITEM_PROTOS:
	case NETMSG_QUESTS:
	case NETMSG_HOLO_INFO:
	case NETMSG_SEND_SET_USER_HOLO_STR:
	case NETMSG_USER_HOLO_STR:
		{
			//Changeable Size
			DWORD msg_len=*(DWORD*)(bufData+bufReadPos+sizeof(msg));
			bufReadPos+=msg_len;
		}
		break;
	default:
		Reset();
		return;
	}
}

void BufferManager::SeekValidMsg()
{
	while(true)
	{
		if(bufReadPos+sizeof(MSGTYPE)>bufEndPos)
		{
			Reset();
			return;
		}
		MSGTYPE msg=*(MSGTYPE*)(bufData+bufReadPos);
		switch(msg)
		{
		case 0xFFFFFFFF:
		case NETMSG_CHECK_UID0:
		case NETMSG_CHECK_UID1:
		case NETMSG_CHECK_UID2:
		case NETMSG_CHECK_UID3:
		case NETMSG_CHECK_UID4:
		case NETMSG_LOGIN:
		case NETMSG_LOGIN_SUCCESS:
		case NETMSG_CREATE_CLIENT:
		case NETMSG_REGISTER_SUCCESS:
		case NETMSG_PING:
		case NETMSG_ADD_PLAYER:
		case NETMSG_ADD_NPC:
		case NETMSG_REMOVE_CRITTER:
		case NETMSG_MSG:
		case NETMSG_MAP_TEXT_MSG:
		case NETMSG_DIR:
		case NETMSG_CRITTER_DIR:
		case NETMSG_SEND_MOVE_WALK:
		case NETMSG_SEND_MOVE_RUN:
		case NETMSG_CRITTER_MOVE:
		case NETMSG_CRITTER_XY:
		case NETMSG_ALL_PARAMS:
		case NETMSG_PARAM:
		case NETMSG_CRITTER_PARAM:
		case NETMSG_SEND_CRAFT:
		case NETMSG_CRAFT_RESULT:
		case NETMSG_CLEAR_ITEMS:
		case NETMSG_ADD_ITEM:
		case NETMSG_REMOVE_ITEM:
		case NETMSG_SEND_SORT_VALUE_ITEM:
		case NETMSG_ADD_ITEM_ON_MAP:
		case NETMSG_CHANGE_ITEM_ON_MAP:
		case NETMSG_ERASE_ITEM_FROM_MAP:
		case NETMSG_ANIMATE_ITEM:
		case NETMSG_SEND_RATE_ITEM:
		case NETMSG_SEND_CHANGE_ITEM:
		case NETMSG_SEND_PICK_ITEM:
		case NETMSG_SEND_ITEM_CONT:
		case NETMSG_SEND_USE_ITEM:
		case NETMSG_SEND_USE_SKILL:
		case NETMSG_SEND_PICK_CRITTER:
		case NETMSG_SOME_ITEM:
		case NETMSG_CRITTER_ACTION:
		case NETMSG_CRITTER_KNOCKOUT:
		case NETMSG_CRITTER_MOVE_ITEM:
		case NETMSG_CRITTER_ANIMATE:
		case NETMSG_EFFECT:
		case NETMSG_FLY_EFFECT:
		case NETMSG_PLAY_SOUND:
		case NETMSG_PLAY_SOUND_TYPE:
		case NETMSG_SEND_KARMA_VOTING:
		case NETMSG_SEND_TALK_NPC:
		case NETMSG_SEND_SAY_NPC:
		case NETMSG_PLAYERS_BARTER:
		case NETMSG_PLAYERS_BARTER_SET_HIDE:
		case NETMSG_SEND_GET_INFO:
		case NETMSG_GAME_INFO:
		case NETMSG_SEND_COMBAT:
		case NETMSG_LOADMAP:
		case NETMSG_SEND_GIVE_ME_MAP:
		case NETMSG_SEND_LOAD_MAP_OK:
		case NETMSG_SHOW_SCREEN:
		case NETMSG_SEND_SCREEN_ANSWER:
		case NETMSG_DROP_TIMERS:
		case NETMSG_SEND_REFRESH_ME:
		case NETMSG_VIEW_MAP:
		case NETMSG_SEND_GIVE_GLOBAL_INFO:
		case NETMSG_SEND_RULE_GLOBAL:
		case NETMSG_FOLLOW:
		case NETMSG_RADIO:
		case NETMSG_QUEST:
		case NETMSG_SEND_GET_USER_HOLO_STR:
		case NETMSG_SEND_GET_SCORES:
		case NETMSG_SCORES:
		case NETMSG_CRITTER_LEXEMS:
		case NETMSG_ITEM_LEXEMS:
		case NETMSG_SEND_COMMAND:
		case NETMSG_SEND_TEXT:
		case NETMSG_CRITTER_TEXT:
		case NETMSG_MSG_LEX:
		case NETMSG_MAP_TEXT:
		case NETMSG_MAP_TEXT_MSG_LEX:
		case NETMSG_SEND_LEVELUP:
		case NETMSG_CRAFT_ASK:
		case NETMSG_CONTAINER_INFO:
		case NETMSG_TALK_NPC:
		case NETMSG_SEND_BARTER:
		case NETMSG_MAP:
		case NETMSG_RUN_CLIENT_SCRIPT:
		case NETMSG_SEND_RUN_SERVER_SCRIPT:
		case NETMSG_GLOBAL_INFO:
		case NETMSG_GLOBAL_ENTRANCES:
		case NETMSG_MSG_DATA:
		case NETMSG_ITEM_PROTOS:
		case NETMSG_QUESTS:
		case NETMSG_HOLO_INFO:
		case NETMSG_SEND_SET_USER_HOLO_STR:
		case NETMSG_USER_HOLO_STR:
			return;
		default:
			bufReadPos++;
			continue;
		}
	}
}
#endif
