from __future__ import annotations

import json
from pathlib import Path
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import check_text_packs as ctp  # noqa: E402


def write_pack(root: Path, name: str, lines: list[str], newline: str = "\n") -> Path:
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes((newline.join(lines) + newline).encode("utf-8"))
    return path


def run_check(root: Path, *options: str) -> tuple[int, list[str]]:
    json_path = root.parent / f"{root.name}-findings.json"
    code = ctp.main([str(root), "--base-dir", str(root), "--json", str(json_path), *options])
    findings = json.loads(json_path.read_text(encoding="utf-8"))["findings"] if json_path.is_file() else []
    return code, [f"{item['rule']} {item['path']} {item['key']}".rstrip() for item in findings]


@pytest.fixture
def texts(tmp_path: Path) -> Path:
    root = tmp_path.resolve() / "Texts"
    root.mkdir()
    return root


def test_reader_follows_the_engine_entry_grammar() -> None:
    parsed = ctp.parse_text_pack("\n".join([
        "Plain heading without braces",
        "{Welcome}{}{Welcome to the wasteland.}",
        "{QuestName}{Short}{A difficult choice} trailing data is ignored",
        "{LongMessage}{}{First line",
        "Second line}",
        "  leading text {Indented}{}{found anywhere on the line}",
        "{Open without a close",
        "",
    ]))

    assert [(entry.key, entry.text, entry.line) for entry in parsed.entries] == [
        ("{Welcome}{}", "Welcome to the wasteland.", 2),
        ("{QuestName}{Short}", "A difficult choice", 3),
        ("{LongMessage}{}", "First line\nSecond line", 4),
        ("{Indented}{}", "found anywhere on the line", 6),
    ]
    assert parsed.errors == []


def test_reader_reports_entries_that_fail_baking() -> None:
    parsed = ctp.parse_text_pack("{OnlyKey}\n{}{}{Empty first key}\n{Good}{}{Kept}\n{Tail}{}{never closed\nlast line\n")

    assert [(error.line, error.message) for error in parsed.errors] == [
        (1, "entry has no second {Key2} field"),
        (2, "entry has an empty Key1"),
        (4, "entry has no closed third {Text} field"),
    ]
    assert [entry.key for entry in parsed.entries] == ["{Good}{}"]


def test_reader_ends_a_value_at_the_first_closing_brace_of_a_later_line() -> None:
    # There is no closing-brace escape, so an unclosed value swallows text up to the next entry's first brace
    parsed = ctp.parse_text_pack("{Unclosed}{}{runs on\n{Next}{}{swallowed}\n{After}{}{kept}\n")

    assert [(entry.key, entry.text) for entry in parsed.entries] == [("{Unclosed}{}", "runs on\n{Next"), ("{After}{}", "kept")]


def test_reader_keeps_braces_of_a_supposed_comment_as_an_entry() -> None:
    parsed = ctp.parse_text_pack("# see {Note}{}{this becomes text}\n")

    assert [entry.key for entry in parsed.entries] == ["{Note}{}"]


def test_reader_keeps_crlf_and_bom_like_the_engine() -> None:
    parsed = ctp.parse_text_pack("\ufeff{A}{}{One}\r\n{B}{}{Two\r\nlines}\r\n")

    assert [(entry.key, entry.text) for entry in parsed.entries] == [("{A}{}", "One"), ("{B}{}", "Two\r\nlines")]


def test_bake_language_declarations_follow_the_engine_rules() -> None:
    config = ctp.parse_bake_languages(["russ", "engl", "ru18:russ", "en18:engl"])

    assert config.base == "russ"
    assert config.fallback("engl") == "russ"
    assert config.fallback("en18") == "engl"

    for declarations in ([], ["engl", "engl"], ["ru18:russ", "russ"], ["engl", ":engl"], ["engl", "a:b:c"], ["engl", "x:"]):
        with pytest.raises(ctp.UsageError):
            ctp.parse_bake_languages(declarations)


