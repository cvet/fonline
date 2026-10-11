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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "catch_amalgamated.hpp"

#if FO_ANGELSCRIPT_SCRIPTING
#include "AngelScriptBackend.h"
#include "AngelScriptScripting.h"
#include "PropertiesSerializer.h"
#include "Server.h"
#include "Test_BakerHelpers.h"
#endif

FO_BEGIN_NAMESPACE

#if FO_ANGELSCRIPT_SCRIPTING
namespace
{
    static auto MakeMigrationMetadata() -> vector<uint8_t>
    {
        return BakerTests::MakeMetadataBlob({
            {"RefType", {{"MigrationRecord", "Identity", "hstring", "0", "Kind", "int32", "0"}}},
            {"Property",
                {
                    {"Game", "Server", "hstring", "MigrationFirst", "Mutable", "Persistent"},
                    {"Game", "Server", "hstring", "MigrationSecond", "Mutable", "Persistent"},
                    {"Game", "Server", "string", "MigrationNote", "Mutable", "Persistent"},
                    {"Game", "Server", "string", "MigrationKeep", "Mutable", "Persistent"},
                    {"Game", "Server", "int32", "MigrationSelector", "Mutable", "Persistent"},
                    {"Game", "Server", "any", "MigrationVariant", "Mutable", "Persistent"},
                    {"Game", "Server", "hstring[]", "MigrationTokens", "Mutable", "Persistent"},
                    {"Game", "Server", "hstring=>int32", "MigrationCounts", "Mutable", "Persistent"},
                    {"Game", "Server", "MigrationRecord", "MigrationSnapshot", "Mutable", "Persistent"},
                    {"Game", "Server", "MigrationRecord[]", "MigrationRecords", "Mutable", "Persistent"},
                    {"Item", "Server", "int32", "MigrationKind", "Mutable", "Persistent"},
                    {"Item", "Server", "hstring", "MigrationIdentity", "Mutable", "Persistent"},
                }},
            {"MigrationRule",
                {
                    {"Property", "Game", "Rename", "MigrationOldFirst", "MigrationFirst"},
                    {"Property", "Game", "Remove", "MigrationRetired"},
                    {"Property", "Game", "Transform", "MigrationFirst", "DocMigrations::First"},
                    {"Property", "Game", "Transform", "MigrationSecond", "DocMigrations::Second"},
                    {"Property", "Game", "Transform", "MigrationNote", "DocMigrations::Note"},
                    {"Property", "Game", "Transform", "MigrationKeep", "DocMigrations::Keep"},
                    {"Property", "Game", "Transform", "MigrationVariant", "DocMigrations::Variant"},
                    {"Property", "Game", "Transform", "MigrationTokens", "DocMigrations::Tokens"},
                    {"Property", "Game", "Transform", "MigrationCounts", "DocMigrations::Counts"},
                    {"Property", "Game", "Transform", "MigrationSnapshot", "DocMigrations::Snapshot"},
                    {"Property", "Game", "Transform", "MigrationRecords", "DocMigrations::Records"},
                    {"Property", "Item", "Transform", "MigrationIdentity", "DocMigrations::ItemIdentity"},
                    {"Proto", "Item", "Rename", "BeforeLegacy", "Legacy"},
                    {"Proto", "Item", "Transform", "Legacy", "DocMigrations::Prototype"},
                    {"Proto", "Item", "Remove", "Retired"},
                }},
        });
    }

    static constexpr string_view MIGRATION_SCRIPT = R"(
namespace DocMigrations
{
    void Verify(bool condition, const string&in message)
    {
        if (!condition) {
            throw(message);
        }
    }

    [[PropertyMigrator]]
    bool First(hstring&inout value, const DatabaseDocument&inout document)
    {
        int selector;
        document.Read("MigrationSelector", selector);

        if (selector != 1 || value != "old-first".hstr()) {
            return false;
        }

        hstring sibling;
        Verify(document.Read("MigrationSecond", sibling) && sibling == "old-second".hstr(), "Original sibling was changed");
        Verify(document.EntityType == "Game", "Wrong document owner");
        value = "new-first".hstr();

        return true;
    }

