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

#include "Script.h"
#include "AngelScriptExt/reflection.h"
#include "FileSystem.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"

#include "datetime/datetime.h"
#include "mono/dis/meta.h"
#include "mono/metadata/assembly.h"
#include "mono/metadata/debug-helpers.h"
#include "mono/metadata/mono-config.h"
#include "mono/mini/jit.h"
#include "preprocessor.h"
#include "scriptany/scriptany.h"
#include "scriptfile/scriptfile.h"
#include "scriptfile/scriptfilesystem.h"
#include "scripthelper/scripthelper.h"
#include "scriptmath/scriptmath.h"
#include "scriptstdstring/scriptstdstring.h"
#include "weakref/weakref.h"

static FileManager* FileMngr;
static map<string, MonoImage*> EngineAssemblyImages;
static void SetMonoInternalCalls();
static MonoAssembly* LoadNetAssembly(const string& name);
static MonoAssembly* LoadGameAssembly(const string& name, map<string, MonoImage*>& assembly_images);
static bool CompileGameAssemblies(const string& target, map<string, MonoImage*>& assembly_images);

static const string ContextStatesStr[] = {
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
typedef vector<BindFunction> BindFunctionVec;

static ScriptSettings* Settings;
static asIScriptEngine* Engine;
static BindFunctionVec BindedFunctions;
static HashIntMap ScriptFuncBinds; // Func Num -> Bind Id
static ExceptionCallback OnException;

// Contexts
struct ContextData
{
    char Info[1024];
    uint StartTick;
    uint SuspendEndTick;
    Entity* EntityArgs[20];
    uint EntityArgsCount;
    asIScriptContext* Parent;
};
static ContextVec FreeContexts;
static ContextVec BusyContexts;

// Script watcher
#if 0
#define SCRIPT_WATCHER
#endif
#ifdef SCRIPT_WATCHER
static Thread ScriptWatcherThread;
static bool ScriptWatcherFinish = false;
static uint RunTimeoutAbort = 600000; // 10 minutes
static uint RunTimeoutMessage = 300000; // 5 minutes
#endif

struct EventData
{
    void* ASEvent;
    MonoMethod* MMethod;
};

ServerScriptFunctions ServerFunctions;
ClientScriptFunctions ClientFunctions;
MapperScriptFunctions MapperFunctions;

bool Script::Init(
    ScriptSettings& sett, FileManager& file_mngr, ScriptPragmaCallback* pragma_callback, const string& target)
{
    Settings = &sett;
    FileMngr = &file_mngr;

    Engine = CreateEngine(pragma_callback, target);
    if (!Engine)
    {
        WriteLog("Can't create AS engine.\n");
        return false;
    }

    if (!BindedFunctions.empty())
        BindedFunctions[1].ScriptFunc = nullptr;
    for (auto it = BindedFunctions.begin(), end = BindedFunctions.end(); it != end; ++it)
        it->Clear();
    BindedFunctions.clear();
    BindedFunctions.reserve(10000);
    BindedFunctions.push_back(BindFunction()); // None
    BindedFunctions.push_back(BindFunction()); // Temp
    ScriptFuncBinds.clear();

    Engine->SetModuleUserDataCleanupCallback([](asIScriptModule* module) {
        Preprocessor::DeleteLineNumberTranslator((Preprocessor::LineNumberTranslator*)module->GetUserData());
        module->SetUserData(nullptr);
    });
    Engine->SetFunctionUserDataCleanupCallback([](asIScriptFunction* func) {
        hash* func_num_ptr = (hash*)func->GetUserData();
        if (func_num_ptr)
        {
            delete func_num_ptr;
            func->SetUserData(nullptr);
        }
    });

    while (FreeContexts.size() < 10)
        CreateContext();

#ifdef SCRIPT_WATCHER
    ScriptWatcherFinish = false;
    ScriptWatcherThread.Start(Script::Watcher, "ScriptWatcher");
#endif
    return true;
}

bool Script::InitMono(const string& target, map<string, UCharVec>* assemblies_data)
{
    g_set_print_handler([](const gchar* string) { WriteLog("{}", string); });
    g_set_printerr_handler([](const gchar* string) { WriteLog("{}", string); });

    mono_config_parse_memory(R"(
    <configuration>
        <dllmap dll="i:cygwin1.dll" target="libc.so.6" os="!windows" />
        <dllmap dll="libc" target="libc.so.6" os="!windows"/>
        <dllmap dll="intl" target="libc.so.6" os="!windows"/>
        <dllmap dll="intl" name="bind_textdomain_codeset" target="libc.so.6" os="solaris"/>
        <dllmap dll="libintl" name="bind_textdomain_codeset" target="libc.so.6" os="solaris"/>
        <dllmap dll="libintl" target="libc.so.6" os="!windows"/>
        <dllmap dll="i:libxslt.dll" target="libxslt.so" os="!windows"/>
        <dllmap dll="i:odbc32.dll" target="libodbc.so" os="!windows"/>
        <dllmap dll="i:odbc32.dll" target="libiodbc.dylib" os="osx"/>
        <dllmap dll="oci" target="libclntsh.so" os="!windows"/>
        <dllmap dll="db2cli" target="libdb2_36.so" os="!windows"/>
        <dllmap dll="MonoPosixHelper" target="$mono_libdir/libMonoPosixHelper.so" os="!windows" />
        <dllmap dll="System.Native" target="$mono_libdir/libmono-system-native.so" os="!windows" />
        <dllmap dll="libmono-btls-shared" target="$mono_libdir/libmono-btls-shared.so" os="!windows" />
        <dllmap dll="i:msvcrt" target="libc.so.6" os="!windows"/>
        <dllmap dll="i:msvcrt.dll" target="libc.so.6" os="!windows"/>
        <dllmap dll="sqlite" target="libsqlite.so.0" os="!windows"/>
        <dllmap dll="sqlite3" target="libsqlite3.so.0" os="!windows"/>
        <dllmap dll="libX11" target="libX11.so" os="!windows" />
        <dllmap dll="libgdk-x11-2.0" target="libgdk-x11-2.0.so.0" os="!windows"/>
        <dllmap dll="libgdk_pixbuf-2.0" target="libgdk_pixbuf-2.0.so.0" os="!windows"/>
        <dllmap dll="libgtk-x11-2.0" target="libgtk-x11-2.0.so.0" os="!windows"/>
        <dllmap dll="libglib-2.0" target="libglib-2.0.so.0" os="!windows"/>
        <dllmap dll="libgobject-2.0" target="libgobject-2.0.so.0" os="!windows"/>
        <dllmap dll="libgnomeui-2" target="libgnomeui-2.so.0" os="!windows"/>
        <dllmap dll="librsvg-2" target="librsvg-2.so.2" os="!windows"/>
        <dllmap dll="libXinerama" target="libXinerama.so.1" os="!windows" />
        <dllmap dll="libasound" target="libasound.so.2" os="!windows" />
        <dllmap dll="libcairo-2.dll" target="libcairo.so.2" os="!windows"/>
        <dllmap dll="libcairo-2.dll" target="libcairo.2.dylib" os="osx"/>
        <dllmap dll="libcups" target="libcups.so.2" os="!windows"/>
        <dllmap dll="libcups" target="libcups.dylib" os="osx"/>
        <dllmap dll="i:kernel32.dll">
            <dllentry dll="__Internal" name="CopyMemory" target="mono_win32_compat_CopyMemory"/>
            <dllentry dll="__Internal" name="FillMemory" target="mono_win32_compat_FillMemory"/>
            <dllentry dll="__Internal" name="MoveMemory" target="mono_win32_compat_MoveMemory"/>
            <dllentry dll="__Internal" name="ZeroMemory" target="mono_win32_compat_ZeroMemory"/>
        </dllmap>
        <dllmap dll="gdiplus" target="libgdiplus.so" os="!windows"/>
        <dllmap dll="gdiplus.dll" target="libgdiplus.so"  os="!windows"/>
        <dllmap dll="gdi32" target="libgdiplus.so" os="!windows"/>
        <dllmap dll="gdi32.dll" target="libgdiplus.so" os="!windows"/>
    </configuration>)");

    mono_set_dirs("./dummy/lib", "./dummy/etc");
    mono_set_assemblies_path("./dummy/lib/gac");

    RUNTIME_ASSERT(EngineAssemblyImages.empty());
    mono_install_assembly_preload_hook(
        [](MonoAssemblyName* aname, char** assemblies_path, void* user_data) -> MonoAssembly* {
            auto assembly_images = (map<string, MonoImage*>*)user_data;
            if (assembly_images->count(aname->name))
                return LoadGameAssembly(aname->name, *assembly_images);
            return LoadNetAssembly(aname->name);
        },
        (void*)&EngineAssemblyImages);

    MonoDomain* domain = mono_jit_init_version("FOnlineDomain", "v4.0.30319");
    RUNTIME_ASSERT(domain);

    SetMonoInternalCalls();

    if (assemblies_data)
    {
        for (auto& kv : *assemblies_data)
        {
            MonoImageOpenStatus status = MONO_IMAGE_OK;
            MonoImage* image = mono_image_open_from_data((char*)&kv.second[0], (uint)kv.second.size(), TRUE, &status);
            RUNTIME_ASSERT((status == MONO_IMAGE_OK && image));

            EngineAssemblyImages[kv.first] = image;
        }
    }
    else
    {
        bool ok = CompileGameAssemblies(target, EngineAssemblyImages);
        RUNTIME_ASSERT(ok);
    }

    for (auto& kv : EngineAssemblyImages)
    {
        bool ok = LoadGameAssembly(kv.first, EngineAssemblyImages);
        RUNTIME_ASSERT(ok);
    }

    MonoClass* global_events_class =
        mono_class_from_name(EngineAssemblyImages["FOnline.Core.dll"], "FOnlineEngine", "GlobalEvents");
    RUNTIME_ASSERT(global_events_class);
    MonoMethodDesc* init_method_desc = mono_method_desc_new(":Initialize()", FALSE);
    RUNTIME_ASSERT(init_method_desc);
    MonoMethod* init_method = mono_method_desc_search_in_class(init_method_desc, global_events_class);
    RUNTIME_ASSERT(init_method);
    mono_method_desc_free(init_method_desc);

    MonoObject* exc;
    MonoObject* init_method_result = mono_runtime_invoke(init_method, NULL, NULL, &exc);
    if (exc)
        mono_print_unhandled_exception(exc);

    if (exc || *(bool*)mono_object_unbox(init_method_result) == false)
        return false;

    return true;
}

bool Script::GetMonoAssemblies(const string& target, map<string, UCharVec>& assemblies_data)
{
    map<string, MonoImage*> assembly_images;
    if (!CompileGameAssemblies(target, assembly_images))
        return false;

    for (auto& kv : assembly_images)
    {
        UCharVec data(kv.second->raw_data_len);
        memcpy(&data[0], kv.second->raw_data, data.size());
        assemblies_data[kv.first] = std::move(data);
    }
    return true;
}

uint Script::CreateMonoObject(const string& type_name)
{
    MonoImage* image = EngineAssemblyImages["FOnline.Core.dll"];
    RUNTIME_ASSERT(image);
    MonoClass* obj_class = mono_class_from_name(image, "FOnlineEngine", type_name.c_str());
    RUNTIME_ASSERT(obj_class);

    MonoObject* mono_obj = mono_object_new(mono_domain_get(), obj_class);
    RUNTIME_ASSERT(mono_obj);
    mono_runtime_object_init(mono_obj);

    return mono_gchandle_new(mono_obj, TRUE);
}

void Script::CallMonoObjectMethod(const string& type_name, const string& method_name, uint obj, void* arg)
{
    MonoImage* image = EngineAssemblyImages["FOnline.Core.dll"];
    RUNTIME_ASSERT(image);
    MonoClass* obj_class = mono_class_from_name(image, "FOnlineEngine", type_name.c_str());
    RUNTIME_ASSERT(obj_class);
    MonoMethodDesc* method_desc = mono_method_desc_new(_str(":{}", method_name).c_str(), FALSE);
    RUNTIME_ASSERT(method_desc);
    MonoMethod* method = mono_method_desc_search_in_class(method_desc, obj_class);
    RUNTIME_ASSERT(method);
    mono_method_desc_free(method_desc);

    RUNTIME_ASSERT(obj);
    MonoObject* mono_obj = mono_gchandle_get_target(obj);
    RUNTIME_ASSERT(mono_obj);

    MonoObject* exc;
    void* args[1] = {arg};
    mono_runtime_invoke(method, mono_obj, args, &exc);
    if (exc)
        mono_print_unhandled_exception(exc);
}

void Script::DestroyMonoObject(uint obj)
{
    RUNTIME_ASSERT(obj);
    mono_gchandle_free(obj);
}

static void Engine_Log(MonoString* s)
{
    const char* utf8 = mono_string_to_utf8(s);
    WriteLog("{}\n", utf8);
    mono_free((void*)utf8);
}

static void SetMonoInternalCalls()
{
    mono_add_internal_call("FOnlineEngine.Engine::Log", (void*)&Engine_Log);
}

void Script::Finish()
{
    if (!Engine)
        return;

    EngineData* edata = (EngineData*)Engine->GetUserData();
    SAFEDEL(edata->Invoker);

#ifdef SCRIPT_WATCHER
    RunTimeoutAbort = 0;
    RunTimeoutMessage = 0;
    ScriptWatcherFinish = true;
    ScriptWatcherThread.Wait();
#endif

    if (!BindedFunctions.empty())
        BindedFunctions[1].ScriptFunc = nullptr;
    for (auto it = BindedFunctions.begin(), end = BindedFunctions.end(); it != end; ++it)
        it->Clear();
    BindedFunctions.clear();
    ScriptFuncBinds.clear();

    Preprocessor::SetPragmaCallback(nullptr);
    Preprocessor::UndefAll();
    UnloadScripts();

    while (!BusyContexts.empty())
        ReturnContext(BusyContexts[0]);
    while (!FreeContexts.empty())
        FinishContext(FreeContexts[0]);

    FinishEngine(Engine); // Finish default engine

    mono_jit_cleanup(mono_domain_get());
}

void Script::UnloadScripts()
{
    for (asUINT i = 0, j = Engine->GetModuleCount(); i < j; i++)
    {
        asIScriptModule* module = Engine->GetModuleByIndex(i);
        int result = module->ResetGlobalVars();
        if (result < 0)
            WriteLog("Reset global vars fail, module '{}', error {}.\n", module->GetName(), result);
        result = module->UnbindAllImportedFunctions();
        if (result < 0)
            WriteLog("Unbind fail, module '{}', error {}.\n", module->GetName(), result);
    }

    while (Engine->GetModuleCount() > 0)
        Engine->GetModuleByIndex(0)->Discard();
}

static int SystemCall(const string& command, string& output)
{
#if defined(FO_WINDOWS)
    HANDLE out_read = nullptr;
    HANDLE out_write = nullptr;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    if (!CreatePipe(&out_read, &out_write, &sa, 0))
        return -1;
    if (!SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(out_read);
        CloseHandle(out_write);
        return -1;
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = out_write;
    si.hStdOutput = out_write;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    wchar_t* cmd_line = _wcsdup(_str(command).toWideChar().c_str());
    BOOL result = CreateProcessW(nullptr, cmd_line, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
    SAFEDELA(cmd_line);
    if (!result)
    {
        CloseHandle(out_read);
        CloseHandle(out_write);
        return -1;
    }

    string log;
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        DWORD bytes;
        while (PeekNamedPipe(out_read, nullptr, 0, nullptr, &bytes, nullptr) && bytes > 0)
        {
            char buf[TEMP_BUF_SIZE];
            if (ReadFile(out_read, buf, sizeof(buf), &bytes, nullptr))
                output.append(buf, bytes);
        }

        if (WaitForSingleObject(pi.hProcess, 0) != WAIT_TIMEOUT)
            break;
    }

    DWORD retval;
    GetExitCodeProcess(pi.hProcess, &retval);
    CloseHandle(out_read);
    CloseHandle(out_write);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)retval;

#elif !defined(FO_WEB)
    FILE* in = popen(command.c_str(), "r");
    if (!in)
        return -1;

    string log;
    char buf[TEMP_BUF_SIZE];
    while (fgets(buf, sizeof(buf), in))
        output.append(buf);

    return pclose(in);

#else
    WriteLog("Not supported!");
    return -1;
#endif
}

static MonoAssembly* LoadNetAssembly(const string& name)
{
    string assemblies_path = "Assemblies/" + name + (_str(name).endsWith(".dll") ? "" : ".dll");
    // Todo: rework FONLINE_
    /*#ifdef FONLINE_SERVER
    assemblies_path = "Resources/Mono/" + assemblies_path;
    #endif*/

    File file = FileMngr->ReadFile(assemblies_path);
    RUNTIME_ASSERT(file);

    MonoImageOpenStatus status = MONO_IMAGE_OK;
    MonoImage* image = mono_image_open_from_data((char*)file.GetBuf(), file.GetFsize(), TRUE, &status);
    RUNTIME_ASSERT((status == MONO_IMAGE_OK && image));

    MonoAssembly* assembly = mono_assembly_load_from(image, name.c_str(), &status);
    RUNTIME_ASSERT((status == MONO_IMAGE_OK && assembly));

    return assembly;
}

static MonoAssembly* LoadGameAssembly(const string& name, map<string, MonoImage*>& assembly_images)
{
    RUNTIME_ASSERT(assembly_images.count(name));
    MonoImage* image = assembly_images[name];

    MonoImageOpenStatus status = MONO_IMAGE_OK;
    MonoAssembly* assembly = mono_assembly_load_from(image, name.c_str(), &status);
    RUNTIME_ASSERT((status == MONO_IMAGE_OK && assembly));

    return assembly;
}

static bool CompileGameAssemblies(const string& target, map<string, MonoImage*>& assembly_images)
{
    string mono_path = Settings->MonoPath;
    string xbuild_path = _str(mono_path + "/bin/xbuild.bat");
    DiskFileSystem::ResolvePath(xbuild_path);

    FileCollection proj_files = FileMngr->FilterFiles("csproj");
    while (proj_files.MoveNext())
    {
        File file = proj_files.GetCurFile();
        RUNTIME_ASSERT(file);

        // Compile
        string command =
            _str("{} /property:Configuration={} /nologo /verbosity:quiet \"{}\"", xbuild_path, target, file.GetPath());
        string output;
        int call_result = SystemCall(command, output);
        if (call_result)
        {
            StrVec errors = _str(output).split('\n');
            WriteLog("Compilation failed! Error{}:", errors.size() > 1 ? "s" : "");
            for (string& error : errors)
                WriteLog("{}\n", error);
            return false;
        }

        // Get output path
        string file_content = (char*)file.GetBuf();
        size_t pos = file_content.find("'$(Configuration)|$(Platform)' == '" + target + "|");
        RUNTIME_ASSERT(pos != string::npos);
        pos = file_content.find("<OutputPath>", pos);
        RUNTIME_ASSERT(pos != string::npos);
        size_t epos = file_content.find("</OutputPath>", pos);
        RUNTIME_ASSERT(epos != string::npos);
        pos += _str("<OutputPath>").length();

        string assembly_name = file.GetName() + ".dll";
        string assembly_path =
            _str("{}/{}/{}", _str(file.GetPath()).extractDir(), file_content.substr(pos, epos - pos), assembly_name);
        DiskFileSystem::ResolvePath(assembly_path);

        File assembly_file = FileMngr->ReadFile(assembly_path);
        RUNTIME_ASSERT(assembly_file);

        MonoImageOpenStatus status = MONO_IMAGE_OK;
        MonoImage* image =
            mono_image_open_from_data((char*)assembly_file.GetCStr(), assembly_file.GetFsize(), TRUE, &status);
        RUNTIME_ASSERT((status == MONO_IMAGE_OK && image));

        assembly_images[assembly_name] = image;
    }

    return true;
}

bool Script::ReloadScripts(const string& target)
{
    WriteLog("Reload scripts...\n");

    Script::UnloadScripts();

    EngineData* edata = (EngineData*)Engine->GetUserData();

    // Combine scripts
    FileCollection fos_files = FileMngr->FilterFiles("fos");
    int file_index = 0;
    int errors = 0;
    ScriptEntryVec scripts;
    while (fos_files.MoveNext())
    {
        File file = fos_files.GetCurFile();
        if (!file)
        {
            WriteLog("Unable to open file '{}'.\n", file.GetName());
            errors++;
            continue;
        }

        // Get first line
        string line = file.GetNonEmptyLine();
        if (line.empty())
        {
            WriteLog("Error in script '{}', file empty.\n", file.GetName());
            errors++;
            continue;
        }

        // Check signature
        if (line.find("FOS") == string::npos)
        {
            WriteLog("Error in script '{}', invalid header '{}'.\n", file.GetName(), line);
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
        entry.Content = _str("namespace {}{{{}\n}}\n", file.GetName(), (const char*)file.GetBuf());
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
        WriteLog("No scripts found.\n");
        return false;
    }

    // Build
    string result_code;
    if (!LoadRootModule(scripts, result_code))
    {
        WriteLog("Load scripts from files fail.\n");
        return false;
    }

    // Cache enums
    CacheEnumValues();

    // Done
    WriteLog("Reload scripts complete.\n");
    return true;
}

bool Script::PostInitScriptSystem()
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    if (edata->PragmaCB->IsError())
    {
        WriteLog("Error in pragma(s) during loading.\n");
        return false;
    }

    edata->PragmaCB->Finish();
    if (edata->PragmaCB->IsError())
    {
        WriteLog("Error in pragma(s) after finalization.\n");
        return false;
    }
    return true;
}

bool Script::RunModuleInitFunctions()
{
    RUNTIME_ASSERT(Engine->GetModuleCount() == 1);

    asIScriptModule* module = Engine->GetModuleByIndex(0);
    for (asUINT i = 0; i < module->GetFunctionCount(); i++)
    {
        asIScriptFunction* func = module->GetFunctionByIndex(i);
        if (!Str::Compare(func->GetName(), "ModuleInit"))
            continue;

        if (func->GetParamCount() != 0)
        {
            WriteLog("Init function '{}::{}' can't have arguments.\n", func->GetNamespace(), func->GetName());
            return false;
        }
        if (func->GetReturnTypeId() != asTYPEID_VOID && func->GetReturnTypeId() != asTYPEID_BOOL)
        {
            WriteLog(
                "Init function '{}::{}' must have void or bool return type.\n", func->GetNamespace(), func->GetName());
            return false;
        }

        uint bind_id = Script::BindByFunc(func, true);
        Script::PrepareContext(bind_id, "Script");

        if (!Script::RunPrepared())
        {
            WriteLog("Error executing init function '{}::{}'.\n", func->GetNamespace(), func->GetName());
            return false;
        }

        if (func->GetReturnTypeId() == asTYPEID_BOOL && !Script::GetReturnedBool())
        {
            WriteLog("Initialization stopped by init function '{}::{}'.\n", func->GetNamespace(), func->GetName());
            return false;
        }
    }

    return true;
}

asIScriptEngine* Script::GetEngine()
{
    return Engine;
}

void Script::SetEngine(asIScriptEngine* engine)
{
    Engine = engine;
}

asIScriptEngine* Script::CreateEngine(ScriptPragmaCallback* pragma_callback, const string& target)
{
    asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    if (!engine)
    {
        WriteLog("asCreateScriptEngine fail.\n");
        return nullptr;
    }

    engine->SetMessageCallback(asFUNCTION(CallbackMessage), nullptr, asCALL_CDECL);

    engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, true);
    engine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, true);
    engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, true);
    engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);
    engine->SetEngineProperty(asEP_DISALLOW_EMPTY_LIST_ELEMENTS, true);
    engine->SetEngineProperty(asEP_PRIVATE_PROP_AS_PROTECTED, true);
    engine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE, true);
    engine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, true);
    engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);

    RegisterScriptArray(engine, true);
    RegisterScriptArrayExtensions(engine);
    RegisterStdString(engine);
    RegisterScriptStdStringExtensions(engine);
    RegisterScriptAny(engine);
    RegisterScriptDictionary(engine);
    RegisterScriptDict(engine);
    RegisterScriptDictExtensions(engine);
    RegisterScriptFile(engine);
    RegisterScriptFileSystem(engine);
    RegisterScriptDateTime(engine);
    RegisterScriptMath(engine);
    RegisterScriptWeakRef(engine);
    RegisterScriptReflection(engine);

    EngineData* edata = new EngineData();
    edata->PragmaCB = pragma_callback;
    edata->Invoker = new ScriptInvoker(*Settings);
    engine->SetUserData(edata);
    return engine;
}

