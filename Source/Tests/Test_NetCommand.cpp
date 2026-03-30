#include "catch_amalgamated.hpp"

#include "Geometry.h"
#include "NetCommand.h"

FO_BEGIN_NAMESPACE

TEST_CASE("NetCommand")
{
    SECTION("UnknownCommandReturnsFalse")
    {
        HashStorage hashes;
        NetOutBuffer out_buf {8, true};

        const auto result = PackNetCommand("unknown 1 2 3", &out_buf, [](string_view) { }, hashes);

        CHECK_FALSE(result);
        CHECK(out_buf.GetDataSize() == 0);
    }

    SECTION("ExitCommandSerializesWithoutArguments")
    {
        HashStorage hashes;
        NetOutBuffer out_buf {8, true};

        REQUIRE(PackNetCommand("exit", &out_buf, [](string_view) { }, hashes));

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        REQUIRE(in_buf.NeedProcess());
        CHECK(in_buf.ReadMsg() == NetMessage::SendCommand);
        CHECK(in_buf.Read<uint8>() == CMD_EXIT);
    }

    SECTION("MoveCommandSerializesCritterAndHex")
    {
        HashStorage hashes;
        NetOutBuffer out_buf {8, true};

        REQUIRE(PackNetCommand("move 42 17 23", &out_buf, [](string_view) { }, hashes));

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        REQUIRE(in_buf.NeedProcess());
        CHECK(in_buf.ReadMsg() == NetMessage::SendCommand);
        CHECK(in_buf.Read<uint8>() == CMD_MOVECRIT);
        CHECK(in_buf.Read<ident_t>() == ident_t {42});
        const auto pos = in_buf.Read<mpos>();
        CHECK(pos == mpos {17, 23});
    }

    SECTION("AliasesAndHashedPayloadAreSerialized")
    {
        HashStorage hashes;
        NetOutBuffer out_buf {8, true};

        REQUIRE(PackNetCommand("ais ammo_10mm 5", &out_buf, [](string_view) { }, hashes));

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        REQUIRE(in_buf.NeedProcess());
        CHECK(in_buf.ReadMsg() == NetMessage::SendCommand);
        CHECK(in_buf.Read<uint8>() == CMD_ADDITEM_SELF);
        CHECK(in_buf.Read<hstring>(hashes).as_str() == "ammo_10mm");
        CHECK(in_buf.Read<int32>() == 5);
    }

    SECTION("AddNpcAndAddLocSerializeHashedProtos")
    {
        HashStorage hashes;
        NetOutBuffer npc_buf {8, true};
        NetOutBuffer loc_buf {8, true};

        REQUIRE(PackNetCommand("addnpc 3 4 5 critter_proto", &npc_buf, [](string_view) { }, hashes));
        REQUIRE(PackNetCommand("addloc town_proto", &loc_buf, [](string_view) { }, hashes));

        NetInBuffer npc_in {8};
        npc_in.AddData(npc_buf.GetData());
        REQUIRE(npc_in.NeedProcess());
        CHECK(npc_in.ReadMsg() == NetMessage::SendCommand);
        CHECK(npc_in.Read<uint8>() == CMD_ADDNPC);
        CHECK(npc_in.Read<mpos>() == mpos {3, 4});
        CHECK(npc_in.Read<uint8>() == 5);
        CHECK(npc_in.Read<hstring>(hashes).as_str() == "critter_proto");

        NetInBuffer loc_in {8};
        loc_in.AddData(loc_buf.GetData());
        REQUIRE(loc_in.NeedProcess());
        CHECK(loc_in.ReadMsg() == NetMessage::SendCommand);
        CHECK(loc_in.Read<uint8>() == CMD_ADDLOCATION);
        CHECK(loc_in.Read<hstring>(hashes).as_str() == "town_proto");
    }

    SECTION("PropertyCommandMarksSetValueAndTrimsWhitespace")
    {
        HashStorage hashes;
        NetOutBuffer out_buf {8, true};

        REQUIRE(PackNetCommand("prop 77 Health   150   ", &out_buf, [](string_view) { }, hashes));

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        REQUIRE(in_buf.NeedProcess());
        CHECK(in_buf.ReadMsg() == NetMessage::SendCommand);
        CHECK(in_buf.Read<uint8>() == CMD_PROPERTY);
        CHECK(in_buf.Read<ident_t>() == ident_t {77});
        CHECK(in_buf.Read<string>() == "Health");
        CHECK(in_buf.Read<bool>());
        CHECK(in_buf.Read<string>() == "150");
    }

    SECTION("PropertyCommandWithoutValueMarksQuery")
    {
        HashStorage hashes;
        NetOutBuffer out_buf {8, true};

        REQUIRE(PackNetCommand("prop 88 Stamina", &out_buf, [](string_view) { }, hashes));

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        REQUIRE(in_buf.NeedProcess());
        CHECK(in_buf.ReadMsg() == NetMessage::SendCommand);
        CHECK(in_buf.Read<uint8>() == CMD_PROPERTY);
        CHECK(in_buf.Read<ident_t>() == ident_t {88});
        CHECK(in_buf.Read<string>() == "Stamina");
        CHECK_FALSE(in_buf.Read<bool>());
        CHECK(in_buf.Read<string>().empty());
    }

    SECTION("RunAliasSerializesFunctionAndOptionalParameters")
    {
        HashStorage hashes;
        NetOutBuffer out_buf {8, true};

        REQUIRE(PackNetCommand("run module::Func alpha beta gamma", &out_buf, [](string_view) { }, hashes));

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        REQUIRE(in_buf.NeedProcess());
        CHECK(in_buf.ReadMsg() == NetMessage::SendCommand);
        CHECK(in_buf.Read<uint8>() == CMD_RUNSCRIPT);
        CHECK(in_buf.Read<string>() == "module::Func");
        CHECK(in_buf.Read<string>() == "alpha");
        CHECK(in_buf.Read<string>() == "beta");
        CHECK(in_buf.Read<string>() == "gamma");
    }

    SECTION("LogCommandSerializesFlags")
    {
        HashStorage hashes;
        NetOutBuffer out_buf {8, true};

        REQUIRE(PackNetCommand("log --", &out_buf, [](string_view) { }, hashes));

        NetInBuffer in_buf {8};
        in_buf.AddData(out_buf.GetData());

        REQUIRE(in_buf.NeedProcess());
        CHECK(in_buf.ReadMsg() == NetMessage::SendCommand);
        CHECK(in_buf.Read<uint8>() == CMD_LOG);
        CHECK(in_buf.Read<string>() == "--");
    }

    SECTION("InvalidArgumentsLogAndDoNotWriteMessage")
    {
        HashStorage hashes;
        NetOutBuffer out_buf {8, true};
        string log_message;

        REQUIRE(PackNetCommand("move 12 34", &out_buf, [&](string_view message) { log_message = string(message); }, hashes));

        CHECK(log_message == "Invalid arguments. Example: move cr_id hx hy");
        CHECK(out_buf.GetDataSize() == 0);
    }

    SECTION("InvalidRunscriptAndLogArgumentsDoNotWriteMessage")
    {
        HashStorage hashes;
        string log_message;

        {
            NetOutBuffer out_buf {8, true};
            REQUIRE(PackNetCommand("runscript", &out_buf, [&](string_view message) { log_message = string(message); }, hashes));
            CHECK(log_message == "No function name provided. Example: runscript module::func param0 param1 param2");
            CHECK(out_buf.GetDataSize() == 0);
        }

        {
            NetOutBuffer out_buf {8, true};
            log_message.clear();
            REQUIRE(PackNetCommand("log +++", &out_buf, [&](string_view message) { log_message = string(message); }, hashes));
            CHECK(log_message == "Invalid arguments. Example: log flag. Valid flags: '+' attach, '-' detach, '--' detach all");
            CHECK(out_buf.GetDataSize() == 0);
        }
    }
}

FO_END_NAMESPACE
