#include "StdAfx.h"
#include "DataFile.h"
#include "DatFile/cfile.h"
#include "Zlib/unzip.h"

/************************************************************************/
/* Dat/Zip loaders                                                      */
/************************************************************************/

class FalloutDatFile : public DataFile
{
private:
	typedef map<string,BYTE*> IndexMap;
	typedef map<string,BYTE*>::iterator IndexMapIt;
	typedef map<string,BYTE*>::value_type IndexMapVal;

	IndexMap filesTree;
	string fileName;
	BYTE* memTree;
	HANDLE datHandle;

	FILETIME timeCreate,timeAccess,timeWrite;

	bool ReadTree();

public:
	bool Init(const char* fname);
	~FalloutDatFile();

	const string& GetPackName(){return fileName;}
	BYTE* OpenFile(const char* fname, DWORD& len);
	void GetFileNames(const char* path, const char* ext, StrVec& result);
	void GetTime(FILETIME* create, FILETIME* access, FILETIME* write);
};

class ZipFile : public DataFile
{
private:
	struct ZipFileInfo
	{
		unz_file_pos Pos;
		uLong UncompressedSize;
	};
	typedef map<string,ZipFileInfo> IndexMap;
	typedef map<string,ZipFileInfo>::iterator IndexMapIt;
	typedef map<string,ZipFileInfo>::value_type IndexMapVal;

	IndexMap filesTree;
	string fileName;
	unzFile zipHandle;

	FILETIME timeCreate,timeAccess,timeWrite;

	bool ReadTree();

public:
	bool Init(const char* fname);
	~ZipFile();

	const string& GetPackName(){return fileName;}
	BYTE* OpenFile(const char* fname, DWORD& len);
	void GetFileNames(const char* path, const char* ext, StrVec& result);
	void GetTime(FILETIME* create, FILETIME* access, FILETIME* write);
};

/************************************************************************/
/* Manage                                                               */
/************************************************************************/

DataFile* OpenDataFile(const char* fname)
{
	if(!fname || !fname[0])
	{
		WriteLog(__FUNCTION__" - Invalid file name, empty or nullptr.\n");
		return NULL;
	}

	const char* ext=strstr(fname,".");
	if(!ext)
	{
		WriteLog(__FUNCTION__" - File<%s> extension not found.\n",fname);
		return false;
	}

	const char* ext_=ext;
	while(ext_=strstr(ext_+1,".")) ext=ext_;

	if(!_stricmp(ext,".dat")) // Try open DAT
	{
		FalloutDatFile* dat=new(nothrow) FalloutDatFile();
		if(!dat || !dat->Init(fname))
		{
			WriteLog(__FUNCTION__" - Unable to open DAT file<%s>.\n",fname);
			if(dat) delete dat;
			return NULL;
		}
		return dat;
	}
	else if(!_stricmp(ext,".zip")) // Try open ZIP
	{
		ZipFile* zip=new(nothrow) ZipFile();
		if(!zip || !zip->Init(fname))
		{
			WriteLog(__FUNCTION__" - Unable to open ZIP file<%s>.\n",fname);
			if(zip) delete zip;
			return NULL;
		}
		return zip;
	}
	else // Bad file format
	{
		WriteLog(__FUNCTION__" - Invalid file format<%s>. Supported only DAT and ZIP.\n",fname);
	}

	return NULL;
}

/************************************************************************/
/* Dat file                                                             */
/************************************************************************/

