from __future__ import annotations

import argparse
import ast
import base64
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ElementTree
from collections import Counter
from pathlib import Path, PurePosixPath


SCHEMA_VERSION = 1
GENERATED_BY = "BuildTools/docs_snippets.py"
DEFAULT_MANIFEST = "Docs/documentation-manifest.json"
DEFAULT_POLICY = "BuildTools/SnippetPolicy.json"
DEFAULT_OUTPUT = "Docs/generated/snippets.json"

HEADING_RE = re.compile(r"^(?P<marks>#{1,6})\s+(?P<title>.+?)\s*#*\s*$")
FENCE_RE = re.compile(r"^(?P<indent> {0,3})(?P<fence>`{3,}|~{3,})[ \t]*(?P<info>[^ \t]*)[ \t]*$")
SECTION_RE = re.compile(r"^\[[^\[\]\r\n]+\]$")
ASSIGNMENT_RE = re.compile(r"^[^=\s][^=]*=")
ANGLE_PLACEHOLDER_RE = re.compile(r"<[^<>\r\n]+>")
ELLIPSIS_RE = re.compile(r"(?<!\.)\.\.\.(?!\.)|…")
CONTROL_RE = re.compile(r"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]")
VALID_ID_RE = re.compile(r"[a-z0-9][a-z0-9-]*")

EXPECTED_LANGUAGES = {
    "angelscript": ("normative", "c-family-parse"),
    "bash": ("normative", "bash-parse"),
    "cmake": ("normative", "cmake-parse"),
    "cpp": ("normative", "c-family-parse"),
    "glsl": ("normative", "c-family-parse"),
    "ini": ("normative", "ini-parse"),
    "json": ("normative", "json-parse"),
    "powershell": ("normative", "powershell-parse"),
    "python": ("normative", "python-parse"),
    "text": ("evidence", "text-contract"),
    "xml": ("normative", "xml-parse"),
}
EXTERNAL_HARNESSES = {"bash-parse", "powershell-parse"}


def _load_json(path: Path, label: str) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exception:
        raise ValueError(f"unable to read {label} {path.as_posix()}: {exception}") from exception
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object")
    return value


def _repository_path(value: object, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{label} must be a non-empty string")
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or "\\" in value:
        raise ValueError(f"{label} must be a repository-relative forward-slash path")
    return value


def _load_policy(path: Path) -> dict[str, dict[str, str]]:
    policy = _load_json(path, "snippet policy")
    if policy.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"snippet policy schema_version must be {SCHEMA_VERSION}")
    if policy.get("scope") != {"visibility": "public", "state": "current", "human": True}:
        raise ValueError("snippet policy scope must be public/current/human")
    languages = policy.get("languages")
    if not isinstance(languages, dict) or set(languages) != set(EXPECTED_LANGUAGES):
        raise ValueError(
            "snippet policy languages must be exactly: "
            + ", ".join(sorted(EXPECTED_LANGUAGES))
        )
    result: dict[str, dict[str, str]] = {}
    for language, expected in EXPECTED_LANGUAGES.items():
        entry = languages.get(language)
        if not isinstance(entry, dict):
            raise ValueError(f"snippet policy language {language} must be an object")
        contract, harness = expected
        if entry != {"contract": contract, "harness": harness}:
            raise ValueError(
                f"snippet policy language {language} must declare "
                f"contract={contract} and harness={harness}"
            )
        result[language] = {"contract": contract, "harness": harness}
    external = policy.get("external_parsers")
    if not isinstance(external, dict) or set(external) != EXTERNAL_HARNESSES:
        raise ValueError(
            "snippet policy external_parsers must declare bash-parse and powershell-parse"
        )
    if any(not isinstance(value, str) or not value for value in external.values()):
        raise ValueError("snippet policy external parser descriptions must be non-empty strings")
    return result


def _heading_slug(title: str) -> str:
    title = re.sub(r"<[^>]+>", "", title).strip().lower()
    title = re.sub(r"[^\w\- ]", "", title, flags=re.UNICODE)
    return re.sub(r"\s+", "-", title)


def _normalized_sha256(text: str) -> str:
    normalized = text.replace("\r\n", "\n").replace("\r", "\n")
    return hashlib.sha256(normalized.encode("utf-8")).hexdigest()


