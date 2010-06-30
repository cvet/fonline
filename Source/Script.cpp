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

asIScriptEngine* Engine=NULL;
HANDLE EngineLogFile=NULL;
StrVec ModuleNames;
asIScriptContext* GlobalCtx[GLOBAL_CONTEXT_STACK_SIZE]={0};
BindFunctionVec BindedFunctions;
string ScriptsPath;
bool LogDebugInfo=true;

// Timeouts
DWORD RunTimeoutSuspend=0;
DWORD RunTimeoutMessage=0;
HANDLE RunTimeoutThreadHandle=NULL;
HANDLE RunTimeoutStartEvent=NULL;
HANDLE RunTimeoutFinishEvent=NULL;
unsigned int __stdcall RunTimeoutThread(void*);

// #pragma globalvar "int __MyGlobalVar = 100"
class GvarPragmaCallback : public Preprocessor::PragmaCallback
{
private:
	static set<string> addedVars;
	static list<int> intArray;
	static list<__int64> int64Array;
	static list<CScriptString*> stringArray;
	static list<float> floatArray;
	static list<double> doubleArray;
	static list<char> boolArray;

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
			value=instance.text.substr(instance.text.find(value),string::npos);
			list<CScriptString*>::iterator it=stringArray.insert(stringArray.begin(),new CScriptString(value));
			if(Engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) WriteLog("Unable to register string global var, pragma<%s>.\n",instance.text.c_str());
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

	static void clear()
	{
		addedVars.clear();
		intArray.clear();
		int64Array.clear();
		stringArray.clear();
		floatArray.clear();
		doubleArray.clear();
		boolArray.clear();
	}
};
set<string> GvarPragmaCallback::addedVars;
list<int> GvarPragmaCallback::intArray;
list<__int64> GvarPragmaCallback::int64Array;
list<CScriptString*> GvarPragmaCallback::stringArray;
list<float> GvarPragmaCallback::floatArray;
list<double> GvarPragmaCallback::doubleArray;
list<char> GvarPragmaCallback::boolArray;

// #pragma crdata "Stat 0 199"
class CrDataPragmaCallback : public Preprocessor::PragmaCallback
{
public:
	static PragmaCallbackFunc CallFunc;

	void pragma(const Preprocessor::PragmaInstance& instance)
	{
		if(CallFunc && !(*CallFunc)(instance.text.c_str())) WriteLog("Unable to parse crdata pragma<%s>.\n",instance.text.c_str());
	}
};
PragmaCallbackFunc CrDataPragmaCallback::CallFunc=NULL;

