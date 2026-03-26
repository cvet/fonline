#!/usr/bin/python3

from __future__ import annotations

import argparse
import hashlib
import os
import subprocess
import sys
import time
import uuid
from collections.abc import Callable, Iterable, Mapping
from dataclasses import astuple, dataclass, is_dataclass
from typing import Any, TypeAlias, TypedDict, cast


GeneratedFileMap = dict[str, list[str]]
CommentLines: TypeAlias = list[str]
TagContext: TypeAlias = bool | int | str | list[str] | None

EXPORT_TARGETS = ('Server', 'Client', 'Mapper', 'Common')
REGISTRATION_TARGETS = ('Server', 'Client', 'Mapper')
CLIENT_ENTITY_TARGETS = ('Client', 'Mapper')


@dataclass(slots=True)
class TagMetaRecord:
    abs_path: str
    line_index: int
    tag_info: str | None
    tag_context: TagContext
    comment: CommentLines


@dataclass(slots=True)
class MethodArg:
    arg_type: str
    name: str


@dataclass(slots=True)
class RefTypeField:
    field_type: str
    name: str
    comment: CommentLines


@dataclass(slots=True)
class RefTypeMethod:
    name: str
    ret: str
    args: list[MethodArg]
    comment: CommentLines


@dataclass(slots=True)
class SettingsEntry:
    kind: str
    value_type: str
    name: str
    init_values: list[str]
    comment: CommentLines


@dataclass(slots=True)
class EnumKeyValue:
    key: str
    value: str | None
    comment: CommentLines


@dataclass(slots=True)
class EntityInfo:
    server: str
    client: str
    is_global: bool
    has_protos: bool
    has_statics: bool
    has_abstract: bool
    has_time_events: bool
    exported: bool
    comment: CommentLines


@dataclass(slots=True)
class MethodRegistrationInfo:
    function_name: str
    engine_entity_type_extern: str
    return_type: str


@dataclass(slots=True)
class ExportEnumTag:
    group_name: str
    underlying_type: str
    key_values: list[EnumKeyValue]
    flags: list[str]
    comment: CommentLines


@dataclass(slots=True)
class ExportValueTypeTag:
    name: str
    native_type: str
    flags: list[str]
    comment: CommentLines


@dataclass(slots=True)
class ExportPropertyTag:
    entity: str
    access: str
    property_type: str
    name: str
    flags: list[str]
    comment: CommentLines


@dataclass(slots=True)
class ExportMethodTag:
    target: str
    entity: str
    name: str
    ret: str
    args: list[MethodArg]
    flags: list[str]
    comment: CommentLines


@dataclass(slots=True)
class ExportEventTag:
    target: str
    entity: str
    name: str
    args: list[MethodArg]
    flags: list[str]
    comment: CommentLines


@dataclass(slots=True)
class ExportRefTypeTag:
    target: str
    name: str
    fields: list[RefTypeField]
    methods: list[RefTypeMethod]
    flags: list[str]
    comment: CommentLines


@dataclass(slots=True)
class ExportEntityTag:
    name: str
    server_class_name: str
    client_class_name: str
    flags: list[str]
    comment: CommentLines


@dataclass(slots=True)
class ExportSettingsTag:
    group_name: str
    target: str
    settings: list[SettingsEntry]
    flags: list[str]
    comment: CommentLines


@dataclass(slots=True)
class EngineHookTag:
    name: str
    flags: list[str]
    comment: CommentLines


@dataclass(slots=True)
class MigrationRuleTag:
    args: list[str]
    comment: CommentLines


@dataclass(slots=True)
class CodeGenTag:
    template_type: str
    abs_path: str
    entry: str
    line_index: int
    tag_context: TagContext
    flags: list[str]
    comment: CommentLines


class CodeGenTagStore(TypedDict):
    ExportEnum: list[ExportEnumTag]
    ExportValueType: list[ExportValueTypeTag]
    ExportProperty: list[ExportPropertyTag]
    ExportMethod: list[ExportMethodTag]
    ExportEvent: list[ExportEventTag]
    ExportRefType: list[ExportRefTypeTag]
    ExportEntity: list[ExportEntityTag]
    ExportSettings: list[ExportSettingsTag]
    EngineHook: list[EngineHookTag]
    MigrationRule: list[MigrationRuleTag]
    CodeGen: list[CodeGenTag]


class TagMetaStore(TypedDict):
    ExportEnum: list[TagMetaRecord]
    ExportValueType: list[TagMetaRecord]
    ExportProperty: list[TagMetaRecord]
    ExportMethod: list[TagMetaRecord]
    ExportEvent: list[TagMetaRecord]
    ExportRefType: list[TagMetaRecord]
    ExportEntity: list[TagMetaRecord]
    ExportSettings: list[TagMetaRecord]
    EngineHook: list[TagMetaRecord]
    MigrationRule: list[TagMetaRecord]
    CodeGen: list[TagMetaRecord]


def create_codegen_tag_store() -> CodeGenTagStore:
    return {
        'ExportEnum': [],
        'ExportValueType': [],
        'ExportProperty': [],
        'ExportMethod': [],
        'ExportEvent': [],
        'ExportRefType': [],
        'ExportEntity': [],
        'ExportSettings': [],
        'EngineHook': [],
        'MigrationRule': [],
        'CodeGen': [],
    }


def create_tag_meta_store() -> TagMetaStore:
    return {
        'ExportEnum': [],
        'ExportValueType': [],
        'ExportProperty': [],
        'ExportMethod': [],
        'ExportEvent': [],
        'ExportRefType': [],
        'ExportEntity': [],
        'ExportSettings': [],
        'EngineHook': [],
        'MigrationRule': [],
        'CodeGen': [],
    }


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description='FOnline code generator', fromfile_prefix_chars='@')
    parser.add_argument('-maincfg', dest='maincfg', required=True, help='main config file')
    parser.add_argument('-buildhash', dest='buildhash', required=True, help='build hash')
    parser.add_argument('-devname', dest='devname', required=True, help='dev game name')
    parser.add_argument('-nicename', dest='nicename', required=True, help='nice game name')
    parser.add_argument('-embedded', dest='embedded', required=True, help='embedded buffer capacity')
    parser.add_argument('-meta', dest='meta', required=True, action='append', help='path to script api metadata (///@ tags)')
    parser.add_argument('-commonheader', dest='commonheader', action='append', default=[], help='path to common header file')
    parser.add_argument('-genoutput', dest='genoutput', required=True, help='generated code output dir')
    parser.add_argument('-verbose', dest='verbose', action='store_true', help='verbose mode')
    return parser


args = argparse.Namespace()
start_time = 0.0


def log(*parts: object) -> None:
    print('[CodeGen]', *parts)


def get_guid(name: str) -> str:
    return '{' + str(uuid.uuid3(uuid.NAMESPACE_OID, name)).upper() + '}'


def int_to_4_bytes(value: int) -> int:
    return value & 0xffffffff


def get_hash(input_text: str, seed: int = 0) -> str:
    input_bytes = input_text.encode()

    m = 0x5bd1e995
    r = 24

    length = len(input_bytes)
    h = seed ^ length

    round_index = 0
    while length >= (round_index * 4) + 4:
        k = input_bytes[round_index * 4]
        k |= input_bytes[(round_index * 4) + 1] << 8
        k |= input_bytes[(round_index * 4) + 2] << 16
        k |= input_bytes[(round_index * 4) + 3] << 24
        k = int_to_4_bytes(k)
        k = int_to_4_bytes(k * m)
        k ^= k >> r
        k = int_to_4_bytes(k * m)
        h = int_to_4_bytes(h * m)
        h ^= k
        round_index += 1

    length_diff = length - (round_index * 4)

    if length_diff == 1:
        h ^= input_bytes[-1]
        h = int_to_4_bytes(h * m)
    elif length_diff == 2:
        h ^= int_to_4_bytes(input_bytes[-1] << 8)
        h ^= input_bytes[-2]
        h = int_to_4_bytes(h * m)
    elif length_diff == 3:
        h ^= int_to_4_bytes(input_bytes[-1] << 16)
        h ^= int_to_4_bytes(input_bytes[-2] << 8)
        h ^= input_bytes[-3]
        h = int_to_4_bytes(h * m)

    h ^= h >> 13
    h = int_to_4_bytes(h * m)
    h ^= h >> 15

    assert h <= 0xffffffff
    if h & 0x80000000 == 0:
        return str(h)
    else:
        return str(-((h ^ 0xFFFFFFFF) + 1))

assert get_hash('abcd') == '646393889'
assert get_hash('abcde') == '1594468574'
assert get_hash('abcdef') == '1271458169'
assert get_hash('abcdefg') == '-106836237'

# Generated file list
generated_file_list = ['EmbeddedResources-Include.h',
        'Version-Include.h',
        'GenericCode-Common.cpp',
        'MetadataRegistration-Server.cpp',
        'MetadataRegistration-Client.cpp',
        'MetadataRegistration-Mapper.cpp',
        'MetadataRegistration-ServerStub.cpp',
        'MetadataRegistration-ClientStub.cpp',
        'MetadataRegistration-MapperStub.cpp']

# Parse meta information
codegen_tags: CodeGenTagStore = create_codegen_tag_store()

def verbose_print(*parts: object) -> None:
    if args.verbose:
        log(*parts)

errors: list[tuple[object, ...]] = []

def show_error(*messages: object) -> None:
    global errors
    errors.append(messages)
    log(str(messages[0]))
    for message_part in messages[1:]:
        log('-', str(message_part))


def write_error_stub(output_dir: str, error_lines: list[str], file_name: str) -> None:
    with open(output_dir.rstrip('/') + '/' + file_name, 'w', encoding='utf-8') as file:
        file.write('\n'.join(error_lines))


def check_errors() -> None:
    if errors:
        try:
            error_lines = []
            error_lines.append('')
            error_lines.append('#error Code generation failed')
            error_lines.append('')
            error_lines.append('//  Stub generated due to code generation error')
            error_lines.append('//')
            for messages in errors:
                error_lines.append('//  ' + str(messages[0]))
                for message_part in messages:
                    error_lines.append('//  - ' + str(message_part))
            error_lines.append('//')

            for file_name in generated_file_list:
                write_error_stub(args.genoutput, error_lines, file_name)
            
        except Exception as ex:
            show_error('Can\'t write stubs', ex)
        
        for error_entry in errors[0]:
            if isinstance(error_entry, BaseException):
                log('Most recent exception:')
                raise error_entry
        
        sys.exit(1)


