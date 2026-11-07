"""Regression tests for format_content and the foconfig reader it relies on.

All projects are built in tmp_path. The native reader is represented by foconfig, whose cases below are taken
from Source/Tests/Test_ConfigFile.cpp; the formatter must leave every section, key and value unchanged.
"""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import buildtools  # noqa: E402
import foconfig  # noqa: E402
import format_content as fc  # noqa: E402


def write(path: Path, content: str | bytes) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content if isinstance(content, bytes) else content.encode("utf-8"))
    return path


def fomain_lines(lines: list[str], order: dict[str, int] | None = None) -> list[str]:
    return fc.format_fomain_text("\n".join(lines), order or {}).split("\n")


def proto_text(text: str, ordering: dict[str, fc.EntityOrdering] | None = None, name: str | None = None) -> str:
    return fc.format_proto_text(text, ordering or {}, name)


def sections(text: str) -> list[tuple[str, dict[str, str]]]:
    parser = foconfig.ConfigParser()
    parser.loadFromText(text)
    return [(section.name, section.content) for section in parser.sections]


def run_cli(*args: str) -> int:
    return fc.main(list(args))


# foconfig follows the native ConfigFile


@pytest.mark.parametrize("suffix", ["", "\n"])
def test_reader_keeps_terminal_continuation(suffix: str) -> None:
    assert sections(f"Size = \\{suffix}") == [("", {"Size": ""})]
    assert sections(f"Name = first \\{suffix}") == [("", {"Name": "first"})]


def test_reader_appends_and_ignores_empty_append_for_existing_key() -> None:
    assert sections("[ProtoItem]\nName = Base\nName += Two\n")[1] == ("ProtoItem", {"Name": "Base Two"})
    assert sections("[ProtoItem]\nName += Two\n")[1] == ("ProtoItem", {"Name": "Two"})
    assert sections("[ProtoItem]\nName = Base\nName +=    # ignored\n")[1] == ("ProtoItem", {"Name": "Base"})


def test_reader_comment_rules_follow_quotes_and_escapes() -> None:
    content = sections(
        "[Section]\n"
        "Escaped = keep\\#hash # strip this\n"
        'Quoted = "quoted # hash" # strip this\n'
        'Continued = "quoted # hash" \\\n'
        "continued # tail\n"
        'EscapedQuote = "quoted \\" # hash" # strip this\n'
    )[1][1]

    assert content == {
        "Escaped": "keep\\#hash",
        "Quoted": '"quoted # hash"',
        "Continued": '"quoted # hash" continued',
        "EscapedQuote": '"quoted \\" # hash"',
    }


def test_reader_whitespace_and_line_endings_match_the_engine() -> None:
    assert sections("[Section]\n\fCount\v=\f42\v\n")[1] == ("Section", {"Count": "42"})
    assert sections("[ShaderCommon]\r\nline_1 \\\r\nline_2 = x\r\n[Section]\r\nKey = Value\r\n")[1:] == [
        ("ShaderCommon", {"line_1 line_2": "x"}),
        ("Section", {"Key": "Value"}),
    ]
    # Only LF ends a line, other Unicode whitespace is part of a value
    assert sections("Key = a\rB = b\nNbsp = x \n") == [("", {"Key": "a\rB = b", "Nbsp": "x "})]


def test_reader_ignores_malformed_headers_and_reads_header_suffixes() -> None:
    assert sections("[]\nNoSeparator\n[ValidSection\n[Good] trailing\nKey = Value\n") == [("", {}), ("Good", {"Key": "Value"})]
    assert sections("[ProtoItem]\n[$Name/Item]\nKind = Nested\n")[2] == ("$Name/Item", {"Kind": "Nested"})


def test_reader_continuation_swallows_the_next_header() -> None:
    assert sections("Key = a \\\n[Section]\nOther = b\n") == [("", {"Key": "a [Section]", "Other": "b"})]


def test_reader_file_strips_bom_but_text_keeps_it(tmp_path: Path) -> None:
    path = write(tmp_path / "Bom.fopro", b"\xef\xbb\xbf[ProtoItem]\nWeight = 1\n")
    parser = foconfig.ConfigParser()
    parser.loadFromFile(path)

    assert parser.getSections("ProtoItem")[0].content == {"Weight": "1"}
    assert sections("﻿[ProtoItem]\nWeight = 1\n") == [("", {"Weight": "1"})]


# Main config layout


