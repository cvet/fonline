#!/usr/bin/python3
"""Read Engine-owned content formats exactly as the Engine's own parsers read them.

One stdlib-only library for project tools that inspect authored content without running a bake:

- configuration files (`.fomain`, sub-configs, `.fofx`-style sections): `ConfigFile.cpp` grammar, the
  `$ENV{}`/`$FILE{}` setting directives of `Settings.cpp`, `[ResourcePack]` and `[SubConfig]` sections and
  the resource-pack file walk of the baker (`DataSource-Directory.cpp`, `FileSystem.cpp`);
- prototype files: section typing, `$Name`, migration-resolved identities and `$Parent` inheritance as
  `ProtoBaker.cpp` (properties) and `ProtoTextBaker.cpp` (`$Text` fields) resolve them;
- `.fotxt` text packs and language fallback normalization (`TextPack.cpp`, `TextBaker.cpp`);
- `///@` metadata tags and `MigrationRule` declarations as `MetadataBaker.cpp` reads them.

The module only reads and reports. It does not resolve property types or validate values against metadata,
which needs the baked metadata the bakers load.
"""

from __future__ import annotations

import argparse
import dataclasses
import io
import json
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Collection, Iterable, Iterator, Mapping, Sequence


# Whitespace sets differ between the Engine helpers: strvex::trim strips these four characters,
# while ConfigFile::TrimConfigRange also strips form feed and vertical tab
TRIM_SPACE = ' \n\r\t'
CONFIG_SPACE = ' \t\r\n\f\v'

# Engine defaults of the settings this module consults, as declared in Source/Common/Settings.inc
ENGINE_SETTING_DEFAULTS = {
    'Baking.ProtoFileExtensions': 'fopro',
    'Baking.AllowRepeatedProtoParents': 'True',
    'Baking.BakeLanguages': 'engl',
    'Baking.IgnoreInputDirs': '',
}

PROTO_BAKER = 'Proto'
PROTO_TEXT_BAKER = 'ProtoText'
TEXT_BAKER = 'Text'
METADATA_BAKER = 'Metadata'
METADATA_SOURCE_EXTENSIONS = ('fos', 'cs')
TEXT_PACK_EXTENSION = 'fotxt'

# ProtoTextBaker routes these entity types to dedicated packs and every other prototype type to Protos
PROTO_TEXT_PACKS = {'Item': 'Items', 'Critter': 'Critters', 'Map': 'Maps', 'Location': 'Locations'}
PROTO_TEXT_FALLBACK_PACK = 'Protos'

# strvex::tokenize emits each of these characters as its own token
TOKEN_SYMBOLS = frozenset('`~!@#$%^&*()+-=|\\/.,\';[]{}:><"')

ACTION_RULE_KINDS = ('Property', 'Proto')
RULE_ACTIONS = ('Rename', 'Remove', 'Transform')


class FormatError(ValueError):
    """Input the Engine parser rejects. The message follows the corresponding Engine diagnostic."""

    def __init__(self, message: str, *, path: str = '', line: int = 0) -> None:
        location = f'{path}:{line}: ' if path and line else (f'{path}: ' if path else '')
        super().__init__(location + message)
        self.path = path
        self.line = line
        self.reason = message


@dataclass(frozen=True, slots=True)
class FormatIssue:
    code: str
    message: str
    path: str = ''
    line: int = 0

    def __str__(self) -> str:
        location = f'{self.path}:{self.line}' if self.line else self.path
        return f'{location}\t{self.code}\t{self.message}' if location else f'{self.code}\t{self.message}'


def split_list(value: str) -> list[str]:
    """`strex::split(' ')`: space-separated entries, each trimmed, empty entries dropped."""
    return [entry.strip(TRIM_SPACE) for entry in value.split(' ') if entry.strip(TRIM_SPACE)]


def split_setting_list(value: str) -> list[str]:
    """Elements of a `vector<string>` setting: tabs count as spaces, as in `SetEntry`."""
    return split_list(value.replace('\t', ' '))


def parse_bool(value: str, *, what: str = 'value') -> bool:
    """A bool setting value: `true`/`false` in any case or the numbers 0 and 1 (`AnyData` Bool)."""
    text = value.strip(TRIM_SPACE)
    if text.lower() == 'true':
        return True
    if text.lower() == 'false':
        return False
    if text in ('0', '1'):
        return text == '1'
    raise FormatError(f'Invalid bool value for {what}: {value}')


def read_text(path: str | Path) -> str:
    """File text as the Engine sees it: UTF-8 without newline translation and without dropping a BOM."""
    try:
        with open(path, 'r', encoding='utf-8', newline='') as file:
            return file.read()
    except UnicodeDecodeError as ex:
        raise FormatError(f'File is not valid UTF-8 ({ex.reason} at byte {ex.start})', path=Path(path).as_posix()) from ex


# ---------------------------------------------------------------------------------------------------------------
# Configuration grammar (Source/Common/ConfigFile.cpp)
# ---------------------------------------------------------------------------------------------------------------

@dataclass(slots=True)
class ConfigSection:
    name: str
    values: dict[str, str]
    line: int = 0
    key_lines: dict[str, tuple[int, int]] = field(default_factory=dict)


@dataclass(slots=True)
class ConfigDocument:
    """Sections in file order; the first one is the unnamed default section."""

    sections: list[ConfigSection]

    @property
    def main(self) -> ConfigSection:
        return self.sections[0]

    def first(self, name: str) -> ConfigSection | None:
        return next((section for section in self.sections if section.name == name), None)

    def all(self, name: str) -> list[ConfigSection]:
        return [section for section in self.sections if section.name == name]

    def get(self, section: str, key: str, default: str | None = None) -> str | None:
        found = self.first(section)
        return found.values.get(key, default) if found is not None else default


@dataclass(slots=True)
class ConfigLine:
    """One line as the `ConfigFile` constructor parses it.

    `physical` holds the physical lines a continuation merged, without line terminators, `text` the merged and
    trimmed text, `first_line` and `last_line` the 1-based numbers of the first and last physical line."""

    physical: list[str]
    text: str
    first_line: int
    last_line: int


@dataclass(slots=True)
class ConfigKeyValue:
    key: str
    value: str
    append: bool
    # Index of the separating `=` in the parsed line text
    separator: int


def split_config_lines(text: str) -> list[str]:
    """Physical lines as the `ConfigFile` constructor cuts them: at LF only, one trailing CR removed.

    A final LF ends the last line instead of starting an empty one; the Engine reads that empty remainder too,
    which changes nothing because an empty line is skipped and a pending continuation is closed either way."""
    lines = text.split('\n')

    if text.endswith('\n'):
        lines.pop()

    return [line[:-1] if line.endswith('\r') else line for line in lines]


