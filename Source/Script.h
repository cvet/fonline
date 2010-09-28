#ifndef __SCRIPT__
#define __SCRIPT__

#include "AngelScript/angelscript.h"
#include "AngelScript/scriptstring.h"
#include <vector>

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

namespace Script
{
	bool Init(bool with_log, PragmaCallbackFunc crdata);
	void Finish();
	bool InitThread();
	void FinisthThread();

	HMODULE LoadDynamicLibrary(const char* dll_name);
	void SetWrongGlobalObjects(StrVec& names);
	void SetConcurrentExecution(bool enabled);

	void UnloadScripts();
	bool ReloadScripts(const char* config, const char* key, bool skip_binaries);
	bool BindReservedFunctions(const char* config, const char* key, ReservedScriptFunction* bind_func, DWORD bind_func_count);

	void AddRef();
	void Release();

	asIScriptEngine* GetEngine();
	void SetEngine(asIScriptEngine* engine);
	asIScriptEngine* CreateEngine(PragmaCallbackFunc crdata);
	void FinishEngine(asIScriptEngine*& engine);

	asIScriptContext* CreateContext();
	void FinishContext(asIScriptContext*& ctx);
	asIScriptContext* GetGlobalContext();
	void PrintContextCallstack(asIScriptContext* ctx);

	const char* GetActiveModuleName();
	const char* GetActiveFuncName();
	asIScriptModule* GetModule(const char* name);
	asIScriptModule* CreateModule(const char* module_name);

	void SetGarbageCollectTime(DWORD ticks);
	void CollectGarbage(bool force);
	void SetRunTimeout(DWORD suspend_timeout, DWORD message_timeout);

	void SetScriptsPath(int path_type);
	void Define(const char* def);
	void Undefine(const char* def);
	char* Preprocess(const char* fname, bool process_pragmas);
	void CallPragmas(const StrVec& pragmas);
	bool LoadScript(const char* module_name, const char* source, bool skip_binary, const char* file_pefix = NULL);
	bool LoadScript(const char* module_name, const BYTE* bytecode, DWORD len);

	int BindImportedFunctions();
	int Bind(const char* module_name, const char* func_name, const char* decl, bool is_temp, bool disable_log = false);
	int Bind(const char* script_name, const char* decl, bool is_temp, bool disable_log = false);
	int RebindFunctions();
	bool ReparseScriptName(const char* script_name, char* module_name, char* func_name, bool disable_log = false);

	const StrVec& GetScriptFuncCache();
	void ResizeCache(DWORD count);
	DWORD GetScriptFuncNum(const char* script_name, const char* decl);
	int GetScriptFuncBindId(DWORD func_num);
	string GetScriptFuncName(DWORD func_num);

	// Script execution
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
	bool SynchronizeThread();
	bool ResynchronizeThread();

	// Logging
	bool StartLog();
	void EndLog();
	void Log(const char* str);
	void LogA(const char* str);
	void LogError(const char* error);
	void SetLogDebugInfo(bool enabled);

	void CallbackMessage(const asSMessageInfo* msg, void* param);
	void CallbackException(asIScriptContext* ctx, void* param);

	// Arrays stuff
	asIScriptArray* CreateArray(const char* type);

	template<typename Type>
	void AppendVectorToArray(vector<Type>& vec, asIScriptArray* arr)
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
	void AppendVectorToArrayRef(vector<Type>& vec, asIScriptArray* arr)
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
	void AssignScriptArrayInVector(vector<Type>& vec, asIScriptArray* arr)
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
}

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