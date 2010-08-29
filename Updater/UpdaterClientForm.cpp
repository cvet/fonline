//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "UpdaterClientForm.h"
#include <process.h>
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "cspin"
#pragma resource "*.dfm"
TMainForm *MainForm;
void __cdecl ProcessThread(void*);
//---------------------------------------------------------------------------
#define GET_LANG(ru,en) (Lang==LANG_RUS?ru:en)
__fastcall TMainForm::TMainForm(TComponent* Owner)
	: TForm(Owner)
{
	DotsCount=0;
	Lang=LANG_RUS;
	char str[256];
	GetPrivateProfileString(CFG_FILE_APP_NAME,"Language","russ",str,5,CFG_FILE);
	if(!strcmp(str,"engl")) Lang=LANG_ENG;

	MainForm->Caption=GET_LANG("Обновления     v1.6","Updater     v1.6");
	LHost->Caption=GET_LANG("Хост","Host");
	LPort->Caption=GET_LANG("Порт","Port");
	BCheck->Caption=GET_LANG("Проверить","Check");

	GetPrivateProfileString(CFG_FILE_APP_NAME,"RemoteHost","localhost",str,256,CFG_FILE);
	CbHost->Text=AnsiString(str);
	for(int i=0;i<999;i++)
	{
		char rhost[256];
		wsprintf(rhost,"RemoteHost_%d",i);
		GetPrivateProfileString(CFG_FILE_APP_NAME,rhost,"",str,256,CFG_FILE);
		if(strlen(str)==0) break;
		CbHost->Items->Add(str);
	}

	InitCrc32();

	if(strstr(GetCommandLine(),"-start ")) BCheckClick(NULL);
}
//---------------------------------------------------------------------------
#define UPD_INFO(ru,en) do{MLog->Lines->Add(Lang==LANG_RUS?ru:en); MLog->Refresh();}while(0)
#define UPD_ERROR(ru,en) do{MLog->Lines->Add(Lang==LANG_RUS?ru:en); MLog->Refresh(); goto label_Error;}while(0)
#define RECEIVE(buf,bytes,errru,erren) do{unsigned int need=bytes; for(unsigned int recb;need;need-=recb) if((recb=TcpClient->ReceiveBuf(((char*)buf)+(bytes)-need,need))==0) break; if(need) UPD_ERROR(errru,erren);}while(0)
void __fastcall TMainForm::BCheckClick(TObject *Sender)
{
	BCheck->Enabled=false;
	UPD_INFO("Начинаем проверку...","Begin checking...");
	MLog->Refresh();
	DotsCount=0;
	BCheck->Caption=GET_LANG("Ждите","Wait");
	_beginthread(ProcessThread,0,NULL);
}
//---------------------------------------------------------------------------
void __cdecl ProcessThread(void*)
{
	MainForm->Process();
	_endthread();
}
//---------------------------------------------------------------------------
void TMainForm::Process()
{
	TcpClient->RemoteHost=CbHost->Text;
	TcpClient->RemotePort=CsePort->Value;
	TcpClient->BlockMode=bmThreadBlocking;
	TcpClient->Active=true;
	if(TcpClient->Connect())
	{
		TcpClient->Sendln("Hello1");
		if(TcpClient->Receiveln()=="Greetings")
		{
			TcpClient->Sendln("Give hashes list");
			if(TcpClient->Receiveln()=="Take hashes list")
			{
				// Receive list and collect not equal hashes
				unsigned int hashesCount;
				RECEIVE(&hashesCount,4,"Ошибка при приеме информации о количестве файлов.","Error on files count information receive.");
				AnsiStringVec needFiles;

				for(;hashesCount;hashesCount--)
				{
					AnsiString fname=TcpClient->Receiveln();
					AnsiString hash=TcpClient->Receiveln();
					if(fname=="" || hash=="") UPD_ERROR("Ошибка при приеме данных о файле.","Error on file information receive.");

					try
					{
						TMemoryStream* stream=new TMemoryStream();
						stream->LoadFromFile(fname);
						AnsiString hash_=Crc32((unsigned char*)stream->Memory,stream->Size);
						delete stream;
						if(hash_!=hash) needFiles.push_back(fname);
					}
					catch(...)
					{
						needFiles.push_back(fname);
					}
				}

				// All hashes is equal
				if(needFiles.empty())
				{
					UPD_INFO("Обновление не требуется.","No update needed.");
					TcpClient->Sendln("Bye");
					TcpClient->WaitForData(15000);
					goto label_Error;
				}

				// Founded not equal hashes, update this files
				for(AnsiStringVecIt it=needFiles.begin(),end=needFiles.end();it!=end;++it)
				{
					AnsiString& fname=*it;
					TcpClient->Sendln("Get");
					TcpClient->Sendln(fname);
					AnsiString answer=TcpClient->Receiveln();
					if(answer=="Catch" || answer=="Catchpack")
					{
						unsigned int tick=GetTickCount();
						int size;
						RECEIVE(&size,4,"Ошибка при приеме информации о размере файла.","Error on file size information receive.");
						UPD_INFO("Принимаем "+fname+" "+size+" байт.","Receive "+fname+" "+size+" bytes.");
						TMemoryStream* stream=new TMemoryStream();
						stream->SetSize(size);
						RECEIVE(stream->Memory,size,"Ошибка при приеме данных файла.","Error on file data receive.");
						try
						{
							// Unpack
							if(answer=="Catchpack" && !Uncompress(stream)) UPD_ERROR("Не удалось распаковать принятый файл.","Unable to unpack received file.");

                            // Check folder availability
							char buf[1024];
							char* part=NULL;
							if(GetFullPathName(fname.c_str(),1024,buf,&part)!=0)
							{
								if(part) *part=0;
								if(!CreateDirectory(buf,NULL))
								{
									// Create all folders tree
									for(int i=0,j=strlen(buf);i<j;i++)
									{
										if(buf[i]=='\\')
										{
											char c=buf[i+1];
											buf[i+1]=0;
											CreateDirectory(buf,NULL);
											buf[i+1]=c;
										}
									}
								}
							}

							// Save to file
							stream->SaveToFile(fname);
							tick=(GetTickCount()-tick)/1000;
							AnsiString tickStr=tick;
							UPD_INFO("Успешно, за "+tickStr+" секунд.","Done, for "+tickStr+" seconds.");
						}
						catch(...)
						{
							UPD_INFO("Не удалось сохранить файл.","Unable to save file.");
						}
						delete stream;
					}
					else
					{
						UPD_ERROR("Неизвестное сообщение при приеме файлов.","Unknown message on files receiving.");
					}
				}

				// Done
				UPD_INFO("Обновление завершено.","Update complete.");
				TcpClient->Sendln("Bye");
				TcpClient->WaitForData(15000);
			}
			else UPD_ERROR("Сервер не выслал список файлов.","Server not give list of hashes.");
		}
		else UPD_ERROR("Сервер не отвечает.","Server responce fail.");
	}
	else UPD_ERROR("Невозможно подключиться к серверу обновлений.","Can't connect to update server.");

label_Error:
	TcpClient->Active=false;
	BCheck->Enabled=true;
	BCheck->Caption=GET_LANG("Проверить","Check");
	UPD_INFO("Проверка закончена.","Checking end.");
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::WaitTimerTimer(TObject *Sender)
{
	if(!BCheck->Enabled)
	{
		AnsiString title=GET_LANG("Ждите","Wait");
		for(int i=0;i<DotsCount;i++) title="."+title+".";
		BCheck->Caption=title;
		if(++DotsCount>=10) DotsCount=0;
	}
}
//---------------------------------------------------------------------------

