#!/usr/bin/python3
"""Guard persisted identities against unmigrated removal and reuse.

Stored entities and authored content refer to prototypes, property names and enum values by name. The guard
compares the identities a project declares now with a committed manifest of the identities it declared
before. An identity that left the tree needs a `///@ MigrationRule` (`Proto <Type> Rename|Remove|Transform`,
`Property <Owner> Rename|Remove`, `Enum <Enum> <Old> <New>`), and an identity that a rule or an earlier
removal retired may never be declared again.

Prototypes come from the `Proto` baker packs of the main config, read by `fo_formats` exactly as ProtoBaker
reads them. Property, enum, RefType and MigrationRule declarations come from the `.fos` and `.cs` sources of
its `Metadata` baker packs, so both script backends are covered, plus optional native sources.
"""

from __future__ import annotations

import argparse
import io
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence

import fo_formats as ff


KINDS = ('proto', 'property', 'enum')
STATUSES = ('live', 'retired')
MANIFEST_HEADER = '# FOnline migration guard manifest v1: kind, status, owner, name'
REF_TYPE_REGISTRAR_SUFFIX = 'RefType'
CONFIG_KEYS = {'main_config', 'sub_config', 'manifest', 'content_dirs', 'script_dirs', 'proto_extensions',
               'native_sources', 'exclude', 'file_identities', 'skip_git_ignored'}

REUSED_CODES = {'proto': 'REUSED_PROTO_ID', 'property': 'REUSED_PROPERTY_NAME', 'enum': 'REUSED_ENUM_VALUE'}
NOUNS = {'proto': 'prototype id', 'property': 'property name', 'enum': 'enum value'}
RULE_HINTS = {
    'proto': '///@ MigrationRule Proto {owner} Rename {name} <New>, Remove {name} or Transform {name} <Function>',
    'property': '///@ MigrationRule Property {owner} Rename {name} <New> or Remove {name}',
    'enum': '///@ MigrationRule Enum {owner} {name} <ExistingValue>',
}
BLOCKING_CODES = {'INVALID_CONTENT', 'INVALID_TAG', 'INVALID_RULE', 'MISSING_RULE', 'DANGLING_TARGET', *REUSED_CODES.values()}

ENTITY_TYPE_NAME_RE = re.compile(r'^\s*static\s+constexpr\s+string_view\s+ENTITY_TYPE_NAME\s*(?:=\s*"(?P<assigned>\w+)"\s*;|\{\s*"(?P<braced>\w+)"\s*\}\s*;)')
ENTITY_PROPERTY_RE = re.compile(r'FO_ENTITY_PROPERTY\(\s*[^,]+,\s*(\w+)\s*\)')
EXPORT_ENUM_RE = re.compile(r'^\s*///@\s+ExportEnum\b')
ENUM_CLASS_RE = re.compile(r'^\s*enum\s+class\s+(\w+)')
ENUM_ENTRY_RE = re.compile(r'^\s*(\w+)\s*(?:=[^,]*)?,?\s*(?://.*)?$')

Identity = tuple[str, str, str]


class GuardError(Exception):
    """Invalid guard configuration or an environment the guard cannot run in; exit code 2."""


@dataclass(frozen=True, slots=True)
class Violation:
    code: str
    kind: str
    owner: str
    name: str
    message: str
    location: str = ''

    def __str__(self) -> str:
        text = f'{self.code}\t{self.kind}\t{self.owner}\t{self.name}\t{self.message}'
        return f'{text}\t{self.location}' if self.location else text


@dataclass(frozen=True, slots=True)
class FileIdentitySource:
    type_name: str
    patterns: tuple[str, ...]


@dataclass(slots=True)
class GuardConfig:
    root: Path
    manifest: Path
    main_config: Path | None = None
    sub_config: str | None = None
    content_dirs: list[Path] | None = None
    script_dirs: list[Path] | None = None
    proto_extensions: list[str] | None = None
    native_sources: list[str] = field(default_factory=list)
    exclude: list[str] = field(default_factory=list)
    file_identities: list[FileIdentitySource] = field(default_factory=list)
    skip_git_ignored: bool = True