def run_codegen_step(action: Callable[[], None], error_message: str) -> None:
    try:
        action()
    except Exception as ex:
        show_error(error_message, ex)
    check_errors()


# Parse tags
tag_metas: TagMetaStore = create_tag_meta_store()


def find_comment_start(line: str) -> int:
    for start_pos, char in enumerate(line):
        if char != ' ' and char != '\t':
            return start_pos
    return -1


def collect_stripped_block(lines: list[str], start_index: int, end_marker: str, trim_left: bool = False) -> list[str] | None:
    for index in range(start_index, len(lines)):
        candidate = lines[index].lstrip() if trim_left else lines[index]
        if candidate.startswith(end_marker):
            return [line.strip() for line in lines[start_index:index] if line.strip()]
    return None


def find_enclosing_class_name(lines: list[str], line_index: int) -> str | None:
    for index in range(line_index, 0, -1):
        if lines[index].startswith('class '):
            return lines[index][6:lines[index].find(' ', 6)]
    return None


def find_enclosing_property_entity(lines: list[str], line_index: int) -> str | None:
    for index in range(line_index, 0, -1):
        if lines[index].startswith('class '):
            property_pos = lines[index].find('Properties')
            assert property_pos != -1
            return lines[index][6:property_pos]
    return None


def resolve_export_tag_context(tag_name: str, lines: list[str], line_index: int) -> TagContext:
    if tag_name == 'ExportEnum':
        return collect_stripped_block(lines, line_index + 1, '};', trim_left=True)
    if tag_name == 'ExportValueType':
        return True
    if tag_name == 'ExportProperty':
        property_entity = find_enclosing_property_entity(lines, line_index)
        assert property_entity is not None
        return property_entity + ' ' + lines[line_index + 1].strip()
    if tag_name == 'ExportMethod':
        return lines[line_index + 1].strip()
    if tag_name == 'ExportEvent':
        class_name = find_enclosing_class_name(lines, line_index)
        assert class_name is not None
        return class_name + ' ' + lines[line_index + 1].strip()
    if tag_name == 'ExportRefType':
        return collect_stripped_block(lines, line_index + 1, '};')
    if tag_name == 'ExportEntity':
        return True
    if tag_name == 'ExportSettings':
        return collect_stripped_block(lines, line_index + 1, 'SETTING_GROUP_END')
    assert False, 'Invalid export tag context ' + tag_name


def resolve_tag_context(tag_name: str, lines: list[str], line_index: int, tag_pos: int) -> TagContext:
    if tag_name.startswith('Export'):
        return resolve_export_tag_context(tag_name, lines, line_index)
    if tag_name == 'EngineHook':
        return lines[line_index + 1].strip()
    if tag_name == 'CodeGen':
        return tag_pos
    return None

def parse_meta_file(abs_path: str) -> None:
    try:
        with open(abs_path, 'r', encoding='utf-8-sig') as f:
            lines = f.readlines()
    except Exception as ex:
        show_error('Can\'t read file', abs_path, ex)
        return
        
    last_comment: CommentLines = []
    
    for line_index in range(len(lines)):
        line = lines[line_index]
        line_len = len(line)
        
        if line_len < 5:
            continue
        
        try:
            tag_pos = find_comment_start(line)
            if tag_pos == -1 or line_len - tag_pos <= 2 or line[tag_pos] != '/' or line[tag_pos + 1] != '/':
                last_comment = []
                continue
            
            if line_len - tag_pos >= 4 and line[tag_pos + 2] == '/' and line[tag_pos + 3] == '@':
                tag_str = line[tag_pos + 4:].strip()
                
                comment_pos = tag_str.find('//')
                if comment_pos != -1:
                    last_comment = [tag_str[comment_pos + 2:].strip()]
                    tag_str = tag_str[:comment_pos].rstrip()
                
                comment = last_comment if last_comment else []
                
                tag_split = tag_str.split(' ', 1)
                tag_name = tag_split[0]
                
                if tag_name not in tag_metas:
                    show_error('Invalid tag ' + tag_name, abs_path + ' (' + str(line_index + 1) + ')', line.strip())
                    continue
                
                tag_info = tag_split[1] if len(tag_split) > 1 else None

                tag_context = resolve_tag_context(tag_name, lines, line_index, tag_pos)
                
                tag_metas[tag_name].append(TagMetaRecord(abs_path, line_index, tag_info, tag_context, comment))
                last_comment = []
                
            elif line_len - tag_pos >= 3 and line[tag_pos + 2] != '/':
                last_comment.append(line[tag_pos + 2:].strip())
            else:
                last_comment = []
                
        except Exception as ex:
            show_error('Invalid tag format', abs_path + ' (' + str(line_index + 1) + ')', line.strip(), ex)

# Meta files from build system
meta_files_registry = []

def collect_meta_files() -> list[str]:
    meta_files: list[str] = []
    for path in args.meta:
        abs_path = os.path.abspath(path)
        if abs_path not in meta_files and 'GeneratedSource' not in abs_path:
            assert os.path.isfile(abs_path), 'Invalid meta file path ' + path
            meta_files.append(abs_path)
    meta_files.sort()
    return meta_files


def parse_meta_files() -> None:
    for abs_path in collect_meta_files():
        parse_meta_file(abs_path)

    check_errors()

ref_types: set[str] = set()
engine_enums: set[str] = set()
custom_types: set[str] = set()
custom_types_native_map: dict[str, str] = {}
game_entities: list[str] = []
game_entities_info: dict[str, EntityInfo] = {}
entity_relatives: set[str] = set()
generic_funcdefs: set[str] = set()

symbol_tokens = set('`~!@#$%^&*()+-=|\\/.,\';][]}{:><"')


def flush_token(result: list[str], current_token: str) -> str:
    if current_token.strip():
        result.append(current_token.strip())
    return ''


def tokenize(text: str | None, anySymbols: list[int] | None = None) -> list[str]:
    if anySymbols is None:
        anySymbols = []
    if text is None:
        return []
    text = text.strip()
    result: list[str] = []
    current_token = ''

    for ch in text:
        if ch in symbol_tokens:
            if len(result) in anySymbols:
                current_token += ch
                continue
            current_token = flush_token(result, current_token)
            current_token = ch
            current_token = flush_token(result, current_token)
        elif ch in ' \t':
            current_token = flush_token(result, current_token)
        else:
            current_token += ch
    current_token = flush_token(result, current_token)
    return result


def normalize_hash_text(text: str) -> str:
    return text.replace('\r', '').replace('\n', '').replace('\t', '').replace(' ', '')


def require_list_context(tag_context: TagContext, tag_name: str) -> list[str]:
    assert isinstance(tag_context, list), 'Invalid context type for ' + tag_name
    return tag_context


def require_str_context(tag_context: TagContext, tag_name: str) -> str:
    assert isinstance(tag_context, str), 'Invalid context type for ' + tag_name
    return tag_context


def require_int_context(tag_context: TagContext, tag_name: str) -> int:
    assert isinstance(tag_context, int), 'Invalid context type for ' + tag_name
    return tag_context


def require_enum_value_text(key_value: EnumKeyValue) -> str:
    assert key_value.value is not None, 'Enum value is not resolved for ' + key_value.key
    return key_value.value


def get_context_preview(tag_context: TagContext) -> str:
    if isinstance(tag_context, list):
        return tag_context[0] if tag_context else '(empty)'
    if isinstance(tag_context, str):
        return tag_context
    return '(empty)'


def hash_recursive(hasher: Any, data: Any) -> None:
    if isinstance(data, Mapping):
        for key in sorted(data.keys()):
            hasher.update(normalize_hash_text(str(key)).encode('utf-8'))
            hash_recursive(hasher, data[key])
    elif is_dataclass(data):
        hash_recursive(hasher, astuple(cast(Any, data)))
    elif isinstance(data, Iterable) and not isinstance(data, str):
        for index, item in enumerate(data):
            hasher.update(normalize_hash_text(str(index)).encode('utf-8'))
            hash_recursive(hasher, item)
    else:
        hasher.update(normalize_hash_text(str(data)).encode('utf-8'))

compatibility_hasher = hashlib.new('sha256')


def split_engine_args(function_args_text: str) -> list[str]:
    result: list[str] = []
    braces = 0
    current = ''
    for char in function_args_text:
        if char == ',' and braces == 0:
            result.append(current.strip())
            current = ''
        else:
            current += char
            if char == '<':
                braces += 1
            elif char == '>':
                braces -= 1
    if current.strip():
        result.append(current.strip())
    return result


def parse_export_property_definition(tag_context: str, export_flags: list[str], valid_types: set[str]) -> tuple[str, str, str, str, list[str]]:
    entity = tag_context[:tag_context.find(' ')]

    access = export_flags[0]
    assert access in ['Common', 'Server', 'Client'], 'Invalid export property target ' + access
    property_flags = export_flags[1:]

    tokens = [token.strip() for token in tag_context[tag_context.find('(') + 1:tag_context.find(')')].split(',')]
    assert len(tokens) == 2
    property_type = engine_type_to_meta_type(tokens[0], valid_types)
    assert 'uint64' not in property_type, 'Type uint64 is not supported by properties'
    property_name = tokens[1]
    property_flags.append('CoreProperty')
    return entity, access, property_type, property_name, property_flags


def resolve_property_targets(entity: str, property_flags: list[str], game_entities: list[str]) -> list[str]:
    if entity == 'Entity':
        property_flags.append('SharedProperty')
        return game_entities
    assert entity in game_entities, 'Invalid entity ' + entity
    return [entity]


def parse_method_args(args_text: str, valid_types: set[str], skip_first_arg: bool = False) -> list[MethodArg]:
    result_args: list[MethodArg] = []
    raw_args = split_engine_args(args_text)
    for arg in raw_args[1:] if skip_first_arg else raw_args:
        arg = arg.strip()
        separator = arg.rfind(' ')
        arg_type = engine_type_to_meta_type(arg[:separator].rstrip(), valid_types)
        arg_name = arg[separator + 1:]
        result_args.append(MethodArg(arg_type, arg_name))
    return result_args