def iter_config_lines(physical_lines: Iterable[str]) -> Iterator[ConfigLine]:
    """Logical lines in file order, empty ones included.

    Each physical line is trimmed with `strvex::trim` (`TRIM_SPACE`, so form feed and vertical tab stay). A line
    ending in a backslash preceded by a space or tab continues: its text without the backslash, trimmed, plus
    one space is prepended to the next line. A continuation on the last line still forms a line."""
    group: list[str] = []
    accumulated = ''
    first_line = 0
    line_number = 0

    for line_number, physical in enumerate(physical_lines, start=1):
        if not group:
            first_line = line_number

        group.append(physical)
        line = physical.strip(TRIM_SPACE)

        if accumulated:
            line = accumulated + line
            accumulated = ''

        if len(line) >= 2 and line[-1] == '\\' and line[-2] in ' \t':
            accumulated = line[:-1].strip(TRIM_SPACE) + ' '
            continue

        yield ConfigLine(group, line, first_line, line_number)
        group = []

    if accumulated:
        yield ConfigLine(group, accumulated, first_line, line_number)


def parse_config_section_header(line: str) -> str | None:
    """Section name of a logical line starting with `[`: the text before the first `]` trimmed with
    `strvex::trim`; text after `]` is ignored. None for any other line and for a header without `]` or with an
    empty name, which the Engine ignores as a whole."""
    if not line.startswith('['):
        return None

    end = line.find(']')

    if end == -1:
        return None

    name = line[1:end].strip(TRIM_SPACE)
    return name or None


def parse_config_key_value(line: str) -> ConfigKeyValue | None:
    """Port of `ConfigFile::ParseConfigKeyValueLine`, or None when the line holds no assignment.

    An unescaped `#` outside double quotes starts a comment, the first `=` separates key and value, a `+` right
    before it marks an append, and key and value are trimmed with `TrimConfigRange` (`CONFIG_SPACE`)."""
    inside_quotes = False
    backslash_run = 0
    separator = -1
    content_end = len(line)

    for index, char in enumerate(line):
        escaped = (backslash_run & 1) != 0

        if char == '"' and not escaped:
            inside_quotes = not inside_quotes
        elif char == '#' and not inside_quotes and not escaped:
            content_end = index
            break
        elif char == '=' and separator == -1:
            separator = index

        backslash_run = backslash_run + 1 if char == '\\' else 0

    begin, end = _trim_range(line, 0, content_end)

    if begin == end or separator == -1 or separator <= begin or separator >= end:
        return None

    append = line[separator - 1] == '+'
    key_begin, key_end = _trim_range(line, begin, separator - 1 if append else separator)
    value_begin, value_end = _trim_range(line, separator + 1, end)

    if key_begin == key_end:
        return None

    return ConfigKeyValue(line[key_begin:key_end], line[value_begin:value_end], append, separator)


def parse_config_key_value_line(line: str) -> tuple[str, str, bool] | None:
    """`parse_config_key_value` as a `(key, value, append)` tuple."""
    entry = parse_config_key_value(line)
    return (entry.key, entry.value, entry.append) if entry is not None else None


def _trim_range(line: str, begin: int, end: int) -> tuple[int, int]:
    while begin < end and line[begin] in CONFIG_SPACE:
        begin += 1
    while end > begin and line[end - 1] in CONFIG_SPACE:
        end -= 1
    return begin, end


def parse_config_text(text: str, *, skip_nested_sections: bool = False) -> ConfigDocument:
    """Port of the `ConfigFile` constructor.

    Repeated sections stay separate, a repeated key replaces the earlier value, `key += value` appends with a
    space, a trailing backslash preceded by space or tab continues the line, and a header without `]` or with an
    empty name is ignored (following keys stay in the previous section). Section names containing `/` are
    nested sections; `skip_nested_sections` drops them with their keys, as `ConfigFileOption::SkipNestedSections`."""
    return parse_config_lines(split_config_lines(text), skip_nested_sections=skip_nested_sections)


def parse_config_lines(physical_lines: Iterable[str], *, skip_nested_sections: bool = False) -> ConfigDocument:
    """`parse_config_text` over physical lines already split, without their line terminators."""
    current = ConfigSection('', {}, 0, {})
    sections = [current]
    skip_current = False

    for logical in iter_config_lines(physical_lines):
        line = logical.text

        if not line:
            continue

        if line[0] == '[':
            name = parse_config_section_header(line)
            if name is None:
                continue
            if '/' in name and skip_nested_sections:
                skip_current = True
                continue
            skip_current = False
            current = ConfigSection(name, {}, logical.last_line, {})
            sections.append(current)
            continue

        if skip_current:
            continue

        entry = parse_config_key_value(line)
        if entry is None:
            continue

        if entry.append and entry.key in current.values:
            if entry.value:
                current.values[entry.key] = current.values[entry.key] + ' ' + entry.value
            current.key_lines[entry.key] = (current.key_lines[entry.key][0], logical.last_line)
        else:
            current.values[entry.key] = entry.value
            current.key_lines[entry.key] = (logical.first_line, logical.last_line)

    return ConfigDocument(sections)


def read_config_file(path: str | Path, *, skip_nested_sections: bool = False) -> ConfigDocument:
    return parse_config_text(read_text(path), skip_nested_sections=skip_nested_sections)


# ---------------------------------------------------------------------------------------------------------------
# Settings, resource packs and sub-configs (Source/Common/Settings.cpp)
# ---------------------------------------------------------------------------------------------------------------

def resolve_setting_value(name: str, value: str, config_dir: str | Path, *, baking_mode: bool = True,
                          environ: Mapping[str, str] | None = None, warnings: list[str] | None = None) -> str:
    """Port of `ResolveSettingValue`: substitutes `$ENV{NAME}` and `$FILE{path}`.

    A file path is relative to the declaring config's directory and its content is trimmed. An unknown variable
    or missing file keeps the directive text and adds a warning, as the Engine logs one. `$TARGET_ENV{}` and
    `$TARGET_FILE{}` stay verbatim in baking mode and resolve like their plain forms otherwise."""
    if '$' not in value:
        return value

    env = os.environ if environ is None else environ
    resolved = ''
    prev_pos = 0
    pos = value.find('$')

    while pos != -1:
        kinds = (('$ENV{', 'env'), ('$FILE{', 'file'), ('$TARGET_ENV{', 'target_env'), ('$TARGET_FILE{', 'target_file'))
        match = next(((prefix, kind) for prefix, kind in kinds if value.startswith(prefix, pos)), None)

        if match is None or (baking_mode and match[1].startswith('target_')):
            pos = value.find('$', pos + 1)
            continue

        prefix, kind = match
        start = pos + len(prefix)
        end_pos = value.find('}', start)

        if end_pos == -1:
            raise FormatError(f'Not closed $ tag in settings: {name} = {value}')

        directive_name = value[start:end_pos]
        substitution: str | None = None

        if kind in ('env', 'target_env'):
            substitution = env.get(directive_name) if directive_name else None
            if substitution is None and warnings is not None:
                warnings.append(f'Environment variable {directive_name} for setting {name} is not found')
        else:
            file_path = Path(directive_name) if os.path.isabs(directive_name) else Path(config_dir) / directive_name
            try:
                substitution = read_text(file_path).strip(TRIM_SPACE)
            except FileNotFoundError:
                if warnings is not None:
                    warnings.append(f'File {file_path.as_posix()} for setting {name} is not found')

        if substitution is not None:
            resolved += value[prev_pos:pos] + substitution
            prev_pos = end_pos + 1
        else:
            resolved += value[prev_pos:start] + directive_name
            prev_pos = end_pos

        pos = value.find('$', end_pos)

    return resolved + value[prev_pos:]


