#include "catch_amalgamated.hpp"

#include "SettingsStore.h"

FO_BEGIN_NAMESPACE

TEST_CASE("SettingsStore")
{
    // Uses the real platform backend (registry on Windows, file store elsewhere) under a dedicated application
    // name so it never touches a tool's real settings. Every key is removed after use to leave no residue.
    SettingsStore store {"Test_SettingsStore"};

    for (const string_view key : {"str", "multiline", "int", "bool_true", "bool_false", "float"}) {
        store.Remove(key);
    }

    SECTION("StringRoundtrip")
    {
        store.SetString("str", "hello settings");

        CHECK(store.HasKey("str"));
        CHECK(store.GetString("str") == "hello settings");

        store.Remove("str");
    }

    SECTION("MultilineBlobRoundtrip")
    {
        // The ImGui layout blob is multi-line text and must survive the round-trip verbatim.
        string blob = "[Window][Preview]\nPos=10,20\nSize=300,400\n\n[Window][List]\nPos=0,0\n";
        store.SetString("multiline", blob);

        CHECK(store.GetString("multiline") == blob);

        store.Remove("multiline");
    }

    SECTION("TypedRoundtrip")
    {
        store.SetInt("int", -12345);
        store.SetBool("bool_true", true);
        store.SetBool("bool_false", false);
        store.SetFloat("float", 2.5);

        CHECK(store.GetInt("int") == -12345);
        CHECK(store.GetBool("bool_true"));
        CHECK_FALSE(store.GetBool("bool_false"));
        CHECK(store.GetFloat("float") == 2.5);

        store.Remove("int");
        store.Remove("bool_true");
        store.Remove("bool_false");
        store.Remove("float");
    }

    SECTION("MissingKeysReturnDefaults")
    {
        CHECK_FALSE(store.HasKey("missing"));
        CHECK(store.GetString("missing", "fallback") == "fallback");
        CHECK(store.GetInt("missing", 77) == 77);
        CHECK(store.GetBool("missing", true));
        CHECK(store.GetFloat("missing", 1.5) == 1.5);
    }

    SECTION("RemoveClearsKey")
    {
        store.SetString("str", "temp");
        REQUIRE(store.HasKey("str"));

        store.Remove("str");

        CHECK_FALSE(store.HasKey("str"));
        CHECK(store.GetString("str", "def") == "def");
    }
}

FO_END_NAMESPACE
