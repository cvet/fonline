//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include "Common.h"

#include "DataBase.h"
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

DECLARE_EXCEPTION(ScriptSystemException);

#if 1
#define SCRIPT_WATCHER
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

struct EventData;

using EndExecutionCallback = void();
using ContextVec = vector<asIScriptContext*>;
using ExceptionCallback = std::function<void(const string&)>;

struct ScriptEntry
{
    string Name;
    string Path;
    string Content;
    int SortValue;
    int SortValueExt;
};
using ScriptEntryVec = vector<ScriptEntry>;

class BindFunction
{
public:
    asIScriptFunction* ScriptFunc;
    string FuncName;

    BindFunction() { ScriptFunc = nullptr; }

    BindFunction(asIScriptFunction* func)
    {
        ScriptFunc = func;
        FuncName = func->GetDeclaration();
        func->AddRef();
    }

    void Clear()
    {
        if (ScriptFunc)
            ScriptFunc->Release();
        memzero(this, sizeof(BindFunction));
    }
};
using BindFunctionVec = vector<BindFunction>;

struct ContextData
{
    char Info[1024];
    uint StartTick;
    uint SuspendEndTick;
    Entity* EntityArgs[20];
    uint EntityArgsCount;
    asIScriptContext* Parent;
};

class NameResolver
{
public:
    virtual int GetEnumValue(const string& enum_value_name, bool& fail) = 0;
    virtual int GetEnumValue(const string& enum_name, const string& value_name, bool& fail) = 0;
    virtual string GetEnumValueName(const string& enum_name, int value) = 0;
};

// Todo: separate interface for calling script events
// class ScriptEvents

class ScriptSystem : public NameResolver, public NonCopyable
{
public:
    ScriptSystem(ScriptSettings& sett, FileManager& file_mngr);
    ~ScriptSystem();

    bool Init(ScriptPragmaCallback* pragma_callback, const string& target);
    void UnloadScripts();
    bool ReloadScripts(const string& target);
    bool PostInitScriptSystem();
    bool RunModuleInitFunctions();

    void SetExceptionCallback(ExceptionCallback callback);
    void RaiseException(const string& message);
    void PassException();
    void HandleException(asIScriptContext* ctx, const string& message);
    string GetTraceback();
    string MakeContextTraceback(asIScriptContext* ctx);

    asIScriptEngine* GetEngine();
    const Pragmas& GetProcessedPragmas();
    ScriptInvoker* GetInvoker();
    string GetDeferredCallsStatistics();
    void ProcessDeferredCalls();
    // Todo: rework FONLINE_
    /*#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
        bool LoadDeferredCalls();
    #endif*/

    StrVec GetCustomEntityTypes();
    /*#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
        bool RestoreCustomEntity(const string& type_name, uint id, const DataBase::Document& doc);
    #endif*/

    EventData* FindInternalEvent(const string& event_name);
    bool RaiseInternalEvent(EventData* event_ptr, ...);
    void RemoveEventsEntity(Entity* entity);

    void HandleRpc(void* context);

    string GetActiveFuncName();

    void Define(const string& define);
    void Undef(const string& define);
    void CallPragmas(const Pragmas& pragmas);
    bool LoadRootModule(const ScriptEntryVec& scripts, string& result_code);
    bool RestoreRootModule(const UCharVec& bytecode, const UCharVec& lnt_data);

    uint BindByFuncName(const string& func_name, const string& decl, bool is_temp, bool disable_log = false);
    uint BindByFunc(asIScriptFunction* func, bool is_temp, bool disable_log = false);
    uint BindByFuncNum(hash func_num, bool is_temp, bool disable_log = false);
    asIScriptFunction* GetBindFunc(uint bind_id);
    string GetBindFuncName(uint bind_id);

    hash GetFuncNum(asIScriptFunction* func);
    asIScriptFunction* FindFunc(hash func_num);
    hash BindScriptFuncNumByFuncName(const string& func_name, const string& decl);
    hash BindScriptFuncNumByFunc(asIScriptFunction* func);
    uint GetScriptFuncBindId(hash func_num);
    void PrepareScriptFuncContext(hash func_num, const string& ctx_info);

    void CacheEnumValues();
    int GetEnumValue(const string& enum_value_name, bool& fail);
    int GetEnumValue(const string& enum_name, const string& value_name, bool& fail);
    string GetEnumValueName(const string& enum_name, int value);

    void PrepareContext(uint bind_id, const string& ctx_info);
    void SetArgUChar(uchar value);
    void SetArgUShort(ushort value);
    void SetArgUInt(uint value);
    void SetArgUInt64(uint64 value);
    void SetArgBool(bool value);
    void SetArgFloat(float value);
    void SetArgDouble(double value);
    void SetArgObject(void* value);
    void SetArgEntity(Entity* value);
    void SetArgAddress(void* value);
    bool RunPrepared();
    void RunPreparedSuspend();
    asIScriptContext* SuspendCurrentContext(uint time);
    void ResumeContext(asIScriptContext* ctx);
    void RunSuspended();
    void RunMandatorySuspended();
    bool CheckContextEntities(asIScriptContext* ctx);
    uint GetReturnedUInt();
    bool GetReturnedBool();
    void* GetReturnedObject();
    float GetReturnedFloat();
    double GetReturnedDouble();
    void* GetReturnedRawAddress();

    // Logging
    void Log(const string& str);

    void CallbackMessage(const asSMessageInfo* msg, void* param);
    void CallbackException(asIScriptContext* ctx, void* param);

    // Arrays stuff
    CScriptArray* CreateArray(const string& type);

    template<typename Type>
    CScriptArray* CreateArrayRef(const string& type, const vector<Type*>& vec)
    {
        CScriptArray* arr = CreateArray(type);
        AppendVectorToArrayRef(vec, arr);
        return arr;
    }

    template<typename Type>
    void AppendVectorToArray(const vector<Type>& vec, CScriptArray* arr)
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
    void AppendVectorToArrayRef(const vector<Type>& vec, CScriptArray* arr)
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
    void AssignScriptArrayInVector(vector<Type>& vec, const CScriptArray* arr)
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

private:
    void CreateContext();
    void FinishContext(asIScriptContext* ctx);
    asIScriptContext* RequestContext();
    void ReturnContext(asIScriptContext* ctx);
    void Watcher();

    ScriptSettings& settings;
    FileManager& fileMngr;
    asIScriptEngine* asEngine {};
    BindFunctionVec bindedFunctions {};
    HashIntMap scriptFuncBinds {}; // Func Num -> Bind Id
    ExceptionCallback onException {};
    ScriptPragmaCallback* pragmaCB {};
    ScriptInvoker* invoker {};
    StrIntMap cachedEnums {};
    map<string, IntStrMap> cachedEnumNames {};
    asIScriptContext* currentCtx {};
    size_t currentArg {};
    void* retValue {};
    ContextVec freeContexts {};
    ContextVec busyContexts {};
#ifdef SCRIPT_WATCHER
    std::thread scriptWatcherThread {};
    std::atomic_bool scriptWatcherFinish {};
#endif
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
