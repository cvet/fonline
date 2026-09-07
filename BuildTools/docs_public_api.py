from __future__ import annotations

import argparse
import json
import posixpath
import re
import sys
from collections import Counter
from pathlib import Path, PurePosixPath
from typing import Any

import docs_api_diff
import docs_contract_diff
import docs_localization


DEFAULT_OUTPUT = "Docs/en/reference/public-contract/index.md"
RUSSIAN_OUTPUT = "Docs/ru/reference/public-contract/index.md"
LEGACY_OUTPUT = "PUBLIC_API.md"
OUTPUT_PATHS = (DEFAULT_OUTPUT, RUSSIAN_OUTPUT, LEGACY_OUTPUT)
GENERATED_BY = "BuildTools/docs_public_api.py"
SOURCE_MODELS = {
    domain: (Path("Docs/generated") / docs_contract_diff.MODEL_FILES[domain]).as_posix()
    for domain in docs_contract_diff.DOMAIN_ORDER
}
SOURCE_REFERENCES = {
    domain: (Path("Docs/generated") / domain / "index.md").as_posix()
    for domain in docs_contract_diff.DOMAIN_ORDER
}
SOURCE_REFERENCES.update(
    {
        "cmake": "Docs/en/reference/cmake/index.md",
        "cli": "Docs/en/reference/buildtools/index.md",
        "package": "Docs/en/reference/packages/index.md",
        "helper-cli": "Docs/en/reference/helper-cli/index.md",
        "audio": "Docs/en/reference/audio/index.md",
        "video": "Docs/en/reference/video/index.md",
        "gui-runtime": "Docs/en/reference/gui-runtime/index.md",
        "text-format": "Docs/en/reference/text-format/index.md",
        "effect-format": "Docs/en/reference/effect-format/index.md",
        "image-format": "Docs/en/reference/image-format/index.md",
        "particle-format": "Docs/en/reference/particle-format/index.md",
        "font-format": "Docs/en/reference/font-format/index.md",
        "map-format": "Docs/en/reference/map-format/index.md",
        "model-format": "Docs/en/reference/model-format/index.md",
        "prototype-format": "Docs/en/reference/prototype-format/index.md",
        "ai-control-protocol": "Docs/en/reference/ai-control-protocol/index.md",
        "native-extension": "Docs/en/reference/native-extension/index.md",
        "api": "Docs/en/reference/script-api/index.md",
    }
)
DOMAIN_TITLES = {
    "api": "Native script API",
    "cmake": "CMake project interface",
    "cli": "BuildTools CLI",
    "package": "Package interface",
    "helper-cli": "Helper CLIs",
    "native-extension": "Native extensions",
    "prototype-format": "Prototype format",
    "map-format": "Map format",
    "model-format": "Model format",
    "text-format": "Text and localization format",
    "effect-format": "Effect format",
    "image-format": "Image format",
    "particle-format": "Particle format",
    "font-format": "Font format",
    "audio": "Audio",
    "video": "Video",
    "gui-runtime": "GUI runtime",
    "ai-control-protocol": "AiControl protocol",
}
RUSSIAN_DOMAIN_TITLES = {
    "api": "Нативный script API",
    "cmake": "Project interface CMake",
    "cli": "BuildTools CLI",
    "package": "Интерфейс пакетов",
    "helper-cli": "Helper CLI",
    "native-extension": "Нативные расширения",
    "prototype-format": "Формат прототипов",
    "map-format": "Формат карт",
    "model-format": "Формат моделей",
    "text-format": "Формат текста и локализации",
    "effect-format": "Формат эффектов",
    "image-format": "Формат изображений",
    "particle-format": "Формат частиц",
    "font-format": "Формат шрифтов",
    "audio": "Аудио",
    "video": "Видео",
    "gui-runtime": "GUI runtime",
    "ai-control-protocol": "Протокол AiControl",
}


