"""Reject hidden entity-cover changes reachable from native script exports.

Server scripts own entity-cover acquisition. Before an ordinary native
``FO_SCRIPT_API`` call, the script backend synchronizes every existing entity
the native call graph can read or mutate. Inside that call graph native code may
call ``ValidateEntityAccess`` and ``EnsureEntitySynced`` freely and repeatedly:
they only check that an entity is accessible (``EnsureEntitySynced`` also takes
the entity's own lock when not already held, so a mid-operation parent change
such as a transfer cannot slip it out from under the cover). Both are idempotent
no-ops when the lock is already held, so there are no call-count, loop,
catch, or recursion restrictions on them. What native code must not do is obtain,
replace, or release cover on the script's behalf: it must not call
``SyncEntity`` / ``SyncEntities`` / ``ScopedSyncContext::Sync``, manipulate an
``EntityLock`` directly, probe cover (outside the one reviewed
``Game.IsEntityLocked`` site), or wait/sleep/block/retry while obtaining cover.
An ``EnsureEntitySynced`` failure is a caller error (an omitted dependency), not
something native code may repair. A private trusted registration path may
capture a newly constructed parentless entity before publication.

The audit builds a conservative C++ call graph from server ``ExportMethod``
definitions and registered ``FO_SCRIPT_GENERIC`` callbacks through Engine
server/scripting code and the project native scopes. ``ValidateEntityAccess``
and ``EnsureEntitySynced`` are allowed terminals; the exact cover-retention
terminal bodies are checked separately so they stay non-blocking. The explicit
script synchronization primitives (``Game.Sync``, ``Game.SyncWiden``,
``Game.SyncYield``, ``Game.SyncRelease``, ``Game.Lock``, ``Game.Unlock``, and
``Game.TrySyncEntity``) are exact-identity exempt roots, not general-purpose
allowlisted helpers. The read-only ``Game.IsEntityLocked`` root remains audited
and may only use its exact non-diagnosing access probe. Exact trusted
registration/connection functions may use the private
``EnsureFreshEntitySynced`` entry point for a fresh parentless entity, but
remain in the call graph: registration is not permission to hide acquisition of
any pre-existing dependency. Exact reviewed infrastructure entry points may
construct an empty ``ScopedSyncContext``; any call that actually acquires or
releases entity cover remains visible and forbidden.

Engine rules are built in and name Engine-relative identities. A project adds
its own reviewed independent job schedulers, typed receiver factories and
deferred-callback APIs through the configuration file.
"""

from __future__ import annotations

import json
import re
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from . import cpp_parser as parser
from .scope import ENGINE_ORIGIN, PROJECT_ORIGIN, AuditContext, SourceFile, engine_files, project_files, resolve_input


ENGINE_SCAN_DIRECTORIES = (
    ("Source/Common", (".h", ".hpp")),
    ("Source/Server", (".cpp", ".h", ".hpp")),
    ("Source/Scripting", (".cpp", ".h", ".hpp")),
)
PROJECT_SUFFIXES = (".cpp", ".h", ".hpp")


def _engine(path: str, name: str) -> tuple[str, str, str]:
    return ENGINE_ORIGIN, path, name


EXPLICIT_SYNC_EXPORT_IDENTITIES = {
    _engine("Source/Scripting/ServerGlobalScriptMethods.cpp", "Server_Game_Sync"),
    _engine("Source/Scripting/ServerGlobalScriptMethods.cpp", "Server_Game_SyncRelease"),
    # In-place cover widening: the same Game.Sync family primitive that keeps the held locks
    _engine("Source/Scripting/ServerGlobalScriptMethods.cpp", "Server_Game_SyncWiden"),
    # In-place cover yield: releases the held locks and takes the same set back without adding to it
    _engine("Source/Scripting/ServerGlobalScriptMethods.cpp", "Server_Game_SyncYield"),
    _engine("Source/Scripting/ServerGlobalScriptMethods.cpp", "Server_Game_Lock"),
    _engine("Source/Scripting/ServerGlobalScriptMethods.cpp", "Server_Game_Unlock"),
    # Resolving an id to a live entity and taking cover for it IS this export's script-visible purpose --
    # it is the id-keyed member of the same family, not a gameplay call that happens to synchronize
    _engine("Source/Scripting/ServerGlobalScriptMethods.cpp", "Server_Game_TrySyncEntity"),
}

IS_ENTITY_LOCKED_EXPORT_IDENTITY = _engine("Source/Scripting/ServerGlobalScriptMethods.cpp", "Server_Game_IsEntityLocked")

ENTITY_ACCESS_PROBE_IDENTITY = _engine("Source/Server/EntitySync.cpp", "IsEntityAccessValid")

ALLOWED_TERMINAL_IDENTITIES = {
    _engine("Source/Common/Entity.h", "ValidateEntityAccess"),
    _engine("Source/Server/EntitySync.cpp", "EnsureEntitySynced"),
    _engine("Source/Server/EntitySync.cpp", "SyncContext::EnsureEntitySynced"),
}

FRESH_ENSURE_TERMINAL_IDENTITY = _engine("Source/Server/EntitySync.cpp", "SyncContext::EnsureFreshEntitySynced")

TRUSTED_FRESH_CAPTURE_CALLER_IDENTITIES = {
    _engine("Source/Server/EntityManager.cpp", "EntityManager::CaptureFreshEntity"),
    _engine("Source/Server/Server.cpp", "ServerEngine::CreateNotLoggedInPlayer"),
    _engine("Source/Server/Server.cpp", "ServerEngine::OnPlayerConnected"),
}

PROPERTY_LOCK_METHOD_TERMINAL_IDENTITIES = {
    # Property serialization uses the engine singleton lock, not the script's
    # entity cover.  It has its own contract and must not make entity-package
    # paths look like hidden SyncContext expansion.  Keep the targets exact:
    # an unrelated class must not inherit this exemption merely by reusing a
    # property-lock method name
    _engine("Source/Common/Entity.h", "Entity::LockForPropertyAccess"),
    _engine("Source/Common/Entity.h", "Entity::LockForPropertyAccessShared"),
    _engine("Source/Common/Entity.h", "Entity::UnlockForPropertyAccess"),
    _engine("Source/Common/Entity.h", "Entity::UnlockForPropertyAccessShared"),
    _engine("Source/Server/Server.cpp", "ServerEngine::LockForPropertyAccess"),
    _engine("Source/Server/Server.cpp", "ServerEngine::LockForPropertyAccessShared"),
    _engine("Source/Server/Server.cpp", "ServerEngine::UnlockForPropertyAccess"),
    _engine("Source/Server/Server.cpp", "ServerEngine::UnlockForPropertyAccessShared"),
}

SCOPED_SYNC_CONTEXT_ALLOWED_CALLER_IDENTITIES = {
    # These exact entry points create fresh/infrastructure contexts rather
    # than replacing a script caller's prepared entity cover
    _engine("Source/Server/MapManager.cpp", "MapManager::CreateLocation"),
    _engine("Source/Server/Server.cpp", "ServerEngine::OnPlayerConnected"),
    # Script execution needs an empty nested context so explicit Game.Sync /
    # Game.Lock calls have a context to mutate.  Construction alone takes no
    # entity lock; any Sync/SyncEntity/SyncEntities call remains a sink
    _engine("Source/Server/Server.cpp", "ServerEngine::RunScriptContext"),
}

# These are synchronization mechanisms, not every function that happens to
# wait for unrelated I/O.  EntityLock acquisition and the SyncContext helpers
# below are exactly where missing entity cover can block, retry, release, or be
# replaced.  ScopedSyncContext::Sync is inline in EntitySync.h, so it is listed
# explicitly even though the .cpp function parser has no node for it
FORBIDDEN_SINKS = {
    "EntityLock::Acquire": "blocking entity-lock acquisition",
    "EntityLock::AcquireShared": "blocking shared entity-lock acquisition",
    "EntityLock::Release": "entity-lock release",
    "EntityLock::ReleaseShared": "shared entity-lock release",
    "EntityLock::RegisterDescendantHold": "blocking ancestor-hold acquisition",
    "EntityLock::TryAcquire": "hidden entity-lock acquisition",
    "EntityLock::TryRegisterDescendantHold": "hidden ancestor-hold acquisition",
    "EntityLock::UnregisterDescendantHold": "ancestor-hold release",
    "ScopedSyncContext::Sync": "nested entity-cover acquisition",
    "ServerEngine::Lock": "blocking engine/entity-lock acquisition",
    "ServerEngine::SyncWholeWorld": "whole-world entity-cover acquisition",
    "ServerEngine::Unlock": "native sync-context release",
    "SyncContext::AcquireLocks": "blocking entity-cover acquisition",
    "SyncContext::AcquireLocksOrderedFair": "blocking entity-cover reacquisition",
    "SyncContext::Activate": "native sync-context activation",
    "SyncContext::Deactivate": "native sync-context deactivation",
    "SyncContext::LockSingleton": "blocking singleton-lock acquisition",
    "SyncContext::Release": "entity-cover release",
    "SyncContext::ReleaseLocks": "entity-cover release",
    "SyncContext::ReleaseSingletonLocks": "singleton-lock release",
    "SyncContext::SyncEntities": "blocking entity-cover replacement",
    "SyncContext::SyncEntity": "blocking entity-cover replacement",
    "SyncContext::ValidateAccess": "native cover probing instead of strict access validation",
    "SyncContext::UnlockSingleton": "singleton-lock release",
}

