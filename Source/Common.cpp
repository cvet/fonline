#include "StdAfx.h"
#include "Common.h"
#include "Exception.h"
#include "Crypt.h"
#include "Script.h"
#include "Text.h"
#include <math.h>
#include "FileManager.h"

/************************************************************************/
/*                                                                      */
/************************************************************************/

Randomizer DefaultRandomizer;
int Random(int minimum, int maximum)
{
	return DefaultRandomizer.Random(minimum,maximum);
}

int Procent(int full, int peace)
{
	if(!full) return 0;
	int procent=peace*100/full;
	return CLAMP(procent,0,100);
}

DWORD DistSqrt(int x1, int y1, int x2, int y2)
{
	int dx=x1-x2;
	int dy=y1-y2;
	return (DWORD)sqrt(double(dx*dx+dy*dy));
}

DWORD DistGame(int x1, int y1, int x2, int y2)
{
	int dx=(x1>x2?x1-x2:x2-x1);
	if(!(x1&1))
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
	if(num&1)
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

	if(x1&1)
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
	float ny=(ty-hy)*SQRT3T2_FLOAT-(float(x2&1)-float(x1&1))*SQRT3_FLOAT;
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
		if(!(x1&1)) y1--;
		break;
	case 1:
		x1--;
		if(x1&1) y1++;
		break;
	case 2:
		y1++;
		break;
	case 3:
		x1++;
		if(x1&1) y1++;
		break;
	case 4:
		x1++;
		if(!(x1&1)) y1--;
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
	float dx=abs(x2-x1);
	float dy=abs(y2-y1);

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

bool MoveHexByDir(WORD& hx, WORD& hy, BYTE dir, WORD maxhx, WORD maxhy)
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
		return false;
	}

	if(hx_>=0 && hx_<maxhx && hy_>=0 && hy_<maxhy)
	{
		hx=hx_;
		hy=hy_;
		return true;
	}
	return false;
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

void RestoreMainDirectory()
{
	// Get executable file path
	char path[MAX_FOPATH]={0};
	GetModuleFileName(GetModuleHandle(NULL),path,MAX_FOPATH);

	// Cut off executable name
	int last=0;
	for(int i=0;path[i];i++)
		if(path[i]=='\\') last=i;
	path[last+1]=0;

	// Set executable directory
	SetCurrentDirectory(path);
}

/************************************************************************/
/* Hex offsets                                                          */
/************************************************************************/

short SXEven[HEX_OFFSET_SIZE]={0};
short SYEven[HEX_OFFSET_SIZE]={0};
short SXOdd[HEX_OFFSET_SIZE]={0};
short SYOdd[HEX_OFFSET_SIZE]={0};

class InitializeOffsets
{
public:
	InitializeOffsets()
	{
		int pos=0;
		int xe=0,ye=0,xo=1,yo=0;
		for(int i=0;i<MAX_HEX_OFFSET;i++)
		{
			MoveHexByDirUnsafe(xe,ye,0);
			MoveHexByDirUnsafe(xo,yo,0);

			for(int j=0;j<6;j++)
			{
				int dir=(j+2)%6;
				for(int k=0;k<i+1;k++)
				{
					SXEven[pos]=xe;
					SYEven[pos]=ye;
					SXOdd[pos]=xo-1;
					SYOdd[pos]=yo;
					pos++;
					MoveHexByDirUnsafe(xe,ye,dir);
					MoveHexByDirUnsafe(xo,yo,dir);
				}
			}
		}
	}
} InitializeOffsets_;

/************************************************************************/
/* True chars                                                           */
/************************************************************************/

bool NameTrueChar[0x100];
bool PassTrueChar[0x100];

class InitializeTrueChar
{
public:
	InitializeTrueChar()
	{
		for(int i=0;i<0x100;i++) NameTrueChar[i]=false;
		for(int i=0;i<0x100;i++) PassTrueChar[i]=false;
		const BYTE name_char[]=" _.-1234567890AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZzÀàÁáÂâÃãÄäÅå¨¸ÆæÇçÈèÉéÊêËëÌìÍíÎîÏïÐðÑñÒòÓóÔôÕõÖö×÷ØøÙùÚúÛûÜüÝýÞþßÿ";
		const BYTE pass_char[]=" _-,.=[]{}?!@#$^&()|`~:;1234567890AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZzÀàÁáÂâÃãÄäÅå¨¸ÆæÇçÈèÉéÊêËëÌìÍíÎîÏïÐðÑñÒòÓóÔôÕõÖö×÷ØøÙùÚúÛûÜüÝýÞþßÿ";
		for(int i=0,j=sizeof(name_char);i<j;i++) NameTrueChar[name_char[i]]=true;
		for(int i=0,j=sizeof(pass_char);i<j;i++) PassTrueChar[pass_char[i]]=true;
	}
} InitializeTrueChar_;

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
	// Defines
