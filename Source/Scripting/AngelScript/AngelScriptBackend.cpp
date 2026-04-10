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
#include "AngelScriptAttributes.h"
#include "AngelScriptCall.h"
#include "AngelScriptContext.h"
#include "AngelScriptDebugger.h"
#include "AngelScriptDict.h"
#include "AngelScriptEntity.h"
#include "AngelScriptGlobals.h"
#include "AngelScriptHelpers.h"
#include "AngelScriptMath.h"
#include "AngelScriptReflection.h"
#include "AngelScriptRemoteCalls.h"
#include "AngelScriptString.h"
#include "AngelScriptTypes.h"

#include <angelscript.h>
#include <json.hpp>
#include <preprocessor.h>

FO_BEGIN_NAMESPACE

static constexpr uint32 AS_BYTECODE_CONTAINER_MAGIC = 0x464F4153; // 'FOAS'
static constexpr uint8 AS_BYTECODE_POINTER_SIZE = sizeof(void*);
static constexpr uint8 AS_BYTECODE_ENDIAN_TAG = std::endian::native == std::endian::little ? 1 : 2;
static constexpr AngelScript::asPWORD AS_PREPROCESSOR_LNT_USER_DATA = 5;

static void AngelScriptMessage(const AngelScript::asSMessageInfo* msg, void* param)
{
    FO_STACK_TRACE_ENTRY();

    const char* type = msg->type == AngelScript::asMSGTYPE_WARNING ? "warning" : (msg->type == AngelScript::asMSGTYPE_INFORMATION ? "info" : "error");
    auto* as_engine = cast_from_void<AngelScript::asIScriptEngine*>(param);
    const auto* backend = GetScriptBackend(as_engine);
    const auto* lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(as_engine->GetUserData(AS_PREPROCESSOR_LNT_USER_DATA));
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

    auto endpoint_server = std::move(_debuggerEndpointServer);

    if (endpoint_server) {
        endpoint_server->Stop();
    }

    for (const auto& cb : _cleanupCallbacks) {
        cb();
    }

    _contextMngr.reset();

    _meta.reset();
    _scriptSys.reset();
    _engine.reset();
    _entityMngr.reset();

    if (_asEngine) {
        const auto as_engine_ref_count = _asEngine->ShutDownAndRelease();
        FO_STRONG_ASSERT(as_engine_ref_count == 0);
    }

    for (const auto& cb : _postCleanupCallbacks) {
        cb();
    }
}

auto AngelScriptBackend::GetGameEngine() -> BaseEngine*
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_engine);
    return _engine.get();
}

