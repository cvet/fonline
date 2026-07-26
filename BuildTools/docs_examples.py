from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import subprocess
import sys
from datetime import date
from pathlib import Path, PurePosixPath
from typing import Any


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "Examples/PublicRepositories.json"
DEFAULT_MODEL = "Docs/generated/public-examples.json"
DEFAULT_INDEX = "Docs/generated/public-examples/index.md"
GENERATED_BY = "BuildTools/docs_examples.py"
OUTPUT_PATHS = (DEFAULT_INDEX,)
REPOSITORY_ID_PATTERN = re.compile(r"^[a-z][a-z0-9-]*$")
REVISION_PATTERN = re.compile(r"^[0-9a-f]{40}$")
PLACEHOLDER_PATTERN = re.compile(r"\{\{[A-Z][A-Z0-9_]*\}\}")
VALID_STATUSES = {"source-ready", "planned", "blocked", "published"}
VALID_TIERS = {"foundation", "tutorial", "showcase", "advanced"}
VALID_REMOTE_VISIBILITIES = {"private", "public"}
VALID_REMOTE_STATES = {"reserved", "source-staged", "published"}
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
        if visibility not in VALID_REMOTE_VISIBILITIES:
            raise ValueError(f"unsupported {label}.remote.visibility: {visibility}")
        if remote_state not in VALID_REMOTE_STATES:
            raise ValueError(f"unsupported {label}.remote.state: {remote_state}")
        try:
            date.fromisoformat(created_on)
        except ValueError as error:
            raise ValueError(f"{label}.remote.created_on must be an ISO date") from error
        if status == "published" and (visibility != "public" or remote_state != "published"):
            raise ValueError(f"{label} published status requires a public, published remote")
        if remote_state == "published" and (visibility != "public" or status != "published"):
            raise ValueError(f"{label}.remote published state requires public visibility and published status")
        if remote_state == "source-staged" and status != "source-ready":
            raise ValueError(f"{label}.remote source-staged state requires source-ready status")
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
        elif "source_required_files" in repository:
            raise ValueError(f"{label}.source_required_files requires source_path")
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
        "[Human policy](../../PublicExampleRepositories.md) | [Canonical JSON](../public-examples.json) | [Source registry](../../../Examples/PublicRepositories.json)",
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
            f"Stable ID: `{_escape(repository['id'])}`  ",
            f"Engine-owned source: {source_path}  ",
            f"Remote: `{_escape(repository['remote']['visibility'])}` / `{_escape(repository['remote']['state'])}` "
            f"(created `{_escape(repository['remote']['created_on'])}`)  ",
            f"Dependencies: {dependencies}  ",
            f"Asset policy: `{_escape(repository['asset_policy'])}`",
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


def render_outputs(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> dict[str, str]:
    model_content = render_model(root, manifest_relative_path)
    model = json.loads(model_content)
    return {
        DEFAULT_MODEL: model_content,
        DEFAULT_INDEX: render_index(model),
    }


def _validate_provenance(path: Path, repository_root: Path, *, require_files: bool) -> None:
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
        if require_files and not _path(repository_root, asset_path).is_file():
            raise ValueError(f"asset provenance path does not exist: {asset_path}")
        ids.add(asset_id)
        paths.add(asset_path)


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


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate and validate the FOnline public example repository program")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--verify-repository", type=Path)
    parser.add_argument("--engine-mode", choices=("pinned", "current"), default="pinned")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
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