def parse_export_method_signature(tag_context: str, valid_types: set[str], game_entities: list[str]) -> tuple[str, str, str, str, list[MethodArg]]:
    line_tokens = tokenize(tag_context)
    brace_open_pos = tag_context.find('(')
    brace_close_pos = tag_context.rfind(')')
    function_args = tag_context[brace_open_pos + 1:brace_close_pos]

    function_token_index = find_token_index(line_tokens, '(')
    assert function_token_index > 1, tag_context
    function_name = line_tokens[function_token_index - 1]
    ret = engine_type_to_meta_type(''.join(line_tokens[1:function_token_index - 1]), valid_types)

    function_tokens = function_name.split('_')
    assert len(function_tokens) == 3, function_name

    target = function_tokens[0].strip()
    assert target in EXPORT_TARGETS, target
    entity = function_tokens[1]
    assert entity in game_entities + ['Entity'], entity
    name = function_tokens[2]
    return target, entity, name, ret, parse_method_args(function_args, valid_types, skip_first_arg=True)


def resolve_event_target(tag_context: str, game_entities_info: Mapping[str, EntityInfo]) -> tuple[str, str]:
    class_name = tag_context[:tag_context.find(' ')].strip()
    for entity_name, entity_info in game_entities_info.items():
        if class_name == entity_info.server or class_name == entity_info.client:
            return ('Server' if class_name == entity_info.server else 'Client'), entity_name
    if class_name == 'MapperEngine':
        return 'Mapper', 'Game'
    assert False, class_name


def parse_export_event_signature(tag_context: str, valid_types: set[str]) -> tuple[str, list[MethodArg]]:
    brace_open_pos = tag_context.find('(')
    brace_close_pos = tag_context.rfind(')')
    first_comma_pos = tag_context.find(',', brace_open_pos)
    event_name = tag_context[brace_open_pos + 1:first_comma_pos if first_comma_pos != -1 else brace_close_pos].strip()

    event_args: list[MethodArg] = []
    if first_comma_pos != -1:
        args_text = tag_context[first_comma_pos + 1:brace_close_pos].strip()
        if args_text:
            for arg in split_engine_args(args_text):
                arg = arg.strip()
                separator = arg.find('/')
                arg_type = engine_type_to_meta_type(arg[:separator - 1].rstrip(), valid_types)
                arg_name = arg[separator + 2:-2]
                event_args.append(MethodArg(arg_type, arg_name))

    return event_name, event_args


def parse_settings_group_name(first_line: str) -> str:
    assert first_line.startswith('SETTING_GROUP'), 'Invalid start macro'
    group_name = first_line[first_line.find('(') + 1:first_line.find(',')]
    assert group_name.endswith('Settings'), 'Invalid group ending ' + group_name
    return group_name[:-len('Settings')]


def parse_settings_entry(line: str, valid_types: set[str]) -> SettingsEntry:
    setting_comment = [line[line.find('//') + 2:].strip()] if line.find('//') != -1 else []
    setting_type = line[:line.find('(')]
    assert setting_type in ['FIXED_SETTING', 'VARIABLE_SETTING'], 'Invalid setting type ' + setting_type
    setting_args = [token.strip().strip('"') for token in line[line.find('(') + 1:line.find(')')].split(',')]
    return SettingsEntry(
        'fix' if setting_type == 'FIXED_SETTING' else 'var',
        engine_type_to_meta_type(setting_args[0], valid_types),
        setting_args[1],
        setting_args[2:],
        setting_comment,
    )


def parse_settings_entries(tag_context: list[str], valid_types: set[str], hasher: Any) -> list[SettingsEntry]:
    settings: list[SettingsEntry] = []
    for line in tag_context[1:]:
        settings.append(parse_settings_entry(line, valid_types))
        hash_recursive(hasher, (settings[-1].kind, settings[-1].value_type, settings[-1].name, settings[-1].init_values))
    return settings


def parse_enum_key_values(enum_lines: list[str]) -> list[EnumKeyValue]:
    key_values: list[EnumKeyValue] = []
    assert enum_lines[1].startswith('{')
    for raw_line in enum_lines[2:]:
        comment_position = raw_line.find('//')
        line = raw_line[:comment_position].rstrip() if comment_position != -1 else raw_line
        separator = line.find('=')
        if separator == -1:
            next_value = str(int(require_enum_value_text(key_values[-1]), 0) + 1) if key_values else '0'
            key_values.append(EnumKeyValue(line.rstrip(','), next_value, []))
        else:
            key_values.append(EnumKeyValue(line[:separator].rstrip(), line[separator + 1:].lstrip().rstrip(','), []))
    return key_values


def find_token_index(tokens: list[str], value: str, start: int = 0) -> int:
    try:
        return tokens.index(value, start)
    except ValueError:
        return -1


def parse_exported_ref_type_method(stripped_line: str, line_tokens: list[str], function_token_index: int, valid_types: set[str]) -> RefTypeMethod:
    if line_tokens[0] == 'auto':
        signature_end = line_tokens.index(')', function_token_index)
        assert line_tokens[signature_end + 1] == '-'
        assert line_tokens[signature_end + 2] == '>'
        declaration_end = line_tokens.index(';', signature_end + 2)
        return_type = ''.join(line_tokens[signature_end + 3:declaration_end])
    else:
        assert line_tokens[0] == 'void'
        return_type = 'void'

    function_begin = stripped_line.index('(')
    function_end = stripped_line.index(')')
    assert function_begin != -1 and function_end != -1 and function_begin < function_end, 'Invalid function definition'
    function_args = stripped_line[function_begin + 1:function_end]
    result_args = parse_method_args(function_args, valid_types)
    return RefTypeMethod(line_tokens[function_token_index - 1], engine_type_to_meta_type(return_type, valid_types), result_args, [])


def parse_exported_ref_type_members(tag_context: list[str], export_flags: list[str], valid_types: set[str]) -> tuple[list[RefTypeField], list[RefTypeMethod]]:

    fields: list[RefTypeField] = []
    methods: list[RefTypeMethod] = []

    export_key = find_token_index(export_flags, 'Export')
    if export_key == -1:
        return fields, methods

    assert export_flags[export_key + 1] == '='
    export_tokens = [token for token in export_flags[export_key + 2:] if token != ',']
    assert export_tokens

    for line in tag_context[2:]:
        stripped_line = line.lstrip()
        comment_position = stripped_line.find('//')
        if comment_position != -1:
            stripped_line = stripped_line[:comment_position]
        if not stripped_line:
            continue

        line_tokens = tokenize(stripped_line)
        assert len(line_tokens) >= 2
        value_token_index = find_token_index(line_tokens, '{')
        function_token_index = find_token_index(line_tokens, '(')

        if value_token_index != -1 and line_tokens[value_token_index - 1] in export_tokens:
            fields.append(RefTypeField(engine_type_to_meta_type(''.join(line_tokens[:value_token_index - 1]), valid_types), line_tokens[value_token_index - 1], []))
            export_tokens.remove(line_tokens[value_token_index - 1])
            continue

        if function_token_index == -1 or line_tokens[function_token_index - 1] not in export_tokens:
            continue

        methods.append(parse_exported_ref_type_method(stripped_line, line_tokens, function_token_index, valid_types))
        export_tokens.remove(line_tokens[function_token_index - 1])

    assert not export_tokens, 'Exports not found ' + str(export_tokens)
    return fields, methods


def create_valid_types() -> set[str]:
    valid_types = set()
    valid_types.update([
        'int8', 'uint8', 'int16', 'uint16', 'int32', 'uint32', 'int64', 'uint64',
        'float32', 'float64', 'string', 'bool', 'Entity', 'void', 'hstring', 'any',
    ])
    return valid_types


def register_export_enum(group_name: str, underlying_type: str, key_values: list[EnumKeyValue], export_flags: list[str], comment: CommentLines, valid_types: set[str]) -> None:
    codegen_tags['ExportEnum'].append(ExportEnumTag(group_name, underlying_type, key_values, export_flags, comment))
    hash_recursive(compatibility_hasher, (group_name, underlying_type, key_values, export_flags))

    assert group_name not in valid_types, 'Enum already in valid types'
    valid_types.add(group_name)
    assert group_name not in engine_enums, 'Enum already in engine enums'
    engine_enums.add(group_name)


def register_entity_relative_type(type_name: str, valid_types: set[str]) -> None:
    assert type_name not in valid_types
    valid_types.add(type_name)
    assert type_name not in entity_relatives
    entity_relatives.add(type_name)


def unified_type_to_meta_type(unified_type: str, valid_types: set[str]) -> str:
    if unified_type.startswith('callback('):
        assert unified_type[-1] == ')'
        callback_args = [unified_type_to_meta_type(arg, valid_types) for arg in unified_type[unified_type.find('(') + 1:unified_type.find(')')].split(',')]
        generic_funcdefs.add('|'.join(callback_args))
        return 'callback.' + '|'.join(callback_args) + '|'
    if unified_type.endswith('&'):
        return unified_type_to_meta_type(unified_type[:-1], valid_types) + '.ref'
    if '=>' in unified_type:
        key_type, value_type = unified_type.split('=>')
        return 'dict.' + unified_type_to_meta_type(key_type, valid_types) + '.' + unified_type_to_meta_type(value_type, valid_types)
    if unified_type.endswith('[]'):
        return 'arr.' + unified_type_to_meta_type(unified_type[:-2], valid_types)
    if unified_type == 'SELF_ENTITY':
        return 'SELF_ENTITY'
    assert unified_type in valid_types, 'Invalid type ' + unified_type
    return unified_type


