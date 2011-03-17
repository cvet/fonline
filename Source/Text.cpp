#include "StdAfx.h"
#include "Text.h"
#include "FileManager.h"

char TextMsgFileName[TEXTMSG_COUNT][16]=
{
	{"FOTEXT.MSG"},
	{"FODLG.MSG"},
	{"FOOBJ.MSG"},
	{"FOGAME.MSG"},
	{"FOGM.MSG"},
	{"FOCOMBAT.MSG"},
	{"FOQUEST.MSG"},
	{"FOHOLO.MSG"},
	{"FOCRAFT.MSG"},
	{"FOINTERNAL.MSG"},
};

/************************************************************************/
/* Format string                                                        */
/************************************************************************/

void Str::ChangeValue(char* str, int value)
{
	for(int i=0;str[i];++i) str[i]+=value;
}

void Str::EraseInterval(char* str, int len)
{
	if(!str || len<=0) return;

	char* str2=str+len;
	while(*str2)
	{
		*str=*str2;
		++str;
		++str2;
	}

	*str=0;
}

void Str::Insert(char* to, const char* from)
{
	if(!to || !from) return;

	int flen=strlen(from);
	if(!flen) return;

	char* end_to=to;
	while(*end_to) ++end_to;

	for(;end_to>=to;--end_to) *(end_to+flen)=*end_to;

	while(*from)
	{
		*to=*from;
		++to;
		++from;
	}
}

void Str::Copy(char* to, const char* from)
{
	if(!to || !from) return;

	while(*from)
	{
		*to=*from;
		++to;
		++from;
	}
}

void Str::EraseWords(char* str, char begin, char end)
{
	if(!str) return;

	for(int i=0;str[i];++i)
	{
		if(str[i]==begin)
		{
			for(int k=i+1;;++k)
			{
				if(!str[k] || str[k]==end)
				{
					Str::EraseInterval(&str[i],k-i+1);
					break;
				}
			}
			i--;
		}
	}
}

void Str::EraseChars(char* str, char ch)
{
	if(!str) return;

	while(*str)
	{
		if(*str==ch) CopyBack(str);
		else ++str;
	}
}

void Str::CopyWord(char* to, const char* from, char end, bool include_end /* = false */)
{
	if(!from || !to) return;

	*to=0;

	while(*from && *from!=end)
	{
		*to=*from;
		to++;
		from++;
	}

	if(include_end && *from)
	{
		*to=*from;
		to++;
	}

	*to=0;
}

char* Str::Lwr(char* str)
{
	return _strlwr(str);
}

char* Str::Upr(char* str)
{
	return _strupr(str);
}

void Str::CopyBack(char* str)
{
	while(*str)
	{
		*str=*(str+1);
		str++;
	}
}

void Str::Replacement(char* str, char ch)
{
	while(*str)
	{
		if(*str==ch) CopyBack(str);
		else ++str;
	}
}

void Str::Replacement(char* str, char from, char to)
{
	while(*str)
	{
		if(*str==from) *str=to;
		++str;
	}
}

void Str::Replacement(char* str, char from1, char from2, char to)
{
	while(*str && *(str+1))
	{
		if(*str==from1 && *(str+1)==from2)
		{
			CopyBack(str);
			*str=to;
		}
		++str;
	}
}

void Str::SkipLine(char*& str)
{
	while(*str && *str!='\n') ++str;
	if(*str) ++str;
}

void Str::GoTo(char*& str, char ch, bool skip_char /* = false */)
{
	while(*str && *str!=ch) ++str;
	if(skip_char && *str) ++str;
}

const char* Str::ItoA(int i)
{
	static THREAD char str[128];
	sprintf(str,"%d",i);
	return str;
}

const char* Str::DWtoA(DWORD dw)
{
	static THREAD char str[128];
	sprintf(str,"%u",dw);
	return str;
}

const char* Str::Format(const char* fmt, ...)
{
	static THREAD char res[0x4000];
	va_list list;
	va_start(list,fmt);
	vsprintf(res,fmt,list);
	va_end(list);
	return res;
}

const char* Str::SFormat(char* stream, const char* fmt, ...)
{
	va_list list;
	va_start(list,fmt);
	vsprintf(stream,fmt,list);
	va_end(list);
	return stream;
}