@dataclass(slots=True)
class Declarations:
    """What the tree declares now, plus the rules that retire or migrate identities."""

    current: set[Identity] = field(default_factory=set)
    rules: list[ff.MigrationRule] = field(default_factory=list)
    ref_types: set[str] = field(default_factory=set)
    locations: dict[Identity, str] = field(default_factory=dict)
    violations: list[Violation] = field(default_factory=list)
    counts: dict[str, int] = field(default_factory=dict)


@dataclass(slots=True)
class Manifest:
    live: set[Identity] = field(default_factory=set)
    retired: set[Identity] = field(default_factory=set)


# ---------------------------------------------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------------------------------------------

def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog='migration_guard.py',
        description='Require a MigrationRule for every removed or renamed prototype, property and enum value, '
                    'and refuse reuse of a retired identifier, against a committed manifest.')
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--check', action='store_true', help='Compare the tree with the manifest; exit 1 on any violation.')
    mode.add_argument('--update-manifest', action='store_true',
                      help='Rewrite the manifest from the tree; refuses while a rule is missing or an identifier is reused.')
    parser.add_argument('--root', type=Path, default='.', help='Project root; relative paths resolve against it (default: current directory).')
    parser.add_argument('--config', type=Path, help='JSON guard configuration, relative to the root.')
    parser.add_argument('--main-config', type=Path, help='Main configuration file (.fomain) whose packs define the scope.')
    parser.add_argument('--sub-config', help='Apply this [SubConfig] of the main config before reading resource packs.')
    parser.add_argument('--manifest', type=Path, help='Manifest file to check or rewrite.')
    parser.add_argument('--content-dir', type=Path, action='append', help='Prototype directory; replaces the Proto packs of the main config.')
    parser.add_argument('--proto-extension', action='append', help='Prototype file extension used with --content-dir (default: Baking.ProtoFileExtensions).')
    parser.add_argument('--script-dir', type=Path, action='append', help='Script source directory; replaces the Metadata packs of the main config.')
    parser.add_argument('--native-source', action='append', help='Root-relative glob of native sources with property and enum declarations.')
    return parser


def _string_list(value: object, key: str) -> list[str]:
    if not isinstance(value, list) or not all(isinstance(item, str) and item for item in value):
        raise GuardError(f'Config key {key} must be a list of non-empty strings')
    return list(value)


def load_guard_config(args: argparse.Namespace) -> GuardConfig:
    root = args.root.resolve()
    data: dict[str, object] = {}

    if args.config is not None:
        config_path = root / args.config
        try:
            data = json.loads(config_path.read_text(encoding='utf-8'))
        except (OSError, json.JSONDecodeError) as ex:
            raise GuardError(f'Cannot read guard config {config_path.as_posix()}: {ex}') from ex
        if not isinstance(data, dict):
            raise GuardError('Guard config must be a JSON object')
        unknown = sorted(set(data) - CONFIG_KEYS)
        if unknown:
            raise GuardError(f'Unknown guard config keys: {", ".join(unknown)}')

    def path_of(key: str, override: Path | None) -> Path | None:
        value = override if override is not None else data.get(key)
        if value is None:
            return None
        if not isinstance(value, (str, Path)) or not str(value):
            raise GuardError(f'Config key {key} must be a path')
        return root / value

    def paths_of(key: str, override: list[Path] | None) -> list[Path] | None:
        if override is not None:
            return [root / path for path in override]
        if key not in data:
            return None
        return [root / path for path in _string_list(data[key], key)]

    manifest = path_of('manifest', args.manifest)
    if manifest is None:
        raise GuardError('No manifest path: set "manifest" in the guard config or pass --manifest')

    sub_config = args.sub_config if args.sub_config is not None else data.get('sub_config')
    if sub_config is not None and not isinstance(sub_config, str):
        raise GuardError('Config key sub_config must be a string')

    skip_git_ignored = data.get('skip_git_ignored', True)
    if not isinstance(skip_git_ignored, bool):
        raise GuardError('Config key skip_git_ignored must be true or false')

    file_identities = []
    for index, entry in enumerate(data.get('file_identities', [])):
        if not isinstance(entry, dict) or set(entry) != {'type', 'patterns'} or not isinstance(entry['type'], str) or not entry['type']:
            raise GuardError(f'file_identities[{index}] must be an object with "type" and "patterns"')
        file_identities.append(FileIdentitySource(entry['type'], tuple(_string_list(entry['patterns'], f'file_identities[{index}].patterns'))))

    config = GuardConfig(
        root=root,
        manifest=manifest,
        main_config=path_of('main_config', args.main_config),
        sub_config=sub_config,
        content_dirs=paths_of('content_dirs', args.content_dir),
        script_dirs=paths_of('script_dirs', args.script_dir),
        proto_extensions=args.proto_extension if args.proto_extension is not None else (
            _string_list(data['proto_extensions'], 'proto_extensions') if 'proto_extensions' in data else None),
        native_sources=args.native_source if args.native_source is not None else _string_list(data.get('native_sources', []), 'native_sources'),
        exclude=_string_list(data.get('exclude', []), 'exclude'),
        file_identities=file_identities,
        skip_git_ignored=skip_git_ignored,
    )

    if config.main_config is None and (config.content_dirs is None or config.script_dirs is None):
        candidates = sorted(root.glob('*.fomain'))
        if len(candidates) != 1:
            raise GuardError('Cannot choose a main config: set "main_config", pass --main-config, or give both content and script directories')
        config.main_config = candidates[0]

    return config


