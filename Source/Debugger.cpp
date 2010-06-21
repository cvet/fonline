#include "StdAfx.h"
#include "Debugger.h"

#define MAX_BLOCKS        (25)
#define MAX_ENTRY         (2000)
#define MAX_PROCESS       (20)
double Ticks[MAX_BLOCKS][MAX_ENTRY][MAX_PROCESS];
int Identifiers[MAX_BLOCKS][MAX_ENTRY][MAX_PROCESS];
DWORD CurTick[MAX_BLOCKS][MAX_ENTRY];
int CurEntry[MAX_BLOCKS];

void Debugger::BeginCycle()
{
	ZeroMemory(Identifiers,sizeof(Identifiers));
	for(int i=0;i<MAX_BLOCKS;i++) CurEntry[i]=-1;
}

void Debugger::BeginBlock(int num_block)
{
	if(CurEntry[num_block]+1>=MAX_ENTRY) return;
	CurEntry[num_block]++;
	CurTick[num_block][CurEntry[num_block]]=0;
	Ticks[num_block][CurEntry[num_block]][0]=Timer::AccurateTick();
}

void Debugger::ProcessBlock(int num_block, int identifier)
{
	if(CurEntry[num_block]+1>=MAX_ENTRY) return;
	CurTick[num_block][CurEntry[num_block]]++;
	Ticks[num_block][CurEntry[num_block]][CurTick[num_block][CurEntry[num_block]]]=Timer::AccurateTick();
	Identifiers[num_block][CurEntry[num_block]][CurTick[num_block][CurEntry[num_block]]]=identifier;
}

void Debugger::EndCycle(double lag_to_show)
{
	for(int num_block=0;num_block<MAX_BLOCKS;num_block++)
	{
		for(int entry=0;entry<=CurEntry[num_block];entry++)
		{
			bool is_first=true;
			for(int i=1,j=CurTick[num_block][entry];i<=j;i++)
			{
				double diff=Ticks[num_block][entry][i]-Ticks[num_block][entry][i-1];
				if(diff>=lag_to_show)
				{
					if(is_first)
					{
						WriteLog("Lag in block %d...",num_block);
						is_first=false;
					}
					WriteLog("<%d,%g>",Identifiers[num_block][entry][i],diff);
				}
			}
			if(!is_first) WriteLog("\n");
		}
	}
}

void Debugger::ShowLags(int num_block, double lag_to_show)
{
	for(int entry=0;entry<=CurEntry[num_block];entry++)
	{
		double diff=Ticks[num_block][entry][CurTick[num_block][entry]]-Ticks[num_block][entry][0];
		if(diff>=lag_to_show)
		{
			WriteLog("Lag in block %d...",num_block);
			for(int i=1,j=CurTick[num_block][entry];i<=j;i++)
			{
				double diff_=Ticks[num_block][entry][i]-Ticks[num_block][entry][i-1];
				WriteLog("<%d,%g>",Identifiers[num_block][entry][i],diff_);
			}
			WriteLog("\n");
		}
	}
}


#define MAX_MEM_NODES          (14)
struct MemNode
{
	__int64 AllocMem;
	__int64 DeallocMem;
	__int64 MinAlloc;
	__int64 MaxAlloc;
} MemNodes[MAX_MEM_NODES]={0};

const char* MemBlockNames[MAX_MEM_NODES]=
{
	{"Static       "},
	{"Npc          "},
	{"Clients      "},
	{"Maps         "},
	{"Map fields   "},
	{"Proto maps   "},
	{"Net buffers  "},
	{"Vars         "},
	{"Npc planes   "},
	{"Items        "},
	{"Dialogs      "},
	{"Save data    "},
	{"Script string"},
	{"Angel Script "},
};

void Debugger::Memory(int block, int value)
{
	if(value)
	{
		MemNode& node=MemNodes[block];
		if(value>0)
		{
			node.AllocMem+=value;
			if(!node.MinAlloc || value<node.MinAlloc) node.MinAlloc=value;
			if(value>node.MaxAlloc) node.MaxAlloc=value;
		}
		else
		{
			node.DeallocMem-=value;
		}
	}
}

struct MemNodeStr
{
	char Name[256];
	__int64 AllocMem;
	__int64 DeallocMem;
	__int64 MinAlloc;
	__int64 MaxAlloc;
	bool operator==(const char* str)const{return !strcmp(str,Name);}
};
typedef vector<MemNodeStr> MemNodeStrVec;
typedef vector<MemNodeStr>::iterator MemNodeStrVecIt;
MemNodeStrVec MemNodesStr;

void Debugger::MemoryStr(const char* block, int value)
{
	if(block && value)
	{
		MemNodeStr* node;
		MemNodeStrVecIt it=std::find(MemNodesStr.begin(),MemNodesStr.end(),block);
		if(it==MemNodesStr.end())
		{
			MemNodeStr node_;
			StringCopy(node_.Name,block);
			node_.AllocMem=0;
			node_.DeallocMem=0;
			node_.MinAlloc=0;
			node_.MaxAlloc=0;
			MemNodesStr.push_back(node_);
			node=&MemNodesStr.back();
		}
		else
		{
			node=&(*it);
		}

		if(value>0)
		{
			node->AllocMem+=value;
			if(!node->MinAlloc || value<node->MinAlloc) node->MinAlloc=value;
			if(value>node->MaxAlloc) node->MaxAlloc=value;
		}
		else
		{
			node->DeallocMem-=value;
		}
	}
}

const char* Debugger::GetMemoryStatistics()
{
	static string result;
	result="Memory statistics:\n";

#ifdef FONLINE_SERVER
	char buf[512];
	__int64 all_alloc=0,all_dealloc=0;

	if(MemoryDebugLevel>=1)
	{
		result+="\n  Level 1            Memory        Alloc         Free    Min block    Max block\n";
		for(int i=0;i<MAX_MEM_NODES;i++)
		{
			MemNode& node=MemNodes[i];
			sprintf(buf,"%s : %12I64d %12I64d %12I64d %12I64d %12I64d\n",MemBlockNames[i],node.AllocMem-node.DeallocMem,node.AllocMem,node.DeallocMem,node.MinAlloc,node.MaxAlloc);
			result+=buf;
			all_alloc+=node.AllocMem;
			all_dealloc+=node.DeallocMem;
		}
		sprintf(buf,"Whole memory : %12I64d %12I64d %12I64d\n",all_alloc-all_dealloc,all_alloc,all_dealloc);
		result+=buf;
	}

	if(MemoryDebugLevel>=2)
	{
		all_alloc=all_dealloc=0;
		result+="\n  Level 2                                                  Memory        Alloc         Free    Min block    Max block\n";
		for(MemNodeStrVecIt it=MemNodesStr.begin(),end=MemNodesStr.end();it!=end;++it)
		{
			MemNodeStr& node=*it;
			sprintf(buf,"%-50s : %12I64d %12I64d %12I64d %12I64d %12I64d\n",node.Name,node.AllocMem-node.DeallocMem,node.AllocMem,node.DeallocMem,node.MinAlloc,node.MaxAlloc);
			result+=buf;
			all_alloc+=node.AllocMem;
			all_dealloc+=node.DeallocMem;
		}
		sprintf(buf,"Whole memory                                       : %12I64d %12I64d %12I64d\n",all_alloc-all_dealloc,all_alloc,all_dealloc);
		result+=buf;
	}

	if(MemoryDebugLevel<=0) result+="\n  Disabled\n";
#endif

	return result.c_str();
}