// #pragma bindfunc "int MyFunc(int, uint) -> my.dll MyDllFunc"
// #pragma bindfunc "int MyObject::MyMethod(int, uint) -> my.dll MyDllFunc"
class BindFuncPragmaCallback : public Preprocessor::PragmaCallback
{
private:
	static set<string> alreadyProcessed;

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
set<string> BindFuncPragmaCallback::alreadyProcessed;

bool Init(bool with_log, PragmaCallbackFunc crdata)
{
	if(with_log && !StartLog())
	{
		WriteLog(__FUNCTION__" - Log creation error.\n");
		return false;
	}

	Engine=asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if(!Engine)
	{
		WriteLog(__FUNCTION__" - asCreateScriptEngine fail.\n");
		return false;
	}

	Engine->SetMessageCallback(asFUNCTION(CallbackMessage),NULL,asCALL_CDECL);
	RegisterScriptString(Engine);
	RegisterScriptStringUtils(Engine);
	RegisterScriptAny(Engine);
	RegisterScriptDictionary(Engine);
	RegisterScriptFile(Engine);
	RegisterScriptMath(Engine);
	RegisterScriptMath3D(Engine);
	RegisterScriptArray(Engine);

	BindedFunctions.clear();
	BindedFunctions.reserve(10000);
	BindedFunctions.push_back(BindFunction(0,0,"","","")); // None
	BindedFunctions.push_back(BindFunction(0,0,"","","")); // Temp

	for(int i=0;i<GLOBAL_CONTEXT_STACK_SIZE;i++)
	{
		GlobalCtx[i]=CreateContext();
		if(!GlobalCtx[i])
		{
			WriteLog(__FUNCTION__" - Create global contexts fail.\n");
			return false;
		}
	}

	CrDataPragmaCallback::CallFunc=crdata;
	Preprocessor::RegisterPragma("globalvar",new GvarPragmaCallback());
	Preprocessor::RegisterPragma("crdata",new CrDataPragmaCallback());
	Preprocessor::RegisterPragma("bindfunc",new BindFuncPragmaCallback());

	RunTimeoutSuspend=30000;
	RunTimeoutMessage=10000;
	RunTimeoutThreadHandle=(HANDLE)_beginthreadex(NULL,0,RunTimeoutThread,NULL,0,NULL);
	RunTimeoutStartEvent=CreateEvent(NULL,FALSE,FALSE,NULL);
	RunTimeoutFinishEvent=CreateEvent(NULL,FALSE,FALSE,NULL);
	return true;
}

void Finish()
{
	EndLog();
	RunTimeoutSuspend=0;
	RunTimeoutMessage=0;
	SetEvent(RunTimeoutStartEvent);
	SetEvent(RunTimeoutFinishEvent);
	WaitForSingleObject(RunTimeoutThreadHandle,INFINITE);
	CloseHandle(RunTimeoutThreadHandle);
	RunTimeoutThreadHandle=NULL;
	CloseHandle(RunTimeoutStartEvent);
	RunTimeoutStartEvent=NULL;
	CloseHandle(RunTimeoutFinishEvent);
	RunTimeoutFinishEvent=NULL;

	ModuleNames.clear();
	BindedFunctions.clear();
	GvarPragmaCallback::clear();
	for(int i=0;i<GLOBAL_CONTEXT_STACK_SIZE;i++)
	{
		FinishContext(GlobalCtx[i]);
		GlobalCtx[i]=NULL;
	}
	if(Engine)
	{
		Engine->Release();
		Engine=NULL;
	}
}

HMODULE LoadDynamicLibrary(const char* dll_name)
{
	// Load dynamic library
	char dll_name_[256];
	StringCopy(dll_name_,ScriptsPath.c_str());
	StringAppend(dll_name_,dll_name);
	HMODULE dll=LoadLibrary(dll_name_);
	if(!dll) return NULL;

	// Register global function and vars
	static StringSet alreadyLoadedDll;
	if(!alreadyLoadedDll.count(dll_name_))
	{
		// Register variables
		size_t* ptr=(size_t*)GetProcAddress(dll,"GameOpt");
		if(ptr) *ptr=(size_t)&GameOpt;
		ptr=(size_t*)GetProcAddress(dll,"ASEngine");
		if(ptr) *ptr=(size_t)Engine;

		// Register functions
		ptr=(size_t*)GetProcAddress(dll,"Log");
		if(ptr) *ptr=(size_t)&WriteLog;

		// Call init function
		typedef void(*DllMainEx)(bool);
		DllMainEx func=(DllMainEx)GetProcAddress(dll,"DllMainEx");
		if(func) (*func)(false);

		alreadyLoadedDll.insert(dll_name_);
	}
	return dll;
}

void UnloadScripts()
{
	for(StrVecIt it=ModuleNames.begin(),end=ModuleNames.end();it!=end;++it)
	{
		string& mod_name=*it;
		asIScriptModule* mod=Engine->GetModule(mod_name.c_str(),asGM_ONLY_IF_EXISTS);
		if(!mod)
		{
			WriteLog(__FUNCTION__" - Module<%s> not found.\n",mod_name.c_str());
			continue;
		}

		int result=mod->ResetGlobalVars();
		if(result<0) WriteLog(__FUNCTION__" - Reset global vars fail, module<%s>, error<%d>.\n",mod_name.c_str(),result);
		result=mod->UnbindAllImportedFunctions();
		if(result<0) WriteLog(__FUNCTION__" - Unbind fail, module<%s>, error<%d>.\n",mod_name.c_str(),result);
	}

	for(StrVecIt it=ModuleNames.begin(),end=ModuleNames.end();it!=end;++it)
	{
		string& mod_name=*it;
		asIScriptModule* mod=Engine->GetModule(mod_name.c_str(),asGM_ONLY_IF_EXISTS);
		if(!mod)
		{
			WriteLog(__FUNCTION__" - Module<%s> not found.\n",mod_name.c_str());
			continue;
		}
		Engine->DiscardModule(mod_name.c_str());
	}
	CollectGarbage(true);
	ModuleNames.clear();
}

bool ReloadScripts(const char* config, const char* key, bool skip_binaries)
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
		if(str.fail() || key!=value) continue;
		str >> value;
		if(str.fail() || value!="module") continue;