    [[PropertyMigrator]]
    bool Second(hstring&inout value, const DatabaseDocument&inout document)
    {
        int selector;
        document.Read("MigrationSelector", selector);

        if (selector != 1 || value != "old-second".hstr()) {
            return false;
        }

        hstring sibling;
        Verify(document.Read("MigrationFirst", sibling) && sibling == "old-first".hstr(), "Original renamed sibling was changed");
        value = "new-second".hstr();

        return true;
    }

    [[PropertyMigrator]]
    bool Note(string&inout value, const DatabaseDocument&inout document)
    {
        if (value == "new-note") {
            return false;
        }

        int selector;
        document.Read("MigrationSelector", selector);

        if (selector == 2) {
            value = "discarded";
            throw("Rejected document fixture");
        }
        if (selector == 3) {
            int wrong;
            document.Read("MigrationFirst", wrong);
        }

        Verify(document.ReadEncoded("_Opaque").length() > 0, "Missing encoded context");
        value = "new-note";

        return true;
    }

    [[PropertyMigrator]]
    bool Keep(string&inout value, const DatabaseDocument&inout document)
    {
        value = "discarded";

        return false;
    }

    [[PropertyMigrator]]
    bool Variant(any&inout value, const DatabaseDocument&inout document)
    {
        int selector;
        document.Read("MigrationSelector", selector);

        if (selector != 1 || value == any("new-variant")) {
            return false;
        }

        value = any("new-variant");

        return true;
    }

    [[PropertyMigrator]]
    bool Tokens(hstring[]&inout value, const DatabaseDocument&inout document)
    {
        if (value[0] == "new-token".hstr()) {
            return false;
        }

        hstring[] original;
        document.Read("MigrationTokens", original);
        Verify(original[0] == "old-token".hstr(), "Wrong array context");
        original[0] = "detached-copy".hstr();
        document.Read("MigrationTokens", original);
        Verify(original[0] == "old-token".hstr(), "Array context was changed");
        value[0] = "new-token".hstr();

        return true;
    }

    [[PropertyMigrator]]
    bool Counts(dict<hstring,int>&inout value, const DatabaseDocument&inout document)
    {
        if (value["old".hstr()] == 17) {
            return false;
        }

        dict<hstring,int> original;
        document.Read("MigrationCounts", original);
        Verify(original["old".hstr()] == 9, "Wrong dictionary context");
        value["old".hstr()] = 17;

        return true;
    }

    [[PropertyMigrator]]
    bool Snapshot(MigrationRecord&inout value, const DatabaseDocument&inout document)
    {
        if (value.Identity == "new-record".hstr()) {
            return false;
        }

        MigrationRecord original;
        document.Read("MigrationSnapshot", original);
        Verify(original.Identity == "old-record".hstr(), "Wrong record context");
        value.Identity = "new-record".hstr();

        return true;
    }

    [[PropertyMigrator]]
    bool Records(MigrationRecord[]&inout value, const DatabaseDocument&inout document)
    {
        if (value[0].Identity == "new-array-record".hstr()) {
            return false;
        }

        MigrationRecord[] original;
        document.Read("MigrationRecords", original);
        Verify(original[0].Identity == "old-array-record".hstr(), "Wrong record array context");
        value[0].Identity = "new-array-record".hstr();

        return true;
    }

    [[PropertyMigrator]]
    bool ItemIdentity(hstring&inout value, const DatabaseDocument&inout document)
    {
        int kind;
        Verify(document.Read("MigrationKind", kind), "Missing prototype default");
        string original;
        Verify(document.Read("_Proto", original), "Missing original prototype");

        if (kind == 7) {
            value = "prototype-default".hstr();
        }
        else {
            Verify(original == "Legacy", "Property migrator saw the new prototype");
            value = "new-item".hstr();
        }

        return true;
    }