def engine_type_to_unified_type(engine_type: str, valid_types: set[str]) -> str:
    type_map = {
        'int8': 'int8', 'uint8': 'uint8', 'int16': 'int16', 'uint16': 'uint16',
        'int32': 'int32', 'uint32': 'uint32', 'int64': 'int64', 'uint64': 'uint64',
        'float32': 'float32', 'float64': 'float64', 'bool': 'bool', 'void': 'void',
        'string_view': 'string', 'string': 'string', 'hstring': 'hstring', 'any_t': 'any',
    }
    if engine_type.startswith('ScriptFunc<'):
        function_args = split_engine_args(engine_type[engine_type.find('<') + 1:engine_type.rfind('>')])
        return 'callback(' + ','.join([engine_type_to_unified_type(arg.strip(), valid_types) for arg in function_args]) + ')'
    if engine_type.startswith('readonly_vector<'):
        result = engine_type_to_unified_type(engine_type[engine_type.find('<') + 1:engine_type.rfind('>')], valid_types) + '[]'
        assert not engine_type.endswith('&')
        return result
    if engine_type.startswith('readonly_map<'):
        key_type, value_type = engine_type[engine_type.find('<') + 1:engine_type.rfind('>')].split(',', 1)
        result = engine_type_to_unified_type(key_type.strip(), valid_types) + '=>' + engine_type_to_unified_type(value_type.strip(), valid_types)
        assert not engine_type.endswith('&')
        return result
    if engine_type.startswith('vector<'):
        result = engine_type_to_unified_type(engine_type[engine_type.find('<') + 1:engine_type.rfind('>')], valid_types) + '[]'
        if engine_type.endswith('&'):
            result += '&'
        return result
    if engine_type.startswith('map<'):
        key_type, value_type = engine_type[engine_type.find('<') + 1:engine_type.rfind('>')].split(',', 1)
        result = engine_type_to_unified_type(key_type.strip(), valid_types) + '=>' + engine_type_to_unified_type(value_type.strip(), valid_types)
        if engine_type.endswith('&'):
            result += '&'
        return result
    if engine_type[-1] in ['&', '*'] and engine_type[:-1] in custom_types_native_map:
        return custom_types_native_map[engine_type[:-1]] + engine_type[-1]
    if engine_type in custom_types_native_map:
        return custom_types_native_map[engine_type]
    if engine_type in type_map:
        return type_map[engine_type]
    if engine_type[-1] == '&' and engine_type[:-1] in type_map:
        return type_map[engine_type[:-1]] + '&'
    if engine_type[-1] == '*' and engine_type[:-1] in type_map:
        return type_map[engine_type[:-1]] + '*'
    if engine_type in valid_types:
        return engine_type
    if engine_type[-1] == '&' and engine_type[:-1] in valid_types:
        return engine_type
    if engine_type[-1] == '*' and engine_type not in type_map:
        entity_type = engine_type[:-1]
        if entity_type in ref_types or entity_type in entity_relatives:
            return entity_type
        for entity_name, entity_info in game_entities_info.items():
            if entity_type == entity_info.server or entity_type == entity_info.client:
                return entity_name
        if entity_type in ['ServerEntity', 'ClientEntity', 'Entity']:
            return 'Entity'
        if entity_type == 'ScriptSelfEntity':
            return 'SELF_ENTITY'
        assert False, entity_type
    assert False, 'Invalid engine type ' + engine_type


def engine_type_to_meta_type(engine_type: str, valid_types: set[str]) -> str:
    return unified_type_to_meta_type(engine_type_to_unified_type(engine_type, valid_types), valid_types)


def find_next_enum_value(key_values: list[EnumKeyValue]) -> str:
    result = 0
    for key_value in key_values:
        if key_value.value is not None:
            int_value = int(key_value.value, 0)
            if int_value >= result:
                result = int_value + 1
    return str(result)


def postprocess_tags() -> None:
    for enum_tag in codegen_tags['ExportEnum']:
        for key_value in enum_tag.key_values:
            if key_value.value is None:
                key_value.value = find_next_enum_value(enum_tag.key_values)
                hash_recursive(compatibility_hasher, key_value.value)

    for entity in game_entities:
        key_values = [EnumKeyValue('None', '0', [])]
        index = 1
        for prop_tag in codegen_tags['ExportProperty']:
            if prop_tag.entity == entity:
                key_values.append(EnumKeyValue(prop_tag.name.replace('.', '_'), str(index), []))
                index += 1
        codegen_tags['ExportEnum'].append(ExportEnumTag(entity + 'Property', 'uint16', key_values, [], []))
        hash_recursive(compatibility_hasher, (entity + 'Property', 'uint16', key_values))

    for enum_tag in codegen_tags['ExportEnum']:
        if not [1 for key_value in enum_tag.key_values if int(require_enum_value_text(key_value), 0) == 0]:
            show_error('Zero entry not found in enum group ' + enum_tag.group_name)

    for enum_tag in codegen_tags['ExportEnum']:
        keys = set()
        values = set()
        for key_value in enum_tag.key_values:
            if key_value.key in keys:
                show_error('Detected collision in enum group ' + enum_tag.group_name + ' key ' + key_value.key)
            value_text = require_enum_value_text(key_value)
            evaluated_value = int(value_text, 0)
            if evaluated_value in values:
                show_error('Detected collision in enum group ' + enum_tag.group_name + ' value ' + value_text + (' (evaluated to ' + str(evaluated_value) + ')' if str(evaluated_value) != value_text else ''))
            keys.add(key_value.key)
            values.add(evaluated_value)


def parse_enum_tags(valid_types: set[str]) -> None:
    for tag_meta in tag_metas['ExportEnum']:
        abs_path = tag_meta.abs_path
        line_index = tag_meta.line_index
        tag_info = tag_meta.tag_info
        tag_context = tag_meta.tag_context
        comment = tag_meta.comment

        try:
            enum_lines = require_list_context(tag_context, 'ExportEnum')
            export_flags = tokenize(tag_info)

            first_line = enum_lines[0]
            assert first_line.startswith('enum class ')
            separator = first_line.find(':')
            group_name = first_line[len('enum class '):separator if separator != -1 else len(first_line)].strip()
            underlying_type = engine_type_to_meta_type('int32' if separator == -1 else first_line[separator + 1:].strip(), valid_types)

            key_values = parse_enum_key_values(enum_lines)
            register_export_enum(group_name, underlying_type, key_values, export_flags, comment, valid_types)

        except Exception as ex:
            show_error('Invalid tag ExportEnum', abs_path + ' (' + str(line_index + 1) + ')', get_context_preview(tag_context), ex)


def parse_export_value_type_tags(valid_types: set[str]) -> None:
    for tag_meta in tag_metas['ExportValueType']:
        abs_path = tag_meta.abs_path
        line_index = tag_meta.line_index
        tag_info = tag_meta.tag_info
        tag_context = tag_meta.tag_context
        comment = tag_meta.comment

        try:
            tokens = tokenize(tag_info)

            type_name = tokens[0]
            native_type = tokens[1]
            export_flags = tokens[2:]

            assert 'Layout' in export_flags, 'No Layout specified in ExportValueType'
            assert export_flags[export_flags.index('Layout') + 1] == '=', 'Expected "=" after Layout tag'

            codegen_tags['ExportValueType'].append(ExportValueTypeTag(type_name, native_type, export_flags, comment))
            hash_recursive(compatibility_hasher, (type_name, native_type, export_flags))

            assert type_name not in valid_types, 'Type already in valid types'
            valid_types.add(type_name)
            assert type_name not in custom_types, 'Type already in custom types'
            custom_types.add(type_name)
            assert native_type not in custom_types_native_map, 'Type already in custom types native map'
            custom_types_native_map[native_type] = type_name

        except Exception as ex:
            show_error('Invalid tag ExportValueType', abs_path + ' (' + str(line_index + 1) + ')', get_context_preview(tag_context), ex)


def parse_export_entity_tags(valid_types: set[str]) -> None:
    for tag_meta in tag_metas['ExportEntity']:
        abs_path = tag_meta.abs_path
        line_index = tag_meta.line_index
        tag_info = tag_meta.tag_info
        tag_context = tag_meta.tag_context
        comment = tag_meta.comment

        try:
            export_flags = tokenize(tag_info)

            entity_name = export_flags[0]
            server_class_name = export_flags[1]
            client_class_name = export_flags[2]
            export_flags = export_flags[3:]

            codegen_tags['ExportEntity'].append(ExportEntityTag(entity_name, server_class_name, client_class_name, export_flags, comment))
            hash_recursive(compatibility_hasher, (entity_name, server_class_name, client_class_name, export_flags))

            assert entity_name not in valid_types
            valid_types.add(entity_name)
            assert entity_name not in game_entities
            game_entities.append(entity_name)
            game_entities_info[entity_name] = EntityInfo(
                server=server_class_name,
                client=client_class_name,
                is_global='Global' in export_flags,
                has_protos='HasProtos' in export_flags,
                has_statics='HasStatics' in export_flags,
                has_abstract='HasAbstract' in export_flags,
                has_time_events='HasTimeEvents' in export_flags,
                exported=True,
                comment=comment,
            )

            assert entity_name + 'Component' not in valid_types, entity_name + 'Property component already in valid types'
            valid_types.add(entity_name + 'Component')
            assert entity_name + 'Property' not in valid_types, entity_name + 'Property already in valid types'
            valid_types.add(entity_name + 'Property')

            if game_entities_info[entity_name].has_abstract:
                register_entity_relative_type('Abstract' + entity_name, valid_types)
            if game_entities_info[entity_name].has_protos:
                register_entity_relative_type('Proto' + entity_name, valid_types)
            if game_entities_info[entity_name].has_statics:
                register_entity_relative_type('Static' + entity_name, valid_types)

        except Exception as ex:
            show_error('Invalid tag ExportEntity', abs_path + ' (' + str(line_index + 1) + ')', ex)


def parse_foundation_tags(valid_types: set[str]) -> None:
    parse_export_value_type_tags(valid_types)
    parse_export_entity_tags(valid_types)


def parse_ref_type_tags(valid_types: set[str]) -> None:
    for tag_meta in tag_metas['ExportRefType']:
        abs_path = tag_meta.abs_path
        line_index = tag_meta.line_index
        tag_info = tag_meta.tag_info
        tag_context = tag_meta.tag_context
        comment = tag_meta.comment

        try:
            ref_type_lines = require_list_context(tag_context, 'ExportRefType')
            export_flags = tokenize(tag_info)
            assert export_flags and export_flags[0] in EXPORT_TARGETS, 'Expected target in tag info'
            target = export_flags[0]
            export_flags = export_flags[1:]

            first_line = ref_type_lines[0]
            first_line_tokens = tokenize(first_line)
            assert len(first_line_tokens) >= 2, 'Expected 4 or more tokens in first line'
            assert first_line_tokens[0] in ['class', 'struct'], 'Expected class/struct'

            ref_type_name = first_line_tokens[1]
            assert ref_type_name not in valid_types

            assert ref_type_lines[1].startswith('{')

            fields, methods = parse_exported_ref_type_members(ref_type_lines, export_flags, valid_types)

            codegen_tags['ExportRefType'].append(ExportRefTypeTag(target, ref_type_name, fields, methods, export_flags, comment))
            hash_recursive(compatibility_hasher, (target, ref_type_name, fields, methods, export_flags))

            valid_types.add(ref_type_name)
            ref_types.add(ref_type_name)

        except Exception as ex:
            show_error('Invalid tag ExportRefType', abs_path + ' (' + str(line_index + 1) + ')', get_context_preview(tag_context), ex)


