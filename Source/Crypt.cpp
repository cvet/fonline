#include "StdAfx.h"
#include "Crypt.h"
#include "Zlib/zlib.h"

CryptManager Crypt;

CryptManager::CryptManager()
{
	// Create crc32 mass
	const DWORD CRC_POLY=0xEDB88320;
	DWORD i,j,r;
	for(i=0;i<0x100;i++)
	{
		for(r=i,j=0x8;j;j--)
			r=((r&1)?(r>>1)^CRC_POLY:r>>1);
		crcTable[i]=r;
	}
}

DWORD CryptManager::Crc32(BYTE* data, DWORD len)
{
	const DWORD CRC_MASK=0xD202EF8D;
	tmpValue=0;
	while(len--)
	{
		tmpValue=crcTable[(BYTE)tmpValue^*data++]^tmpValue>>8;
		tmpValue^=CRC_MASK;
	}
	return tmpValue;
}

void CryptManager::Crc32(BYTE* data, DWORD len, DWORD& crc)
{
	const DWORD CRC_MASK=0xD202EF8D;
	while(len--)
	{
		crc=crcTable[(BYTE)crc^*data++]^crc>>8;
		crc^=CRC_MASK;
	}
}

DWORD CryptManager::CheckSum(BYTE* data, DWORD len)
{
	tmpValue=0;
	while(len--) tmpValue+=*data++;
	return tmpValue;
}

void CryptManager::XOR(char* data, DWORD len, char* xor, DWORD xor_len)
{
	for(DWORD i=0,k=0;i<len;++i,++k)
	{
		if(k>=xor_len) k=0;
		data[i]^=xor[k];
	}
}

void CryptManager::TextXOR(char* data, DWORD len, char* xor, DWORD xor_len)
{
	char cur_char;
	for(DWORD i=0,k=0;i<len;++i,++k)
	{
		if(k>=xor_len) k=0;

		cur_char=(data[i]-(i+5)*(i*5))^xor[k];

#define TRUECHAR(a) \
	(((a)>=32 && (a)<=126) ||\
	((a)>=-64 && (a)<=-1))	

	//	WriteLog("%c",cur_char);

		if(TRUECHAR(cur_char)) data[i]=cur_char;
	//	if(cur_char) data[i]=cur_char;
	}
}

BYTE* CryptManager::Compress(const BYTE* data, DWORD& data_len)
{
	DWORD buf_len=data_len*110/100+12;
	AutoPtrArr<BYTE> buf(new(nothrow) BYTE[buf_len]);
	if(!buf.IsValid()) return NULL;

	if(compress(buf.Get(),&buf_len,data,data_len)!=Z_OK) return NULL;
	XOR((char*)buf.Get(),buf_len,(char*)&crcTable[1],sizeof(crcTable)-4);
	XOR((char*)buf.Get(),4,(char*)buf.Get()+4,4);

	data_len=buf_len;
	return buf.Release();
}

BYTE* CryptManager::Uncompress(const BYTE* data, DWORD& data_len, DWORD mul_approx)
{
	DWORD buf_len=data_len*mul_approx;
	if(buf_len>100000000) // 100mb
	{
		WriteLog("Unpack - Buffer length is too large, data length<%u>, multiplier<%u>.\n",data_len,mul_approx);
		return NULL;
	}

	AutoPtrArr<BYTE> buf(new(nothrow) BYTE[buf_len]);
	if(!buf.IsValid())
	{
		WriteLog("Unpack - Bad alloc, size<%u>.\n",buf_len);
		return NULL;
	}

	AutoPtrArr<BYTE> data_(new(nothrow) BYTE[data_len]);
	if(!data_.IsValid())
	{
		WriteLog("Unpack - Bad alloc, size<%u>.\n",data_len);
		return NULL;
	}

	memcpy(data_.Get(),data,data_len);
	if(*(WORD*)data_.Get()!=0x9C78)
	{
		XOR((char*)data_.Get(),4,(char*)data_.Get()+4,4);
		XOR((char*)data_.Get(),data_len,(char*)&crcTable[1],sizeof(crcTable)-4);
	}

	if(*(WORD*)data_.Get()!=0x9C78)
	{
		WriteLog("Unpack - Signature not found.\n");
		return NULL;
	}

	while(true)
	{
		int result=uncompress(buf.Get(),&buf_len,data_.Get(),data_len);
		if(result==Z_BUF_ERROR)
		{
			buf_len*=2;
			buf.Reset(new(nothrow) BYTE[buf_len]);
			if(!buf.IsValid())
			{
				WriteLog("Unpack - Bad alloc, size<%u>.\n",buf_len);
				return NULL;
			}
		}
		else if(result!=Z_OK)
		{
			WriteLog("Unpack error<%d>.\n",result);
			return NULL;
		}
		else break;
	}

	data_len=buf_len;
	return buf.Release();
}

