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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "AngelScriptBackend.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptArray.h"
#include "AngelScriptCall.h"
#include "AngelScriptContext.h"
#include "AngelScriptDict.h"
#include "AngelScriptEntity.h"
#include "AngelScriptGlobals.h"
#include "AngelScriptHelpers.h"
#include "AngelScriptMath.h"
#include "AngelScriptReflection.h"
#include "AngelScriptRemoteCalls.h"
#include "AngelScriptString.h"
#include "AngelScriptTypes.h"

#include <preprocessor.h>

FO_BEGIN_NAMESPACE

static void AngelScriptMessage(const AngelScript::asSMessageInfo* msg, void* param)
{
    FO_STACK_TRACE_ENTRY();

    const char* type = msg->type == AngelScript::asMSGTYPE_WARNING ? "warning" : (msg->type == AngelScript::asMSGTYPE_INFORMATION ? "info" : "error");
    auto* as_engine = cast_from_void<AngelScript::asIScriptEngine*>(param);
    const auto* backend = GetScriptBackend(as_engine);
    const auto* lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(as_engine->GetUserData(5));
    const auto& orig_file = Preprocessor::ResolveOriginalFile(msg->row, lnt);
    const auto orig_line = Preprocessor::ResolveOriginalLine(msg->row, lnt);
    const auto formatted_message = strex("{}({},{}): {} : {}", orig_file, orig_line, msg->col, type, msg->message).str();

    backend->SendMessage(formatted_message);
}

static void CleanupScriptFunction(AngelScript::asIScriptFunction* func)
{
    FO_STACK_TRACE_ENTRY();

    const auto* func_desc = cast_from_void<ScriptFuncDesc*>(func->GetUserData());
    delete func_desc;
}

AngelScriptBackend::AngelScriptBackend()
{
    FO_STACK_TRACE_ENTRY();
}

AngelScriptBackend::~AngelScriptBackend()
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& cb : _cleanupCallbacks) {
        cb();
    }

    _meta.reset();
    _scriptSys.reset();
    _engine.reset();
    _entityMngr.reset();

    if (_asEngine) {
        _asEngine->SetUserData(nullptr);
        _asEngine->ShutDownAndRelease();
    }
}

auto AngelScriptBackend::GetGameEngine() -> BaseEngine*
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_engine);
    return _engine.get();
}

auto AngelScriptBackend::GetEntityMngr() -> EntityManagerApi*
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_entityMngr);
    return _entityMngr.get();
}

void AngelScriptBackend::RegisterMetadata(EngineMetadata* meta)
{
    FO_STACK_TRACE_ENTRY();

    auto* as_engine = AngelScript::asCreateScriptEngine(ANGELSCRIPT_VERSION);
    FO_RUNTIME_ASSERT(as_engine);

    as_engine->SetUserData(cast_to_void(this));

    _meta = meta;
    _engine = dynamic_cast<BaseEngine*>(meta);
    _scriptSys = dynamic_cast<ScriptSystem*>(meta);
    _entityMngr = dynamic_cast<EntityManagerApi*>(meta);
    _asEngine = as_engine;

    int32 as_result;
    FO_AS_VERIFY(as_engine->SetMessageCallback(asFUNCTION(AngelScriptMessage), cast_to_void(as_engine), AngelScript::asCALL_CDECL));

    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_ALLOW_UNSAFE_REFERENCES, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_USE_CHARACTER_LITERALS, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_BUILD_WITHOUT_LINE_CUES, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_DISALLOW_EMPTY_LIST_ELEMENTS, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_PRIVATE_PROP_AS_PROTECTED, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_REQUIRE_ENUM_SCOPE, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_INIT_GLOBAL_VARS_AFTER_BUILD, true));

    as_engine->SetFunctionUserDataCleanupCallback(CleanupScriptFunction);

    RegisterAngelScriptArray(as_engine);
    RegisterAngelScriptString(as_engine);
    RegisterAngelScriptDict(as_engine);
    RegisterAngelScriptReflection(as_engine);
    RegisterAngelScriptMath(as_engine);
    RegisterAngelScriptEnums(as_engine);
    RegisterAngelScriptTypes(as_engine);
    RegisterAngelScriptEntity(as_engine);
    RegisterAngelScriptGlobals(as_engine);
    RegisterAngelScriptRemoteCalls(as_engine);
}