def _scope_documents(
    root: Path, manifest_path: str
) -> list[tuple[str, str, str]]:
    manifest = _load_json(root / manifest_path, "documentation manifest")
    documents = manifest.get("documents")
    if not isinstance(documents, dict):
        raise ValueError("documentation manifest documents must be an object")
    scoped: list[tuple[str, str, str]] = []
    seen_ids: set[str] = set()
    for raw_path, raw_record in documents.items():
        path = _repository_path(raw_path, "documentation document path")
        if not isinstance(raw_record, dict):
            raise ValueError(f"documentation record must be an object: {path}")
        classification = raw_record.get("classification")
        if not isinstance(classification, dict):
            raise ValueError(f"documentation classification must be an object: {path}")
        if (
            raw_record.get("state") != "current"
            or classification.get("visibility") != "public"
            or classification.get("human") is not True
        ):
            continue
        document_id = raw_record.get("id")
        title = raw_record.get("title")
        if (
            not isinstance(document_id, str)
            or not VALID_ID_RE.fullmatch(document_id)
            or document_id in seen_ids
        ):
            raise ValueError(f"invalid or duplicate scoped document id: {document_id}")
        if not isinstance(title, str) or not title:
            raise ValueError(f"scoped document title must be non-empty: {path}")
        if not (root / path).is_file():
            raise ValueError(f"scoped snippet document is missing: {path}")
        seen_ids.add(document_id)
        scoped.append((path, document_id, title))
    return scoped


def _validate_text(body: str) -> None:
    if not body.strip():
        raise ValueError("snippet body must not be empty")
    if CONTROL_RE.search(body):
        raise ValueError("snippet contains a forbidden control character")


def _validate_balanced(body: str) -> None:
    _validate_text(body)
    opening = {"(": ")", "[": "]", "{": "}"}
    closing = {value: key for key, value in opening.items()}
    stack: list[tuple[str, int]] = []
    quote = ""
    escaped = False
    line_comment = False
    block_comment = False
    index = 0
    while index < len(body):
        char = body[index]
        next_char = body[index + 1] if index + 1 < len(body) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
            index += 1
            continue
        if block_comment:
            if char == "*" and next_char == "/":
                block_comment = False
                index += 2
            else:
                index += 1
            continue
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = ""
            index += 1
            continue
        if char == "/" and next_char == "/":
            line_comment = True
            index += 2
            continue
        if char == "/" and next_char == "*":
            block_comment = True
            index += 2
            continue
        if char in {'"', "'"}:
            quote = char
        elif char in opening:
            stack.append((char, index))
        elif char in closing:
            if not stack or stack[-1][0] != closing[char]:
                raise ValueError(f"unmatched closing delimiter {char!r}")
            stack.pop()
        index += 1
    if quote:
        raise ValueError(f"unterminated string delimited by {quote!r}")
    if block_comment:
        raise ValueError("unterminated block comment")
    if stack:
        raise ValueError(f"unclosed delimiter {stack[-1][0]!r}")


def _consume_cmake_call(text: str, start: int) -> int:
    identifier = re.match(r"[A-Za-z_][A-Za-z0-9_]*", text[start:])
    if identifier is None:
        raise ValueError("CMake snippet must contain command invocations")
    index = start + identifier.end()
    while index < len(text) and text[index].isspace():
        index += 1
    if index >= len(text) or text[index] != "(":
        raise ValueError(f"CMake command {identifier.group(0)} is missing '('")
    depth = 0
    quote = False
    escaped = False
    while index < len(text):
        char = text[index]
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quote = False
        elif char == '"':
            quote = True
        elif char == "#":
            newline = text.find("\n", index)
            index = len(text) if newline < 0 else newline
            continue
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return index + 1
            if depth < 0:
                raise ValueError("CMake snippet has an unmatched ')'")
        index += 1
    raise ValueError("CMake command has an unclosed '('")


def _validate_cmake(body: str) -> None:
    _validate_text(body)
    index = 0
    calls = 0
    while index < len(body):
        while index < len(body):
            if body[index].isspace():
                index += 1
            elif body[index] == "#":
                newline = body.find("\n", index)
                index = len(body) if newline < 0 else newline + 1
            else:
                break
        if index >= len(body):
            break
        index = _consume_cmake_call(body, index)
        calls += 1
    if calls == 0:
        raise ValueError("CMake snippet must contain at least one command")