# ---------------------------------------------------------------------------------------------------------------
# Scope
# ---------------------------------------------------------------------------------------------------------------

def _relative(config: GuardConfig, path: Path) -> str:
    try:
        return path.resolve().relative_to(config.root).as_posix()
    except ValueError:
        return path.resolve().as_posix()


def expand_globs(root: Path, patterns: Sequence[str]) -> list[Path]:
    """Files below `root` matching root-relative resource globs; only the literal prefix of a pattern is walked."""
    found: set[Path] = set()

    for pattern in patterns:
        normalized = pattern.replace('\\', '/')
        segments = normalized.split('/')
        literal = []
        for segment in segments[:-1]:
            if any(char in segment for char in '*?'):
                break
            literal.append(segment)
        base = root.joinpath(*literal) if literal else root

        if not any(char in normalized for char in '*?'):
            if (root / normalized).is_file():
                found.add((root / normalized).resolve())
            continue

        for directory, dirnames, filenames in os.walk(base):
            dirnames[:] = sorted(name for name in dirnames if name != '.git')
            for filename in filenames:
                path = Path(directory) / filename
                if ff.match_resource_glob(path.relative_to(root).as_posix(), normalized):
                    found.add(path.resolve())

    return sorted(found)


def git_ignored(root: Path, paths: Sequence[str]) -> set[str]:
    """Root-relative paths git ignores, so the guard sees the tree a clean checkout has.

    Paths inside a submodule, such as an embedded Engine, are asked of that submodule's repository."""
    if not paths:
        return set()

    submodule_dirs: list[str] = []
    if (root / '.gitmodules').is_file():
        # Each -z entry is "submodule.<name>.path\n<directory>"
        listing = _git(root, ['config', '--null', '--file', '.gitmodules', '--get-regexp', r'\.path$'], allow_codes=(0, 1))
        submodule_dirs = sorted((entry.split('\n', 1)[1].strip('/') for entry in listing.split('\0') if '\n' in entry), key=len, reverse=True)

    by_submodule: dict[str, list[str]] = {}
    own: list[str] = []

    for path in paths:
        owner = next((directory for directory in submodule_dirs if path.startswith(directory + '/')), None)
        if owner is None:
            own.append(path)
        else:
            by_submodule.setdefault(owner, []).append(path[len(owner) + 1:])

    ignored: set[str] = set()
    if own:
        output = _git(root, ['check-ignore', '-z', '--stdin'], allow_codes=(0, 1), stdin='\0'.join(own) + '\0')
        ignored = {token for token in output.split('\0') if token}

    for directory, nested in by_submodule.items():
        ignored |= {f'{directory}/{path}' for path in git_ignored(root / directory, nested)}

    return ignored


