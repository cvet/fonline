"""Regression tests for the exception-safety classification audit.

Parser fixtures are inline; audit fixtures are temporary Engine and project
trees with their own baselines. The final test checks the shipped Engine
baseline against the Engine sources of this checkout.
"""

from __future__ import annotations

import dataclasses
import subprocess
import sys
from pathlib import Path

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))

from native_safety_audit import cpp_parser  # noqa: E402
from native_safety_audit import exception_safety as audit  # noqa: E402
from native_safety_audit import scope  # noqa: E402
from native_safety_audit.cpp_parser import FuncEntry  # noqa: E402


CLI = BUILDTOOLS / "native_safety_audit.py"
ENGINE_ROOT = BUILDTOOLS.parent
FIXTURE_PATH = "Source/Test/Fixture.cpp"
HEADER = "\t".join(audit.TSV_COLUMNS) + "\n"

FIXTURE_SOURCE = '''\
// license header

#include "Stuff.h"

FO_BEGIN_NAMESPACE

static auto ParseThing(string_view text) -> int32_t
{
    FO_TRACE_ZONE(Core);

    const char* brace = "{ not a real brace }";
    return numeric_cast<int32_t>(text.size() + 1'000);
}

// Doc comment kept in place.
Item::Item(ptr<Engine> engine, ident_t id) noexcept :
    Entity(engine, id),
    _mask {0xFF},
    _name {"x"}
{
    FO_TRACE_ZONE(Entity);

    ignore_unused(id);
}

#if FO_WINDOWS
auto Item::PlatformThing() -> int32_t
{
    return 1;
}
#else
auto Item::PlatformThing() -> int32_t
{
    return 2;
}
#endif

auto Item::operator==(const Item& other) const -> bool
{
    const string raw = R"x(weird } stuff { here)x";
    return raw.size() == other.Size();
}

FO_GCC_IGNORE_WARNINGS_PUSH("-Wunused")
static void WrappedThing() noexcept
{
    FO_TRACE_ZONE_NAMED(Core, "Wrapped");

    ignore_unused(1);
}
FO_GCC_IGNORE_WARNINGS_POP

class StubThing final : public IThing
{
public:
    explicit StubThing(ptr<GlobalSettings> settings)
    {
        FO_TRACE_ZONE(Core);

        _renderer.Init(*settings);
    }
    auto First() const -> int32_t { return 1; }
    auto Second() const -> int32_t { return 2; }

private:
    vector<int32_t> _values FO_TSA_GUARDED_BY(_locker) {};
};

static auto Doubler = [](int32_t value) { return value * 2; };
static const int32_t Table[] {Doubler(1), Doubler(2)};

static void Defaulted(int32_t value = int32_t {}, any_t extra = any_t {})
{
    Consume(value, [&](int32_t inner) { return inner; });
}

///@ ExportMethod
FO_SCRIPT_API void Server_Item_DoThing(ptr<Item> self, int32_t count, any_t stackId = any_t {})
{
    self->Mutate(count);
    throw ScriptException("nope");
}

#define FO_MAKE_READERS(name) \\
    auto Item::Get##name() const -> int32_t \\
    { \\
        return _##name; \\
    }

inline void LocalHelperNoMarker(int32_t value)
{
    ignore_unused(value);
}

__declspec(naked) auto NakedThing(int32_t* value) noexcept -> int32_t
{
    __asm {
        xor eax, eax
        ret
    }
}

FO_END_NAMESPACE
'''


# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("newline", ["\n", "\r\n"])
def test_continued_line_comment_is_masked(newline: str) -> None:
    source = "// continued \\" + newline + "HiddenCall(); \\" + newline + "StillHidden();" + newline + "VisibleCall();" + newline

    code_mask, hash_mask = cpp_parser.build_masks(source)

    assert len(code_mask) == len(source) and len(hash_mask) == len(source)
    assert "HiddenCall" not in code_mask and "StillHidden" not in code_mask
    assert "HiddenCall" not in hash_mask and "StillHidden" not in hash_mask
    assert "VisibleCall" in code_mask and "VisibleCall" in hash_mask


