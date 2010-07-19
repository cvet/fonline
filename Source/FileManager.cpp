#include "StdAfx.h"
#include "FileManager.h"

#define OUT_BUF_START_SIZE	 (0x100)

char PathLst[][50]=
{
	// Client and mapper paths
	"",
	"art\\",
	"art\\critters\\",
	"art\\intrface\\",
	"art\\inven\\",
	"art\\items\\",
	"art\\misc\\",
	"art\\scenery\\",
	"art\\skilldex\\",
	"art\\splash\\",
	"art\\tiles\\",
	"art\\walls\\",
	"textures\\",
	"effects\\",
	"maps\\",
	"terrain\\",
	"sound\\music\\",
	"sound\\sfx\\",
	"scripts\\",
	"video\\",
	"text\\",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",

	// Server paths
	"",
	"data\\",
	"text\\",
	"dialogs\\",
	"maps\\",
	"proto\\items\\",
	"proto\\critters\\",
	"scripts\\",
};

TDatFilePtrVec FileManager::datFiles;
char FileManager::dataPath[MAX_FOPATH]={".\\"};
char FileManager::dataPathServer[MAX_FOPATH]={".\\"};


void FileManager::SetDataPath(const char* data_path, bool server_path)
{
	char* path=(server_path?dataPathServer:dataPath);
	if(data_path)
	{
		StringCopy(path,MAX_FOPATH,data_path);
		if(path[strlen(path)-1]!='\\') StringAppend(path,MAX_FOPATH,"\\");
	}
	else
	{
		StringCopy(path,MAX_FOPATH,".\\");
	}
}

bool FileManager::LoadDat(const char* path)
{
	if(!path)
	{
		WriteLog(__FUNCTION__" - Path nullptr.\n");
		return false;
	}

	// Find already loaded
	for(TDatFilePtrVecIt it=datFiles.begin(),end=datFiles.end();it!=end;++it)
	{
		TDatFile* dat=*it;
		if(dat->Datname==path) return true;
	}

	// Add new
	TDatFile* dat=new(nothrow) TDatFile(path);	
	if(!dat || dat->IsError)
	{
		WriteLog(__FUNCTION__" - Load dat file<%s> fail, error<%s>.\n",path,TDatFile::GetError(dat->ErrorType));
		if(dat) delete dat;
		return false;
	}

	datFiles.insert(datFiles.begin(),dat);
	return true;
}

void FileManager::EndOfWork()
{
	for(TDatFilePtrVecIt it=datFiles.begin(),end=datFiles.end();it!=end;++it) delete *it;
	datFiles.clear();
}

void FileManager::UnloadFile()
{
	SAFEDELA(fileBuf);
	fileSize=0;
	curPos=0;
}

void FileManager::LoadSimple(HANDLE h_file)
{
	GetFileTime(h_file,&timeCreate,&timeAccess,&timeWrite);
  	fileSize=GetFileSize(h_file,NULL);
	fileBuf=new(nothrow) BYTE[fileSize+1];
	if(!fileBuf) return;
	DWORD br;
	if(ReadFile(h_file,fileBuf,fileSize,&br,NULL)) CloseHandle(h_file);	
	fileBuf[fileSize]=0;
	curPos=0;
}