def _validate_ini(body: str) -> None:
    _validate_text(body)
    current_section = ""
    shader_lines: list[str] = []
    continuation = False

    def finish_shader() -> None:
        if shader_lines:
            _validate_balanced("\n".join(shader_lines))
            shader_lines.clear()

    for line_number, line in enumerate(body.splitlines(), start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith(("#", ";")):
            continue
        if SECTION_RE.fullmatch(stripped):
            finish_shader()
            current_section = stripped[1:-1]
            continuation = False
            continue
        if current_section in {"VertexShader", "FragmentShader"}:
            shader_lines.append(line)
            continue
        if continuation:
            continuation = stripped.endswith("\\")
            continue
        if not ASSIGNMENT_RE.match(stripped):
            raise ValueError(f"INI/data line {line_number} is not a section or assignment")
        continuation = stripped.endswith("\\")
    finish_shader()
    if continuation:
        raise ValueError("INI/data snippet ends inside a continuation")


def _validate_json(body: str) -> None:
    _validate_text(body)
    try:
        json.loads(body)
    except json.JSONDecodeError as exception:
        raise ValueError(f"invalid JSON: {exception.msg} at line {exception.lineno}") from exception


def _validate_xml(body: str) -> None:
    _validate_text(body)
    try:
        ElementTree.fromstring(body)
    except ElementTree.ParseError as exception:
        line, column = exception.position
        raise ValueError(
            f"invalid XML: {exception.msg} at line {line}, column {column}"
        ) from exception


def _validate_python(body: str) -> None:
    _validate_text(body)
    try:
        ast.parse(body)
    except SyntaxError as exception:
        raise ValueError(
            f"invalid Python: {exception.msg} at line {exception.lineno}"
        ) from exception


def _validate_static(harness: str, body: str) -> None:
    if harness in {"bash-parse", "powershell-parse", "text-contract"}:
        _validate_text(body)
    elif harness == "c-family-parse":
        _validate_balanced(body)
    elif harness == "cmake-parse":
        _validate_cmake(body)
    elif harness == "ini-parse":
        _validate_ini(body)
    elif harness == "json-parse":
        _validate_json(body)
    elif harness == "xml-parse":
        _validate_xml(body)
    elif harness == "python-parse":
        _validate_python(body)
    else:
        raise ValueError(f"unsupported snippet harness: {harness}")


def _collect_document_snippets(
    path: str,
    document_id: str,
    document_title: str,
    text: str,
    policy: dict[str, dict[str, str]],
) -> tuple[list[dict[str, object]], list[str]]:
    snippets: list[dict[str, object]] = []
    errors: list[str] = []
    lines = text.replace("\r\n", "\n").replace("\r", "\n").splitlines()
    heading = document_title
    anchor = ""
    open_fence = ""
    open_line = 0
    language = ""
    body_lines: list[str] = []
    duplicate_ids: Counter[str] = Counter()

    for line_number, line in enumerate(lines, start=1):
        if not open_fence:
            heading_match = HEADING_RE.match(line)
            if heading_match:
                heading = heading_match.group("title")
                anchor = _heading_slug(heading)
            fence_match = FENCE_RE.match(line)
            if fence_match is None:
                continue
            open_fence = fence_match.group("fence")
            open_line = line_number
            language = fence_match.group("info").lower()
            body_lines = []
            continue
        closing = re.match(
            rf"^ {{0,3}}{re.escape(open_fence[0])}{{{len(open_fence)},}}[ \t]*$",
            line,
        )
        if closing is None:
            body_lines.append(line)
            continue

        body = "\n".join(body_lines) + "\n"
        body_sha256 = _normalized_sha256(body)
        base_id = f"{document_id}-{body_sha256[:12]}"
        duplicate_ids[base_id] += 1
        snippet_id = (
            base_id
            if duplicate_ids[base_id] == 1
            else f"{base_id}-{duplicate_ids[base_id]}"
        )
        entry: dict[str, object] = {
            "id": snippet_id,
            "document_id": document_id,
            "path": path,
            "heading": heading,
            "anchor": anchor,
            "start_line": open_line,
            "end_line": line_number,
            "language": language,
            "contract": "unknown",
            "harness": "unknown",
            "template": bool(
                ELLIPSIS_RE.search(body)
                or (
                    language in {"bash", "powershell", "cmake", "text"}
                    and ANGLE_PLACEHOLDER_RE.search(body)
                )
            ),
            "line_count": len(body_lines),
            "byte_count": len(body.encode("utf-8")),
            "sha256": body_sha256,
            "status": "failed",
        }
        if not language:
            errors.append(f"{path}:{open_line}: fenced snippet must declare a language")
        elif language not in policy:
            errors.append(f"{path}:{open_line}: unsupported snippet language: {language}")
        else:
            entry["contract"] = policy[language]["contract"]
            entry["harness"] = policy[language]["harness"]
            try:
                _validate_static(str(entry["harness"]), body)
            except ValueError as exception:
                errors.append(
                    f"{path}:{open_line}: {entry['harness']} failed: {exception}"
                )
            else:
                entry["status"] = "passed"
        entry["_body"] = body
        snippets.append(entry)
        open_fence = ""
        open_line = 0
        language = ""
        body_lines = []

    if open_fence:
        errors.append(f"{path}:{open_line}: fenced snippet is not closed")
    return snippets, errors


def _collect(
    root: Path,
    manifest_path: str,
    policy_path: str,
) -> tuple[dict[str, object], list[dict[str, object]]]:
    root = root.resolve()
    policy = _load_policy(root / policy_path)
    documents = _scope_documents(root, manifest_path)
    snippets: list[dict[str, object]] = []
    errors: list[str] = []
    corpus_hash = hashlib.sha256()
    for metadata_path in (manifest_path, policy_path):
        metadata_text = (root / metadata_path).read_text(encoding="utf-8")
        corpus_hash.update(
            metadata_text.replace("\r\n", "\n").replace("\r", "\n").encode("utf-8")
        )
    for path, document_id, title in documents:
        text = (root / path).read_text(encoding="utf-8")
        corpus_hash.update(path.encode("utf-8"))
        corpus_hash.update(text.replace("\r\n", "\n").replace("\r", "\n").encode("utf-8"))
        document_snippets, document_errors = _collect_document_snippets(
            path,
            document_id,
            title,
            text,
            policy,
        )
        snippets.extend(document_snippets)
        errors.extend(document_errors)

    language_counts = Counter(str(entry["language"]) for entry in snippets)
    harness_counts = Counter(str(entry["harness"]) for entry in snippets)
    contract_counts = Counter(str(entry["contract"]) for entry in snippets)
    normative = [entry for entry in snippets if entry["contract"] == "normative"]
    normative_passed = [entry for entry in normative if entry["status"] == "passed"]
    passed = [entry for entry in snippets if entry["status"] == "passed"]
    external = [
        entry for entry in snippets if entry["harness"] in EXTERNAL_HARNESSES
    ]
    public_entries = [
        {key: value for key, value in entry.items() if key != "_body"}
        for entry in snippets
    ]
    report: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "manifest_path": manifest_path,
        "policy_path": policy_path,
        "corpus_sha256": corpus_hash.hexdigest(),
        "document_count": len(documents),
        "snippet_count": len(snippets),
        "validated_count": len(passed),
        "normative_count": len(normative),
        "normative_validated_count": len(normative_passed),
        "normative_coverage": (
            round(len(normative_passed) / len(normative), 6) if normative else 1.0
        ),
        "evidence_count": contract_counts.get("evidence", 0),
        "template_count": sum(bool(entry["template"]) for entry in snippets),
        "external_parser_required_count": len(external),
        "language_counts": dict(sorted(language_counts.items())),
        "harness_counts": dict(sorted(harness_counts.items())),
        "error_count": len(errors),
        "errors": errors,
        "snippets": public_entries,
    }
    return report, snippets


