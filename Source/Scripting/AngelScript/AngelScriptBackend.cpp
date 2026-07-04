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
    FO_VERIFY_AND_THROW(!!message, "AngelScript message info is null");
    const string_view type = message->type == AngelScript::asMSGTYPE_WARNING ? "warning" : (message->type == AngelScript::asMSGTYPE_INFORMATION ? "info" : "error");
    nptr<AngelScript::asIScriptEngine> as_engine = cast_from_void<AngelScript::asIScriptEngine*>(param);
    FO_VERIFY_AND_THROW(!!as_engine, "AngelScript engine callback parameter is null");
    auto backend = GetScriptBackend(as_engine.as_ptr());
    nptr<const Preprocessor::LineNumberTranslator> lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(as_engine->GetUserData(AS_PREPROCESSOR_LNT_USER_DATA));
    const string_view orig_file = Preprocessor::ResolveOriginalFile(message->row, lnt.get());
    const uint32_t orig_line = Preprocessor::ResolveOriginalLine(message->row, lnt.get());
    const string orig_file_name = strex(string_view {orig_file.data(), orig_file.size()}).extract_file_name().str();
    const nptr<const char> nullable_message_text = message->message;
    const string_view message_text = nullable_message_text ? string_view {nullable_message_text.get()} : string_view {"<no message>"};
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
    ptr<AngelScript::asIScriptFunction> func = raw_func;
    nptr<ScriptFuncDesc> func_desc = cast_from_void<ScriptFuncDesc*>(func->GetUserData());

    if (func_desc) {
        CleanupScriptFuncDesc(func_desc.as_ptr());
    }
}

static auto GetGlobalHandleSlot(ptr<AngelScript::asIScriptModule> mod, AngelScript::asUINT var_index) noexcept -> nptr<void*>
{
    FO_STACK_TRACE_ENTRY();

    static_assert(sizeof(void*) == sizeof(void**));

    nptr<void> slot_address = mod->GetAddressOfGlobalVar(var_index);
    return slot_address ? std::bit_cast<void**>(slot_address.get()) : nullptr;
}

static auto ReadGlobalFunctionHandle(ptr<void*> slot) noexcept -> ptr<AngelScript::asIScriptFunction>
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> value = *slot;
    FO_STRONG_ASSERT(value, "Global function handle slot is empty");
    return cast_from_void<AngelScript::asIScriptFunction*>(value.get());
}

static void WriteGlobalHandleSlot(ptr<void*> slot, nptr<void> value) noexcept
{
    FO_STACK_TRACE_ENTRY();

    *slot = value.get();
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

    auto* lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(engine->GetUserData(AS_PREPROCESSOR_LNT_USER_DATA));
    Preprocessor::DeleteLineNumberTranslator(lnt);
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
        auto endpoint_server = _debuggerEndpointServer.as_ptr();
        endpoint_server->Stop();
        _debuggerEndpointServer.reset();
    }

    for (const auto& cb : _cleanupCallbacks) {
        cb();
    }

    _contextMngr.reset();

    _meta.reset();
    _scriptSys.reset();
    _engine.reset();
    _entityMngr.reset();

    ReleaseScriptGlobalsAndReportGC();

    if (_asEngine) {
        const auto as_engine_ref_count = _asEngine->ShutDownAndRelease();
        FO_STRONG_ASSERT(as_engine_ref_count == 0, "AngelScript engine was not fully released", as_engine_ref_count);
    }

    for (const auto& cb : _postCleanupCallbacks) {
        cb();
    }
}

auto AngelScriptBackend::GetGameEngine() -> ptr<BaseEngine>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_engine, "Missing engine instance");
    return _engine.as_ptr();
}

auto AngelScriptBackend::GetGameEngine() const -> ptr<const BaseEngine>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_engine, "Missing engine instance");
    return _engine.as_ptr();
}

auto AngelScriptBackend::GetEntityMngr() -> ptr<EntityManagerApi>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_entityMngr, "Missing entity manager");
    return _entityMngr.as_ptr();
}

