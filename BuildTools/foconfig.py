#!/usr/bin/python3

"""Configuration reader API of the packaging and formatting scripts over the `fo_formats` grammar.

`fo_formats` owns the port of `Source/Common/ConfigFile.cpp`; this module only adapts it to the interface its
callers use: `ConfigParser` with camelCase and snake_case methods, `ConfigSection` with typed getters, and the
line-level helpers `split_physical_lines`, `iter_logical_lines`, `parse_section_header` and `parse_key_value`
that a tool rewriting config files needs. `loadFromFile` additionally removes a leading UTF-8 BOM, which the
native reader does not do; `loadFromText` and `loadFromLines` keep the text exactly as given.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import fo_formats


ConfigValue = str | int | bool | None

# Keys and values are trimmed with CONFIG_SPACE, whole lines and section names with TRIM_SPACE
CONFIG_SPACE = fo_formats.CONFIG_SPACE
TRIM_SPACE = fo_formats.TRIM_SPACE

LogicalLine = fo_formats.ConfigLine
KeyValue = fo_formats.ConfigKeyValue

split_physical_lines = fo_formats.split_config_lines
iter_logical_lines = fo_formats.iter_config_lines
parse_section_header = fo_formats.parse_config_section_header
parse_key_value = fo_formats.parse_config_key_value


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


class ConfigParser:
    def __init__(self) -> None:
        self.sections: list[ConfigSection] = []

    def loadFromFile(self, path: str | Path) -> None:
        self.loadFromText(Path(path).read_bytes().decode('utf-8-sig'))

    def loadFromText(self, text: str) -> None:
        self._load(fo_formats.parse_config_text(text))

    def loadFromLines(self, lines: Iterable[str]) -> None:
        physical_lines: list[str] = []

        for line in lines:
            line = line[:-1] if line.endswith('\n') else line
            physical_lines.append(line[:-1] if line.endswith('\r') else line)

        self._load(fo_formats.parse_config_lines(physical_lines))

    def _load(self, document: fo_formats.ConfigDocument) -> None:
        self.sections = [ConfigSection(section.name, section.values) for section in document.sections]

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