def test_marker_and_export_discovery() -> None:
    parsed = cpp_parser.parse_file(FIXTURE_PATH, FIXTURE_SOURCE)
    names = sorted(f"{entry.kind}:{entry.function}" for entry in parsed.functions)

    assert names.count("marker:ParseThing") == 1
    assert names.count("marker:Item::Item") == 1
    assert names.count("marker:WrappedThing") == 1
    assert names.count("export:Server_Item_DoThing") == 1
    assert not any("PlatformThing" in name or "LocalHelperNoMarker" in name for name in names)


def test_shape_discovery_finds_every_definition_and_nothing_else() -> None:
    parsed = cpp_parser.parse_file(FIXTURE_PATH, FIXTURE_SOURCE)
    definitions = [entry.function for entry in parsed.definitions]
    by_function = {entry.function: entry for entry in parsed.functions}
    ctor = next(entry for entry in parsed.definitions if entry.function == "Item::Item")

    assert definitions == [
        "ParseThing", "Item::Item", "Item::PlatformThing", "Item::PlatformThing", "Item::operator==", "WrappedThing",
        "StubThing", "First", "Second", "Defaulted", "Server_Item_DoThing", "LocalHelperNoMarker", "NakedThing",
    ]
    assert parsed.code_lines[cpp_parser.offset_to_line(parsed.offsets, ctor.body_off)].strip() == "{"
    assert all(entry.body_hash == by_function[entry.function].body_hash for entry in parsed.definitions if entry.function in by_function)


def test_body_hash_ignores_comments_and_zones_but_not_code() -> None:
    base = {entry.function: entry.body_hash for entry in cpp_parser.parse_file(FIXTURE_PATH, FIXTURE_SOURCE).definitions}
    commented = FIXTURE_SOURCE.replace("    ignore_unused(value);\n", "    // a note\n    ignore_unused(value);\n")
    recategorized = FIXTURE_SOURCE.replace("FO_TRACE_ZONE(Core);", "FO_TRACE_ZONE(Map);")
    unzoned = FIXTURE_SOURCE.replace("    FO_TRACE_ZONE(Core);\n\n", "").replace("    FO_TRACE_ZONE(Entity);\n", "")
    changed = FIXTURE_SOURCE.replace("return raw.size() == other.Size();", "return raw.size() != other.Size();")

    for variant in (commented, recategorized, unzoned):
        assert {entry.function: entry.body_hash for entry in cpp_parser.parse_file(FIXTURE_PATH, variant).definitions} == base

    changed_hashes = {entry.function: entry.body_hash for entry in cpp_parser.parse_file(FIXTURE_PATH, changed).definitions}
    assert changed_hashes["Item::operator=="] != base["Item::operator=="]
    assert changed_hashes["ParseThing"] == base["ParseThing"]


def test_markerless_and_macro_family_rows_are_located() -> None:
    parsed = cpp_parser.parse_file(FIXTURE_PATH, FIXTURE_SOURCE)
    helper = next(entry for entry in parsed.definitions if entry.function == "LocalHelperNoMarker")
    macro = FuncEntry(FIXTURE_PATH, "macro_family", "FO_MAKE_READERS", "#define FO_MAKE_READERS", "")

    located = cpp_parser.locate_markerless(parsed, dataclasses.replace(helper, body_hash=""))
    located_macro = cpp_parser.locate_macro_family(parsed, macro)

    assert located is not None and located.body_hash == helper.body_hash
    assert located_macro is not None and located_macro.body_hash
    assert cpp_parser.locate_macro_family(parsed, dataclasses.replace(macro, function="FO_MISSING")) is None


# ---------------------------------------------------------------------------
# Baseline maintenance primitives
# ---------------------------------------------------------------------------