		str >> value;
		if(str.fail() || !LoadScript(value.c_str(),NULL,!skip_binaries))
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

void AddRef(){} // Dummy
void Release(){} // Dummy

asIScriptEngine* GetEngine()
{
	return Engine;
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
	}
	ctx=NULL;
}

asIScriptContext* GetGlobalContext()
{
	if(GlobalCtx[0]->GetState()!=asEXECUTION_ACTIVE)
	{
		return GlobalCtx[0];
	}
	else
	{
		for(int i=1;i<GLOBAL_CONTEXT_STACK_SIZE;i++) if(GlobalCtx[i]->GetState()!=asEXECUTION_ACTIVE) return GlobalCtx[i];
	}
	WriteLog(__FUNCTION__" - Script context stack overflow! Context call stack:\n");
	for(int i=GLOBAL_CONTEXT_STACK_SIZE-1;i>=0;i--) WriteLog("  %d) %s.\n",i,GlobalCtx[i]->GetUserData());
	return NULL;
}

void PrintContextCallstack(asIScriptContext *ctx)
{
	int line,column;
	int func_id;
	const asIScriptFunction* func;
	int stack_size=ctx->GetCallstackSize();
	WriteLog("Context<%s>, state<%s>, call stack<%d>:\n",ctx->GetUserData(),ContextStatesStr[(int)ctx->GetState()],stack_size+1);

	// Print current function
	if(ctx->GetState()==asEXECUTION_EXCEPTION)
	{
		line=ctx->GetExceptionLineNumber(&column);
		func_id=ctx->GetExceptionFunction();
	}
	else
	{
		line=ctx->GetCurrentLineNumber(&column);
		func_id=ctx->GetCurrentFunction();
	}
	func=Engine->GetFunctionDescriptorById(func_id);
	if(func) WriteLog("  %d) %s : %s : %d, %d.\n",stack_size,func->GetModuleName(),func->GetDeclaration(),line,column);

	// Print call stack
	for(int i=stack_size-1;i>=0;i--)
	{
		func_id=ctx->GetCallstackFunction(i);
		line=ctx->GetCallstackLineNumber(i,&column);
		func=Engine->GetFunctionDescriptorById(func_id);
		if(func) WriteLog("  %d) %s : %s : %d, %d.\n",i,func->GetModuleName(),func->GetDeclaration(),line,column);
	}
}

const char* GetActiveModuleName()
{
	static char error[]="<error>";
	asIScriptContext* ctx=asGetActiveContext();
	if(!ctx) return error;
	asIScriptFunction* func=Engine->GetFunctionDescriptorById(ctx->GetCurrentFunction());
	if(!func)
	{
		WriteLog(__FUNCTION__" - Function descriptor not found, context name<%s>, current function id<%d>.",ctx->GetUserData(),ctx->GetCurrentFunction());
		return error;
	}
	return func->GetModuleName();
}