CONTROL_CALLS = {
    "alignof",
    "catch",
    "co_await",
    "decltype",
    "delete",
    "for",
    "if",
    "new",
    "noexcept",
    "requires",
    "return",
    "sizeof",
    "static_assert",
    "switch",
    "throw",
    "typeid",
    "while",
}

ENGINE_DEFERRED_CALLBACK_CALLS = frozenset(
    {
        "add_job",
        "SetDataArrivedCallback",
        "Submit",
        "run_thread",
    }
)

# A callback submitted by one of these exact engine schedulers starts later in
# a fresh top-level worker context.  Its own SyncEntity/SyncEntities call is the
# job-entry acquisition, not a hidden change to the script caller's context.
# Every other callback remains part of the script-reachable call graph even when
# passed to Submit/add_job/run_thread: adding a deferred hop is not an escape from
# the native-boundary contract and must be reviewed before joining this list.
# Project schedulers are reviewed in the project configuration file
ENGINE_INDEPENDENT_TOP_LEVEL_JOB_SCHEDULER_IDENTITIES = frozenset(
    {
        _engine("Source/Server/Server.cpp", "ServerEngine::OnPlayerConnected"),
        _engine("Source/Server/Server.cpp", "ServerEngine::OnPlayerLoggedIn"),
        _engine("Source/Server/Server.cpp", "ServerEngine::OnTimeEventSchedule"),
        _engine("Source/Server/Server.cpp", "ServerEngine::ScheduleDelayedCallback"),
        _engine("Source/Server/Server.cpp", "ServerEngine::StartCritterMoving"),
    }
)

# Only the inherited receiver in this exact owner schedules the reviewed independent job
# Different or shadowed receivers remain in the caller's audited path
SPECTATOR_REFRESH_SCHEDULER_IDENTITY = _engine("Source/Server/Map.cpp", "Map::ScheduleSpectatorRefresh")
SPECTATOR_REFRESH_SCHEDULER_CALL = "_engine->ScheduleDelayedCallback"

CALL_RE = re.compile(
    r"(?<![A-Za-z0-9_])"
    r"(?P<target>[A-Za-z_][A-Za-z0-9_]*"
    r"(?:\s*(?:::|->|\.)\s*[A-Za-z_][A-Za-z0-9_]*)*)\s*\("
)
SCRIPT_EXPORT_RE = re.compile(
    r"\bFO_SCRIPT_API\b[^;{]*?\b(?P<name>Server_[A-Za-z_][A-Za-z0-9_]*)\s*\("
)
SCRIPT_GENERIC_RE = re.compile(
    r"\bFO_SCRIPT_GENERIC\s*\(\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\)"
)
WRAPPED_TYPE_RE = re.compile(
    r"\b(?:ptr|nptr|refcount_ptr|refcount_nptr|unique_ptr|unique_nptr|shared_ptr|weak_ptr)"
    r"\s*<\s*(?:const\s+)?(?P<type>[A-Za-z_][A-Za-z0-9_:]*)[^>]*>"
    r"\s*(?:const\s+)?(?P<name>[A-Za-z_][A-Za-z0-9_]*)\b"
)
DIRECT_TYPE_RE = re.compile(
    r"\b(?:const\s+)?(?P<type>Entity|SyncContext|ScopedSyncContext|EntityLock|ServerEngine|"
    r"CritterManager|EntityManager|ItemManager|MapManager)"
    r"\s*(?:[*&]\s*)?(?P<name>[A-Za-z_][A-Za-z0-9_]*)\b"
)
AUTO_SYNC_CONTEXT_RE = re.compile(
    r"\b(?:const\s+)?auto\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{)\s*"
    r"[^;]*(?:RequireCurrentSyncContext|GetCurrentSyncContext|GetCurrentOnThisThread|GetOutermostOnThisThread)\s*\("
)
AUTO_ENTITY_LOCK_RE = re.compile(
    r"\b(?:const\s+)?auto\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{)\s*"
    r"[^;]*Get(?:Entity|Owned)Lock\s*\("
)
AUTO_MANAGER_RE = re.compile(
    r"\b(?:const\s+)?auto\s*&?\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{)\s*"
    r"[^;\n]*(?:->|\.)?(?P<field>CrMngr|EntityMngr|ItemMngr|MapMngr)\s*\}?\s*;"
)
AUTO_DYN_CAST_RE = re.compile(
    r"\b(?:const\s+)?auto\s*&?\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{)\s*"
    r"[^;\n]*?\.(?:template\s+)?dyn_cast\s*<\s*(?:const\s+)?"
    r"(?P<type>[A-Za-z_][A-Za-z0-9_:]*)\s*>\s*\("
)
AUTO_POINTER_ALIAS_RE = re.compile(
    r"\b(?:const\s+)?auto\s*(?:[*&]\s*)?(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{)\s*"
    r"\s*(?P<source>[A-Za-z_][A-Za-z0-9_]*)\s*\."
    r"(?:as_ptr|as_nptr|hold_ref|try_hold_ref|get)\s*\("
)
AUTO_KNOWN_ALIAS_RE = re.compile(
    r"\b(?:const\s+)?auto\s*(?:[*&]\s*)?(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{)\s*"
    r"\s*(?:&\s*)?(?:\*\s*)?(?P<source>[A-Za-z_][A-Za-z0-9_]*)\s*\}?\s*;"
)
DECLTYPE_ALIAS_RE = re.compile(
    r"\bdecltype\s*\(\s*(?P<type_source>[A-Za-z_][A-Za-z0-9_]*)\s*\)"
    r"\s*(?:[*&]\s*)?(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{)\s*"
    r"\s*(?P<source>[A-Za-z_][A-Za-z0-9_]*)\s*\}?\s*;"
)
AUTO_TEMPLATE_FACTORY_RE = re.compile(
    r"\b(?:const\s+)?auto\s+&?\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{)\s*"
    r"[^;\n]*?\bGetGenericObjectAs\s*<\s*(?:const\s+)?"
    r"(?P<type>[A-Za-z_][A-Za-z0-9_:]*)\s*>\s*\("
)
SCOPED_SYNC_CONTEXT_DECL_RE = re.compile(
    r"\bScopedSyncContext\s+[A-Za-z_][A-Za-z0-9_]*\s*"
    r"(?:\([^;{}]*\)|\{[^;{}]*\})?\s*;"
)

SERVER_ENGINE_MANAGER_FIELDS = {
    "CrMngr": "CritterManager",
    "EntityMngr": "EntityManager",
    "ItemMngr": "ItemManager",
    "MapMngr": "MapManager",
}

SERVER_ENTITY_TYPES = frozenset(
    {
        "Critter",
        "CustomEntity",
        "Item",
        "Location",
        "Map",
        "Player",
        "ServerEntity",
    }
)
ENTITY_TYPES = SERVER_ENTITY_TYPES | frozenset({"Entity"})


@dataclass(frozen=True)
class SyncRules:
    """Built-in Engine rules plus the reviewed project additions from the configuration file."""

    independent_job_schedulers: frozenset[tuple[str, str, str]]
    deferred_callback_calls: frozenset[str]
    typed_factories: tuple[tuple[str, str], ...]

    @property
    def typed_factory_re(self) -> re.Pattern[str] | None:
        if not self.typed_factories:
            return None

        names = "|".join(re.escape(name) for name, _ in self.typed_factories)

        return re.compile(
            r"\b(?:const\s+)?auto\s+&?\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{)\s*"
            rf"[^;\n]*?\b(?P<factory>{names})\s*\("
        )