@dataclass(slots=True)
class ResourcePack:
    name: str
    config_dir: Path
    input_dirs: list[Path]
    input_files: list[Path]
    include_patterns: list[str]
    exclude_patterns: list[str]
    bakers: list[str]
    server_only: bool = False
    client_only: bool = False
    mapper_only: bool = False

    def has_baker(self, baker: str) -> bool:
        return baker in self.bakers


@dataclass(slots=True)
class SubConfig:
    name: str
    config_dir: Path
    settings: dict[str, str]


@dataclass(slots=True)
class ProjectConfig:
    """Effective configuration of one main config file, optionally with one sub-config applied.

    `settings` holds the text of every setting the files assign, after directive resolution; Engine
    defaults are not included (see `setting`). `resource_packs` are the packs in effect after
    `Baking.IgnoreInputDirs`; `declared_resource_packs` keep every declared input."""

    path: Path
    settings: dict[str, str]
    declared_resource_packs: list[ResourcePack]
    resource_packs: list[ResourcePack]
    sub_configs: list[SubConfig]
    applied_sub_config: str | None = None
    warnings: list[str] = field(default_factory=list)

    @property
    def config_dir(self) -> Path:
        return self.path.parent

    def setting(self, name: str, default: str | None = None) -> str | None:
        """The assigned text, else the Engine default this module knows, else `default`."""
        if name in self.settings:
            return self.settings[name]
        return ENGINE_SETTING_DEFAULTS.get(name, default)

    def setting_list(self, name: str) -> list[str]:
        return split_setting_list(self.setting(name, '') or '')

    def setting_bool(self, name: str, default: bool = False) -> bool:
        text = self.setting(name)
        return parse_bool(text, what=name) if text is not None else default

    def packs_with_baker(self, baker: str) -> list[ResourcePack]:
        return [pack for pack in self.resource_packs if pack.has_baker(baker)]


def _combine_path(config_dir: Path, path: str) -> Path:
    return Path(os.path.normpath(config_dir / path.replace('\\', '/')))


def _apply_setting(settings: dict[str, str], name: str, value: str, config_dir: Path, *, baking_mode: bool,
                   environ: Mapping[str, str] | None, warnings: list[str]) -> None:
    # `SetValue`: a leading `+` appends to the current value; strings and lists both join with one space
    append = value.startswith('+')
    text = resolve_setting_value(name, value[1:] if append else value, config_dir, baking_mode=baking_mode,
                                 environ=environ, warnings=warnings)

    if append and settings.get(name):
        settings[name] = settings[name] + ' ' + text if text else settings[name]
    else:
        settings[name] = text


def _read_resource_pack(section: ConfigSection, config_dir: Path, path: Path) -> ResourcePack:
    values = section.values
    name = values.get('Name', '')

    if not name:
        keys = '; '.join(f'{key}={value}' for key, value in sorted(values.items()))
        raise FormatError(f'Resource pack name not specified ({keys})', path=path.as_posix(), line=section.line)

    flags = {key: parse_bool(values[key], what=f'{name}.{key}') if values.get(key) else False
             for key in ('ServerOnly', 'ClientOnly', 'MapperOnly')}

    if sum(flags.values()) > 1:
        raise FormatError('Resource pack can be common or server, client or mapper only', path=path.as_posix(), line=section.line)

    return ResourcePack(
        name=name,
        config_dir=config_dir,
        input_dirs=[_combine_path(config_dir, entry) for entry in split_list(values.get('InputDirs', ''))],
        input_files=[_combine_path(config_dir, entry) for entry in split_list(values.get('InputFiles', ''))],
        include_patterns=split_list(values.get('IncludePatterns', '')),
        exclude_patterns=split_list(values.get('ExcludePatterns', '')),
        bakers=split_list(values.get('Bakers', '')),
        server_only=flags['ServerOnly'],
        client_only=flags['ClientOnly'],
        mapper_only=flags['MapperOnly'],
    )


def load_project_config(path: str | Path, *, sub_config: str | None = None, baking_mode: bool = True,
                        environ: Mapping[str, str] | None = None) -> ProjectConfig:
    """Read a main config the way `GlobalSettings::SetupConfigAtPath` and `ApplySubConfigSection` do."""
    config_path = Path(path).resolve()
    config_dir = config_path.parent
    document = read_config_file(config_path)
    warnings: list[str] = []
    settings: dict[str, str] = {}

    for key, value in document.main.values.items():
        _apply_setting(settings, key, value, config_dir, baking_mode=baking_mode, environ=environ, warnings=warnings)

    packs = [_read_resource_pack(section, config_dir, config_path) for section in document.all('ResourcePack')]
    sub_configs: list[SubConfig] = []

    for section in document.all('SubConfig'):
        name = section.values.get('Name', '')
        if not name:
            raise FormatError('Sub config name not specified', path=config_path.as_posix(), line=section.line)

        # Later parents override earlier ones per key; the section's own settings override all parents
        merged: dict[str, str] = {}
        for parent in split_list(section.values.get('Parent', '')):
            parent_config = next((cfg for cfg in sub_configs if cfg.name == parent), None)
            if parent_config is None:
                raise FormatError(f'Parent sub config not found: {parent}', path=config_path.as_posix(), line=section.line)
            merged.update(parent_config.settings)

        merged.update({key: value for key, value in section.values.items() if key not in ('Name', 'Parent')})
        sub_configs.append(SubConfig(name, config_dir, merged))

    if sub_config is not None:
        selected = next((cfg for cfg in sub_configs if cfg.name == sub_config), None)
        if selected is None:
            raise FormatError(f'Sub config not found: {sub_config}', path=config_path.as_posix())
        for key, value in sorted(selected.settings.items()):
            _apply_setting(settings, key, value, selected.config_dir, baking_mode=baking_mode, environ=environ, warnings=warnings)

    config = ProjectConfig(config_path, settings, packs, [], sub_configs, sub_config, warnings)
    config.resource_packs = _apply_ignore_input_dirs(config)
    return config


def _apply_ignore_input_dirs(config: ProjectConfig) -> list[ResourcePack]:
    # `GlobalSettings::ApplyIgnoreInputDirs`: each entry is relative to the config declaring the pack
    packs = [dataclasses.replace(pack, input_dirs=list(pack.input_dirs)) for pack in config.declared_resource_packs]
    packs_have_inputs = any(pack.input_dirs or pack.input_files for pack in packs)

    for ignored in config.setting_list('Baking.IgnoreInputDirs'):
        removed = False
        for pack in packs:
            ignored_path = _combine_path(pack.config_dir, ignored)
            kept = [input_dir for input_dir in pack.input_dirs if input_dir != ignored_path]
            removed = removed or len(kept) != len(pack.input_dirs)
            pack.input_dirs = kept

        if not removed and packs_have_inputs:
            raise FormatError(f'Ignored input directory is not an input directory of any resource pack: {ignored}',
                              path=config.path.as_posix())

    return packs


# ---------------------------------------------------------------------------------------------------------------
# Resource pack input files (DataSource-Directory.cpp, DiskFileSystem.cpp, FileSystem::FilterFiles)
# ---------------------------------------------------------------------------------------------------------------

