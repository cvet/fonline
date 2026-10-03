from __future__ import annotations

import argparse
import copy
import json
import re
import sys
from pathlib import Path
from typing import Any

import buildtools
import docs_localization
import docs_description_translations


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/SupportMatrix.json"
DEFAULT_MODEL = "Docs/generated/support-matrix.json"
DEFAULT_INDEX = "Docs/en/reference/platforms/generated-matrix.md"
RUSSIAN_INDEX = "Docs/ru/reference/platforms/generated-matrix.md"
LEGACY_INDEX = "Docs/generated/support-matrix/index.md"
CANONICAL_OUTPUT_PATHS = (DEFAULT_INDEX,)
RUSSIAN_OUTPUT_PATHS = (RUSSIAN_INDEX,)
LEGACY_OUTPUT_PATHS = (LEGACY_INDEX,)
OUTPUT_PATHS = CANONICAL_OUTPUT_PATHS + RUSSIAN_OUTPUT_PATHS + LEGACY_OUTPUT_PATHS
GENERATED_BY = "BuildTools/docs_support_matrix.py"
VALID_LEVELS = {
    "build_gated",
    "smoke_gated",
    "source_capable",
    "not_in_public_matrix",
}


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _string_list(
    value: object, label: str, *, allow_empty: bool = False
) -> list[str]:
    if not isinstance(value, list) or (not value and not allow_empty):
        qualifier = "an" if allow_empty else "a non-empty"
        raise ValueError(f"{label} must be {qualifier} array of strings")
    if any(not isinstance(item, str) or not item.strip() for item in value):
        raise ValueError(f"{label} must contain only non-empty strings")
    if len(value) != len(set(value)):
        raise ValueError(f"{label} must not contain duplicates")
    return list(value)


def _workflow_matrix_targets(workflow: str) -> set[str]:
    stripped_lines = [line.strip() for line in workflow.splitlines()]
    return {
        line.removeprefix("- ").removeprefix("app:").strip()
        for line in stripped_lines
        if line.startswith("- app:") or line.startswith("app:")
    } | {
        line.removeprefix("-").strip()
        for line in stripped_lines
        if line.startswith("- win") and not line.startswith("- windows")
    }


def generate_support_matrix(
    root: Path, manifest_path: str = DEFAULT_MANIFEST
) -> dict[str, Any]:
    raw = json.loads((root / manifest_path).read_text(encoding="utf-8"))
    if raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"support matrix schema_version must be {SCHEMA_VERSION}")
    _required_string(raw.get("title"), "title")
    if raw.get("channel") != "current":
        raise ValueError("support matrix channel must be current")

    policy = raw.get("policy")
    if not isinstance(policy, dict) or set(policy) != VALID_LEVELS:
        raise ValueError(f"policy must define exactly {sorted(VALID_LEVELS)}")
    for level, description in policy.items():
        _required_string(description, f"policy.{level}")

    sources = raw.get("sources")
    if not isinstance(sources, dict):
        raise ValueError("sources must be an object")
    for key in (
        "workflow",
        "validation_registry",
        "platform_configuration",
        "application_targets",
    ):
        source = _required_string(sources.get(key), f"sources.{key}")
        if not (root / source).is_file():
            raise ValueError(f"sources.{key} does not exist: {source}")

    workflow_text = (root / str(sources["workflow"])).read_text(encoding="utf-8")
    workflow_targets = _workflow_matrix_targets(workflow_text)
    validation_targets = set(buildtools.VALIDATION_TARGETS)

    platforms = raw.get("platforms")
    if not isinstance(platforms, list) or not platforms:
        raise ValueError("platforms must be a non-empty array")
    ids: set[str] = set()
    normalized_platforms: list[dict[str, Any]] = []
    all_ci_targets: set[str] = set()
    for index, value in enumerate(platforms):
        label = f"platforms[{index}]"
        if not isinstance(value, dict):
            raise ValueError(f"{label} must be an object")
        entry = copy.deepcopy(value)
        platform_id = _required_string(entry.get("id"), f"{label}.id")
        if platform_id in ids:
            raise ValueError(f"duplicate platform id: {platform_id}")
        ids.add(platform_id)
        for field in (
            "host",
            "target",
            "compiler",
            "runtime_evidence",
            "limitations",
        ):
            _required_string(entry.get(field), f"{label}.{field}")
        level = _required_string(entry.get("level"), f"{label}.level")
        if level not in VALID_LEVELS:
            raise ValueError(f"{label}.level is unsupported: {level}")
        applications = _string_list(entry.get("applications"), f"{label}.applications")
        ci_targets = _string_list(
            entry.get("ci_validation_targets"),
            f"{label}.ci_validation_targets",
            allow_empty=True,
        )
        available_targets = _string_list(
            entry.get("available_validation_targets", []),
            f"{label}.available_validation_targets",
            allow_empty=True,
        )
        for target in ci_targets + available_targets:
            if target not in validation_targets:
                raise ValueError(
                    f"{label} references unknown BuildTools validation target: {target}"
                )
        if level == "source_capable" and ci_targets:
            raise ValueError(f"{label} source_capable entry must not declare CI targets")
        missing_workflow = sorted(set(ci_targets) - workflow_targets)
        if missing_workflow:
            raise ValueError(
                f"{label} CI targets are absent from the required workflow: "
                + ", ".join(missing_workflow)
            )
        if level in {"build_gated", "smoke_gated"} and not ci_targets:
            raise ValueError(f"{label} {level} entry must declare CI targets")
        if level == "smoke_gated" and not any(
            target.endswith(("-starter-smoke", "-tutorial-smoke", "-showcase-runtime"))
            for target in ci_targets
        ):
            raise ValueError(f"{label} smoke_gated entry must declare a smoke target")
        all_ci_targets.update(ci_targets)
        entry["applications"] = applications
        entry["ci_validation_targets"] = ci_targets
        entry["available_validation_targets"] = available_targets
        normalized_platforms.append(entry)

    renderer_policy = raw.get("renderer_policy")
    if not isinstance(renderer_policy, list) or not renderer_policy:
        raise ValueError("renderer_policy must be a non-empty array")
    for index, entry in enumerate(renderer_policy):
        if not isinstance(entry, dict):
            raise ValueError(f"renderer_policy[{index}] must be an object")
        for field in ("platforms", "compiled_backends", "qualification"):
            _required_string(entry.get(field), f"renderer_policy[{index}].{field}")

    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "title": raw["title"],
        "channel": raw["channel"],
        "policy": copy.deepcopy(policy),
        "sources": copy.deepcopy(sources),
        "platforms": normalized_platforms,
        "renderer_policy": copy.deepcopy(renderer_policy),
        "summary": {
            "platform_profile_count": len(normalized_platforms),
            "build_gated_profile_count": sum(
                entry["level"] in {"build_gated", "smoke_gated"}
                for entry in normalized_platforms
            ),
            "smoke_gated_profile_count": sum(
                entry["level"] == "smoke_gated" for entry in normalized_platforms
            ),
            "ci_validation_target_count": len(all_ci_targets),
        },
    }