def _git(cwd: Path, args: list[str], *, allow_codes: tuple[int, ...] = (0,), stdin: str | None = None) -> str:
    try:
        result = subprocess.run(['git', '-c', 'core.quotePath=false', *args], cwd=cwd, capture_output=True,
                                input=stdin.encode('utf-8') if stdin is not None else None)
    except OSError as ex:
        raise GuardError(f'git is required to skip ignored files ({ex}); set "skip_git_ignored": false to scan every file') from ex

    # check-ignore answers 1 when nothing is ignored; any other code means git could not answer
    if result.returncode not in allow_codes:
        raise GuardError(f'git {args[0]} failed in {cwd.as_posix()}: {result.stderr.decode("utf-8", "replace").strip()}; '
                         'set "skip_git_ignored": false outside a repository')

    return result.stdout.decode('utf-8')


def _filter(config: GuardConfig, files: list[tuple[str, Path]]) -> list[tuple[str, Path]]:
    # Paths are keyed root-relative for exclusion and git, and keep their resource path for prototype naming
    keyed = [(resource_path, path, _relative(config, path)) for resource_path, path in files]
    kept = [item for item in keyed if not any(ff.match_resource_glob(item[2], pattern) for pattern in config.exclude)]
    ignored = git_ignored(config.root, [item[2] for item in kept]) if config.skip_git_ignored else set()
    return [(resource_path, path) for resource_path, path, relative in kept if relative not in ignored]


def _dir_files(directories: Sequence[Path], extensions: Sequence[str]) -> list[tuple[str, Path]]:
    pack = ff.ResourcePack('Explicit', Path('.'), list(directories), [], [], [], [])
    return [(item.path, item.file) for item in ff.pack_files(pack, extensions=extensions)]


def collect_scope(config: GuardConfig) -> tuple[list[tuple[str, Path]], list[tuple[str, Path]], list[Path]]:
    """`(prototype files, script sources, native sources)`, each as the bakers and codegen would read them."""
    project = ff.load_project_config(config.main_config, sub_config=config.sub_config) if config.main_config is not None else None

    if config.content_dirs is not None:
        extensions = config.proto_extensions or (project.setting_list('Baking.ProtoFileExtensions') if project is not None
                                                 else ff.split_list(ff.ENGINE_SETTING_DEFAULTS['Baking.ProtoFileExtensions']))
        proto_files = _dir_files(config.content_dirs, extensions)
    elif project is not None:
        proto_files = [(item.path, item.file) for item in ff.project_prototype_files(project)]
    else:
        raise GuardError('Prototype directories need either --content-dir or a main config')

    if config.script_dirs is not None:
        script_files = _dir_files(config.script_dirs, ff.METADATA_SOURCE_EXTENSIONS)
    elif project is not None:
        script_files = [(item.path, item.file) for item in ff.project_metadata_sources(project)]
    else:
        raise GuardError('Script directories need either --script-dir or a main config')

    native_files = [(path.name, path) for path in expand_globs(config.root, config.native_sources)]

    return _filter(config, proto_files), _filter(config, script_files), [path for _, path in _filter(config, native_files)]


# ---------------------------------------------------------------------------------------------------------------
# Declarations
# ---------------------------------------------------------------------------------------------------------------

def type_token_count(tokens: Sequence[str]) -> int:
    """Tokens of a property type, following `EngineMetadata::ResolveComplexType` syntax."""
    if not tokens:
        raise ValueError('missing property type')
    if len(tokens) >= 2 and tokens[1] == '=':
        if len(tokens) < 4 or tokens[2] != '>':
            raise ValueError('expected => for a dict type')
        value_count = type_token_count(tokens[3:])
        return 3 + value_count
    if len(tokens) >= 2 and tokens[1] == '[':
        if len(tokens) < 3 or tokens[2] != ']':
            raise ValueError('expected ] for an array type')
        return 3 + (1 if len(tokens) >= 4 and tokens[3] == '&' else 0)
    if tokens[0] == 'callback':
        raise ValueError('callback is not a property type')
    return 1 + (1 if len(tokens) >= 2 and tokens[1] == '&' else 0)


def property_identity(tag: ff.MetadataTag) -> tuple[str, str]:
    """`(owner, name)` of `///@ Property <Owner> <Target> <Type> <Name>[.<Field>] [flags]`."""
    tokens = tag.tokens
    if len(tokens) < 4:
        raise ValueError('insufficient parameters')
    index = 2 + type_token_count(tokens[2:])
    if index >= len(tokens):
        raise ValueError('missing property name')
    name = tokens[index]
    if index < len(tokens) - 2 and tokens[index + 1] == '.':
        name += '.' + tokens[index + 2]
    return tokens[0], name


