//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "FOConfig.h"
#include <stdio.h>
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "cspin"
#pragma resource "*.dfm"
TMainForm *MainForm;
//---------------------------------------------------------------------------
__fastcall TMainForm::TMainForm(TComponent* Owner)
	: TForm(Owner)
{

}
//---------------------------------------------------------------------------
AnsiString GetString( const char* key, const char* def_val )
{
	char buf[ 2048 ];
	GetPrivateProfileString( CFG_FILE_APP_NAME, key, def_val, buf, 2048, CFG_FILE );
	char* str = buf;
	while( *str && ( *str == ' ' || *str == '\t' || *str == '\n' || *str == '\r' ) )
		str++;
	char* str_begin = str;
	while( *str && !( *str == ' ' || *str == '\t' || *str == '\n' || *str == '\r' ) )
		str++;
	*str = 0;
	return AnsiString( str_begin );
}
//---------------------------------------------------------------------------
int GetInt( const char* key, int def_val )
{
	char buf[ 32 ];
	AnsiString str = GetString( key, itoa( def_val, buf, 10 ) );
	if( !stricmp( str.c_str(), "true" ) )
		return 1;
	if( !stricmp( str.c_str(), "false" ) )
		return 0;
	return atoi( str.c_str() );
}
//---------------------------------------------------------------------------
void SetString( const char* key, const char* val )
{
	if( GetString( key, "$_$$$$MAGIC$$$$_$" ) == "$_$$$$MAGIC$$$$_$" )
	{
		char buf[ 2048 ];
		strcpy( buf, "  " );
		strcat( buf, val );
		WritePrivateProfileString( CFG_FILE_APP_NAME, key, buf, CFG_FILE );
		FILE* f = fopen( CFG_FILE, "rb" );
		if( f )
		{
			fseek( f, 0, SEEK_END );
			int len = ftell( f );
			fseek( f, 0, SEEK_SET );
			char* fbuf = new char[ len ];
			fread( fbuf, sizeof( char ), len, f );
			fclose( f );

			strcpy( buf, key );
			strcat( buf, "= " );
			char* s = strstr( fbuf, buf );
			*( s + strlen( key ) + 0 ) = ' ';
			*( s + strlen( key ) + 1 ) = '=';

			FILE* f = fopen( CFG_FILE, "wb" );
			fwrite( fbuf, sizeof( char ), len, f );
			fclose( f );
			delete[] fbuf;
		}
	}
	else
	{
		char buf[ 2048 ] = { 0 };
		if( val[ 0 ] )
		{
			strcat( buf, " " );
			strcat( buf, val );
		}
		WritePrivateProfileString( CFG_FILE_APP_NAME, key, buf, CFG_FILE );
	}
}
//---------------------------------------------------------------------------
void SetInt( const char* key, int val, bool as_boolean = false )
{
	if( as_boolean )
	{
		SetString( key, val ? "True" : "False" );
	}
	else
	{
		char buf[ 32 ];
		SetString( key, itoa( val, buf, 10 ) );
	}
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FormCreate(TObject *Sender)
{
	Serialize(false);
	Translate();
}
//---------------------------------------------------------------------------
#define SE_STR(comp,key,def_val) if(!save) comp=GetString(key,def_val); else SetString(key,AnsiString(comp).c_str())
#define SE_INT(comp,key,min_,max_,def_) if(!save){int i=GetInt(key,def_); if(i<min_ || i>max_) i=def_; comp=i;} else SetInt(key,comp)
#define SE_BOOL(comp,key,min_,max_,def_) if(!save){int i=GetInt(key,def_); if(i<min_ || i>max_) i=def_; comp=i;} else SetInt(key,comp,true)
#define SE_INTSTR(comp,key,min_,max_,def_) if(!save){int i=GetInt(key,def_); if(i<min_ || i>max_) i=def_; comp=i;} else SetInt(key,atoi(AnsiString(comp).c_str()))
#define SE_RBTN(comp,key,val,def_val) if(!save){int i=GetInt(key,def_val);comp->Checked=(i==val?true:false);} else if(comp->Checked) SetInt(key,val)
#define SE_COMBO(comp,key,def_val) do{                                        \
	AnsiString buf;                                                           \
	if(!save)                                                                 \
	{                                                                         \
		buf=GetString(key,def_val);                        				      \
		comp->Text=buf;                                      		          \
		comp->Items->Add(buf);                                       		  \
		for(int i=0;i<100;i++)                                                \
		{                                                                     \
			buf=GetString(buf.sprintf(key"_%d",i).c_str(),"empty");           \
			if(buf==AnsiString("empty")) continue;                            \
			comp->Items->Add(buf);                                            \
		}                                                                     \
	}                                                                         \
	else                                                                      \
	{                                                                         \
		SetString(key,AnsiString(comp->Text).c_str());                        \
		for(int i=0,j=0;i<comp->Items->Count;i++)                             \
		{                                                                     \
			if(comp->Text==(*comp->Items)[i]) continue;                       \
			SetString(buf.sprintf(key"_%d",j).c_str(),                        \
				AnsiString((*comp->Items)[i]).c_str());                       \
			j++;                                                              \
		}                                                                     \
	}                                                                         \
}while(0)

