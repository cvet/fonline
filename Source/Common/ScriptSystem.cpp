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
#include "Log.h"
#include "StringUtils.h"
#include "Timer.h"

ScriptSystem::ScriptSystem(GlobalSettings& settings) : _settings {settings}
{
}

/*void ScriptSystem::UnloadScripts()
{
    for (asUINT i = 0, j = asEngine->GetModuleCount(); i < j; i++)
    {
        asIScriptModule* module = asEngine->GetModuleByIndex(i);
        int result = module->ResetGlobalVars();
        if (result < 0)
            WriteLog("Reset global vars fail, module '{}', error {}", module->GetName(), result);
        result = module->UnbindAllImportedFunctions();
        if (result < 0)
            WriteLog("Unbind fail, module '{}', error {}", module->GetName(), result);
    }

    while (asEngine->GetModuleCount() > 0)
        asEngine->GetModuleByIndex(0)->Discard();
}

bool ScriptSystem::ReloadScripts(string_view target)
{
    WriteLog("Reload scripts...");

    ScriptSystem::UnloadScripts();

    // Combine scripts
    FileCollection fos_files = fileMngr.FilterFiles("fos");
    int file_index = 0;
    int errors = 0;
    ScriptEntryVec scripts;
    while (fos_files.MoveNext())
    {
        File file = fos_files.GetCurFile();
        if (!file)
        {
            WriteLog("Unable to open file '{}'", file.GetName());
            errors++;
            continue;
        }

        // Get first line
        string line = file.GetNonEmptyLine();
        if (line.empty())
        {
            WriteLog("Error in script '{}', file empty", file.GetName());
            errors++;
            continue;
        }

        // Check signature
        if (line.find("FOS") == string::npos)
        {
            WriteLog("Error in script '{}', invalid header '{}'", file.GetName(), line);
            errors++;
            continue;
        }

        // Skip different targets
        if (line.find(target) == string::npos && line.find("Common") == string::npos)
            continue;

        // Sort value
        int sort_value = 0;
        string sort_str = _str(line).substringAfter("Sort ");
        if (!sort_str.empty())
            sort_value = _str(sort_str).toInt();

        // Append
        ScriptEntry entry;
        entry.Name = file.GetName();
        entry.Path = file.GetPath();
        entry.Content = _str("namespace {}{{{}\n}}\n", file.GetName(), file.GetStr());
        entry.SortValue = sort_value;
        entry.SortValueExt = ++file_index;
        scripts.push_back(entry);
    }
    if (errors)
        return false;

    std::sort(scripts.begin(), scripts.end(), [](ScriptEntry& a, ScriptEntry& b) {
        if (a.SortValue == b.SortValue)
            return a.SortValueExt < b.SortValueExt;
        return a.SortValue < b.SortValue;
    });
    if (scripts.empty())
    {
        WriteLog("No scripts found");
        return false;
    }

    // Build
    string result_code;
    if (!LoadRootModule(scripts, result_code))
    {
        WriteLog("Load scripts from files fail");
        return false;
    }

    // Cache enums
    CacheEnumValues();

    // Done
    WriteLog("Reload scripts complete");
    return true;
}

void ScriptSystem::RunModuleInitFunctions()
{
}

void ScriptSystem::CreateContext()
{
    asIScriptContext* ctx = asEngine->CreateContext();
    RUNTIME_ASSERT(ctx);

    int r = ctx->SetExceptionCallback(
        asMETHODPR(ScriptSystem, CallbackException, (asIScriptContext*, void*), void), this, asCALL_THISCALL);
    RUNTIME_ASSERT(r >= 0);

    ContextData* ctx_data = new ContextData();
    memzero(ctx_data, sizeof(ContextData));
    ctx->SetUserData(ctx_data);

    freeContexts.push_back(ctx);
}

void ScriptSystem::FinishContext(asIScriptContext* ctx)
{
    auto it = std::find(freeContexts.begin(), freeContexts.end(), ctx);
    RUNTIME_ASSERT(it != freeContexts.end());
    freeContexts.erase(it);

    delete (ContextData*)ctx->GetUserData();
    ctx->Release();
    ctx = nullptr;
}

asIScriptContext* ScriptSystem::RequestContext()
{
    if (freeContexts.empty())
        CreateContext();

    asIScriptContext* ctx = freeContexts.back();
    freeContexts.pop_back();
    busyContexts.push_back(ctx);
    return ctx;
}

void ScriptSystem::ReturnContext(asIScriptContext* ctx)
{
    auto it = std::find(busyContexts.begin(), busyContexts.end(), ctx);
    RUNTIME_ASSERT(it != busyContexts.end());
    busyContexts.erase(it);
    freeContexts.push_back(ctx);

    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    for (uint i = 0; i < ctx_data->EntityArgsCount; i++)
        ctx_data->EntityArgs[i]->Release();
    memzero(ctx_data, sizeof(ContextData));
}

void ScriptSystem::SetExceptionCallback(ExceptionCallback callback)
{
    onException = callback;
}

void ScriptSystem::RaiseException(string_view message)
{
    asIScriptContext* ctx = asGetActiveContext();
    if (ctx && ctx->GetState() == asEXECUTION_EXCEPTION)
        return;

    if (ctx)
        ctx->SetException(message.c_str());
    else
        HandleException(nullptr, _str("{}", "Engine exception: {}\n", message));
}

void ScriptSystem::PassException()
{
    RaiseException("Pass");
}

void ScriptSystem::HandleException(asIScriptContext* ctx, string_view message)
{
    string buf = message;

    if (ctx)
    {
        buf += "\n";

        asIScriptContext* ctx_ = ctx;
        while (ctx_)
        {
            buf += MakeContextTraceback(ctx_);
            ContextData* ctx_data = (ContextData*)ctx_->GetUserData();
            ctx_ = ctx_data->Parent;
        }
    }

    WriteLog("{}", buf);

    if (onException)
        onException(buf);
}

string ScriptSystem::GetTraceback()
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

asIScriptEngine* ScriptSystem::GetEngine()
{
    return asEngine;
}

const Pragmas& ScriptSystem::GetProcessedPragmas()
{
    return pragmaCB->GetProcessedPragmas();
}

ScriptInvoker* ScriptSystem::GetInvoker()
{
    return invoker;
}

string ScriptSystem::GetDeferredCallsStatistics()
{
    return invoker->GetStatistics();
}

void ScriptSystem::ProcessDeferredCalls()
{
    invoker->Process();
}

/ *#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
bool ScriptSystem::LoadDeferredCalls()
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    return edata->Invoker->LoadDeferredCalls();
}
#endif* /

StrVec ScriptSystem::GetCustomEntityTypes()
{
    return pragmaCB->GetCustomEntityTypes();
}

/ *#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
bool ScriptSystem::RestoreCustomEntity(string_view type_name, uint id, const DataBase::Document& doc)
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    return edata->PragmaCB->RestoreCustomEntity(type_name, id, doc);
}
#endif* /

EventData* ScriptSystem::FindInternalEvent(string_view event_name)
{
    EventData* ev_data = new EventData;
    memzero(ev_data, sizeof(EventData));

    void* as_event = pragmaCB->FindInternalEvent(event_name);
    RUNTIME_ASSERT(as_event);
    ev_data->ASEvent = as_event;
    return ev_data;
}

bool ScriptSystem::RaiseInternalEvent(EventData* ev_data, ...)
{
    va_list args;
    va_start(args, ev_data);
    bool result = pragmaCB->RaiseInternalEvent(ev_data->ASEvent, args);
    va_end(args);
    return result;
}

void ScriptSystem::RemoveEventsEntity(Entity* entity)
{
    pragmaCB->RemoveEventsEntity(entity);
}

void ScriptSystem::HandleRpc(void* context)
{
    pragmaCB->HandleRpc(context);
}

string ScriptSystem::GetActiveFuncName()
{
    asIScriptContext* ctx = asGetActiveContext();
    if (!ctx)
        return "";
    asIScriptFunction* func = ctx->GetFunction(0);
    if (!func)
        return "";
    return func->GetName();
}

void ScriptSystem::Define(string_view define)
{
    Preprocessor::Define(define);
}

void ScriptSystem::Undef(string_view define)
{
    if (!define.empty())
        Preprocessor::Undef(define);
    else
        Preprocessor::UndefAll();
}

void ScriptSystem::CallPragmas(const Pragmas& pragmas)
{
    Preprocessor::SetPragmaCallback(pragmaCB);

    for (size_t i = 0; i < pragmas.size(); i++)
        Preprocessor::CallPragma(pragmas[i]);
}

bool ScriptSystem::LoadRootModule(const ScriptEntryVec& scripts, string& result_code)
{
    RUNTIME_ASSERT(asEngine->GetModuleCount() == 0);

    Preprocessor::SetPragmaCallback(pragmaCB);

    class MemoryFileLoader : public Preprocessor::FileLoader
    {
        string* rootScript;
        ScriptEntryVec includeScripts;
        int includeDeep;

    public:
        MemoryFileLoader(string& root, const ScriptEntryVec& scripts) :
            rootScript(&root), includeScripts(scripts), includeDeep(0)
        {
        }
        virtual ~MemoryFileLoader() = default;

        virtual bool LoadFile(string_view dir, string_view file_name, std::vector<char>& data,
            std::string& file_path) override
        {
            if (rootScript)
            {
                data.resize(rootScript->length());
                std::memcpy(&data[0], rootScript->c_str(), rootScript->length());
                rootScript = nullptr;
                file_path = "(Root)";
                return true;
            }

            includeDeep++;
            RUNTIME_ASSERT(includeDeep <= 1);

            if (includeDeep == 1 && !includeScripts.empty())
            {
                data.resize(includeScripts.front().Content.length());
                std::memcpy(&data[0], includeScripts.front().Content.c_str(), includeScripts.front().Content.length());
                file_path = includeScripts.front().Name;
                includeScripts.erase(includeScripts.begin());
                return true;
            }

            bool loaded = Preprocessor::FileLoader::LoadFile(dir, file_name, data, file_path);
            if (loaded)
            {
                file_path = includeScripts.front().Path;
                DiskFileSystem::ResolvePath(file_path);
            }
            return loaded;
        }

        virtual void FileLoaded() override { includeDeep--; }
    };

    // Make Root scripts
    string root;
    for (auto& script : scripts)
        root += _str("#include \"{}\"\n", script.Name);

    // Set preprocessor defines from command line
    StrMap config; // Todo: fill settings to scripts
    // const StrMap& config = MainConfig->GetApp("");
    for (auto& kv : config)
    {
        if (kv.first.length() > 2 && kv.first[0] == '-' && kv.first[1] == '-')
            Preprocessor::Define(kv.first.substr(2));
    }

    // Preprocess
    MemoryFileLoader loader(root, scripts);
    Preprocessor::StringOutStream result, errors;
    int errors_count = Preprocessor::Preprocess("Root", result, &errors, &loader);

    if (errors.String != "")
    {
        while (errors.String[errors.String.length() - 1] == '\n')
            errors.String.pop_back();
        WriteLog("Preprocessor message '{}'", errors.String);
    }

    if (errors_count)
    {
        WriteLog("Unable to preprocess");
        return false;
    }

    // Set global properties from command line
    for (auto& kv : config)
    {
        // Skip defines
        if (kv.first.length() > 2 && kv.first[0] == '-' && kv.first[1] == '-')
            continue;

        // Find property, with prefix and without
        int index = asEngine->GetGlobalPropertyIndexByName(("__" + kv.first).c_str());
        if (index < 0)
        {
            index = asEngine->GetGlobalPropertyIndexByName(kv.first.c_str());
            if (index < 0)
                continue;
        }

        int type_id;
        void* ptr;
        int r = asEngine->GetGlobalPropertyByIndex(index, nullptr, nullptr, &type_id, nullptr, nullptr, &ptr, nullptr);
        RUNTIME_ASSERT(r >= 0);

        // Try set value
        asITypeInfo* obj_type = (type_id & asTYPEID_MASK_OBJECT ? asEngine->GetTypeInfoById(type_id) : nullptr);
        bool is_hashes[] = {false, false, false, false};
        uchar pod_buf[8];
        bool is_error = false;
        void* value = ReadValue(kv.second.c_str(), type_id, obj_type, is_hashes, 0, pod_buf, is_error);
        if (!is_error)
        {
            if (!obj_type)
            {
                std::memcpy(ptr, value, asEngine->GetSizeOfPrimitiveType(type_id));
            }
            else if (type_id & asTYPEID_OBJHANDLE)
            {
                if (*(void**)ptr)
                    asEngine->ReleaseScriptObject(*(void**)ptr, obj_type);
                *(void**)ptr = value;
            }
            else
            {
                asEngine->AssignScriptObject(ptr, value, obj_type);
                asEngine->ReleaseScriptObject(value, obj_type);
            }
        }
    }

    // Add new
    asIScriptModule* module = asEngine->GetModule("Root", asGM_ALWAYS_CREATE);
    if (!module)
    {
        WriteLog("Create 'Root' module fail");
        return false;
    }

    // Store line number translator
    Preprocessor::LineNumberTranslator* lnt = Preprocessor::GetLineNumberTranslator();
    UCharVec lnt_data;
    Preprocessor::StoreLineNumberTranslator(lnt, lnt_data);
    module->SetUserData(lnt);

    // Add single script section
    int as_result = module->AddScriptSection("Root", result.String.c_str());
    if (as_result < 0)
    {
        WriteLog("Unable to add script section, result {}", as_result);
        module->Discard();
        return false;
    }

    // Build module
    as_result = module->Build();
    if (as_result < 0)
    {
        WriteLog("Unable to build module, result {}", as_result);
        module->Discard();
        return false;
    }

    result_code = result.String;
    return true;
}

bool ScriptSystem::RestoreRootModule(const UCharVec& bytecode, const UCharVec& lnt_data)
{
    RUNTIME_ASSERT(asEngine->GetModuleCount() == 0);
    RUNTIME_ASSERT(!bytecode.empty());
    RUNTIME_ASSERT(!lnt_data.empty());

    asIScriptModule* module = asEngine->GetModule("Root", asGM_ALWAYS_CREATE);
    if (!module)
    {
        WriteLog("Create 'Root' module fail");
        return false;
    }

    Preprocessor::LineNumberTranslator* lnt = Preprocessor::RestoreLineNumberTranslator(lnt_data);
    module->SetUserData(lnt);

    CBytecodeStream binary;
    binary.Write(&bytecode[0], (asUINT)bytecode.size());
    int result = module->LoadByteCode(&binary);
    if (result < 0)
    {
        WriteLog("Can't load binary, result {}", result);
        module->Discard();
        return false;
    }

    return true;
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

uint ScriptSystem::GetReturnedUInt()
{
    return *(uint*)retValue;
}

bool ScriptSystem::GetReturnedBool()
{
    return *(bool*)retValue;
}

void* ScriptSystem::GetReturnedObject()
{
    return *(void**)retValue;
}

float ScriptSystem::GetReturnedFloat()
{
    return *(float*)retValue;
}

double ScriptSystem::GetReturnedDouble()
{
    return *(double*)retValue;
}

void* ScriptSystem::GetReturnedRawAddress()
{
    return retValue;
}

void ScriptSystem::Log(string_view str)
{
    asIScriptContext* ctx = asGetActiveContext();
    if (!ctx)
    {
        WriteLog("<unknown> : {}", str);
        return;
    }
    asIScriptFunction* func = ctx->GetFunction(0);
    if (!func)
    {
        WriteLog("<unknown> : {}", str);
        return;
    }

    int line = ctx->GetLineNumber(0);
    Preprocessor::LineNumberTranslator* lnt = (Preprocessor::LineNumberTranslator*)func->GetModule()->GetUserData();
    WriteLog("{} : {}", Preprocessor::ResolveOriginalFile(line, lnt), str);
}

void ScriptSystem::CallbackMessage(const asSMessageInfo* msg, void* param)
{
    const char* type = "Error";
    if (msg->type == asMSGTYPE_WARNING)
        type = "Warning";
    else if (msg->type == asMSGTYPE_INFORMATION)
        type = "Info";

    WriteLog("{} : {} : {} : Line {}", Preprocessor::ResolveOriginalFile(msg->row), type, msg->message,
        Preprocessor::ResolveOriginalLine(msg->row));
}

void ScriptSystem::CallbackException(asIScriptContext* ctx, void* param)
{
    string str = ctx->GetExceptionString();
    if (str != "Pass")
        HandleException(ctx, _str("Script exception: {}{}", str, !_str(str).endsWith('.') ? "." : ""));
}

CScriptArray* ScriptSystem::CreateArray(string_view type)
{
    return CScriptArray::Create(asEngine->GetTypeInfoById(asEngine->GetTypeIdByDecl(type.c_str())));
}
*/

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