void Script::FinishEngine(asIScriptEngine*& engine)
{
    if (engine)
    {
        EngineData* edata = (EngineData*)engine->SetUserData(nullptr);
        delete edata->PragmaCB;
        delete edata;
        engine->ShutDownAndRelease();
        engine = nullptr;
    }
}

void Script::CreateContext()
{
    asIScriptContext* ctx = Engine->CreateContext();
    RUNTIME_ASSERT(ctx);

    int r = ctx->SetExceptionCallback(asFUNCTION(CallbackException), nullptr, asCALL_CDECL);
    RUNTIME_ASSERT(r >= 0);

    ContextData* ctx_data = new ContextData();
    memzero(ctx_data, sizeof(ContextData));
    ctx->SetUserData(ctx_data);

    FreeContexts.push_back(ctx);
}

void Script::FinishContext(asIScriptContext* ctx)
{
    auto it = std::find(FreeContexts.begin(), FreeContexts.end(), ctx);
    RUNTIME_ASSERT(it != FreeContexts.end());
    FreeContexts.erase(it);

    delete (ContextData*)ctx->GetUserData();
    ctx->Release();
    ctx = nullptr;
}

asIScriptContext* Script::RequestContext()
{
    if (FreeContexts.empty())
        CreateContext();

    asIScriptContext* ctx = FreeContexts.back();
    FreeContexts.pop_back();
    BusyContexts.push_back(ctx);
    return ctx;
}