auto AngelScriptBackend::GetGameEngine() const -> const BaseEngine*
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
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_DISALLOW_EMPTY_LIST_ELEMENTS, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_PRIVATE_PROP_AS_PROTECTED, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_REQUIRE_ENUM_SCOPE, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_PROPERTY_ACCESSOR_MODE, 2));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_INIT_GLOBAL_VARS_AFTER_BUILD, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_ALWAYS_IMPL_DEFAULT_COPY, 2));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT, 2));

    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_BUILD_WITHOUT_LINE_CUES, false));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_OPTIMIZE_BYTECODE, false));

    as_engine->SetFunctionUserDataCleanupCallback(CleanupScriptFunction);
    as_engine->SetFunctionUserDataCleanupCallback(CleanupScriptFunctionAttributes, AS_FUNC_ATTRIBUTES_USER_DATA);

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

    if (_engine && _engine->Settings.DebuggerEnabled) {
        if (_debuggerEndpointServer == nullptr) {
            try {
                _debuggerEndpointServer = SafeAlloc::MakeUnique<DebuggerEndpointServer>(this);
            }
            catch (...) {
                WriteLog("Can't start AngelScript debugger endpoint server");
            }
        }
    }
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

    auto Write(const void* ptr, AngelScript::asUINT size) -> int override
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (ptr == nullptr || size == 0) {
            return 0;
        }

        _binBuf.resize(_binBuf.size() + size);
        MemCopy(&_binBuf[_writePos], ptr, size);
        _writePos += size;

        return 0;
    }

    auto Read(void* ptr, AngelScript::asUINT size) -> int override
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (ptr == nullptr || size == 0) {
            return 0;
        }

        if (_readPos + size > _binBuf.size()) {
            return -1;
        }

        MemCopy(ptr, &_binBuf[_readPos], size);
        _readPos += size;

        return 0;
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

    const auto container_magic = reader.Read<uint32>();

    if (container_magic != AS_BYTECODE_CONTAINER_MAGIC) {
        throw ScriptException("Incompatible script bytecode container");
    }

    const auto source_pointer_size = reader.Read<uint8>();
    const auto source_endian_tag = reader.Read<uint8>();

    if (source_pointer_size != AS_BYTECODE_POINTER_SIZE) {
        WriteLog("Loading cross-platform bytecode: compiled with {}-bit pointers, running with {}-bit pointers", source_pointer_size * 8, AS_BYTECODE_POINTER_SIZE * 8);
    }
    if (source_endian_tag != AS_BYTECODE_ENDIAN_TAG) {
        WriteLog("Loading cross-endian bytecode: source endian tag {}, local endian tag {}", source_endian_tag, AS_BYTECODE_ENDIAN_TAG);
    }

    vector<AngelScript::asBYTE> buf(reader.Read<uint32>());
    MemCopy(buf.data(), reader.ReadPtr<AngelScript::asBYTE>(buf.size()), buf.size());

    std::vector<uint8> lnt_data(reader.Read<uint32>());
    MemCopy(lnt_data.data(), reader.ReadPtr<uint8>(lnt_data.size()), lnt_data.size());
    FO_RUNTIME_ASSERT(!buf.empty());
    FO_RUNTIME_ASSERT(!lnt_data.empty());

    auto* mod = _asEngine->GetModule("Root", AngelScript::asGM_ALWAYS_CREATE);

    if (mod == nullptr) {
        throw ScriptException("Create root module fail");
    }

    auto* lnt = Preprocessor::RestoreLineNumberTranslator(lnt_data);
    _asEngine->SetUserData(cast_to_void(lnt), AS_PREPROCESSOR_LNT_USER_DATA);

    BinaryStream binary {buf};
    int32 as_result = mod->LoadByteCode(&binary);

    if (as_result < 0) {
        throw ScriptException("Can't load binary", as_result);
    }

    // Validate loaded bytecode
    for (AngelScript::asUINT i = 0; i < mod->GetFunctionCount(); i++) {
        auto* func = mod->GetFunctionByIndex(i);

        if (func == nullptr) {
            continue;
        }

        // Walk the bytecode to verify instruction boundaries are well-formed
        AngelScript::asUINT bc_length = 0;
        auto* bc = func->GetByteCode(&bc_length);

        if (bc == nullptr || bc_length == 0) {
            continue;
        }

        AngelScript::asUINT pos = 0;

        while (pos < bc_length) {
            const auto opcode = static_cast<AngelScript::asEBCInstr>(static_cast<uint8>(bc[pos]));
            const auto instr_size = AngelScript::asBCTypeSize[AngelScript::asBCInfo[opcode].type];

            if (instr_size == 0 || pos + instr_size > bc_length) {
                throw ScriptException("Bytecode validation failed - invalid instruction boundary", func->GetName(), pos, opcode, instr_size, bc_length);
            }

            pos += instr_size;
        }

        if (pos != bc_length) {
            throw ScriptException("Bytecode validation failed - instruction boundary mismatch", func->GetName(), pos, bc_length);
        }
    }

    FO_RUNTIME_ASSERT(script_bin.size() >= sizeof(uint32) + sizeof(uint8) + sizeof(uint8) + sizeof(uint32) + buf.size() + sizeof(uint32) + lnt_data.size());
    const auto records = DeserializeFunctionAttributeRecords(reader);
    reader.VerifyEnd();

    if (const auto bind_error = BindFunctionAttributeRecords(mod, records); !bind_error.empty()) {
        throw ScriptException(bind_error);
    }
    if (const auto usage_error = ValidateAttributedFunctionUsage(mod, lnt); !usage_error.empty()) {
        throw ScriptException(usage_error);
    }
    if (const auto admin_remote_call_error = ValidateAdminRemoteCallAttributes(mod, lnt); !admin_remote_call_error.empty()) {
        throw ScriptException(admin_remote_call_error);
    }
    if (const auto event_error = ValidateEventSubscriptions(mod, lnt); !event_error.empty()) {
        throw ScriptException(event_error);
    }
    if (const auto remote_call_error = ValidateAngelScriptRemoteCallAttributes(mod, *_meta, lnt); !remote_call_error.empty()) {
        throw ScriptException(remote_call_error);
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

            data.resize(0);

            const auto load_from_memory = [&](string_view path) -> bool {
                const auto it = _scriptFiles->find(string(path));

                if (it == _scriptFiles->end()) {
                    return false;
                }

                data.resize(it->second.size());
                MemCopy(data.data(), it->second.data(), it->second.size());
                file_path = string(path);
                return true;
            };

            if (!dir.empty()) {
                const auto combined_path = strex(dir).combine_path(file_name).str();

                if (load_from_memory(combined_path)) {
                    return true;
                }
            }

            if (load_from_memory(file_name)) {
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
        string script_path = script_file.GetDataSource()->IsDiskDir() ? string(script_file.GetDiskPath()) : string(script_file.GetPath());
        string script_content = script_file.GetStr();

        const auto line_sep = script_content.find('\n');
        const auto first_line = script_content.substr(0, line_sep);

        int32 sort = 0;
        const auto sort_pos = first_line.find("Sort ");

        if (sort_pos != string::npos) {
            sort = strvex(first_line.substr(sort_pos + "Sort "_len)).substring_until(' ').to_int32();
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
    Preprocessor::StringOutStream errors;
    Preprocessor::LexemList lexems;
    const auto errors_count = Preprocessor::PreprocessToLexems(preprocessor_context, "", lexems, &errors, &loader);

    while (!errors.String.empty() && errors.String.back() == '\n') {
        errors.String.pop_back();
    }

    if (errors_count > 0) {
        throw ScriptCompilerException("Preprocessor failed", errors.String);
    }
    else if (!errors.String.empty()) {
        WriteLog("Preprocessor message: {}", errors.String);
    }

    string attribute_errors;
    const auto parsed_attributes = ParseFunctionAttributeRecords(preprocessor_context, lexems, attribute_errors);
    if (!attribute_errors.empty()) {
        throw ScriptCompilerException("Function attribute parsing failed", attribute_errors);
    }

    Preprocessor::StringOutStream result;
    Preprocessor::PrintLexemList(preprocessor_context, lexems, result);

    Preprocessor::LineNumberTranslator* lnt = Preprocessor::GetLineNumberTranslator(preprocessor_context);
    _asEngine->SetUserData(cast_to_void(lnt), AS_PREPROCESSOR_LNT_USER_DATA);

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

    if (const auto bind_error = BindFunctionAttributeRecords(mod, parsed_attributes); !bind_error.empty()) {
        throw ScriptCompilerException("Unable to bind function attributes", bind_error);
    }
    if (const auto usage_error = ValidateAttributedFunctionUsage(mod, lnt); !usage_error.empty()) {
        throw ScriptCompilerException("Attributed function usage validation failed", usage_error);
    }
    if (const auto special_attr_error = ValidateSpecialFunctionAttributes(mod, lnt); !special_attr_error.empty()) {
        throw ScriptCompilerException("Special function attribute validation failed", special_attr_error);
    }
    if (const auto admin_remote_call_error = ValidateAdminRemoteCallAttributes(mod, lnt); !admin_remote_call_error.empty()) {
        throw ScriptCompilerException("Admin remote call attribute validation failed", admin_remote_call_error);
    }
    if (const auto event_error = ValidateEventSubscriptions(mod, lnt); !event_error.empty()) {
        throw ScriptCompilerException("Callback attribute validation failed", event_error);
    }
    if (const auto remote_call_error = ValidateAngelScriptRemoteCallAttributes(mod, *_meta, lnt); !remote_call_error.empty()) {
        throw ScriptCompilerException("Remote call attribute validation failed", remote_call_error);
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
    writer.Write<uint32>(AS_BYTECODE_CONTAINER_MAGIC);
    writer.Write<uint8>(AS_BYTECODE_POINTER_SIZE);
    writer.Write<uint8>(AS_BYTECODE_ENDIAN_TAG);
    writer.Write<uint32>(numeric_cast<uint32>(buf.size()));
    writer.WritePtr(buf.data(), buf.size());
    writer.Write<uint32>(numeric_cast<uint32>(lnt_data.size()));
    writer.WritePtr(lnt_data.data(), lnt_data.size());
    SerializeFunctionAttributeRecords(writer, parsed_attributes);
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

            // Check for special module init functions
            if (func_desc->Call && func_desc->Args.empty() && func_desc->Ret.Kind == ComplexTypeKind::None) {
                auto func_wrapper = ScriptFunc<void>(unique_del_ptr<ScriptFuncDesc>(func_desc, [func_ = refcount_ptr(func)](auto&&) { }));

                if (const auto raw_init_attr = FindFunctionAttribute(func, "ModuleInit"); !raw_init_attr.empty()) {
                    auto priority = int32 {};
                    const auto parsed = TryParseModuleFuncPriority(raw_init_attr, "ModuleInit", priority);
                    FO_RUNTIME_ASSERT(parsed);
                    _scriptSys->AddInitFunc(std::move(func_wrapper), priority);
                }
            }
        }
    }

    if (HasGameEngine()) {
        auto* engine = GetGameEngine();

        engine->AddLoopCallback([this, engine]() FO_DEFERRED {
            const auto time = engine->GameTime.GetFrameTime();

            if (_debuggerEndpointServer == nullptr || !_debuggerEndpointServer->IsPaused()) {
                _contextMngr->ResumeSuspendedContexts(time);
            }
        });

        _contextMngr = SafeAlloc::MakeUnique<AngelScriptContextManager>(_asEngine.get(), std::chrono::milliseconds(engine->Settings.OverrunReportTime), [this](string_view reason, string_view text, string_view source_path, std::optional<uint32> line, string_view function_name) {
            if (_debuggerEndpointServer != nullptr) {
                nlohmann::json body;
                body["reason"] = reason;

                if (!text.empty()) {
                    body["text"] = text;
                }
                if (!source_path.empty()) {
                    body["source"] = source_path;
                }
                if (line.has_value()) {
                    body["line"] = line.value();
                }
                if (!function_name.empty()) {
                    body["function"] = function_name;
                }

                _debuggerEndpointServer->EmitEvent("stopped", body.dump());
            }
        });

        _contextMngr->SetContextSetupCallback([this](AngelScript::asIScriptContext* ctx, AngelScriptContextSetupReason reason) {
            if (_debuggerEndpointServer != nullptr) {
                _debuggerEndpointServer->SetupContext(ctx, reason);
            }
        });
    }
}

auto AngelScriptBackend::TryParseModuleFuncPriority(string_view raw_attribute, string_view attribute_name, int32& priority) noexcept -> bool
{
    priority = 0;

    if (raw_attribute.empty()) {
        return false;
    }

    if (raw_attribute == attribute_name) {
        return true;
    }

    if (!raw_attribute.starts_with(attribute_name) || raw_attribute.length() <= attribute_name.length() + 2 || raw_attribute[attribute_name.length()] != '(' || raw_attribute.back() != ')') {
        return false;
    }

    const auto args = raw_attribute.substr(attribute_name.length() + 1, raw_attribute.length() - attribute_name.length() - 2);
    auto parsed_priority = int32 {};
    const auto* begin = args.data();
    const auto* end = begin + args.length();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed_priority);
    if (ec != std::errc {} || ptr != end) {
        return false;
    }

    priority = parsed_priority;
    return true;
}

void AngelScriptBackend::AddCleanupCallback(function<void()> callback)
{
    FO_STACK_TRACE_ENTRY();

    _cleanupCallbacks.emplace_back(std::move(callback));
}

void AngelScriptBackend::AddPostCleanupCallback(function<void()> callback)
{
    FO_STACK_TRACE_ENTRY();

    _postCleanupCallbacks.emplace_back(std::move(callback));
}

FO_END_NAMESPACE

#endif
