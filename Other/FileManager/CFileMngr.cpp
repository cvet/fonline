#include "stdafx.h"

/********************************************************************
author:	Oleg Mareskin
edit: Anton Tsvetinsky aka Cvet
*********************************************************************/

#include "CFileMngr.h"

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-//

char pathlst[][50]=
{
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

	"maps\\",

	"proto\\items\\",
	"proto\\misc\\",
	"proto\\scenery\\",
	"proto\\tiles\\",
	"proto\\walls\\",

	"sound\\music\\",
	"sound\\sfx\\",

	"text\\english\\game\\",

	"maps\\", //!Cvet три серверных путя
	"maps\\wm_mask\\",

	"crit_proto\\"			// !Deniska прототипы критов.
};

#ifdef FM_USE_DAT_MANAGER //!Cvet
int CFileMngr::Init(char* data_path, char* master_path, char* critter_path) //!Cvet
#else
int CFileMngr::Init(char* data_path)
#endif //FM_USE_DAT_MANAGER

{
	//WriteLog("CFileMngr Initialization...\n");

#ifdef FM_USE_DAT_MANAGER //!Cvet

	master_dat[0]=0;

	if(!master_path[0])
	{
		//WriteLog("CFileMngr Init - Не найден файл %s или в нем нет раздела master_dat",CFG_FILE);
		return 0;
	}

	if(master_path[0]=='.')
		strcat(master_dat,".");
	else if(master_path[1]!=':')
		strcat(master_dat,"..\\");

	strcat(master_dat,master_path);
	if(strstr(master_dat,".dat"))
	{
		lpDAT=new TDatFile(master_dat);	
		if(lpDAT->ErrorType==ERR_CANNOT_OPEN_FILE)
		{
			//WriteLog("CFileMngr Init> файл %s не найден",master_dat);
			return 0;
		}
	}
	else
	{
		if(master_dat[strlen(master_dat)-1]!='\\') strcat(master_dat,"\\");
	}

	crit_dat[0]=0;

	if(!critter_path[0])
	{
		//WriteLog("CFileMngr Init - Не найден файл %s или в нем нет раздела critter_dat",CFG_FILE);
		return 0;
	}

	if(critter_path[0]=='.')
		strcat(crit_dat,".");
	else if(critter_path[1]!=':')
		strcat(crit_dat,"..\\");

	strcat(crit_dat,critter_path);
	if(strstr(crit_dat,".dat"))
	{
		lpDATcr=new TDatFile(crit_dat);		
		if(lpDATcr->ErrorType==ERR_CANNOT_OPEN_FILE)
		{
			//WriteLog("CFileMngr Init> файл %s не найден",crit_dat);
			return 0;
		}
	}
	else
	{
		if(crit_dat[strlen(crit_dat)-1]!='\\') strcat(crit_dat,"\\");
	}
#endif

	if(!data_path)
	{
		//WriteLog("CFileMngr Init - Не указан путь к data, выставлено имя по умолчанию: \".\"data\"\"\n");
		strcpy(fo_dat,".\\data\\");
	}
	else
	{
		strcpy(fo_dat,data_path);
		if(fo_dat[strlen(fo_dat)-1]!='\\') strcat(fo_dat,"\\");
	}

	//WriteLog("CFileMngr Initialization complete\n");
	crtd=1;
	return 1;
}

void CFileMngr::Clear()
{
	//WriteLog("CFileMngr Clear...\n");
	UnloadFile();

	ClearOutBuf(); //!Cvet

#ifdef FM_USE_DAT_MANAGER //!Cvet
	SAFEDEL(lpDAT);
	SAFEDEL(lpDATcr);
#endif

	//WriteLog("CFileMngr Clear complete\n");
}

void CFileMngr::UnloadFile()
{
	SAFEDELA(buf);
}

void CFileMngr::LoadSimple(HANDLE hFile)
{
  	fsize = GetFileSize(hFile,NULL);
	buf = new BYTE[fsize+1];
	DWORD br;
	ReadFile(hFile,buf,fsize,&br,NULL);
	CloseHandle(hFile);	

	buf[fsize]=0;

	cur_pos=0;
}