void Script::ReturnContext(asIScriptContext* ctx)
{
    auto it = std::find(BusyContexts.begin(), BusyContexts.end(), ctx);
    RUNTIME_ASSERT(it != BusyContexts.end());
    BusyContexts.erase(it);
    FreeContexts.push_back(ctx);

    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    for (uint i = 0; i < ctx_data->EntityArgsCount; i++)
        ctx_data->EntityArgs[i]->Release();
    memzero(ctx_data, sizeof(ContextData));
}

void Script::SetExceptionCallback(ExceptionCallback callback)
{
    OnException = callback;
}

void Script::RaiseException(const string& message)
{
    asIScriptContext* ctx = asGetActiveContext();
    if (ctx && ctx->GetState() == asEXECUTION_EXCEPTION)
        return;

    if (ctx)
        ctx->SetException(message.c_str());
    else
        HandleException(nullptr, _str("{}", "Engine exception: {}\n", message));
}

void Script::PassException()
{
    RaiseException("Pass");
}

void Script::HandleException(asIScriptContext* ctx, const string& message)
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

    if (OnException)
        OnException(buf);
}

string Script::GetTraceback()
{
    string result = "";
    ContextVec contexts = BusyContexts;
    for (int i = (int)contexts.size() - 1; i >= 0; i--)
    {
        result += MakeContextTraceback(contexts[i]);
        result += "\n";
    }
    return result;
}

