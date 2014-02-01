//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "Updater.h"
#include "Config.h"
#include "Selector.h"
#include "TypeProxy.h"
#include <idl/CDOSys_I.c>
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "cgauges"
#pragma resource "*.dfm"
TUpdaterForm *UpdaterForm;
void __cdecl ProcessThread(void*);
//---------------------------------------------------------------------------
__fastcall TUpdaterForm::TUpdaterForm(TComponent* Owner)
	: TForm(Owner)
{
	// Initialization
	setlocale(LC_ALL, "Russian");
	RestoreMainDirectory();
	InitCrc32();
	SetConfigName("", "");

	DoubleBuffered = true;
    Caption = "Launcher";

	BtnPlay->Caption = "";
    BtnOptions->Caption = "";
	BtnPlay->Enabled = false;
	BtnOptions->Enabled = false;

	WebBrowser->Align = alClient;
	PanelWB->Align = alClient;
	WebBrowserCached->Align = alClient;
	PanelWBCached->Align = alClient;

	ProgressCur = 0;
	ProgressMax = 0;
	GaugeText("", "");
	ShapeGauge->Width = 0;
	LGaugeProgress->Caption = "";

	IsPlayed = false;

    // Pathes
	char path[2048];
	GetCurrentDirectory(2048, path);
	LauncherPath = AnsiString(path) + "\\";
    OptionsFile = LauncherPath + "data\\cache\\launcher.cfg";
	FileHashes = LauncherPath + "data\\cache\\file_hashes.lst";
	WebPageMht = LauncherPath + "data\\cache\\launcher.mht";
}
//---------------------------------------------------------------------------
void __fastcall TUpdaterForm::FormActivate(TObject *Sender)
{
	// Prevent
	static bool isInit = false;
    if(isInit) return;
    isInit = true;

    // Check stored exe name
    SetConfigName(OptionsFile, "Options");
	ExeName = GetString("ExeName", "");
	IsPlayed = GetInt("IsPlayed", 0) != 0;

    // Game selected
    if(ExeName != "")
    {
		// Hosts
		UpdaterHost = GetString("UpdaterHost", "localhost");
  		UpdaterPort = GetString("UpdaterPort", "4040");
		UpdaterUrl = GetString("UpdaterUrl", DEFAULT_WEB_HOST);

        // Switch to game configurator
    	SetConfigName(ExeName + ".cfg", "Game Options");
   	    Caption = ExeName + " Launcher";
		IsEnglish = (GetString("Language", "") == "engl" ? true : false);

		// Proxy settings
		TypeProxyForm->ProxyType = GetInt("ProxyType", PROXY_NONE);
		TypeProxyForm->ProxyHost = GetString("ProxyHost", "localhost");
		TypeProxyForm->ProxyPort = GetInt("ProxyPort", 8080);
		TypeProxyForm->ProxyUsername = GetString("ProxyUser", "");
		TypeProxyForm->ProxyPassword = GetString("ProxyPass", "");
	}
    // Selector
	else
	{
		TSelectorForm* selector = new TSelectorForm(this);
		if(selector->ShowModal() != mrOk || !selector->Result.IsValid()) ExitProcess(0);

		UpdaterHost = selector->Result.UpdaterHost;
        UpdaterPort = selector->Result.UpdaterPort;
		UpdaterUrl = DEFAULT_WEB_HOST;

        delete selector;

        CheckDirectory(WebPageMht.c_str());
	}

    // Localization
    BtnPlay->Caption = LANG("Играть", "Play");
    BtnOptions->Caption = LANG("Опции", "Options");

	// Navigate to updater host
	UnicodeString UpdaterUrlW = UpdaterUrl;
	WebBrowser->Navigate(UpdaterUrlW.c_str());

	// Until navigate load cached page
	UnicodeString WebPageMhtW = UpdaterUrl;
	WebBrowserCached->Navigate(WebPageMhtW.c_str());

	// File names hashes
	LoadCachedHashes();

	// Checking updates
	_beginthread(ProcessThread, 0, this);
}
//---------------------------------------------------------------------------
void __fastcall TUpdaterForm::WebBrowserDocumentComplete(TObject *Sender, LPDISPATCH pDisp, Variant *URL)
{
	// Save as MHT
	//WebBrowser->OnDocumentComplete = NULL;
	if(!WebBrowser->Document) goto label_End;
	CoInitialize(NULL);
	IMessage* msg;
	if(FAILED(CoCreateInstance(__uuidof(Message), NULL, CLSCTX_INPROC_SERVER,
		__uuidof(IMessage), (void**)&msg))) goto label_End;
	IConfiguration* config;
	if(FAILED(CoCreateInstance(__uuidof(Configuration), NULL, CLSCTX_INPROC_SERVER,
		__uuidof(IConfiguration), (void**)&config))) goto label_End;
	if(FAILED(msg->put_Configuration(config))) goto label_End;
	if(FAILED(msg->CreateMHTMLBody(WebBrowser->LocationURL.data(), cdoSuppressNone, L"", L""))) goto label_End;
	_Stream* stream;
	if(FAILED(msg->GetStream(&stream))) goto label_End;
	if(FAILED(stream->SaveToFile(WideString(WebPageMht).c_bstr(), adSaveCreateOverWrite))) goto label_End;

label_End:
	PanelWBCached->Visible = false;
}
//---------------------------------------------------------------------------
void __fastcall TUpdaterForm::WebBrowserNavigateError(TObject *Sender, LPDISPATCH pDisp,
		  Variant *URL, Variant *Frame, Variant *StatusCode, VARIANT_BOOL *Cancel)
{
	PanelWB->Visible = false;
}
//---------------------------------------------------------------------------
void __fastcall TUpdaterForm::WebBrowserCachedNavigateError(TObject *Sender, LPDISPATCH pDisp,
		  Variant *URL, Variant *Frame, Variant *StatusCode, VARIANT_BOOL *Cancel)
{
	PanelWBCached->Visible = false;
}
//---------------------------------------------------------------------------
void __fastcall TUpdaterForm::BtnOptionsClick(TObject *Sender)
{
	TConfigForm* cfg = new TConfigForm(this);
	cfg->ShowModal();

	if(cfg->Selector)
	{
		TSelectorForm* selector = cfg->Selector;
		DeleteFile(OptionsFile.c_str());

		// Check stored exe name
		SetConfigName(OptionsFile, "Options");
		ExeName = GetString("ExeName", "");
		IsPlayed = GetInt("IsPlayed", 0) != 0;

		// Selector
		UpdaterHost = selector->Result.UpdaterHost;
		UpdaterPort = selector->Result.UpdaterPort;
		UpdaterUrl = DEFAULT_WEB_HOST;

		delete selector;

		CheckDirectory(WebPageMht.c_str());
	}

	// Check for new language
	IsEnglish = (GetString("Language", "") == "engl" ? true : false);
	BtnPlay->Caption = LANG("Играть", "Play");
	BtnOptions->Caption = LANG("Опции", "Options");
	GaugeText(LastLogMessageRus.c_str(), LastLogMessageEng.c_str());

	// Checking updates
	if(cfg->Selector)
	{
		BtnPlay->Enabled = false;
		BtnOptions->Enabled = false;
		ProgressCur = 0;
		ProgressMax = 0;
		GaugeText("", "");
		ShapeGauge->Width = 0;
		LGaugeProgress->Caption = "";
		ShapeGaugeBg->Brush->Color = clSilver;

		_beginthread(ProcessThread, 0, this);
	}

	delete cfg;
}
//---------------------------------------------------------------------------
void __fastcall TUpdaterForm::BtnPlayClick(TObject *Sender)
{
	char path[2048];
	if(!GetCurrentDirectory(2048, path)) return;

	AnsiString exeName = ExeName + ".exe";
	bool singleplayer = (GetKeyState(VK_SHIFT) >> 15) != 0;
	if((int)ShellExecute(NULL, "open", exeName.c_str(), singleplayer ? "-singleplayer" : NULL, path, SW_SHOW) > 32)
	{
		if(!IsPlayed)
		{
			SetConfigName(OptionsFile, "Options");
			SetInt("IsPlayed", 1);
		}
		Close();
	}
	else
	{
		ShowMessageOK(LANG(exeName + " не найден!", exeName + " not found!"));
	}
}
//---------------------------------------------------------------------------
void TUpdaterForm::GaugeText(AnsiString rus, AnsiString eng)
{
	LGauge->Caption = LANG(rus, eng);
	LastLogMessageRus = rus;
	LastLogMessageEng = eng;
}
//---------------------------------------------------------------------------
void TUpdaterForm::GaugeText(AnsiString text)
{
	LGauge->Caption = text;
	LastLogMessageRus = text;
	LastLogMessageEng = text;
}
//---------------------------------------------------------------------------
void __cdecl ProcessThread(void* updater)
{
	((TUpdaterForm*)updater)->Process();
	_endthread();
}
//---------------------------------------------------------------------------
bool TUpdaterForm::Receive(char* buf, int bytes, bool showProgress)
{
	int need = bytes;
	while(need)
	{
		int readNow = need < RECV_PORTION ? need : RECV_PORTION;
		int recb = TcpClient->ReceiveBuf(((char*)buf) + (bytes) - need, readNow);
		if(recb <= 0) break;
		if(showProgress)
		{
			ProgressCur += recb;
			int procent = (int)(ProgressCur * 100 / ProgressMax);
			ShapeGauge->Width = ShapeGaugeBg->Width * procent / 100;
			LGaugeProgress->Caption = FormatSize(ProgressCur) + " / " + FormatSize(ProgressMax);
		}
		need -= recb;
		if(need) Sleep(0);
	}
	return need == 0;
}
//---------------------------------------------------------------------------
void TUpdaterForm::Process()
{
	GaugeText("Проверяем наличие обновлений", "Checking updates availablility");
	Sleep(1000);

	// Update size
	__int64 wholeSize = 0;

	// Recived game server
	AnsiString gameServer, gameServerPort;

	// Launcher exe name
	const char* moduleName = NULL;
	char modulePathName[2048];
	GetModuleFileName(NULL, modulePathName, 2048);
	for(size_t i = 0, j = strlen(modulePathName); i < j; i++)
		if(modulePathName[i] == '\\') moduleName = &modulePathName[i + 1];

	// Proxy
	if(TypeProxyForm->ProxyType != PROXY_NONE)
	{
		// Connect to proxy
		TcpClient->RemoteHost = TypeProxyForm->ProxyHost;
		TcpClient->RemotePort = TypeProxyForm->ProxyPort;
		TcpClient->BlockMode = bmThreadBlocking;
		TcpClient->Active = true;

		if(!TcpClient->Connect())
		{
			GaugeText("Невозможно подключиться к прокси серверу", "Can't connect to proxy server");
			goto label_Error;
		}

		// Resolve host address
		unsigned int ProxyHostUI = inet_addr(UpdaterHost.c_str());
		if(ProxyHostUI == (unsigned int)(-1))
		{
			hostent* h=gethostbyname(UpdaterHost.c_str());
			if(!h)
			{
				GaugeText(AnsiString("Can't resolve remote host ") + UpdaterHost);
				goto label_Error;
			}

			ProxyHostUI = *(unsigned int*)h->h_addr;
		}

		// Send/Recive wrappers
		unsigned char uc;
		unsigned short us;
		unsigned int ui;
		#define SEND_UCHAR(value) TcpClient->SendBuf(&(uc = (value)), sizeof(uc), 0)
		#define SEND_USHORT(value) TcpClient->SendBuf(&(us = (value)), sizeof(us), 0)
		#define SEND_UINT(value) TcpClient->SendBuf(&(ui = (value)), sizeof(ui), 0)
		unsigned char b1;
		unsigned char b2;
		#define RECV_UCHAR(value) TcpClient->ReceiveBuf(&(value = 0xFF), sizeof(value), 0)

		// SOCKS4
		if(TypeProxyForm->ProxyType == PROXY_SOCKS4)
		{
			// Connect
			SEND_UCHAR(4); // Socks version
			SEND_UCHAR(1); // Connect command
			SEND_USHORT(UpdaterPort.ToInt());
			SEND_UINT(ProxyHostUI);
			SEND_UCHAR(0);
			RECV_UCHAR(b1); // Null byte
			RECV_UCHAR(b2); // Answer code
			if(b2 != 0x5A)
			{
				switch(b2)
				{
				case 0x5B: GaugeText("Proxy connection error, request rejected or failed"); break;
				case 0x5C: GaugeText("Proxy connection error, request failed because client is not running identd (or not reachable from the server)"); break;
				case 0x5D: GaugeText("Proxy connection error, request failed because client's identd could not confirm the user ID string in the request"); break;
				default: GaugeText(AnsiString("Proxy connection error, Unknown error ") + b2); break;
				}
				goto label_Error;
			}
		}
		// SOCKS5
		else if(TypeProxyForm->ProxyType == PROXY_SOCKS5)
		{
			SEND_UCHAR(5); // Socks version
			SEND_UCHAR(1); // Count methods
			SEND_UCHAR(2); // Method
			RECV_UCHAR(b1); // Socks version
			RECV_UCHAR(b2); // Method
			if(b2 == 2) // User/Password
			{
				SEND_UCHAR(1); // Subnegotiation version
				SEND_UCHAR(TypeProxyForm->ProxyUsername.Length()); // Name length
				TcpClient->SendBuf(TypeProxyForm->ProxyUsername.c_str(), TypeProxyForm->ProxyUsername.Length(), 0); // Name
				SEND_UCHAR(TypeProxyForm->ProxyPassword.Length()); // Pass length
				TcpClient->SendBuf(TypeProxyForm->ProxyPassword.c_str(), TypeProxyForm->ProxyPassword.Length(), 0); // Pass
				RECV_UCHAR(b1); // Subnegotiation version
				RECV_UCHAR(b2); // Status
				if(b2 != 0)
				{
					GaugeText("Invalid proxy user or password");
					goto label_Error;
				}
			}
			else if(b2 != 0) // Other authorization
			{
				GaugeText("Socks server connect fail");
				goto label_Error;
			}

			// Connect
			SEND_UCHAR(5); // Socks version
			SEND_UCHAR(1); // Connect command
			SEND_UCHAR(0); // Reserved
			SEND_UCHAR(1); // IP v4 address
			SEND_UINT(UpdaterPort.ToInt());
			SEND_USHORT(ProxyHostUI);
			RECV_UCHAR(b1); // Socks version
			RECV_UCHAR(b2); // Answer code
			if(b2 != 0)
			{
				switch(b2)
				{
				case 1: GaugeText("Proxy connection error, SOCKS-server error"); break;
				case 2: GaugeText("Proxy connection error, connections fail by proxy rules"); break;
				case 3: GaugeText("Proxy connection error, network is not aviable"); break;
				case 4: GaugeText("Proxy connection error, host is not aviable"); break;
				case 5: GaugeText("Proxy connection error, connection denied"); break;
				case 6: GaugeText("Proxy connection error, TTL expired"); break;
				case 7: GaugeText("Proxy connection error, command not supported"); break;
				case 8: GaugeText("Proxy connection error, address type not supported"); break;
				default: GaugeText(AnsiString("Proxy connection error, Unknown error ") + b2); break;
				}
				goto label_Error;
			}
		}
		// HTTP
		else if(TypeProxyForm->ProxyType == PROXY_HTTP)
		{
			char buf[0x1000];
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "CONNECT %s:%d HTTP/1.0\r\n\r\n", UpdaterHost.c_str(), UpdaterPort.ToInt());
			TcpClient->SendBuf(buf, strlen(buf), 0);
			memset(buf, 0, sizeof(buf));
			TcpClient->ReceiveBuf(buf, 0x1000 - 1, 0);
			if(!strstr(buf," 200 "))
			{
				GaugeText(AnsiString("Proxy connection error, receive message: ") + buf);
				goto label_Error;
			}
		}
		// Unknown
		else
		{
			goto label_Error;
		}
	}
	else
	{
		// Connect without proxy
		TcpClient->RemoteHost = UpdaterHost;
		TcpClient->RemotePort = UpdaterPort.ToInt();
		TcpClient->BlockMode = bmThreadBlocking;
		TcpClient->Active = true;
	}

	// Connect
	if(TypeProxyForm->ProxyType != PROXY_NONE || TcpClient->Connect())
	{
		TcpClient->Sendln("Hello2");
		if(TcpClient->Receiveln() == "Greetings")
		{
        	// Extra information
        	AnsiString exeName = TcpClient->Receiveln();
            AnsiString updaterUrl = TcpClient->Receiveln();
            gameServer = TcpClient->Receiveln();
            gameServerPort = TcpClient->Receiveln();
            TcpClient->Receiveln(); // Reserved
            TcpClient->Receiveln(); // Reserved
            TcpClient->Receiveln(); // Reserved

            // Navigate to new page
			if(UpdaterUrl != updaterUrl)
			{
				// Navigate to updater host
				UnicodeString updaterUrlW = UpdaterUrl;
				WebBrowser->Navigate(updaterUrlW.c_str());
			}

			// Save launcher configuration
            if(ExeName != exeName || UpdaterUrl != updaterUrl)
            {
            	HANDLE hf = CreateFile(OptionsFile.c_str(), 0, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
                if(hf != INVALID_HANDLE_VALUE) CloseHandle(hf);

            	SetConfigName(OptionsFile, "Options");

    			ExeName = exeName;
                UpdaterUrl = updaterUrl;

                SetString("ExeName", ExeName.c_str());
            	SetString("UpdaterHost", UpdaterHost.c_str());
  				SetString("UpdaterPort", UpdaterPort.c_str());
				SetString("UpdaterUrl", UpdaterUrl.c_str());

                Caption = ExeName + " Launcher";
				SetConfigName(ExeName + ".cfg", "Game Options");
            }

            // Request hashes
			TcpClient->Sendln("Give hashes list");
			AnsiString listStr = TcpClient->Receiveln();
			if(listStr == "Take hashes list" || listStr == "Take hashes list extended")
			{
				// Receive list and collect not equal hashes
				unsigned int hashesCount;
				if(!Receive((char*)&hashesCount, 4, false))
				{
					GaugeText("Ошибка при приеме информации о количестве файлов", "Error while receiving files count information");
					goto label_Error;
				}

				AnsiStringVec needFiles;
                AnsiStringVec needFilesOptions;
				bool extended = (listStr == "Take hashes list extended");

				for(;hashesCount; hashesCount--)
				{
					AnsiString fname = TcpClient->Receiveln();
					AnsiString hash = TcpClient->Receiveln();
					AnsiString fsize = (extended ? TcpClient->Receiveln() : AnsiString("0"));
					AnsiString options = (extended ? TcpClient->Receiveln() : AnsiString(""));
					if(fname == "" || hash == "")
					{
						GaugeText("Ошибка при приеме данных о файле", "Error while receiving file information");
						goto label_Error;
					}

					// Check need file or not
					bool getFile = false;

					HANDLE hf = CreateFile(fname.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					if(hf == INVALID_HANDLE_VALUE)
					{
						// File not found
						getFile = true;
					}
                    else if(options.Pos("<delete>") != 0)
					{
						// Delete file
                    	CloseHandle(hf);
                        DeleteFile(fname.c_str());
                    }
					else
					{
						// Check in cached hashes
						int curHash = GetCachedHash(fname);

						// Calculate new hash
						if(!curHash)
						{
							DWORD curSize = GetFileSize(hf, NULL);
							if(curSize && curSize != INVALID_FILE_SIZE)
							{
								char* buf = new char[curSize];
								DWORD rb;
								
								if(ReadFile(hf, buf, curSize, &rb, NULL) && rb == curSize)
								{
									curHash = Crc32((unsigned char*)buf, curSize);
									AddCachedHash(fname, curHash);
								}

								delete[] buf;
							}
						}

						if(!curHash || (curHash != hash.ToInt() && options.Pos("<norewrite>") == 0))
						{
							getFile = true;
						}

						CloseHandle(hf);
					}

					if(getFile)
					{
						needFiles.push_back(fname);
                        needFilesOptions.push_back(options);
						wholeSize += fsize.ToInt();
					}
				}

				// All hashes is equal
				if(needFiles.empty())
				{
					ShapeGauge->Width = ShapeGaugeBg->Width;
					GaugeText("Обновление не требуется", "Update not needed");
					TcpClient->Sendln("Bye");
					TcpClient->WaitForData(15000);
					goto label_Success;
				}

				// Drop progress
				ProgressCur = 0;
				ProgressMax = wholeSize + needFiles.size() * 4;

				// Founded not equal hashes, update this files
				for(size_t i = 0; i < needFiles.size(); i++)
				{
					AnsiString& fname = needFiles[i];
                    AnsiString& options = needFilesOptions[i];

					TcpClient->Sendln("Get");
					TcpClient->Sendln(fname);
					AnsiString answer = TcpClient->Receiveln();
					if(answer == "Catch" || answer == "Catchpack")
					{
						int size;
						if(!Receive((char*)&size, 4, true))
						{
							GaugeText("Ошибка при приеме информации о размере файла", "Error while receiving file size information");
							goto label_Error;
						}

						AnsiString sizeStr = FormatSize(size);
						GaugeText("Принимаем '" + fname + "' " + sizeStr, "Receiving '" + fname + "' " + sizeStr);

						TMemoryStream* stream = new TMemoryStream();
						stream->SetSize(size);
						if(!Receive((char*)stream->Memory, size, true))
						{
							GaugeText("Ошибка при приеме файла", "Error while receiving file");
							goto label_Error;
						}

						try
						{
							// Unpack
							if(answer == "Catchpack" || options.Pos("<pack>") != 0)
							{
								if(!Uncompress(stream))
								{
									GaugeText("Не удалось распаковать принятый файл", "Unable to unpack received file");
									goto label_Error;
								}
							}

							// Check folder availability
							CheckDirectory(fname.c_str());

							// Save to file
							if(fname == moduleName)
                    		{
								// Add special prefix and swap program in end of update
								stream->SaveToFile(fname + "SWAP");
							}
							else
							{
								// Other files
								stream->SaveToFile(fname);
							}

							// Refresh hash
							AddCachedHash(fname, Crc32((unsigned char*)stream->Memory, stream->Size));
						}
						catch(...)
						{
							GaugeText("Не удалось сохранить файл", "Unable to save file");
                            Sleep((unsigned)1000);
						}
					   	delete stream;
					}
					else
					{
						GaugeText("Неизвестное сообщение при приеме файлов", "Unknown message on files receiving");
						goto label_Error;
					}
				}

				// Done
				GaugeText("Обновление завершено", "Update complete");
				TcpClient->Sendln("Bye");
				TcpClient->WaitForData(15000);
				goto label_Success;
			}
			else
			{
				GaugeText("Сервер не выслал список файлов", "Server did not provide the hash list");
				goto label_Error;
			}
		}
		else
		{
			GaugeText("Сервер не отвечает", "Server response fail");
			goto label_Error;
		}
	}
	else
	{
		GaugeText("Невозможно подключиться к серверу обновлений", "Can't connect to update server");
		goto label_Error;
	}

label_Error:
	ShapeGaugeBg->Brush->Color = clMaroon;
	goto label_Finish;

label_Success:
	BtnPlay->Enabled = true;
	BtnOptions->Enabled = true;
	BtnPlay->SetFocus();
	goto label_Finish;

label_Finish:
	TcpClient->Active = false;

    // Game host configuration, check after getting game config
	if(gameServer != "" && gameServer != GetString("RemoteHost", "")) SetString("RemoteHost", gameServer.c_str());
	if(gameServerPort != "" && gameServerPort != GetString("RemotePort", "")) SetString("RemotePort", gameServerPort.c_str());

	// Proxy settings
	SetInt("ProxyType", TypeProxyForm->ProxyType);
	SetString("ProxyHost", TypeProxyForm->ProxyHost);
	SetInt("ProxyPort", TypeProxyForm->ProxyPort);
	SetString("ProxyUser", TypeProxyForm->ProxyUsername);
	SetString("ProxyPass", TypeProxyForm->ProxyPassword);

    // Resave language
    SetString("Language", IsEnglish ? "engl" : "russ");

    // Check for update self
    UpdateSelf();
}
//---------------------------------------------------------------------------
void TUpdaterForm::LoadCachedHashes()
{
	FILE* f = fopen(".\\data\\cache\\file_hashes.lst", "rt");
	if(!f) return;

	char name[2048];
	int hash;
	while(fscanf(f, "%s%d", name, &hash) == 2)
	{
		CachedHashes.push_back(name);
		CachedHashes.push_back(AnsiString("") + hash);
	}
	fclose(f);
}
//---------------------------------------------------------------------------
int TUpdaterForm::GetCachedHash(const AnsiString& name)
{
	for(AnsiStringVecIt it = CachedHashes.begin(); it != CachedHashes.end();)
	{
		const AnsiString& cachedName = *it++;
		const AnsiString& cachedHash = *it++;
		if(cachedName == name) return cachedHash.ToInt();
	}
	return 0;
}
//---------------------------------------------------------------------------
void TUpdaterForm::AddCachedHash(const AnsiString& name, int hash)
{
	// Check for exiting
	for(AnsiStringVecIt it = CachedHashes.begin(); it != CachedHashes.end();)
	{
		if(*it == name)
		{
			it = CachedHashes.erase(it);
			it = CachedHashes.erase(it);
		}
		else
		{
			it++;
			it++;
		}
	}

	// Add
	CachedHashes.push_back(name);
	CachedHashes.push_back(hash);

	// Resave cache
	FILE* f = fopen(".\\data\\cache\\file_hashes.lst", "wt");
	if(!f) return;
	for(AnsiStringVecIt it = CachedHashes.begin(); it != CachedHashes.end();)
	{
		const AnsiString& cachedName = *it++;
		const AnsiString& cachedHash = *it++;
		fprintf(f, "%-80s %d\t\n", cachedName.c_str(), cachedHash.ToInt());
	}
	fclose(f);
}
//---------------------------------------------------------------------------
void TUpdaterForm::UpdateSelf()
{
	// Self path
    char modulePathName[2048];
 	GetModuleFileName(NULL, modulePathName, 2048);

    // Self file name
    const char* moduleName = NULL;
    for(size_t i = 0, j = strlen(modulePathName); i < j; i++)
    	if(modulePathName[i] == '\\') moduleName = &modulePathName[i + 1];

    // Swap path
    char modulePathNameSWAP[2048];
    strcpy(modulePathNameSWAP, modulePathName);
    strcat(modulePathNameSWAP, "SWAP");

    // Check swap file availability
    if(GetFileAttributes(modulePathNameSWAP) == DWORD(-1)) return;

    // Bat file name
    char batFile[2048];
    srand(GetTickCount());
	sprintf(batFile, ".\\data\\cache\\updateself%d.bat", rand());

    // Bat commands
    char wait[] = {"ping 127.0.0.1 -n 2 >nul"};
    char batLines[2048 * 7];
    sprintf(batLines, "@echo off\n%s\n"
    	":try\ndel \"%s\"\nif exist \"%s\" goto try\n%s\n"
    	"ren \"%s\" \"%s\"\n%s\n"
		"\"%s\"\n"
        "del \"%s\"",
        wait,
    	modulePathName, modulePathName, wait, // Delete current file
        modulePathNameSWAP, moduleName, wait, // Rename swap to normal
        modulePathName,                       // Start launcher
        batFile);                             // Delete bat file

    // Write bat file
 	HANDLE f = CreateFile(batFile, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ
		| FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD bw;
 	WriteFile(f, batLines, strlen(batLines), &bw, NULL);
 	CloseHandle(f);

    // Run bat file
    ShellExecute(NULL, "open", batFile, NULL, NULL, SW_SHOW);

    // Destroy self
    ExitProcess(0);
}
//---------------------------------------------------------------------------