ENGINE_RULES = SyncRules(
    independent_job_schedulers=ENGINE_INDEPENDENT_TOP_LEVEL_JOB_SCHEDULER_IDENTITIES,
    deferred_callback_calls=ENGINE_DEFERRED_CALLBACK_CALLS,
    typed_factories=(),
)


def rules_for(context: AuditContext) -> SyncRules:
    """Combine the Engine rules with the project additions of the context configuration."""

    config = context.config.sync_scope

    return SyncRules(
        independent_job_schedulers=ENGINE_INDEPENDENT_TOP_LEVEL_JOB_SCHEDULER_IDENTITIES
        | {(PROJECT_ORIGIN, rule.file, rule.function) for rule in config.independent_job_schedulers},
        deferred_callback_calls=ENGINE_DEFERRED_CALLBACK_CALLS | {rule.name for rule in config.deferred_callback_calls},
        typed_factories=tuple((rule.name, rule.type) for rule in config.typed_factories),
    )


@dataclass(frozen=True)
class Function:
    key: str
    name: str
    path: Path
    line: int
    signature: str
    body: str
    body_line: int
    is_script_export: bool
    origin: str = ENGINE_ORIGIN
    unit_path: str = ""

    @property
    def basename(self) -> str:
        return self.name.rsplit("::", 1)[-1]

    @property
    def owner(self) -> str | None:
        return self.name.rsplit("::", 1)[0] if "::" in self.name else None


def function_identity(function: Function) -> tuple[str, str, str]:
    return function.origin, function.unit_path, function.name


def candidates_are_exact_terminals(
    candidates: Iterable[Function],
    identities: set[tuple[str, str, str]] | frozenset[tuple[str, str, str]],
) -> bool:
    candidates = tuple(candidates)
    return bool(candidates) and all(
        function_identity(candidate) in identities for candidate in candidates
    )


@dataclass(frozen=True)
class Call:
    target: str
    line: int
    offset: int
    arguments: tuple[str, ...] = ()
    implicit_member: bool = False
    owner_hint: str | None = None

    @property
    def basename(self) -> str:
        normalized = re.sub(r"\s+", "", self.target)
        return re.split(r"::|->|\.", normalized)[-1]

    @property
    def is_member_call(self) -> bool:
        return self.implicit_member or "->" in self.target or "." in self.target


@dataclass(frozen=True)
class Edge:
    source: str
    target: str
    line: int
    offset: int
    arguments: tuple[str, ...]


@dataclass(frozen=True)
class Diagnostic:
    root: Function
    sink: str
    reason: str
    path: tuple[Function, ...]
    call_lines: tuple[int, ...]

    def to_dict(self) -> dict[str, object]:
        return {
            "path": str(self.root.path.as_posix()),
            "line": self.root.line,
            "export": self.root.name,
            "sink": self.sink,
            "reason": self.reason,
            "call_path": [function.name for function in self.path] + [self.sink],
            "call_lines": list(self.call_lines),
        }


@dataclass(frozen=True)
class EnsureImplementationViolation:
    function: Function
    line: int
    target: str
    reason: str


def nearby_export_method(lines: list[str], line_index: int) -> bool:
    for index in range(line_index - 1, max(-1, line_index - 6), -1):
        stripped = lines[index].strip()
        if "///@ ExportMethod" in stripped:
            return True
        if stripped and not stripped.startswith("//"):
            return False
    return False


def declared_script_exports(text: str) -> set[str]:
    lines = text.splitlines()
    result: set[str] = set()
    for index, line in enumerate(lines):
        match = SCRIPT_EXPORT_RE.search(line)
        if match is not None and nearby_export_method(lines, index):
            result.add(match.group("name"))
    return result


def declared_script_generics(text: str) -> set[str]:
    return {match.group("name") for match in SCRIPT_GENERIC_RE.finditer(text)}


def find_matching_paren(code: str, open_offset: int) -> int:
    depth = 0
    for index in range(open_offset, len(code)):
        if code[index] == "(":
            depth += 1
        elif code[index] == ")":
            depth -= 1
            if depth == 0:
                return index
    return -1


def skip_whitespace(code: str, offset: int) -> int:
    while offset < len(code) and code[offset].isspace():
        offset += 1
    return offset


def statement_end(code: str, start: int) -> int:
    """Return the inclusive end offset of one masked C++ statement/body."""

    start = skip_whitespace(code, start)
    if start >= len(code):
        return len(code) - 1
    if code[start] == "{":
        depth = 0
        for offset in range(start, len(code)):
            if code[offset] == "{":
                depth += 1
            elif code[offset] == "}":
                depth -= 1
                if depth == 0:
                    return offset
        return len(code) - 1

    paren_depth = 0
    bracket_depth = 0
    brace_depth = 0
    for offset in range(start, len(code)):
        char = code[offset]
        if char == "(":
            paren_depth += 1
        elif char == ")":
            paren_depth = max(0, paren_depth - 1)
        elif char == "[":
            bracket_depth += 1
        elif char == "]":
            bracket_depth = max(0, bracket_depth - 1)
        elif char == "{":
            brace_depth += 1
        elif char == "}":
            if brace_depth == 0:
                return max(start, offset - 1)
            brace_depth -= 1
        elif char == ";" and paren_depth == bracket_depth == brace_depth == 0:
            return offset
    return len(code) - 1


def split_top_level(text: str, separator: str = ",") -> list[str]:
    parts: list[str] = []
    start = 0
    paren_depth = 0
    bracket_depth = 0
    brace_depth = 0
    angle_depth = 0
    for index, char in enumerate(text):
        if char == "(":
            paren_depth += 1
        elif char == ")":
            paren_depth = max(0, paren_depth - 1)
        elif char == "[":
            bracket_depth += 1
        elif char == "]":
            bracket_depth = max(0, bracket_depth - 1)
        elif char == "{":
            brace_depth += 1
        elif char == "}":
            brace_depth = max(0, brace_depth - 1)
        elif char == "<":
            angle_depth += 1
        elif char == ">":
            angle_depth = max(0, angle_depth - 1)
        elif (
            char == separator
            and not (separator == ":" and (text[index - 1 : index] == ":" or text[index + 1 : index + 2] == ":"))
            and paren_depth == bracket_depth == brace_depth == angle_depth == 0
        ):
            parts.append(text[start:index].strip())
            start = index + 1
    parts.append(text[start:].strip())
    return parts


def normalized_entity_expression(expression: str) -> str:
    normalized = re.sub(r"\s+", "", expression)
    while normalized.startswith("(") and normalized.endswith(")"):
        close = find_matching_paren(normalized, 0)
        if close != len(normalized) - 1:
            break
        normalized = normalized[1:-1]
    normalized = re.sub(
        r"\.(?:as_ptr|as_nptr|hold_ref|try_hold_ref)\(\)",
        "",
        normalized,
    )
    return normalized


def identifier_expression(expression: str) -> str | None:
    normalized = normalized_entity_expression(expression)
    return normalized if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", normalized) else None


def locate_markerless_export(
    parsed: parser.ParsedFile,
    source: SourceFile,
    name: str,
) -> Function | None:
    pattern = re.compile(rf"\b{re.escape(name)}\s*\(")
    for match in pattern.finditer(parsed.code_mask):
        open_paren = parsed.code_mask.find("(", match.start())
        close_paren = find_matching_paren(parsed.code_mask, open_paren)
        if close_paren == -1:
            continue
        body_open = parser.find_body_brace(parsed.code_mask, close_paren + 1)
        if body_open == -1:
            continue
        body_close = parser.match_brace(parsed.code_mask, body_open)
        if body_close == -1:
            continue

        line_index = parser.offset_to_line(parsed.offsets, match.start())
        signature_start = parsed.offsets[line_index]
        signature = parser.normalize_ws(parsed.hash_mask[signature_start:body_open])
        return Function(
            key=f"{source.display}:{line_index + 1}:{signature}",
            name=name,
            path=Path(source.display),
            line=line_index + 1,
            signature=signature,
            body=parsed.code_mask[body_open + 1 : body_close],
            body_line=parser.offset_to_line(parsed.offsets, body_open) + 1,
            is_script_export=True,
            origin=source.origin,
            unit_path=source.unit_path,
        )
    return None


