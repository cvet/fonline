#ifndef __SCRIPT__
#define __SCRIPT__

#include "AngelScript/angelscript.h"
#include "AngelScript/scriptstring.h"

#define GLOBAL_CONTEXT_STACK_SIZE      (10)
#define CONTEXT_BUFFER_SIZE            (512)
#define CALL_FUNC_STR                  __FUNCTION__" : "

typedef bool(*PragmaCallbackFunc)(const char*);

struct ReservedScriptFunction
{
	int* BindId;
	char FuncName[256];
	char FuncDecl[256];
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

class Script
{
private:
	asIScriptEngine* Engine;
	static HANDLE EngineLogFile;
	StrVec ModuleNames;
	asIScriptContext* GlobalCtx[GLOBAL_CONTEXT_STACK_SIZE];
	BindFunctionVec BindedFunctions;
	string ScriptsPath;
	bool LogDebugInfo;
	StringSet alreadyLoadedDll;

	// Pragmas
	void* pragmaGlobalvar;
	void* pragmaCrata;
	void* pragmaBindfunc;

	// Garbage
	DWORD GarbageCollectTime;
	DWORD garbageLastTick;

	// Functions accord
	StrVec ScriptFuncCache;
	IntVec ScriptFuncBindId;

	// Contexts
	bool ScriptCall;
	asIScriptContext* CurrentCtx;
	size_t NativeFuncAddr;
	size_t NativeArgs[256];
	size_t NativeRetValue;
	size_t CurrentArg;

	// Timeouts
	DWORD RunTimeoutSuspend;
	DWORD RunTimeoutMessage;
	HANDLE RunTimeoutThreadHandle;
	HANDLE RunTimeoutStartEvent;
	HANDLE RunTimeoutFinishEvent;

	static unsigned int __stdcall RunTimeoutThread(void* data);

public:
	Script();

	bool Init(bool with_log, PragmaCallbackFunc crdata);
	void Finish();
	HMODULE LoadDynamicLibrary(const char* dll_name);
	void UnloadScripts();

	bool ReloadScripts(const char* config, const char* key, bool skip_binaries);
	bool BindReservedFunctions(const char* config, const char* key, ReservedScriptFunction* bind_func, DWORD bind_func_count);

	asIScriptEngine* GetEngine();
	asIScriptContext* CreateContext();
	void FinishContext(asIScriptContext*& ctx);
	asIScriptContext* GetGlobalContext();
	asIScriptContext* GetActiveContext();
	void PrintContextCallstack(asIScriptContext *ctx);
	const char* GetActiveModuleName();
	const char* GetActiveFuncName();
	asIScriptModule* GetModule(const char* name);
	asIScriptModule* CreateModule(const char* module_name);
	char* Preprocess(const char* fname, bool process_pragmas);
	void SetGarbageCollectTime(DWORD ticks);
	void CollectGarbage(bool force);
	void SetRunTimeout(DWORD suspend_timeout, DWORD message_timeout);

	void SetScriptsPath(const char* path);
	void Define(const char* def);
	void Undefine(const char* def);
	bool LoadScript(const char* module_name, const char* source, bool check_binary);
	int BindImportedFunctions();
	int Bind(const char* module_name, const char* func_name, const char* decl, bool is_temp);
	int Bind(const char* script_name, const char* decl, bool is_temp);
	int RebindFunctions();
	bool ReparseScriptName(const char* script_name, char* module_name, char* func_name);

	const StrVec& GetScriptFuncCache();
	void ResizeCache(DWORD count);
	DWORD GetScriptFuncNum(const char* script_name, const char* decl);
	int GetScriptFuncBindId(DWORD func_num);
	string GetScriptFuncName(DWORD func_num);

	// Run context
	bool PrepareContext(int bind_id, const char* call_func, const char* ctx_info);
	void SetArgWord(WORD w);
	void SetArgDword(DWORD dw);
	void SetArgByte(BYTE b);
	void SetArgBool(bool bl);
	void SetArgObject(void* obj);
	void SetArgAddress(void* addr);
	bool RunPrepared();
	DWORD GetReturnedDword();
	bool GetReturnedBool();
	void* GetReturnedObject();

	static bool StartLog();
	static void EndLog();
	void Log(const char* str);
	static void LogA(const char* str);
	void LogError(const char* error);
	void SetLogDebugInfo(bool enabled);

	static void CallbackMessage(const asSMessageInfo* msg, void* param);
	static void CallbackException(asIScriptContext* ctx, void* param);

	// Arrays stuff
	asIScriptArray* CreateArray(const char* type);

	template<typename Type>
	static void AppendVectorToArray(vector<Type>& vec, asIScriptArray* arr)
	{
		if(!vec.empty() && arr)
		{
			asUINT i=arr->GetElementCount();
			arr->Resize(arr->GetElementCount()+vec.size());
			for(size_t k=0,l=vec.size();k<l;k++,i++)
			{
				Type* p=(Type*)arr->GetElementPointer(i);
				*p=vec[k];
			}
		}
	}
	template<typename Type>
	static void AppendVectorToArrayRef(vector<Type>& vec, asIScriptArray* arr)
	{
		if(!vec.empty() && arr)
		{
			asUINT i=arr->GetElementCount();
			arr->Resize(arr->GetElementCount()+vec.size());
			for(size_t k=0,l=vec.size();k<l;k++,i++)
			{
				Type* p=(Type*)arr->GetElementPointer(i);
				*p=vec[k];
				(*p)->AddRef();
			}
		}
	}
	template<typename Type>
	static void AssignScriptArrayInVector(vector<Type>& vec, asIScriptArray* arr)
	{
		if(arr)
		{
			asUINT count=arr->GetElementCount();
			if(count)
			{
				vec.resize(count);
				for(asUINT i=0;i<count;i++)
				{
					Type* p=(Type*)arr->GetElementPointer(i);
					vec[i]=*p;
				}
			}
		}
	}

	// Dummy functions
	static void AddRef();
	static void Release();
};

#ifdef FONLINE_SERVER
extern Script ServerScript;
#endif
#ifdef FONLINE_CLIENT
extern Script ClientScript;
#endif
#ifdef FONLINE_MAPPER
extern Script MapperScript;
#endif

class CBytecodeStream : public asIBinaryStream
{
private:
	int readPos;
	int writePos;
	std::vector<asBYTE> binBuf;

public:
	CBytecodeStream(){writePos=0; readPos=0;}
	void Write(const void* ptr, asUINT size){if(!ptr || !size) return; binBuf.resize(binBuf.size()+size); memcpy(&binBuf[writePos],ptr,size); writePos+=size;}
	void Read(void* ptr, asUINT size){if(!ptr || !size) return; memcpy(ptr,&binBuf[readPos],size); readPos+=size;}
	std::vector<asBYTE>& GetBuf(){return binBuf;}
};

#endif // __SCRIPT__