//---------------------------------------------------------------------------


#pragma hdrstop

#include "Common.h"
#define Byte ByteZlib
#include "..\Source\zlib\zlib.h"

//---------------------------------------------------------------------------

#pragma package(smart_init)

// Crc32 table
unsigned int CrcTable[0x100];

void InitCrc32()
{
	// Create crc32 mass
	const unsigned int CRC_POLY=0xEDB88320;
	unsigned int i,j,r;
	for(i=0;i<0x100;i++)
	{
		for(r=i,j=0x8;j;j--)
			r=(r&1?(r>>1)^CRC_POLY:r>>1);
		CrcTable[i]=r;
	}
}

unsigned int Crc32(unsigned char* data, unsigned int len)
{
	const unsigned int CRC_MASK=0xD202EF8D;
	unsigned int reslt=0;
	while(len--)
	{
		reslt=CrcTable[(unsigned char)reslt^*data++]^reslt>>8;
		reslt^=CRC_MASK;
	}
	return reslt;
}

bool Compress(TMemoryStream* stream)
{
	unsigned long bufLen=stream->Size*110/100+12;
	unsigned char* buf=new unsigned char[bufLen];
	if(compress(buf,&bufLen,(unsigned char*)stream->Memory,stream->Size)!=Z_OK)
	{
		delete[] buf;
		return false;
	}
	stream->SetSize((int)bufLen);
	CopyMemory(stream->Memory,buf,bufLen);
	delete[] buf;
	return true;
}

bool Uncompress(TMemoryStream* stream)
{
	unsigned long bufLen=stream->Size*2;
	unsigned char* buf=new unsigned char[bufLen];
	while(true)
	{
		int result=uncompress(buf,&bufLen,(unsigned char*)stream->Memory,stream->Size);
		if(result==Z_BUF_ERROR)
		{
			delete[] buf;
			bufLen*=2;
			buf=new unsigned char[bufLen];
		}
		else if(result!=Z_OK)
		{
			delete[] buf;
			return false;
		}
		else
		{
			stream->SetSize((int)bufLen);
			CopyMemory(stream->Memory,buf,bufLen);
			delete[] buf;
			return true;
		}
	}
}