def collect_markerless_header_functions(
    parsed: parser.ParsedFile,
    source: SourceFile,
) -> list[Function]:
    """Collect simple inline/header definitions omitted by the marker parser."""

    result: list[Function] = []
    definition_re = re.compile(
        r"\b(?P<name>[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)*)\s*\("
    )
    for match in definition_re.finditer(parsed.code_mask):
        name = match.group("name")
        if name in CONTROL_CALLS or name.startswith("FO_"):
            continue
        open_paren = parsed.code_mask.find("(", match.start(), match.end())
        close_paren = find_matching_paren(parsed.code_mask, open_paren)
        if close_paren == -1:
            continue
        body_open = parser.find_body_brace(parsed.code_mask, close_paren + 1)
        if body_open == -1:
            continue
        trailer = parsed.code_mask[close_paren + 1 : body_open]
        if ";" in trailer or "=" in trailer:
            continue
        body_close = parser.match_brace(parsed.code_mask, body_open)
        if body_close == -1:
            continue

        line_index = parser.offset_to_line(parsed.offsets, match.start())
        line_start = parsed.offsets[line_index]
        prefix = parsed.code_mask[line_start : match.start()]
        if not re.search(
            r"(?:^|\s)(?:inline\s+)?(?:static\s+)?(?:auto|void|bool|[A-Za-z_][A-Za-z0-9_:<>]*)"
            r"(?:\s+|\s*[*&]\s*)$",
            prefix,
        ):
            continue
        signature = parser.normalize_ws(parsed.hash_mask[line_start:body_open])
        result.append(
            Function(
                key=(
                    f"{source.display}:"
                    f"{line_index + 1}:{signature}"
                ),
                name=name,
                path=Path(source.display),
                line=line_index + 1,
                signature=signature,
                body=parsed.code_mask[body_open + 1 : body_close],
                body_line=parser.offset_to_line(parsed.offsets, body_open) + 1,
                is_script_export=False,
                origin=source.origin,
                unit_path=source.unit_path,
            )
        )
    return result



def source_files(context: AuditContext) -> tuple[list[SourceFile], list[str]]:
    """Return the Engine and project inputs plus stale-exclusion errors."""

    files = engine_files(context, ENGINE_SCAN_DIRECTORIES)
    project, stale = project_files(context, PROJECT_SUFFIXES)

    return files + project, stale


def collect_functions(sources: Iterable[SourceFile]) -> list[Function]:
    functions: list[Function] = []
    seen: set[tuple[str, int, str]] = set()

    for source in sources:
        text = source.path.read_text(encoding="utf-8-sig", errors="replace")
        parsed = parser.parse_file(source.display, text)
        export_names = declared_script_exports(text)
        generic_names = declared_script_generics(text)
        script_root_names = export_names | generic_names
        display_path = Path(source.display)

        # Definitions are found by shape: a zone marker is instrumentation and says nothing about the call graph
        claimed = {entry.body_off for entry in parsed.functions}
        entries = parsed.functions + [entry for entry in parsed.definitions if entry.body_off not in claimed]
        for entry in entries:
            body_open = parser.find_body_brace(parsed.code_mask, parsed.offsets[entry.line - 1])
            if body_open == -1:
                continue
            body_close = parser.match_brace(parsed.code_mask, body_open)
            if body_close == -1:
                continue
            identity = (source.display, entry.line, entry.signature)
            if identity in seen:
                continue
            seen.add(identity)
            functions.append(
                Function(
                    key=f"{source.display}:{entry.line}:{entry.signature}",
                    name=entry.function,
                    path=display_path,
                    line=entry.line,
                    signature=entry.signature,
                    body=parsed.code_mask[body_open + 1 : body_close],
                    body_line=parser.offset_to_line(parsed.offsets, body_open) + 1,
                    is_script_export=entry.function in script_root_names,
                    origin=source.origin,
                    unit_path=source.unit_path,
                )
            )

        if source.path.suffix in {".h", ".hpp"}:
            for function in collect_markerless_header_functions(parsed, source):
                identity = (source.display, function.line, function.signature)
                if identity not in seen:
                    seen.add(identity)
                    functions.append(function)

        parsed_names = {function.name for function in functions if function.path == display_path}
        for name in sorted(export_names - parsed_names):
            function = locate_markerless_export(parsed, source, name)
            if function is None:
                continue
            identity = (source.display, function.line, function.signature)
            if identity not in seen:
                seen.add(identity)
                functions.append(function)

    return functions


def declaration_value_type(declaration: str) -> str | None:
    declaration = split_top_level(declaration, "=")[0].strip()
    sequence_match = re.search(
        r"\b(?:const_span|readonly_vector|span|vector)\s*<(?P<value>.+)>\s*"
        r"(?:const\s*)?(?:[&*]\s*)?[A-Za-z_][A-Za-z0-9_]*\s*$",
        declaration,
    )
    if sequence_match is not None:
        wrapped = re.search(
            r"\b(?:ptr|nptr|refcount_ptr|refcount_nptr|unique_ptr|unique_nptr|shared_ptr|weak_ptr)"
            r"\s*<\s*(?:const\s+)?(?P<type>[A-Za-z_][A-Za-z0-9_:]*)",
            sequence_match.group("value"),
        )
        if wrapped is not None:
            return f"sequence<{wrapped.group('type').rsplit('::', 1)[-1]}>"

    wrapped = re.search(
        r"\b(?:ptr|nptr|refcount_ptr|refcount_nptr|unique_ptr|unique_nptr|shared_ptr|weak_ptr)"
        r"\s*<\s*(?:const\s+)?(?P<type>[A-Za-z_][A-Za-z0-9_:]*)",
        declaration,
    )
    if wrapped is not None:
        return wrapped.group("type").rsplit("::", 1)[-1]

    direct = re.search(
        r"(?P<type>[A-Za-z_][A-Za-z0-9_:]*)\s*(?:const\s*)?(?:[&*]\s*)?"
        r"[A-Za-z_][A-Za-z0-9_]*\s*$",
        declaration,
    )
    return direct.group("type").rsplit("::", 1)[-1] if direct is not None else None


def collect_variable_types(function: Function, rules: SyncRules = ENGINE_RULES) -> dict[str, str]:
    text = function.signature + "\n" + function.body
    result: dict[str, str] = {}
    for name, declaration in function_parameter_declarations(function):
        value_type = declaration_value_type(declaration)
        if name is not None and value_type is not None:
            result[name] = value_type
    for match in WRAPPED_TYPE_RE.finditer(text):
        result[match.group("name")] = match.group("type").rsplit("::", 1)[-1]
    for match in DIRECT_TYPE_RE.finditer(text):
        result[match.group("name")] = match.group("type")
    for match in AUTO_SYNC_CONTEXT_RE.finditer(text):
        result[match.group("name")] = "SyncContext"
    for match in AUTO_ENTITY_LOCK_RE.finditer(text):
        result[match.group("name")] = "EntityLock"
    for match in AUTO_MANAGER_RE.finditer(text):
        result[match.group("name")] = SERVER_ENGINE_MANAGER_FIELDS[match.group("field")]
    for match in AUTO_DYN_CAST_RE.finditer(text):
        result[match.group("name")] = match.group("type").rsplit("::", 1)[-1]
    for match in AUTO_TEMPLATE_FACTORY_RE.finditer(text):
        result[match.group("name")] = match.group("type").rsplit("::", 1)[-1]
    factory_re = rules.typed_factory_re
    if factory_re is not None:
        factory_types = dict(rules.typed_factories)
        for match in factory_re.finditer(text):
            result[match.group("name")] = factory_types[match.group("factory")]
    for match in re.finditer(
        r"\bauto\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{)\s*"
        r"[^;]+(?:->|\.)GetControlledCritter\s*\(",
        text,
    ):
        result[match.group("name")] = "Critter"
    changed = True
    while changed:
        changed = False
        for alias_re in (AUTO_POINTER_ALIAS_RE, AUTO_KNOWN_ALIAS_RE):
            for match in alias_re.finditer(text):
                source_type = result.get(match.group("source"))
                if source_type is not None and match.group("name") not in result:
                    result[match.group("name")] = source_type
                    changed = True
        for match in DECLTYPE_ALIAS_RE.finditer(text):
            source_type = result.get(match.group("type_source"))
            if source_type is not None and match.group("name") not in result:
                result[match.group("name")] = source_type
                changed = True
    return result


def function_accepts_argument_count(function: Function, count: int) -> bool:
    open_paren = function.signature.find("(")
    if open_paren == -1:
        return True
    close_paren = find_matching_paren(function.signature, open_paren)
    if close_paren == -1:
        return True

    parameters = split_top_level(function.signature[open_paren + 1 : close_paren])
    if parameters == [""] or parameters == ["void"]:
        parameters = []
    if any(parameter.strip() == "..." for parameter in parameters):
        maximum = None
    else:
        maximum = len(parameters)
    minimum = sum(
        1
        for parameter in parameters
        if parameter.strip() != "..." and len(split_top_level(parameter, "=")) == 1
    )
    return count >= minimum and (maximum is None or count <= maximum)


