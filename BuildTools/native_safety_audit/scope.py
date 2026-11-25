"""Audit scopes, project configuration and source enumeration.

The Engine scope is always read from the Engine checkout that contains this
package (or ``--engine-root``). A game project adds its own native directories
with ``--scope``; those paths, the configuration file and project baselines are
relative to ``--root``. Engine files keep Engine-relative identities
(``Source/...``) so Engine rules and the Engine baseline match whether the
Engine is audited standalone or embedded in a project.
"""

from __future__ import annotations

import dataclasses
import fnmatch
import json
from pathlib import Path, PurePosixPath
from typing import Iterable


ENGINE_ROOT = Path(__file__).resolve().parents[2]
CONFIG_SCHEMA_VERSION = 1
ENGINE_ORIGIN = "engine"
PROJECT_ORIGIN = "project"


class ConfigError(ValueError):
    """Raised for an invalid command line scope or configuration file."""


@dataclasses.dataclass(frozen=True)
class SourceFile:
    path: Path
    display: str
    origin: str
    unit_path: str


@dataclasses.dataclass(frozen=True)
class Exclusion:
    pattern: str
    reason: str


@dataclasses.dataclass(frozen=True)
class FunctionRule:
    file: str
    function: str
    reason: str


@dataclasses.dataclass(frozen=True)
class TypedFactoryRule:
    name: str
    type: str
    reason: str


@dataclasses.dataclass(frozen=True)
class NamedRule:
    name: str
    reason: str


@dataclasses.dataclass(frozen=True)
class SyncScopeConfig:
    independent_job_schedulers: tuple[FunctionRule, ...] = ()
    typed_factories: tuple[TypedFactoryRule, ...] = ()
    deferred_callback_calls: tuple[NamedRule, ...] = ()


@dataclasses.dataclass(frozen=True)
class Config:
    exclusions: tuple[Exclusion, ...] = ()
    sync_scope: SyncScopeConfig = SyncScopeConfig()


@dataclasses.dataclass(frozen=True)
class AuditContext:
    root: Path
    engine_root: Path
    scopes: tuple[str, ...]
    config: Config

    @property
    def engine_display_prefix(self) -> str:
        if self.engine_root == self.root:
            return ""

        try:
            return self.engine_root.relative_to(self.root).as_posix() + "/"
        except ValueError:
            return ""

    def engine_file(self, path: Path) -> SourceFile:
        unit_path = path.relative_to(self.engine_root).as_posix()

        return SourceFile(path, self.engine_display_prefix + unit_path, ENGINE_ORIGIN, unit_path)

    def project_file(self, path: Path) -> SourceFile:
        unit_path = path.relative_to(self.root).as_posix()

        return SourceFile(path, unit_path, PROJECT_ORIGIN, unit_path)

    def display_path(self, origin: str, unit_path: str) -> str:
        return self.engine_display_prefix + unit_path if origin == ENGINE_ORIGIN else unit_path


def create_context(root: Path | None, engine_root: Path | None, scopes: Iterable[str], config_path: Path | None) -> AuditContext:
    """Validate the command line scope and return the audit context."""

    resolved_root = (root if root is not None else Path.cwd()).resolve()
    resolved_engine = (engine_root if engine_root is not None else ENGINE_ROOT).resolve()

    if not resolved_root.is_dir():
        raise ConfigError(f"root is not a directory: {resolved_root}")

    if not (resolved_engine / "Source").is_dir():
        raise ConfigError(f"engine root has no Source directory: {resolved_engine}")

    normalized_scopes: list[str] = []

    for scope in scopes:
        relative = _relative_path(scope, "--scope")
        scope_path = (resolved_root / relative).resolve()

        if not scope_path.is_dir():
            raise ConfigError(f"--scope {scope}: directory does not exist under the root")

        if not _is_within(scope_path, resolved_root):
            raise ConfigError(f"--scope {scope}: directory resolves outside the root")

        engine_source = resolved_engine / "Source"

        if _is_within(scope_path, engine_source) or _is_within(engine_source, scope_path):
            raise ConfigError(f"--scope {scope}: project scope overlaps the Engine Source scope, which is always audited")

        if relative in normalized_scopes:
            raise ConfigError(f"--scope {scope}: duplicate scope")

        normalized_scopes.append(relative)

    config = load_config(_resolve_input(resolved_root, config_path)) if config_path is not None else Config()

    return AuditContext(resolved_root, resolved_engine, tuple(normalized_scopes), config)


def resolve_input(context: AuditContext, path: Path) -> Path:
    """Resolve a root-relative or absolute input path."""

    return _resolve_input(context.root, path)