def _native_tags(text: str, path: str) -> list[ff.MetadataTag]:
    # Codegen reads a native tag only where the marker starts its line
    return [tag for tag in ff.iter_metadata_tags(text, path)
            if text.split('\n')[tag.line - 1].lstrip(' \t').startswith('///@')]


def _native_declarations(text: str) -> tuple[set[tuple[str, str]], set[tuple[str, str]]]:
    properties: set[tuple[str, str]] = set()
    enums: set[tuple[str, str]] = set()
    lines = text.splitlines()
    owner: str | None = None

    for index, line in enumerate(lines):
        type_match = ENTITY_TYPE_NAME_RE.search(line)
        if type_match:
            owner = type_match.group('assigned') or type_match.group('braced')
            continue

        property_match = ENTITY_PROPERTY_RE.search(line)
        if property_match and owner is not None:
            properties.add((owner, property_match.group(1)))

        if EXPORT_ENUM_RE.match(line):
            enums |= _export_enum(lines[index + 1:])

    return properties, enums


def _export_enum(lines: Sequence[str]) -> set[tuple[str, str]]:
    values: set[tuple[str, str]] = set()
    name: str | None = None

    for raw in lines:
        text = raw.strip()
        if name is None:
            match = ENUM_CLASS_RE.match(text)
            if match:
                name = match.group(1)
            continue
        if text.startswith('}'):
            break
        if not text or text == '{' or text.startswith('//') or text.startswith('#'):
            continue
        match = ENUM_ENTRY_RE.match(text)
        if match:
            values.add((name, match.group(1)))

    return values


def collect_declarations(config: GuardConfig) -> Declarations:
    declarations = Declarations()
    proto_files, script_files, native_files = collect_scope(config)

    def declare(kind: str, owner: str, name: str, location: str) -> None:
        identity = (kind, owner, name)
        declarations.current.add(identity)
        declarations.locations.setdefault(identity, location)

    # Prototypes keep their authored names: a live name a rule renames away is a reuse, not an alias
    prototypes = ff.collect_prototypes([(_relative(config, path), ff.read_text(path)) for _, path in proto_files])
    for issue in prototypes.issues:
        declarations.violations.append(Violation('INVALID_CONTENT', 'proto', '', '', f'{issue.code}: {issue.message}', f'{issue.path}:{issue.line}'))
    for (type_name, name), source in prototypes.prototypes.items():
        declare('proto', type_name, name, f'{source.path}:{source.line}')

    for source in config.file_identities:
        for path in [path for _, path in _filter(config, [(p.name, p) for p in expand_globs(config.root, source.patterns)])]:
            declare('proto', source.type_name, ff.PackFile(path.name, path).stem, _relative(config, path))

    tags: list[ff.MetadataTag] = []
    for _, path in script_files:
        tags += _read_tags(config, path, False, declarations)
    for path in native_files:
        tags += _read_tags(config, path, True, declarations)
        properties, enums = _native_declarations(ff.read_text(path))
        for owner, name in properties:
            declare('property', owner, name, _relative(config, path))
        for owner, name in enums:
            declare('enum', owner, name, _relative(config, path))

    for tag in tags:
        location = f'{tag.path}:{tag.line}'
        try:
            if tag.name == 'Property':
                owner, name = property_identity(tag)
                declare('property', owner, name, location)
            elif tag.name == 'Enum':
                if len(tag.tokens) < 2:
                    raise ValueError('insufficient parameters')
                declare('enum', tag.tokens[0], tag.tokens[1], location)
            elif tag.name == 'RefType':
                if len(tag.tokens) < 2:
                    raise ValueError('insufficient parameters')
                declarations.ref_types.add(tag.tokens[1])
            elif tag.name == 'MigrationRule':
                declarations.rules.append(ff.parse_migration_rule(tag))
        except ff.FormatError as ex:
            declarations.violations.append(Violation('INVALID_RULE', '', '', '', ex.reason, location))
        except ValueError as ex:
            declarations.violations.append(Violation('INVALID_TAG', '', '', '', f'Invalid {tag.name} tag: {ex}', location))

    try:
        ff.MigrationRules(declarations.rules)
    except ff.FormatError as ex:
        declarations.violations.append(Violation('INVALID_RULE', '', '', '', ex.reason, f'{ex.path}:{ex.line}'))

    declarations.counts = {kind: sum(1 for identity in declarations.current if identity[0] == kind) for kind in KINDS}
    return declarations