def _load_contracts(root: Path) -> list[dict[str, Any]]:
    contracts: list[dict[str, Any]] = []
    for domain in docs_contract_diff.DOMAIN_ORDER:
        model_path = Path(SOURCE_MODELS[domain])
        index_path = Path(SOURCE_REFERENCES[domain])
        model = docs_contract_diff.load_model(
            domain,
            root / model_path,
            "current",
        )
        if not (root / index_path).is_file():
            raise ValueError(f"Missing generated human reference: {index_path.as_posix()}")

        scope = model["scope"]
        stability = (
            scope.get("default_stability")
            if domain == "api"
            else scope.get("stability")
        )
        if stability not in docs_api_diff.VALID_STABILITIES:
            raise ValueError(
                f"{model_path.as_posix()} has invalid contract stability: {stability}"
            )
        contracts.append(
            {
                "domain": domain,
                "title": DOMAIN_TITLES[domain],
                "surface": scope["surface"],
                "stability": stability,
                "index_path": index_path.as_posix(),
                "model_path": model_path.as_posix(),
                "model": model,
            }
        )
    return contracts


def _relative_target(output_path: str, target: str) -> str:
    return posixpath.relpath(target, PurePosixPath(output_path).parent.as_posix())


def _localized_target(root: Path, target: str, locale: str) -> str:
    if locale == "ru" and target.startswith("Docs/en/"):
        candidate = "Docs/ru/" + target.removeprefix("Docs/en/")
        if (root / candidate).is_file():
            return candidate
    return target