@dataclass(frozen=True, slots=True)
class PackFile:
    path: str
    file: Path

    @property
    def extension(self) -> str:
        """`strex::get_file_extension`: the text after the last dot, lower-cased."""
        name = self.path.rsplit('/', 1)[-1]
        return name.rsplit('.', 1)[1].lower() if '.' in name else ''

    @property
    def stem(self) -> str:
        """`FileHeader::GetNameNoExt`: the file name without its last extension."""
        name = self.path.rsplit('/', 1)[-1]
        return name.rsplit('.', 1)[0] if '.' in name else name


def match_resource_glob(path: str, pattern: str) -> bool:
    """Port of `MatchResourcePathGlob`: `*` and `?` stay within one path segment, `**` crosses segments and
    `**/` also matches zero directories."""
    memo: dict[tuple[int, int], bool] = {}

    def match(pattern_pos: int, path_pos: int) -> bool:
        key = (pattern_pos, path_pos)
        if key in memo:
            return memo[key]

        if pattern_pos == len(pattern):
            result = path_pos == len(path)
        elif pattern[pattern_pos] == '*':
            next_pos = pattern_pos
            while next_pos < len(pattern) and pattern[next_pos] == '*':
                next_pos += 1
            if next_pos - pattern_pos >= 2:
                if next_pos < len(pattern) and pattern[next_pos] == '/':
                    result = match(next_pos + 1, path_pos)
                    for next_path_pos in range(path_pos, len(path)):
                        if result:
                            break
                        if path[next_path_pos] == '/':
                            result = match(pattern_pos, next_path_pos + 1)
                else:
                    result = match(next_pos, path_pos) or (path_pos < len(path) and match(pattern_pos, path_pos + 1))
            else:
                result = match(next_pos, path_pos) or (path_pos < len(path) and path[path_pos] != '/' and match(pattern_pos, path_pos + 1))
        elif pattern[pattern_pos] == '?':
            result = path_pos < len(path) and path[path_pos] != '/' and match(pattern_pos + 1, path_pos + 1)
        else:
            result = path_pos < len(path) and pattern[pattern_pos] == path[path_pos] and match(pattern_pos + 1, path_pos + 1)

        memo[key] = result
        return result

    return match(0, 0)


def list_input_dir(root: str | Path) -> list[str]:
    """Resource paths below one input directory, as the baker mounts it: recursive, `/`-separated, skipping
    entries whose name starts with `.` or `~` and directories whose name starts with `_`."""
    root_path = Path(root)

    if not root_path.is_dir():
        raise FormatError('Directory not found', path=root_path.as_posix())

    result: list[str] = []

    def walk(directory: Path, prefix: str) -> None:
        with os.scandir(directory) as entries:
            for entry in sorted(entries, key=lambda item: item.name):
                if not entry.name or entry.name[0] in '.~':
                    continue
                if entry.is_dir():
                    if entry.name[0] != '_':
                        walk(Path(entry.path), prefix + entry.name + '/')
                else:
                    result.append(prefix + entry.name)

    walk(root_path, '')
    return result


def pack_files(pack: ResourcePack, *, extensions: Collection[str] | None = None, skip_archives: bool = False) -> list[PackFile]:
    """Input files of a resource pack after its include and exclude patterns.

    A resource path present in several input directories comes from the first one, as in `FileSystem`.
    Archive inputs (`InputFiles`) are not read: they raise unless `skip_archives` is set."""
    if pack.input_files and not skip_archives:
        raise FormatError(f'Resource pack {pack.name} reads archive inputs, which this reader does not open',
                          path=pack.input_files[0].as_posix())

    includes = [pattern.replace('\\', '/') for pattern in pack.include_patterns]
    excludes = [pattern.replace('\\', '/') for pattern in pack.exclude_patterns]
    wanted = {extension.lower() for extension in extensions} if extensions is not None else None
    seen: set[str] = set()
    files: list[PackFile] = []

    for input_dir in pack.input_dirs:
        for resource_path in list_input_dir(input_dir):
            if resource_path in seen:
                continue
            seen.add(resource_path)

            if includes and not any(match_resource_glob(resource_path, pattern) for pattern in includes):
                continue
            if any(match_resource_glob(resource_path, pattern) for pattern in excludes):
                continue

            pack_file = PackFile(resource_path, input_dir / resource_path)
            if wanted is None or pack_file.extension in wanted:
                files.append(pack_file)

    return files


def project_files(config: ProjectConfig, baker: str, extensions: Collection[str]) -> list[PackFile]:
    """Files every pack using `baker` passes to it, in pack order; a file shared by two packs appears once."""
    seen: set[Path] = set()
    files: list[PackFile] = []

    for pack in config.packs_with_baker(baker):
        for pack_file in pack_files(pack, extensions=extensions):
            if pack_file.file not in seen:
                seen.add(pack_file.file)
                files.append(pack_file)

    return files


# ---------------------------------------------------------------------------------------------------------------
# Metadata tags and migration rules (Source/Tools/MetadataBaker.cpp, Source/Common/EngineBase.cpp)
# ---------------------------------------------------------------------------------------------------------------

@dataclass(frozen=True, slots=True)
class MetadataTag:
    name: str
    tokens: tuple[str, ...]
    path: str = ''
    line: int = 0


def tokenize(text: str) -> list[str]:
    """Port of `strvex::tokenize`: symbols become single tokens, whitespace separates."""
    result: list[str] = []
    current = ''

    for char in text.strip(TRIM_SPACE):
        if char in TOKEN_SYMBOLS:
            if current:
                result.append(current)
                current = ''
            result.append(char)
        elif char in ' \t\r\n':
            if current:
                result.append(current)
                current = ''
        else:
            current += char

    if current:
        result.append(current)

    return result


def tokenize_migration_rule(text: str, *, path: str = '', line: int = 0) -> list[str]:
    """MetadataBaker's MigrationRule tokenizer: double-quoted arguments are single literal tokens."""
    result: list[str] = []
    segment_begin = 0

    while True:
        quote_begin = text.find('"', segment_begin)
        result.extend(tokenize(text[segment_begin:quote_begin] if quote_begin != -1 else text[segment_begin:]))

        if quote_begin == -1:
            return result

        quote_end = text.find('"', quote_begin + 1)
        if quote_end == -1:
            raise FormatError('Invalid MigrationRule codegen tag: malformed quoted rule argument', path=path, line=line)

        result.append(text[quote_begin + 1:quote_end])
        segment_begin = quote_end + 1


