#include "StdAfx.h"
#include "FileManager.h"

#define OUT_BUF_START_SIZE	 (0x100)

char PathLst[][50]=
{
	// Client and mapper paths
	"",
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
	"sound\\music\\",
	"sound\\sfx\\",
	"scripts\\",
	"video\\",
	"text\\",
	"save\\",
	"fonts\\",
	"cache\\",
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
	"save\\",
	"save\\clients\\",
	"save\\bans\\",
	"",
	"",
	"",
	"",

	// Other
	"data\\", // Mapper data
};

DataFileVec FileManager::dataFiles;
char FileManager::dataPath[MAX_FOPATH]={".\\"};

void FileManager::SetDataPath(const char* path)
{
	StringCopy(dataPath,path);
	if(dataPath[strlen(path)-1]!='\\') StringAppend(dataPath,"\\");
}

void FileManager::InitDataFiles(const char* path)
{
	char list_path[MAX_FOPATH];
	sprintf(list_path,"%sDataFiles.cfg",path);

	FileManager list;
	if(list.LoadFile(list_path,-1))
	{
		char line[1024];
		while(list.GetLine(line,1024))
		{
			// Cut off comments
			char* comment1=strstr(line,"#");
			char* comment2=strstr(line,";");
			if(comment1) *comment1=0;
			if(comment2) *comment2=0;

			// Skip white spaces and tabs
			char* str=line;
			while(*str && (*str==' ' || *str=='\t')) str++;
			if(!*str) continue;

			// Cut off white spaces and tabs in end of line
			char* str_=&str[strlen(str)-1];
			while(*str_==' ' || *str_=='\t') str_--;
			*(str_+1)=0;
			if(!*str) continue;

			// Try load
			char fpath[MAX_FOPATH];
			sprintf(fpath,"%s%s",path,str);
			if(!LoadDataFile(fpath))
			{
				char errmsg[1024];
				sprintf(errmsg,"Data file '%s' not found. Run Updater.exe.",str);
				HWND wnd=GetActiveWindow();
				if(wnd) MessageBox(wnd,errmsg,"FOnline",MB_OK);
				WriteLog(__FUNCTION__" - %s\n",errmsg);
			}
		}
	}
}

bool FileManager::LoadDataFile(const char* path)
{
	if(!path)
	{
		WriteLog(__FUNCTION__" - Path empty or nullptr.\n");
		return false;
	}

	// Extract full path
	char path_[MAX_FOPATH];
	StringCopy(path_,path);
	if(!GetFullPathName(path,MAX_FOPATH,path_,NULL))
	{
		WriteLog(__FUNCTION__" - Extract full path file<%s> fail.\n",path);
		return false;
	}

	// Find already loaded
	for(DataFileVecIt it=dataFiles.begin(),end=dataFiles.end();it!=end;++it)
	{
		DataFile* pfile=*it;
		if(pfile->GetPackName()==path_) return true;
	}

	// Add new
	DataFile* pfile=OpenDataFile(path_);
	if(!pfile)
	{
		WriteLog(__FUNCTION__" - Load packed file<%s> fail.\n",path_);
		return false;
	}

	dataFiles.insert(dataFiles.begin(),pfile);
	return true;
}

void FileManager::EndOfWork()
{
	for(DataFileVecIt it=dataFiles.begin(),end=dataFiles.end();it!=end;++it) delete *it;
	dataFiles.clear();
}

void FileManager::UnloadFile()
{
	SAFEDELA(fileBuf);
	fileSize=0;
	curPos=0;
}

BYTE* FileManager::ReleaseBuffer()
{
	BYTE* tmp=fileBuf;
	fileBuf=NULL;
	fileSize=0;
	curPos=0;
	return tmp;
}