#define GETOPTIONS_CMD_LINE_INT(opt,str_id) do{char* str=strstr(GetCommandLine(),str_id); if(str) opt=atoi(str+strlen(str_id)+1);}while(0)
#define GETOPTIONS_CMD_LINE_BOOL(opt,str_id) do{char* str=strstr(GetCommandLine(),str_id); if(str) opt=atoi(str+strlen(str_id)+1)!=0;}while(0)
#define GETOPTIONS_CMD_LINE_BOOL_ON(opt,str_id) do{char* str=strstr(GetCommandLine(),str_id); if(str) opt=true;}while(0)
#define GETOPTIONS_CMD_LINE_STR(opt,str_id) do{char* str=strstr(GetCommandLine(),str_id); if(str) sscanf_s(str+strlen(str_id)+1,"%s",opt);}while(0)
#define GETOPTIONS_CHECK(val_,min_,max_,def_) do{if(val_<min_ || val_>max_) val_=def_;} while(0)

	char buf[MAX_FOTEXT];

	// Load config file
#ifdef FONLINE_MAPPER
	IniParser cfg_mapper;
	cfg_mapper.LoadFile(MAPPER_CONFIG_FILE,PT_ROOT);

	cfg_mapper.GetStr("ClientPath","",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"-ClientPath");
	GameOpt.ClientPath=buf;
	if(GameOpt.ClientPath.length() && GameOpt.ClientPath[GameOpt.ClientPath.length()-1]!='\\') GameOpt.ClientPath+="\\";
	cfg_mapper.GetStr("ServerPath","",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"-ServerPath");
	GameOpt.ServerPath=buf;
	if(GameOpt.ServerPath.length() && GameOpt.ServerPath[GameOpt.ServerPath.length()-1]!='\\') GameOpt.ServerPath+="\\";

	FileManager::SetDataPath(GameOpt.ClientPath.c_str());
