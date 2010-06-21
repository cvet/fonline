#include "stdafx.h"
#include "BufMngr.h"

CBufMngr::CBufMngr()
{
	len=2048;
	pos=0;
	read_pos=0;
	data=new char[len];
	memset(data,0,len);
	error=0;
}

CBufMngr::CBufMngr(DWORD alen)
{
	len=alen;
	pos=0;
	read_pos=0;
	data=new char[len];
	memset(data,0,len);
	error=0;
}

CBufMngr::~CBufMngr()
{
	SAFEDELA(data);
}

void CBufMngr::reset()
{
	pos=0;
	read_pos=0;
	memset(data,0,len);
}

void CBufMngr::grow_buf(DWORD alen)
{
	while(pos+alen>=len) len<<=1;
	char* newBuf=new char[len];
	memcpy(newBuf,data,pos);
	SAFEDELA(data);
	data=newBuf;
}

void CBufMngr::resize(DWORD new_size)
{
	SAFEDELA(data);
	pos = 0;
	read_pos = 0;
	len = 0;
	data = new char[new_size];
	if (!data)
	{
		WriteLog("ОШИБКА при выделении памяти буферу\n");
		return;
	}	
	len = new_size;
}

void CBufMngr::push(char *buf,DWORD alen)
{
	if(error) return;

	if(pos+alen>=len) grow_buf(alen);
	memcpy(data+pos,buf,alen);
	pos+=alen;
}

void CBufMngr::pop(char *buf,DWORD alen)
{
	if(error) return;

	if(read_pos+alen>pos) {error=1;return;}

	memcpy(buf,data+read_pos,alen);
	read_pos+=alen;
}


CBufMngr &CBufMngr::operator<<(int i)
{
	if(error) return *this;

	if(pos+4>=len) grow_buf(4);
	*(int*)(data+pos)=i;
	pos+=4;
	return *this;
}

CBufMngr &CBufMngr::operator>>(int &i)
{
	if(error) return *this;

//	#pragma chMSG("Необходимо позже сделать чтобы при недостатке данных он попытался их прочитать несколько раз с некоторым интервалом и только потом выдавать ошибку")
	if(read_pos+4>pos) {error=1;return *this;}
	i=*(int*)(data+read_pos);
	read_pos+=4;

	return *this;
}

CBufMngr &CBufMngr::operator<<(DWORD i)
{
	if(error) return *this;

	if(pos+4>=len) grow_buf(4);
	*(DWORD*)(data+pos)=i;
	pos+=4;
	return *this;
}

CBufMngr &CBufMngr::operator>>(DWORD &i)
{
	if(error) return *this;

	if(read_pos+4>pos) {error=1;return *this;}
	i=*(DWORD*)(data+read_pos);
	read_pos+=4;

	return *this;
}

CBufMngr &CBufMngr::operator<<(WORD i)
{
	if(error) return *this;

	if(pos+2>=len) grow_buf(2);
	*(WORD*)(data+pos)=i;
	pos+=2;
	return *this;
}

CBufMngr &CBufMngr::operator>>(WORD &i)
{
	if(error) return *this;

	if(read_pos+2>pos) {error=1;return *this;}
	i=*(WORD*)(data+read_pos);
	read_pos+=2;

	return *this;
}

CBufMngr &CBufMngr::operator<<(BYTE i)
{
	if(error) return *this;

	if(pos+1>=len) grow_buf(1);
	*(BYTE*)(data+pos)=i;
	pos+=1;
	return *this;
}

CBufMngr &CBufMngr::operator>>(BYTE &i)
{
	if(error) return *this;

	if(read_pos+1>pos) {error=1;return *this;}
	i=*(BYTE*)(data+read_pos);
	read_pos+=1;

	return *this;
}