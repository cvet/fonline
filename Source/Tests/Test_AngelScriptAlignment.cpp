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
#include "AngelScriptBackend.h"
#include "AngelScriptHelpers.h"
#include "AngelScriptScripting.h"
#include "Baker.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

#include <angelscript.h>
#endif

FO_BEGIN_NAMESPACE

#if FO_ANGELSCRIPT_SCRIPTING

// Regression coverage for the 8-byte value alignment of AngelScript STORAGE. Each historical
// misalignment class gets both a direct layout assertion (member byte offsets via asITypeInfo,
// checked on every build) and a runtime execution path (constructing/using the values, so the UBSan
// leg catches any regression as a hard `alignment` runtime error):
//   1. Script-class member layout — `as_objecttype.cpp AddPropertyToClass` used to pack 8-byte
//      members on 4-byte boundaries (observed on script GUI classes and Ai::Plan in gameplay).
//      Covered across primitives of every size, inline POD value types (ident/timespan/nanotime/
//      mpos/ipos/irect/ucolor/hdir), non-POD value types stored as references (string/hstring/any),
//      enums, funcdef/class/array handle members, two inheritance levels and a mixin.
//   2. 8-byte locals in a callee whose argument block is an odd DWORD count —
//      `as_context.cpp PrepareScriptFunction` aligns the frame base by relocating the arg block.
//   3. Module globals of 8-byte types (initialized in the module-init frame), array init-list
//      element storage, and the non-POD `any` value.
// Call-shape coverage (method/virtual/interface/funcdef/delegate/parameters) lives in
// Test_AngelScriptCall.cpp.
namespace
{
    struct AlignTestRig
    {
        static auto MakeSettings() -> GlobalSettings
        {
            auto settings = GlobalSettings(false);

            settings.ApplyDefaultSettings();
            settings.ApplyAutoSettings();

            BakerTests::ApplySelfContainedServerSettings(settings);

            return settings;
        }