def test_script_of_a_letter_comes_from_its_unicode_name() -> None:
    assert [ctp.letter_script(char) for char in "aЖΩ中ｶ1 !"] == ["latin", "cyrillic", "greek", "han", "katakana", None, None, None]


def test_clean_two_language_packs_pass(texts: Path, capsys: pytest.CaptureFixture[str]) -> None:
    write_pack(texts, "Game.engl.fotxt", ["{Bark}{}{Hold still!}", "{Bark}{}{Fresh meat!}", "{Title}{}{@color:FF00FF@Journal@color@}"])
    write_pack(texts, "Game.russ.fotxt", ["{Bark}{}{Стой смирно!}", "{Bark}{}{Свежатина!}", "{Title}{}{@color:0xFF00FF@Журнал@color@}"])

    assert run_check(texts, "--languages", "engl russ", "--script", "engl=latin", "--script", "russ=cyrillic") == (0, [])
    assert "Text pack check passed: 2 file(s), 6 entries, 0 exempted" in capsys.readouterr().out


def test_identical_variant_fails(texts: Path) -> None:
    write_pack(texts, "Game.engl.fotxt", ["{PdaTitle}{}{Handbook}", "{Other}{}{Handbook}", "{PdaTitle}{}{Handbook}"])

    assert run_check(texts, "--languages", "engl") == (1, ["duplicate-variant Game.engl.fotxt {PdaTitle}{}"])


def test_wrong_script_is_reported_per_language(texts: Path) -> None:
    write_pack(texts, "Game.engl.fotxt", ["{Cargo}{}{Your cargo:}", "{Cargo}{}{Ваш груз:}", "{Device}{}{Open the КПК}"])
    write_pack(texts, "Game.russ.fotxt", ["{Cargo}{}{Your load:}", "{Cargo}{}{Ваш груз:}", "{Device}{}{Откройте КПК}"])

    code, findings = run_check(texts, "--languages", "engl russ", "--script", "engl=latin", "--script", "russ=cyrillic,latin")

    assert code == 1
    assert findings == ["wrong-script Game.engl.fotxt {Cargo}{}", "wrong-script Game.engl.fotxt {Device}{}"]


def test_ignore_pattern_removes_project_placeholders(texts: Path) -> None:
    write_pack(texts, "Game.russ.fotxt", ["{Greeting}{}{Привет, @pname@!}"])

    assert run_check(texts, "--languages", "russ", "--script", "russ=cyrillic")[1] == ["wrong-script Game.russ.fotxt {Greeting}{}"]
    assert run_check(texts, "--languages", "russ", "--script", "russ=cyrillic", "--ignore-pattern", "@[a-z]+@") == (0, [])


def test_key_parity_reports_missing_and_extra_keys(texts: Path) -> None:
    write_pack(texts, "Game.russ.fotxt", ["{Shared}{}{Общий ключ}", "{OnlyRuss}{}{Первая}", "{OnlyRuss}{}{Вторая}"])
    write_pack(texts, "Game.engl.fotxt", ["{Shared}{}{Shared key}", "{OnlyEngl}{}{Leftover English key}"])

    code, findings = run_check(texts, "--languages", "russ engl")

    assert code == 1
    assert findings == ["missing-key Game.engl.fotxt {OnlyRuss}{}", "extra-key Game.engl.fotxt {OnlyEngl}{}"]


def test_pack_parity_reports_missing_and_orphan_packs(texts: Path) -> None:
    write_pack(texts, "Game.engl.fotxt", ["{Shared}{}{Shared}"])
    write_pack(texts, "Dialogs.russ.fotxt", ["{Line}{}{Строка}"])

    code, findings = run_check(texts, "--languages", "engl russ")

    assert code == 1
    assert findings == ["orphan-pack Dialogs.russ.fotxt", "missing-pack Game.russ.fotxt"]