def platform_entry(body_hash: str, level: str = "Unclassified", status: str = "unclassified") -> FuncEntry:
    return FuncEntry("Source/Test.cpp", "marker", "PlatformThing", "void PlatformThing()", body_hash, level, status)


def test_refresh_keeps_unchanged_duplicates_and_rejects_ambiguity() -> None:
    duplicate_base = [platform_entry("same", "Strong", "verified"), platform_entry("old", "Strong", "verified")]
    ambiguous_base = [platform_entry("old-a", "Strong", "verified"), platform_entry("old-b", "Basic", "verified")]

    refreshed, problem = audit.refresh_group_hashes(duplicate_base, [platform_entry("same"), platform_entry("new")])
    ambiguous, ambiguous_problem = audit.refresh_group_hashes(ambiguous_base, [platform_entry("new-a"), platform_entry("new-b")])

    assert refreshed == 1 and not problem
    assert sorted(entry.body_hash for entry in duplicate_base) == ["new", "same"]
    assert all(entry.level == "Strong" and entry.status == "verified" for entry in duplicate_base)
    assert ambiguous == 0 and "different classifications" in ambiguous_problem
    assert [entry.body_hash for entry in ambiguous_base] == ["old-a", "old-b"]


@pytest.mark.parametrize(("base", "current"), [(["a"], ["a", "b"]), (["a", "b"], ["a"])])
def test_refresh_rejects_definition_count_changes(base: list[str], current: list[str]) -> None:
    refreshed, problem = audit.refresh_group_hashes([platform_entry(h, "Strong", "verified") for h in base], [platform_entry(h) for h in current])

    assert refreshed == 0 and "definition count changed" in problem


def test_signature_migration_requires_an_identical_body() -> None:
    same_body = [FuncEntry("Source/Test.cpp", "marker", "Thing", "void Thing(int* value)", "body", "Strong", "verified", "Pure read")]
    changed_body = [FuncEntry("Source/Test.cpp", "marker", "Changed", "void Changed(int* value)", "old", "Basic", "verified", "Partial write")]

    migrated, notes = audit.migrate_signature_only_entries(same_body, [FuncEntry("Source/Test.cpp", "marker", "Thing", "void Thing(ptr<int> value)", "body")])
    not_migrated, changed_notes = audit.migrate_signature_only_entries(changed_body, [FuncEntry("Source/Test.cpp", "marker", "Changed", "void Changed(ptr<int> value)", "new")])

    assert migrated == 1 and not notes
    assert same_body[0].signature == "void Thing(ptr<int> value)"
    assert (same_body[0].level, same_body[0].status, same_body[0].reason) == ("Strong", "verified", "Pure read")
    assert not_migrated == 0 and not changed_notes
    assert changed_body[0].signature == "void Changed(int* value)"


VALID_ROW = "\t".join(("Source/Test.cpp", "marker", "Thing", "void Thing()", "h", "Strong", "verified", "", "Pure read"))


def load_rows(tmp_path: Path, rows: list[str]) -> tuple[list[FuncEntry], list[str]]:
    path = tmp_path.resolve() / "baseline.tsv"
    path.write_text(HEADER + "".join(row + "\n" for row in rows), encoding="utf-8")

    return audit.load_baseline(path)


def test_baseline_round_trip_preserves_empty_trailing_columns(tmp_path: Path) -> None:
    path = tmp_path.resolve() / "baseline.tsv"

    audit.save_baseline(path, [FuncEntry("Source/Test.cpp", "marker", "Thing", "void Thing()", "h", "Strong", "verified")])
    entries, problems = audit.load_baseline(path)

    assert len(path.read_text(encoding="utf-8").splitlines()[1].split("\t")) == len(audit.TSV_COLUMNS)
    assert len(entries) == 1 and not problems