def test_comment_does_not_break_a_setting_group() -> None:
    lines = [
        "Packaging.CodeSigningHook = ",
        "# Windows MSI installer settings follow.",
        "Packaging.AppIcon = Resources/Game.ico",
        "Packaging.MsiUpgradeCode = GUID",
        "WorldTime.Multiplier = 20",
    ]

    assert fomain_lines(lines) == [
        "Packaging.CodeSigningHook =",
        "# Windows MSI installer settings follow.",
        "Packaging.AppIcon = Resources/Game.ico",
        "Packaging.MsiUpgradeCode = GUID",
        "",
        "WorldTime.Multiplier = 20",
    ]


def test_empty_setting_value_has_no_trailing_whitespace() -> None:
    assert fomain_lines(["Testing.Filter = ", "Testing.Enabled=False"]) == ["Testing.Filter =", "Testing.Enabled = False"]


def test_blank_line_only_separates_different_groups() -> None:
    assert fomain_lines(["Alpha.One = 1", "# beta block", "Beta.Two = 2"]) == ["Alpha.One = 1", "", "# beta block", "Beta.Two = 2"]


def test_grouped_subconfig_stays_contiguous_without_group_breaks() -> None:
    lines = ["[SubConfig]", "Beta.Two = 2", "Parent = Base", "Name = Demo", "Alpha.One = 1", "# beta block", "Beta.Three = 3"]

    assert fomain_lines(lines) == [
        "[SubConfig]",
        "Name = Demo",
        "Parent = Base",
        "Alpha.One = 1",
        "Beta.Two = 2",
        "# beta block",
        "Beta.Three = 3",
    ]


def test_attached_comment_before_section_stays_attached() -> None:
    lines = ["WorldTime.Multiplier = 20", "", "[SubConfig]", "Name = First", "Alpha.One = 1", "# second sub config", "# keeps this context", "[SubConfig]", "Name = Second"]

    assert fomain_lines(lines) == [
        "WorldTime.Multiplier = 20",
        "",
        "[SubConfig]",
        "Name = First",
        "Alpha.One = 1",
        "",
        "# second sub config",
        "# keeps this context",
        "[SubConfig]",
        "Name = Second",
    ]


def test_detached_comment_before_section_stays_detached() -> None:
    lines = ["WorldTime.Multiplier = 20", "", "[SubConfig]", "Name = First", "# applies below", "", "[SubConfig]", "Name = Second"]

    assert fomain_lines(lines) == ["WorldTime.Multiplier = 20", "", "[SubConfig]", "Name = First", "", "# applies below", "", "[SubConfig]", "Name = Second"]


def test_detached_banner_before_first_section_stays_detached() -> None:
    lines = ["WorldTime.Multiplier = 20", "", "# Sub configs", "#", "", "[SubConfig]", "Name = First"]

    assert fomain_lines(lines) == lines


def test_settings_follow_declaration_order_and_groups_sort_case_insensitively() -> None:
    order = {"beta.B": 0, "Alpha.Second": 1, "Alpha.First": 2}
    lines = ["# File header", "", "Name = Plain", "Alpha.First = 1", "beta.B = 2", "Alpha.Unknown = 3", "Alpha.Second = 4"]

    assert fomain_lines(lines, order) == ["# File header", "", "Name = Plain", "", "Alpha.Second = 4", "Alpha.First = 1", "Alpha.Unknown = 3", "", "beta.B = 2"]


def test_resource_pack_sections_are_kept_as_authored() -> None:
    lines = ["[ResourcePack]", "Name = Protos", "  InputDirs = Content   ", "Bakers = Proto"]

    assert fomain_lines(lines) == ["[ResourcePack]", "Name = Protos", "  InputDirs = Content", "Bakers = Proto"]


def test_append_and_continued_settings_keep_their_order_and_lines() -> None:
    lines = ["Zeta.List = a", "Alpha.Long = one \\", "    two", "Zeta.List += b", "Zeta.List+=c"]

    assert fomain_lines(lines) == ["Alpha.Long = one \\", "    two", "", "Zeta.List = a", "Zeta.List += b", "Zeta.List += c"]


# Prototype layout


def entity(properties: list[str], markers: tuple[str, ...] = ()) -> fc.EntityOrdering:
    ordering = fc.EntityOrdering()

    for rank, name in enumerate(properties):
        root = name.split(".", 1)[0] if "." in name or name in markers else ""
        ordering.properties[name] = fc.PropertyOrder(rank, root)
        if root:
            ordering.components.setdefault(root, rank)

    return ordering