bool FalloutDatFile::Init(const char* fname)
{
	datHandle=NULL;
	memTree=NULL;
	fileName=fname;

	datHandle=CreateFile(fname,GENERIC_READ,FILE_SHARE_READ,
		NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

	if(datHandle==INVALID_HANDLE_VALUE)
	{
		WriteLog(__FUNCTION__" - Cannot open file.\n");
		return false;
	}

	GetFileTime(datHandle,&timeCreate,&timeAccess,&timeWrite);

	if(!ReadTree())
	{
		WriteLog(__FUNCTION__" - Read file tree fail.\n");
		return false;
	}

	return true;
}

FalloutDatFile::~FalloutDatFile()
{
	if(datHandle!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(datHandle);
		datHandle=INVALID_HANDLE_VALUE;
	}
	SAFEDELA(memTree);
}

bool FalloutDatFile::ReadTree()
{
	DWORD dw,version;
	if(SetFilePointer(datHandle,-12,NULL,FILE_END)==INVALID_SET_FILE_POINTER) return false;
	if(!ReadFile(datHandle,&version,4,&dw,NULL)) return false;

	// DAT 2.1 Arcanum
	if(version==0x44415431) // 1TAD
	{
		// Readed data
		DWORD files_total,tree_size;

		// Read info
		if(SetFilePointer(datHandle,-4,NULL,FILE_END)==INVALID_SET_FILE_POINTER) return false;
		if(!ReadFile(datHandle,&tree_size,4,&dw,NULL)) return false;

		// Read tree
		if(SetFilePointer(datHandle,-(LONG)tree_size,NULL,FILE_END)==INVALID_SET_FILE_POINTER) return false;
		if(!ReadFile(datHandle,&files_total,4,&dw,NULL)) return false;
		tree_size-=28+4; // Subtract information block and files total
		if((memTree=new(nothrow) BYTE[tree_size])==NULL) return false;
		ZeroMemory(memTree,tree_size);
		if(!ReadFile(datHandle,memTree,tree_size,&dw,NULL)) return false;

		// Indexing tree
		char name[MAX_FOPATH];
		BYTE* ptr=memTree;
		BYTE* end_ptr=memTree+tree_size;
		while(true)
		{
			DWORD fnsz=*(DWORD*)ptr; // Include zero
			DWORD type=*(DWORD*)(ptr+4+fnsz+4);

			if(fnsz>1 && fnsz<MAX_FOPATH && type!=0x400) // Not folder
			{
				memcpy(name,ptr+4,fnsz);
				_strlwr_s(name);
				if(type==2) *(ptr+4+fnsz+7)=1; // Compressed
				filesTree.insert(IndexMapVal(name,ptr+4+fnsz+7));
			}

			if(ptr+fnsz+24>=end_ptr) break;
			ptr+=fnsz+24;
		}

		return true;
	}

	// DAT 2.0 Fallout2
	// Readed data
	DWORD dir_count,dat_size,files_total,tree_size;

	// Read info
	if(SetFilePointer(datHandle,-8,NULL,FILE_END)==INVALID_SET_FILE_POINTER) return false;
	if(!ReadFile(datHandle,&tree_size,4,&dw,NULL)) return false;
	if(!ReadFile(datHandle,&dat_size,4,&dw,NULL)) return false;

	// Check for DAT1.0 Fallout1 dat file
	if(SetFilePointer(datHandle,0,NULL,FILE_BEGIN)==INVALID_SET_FILE_POINTER) return false;
	if(!ReadFile(datHandle,&dir_count,4,&dw,NULL)) return false;
	dir_count>>=24;
	if(dir_count==0x01 || dir_count==0x33) return false;

	// Check for truncated
	if(GetFileSize(datHandle,NULL)!=dat_size) return false;

	// Read tree
	if(SetFilePointer(datHandle,-((LONG)tree_size+8),NULL,FILE_END)==INVALID_SET_FILE_POINTER) return false;
	if(!ReadFile(datHandle,&files_total,4,&dw,NULL)) return false;
	tree_size-=4;
	if((memTree=new(nothrow) BYTE[tree_size])==NULL) return false;
	ZeroMemory(memTree,tree_size);
	if(!ReadFile(datHandle,memTree,tree_size,&dw,NULL)) return false;

	// Indexing tree
	char name[MAX_FOPATH];
	BYTE* ptr=memTree;
	BYTE* end_ptr=memTree+tree_size;
	while(true)
	{
		DWORD fnsz=*(DWORD*)ptr;

		if(fnsz && fnsz+1<MAX_FOPATH)
		{
			memcpy(name,ptr+4,fnsz);
			name[fnsz]=0;
			_strlwr_s(name);
			filesTree.insert(IndexMapVal(name,ptr+4+fnsz));
		}

		if(ptr+fnsz+17>=end_ptr) break;
		ptr+=fnsz+17;
	}

	return true;
}

BYTE* FalloutDatFile::OpenFile(const char* fname, DWORD& len)
{
	if(datHandle==INVALID_HANDLE_VALUE) return NULL;

	IndexMapIt it=filesTree.find(fname);
	if(it==filesTree.end()) return NULL;

	BYTE* ptr=(*it).second;
	BYTE type=*ptr;
	DWORD real_size=*(DWORD*)(ptr+1);
	DWORD packed_size=*(DWORD*)(ptr+5);
	DWORD offset=*(DWORD*)(ptr+9);

	CFile* reader=NULL;
	if(!type) reader=new(nothrow) CPlainFile(datHandle,offset,real_size);
	else reader=new(nothrow) C_Z_PackedFile(datHandle,offset,real_size,packed_size);
	if(!reader) return NULL;

	BYTE* buf=new(nothrow) BYTE[real_size+1];
	if(!buf)
	{
		delete reader;
		return NULL;
	}

	if(real_size)
	{
		DWORD br;
		reader->read(buf,real_size,(long*)&br);
		delete reader;
		if(real_size!=br)
		{
			delete[] buf;
			return NULL;
		}
	}

	len=real_size;
	buf[len]=0;
	return buf;
}

void FalloutDatFile::GetFileNames(const char* path, const char* ext, StrVec& result)
{
	size_t path_len=strlen(path);
	for(IndexMapIt it=filesTree.begin(),end=filesTree.end();it!=end;++it)
	{
		const string& fname=(*it).first;
		if(!fname.compare(0,path_len,path))
		{
			if(ext && *ext)
			{
				size_t pos=fname.find_last_of('.');
				if(pos!=string::npos && !_stricmp(&fname.c_str()[pos+1],ext)) result.push_back(fname);
			}
			else
			{
				result.push_back(fname);
			}
		}
	}
}

void FalloutDatFile::GetTime(FILETIME* create, FILETIME* access, FILETIME* write)
{
	if(create) *create=timeCreate;
	if(access) *access=timeAccess;
	if(write) *write=timeWrite;
}

/************************************************************************/
/* Zip file                                                             */
/************************************************************************/

bool ZipFile::Init(const char* fname)
{
	fileName=fname;
	zipHandle=NULL;

	char path[MAX_FOPATH];
	if(GetFullPathName(fname,MAX_FOPATH,path,NULL)==0)
	{
		WriteLog(__FUNCTION__" - Can't retrieve file full path.\n");
		return false;
	}

	HANDLE fh=CreateFile(fname,GENERIC_READ,FILE_SHARE_READ,
		NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(fh==INVALID_HANDLE_VALUE)
	{
		WriteLog(__FUNCTION__" - Cannot open file.\n");
		return false;
	}
	GetFileTime(fh,&timeCreate,&timeAccess,&timeWrite);
	CloseHandle(fh);

	zipHandle=unzOpen(path);
	if(!zipHandle)
	{
		WriteLog(__FUNCTION__" - Cannot open file.\n");
		return false;
	}

	if(!ReadTree())
	{
		WriteLog(__FUNCTION__" - Read file tree fail.\n");
		return false;
	}

	return true;
}

ZipFile::~ZipFile()
{
	if(zipHandle)
	{
		unzClose(zipHandle);
		zipHandle=NULL;
	}
}

bool ZipFile::ReadTree()
{
	unz_global_info gi;
	if(unzGetGlobalInfo(zipHandle,&gi)!=UNZ_OK || gi.number_entry==0) return false;

	ZipFileInfo zip_info;
	unz_file_pos pos;
	unz_file_info info;
	char name[MAX_FOPATH]={0};
	for(uLong i=0;i<gi.number_entry;i++)
	{
		if(unzGetFilePos(zipHandle,&pos)!=UNZ_OK) return false;
		if(unzGetCurrentFileInfo(zipHandle,&info,name,MAX_FOPATH,NULL,0,NULL,0)!=UNZ_OK) return false;

		if(info.external_fa&0x20) // File == 0x20, Folder == 0x10
		{
			_strlwr_s(name);
			for(char* str=name;*str;str++) if(*str=='/') *str='\\';
			zip_info.Pos=pos;
			zip_info.UncompressedSize=info.uncompressed_size;
			filesTree.insert(IndexMapVal(name,zip_info));
		}

		if(i+1<gi.number_entry && unzGoToNextFile(zipHandle)!=UNZ_OK) return false;
	}

	return true;
}

BYTE* ZipFile::OpenFile(const char* fname, DWORD& len)
{
	if(!zipHandle) return NULL;

	IndexMapIt it=filesTree.find(fname);
	if(it==filesTree.end()) return NULL;

	ZipFileInfo& info=(*it).second;

	if(unzGoToFilePos(zipHandle,&info.Pos)!=UNZ_OK) return NULL;

	BYTE* buf=new(nothrow) BYTE[info.UncompressedSize+1];
	if(!buf) return NULL;

	if(unzOpenCurrentFile(zipHandle)!=UNZ_OK)
	{
		delete[] buf;
		return NULL;
	}

	int read=unzReadCurrentFile(zipHandle,buf,info.UncompressedSize);
	if(unzCloseCurrentFile(zipHandle)!=UNZ_OK || read!=info.UncompressedSize)
	{
		delete[] buf;
		return NULL;
	}

	len=info.UncompressedSize;
	buf[len]=0;
	return buf;
}

void ZipFile::GetFileNames(const char* path, const char* ext, StrVec& result)
{
	size_t path_len=strlen(path);
	for(IndexMapIt it=filesTree.begin(),end=filesTree.end();it!=end;++it)
	{
		const string& fname=(*it).first;
		if(!fname.compare(0,path_len,path))
		{
			if(ext && *ext)
			{
				size_t pos=fname.find_last_of('.');
				if(pos!=string::npos && !_stricmp(&fname.c_str()[pos+1],ext)) result.push_back(fname);
			}
			else
			{
				result.push_back(fname);
			}
		}
	}
}

void ZipFile::GetTime(FILETIME* create, FILETIME* access, FILETIME* write)
{
	if(create) *create=timeCreate;
	if(access) *access=timeAccess;
	if(write) *write=timeWrite;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/