char* Str::EraseFrontBackSpecificChars(char* str)
{
	char* front=str;
	while(*front && (*front==' ' || *front=='\t' || *front=='\n' || *front=='\r')) front++;
	if(front!=str)
	{
		char* str_=str;
		while(*front) *str_++=*front++;
		*str_=0;
	}

	char* back=str;
	while(*back) back++;
	back--;
	while(back>=str && (*back==' ' || *back=='\t' || *back=='\n' || *back=='\r')) back--;
	*(back+1)=0;

	return str;
}

const char* Str::ParseLineDummy(const char* str)
{
	return str;
}

int Str::GetMsgType(const char* type_name)
{
	if(!_stricmp(type_name,"text")) return TEXTMSG_TEXT;
	else if(!_stricmp(type_name,"dlg")) return TEXTMSG_DLG;
	else if(!_stricmp(type_name,"item")) return TEXTMSG_ITEM;
	else if(!_stricmp(type_name,"obj")) return TEXTMSG_ITEM;
	else if(!_stricmp(type_name,"game")) return TEXTMSG_GAME;
	else if(!_stricmp(type_name,"gm")) return TEXTMSG_GM;
	else if(!_stricmp(type_name,"combat")) return TEXTMSG_COMBAT;
	else if(!_stricmp(type_name,"quest")) return TEXTMSG_QUEST;
	else if(!_stricmp(type_name,"holo")) return TEXTMSG_HOLO;
	else if(!_stricmp(type_name,"craft")) return TEXTMSG_CRAFT;
	else if(!_stricmp(type_name,"fix")) return TEXTMSG_CRAFT;
	else if(!_stricmp(type_name,"internal")) return TEXTMSG_INTERNAL;
	return -1;
}

static char BigBuf[0x100000]; // 1 mb
char* Str::GetBigBuf()
{
	return BigBuf;
}

bool Str::IsNumber(const char* str)
{
	// Check number it or not
	bool is_number=true;
	size_t pos=0;
	size_t len=strlen(str);
	for(size_t i=0,j=len;i<j;i++,pos++) if(str[i]!=' ' && str[i]!='\t') break;
	if(pos>=len) is_number=false; // Empty string
	for(size_t i=pos,j=len;i<j;i++,pos++)
	{
		if(!((str[i]>='0' && str[i]<='9') || (i==0 && str[i]=='-')))
		{
			is_number=false; // Found not a number
			break;
		}
	}
	if(is_number && str[pos-1]=='-') return false;
	return is_number;
}

bool Str::StrToInt(const char* str, int& i)
{
	if(IsNumber(str))
	{
		i=atoi(str);
		return true;
	}
	i=0;
	return false;
}

DwordStrMap NamesHash;
DWORD Str::GetHash(const char* str)
{
	if(!str) return 0;

	char str_[MAX_FOPATH];
	StringCopy(str_,str);
	Str::Lwr(str_);
	DWORD len=0;
	for(char* s=str_;*s;s++,len++) if(*s=='/') *s='\\';

	EraseFrontBackSpecificChars(str_);

	return Crypt.Crc32((BYTE*)str_,len);
}

const char* Str::GetName(DWORD hash)
{
	if(!hash) return NULL;

	DwordStrMapIt it=NamesHash.find(hash);
	return it!=NamesHash.end()?(*it).second.c_str():NULL;
}

void Str::AddNameHash(const char* name)
{
	DWORD hash=GetHash(name);

	DwordStrMapIt it=NamesHash.find(hash);
	if(it==NamesHash.end())
		NamesHash.insert(DwordStrMapVal(hash,name));
	else if(_stricmp(name,(*it).second.c_str()))
		WriteLog(__FUNCTION__" - Found equal hash for different names, name1<%s>, name2<%s>, hash<%u>.\n",name,(*it).second.c_str(),hash);
}

/************************************************************************/
/* Ini parser                                                           */
/************************************************************************/
#define STR_PRIVATE_APP_BEGIN		'['
#define STR_PRIVATE_APP_END			']'
#define STR_PRIVATE_KEY_CHAR		'='

IniParser::IniParser()
{
	bufPtr=NULL;
	bufLen=0;
	lastApp[0]=0;
	lastAppPos=0;
}

IniParser::~IniParser()
{
	UnloadFile();
}

