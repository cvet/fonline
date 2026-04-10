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

#include "catch_amalgamated.hpp"

#if FO_ANGELSCRIPT_SCRIPTING
#include "AngelScriptAttributes.h"
#include "AngelScriptRemoteCalls.h"
#include "Common.h"
#include "EngineBase.h"

#include <angelscript.h>
#include <preprocessor.h>
#endif

FO_BEGIN_NAMESPACE

namespace
{
#if FO_ANGELSCRIPT_SCRIPTING
    using namespace AngelScript;

    class BytecodeStream final : public asIBinaryStream
    {
    public:
        explicit BytecodeStream(vector<asBYTE>& buf) :
            _buf {buf}
        {
        }

        int Read(void* ptr, asUINT size) override
        {
            if (ptr == nullptr || size == 0) {
                return 0;
            }

            REQUIRE(_readPos + size <= _buf.size());
            MemCopy(ptr, _buf.data() + _readPos, size);
            _readPos += size;
            return 0;
        }

        int Write(const void* ptr, asUINT size) override
        {
            if (ptr == nullptr || size == 0) {
                return 0;
            }

            _buf.resize(_writePos + size);
            MemCopy(_buf.data() + _writePos, ptr, size);
            _writePos += size;
            return 0;
        }

    private:
        vector<asBYTE>& _buf;
        size_t _readPos {};
        size_t _writePos {};
    };

    class SingleScriptLoader final : public Preprocessor::FileLoader
    {
    public:
        SingleScriptLoader(string path, string content) :
            _path {std::move(path)},
            _content {std::move(content)}
        {
        }

        auto LoadFile(const std::string& dir, const std::string& file_name, std::vector<char>& data, std::string& file_path) -> bool override
        {
            ignore_unused(dir);
            ignore_unused(file_name);

            data.assign(_content.begin(), _content.end());
            file_path = _path;
            return true;
        }

    private:
        string _path;
        string _content;
    };

    struct DummyEventObject
    {
    };

    struct DummySchedulerObject
    {
    };

    struct DummyPropertyApiObject
    {
    };

    struct DummyCritterObject
    {
    };

    static DummyEventObject DummyEventInstance;
    static DummySchedulerObject DummySchedulerInstance;
    static DummyPropertyApiObject DummyPropertyApiInstance;
    static DummyCritterObject DummyCritterInstance;

    static auto GetDummyEvent() -> DummyEventObject*
    {
        return &DummyEventInstance;
    }

    static auto GetDummyScheduler() -> DummySchedulerObject*
    {
        return &DummySchedulerInstance;
    }

    static auto GetDummyPropertyApi() -> DummyPropertyApiObject*
    {
        return &DummyPropertyApiInstance;
    }

    static auto GetDummyCritter() -> DummyCritterObject*
    {
        return &DummyCritterInstance;
    }

    static void DummyEvent_Subscribe(asIScriptGeneric* gen)
    {
        ignore_unused(gen->GetObject());
        ignore_unused(gen->GetAddressOfArg(0));
        ignore_unused(gen->GetAddressOfArg(1));
    }

    static void DummyEvent_Unsubscribe(asIScriptGeneric* gen)
    {
        ignore_unused(gen->GetObject());
        ignore_unused(gen->GetAddressOfArg(0));
    }

    static void DummyRefAddRef(void* obj)
    {
        ignore_unused(obj);
    }

    static void DummyRefRelease(void* obj)
    {
        ignore_unused(obj);
    }

    static void RegisterDummyPlayerType(asIScriptEngine* engine)
    {
        REQUIRE(engine->RegisterObjectType("Player", 0, asOBJ_REF) >= 0);
        REQUIRE(engine->RegisterObjectBehaviour("Player", asBEHAVE_ADDREF, "void f()", asFUNCTION(DummyRefAddRef), asCALL_CDECL_OBJFIRST) >= 0);
        REQUIRE(engine->RegisterObjectBehaviour("Player", asBEHAVE_RELEASE, "void f()", asFUNCTION(DummyRefRelease), asCALL_CDECL_OBJFIRST) >= 0);
    }

    static void RegisterDummyCritterType(asIScriptEngine* engine)
    {
        REQUIRE(engine->RegisterObjectType("Critter", 0, asOBJ_REF) >= 0);
        REQUIRE(engine->RegisterObjectBehaviour("Critter", asBEHAVE_ADDREF, "void f()", asFUNCTION(DummyRefAddRef), asCALL_CDECL_OBJFIRST) >= 0);
        REQUIRE(engine->RegisterObjectBehaviour("Critter", asBEHAVE_RELEASE, "void f()", asFUNCTION(DummyRefRelease), asCALL_CDECL_OBJFIRST) >= 0);
    }

    static void DummyScheduler_StartTimeEvent(asIScriptGeneric* gen)
    {
        ignore_unused(gen->GetObject());
        ignore_unused(gen->GetAddressOfArg(0));
        ignore_unused(gen->GetAddressOfArg(1));
    }

    static void DummyScheduler_StartTimeEventRepeat(asIScriptGeneric* gen)
    {
        ignore_unused(gen->GetObject());
        ignore_unused(gen->GetAddressOfArg(0));
        ignore_unused(gen->GetAddressOfArg(1));
        ignore_unused(gen->GetAddressOfArg(2));
    }

    static void DummyScheduler_StopTimeEvent(asIScriptGeneric* gen)
    {
        ignore_unused(gen->GetObject());
        ignore_unused(gen->GetAddressOfArg(0));
    }

    static void DummyScheduler_CountTimeEvent(asIScriptGeneric* gen)
    {
        ignore_unused(gen->GetObject());
        ignore_unused(gen->GetAddressOfArg(0));
        gen->SetReturnDWord(0);
    }

    static void DummyScheduler_RepeatTimeEvent(asIScriptGeneric* gen)
    {
        ignore_unused(gen->GetObject());
        ignore_unused(gen->GetAddressOfArg(0));
        ignore_unused(gen->GetAddressOfArg(1));
    }

    static void DummyScheduler_SetTimeEventData(asIScriptGeneric* gen)
    {
        ignore_unused(gen->GetObject());
        ignore_unused(gen->GetAddressOfArg(0));
        ignore_unused(gen->GetAddressOfArg(1));
    }

    static void DummyProperty_SetPropertyGetter(asIScriptGeneric* gen)
    {
        ignore_unused(gen->GetObject());
        ignore_unused(gen->GetAddressOfArg(0));
        ignore_unused(gen->GetAddressOfArg(1));
    }

    static void DummyProperty_AddPropertySetter(asIScriptGeneric* gen)
    {
        ignore_unused(gen->GetObject());
        ignore_unused(gen->GetAddressOfArg(0));
        ignore_unused(gen->GetAddressOfArg(1));
    }

    static void DummyProperty_AddPropertyDeferredSetter(asIScriptGeneric* gen)
    {
        ignore_unused(gen->GetObject());
        ignore_unused(gen->GetAddressOfArg(0));
        ignore_unused(gen->GetAddressOfArg(1));
    }

    static void DummyCritter_AddAnimCallback(asIScriptGeneric* gen)
    {
        ignore_unused(gen->GetObject());
        ignore_unused(gen->GetAddressOfArg(0));
        ignore_unused(gen->GetAddressOfArg(1));
        ignore_unused(gen->GetAddressOfArg(2));
        ignore_unused(gen->GetAddressOfArg(3));
    }

