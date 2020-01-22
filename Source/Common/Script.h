#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "ScriptInvoker.h"
#include "ScriptPragmas.h"
#include "Settings.h"

#include "AngelScriptExt/scriptdict.h"
#include "angelscript.h"
#include "preprocessor.h"
#include "scriptarray/scriptarray.h"
#include "scriptdictionary/scriptdictionary.h"
#ifdef AS_MAX_PORTABILITY
#include "autowrapper/aswrappedcall.h"
#endif

#ifdef AS_MAX_PORTABILITY
#define SCRIPT_FUNC(name) WRAP_FN(name)
#define SCRIPT_FUNC_THIS(name) WRAP_OBJ_FIRST(name)
#define SCRIPT_METHOD(type, name) WRAP_MFN(type, name)
#define SCRIPT_FUNC_CONV asCALL_GENERIC
#define SCRIPT_FUNC_THIS_CONV asCALL_GENERIC
#define SCRIPT_METHOD_CONV asCALL_GENERIC
#else
#define SCRIPT_FUNC(name) asFUNCTION(name)
#define SCRIPT_FUNC_THIS(name) asFUNCTION(name)
#define SCRIPT_METHOD(type, name) asMETHOD(type, name)
#define SCRIPT_FUNC_CONV asCALL_CDECL
#define SCRIPT_FUNC_THIS_CONV asCALL_CDECL_OBJFIRST
#define SCRIPT_METHOD_CONV asCALL_THISCALL
#endif