bool IniParser::IsLoaded()
{
	return bufPtr!=NULL;
}

bool IniParser::LoadFile(const char* fname, int path_type)
{
	UnloadFile();

	FileManager fm;
	if(!fm.LoadFile(fname,path_type)) return false;

	bufLen=fm.GetFsize();
	bufPtr=(char*)fm.ReleaseBuffer();
	return true;
}

bool IniParser::LoadFilePtr(const char* buf, DWORD len)
{
	UnloadFile();

	bufPtr=new char[len+1];
	if(!bufPtr) return false;

	memcpy(bufPtr,buf,len);
	bufPtr[len]=0;
	bufLen=len;
	return true;
}

bool IniParser::AppendToBegin(const char* fname, int path_type)
{
	FileManager fm;
	if(!fm.LoadFile(fname,path_type)) return false;
	DWORD len=fm.GetFsize();
	char* buf=(char*)fm.ReleaseBuffer();

	char* grow_buf=new char[bufLen+len+1];
	memcpy(grow_buf,buf,len);
	memcpy(grow_buf+len,bufPtr,bufLen);
	grow_buf[bufLen+len]=0;

	SAFEDELA(bufPtr);
	bufPtr=grow_buf;
	bufLen+=len;
	lastApp[0]=0;
	lastAppPos=0;
	return true;
}

bool IniParser::AppendToEnd(const char* fname, int path_type)
{
	FileManager fm;
	if(!fm.LoadFile(fname,path_type)) return false;
	DWORD len=fm.GetFsize();
	char* buf=(char*)fm.ReleaseBuffer();

	char* grow_buf=new char[bufLen+len+1];
	memcpy(grow_buf,bufPtr,bufLen);
	memcpy(grow_buf+bufLen,buf,len);
	grow_buf[bufLen+len]=0;

	SAFEDELA(bufPtr);
	bufPtr=grow_buf;
	bufLen+=len;
	lastApp[0]=0;
	lastAppPos=0;
	return true;
}

bool IniParser::AppendPtrToBegin(const char* buf, DWORD len)
{
	char* grow_buf=new char[bufLen+len+1];
	memcpy(grow_buf,buf,len);
	memcpy(grow_buf+len,bufPtr,bufLen);
	grow_buf[bufLen+len]=0;

	SAFEDELA(bufPtr);
	bufPtr=grow_buf;
	bufLen+=len;
	lastApp[0]=0;
	lastAppPos=0;
	return true;
}

void IniParser::UnloadFile()
{
	SAFEDELA(bufPtr);
	bufLen=0;
	lastApp[0]=0;
	lastAppPos=0;
}

char* IniParser::GetBuffer()
{
	return bufPtr;
}

void IniParser::GotoEol(DWORD& iter)
{
	for(;iter<bufLen;++iter)
		if(bufPtr[iter]=='\n') return;
}

bool IniParser::GotoApp(const char* app_name, DWORD& iter)
{
	DWORD j;
	for(;iter<bufLen;++iter)
	{
		// Skip white spaces and tabs
		while(iter<bufLen && (bufPtr[iter]==' ' || bufPtr[iter]=='\t')) ++iter;
		if(iter>=bufLen) break;

		if(bufPtr[iter]==STR_PRIVATE_APP_BEGIN)
		{
			++iter;
			for(j=0;app_name[j];++iter,++j)
			{
				if(iter>=bufLen) return false;
				if(app_name[j]!=bufPtr[iter]) goto label_NextLine;
			}

			if(bufPtr[iter]!=STR_PRIVATE_APP_END) goto label_NextLine;

			GotoEol(iter);
			++iter;

			StringCopy(lastApp,app_name);
			lastAppPos=iter;
			return true;
		}

label_NextLine:
		GotoEol(iter);
	}
	return false;
}

