#include "StdAfx.h"
#include "Server.h"

void* zlib_alloc(void *opaque, unsigned int items, unsigned int size){return calloc(items, size);}
void zlib_free(void *opaque, void *address){free(address);}

#define MAX_CLIENTS_IN_GAME		(3000)

#ifdef FOSERVER_DUMP
HANDLE hDump;
#endif

int FOServer::UpdateIndex=-1;
int FOServer::UpdateLastIndex=-1;
DWORD  FOServer::UpdateLastTick=0;
ClVec FOServer::LogClients;
HANDLE FOServer::IOCompletionPort=NULL;
HANDLE* FOServer::IOThreadHandles=NULL;
DWORD FOServer::WorkThreadCount=0;
SOCKET FOServer::ListenSock=INVALID_SOCKET;
ClVec FOServer::ConnectedClients;
MutexSpinlock FOServer::ConnectedClientsLocker;
FOServer::Statistics_ FOServer::Statistics;
FOServer::ClientSaveDataVec FOServer::ClientsSaveData;
size_t FOServer::ClientsSaveDataCount=0;
PByteVec FOServer::WorldSaveData;
size_t FOServer::WorldSaveDataBufCount=0;
size_t FOServer::WorldSaveDataBufFreeSize=0;
FILE* FOServer::DumpFile=NULL;
HANDLE FOServer::DumpBeginEvent=NULL;
HANDLE FOServer::DumpEndEvent=NULL;
DWORD FOServer::SaveWorldIndex=0;
DWORD FOServer::SaveWorldTime=0;
DWORD FOServer::SaveWorldNextTick=0;
DwordVec FOServer::SaveWorldDeleteIndexes;
HANDLE FOServer::DumpThreadHandle=NULL;
SYSTEM_INFO FOServer::SystemInfo;
HANDLE* FOServer::LogicThreadHandles=NULL;
DWORD FOServer::LogicThreadCount=0;
bool FOServer::LogicThreadSetAffinity=false;
bool FOServer::RequestReloadClientScripts=false;
LangPackVec FOServer::LangPacks;
FOServer::HoloInfoMap FOServer::HolodiskInfo;
MutexSpinlock FOServer::HolodiskLocker;
DWORD FOServer::LastHoloId=0;
FOServer::TimeEventVec FOServer::TimeEvents;
DWORD FOServer::TimeEventsLastNum=0;
MutexSpinlock FOServer::TimeEventsLocker;
FOServer::AnyDataMap FOServer::AnyData;
MutexSpinlock FOServer::AnyDataLocker;
StrVec FOServer::ServerWrongGlobalObjects;
FOServer::TextListenVec FOServer::TextListeners;
MutexSpinlock FOServer::TextListenersLocker;
DwordVec* FOServer::RadioChannels[0x10000]={0};
MutexSpinlock FOServer::RadioLocker;
HWND FOServer::ServerWindow=NULL;
bool FOServer::Active=false;
FileManager FOServer::FileMngr;
ClVec FOServer::SaveClients;
MutexSpinlock FOServer::SaveClientsLocker;
DwordMap FOServer::RegIp;
MutexSpinlock FOServer::RegIpLocker;
DWORD FOServer::VarsGarbageLastTick=0;
StrVec FOServer::AccessClient;
StrVec FOServer::AccessTester;
StrVec FOServer::AccessModer;
StrVec FOServer::AccessAdmin;
FOServer::ClientBannedVec FOServer::Banned;
Mutex FOServer::BannedLocker;
FOServer::ClientDataVec FOServer::ClientsData;
MutexSpinlock FOServer::ClientsDataLocker;
volatile DWORD FOServer::LastClientId=0;
ScoreType FOServer::BestScores[SCORES_MAX]={0};
MutexSpinlock FOServer::BestScoresLocker;
FOServer::SingleplayerSave_ FOServer::SingleplayerSave;
MutexSynchronizer FOServer::LogicThreadSync;

FOServer::FOServer()
{
	Active=false;
	ZeroMemory(&Statistics,sizeof(Statistics));
	ZeroMemory(&ServerFunctions,sizeof(ServerFunctions));
	ZeroMemory(RadioChannels,sizeof(RadioChannels));
	ServerWindow=NULL;
	LastClientId=0;
	SingleplayerSave.Valid=false;
	VarsGarbageLastTick=0;
	RequestReloadClientScripts=false;
	MEMORY_PROCESS(MEMORY_STATIC,sizeof(FOServer));
}

FOServer::~FOServer()
{
#ifdef FOSERVER_DUMP
	CloseHandle(hDump);
#endif
}

void FOServer::Finish()
{
	if(!Active) return;

	// World dumper
	if(WorldSaveManager)
	{
		WaitForSingleObject(DumpEndEvent,INFINITE);
		MEMORY_PROCESS(MEMORY_SAVE_DATA,-(int)ClientsSaveData.size()*sizeof(ClientSaveData));
		ClientsSaveData.clear();
		ClientsSaveDataCount=0;
		MEMORY_PROCESS(MEMORY_SAVE_DATA,-(int)WorldSaveData.size()*WORLD_SAVE_DATA_BUFFER_SIZE);
		for(size_t i=0;i<WorldSaveData.size();i++) delete[] WorldSaveData[i];
		WorldSaveData.clear();
		WorldSaveDataBufCount=0;
		WorldSaveDataBufFreeSize=0;
		CloseHandle(DumpBeginEvent);
		DumpBeginEvent=NULL;
		CloseHandle(DumpEndEvent);
		DumpEndEvent=NULL;
		//WaitForSingleObject(DumpThreadHandle,INFINITE);
		CloseHandle(DumpThreadHandle);
		DumpThreadHandle=NULL;
	}
	if(DumpFile) fclose(DumpFile);
	DumpFile=NULL;
	SaveWorldIndex=0;
	SaveWorldTime=0;
	SaveWorldNextTick=0;

	// End script
	if(Script::PrepareContext(ServerFunctions.Finish,CALL_FUNC_STR,"Game")) Script::RunPrepared();

	// Logging clients
	LogFinish(LOG_FUNC);
	for(ClVecIt it=LogClients.begin(),end=LogClients.end();it!=end;++it) (*it)->Release();
	LogClients.clear();

	// Net
	ConnectedClientsLocker.Lock();
	for(ClVecIt it=ConnectedClients.begin(),end=ConnectedClients.end();it!=end;++it)
	{
		Client* cl=*it;
		cl->Shutdown();
	}
	ConnectedClientsLocker.Unlock();
	closesocket(ListenSock);
	ListenSock=INVALID_SOCKET;
	for(DWORD i=0;i<WorkThreadCount;i++) PostQueuedCompletionStatus(IOCompletionPort,0,1,NULL);
	WaitForMultipleObjects(WorkThreadCount+1,IOThreadHandles,TRUE,INFINITE);
	for(DWORD i=0;i<WorkThreadCount;i++) CloseHandle(IOThreadHandles[i]);
	SAFEDELA(IOThreadHandles);
	WorkThreadCount=0;
	CloseHandle(IOCompletionPort);
	IOCompletionPort=NULL;

	// Managers
	AIMngr.Finish();
	MapMngr.Finish();
	CrMngr.Finish();
	ItemMngr.Finish();
	DlgMngr.Finish();
	VarMngr.Finish();
	FinishScriptSystem();
	FinishLangPacks();
	RadioClearChannels();
	FileManager::EndOfWork();
	Active=false;

	// Statistics
	WriteLog("Server stopped.\n");
	WriteLog("Statistics:\n");
	WriteLog("Traffic:\n"
		"Bytes Send:%u\n"
		"Bytes Recv:%u\n",
		Statistics.BytesSend,
		Statistics.BytesRecv);
	WriteLog("Cycles count:%u\n"
		"Approx cycle period:%u\n"
		"Min cycle period:%u\n"
		"Max cycle period:%u\n"
		"Count of lags (>100ms):%u\n",
		Statistics.LoopCycles,
		Statistics.LoopTime/(Statistics.LoopCycles?Statistics.LoopCycles:1),
		Statistics.LoopMin,
		Statistics.LoopMax,
		Statistics.LagsCount);
}

string FOServer::GetIngamePlayersStatistics()
{
	static string result;
	const char* net_states_str[]={"None","Disconnect","Connect","Game","LoginOk","InitNet","Remove"};
	const char* cond_states_str[]={"None","Life","Knockout","Dead"};
	char str[512];
	char str_loc[256];
	char str_map[256];

	ConnectedClientsLocker.Lock();
	DWORD conn_count=ConnectedClients.size();
	ConnectedClientsLocker.Unlock();

	ClVec players;
	CrMngr.GetCopyPlayers(players);

	sprintf(str,"Players in game: %u\nConnections: %u\n",players.size(),conn_count);
	result=str;
	result+="Name                 Id        Ip              NetState   Cond     X     Y     Location (Id, Pid)             Map (Id, Pid)                  Level\n";
	for(size_t i=0,j=players.size();i<j;i++)
	{
		Client* cl=players[i];
		const char* name=cl->GetName();

		Map* map=MapMngr.GetMap(cl->GetMap());
		Location* loc=map->GetLocation(false);
		sprintf(str_loc,"%s (%u, %u)",map?loc->Proto->Name.c_str():"",map?loc->GetId():0,map?loc->GetPid():0);
		sprintf(str_map,"%s (%u, %u)",map?map->Proto->GetName():"",map?map->GetId():0,map?map->GetPid():0);
		sprintf(str,"%-20s %-9u %-15s %-10s %-8s %-5u %-5u %-30s %-30s %-4d\n",
			cl->GetName(),cl->GetId(),cl->GetIpStr(),net_states_str[InterlockedCompareExchange(&cl->NetState,0,0)],cond_states_str[cl->Data.Cond],
			map?cl->GetHexX():cl->Data.WorldX,map?cl->GetHexY():cl->Data.WorldY,map?str_loc:"Global map",map?str_map:"",cl->Data.Params[ST_LEVEL]);
		result+=str;
	}
	return result;
}

void FOServer::DisconnectClient(Client* cl)
{
	if(cl->IsDisconnected) return;
	cl->IsDisconnected=true;
	if(InterlockedCompareExchange(&cl->WSAOut->Operation,0,0)==WSAOP_FREE)
	{
		cl->Bout.Lock();
		bool is_empty=cl->Bout.IsEmpty();
		cl->Bout.Unlock();
		if(is_empty) cl->Shutdown();
	}

	DWORD id=cl->GetId();
	Client* cl_=(id?CrMngr.GetPlayer(id):NULL);
	if(cl_ && cl_==cl)
	{
		cl->Disconnect(); // Refresh disconnectTick
		EraseSaveClient(cl->GetId());
		AddSaveClient(cl);

		SETFLAG(cl->Flags,FCRIT_DISCONNECT);
		if(cl->GetMap())
		{
			cl->SendA_Action(ACTION_DISCONNECT,0,NULL);
		}
		else if(cl->GroupMove)
		{
			for(CrVecIt it=cl->GroupMove->CritMove.begin(),end=cl->GroupMove->CritMove.end();it!=end;++it)
				(*it)->Send_CritterParam(cl,OTHER_FLAGS,cl->Flags);
		}
	}
}

void FOServer::RemoveClient(Client* cl)
{
	DWORD id=cl->GetId();
	Client* cl_=(id?CrMngr.GetPlayer(id):NULL);
	if(cl_ && cl_==cl)
	{
		cl->EventFinish(cl->Data.ClientToDelete);
		if(Script::PrepareContext(ServerFunctions.CritterFinish,CALL_FUNC_STR,cl->GetInfo()))
		{
			Script::SetArgObject(cl);
			Script::SetArgBool(cl->Data.ClientToDelete);
			Script::RunPrepared();
		}

		if(cl->GetMap())
		{
			Map* map=MapMngr.GetMap(cl->GetMap());
			if(map) MapMngr.EraseCrFromMap(cl,map,cl->GetHexX(),cl->GetHexY());
		}
		else if(cl->GroupMove)
		{
			GlobalMapGroup* group=cl->GroupMove;
			group->EraseCrit(cl);
			if(cl==group->Rule)
			{
				for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it)
				{
					Critter* cr=*it;
					MapMngr.GM_GroupStartMove(cr,true);
				}
			}
			else
			{
				for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it)
				{
					Critter* cr=*it;
					cr->Send_RemoveCritter(cl);
				}

				Item* car=cl->GetItemCar();
				if(car && car->GetId()==group->CarId)
				{
					group->CarId=0;
					MapMngr.GM_GroupSetMove(group,group->MoveX,group->MoveY,0); // Stop others
				}
			}
			cl->GroupMove=NULL;
		}

		// Deferred saving
		EraseSaveClient(cl->GetId());
		if(!cl->Data.ClientToDelete) AddSaveClient(cl);

		CrMngr.EraseCritter(cl);
		cl->Disconnect();
		cl->IsNotValid=true;

		// Full delete
		if(cl->Data.ClientToDelete)
		{
			SaveClientsLocker.Lock();
			ClientData* data=GetClientData(id);
			if(data) data->Clear();
			SaveClientsLocker.Unlock();

			cl->FullClear();
		}

		Job::DeferredRelease(cl);
	}
	else
	{
		cl->Disconnect();
		cl->IsNotValid=true;
	}
}

void FOServer::AddSaveClient(Client* cl)
{
	if(Singleplayer) return;

	SCOPE_SPINLOCK(SaveClientsLocker);

	cl->AddRef();
	SaveClients.push_back(cl);
}

void FOServer::EraseSaveClient(DWORD crid)
{
	SCOPE_SPINLOCK(SaveClientsLocker);

	for(ClVecIt it=SaveClients.begin();it!=SaveClients.end();)
	{
		Client* cl=*it;
		if(cl->GetId()==crid)
		{
			cl->Release();
			it=SaveClients.erase(it);
		}
		else ++it;
	}
}

void FOServer::MainLoop()
{
	if(!Active) return;

	// Add general jobs
	for(int i=0;i<TIME_EVENTS_PER_CYCLE;i++) Job::PushBack(JOB_TIME_EVENTS);
	Job::PushBack(JOB_GARBAGE_ITEMS);
	Job::PushBack(JOB_GARBAGE_CRITTERS);
	Job::PushBack(JOB_GARBAGE_LOCATIONS);
	Job::PushBack(JOB_GARBAGE_SCRIPT);
	Job::PushBack(JOB_GARBAGE_VARS);
	Job::PushBack(JOB_DEFERRED_RELEASE);
	Job::PushBack(JOB_GAME_TIME);
	Job::PushBack(JOB_BANS);
	Job::PushBack(JOB_LOOP_SCRIPT);

	// Start logic threads
	WriteLog("Starting logic threads, count<%u>.\n",LogicThreadCount);
	WriteLog("***   Starting game loop   ***\n");

	LogicThreadHandles=new HANDLE[LogicThreadCount];
	for(DWORD i=0;i<LogicThreadCount;i++)
	{
		Job::PushBack(JOB_THREAD_SYNCHRONIZE);
		LogicThreadHandles[i]=(HANDLE)_beginthreadex(NULL,0,Logic_Work,NULL,CREATE_SUSPENDED,NULL);
		if(LogicThreadSetAffinity) SetThreadAffinityMask(LogicThreadHandles[i],1<<(i%SystemInfo.dwNumberOfProcessors));
	}
	for(DWORD i=0;i<LogicThreadCount;i++) ResumeThread(LogicThreadHandles[i]);

	SyncManager* sync_mngr=SyncManager::GetForCurThread();
	sync_mngr->UnlockAll();
	while(!FOQuit)
	{
		// Synchronize point
		LogicThreadSync.SynchronizePoint();

		// Synchronize single player data
		if(Singleplayer)
		{
			// Check client process
			if(WaitForSingleObject(SingleplayerClientProcess,0)==WAIT_OBJECT_0)
			{
				FOQuit=true;
				break;
			}

			SingleplayerData.Refresh();

			if(SingleplayerData.Pause!=Timer::IsGamePaused())
			{
				SynchronizeLogicThreads();
				Timer::SetGamePause(SingleplayerData.Pause);
				ResynchronizeLogicThreads();
			}
		}

		// World saver
		if(Timer::FastTick()>=SaveWorldNextTick)
		{
			SynchronizeLogicThreads();
			SaveWorld(NULL);
			SaveWorldNextTick=Timer::FastTick()+SaveWorldTime;
			ResynchronizeLogicThreads();
		}

		// Client script
		if(RequestReloadClientScripts)
		{
			SynchronizeLogicThreads();
			ReloadClientScripts();
			RequestReloadClientScripts=false;
			ResynchronizeLogicThreads();
		}

		// Statistics
		Statistics.Uptime=(Timer::FastTick()-Statistics.ServerStartTick)/1000;
		SetEvent(UpdateEvent);

//		sync_mngr->UnlockAll();
		Sleep(100);
	}

	WriteLog("***   Finishing game loop  ***\n");

	// Stop logic threads
	for(DWORD i=0;i<LogicThreadCount;i++) Job::PushBack(JOB_THREAD_FINISH);
	WaitForMultipleObjects(LogicThreadCount,LogicThreadHandles,TRUE,INFINITE);
	for(DWORD i=0;i<LogicThreadCount;i++) CloseHandle(LogicThreadHandles[i]);
	SAFEDELA(LogicThreadHandles);

	// Erase all jobs
	for(int i=0;i<JOB_COUNT;i++) Job::Erase(i);

	// Finish all critters
	CrMap& critters=CrMngr.GetCrittersNoLock();
	for(CrMapIt it=critters.begin(),end=critters.end();it!=end;++it)
	{
		Critter* cr=(*it).second;
		bool to_delete=(cr->IsPlayer() && ((Client*)cr)->Data.ClientToDelete);

		cr->EventFinish(to_delete);
		if(Script::PrepareContext(ServerFunctions.CritterFinish,CALL_FUNC_STR,cr->GetInfo()))
		{
			Script::SetArgObject(cr);
			Script::SetArgBool(to_delete);
			Script::RunPrepared();
		}
		if(to_delete) cr->FullClear();
	}

	// Last process
	ProcessBans();
	ProcessGameTime();
	ItemMngr.ItemGarbager();
	CrMngr.CritterGarbager();
	MapMngr.LocationGarbager();
	Script::CollectGarbage(true);
	Job::SetDeferredReleaseCycle(0xFFFFFFFF);
	Job::ProcessDeferredReleasing();

	// Save
	SaveWorld(NULL);

	// Last unlock
	sync_mngr->UnlockAll();
}

void FOServer::SynchronizeLogicThreads()
{
	SyncManager* sync_mngr=SyncManager::GetForCurThread();
	sync_mngr->Suspend();
	LogicThreadSync.Synchronize(LogicThreadCount);
	sync_mngr->Resume();
}

void FOServer::ResynchronizeLogicThreads()
{
	LogicThreadSync.Resynchronize();
}