string Script::MakeContextTraceback(asIScriptContext* ctx)
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

ScriptInvoker* Script::GetInvoker()
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    return edata->Invoker;
}

string Script::GetDeferredCallsStatistics()
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    return edata->Invoker->GetStatistics();
}

void Script::ProcessDeferredCalls()
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    edata->Invoker->Process();
}

// Todo: rework FONLINE_
/*#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
bool Script::LoadDeferredCalls()
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    return edata->Invoker->LoadDeferredCalls();
}
#endif*/

StrVec Script::GetCustomEntityTypes()
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    return edata->PragmaCB->GetCustomEntityTypes();
}

/*#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
bool Script::RestoreCustomEntity(const string& type_name, uint id, const DataBase::Document& doc)
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    return edata->PragmaCB->RestoreCustomEntity(type_name, id, doc);
}
#endif*/

EventData* Script::FindInternalEvent(const string& event_name)
{
    EventData* ev_data = new EventData;
    memzero(ev_data, sizeof(EventData));

    // AngelScript part
    EngineData* edata = (EngineData*)Engine->GetUserData();
    void* as_event = edata->PragmaCB->FindInternalEvent(event_name);
    RUNTIME_ASSERT(as_event);
    ev_data->ASEvent = as_event;

    // Mono part
    MonoClass* global_events_class =
        mono_class_from_name(EngineAssemblyImages["FOnline.Core.dll"], "FOnlineEngine", "GlobalEvents");
    RUNTIME_ASSERT(global_events_class);
    string mono_event_name = ":On" + _str(event_name).erase('E', 't') + "Event";
    MonoMethodDesc* init_method_desc = mono_method_desc_new(mono_event_name.c_str(), FALSE);
    RUNTIME_ASSERT(init_method_desc);
    MonoMethod* init_method = mono_method_desc_search_in_class(init_method_desc, global_events_class);
    // RUNTIME_ASSERT(init_method);
    mono_method_desc_free(init_method_desc);
    ev_data->MMethod = init_method;

    return ev_data;
}

