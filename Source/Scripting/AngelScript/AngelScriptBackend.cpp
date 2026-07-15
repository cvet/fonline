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
#include "Settings.h"

#include <angelscript.h>
#include <preprocessor.h>
FO_DISABLE_WARNINGS_PUSH()
#include <json.hpp>
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE

static constexpr uint32_t AS_BYTECODE_CONTAINER_MAGIC = 0x464F4153; // 'FOAS'
static constexpr uint8_t AS_BYTECODE_POINTER_SIZE = sizeof(void*);
static constexpr uint8_t AS_BYTECODE_ENDIAN_TAG = std::endian::native == std::endian::little ? 1 : 2;
static constexpr AngelScript::asPWORD AS_PREPROCESSOR_LNT_USER_DATA = 5;

static void AngelScriptMessage(const AngelScript::asSMessageInfo* msg, void* param)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const AngelScript::asSMessageInfo> message = msg;
    FO_VERIFY_AND_THROW(message, "AngelScript message info is null");
    const string_view type = message->type == AngelScript::asMSGTYPE_WARNING ? "warning" : (message->type == AngelScript::asMSGTYPE_INFORMATION ? "info" : "error");
    auto as_engine = cast_from_void<AngelScript::asIScriptEngine*>(param);
    FO_VERIFY_AND_THROW(as_engine, "AngelScript engine callback parameter is null");
    auto backend = GetScriptBackend(as_engine);
    auto lnt = cast_from_void<const Preprocessor::LineNumberTranslator*>(as_engine->GetUserData(AS_PREPROCESSOR_LNT_USER_DATA));
    const string_view orig_file = Preprocessor::ResolveOriginalFile(message->row, lnt.get());
    const uint32_t orig_line = Preprocessor::ResolveOriginalLine(message->row, lnt.get());
    const string orig_file_name = strex(string_view {orig_file.data(), orig_file.size()}).extract_file_name().str();
    const nptr<const char> message_chars = message->message;
    const string_view message_text = message_chars ? string_view {message_chars.get()} : string_view {"<no message>"};
    const string formatted_message = strex("{}({},{}): {} : {}", orig_file_name, orig_line, message->col, type, message_text).str();

    backend->SendMessage(formatted_message);
}

static void CleanupScriptFuncDesc(ptr<ScriptFuncDesc> func_desc) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto owned_func_desc = adopt_unique_ptr(func_desc);
    ignore_unused(owned_func_desc);
}

static void CleanupScriptFunction(AngelScript::asIScriptFunction* raw_func)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(raw_func != nullptr, "Script function to clean up is null");
    auto func = make_ptr(raw_func);
    auto func_desc = cast_from_void<ScriptFuncDesc*>(func->GetUserData());

    if (func_desc) {
        CleanupScriptFuncDesc(func_desc);
    }
}

template<typename Allocator>
static void CopyScriptTextToBuffer(std::vector<char, Allocator>& data, const string& text)
{
    FO_STACK_TRACE_ENTRY();

    data.resize(text.size());

    if (text.empty()) {
        return;
    }

    MemCopy(data.data(), text.data(), text.size());
}

static void CleanupLineNumberTranslator(AngelScript::asIScriptEngine* engine) noexcept
{
    FO_STACK_TRACE_ENTRY();

    auto lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(engine->GetUserData(AS_PREPROCESSOR_LNT_USER_DATA));
    Preprocessor::DeleteLineNumberTranslator(lnt.get());
    engine->SetUserData(nullptr, AS_PREPROCESSOR_LNT_USER_DATA);
}

AngelScriptBackend::AngelScriptBackend(ptr<const ScriptSettings> settings) :
    _settings {settings}
{
    FO_STACK_TRACE_ENTRY();
}