bool FileManager::LoadFile(const char* full_path)
{
	UnloadFile();

	WIN32_FIND_DATA fd;
	HANDLE f=FindFirstFile(full_path,&fd);
	if(f!=INVALID_HANDLE_VALUE)
	{
		HANDLE hFile=CreateFile(full_path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
		if(hFile!=INVALID_HANDLE_VALUE)
		{
			FindClose(f);
			LoadSimple(hFile);
			return true;
		}
	}
	FindClose(f);
	return false;
}

bool FileManager::LoadFile(const char* fname, int path_type)
{
	UnloadFile();

	if(!fname || !fname[0]) return false;

	char full_path[1024]="";
	char short_path[1024]="";

	if(path_type<0)
	{
		return LoadFile(fname);
	}
	else if(path_type<PATH_LIST_COUNT)
	{
		StringCopy(short_path,PathLst[path_type]);
		StringAppend(short_path,fname);
		FormatPath(short_path);
		StringCopy(full_path,GetDataPath(path_type));
		StringAppend(full_path,short_path);
	}
	else
	{
		return false;
	}

	WIN32_FIND_DATA fd;
	HANDLE f=FindFirstFile(full_path,&fd);
	if(f!=INVALID_HANDLE_VALUE)
	{
		HANDLE h_file=CreateFile(full_path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
		if(h_file!=INVALID_HANDLE_VALUE)
		{
			LoadSimple(h_file);
			return true;
		}
	}
	FindClose(f);

	_strlwr(short_path);
	for(TDatFilePtrVecIt it=datFiles.begin(),end=datFiles.end();it!=end;++it)
	{
		TDatFile* dat=*it;
		if(dat->DATOpenFile(short_path)!=INVALID_HANDLE_VALUE)
		{
			fileSize=dat->DATGetFileSize();
			fileBuf=new(nothrow) BYTE[fileSize+1];
			if(!fileBuf) return false;
			if(!dat->DATReadFile(fileBuf,fileSize)) return false;
			fileBuf[fileSize]=0;
			curPos=0;
			return true;
		}
	}

	return false;
}

/*
	if(datFoPatch && datFoPatch->DATOpenFile(short_path)!=INVALID_HANDLE_VALUE)
	{
		fileSize=datFoPatch->DATGetFileSize();
		fileBuf=new BYTE[fileSize+1];
		DWORD br;
		datFoPatch->DATReadFile(fileBuf,fileSize,&br);
		fileBuf[fileSize]=0;
		curPos=0;
		return true;
	}

	if(path_type==PT_ART_CRITTERS)
	{
		// Critter folder
		if(!datCritter)
		{
			//пробуем загрузить из critter_dat если это каталог
			StringCopy(full_path,pathCritterDat);
			StringAppend(full_path,short_path);

			HANDLE h_file=CreateFile(full_path,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
			if(h_file!=INVALID_HANDLE_VALUE)
			{
				LoadSimple(h_file);
				return true;
			}

			return false;
		}

		if(datCritter->DATOpenFile(short_path)!=INVALID_HANDLE_VALUE)
		{
			fileSize = datCritter->DATGetFileSize();
			fileBuf = new BYTE[fileSize+1];
			DWORD br;
			datCritter->DATReadFile(fileBuf,fileSize,&br);
			fileBuf[fileSize]=0;
			curPos=0;
			return true;
		}

		return false;
	}

	// Master folder
	if(!datMaster)
	{
		StringCopy(full_path,pathMasterDat);
		StringAppend(full_path,short_path);

		HANDLE h_file=CreateFile(full_path,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
		if(h_file!=INVALID_HANDLE_VALUE)
		{
			LoadSimple(h_file);
			return true;
		}

		return false;
	}

	// Master dat
	if(datMaster->DATOpenFile(short_path)!=INVALID_HANDLE_VALUE)
	{
		fileSize = datMaster->DATGetFileSize();
		fileBuf = new BYTE[fileSize+1];
		DWORD br;
		datMaster->DATReadFile(fileBuf,fileSize,&br);
		fileBuf[fileSize]=0;
		curPos=0;
		return true;
	}
	return false;
}*/

bool FileManager::LoadStream(BYTE* stream, DWORD length)
{
	UnloadFile();
	if(!length) return false;
	fileSize=length;
	fileBuf=new(nothrow) BYTE[fileSize+1];
	if(!fileBuf) return false;
	memcpy(fileBuf,stream,fileSize);
	fileBuf[fileSize]=0;
	curPos=0;
	return true;
}

void FileManager::SetCurPos(DWORD pos)
{
	curPos=pos;
}

void FileManager::GoForward(DWORD offs)
{
	curPos+=offs;
}

bool FileManager::GetLine(char* str, DWORD len)
{
	if(curPos>=fileSize) return false;

	int wpos=0;
	for(;wpos<len && curPos<fileSize;curPos++,wpos++)
	{
		if(fileBuf[curPos]=='\r' || fileBuf[curPos]=='\n' || fileBuf[curPos]=='#' || fileBuf[curPos]==';')
		{
			for(;curPos<fileSize;curPos++) if(fileBuf[curPos]=='\n') break;
			curPos++;
			break;
		}

		str[wpos]=fileBuf[curPos];
	}

	str[wpos]=0;
	if(!str[0]) return GetLine(str,len); // Skip empty line
	return true;
}

bool FileManager::CopyMem(void* ptr, size_t size)
{
	if(!size) return false;
	if(curPos+size>fileSize) return false;
	memcpy(ptr,fileBuf+curPos,size);
	curPos+=size;
	return true;
}

void FileManager::GetStr(char* str)
{
	if(!str || curPos+1>fileSize) return;
	DWORD len=1; // Zero terminated
	while(*(fileBuf+curPos+len-1)) len++;
	memcpy(str,fileBuf+curPos,len);
	curPos+=len;
}

BYTE FileManager::GetByte()
{
	if(curPos+sizeof(BYTE)>fileSize) return 0;
	BYTE res=0;
	res=fileBuf[curPos++];
	return res;
}

WORD FileManager::GetBEWord()
{
	if(curPos+sizeof(WORD)>fileSize) return 0;
	WORD res=0;
	BYTE* cres=(BYTE*)&res;
	cres[1]=fileBuf[curPos++];
	cres[0]=fileBuf[curPos++];
	return res;
}

WORD FileManager::GetLEWord()
{
	if(curPos+sizeof(WORD)>fileSize) return 0;
	WORD res=0;
	BYTE* cres=(BYTE*)&res;
	cres[0]=fileBuf[curPos++];
	cres[1]=fileBuf[curPos++];
	return res;
}

DWORD FileManager::GetBEDWord()
{
	if(curPos+sizeof(DWORD)>fileSize) return 0;
	DWORD res=0;
	BYTE* cres=(BYTE*)&res;
	for(int i=3;i>=0;i--) cres[i]=fileBuf[curPos++];
	return res;
}

DWORD FileManager::GetLEDWord()
{
	if(curPos+sizeof(DWORD)>fileSize) return 0;
	DWORD res=0;
	BYTE* cres=(BYTE*)&res;
	for(int i=0;i<=3;i++) cres[i]=fileBuf[curPos++];
	return res;
}

DWORD FileManager::GetLE3Bytes()
{
	if(curPos+sizeof(BYTE)*3>fileSize) return 0;
	DWORD res=0;
	BYTE* cres=(BYTE*)&res;
	for(int i=0;i<=2;i++) cres[i]=fileBuf[curPos++];
	return res;
}

float FileManager::GetBEFloat()
{
	if(curPos+sizeof(float)>fileSize) return 0.0f;
	float res;
	BYTE* cres=(BYTE*)&res;
	for(int i=3;i>=0;i--) cres[i]=fileBuf[curPos++];
	return res;
}

float FileManager::GetLEFloat()
{
	if(curPos+sizeof(float)>fileSize) return 0.0f;
	float res;
	BYTE* cres=(BYTE*)&res;
	for(int i=0;i<=3;i++) cres[i]=fileBuf[curPos++];
	return res;
}

int FileManager::GetNum()
{
	while(curPos<fileSize)
	{
		if(fileBuf[curPos]>='0' && fileBuf[curPos]<='9') break;
		else curPos++;
	}

	if(curPos>=fileSize) return 0; //TODO: send error
	int res=atoi((const char*)&fileBuf[curPos]);
	if(res/1000000000) curPos+=10;
	else if(res/100000000) curPos+=9;
	else if(res/10000000) curPos+=8;
	else if(res/1000000) curPos+=7;
	else if(res/100000) curPos+=6;
	else if(res/10000) curPos+=5;
	else if(res/1000) curPos+=4;
	else if(res/100) curPos+=3;
	else if(res/10) curPos+=2;
	else curPos++;
	return res;
}

void FileManager::ClearOutBuf()
{
	SAFEDELA(dataOutBuf);
	posOutBuf=0;
	lenOutBuf=0;
	endOutBuf=0;
}

bool FileManager::ResizeOutBuf()
{
	if(!lenOutBuf)
	{
		dataOutBuf=new(nothrow) BYTE[OUT_BUF_START_SIZE];
		if(!dataOutBuf) return false;
		lenOutBuf=OUT_BUF_START_SIZE;
		ZeroMemory((void*)dataOutBuf,lenOutBuf);
		return true;
	}

	BYTE* old_obuf=dataOutBuf;
	dataOutBuf=new(nothrow) BYTE[lenOutBuf*2];
	if(!dataOutBuf) return false;
	ZeroMemory((void*)dataOutBuf,lenOutBuf*2);
	memcpy((void*)dataOutBuf,(void*)old_obuf,lenOutBuf);
	SAFEDELA(old_obuf);
	lenOutBuf*=2;
	return true;
}

void FileManager::SetPosOutBuf(DWORD pos)
{
	if(pos>lenOutBuf)
	{
		if(ResizeOutBuf()) SetPosOutBuf(pos);
		return;
	}
	posOutBuf=pos;
}

bool FileManager::SaveOutBufToFile(const char* fname, int path_type)
{
	if(!dataOutBuf || !fname) return false;
	char fpath[2028];

	if(path_type<0)
		StringCopy(fpath,fname);
	else
	{
		StringCopy(fpath,GetDataPath(path_type));
		StringAppend(fpath,PathLst[path_type]);
		StringAppend(fpath,fname);
	}

	HANDLE h_file=CreateFile(fpath,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_FLAG_WRITE_THROUGH,NULL);
	if(h_file==INVALID_HANDLE_VALUE) return false;

	DWORD br;
	WriteFile(h_file,dataOutBuf,endOutBuf,&br,NULL);
	if(br!=endOutBuf)
	{
		CloseHandle(h_file);
		h_file=NULL;
		DeleteFile(fpath);
		return false;
	}

	CloseHandle(h_file);
	h_file=NULL;
	return true;
}

void FileManager::SetData(void* data, DWORD len)
{
	if(!len) return;
	if(posOutBuf+len>lenOutBuf)
	{
		if(ResizeOutBuf()) SetData(data,len);
		return;
	}

	memcpy(&dataOutBuf[posOutBuf],data,len);
	posOutBuf+=len;
	if(posOutBuf>endOutBuf) endOutBuf=posOutBuf;
}

void FileManager::SetStr(const char* fmt, ...)
{
	char str[2048];

	va_list list;
	va_start(list,fmt);
	vsprintf_s(str,fmt,list);
	va_end(list);

	SetData(str,strlen(str));
}

void FileManager::SetByte(BYTE data)
{
	if(posOutBuf+sizeof(BYTE)>lenOutBuf && !ResizeOutBuf()) return;
	dataOutBuf[posOutBuf++]=data;
	if(posOutBuf>endOutBuf) endOutBuf=posOutBuf;
}

void FileManager::SetBEWord(WORD data)
{
	if(posOutBuf+sizeof(WORD)>lenOutBuf && !ResizeOutBuf()) return;
	BYTE* pdata=(BYTE*)&data;
	dataOutBuf[posOutBuf++]=pdata[1];
	dataOutBuf[posOutBuf++]=pdata[0];
	if(posOutBuf>endOutBuf) endOutBuf=posOutBuf;
}

void FileManager::SetLEWord(WORD data)
{
	if(posOutBuf+sizeof(WORD)>lenOutBuf && !ResizeOutBuf()) return;
	BYTE* pdata=(BYTE*)&data;
	dataOutBuf[posOutBuf++]=pdata[0];
	dataOutBuf[posOutBuf++]=pdata[1];
	if(posOutBuf>endOutBuf) endOutBuf=posOutBuf;
}

void FileManager::SetBEDWord(DWORD data)
{
	if(posOutBuf+sizeof(DWORD)>lenOutBuf && !ResizeOutBuf()) return;
	BYTE* pdata=(BYTE*)&data;
	dataOutBuf[posOutBuf++]=pdata[3];
	dataOutBuf[posOutBuf++]=pdata[2];
	dataOutBuf[posOutBuf++]=pdata[1];
	dataOutBuf[posOutBuf++]=pdata[0];
	if(posOutBuf>endOutBuf) endOutBuf=posOutBuf;
}

void FileManager::SetLEDWord(DWORD data)
{
	if(posOutBuf+sizeof(DWORD)>lenOutBuf && !ResizeOutBuf()) return;
	BYTE* pdata=(BYTE*)&data;
	dataOutBuf[posOutBuf++]=pdata[0];
	dataOutBuf[posOutBuf++]=pdata[1];
	dataOutBuf[posOutBuf++]=pdata[2];
	dataOutBuf[posOutBuf++]=pdata[3];
	if(posOutBuf>endOutBuf) endOutBuf=posOutBuf;
}

const char* FileManager::GetFullPath(const char* fname, int path_type)
{
	static char buf[2048];
	StringCopy(buf,GetDataPath(path_type));
	if(path_type>=0) StringAppend(buf,PathLst[path_type]);
	if(fname) StringAppend(buf,fname);
	return buf;
}

void FileManager::GetFullPath(const char* fname, int path_type, char* get_path)
{
	if(!get_path) return;
	StringCopy(get_path,MAX_FOPATH,GetDataPath(path_type));
	if(path_type>=0) StringAppend(get_path,MAX_FOPATH,PathLst[path_type]);
	if(fname) StringAppend(get_path,MAX_FOPATH,fname);
}

const char* FileManager::GetPath(int path_type)
{
	static const char any[]="error";
	return (DWORD)path_type>=PATH_LIST_COUNT?any:PathLst[path_type];
}

void FileManager::FormatPath(char* path)
{
	char* str=strstr(path,"..\\");
	if(str)
	{
		char* str_=str+3;
		str-=2;
		if(str<=path) return;
		for(;str!=path;str--) if(*str=='\\') break;
		str++;
		for(;*str_;str++,str_++) *str=*str_;
		*str=0;
		FormatPath(path);
	}
}

void FileManager::ExtractPath(const char* fname, char* path)
{
	const char* str=strstr(fname,"\\");
	if(str)
	{
		str++;
		while(true)
		{
			const char* str_=strstr(str,"\\");
			if(str_) str=str_+1;
			else break;
		}
		size_t len=str-fname;
		if(len) memcpy(path,fname,len);
		path[len]=0;
	}
	else
	{
		path[0]=0;
	}
}

const char* FileManager::GetExtension(const char* fname)
{
	if(!fname) return NULL;
	const char* last_dot=NULL;
	for(;*fname;fname++) if(*fname=='.') last_dot=fname;
	if(!last_dot) return NULL;
	last_dot++;
	if(!*last_dot) return NULL;
	return last_dot;
}

int FileManager::ParseLinesInt(const char* fname, int path_type, IntVec& lines)
{
	lines.clear();
	if(!LoadFile(fname,path_type)) return -1;

	char cur_line[129];
	while(GetLine(cur_line,128)) lines.push_back(atoi(cur_line));
	UnloadFile();
	return lines.size();
}

void FileManager::GetTime(FILETIME* create, FILETIME* access, FILETIME* write)
{
	if(create) *create=timeCreate;
	if(access) *access=timeAccess;
	if(write) *write=timeWrite;
}

void FileManager::RecursiveDirLook(const char* init_dir, const char* ext, StrVec& result)
{
	WIN32_FIND_DATA fd;
	char buf[MAX_FOPATH];
	sprintf(buf,"%s%s*",dataPath,init_dir);
	HANDLE h=FindFirstFile(buf,&fd);
	while(h!=INVALID_HANDLE_VALUE)
	{
		if(fd.cFileName[0]!='.')
		{
			if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
			{
				sprintf(buf,"%s%s\\",init_dir,fd.cFileName);
				RecursiveDirLook(buf,ext,result);
			}
			else
			{
				if(ext)
				{
					const char* ext_=GetExtension(fd.cFileName);
					if(ext_ && !_stricmp(ext,ext_))
					{
						StringCopy(buf,init_dir);
						StringAppend(buf,fd.cFileName);
						result.push_back(buf);
					}
				}
				else
				{
					StringCopy(buf,init_dir);
					StringAppend(buf,fd.cFileName);
					result.push_back(buf);
				}
			}
		}

		if(!FindNextFile(h,&fd)) break;
	}
	CloseHandle(h);
}

void FileManager::GetFolderFileNames(int path_type, const char* ext, StrVec& result)
{
	if(path_type<0 || path_type>=PATH_LIST_COUNT) return;

	// Find in folder files
	char path[MAX_FOPATH];
	StringCopy(path,dataPath);
	StringAppend(path,PathLst[path_type]);
	RecursiveDirLook(PathLst[path_type],ext,result);
}

void FileManager::GetDatsFileNames(int path_type, const char* ext, StrVec& result)
{
	if(path_type<0 || path_type>=PATH_LIST_COUNT) return;

	// Find in dat files
	for(TDatFilePtrVecIt it=datFiles.begin(),end=datFiles.end();it!=end;++it)
	{
		TDatFile* dat=*it;
		dat->GetFileNames(PathLst[path_type],ext,result);
	}
}