def parse_export_property_tags(valid_types: set[str]) -> None:
    for tag_meta in tag_metas['ExportProperty']:
        abs_path = tag_meta.abs_path
        line_index = tag_meta.line_index
        tag_info = tag_meta.tag_info
        tag_context = tag_meta.tag_context
        comment = tag_meta.comment

        try:
            property_context = require_str_context(tag_context, 'ExportProperty')
            entity, access, property_type, property_name, property_flags = parse_export_property_definition(property_context, tokenize(tag_info), valid_types)
            for target_entity in resolve_property_targets(entity, property_flags, game_entities):
                codegen_tags['ExportProperty'].append(ExportPropertyTag(target_entity, access, property_type, property_name, property_flags, comment))
                hash_recursive(compatibility_hasher, (target_entity, access, property_type, property_name, property_flags))

        except Exception as ex:
            show_error('Invalid tag ExportProperty', abs_path + ' (' + str(line_index + 1) + ')', tag_context, ex)


def parse_export_method_tags(valid_types: set[str]) -> None:
    for tag_meta in tag_metas['ExportMethod']:
        abs_path = tag_meta.abs_path
        line_index = tag_meta.line_index
        tag_info = tag_meta.tag_info
        tag_context = tag_meta.tag_context
        comment = tag_meta.comment

        try:
            method_context = require_str_context(tag_context, 'ExportMethod')
            export_flags = tokenize(tag_info)

            target, entity, name, ret, result_args = parse_export_method_signature(method_context, valid_types, game_entities)

            codegen_tags['ExportMethod'].append(ExportMethodTag(target, entity, name, ret, result_args, export_flags, comment))
            hash_recursive(compatibility_hasher, (target, entity, name, ret, result_args, export_flags))

        except Exception as ex:
            show_error('Invalid tag ExportMethod', abs_path + ' (' + str(line_index + 1) + ')', tag_context, ex)


def parse_export_event_tags(valid_types: set[str]) -> None:
    for tag_meta in tag_metas['ExportEvent']:
        abs_path = tag_meta.abs_path
        line_index = tag_meta.line_index
        tag_info = tag_meta.tag_info
        tag_context = tag_meta.tag_context
        comment = tag_meta.comment

        try:
            event_context = require_str_context(tag_context, 'ExportEvent')
            export_flags = tokenize(tag_info)

            target, entity = resolve_event_target(event_context, game_entities_info)
            assert target in ['Server', 'Client', 'Mapper'], target

            event_name, event_args = parse_export_event_signature(event_context, valid_types)

            codegen_tags['ExportEvent'].append(ExportEventTag(target, entity, event_name, event_args, export_flags, comment))
            hash_recursive(compatibility_hasher, (target, entity, event_name, event_args, export_flags))

        except Exception as ex:
            show_error('Invalid tag ExportEvent', abs_path + ' (' + str(line_index + 1) + ')', tag_context, ex)


def parse_export_settings_tags(valid_types: set[str]) -> None:
    for tag_meta in tag_metas['ExportSettings']:
        abs_path = tag_meta.abs_path
        line_index = tag_meta.line_index
        tag_info = tag_meta.tag_info
        tag_context = tag_meta.tag_context
        comment = tag_meta.comment

        try:
            settings_context = require_list_context(tag_context, 'ExportSettings')
            export_flags = tokenize(tag_info)
            assert export_flags and export_flags[0] in EXPORT_TARGETS, 'Expected target in tag info'
            target = export_flags[0]
            export_flags = export_flags[1:]

            group_name = parse_settings_group_name(settings_context[0])
            settings = parse_settings_entries(settings_context, valid_types, compatibility_hasher)

            codegen_tags['ExportSettings'].append(ExportSettingsTag(group_name, target, settings, export_flags, comment))
            hash_recursive(compatibility_hasher, (group_name, target, export_flags))

        except Exception as ex:
            show_error('Invalid tag ExportSettings', abs_path + ' (' + str(line_index + 1) + ')', tag_context, ex)


def parse_engine_hook_tags() -> None:
    for tag_meta in tag_metas['EngineHook']:
        abs_path = tag_meta.abs_path
        line_index = tag_meta.line_index
        tag_info = tag_meta.tag_info
        tag_context = tag_meta.tag_context
        comment = tag_meta.comment

        try:
            hook_context = require_str_context(tag_context, 'EngineHook')
            name = tokenize(hook_context)[2]
            assert name in ['InitServerEngine', 'InitClientEngine', 'ConfigSectionParseHook', 'ConfigEntryParseHook', 'SetupBakersHook'], 'Invalid engine hook ' + name

            codegen_tags['EngineHook'].append(EngineHookTag(name, [], comment))
            hash_recursive(compatibility_hasher, name)

        except Exception as ex:
            show_error('Invalid tag EngineHook', abs_path + ' (' + str(line_index + 1) + ')', tag_info, ex)


def parse_migration_rule_tags() -> None:
    for tag_meta in tag_metas['MigrationRule']:
        abs_path = tag_meta.abs_path
        line_index = tag_meta.line_index
        tag_info = tag_meta.tag_info
        tag_context = tag_meta.tag_context
        comment = tag_meta.comment

        try:
            rule_args = tokenize(tag_info, anySymbols=[2, 3])
            assert len(rule_args) and rule_args[0] in ['Version', 'Property', 'Proto', 'Component', 'Remove'], 'Invalid migration rule'
            assert len(rule_args) == 4, 'Invalid migration rule args'
            assert not len([rule_tag for rule_tag in codegen_tags['MigrationRule'] if rule_tag.args[0:3] == rule_args[0:3]]), 'Migration rule already added'
            assert rule_args[2] != rule_args[3], 'Migration rule same last args'

            codegen_tags['MigrationRule'].append(MigrationRuleTag(rule_args, comment))
            hash_recursive(compatibility_hasher, rule_args)

        except Exception as ex:
            show_error('Invalid tag MigrationRule', abs_path + ' (' + str(line_index + 1) + ')', tag_info, ex)


def parse_codegen_tags() -> None:
    for tag_meta in tag_metas['CodeGen']:
        abs_path = tag_meta.abs_path
        line_index = tag_meta.line_index
        tag_info = tag_meta.tag_info
        tag_context = tag_meta.tag_context
        comment = tag_meta.comment

        try:
            file_name = os.path.basename(abs_path)
            if file_name == 'MetadataRegistration-Template.cpp':
                template_type = 'MetadataRegistration'
            elif file_name == 'GenericCode-Template.cpp':
                template_type = 'GenericCode'
            else:
                assert False, file_name

            flags = tokenize(tag_info)
            assert len(flags) >= 1, 'Invalid CodeGen entry'

            codegen_tags['CodeGen'].append(CodeGenTag(template_type, abs_path, flags[0], line_index, tag_context, flags[1:], comment))
            hash_recursive(compatibility_hasher, (template_type, flags[0], line_index, flags[1:]))

        except Exception as ex:
            show_error('Invalid tag CodeGen', abs_path + ' (' + str(line_index + 1) + ')', tag_info, ex)


def parse_runtime_tags(valid_types: set[str]) -> None:
    parse_export_property_tags(valid_types)
    parse_export_method_tags(valid_types)
    parse_export_event_tags(valid_types)
    parse_export_settings_tags(valid_types)
    parse_engine_hook_tags()
    parse_migration_rule_tags()
    parse_codegen_tags()

def parse_tags() -> None:
    valid_types = create_valid_types()

    parse_enum_tags(valid_types)
    parse_foundation_tags(valid_types)
    parse_ref_type_tags(valid_types)
    parse_runtime_tags(valid_types)
    postprocess_tags()

def parse_all_tags() -> None:
    global tag_metas
    parse_tags()
    check_errors()
    tag_metas = create_tag_meta_store()

# Generate API
class GeneratedOutput:
    def __init__(self) -> None:
        self.files: GeneratedFileMap = {}
        self.current_file: list[str] | None = None
        self.generated_file_names: list[str] = []
        self.current_template_type: str | None = None

    def create_file(self, name: str, output: str) -> None:
        assert output
        path = os.path.join(output, name)
        self.generated_file_names.append(name)
        self.current_file = []
        self.files[path] = self.current_file

    def write_line(self, line: str) -> None:
        assert self.current_file is not None, 'Output file is not initialized'
        self.current_file.append(line)

    def insert_lines(self, lines: list[str], line_index: int) -> None:
        assert self.current_file is not None, 'Output file is not initialized'
        assert line_index <= len(self.current_file), 'Invalid line index'
        updated_lines = self.current_file[:line_index] + lines + self.current_file[line_index:]
        self.current_file.clear()
        self.current_file.extend(updated_lines)

    def flush(self) -> None:
        for file_name in generated_file_list:
            if file_name not in self.generated_file_names:
                self.create_file(file_name, args.genoutput)
                self.write_line('// Empty file')
                verbose_print('flushFiles', 'empty', file_name)

        for path, lines in self.files.items():
            path_dir = os.path.dirname(path)
            if not os.path.isdir(path_dir):
                os.makedirs(path_dir)

            if os.path.isfile(path):
                with open(path, 'r', encoding='utf-8-sig') as f:
                    current_lines = f.readlines()
                new_lines_str = ''.join([line.rstrip('\r\n') for line in current_lines]).rstrip()
                current_lines_str = ''.join(lines).rstrip()
                if new_lines_str == current_lines_str:
                    verbose_print('flushFiles', 'skip', path)
                    continue
                verbose_print('flushFiles', 'diff found', path, len(new_lines_str), len(current_lines_str))
            else:
                verbose_print('flushFiles', 'no file', path)

            with open(path, 'w', encoding='utf-8') as f:
                f.write('\n'.join(lines).rstrip('\n') + '\n')
            verbose_print('flushFiles', 'write', path)

    def write_codegen_template(self, template_type: str) -> None:
        self.current_template_type = template_type

        template_path = None
        for gen_tag in codegen_tags['CodeGen']:
            if self.current_template_type == gen_tag.template_type:
                template_path = gen_tag.abs_path
                break
        assert template_path, 'Code gen template not found'

        with open(template_path, 'r', encoding='utf-8-sig') as f:
            lines = f.readlines()
        self.insert_lines([line.rstrip('\r\n') for line in lines], 0)

    def insert_codegen_lines(self, lines: list[str], entry_name: str) -> None:
        line_index, padding = self._find_codegen_tag(entry_name)

        if padding:
            lines = [''.center(padding) + line for line in lines]

        self.insert_lines(lines, line_index)

    def _find_codegen_tag(self, entry_name: str) -> tuple[int, int]:
        for gen_tag in codegen_tags['CodeGen']:
            if self.current_template_type == gen_tag.template_type and entry_name == gen_tag.entry:
                return gen_tag.line_index + 1, require_int_context(gen_tag.tag_context, 'CodeGen')
        assert self.current_template_type is not None, 'Code gen template type is not initialized'
        assert False, 'Code gen entry ' + entry_name + ' in ' + self.current_template_type + ' not found'