const char* GetActiveFuncName()
{
	static char error[]="<error>";
	asIScriptContext* ctx=asGetActiveContext();
	if(!ctx) return error;
	asIScriptFunction* func=Engine->GetFunctionDescriptorById(ctx->GetCurrentFunction());
	if(!func)
	{
		WriteLog(__FUNCTION__" - Function descriptor not found, context name<%s>, current function id<%d>.",ctx->GetUserData(),ctx->GetCurrentFunction());
		return error;
	}
	return func->GetName();
}

asIScriptModule* GetModule(const char* name)
{
	return Engine->GetModule(name,asGM_ONLY_IF_EXISTS);
}

asIScriptModule* CreateModule(const char* module_name)
{
	StrVecIt it=std::find(ModuleNames.begin(),ModuleNames.end(),module_name);
	if(it!=ModuleNames.end()) ModuleNames.erase(it);
	asIScriptModule* mod=Engine->GetModule(module_name,asGM_ALWAYS_CREATE);
	if(!mod) return NULL;
	ModuleNames.push_back(module_name);
	return mod;
}

char* Preprocess(const char* fname, bool process_pragmas)
{
	Preprocessor::VectorOutStream vos;
	Preprocessor::FileSource fsrc;
	fsrc.CurrentDir=ScriptsPath;

	if(Preprocessor::Preprocess(fname,fsrc,vos,process_pragmas))
	{
		WriteLog(__FUNCTION__" - Unable to preprocess<%s> script.\n",fname);
		return NULL;
	}
	char* result=new char[vos.GetSize()+1];
	memcpy(result,vos.GetData(),vos.GetSize());
	result[vos.GetSize()]=0;
	return result;
}

DWORD GarbageCollectTime=60000;
void SetGarbageCollectTime(DWORD ticks)
{
	GarbageCollectTime=ticks;
}

void CollectGarbage(bool force)
{
	static DWORD last_tick=Timer::FastTick();
	if(force || (GarbageCollectTime && Timer::FastTick()-last_tick>=GarbageCollectTime))
	{
		Engine->GarbageCollect(asGC_FULL_CYCLE);
		last_tick=Timer::FastTick();
	}
}

