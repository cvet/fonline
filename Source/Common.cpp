#include "StdAfx.h"
#include "Common.h"
#include "Exception.h"
#include "Crypt.h"
#include "Script.h"
#include "Text.h"
#include <math.h>

/************************************************************************/
/*                                                                      */
/************************************************************************/

int Random(int minimum, int maximum)
{
	return minimum+((int)(__int64(rand())*__int64(maximum-minimum+1)/(__int64(RAND_MAX)+1)));
}

int Procent(int full, int peace)
{
	if(!full) return 0;
	return peace*100/full;
}

int DistSqrt(int x1, int y1, int x2, int y2)
{
	int dx=x1-x2;
	int dy=y1-y2;
	return (int)sqrt(double(dx*dx+dy*dy));
}

int DistGame(int x1, int y1, int x2, int y2)
{
	int dx=(x1>x2?x1-x2:x2-x1);
	if(x1%2==0)
	{
		if(y2<=y1)
		{
			int rx=y1-y2-dx/2;
			return dx+(rx>0?rx:0);
		}
		else
		{
			int rx=y2-y1-(dx+1)/2;
			return dx+(rx>0?rx:0);
		}
	}
	else
	{
		if(y2>=y1)
		{
			int rx=y2-y1-dx/2;
			return dx+(rx>0?rx:0);
		}
		else
		{
			int rx=y1-y2-(dx+1)/2;
			return dx+(rx>0?rx:0);
		}
	}
}

DWORD NumericalNumber(DWORD num)
{
	if(num%2)
		return num*(num/2+1);
	else
		return num*num/2+num/2;
}

int NextLevel(int cur_level)
{
	//return cur_level>20?210000+(cur_level-20)*40000:NumericalNumber(cur_level)*1000;
	return NumericalNumber(cur_level)*1000;
}

int GetDir(int x1, int y1, int x2, int y2)
{
	int dir=0;

	if(x1%2)
	{
		if(x1> x2 && y1> y2) dir=0;
		else if(x1> x2 && y1==y2) dir=1;
		else if(x1==x2 && y1< y2) dir=2;
		else if(x1< x2 && y1==y2) dir=3;
		else if(x1< x2 && y1> y2) dir=4;
		else if(x1==x2 && y1> y2) dir=5;
	}
	else
	{
		if(x1> x2 && y1==y2) dir=0;
		else if(x1> x2 && y1< y2) dir=1;
		else if(x1==x2 && y1< y2) dir=2;
		else if(x1< x2 && y1< y2) dir=3;
		else if(x1< x2 && y1==y2) dir=4;
		else if(x1==x2 && y1> y2) dir=5;
	}

	return dir;
}

int GetFarDir(int x1, int y1, int x2, int y2)
{
	float hx=x1;
	float hy=y1;
	float tx=x2;
	float ty=y2;
	float nx=3*(tx-hx);
	float ny=(ty-hy)*SQRT3T2_FLOAT-(float(x2%2)-float(x1%2))*SQRT3_FLOAT;
	float dir=180.0f+RAD2DEG*atan2f(ny,nx);

	if(dir>=60.0f  && dir<120.0f) return 5;
	if(dir>=120.0f && dir<180.0f) return 4;
	if(dir>=180.0f && dir<240.0f) return 3;
	if(dir>=240.0f && dir<300.0f) return 2;
	if(dir>=300.0f && dir<360.0f) return 1;
	return 0;
}

bool GetCoords(WORD x1, WORD y1, BYTE ori, WORD& x2, WORD& y2)
{
	switch(ori)
	{
	case 0:
		x1--;
		if(!(x1%2)) y1--;
		break;
	case 1:
		x1--;
		if(x1%2) y1++;
		break;
	case 2:
		y1++;
		break;
	case 3:
		x1++;
		if(x1%2) y1++;
		break;
	case 4:
		x1++;
		if(!(x1%2)) y1--;
		break;
	case 5:
		y1--;
		break;
	default:
		return false;
	}

	x2=x1;
	y2=y1;
	return true;
}

bool CheckDist(WORD x1, WORD y1, WORD x2, WORD y2, DWORD dist)
{
	return DistGame(x1,y1,x2,y2)<=dist;
}

int ReverseDir(int dir)
{
	dir+=3;
	if(dir>5) dir-=6;
	return dir;
}

void GetStepsXY(float& sx, float& sy, int x1, int y1, int x2, int y2)
{
	float dx=ABS(x2-x1);
	float dy=ABS(y2-y1);

	sx=1.0f;
	sy=1.0f;

	dx<dy?sx=dx/dy:sy=dy/dx;

	if(x2<x1) sx=-sx;
	if(y2<y1) sy=-sy;
}

void ChangeStepsXY(float& sx, float& sy, float deq)
{
	float rad=deq*PI_FLOAT/180.0f;
	sx=sx*cos(rad)-sy*sin(rad);
	sy=sx*sin(rad)+sy*cos(rad);
}

void MoveHexByDir(WORD& hx, WORD& hy, BYTE dir, WORD maxhx, WORD maxhy)
{
	int hx_=hx;
	int hy_=hy;
	switch(dir)
	{
	case 0:
		hx_--;
		if(!(hx_&1)) hy_--;
		break;
	case 1:
		hx_--;
		if(hx_&1) hy_++;
		break;
	case 2:
		hy_++;
		break;
	case 3:
		hx_++;
		if(hx_&1) hy_++;
		break;
	case 4:
		hx_++;
		if(!(hx_&1)) hy_--;
		break;
	case 5:
		hy_--;
		break;
	default:
		return;
	}

	if(hx_>=0 && hx_<maxhx && hy_>=0 && hy_<maxhy)
	{
		hx=hx_;
		hy=hy_;
	}
}

void MoveHexByDirUnsafe(int& hx, int& hy, BYTE dir)
{
	switch(dir)
	{
	case 0:
		hx--;
		if(!(hx&1)) hy--;
		break;
	case 1:
		hx--;
		if(hx&1) hy++;
		break;
	case 2:
		hy++;
		break;
	case 3:
		hx++;
		if(hx&1) hy++;
		break;
	case 4:
		hx++;
		if(!(hx&1)) hy--;
		break;
	case 5:
		hy--;
		break;
	default:
		return;
	}
}