bool Script::RaiseInternalEvent(EventData* ev_data, ...)
{
    // AngelScript part
    EngineData* edata = (EngineData*)Engine->GetUserData();
    va_list args;
    va_start(args, ev_data);
    bool result = edata->PragmaCB->RaiseInternalEvent(ev_data->ASEvent, args);
    va_end(args);

    // Mono part
    if (ev_data->MMethod)
    {
        void* mono_args[16];
        int64 mono_args_data[16];
        const IntVec& arg_infos = edata->PragmaCB->GetInternalEventArgInfos(ev_data->ASEvent);
        va_start(args, ev_data);
        for (size_t i = 0; i < arg_infos.size(); i++)
        {
            int arg_info = arg_infos[i];
            if (arg_info == -2)
                mono_args[i] = mono_gchandle_get_target(va_arg(args, Entity*)->MonoHandle);
            else if (arg_info == -3)
                mono_args[i] = va_arg(args, void*);
            else if (arg_info == 1 || arg_info == 2 || arg_info == 4)
                mono_args[i] = &(mono_args_data[i] = (int64)va_arg(args, int));
            else if (arg_info == 8)
                mono_args[i] = &(mono_args_data[i] = (int64)va_arg(args, int64));
            else
                throw UnreachablePlaceException("Unreachable place");
        }
        va_end(args);

        MonoObject* exc;
        MonoObject* method_result = mono_runtime_invoke(ev_data->MMethod, nullptr, mono_args, &exc);
        if (exc)
            mono_print_unhandled_exception(exc);
        if (exc || (method_result && *(bool*)mono_object_unbox(method_result) == false))
            return false;
    }

    return result;
}

void Script::RemoveEventsEntity(Entity* entity)
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    edata->PragmaCB->RemoveEventsEntity(entity);
}

void Script::HandleRpc(void* context)
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    edata->PragmaCB->HandleRpc(context);
}

string Script::GetActiveFuncName()
{
    asIScriptContext* ctx = asGetActiveContext();
    if (!ctx)
        return "";
    asIScriptFunction* func = ctx->GetFunction(0);
    if (!func)
        return "";
    return func->GetName();
}

void Script::Watcher(void*)
{
#ifdef SCRIPT_WATCHER
    while (!ScriptWatcherFinish)
    {
        // Execution timeout
        if (RunTimeoutAbort)
        {
            uint cur_tick = Timer::FastTick();
            for (auto it = BusyContexts.begin(); it != BusyContexts.end(); ++it)
            {
                asIScriptContext* ctx = *it;
                ContextData* ctx_data = (ContextData*)ctx->GetUserData();
                if (ctx->GetState() == asEXECUTION_ACTIVE && ctx_data->StartTick &&
                    cur_tick >= ctx_data->StartTick + RunTimeoutAbort)
                    ctx->Abort();
            }
        }

        Thread::Sleep(100);
    }
#endif
}

void Script::SetRunTimeout(uint abort_timeout, uint message_timeout)
{
#ifdef SCRIPT_WATCHER
    RunTimeoutAbort = abort_timeout;
    RunTimeoutMessage = message_timeout;
#endif
}

/************************************************************************/
/* Load / Bind                                                          */
/************************************************************************/

void Script::Define(const string& define)
{
    Preprocessor::Define(define);
}

void Script::Undef(const string& define)
{
    if (!define.empty())
        Preprocessor::Undef(define);
    else
        Preprocessor::UndefAll();
}

void Script::CallPragmas(const Pragmas& pragmas)
{
    EngineData* edata = (EngineData*)Engine->GetUserData();

    // Set current pragmas
    Preprocessor::SetPragmaCallback(edata->PragmaCB);

    // Call pragmas
    for (size_t i = 0; i < pragmas.size(); i++)
        Preprocessor::CallPragma(pragmas[i]);
}