/*void CryptManager::CryptText(char* text)
{
	DWORD len=strlen(text);
	char* buf=(char*)Compress((BYTE*)text,len);
	if(!buf) len=0;
	/ *for(int i=0;i<len;i++)
	{
		char& c=buf[i];
		if(c==) c=;
		text[i]=c;
	}* /
	text[len]=0;
	if(buf) delete[] buf;
}

void CryptManager::UncryptText(char* text)
{
	DWORD len=strlen(text);
	char* buf=(char*)Uncompress((BYTE*)text,len,2);
	if(buf) memcpy(text,buf,len);
	else len=0;
	text[len]=0;
	if(buf) delete[] buf;
}*/

#define MAX_CACHE_DESCRIPTORS       (10000)
#define CACHE_DATA_VALID            (0x08)
#define CACHE_SIZE_VALID            (0x10)

struct CacheDescriptor 
{
	DWORD Rnd2[2];
	BYTE Flags;
	char Rnd0;
	WORD Rnd1;
	DWORD DataOffset;
	DWORD DataCurLen;
	DWORD DataMaxLen;
	DWORD Rnd3;
	DWORD DataCrc;
	DWORD Rnd4;
	char DataName[32];
	DWORD TableSize;
	DWORD XorKey[5];
	DWORD Crc;
} CacheTable[MAX_CACHE_DESCRIPTORS];
string CacheTableName;

bool CryptManager::IsCacheTable(const char* chache_fname)
{
	if(!chache_fname || !chache_fname[0]) return false;
	FILE* f=NULL;
	if(fopen_s(&f,chache_fname,"rb")) return false;
	fclose(f);
	return true;
}

bool CryptManager::CreateCacheTable(const char* chache_fname)
{
	FILE* f=NULL;
	if(fopen_s(&f,chache_fname,"wb")) return false;

	for(int i=0;i<sizeof(CacheTable);i++) ((BYTE*)&CacheTable[0])[i]=rand();
	for(int i=0;i<MAX_CACHE_DESCRIPTORS;i++)
	{
		CacheDescriptor& desc=CacheTable[i];
		UNSETFLAG(desc.Flags,CACHE_DATA_VALID);
		UNSETFLAG(desc.Flags,CACHE_SIZE_VALID);
		desc.TableSize=MAX_CACHE_DESCRIPTORS;
		CacheDescriptor desc_=desc;
		XOR((char*)&desc_,sizeof(CacheDescriptor)-24,(char*)&desc_.XorKey[0],20);
		fwrite((void*)&desc_,sizeof(BYTE),sizeof(CacheDescriptor),f);
	}

	fclose(f);
	CacheTableName=chache_fname;
	return true;
}

bool CryptManager::SetCacheTable(const char* chache_fname)
{
	if(!chache_fname || !chache_fname[0]) return false;

	FILE* f=NULL;
	if(fopen_s(&f,chache_fname,"rb"))
	{
		if(CacheTableName.length()) // default.cache
		{
			FILE* fr=NULL;
			if(fopen_s(&fr,CacheTableName.c_str(),"rb")) return CreateCacheTable(chache_fname);

			FILE* fw=NULL;;
			if(fopen_s(&fw,chache_fname,"wb"))
			{
				fclose(fr);
				return CreateCacheTable(chache_fname);
			}

			CacheTableName=chache_fname;
			fseek(fr,0,SEEK_END);
			DWORD len=ftell(fr);
			fseek(fr,0,SEEK_SET);
			BYTE* buf=new(nothrow) BYTE[len];
			if(!buf) return false;
			fread(buf,sizeof(BYTE),len,fr);
			fwrite(buf,sizeof(BYTE),len,fw);
			delete[] buf;
			fclose(fr);
			fclose(fw);
			return true;
		}
		else
		{
			return CreateCacheTable(chache_fname);
		}
	}

	fseek(f,0,SEEK_END);
	DWORD len=ftell(f);
	fseek(f,0,SEEK_SET);

	if(len<sizeof(CacheTable))
	{
		fclose(f);
		return CreateCacheTable(chache_fname);
	}

	fread((void*)&CacheTable[0],sizeof(BYTE),sizeof(CacheTable),f);
	fclose(f);
	CacheTableName=chache_fname;
	for(int i=0;i<MAX_CACHE_DESCRIPTORS;i++)
	{
		CacheDescriptor& desc=CacheTable[i];
		XOR((char*)&desc,sizeof(CacheDescriptor)-24,(char*)&desc.XorKey[0],20);
		if(desc.TableSize!=MAX_CACHE_DESCRIPTORS) return CreateCacheTable(chache_fname);
	}
	return true;
}