def evaluate(
    root: Path,
    manifest_path: str = DEFAULT_MANIFEST,
    policy_path: str = DEFAULT_POLICY,
) -> dict[str, object]:
    report, _ = _collect(root, manifest_path, policy_path)
    return report


def _external_source(body: str) -> str:
    body = ANGLE_PLACEHOLDER_RE.sub("placeholder", body)
    return ELLIPSIS_RE.sub("placeholder", body)


def _find_bash() -> str:
    configured = os.environ.get("DOCS_BASH")
    candidates = [
        configured,
        shutil.which("bash"),
        r"C:\Program Files\Git\bin\bash.exe",
        r"C:\Program Files\Git\usr\bin\bash.exe",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return str(candidate)
    raise ValueError("bash parser is unavailable; install Bash or set DOCS_BASH")


def _find_powershell() -> str:
    configured = os.environ.get("DOCS_POWERSHELL")
    candidates = [configured, shutil.which("pwsh"), shutil.which("powershell")]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return str(candidate)
    raise ValueError(
        "PowerShell parser is unavailable; install pwsh/PowerShell or set DOCS_POWERSHELL"
    )


def _run_external(snippets: list[dict[str, object]]) -> list[str]:
    errors: list[str] = []
    bash = _find_bash()
    powershell = _find_powershell()
    for entry in snippets:
        harness = entry["harness"]
        if harness not in EXTERNAL_HARNESSES:
            continue
        body = _external_source(str(entry["_body"]))
        if harness == "bash-parse":
            result = subprocess.run(
                [bash, "-n"],
                # Send bytes so Windows does not translate normalized LF back
                # to CRLF while writing the parser's stdin.
                input=body.encode("utf-8"),
                capture_output=True,
                check=False,
            )
        else:
            encoded = base64.b64encode(body.encode("utf-8")).decode("ascii")
            parser_script = (
                "$source=[Text.Encoding]::UTF8.GetString("
                f"[Convert]::FromBase64String('{encoded}'));"
                "$tokens=$null;$parseErrors=$null;"
                "[System.Management.Automation.Language.Parser]::ParseInput("
                "$source,[ref]$tokens,[ref]$parseErrors)|Out-Null;"
                "if($parseErrors.Count -gt 0){"
                "$parseErrors|ForEach-Object{[Console]::Error.WriteLine($_.Message)};"
                "exit 1}"
            )
            result = subprocess.run(
                [powershell, "-NoProfile", "-NonInteractive", "-Command", parser_script],
                text=True,
                encoding="utf-8",
                capture_output=True,
                check=False,
            )
        if result.returncode != 0:
            detail_output = result.stderr or result.stdout
            if isinstance(detail_output, bytes):
                detail_output = detail_output.decode("utf-8", errors="replace")
            detail = detail_output.strip().replace("\n", " | ")
            errors.append(
                f"{entry['path']}:{entry['start_line']}: "
                f"{harness} external parser failed: {detail}"
            )
    return errors


def _render(report: dict[str, object]) -> str:
    return json.dumps(report, indent=2, ensure_ascii=True) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate documentation snippets")
    parser.add_argument("--root", default=".")
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    parser.add_argument("--policy", default=DEFAULT_POLICY)
    parser.add_argument("--output", default=DEFAULT_OUTPUT)
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--external", action="store_true")
    args = parser.parse_args(argv)
    if args.write and args.check:
        parser.error("--write and --check are mutually exclusive")

    root = Path(args.root).resolve()
    try:
        report, snippets = _collect(root, args.manifest, args.policy)
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError) as exception:
        print(f"Unable to validate documentation snippets: {exception}", file=sys.stderr)
        return 1
    errors = list(report["errors"])
    if args.external:
        try:
            errors.extend(_run_external(snippets))
        except ValueError as exception:
            errors.append(str(exception))
    content = _render(report)
    output_path = root / args.output
    if args.write:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote {args.output}")
    if args.check:
        if not output_path.is_file() or output_path.read_text(encoding="utf-8") != content:
            errors.append(
                f"snippet report is missing or stale: {args.output}; "
                "run python BuildTools/docs_snippets.py --write"
            )
    if errors:
        for error in errors[:50]:
            print(f"ERROR: {error}", file=sys.stderr)
        if len(errors) > 50:
            print(f"ERROR: {len(errors) - 50} additional errors omitted", file=sys.stderr)
        print(
            f"Documentation snippet validation failed with {len(errors)} error(s)",
            file=sys.stderr,
        )
        return 1
    print(
        "Documentation snippets passed: "
        f"{report['normative_validated_count']}/{report['normative_count']} normative, "
        f"{report['evidence_count']} evidence, "
        f"{report['external_parser_required_count']} external-parser checks"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