def test_comment_does_not_break_a_property_group() -> None:
    assert proto_text("[ProtoItem]\n$Name = Foo\nWeight = 5\n# trade value\nCost = 10\n") == (
        "[ProtoItem]\n$Name = Foo\n\nWeight = 5\n# trade value\nCost = 10\n"
    )


def test_blank_line_separates_component_roots_not_comments() -> None:
    text = "[ProtoItem]\n$Name = Gun\nWeapon.DmgMin = 4\n# upper bound\nWeapon.DmgMax = 8\nArmor.DTPhysical = 2\n"

    assert proto_text(text) == "[ProtoItem]\n$Name = Gun\n\nWeapon.DmgMin = 4\n# upper bound\nWeapon.DmgMax = 8\n\nArmor.DTPhysical = 2\n"


def test_unknown_component_marker_stays_with_its_properties() -> None:
    text = "[ProtoItem]\n$Name = Suit\nArmor = True\nHeatSource = True\nArmor.Camouflage = 0\nHeatSource.EquippedTemperature = 32\n"

    assert proto_text(text) == (
        "[ProtoItem]\n$Name = Suit\n\nArmor = True\nArmor.Camouflage = 0\n\nHeatSource = True\nHeatSource.EquippedTemperature = 32\n"
    )


def test_properties_follow_metadata_order_and_text_comes_last() -> None:
    ordering = {"Critter": entity(["Strength", "Bulletin", "Bulletin.Region", "Agility"], markers=("Bulletin",))}
    text = "[ProtoCritter]\n$Text engl = Guard\nBulletin.Region = North\nCustom = 1\nAgility = 2\n$Parent = Base\nBulletin = True\nStrength = 3\n"

    assert proto_text(text, ordering, "Guard") == (
        "[ProtoCritter]\n$Name = Guard\n$Parent = Base\n\nStrength = 3\nAgility = 2\nCustom = 1\n\nBulletin = True\nBulletin.Region = North\n\n$Text engl = Guard\n"
    )


def test_attached_comment_before_section_stays_attached_in_protos() -> None:
    text = "[ProtoItem]\n$Name = First\nWeight = 1\n# second item\n# keeps this context\n[ProtoItem]\n$Name = Second\nWeight = 2\n"

    assert proto_text(text) == (
        "[ProtoItem]\n$Name = First\n\nWeight = 1\n\n# second item\n# keeps this context\n[ProtoItem]\n$Name = Second\n\nWeight = 2\n"
    )


def test_detached_comment_before_section_stays_detached_in_protos() -> None:
    text = "[ProtoItem]\n$Name = First\nWeight = 1\n# applies below\n\n[ProtoItem]\n$Name = Second\nWeight = 2\n"

    assert proto_text(text) == "[ProtoItem]\n$Name = First\n\nWeight = 1\n\n# applies below\n\n[ProtoItem]\n$Name = Second\n\nWeight = 2\n"


def test_detached_comment_at_file_start_and_fixed_type_section_stay_as_authored() -> None:
    text = "# Doc: Quest notes\n\n[QuestData]\nId = Quest\nAlpha = 1\n"

    assert proto_text(text, name="Quest") == text


def test_map_proto_and_nested_sections_are_left_to_the_mapper() -> None:
    text = "[ProtoMap]\n$Text engl = Map\nSize = 40 40\n$Name = Demo\n\n[Demo/Item]\n$Proto = Floor\n$Id = 1\n"

    assert proto_text(text, name="Demo") == text


def test_missing_name_is_written_as_the_baker_fallback() -> None:
    assert proto_text("[ProtoItem]\nWeight = 1\n", name="Crate") == "[ProtoItem]\n$Name = Crate\n\nWeight = 1\n"


def test_trailing_comment_of_the_last_section_stays_last() -> None:
    text = "[ProtoItem]\n$Name = Suit\nArmor.Camouflage = 0\nWeight = 1\n# end of the item\n"
    once = proto_text(text)

    assert once == "[ProtoItem]\n$Name = Suit\n\nWeight = 1\n\nArmor.Camouflage = 0\n# end of the item\n"
    assert proto_text(once) == once


def test_any_proto_entity_section_is_formatted() -> None:
    assert proto_text("[ProtoModifier]\nPower = 1\n$Name = Haste\n") == "[ProtoModifier]\n$Name = Haste\n\nPower = 1\n"


# Metadata and resource packs


