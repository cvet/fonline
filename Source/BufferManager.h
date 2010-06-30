#ifndef __BUFFER_MANAGER__
#define __BUFFER_MANAGER__

class BufferManager
{
private:
	bool isError;
	CRITICAL_SECTION cs;
	char* bufData;
	DWORD bufLen;
	DWORD bufEndPos;
	DWORD bufReadPos;

public:
	BufferManager();
	BufferManager(DWORD alen);
	BufferManager& operator=(const BufferManager& r);
	~BufferManager();

	void Lock();
	void Unlock();
	void Refresh();
	void Reset();
	void LockReset();
	void Push(const char* buf, DWORD len);
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
	void MoveReadPos(int val){bufReadPos+=val;}
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

	BufferManager& operator<<(int i);
	BufferManager& operator>>(int &i);
	BufferManager& operator<<(DWORD i);
	BufferManager& operator>>(DWORD &i);
	BufferManager& operator<<(WORD i);
	BufferManager& operator>>(WORD &i);
	BufferManager& operator<<(short i);
	BufferManager& operator>>(short &i);
	BufferManager& operator<<(BYTE i);
	BufferManager& operator>>(BYTE &i);
	BufferManager& operator<<(char *i);
	BufferManager& operator>>(char *i);
	BufferManager& operator<<(bool i);
	BufferManager& operator>>(bool &i);
};

#endif // __BUFFER_MANAGER__