generated_output = GeneratedOutput()

def get_entity_from_target(target: str) -> str:
    if target == 'Server':
        return 'ServerEntity*'
    if target in CLIENT_ENTITY_TARGETS:
        return 'ClientEntity*'
    return 'Entity*'


def meta_type_to_unified_type(meta_type: str, self_entity: str = 'SELF_ENTITY') -> str:
    type_parts = meta_type.split('.')
    if type_parts[0] == 'dict':
        value_type = type_parts[2] if type_parts[2] != 'arr' else type_parts[2] + '.' + type_parts[3]
        result = meta_type_to_unified_type(type_parts[1], self_entity) + '=>' + meta_type_to_unified_type(value_type, self_entity)
    elif type_parts[0] == 'arr':
        result = meta_type_to_unified_type(type_parts[1], self_entity) + '[]'
    elif type_parts[0] == 'callback':
        result = 'callback(' + ','.join([meta_type_to_unified_type(arg, self_entity) for arg in '.'.join(type_parts[1:]).split('|') if arg]) + ')'
    elif type_parts[0] == 'Entity':
        result = type_parts[0]
    elif type_parts[0] in game_entities:
        result = type_parts[0]
    elif type_parts[0] in ref_types or type_parts[0] in entity_relatives:
        result = type_parts[0]
    elif type_parts[0] == 'SELF_ENTITY':
        result = self_entity
    else:
        result = type_parts[0]
    if type_parts[-1] == 'ref':
        result += '&'
    return result


def map_meta_type(meta_type: str) -> str:
    type_map = {'any': 'any_t'}
    return type_map[meta_type] if meta_type in type_map else meta_type


def cpp_bool(value: bool) -> str:
    return 'true' if value else 'false'


def resolve_method_registration_info(entity: str, method_tag: ExportMethodTag, target: str) -> MethodRegistrationInfo:
    is_generic = method_tag.entity == 'Entity'
    entity_info = game_entities_info[entity]
    engine_entity_type = entity_info.client if target != 'Server' else entity_info.server
    engine_entity_type_extern = get_entity_from_target(target) if is_generic else engine_entity_type + '*'
    engine_entity_type_name = 'Entity' if is_generic else entity
    if entity == 'Game':
        if target == 'Mapper':
            engine_entity_type = 'MapperEngine*'
        if method_tag.target == 'Common':
            engine_entity_type_extern = 'BaseEngine*'
        elif method_tag.target == 'Mapper':
            engine_entity_type_extern = 'MapperEngine*'
    return MethodRegistrationInfo(
        function_name=method_tag.target + '_' + engine_entity_type_name + '_' + method_tag.name,
        engine_entity_type_extern=engine_entity_type_extern,
        return_type=meta_type_to_engine_type(method_tag.ret, method_tag.target, False, self_entity='Entity'),
    )


def resolve_engine_entity_type(entity_name: str, target: str) -> str:
    entity_info = game_entities_info[entity_name]
    if target == 'Server':
        return entity_info.server + '*'
    if entity_name == 'Game' and target == 'Mapper':
        return 'MapperEngine*'
    return entity_info.client + '*'


def meta_type_to_engine_type(meta_type: str, target: str, pass_in: bool, ref_as_ptr: bool = False, self_entity: str | None = None, no_ref: bool = False) -> str:
    type_parts = meta_type.split('.')
    if type_parts[0] == 'string':
        result = 'string_view' if pass_in and type_parts[-1] != 'ref' else 'string'
    elif type_parts[0] == 'dict':
        value_type = type_parts[2] if type_parts[2] != 'arr' else type_parts[2] + '.' + type_parts[3]
        if pass_in and type_parts[-1] != 'ref':
            result = 'readonly_map<' + meta_type_to_engine_type(type_parts[1], target, False, self_entity=self_entity) + ', ' + meta_type_to_engine_type(value_type, target, False, self_entity=self_entity) + '>'
        else:
            result = 'map<' + meta_type_to_engine_type(type_parts[1], target, False, self_entity=self_entity) + ', ' + meta_type_to_engine_type(value_type, target, False, self_entity=self_entity) + '>'
    elif type_parts[0] == 'arr':
        if pass_in and type_parts[-1] != 'ref':
            result = 'readonly_vector<' + meta_type_to_engine_type(type_parts[1], target, False, self_entity=self_entity) + '>'
        else:
            result = 'vector<' + meta_type_to_engine_type(type_parts[1], target, False, self_entity=self_entity) + '>'
    elif type_parts[0] == 'callback':
        callback_signature = '<' + ', '.join([meta_type_to_engine_type(arg, target, False, ref_as_ptr=True, self_entity=self_entity) for arg in '.'.join(type_parts[1:]).split('|') if arg]) + '>'
        result = 'ScriptFunc' + callback_signature
    elif type_parts[0] == 'Entity':
        result = get_entity_from_target(target)
    elif type_parts[0] == 'SELF_ENTITY':
        assert self_entity
        result = resolve_engine_entity_type(self_entity, target) if self_entity != 'Entity' else 'ScriptSelfEntity*'
    elif type_parts[0] in game_entities:
        result = resolve_engine_entity_type(type_parts[0], target)
    elif type_parts[0] in ref_types or type_parts[0] in entity_relatives:
        result = type_parts[0] + '*'
    elif type_parts[0] in custom_types:
        result = ''
        for exported_value_type in codegen_tags['ExportValueType']:
            if exported_value_type.name == type_parts[0]:
                result = exported_value_type.native_type
                break
        assert result, 'Invalid native type ' + type_parts[0]
    else:
        result = map_meta_type(type_parts[0])
    if type_parts[-1] == 'ref' and not no_ref:
        result += '&' if not ref_as_ptr else '*'
    return result


def is_engine_hook_enabled(hook_name: str) -> bool:
    for hook_tag in codegen_tags['EngineHook']:
        if hook_tag.name == hook_name:
            return True
    return False


def append_settings_getter(global_lines: list[str], target: str) -> None:
    global_lines.append('[[maybe_unused]] auto Get' + target + 'Settings() -> unordered_set<string>')
    global_lines.append('{')
    global_lines.append('    unordered_set<string> settings = {')
    for settings_tag in codegen_tags['ExportSettings']:
        if settings_tag.target in [target, 'Common']:
            for setting in settings_tag.settings:
                global_lines.append('        "' + setting.name + '",')
    global_lines.append('    };')
    global_lines.append('    return settings;')
    global_lines.append('}')
    global_lines.append('')

def generate_generic_code() -> None:
    global_lines = []
    
    global_lines.append('// Engine hooks')
    if not is_engine_hook_enabled('InitServerEngine'):
        global_lines.append('class ServerEngine;')
        global_lines.append('void InitServerEngine(ServerEngine*) { /* Stub */ }')
    if not is_engine_hook_enabled('InitClientEngine'):
        global_lines.append('class ClientEngine;')
        global_lines.append('void InitClientEngine(ClientEngine*) { /* Stub */ }')
    if not is_engine_hook_enabled('ConfigSectionParseHook'):
        global_lines.append('bool ConfigSectionParseHook(string_view, string_view, string&, map<string, string>&) {  /* Stub */ return false; }')
    if not is_engine_hook_enabled('ConfigEntryParseHook'):
        global_lines.append('bool ConfigEntryParseHook(string_view, string_view, string_view, string_view, string&, string&) {  /* Stub */ return false; }')
    if not is_engine_hook_enabled('SetupBakersHook'):
        global_lines.append('class BaseBaker;')
        global_lines.append('struct BakingContext;')
        global_lines.append('void SetupBakersHook(span<const string>, vector<unique_ptr<BaseBaker>>&, shared_ptr<BakingContext>) { /* Stub */ }')
    global_lines.append('')
    
    # Engine properties
    global_lines.append('// Engine property indices')
    global_lines.append('EntityProperties::EntityProperties(Properties& props) noexcept : _propsRef(&props) { }')
    common_parsed: set[str] = set()
    for entity in game_entities:
        index = 1
        for prop_tag in codegen_tags['ExportProperty']:
            if prop_tag.entity == entity:
                if 'SharedProperty' not in prop_tag.flags:
                    global_lines.append('uint16 ' + entity + 'Properties::' + prop_tag.name + '_RegIndex = ' + str(index) + ';')
                elif prop_tag.name not in common_parsed:
                    global_lines.append('uint16 EntityProperties::' + prop_tag.name + '_RegIndex = ' + str(index) + ';')
                    common_parsed.add(prop_tag.name)
                index += 1
    global_lines.append('')
    
    # Settings list
    append_settings_getter(global_lines, 'Server')
    append_settings_getter(global_lines, 'Client')
    
    generated_output.create_file('GenericCode-Common.cpp', args.genoutput)
    generated_output.write_codegen_template('GenericCode')

    generated_output.insert_codegen_lines(global_lines, 'Body')

def run_generic_codegen() -> None:
    run_codegen_step(generate_generic_code, 'Code generation for generic code failed')


def get_allowed_registration_targets(target: str) -> list[str]:
    return ['Common', target] + (['Client'] if target == 'Mapper' else [])


def entity_allowed(entity: str, target: str) -> bool:
    if target == 'Server':
        return game_entities_info[entity].server is not None
    if target in CLIENT_ENTITY_TARGETS:
        return game_entities_info[entity].client is not None
    assert False, target


def append_ref_call_registration(register_lines: list[str], method_name: str, call: str, is_stub: bool) -> None:
    register_lines.append('    MethodDesc{ .Name = "' + method_name + '", ' +
            '.Call = [](FuncCallData& call) {' + (' ignore_unused(call); } },' if is_stub else ''))
    if not is_stub:
        register_lines.append('        struct Wrapped { ' + call + ' };')
        register_lines.append('        NativeDataCaller::NativeCall<&Wrapped::Call>(call);')
        register_lines.append('    } },')