#endif

	IniParser cfg;
	cfg.LoadFile(CLIENT_CONFIG_FILE,PT_ROOT);

	// Language
	cfg.GetStr(CLIENT_CONFIG_APP,"Language","russ",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"Language");
	if(!strcmp(buf,"russ")) SetExceptionsRussianText();

	// Int
	GameOpt.FullScreen=cfg.GetInt(CLIENT_CONFIG_APP,"FullScreen",false)!=0;
	GETOPTIONS_CMD_LINE_BOOL(GameOpt.FullScreen,"-FullScreen");
	GameOpt.VSync=cfg.GetInt(CLIENT_CONFIG_APP,"VSync",false)!=0;
	GETOPTIONS_CMD_LINE_BOOL(GameOpt.VSync,"-VSync");
	GameOpt.ScreenClear=cfg.GetInt(CLIENT_CONFIG_APP,"BackGroundClear",false);
	GETOPTIONS_CMD_LINE_INT(GameOpt.ScreenClear,"-BackGroundClear");
	GETOPTIONS_CHECK(GameOpt.ScreenClear,0,1,0);
	GameOpt.Light=cfg.GetInt(CLIENT_CONFIG_APP,"Light",20);
	GETOPTIONS_CMD_LINE_INT(GameOpt.Light,"-Light");
	GETOPTIONS_CHECK(GameOpt.Light,0,100,20);
	GameOpt.ScrollDelay=cfg.GetInt(CLIENT_CONFIG_APP,"ScrollDelay",4);
	GETOPTIONS_CMD_LINE_INT(GameOpt.ScrollDelay,"-ScrollDelay");
	GETOPTIONS_CHECK(GameOpt.ScrollDelay,1,32,4);
	GameOpt.ScrollStep=cfg.GetInt(CLIENT_CONFIG_APP,"ScrollStep",32);
	GETOPTIONS_CMD_LINE_INT(GameOpt.ScrollStep,"-ScrollStep");
	GETOPTIONS_CHECK(GameOpt.ScrollStep,4,32,32);
	GameOpt.MouseSpeed=cfg.GetInt(CLIENT_CONFIG_APP,"MouseSpeed",100);
	GETOPTIONS_CMD_LINE_INT(GameOpt.MouseSpeed,"-MouseSpeed");
	GETOPTIONS_CHECK(GameOpt.MouseSpeed,10,1000,100);
	GameOpt.TextDelay=cfg.GetInt(CLIENT_CONFIG_APP,"TextDelay",3000);
	GETOPTIONS_CMD_LINE_INT(GameOpt.TextDelay,"-TextDelay");
	GETOPTIONS_CHECK(GameOpt.TextDelay,1000,3000,30000);
	GameOpt.DamageHitDelay=cfg.GetInt(CLIENT_CONFIG_APP,"DamageHitDelay",0);
	GETOPTIONS_CMD_LINE_INT(GameOpt.DamageHitDelay,"-OptDamageHitDelay");
	GETOPTIONS_CHECK(GameOpt.DamageHitDelay,0,30000,0);
	GameOpt.ScreenWidth=cfg.GetInt(CLIENT_CONFIG_APP,"ScreenWidth",0);
	GETOPTIONS_CMD_LINE_INT(GameOpt.ScreenWidth,"-ScreenWidth");
	GETOPTIONS_CHECK(GameOpt.ScreenWidth,100,30000,800);
	GameOpt.ScreenHeight=cfg.GetInt(CLIENT_CONFIG_APP,"ScreenHeight",0);
	GETOPTIONS_CMD_LINE_INT(GameOpt.ScreenHeight,"-ScreenHeight");
	GETOPTIONS_CHECK(GameOpt.ScreenHeight,100,30000,600);
	GameOpt.MultiSampling=cfg.GetInt(CLIENT_CONFIG_APP,"MultiSampling",0);
	GETOPTIONS_CMD_LINE_INT(GameOpt.MultiSampling,"-MultiSampling");
	GETOPTIONS_CHECK(GameOpt.MultiSampling,-1,16,-1);
	GameOpt.SoftwareSkinning=cfg.GetInt(CLIENT_CONFIG_APP,"SoftwareSkinning",1)!=0;
	GETOPTIONS_CMD_LINE_BOOL(GameOpt.SoftwareSkinning,"-SoftwareSkinning");
	GameOpt.AlwaysOnTop=cfg.GetInt(CLIENT_CONFIG_APP,"AlwaysOnTop",false)!=0;
	GETOPTIONS_CMD_LINE_BOOL(GameOpt.AlwaysOnTop,"-AlwaysOnTop");
	GameOpt.FlushVal=cfg.GetInt(CLIENT_CONFIG_APP,"FlushValue",50);
	GETOPTIONS_CMD_LINE_INT(GameOpt.FlushVal,"-FlushValue");
	GETOPTIONS_CHECK(GameOpt.FlushVal,1,1000,50);
	GameOpt.BaseTexture=cfg.GetInt(CLIENT_CONFIG_APP,"BaseTexture",1024);
	GETOPTIONS_CMD_LINE_INT(GameOpt.BaseTexture,"-BaseTexture");
	GETOPTIONS_CHECK(GameOpt.BaseTexture,128,8192,1024);
	GameOpt.Sleep=cfg.GetInt(CLIENT_CONFIG_APP,"Sleep",0);
	GETOPTIONS_CMD_LINE_INT(GameOpt.Sleep,"-Sleep");
	GETOPTIONS_CHECK(GameOpt.Sleep,-1,100,0);
	GameOpt.MsgboxInvert=cfg.GetInt(CLIENT_CONFIG_APP,"InvertMessBox",false)!=0;
	GETOPTIONS_CMD_LINE_BOOL(GameOpt.MsgboxInvert,"-InvertMessBox");
	GameOpt.ChangeLang=cfg.GetInt(CLIENT_CONFIG_APP,"LangChange",CHANGE_LANG_CTRL_SHIFT);
	GETOPTIONS_CMD_LINE_INT(GameOpt.ChangeLang,"-LangChange");
	GETOPTIONS_CHECK(GameOpt.ChangeLang,0,1,0);
	GameOpt.MessNotify=cfg.GetInt(CLIENT_CONFIG_APP,"WinNotify",true)!=0;
	GETOPTIONS_CMD_LINE_BOOL(GameOpt.MessNotify,"-WinNotify");
	GameOpt.SoundNotify=cfg.GetInt(CLIENT_CONFIG_APP,"SoundNotify",false)!=0;
	GETOPTIONS_CMD_LINE_BOOL(GameOpt.SoundNotify,"-SoundNotify");
	GameOpt.Port=cfg.GetInt(CLIENT_CONFIG_APP,"RemotePort",4000);
	GETOPTIONS_CMD_LINE_INT(GameOpt.Port,"-RemotePort");
	GETOPTIONS_CHECK(GameOpt.Port,0,0xFFFF,4000);
	GameOpt.ProxyType=cfg.GetInt(CLIENT_CONFIG_APP,"ProxyType",0);
	GETOPTIONS_CMD_LINE_INT(GameOpt.ProxyType,"-ProxyType");
	GETOPTIONS_CHECK(GameOpt.ProxyType,0,3,0);
	GameOpt.ProxyPort=cfg.GetInt(CLIENT_CONFIG_APP,"ProxyPort",1080);
	GETOPTIONS_CMD_LINE_INT(GameOpt.ProxyPort,"-ProxyPort");
	GETOPTIONS_CHECK(GameOpt.ProxyPort,0,0xFFFF,1080);
	GameOpt.GlobalSound=cfg.GetInt(CLIENT_CONFIG_APP,"GlobalSound",true)!=0;
	GETOPTIONS_CMD_LINE_BOOL(GameOpt.GlobalSound,"-GlobalSound");
	GameOpt.AlwaysRun=cfg.GetInt(CLIENT_CONFIG_APP,"AlwaysRun",false)!=0;
	GETOPTIONS_CMD_LINE_BOOL(GameOpt.AlwaysRun,"-AlwaysRun");
	GameOpt.DefaultCombatMode=cfg.GetInt(CLIENT_CONFIG_APP,"DefaultCombatMode",COMBAT_MODE_ANY);
	GETOPTIONS_CMD_LINE_INT(GameOpt.DefaultCombatMode,"-DefaultCombatMode");
	GETOPTIONS_CHECK(GameOpt.DefaultCombatMode,COMBAT_MODE_ANY,COMBAT_MODE_TURN_BASED,COMBAT_MODE_ANY);
	GameOpt.IndicatorType=cfg.GetInt(CLIENT_CONFIG_APP,"IndicatorType",COMBAT_MODE_ANY);
	GETOPTIONS_CMD_LINE_INT(GameOpt.IndicatorType,"-IndicatorType");
	GETOPTIONS_CHECK(GameOpt.IndicatorType,INDICATOR_LINES,INDICATOR_BOTH,INDICATOR_LINES);
	GameOpt.DoubleClickTime=cfg.GetInt(CLIENT_CONFIG_APP,"DoubleClickTime",COMBAT_MODE_ANY);
	GETOPTIONS_CMD_LINE_INT(GameOpt.DoubleClickTime,"-DoubleClickTime");
	GETOPTIONS_CHECK(GameOpt.DoubleClickTime,0,1000,0);
	if(!GameOpt.DoubleClickTime) GameOpt.DoubleClickTime=GetDoubleClickTime();
	GameOpt.CombatMessagesType=cfg.GetInt(CLIENT_CONFIG_APP,"CombatMessagesType",0);
	GETOPTIONS_CMD_LINE_INT(GameOpt.CombatMessagesType,"-CombatMessagesType");
	GETOPTIONS_CHECK(GameOpt.CombatMessagesType,0,1,0);
	GameOpt.Animation3dFPS=cfg.GetInt(CLIENT_CONFIG_APP,"Animation3dFPS",10);
	GETOPTIONS_CMD_LINE_INT(GameOpt.Animation3dFPS,"-Animation3dFPS");
	GETOPTIONS_CHECK(GameOpt.Animation3dFPS,0,1000,10);
	GameOpt.Animation3dSmoothTime=cfg.GetInt(CLIENT_CONFIG_APP,"Animation3dSmoothTime",0);
	GETOPTIONS_CMD_LINE_INT(GameOpt.Animation3dSmoothTime,"-Animation3dSmoothTime");
	GETOPTIONS_CHECK(GameOpt.Animation3dSmoothTime,0,10000,250);

	GETOPTIONS_CMD_LINE_BOOL_ON(GameOpt.HelpInfo,"-HelpInfo");
	GETOPTIONS_CMD_LINE_BOOL_ON(GameOpt.DebugInfo,"-DebugInfo");
	GETOPTIONS_CMD_LINE_BOOL_ON(GameOpt.DebugNet,"-DebugNet");
	GETOPTIONS_CMD_LINE_BOOL_ON(GameOpt.DebugSprites,"-DebugSprites");

	// Str
	cfg.GetStr(CLIENT_CONFIG_APP,"MasterDatPath","master.dat",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"-MasterDatPath");
	GameOpt.MasterPath=buf;
	cfg.GetStr(CLIENT_CONFIG_APP,"CritterDatPath","critter.dat",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"-CritterDatPath");
	GameOpt.CritterPath=buf;
	cfg.GetStr(CLIENT_CONFIG_APP,"FonlineDataPath",".\\data",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"-FonlineDataPath");
	GameOpt.FoDataPath=buf;
	cfg.GetStr(CLIENT_CONFIG_APP,"RemoteHost","localhost",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"-RemoteHost");
	GameOpt.Host=buf;
	cfg.GetStr(CLIENT_CONFIG_APP,"ProxyHost","localhost",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"-ProxyHost");
	GameOpt.ProxyHost=buf;
	cfg.GetStr(CLIENT_CONFIG_APP,"ProxyUser","",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"-ProxyUser");
	GameOpt.ProxyUser=buf;
	cfg.GetStr(CLIENT_CONFIG_APP,"ProxyPass","",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"-ProxyPass");
	GameOpt.ProxyPass=buf;
	cfg.GetStr(CLIENT_CONFIG_APP,"UserName","",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"-UserName");
	GameOpt.Name=buf;
	cfg.GetStr(CLIENT_CONFIG_APP,"UserPass","",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"-UserPass");
	GameOpt.Pass=buf;
	cfg.GetStr(CLIENT_CONFIG_APP,"KeyboardRemap","",buf);
	GETOPTIONS_CMD_LINE_STR(buf,"-KeyboardRemap");
	GameOpt.KeyboardRemap=buf;

	// Logging
	bool logging=cfg.GetInt(CLIENT_CONFIG_APP,"Logging",1)!=0;
	GETOPTIONS_CMD_LINE_BOOL(logging,"-Logging");
	if(!logging)
	{
		WriteLog("File logging off.\n");
		LogFinish(-1);
	}

	logging=cfg.GetInt(CLIENT_CONFIG_APP,"LoggingDebugOutput",0)!=0;
	GETOPTIONS_CMD_LINE_BOOL(logging,"-LoggingDebugOutput");
	if(logging) LogToDebugOutput();

	logging=cfg.GetInt(CLIENT_CONFIG_APP,"LoggingTime",false)!=0;
	GETOPTIONS_CMD_LINE_BOOL(logging,"-LoggingTime");
	LogWithTime(logging);
	logging=cfg.GetInt(CLIENT_CONFIG_APP,"LoggingThread",false)!=0;
	GETOPTIONS_CMD_LINE_BOOL(logging,"-LoggingThread");
	LogWithThread(logging);

