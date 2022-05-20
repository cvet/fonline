//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "ScriptSystem.h"
#include "EngineBase.h"

auto ScriptSystem::ValidateArgs(const GenericScriptFunc& gen_func, initializer_list<const type_info*> args_type, const type_info* ret_type) -> bool
{
    if (gen_func.CallNotSupported) {
        return false;
    }

    if (gen_func.RetType != ret_type) {
        return false;
    }
    if (gen_func.ArgsType.size() != args_type.size()) {
        return false;
    }

    size_t index = 0;
    for (const auto* arg_type : args_type) {
        if (arg_type != gen_func.ArgsType[index]) {
            return false;
        }
        ++index;
    }

    return true;
}

void ScriptSystem::InitModules()
{
    NON_CONST_METHOD_HINT();

    for (const auto* func : _initFunc) {
        if (!func->Call({}, nullptr)) {
            throw ScriptSystemException("Module initialization failed");
        }
    }
}

auto ScriptHelpers::GetIntConvertibleEntityProperty(const FOEngineBase* engine, string_view class_name, int prop_index) -> const Property*
{
    const auto* prop_reg = engine->GetPropertyRegistrator(class_name);
    RUNTIME_ASSERT(prop_reg);
    const auto* prop = prop_reg->GetByIndex(static_cast<int>(prop_index));
    if (prop == nullptr) {
        throw ScriptException("Invalid property index", class_name, prop_index);
    }
    if (!prop->IsReadable()) {
        throw ScriptException("Property is not readable", class_name, prop_index);
    }
    if (!prop->IsPlainData()) {
        throw ScriptException("Property is not plain data", class_name, prop_index);
    }
    return prop;
}

// Todo: remove commented code
/*
public:
void RunModuleInitFunctions();

string GetDeferredCallsStatistics();
void ProcessDeferredCalls();
/ *#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    bool LoadDeferredCalls();
#endif* /

StrVec GetCustomEntityTypes();
/ *#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    bool RestoreCustomEntity(string_view type_name, uint id, const DataBase::Document& doc);
#endif* /

void RemoveEventsEntity(Entity* entity);

void HandleRpc(void* context);

string GetActiveFuncName();

uint BindByFuncName(string_view func_name, string_view decl, bool is_temp, bool disable_log = false);
// uint BindByFunc(asIScriptFunction* func, bool is_temp, bool disable_log = false);
uint BindByFuncNum(hash func_num, bool is_temp, bool disable_log = false);
// asIScriptFunction* GetBindFunc(uint bind_id);
string GetBindFuncName(uint bind_id);

// hash GetFuncNum(asIScriptFunction* func);
// asIScriptFunction* FindFunc(hash func_num);
hash BindScriptFuncNumByFuncName(string_view func_name, string_view decl);
// hash BindScriptFuncNumByFunc(asIScriptFunction* func);
uint GetScriptFuncBindId(hash func_num);
void PrepareScriptFuncContext(hash func_num, string_view ctx_info);

void CacheEnumValues();

void PrepareContext(uint bind_id, string_view ctx_info);
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
// asIScriptContext* SuspendCurrentContext(uint time);
// void ResumeContext(asIScriptContext* ctx);
void RunSuspended();
void RunMandatorySuspended();
// bool CheckContextEntities(asIScriptContext* ctx);
uint GetReturnedUInt();
bool GetReturnedBool();
void* GetReturnedObject();
float GetReturnedFloat();
double GetReturnedDouble();
void* GetReturnedRawAddress();

private:
HashIntMap scriptFuncBinds {}; // Func Num -> Bind Id
StrIntMap cachedEnums {};
map<string, IntStrMap> cachedEnumNames {};*/

