#ifndef __BUFFER_MANAGER__
#define __BUFFER_MANAGER__

#include "Common.h"

#define CRYPT_KEYS_COUNT      (50)

class BufferManager
{
private:
	bool isError;
	Mutex bufLocker;
	char* bufData;
	DWORD bufLen;
	DWORD bufEndPos;
	DWORD bufReadPos;
	bool encryptActive;
	int encryptKeyPos;
	DWORD encryptKeys[CRYPT_KEYS_COUNT];

	void CopyBuf(const char* from, char* to, const char* mask, DWORD crypt_key, DWORD len);

public:
	BufferManager();
	BufferManager(DWORD alen);
	BufferManager& operator=(const BufferManager& r);
	~BufferManager();

	void SetEncryptKey(DWORD seed);
	void Lock();
	void Unlock();
	void Refresh();
	void Reset();
	void LockReset();
	void Push(const char* buf, DWORD len, bool no_crypt = false);
	void Push(const char* buf, const char* mask, DWORD len);
	void Pop(char* buf, DWORD len);
	void Pop(DWORD len);
	void GrowBuf(DWORD len);

	char* GetData(){return bufData;}
	char* GetCurData(){return bufData+bufReadPos;}
	DWORD GetLen(){return bufLen;}
	DWORD GetCurPos(){return bufReadPos;}
	void SetEndPos(DWORD pos){bufEndPos=pos;}
	DWORD GetEndPos()const{return bufEndPos;}
	void MoveReadPos(int val){bufReadPos+=val; EncryptKey(val);}
	bool IsError()const{return isError;}
	bool IsEmpty()const{return bufReadPos>=bufEndPos;}
	bool IsHaveSize(DWORD size)const{return bufReadPos+size<=bufEndPos;}

#if (defined(FONLINE_SERVER)) || (defined(FONLINE_CLIENT))
	bool NeedProcess();
	void SkipMsg(MSGTYPE msg);
	void SeekValidMsg();
#else
	bool NeedProcess() {return (bufReadPos<bufEndPos);}
#endif

	BufferManager& operator<<(DWORD i);
	BufferManager& operator>>(DWORD& i);
	BufferManager& operator<<(int i);
	BufferManager& operator>>(int& i);
	BufferManager& operator<<(WORD i);
	BufferManager& operator>>(WORD& i);
	BufferManager& operator<<(short i);
	BufferManager& operator>>(short& i);
	BufferManager& operator<<(BYTE i);
	BufferManager& operator>>(BYTE& i);
	BufferManager& operator<<(char i);
	BufferManager& operator>>(char& i);
	BufferManager& operator<<(bool i);
	BufferManager& operator>>(bool& i);

private:
	inline DWORD EncryptKey(int move)
	{
		if(!encryptActive) return 0;
		DWORD key=encryptKeys[encryptKeyPos];
		encryptKeyPos+=move;
		if(encryptKeyPos<0 || encryptKeyPos>=CRYPT_KEYS_COUNT)
		{
			encryptKeyPos%=CRYPT_KEYS_COUNT;
			if(encryptKeyPos<0) encryptKeyPos+=CRYPT_KEYS_COUNT;
		}
		return key;
	}
};

#endif // __BUFFER_MANAGER__