def test_sparse_language_inherits_but_still_rejects_extra_keys(texts: Path) -> None:
    write_pack(texts, "Game.russ.fotxt", ["{Shared}{}{Обычная строка}", "{Adult}{}{Сдержанная строка}"])
    write_pack(texts, "Game.engl.fotxt", ["{Shared}{}{Ordinary line}", "{Adult}{}{Restrained line}"])
    write_pack(texts, "Game.ru18.fotxt", ["{Adult}{}{Грубая строка}", "{Unknown}{}{Лишний ключ}"])
    write_pack(texts, "Combat.russ.fotxt", ["{Attack}{}{Получай!}"])
    write_pack(texts, "Combat.engl.fotxt", ["{Attack}{}{Take this!}"])

    code, findings = run_check(texts, "--languages", "russ engl ru18:russ en18:engl", "--sparse", "ru18", "--sparse", "en18")

    assert code == 1
    assert findings == ["extra-key Game.ru18.fotxt {Unknown}{}"]


def test_fallback_chain_is_normalized_in_declaration_order(texts: Path) -> None:
    # A child of a non-base language still ends with the base keys: the key absent from both parents is dropped
    write_pack(texts, "Game.engl.fotxt", ["{A}{}{A}", "{B}{}{B}"])
    write_pack(texts, "Game.russ.fotxt", ["{A}{}{А}"])
    write_pack(texts, "Game.ru18.fotxt", ["{B}{}{Б}", "{C}{}{В}"])

    code, findings = run_check(texts, "--languages", "engl russ ru18:russ")

    assert code == 1
    assert findings == ["missing-key Game.ru18.fotxt {A}{}", "extra-key Game.ru18.fotxt {C}{}", "missing-key Game.russ.fotxt {B}{}"]


def test_variant_count_mismatch_is_opt_in(texts: Path) -> None:
    write_pack(texts, "Game.russ.fotxt", ["{Bark}{}{Стой смирно!}", "{Bark}{}{Свежатина на льду!}"])
    write_pack(texts, "Game.engl.fotxt", ["{Bark}{}{Hold still!}"])

    assert run_check(texts, "--languages", "russ engl") == (0, [])
    assert run_check(texts, "--languages", "russ engl", "--strict-variant-counts") == (1, ["variant-count-mismatch Game.engl.fotxt {Bark}{}"])


def test_structural_problems_are_reported(texts: Path) -> None:
    write_pack(texts, "Game.engl.fotxt", ["{A}{}{A}", "{Broken}"])
    write_pack(texts, "Nested/Game.engl.fotxt", ["{A}{}{Second copy}"])
    write_pack(texts, "Game.deu.fotxt", ["{A}{}{A}"])
    write_pack(texts, "Game.fotxt", ["{A}{}{A}"])
    (texts / "Menu.engl.fotxt").write_bytes(b"{A}{}{\xff}\n")

    code, findings = run_check(texts, "--languages", "engl")

    assert code == 1
    assert findings == [
        "unknown-language Game.deu.fotxt",
        "parse-error Game.engl.fotxt",
        "invalid-file-name Game.fotxt",
        "invalid-utf8 Menu.engl.fotxt",
        "duplicate-pack-file Nested/Game.engl.fotxt",
    ]


def test_baseline_accepts_known_findings_and_rejects_stale_ones(texts: Path) -> None:
    write_pack(texts, "Game.russ.fotxt", ["{Shared}{}{Общий}", "{Untranslated}{}{Ещё нет}"])
    write_pack(texts, "Game.engl.fotxt", ["{Shared}{}{Shared}"])
    baseline = texts / "text-baseline.json"
    exemption = {"rule": "missing-key", "path": "Game.engl.fotxt", "key": "{Untranslated}{}", "reason": "Translation is scheduled with the next quest batch."}
    baseline.write_text(json.dumps({"schema_version": 1, "exemptions": [exemption]}), encoding="utf-8")

    assert run_check(texts, "--languages", "russ engl", "--baseline", str(baseline)) == (0, [])

    write_pack(texts, "Game.engl.fotxt", ["{Shared}{}{Shared}", "{Untranslated}{}{Not yet}"])

    assert run_check(texts, "--languages", "russ engl", "--baseline", str(baseline)) == (1, ["stale-exemption text-baseline.json {Untranslated}{}"])