@pytest.mark.parametrize(
    ("row", "message"),
    [
        (VALID_ROW.replace("\tStrong\t", "\tStrng\t"), "invalid level"),
        (VALID_ROW.replace("\tverified\t", "\tverifed\t"), "invalid status"),
        (VALID_ROW.replace("\tmarker\t", "\tmarkr\t"), "invalid kind"),
        (VALID_ROW.rsplit("\t", 1)[0], f"expected {len(audit.TSV_COLUMNS)} columns, got {len(audit.TSV_COLUMNS) - 1}"),
        (VALID_ROW + "\textra", f"expected {len(audit.TSV_COLUMNS)} columns, got {len(audit.TSV_COLUMNS) + 1}"),
    ],
)
def test_malformed_baseline_rows_are_problems(tmp_path: Path, row: str, message: str) -> None:
    _entries, problems = load_rows(tmp_path, [row])

    assert any(message in problem for problem in problems), problems


def test_hand_edited_spaces_and_crlf_rows_are_normalized(tmp_path: Path) -> None:
    entries, problems = load_rows(tmp_path, [VALID_ROW.replace("\tStrong\t", "\tStrong \t") + "\r"])

    assert not problems
    assert entries[0].level == "Strong" and entries[0].review_note == ""


# ---------------------------------------------------------------------------
# Audit and update over fixture trees
# ---------------------------------------------------------------------------

ENGINE_SOURCE = """
void Engine::Work()
{
    FO_TRACE_ZONE(Core);

    Step();
}
"""

PROJECT_SOURCE = """
void Project::Run()
{
    FO_TRACE_ZONE(Core);

    Engine::Work();
}
"""


def make_tree(root: Path, files: dict[str, str]) -> None:
    for relative, text in files.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")


def run_cli(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(CLI), "exception-safety", *args, "--root", str(root), "--engine-root", str(root / "Engine")],
        capture_output=True,
        text=True,
        encoding="utf-8",
    )


def classify(path: Path, level: str = "Basic", reason: str = "Steps may throw after a mutation; invariants hold") -> None:
    lines = path.read_text(encoding="utf-8").split("\n")

    for index, line in enumerate(lines[1:], start=1):
        parts = line.split("\t")

        if len(parts) == len(audit.TSV_COLUMNS) and parts[5] == "Unclassified":
            parts[5], parts[6], parts[8] = level, "annotated", reason
            lines[index] = "\t".join(parts)

    path.write_text("\n".join(lines), encoding="utf-8")


@pytest.fixture
def engine_tree(tmp_path: Path) -> Path:
    root = tmp_path.resolve()
    make_tree(root, {
        "Engine/Source/Core/Work.cpp": ENGINE_SOURCE,
        "Engine/Source/Tests/Test_Work.cpp": ENGINE_SOURCE.replace("Engine::Work", "TestOnly"),
        "Engine/Source/Core/Generated.template.cpp": ENGINE_SOURCE.replace("Engine::Work", "TemplateOnly"),
        "Engine/BuildTools/audit_baselines/exception-safety.tsv": HEADER,
    })

    result = run_cli(root, "update", "--add-new")
    assert result.returncode == 0, result.stdout
    classify(root / "Engine" / audit.ENGINE_BASELINE)

    return root


def test_classified_engine_tree_passes_and_skips_tests_and_templates(engine_tree: Path) -> None:
    baseline = (engine_tree / "Engine" / audit.ENGINE_BASELINE).read_text(encoding="utf-8")

    result = run_cli(engine_tree, "audit")

    assert result.returncode == 0, result.stdout
    assert "1 functions checked, 0 error(s)" in result.stdout
    assert "TestOnly" not in baseline and "TemplateOnly" not in baseline