def test_resource_glob_matches_the_engine_grammar() -> None:
    assert fc.match_resource_glob("Item.fopro", "*")
    assert not fc.match_resource_glob("Dir/Item.fopro", "*")
    assert fc.match_resource_glob("Dir/Sub/Item.fopro", "**")
    assert fc.match_resource_glob("Item.fomap", "**/*.fomap")
    assert fc.match_resource_glob("Dir/Item.fomap", "**/*.fomap")
    assert not fc.match_resource_glob("Dir/Item.fopro", "**/*.fomap")
    assert fc.match_resource_glob("Dir/_Draft.fomap", "**/_*.fomap")
    assert fc.match_resource_glob("A1.fos", "A?.fos")
    assert not fc.match_resource_glob("A/1.fos", "A?1.fos")


def metadata(declarations: str) -> str:
    return f"namespace Game;\n\npublic static partial class Probe\n{{\n{declarations}\n}}\n"


def test_metadata_packs_use_the_metadata_baker_order(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    write(root / "Scripts/Admin.cs", metadata("///@ Setting Client int32 Admin.SpawnColumns"))
    write(root / "Scripts/Admin.Inspection.cs", metadata("///@ Setting Server int32 Admin.MaxInspectionItems // limit"))
    write(root / "Scripts/Modules/Module.cs", metadata("///@ Setting Server int32 Admin.ModuleLimit"))
    write(root / "Scripts/Tests/Probe.fos", "///@ Setting Server int32 Admin.ProbeLimit\n")
    write(root / "Scripts/Notes.txt", "///@ Setting Server int32 Admin.Ignored\n")
    write(root / "Shadow/Admin.cs", metadata("///@ Setting Server int32 Admin.Shadowed"))
    fomain = write(root / "Game.fomain", "[ResourcePack]\nName = Metadata\nInputDirs = Scripts Shadow Scripts/Tests\nBakers = Metadata\n")
    config = foconfig.ConfigParser()
    config.loadFromFile(fomain)

    files = fc.discover_metadata_files(fomain, config)

    # In-pack paths sort bytewise like MetadataBaker and the first input dir providing a path wins; a file reachable
    # from two input dirs has two in-pack paths, as in the Engine file system
    assert [path.relative_to(root).as_posix() for path in files] == [
        "Scripts/Admin.Inspection.cs",
        "Scripts/Admin.cs",
        "Scripts/Modules/Module.cs",
        "Scripts/Tests/Probe.fos",
        "Scripts/Tests/Probe.fos",
    ]
    assert fc.read_metadata_order(files)[1] == ["Admin.MaxInspectionItems", "Admin.SpawnColumns", "Admin.ModuleLimit", "Admin.ProbeLimit", "Admin.ProbeLimit"]


def test_non_recursive_include_patterns_do_not_cross_directories(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    for relative in ("Scripts/Freeze.cs", "Scripts/Legacy.fos", "Scripts/Nested/Skipped.cs", "Scripts/Scenes/Scene.cs", "Scripts/Native.cpp"):
        write(root / relative, "")
    fomain = write(root / "Game.fomain", "[ResourcePack]\nName = Metadata\nInputDirs = Scripts Scripts/Scenes\nIncludePatterns = *\nBakers = Metadata\n")
    config = foconfig.ConfigParser()
    config.loadFromFile(fomain)

    assert [path.relative_to(root).as_posix() for path in fc.discover_metadata_files(fomain, config)] == [
        "Scripts/Freeze.cs",
        "Scripts/Legacy.fos",
        "Scripts/Scenes/Scene.cs",
    ]


def test_property_tags_handle_complex_types_components_and_comments(tmp_path: Path) -> None:
    path = write(
        tmp_path / "Props.fos",
        "///@ Property Item Common dict<int32, string> Labels Mutable Persistent\n"
        "///@ Property Item Common bool Weapon Component // marker\n"
        "///@ Property Item Server int32 Weapon.DmgMin \\\n    Group = Combat\n"
        "///@ Property Critter Common int32 Strength\n"
        "///@ Property Item Common int32 Labels\n",
    )

    ordering, _ = fc.read_metadata_order([path])

    assert {name: (info.rank, info.component_root) for name, info in ordering["Item"].properties.items()} == {
        "Labels": (0, ""),
        "Weapon": (1, "Weapon"),
        "Weapon.DmgMin": (2, "Weapon"),
    }
    assert ordering["Item"].components == {"Weapon": 1}
    assert list(ordering["Critter"].properties) == ["Strength"]


def make_project(root: Path, protos_pack: str = "InputDirs = Content\nBakers = Proto\n") -> Path:
    write(root / "Scripts/Game.fos", "///@ Property Item Common int32 Weight\n///@ Property Item Common int32 Cost\n///@ Setting Server int32 Game.Limit\n")
    write(root / "Content/Items/Crate.foitem", "[ProtoItem]\nCost = 2\nWeight = 1\n")
    write(root / "Content/Notes.txt", "[ProtoItem]\nCost = 2\nWeight = 1\n")
    return write(
        root / "Game.fomain",
        "Game.Limit = 3\nCommon.GameName = Demo\nBaking.ProtoFileExtensions = fopro foitem\n\n"
        "[ResourcePack]\nName = Metadata\nInputDirs = Scripts\nBakers = Metadata\n\n"
        f"[ResourcePack]\nName = Protos\n{protos_pack}",
    )


def test_end_to_end_formats_main_config_and_recursive_protos(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    fomain = make_project(root)

    assert run_cli("--project-root", str(root)) == fc.EXIT_OK
    assert fomain.read_text(encoding="utf-8") == (
        "Baking.ProtoFileExtensions = fopro foitem\n\nCommon.GameName = Demo\n\nGame.Limit = 3\n\n"
        "[ResourcePack]\nName = Metadata\nInputDirs = Scripts\nBakers = Metadata\n\n"
        "[ResourcePack]\nName = Protos\nInputDirs = Content\nBakers = Proto\n"
    )
    assert (root / "Content/Items/Crate.foitem").read_text(encoding="utf-8") == "[ProtoItem]\n$Name = Crate\n\nWeight = 1\nCost = 2\n"
    assert (root / "Content/Notes.txt").read_text(encoding="utf-8") == "[ProtoItem]\nCost = 2\nWeight = 1\n"
    assert run_cli("--project-root", str(root), "--check") == fc.EXIT_OK


def test_explicit_metadata_and_proto_paths_replace_discovery(tmp_path: Path) -> None:
    root = (tmp_path / "project").resolve()
    make_project(root)
    external = write(tmp_path.resolve() / "External.fos", "///@ Property Item Common int32 Cost\n///@ Property Item Common int32 Weight\n")
    other = write(root / "Elsewhere/Box.fopro", "[ProtoItem]\nWeight = 1\nCost = 2\n")

    assert run_cli("--project-root", str(root), "--skip-fomain", "--metadata", str(external), "--proto-path", "Elsewhere") == fc.EXIT_OK
    assert other.read_text(encoding="utf-8") == "[ProtoItem]\n$Name = Box\n\nCost = 2\nWeight = 1\n"
    assert (root / "Content/Items/Crate.foitem").read_text(encoding="utf-8") == "[ProtoItem]\nCost = 2\nWeight = 1\n"


def test_explicit_proto_file_with_an_unknown_extension_fails(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    make_project(root)

    assert run_cli("--project-root", str(root), "--proto-path", "Content/Notes.txt") == fc.EXIT_ERROR


def test_ignored_and_excluded_inputs_are_not_formatted(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    make_project(root, "InputDirs = Content Drafts\nExcludePatterns = **/_*.foitem\nBakers = Proto\n")
    draft = write(root / "Content/_Draft.foitem", "[ProtoItem]\nCost = 2\n")
    ignored = write(root / "Drafts/Ignored.foitem", "[ProtoItem]\nCost = 2\n")
    fomain = root / "Game.fomain"
    write(fomain, "Baking.IgnoreInputDirs = Drafts\n" + fomain.read_text(encoding="utf-8"))

    assert run_cli("--project-root", str(root), "--skip-fomain") == fc.EXIT_OK
    assert draft.read_text(encoding="utf-8") == "[ProtoItem]\nCost = 2\n"
    assert ignored.read_text(encoding="utf-8") == "[ProtoItem]\nCost = 2\n"


def test_engine_default_proto_extension_applies_without_the_setting(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    write(root / "Content/Box.fopro", "[ProtoItem]\nWeight = 1\n")
    write(root / "Content/Box.foitem", "[ProtoItem]\nWeight = 1\n")
    write(root / "Game.fomain", "[ResourcePack]\nName = Protos\nInputDirs = Content\nBakers = Proto\n")

    assert fc.read_engine_setting_default("Baking", "ProtoFileExtensions") == "fopro"
    assert run_cli("--project-root", str(root), "--skip-fomain") == fc.EXIT_OK
    assert (root / "Content/Box.fopro").read_text(encoding="utf-8").startswith("[ProtoItem]\n$Name = Box\n")
    assert (root / "Content/Box.foitem").read_text(encoding="utf-8") == "[ProtoItem]\nWeight = 1\n"


# Check mode, encodings and exit codes


def test_check_reports_drift_without_writing(tmp_path: Path, capsys) -> None:
    root = tmp_path.resolve()
    fomain = make_project(root)
    before = fomain.read_bytes()

    assert run_cli("--project-root", str(root), "--check") == fc.EXIT_DRIFT
    assert fomain.read_bytes() == before
    assert "Needs formatting: Game.fomain" in capsys.readouterr().out


@pytest.mark.parametrize("eol", ["\n", "\r\n"])
def test_bom_and_line_endings_are_preserved(tmp_path: Path, eol: str) -> None:
    path = write(tmp_path / "Bom.fopro", ("﻿# Header" + eol + "[ProtoItem]" + eol + "Weight = 1" + eol * 3).encode("utf-8"))

    assert fc.format_proto_file(path, {}, check=False).changed
    assert path.read_bytes() == ("﻿# Header" + eol + "[ProtoItem]" + eol + "$Name = Bom" + eol + eol + "Weight = 1" + eol).encode("utf-8")
    assert not fc.format_proto_file(path, {}, check=False).changed


def test_bom_before_a_header_is_refused_because_the_engine_reads_it_differently(tmp_path: Path) -> None:
    path = write(tmp_path / "Bom.fopro", b"\xef\xbb\xbf[ProtoItem]\nWeight = 1\n")
    before = path.read_bytes()

    with pytest.raises(fc.ContentFormatError, match="BOM"):
        fc.format_proto_file(path, {}, check=False)

    assert path.read_bytes() == before


def test_missing_or_ambiguous_main_config_is_an_error(tmp_path: Path) -> None:
    root = tmp_path.resolve()

    assert run_cli("--project-root", str(root)) == fc.EXIT_ERROR
    write(root / "A.fomain", "")
    write(root / "B.fomain", "")
    assert run_cli("--project-root", str(root)) == fc.EXIT_ERROR
    assert run_cli("--project-root", str(root), "--fomain", "A.fomain", "--skip-protos") == fc.EXIT_OK


# Meaning preservation


TRICKY_FOMAIN = (
    "# header\n\nZeta.List = a\nZeta.List += b\nAlpha.Long = one \\\n  two # tail\n"
    'Alpha.Quoted = "x # y" # comment\nNoSeparator\nAlpha.Escaped = a\\#b\n'
    "Common.Name = \\\n\nBroken = [x \\\n[NotAHeader\n[ ResourcePack ] tail\nName = A\n"
    "# moves to the sub-config\n[SubConfig]\nBeta.B = 1\nName = Sub\nAlpha.A = 2\nParent = A\n"
)
TRICKY_PROTO = (
    "Loose = 1\n# about the item\n[ProtoItem]\n$Text engl = \"Tag # one\"\nWeight = 1\nWeight += 2\nCost = 3 \\\n4\n"
    "Weapon.A = 1\nWeapon = True\n# comment \\\nHidden = swallowed\nGarbage line\n\n[QuestData]\nB = 1\nA = 2\n"
    "[ProtoCritter]\n$Parent = A B\n$Name = Npc\n"
)


@pytest.mark.parametrize("text", [TRICKY_FOMAIN, TRICKY_FOMAIN.replace("\n", "\r\n")])
def test_main_config_meaning_is_unchanged_and_formatting_is_idempotent(tmp_path: Path, text: str) -> None:
    path = write(tmp_path / "Game.fomain", text)
    order = {"Alpha.Quoted": 0, "Zeta.List": 1}

    assert fc.format_fomain_file(path, order, check=False).changed
    formatted = path.read_bytes().decode("utf-8")
    assert sections(formatted) == sections(text)
    assert ("\r\n" in formatted) == ("\r\n" in text)
    assert not fc.format_fomain_file(path, order, check=False).changed


@pytest.mark.parametrize("text", [TRICKY_PROTO, TRICKY_PROTO.replace("\n", "\r\n")])
def test_proto_meaning_is_unchanged_and_formatting_is_idempotent(tmp_path: Path, text: str) -> None:
    path = write(tmp_path / "Npc.fopro", text)
    ordering = {"Item": entity(["Cost", "Weight", "Weapon", "Weapon.A"], markers=("Weapon",))}

    assert fc.format_proto_file(path, ordering, check=False).changed
    formatted = path.read_bytes().decode("utf-8")
    assert fc.config_meaning(formatted, "Npc") == fc.config_meaning(text, "Npc")
    assert not fc.format_proto_file(path, ordering, check=False).changed


def test_moved_end_of_file_continuation_is_closed_by_a_blank_line(tmp_path: Path) -> None:
    text = "Zeta.Last = 1\nAlpha.First = 2\nAlpha.Tail = \\"
    path = write(tmp_path / "Game.fomain", text)

    assert fc.format_fomain_file(path, {"Alpha.Tail": 0, "Alpha.First": 1}, check=False).changed
    assert path.read_text(encoding="utf-8") == "Alpha.Tail = \\\n\nAlpha.First = 2\n\nZeta.Last = 1\n"
    assert sections(path.read_text(encoding="utf-8")) == sections(text)


def test_meaning_check_rejects_a_changed_reading(tmp_path: Path) -> None:
    path = tmp_path / "Game.fomain"

    with pytest.raises(fc.ContentFormatError, match="change how the Engine reads"):
        fc.verify_meaning(path, "A.B = 1\nA.B += 2\n", "A.B += 2\nA.B = 1\n", None)
    with pytest.raises(fc.ContentFormatError):
        fc.verify_meaning(path, "[ProtoItem]\n$Name = A\n", "[ProtoItem]\n$Name = B\n", "A")
    fc.verify_meaning(path, "[ProtoItem]\nWeight = 1\n", "[ProtoItem]\n$Name = Box\nWeight = 1\n", "Box")


def test_formatter_refuses_a_transform_that_changes_meaning(tmp_path: Path, monkeypatch) -> None:
    path = write(tmp_path / "Game.fomain", "A.B = 1\nA.B += 2\n")
    monkeypatch.setattr(fc, "format_fomain_text", lambda text, order: "A.B += 2\nA.B = 1\n")

    assert run_cli("--project-root", str(tmp_path.resolve()), "--skip-protos") == fc.EXIT_ERROR
    assert path.read_text(encoding="utf-8") == "A.B = 1\nA.B += 2\n"


# AngelScript through the Engine clang-format wrapper


@pytest.mark.parametrize("eol", ["\n", "\r\n"])
def test_fos_files_use_the_engine_wrapper_and_keep_bom_and_eol(tmp_path: Path, monkeypatch, eol: str) -> None:
    root = tmp_path.resolve()
    path = write(root / "Scripts/Game.fos", ("﻿Critter? Find();" + eol * 3).encode("utf-8"))
    write(root / "Scripts/Readme.txt", "x")
    monkeypatch.setattr(buildtools, "discover_clang_format", lambda: "clang-format-20")
    monkeypatch.setattr(buildtools, "run_capture_text", lambda *args, **kwargs: "﻿Critter ? Find();\n")

    assert run_cli("--project-root", str(root), "--skip-fomain", "--skip-protos", "--fos", "Scripts/**/*.fos", "--check") == fc.EXIT_DRIFT
    assert run_cli("--project-root", str(root), "--skip-fomain", "--skip-protos", "--fos", "Scripts/**/*.fos") == fc.EXIT_OK
    assert path.read_bytes() == ("﻿Critter? Find();" + eol).encode("utf-8")
    assert run_cli("--project-root", str(root), "--skip-fomain", "--skip-protos", "--fos", "Scripts/**/*.fos", "--check") == fc.EXIT_OK


def test_missing_clang_format_is_an_error(tmp_path: Path, monkeypatch) -> None:
    root = tmp_path.resolve()
    write(root / "Scripts/Game.fos", "void F() {}\n")

    def missing() -> str:
        raise SystemExit("clang-format version 20 not found")

    monkeypatch.setattr(buildtools, "discover_clang_format", missing)

    assert run_cli("--project-root", str(root), "--skip-fomain", "--skip-protos", "--fos", "Scripts/*.fos") == fc.EXIT_ERROR


def test_real_clang_format_round_trip(tmp_path: Path) -> None:
    try:
        clang_format = buildtools.discover_clang_format()
    except SystemExit as ex:
        pytest.skip(f"clang-format 20 is not available: {ex.code}")

    root = tmp_path.resolve()
    shutil.copyfile(BUILDTOOLS.parent / "Source" / ".clang-format", root / ".clang-format")
    path = write(root / "Game.fos", "namespace Game\n{\nCritter? Find(Critter? fallback = null) { return fallback; }\n}\n")

    assert run_cli("--project-root", str(root), "--skip-fomain", "--skip-protos", "--fos", "*.fos") == fc.EXIT_OK
    assert "Critter? Find(Critter? fallback = null)" in path.read_text(encoding="utf-8")
    assert run_cli("--project-root", str(root), "--skip-fomain", "--skip-protos", "--fos", "*.fos", "--check") == fc.EXIT_OK
    assert clang_format


# Nullable repair cases from both project formatters


@pytest.mark.parametrize(
    ("source", "expected"),
    [
        ("void f() {\n    dict<string, any> ? context = confirm.ActionContext;\n}\n", "void f() {\n    dict<string, any>? context = confirm.ActionContext;\n}\n"),
        ("class A {\n    dict<string, any> ? ActionContext;\n}\n", "class A {\n    dict<string, any>? ActionContext;\n}\n"),
        ("void f() {\n    array<Critter> ? found;\n}\n", "void f() {\n    array<Critter>? found;\n}\n"),
        ("void f() {\n    Critter ? cr = Game.GetCritter(id);\n    Item ? [] items;\n}\n", "void f() {\n    Critter? cr = Game.GetCritter(id);\n    Item?[] items;\n}\n"),
        ("Critter ? MakeFixture(Testing::TestContext ctx, Map homeMap)\n", "Critter? MakeFixture(Testing::TestContext ctx, Map homeMap)\n"),
        ("NpcPlanes::NpcPlane ? MakeFixture(Testing::TestContext ctx)\n", "NpcPlanes::NpcPlane? MakeFixture(Testing::TestContext ctx)\n"),
        ("ProtoItem ? MakeFixture(Critter cr, int slot = CritterItemSlot::Main)\n", "ProtoItem? MakeFixture(Critter cr, int slot = CritterItemSlot::Main)\n"),
        ("Item ? Find()\r\n", "Item? Find()\r\n"),
        ("Map ?\n    Create(Testing::TestContext ctx)\n", "Map? Create(Testing::TestContext ctx)\n"),
        ("class A {\n    Object ? get_Parent() final\n}\n", "class A {\n    Object? get_Parent() final\n}\n"),
        ("class A {\n    callback_void_Critter ? Script;\n}\n", "class A {\n    callback_void_Critter? Script;\n}\n"),
        ("void f() {\n    callback_void_Critter ? script = mi.Script;\n}\n", "void f() {\n    callback_void_Critter? script = mi.Script;\n}\n"),
        ("void f() {\n    Find(first, findFirst : true);\n}\n", "void f() {\n    Find(first, findFirst: true);\n}\n"),
        ("void f() {\n    Critter ? owner = id != 0 ? Game.GetCritter(id) : null;\n}\n", "void f() {\n    Critter? owner = id != 0 ? Game.GetCritter(id) : null;\n}\n"),
        ("Critter ? Find() // note: first\n", "Critter? Find() // note: first\n"),
    ],
)
def test_nullable_forms_are_repaired(source: str, expected: str) -> None:
    assert buildtools.fix_fos_nullable_suffix(source) == expected
    assert buildtools.fix_fos_nullable_suffix(expected) == expected


@pytest.mark.parametrize(
    "source",
    [
        "Condition ? Get(Testing::Value) : Other()\n",
        "Condition ? Get(flag ? first : second) : Other()\n",
        "class A {\n    int x = Flag ? Get(a) + 1 : 0;\n}\n",
        "    result = flag ? Testing::First() : Testing::Second();\n",
        "void f() {\n    int v = flag ? a : b;\n    return count > 0 ? first : second;\n}\n",
        "void f() {\n    x = flag ? value = 1 : 2;\n    y = (ready ? Get(a) : b);\n}\n",
    ],
)
def test_ternaries_are_left_alone(source: str) -> None:
    assert buildtools.fix_fos_nullable_suffix(source) == source


# Command registration


def test_buildtools_exposes_the_same_command_line() -> None:
    parser = buildtools.create_parser()
    args = parser.parse_args(["format-content", "--check", "--fomain", "Game.fomain", "--fos", "Scripts/**/*.fos", "--proto-path", "Content"])

    assert args.command == "format-content"
    assert (args.check, args.fomain, args.fos, args.proto_path) == (True, "Game.fomain", ["Scripts/**/*.fos"], ["Content"])
    assert vars(fc.create_parser().parse_args([])).keys() == vars(args).keys() - {"command"}


def test_buildtools_command_runs_in_the_project_root(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    make_project(root)
    env = {**os.environ, "FO_PROJECT_ROOT": str(root)}
    result = subprocess.run([sys.executable, str(BUILDTOOLS / "buildtools.py"), "format-content", "--check"], capture_output=True, text=True, env=env, check=False)

    assert result.returncode == fc.EXIT_DRIFT, result.stdout + result.stderr
    assert "Needs formatting: Game.fomain" in result.stdout
