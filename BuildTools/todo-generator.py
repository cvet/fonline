#!/usr/bin/python3

from __future__ import annotations

import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, TypeVar


ROOT_PATH = Path('../')
TODO_PATHS = [Path('README.md')]
SOURCE_PATHS = [Path('Source'), Path('CMakeLists.txt')]
SOURCE_EXTENSIONS = {'cpp', 'h'}
GENERATE_SECTION = False
TODO_MARKERS = ['// Todo:', '# Todo:']
HEAD_LINE = '### Todo list *(generated from source code)*'
TAIL_LINE = '  '
PRIORITY_FILES = ['Common.h', 'Common.cpp']
CAPITALIZE_FIRST_LETTER = False
UNCAPITALIZE_FIRST_LETTER = False
PRINT_RESULT = True
DONT_WRITE_FILE = False


@dataclass
class TodoEntry:
    text: str
    file_path: Path
    count: int = 1


ListItem = TypeVar('ListItem')


def decide_new_line(file_lines: list[str]) -> str:
    crCount = 0
    lfCount = 0
    crlfCount = 0
    for line in file_lines:
        if len(line) >= 2 and line[-1:] == '\n' and line[-2:-1] == '\r':
            crlfCount += 1
        elif line[-1:] == '\n':
            lfCount += 1
        elif line[-1:] == '\r':
            crCount += 1

    if crlfCount == 0 and lfCount == 0 and crCount == 0:
        return os.linesep
    if crlfCount >= crCount and crlfCount >= lfCount:
        return '\r\n'
    if lfCount >= crlfCount and lfCount >= crCount:
        return '\n'
    if crCount >= crlfCount and crCount >= lfCount:
        return '\r'

    raise AssertionError('Unable to determine newline style')


def read_file_lines(file_path: Path) -> list[str]:
    with file_path.open('rb') as file:
        return [line.decode('utf-8') for line in file.readlines()]


def find_in_list(items: list[ListItem], predicate: Callable[[ListItem], bool]) -> int:
    index = 0
    for item in items:
        if predicate(item):
            return index
        index += 1
    return -1


def process_file(file_path: Path, todo_info: list[TodoEntry]) -> None:
    file_lines = read_file_lines(file_path)
    lineIndex = 0
    for line in file_lines:
        lineIndex += 1
        for todoMarker in TODO_MARKERS:
            todoIndex = line.find(todoMarker)
            if todoIndex != -1:
                todoEntry = line[todoIndex + len(todoMarker):].strip('\r\n \t')
                if len(todoEntry) == 0:
                    todoEntry = '(Unnamed todo)'

                entryIndex = find_in_list(todo_info, lambda item: item.text == todoEntry and item.file_path == file_path)
                if entryIndex == -1:
                    todo_info.append(TodoEntry(todoEntry, file_path))
                else:
                    todo_info[entryIndex].count += 1


def get_files_at_path(path: Path) -> list[Path]:
    def fill_recursive(current_path: Path, path_list: list[Path]) -> None:
        if current_path.is_file():
            path_list.append(current_path)
        elif current_path.is_dir():
            for sub_path in current_path.iterdir():
                fill_recursive(sub_path, path_list)

    def is_need_parse(file_path: Path) -> bool:
        return file_path.suffix.lower()[1:] in SOURCE_EXTENSIONS

    file_path_list: list[Path] = []
    fill_recursive(path, file_path_list)
    file_path_list = sorted(filter(is_need_parse, file_path_list))

    priority_path_list: list[Path] = []
    for priority_file in PRIORITY_FILES:
        for file_path in file_path_list[:]:
            if priority_file in str(file_path):
                priority_path_list.append(file_path)
                file_path_list.remove(file_path)
                break

    return priority_path_list + file_path_list


def process_path(path: Path, todo_info: list[TodoEntry]) -> None:
    for file_path in get_files_at_path(path):
        process_file(file_path, todo_info)


def generate_todo_desc(todo_info: list[TodoEntry]) -> list[str]:
    desc_list: list[str] = []
    for todo_entry in todo_info:
        entry_name = todo_entry.text
        if CAPITALIZE_FIRST_LETTER:
            entry_name = entry_name[:1].upper() + entry_name[1:]
        if UNCAPITALIZE_FIRST_LETTER:
            entry_name = entry_name[:1].lower() + entry_name[1:]

        file_name = todo_entry.file_path.stem
        desc = '* ' + file_name + ': ' + entry_name

        if todo_entry.count > 1:
            desc += ' (' + str(todo_entry.count) + ')'

        desc_list.append(desc)

    if not desc_list:
        desc_list.append('Nothing todo :)')
    return desc_list


def inject_desc(path: Path, desc_list: list[str]) -> None:
    todoList = []
    todoList.append(HEAD_LINE)
    todoList.append('')
    todoList.extend(desc_list)
    todoList.append(TAIL_LINE)

    if PRINT_RESULT:
        for todo in todoList:
            print(todo)

    if not DONT_WRITE_FILE:
        fileLines = read_file_lines(path)

        todoBegin = find_in_list(fileLines, lambda line: line.strip('\r\n') == HEAD_LINE)
        todoEnd = find_in_list(fileLines, lambda line: line.strip('\r\n') == TAIL_LINE)

        if todoBegin == -1 and not GENERATE_SECTION:
            print('Begin section not found')
            sys.exit(1)

        if todoBegin != -1 and todoEnd != -1 and todoEnd > todoBegin:
            fileLines = fileLines[:todoBegin] + fileLines[todoEnd + 1:]

        nl = decide_new_line(fileLines)

        finalOutput = fileLines[:todoBegin]
        finalOutput.extend([todo + nl for todo in todoList])
        finalOutput.extend(fileLines[todoBegin:])

        with path.open('wb') as file:
            file.writelines([line.encode('utf-8') for line in finalOutput])


def main() -> None:
    os.chdir(ROOT_PATH)

    todo_info: list[TodoEntry] = []
    for src_path in SOURCE_PATHS:
        process_path(src_path, todo_info)

    desc_list = generate_todo_desc(todo_info)

    for todo_path in TODO_PATHS:
        inject_desc(todo_path, desc_list)


if __name__ == '__main__':
    main()