def iter_metadata_tags(text: str, path: str = '') -> list[MetadataTag]:
    """Every `///@` tag of one source file, read as MetadataBaker reads script sources.

    A tag starts at `///@` anywhere on a line, a trailing backslash continues it, the first `//` after the
    marker starts a comment. Native sources are read by codegen, which additionally requires the marker to
    start the line; tags written that way read identically here."""
    tags: list[MetadataTag] = []
    pos = 0
    counted_pos = 0
    line_number = 1

    while True:
        pos = text.find('///@', pos)

        if pos == -1:
            return tags

        # Counted from the previous tag start, so lines joined by a continuation still count
        line_number += text.count('\n', counted_pos, pos)
        counted_pos = pos
        end_pos = text.find('\n', pos)

        while end_pos != -1 and (text[end_pos - 1] == '\\' or (text[end_pos - 1] == '\r' and text[end_pos - 2] == '\\')):
            end_pos = text.find('\n', end_pos + 1)

        if end_pos != -1:
            line = text[pos + 4:end_pos]
            pos = end_pos
        else:
            line = text[pos + 4:]
            pos = len(text)

        if '\\' in line:
            normalized = []
            index = 0
            while index < len(line):
                char = line[index]
                if char == '\\' and index + 1 < len(line) and line[index + 1] in '\r\n':
                    normalized.append(' ')
                    while index + 1 < len(line) and line[index + 1] in '\r\n':
                        index += 1
                    index += 1
                    continue
                normalized.append(char)
                index += 1
            line = ''.join(normalized)

        comment_pos = line.find('//')
        if comment_pos != -1:
            line = line[:comment_pos]

        tokens = tokenize(line)
        if not tokens:
            raise FormatError('Invalid codegen tag: empty tag', path=path, line=line_number)

        name = tokens[0]
        if name == 'MigrationRule':
            tokens = tokenize_migration_rule(line, path=path, line=line_number)

        tags.append(MetadataTag(name, tuple(tokens[1:]), path, line_number))


@dataclass(frozen=True, slots=True)
class MigrationRule:
    """One `///@ MigrationRule`. `action` is empty for kinds other than Property and Proto; `replacement` is
    empty for Remove and names the migrator function for Transform."""

    kind: str
    owner: str
    action: str
    source: str
    replacement: str
    path: str = ''
    line: int = 0

    def __str__(self) -> str:
        parts = [self.kind, self.owner, self.action, _quote_rule_arg(self.source), self.replacement]
        return ' '.join(part for part in parts if part)


def _quote_rule_arg(value: str) -> str:
    return f'"{value}"' if any(char.isspace() for char in value) else value


def parse_migration_rule(tag: MetadataTag) -> MigrationRule:
    """Port of `MetadataBaker::ParseMigrationRule` (syntax only): dotted names and `Namespace::Function`
    migrators are merged back into single arguments."""
    tokens = list(tag.tokens)

    def fail(message: str) -> FormatError:
        return FormatError(f'Invalid MigrationRule codegen tag: {message}', path=tag.path, line=tag.line)

    if tag.name != 'MigrationRule':
        raise fail('not a MigrationRule tag')
    if len(tokens) < 4:
        raise fail('insufficient parameters')

    def merge(parts: Sequence[str], allow_namespace: bool = False) -> str:
        if not parts:
            raise fail('empty rule argument')

        value = ''
        expect_token = True
        index = 0

        while index < len(parts):
            token = parts[index]
            if not token:
                raise fail('empty rule argument')
            if token == ':':
                if not allow_namespace or expect_token or index + 1 == len(parts) or parts[index + 1] != ':':
                    raise fail('malformed qualified function name')
                value += '::'
                expect_token = True
                index += 2
                continue
            if token == '.':
                if expect_token:
                    raise fail('malformed dotted name')
                value += '.'
                expect_token = True
                index += 1
                continue
            if not expect_token:
                raise fail('too many rule arguments')
            value += token
            expect_token = False
            index += 1

        if expect_token:
            raise fail('malformed dotted name')

        return value

    kind, owner = tokens[0], tokens[1]
    action_rule = kind in ACTION_RULE_KINDS
    action = tokens[2] if action_rule else ''

    if action_rule and action == 'Remove':
        return MigrationRule(kind, owner, action, merge(tokens[3:]), '', tag.path, tag.line)

    last_arg_begin = len(tokens) - 1
    while last_arg_begin > 2:
        if tokens[last_arg_begin - 1] == '.':
            last_arg_begin -= 2
        elif action == 'Transform' and last_arg_begin > 3 and tokens[last_arg_begin - 1] == ':' and tokens[last_arg_begin - 2] == ':':
            last_arg_begin -= 3
        else:
            break

    if last_arg_begin <= 2:
        raise fail('insufficient parameters')

    target_begin = 3 if action_rule else 2
    if action_rule and (len(tokens) < 5 or last_arg_begin <= target_begin):
        raise FormatError('Invalid MigrationRule action codegen tag: insufficient parameters', path=tag.path, line=tag.line)
    if action_rule and action not in RULE_ACTIONS:
        raise fail(f'unknown action {action}')

    source = merge(tokens[target_begin:last_arg_begin])
    replacement = merge(tokens[last_arg_begin:], action == 'Transform')

    if any(char.isspace() for char in replacement):
        raise fail('rule replacement must not contain whitespace')

    return MigrationRule(kind, owner, action, source, replacement, tag.path, tag.line)


class MigrationRules:
    """Registered rules with the Engine's lookup: `resolve` follows Rename chains to their end, as
    `EngineMetadata::CheckMigrationRule` does, and returns an empty name for Remove."""

    def __init__(self, rules: Iterable[MigrationRule]) -> None:
        self.rules: list[MigrationRule] = list(rules)
        self._renames: dict[tuple[str, str], dict[str, str]] = {}
        self._actions: dict[tuple[str, str, str], MigrationRule] = {}

        for rule in self.rules:
            key = (rule.kind, rule.owner, rule.source)
            if key in self._actions:
                previous = self._actions[key]
                raise FormatError(f'Migration source already added: {rule} (also {previous.path}:{previous.line})',
                                  path=rule.path, line=rule.line)
            self._actions[key] = rule

            if rule.action == 'Transform':
                continue
            if rule.source == rule.replacement:
                raise FormatError(f'Migration target and replacement are identical: {rule}', path=rule.path, line=rule.line)
            self._renames.setdefault((rule.kind, rule.owner), {})[rule.source] = rule.replacement

        for (kind, owner), renames in self._renames.items():
            for source in renames:
                visited = {source}
                current = renames[source]
                while current in renames:
                    if current in visited:
                        rule = self._actions[(kind, owner, source)]
                        raise FormatError(f'Migration rule chain contains a cycle: {rule}', path=rule.path, line=rule.line)
                    visited.add(current)
                    current = renames[current]

    @classmethod
    def from_tags(cls, tags: Iterable[MetadataTag]) -> MigrationRules:
        return cls(parse_migration_rule(tag) for tag in tags if tag.name == 'MigrationRule')

    def resolve(self, kind: str, owner: str, name: str) -> str | None:
        renames = self._renames.get((kind, owner))
        if renames is None or name not in renames:
            return None
        current = renames[name]
        while current in renames:
            current = renames[current]
        return current

    def renamed(self, kind: str, owner: str, name: str) -> str:
        resolved = self.resolve(kind, owner, name)
        return resolved if resolved is not None else name

    def find(self, kind: str, owner: str, source: str) -> MigrationRule | None:
        return self._actions.get((kind, owner, source))

    def of_kind(self, kind: str, actions: Collection[str] | None = None) -> list[MigrationRule]:
        return [rule for rule in self.rules if rule.kind == kind and (actions is None or rule.action in actions)]


# ---------------------------------------------------------------------------------------------------------------
# Prototypes (Source/Tools/ProtoBaker.cpp, Source/Tools/ProtoTextBaker.cpp)
# ---------------------------------------------------------------------------------------------------------------

