#!/usr/bin/python3

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


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
        self.sections = []
        section_name = ''
        section_content: dict[str, str] = {}

        for raw_line in lines:
            line = raw_line.split('#', 1)[0].strip()
            if not line:
                continue

            if line.startswith('[') and line.endswith(']'):
                self.sections.append(ConfigSection(section_name, section_content))
                section_name = line[1:-1].strip()
                section_content = {}
                continue

            if '=' not in line:
                continue

            key, value = line.split('=', 1)
            key = key.strip()
            if key:
                section_content[key] = value.strip()

        self.sections.append(ConfigSection(section_name, section_content))

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
