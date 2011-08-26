//---------------------------------------------------------------------------

#pragma hdrstop

#include "Common.h"
#define Byte ByteZlib
#include "..\Source\zlib\zlib.h"

bool IsEnglish = false;

//---------------------------------------------------------------------------

#pragma package(smart_init)

// Crc32 table
int CrcTable[0x100];

void InitCrc32()
{
	// Create crc32 mass
	const unsigned int CRC_POLY = 0xEDB88320;
	unsigned int i, j, r;
	for(i = 0; i < 0x100; i++)
	{
		for(r = i, j = 0x8; j; j--)
			r = (r & 1 ? (r >> 1) ^ CRC_POLY : r >> 1);
		CrcTable[i] = r;
	}
}

int Crc32(unsigned char* data, unsigned int len)
{
	const int CRC_MASK = 0xD202EF8D;
	unsigned int result = 0;

	while(len--)
	{
		result = CrcTable[(unsigned char)result ^ *data++] ^ result >> 8;
		result ^= CRC_MASK;
	}

	return (int)result;
}

int Crc32(TStream* stream)
{
	const int CRC_MASK = 0xD202EF8D;
	unsigned int len = (unsigned int)stream->Size;
	unsigned int result = 0;

	static const int bigBufSize = 0x100000;
	static unsigned char* bigBuf = NULL;
	if(!bigBuf) bigBuf = new unsigned char[bigBufSize];
	int bufPos = bigBufSize;
	
	while(len--)
	{
		if(bufPos == bigBufSize)
		{
			stream->Read(bigBuf, bigBufSize);
			bufPos = 0;
		}

		result = CrcTable[(unsigned char)result ^ bigBuf[bufPos]] ^ result >> 8;
		result ^= CRC_MASK;
		bufPos++;
	}

	return (int)result;
}

bool Compress(TMemoryStream* stream)
{
	unsigned long bufLen = stream->Size * 110 / 100 + 12;
	unsigned char* buf = new unsigned char[bufLen];
	if(compress(buf, &bufLen, (unsigned char*)stream->Memory, stream->Size) != Z_OK)
	{
		delete[] buf;
		return false;
	}
	stream->SetSize((int)bufLen);
	CopyMemory(stream->Memory, buf, bufLen);
	delete[] buf;
	return true;
}

bool Uncompress(TMemoryStream* stream)
{
	unsigned long bufLen = stream->Size * 2;
	unsigned char* buf = new unsigned char[bufLen];
	while(true)
	{
		int result = uncompress(buf, &bufLen,(unsigned char*)stream->Memory, stream->Size);
		if(result == Z_BUF_ERROR)
		{
			delete[] buf;
			bufLen *= 2;
			buf = new unsigned char[bufLen];
		}
		else if(result != Z_OK)
		{
			delete[] buf;
			return false;
		}
		else
		{
			stream->SetSize((int)bufLen);
			CopyMemory(stream->Memory, buf, bufLen);
			delete[] buf;
			return true;
		}
	}
}

//---------------------------------------------------------------------------
AnsiString CfgFileName;
AnsiString CfgAppName;

void SetConfigName(AnsiString cfgName, AnsiString appName)
{
	CfgFileName = cfgName;
	if(cfgName.Length() > 2 && cfgName[2] != ':' && cfgName[2] != '\\')
    	CfgFileName = AnsiString(".\\") + CfgFileName;
	CfgAppName = appName;
}

AnsiString GetString(AnsiString key, AnsiString defVal)
{
	if(CfgFileName.Length() == 0) return defVal;
	char buf[2048];
	GetPrivateProfileString(CfgAppName.c_str(), key.c_str(), defVal.c_str(), buf, 2048, CfgFileName.c_str());
	return AnsiString(buf);
}

int GetInt(AnsiString key, int defVal)
{
	if(CfgFileName.Length() == 0) return defVal;
	return GetPrivateProfileInt(CfgAppName.c_str(), key.c_str(), defVal, CfgFileName.c_str());
}

void SetString(AnsiString key, AnsiString val)
{
	if(CfgFileName.Length() == 0) return;
	WritePrivateProfileString(CfgAppName.c_str(), key.c_str(), val.c_str(), CfgFileName.c_str());
}

void SetInt(AnsiString key, int val)
{
	if(CfgFileName.Length() == 0) return;
	char buf[32];
	itoa(val, buf, 10);
	WritePrivateProfileString(CfgAppName.c_str(), key.c_str(), buf, CfgFileName.c_str());
}

//---------------------------------------------------------------------------

void ShowMessageOK(AnsiString msg)
{
	static AnsiString caption;
	if(caption == "") caption = GetString("GameName", "FOnline") + " Launcher";
	MessageBox(GetActiveWindow(), msg.c_str(), caption.c_str(), MB_OK);
}

void RestoreMainDirectory()
{
	// Get executable file path
	char path[2048] = {0};
	GetModuleFileName(GetModuleHandle(NULL), path, 2048);

	// Cut off executable name
	int last = 0;
	for(int i = 0; path[i]; i++)
		if(path[i] == '\\') last = i;
	path[last + 1] = 0;

	// Set executable directory
	SetCurrentDirectory(path);
}

void CheckDirectory(AnsiString fileName)
{
	char buf[2048];
	char* part = NULL;
	if(GetFullPathName(fileName.c_str(), 2048, buf, &part) != 0)
	{
		if(part) *part = 0;
		if(!CreateDirectory(buf, NULL))
		{
			// Create all folders tree
			for(int i = 0, j = strlen(buf); i < j; i++)
			{
				if(buf[i] == '\\')
				{
					char c = buf[i + 1];
					buf[i + 1] = 0;
					CreateDirectory(buf, NULL);
					buf[i + 1] = c;
				}
			}
		}
	}
}

AnsiString FormatSize(int size)
{
	float sizeF = (float)size / 1024.0f / 1024.0f;
	if(sizeF < 0.1f) sizeF = 0.1f;
	AnsiString sizeStr;
	sizeStr.sprintf("%.1f ", sizeF);
	sizeStr += LANG("Ìב", "Mb");
	return sizeStr;
}

char* EraseFrontBackSpecificChars(char* str)
{
	char* front = str;
	while(*front && (*front == ' ' || *front == '\t' || *front == '\n' || *front == '\r')) front++;
	if(front != str)
	{
		char* str_ = str;
		while(*front) *str_++ = *front++;
		*str_ = 0;
	}

	char* back = str;
	while(*back) back++;
	back--;
	while(back >= str && (*back == ' ' || *back == '\t' || *back == '\n' || *back == '\r')) back--;
	*(back + 1) = 0;

	return str;
}

bool SpecialCompare(const AnsiString& astrL, const AnsiString& astrR)
{
	int l = 0;
	int r = 0;
	const char* strL = astrL.c_str();
	const char* strR = astrR.c_str();
	int lLen = astrL.Length();
	int rLen = astrR.Length();

	while(r < rLen && l < lLen)
	{
		char lC = tolower(strL[l]);
		char rC = tolower(strR[r]);

		if(rC == '*')
		{
			// Skip some letters
			for(r++; r < rLen; r++)
				if(strR[r] != '*' && strR[r] != '?') break;
			if(r >= rLen) return true;
			rC = strR[r];

			for(l++; l < lLen; l++)
				if(strL[l] == rC) break;
			if(l >= lLen) return false;
		}
		else if(rC == '?')
		{
			// Skip one letter
		}
		else if(lC != rC)
		{
			// Different symbols
			return false;
		}

		l++;
		r++;
	}

	if(l < lLen || r < rLen) return false;
	return true;
}
//---------------------------------------------------------------------------