@pytest.mark.parametrize(
    ("mutation", "message"),
    [
        (lambda root: (root / "Engine/Source/Core/Work.cpp").write_text(ENGINE_SOURCE.replace("Step();", "Step();\n    Other();"), encoding="utf-8"), "body changed since classification"),
        (lambda root: (root / "Engine/Source/Core/Work.cpp").write_text(ENGINE_SOURCE + ENGINE_SOURCE.replace("Engine::Work", "Engine::More"), encoding="utf-8"), "new function is not classified"),
        (lambda root: (root / "Engine/Source/Core/Work.cpp").write_text("", encoding="utf-8"), "baseline entry vanished from source"),
        (lambda root: classify_reason(root, ""), "baseline row has no reason"),
        (lambda root: classify_reason(root, "Basic", level="Unclassified"), "unclassified entry"),
    ],
)
def test_audit_failures(engine_tree: Path, mutation, message: str) -> None:
    mutation(engine_tree)

    result = run_cli(engine_tree, "audit")

    assert result.returncode == 1
    assert message in result.stdout, result.stdout


def classify_reason(root: Path, reason: str, level: str = "Basic") -> None:
    path = root / "Engine" / audit.ENGINE_BASELINE
    header, row = path.read_text(encoding="utf-8").split("\n")[:2]
    parts = row.split("\t")
    parts[5], parts[8] = level, reason
    path.write_text(header + "\n" + "\t".join(parts) + "\n", encoding="utf-8")


def test_accept_refreshes_only_the_named_function(engine_tree: Path) -> None:
    (engine_tree / "Engine/Source/Core/Work.cpp").write_text(ENGINE_SOURCE.replace("Step();", "Step();\n    Other();"), encoding="utf-8")

    accepted = run_cli(engine_tree, "update", "--accept", "Source/Core/Work.cpp::Engine::Work")
    result = run_cli(engine_tree, "audit")

    assert "accepted 1 refreshed body hash(es)" in accepted.stdout
    assert result.returncode == 0, result.stdout


def test_none_rows_warn_and_fail_only_on_request(engine_tree: Path) -> None:
    classify_reason(engine_tree, "A throw leaves the cache half rebuilt", level="None")

    default = run_cli(engine_tree, "audit")
    strict = run_cli(engine_tree, "audit", "--fail-on", "warning")

    assert default.returncode == 0
    assert "ES: None pending skeptic adjudication" in default.stdout
    assert strict.returncode == 1


def test_project_scope_uses_its_own_baseline(engine_tree: Path) -> None:
    make_tree(engine_tree, {"SourceExt/Run.cpp": PROJECT_SOURCE, "SourceExt/Tests/Test_Run.cpp": PROJECT_SOURCE.replace("Project::Run", "TestOnly")})

    missing = run_cli(engine_tree, "audit", "--scope", "SourceExt", "--baseline", "project-es.tsv")
    created = run_cli(engine_tree, "update", "--add-new", "--scope", "SourceExt", "--baseline", "project-es.tsv")
    unclassified = run_cli(engine_tree, "audit", "--scope", "SourceExt", "--baseline", "project-es.tsv")
    classify(engine_tree / "project-es.tsv")
    passing = run_cli(engine_tree, "audit", "--scope", "SourceExt", "--baseline", "project-es.tsv")

    project_rows = (engine_tree / "project-es.tsv").read_text(encoding="utf-8")
    engine_rows = (engine_tree / "Engine" / audit.ENGINE_BASELINE).read_text(encoding="utf-8")
    assert missing.returncode == 1 and "baseline not found" in missing.stdout
    assert created.returncode == 0, created.stdout
    assert unclassified.returncode == 1 and "SourceExt/Run.cpp: unclassified entry: Project::Run" in unclassified.stdout
    assert passing.returncode == 0, passing.stdout
    assert "2 functions checked" in passing.stdout
    assert "SourceExt/Run.cpp\tmarker\tProject::Run" in project_rows and "TestOnly" not in project_rows
    assert "Project::Run" not in engine_rows


