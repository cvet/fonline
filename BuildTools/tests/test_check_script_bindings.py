"""Tests for check_script_bindings: native ptr/nptr rules and AngelScript handler parity."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS.parent
sys.path.insert(0, str(BUILDTOOLS))
import check_script_bindings as bindings  # noqa: E402


CHECKER = BUILDTOOLS / "check_script_bindings.py"


def native(text: str) -> list[bindings.Finding]:
    findings, _ = bindings.check_native_text("Source/Fixture.cpp", text)
    return findings


def project(tmp_path: Path, files: dict[str, str]) -> Path:
    root = tmp_path.resolve()

    for name, text in files.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    return root


def run_cli(*args: object) -> subprocess.CompletedProcess[str]:
    return subprocess.run([sys.executable, str(CHECKER), *(str(arg) for arg in args)],
                          capture_output=True, text=True, encoding="utf-8")


def test_export_method_rejects_raw_return_receiver_and_argument() -> None:
    findings = native("///@ ExportMethod\nFO_SCRIPT_API Critter* Server_Game_Find(ServerEngine* engine, Item* item);\n")

    assert sorted(item.message.split("'")[1] for item in findings) == ["Critter*", "Item*", "ServerEngine*"]
    assert all(item.rule == "raw-pointer" and item.line == 2 for item in findings)
    assert all("ExportMethod signature" in item.message for item in findings)


def test_export_method_accepts_ptr_and_nptr() -> None:
    assert native("///@ ExportMethod\n"
                  "FO_SCRIPT_API nptr<Critter> Server_Game_Find(ptr<ServerEngine> engine, ptr<Item> item, "
                  "int32_t count = 2 * 3);\n") == []


def test_multiline_signature_is_checked_to_its_closing_parenthesis() -> None:
    findings = native("///@ ExportMethod\nFO_SCRIPT_API void Server_Game_Use(ptr<ServerEngine> engine,\n"
                      "    Item* item)\n{\n    Critter* local {};\n}\n")

    assert [(item.line, item.message.split("'")[1]) for item in findings] == [(3, "Item*")]


def test_unannotated_script_api_still_rejects_raw_handles() -> None:
    findings = native("FO_SCRIPT_API Item* NativeBridge(Critter* cr);\n")

    assert len(findings) == 2
    assert all("FO_SCRIPT_API signature" in item.message for item in findings)


def test_internal_pointers_engine_hooks_and_macro_definitions_are_outside_the_contract() -> None:
    assert native("Critter* FindInternal(Item* item);\n\n"
                  "///@ EngineHook\n"
                  "FO_SCRIPT_API void SetupBakersHook(const_span<string> names, BakingContext* context);\n"
                  "#define FO_SCRIPT_API extern \"C\" void*\n") == []


def test_ref_type_checks_only_members_named_by_its_export_list() -> None:
    findings = native("///@ ExportRefType Server RefCounted Export = GetDemand, Current\n"
                      "class DialogAnswer\n{\npublic:\n"
                      "    auto GetDemand(int32_t index) -> DialogAnswerReq*;\n"
                      "    DialogAnswerReq* GetInternal(int32_t index);\n"
                      "    Critter* Current {};\n"
                      "    Item* InternalItem {};\n};\n")

    assert sorted((item.line, item.message.split("'")[1]) for item in findings) == [
        (5, "DialogAnswerReq*"), (7, "Critter*")]


def test_template_arguments_and_entity_events_reject_nested_raw_handles() -> None:
    findings = native(
        "auto first = engine.FindFunc<void, Critter*, vector<Item*>>(name);\n"
        "auto second = engine.CheckFunc<bool, ptr<Critter>, nptr<Item>>(name);\n"
        "auto third = make_shared<ScriptFunc<bool, Map*>>(std::move(cb));\n"
        "auto forwarded = engine.FindFunc<TRet, Args...>(name);\n"
        "// engine.CheckFunc<void, Map*>(commented_out);\n"
        "const auto example = \"FindFunc<void, Location*>(not_code)\";\n"
        "const auto raw = R\"cpp(CheckFunc<void, Player*>(not_code))cpp\";\n"
        "///@ ExportEvent\n"
        "FO_ENTITY_EVENT(OnUse, ptr<Critter> /*cr*/, Item* /*item*/);\n")

    assert sorted((item.line, item.message.split("'")[1]) for item in findings) == [
        (1, "Critter*"), (1, "Item*"), (3, "Map*"), (9, "Item*")]


def test_obsolete_nullable_marker_is_rejected_outside_comments() -> None:
    findings = native("///@ ExportMethod\n"
                      "FO_SCRIPT_API void Server_Game_Find(ptr<ServerEngine> engine, FO_NULLABLE Item item);\n"
                      "// FO_NULLABLE in a comment is prose\n")

    assert [(item.rule, item.line) for item in findings] == [("obsolete-nullable-marker", 2)]


def test_stored_callback_must_spell_the_exported_parameter_signature() -> None:
    source = ("///@ ExportMethod\n"
              "FO_SCRIPT_API void Server_Map_Check(ptr<Map> self, ScriptFunc<bool, nptr<Item>> callback)\n"
              "{\n"
              "    auto stored = safe_alloc::make_shared<ScriptFunc<bool, ptr<Item>>>(std::move(callback));\n"
              "    auto kept = safe_alloc::make_shared<ScriptFunc<bool, nptr<Item>>>(std::move(callback));\n"
              "}\n")

    findings = native(source)

    assert [(item.rule, item.line) for item in findings] == [("stored-callback-signature", 4)]
    assert "ScriptFunc<bool,nptr<Item>>" in findings[0].message


def test_native_exported_events_become_declarations() -> None:
    _, declarations = bindings.check_native_text(
        "Source/Server/Critter.h",
        "    ///@ ExportEvent\n    FO_ENTITY_EVENT(OnDropped, ptr<Item> /*item*/, nptr<Critter> /*dropper*/);\n"
        "    FO_ENTITY_EVENT(OnInternal, nptr<Item> /*item*/);\n")

    assert [(item.name, item.nullable) for item in declarations] == [("OnDropped", (False, True))]


EVENT_SOURCE = """namespace Game
{
///@ Event Server Critter OnSeen(Critter? watcher)
///@ RemoteCall Server Pick(Item? item)
///@ RemoteCall Client Shown(Critter cr)

void Init(Critter cr)
{
    cr.OnSeen.Subscribe(Seen);
    cr.OnDropped.Subscribe(Game::Dropped);
}

[[Event]]
void Seen(Critter self, Critter watcher)
{
}

[[Event]]
void OnSeen(Critter self, Critter? watcher)
{
}

[[Event]]
void Dropped(Critter self, Item item, Critter dropper)
{
}

[[ServerRemoteCall]] [[Async]]
void Pick(Player player, Item item)
{
}

[[ClientRemoteCall]]
void Shown(Critter? cr)
{
}
}
"""

NATIVE_EVENT = "///@ ExportEvent\nFO_ENTITY_EVENT(OnDropped, ptr<Item> /*item*/, nptr<Critter> /*dropper*/);\n"


def test_handlers_must_match_event_and_remote_call_nullability(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Critter.h": NATIVE_EVENT, "Scripts/Game.fos": EVENT_SOURCE})

    findings, errors = bindings.check_bindings(root, ["Source"], ["Scripts"])

    assert errors == []
    assert [(item.rule, item.line, item.message.split("'")[1]) for item in findings] == [
        ("event-handler-nullability", 14, "Seen"),
        ("event-handler-nullability", 24, "Dropped"),
        ("remote-call-handler-nullability", 29, "Pick"),
        ("remote-call-handler-nullability", 34, "Shown"),
    ]
    assert "argument 2 'watcher' is declared nullable" in findings[0].message
    assert "Source/Critter.h:2" in findings[1].message


def test_matching_handlers_comments_and_ambiguous_events_pass(tmp_path: Path) -> None:
    script = """namespace Ok
{
///@ Event Server Game OnTwice(Critter? a)
///@ Event Client Game OnTwice(Critter a)
// cr.OnSeen.Subscribe(Seen); in a comment is not a subscription

void Init(Critter cr)
{
    Game.OnTwice.Subscribe(Twice);
    cr.OnDropped.Subscribe(Dropped);
}

void Twice(Critter a)
{
}

void Dropped(Critter self, Item item, Critter? dropper)
{
}

void Unrelated(Critter? a, int b)
{
}
}
"""
    root = project(tmp_path, {"Source/Critter.h": NATIVE_EVENT, "Scripts/Ok.fos": script})

    findings, errors = bindings.check_bindings(root, ["Source"], ["Scripts"])

    assert errors == []
    assert findings == []


def test_engine_source_and_examples_pass_without_a_baseline() -> None:
    findings, errors = bindings.check_bindings(ENGINE_ROOT, ["Source", "Examples"], ["Examples"])

    assert errors == []
    assert findings == [], [bindings.format_finding(item) for item in findings]


def test_cli_scope_errors_baseline_and_excludes(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "SourceExt/Bridge.cpp": "FO_SCRIPT_API Item* Bridge(ptr<Critter> cr);\n",
        "SourceExt/Vendor/Lib.cpp": "FO_SCRIPT_API Item* Vendored();\n",
    })

    missing = run_cli("--root", root, "--scope", "Missing")
    failing = run_cli("--root", root, "--scope", "SourceExt", "--exclude", "SourceExt/Vendor/*")

    assert missing.returncode == 2
    assert "scope directory does not exist" in missing.stderr
    assert failing.returncode == 1
    assert "SourceExt/Bridge.cpp:1: [raw-pointer]" in failing.stdout
    assert "Vendor" not in failing.stdout

    draft = root / "bindings.tsv"
    written = run_cli("--root", root, "--scope", "SourceExt", "--write-baseline", draft)

    assert written.returncode == 0
    assert run_cli("--root", root, "--scope", "SourceExt", "--baseline", draft).returncode == 2

    draft.write_text(draft.read_text(encoding="utf-8").replace("\t1\t\n", "\t1\tBridge pending ptr<T>\n"),
                     encoding="utf-8")
    accepted = run_cli("--root", root, "--scope", "SourceExt", "--baseline", draft)

    assert accepted.returncode == 0, accepted.stdout + accepted.stderr

    (root / "SourceExt/Vendor/Lib.cpp").write_text("FO_SCRIPT_API ptr<Item> Vendored();\n", encoding="utf-8")
    stale = run_cli("--root", root, "--scope", "SourceExt", "--baseline", draft)

    assert stale.returncode == 1
    assert "raw-pointer SourceExt/Vendor/Lib.cpp matches no finding" in stale.stdout


def test_cli_lists_every_rule() -> None:
    listed = run_cli("--list-rules")

    assert listed.returncode == 0
    assert [line.split("\t")[0] for line in listed.stdout.splitlines()] == list(bindings.RULES)


@pytest.mark.parametrize("scope", ["../Outside", "/absolute", "C:/drive"])
def test_scopes_must_stay_inside_the_root(tmp_path: Path, scope: str) -> None:
    _, errors = bindings.check_bindings(tmp_path.resolve(), [scope])

    assert errors and "root-relative" in errors[0]