void AngelScriptBackend::RegisterMetadata(ptr<EngineMetadata> meta)
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asIScriptEngine> nullable_as_engine = AngelScript::asCreateScriptEngine(ANGELSCRIPT_VERSION);
    FO_VERIFY_AND_THROW(nullable_as_engine, "Missing AngelScript engine");
    auto as_engine = nullable_as_engine.as_ptr();

    as_engine->SetUserData(cast_to_void(this));

    _meta = meta;
    _engine = meta.dyn_cast<BaseEngine>();
    _scriptSys = meta.dyn_cast<ScriptSystem>();
    _entityMngr = meta.dyn_cast<EntityManagerApi>();
    _asEngine = as_engine;

    int32_t as_result;
    FO_AS_VERIFY(as_engine->SetMessageCallback(asFUNCTION(AngelScriptMessage), cast_to_void(as_engine.get()), AngelScript::asCALL_CDECL));

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
    explicit BinaryStream(ptr<vector<AngelScript::asBYTE>> buf) :
        _binBuf {buf}
    {
        FO_STACK_TRACE_ENTRY();
    }

    auto Write(const void* raw_source, AngelScript::asUINT size) -> int override
    {
        FO_NO_STACK_TRACE_ENTRY();

        nptr<const void> source = raw_source;

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

        nptr<void> target = raw_target;

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

    nptr<AngelScript::asIScriptModule> nullable_mod = _asEngine->GetModule("Root", AngelScript::asGM_ALWAYS_CREATE);

    if (!nullable_mod) {
        throw ScriptException("Create root module fail");
    }

    auto mod = nullable_mod.as_ptr();
    nptr<Preprocessor::LineNumberTranslator> nullable_lnt = Preprocessor::RestoreLineNumberTranslator(lnt_data);
    FO_VERIFY_AND_THROW(!!nullable_lnt, "Failed to restore line-number translator");
    auto lnt = nullable_lnt.as_ptr();
    CleanupLineNumberTranslator(_asEngine.get());
    _asEngine->SetUserData(cast_to_void(lnt.get()), AS_PREPROCESSOR_LNT_USER_DATA);

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
    if (const auto usage_error = ValidateAttributedFunctionUsage(mod, nullable_lnt, &_settings->AttributedFunctionDirectCallAllowedNamespaces, &_settings->ExtraDirectCallBlockingAttributes); !usage_error.empty()) {
        throw ScriptException(usage_error);
    }
    if (const auto admin_remote_call_error = ValidateAdminRemoteCallAttributes(mod, nullable_lnt); !admin_remote_call_error.empty()) {
        throw ScriptException(admin_remote_call_error);
    }
    if (const auto event_error = ValidateEventSubscriptions(mod, nullable_lnt); !event_error.empty()) {
        throw ScriptException(event_error);
    }
    if (const auto remote_call_error = ValidateAngelScriptRemoteCallAttributes(mod, *_meta, nullable_lnt); !remote_call_error.empty()) {
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

    auto preprocessor_context = make_unique_del_ptr(ptr<Preprocessor::Context>(Preprocessor::CreateContext()), [](ptr<Preprocessor::Context> ctx) FO_DEFERRED { Preprocessor::DeleteContext(ctx.get()); });

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

    nptr<Preprocessor::LineNumberTranslator> nullable_lnt = Preprocessor::GetLineNumberTranslator(preprocessor_context.get());
    FO_VERIFY_AND_THROW(!!nullable_lnt, "Missing line-number translator");
    auto lnt = nullable_lnt.as_ptr();
    CleanupLineNumberTranslator(_asEngine.get());
    _asEngine->SetUserData(cast_to_void(lnt.get()), AS_PREPROCESSOR_LNT_USER_DATA);

    nptr<AngelScript::asIScriptModule> nullable_mod = _asEngine->GetModule("Root", AngelScript::asGM_ALWAYS_CREATE);

    if (!nullable_mod) {
        throw ScriptCompilerException("Create root module failed");
    }

    auto mod = nullable_mod.as_ptr();
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
    if (const auto usage_error = ValidateAttributedFunctionUsage(mod, nullable_lnt, &_settings->AttributedFunctionDirectCallAllowedNamespaces, &_settings->ExtraDirectCallBlockingAttributes); !usage_error.empty()) {
        throw ScriptCompilerException("Attributed function usage validation failed", usage_error);
    }
    if (const auto special_attr_error = ValidateSpecialFunctionAttributes(mod, nullable_lnt); !special_attr_error.empty()) {
        throw ScriptCompilerException("Special function attribute validation failed", special_attr_error);
    }
    if (const auto admin_remote_call_error = ValidateAdminRemoteCallAttributes(mod, nullable_lnt); !admin_remote_call_error.empty()) {
        throw ScriptCompilerException("Admin remote call attribute validation failed", admin_remote_call_error);
    }
    if (const auto event_error = ValidateEventSubscriptions(mod, nullable_lnt); !event_error.empty()) {
        throw ScriptCompilerException("Callback attribute validation failed", event_error);
    }
    if (const auto remote_call_error = ValidateAngelScriptRemoteCallAttributes(mod, *_meta, nullable_lnt); !remote_call_error.empty()) {
        throw ScriptCompilerException("Remote call attribute validation failed", remote_call_error);
    }

    vector<AngelScript::asBYTE> buf;
    BinaryStream binary {&buf};
    as_result = mod->SaveByteCode(&binary);

    if (as_result < 0) {
        throw ScriptCompilerException("Unable to save byte code", as_result);
    }

    std::vector<uint8_t> lnt_data;
    Preprocessor::StoreLineNumberTranslator(nullable_lnt.get(), lnt_data);

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

    BindAngelScriptRemoteCalls(_asEngine.as_ptr());

    if (HasEntityMngr() && _asEngine->GetModuleCount() == 1) {
        nptr<const AngelScript::asIScriptModule> nullable_mod = _asEngine->GetModuleByIndex(0);
        FO_VERIFY_AND_THROW(!!nullable_mod, "Missing compiled AngelScript module");
        auto mod = nullable_mod.as_ptr();
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
        nptr<AngelScript::asIScriptModule> nullable_mod = _asEngine->GetModuleByIndex(0);
        FO_VERIFY_AND_THROW(!!nullable_mod, "Missing compiled AngelScript module");
        auto mod = nullable_mod.as_ptr();

        for (AngelScript::asUINT i = 0; i < mod->GetFunctionCount(); i++) {
            nptr<AngelScript::asIScriptFunction> func = mod->GetFunctionByIndex(i);
            FO_VERIFY_AND_THROW(!!func, "Module function lookup returned null");
            auto func_desc = IndexScriptFunc(func.as_ptr());

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

        _contextMngr.emplace(_asEngine.as_ptr(), overrun_report_time, [this](string_view reason, string_view text, string_view source_path, std::optional<uint32_t> line, string_view function_name) {
            if (_debuggerEndpointServer) {
                auto endpoint_server = _debuggerEndpointServer.as_ptr();
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

                endpoint_server->EmitEvent("stopped", body.dump());
            }
        });

        _contextMngr->SetContextSetupCallback([this](ptr<AngelScript::asIScriptContext> ctx, AngelScriptContextSetupReason reason) {
            if (_debuggerEndpointServer) {
                auto endpoint_server = _debuggerEndpointServer.as_ptr();
                endpoint_server->SetupContext(ctx, reason);
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

void AngelScriptBackend::ReleaseScriptGlobalsAndReportGC()
{
    FO_STACK_TRACE_ENTRY();

    if (!_asEngine) {
        return;
    }

    // Drop every script-side reference held by module global variables, then run a full GC so the now-unrooted
    // object graphs are reclaimed before final shutdown. AngelScript also releases globals during module
    // discard, but doing it explicitly here (while the engine is fully alive) lets the collector collapse the
    // graphs in a normal multi-iteration cycle, so the engine cleans up after itself without per-system manual
    // cleanup in game scripts.
    size_t released_globals = 0;

    for (AngelScript::asUINT module_index = 0; module_index < _asEngine->GetModuleCount(); module_index++) {
        nptr<AngelScript::asIScriptModule> nullable_mod = _asEngine->GetModuleByIndex(module_index);

        if (!nullable_mod) {
            continue;
        }

        auto mod = nullable_mod.as_ptr();

        for (AngelScript::asUINT var_index = 0; var_index < mod->GetGlobalVarCount(); var_index++) {
            int32_t type_id = 0;

            if (mod->GetGlobalVar(var_index, nullptr, nullptr, &type_id, nullptr) < 0) {
                continue;
            }

            nptr<AngelScript::asITypeInfo> type_info = _asEngine->GetTypeInfoById(type_id);

            if (!type_info) {
                continue; // Primitive global, nothing to release
            }

            const nptr<AngelScript::asIScriptFunction> funcdef = type_info->GetFuncdefSignature();
            const bool is_funcdef = !!funcdef;
            // Only funcdef handles and reference-typed object globals store a pointer in the slot. Value-type
            // object globals keep their instance inline (as do enums/primitives), so reading the slot as a
            // pointer would over-read the inline value; those are destructed by the standard module teardown.
            const bool is_ref_object = (type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (type_info->GetFlags() & AngelScript::asOBJ_REF) != 0;

            if (!is_funcdef && !is_ref_object) {
                continue;
            }

            auto nullable_slot = GetGlobalHandleSlot(mod, var_index);

            if (!nullable_slot || !*nullable_slot) {
                continue;
            }

            auto slot = nullable_slot.as_ptr();

            // Funcdef globals (function handles / delegates) and reference-typed globals (handles, arrays,
            // dictionaries, script classes) store a pointer in the slot; release it and null the slot so the
            // standard module teardown skips it. Value-type object globals store the instance inline and are
            // destructed by the module teardown, so they are left untouched here.
            if (is_funcdef) {
                auto func = ReadGlobalFunctionHandle(slot);
                func->Release();
                WriteGlobalHandleSlot(slot, nullptr);
                released_globals++;
            }
            else if ((type_info->GetFlags() & AngelScript::asOBJ_REF) != 0) {
                _asEngine->ReleaseScriptObject(*slot, type_info.get());
                WriteGlobalHandleSlot(slot, nullptr);
                released_globals++;
            }
        }
    }

    // Collapse object graphs that were kept alive only by the released globals. A single asGC_FULL_CYCLE
    // runs the collector to completion, but destructors can release the last reference to other objects,
    // so iterate until the live count stops dropping.
    AngelScript::asUINT gc_size = 0;
    _asEngine->GetGCStatistics(&gc_size);

    for (int32_t pass = 0; pass < 8 && gc_size != 0; pass++) {
        _asEngine->GarbageCollect(AngelScript::asGC_FULL_CYCLE);

        AngelScript::asUINT new_gc_size = 0;
        _asEngine->GetGCStatistics(&new_gc_size);

        if (new_gc_size == gc_size) {
            break; // Steady state — survivors are not collectable from the script side
        }

        gc_size = new_gc_size;
    }

    // Report any GC objects that survive (kept alive by references the collector cannot reclaim, e.g. a
    // pre-existing GUI screen-graph cycle). The subsequent ShutDownAndRelease force-releases them; the
    // by-type summary here confirms the global release ran and points at the owning system.
    if (gc_size != 0) {
        unordered_map<string, size_t> survivors_by_type;

        for (AngelScript::asUINT i = 0; i < gc_size; i++) {
            nptr<AngelScript::asITypeInfo> type_info;

            if (_asEngine->GetObjectInGC(i, nullptr, nullptr, type_info.get_pp()) >= 0) {
                if (!type_info) {
                    continue;
                }

                survivors_by_type[type_info->GetName()]++;
            }
        }

        WriteLog("AngelScript shutdown: released {} global var(s), {} GC object(s) still alive:", released_globals, gc_size);

        for (const auto& [type_name, type_count] : survivors_by_type) {
            WriteLog("- {} x {}", type_count, type_name);
        }
    }
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