    [[ProtoMigrator]]
    bool Prototype(hstring&inout value, const DatabaseDocument&inout document)
    {
        int kind;
        Verify(document.Read("MigrationKind", kind), "Missing prototype discriminator");
        string original;
        Verify(document.Read("_Proto", original) && original == "Legacy", "Wrong original prototype");

        if (kind == 0) {
            return false;
        }
        if (kind == 2) {
            value = "".hstr();
        }
        else if (kind == 3) {
            value = "BeforeLegacy".hstr();
        }
        else {
            value = "Current".hstr();
        }

        return true;
    }
}
)";

    static auto MakeMigrationResources() -> FileSystem
    {
        auto metadata = MakeMigrationMetadata();
        auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("MigrationCompiler");
        compiler_source->AddFile("Metadata.fometa-server", metadata);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));
        BakerServerEngine compiler {compiler_resources};
        auto scripts = BakerTests::CompileInlineScripts(&compiler, "DocumentMigrations", {{"Scripts/DocMigrations.fos", string {MIGRATION_SCRIPT}}}, [](string_view text) { logging::write("Migration script compiler: {}", text); });
        auto item_type = compiler.Hashes.to_hashed_string("Item");
        auto configure_proto = [](ProtoItem& proto) {
            auto kind = proto.GetProperties()->GetRegistrar()->FindProperty("MigrationKind");
            proto.GetPropertiesForEdit()->SetValue<int32_t>(kind.as_ptr(), 7);
        };
        vector<pair<string, function<void(ProtoItem&)>>> prototype_entries;
        prototype_entries.emplace_back("Current", configure_proto);
        auto prototypes = BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(compiler, item_type, prototype_entries);

        auto runtime_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("MigrationRuntime");
        runtime_source->AddFile("Metadata.fometa-server", metadata);
        runtime_source->AddFile("DocumentMigrations.fos-bin-server", scripts);
        runtime_source->AddFile("MigrationItems.fopro-bin-server", prototypes);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    static auto MakeGameDocument(int32_t selector) -> AnyData::Document
    {
        AnyData::Document document;
        document.Emplace("MigrationOldFirst", string {"old-first"});
        document.Emplace("MigrationSecond", string {"old-second"});
        document.Emplace("MigrationNote", string {"old-note"});
        document.Emplace("MigrationKeep", string {"exact original"});
        document.Emplace("MigrationSelector", AnyData::Value {static_cast<int64_t>(selector)});
        document.Emplace("MigrationVariant", string {"old-variant"});

        AnyData::Array tokens;
        tokens.EmplaceBack(AnyData::Value {string {"old-token"}});
        tokens.EmplaceBack(AnyData::Value {string {"unchanged"}});
        document.Emplace("MigrationTokens", std::move(tokens));

        AnyData::Dict counts;
        counts.Emplace("old", AnyData::Value {int64_t {9}});
        document.Emplace("MigrationCounts", std::move(counts));

        AnyData::Dict snapshot;
        snapshot.Emplace("Identity", AnyData::Value {string {"old-record"}});
        snapshot.Emplace("Kind", AnyData::Value {int64_t {1}});
        document.Emplace("MigrationSnapshot", std::move(snapshot));

        AnyData::Dict array_record;
        array_record.Emplace("Identity", string {"old-array-record"});
        array_record.Emplace("Kind", AnyData::Value {int64_t {1}});
        AnyData::Array records;
        records.EmplaceBack(AnyData::Value {std::move(array_record)});
        document.Emplace("MigrationRecords", std::move(records));

        document.Emplace("MigrationRetired", string {"opaque retired payload"});
        document.Emplace("_Opaque", string {"opaque original"});

        return document;
    }
}

