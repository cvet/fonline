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
	uint bufLen;
	uint bufEndPos;
	uint bufReadPos;
	bool encryptActive;
	int encryptKeyPos;
	uint encryptKeys[CRYPT_KEYS_COUNT];

	void CopyBuf(const char* from, char* to, const char* mask, uint crypt_key, uint len);
	bool IsValidMsg(uint msg);

public:
	BufferManager();
	BufferManager(uint alen);
	BufferManager& operator=(const BufferManager& r);
	~BufferManager();

	void SetEncryptKey(uint seed);
	void Lock();
	void Unlock();
	void Refresh();
	void Reset();
	void LockReset();
	void Push(const char* buf, uint len, bool no_crypt = false);
	void Push(const char* buf, const char* mask, uint len);
	void Pop(char* buf, uint len);
	void Cut(uint len);
	void GrowBuf(uint len);

	char* GetData(){return bufData;}
	char* GetCurData(){return bufData+bufReadPos;}
	uint GetLen(){return bufLen;}
	uint GetCurPos(){return bufReadPos;}
	void SetEndPos(uint pos){bufEndPos=pos;}
	uint GetEndPos()const{return bufEndPos;}
	void MoveReadPos(int val){bufReadPos+=val; EncryptKey(val);}
	bool IsError()const{return isError;}
	bool IsEmpty()const{return bufReadPos>=bufEndPos;}
	bool IsHaveSize(uint size)const{return bufReadPos+size<=bufEndPos;}

#if (defined(FONLINE_SERVER)) || (defined(FONLINE_CLIENT))
	bool NeedProcess();
	void SkipMsg(uint msg);
	void SeekValidMsg();
#else
	bool NeedProcess() {return (bufReadPos<bufEndPos);}
#endif

	BufferManager& operator<<(uint i);
	BufferManager& operator>>(uint& i);
	BufferManager& operator<<(int i);
	BufferManager& operator>>(int& i);
	BufferManager& operator<<(ushort i);
	BufferManager& operator>>(ushort& i);
	BufferManager& operator<<(short i);
	BufferManager& operator>>(short& i);
	BufferManager& operator<<(uchar i);
	BufferManager& operator>>(uchar& i);
	BufferManager& operator<<(char i);
	BufferManager& operator>>(char& i);
	BufferManager& operator<<(bool i);
	BufferManager& operator>>(bool& i);

private:
	inline uint EncryptKey(int move)
	{
		if(!encryptActive) return 0;
		uint key=encryptKeys[encryptKeyPos];
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