AngelScriptBackend::~AngelScriptBackend()
{
    FO_STACK_TRACE_ENTRY();

    if (_debuggerEndpointServer) {
        _debuggerEndpointServer->Stop();
        _debuggerEndpointServer.reset();
    }

    for (const auto& cb : _cleanupCallbacks) {
        cb();
    }

    _contextMngr.reset();

    if (_asEngine) {
        const auto as_engine_ref_count = _asEngine->ShutDownAndRelease();
        FO_STRONG_ASSERT(as_engine_ref_count == 0, "AngelScript engine was not fully released", as_engine_ref_count);
    }

    _meta.reset();
    _scriptSys.reset();
    _engine.reset();
    _entityMngr.reset();

    for (const auto& cb : _postCleanupCallbacks) {
        cb();
    }
}

auto AngelScriptBackend::GetGameEngine() -> ptr<BaseEngine>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_engine, "Missing engine instance");
    return _engine;
}

auto AngelScriptBackend::GetGameEngine() const -> ptr<const BaseEngine>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_engine, "Missing engine instance");
    return _engine;
}

auto AngelScriptBackend::GetEntityMngr() -> ptr<EntityManagerApi>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_entityMngr, "Missing entity manager");
    return _entityMngr;
}

void AngelScriptBackend::RegisterMetadata(ptr<EngineMetadata> meta)
{
    FO_STACK_TRACE_ENTRY();

    auto as_engine = make_nptr(AngelScript::asCreateScriptEngine(ANGELSCRIPT_VERSION));
    FO_VERIFY_AND_THROW(as_engine, "Missing AngelScript engine");

    as_engine->SetUserData(make_nptr(this).void_cast());

    _meta = meta;
    _engine = meta.dyn_cast<BaseEngine>();
    _scriptSys = meta.dyn_cast<ScriptSystem>();
    _entityMngr = meta.dyn_cast<EntityManagerApi>();
    _asEngine = as_engine;

    int32_t as_result;
    FO_AS_VERIFY(as_engine->SetMessageCallback(asFUNCTION(AngelScriptMessage), make_nptr(as_engine.get()).void_cast(), AngelScript::asCALL_CDECL));

    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_ALLOW_UNSAFE_REFERENCES, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_USE_CHARACTER_LITERALS, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_DISALLOW_EMPTY_LIST_ELEMENTS, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_PRIVATE_PROP_AS_PROTECTED, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_REQUIRE_ENUM_SCOPE, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_PROPERTY_ACCESSOR_MODE, 2));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_INIT_GLOBAL_VARS_AFTER_BUILD, true));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_ALWAYS_IMPL_DEFAULT_COPY, 2));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT, 2));

    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_BUILD_WITHOUT_LINE_CUES, !_settings->DebuggerEnabled));
    FO_AS_VERIFY(as_engine->SetEngineProperty(AngelScript::asEP_OPTIMIZE_BYTECODE, !_settings->DebuggerEnabled));

    as_engine->SetFunctionUserDataCleanupCallback(CleanupScriptFunction);
    as_engine->SetEngineUserDataCleanupCallback(CleanupLineNumberTranslator, AS_PREPROCESSOR_LNT_USER_DATA);

    RegisterAngelScriptArray(as_engine);
    RegisterAngelScriptString(as_engine);
    RegisterAngelScriptDict(as_engine);
    RegisterAngelScriptReflection(as_engine);
    RegisterAngelScriptMath(as_engine);
    RegisterAngelScriptEnums(as_engine);
    RegisterAngelScriptTypes(as_engine);
    RegisterAngelScriptEntity(as_engine);
    RegisterAngelScriptTypeProperties(as_engine);
    RegisterAngelScriptGlobals(as_engine);
    RegisterAngelScriptRemoteCalls(as_engine);

    if (_engine && _settings->DebuggerEnabled) {
        if (!_debuggerEndpointServer) {
            try {
                _debuggerEndpointServer.emplace(make_ptr(this));
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
    explicit BinaryStream(ptr<vector<AngelScript::asBYTE>> buf) :
        _binBuf {buf}
    {
        FO_STACK_TRACE_ENTRY();
    }

    auto Write(const void* raw_source, AngelScript::asUINT size) -> int override
    {
        FO_NO_STACK_TRACE_ENTRY();

        auto source = make_nptr(raw_source);

        if (!source || size == 0) {
            return 0;
        }

        _binBuf->resize(_binBuf->size() + size);
        ptr<AngelScript::asBYTE> target = _binBuf->data() + _writePos;
        MemCopy(target, source, size);
        _writePos += size;

        return 0;
    }

    auto Read(void* raw_target, AngelScript::asUINT size) -> int override
    {
        FO_NO_STACK_TRACE_ENTRY();

        auto target = make_nptr(raw_target);

        if (!target || size == 0) {
            return 0;
        }

        if (_readPos + size > _binBuf->size()) {
            return -1;
        }

        ptr<const AngelScript::asBYTE> source = _binBuf->data() + _readPos;
        MemCopy(target, source, size);
        _readPos += size;

        return 0;
    }

private:
    ptr<vector<AngelScript::asBYTE>> _binBuf;
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

    FO_VERIFY_AND_THROW(script_bin_files.GetFilesCount() == 1, "Resource pack must contain exactly one script bytecode file for this engine side", _meta->GetSide(), script_bin_files.GetFilesCount());
    const auto script_bin_file = File::Load(*script_bin_files.begin());
    const_span<uint8_t> script_bin = script_bin_file.GetDataSpan();

    FO_VERIFY_AND_THROW(_asEngine->GetModuleCount() == 0, "AngelScript engine must not contain modules before loading bytecode", _asEngine->GetModuleCount());
    FO_VERIFY_AND_THROW(!script_bin.empty(), "AngelScript bytecode resource is empty", script_bin_file.GetPath(), _meta->GetSide());

    auto reader = DataReader({script_bin.data(), script_bin.size()});

    const auto container_magic = reader.Read<uint32_t>();

    if (container_magic != AS_BYTECODE_CONTAINER_MAGIC) {
        throw ScriptException("Incompatible script bytecode container");
    }

    const auto source_pointer_size = reader.Read<uint8_t>();
    const auto source_endian_tag = reader.Read<uint8_t>();

    if (source_pointer_size != AS_BYTECODE_POINTER_SIZE) {
        WriteLog("Loading cross-platform bytecode: compiled with {}-bit pointers, running with {}-bit pointers", source_pointer_size * 8, AS_BYTECODE_POINTER_SIZE * 8);
    }
    if (source_endian_tag != AS_BYTECODE_ENDIAN_TAG) {
        WriteLog("Loading cross-endian bytecode: source endian tag {}, local endian tag {}", source_endian_tag, AS_BYTECODE_ENDIAN_TAG);
    }

    vector<AngelScript::asBYTE> buf(reader.Read<uint32_t>());
    FO_VERIFY_AND_THROW(!buf.empty(), "AngelScript bytecode payload size is zero");
    reader.ReadObjectArray(span<AngelScript::asBYTE> {buf.data(), buf.size()});

    std::vector<uint8_t> lnt_data(reader.Read<uint32_t>());
    FO_VERIFY_AND_THROW(!buf.empty(), "AngelScript bytecode container has an empty script bytecode payload", script_bin_file.GetPath(), script_bin.size());
    FO_VERIFY_AND_THROW(!lnt_data.empty(), "AngelScript bytecode container has an empty line-number table payload", script_bin_file.GetPath(), script_bin.size(), buf.size());
    reader.ReadBytes({lnt_data.data(), lnt_data.size()});

    nptr<AngelScript::asIScriptModule> mod = _asEngine->GetModule("Root", AngelScript::asGM_ALWAYS_CREATE);

    if (!mod) {
        throw ScriptException("Create root module fail");
    }

    nptr<Preprocessor::LineNumberTranslator> lnt = Preprocessor::RestoreLineNumberTranslator(lnt_data);
    FO_VERIFY_AND_THROW(lnt, "Failed to restore line-number translator");
    CleanupLineNumberTranslator(_asEngine.get());
    _asEngine->SetUserData(make_nptr(lnt.get()).void_cast(), AS_PREPROCESSOR_LNT_USER_DATA);

    BinaryStream binary {&buf};
    int32_t as_result = mod->LoadByteCode(&binary);

    if (as_result < 0) {
        throw ScriptException("Can't load binary", as_result);
    }

    // Validate loaded bytecode
    for (AngelScript::asUINT i = 0; i < mod->GetFunctionCount(); i++) {
        nptr<AngelScript::asIScriptFunction> func = mod->GetFunctionByIndex(i);

        if (!func) {
            continue;
        }

        // Walk the bytecode to verify instruction boundaries are well-formed
        AngelScript::asUINT bc_length = 0;
        nptr<const AngelScript::asDWORD> bc = func->GetByteCode(&bc_length);

        if (!bc || bc_length == 0) {
            continue;
        }

        AngelScript::asUINT pos = 0;

        while (pos < bc_length) {
            const auto opcode = static_cast<AngelScript::asEBCInstr>(static_cast<uint8_t>(bc[pos]));
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

    FO_VERIFY_AND_THROW(script_bin.size() >= sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint32_t) + buf.size() + sizeof(uint32_t) + lnt_data.size(), "AngelScript bytecode container is shorter than its declared payload sizes", script_bin_file.GetPath(), script_bin.size(), sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint32_t) + buf.size() + sizeof(uint32_t) + lnt_data.size(), buf.size(), lnt_data.size());
    const auto records = DeserializeFunctionAttributeRecords(reader);
    reader.VerifyEnd();

    if (const auto bind_error = BindFunctionAttributeRecords(mod, records, &_settings->ExtraDirectCallBlockingAttributes); !bind_error.empty()) {
        throw ScriptException(bind_error);
    }
    if (const auto usage_error = ValidateAttributedFunctionUsage(mod, lnt, &_settings->AttributedFunctionDirectCallAllowedNamespaces, &_settings->ExtraDirectCallBlockingAttributes); !usage_error.empty()) {
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

auto AngelScriptBackend::CompileTextScripts(const vector<File>& files) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_asEngine->GetModuleCount() == 0, "AngelScript engine must not contain modules before compiling scripts", _asEngine->GetModuleCount());

    // File loader
    class ScriptLoader : public Preprocessor::FileLoader
    {
    public:
        ScriptLoader(nptr<const string> root, ptr<const map<string, string>> files) :
            _rootScript {root},
            _scriptFiles {files}
        {
            FO_STACK_TRACE_ENTRY();
        }

        auto LoadFile(const std::string& dir, const std::string& file_name, std::vector<char>& data, std::string& file_path) -> bool override
        {
            FO_STACK_TRACE_ENTRY();

            if (_rootScript) {
                CopyScriptTextToBuffer(data, *_rootScript);
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

                CopyScriptTextToBuffer(data, it->second);
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
        nptr<const string> _rootScript {};
        ptr<const map<string, string>> _scriptFiles;
        int32_t _includeDeep {};
    };

    map<string, string> final_script_files;
    vector<tuple<int32_t, string, string>> final_script_files_order;

    for (const auto& script_file : files) {
        string script_name = string(script_file.GetNameNoExt());
        string script_path = script_file.GetDataSource()->IsDiskDir() ? string(script_file.GetDiskPath()) : string(script_file.GetPath());
        string script_content = script_file.GetStr();

        const auto line_sep = script_content.find('\n');
        const auto first_line = script_content.substr(0, line_sep);

        int32_t sort = 0;
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

    auto preprocessor_context = make_unique_del_ptr(make_ptr(Preprocessor::CreateContext()), [](ptr<Preprocessor::Context> ctx) FO_DEFERRED { Preprocessor::DeleteContext(ctx.get()); });

    Preprocessor::UndefAll(preprocessor_context.get());

    switch (_meta->GetSide()) {
    case EngineSideKind::ServerSide:
        Preprocessor::Define(preprocessor_context.get(), "SERVER 1");
        Preprocessor::Define(preprocessor_context.get(), "CLIENT 0");
        Preprocessor::Define(preprocessor_context.get(), "MAPPER 0");
        break;
    case EngineSideKind::ClientSide:
        Preprocessor::Define(preprocessor_context.get(), "SERVER 0");
        Preprocessor::Define(preprocessor_context.get(), "CLIENT 1");
        Preprocessor::Define(preprocessor_context.get(), "MAPPER 0");
        break;
    case EngineSideKind::MapperSide:
        Preprocessor::Define(preprocessor_context.get(), "SERVER 0");
        Preprocessor::Define(preprocessor_context.get(), "CLIENT 0");
        Preprocessor::Define(preprocessor_context.get(), "MAPPER 1");
        break;
    }

#if FO_MANAGED_SCRIPTING
    Preprocessor::Define(preprocessor_context.get(), "MANAGED 1");
#else
    Preprocessor::Define(preprocessor_context.get(), "MANAGED 0");
#endif

    auto loader = ScriptLoader(&root_script, &final_script_files);
    Preprocessor::StringOutStream errors;
    Preprocessor::LexemList lexems;
    const auto errors_count = Preprocessor::PreprocessToLexems(preprocessor_context.get(), "", lexems, &errors, &loader);

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
    Preprocessor::PrintLexemList(preprocessor_context.get(), lexems, result);

    nptr<Preprocessor::LineNumberTranslator> lnt = Preprocessor::GetLineNumberTranslator(preprocessor_context.get());
    FO_VERIFY_AND_THROW(lnt, "Missing line-number translator");
    CleanupLineNumberTranslator(_asEngine.get());
    _asEngine->SetUserData(make_nptr(lnt.get()).void_cast(), AS_PREPROCESSOR_LNT_USER_DATA);

    nptr<AngelScript::asIScriptModule> mod = _asEngine->GetModule("Root", AngelScript::asGM_ALWAYS_CREATE);

    if (!mod) {
        throw ScriptCompilerException("Create root module failed");
    }

    int32_t as_result = mod->AddScriptSection("Root", result.String.c_str());

    if (as_result < 0) {
        throw ScriptCompilerException("Unable to add script section", as_result);
    }

    as_result = mod->Build();

    if (as_result < 0) {
        throw ScriptCompilerException("Unable to build module", as_result);
    }

    if (const auto bind_error = BindFunctionAttributeRecords(mod, parsed_attributes, &_settings->ExtraDirectCallBlockingAttributes); !bind_error.empty()) {
        throw ScriptCompilerException("Unable to bind function attributes", bind_error);
    }
    if (const auto usage_error = ValidateAttributedFunctionUsage(mod, lnt, &_settings->AttributedFunctionDirectCallAllowedNamespaces, &_settings->ExtraDirectCallBlockingAttributes); !usage_error.empty()) {
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
    BinaryStream binary {&buf};
    as_result = mod->SaveByteCode(&binary);

    if (as_result < 0) {
        throw ScriptCompilerException("Unable to save byte code", as_result);
    }

    std::vector<uint8_t> lnt_data;
    Preprocessor::StoreLineNumberTranslator(lnt.get(), lnt_data);

    vector<uint8_t> data;
    auto writer = DataWriter(data);
    writer.Write<uint32_t>(AS_BYTECODE_CONTAINER_MAGIC);
    writer.Write<uint8_t>(AS_BYTECODE_POINTER_SIZE);
    writer.Write<uint8_t>(AS_BYTECODE_ENDIAN_TAG);
    writer.Write<uint32_t>(numeric_cast<uint32_t>(buf.size()));
    if (!buf.empty()) {
        writer.WriteObjectArray(const_span<AngelScript::asBYTE> {buf.data(), buf.size()});
    }
    writer.Write<uint32_t>(numeric_cast<uint32_t>(lnt_data.size()));
    if (!lnt_data.empty()) {
        writer.WriteBytes({lnt_data.data(), lnt_data.size()});
    }
    SerializeFunctionAttributeRecords(writer, parsed_attributes);
    return data;
}

void AngelScriptBackend::BindRequiredStuff()
{
    FO_STACK_TRACE_ENTRY();

    BindAngelScriptRemoteCalls(_asEngine);

    if (HasEntityMngr() && _asEngine->GetModuleCount() == 1) {
        nptr<const AngelScript::asIScriptModule> mod = _asEngine->GetModuleByIndex(0);
        FO_VERIFY_AND_THROW(mod, "Missing compiled AngelScript module");
        const auto global_count = mod->GetGlobalVarCount();

        vector<string> violations;

        for (AngelScript::asUINT i = 0; i < global_count; i++) {
            nptr<const char> name;
            nptr<const char> name_space;
            int type_id = 0;
            bool is_const = false;

            mod->GetGlobalVar(i, name.get_pp(), name_space.get_pp(), &type_id, &is_const);

            const nptr<const char> decl = mod->GetGlobalVarDeclaration(i, true);
            const auto decl_str = decl ? string_view {decl.get()} : string_view {};
            const auto explicitly_const = decl_str.starts_with("const ");

            if (is_const || explicitly_const) {
                continue;
            }

            const auto ns_view = name_space ? string_view {name_space.get()} : string_view {};

            if (IsScriptNamespaceAllowed(ns_view, _settings->MutableGlobalsAllowedNamespaces)) {
                continue;
            }

            violations.emplace_back(!decl_str.empty() ? string(decl_str) : (name ? string(name.get()) : string("<unknown>")));
        }

        if (!violations.empty()) {
            string msg = strex("Found {} mutable global variable(s):", violations.size());

            for (const auto& v : violations) {
                msg += strex("\n  - {}", v);
            }

            throw ScriptCompilerException(msg);
        }
    }

    // Index all functions
    if (_scriptSys) {
        FO_VERIFY_AND_THROW(_asEngine->GetModuleCount() == 1, "AngelScript engine must contain one compiled module before indexing functions", _asEngine->GetModuleCount());
        nptr<AngelScript::asIScriptModule> mod = _asEngine->GetModuleByIndex(0);
        FO_VERIFY_AND_THROW(mod, "Missing compiled AngelScript module");

        for (AngelScript::asUINT i = 0; i < mod->GetFunctionCount(); i++) {
            nptr<AngelScript::asIScriptFunction> func = mod->GetFunctionByIndex(i);
            FO_VERIFY_AND_THROW(func, "Module function lookup returned null");
            auto func_desc = IndexScriptFunc(func);

            _scriptSys->AddGlobalScriptFunc(func_desc);

            // Check for special module init functions
            if (func_desc->Call && func_desc->Args.empty() && func_desc->Ret.Kind == ComplexTypeKind::None) {
                auto func_wrapper = ScriptFunc<void>(unique_del_nptr<ScriptFuncDesc>(MakeAngelScriptFuncDescBorrow(func_desc, refcount_ptr<AngelScript::asIScriptFunction>::from_add_ref(func.get()))));

                if (const auto raw_init_attr = FindFunctionAttribute(func.get(), "ModuleInit"); !raw_init_attr.empty()) {
                    int32_t priority = 0;
                    const auto parsed = TryParseModuleFuncPriority(raw_init_attr, "ModuleInit", priority);
                    FO_VERIFY_AND_THROW(parsed, "Failed to parse serialized script metadata");
                    _scriptSys->AddInitFunc(std::move(func_wrapper), priority);
                }
            }
        }
    }

    if (HasGameEngine()) {
        auto engine = GetGameEngine();

        const auto overrun_report_time = std::chrono::milliseconds(_settings->OverrunReportTime);

        _contextMngr.emplace(_asEngine, engine, overrun_report_time, [this](string_view reason, string_view text, string_view source_path, std::optional<uint32_t> line, string_view function_name) {
            if (_debuggerEndpointServer) {
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

        _contextMngr->SetContextSetupCallback([this](ptr<AngelScript::asIScriptContext> ctx, AngelScriptContextSetupReason reason) {
            if (_debuggerEndpointServer) {
                _debuggerEndpointServer->SetupContext(ctx, reason);
            }
        });

        _contextMngr->SetDelayedScheduler([engine](timespan delay, function<void()> body) mutable { //
            engine->ScheduleDelayedCallback(delay, std::move(body));
        });
    }
}

auto AngelScriptBackend::TryParseModuleFuncPriority(string_view raw_attribute, string_view attribute_name, int32_t& priority) noexcept -> bool
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
    auto parsed_priority = int32_t {};
    const char* begin = args.data();
    const char* end = begin + args.size();
    const auto [parsed_end, ec] = std::from_chars(begin, end, parsed_priority);

    if (ec != std::errc {} || parsed_end != end) {
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