def _read_tags(config: GuardConfig, path: Path, native: bool, declarations: Declarations) -> list[ff.MetadataTag]:
    relative = _relative(config, path)
    text = ff.read_text(path)
    try:
        return _native_tags(text, relative) if native else ff.iter_metadata_tags(text, relative)
    except ff.FormatError as ex:
        declarations.violations.append(Violation('INVALID_TAG', '', '', '', ex.reason, f'{ex.path}:{ex.line}'))
        return []


# ---------------------------------------------------------------------------------------------------------------
# Manifest
# ---------------------------------------------------------------------------------------------------------------

def read_manifest(path: Path) -> Manifest | None:
    if not path.is_file():
        return None

    manifest = Manifest()
    for number, raw in enumerate(path.read_text(encoding='utf-8').splitlines(), start=1):
        if not raw or raw.startswith('#'):
            continue
        parts = raw.split('\t')
        if len(parts) != 4 or parts[0] not in KINDS or parts[1] not in STATUSES or not parts[2] or not parts[3]:
            raise GuardError(f'{path.as_posix()}:{number}: manifest line must be "<kind>\\t<live|retired>\\t<owner>\\t<name>"')
        kind, status, owner, name = parts
        (manifest.live if status == 'live' else manifest.retired).add((kind, owner, name))

    return manifest


def write_manifest(path: Path, manifest: Manifest) -> None:
    lines = [MANIFEST_HEADER]
    rows = [(kind, 'live', owner, name) for kind, owner, name in manifest.live]
    rows += [(kind, 'retired', owner, name) for kind, owner, name in manifest.retired]
    lines += ['\t'.join(row) for row in sorted(rows)]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')


# ---------------------------------------------------------------------------------------------------------------
# Rules
# ---------------------------------------------------------------------------------------------------------------

def _rule_owner(rule: ff.MigrationRule, ref_types: set[str]) -> str:
    # A RefType's fields live in the `<RefType>RefType` registrar (EngineMetadata::RegisterRefType)
    if rule.kind == 'Property' and rule.owner.endswith(REF_TYPE_REGISTRAR_SUFFIX):
        stripped = rule.owner[:-len(REF_TYPE_REGISTRAR_SUFFIX)]
        if stripped in ref_types:
            return stripped
    return rule.owner


def rule_sets(rules: Iterable[ff.MigrationRule], ref_types: set[str]) -> tuple[set[Identity], set[Identity], dict[Identity, str]]:
    """`(retiring sources, migrating sources, rename targets)`.

    Rename and Remove retire their source; a Proto Transform migrates its source, which may stay live only if
    it already was. Every rule covers the removal of its source."""
    retiring: set[Identity] = set()
    migrating: set[Identity] = set()
    renames: dict[Identity, str] = {}

    for rule in rules:
        owner = _rule_owner(rule, ref_types)
        if rule.kind == 'Proto':
            identity = ('proto', owner, rule.source)
            (migrating if rule.action == 'Transform' else retiring).add(identity)
            if rule.action == 'Rename':
                renames[identity] = rule.replacement
        elif rule.kind == 'Property' and rule.action in ('Rename', 'Remove'):
            retiring.add(('property', owner, rule.source))
        elif rule.kind == 'Enum':
            retiring.add(('enum', owner, rule.source))

    return retiring, migrating, renames