bool IniParser::GotoKey(const char* key_name, DWORD& iter)
{
	DWORD j;
	for(;iter<bufLen;++iter)
	{
		if(bufPtr[iter]==STR_PRIVATE_APP_BEGIN) return false;

		// Skip white spaces and tabs
		while(iter<bufLen && (bufPtr[iter]==' ' || bufPtr[iter]=='\t')) ++iter;
		if(iter>=bufLen) break;

		// Compare first character
		if(bufPtr[iter]==key_name[0])
		{
			// Compare others
			++iter;
			for(j=1;key_name[j];++iter,++j)
			{
				if(iter>=bufLen) return false;
				if(key_name[j]!=bufPtr[iter]) goto label_NextLine;
			}

			// Skip white spaces and tabs
			while(iter<bufLen && (bufPtr[iter]==' ' || bufPtr[iter]=='\t')) ++iter;
			if(iter>=bufLen) break;

			// Check for '='
			if(bufPtr[iter]!=STR_PRIVATE_KEY_CHAR) goto label_NextLine;
			++iter;

			return true;
		}

label_NextLine:
		GotoEol(iter);
	}

	return false;
}

bool IniParser::GetPos(const char* app_name, const char* key_name, DWORD& iter)
{
	// Check
	if(!key_name || !key_name[0]) return false;

	// Find
	iter=0;
	if(app_name && lastAppPos && !strcmp(app_name,lastApp))
	{
		iter=lastAppPos;
	}
	else if(app_name)
	{
		if(!GotoApp(app_name,iter)) return false;
	}

	if(!GotoKey(key_name,iter)) return false;
	return true;
}

int IniParser::GetInt(const char* app_name, const char* key_name, int def_val)
{
	if(!bufPtr) return def_val;

	// Get pos
	DWORD iter;
	if(!GetPos(app_name,key_name,iter)) return def_val;

	// Read number
	DWORD j;
	char num[64];
	for(j=0;iter<bufLen;++iter,++j)
	{
		if(j>=63 || bufPtr[iter]=='\n' || bufPtr[iter]=='#' || bufPtr[iter]==';') break;
		//if(Buffer[iter]<'0' || Buffer[iter]>'9') return def_val;
		num[j]=bufPtr[iter];
	}

	if(!j) return def_val;
	num[j]=0;
	if(!_stricmp(num,"true")) return 1;
	if(!_stricmp(num,"false")) return 0;
	return atoi(num);
}

int IniParser::GetInt(const char* key_name, int def_val)
{
	return GetInt(NULL,key_name,def_val);
}

bool IniParser::GetStr(const char* app_name, const char* key_name, const char* def_val, char* ret_buf, char end /* = 0 */)
{
	// Check
	if(!ret_buf) return false;
	if(!bufPtr) goto label_DefVal;

	// Get pos
	DWORD iter=0;
	if(!GetPos(app_name,key_name,iter)) goto label_DefVal;

	// Skip white spaces and tabs
	while(iter<bufLen && (bufPtr[iter]==' ' || bufPtr[iter]=='\t')) ++iter;
	if(iter>=bufLen) goto label_DefVal;

	// Read string
	DWORD j=0;
	for(;iter<bufLen && bufPtr[iter];++iter)
	{
		if(end)
		{
			if(bufPtr[iter]==end) break;

			if(bufPtr[iter]=='\n' || bufPtr[iter]=='\r')
			{
				if(j && ret_buf[j-1]!=' ') ret_buf[j++]=' ';
				continue;
			}
			if(bufPtr[iter]==';' || bufPtr[iter]=='#')
			{
				if(j && ret_buf[j-1]!=' ') ret_buf[j++]=' ';
				GotoEol(iter);
				continue;
			}
		}
		else
		{
			if(!bufPtr[iter] || bufPtr[iter]=='\n' || bufPtr[iter]=='\r' || bufPtr[iter]=='#' || bufPtr[iter]==';') break;
		}

		ret_buf[j]=bufPtr[iter];
		j++;
	}

	// Erase white spaces and tabs from end
	for(;j;j--) if(ret_buf[j-1]!=' ' && ret_buf[j-1]!='\t') break;

	ret_buf[j]=0;
	return true;

label_DefVal:
	ret_buf[0]=0;
	if(def_val) StringCopy(ret_buf,MAX_FOTEXT,def_val);
	return false;
}

bool IniParser::GetStr(const char* key_name, const char* def_val, char* ret_buf, char end /* = 0 */)
{
	return GetStr(NULL,key_name,def_val,ret_buf,end);
}

bool IniParser::IsApp(const char* app_name)
{
	if(!bufPtr) return false;
	DWORD iter=0;
	return GotoApp(app_name,iter);
}

bool IniParser::IsKey(const char* app_name, const char* key_name)
{
	if(!bufPtr) return false;
	DWORD iter=0;
	return GetPos(app_name,key_name,iter);
}