/*string ScriptSystem::GetTraceback()
{
    string result = "";
    ContextVec contexts = busyContexts;
    for (int i = (int)contexts.size() - 1; i >= 0; i--)
    {
        result += MakeContextTraceback(contexts[i]);
        result += "\n";
    }
    return result;
}

string ScriptSystem::MakeContextTraceback(asIScriptContext* ctx)
{
    string result = "";
    int line;
    const asIScriptFunction* func;
    int stack_size = ctx->GetCallstackSize();

    // Print system function
    asIScriptFunction* sys_func = ctx->GetSystemFunction();
    if (sys_func)
        result += _str("\t{}\n", sys_func->GetDeclaration(true, true, true));

    // Print current function
    if (ctx->GetState() == asEXECUTION_EXCEPTION)
    {
        line = ctx->GetExceptionLineNumber();
        func = ctx->GetExceptionFunction();
    }
    else
    {
        line = ctx->GetLineNumber(0);
        func = ctx->GetFunction(0);
    }
    if (func)
    {
        Preprocessor::LineNumberTranslator* lnt = (Preprocessor::LineNumberTranslator*)func->GetModule()->GetUserData();
        result +=
            _str("\t{} : Line {}\n", func->GetDeclaration(true, true), Preprocessor::ResolveOriginalLine(line, lnt));
    }

    // Print call stack
    for (int i = 1; i < stack_size; i++)
    {
        func = ctx->GetFunction(i);
        line = ctx->GetLineNumber(i);
        if (func)
        {
            Preprocessor::LineNumberTranslator* lnt =
                (Preprocessor::LineNumberTranslator*)func->GetModule()->GetUserData();
            result += _str(
                "\t{} : Line {}\n", func->GetDeclaration(true, true), Preprocessor::ResolveOriginalLine(line, lnt));
        }
    }

    return result;
}

uint ScriptSystem::BindByFuncName(
    string_view func_name, string_view decl, bool is_temp, bool disable_log)
{
    // Collect functions in all modules
    RUNTIME_ASSERT(asEngine->GetModuleCount() == 1);

    // Find function
    string func_decl;
    if (!decl.empty())
        func_decl = _str(decl).replace("%s", func_name);
    else
        func_decl = func_name;

    asIScriptModule* module = asEngine->GetModuleByIndex(0);
    asIScriptFunction* func = module->GetFunctionByDecl(func_decl.c_str());
    if (!func)
    {
        if (!disable_log)
            WriteLog("Function '{}' not found", func_decl);
        return 0;
    }

    // Save to temporary bind
    if (is_temp)
    {
        bindedFunctions[1].ScriptFunc = func;
        return 1;
    }

    // Find already binded
    for (int i = 2, j = (int)bindedFunctions.size(); i < j; i++)
    {
        BindFunction& bf = bindedFunctions[i];
        if (bf.ScriptFunc == func)
            return i;
    }

    // Create new bind
    bindedFunctions.push_back(BindFunction(func));
    return (uint)bindedFunctions.size() - 1;
}

uint ScriptSystem::BindByFunc(asIScriptFunction* func, bool is_temp, bool disable_log)
{
    // Save to temporary bind
    if (is_temp)
    {
        bindedFunctions[1].ScriptFunc = func;
        return 1;
    }

    // Find already binded
    for (int i = 2, j = (int)bindedFunctions.size(); i < j; i++)
    {
        if (bindedFunctions[i].ScriptFunc == func)
            return i;
    }

    // Create new bind
    bindedFunctions.push_back(BindFunction(func));
    return (uint)bindedFunctions.size() - 1;
}

uint ScriptSystem::BindByFuncNum(hash func_num, bool is_temp, bool disable_log)
{
    asIScriptFunction* func = FindFunc(func_num);
    if (!func)
    {
        if (!disable_log)
            WriteLog("Function '{}' not found", _str().parseHash(func_num));
        return 0;
    }

    return BindByFunc(func, is_temp, disable_log);
}

asIScriptFunction* ScriptSystem::GetBindFunc(uint bind_id)
{
    RUNTIME_ASSERT(bind_id);
    RUNTIME_ASSERT(bind_id < bindedFunctions.size());

    return bindedFunctions[bind_id].ScriptFunc;
}

string ScriptSystem::GetBindFuncName(uint bind_id)
{
    if (!bind_id || bind_id >= (uint)bindedFunctions.size())
    {
        WriteLog("Wrong bind id {}, bind buffer size {}", bind_id, bindedFunctions.size());
        return "";
    }

    return bindedFunctions[bind_id].FuncName;
}

hash ScriptSystem::GetFuncNum(asIScriptFunction* func)
{
    hash* func_num_ptr = (hash*)func->GetUserData();
    if (!func_num_ptr)
    {
        string script_name = _str("{}::{}", func->GetNamespace(), func->GetName());
        func_num_ptr = new hash(_str(script_name).toHash());
        func->SetUserData(func_num_ptr);
    }
    return *func_num_ptr;
}

asIScriptFunction* ScriptSystem::FindFunc(hash func_num)
{
    for (asUINT i = 0, j = asEngine->GetModuleCount(); i < j; i++)
    {
        asIScriptModule* module = asEngine->GetModuleByIndex(i);
        for (asUINT n = 0, m = module->GetFunctionCount(); n < m; n++)
        {
            asIScriptFunction* func = module->GetFunctionByIndex(n);
            if (GetFuncNum(func) == func_num)
                return func;
        }
    }
    return nullptr;
}

hash ScriptSystem::BindScriptFuncNumByFuncName(string_view func_name, string_view decl)
{
    // Bind function
    int bind_id = ScriptSystem::BindByFuncName(func_name, decl, false);
    if (bind_id <= 0)
        return 0;

    // Native and broken binds not allowed
    BindFunction& bf = bindedFunctions[bind_id];
    if (!bf.ScriptFunc)
        return 0;

    // Get func num
    hash func_num = GetFuncNum(bf.ScriptFunc);

    // Duplicate checking
    auto it = scriptFuncBinds.find(func_num);
    if (it != scriptFuncBinds.end() && it->second != bind_id)
        return 0;

    // Store
    if (it == scriptFuncBinds.end())
        scriptFuncBinds.insert(std::make_pair(func_num, bind_id));

    return func_num;
}

hash ScriptSystem::BindScriptFuncNumByFunc(asIScriptFunction* func)
{
    // Bind function
    int bind_id = ScriptSystem::BindByFunc(func, false);
    if (bind_id <= 0)
        return 0;

    // Get func num
    hash func_num = GetFuncNum(func);

    // Duplicate checking
    auto it = scriptFuncBinds.find(func_num);
    if (it != scriptFuncBinds.end() && it->second != bind_id)
        return 0;

    // Store
    if (it == scriptFuncBinds.end())
        scriptFuncBinds.insert(std::make_pair(func_num, bind_id));

    return func_num;
}

uint ScriptSystem::GetScriptFuncBindId(hash func_num)
{
    if (!func_num)
        return 0;

    // Indexing by hash
    auto it = scriptFuncBinds.find(func_num);
    if (it != scriptFuncBinds.end())
        return it->second;

    // Function not binded, try find and bind it
    asIScriptFunction* func = FindFunc(func_num);
    if (!func)
        return 0;

    // Bind
    func_num = ScriptSystem::BindScriptFuncNumByFunc(func);
    if (func_num)
        return scriptFuncBinds[func_num];

    return 0;
}

void ScriptSystem::PrepareScriptFuncContext(hash func_num, string_view ctx_info)
{
    uint bind_id = GetScriptFuncBindId(func_num);
    PrepareContext(bind_id, ctx_info);
}

void ScriptSystem::CacheEnumValues()
{
    // Engine enums
    cachedEnums.clear();
    cachedEnumNames.clear();
    for (asUINT i = 0, j = asEngine->GetEnumCount(); i < j; i++)
    {
        asITypeInfo* enum_type = asEngine->GetEnumByIndex(i);
        const char* enum_ns = enum_type->GetNamespace();
        enum_ns = (enum_ns && enum_ns[0] ? enum_ns : nullptr);
        const char* enum_name = enum_type->GetName();
        string name = _str("{}{}{}", enum_ns ? enum_ns : "", enum_ns ? "::" : "", enum_name);
        IntStrMap& value_names = cachedEnumNames.insert(std::make_pair(name, IntStrMap())).first->second;
        for (asUINT k = 0, l = enum_type->GetEnumValueCount(); k < l; k++)
        {
            int value;
            string value_name = enum_type->GetEnumValueByIndex(k, &value);
            name = _str("{}{}{}::{}", enum_ns ? enum_ns : "", enum_ns ? "::" : "", enum_name, value_name);
            cachedEnums.insert(std::make_pair(name, value));
            value_names.insert(std::make_pair(value, value_name));
        }
    }

    // Script enums
    RUNTIME_ASSERT(asEngine->GetModuleCount() == 1);
    asIScriptModule* module = asEngine->GetModuleByIndex(0);
    for (asUINT i = 0; i < module->GetEnumCount(); i++)
    {
        asITypeInfo* enum_type = module->GetEnumByIndex(i);
        const char* enum_ns = enum_type->GetNamespace();
        enum_ns = (enum_ns && enum_ns[0] ? enum_ns : nullptr);
        const char* enum_name = enum_type->GetName();
        string name = _str("{}{}{}", enum_ns ? enum_ns : "", enum_ns ? "::" : "", enum_name);
        IntStrMap& value_names = cachedEnumNames.insert(std::make_pair(name, IntStrMap())).first->second;
        for (asUINT k = 0, l = enum_type->GetEnumValueCount(); k < l; k++)
        {
            int value;
            string value_name = enum_type->GetEnumValueByIndex(k, &value);
            name = _str("{}{}{}::{}", enum_ns ? enum_ns : "", enum_ns ? "::" : "", enum_name, value_name);
            cachedEnums.insert(std::make_pair(name, value));
            value_names.insert(std::make_pair(value, value_name));
        }
    }

    // Content
    for (asUINT i = 0, j = asEngine->GetGlobalPropertyCount(); i < j; i++)
    {
        const char* name;
        const char* ns;
        int type_id;
        hash* value;
        asEngine->GetGlobalPropertyByIndex(i, &name, &ns, &type_id, nullptr, nullptr, (void**)&value);
        if (_str(ns).startsWith("Content"))
        {
            RUNTIME_ASSERT(type_id == asTYPEID_UINT32); // hash
            cachedEnums.insert(std::make_pair(_str("{}::{}", ns, name), (int)*value));
        }
    }
}

int ScriptSystem::ResolveEnumValue(string_view enum_value_name, bool& fail)
{
    auto it = cachedEnums.find(enum_value_name);
    if (it == cachedEnums.end())
    {
        WriteLog("Enum value '{}' not found", enum_value_name);
        fail = true;
        return 0;
    }
    return it->second;
}

int ScriptSystem::ResolveEnumValue(string_view enum_name, string_view value_name, bool& fail)
{
    if (_str(value_name).isNumber())
        return _str(value_name).toInt();
    return ResolveEnumValue(_str("{}::{}", enum_name, value_name), fail);
}

string ScriptSystem::ResolveEnumValueName(string_view enum_name, int value)
{
    auto it = cachedEnumNames.find(enum_name);
    RUNTIME_ASSERT(it != cachedEnumNames.end());
    auto it_value = it->second.find(value);
    return it_value != it->second.end() ? it_value->second : _str("{}", value).str();
}

void ScriptSystem::PrepareContext(uint bind_id, string_view ctx_info)
{
    RUNTIME_ASSERT(bind_id > 0);
    RUNTIME_ASSERT(bind_id < (uint)bindedFunctions.size());

    BindFunction& bf = bindedFunctions[bind_id];
    asIScriptFunction* script_func = bf.ScriptFunc;

    RUNTIME_ASSERT(script_func);

    asIScriptContext* ctx = RequestContext();
    RUNTIME_ASSERT(ctx);

    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    Str::Copy(ctx_data->Info, ctx_info.c_str());

    int result = ctx->Prepare(script_func);
    RUNTIME_ASSERT(result >= 0);

    RUNTIME_ASSERT(!currentCtx);
    currentCtx = ctx;

    currentArg = 0;
}

void ScriptSystem::SetArgUChar(uchar value)
{
    currentCtx->SetArgByte((asUINT)currentArg, value);
    currentArg++;
}

void ScriptSystem::SetArgUShort(ushort value)
{
    currentCtx->SetArgWord((asUINT)currentArg, value);
    currentArg++;
}

void ScriptSystem::SetArgUInt(uint value)
{
    currentCtx->SetArgDWord((asUINT)currentArg, value);
    currentArg++;
}

void ScriptSystem::SetArgUInt64(uint64 value)
{
    currentCtx->SetArgQWord((asUINT)currentArg, value);
    currentArg++;
}

void ScriptSystem::SetArgBool(bool value)
{
    currentCtx->SetArgByte((asUINT)currentArg, value);
    currentArg++;
}

void ScriptSystem::SetArgFloat(float value)
{
    currentCtx->SetArgFloat((asUINT)currentArg, value);
    currentArg++;
}

void ScriptSystem::SetArgDouble(double value)
{
    currentCtx->SetArgDouble((asUINT)currentArg, value);
    currentArg++;
}

void ScriptSystem::SetArgObject(void* value)
{
    currentCtx->SetArgObject((asUINT)currentArg, value);
    currentArg++;
}

void ScriptSystem::SetArgEntity(Entity* value)
{
    RUNTIME_ASSERT(!value || !value->IsDestroyed);

    if (value)
    {
        ContextData* ctx_data = (ContextData*)currentCtx->GetUserData();
        value->AddRef();
        RUNTIME_ASSERT(ctx_data->EntityArgsCount < 20);
        ctx_data->EntityArgs[ctx_data->EntityArgsCount] = value;
        ctx_data->EntityArgsCount++;
    }

    SetArgObject(value);
}

void ScriptSystem::SetArgAddress(void* value)
{
    currentCtx->SetArgAddress((asUINT)currentArg, value);
    currentArg++;
}

bool ScriptSystem::RunPrepared()
{
    RUNTIME_ASSERT(currentCtx);
    asIScriptContext* ctx = currentCtx;
    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    uint tick = Timer::FrameTick();
    currentCtx = nullptr;
    ctx_data->StartTick = tick;
    ctx_data->Parent = asGetActiveContext();

    int result = ctx->Execute();

#if SCRIPT_WATCHER
    uint delta = Timer::FrameTick() - tick;
#endif

    asEContextState state = ctx->GetState();
    if (state == asEXECUTION_SUSPENDED)
    {
        retValue = 0;
        return true;
    }
    else if (state != asEXECUTION_FINISHED)
    {
        if (state != asEXECUTION_EXCEPTION)
        {
            if (state == asEXECUTION_ABORTED)
                HandleException(ctx, "Execution of script aborted (due to timeout).");
            else
                HandleException(ctx, _str("Execution of script stopped due to {}.", ContextStatesStr[(int)state]));
        }
        ctx->Abort();
        ReturnContext(ctx);
        return false;
    }

    if (result < 0)
    {
        WriteLog("Context '{}' execute error {}, state '{}'", ctx_data->Info, result, ContextStatesStr[(int)state]);
        ctx->Abort();
        ReturnContext(ctx);
        return false;
    }

    retValue = ctx->GetAddressOfReturnValue();

    ReturnContext(ctx);

    return true;
}

void ScriptSystem::RunPreparedSuspend()
{
    RUNTIME_ASSERT(currentCtx);

    asIScriptContext* ctx = currentCtx;
    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    uint tick = Timer::FrameTick();
    currentCtx = nullptr;
    ctx_data->StartTick = tick;
    ctx_data->Parent = asGetActiveContext();
    retValue = 0;
}

asIScriptContext* ScriptSystem::SuspendCurrentContext(uint time)
{
    asIScriptContext* ctx = asGetActiveContext();
    RUNTIME_ASSERT(std::find(busyContexts.begin(), busyContexts.end(), ctx) != busyContexts.end());
    if (ctx->GetFunction(ctx->GetCallstackSize() - 1)->GetReturnTypeId() != asTYPEID_VOID)
        throw ScriptSystemException("Can't yield context which must return value");

    ctx->Suspend();
    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    ctx_data->SuspendEndTick = (time != uint(-1) ? (time ? Timer::FrameTick() + time : 0) : uint(-1));
    return ctx;
}

void ScriptSystem::ResumeContext(asIScriptContext* ctx)
{
    RUNTIME_ASSERT(ctx->GetState() == asEXECUTION_SUSPENDED);
    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    RUNTIME_ASSERT(ctx_data->SuspendEndTick == uint(-1));
    ctx_data->SuspendEndTick = Timer::FrameTick();
}

void ScriptSystem::RunSuspended()
{
    if (busyContexts.empty())
        return;

    // Collect contexts to resume
    ContextVec resume_contexts;
    uint tick = Timer::FrameTick();
    for (auto it = busyContexts.begin(), end = busyContexts.end(); it != end; ++it)
    {
        asIScriptContext* ctx = *it;
        ContextData* ctx_data = (ContextData*)ctx->GetUserData();
        if ((ctx->GetState() == asEXECUTION_PREPARED || ctx->GetState() == asEXECUTION_SUSPENDED) &&
            ctx_data->SuspendEndTick != uint(-1) && tick >= ctx_data->SuspendEndTick)
        {
            resume_contexts.push_back(ctx);
        }
    }

    if (resume_contexts.empty())
        return;

    // Resume
    for (auto& ctx : resume_contexts)
    {
        if (!CheckContextEntities(ctx))
        {
            ReturnContext(ctx);
            continue;
        }

        currentCtx = ctx;
        RunPrepared();
    }
}

void ScriptSystem::RunMandatorySuspended()
{
    uint i = 0;
    while (true)
    {
        if (busyContexts.empty())
            break;

        // Collect contexts to resume
        ContextVec resume_contexts;
        for (auto it = busyContexts.begin(), end = busyContexts.end(); it != end; ++it)
        {
            asIScriptContext* ctx = *it;
            ContextData* ctx_data = (ContextData*)ctx->GetUserData();
            if ((ctx->GetState() == asEXECUTION_PREPARED || ctx->GetState() == asEXECUTION_SUSPENDED) &&
                ctx_data->SuspendEndTick == 0)
                resume_contexts.push_back(ctx);
        }

        if (resume_contexts.empty())
            break;

        // Resume
        for (auto& ctx : resume_contexts)
        {
            if (!CheckContextEntities(ctx))
            {
                ReturnContext(ctx);
                continue;
            }

            currentCtx = ctx;
            RunPrepared();
        }

        // Detect recursion
        if (++i % 10000 == 0)
            WriteLog("Big loops in suspended contexts");
    }
}

bool ScriptSystem::CheckContextEntities(asIScriptContext* ctx)
{
    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    for (uint i = 0; i < ctx_data->EntityArgsCount; i++)
        if (ctx_data->EntityArgs[i]->IsDestroyed)
            return false;
    return true;
}
*/