    struct ScriptMessages
    {
        vector<string> Entries {};

        static void Callback(const asSMessageInfo* msg, void* param)
        {
            auto* self = static_cast<ScriptMessages*>(param);
            FO_RUNTIME_ASSERT(self != nullptr);

            self->Entries.emplace_back(strex("{}({},{}): {}", msg->section != nullptr ? msg->section : "<unknown>", msg->row, msg->col, msg->message != nullptr ? msg->message : "<no message>").str());
        }
    };

    struct ParsedScript
    {
        vector<ParsedFunctionAttributeRecord> Records {};
        string Source {};
        string Errors {};
    };

    struct PreprocessorContextDeleter
    {
        void operator()(Preprocessor::Context* ctx) const noexcept { Preprocessor::DeleteContext(ctx); }
    };

    static auto ParseScript(string_view path, string_view script, std::initializer_list<string_view> defines = {}) -> ParsedScript
    {
        auto pp_ctx = std::unique_ptr<Preprocessor::Context, PreprocessorContextDeleter> {Preprocessor::CreateContext()};
        for (const auto define : defines) {
            Preprocessor::Define(pp_ctx.get(), std::string(define));
        }
        Preprocessor::StringOutStream pp_errors;
        Preprocessor::LexemList lexems;
        auto loader = SingleScriptLoader {string(path), string(script)};
        const auto std_path = std::string {path};

        const auto pp_errors_count = Preprocessor::PreprocessToLexems(pp_ctx.get(), std_path, lexems, &pp_errors, &loader);
        REQUIRE(pp_errors_count == 0);
        REQUIRE(pp_errors.String.empty());

        ParsedScript result;
        result.Records = ParseFunctionAttributeRecords(pp_ctx.get(), lexems, result.Errors);

        Preprocessor::StringOutStream out;
        Preprocessor::PrintLexemList(pp_ctx.get(), lexems, out);
        result.Source = std::move(out.String);
        return result;
    }

    static auto MakeEngine(ScriptMessages& messages) -> asIScriptEngine*
    {
        auto* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
        REQUIRE(engine != nullptr);

        REQUIRE(engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, false) >= 0);
        REQUIRE(engine->SetMessageCallback(asFUNCTION(ScriptMessages::Callback), &messages, asCALL_CDECL) >= 0);
        return engine;
    }

    static void RegisterDummyEventApi(asIScriptEngine* engine)
    {
        REQUIRE(engine->RegisterFuncdef("void DummyEventFunc()") >= 0);
        REQUIRE(engine->RegisterObjectType("DummyEvent", 0, asOBJ_REF | asOBJ_NOCOUNT) >= 0);
        REQUIRE(engine->RegisterGlobalFunction("DummyEvent@ GetDummyEvent()", asFUNCTION(GetDummyEvent), asCALL_CDECL) >= 0);
        REQUIRE(engine->RegisterObjectMethod("DummyEvent", "void Subscribe(DummyEventFunc@+ func, int priority = 0)", asFUNCTION(DummyEvent_Subscribe), asCALL_GENERIC) >= 0);
        REQUIRE(engine->RegisterObjectMethod("DummyEvent", "void Unsubscribe(DummyEventFunc@+ func)", asFUNCTION(DummyEvent_Unsubscribe), asCALL_GENERIC) >= 0);
    }

    static void RegisterDummySchedulerApi(asIScriptEngine* engine)
    {
        REQUIRE(engine->RegisterFuncdef("void TimeEventFunc()") >= 0);
        REQUIRE(engine->RegisterObjectType("DummyScheduler", 0, asOBJ_REF | asOBJ_NOCOUNT) >= 0);
        REQUIRE(engine->RegisterGlobalFunction("DummyScheduler@ GetDummyScheduler()", asFUNCTION(GetDummyScheduler), asCALL_CDECL) >= 0);
        REQUIRE(engine->RegisterObjectMethod("DummyScheduler", "void StartTimeEvent(int delay, TimeEventFunc@+ func)", asFUNCTION(DummyScheduler_StartTimeEvent), asCALL_GENERIC) >= 0);
        REQUIRE(engine->RegisterObjectMethod("DummyScheduler", "void StartTimeEvent(int delay, int repeat, TimeEventFunc@+ func)", asFUNCTION(DummyScheduler_StartTimeEventRepeat), asCALL_GENERIC) >= 0);
        REQUIRE(engine->RegisterObjectMethod("DummyScheduler", "void StopTimeEvent(TimeEventFunc@+ func)", asFUNCTION(DummyScheduler_StopTimeEvent), asCALL_GENERIC) >= 0);
        REQUIRE(engine->RegisterObjectMethod("DummyScheduler", "int CountTimeEvent(TimeEventFunc@+ func)", asFUNCTION(DummyScheduler_CountTimeEvent), asCALL_GENERIC) >= 0);
        REQUIRE(engine->RegisterObjectMethod("DummyScheduler", "void RepeatTimeEvent(TimeEventFunc@+ func, int repeat)", asFUNCTION(DummyScheduler_RepeatTimeEvent), asCALL_GENERIC) >= 0);
        REQUIRE(engine->RegisterObjectMethod("DummyScheduler", "void SetTimeEventData(TimeEventFunc@+ func, int data)", asFUNCTION(DummyScheduler_SetTimeEventData), asCALL_GENERIC) >= 0);
    }

    static void RegisterDummyPropertyApi(asIScriptEngine* engine)
    {
        REQUIRE(engine->RegisterFuncdef("int PropertyGetterFunc(int entity)") >= 0);
        REQUIRE(engine->RegisterFuncdef("void PropertySetterFunc(int entity)") >= 0);
        REQUIRE(engine->RegisterObjectType("DummyPropertyApi", 0, asOBJ_REF | asOBJ_NOCOUNT) >= 0);
        REQUIRE(engine->RegisterGlobalFunction("DummyPropertyApi@ GetDummyPropertyApi()", asFUNCTION(GetDummyPropertyApi), asCALL_CDECL) >= 0);
        REQUIRE(engine->RegisterObjectMethod("DummyPropertyApi", "void SetPropertyGetter(int prop, PropertyGetterFunc@+ func)", asFUNCTION(DummyProperty_SetPropertyGetter), asCALL_GENERIC) >= 0);
        REQUIRE(engine->RegisterObjectMethod("DummyPropertyApi", "void AddPropertySetter(int prop, PropertySetterFunc@+ func)", asFUNCTION(DummyProperty_AddPropertySetter), asCALL_GENERIC) >= 0);
        REQUIRE(engine->RegisterObjectMethod("DummyPropertyApi", "void AddPropertyDeferredSetter(int prop, PropertySetterFunc@+ func)", asFUNCTION(DummyProperty_AddPropertyDeferredSetter), asCALL_GENERIC) >= 0);
    }

    static void RegisterDummyAnimCallbackApi(asIScriptEngine* engine)
    {
        RegisterDummyCritterType(engine);
        REQUIRE(engine->RegisterFuncdef("void AnimCallbackFunc(Critter@+ cr)") >= 0);
        REQUIRE(engine->RegisterGlobalFunction("Critter@ GetDummyCritter()", asFUNCTION(GetDummyCritter), asCALL_CDECL) >= 0);
        REQUIRE(engine->RegisterObjectMethod("Critter", "void AddAnimCallback(int stateAnim, int actionAnim, float normalizedTime, AnimCallbackFunc@+ func)", asFUNCTION(DummyCritter_AddAnimCallback), asCALL_GENERIC) >= 0);
    }

    static auto BuildModule(asIScriptEngine* engine, string_view module_name, string_view source, ScriptMessages& messages) -> asIScriptModule*
    {
        auto* mod = engine->GetModule(string(module_name).c_str(), asGM_ALWAYS_CREATE);
        REQUIRE(mod != nullptr);
        REQUIRE(mod->AddScriptSection(string(module_name).c_str(), source.data(), source.length()) >= 0);
        REQUIRE(mod->Build() >= 0);
        if (!messages.Entries.empty()) {
            INFO(messages.Entries.front());
        }
        CHECK(messages.Entries.empty());
        return mod;
    }

    static auto FindScriptFunction(asIScriptModule* mod, string_view decl) -> asIScriptFunction*
    {
        for (asUINT i = 0; i < mod->GetFunctionCount(); i++) {
            auto* func = mod->GetFunctionByIndex(i);
            if ((func->GetFuncType() == asFUNC_SCRIPT || func->GetFuncType() == asFUNC_VIRTUAL) && string_view(func->GetDeclaration(true, true, false)) == decl) {
                return func;
            }
        }

        for (asUINT i = 0; i < mod->GetObjectTypeCount(); i++) {
            auto* object_type = mod->GetObjectTypeByIndex(i);
            if (object_type == nullptr) {
                continue;
            }

            for (asUINT j = 0; j < object_type->GetMethodCount(); j++) {
                auto* func = object_type->GetMethodByIndex(j, false);
                if ((func->GetFuncType() == asFUNC_SCRIPT || func->GetFuncType() == asFUNC_VIRTUAL) && string_view(func->GetDeclaration(true, true, false)) == decl) {
                    return func;
                }
            }
        }

        return nullptr;
    }

    static void CheckAttributes(const asIScriptFunction* func, std::initializer_list<string_view> expected)
    {
        REQUIRE(func != nullptr);

        const auto* user_data = GetFunctionAttributesUserData(func);
        if (expected.size() == 0) {
            CHECK(user_data == nullptr);
            return;
        }

        REQUIRE(user_data != nullptr);
        REQUIRE(user_data->Attributes.size() == expected.size());

        size_t index = 0;
        for (const auto attr : expected) {
            CHECK(user_data->Attributes[index] == attr);
            index++;
        }
    }

    static auto MakeSimpleRemoteCallArg(const BaseTypeDesc& type, string_view name = "value") -> ArgDesc
    {
        return ArgDesc {.Name = string(name), .Type = ComplexTypeDesc {.Kind = ComplexTypeKind::Simple, .BaseType = type}};
    }

    static auto MakeInboundRemoteCall(EngineMetadata& meta, string_view name, string_view subsystem_hint, std::initializer_list<ArgDesc> args) -> RemoteCallDesc
    {
        RemoteCallDesc call {};
        call.Name = meta.Hashes.ToHashedString(name);
        call.SubsystemHint = string(subsystem_hint);
        call.Args.assign(args.begin(), args.end());
        return call;
    }

    static constexpr string_view PositiveScript = R"(