@pytest.mark.parametrize("exemptions", [
    [{"rule": "missing-key", "path": "Game.engl.fotxt", "key": "{A}{}", "reason": " "}],
    [{"rule": "no-such-rule", "path": "Game.engl.fotxt", "key": "{A}{}", "reason": "r"}],
    [{"rule": "missing-key", "path": "Game.engl.fotxt", "key": "{A}{}", "reason": "r", "extra": "x"}],
    [{"rule": "missing-key", "path": "Game.engl.fotxt", "key": "{A}{}", "reason": "r"}] * 2,
])
def test_malformed_baseline_is_a_usage_error(texts: Path, exemptions: list[dict[str, str]]) -> None:
    write_pack(texts, "Game.engl.fotxt", ["{A}{}{A}"])
    baseline = texts.parent / "bad.json"
    baseline.write_text(json.dumps({"schema_version": 1, "exemptions": exemptions}), encoding="utf-8")

    assert run_check(texts, "--languages", "engl", "--baseline", str(baseline))[0] == 2


def test_languages_are_read_from_the_config_and_sub_config(texts: Path) -> None:
    write_pack(texts, "Game.engl.fotxt", ["{A}{}{A}"])
    write_pack(texts, "Game.russ.fotxt", ["{A}{}{А}"])
    config = texts.parent / "Game.fomain"
    config.write_text("\n".join([
        "Baking.BakeLanguages = engl",
        "[SubConfig]",
        "Name = Localized",
        "Baking.BakeLanguages = engl russ",
        "[SubConfig]",
        "Name = LocalizedTest",
        "Parent = Localized",
        "",
    ]), encoding="utf-8")

    assert run_check(texts, "--config", str(config))[1] == ["unknown-language Game.russ.fotxt"]
    assert run_check(texts, "--config", str(config), "--sub-config", "LocalizedTest") == (0, [])
    assert run_check(texts, "--config", str(config), "--sub-config", "Missing")[0] == 2


@pytest.mark.parametrize("options", [
    ["--languages", "engl", "--script", "deu=latin"],
    ["--languages", "engl", "--script", "engl"],
    ["--languages", "engl russ", "--sparse", "engl"],
    ["--languages", "engl", "--ignore-pattern", "("],
    ["--languages", "engl engl"],
])
def test_invalid_options_are_usage_errors(texts: Path, options: list[str]) -> None:
    write_pack(texts, "Game.engl.fotxt", ["{A}{}{A}"])

    assert run_check(texts, *options)[0] == 2


def test_missing_root_is_a_usage_error(tmp_path: Path) -> None:
    assert ctp.main([str(tmp_path.resolve() / "absent"), "--languages", "engl"]) == 2


def test_sub_config_requires_config(texts: Path) -> None:
    with pytest.raises(SystemExit) as error:
        ctp.main([str(texts), "--languages", "engl", "--sub-config", "X"])

    assert error.value.code == 2


def test_output_is_sorted_and_deterministic(texts: Path, capsys: pytest.CaptureFixture[str]) -> None:
    write_pack(texts, "B.engl.fotxt", ["{X}{}{x}", "{X}{}{x}"])
    write_pack(texts, "A.engl.fotxt", ["{Y}{}{y}", "{Y}{}{y}"])

    assert ctp.main([str(texts), "--languages", "engl", "--base-dir", str(texts)]) == 1
    first = capsys.readouterr().out
    assert ctp.main([str(texts), "--languages", "engl", "--base-dir", str(texts)]) == 1

    assert capsys.readouterr().out == first
    assert first.splitlines()[:2] == ["A.engl.fotxt:2: duplicate-variant: {Y}{} repeats line 1 verbatim", "B.engl.fotxt:2: duplicate-variant: {X}{} repeats line 1 verbatim"]