def render_support_matrix(root: Path, manifest_path: str = DEFAULT_MANIFEST) -> str:
    return (
        json.dumps(
            generate_support_matrix(root, manifest_path),
            ensure_ascii=False,
            indent=2,
        )
        + "\n"
    )


def _code_list(values: list[str]) -> str:
    return ", ".join(f"`{value}`" for value in values) if values else "-"


def generate_reference_page(model: dict[str, Any]) -> str:
    lines = [
        "---",
        "title: Generated Support Matrix",
        "document_id: generated-support-matrix-index",
        "locale: en",
        "generated: true",
        f"generated_by: {GENERATED_BY}",
        "---",
        "",
        "# Generated Support Matrix",
        "",
        "This page is generated from the checked support policy and the live BuildTools/CI target registry.",
        "A build gate proves configuration and compilation; only a smoke gate proves the named process route.",
        "",
        "## Evidence levels",
        "",
        "| Level | Meaning |",
        "|---|---|",
    ]
    for level, description in model["policy"].items():
        lines.append(f"| `{level}` | {description} |")
    lines.extend(
        [
            "",
            "## Platform profiles",
            "",
            "| Host / target | Compiler | Level | Applications | Required validation targets | Runtime evidence | Limitations |",
            "|---|---|---|---|---|---|---|",
        ]
    )
    for entry in model["platforms"]:
        targets = entry["ci_validation_targets"] or entry["available_validation_targets"]
        lines.append(
            f"| {entry['host']} / {entry['target']} | {entry['compiler']} | "
            f"`{entry['level']}` | {'; '.join(entry['applications'])} | "
            f"{_code_list(targets)} | {entry['runtime_evidence']} | {entry['limitations']} |"
        )
    lines.extend(
        [
            "",
            "## Renderer qualification",
            "",
            "| Platforms | Compiled backends | Qualification boundary |",
            "|---|---|---|",
        ]
    )
    for entry in model["renderer_policy"]:
        lines.append(
            f"| {entry['platforms']} | {entry['compiled_backends']} | "
            f"{entry['qualification']} |"
        )
    summary = model["summary"]
    lines.extend(
        [
            "",
            "## Summary",
            "",
            f"- Platform profiles: **{summary['platform_profile_count']}**",
            f"- Build-gated profiles: **{summary['build_gated_profile_count']}**",
            f"- Smoke-gated profiles: **{summary['smoke_gated_profile_count']}**",
            f"- Distinct required CI validation targets: **{summary['ci_validation_target_count']}**",
            "",
            "See [Support Matrix](support-matrix.md) for release interpretation and project acceptance requirements.",
            "",
        ]
    )
    return "\n".join(lines)