def append_ref_method_registration(
    register_lines: list[str],
    ref_type_tag: ExportRefTypeTag,
    method_name: str,
    ret: str,
    params: list[MethodArg],
    is_stub: bool,
    getter: bool = False,
    setter: bool = False,
) -> None:
    register_lines.append('    MethodDesc{ .Name = "' + method_name + '", ' +
        '.Args = {' + ', '.join('{"' + p.name + '", meta->ResolveComplexType("' + meta_type_to_unified_type(p.arg_type) + '")}' for p in params) + '}, ' +
        '.Ret = ' + ('meta->ResolveComplexType("' + meta_type_to_unified_type(ret) + '")' if ret != 'void' else '{' + '}') + ', ' +
            '.Call = [](FuncCallData& call) {' + (' ignore_unused(call); }' if is_stub else ''))
    if not is_stub:
        is_property = getter or setter
        register_lines.append('        FO_STACK_TRACE_ENTRY_NAMED("' + ref_type_tag.name + '::' + method_name +
                (' (Getter)' if getter else '') + (' (Setter)' if setter else '') + '");')
        register_lines.append('        struct Wrapped { static ' +
                ('auto' if ret != 'void' else 'void') + ' Call(' + ref_type_tag.name + '* self' + (', ' if params else '') +
        ', '.join([meta_type_to_engine_type(p.arg_type, ref_type_tag.target, True) + ' ' + p.name for p in params]) + ') ' +
            ('-> ' + meta_type_to_engine_type(ret, ref_type_tag.target, False) if ret != 'void' else '') +
                ' { ' + ('return ' if ret != 'void' else '') + 'self->' + method_name +
        ('(' if not is_property else ' = ' if setter else '') + ', '.join([p.name for p in params]) + (')' if not is_property else '') + '; } };')
        register_lines.append('        NativeDataCaller::NativeCall<&Wrapped::Call>(call);')
        register_lines.append('    }' + (', .Getter = true' if getter else '') + (', .Setter = true' if setter else '') + ' },')
    else:
        register_lines[-1] += (', .Getter = true' if getter else '') + (', .Setter = true' if setter else '') + ' },'


def calc_unique_zone_name(zone_names: list[str], name: str) -> str:
    count = zone_names.count(name)
    zone_names.append(name)
    return name if count == 0 else name + '_' + str(count + 1)


def sanitize_cpp_identifier(name: str) -> str:
    result = ''.join(char if char.isalnum() else '_' for char in name)
    if not result:
        return 'Generated'
    if result[0].isdigit():
        return '_' + result
    return result


def make_unique_cpp_identifier(used_names: set[str], prefix: str, name: str) -> str:
    base_name = prefix + sanitize_cpp_identifier(name)
    unique_name = base_name
    suffix = 2

    while unique_name in used_names:
        unique_name = base_name + '_' + str(suffix)
        suffix += 1

    used_names.add(unique_name)
    return unique_name


def append_static_function(lines: list[str], signature: str, body_lines: list[str], add_break: bool = False) -> None:
    lines.append(signature)
    lines.append('{')
    for body_line in body_lines:
        lines.append(('    ' + body_line) if body_line else '')
    lines.append('}')
    lines.append('')


def append_enum_registration(helper_lines: list[str], register_lines: list[str]) -> None:
    used_names: set[str] = set()

    for enum_tag in codegen_tags['ExportEnum']:
        function_name = make_unique_cpp_identifier(used_names, 'RegisterEnum_', enum_tag.group_name)
        body_lines = ['meta->RegisterEnumGroup("' + enum_tag.group_name + '", "' + enum_tag.underlying_type + '",', '{']
        for key_value in enum_tag.key_values:
            body_lines.append('    {"' + key_value.key + '", ' + require_enum_value_text(key_value) + '},')
        body_lines.append('});')

        append_static_function(helper_lines, 'static void ' + function_name + '(EngineMetadata* meta)', body_lines)
        register_lines.append(function_name + '(meta);')

    if codegen_tags['ExportEnum']:
        register_lines.append('')


def append_value_type_registration(helper_lines: list[str], register_lines: list[str]) -> None:
    if not codegen_tags['ExportValueType']:
        return

    body_lines: list[str] = []

    for value_type_tag in codegen_tags['ExportValueType']:
        body_lines.append('meta->RegisterValueType("' + value_type_tag.name + '");')

    body_lines.append('')

    for value_type_tag in codegen_tags['ExportValueType']:
        body_lines.append('meta->RegisterValueTypeLayout("' + value_type_tag.name + '", { {')
        for layout_entry in ''.join(value_type_tag.flags[value_type_tag.flags.index('Layout') + 2:]).split('+'):
            field_type, field_name = layout_entry.split('-')
            body_lines.append('    {string_view("' + field_name + '"), string_view("' + field_type + '")},')
        body_lines.append('} });')
        body_lines.append('')

    append_static_function(helper_lines, 'static void RegisterValueTypes(EngineMetadata* meta)', body_lines)
    register_lines.append('RegisterValueTypes(meta);')
    register_lines.append('')


def append_ref_type_registration(helper_lines: list[str], register_lines: list[str], target: str, is_stub: bool) -> None:
    allowed_targets = get_allowed_registration_targets(target)
    used_names: set[str] = set()

    for ref_type_tag in codegen_tags['ExportRefType']:
        if ref_type_tag.target not in allowed_targets:
            continue

        function_name = make_unique_cpp_identifier(used_names, 'RegisterRefType_', ref_type_tag.name)
        body_lines = ['meta->RegisterRefType("' + ref_type_tag.name + '");',
                '',
                'meta->RegisterRefTypeMethods("' + ref_type_tag.name + '", {']

        if 'RefCounted' in ref_type_tag.flags:
            append_ref_call_registration(body_lines, '__AddRef', 'static void Call(' + ref_type_tag.name + '* self) { self->AddRef(); }', is_stub)
            append_ref_call_registration(body_lines, '__Release', 'static void Call(' + ref_type_tag.name + '* self) { self->Release(); }', is_stub)

        if 'HasFactory' in ref_type_tag.flags:
            body_lines.append('    MethodDesc{ .Name = "__Factory", ' +
                    '.Ret = meta->ResolveComplexType("' + ref_type_tag.name + '"), .Call = [](FuncCallData& call) {' +
                    (' ignore_unused(call); } },' if is_stub else ''))
            if not is_stub:
                body_lines.append('        FO_STACK_TRACE_ENTRY_NAMED("' + ref_type_tag.name + '::__Factory");')
                body_lines.append('        struct Wrapped { ' + 'static auto Call() -> ' + ref_type_tag.name + '* ' +
                        '{ return SafeAlloc::MakeRefCounted<' + ref_type_tag.name + '>().release_ownership(); }' + ' };')
                body_lines.append('        NativeDataCaller::NativeCall<&Wrapped::Call>(call);')
                body_lines.append('    } },')

        for field in ref_type_tag.fields:
            append_ref_method_registration(body_lines, ref_type_tag, field.name, field.field_type, [], is_stub, getter=True)
            append_ref_method_registration(body_lines, ref_type_tag, field.name, 'void', [MethodArg(field.field_type, 'value')], is_stub, setter=True)
        for method in ref_type_tag.methods:
            append_ref_method_registration(body_lines, ref_type_tag, method.name, method.ret, method.args, is_stub)
        body_lines.append('});')

        append_static_function(helper_lines, 'static void ' + function_name + '(EngineMetadata* meta)', body_lines)
        register_lines.append(function_name + '(meta);')

    if any(ref_type_tag.target in allowed_targets for ref_type_tag in codegen_tags['ExportRefType']):
        register_lines.append('')


def append_entity_type_registration(register_lines: list[str], target: str) -> None:
    register_lines.append('// Entity types')
    register_lines.append('unordered_map<string, PropertyRegistrator*> registrators;')
    for entity in game_entities:
        if not entity_allowed(entity, target):
            continue
        entity_info = game_entities_info[entity]
        register_lines.append('registrators["' + entity + '"] = meta->RegisterEntityType("' + entity + '", ' +
                cpp_bool(entity_info.exported) + ', ' +
                cpp_bool(entity_info.is_global) + ', ' +
                cpp_bool(entity_info.has_protos) + ', ' +
                cpp_bool(entity_info.has_statics) + ', ' +
                cpp_bool(entity_info.has_abstract) + ');')
    register_lines.append('')


def get_register_flags(value_type: str, name: str, access: str, base_flags: list[str]) -> list[str]:
    return [access, meta_type_to_unified_type(value_type), name] + base_flags


def append_property_registration(helper_lines: list[str], register_lines: list[str], target: str) -> None:
    used_names: set[str] = set()

    for entity in game_entities:
        if not entity_allowed(entity, target):
            continue

        body_lines: list[str] = []
        for prop_tag in codegen_tags['ExportProperty']:
            if prop_tag.entity == entity:
                body_lines.append('registrator->RegisterProperty({' + ', '.join(['"' + register_flag + '"' for register_flag in get_register_flags(prop_tag.property_type, prop_tag.name, prop_tag.access, prop_tag.flags)]) + '});')

        if not body_lines:
            continue

        function_name = make_unique_cpp_identifier(used_names, 'RegisterProperties_', entity)
        append_static_function(helper_lines, 'static void ' + function_name + '(PropertyRegistrator* registrator)', body_lines)
        register_lines.append(function_name + '(registrators["' + entity + '"]);')

    register_lines.append('')


