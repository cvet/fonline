//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "UpdaterServerForm.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "cspin"
#pragma link "IdCustomTCPServer"
#pragma link "IdCustomTCPServer"
#pragma resource "*.dfm"
TServerForm *ServerForm;
void __cdecl RecalcFilesThread(void*);
//---------------------------------------------------------------------------
__fastcall TServerForm::TServerForm(TComponent* Owner)
	: TForm(Owner)
{
	RestoreMainDirectory();
	SetConfigName("UpdaterServer.cfg", "Options");
	DoubleBuffered = true;
	Caption = "Updater server     v2.0";
	ConnAll = 0;
	ConnCur = 0;
	ConnSucc = 0;
	ConnFail = 0;
	BytesSent = 0;
	FilesSent = 0;
	Pings = 0;
	LPings->Caption = "0";
	UpdateInfo();
	InitCrc32();
}
//---------------------------------------------------------------------------
void __fastcall TServerForm::FormShow(TObject *Sender)
{
	static bool recalc = false;
	if(!recalc)
	{
		recalc = true;
		_beginthread(RecalcFilesThread, 0, this);
    }
}
//---------------------------------------------------------------------------
void TServerForm::UpdateInfo()
{
	LConnAll->Caption = ConnAll;
	LConnCur->Caption = ConnCur;
	LConnSucc->Caption = ConnSucc;
	LConnFail->Caption = ConnFail;
	LBytesSent->Caption = BytesSent / 1024 / 1024;
	LFilesSent->Caption = FilesSent;
}
//---------------------------------------------------------------------------
void __fastcall TServerForm::BProcessClick(TObject *Sender)
{
	SwitchListen();       
}
//---------------------------------------------------------------------------
void TServerForm::ConnStart(TIdContext* context, const AnsiString& infoMessage)
{
	TIdSocketHandle* sock = context->Binding;
	LOG(sock->PeerIP + " <info> " + infoMessage);
	ConnAll++;
	ConnCur++;
	UpdateInfo();
}
//---------------------------------------------------------------------------
void TServerForm::ConnError(TIdContext* context, const AnsiString& errorMessage)
{
	if(((ContextInfo*)context->Data)->IsClosed) return;
	TIdSocketHandle* sock = context->Binding;
	LOG(sock->PeerIP + " <error> " + errorMessage);
	ConnFail++;
	ConnCur--;
	UpdateInfo();
	((ContextInfo*)context->Data)->IsClosed = true;
	if(context->Data)
	{
		delete context->Data;
		context->Data = NULL;
	}
	context->Connection->Disconnect();
}
//---------------------------------------------------------------------------
void TServerForm::ConnEnd(TIdContext* context, const AnsiString& infoMessage)
{
	if(((ContextInfo*)context->Data)->IsClosed) return;
	TIdSocketHandle* sock = context->Binding;
	LOG(sock->PeerIP + " <info> " + infoMessage);
	context->Connection->Disconnect();
	ConnSucc++;
	ConnCur--;
	UpdateInfo();
	((ContextInfo*)context->Data)->IsClosed = true;
}
//---------------------------------------------------------------------------
void TServerForm::ConnInfo(TIdContext* context, const AnsiString& infoMessage)
{
	TIdSocketHandle* sock = context->Binding;
	LOG(sock->PeerIP + " <info> " + infoMessage);
}
//---------------------------------------------------------------------------
void __fastcall TServerForm::IdTCPServerExecute(TIdContext *AContext)
{
	int& version=((ContextInfo*)AContext->Data)->Version;
	TIdIOHandler* handler = AContext->Connection->IOHandler;
	AnsiString msg = handler->ReadLn();
	if(msg == "Hello" || msg == "Hello1" || msg == "Hello2")
	{
		// Identify
		handler->WriteLn("Greetings");
		ConnInfo(AContext, "Begin work.");
		if(msg == "Hello1") version = 1;
		if(msg == "Hello2") version = 2;

        // Extra information
        if(version >= 2)
        {
        	handler->WriteLn(GameFileName);
            handler->WriteLn(UpdaterURL);
            handler->WriteLn(GameServer);
            handler->WriteLn(GameServerPort);
            handler->WriteLn(""); // Reserved
            handler->WriteLn(""); // Reserved
            handler->WriteLn(""); // Reserved
        }
	}
	else if(msg == "Give hashes list")
	{
		// List of files and hashes
		if(version >= 2)
			handler->WriteLn("Take hashes list extended");
		else
			handler->WriteLn("Take hashes list");

		unsigned int hashesCount = PrepFiles.size();
		handler->Write((unsigned)hashesCount, false);
		for(unsigned int i = 0; i < PrepFiles.size(); i++)
		{
			PrepFile& pf = PrepFiles[i];

			if(version >= 2)
			{
				// Extended
				handler->WriteLn(pf.FName);
				handler->WriteLn(pf.FHash);
				handler->WriteLn(pf.FSize);
				handler->WriteLn(pf.FOptions);
			}
			else
			{
				handler->WriteLn(pf.FName);
				unsigned int ui = (unsigned int)pf.FHash.ToInt();
				handler->WriteLn(AnsiString("") + ui);
            }
		}
		ConnInfo(AContext, "Send files list.");
	}
	else if(msg == "Get" || msg == "Get unpacked")
	{
		AnsiString fname = handler->ReadLn();

		for(unsigned int i = 0; i < PrepFiles.size(); i++)
		{
			PrepFile& pf = PrepFiles[i];

			// Send file
			if(pf.FName == fname)
			{
				TStream* stream = pf.Stream;
				unsigned int size = (stream ? stream->Size : 0);

                if(!pf.Packed)
					handler->WriteLn("Catch");
                else
					handler->WriteLn("Catchpack");

				handler->Write(size, false);
				while(size > 0)
				{
					unsigned int portion = size < SEND_PORTION ? size : SEND_PORTION;

					pf.StreamLocker->Enter();
					stream->Seek(stream->Size - size, soBeginning);
					stream->Read(&pf.StreamBuffer[0], portion);
					pf.StreamLocker->Leave();

					handler->Write(pf.StreamBuffer, portion, 0);

					BytesSent += portion;
					LBytesSent->Caption = BytesSent / 1024 / 1024;

					size -= portion;
				}

				ConnInfo(AContext, "Sent file <" + pf.FName + ">.");

				BytesSent += 4;
				FilesSent++;
				UpdateInfo();
				break;
			}

			// Not found
			if(i == PrepFiles.size() - 1)
				ConnError(AContext, "Unknown file name <" + fname + ">.");
		}
	}
	else if(msg == "Bye")
	{
		ConnEnd(AContext, "End of work.");
	}
	else if(msg == "Ping")
	{
		handler->WriteLn("PingOk");

		Pings++;
		LPings->Caption = Pings;
	}
	else
	{
		ConnError(AContext, "Unknown message <" + msg + ">.");
	}
}
//---------------------------------------------------------------------------
void __fastcall TServerForm::BRecalcFilesClick(TObject *Sender)
{
	_beginthread(RecalcFilesThread, 0, this);
}
//---------------------------------------------------------------------------
void TServerForm::SwitchListen()
{
	if(!IdTCPServer->Active)
	{
		// Start listening
		LOG("Start listening.");

		IdTCPServer->DefaultPort = CseListenPort->Value;
		IdTCPServer->Active = true;

		// Check result
		if(IdTCPServer->Active)
		{
			BProcess->Caption = "Stop listening";
			ShapeIndicator->Brush->Color = clLime;
			CseListenPort->Enabled = false;
		}
		else
		{
			LOG("Unable bind port.");
		}
	}
	else
	{
		// Stop listening
		LOG("Stop listening.");

        TList* list = IdTCPServer->Contexts->LockList();
        for(int i = 0; i < list->Count; i++)
        {
        	TIdContext* context = (TIdContext*)(*list)[i];
            context->Connection->Disconnect();
        }
        IdTCPServer->Contexts->UnlockList();
		IdTCPServer->Active = false;
		
		BProcess->Caption = "Start listening";
		ShapeIndicator->Brush->Color = clRed;
		CseListenPort->Enabled = true;
	}
}
//---------------------------------------------------------------------------
void TServerForm::LoadConfigArray(AnsiStringVec& cfg, AnsiString keyName)
{
	cfg.clear();

	AnsiString str = GetString(keyName.c_str(), "");
	if(!str.Length()) return;

	const char* token = strtok(str.c_str(), ",");
	while(token)
	{
		char* tmpToFree = strdup(token);
		char* tmp = tmpToFree;

		EraseFrontBackSpecificChars(tmp);
		if(tmp[0] == '.' && (tmp[1] == '\\' || tmp[1] == '/')) tmp = tmp + 2;

		cfg.push_back(tmp);

		free(tmpToFree);
		token = strtok(NULL, ",");
	}
}
//---------------------------------------------------------------------------
bool TServerForm::CheckName(const char* name, AnsiStringVec& where)
{
	for(AnsiStringVecIt it = where.begin(); it != where.end(); it++)
    {
    	const AnsiString& mask = *it;
        const char* name_ = name;

    	// Compare base name
		if(SpecialCompare(name_, mask.c_str())) return true;

        // Compare all parts of name
    	if(mask.Pos("*") != 0 || mask.Pos("?") != 0)
        {
        	for(name_++; *name_; name_++)
            	if(SpecialCompare(name_, mask.c_str())) return true;
        }
    }
	return false;
}
//---------------------------------------------------------------------------
void TServerForm::RecursiveDirLook(const char* initDir, const char* curDir, const char* query)
{
	// initDir ..\\..\\Client\\
	// curDir  data\\
	// query   *

	AnsiStringVec subDirs;

	WIN32_FIND_DATA fd;
	char buf[2048];
	sprintf(buf, "%s%s%s", initDir, curDir, query);

	HANDLE h = FindFirstFile(buf, &fd);
	while(h != INVALID_HANDLE_VALUE)
	{
		if(fd.cFileName[0] != '.')
		{
			if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
            	// Collect subdirs
				if(!CheckName(fd.cFileName, CfgIgnore)) subDirs.push_back(fd.cFileName);
			}
			else
			{
				sprintf(buf, "%s%s", curDir, fd.cFileName);
				if(!CheckName(buf, CfgIgnore) && !CheckName(fd.cFileName, CfgIgnore))
				{
					AnsiString fname = buf;
					AnsiString options;

					try
					{
						TStream* stream = NULL;
                        int hash = 0;
                        int size = 0;
                        bool packed = false;

						if(!CheckName(fname.c_str(), CfgDelete))
						{
							// Generate stream
							if(CheckName(fname.c_str(), CfgHddInsteadRam))
							{
								// HDD streaming
								TFileStream* fileStream = new TFileStream(AnsiString(initDir) + fname, Sysutils::fmOpenRead);
                                hash = Crc32(fileStream);

								options += "<hdd>";

								stream = fileStream;
                                size = stream->Size;
							}
							else
							{
								// Open and calcuate hash
								TMemoryStream* memoryStream = new TMemoryStream();
								memoryStream->LoadFromFile(AnsiString(initDir) + fname);
                                hash = Crc32(memoryStream);

								// Pack
								if(CheckName(fname.c_str(), CfgPack) && Compress(memoryStream))
								{
									options += "<pack>";
                                    packed = true;
								}

								stream = memoryStream;
                                size = stream->Size;
							}
						}
						else
						{
                        	// Without stream
                        	options += "<delete>";
						}

						if(CheckName(fname.c_str(), CfgNoRewrite)) options += "<norewrite>";

						// Add to list
						PrepFile pf;
						pf.FName = fname;
						pf.FHash = hash;
						pf.FSize = size;
						pf.FOptions = options;
						pf.Stream = stream;
						pf.StreamLocker = new TCriticalSection();
						pf.StreamBuffer.Length = size < SEND_PORTION ? size : SEND_PORTION;
						pf.Packed = packed;
						PrepFiles.push_back(pf);

						char listLine[2048];
						sprintf(listLine, "%-50s %s", fname.c_str(), options.c_str());
						LbList->Items->Add(listLine);
						LbList->TopIndex = LbList->Items->Count - 1;
					}
					catch(...)
					{
						LOG("Fail to load file '" + fname + "'.");
					}
				}
			}
		}

		if(!FindNextFile(h, &fd)) break;
	}
	if(h != INVALID_HANDLE_VALUE) FindClose(h);

	// Check subdirs
	for(AnsiStringVecIt it = subDirs.begin(); it != subDirs.end(); it++)
	{
		sprintf(buf, "%s%s\\", curDir, (*it).c_str());
		RecursiveDirLook(initDir, buf, query);
	}
}
//---------------------------------------------------------------------------
void __cdecl RecalcFilesThread(void* ptr)
{
	TServerForm* form = (TServerForm*)ptr;
	form->RecalcFiles();
}
//---------------------------------------------------------------------------
void TServerForm::RecalcFiles()
{
	// Shutdown current connection
	bool startListen = false;
	if(IdTCPServer->Active)
	{
		SwitchListen();
		startListen = true;
	}

	// Begin indexing
	LOG("Indexing files. Wait.");
	BProcess->Enabled = false;
	BRecalcFiles->Enabled = false;
	CseListenPort->Enabled = false;

	// Parse config
    GameFileName = GetString("GameFileName", "FOnline");
    UpdaterURL = GetString("UpdaterURL", DEFAULT_WEB_HOST);
    GameServer = GetString("GameServer", "localhost");
    GameServerPort = GetString("GameServerPort", "4040");
	LoadConfigArray(CfgContent, "Content");
	if(CfgContent.empty()) CfgContent.push_back("");
	LoadConfigArray(CfgQuery, "Query");
	if(CfgQuery.empty()) CfgQuery.push_back("*");
	LoadConfigArray(CfgIgnore, "Ignore");
	LoadConfigArray(CfgPack, "Pack");
	LoadConfigArray(CfgNoRewrite, "NoRewrite");
	LoadConfigArray(CfgDelete, "Delete");
	LoadConfigArray(CfgHddInsteadRam, "HddInsteadRam");

	// Clear current collection
	LbList->Items->Clear();
	for(size_t i = 0; i < PrepFiles.size(); i++)
		if(PrepFiles[i].Stream) delete PrepFiles[i].Stream;
	PrepFiles.clear();

	// Foreach content queries
	for(AnsiStringVecIt itC = CfgContent.begin(); itC != CfgContent.end(); itC++)
	{
		for(AnsiStringVecIt itQ = CfgQuery.begin(); itQ != CfgQuery.end(); itQ++)
		{
			RecursiveDirLook((*itC).c_str(), "", (*itQ).c_str());
		}
	}

	// Count whole size
	int wholeSize = 0;
	for(PrepFileVecIt it = PrepFiles.begin(); it != PrepFiles.end(); it++)
		wholeSize += (*it).Stream->Size;
	wholeSize = wholeSize / 1024 / 1024;

	// End of indexing
	LOG(AnsiString("Indexed ") + LbList->Items->Count + " files, whole size " + wholeSize + " mb.");
	LbList->TopIndex = 0;
	BProcess->Enabled = true;
	BRecalcFiles->Enabled = true;
	CseListenPort->Enabled = true;

	// Start listen
	if(startListen) SwitchListen();
}
//---------------------------------------------------------------------------
void __fastcall TServerForm::IdTCPServerConnect(TIdContext *AContext)
{
	ConnStart(AContext, "Connected.");
	AContext->Data = new ContextInfo();
	((ContextInfo*)AContext->Data)->Version = 0;
	((ContextInfo*)AContext->Data)->IsClosed = false;
}
//---------------------------------------------------------------------------
void __fastcall TServerForm::IdTCPServerDisconnect(TIdContext *AContext)
{
	ConnError(AContext, "Connection closed.");
}
//---------------------------------------------------------------------------
void __fastcall TServerForm::IdTCPServerException(TIdContext *AContext,
	  Exception *AException)
{
	ConnError(AContext, "Exception \"" + AException->Message + "\".");
}
//---------------------------------------------------------------------------