bool IniParser::IsKey(const char* key_name)
{
	return IsKey(NULL,key_name);
}

char* IniParser::GetApp(const char* app_name)
{
	if(!bufPtr) return NULL;
	if(!app_name) return NULL;

	DWORD iter=0;
	if(lastAppPos && !strcmp(app_name,lastApp)) iter=lastAppPos;
	else if(!GotoApp(app_name,iter)) return NULL;

	DWORD i=iter,len=0;
	for(;i<bufLen;i++,len++) if(i>0 && bufPtr[i-1]=='\n' && bufPtr[i]==STR_PRIVATE_APP_BEGIN) break;
	for(;len;i--,len--) if(i>0 && bufPtr[i-1]!='\n' && bufPtr[i-1]!='\r') break;

	char* ret_buf=new char[len+1];
	if(len) memcpy(ret_buf,&bufPtr[iter],len);
	ret_buf[len]=0;
	return ret_buf;
}

bool IniParser::GotoNextApp(const char* app_name)
{
	if(!bufPtr) return false;
	return GotoApp(app_name,lastAppPos);
}

void IniParser::GetAppLines(StrVec& lines)
{
	if(!bufPtr) return;
	int i=lastAppPos,len=0;
	for(;i<bufLen;i++,len++) if(i>0 && bufPtr[i-1]=='\n' && bufPtr[i]==STR_PRIVATE_APP_BEGIN) break;
	for(;len;i--,len--) if(i>0 && bufPtr[i-1]!='\n' && bufPtr[i-1]!='\r') break;

	istrstream str(&bufPtr[lastAppPos],len);
	char line[MAX_FOTEXT];
	while(!str.eof())
	{
		str.getline(line,sizeof(line),'\n');
		lines.push_back(line);
	}
}

void IniParser::CacheApps()
{
	if(!bufPtr) return;

	istrstream str(bufPtr,bufLen);
	char line[MAX_FOTEXT];
	while(!str.eof())
	{
		str.getline(line,sizeof(line),'\n');
		if(line[0]==STR_PRIVATE_APP_BEGIN)
		{
			char* end=strstr(line+1,"]");
			if(end)
			{
				int len=end-line-1;
				if(len>0)
				{
					string app;
					app.assign(line+1,len);
					cachedKeys.insert(app);
				}
			}
		}
	}
}

bool IniParser::IsCachedApp(const char* app_name)
{
	return cachedKeys.count(string(app_name))!=0;
}

void IniParser::CacheKeys()
{
	if(!bufPtr) return;

	istrstream str(bufPtr,bufLen);
	char line[MAX_FOTEXT];
	while(!str.eof())
	{
		str.getline(line,sizeof(line),'\n');
		char* key_end=strstr(line,"=");
		if(key_end)
		{
			*key_end=0;
			Str::EraseFrontBackSpecificChars(line);
			if(strlen(line)) cachedKeys.insert(line);
		}
	}
}

bool IniParser::IsCachedKey(const char* key_name)
{
	return cachedKeys.count(string(key_name))!=0;
}

StrSet& IniParser::GetCachedKeys()
{
	return cachedKeys;
}

FOMsg::FOMsg()
{
	Clear();
}

FOMsg& FOMsg::operator+=(const FOMsg& r)
{
	DwordStrMulMap::const_iterator it=r.strData.begin();
	DwordStrMulMap::const_iterator end=r.strData.end();
	it++; // skip FOMSG_ERRNUM
	for(;it!=end;++it)
	{
		EraseStr((*it).first);
		AddStr((*it).first,(*it).second);
	}
	CalculateHash();
	return *this;
}

void FOMsg::AddStr(DWORD num, const char* str)
{
	if(num==FOMSG_ERRNUM) return;
	if(!str || !strlen(str)) strData.insert(DwordStrMulMapVal(num," "));
	else strData.insert(DwordStrMulMapVal(num,str));
}

void FOMsg::AddStr(DWORD num, const string& str)
{
	if(num==FOMSG_ERRNUM) return;
	if(!str.length()) strData.insert(DwordStrMulMapVal(num," "));
	else strData.insert(DwordStrMulMapVal(num,str));
}

