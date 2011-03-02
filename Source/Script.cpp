#include "StdAfx.h"
#include "Script.h"
#include "Text.h"
#include "FileManager.h"
#include "Version.h"
#include "AngelScript/scriptstring.h"
#include "AngelScript/scriptany.h"
#include "AngelScript/scriptdictionary.h"
#include "AngelScript/scriptfile.h"
#include "AngelScript/scriptmath.h"
#include "AngelScript/scriptmath3d.h"
#include "AngelScript/scriptarray.h"
#include "AngelScript/Preprocessor/preprocess.h"
#include <strstream>
#include <process.h>

namespace Script
{

const char* ContextStatesStr[]=
{
	"Finished",
	"Suspended",
	"Aborted",
	"Exception",
	"Prepared",
	"Uninitialized",
	"Active",
	"Error",
};

class BindFunction
{
public:
	bool IsScriptCall;
	int ScriptFuncId;
	string ModuleName;
	string FuncName;
	string FuncDecl;
	size_t NativeFuncAddr;

	BindFunction(int func_id, size_t native_func_addr, const char* module_name,  const char* func_name, const char* func_decl)
	{
		IsScriptCall=(native_func_addr==0);
		ScriptFuncId=func_id;
		NativeFuncAddr=native_func_addr;
		ModuleName=module_name;
		FuncName=func_name;
		FuncDecl=func_decl;
	}
};
typedef vector<BindFunction> BindFunctionVec;

typedef std::vector<asIScriptModule*> ScriptModuleVec;
typedef std::vector<asIScriptModule*>::iterator ScriptModuleVecIt;

struct EngineData
{
	ScriptModuleVec Modules;
	Preprocessor::PragmaCallback* PrGlobalVar;
	Preprocessor::PragmaCallback* PrCrData;
	Preprocessor::PragmaCallback* PrBindFunc;
};

asIScriptEngine* Engine=NULL;
HANDLE EngineLogFile=NULL;
int ScriptsPath=PT_SCRIPTS;
bool LogDebugInfo=true;
StrVec WrongGlobalObjects;

#ifdef FONLINE_SERVER
DWORD GarbagerCycle=120000;
DWORD EvaluationCycle=120000;
double MaxGarbagerTime=40.0;
#else
DWORD GarbageCollectTime=120000;
#endif

#ifdef SCRIPT_MULTITHREADING
MutexCode GarbageLocker;
#endif

BindFunctionVec BindedFunctions;
#ifdef SCRIPT_MULTITHREADING
Mutex BindedFunctionsLocker;
#endif
StrVec ScriptFuncCache;
IntVec ScriptFuncBindId;

bool ConcurrentExecution=false;
#ifdef SCRIPT_MULTITHREADING
Mutex ConcurrentExecutionLocker;
#endif

bool LoadLibraryCompiler=false;

// Contexts
THREAD asIScriptContext* GlobalCtx[GLOBAL_CONTEXT_STACK_SIZE]={0};
THREAD DWORD GlobalCtxIndex=0;
class ActiveContext
{
public:
	asIScriptContext** Contexts;
	DWORD StartTick;
	ActiveContext(asIScriptContext** ctx, DWORD tick):Contexts(ctx),StartTick(tick){}
	bool operator==(asIScriptContext** ctx){return ctx==Contexts;}
};
typedef vector<ActiveContext> ActiveContextVec;
typedef vector<ActiveContext>::iterator ActiveContextVecIt;
ActiveContextVec ActiveContexts;
Mutex ActiveGlobalCtxLocker;

// Timeouts
DWORD RunTimeoutSuspend=600000; // 10 minutes
DWORD RunTimeoutMessage=300000; // 5 minutes
HANDLE RunTimeoutThreadHandle=NULL;
unsigned int __stdcall RunTimeoutThread(void*);

// #pragma globalvar "int __MyGlobalVar = 100"
class GvarPragmaCallback : public Preprocessor::PragmaCallback
{
private:
	set<string> addedVars;
	list<int> intArray;
	list<__int64> int64Array;
	list<CScriptString*> stringArray;
	list<float> floatArray;
	list<double> doubleArray;
	list<char> boolArray;

public:
	void pragma(const Preprocessor::PragmaInstance& instance)
	{
		string type,decl,value;
		char ch=0;
		istrstream str(instance.text.c_str());
		str >> type;
		str >> decl;
		str >> ch;
		str >> value;

		if(decl=="")
		{
			WriteLog("Global var name not found, pragma<%s>.\n",instance.text.c_str());
			return;
		}

		int int_value=(ch=='='?atoi(value.c_str()):0);
		double float_value=(ch=='='?atof(value.c_str()):0.0);
		string name=type+" "+decl;

		// Check for exists
		if(addedVars.count(name)) return;

		// Register
		if(type=="int8" || type=="int16" || type=="int32" || type=="int" || type=="uint8" || type=="uint16" || type=="uint32" || type=="uint")
		{
			list<int>::iterator it=intArray.insert(intArray.begin(),int_value);
			if(Engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) WriteLog("Unable to register integer global var, pragma<%s>.\n",instance.text.c_str());
		}
		else if(type=="int64" || type=="uint64")
		{
			list<__int64>::iterator it=int64Array.insert(int64Array.begin(),int_value);
			if(Engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) WriteLog("Unable to register integer64 global var, pragma<%s>.\n",instance.text.c_str());
		}
		else if(type=="string")
		{
			if(value!="") value=instance.text.substr(instance.text.find(value),string::npos);
			list<CScriptString*>::iterator it=stringArray.insert(stringArray.begin(),new CScriptString(value));
			if(Engine->RegisterGlobalProperty(name.c_str(),(*it))<0) WriteLog("Unable to register string global var, pragma<%s>.\n",instance.text.c_str());
		}
		else if(type=="float")
		{
			list<float>::iterator it=floatArray.insert(floatArray.begin(),(float)float_value);
			if(Engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) WriteLog("Unable to register float global var, pragma<%s>.\n",instance.text.c_str());
		}
		else if(type=="double")
		{
			list<double>::iterator it=doubleArray.insert(doubleArray.begin(),float_value);
			if(Engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) WriteLog("Unable to register double global var, pragma<%s>.\n",instance.text.c_str());
		}
		else if(type=="bool")
		{
			value=(ch=='='?value:"false");
			if(value!="true" && value!="false")
			{
				WriteLog("Invalid start value of boolean type, pragma<%s>.\n",instance.text.c_str());
				return;
			}
			list<char>::iterator it=boolArray.insert(boolArray.begin(),value=="true"?true:false);
			if(Engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) WriteLog("Unable to register boolean global var, pragma<%s>.\n",instance.text.c_str());
		}
		else
		{
			WriteLog("Global var not registered, unknown type, pragma<%s>.\n",instance.text.c_str());
		}
		addedVars.insert(name);
	}
};

// #pragma crdata "Stat 0 199"
class CrDataPragmaCallback : public Preprocessor::PragmaCallback
{
public:
	PragmaCallbackFunc CallFunc;

	CrDataPragmaCallback(PragmaCallbackFunc call_func):CallFunc(call_func){}

