#!/usr/bin/python3

"""Read FOnline configuration text (`.fomain`, prototype files, baked configs) the way the Engine does.

The reader mirrors `Source/Common/ConfigFile.cpp`: lines split at LF with one trailing CR removed, ASCII
configuration whitespace trimmed, a final backslash preceded by a space or tab continues the logical line,
a `[` line with a non-empty name before its first `]` opens a section, `#` outside double quotes and not
escaped starts a comment, the first `=` separates key and value, `+=` appends with one space and the last
assignment of a key wins. `loadFromFile` additionally removes a leading UTF-8 BOM, which the native reader
does not do; `loadFromText` keeps the text exactly as given.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator

import fo_formats


ConfigValue = str | int | bool | None

CONFIG_SPACE = ' \t\r\n\f\v'


@dataclass(slots=True)
class ConfigSection:
    name: str
    content: dict[str, str]

    def getStr(self, key: str, default: ConfigValue = None) -> str:
        self.checkKey(key, default)
        return self.content.get(key, str(default))

    def getBool(self, key: str, default: ConfigValue = None) -> bool:
        self.checkKey(key, default)
        value = self.content.get(key, str(default)).lower()
        if value == 'true':
            return True
        if value == 'false':
            return False
        return int(value) != 0

    def getInt(self, key: str, default: ConfigValue = None) -> int:
        self.checkKey(key, default)
        return int(self.content.get(key, str(default)))

    def checkKey(self, key: str, default: ConfigValue) -> None:
        if default is None and key not in self.content:
            raise KeyError(f"Key '{key}' not found in section '{self.name}'")

    def get_str(self, key: str, default: ConfigValue = None) -> str:
        return self.getStr(key, default)

    def get_bool(self, key: str, default: ConfigValue = None) -> bool:
        return self.getBool(key, default)

    def get_int(self, key: str, default: ConfigValue = None) -> int:
        return self.getInt(key, default)


@dataclass(slots=True)
class LogicalLine:
    # Physical lines without their line terminators, followed by the merged and trimmed text the reader parses
    physical: list[str]
    text: str


@dataclass(slots=True)
class KeyValue:
    key: str
    value: str
    append: bool
    separator: int


def split_physical_lines(text: str) -> list[str]:
    lines = text.split('\n')

    if text.endswith('\n'):
        lines.pop()

    return [line[:-1] if line.endswith('\r') else line for line in lines]


def iter_logical_lines(physical_lines: Iterable[str]) -> Iterator[LogicalLine]:
    group: list[str] = []
    accum = ''

    for physical in physical_lines:
        group.append(physical)
        line = physical.strip(CONFIG_SPACE)

        if accum:
            line = accum + line
            accum = ''

        if len(line) >= 2 and line[-1] == '\\' and line[-2] in ' \t':
            accum = line[:-1].strip(CONFIG_SPACE) + ' '
            continue

        yield LogicalLine(group, line)
        group = []

    # A continuation on the last line is still read, with or without a final line terminator
    if accum:
        yield LogicalLine(group, accum)


def parse_section_header(text: str) -> str | None:
    if not text.startswith('['):
        return None

    end = text.find(']')

    if end == -1:
        return None

    name = text[1:end].strip(CONFIG_SPACE)
    return name or None


def parse_key_value(text: str) -> KeyValue | None:
    inside_quotes = False
    backslash_run = 0
    separator = -1
    content_end = len(text)

    for index, char in enumerate(text):
        escaped = backslash_run % 2 == 1

        if char == '"' and not escaped:
            inside_quotes = not inside_quotes
        elif char == '#' and not inside_quotes and not escaped:
            content_end = index
            break
        elif char == '=' and separator == -1:
            separator = index

        backslash_run = backslash_run + 1 if char == '\\' else 0

    content = text[:content_end]
    content_begin = len(content) - len(content.lstrip(CONFIG_SPACE))
    content_end = len(content.rstrip(CONFIG_SPACE))

    if content_begin == content_end or separator == -1 or separator <= content_begin or separator >= content_end:
        return None

    append = text[separator - 1] == '+'
    key = text[content_begin : separator - 1 if append else separator].strip(CONFIG_SPACE)
    value = text[separator + 1 : content_end].strip(CONFIG_SPACE)

    if not key:
        return None

    return KeyValue(key, value, append, separator)


class ConfigParser:
    def __init__(self) -> None:
        self.sections: list[ConfigSection] = []

    def loadFromFile(self, path: str | Path) -> None:
        self.loadFromText(Path(path).read_bytes().decode('utf-8-sig'))

    def loadFromText(self, text: str) -> None:
        self._load(split_physical_lines(text))

    def loadFromLines(self, lines: Iterable[str]) -> None:
        physical_lines: list[str] = []

        for line in lines:
            line = line[:-1] if line.endswith('\n') else line
            physical_lines.append(line[:-1] if line.endswith('\r') else line)

        self._load(physical_lines)

    def _load(self, physical_lines: Iterable[str]) -> None:
        self.sections = []
        section_name = ''
        section_content: dict[str, str] = {}

        for logical in iter_logical_lines(physical_lines):
            if not logical.text:
                continue

            # A malformed header is ignored as a whole, it is never read as a key
            if logical.text.startswith('['):
                name = parse_section_header(logical.text)
                if name is not None:
                    self.sections.append(ConfigSection(section_name, section_content))
                    section_name = name
                    section_content = {}
                continue

            entry = parse_key_value(logical.text)
            if entry is None:
                continue

            if entry.append and entry.key in section_content:
                if entry.value:
                    section_content[entry.key] = f'{section_content[entry.key]} {entry.value}'
            else:
                section_content[entry.key] = entry.value

        self.sections.append(ConfigSection(section_name, section_content))

    def mainSection(self) -> ConfigSection:
        return self.sections[0]

    def getSections(self, sectionName: str) -> list[ConfigSection]:
        return [section for section in self.sections if section.name == sectionName]

    def load_from_file(self, path: str | Path) -> None:
        self.loadFromFile(path)

    def load_from_text(self, text: str) -> None:
        self.loadFromText(text)

    def load_from_lines(self, lines: Iterable[str]) -> None:
        self.loadFromLines(lines)

    def main_section(self) -> ConfigSection:
        return self.mainSection()

    def get_sections(self, section_name: str) -> list[ConfigSection]:
        return self.getSections(section_name)
