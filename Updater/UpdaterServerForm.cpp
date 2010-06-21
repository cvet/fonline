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
//---------------------------------------------------------------------------
__fastcall TServerForm::TServerForm(TComponent* Owner)
	: TForm(Owner)
{
	ServerForm->Caption="Updater server     v1.9";
	ConnAll=0;
	ConnCur=0;
	ConnSucc=0;
	ConnFail=0;
	BytesSend=0;
	FilesSend=0;
	BufferSize=0;
	UpdateInfo();
	InitCrc32();
	RecalcFiles();
}
//---------------------------------------------------------------------------
void TServerForm::UpdateInfo()
{
	LConnAll->Caption=ConnAll;
	LConnCur->Caption=ConnCur;
	LConnSucc->Caption=ConnSucc;
	LConnFail->Caption=ConnFail;
	LBytesSend->Caption=BytesSend;
	LFilesSend->Caption=FilesSend;
}
//---------------------------------------------------------------------------
void __fastcall TServerForm::BProcessClick(TObject *Sender)
{
	SwitchListen();       
}
//---------------------------------------------------------------------------
void TServerForm::ConnStart(TIdContext* context, const AnsiString& infoMessage)
{
	TIdSocketHandle* sock=context->Binding();
	LOG(sock->PeerIP+" <info> "+infoMessage);
	ConnAll++;
	ConnCur++;
	UpdateInfo();
}
//---------------------------------------------------------------------------
void TServerForm::ConnError(TIdContext* context, const AnsiString& errorMessage)
{
	if(((ContextInfo*)context->Data)->IsClosed) return;
	TIdSocketHandle* sock=context->Binding();
	LOG(sock->PeerIP+" <error> "+errorMessage);
	ConnFail++;
	ConnCur--;
	UpdateInfo();
	((ContextInfo*)context->Data)->IsClosed=true;
	if(context->Data)
	{
		delete context->Data;
		context->Data=NULL;
	}
	context->Connection->Disconnect();
}
//---------------------------------------------------------------------------
void TServerForm::ConnEnd(TIdContext* context, const AnsiString& infoMessage)
{
	if(((ContextInfo*)context->Data)->IsClosed) return;
	TIdSocketHandle* sock=context->Binding();
	LOG(sock->PeerIP+" <info> "+infoMessage);
	context->Connection->Disconnect();
	ConnSucc++;
	ConnCur--;
	UpdateInfo();
	((ContextInfo*)context->Data)->IsClosed=true;
}
//---------------------------------------------------------------------------
void TServerForm::ConnInfo(TIdContext* context, const AnsiString& infoMessage)
{
	TIdSocketHandle* sock=context->Binding();
	LOG(sock->PeerIP+" <info> "+infoMessage);
}
//---------------------------------------------------------------------------
void __fastcall TServerForm::IdTCPServerExecute(TIdContext *AContext)
{
	int& version=((ContextInfo*)AContext->Data)->Version;
	TIdIOHandler* handler=AContext->Connection->IOHandler;
	AnsiString msg=handler->ReadLn();
	if(msg=="Hello" || msg=="Hello1")
	{
		// Identify
		handler->WriteLn("Greetings");
		ConnInfo(AContext,"Begin work.");
		if(msg=="Hello1") version=1;
	}
	else if(msg=="Give hashes list")
	{
		// List of files and hashes
		handler->WriteLn("Take hashes list");
		unsigned int hashesCount=PrepFiles.size();
		handler->Write((unsigned)hashesCount,false);
		for(unsigned int i=0;i<PrepFiles.size();i++)
		{
			PrepFile& pf=PrepFiles[i];
			handler->WriteLn(pf.FName);
			handler->WriteLn(pf.FHash);
		}
		ConnInfo(AContext,"Send files list.");
	}
	else if(msg=="Get")
	{
		AnsiString fname=handler->ReadLn();

		for(unsigned int i=0;i<PrepFiles.size();i++)
		{
			PrepFile& pf=PrepFiles[i];
			// Send file
			if(pf.FName==fname)
			{
				int size;
				if(version>=1 && pf.Stream!=pf.StreamPack)
				{
					handler->WriteLn("Catchpack");
					size=pf.StreamPack->Size;
					handler->Write((unsigned)size,false);
					handler->Write(pf.StreamPack);
					ConnInfo(AContext,"Send packed file <"+pf.FName+">.");
				}
				else
				{
					handler->WriteLn("Catch");
					size=pf.Stream->Size;
					handler->Write((unsigned)size,false);
					handler->Write(pf.Stream);
					ConnInfo(AContext,"Send file <"+pf.FName+">.");
				}
				BytesSend+=4+size;
				FilesSend++;
				UpdateInfo();
				break;
			}

			// Not found
			if(i==PrepFiles.size()-1) ConnError(AContext,"Unknown file name <"+fname+">.");
		}
	}
	else if(msg=="Bye")
	{
		ConnEnd(AContext,"End of work.");
	}
	else
	{
		ConnError(AContext,"Unknown message <"+msg+">.");
	}
}
//---------------------------------------------------------------------------
void __fastcall TServerForm::BRecalcFilesClick(TObject *Sender)
{
	RecalcFiles();
}
//---------------------------------------------------------------------------
void TServerForm::SwitchListen()
{
	if(IdTCPServer->Active==false)
	{
		IdTCPServer->DefaultPort=CseListenPort->Value;
		IdTCPServer->Active=true;
		if(IdTCPServer->Active==true)
		{
			BProcess->Caption="Stop listen";
			ShapeIndicator->Brush->Color=clLime;
		}
		else
		{
			LOG("Unable bind port.");
		}
	}
	else
	{
		IdTCPServer->Active=false;
		BProcess->Caption="Start listen";
		ShapeIndicator->Brush->Color=clRed;
	}
}
//---------------------------------------------------------------------------
void TServerForm::RecalcFiles()
{
	LOG("Begin load files...");
	BufferSize=0;
	for(unsigned int i=0;i<PrepFiles.size();i++)
	{
		delete PrepFiles[i].Stream;
		if(PrepFiles[i].StreamPack!=PrepFiles[i].Stream) delete PrepFiles[i].StreamPack;
	}
	PrepFiles.clear();
	LbList->Items->Clear();

	try
	{
		LbList->Items->LoadFromFile("UpdateFiles.lst");
	}
	catch(...)
	{
		LOG("UpdateFiles.lst not found.");
		return;
	}

	for(int i=0;i<LbList->Items->Count;i++)
	{
		AnsiString fname=(*LbList->Items)[i];
		try
		{
			// Open and calcuate hash
			TMemoryStream* stream=new TMemoryStream();
			stream->LoadFromFile("Update\\"+fname);
			unsigned int hash=Crc32((unsigned char*)stream->Memory,stream->Size);
			LbList->Items->Strings[i]=fname+" loaded, hash "+hash;

			// Pack
			TMemoryStream* streamPack=new TMemoryStream();
			streamPack->SetSize((int)stream->Size);
			CopyMemory(streamPack->Memory,stream->Memory,stream->Size);
			if(Compress(streamPack))
			{
				// Check useless packing
				if(streamPack->Size>=stream->Size*90/100) // 10% diff
				{
					delete streamPack;
					streamPack=stream;
				}

				// Add to list
				PrepFile pf;
				pf.FName=fname;
				pf.FHash=hash;
				pf.Stream=stream;
				pf.StreamPack=streamPack;
				PrepFiles.push_back(pf);
				if(streamPack->Size>BufferSize) BufferSize=streamPack->Size;
			}
			else
			{
				LOG("Unable to complress <"+fname+">.");
				LbList->Items->Strings[i]=fname+" unable to pack.";
				delete stream;
				delete streamPack;
			}
		}
		catch(...)
		{
			LOG("File <"+fname+"> not found.");
			LbList->Items->Strings[i]=fname+" missed";
		}
	}
	BufferSize+=100000; // Extra 100kb
	LOG("End of loading.");
}
//---------------------------------------------------------------------------
void __fastcall TServerForm::IdTCPServerConnect(TIdContext *AContext)
{
	ConnStart(AContext,"Connected.");
	AContext->Data=new ContextInfo();
	((ContextInfo*)AContext->Data)->Version=0;
	((ContextInfo*)AContext->Data)->IsClosed=false;
	AContext->Connection->IOHandler->SendBufferSize=BufferSize;
}
//---------------------------------------------------------------------------
void __fastcall TServerForm::IdTCPServerDisconnect(TIdContext *AContext)
{
	ConnError(AContext,"Connection closed.");

}
//---------------------------------------------------------------------------
void __fastcall TServerForm::IdTCPServerException(TIdContext *AContext,
	  Exception *AException)
{
	ConnError(AContext,"Exception \""+AException->Message+"\".");
}
//---------------------------------------------------------------------------