bool Script::LoadRootModule(const ScriptEntryVec& scripts, string& result_code)
{
    RUNTIME_ASSERT(Engine->GetModuleCount() == 0);

    // Set current pragmas
    EngineData* edata = (EngineData*)Engine->GetUserData();
    Preprocessor::SetPragmaCallback(edata->PragmaCB);

    // File loader
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

        virtual bool LoadFile(const std::string& dir, const std::string& file_name, std::vector<char>& data,
            std::string& file_path) override
        {
            if (rootScript)
            {
                data.resize(rootScript->length());
                memcpy(&data[0], rootScript->c_str(), rootScript->length());
                rootScript = nullptr;
                file_path = "(Root)";
                return true;
            }

            includeDeep++;
            RUNTIME_ASSERT(includeDeep <= 1);

            if (includeDeep == 1 && !includeScripts.empty())
            {
                data.resize(includeScripts.front().Content.length());
                memcpy(&data[0], includeScripts.front().Content.c_str(), includeScripts.front().Content.length());
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
        WriteLog("Preprocessor message '{}'.\n", errors.String);
    }

    if (errors_count)
    {
        WriteLog("Unable to preprocess.\n");
        return false;
    }

    // Set global properties from command line
    for (auto& kv : config)
    {
        // Skip defines
        if (kv.first.length() > 2 && kv.first[0] == '-' && kv.first[1] == '-')
            continue;

        // Find property, with prefix and without
        int index = Engine->GetGlobalPropertyIndexByName(("__" + kv.first).c_str());
        if (index < 0)
        {
            index = Engine->GetGlobalPropertyIndexByName(kv.first.c_str());
            if (index < 0)
                continue;
        }

        int type_id;
        void* ptr;
        int r = Engine->GetGlobalPropertyByIndex(index, nullptr, nullptr, &type_id, nullptr, nullptr, &ptr, nullptr);
        RUNTIME_ASSERT(r >= 0);

        // Try set value
        asITypeInfo* obj_type = (type_id & asTYPEID_MASK_OBJECT ? Engine->GetTypeInfoById(type_id) : nullptr);
        bool is_hashes[] = {false, false, false, false};
        uchar pod_buf[8];
        bool is_error = false;
        void* value = ReadValue(kv.second.c_str(), type_id, obj_type, is_hashes, 0, pod_buf, is_error);
        if (!is_error)
        {
            if (!obj_type)
            {
                memcpy(ptr, value, Engine->GetSizeOfPrimitiveType(type_id));
            }
            else if (type_id & asTYPEID_OBJHANDLE)
            {
                if (*(void**)ptr)
                    Engine->ReleaseScriptObject(*(void**)ptr, obj_type);
                *(void**)ptr = value;
            }
            else
            {
                Engine->AssignScriptObject(ptr, value, obj_type);
                Engine->ReleaseScriptObject(value, obj_type);
            }
        }
    }

    // Add new
    asIScriptModule* module = Engine->GetModule("Root", asGM_ALWAYS_CREATE);
    if (!module)
    {
        WriteLog("Create 'Root' module fail.\n");
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
        WriteLog("Unable to add script section, result {}.\n", as_result);
        module->Discard();
        return false;
    }

    // Build module
    as_result = module->Build();
    if (as_result < 0)
    {
        WriteLog("Unable to build module, result {}.\n", as_result);
        module->Discard();
        return false;
    }

    result_code = result.String;
    return true;
}

bool Script::RestoreRootModule(const UCharVec& bytecode, const UCharVec& lnt_data)
{
    RUNTIME_ASSERT(Engine->GetModuleCount() == 0);
    RUNTIME_ASSERT(!bytecode.empty());
    RUNTIME_ASSERT(!lnt_data.empty());

    asIScriptModule* module = Engine->GetModule("Root", asGM_ALWAYS_CREATE);
    if (!module)
    {
        WriteLog("Create 'Root' module fail.\n");
        return false;
    }

    Preprocessor::LineNumberTranslator* lnt = Preprocessor::RestoreLineNumberTranslator(lnt_data);
    module->SetUserData(lnt);

    CBytecodeStream binary;
    binary.Write(&bytecode[0], (asUINT)bytecode.size());
    int result = module->LoadByteCode(&binary);
    if (result < 0)
    {
        WriteLog("Can't load binary, result {}.\n", result);
        module->Discard();
        return false;
    }

    return true;
}

uint Script::BindByFuncName(const string& func_name, const string& decl, bool is_temp, bool disable_log /* = false */)
{
    // Collect functions in all modules
    RUNTIME_ASSERT(Engine->GetModuleCount() == 1);

    // Find function
    string func_decl;
    if (!decl.empty())
        func_decl = _str(decl).replace("%s", func_name);
    else
        func_decl = func_name;

    asIScriptModule* module = Engine->GetModuleByIndex(0);
    asIScriptFunction* func = module->GetFunctionByDecl(func_decl.c_str());
    if (!func)
    {
        if (!disable_log)
            WriteLog("Function '{}' not found.\n", func_decl);
        return 0;
    }

    // Save to temporary bind
    if (is_temp)
    {
        BindedFunctions[1].ScriptFunc = func;
        return 1;
    }

    // Find already binded
    for (int i = 2, j = (int)BindedFunctions.size(); i < j; i++)
    {
        BindFunction& bf = BindedFunctions[i];
        if (bf.ScriptFunc == func)
            return i;
    }

    // Create new bind
    BindedFunctions.push_back(BindFunction(func));
    return (uint)BindedFunctions.size() - 1;
}

uint Script::BindByFunc(asIScriptFunction* func, bool is_temp, bool disable_log /* = false */)
{
    // Save to temporary bind
    if (is_temp)
    {
        BindedFunctions[1].ScriptFunc = func;
        return 1;
    }

    // Find already binded
    for (int i = 2, j = (int)BindedFunctions.size(); i < j; i++)
    {
        if (BindedFunctions[i].ScriptFunc == func)
            return i;
    }

    // Create new bind
    BindedFunctions.push_back(BindFunction(func));
    return (uint)BindedFunctions.size() - 1;
}

uint Script::BindByFuncNum(hash func_num, bool is_temp, bool disable_log /* = false */)
{
    asIScriptFunction* func = FindFunc(func_num);
    if (!func)
    {
        if (!disable_log)
            WriteLog("Function '{}' not found.\n", _str().parseHash(func_num));
        return 0;
    }

    return BindByFunc(func, is_temp, disable_log);
}

asIScriptFunction* Script::GetBindFunc(uint bind_id)
{
    RUNTIME_ASSERT(bind_id);
    RUNTIME_ASSERT(bind_id < BindedFunctions.size());

    return BindedFunctions[bind_id].ScriptFunc;
}

string Script::GetBindFuncName(uint bind_id)
{
    if (!bind_id || bind_id >= (uint)BindedFunctions.size())
    {
        WriteLog("Wrong bind id {}, bind buffer size {}.\n", bind_id, BindedFunctions.size());
        return "";
    }

    return BindedFunctions[bind_id].FuncName;
}

/************************************************************************/
/* Functions association                                                */
/************************************************************************/

hash Script::GetFuncNum(asIScriptFunction* func)
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

asIScriptFunction* Script::FindFunc(hash func_num)
{
    for (asUINT i = 0, j = Engine->GetModuleCount(); i < j; i++)
    {
        asIScriptModule* module = Engine->GetModuleByIndex(i);
        for (asUINT n = 0, m = module->GetFunctionCount(); n < m; n++)
        {
            asIScriptFunction* func = module->GetFunctionByIndex(n);
            if (GetFuncNum(func) == func_num)
                return func;
        }
    }
    return nullptr;
}

hash Script::BindScriptFuncNumByFuncName(const string& func_name, const string& decl)
{
    // Bind function
    int bind_id = Script::BindByFuncName(func_name, decl, false);
    if (bind_id <= 0)
        return 0;

    // Native and broken binds not allowed
    BindFunction& bf = BindedFunctions[bind_id];
    if (!bf.ScriptFunc)
        return 0;

    // Get func num
    hash func_num = GetFuncNum(bf.ScriptFunc);

    // Duplicate checking
    auto it = ScriptFuncBinds.find(func_num);
    if (it != ScriptFuncBinds.end() && it->second != bind_id)
        return 0;

    // Store
    if (it == ScriptFuncBinds.end())
        ScriptFuncBinds.insert(std::make_pair(func_num, bind_id));

    return func_num;
}

hash Script::BindScriptFuncNumByFunc(asIScriptFunction* func)
{
    // Bind function
    int bind_id = Script::BindByFunc(func, false);
    if (bind_id <= 0)
        return 0;

    // Get func num
    hash func_num = GetFuncNum(func);

    // Duplicate checking
    auto it = ScriptFuncBinds.find(func_num);
    if (it != ScriptFuncBinds.end() && it->second != bind_id)
        return 0;

    // Store
    if (it == ScriptFuncBinds.end())
        ScriptFuncBinds.insert(std::make_pair(func_num, bind_id));

    return func_num;
}

uint Script::GetScriptFuncBindId(hash func_num)
{
    if (!func_num)
        return 0;

    // Indexing by hash
    auto it = ScriptFuncBinds.find(func_num);
    if (it != ScriptFuncBinds.end())
        return it->second;

    // Function not binded, try find and bind it
    asIScriptFunction* func = FindFunc(func_num);
    if (!func)
        return 0;

    // Bind
    func_num = Script::BindScriptFuncNumByFunc(func);
    if (func_num)
        return ScriptFuncBinds[func_num];

    return 0;
}

void Script::PrepareScriptFuncContext(hash func_num, const string& ctx_info)
{
    uint bind_id = GetScriptFuncBindId(func_num);
    PrepareContext(bind_id, ctx_info);
}

void Script::CacheEnumValues()
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    auto& cached_enums = edata->CachedEnums;
    auto& cached_enum_names = edata->CachedEnumNames;

    // Engine enums
    cached_enums.clear();
    cached_enum_names.clear();
    for (asUINT i = 0, j = Engine->GetEnumCount(); i < j; i++)
    {
        asITypeInfo* enum_type = Engine->GetEnumByIndex(i);
        const char* enum_ns = enum_type->GetNamespace();
        enum_ns = (enum_ns && enum_ns[0] ? enum_ns : nullptr);
        const char* enum_name = enum_type->GetName();
        string name = _str("{}{}{}", enum_ns ? enum_ns : "", enum_ns ? "::" : "", enum_name);
        IntStrMap& value_names = cached_enum_names.insert(std::make_pair(name, IntStrMap())).first->second;
        for (asUINT k = 0, l = enum_type->GetEnumValueCount(); k < l; k++)
        {
            int value;
            string value_name = enum_type->GetEnumValueByIndex(k, &value);
            name = _str("{}{}{}::{}", enum_ns ? enum_ns : "", enum_ns ? "::" : "", enum_name, value_name);
            cached_enums.insert(std::make_pair(name, value));
            value_names.insert(std::make_pair(value, value_name));
        }
    }

    // Script enums
    RUNTIME_ASSERT(Engine->GetModuleCount() == 1);
    asIScriptModule* module = Engine->GetModuleByIndex(0);
    for (asUINT i = 0; i < module->GetEnumCount(); i++)
    {
        asITypeInfo* enum_type = module->GetEnumByIndex(i);
        const char* enum_ns = enum_type->GetNamespace();
        enum_ns = (enum_ns && enum_ns[0] ? enum_ns : nullptr);
        const char* enum_name = enum_type->GetName();
        string name = _str("{}{}{}", enum_ns ? enum_ns : "", enum_ns ? "::" : "", enum_name);
        IntStrMap& value_names = cached_enum_names.insert(std::make_pair(name, IntStrMap())).first->second;
        for (asUINT k = 0, l = enum_type->GetEnumValueCount(); k < l; k++)
        {
            int value;
            string value_name = enum_type->GetEnumValueByIndex(k, &value);
            name = _str("{}{}{}::{}", enum_ns ? enum_ns : "", enum_ns ? "::" : "", enum_name, value_name);
            cached_enums.insert(std::make_pair(name, value));
            value_names.insert(std::make_pair(value, value_name));
        }
    }

    // Content
    for (asUINT i = 0, j = Engine->GetGlobalPropertyCount(); i < j; i++)
    {
        const char* name;
        const char* ns;
        int type_id;
        hash* value;
        Engine->GetGlobalPropertyByIndex(i, &name, &ns, &type_id, nullptr, nullptr, (void**)&value);
        if (_str(ns).startsWith("Content"))
        {
            RUNTIME_ASSERT(type_id == asTYPEID_UINT32); // hash
            cached_enums.insert(std::make_pair(_str("{}::{}", ns, name), (int)*value));
        }
    }
}