#ifdef FONLINE_CLIENT
	Script::SetGarbageCollectTime(120000);
#endif
#ifdef FONLINE_MAPPER
	Script::SetRunTimeout(0,0);
#endif
}

ClientScriptFunctions ClientFunctions;
MapperScriptFunctions MapperFunctions;
#endif
/************************************************************************/
/*                                                                      */
/************************************************************************/
#ifdef FONLINE_SERVER

volatile bool FOAppQuit=false;
volatile bool FOQuit=false;
HANDLE UpdateEvent=NULL;
HANDLE LogEvent=NULL;
int ServerGameSleep=10;
int MemoryDebugLevel=10;
DWORD VarsGarbageTime=3600000;
bool WorldSaveManager=true;
bool LogicMT=false;

void GetServerOptions()
{
	IniParser cfg;
	cfg.LoadFile(SERVER_CONFIG_FILE,PT_SERVER_ROOT);
	ServerGameSleep=cfg.GetInt("GameSleep",10);
	Script::SetConcurrentExecution(cfg.GetInt("ScriptConcurrentExecution",0)!=0);
	WorldSaveManager=(cfg.GetInt("WorldSaveManager",1)==1);
}

ServerScriptFunctions ServerFunctions;

#endif
/************************************************************************/
/*                                                                      */
/************************************************************************/
#if defined(FONLINE_CLIENT) || defined(FONLINE_SERVER)