void AngelScriptBackend::SetMessageCallback(function<void(string_view)> message_callback)
{
    FO_STACK_TRACE_ENTRY();

    _messageCallback = std::move(message_callback);
}

void AngelScriptBackend::SendMessage(string_view message) const
{
    FO_STACK_TRACE_ENTRY();

    if (_messageCallback) {
        _messageCallback(message);
    }
    else {
        WriteLog(message);
    }
}

class BinaryStream : public AngelScript::asIBinaryStream
{
public:
    explicit BinaryStream(vector<AngelScript::asBYTE>& buf) :
        _binBuf {buf}
    {
        FO_STACK_TRACE_ENTRY();
    }

    void Write(const void* ptr, AngelScript::asUINT size) override
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (ptr == nullptr || size == 0) {
            return;
        }

        _binBuf.resize(_binBuf.size() + size);
        MemCopy(&_binBuf[_writePos], ptr, size);
        _writePos += size;
    }

    void Read(void* ptr, AngelScript::asUINT size) override
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (ptr == nullptr || size == 0) {
            return;
        }

        MemCopy(ptr, &_binBuf[_readPos], size);
        _readPos += size;
    }

    auto GetBuf() const -> vector<AngelScript::asBYTE>&
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _binBuf;
    }

private:
    vector<AngelScript::asBYTE>& _binBuf;
    size_t _readPos {};
    size_t _writePos {};
};

void AngelScriptBackend::LoadBinaryScripts(const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    FileCollection script_bin_files = [&]() -> FileCollection {
        switch (_meta->GetSide()) {
        case EngineSideKind::ServerSide:
            return resources.FilterFiles("fos-bin-server");
        case EngineSideKind::ClientSide:
            return resources.FilterFiles("fos-bin-client");
        case EngineSideKind::MapperSide:
            return resources.FilterFiles("fos-bin-mapper");
        }
        FO_UNREACHABLE_PLACE();
    }();

    FO_RUNTIME_ASSERT(script_bin_files.GetFilesCount() == 1);
    const auto script_bin_file = File::Load(*script_bin_files.begin());
    const auto script_bin = span(script_bin_file.GetBuf(), script_bin_file.GetSize());

    FO_RUNTIME_ASSERT(_asEngine->GetModuleCount() == 0);
    FO_RUNTIME_ASSERT(!script_bin.empty());

    auto reader = DataReader({script_bin.data(), script_bin.size()});

    vector<AngelScript::asBYTE> buf(reader.Read<uint32>());
    MemCopy(buf.data(), reader.ReadPtr<AngelScript::asBYTE>(buf.size()), buf.size());

    std::vector<uint8> lnt_data(reader.Read<uint32>());
    MemCopy(lnt_data.data(), reader.ReadPtr<uint8>(lnt_data.size()), lnt_data.size());

    reader.VerifyEnd();
    FO_RUNTIME_ASSERT(!buf.empty());
    FO_RUNTIME_ASSERT(!lnt_data.empty());

    auto* mod = _asEngine->GetModule("Root", AngelScript::asGM_ALWAYS_CREATE);

    if (mod == nullptr) {
        throw ScriptException("Create root module fail");
    }

    auto* lnt = Preprocessor::RestoreLineNumberTranslator(lnt_data);
    _asEngine->SetUserData(cast_to_void(lnt), 5);

    BinaryStream binary {buf};
    int32 as_result = mod->LoadByteCode(&binary);

    if (as_result < 0) {
        throw ScriptException("Can't load binary", as_result);
    }
}