TEST_CASE("AngelScriptDocumentMigrationSignatures", "[angelscript][migration]")
{
    const vector<tuple<string, string, string>> invalid {
        {"missing function", "void Different() {}", "function does not exist"},
        {"missing attribute", "bool Rewrite(hstring&inout value, const DatabaseDocument&inout document) { return false; }", "attributed global"},
        {"wrong return", "[[PropertyMigrator]] void Rewrite(hstring&inout value, const DatabaseDocument&inout document) {}", "must return bool"},
        {"async function", "[[PropertyMigrator]][[Async]] bool Rewrite(hstring&inout value, const DatabaseDocument&inout document) { return false; }", "must be synchronous"},
        {"wrong value type", "[[PropertyMigrator]] bool Rewrite(int&inout value, const DatabaseDocument&inout document) { return false; }", "value must match"},
        {"input value", "[[PropertyMigrator]] bool Rewrite(const hstring&in value, const DatabaseDocument&inout document) { return false; }", "value must match"},
        {"wrong context", "[[PropertyMigrator]] bool Rewrite(hstring&inout value, const string&in document) { return false; }", "requires const DatabaseDocument"},
        {"shadowed context type", "namespace Fake { class DatabaseDocument {} } [[PropertyMigrator]] bool Rewrite(hstring&inout value, const Fake::DatabaseDocument&in document) { return false; }", "requires const DatabaseDocument"},
        {"shadowed value type", "namespace Fake { class hstring {} } [[PropertyMigrator]] bool Rewrite(Fake::hstring&inout value, const DatabaseDocument&inout document) { return false; }", "value must match"},
        {"mutable context", "[[PropertyMigrator]] bool Rewrite(hstring&inout value, DatabaseDocument&inout document) { return false; }", "requires const DatabaseDocument"},
        {"ambiguous function", "[[PropertyMigrator]] bool Rewrite(hstring&inout value, const DatabaseDocument&inout document) { return false; } bool Rewrite(int value) { return false; }", "function is ambiguous"},
        {"retained context", "DatabaseDocument? saved; [[PropertyMigrator]] bool Rewrite(hstring&inout value, const DatabaseDocument&inout document) { saved = document; return false; }", "Unable to build module"},
        {"direct invocation", "[[PropertyMigrator]] bool Rewrite(hstring&inout value, const DatabaseDocument&inout document) { return false; } void Call(hstring&inout value, const DatabaseDocument&inout document) { Rewrite(value, document); }", "Attributed function usage validation failed"},
    };

    for (const auto& [reason, script, error] : invalid) {
        INFO(reason);
        auto metadata = BakerTests::MakeMetadataBlob({
            {"Property", {{"Game", "Server", "hstring", "MigrationValue", "Mutable", "Persistent"}}},
            {"MigrationRule", {{"Property", "Game", "Transform", "MigrationValue", "SignatureTest::Rewrite"}}},
        });
        auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("SignatureMetadata");
        source->AddFile("Metadata.fometa-server", metadata);
        FileSystem resources;
        resources.AddCustomSource(std::move(source));
        BakerServerEngine compiler {resources};

        CHECK_THROWS_WITH(BakerTests::CompileInlineScripts(&compiler, "SignatureTest", {{"Scripts/SignatureTest.fos", "namespace SignatureTest { " + script + " }"}}, [](string_view) { }), Catch::Matchers::ContainsSubstring(error.c_str()));
    }
}