void TMainForm::Serialize(bool save)
{
	SE_STR(Lang,"Language","russ");
	SE_BOOL(CbWinNotify->State,"WinNotify",0,1,1);
	SE_BOOL(CbSoundNotify->State,"SoundNotify",0,1,0);
	SE_BOOL(CbInvertMessBox->State,"InvertMessBox",0,1,0);
	SE_BOOL(CbLogging->State,"Logging",0,1,1);
	SE_BOOL(CbLoggingTime->State,"LoggingTime",0,1,0);
	SE_INT(SeFixedFPS->Value,"FixedFPS",0,10000,100);
	SE_INT(SeScrollDelay->Value,"ScrollDelay",0,100,10);
	SE_INT(SeScrollStep->Value,"ScrollStep",4,32,12);
	SE_INT(SeTextDelay->Value,"TextDelay",1000,30000,3000);
	SE_RBTN(RbCtrlShift,"LangChange",0,0);
	SE_RBTN(RbAltShift,"LangChange",1,0);
	SE_BOOL(CbAlwaysRun->State,"AlwaysRun",0,1,0);
	SE_COMBO(CbServerHost,"RemoteHost","localhost");
	SE_INT(SeServerPort->Value,"RemotePort",0,0xFFFF,4000);
	SE_RBTN(RbProxyNone,"ProxyType",0,0);
	SE_RBTN(RbProxySocks4,"ProxyType",1,0);
	SE_RBTN(RbProxySocks5,"ProxyType",2,0);
	SE_RBTN(RbProxyHttp,"ProxyType",3,0);
	SE_COMBO(CbProxyHost,"ProxyHost","localhost");
	SE_INT(SeProxyPort->Value,"ProxyPort",0,0xFFFF,8080);
	SE_STR(EditProxyUser->Text,"ProxyUser","");
	SE_STR(EditProxyPass->Text,"ProxyPass","");
	SE_INTSTR(CbScreenWidth->Text,"ScreenWidth",100,10000,800);
	SE_INTSTR(CbScreenHeight->Text,"ScreenHeight",100,10000,600);
	SE_INT(SeLight->Value,"Light",0,100,20);
	SE_INT(SeSprites->Value,"FlushValue",1,1000,100);
	SE_INT(SeTexture->Value,"BaseTexture",128,8192,1024);
	SE_BOOL(CbFullScreen->State,"FullScreen",0,1,0);
	SE_BOOL(CbVSync->State,"VSync",0,1,0);
	SE_BOOL(CbAlwaysOnTop->State,"AlwaysOnTop",0,1,0);
	SE_INT(SeAnimation3dSmoothTime->Value,"Animation3dSmoothTime",0,10000,250);
	SE_INT(SeAnimation3dFPS->Value,"Animation3dFPS",0,1000,0);
	SE_INT(TbMusicVolume->Position,"MusicVolume",0,100,100);
	SE_INT(TbSoundVolume->Position,"SoundVolume",0,100,100);
	SE_RBTN(RbDefCmbtModeBoth,"DefaultCombatMode",0,0);
	SE_RBTN(RbDefCmbtModeRt,"DefaultCombatMode",1,0);
	SE_RBTN(RbDefCmbtModeTb,"DefaultCombatMode",2,0);
	SE_RBTN(RbIndicatorLines,"IndicatorType",0,0);
	SE_RBTN(RbIndicatorNumbers,"IndicatorType",1,0);
	SE_RBTN(RbIndicatorBoth,"IndicatorType",2,0);
	SE_RBTN(RbCmbtMessVerbose,"CombatMessagesType",0,0);
	SE_RBTN(RbCmbtMessBrief,"CombatMessagesType",1,0);
	SE_INT(SeDamageHitDelayValue->Value,"DamageHitDelay",0,30000,0);

	int value=CbMultisampling->ItemIndex;
	if(save) value--;
	SE_INT(value,"Multisampling",-1,16,-1);
	if(!save) CbMultisampling->ItemIndex=value+1;
}
//---------------------------------------------------------------------------
#define TR_(comp,rus,eng) comp->Caption=(Lang=="russ"?rus:eng)
void TMainForm::Translate()
{
	if(Lang=="russ") RbRussian->Checked=true;
	else RbEnglish->Checked=true;
	TR_(MainForm,"FOnline конфигуратор   v2.5.0","FOnline configurator   v2.5.0");
	TR_(BtnExecute,"Играть","Play");
	TR_(BtnParse,"Сохранить","Save");
	TR_(BtnExit,"Выход","Exit");
	TR_(TabOther,"Разное","Other");
	TR_(GbLanguage,"Язык \\ Language","Language \\ Язык");
	TR_(RbRussian,"Русский","Русский");
	TR_(RbEnglish,"English","English");
	TR_(GbOther,"","");
	TR_(CbWinNotify,"Извещение о сообщениях\nпри неактивном окне.","Flush window on\nnot active game.");
	TR_(CbSoundNotify,"Звуковое извещение о сообщениях\nпри неактивном окне.","Beep sound on\nnot active game.");
	TR_(CbInvertMessBox,"Инвертирование текста\nв окне сообщений.","Invert text\nin messbox.");
	TR_(CbLogging,"Ведение лога в 'FOnline.log'.","Logging in 'FOnline.log'.");
	TR_(CbLoggingTime,"Запись в лог с указанием времени.","Logging with time.");
	TR_(LabelFixedFPS,"Фиксированный\n         FPS","              Fixed\n               FPS");
	TR_(TabGame,"Игра","Game");
	TR_(GbGame,"Игра","Game");
	TR_(LabelScrollDelay,"Задержка скроллинга","Scroll delay");
	TR_(LabelScrollStep,"Шаг скроллинга","Scroll step");
	TR_(LabelTextDelay,"Время задержки текста (мс)","Text delay (ms)");
	TR_(CbAlwaysRun,"Постоянный бег","Always run");
	TR_(GbLangSwitch,"Переключение раскладки","Keyboard language switch");
	TR_(RbCtrlShift,"Ctrl + Shift","Ctrl + Shift");
	TR_(RbAltShift,"Alt + Shift","Alt + Shift");
	TR_(TabNet,"Сеть","Net");
	TR_(GbServer,"Игровой сервер","Game server");
	TR_(LabelServerHost,"Хост","Host");
	TR_(LabelServerPort,"Порт","Port");
	TR_(GbProxy,"Прокси","Proxy");
	TR_(LabelProxyType,"Тип","Type");
	TR_(RbProxyNone,"Нет","None");
	TR_(RbProxySocks4,"Socks4","Socks4");
	TR_(RbProxySocks5,"Socks5","Socks5");
	TR_(RbProxyHttp,"Http","Http");
	TR_(LabelProxyHost,"Хост","Host");
	TR_(LabelProxyPort,"Порт","Port");
	TR_(LabelProxyName,"Логин","Login");
	TR_(LabelProxyPass,"Пароль","Password");
	TR_(TabVideo,"Видео","Video");
	TR_(GbScreenSize,"Разрешение","Resolution");
	TR_(GbVideoOther,"","");
	TR_(LabelLight,"Яркость","Light");
	TR_(LabelSprites,"Кешируемые\nспрайты","Cache sprites");
	TR_(LabelTexture,"Размер текстур","Texture size");
	TR_(LabelMultisampling,"Мультисэмплинг 3d","Multisampling 3d");
	TR_(CbFullScreen,"Полноэкранный режим","Fullscreen");
	TR_(CbVSync,"Вертикальная синхронизация","VSync");
	TR_(CbAlwaysOnTop,"Поверх всех окон","Always on top");
	TR_(LabelAnimation3dFPS,"3d FPS","3d FPS");
	TR_(LabelAnimation3dSmoothTime,"Сглаживание 3d переходов","3d smooth transition");
	TR_(TabSound,"Звук","Sound");
	TR_(GbSoundVolume,"Громкость","Volume");
	TR_(LabelMusicVolume,"Музыка","Music");
	TR_(LabelSoundVolume,"Звуки","Sound");
	TR_(TabCombat,"Боевка","Combat");
	TR_(GbDefCmbtMode,"Режим боя по-умолчанию","Default combat mode");
	TR_(RbDefCmbtModeBoth,"Оба режима","Both modes");
	TR_(RbDefCmbtModeRt,"Реальное время","Real-time");
	TR_(RbDefCmbtModeTb,"Пошаговый режим","Turn-based");
	TR_(GbIndicator,"Индикатор патронов и износа","Ammo amount and deterioration display");
	TR_(RbIndicatorLines,"Линии","Lines");
	TR_(RbIndicatorNumbers,"Номера","Numbers");
	TR_(RbIndicatorBoth,"Линии и номера","Lines and numbers");
	TR_(GbCmbtMess,"Боевые сообщения","Combat messages");
	TR_(RbCmbtMessVerbose,"Полные","Verbose");
	TR_(RbCmbtMessBrief,"Краткие","Brief");
	TR_(GbDamageHitDelay,"Режим индикации повреждений над головой","Damage indication on head");
	TR_(LabelDamageHitDelay,"Задержка, в мс","Delay (ms)");
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::BtnExecuteClick(TObject *Sender)
{
	Serialize(true);
	char path[2048];
	if(!GetCurrentDirectory(2000,path)) return;
	if((int)ShellExecute(NULL,"open","FOnline.exe",NULL,path,SW_SHOW)>32) Close();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::BtnParseClick(TObject *Sender)
{
	Serialize(true);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::BtnExitClick(TObject *Sender)
{
	Close();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::RbEnglishClick(TObject *Sender)
{
	Lang="engl";
	Translate();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::RbRussianClick(TObject *Sender)
{
	Lang="russ";
	Translate();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::CbScreenWidthChange(TObject *Sender)
{
	CbScreenHeight->ItemIndex=CbScreenWidth->ItemIndex;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::CbScreenHeightChange(TObject *Sender)
{
	CbScreenWidth->ItemIndex=CbScreenHeight->ItemIndex;
}
//---------------------------------------------------------------------------