        static auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
        {
            BakerServerEngine compiler_engine {metadata_resources};

            return BakerTests::CompileInlineScripts(&compiler_engine, "AlignTestScripts",
                {
                    {"Scripts/AlignTest.fos",
                        R"(
namespace AlignTest
{
    enum SmallEnum
    {
        SE_Zero = 0,
        SE_One = 1,
    }

    funcdef int64 UnaryOp(int8);

    // Adversarial member ordering: every 8-byte or pointer-sized member is preceded by a smaller
    // member, so the historical 4-byte packing would misalign each of them.
    class MixedMembers
    {
        int8 SmallA;
        timespan TsMember;
        int16 SmallB;
        int64 I64Member;
        bool SmallC;
        double DblMember;
        uint8 SmallD;
        ident IdMember;
        int SmallE;
        nanotime NtMember;
        int8 SmallF;
        hstring HsMember;
        int16 SmallG;
        MixedMembers HandleMember;
        int8 SmallH;
        string StrMember;
        SmallEnum EnumMember;
        uint16 SmallI;
        uint64 U64Member;
        int8 SmallJ;
        UnaryOp FnMember;
        bool SmallK;
        int64[] ArrMember;
        mpos PosMember;
        int8 SmallL;
        ipos IposMember;
        uint8 SmallM;
        irect RectMember;
        ucolor ColMember;
        hdir DirMember;
        int8 SmallN;
        any AnyMember;
        float FltMember;
    }

    // Inherited members continue after the base layout; derived 8-byte members must stay aligned
    // regardless of the base class total size.
    class DerivedMembers : MixedMembers
    {
        int8 SmallDerivedA;
        synctime StMember;
    }

    class DerivedTwice : DerivedMembers
    {
        bool SmallDerivedB;
        double DblDerived;
        int8 SmallDerivedC;
        ident IdDerived;
    }

    mixin class WideMixin
    {
        int64 MixWide;
        int8 MixSmall;
        hstring MixHs;
    }

    class WithMixin : WideMixin
    {
        int8 Tiny;
        timespan TsAfterMixin;
    }

    int64 ConstructAndUseMembers()
    {
        MixedMembers m = MixedMembers();
        m.I64Member = 40;
        m.DblMember = 1.5;
        m.TsMember = timespan(2500, 2);
        m.IdMember = ident();
        m.StrMember = "aligned";
        m.U64Member = 5;
        m.EnumMember = SmallEnum::SE_One;
        m.FltMember = 2.5;

        DerivedMembers d = DerivedMembers();
        d.I64Member = 1;
        d.SmallDerivedA = 1;

        DerivedTwice t = DerivedTwice();
        t.DblDerived = 3.5;
        t.IdDerived = ident();

        WithMixin wm = WithMixin();
        wm.MixWide = 2;
        wm.TsAfterMixin = timespan(9, 3);

        return m.I64Member + int64(m.DblMember) + int64(m.U64Member) + int64(m.EnumMember) + int64(m.FltMember) + d.I64Member + int64(d.SmallDerivedA) + int64(t.DblDerived) + wm.MixWide + wm.TsAfterMixin.seconds;
    }

    // One-DWORD argument block: without the runtime frame-base alignment the callee frame lands on
    // a 4-mod-8 boundary and every 8-byte local below is misaligned.
    int64 OddArgFrameLocals(int8 pad)
    {
        int64 wide = 40;
        double real = 1.5;
        ident id;
        timespan ts = timespan(1, 3);
        nanotime nt;

        return wide + int64(real) + pad + ts.seconds;
    }

    int64 CallOddArgFrameLocals()
    {
        return OddArgFrameLocals(1);
    }

    // Module globals of 8-byte types: initialized in the module-init frame, then read at runtime.
    const int64 g_wide = 80;
    const double g_real = 1.5;
    const timespan g_ts = timespan(6, 3);
    const ident g_id = ident();
    const nanotime g_nt = nanotime();

    int64 UseGlobals()
    {
        return g_wide + int64(g_real) + g_ts.seconds;
    }

    // Array storage of 8-byte elements filled through init lists and element access.
    int64 UseArrays()
    {
        int64[] wides = {5, 6};
        double[] reals = {1.5, 2.5};
        ident[] ids = {ident()};

        return wides[0] + wides[1] + int64(reals[0] + reals[1]) + int64(ids.length());
    }

    // Construct-only coverage for the non-POD any value.
    int64 UseAny()
    {
        any holder = any(int64(42));

        return 1;
    }
}
)"},
                },
                [](string_view message) {
                    const auto message_str = string(message);

                    if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                        throw ScriptSystemException(message_str);
                    }
                });
        }

        static auto MakeResources() -> FileSystem
        {
            const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

            auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("AlignTestCompilerResources");
            compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

            FileSystem compiler_resources;
            compiler_resources.AddCustomSource(std::move(compiler_resources_source));

            const auto script_blob = MakeScriptBinary(compiler_resources);

            auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("AlignTestRuntimeResources");
            runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
            runtime_source->AddFile("AlignTest.fos-bin-server", script_blob);

            FileSystem resources;
            resources.AddCustomSource(std::move(runtime_source));

            return resources;
        }

        static auto WaitForStart(ptr<ServerEngine> server) -> string
        {
            for (int32_t i = 0; i < 6000; i++) {
                if (server->IsStarted()) {
                    return {};
                }
                if (server->IsStartingError()) {
                    return "ServerEngine startup failed";
                }

                std::this_thread::sleep_for(std::chrono::milliseconds {10});
            }

            return "ServerEngine startup timed out";
        }