RUSSIAN_REPLACEMENTS = {
    "Generated Support Matrix": "Сгенерированная матрица поддержки",
    "This page is generated from the checked support policy and the live BuildTools/CI target registry.":
        "Эта страница создаётся из проверяемой политики поддержки и актуального реестра целей BuildTools/CI.",
    "A build gate proves configuration and compilation; only a smoke gate proves the named process route.":
        "Проверка сборки подтверждает конфигурацию и компиляцию; только smoke-проверка подтверждает указанный маршрут запуска процесса.",
    "## Evidence levels": "## Уровни подтверждения",
    "## Platform profiles": "## Профили платформ",
    "## Renderer qualification": "## Квалификация рендеринга",
    "## Summary": "## Сводка",
    "Configured and compiled by the required validation workflow on every change.":
        "Конфигурируется и компилируется обязательным workflow проверки при каждом изменении.",
    "Build-gated and exercised by an automated process-level starter or multiplayer smoke test.":
        "Проходит проверку сборки и автоматический smoke-тест процесса стартового или многопользовательского проекта.",
    "The checked BuildTools registry exposes a target, but the required workflow does not exercise it.":
        "Проверяемый реестр BuildTools предоставляет цель, но обязательный workflow её не запускает.",
    "No supported validation target is published for this application/platform combination.":
        "Для этой комбинации приложения и платформы не опубликована поддерживаемая цель проверки.",
    "- Platform profiles:": "- Профили платформ:",
    "- Build-gated profiles:": "- Профили с проверкой сборки:",
    "- Smoke-gated profiles:": "- Профили со smoke-проверкой:",
    "- Distinct required CI validation targets:": "- Уникальные обязательные цели проверки CI:",
    "See [Support Matrix](support-matrix.md) for release interpretation and project acceptance requirements.":
        "Правила интерпретации релизов и требования приёмки проекта приведены в [матрице поддержки](support-matrix.md).",
    "| Level | Meaning |": "| Уровень | Значение |",
    "| Host / target | Compiler | Level | Applications | Required validation targets | Runtime evidence | Limitations |":
        "| Хост / цель | Компилятор | Уровень | Приложения | Обязательные цели проверки | Подтверждение runtime | Ограничения |",
    "| Platforms | Compiled backends | Qualification boundary |":
        "| Платформы | Скомпилированные backend | Граница квалификации |",
}


def generate_russian_reference_page(
    english_content: str,
    russian_base_content: str,
) -> str:
    content = russian_base_content.replace("locale: en", "locale: ru", 1)
    for english, russian in sorted(
        RUSSIAN_REPLACEMENTS.items(), key=lambda item: -len(item[0])
    ):
        content = content.replace(english, russian)
    front_matter_end = content.find("\n---\n", 4)
    if front_matter_end < 0:
        raise ValueError("generated support matrix page has no front matter")
    insert_at = front_matter_end + len("\n---\n")
    marker = docs_localization.translation_metadata_line(
        "generated-support-matrix-index",
        DEFAULT_INDEX,
        docs_localization.normalized_sha256(english_content),
    )
    return content[:insert_at] + "\n" + marker + "\n" + content[insert_at:]


def generate_legacy_reference_page(english_content: str) -> str:
    english_path = "../../en/reference/platforms/generated-matrix.md"
    russian_path = "../../ru/reference/platforms/generated-matrix.md"
    lines = [
        "# Generated Support Matrix",
        "",
        "> Legacy route.",
        "",
        "The canonical generated reference moved to locale-specific paths.",
        "",
        f"[English]({english_path}) | [Russian]({russian_path})",
        "",
    ]
    for line in english_content.splitlines():
        heading = re.fullmatch(r"(#{2,3}) (.+)", line)
        if heading:
            lines.extend(
                [
                    f"{heading.group(1)} {heading.group(2)}",
                    "",
                    f"Continue with the [canonical reference]({english_path}).",
                    "",
                ]
            )
    return "\n".join(lines).rstrip() + "\n"


def render_outputs(root: Path) -> dict[str, str]:
    model = generate_support_matrix(root)
    english_content = generate_reference_page(model)
    russian_model = docs_description_translations.apply_translations(
        root,
        "support-matrix",
        model,
    )
    russian_base_content = generate_reference_page(russian_model)
    return {
        DEFAULT_MODEL: json.dumps(model, ensure_ascii=False, indent=2) + "\n",
        DEFAULT_INDEX: english_content,
        RUSSIAN_INDEX: generate_russian_reference_page(
            english_content,
            russian_base_content,
        ),
        LEGACY_INDEX: generate_legacy_reference_page(english_content),
    }


def _write_or_check(root: Path, *, check: bool) -> int:
    stale: list[str] = []
    for relative_path, content in render_outputs(root).items():
        output = root / relative_path
        if check:
            if not output.is_file() or output.read_text(encoding="utf-8") != content:
                stale.append(relative_path)
        else:
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(content, encoding="utf-8")
    if stale:
        print(
            "Support matrix documentation is stale: " + ", ".join(stale),
            file=sys.stderr,
        )
        return 1
    if check:
        print("Support matrix documentation is current")
    return 0


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate the source-backed FOnline support matrix."
    )
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    try:
        return _write_or_check(args.root.resolve(), check=args.check)
    except (OSError, json.JSONDecodeError, ValueError) as exception:
        print(f"Support matrix generation failed: {exception}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