void FOMsg::AddBinary(DWORD num, const BYTE* binary, DWORD len)
{
	CharVec str;
	str.reserve(len*2+1);
	for(int i=0;i<len;i++)
	{
		char c=(char)binary[i];
		if(c==0 || c=='}')
		{
			str.push_back(2);
			str.push_back(c+1);
		}
		else
		{
			str.push_back(1);
			str.push_back(c);
		}
	}
	str.push_back(0);

	AddStr(num,(char*)&str[0]);
}

DWORD FOMsg::AddStr(const char* str)
{
	DWORD i=Random(100000000,999999999);
	if(strData.count(i)) return AddStr(str);
	AddStr(i,str);
	return i;
}

const char* FOMsg::GetStr(DWORD num)
{
	DWORD str_count=strData.count(num);
	DwordStrMulMapIt it=strData.find(num);

	switch(str_count)
	{
	case 0: return (*strData.begin()).second.c_str(); // give FOMSG_ERRNUM
	case 1: break;
	default: for(int i=0,j=Random(0,str_count)-1;i<j;i++) ++it; break;
	}

	return (*it).second.c_str();
}

const char* FOMsg::GetStr(DWORD num, DWORD skip)
{
	DWORD str_count=strData.count(num);
	DwordStrMulMapIt it=strData.find(num);

	if(skip>=str_count) return (*strData.begin()).second.c_str(); // give FOMSG_ERRNUM
	for(int i=0;i<skip;i++) ++it;

	return (*it).second.c_str();
}

DWORD FOMsg::GetStrNumUpper(DWORD num)
{
	DwordStrMulMapIt it=strData.upper_bound(num);
	if(it==strData.end()) return 0;
	return (*it).first;
}

DWORD FOMsg::GetStrNumLower(DWORD num)
{
	DwordStrMulMapIt it=strData.lower_bound(num);
	if(it==strData.end()) return 0;
	return (*it).first;
}

int FOMsg::GetInt(DWORD num)
{
	DWORD str_count=strData.count(num);
	DwordStrMulMapIt it=strData.find(num);

	switch(str_count)
	{
	case 0: return -1;
	case 1: break;
	default: for(int i=0,j=Random(0,str_count)-1;i<j;i++) ++it; break;
	}

	return atoi((*it).second.c_str());
}

const BYTE* FOMsg::GetBinary(DWORD num, DWORD& len)
{
	if(!Count(num)) return NULL;

	static THREAD ByteVec* binary=NULL;
	if(!binary) binary=new(nothrow) ByteVec();

	const char* str=GetStr(num);
	binary->clear();
	for(int i=0,j=strlen(str);i<j-1;i+=2)
	{
		if(str[i]==1) binary->push_back(str[i+1]);
		else if(str[i]==2) binary->push_back(str[i+1]-1);
		else return NULL;
	}

	len=binary->size();
	return &(*binary)[0];
}

int FOMsg::Count(DWORD num)
{
	return !num?0:strData.count(num);
}

void FOMsg::EraseStr(DWORD num)
{
	if(num==FOMSG_ERRNUM) return;

	while(true)
	{
		DwordStrMulMapIt it=strData.find(num);
		if(it!=strData.end()) strData.erase(it);
		else break;
	}
}

DWORD FOMsg::GetSize()
{
	return strData.size()-1;
}

void FOMsg::CalculateHash()
{
	strDataHash=0;
#ifdef FONLINE_SERVER
	toSend.clear();
#endif
	DwordStrMulMapIt it=strData.begin();
	DwordStrMulMapIt end=strData.end();
	it++; // skip FOMSG_ERRNUM
	for(;it!=end;++it)
	{
		DWORD num=(*it).first;
		string& str=(*it).second;
		DWORD str_len=(DWORD)str.size();

#ifdef FONLINE_SERVER
		toSend.resize(toSend.size()+sizeof(num)+sizeof(str_len)+str_len);
		memcpy(&toSend[toSend.size()-(sizeof(num)+sizeof(str_len)+str_len)],&num,sizeof(num));
		memcpy(&toSend[toSend.size()-(sizeof(str_len)+str_len)],&str_len,sizeof(str_len));
		memcpy(&toSend[toSend.size()-str_len],(void*)str.c_str(),str_len);
#endif

		Crypt.Crc32((BYTE*)&num,sizeof(num),strDataHash);
		Crypt.Crc32((BYTE*)&str_len,sizeof(str_len),strDataHash);
		Crypt.Crc32((BYTE*)str.c_str(),str_len,strDataHash);
	}
}