@dataclass(slots=True)
class PrototypeSource:
    """One authored prototype section, identified by its migration-resolved name."""

    type_name: str
    name: str
    authored_name: str
    path: str
    line: int
    values: dict[str, str]
    key_lines: dict[str, tuple[int, int]] = field(default_factory=dict)

    @property
    def parents(self) -> list[str]:
        return split_list(self.values.get('$Parent', ''))


@dataclass(slots=True)
class ResolvedPrototype:
    """Effective values: `properties` are the keys ProtoBaker applies (no `$` prefix), `texts` the `$Text`
    keys ProtoTextBaker applies. `*_sources` name the prototype whose own section supplied each value."""

    source: PrototypeSource
    properties: dict[str, str]
    texts: dict[str, str]
    property_sources: dict[str, str]
    text_sources: dict[str, str]
    ancestors: list[str]


@dataclass(slots=True)
class PrototypeSet:
    prototypes: dict[tuple[str, str], PrototypeSource]
    issues: list[FormatIssue]


def proto_section_type(section_name: str, fixed_types: Collection[str] | None) -> str | None:
    """`Proto<Type>` names an entity type; any other name must be a FixedType. With `fixed_types` None every
    other section is taken as a FixedType, because only baked metadata knows the declared set."""
    if section_name.startswith('Proto') and len(section_name) > len('Proto'):
        return section_name[len('Proto'):]
    if fixed_types is None or section_name in fixed_types:
        return section_name
    return None


def collect_prototypes(files: Iterable[tuple[str, str]], *, fixed_types: Collection[str] | None = None,
                       migrations: MigrationRules | None = None) -> PrototypeSet:
    """Prototype sections of `(resource path, text)` pairs, keyed by `(type, migrated name)`.

    Issues follow the ProtoBaker rejections: INVALID_SECTION, INVALID_PROTO_NAME and DUPLICATE_PROTO. Entity
    types are not checked against metadata."""
    prototypes: dict[tuple[str, str], PrototypeSource] = {}
    issues: list[FormatIssue] = []

    for path, text in files:
        stem = PackFile(path.replace('\\', '/'), Path(path)).stem
        document = parse_config_text(text, skip_nested_sections=True)

        for section in document.sections[1:]:
            type_name = proto_section_type(section.name, fixed_types)
            if type_name is None:
                issues.append(FormatIssue('INVALID_SECTION', f'Invalid proto section name {section.name}', path, section.line))
                continue

            authored = section.values.get('$Name', stem)
            if any(char in authored for char in ' \t\r\n'):
                issues.append(FormatIssue('INVALID_PROTO_NAME', f'Proto name must not contain whitespace: {authored}', path, section.line))
                continue
            if '/' in authored or '$' in authored:
                issues.append(FormatIssue('INVALID_PROTO_NAME', f'Proto name must not contain a slash or dollar sign: {authored}', path, section.line))
                continue

            name = migrations.renamed('Proto', type_name, authored) if migrations is not None else authored
            key = (type_name, name)

            if key in prototypes:
                first = prototypes[key]
                issues.append(FormatIssue('DUPLICATE_PROTO', f'Proto already loaded: {type_name} {name} (first at {first.path}:{first.line})', path, section.line))
                continue

            prototypes[key] = PrototypeSource(type_name, name, authored, path, section.line, dict(section.values), dict(section.key_lines))

    return PrototypeSet(prototypes, issues)


class _ResolveFailure(Exception):
    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code


def resolve_prototypes(prototypes: PrototypeSet, *, allow_repeated_parents: bool = True,
                       migrations: MigrationRules | None = None) -> tuple[dict[tuple[str, str], ResolvedPrototype], list[FormatIssue]]:
    """Apply `$Parent` inheritance as `fill_parent_recursive` does in both bakers.

    Parents apply left to right, each after its own ancestors, and the child applies last; a later write of a
    key replaces the earlier value whole. An ancestor reached twice contributes only where it is first reached,
    or fails with REPEATED_PARENT when `allow_repeated_parents` is false (`Baking.AllowRepeatedProtoParents`).
    MISSING_PARENT and PARENT_CYCLE fail the prototype. Parent names are migration-resolved."""
    by_type: dict[str, dict[str, PrototypeSource]] = {}
    for (type_name, name), source in prototypes.prototypes.items():
        by_type.setdefault(type_name, {})[name] = source

    resolved: dict[tuple[str, str], ResolvedPrototype] = {}
    issues: list[FormatIssue] = []

    for key in sorted(prototypes.prototypes):
        source = prototypes.prototypes[key]
        try:
            resolved[key] = _resolve_one(source, by_type[source.type_name], allow_repeated_parents, migrations)
        except _ResolveFailure as failure:
            issues.append(FormatIssue(failure.code, str(failure), source.path, source.line))

    return resolved, issues


def _resolve_one(base: PrototypeSource, same_type: Mapping[str, PrototypeSource], allow_repeated: bool,
                 migrations: MigrationRules | None) -> ResolvedPrototype:
    result = ResolvedPrototype(base, {}, {}, {}, {}, [])
    reached: set[str] = set()
    parent_path: list[str] = []

    def apply(source: PrototypeSource) -> None:
        for key, value in source.values.items():
            if key.startswith('$Text'):
                result.texts[key] = value
                result.text_sources[key] = source.name
            elif not key.startswith('$'):
                result.properties[key] = value
                result.property_sources[key] = source.name

    def walk(name: str, source: PrototypeSource) -> None:
        for parent_name in source.parents:
            parent_id = migrations.renamed('Proto', base.type_name, parent_name) if migrations is not None else parent_name
            parent = same_type.get(parent_id)

            if parent is None:
                where = '' if name == base.name else f' (parent of {name})'
                raise _ResolveFailure('MISSING_PARENT', f'Proto {base.type_name} {base.name} fails to load parent {parent_name}{where}')
            if parent_id in parent_path:
                raise _ResolveFailure('PARENT_CYCLE', f'Proto parent chain contains a cycle: {" -> ".join([base.name, *parent_path, parent_id])}')
            if parent_id in reached:
                if not allow_repeated:
                    raise _ResolveFailure('REPEATED_PARENT', f'Proto {base.name} reaches parent {parent_id} through several inheritance paths')
                continue

            reached.add(parent_id)
            parent_path.append(parent_id)
            walk(parent_name, parent)
            parent_path.pop()
            apply(parent)
            result.ancestors.append(parent_id)

    walk(base.name, base)
    apply(base)
    return result


@dataclass(frozen=True, slots=True)
class TextEntry:
    collection: str
    key1: str
    key2: str = ''
    key3: str = ''
    text: str = ''
    path: str = ''
    line: int = 0

    @property
    def key(self) -> tuple[str, str, str, str]:
        return self.collection, self.key1, self.key2, self.key3