unsigned int __stdcall FOServer::Logic_Work(void* data)
{
	// Set thread name
	static int thread_count=0;
	LogSetThreadName(Str::Format("Logic%d",thread_count));
	thread_count++;

	// Init scripts
	if(!Script::InitThread()) return 0;

	// Add sleep job for current thread
	Job::PushBack(Job(JOB_THREAD_LOOP,NULL,true));

	// Get synchronize manager
	SyncManager* sync_mngr=SyncManager::GetForCurThread();

	// Wait next threads initialization
	Sleep(10);

	// Cycle time
	DWORD cycle_tick=Timer::FastTick();

	// Word loop
	while(true)
	{
		sync_mngr->UnlockAll();
		Job job=Job::PopFront();

		if(job.Type==JOB_CLIENT)
		{
			Client* cl=(Client*)job.Data;
			SYNC_LOCK(cl);

			// Check for removing
			if(cl->IsNotValid) continue;

			// Check for removing
			if(InterlockedCompareExchange(&cl->WSAIn->Operation,0,0)==WSAOP_FREE &&
				InterlockedCompareExchange(&cl->WSAOut->Operation,0,0)==WSAOP_FREE)
			{
				cl->Shutdown();
				InterlockedExchange(&cl->WSAIn->Operation,WSAOP_END);
				InterlockedExchange(&cl->WSAOut->Operation,WSAOP_END);

				ConnectedClientsLocker.Lock();
				ClVecIt it=std::find(ConnectedClients.begin(),ConnectedClients.end(),cl);
				if(it!=ConnectedClients.end())
				{
					ConnectedClients.erase(it);
					Statistics.CurOnline--;
				}
				ConnectedClientsLocker.Unlock();

				Job::DeferredRelease(cl);
				continue;
			}

			// Process net
			Process(cl);
			if((Client*)job.Data!=cl) job.Data=cl;

			// Disconnect
			if(cl->IsOffline()) DisconnectClient(cl);
		}
		else if(job.Type==JOB_CRITTER)
		{
			Critter* cr=(Critter*)job.Data;
			SYNC_LOCK(cr);

			// Player specific
			if(cr->CanBeRemoved) RemoveClient((Client*)cr); // Todo: rework, add to garbage collector

			// Check for removing
			if(cr->IsNotValid) continue;

			// Process logic
			ProcessCritter(cr);
		}
		else if(job.Type==JOB_MAP)
		{
			Map* map=(Map*)job.Data;
			SYNC_LOCK(map);

			// Check for removing
			if(map->IsNotValid) continue;

			// Process logic
			map->Process();
		}
		else if(job.Type==JOB_TIME_EVENTS)
		{
			// Time events
			ProcessTimeEvents();
		}
		else if(job.Type==JOB_GARBAGE_ITEMS)
		{
			// Items garbage
			sync_mngr->PushPriority(2);
			ItemMngr.ItemGarbager();
			sync_mngr->PopPriority();
		}
		else if(job.Type==JOB_GARBAGE_CRITTERS)
		{
			// Critters garbage
			sync_mngr->PushPriority(2);
			CrMngr.CritterGarbager();
			sync_mngr->PopPriority();
		}
		else if(job.Type==JOB_GARBAGE_LOCATIONS)
		{
			// Locations and maps garbage
			sync_mngr->PushPriority(2);
			MapMngr.LocationGarbager();
			sync_mngr->PopPriority();
		}
		else if(job.Type==JOB_GARBAGE_SCRIPT)
		{
			// AngelScript garbage
			Script::CollectGarbage(false);
		}
		else if(job.Type==JOB_GARBAGE_VARS)
		{
			// Game vars garbage
			VarsGarbarger(false);
		}
		else if(job.Type==JOB_DEFERRED_RELEASE)
		{
			// Release pointers
			Job::ProcessDeferredReleasing();
		}
		else if(job.Type==JOB_GAME_TIME)
		{
			// Game time
			ProcessGameTime();
		}
		else if(job.Type==JOB_BANS)
		{
			// Bans
			ProcessBans();
		}
		else if(job.Type==JOB_LOOP_SCRIPT)
		{
			// Script game loop
			static DWORD game_loop_tick=1;
			if(game_loop_tick && Timer::FastTick()>=game_loop_tick)
			{
				DWORD wait=3600000; // 1hour
				if(Script::PrepareContext(ServerFunctions.Loop,CALL_FUNC_STR,"Game") && Script::RunPrepared()) wait=Script::GetReturnedDword();
				if(!wait) game_loop_tick=0; // Disable
				else game_loop_tick=Timer::FastTick()+wait;
			}
		}
		else if(job.Type==JOB_THREAD_LOOP)
		{
			// Sleep
			DWORD sleep_time=Timer::FastTick();
			if(ServerGameSleep>=0) Sleep(ServerGameSleep);
			sleep_time=Timer::FastTick()-sleep_time;

			// Thread statistics
			// Manage threads data
			static MutexSpinlock stats_locker;
			static PtrVec stats_ptrs;
			stats_locker.Lock();
			struct StatisticsThread
			{
				DWORD CycleTime;
				DWORD FPS;
				DWORD LoopTime;
				DWORD LoopCycles;
				DWORD LoopMin;
				DWORD LoopMax;
				DWORD LagsCount;
			} static THREAD *stats=NULL;
			if(!stats)
			{
				stats=new StatisticsThread();
				ZeroMemory(stats,sizeof(StatisticsThread));
				stats->LoopMin=MAX_DWORD;
				stats_ptrs.push_back(stats);
			}

			// Fill statistics
			DWORD loop_tick=Timer::FastTick()-cycle_tick-sleep_time;
			stats->LoopTime+=loop_tick;
			stats->LoopCycles++;
			if(loop_tick>stats->LoopMax) stats->LoopMax=loop_tick;
			if(loop_tick<stats->LoopMin) stats->LoopMin=loop_tick;
			stats->CycleTime=loop_tick;

			// Calculate whole threads statistics
			DWORD real_min_cycle=MAX_DWORD; // Calculate real cycle count for deferred releasing
			DWORD cycle_time=0,loop_time=0,loop_cycles=0,loop_min=0,loop_max=0,lags=0;
			for(PtrVecIt it=stats_ptrs.begin(),end=stats_ptrs.end();it!=end;++it)
			{
				StatisticsThread* stats_thread=(StatisticsThread*)*it;
				cycle_time+=stats_thread->CycleTime;
				loop_time+=stats_thread->LoopTime;
				loop_cycles+=stats_thread->LoopCycles;
				loop_min+=stats_thread->LoopMin;
				loop_max+=stats_thread->LoopMax;
				lags+=stats_thread->LagsCount;
				real_min_cycle=min(real_min_cycle,stats->LoopCycles);
			}
			DWORD count=stats_ptrs.size();
			Statistics.CycleTime=cycle_time/count;
			Statistics.LoopTime=loop_time/count;
			Statistics.LoopCycles=loop_cycles/count;
			Statistics.LoopMin=loop_min/count;
			Statistics.LoopMax=loop_max/count;
			Statistics.LagsCount=lags/count;
			stats_locker.Unlock();

			// Set real cycle count for deferred releasing
			Job::SetDeferredReleaseCycle(real_min_cycle);

			// Start time of next cycle
			cycle_tick=Timer::FastTick();
		}
		else if(job.Type==JOB_THREAD_SYNCHRONIZE)
		{
			// Threads synchronization
			LogicThreadSync.SynchronizePoint();
		}
		else if(job.Type==JOB_THREAD_FINISH)
		{
			// Exit from thread
			break;
		}
		else // JOB_NOP
		{
			Sleep(100);
			continue;
		}

		// Add job to back
		size_t job_count=Job::PushBack(job);

		// Calculate fps
		static volatile size_t job_cur=0;
		static volatile size_t fps=0;
		static volatile DWORD job_tick=0;
		if(++job_cur>=job_count)
		{
			job_cur=0;
			fps++;
			if(!job_tick || Timer::FastTick()-job_tick>=1000)
			{
				Statistics.FPS=fps;
				fps=0;
				job_tick=Timer::FastTick();
			}
		}
	}

	sync_mngr->UnlockAll();
	Script::FinisthThread();
	return 0;
}

unsigned int __stdcall FOServer::Net_Listen(HANDLE iocp)
{
	LogSetThreadName("NetListen");

	while(true)
	{
		// Blocked
		SOCKADDR_IN from;
		int addrsize=sizeof(from);
		SOCKET sock=WSAAccept(ListenSock,(sockaddr*)&from,&addrsize,NULL,NULL);
		if(sock==INVALID_SOCKET)
		{
			int error=WSAGetLastError();
			if(error==WSAEINTR || error==WSAENOTSOCK) break; // End of work

			WriteLog(__FUNCTION__" - Listen error<%s>. Continue listening.\n",GetLastSocketError());
			continue;
		}

		ConnectedClientsLocker.Lock();
		DWORD count=ConnectedClients.size();
		ConnectedClientsLocker.Unlock();
		if(count>=MAX_CLIENTS_IN_GAME)
		{
			closesocket(sock);
			continue;
		}

		if(GameOpt.DisableTcpNagle)
		{
			int optval=1;
			if(setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(char*)&optval,sizeof(optval)))
				WriteLog(__FUNCTION__" - Can't set TCP_NODELAY (disable Nagle) to socket, error<%s>.\n",GetLastSocketError());
		}

		Client* cl=new Client();

		// ZLib
		cl->Zstrm.zalloc=zlib_alloc;
		cl->Zstrm.zfree=zlib_free;
		cl->Zstrm.opaque=NULL;
		if(deflateInit(&cl->Zstrm,Z_DEFAULT_COMPRESSION)!=Z_OK)
		{
			WriteLog(__FUNCTION__" - Client Zlib deflateInit fail.\n");
			closesocket(sock);
			delete cl;
			continue;
		}
		cl->ZstrmInit=true;

		// CompletionPort
		if(!CreateIoCompletionPort((HANDLE)sock,IOCompletionPort,0,0))
		{
			WriteLog(__FUNCTION__" - CreateIoCompletionPort fail, error<%u>.\n",GetLastError());
			delete cl;
			continue;
		}

		// Socket
		cl->Sock=sock;
		cl->From=from;

		// First receive queue
		if(WSARecv(cl->Sock,&cl->WSAIn->Buffer,1,NULL,&cl->WSAIn->Flags,&cl->WSAIn->OV,NULL)==SOCKET_ERROR && WSAGetLastError()!=WSA_IO_PENDING)
		{
			WriteLog(__FUNCTION__" - First recv fail, error<%s>.\n",GetLastSocketError());
			closesocket(sock);
			delete cl;
			continue;
		}

		// Add to connected collection
		ConnectedClientsLocker.Lock();
		InterlockedExchange(&cl->NetState,STATE_CONN);
		ConnectedClients.push_back(cl);
		Statistics.CurOnline++;
		ConnectedClientsLocker.Unlock();

		// Add job
		Job::PushBack(JOB_CLIENT,cl);
	}
	return 0;
}

unsigned int __stdcall FOServer::Net_Work(HANDLE iocp)
{
	static int thread_count=0;
	LogSetThreadName(Str::Format("NetWork%d",thread_count));
	thread_count++;

	while(true)
	{
		DWORD bytes;
		DWORD key;
		WSAOVERLAPPED_EX* io;
		DWORD error=ERROR_SUCCESS;
		if(!GetQueuedCompletionStatus(iocp,&bytes,(PULONG_PTR)&key,(LPOVERLAPPED*)&io,INFINITE)) error=GetLastError();
		if(key==1) break; // End of work

		if(error!=ERROR_SUCCESS && error!=ERROR_NETNAME_DELETED && error!=ERROR_CONNECTION_ABORTED && error!=ERROR_OPERATION_ABORTED && error!=ERROR_SEM_TIMEOUT)
		{
			WriteLog(__FUNCTION__" - GetQueuedCompletionStatus fail, error<%u>. Work thread closed!\n",error);
			break;
		}

		io->Locker.Lock();
		Client* cl=(Client*)io->PClient;
		cl->AddRef();

		if(error==ERROR_SUCCESS && bytes)
		{
			switch(InterlockedCompareExchange(&io->Operation,0,0))
			{
			case WSAOP_SEND:
				Statistics.BytesSend+=bytes;
				Net_Output(io);
				break;
			case WSAOP_RECV:
				Statistics.BytesRecv+=bytes;
				io->Bytes=bytes;
				Net_Input(io);
				break;
			default:
				WriteLog(__FUNCTION__" - Unknown operation<%d>.\n",io->Operation);
				break;
			}
		}
		else
		{
			InterlockedExchange(&io->Operation,WSAOP_FREE);
		}

		io->Locker.Unlock();
		cl->Release();
	}
	return 0;
}

void FOServer::Net_Input(WSAOVERLAPPED_EX* io)
{
	Client* cl=(Client*)io->PClient;

	cl->Bin.Lock();
	if(cl->Bin.GetCurPos()+io->Bytes>=GameOpt.FloodSize && !Singleplayer)
	{
		WriteLog(__FUNCTION__" - Flood.\n");
		cl->Disconnect();
		cl->Bin.Reset();
		cl->Bin.Unlock();
		InterlockedExchange(&io->Operation,WSAOP_FREE);
		return;
	}
	cl->Bin.Push(io->Buffer.buf,io->Bytes);
	cl->Bin.Unlock();

	io->Flags=0;
	if(WSARecv(cl->Sock,&io->Buffer,1,NULL,&io->Flags,&io->OV,NULL)==SOCKET_ERROR && WSAGetLastError()!=WSA_IO_PENDING)
	{
		WriteLog(__FUNCTION__" - Recv fail, error<%s>.\n",GetLastSocketError());
		cl->Disconnect();
		InterlockedExchange(&io->Operation,WSAOP_FREE);
	}
}

void FOServer::Net_Output(WSAOVERLAPPED_EX* io)
{
	Client* cl=(Client*)io->PClient;

	cl->Bout.Lock();
	if(cl->Bout.IsEmpty()) // Nothing to send
	{
		cl->Bout.Unlock();
		InterlockedExchange(&io->Operation,WSAOP_FREE);
		if(cl->IsDisconnected) cl->Shutdown();
		return;
	}

	// Compress
	if(!GameOpt.DisableZlibCompression && !cl->DisableZlib)
	{
		DWORD to_compr=cl->Bout.GetEndPos();
		if(to_compr>WSA_BUF_SIZE) to_compr=WSA_BUF_SIZE;

		cl->Zstrm.next_in=(UCHAR*)cl->Bout.GetCurData();
		cl->Zstrm.avail_in=to_compr;
		cl->Zstrm.next_out=(UCHAR*)io->Buffer.buf;
		cl->Zstrm.avail_out=WSA_BUF_SIZE;

 		if(deflate(&cl->Zstrm,Z_SYNC_FLUSH)!=Z_OK)
 		{
			WriteLog(__FUNCTION__" - Deflate fail.\n");
			cl->Disconnect();
			cl->Bout.Reset();
			cl->Bout.Unlock();
			InterlockedExchange(&io->Operation,WSAOP_FREE);
			return;
		}

		DWORD compr=cl->Zstrm.next_out-(UCHAR*)io->Buffer.buf;
		DWORD real=cl->Zstrm.next_in-(UCHAR*)cl->Bout.GetCurData();
		io->Buffer.len=compr;
		cl->Bout.Pop(real);
		Statistics.DataReal+=real;
		Statistics.DataCompressed+=compr;
	}
	// Without compressing
	else
	{
		DWORD len=cl->Bout.GetEndPos();
		if(len>WSA_BUF_SIZE) len=WSA_BUF_SIZE;
		memcpy(io->Buffer.buf,cl->Bout.GetCurData(),len);
		io->Buffer.len=len;
		cl->Bout.Pop(len);
		Statistics.DataReal+=len;
		Statistics.DataCompressed+=len;
	}
	if(cl->Bout.IsEmpty()) cl->Bout.Reset();
	cl->Bout.Unlock();

	// Send
	InterlockedExchange(&io->Operation,WSAOP_SEND);
	if(WSASend(cl->Sock,&io->Buffer,1,NULL,0,(LPOVERLAPPED)io,NULL)==SOCKET_ERROR && WSAGetLastError()!=WSA_IO_PENDING)
	{
		WriteLog(__FUNCTION__" - Send fail, error<%s>.\n",GetLastSocketError());
		cl->Disconnect();
		InterlockedExchange(&io->Operation,WSAOP_FREE);
	}
}