TEST_CASE("AngelScriptDocumentMigrations", "[angelscript][migration]")
{
    auto settings = BakerTests::MakeScriptCompilerSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedServerSettings(settings);
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeMigrationResources());
    auto shutdown = scope_exit([&]() noexcept {
        safe_call([&] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    for (int32_t attempt = 0; attempt < 6000 && !server->IsStarted() && !server->IsStartingError(); attempt++) {
        std::this_thread::sleep_for(std::chrono::milliseconds {10});
    }

    REQUIRE(server->IsStarted());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&]() noexcept { safe_call([&] { server->Unlock(); }); });
    auto game_registrar = server->GetPropertyRegistrar(server->Hashes.to_hashed_string("Game"));

    SECTION("DetachedValuesAndOriginalContext")
    {
        auto document = MakeGameDocument(1);
        AnyData::Document updates;

        CHECK(PropertiesSerializer::MigrateDocument(game_registrar.as_ptr(), document, &updates));
        CHECK(document["MigrationFirst"].AsString() == "new-first");
        CHECK(document["MigrationSecond"].AsString() == "new-second");
        CHECK(document["MigrationNote"].AsString() == "new-note");
        CHECK(document["MigrationVariant"].AsString() == "new-variant");
        CHECK(document["MigrationTokens"].AsArray()[0].AsString() == "new-token");
        CHECK(document["MigrationTokens"].AsArray()[1].AsString() == "unchanged");
        CHECK(document["MigrationCounts"].AsDict()["old"].AsInt64() == 17);
        CHECK(document["MigrationSnapshot"].AsDict()["Identity"].AsString() == "new-record");
        CHECK(document["MigrationRecords"].AsArray()[0].AsDict()["Identity"].AsString() == "new-array-record");
        CHECK(document["MigrationKeep"].AsString() == "exact original");
        CHECK(document["_Opaque"].AsString() == "opaque original");
        CHECK(document["MigrationRetired"].AsString() == "opaque retired payload");
        CHECK_FALSE(document.Contains("MigrationOldFirst"));
        CHECK_FALSE(updates.Contains("MigrationKeep"));
        CHECK_FALSE(updates.Contains("MigrationRetired"));

        AnyData::Document repeated_updates;

        CHECK_FALSE(PropertiesSerializer::MigrateDocument(game_registrar.as_ptr(), document, &repeated_updates));
        CHECK(repeated_updates.Empty());
    }
    SECTION("FalseRetainsDifferentDomain")
    {
        auto document = MakeGameDocument(0);

        CHECK(PropertiesSerializer::MigrateDocument(game_registrar.as_ptr(), document));
        CHECK(document["MigrationFirst"].AsString() == "old-first");
        CHECK(document["MigrationVariant"].AsString() == "old-variant");
    }
    SECTION("ExceptionAndTypeMismatchPreserveDocumentAndUpdates")
    {
        for (int32_t selector : {2, 3}) {
            auto document = MakeGameDocument(selector);
            string original = AnyData::ValueToString(AnyData::Value {document.Copy()});
            AnyData::Document updates;

            CHECK_THROWS(PropertiesSerializer::MigrateDocument(game_registrar.as_ptr(), document, &updates));
            CHECK(AnyData::ValueToString(AnyData::Value {document.Copy()}) == original);
            CHECK(updates.Empty());
        }
    }
    SECTION("ProtoTransformsUseOriginalContextAndDefaults")
    {
        auto item = server->Hashes.to_hashed_string("Item");
        auto legacy = server->Hashes.to_hashed_string("Legacy");
        AnyData::Document document;
        document.Emplace("_Proto", string {"Legacy"});
        document.Emplace("MigrationKind", AnyData::Value {int64_t {1}});
        document.Emplace("MigrationIdentity", string {"old-item"});
        auto result = server->ResolveDocumentProto(item, server->Hashes.to_hashed_string("BeforeLegacy"), document);

        CHECK(result.as_str() == "Current");
        AnyData::Document updates;
        CHECK(PropertiesSerializer::MigrateDocument(server->GetPropertyRegistrar(item).as_ptr(), document, &updates, AnyData::Value {string {result.as_str()}}));
        CHECK(document["_Proto"].AsString() == "Current");
        CHECK(document["MigrationIdentity"].AsString() == "new-item");
        CHECK(updates.Size() == 2);

        AnyData::Document defaults;
        defaults.Emplace("_Proto", string {"Current"});
        defaults.Emplace("MigrationIdentity", string {"old-item"});

        CHECK(PropertiesSerializer::MigrateDocument(server->GetPropertyRegistrar(item).as_ptr(), defaults));
        CHECK(defaults["MigrationIdentity"].AsString() == "prototype-default");

        document.Assign("_Proto", AnyData::Value {string {"Legacy"}});
        document.Assign("MigrationKind", AnyData::Value {int64_t {0}});
        CHECK(server->ResolveDocumentProto(item, legacy, document) == legacy);
        document.Assign("MigrationKind", AnyData::Value {int64_t {2}});
        CHECK_FALSE(server->ResolveDocumentProto(item, legacy, document));
        document.Assign("MigrationKind", AnyData::Value {int64_t {3}});
        CHECK_THROWS(server->ResolveDocumentProto(item, legacy, document));
        CHECK_FALSE(server->ResolveDocumentProto(item, server->Hashes.to_hashed_string("Retired"), document));
    }
}
#endif

FO_END_NAMESPACE