int Script::GetEnumValue(const string& enum_value_name, bool& fail)
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    const auto& cached_enums = edata->CachedEnums;
    auto it = cached_enums.find(enum_value_name);
    if (it == cached_enums.end())
    {
        WriteLog("Enum value '{}' not found.\n", enum_value_name);
        fail = true;
        return 0;
    }
    return it->second;
}

int Script::GetEnumValue(const string& enum_name, const string& value_name, bool& fail)
{
    if (_str(value_name).isNumber())
        return _str(value_name).toInt();
    return GetEnumValue(_str("{}::{}", enum_name, value_name), fail);
}

string Script::GetEnumValueName(const string& enum_name, int value)
{
    EngineData* edata = (EngineData*)Engine->GetUserData();
    const auto& cached_enum_names = edata->CachedEnumNames;
    auto it = cached_enum_names.find(enum_name);
    RUNTIME_ASSERT(it != cached_enum_names.end());
    auto it_value = it->second.find(value);
    return it_value != it->second.end() ? it_value->second : _str("{}", value).str();
}

/************************************************************************/
/* Contexts                                                             */
/************************************************************************/

static asIScriptContext* CurrentCtx = nullptr;
static size_t CurrentArg = 0;
static void* RetValue;

void Script::PrepareContext(uint bind_id, const string& ctx_info)
{
    RUNTIME_ASSERT(bind_id > 0);
    RUNTIME_ASSERT(bind_id < (uint)BindedFunctions.size());

    BindFunction& bf = BindedFunctions[bind_id];
    asIScriptFunction* script_func = bf.ScriptFunc;

    RUNTIME_ASSERT(script_func);

    asIScriptContext* ctx = RequestContext();
    RUNTIME_ASSERT(ctx);

    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    Str::Copy(ctx_data->Info, ctx_info.c_str());

    int result = ctx->Prepare(script_func);
    RUNTIME_ASSERT(result >= 0);

    RUNTIME_ASSERT(!CurrentCtx);
    CurrentCtx = ctx;

    CurrentArg = 0;
}

void Script::SetArgUChar(uchar value)
{
    CurrentCtx->SetArgByte((asUINT)CurrentArg, value);
    CurrentArg++;
}

void Script::SetArgUShort(ushort value)
{
    CurrentCtx->SetArgWord((asUINT)CurrentArg, value);
    CurrentArg++;
}

void Script::SetArgUInt(uint value)
{
    CurrentCtx->SetArgDWord((asUINT)CurrentArg, value);
    CurrentArg++;
}

void Script::SetArgUInt64(uint64 value)
{
    CurrentCtx->SetArgQWord((asUINT)CurrentArg, value);
    CurrentArg++;
}

void Script::SetArgBool(bool value)
{
    CurrentCtx->SetArgByte((asUINT)CurrentArg, value);
    CurrentArg++;
}