auto AngelScriptBackend::CompileTextScripts(const vector<File>& files) -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_asEngine->GetModuleCount() == 0);

    // File loader
    class ScriptLoader : public Preprocessor::FileLoader
    {
    public:
        ScriptLoader(const string* root, const map<string, string>* files) :
            _rootScript {root},
            _scriptFiles {files}
        {
            FO_STACK_TRACE_ENTRY();
        }

        auto LoadFile(const std::string& dir, const std::string& file_name, std::vector<char>& data, std::string& file_path) -> bool override
        {
            FO_STACK_TRACE_ENTRY();

            if (_rootScript != nullptr) {
                data.resize(_rootScript->size());
                MemCopy(data.data(), _rootScript->data(), _rootScript->size());
                _rootScript = nullptr;
                file_path = "(Root)";
                return true;
            }

            _includeDeep++;

            file_path = file_name;
            data.resize(0);

            if (_includeDeep == 1) {
                const auto it = _scriptFiles->find(string(file_name));
                FO_RUNTIME_ASSERT(it != _scriptFiles->end());

                data.resize(it->second.size());
                MemCopy(data.data(), it->second.data(), it->second.size());
                return true;
            }

            return Preprocessor::FileLoader::LoadFile(dir, file_name, data, file_path);
        }

        void FileLoaded() override
        {
            FO_STACK_TRACE_ENTRY();

            _includeDeep--;
        }

    private:
        const string* _rootScript;
        const map<string, string>* _scriptFiles;
        int32 _includeDeep {};
    };

    map<string, string> final_script_files;
    vector<tuple<int32, string, string>> final_script_files_order;

    for (const auto& script_file : files) {
        string script_name = string(script_file.GetNameNoExt());
        string script_path = string(script_file.GetDiskPath());
        string script_content = script_file.GetStr();

        const auto line_sep = script_content.find('\n');
        const auto first_line = script_content.substr(0, line_sep);

        int32 sort = 0;
        const auto sort_pos = first_line.find("Sort ");

        if (sort_pos != string::npos) {
            sort = strex(first_line.substr(sort_pos + "Sort "_len)).substring_until(' ').to_int32();
        }

        final_script_files_order.emplace_back(std::make_tuple(sort, std::move(script_name), script_path));
        final_script_files.emplace(std::move(script_path), std::move(script_content));
    }

    std::ranges::stable_sort(final_script_files_order, [](auto&& a, auto&& b) {
        if (std::get<0>(a) == std::get<0>(b)) {
            return std::get<1>(a) < std::get<1>(b);
        }
        else {
            return std::get<0>(a) < std::get<0>(b);
        }
    });

    string root_script;
    root_script.reserve(final_script_files.size() * 128);

    for (auto&& [script_order, script_name, script_path] : final_script_files_order) {
        root_script.append("#include \"");
        root_script.append(script_path);
        root_script.append("\"\n");
    }

    auto* preprocessor_context = Preprocessor::CreateContext();
    auto delete_preprocessor_context = scope_exit([&]() noexcept { Preprocessor::DeleteContext(preprocessor_context); });

    Preprocessor::UndefAll(preprocessor_context);

    switch (_meta->GetSide()) {
    case EngineSideKind::ServerSide:
        Preprocessor::Define(preprocessor_context, "SERVER 1");
        Preprocessor::Define(preprocessor_context, "CLIENT 0");
        Preprocessor::Define(preprocessor_context, "MAPPER 0");
        break;
    case EngineSideKind::ClientSide:
        Preprocessor::Define(preprocessor_context, "SERVER 0");
        Preprocessor::Define(preprocessor_context, "CLIENT 1");
        Preprocessor::Define(preprocessor_context, "MAPPER 0");
        break;
    case EngineSideKind::MapperSide:
        Preprocessor::Define(preprocessor_context, "SERVER 0");
        Preprocessor::Define(preprocessor_context, "CLIENT 0");
        Preprocessor::Define(preprocessor_context, "MAPPER 1");
        break;
    }

    auto loader = ScriptLoader(&root_script, &final_script_files);
    Preprocessor::StringOutStream result;
    Preprocessor::StringOutStream errors;
    const auto errors_count = Preprocessor::Preprocess(preprocessor_context, "", result, &errors, &loader);

    while (!errors.String.empty() && errors.String.back() == '\n') {
        errors.String.pop_back();
    }

    if (errors_count > 0) {
        throw ScriptCompilerException("Preprocessor failed", errors.String);
    }
    else if (!errors.String.empty()) {
        WriteLog("Preprocessor message: {}", errors.String);
    }

    Preprocessor::LineNumberTranslator* lnt = Preprocessor::GetLineNumberTranslator(preprocessor_context);
    _asEngine->SetUserData(cast_to_void(lnt), 5);

    auto* mod = _asEngine->GetModule("Root", AngelScript::asGM_ALWAYS_CREATE);

    if (mod == nullptr) {
        throw ScriptCompilerException("Create root module failed");
    }

    int32 as_result = mod->AddScriptSection("Root", result.String.c_str());

    if (as_result < 0) {
        throw ScriptCompilerException("Unable to add script section", as_result);
    }

    as_result = mod->Build();

    if (as_result < 0) {
        throw ScriptCompilerException("Unable to build module", as_result);
    }

    vector<AngelScript::asBYTE> buf;
    BinaryStream binary {buf};
    as_result = mod->SaveByteCode(&binary);

    if (as_result < 0) {
        throw ScriptCompilerException("Unable to save byte code", as_result);
    }

    std::vector<uint8> lnt_data;
    Preprocessor::StoreLineNumberTranslator(lnt, lnt_data);

    vector<uint8> data;
    auto writer = DataWriter(data);
    writer.Write<uint32>(numeric_cast<uint32>(buf.size()));
    writer.WritePtr(buf.data(), buf.size());
    writer.Write<uint32>(numeric_cast<uint32>(lnt_data.size()));
    writer.WritePtr(lnt_data.data(), lnt_data.size());
    return data;
}

