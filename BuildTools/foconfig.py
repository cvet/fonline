#!/usr/bin/python3

class ConfigSection:
    def __init__(self, name, content):
        self.name = name
        self.content = content

    def getStr(self, key, default=None):
        self.checkKey(key, default)
        return self.content.get(key, str(default))

    def getBool(self, key, default=None):
        self.checkKey(key, default)
        value = self.content.get(key, str(default))
        if value.lower() == 'true':
            return True
        elif value.lower() == 'false':
            return False
        return int(value) != 0

    def getInt(self, key, default=None):
        self.checkKey(key, default)
        return int(self.content.get(key, str(default)))

    def checkKey(self, key, default):
        if default is None and key not in self.content:
            raise KeyError(f"Key '{key}' not found in section '{self.name}'")


class ConfigParser:
    def __init__(self):
        self.sections = []

    def loadFromFile(self, path):
        with open(path, 'r', encoding='utf-8-sig') as f:
            lines = f.readlines()
        self.loadFromLines(lines)

    def loadFromLines(self, lines):
        sectionName = ''
        sectionContent = {}

        for line in lines:
            # Remove comments
            commentIndex = line.find('#')
            if commentIndex != -1:
                line = line[:commentIndex]

            line = line.strip()

            # Handle section headers
            if line.startswith('[') and line.endswith(']'):
                self.sections.append(ConfigSection(sectionName, sectionContent))
                sectionName = line[1:-1].strip()
                sectionContent = {}

            # Handle key-value pairs
            elif '=' in line:
                key, value = line.split('=', 1)
                key = key.strip()
                value = value.strip()
                if key:
                    sectionContent[key] = value

        # Add the last section
        self.sections.append(ConfigSection(sectionName, sectionContent))

    def mainSection(self):
        return self.sections[0]

    def getSections(self, sectionName):
        return [section for section in self.sections if section.name == sectionName]