def render_public_api(
    root: Path,
    *,
    locale: str = "en",
    output_path: str = DEFAULT_OUTPUT,
    source_hash: str | None = None,
) -> str:
    if locale not in {"en", "ru"}:
        raise ValueError(f"Unsupported public API locale: {locale}")
    contracts = _load_contracts(root)
    api_summary = next(
        contract["model"]["summary"]
        for contract in contracts
        if contract["domain"] == "api"
    )
    stability_counts = Counter(contract["stability"] for contract in contracts)
    stability_summary = ", ".join(
        f"`{stability}` {stability_counts[stability]}"
        for stability in ("stable", "experimental", "deprecated", "internal")
        if stability_counts[stability]
    )

    is_russian = locale == "ru"
    title = "Индекс публичных контрактов FOnline" if is_russian else "FOnline Public Contract Index"
    lines = [
        "---",
        f"title: {title}",
        "document_id: legacy-public-api-entry",
        f"locale: {locale}",
        "generated: true",
        "---",
        "",
    ]
    if is_russian:
        if source_hash is None:
            raise ValueError("Russian public API rendering requires the English source hash")
        lines.extend(
            [
                docs_localization.translation_metadata_line(
                    "legacy-public-api-entry",
                    DEFAULT_OUTPUT,
                    source_hash,
                ),
                "",
            ]
        )
    lines.extend([
        "<!-- Generated by BuildTools/docs_public_api.py. Do not edit by hand. -->",
        "",
        f"# {title}",
        "",
        (
            "Эта страница является привязанной к ревизии точкой входа в переиспользуемые интерфейсы и форматы данных FOnline Engine. Она генерируется из тех же машиночитаемых моделей, которые используются contract-diff проверками. Доступный символ или принимаемый файл не создаёт обещание совместимости автоматически; авторитетна метка стабильности владеющего контракта."
            if is_russian
            else "This page is the revision-pinned entry point to the reusable interfaces and data formats exposed by the FOnline engine. It is generated from the same machine-readable models used by contract-diff checks. A reachable symbol or accepted file is not automatically a compatibility promise; the stability label on its owning contract is authoritative."
        ),
        "",
        "## Решение о контракте" if is_russian else "## Contract decision",
        "",
        (
            "Разрешайте каждое расхождение строго в четырёхуровневом порядке: 1) владеющий исходник Engine и его metadata задают поведение и стабильность; 2) машинная модель той же ревизии задаёт точный inventory и diff; 3) сгенерированный human reference служит навигацией; 4) handwritten guides дают workflow и контекст, но не переопределяют первые три уровня. Доступный символ сам по себе не равен обещанию совместимости. Этот индекс не определяет написание annotations или механизм pin во встраивающем проекте; не выдумывайте их, если владеющий исходник или проект их не предоставляют."
            if is_russian
            else "Resolve every disagreement in this exact four-level order: (1) owning Engine source and its metadata define behavior and stability; (2) the same-revision machine model defines exact inventory and drives diffs; (3) the generated human reference provides navigation; (4) handwritten guides provide workflow and context but do not override the first three levels. Reachability alone is not a compatibility promise. This index does not define annotation spelling or an embedding project's pin mechanism; do not invent either when the owning source or project does not supply it."
        ),
        "",
        (
            "Native script API является ревизионно привязанным experimental inventory: текущий проверенный inventory-pinned scope классифицирует весь inventory, поэтому не требуйте отдельный tag для каждого symbol. Если scope отсутствует или не проходит проверку, unannotated native symbols remain internal. Фиксируйте exact Engine revision, но если embedding project не документирует хранение pin, не предлагайте dedicated file, CI variable, config field, FO_ENGINE_ROOT, FO_WORKSPACE или другой механизм. При обновлении зафиксируйте exact commit, сравните все сгенерированные домены контрактов и выполните Engine Upgrade Guide до переноса pin."
            if is_russian
            else "The native script API is a revision-pinned experimental inventory: the current validated inventory-pinned scope classifies the complete inventory, so do not require an individual tag on every symbol. If that scope is absent or fails validation, unannotated native symbols remain internal. Pin the exact Engine revision, but when the embedding project does not document pin storage, do not propose a dedicated file, CI variable, config field, `FO_ENGINE_ROOT`, `FO_WORKSPACE`, or another mechanism. On upgrade, record the exact commit, compare all generated contract domains, and follow the Engine Upgrade Guide before moving the pin."
        ),
        "",
        "## Поверхности контрактов" if is_russian else "## Contract surfaces",
        "",
        (
            "| Домен | Поверхность контракта | Текущая стабильность | Справочник для людей | Машинная модель |"
            if is_russian
            else "| Domain | Contract surface | Current stability | Human reference | Machine model |"
        ),
        "| --- | --- | --- | --- | --- |",
    ])
    for contract in contracts:
        reference_path = _localized_target(root, contract["index_path"], locale)
        reference_link = _relative_target(output_path, reference_path)
        model_link = _relative_target(output_path, contract["model_path"])
        domain_title = (
            RUSSIAN_DOMAIN_TITLES[contract["domain"]]
            if is_russian
            else contract["title"]
        )
        lines.append(
            f"| {domain_title} | `{contract['surface']}` | `{contract['stability']}` | "
            f"[{'Справочник' if is_russian else 'Guide'}]({reference_link}) | "
            f"[{Path(contract['model_path']).name}]({model_link}) |"
        )

    lines.extend(
        [
            "",
            (
                f"Текущая ревизия содержит **{len(contracts)}** моделируемых доменов контрактов: {stability_summary}."
                if is_russian
                else f"The current revision contains **{len(contracts)}** modeled contract domains: {stability_summary}."
            ),
            "",
            "## Состояние нативного script API" if is_russian else "## Native script API status",
            "",
            f"- {'Обнаруженные символы' if is_russian else 'Discovered symbols'}: **{api_summary['symbol_count']}**",
            f"- {'Символы с подтверждёнными исходниками описаниями' if is_russian else 'Symbols with source-backed descriptions'}: **{api_summary['described_symbol_count']}**",
            f"- {'Символы без описаний' if is_russian else 'Symbols without descriptions'}: **{api_summary['missing_description_count']}**",
            f"- {'Явно классифицированные символы' if is_russian else 'Explicitly classified symbols'}: **{api_summary['explicit_contract_symbol_count']}**",
            f"- {'Символы с классификацией `internal` по умолчанию' if is_russian else 'Symbols inheriting the default `internal` classification'}: **{api_summary['default_contract_symbol_count']}**",
            "",
            (
                "Сгенерированный нативный справочник является полным inventory моделируемой поверхности code generation. Текущий закреплённый инвентарём scope явно классифицирован как `experimental` и требует точной фиксации ревизии Engine; это не обещание широкой совместимости `stable`. Если scope отсутствует или не проходит проверку, неаннотированные нативные символы остаются `internal`."
                if is_russian
                else "The generated native reference is complete as an inventory of the modeled code-generation surface. Its current inventory-pinned scope is explicitly `experimental` and requires an exact Engine revision pin; it is not a broad `stable` compatibility promise. If the scope is absent or fails validation, unannotated native symbols remain `internal`."
            ),
            "",
            "## Словарь стабильности" if is_russian else "## Stability vocabulary",
            "",
            ("- `stable`: сопровождаемый контракт совместимости; несовместимые изменения требуют документированного процесса миграции." if is_russian else "- `stable`: maintained compatibility contract; incompatible changes require the documented migration process."),
            ("- `experimental`: пригодно при точной фиксации ревизии Engine; совместимость может меняться с документированными примечаниями о миграции." if is_russian else "- `experimental`: usable with an exact Engine revision pin; compatibility may change with documented migration notes."),
            ("- `deprecated`: временно сохраняется для миграции и запланировано к удалению через процесс управления изменениями." if is_russian else "- `deprecated`: retained temporarily for migration and scheduled for removal through the change-management process."),
            ("- `internal`: деталь реализации без обещания downstream-совместимости." if is_russian else "- `internal`: implementation detail with no downstream compatibility promise."),
            "",
            (
                f"Правила классификации описаны в [ADR-0002]({_relative_target(output_path, _localized_target(root, 'Docs/en/contributing/decisions/0002-public-api-stability-contract.md', locale))}), а требования к review, diff, deprecation и миграции — в [Управлении изменениями контрактов]({_relative_target(output_path, _localized_target(root, 'Docs/en/contributing/contract-change-management.md', locale))})."
                if is_russian
                else f"See [ADR-0002]({_relative_target(output_path, 'Docs/en/contributing/decisions/0002-public-api-stability-contract.md')}) for classification rules and [Contract Change Management]({_relative_target(output_path, 'Docs/en/contributing/contract-change-management.md')}) for review, diff, deprecation, and migration requirements."
            ),
            "",
            "## Порядок источников истины" if is_russian else "## Source-of-truth order",
            "",
            ("1. Владеющий исходник Engine и его явная metadata контракта определяют поведение и стабильность." if is_russian else "1. The owning Engine source and its explicit contract metadata define behavior and stability."),
            ("2. Машинная модель фиксирует извлечённый контракт для закреплённой ревизии и управляет автоматическими diff." if is_russian else "2. The machine model records the extracted contract for the pinned revision and drives automated diffs."),
            ("3. Сгенерированный справочник для людей объясняет моделируемую поверхность и направляет к специализированным руководствам." if is_russian else "3. The generated human reference explains the modeled surface and routes to focused manuals."),
            ("4. Написанные вручную руководства дают workflows и примеры, но не расширяют гарантии совместимости молча." if is_russian else "4. Handwritten guides provide workflows and examples but do not silently widen compatibility guarantees."),
            "",
            ("При расхождении слоёв считайте утверждение непроверенным: исправьте metadata исходника или генератор, перегенерируйте затронутую модель и справочник и в том же изменении обновите руководство по миграции." if is_russian else "When these layers disagree, treat the claim as unverified, fix the source metadata or generator, regenerate the affected model and reference, and update migration guidance in the same change."),
            "",
            "## Границы владения проекта" if is_russian else "## Project-owned boundaries",
            "",
            ("Контракт Engine не определяет схему remote calls игры, конкретные прототипы, идентификаторы контента, правила gameplay, топологию deployment, credentials сервисов, политику signing или матрицу приёмки релиза. Подключаемые проекты обязаны документировать и проверять эти границы в собственных репозиториях. Руководства Engine могут использовать проектные свидетельства как ненормативный контекст, но должны оставаться применимыми без этого проекта." if is_russian else "The Engine contract does not define a game's remote-call schema, concrete prototypes, content identifiers, gameplay rules, deployment topology, service credentials, signing policy, or release acceptance matrix. Embedding projects must document and validate those boundaries in their own repositories. Engine manuals may use project evidence as non-normative context, but must remain usable without that project."),
            "",
            "## Фиксация ревизии и обновления" if is_russian else "## Revision pinning and upgrades",
            "",
            (
                f"Фиксируйте Engine точным commit в каждом игровом репозитории и публичном примере. До изменения pin сравните все сгенерированные домены контрактов, проверьте breaking и поведенческие изменения, следуйте [руководству по обновлению Engine]({_relative_target(output_path, _localized_target(root, 'Docs/en/how-to/migration/engine-upgrade.md', locale))}) и повторно запустите build, bake, test, packaging и документационные проверки подключаемого проекта. Квалификация платформ ведётся отдельно в [матрице поддержки]({_relative_target(output_path, _localized_target(root, 'Docs/en/reference/platforms/support-matrix.md', locale))})."
                if is_russian
                else f"Pin the Engine by an exact commit in every game repository and public example. Before moving the pin, compare all generated contract domains, review breaking and behavioral changes, follow the [Engine Upgrade Guide]({_relative_target(output_path, 'Docs/en/how-to/migration/engine-upgrade.md')}), and re-run the embedding project's build, bake, test, packaging, and documentation checks. Platform qualification is tracked separately in the [Support Matrix]({_relative_target(output_path, 'Docs/en/reference/platforms/support-matrix.md')})."
            ),
            "",
        ]
    )
    return "\n".join(lines).rstrip() + "\n"


