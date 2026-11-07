#!/usr/bin/env python3

"""Format FOnline content files without changing what the Engine reads from them.

Three file kinds are covered:

* the main ``.fomain`` config: main-section settings are grouped by the part before the first dot, groups
  are separated by one blank line and sorted case-insensitively, settings inside a group follow the
  declaration order of ``Source/Common/Settings.inc`` and then the project ``///@ Setting`` tags, keys
  without a group come first; ``[SubConfig]`` sections get ``Name`` and ``Parent`` first and the same
  ordering without blank lines between groups; every other section is kept as authored;
* prototype files (every resource pack with the ``Proto`` baker, files whose extension is listed in
  ``Baking.ProtoFileExtensions``): each ``[Proto<Type>]`` section is laid out as ``$Name``/``$Parent``,
  plain properties in metadata declaration order, component markers with their fields, then ``$Text``;
  a missing ``$Name`` is written out as the file name the baker uses anyway. ``[ProtoMap]`` sections
  belong to the Mapper, which writes them itself, and fixed-type or nested sections are kept as authored;
* optionally AngelScript ``.fos`` files, through the Engine clang-format wrapper in ``buildtools.py``.

A comment block directly above a setting or property moves with it. A comment block at the end of a
section body describes the following section and moves to it. Every rewrite of a config or prototype file
is parsed again with ``foconfig`` (which mirrors ``Source/Common/ConfigFile.cpp``) and refused when any
section, key or value would differ. A UTF-8 BOM and the file's LF or CRLF convention are preserved.

Metadata order is the ``MetadataBaker`` order: the files of every ``Metadata`` resource pack sorted by
their path inside the pack. Unknown keys keep their authored order after known ones.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence

import foconfig


ENGINE_ROOT = Path(__file__).resolve().parents[1]
SETTINGS_INCLUDE = ENGINE_ROOT / 'Source' / 'Common' / 'Settings.inc'
LOG_PREFIX = '[ContentFormatter]'
EXIT_OK = 0
EXIT_DRIFT = 1
EXIT_ERROR = 2

UTF8_BOM = '\ufeff'
SPACE = foconfig.CONFIG_SPACE
METADATA_EXTENSIONS = ('fos', 'cs')
MAPPER_OWNED_SECTIONS = ('ProtoMap',)
ENGINE_SETTING_RE = re.compile(r'^SETTING\(\s*(?:[^,<]|<[^>]*>)+,\s*([A-Za-z0-9_]+)\s*,\s*([A-Za-z0-9_]+)\s*[,)]')
ENGINE_SETTING_DEFAULT_RE = re.compile(r'^SETTING\(\s*(?:[^,<]|<[^>]*>)+,\s*{group}\s*,\s*{name}\s*,(.*)\);')

BLANK = 'blank'
COMMENT = 'comment'
HEADER = 'header'
ENTRY = 'entry'
OTHER = 'other'
UNKNOWN_RANK = 10**9


class ContentFormatError(Exception):
    pass


@dataclass(slots=True)
class Line:
    # One logical line as the Engine reader sees it, kept as its physical lines without trailing spaces
    physical: list[str]
    kind: str
    name: str = ''
    entry: foconfig.KeyValue | None = None

    @property
    def key(self) -> str | None:
        return self.entry.key if self.entry is not None else None


@dataclass(slots=True)
class PropertyOrder:
    rank: int
    component_root: str


@dataclass(slots=True)
class EntityOrdering:
    properties: dict[str, PropertyOrder] = field(default_factory=dict)
    components: dict[str, int] = field(default_factory=dict)


@dataclass(slots=True)
class FormatResult:
    path: Path
    changed: bool


# Shared line model


def read_lines(text: str) -> list[Line]:
    lines: list[Line] = []

    for logical in foconfig.iter_logical_lines(foconfig.split_physical_lines(text)):
        physical = [part.rstrip(SPACE) for part in logical.physical]

        # A continuation closed by the end of the file would swallow whatever follows it once moved; a blank line
        # closes it the same way
        if len(physical[-1]) >= 2 and physical[-1][-1] == '\\' and physical[-1][-2] in ' \t':
            physical.append('')

        if not logical.text:
            lines.append(Line(physical, BLANK))
        elif logical.text.startswith('['):
            name = foconfig.parse_section_header(logical.text)
            lines.append(Line(physical, HEADER, name) if name is not None else Line(physical, OTHER))
        elif (entry := foconfig.parse_key_value(logical.text)) is not None:
            lines.append(Line(physical, ENTRY, entry=entry))
        elif logical.text.startswith('#'):
            lines.append(Line(physical, COMMENT))
        else:
            lines.append(Line(physical, OTHER))

    return lines


def stripped_lines(line: Line) -> list[str]:
    return [line.physical[0].strip(SPACE), *line.physical[1:]]


def split_trailing_section_comments(lines: list[Line]) -> tuple[list[Line], list[str], bool]:
    # A comment block at the very end of a section body documents the next section: it is handed over as that
    # section's leading comments, remembering whether a blank line kept it detached from the header
    index = len(lines) - 1
    separate_header_from_comments = False

    while index >= 0 and lines[index].kind == BLANK:
        separate_header_from_comments = True
        index -= 1

    if index < 0 or lines[index].kind != COMMENT:
        return lines, [], False

    comment_end = index

    while index >= 0 and lines[index].kind in (COMMENT, BLANK):
        index -= 1

    comments = [text for line in lines[index + 1 : comment_end + 1] if line.kind == COMMENT for text in stripped_lines(line)]
    return lines[: index + 1], comments, separate_header_from_comments


@dataclass(slots=True)
class Section:
    header: Line
    body: list[Line]
    leading_comments: list[str]
    separate_header_from_comments: bool


def split_sections(lines: list[Line]) -> tuple[list[Line], list[Section]]:
    first_header = next((index for index, line in enumerate(lines) if line.kind == HEADER), len(lines))
    preamble, leading_comments, separate = split_trailing_section_comments(lines[:first_header])
    sections: list[Section] = []
    index = first_header

    while index < len(lines):
        header = lines[index]
        index += 1
        body_start = index

        while index < len(lines) and lines[index].kind != HEADER:
            index += 1

        body = lines[body_start:index]
        next_leading: list[str] = []
        next_separate = False

        if index < len(lines):
            body, next_leading, next_separate = split_trailing_section_comments(body)

        sections.append(Section(header, body, leading_comments, separate))
        leading_comments = next_leading
        separate = next_separate

    return preamble, sections


def section_prefix(section: Section) -> list[str]:
    prefix = list(section.leading_comments)

    if prefix and section.separate_header_from_comments:
        prefix.append('')

    prefix.extend(stripped_lines(section.header))
    return prefix


# Main config


@dataclass(slots=True)
class SettingEntry:
    line: Line
    comments: list[str]
    index: int


def normalize_comment_block(lines: list[str]) -> list[str]:
    block = [line.rstrip(SPACE) for line in lines]

    while block and not block[0].strip(SPACE):
        block.pop(0)

    while block and not block[-1].strip(SPACE):
        block.pop()

    return block


def render_setting(line: Line) -> list[str]:
    if len(line.physical) != 1 or line.entry is None:
        return stripped_lines(line)

    text = line.physical[0].strip(SPACE)
    operator = '+=' if line.entry.append else '='
    rest = text[line.entry.separator + 1 :].strip(SPACE)
    return [f'{line.entry.key} {operator} {rest}' if rest else f'{line.entry.key} {operator}']


def physical_block(lines: list[Line]) -> list[str]:
    return normalize_comment_block([physical for line in lines for physical in line.physical])


def parse_setting_entries(lines: list[Line]) -> tuple[list[str], list[SettingEntry], list[str]]:
    entries: list[SettingEntry] = []
    pending: list[Line] = []
    leading: list[str] = []

    for line in lines:
        if line.kind != ENTRY:
            pending.append(line)
            continue

        # Before the first setting, text up to the last blank line belongs to the section; the rest to the setting
        if not entries:
            split = max((index + 1 for index, item in enumerate(pending) if item.kind == BLANK), default=0)
            leading = physical_block(pending[:split])
            pending = pending[split:]

        entries.append(SettingEntry(line, physical_block(pending), len(entries)))
        pending = []

    return leading, entries, physical_block(pending)


def setting_group(entry: SettingEntry) -> str:
    return entry.line.entry.key.split('.', 1)[0] if entry.line.entry is not None else ''


def sort_settings(entries: list[SettingEntry], setting_order: dict[str, int]) -> list[SettingEntry]:
    unknown: dict[str, int] = {}

    def rank(name: str) -> int:
        if name in setting_order:
            return setting_order[name]
        return unknown.setdefault(name, len(setting_order) + len(unknown))

    return sorted(entries, key=lambda entry: (setting_group(entry).lower(), setting_group(entry), rank(entry.line.key or ''), entry.index))


def render_entry_block(entries: list[SettingEntry]) -> list[str]:
    # Leading comments stay attached to settings; blank lines separate groups only
    lines: list[str] = []

    for entry in entries:
        lines.extend(entry.comments)
        lines.extend(render_setting(entry.line))

    return lines


def append_block(target: list[str], block: list[str]) -> None:
    if not block:
        return

    if target and target[-1] != '':
        target.append('')

    target.extend(block)


def format_settings_section(lines: list[Line], setting_order: dict[str, int], metadata_keys: tuple[str, ...] = (), *, separate_groups: bool = True) -> list[str]:
    leading, entries, tail = parse_setting_entries(lines)
    metadata_rank = {key: index for index, key in enumerate(metadata_keys)}

    metadata_entries = sorted((entry for entry in entries if entry.line.key in metadata_rank), key=lambda entry: (metadata_rank[entry.line.key or ''], entry.index))
    grouped_entries = sort_settings([entry for entry in entries if entry.line.key not in metadata_rank and '.' in (entry.line.key or '')], setting_order)
    other_entries = [entry for entry in entries if entry.line.key not in metadata_rank and '.' not in (entry.line.key or '')]

    result = list(leading)
    append_block(result, render_entry_block(metadata_entries))
    append_block(result, render_entry_block(other_entries))

    group_block: list[SettingEntry] = []

    for entry in grouped_entries:
        if group_block and setting_group(entry) != setting_group(group_block[-1]):
            if separate_groups:
                append_block(result, render_entry_block(group_block))
            else:
                result.extend(render_entry_block(group_block))
            group_block = []

        group_block.append(entry)

    if separate_groups:
        append_block(result, render_entry_block(group_block))
    else:
        result.extend(render_entry_block(group_block))

    trailing = tail

    if result and trailing:
        result.append('')

    result.extend(trailing)
    return result


def format_fomain_lines(lines: list[Line], setting_order: dict[str, int]) -> list[str]:
    main_lines, sections = split_sections(lines)
    formatted = format_settings_section(main_lines, setting_order)

    for section in sections:
        if formatted and formatted[-1] != '':
            formatted.append('')

        formatted.extend(section.leading_comments)

        if section.leading_comments and section.separate_header_from_comments:
            formatted.append('')

        formatted.extend(section.header.physical)

        if section.header.name == 'SubConfig':
            formatted.extend(format_settings_section(section.body, setting_order, metadata_keys=('Name', 'Parent'), separate_groups=False))
        else:
            formatted.extend(physical for line in section.body for physical in line.physical)

    return formatted


def format_fomain_text(text: str, setting_order: dict[str, int]) -> str:
    return '\n'.join(format_fomain_lines(read_lines(text), setting_order))


# Prototype files


@dataclass(slots=True)
class ProtoEntry:
    lines: list[str]
    comments: list[str]
    key: str | None
    index: int

    def emit_lines(self) -> list[str]:
        return [*self.comments, *self.lines]


def parse_proto_entries(lines: list[Line]) -> list[ProtoEntry]:
    entries: list[ProtoEntry] = []
    pending: list[str] = []

    for line in lines:
        if line.kind == BLANK:
            continue

        if line.kind == COMMENT:
            pending.extend(stripped_lines(line))
            continue

        entries.append(ProtoEntry(stripped_lines(line), pending, line.key, len(entries)))
        pending = []

    if pending:
        entries.append(ProtoEntry([], pending, None, len(entries)))

    return entries


def proto_section_entity(name: str) -> str | None:
    if not name.startswith('Proto') or len(name) == len('Proto') or '/' in name or name in MAPPER_OWNED_SECTIONS:
        return None

    return name[len('Proto') :]


def resolve_component_root(ordering: EntityOrdering | None, key: str, known_roots: set[str]) -> str:
    if '.' in key:
        return key.split('.', 1)[0]

    if (ordering is not None and key in ordering.components) or key in known_roots:
        return key

    return ''


def detect_component_roots(entries: list[ProtoEntry], ordering: EntityOrdering | None) -> set[str]:
    roots = set(ordering.components) if ordering is not None else set()
    roots.update(entry.key.split('.', 1)[0] for entry in entries if entry.key and '.' in entry.key)
    return roots


def header_sort_key(entry: ProtoEntry) -> tuple[int, int]:
    return ({'$Name': 0, '$Parent': 1}.get(entry.key or '', 2), entry.index)


def plain_sort_key(ordering: EntityOrdering | None, entry: ProtoEntry) -> tuple[int, int, int]:
    info = ordering.properties.get(entry.key) if ordering is not None and entry.key is not None else None

    if info is None or info.component_root:
        return (1, UNKNOWN_RANK, entry.index)

    return (0, info.rank, entry.index)


def component_sort_key(ordering: EntityOrdering | None, entry: ProtoEntry, known_roots: set[str], root_indices: dict[str, int]) -> tuple[int, int, int, int, int, int]:
    root = resolve_component_root(ordering, entry.key or '', known_roots)
    root_rank = ordering.components.get(root) if ordering is not None else None
    root_order = (0, root_rank) if root_rank is not None else (1, UNKNOWN_RANK)
    root_index = root_indices.get(root, entry.index)

    if entry.key == root:
        return (*root_order, root_index, 0, -1, entry.index)

    info = ordering.properties.get(entry.key or '') if ordering is not None else None

    if info is None:
        return (*root_order, root_index, 2, UNKNOWN_RANK, entry.index)

    return (*root_order, root_index, 1, info.rank, entry.index)


def emit_component_entries(lines: list[str], entries: list[ProtoEntry], ordering: EntityOrdering | None, known_roots: set[str]) -> None:
    previous_root = ''

    for entry in entries:
        root = resolve_component_root(ordering, entry.key or '', known_roots)

        if previous_root and root and root != previous_root:
            lines.append('')

        lines.extend(entry.emit_lines())

        if root:
            previous_root = root


def format_proto_section(section: Section, ordering: dict[str, EntityOrdering], default_name: str | None) -> list[str]:
    lines = section_prefix(section)
    entries = parse_proto_entries(section.body)
    entity = proto_section_entity(section.header.name)

    if entity is None:
        for entry in entries:
            lines.extend(entry.emit_lines())
        return lines

    entity_ordering = ordering.get(entity)
    known_roots = detect_component_roots(entries, entity_ordering)
    root_indices: dict[str, int] = {}

    for entry in entries:
        root = resolve_component_root(entity_ordering, entry.key, known_roots) if entry.key is not None else ''
        if root:
            root_indices.setdefault(root, entry.index)

    header_entries: list[ProtoEntry] = []
    plain_entries: list[ProtoEntry] = []
    component_entries: list[ProtoEntry] = []
    text_entries: list[ProtoEntry] = []
    tail_entries: list[ProtoEntry] = []

    for entry in entries:
        # Comments after the last property stay last, otherwise the next run would attach them to a property
        if not entry.lines:
            tail_entries.append(entry)
        elif entry.key is None:
            plain_entries.append(entry)
        elif entry.key in ('$Name', '$Parent'):
            header_entries.append(entry)
        elif entry.key.startswith('$Text'):
            text_entries.append(entry)
        elif resolve_component_root(entity_ordering, entry.key, known_roots):
            component_entries.append(entry)
        else:
            plain_entries.append(entry)

    # The baker names a section without $Name after its file, so the formatter states that identity explicitly
    if default_name is not None and not any(entry.key == '$Name' for entry in header_entries):
        header_entries.append(ProtoEntry([f'$Name = {default_name}'], [], '$Name', -1))

    header_entries.sort(key=header_sort_key)
    plain_entries.sort(key=lambda entry: plain_sort_key(entity_ordering, entry))
    component_entries.sort(key=lambda entry: component_sort_key(entity_ordering, entry, known_roots, root_indices))

    emitted_group = False

    for group in (header_entries, plain_entries, component_entries, text_entries):
        if not group:
            continue

        if emitted_group:
            lines.append('')

        if group is component_entries:
            emit_component_entries(lines, group, entity_ordering, known_roots)
        else:
            for entry in group:
                lines.extend(entry.emit_lines())

        emitted_group = True

    for entry in tail_entries:
        lines.extend(entry.emit_lines())

    return lines


def format_proto_text(text: str, ordering: dict[str, EntityOrdering], default_name: str | None) -> str:
    preamble, sections = split_sections(read_lines(text))
    output = [physical.rstrip(SPACE) for line in preamble if line.kind != BLANK for physical in line.physical]

    for section in sections:
        if output:
            output.append('')

        output.extend(format_proto_section(section, ordering, default_name))

    return '\n'.join(output).rstrip('\n') + '\n'


# Metadata order


def iter_metadata_tags(text: str) -> Iterable[list[str]]:
    # Mirrors MetadataBaker: a tag runs from `///@` to the line end, a backslash continues it, `//` starts a comment
    position = 0

    while (position := text.find('///@', position)) != -1:
        end = text.find('\n', position)

        while end != -1 and (text[end - 1] == '\\' or (text[end - 1] == '\r' and text[end - 2] == '\\')):
            end = text.find('\n', end + 1)

        line = text[position + 4 : end if end != -1 else len(text)]
        position = end if end != -1 else len(text)
        line = re.sub(r'\\[\r\n]+', ' ', line)
        comment = line.find('//')

        if comment != -1:
            line = line[:comment]

        tokens = line.split()

        if tokens:
            yield tokens


def property_name_index(tokens: list[str]) -> int:
    # A complex type such as `dict<int32, string>` spans several tokens; the name follows its closing bracket
    index = 2
    depth = 0

    while index < len(tokens):
        depth += tokens[index].count('<') - tokens[index].count('>')
        index += 1

        if depth <= 0:
            break

    return index


def setting_tag_name(tokens: list[str]) -> str:
    name = ''

    for token in tokens[2:]:
        if token == '.':
            if name and not name.endswith('.'):
                name += '.'
            continue

        if name and not name.endswith('.'):
            name += '.'

        name += token

    return name


def read_metadata_order(metadata_files: Sequence[Path]) -> tuple[dict[str, EntityOrdering], list[str]]:
    ordering: dict[str, EntityOrdering] = {}
    settings: list[str] = []
    rank = 0

    for path in metadata_files:
        for tokens in iter_metadata_tags(path.read_bytes().decode('utf-8-sig')):
            tag, arguments = tokens[0], tokens[1:]

            if tag == 'Setting' and len(arguments) >= 3:
                settings.append(setting_tag_name(arguments))
                continue

            if tag != 'Property' or len(arguments) < 4:
                continue

            name_index = property_name_index(arguments)

            if name_index >= len(arguments):
                continue

            entity_ordering = ordering.setdefault(arguments[0], EntityOrdering())
            name = arguments[name_index]

            if name in entity_ordering.properties:
                continue

            is_marker = 'Component' in arguments[name_index + 1 :] and '.' not in name
            root = name.split('.', 1)[0] if '.' in name or is_marker else ''
            entity_ordering.properties[name] = PropertyOrder(rank, root)

            if root:
                entity_ordering.components.setdefault(root, rank)

            rank += 1

    return ordering, settings


def read_engine_setting_order() -> list[str]:
    names: list[str] = []

    for line in SETTINGS_INCLUDE.read_text(encoding='utf-8-sig').splitlines():
        match = ENGINE_SETTING_RE.match(line.strip())
        if match is not None:
            names.append(f'{match.group(1)}.{match.group(2)}')

    return names


def read_engine_setting_default(group: str, name: str) -> str:
    pattern = re.compile(ENGINE_SETTING_DEFAULT_RE.pattern.format(group=re.escape(group), name=re.escape(name)))

    for line in SETTINGS_INCLUDE.read_text(encoding='utf-8-sig').splitlines():
        match = pattern.match(line.strip())
        if match is not None:
            return ' '.join(re.findall(r'"([^"]*)"', match.group(1)))

    raise ContentFormatError(f'Engine setting {group}.{name} has no default in {SETTINGS_INCLUDE}')


def build_setting_order(project_settings: Iterable[str]) -> dict[str, int]:
    order: dict[str, int] = {}

    for name in [*read_engine_setting_order(), *project_settings]:
        order.setdefault(name, len(order))

    return order


# Resource packs


def match_resource_glob(path: str, pattern: str) -> bool:
    # Mirrors FileSystem MatchResourcePathGlob: `*` and `?` stop at `/`, two or more stars cross directories
    memo: dict[tuple[int, int], bool] = {}

    def match(pattern_pos: int, path_pos: int) -> bool:
        cached = memo.get((pattern_pos, path_pos))
        if cached is not None:
            return cached

        if pattern_pos == len(pattern):
            result = path_pos == len(path)
        elif pattern[pattern_pos] == '*':
            next_pattern_pos = pattern_pos

            while next_pattern_pos < len(pattern) and pattern[next_pattern_pos] == '*':
                next_pattern_pos += 1

            if next_pattern_pos - pattern_pos >= 2:
                if next_pattern_pos < len(pattern) and pattern[next_pattern_pos] == '/':
                    result = match(next_pattern_pos + 1, path_pos)
                    next_path_pos = path_pos

                    while not result and next_path_pos < len(path):
                        if path[next_path_pos] == '/':
                            result = match(pattern_pos, next_path_pos + 1)
                        next_path_pos += 1
                else:
                    result = match(next_pattern_pos, path_pos) or (path_pos < len(path) and match(pattern_pos, path_pos + 1))
            else:
                result = match(next_pattern_pos, path_pos) or (path_pos < len(path) and path[path_pos] != '/' and match(pattern_pos, path_pos + 1))
        elif pattern[pattern_pos] == '?':
            result = path_pos < len(path) and path[path_pos] != '/' and match(pattern_pos + 1, path_pos + 1)
        else:
            result = path_pos < len(path) and pattern[pattern_pos] == path[path_pos] and match(pattern_pos + 1, path_pos + 1)

        memo[(pattern_pos, path_pos)] = result
        return result

    return match(0, 0)


def file_extension(path: str) -> str:
    dot = path.rfind('.')
    return path[dot + 1 :].lower() if dot != -1 else ''


def resource_pack_files(fomain: Path, config: foconfig.ConfigParser, baker: str, extensions: Sequence[str]) -> list[list[tuple[str, Path]]]:
    # Each pack yields (path inside the pack, file) pairs; a path already provided by an earlier input dir wins
    config_dir = fomain.parent
    ignored = {(config_dir / entry).resolve() for entry in config.mainSection().getStr('Baking.IgnoreInputDirs', '').split()}
    packs: list[list[tuple[str, Path]]] = []

    for pack in config.getSections('ResourcePack'):
        if baker not in pack.getStr('Bakers', '').split():
            continue

        include = [pattern.replace('\\', '/') for pattern in pack.getStr('IncludePatterns', '').split()]
        exclude = [pattern.replace('\\', '/') for pattern in pack.getStr('ExcludePatterns', '').split()]
        files: dict[str, Path] = {}

        for input_dir in pack.getStr('InputDirs', '').split():
            root = (config_dir / input_dir).resolve()

            if root in ignored or not root.is_dir():
                continue

            for path in sorted(root.rglob('*')):
                relative = path.relative_to(root).as_posix()

                if relative in files or not path.is_file() or file_extension(relative) not in extensions:
                    continue

                if (not include or any(match_resource_glob(relative, pattern) for pattern in include)) and not any(match_resource_glob(relative, pattern) for pattern in exclude):
                    files[relative] = path

        packs.append(sorted(files.items()))

    return packs


def discover_metadata_files(fomain: Path, config: foconfig.ConfigParser) -> list[Path]:
    return [path for pack in resource_pack_files(fomain, config, 'Metadata', METADATA_EXTENSIONS) for _, path in pack]


def proto_extensions(config: foconfig.ConfigParser) -> list[str]:
    value = config.mainSection().getStr('Baking.ProtoFileExtensions', '') or read_engine_setting_default('Baking', 'ProtoFileExtensions')
    return [extension.lower() for extension in value.split()]


def discover_proto_files(fomain: Path, config: foconfig.ConfigParser, explicit_paths: Sequence[Path]) -> list[Path]:
    extensions = proto_extensions(config)
    found: set[Path] = set()

    if not explicit_paths:
        found.update(path for pack in resource_pack_files(fomain, config, 'Proto', extensions) for _, path in pack)
        return sorted(found)

    for path in explicit_paths:
        if path.is_dir():
            found.update(item for item in path.rglob('*') if item.is_file() and file_extension(item.name) in extensions)
        elif path.is_file() and file_extension(path.name) in extensions:
            found.add(path)
        else:
            raise ContentFormatError(f'Prototype path is neither a directory nor a file with a Baking.ProtoFileExtensions extension: {path}')

    return sorted(found)


# File handling


def decode_file(path: Path) -> str:
    try:
        return path.read_bytes().decode('utf-8')
    except UnicodeDecodeError as ex:
        raise ContentFormatError(f'File is not valid UTF-8: {path}') from ex


def finish_text(original: str, formatted_body: str) -> str:
    # The BOM and the LF/CRLF convention are the file's own; the formatter only guarantees one final line ending
    bom = UTF8_BOM if original.startswith(UTF8_BOM) else ''
    line_ending = '\r\n' if '\r\n' in original else '\n'
    body = formatted_body.replace('\r\n', '\n').rstrip('\n')
    return bom + (body + '\n').replace('\n', line_ending)


def config_meaning(text: str, proto_name: str | None) -> list[tuple[str, dict[str, str]]]:
    parser = foconfig.ConfigParser()
    parser.loadFromText(text)
    meaning = [(section.name, dict(section.content)) for section in parser.sections]

    # ProtoBaker names a section without $Name after its file
    if proto_name is not None:
        for name, content in meaning[1:]:
            if '/' not in name:
                content.setdefault('$Name', proto_name)

    return meaning


def verify_meaning(path: Path, original: str, formatted: str, proto_name: str | None) -> None:
    if config_meaning(original, proto_name) == config_meaning(formatted, proto_name):
        return

    hint = ' The file starts with a UTF-8 BOM, which the Engine reader keeps as part of the first line; remove it.' if original.startswith(UTF8_BOM) else ''
    raise ContentFormatError(f'Formatting would change how the Engine reads {path}; the file was left unchanged.{hint}')


def format_config_file(path: Path, transform, proto_name: str | None) -> tuple[str, str]:
    original = decode_file(path)
    body = original[1:] if original.startswith(UTF8_BOM) else original
    formatted = finish_text(original, transform(body))
    verify_meaning(path, original, formatted, proto_name)
    return original, formatted


def apply_result(path: Path, original: str, formatted: str, check: bool) -> FormatResult:
    if formatted == original:
        return FormatResult(path, False)

    if not check:
        path.write_bytes(formatted.encode('utf-8'))

    return FormatResult(path, True)


def format_fomain_file(path: Path, setting_order: dict[str, int], check: bool) -> FormatResult:
    original, formatted = format_config_file(path, lambda text: format_fomain_text(text, setting_order), None)
    return apply_result(path, original, formatted, check)


def format_proto_file(path: Path, ordering: dict[str, EntityOrdering], check: bool) -> FormatResult:
    name = path.name.rsplit('.', 1)[0]
    original, formatted = format_config_file(path, lambda text: format_proto_text(text, ordering, name), name)
    return apply_result(path, original, formatted, check)


def format_fos_files(project_root: Path, patterns: Sequence[str], check: bool) -> list[FormatResult]:
    import buildtools

    files = sorted({path for pattern in patterns for path in project_root.glob(pattern) if path.is_file() and path.suffix == '.fos'})

    if not files:
        return []

    try:
        clang_format = buildtools.discover_clang_format()
    except SystemExit as ex:
        raise ContentFormatError(str(ex.code)) from ex

    results: list[FormatResult] = []

    for path in files:
        original = decode_file(path)
        body = original[1:] if original.startswith(UTF8_BOM) else original
        formatted = finish_text(original, buildtools.format_source_text(clang_format, path, body))
        results.append(apply_result(path, original, formatted, check))

    return results


# Orchestration


def find_fomain(project_root: Path, explicit: str | None) -> Path:
    if explicit is not None:
        path = resolve_path(project_root, explicit)

        if not path.is_file():
            raise ContentFormatError(f'Main config not found: {path}')

        return path

    candidates = sorted(project_root.glob('*.fomain'))

    if len(candidates) != 1:
        raise ContentFormatError(f'Expected exactly one .fomain file in {project_root}, found {len(candidates)}; pass --fomain')

    return candidates[0]


def resolve_path(project_root: Path, value: str) -> Path:
    path = Path(value)
    return (path if path.is_absolute() else project_root / path).resolve()


def display_path(project_root: Path, path: Path) -> str:
    return path.relative_to(project_root).as_posix() if path.is_relative_to(project_root) else path.as_posix()


def format_content(args: argparse.Namespace) -> list[FormatResult]:
    project_root = resolve_path(Path.cwd(), args.project_root or os.environ.get('FO_PROJECT_ROOT') or '.')
    results: list[FormatResult] = []

    if not args.skip_fomain or not args.skip_protos:
        fomain = find_fomain(project_root, args.fomain)
        config = foconfig.ConfigParser()
        config.loadFromFile(fomain)
        metadata_files = [resolve_path(project_root, path) for path in args.metadata] if args.metadata else discover_metadata_files(fomain, config)
        ordering, project_settings = read_metadata_order(metadata_files)

        if not args.skip_fomain:
            results.append(format_fomain_file(fomain, build_setting_order(project_settings), args.check))

        if not args.skip_protos:
            proto_paths = [resolve_path(project_root, path) for path in args.proto_path]
            results.extend(format_proto_file(path, ordering, args.check) for path in discover_proto_files(fomain, config, proto_paths))

    results.extend(format_fos_files(project_root, args.fos, args.check))

    for result in results:
        if result.changed:
            print(f'{LOG_PREFIX} {"Needs formatting" if args.check else "Formatted"}: {display_path(project_root, result.path)}', flush=True)

    return results


def run(args: argparse.Namespace) -> int:
    try:
        results = format_content(args)
    except ContentFormatError as ex:
        print(f'{LOG_PREFIX} Error: {ex}', file=sys.stderr, flush=True)
        return EXIT_ERROR

    changed = sum(1 for result in results if result.changed)

    if args.check and changed:
        print(f'{LOG_PREFIX} {changed} of {len(results)} file(s) need formatting; run format-content without --check and commit the result', flush=True)
        return EXIT_DRIFT

    print(f'{LOG_PREFIX} Completed, checked {len(results)} file(s), {"found" if args.check else "changed"} {changed}', flush=True)
    return EXIT_OK


def add_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument('--check', action='store_true', help='report files that need formatting and exit with 1 without writing')
    parser.add_argument('--project-root', help='project root for relative paths and the .fomain lookup (default: FO_PROJECT_ROOT, then the current directory)')
    parser.add_argument('--fomain', help='main config path (default: the only *.fomain file in the project root)')
    parser.add_argument('--skip-fomain', action='store_true', help='do not format the main config')
    parser.add_argument('--skip-protos', action='store_true', help='do not format prototype files')
    parser.add_argument('--proto-path', action='append', default=[], metavar='PATH', help='format only these prototype files or directories instead of the Proto resource packs (repeatable)')
    parser.add_argument('--metadata', action='append', default=[], metavar='PATH', help='read property and setting order from these files, in this order, instead of the Metadata resource packs (repeatable)')
    parser.add_argument('--fos', action='append', default=[], metavar='PATTERN', help='also format AngelScript files matching this glob relative to the project root with clang-format 20 (repeatable)')


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog='format_content.py', description='Format .fomain, prototype and optional AngelScript files without changing what the Engine reads from them.')
    add_arguments(parser)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    return run(create_parser().parse_args(argv))


if __name__ == '__main__':
    raise SystemExit(main())