bool IntersectCircleLine(int cx, int cy, int radius, int x1, int y1, int x2, int y2)
{
	int x01=x1-cx;
	int y01=y1-cy;
	int x02=x2-cx;
	int y02=y2-cy;
	int dx=x02-x01;
	int dy=y02-y01;
	int a=dx*dx+dy*dy;
	int b=2*(x01*dx+y01*dy);
	int c=x01*x01+y01*y01-radius*radius;
	if(-b<0)return c<0;
	if(-b<2*a)return 4*a*c-b*b<0;
	return a+b+c<0;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

bool NameTrueChar[0x100];
bool PassTrueChar[0x100];
void LoadTrueChars()
{
	for(int i=0;i<0x100;i++) NameTrueChar[i]=false;
	for(int i=0;i<0x100;i++) PassTrueChar[i]=false;
	const BYTE name_char[]=" _.-1234567890AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZzÀàÁáÂâÃãÄäÅå¨¸ÆæÇçÈèÉéÊêËëÌìÍíÎîÏïÐðÑñÒòÓóÔôÕõÖö×÷ØøÙùÚúÛûÜüÝýÞþßÿ";
	const BYTE pass_char[]=" _-,.=[]{}?!@#$^&()|`~:;1234567890AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZzÀàÁáÂâÃãÄäÅå¨¸ÆæÇçÈèÉéÊêËëÌìÍíÎîÏïÐðÑñÒòÓóÔôÕõÖö×÷ØøÙùÚúÛûÜüÝýÞþßÿ";
	for(int i=0,j=strlen((char*)name_char);i<j;i++) NameTrueChar[name_char[i]]=true;
	for(int i=0,j=strlen((char*)pass_char);i<j;i++) PassTrueChar[pass_char[i]]=true;
}

bool CheckUserName(const char* str)
{
	for(;*str;str++) if(!NameTrueChar[(BYTE)*str]) return false;
	return true;
}

bool CheckUserPass(const char* str)
{
	for(;*str;str++) if(!PassTrueChar[(BYTE)*str]) return false;
	return true;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
#if defined(FONLINE_CLIENT) || defined(FONLINE_MAPPER)

bool CmnQuit=false;
int CmnScrOx=0;
int CmnScrOy=0;
bool OptShowTile=true;
bool OptShowRoof=true;
bool OptShowItem=true;
bool OptShowScen=true;
bool OptShowWall=true;
bool OptShowCrit=true;
bool OptShowFast=true;
bool CmnShowPlayerNames=false;
bool CmnShowNpcNames=false;
bool CmnShowCritId=false;
bool CmnDiLeft=false;
bool CmnDiRight=false;
bool CmnDiUp=false;
bool CmnDiDown=false;
bool CmnDiMleft=false;
bool CmnDiMright=false;
bool CmnDiMup=false;
bool CmnDiMdown=false;
bool OptShowGroups=true;
bool OptHelpInfo=false;
bool OptDebugInfo=false;
bool OptDebugNet=false;
bool OptDebugIface=false;
bool OptFullScr=false;
bool OptVSync=false;
int OptFlushVal=256;
int OptBaseTex=256;
int OptScreenClear=0;
int OptLight=0;
string OptHost;
DWORD OptPort=0;
DWORD OptProxyType=0;
string OptProxyHost;
DWORD OptProxyPort=0;
string OptProxyUser;
string OptProxyPass;
string OptName;
string OptPass;
int OptScrollDelay=0;
int OptScrollStep=1;
bool OptScrollCheck=true;
int OptMouseSpeed=1;
bool OptGlobalSound=true;
string OptMasterPath;
string OptCritterPath;
string OptFoPatchPath;
string OptFoDataPath;
int OptSleep=0;
bool OptMsgboxInvert=false;
int OptChangeLang=CHANGE_LANG_CTRL_SHIFT;
BYTE OptDefaultCombatMode=COMBAT_MODE_ANY;
bool OptMessNotify=true;
bool OptSoundNotify=true;
bool OptAlwaysOnTop=false;
DWORD OptTextDelay=3000;
DWORD OptDamageHitDelay=0;
int OptScreenWidth=800;
int OptScreenHeight=600;
int OptMultiSampling=0;
bool OptSoftwareSkinning=false;
bool OptMouseScroll=true;
int OptIndicatorType=INDICATOR_LINES;
DWORD OptDoubleClickTime=0;
BYTE OptRoofAlpha=200;
bool OptHideCursor=false;
bool OptDisableLMenu=false;
bool OptDisableMouseEvents=false;
bool OptDisableKeyboardEvents=false;
string OptPlayerOffAppendix="_off";
int OptCombatMessagesType=0;
string OptMapperCache="default";

DWORD GetColorDay(int* day_time, BYTE* colors, int game_time, int* light)
{
	BYTE result[3];
	int color_r[4]={colors[0],colors[1],colors[2],colors[3]};
	int color_g[4]={colors[4],colors[5],colors[6],colors[7]};
	int color_b[4]={colors[8],colors[9],colors[10],colors[11]};

	game_time%=1440;
	int time,duration;
	if(game_time>=day_time[0] && game_time<day_time[1])
	{
		time=0;
		game_time-=day_time[0];
		duration=day_time[1]-day_time[0];
	}
	else if(game_time>=day_time[1] && game_time<day_time[2])
	{
		time=1;
		game_time-=day_time[1];
		duration=day_time[2]-day_time[1];
	}
	else if(game_time>=day_time[2] && game_time<day_time[3])
	{
		time=2;
		game_time-=day_time[2];
		duration=day_time[3]-day_time[2];
	}
	else
	{
		time=3;
		if(game_time>=day_time[3]) game_time-=day_time[3];
		else game_time+=1440-day_time[3];
		duration=(1440-day_time[3])+day_time[0];
	}

	if(!duration) duration=1;
	result[0]=color_r[time]+(color_r[time<3?time+1:0]-color_r[time])*game_time/duration;
	result[1]=color_g[time]+(color_g[time<3?time+1:0]-color_g[time])*game_time/duration;
	result[2]=color_b[time]+(color_b[time<3?time+1:0]-color_b[time])*game_time/duration;

	if(light)
	{
		int max_light=(max(max(max(color_r[0],color_r[1]),color_r[2]),color_r[3])+
			max(max(max(color_g[0],color_g[1]),color_g[2]),color_g[3])+
			max(max(max(color_b[0],color_b[1]),color_b[2]),color_b[3]))/3;
		int min_light=(min(min(min(color_r[0],color_r[1]),color_r[2]),color_r[3])+
			min(min(min(color_g[0],color_g[1]),color_g[2]),color_g[3])+
			min(min(min(color_b[0],color_b[1]),color_b[2]),color_b[3]))/3;
		int cur_light=(result[0]+result[1]+result[2])/3;
		*light=Procent(max_light-min_light,max_light-cur_light);
		*light=CLAMP(*light,0,100);
	}

	return (result[0]<<16)|(result[1]<<8)|(result[2]);
}

void GetClientOptions()
{
	// Randomize
	srand(Timer::FastTick());

	// Cache distantion
//	PrepareDistantion(CacheDistC,CacheDistLine,CacheDistLine);
//	PrepareDistantion(CacheDistNC,CacheDistLine+1,CacheDistLine);

	// Defines
#define GETOPTIONS_CMD_LINE_INT(opt,str_id) do{char* str=strstr(GetCommandLine(),str_id); if(str) opt=atoi(str+strlen(str_id)+1);}while(0)
#define GETOPTIONS_CMD_LINE_BOOL(opt,str_id) do{char* str=strstr(GetCommandLine(),str_id); if(str) opt=atoi(str+strlen(str_id)+1)!=0;}while(0)
#define GETOPTIONS_CMD_LINE_STR(opt,str_id) do{char* str=strstr(GetCommandLine(),str_id); if(str) sscanf_s(str+strlen(str_id)+1,"%s",opt);}while(0)
#define GETOPTIONS_CHECK(val_,min_,max_,def_) do{if(val_<min_ || val_>max_) val_=def_;} while(0)

	// Language
	char lang_name[256];
	GetPrivateProfileString(CFG_FILE_APP_NAME,"Language","russ",lang_name,5,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_STR(lang_name,"Language");
	if(!strcmp(lang_name,"russ")) SetExceptionsRussianText();

	// Int
	OptFullScr=GetPrivateProfileInt(CFG_FILE_APP_NAME,"FullScreen",false,CLIENT_CONFIG_FILE)!=0;
	GETOPTIONS_CMD_LINE_BOOL(OptFullScr,"-FullScreen");
	OptVSync=GetPrivateProfileInt(CFG_FILE_APP_NAME,"VSync",false,CLIENT_CONFIG_FILE)!=0;
	GETOPTIONS_CMD_LINE_BOOL(OptVSync,"-VSync");
	OptScreenClear=GetPrivateProfileInt(CFG_FILE_APP_NAME,"BackGroundClear",false,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptScreenClear,"-BackGroundClear");
	GETOPTIONS_CHECK(OptScreenClear,0,1,0);
	OptLight=GetPrivateProfileInt(CFG_FILE_APP_NAME,"Light",20,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptLight,"-Light");
	GETOPTIONS_CHECK(OptLight,0,50,20);
	OptScrollDelay=GetPrivateProfileInt(CFG_FILE_APP_NAME,"ScrollDelay",4,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptScrollDelay,"-ScrollDelay");
	GETOPTIONS_CHECK(OptScrollDelay,1,32,4);
	OptScrollStep=GetPrivateProfileInt(CFG_FILE_APP_NAME,"ScrollStep",32,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptScrollStep,"-ScrollStep");
	GETOPTIONS_CHECK(OptScrollStep,4,32,32);
	OptMouseSpeed=GetPrivateProfileInt(CFG_FILE_APP_NAME,"MouseSpeed",1,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptMouseSpeed,"-MouseSpeed");
	GETOPTIONS_CHECK(OptMouseSpeed,1,5,1);
	OptTextDelay=GetPrivateProfileInt(CFG_FILE_APP_NAME,"TextDelay",3000,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptTextDelay,"-TextDelay");
	GETOPTIONS_CHECK(OptTextDelay,1000,3000,30000);
	OptDamageHitDelay=GetPrivateProfileInt(CFG_FILE_APP_NAME,"DamageHitDelay",0,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptDamageHitDelay,"-OptDamageHitDelay");
	GETOPTIONS_CHECK(OptDamageHitDelay,0,30000,0);
	OptScreenWidth=GetPrivateProfileInt(CFG_FILE_APP_NAME,"ScreenWidth",0,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptScreenWidth,"-ScreenWidth");
	GETOPTIONS_CHECK(OptScreenWidth,100,30000,800);
	OptScreenHeight=GetPrivateProfileInt(CFG_FILE_APP_NAME,"ScreenHeight",0,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptScreenHeight,"-ScreenHeight");
	GETOPTIONS_CHECK(OptScreenHeight,100,30000,600);
	OptMultiSampling=GetPrivateProfileInt(CFG_FILE_APP_NAME,"MultiSampling",0,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptMultiSampling,"-MultiSampling");
	GETOPTIONS_CHECK(OptMultiSampling,-1,16,-1);
	OptSoftwareSkinning=GetPrivateProfileInt(CFG_FILE_APP_NAME,"SoftwareSkinning",1,CLIENT_CONFIG_FILE)!=0;
	GETOPTIONS_CMD_LINE_BOOL(OptSoftwareSkinning,"-SoftwareSkinning");
	OptAlwaysOnTop=GetPrivateProfileInt(CFG_FILE_APP_NAME,"AlwaysOnTop",false,CLIENT_CONFIG_FILE)!=0;
	GETOPTIONS_CMD_LINE_BOOL(OptAlwaysOnTop,"-AlwaysOnTop");
	OptFlushVal=GetPrivateProfileInt(CFG_FILE_APP_NAME,"FlushValue",50,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptFlushVal,"-FlushValue");
	GETOPTIONS_CHECK(OptFlushVal,1,1000,50);
	OptBaseTex=GetPrivateProfileInt(CFG_FILE_APP_NAME,"BaseTexture",1024,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptBaseTex,"-BaseTexture");
	GETOPTIONS_CHECK(OptBaseTex,128,8192,1024);
	OptSleep=GetPrivateProfileInt(CFG_FILE_APP_NAME,"Sleep",0,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptSleep,"-Sleep");
	GETOPTIONS_CHECK(OptSleep,-1,100,0);
	OptMsgboxInvert=GetPrivateProfileInt(CFG_FILE_APP_NAME,"InvertMessBox",false,CLIENT_CONFIG_FILE)!=0;
	GETOPTIONS_CMD_LINE_BOOL(OptMsgboxInvert,"-InvertMessBox");
	OptChangeLang=GetPrivateProfileInt(CFG_FILE_APP_NAME,"LangChange",CHANGE_LANG_CTRL_SHIFT,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptChangeLang,"-LangChange");
	GETOPTIONS_CHECK(OptChangeLang,0,1,0);
	OptMessNotify=GetPrivateProfileInt(CFG_FILE_APP_NAME,"WinNotify",true,CLIENT_CONFIG_FILE)!=0;
	GETOPTIONS_CMD_LINE_BOOL(OptMessNotify,"-WinNotify");
	OptSoundNotify=GetPrivateProfileInt(CFG_FILE_APP_NAME,"SoundNotify",false,CLIENT_CONFIG_FILE)!=0;
	GETOPTIONS_CMD_LINE_BOOL(OptSoundNotify,"-SoundNotify");
	OptPort=GetPrivateProfileInt(CFG_FILE_APP_NAME,"RemotePort",4000,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptPort,"-RemotePort");
	GETOPTIONS_CHECK(OptPort,0,0xFFFF,4000);
	OptProxyType=GetPrivateProfileInt(CFG_FILE_APP_NAME,"ProxyType",0,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptProxyType,"-ProxyType");
	GETOPTIONS_CHECK(OptProxyType,0,3,0);
	OptProxyPort=GetPrivateProfileInt(CFG_FILE_APP_NAME,"ProxyPort",1080,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptProxyPort,"-ProxyPort");
	GETOPTIONS_CHECK(OptProxyPort,0,0xFFFF,1080);
	OptGlobalSound=GetPrivateProfileInt(CFG_FILE_APP_NAME,"GlobalSound",true,CLIENT_CONFIG_FILE)!=0;
	GETOPTIONS_CMD_LINE_BOOL(OptGlobalSound,"-GlobalSound");
	OptDefaultCombatMode=GetPrivateProfileInt(CFG_FILE_APP_NAME,"DefaultCombatMode",COMBAT_MODE_ANY,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptDefaultCombatMode,"-DefaultCombatMode");
	GETOPTIONS_CHECK(OptDefaultCombatMode,COMBAT_MODE_ANY,COMBAT_MODE_TURN_BASED,COMBAT_MODE_ANY);
	OptIndicatorType=GetPrivateProfileInt(CFG_FILE_APP_NAME,"IndicatorType",COMBAT_MODE_ANY,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptIndicatorType,"-IndicatorType");
	GETOPTIONS_CHECK(OptIndicatorType,INDICATOR_LINES,INDICATOR_BOTH,INDICATOR_LINES);
	OptDoubleClickTime=GetPrivateProfileInt(CFG_FILE_APP_NAME,"DoubleClickTime",COMBAT_MODE_ANY,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptDoubleClickTime,"-DoubleClickTime");
	GETOPTIONS_CHECK(OptDoubleClickTime,0,1000,0);
	if(!OptDoubleClickTime) OptDoubleClickTime=GetDoubleClickTime();
	OptCombatMessagesType=GetPrivateProfileInt(CFG_FILE_APP_NAME,"CombatMessagesType",0,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(OptCombatMessagesType,"-CombatMessagesType");
	GETOPTIONS_CHECK(OptCombatMessagesType,0,1,0);
	GameOpt.Animation3dFPS=GetPrivateProfileInt(CFG_FILE_APP_NAME,"Animation3dFPS",0,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(GameOpt.Animation3dFPS,"-Animation3dFPS");
	GETOPTIONS_CHECK(GameOpt.Animation3dFPS,0,1000,0);
	GameOpt.Animation3dSmoothTime=GetPrivateProfileInt(CFG_FILE_APP_NAME,"Animation3dSmoothTime",0,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_INT(GameOpt.Animation3dSmoothTime,"-Animation3dSmoothTime");
	GETOPTIONS_CHECK(GameOpt.Animation3dSmoothTime,0,10000,250);

	GETOPTIONS_CMD_LINE_BOOL(OptHelpInfo,"-HelpInfo");
	GETOPTIONS_CMD_LINE_BOOL(OptDebugInfo,"-DebugInfo");
	GETOPTIONS_CMD_LINE_BOOL(OptDebugNet,"-DebugNet");
	GETOPTIONS_CMD_LINE_BOOL(OptDebugIface,"-DebugIface");

	// Str
	char buf[2048];
	GetPrivateProfileString(CFG_FILE_APP_NAME,"MasterDatPath","master.dat",buf,2048,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_STR(buf,"-MasterDatPath");
	OptMasterPath=buf;
	GetPrivateProfileString(CFG_FILE_APP_NAME,"CritterDatPath","critter.dat",buf,2048,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_STR(buf,"-CritterDatPath");
	OptCritterPath=buf;
	GetPrivateProfileString(CFG_FILE_APP_NAME,"PatchDatPath","fopatch.dat",buf,2048,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_STR(buf,"-PatchDatPath");
	OptFoPatchPath=buf;
	GetPrivateProfileString(CFG_FILE_APP_NAME,"FonlineDataPath",".\\data",buf,2048,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_STR(buf,"-FonlineDataPath");
	OptFoDataPath=buf;
	GetPrivateProfileString(CFG_FILE_APP_NAME,"RemoteHost","localhost",buf,2048,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_STR(buf,"-RemoteHost");
	OptHost=buf;
	GetPrivateProfileString(CFG_FILE_APP_NAME,"ProxyHost","localhost",buf,2048,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_STR(buf,"-ProxyHost");
	OptProxyHost=buf;
	GetPrivateProfileString(CFG_FILE_APP_NAME,"ProxyUser","",buf,2048,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_STR(buf,"-ProxyUser");
	OptProxyUser=buf;
	GetPrivateProfileString(CFG_FILE_APP_NAME,"ProxyPass","",buf,2048,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_STR(buf,"-ProxyPass");
	OptProxyPass=buf;
	GetPrivateProfileString(CFG_FILE_APP_NAME,"UserName","",buf,2048,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_STR(buf,"-UserName");
	OptName=buf;
	GetPrivateProfileString(CFG_FILE_APP_NAME,"UserPass","",buf,2048,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_STR(buf,"-UserPass");
	OptPass=buf;

	GetPrivateProfileString(CFG_FILE_APP_NAME,"MapperCache","default",buf,2048,CLIENT_CONFIG_FILE);
	GETOPTIONS_CMD_LINE_STR(buf,"-MapperCache");
	OptMapperCache=buf;

	// Logging
	bool logging=GetPrivateProfileInt(CFG_FILE_APP_NAME,"Logging",1,CLIENT_CONFIG_FILE)!=0;
	GETOPTIONS_CMD_LINE_BOOL(logging,"-Logging");
	if(!logging)
	{
		WriteLog("Logging off.\n");
		LogFinish(-1);
	}

	logging=GetPrivateProfileInt(CFG_FILE_APP_NAME,"LoggingTime",false,CLIENT_CONFIG_FILE)!=0;
	GETOPTIONS_CMD_LINE_BOOL(logging,"-LoggingTime");
	LogWithTime(logging);

	LoadTrueChars();

#ifdef FONLINE_CLIENT
	Script::SetGarbageCollectTime(120000);
#endif
}

ClientScriptFunctions ClientFunctions;
MapperScriptFunctions MapperFunctions;
#endif
/************************************************************************/
/*                                                                      */
/************************************************************************/
#ifdef FONLINE_SERVER

bool FOAppQuit=false;
volatile bool FOQuit=false;
HANDLE UpdateEvent=NULL;
HANDLE LogEvent=NULL;
int ServerGameSleep=10;
int MemoryDebugLevel=10;
DWORD VarsGarbageTime=3600000;
bool WorldSaveManager=true;

void GetServerOptions()
{
	srand(Timer::FastTick());

	// Cache distantion
//	PrepareDistantion(CacheDistC,CacheDistLine,CacheDistLine);
//	PrepareDistantion(CacheDistNC,CacheDistLine+1,CacheDistLine);

	IniParser cfg;
	cfg.LoadFile(SERVER_CONFIG_FILE);
	ServerGameSleep=cfg.GetInt("GameSleep",10);
	Script::SetGarbageCollectTime(cfg.GetInt("ASGarbageTime",120)*1000);
	VarsGarbageTime=cfg.GetInt("VarsGarbageTime",3600)*1000;
	WorldSaveManager=(cfg.GetInt("WorldSaveManager",1)==1);
	LoadTrueChars();
}

ServerScriptFunctions ServerFunctions;

#endif
/************************************************************************/
/*                                                                      */
/************************************************************************/
#if defined(FONLINE_CLIENT) || defined(FONLINE_SERVER)

const char* GetLastSocketError()
{
	static char str[256];
	int error=WSAGetLastError();
#define CASE_SOCK_ERROR(code,message) case code: sprintf(str,#code", %d, "message,code); break

	switch(error)
	{
	default: sprintf(str,"%d, unknown error code.",error); break;
	CASE_SOCK_ERROR(WSAEINTR,"A blocking operation was interrupted by a call to WSACancelBlockingCall.");
	CASE_SOCK_ERROR(WSAEBADF,"The file handle supplied is not valid.");
	CASE_SOCK_ERROR(WSAEACCES,"An attempt was made to access a socket in a way forbidden by its access permissions.");
	CASE_SOCK_ERROR(WSAEFAULT,"The system detected an invalid pointer address in attempting to use a pointer argument in a call.");
	CASE_SOCK_ERROR(WSAEINVAL,"An invalid argument was supplied.");
	CASE_SOCK_ERROR(WSAEMFILE,"Too many open sockets.");
	CASE_SOCK_ERROR(WSAEWOULDBLOCK,"A non-blocking socket operation could not be completed immediately.");
	CASE_SOCK_ERROR(WSAEINPROGRESS,"A blocking operation is currently executing.");
	CASE_SOCK_ERROR(WSAEALREADY,"An operation was attempted on a non-blocking socket that already had an operation in progress.");
	CASE_SOCK_ERROR(WSAENOTSOCK,"An operation was attempted on something that is not a socket.");
	CASE_SOCK_ERROR(WSAEDESTADDRREQ,"A required address was omitted from an operation on a socket.");
	CASE_SOCK_ERROR(WSAEMSGSIZE,"A message sent on a datagram socket was larger than the internal message buffer or some other network limit, or the buffer used to receive a datagram into was smaller than the datagram itself.");
	CASE_SOCK_ERROR(WSAEPROTOTYPE,"A protocol was specified in the socket function call that does not support the semantics of the socket type requested.");
	CASE_SOCK_ERROR(WSAENOPROTOOPT,"An unknown, invalid, or unsupported option or level was specified in a getsockopt or setsockopt call.");
	CASE_SOCK_ERROR(WSAEPROTONOSUPPORT,"The requested protocol has not been configured into the system, or no implementation for it exists.");
	CASE_SOCK_ERROR(WSAESOCKTNOSUPPORT,"The support for the specified socket type does not exist in this address family.");
	CASE_SOCK_ERROR(WSAEOPNOTSUPP,"The attempted operation is not supported for the type of object referenced.");
	CASE_SOCK_ERROR(WSAEPFNOSUPPORT,"The protocol family has not been configured into the system or no implementation for it exists.");
	CASE_SOCK_ERROR(WSAEAFNOSUPPORT,"An address incompatible with the requested protocol was used.");
	CASE_SOCK_ERROR(WSAEADDRINUSE,"Only one usage of each socket address (protocol/network address/port) is normally permitted.");
	CASE_SOCK_ERROR(WSAEADDRNOTAVAIL,"The requested address is not valid in its context.");
	CASE_SOCK_ERROR(WSAENETDOWN,"A socket operation encountered a dead network.");
	CASE_SOCK_ERROR(WSAENETUNREACH,"A socket operation was attempted to an unreachable network.");
	CASE_SOCK_ERROR(WSAENETRESET,"The connection has been broken due to keep-alive activity detecting a failure while the operation was in progress.");
	CASE_SOCK_ERROR(WSAECONNABORTED,"An established connection was aborted by the software in your host machine.");
	CASE_SOCK_ERROR(WSAECONNRESET,"An existing connection was forcibly closed by the remote host.");
	CASE_SOCK_ERROR(WSAENOBUFS,"An operation on a socket could not be performed because the system lacked sufficient buffer space or because a queue was full.");
	CASE_SOCK_ERROR(WSAEISCONN,"A connect request was made on an already connected socket.");
	CASE_SOCK_ERROR(WSAENOTCONN,"A request to send or receive data was disallowed because the socket is not connected and (when sending on a datagram socket using a sendto call) no address was supplied.");
	CASE_SOCK_ERROR(WSAESHUTDOWN,"A request to send or receive data was disallowed because the socket had already been shut down in that direction with a previous shutdown call.");
	CASE_SOCK_ERROR(WSAETOOMANYREFS,"Too many references to some kernel object.");
	CASE_SOCK_ERROR(WSAETIMEDOUT,"A connection attempt failed because the connected party did not properly respond after a period of time, or established connection failed because connected host has failed to respond.");
	CASE_SOCK_ERROR(WSAECONNREFUSED,"No connection could be made because the target machine actively refused it.");
	CASE_SOCK_ERROR(WSAELOOP,"Cannot translate name.");
	CASE_SOCK_ERROR(WSAENAMETOOLONG,"Name component or name was too long.");
	CASE_SOCK_ERROR(WSAEHOSTDOWN,"A socket operation failed because the destination host was down.");
	CASE_SOCK_ERROR(WSAEHOSTUNREACH,"A socket operation was attempted to an unreachable host.");
	CASE_SOCK_ERROR(WSAENOTEMPTY,"Cannot remove a directory that is not empty.");
	CASE_SOCK_ERROR(WSAEPROCLIM,"A Windows Sockets implementation may have a limit on the number of applications that may use it simultaneously.");
	CASE_SOCK_ERROR(WSAEUSERS,"Ran out of quota.");
	CASE_SOCK_ERROR(WSAEDQUOT,"Ran out of disk quota.");
	CASE_SOCK_ERROR(WSAESTALE,"File handle reference is no longer available.");
	CASE_SOCK_ERROR(WSAEREMOTE,"Item is not available locally.");
	CASE_SOCK_ERROR(WSASYSNOTREADY,"WSAStartup cannot function at this time because the underlying system it uses to provide network services is currently unavailable.");
	CASE_SOCK_ERROR(WSAVERNOTSUPPORTED,"The Windows Sockets version requested is not supported.");
	CASE_SOCK_ERROR(WSANOTINITIALISED,"Either the application has not called WSAStartup, or WSAStartup failed.");
	CASE_SOCK_ERROR(WSAEDISCON,"Returned by WSARecv or WSARecvFrom to indicate the remote party has initiated a graceful shutdown sequence.");
	CASE_SOCK_ERROR(WSAENOMORE,"No more results can be returned by WSALookupServiceNext.");
	CASE_SOCK_ERROR(WSAECANCELLED,"A call to WSALookupServiceEnd was made while this call was still processing. The call has been canceled.");
	CASE_SOCK_ERROR(WSAEINVALIDPROCTABLE,"The procedure call table is invalid.");
	CASE_SOCK_ERROR(WSAEINVALIDPROVIDER,"The requested service provider is invalid.");
	CASE_SOCK_ERROR(WSAEPROVIDERFAILEDINIT,"The requested service provider could not be loaded or initialized.");
	CASE_SOCK_ERROR(WSASYSCALLFAILURE,"A system call that should never fail has failed.");
	CASE_SOCK_ERROR(WSASERVICE_NOT_FOUND,"No such service is known. The service cannot be found in the specified name space.");
	CASE_SOCK_ERROR(WSATYPE_NOT_FOUND,"The specified class was not found.");
	CASE_SOCK_ERROR(WSA_E_NO_MORE,"No more results can be returned by WSALookupServiceNext.");
	CASE_SOCK_ERROR(WSA_E_CANCELLED,"A call to WSALookupServiceEnd was made while this call was still processing. The call has been canceled.");
	CASE_SOCK_ERROR(WSAEREFUSED,"A database query failed because it was actively refused.");
	CASE_SOCK_ERROR(WSAHOST_NOT_FOUND,"No such host is known.");
	CASE_SOCK_ERROR(WSATRY_AGAIN,"This is usually a temporary error during hostname resolution and means that the local server did not receive a response from an authoritative server.");
	CASE_SOCK_ERROR(WSANO_RECOVERY,"A non-recoverable error occurred during a database lookup.");
	CASE_SOCK_ERROR(WSANO_DATA,"The requested name is valid, but no data of the requested type was found.");
	CASE_SOCK_ERROR(WSA_QOS_RECEIVERS,"At least one reserve has arrived.");
	CASE_SOCK_ERROR(WSA_QOS_SENDERS,"At least one path has arrived.");
	CASE_SOCK_ERROR(WSA_QOS_NO_SENDERS,"There are no senders.");
	CASE_SOCK_ERROR(WSA_QOS_NO_RECEIVERS,"There are no receivers.");
	CASE_SOCK_ERROR(WSA_QOS_REQUEST_CONFIRMED,"Reserve has been confirmed.");
	CASE_SOCK_ERROR(WSA_QOS_ADMISSION_FAILURE,"Error due to lack of resources.");
	CASE_SOCK_ERROR(WSA_QOS_POLICY_FAILURE,"Rejected for administrative reasons - bad credentials.");
	CASE_SOCK_ERROR(WSA_QOS_BAD_STYLE,"Unknown or conflicting style.");
	CASE_SOCK_ERROR(WSA_QOS_BAD_OBJECT,"Problem with some part of the filterspec or providerspecific buffer in general.");
	CASE_SOCK_ERROR(WSA_QOS_TRAFFIC_CTRL_ERROR,"Problem with some part of the flowspec.");
	CASE_SOCK_ERROR(WSA_QOS_GENERIC_ERROR,"General QOS error.");
	CASE_SOCK_ERROR(WSA_QOS_ESERVICETYPE,"An invalid or unrecognized service type was found in the flowspec.");
	CASE_SOCK_ERROR(WSA_QOS_EFLOWSPEC,"An invalid or inconsistent flowspec was found in the QOS structure.");
	CASE_SOCK_ERROR(WSA_QOS_EPROVSPECBUF,"Invalid QOS provider-specific buffer.");
	CASE_SOCK_ERROR(WSA_QOS_EFILTERSTYLE,"An invalid QOS filter style was used.");
	CASE_SOCK_ERROR(WSA_QOS_EFILTERTYPE,"An invalid QOS filter type was used.");
	CASE_SOCK_ERROR(WSA_QOS_EFILTERCOUNT,"An incorrect number of QOS FILTERSPECs were specified in the FLOWDESCRIPTOR.");
	CASE_SOCK_ERROR(WSA_QOS_EOBJLENGTH,"An object with an invalid ObjectLength field was specified in the QOS provider-specific buffer.");
	CASE_SOCK_ERROR(WSA_QOS_EFLOWCOUNT,"An incorrect number of flow descriptors was specified in the QOS structure.");
	CASE_SOCK_ERROR(WSA_QOS_EUNKOWNPSOBJ,"An unrecognized object was found in the QOS provider-specific buffer.");
	CASE_SOCK_ERROR(WSA_QOS_EPOLICYOBJ,"An invalid policy object was found in the QOS provider-specific buffer.");
	CASE_SOCK_ERROR(WSA_QOS_EFLOWDESC,"An invalid QOS flow descriptor was found in the flow descriptor list.");
	CASE_SOCK_ERROR(WSA_QOS_EPSFLOWSPEC,"An invalid or inconsistent flowspec was found in the QOS provider specific buffer.");
	CASE_SOCK_ERROR(WSA_QOS_EPSFILTERSPEC,"An invalid FILTERSPEC was found in the QOS provider-specific buffer.");
	CASE_SOCK_ERROR(WSA_QOS_ESDMODEOBJ,"An invalid shape discard mode object was found in the QOS provider specific buffer.");
	CASE_SOCK_ERROR(WSA_QOS_ESHAPERATEOBJ,"An invalid shaping rate object was found in the QOS provider-specific buffer.");
	CASE_SOCK_ERROR(WSA_QOS_RESERVED_PETYPE,"A reserved policy element was found in the QOS provider-specific buffer.");
	}
	return str;
}

#endif
/************************************************************************/
/*                                                                      */
/************************************************************************/
#ifdef GAME_TIME

DWORD GetFullMinute(DWORD year, DWORD month, DWORD day, DWORD hour, DWORD minute)
{
	SYSTEMTIME st={year,month,0,day,hour,minute,0,0};
	FILETIMELI ft;
	if(!SystemTimeToFileTime(&st,&ft.ft)) WriteLog(__FUNCTION__" - Error<%u>, args<%u,%u,%u,%u,%u>.\n",GetLastError(),year,month,day,hour,minute);
	return DWORD(ft.ul.QuadPart/600000000);
}

SYSTEMTIME GetGameTime(DWORD full_minute)
{
	SYSTEMTIME st;
	FILETIMELI ft;
	ft.ul.QuadPart=__int64(full_minute)*600000000;
	if(!FileTimeToSystemTime(&ft.ft,&st)) WriteLog(__FUNCTION__" - Error<%u>, full minute<%u>.\n",GetLastError(),full_minute);
	return st;
}

DWORD GameTimeMonthDay(WORD year, WORD month)
{
	switch(month)
	{
	case 1:case 3:case 5:case 7:case 8:case 10:case 12: // 31
		return 31;
	case 2: // 28-29
		if(year%4) return 28;
		return 29;
	default: // 30
		return 30;
	}
	return 0;
}

#endif

GameOptions GameOpt;
GameOptions::GameOptions()
{
	Year=2246;
	Month=1;
	Day=2;
	Hour=14;
	Minute=5;
	FullMinute=0;
	FullMinuteTick=0;
	TimeMultiplier=0;

	DisableTcpNagle=false;
	DisableZlibCompression=false;
	FloodSize=2048;
	FreeExp=false;
	RegulatePvP=false;
	NoAnswerShuffle=false;
	ForceDialog=0;
	DialogDemandRecheck=false;
	FixBoyDefaultExperience=50;
	SneakDivider=6;
	LevelCap=99;
	LevelCapAddExperience=false;
	LookNormal=20;
	LookMinimum=6;
	GlobalMapMaxGroupCount=10;
	CritterIdleTick=10000;
	TurnBasedTick=30000;
	DeadHitPoints=-6;

	Breaktime=1200;
	TimeoutTransfer=3;
	TimeoutBattle=10;
	ApRegeneration=10000;
	RtApCostCritterWalk=0;
	RtApCostCritterRun=1;
	RtApCostMoveItemContainer=0;
	RtApCostMoveItemInventory=2;
	RtApCostPickItem=1;
	RtApCostDropItem=1;
	RtApCostReloadWeapon=2;
	RtApCostPickCritter=1;
	RtApCostUseItem=3;
	RtApCostUseSkill=2;
	RtAlwaysRun=false;
	TbApCostCritterMove=1;
	TbApCostMoveItemContainer=0;
	TbApCostMoveItemInventory=1;
	TbApCostPickItem=3;
	TbApCostDropItem=1;
	TbApCostReloadWeapon=2;
	TbApCostPickCritter=3;
	TbApCostUseItem=3;
	TbApCostUseSkill=3;
	TbAlwaysRun=false;
	ApCostAimEyes=1;
	ApCostAimHead=1;
	ApCostAimGroin=1;
	ApCostAimTorso=1;
	ApCostAimArms=1;
	ApCostAimLegs=1;
	HitAimEyes=60;
	HitAimHead=40;
	HitAimGroin=30;
	HitAimTorso=0;
	HitAimArms=30;
	HitAimLegs=20;
	RunOnCombat=false;
	RunOnTransfer=false;
	GlobalMapWidth=28;
	GlobalMapHeight=30;
	GlobalMapZoneLength=50;
	BagRefreshTime=60;
	AttackAnimationsMinDist=0;
	WhisperDist=2;
	LookChecks=0;
	LookDir[0]=0;
	LookDir[1]=20;
	LookDir[2]=40;
	LookDir[3]=60;
	LookSneakDir[0]=90;
	LookSneakDir[1]=60;
	LookSneakDir[2]=30;
	LookSneakDir[3]=0;
	LookWeight=200;
	CustomItemCost=false;
	RegistrationTimeout=5;
	AccountPlayTime=0;
	LoggingVars=false;
	SkipScriptBinaries=false;
	ScriptRunSuspendTimeout=30000;
	ScriptRunMessageTimeout=10000;
	TalkDistance=3;
	MinNameLength=4;
	MaxNameLength=12;

	AbsoluteOffsets=true;
	SkillBegin=200;
	SkillEnd=217;
	TimeoutBegin=230;
	TimeoutEnd=249;
	KillBegin=260;
	KillEnd=278;
	PerkBegin=300;
	PerkEnd=435;
	AddictionBegin=470;
	AddictionEnd=476;
	KarmaBegin=480;
	KarmaEnd=495;
	DamageBegin=500;
	DamageEnd=506;
	TraitBegin=550;
	TraitEnd=565;
	ReputationBegin=570;
	ReputationEnd=599;

	ReputationLoved=30;
	ReputationLiked=15;
	ReputationAccepted=1;
	ReputationNeutral=0;
	ReputationAntipathy=-14;
	ReputationHated=-29;

	// Client
	DisableDrawScreens=false;
	UserInterface="";
	Animation3dSmoothTime=250;
	Animation3dFPS=0;
	RunModMul=1;
	RunModDiv=3;
	RunModAdd=0;
}

DWORD GameAimApCost(int hit_location)
{
	switch(hit_location)
	{
	default: break;
	case HIT_LOCATION_NONE: break;
	case HIT_LOCATION_UNCALLED: break;
	case HIT_LOCATION_TORSO: return GameOpt.ApCostAimTorso;
	case HIT_LOCATION_EYES: return GameOpt.ApCostAimEyes;
	case HIT_LOCATION_HEAD: return GameOpt.ApCostAimHead;
	case HIT_LOCATION_LEFT_ARM:
	case HIT_LOCATION_RIGHT_ARM: return GameOpt.ApCostAimArms;
	case HIT_LOCATION_GROIN: return GameOpt.ApCostAimGroin;
	case HIT_LOCATION_RIGHT_LEG:
	case HIT_LOCATION_LEFT_LEG: return GameOpt.ApCostAimLegs;
	}
	return 0;
}

DWORD GameHitAim(int hit_location)
{
	switch(hit_location)
	{
	default: break;
	case HIT_LOCATION_NONE: break;
	case HIT_LOCATION_UNCALLED: break;
	case HIT_LOCATION_TORSO: return GameOpt.HitAimTorso;
	case HIT_LOCATION_EYES: return GameOpt.HitAimEyes;
	case HIT_LOCATION_HEAD: return GameOpt.HitAimHead;
	case HIT_LOCATION_LEFT_ARM:
	case HIT_LOCATION_RIGHT_ARM: return GameOpt.HitAimArms;
	case HIT_LOCATION_GROIN: return GameOpt.HitAimGroin;
	case HIT_LOCATION_RIGHT_LEG:
	case HIT_LOCATION_LEFT_LEG: return GameOpt.HitAimLegs;
	}
	return 0;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

FileLogger::FileLogger(const char* fname)
{
	logFile=NULL;
	fopen_s(&logFile,fname,"wt");
	startTick=Timer::FastTick();
}

FileLogger::~FileLogger()
{
	if(logFile)
	{
		fclose(logFile);
		logFile=NULL;
	}
}

void FileLogger::Write(const char* fmt, ...)
{
	if(logFile)
	{
		fprintf(logFile,"%10u) ",(Timer::FastTick()-startTick)/1000);
		va_list list;
		va_start(list,fmt);
		vfprintf(logFile,fmt,list);
		va_end(list);
	}
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

void StringCopy(char* to, size_t size, const char* from)
{
	if(!to) return;
	
	if(!from)
	{
		to[0]=0;
		return;
	}

	size_t from_len=strlen(from);
	if(!from_len)
	{
		to[0]=0;
		return;
	}

	if(from_len>=size)
	{
		memcpy(to,from,size-1);
		to[size-1]=0;
	}
	else
	{
		memcpy(to,from,from_len);
		to[from_len]=0;
	}
}

void StringAppend(char* to, size_t size, const char* from)
{
	if(!to || !from) return;

	size_t from_len=strlen(from);
	if(!from_len) return;

	size_t to_len=strlen(to);
	size_t to_free=size-to_len;
	if((int)to_free<=1) return;

	char* ptr=to+to_len;

	if(from_len>=to_free)
	{
		memcpy(ptr,from,to_free-1);
		to[size-1]=0;
	}
	else
	{
		memcpy(ptr,from,from_len);
		ptr[from_len]=0;
	}
}

char* StringDuplicate(const char* str)
{
	if(!str) return NULL;
	size_t len=strlen(str);
	char* dup=new(nothrow) char[len+1];
	if(!dup) return NULL;
	if(len) memcpy(dup,str,len);
	dup[len]=0;
	return dup;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

// Deprecated stuff
#include <Item.h>
#include <FileManager.h>
#include <Text.h>

bool ListsLoaded=false;
PCharVec LstNames[PATH_LIST_COUNT];

void LoadList(const char* lst_name, int path_type)
{
	FileManager fm;
	if(!fm.LoadFile(lst_name)) return;

	char str[1024];
	DWORD str_cnt=0;
	const char* path=FileManager::GetPath(path_type);

	PCharVec& lst=LstNames[path_type];
	for(size_t i=0,j=lst.size();i<j;i++) SAFEDELA(lst[i]);
	lst.clear();

	while(fm.GetLine(str,1023))
	{
		// Lower text
		_strlwr_s(str);

		// Skip comments
		if(!strlen(str) || str[0]=='#' || str[0]==';') continue;

		// New value of line
		if(str[0]=='*')
		{
			str_cnt=atoi(&str[1]);
			continue;
		}

		// Find ext
		char* ext=(char*)FileManager::GetExtension(str);
		if(!ext)
		{
			str_cnt++;
			WriteLog(__FUNCTION__" - Extension not found in line<%s>, skip.\n",str);
			continue;
		}

		// Cut off comments
		int j=0;
		while(ext[j] && ext[j]!=' ') j++;
		ext[j]='\0';

		// Create name
		size_t len=strlen(path)+strlen(str)+1;
		char* rec=new char[len];
		strcpy_s(rec,len,path);
		strcat_s(rec,len,str);

		// Check for size
		if(str_cnt>=lst.size()) lst.resize(str_cnt+1);

		// Add
		lst[str_cnt]=rec;
		str_cnt++;
	}
}

string GetPicName(DWORD lst_num, WORD pic_num)
{
	if(pic_num>=LstNames[lst_num].size() || !LstNames[lst_num][pic_num]) return "";
	return string(LstNames[lst_num][pic_num]);
}

string Deprecated_GetPicName(int pid, int type, WORD pic_num)
{
	if(!ListsLoaded)
	{
		LoadList(".\\data\\deprecated_lists\\tiles.lst",PT_ART_TILES);
		LoadList(".\\data\\deprecated_lists\\items.lst",PT_ART_ITEMS);
		LoadList(".\\data\\deprecated_lists\\scenery.lst",PT_ART_SCENERY);
		LoadList(".\\data\\deprecated_lists\\walls.lst",PT_ART_WALLS);
		LoadList(".\\data\\deprecated_lists\\misc.lst",PT_ART_MISC);
		LoadList(".\\data\\deprecated_lists\\intrface.lst",PT_ART_INTRFACE);
		LoadList(".\\data\\deprecated_lists\\inven.lst",PT_ART_INVEN);
		ListsLoaded=true;
	}

	if(pid==-1) return GetPicName(PT_ART_INTRFACE,pic_num);
	if(pid==-2) return GetPicName(PT_ART_TILES,pic_num);
	if(pid==-3) return GetPicName(PT_ART_INVEN,pic_num);

	if(type==ITEM_TYPE_DOOR) return GetPicName(PT_ART_SCENERY,pic_num); // For doors from Scenery
	else if(pid==SP_MISC_SCRBLOCK || SP_MISC_GRID_MAP(pid) || SP_MISC_GRID_GM(pid)) return GetPicName(PT_ART_MISC,pic_num); // For exit grids from Misc
	else if(pid>=F2PROTO_OFFSET_MISC && pid<=F2PROTO_OFFSET_MISC+MISC_MAX) return GetPicName(PT_ART_MISC,pic_num); // From Misc
	else if(type>=ITEM_TYPE_ARMOR && type<=ITEM_TYPE_DOOR) return GetPicName(PT_ART_ITEMS,pic_num); // From Items
	else if(type==ITEM_TYPE_GENERIC || type==ITEM_TYPE_GRID) return GetPicName(PT_ART_SCENERY,pic_num); // From Scenery
	else if(type==ITEM_TYPE_WALL) return GetPicName(PT_ART_WALLS,pic_num); // From Walls
	return "";
}

DWORD Deprecated_GetPicHash(int pid, int type, WORD pic_num)
{
	string name=Deprecated_GetPicName(pid,type,pic_num);
	if(!name.length()) return 0;
	return Str::GetHash(name.c_str());
}