namespace AttrTest
{
[[Primary]]
void Foo()
{
}

[[One]][[Two]]
int Multi(int x)
{
    return x;
}

[[NoArgs]]
void Overload()
{
}

[[WithInt]]
void Overload(int x)
{
}

void Plain()
{
}
}
)";

    static constexpr string_view AfterClassScript = R"(
namespace AttrTest
{
class Holder
{
    int Value;
}

[[AfterClass]]
void After()
{
}
}
)";

    static constexpr string_view AfterClassWithFunctionScript = R"(
namespace Footsteps
{
class Footstep
{
    int Value;
}

void InitFootsteps()
{
}

[[Event]]
void OnCritterIn()
{
}
}
)";

    static constexpr string_view CrossNamespaceAfterClassScript = R"(
namespace CritterState
{
class KnockoutTracer
{
    void Exec()
    {
    }
}

void ToKnockout()
{
}
}

namespace Deterioration
{
[[Event]]
void OnCritterPickItem()
{
}
}
)";

    static constexpr string_view ClientFootstepsScript = R"(
namespace Footsteps
{
#if CLIENT
class Footstep
{
    int Value;
}

void InitFootstepsClient()
{
}

[[Event]]
void OnCritterIn()
{
}
#endif
}
)";

    static constexpr string_view ServerDeteriorationScript = R"(
namespace CritterState
{
#if SERVER
class KnockoutTracer
{
    void Exec()
    {
    }
}

void ToKnockout()
{
}
#endif
}

namespace Deterioration
{
#if SERVER
[[Event]]
void OnCritterPickItem()
{
}
#endif
}
)";

    static constexpr string_view InvalidPlacementScript = R"(
namespace AttrTest
{
[[Invalid]]
int Value = 10;
}
)";

    static constexpr string_view DirectCallScript = R"(
namespace AttrTest
{
[[EngineOnly]]
void Hidden()
{
}

void User()
{
    Hidden();
}
}
)";

    static constexpr string_view FuncPtrScript = R"(
namespace AttrTest
{
funcdef void HiddenFunc();

[[EngineOnly]]
void Hidden()
{
}

void UsePtr()
{
    HiddenFunc@ fn = @Hidden;
}
}
)";

    static constexpr string_view AttributeParametersScript = R"(
namespace AttrTest
{
[[Primary(2)]][[Secondary]]
void WithPriority()
{
}
}
)";

    static constexpr string_view InvalidModuleInitAttributeScript = R"(
namespace AttrTest
{
[[ModuleInit(bad)]]
void InitLater()
{
}
}
)";

    static constexpr string_view EventPositiveScript = R"(
[[Event]]
void OnEvent()
{
}

    int GetPriority()
    {
        return 1;
    }

void RegisterDirect()
{
    GetDummyEvent().Subscribe(OnEvent);
}

void RegisterWithPriority()
{
    GetDummyEvent().Subscribe(OnEvent, 1);
}

void RegisterWithPriorityFactory()
{
    GetDummyEvent().Subscribe(OnEvent, GetPriority());
}

void Unregister()
{
    GetDummyEvent().Unsubscribe(OnEvent);
}
)";

    static constexpr string_view EventNegativeScript = R"(
void OnEvent()
{
}

void RegisterDirect()
{
    GetDummyEvent().Subscribe(OnEvent);
}

void UnregisterDirect()
{
    GetDummyEvent().Unsubscribe(OnEvent);
}
)";

    static constexpr string_view EventMethodPositiveScript = R"(
class Listener
{
    [[Event]]
    void OnEvent()
    {
    }

    void RegisterDirect()
    {
        GetDummyEvent().Subscribe(OnEvent);
    }

    void RegisterWithPriority()
    {
        GetDummyEvent().Subscribe(OnEvent, 1);
    }
}
)";

    static constexpr string_view EventMethodNegativeScript = R"(
class Listener
{
    void OnEvent()
    {
    }

    void RegisterDirect()
    {
        GetDummyEvent().Subscribe(OnEvent);
    }

