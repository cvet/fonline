from __future__ import annotations

import argparse
import configparser
import copy
import fnmatch
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import date
from pathlib import Path, PurePosixPath
from typing import Any

import docs_localization
import docs_description_translations


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "Examples/PublicRepositories.json"
DEFAULT_MODEL = "Docs/generated/public-examples.json"
DEFAULT_INDEX = "Docs/en/reference/public-examples/index.md"
RUSSIAN_INDEX = "Docs/ru/reference/public-examples/index.md"
LEGACY_INDEX = "Docs/generated/public-examples/index.md"
GENERATED_BY = "BuildTools/docs_examples.py"
CANONICAL_OUTPUT_PATHS = (DEFAULT_INDEX,)
RUSSIAN_OUTPUT_PATHS = (RUSSIAN_INDEX,)
LEGACY_OUTPUT_PATHS = (LEGACY_INDEX,)
OUTPUT_PATHS = CANONICAL_OUTPUT_PATHS + RUSSIAN_OUTPUT_PATHS + LEGACY_OUTPUT_PATHS
REPOSITORY_ID_PATTERN = re.compile(r"^[a-z][a-z0-9-]*$")
REVISION_PATTERN = re.compile(r"^[0-9a-f]{40}$")
PLACEHOLDER_PATTERN = re.compile(r"\{\{[A-Z][A-Z0-9_]*\}\}")
ENGINE_PROVENANCE_PATTERN = re.compile(
    r"^(https://github\.com/[^/]+/fonline/blob/)[0-9a-f]{40}(/.+)$"
)
VALID_STATUSES = {"source-ready", "planned", "blocked", "published"}
VALID_TIERS = {"foundation", "tutorial", "showcase", "advanced"}
VALID_REMOTE_VISIBILITIES = {"private", "public"}
VALID_REMOTE_STATES = {"reserved", "source-staged", "published"}
VALID_REMOTE_CHECK_STATES = {"not-observed", "failing", "passing"}
VALID_ASSET_POLICIES = {
    "none",
    "project-original-or-permissive",
    "audited-public-or-project-original",
}
VALID_ASSET_LICENSES = {"CC0-1.0", "CC-BY-4.0", "MIT", "project-original", "public-domain"}
REQUIRED_OWNER_ROLES = {"documentation", "build_release", "runtime", "content", "security"}
REQUIRED_COMPATIBILITY_LANES = {"pinned-engine", "current-engine"}
REQUIRED_COMPATIBILITY_BOUNDARIES = {
    "gameplay-compatibility-version",
    "updater-protocol-generation",
    "client-host-runtime-abi",
}


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _string_list(value: object, label: str, *, allow_empty: bool = False) -> list[str]:
    if not isinstance(value, list) or (not value and not allow_empty):
        qualifier = "an" if allow_empty else "a non-empty"
        raise ValueError(f"{label} must be {qualifier} array of strings")
    if any(not isinstance(item, str) or not item.strip() for item in value):
        raise ValueError(f"{label} must contain only non-empty strings")
    if len(value) != len(set(value)):
        raise ValueError(f"{label} must not contain duplicates")
    return list(value)


def _relative_path(value: object, label: str) -> str:
    path = _required_string(value, label)
    relative = PurePosixPath(path)
    if "\\" in path or relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"{label} must be a repository-relative forward-slash path")
    return path


def _relative_pattern(value: object, label: str) -> str:
    pattern = _required_string(value, label)
    relative = PurePosixPath(pattern)
    if "\\" in pattern or relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"{label} must be a repository-relative forward-slash pattern")
    return pattern


def _path(root: Path, relative: str) -> Path:
    return root.joinpath(*PurePosixPath(relative).parts)


def _validate_template(root: Path, publication: dict[str, Any]) -> None:
    template_path = _relative_path(publication.get("template_path"), "program.publication.template_path")
    template_root = _path(root, template_path)
    if not template_root.is_dir():
        raise ValueError(f"program.publication.template_path does not exist: {template_path}")

    required_files = _string_list(publication.get("required_files"), "program.publication.required_files")
    copy_files = publication.get("copy_files")
    if not isinstance(copy_files, dict) or not copy_files:
        raise ValueError("program.publication.copy_files must be a non-empty object")
    normalized_copy_files: dict[str, str] = {}
    for raw_source, raw_destination in copy_files.items():
        source = _relative_path(raw_source, "program.publication.copy_files source")
        destination = _relative_path(raw_destination, f"program.publication.copy_files[{source}]")
        if not _path(template_root, source).is_file():
            raise ValueError(f"public repository template file does not exist: {template_path}/{source}")
        if destination in normalized_copy_files.values():
            raise ValueError(f"duplicate public repository template destination: {destination}")
        normalized_copy_files[source] = destination
    if sorted(normalized_copy_files.values()) != sorted(required_files):
        raise ValueError("program.publication.copy_files destinations must exactly match required_files")

    placeholders = set(_string_list(publication.get("placeholders"), "program.publication.placeholders"))
    if any(not PLACEHOLDER_PATTERN.fullmatch(value) for value in placeholders):
        raise ValueError("program.publication.placeholders contains an invalid placeholder")
    discovered: set[str] = set()
    for source in normalized_copy_files:
        source_path = _path(template_root, source)
        try:
            discovered.update(PLACEHOLDER_PATTERN.findall(source_path.read_text(encoding="utf-8")))
        except UnicodeDecodeError as error:
            raise ValueError(f"public repository template file must be UTF-8 text: {source}") from error
    if discovered != placeholders:
        missing = sorted(placeholders - discovered)
        unknown = sorted(discovered - placeholders)
        raise ValueError(f"public repository template placeholder mismatch; missing={missing}, unknown={unknown}")

    metadata_path = _relative_path(publication.get("metadata_path"), "program.publication.metadata_path")
    provenance_path = _relative_path(publication.get("asset_provenance_path"), "program.publication.asset_provenance_path")
    if metadata_path not in required_files or provenance_path not in required_files:
        raise ValueError("publication metadata and asset provenance paths must be required files")
    _required_string(publication.get("protected_branch"), "program.publication.protected_branch")
    if publication.get("security_advisories_required") is not True:
        raise ValueError("program.publication.security_advisories_required must be true")

    provenance_source = next(source for source, destination in normalized_copy_files.items() if destination == provenance_path)
    _validate_provenance(_path(template_root, provenance_source), template_root, require_files=False)