        static auto MakeServerEngine(GlobalSettings& settings) -> refcount_ptr<ServerEngine> { return SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeResources()); }
    };

    // Required byte alignment of one script-class member slot, derived from what the layout stores
    // there: handles, script-object/array members and non-POD value references are a pointer; inline
    // value types and primitives align by their size (mirrors AddPropertyToClass).
    static auto RequiredMemberAlignment(ptr<AngelScript::asIScriptEngine> as_engine, int32_t type_id, bool is_reference) -> size_t
    {
        if (is_reference || (type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
            return sizeof(void*);
        }

        if ((type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
            nptr<AngelScript::asITypeInfo> type_info = as_engine->GetTypeInfoById(type_id);
            FO_VERIFY_AND_THROW(type_info, "Member type info must resolve", type_id);

            if ((type_info->GetFlags() & AngelScript::asOBJ_REF) != 0) {
                return sizeof(void*); // Script classes / arrays / funcdefs held as implicit handles
            }

            const size_t size = type_info->GetSize();
            return size >= 8 ? 8 : size >= 4 ? 4 : size >= 2 ? 2 : 1;
        }

        const int32_t primitive_size = as_engine->GetSizeOfPrimitiveType(type_id);

        if (primitive_size == 8) {
            return 8;
        }
        if (primitive_size == 2) {
            return 2;
        }
        if (primitive_size == 1) {
            return 1;
        }

        return 4; // 4-byte primitives and enums
    }

    static void CheckClassMemberAlignment(ptr<AngelScript::asIScriptEngine> as_engine, ptr<AngelScript::asITypeInfo> type_info)
    {
        REQUIRE(type_info->GetPropertyCount() > 0);

        for (AngelScript::asUINT i = 0; i < type_info->GetPropertyCount(); i++) {
            const char* prop_name = nullptr;
            int32_t prop_type_id = 0;
            int32_t prop_offset = 0;
            bool is_reference = false;
            REQUIRE(type_info->GetProperty(i, &prop_name, &prop_type_id, nullptr, nullptr, &prop_offset, &is_reference) >= 0);

            const size_t required_alignment = RequiredMemberAlignment(as_engine, prop_type_id, is_reference);

            INFO(strex("{}::{} offset {} required alignment {}", type_info->GetName(), prop_name != nullptr ? prop_name : "?", prop_offset, required_alignment).str());
            CHECK(numeric_cast<size_t>(prop_offset) % required_alignment == 0);
        }
    }
}

TEST_CASE("AngelScriptValueAlignment")
{
    auto settings = AlignTestRig::MakeSettings();
    auto server = AlignTestRig::MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = AlignTestRig::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    // Script-class member layout: every member must sit on a correctly aligned byte offset across
    // the base class, both inheritance levels and the mixin-including class.
    {
        auto backend = GetScriptBackend(ptr<BaseEngine>(server.get()));
        auto context_mngr = backend->GetContextMngr();
        REQUIRE(context_mngr);

        auto ctx = context_mngr->RequestContext();
        const uint64_t context_generation = context_mngr->GetContextGeneration(ctx);
        auto return_context = scope_exit([&context_mngr, &ctx, &context_generation]() noexcept { context_mngr->ReturnContext(ctx, context_generation); });

        nptr<AngelScript::asIScriptEngine> as_engine = ctx->GetEngine();
        REQUIRE(as_engine != nullptr);

        // Script classes live in the script module, not in the engine's registered-type scope.
        REQUIRE(as_engine->GetModuleCount() >= 1);
        nptr<AngelScript::asIScriptModule> script_module = as_engine->GetModuleByIndex(0);
        REQUIRE(script_module != nullptr);

        for (const string_view class_decl : {string_view {"AlignTest::MixedMembers"}, string_view {"AlignTest::DerivedMembers"}, string_view {"AlignTest::DerivedTwice"}, string_view {"AlignTest::WithMixin"}}) {
            INFO(class_decl);
            nptr<AngelScript::asITypeInfo> class_type = script_module->GetTypeInfoByDecl(class_decl.data());
            REQUIRE(class_type != nullptr);
            CheckClassMemberAlignment(as_engine, class_type);
        }
    }

    // Runtime execution: under the UBSan leg any misaligned member/local/global constructor or read
    // trips a hard `alignment` runtime error in the value-type behaviours.
    const auto call_and_check = [&](string_view func_name, int64_t expected) {
        INFO(func_name);
        auto func = server->FindFunc<int64_t>(fn(func_name));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == expected);
    };

    call_and_check("AlignTest::ConstructAndUseMembers", 65);
    call_and_check("AlignTest::CallOddArgFrameLocals", 43);
    call_and_check("AlignTest::UseGlobals", 87);
    call_and_check("AlignTest::UseArrays", 16);
    call_and_check("AlignTest::UseAny", 1);
}

#endif

FO_END_NAMESPACE