	void pragma(const Preprocessor::PragmaInstance& instance)
	{
		if(CallFunc && !(*CallFunc)(instance.text.c_str())) WriteLog("Unable to parse crdata pragma<%s>.\n",instance.text.c_str());
	}
};

// #pragma bindfunc "int MyFunc(int, uint) -> my.dll MyDllFunc"
// #pragma bindfunc "int MyObject::MyMethod(int, uint) -> my.dll MyDllFunc"
class BindFuncPragmaCallback : public Preprocessor::PragmaCallback
{
private:
	set<string> alreadyProcessed;

public:
	void pragma(const Preprocessor::PragmaInstance& instance)
	{
		if(alreadyProcessed.count(instance.text)) return;
		alreadyProcessed.insert(instance.text);

		string func_name,dll_name,func_dll_name;
		istrstream str(instance.text.c_str());
		while(true)
		{
			string s;
			str >> s;
			if(str.fail() || s=="->") break;
			if(func_name!="") func_name+=" ";
			func_name+=s;
		}
		str >> dll_name;
		str >> func_dll_name;

		if(str.fail())
		{
			WriteLog("Error in bindfunc pragma<%s>, parse fail.\n",instance.text.c_str());
			return;
		}

		HMODULE dll=LoadDynamicLibrary(dll_name.c_str());
		if(!dll)
		{
			WriteLog("Error in bindfunc pragma<%s>, dll not found, error<%u>.\n",instance.text.c_str(),GetLastError());
			return;
		}

		// Find function
		FARPROC func=GetProcAddress(dll,func_dll_name.c_str());
		if(!func)
		{
			WriteLog("Error in bindfunc pragma<%s>, function not found, error<%u>.\n",instance.text.c_str(),GetLastError());
			return;
		}

		int result=0;
		if(func_name.find("::")==string::npos)
		{
			// Register global function
			result=Engine->RegisterGlobalFunction(func_name.c_str(),asFUNCTION(func),asCALL_CDECL);
		}
		else
		{
			// Register class method
			string::size_type i=func_name.find(" ");
			string::size_type j=func_name.find("::");
			if(i==string::npos || i+1>=j)
			{
				WriteLog("Error in bindfunc pragma<%s>, parse class name fail.\n",instance.text.c_str());
				return;
			}
			i++;
			string class_name;
			class_name.assign(func_name,i,j-i);
			func_name.erase(i,j-i+2);
			result=Engine->RegisterObjectMethod(class_name.c_str(),func_name.c_str(),asFUNCTION(func),asCALL_CDECL_OBJFIRST);
		}
		if(result<0) WriteLog("Error in bindfunc pragma<%s>, script registration fail, error<%d>.\n",instance.text.c_str(),result);
	}
};

bool Init(bool with_log, PragmaCallbackFunc crdata)
{
	if(with_log && !StartLog())
	{
		WriteLog(__FUNCTION__" - Log creation error.\n");
		return false;
	}

	// Create default engine
	Engine=CreateEngine(crdata);
	if(!Engine)
	{
		WriteLog(__FUNCTION__" - Can't create AS engine.\n");
		return false;
	}

	BindedFunctions.clear();
	BindedFunctions.reserve(10000);
	BindedFunctions.push_back(BindFunction(0,0,"","","")); // None
	BindedFunctions.push_back(BindFunction(0,0,"","","")); // Temp

	if(!InitThread()) return false;

	RunTimeoutThreadHandle=(HANDLE)_beginthreadex(NULL,0,RunTimeoutThread,NULL,0,NULL);
	return true;
}

void Finish()
{
	if(!Engine) return;

	EndLog();
	RunTimeoutSuspend=0;
	RunTimeoutMessage=0;
	WaitForSingleObject(RunTimeoutThreadHandle,INFINITE);
	CloseHandle(RunTimeoutThreadHandle);
	RunTimeoutThreadHandle=NULL;

	BindedFunctions.clear();
	Preprocessor::ClearPragmas();
	Preprocessor::UndefAll();
	UnloadScripts();

	FinishEngine(Engine); // Finish default engine

	FinisthThread();
}

bool InitThread()
{
	for(int i=0;i<GLOBAL_CONTEXT_STACK_SIZE;i++)
	{
		GlobalCtx[i]=CreateContext();
		if(!GlobalCtx[i])
		{
			WriteLog(__FUNCTION__" - Create global contexts fail.\n");
			Engine->Release();
			Engine=NULL;
			return false;
		}
	}

	return true;
}

void FinisthThread()
{
	ActiveGlobalCtxLocker.Lock();
	ActiveContextVecIt it=std::find(ActiveContexts.begin(),ActiveContexts.end(),(asIScriptContext**)GlobalCtx);
	if(it!=ActiveContexts.end()) ActiveContexts.erase(it);
	ActiveGlobalCtxLocker.Unlock();

	for(int i=0;i<GLOBAL_CONTEXT_STACK_SIZE;i++)
		FinishContext(GlobalCtx[i]);

	asThreadCleanup();
}

HMODULE LoadDynamicLibrary(const char* dll_name)
{
	// Load dynamic library
	char dll_name_[256];
	StringCopy(dll_name_,FileManager::GetFullPath("",ScriptsPath));
	StringAppend(dll_name_,dll_name);
	HMODULE dll=LoadLibrary(dll_name_);
	if(!dll) return NULL;

	// Register global function and vars
	static StrSet alreadyLoadedDll;
	if(!alreadyLoadedDll.count(dll_name_))
	{
		// Register variables
		size_t* ptr=(size_t*)GetProcAddress(dll,"Game");
		if(ptr) *ptr=(size_t)&GameOpt;
		ptr=(size_t*)GetProcAddress(dll,"ASEngine");
		if(ptr) *ptr=(size_t)Engine;

		// Register functions
		ptr=(size_t*)GetProcAddress(dll,"Log");
		if(ptr) *ptr=(size_t)&WriteLog;

		// Call init function
		typedef void(*DllMainEx)(bool);
		DllMainEx func=(DllMainEx)GetProcAddress(dll,"DllMainEx");
		if(func) (*func)(LoadLibraryCompiler);

		alreadyLoadedDll.insert(dll_name_);
	}
	return dll;
}

void SetWrongGlobalObjects(StrVec& names)
{
	WrongGlobalObjects=names;
}

void SetConcurrentExecution(bool enabled)
{
	ConcurrentExecution=enabled;
}

void SetLoadLibraryCompiler(bool enabled)
{
	LoadLibraryCompiler=enabled;
}

void UnloadScripts()
{
	EngineData* edata=(EngineData*)Engine->GetUserData();
	ScriptModuleVec& modules=edata->Modules;

	for(ScriptModuleVecIt it=modules.begin(),end=modules.end();it!=end;++it)
	{
		asIScriptModule* module=*it;
		int result=module->ResetGlobalVars();
		if(result<0) WriteLog(__FUNCTION__" - Reset global vars fail, module<%s>, error<%d>.\n",module->GetName(),result);
		result=module->UnbindAllImportedFunctions();
		if(result<0) WriteLog(__FUNCTION__" - Unbind fail, module<%s>, error<%d>.\n",module->GetName(),result);
	}

	for(ScriptModuleVecIt it=modules.begin(),end=modules.end();it!=end;++it)
	{
		asIScriptModule* module=*it;
		Engine->DiscardModule(module->GetName());
	}

	modules.clear();
	CollectGarbage();
}

bool ReloadScripts(const char* config, const char* key, bool skip_binaries, const char* file_pefix /* = NULL */)
{
	WriteLog("Reload scripts...\n");

	Script::UnloadScripts();

	int errors=0;
	char buf[1024];
	string value;

	istrstream config_(config);
	while(!config_.eof())
	{
		config_.getline(buf,1024);
		if(buf[0]!='@') continue;

		istrstream str(&buf[1]);
		str >> value;
		if(str.fail() || value!=key) continue;
		str >> value;
		if(str.fail() || value!="module") continue;

		str >> value;
		if(str.fail() || !LoadScript(value.c_str(),NULL,skip_binaries,file_pefix))
		{
			WriteLog("Load module fail, name<%s>.\n",value.c_str());
			errors++;
		}
	}

	errors+=BindImportedFunctions();
	errors+=RebindFunctions();

	if(errors)
	{
		WriteLog("Reload scripts fail.\n");
		return false;
	}

	WriteLog("Reload scripts complete.\n");
	return true;
}

bool BindReservedFunctions(const char* config, const char* key, ReservedScriptFunction* bind_func, DWORD bind_func_count)
{
	WriteLog("Bind reserved functions...\n");

	int errors=0;
	char buf[1024];
	string value;
	for(DWORD i=0;i<bind_func_count;i++)
	{
		ReservedScriptFunction* bf=&bind_func[i];
		int bind_id=0;

		istrstream config_(config);
		while(!config_.eof())
		{
			config_.getline(buf,1024);
			if(buf[0]!='@') continue;

			istrstream str(&buf[1]);
			str >> value;
			if(str.fail() || key!=value) continue;
			str >> value;
			if(str.fail() || value!="bind") continue;
			str >> value;
			if(str.fail() || value!=bf->FuncName) continue;

			str >> value;
			if(!str.fail()) bind_id=Bind(value.c_str(),bf->FuncName,bf->FuncDecl,false);
			break;
		}

		if(bind_id>0)
		{
			*bf->BindId=bind_id;
		}
		else
		{
			WriteLog("Bind reserved function fail, name<%s>.\n",bf->FuncName);
			errors++;
		}
	}

	if(errors)
	{
		WriteLog("Bind reserved functions fail.\n");
		return false;
	}

	WriteLog("Bind reserved functions complete.\n");
	return true;
}

void AddRef(void*)
{
	// Dummy
}

void Release(void*)
{
	// Dummy
}

asIScriptEngine* GetEngine()
{
	return Engine;
}

void SetEngine(asIScriptEngine* engine)
{
	Engine=engine;
}

asIScriptEngine* CreateEngine(PragmaCallbackFunc crdata)
{
	asIScriptEngine* engine=asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if(!engine)
	{
		WriteLog(__FUNCTION__" - asCreateScriptEngine fail.\n");
		return false;
	}

	engine->SetMessageCallback(asFUNCTION(CallbackMessage),NULL,asCALL_CDECL);
	RegisterScriptArray(engine,true);
	RegisterScriptString(engine);
	RegisterScriptStringUtils(engine);
	RegisterScriptAny(engine);
	RegisterScriptDictionary(engine);
	RegisterScriptFile(engine);
	RegisterScriptMath(engine);
	RegisterScriptMath3D(engine);

	EngineData* edata=new EngineData();
	edata->PrGlobalVar=new GvarPragmaCallback();
	edata->PrCrData=new CrDataPragmaCallback(crdata);
	edata->PrBindFunc=new BindFuncPragmaCallback();
	engine->SetUserData(edata);
	return engine;
}

void FinishEngine(asIScriptEngine*& engine)
{
	if(engine)
	{
		EngineData* edata=(EngineData*)engine->SetUserData(NULL);
		delete edata->PrGlobalVar;
		delete edata->PrCrData;
		delete edata->PrBindFunc;
		delete edata;
		engine->Release();
		engine=NULL;
	}
}

asIScriptContext* CreateContext()
{
	asIScriptContext* ctx=Engine->CreateContext();
	if(!ctx)
	{
		WriteLog(__FUNCTION__" - CreateContext fail.\n");
		return NULL;
	}

	if(ctx->SetExceptionCallback(asFUNCTION(CallbackException),NULL,asCALL_CDECL)<0)
	{
		WriteLog(__FUNCTION__" - SetExceptionCallback to fail.\n");
		ctx->Release();
		return NULL;
	}

	char* buf=new char[CONTEXT_BUFFER_SIZE];
	if(!buf)
	{
		WriteLog(__FUNCTION__" - Allocate memory for buffer fail.\n");
		ctx->Release();
		return NULL;
	}

	StringCopy(buf,CONTEXT_BUFFER_SIZE,"<error>");
	ctx->SetUserData(buf);
	return ctx;
}

void FinishContext(asIScriptContext*& ctx)
{
	if(ctx)
	{
		char* buf=(char*)ctx->GetUserData();
		if(buf) delete[] buf;
		ctx->Release();
		ctx=NULL;
	}
}

asIScriptContext* GetGlobalContext()
{
	if(GlobalCtxIndex>=GLOBAL_CONTEXT_STACK_SIZE)
	{
		WriteLog(__FUNCTION__" - Script context stack overflow! Context call stack:\n");
		for(int i=GLOBAL_CONTEXT_STACK_SIZE-1;i>=0;i--) WriteLog("  %d) %s.\n",i,GlobalCtx[i]->GetUserData());
		return NULL;
	}
	GlobalCtxIndex++;
	return GlobalCtx[GlobalCtxIndex-1];
}

void PrintContextCallstack(asIScriptContext* ctx)
{
	int line,column;
	const asIScriptFunction* func;
	int stack_size=ctx->GetCallstackSize();
	WriteLog("Context<%s>, state<%s>, call stack<%d>:\n",ctx->GetUserData(),ContextStatesStr[(int)ctx->GetState()],stack_size);

	// Print current function
	if(ctx->GetState()==asEXECUTION_EXCEPTION)
	{
		line=ctx->GetExceptionLineNumber(&column);
		func=Engine->GetFunctionDescriptorById(ctx->GetExceptionFunction());
	}
	else
	{
		line=ctx->GetLineNumber(0,&column);
		func=ctx->GetFunction(0);
	}
	if(func) WriteLog("  %d) %s : %s : %d, %d.\n",stack_size-1,func->GetModuleName(),func->GetDeclaration(),line,column);

	// Print call stack
	for(int i=1;i<stack_size;i++)
	{
		func=ctx->GetFunction(i);
		line=ctx->GetLineNumber(i,&column);
		if(func) WriteLog("  %d) %s : %s : %d, %d.\n",stack_size-i-1,func->GetModuleName(),func->GetDeclaration(),line,column);
	}
}

const char* GetActiveModuleName()
{
	static const char error[]="<error>";
	asIScriptContext* ctx=asGetActiveContext();
	if(!ctx) return error;
	asIScriptFunction* func=ctx->GetFunction(0);
	if(!func) return error;
	return func->GetModuleName();
}

const char* GetActiveFuncName()
{
	static const char error[]="<error>";
	asIScriptContext* ctx=asGetActiveContext();
	if(!ctx) return error;
	asIScriptFunction* func=ctx->GetFunction(0);
	if(!func) return error;
	return func->GetName();
}

asIScriptModule* GetModule(const char* name)
{
	return Engine->GetModule(name,asGM_ONLY_IF_EXISTS);
}

asIScriptModule* CreateModule(const char* module_name)
{
	// Delete old
	EngineData* edata=(EngineData*)Engine->GetUserData();
	ScriptModuleVec& modules=edata->Modules;
	for(ScriptModuleVecIt it=modules.begin(),end=modules.end();it!=end;++it)
	{
		asIScriptModule* module=*it;
		if(!strcmp(module->GetName(),module_name))
		{
			modules.erase(it);
			break;
		}
	}

	// Create new
	asIScriptModule* module=Engine->GetModule(module_name,asGM_ALWAYS_CREATE);
	if(!module) return NULL;
	modules.push_back(module);
	return module;
}

#ifndef FONLINE_SERVER
void SetGarbageCollectTime(DWORD ticks)
{
	GarbageCollectTime=ticks;
}
#endif

#ifdef FONLINE_SERVER
DWORD GetGCStatistics()
{
	asUINT current_size=0;
	Engine->GetGCStatistics(&current_size);
	return (DWORD)current_size;
}

void ScriptGarbager()
{
	static BYTE garbager_state=5;
	static DWORD last_nongarbage=0;
	static DWORD best_count=0;
	static DWORD last_garbager_tick=0;
	static DWORD last_evaluation_tick=0;

	static DWORD garbager_count[3]; // Uses garbager_state as index
	static double garbager_time[3]; // Uses garbager_state as index

	switch(garbager_state)
	{
	case 5: // First time
		{
			best_count=1000;
		}
	case 6: // Statistics init and first time
		{
			CollectGarbage(asGC_FULL_CYCLE|asGC_DESTROY_GARBAGE);

			last_nongarbage=GetGCStatistics();
			garbager_state=0;
		}
		break;
	case 0: // Statistics stage 0, 1, 2
	case 1:
	case 2:
		{
			DWORD current_size=GetGCStatistics();

			// Try 1x, 1.5x and 2x count time for time extrapolation
			if(current_size<last_nongarbage+(best_count*(2+garbager_state))/2) break;

			garbager_time[garbager_state]=Timer::AccurateTick();
			CollectGarbage(asGC_FULL_CYCLE|asGC_DESTROY_GARBAGE);
			garbager_time[garbager_state]=Timer::AccurateTick()-garbager_time[garbager_state];

			last_nongarbage=GetGCStatistics();
			garbager_count[garbager_state]=current_size-last_nongarbage;

			if(!garbager_count[garbager_state]) break; // Repeat this step
			garbager_state++;
		}
		break;
	case 3: // Statistics last stage, calculate best count
		{
			double obj_times[2];
			bool undetermined=false;
			for(int i=0;i<2;i++)
			{
				if(garbager_count[i+1]==garbager_count[i])
				{
					undetermined=true; // Too low resolution, break statistics and repeat later
					break;
				}

				obj_times[i]=(garbager_time[i+1]-garbager_time[i])/((double)garbager_count[i+1]-(double)garbager_count[i]);

				if(obj_times[i]<=0.0f) // Should no happen
				{
					undetermined=true; // Too low resolution, break statistics and repeat later
					break;
				}
			}
			garbager_state=4;
			if(undetermined) break;

			double object_delete_time=(obj_times[0]+obj_times[1])/2;
			double overhead=0.0f;
			for(int i=0;i<3;i++) overhead+=(garbager_time[i]-garbager_count[i]*object_delete_time);
			overhead/=3;
			if(overhead>MaxGarbagerTime) overhead=MaxGarbagerTime; // Will result on deletion on every frame
			best_count=(DWORD)((MaxGarbagerTime-overhead)/object_delete_time);
		}
		break;
	case 4: // Normal garbage check
		{
			if(GarbagerCycle && Timer::FastTick()-last_garbager_tick>=GarbagerCycle)
			{
				CollectGarbage(asGC_ONE_STEP|asGC_DETECT_GARBAGE);
				last_garbager_tick=Timer::FastTick();
			}

			if(EvaluationCycle && Timer::FastTick()-last_evaluation_tick>=EvaluationCycle)
			{
				garbager_state=6; // Enter statistics after normal garbaging is done
				last_evaluation_tick=Timer::FastTick();
			}

			if(GetGCStatistics()>last_nongarbage+best_count)
			{
				CollectGarbage(asGC_FULL_CYCLE|asGC_DESTROY_GARBAGE);
			}
		}
		break;
	default:
		break;
	}
}

void CollectGarbage(asDWORD flags)
{
#ifdef SCRIPT_MULTITHREADING
	if(LogicMT) GarbageLocker.LockCode();
	Engine->GarbageCollect(flags);
	if(LogicMT) GarbageLocker.UnlockCode();
#else
	Engine->GarbageCollect(flags);
#endif
}
#endif

#ifndef FONLINE_SERVER
void CollectGarbage(bool force)
{
	static DWORD last_tick=Timer::FastTick();
	if(force || (GarbageCollectTime && Timer::FastTick()-last_tick>=GarbageCollectTime))
	{
#ifdef SCRIPT_MULTITHREADING
		if(LogicMT) GarbageLocker.LockCode();
		Engine->GarbageCollect(asGC_FULL_CYCLE);
		if(LogicMT) GarbageLocker.UnlockCode();
#else
		Engine->GarbageCollect(asGC_FULL_CYCLE);
#endif

		last_tick=Timer::FastTick();
	}
}
#endif

unsigned int __stdcall RunTimeoutThread(void* data)
{
	LogSetThreadName("ScriptTimeout");

	while(RunTimeoutSuspend)
	{
		// Check execution time every 1/10 second
		Sleep(100);

		SCOPE_LOCK(ActiveGlobalCtxLocker);

		DWORD cur_tick=Timer::FastTick();
		for(ActiveContextVecIt it=ActiveContexts.begin(),end=ActiveContexts.end();it!=end;++it)
		{
			ActiveContext& actx=*it;
			if(cur_tick>=actx.StartTick+RunTimeoutSuspend)
			{
				// Suspend all contexts
				for(int i=GLOBAL_CONTEXT_STACK_SIZE-1;i>=0;i--)
					if(actx.Contexts[i]->GetState()==asEXECUTION_ACTIVE) actx.Contexts[i]->Suspend();

				// Erase from collection
				ActiveContexts.erase(it);
				break;
			}
		}
	}
	return 0;
}

void SetRunTimeout(DWORD suspend_timeout, DWORD message_timeout)
{
	RunTimeoutSuspend=suspend_timeout;
	RunTimeoutMessage=message_timeout;
}

/************************************************************************/
/* Load / Bind                                                          */
/************************************************************************/

void SetScriptsPath(int path_type)
{
	ScriptsPath=path_type;
}

void Define(const char* def)
{
	Preprocessor::Define(def);
}

void Undefine(const char* def)
{
	if(def) Preprocessor::Undefine(def);
	else Preprocessor::UndefAll();
}

char* Preprocess(const char* fname, bool process_pragmas)
{
	// Prepare preprocessor
	Preprocessor::VectorOutStream vos;
	Preprocessor::FileSource fsrc;
	fsrc.CurrentDir=FileManager::GetFullPath("",ScriptsPath);

	// Set current pragmas
	EngineData* edata=(EngineData*)Engine->GetUserData();
	Preprocessor::RegisterPragma("globalvar",edata->PrGlobalVar);
	Preprocessor::RegisterPragma("crdata",edata->PrCrData);
	Preprocessor::RegisterPragma("bindfunc",edata->PrBindFunc);

	// Preprocess
	if(Preprocessor::Preprocess(fname,fsrc,vos,process_pragmas))
	{
		WriteLog(__FUNCTION__" - Unable to preprocess<%s> script.\n",fname);
		return NULL;
	}

	// Store result
	char* result=new char[vos.GetSize()+1];
	memcpy(result,vos.GetData(),vos.GetSize());
	result[vos.GetSize()]=0;
	return result;
}

void CallPragmas(const StrVec& pragmas)
{
	EngineData* edata=(EngineData*)Engine->GetUserData();

	// Set current pragmas
	Preprocessor::RegisterPragma("globalvar",edata->PrGlobalVar);
	Preprocessor::RegisterPragma("crdata",edata->PrCrData);
	Preprocessor::RegisterPragma("bindfunc",edata->PrBindFunc);

	// Call pragmas
	for(size_t i=0,j=pragmas.size()/2;i<j;i++)
	{
		Preprocessor::PragmaInstance pi;
		pi.text=pragmas[i*2+1];
		pi.current_file="";
		pi.current_file_line=0;
		pi.root_file="";
		pi.global_line=0;
		Preprocessor::CallPragma(pragmas[i*2],pi);
	}
}

bool LoadScript(const char* module_name, const char* source, bool skip_binary, const char* file_pefix /* = NULL */)
{
	EngineData* edata=(EngineData*)Engine->GetUserData();
	ScriptModuleVec& modules=edata->Modules;

	// Compute whole version for server, client, mapper
	int version=(SERVER_VERSION<<20)|(CLIENT_VERSION<<10)|MAPPER_VERSION;

	// Get script names
	char fname_real[MAX_FOPATH]={0};
	StringAppend(fname_real,module_name);
	StringAppend(fname_real,".fos");

	char fname_script[MAX_FOPATH]={0};
	if(file_pefix) StringCopy(fname_script,file_pefix);
	StringAppend(fname_script,module_name);
	Str::Replacement(fname_script,'.','\\');
	StringAppend(fname_script,".fos");

	// Set current pragmas
	Preprocessor::RegisterPragma("globalvar",edata->PrGlobalVar);
	Preprocessor::RegisterPragma("crdata",edata->PrCrData);
	Preprocessor::RegisterPragma("bindfunc",edata->PrBindFunc);

	// Try load precompiled script
	FileManager file_bin;
	if(!skip_binary)
	{
		FileManager file;
		file.LoadFile(fname_real,ScriptsPath);
		file_bin.LoadFile(Str::Format("%sb",fname_script),ScriptsPath);

		if(file_bin.IsLoaded() && file_bin.GetFsize()>sizeof(DWORD))
		{
			// Load file dependencies and pragmas
			char str[1024];
			DWORD bin_version=file_bin.GetBEDWord();
			DWORD dependencies_size=file_bin.GetBEDWord();
			StrVec dependencies;
			for(DWORD i=0;i<dependencies_size;i++)
			{
				file_bin.GetStr(str);
				dependencies.push_back(str);
			}
			DWORD pragmas_size=file_bin.GetBEDWord();
			StrVec pragmas;
			for(DWORD i=0;i<pragmas_size;i++)
			{
				file_bin.GetStr(str);
				pragmas.push_back(str);
			}

			// Check for outdated
			FILETIME last_write,last_write_bin;
			file_bin.GetTime(NULL,NULL,&last_write_bin);
			// Main file
			file.GetTime(NULL,NULL,&last_write);
			bool no_all_files=!file.IsLoaded();
			bool outdated=(file.IsLoaded() && CompareFileTime(&last_write,&last_write_bin)>0);
			// Include files
			for(size_t i=0,j=dependencies.size();i<j;i++)
			{
				FileManager file_dep;
				file_dep.LoadFile(dependencies[i].c_str(),ScriptsPath);
				file_dep.GetTime(NULL,NULL,&last_write);
				if(!no_all_files) no_all_files=!file_dep.IsLoaded();
				if(!outdated) outdated=(file_dep.IsLoaded() && CompareFileTime(&last_write,&last_write_bin)>0);
			}

			if(no_all_files || (!outdated && bin_version==version))
			{
				if(bin_version!=version) WriteLog(__FUNCTION__" - Script<%s> compiled in older server version.\n",module_name);
				if(outdated) WriteLog(__FUNCTION__" - Script<%s> outdated.\n",module_name);

				// Delete old
				for(ScriptModuleVecIt it=modules.begin(),end=modules.end();it!=end;++it)
				{
					asIScriptModule* module=*it;
					if(!strcmp(module->GetName(),module_name))
					{
						WriteLog(__FUNCTION__" - Warning, script for this name<%s> already exist. Discard it.\n",module_name);
						Engine->DiscardModule(module_name);
						modules.erase(it);
						break;
					}
				}

				asIScriptModule* module=Engine->GetModule(module_name,asGM_ALWAYS_CREATE);
				if(module)
				{
					for(size_t i=0,j=pragmas.size()/2;i<j;i++)
					{
						Preprocessor::PragmaInstance pi;
						pi.text=pragmas[i*2+1];
						pi.current_file="";
						pi.current_file_line=0;
						pi.root_file="";
						pi.global_line=0;
						Preprocessor::CallPragma(pragmas[i*2],pi);
					}

					CBytecodeStream binary;
					binary.Write(file_bin.GetCurBuf(),file_bin.GetFsize()-file_bin.GetCurPos());

					if(module->LoadByteCode(&binary)>=0)
					{
						StrVec& pragmas_=Preprocessor::GetParsedPragmas();
						pragmas_=pragmas;
						modules.push_back(module);
						return true;
					}
					else WriteLog(__FUNCTION__" - Can't load binary, script<%s>.\n",module_name);
				}
				else WriteLog(__FUNCTION__" - Create module fail, script<%s>.\n",module_name);
			}
		}

		if(!file.IsLoaded())
		{
			WriteLog(__FUNCTION__" - Script<%s> not found.\n",fname_real);
			return false;
		}

		file_bin.UnloadFile();
	}

	Preprocessor::VectorOutStream vos;
	Preprocessor::FileSource fsrc;
	Preprocessor::VectorOutStream vos_err;
	if(source) fsrc.Stream=source;
	fsrc.CurrentDir=FileManager::GetFullPath("",ScriptsPath);

	if(Preprocessor::Preprocess(fname_real,fsrc,vos,true,vos_err))
	{
		vos_err.PushNull();
		WriteLog(__FUNCTION__" - Unable to preprocess file<%s>, error<%s>.\n",fname_real,vos_err.GetData());
		return false;
	}
	vos.Format();

	// Delete old
	for(ScriptModuleVecIt it=modules.begin(),end=modules.end();it!=end;++it)
	{
		asIScriptModule* module=*it;
		if(!strcmp(module->GetName(),module_name))
		{
			WriteLog(__FUNCTION__" - Warning, script for this name<%s> already exist. Discard it.\n",module_name);
			Engine->DiscardModule(module_name);
			modules.erase(it);
			break;
		}
	}

	asIScriptModule* module=Engine->GetModule(module_name,asGM_ALWAYS_CREATE);
	if(!module)
	{
		WriteLog(__FUNCTION__" - Create module fail, script<%s>.\n",module_name);
		return false;
	}

	if(module->AddScriptSection(module_name,vos.GetData(),vos.GetSize(),0)<0)
	{
		WriteLog(__FUNCTION__" - Unable to AddScriptSection module<%s>.\n",module_name);
		return false;
	}

	if(module->Build()<0)
	{
		WriteLog(__FUNCTION__" - Unable to Build module<%s>.\n",module_name);
		return false;
	}

	// Check not allowed global variables
	if(WrongGlobalObjects.size())
	{
		IntVec bad_typeids;
		bad_typeids.reserve(WrongGlobalObjects.size());
		for(StrVecIt it=WrongGlobalObjects.begin(),end=WrongGlobalObjects.end();it!=end;++it)
			bad_typeids.push_back(Engine->GetTypeIdByDecl((*it).c_str())&asTYPEID_MASK_SEQNBR);

		IntVec bad_typeids_class;
		for(int m=0,n=module->GetObjectTypeCount();m<n;m++)
		{
			asIObjectType* ot=module->GetObjectTypeByIndex(m);
			for(int i=0,j=ot->GetPropertyCount();i<j;i++)
			{
				int type=0;
				ot->GetProperty(i,NULL,&type,NULL,NULL);
				type&=asTYPEID_MASK_SEQNBR;
				for(int k=0;k<bad_typeids.size();k++)
				{
					if(type==bad_typeids[k])
					{
						bad_typeids_class.push_back(ot->GetTypeId()&asTYPEID_MASK_SEQNBR);
						break;
					}
				}
			}
		}

		bool global_fail=false;
		for(int i=0,j=module->GetGlobalVarCount();i<j;i++)
		{
			int type=0;
			module->GetGlobalVar(i,NULL,&type,NULL);

			while(type&asTYPEID_TEMPLATE)
			{
				asIObjectType* obj=(asIObjectType*)Engine->GetObjectTypeById(type);
				if(!obj) break;
				type=obj->GetSubTypeId();
				obj->Release();
			}

			type&=asTYPEID_MASK_SEQNBR;

			for(int k=0;k<bad_typeids.size();k++)
			{
				if(type==bad_typeids[k])
				{
					const char* name=NULL;
					module->GetGlobalVar(i,&name,NULL,NULL);
					string msg="The global variable '"+string(name)+"' uses a type that cannot be stored globally";
					Engine->WriteMessage("",0,0,asMSGTYPE_ERROR,msg.c_str());
					global_fail=true;
					break;
				}
			}
			if(std::find(bad_typeids_class.begin(),bad_typeids_class.end(),type)!=bad_typeids_class.end())
			{
				const char* name=NULL;
				module->GetGlobalVar(i,&name,NULL,NULL);
				string msg="The global variable '"+string(name)+"' uses a type in class property that cannot be stored globally";
				Engine->WriteMessage("",0,0,asMSGTYPE_ERROR,msg.c_str());
				global_fail=true;
			}
		}

		if(global_fail)
		{
			WriteLog(__FUNCTION__" - Wrong global variables in module<%s>.\n",module_name);
			return false;
		}
	}

	// Done, add to collection
	modules.push_back(module);

	// Save binary version of script and preprocessed version
	if(!skip_binary && !file_bin.IsLoaded())
	{
		CBytecodeStream binary;
		if(module->SaveByteCode(&binary)>=0)
		{
			std::vector<asBYTE>& data=binary.GetBuf();
			StrVec& dependencies=Preprocessor::GetFileDependencies();
			StrVec& pragmas=Preprocessor::GetParsedPragmas();

			file_bin.SetBEDWord(version);
			file_bin.SetBEDWord(dependencies.size());
			for(size_t i=0,j=dependencies.size();i<j;i++) file_bin.SetData((BYTE*)dependencies[i].c_str(),dependencies[i].length()+1);
			file_bin.SetBEDWord(pragmas.size());
			for(size_t i=0,j=pragmas.size();i<j;i++) file_bin.SetData((BYTE*)pragmas[i].c_str(),pragmas[i].length()+1);
			file_bin.SetData(&data[0],data.size());

			if(!file_bin.SaveOutBufToFile(Str::Format("%sb",fname_script),ScriptsPath)) WriteLog(__FUNCTION__" - Can't save bytecode, script<%s>.\n",module_name);
		}
		else
		{
			WriteLog(__FUNCTION__" - Can't write bytecode, script<%s>.\n",module_name);
		}

		FileManager file_prep;
		file_prep.SetData((void*)vos.GetData(),vos.GetSize());
		if(!file_prep.SaveOutBufToFile(Str::Format("%sp",fname_script),ScriptsPath))
			WriteLog(__FUNCTION__" - Can't write preprocessed file, script<%s>.\n",module_name);
	}
	return true;
}

bool LoadScript(const char* module_name, const BYTE* bytecode, DWORD len)
{
	if(!bytecode || !len)
	{
		WriteLog(__FUNCTION__" - Bytecode empty, module name<%s>.\n",module_name);
		return false;
	}

	EngineData* edata=(EngineData*)Engine->GetUserData();
	ScriptModuleVec& modules=edata->Modules;

	for(ScriptModuleVecIt it=modules.begin(),end=modules.end();it!=end;++it)
	{
		asIScriptModule* module=*it;
		if(!strcmp(module->GetName(),module_name))
		{
			WriteLog(__FUNCTION__" - Warning, script for this name<%s> already exist. Discard it.\n",module_name);
			Engine->DiscardModule(module_name);
			modules.erase(it);
			break;
		}
	}

	asIScriptModule* module=Engine->GetModule(module_name,asGM_ALWAYS_CREATE);
	if(!module)
	{
		WriteLog(__FUNCTION__" - Create module fail, script<%s>.\n",module_name);
		return false;
	}

	CBytecodeStream binary;
	binary.Write(bytecode,len);
	int result=module->LoadByteCode(&binary);
	if(result<0)
	{
		WriteLog(__FUNCTION__" - Can't load binary, module<%s>, result<%d>.\n",module_name,result);
		return false;
	}

	modules.push_back(module);
	return true;
}

int BindImportedFunctions()
{
	//WriteLog("Bind all imported functions...\n");

	EngineData* edata=(EngineData*)Engine->GetUserData();
	ScriptModuleVec& modules=edata->Modules;
	int errors=0;
	for(ScriptModuleVecIt it=modules.begin(),end=modules.end();it!=end;++it)
	{
		asIScriptModule* module=*it;
		int result=module->BindAllImportedFunctions();
		if(result<0)
		{
			WriteLog(__FUNCTION__" - Fail to bind imported functions, module<%s>, error<%d>.\n",module->GetName(),result);
			errors++;
			continue;
		}
	}

	//WriteLog("Bind all imported functions complete.\n");
	return errors;
}

int Bind(const char* module_name, const char* func_name, const char* decl, bool is_temp, bool disable_log /* = false */)
{
#ifdef SCRIPT_MULTITHREADING
	SCOPE_LOCK(BindedFunctionsLocker);
#endif

	// Detect native dll
	if(!strstr(module_name,".dll"))
	{
		// Find module
		asIScriptModule* module=Engine->GetModule(module_name,asGM_ONLY_IF_EXISTS);
		if(!module)
		{
			if(!disable_log) WriteLog(__FUNCTION__" - Module<%s> not found.\n",module_name);
			return 0;
		}

		// Find function
		char decl_[256];
		sprintf(decl_,decl,func_name);
		int result=module->GetFunctionIdByDecl(decl_);
		if(result<=0)
		{
			if(!disable_log) WriteLog(__FUNCTION__" - Function<%s> in module<%s> not found, result<%d>.\n",decl_,module_name,result);
			return 0;
		}

		// Save to temporary bind
		if(is_temp)
		{
			BindedFunctions[1].IsScriptCall=true;
			BindedFunctions[1].ScriptFuncId=result;
			BindedFunctions[1].NativeFuncAddr=0;
			return 1;
		}

		// Find already binded
		for(int i=2,j=(int)BindedFunctions.size();i<j;i++)
		{
			BindFunction& bf=BindedFunctions[i];
			if(bf.IsScriptCall && bf.ScriptFuncId==result) return i;
		}

		// Create new bind
		BindedFunctions.push_back(BindFunction(result,0,module_name,func_name,decl));
	}
	else
	{
		// Load dynamic library
		HMODULE dll=LoadDynamicLibrary(module_name);
		if(!dll)
		{
			if(!disable_log) WriteLog(__FUNCTION__" - Dll<%s> not found in scripts folder, error<%u>.\n",module_name,GetLastError());
			return 0;
		}

		// Load function
		size_t func=(size_t)GetProcAddress(dll,func_name);
		if(!func)
		{
			if(!disable_log) WriteLog(__FUNCTION__" - Function<%s> in dll<%s> not found, error<%u>.\n",func_name,module_name,GetLastError());
			return 0;
		}

		// Save to temporary bind
		if(is_temp)
		{
			BindedFunctions[1].IsScriptCall=false;
			BindedFunctions[1].ScriptFuncId=0;
			BindedFunctions[1].NativeFuncAddr=func;
			return 1;
		}

		// Find already binded
		for(int i=2,j=(int)BindedFunctions.size();i<j;i++)
		{
			BindFunction& bf=BindedFunctions[i];
			if(!bf.IsScriptCall && bf.NativeFuncAddr==func) return i;
		}

		// Create new bind
		BindedFunctions.push_back(BindFunction(0,func,module_name,func_name,decl));
	}
	return BindedFunctions.size()-1;
}

int Bind(const char* script_name, const char* decl, bool is_temp, bool disable_log /* = false */)
{
	char module_name[256];
	char func_name[256];
	if(!ReparseScriptName(script_name,module_name,func_name,disable_log))
	{
		WriteLog(__FUNCTION__" - Parse script name<%s> fail.\n",script_name);
		return 0;
	}
	return Bind(module_name,func_name,decl,is_temp,disable_log);
}

int RebindFunctions()
{
#ifdef SCRIPT_MULTITHREADING
	SCOPE_LOCK(BindedFunctionsLocker);
#endif

	int errors=0;
	for(int i=2,j=BindedFunctions.size();i<j;i++)
	{
		BindFunction& bf=BindedFunctions[i];
		if(bf.IsScriptCall)
		{
			int bind_id=Bind(bf.ModuleName.c_str(),bf.FuncName.c_str(),bf.FuncDecl.c_str(),true);
			if(bind_id<=0)
			{
				WriteLog(__FUNCTION__" - Unable to bind function, module<%s>, function<%s>, declaration<%s>.\n",bf.ModuleName.c_str(),bf.FuncName.c_str(),bf.FuncDecl.c_str());
				bf.ScriptFuncId=0;
				errors++;
			}
			else
			{
				bf.ScriptFuncId=BindedFunctions[1].ScriptFuncId;
			}
		}
	}
	return errors;
}

bool ReparseScriptName(const char* script_name, char* module_name, char* func_name, bool disable_log /* = false */)
{
	if(!script_name || !module_name || !func_name)
	{
		WriteLog(__FUNCTION__" - Some name null ptr.\n");
		CreateDump("ReparseScriptName");
		return false;
	}

	char script[MAX_SCRIPT_NAME*2+2];
	StringCopy(script,script_name);
	Str::EraseChars(script,' ');

	if(strstr(script,"@"))
	{
		char* script_ptr=&script[0];
		Str::CopyWord(module_name,script_ptr,'@');
		Str::GoTo((char*&)script_ptr,'@',true);
		Str::CopyWord(func_name,script_ptr,'\0');
	}
	else
	{
		StringCopy(module_name,MAX_SCRIPT_NAME,GetActiveModuleName());
		StringCopy(func_name,MAX_SCRIPT_NAME,script);
	}

	if(!strlen(module_name) || !strcmp(module_name,"<error>"))
	{
		if(!disable_log) WriteLog(__FUNCTION__" - Script name parse error, string<%s>.\n",script_name);
		module_name[0]=func_name[0]=0;
		return false;
	}
	if(!strlen(func_name))
	{
		if(!disable_log) WriteLog(__FUNCTION__" - Function name parse error, string<%s>.\n",script_name);
		module_name[0]=func_name[0]=0;
		return false;
	}
	return true;
}

/************************************************************************/
/* Functions accord                                                     */
/************************************************************************/

const StrVec& GetScriptFuncCache()
{
	return ScriptFuncCache;
}

void ResizeCache(DWORD count)
{
	ScriptFuncCache.resize(count);
	ScriptFuncBindId.resize(count);
}

DWORD GetScriptFuncNum(const char* script_name, const char* decl)
{
#ifdef SCRIPT_MULTITHREADING
	SCOPE_LOCK(BindedFunctionsLocker);
#endif

	char full_name[MAX_SCRIPT_NAME*2+1];
	char module_name[MAX_SCRIPT_NAME+1];
	char func_name[MAX_SCRIPT_NAME+1];
	if(!Script::ReparseScriptName(script_name,module_name,func_name)) return 0;
	Str::SFormat(full_name,"%s@%s",module_name,func_name);

	string str_cache=full_name;
	str_cache+="|";
	str_cache+=decl;

	// Find already cached
	for(size_t i=0,j=ScriptFuncCache.size();i<j;i++)
		if(ScriptFuncCache[i]==str_cache) return i+1;

	// Create new
	int bind_id=Script::Bind(full_name,decl,false);
	if(bind_id<=0) return 0;

	ScriptFuncCache.push_back(str_cache);
	ScriptFuncBindId.push_back(bind_id);
	return ScriptFuncCache.size();
}

int GetScriptFuncBindId(DWORD func_num)
{
#ifdef SCRIPT_MULTITHREADING
	SCOPE_LOCK(BindedFunctionsLocker);
#endif

	func_num--;
	if(func_num>=ScriptFuncBindId.size())
	{
		WriteLog(__FUNCTION__" - Function index<%u> is greater than bind buffer size<%u>.\n",func_num,ScriptFuncBindId.size());
		return 0;
	}
	return ScriptFuncBindId[func_num];
}

string GetScriptFuncName(DWORD func_num)
{
#ifdef SCRIPT_MULTITHREADING
	SCOPE_LOCK(BindedFunctionsLocker);
#endif

	func_num--;
	if(func_num>=ScriptFuncBindId.size())
	{
		WriteLog(__FUNCTION__" - Function index<%u> is greater than bind buffer size<%u>.\n",func_num,ScriptFuncBindId.size());
		return "error func index";
	}

	if(ScriptFuncBindId[func_num]>=BindedFunctions.size())
	{
		WriteLog(__FUNCTION__" - Bind index<%u> is greater than bind buffer size<%u>.\n",ScriptFuncBindId[func_num],BindedFunctions.size());
		return "error bind index";
	}

	BindFunction& bf=BindedFunctions[ScriptFuncBindId[func_num]];
	string result;
	result+=bf.ModuleName;
	result+="@";
	result+=bf.FuncName;
	return result;

	/*BindFunction& bf=BindedFunctions[ScriptFuncBindId[func_num]];
	string result;
	char buf[32];
	result+="(";
	result+=_itoa(func_num+1,buf,10);
	result+=") ";
	result+=bf.ModuleName;
	result+="@";
	result+=bf.FuncName;
	result+="(";
	result+=bf.FuncDecl;
	result+=")";
	return result;*/
}

/************************************************************************/
/* Contexts                                                             */
/************************************************************************/

THREAD bool ScriptCall=false;
THREAD asIScriptContext* CurrentCtx=NULL;
THREAD size_t NativeFuncAddr=0;
THREAD size_t NativeArgs[256]={0};
THREAD size_t NativeRetValue=0;
THREAD size_t CurrentArg=0;
THREAD int ExecutionRecursionCounter=0;

#ifdef SCRIPT_MULTITHREADING
DWORD SynchronizeThreadId=0;
int SynchronizeThreadCounter=0;
MutexEvent SynchronizeThreadLocker;
Mutex SynchronizeThreadLocalLocker;

typedef vector<EndExecutionCallback> EndExecutionCallbackVec;
typedef vector<EndExecutionCallback>::iterator EndExecutionCallbackVecIt;
THREAD EndExecutionCallbackVec* EndExecutionCallbacks;
#endif

void BeginExecution()
{
#ifdef SCRIPT_MULTITHREADING
	if(!LogicMT) return;

	if(!ExecutionRecursionCounter)
	{
		GarbageLocker.EnterCode();

		SyncManager* sync_mngr=SyncManager::GetForCurThread();
		if(!ConcurrentExecution)
		{
			sync_mngr->Suspend();
			ConcurrentExecutionLocker.Lock();
			sync_mngr->PushPriority(5);
			sync_mngr->Resume();
		}
		else
		{
			sync_mngr->PushPriority(5);
		}
	}
	ExecutionRecursionCounter++;
#endif
}

void EndExecution()
{
#ifdef SCRIPT_MULTITHREADING
	if(!LogicMT) return;

	ExecutionRecursionCounter--;
	if(!ExecutionRecursionCounter)
	{
		GarbageLocker.LeaveCode();

		if(!ConcurrentExecution)
		{
			ConcurrentExecutionLocker.Unlock();

			SyncManager* sync_mngr=SyncManager::GetForCurThread();
			sync_mngr->PopPriority();
		}
		else
		{
			SyncManager* sync_mngr=SyncManager::GetForCurThread();
			sync_mngr->PopPriority();

			SynchronizeThreadLocalLocker.Lock();
			bool sync_not_closed=(SynchronizeThreadId==GetCurrentThreadId());
			if(sync_not_closed)
			{
				SynchronizeThreadId=0;
				SynchronizeThreadCounter=0;
				SynchronizeThreadLocker.Allow(); // Unlock synchronization section
			}
			SynchronizeThreadLocalLocker.Unlock();

			if(sync_not_closed)
			{
				WriteLog(__FUNCTION__" - Synchronization section is not closed in script!\n");
				sync_mngr->PopPriority();
			}
		}

		if(EndExecutionCallbacks && !EndExecutionCallbacks->empty())
		{
			for(EndExecutionCallbackVecIt it=EndExecutionCallbacks->begin(),end=EndExecutionCallbacks->end();it!=end;++it)
			{
				EndExecutionCallback func=*it;
				(*func)();
			}
			EndExecutionCallbacks->clear();
		}
	}
#endif
}

void AddEndExecutionCallback(EndExecutionCallback func)
{
#ifdef SCRIPT_MULTITHREADING
	if(!LogicMT) return;

	if(!EndExecutionCallbacks) EndExecutionCallbacks=new(nothrow) EndExecutionCallbackVec();
	EndExecutionCallbackVecIt it=std::find(EndExecutionCallbacks->begin(),EndExecutionCallbacks->end(),func);
	if(it==EndExecutionCallbacks->end()) EndExecutionCallbacks->push_back(func);
#endif
}

bool PrepareContext(int bind_id, const char* call_func, const char* ctx_info)
{
#ifdef SCRIPT_MULTITHREADING
	if(LogicMT) BindedFunctionsLocker.Lock();
#endif

	if(bind_id<=0 || bind_id>=BindedFunctions.size())
	{
		WriteLog(__FUNCTION__" - Invalid bind id<%d>.\n",bind_id);
#ifdef SCRIPT_MULTITHREADING
		if(LogicMT) BindedFunctionsLocker.Unlock();
#endif
		return false;
	}

	BindFunction& bf=BindedFunctions[bind_id];
	bool is_script=bf.IsScriptCall;
	int func_id=bf.ScriptFuncId;
	size_t func_addr=bf.NativeFuncAddr;

#ifdef SCRIPT_MULTITHREADING
	if(LogicMT) BindedFunctionsLocker.Unlock();
#endif

	if(is_script)
	{
		if(func_id<=0) return false;

		asIScriptContext* ctx=GetGlobalContext();
		if(!ctx) return false;

		BeginExecution();

		StringCopy((char*)ctx->GetUserData(),CONTEXT_BUFFER_SIZE,call_func);
		StringAppend((char*)ctx->GetUserData(),CONTEXT_BUFFER_SIZE,ctx_info);

		int result=ctx->Prepare(func_id);
		if(result<0)
		{
			WriteLog(__FUNCTION__" - Prepare error, context name<%s>, bind_id<%d>, func_id<%d>, error<%d>.\n",ctx->GetUserData(),bind_id,func_id,result);
			GlobalCtxIndex--;
			EndExecution();
			return false;
		}

		CurrentCtx=ctx;
		ScriptCall=true;
	}
	else
	{
		BeginExecution();

		NativeFuncAddr=func_addr;
		ScriptCall=false;
	}

	CurrentArg=0;
	return true;
}

void SetArgWord(WORD w)
{
	if(ScriptCall) CurrentCtx->SetArgWord(CurrentArg,w);
	else NativeArgs[CurrentArg]=w;
	CurrentArg++;
}

void SetArgDword(DWORD dw)
{
	if(ScriptCall) CurrentCtx->SetArgDWord(CurrentArg,dw);
	else NativeArgs[CurrentArg]=dw;
	CurrentArg++;
}

void SetArgByte(BYTE b)
{
	if(ScriptCall) CurrentCtx->SetArgByte(CurrentArg,b);
	else NativeArgs[CurrentArg]=b;
	CurrentArg++;
}

void SetArgBool(bool bl)
{
	if(ScriptCall) CurrentCtx->SetArgByte(CurrentArg,bl);
	else NativeArgs[CurrentArg]=bl;
	CurrentArg++;
}

void SetArgObject(void* obj)
{
	if(ScriptCall) CurrentCtx->SetArgObject(CurrentArg,obj);
	else NativeArgs[CurrentArg]=(size_t)obj;
	CurrentArg++;
}

void SetArgAddress(void* addr)
{
	if(ScriptCall) CurrentCtx->SetArgAddress(CurrentArg,addr);
	else NativeArgs[CurrentArg]=(size_t)addr;
	CurrentArg++;
}

bool RunPrepared()
{
	if(ScriptCall)
	{
		asIScriptContext* ctx=CurrentCtx;
		DWORD tick=Timer::FastTick();

		if(GlobalCtxIndex==1) // First context from stack, add timing
		{
			SCOPE_LOCK(ActiveGlobalCtxLocker);
			ActiveContexts.push_back(ActiveContext(GlobalCtx,tick));
		}

		int result=ctx->Execute();

		GlobalCtxIndex--;
		if(GlobalCtxIndex==0) // All scripts execution complete, erase timing
		{
			SCOPE_LOCK(ActiveGlobalCtxLocker);
			ActiveContextVecIt it=std::find(ActiveContexts.begin(),ActiveContexts.end(),(asIScriptContext**)GlobalCtx);
			if(it!=ActiveContexts.end()) ActiveContexts.erase(it);
		}

		DWORD delta=Timer::FastTick()-tick;

		asEContextState state=ctx->GetState();
		if(state!=asEXECUTION_FINISHED)
		{
			if(state==asEXECUTION_EXCEPTION) WriteLog("Execution of script stopped due to exception.\n");
			else if(state==asEXECUTION_SUSPENDED) WriteLog("Execution of script stopped due to timeout<%u>.\n",RunTimeoutSuspend);
			else WriteLog("Execution of script stopped due to %s.\n",ContextStatesStr[(int)state]);
			PrintContextCallstack(ctx); // Name and state of context will be printed in this function
			ctx->Abort();
			EndExecution();
			return false;
		}
		else if(RunTimeoutMessage && delta>=RunTimeoutMessage)
		{
			WriteLog("Script work time<%u> in context<%s>.\n",delta,ctx->GetUserData());
		}

		if(result<0)
		{
			WriteLog(__FUNCTION__" - Context<%s> execute error<%d>, state<%s>.\n",ctx->GetUserData(),result,ContextStatesStr[(int)state]);
			EndExecution();
			return false;
		}

		CurrentCtx=ctx;
		ScriptCall=true;
	}
	else
	{
		size_t* args=NativeArgs;
		size_t args_size=CurrentArg*4;
		size_t func_addr=NativeFuncAddr;
		size_t ret_value=0;

		__asm
		{
			// We must save registers that are used
			push ecx
			// Clear the FPU stack, in case the called function doesn't do it by itself
			// Use emms instead of fninit to preserve FPU control word
			emms
			// Copy arguments from script stack to application stack
			mov  ecx, args_size
			mov  eax, args
			add  eax, ecx
			cmp  ecx, 0
			je   endcopy
		copyloop:
			sub  eax, 4
			push dword ptr [eax]
			sub  ecx, 4
			jne  copyloop
		endcopy:
			// Call function
			call [func_addr]
			// Pop arguments from stack
			add  esp, args_size
			// Restore registers
			pop  ecx
			// return value in EAX or EAX:EDX
			mov  ret_value, eax
		}

		NativeRetValue=ret_value;
		ScriptCall=false;
	}

	EndExecution();
	return true;
}

DWORD GetReturnedDword()
{
	return ScriptCall?CurrentCtx->GetReturnDWord():NativeRetValue;
}

bool GetReturnedBool()
{
	return ScriptCall?(CurrentCtx->GetReturnByte()!=0):((NativeRetValue&1)!=0);
}

void* GetReturnedObject()
{
	return ScriptCall?CurrentCtx->GetReturnObject():(void*)NativeRetValue;
}

bool SynchronizeThread()
{
#ifdef SCRIPT_MULTITHREADING
	if(!ConcurrentExecution) return true;

	SynchronizeThreadLocalLocker.Lock(); // Local lock

	if(!SynchronizeThreadId) // Section is free
	{
		SynchronizeThreadId=GetCurrentThreadId();
		SynchronizeThreadCounter=1;
		SynchronizeThreadLocker.Disallow(); // Lock synchronization section
		SynchronizeThreadLocalLocker.Unlock(); // Local unlock

		SyncManager* sync_mngr=SyncManager::GetForCurThread();
		sync_mngr->PushPriority(10);
	}
	else if(SynchronizeThreadId==GetCurrentThreadId()) // Section busy by current thread
	{
		SynchronizeThreadCounter++;
		SynchronizeThreadLocalLocker.Unlock(); // Local unlock
	}
	else // Section busy by another thread
	{
		SynchronizeThreadLocalLocker.Unlock(); // Local unlock

		SyncManager* sync_mngr=SyncManager::GetForCurThread();
		sync_mngr->Suspend(); // Allow other threads take objects
		SynchronizeThreadLocker.Wait(); // Sleep until synchronization section locked
		sync_mngr->Resume(); // Relock busy objects
		return SynchronizeThread(); // Try enter again
	}
#endif
	return true;
}

bool ResynchronizeThread()
{
#ifdef SCRIPT_MULTITHREADING
	if(!ConcurrentExecution) return true;

	SynchronizeThreadLocalLocker.Lock(); // Local lock

	if(SynchronizeThreadId==GetCurrentThreadId())
	{
		if(--SynchronizeThreadCounter==0)
		{
			SynchronizeThreadId=0;
			SynchronizeThreadLocker.Allow(); // Unlock synchronization section
			SynchronizeThreadLocalLocker.Unlock(); // Local unlock

			SyncManager* sync_mngr=SyncManager::GetForCurThread();
			sync_mngr->PopPriority();
		}
		else
		{
			SynchronizeThreadLocalLocker.Unlock(); // Local unlock
		}
	}
	else
	{
		// Invalid call
		SynchronizeThreadLocalLocker.Unlock(); // Local unlock
		return false;
	}
#endif
	return true;
}

/************************************************************************/
/* Logging                                                              */
/************************************************************************/

bool StartLog()
{
	if(EngineLogFile) return true;
	EngineLogFile=CreateFile(".\\FOscript.log",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_FLAG_WRITE_THROUGH,NULL);
	if(EngineLogFile==INVALID_HANDLE_VALUE) return false;
	LogA("Start logging script system.\n");
	return true;
}

void EndLog()
{
	if(!EngineLogFile) return;
	LogA("End logging script system.\n");
	CloseHandle(EngineLogFile);
	EngineLogFile=NULL;
}

void Log(const char* str)
{
	asIScriptContext* ctx=asGetActiveContext();
	if(!ctx)
	{
		LogA(str);
		return;
	}
	asIScriptFunction* func=ctx->GetFunction(0);
	if(!func)
	{
		LogA(str);
		return;
	}
	if(LogDebugInfo)
	{
		int line,column;
		line=ctx->GetLineNumber(0,&column);
		LogA(Str::Format("Script callback: %s : %s : %s : %d, %d : %s.\n",str,func->GetModuleName(),func->GetDeclaration(true),line,column,ctx->GetUserData()));
	}
	else LogA(Str::Format("%s : %s\n",func->GetModuleName(),str));
}

void LogA(const char* str)
{
	WriteLog("%s",str);
	if(!EngineLogFile) return;
	DWORD br;
	WriteFile(EngineLogFile,str,strlen(str),&br,NULL);
}

void LogError(const char* error)
{
	asIScriptContext* ctx=asGetActiveContext();
	if(!ctx)
	{
		LogA(error);
		return;
	}
	asIScriptFunction* func=ctx->GetFunction(0);
	if(!func)
	{
		LogA(error);
		return;
	}
	int line,column;
	line=ctx->GetLineNumber(0,&column);
	LogA(Str::Format("Script error: %s : %s : %s : %d, %d : %s.\n",error,func->GetModuleName(),func->GetDeclaration(true),line,column,ctx->GetUserData()));
}

void SetLogDebugInfo(bool enabled)
{
	LogDebugInfo=enabled;
}

void CallbackMessage(const asSMessageInfo* msg, void* param)
{
	const char* type="Error";
	if(msg->type==asMSGTYPE_WARNING) type="Warning";
	else if(msg->type==asMSGTYPE_INFORMATION) type="Info";
	LogA(Str::Format("Script message: %s : %s : %s : %d, %d.\n",msg->section,type,msg->message,msg->row,msg->col));
}

void CallbackException(asIScriptContext* ctx, void* param)
{
	int line,column;
	line=ctx->GetExceptionLineNumber(&column);
	asIScriptFunction* func=Engine->GetFunctionDescriptorById(ctx->GetExceptionFunction());
	if(!func)
	{
		LogA(Str::Format("Script exception: %s : %s.\n",ctx->GetExceptionString(),ctx->GetUserData()));
		return;
	}
	LogA(Str::Format("Script exception: %s : %s : %s : %d, %d : %s.\n",ctx->GetExceptionString(),func->GetModuleName(),func->GetDeclaration(true),line,column,ctx->GetUserData()));
}

/************************************************************************/
/* Array                                                                */
/************************************************************************/

CScriptArray* CreateArray(const char* type)
{
	return new CScriptArray(0,Engine->GetObjectTypeById(Engine->GetTypeIdByDecl(type)));
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

}