def expression_value_type(
    expression: str,
    variable_types: dict[str, str],
) -> str | None:
    expression = normalized_entity_expression(expression)
    identifier = identifier_expression(expression)
    if identifier is not None:
        return variable_types.get(identifier)
    if expression in {"false", "true"}:
        return "bool"
    if expression == "nullptr":
        return "nullptr"
    return None


def type_conversion_score(actual: str | None, expected: str | None) -> int | None:
    if actual is None or expected is None or actual == "nullptr":
        return 0
    if actual == expected:
        return 2
    actual_sequence = re.fullmatch(r"sequence<(?P<type>[^>]+)>", actual)
    expected_sequence = re.fullmatch(r"sequence<(?P<type>[^>]+)>", expected)
    if actual_sequence is not None or expected_sequence is not None:
        if actual_sequence is None or expected_sequence is None:
            return None
        return type_conversion_score(
            actual_sequence.group("type"),
            expected_sequence.group("type"),
        )
    if actual in SERVER_ENTITY_TYPES and expected in {"Entity", "ServerEntity"}:
        return 1
    if actual in ENTITY_TYPES and expected in ENTITY_TYPES:
        return None
    return 0


def matching_call_candidates(
    call: Call,
    candidates: Iterable[Function],
    caller: Function | None = None,
    variable_types: dict[str, str] | None = None,
    rules: SyncRules = ENGINE_RULES,
) -> list[Function]:
    argument_count = 0 if call.arguments == ("",) else len(call.arguments)
    matches = [
        candidate
        for candidate in candidates
        if function_accepts_argument_count(candidate, argument_count)
    ]
    if caller is None or not matches:
        return matches
    if variable_types is None:
        variable_types = collect_variable_types(caller, rules)

    actual_types = tuple(
        expression_value_type(argument, variable_types)
        for argument in call.arguments
    )
    scored: list[tuple[int, Function]] = []
    for candidate in matches:
        declarations = function_parameter_declarations(candidate)
        expected_types = tuple(
            declaration_value_type(declaration)
            for _, declaration in declarations
        )
        scores = [
            type_conversion_score(actual, expected)
            for actual, expected in zip(actual_types, expected_types)
        ]
        if any(score is None for score in scores):
            continue
        scored.append((sum(score for score in scores if score is not None), candidate))
    if not scored:
        return []
    best_score = max(score for score, _ in scored)
    return [candidate for score, candidate in scored if score == best_score]


def collect_calls(function: Function, rules: SyncRules = ENGINE_RULES) -> list[Call]:
    calls: list[Call] = []
    body = mask_deferred_lambda_bodies(function, rules)
    variable_types = collect_variable_types(function, rules)
    for match in CALL_RE.finditer(body):
        target = re.sub(r"\s+", "", match.group("target"))
        basename = re.split(r"::|->|\.", target)[-1]
        if basename in CONTROL_CALLS:
            continue
        line = function.body_line + body.count("\n", 0, match.start())
        open_paren = match.end() - 1
        close_paren = find_matching_paren(body, open_paren)
        arguments = (
            tuple(split_top_level(body[open_paren + 1 : close_paren]))
            if close_paren != -1
            else ()
        )
        prefix = body[max(0, match.start() - 2) : match.start()]
        implicit_member = prefix.endswith("->") or prefix.endswith(".")
        owner_hint = None
        if implicit_member:
            receiver_prefix = body[max(0, match.start() - 160) : match.start()]
            dereferenced_receiver = re.search(
                r"\(\s*(?:&\s*)?\*\s*(?P<receiver>[A-Za-z_][A-Za-z0-9_]*)\s*\)"
                r"\s*(?:->|\.)\s*$",
                receiver_prefix,
            )
            if dereferenced_receiver is not None:
                owner_hint = variable_types.get(dereferenced_receiver.group("receiver"))

            receiver_factory = re.search(
                r"(?:(?P<receiver>[A-Za-z_][A-Za-z0-9_]*)\s*(?P<access>::|->|\.)\s*)?"
                r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\([^()]*\)\s*(?:->|\.)\s*$",
                receiver_prefix,
            )
            if owner_hint is None and receiver_factory is not None:
                factory_name = receiver_factory.group("name")
                receiver = receiver_factory.group("receiver")
                access = receiver_factory.group("access")
                if receiver is None:
                    factory_owner = function.owner
                elif access == "::":
                    factory_owner = receiver
                elif receiver == "_engine":
                    factory_owner = "ServerEngine"
                else:
                    factory_owner = variable_types.get(receiver)
                if (factory_owner, factory_name) in {
                    ("ScopedSyncContext", "GetContext"),
                    ("ServerEngine", "GetCurrentSyncContext"),
                    ("ServerEngine", "RequireCurrentSyncContext"),
                    ("SyncContext", "GetCurrentOnThisThread"),
                }:
                    owner_hint = "SyncContext"
                elif factory_name == "GetEngine":
                    owner_hint = "ServerEngine"
                elif factory_name == "ScopedSyncContext":
                    owner_hint = "ScopedSyncContext"
                elif factory_name in {
                    "as_ptr",
                    "as_nptr",
                    "get",
                    "hold_ref",
                    "try_hold_ref",
                }:
                    owner_hint = factory_owner
        calls.append(
            Call(
                target=target,
                line=line,
                offset=match.start(),
                arguments=arguments,
                implicit_member=implicit_member,
                owner_hint=owner_hint,
            )
        )
    return calls


def enclosing_call_basename(body: str, offset: int) -> str | None:
    depth = 0
    for index in range(offset - 1, -1, -1):
        if body[index] == ")":
            depth += 1
        elif body[index] == "(":
            if depth > 0:
                depth -= 1
                continue
            prefix = body[:index]
            match = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*$", prefix)
            return match.group(1) if match is not None else None
    return None


def find_matching_delimiter(
    code: str,
    open_offset: int,
    end: int,
    opener: str,
    closer: str,
) -> int:
    depth = 0
    for index in range(open_offset, end):
        if code[index] == opener:
            depth += 1
        elif code[index] == closer:
            depth -= 1
            if depth == 0:
                return index
    return -1


def reviewed_spectator_callback_body(
    body: str,
    start: int,
    end: int,
) -> tuple[int, int] | None:
    """Return the exact direct callback body range, or ``None`` when unsure."""

    offset = skip_whitespace(body, start)
    if offset >= end or body[offset] != "[":
        return None
    capture_close = find_matching_delimiter(body, offset, end, "[", "]")
    if capture_close == -1:
        return None
    offset = skip_whitespace(body, capture_close + 1)

    if offset < end and body[offset] == "(":
        parameters_close = find_matching_paren(body, offset)
        if parameters_close == -1 or parameters_close >= end:
            return None
        offset = skip_whitespace(body, parameters_close + 1)

    seen_specifiers: set[str] = set()
    while offset < end:
        offset = skip_whitespace(body, offset)
        if offset >= end:
            break
        if body[offset] == "{":
            break
        specifier = re.match(r"[A-Za-z_][A-Za-z0-9_]*", body[offset:end])
        if specifier is None:
            return None
        name = specifier.group(0)
        offset += len(name)
        if name == "noexcept":
            probe = skip_whitespace(body, offset)
            if probe < end and body[probe] == "(":
                close = find_matching_paren(body, probe)
                if close == -1 or close >= end:
                    return None
                offset = close + 1
        elif name not in {"mutable", "constexpr", "consteval", "FO_DEFERRED"}:
            return None
        if name in seen_specifiers:
            return None
        seen_specifiers.add(name)

    if offset >= end or body[offset] != "{":
        return None
    body_close = find_matching_delimiter(body, offset, end, "{", "}")
    if body_close == -1:
        return None
    if skip_whitespace(body, body_close + 1) != end:
        return None
    return offset, body_close