const char* GetLastSocketError()
{
	static THREAD char str[256];
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

DWORD GetFullSecond(WORD year, WORD month, WORD day, WORD hour, WORD minute, WORD second)
{
	SYSTEMTIME st={year,month,0,day,hour,minute,second,0};
	FILETIMELI ft;
	if(!SystemTimeToFileTime(&st,&ft.ft)) WriteLog(__FUNCTION__" - Error<%u>, args<%u,%u,%u,%u,%u,%u>.\n",GetLastError(),year,month,day,hour,minute,second);
	ft.ul.QuadPart-=GameOpt.YearStartFT;
	return DWORD(ft.ul.QuadPart/10000000);
}

SYSTEMTIME GetGameTime(DWORD full_second)
{
	SYSTEMTIME st;
	FILETIMELI ft;
	ft.ul.QuadPart=GameOpt.YearStartFT+ULONGLONG(full_second)*10000000;
	if(!FileTimeToSystemTime(&ft.ft,&st)) WriteLog(__FUNCTION__" - Error<%u>, full second<%u>.\n",GetLastError(),full_second);
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

void ProcessGameTime()
{
	DWORD tick=Timer::GameTick();
	DWORD dt=tick-GameOpt.GameTimeTick;
	DWORD delta_second=dt/1000*GameOpt.TimeMultiplier+dt%1000*GameOpt.TimeMultiplier/1000;
	DWORD fs=GameOpt.FullSecondStart+delta_second;
	if(GameOpt.FullSecond!=fs)
	{
		GameOpt.FullSecond=fs;
		SYSTEMTIME st=GetGameTime(GameOpt.FullSecond);
		GameOpt.Year=st.wYear;
		GameOpt.Month=st.wMonth;
		GameOpt.Day=st.wDay;
		GameOpt.Hour=st.wHour;
		GameOpt.Minute=st.wMinute;
		GameOpt.Second=st.wSecond;
	}
}

#endif

GameOptions GameOpt;
GameOptions::GameOptions()
{
	YearStart=2246;
	Year=2246;
	Month=1;
	Day=2;
	Hour=14;
	Minute=5;
	Second=0;
	FullSecondStart=0;
	FullSecond=0;
	TimeMultiplier=0;
	GameTimeTick=0;

	DisableTcpNagle=false;
	DisableZlibCompression=false;
	FloodSize=2048;
	FreeExp=false;
	RegulatePvP=false;
	NoAnswerShuffle=false;
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
	RunOnCombat=false;
	RunOnTransfer=false;
	GlobalMapWidth=28;
	GlobalMapHeight=30;
	GlobalMapZoneLength=50;
	BagRefreshTime=60;
	AttackAnimationsMinDist=0;
	WhisperDist=2;
	ShoutDist=200;
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
	ScriptRunSuspendTimeout=30000;
	ScriptRunMessageTimeout=10000;
	TalkDistance=3;
	MinNameLength=4;
	MaxNameLength=12;
	DlgTalkMinTime=0;
	DlgBarterMinTime=0;

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

	// Client and Mapper
	Quit=false;
	MouseX=0;
	MouseY=0;
	ScrOx=0;
	ScrOy=0;
	ShowTile=true;
	ShowRoof=true;
	ShowItem=true;
	ShowScen=true;
	ShowWall=true;
	ShowCrit=true;
	ShowFast=true;
	ShowPlayerNames=false;
	ShowNpcNames=false;
	ShowCritId=false;
	ScrollKeybLeft=false;
	ScrollKeybRight=false;
	ScrollKeybUp=false;
	ScrollKeybDown=false;
	ScrollMouseLeft=false;
	ScrollMouseRight=false;
	ScrollMouseUp=false;
	ScrollMouseDown=false;
	ShowGroups=true;
	HelpInfo=false;
	DebugInfo=false;
	DebugNet=false;
	DebugSprites=false;
	FullScreen=false;
	VSync=false;
	FlushVal=256;
	BaseTexture=256;
	ScreenClear=0;
	Light=0;
	Host="localhost";
	HostRefCounter=1;
	Port=0;
	ProxyType=0;
	ProxyHost;
	ProxyHostRefCounter=1;
	ProxyPort=0;
	ProxyUser;
	ProxyUserRefCounter=1;
	ProxyPass;
	ProxyPassRefCounter=1;
	Name="";
	NameRefCounter=1;
	Pass;
	PassRefCounter=1;
	ScrollDelay=0;
	ScrollStep=1;
	ScrollCheck=true;
	MouseSpeed=100;
	GlobalSound=true;
	MasterPath="";
	MasterPathRefCounter=1;
	CritterPath="";
	CritterPathRefCounter=1;
	FoDataPath="";
	FoDataPathRefCounter=1;
	Sleep=0;
	MsgboxInvert=false;
	ChangeLang=CHANGE_LANG_CTRL_SHIFT;
	DefaultCombatMode=COMBAT_MODE_ANY;
	MessNotify=true;
	SoundNotify=true;
	AlwaysOnTop=false;
	TextDelay=3000;
	DamageHitDelay=0;
	ScreenWidth=800;
	ScreenHeight=600;
	MultiSampling=0;
	SoftwareSkinning=false;
	MouseScroll=true;
	IndicatorType=INDICATOR_LINES;
	DoubleClickTime=0;
	RoofAlpha=200;
	HideCursor=false;
	DisableLMenu=false;
	DisableMouseEvents=false;
	DisableKeyboardEvents=false;
	HidePassword=true;
	PlayerOffAppendix="_off";
	PlayerOffAppendixRefCounter=1;
	CombatMessagesType=0;
	DisableDrawScreens=false;
	Animation3dSmoothTime=250;
	Animation3dFPS=10;
	RunModMul=1;
	RunModDiv=3;
	RunModAdd=0;
	SpritesZoom=1.0f;
	SpritesZoomMax=MAX_ZOOM;
	SpritesZoomMin=MIN_ZOOM;
	ZeroMemory(EffectValues,sizeof(EffectValues));
	AlwaysRun=false;
	AlwaysRunMoveDist=1;
	AlwaysRunUseDist=5;
	KeyboardRemap="";
	KeyboardRemapRefCounter=1;
	CritterFidgetTime=50000;
	Anim2CombatBegin=0;
	Anim2CombatIdle=0;
	Anim2CombatEnd=0;

	// Mapper
	ClientPath=".\\";
	ClientPathRefCounter=1;
	ServerPath=".\\";
	ServerPathRefCounter=1;
	ShowCorners=false;
	ShowSpriteCuts=false;
	ShowDrawOrder=false;

	// Engine data
	CritterChangeParameter=NULL;
	CritterTypes=NULL;

	ClientMap=NULL;
	ClientMapWidth=0;
	ClientMapHeight=0;

	GetDrawingSprites=NULL;
	GetSpriteInfo=NULL;
	GetSpriteColor=NULL;
	IsSpriteHit=NULL;

	GetNameByHash=NULL;
	GetHashByName=NULL;

	// Callbacks
	GetUseApCost=NULL;
	GetAttackDistantion=NULL;
}

/************************************************************************/
/* File logger                                                          */
/************************************************************************/

FileLogger::FileLogger(const char* fname)
{
	logFile=NULL;
	fopen_s(&logFile,fname,"wb");
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
/* Safe string functions                                                */
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
/* Single player                                                        */
/************************************************************************/

#define INTERPROCESS_DATA_SIZE          (offsetof(InterprocessData,mapFileMutex))

HANDLE InterprocessData::Init()
{
	SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};
	mapFile=CreateFileMapping(INVALID_HANDLE_VALUE,&sa,PAGE_READWRITE,0,INTERPROCESS_DATA_SIZE+sizeof(mapFileMutex),NULL);
	if(!mapFile) return NULL;
	mapFilePtr=NULL;

	mapFileMutex=CreateMutex(&sa,FALSE,NULL);
	if(!mapFileMutex) return NULL;

	if(!Lock()) return NULL;
	ZeroMemory(this,INTERPROCESS_DATA_SIZE);
	((InterprocessData*)mapFilePtr)->mapFileMutex=mapFileMutex;
	Unlock();
	return mapFile;
}

void InterprocessData::Finish()
{
	if(mapFile) CloseHandle(mapFile);
	if(mapFileMutex) CloseHandle(mapFileMutex);
	mapFile=NULL;
	mapFilePtr=NULL;
}

bool InterprocessData::Attach(HANDLE map_file)
{
	if(!map_file) return false;
	mapFile=map_file;
	mapFilePtr=NULL;

	// Read mutex handle
	void* ptr=MapViewOfFile(mapFile,FILE_MAP_WRITE,0,0,0);
	if(!ptr) return false;
	mapFileMutex=((InterprocessData*)ptr)->mapFileMutex;
	UnmapViewOfFile(ptr);
	if(!mapFileMutex) return false;

	return Refresh();
}

bool InterprocessData::Lock()
{
	if(!mapFile) return false;

	DWORD result=WaitForSingleObject(mapFileMutex,INFINITE);
	if(result!=WAIT_OBJECT_0) return false;

	mapFilePtr=MapViewOfFile(mapFile,FILE_MAP_WRITE,0,0,0);
	if(!mapFilePtr) return false;
	memcpy(this,mapFilePtr,INTERPROCESS_DATA_SIZE);
	return true;
}

void InterprocessData::Unlock()
{
	if(!mapFile || !mapFilePtr) return;

	memcpy(mapFilePtr,this,INTERPROCESS_DATA_SIZE);
	UnmapViewOfFile(mapFilePtr);
	mapFilePtr=NULL;

	ReleaseMutex(mapFileMutex);
}

bool InterprocessData::Refresh()
{
	if(!Lock()) return false;
	Unlock();
	return true;
}

bool Singleplayer=false;
InterprocessData SingleplayerData;
HANDLE SingleplayerClientProcess=NULL;

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
	if(!fm.LoadFile(lst_name,PT_ROOT)) return;

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
		LoadList("data\\deprecated_lists\\tiles.lst",PT_ART_TILES);
		LoadList("data\\deprecated_lists\\items.lst",PT_ART_ITEMS);
		LoadList("data\\deprecated_lists\\scenery.lst",PT_ART_SCENERY);
		LoadList("data\\deprecated_lists\\walls.lst",PT_ART_WALLS);
		LoadList("data\\deprecated_lists\\misc.lst",PT_ART_MISC);
		LoadList("data\\deprecated_lists\\intrface.lst",PT_ART_INTRFACE);
		LoadList("data\\deprecated_lists\\inven.lst",PT_ART_INVEN);
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

void Deprecated_CondExtToAnim2(BYTE cond, BYTE cond_ext, DWORD& anim2ko, DWORD& anim2dead)
{
	if(cond==COND_KNOCKOUT)
	{
		if(cond_ext==2) anim2ko=ANIM2_IDLE_PRONE_FRONT; // COND_KNOCKOUT_FRONT
		anim2ko=ANIM2_IDLE_PRONE_BACK; // COND_KNOCKOUT_BACK
	}
	else if(cond==COND_DEAD)
	{
		switch(cond_ext)
		{
		case 1: anim2dead=ANIM2_DEAD_FRONT; // COND_DEAD_FRONT
		case 2: anim2dead=ANIM2_DEAD_BACK; // COND_DEAD_BACK
		case 3: anim2dead=112; // COND_DEAD_BURST -> ANIM2_DEAD_BURST
		case 4: anim2dead=110; // COND_DEAD_BLOODY_SINGLE -> ANIM2_DEAD_BLOODY_SINGLE
		case 5: anim2dead=111; // COND_DEAD_BLOODY_BURST -> ANIM2_DEAD_BLOODY_BURST
		case 6: anim2dead=113; // COND_DEAD_PULSE -> ANIM2_DEAD_PULSE
		case 7: anim2dead=114; // COND_DEAD_PULSE_DUST -> ANIM2_DEAD_PULSE_DUST
		case 8: anim2dead=115; // COND_DEAD_LASER -> ANIM2_DEAD_LASER
		case 9: anim2dead=117; // COND_DEAD_EXPLODE -> ANIM2_DEAD_EXPLODE
		case 10: anim2dead=116; // COND_DEAD_FUSED -> ANIM2_DEAD_FUSED
		case 11: anim2dead=118; // COND_DEAD_BURN -> ANIM2_DEAD_BURN
		case 12: anim2dead=118; // COND_DEAD_BURN2 -> ANIM2_DEAD_BURN
		case 13: anim2dead=119; // COND_DEAD_BURN_RUN -> ANIM2_DEAD_BURN_RUN
		default: anim2dead=ANIM2_DEAD_FRONT;
		}
	}
}
