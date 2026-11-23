from __future__ import annotations

import re
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
ESSENTIALS_DIR = ENGINE_ROOT / "Source" / "Essentials"
ESSENTIALS_HEADER = ESSENTIALS_DIR / "Essentials.h"

MODULE_INCLUDE_PATTERN = re.compile(r'^#include "([A-Za-z0-9]+)\.h"$', re.MULTILINE)
# A declaration is a parameter list closed by a semicolon; a definition opens a body instead
DECLARATION_PATTERN = re.compile(
    r"^[ \t]*(?:\[\[[a-z]+\]\]\s+)?(?!(?:return|throw|else|case)\b)"
    r"[A-Za-z_][\w:<>,*& \t]*?\b([A-Za-z_]\w*)\s*\([^;\n]*\)[^;\n{}=]*;",
    re.MULTILINE,
)
NAMESPACE_PATTERN = re.compile(r"^namespace\s+([a-z_]\w*)\s*$", re.MULTILINE)


def _declared_functions(header_text: str) -> set[str]:
    """Returns each declared function as ns::name, or a bare name outside any namespace."""

    names: set[str] = set()
    namespace: str | None = None
    depth = 0

    for line in header_text.splitlines():
        if namespace is None:
            match = NAMESPACE_PATTERN.match(line)

            if match:
                namespace = match.group(1)
                depth = 0
                continue
        else:
            depth += line.count("{") - line.count("}")

            if depth <= 0 and "}" in line:
                namespace = None
                continue

        # Only a class body is indented outside a namespace, and its members are not namespace-level APIs
        if namespace is None and line[:1].isspace():
            continue

        match = DECLARATION_PATTERN.match(line)

        if match:
            names.add(f"{namespace}::{match.group(1)}" if namespace else match.group(1))

    return names


def _definition_pattern(function_name: str) -> re.Pattern[str]:
    qualified = re.escape(function_name)

    return re.compile(
        rf"^\s*(?:\[\[noreturn\]\]\s+)?[^;\n{{}}]+?(?<![\w:]){qualified}\s*"
        rf"\([^;\n{{}}]*\)[^;\n{{}}]*\n\s*\{{",
        re.MULTILINE,
    )


def _module_order() -> dict[str, int]:
    modules = MODULE_INCLUDE_PATTERN.findall(ESSENTIALS_HEADER.read_text(encoding="utf-8"))
    return {module: index for index, module in enumerate(modules)}


def test_essentials_modules_include_only_their_own_or_earlier_modules() -> None:
    """Headers and implementations must follow the authoritative include cascade."""

    module_order = _module_order()
    violations: list[str] = []

    for module, module_index in module_order.items():
        for suffix in (".h", ".cpp"):
            source_path = ESSENTIALS_DIR / f"{module}{suffix}"

            if not source_path.is_file():
                continue

            dependencies = MODULE_INCLUDE_PATTERN.findall(
                source_path.read_text(encoding="utf-8", errors="replace")
            )

            for dependency in dependencies:
                if (
                    dependency in module_order
                    and dependency != module
                    and module_order[dependency] > module_index
                ):
                    violations.append(f"{source_path.name} -> {dependency}.h")

    assert not violations, "Essentials reverse include dependencies:\n" + "\n".join(sorted(violations))


def test_essentials_public_apis_do_not_depend_on_later_modules() -> None:
    """A forward declaration must not disguise a reverse link-time dependency.

    Essentials.h is the authoritative dependency order. A public API declared by
    an earlier module may be defined by that module or an earlier one, never by a
    module below it in the cascade.
    """

    module_order = _module_order()
    module_sources = {
        path.stem: path.read_text(encoding="utf-8", errors="replace")
        for path in ESSENTIALS_DIR.glob("*.cpp")
        if path.stem in module_order
    }
    violations: list[str] = []

    for header in ESSENTIALS_DIR.glob("*.h"):
        declaring_module = header.stem

        if declaring_module not in module_order:
            continue

        header_text = header.read_text(encoding="utf-8", errors="replace")

        for function_name in _declared_functions(header_text):
            definition_pattern = _definition_pattern(function_name)

            for defining_module, source_text in module_sources.items():
                if definition_pattern.search(source_text) and module_order[defining_module] > module_order[declaring_module]:
                    violations.append(f"{declaring_module}.{function_name} -> {defining_module}")

    assert not violations, "Essentials reverse link dependencies:\n" + "\n".join(sorted(violations))