def mask_spectator_refresh_callback(function: Function) -> str:
    """Mask only the directly passed spectator-refresh deferred callback body."""

    body = function.body
    if any(name == "_engine" for name, _ in function_parameter_declarations(function)):
        return body
    # Another occurrence may shadow the inherited receiver, so keep the callback visible
    if len(re.findall(r"\b_engine\b", body)) != 1:
        return body

    masked = list(body)
    for match in CALL_RE.finditer(body):
        if re.sub(r"\s+", "", match.group("target")) != SPECTATOR_REFRESH_SCHEDULER_CALL:
            continue
        prefix = body[: match.start()].rstrip()
        if prefix and prefix[-1] in ".>:*&":
            continue

        open_paren = match.end() - 1
        close_paren = find_matching_paren(body, open_paren)
        if close_paren == -1:
            continue
        arguments = split_top_level(body[open_paren + 1 : close_paren])
        if len(arguments) != 2:
            continue
        callback = arguments[1]
        callback_start = body.rfind(callback, open_paren + 1, close_paren)
        if callback_start == -1:
            continue
        callback_body = reviewed_spectator_callback_body(
            body,
            callback_start,
            callback_start + len(callback),
        )
        if callback_body is None:
            continue
        body_open, body_close = callback_body
        for index in range(body_open + 1, body_close):
            if masked[index] != "\n":
                masked[index] = " "
    return "".join(masked)


def mask_deferred_lambda_bodies(function: Function, rules: SyncRules = ENGINE_RULES) -> str:
    """Blank approved top-level job bodies while preserving offsets/newlines.

    Merely passing a lambda to a deferred API does not remove it from the
    script-reached call path: ordinary callbacks remain visible to the audit.
    Only exact reviewed schedulers whose callback is an independent top-level
    worker entry may hide that callback's job-entry acquisition. The spectator
    refresh owner additionally requires its direct ``_engine`` receiver and an
    exact recognized callback. Lambdas used by synchronous helpers (including
    ``scope_exit``/``scope_fail`` and immediately-invoked lambdas) are never
    blanked.
    """

    body = function.body
    if function_identity(function) == SPECTATOR_REFRESH_SCHEDULER_IDENTITY:
        return mask_spectator_refresh_callback(function)
    if function_identity(function) not in rules.independent_job_schedulers:
        return body

    masked = list(body)
    lambda_re = re.compile(
        r"\[[^\]\n]*\]\s*"
        r"(?:\([^{};]*\)\s*)?"
        r"(?:mutable\s*)?(?:noexcept(?:\s*\([^)]*\))?\s*)?"
        r"(?:->[^{;]+\s*)?\{"
    )
    search_offset = 0
    while True:
        match = lambda_re.search(body, search_offset)
        if match is None:
            break
        open_brace = body.find("{", match.start(), match.end())
        if open_brace == -1:
            break
        depth = 0
        close_brace = -1
        for index in range(open_brace, len(body)):
            if body[index] == "{":
                depth += 1
            elif body[index] == "}":
                depth -= 1
                if depth == 0:
                    close_brace = index
                    break
        if close_brace == -1:
            break
        if enclosing_call_basename(body, match.start()) not in rules.deferred_callback_calls:
            search_offset = close_brace + 1
            continue
        for index in range(open_brace + 1, close_brace):
            if masked[index] != "\n":
                masked[index] = " "
        search_offset = close_brace + 1
    return "".join(masked)


def typed_call_target(call: Call, variable_types: dict[str, str]) -> str | None:
    target = re.sub(r"\s+", "", call.target)
    if "::" in target and "->" not in target and "." not in target:
        return target

    chain_parts = re.split(r"->|\.", target)
    if len(chain_parts) >= 2:
        manager_owner = SERVER_ENGINE_MANAGER_FIELDS.get(chain_parts[-2])
        if manager_owner is not None:
            return f"{manager_owner}::{chain_parts[-1]}"

    if call.owner_hint is not None:
        return f"{call.owner_hint}::{call.basename}"

    member_match = re.fullmatch(
        r"(?P<receiver>[A-Za-z_][A-Za-z0-9_]*)(?:->|\.)(?P<method>[A-Za-z_][A-Za-z0-9_]*)",
        target,
    )
    if member_match is None:
        return None
    receiver = member_match.group("receiver")
    method = member_match.group("method")
    owner = variable_types.get(receiver)
    if owner is None and receiver == "_engine" and method in {
        "LockForPropertyAccess",
        "LockForPropertyAccessShared",
        "UnlockForPropertyAccess",
        "UnlockForPropertyAccessShared",
    }:
        owner = "ServerEngine"
    if owner is None:
        return None
    return f"{owner}::{method}"


def resolve_calls(
    function: Function,
    functions_by_name: dict[str, list[Function]],
    functions_by_basename: dict[str, list[Function]],
    rules: SyncRules = ENGINE_RULES,
) -> tuple[list[Edge], list[tuple[str, str, int]]]:
    variable_types = collect_variable_types(function, rules)
    edges: list[Edge] = []
    direct_sinks: list[tuple[str, str, int]] = []
    scoped_lifecycle_sinks: list[tuple[str, str, int]] = []
    scoped_body = (
        mask_spectator_refresh_callback(function)
        if function_identity(function) == SPECTATOR_REFRESH_SCHEDULER_IDENTITY
        else function.body
    )

    if function_identity(function) not in SCOPED_SYNC_CONTEXT_ALLOWED_CALLER_IDENTITIES:
        for declaration in SCOPED_SYNC_CONTEXT_DECL_RE.finditer(scoped_body):
            line = function.body_line + scoped_body.count("\n", 0, declaration.start())
            scoped_lifecycle_sinks.append(
                (
                    "ScopedSyncContext::Lifecycle",
                    "nested sync-context activation/release",
                    line,
                )
            )

    for call in collect_calls(function, rules):
        typed_target = typed_call_target(call, variable_types)

        if typed_target is None and function.owner is not None and "->" not in call.target and "." not in call.target and "::" not in call.target:
            same_owner = f"{function.owner}::{call.basename}"
            if same_owner in functions_by_name or same_owner in FORBIDDEN_SINKS:
                typed_target = same_owner

        is_free_call = "->" not in call.target and "." not in call.target and "::" not in call.target
        if typed_target is not None:
            terminal_candidates = functions_by_name.get(typed_target, [])
        elif is_free_call:
            terminal_candidates = functions_by_name.get(call.target, [])
            if not terminal_candidates:
                terminal_candidates = functions_by_basename.get(call.basename, [])
        else:
            terminal_candidates = []
        terminal_candidates = matching_call_candidates(
            call,
            terminal_candidates,
            function,
            variable_types,
            rules,
        )

        is_named_validate_terminal = (
            call.basename == "ValidateEntityAccess" and is_free_call
        )
        is_named_ensure_terminal = call.basename == "EnsureEntitySynced"
        is_exact_terminal = candidates_are_exact_terminals(
            terminal_candidates,
            ALLOWED_TERMINAL_IDENTITIES,
        )
        if is_named_validate_terminal and is_exact_terminal:
            continue
        if (
            call.basename == "IsEntityAccessValid"
            and is_free_call
            and function_identity(function) == IS_ENTITY_LOCKED_EXPORT_IDENTITY
        ):
            exact_probe = (
                candidates_are_exact_terminals(
                    terminal_candidates,
                    {ENTITY_ACCESS_PROBE_IDENTITY},
                )
                and len(call.arguments) == 2
                and normalized_entity_expression(call.arguments[0]) == "entity"
                and normalized_entity_expression(call.arguments[1]) == "false"
            )
            if exact_probe:
                continue
            direct_sinks.append(
                (
                    call.basename,
                    "Game.IsEntityLocked must use the exact non-diagnosing entity-access probe",
                    call.line,
                )
            )
            continue
        # ValidateEntityAccess and EnsureEntitySynced are freely repeatable
        # idempotent terminals: the script prepares the cover and native code may
        # validate/retain it any number of times, in loops, catches, or recursion.
        # An exact terminal is a skipped no-op (its body's non-blocking contract is
        # checked separately); a non-canonical spelling falls through to ordinary
        # graph handling so a genuine impostor still exposes its own sinks
        if is_named_ensure_terminal and candidates_are_exact_terminals(
            terminal_candidates,
            ALLOWED_TERMINAL_IDENTITIES,
        ):
            continue

        # EnsureFreshEntitySynced is private and friend-restricted in the
        # engine. Both its implementation identity and the qualified caller
        # identity must match; a same-named fixture/helper is not trusted
        if typed_target == "SyncContext::EnsureFreshEntitySynced":
            trusted_caller = (
                function_identity(function)
                in TRUSTED_FRESH_CAPTURE_CALLER_IDENTITIES
            )
            exact_fresh_terminal = candidates_are_exact_terminals(
                terminal_candidates,
                {FRESH_ENSURE_TERMINAL_IDENTITY},
            )
            if trusted_caller and exact_fresh_terminal:
                continue
            direct_sinks.append(
                (
                    "SyncContext::EnsureFreshEntitySynced",
                    "fresh-entity capture outside an exact trusted registration caller",
                    call.line,
                )
            )
            continue

        if call.basename == "EnsureFreshEntitySynced" and typed_target is None:
            direct_sinks.append(
                (
                    "EnsureFreshEntitySynced",
                    "unresolved non-SyncContext fresh-entity capture",
                    call.line,
                )
            )
            continue

        if is_named_validate_terminal and terminal_candidates:
            # Fall through into normal graph traversal. This is deliberately
            # not an allowlist by spelling: a fake helper must expose its sink
            pass

        if typed_target is not None and candidates_are_exact_terminals(
            terminal_candidates,
            PROPERTY_LOCK_METHOD_TERMINAL_IDENTITIES,
        ):
            continue

        if (
            call.basename == "ScopedSyncContext"
            and function_identity(function)
            not in SCOPED_SYNC_CONTEXT_ALLOWED_CALLER_IDENTITIES
        ):
            scoped_lifecycle_sinks.append(
                (
                    "ScopedSyncContext::Lifecycle",
                    "nested sync-context activation/release",
                    call.line,
                )
            )
            continue

        if typed_target in FORBIDDEN_SINKS:
            direct_sinks.append((typed_target, FORBIDDEN_SINKS[typed_target], call.line))
            continue

        if typed_target is not None:
            candidates = matching_call_candidates(
                call,
                functions_by_name.get(typed_target, []),
                function,
                variable_types,
                rules,
            )
            # A known receiver type that has no parsed implementation is an
            # inherited/header/external method.  Do not fall back to an
            # unrelated same-named engine method (notably Entity::Release vs
            # SyncContext::Release)
            if candidates or re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*(?:->|\.)[A-Za-z_][A-Za-z0-9_]*", call.target):
                edges.extend(Edge(function.key, candidate.key, call.line, call.offset, call.arguments) for candidate in candidates)
                continue

        # A member invoked on an expression such as
        # cast_from_void<T*>(value)->Release() has no recoverable receiver
        # type in this lightweight parser. For a retained identifier/member
        # receiver, a basename is still conservative when every parsed
        # implementation belongs to one owner (including overloads). Bare
        # expression members and ambiguous names such as Release/GetName stay
        # disconnected instead of aliasing unrelated classes
        if call.is_member_call:
            if "->" in call.target or "." in call.target:
                candidates = matching_call_candidates(
                    call,
                    functions_by_basename.get(call.basename, []),
                    function,
                    variable_types,
                    rules,
                )
                candidate_owners = {candidate.owner for candidate in candidates}
                if len(candidate_owners) == 1 and None not in candidate_owners:
                    edges.extend(
                        Edge(function.key, candidate.key, call.line, call.offset, call.arguments)
                        for candidate in candidates
                    )
            continue

        candidates = matching_call_candidates(
            call,
            functions_by_name.get(call.target, []),
            function,
            variable_types,
            rules,
        )
        if not candidates:
            candidates = matching_call_candidates(
                call,
                functions_by_basename.get(call.basename, []),
                function,
                variable_types,
                rules,
            )
        edges.extend(Edge(function.key, candidate.key, call.line, call.offset, call.arguments) for candidate in candidates)

    direct_sinks.extend(scoped_lifecycle_sinks)
    return edges, direct_sinks