    void UnregisterDirect()
    {
        GetDummyEvent().Unsubscribe(OnEvent);
    }
}
)";

    static constexpr string_view EventDirectCallScript = R"(
[[Event]]
void OnEvent()
{
}

void CallDirect()
{
    OnEvent();
}
)";

    static constexpr string_view EventWrongUsageScript = R"(
[[Event]]
void OnEvent()
{
}

void StoreHandler()
{
    DummyEventFunc@ fn = @OnEvent;
}
)";

    static constexpr string_view TimeEventPositiveScript = R"(
[[TimeEvent]]
void OnTick()
{
}

    int GetDelay()
    {
        return 1;
    }

    int GetRepeat()
    {
        return 2;
    }

    int GetData()
    {
        return 42;
    }

void RegisterSingle()
{
    GetDummyScheduler().StartTimeEvent(1, OnTick);
}

void RegisterRepeat()
{
    GetDummyScheduler().StartTimeEvent(1, 2, OnTick);
}

void RegisterSingleWithHelper()
{
    GetDummyScheduler().StartTimeEvent(GetDelay(), OnTick);
}

void RegisterRepeatWithHelpers()
{
    GetDummyScheduler().StartTimeEvent(GetDelay(), GetRepeat(), OnTick);
}

int CountScheduled()
{
    return GetDummyScheduler().CountTimeEvent(OnTick);
}

void StopScheduled()
{
    GetDummyScheduler().StopTimeEvent(OnTick);
}

void RepeatScheduled()
{
    GetDummyScheduler().RepeatTimeEvent(OnTick, GetRepeat());
}

void SetData()
{
    GetDummyScheduler().SetTimeEventData(OnTick, GetData());
}
)";

    static constexpr string_view TimeEventNegativeScript = R"(
void OnTick()
{
}

void RegisterSingle()
{
    GetDummyScheduler().StartTimeEvent(1, OnTick);
}
)";

    static constexpr string_view TimeEventDirectCallScript = R"(
[[TimeEvent]]
void OnTick()
{
}

void CallDirect()
{
    OnTick();
}
)";

    static constexpr string_view TimeEventWrongUsageScript = R"(
[[TimeEvent]]
void OnTick()
{
}

void StoreHandler()
{
    TimeEventFunc@ fn = @OnTick;
}
)";

    static constexpr string_view AnimCallbackPositiveScript = R"(
[[AnimCallback]]
void OnAnim(Critter@+ cr)
{
}

void RegisterAnim()
{
    GetDummyCritter().AddAnimCallback(0, 1, 0.5f, OnAnim);
}
)";

    static constexpr string_view AnimCallbackNegativeScript = R"(
void OnAnim(Critter@+ cr)
{
}

void RegisterAnim()
{
    GetDummyCritter().AddAnimCallback(0, 1, 0.5f, OnAnim);
}
)";

    static constexpr string_view AnimCallbackDirectCallScript = R"(
[[AnimCallback]]
void OnAnim(Critter@+ cr)
{
}

void CallDirect()
{
    OnAnim(GetDummyCritter());
}
)";

    static constexpr string_view AnimCallbackWrongUsageScript = R"(
[[AnimCallback]]
void OnAnim(Critter@+ cr)
{
}

void StoreHandler()
{
    AnimCallbackFunc@ fn = @OnAnim;
}
)";

    static constexpr string_view EventUsedForTimeEventScript = R"(
[[Event]]
void OnEvent()
{
}

void RegisterTimer()
{
    GetDummyScheduler().StartTimeEvent(1, OnEvent);
}
)";

    static constexpr string_view TimeEventUsedForEventScript = R"(
[[TimeEvent]]
void OnTick()
{
}

void RegisterEvent()
{
    GetDummyEvent().Subscribe(OnTick);
}
)";

    static constexpr string_view PropertyGetterPositiveScript = R"(
[[PropertyGetter]]
int OnGet(int entity)
{
    return entity;
}

void RegisterGetter()
{
    GetDummyPropertyApi().SetPropertyGetter(1, OnGet);
}
)";

    static constexpr string_view PropertyGetterNegativeScript = R"(
int OnGet(int entity)
{
    return entity;
}

void RegisterGetter()
{
    GetDummyPropertyApi().SetPropertyGetter(1, OnGet);
}
)";

    static constexpr string_view PropertyGetterDirectCallScript = R"(
[[PropertyGetter]]
int OnGet(int entity)
{
    return entity;
}

int CallDirect()
{
    return OnGet(1);
}
)";

    static constexpr string_view PropertyGetterWrongUsageScript = R"(
[[PropertyGetter]]
int OnGet(int entity)
{
    return entity;
}

void StoreGetter()
{
    PropertyGetterFunc@ fn = @OnGet;
}
)";

    static constexpr string_view PropertySetterPositiveScript = R"(
[[PropertySetter]]
void OnSet(int entity)
{
}

void RegisterSetter()
{
    GetDummyPropertyApi().AddPropertySetter(1, OnSet);
}

void RegisterDeferredSetter()
{
    GetDummyPropertyApi().AddPropertyDeferredSetter(1, OnSet);
}
)";

    static constexpr string_view PropertySetterNegativeScript = R"(
void OnSet(int entity)
{
}

void RegisterSetter()
{
    GetDummyPropertyApi().AddPropertySetter(1, OnSet);
}
)";

    static constexpr string_view PropertySetterDirectCallScript = R"(
[[PropertySetter]]
void OnSet(int entity)
{
}

void CallDirect()
{
    OnSet(1);
}
)";

    static constexpr string_view PropertySetterWrongUsageScript = R"(
[[PropertySetter]]
void OnSet(int entity)
{
}

void StoreSetter()
{
    PropertySetterFunc@ fn = @OnSet;
}
)";

    static constexpr string_view AdminRemoteCallPositiveScript = R"(
    [[AdminRemoteCall]]
    void Run()
    {
    }

    [[AdminRemoteCall]]
    void RunForPlayer(Player@+ player, int arg0, int arg1, int arg2)
    {
    }

    [[AdminRemoteCall]]
    void RunForCritter(Critter@+ cr)
    {
    }
    )";

    static constexpr string_view AdminRemoteCallDirectCallScript = R"(
    [[AdminRemoteCall]]
    void Run()
    {
    }

    void CallDirect()
    {
        Run();
    }
    )";

    static constexpr string_view AdminRemoteCallWrongSignatureScript = R"(
    [[AdminRemoteCall]]
    void Run(Player@+ player, int arg0, uint8 arg1)
    {
    }
    )";

    static constexpr string_view ServerRemoteCallPositiveScript = R"(
namespace RemoteCallTest
{
[[ServerRemoteCall]]
void Activate(Player@+ player, int value)
{
}
}
)";

    static constexpr string_view ClientRemoteCallPositiveScript = R"(
namespace RemoteCallTest
{
[[ClientRemoteCall]]
void Activate(int value)
{
}
}
)";

    static constexpr string_view ServerRemoteCallWrongSignatureScript = R"(
namespace RemoteCallTest
{
[[ServerRemoteCall]]
void Activate(int value)
{
}
}
)";

    static constexpr string_view ServerRemoteCallWrongSideScript = R"(
namespace RemoteCallTest
{
[[ClientRemoteCall]]
void Activate(Player@+ player, int value)
{
}
}
)";

    static constexpr string_view ClientRemoteCallExtraScript = R"(