def append_method_registration(extern_lines: list[str], helper_lines: list[str], register_lines: list[str], target: str, is_stub: bool) -> None:
    allowed_targets = get_allowed_registration_targets(target)
    used_names: set[str] = set()
    method_chunk_size = 20

    for entity in game_entities:
        zone_names: list[str] = []
        body_lines = ['vector<MethodDesc> methods;',
                'methods.reserve(256);']
        has_methods = False
        method_blocks: list[list[str]] = []

        for method_tag in codegen_tags['ExportMethod']:
            if method_tag.target not in allowed_targets or (method_tag.entity != entity and method_tag.entity != 'Entity'):
                continue
            if 'GlobalGetter' in method_tag.flags:
                assert method_tag.entity == 'Game', 'GlobalGetter export flag can be used only on Game methods'
                assert not method_tag.args, 'GlobalGetter export flag requires a method without parameters'
            if 'TimeEventRelated' in method_tag.flags and not game_entities_info[entity].has_time_events:
                continue

            has_methods = True
            registration_info = resolve_method_registration_info(entity, method_tag, target)
            if not is_stub:
                extern_lines.append('extern ' + registration_info.return_type + ' ' + registration_info.function_name + '(' + registration_info.engine_entity_type_extern + (', ' if method_tag.args else '') +
                    ', '.join([meta_type_to_engine_type(p.arg_type, method_tag.target, True, self_entity='Entity') for p in method_tag.args]) + ');')

            method_body_lines = ['methods.emplace_back(MethodDesc{ .Name = "' + method_tag.name + '", ' +
                    '.Args = {' + ', '.join('{"' + p.name + '", meta->ResolveComplexType("' + meta_type_to_unified_type(p.arg_type, self_entity=entity) + '")}' for p in method_tag.args) + '}, ' +
                    '.Ret = ' + ('meta->ResolveComplexType("' + meta_type_to_unified_type(method_tag.ret, self_entity=entity) + '")' if method_tag.ret != 'void' else '{' + '}') + ', ' +
                    '.Call = [](FuncCallData& call) {']
            if not is_stub:
                method_body_lines.append('    FO_STACK_TRACE_ENTRY_NAMED("' + calc_unique_zone_name(zone_names, registration_info.function_name) + '");')
                method_body_lines.append('    NativeDataCaller::NativeCall<static_cast<' + registration_info.return_type + '(*)(' + registration_info.engine_entity_type_extern + (', ' if method_tag.args else '') +
                    ', '.join([meta_type_to_engine_type(p.arg_type, method_tag.target, True, self_entity='Entity') for p in method_tag.args]) + ')>(&' + registration_info.function_name + ')>(call);')
            else:
                method_body_lines.append('    ignore_unused(call);')
            method_body_lines.append('}' +
                    (', .GlobalGetter = true' if 'GlobalGetter' in method_tag.flags else '') +
                    (', .Getter = true' if 'Getter' in method_tag.flags or 'GlobalGetter' in method_tag.flags else '') +
                    (', .Setter = true' if 'Setter' in method_tag.flags else '') +
                    (', .PassOwnership = true' if 'PassOwnership' in method_tag.flags else '') +
                    ' });')
            method_blocks.append(method_body_lines)

        if not has_methods:
            continue

        chunk_function_names: list[str] = []
        for chunk_index in range(0, len(method_blocks), method_chunk_size):
            chunk_methods = method_blocks[chunk_index:chunk_index + method_chunk_size]
            chunk_body_lines: list[str] = []
            for method_block in chunk_methods:
                chunk_body_lines.extend(method_block)

            chunk_function_name = make_unique_cpp_identifier(used_names, 'AddMethods_' + entity + '_', 'Chunk_' + str(chunk_index // method_chunk_size + 1))
            append_static_function(helper_lines, 'static void ' + chunk_function_name + '(vector<MethodDesc>& methods, EngineMetadata* meta)', chunk_body_lines)
            chunk_function_names.append(chunk_function_name)

        for chunk_function_name in chunk_function_names:
            body_lines.append(chunk_function_name + '(methods, meta);')
        body_lines.append('meta->RegisterEntityMethods("' + entity + '", std::move(methods));')
        function_name = make_unique_cpp_identifier(used_names, 'RegisterMethods_', entity)
        append_static_function(helper_lines, 'static void ' + function_name + '(EngineMetadata* meta)', body_lines)
        register_lines.append(function_name + '(meta);')

    register_lines.append('')


def append_event_registration(helper_lines: list[str], register_lines: list[str], target: str) -> None:
    allowed_targets = get_allowed_registration_targets(target)
    used_names: set[str] = set()

    for entity in game_entities:
        body_lines: list[str] = []

        for event_tag in codegen_tags['ExportEvent']:
            if event_tag.target in allowed_targets and event_tag.entity == entity:
                resolved_args = ', '.join('{"' + p.name + '", meta->ResolveComplexType("' + meta_type_to_unified_type(p.arg_type, self_entity=entity) + '")}' for p in event_tag.args)
                body_lines.append('meta->RegisterEntityEvent("' + entity + '", EntityEventDesc { .Name = "' + event_tag.name + '", ' +
                        '.Args = {' + resolved_args + '}, .Exported = true, .Deferred = false });')

        if not body_lines:
            continue

        function_name = make_unique_cpp_identifier(used_names, 'RegisterEvents_', entity)
        append_static_function(helper_lines, 'static void ' + function_name + '(EngineMetadata* meta)', body_lines)
        register_lines.append(function_name + '(meta);')

    register_lines.append('')


def append_migration_rule_registration(helper_lines: list[str], register_lines: list[str]) -> None:
    if not codegen_tags['MigrationRule']:
        return

    body_lines = ['const auto to_hstring = [&](string_view str) -> hstring { return meta->Hashes.ToHashedString(str); };', '', 'meta->RegisterMigrationRules({']
    for source_type in sorted(set(rule_tag.args[0] for rule_tag in codegen_tags['MigrationRule'])):
        body_lines.append('    {')
        body_lines.append('        to_hstring("' + source_type + '"), {')
        for source_name in sorted(set(rule_tag.args[1] for rule_tag in codegen_tags['MigrationRule'] if rule_tag.args[0] == source_type)):
            body_lines.append('            {')
            body_lines.append('                to_hstring("' + source_name + '"), {')
            for rule_tag in codegen_tags['MigrationRule']:
                if rule_tag.args[0] == source_type and rule_tag.args[1] == source_name:
                    body_lines.append('                    {to_hstring("' + rule_tag.args[2] + '"), to_hstring("' + rule_tag.args[3] + '")},')
            body_lines.append('                },')
            body_lines.append('            },')
        body_lines.append('        },')
        body_lines.append('    },')
    body_lines.append('});')

    append_static_function(helper_lines, 'static void RegisterMigrationRulesSection(EngineMetadata* meta)', body_lines)
    register_lines.append('RegisterMigrationRulesSection(meta);')
    register_lines.append('')


def get_registration_define_lines(target: str, is_stub: bool) -> list[str]:
    if target == 'Server':
        return ['#define SERVER_REGISTRATION 1', '#define CLIENT_REGISTRATION 0',
                '#define MAPPER_REGISTRATION 0', '#define STUB_MODE ' + ('1' if is_stub else '0')]
    if target == 'Client':
        return ['#define SERVER_REGISTRATION 0', '#define CLIENT_REGISTRATION 1',
                '#define MAPPER_REGISTRATION 0', '#define STUB_MODE ' + ('1' if is_stub else '0')]
    assert target == 'Mapper', target
    return ['#define SERVER_REGISTRATION 0', '#define CLIENT_REGISTRATION 0',
            '#define MAPPER_REGISTRATION 1', '#define STUB_MODE ' + ('1' if is_stub else '0')]


def build_common_header_include_lines() -> list[str]:
    return ['#include "' + common_header + '"' for common_header in args.commonheader]

def generate_metadata_registration(target: str, is_stub: bool) -> None:
    extern_lines: list[str] = []
    helper_lines: list[str] = []
    register_lines: list[str] = []

    append_enum_registration(helper_lines, register_lines)
    append_value_type_registration(helper_lines, register_lines)
    append_ref_type_registration(helper_lines, register_lines, target, is_stub)
    append_entity_type_registration(register_lines, target)
    append_property_registration(helper_lines, register_lines, target)
    append_method_registration(extern_lines, helper_lines, register_lines, target, is_stub)
    append_event_registration(helper_lines, register_lines, target)
    append_migration_rule_registration(helper_lines, register_lines)
    include_lines = build_common_header_include_lines()
    
    generated_output.create_file('MetadataRegistration-' + target + ('Stub' if is_stub else '') + '.cpp', args.genoutput)
    generated_output.write_codegen_template('MetadataRegistration')
    generated_output.insert_codegen_lines(register_lines, 'Register')
    generated_output.insert_codegen_lines(helper_lines, 'RegisterHelpers')
    generated_output.insert_codegen_lines(extern_lines, 'Global')
    generated_output.insert_codegen_lines(include_lines, 'Includes')
    generated_output.insert_codegen_lines(get_registration_define_lines(target, is_stub), 'Defines')

def run_metadata_registration_codegen() -> None:
    def generate_all_metadata_registration() -> None:
        for target in REGISTRATION_TARGETS:
            for is_stub in (False, True):
                generate_metadata_registration(target, is_stub)

    run_codegen_step(generate_all_metadata_registration, 'Code generation for data registration failed')

def write_embedded_resources() -> None:
    def write_embedded_resources_impl() -> None:
        capacity = int(args.embedded)
        assert capacity >= 10000, 'Embedded capacity must be greater than or equal to 10000'
        assert capacity % 10000 == 0, 'Embedded capacity must be divisible by 10000'
        generated_output.create_file('EmbeddedResources-Include.h', args.genoutput)
        generated_output.write_line('alignas(uint32_t) volatile const uint8_t EMBEDDED_RESOURCES[' + str(capacity) + '] = {' + ','.join([str((i + 42) % 200) for i in range(capacity)]) + '};')

    run_codegen_step(write_embedded_resources_impl, 'Can\'t write embedded resources')


def try_get_git_branch() -> str:
    try:
        return subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD'], text=True).strip()
    except Exception:
        return ''


def write_version_info() -> None:
    def write_version_info_impl() -> None:
        generated_output.create_file('Version-Include.h', args.genoutput)
        generated_output.write_line('static constexpr string_view_nt FO_BUILD_HASH = "' + args.buildhash + '";')
        generated_output.write_line('static constexpr string_view_nt FO_DEV_NAME = "' + args.devname + '";')
        generated_output.write_line('static constexpr string_view_nt FO_NICE_NAME = "' + args.nicename + '";')

        compatibility_version = compatibility_hasher.hexdigest()[:16]
        generated_output.write_line('static constexpr string_view_nt FO_COMPATIBILITY_VERSION = "' + compatibility_version + '";')
        log('Compatibility version: ' + compatibility_version)
        generated_output.write_line('static constexpr string_view_nt FO_GIT_BRANCH = "' + try_get_git_branch() + '";')

    run_codegen_step(write_version_info_impl, 'Can\'t write version info')


def flush_generated_files() -> None:
    run_codegen_step(generated_output.flush, 'Can\'t flush generated files')


def run_codegen() -> None:
    parse_meta_files()
    parse_all_tags()
    run_generic_codegen()
    run_metadata_registration_codegen()
    write_embedded_resources()
    write_version_info()
    flush_generated_files()


def main() -> None:
    global args
    global start_time

    args = create_parser().parse_args()
    start_time = time.time()
    log('Start code generation')
    run_codegen()
    elapsed_time = time.time() - start_time
    log('Code generation complete in ' + '{:.2f}'.format(elapsed_time) + ' seconds')


if __name__ == '__main__':
    main()