DWORD FOMsg::GetHash()
{
	return strDataHash;
}

DwordStrMulMap& FOMsg::GetData()
{
	return strData;
}

#ifdef FONLINE_SERVER
const char* FOMsg::GetToSend()
{
	return &toSend[0];
}

DWORD FOMsg::GetToSendLen()
{
	return (DWORD)toSend.size();
}
#endif

#ifdef FONLINE_CLIENT
int FOMsg::LoadMsgStream(CharVec& stream)
{
	Clear();

	if(!stream.size()) return 0;

	DWORD pos=0;
	DWORD num=0;
	DWORD len=0;
	string str;
	while(true)
	{
		if(pos+sizeof(num)>stream.size()) break;
		memcpy(&num,&stream[pos],sizeof(num));
		pos+=sizeof(num);

		if(pos+sizeof(len)>stream.size()) break;
		memcpy(&len,&stream[pos],sizeof(len));
		pos+=sizeof(len);
	
		if(pos+len>stream.size()) break;
		str.resize(len);
		memcpy(&str[0],&stream[pos],len); //!!!
		pos+=len;
		
		AddStr(num,str);
	}
	return GetSize();
}
#endif

int FOMsg::LoadMsgFile(const char* fname, int path_type)
{
	Clear();

#ifdef FONLINE_CLIENT
	DWORD buf_len;
	char* buf=(char*)Crypt.GetCache(fname,buf_len);
	if(!buf) return -1;
#else
	FileManager fm;
	if(!fm.LoadFile(fname,path_type)) return -2;
	DWORD buf_len=fm.GetFsize();
	char* buf=(char*)fm.ReleaseBuffer();
#endif

	int result=LoadMsgFileBuf(buf,buf_len);
	delete[] buf;
	return result;
}

#pragma MESSAGE("Somewhere here bug.")
//  	FOnline.exe!_inflate_fast()  + 0x388 bytes	C
//  	FOnline.exe!_inflate()  + 0xd4a bytes	C
//  	FOnline.exe!_uncompress()  + 0x5b bytes	C
//  	FOnline.exe!CCrypt::Uncompress()  + 0x139 bytes	C++
//  	FOnline.exe!FOMsg::LoadMsgFile()  + 0x20 bytes	C++
//  	FOnline.exe!FOMsg::LoadMsgFile()  + 0x3a bytes	C++
//  	FOnline.exe!LanguagePack::LoadAll()  + 0x45 bytes	C++
//  	FOnline.exe!LanguagePack::Init()  + 0x20e bytes	C++
//  	FOnline.exe!CFEngine::Init()  + 0xa0c bytes	C++
//  	FOnline.exe!_WinMain@16()  + 0x205 bytes	C++
// >	FOnline.exe!__tmainCRTStartup()  Line 263 + 0x1b bytes	C

//  	FOnline.exe!_inflate_fast()  + 0x388 bytes	C
//  	FOnline.exe!_inflate()  + 0xd4a bytes	C
//  	FOnline.exe!_uncompress()  + 0x5b bytes	C
//  	FOnline.exe!CCrypt::Uncompress()  + 0x139 bytes	C++
//  	FOnline.exe!FOMsg::LoadMsgFile()  + 0x20 bytes	C++
//  	FOnline.exe!FOMsg::LoadMsgFile()  + 0x3a bytes	C++
//  	FOnline.exe!LanguagePack::LoadAll()  + 0x45 bytes	C++
//  	FOnline.exe!LanguagePack::Init()  + 0x20e bytes	C++
//  	FOnline.exe!CFEngine::Init()  + 0xa0c bytes	C++
//  	FOnline.exe!_WinMain@16()  + 0x205 bytes	C++
// >	FOnline.exe!__tmainCRTStartup()  Line 263 + 0x1b bytes	C