def compute_violations(current: set[Identity], manifest: Manifest, retiring: set[Identity], migrating: set[Identity],
                       renames: dict[Identity, str], locations: dict[Identity, str] | None = None) -> list[Violation]:
    """Violations of one tree state against the manifest; pure, so every rule is directly testable."""
    locations = locations or {}
    violations: list[Violation] = []
    covered = retiring | migrating
    removed = manifest.live - current

    for identity in sorted(removed - covered):
        kind, owner, name = identity
        violations.append(Violation('MISSING_RULE', kind, owner, name,
                                    f'{NOUNS[kind]} left the tree without a rule; add {RULE_HINTS[kind].format(owner=owner, name=name)}'))

    reused = (current & (manifest.retired | retiring)) | ((current & migrating) - manifest.live)
    for identity in sorted(reused):
        kind, owner, name = identity
        violations.append(Violation(REUSED_CODES[kind], kind, owner, name,
                                    f'{NOUNS[kind]} is retired and stored data may still carry it; give the new one a name of its own',
                                    locations.get(identity, '')))

    for identity, target in sorted(renames.items()):
        kind, owner, name = identity
        final = target
        visited = {name}
        while (kind, owner, final) in renames and final not in visited:
            visited.add(final)
            final = renames[(kind, owner, final)]
        if (kind, owner, final) not in current and (kind, owner, final) not in migrating:
            violations.append(Violation('DANGLING_TARGET', kind, owner, name, f'rename target {final} is not a current {owner} prototype'))

    for identity in sorted(current - manifest.live - reused):
        kind, owner, name = identity
        violations.append(Violation('STALE_MANIFEST', kind, owner, name, f'{NOUNS[kind]} is declared but not recorded live; run --update-manifest',
                                    locations.get(identity, '')))

    for identity in sorted(retired_after(current, manifest, retiring) - manifest.retired - (removed - covered)):
        kind, owner, name = identity
        violations.append(Violation('STALE_MANIFEST', kind, owner, name, f'{NOUNS[kind]} left circulation but is not recorded retired; run --update-manifest'))

    return violations


def retired_after(current: set[Identity], manifest: Manifest, retiring: set[Identity]) -> set[Identity]:
    """Nothing ever leaves the retired set; every identity that stopped being declared or that a rule retires joins it."""
    return manifest.retired | ((manifest.live - current) | retiring) - current


# ---------------------------------------------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------------------------------------------

def _report(violations: Sequence[Violation], updating: bool) -> None:
    print('MIGRATION GUARD: ' + ('refusing to update the manifest:' if updating else 'violations found:'))
    for violation in violations:
        print('  ' + str(violation))
    print('')
    print('A removed, renamed or moved prototype, property or enum value needs its MigrationRule in the same change,')
    print('followed by --update-manifest. A retired identifier is never declared again: give a new meaning a new name.')


def run(config: GuardConfig, update: bool) -> int:
    manifest = read_manifest(config.manifest)

    if manifest is None and not update:
        print(f'MIGRATION GUARD: manifest {_relative(config, config.manifest)} not found; create it with --update-manifest')
        return 1

    declarations = collect_declarations(config)
    previous = manifest if manifest is not None else Manifest()
    # A RefType whose last field is gone is still known from the owners the manifest recorded
    known_owners = declarations.ref_types | {owner for kind, owner, _ in previous.live | previous.retired if kind == 'property'}
    retiring, migrating, renames = rule_sets(declarations.rules, known_owners)
    violations = declarations.violations + compute_violations(declarations.current, previous, retiring, migrating, renames, declarations.locations)

    if update:
        blocking = [violation for violation in violations if violation.code in BLOCKING_CODES]
        if blocking:
            _report(blocking, True)
            return 1

        new_manifest = Manifest(set(declarations.current), retired_after(declarations.current, previous, retiring))
        write_manifest(config.manifest, new_manifest)
        print(f'Manifest updated: {len(new_manifest.live)} live, {len(new_manifest.retired)} retired -> {_relative(config, config.manifest)}')
        return 0

    if violations:
        _report(violations, False)
        return 1

    counts = ', '.join(f'{declarations.counts[kind]} {NOUNS[kind]}s' for kind in KINDS)
    print(f'OK: {counts}; {len(manifest.retired)} retired; manifest in sync, every removal covered, nothing reused.')
    return 0


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)

    if isinstance(sys.stdout, io.TextIOWrapper):
        sys.stdout.reconfigure(encoding='utf-8')

    try:
        return run(load_guard_config(args), args.update_manifest)
    except (GuardError, ff.FormatError, OSError, UnicodeDecodeError) as ex:
        sys.stderr.write(f'migration_guard: {ex}\n')
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
