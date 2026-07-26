from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


DEFAULT_OUTPUT = "Docs/generated/source-inventory.json"
SETTING_RE = re.compile(r"^(?:FIXED|VARIABLE)_SETTING\(", re.MULTILINE)


def _relative_paths(root: Path, paths: list[Path]) -> list[str]:
    return [path.relative_to(root).as_posix() for path in sorted(paths)]


def generate_inventory(root: Path) -> dict[str, object]:
    method_paths = sorted((root / "Source/Scripting").glob("*ScriptMethods.cpp"))
    method_files = []
    for path in method_paths:
        declarations = path.read_text(encoding="utf-8").count("///@ ExportMethod")
        method_files.append(
            {
                "path": path.relative_to(root).as_posix(),
                "export_method_declarations": declarations,
            }
        )

    test_paths = list((root / "Source/Tests").glob("Test_*.cpp"))
    settings_path = root / "Source/Common/Settings.inc"
    settings_text = settings_path.read_text(encoding="utf-8")

    return {
        "schema_version": 1,
        "generated_by": "BuildTools/docs_inventory.py",
        "script_api": {
            "export_method_declarations": sum(
                int(method["export_method_declarations"]) for method in method_files
            ),
            "method_file_count": len(method_files),
            "method_files": method_files,
        },
        "engine_tests": {
            "test_file_count": len(test_paths),
            "files": _relative_paths(root, test_paths),
        },
        "settings": {
            "declaration_count": len(SETTING_RE.findall(settings_text)),
            "source": settings_path.relative_to(root).as_posix(),
        },
    }


def render_inventory(root: Path) -> str:
    return json.dumps(generate_inventory(root), indent=2, ensure_ascii=True) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate source-backed documentation inventory")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", default=DEFAULT_OUTPUT)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="write the generated inventory")
    mode.add_argument("--check", action="store_true", help="fail when committed inventory is stale")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    output_path = root / args.output
    generated = render_inventory(root)

    if args.write:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(generated, encoding="utf-8", newline="\n")
        print(f"Wrote {output_path.relative_to(root).as_posix()}")
        return 0

    if not output_path.is_file():
        print(f"Generated documentation inventory is missing: {args.output}", file=sys.stderr)
        return 1
    if output_path.read_text(encoding="utf-8") != generated:
        print(
            f"Generated documentation inventory is stale: run "
            f"python BuildTools/docs_inventory.py --write",
            file=sys.stderr,
        )
        return 1

    inventory = generate_inventory(root)
    print(
        "Documentation source inventory is current: "
        f"{inventory['script_api']['export_method_declarations']} export methods, "
        f"{inventory['engine_tests']['test_file_count']} test files, "
        f"{inventory['settings']['declaration_count']} settings"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