#define SCRIPT_ERROR_R(error, ...) \
    do \
    { \
        Script::RaiseException(_str(error, ##__VA_ARGS__)); \
        return; \
    } while (0)
#define SCRIPT_ERROR_R0(error, ...) \
    do \
    { \
        Script::RaiseException(_str(error, ##__VA_ARGS__)); \
        return 0; \
    } while (0)

typedef void (*EndExecutionCallback)();
typedef vector<asIScriptContext*> ContextVec;
typedef std::function<void(const string&)> ExceptionCallback;

struct EngineData
{
    ScriptPragmaCallback* PragmaCB;
    ScriptInvoker* Invoker;
    StrIntMap CachedEnums;
    map<string, IntStrMap> CachedEnumNames;
};

struct ScriptEntry
{
    string Name;
    string Path;
    string Content;
    int SortValue;
    int SortValueExt;
};
using ScriptEntryVec = vector<ScriptEntry>;

struct EventData;

class Script
{
public:
    static bool Init(ScriptSettings& sett, FileManager& file_mngr, ScriptPragmaCallback* pragma_callback, const string& target);
    static bool InitMono(const string& target, map<string, UCharVec>* assemblies_data);
    static bool GetMonoAssemblies(const string& target, map<string, UCharVec>& assemblies_data);
    static uint CreateMonoObject(const string& type_name);
    static void CallMonoObjectMethod(const string& type_name, const string& method_name, uint obj, void* arg);
    static void DestroyMonoObject(uint obj);
    static void Finish();

    static void UnloadScripts();
    static bool ReloadScripts(const string& target);
    static bool PostInitScriptSystem();
    static bool RunModuleInitFunctions();

    static asIScriptEngine* GetEngine();
    static void SetEngine(asIScriptEngine* engine);
    static asIScriptEngine* CreateEngine(ScriptPragmaCallback* pragma_callback, const string& target);
    static void RegisterScriptArrayExtensions(asIScriptEngine* engine);
    static void RegisterScriptDictExtensions(asIScriptEngine* engine);
    static void RegisterScriptStdStringExtensions(asIScriptEngine* engine);
    static void FinishEngine(asIScriptEngine*& engine);

    static void CreateContext();
    static void FinishContext(asIScriptContext* ctx);
    static asIScriptContext* RequestContext();
    static void ReturnContext(asIScriptContext* ctx);

    static void SetExceptionCallback(ExceptionCallback callback);
    static void RaiseException(const string& message);
    static void PassException();
    static void HandleException(asIScriptContext* ctx, const string& message);
    static string GetTraceback();
    static string MakeContextTraceback(asIScriptContext* ctx);

    static ScriptInvoker* GetInvoker();
    static string GetDeferredCallsStatistics();
    static void ProcessDeferredCalls();
#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    static bool LoadDeferredCalls();
#endif

    static StrVec GetCustomEntityTypes();
#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    static bool RestoreCustomEntity(const string& type_name, uint id, const DataBase::Document& doc);
#endif

    static EventData* FindInternalEvent(const string& event_name);
    static bool RaiseInternalEvent(EventData* event_ptr, ...);
    static void RemoveEventsEntity(Entity* entity);

    static void HandleRpc(void* context);

    static string GetActiveFuncName();

    static void Watcher(void*);
    static void SetRunTimeout(uint abort_timeout, uint message_timeout);

    static void Define(const string& define);
    static void Undef(const string& define);
    static void CallPragmas(const Pragmas& pragmas);
    static bool LoadRootModule(const ScriptEntryVec& scripts, string& result_code);
    static bool RestoreRootModule(const UCharVec& bytecode, const UCharVec& lnt_data);

    static uint BindByFuncName(const string& func_name, const string& decl, bool is_temp, bool disable_log = false);
    static uint BindByFunc(asIScriptFunction* func, bool is_temp, bool disable_log = false);
    static uint BindByFuncNum(hash func_num, bool is_temp, bool disable_log = false);
    static asIScriptFunction* GetBindFunc(uint bind_id);
    static string GetBindFuncName(uint bind_id);

    static hash GetFuncNum(asIScriptFunction* func);
    static asIScriptFunction* FindFunc(hash func_num);
    static hash BindScriptFuncNumByFuncName(const string& func_name, const string& decl);
    static hash BindScriptFuncNumByFunc(asIScriptFunction* func);
    static uint GetScriptFuncBindId(hash func_num);
    static void PrepareScriptFuncContext(hash func_num, const string& ctx_info);

    static void CacheEnumValues();
    static int GetEnumValue(const string& enum_value_name, bool& fail);
    static int GetEnumValue(const string& enum_name, const string& value_name, bool& fail);
    static string GetEnumValueName(const string& enum_name, int value);

    // Script execution
    static void PrepareContext(uint bind_id, const string& ctx_info);
    static void SetArgUChar(uchar value);
    static void SetArgUShort(ushort value);
    static void SetArgUInt(uint value);
    static void SetArgUInt64(uint64 value);
    static void SetArgBool(bool value);
    static void SetArgFloat(float value);
    static void SetArgDouble(double value);
    static void SetArgObject(void* value);
    static void SetArgEntity(Entity* value);
    static void SetArgAddress(void* value);
    static bool RunPrepared();
    static void RunPreparedSuspend();
    static asIScriptContext* SuspendCurrentContext(uint time);
    static void ResumeContext(asIScriptContext* ctx);
    static void RunSuspended();
    static void RunMandatorySuspended();
    static bool CheckContextEntities(asIScriptContext* ctx);
    static uint GetReturnedUInt();
    static bool GetReturnedBool();
    static void* GetReturnedObject();
    static float GetReturnedFloat();
    static double GetReturnedDouble();
    static void* GetReturnedRawAddress();

    // Logging
    static void Log(const string& str);

    static void CallbackMessage(const asSMessageInfo* msg, void* param);
    static void CallbackException(asIScriptContext* ctx, void* param);

    // Arrays stuff
    static CScriptArray* CreateArray(const string& type);

    template<typename Type>
    static CScriptArray* CreateArrayRef(const string& type, const vector<Type*>& vec)
    {
        CScriptArray* arr = CreateArray(type);
        AppendVectorToArrayRef(vec, arr);
        return arr;
    }

    template<typename Type>
    static void AppendVectorToArray(const vector<Type>& vec, CScriptArray* arr)
    {
        if (!vec.empty() && arr)
        {
            uint i = (uint)arr->GetSize();
            arr->Resize((asUINT)(i + (uint)vec.size()));
            for (uint k = 0, l = (uint)vec.size(); k < l; k++, i++)
            {
                Type* p = (Type*)arr->At(i);
                *p = vec[k];
            }
        }
    }
    template<typename Type>
    static void AppendVectorToArrayRef(const vector<Type>& vec, CScriptArray* arr)
    {
        if (!vec.empty() && arr)
        {
            uint i = (uint)arr->GetSize();
            arr->Resize((asUINT)(i + (uint)vec.size()));
            for (uint k = 0, l = (uint)vec.size(); k < l; k++, i++)
            {
                Type* p = (Type*)arr->At(i);
                *p = vec[k];
                (*p)->AddRef();
            }
        }
    }
    template<typename Type>
    static void AssignScriptArrayInVector(vector<Type>& vec, const CScriptArray* arr)
    {
        if (arr)
        {
            uint count = (uint)arr->GetSize();
            if (count)
            {
                vec.resize(count);
                for (uint i = 0; i < count; i++)
                {
                    Type* p = (Type*)arr->At(i);
                    vec[i] = *p;
                }
            }
        }
    }
};

class CBytecodeStream : public asIBinaryStream
{
private:
    int readPos;
    int writePos;
    std::vector<asBYTE> binBuf;

public:
    CBytecodeStream()
    {
        writePos = 0;
        readPos = 0;
    }
    void Write(const void* ptr, asUINT size)
    {
        if (!ptr || !size)
            return;
        binBuf.resize(binBuf.size() + size);
        memcpy(&binBuf[writePos], ptr, size);
        writePos += size;
    }
    void Read(void* ptr, asUINT size)
    {
        if (!ptr || !size)
            return;
        memcpy(ptr, &binBuf[readPos], size);
        readPos += size;
    }
    std::vector<asBYTE>& GetBuf() { return binBuf; }
};

#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
struct ServerScriptFunctions
{
    EventData* ResourcesGenerated;
    EventData* Init;
    EventData* GenerateWorld;
    EventData* Start;
    EventData* Finish;
    EventData* Loop;
    EventData* GlobalMapCritterIn;
    EventData* GlobalMapCritterOut;

    EventData* LocationInit;
    EventData* LocationFinish;

    EventData* MapInit;
    EventData* MapFinish;
    EventData* MapLoop;
    EventData* MapCritterIn;
    EventData* MapCritterOut;
    EventData* MapCheckLook;
    EventData* MapCheckTrapLook;

    EventData* CritterInit;
    EventData* CritterFinish;
    EventData* CritterIdle;
    EventData* CritterGlobalMapIdle;
    EventData* CritterCheckMoveItem;
    EventData* CritterMoveItem;
    EventData* CritterShow;
    EventData* CritterShowDist1;
    EventData* CritterShowDist2;
    EventData* CritterShowDist3;
    EventData* CritterHide;
    EventData* CritterHideDist1;
    EventData* CritterHideDist2;
    EventData* CritterHideDist3;
    EventData* CritterShowItemOnMap;
    EventData* CritterHideItemOnMap;
    EventData* CritterChangeItemOnMap;
    EventData* CritterMessage;
    EventData* CritterTalk;
    EventData* CritterBarter;
    EventData* CritterGetAttackDistantion;
    EventData* PlayerRegistration;
    EventData* PlayerLogin;
    EventData* PlayerGetAccess;
    EventData* PlayerAllowCommand;
    EventData* PlayerLogout;

    EventData* ItemInit;
    EventData* ItemFinish;
    EventData* ItemWalk;
    EventData* ItemCheckMove;

    EventData* StaticItemWalk;
} extern ServerFunctions;
#endif
#if defined(FONLINE_CLIENT) || defined(FONLINE_EDITOR)
struct ClientScriptFunctions
{
    EventData* Start;
    EventData* Finish;
    EventData* Loop;
    EventData* AutoLogin;
    EventData* GetActiveScreens;
    EventData* ScreenChange;
    EventData* ScreenScroll;
    EventData* RenderIface;
    EventData* RenderMap;
    EventData* MouseDown;
    EventData* MouseUp;
    EventData* MouseMove;
    EventData* KeyDown;
    EventData* KeyUp;
    EventData* InputLost;
    EventData* CritterIn;
    EventData* CritterOut;
    EventData* ItemMapIn;
    EventData* ItemMapChanged;
    EventData* ItemMapOut;
    EventData* ItemInvAllIn;
    EventData* ItemInvIn;
    EventData* ItemInvChanged;
    EventData* ItemInvOut;
    EventData* MapLoad;
    EventData* MapUnload;
    EventData* ReceiveItems;
    EventData* MapMessage;
    EventData* InMessage;
    EventData* OutMessage;
    EventData* MessageBox;
    EventData* CombatResult;
    EventData* ItemCheckMove;
    EventData* CritterAction;
    EventData* Animation2dProcess;
    EventData* Animation3dProcess;
    EventData* CritterAnimation;
    EventData* CritterAnimationSubstitute;
    EventData* CritterAnimationFallout;
    EventData* CritterCheckMoveItem;
    EventData* CritterGetAttackDistantion;
} extern ClientFunctions;

struct MapperScriptFunctions
{
    EventData* Start;
    EventData* Finish;
    EventData* Loop;
    EventData* ConsoleMessage;
    EventData* RenderIface;
    EventData* MouseDown;
    EventData* MouseUp;
    EventData* MouseMove;
    EventData* KeyDown;
    EventData* KeyUp;
    EventData* InputLost;
    EventData* MapLoad;
    EventData* MapSave;
    EventData* InspectorProperties;
} extern MapperFunctions;
#endif