unsigned int __stdcall RunTimeoutThread(void* data)
{
	while(RunTimeoutSuspend)
	{
		WaitForSingleObject(RunTimeoutStartEvent,INFINITE);
		if(WaitForSingleObject(RunTimeoutFinishEvent,RunTimeoutSuspend)==WAIT_TIMEOUT)
		{
			for(int i=GLOBAL_CONTEXT_STACK_SIZE-1;i>=0;i--)
				if(GlobalCtx[i]->GetState()==asEXECUTION_ACTIVE) GlobalCtx[i]->Suspend();
			WaitForSingleObject(RunTimeoutFinishEvent,INFINITE);
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

void SetScriptsPath(const char* path)
{
	ScriptsPath=path;
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

bool LoadScript(const char* module_name, const char* source, bool check_binary)
{
	char fname[1024];
	StringCopy(fname,module_name);
	Str::Replacement(fname,'.','\\');
	StringAppend(fname,".fos");

	// Try load precompiled script
	FileManager file_bin;
	if(check_binary)
	{
		FileManager file;
		file.LoadFile(Str::Format("%s%s",ScriptsPath.c_str(),fname));
		file_bin.LoadFile(Str::Format("%s%sb",ScriptsPath.c_str(),fname));

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
				file_dep.LoadFile(Str::Format("%s%s",ScriptsPath.c_str(),dependencies[i].c_str()));
				file_dep.GetTime(NULL,NULL,&last_write);
				if(!no_all_files) no_all_files=!file_dep.IsLoaded();
				if(!outdated) outdated=(file_dep.IsLoaded() && CompareFileTime(&last_write,&last_write_bin)>0);
			}

			if(no_all_files || (!outdated && bin_version==SERVER_VERSION))
			{
				if(bin_version!=SERVER_VERSION) WriteLog(__FUNCTION__" - Script<%s> compiled in older server version.\n",module_name);
				if(outdated) WriteLog(__FUNCTION__" - Script<%s> outdated.\n",module_name);

				StrVecIt it=std::find(ModuleNames.begin(),ModuleNames.end(),module_name);
				if(it!=ModuleNames.end())
				{
					WriteLog(__FUNCTION__" - Warning, script for this name<%s> already exist. Discard it.\n",module_name);
					Engine->DiscardModule(module_name);
					ModuleNames.erase(it);
				}

				asIScriptModule* mod=Engine->GetModule(module_name,asGM_ALWAYS_CREATE);
				if(mod)
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

					if(mod->LoadByteCode(&binary)>=0)
					{
						ModuleNames.push_back(module_name);
						return true;
					}
					else WriteLog(__FUNCTION__" - Can't load binary, script<%s>.\n",module_name);
				}
				else WriteLog(__FUNCTION__" - Create module fail, script<%s>.\n",module_name);
			}
		}

		if(!file.IsLoaded())
		{
			WriteLog(__FUNCTION__" - Script<%s> not found.\n",fname);
			return false;
		}

		file_bin.UnloadFile();
	}

	Preprocessor::VectorOutStream vos;
	Preprocessor::FileSource fsrc;
	Preprocessor::VectorOutStream vos_err;
	if(source) fsrc.Stream=source;
	fsrc.CurrentDir=ScriptsPath;

	if(Preprocessor::Preprocess(fname,fsrc,vos,true,vos_err))
	{
		vos_err.PushNull();
		WriteLog(__FUNCTION__" - Unable to preprocess file<%s>, error<%s>.\n",fname,vos_err.GetData());
		return false;
	}
	vos.Format();

	StrVecIt it=std::find(ModuleNames.begin(),ModuleNames.end(),module_name);
	if(it!=ModuleNames.end())
	{
		WriteLog(__FUNCTION__" - Warning, script for this name<%s> already exist. Discard it.\n",module_name);
		Engine->DiscardModule(module_name);
		ModuleNames.erase(it);
	}

	asIScriptModule* mod=Engine->GetModule(module_name,asGM_ALWAYS_CREATE);
	if(!mod)
	{
		WriteLog(__FUNCTION__" - Create module fail, script<%s>.\n",module_name);
		return false;
	}

	if(mod->AddScriptSection(module_name,vos.GetData(),vos.GetSize(),0)<0)
	{
		WriteLog(__FUNCTION__" - Unable to AddScriptSection module<%s>.\n",module_name);
		return false;
	}

	if(mod->Build()<0)
	{
		WriteLog(__FUNCTION__" - Unable to Build module<%s>.\n",module_name);
		return false;
	}

	ModuleNames.push_back(module_name);

	// Save binary version of script and preprocessed version
	if(check_binary && !file_bin.IsLoaded())
	{
		CBytecodeStream binary;
		if(mod->SaveByteCode(&binary)>=0)
		{
			std::vector<asBYTE>& data=binary.GetBuf();
			StrVec& dependencies=Preprocessor::GetFileDependencies();
			StrVec& pragmas=Preprocessor::GetPragmas();

			file_bin.SetBEDWord(SERVER_VERSION);
			file_bin.SetBEDWord(dependencies.size());
			for(size_t i=0,j=dependencies.size();i<j;i++) file_bin.SetData((BYTE*)dependencies[i].c_str(),dependencies[i].length()+1);
			file_bin.SetBEDWord(pragmas.size());
			for(size_t i=0,j=pragmas.size();i<j;i++) file_bin.SetData((BYTE*)pragmas[i].c_str(),pragmas[i].length()+1);
			file_bin.SetData(&data[0],data.size());

			if(!file_bin.SaveOutBufToFile(Str::Format("%s%sb",ScriptsPath.c_str(),fname),-1)) WriteLog(__FUNCTION__" - Can't save bytecode, script<%s>.\n",module_name);
		}
		else
		{
			WriteLog(__FUNCTION__" - Can't write bytecode, script<%s>.\n",module_name);
		}

		FILE* f=NULL;
		if(!fopen_s(&f,Str::Format("%s%sp",ScriptsPath.c_str(),fname),"wt"))
		{
			fwrite(vos.GetData(),sizeof(char),vos.GetSize(),f);
			fclose(f);
		}
		else
		{
			WriteLog(__FUNCTION__" - Can't write preprocessed file, script<%s>.\n",module_name);
		}
	}
	return true;
}

int BindImportedFunctions()
{
	WriteLog("Bind all imported functions...");
	int errors=0;
	//char fails[4096];
	for(StrVecIt it=ModuleNames.begin(),end=ModuleNames.end();it!=end;++it)
	{
		const string& mod_name=*it;
		asIScriptModule* mod=Engine->GetModule(mod_name.c_str(),asGM_ONLY_IF_EXISTS);
		if(!mod)
		{
			WriteLog("fail to find module<%s>...\n",mod_name.c_str());
			errors++;
			continue;
		}

		int result=mod->BindAllImportedFunctions();
		if(result<0)
		{
			//WriteLog("\n%s",fails);
			WriteLog("fail to bind, module<%s>, error<%d>...\n",mod_name.c_str(),result);
			errors++;
			continue;
		}
	}
	WriteLog("success.\n");
	return errors;
}

int Bind(const char* module_name, const char* func_name, const char* decl, bool is_temp)
{
	// Detect native dll
	if(!strstr(module_name,".dll"))
	{
		// Find module
		asIScriptModule* mod=Engine->GetModule(module_name,asGM_ONLY_IF_EXISTS);
		if(!mod)
		{
			WriteLog(__FUNCTION__" - Module<%s> not found.\n",module_name);
			return 0;
		}

		// Find function
		char decl_[256];
		sprintf(decl_,decl,func_name);
		int result=mod->GetFunctionIdByDecl(decl_);
		if(result<=0)
		{
			WriteLog(__FUNCTION__" - Function<%s> in module<%s> not found, result<%d>.\n",decl_,module_name,result);
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
			WriteLog(__FUNCTION__" - Dll<%s> not found in scripts folder, error<%u>.\n",module_name,GetLastError());
			return 0;
		}

		// Load function
		size_t func=(size_t)GetProcAddress(dll,func_name);
		if(!func)
		{
			WriteLog(__FUNCTION__" - Function<%s> in dll<%s> not found, error<%u>.\n",func_name,module_name,GetLastError());
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

int Bind(const char* script_name, const char* decl, bool is_temp)
{
	char module_name[256];
	char func_name[256];
	if(!ReparseScriptName(script_name,module_name,func_name))
	{
		WriteLog(__FUNCTION__" - Parse script name<%s> fail.\n",script_name);
		return 0;
	}
	return Bind(module_name,func_name,decl,is_temp);
}

int RebindFunctions()
{
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

bool ReparseScriptName(const char* script_name, char* module_name, char* func_name)
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
		WriteLog(__FUNCTION__" - Script name parse error, string<%s>.\n",script_name);
		module_name[0]=func_name[0]=0;
		return false;
	}
	if(!strlen(func_name))
	{
		WriteLog(__FUNCTION__" - Function name parse error, string<%s>.\n",script_name);
		module_name[0]=func_name[0]=0;
		return false;
	}
	return true;
}

/************************************************************************/
/* Functions accord                                                     */
/************************************************************************/

StrVec ScriptFuncCache;
IntVec ScriptFuncBindId;

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

bool ScriptCall=false;
asIScriptContext* CurrentCtx=NULL;
size_t NativeFuncAddr=0;
size_t NativeArgs[256]={0}; // Large buffer
size_t NativeRetValue=0;
size_t CurrentArg=0;

bool PrepareContext(int bind_id, const char* call_func, const char* ctx_info)
{
	if(bind_id<=0 || bind_id>=BindedFunctions.size())
	{
		WriteLog(__FUNCTION__" - Invalid bind id<%d>.\n",bind_id);
		return false;
	}

	BindFunction& bf=BindedFunctions[bind_id];
	if(bf.IsScriptCall)
	{
		int func_id=BindedFunctions[bind_id].ScriptFuncId;
		if(func_id<=0) return false;

		asIScriptContext* ctx=GetGlobalContext();
		if(!ctx) return false;

		StringCopy((char*)ctx->GetUserData(),CONTEXT_BUFFER_SIZE,call_func);
		StringAppend((char*)ctx->GetUserData(),CONTEXT_BUFFER_SIZE,ctx_info);

		int result=ctx->Prepare(func_id);
		if(result<0)
		{
			WriteLog(__FUNCTION__" - Prepare error, context name<%s>, bind_id<%d>, func_id<%d>, error<%d>.\n",ctx->GetUserData(),bind_id,func_id,result);
			return false;
		}

		CurrentCtx=ctx;
		ScriptCall=true;
	}
	else
	{
		NativeFuncAddr=bf.NativeFuncAddr;
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
		DWORD message_tick=Timer::FastTick();
		if(ctx==GlobalCtx[0]) SetEvent(RunTimeoutStartEvent);
		int result=ctx->Execute();
		if(ctx==GlobalCtx[0]) SetEvent(RunTimeoutFinishEvent);
		DWORD delta=Timer::FastTick()-message_tick;

		asEContextState state=ctx->GetState();
		if(state!=asEXECUTION_FINISHED)
		{
			if(state==asEXECUTION_EXCEPTION) WriteLog("Execution of script stopped due to exception.\n");
			else if(state==asEXECUTION_SUSPENDED) WriteLog("Execution of script stopped due to timeout<%u>.\n",RunTimeoutSuspend);
			else WriteLog("Execution of script stopped due to %s.\n",ContextStatesStr[(int)state]);
			PrintContextCallstack(ctx); // Name and state of context will be printed in this function
			ctx->Abort();
			return false;
		}
		else if(delta>=RunTimeoutMessage)
		{
			WriteLog("Script work time<%u> in context<%s>.\n",delta,ctx->GetUserData());
		}

		if(result<0)
		{
			WriteLog(__FUNCTION__" - Context<%s> execute error<%d>, state<%s>.\n",ctx->GetUserData(),result,ContextStatesStr[(int)state]);
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
			fninit
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
	asIScriptFunction* func=Engine->GetFunctionDescriptorById(ctx->GetCurrentFunction());
	if(!func)
	{
		LogA(str);
		return;
	}
	if(LogDebugInfo)
	{
		int line,column;
		line=ctx->GetCurrentLineNumber(&column);
		LogA(Str::Format("Script callback: %s : %s : %s : %d, %d : %s.\n",str,func->GetModuleName(),func->GetDeclaration(true),line,column,ctx->GetUserData()));
	}
	else LogA(Str::Format("%s : %s\n",func->GetModuleName(),str));
}

void LogA(const char* str)
{
	WriteLog(str);
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
	asIScriptFunction* func=Engine->GetFunctionDescriptorById(ctx->GetCurrentFunction());
	if(!func)
	{
		LogA(error);
		return;
	}
	int line,column;
	line=ctx->GetCurrentLineNumber(&column);
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
/* Built-In types                                                       */
/************************************************************************/

asIScriptArray* CreateArray(const char* type)
{
	return (asIScriptArray*)Engine->CreateScriptObject(Engine->GetTypeIdByDecl(type));
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

}