def test_engine_baseline_row_cannot_classify_a_project_function(engine_tree: Path) -> None:
    make_tree(engine_tree, {"SourceExt/Run.cpp": PROJECT_SOURCE, "project-es.tsv": HEADER})
    engine_baseline = engine_tree / "Engine" / audit.ENGINE_BASELINE
    run_cli(engine_tree, "update", "--add-new", "--scope", "SourceExt", "--baseline", "project-es.tsv")
    project_row = (engine_tree / "project-es.tsv").read_text(encoding="utf-8").split("\n")[1]
    engine_baseline.write_text(engine_baseline.read_text(encoding="utf-8") + project_row.replace("Unclassified\tunclassified\t\t", "Basic\tannotated\t\tMisplaced") + "\n", encoding="utf-8")
    (engine_tree / "project-es.tsv").write_text(HEADER, encoding="utf-8")

    result = run_cli(engine_tree, "audit", "--scope", "SourceExt", "--baseline", "project-es.tsv")

    assert result.returncode == 1
    assert "SourceExt/Run.cpp:2: new function is not classified: Project::Run" in result.stdout
    assert "SourceExt/Run.cpp: baseline entry vanished from source: Project::Run" in result.stdout


def test_vendored_exclusions_apply_and_stale_exclusions_fail(engine_tree: Path) -> None:
    make_tree(engine_tree, {
        "SourceExt/Run.cpp": PROJECT_SOURCE,
        "SourceExt/Vendor/Lib.cpp": PROJECT_SOURCE.replace("Project::Run", "Vendor::Run"),
        "project-es.tsv": HEADER,
        "config.json": '{"schema_version": 1, "exclude": [{"path": "SourceExt/Vendor", "reason": "Vendored library"}]}',
        "stale.json": '{"schema_version": 1, "exclude": [{"path": "SourceExt/Gone", "reason": "Removed library"}]}',
    })
    args = ("--scope", "SourceExt", "--baseline", "project-es.tsv")
    run_cli(engine_tree, "update", "--add-new", *args, "--config", "config.json")
    classify(engine_tree / "project-es.tsv")

    passing = run_cli(engine_tree, "audit", *args, "--config", "config.json")
    stale = run_cli(engine_tree, "audit", *args, "--config", "stale.json")

    assert passing.returncode == 0, passing.stdout
    assert "Vendor::Run" not in (engine_tree / "project-es.tsv").read_text(encoding="utf-8")
    assert stale.returncode == 1
    assert "configuration exclusion matches no file in the project scopes: SourceExt/Gone" in stale.stdout


@pytest.mark.parametrize(
    ("args", "message"),
    [
        (("audit", "--scope", "SourceExt"), "requires a project --baseline"),
        (("audit", "--baseline", "project-es.tsv"), "requires at least one --scope"),
        (("audit", "--scope", "../Outside", "--baseline", "project-es.tsv"), "root-relative"),
    ],
)
def test_invalid_scope_arguments_exit_with_usage_error(engine_tree: Path, args: tuple[str, ...], message: str) -> None:
    make_tree(engine_tree, {"SourceExt/Run.cpp": PROJECT_SOURCE})

    result = run_cli(engine_tree, *args)

    assert result.returncode == 2
    assert message in result.stderr


def test_report_lists_levels_and_filters(engine_tree: Path) -> None:
    summary = run_cli(engine_tree, "report")
    filtered = run_cli(engine_tree, "report", "--level", "Basic", "--area", "Core")

    assert summary.returncode == 0
    assert "=== Exception-safety classification: 1 functions ===" in summary.stdout
    assert "1 matching function(s):" in filtered.stdout
    assert "Source/Core/Work.cpp :: Engine::Work (Steps may throw after a mutation; invariants hold)" in filtered.stdout


# ---------------------------------------------------------------------------
# Shipped Engine baseline
# ---------------------------------------------------------------------------

def test_engine_baseline_matches_the_engine_sources() -> None:
    context = scope.create_context(ENGINE_ROOT, ENGINE_ROOT, [], None)
    units, errors = audit.create_units(context, None)

    checked, unit_errors, _warnings = audit.audit_unit(units[0])

    assert errors == []
    assert unit_errors == []
    assert checked > 0