def load_config(path: Path) -> Config:
    """Read and strictly validate a project configuration file."""

    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exception:
        raise ConfigError(f"unable to read configuration {path}: {exception}") from exception

    label = path.name
    _require_object(raw, label, {"schema_version", "exclude", "sync_scope"})

    if raw.get("schema_version") != CONFIG_SCHEMA_VERSION:
        raise ConfigError(f"{label}: schema_version must be {CONFIG_SCHEMA_VERSION}")

    exclusions: list[Exclusion] = []

    for index, entry in enumerate(_list(raw.get("exclude", []), f"{label}.exclude")):
        entry_label = f"{label}.exclude[{index}]"
        _require_object(entry, entry_label, {"path", "reason"})
        pattern = _relative_path(_string(entry.get("path"), f"{entry_label}.path"), f"{entry_label}.path", allow_glob=True)
        exclusions.append(Exclusion(pattern, _string(entry.get("reason"), f"{entry_label}.reason")))

    sync_raw = raw.get("sync_scope", {})
    sync_label = f"{label}.sync_scope"
    _require_object(sync_raw, sync_label, {"independent_job_schedulers", "typed_factories", "deferred_callback_calls"})

    schedulers: list[FunctionRule] = []

    for index, entry in enumerate(_list(sync_raw.get("independent_job_schedulers", []), f"{sync_label}.independent_job_schedulers")):
        entry_label = f"{sync_label}.independent_job_schedulers[{index}]"
        _require_object(entry, entry_label, {"file", "function", "reason"})
        schedulers.append(FunctionRule(
            _relative_path(_string(entry.get("file"), f"{entry_label}.file"), f"{entry_label}.file"),
            _string(entry.get("function"), f"{entry_label}.function"),
            _string(entry.get("reason"), f"{entry_label}.reason")))

    factories: list[TypedFactoryRule] = []

    for index, entry in enumerate(_list(sync_raw.get("typed_factories", []), f"{sync_label}.typed_factories")):
        entry_label = f"{sync_label}.typed_factories[{index}]"
        _require_object(entry, entry_label, {"name", "type", "reason"})
        factories.append(TypedFactoryRule(
            _identifier(entry.get("name"), f"{entry_label}.name"),
            _identifier(entry.get("type"), f"{entry_label}.type"),
            _string(entry.get("reason"), f"{entry_label}.reason")))

    deferred: list[NamedRule] = []

    for index, entry in enumerate(_list(sync_raw.get("deferred_callback_calls", []), f"{sync_label}.deferred_callback_calls")):
        entry_label = f"{sync_label}.deferred_callback_calls[{index}]"
        _require_object(entry, entry_label, {"name", "reason"})
        deferred.append(NamedRule(
            _identifier(entry.get("name"), f"{entry_label}.name"),
            _string(entry.get("reason"), f"{entry_label}.reason")))

    _reject_duplicates([entry.pattern for entry in exclusions], f"{label}.exclude")
    _reject_duplicates([(entry.file, entry.function) for entry in schedulers], f"{sync_label}.independent_job_schedulers")
    _reject_duplicates([entry.name for entry in factories], f"{sync_label}.typed_factories")
    _reject_duplicates([entry.name for entry in deferred], f"{sync_label}.deferred_callback_calls")

    return Config(tuple(exclusions), SyncScopeConfig(tuple(schedulers), tuple(factories), tuple(deferred)))


def engine_files(context: AuditContext, directories: Iterable[tuple[str, tuple[str, ...]]], skip=None) -> list[SourceFile]:
    """Enumerate Engine files per ``(engine-relative directory, suffixes)`` in a stable order."""

    files: list[SourceFile] = []

    for directory, suffixes in directories:
        base = context.engine_root / directory

        if not base.is_dir():
            continue

        for suffix in suffixes:
            for path in sorted(base.rglob(f"*{suffix}")):
                source = context.engine_file(path)

                if skip is None or not skip(source.unit_path):
                    files.append(source)

    return files


def project_files(context: AuditContext, suffixes: tuple[str, ...], skip=None) -> tuple[list[SourceFile], list[str]]:
    """Enumerate project scope files, applying exclusions; return files and stale-exclusion errors."""

    files: list[SourceFile] = []
    matched: set[str] = set()

    for scope in context.scopes:
        base = context.root / scope
        every_file = sorted(path for path in base.rglob("*") if path.is_file())

        for path in every_file:
            unit_path = path.relative_to(context.root).as_posix()
            matched.update(entry.pattern for entry in context.config.exclusions if _excluded(unit_path, entry.pattern))

        for suffix in suffixes:
            for path in sorted(base.rglob(f"*{suffix}")):
                source = context.project_file(path)

                if any(_excluded(source.unit_path, entry.pattern) for entry in context.config.exclusions):
                    continue

                if skip is None or not skip(source.unit_path):
                    files.append(source)

    stale = [
        f"configuration exclusion matches no file in the project scopes: {entry.pattern} ({entry.reason})"
        for entry in context.config.exclusions
        if entry.pattern not in matched
    ]

    return files, stale


def _excluded(unit_path: str, pattern: str) -> bool:
    return unit_path == pattern or unit_path.startswith(pattern.rstrip("/") + "/") or fnmatch.fnmatchcase(unit_path, pattern)


def _resolve_input(root: Path, path: Path) -> Path:
    return (path if path.is_absolute() else root / path).resolve()


def _is_within(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
    except ValueError:
        return False

    return True


def _relative_path(value: str, label: str, allow_glob: bool = False) -> str:
    text = value.replace("\\", "/").strip()
    relative = PurePosixPath(text)

    if not text or relative.is_absolute() or ".." in relative.parts or ":" in text:
        raise ConfigError(f"{label}: expected a root-relative path without '..', got {value!r}")

    if not allow_glob and any(char in text for char in "*?["):
        raise ConfigError(f"{label}: wildcards are not allowed here, got {value!r}")

    normalized = relative.as_posix()

    return "." if normalized == "." else normalized


def _require_object(value: object, label: str, allowed: set[str]) -> None:
    if not isinstance(value, dict):
        raise ConfigError(f"{label}: expected a JSON object")

    unknown = sorted(set(value) - allowed)

    if unknown:
        raise ConfigError(f"{label}: unknown key(s): {', '.join(unknown)}")


def _list(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise ConfigError(f"{label}: expected a JSON array")

    return value


def _string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ConfigError(f"{label}: expected a non-empty string")

    return value.strip()


def _identifier(value: object, label: str) -> str:
    text = _string(value, label)

    if not text.replace("_", "a").isalnum() or text[0].isdigit():
        raise ConfigError(f"{label}: expected a C++ identifier, got {text!r}")

    return text


def _reject_duplicates(values: list[object], label: str) -> None:
    seen: set[object] = set()

    for value in values:
        if value in seen:
            raise ConfigError(f"{label}: duplicate entry {value!r}")

        seen.add(value)