bool FileManager::LoadFile(const char* fname, int path_type)
{
	UnloadFile();

	if(!fname || !fname[0]) return false;

	char folder_path[MAX_FOPATH]={0};
	char dat_path[MAX_FOPATH]={0};
	bool only_folder=false;

	if(path_type>=0 && path_type<PATH_LIST_COUNT)
	{
		StringCopy(dat_path,PathLst[path_type]);
		StringAppend(dat_path,fname);
		FormatPath(dat_path);
		StringCopy(folder_path,GetDataPath(path_type));
		StringAppend(folder_path,dat_path);

		// Erase '.\'
		while(folder_path[0]=='.' && folder_path[1]=='\\')
		{
			char* str=folder_path;
			char* str_=folder_path+2;
			for(;*str_;str++,str_++) *str=*str_;
			*str=0;
		}

		// Check for full path
		if(folder_path[1]==':') only_folder=true;
	}
	else if(path_type==-1)
	{
		StringCopy(folder_path,fname);
		only_folder=true;
	}
	else
	{
		WriteLog(__FUNCTION__" - Invalid path<%d>.\n",path_type);
		return false;
	}

	WIN32_FIND_DATA fd;
	HANDLE f=FindFirstFile(folder_path,&fd);
	if(f!=INVALID_HANDLE_VALUE)
	{
		FindClose(f);
		HANDLE h_file=CreateFile(folder_path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
		if(h_file!=INVALID_HANDLE_VALUE)
		{
			GetFileTime(h_file,&timeCreate,&timeAccess,&timeWrite);
			DWORD size=GetFileSize(h_file,NULL);
			BYTE* buf=new(nothrow) BYTE[size+1];
			if(!buf) return false;
			DWORD br;
			BOOL result=ReadFile(h_file,buf,size,&br,NULL);
			CloseHandle(h_file);
			if(!result || size!=br) return false;
			fileSize=size;
			fileBuf=buf;
			fileBuf[fileSize]=0;
			curPos=0;
			return true;
		}
	}

	if(only_folder) return false;

	_strlwr(dat_path);
	for(DataFileVecIt it=dataFiles.begin(),end=dataFiles.end();it!=end;++it)
	{
		DataFile* dat=*it;
		fileBuf=dat->OpenFile(dat_path,fileSize);
		if(fileBuf)
		{
			curPos=0;
			dat->GetTime(&timeCreate,&timeAccess,&timeWrite);
			return true;
		}
	}

	return false;
}

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

void FileManager::GoBack(DWORD offs)
{
	curPos-=offs;
}

bool FileManager::FindFragment(const BYTE* fragment, DWORD fragment_len, DWORD begin_offs)
{
	if(!fileBuf || fragment_len>fileSize) return false;

	for(DWORD i=begin_offs;i<fileSize-fragment_len;i++)
	{
		if(fileBuf[i]==fragment[0])
		{
			bool not_match=false;
			for(DWORD j=1;j<fragment_len;j++)
			{
				if(fileBuf[i+j]!=fragment[j])
				{
					not_match=true;
					break;
				}
			}

			if(!not_match)
			{
				curPos=i;
				return true;
			}
		}
	}
	return false;
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
	// Todo: rework
	while(curPos<fileSize)
	{
		if(fileBuf[curPos]>='0' && fileBuf[curPos]<='9') break;
		else curPos++;
	}

	if(curPos>=fileSize) return 0;
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

void FileManager::SwitchToRead()
{
	fileBuf=dataOutBuf;
	dataOutBuf=NULL;
	fileSize=endOutBuf;
	curPos=0;
	lenOutBuf=0;
	endOutBuf=0;
	posOutBuf=0;
}

void FileManager::SwitchToWrite()
{
	dataOutBuf=fileBuf;
	fileBuf=NULL;
	lenOutBuf=fileSize;
	endOutBuf=fileSize;
	posOutBuf=fileSize;
	fileSize=0;
	curPos=0;
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

	char fpath[MAX_FOPATH];
	if(path_type>=0 && path_type<PATH_LIST_COUNT)
	{
		StringCopy(fpath,GetDataPath(path_type));
		StringAppend(fpath,PathLst[path_type]);
		StringAppend(fpath,fname);
	}
	else if(path_type==-1)
	{
		StringCopy(fpath,fname);
	}
	else
	{
		WriteLog(__FUNCTION__" - Invalid path<%d>.\n",path_type);
		return false;
	}

	HANDLE h_file=CreateFile(fpath,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_FLAG_WRITE_THROUGH,NULL);
	if(h_file==INVALID_HANDLE_VALUE) return false;

	DWORD bw;
	if(!WriteFile(h_file,dataOutBuf,endOutBuf,&bw,NULL) || bw!=endOutBuf)
	{
		CloseHandle(h_file);
		h_file=NULL;
		DeleteFile(fpath);
		return false;
	}

	CloseHandle(h_file);
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
	static THREAD char buf[MAX_FOPATH];
	StringCopy(buf,GetDataPath(path_type));
	if(path_type>=0) StringAppend(buf,PathLst[path_type]);
	if(fname) StringAppend(buf,fname);
	FormatPath(buf);
	return buf;
}

void FileManager::GetFullPath(const char* fname, int path_type, char* get_path)
{
	if(!get_path) return;
	StringCopy(get_path,MAX_FOPATH,GetDataPath(path_type));
	if(path_type>=0) StringAppend(get_path,MAX_FOPATH,PathLst[path_type]);
	if(fname) StringAppend(get_path,MAX_FOPATH,fname);
	FormatPath(get_path);
}

const char* FileManager::GetPath(int path_type)
{
	static const char any[]="error";
	return (DWORD)path_type>=PATH_LIST_COUNT?any:PathLst[path_type];
}

const char* FileManager::GetDataPath(int path_type)
{
	static const char root_path[]=".\\";
	if(path_type==PT_ROOT || path_type==PT_SERVER_ROOT || path_type==PT_MAPPER_DATA) return root_path;
	return dataPath;
}

void FileManager::FormatPath(char* path)
{
	// Change '/' to '\'
	for(char* str=path;*str;str++) if(*str=='/') *str='\\';

	// Erase 'folder..\'
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
	bool dup=false;
	if(fname==path)
	{
		fname=StringDuplicate(fname);
		dup=true;
	}

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

	if(dup) delete[] fname;
}

void FileManager::ExtractFileName(const char* fname, char* name)
{
	bool dup=false;
	if(fname==name)
	{
		fname=StringDuplicate(fname);
		dup=true;
	}

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
		int len=0;
		for(;*str;len++,str++) name[len]=*str;
		name[len]=0;
	}
	else
	{
		name[0]=0;
	}

	if(dup) delete[] fname;
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
	if(h!=INVALID_HANDLE_VALUE) FindClose(h);
}

void FileManager::GetFolderFileNames(int path_type, const char* ext, StrVec& result)
{
	if(path_type<0 || path_type>=PATH_LIST_COUNT || path_type==PT_ROOT || path_type==PT_SERVER_ROOT) return;

	// Find in folder files
	RecursiveDirLook(PathLst[path_type],ext,result);
}

void FileManager::GetDatsFileNames(int path_type, const char* ext, StrVec& result)
{
	if(path_type<0 || path_type>=PATH_LIST_COUNT || path_type==PT_ROOT || path_type==PT_SERVER_ROOT) return;

	// Find in dat files
	for(DataFileVecIt it=dataFiles.begin(),end=dataFiles.end();it!=end;++it)
	{
		DataFile* dat=*it;
		dat->GetFileNames(PathLst[path_type],ext,result);
	}
}