def _render_legacy_page(canonical_content: str) -> str:
    lines = [
        "# FOnline Public Contract Index",
        "",
        "> Legacy route.",
        "",
        "The generated public contract index moved to locale-specific paths.",
        "",
        f"[English]({DEFAULT_OUTPUT}) | [Russian]({RUSSIAN_OUTPUT})",
        "",
    ]
    for line in canonical_content.splitlines():
        heading = re.fullmatch(r"(#{2,3}) (.+)", line)
        if heading:
            lines.extend(
                [
                    f"{heading.group(1)} {heading.group(2)}",
                    "",
                    f"Continue with the [canonical contract index]({DEFAULT_OUTPUT}).",
                    "",
                ]
            )
        for anchor in re.findall(r'<a id="([^"]+)"></a>', line):
            lines.extend(
                [
                    f'<a id="{anchor}"></a>',
                    f"- [`{anchor}`]({DEFAULT_OUTPUT}#{anchor})",
                    "",
                ]
            )
    return "\n".join(lines).rstrip() + "\n"


def generate_public_api_pages(root: Path) -> dict[str, str]:
    english = render_public_api(root, locale="en", output_path=DEFAULT_OUTPUT)
    russian = render_public_api(
        root,
        locale="ru",
        output_path=RUSSIAN_OUTPUT,
        source_hash=docs_localization.normalized_sha256(english),
    )
    return {
        DEFAULT_OUTPUT: english,
        RUSSIAN_OUTPUT: russian,
        LEGACY_OUTPUT: _render_legacy_page(english),
    }