def function_parameter_declarations(function: Function) -> tuple[tuple[str | None, str], ...]:
    open_paren = function.signature.find("(")
    close_paren = find_matching_paren(function.signature, open_paren) if open_paren != -1 else -1
    if close_paren == -1:
        return ()
    result: list[tuple[str | None, str]] = []
    for declaration in split_top_level(function.signature[open_paren + 1 : close_paren]):
        if not declaration or declaration == "void":
            continue
        declaration = split_top_level(declaration, "=")[0]
        identifiers = re.findall(r"[A-Za-z_][A-Za-z0-9_]*", declaration)
        result.append((identifiers[-1] if identifiers else None, declaration))
    return tuple(result)


# The cover-retention terminals are the allowed skipped terminals of the main
# traversal (ValidateEntityAccess / EnsureEntitySynced / EnsureFreshEntitySynced
# and the exact internal acquisition helpers), so the general no-blocking BFS
# never enters their bodies. This narrow set gives them a direct check
NONBLOCKING_TERMINAL_IDENTITIES = {
    _engine("Source/Common/Entity.h", "ValidateEntityAccess"),
    _engine("Source/Server/EntitySync.cpp", "EnsureEntitySynced"),
    _engine("Source/Server/EntitySync.cpp", "SyncContext::EnsureEntitySynced"),
    _engine("Source/Server/EntitySync.cpp", "SyncContext::EnsureFreshEntitySynced"),
    _engine("Source/Server/EntitySync.cpp", "SyncContext::EnsureEntitySyncedImpl"),
    _engine("Source/Server/EntitySync.cpp", "EntityLock::TryLockStateMutex"),
    _engine("Source/Server/EntitySync.cpp", "EntityLock::UnlockStateMutex"),
    _engine("Source/Server/EntitySync.cpp", "EntityLock::IsEnsureOpCompatible"),
    _engine("Source/Server/EntitySync.cpp", "EntityLock::CommitEnsureOp"),
    _engine("Source/Server/EntitySync.cpp", "EntityLock::HasForeignDescendantHolder"),
    _engine("Source/Server/EntitySync.cpp", "EntityLock::HasWaitingExclusive"),
}

# Call basenames that would turn a cover-retention terminal into a blocking or
# waiting acquisition, an entity-cover replacement, or a cover/lock release. The
# transient internal state-mutex handles (try_lock / unlock) and the read-only
# predicates the batch consults stay allowed; only the blocking/replacing/
# releasing spellings below are rejected
TERMINAL_BLOCKING_CALL_BASENAMES = {
    # blocking entity-lock / singleton / state-mutex acquisition
    "Acquire",
    "AcquireShared",
    "AcquireLocks",
    "AcquireLocksOrderedFair",
    "RegisterDescendantHold",
    "LockSingleton",
    "LockStateMutex",
    "Lock",
    "lock",
    # entity-cover replacement / whole-world acquisition
    "Sync",
    "SyncEntity",
    "SyncEntities",
    "SyncWholeWorld",
    # cover / lock release
    "Release",
    "ReleaseShared",
    "ReleaseLocks",
    "ReleaseSingletonLocks",
    "UnregisterDescendantHold",
    "UnlockSingleton",
    "Unlock",
    # condition-variable style blocking wait (exact; a substring "wait" would
    # collide with the read-only HasWaitingExclusive predicate)
    "wait",
    "wait_for",
    "wait_until",
}

# Basename families that always denote sleeping, spinning, backing off, rolling
# back, or retrying while obtaining cover. Matched as a substring because these
# appear inside descriptive helper names (e.g. BackoffBeforeSyncRetry) and never
# collide with a legitimate cover-retention call
TERMINAL_BLOCKING_BASENAME_RE = re.compile(r"(?i)(sleep|yield|backoff|spin|rollback|retry)")

# EnsureEntitySyncedImpl may retry only its exact all-or-nothing internal
# state-mutex batch. The helper performs bounded yield/sleep back-off after the
# partial transient state-mutex prefix has been released; the caller's entity
# cover is never released or replaced. Keep this exception pinned to both the
# exact terminal identity and helper basename so no other terminal can acquire
# a general waiting/retry capability
TERMINAL_ALLOWED_BOUNDED_RETRY_CALLS = {
    (
        ENGINE_ORIGIN,
        "Source/Server/EntitySync.cpp",
        "SyncContext::EnsureEntitySyncedImpl",
        "BackoffBeforeSyncRetry",
    ),
}


def terminal_body_blocking_violations(
    functions: list[Function],
    rules: SyncRules = ENGINE_RULES,
) -> list[EnsureImplementationViolation]:
    """Keep the skipped cover-retention terminals from changing caller cover.

    EnsureEntitySynced / ValidateEntityAccess and their internal acquisition
    helpers are allowed terminals of the main traversal, so the general
    no-blocking rule never enters their bodies. This checks the contract
    directly: those exact functions must not block-acquire an entity/state lock
    or replace/release the caller's cover. The one reviewed exception is
    EnsureEntitySyncedImpl's bounded back-off between all-or-nothing
    state-mutex try batches; no entity cover is retained by the failed batch.
    """

    violations: list[EnsureImplementationViolation] = []
    for function in functions:
        identity = function_identity(function)
        if identity not in NONBLOCKING_TERMINAL_IDENTITIES:
            continue
        for call in collect_calls(function, rules):
            if (*identity, call.basename) in TERMINAL_ALLOWED_BOUNDED_RETRY_CALLS:
                continue
            if (
                call.basename in TERMINAL_BLOCKING_CALL_BASENAMES
                or TERMINAL_BLOCKING_BASENAME_RE.search(call.basename)
            ):
                violations.append(
                    EnsureImplementationViolation(
                        function,
                        call.line,
                        call.basename,
                        "cover-retention terminal may wait, block-acquire, "
                        "replace, or release cover",
                    )
                )
    return violations


