#!/usr/bin/python3

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import fo_formats


ConfigValue = str | int | bool | None


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
        with Path(path).open('r', encoding='utf-8-sig') as file:
            self.loadFromLines(file.readlines())

    def loadFromLines(self, lines: Iterable[str]) -> None:
        # The Engine's ConfigFile grammar, so packaging reads a config exactly as the runtime and the baker do
        text = '\n'.join(line[:-1] if line.endswith('\n') else line for line in lines)
        document = fo_formats.parse_config_text(text)
        self.sections = [ConfigSection(section.name, dict(section.values)) for section in document.sections]

    def mainSection(self) -> ConfigSection:
        return self.sections[0]

    def getSections(self, sectionName: str) -> list[ConfigSection]:
        return [section for section in self.sections if section.name == sectionName]

    def load_from_file(self, path: str | Path) -> None:
        self.loadFromFile(path)

    def load_from_lines(self, lines: Iterable[str]) -> None:
        self.loadFromLines(lines)

    def main_section(self) -> ConfigSection:
        return self.mainSection()

    def get_sections(self, section_name: str) -> list[ConfigSection]:
        return self.getSections(section_name)