//  	FOnline.exe!_inflate_fast()  + 0x377 bytes	C
//  	FOnline.exe!_inflate()  + 0xd4a bytes	C
//  	FOnline.exe!_uncompress()  + 0x5b bytes	C
//  	FOnline.exe!CCrypt::Uncompress()  + 0x139 bytes	C++
//  	FOnline.exe!FOMsg::LoadMsgFile()  + 0x20 bytes	C++
//  	FOnline.exe!FOMsg::LoadMsgFile()  + 0x3a bytes	C++
//  	FOnline.exe!LanguagePack::LoadAll()  + 0x45 bytes	C++
//  	FOnline.exe!LanguagePack::Init()  + 0x20e bytes	C++
//  	FOnline.exe!CFEngine::Init()  + 0xa0c bytes	C++
//  	FOnline.exe!_WinMain@16()  + 0x205 bytes	C++
// >	FOnline.exe!__tmainCRTStartup()  Line 263 + 0x1b bytes	C

int FOMsg::LoadMsgFileBuf(char* data, DWORD data_len)
{
	Clear();

#ifdef FONLINE_CLIENT
	char* buf=(char*)Crypt.Uncompress((BYTE*)data,data_len,10);
	if(!buf) return -3;
#else
	char* buf=data;
	DWORD last_num=0;
#endif

	char* pbuf=buf;
	for(DWORD i=0;*pbuf && i<data_len;i++)
	{
		// Find '{' in begin of line
		if(*pbuf!='{')
		{
			Str::SkipLine(pbuf);
			continue;
		}

		// Skip '{'
		pbuf++;
		if(!*pbuf) break;

		// atoi
		DWORD num_info=(DWORD)_atoi64(pbuf);
		if(!num_info)
		{
			Str::SkipLine(pbuf);
			continue;
		}

		// Skip 'Number}{}{'
		Str::GoTo(pbuf,'{',true);
		Str::GoTo(pbuf,'{',true);
		if(!*pbuf) break;

		// Find '}'	
		char* _pbuf=pbuf;
		Str::GoTo(pbuf,'}');
		if(!*pbuf) break;
		*pbuf=0;

#ifndef FONLINE_CLIENT
		if(num_info<last_num)
		{
			WriteLog(__FUNCTION__" - Error string id, cur<%u>, last<%u>\n",num_info,last_num);
			return -4;
		}
		last_num=num_info;
#endif

		AddStr(num_info,_pbuf);
		pbuf++;
	}

#ifdef FONLINE_CLIENT
	delete[] buf;
#endif
	CalculateHash();
	return GetSize();
}

int FOMsg::SaveMsgFile(const char* fname, int path_type)
{
#ifndef FONLINE_CLIENT
	FileManager fm;
#endif

	DwordStrMulMapIt it=strData.begin();
	it++; //skip FOMSG_ERRNUM

	string str;
	for(;it!=strData.end();it++)
	{
		str+="{";
		str+=Str::DWtoA((*it).first);
		str+="}{}{";
		str+=(*it).second;
		str+="}\n";
	}

	char* buf=(char*)str.c_str();
	DWORD buf_len=str.length();

#ifdef FONLINE_CLIENT
	buf=(char*)Crypt.Compress((BYTE*)buf,buf_len);
	if(!buf) return -2;
	Crypt.SetCache(fname,(BYTE*)buf,buf_len);
	delete[] buf;
#else
	fm.SetData(buf,buf_len);
	if(!fm.SaveOutBufToFile(fname,path_type)) return -2;
#endif

	return 1;
}

void FOMsg::Clear()
{
	strData.clear();
	strData.insert(DwordStrMapVal(FOMSG_ERRNUM,string("error")));

#ifdef FONLINE_SERVER
	toSend.clear();
#endif

	strDataHash=0;
}

bool LanguagePack::Init(const char* lang, int path_type)
{
	memcpy(NameStr,lang,4);
	PathType=path_type;
	if(LoadAll()<0) return false;
	return true;
}

int LanguagePack::LoadAll()
{
	// Loading All MSG files
	if(!Name)
	{
		WriteLog(__FUNCTION__" - Lang Pack is not initialized.\n");
		return -1;
	}

	int count_fail=0;
	for(int i=0;i<TEXTMSG_COUNT;i++)
	{
		if(Msg[i].LoadMsgFile(Str::Format("%s\\%s",NameStr,TextMsgFileName[i]),PathType)<0)
		{
			count_fail++;
			WriteLog(__FUNCTION__" - Unable to load MSG<%s>.\n",TextMsgFileName[i]);
		}
	}

	return -count_fail;
}