def analyze(functions: list[Function], rules: SyncRules = ENGINE_RULES) -> list[Diagnostic]:
    by_key = {function.key: function for function in functions}
    by_name: dict[str, list[Function]] = {}
    by_basename: dict[str, list[Function]] = {}
    for function in functions:
        by_name.setdefault(function.name, []).append(function)
        by_basename.setdefault(function.basename, []).append(function)

    edges_by_source: dict[str, list[Edge]] = {}
    direct_sinks_by_source: dict[str, list[tuple[str, str, int]]] = {}
    for function in functions:
        edges, direct_sinks = resolve_calls(
            function,
            by_name,
            by_basename,
            rules,
        )
        edges_by_source[function.key] = edges
        direct_sinks_by_source[function.key] = direct_sinks

    diagnostics = [
        Diagnostic(
            root=violation.function,
            sink=violation.target,
            reason=violation.reason,
            path=(violation.function,),
            call_lines=(violation.line,),
        )
        for violation in terminal_body_blocking_violations(functions, rules)
    ]
    roots = sorted(
        (
            function
            for function in functions
            if function.is_script_export
            and function_identity(function) not in EXPLICIT_SYNC_EXPORT_IDENTITIES
        ),
        key=lambda function: (function.path.as_posix(), function.line, function.signature),
    )

    for root in roots:
        queue = deque([(root.key, (root.key,), ())])
        visited = {root.key}
        found: Diagnostic | None = None
        while queue and found is None:
            current_key, path_keys, call_lines = queue.popleft()
            current = by_key[current_key]

            if current.name in FORBIDDEN_SINKS and current_key != root.key:
                found = Diagnostic(
                    root=root,
                    sink=current.name,
                    reason=FORBIDDEN_SINKS[current.name],
                    path=tuple(by_key[key] for key in path_keys[:-1]),
                    call_lines=call_lines,
                )
                break

            for sink, reason, line in direct_sinks_by_source[current_key]:
                found = Diagnostic(
                    root=root,
                    sink=sink,
                    reason=reason,
                    path=tuple(by_key[key] for key in path_keys),
                    call_lines=call_lines + (line,),
                )
                break
            if found is not None:
                break

            for edge in edges_by_source[current_key]:
                if edge.target in visited:
                    continue
                visited.add(edge.target)
                queue.append((edge.target, path_keys + (edge.target,), call_lines + (edge.line,)))

        if found is not None:
            diagnostics.append(found)

    return diagnostics



# ---------------------------------------------------------------------------
# Baselines, configuration staleness and the command entry
# ---------------------------------------------------------------------------

BASELINE_COLUMNS = ("file", "export", "sink", "reason")
ENGINE_BASELINE = Path("BuildTools") / "audit_baselines" / "sync-scope.tsv"


@dataclass(frozen=True)
class BaselineRow:
    origin: str
    file: str
    export: str
    sink: str
    reason: str
    source: str

    def key(self) -> tuple[str, str, str, str]:
        return self.origin, self.file, self.export, self.sink


def diagnostic_key(diagnostic: Diagnostic) -> tuple[str, str, str, str]:
    return diagnostic.root.origin, diagnostic.root.unit_path, diagnostic.root.name, diagnostic.sink


def load_baseline(path: Path, origin: str) -> tuple[list[BaselineRow], list[str]]:
    """Read one sync-scope baseline; every row needs a reason and a malformed row is an error."""

    rows: list[BaselineRow] = []
    problems: list[str] = []

    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as exception:
        return rows, [f"{path}: unable to read baseline: {exception}"]

    lines = text.split("\n")
    header = lines[0].rstrip("\r").split("\t") if lines else []

    if header != list(BASELINE_COLUMNS):
        return rows, [f"{path.name}:1: unexpected baseline header {header} (expected {list(BASELINE_COLUMNS)})"]

    seen: set[tuple[str, str, str, str]] = set()

    for line_number, raw in enumerate(lines[1:], start=2):
        line = raw.rstrip("\r")

        if not line:
            continue

        parts = line.split("\t")

        if len(parts) != len(BASELINE_COLUMNS):
            problems.append(f"{path.name}:{line_number}: expected {len(BASELINE_COLUMNS)} columns, got {len(parts)}")
            continue

        file, export, sink, reason = (part.strip() for part in parts)
        row = BaselineRow(origin, file, export, sink, reason, f"{path.name}:{line_number}")

        if not file or not export or not sink:
            problems.append(f"{row.source}: file, export and sink must not be empty")
            continue

        if not reason:
            problems.append(f"{row.source}: baseline row has no reason: {export} -> {sink}")

        if row.key() in seen:
            problems.append(f"{row.source}: duplicate baseline row: {export} -> {sink}")
            continue

        seen.add(row.key())
        rows.append(row)

    return rows, problems


def config_staleness(context: AuditContext, functions: list[Function]) -> list[str]:
    """Reject configured project rules that no longer name anything in the audited sources."""

    config = context.config.sync_scope
    identities = {function_identity(function) for function in functions}
    basenames = {function.basename for function in functions}
    errors: list[str] = []

    for rule in config.independent_job_schedulers:
        if (PROJECT_ORIGIN, rule.file, rule.function) not in identities:
            errors.append(f"configured independent job scheduler not found in the project scopes: {rule.file}::{rule.function}")

    for rule in config.typed_factories:
        if rule.name not in basenames:
            errors.append(f"configured typed factory has no definition in the audited sources: {rule.name}")

    for rule in config.deferred_callback_calls:
        call_re = re.compile(rf"\b{re.escape(rule.name)}\s*\(")

        if not any(call_re.search(function.body) for function in functions):
            errors.append(f"configured deferred-callback call is never called in the audited sources: {rule.name}")

    return errors


def run(context: AuditContext, baseline: Path | None, json_output: bool) -> int:
    """Audit the Engine scope plus the project scopes; return the process exit code."""

    sources, errors = source_files(context)
    functions = collect_functions(sources)
    errors += config_staleness(context, functions)
    diagnostics = analyze(functions, rules_for(context))

    rows, problems = load_baseline(context.engine_root / ENGINE_BASELINE, ENGINE_ORIGIN)
    errors += problems

    if baseline is not None:
        project_rows, problems = load_baseline(resolve_input(context, baseline), PROJECT_ORIGIN)
        rows += project_rows
        errors += problems

    rows_by_key = {row.key(): row for row in rows}
    reported = [diagnostic for diagnostic in diagnostics if diagnostic_key(diagnostic) not in rows_by_key]
    baselined = [diagnostic for diagnostic in diagnostics if diagnostic_key(diagnostic) in rows_by_key]
    used_keys = {diagnostic_key(diagnostic) for diagnostic in baselined}

    for row in rows:
        if row.key() not in used_keys:
            errors.append(f"{row.source}: stale baseline row, the audit no longer reports it: {row.file} {row.export} -> {row.sink}")

    roots = sum(function.is_script_export for function in functions)

    if json_output:
        print(
            json.dumps(
                {
                    "summary": {
                        "functions": len(functions),
                        "script_native_roots": roots,
                        "violations": len(reported),
                        "baselined": len(baselined),
                        "errors": len(errors),
                    },
                    "diagnostics": [diagnostic.to_dict() for diagnostic in reported],
                    "errors": errors,
                },
                indent=2,
            )
        )
    else:
        for diagnostic in reported:
            call_path = " -> ".join(function.name for function in diagnostic.path)
            if call_path:
                call_path += " -> "
            call_path += diagnostic.sink
            print(
                f"{diagnostic.root.path.as_posix()}:{diagnostic.root.line}: error: "
                f"{diagnostic.root.name} reaches {diagnostic.reason}: {call_path}"
            )

        for message in errors:
            print(f"error: {message}")

        print(
            "sync-scope audit: "
            f"{len(functions)} functions, "
            f"{roots} script-native roots, "
            f"{len(reported)} violation(s), "
            f"{len(baselined)} baselined, "
            f"{len(errors)} error(s)"
        )

    return 1 if reported or errors else 0