def _validate_program(root: Path, raw: object) -> dict[str, Any]:
    if not isinstance(raw, dict):
        raise ValueError("program must be an object")
    program = copy.deepcopy(raw)
    if _required_string(program.get("id"), "program.id") != "fonline-public-examples":
        raise ValueError("program.id must be fonline-public-examples")
    organization = _required_string(program.get("organization"), "program.organization")
    engine_repository = _required_string(program.get("engine_repository"), "program.engine_repository")
    if engine_repository != f"{organization}/fonline":
        raise ValueError("program.engine_repository must identify the owning FOnline repository")
    engine_clone_url = _required_string(program.get("engine_clone_url"), "program.engine_clone_url")
    if engine_clone_url != f"https://github.com/{engine_repository}.git":
        raise ValueError("program.engine_clone_url must be the HTTPS URL for program.engine_repository")
    documentation_url = _required_string(program.get("documentation_url"), "program.documentation_url")
    if not documentation_url.startswith("https://"):
        raise ValueError("program.documentation_url must use HTTPS")
    _required_string(program.get("default_branch"), "program.default_branch")
    _required_string(program.get("license"), "program.license")

    owners = program.get("owners")
    if not isinstance(owners, dict) or set(owners) != REQUIRED_OWNER_ROLES:
        raise ValueError(f"program.owners must define exactly {sorted(REQUIRED_OWNER_ROLES)}")
    for role, owner in owners.items():
        _required_string(owner, f"program.owners.{role}")

    compatibility = program.get("compatibility")
    if not isinstance(compatibility, dict):
        raise ValueError("program.compatibility must be an object")
    if compatibility.get("release_engine_ref") != "exact-commit":
        raise ValueError("program.compatibility.release_engine_ref must be exact-commit")
    if compatibility.get("development_engine_ref") != "master":
        raise ValueError("program.compatibility.development_engine_ref must be master")
    if compatibility.get("scheduled_cadence") != "weekly":
        raise ValueError("program.compatibility.scheduled_cadence must be weekly")
    if compatibility.get("update_delivery") != "reviewed-pull-request":
        raise ValueError("program.compatibility.update_delivery must be reviewed-pull-request")
    lanes = set(_string_list(compatibility.get("required_lanes"), "program.compatibility.required_lanes"))
    if lanes != REQUIRED_COMPATIBILITY_LANES:
        raise ValueError(f"program.compatibility.required_lanes must be {sorted(REQUIRED_COMPATIBILITY_LANES)}")
    boundaries = set(_string_list(compatibility.get("boundaries"), "program.compatibility.boundaries"))
    if boundaries != REQUIRED_COMPATIBILITY_BOUNDARIES:
        raise ValueError(f"program.compatibility.boundaries must be {sorted(REQUIRED_COMPATIBILITY_BOUNDARIES)}")

    publication = program.get("publication")
    if not isinstance(publication, dict):
        raise ValueError("program.publication must be an object")
    _validate_template(root, publication)
    return program