namespace RemoteCallTest
{
[[ClientRemoteCall]]
void Extra(int value)
{
}
}
)";

    static constexpr string_view ServerRemoteCallMissingAttributeScript = R"(
namespace RemoteCallTest
{
void Activate(Player@+ player, int value)
{
}
}
)";

    static constexpr string_view LegacyServerRemoteCallCommentScript = R"(
namespace RemoteCallTest
{
// [[ServerRemoteCall]]
void Activate(Player@+ player, int value)
{
}
}
)";
#endif
}

TEST_CASE("AngelScriptAttributes", "[angelscript][attributes]")
{
#if FO_ANGELSCRIPT_SCRIPTING
    SECTION("BindsAndStripsAttributes")
    {
        auto parsed = ParseScript("AttributesPositive.fos", PositiveScript);

        CHECK(parsed.Errors.empty());
        REQUIRE(parsed.Records.size() == 4);
        CHECK(parsed.Source.find("[Primary]") == string::npos);
        CHECK(parsed.Source.find("[One]") == string::npos);
        CHECK(parsed.Source.find("[Two]") == string::npos);

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "AttrTestModule", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());
        CHECK(ValidateAttributedFunctionUsage(mod).empty());

        CheckAttributes(FindScriptFunction(mod, "void AttrTest::Foo()"), {"Primary"});
        CheckAttributes(FindScriptFunction(mod, "int AttrTest::Multi(int)"), {"One", "Two"});
        CheckAttributes(FindScriptFunction(mod, "void AttrTest::Overload()"), {"NoArgs"});
        CheckAttributes(FindScriptFunction(mod, "void AttrTest::Overload(int)"), {"WithInt"});
        CheckAttributes(FindScriptFunction(mod, "void AttrTest::Plain()"), {});
    }

    SECTION("BindsAttributesAfterClosedClassScope")
    {
        auto parsed = ParseScript("AttributesAfterClass.fos", AfterClassScript);

        CHECK(parsed.Errors.empty());
        REQUIRE(parsed.Records.size() == 1);

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "AttrAfterClass", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        CheckAttributes(FindScriptFunction(mod, "void AttrTest::After()"), {"AfterClass"});
    }

    SECTION("BindsAttributesWithParameters")
    {
        auto parsed = ParseScript("AttributesWithParameters.fos", AttributeParametersScript);

        CHECK(parsed.Errors.empty());
        REQUIRE(parsed.Records.size() == 1);

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "AttrWithParameters", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        auto* func = FindScriptFunction(mod, "void AttrTest::WithPriority()");
        CheckAttributes(func, {"Primary(2)", "Secondary"});
        CHECK(HasFunctionAttribute(func, "Primary"));
        CHECK(FindFunctionAttribute(func, "Primary") == "Primary(2)");
    }

    SECTION("KeepsTopLevelAttributesOutsideClosedClass")
    {
        auto parsed = ParseScript("AttributesAfterClassWithFunction.fos", AfterClassWithFunctionScript);

        CHECK(parsed.Errors.empty());
        REQUIRE(parsed.Records.size() == 1);
        CHECK(parsed.Records.front().Namespace == "Footsteps");
        CHECK(parsed.Records.front().ObjectType.empty());
        CHECK(parsed.Records.front().Name == "OnCritterIn");
    }

    SECTION("DoesNotLeakTypeScopeIntoNextNamespace")
    {
        auto parsed = ParseScript("AttributesCrossNamespace.fos", CrossNamespaceAfterClassScript);

        CHECK(parsed.Errors.empty());
        REQUIRE(parsed.Records.size() == 1);
        CHECK(parsed.Records.front().Namespace == "Deterioration");
        CHECK(parsed.Records.front().ObjectType.empty());
        CHECK(parsed.Records.front().Name == "OnCritterPickItem");
    }

    SECTION("KeepsTopLevelAttributesOutsideClosedClassWithClientDefine")
    {
        auto parsed = ParseScript("AttributesClientFootsteps.fos", ClientFootstepsScript, {"CLIENT 1", "SERVER 0", "MAPPER 0"});

        CHECK(parsed.Errors.empty());
        REQUIRE(parsed.Records.size() == 1);
        CHECK(parsed.Records.front().Namespace == "Footsteps");
        CHECK(parsed.Records.front().ObjectType.empty());
        CHECK(parsed.Records.front().Name == "OnCritterIn");
    }

    SECTION("DoesNotLeakTypeScopeAcrossServerOnlyNamespaces")
    {
        auto parsed = ParseScript("AttributesServerDeterioration.fos", ServerDeteriorationScript, {"CLIENT 0", "SERVER 1", "MAPPER 0"});

        CHECK(parsed.Errors.empty());
        REQUIRE(parsed.Records.size() == 1);
        CHECK(parsed.Records.front().Namespace == "Deterioration");
        CHECK(parsed.Records.front().ObjectType.empty());
        CHECK(parsed.Records.front().Name == "OnCritterPickItem");
    }

    SECTION("BytecodeRoundtripPreservesAttributes")
    {
        auto parsed = ParseScript("AttributesRoundtrip.fos", PositiveScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "AttrRoundtripSave", parsed.Source, messages);

        vector<asBYTE> bytecode;
        auto stream = BytecodeStream {bytecode};
        REQUIRE(mod->SaveByteCode(&stream) >= 0);

        vector<uint8> payload;
        auto writer = DataWriter {payload};
        SerializeFunctionAttributeRecords(writer, parsed.Records);

        ScriptMessages load_messages;
        auto* load_engine = MakeEngine(load_messages);
        auto release_load_engine = scope_exit([&load_engine]() noexcept { safe_call([&load_engine] { load_engine->ShutDownAndRelease(); }); });

        auto* load_mod = load_engine->GetModule("AttrRoundtripLoad", asGM_ALWAYS_CREATE);
        REQUIRE(load_mod != nullptr);

        auto load_stream = BytecodeStream {bytecode};
        REQUIRE(load_mod->LoadByteCode(&load_stream) >= 0);

        auto reader = DataReader {payload};
        const auto records = DeserializeFunctionAttributeRecords(reader);
        reader.VerifyEnd();

        const auto bind_error = BindFunctionAttributeRecords(load_mod, records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());
        CHECK(ValidateAttributedFunctionUsage(load_mod).empty());

        CheckAttributes(FindScriptFunction(load_mod, "void AttrTest::Foo()"), {"Primary"});
        CheckAttributes(FindScriptFunction(load_mod, "int AttrTest::Multi(int)"), {"One", "Two"});
        CheckAttributes(FindScriptFunction(load_mod, "void AttrTest::Overload()"), {"NoArgs"});
        CheckAttributes(FindScriptFunction(load_mod, "void AttrTest::Overload(int)"), {"WithInt"});
        CheckAttributes(FindScriptFunction(load_mod, "void AttrTest::Plain()"), {});
    }

    SECTION("RejectsInvalidPlacement")
    {
        auto parsed = ParseScript("AttributesInvalidPlacement.fos", InvalidPlacementScript);

        CHECK(parsed.Records.empty());
        CHECK(!parsed.Errors.empty());
        CHECK(parsed.Errors.find("Function attribute must be placed directly before a function declaration") != string::npos);
        CHECK(parsed.Errors.find("AttributesInvalidPlacement.fos") != string::npos);
    }

    SECTION("RejectsInvalidModuleInitPriorityArgument")
    {
        auto parsed = ParseScript("AttributesInvalidModuleInitPriority.fos", InvalidModuleInitAttributeScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "AttrInvalidModuleInitPriority", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto special_attr_error = ValidateSpecialFunctionAttributes(mod);
        CHECK(!special_attr_error.empty());
        CHECK(special_attr_error.find("[[ModuleInit(bad)]]") != string::npos);
        CHECK(special_attr_error.find("optional single integer priority") != string::npos);
    }

    SECTION("RejectsDirectCallsToAttributedFunctions")
    {
        auto parsed = ParseScript("AttributesDirectCall.fos", DirectCallScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "AttrDirectCall", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto usage_error = ValidateAttributedFunctionUsage(mod);
        CHECK(!usage_error.empty());
        CHECK(usage_error.find("void AttrTest::Hidden()") != string::npos);
        CHECK(usage_error.find("void AttrTest::User()") != string::npos);
        CHECK(usage_error.find("cannot be called") != string::npos);
    }

    SECTION("AllowsFunctionPointersToAttributedFunctions")
    {
        auto parsed = ParseScript("AttributesFuncPtr.fos", FuncPtrScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "AttrFuncPtr", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto usage_error = ValidateAttributedFunctionUsage(mod);
        CHECK(usage_error.empty());
    }

    SECTION("AllowsSubscribeForEventAttributedFunctions")
    {
        auto parsed = ParseScript("AttributesEventPositive.fos", EventPositiveScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyEventApi(engine);

        auto* mod = BuildModule(engine, "AttrEventPositive", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(event_error.empty());
    }

    SECTION("RejectsSubscribeForNonEventFunctions")
    {
        auto parsed = ParseScript("AttributesEventNegative.fos", EventNegativeScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyEventApi(engine);

        auto* mod = BuildModule(engine, "AttrEventNegative", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions passed to Subscribe/Unsubscribe must be marked [[Event]]") != string::npos);
        CHECK(event_error.find("void OnEvent()") != string::npos);
    }

    SECTION("AllowsImplicitDelegateSubscribeForEventAttributedClassMethods")
    {
        auto parsed = ParseScript("AttributesEventMethodPositive.fos", EventMethodPositiveScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyEventApi(engine);

        auto* mod = BuildModule(engine, "AttrEventMethodPositive", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        CheckAttributes(FindScriptFunction(mod, "void Listener::OnEvent()"), {"Event"});

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(event_error.empty());
    }

    SECTION("RejectsImplicitDelegateSubscribeForNonEventClassMethods")
    {
        auto parsed = ParseScript("AttributesEventMethodNegative.fos", EventMethodNegativeScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyEventApi(engine);

        auto* mod = BuildModule(engine, "AttrEventMethodNegative", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions passed to Subscribe/Unsubscribe must be marked [[Event]]") != string::npos);
        CHECK(event_error.find("void Listener::OnEvent()") != string::npos);
    }

    SECTION("RejectsDirectCallsToEventFunctions")
    {
        auto parsed = ParseScript("AttributesEventDirectCall.fos", EventDirectCallScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "AttrEventDirectCall", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto usage_error = ValidateAttributedFunctionUsage(mod);
        CHECK(!usage_error.empty());
        CHECK(usage_error.find("void OnEvent()") != string::npos);
        CHECK(usage_error.find("void CallDirect()") != string::npos);
        CHECK(usage_error.find("cannot be called") != string::npos);
    }

    SECTION("RejectsNonSubscribeUsesOfEventFunctions")
    {
        auto parsed = ParseScript("AttributesEventWrongUsage.fos", EventWrongUsageScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyEventApi(engine);

        auto* mod = BuildModule(engine, "AttrEventWrongUsage", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions marked [[Event]] can only be passed to Subscribe/Unsubscribe") != string::npos);
        CHECK(event_error.find("void OnEvent()") != string::npos);
    }

    SECTION("AllowsTimeEventApisForTimeEventFunctions")
    {
        auto parsed = ParseScript("AttributesTimeEventPositive.fos", TimeEventPositiveScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummySchedulerApi(engine);

        auto* mod = BuildModule(engine, "AttrTimeEventPositive", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        CheckAttributes(FindScriptFunction(mod, "void OnTick()"), {"TimeEvent"});

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(event_error.empty());
    }

    SECTION("RejectsNonTimeEventFunctionsForTimeEventApis")
    {
        auto parsed = ParseScript("AttributesTimeEventNegative.fos", TimeEventNegativeScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummySchedulerApi(engine);

        auto* mod = BuildModule(engine, "AttrTimeEventNegative", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions passed to time-event APIs (StartTimeEvent, StopTimeEvent, CountTimeEvent, RepeatTimeEvent, SetTimeEventData) must be marked [[TimeEvent]]") != string::npos);
        CHECK(event_error.find("void OnTick()") != string::npos);
    }

    SECTION("RejectsDirectCallsToTimeEventFunctions")
    {
        auto parsed = ParseScript("AttributesTimeEventDirectCall.fos", TimeEventDirectCallScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "AttrTimeEventDirectCall", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto usage_error = ValidateAttributedFunctionUsage(mod);
        CHECK(!usage_error.empty());
        CHECK(usage_error.find("void OnTick()") != string::npos);
        CHECK(usage_error.find("void CallDirect()") != string::npos);
        CHECK(usage_error.find("cannot be called") != string::npos);
    }

    SECTION("RejectsNonTimeEventApiUsesOfTimeEventFunctions")
    {
        auto parsed = ParseScript("AttributesTimeEventWrongUsage.fos", TimeEventWrongUsageScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummySchedulerApi(engine);

        auto* mod = BuildModule(engine, "AttrTimeEventWrongUsage", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions marked [[TimeEvent]] can only be passed to time-event APIs") != string::npos);
        CHECK(event_error.find("void OnTick()") != string::npos);
    }

    SECTION("RejectsUsingEventFunctionsForTimeEventApi")
    {
        auto parsed = ParseScript("AttributesEventUsedForTimeEvent.fos", EventUsedForTimeEventScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummySchedulerApi(engine);

        auto* mod = BuildModule(engine, "AttrEventUsedForTimeEvent", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions marked [[Event]] can only be passed to Subscribe/Unsubscribe") != string::npos);
        CHECK(event_error.find("void OnEvent()") != string::npos);
    }

    SECTION("RejectsUsingTimeEventFunctionsForSubscribe")
    {
        auto parsed = ParseScript("AttributesTimeEventUsedForEvent.fos", TimeEventUsedForEventScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyEventApi(engine);

        auto* mod = BuildModule(engine, "AttrTimeEventUsedForEvent", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions marked [[TimeEvent]] can only be passed to time-event APIs") != string::npos);
        CHECK(event_error.find("void OnTick()") != string::npos);
    }

    SECTION("AllowsAddAnimCallbackForAnimCallbackFunctions")
    {
        auto parsed = ParseScript("AttributesAnimCallbackPositive.fos", AnimCallbackPositiveScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyAnimCallbackApi(engine);

        auto* mod = BuildModule(engine, "AttrAnimCallbackPositive", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(event_error.empty());
    }

    SECTION("RejectsNonAnimCallbackFunctionsForAddAnimCallback")
    {
        auto parsed = ParseScript("AttributesAnimCallbackNegative.fos", AnimCallbackNegativeScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyAnimCallbackApi(engine);

        auto* mod = BuildModule(engine, "AttrAnimCallbackNegative", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions passed to AddAnimCallback must be marked [[AnimCallback]]") != string::npos);
        CHECK(event_error.find("void OnAnim(Critter@)") != string::npos);
    }

    SECTION("RejectsDirectCallsToAnimCallbackFunctions")
    {
        auto parsed = ParseScript("AttributesAnimCallbackDirectCall.fos", AnimCallbackDirectCallScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyAnimCallbackApi(engine);

        auto* mod = BuildModule(engine, "AttrAnimCallbackDirectCall", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto usage_error = ValidateAttributedFunctionUsage(mod);
        CHECK(!usage_error.empty());
        CHECK(usage_error.find("void OnAnim(Critter@)") != string::npos);
        CHECK(usage_error.find("void CallDirect()") != string::npos);
        CHECK(usage_error.find("cannot be called") != string::npos);
    }

    SECTION("RejectsNonAddAnimCallbackUsesOfAnimCallbackFunctions")
    {
        auto parsed = ParseScript("AttributesAnimCallbackWrongUsage.fos", AnimCallbackWrongUsageScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyAnimCallbackApi(engine);

        auto* mod = BuildModule(engine, "AttrAnimCallbackWrongUsage", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions marked [[AnimCallback]] can only be passed to AddAnimCallback") != string::npos);
        CHECK(event_error.find("void OnAnim(Critter@)") != string::npos);
    }

    SECTION("AllowsPropertyGetterApisForPropertyGetterFunctions")
    {
        auto parsed = ParseScript("AttributesPropertyGetterPositive.fos", PropertyGetterPositiveScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPropertyApi(engine);

        auto* mod = BuildModule(engine, "AttrPropertyGetterPositive", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        CheckAttributes(FindScriptFunction(mod, "int OnGet(int)"), {"PropertyGetter"});

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(event_error.empty());
    }

    SECTION("RejectsNonPropertyGetterFunctionsForPropertyGetterApis")
    {
        auto parsed = ParseScript("AttributesPropertyGetterNegative.fos", PropertyGetterNegativeScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPropertyApi(engine);

        auto* mod = BuildModule(engine, "AttrPropertyGetterNegative", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions passed to property getter APIs (SetPropertyGetter) must be marked [[PropertyGetter]]") != string::npos);
        CHECK(event_error.find("int OnGet(int)") != string::npos);
    }

    SECTION("RejectsDirectCallsToPropertyGetterFunctions")
    {
        auto parsed = ParseScript("AttributesPropertyGetterDirectCall.fos", PropertyGetterDirectCallScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "AttrPropertyGetterDirectCall", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto usage_error = ValidateAttributedFunctionUsage(mod);
        CHECK(!usage_error.empty());
        CHECK(usage_error.find("int OnGet(int)") != string::npos);
        CHECK(usage_error.find("int CallDirect()") != string::npos);
        CHECK(usage_error.find("cannot be called") != string::npos);
    }

    SECTION("RejectsNonPropertyGetterApiUsesOfPropertyGetterFunctions")
    {
        auto parsed = ParseScript("AttributesPropertyGetterWrongUsage.fos", PropertyGetterWrongUsageScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPropertyApi(engine);

        auto* mod = BuildModule(engine, "AttrPropertyGetterWrongUsage", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions marked [[PropertyGetter]] can only be passed to property getter APIs (SetPropertyGetter)") != string::npos);
        CHECK(event_error.find("int OnGet(int)") != string::npos);
    }

    SECTION("AllowsPropertySetterApisForPropertySetterFunctions")
    {
        auto parsed = ParseScript("AttributesPropertySetterPositive.fos", PropertySetterPositiveScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPropertyApi(engine);

        auto* mod = BuildModule(engine, "AttrPropertySetterPositive", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        CheckAttributes(FindScriptFunction(mod, "void OnSet(int)"), {"PropertySetter"});

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(event_error.empty());
    }

    SECTION("RejectsNonPropertySetterFunctionsForPropertySetterApis")
    {
        auto parsed = ParseScript("AttributesPropertySetterNegative.fos", PropertySetterNegativeScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPropertyApi(engine);

        auto* mod = BuildModule(engine, "AttrPropertySetterNegative", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions passed to property setter APIs (AddPropertySetter, AddPropertyDeferredSetter) must be marked [[PropertySetter]]") != string::npos);
        CHECK(event_error.find("void OnSet(int)") != string::npos);
    }

    SECTION("RejectsDirectCallsToPropertySetterFunctions")
    {
        auto parsed = ParseScript("AttributesPropertySetterDirectCall.fos", PropertySetterDirectCallScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "AttrPropertySetterDirectCall", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto usage_error = ValidateAttributedFunctionUsage(mod);
        CHECK(!usage_error.empty());
        CHECK(usage_error.find("void OnSet(int)") != string::npos);
        CHECK(usage_error.find("void CallDirect()") != string::npos);
        CHECK(usage_error.find("cannot be called") != string::npos);
    }

    SECTION("RejectsNonPropertySetterApiUsesOfPropertySetterFunctions")
    {
        auto parsed = ParseScript("AttributesPropertySetterWrongUsage.fos", PropertySetterWrongUsageScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPropertyApi(engine);

        auto* mod = BuildModule(engine, "AttrPropertySetterWrongUsage", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto event_error = ValidateEventSubscriptions(mod);
        CHECK(!event_error.empty());
        CHECK(event_error.find("Functions marked [[PropertySetter]] can only be passed to property setter APIs (AddPropertySetter, AddPropertyDeferredSetter)") != string::npos);
        CHECK(event_error.find("void OnSet(int)") != string::npos);
    }

    SECTION("AllowsCompatibleAdminRemoteCallFunctions")
    {
        auto parsed = ParseScript("AttributesAdminRemoteCallPositive.fos", AdminRemoteCallPositiveScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPlayerType(engine);
        RegisterDummyCritterType(engine);

        auto* mod = BuildModule(engine, "AttrAdminRemoteCallPositive", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        CheckAttributes(FindScriptFunction(mod, "void Run()"), {"AdminRemoteCall"});
        CheckAttributes(FindScriptFunction(mod, "void RunForPlayer(Player@, int, int, int)"), {"AdminRemoteCall"});
        CheckAttributes(FindScriptFunction(mod, "void RunForCritter(Critter@)"), {"AdminRemoteCall"});

        CHECK(ValidateAdminRemoteCallAttributes(mod).empty());
        CHECK(ValidateAttributedFunctionUsage(mod).empty());
    }

    SECTION("AllowsDirectCallsToAdminRemoteCallFunctions")
    {
        auto parsed = ParseScript("AttributesAdminRemoteCallDirectCall.fos", AdminRemoteCallDirectCallScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "AttrAdminRemoteCallDirectCall", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        CHECK(ValidateAdminRemoteCallAttributes(mod).empty());
        CHECK(ValidateAttributedFunctionUsage(mod).empty());
    }

    SECTION("RejectsUnsupportedAdminRemoteCallSignatures")
    {
        auto parsed = ParseScript("AttributesAdminRemoteCallWrongSignature.fos", AdminRemoteCallWrongSignatureScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPlayerType(engine);

        auto* mod = BuildModule(engine, "AttrAdminRemoteCallWrongSignature", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        const auto admin_remote_call_error = ValidateAdminRemoteCallAttributes(mod);
        CHECK(!admin_remote_call_error.empty());
        CHECK(admin_remote_call_error.find("[[AdminRemoteCall]]") != string::npos);
        CHECK(admin_remote_call_error.find("~run entrypoint") != string::npos);
        CHECK(admin_remote_call_error.find("void Run(Player@, int, uint8)") != string::npos);
    }

    SECTION("AllowsMatchingServerRemoteCallImplementations")
    {
        auto parsed = ParseScript("RemoteCallServerPositive.fos", ServerRemoteCallPositiveScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPlayerType(engine);

        auto* mod = BuildModule(engine, "RemoteCallServerPositive", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ServerSide);
        meta.RegisterInboundRemoteCall(MakeInboundRemoteCall(meta, "Activate", "RemoteCallTest.fos", {MakeSimpleRemoteCallArg(meta.GetBaseType("int32"))}));

        const auto remote_call_error = ValidateAngelScriptRemoteCallAttributes(mod, meta);
        CHECK(remote_call_error.empty());
    }

    SECTION("AllowsMatchingClientRemoteCallImplementations")
    {
        auto parsed = ParseScript("RemoteCallClientPositive.fos", ClientRemoteCallPositiveScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "RemoteCallClientPositive", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ClientSide);
        meta.RegisterInboundRemoteCall(MakeInboundRemoteCall(meta, "Activate", "RemoteCallTest.fos", {MakeSimpleRemoteCallArg(meta.GetBaseType("int32"))}));

        const auto remote_call_error = ValidateAngelScriptRemoteCallAttributes(mod, meta);
        CHECK(remote_call_error.empty());
    }

    SECTION("RejectsLegacyServerRemoteCallCommentMarkers")
    {
        auto parsed = ParseScript("LegacyRemoteCallServerComment.fos", LegacyServerRemoteCallCommentScript);
        REQUIRE(parsed.Errors.empty());
        CHECK(parsed.Records.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPlayerType(engine);

        auto* mod = BuildModule(engine, "LegacyRemoteCallServerComment", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ServerSide);
        meta.RegisterInboundRemoteCall(MakeInboundRemoteCall(meta, "Activate", "RemoteCallTest.fos", {MakeSimpleRemoteCallArg(meta.GetBaseType("int32"))}));

        auto* func = FindScriptFunction(mod, "void RemoteCallTest::Activate(Player@, int)");
        CheckAttributes(func, {});

        const auto remote_call_error = ValidateAngelScriptRemoteCallAttributes(mod, meta);
        CHECK(!remote_call_error.empty());
        CHECK(remote_call_error.find("must be marked [[ServerRemoteCall]]") != string::npos);
        CHECK(remote_call_error.find("void RemoteCallTest::Activate(Player@, int)") != string::npos);
    }

    SECTION("RejectsServerRemoteCallImplementationsWithWrongSignature")
    {
        auto parsed = ParseScript("RemoteCallServerWrongSignature.fos", ServerRemoteCallWrongSignatureScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPlayerType(engine);

        auto* mod = BuildModule(engine, "RemoteCallServerWrongSignature", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ServerSide);
        meta.RegisterInboundRemoteCall(MakeInboundRemoteCall(meta, "Activate", "RemoteCallTest.fos", {MakeSimpleRemoteCallArg(meta.GetBaseType("int32"))}));

        const auto remote_call_error = ValidateAngelScriptRemoteCallAttributes(mod, meta);
        CHECK(!remote_call_error.empty());
        CHECK(remote_call_error.find("[[ServerRemoteCall]]") != string::npos);
        CHECK(remote_call_error.find("has no matching ///@ RemoteCall declaration") != string::npos);
        CHECK(remote_call_error.find("void RemoteCallTest::Activate(int)") != string::npos);
    }

    SECTION("RejectsWrongSideRemoteCallAttributes")
    {
        auto parsed = ParseScript("RemoteCallServerWrongSide.fos", ServerRemoteCallWrongSideScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPlayerType(engine);

        auto* mod = BuildModule(engine, "RemoteCallServerWrongSide", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ServerSide);
        meta.RegisterInboundRemoteCall(MakeInboundRemoteCall(meta, "Activate", "RemoteCallTest.fos", {MakeSimpleRemoteCallArg(meta.GetBaseType("int32"))}));

        const auto remote_call_error = ValidateAngelScriptRemoteCallAttributes(mod, meta);
        CHECK(!remote_call_error.empty());
        CHECK(remote_call_error.find("[[ClientRemoteCall]]") != string::npos);
        CHECK(remote_call_error.find("uses the wrong remote-call attribute for this engine side") != string::npos);
        CHECK(remote_call_error.find("void RemoteCallTest::Activate(Player@, int)") != string::npos);
    }

    SECTION("RejectsExtraClientRemoteCallImplementations")
    {
        auto parsed = ParseScript("RemoteCallClientExtra.fos", ClientRemoteCallExtraScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });

        auto* mod = BuildModule(engine, "RemoteCallClientExtra", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ClientSide);
        meta.RegisterInboundRemoteCall(MakeInboundRemoteCall(meta, "Activate", "RemoteCallTest.fos", {MakeSimpleRemoteCallArg(meta.GetBaseType("int32"))}));

        const auto remote_call_error = ValidateAngelScriptRemoteCallAttributes(mod, meta);
        CHECK(!remote_call_error.empty());
        CHECK(remote_call_error.find("[[ClientRemoteCall]]") != string::npos);
        CHECK(remote_call_error.find("has no matching ///@ RemoteCall declaration") != string::npos);
        CHECK(remote_call_error.find("void RemoteCallTest::Extra(int)") != string::npos);
    }

    SECTION("RejectsInboundRemoteCallImplementationsMissingAttribute")
    {
        auto parsed = ParseScript("RemoteCallServerMissingAttribute.fos", ServerRemoteCallMissingAttributeScript);
        REQUIRE(parsed.Errors.empty());

        ScriptMessages messages;
        auto* engine = MakeEngine(messages);
        auto release_engine = scope_exit([&engine]() noexcept { safe_call([&engine] { engine->ShutDownAndRelease(); }); });
        RegisterDummyPlayerType(engine);

        auto* mod = BuildModule(engine, "RemoteCallServerMissingAttribute", parsed.Source, messages);
        const auto bind_error = BindFunctionAttributeRecords(mod, parsed.Records);
        INFO(bind_error);
        REQUIRE(bind_error.empty());

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ServerSide);
        meta.RegisterInboundRemoteCall(MakeInboundRemoteCall(meta, "Activate", "RemoteCallTest.fos", {MakeSimpleRemoteCallArg(meta.GetBaseType("int32"))}));

        const auto remote_call_error = ValidateAngelScriptRemoteCallAttributes(mod, meta);
        CHECK(!remote_call_error.empty());
        CHECK(remote_call_error.find("must be marked [[ServerRemoteCall]]") != string::npos);
        CHECK(remote_call_error.find("void RemoteCallTest::Activate(Player@, int)") != string::npos);
    }

#endif
}

FO_END_NAMESPACE