void CryptManager::SetCache(const char* data_name, const BYTE* data, DWORD data_len)
{
	FILE* f=NULL;
	if(fopen_s(&f,CacheTableName.c_str(),"r+b")) return;

	CacheDescriptor desc_;
	int desc_place=-1;

	// In prev place
	for(int i=0;i<MAX_CACHE_DESCRIPTORS;i++)
	{
		CacheDescriptor& desc=CacheTable[i];
		if(!FLAG(desc.Flags,CACHE_DATA_VALID)) continue;
		if(strcmp(data_name,desc.DataName)) continue;

		if(data_len>desc.DataMaxLen)
		{
			UNSETFLAG(desc.Flags,CACHE_DATA_VALID);
			CacheDescriptor desc__=desc;
			XOR((char*)&desc__,sizeof(CacheDescriptor)-24,(char*)&desc__.XorKey[0],20);
			fseek(f,i*sizeof(CacheDescriptor),SEEK_SET);
			fwrite((void*)&desc__,sizeof(BYTE),sizeof(CacheDescriptor),f);
			break;
		}

		desc.DataCurLen=data_len;
		desc_=desc;
		desc_place=i;
		goto label_PlaceFound;
	}

	// Valid size
	for(int i=0;i<MAX_CACHE_DESCRIPTORS;i++)
	{
		CacheDescriptor& desc=CacheTable[i];
		if(FLAG(desc.Flags,CACHE_DATA_VALID)) continue;
		if(!FLAG(desc.Flags,CACHE_SIZE_VALID)) continue;
		if(data_len>desc.DataMaxLen) continue;

		SETFLAG(desc.Flags,CACHE_DATA_VALID);
		desc.DataCurLen=data_len;
		StringCopy(desc.DataName,data_name);
		desc_=desc;
		desc_place=i;
		goto label_PlaceFound;
	}

	// Grow
	for(int i=0;i<MAX_CACHE_DESCRIPTORS;i++)
	{
		CacheDescriptor& desc=CacheTable[i];
		if(FLAG(desc.Flags,CACHE_DATA_VALID)) continue;
		if(FLAG(desc.Flags,CACHE_SIZE_VALID)) continue;

		fseek(f,0,SEEK_END);
		int offset=ftell(f)-sizeof(CacheTable);
		if(offset<0)
		{
			fclose(f);
			return;
		}

		DWORD max_len=data_len*2;
 		if(max_len<128)
 		{
 			max_len=128;
 			fwrite((void*)&crcTable[1],sizeof(BYTE),max_len,f);
 		}
 		else
		{
			fwrite(data,sizeof(BYTE),data_len,f);
			fwrite(data,sizeof(BYTE),data_len,f);
		}

		SETFLAG(desc.Flags,CACHE_DATA_VALID);
		SETFLAG(desc.Flags,CACHE_SIZE_VALID);
		desc.DataOffset=offset;
		desc.DataCurLen=data_len;
		desc.DataMaxLen=max_len;
		StringCopy(desc.DataName,data_name);
		desc_=desc;
		desc_place=i;
		goto label_PlaceFound;
	}

	fclose(f);
	return;
label_PlaceFound:

	CacheDescriptor desc__=desc_;
	XOR((char*)&desc__,sizeof(CacheDescriptor)-24,(char*)&desc__.XorKey[0],20);
	fseek(f,desc_place*sizeof(CacheDescriptor),SEEK_SET);
	fwrite((void*)&desc__,sizeof(BYTE),sizeof(CacheDescriptor),f);
	fseek(f,sizeof(CacheTable)+desc_.DataOffset,SEEK_SET);
	fwrite(data,sizeof(BYTE),data_len,f);
	fclose(f);
}

BYTE* CryptManager::GetCache(const char* data_name, DWORD& data_len)
{
	for(int i=0;i<MAX_CACHE_DESCRIPTORS;i++)
	{
		CacheDescriptor& desc=CacheTable[i];
		if(!FLAG(desc.Flags,CACHE_DATA_VALID)) continue;
		if(strcmp(data_name,desc.DataName)) continue;

		FILE* f=NULL;
		if(fopen_s(&f,CacheTableName.c_str(),"rb")) return NULL;

		if(desc.DataCurLen>0xFFFFFF)
		{
			fclose(f);
			UNSETFLAG(desc.Flags,CACHE_DATA_VALID);
			UNSETFLAG(desc.Flags,CACHE_SIZE_VALID);
			return NULL;
		}

		fseek(f,0,SEEK_END);
		DWORD file_len=ftell(f)+1;
		fseek(f,0,SEEK_SET);

		if(file_len<sizeof(CacheTable)+desc.DataOffset+desc.DataCurLen)
		{
			fclose(f);
			UNSETFLAG(desc.Flags,CACHE_DATA_VALID);
			UNSETFLAG(desc.Flags,CACHE_SIZE_VALID);
			return NULL;
		}

		BYTE* data=new(nothrow) BYTE[desc.DataCurLen];
		if(!data)
		{
			fclose(f);
			return NULL;
		}

		data_len=desc.DataCurLen;
		fseek(f,sizeof(CacheTable)+desc.DataOffset,SEEK_SET);
		fread(data,sizeof(BYTE),desc.DataCurLen,f);
		fclose(f);
		return data;
	}
	return NULL;
}