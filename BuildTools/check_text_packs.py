"""Lint raw ``.fotxt`` text packs before baking.

The checks follow the Engine text-pack grammar (``TextPack::LoadFromString``), file naming
(``TextBaker``) and language normalization (``TextPack::FixPacks``) for an ordered
``Baking.BakeLanguages`` declaration:

  invalid-file-name        the name is not ``<TextPack>.<Language>.fotxt``; baking fails
  invalid-utf8             the file is not UTF-8 text
  parse-error              an entry the Engine parser rejects; baking fails
  unknown-language         the language is not declared; baking warns and skips the file
  duplicate-pack-file      a second file of one pack and language in one root; baking ignores one
  duplicate-variant        the same text twice under one complete key; one copy is never told apart
  wrong-script             letters of a script the language does not allow (``--script``)
  orphan-pack              a pack its fallback language does not have; baking drops it
  extra-key                a key its fallback language does not have; baking drops it
  missing-pack             a pack the language lacks; baking copies the fallback pack (``--sparse`` allows it)
  missing-key              a key the language lacks; baking copies the fallback text (``--sparse`` allows it)
  variant-count-mismatch   a different number of variants than the fallback (``--strict-variant-counts``)

Known findings are accepted through ``--baseline`` files whose every exemption carries a reason; an
exemption that no longer matches a finding fails the run, so a baseline only shrinks.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import unicodedata
from collections.abc import Iterable, Sequence
from dataclasses import dataclass, field
from pathlib import Path

import foconfig


TEXT_PACK_SUFFIX = ".fotxt"
BASELINE_SCHEMA_VERSION = 1
ENGINE_DEFAULT_LANGUAGES = "engl"
# Inline color tags are parsed by the client font renderer, so their hex digits are not authored letters
COLOR_TAG_PATTERN = r"@color(?::(?:0x)?(?:[0-9A-Fa-f]{8}|[0-9A-Fa-f]{6}))?@"
# Unicode names that qualify a script word instead of naming the script
SCRIPT_NAME_QUALIFIERS = frozenset({"FULLWIDTH", "HALFWIDTH"})
SCRIPT_NAME_ALIASES = {"CJK": "han"}
RULES = (
    "invalid-file-name",
    "invalid-utf8",
    "parse-error",
    "unknown-language",
    "duplicate-pack-file",
    "duplicate-variant",
    "wrong-script",
    "orphan-pack",
    "extra-key",
    "missing-pack",
    "missing-key",
    "variant-count-mismatch",
    "stale-exemption",
)


class UsageError(Exception):
    pass


@dataclass(frozen=True)
class Entry:
    key1: str
    key2: str
    text: str
    line: int

    @property
    def key(self) -> str:
        return f"{{{self.key1}}}{{{self.key2}}}"


@dataclass(frozen=True)
class ParseError:
    line: int
    message: str


@dataclass
class ParsedPack:
    entries: list[Entry] = field(default_factory=list)
    errors: list[ParseError] = field(default_factory=list)

    def keys(self) -> dict[str, list[Entry]]:
        result: dict[str, list[Entry]] = {}

        for entry in self.entries:
            result.setdefault(entry.key, []).append(entry)

        return result


@dataclass(frozen=True)
class LanguageConfig:
    languages: tuple[str, ...]
    fallbacks: dict[str, str]

    @property
    def base(self) -> str:
        return self.languages[0]

    def fallback(self, language: str) -> str:
        return self.fallbacks.get(language, self.base)


@dataclass(frozen=True, order=True)
class Finding:
    path: str
    line: int
    rule: str
    key: str
    detail: str

    def __str__(self) -> str:
        key = f" {self.key}" if self.key else ""
        return f"{self.path}:{self.line}: {self.rule}:{key} {self.detail}"


@dataclass(frozen=True)
class Exemption:
    rule: str
    path: str
    key: str
    reason: str
    source: str


@dataclass
class PackFile:
    path: Path
    display: str
    pack: str
    language: str
    parsed: ParsedPack


def _extract_brace_token(lines: list[str], index: int, line: str, offset: int, multiline: bool) -> tuple[str, int, str, int] | None:
    """Port of ExtractBraceToken: the token, the next offset, the possibly extended line and the next line index."""
    first = line.find("{", offset)

    if first < 0:
        return None

    last = line.find("}", first)

    while last < 0 and multiline and index < len(lines):
        line = f"{line}\n{lines[index]}"
        index += 1
        last = line.find("}", first)

    if last < 0:
        return None

    return line[first + 1:last], last + 1, line, index


def parse_text_pack(text: str) -> ParsedPack:
    """Read raw .fotxt text as TextPack::LoadFromString does, recording the lines that fail baking."""
    lines = text.split("\n")

    # getline yields no final empty line after a trailing newline
    if lines and lines[-1] == "":
        lines.pop()

    result = ParsedPack()
    index = 0

    while index < len(lines):
        line_no = index + 1
        line = lines[index]
        index += 1

        token1 = _extract_brace_token(lines, index, line, 0, False)

        if token1 is None:
            continue

        key1, offset, line, index = token1
        token2 = _extract_brace_token(lines, index, line, offset, False)

        if token2 is None:
            result.errors.append(ParseError(line_no, "entry has no second {Key2} field"))
            continue

        key2, offset, line, index = token2
        token3 = _extract_brace_token(lines, index, line, offset, True)

        if token3 is None:
            # The unterminated value has consumed every remaining line, as the Engine stream read does
            result.errors.append(ParseError(line_no, "entry has no closed third {Text} field"))
            break

        value, _, _, index = token3

        if not key1:
            result.errors.append(ParseError(line_no, "entry has an empty Key1"))
            continue

        result.entries.append(Entry(key1, key2, value, line_no))

    return result


def parse_bake_languages(declarations: Sequence[str]) -> LanguageConfig:
    """Port of TextPack::ParseBakeLanguages."""
    if not declarations:
        raise UsageError("Baking.BakeLanguages must contain at least one language declaration")

    languages: list[str] = []
    fallbacks: dict[str, str] = {}

    for declaration in declarations:
        parts = declaration.split(":")

        if not declaration or len(parts) > 2 or (len(parts) == 2 and (not parts[0] or not parts[1])):
            raise UsageError(f"bake language declaration must use language or language:parent format: {declaration}")

        language = parts[0]

        if language in languages:
            raise UsageError(f"bake language is declared more than once: {language}")

        if len(parts) == 2:
            if parts[1] not in languages:
                raise UsageError(f"bake language fallback parent must be declared before its child: {declaration}")

            fallbacks[language] = parts[1]

        languages.append(language)

    return LanguageConfig(tuple(languages), fallbacks)


def load_bake_languages(config_path: Path, sub_config: str | None) -> LanguageConfig:
    parser = foconfig.ConfigParser()

    try:
        parser.loadFromFile(config_path)
    except (OSError, UnicodeDecodeError) as exception:
        raise UsageError(f"cannot read config {config_path}: {exception}") from exception

    try:
        settings = parser.resolveSettings(sub_config)
    except KeyError as exception:
        raise UsageError(str(exception.args[0])) from exception

    value = settings.get("Baking.BakeLanguages", ENGINE_DEFAULT_LANGUAGES)

    if '"' in value or "$" in value:
        raise UsageError(f"Baking.BakeLanguages uses quoting or substitution, pass --languages instead: {value}")

    return parse_bake_languages(value.split())


def letter_script(char: str) -> str | None:
    """The script of a letter as the first word of its Unicode name, or None for a non-letter."""
    if not unicodedata.category(char).startswith("L"):
        return None

    words = unicodedata.name(char, "UNKNOWN").split()

    if len(words) > 1 and words[0] in SCRIPT_NAME_QUALIFIERS:
        words = words[1:]

    return SCRIPT_NAME_ALIASES.get(words[0], words[0].lower())


def foreign_letters(text: str, allowed: frozenset[str], ignore: Sequence[re.Pattern[str]]) -> dict[str, list[str]]:
    for pattern in ignore:
        text = pattern.sub(" ", text)

    found: dict[str, list[str]] = {}

    for char in text:
        script = letter_script(char)

        if script is not None and script not in allowed:
            letters = found.setdefault(script, [])

            if char not in letters:
                letters.append(char)

    return found


def display_path(path: Path, base_dir: Path) -> str:
    try:
        return path.relative_to(base_dir).as_posix()
    except ValueError:
        return path.as_posix()


def collect_pack_files(root: Path, base_dir: Path) -> tuple[list[PackFile], list[Finding]]:
    files: list[PackFile] = []
    findings: list[Finding] = []

    for path in sorted(item for item in root.rglob("*") if item.is_file() and item.suffix.lower() == TEXT_PACK_SUFFIX):
        display = display_path(path, base_dir)
        segments = path.name[:-len(TEXT_PACK_SUFFIX)].split(".")

        if len(segments) != 2 or not all(segments):
            findings.append(Finding(display, 0, "invalid-file-name", "", "name must be <TextPack>.<Language>.fotxt"))
            continue

        try:
            text = path.read_bytes().decode("utf-8")
        except UnicodeDecodeError as exception:
            findings.append(Finding(display, 0, "invalid-utf8", "", f"byte {exception.start} is not UTF-8"))
            continue

        files.append(PackFile(path, display, segments[0], segments[1], parse_text_pack(text)))

    return files, findings


def check_file(pack_file: PackFile, scripts: dict[str, frozenset[str]], ignore: Sequence[re.Pattern[str]]) -> list[Finding]:
    findings = [Finding(pack_file.display, error.line, "parse-error", "", error.message) for error in pack_file.parsed.errors]
    allowed = scripts.get(pack_file.language)

    for key, entries in pack_file.parsed.keys().items():
        seen: dict[str, int] = {}

        for entry in entries:
            if entry.text in seen:
                findings.append(Finding(pack_file.display, entry.line, "duplicate-variant", key, f"repeats line {seen[entry.text]} verbatim"))
            else:
                seen[entry.text] = entry.line

            if allowed is None:
                continue

            for script, letters in sorted(foreign_letters(entry.text, allowed, ignore).items()):
                sample = "".join(letters[:8])
                findings.append(Finding(pack_file.display, entry.line, "wrong-script", key,
                                        f"{script} letters in a {pack_file.language} pack (allowed: {', '.join(sorted(allowed))}): {sample}"))

    return findings


def check_root(files: list[PackFile], config: LanguageConfig, sparse: frozenset[str], strict_counts: bool) -> list[Finding]:
    findings: list[Finding] = []
    by_language: dict[str, dict[str, PackFile]] = {language: {} for language in config.languages}

    for pack_file in files:
        if pack_file.language not in by_language:
            findings.append(Finding(pack_file.display, 0, "unknown-language", "", f"language {pack_file.language} is not in Baking.BakeLanguages ({' '.join(config.languages)})"))
            continue

        packs = by_language[pack_file.language]
        kept = packs.get(pack_file.pack)

        if kept is not None:
            findings.append(Finding(pack_file.display, 0, "duplicate-pack-file", "", f"pack {pack_file.pack} is already authored in {kept.display}"))
            continue

        packs[pack_file.pack] = pack_file

    # Normalized variant counts per language, pack and key, simulated in declaration order like FixPacks:
    # every language ends with exactly its fallback's packs and keys
    normalized: dict[str, dict[str, dict[str, int]]] = {}
    base_packs = by_language[config.base]
    normalized[config.base] = {pack: {key: len(entries) for key, entries in pack_file.parsed.keys().items()} for pack, pack_file in base_packs.items()}

    for language in config.languages[1:]:
        fallback = config.fallback(language)
        fallback_packs = normalized[fallback]
        own_packs = by_language[language]
        result: dict[str, dict[str, int]] = {}

        for pack, pack_file in sorted(own_packs.items()):
            if pack not in fallback_packs:
                findings.append(Finding(pack_file.display, 0, "orphan-pack", "", f"pack {pack} is absent from fallback language {fallback}; baking drops it"))

        for pack, fallback_keys in sorted(fallback_packs.items()):
            pack_file = own_packs.get(pack)

            if pack_file is None:
                if language not in sparse:
                    expected = _sibling_display(by_language[fallback].get(pack), base_packs.get(pack), pack, language)
                    findings.append(Finding(expected, 0, "missing-pack", "", f"no {language} file for pack {pack}; baking copies {fallback}"))

                result[pack] = dict(fallback_keys)
                continue

            own_keys = pack_file.parsed.keys()

            for key, entries in sorted(own_keys.items()):
                if key not in fallback_keys:
                    findings.append(Finding(pack_file.display, entries[0].line, "extra-key", key, f"absent from fallback language {fallback}; baking drops it"))
                elif strict_counts and len(entries) != fallback_keys[key]:
                    findings.append(Finding(pack_file.display, entries[0].line, "variant-count-mismatch", key, f"{len(entries)} variant(s), fallback language {fallback} has {fallback_keys[key]}"))

            if language not in sparse:
                for key in sorted(key for key in fallback_keys if key not in own_keys):
                    findings.append(Finding(pack_file.display, 0, "missing-key", key, f"absent; baking copies the {fallback} text"))

            result[pack] = {key: len(own_keys[key]) if key in own_keys else count for key, count in fallback_keys.items()}

        normalized[language] = result

    return findings


def _sibling_display(fallback_file: PackFile | None, base_file: PackFile | None, pack: str, language: str) -> str:
    # A missing pack is reported at the path it would have beside its nearest authored fallback
    source = fallback_file or base_file

    if source is None:
        return f"{pack}.{language}{TEXT_PACK_SUFFIX}"

    return str(Path(source.display).with_name(f"{pack}.{language}{TEXT_PACK_SUFFIX}").as_posix())


def load_baselines(paths: Iterable[Path], base_dir: Path) -> list[Exemption]:
    exemptions: list[Exemption] = []
    identities: set[tuple[str, str, str]] = set()

    for path in paths:
        source = display_path(path.resolve(), base_dir)

        try:
            raw = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exception:
            raise UsageError(f"cannot read baseline {source}: {exception}") from exception

        if not isinstance(raw, dict) or set(raw) != {"schema_version", "exemptions"} or raw["schema_version"] != BASELINE_SCHEMA_VERSION:
            raise UsageError(f"baseline {source} must be an object with schema_version {BASELINE_SCHEMA_VERSION} and exemptions")
        if not isinstance(raw["exemptions"], list):
            raise UsageError(f"baseline {source}: exemptions must be an array")

        for index, item in enumerate(raw["exemptions"]):
            label = f"baseline {source} exemption {index}"

            if not isinstance(item, dict) or set(item) != {"rule", "path", "key", "reason"} or not all(isinstance(value, str) for value in item.values()):
                raise UsageError(f"{label} must hold exactly the string fields rule, path, key and reason")
            if item["rule"] not in RULES or item["rule"] == "stale-exemption":
                raise UsageError(f"{label} names an unknown rule: {item['rule']}")
            if not item["path"] or not item["reason"].strip():
                raise UsageError(f"{label} needs a path and a reason")

            identity = (item["rule"], item["path"], item["key"])

            if identity in identities:
                raise UsageError(f"{label} duplicates an earlier exemption: {' '.join(identity)}")

            identities.add(identity)
            exemptions.append(Exemption(item["rule"], item["path"], item["key"], item["reason"], source))

    return exemptions


def apply_baselines(findings: list[Finding], exemptions: list[Exemption]) -> tuple[list[Finding], int]:
    by_identity = {(exemption.rule, exemption.path, exemption.key): exemption for exemption in exemptions}
    used: set[tuple[str, str, str]] = set()
    remaining: list[Finding] = []

    for finding in findings:
        identity = (finding.rule, finding.path, finding.key)

        if identity in by_identity:
            used.add(identity)
        else:
            remaining.append(finding)

    exempted = len(findings) - len(remaining)

    for identity, exemption in sorted(by_identity.items()):
        if identity not in used:
            remaining.append(Finding(exemption.source, 0, "stale-exemption", exemption.key, f"{exemption.rule} {exemption.path} no longer occurs; delete the exemption"))

    return sorted(remaining), exempted


def parse_script_options(values: Sequence[str], config: LanguageConfig) -> dict[str, frozenset[str]]:
    scripts: dict[str, frozenset[str]] = {}

    for value in values:
        language, separator, names = value.partition("=")
        allowed = frozenset(name.strip().lower() for name in names.split(",") if name.strip())

        if not separator or not language or not allowed:
            raise UsageError(f"--script must be LANGUAGE=SCRIPT[,SCRIPT...]: {value}")
        if language not in config.languages:
            raise UsageError(f"--script names a language outside Baking.BakeLanguages: {language}")
        if language in scripts:
            raise UsageError(f"--script repeats language {language}")

        scripts[language] = allowed

    return scripts


def run(args: argparse.Namespace) -> int:
    base_dir = (args.base_dir or Path.cwd()).resolve()
    config = parse_bake_languages(args.languages.split()) if args.languages is not None else load_bake_languages(args.config, args.sub_config)
    scripts = parse_script_options(args.script, config)
    sparse = frozenset(args.sparse)

    for language in sorted(sparse):
        if language not in config.languages[1:]:
            raise UsageError(f"--sparse must name a non-base language of Baking.BakeLanguages: {language}")

    try:
        ignore = [re.compile(COLOR_TAG_PATTERN), *(re.compile(pattern) for pattern in args.ignore_pattern)]
    except re.error as exception:
        raise UsageError(f"invalid --ignore-pattern: {exception}") from exception

    exemptions = load_baselines(args.baseline, base_dir)
    findings: list[Finding] = []
    file_count = 0
    entry_count = 0

    for root in args.roots:
        root_path = root.resolve()

        if not root_path.is_dir():
            raise UsageError(f"text root is not a directory: {root}")

        files, root_findings = collect_pack_files(root_path, base_dir)
        findings.extend(root_findings)
        file_count += len(files) + len(root_findings)
        entry_count += sum(len(pack_file.parsed.entries) for pack_file in files)

        for pack_file in files:
            findings.extend(check_file(pack_file, scripts, ignore))

        findings.extend(check_root(files, config, sparse, args.strict_variant_counts))

    remaining, exempted = apply_baselines(sorted(set(findings)), exemptions)

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        payload = {"files": file_count, "entries": entry_count, "exempted": exempted,
                   "findings": [{"rule": item.rule, "path": item.path, "line": item.line, "key": item.key, "detail": item.detail} for item in remaining]}
        args.json.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    for finding in remaining:
        print(finding)

    if remaining:
        print(f"Text pack check failed: {len(remaining)} finding(s), {exempted} exempted, {file_count} file(s)")
        return 1

    print(f"Text pack check passed: {file_count} file(s), {entry_count} entries, {exempted} exempted")
    return 0


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="check_text_packs.py", description="Lint raw .fotxt text packs against the Engine grammar and language normalization.")
    parser.add_argument("roots", nargs="+", type=Path, help="text directories to scan recursively; each root is checked as one resource-pack input")
    languages = parser.add_mutually_exclusive_group(required=True)
    languages.add_argument("--languages", help="ordered Baking.BakeLanguages declarations, such as 'engl russ'")
    languages.add_argument("--config", type=Path, help="main .fomain config whose Baking.BakeLanguages is read")
    parser.add_argument("--sub-config", help="sub-config applied over the main config section (with --config)")
    parser.add_argument("--script", action="append", default=[], metavar="LANGUAGE=SCRIPTS", help="letter scripts a language allows, such as engl=latin (repeatable)")
    parser.add_argument("--ignore-pattern", action="append", default=[], metavar="REGEX", help="text removed before the script check, such as project placeholders (repeatable)")
    parser.add_argument("--sparse", action="append", default=[], metavar="LANGUAGE", help="language that intentionally inherits missing packs and keys from its fallback (repeatable)")
    parser.add_argument("--strict-variant-counts", action="store_true", help="also report keys whose variant count differs from the fallback language")
    parser.add_argument("--baseline", action="append", default=[], type=Path, help="JSON exemption file with a reason per known finding (repeatable)")
    parser.add_argument("--base-dir", type=Path, help="directory that finding and exemption paths are relative to (default: current directory)")
    parser.add_argument("--json", type=Path, help="also write the remaining findings as JSON")
    return parser


def main(argv: list[str] | None = None) -> int:
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)

        if reconfigure is not None:
            reconfigure(encoding="utf-8", errors="replace")

    parser = create_parser()
    args = parser.parse_args(argv)

    if args.sub_config is not None and args.config is None:
        parser.error("--sub-config requires --config")

    try:
        return run(args)
    except UsageError as exception:
        print(f"error: {exception}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