void AngelScriptBackend::BindRequiredStuff()
{
    FO_STACK_TRACE_ENTRY();

    BindAngelScriptRemoteCalls(_asEngine.get());

    // Index all functions
    if (_scriptSys) {
        FO_RUNTIME_ASSERT(_asEngine->GetModuleCount() == 1);
        const auto* mod = _asEngine->GetModuleByIndex(0);

        for (AngelScript::asUINT i = 0; i < mod->GetFunctionCount(); i++) {
            auto* func = mod->GetFunctionByIndex(i);
            auto* func_desc = IndexScriptFunc(func);

            _scriptSys->AddGlobalScriptFunc(func_desc);

            // Check for special module init function
            if (func_desc->Call && func_desc->Args.empty() && func_desc->Ret.Kind == ComplexTypeKind::None) {
                if (strvex(func->GetName()).starts_with("ModuleInit") || strvex(func->GetName()).starts_with("module_init")) {
                    const auto priority = strvex(func->GetName()).substring_after('_').to_int32();
                    auto func_wrapper = ScriptFunc<void>(unique_del_ptr<ScriptFuncDesc>(func_desc, [func_ = refcount_ptr(func)](auto&&) { }));
                    _scriptSys->AddInitFunc(std::move(func_wrapper), priority);
                }
            }
        }
    }

    if (HasGameEngine()) {
        auto* engine = GetGameEngine();

        engine->AddLoopCallback([this, engine]() FO_DEFERRED {
            const auto time = engine->GameTime.GetFrameTime();
            _contextMngr->ResumeSuspendedContexts(time);
        });

        _contextMngr = SafeAlloc::MakeUnique<AngelScriptContextManager>(_asEngine.get(), std::chrono::milliseconds(engine->Settings.ScriptOverrunReportTime));
    }
}

void AngelScriptBackend::AddCleanupCallback(function<void()> callback)
{
    FO_STACK_TRACE_ENTRY();

    _cleanupCallbacks.emplace_back(std::move(callback));
}

FO_END_NAMESPACE

#endif