int CFileMngr::LoadFile(char* fname, int PathType)
{
	if(!crtd)
	{
		//WriteLog("FileMngr LoadFile - FileMngr не был иницилазирован до загрузки файла %s",fname);
		return 0;
	}
	UnloadFile();

	char path[1024]="";
	char pfname[1024]="";

	if(PathType==-1)
		strcpy(pfname,fname);
	else
	{
		strcpy(pfname,pathlst[PathType]);
		strcat(pfname,fname);
	}

	strcpy(path,fo_dat);
	strcat(path,pfname);

	//данные fo
	WIN32_FIND_DATA fd;
	HANDLE f=FindFirstFile(path,&fd);
	if(f!=INVALID_HANDLE_VALUE)
	{
		HANDLE hFile=CreateFile(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
		if(hFile!=INVALID_HANDLE_VALUE)
		{
			LoadSimple(hFile);
			return 1;
		}
	}
	FindClose(f);

#ifdef FM_USE_DAT_MANAGER //!Cvet
	if(PathType==PT_ART_CRITTERS)
	{
		if(!lpDATcr)
		{
			//пробуем загрузить из critter_dat если это каталог
			strcpy(path,crit_dat);
			strcat(path,pfname);

			HANDLE hFile=CreateFile(path,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
			if(hFile!=INVALID_HANDLE_VALUE)
			{
				LoadSimple(hFile);
				return 1;
			}
			else return 0; //а вот не вышло
		}	

		if(lpDATcr->DATOpenFile(pfname)!=INVALID_HANDLE_VALUE)
		{

			fsize = lpDATcr->DATGetFileSize();			

			buf = new BYTE[fsize+1];
			DWORD br;

			lpDATcr->DATReadFile(buf,fsize,&br);

			buf[fsize]=0;

			cur_pos=0;
			return 1;
		}
	}
		
	if(!lpDAT)
	{
		//попрбуем загрузить из master_dat если это каталог
		strcpy(path,master_dat);
		strcat(path,pfname);

		HANDLE hFile=CreateFile(path,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
		if(hFile!=INVALID_HANDLE_VALUE)
		{
			LoadSimple(hFile);
			return 1;
		}
		else return 0; //а вот не вышло
	}

	if(lpDAT->DATOpenFile(pfname)!=INVALID_HANDLE_VALUE)
	{
		fsize = lpDAT->DATGetFileSize();

		buf = new BYTE[fsize+1];
		DWORD br;

		lpDAT->DATReadFile(buf,fsize,&br);

		buf[fsize]=0;

		cur_pos=0;
		return 1;
	}
#endif
	//WriteLog("FileMngr - Файл %s не найден\n",pfname);//!Cvet
	return 0;
}

void CFileMngr::SetCurPos(DWORD pos)
{
	if(pos<fsize) cur_pos=pos;
}

void CFileMngr::GoForward(DWORD offs)
{
	if((cur_pos+offs)<fsize) cur_pos+=offs;
}


int CFileMngr::GetStr(char* str,DWORD len)
{
	if(cur_pos>=fsize) return 0;

	int rpos=cur_pos;
	int wpos=0;
	for(;wpos<len && rpos<fsize;rpos++)
	{
		if(buf[rpos]==0xD)
		{
			rpos+=2;
			break;
		}

		str[wpos++]=buf[rpos];
	}
	str[wpos]=0;
	cur_pos=rpos;

	return 1;
}

int CFileMngr::CopyMem(void* ptr, size_t size)
{
	if(cur_pos+size>fsize) return 0;

	memcpy(ptr,buf+cur_pos,size);

	cur_pos+=size;

	return 1;
}

BYTE CFileMngr::GetByte() //!Cvet
{
	if(cur_pos>=fsize) return 0;
	BYTE res=0;

	res=buf[cur_pos++];

	return res;
}

WORD CFileMngr::GetBEWord()
{
	if(cur_pos>=fsize) return 0;
	WORD res=0;

	BYTE *cres=(BYTE*)&res;
	cres[1]=buf[cur_pos++];
	cres[0]=buf[cur_pos++];

	return res;
}

WORD CFileMngr::GetLEWord() //!Cvet
{
	if(cur_pos>=fsize) return 0;
	WORD res=0;

	BYTE *cres=(BYTE*)&res;
	cres[0]=buf[cur_pos++];
	cres[1]=buf[cur_pos++];

	return res;
}

DWORD CFileMngr::GetBEDWord()
{
	if(cur_pos>=fsize) return 0;
	DWORD res=0;
	BYTE *cres=(BYTE*)&res;
	for(int i=3;i>=0;i--)
	{
		cres[i]=buf[cur_pos++];
	}

	return res;
}

DWORD CFileMngr::GetLEDWord() //!Cvet
{
	if(cur_pos>=fsize) return 0;
	DWORD res=0;
	BYTE *cres=(BYTE*)&res;
	for(int i=0;i<=3;i++)
	{
		cres[i]=buf[cur_pos++];
	}

	return res;
}

//!Cvet +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void CFileMngr::ClearOutBuf()
{
	SAFEDELA(obuf);
	pos_obuf=0;
	len_obuf=0;
	end_obuf=0;
}

void CFileMngr::ResizeOutBuf()
{
	if(!len_obuf)
	{
		obuf=new BYTE[OUT_BUF_START_SIZE];
		len_obuf=OUT_BUF_START_SIZE;

		ZeroMemory((void*)obuf,len_obuf);

		return;
	}

	BYTE* old_obuf=obuf;

	obuf=new BYTE[len_obuf*2];
	ZeroMemory((void*)obuf,len_obuf*2);

	memcpy((void*)obuf,(void*)old_obuf,len_obuf);

	SAFEDELA(old_obuf);

	len_obuf*=2;
}

void CFileMngr::SetPosOutBuf(UINT pos)
{
	if(pos>=len_obuf)
	{
		ResizeOutBuf();
		SetPosOutBuf(pos);
		return;
	}

	pos_obuf=pos;
}

BOOL CFileMngr::SaveOutBufToFile(char* fname, int PathType)
{
	if(!obuf) return FALSE;

	char fpath[1024];
	strcpy(fpath,fo_dat);
	strcat(fpath,pathlst[PathType]);
	strcat(fpath,fname);

	HANDLE hOutFile=CreateFile(&fpath[0],GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_FLAG_WRITE_THROUGH,NULL);
	
	if(hOutFile==INVALID_HANDLE_VALUE) return FALSE;

	DWORD br;
	WriteFile(hOutFile,obuf,end_obuf,&br,NULL);

	if(br!=end_obuf)
	{
		CloseHandle(hOutFile);
		hOutFile=NULL;
		DeleteFile(fpath);
		return FALSE;
	}

	CloseHandle(hOutFile);
	hOutFile=NULL;

	return TRUE;
}	

void CFileMngr::SetByte(BYTE data)
{
	if(pos_obuf+sizeof(BYTE)>=len_obuf) ResizeOutBuf();
	
	obuf[pos_obuf++]=data;

	if(pos_obuf>end_obuf) end_obuf=pos_obuf;
}

void CFileMngr::SetBEWord(WORD data)
{
	if(pos_obuf+sizeof(WORD)>=len_obuf) ResizeOutBuf();

	BYTE* pdata=(BYTE*)&data;

	obuf[pos_obuf++]=pdata[1];
	obuf[pos_obuf++]=pdata[0];

	if(pos_obuf>end_obuf) end_obuf=pos_obuf;
}

void CFileMngr::SetLEWord(WORD data)
{
	if(pos_obuf+sizeof(WORD)>=len_obuf) ResizeOutBuf();
	
	BYTE* pdata=(BYTE*)&data;

	obuf[pos_obuf++]=pdata[0];
	obuf[pos_obuf++]=pdata[1];

	if(pos_obuf>end_obuf) end_obuf=pos_obuf;
}

void CFileMngr::SetBEDWord(DWORD data)
{
	if(pos_obuf+sizeof(DWORD)>=len_obuf) ResizeOutBuf();
	
	BYTE* pdata=(BYTE*)&data;
	
	obuf[pos_obuf++]=pdata[3];
	obuf[pos_obuf++]=pdata[2];
	obuf[pos_obuf++]=pdata[1];
	obuf[pos_obuf++]=pdata[0];

	if(pos_obuf>end_obuf) end_obuf=pos_obuf;
}

void CFileMngr::SetLEDWord(DWORD data)
{
	if(pos_obuf+sizeof(DWORD)>=len_obuf) ResizeOutBuf();
	
	BYTE* pdata=(BYTE*)&data;
	
	obuf[pos_obuf++]=pdata[0];
	obuf[pos_obuf++]=pdata[1];
	obuf[pos_obuf++]=pdata[2];
	obuf[pos_obuf++]=pdata[3];

	if(pos_obuf>end_obuf) end_obuf=pos_obuf;
}

void CFileMngr::SetData(void* data, UINT len)
{
	if(pos_obuf+len>=len_obuf)
	{
		ResizeOutBuf();
		SetData(data,len);
		return;
	}

	memcpy(&obuf[pos_obuf],data,len);

	pos_obuf+=len;
	if(pos_obuf>end_obuf) end_obuf=pos_obuf;
}

BOOL CFileMngr::GetFullPath(char* fname, int PathType, char* get_path)
{
	get_path[0]=0;
	strcpy(get_path,fo_dat);
	strcat(get_path,pathlst[PathType]);
	strcat(get_path,fname);
	
	WIN32_FIND_DATA fd;
	HANDLE f=FindFirstFile(get_path,&fd);
	
	if(f!=INVALID_HANDLE_VALUE) return TRUE;
	
//	WriteLog("Файл:|%s|\n",get_path);
	
	return FALSE;
}

void CFileMngr::GetFullPath(int PathType, char* get_path)
{
	if(!get_path) return;

	get_path[0]=0;
	strcpy(get_path,fo_dat);
	strcat(get_path,pathlst[PathType]);
}

const char* CFileMngr::GetPath(int PathType)
{
	return pathlst[PathType];
}

int CFileMngr::ParseLinesInt(char* fname, int PathType, vector<int>& lines)
{
	lines.clear();

	if(!LoadFile(fname,PathType)) return -1;

	char cur_line[128];
	
	while(GetStr(cur_line,128))
		lines.push_back(vector<int>::value_type(atoi(cur_line)));

	UnloadFile();

	return lines.size();
}
//!Cvet ----------------------------------------------------------------------