def prototype_text_entries(prototype: ResolvedPrototype, languages: BakeLanguages) -> tuple[dict[str, list[TextEntry]], list[FormatIssue]]:
    """Text entries a resolved prototype contributes per language, as ProtoTextBaker builds them.

    `$Text [Language] [Key2] [Key3] = Value`; an omitted language is the first bake language, the prototype
    id is Key1 and the value is decoded with `decode_string`. A language outside `languages` is reported as
    UNSUPPORTED_LANGUAGE and dropped, as the Engine warns and drops it."""
    source = prototype.source
    collection = PROTO_TEXT_PACKS.get(source.type_name, PROTO_TEXT_FALLBACK_PACK)
    entries: dict[str, list[TextEntry]] = {}
    issues: list[FormatIssue] = []

    for key in sorted(prototype.texts):
        tokens = split_list(key)
        if len(tokens) > 4:
            raise FormatError(f'Prototype text key has too many tokens: {key}', path=source.path, line=source.line)

        language = tokens[1] if len(tokens) >= 2 else languages.default
        if language not in languages.languages:
            issues.append(FormatIssue('UNSUPPORTED_LANGUAGE', f'Unsupported language {language} in proto {source.name}', source.path, source.line))
            continue

        text = decode_string(prototype.texts[key])
        entries.setdefault(language, []).append(TextEntry(collection, source.name, tokens[2] if len(tokens) >= 3 else '',
                                                          tokens[3] if len(tokens) >= 4 else '', text, source.path, source.line))

    return entries, issues


def decode_string(text: str) -> str:
    """Port of `StringEscaping::DecodeString`: `\\n`, `\\"` and `\\\\` decode, `\\r` is dropped, other escapes stay
    literal, and a value starting with `"` must be closed by its last character."""
    if not text:
        return ''

    protected = text[0] == '"'
    result: list[str] = []
    closed = False
    index = 1 if protected else 0

    while index < len(text):
        char = text[index]

        if protected and char == '"':
            if index != len(text) - 1:
                raise FormatError(f'Unexpected closing quote inside protected string: {text}')
            closed = True
            break

        if char == '\\':
            index += 1
            if index >= len(text):
                raise FormatError(f'Invalid escape sequence in string: {text}')
            escaped = text[index]
            if escaped == 'n':
                result.append('\n')
            elif escaped in '"\\':
                result.append(escaped)
            elif escaped != 'r':
                result.append('\\' + escaped)
        else:
            result.append(char)

        index += 1

    if protected and not closed:
        raise FormatError(f'Unterminated protected string: {text}')

    return ''.join(result)


# ---------------------------------------------------------------------------------------------------------------
# Text packs (Source/Common/TextPack.cpp, Source/Tools/TextBaker.cpp)
# ---------------------------------------------------------------------------------------------------------------

@dataclass(frozen=True, slots=True)
class BakeLanguages:
    languages: tuple[str, ...]
    fallbacks: Mapping[str, str]

    @property
    def default(self) -> str:
        return self.languages[0]

    def fallback_of(self, language: str) -> str:
        return self.fallbacks.get(language, self.default)


def parse_bake_languages(declarations: Sequence[str]) -> BakeLanguages:
    """Port of `TextPack::ParseBakeLanguages`: `language` or `language:parent`, parents declared first."""
    if not declarations:
        raise FormatError('BakeLanguages must contain at least one language declaration')

    languages: list[str] = []
    fallbacks: dict[str, str] = {}

    for declaration in declarations:
        separator = declaration.find(':')
        has_fallback = separator != -1
        if not declaration or (has_fallback and (separator == 0 or separator + 1 >= len(declaration) or ':' in declaration[separator + 1:])):
            raise FormatError(f'Bake language declaration must use language or language:parent format: {declaration}')

        language = declaration[:separator] if has_fallback else declaration
        if language in languages:
            raise FormatError(f'Bake language is declared more than once: {language}')

        if has_fallback:
            parent = declaration[separator + 1:]
            if parent not in languages:
                raise FormatError(f'Bake language fallback parent must be declared before its child: {language}:{parent}')
            fallbacks[language] = parent

        languages.append(language)

    return BakeLanguages(tuple(languages), fallbacks)


@dataclass(slots=True)
class TextPack:
    collection: str
    language: str
    entries: list[TextEntry]
    issues: list[FormatIssue] = field(default_factory=list)

    def values(self, key1: str, key2: str = '', key3: str = '') -> list[str]:
        """Every variant under one key, in authored order."""
        key = (self.collection, key1, key2, key3)
        return [entry.text for entry in self.entries if entry.key == key]


def text_pack_name(path: str | Path) -> tuple[str, str]:
    """`(collection, language)` from `<TextPack>.<Language>.fotxt`, rejected otherwise as TextBaker does."""
    stem = PackFile(Path(path).as_posix(), Path(path)).stem
    parts = [part.strip(TRIM_SPACE) for part in stem.split('.') if part.strip(TRIM_SPACE)]
    if len(parts) != 2:
        raise FormatError(f'Text filename must contain pack name and language suffix: {stem}', path=Path(path).as_posix())
    return parts[0], parts[1]


def _extract_brace_token(lines: list[str], line_index: int, line: str, offset: int, multiline: bool) -> tuple[str, int, str, int] | None:
    # `ExtractBraceToken`: returns (token, new offset, possibly extended line, index of its last physical line)
    first = line.find('{', offset)
    if first == -1:
        return None

    last = line.find('}', first)
    while last == -1 and multiline and line_index + 1 < len(lines):
        line_index += 1
        line += '\n' + lines[line_index]
        last = line.find('}', first)

    if last == -1:
        return None

    return line[first + 1:last], last + 1, line, line_index


def parse_text_pack(text: str, collection: str, *, language: str = '', path: str = '') -> TextPack:
    """Port of `TextPack::LoadFromString`.

    Every physical line holds `{Key1}{Key2}{Text}`; a line without `{` is skipped, only the text may continue
    over following lines (newlines and any carriage returns are kept) and ends at the first `}`. Text has no
    escapes. A line whose first field opens but never closes is skipped silently. Valid entries are kept when
    another line fails; failures are listed in `issues`, which makes TextBaker reject the file."""
    lines = text.split('\n')
    if lines and lines[-1] == '' and text.endswith('\n'):
        lines.pop()

    entries: list[TextEntry] = []
    issues: list[FormatIssue] = []
    index = 0

    while index < len(lines):
        start_line = index + 1
        line = lines[index]
        first = _extract_brace_token(lines, index, line, 0, False)

        if first is None:
            index += 1
            continue

        key1, offset, line, _ = first
        second = _extract_brace_token(lines, index, line, offset, False)

        if second is None:
            issues.append(FormatIssue('MALFORMED_ENTRY', f'Entry {{{key1}}} has no second brace field', path, start_line))
            index += 1
            continue

        key2, offset, line, _ = second
        third = _extract_brace_token(lines, index, line, offset, True)

        if third is None:
            issues.append(FormatIssue('MALFORMED_ENTRY', f'Entry {{{key1}}}{{{key2}}} has no closed text field', path, start_line))
            index = len(lines) if line.find('{', offset) != -1 else index + 1
            continue

        value, _, line, last_index = third
        index = last_index + 1

        if not collection or not key1:
            issues.append(FormatIssue('MALFORMED_ENTRY', 'Entry has an empty first key or the pack has no name', path, start_line))
            continue

        entries.append(TextEntry(collection, key1, key2, '', value, path, start_line))

    return TextPack(collection, language, entries, issues)


