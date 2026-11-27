"""Lexical helpers for read-only checks of authored AngelScript (``.fos``) sources.

The helpers classify characters as code, comment or literal, blank everything
that is not code while keeping offsets and line numbers, and recognize
namespace-level function definitions with their attributes and parameters.
They do not preprocess: every ``#if`` branch is scanned.
"""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass
import re


CODE = "c"
LINE_COMMENT = "L"
BLOCK_COMMENT = "B"
STRING = "s"

KEYWORDS = frozenset({
    "and", "break", "case", "cast", "catch", "class", "const", "continue", "default", "do", "else", "enum",
    "for", "foreach", "funcdef", "function", "if", "import", "interface", "is", "mixin", "namespace", "not",
    "null", "or", "return", "switch", "try", "while", "xor",
})

_FUNCTION_RE = re.compile(
    r"(?P<attrs>(?:\[\[[^\[\]\n]*\]\][ \t\r\n]*)*)"
    r"(?P<ret>(?:const[ \t]+)?[A-Za-z_][\w:]*(?:[ \t]*<[^;{}()]*?>)?(?:[ \t]*\[[ \t]*\])*[ \t]*[?@&+]*)"
    r"[ \t\r\n]+(?P<name>[A-Za-z_]\w*)[ \t]*\((?P<params>[^;{}]*?)\)[ \t\r\n]*(?:const[ \t\r\n]*)?\{")
_ATTRIBUTE_RE = re.compile(r"\[\[\s*([A-Za-z_]\w*)")
_NAMESPACE_RE = re.compile(r"\bnamespace\s+([A-Za-z_][\w:]*)\s*\{")
_REFERENCE_SUFFIX_RE = re.compile(r"&\s*(?:in|out|inout)?\s*$")


@dataclass(frozen=True)
class Parameter:
    type: str
    name: str
    nullable: bool


@dataclass(frozen=True)
class ScriptFunction:
    name: str
    namespace: str
    attributes: tuple[str, ...]
    parameters: tuple[Parameter, ...]
    line: int


def classify(text: str) -> str:
    """Return one class character per input character (code, comment or string)."""

    kinds = bytearray(CODE.encode() * len(text))
    length = len(text)
    index = 0

    while index < length:
        if text.startswith("//", index):
            end = text.find("\n", index)
            end = length if end < 0 else end
            kinds[index:end] = LINE_COMMENT.encode() * (end - index)
            index = end
            continue

        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = length if end < 0 else end + 2
            kinds[index:end] = BLOCK_COMMENT.encode() * (end - index)
            index = end
            continue

        if text.startswith('"""', index):
            end = text.find('"""', index + 3)
            end = length if end < 0 else end + 3
            kinds[index:end] = STRING.encode() * (end - index)
            index = end
            continue

        char = text[index]

        if char in ('"', "'"):
            end = _literal_end(text, index, char)
            kinds[index:end] = STRING.encode() * (end - index)
            index = end
            continue

        index += 1

    return kinds.decode("ascii")


def mask(text: str, kinds: str, keep: str = CODE) -> str:
    """Blank every character whose class is not in *keep*, preserving newlines."""

    return "".join(char if kind in keep or char == "\n" else " " for char, kind in zip(text, kinds))


def code_only(text: str) -> str:
    return mask(text, classify(text))


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def split_top_level(text: str, separator: str = ",") -> list[str]:
    """Split on *separator* outside nested ``<>``, ``()`` and ``[]`` pairs."""

    pieces: list[str] = []
    depth = 0
    current: list[str] = []

    for char in text:
        if char in "<([":
            depth += 1
        elif char in ">)]":
            depth = max(depth - 1, 0)

        if char == separator and depth == 0:
            pieces.append("".join(current))
            current = []
            continue

        current.append(char)

    if "".join(current).strip() or pieces:
        pieces.append("".join(current))

    return pieces


def parse_parameters(text: str) -> tuple[Parameter, ...]:
    """Parse an AngelScript parameter list; nullable means a top-level ``?`` handle."""

    result: list[Parameter] = []

    for piece in split_top_level(text):
        declaration = piece.strip()

        if not declaration or declaration == "void":
            continue

        default = _top_level_index(declaration, "=")

        if default >= 0:
            declaration = declaration[:default].rstrip()

        match = re.search(r"(?<![\w:])([A-Za-z_]\w*)$", declaration)
        type_text = declaration
        name = ""

        if match is not None and declaration[:match.start()].strip() and not declaration[:match.start()].rstrip().endswith("::"):
            candidate_type = declaration[:match.start()].rstrip()

            if candidate_type not in ("const",) and match.group(1) not in ("in", "out", "inout"):
                type_text = candidate_type
                name = match.group(1)

        result.append(Parameter(_normalize_space(type_text), name, is_nullable_type(type_text)))

    return tuple(result)


def is_nullable_type(type_text: str) -> bool:
    core = _REFERENCE_SUFFIX_RE.sub("", type_text.strip()).rstrip()
    core = core.rstrip("+").rstrip()

    return core.endswith("?")


def find_functions(code: str) -> list[ScriptFunction]:
    """Return namespace-level function definitions found in code-only text."""

    matches = [match for match in _FUNCTION_RE.finditer(code)
               if match.group("name") not in KEYWORDS and _return_type_is_type(match.group("ret"))]
    scopes = _scope_at(code, [match.start("ret") for match in matches])
    functions: list[ScriptFunction] = []

    for match in matches:
        namespace, namespace_level = scopes[match.start("ret")]

        if not namespace_level:
            continue

        functions.append(ScriptFunction(
            name=match.group("name"),
            namespace=namespace,
            attributes=tuple(_ATTRIBUTE_RE.findall(match.group("attrs"))),
            parameters=parse_parameters(match.group("params")),
            line=line_of(code, match.start("name"))))

    return functions


def namespace_at(code: str, offsets: Iterable[int]) -> dict[int, str]:
    return {offset: scope[0] for offset, scope in _scope_at(code, sorted(set(offsets))).items()}


def _scope_at(code: str, offsets: list[int]) -> dict[int, tuple[str, bool]]:
    """Map each offset to its enclosing namespace path and whether only namespaces enclose it."""

    namespace_opens = {match.end() - 1: match.group(1) for match in _NAMESPACE_RE.finditer(code)}
    wanted = sorted(set(offsets))
    result: dict[int, tuple[str, bool]] = {}
    stack: list[str | None] = []
    position = 0

    for offset in wanted:
        while position < offset and position < len(code):
            char = code[position]

            if char == "{":
                stack.append(namespace_opens.get(position))
            elif char == "}" and stack:
                stack.pop()

            position += 1

        names = [name for name in stack if name is not None]
        result[offset] = ("::".join(names), all(name is not None for name in stack))

    return result


def _return_type_is_type(ret: str) -> bool:
    words = re.findall(r"[A-Za-z_]\w*", ret)

    return bool(words) and not any(word in KEYWORDS - {"const"} for word in words)


def _top_level_index(text: str, target: str) -> int:
    depth = 0

    for index, char in enumerate(text):
        if char in "<([":
            depth += 1
        elif char in ">)]":
            depth = max(depth - 1, 0)
        elif char == target and depth == 0:
            return index

    return -1


def _literal_end(text: str, start: int, quote: str) -> int:
    index = start + 1
    length = len(text)

    while index < length:
        char = text[index]

        if char == "\\":
            index += 2
            continue

        if char == quote:
            return index + 1

        if char == "\n":
            return index

        index += 1

    return length


def _normalize_space(text: str) -> str:
    return " ".join(text.split())