void FOServer::Process(ClientPtr& cl)
{
	if(cl->IsOffline())
	{
		cl->Bin.LockReset();
		return;
	}

	MSGTYPE msg=0;
	if(InterlockedCompareExchange(&cl->NetState,0,0)==STATE_CONN) 
	{
		BIN_BEGIN(cl);
		if(cl->Bin.IsEmpty()) cl->Bin.Reset();
		cl->Bin.Refresh();
		if(cl->Bin.NeedProcess())
		{
			cl->Bin >> msg;

			DWORD tick=Timer::FastTick();
			switch(msg) 
			{
			case 0xFFFFFFFF:
				{
			 		DWORD answer[4]={CrMngr.PlayersInGame(),Statistics.Uptime,0,0};
			 		BOUT_BEGIN(cl);
					cl->Bout.Push((char*)answer,sizeof(answer));
					cl->DisableZlib=true;
					BOUT_END(cl);
			 		cl->Disconnect();
			 	}
				BIN_END(cl);
				break;
			case NETMSG_PING:
				Process_Ping(cl);
				BIN_END(cl);
				break;
			case NETMSG_LOGIN:
				Process_LogIn(cl);
				BIN_END(cl);
				break;
			case NETMSG_CREATE_CLIENT:
				Process_CreateClient(cl);
				BIN_END(cl);
				break;
			case NETMSG_SINGLEPLAYER_SAVE_LOAD:
				Process_SingleplayerSaveLoad(cl);
				BIN_END(cl);
				break;
			default:
				//WriteLog(__FUNCTION__" - Invalid msg<%u> from client<%s> on STATE_CONN. Skip.\n",msg,cl->GetInfo());
				cl->Bin.SkipMsg(msg);
				BIN_END(cl);
				break;
			}
		}
		else
		{
			BIN_END(cl);
		}

		if(cl->NetState==STATE_CONN && cl->ConnectTime && Timer::FastTick()-cl->ConnectTime>PING_CLIENT_LIFE_TIME) // Kick bot
		{
			WriteLog(__FUNCTION__" - Connection timeout, client kicked, maybe bot.\n");
			cl->Disconnect();
		}
	}
	else if(InterlockedCompareExchange(&cl->NetState,0,0)==STATE_LOGINOK)
	{
#define CHECK_BUSY if(cl->IsBusy() && !Singleplayer) {cl->Bin.MoveReadPos(-int(sizeof(msg))); BIN_END(cl); return;}

		BIN_BEGIN(cl);
		if(cl->Bin.IsEmpty()) cl->Bin.Reset();
		cl->Bin.Refresh();
		if(cl->Bin.NeedProcess())
		{
			cl->Bin >> msg;

			DWORD tick=Timer::FastTick();
			switch(msg) 
			{
			case NETMSG_PING:
				Process_Ping(cl);
				BIN_END(cl);
				break;
			case NETMSG_SEND_GIVE_MAP:
				CHECK_BUSY;
				Process_GiveMap(cl);
				BIN_END(cl);
				break;
			case NETMSG_SEND_LOAD_MAP_OK:
				CHECK_BUSY;
				Process_ParseToGame(cl);
				BIN_END(cl);
				break;
			default:
				//if(msg<NETMSG_SEND_MOVE && msg>NETMSG_CRITTER_XY) WriteLog(__FUNCTION__" - Invalid msg<%u> from client<%s> on STATE_LOGINOK. Skip.\n",msg,cl->GetInfo());
				cl->Bin.SkipMsg(msg);
				BIN_END(cl);
				break;
			}
		}
		else
		{
			BIN_END(cl);
		}
	}
	else if(InterlockedCompareExchange(&cl->NetState,0,0)==STATE_GAME)
	{
#define MESSAGES_PER_CYCLE         (5)
#define CHECK_BUSY_AND_LIFE if(!cl->IsLife()) break; if(cl->IsBusy() && !Singleplayer){cl->Bin.MoveReadPos(-int(sizeof(msg))); BIN_END(cl); return;}
#define CHECK_NO_GLOBAL if(!cl->GetMap()) break;
#define CHECK_IS_GLOBAL if(cl->GetMap() || !cl->GroupMove) break;
#define CHECK_AP_MSG BYTE ap; cl->Bin >> ap; if(!Singleplayer){if(!cl->IsTurnBased()){if(ap>cl->GetMaxAp()) break; if((int)ap>cl->GetAp()){cl->Bin.MoveReadPos(-int(sizeof(msg)+sizeof(ap))); BIN_END(cl); return;}}}
#define CHECK_AP(ap) if(!Singleplayer){if(!cl->IsTurnBased()){if((int)(ap)>cl->GetMaxAp()) break; if((int)(ap)>cl->GetAp()){cl->Bin.MoveReadPos(-int(sizeof(msg))); BIN_END(cl); return;}}}
#define CHECK_REAL_AP(ap) if(!Singleplayer){if(!cl->IsTurnBased()){if((int)(ap)>cl->GetMaxAp()*AP_DIVIDER) break; if((int)(ap)>cl->GetRealAp()){cl->Bin.MoveReadPos(-int(sizeof(msg))); BIN_END(cl); return;}}}

		for(int i=0;i<MESSAGES_PER_CYCLE;i++)
		{
			BIN_BEGIN(cl);
			if(cl->Bin.IsEmpty()) cl->Bin.Reset();
			cl->Bin.Refresh();
			if(!cl->Bin.NeedProcess())
			{
				BIN_END(cl);
				break;
			}
			cl->Bin >> msg;

			DWORD tick=Timer::FastTick();
			switch(msg) 
			{
			case NETMSG_PING:
				{
					Process_Ping(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_TEXT:
				{
					Process_Text(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_COMMAND:
				{
					Process_Command(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_LEVELUP:
				{
					Process_LevelUp(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_CRAFT_ASK:
				{
					Process_CraftAsk(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_CRAFT:
				{
					CHECK_BUSY_AND_LIFE;
					Process_Craft(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_DIR:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_NO_GLOBAL;
					Process_Dir(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_MOVE_WALK:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_NO_GLOBAL;
					CHECK_REAL_AP(cl->GetApCostCritterMove(false));
					Process_Move(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_MOVE_RUN:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_NO_GLOBAL;
					CHECK_REAL_AP(cl->GetApCostCritterMove(true));
					Process_Move(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_USE_ITEM:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_AP_MSG;
					Process_UseItem(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_PICK_ITEM:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_NO_GLOBAL;
					CHECK_AP(cl->GetApCostPickItem());
					Process_PickItem(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_PICK_CRITTER:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_NO_GLOBAL;
					CHECK_AP(cl->GetApCostPickCritter());
					Process_PickCritter(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_ITEM_CONT:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_AP(cl->GetApCostMoveItemContainer());
					Process_ContainerItem(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_CHANGE_ITEM:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_AP_MSG;
					Process_ChangeItem(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_RATE_ITEM:
				{
					Process_RateItem(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_SORT_VALUE_ITEM:
				{
					Process_SortValueItem(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_USE_SKILL:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_AP(cl->GetApCostUseSkill());
					Process_UseSkill(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_TALK_NPC:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_NO_GLOBAL;
					Process_Dialog(cl,false);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_SAY_NPC:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_NO_GLOBAL;
					Process_Dialog(cl,true);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_BARTER:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_NO_GLOBAL;
					Process_Barter(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_PLAYERS_BARTER:
				{
					CHECK_BUSY_AND_LIFE;
					Process_PlayersBarter(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_SCREEN_ANSWER:
				{
					Process_ScreenAnswer(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_REFRESH_ME:
				{
					cl->Send_LoadMap(NULL);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_COMBAT:
				{
					CHECK_BUSY_AND_LIFE;
					CHECK_NO_GLOBAL;
					Process_Combat(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_GET_INFO:
				{
					cl->Send_GameInfo(MapMngr.GetMap(cl->GetMap()));
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_GIVE_MAP:
				{
					CHECK_BUSY;
					Process_GiveMap(cl);
					BIN_END(cl);
					continue;
				}
				break;
	//		case NETMSG_SEND_LOAD_MAP_OK:
	//			Process_MapLoaded(acl);
	//			break;
			case NETMSG_SEND_GIVE_GLOBAL_INFO:
				{
					CHECK_IS_GLOBAL;
					Process_GiveGlobalInfo(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_RULE_GLOBAL:
				{
					CHECK_BUSY_AND_LIFE;
					Process_RuleGlobal(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_RADIO:
				{
					CHECK_BUSY_AND_LIFE;
					Process_Radio(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_SET_USER_HOLO_STR:
				{
					CHECK_BUSY_AND_LIFE;
					Process_SetUserHoloStr(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_GET_USER_HOLO_STR:
				{
					CHECK_BUSY_AND_LIFE;
					Process_GetUserHoloStr(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_GET_SCORES:
				{
					CHECK_BUSY;
					Process_GetScores(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_RUN_SERVER_SCRIPT:
				{
					Process_RunServerScript(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SEND_KARMA_VOTING:
				{
					Process_KarmaVoting(cl);
					BIN_END(cl);
					continue;
				}
			case NETMSG_SINGLEPLAYER_SAVE_LOAD:
				{
					Process_SingleplayerSaveLoad(cl);
					BIN_END(cl);
					continue;
				}
			default:
				{
					//if(msg<NETMSG_SEND_MOVE && msg>NETMSG_CRITTER_XY) WriteLog(__FUNCTION__" - Invalid msg<%u> from client<%s> on STATE_GAME. Skip.\n",msg,cl->GetInfo());
					cl->Bin.SkipMsg(msg);
					BIN_END(cl);
					continue;
				}
			}

			//if(msg<NETMSG_SEND_MOVE && msg>NETMSG_CRITTER_XY) WriteLog(__FUNCTION__" - Access denied. on msg<%u> for client<%s>. Skip.\n",msg,cl->GetInfo());
			cl->Bin.SkipMsg(msg);
			BIN_END(cl);
		}
	}
}

void FOServer::Process_Text(Client* cl)
{
	DWORD msg_len=0;
	BYTE how_say=0;
	WORD len=0;
	char str[MAX_NET_TEXT+1];

	cl->Bin >> msg_len;
	cl->Bin >> how_say;
	cl->Bin >> len;
	CHECK_IN_BUFF_ERROR(cl);

	if(!len || len>MAX_NET_TEXT)
	{
		WriteLog(__FUNCTION__" - Buffer zero sized or too large, length<%u>. Disconnect.\n",len);
		cl->Disconnect();
		return;
	}

	cl->Bin.Pop(str,len);
	str[len]='\0';
	CHECK_IN_BUFF_ERROR(cl);

	if(!cl->IsLife() && how_say>=SAY_NORM && how_say<=SAY_RADIO) how_say=SAY_WHISP;

	if(!strcmp(str,cl->LastSay))
	{
		cl->LastSayEqualCount++;
		if(cl->LastSayEqualCount>=10)
		{
			WriteLog(__FUNCTION__" - Flood detected, client<%s>. Disconnect.\n",cl->GetInfo());
			cl->Disconnect();
			return;
		}
		if(cl->LastSayEqualCount>=3) return;
	}
	else
	{
		StringCopy(cl->LastSay,str);
		cl->LastSayEqualCount=0;
	}

	switch(how_say)
	{
	case SAY_NORM:
		{
			if(cl->GetMap())
				cl->SendAA_Text(cl->VisCr,str,SAY_NORM,true);
			else
				cl->SendAA_Text(cl->GroupMove->CritMove,str,SAY_NORM,true);
		}
		break;
	case SAY_SHOUT:
		{
			if(cl->GetMap()) //local
			{
				Map* map=MapMngr.GetMap(cl->GetMap());
				if(!map) return;

				CrVec critters;
				map->GetCritters(critters);

				cl->SendAA_Text(critters,str,SAY_SHOUT,true);
			}
			else //global
			{
				cl->SendAA_Text(cl->GroupMove->CritMove,str,SAY_SHOUT,true);
			}
		}
		break;
	case SAY_EMOTE:
		{
			if(cl->GetMap()) //local
				cl->SendAA_Text(cl->VisCr,str,SAY_EMOTE,true);
			else //global
				cl->SendAA_Text(cl->GroupMove->CritMove,str,SAY_EMOTE,true);
		}
		break;
	case SAY_WHISP:
		{
			if(cl->GetMap()) //local
				cl->SendAA_Text(cl->VisCr,str,SAY_WHISP,true);
			else //global
				//cl->SendA_Text(cl->GroupMove->critMove,pstr,SAY_WHISP,true);
				return;
		}
		break;
	case SAY_SOCIAL: //TODO:
		{
			return;
		}
		break;
	case SAY_RADIO:
		{
			if(cl->GetRadio())
			{
				if(cl->GetMap()) cl->SendAA_Text(cl->VisCr,str,SAY_WHISP,true);
				RadioSendText(cl,cl->GetRadioChannel(),str,true);
			}
			else
			{
				cl->Send_Text(cl,"Error - No radio",SAY_NETMSG);
				return;
			}
		}
		break;
	default:
		cl->Send_Text(cl,"Unknown Say Param.",SAY_NETMSG);
		return;
	}

	// Best score
	SetScore(SCORE_SPEAKER,cl,1);

	// Text listen
	int listen_count=0;
	int licten_func_id[100]; // 100 calls per one message is enough
	CScriptString* licten_str[100];

	TextListenersLocker.Lock();

	WORD parameter=(how_say==SAY_RADIO?cl->GetRadioChannel():cl->GetProtoMap());
	for(int i=0;i<TextListeners.size();i++)
	{
		TextListen& tl=TextListeners[i];
		if(how_say==tl.SayType && parameter==tl.Parameter && !_strnicmp(str,tl.FirstStr,tl.FirstStrLen))
		{
			licten_func_id[listen_count]=tl.FuncId;
			licten_str[listen_count]=new CScriptString(str);
			if(++listen_count>=100) break;
		}
	}

	TextListenersLocker.Unlock();

	for(int i=0;i<listen_count;i++)
	{
		if(Script::PrepareContext(licten_func_id[i],CALL_FUNC_STR,cl->GetInfo()))
		{
			Script::SetArgObject(cl);
			Script::SetArgObject(licten_str[i]);
			Script::RunPrepared();
		}
		licten_str[i]->Release();
	}
}

void FOServer::Process_Command(Client* cl)
{
	DWORD msg_len=0;
	BYTE cmd=0;
	
	cl->Bin >> msg_len;
	cl->Bin >> cmd;

	switch(cmd)
	{
/************************************************************************/
/* EXIT                                                                 */
/************************************************************************/
	case CMD_EXIT:
		{
			if(!FLAG(cl->Access,CMD_EXIT_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			cl->Disconnect();
		}
		break;
/************************************************************************/
/* MYINFO                                                               */
/************************************************************************/
	case CMD_MYINFO:
		{
			if(!FLAG(cl->Access,CMD_MYINFO_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			char istr[1024];
			sprintf(istr,"|0xFF00FF00 Name: |0xFFFF0000 %s"
				"|0xFF00FF00 , Password: |0xFFFF0000 %s"
				"|0xFF00FF00 , Id: |0xFFFF0000 %u"
				"|0xFF00FF00 , Access: ",
				cl->GetName(),cl->Pass,cl->GetId());

			switch(cl->Access)
			{
			case ACCESS_CLIENT: strcat(istr,"|0xFFFF0000 Client|0xFF00FF00 ."); break;
			case ACCESS_TESTER: strcat(istr,"|0xFFFF0000 Tester|0xFF00FF00 ."); break;
			case ACCESS_MODER: strcat(istr,"|0xFFFF0000 Moderator|0xFF00FF00 ."); break;
			case ACCESS_ADMIN: strcat(istr,"|0xFFFF0000 Administrator|0xFF00FF00 ."); break;
			case ACCESS_IMPLEMENTOR: strcat(istr,"|0xFFFFFFFF Implementor|0xFF00FF00 ."); break;
			default: break;
			}

			cl->Send_Text(cl,istr,SAY_NETMSG);
		}
		break;
/************************************************************************/
/* GAMEINFO                                                             */
/************************************************************************/
	case CMD_GAMEINFO:
		{
			int info;
			cl->Bin >> info;

			if(!FLAG(cl->Access,CMD_GAMEINFO_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			string result;
			switch(info)
			{
			case 0: result=Debugger::GetMemoryStatistics(); break;
			case 1: result=GetIngamePlayersStatistics(); break;
			case 2: result=MapMngr.GetLocationsMapsStatistics(); break;
			case 3: result=GetTimeEventsStatistics(); break;
			case 4: result=GetAnyDataStatistics(); break;
			case 5: result=ItemMngr.GetItemsStatistics(); break;
			default: break;
			}

			char str[512];
			ConnectedClientsLocker.Lock();
			sprintf(str,"Connections: %u, Players: %u, Npc: %u. "
				"FOServer machine uptime: %u min., FOServer uptime: %u min.",
				ConnectedClients.size(),CrMngr.PlayersInGame(),CrMngr.NpcInGame(),
				Timer::FastTick()/1000/60,(Timer::FastTick()-Statistics.ServerStartTick)/1000/60);
			ConnectedClientsLocker.Unlock();
			result+=str;

			const char* ptext=result.c_str();
			size_t text_begin=0;
			for(size_t i=0,j=result.length();i<j;i++)
			{
				if(result[i]=='\n')
				{
					size_t len=i-text_begin;
					if(len) cl->Send_TextEx(cl->GetId(),&ptext[text_begin],len,SAY_NETMSG,0,false);
					text_begin=i+1;
				}
			}
		}
		break;
/************************************************************************/
/* CRITID                                                               */
/************************************************************************/
	case CMD_CRITID:
		{
			char name[MAX_NAME+1];
			cl->Bin.Pop(name,MAX_NAME);
			name[MAX_NAME]=0;

			if(!FLAG(cl->Access,CMD_CRITID_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			SaveClientsLocker.Lock();

			ClientData* cd=GetClientData(name);
			if(cd) cl->Send_Text(cl,Str::Format("Client id is %u.",cd->ClientId),SAY_NETMSG);
			else cl->Send_Text(cl,"Client not found.",SAY_NETMSG);

			SaveClientsLocker.Unlock();
		}
		break;
/************************************************************************/
/* MOVECRIT                                                             */
/************************************************************************/
	case CMD_MOVECRIT:
		{
			DWORD crid;
			WORD hex_x;
			WORD hex_y;
			cl->Bin >> crid;
			cl->Bin >> hex_x;
			cl->Bin >> hex_y;
			
			if(!FLAG(cl->Access,CMD_MOVECRIT_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			Critter* cr=CrMngr.GetCritter(crid);
			if(!cr)
			{
				cl->Send_Text(cl,"Critter not found.",SAY_NETMSG);
				break;
			}

			Map* map=MapMngr.GetMap(cr->GetMap());
			if(!map)
			{
				cl->Send_Text(cl,"Critter is on global.",SAY_NETMSG);
				break;
			}

			if(hex_x>=map->GetMaxHexX() || hex_y>=map->GetMaxHexY())
			{
				cl->Send_Text(cl,"Invalid hex position.",SAY_NETMSG);
				break;
			}

			if(MapMngr.Transit(cr,map,hex_x,hex_y,cr->GetDir(),3,true)) cl->Send_Text(cl,"Critter move success.",SAY_NETMSG);
			else cl->Send_Text(cl,"Critter move fail.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/* KILLCRIT                                                             */
/************************************************************************/
	case CMD_KILLCRIT:
		{
			DWORD crid;
			cl->Bin >> crid;

			if(!FLAG(cl->Access,CMD_KILLCRIT_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			Critter* cr=CrMngr.GetCritter(crid);
			if(!cr)
			{
				cl->Send_Text(cl,"Critter not found.",SAY_NETMSG);
				break;
			}

			KillCritter(cr,COND_DEAD_BURN_RUN,NULL);
			cl->Send_Text(cl,"Critter is dead.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/* DISCONNCRIT                                                          */
/************************************************************************/
	case CMD_DISCONCRIT:
		{
			DWORD crid;
			cl->Bin >> crid;

			if(!FLAG(cl->Access,CMD_DISCONCRIT_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(cl->GetId()==crid)
			{
				cl->Send_Text(cl,"To kick yourself type <~exit>",SAY_NETMSG);
				return;
			}

			Critter* cr=CrMngr.GetCritter(crid);
			if(!cr)
			{
				cl->Send_Text(cl,"Critter not found.",SAY_NETMSG);
				return;
			}

			if(!cr->IsPlayer())
			{
				cl->Send_Text(cl,"Finding critter is not a player.",SAY_NETMSG);
				return;
			}

			Client* cl2=(Client*)cr;

			if(InterlockedCompareExchange(&cl2->NetState,0,0)!=STATE_GAME)
			{
				cl->Send_Text(cl,"Player is not in a game.",SAY_NETMSG);
				return;
			}

			cl2->Send_Text(cl2,"You are kicked from game.",SAY_NETMSG);
			cl2->Disconnect();

			cl->Send_Text(cl,"Player disconnected.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/* TOGLOBAL                                                             */
/************************************************************************/
	case CMD_TOGLOBAL:
		{
			if(!FLAG(cl->Access,CMD_TOGLOBAL_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(!cl->IsLife())
			{
				cl->Send_Text(cl,"To global fail, only life none.",SAY_NETMSG);
				break;
			}

			if(MapMngr.TransitToGlobal(cl,0,0,false)==true)
				cl->Send_Text(cl,"To global success.",SAY_NETMSG);
			else
				cl->Send_Text(cl,"To global fail.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/* RESPAWN                                                              */
/************************************************************************/
	case CMD_RESPAWN:
		{
			DWORD crid;
			cl->Bin >> crid;

			if(!FLAG(cl->Access,CMD_RESPAWN_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			Critter* cr=(!crid?cl:CrMngr.GetCritter(crid));
			if(!cr) cl->Send_Text(cl,"Critter not found.",SAY_NETMSG);
			else if(!cr->IsDead()) cl->Send_Text(cl,"Critter does not require respawn.",SAY_NETMSG);
			else
			{
				RespawnCritter(cr);
				cl->Send_Text(cl,"Respawn success.",SAY_NETMSG);
			}
		}
		break;
/************************************************************************/
/* PARAM                                                                */
/************************************************************************/
	case CMD_PARAM:
		{
			WORD param_type;
			WORD param_num;
			int param_val;
			cl->Bin >> param_type;
			cl->Bin >> param_num;
			cl->Bin >> param_val;

			if(!FLAG(cl->Access,CMD_PARAM_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(FLAG(cl->Access,ACCESS_TESTER)) //Free XP
			{
				if(param_type==0 && GameOpt.FreeExp && param_val>0 && cl->Data.Params[ST_LEVEL]<300)
				{
					cl->ChangeParam(ST_EXPERIENCE);
					cl->Data.Params[ST_EXPERIENCE]+=param_val>10000?10000:param_val;
				}

				if(param_type==1 && GameOpt.RegulatePvP) //PvP
				{
					if(!param_val)
					{
						cl->ChangeParam(MODE_NO_PVP);
						cl->Data.Params[MODE_NO_PVP]=0;
						cl->Send_Text(cl,"No PvP off.",SAY_NETMSG);
					}
					else
					{
						cl->ChangeParam(MODE_NO_PVP);
						cl->Data.Params[MODE_NO_PVP]=1;
						cl->Send_Text(cl,"No PvP on.",SAY_NETMSG);
					}
				}
				if(param_type==2)
				{
					ItemMngr.SetItemCritter(cl,PID_HOLODISK,1);
					Item* holo=cl->GetItemByPid(PID_HOLODISK);
					if(holo) holo->HolodiskSetNum(99999);
				}
				return;
			}

			// Implementor commands
			if(param_type>10 && FLAG(cl->Access,ACCESS_ADMIN))
			{
				if(param_type==255)
				{
					// Generate params
					for(int i=ST_STRENGTH;i<=ST_LUCK;i++)
					{
						cl->ChangeParam(i);
						cl->Data.Params[i]=param_num;
					}
					for(int i=SKILL_BEGIN;i<=SKILL_END;i++)
					{
						cl->ChangeParam(i);
						cl->Data.Params[i]=param_val;
					}

					cl->ChangeParam(ST_CURRENT_HP);
					cl->Data.Params[ST_CURRENT_HP]=99999999;
					cl->ChangeParam(ST_MAX_LIFE);
					cl->Data.Params[ST_MAX_LIFE]=99999999;
					cl->ChangeParam(ST_MELEE_DAMAGE);
					cl->Data.Params[ST_MELEE_DAMAGE]=30000;
					cl->ChangeParam(ST_ACTION_POINTS);
					cl->Data.Params[ST_ACTION_POINTS]=300;
					cl->ChangeParam(ST_CURRENT_AP);
					cl->Data.Params[ST_CURRENT_AP]=300*AP_DIVIDER;
					cl->ChangeParam(ST_BONUS_LOOK);
					cl->Data.Params[ST_BONUS_LOOK]=400;
					cl->ChangeParam(PE_SILENT_RUNNING);
					cl->Data.Params[PE_SILENT_RUNNING]=1;
					cl->ChangeParam(SK_SNEAK);
					cl->Data.Params[SK_SNEAK]=1000;

					// Get locations on global
					LocVec locs;
					MapMngr.GetLocations(locs,false);
					for(LocVecIt it=locs.begin(),end=locs.end();it!=end;++it)
					{
						Location* loc=*it;
						cl->AddKnownLoc(loc->GetId());
					}
					cl->GMapFog.Fill(0xFF);
					cl->Send_Text(cl,"Lets Rock my Master.",SAY_NETMSG);
				}
				else if(param_type==244)
				{
					cl->Data.BaseType=param_num;
					cl->Send_Text(cl,"Type changed - please reconnect.",SAY_NETMSG);
					cl->Disconnect();
				}
				else if(param_type==234)
				{
					for(int i=TIMEOUT_BEGIN;i<=TIMEOUT_END;i++) cl->SetTimeout(i,0);
					cl->Send_Text(cl,"Timeouts clear.",SAY_NETMSG);
				}
				else if(param_type==233)
				{
					cl->ChangeParam(ST_CURRENT_HP);
					cl->ChangeParam(DAMAGE_EYE);
					cl->ChangeParam(DAMAGE_RIGHT_ARM);
					cl->ChangeParam(DAMAGE_LEFT_ARM);
					cl->ChangeParam(DAMAGE_RIGHT_LEG);
					cl->ChangeParam(DAMAGE_LEFT_LEG);
					cl->Data.Params[ST_CURRENT_HP]=cl->GetParam(ST_MAX_LIFE);
					cl->Data.Params[DAMAGE_EYE]=0;
					cl->Data.Params[DAMAGE_RIGHT_ARM]=0;
					cl->Data.Params[DAMAGE_LEFT_ARM]=0;
					cl->Data.Params[DAMAGE_RIGHT_LEG]=0;
					cl->Data.Params[DAMAGE_LEFT_LEG]=0;
					cl->Send_Text(cl,"Full heal done.",SAY_NETMSG);
				}
				else if(param_type==222)
				{
					GameOpt.FreeExp=(param_num?true:false);
					cl->Send_Text(cl,"OptFreeExp changed.",SAY_NETMSG);
				}
				else if(param_type==210)
				{
					 if(param_val>0) cl->Data.Params[ST_EXPERIENCE]+=param_val;
				}
				else if(param_type==211)
				{
//					cl->Send_AllQuests();
//					cl->Send_Text(cl,"Local vars clear.",SAY_NETMSG);
				}
				else if(param_type==200)
				{
					PcVec npcs;
					CrMngr.GetCopyNpcs(npcs);
					for(PcVecIt it=npcs.begin(),end=npcs.end();it!=end;++it)
					{
						Npc* npc=*it;
						if(!npc->VisCr.size()) continue;
						CrVecIt it_=npc->VisCr.begin();
						for(int i=0,j=Random(0,npc->VisCr.size()-1);i<j;i++) ++it_;
						npc->SetTarget(-1,*it_,GameOpt.DeadHitPoints,false);
					}
				}
				else if(param_type==100)
				{
					/*DWORD len=0;
					EnterCriticalSection(&CSConnectedClients);
					for(ClVecIt it=ConnectedClients.begin(),end=ConnectedClients.end();it!=end;++it)
					{
						Client* cl_=*it;
						len+=cl->Bin.GetLen();
					}
					LeaveCriticalSection(&CSConnectedClients);
					cl->Send_Text(cl,Str::Format("bin/bout len %u (%u)",len,ConnectedClients.size()),SAY_NETMSG);
					cl->Send_Text(cl,Str::Format("AI planes %d",DymmyVar),SAY_NETMSG);
					cl->Send_Text(cl,Str::Format("Clients %d",DummyVar2),SAY_NETMSG);
					cl->Send_Text(cl,Str::Format("Npc %d",DummyVar3),SAY_NETMSG);*/
				}
				else if(param_type==99)
				{
				}
				else if(param_type==98)
				{
					char* leak=new char[param_val*1000000];
				}
				else if(param_type==97)
				{
					PcVec npcs;
					CrMngr.GetCopyNpcs(npcs);
					for(PcVecIt it=npcs.begin(),end=npcs.end();it!=end;++it)
					{
						Npc* npc=*it;
						ZeroMemory(&npc->Data.EnemyStack,sizeof(npc->Data.EnemyStack));
					}
				}
				else if(param_type==96)
				{
					PcVec npcs;
					CrMngr.GetCopyNpcs(npcs);
					for(PcVecIt it=npcs.begin(),end=npcs.end();it!=end;++it)
					{
						Npc* npc=*it;
						npc->DropPlanes();
					}
				}
				else if(param_type==95)
				{
					PcVec npcs;
					CrMngr.GetCopyNpcs(npcs);
					for(PcVecIt it=npcs.begin(),end=npcs.end();it!=end;++it)
					{
						Npc* npc=*it;
						if(npc->IsDead() && npc->GetParam(ST_REPLICATION_TIME)) npc->SetTimeout(ST_REPLICATION_TIME,0);
					}
				}
				else if(param_type==93)
				{
					SaveWorldNextTick=Timer::FastTick();
				}
				else if(param_type==91)
				{
					Critter* cr=CrMngr.GetCritter(param_val);
					if(cr)
					{
						WriteLog("Cond %u\n",cr->Data.Cond);
						WriteLog("CondExt %u\n",cr->Data.CondExt);
						for(int i=0;i<MAX_PARAMS;i++) WriteLog("%s %u\n",FONames::GetParamName(i),cr->Data.Params[i]);
					}
				}
				else if(param_type==90)
				{
					DWORD count=VarMngr.GetVarsCount();
					double tick=Timer::AccurateTick();
					VarsGarbarger(true);
					count-=VarMngr.GetVarsCount();
					tick=Timer::AccurateTick()-tick;
					cl->Send_Text(cl,Str::Format("Erased %u vars in %g ms.",count,tick),SAY_NETMSG);
				}
				return;
			}

			if(param_type==0)
			{
				if(param_num>=MAX_PARAMS)
				{
					cl->Send_Text(cl,"Wrong param number.",SAY_NETMSG);
					return;
				}

				cl->ChangeParam(param_num);
				cl->Data.Params[param_num]=param_val;
				cl->Send_Text(cl,"Done.",SAY_NETMSG);
			}
			else
			{
				cl->Send_Text(cl,"Wrong param type.",SAY_NETMSG);
			}
		}
		break;
/************************************************************************/
/* GETACCESS                                                            */
/************************************************************************/
	case CMD_GETACCESS:
		{
			char name_access[MAX_NAME+1];
			char pasw_access[128];
			cl->Bin.Pop(name_access,MAX_NAME);
			cl->Bin.Pop(pasw_access,128);
			name_access[MAX_NAME]=0;
			pasw_access[127]=0;

			if(!FLAG(cl->Access,CMD_GETACCESS_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			int wanted_access=-1;
			if(!strcmp(name_access,"client") && std::find(AccessClient.begin(),AccessClient.end(),pasw_access)!=AccessClient.end()) wanted_access=ACCESS_CLIENT;
			else if(!strcmp(name_access,"tester") && std::find(AccessTester.begin(),AccessTester.end(),pasw_access)!=AccessTester.end()) wanted_access=ACCESS_TESTER;
			else if(!strcmp(name_access,"moder") && std::find(AccessModer.begin(),AccessModer.end(),pasw_access)!=AccessModer.end()) wanted_access=ACCESS_MODER;
			else if(!strcmp(name_access,"admin") && std::find(AccessAdmin.begin(),AccessAdmin.end(),pasw_access)!=AccessAdmin.end()) wanted_access=ACCESS_ADMIN;

			bool allow=false;
			if(wanted_access!=-1 && Script::PrepareContext(ServerFunctions.PlayerGetAccess,CALL_FUNC_STR,cl->GetInfo()))
			{
				int script_wanted_access=-1;
				switch(wanted_access)
				{
					case ACCESS_IMPLEMENTOR:
						script_wanted_access=4;
						break;
					case ACCESS_ADMIN:
						script_wanted_access=3;
						break;
					case ACCESS_MODER:
						script_wanted_access=2;
						break;
					case ACCESS_TESTER:
						script_wanted_access=1;
						break;
					case ACCESS_CLIENT:
					default:
						script_wanted_access=0;
						break;
				}

				CScriptString* pass=new CScriptString(pasw_access);
				Script::SetArgObject(cl);
				Script::SetArgDword(script_wanted_access);
				Script::SetArgObject(pass);
				if(Script::RunPrepared() && Script::GetReturnedBool()) allow=true;
				pass->Release();
			}

			if(!allow)
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				break;
			}

			cl->Access=wanted_access;
			cl->Send_Text(cl,"Access changed.",SAY_NETMSG);

			if(cl->Access==ACCESS_IMPLEMENTOR) cl->Send_Text(cl,"Welcome Master.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/* CRASH                                                                */
/************************************************************************/
	case CMD_CRASH:
		{
			if(!FLAG(cl->Access,CMD_CRASH_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			static int count_crash=0;
			if(count_crash<22)
			{
				count_crash++;
				cl->Send_Text(cl,"Not implemented.",SAY_NETMSG);
				return;
			}

			Critter* crash=NULL;
			crash->ItemSlotMain->Accessory=10;
			crash-=10;
			delete crash;
			Item* crash2=(Item*)0x1234;
			crash2->Count_Set(1111);

			cl->Send_Text(cl,"Crash complete.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/* ADDITEM                                                              */
/************************************************************************/
	case CMD_ADDITEM:
		{
			WORD hex_x;
			WORD hex_y;
			WORD pid;
			DWORD count;
			cl->Bin >> hex_x;
			cl->Bin >> hex_y;
			cl->Bin >> pid;
			cl->Bin >> count;
	
			if(!FLAG(cl->Access,CMD_ADDITEM_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(!CreateItemToHexCr(cl,hex_x,hex_y,pid,count))
			{
				cl->Send_Text(cl,"Item(s) not added.",SAY_NETMSG);
			}
			else
			{
				cl->Send_Text(cl,"Item(s) added.",SAY_NETMSG);
			}
		}
		break;
/************************************************************************/
/* ADDITEM_SELF                                                         */
/************************************************************************/
	case CMD_ADDITEM_SELF:
		{
			WORD pid;
			DWORD count;
			cl->Bin >> pid;
			cl->Bin >> count;

			if(!FLAG(cl->Access,CMD_ADDITEM_SELF_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(ItemMngr.AddItemCritter(cl,pid,count)!=NULL)
				cl->Send_Text(cl,"Item(s) added.",SAY_NETMSG);
			else
				cl->Send_Text(cl,"Item(s) added fail.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/* ADDNPC                                                               */
/************************************************************************/
	case CMD_ADDNPC:
		{
			WORD hex_x;
			WORD hex_y;
			BYTE dir;
			WORD pid;
			cl->Bin >> hex_x;
			cl->Bin >> hex_y;
			cl->Bin >> dir;
			cl->Bin >> pid;
			
			if(!FLAG(cl->Access,CMD_ADDNPC_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			Npc* npc=CrMngr.CreateNpc(pid,0,NULL,0,NULL,NULL,MapMngr.GetMap(cl->GetMap()),hex_x,hex_y,dir,true);
			if(!npc)
				cl->Send_Text(cl,"Npc not created.",SAY_NETMSG);
			else
				cl->Send_Text(cl,"Npc created.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/* ADDLOCATION                                                          */
/************************************************************************/
	case CMD_ADDLOCATION:
		{
			WORD wx;
			WORD wy;
			WORD pid;
			cl->Bin >> wx;
			cl->Bin >> wy;
			cl->Bin >> pid;

			if(!FLAG(cl->Access,CMD_ADDLOCATION_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			Location* loc=MapMngr.CreateLocation(pid,wx,wy,0);
			if(!loc)
				cl->Send_Text(cl,"Location not created.",SAY_NETMSG);
			else
				cl->Send_Text(cl,"Location created.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/* RELOADSCRIPTS                                                        */
/************************************************************************/
	case CMD_RELOADSCRIPTS:
		{
			if(!FLAG(cl->Access,CMD_RELOADSCRIPTS_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(Script::ReloadScripts(SCRIPTS_LST,"server",false))
				cl->Send_Text(cl,"Success.",SAY_NETMSG);
			else
				cl->Send_Text(cl,"Fail.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/* LOADSCRIPT                                                           */
/************************************************************************/
	case CMD_LOADSCRIPT:
		{
			char module_name[MAX_SCRIPT_NAME+1];
			cl->Bin.Pop(module_name,MAX_SCRIPT_NAME);
			module_name[MAX_SCRIPT_NAME]=0;

			if(!FLAG(cl->Access,CMD_LOADSCRIPT_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(!strlen(module_name))
			{
				cl->Send_Text(cl,"Fail, name length is zero.",SAY_NETMSG);
				break;
			}

			SynchronizeLogicThreads();

			if(Script::LoadScript(module_name,NULL,true))
			{
				int errors=Script::BindImportedFunctions();
				if(!errors) cl->Send_Text(cl,"Complete.",SAY_NETMSG);
				else cl->Send_Text(cl,Str::Format("Complete, bind errors<%d>.",errors),SAY_NETMSG);
			}
			else
			{
				cl->Send_Text(cl,"Unable to load script.",SAY_NETMSG);
			}

			ResynchronizeLogicThreads();
		}
		break;
/************************************************************************/
/* LOADSCRIPT                                                           */
/************************************************************************/
	case CMD_RELOAD_CLIENT_SCRIPTS:
		{
			if(!FLAG(cl->Access,CMD_RELOAD_CLIENT_SCRIPTS_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			SynchronizeLogicThreads();

			if(ReloadClientScripts()) cl->Send_Text(cl,"Reload client scripts success.",SAY_NETMSG);
			else cl->Send_Text(cl,"Reload client scripts fail.",SAY_NETMSG);

			ResynchronizeLogicThreads();
		}
		break;
/************************************************************************/
/* RUNSCRIPT                                                            */
/************************************************************************/
	case CMD_RUNSCRIPT:
		{
			char module_name[MAX_SCRIPT_NAME+1];
			char func_name[MAX_SCRIPT_NAME+1];
			DWORD param0,param1,param2;
			cl->Bin.Pop(module_name,MAX_SCRIPT_NAME);
			module_name[MAX_SCRIPT_NAME]=0;
			cl->Bin.Pop(func_name,MAX_SCRIPT_NAME);
			func_name[MAX_SCRIPT_NAME]=0;
			cl->Bin >> param0;
			cl->Bin >> param1;
			cl->Bin >> param2;

			if(!FLAG(cl->Access,CMD_RUNSCRIPT_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(!strlen(module_name) || !strlen(func_name))
			{
				cl->Send_Text(cl,"Fail, length is zero.",SAY_NETMSG);
				break;
			}

			int bind_id=Script::Bind(module_name,func_name,"void %s(Critter&,int,int,int)",true);
			if(!bind_id)
			{
				cl->Send_Text(cl,"Fail, function not found.",SAY_NETMSG);
				break;
			}

			if(!Script::PrepareContext(bind_id,CALL_FUNC_STR,cl->GetInfo()))
			{
				cl->Send_Text(cl,"Fail, prepare error.",SAY_NETMSG);
				break;
			}

			Script::SetArgObject(cl);
			Script::SetArgDword(param0);
			Script::SetArgDword(param1);
			Script::SetArgDword(param2);

			if(Script::RunPrepared()) cl->Send_Text(cl,"Run script success.",SAY_NETMSG);
			else cl->Send_Text(cl,"Run script fail.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/*                                                                      */
/************************************************************************/
	case CMD_RELOADLOCATIONS:
		{
			if(!FLAG(cl->Access,CMD_RELOADLOCATIONS_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			SynchronizeLogicThreads();

			if(MapMngr.LoadLocationsProtos()) cl->Send_Text(cl,"Reload proto locations success.",SAY_NETMSG);
			else cl->Send_Text(cl,"Reload proto locations fail.",SAY_NETMSG);

			ResynchronizeLogicThreads();
		}
		break;
/************************************************************************/
/*                                                                      */
/************************************************************************/
	case CMD_LOADLOCATION:
		{
			WORD loc_pid;
			cl->Bin >> loc_pid;

			if(!FLAG(cl->Access,CMD_LOADLOCATION_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(!loc_pid || loc_pid>=MAX_PROTO_LOCATIONS)
			{
				cl->Send_Text(cl,"Invalid proto location pid.",SAY_NETMSG);
				break;
			}

			SynchronizeLogicThreads();

			IniParser city_txt;
			if(city_txt.LoadFile("Locations.cfg",PT_SERVER_MAPS))
			{
				ProtoLocation ploc;
				if(!MapMngr.LoadLocationProto(city_txt,ploc,loc_pid)) cl->Send_Text(cl,"Load proto location fail.",SAY_NETMSG);
				else
				{
					MapMngr.ProtoLoc[loc_pid]=ploc;
					cl->Send_Text(cl,"Load proto location success.",SAY_NETMSG);
				}
			}
			else
			{
				cl->Send_Text(cl,"Locations.cfg not found.",SAY_NETMSG);
				WriteLog("File<%s> not found.\n",Str::Format("%sLocations.cfg",FileMngr.GetPath(PT_SERVER_MAPS)));
			}

			ResynchronizeLogicThreads();
		}
		break;
/************************************************************************/
/*                                                                      */
/************************************************************************/
	case CMD_RELOADMAPS:
		{
			if(!FLAG(cl->Access,CMD_RELOADMAPS_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			SynchronizeLogicThreads();

			if(MapMngr.LoadMapsProtos()) cl->Send_Text(cl,"Reload proto maps success.",SAY_NETMSG);
			else cl->Send_Text(cl,"Reload proto maps fail.",SAY_NETMSG);

			ResynchronizeLogicThreads();
		}
		break;
/************************************************************************/
/*                                                                      */
/************************************************************************/
	case CMD_LOADMAP:
		{
			WORD map_pid;
			cl->Bin >> map_pid;

			if(!FLAG(cl->Access,CMD_LOADMAP_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(!map_pid || map_pid>=MAX_PROTO_MAPS)
			{
				cl->Send_Text(cl,"Invalid proto map pid.",SAY_NETMSG);
				break;
			}

			SynchronizeLogicThreads();

			IniParser maps_txt;
			if(maps_txt.LoadFile("Maps.cfg",PT_SERVER_MAPS))
			{
				ProtoMap pmap;
				if(!MapMngr.LoadMapProto(maps_txt,pmap,map_pid)) cl->Send_Text(cl,"Load proto map fail.",SAY_NETMSG);
				else
				{
					MapMngr.ProtoMaps[map_pid]=pmap;
					cl->Send_Text(cl,"Load proto map success.",SAY_NETMSG);
				}
			}
			else
			{
				cl->Send_Text(cl,"Maps.cfg not found.",SAY_NETMSG);
				WriteLog("File<%s> not found.\n",Str::Format("%sMaps.cfg",FileMngr.GetPath(PT_SERVER_MAPS)));
			}

			ResynchronizeLogicThreads();
		}
		break;
/************************************************************************/
/* REGENMAP                                                             */
/************************************************************************/
	case CMD_REGENMAP:
		{
			if(!FLAG(cl->Access,CMD_REGENMAP_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			// Check global	
			if(!cl->GetMap())
			{
				cl->Send_Text(cl,"Only on local map.",SAY_NETMSG);
				return;
			}

			// Find map	
			Map* map=MapMngr.GetMap(cl->GetMap());
			if(!map)
			{
				cl->Send_Text(cl,"Map not found.",SAY_NETMSG);
				return;
			}

			// Regenerate
			WORD hx=cl->GetHexX();
			WORD hy=cl->GetHexY();
			BYTE dir=cl->GetDir();
			if(RegenerateMap(map))
			{
				// Transit to old position
				MapMngr.Transit(cl,map,hx,hy,dir,5,true);
				cl->Send_Text(cl,"Regenerate map success.",SAY_NETMSG);
			}
			else cl->Send_Text(cl,"Regenerate map fail.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/*                                                                      */
/************************************************************************/
	case CMD_RELOADDIALOGS:
		{
			if(!FLAG(cl->Access,CMD_RELOADDIALOGS_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			SynchronizeLogicThreads();

			DlgMngr.DialogsPacks.clear();
			DlgMngr.DlgPacksNames.clear();
			int errors=DlgMngr.LoadDialogs(DIALOGS_LST_NAME);

			InitLangPacks(LangPacks);
			InitLangPacksDialogs(LangPacks);
			cl->Send_Text(cl,Str::Format("Dialogs reload done, errors<%d>.",errors),SAY_NETMSG);

			ResynchronizeLogicThreads();
		}
		break;
/************************************************************************/
/*                                                                      */
/************************************************************************/
	case CMD_LOADDIALOG:
		{
			char dlg_name[128];
			DWORD dlg_id;
			cl->Bin.Pop(dlg_name,128);
			cl->Bin >> dlg_id;
			dlg_name[127]=0;

			if(!FLAG(cl->Access,CMD_LOADDIALOG_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			SynchronizeLogicThreads();

			if(FileMngr.LoadFile(Str::Format("%s%s",dlg_name,DIALOG_FILE_EXT),PT_SERVER_DIALOGS))
			{
				DialogPack* pack=DlgMngr.ParseDialog(dlg_name,dlg_id,(char*)FileMngr.GetBuf());
				if(pack)
				{
					DlgMngr.EraseDialogs(dlg_id);
					DlgMngr.EraseDialogs(string(dlg_name));

					if(DlgMngr.AddDialogs(pack))
					{
						InitLangPacks(LangPacks);
						InitLangPacksDialogs(LangPacks);
						cl->Send_Text(cl,"Load dialog success.",SAY_NETMSG);
					}
					else
					{
						cl->Send_Text(cl,"Unable to add dialog.",SAY_NETMSG);
						WriteLog("Dialog<%s> add fail.\n",dlg_name);
					}
				}
				else
				{
					cl->Send_Text(cl,"Unable to parse dialog.",SAY_NETMSG);
					WriteLog("Dialog<%s> parse fail.\n",dlg_name);
				}
			}
			else
			{
				cl->Send_Text(cl,"File not found.",SAY_NETMSG);
				WriteLog("File<%s> not found.\n",dlg_name);
			}

			ResynchronizeLogicThreads();
		}
		break;
/************************************************************************/
/*                                                                      */
/************************************************************************/
	case CMD_RELOADTEXTS:
		{
			if(!FLAG(cl->Access,CMD_RELOADTEXTS_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			SynchronizeLogicThreads();

			LangPackVec lang_packs;
			if(InitLangPacks(lang_packs) && InitLangPacksDialogs(lang_packs) && InitCrafts(lang_packs))
			{
				LangPacks=lang_packs;
				cl->Send_Text(cl,"Reload texts success.",SAY_NETMSG);
			}
			else
			{
				lang_packs.clear();
				cl->Send_Text(cl,"Reload texts fail.",SAY_NETMSG);
			}

			ResynchronizeLogicThreads();
		}
		break;
/************************************************************************/
/*                                                                      */
/************************************************************************/
	case CMD_RELOADAI:
		{
			if(!FLAG(cl->Access,CMD_RELOADAI_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			SynchronizeLogicThreads();

			NpcAIMngr ai_mngr;
			if(ai_mngr.Init())
			{
				AIMngr=ai_mngr;
				cl->Send_Text(cl,"Reload ai success.",SAY_NETMSG);
			}
			else
			{
				cl->Send_Text(cl,"Init AI manager fail.",SAY_NETMSG);
			}

			ResynchronizeLogicThreads();
		}
		break;
/************************************************************************/
/* CHECKVAR                                                             */
/************************************************************************/
	case CMD_CHECKVAR:
		{
			WORD tid_var;
			BYTE master_is_npc;
			DWORD master_id;
			DWORD slave_id;
			BYTE full_info;
			cl->Bin >> tid_var;
			cl->Bin >> master_is_npc;
			cl->Bin >> master_id;
			cl->Bin >> slave_id;
			cl->Bin >> full_info;

			if(!FLAG(cl->Access,CMD_CHECKVAR_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(master_is_npc) master_id+=NPC_START_ID-1;
			GameVar* var=VarMngr.GetVar(tid_var,master_id,slave_id,true);
			if(!var)
			{
				cl->Send_Text(cl,"Var not found.",SAY_NETMSG);
				break;
			}

			if(!full_info)
			{
				cl->Send_Text(cl,Str::Format("Value<%d>.",var->GetValue()),SAY_NETMSG);
			}
			else
			{
				TemplateVar* tvar=var->GetTemplateVar();
				cl->Send_Text(cl,Str::Format("Value<%d>, Name<%s>, Start<%d>, Min<%d>, Max<%d>.",
					var->GetValue(),tvar->Name.c_str(),tvar->StartVal,tvar->MinVal,tvar->MaxVal),SAY_NETMSG);
			}
		}
		break;
/************************************************************************/
/* SETVAR                                                               */
/************************************************************************/
	case CMD_SETVAR:
		{
			WORD tid_var;
			BYTE master_is_npc;
			DWORD master_id;
			DWORD slave_id;
			int value;
			cl->Bin >> tid_var;
			cl->Bin >> master_is_npc;
			cl->Bin >> master_id;
			cl->Bin >> slave_id;
			cl->Bin >> value;

			if(!FLAG(cl->Access,CMD_SETVAR_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(master_is_npc) master_id+=NPC_START_ID-1;
			GameVar* var=VarMngr.GetVar(tid_var,master_id,slave_id,true);
			if(!var)
			{
				cl->Send_Text(cl,"Var not found.",SAY_NETMSG);
				break;
			}

			TemplateVar* tvar=var->GetTemplateVar();
			if(value<tvar->MinVal) cl->Send_Text(cl,"Incorrect new value. Less than minimum.",SAY_NETMSG);
			if(value>tvar->MaxVal) cl->Send_Text(cl,"Incorrect new value. Greater than maximum.",SAY_NETMSG);
			else
			{
				*var=value;
				if(var->IsQuest())
				{
					Critter* cr=CrMngr.GetCritter(master_id);
					if(cr) cr->Send_Quest(var->GetQuestStr());
				}
				cl->Send_Text(cl,"Var changed.",SAY_NETMSG);
			}
		}
		break;
/************************************************************************/
/* SETTIME                                                              */
/************************************************************************/
	case CMD_SETTIME:
		{
			int multiplier;
			int year;
			int month;
			int day;
			int hour;
			int minute;
			int second;
			cl->Bin >> multiplier;
			cl->Bin >> year;
			cl->Bin >> month;
			cl->Bin >> day;
			cl->Bin >> hour;
			cl->Bin >> minute;
			cl->Bin >> second;

			if(!FLAG(cl->Access,CMD_SETTIME_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			SynchronizeLogicThreads();

			if(multiplier>=1 && multiplier<=50000) GameOpt.TimeMultiplier=multiplier;
			if(year>=GameOpt.YearStart && year<=GameOpt.YearStart+130) GameOpt.Year=year;
			if(month>=1 && month<=12) GameOpt.Month=month;
			if(day>=1 && day<=31) GameOpt.Day=day;
			if(hour>=0 && hour<=23) GameOpt.Hour=hour;
			if(minute>=0 && minute<=59) GameOpt.Minute=minute;
			if(second>=0 && second<=59) GameOpt.Second=second;
			GameOpt.FullSecond=GetFullSecond(GameOpt.Year,GameOpt.Month,GameOpt.Day,GameOpt.Hour,GameOpt.Minute,GameOpt.Second);
			GameOpt.FullSecondStart=GameOpt.FullSecond;
			GameOpt.GameTimeTick=Timer::GameTick();

			ConnectedClientsLocker.Lock();
			for(ClVecIt it=ConnectedClients.begin(),end=ConnectedClients.end();it!=end;++it)
			{
				Client* cl_=*it;
				if(cl_->IsOnline()) cl_->Send_GameInfo(MapMngr.GetMap(cl_->GetMap(),false));
			}
			ConnectedClientsLocker.Unlock();

			cl->Send_Text(cl,"Time changed.",SAY_NETMSG);

			ResynchronizeLogicThreads();
		}
		break;
/************************************************************************/
/* BAN                                                                  */
/************************************************************************/
	case CMD_BAN:
		{
			char name[MAX_NAME+1];
			char params[MAX_NAME+1];
			DWORD ban_hours;
			char info[128+1];
			cl->Bin.Pop(name,MAX_NAME);
			name[MAX_NAME]=0;
			cl->Bin.Pop(params,MAX_NAME);
			params[MAX_NAME]=0;
			cl->Bin >> ban_hours;
			cl->Bin.Pop(info,128);
			info[MAX_NAME]=0;

			if(!FLAG(cl->Access,CMD_BAN_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			SCOPE_LOCK(BannedLocker);

			if(!_stricmp(params,"list"))
			{
				if(Banned.empty())
				{
					cl->Send_Text(cl,"Ban list empty.",SAY_NETMSG);
					return;
				}

				DWORD index=1;
				for(ClientBannedVecIt it=Banned.begin(),end=Banned.end();it!=end;++it)
				{
					ClientBanned& ban=*it;
					cl->Send_Text(cl,Str::Format("--- %3u ---",index),SAY_NETMSG);
					if(ban.ClientName[0]) cl->Send_Text(cl,Str::Format("User: %s",ban.ClientName),SAY_NETMSG);
					if(ban.ClientIp) cl->Send_Text(cl,Str::Format("UserIp: %u",ban.ClientIp),SAY_NETMSG);
					cl->Send_Text(cl,Str::Format("BeginTime: %u %u %u %u %u",ban.BeginTime.wYear,ban.BeginTime.wMonth,ban.BeginTime.wDay,ban.BeginTime.wHour,ban.BeginTime.wMinute),SAY_NETMSG);
					cl->Send_Text(cl,Str::Format("EndTime: %u %u %u %u %u",ban.EndTime.wYear,ban.EndTime.wMonth,ban.EndTime.wDay,ban.EndTime.wHour,ban.EndTime.wMinute),SAY_NETMSG);
					if(ban.BannedBy[0]) cl->Send_Text(cl,Str::Format("BannedBy: %s",ban.BannedBy),SAY_NETMSG);
					if(ban.BanInfo[0]) cl->Send_Text(cl,Str::Format("Comment: %s",ban.BanInfo),SAY_NETMSG);
					index++;
				}
			}
			else if(!_stricmp(params,"add") || !_stricmp(params,"add+"))
			{
				DWORD name_len=strlen(name);
				if(name_len<MIN_NAME || name_len<GameOpt.MinNameLength || name_len>MAX_NAME || name_len>GameOpt.MaxNameLength || !ban_hours)
				{
					cl->Send_Text(cl,"Invalid arguments.",SAY_NETMSG);
					return;
				}

				Client* cl_banned=CrMngr.GetPlayer(name);
				ClientBanned ban;
				ZeroMemory(&ban,sizeof(ban));
				StringCopy(ban.ClientName,name);
				ban.ClientIp=(cl_banned && strstr(params,"+")?cl_banned->GetIp():0);
				GetLocalTime(&ban.BeginTime);
				ban.EndTime=ban.BeginTime;
				Timer::ContinueTime(ban.EndTime,ban_hours*60*60);
				StringCopy(ban.BannedBy,cl->Name);
				StringCopy(ban.BanInfo,info);

				Banned.push_back(ban);
				SaveBan(ban,false);
				cl->Send_Text(cl,"User banned.",SAY_NETMSG);

				if(cl_banned)
				{
					cl_banned->Send_TextMsg(cl,STR_NET_BAN,SAY_NETMSG,TEXTMSG_GAME);
					cl_banned->Send_TextMsgLex(cl,STR_NET_BAN_REASON,SAY_NETMSG,TEXTMSG_GAME,ban.GetBanLexems());
					cl_banned->Disconnect();
				}
			}
			else if(!_stricmp(params,"delete"))
			{
				if(!strlen(name))
				{
					cl->Send_Text(cl,"Invalid arguments.",SAY_NETMSG);
					return;
				}

				bool resave=false;
				if(!_stricmp(name,"*"))
				{
					int index=(int)ban_hours-1;
					if(index>=0 && index<Banned.size())
					{
						Banned.erase(Banned.begin()+index);
						resave=true;
					}
				}
				else
				{
					for(ClientBannedVecIt it=Banned.begin();it!=Banned.end();)
					{
						ClientBanned& ban=*it;
						if(!_stricmp(ban.ClientName,name))
						{
							SaveBan(ban,true);
							it=Banned.erase(it);
							resave=true;
						}
						else ++it;
					}
				}

				if(resave)
				{
					SaveBans();
					cl->Send_Text(cl,"User unbanned.",SAY_NETMSG);
				}
				else
				{
					cl->Send_Text(cl,"User not found.",SAY_NETMSG);
				}
			}
			else
			{
				cl->Send_Text(cl,"Unknown option.",SAY_NETMSG);
			}
		}
		break;
/************************************************************************/
/* DELETE_ACCOUNT                                                       */
/************************************************************************/
	case CMD_DELETE_ACCOUNT:
		{
			char pass[MAX_NAME+1];
			cl->Bin.Pop(pass,MAX_NAME);
			pass[MAX_NAME]=0;

			if(!FLAG(cl->Access,CMD_DELETE_ACCOUNT_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			if(strcmp(cl->Pass,pass)) cl->Send_Text(cl,"Invalid password.",SAY_NETMSG);
			else
			{
				if(!cl->Data.ClientToDelete)
				{
					cl->Data.ClientToDelete=true;
					cl->Send_Text(cl,"Your account will be deleted after character exit from game.",SAY_NETMSG);
				}
				else
				{
					cl->Data.ClientToDelete=false;
					cl->Send_Text(cl,"Deleting canceled.",SAY_NETMSG);
				}
			}
		}
		break;
/************************************************************************/
/* CHANGE_PASSWORD                                                      */
/************************************************************************/
	case CMD_CHANGE_PASSWORD:
		{
			char pass[MAX_NAME+1];
			char new_pass[MAX_NAME+1];
			cl->Bin.Pop(pass,MAX_NAME);
			pass[MAX_NAME]=0;
			cl->Bin.Pop(new_pass,MAX_NAME);
			new_pass[MAX_NAME]=0;

			if(!FLAG(cl->Access,CMD_CHANGE_PASSWORD_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			DWORD pass_len=strlen(new_pass);
			if(strcmp(cl->Pass,pass)) cl->Send_Text(cl,"Invalid current password.",SAY_NETMSG);
			else if(pass_len<MIN_NAME || pass_len<GameOpt.MinNameLength || pass_len>MAX_NAME || pass_len>GameOpt.MaxNameLength || !CheckUserPass(new_pass)) cl->Send_Text(cl,"Invalid new password.",SAY_NETMSG);
			else
			{
				SCOPE_SPINLOCK(SaveClientsLocker);

				ClientData* data=GetClientData(cl->GetId());
				if(data)
				{
					StringCopy(data->ClientPass,new_pass);
					StringCopy(cl->Pass,new_pass);
					cl->Send_Text(cl,"Password changed.",SAY_NETMSG);
				}
			}
		}
		break;
/************************************************************************/
/* DROP_UID                                                             */
/************************************************************************/
	case CMD_DROP_UID:
		{
			if(!FLAG(cl->Access,CMD_DROP_UID_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			ClientData* data=GetClientData(cl->GetId());
			if(data)
			{
				for(int i=0;i<5;i++) data->UID[i]=0;
				data->UIDEndTick=0;
			}
			cl->Send_Text(cl,"UID dropped, you can relogin on another account without timeout.",SAY_NETMSG);
		}
		break;
/************************************************************************/
/* LOG                                                                  */
/************************************************************************/
	case CMD_LOG:
		{
			char flags[16];
			cl->Bin.Pop(flags,16);

			if(!FLAG(cl->Access,CMD_LOG_ACCESS))
			{
				cl->Send_Text(cl,"Access denied.",SAY_NETMSG);
				return;
			}

			int action=-1;
			if(flags[0]=='-' && flags[1]=='-') action=2; // Detach all
			else if(flags[0]=='-') action=0; // Detach current
			else if(flags[0]=='+') action=1; // Attach current
			else
			{
				cl->Send_Text(cl,"Wrong flags. Valid is '+', '-', '--'.",SAY_NETMSG);
				return;
			}

			SynchronizeLogicThreads();

			LogFinish(LOG_FUNC);
			ClVecIt it=std::find(LogClients.begin(),LogClients.end(),cl);
			if(action==0 && it!=LogClients.end()) // Detach current
			{
				cl->Release();
				LogClients.erase(it);
			}
			else if(action==1 && it==LogClients.end()) // Attach current
			{
				cl->AddRef();
				LogClients.push_back(cl);
			}
			else if(action==2) // Detach all
			{
				for(ClVecIt it_=LogClients.begin();it_<LogClients.end();++it_) (*it_)->Release();
				LogClients.clear();
			}
			if(LogClients.size()) LogToFunc(&FOServer::LogToClients);

			ResynchronizeLogicThreads();
		}
		break;
/************************************************************************/
/*                                                                      */
/************************************************************************/
	default:
		cl->Send_Text(cl,"Unknown Command.",SAY_NETMSG);
		return;
	}
}

void FOServer::SaveGameInfoFile()
{
	// Singleplayer info
	DWORD sp=(SingleplayerSave.Valid?1:0);
	AddWorldSaveData(&sp,sizeof(sp));
	if(SingleplayerSave.Valid)
	{
		// Critter data
		ClientSaveData& csd=SingleplayerSave.CrData;
		AddWorldSaveData(csd.Name,sizeof(csd.Name));
		AddWorldSaveData(&csd.Data,sizeof(csd.Data));
		AddWorldSaveData(&csd.DataExt,sizeof(csd.DataExt));
		DWORD te_count=csd.TimeEvents.size();
		AddWorldSaveData(&te_count,sizeof(te_count));
		if(te_count) AddWorldSaveData(&csd.TimeEvents[0],te_count*sizeof(Critter::CrTimeEvent));

		// Picture data
		DWORD pic_size=SingleplayerSave.PicData.size();
		AddWorldSaveData(&pic_size,sizeof(pic_size));
		if(pic_size) AddWorldSaveData(&SingleplayerSave.PicData[0],pic_size);
	}

	// Time
	AddWorldSaveData(&GameOpt.YearStart,sizeof(GameOpt.YearStart));
	AddWorldSaveData(&GameOpt.Year,sizeof(GameOpt.Year));
	AddWorldSaveData(&GameOpt.Month,sizeof(GameOpt.Month));
	AddWorldSaveData(&GameOpt.Day,sizeof(GameOpt.Day));
	AddWorldSaveData(&GameOpt.Hour,sizeof(GameOpt.Hour));
	AddWorldSaveData(&GameOpt.Minute,sizeof(GameOpt.Minute));
	AddWorldSaveData(&GameOpt.Second,sizeof(GameOpt.Second));
	AddWorldSaveData(&GameOpt.TimeMultiplier,sizeof(GameOpt.TimeMultiplier));

	// Scores
	AddWorldSaveData(&BestScores[0],sizeof(BestScores));
}

bool FOServer::LoadGameInfoFile(FILE* f)
{
	WriteLog("Load game info...");

	// Singleplayer info
	DWORD sp=0;
	if(!fread(&sp,sizeof(sp),1,f)) return false;
	if(sp==1)
	{
		WriteLog("singleplayer...");

		// Critter data
		ClientSaveData& csd=SingleplayerSave.CrData;
		csd.Clear();
		if(!fread(csd.Name,sizeof(csd.Name),1,f)) return false;
		if(!fread(&csd.Data,sizeof(csd.Data),1,f)) return false;
		if(!fread(&csd.DataExt,sizeof(csd.DataExt),1,f)) return false;
		DWORD te_count=csd.TimeEvents.size();
		if(!fread(&te_count,sizeof(te_count),1,f)) return false;
		if(te_count)
		{
			csd.TimeEvents.resize(te_count);
			if(!fread(&csd.TimeEvents[0],te_count*sizeof(Critter::CrTimeEvent),1,f)) return false;
		}

		// Picture data
		DWORD pic_size=0;
		if(!fread(&pic_size,sizeof(pic_size),1,f)) return false;
		if(pic_size)
		{
			SingleplayerSave.PicData.resize(pic_size);
			if(!fread(&SingleplayerSave.PicData[0],pic_size,1,f)) return false;
		}
	}
	SingleplayerSave.Valid=(sp==1);

	// Time
	if(!fread(&GameOpt.YearStart,sizeof(GameOpt.YearStart),1,f)) return false;
	if(!fread(&GameOpt.Year,sizeof(GameOpt.Year),1,f)) return false;
	if(!fread(&GameOpt.Month,sizeof(GameOpt.Month),1,f)) return false;
	if(!fread(&GameOpt.Day,sizeof(GameOpt.Day),1,f)) return false;
	if(!fread(&GameOpt.Hour,sizeof(GameOpt.Hour),1,f)) return false;
	if(!fread(&GameOpt.Minute,sizeof(GameOpt.Minute),1,f)) return false;
	if(!fread(&GameOpt.Second,sizeof(GameOpt.Second),1,f)) return false;
	if(!fread(&GameOpt.TimeMultiplier,sizeof(GameOpt.TimeMultiplier),1,f)) return false;

	// Scores
	if(!fread(&BestScores[0],sizeof(BestScores),1,f)) return false;

	WriteLog("complete.\n");
	return true;
}

void FOServer::InitGameTime()
{
	if(!GameOpt.TimeMultiplier)
	{
		if(Script::PrepareContext(ServerFunctions.GetStartTime,CALL_FUNC_STR,"Game"))
		{
			Script::SetArgAddress(&GameOpt.TimeMultiplier);
			Script::SetArgAddress(&GameOpt.YearStart);
			Script::SetArgAddress(&GameOpt.Month);
			Script::SetArgAddress(&GameOpt.Day);
			Script::SetArgAddress(&GameOpt.Hour);
			Script::SetArgAddress(&GameOpt.Minute);
			Script::RunPrepared();
		}

		GameOpt.YearStart=CLAMP(GameOpt.YearStart,1700,30000);
		GameOpt.Year=GameOpt.YearStart;
		GameOpt.Second=0;
	}

	SYSTEMTIME st={GameOpt.YearStart,1,0,1,0,0,0,0};
	FILETIMELI ft;
	if(!SystemTimeToFileTime(&st,&ft.ft)) WriteLog(__FUNCTION__" - SystemTimeToFileTime error<%u>.\n",GetLastError());
	GameOpt.YearStartFT=ft.ul.QuadPart;

	GameOpt.TimeMultiplier=CLAMP(GameOpt.TimeMultiplier,1,50000);
	GameOpt.Year=CLAMP(GameOpt.Year,GameOpt.YearStart,GameOpt.YearStart+130);
	GameOpt.Month=CLAMP(GameOpt.Month,1,12);
	GameOpt.Day=CLAMP(GameOpt.Day,1,31);
	GameOpt.Hour=CLAMP(GameOpt.Hour,0,23);
	GameOpt.Minute=CLAMP(GameOpt.Minute,0,59);
	GameOpt.Second=CLAMP(GameOpt.Second,0,59);
	GameOpt.FullSecond=GetFullSecond(GameOpt.Year,GameOpt.Month,GameOpt.Day,GameOpt.Hour,GameOpt.Minute,GameOpt.Second);

	GameOpt.FullSecondStart=GameOpt.FullSecond;
	GameOpt.GameTimeTick=Timer::GameTick();
}

bool FOServer::Init()
{
	if(Active) return true;
	Active=0;

	IniParser cfg;
	cfg.LoadFile(SERVER_CONFIG_FILE,PT_SERVER_ROOT);

	WriteLog("***   Starting initialization   ****\n");
	/*WriteLog("FOServer<%u>.\n",sizeof(FOServer));
	WriteLog("MapMngr<%u>.\n",sizeof(CMapMngr));
	WriteLog("ItemMngr<%u>.\n",sizeof(ItemManager));
	WriteLog("VarMngr<%u>.\n",sizeof(CVarMngr));
	WriteLog("MrFixit<%u>.\n",sizeof(CraftManager));
	WriteLog("Client<%u>.\n",sizeof(Client));
	WriteLog("Npc<%u>.\n",sizeof(Npc));
	WriteLog("Location<%u>.\n",sizeof(Location));
	WriteLog("Map<%u>.\n",sizeof(Map));
	WriteLog("Item<%u>.\n",sizeof(Item));
	WriteLog("Item::ItemData<%u>.\n",sizeof(Item::ItemData));
	WriteLog("CScriptString<%u>.\n",sizeof(CScriptString));
	WriteLog("string<%u>.\n",sizeof(string));*/

	// Dump
#ifdef FOSERVER_DUMP
	hDump=CreateFile(".\\dump.dat",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#endif

	// Check the sizes of struct and classes
	STATIC_ASSERT(offsetof(Item,PLexems)==160);
	STATIC_ASSERT(offsetof(Critter::CrTimeEvent,Identifier)==12);
	STATIC_ASSERT(offsetof(Critter,RefCounter)==9792);
	STATIC_ASSERT(offsetof(Client,LanguageMsg)==9860);
	STATIC_ASSERT(offsetof(Npc,Reserved)==9824);
	STATIC_ASSERT(offsetof(GameVar,RefCount)==22);
	STATIC_ASSERT(offsetof(TemplateVar,Flags)==76);
	STATIC_ASSERT(offsetof(AIDataPlane,RefCounter)==88);
	STATIC_ASSERT(offsetof(GlobalMapGroup,EncounterForce)==84);
	STATIC_ASSERT(offsetof(MapObject,RunTime.RefCounter)==244);
	STATIC_ASSERT(offsetof(ProtoMap::MapEntire,Dir)==8);
	STATIC_ASSERT(offsetof(ScenToSend,PicMapHash)==24);
	STATIC_ASSERT(offsetof(ProtoMap,HexFlags)==320);
	STATIC_ASSERT(offsetof(Map,RefCounter)==770);
	STATIC_ASSERT(offsetof(ProtoLocation,GeckEnabled)==92);
	STATIC_ASSERT(offsetof(Location,RefCounter)==286);

	STATIC_ASSERT(sizeof(DWORD)==4);
	STATIC_ASSERT(sizeof(WORD)==2);
	STATIC_ASSERT(sizeof(BYTE)==1);
	STATIC_ASSERT(sizeof(int)==4);
	STATIC_ASSERT(sizeof(short)==2);
	STATIC_ASSERT(sizeof(char)==1);
	STATIC_ASSERT(sizeof(bool)==1);
	STATIC_ASSERT(sizeof(string)==28);
	STATIC_ASSERT(sizeof(IntVec)==16);
	STATIC_ASSERT(sizeof(IntMap)==12);
	STATIC_ASSERT(sizeof(IntSet)==12);
	STATIC_ASSERT(sizeof(IntPair)==8);
	STATIC_ASSERT(sizeof(Item::ItemData)==92);
	STATIC_ASSERT(sizeof(MapObject)==MAP_OBJECT_SIZE+sizeof(MapObject::_RunTime));
	STATIC_ASSERT(sizeof(ScenToSend)==32);
	STATIC_ASSERT(sizeof(NpcBagItem)==16);
	STATIC_ASSERT(sizeof(CritData)==7404);
	STATIC_ASSERT(sizeof(CritDataExt)==6944);
	STATIC_ASSERT(sizeof(GameVar)==28);
	STATIC_ASSERT(sizeof(ProtoItem)==168);

	// Critters parameters
	Critter::SendDataCallback=&Net_Output;
	Critter::ParamsSendMsgLen=sizeof(Critter::ParamsSendCount);
	Critter::ParamsSendCount=0;
	ZeroMemory(Critter::ParamsSendEnabled,sizeof(Critter::ParamsSendEnabled));
	ZeroMemory(Critter::ParamsChangeScript,sizeof(Critter::ParamsChangeScript));
	ZeroMemory(Critter::ParamsGetScript,sizeof(Critter::ParamsGetScript));
	ZeroMemory(Critter::SlotDataSendEnabled,sizeof(Critter::SlotDataSendEnabled));

	// Accesses
	if(cfg.IsLoaded())
	{
		char buf[2048];
		AccessClient.clear();
		AccessTester.clear();
		AccessModer.clear();
		AccessAdmin.clear();
		if(cfg.GetStr("Access_client","",buf)) Str::ParseLine(buf,' ',AccessClient,Str::ParseLineDummy);
		if(cfg.GetStr("Access_tester","",buf)) Str::ParseLine(buf,' ',AccessTester,Str::ParseLineDummy);
		if(cfg.GetStr("Access_moder","",buf)) Str::ParseLine(buf,' ',AccessModer,Str::ParseLineDummy);
		if(cfg.GetStr("Access_admin","",buf)) Str::ParseLine(buf,' ',AccessAdmin,Str::ParseLineDummy);
	}

	// System info
	GetSystemInfo(&SystemInfo);

	// Generic
	ConnectedClients.clear();
	SaveClients.clear();
	RegIp.clear();
	LastHoloId=USER_HOLO_START_NUM;
	TimeEventsLastNum=0;
	VarsGarbageLastTick=Timer::FastTick();

	LogicThreadSetAffinity=cfg.GetInt("LogicThreadSetAffinity",0)!=0;
	LogicThreadCount=cfg.GetInt("LogicThreadCount",0);
	if(!LogicThreadCount) LogicThreadCount=SystemInfo.dwNumberOfProcessors;
	if(LogicThreadCount==1) Script::SetConcurrentExecution(false);

	if(!Singleplayer)
	{
		// Reserve memory
		ConnectedClients.reserve(MAX_CLIENTS_IN_GAME);
		SaveClients.reserve(MAX_CLIENTS_IN_GAME);
	}

	FileManager::SetDataPath(".\\"); // File manager
	CreateDirectory(FileManager::GetFullPath("",PT_SERVER_BANS),NULL);
	CreateDirectory(FileManager::GetFullPath("",PT_SERVER_CLIENTS),NULL);
	CreateDirectory(FileManager::GetFullPath("",PT_SERVER_BANS),NULL);

	FONames::GenerateFoNames(PT_SERVER_DATA); // Generate name of defines
	if(!InitScriptSystem()) goto label_Error; // Script system
	if(!InitLangPacks(LangPacks)) goto label_Error; // Language packs
	if(!ReloadClientScripts()) goto label_Error; // Client scripts, after language packs initialization
	if(!Singleplayer && !LoadClientsData()) goto label_Error;
	if(!Singleplayer) LoadBans();

	// Managers
	if(!AIMngr.Init()) goto label_Error; // NpcAi manager
	if(!ItemMngr.Init()) goto label_Error; // Item manager
	if(!CrMngr.Init()) goto label_Error; // Critter manager
	if(!MapMngr.Init()) goto label_Error; // Map manager
	if(!VarMngr.Init(FileManager::GetFullPath("",PT_SERVER_SCRIPTS))) goto label_Error; // Var Manager (only before dialog manager!)
	if(!DlgMngr.LoadDialogs(DIALOGS_LST_NAME)) goto label_Error; // Dialog manager
	if(!InitLangPacksDialogs(LangPacks)) goto label_Error; // Create FONPC.MSG, FODLG.MSG, need call after InitLangPacks and DlgMngr.LoadDialogs
	if(!InitCrafts(LangPacks)) goto label_Error; // MrFixit
	if(!InitLangCrTypes(LangPacks)) goto label_Error; // Critter types
	if(!MapMngr.RefreshGmMask("wm.msk")) goto label_Error; // Load gm mask

	// Prototypes
	if(!ItemMngr.LoadProtos()) goto label_Error; // Proto items
	if(!CrMngr.LoadProtos()) goto label_Error; // Proto critters
	if(!MapMngr.LoadMapsProtos()) goto label_Error; // Proto maps
	if(!MapMngr.LoadLocationsProtos()) goto label_Error; // Proto locations
	if(!ItemMngr.CheckProtoFunctions()) goto label_Error; // Check valid of proto functions

	// World loading
	if(!Singleplayer)
	{
		// Copy of data
		if(!LoadWorld(NULL)) goto label_Error;

		// Try generate world if not exist
		if(!MapMngr.GetLocationsCount() && !NewWorld()) goto label_Error;

		// Clear unused variables
		VarsGarbarger(true);
	}

	// End of initialization
	Statistics.BytesSend=0;
	Statistics.BytesRecv=0;
	Statistics.DataReal=1;
	Statistics.DataCompressed=1;
	Statistics.ServerStartTick=Timer::FastTick();

	// Net
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2),&wsa))
	{
		WriteLog("WSAStartup error.");
		goto label_Error;
	}

	ListenSock=WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,0,WSA_FLAG_OVERLAPPED);

	WORD port;
	if(!Singleplayer)
	{
		port=cfg.GetInt("Port",4000);
		WriteLog("Starting server on port<%u>.\n",port);
	}
	else
	{
		port=0;
		WriteLog("Starting server on free port.\n",port);
	}

	SOCKADDR_IN sin;
	sin.sin_family=AF_INET;
	sin.sin_port=htons(port);
	sin.sin_addr.s_addr=INADDR_ANY;

	if(bind(ListenSock,(sockaddr*)&sin,sizeof(sin))==SOCKET_ERROR)
	{
		WriteLog("Bind error.\n");
		goto label_Error;
	}

	if(Singleplayer)
	{
		int namelen=sizeof(sin);
		if(getsockname(ListenSock,(sockaddr*)&sin,&namelen))
		{
			WriteLog("Getsockname error.\n");
			goto label_Error;
		}
		WriteLog("Taked port<%u>.\n",sin.sin_port);
	}

	if(listen(ListenSock,SOMAXCONN)==SOCKET_ERROR)
	{
		WriteLog("Listen error!");
		goto label_Error;
	}

	WorkThreadCount=cfg.GetInt("NetWorkThread",0);
	if(!WorkThreadCount) WorkThreadCount=SystemInfo.dwNumberOfProcessors;

	IOCompletionPort=CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,NULL,WorkThreadCount);
	if(!IOCompletionPort)
	{
		WriteLog("Can't create IO Completion Port, error<%u>.\n",GetLastError());
		goto label_Error;
	}

	IOThreadHandles=new HANDLE[WorkThreadCount+1];
	WriteLog("Starting net listen thread.\n");
	IOThreadHandles[0]=(HANDLE)_beginthreadex(NULL,0,Net_Listen,(void*)IOCompletionPort,0,NULL);
	WriteLog("Starting net work threads, count<%u>.\n",WorkThreadCount);
	for(DWORD i=0;i<WorkThreadCount;i++) IOThreadHandles[i+1]=(HANDLE)_beginthreadex(NULL,0,Net_Work,(void*)IOCompletionPort,0,NULL);

	// Start script
	if(!Script::PrepareContext(ServerFunctions.Start,CALL_FUNC_STR,"Game") || !Script::RunPrepared() || !Script::GetReturnedBool())
	{
		WriteLog(__FUNCTION__" - Start script fail.\n");
		goto label_Error;
	}

	// Process command line definitions
	const char* cmd_line=GetCommandLine();
	asIScriptEngine* engine=Script::GetEngine();
	for(int i=0,j=engine->GetGlobalPropertyCount();i<j;i++)
	{
		const char* name;
		int type_id;
		bool is_const;
		const char* config_group;
		void* pointer;
		if(engine->GetGlobalPropertyByIndex(i,&name,&type_id,&is_const,&config_group,&pointer)>=0)
		{
			const char* cmd_name=strstr(cmd_line,name);
			if(cmd_name)
			{
				const char* type_decl=engine->GetTypeDeclaration(type_id);
				if(!strcmp(type_decl,"bool"))
				{
					*(bool*)pointer=atoi(cmd_name+strlen(name)+1)!=0;
					WriteLog("Global var<%s> changed to<%s>.\n",name,*(bool*)pointer?"true":"false");
				}
				else if(!strcmp(type_decl,"string"))
				{
					*(string*)pointer=cmd_name+strlen(name)+1;
					WriteLog("Global var<%s> changed to<%s>.\n",name,(*(string*)pointer).c_str());
				}
				else
				{
					*(int*)pointer=atoi(cmd_name+strlen(name)+1);
					WriteLog("Global var<%s> changed to<%d>.\n",name,*(int*)pointer);
				}
			}
		}
	}

	ScriptSystemUpdate();

	// World saver
	if(Singleplayer) WorldSaveManager=0; // Disable deferred saving
	if(WorldSaveManager)
	{
		DumpBeginEvent=CreateEvent(NULL,FALSE,FALSE,NULL);
		DumpEndEvent=CreateEvent(NULL,FALSE,TRUE,NULL);
		DumpThreadHandle=(HANDLE)_beginthreadex(NULL,0,Dump_Work,NULL,0,NULL);
	}
	SaveWorldTime=cfg.GetInt("WorldSaveTime",60)*60*1000;
	SaveWorldNextTick=Timer::FastTick()+SaveWorldTime;

	Active=true;

	// Inform client about end of initialization
	if(Singleplayer)
	{
		if(!SingleplayerData.Lock()) goto label_Error;
		SingleplayerData.NetPort=sin.sin_port;
		SingleplayerData.Unlock();
	}

	return true;

label_Error:
	WriteLog("Initialization fail!\n");
	if(ListenSock!=INVALID_SOCKET) closesocket(ListenSock);
	return false;
}

bool FOServer::InitCrafts(LangPackVec& lang_packs)
{
	WriteLog("FixBoy load crafts...\n");
	MrFixit.Finish();

	LanguagePack* main_lang;
	for(LangPackVecIt it=lang_packs.begin(),end=lang_packs.end();it!=end;++it)
	{
		LanguagePack& lang=*it;

		if(it==lang_packs.begin())
		{
			if(!MrFixit.LoadCrafts(lang.Msg[TEXTMSG_CRAFT]))
			{
				WriteLog(__FUNCTION__" - Unable to load crafts from<%s>.\n",lang.NameStr);
				return false;
			}
			main_lang=&lang;
			continue;
		}

		CraftManager mr_fixit;
		if(!mr_fixit.LoadCrafts(lang.Msg[TEXTMSG_CRAFT]))
		{
			WriteLog(__FUNCTION__" - Unable to load crafts from<%s>.\n",lang.NameStr);
			return false;
		}

		if(!(MrFixit==mr_fixit))
		{
			WriteLog(__FUNCTION__" - Compare crafts fail. <%s>with<%s>.\n",main_lang->NameStr,lang.NameStr);
			return false;
		}
	}
	WriteLog("FixBoy load crafts complete.\n");
	return true;
}

bool FOServer::InitLangPacks(LangPackVec& lang_packs)
{
	WriteLog("Loading language packs...\n");

	IniParser cfg;
	cfg.LoadFile(SERVER_CONFIG_FILE,PT_SERVER_ROOT);
	DWORD cur_lang=0;

	while(true)
	{
		char cur_str_lang[256];
		char lang_name[256];
		sprintf(cur_str_lang,"Language_%u",cur_lang);

		if(!cfg.GetStr(cur_str_lang,"",lang_name)) break;

		if(strlen(lang_name)!=4)
		{
			WriteLog("Language name not equal to four letters.\n");
			return false;
		}

		DWORD pack_id=*(DWORD*)&lang_name;
		if(std::find(lang_packs.begin(),lang_packs.end(),pack_id)!=lang_packs.end())
		{
			WriteLog("Language pack<%u> is already initialized.\n",cur_lang);
			return false;
		}

		LanguagePack lang;
		if(!lang.Init(lang_name,PT_SERVER_TEXTS))
		{
			WriteLog("Unable to init Language pack<%u>.\n",cur_lang);
			return false;
		}

		lang_packs.push_back(lang);
		cur_lang++;
	}

	WriteLog("Loading language packs complete, loaded<%u> packs.\n",cur_lang);
	return true;
}

bool FOServer::InitLangPacksDialogs(LangPackVec& lang_packs)
{
	// Parse dialog texts with one seed to prevent MSG numbers truncation
	Randomizer def_rnd=DefaultRandomizer;
	DefaultRandomizer.Generate(666666);

	for(DialogPackMapIt it=DlgMngr.DialogsPacks.begin(),end=DlgMngr.DialogsPacks.end();it!=end;++it)
	{
		DialogPack* pack=(*it).second;
		for(int i=0,j=pack->TextsLang.size();i<j;i++)
		{
			for(LangPackVecIt it_=lang_packs.begin(),end_=lang_packs.end();it_!=end_;++it_)
			{
				LanguagePack& lang=*it_;
				if(pack->TextsLang[i]!=lang.NameStr) continue;

				FOMsg* msg=pack->Texts[i];
				FOMsg* msg_dlg=&lang.Msg[TEXTMSG_DLG];

				// Npc texts
				for(int n=100;n<300;n+=10)
				{
					for(int l=0,k=msg->Count(n);l<k;l++)
						msg_dlg->AddStr(pack->PackId*1000+n,msg->GetStr(n,l));
				}

				// Dialogs text
				for(int i_=0;i_<pack->Dialogs.size();i_++)
				{
					Dialog& dlg=pack->Dialogs[i_];
					if(dlg.TextId<100000000) dlg.TextId=msg_dlg->AddStr(msg->GetStr((i_+1)*1000));
					else msg_dlg->AddStr(dlg.TextId,msg->GetStr((i_+1)*1000));
					for(int j_=0;j_<dlg.Answers.size();j_++)
					{
						DialogAnswer& answ=dlg.Answers[j_];
						if(answ.TextId<100000000) answ.TextId=msg_dlg->AddStr(msg->GetStr((i_+1)*1000+(j_+1)*10));
						else msg_dlg->AddStr(answ.TextId,msg->GetStr((i_+1)*1000+(j_+1)*10));
					}
				}

				// Any texts
				DwordStrMulMap& data=msg->GetData();
				DwordStrMulMapIt it__=data.upper_bound(99999999);
				for(;it__!=data.end();++it__) msg_dlg->AddStr(1000000000+pack->PackId*100000+((*it__).first-100000000),(*it__).second);
			}
		}
	}

	for(LangPackVecIt it=lang_packs.begin(),end=lang_packs.end();it!=end;++it)
	{
		LanguagePack& lang=*it;
		lang.Msg[TEXTMSG_DLG].CalculateHash();
		//lang.Msg[TEXTMSG_DLG].SaveMsgFile(Str::Format("%s.txt",lang.NameStr),PT_SERVER_ROOT);
	}

	// Restore default randomizer
	DefaultRandomizer=def_rnd;
	return true;
}

void FOServer::FinishLangPacks()
{
	WriteLog("Finish lang packs...\n");
	LangPacks.clear();
	WriteLog("Finish lang packs complete.\n");
}

bool FOServer::InitLangCrTypes(LangPackVec& lang_packs)
{
	FOMsg msg_crtypes;
	if(!CritType::InitFromFile(&msg_crtypes)) return false;

	for(LangPackVecIt it=lang_packs.begin(),end=lang_packs.end();it!=end;++it)
	{
		LanguagePack& lang=*it;
		for(int i=0;i<MAX_CRIT_TYPES;i++) lang.Msg[TEXTMSG_INTERNAL].EraseStr(STR_INTERNAL_CRTYPE(i));
		lang.Msg[TEXTMSG_INTERNAL]+=msg_crtypes;
	}

	return true;
}

#pragma MESSAGE("Clients logging may be not thread safe.")
void FOServer::LogToClients(char* str)
{
	WORD str_len=strlen(str);
	if(str_len && str[str_len-1]=='\n') str_len--;
	if(str_len)
	{
		for(ClVecIt it=LogClients.begin();it<LogClients.end();)
		{
			Client* cl=*it;
			if(cl->IsOnline())
			{
				cl->Send_TextEx(0,str,str_len,SAY_NETMSG,10,false);
				++it;
			}
			else
			{
				cl->Release();
				it=LogClients.erase(it);
			}
		}
		if(LogClients.empty()) LogFinish(LOG_FUNC);
	}
}

DWORD FOServer::GetBanTime(ClientBanned& ban)
{
	SYSTEMTIME time;
	GetLocalTime(&time);
	int diff=Timer::GetTimeDifference(ban.EndTime,time)/60+1;
	return diff>0?diff:1;
}

void FOServer::ProcessBans()
{
	SCOPE_LOCK(BannedLocker);

	bool resave=false;
	SYSTEMTIME time;
	GetLocalTime(&time);
	for(ClientBannedVecIt it=Banned.begin();it!=Banned.end();)
	{
		SYSTEMTIME& ban_time=(*it).EndTime;
		if(time.wYear>=ban_time.wYear && time.wMonth>=ban_time.wMonth && time.wDay>=ban_time.wDay && time.wHour>=ban_time.wHour && time.wMinute>=ban_time.wMinute)
		{
			SaveBan(*it,true);
			it=Banned.erase(it);
			resave=true;
		}
		else ++it;
	}
	if(resave) SaveBans();
}

void FOServer::SaveBan(ClientBanned& ban, bool expired)
{
	SCOPE_LOCK(BannedLocker);

	FileManager fm;
	const char* fname=(expired?BANS_FNAME_EXPIRED:BANS_FNAME_ACTIVE);
	if(!fm.LoadFile(fname,PT_SERVER_BANS))
	{
		WriteLog(__FUNCTION__" - Can't open file<%s>.\n",FileManager::GetFullPath(fname,PT_SERVER_BANS));
		return;
	}
	fm.SwitchToWrite();

	fm.SetStr("[Ban]\n");
	if(ban.ClientName[0]) fm.SetStr("User=%s\n",ban.ClientName);
	if(ban.ClientIp) fm.SetStr("UserIp=%u\n",ban.ClientIp);
	fm.SetStr("BeginTime=%u %u %u %u %u\n",ban.BeginTime.wYear,ban.BeginTime.wMonth,ban.BeginTime.wDay,ban.BeginTime.wHour,ban.BeginTime.wMinute);
	fm.SetStr("EndTime=%u %u %u %u %u\n",ban.EndTime.wYear,ban.EndTime.wMonth,ban.EndTime.wDay,ban.EndTime.wHour,ban.EndTime.wMinute);
	if(ban.BannedBy[0]) fm.SetStr("BannedBy=%s\n",ban.BannedBy);
	if(ban.BanInfo[0]) fm.SetStr("Comment=%s\n",ban.BanInfo);
	fm.SetStr("\n");

	if(!fm.SaveOutBufToFile(fname,PT_SERVER_BANS))
		WriteLog(__FUNCTION__" - Unable to save file<%s>.\n",FileManager::GetFullPath(fname,PT_SERVER_BANS));
}

void FOServer::SaveBans()
{
	SCOPE_LOCK(BannedLocker);

	FileManager fm;
	for(ClientBannedVecIt it=Banned.begin(),end=Banned.end();it!=end;++it)
	{
		ClientBanned& ban=*it;
		fm.SetStr("[Ban]\n");
		if(ban.ClientName[0]) fm.SetStr("User=%s\n",ban.ClientName);
		if(ban.ClientIp) fm.SetStr("UserIp=%u\n",ban.ClientIp);
		fm.SetStr("BeginTime=%u %u %u %u %u\n",ban.BeginTime.wYear,ban.BeginTime.wMonth,ban.BeginTime.wDay,ban.BeginTime.wHour,ban.BeginTime.wMinute);
		fm.SetStr("EndTime=%u %u %u %u %u\n",ban.EndTime.wYear,ban.EndTime.wMonth,ban.EndTime.wDay,ban.EndTime.wHour,ban.EndTime.wMinute);
		if(ban.BannedBy[0]) fm.SetStr("BannedBy=%s\n",ban.BannedBy);
		if(ban.BanInfo[0]) fm.SetStr("Comment=%s\n",ban.BanInfo);
		fm.SetStr("\n");
	}

	if(!fm.SaveOutBufToFile(BANS_FNAME_ACTIVE,PT_SERVER_BANS))
		WriteLog(__FUNCTION__" - Unable to save file<%s>.\n",FileManager::GetFullPath(BANS_FNAME_ACTIVE,PT_SERVER_BANS));
}

void FOServer::LoadBans()
{
	SCOPE_LOCK(BannedLocker);

	Banned.clear();
	Banned.reserve(1000);
	IniParser bans_txt;
	if(!bans_txt.LoadFile(BANS_FNAME_ACTIVE,PT_SERVER_BANS))
	{
		HANDLE h=CreateFile(FileManager::GetFullPath(BANS_FNAME_ACTIVE,PT_SERVER_BANS),GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
		if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
		return;
	}

	char buf[MAX_FOTEXT];
	while(bans_txt.GotoNextApp("Ban"))
	{
		ClientBanned ban;
		ZeroMemory(&ban,sizeof(ban));
		SYSTEMTIME time;
		ZeroMemory(&time,sizeof(time));
		if(bans_txt.GetStr("Ban","User","",buf)) StringCopy(ban.ClientName,buf);
		ban.ClientIp=bans_txt.GetInt("Ban","UserIp",0);
		if(bans_txt.GetStr("Ban","BeginTime","",buf) && sscanf(buf,"%u%u%u%u%u",&time.wYear,&time.wMonth,&time.wDay,&time.wHour,&time.wMinute)) ban.BeginTime=time;
		if(bans_txt.GetStr("Ban","EndTime","",buf) && sscanf(buf,"%u%u%u%u%u",&time.wYear,&time.wMonth,&time.wDay,&time.wHour,&time.wMinute)) ban.EndTime=time;
		if(bans_txt.GetStr("Ban","BannedBy","",buf)) StringCopy(ban.BannedBy,buf);
		if(bans_txt.GetStr("Ban","Comment","",buf)) StringCopy(ban.BanInfo,buf);
		Banned.push_back(ban);
	}
	ProcessBans();
}

bool FOServer::LoadClientsData()
{
	WriteLog("Indexing client data.\n");

	LastClientId=0;
	ClientsData.reserve(10000);
	WIN32_FIND_DATA fdata;
	HANDLE h=FindFirstFile(".\\save\\clients\\*.client",&fdata);
	if(h==INVALID_HANDLE_VALUE)
	{
		WriteLog("Clients data not found.\n");
		return true;
	}

	DwordSet id_already;
	while(true)
	{
		// Take name from file title
		char name[MAX_FOPATH];
		StringCopy(name,fdata.cFileName);
		*strstr(name,".client")=0;

		// Take id and password from file
		char fname_[MAX_FOPATH];
		sprintf(fname_,".\\save\\clients\\%s",fdata.cFileName);
		FILE* f=NULL;
		if(fopen_s(&f,fname_,"rb"))
		{
			WriteLog("Unable to open client save file<%s>.\n",fname_);
			return false;
		}

		char pass[MAX_NAME+1]={0};
		DWORD id=-1;
		bool read_ok=(fread(pass,sizeof(pass),1,f) && fread(&id,sizeof(id),1,f));
		fclose(f);

		DWORD pass_len=strlen(pass);
		if(!read_ok || !IS_USER_ID(id) || pass_len<MIN_NAME || pass_len>MAX_NAME || !CheckUserPass(pass))
		{
			WriteLog("Wrong id<%u> or password<%s> of client<%s>. Skip.\n",id,pass,name);
			if(!FindNextFile(h,&fdata)) break;
			continue;
		}

		if(id_already.count(id))
		{
			WriteLog("Id<%u> of user<%s> already used by another client.\n",id,name);
			return false;
		}
		id_already.insert(id);

		// Add
		//WriteLog("Indexing<%s> -> id<%u> name<%s> pass<%s>.\n",fdata.cFileName,id,name,pass);
		ClientData data;
		data.Clear();
		StringCopy(data.ClientName,name);
		StringCopy(data.ClientPass,pass);
		data.ClientId=id;
		ClientsData.push_back(data);
		if(id>LastClientId) LastClientId=id;

		if(!FindNextFile(h,&fdata)) break;
	}

	FindClose(h);
	if(ClientsData.size()>10000) ClientsData.reserve(ClientsData.size()*2);
	WriteLog("Indexing complete, clients found<%u>.\n",ClientsData.size());
	return true;
}

bool FOServer::SaveClient(Client* cl, bool deferred)
{
	if(Singleplayer) return true;

	if(!strcmp(cl->Name,"err") || !cl->GetId())
	{
		WriteLog(__FUNCTION__" - Trying save not valid client.\n");
		return false;
	}

	CritDataExt* data_ext=cl->GetDataExt();
	if(!data_ext)
	{
		WriteLog(__FUNCTION__" - Can't get extended critter data.\n");
		return false;
	}

	if(deferred && WorldSaveManager)
	{
		AddClientSaveData(cl);
	}
	else
	{
		char fname[MAX_PATH];
		sprintf(fname,".\\save\\clients\\%s.client",cl->Name);
		FILE* f=NULL;
		if(fopen_s(&f,fname,"wb"))
		{
			WriteLog(__FUNCTION__" - Unable to open client save file<%s>.\n",fname);
			return false;
		}

		_fwrite_nolock(cl->Pass,sizeof(cl->Pass),1,f);
		_fwrite_nolock(&cl->Data,sizeof(cl->Data),1,f);
		_fwrite_nolock(data_ext,sizeof(CritDataExt),1,f);
		DWORD te_count=cl->CrTimeEvents.size();
		_fwrite_nolock(&te_count,sizeof(te_count),1,f);
		if(te_count) _fwrite_nolock(&cl->CrTimeEvents[0],te_count*sizeof(Critter::CrTimeEvent),1,f);
		fclose(f);
	}
	return true;
}

bool FOServer::LoadClient(Client* cl)
{
	if(Singleplayer) return true;

	CritDataExt* data_ext=cl->GetDataExt();
	if(!data_ext)
	{
		WriteLog(__FUNCTION__" - Can't get extended critter data.\n");
		return false;
	}

	char fname[MAX_PATH];
	sprintf(fname,".\\save\\clients\\%s.client",cl->Name);
	FILE* f=NULL;
	if(fopen_s(&f,fname,"rb"))
	{
		WriteLog(__FUNCTION__" - Unable to open client save file<%s>.\n",fname);
		return false;
	}

	if(!fread(cl->Pass,sizeof(cl->Pass),1,f)) goto label_FileTruncated;
	if(!fread(&cl->Data,sizeof(cl->Data),1,f)) goto label_FileTruncated;
	if(!fread(data_ext,sizeof(CritDataExt),1,f)) goto label_FileTruncated;
	DWORD te_count;
	if(!fread(&te_count,sizeof(te_count),1,f)) goto label_FileTruncated;
	if(te_count)
	{
		cl->CrTimeEvents.resize(te_count);
		if(!fread(&cl->CrTimeEvents[0],te_count*sizeof(Critter::CrTimeEvent),1,f)) goto label_FileTruncated;
	}

	fclose(f);
	return true;

label_FileTruncated:
	WriteLog(__FUNCTION__" - Client save file<%s> truncated.\n",fname);
	fclose(f);
	return false;
}

bool FOServer::NewWorld()
{
	UnloadWorld();
	InitGameTime();
	if(!MapMngr.GenerateWorld("GenerateWorld.cfg",PT_SERVER_MAPS)) return false;
	return true;
}

void FOServer::SaveWorld(const char* name)
{
	if(!name && Singleplayer) return; // Disable autosaving in singleplayer mode

	double tick;
	if(WorldSaveManager)
	{
		// Be sure what Dump_Work thread in wait state
		WaitForSingleObject(DumpEndEvent,INFINITE);
		tick=Timer::AccurateTick();
		WorldSaveDataBufCount=0;
		WorldSaveDataBufFreeSize=0;
		ClientsSaveDataCount=0;
	}
	else
	{
		// Save directly to file
		tick=Timer::AccurateTick();
		char fname[64];
		sprintf(fname,".\\save\\world%04d.fo",SaveWorldIndex+1);
		DumpFile=NULL;
		if(fopen_s(&DumpFile,name?name:fname,"wb"))
		{
			WriteLog("Can't create dump file<%s>.\n",name?name:fname);
			return;
		}
	}

	// ServerFunctions.SaveWorld
	SaveWorldDeleteIndexes.clear();
	if(Script::PrepareContext(ServerFunctions.WorldSave,CALL_FUNC_STR,"Game"))
	{
		asIScriptArray* delete_indexes=Script::CreateArray("uint[]");
		Script::SetArgDword(SaveWorldIndex+1);
		Script::SetArgObject(delete_indexes);
		if(Script::RunPrepared()) Script::AssignScriptArrayInVector(SaveWorldDeleteIndexes,delete_indexes);
		delete_indexes->Release();
	}

	// Version
	DWORD version=WORLD_SAVE_LAST;
	AddWorldSaveData(&version,sizeof(version));

	// SaveGameInfoFile
	SaveGameInfoFile();

	// SaveAllLocationsAndMapsFile
	MapMngr.SaveAllLocationsAndMapsFile(AddWorldSaveData);

	// SaveCrittersFile
	CrMngr.SaveCrittersFile(AddWorldSaveData);

	// SaveAllItemsFile
	ItemMngr.SaveAllItemsFile(AddWorldSaveData);

	// SaveVarsDataFile
	VarMngr.SaveVarsDataFile(AddWorldSaveData);

	// SaveHoloInfoFile
	SaveHoloInfoFile();

	// SaveAnyDataFile
	SaveAnyDataFile();

	// SaveTimeEventsFile
	SaveTimeEventsFile();

	// SaveScriptFunctionsFile
	SaveScriptFunctionsFile();

	// End signature
	AddWorldSaveData(&version,sizeof(version));

	// SaveClient
	ConnectedClientsLocker.Lock();
	for(ClVecIt it=ConnectedClients.begin(),end=ConnectedClients.end();it!=end;++it)
	{
		Client* cl=*it;
		if(cl->GetId()) SaveClient(cl,true);
	}
	ConnectedClientsLocker.Unlock();

	SaveClientsLocker.Lock();
	for(ClVecIt it=SaveClients.begin(),end=SaveClients.end();it!=end;++it)
	{
		Client* cl=*it;
		SaveClient(cl,true);
		cl->Release();
	}
	SaveClients.clear();
	SaveClientsLocker.Unlock();

	// Finish collect data
	if(WorldSaveManager)
	{
		// Awake Dump_Work
		SetEvent(DumpBeginEvent);
	}
	else
	{
		_fclose_nolock(DumpFile);
		DumpFile=NULL;
		SaveWorldIndex++;
		if(SaveWorldIndex>=WORLD_SAVE_MAX_INDEX) SaveWorldIndex=0;
	}

	WriteLog("World saved in %g ms.\n",Timer::AccurateTick()-tick);
}

bool FOServer::LoadWorld(const char* name)
{
	UnloadWorld();

	FILE* f=NULL;
	if(!name)
	{
		for(int i=WORLD_SAVE_MAX_INDEX;i>=1;i--)
		{
			char fname[64];
			sprintf(fname,".\\save\\world%04d.fo",i);

			if(!fopen_s(&f,fname,"rb"))
			{
				WriteLog("Begin load world from dump file<%s>.\n",fname);
				SaveWorldIndex=i;
				break;
			}
			f=NULL;
		}

		if(!f)
		{
			WriteLog("World dump file not found.\n");
			SaveWorldIndex=0;
			return true;
		}
	}
	else
	{
		if(fopen_s(&f,name,"rb"))
		{
			WriteLog("World dump file<%s> not found.\n",name);
			return false;
		}

		WriteLog("Begin load world from dump file<%s>.\n",name);
	}

	// File begin
	DWORD version=0;
	fread(&version,sizeof(version),1,f);
	if(version!=WORLD_SAVE_V1 && version!=WORLD_SAVE_V2 && version!=WORLD_SAVE_V3 && version!=WORLD_SAVE_V4 &&
		version!=WORLD_SAVE_V5 && version!=WORLD_SAVE_V6 && version!=WORLD_SAVE_V7 && version!=WORLD_SAVE_V8 &&
		version!=WORLD_SAVE_V9 && version!=WORLD_SAVE_V10)
	{
		WriteLog("Unknown version<%u> of world dump file.\n",version);
		fclose(f);
		return false;
	}
	if(version<WORLD_SAVE_V9)
	{
		WriteLog("Version of save file is not supported.\n");
		fclose(f);
		return false;
	}

	// Main data
	if(!LoadGameInfoFile(f)) return false;
	if(!MapMngr.LoadAllLocationsAndMapsFile(f)) return false;
	if(!CrMngr.LoadCrittersFile(f)) return false;
	if(!ItemMngr.LoadAllItemsFile(f)) return false;
	if(!VarMngr.LoadVarsDataFile(f,version)) return false;
	if(!LoadHoloInfoFile(f)) return false;
	if(!LoadAnyDataFile(f)) return false;
	if(!LoadTimeEventsFile(f)) return false;
	if(!LoadScriptFunctionsFile(f)) return false;

	// File end
	DWORD version_=0;
	if(!fread(&version_,sizeof(version_),1,f) || version!=version_)
	{
		WriteLog("World dump file truncated.\n");
		fclose(f);
		return false;
	}
	fclose(f);

	// Initialize data
	InitGameTime();
	if(!TransferAllNpc()) return false; // Transfer critter copies to maps
	if(!TransferAllItems()) return false; // Transfer items copies to critters and maps
	MapMngr.RunInitScriptMaps(); // Init scripts for maps
	CrMngr.RunInitScriptCritters(); // Init scripts for critters
	ItemMngr.RunInitScriptItems(); // Init scripts for maps
	return true;
}

void FOServer::UnloadWorld()
{
	// Delete critter and map jobs
	Job::Erase(JOB_CRITTER);
	Job::Erase(JOB_MAP);

	// Locations / maps
	MapMngr.Clear();

	// Critters
	CrMngr.Clear();

	// Items
	ItemMngr.Clear();

	// Vars
	VarMngr.Clear();

	// Holo info
	HolodiskInfo.clear();
	LastHoloId=USER_HOLO_START_NUM;

	// Any data
	AnyData.clear();

	// Time events
	for(TimeEventVecIt it=TimeEvents.begin(),end=TimeEvents.end();it!=end;++it) delete *it;
	TimeEvents.clear();
	TimeEventsLastNum=0;

	// Script functions
	Script::ResizeCache(0);

	// Singleplayer header
	SingleplayerSave.Valid=false;
}

void FOServer::AddWorldSaveData(void* data, size_t size)
{
	if(!WorldSaveManager)
	{
		_fwrite_nolock(data,size,1,DumpFile);
		return;
	}

	if(!WorldSaveDataBufFreeSize)
	{
		WorldSaveDataBufCount++;
		WorldSaveDataBufFreeSize=WORLD_SAVE_DATA_BUFFER_SIZE;

		if(WorldSaveDataBufCount>=WorldSaveData.size())
		{
			MEMORY_PROCESS(MEMORY_SAVE_DATA,WORLD_SAVE_DATA_BUFFER_SIZE);
			WorldSaveData.push_back(new BYTE[WORLD_SAVE_DATA_BUFFER_SIZE]);
		}
	}

	size_t flush=(size<=WorldSaveDataBufFreeSize?size:WorldSaveDataBufFreeSize);
	if(flush)
	{
		BYTE* ptr=WorldSaveData[WorldSaveDataBufCount-1];
		memcpy(&ptr[WORLD_SAVE_DATA_BUFFER_SIZE-WorldSaveDataBufFreeSize],data,flush);
		WorldSaveDataBufFreeSize-=flush;
		if(!WorldSaveDataBufFreeSize) AddWorldSaveData(((BYTE*)data)+flush,size-flush);
	}
}

void FOServer::AddClientSaveData(Client* cl)
{
	if(ClientsSaveDataCount>=ClientsSaveData.size())
	{
		ClientsSaveData.push_back(ClientSaveData());
		MEMORY_PROCESS(MEMORY_SAVE_DATA,sizeof(ClientSaveData));
	}

	ClientSaveData& csd=ClientsSaveData[ClientsSaveDataCount];
	memcpy(csd.Name,cl->Name,sizeof(csd.Name));
	memcpy(csd.Password,cl->Pass,sizeof(csd.Password));
	memcpy(&csd.Data,&cl->Data,sizeof(cl->Data));
	memcpy(&csd.DataExt,cl->GetDataExt(),sizeof(CritDataExt));
	csd.TimeEvents=cl->CrTimeEvents;

	ClientsSaveDataCount++;
}

unsigned int __stdcall FOServer::Dump_Work(void* data)
{
	char fname[MAX_FOPATH];

	while(true)
	{
		if(WaitForSingleObject(DumpBeginEvent,INFINITE)!=WAIT_OBJECT_0) break;

		// Save world data
		FILE* fworld=NULL;
		sprintf(fname,".\\save\\world%04d.fo",SaveWorldIndex+1);
		if(!fopen_s(&fworld,fname,"wb"))
		{
			for(size_t i=0;i<WorldSaveDataBufCount;i++)
			{
				BYTE* ptr=WorldSaveData[i];
				size_t flush=WORLD_SAVE_DATA_BUFFER_SIZE;
				if(i==WorldSaveDataBufCount-1) flush-=WorldSaveDataBufFreeSize;
				fwrite(ptr,flush,1,fworld);
				Sleep(10);
			}
			fclose(fworld);
			SaveWorldIndex++;
			if(SaveWorldIndex>=WORLD_SAVE_MAX_INDEX) SaveWorldIndex=0;
		}
		else
		{
			WriteLog(__FUNCTION__" - Can't create world dump file<%s>.\n",fname);
		}

		// Save clients data
		for(size_t i=0;i<ClientsSaveDataCount;i++)
		{
			ClientSaveData& csd=ClientsSaveData[i];

			FILE* fc=NULL;
			sprintf(fname,".\\save\\clients\\%s.client",csd.Name);
			if(fopen_s(&fc,fname,"wb"))
			{
				WriteLog(__FUNCTION__" - Unable to open client save file<%s>.\n",fname);
				continue;
			}

			fwrite(csd.Password,sizeof(csd.Password),1,fc);
			fwrite(&csd.Data,sizeof(csd.Data),1,fc);
			fwrite(&csd.DataExt,sizeof(csd.DataExt),1,fc);
			DWORD te_count=csd.TimeEvents.size();
			fwrite(&te_count,sizeof(te_count),1,fc);
			if(te_count) fwrite(&csd.TimeEvents[0],te_count*sizeof(Critter::CrTimeEvent),1,fc);
			fclose(fc);
			Sleep(1);
		}

		// Clear old dump files
		for(DwordVecIt it=SaveWorldDeleteIndexes.begin(),end=SaveWorldDeleteIndexes.end();it!=end;++it)
		{
			FILE* fold=NULL;
			sprintf_s(fname,".\\save\\world%04d.fo",*it);
			if(!fopen_s(&fold,fname,"rb"))
			{
				fclose(fold);
				DeleteFile(fname);
			}
		}

		// Notify about end of processing
		SetEvent(DumpEndEvent);
	}
	return 0;
}

/************************************************************************/
/* RADIO                                                                */
/************************************************************************/

void FOServer::RadioClearChannels()
{
	SCOPE_SPINLOCK(RadioLocker);

	WriteLog("Clear radio channels.\n");
	for(int i=0;i<0x10000;i++) SAFEDEL(RadioChannels[i]);
	WriteLog("Clear radio channels complete.\n");
}

void FOServer::RadioAddPlayer(Client* cl, WORD channel)
{
	SCOPE_SPINLOCK(RadioLocker);

	DwordVec* cha=RadioChannels[channel];
	if(!cha)
	{
		cha=new(nothrow) DwordVec;
		if(!cha) return;
		RadioChannels[channel]=cha;
	}
	else
	{
		if(std::find(cha->begin(),cha->end(),cl->GetId())!=cha->end()) return;
	}

	cha->push_back(cl->GetId());
}

void FOServer::RadioErasePlayer(Client* cl, WORD channel)
{
	SCOPE_SPINLOCK(RadioLocker);

	DwordVec* cha=RadioChannels[channel];
	if(!cha) return;

	DwordVecIt it=std::find(cha->begin(),cha->end(),cl->GetId());
	if(it!=cha->end()) cha->erase(it);

	if(cha->empty())
	{
		delete cha;
		RadioChannels[channel]=NULL;
	}
}

void FOServer::RadioSendText(Critter* cr, WORD channel, const char* text, bool unsafe_text)
{
	SCOPE_SPINLOCK(RadioLocker);

	DwordVec* cha=RadioChannels[channel];
	if(!cha) return;

	if(!text || !text[0]) return;
	WORD str_len=strlen(text);

#pragma MESSAGE("Add option for allowing sending radio message owner.")
	DWORD from_id=0; // (cr?cr->GetId():0);
	WORD intellect=(cr?cr->IntellectCacheValue:0);

	for(DwordVecIt it=cha->begin();it!=cha->end();)
	{
		Client* send_cl=(Client*)CrMngr.GetCritter(*it);
		if(!send_cl)
		{
			it=cha->erase(it);
			continue;
		}

		if(send_cl->GetRadio() && send_cl->GetRadioChannel()==channel)
		{
			send_cl->Send_TextEx(from_id,text,str_len,SAY_RADIO,intellect,unsafe_text);
			++it;
		}
		else
		{
			it=cha->erase(it);
		}
	}

	if(cha->empty())
	{
		delete cha;
		RadioChannels[channel]=NULL;
	}
}

void FOServer::RadioSendMsg(Critter* cr, WORD channel, WORD text_msg, DWORD num_str)
{
	SCOPE_SPINLOCK(RadioLocker);

	DwordVec* cha=RadioChannels[channel];
	if(!cha) return;

	for(DwordVecIt it=cha->begin();it!=cha->end();)
	{
		Client* send_cl=(Client*)CrMngr.GetCritter(*it);
		if(!send_cl)
		{
			it=cha->erase(it);
			continue;
		}

		if(send_cl->GetRadio() && send_cl->GetRadioChannel()==channel)
		{
			send_cl->Send_TextMsg(cr,num_str,SAY_RADIO,text_msg);
			++it;
		}
		else
		{
			it=cha->erase(it);
		}
	}
}

/************************************************************************/
/* Scores                                                               */
/************************************************************************/

void FOServer::SetScore(int score, Critter* cr, int val)
{
	SCOPE_SPINLOCK(BestScoresLocker);

	cr->Data.Scores[score]+=val;
	if(BestScores[score].ClientId==cr->GetId()) return;
	if(BestScores[score].Value>=cr->Data.Scores[score]) return; //TODO: less/greater
	BestScores[score].Value=cr->Data.Scores[score];
	BestScores[score].ClientId=cr->GetId();
	StringCopy(BestScores[score].ClientName,cr->GetName());
}

void FOServer::SetScore(int score, const char* name)
{
	SCOPE_SPINLOCK(BestScoresLocker);

	BestScores[score].ClientId=0;
	BestScores[score].Value=0;
	StringCopy(BestScores[score].ClientName,name);
}

const char* FOServer::GetScores()
{
	SCOPE_SPINLOCK(BestScoresLocker);

	static THREAD char scores[SCORE_NAME_LEN*SCORES_MAX]; // Only names
	for(int i=0;i<SCORES_MAX;i++)
	{
		char* offs=&scores[i*SCORE_NAME_LEN]; // Name
		memcpy(offs,BestScores[i].ClientName,SCORE_NAME_LEN);
	}

	return scores;
}

void FOServer::ClearScore(int score)
{
	SCOPE_SPINLOCK(BestScoresLocker);

	BestScores[score].ClientId=0;
	BestScores[score].ClientName[0]='\0';
	BestScores[score].Value=0;
}

void FOServer::Process_GetScores(Client* cl)
{
	DWORD tick=Timer::FastTick();
	if(tick<cl->LastSendScoresTick+SCORES_SEND_TIME)
	{
		WriteLog(__FUNCTION__" - Client<%s> ignore send scores timeout.\n",cl->GetInfo());
		return;
	}
	cl->LastSendScoresTick=tick;

	MSGTYPE msg=NETMSG_SCORES;
	const char* scores=GetScores();

	BOUT_BEGIN(cl);
	cl->Bout << msg;
	cl->Bout.Push(scores,SCORE_NAME_LEN*SCORES_MAX);
	BOUT_END(cl);
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

void FOServer::VarsGarbarger(bool force)
{
	if(!force)
	{
		if(!VarsGarbageTime) return;
		DWORD tick=Timer::FastTick();
		if(tick-VarsGarbageLastTick<VarsGarbageTime) return;
	}

	// Get client and npc ids
	DwordSet ids_clients;
	if(!Singleplayer)
	{
		for(ClientDataVecIt it=ClientsData.begin(),end=ClientsData.end();it!=end;++it)
			ids_clients.insert((*it).ClientId);
	}
	else
	{
		ids_clients.insert(1);
	}

	// Get npc ids
	DwordSet ids_npcs;
	CrMngr.GetNpcIds(ids_npcs);

	// Get location and map ids
	DwordSet ids_locs,ids_maps;
	MapMngr.GetLocationAndMapIds(ids_locs,ids_maps);

	// Get item ids
	DwordSet ids_items;
	ItemMngr.GetItemIds(ids_items);

	VarMngr.ClearUnusedVars(ids_npcs,ids_clients,ids_locs,ids_maps,ids_items);
	VarsGarbageLastTick=Timer::FastTick();
}

/************************************************************************/
/* Time events                                                          */
/************************************************************************/

void FOServer::SaveTimeEventsFile()
{
	DWORD count=0;
	for(TimeEventVecIt it=TimeEvents.begin(),end=TimeEvents.end();it!=end;++it) if((*it)->IsSaved) count++;
	AddWorldSaveData(&count,sizeof(count));
	for(TimeEventVecIt it=TimeEvents.begin(),end=TimeEvents.end();it!=end;++it)
	{
		TimeEvent* te=*it;
		if(!te->IsSaved) continue;
		AddWorldSaveData(&te->Num,sizeof(te->Num));
		WORD script_name_len=te->FuncName.length();
		AddWorldSaveData(&script_name_len,sizeof(script_name_len));
		AddWorldSaveData((void*)te->FuncName.c_str(),script_name_len);
		AddWorldSaveData(&te->FullSecond,sizeof(te->FullSecond));
		AddWorldSaveData(&te->Rate,sizeof(te->Rate));
		DWORD values_size=te->Values.size();
		AddWorldSaveData(&values_size,sizeof(values_size));
		if(values_size) AddWorldSaveData(&te->Values[0],values_size*sizeof(DWORD));
	}
}

bool FOServer::LoadTimeEventsFile(FILE* f)
{
	WriteLog("Load time events...");

	TimeEventsLastNum=0;

	DWORD count=0;
	if(!fread(&count,sizeof(count),1,f)) return false;
	for(DWORD i=0;i<count;i++)
	{
		DWORD num;
		if(!fread(&num,sizeof(num),1,f)) return false;

		char script_name[MAX_SCRIPT_NAME*2+2];
		WORD script_name_len;
		if(!fread(&script_name_len,sizeof(script_name_len),1,f)) return false;
		if(!fread(script_name,script_name_len,1,f)) return false;
		script_name[script_name_len]=0;

		DWORD begin_second;
		if(!fread(&begin_second,sizeof(begin_second),1,f)) return false;
		DWORD rate;
		if(!fread(&rate,sizeof(rate),1,f)) return false;

		DwordVec values;
		DWORD values_size;
		if(!fread(&values_size,sizeof(values_size),1,f)) return false;
		if(values_size)
		{
			values.resize(values_size);
			if(!fread(&values[0],values_size*sizeof(DWORD),1,f)) return false;
		}

		bool singed=false;
		int bind_id=Script::Bind(script_name,"uint %s(int[]@)",false,true);
		if(bind_id<=0) bind_id=Script::Bind(script_name,"uint %s(uint[]@)",false);
		else singed=true;
		if(bind_id<=0)
		{
			WriteLog("Unable to bind script function, event num<%u>, name<%s>.\n",num,script_name);
			continue;
		}

		TimeEvent* te=new(nothrow) TimeEvent();
		if(!te) return false;
		te->FullSecond=begin_second;
		te->Num=num;
		te->FuncName=script_name;
		te->BindId=bind_id;
		te->Rate=rate;
		te->SignedValues=singed;
		te->IsSaved=true;
		te->InProcess=0;
		te->EraseMe=false;
		if(values_size) te->Values=values;

		TimeEvents.push_back(te);
		if(num>TimeEventsLastNum) TimeEventsLastNum=num;
	}

	WriteLog("complete, count<%u>.\n",count);
	return true;
}

void FOServer::AddTimeEvent(TimeEvent* te)
{
	// Invoked in locked scope

	for(TimeEventVecIt it=TimeEvents.begin(),end=TimeEvents.end();it!=end;++it)
	{
		TimeEvent* te_=*it;
		if(te->FullSecond<te_->FullSecond)
		{
			TimeEvents.insert(it,te);
			return;
		}
	}
	TimeEvents.push_back(te);
}

DWORD FOServer::CreateTimeEvent(DWORD begin_second, const char* script_name, int values, DWORD val1, asIScriptArray* val2, bool save)
{
	char module_name[256];
	char func_name[256];
	if(!Script::ReparseScriptName(script_name,module_name,func_name)) return 0;

	bool singed=false;
	int bind_id=Script::Bind(module_name,func_name,"uint %s(int[]@)",false,true);
	if(bind_id<=0) bind_id=Script::Bind(module_name,func_name,"uint %s(uint[]@)",false);
	else singed=true;
	if(bind_id<=0) return 0;

	TimeEvent* te=new(nothrow) TimeEvent();
	if(!te) return 0;

	SCOPE_SPINLOCK(TimeEventsLocker);

	te->FullSecond=begin_second;
	te->Num=TimeEventsLastNum+1;
	te->FuncName=Str::Format("%s@%s",module_name,func_name);
	te->BindId=bind_id;
	te->SignedValues=singed;
	te->IsSaved=save;
	te->InProcess=0;
	te->EraseMe=false;
	te->Rate=0;

	if(values==1)
	{
		// Single value
		te->Values.push_back(val1);
	}
	else if(values==2)
	{
		// Array values
		DWORD count=val2->GetElementCount();
		if(count)
		{
			te->Values.resize(count);
			memcpy(&te->Values[0],val2->GetElementPointer(0),count*sizeof(DWORD));
		}
	}

	AddTimeEvent(te);
	TimeEventsLastNum++;
	return TimeEventsLastNum;
}

void FOServer::TimeEventEndScriptCallback()
{
	SCOPE_SPINLOCK(TimeEventsLocker);

	DWORD tid=GetCurrentThreadId();
	for(TimeEventVecIt it=TimeEvents.begin();it!=TimeEvents.end();)
	{
		TimeEvent* te=*it;
		if(te->InProcess==tid)
		{
			te->InProcess=0;

			if(te->EraseMe)
			{
				it=TimeEvents.erase(it);
				delete te;
			}
		}
		else ++it;
	}
}

bool FOServer::GetTimeEvent(DWORD num, DWORD& duration, asIScriptArray* values)
{
	TimeEventsLocker.Lock();

	TimeEvent* te=NULL;
	DWORD tid=GetCurrentThreadId();

	// Find event
	while(true)
	{
		for(TimeEventVecIt it=TimeEvents.begin(),end=TimeEvents.end();it!=end;++it)
		{
			TimeEvent* te_=*it;
			if(te_->Num==num)
			{
				te=te_;
				break;
			}
		}

		// Event not found or erased
		if(!te || te->EraseMe)
		{
			TimeEventsLocker.Unlock();
			return false;
		}

		// Wait of other threads end work with event
		if(te->InProcess && te->InProcess!=tid)
		{
			TimeEventsLocker.Unlock();
			Sleep(0);
			TimeEventsLocker.Lock();
		}
		else
		{
			// Begin work with event
			break;
		}
	}

	// Fill data
	if(values) Script::AppendVectorToArray(te->Values,values);
	duration=(te->FullSecond>GameOpt.FullSecond?te->FullSecond-GameOpt.FullSecond:0);

	// Lock for current thread
	te->InProcess=tid;

	// Add end of script execution callback to unlock event if SetTimeEvent not be called
	if(LogicThreadCount>1) Script::AddEndExecutionCallback(TimeEventEndScriptCallback);

	TimeEventsLocker.Unlock();
	return true;
}

bool FOServer::SetTimeEvent(DWORD num, DWORD duration, asIScriptArray* values)
{
	TimeEventsLocker.Lock();

	TimeEvent* te=NULL;
	DWORD tid=GetCurrentThreadId();

	// Find event
	while(true)
	{
		for(TimeEventVecIt it=TimeEvents.begin(),end=TimeEvents.end();it!=end;++it)
		{
			TimeEvent* te_=*it;
			if(te_->Num==num)
			{
				te=te_;
				break;
			}
		}

		// Event not found or erased
		if(!te || te->EraseMe)
		{
			TimeEventsLocker.Unlock();
			return false;
		}

		// Wait of other threads end work with event
		if(te->InProcess && te->InProcess!=tid)
		{
			TimeEventsLocker.Unlock();
			Sleep(0);
			TimeEventsLocker.Lock();
		}
		else
		{
			// Begin work with event
			break;
		}
	}

	// Fill data, lock event to current thread
	if(values) Script::AssignScriptArrayInVector(te->Values,values);
	te->FullSecond=GameOpt.FullSecond+duration;

	// Unlock from current thread
	te->InProcess=0;

	TimeEventsLocker.Unlock();
	return true;
}

bool FOServer::EraseTimeEvent(DWORD num)
{
	SCOPE_SPINLOCK(TimeEventsLocker);

	for(TimeEventVecIt it=TimeEvents.begin(),end=TimeEvents.end();it!=end;++it)
	{
		TimeEvent* te=*it;
		if(te->Num==num)
		{
			if(te->InProcess)
			{
				te->EraseMe=true;
			}
			else
			{
				delete te;
				TimeEvents.erase(it);
			}
			return true;
		}
	}

	return false;
}

void FOServer::ProcessTimeEvents()
{
	TimeEventsLocker.Lock();

	TimeEvent* cur_event=NULL;
	for(TimeEventVecIt it=TimeEvents.begin(),end=TimeEvents.end();it!=end;++it)
	{
		TimeEvent* te=*it;
		if(!te->InProcess && te->FullSecond<=GameOpt.FullSecond)
		{
			te->InProcess=GetCurrentThreadId();
			cur_event=te;
			break;
		}
	}

	TimeEventsLocker.Unlock();

	if(!cur_event) return;

	DWORD wait_time=0;
	if(Script::PrepareContext(cur_event->BindId,CALL_FUNC_STR,Str::Format("Time event<%u>",cur_event->Num)))
	{
		asIScriptArray* values=NULL;
		DWORD size=cur_event->Values.size();

		if(size>0)
		{
			if(cur_event->SignedValues)
				values=Script::CreateArray("int[]");
			else
				values=Script::CreateArray("uint[]");

			if(!values)
			{
				WriteLog(__FUNCTION__" - Create values array fail. Wait 10 real minutes.\n");
				wait_time=GameOpt.TimeMultiplier*600;
			}
			else
			{
				values->Resize(size);
				memcpy(values->GetElementPointer(0),(void*)&cur_event->Values[0],size*sizeof(DWORD));
			}
		}

		if(!size || values)
		{
			Script::SetArgObject(values);
			if(!Script::RunPrepared())
			{
				WriteLog(__FUNCTION__" - RunPrepared fail. Wait 10 real minutes.\n");
				wait_time=GameOpt.TimeMultiplier*600;
			}
			else
			{
				wait_time=Script::GetReturnedDword();
				if(wait_time && values) // Refresh array
				{
					DWORD arr_size=values->GetElementCount();
					cur_event->Values.resize(arr_size);
					if(arr_size) memcpy(&cur_event->Values[0],values->GetElementPointer(0),arr_size*sizeof(cur_event->Values[0]));
				}
			}

			if(values) values->Release();
		}
	}
	else
	{
		WriteLog(__FUNCTION__" - Game contexts prepare fail. Wait 10 real minutes.\n");
		wait_time=GameOpt.TimeMultiplier*600;
	}

	TimeEventsLocker.Lock();

	if(wait_time && !cur_event->EraseMe)
	{
		cur_event->FullSecond=GameOpt.FullSecond+wait_time;
		cur_event->Rate++;
		AddTimeEvent(cur_event);
		cur_event->InProcess=0;
	}
	else
	{
		TimeEventVecIt it=std::find(TimeEvents.begin(),TimeEvents.end(),cur_event);
		TimeEvents.erase(it);
		delete cur_event;
	}

	TimeEventsLocker.Unlock();
}

DWORD FOServer::GetTimeEventsCount()
{
	SCOPE_SPINLOCK(TimeEventsLocker);

	DWORD count=TimeEvents.size();
	return count;
}

string FOServer::GetTimeEventsStatistics()
{
	SCOPE_SPINLOCK(TimeEventsLocker);

	static string result;
	char str[1024];
	sprintf(str,"Time events: %u\n",TimeEvents.size());
	result=str;
	SYSTEMTIME st=GetGameTime(GameOpt.FullSecond);
	sprintf(str,"Game time: %02u.%02u.%04u %02u:%02u:%02u\n",st.wDay,st.wMonth,st.wYear,st.wHour,st.wMinute,st.wSecond);
	result+=str;
	result+="Number    Date       Time     Rate Saved Function                            Values\n";
	for(size_t i=0,j=TimeEvents.size();i<j;i++)
	{
		TimeEvent* te=TimeEvents[i];
		st=GetGameTime(te->FullSecond);
		sprintf(str,"%09u %02u.%02u.%04u %02u:%02u:%02u %04u %-5s %-35s",te->Num,st.wDay,st.wMonth,st.wYear,st.wHour,st.wMinute,st.wSecond,te->Rate,te->IsSaved?"true":"false",te->FuncName.c_str());
		result+=str;
		for(size_t k=0,l=te->Values.size();k<l;k++)
		{
			sprintf(str," %-10u",te->Values[k]);
			result+=str;
		}
		result+="\n";
	}
	return result;
}

void FOServer::SaveScriptFunctionsFile()
{
	const StrVec& cache=Script::GetScriptFuncCache();
	DWORD count=cache.size();
	AddWorldSaveData(&count,sizeof(count));
	for(size_t i=0,j=cache.size();i<j;i++)
	{
		const string& func_name=cache[i];
		DWORD len=func_name.length();
		AddWorldSaveData(&len,sizeof(len));
		AddWorldSaveData((void*)func_name.c_str(),len);
	}
}

bool FOServer::LoadScriptFunctionsFile(FILE* f)
{
	DWORD count=0;
	if(!fread(&count,sizeof(count),1,f)) return false;
	for(DWORD i=0;i<count;i++)
	{
		Script::ResizeCache(i);

		char script[1024];
		DWORD len=0;
		if(!fread(&len,sizeof(len),1,f)) return false;
		if(!fread(script,len,1,f)) return false;
		script[len]=0;

		// Cut off decl
		char* decl=strstr(script,"|");
		if(!decl)
		{
			WriteLog(__FUNCTION__" - Function declaration not found, script<%s>.\n",script);
			continue;
		}
		*decl=0;
		decl++;

		// Parse
		if(!Script::GetScriptFuncNum(script,decl))
		{
			WriteLog(__FUNCTION__" - Function<%s,%s> not found.\n",script,decl);
			continue;
		}
	}
	return true;
}

/************************************************************************/
/* Any data                                                             */
/************************************************************************/
void FOServer::SaveAnyDataFile()
{
	DWORD count=AnyData.size();
	AddWorldSaveData(&count,sizeof(count));
	for(AnyDataMapIt it=AnyData.begin(),end=AnyData.end();it!=end;++it)
	{
		const string& name=(*it).first;
		ByteVec& data=(*it).second;
		DWORD name_len=name.length();
		AddWorldSaveData(&name_len,sizeof(name_len));
		AddWorldSaveData((void*)name.c_str(),name_len);
		DWORD data_len=data.size();
		AddWorldSaveData(&data_len,sizeof(data_len));
		if(data_len) AddWorldSaveData(&data[0],data_len);
	}
}

bool FOServer::LoadAnyDataFile(FILE* f)
{
	ByteVec data;
	DWORD count=0;
	if(!fread(&count,sizeof(count),1,f)) return false;
	for(DWORD i=0;i<count;i++)
	{
		char name[MAX_FOTEXT];
		DWORD name_len;
		if(!fread(&name_len,sizeof(name_len),1,f)) return false;
		if(!fread(name,name_len,1,f)) return false;
		name[name_len]=0;

		DWORD data_len;
		if(!fread(&data_len,sizeof(data_len),1,f)) return false;
		data.resize(data_len);
		if(data_len && !fread(&data[0],data_len,1,f)) return false;

		AnyDataMapInsert result=AnyData.insert(AnyDataMapVal(name,data));
		MEMORY_PROCESS(MEMORY_ANY_DATA,(*result.first).second.capacity());
	}
	return true;
}

bool FOServer::SetAnyData(const string& name, const BYTE* data, DWORD data_size)
{
	SCOPE_SPINLOCK(AnyDataLocker);

	AnyDataMapInsert result=AnyData.insert(AnyDataMapVal(name,ByteVec()));
	ByteVec& data_=(*result.first).second;

	MEMORY_PROCESS(MEMORY_ANY_DATA,-(int)data_.capacity());
	data_.resize(data_size);
	memcpy(&data_[0],data,data_size);
	MEMORY_PROCESS(MEMORY_ANY_DATA,data_.capacity());
	return true;
}

bool FOServer::GetAnyData(const string& name, asIScriptArray& script_array)
{
	SCOPE_SPINLOCK(AnyDataLocker);

	AnyDataMapIt it=AnyData.find(name);
	if(it==AnyData.end()) return false;

	ByteVec& data=(*it).second;
	size_t length=data.size();

	if(!length)
	{
		script_array.Resize(0);
		return true;
	}

	DWORD element_size=script_array.GetElementSize();
	script_array.Resize(length/element_size+((length%element_size)?1:0));
	memcpy(script_array.GetElementPointer(0),&data[0],length);
	return true;
}

bool FOServer::IsAnyData(const string& name)
{
	SCOPE_SPINLOCK(AnyDataLocker);

	AnyDataMapIt it=AnyData.find(name);
	bool present=(it!=AnyData.end());
	return present;
}

void FOServer::EraseAnyData(const string& name)
{
	SCOPE_SPINLOCK(AnyDataLocker);

	AnyData.erase(name);
}

string FOServer::GetAnyDataStatistics()
{
	SCOPE_SPINLOCK(AnyDataLocker);

	static string result;
	char str[256];
	result="Any data count: ";
	result+=_itoa(AnyData.size(),str,10);
	result+="\nName                          Length    Data\n";
	for(AnyDataMapIt it=AnyData.begin(),end=AnyData.end();it!=end;++it)
	{
		const string& name=(*it).first;
		ByteVec& data=(*it).second;
		sprintf(str,"%-30s",name.c_str());
		result+=str;
		sprintf(str,"%-10u",data.size());
		result+=str;
		for(size_t i=0,j=data.size();i<j;i++)
		{
			sprintf(str,"%02X",data[i]);
			result+=str;
		}
		result+="\n";
	}
	return result;
}

/*
bool FOServer::LoadAnyData()
{
	WriteLog("Load Any data...\n");

	if(!Sql.Query("SELECT * FROM `any_data`;"))
	{
		WriteLog("DB select error.\n");
		return false;
	}

	int res_count;
	if((res_count=Sql.StoreResult())<0)
	{
		WriteLog("DB store result error.\n");
		return false;
	}

	if(!res_count)
	{
		WriteLog("Any data not found.\n");
		return true;
	}

	SQL_RESULT res=NULL;
	DWORD loaded=0;
	for(int i=0;i<res_count;++i)
	{
		if(!(res=Sql.GetNextResult()))
		{
			WriteLog("DB get next result error.\n");
			return false;
		}

		DWORD id=atoi(res[0]);
		DWORD count=atoi(res[1]);
		char* str_data=res[2];

		DwordVec data;
		if(count)
		{
			data.resize(count);
			memcpy(&data[0],str_data,count*sizeof(DWORD));
		}

		AnyData.insert(DwordVecMapVal(id,data));
		loaded++;
	}

	WriteLog("Loaded Any data success, count<%u>.\n",loaded);
	return true;
}*/


/************************************************************************/
/*                                                                      */
/************************************************************************/