def read_text_pack(path: str | Path) -> TextPack:
    collection, language = text_pack_name(path)
    return parse_text_pack(read_text(path), collection, language=language, path=Path(path).as_posix())


def normalize_language_packs(languages: BakeLanguages, packs: Mapping[str, Mapping[str, Sequence[TextEntry]]]) -> dict[str, dict[str, list[TextEntry]]]:
    """Port of `TextPack::FixPacks` over `{language: {collection: entries}}`.

    Unsupported languages are dropped and missing ones added. Every non-default language, in declaration
    order, keeps exactly the collections and keys of its fallback (its declared parent, else the default
    language): a missing key takes the fallback's first variant, a key absent from the fallback is removed."""
    result: dict[str, dict[str, list[TextEntry]]] = {language: {name: list(entries) for name, entries in packs.get(language, {}).items()}
                                                     for language in languages.languages}

    for language in languages.languages[1:]:
        fallback = result[languages.fallback_of(language)]
        own = result[language]
        normalized: dict[str, list[TextEntry]] = {}

        for name, base_entries in fallback.items():
            current = own.get(name)
            normalized[name] = _fix_entries(list(current) if current is not None else list(base_entries), base_entries)

        result[language] = normalized

    return result


def _fix_entries(entries: list[TextEntry], base: Sequence[TextEntry]) -> list[TextEntry]:
    present = {entry.key for entry in entries}
    base_keys: set[tuple[str, str, str, str]] = set()
    missing: list[TextEntry] = []

    for entry in base:
        if entry.key in base_keys:
            continue
        base_keys.add(entry.key)
        if entry.key not in present:
            missing.append(entry)

    return [entry for entry in entries + missing if entry.key in base_keys]


# ---------------------------------------------------------------------------------------------------------------
# Project-level helpers
# ---------------------------------------------------------------------------------------------------------------

def project_prototype_files(config: ProjectConfig) -> list[PackFile]:
    """Prototype inputs of every `Proto` baker pack, filtered by `Baking.ProtoFileExtensions`."""
    return project_files(config, PROTO_BAKER, config.setting_list('Baking.ProtoFileExtensions'))


def project_metadata_sources(config: ProjectConfig) -> list[PackFile]:
    """Script sources MetadataBaker reads: `.fos` and `.cs` files of every `Metadata` baker pack."""
    return project_files(config, METADATA_BAKER, METADATA_SOURCE_EXTENSIONS)


def project_text_packs(config: ProjectConfig) -> list[PackFile]:
    return project_files(config, TEXT_BAKER, (TEXT_PACK_EXTENSION,))


def read_pack_files(files: Iterable[PackFile]) -> list[tuple[str, str]]:
    return [(pack_file.file.as_posix(), read_text(pack_file.file)) for pack_file in files]


def load_project_prototypes(config: ProjectConfig, *, fixed_types: Collection[str] | None = None,
                            migrations: MigrationRules | None = None) -> tuple[PrototypeSet, dict[tuple[str, str], ResolvedPrototype], list[FormatIssue]]:
    """Collect and resolve every prototype the project's Proto packs bake."""
    if migrations is None:
        tags = [tag for path, text in read_pack_files(project_metadata_sources(config)) for tag in iter_metadata_tags(text, path)]
        migrations = MigrationRules.from_tags(tags)

    prototypes = collect_prototypes(read_pack_files(project_prototype_files(config)), fixed_types=fixed_types, migrations=migrations)
    resolved, issues = resolve_prototypes(prototypes, allow_repeated_parents=config.setting_bool('Baking.AllowRepeatedProtoParents'),
                                          migrations=migrations)
    return prototypes, resolved, prototypes.issues + issues


# ---------------------------------------------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------------------------------------------

def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog='fo_formats.py', description='Print Engine content formats as the Engine reads them, as JSON.')
    subparsers = parser.add_subparsers(dest='command', required=True)

    config = subparsers.add_parser('config', help='Effective settings, resource packs and sub-configs of a main config.')
    config.add_argument('main_config', type=Path, help='Main configuration file (.fomain).')
    config.add_argument('--sub-config', help='Apply this [SubConfig] section after the main settings.')
    config.add_argument('--files', action='store_true', help='Also list the input files of every resource pack.')

    protos = subparsers.add_parser('protos', help='Effective prototype values of every Proto baker pack.')
    protos.add_argument('main_config', type=Path, help='Main configuration file (.fomain).')
    protos.add_argument('--sub-config', help='Apply this [SubConfig] section after the main settings.')
    protos.add_argument('--type', dest='type_name', help='Print only prototypes of this type.')
    protos.add_argument('--name', help='Print only the prototype with this migrated name.')

    text = subparsers.add_parser('text', help='Entries of one .fotxt text pack file.')
    text.add_argument('path', type=Path, help='Text pack file named <TextPack>.<Language>.fotxt.')

    return parser


def _print_json(value: object) -> None:
    sys.stdout.write(json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + '\n')


def _pack_json(pack: ResourcePack, files: list[PackFile] | None) -> dict[str, object]:
    value: dict[str, object] = {
        'name': pack.name,
        'input_dirs': [path.as_posix() for path in pack.input_dirs],
        'input_files': [path.as_posix() for path in pack.input_files],
        'include_patterns': pack.include_patterns,
        'exclude_patterns': pack.exclude_patterns,
        'bakers': pack.bakers,
        'server_only': pack.server_only,
        'client_only': pack.client_only,
        'mapper_only': pack.mapper_only,
    }
    if files is not None:
        value['files'] = [pack_file.path for pack_file in files]
    return value


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)

    if isinstance(sys.stdout, io.TextIOWrapper):
        sys.stdout.reconfigure(encoding='utf-8')

    try:
        if args.command == 'config':
            config = load_project_config(args.main_config, sub_config=args.sub_config)
            _print_json({
                'path': config.path.as_posix(),
                'settings': config.settings,
                'resource_packs': [_pack_json(pack, pack_files(pack, skip_archives=True) if args.files else None) for pack in config.resource_packs],
                'sub_configs': {sub.name: sub.settings for sub in config.sub_configs},
                'warnings': config.warnings,
            })
            return 0

        if args.command == 'protos':
            config = load_project_config(args.main_config, sub_config=args.sub_config)
            _, resolved, issues = load_project_prototypes(config)
            selected = {f'{type_name} {name}': {'path': value.source.path, 'line': value.source.line,
                                                 'parents': value.source.parents, 'properties': value.properties,
                                                 'texts': value.texts}
                        for (type_name, name), value in sorted(resolved.items())
                        if (args.type_name is None or type_name == args.type_name) and (args.name is None or name == args.name)}
            _print_json({'prototypes': selected, 'issues': [str(issue) for issue in issues]})
            return 1 if issues else 0

        pack = read_text_pack(args.path)
        _print_json({'collection': pack.collection, 'language': pack.language,
                     'entries': [{'key1': entry.key1, 'key2': entry.key2, 'text': entry.text, 'line': entry.line} for entry in pack.entries],
                     'issues': [str(issue) for issue in pack.issues]})
        return 1 if pack.issues else 0

    except (FormatError, OSError, UnicodeDecodeError) as ex:
        sys.stderr.write(f'fo_formats: {ex}\n')
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