def _write_or_check(root: Path, output_path: str | None, *, check: bool) -> int:
    rendered_pages = (
        generate_public_api_pages(root)
        if output_path is None
        else {
            output_path: render_public_api(
                root,
                locale="en",
                output_path=output_path,
            )
        }
    )
    if check:
        stale_paths = [
            path
            for path, rendered in rendered_pages.items()
            if not (root / path).is_file()
            or (root / path).read_text(encoding="utf-8") != rendered
        ]
        if stale_paths:
            for path in stale_paths:
                print(f"Public API contract index is stale: {path}", file=sys.stderr)
            return 1
        print(
            f"Public API contract index is current ({len(rendered_pages)} files)"
        )
        return 0

    for path, rendered in rendered_pages.items():
        output = root / path
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(rendered, encoding="utf-8")
    print(f"Wrote {len(rendered_pages)} public API contract pages")
    return 0


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate the cross-domain FOnline public contract index."
    )
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument(
        "--output",
        help="Write or check one custom English output instead of the locale set.",
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    try:
        return _write_or_check(
            args.root.resolve(),
            args.output,
            check=args.check,
        )
    except (OSError, json.JSONDecodeError, ValueError, docs_contract_diff.ContractDiffError) as exception:
        print(f"Public API contract generation failed: {exception}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
