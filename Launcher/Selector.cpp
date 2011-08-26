//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "Selector.h"
#include <IdBaseComponent.hpp>
#include <IdComponent.hpp>
#include <IdHTTP.hpp>
#include <IdTCPClient.hpp>
#include <IdTCPConnection.hpp>
#include <IdSocks.hpp>
#include <Strutils.hpp>
#include <strstream>
#include <stdio.h>
#include "TypeServer.h"
#include "TypeProxy.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TSelectorForm *SelectorForm;
//---------------------------------------------------------------------------
#define TOKEN_BEGIN         "_serverbegin_"
#define TOKEN_END           "_serverend_"
#define TOKEN_RU            "_ru_"
#define TOKEN_EN            "_en_"
//---------------------------------------------------------------------------
__fastcall TSelectorForm::TSelectorForm(TComponent* Owner)
	: TForm(Owner)
{
	DoubleBuffered = true;
	ReAbout->Parent = this;
	LRESULT mask = SendMessage(ReAbout->Handle, EM_GETEVENTMASK, 0, 0);
	SendMessage(ReAbout->Handle, EM_SETEVENTMASK, 0, mask | ENM_LINK);
	SendMessage(ReAbout->Handle, EM_AUTOURLDETECT, 1, 0);
	Localization();

	TimerCheck->Enabled = true;
}
//---------------------------------------------------------------------------
void TSelectorForm::Localization()
{
	Caption = LANG("Selector", "Selector");
	LServers->Caption = LANG("Игровые серверы", "Game servers");
	LOptions->Caption = LANG("Опции", "Options");
	LServerInfo->Caption = LANG("Информация о сервере", "Information about server");
	BtnSelect->Caption = LANG("Запуск", "Launch");
	BtnProxy->Caption = LANG("Прокси", "Proxy");
	BtnExit->Caption = LANG("Отмена", "Cancel");
}
//---------------------------------------------------------------------------
bool TSelectorForm::ParseHost(const char* host, AnsiString& resultHost, AnsiString& resultPort)
{
	std::istrstream hostStr(host);
	char str1[2048];
	char str2[2048];
	if((hostStr >> str1 >> str2).fail()) return false;
	resultHost = str1;
	resultPort = str2;
	return true;
}
//---------------------------------------------------------------------------
void TSelectorForm::ShowServers()
{
	// Clear list and text
	LbServers->Clear();
	ReAbout->Text = "";

	// Add servers
	std::vector<ServerInfo*>& servers = Servers;
	for(size_t i = 0; i < servers.size(); i++)
	{
		ServerInfo* si = servers[i];
		LbServers->AddItem(si->Name, NULL);
	}

	// Custom server
	LbServers->AddItem(IsEnglish ? "Other" : "Другой", NULL);

	// Select first server
	LbServers->ItemIndex = 0;
	LbServersClick(LbServers);
}
//---------------------------------------------------------------------------
void __fastcall TSelectorForm::RbLangRusClick(TObject *Sender)
{
	IsEnglish = false;
	Localization();
	ShowServers();
}
//---------------------------------------------------------------------------
void __fastcall TSelectorForm::RbLangEngClick(TObject *Sender)
{
	IsEnglish = true;
	Localization();
	ShowServers();
}
//---------------------------------------------------------------------------
void __fastcall TSelectorForm::LbServersClick(TObject *Sender)
{
	AnsiString about;
	ServerInfo* si = GetSelectedServer();
	if(si)
	{
		about += si->Name + "\r\n";
		about += IsEnglish ? si->DescriptionEn : si->DescriptionRu;
	}
	else
	{
		about += IsEnglish ? "Type server manually." : "Ввести сервер самостоятельно.";
	}
	ReAbout->Text = about;
}
//---------------------------------------------------------------------------
TSelectorForm::ServerInfo* TSelectorForm::GetSelectedServer()
{
	int index = LbServers->ItemIndex;
	if(index == -1 || index >= Servers.size()) return NULL;
	return Servers[index];
}
//---------------------------------------------------------------------------
void __fastcall TSelectorForm::WndProc(Messages::TMessage &Message)
{
	if(Message.Msg == WM_NOTIFY)
	{
		if(((LPNMHDR)Message.LParam)->code == EN_LINK)
		{
			TENLink& link = *(TENLink*)((*(TWMNotify*)&Message).NMHdr);
			if(link.msg == WM_LBUTTONDOWN)
			{
				SendMessage(ReAbout->Handle, EM_EXSETSEL, 0, (long)&link.chrg);
				AnsiString strURL = ReAbout->SelText;
				ShellExecute(Handle, "open", strURL.c_str(), 0, 0, SW_SHOWNORMAL);
			}
		}
	}

	TForm::WndProc(Message);
}
//---------------------------------------------------------------------------
unsigned int __stdcall PingThread(void* arg)
{
	TSelectorForm::ServerInfo* si = (TSelectorForm::ServerInfo*)arg;

	TIdTCPClient* pinger = new TIdTCPClient(NULL);
	pinger->Host = si->UpdaterHost;
	pinger->Port = si->UpdaterPort.ToInt();
	pinger->ConnectTimeout = 5000;
	pinger->ReadTimeout = 5000;

	try
	{
		// Connect
		pinger->Connect();
		if(pinger->Connected())
		{
			// Ping - PingOk
			pinger->IOHandler->WriteLn(L"Ping");
			if(pinger->IOHandler->ReadLn() == L"PingOk")
			{
				// Ping success
				si->Pinged = true;
			}
		}
	}
	catch(...)
	{
		// Fail
	}

	// Free pinger
	delete pinger;

	return 0;
}
//---------------------------------------------------------------------------
void __fastcall TSelectorForm::TimerCheckTimer(TObject *Sender)
{
	TimerCheck->Enabled = false;

	// Get HTTP data
	TIdHTTP* HTTP = new TIdHTTP(this);
	TIdIOHandlerStack* hstack = NULL;
	TIdSocksInfo* proxy = NULL;
	AnsiString data;
	bool fail = false;
	try
	{
		if(TypeProxyForm->ProxyType == PROXY_HTTP)
		{
			HTTP->ProxyParams->ProxyServer = TypeProxyForm->ProxyHost;
			HTTP->ProxyParams->ProxyPort = TypeProxyForm->ProxyPort;
			HTTP->ProxyParams->ProxyUsername = TypeProxyForm->ProxyUsername;
			HTTP->ProxyParams->ProxyPassword = TypeProxyForm->ProxyPassword;
		}
		else if(TypeProxyForm->ProxyType == PROXY_SOCKS4 || TypeProxyForm->ProxyType == PROXY_SOCKS5)
		{
			hstack = new TIdIOHandlerStack(this);
			HTTP->IOHandler = hstack;

			proxy = new TIdSocksInfo(this);
			proxy->Host = L"localhost";
			proxy->Port = 1080;
			proxy->Version = TypeProxyForm->ProxyType == PROXY_SOCKS4 ? svSocks4 : svSocks5;

			hstack->TransparentProxy = proxy;
			hstack->TransparentProxy->Enabled = true;
		}

		data = HTTP->Get(SERVERS_LIST_URL);
	}
	catch(...)
	{
		fail = true;
	}

	// Free resources
	delete HTTP;
	if(hstack) delete hstack;
	if(proxy) delete proxy;

	// Manage fail
	if(fail)
	{
		ShowMessageOK(LANG(
			"Не удалось загрузить список серверов, проверьте соединение с интернетом.",
			"Can't load servers list, check internet connection."));
		goto label_End;
	}

	// Parse data
	data = Strutils::AnsiReplaceText(data, "\r\n", "\n");
	data = Strutils::AnsiReplaceText(data, TOKEN_BEGIN, TOKEN_BEGIN);
	data = Strutils::AnsiReplaceText(data, TOKEN_END, TOKEN_END);
	data = Strutils::AnsiReplaceText(data, TOKEN_RU, TOKEN_RU);
	data = Strutils::AnsiReplaceText(data, TOKEN_EN, TOKEN_EN);

	while(true)
	{
		// Find block
		int beginPos = data.Pos(TOKEN_BEGIN);
		int endPos = data.Pos(TOKEN_END);
		if(beginPos == 0 || endPos == 0) break;

		// Clear block
		AnsiString block = data.SubString(beginPos + strlen(TOKEN_BEGIN) + 1, endPos - beginPos - strlen(TOKEN_BEGIN) - 2);
		data.Delete(beginPos, endPos - beginPos + strlen(TOKEN_END));

		// Prepare data
		std::istrstream blockStr(block.c_str());
		char line[0x10000];
		ServerInfo* si = new ServerInfo();

		// Get name
		blockStr.getline(line, sizeof(line));
		EraseFrontBackSpecificChars(line);
		si->Name = line;

		// Get updater
		blockStr.getline(line, sizeof(line));
		EraseFrontBackSpecificChars(line);
		ParseHost(line, si->UpdaterHost, si->UpdaterPort);

		// Get description
		while(!blockStr.eof())
		{
			blockStr.getline(line, sizeof(line));
			EraseFrontBackSpecificChars(line);
			if(!line[0]) break;

			AnsiString desc = line;
			AnsiString descRu = Strutils::AnsiReplaceText(line, TOKEN_RU, "");
			AnsiString descEn = Strutils::AnsiReplaceText(line, TOKEN_EN, "");

			if(desc != descRu)
			{
				strcpy(line, descRu.c_str());
				EraseFrontBackSpecificChars(line);
				si->DescriptionRu += AnsiString(line) + "\r\n";
			}
			else if(desc != descEn)
			{
				strcpy(line, descEn.c_str());
				EraseFrontBackSpecificChars(line);
				si->DescriptionEn += AnsiString(line) + "\r\n";
			}
			else
			{
				si->DescriptionRu += AnsiString(line) + "\r\n";
				si->DescriptionEn += AnsiString(line) + "\r\n";
			}
		}

		if(si->IsValid())
		{
			// Add to collection
			Servers.push_back(si);

			// Run pinger
			si->HThread = (HANDLE)_beginthreadex(NULL, 0, PingThread, si, 0, NULL);
		}
	}

label_End:
	// Wait pinger threads
	for(size_t i = 0; i < Servers.size(); i++)
	{
		if(Servers[i]->HThread)
		{
			WaitForSingleObject(Servers[i]->HThread, INFINITE);
			CloseHandle(Servers[i]->HThread);
			Servers[i]->HThread = NULL;
		}
    }

	// Delete not pinged servers
	for(size_t i = 0; i < Servers.size();)
	{
		if(!Servers[i]->Pinged)
			Servers.erase(Servers.begin() + i);
		else
			i++;
	}

	// Show servers
	ShowServers();

	// Show panels
	PanelMain->Visible = true;
	ReAbout->Visible = true;
	LWaitRus->Visible = false;
	LWaitEng->Visible = false;
	BtnSelect->SetFocus();
}
//---------------------------------------------------------------------------
void __fastcall TSelectorForm::BtnSelectClick(TObject *Sender)
{
	if(LbServers->ItemIndex == -1) return;

	ServerInfo* si = GetSelectedServer();
	if(si)
	{
		// Normal server
		Result = *si;
	}
	else
	{
		// Custom server
		// Get hosts
		TTypeServerForm* typeServer = new TTypeServerForm(this);
		int r = typeServer->ShowModal();
		AnsiString updaterHost, updaterPort;
		typeServer->GetResult(updaterHost, updaterPort);
		delete typeServer;
		if(r != mrOk) return;

		// Fill
		Result.Name = Result.DescriptionRu = Result.DescriptionEn;
		Result.UpdaterHost = updaterHost;
		Result.UpdaterPort = updaterPort;
	}

	ModalResult = mrOk;
}
//---------------------------------------------------------------------------
void __fastcall TSelectorForm::BtnExitClick(TObject *Sender)
{
	Close();
}
//---------------------------------------------------------------------------
void __fastcall TSelectorForm::BtnProxyClick(TObject *Sender)
{
	if(TypeProxyForm->ShowModal() == mrOk)
	{
		// Try again with proxy
		for(size_t i = 0; i < Servers.size(); i++)
			delete Servers[i];
		Servers.clear();
		ShowServers();
		PanelMain->Visible = false;
		ReAbout->Visible = false;
		LWaitRus->Visible = true;
		LWaitEng->Visible = true;
		TimerCheck->Enabled = true;
	}
}
//---------------------------------------------------------------------------
void __fastcall TSelectorForm::LbServersDblClick(TObject *Sender)
{
	BtnSelectClick(Sender);
}
//---------------------------------------------------------------------------