void Script::SetArgFloat(float value)
{
    CurrentCtx->SetArgFloat((asUINT)CurrentArg, value);
    CurrentArg++;
}

void Script::SetArgDouble(double value)
{
    CurrentCtx->SetArgDouble((asUINT)CurrentArg, value);
    CurrentArg++;
}

void Script::SetArgObject(void* value)
{
    CurrentCtx->SetArgObject((asUINT)CurrentArg, value);
    CurrentArg++;
}

void Script::SetArgEntity(Entity* value)
{
    RUNTIME_ASSERT((!value || !value->IsDestroyed));

    if (value)
    {
        ContextData* ctx_data = (ContextData*)CurrentCtx->GetUserData();
        value->AddRef();
        RUNTIME_ASSERT(ctx_data->EntityArgsCount < 20);
        ctx_data->EntityArgs[ctx_data->EntityArgsCount] = value;
        ctx_data->EntityArgsCount++;
    }

    SetArgObject(value);
}

void Script::SetArgAddress(void* value)
{
    CurrentCtx->SetArgAddress((asUINT)CurrentArg, value);
    CurrentArg++;
}

bool Script::RunPrepared()
{
    RUNTIME_ASSERT(CurrentCtx);
    asIScriptContext* ctx = CurrentCtx;
    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    uint tick = Timer::FastTick();
    CurrentCtx = nullptr;
    ctx_data->StartTick = tick;
    ctx_data->Parent = asGetActiveContext();

    int result = ctx->Execute();

#ifdef SCRIPT_WATCHER
    uint delta = Timer::FastTick() - tick;
#endif

    asEContextState state = ctx->GetState();
    if (state == asEXECUTION_SUSPENDED)
    {
        RetValue = 0;
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
#ifdef SCRIPT_WATCHER
    else if (RunTimeoutMessage && delta >= RunTimeoutMessage)
    {
        WriteLog("Script work time {} in context '{}'.\n", delta, ctx_data->Info);
    }
#endif

    if (result < 0)
    {
        WriteLog("Context '{}' execute error {}, state '{}'.\n", ctx_data->Info, result, ContextStatesStr[(int)state]);
        ctx->Abort();
        ReturnContext(ctx);
        return false;
    }

    RetValue = ctx->GetAddressOfReturnValue();

    ReturnContext(ctx);

    return true;
}

void Script::RunPreparedSuspend()
{
    RUNTIME_ASSERT(CurrentCtx);

    asIScriptContext* ctx = CurrentCtx;
    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    uint tick = Timer::FastTick();
    CurrentCtx = nullptr;
    ctx_data->StartTick = tick;
    ctx_data->Parent = asGetActiveContext();
    RetValue = 0;
}

asIScriptContext* Script::SuspendCurrentContext(uint time)
{
    asIScriptContext* ctx = asGetActiveContext();
    RUNTIME_ASSERT(std::find(BusyContexts.begin(), BusyContexts.end(), ctx) != BusyContexts.end());
    if (ctx->GetFunction(ctx->GetCallstackSize() - 1)->GetReturnTypeId() != asTYPEID_VOID)
        SCRIPT_ERROR_R0("Can't yield context which must return value.");

    ctx->Suspend();
    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    ctx_data->SuspendEndTick = (time != uint(-1) ? (time ? Timer::FastTick() + time : 0) : uint(-1));
    return ctx;
}

void Script::ResumeContext(asIScriptContext* ctx)
{
    RUNTIME_ASSERT(ctx->GetState() == asEXECUTION_SUSPENDED);
    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    RUNTIME_ASSERT(ctx_data->SuspendEndTick == uint(-1));
    ctx_data->SuspendEndTick = Timer::FastTick();
}

void Script::RunSuspended()
{
    if (BusyContexts.empty())
        return;

    // Collect contexts to resume
    ContextVec resume_contexts;
    uint tick = Timer::FastTick();
    for (auto it = BusyContexts.begin(), end = BusyContexts.end(); it != end; ++it)
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

        CurrentCtx = ctx;
        RunPrepared();
    }
}

void Script::RunMandatorySuspended()
{
    uint i = 0;
    while (true)
    {
        if (BusyContexts.empty())
            break;

        // Collect contexts to resume
        ContextVec resume_contexts;
        for (auto it = BusyContexts.begin(), end = BusyContexts.end(); it != end; ++it)
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

            CurrentCtx = ctx;
            RunPrepared();
        }

        // Detect recursion
        if (++i % 10000 == 0)
            WriteLog("Big loops in suspended contexts.\n");
    }
}

bool Script::CheckContextEntities(asIScriptContext* ctx)
{
    ContextData* ctx_data = (ContextData*)ctx->GetUserData();
    for (uint i = 0; i < ctx_data->EntityArgsCount; i++)
        if (ctx_data->EntityArgs[i]->IsDestroyed)
            return false;
    return true;
}

uint Script::GetReturnedUInt()
{
    return *(uint*)RetValue;
}

bool Script::GetReturnedBool()
{
    return *(bool*)RetValue;
}

void* Script::GetReturnedObject()
{
    return *(void**)RetValue;
}

float Script::GetReturnedFloat()
{
    return *(float*)RetValue;
}

double Script::GetReturnedDouble()
{
    return *(double*)RetValue;
}

void* Script::GetReturnedRawAddress()
{
    return RetValue;
}

/************************************************************************/
/* Logging                                                              */
/************************************************************************/

void Script::Log(const string& str)
{
    asIScriptContext* ctx = asGetActiveContext();
    if (!ctx)
    {
        WriteLog("<unknown> : {}.\n", str);
        return;
    }
    asIScriptFunction* func = ctx->GetFunction(0);
    if (!func)
    {
        WriteLog("<unknown> : {}.\n", str);
        return;
    }

    int line = ctx->GetLineNumber(0);
    Preprocessor::LineNumberTranslator* lnt = (Preprocessor::LineNumberTranslator*)func->GetModule()->GetUserData();
    WriteLog("{} : {}\n", Preprocessor::ResolveOriginalFile(line, lnt), str);
}

void Script::CallbackMessage(const asSMessageInfo* msg, void* param)
{
    const char* type = "Error";
    if (msg->type == asMSGTYPE_WARNING)
        type = "Warning";
    else if (msg->type == asMSGTYPE_INFORMATION)
        type = "Info";

    WriteLog("{} : {} : {} : Line {}.\n", Preprocessor::ResolveOriginalFile(msg->row), type, msg->message,
        Preprocessor::ResolveOriginalLine(msg->row));
}

void Script::CallbackException(asIScriptContext* ctx, void* param)
{
    string str = ctx->GetExceptionString();
    if (str != "Pass")
        HandleException(ctx, _str("Script exception: {}{}", str, !_str(str).endsWith('.') ? "." : ""));
}

/************************************************************************/
/* Array                                                                */
/************************************************************************/

CScriptArray* Script::CreateArray(const string& type)
{
    return CScriptArray::Create(Engine->GetTypeInfoById(Engine->GetTypeIdByDecl(type.c_str())));
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