def _validate_repositories(root: Path, raw: object, program: dict[str, Any]) -> list[dict[str, Any]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError("repositories must be a non-empty array")
    repositories: list[dict[str, Any]] = []
    ids: set[str] = set()
    names: set[str] = set()
    sequences: set[int] = set()
    owner_roles = set(program["owners"])
    organization = str(program["organization"])
    for index, raw_repository in enumerate(raw):
        label = f"repositories[{index}]"
        if not isinstance(raw_repository, dict):
            raise ValueError(f"{label} must be an object")
        repository = copy.deepcopy(raw_repository)
        repository_id = _required_string(repository.get("id"), f"{label}.id")
        repository_name = _required_string(repository.get("repository"), f"{label}.repository")
        sequence = repository.get("sequence")
        if not REPOSITORY_ID_PATTERN.fullmatch(repository_id):
            raise ValueError(f"invalid public example repository id: {repository_id}")
        if not repository_name.startswith(f"{organization}/fonline-"):
            raise ValueError(f"{label}.repository must be owned by {organization} and use the fonline- prefix")
        if not isinstance(sequence, int) or sequence <= 0:
            raise ValueError(f"{label}.sequence must be a positive integer")
        if repository_id in ids or repository_name in names or sequence in sequences:
            raise ValueError(f"duplicate public example repository identity: {repository_id}")
        ids.add(repository_id)
        names.add(repository_name)
        sequences.add(sequence)
        tier = _required_string(repository.get("tier"), f"{label}.tier")
        status = _required_string(repository.get("status"), f"{label}.status")
        owner = _required_string(repository.get("owner"), f"{label}.owner")
        asset_policy = _required_string(repository.get("asset_policy"), f"{label}.asset_policy")
        if tier not in VALID_TIERS:
            raise ValueError(f"unsupported {label}.tier: {tier}")
        if status not in VALID_STATUSES:
            raise ValueError(f"unsupported {label}.status: {status}")
        if owner not in owner_roles:
            raise ValueError(f"unknown {label}.owner: {owner}")
        if asset_policy not in VALID_ASSET_POLICIES:
            raise ValueError(f"unsupported {label}.asset_policy: {asset_policy}")
        remote = repository.get("remote")
        if not isinstance(remote, dict):
            raise ValueError(f"{label}.remote must be an object")
        visibility = _required_string(remote.get("visibility"), f"{label}.remote.visibility")
        remote_state = _required_string(remote.get("state"), f"{label}.remote.state")
        created_on = _required_string(remote.get("created_on"), f"{label}.remote.created_on")
        verified_on = _required_string(remote.get("verified_on"), f"{label}.remote.verified_on")
        default_branch = _required_string(remote.get("default_branch"), f"{label}.remote.default_branch")
        head_commit = _required_string(remote.get("head_commit"), f"{label}.remote.head_commit")
        checks_state = _required_string(
            remote.get("required_checks_state"),
            f"{label}.remote.required_checks_state",
        )
        if visibility not in VALID_REMOTE_VISIBILITIES:
            raise ValueError(f"unsupported {label}.remote.visibility: {visibility}")
        if remote_state not in VALID_REMOTE_STATES:
            raise ValueError(f"unsupported {label}.remote.state: {remote_state}")
        if checks_state not in VALID_REMOTE_CHECK_STATES:
            raise ValueError(f"unsupported {label}.remote.required_checks_state: {checks_state}")
        if default_branch != program["default_branch"]:
            raise ValueError(f"{label}.remote.default_branch must match program.default_branch")
        if not REVISION_PATTERN.fullmatch(head_commit):
            raise ValueError(f"{label}.remote.head_commit must be a 40-character lowercase commit")
        for field_name, field_value in (("created_on", created_on), ("verified_on", verified_on)):
            try:
                date.fromisoformat(field_value)
            except ValueError as error:
                raise ValueError(f"{label}.remote.{field_name} must be an ISO date") from error
        if date.fromisoformat(verified_on) < date.fromisoformat(created_on):
            raise ValueError(f"{label}.remote.verified_on must not precede created_on")
        if status == "published" and (visibility != "public" or remote_state != "published"):
            raise ValueError(f"{label} published status requires a public, published remote")
        if status == "published" and checks_state != "passing":
            raise ValueError(f"{label} published status requires passing required checks")
        if remote_state == "published" and (visibility != "public" or status != "published"):
            raise ValueError(f"{label}.remote published state requires public visibility and published status")
        if remote_state == "source-staged" and status != "source-ready":
            raise ValueError(f"{label}.remote source-staged state requires source-ready status")
        engine_revision = remote.get("engine_revision")
        if remote_state == "reserved" and engine_revision is not None:
            raise ValueError(f"{label}.remote.engine_revision requires staged source")
        if remote_state != "reserved":
            normalized_engine_revision = _required_string(
                engine_revision,
                f"{label}.remote.engine_revision",
            )
            if not REVISION_PATTERN.fullmatch(normalized_engine_revision):
                raise ValueError(
                    f"{label}.remote.engine_revision must be a 40-character lowercase commit"
                )
        source_path = repository.get("source_path")
        if source_path is not None:
            normalized_source = _relative_path(source_path, f"{label}.source_path")
            source_root = _path(root, normalized_source)
            if not source_root.is_dir():
                raise ValueError(f"{label}.source_path does not exist: {normalized_source}")
            source_required_files = _string_list(
                repository.get("source_required_files"),
                f"{label}.source_required_files",
            )
            source_excludes = _string_list(
                repository.get("source_excludes"),
                f"{label}.source_excludes",
                allow_empty=True,
            )
            normalized_excludes = [
                _relative_pattern(pattern, f"{label}.source_excludes entry")
                for pattern in source_excludes
            ]
            for source_required_file in source_required_files:
                normalized_required_file = _relative_path(
                    source_required_file,
                    f"{label}.source_required_files entry",
                )
                if not _path(source_root, normalized_required_file).is_file():
                    raise ValueError(
                        f"{label}.source_required_files path does not exist: "
                        f"{normalized_source}/{normalized_required_file}"
                    )
                if _matches_source_exclusion(normalized_required_file, normalized_excludes):
                    raise ValueError(
                        f"{label}.source_required_files path is excluded from staging: "
                        f"{normalized_required_file}"
                    )
            _required_string(repository.get("display_name"), f"{label}.display_name")
            _required_string(repository.get("primary_check"), f"{label}.primary_check")
        elif "source_required_files" in repository:
            raise ValueError(f"{label}.source_required_files requires source_path")
        elif "source_excludes" in repository or "primary_check" in repository:
            raise ValueError(f"{label}.source_excludes and primary_check require source_path")
        else:
            _required_string(repository.get("display_name"), f"{label}.display_name")
        _required_string(repository.get("purpose"), f"{label}.purpose")
        _required_string(repository.get("exit_gate"), f"{label}.exit_gate")
        _string_list(repository.get("capabilities"), f"{label}.capabilities")
        checks = set(_string_list(repository.get("required_checks"), f"{label}.required_checks"))
        if not REQUIRED_COMPATIBILITY_LANES.issubset(checks) or "governance-contract" not in checks:
            raise ValueError(f"{label}.required_checks must include governance-contract and both compatibility lanes")
        _string_list(repository.get("required_artifacts"), f"{label}.required_artifacts")
        _string_list(repository.get("depends_on"), f"{label}.depends_on", allow_empty=True)
        repositories.append(repository)

    if sequences != set(range(1, len(repositories) + 1)):
        raise ValueError("public example repository sequence must be contiguous and start at 1")
    by_id = {str(repository["id"]): repository for repository in repositories}
    for repository in repositories:
        for dependency in repository["depends_on"]:
            if dependency not in by_id:
                raise ValueError(f"repository {repository['id']} depends on unknown repository {dependency}")
            if int(by_id[dependency]["sequence"]) >= int(repository["sequence"]):
                raise ValueError(f"repository {repository['id']} dependency {dependency} must have an earlier sequence")
    return sorted(repositories, key=lambda entry: int(entry["sequence"]))


def generate_model(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> dict[str, Any]:
    manifest_path = _path(root, _relative_path(manifest_relative_path, "manifest path"))
    raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"public example registry schema_version must be {SCHEMA_VERSION}")
    program = _validate_program(root, raw.get("program"))
    repositories = _validate_repositories(root, raw.get("repositories"), program)
    contract_source = {"program": program, "repositories": repositories}
    digest = hashlib.sha256(
        json.dumps(contract_source, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    statuses: dict[str, int] = {}
    remote_states: dict[str, int] = {}
    visibilities: dict[str, int] = {}
    for repository in repositories:
        status = str(repository["status"])
        statuses[status] = statuses.get(status, 0) + 1
        remote_state = str(repository["remote"]["state"])
        visibility = str(repository["remote"]["visibility"])
        remote_states[remote_state] = remote_states.get(remote_state, 0) + 1
        visibilities[visibility] = visibilities.get(visibility, 0) + 1
        if status == "published" and visibility == "public":
            repository["url"] = f"https://github.com/{repository['repository']}"
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": manifest_relative_path,
        "source_ref": "master",
        "contract_digest": digest,
        "summary": {
            "repository_count": len(repositories),
            "published_count": statuses.get("published", 0),
            "source_ready_count": statuses.get("source-ready", 0),
            "planned_count": statuses.get("planned", 0),
            "private_count": visibilities.get("private", 0),
            "source_staged_count": remote_states.get("source-staged", 0),
        },
        "program": program,
        "repositories": repositories,
    }


def render_model(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> str:
    return json.dumps(generate_model(root, manifest_relative_path), ensure_ascii=False, indent=2, sort_keys=True) + "\n"


def _escape(value: object) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


def render_index(model: dict[str, Any]) -> str:
    program = model["program"]
    compatibility = program["compatibility"]
    summary = model["summary"]
    required_check_states = ", ".join(sorted({
        str(repository["remote"]["required_checks_state"])
        for repository in model["repositories"]
    }))
    observed_engine_pins = ", ".join(
        f"`{_escape(repository['id'])}`="
        + (
            f"`{_escape(repository['remote']['engine_revision'])}`"
            if repository["remote"].get("engine_revision")
            else "not observed"
        )
        for repository in model["repositories"]
    )
    current_repository_rows = [
        f"- `{_escape(repository['id'])}`: source `{_escape(repository['status'])}`; "
        f"remote `{_escape(repository['remote']['visibility'])}` / "
        f"`{_escape(repository['remote']['state'])}`; Engine pin "
        + (
            f"`{_escape(repository['remote']['engine_revision'])}`"
            if repository["remote"].get("engine_revision")
            else "not observed"
        )
        + f"; required checks `{_escape(repository['remote']['required_checks_state'])}`."
        for repository in model["repositories"]
    ]
    lines = [
        "---",
        "title: Generated Public Example Repository Registry",
        "document_id: generated-public-examples-index",
        "locale: en",
        "generated: true",
        "---",
        "",
        "# Generated Public Example Repository Registry",
        "",
        "> Generated reference. Do not edit this page directly. Update `Examples/PublicRepositories.json` or the governance overlay, then run `python BuildTools/docs_examples.py --write`.",
        "",
        "[Human policy](../../how-to/build/public-example-repositories.md) | [Canonical JSON](../../../generated/public-examples.json) | [Source registry](../../../../Examples/PublicRepositories.json)",
        "",
        "This registry describes illustrative embedding-project repositories. Engine behavior remains normative only in Engine source, tests, and owning documentation.",
        "",
        "## Program contract",
        "",
        "| Field | Value |",
        "| --- | --- |",
        f"| Organization | `{_escape(program['organization'])}` |",
        f"| Engine repository | `{_escape(program['engine_repository'])}` |",
        f"| Release Engine ref | `{_escape(compatibility['release_engine_ref'])}` |",
        f"| Development ref | `{_escape(compatibility['development_engine_ref'])}` ({_escape(compatibility['scheduled_cadence'])}) |",
        f"| Update delivery | `{_escape(compatibility['update_delivery'])}` |",
        f"| Contract digest | `{_escape(model['contract_digest'])}` |",
        "",
        "## Publication evidence",
        "",
        "Read each repository's source status, remote visibility/state, observed required-check state, exact Engine pin, update-delivery policy, and Contract digest together. Only `published` source with a `public` / `published` remote and `passing` observed checks is publication evidence. A private, reserved, source-staged, planned, or not-observed row remains pre-publication evidence even when its source is ready.",
        "",
        "## Current registry state",
        "",
        f"- Source/remote: `{summary['source_ready_count']}` source-ready, `{summary['private_count']}` private, and `{summary['published_count']}` published repositories.",
        f"- Observed required-check states: `{_escape(required_check_states)}`.",
        f"- Observed Engine pins: {observed_engine_pins}.",
        f"- Program values required in the same report: release Engine ref `{_escape(compatibility['release_engine_ref'])}`, update delivery `{_escape(compatibility['update_delivery'])}`, Contract digest `{_escape(model['contract_digest'])}`.",
        *current_repository_rows,
        "",
        "## Portfolio",
        "",
        "| Order | Repository | Tier | Source status | Remote | Owner | Purpose |",
        "| ---: | --- | --- | --- | --- | --- | --- |",
    ]
    owners = program["owners"]
    for repository in model["repositories"]:
        lines.append(
            f"| {repository['sequence']} | `{_escape(repository['repository'])}` | `{_escape(repository['tier'])}` | "
            f"`{_escape(repository['status'])}` | `{_escape(repository['remote']['visibility'])}` / "
            f"`{_escape(repository['remote']['state'])}` | {_escape(owners[repository['owner']])} | "
            f"{_escape(repository['purpose'])} |"
        )
    for repository in model["repositories"]:
        dependencies = ", ".join(f"`{value}`" for value in repository["depends_on"]) or "None"
        source_path = f"`{repository['source_path']}`" if repository.get("source_path") else "Not assigned"
        lines.extend([
            "",
            f"## {_escape(repository['repository'])}",
            "",
            f"- Stable ID: `{_escape(repository['id'])}`",
            f"- Engine-owned source: {source_path}",
            f"- Remote: `{_escape(repository['remote']['visibility'])}` / "
            f"`{_escape(repository['remote']['state'])}` (created `{_escape(repository['remote']['created_on'])}`)",
            f"- Remote observation: `{_escape(repository['remote']['verified_on'])}`, "
            f"branch `{_escape(repository['remote']['default_branch'])}`, "
            f"head `{_escape(repository['remote']['head_commit'])}`, "
            f"required checks `{_escape(repository['remote']['required_checks_state'])}`",
            *(
                [f"- Observed Engine pin: `{_escape(repository['remote']['engine_revision'])}`"]
                if repository['remote'].get('engine_revision')
                else []
            ),
            f"- Dependencies: {dependencies}",
            f"- Asset policy: `{_escape(repository['asset_policy'])}`",
            "",
            "Capabilities:",
            "",
            *[f"- `{_escape(value)}`" for value in repository["capabilities"]],
            "",
            "Required checks:",
            "",
            *[f"- `{_escape(value)}`" for value in repository["required_checks"]],
            "",
            f"Exit gate: {repository['exit_gate']}",
        ])
    return "\n".join(lines).rstrip() + "\n"


RUSSIAN_REPLACEMENTS = {
    "Generated Public Example Repository Registry": "Сгенерированный реестр публичных репозиториев-примеров",
    "> Generated reference. Do not edit this page directly. Update `Examples/PublicRepositories.json` or the governance overlay, then run `python BuildTools/docs_examples.py --write`.":
        "> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `Examples/PublicRepositories.json` или управляющий overlay, затем выполните `python BuildTools/docs_examples.py --write`.",
    "[Human policy](../../how-to/build/public-example-repositories.md) | [Canonical JSON](../../../generated/public-examples.json) | [Source registry](../../../../Examples/PublicRepositories.json)":
        "[Политика для разработчиков](../../how-to/build/public-example-repositories.md) | [Канонический JSON](../../../generated/public-examples.json) | [Исходный реестр](../../../../Examples/PublicRepositories.json)",
    "This registry describes illustrative embedding-project repositories. Engine behavior remains normative only in Engine source, tests, and owning documentation.":
        "Этот реестр описывает демонстрационные репозитории встраивающих проектов. Нормативное поведение движка определяется только исходным кодом Engine, тестами и документацией-владельцем.",
    "## Program contract": "## Контракт программы",
    "## Publication evidence": "## Свидетельства публикации",
    "Read each repository's source status, remote visibility/state, observed required-check state, exact Engine pin, update-delivery policy, and Contract digest together. Only `published` source with a `public` / `published` remote and `passing` observed checks is publication evidence. A private, reserved, source-staged, planned, or not-observed row remains pre-publication evidence even when its source is ready.":
        "Проверяйте вместе source status каждого репозитория, visibility/state remote, состояние наблюдённых required checks, точный Engine pin, политику update delivery и Contract digest. Свидетельством публикации является только source со статусом `published`, remote `public` / `published` и наблюдёнными checks `passing`. Строка private, reserved, source-staged, planned или not-observed остаётся предпубликационным свидетельством, даже если её исходники готовы.",
    "## Current registry state": "## Текущее состояние реестра",
    "- Source/remote:": "- Source/remote:",
    "- Observed required-check states:": "- Наблюдённые состояния required checks:",
    "- Observed Engine pins:": "- Наблюдённые Engine pins:",
    "- Program values required in the same report:": "- Значения программы, обязательные в том же отчёте:",
    "## Portfolio": "## Портфель",
    "| Field | Value |": "| Поле | Значение |",
    "| Order | Repository | Tier | Source status | Remote | Owner | Purpose |":
        "| Порядок | Репозиторий | Уровень | Статус исходников | Remote | Владелец | Назначение |",
    "| Organization |": "| Организация |",
    "| Engine repository |": "| Репозиторий движка |",
    "| Release Engine ref |": "| Ревизия Engine для релиза |",
    "| Development ref |": "| Ревизия для разработки |",
    "| Update delivery |": "| Доставка обновлений |",
    "| Contract digest |": "| Digest контракта |",
    "- Stable ID:": "- Стабильный ID:",
    "- Engine-owned source:": "- Исходники под ответственностью Engine:",
    "- Remote:": "- Remote:",
    "- Remote observation:": "- Наблюдение remote:",
    "- Observed Engine pin:": "- Зафиксированная ревизия Engine:",
    "- Dependencies:": "- Зависимости:",
    "- Asset policy:": "- Политика ресурсов:",
    "Capabilities:": "Возможности:",
    "Required checks:": "Обязательные проверки:",
    "Exit gate:": "Критерий завершения:",
    "Not assigned": "Не назначено",
    "None": "Нет",
}


def render_russian_index(english_content: str, russian_base_content: str) -> str:
    content = russian_base_content.replace("locale: en", "locale: ru", 1)
    for english, russian in sorted(
        RUSSIAN_REPLACEMENTS.items(), key=lambda item: -len(item[0])
    ):
        content = content.replace(english, russian)
    front_matter_end = content.find("\n---\n", 4)
    if front_matter_end < 0:
        raise ValueError("generated public examples page has no front matter")
    insert_at = front_matter_end + len("\n---\n")
    marker = docs_localization.translation_metadata_line(
        "generated-public-examples-index",
        DEFAULT_INDEX,
        docs_localization.normalized_sha256(english_content),
    )
    return content[:insert_at] + "\n" + marker + "\n" + content[insert_at:]


def render_legacy_index(english_content: str) -> str:
    english_path = "../../en/reference/public-examples/index.md"
    russian_path = "../../ru/reference/public-examples/index.md"
    lines = [
        "# Generated Public Example Repository Registry",
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


def render_outputs(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> dict[str, str]:
    model_content = render_model(root, manifest_relative_path)
    model = json.loads(model_content)
    english_content = render_index(model)
    russian_model = docs_description_translations.apply_translations(
        root,
        "public-examples",
        model,
    )
    russian_base_content = render_index(russian_model)
    return {
        DEFAULT_MODEL: model_content,
        DEFAULT_INDEX: english_content,
        RUSSIAN_INDEX: render_russian_index(english_content, russian_base_content),
        LEGACY_INDEX: render_legacy_index(english_content),
    }


def _validate_provenance(
    path: Path,
    repository_root: Path,
    *,
    require_files: bool,
    allowed_missing_prefixes: tuple[str, ...] = (),
) -> None:
    raw = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(raw, dict) or raw.get("schema_version") != 1:
        raise ValueError(f"asset provenance schema_version must be 1: {path}")
    assets = raw.get("assets")
    if not isinstance(assets, list):
        raise ValueError(f"asset provenance assets must be an array: {path}")
    ids: set[str] = set()
    paths: set[str] = set()
    for index, asset in enumerate(assets):
        label = f"assets[{index}]"
        if not isinstance(asset, dict):
            raise ValueError(f"{label} must be an object")
        asset_id = _required_string(asset.get("id"), f"{label}.id")
        asset_path = _relative_path(asset.get("path"), f"{label}.path")
        license_id = _required_string(asset.get("license"), f"{label}.license")
        source = _required_string(asset.get("source"), f"{label}.source")
        sha256 = _required_string(asset.get("sha256"), f"{label}.sha256")
        if asset_id in ids or asset_path in paths:
            raise ValueError(f"duplicate asset provenance identity: {asset_id}")
        if license_id not in VALID_ASSET_LICENSES:
            raise ValueError(f"unsupported {label}.license: {license_id}")
        if source != "project-original" and not source.startswith("https://"):
            raise ValueError(f"{label}.source must be project-original or an HTTPS URL")
        if not re.fullmatch(r"[0-9a-f]{64}", sha256):
            raise ValueError(f"{label}.sha256 must be a lowercase SHA-256 digest")
        asset_file = _path(repository_root, asset_path)
        allowed_missing = any(
            asset_path == prefix or asset_path.startswith(prefix + "/")
            for prefix in allowed_missing_prefixes
        )
        if require_files:
            if not asset_file.is_file() and not allowed_missing:
                raise ValueError(f"asset provenance path does not exist: {asset_path}")
            if not asset_file.is_file():
                ids.add(asset_id)
                paths.add(asset_path)
                continue
            actual_sha256 = hashlib.sha256(asset_file.read_bytes()).hexdigest()
            if actual_sha256 != sha256:
                raise ValueError(
                    f"asset provenance digest does not match {asset_path}: "
                    f"expected {sha256}, actual {actual_sha256}"
                )
        ids.add(asset_id)
        paths.add(asset_path)


def _matches_source_exclusion(relative_path: str, patterns: list[str]) -> bool:
    if ".git" in PurePosixPath(relative_path).parts:
        return True
    for pattern in patterns:
        if pattern.endswith("/**"):
            prefix = pattern[:-3].rstrip("/")
            if relative_path == prefix or relative_path.startswith(prefix + "/"):
                return True
        if fnmatch.fnmatchcase(relative_path, pattern):
            return True
    return False


def _is_link(path: Path) -> bool:
    return path.is_symlink() or bool(getattr(path, "is_junction", lambda: False)())


def _copy_repository_source(source_root: Path, output_root: Path, exclusions: list[str]) -> int:
    copied = 0
    for current_root, directory_names, file_names in os.walk(source_root, followlinks=False):
        current = Path(current_root)
        relative_current = current.relative_to(source_root)
        retained_directories: list[str] = []
        for directory_name in sorted(directory_names):
            candidate = current / directory_name
            relative = (relative_current / directory_name).as_posix()
            if _matches_source_exclusion(relative, exclusions):
                continue
            if _is_link(candidate):
                raise ValueError(f"source staging refuses directory links: {relative}")
            retained_directories.append(directory_name)
        directory_names[:] = retained_directories

        for file_name in sorted(file_names):
            source_path = current / file_name
            relative = (relative_current / file_name).as_posix()
            if _matches_source_exclusion(relative, exclusions):
                continue
            if _is_link(source_path):
                raise ValueError(f"source staging refuses file links: {relative}")
            destination_relative = "TUTORIAL.md" if relative == "README.md" else relative
            destination_path = _path(output_root, destination_relative)
            if destination_path.exists():
                raise ValueError(f"source staging destination collision: {destination_relative}")
            destination_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source_path, destination_path)
            copied += 1
    return copied


def _render_template_file(source_path: Path, replacements: dict[str, str]) -> str:
    text = source_path.read_text(encoding="utf-8")
    for placeholder, value in replacements.items():
        text = text.replace(placeholder, value)
    unresolved = sorted(set(PLACEHOLDER_PATTERN.findall(text)))
    if unresolved:
        raise ValueError(f"unresolved publication placeholders in {source_path.name}: {unresolved}")
    return text


def _pin_engine_provenance(path: Path, engine_revision: str) -> None:
    raw = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(raw, dict) or not isinstance(raw.get("assets"), list):
        raise ValueError(f"asset provenance is not a valid object: {path}")
    changed = False
    for asset in raw["assets"]:
        if not isinstance(asset, dict) or not isinstance(asset.get("source"), str):
            continue
        source = asset["source"]
        match = ENGINE_PROVENANCE_PATTERN.fullmatch(source)
        if match is not None:
            asset["source"] = f"{match.group(1)}{engine_revision}{match.group(2)}"
            changed = True
    if changed:
        path.write_text(
            json.dumps(raw, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
            newline="\n",
        )


def stage_repository(
    engine_root: Path,
    repository_id: str,
    output_root: Path,
    engine_revision: str,
    *,
    check_engine_git: bool = True,
    manifest_relative_path: str = DEFAULT_MANIFEST,
) -> dict[str, object]:
    if not REVISION_PATTERN.fullmatch(engine_revision):
        raise ValueError("staged Engine revision must be an exact lowercase 40-character commit")
    model = generate_model(engine_root, manifest_relative_path)
    repositories = {str(repository["id"]): repository for repository in model["repositories"]}
    if repository_id not in repositories:
        raise ValueError(f"unknown public example repository id: {repository_id}")
    repository = repositories[repository_id]
    if repository["status"] != "source-ready" or not repository.get("source_path"):
        raise ValueError(f"public example repository is not source-ready: {repository_id}")
    if check_engine_git:
        _validate_staging_engine_revision(engine_root, engine_revision)

    source_root = _path(engine_root, str(repository["source_path"])).resolve()
    output_root = output_root.resolve()
    if output_root.exists():
        raise ValueError(f"source staging output already exists: {output_root}")
    if output_root == source_root or source_root in output_root.parents:
        raise ValueError("source staging output must be outside the example source directory")

    program = model["program"]
    publication = program["publication"]
    repository_slug = str(repository["repository"]).split("/", 1)[1]
    replacements = {
        "{{CODEOWNER}}": f"@{program['organization']}",
        "{{ENGINE_REVISION}}": engine_revision,
        "{{PRIMARY_CHECK_COMMAND}}": str(repository["primary_check"]),
        "{{REPOSITORY_ID}}": repository_id,
        "{{REPOSITORY_NAME}}": str(repository["display_name"]),
        "{{REPOSITORY_SLUG}}": repository_slug,
        "{{SUMMARY}}": str(repository["purpose"]),
    }
    exclusions = [str(pattern) for pattern in repository.get("source_excludes", [])]
    output_root.mkdir(parents=True)
    try:
        source_file_count = _copy_repository_source(source_root, output_root, exclusions)
        template_root = _path(engine_root, str(publication["template_path"]))
        for source, destination in publication["copy_files"].items():
            destination_path = _path(output_root, destination)
            if destination == publication["asset_provenance_path"] and destination_path.is_file():
                continue
            destination_path.parent.mkdir(parents=True, exist_ok=True)
            destination_path.write_text(
                _render_template_file(_path(template_root, source), replacements),
                encoding="utf-8",
                newline="\n",
            )
        provenance_path = _path(output_root, str(publication["asset_provenance_path"]))
        _pin_engine_provenance(provenance_path, engine_revision)
        _validate_provenance(
            provenance_path,
            output_root,
            require_files=True,
            allowed_missing_prefixes=("Engine",),
        )
        for relative_path in publication["required_files"]:
            required_path = _path(output_root, relative_path)
            if not required_path.is_file():
                raise ValueError(f"required staged repository file is missing: {relative_path}")
            unresolved = PLACEHOLDER_PATTERN.findall(required_path.read_text(encoding="utf-8"))
            if unresolved:
                raise ValueError(
                    f"unresolved publication placeholders in {relative_path}: "
                    f"{sorted(set(unresolved))}"
                )
    except Exception:
        shutil.rmtree(output_root)
        raise

    return {
        "repository_id": repository_id,
        "repository": str(repository["repository"]),
        "engine_revision": engine_revision,
        "output_root": str(output_root),
        "source_file_count": source_file_count,
        "governance_file_count": len(publication["copy_files"]),
    }


def _run_git(repository_root: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(repository_root), *arguments],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if result.returncode != 0:
        raise ValueError(f"git {' '.join(arguments)} failed: {result.stderr.strip()}")
    return result.stdout.strip()


def _validate_staging_engine_revision(engine_root: Path, engine_revision: str) -> None:
    checkout_revision = _run_git(engine_root, "rev-parse", "HEAD")
    if checkout_revision != engine_revision:
        raise ValueError(
            "staged Engine revision must match the current Engine checkout: "
            f"requested {engine_revision}, checkout {checkout_revision}"
        )
    dirty = _run_git(engine_root, "status", "--porcelain", "--untracked-files=all")
    if dirty:
        first_path = dirty.splitlines()[0]
        raise ValueError(
            "source staging requires a clean Engine working tree; "
            f"first pending path: {first_path}"
        )
    remote_branches = _run_git(engine_root, "branch", "-r", "--contains", engine_revision)
    if not remote_branches:
        raise ValueError(
            "staged Engine revision is not contained by a fetched remote-tracking branch; "
            "fetch and publish the Engine change before materializing a release candidate"
        )


def _validate_gitmodules(path: Path, submodule_path: str, engine_url: str) -> None:
    if not path.is_file():
        raise ValueError("required public example repository file is missing: .gitmodules")
    parser = configparser.ConfigParser(interpolation=None)
    try:
        parser.read_string(path.read_text(encoding="utf-8"))
    except configparser.Error as error:
        raise ValueError(f".gitmodules must be valid INI: {error}") from error
    matching_sections = [
        section
        for section in parser.sections()
        if section.startswith('submodule "') and parser.get(section, "path", fallback="") == submodule_path
    ]
    if len(matching_sections) != 1:
        raise ValueError(f".gitmodules must define exactly one submodule path for {submodule_path}")
    actual_url = parser.get(matching_sections[0], "url", fallback="")
    if actual_url != engine_url:
        raise ValueError(f".gitmodules Engine URL must be {engine_url}")


def verify_repository(
    engine_root: Path,
    repository_root: Path,
    engine_mode: str,
    *,
    check_git: bool = True,
    manifest_relative_path: str = DEFAULT_MANIFEST,
) -> dict[str, str]:
    if engine_mode not in {"pinned", "current"}:
        raise ValueError("engine mode must be pinned or current")
    model = generate_model(engine_root, manifest_relative_path)
    program = model["program"]
    publication = program["publication"]
    repository_root = repository_root.resolve()
    metadata_path = _path(repository_root, publication["metadata_path"])
    if not metadata_path.is_file():
        raise ValueError(f"public example repository metadata is missing: {publication['metadata_path']}")
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    if not isinstance(metadata, dict) or metadata.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"example-repository.json schema_version must be {SCHEMA_VERSION}")
    repository_id = _required_string(metadata.get("program_id"), "example-repository.json program_id")
    registry = {repository["id"]: repository for repository in model["repositories"]}
    if repository_id not in registry:
        raise ValueError(f"unknown public example repository program_id: {repository_id}")
    expected_repository = registry[repository_id]["repository"]
    if metadata.get("repository") != expected_repository:
        raise ValueError(f"example repository name must be {expected_repository}")
    engine = metadata.get("engine")
    if not isinstance(engine, dict):
        raise ValueError("example-repository.json engine must be an object")
    if engine.get("repository") != program["engine_clone_url"]:
        raise ValueError("example repository Engine URL does not match the program registry")
    revision = _required_string(engine.get("revision"), "example-repository.json engine.revision")
    if not REVISION_PATTERN.fullmatch(revision):
        raise ValueError("example repository Engine revision must be an exact lowercase 40-character commit")
    submodule_path = _relative_path(engine.get("submodule_path"), "example-repository.json engine.submodule_path")
    _validate_gitmodules(repository_root / ".gitmodules", submodule_path, str(program["engine_clone_url"]))
    _required_string(metadata.get("primary_check"), "example-repository.json primary_check")
    if metadata.get("asset_provenance") != publication["asset_provenance_path"]:
        raise ValueError("example repository asset_provenance path does not match the program registry")

    for relative_path in publication["required_files"]:
        candidate = _path(repository_root, relative_path)
        if not candidate.is_file():
            raise ValueError(f"required public example repository file is missing: {relative_path}")
        try:
            text = candidate.read_text(encoding="utf-8")
        except UnicodeDecodeError as error:
            raise ValueError(f"required public example repository file must be UTF-8 text: {relative_path}") from error
        placeholders = PLACEHOLDER_PATTERN.findall(text)
        if placeholders:
            raise ValueError(f"unresolved publication placeholders in {relative_path}: {sorted(set(placeholders))}")
    _validate_provenance(_path(repository_root, publication["asset_provenance_path"]), repository_root, require_files=True)

    tested_revision = revision
    if check_git:
        tree_entry = _run_git(repository_root, "ls-tree", "HEAD", submodule_path)
        fields = tree_entry.split()
        if len(fields) < 3 or fields[0] != "160000" or fields[1] != "commit":
            raise ValueError(f"{submodule_path} must be a committed git submodule")
        gitlink_revision = fields[2]
        if gitlink_revision != revision:
            raise ValueError("example repository Engine gitlink does not match example-repository.json")
        engine_checkout = repository_root / submodule_path
        tested_revision = _run_git(engine_checkout, "rev-parse", "HEAD")
        if engine_mode == "pinned" and tested_revision != revision:
            raise ValueError("pinned Engine checkout does not match the committed gitlink")
    return {
        "repository_id": repository_id,
        "repository": str(expected_repository),
        "pinned_engine_revision": revision,
        "tested_engine_revision": tested_revision,
        "engine_mode": engine_mode,
    }


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate and validate the FOnline public example repository program")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--verify-repository", type=Path)
    mode.add_argument("--stage-repository", metavar="ID")
    parser.add_argument("--engine-mode", choices=("pinned", "current"), default="pinned")
    parser.add_argument("--engine-revision")
    parser.add_argument("--output", type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    root = args.root.resolve()
    try:
        if args.stage_repository is not None:
            if args.engine_revision is None or args.output is None:
                raise ValueError("--stage-repository requires --engine-revision and --output")
            result = stage_repository(
                root,
                args.stage_repository,
                args.output,
                args.engine_revision,
                manifest_relative_path=args.manifest,
            )
            print(
                f"Staged public example repository source: {result['repository']} at "
                f"{result['output_root']} ({result['source_file_count']} source files, "
                f"Engine {result['engine_revision']})"
            )
            return 0
        if args.verify_repository is not None:
            result = verify_repository(root, args.verify_repository, args.engine_mode, manifest_relative_path=args.manifest)
            print(
                f"Public example repository is valid: {result['repository']} "
                f"({result['engine_mode']} Engine {result['tested_engine_revision']})"
            )
            return 0
        outputs = render_outputs(root, args.manifest)
        model = json.loads(outputs[DEFAULT_MODEL])
    except (OSError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to process public example repository documentation: {exception}", file=sys.stderr)
        return 1

    if args.write:
        for relative_path, content in outputs.items():
            output_path = _path(root, relative_path)
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote public example repository model and {len(OUTPUT_PATHS)} reference page")
        return 0

    stale = [
        path
        for path, content in outputs.items()
        if not _path(root, path).is_file() or _path(root, path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated public example repository documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_examples.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(
        f"Generated public example repository documentation is current: {summary['repository_count']} repositories, "
        f"{summary['source_ready_count']} source-ready, {summary['private_count']} private, "
        f"{summary['published_count']} published"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
