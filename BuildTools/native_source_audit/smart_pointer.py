"""Smart-pointer vocabulary audit.

Checks stable textual markers of the native pointer contract: Engine ``ptr``/``nptr`` borrows, ``unique_*`` and
``refcount_*`` owners, no ``std::`` owners, nullable checks before dereference, and raw-pointer boundaries that
are recognized by their shape.  The audit does not replace compilation; ``smart-pointer-ast`` is the AST-backed
companion for newly added raw declarations.

Rules keyed by a policy key (``Source/...``) apply to Engine files only.  Project files (every file outside the
Engine ``Source`` directory) follow the strict rules and the split ``FO_NAMESPACE`` script implementation rule.
"""

from __future__ import annotations

import json
import re
from collections import Counter
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass, field
from itertools import repeat
from pathlib import Path

from native_source_audit.common import PROJECT_KEY_PREFIX, AuditContext, AuditUsageError, Finding


NAME = 'smart-pointer'
DEFAULT_SUMMARY_HOTSPOT_LIMIT = 10
HEADER_EXTS = {'.h', '.hh', '.hpp', '.hxx', '.inl'}
PROPERTY_SERIALIZER_PATH = 'Source/Common/PropertiesSerializer.cpp'
PROPERTY_SERIALIZER_RAW_CURSOR_METRIC = 'property_serializer_raw_cursor_marker'
PROPERTY_SERIALIZER_INLINE_BYTE_EDGE_METRIC = 'property_serializer_inline_byte_edge_marker'
DATA_SERIALIZATION_PATH = 'Source/Essentials/DataSerialization.h'
DATA_SERIALIZATION_TEST_PATH = 'Source/Tests/Test_DataSerialization.cpp'
DATA_SERIALIZATION_INLINE_READ_EDGE_METRIC = 'data_serialization_inline_read_edge_marker'
DATA_SERIALIZATION_INLINE_WRITE_EDGE_METRIC = 'data_serialization_inline_write_edge_marker'
DATA_WRITER_WRITE_PTR_PRODUCTION_CALL_METRIC = 'data_writer_write_ptr_production_call_marker'
DATA_WRITER_WRITE_PTR_TEST_PAYLOAD_CALL_METRIC = 'data_writer_write_ptr_test_payload_call_marker'
DATA_READER_READ_PTR_PRODUCTION_CALL_METRIC = 'data_reader_read_ptr_production_call_marker'
DATA_READER_READ_PTR_NON_API_TEST_CALL_METRIC = 'data_reader_read_ptr_non_api_test_call_marker'
FILE_READER_COPY_DATA_PAYLOAD_CALL_METRIC = 'file_reader_copy_data_payload_call_marker'
INTERNAL_REFCOUNT_FACTORY_FILES = {
    'Source/Essentials/SmartPointers.h',
}
OPTIONAL_REFCOUNT_PTR_ALLOWED_FILES = {
    'Source/Server/EntityManager.h',
    'Source/Server/EntityManager.cpp',
    'Source/Server/Server.cpp',
}
DIRECT_UNIQUE_OWNER_WRAPPER_GET_ALLOWED_FILES = {
    'Source/Essentials/CommonHelpers.h',
    'Source/Essentials/SmartPointers.h',
    'Source/Tests/Test_SmartPointers.cpp',
}
DIRECT_STD_UNIQUE_PTR_ALLOWED_FILES = {
    'Source/Essentials/SmartPointers.h',
    'Source/Essentials/MemorySystem.cpp',
    # PtrRule: DirectStdUniquePtr — Essentials signal handler alt-stack: the engine allocator
    # is unavailable during signal handling, so a raw std::unique_ptr<uint8_t[]> is required
    'Source/Essentials/Posix.cpp',
}
DIRECT_STD_SHARED_PTR_ALLOWED_FILES = {
    'Source/Essentials/MemorySystem.h',
    'Source/Essentials/SmartPointers.h',
    'Source/Essentials/StackTrace.h',
    'Source/Essentials/StackTrace.cpp',
    'Source/Tests/Test_SmartPointers.cpp',
    # PtrRule: DirectStdSharedPtr — these exercise the Essentials StackTrace whose
    # ScriptLayers field is an intentional std::shared_ptr; assigning it requires
    # std::make_shared, which is permitted for Essentials per AGENTS.md
    'Source/Tests/Test_StackTrace.cpp',
    'Source/Tests/Test_ExceptionHandling.cpp',
}
STRICT_NULL_STYLE_PREFIXES = (
    'Source/Essentials/',
    'Source/Common/',
    'Source/Client/',
    'Source/Server/',
    'Source/Tools/',
    PROJECT_KEY_PREFIX,
)
STRICT_WRAPPER_NULL_STYLE_PREFIXES = STRICT_NULL_STYLE_PREFIXES + (
    'Source/Applications/',
    'Source/Frontend/',
    'Source/Scripting/',
)

OLD_RAW_PTR_RE = re.compile(r'(?<![A-Za-z0-9])(?:raw_ptr|nullable_raw_ptr)(?![A-Za-z0-9])')
DIRECT_STD_UNIQUE_PTR_RE = re.compile(r'\bstd\s*::\s*unique_ptr\s*<|\bstd\s*::\s*make_unique\s*(?:<|\()')
DIRECT_STD_SHARED_PTR_RE = re.compile(r'\bstd\s*::\s*(?:shared_ptr|weak_ptr)\s*<|\bstd\s*::\s*make_shared\s*(?:<|\()')
DIRECT_UNIQUE_OWNER_NEW_RE = re.compile(
    r'\b(?:unique_ptr|unique_nptr|unique_del_ptr|unique_del_nptr)\s*<[^;\n]+>\s+'
    r'[A-Za-z_][A-Za-z0-9_]*\s*(?:\{|=\s*)\s*new\b'
)
DIRECT_UNIQUE_OWNER_WRAPPER_GET_RE = re.compile(
    r'(?:\b(?:const\s+)?(?:unique_ptr|unique_nptr|unique_del_ptr|unique_del_nptr)\s*<[^;\n]+>\s+'
    r'[A-Za-z_][A-Za-z0-9_]*\s*\{\s*[A-Za-z_][A-Za-z0-9_:<>]*(?:\.[A-Za-z_][A-Za-z0-9_:<>]*)*\.\s*get\s*\(\s*\)'
    r'|\breturn\s+(?:unique_ptr|unique_nptr|unique_del_ptr|unique_del_nptr)\s*<[^;\n]+>\s*(?:\{|\()\s*'
    r'[A-Za-z_][A-Za-z0-9_:<>]*(?:\.[A-Za-z_][A-Za-z0-9_:<>]*)*\.\s*get\s*\(\s*\))'
)
IGNORED_RELEASE_RE = re.compile(r'\(void\)\s*[^;\n]+?\.\s*release\s*\(\s*\)')
OPTIONAL_UNIQUE_DEL_PTR_RE = re.compile(r'\boptional\s*<\s*unique_del_ptr\s*<')
UNIQUE_DEL_PTR_START_RE = re.compile(r'\bunique_del_ptr\s*<')
EMPTY_UNIQUE_DEL_DECL_RE = re.compile(
    r'\s+[A-Za-z_][A-Za-z0-9_]*\s*'
    r'(?:\{\s*(?:\}|nullptr\b)|=\s*\{\s*(?:\}|nullptr\b)|=\s*nullptr\b)'
)
NULL_UNIQUE_DEL_TEMP_RE = re.compile(r'\s*(?:\{|\()\s*nullptr\b')
# Nullable C factories of the Engine's own dependencies; a project adds its libraries through configuration
NULLABLE_FACTORY_NAMES = r'SDL_(?:Get|Open|Convert)[A-Za-z0-9_]*|asCreateScriptEngine|bson_as_[A-Za-z0-9_]*|BCON_NEW'


def build_nullable_factory_unique_del_ptr_re(factory_names: str) -> re.Pattern[str]:
    return re.compile(
        r'\bunique_del_ptr\s*<[^;\n]+>\s+[A-Za-z_][A-Za-z0-9_]*\s*\{\s*'
        rf'(?:{factory_names})\b'
        r'|\breturn\s+unique_del_ptr\s*<[^;\n]+>\s*(?:\{|\()\s*'
        rf'(?:{factory_names})\b'
    )


DIRECT_NULLABLE_FACTORY_UNIQUE_DEL_PTR_RE = build_nullable_factory_unique_del_ptr_re(NULLABLE_FACTORY_NAMES)
RETURN_UNIQUE_DEL_PTR_FROM_NULLABLE_LOCAL_RE = re.compile(r'\breturn\s+make_unique_del_ptr\s*\(\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*,')
RAW_LOCAL_WRAPPER_GET_ASSIGN_RE = re.compile(
    r'\b(?:(?:const|volatile)\s+)*'
    r'(?:auto|[A-Za-z_][A-Za-z0-9_:<>]*)\s*\*+\s*(?:const\s+)?'
    r'(?!(?:override|final)\b)[A-Za-z_][A-Za-z0-9_]*\s*(?:=|\{)\s*[^;\n]*'
    r'\.\s*get\s*\(\s*\)\s*\}?\s*;'
)
DIRECT_OWNER_RESET_WRAPPER_GET_RE = re.compile(r'\.\s*reset\s*\([^;\n]*\.\s*get\s*\(\s*\)\s*\)')
DIRECT_DELETE_WRAPPER_GET_RE = re.compile(r'\bdelete\s*(?:\[\s*\])?\s+[^;\n]*\.\s*(?:get|get_no_const)\s*\(\s*\)')
DIRECT_WRAPPER_GET_INDEX_RE = re.compile(r'\.\s*get\s*\(\s*\)\s*\[')
DIRECT_WRAPPER_GET_ARITHMETIC_RE = re.compile(r'\.\s*get\s*\(\s*\)\s*(?:\+|-)')
DIRECT_WRAPPER_GET_COMPARISON_RE = re.compile(
    r'(?:\.\s*get\s*\(\s*\)\s*(?:==|!=|<|>|<=|>=)\s*(?:[^;\n]*\.\s*get\s*\(\s*\)|nullptr))'
    r'|(?:nullptr\s*(?:==|!=)\s*[^;\n]*\.\s*get\s*\(\s*\))'
)
DIRECT_SCRIPT_WRAPPER_RETURN_GET_RE = re.compile(r'\breturn\s+[^;\n]+?\.\s*get\s*\(\s*\)\s*;')
DIRECT_SCRIPT_WRAPPER_RETURN_GET_FILES = {
    'Source/Scripting/AngelScript/AngelScriptArray.cpp',
    'Source/Scripting/AngelScript/AngelScriptCall.cpp',
    'Source/Scripting/AngelScript/AngelScriptDict.cpp',
    'Source/Scripting/AngelScript/AngelScriptEntity.cpp',
    'Source/Scripting/AngelScript/AngelScriptHelpers.h',
    'Source/Scripting/AngelScript/AngelScriptString.cpp',
    'Source/Scripting/ClientCritterScriptMethods.cpp',
    'Source/Scripting/ClientGlobalScriptMethods.cpp',
    'Source/Scripting/ClientItemScriptMethods.cpp',
    'Source/Scripting/ClientMapScriptMethods.cpp',
    'Source/Scripting/CommonImGuiScriptMethods.cpp',
    'Source/Scripting/MapperGlobalScriptMethods.cpp',
    'Source/Scripting/ServerCritterScriptMethods.cpp',
    'Source/Scripting/ServerGlobalScriptMethods.cpp',
    'Source/Scripting/ServerItemScriptMethods.cpp',
    'Source/Scripting/ServerLocationScriptMethods.cpp',
    'Source/Scripting/ServerMapScriptMethods.cpp',
    'Source/Scripting/ServerPlayerScriptMethods.cpp',
}
DIRECT_WRAPPER_RETURN_GET_FILES = DIRECT_SCRIPT_WRAPPER_RETURN_GET_FILES | {
    'Source/Client/Client.cpp',
    'Source/Client/VisualParticles.cpp',
    'Source/Common/DataSource.cpp',
    'Source/Common/EngineBase.cpp',
    'Source/Common/ImGuiExt/ImGuiStuff.cpp',
    'Source/Common/WebRelated.cpp',
    'Source/Frontend/Application.cpp',
    'Source/Tests/Test_EntityLifecycle.cpp',
    'Source/Tests/Test_ServerEngine.cpp',
    'Source/Tests/Test_ServerItems.cpp',
    'Source/Tools/Mapper.cpp',
    'Source/Tools/ModelMeshBaker.cpp',
}
DIRECT_WRAPPER_GET_COMPARISON_ALLOWED_FILES = {
    'Source/Essentials/SmartPointers.h',
    'Source/Tests/Test_SmartPointers.cpp',
}
DEFAULT_EMPTY_NONNULL_RE = re.compile(
    r'^\s*(?:(?:mutable|static|inline|extern|const)\s+)*'
    r'(?P<kind>ptr|unique_ptr|refcount_ptr)\s*<[^;\n]+?>\s+'
    r'[A-Za-z_][A-Za-z0-9_]*\s*'
    r'(?:\{\s*\}|=\s*\{\s*\}|=\s*nullptr|\{\s*nullptr\s*\})\s*;'
)
PLAIN_DEFAULT_NONNULL_RE = re.compile(
    r'^\s*(?:(?:mutable|const)\s+)*'
    r'(?P<kind>ptr|unique_ptr|refcount_ptr)\s*<[^;\n]+?>\s+'
    r'[A-Za-z_][A-Za-z0-9_]*\s*;'
)
REFERENCE_MEMBER_RE = re.compile(
    r'^\s*(?:(?:mutable|const|volatile)\s+)*'
    r'[A-Za-z_][A-Za-z0-9_:<>,\s]*?'
    r'\s*&\s*'
    r'[A-Za-z_][A-Za-z0-9_]*\s*'
    r'(?:\{[^;\n]*\}|=\s*[^;\n]+)?\s*;'
)
LOCAL_NONNULL_DECL_RE = re.compile(
    r'(?<![A-Za-z0-9_:<>])(?:(?:const)\s+)*'
    r'(?P<kind>ptr|unique_ptr|refcount_ptr)\s*<[^;\n]+?>\s+'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)\b'
)
LOCAL_NULLABLE_DECL_RE = re.compile(
    r'(?<![A-Za-z0-9_:<>])(?:(?:const)\s+)*'
    r'(?P<kind>nptr|unique_nptr|unique_del_nptr|refcount_nptr)\s*<[^;\n]+?>\s+'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)\b'
)
NONNULL_PARAM_RE = re.compile(
    r'^\s*(?:(?:const|mutable)\s+)*'
    r'(?P<kind>ptr|unique_ptr|refcount_ptr)\s*<.+>\s+'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=.*)?$'
)
RAW_LOCAL_DECL_RE = re.compile(
    r'\b(?:(?:const|volatile)\s+)*'
    r'(?:auto|[A-Za-z_][A-Za-z0-9_:<>]*)\s*\*+\s*(?:const\s+)?'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)\b'
)
NONNULL_SMART_RETURN_RE = re.compile(r'->\s*(?P<kind>ptr|unique_ptr|refcount_ptr)\s*<')
EMPTY_NONNULL_RETURN_RE = re.compile(r'\breturn\s*(?:nullptr|\{\s*\})\s*;')
DIRECT_REFCOUNT_CTOR_RE = re.compile(r'(?<![A-Za-z0-9_:<>])refcount_(?:n?ptr)\s*<[^<>\n;]+>\s*\(')
DIRECT_REFCOUNT_BRACE_RE = re.compile(r'(?<![A-Za-z0-9_:<>])refcount_(?:n?ptr)\s*<[^<>\n;]+>\s*\{\s*[^}\s]')
CTAD_REFCOUNT_CTOR_RE = re.compile(r'(?<![A-Za-z0-9_:<>])refcount_ptr\s*\((?!\s*\))')
# A lambda/function trailing return type `-> refcount_nptr<...> {` spells a type, not a construction
LAMBDA_TRAILING_REFCOUNT_RE = re.compile(r'->\s*refcount_(?:n?ptr)\s*<[^<>\n;]+>\s*\{')


def has_direct_refcount_construction(text: str) -> bool:
    cleaned = LAMBDA_TRAILING_REFCOUNT_RE.sub('', text)
    return bool(DIRECT_REFCOUNT_CTOR_RE.search(cleaned) or DIRECT_REFCOUNT_BRACE_RE.search(cleaned) or CTAD_REFCOUNT_CTOR_RE.search(cleaned))
DIRECT_REFCOUNT_ADOPT_RE = re.compile(r'(?<![A-Za-z0-9_:<>])refcount_(?:n?ptr)\s*(?:<[^<>\n;]+>)?::adopt\b')
CTAD_REFCOUNT_ADOPT_RE = re.compile(r'\brefcount_ptr\s*\([^;\n]*::adopt\b')
OPTIONAL_SMART_POINTER_RE = re.compile(r'\boptional\s*<\s*(?:ptr|nptr|unique_ptr|unique_nptr|unique_del_nptr|refcount_nptr)\s*<')
OPTIONAL_REFCOUNT_PTR_RE = re.compile(r'\boptional\s*<\s*refcount_ptr\s*<')
OPTIONAL_REFCOUNT_LOAD_CONTRACT_RE = re.compile(r'\bLoad(?:Location|Map|Critter|Item|CustomEntity)\s*\([^;\n]*\)\s*(?:noexcept\s*)?->\s*optional\s*<\s*refcount_ptr\s*<')
OPTIONAL_REFCOUNT_LOAD_LOCAL_RE = re.compile(r'\boptional\s*<\s*refcount_ptr\s*<[^;\n]+>\s+[A-Za-z_][A-Za-z0-9_]*\s*(?:=|\{)\s*(?:EntityMngr\.)?Load(?:Location|Map|Critter|Item|CustomEntity)\s*\(')
REFCOUNT_DECL_INIT_RE = re.compile(
    r'(?<![A-Za-z0-9_:<>])(?:(?:const|mutable|static|inline)\s+)*'
    r'(?P<kind>refcount_(?:n?ptr))(?:\s*<[^<>()\n;=]+>)?\s+'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?P<initializer>=|\{)\s*(?P<rhs>[^;]+);'
)
WRAPPER_DYNAMIC_CAST_RE = re.compile(
    r'\bdynamic_cast\s*<[^;\n]+?>\s*\(\s*[^()\n;]*\.\s*(?:get|as_ptr|as_nptr)\s*\(\s*\)'
)
WRAPPER_STATIC_CAST_RE = re.compile(
    r'\bstatic_cast\s*<[^;\n]+?>\s*\(\s*[^()\n;]*\.\s*(?:get|as_ptr|as_nptr)\s*\(\s*\)'
)
WRAPPER_CONST_CAST_RE = re.compile(
    r'\bconst_cast\s*<[^;\n]+?>\s*\(\s*[^()\n;]*\.\s*(?:get|as_ptr|as_nptr)\s*\(\s*\)'
)
WRAPPER_REINTERPRET_CAST_RE = re.compile(
    r'\breinterpret_cast\s*<[^;\n]+?>\s*\(\s*[^()\n;]*\.\s*(?:get|as_ptr|as_nptr)\s*\(\s*\)'
)
DIRECT_WRAPPER_RAW_GET_RE = re.compile(r'\.\s*(?:as_ptr|as_nptr)\s*\(\s*\)\s*\.\s*get\s*\(')
SCRIPT_VECTOR_RAW_HANDOFF_RE = re.compile(r'\bvec_transform\s*\([^;\n]+\.\s*get(?:_no_const)?\s*\(\s*\)')
CLIENT_DATA_VALIDATION_INLINE_BYTE_EDGE_RE = re.compile(
    r'\b(?:raw_data|data)\s*\.\s*data\s*\(\s*\)\s*\+\s*offset\b|'
    r'reinterpret_cast\s*<\s*const\s+char\s*\*\s*>\s*\('
)
DIRECT_COMMAND_LINE_ARG_RAW_USE_RE = re.compile(
    r'\bstrex\s*\(\s*args\s*\[[^\]]+\]\s*\)|'
    r'\bstring_view\s*\(\s*args\s*\[[^\]]+\]\s*\)|'
    r'\bargs\s*\[[^\]]+\]\s*\[|'
    r'\brequested_runtime\.[A-Za-z0-9_]+\s*=\s*args\s*\['
)
SCRIPT_API_FUNCTION_RE = re.compile(r'\bFO_SCRIPT_API\b(?P<signature>[^{;]*\((?P<params>[^)]*)\))\s*\{', re.DOTALL)
# Project extensions commonly split the FO_SCRIPT_API declaration from its FO_NAMESPACE implementation
PROJECT_SCRIPT_IMPL_FUNCTION_RE = re.compile(r'^(?P<signature>[^\n;{}]*\bFO_NAMESPACE\b\s+[A-Za-z_][A-Za-z0-9_]*\s*\((?P<params>[^)]*)\))\s*\{', re.MULTILINE)
SCRIPT_RAW_POINTER_PARAM_RE = re.compile(
    r'\b(?:(?:const)\s+)?'
    r'[A-Za-z_:][A-Za-z0-9_:<>]*(?:\s+const)?'
    r'\s*\*+\s*'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)\b'
)
SCRIPT_RECEIVER_RAW_PARAM_RE = re.compile(
    r'\b(?:(?:const)\s+)?'
    r'[A-Za-z_:][A-Za-z0-9_:<>]*(?:\s+const)?'
    r'\s*\*+\s*'
    r'(?P<name>self|server|client|mapper|engine)\b'
)
PROPERTY_SERIALIZER_RAW_CURSOR_RE = re.compile(
    r'\b(?:const\s+)?uint8_t\s*\*&\s*[A-Za-z_][A-Za-z0-9_]*\b|'
    r'\b(?:const\s+)?uint8_t\s*\*\s*[A-Za-z_][A-Za-z0-9_]*\s*=\s*[^;\n]*\.data\s*\(\s*\)|'
    r'\bpdata\b'
)
PROPERTY_SERIALIZER_INLINE_BYTE_EDGE_RE = re.compile(
    r'\bmemory::(?:copy|compare)\s*\([^;\n]*\.data\s*\(\s*\)|'
    r'\breinterpret_cast\s*<\s*const\s+(?:uint8_t|char)\s*\*\s*>|'
    r'\bAppendRawBytes\s*\([^;\n]*\.data\s*\(\s*\)|'
    r'\bset_data\s*\(\s*\{\s*reinterpret_cast'
)
DATA_SERIALIZATION_INLINE_READ_EDGE_RE = re.compile(
    r'\b_data_buf\s*\.\s*data\s*\(\s*\)\s*\+\s*_read_pos|'
    r'\breinterpret_cast\s*<[^;\n]+?>\s*\([^;\n]*\b_data_buf\s*\.\s*data\s*\(\s*\)\s*\+\s*_read_pos'
)
DATA_SERIALIZATION_INLINE_WRITE_EDGE_RE = re.compile(
    r'\bmemory::copy\s*\([^;\n]*\b_data_buf\s*->\s*data\s*\(\s*\)|'
    r'\b_data_buf\s*->\s*data\s*\(\s*\)\s*\+\s*_data_buf\s*->\s*size\s*\(\s*\)\s*-'
)
DATA_WRITER_WRITE_PTR_CALL_RE = re.compile(r'\.\s*write_ptr\s*\(')
DATA_WRITER_WRITE_PTR_ZERO_SIZE_CALL_RE = re.compile(r'\.\s*write_ptr\s*\([^;\n]*,\s*0\s*\)')
DATA_READER_READ_PTR_CALL_RE = re.compile(r'\.\s*read_ptr\s*(?:<[^;\n]+>)?\s*\(')
FILE_READER_COPY_DATA_CALL_RE = re.compile(r'\.\s*CopyData\s*\(')
NULLABLE_IF_INIT_CONDITION_DEREF_RE = re.compile(
    r'\bif\s*\(\s*(?P<kind>nptr|unique_nptr|refcount_nptr)\s*<[^;\n]+>\s+'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*[^;\n]+;[^\n]*'
    r'\b(?P=name)\s*->'
)
NULLPTR_ASSERT_RE = re.compile(r'\bFO_RUNTIME_ASSERT(?:_STR)?\s*\([^;\n]*(?:==|!=)\s*nullptr')
WRAPPER_DECL_NULLPTR_COMPARISON_RE = re.compile(r'\b(?:nptr|unique_nptr|unique_del_nptr|unique_arr_ptr|refcount_nptr|shared_ptr)\s*<[^;\n]+(?:==|!=)\s*nullptr')
WRAPPER_IF_INIT_NULLPTR_COMPARISON_RE = re.compile(r'\b(?:nptr|unique_nptr|unique_del_nptr|unique_arr_ptr|refcount_nptr|shared_ptr)\s*<[^;\n]+;\s*[A-Za-z_][A-Za-z0-9_]*\s*(?:==|!=)\s*nullptr')
WRAPPER_DYN_CAST_NULLPTR_COMPARISON_RE = re.compile(
    r'\.\s*(?:dyn_cast|dynamic_cast)\s*<[^;\n]+>\s*\(\s*\)\s*(?:==|!=)\s*nullptr|'
    r'\.\s*(?:dyn_cast|dynamic_cast)\s*<[^;\n]+>\s*\(\s*\)\s*;\s*[A-Za-z_][A-Za-z0-9_]*\s*(?:==|!=)\s*nullptr'
)
SIMPLE_IDENTIFIER_RE = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')
CLASS_START_RE = re.compile(r'^\s*(?:template\s*<[^>{;]+>\s*)?(?:class|struct)\s+[A-Za-z_][A-Za-z0-9_]*\b')
CONTROL_SIGNATURE_RE = re.compile(r'\b(?:if|for|while|switch|catch)\s*$')
FUNCTION_SIGNATURE_RE = re.compile(r'^\s*(?P<head>[^;{}()]*[A-Za-z_:~][A-Za-z0-9_:~<>\s*&]+?)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:->\s*(?P<ret>[^;{]+?)\s*)?(?:\{|$)')


@dataclass
class Issue:
    path: str
    line: int
    severity: str
    rule: str
    message: str


@dataclass(frozen=True)
class SmartPointerOptions:
    """Project extension points read from the ``smart_pointer`` configuration section."""

    raw_return_guarded_keys: frozenset[str] = frozenset()
    abi_line_re: re.Pattern[str] | None = None
    nullable_factory_re: re.Pattern[str] | None = None

    def is_abi_line(self, code: str) -> bool:
        return RAW_POINTER_ABI_LINE_RE.search(code) is not None or (self.abi_line_re is not None and self.abi_line_re.search(code) is not None)

    def is_nullable_factory_adoption(self, code: str) -> bool:
        if DIRECT_NULLABLE_FACTORY_UNIQUE_DEL_PTR_RE.search(code):
            return True

        return self.nullable_factory_re is not None and self.nullable_factory_re.search(code) is not None


DEFAULT_OPTIONS = SmartPointerOptions()


@dataclass
class InventoryRow:
    path: str
    metric: str
    count: int


@dataclass
class InventoryDetail:
    path: str
    line: int
    metric: str
    code: str


VOCABULARY_PATTERNS = {
    'legacy_raw_ptr': re.compile(r'(?<![A-Za-z0-9])(?:raw_ptr|nullable_raw_ptr)(?![A-Za-z0-9])'),
    'ptr': re.compile(r'\bptr\s*<'),
    'nptr': re.compile(r'\bnptr\s*<'),
    'unique_ptr': re.compile(r'\bunique_ptr\s*<'),
    'unique_nptr': re.compile(r'\bunique_nptr\s*<'),
    'unique_del_nptr': re.compile(r'\bunique_del_nptr\s*<'),
    'refcount_ptr': re.compile(r'\brefcount_ptr\s*<'),
    'refcount_nptr': re.compile(r'\brefcount_nptr\s*<'),
}
VOCABULARY_INVENTORY_SKIP_LINE_RE = re.compile(r'\bSTATIC_REQUIRE\s*\(|\bstd::same_as\s*<')
RAW_AUTO_POINTER_METRICS = {
    'raw_auto_pointer_marker',
    'raw_auto_pointer_test_marker',
    'raw_auto_pointer_abi_marker',
    'raw_auto_pointer_low_level_marker',
}
WRAPPER_RAW_CAST_METRICS = {
    'wrapper_static_cast_marker',
    'wrapper_const_cast_marker',
    'wrapper_reinterpret_cast_marker',
}
HEADER_RAW_ABI_METRICS = {
    'raw_pointer_abi_marker',
    'raw_pointer_container_abi_marker',
}
RAW_POINTER_TYPE_TOKEN = (
    r'auto'
    r'|void'
    r'|char'
    r'|wchar_t'
    r'|bool'
    r'|u?int(?:8|16|32|64)?_t'
    r'|size_t'
    r'|float(?:32|64)?_t'
    r'|double'
    r'|[A-Z][A-Za-z0-9_:<>]*'
    r'|[A-Za-z_][A-Za-z0-9_]*_t'
    r'|[A-Za-z_][A-Za-z0-9_]*::[A-Za-z0-9_:<>]+'
    # Essentials types are snake_case, so the PascalCase alternative above does not reach them
    r'|stack_trace::catched_data|stack_trace::script_layer|stack_trace::data|stack_trace::frame'
    r'|cpu_usage_core_snapshot|cpu_usage_snapshot|data_reader|data_writer|hash_resolver|hash_storage'
    r'|stream_decompressor|stream_compressor|compressor|time_meter|work_thread'
    r'|(?:refcounted|safe_allocator)\s*<[^;\n]+>'
)
RAW_POINTER_MARKER_RE = re.compile(
    r'(?:^|[,;]\s*)'
    r'(?!(?:return|delete|throw|case)\b)'
    rf'(?:const\s+)?(?:{RAW_POINTER_TYPE_TOKEN})\s*\*+\s*(?:const\s+)?[A-Za-z_][A-Za-z0-9_]*'
)
RAW_POINTER_RANGE_FOR_RE = re.compile(
    r'\bfor\s*\(\s*'
    r'(?:(?:const|volatile)\s+)*'
    rf'(?:{RAW_POINTER_TYPE_TOKEN})\s*\*+\s*(?:const\s+)?'
    r'[A-Za-z_][A-Za-z0-9_]*\s*:'
)
RAW_AUTO_POINTER_MARKER_RE = re.compile(
    r'\b(?:const\s+)?auto\s*\*+\s*(?:const\s+)?[A-Za-z_][A-Za-z0-9_]*'
)
RAW_POINTER_CONTAINER_MARKER_RE = re.compile(
    r'\b(?:std::)?(?:array|vector|list|deque|set|unordered_map|unordered_set|span)\s*<[^>;\n]*\*[^>;\n]*>|'
    r'\b(?:small_vector|readonly_vector|ref_hold_vector)\s*<[^>;\n]*\*[^>;\n]*>'
)
RAW_POINTER_CONTAINER_ABI_LINE_RE = re.compile(
    r'\bspan\s*<\s*char\s*\*>\s*(?:args|\{argv\b)|'
    r'\b(?:std::)?vector\s*<\s*char\s*\*>\s+[A-Za-z_]*args\b|'
    r'\bReleaseScriptOwnershipVector\s*\(|'
    r'\bBorrow(?:Mutable)?Script(?:Ref)?Vector(?:As)?\s*\('
)
RAW_POINTER_ABI_LINE_RE = re.compile(
    r'\b(?:FO_SCRIPT_API|FO_EXPORT_FUNC|WINAPI|CALLBACK|VKAPI_CALL)\b|'
    r'\b(?:int|int32_t)\s+main\s*\(|'
    r'\bextern\s+"C"\b|'
    r'\boverride\b|'
    r'\bFO_NAMESPACE\s+[A-Za-z_][A-Za-z0-9_]*\s*\(|'
    r'\bScript(?:Array|Dict)::Create\s*\(|'
    r'\bstatic\s+auto\s+Create\s*\(\s*AngelScript::asITypeInfo\s*\*|'
    r'\bScript(?:Array|Dict)_[A-Za-z0-9_]+\s*\(|'
    r'\bAngelScript(?:Begin|End)Call\s*\(|'
    r'\bAngelScript(?:Message|Line)\s*\(|'
    r'\b[A-Za-z_][A-Za-z0-9_]*\s*\(\s*const\s+asSMessageInfo\s*\*|'
    r'\bScriptTypeOfFactory2\s*\(|'
    r'\bClientRuntimeRunFunc\b|'
    r'\bQueryClientRuntimeExportsFunc\b|'
    r'\bRunClientRuntime(?:Abi)?\s*\(|'
    r'\bStartMainLoop\s*\(\s*void\s*\(\*\s*entry\s*\)\s*\(\s*void\s*\*\s*\)\s*,|'
    r'\bSetCrash(?:Signal|Seh)Info\s*\(|'
    r'\bffunc\.z(?:open|read|write|tell|seek|close|error)_file\b|'
    r'\bImGui(?:Alloc|Free)\s*\(|'
    r'\bEM_JS\s*\(|'
    r'\bvulkan_debug_callback\b|'
    r'\bAngelScript(?:TranslateAppException|Exception)\s*\(|'
    r'\bScriptString_Join\s*\(|'
    r'\bstream_callback\s*=|'
    r'\bPlatform_SetClipboardTextFn\b|'
    r'\bSparkQuadRenderer::(?:render|computeAABB)\s*\(|'
    r'\bcallbacks\.read_func\b|'
    r'\bglslang::TIntermediate\s*\*|'
    r'\bWinApi_GetProcAddress\s*\(|'
    r'\[\]\s*\(\s*const\s+void\s*\*\s*output\s*,\s*unsigned\s+output_len\s*,\s*void\s*\*\s*user_data\s*\)'
)
# Raw pointers in a header leak the boundary to every includer, so only these named C ABI shapes may stay there:
# the client runtime host/runtime entry points and the emscripten main-loop registration
RAW_POINTER_HEADER_ABI_LINE_RE = re.compile(
    r'\bClientRuntimeRunFunc\b|'
    r'\bQueryClientRuntimeExportsFunc\b|'
    r'\bStartMainLoop\s*\(\s*void\s*\(\*\s*entry\s*\)\s*\(\s*void\s*\*\s*\)\s*,'
)
RAW_POINTER_LOW_LEVEL_LINE_RE = re.compile(
    r'\bmem_(?:malloc|calloc|realloc|free)\b|'
    r'\bmemory::(?:copy|move|fill|compare)\b|'
    r'\bfatal::report_strong_assert_and_exit\b|'
    r'\bsame_frame_function\s*\(|'
    r'\bAppendRawBytes\s*\(|'
    r'\b(?:logging::)?write_base\s*\(string_view message, const stack_trace::catched_data\*|'
    r'\bVisitBaseTypePrimitive\s*\(|'
    r'\bScriptDictComparator::operator\(\)\s*\(|'
    r'\bFO_FORCE_INLINE\s+shared_ptr\s*\(|'
    r'\brefcount_ptr\s*\(\s*adopt_tag\b'
)


def is_strict_null_style_scope(path: str) -> bool:
    return any(path.startswith(prefix) for prefix in STRICT_NULL_STYLE_PREFIXES)


def is_strict_wrapper_null_style_scope(path: str) -> bool:
    return any(path.startswith(prefix) for prefix in STRICT_WRAPPER_NULL_STYLE_PREFIXES)


def is_test_scope(path: str) -> bool:
    return '/Tests/' in path or path.startswith('Source/Tests/')


def is_reviewed_file_reader_copy_data_call(path: str, code: str) -> bool:
    if path in {'Source/Common/FileSystem.h', 'Source/Common/FileSystem.cpp'}:
        return True

    # The API's own bounds test: an overrun must keep the position and report the exact failing size
    if (path == 'Source/Tests/Test_FileSystem.cpp'
            and code.strip() == 'check_read_error([&] { reader.CopyData(output); }, 4);'):
        return True

    reviewed_edges = {
        'reader.CopyData(&waveformatex, 16);',
        'file_context->Reader.CopyData(output, bytes_read);',
    }
    return code.strip() in reviewed_edges


def is_allowed_optional_refcount_owner(path: str, code: str) -> bool:
    if path not in OPTIONAL_REFCOUNT_PTR_ALLOWED_FILES:
        return False

    return OPTIONAL_REFCOUNT_LOAD_CONTRACT_RE.search(code) is not None or OPTIONAL_REFCOUNT_LOAD_LOCAL_RE.search(code) is not None


def is_header_path(path: str) -> bool:
    return Path(path).suffix in HEADER_EXTS


def detect_function_context(code: str) -> str:
    stripped = code.strip()

    if not stripped or stripped.startswith('#') or stripped.endswith(';'):
        return ''
    if '(' not in stripped or ')' not in stripped:
        return ''

    match = FUNCTION_SIGNATURE_RE.match(stripped)

    if match is None:
        return ''

    head = match.group('head').strip()
    head_tokens = head.split()

    if CONTROL_SIGNATURE_RE.search(head) or (head_tokens and head_tokens[0] in {'if', 'for', 'while', 'switch', 'catch', 'else', 'do', 'return'}):
        return ''

    name = head_tokens[-1].split('::')[-1].lstrip('*&') if head_tokens else ''

    if not name or name in {'if', 'for', 'while', 'switch', 'catch'}:
        return ''

    # Determine the function's return type so a `return wrapper.get()` that just hands a raw pointer
    # back to a C/script ABI is recognised as a legitimate raw-pointer-return boundary. Prefer the
    # trailing `-> Type`; fall back to the leading type (head minus the trailing name token)
    ret_type = match.group('ret')

    if ret_type is None:
        ret_type = ' '.join(head_tokens[:-1])
        if head and head[-len(name) - 1:-len(name)] == '*':
            ret_type += '*'

    # Drop trailing declarator qualifiers so `-> const void* override` still reads as a raw return
    ret_type = re.sub(r'\b(?:override|final|noexcept|const)\b', '', ret_type).strip()
    # Raw-pointer return (`void*`, `T*`, `ns::X*`) or a bare deduced/template-parameter return
    # (`auto`, `-> T`) — a `.get()` only compiles into the latter when the instantiated type is a
    # pointer, so it is a genuine raw-pointer return rather than a wrapper unwrap
    is_raw = ret_type.endswith('*') or bool(re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', ret_type))
    suffix = ':rawret' if is_raw else ''

    return f'function:{name}{suffix}'


def is_return_boundary_context(context_stack: list[str]) -> bool:
    return any(context.startswith('function:Return') for context in reversed(context_stack))


def is_raw_return_boundary_context(context_stack: list[str]) -> bool:
    return any(context.startswith('function:Return') or context.startswith('function:BorrowScript') for context in reversed(context_stack))


def returns_raw_pointer(context_stack: list[str]) -> bool:
    for context in reversed(context_stack):
        if context.startswith('function:'):
            return context.endswith(':rawret')
    return False


def strip_comments_from_line(line: str, in_block_comment: bool) -> tuple[str, bool]:
    code = ''
    index = 0

    while index < len(line):
        if in_block_comment:
            block_end = line.find('*/', index)

            if block_end == -1:
                return code, True

            index = block_end + 2
            in_block_comment = False
            continue

        line_comment = line.find('//', index)
        block_start = line.find('/*', index)

        if line_comment == -1 and block_start == -1:
            code += line[index:]
            return code, False

        if line_comment != -1 and (block_start == -1 or line_comment < block_start):
            code += line[index:line_comment]
            return code, False

        code += line[index:block_start]
        index = block_start + 2
        in_block_comment = True

    return code, in_block_comment


def classify_refcount_rhs(kind: str, rhs: str, visible_local_names: dict[str, str], visible_raw_local_names: set[str]) -> str:
    allowed_prefixes = (
        'refcount_ptr<',
        'refcount_nptr<',
        'safe_alloc::make_refcounted',
        'std::move(',
    )
    allowed_fragments = (
        '.hold_ref()',
        '.try_hold_ref()',
        '.take_not_null()',
        '::from_addref(',
        '::try_from_addref(',
        '::from_adopted_ref(',
    )

    if any(rhs.startswith(prefix) for prefix in allowed_prefixes) or any(fragment in rhs for fragment in allowed_fragments):
        return ''
    if kind == 'refcount_nptr' and rhs == 'nullptr':
        return ''
    if rhs in visible_local_names and visible_local_names[rhs] in {'refcount_ptr', 'refcount_nptr'}:
        return ''
    if kind == 'refcount_ptr' and rhs == 'nullptr':
        return 'nullptr assignment'
    if rhs in visible_raw_local_names:
        return 'raw local assignment'
    if rhs == 'this':
        return 'raw this assignment'
    if '.get()' in rhs:
        return '.get() assignment'
    if re.search(r'\b(?:dynamic|static|reinterpret|const)_cast\s*<[^>]+\*>', rhs):
        return 'raw cast assignment'
    if SIMPLE_IDENTIFIER_RE.fullmatch(rhs):
        return 'plain identifier assignment'

    return ''


def get_line_no(text: str, pos: int) -> int:
    return text.count('\n', 0, pos) + 1


def split_top_level_commas(text: str) -> list[str]:
    parts: list[str] = []
    current: list[str] = []
    angle_depth = 0
    paren_depth = 0
    brace_depth = 0
    bracket_depth = 0

    for char in text:
        if char == ',' and angle_depth == 0 and paren_depth == 0 and brace_depth == 0 and bracket_depth == 0:
            parts.append(''.join(current).strip())
            current = []
            continue

        current.append(char)

        if char == '<':
            angle_depth += 1
        elif char == '>':
            angle_depth = max(0, angle_depth - 1)
        elif char == '(':
            paren_depth += 1
        elif char == ')':
            paren_depth = max(0, paren_depth - 1)
        elif char == '{':
            brace_depth += 1
        elif char == '}':
            brace_depth = max(0, brace_depth - 1)
        elif char == '[':
            bracket_depth += 1
        elif char == ']':
            bracket_depth = max(0, bracket_depth - 1)

    if current:
        parts.append(''.join(current).strip())

    return parts


def find_matching_brace(text: str, open_pos: int) -> int:
    depth = 0
    i = open_pos
    state = 'code'

    while i < len(text):
        char = text[i]
        pair = text[i:i + 2]

        if state == 'code':
            if pair == '//':
                state = 'line_comment'
                i += 2
                continue
            if pair == '/*':
                state = 'block_comment'
                i += 2
                continue
            if char == '"':
                state = 'string'
                i += 1
                continue
            if char == "'":
                state = 'char'
                i += 1
                continue
            if char == '{':
                depth += 1
            elif char == '}':
                depth -= 1

                if depth == 0:
                    return i

        elif state == 'line_comment':
            if char == '\n':
                state = 'code'
        elif state == 'block_comment':
            if pair == '*/':
                state = 'code'
                i += 2
                continue
        elif state == 'string':
            if char == '\\':
                i += 2
                continue
            if char == '"':
                state = 'code'
        elif state == 'char':
            if char == '\\':
                i += 2
                continue
            if char == "'":
                state = 'code'

        i += 1

    return -1


def audit_nonnull_empty_returns(rel: str, text: str) -> list[Issue]:
    issues: list[Issue] = []

    for return_match in NONNULL_SMART_RETURN_RE.finditer(text):
        body_open = text.find('{', return_match.end())
        declaration_end = text.find(';', return_match.end())

        if body_open == -1 or (declaration_end != -1 and declaration_end < body_open):
            continue
        if body_open - return_match.end() > 600:
            continue

        body_close = find_matching_brace(text, body_open)

        if body_close == -1:
            continue

        for empty_return_match in EMPTY_NONNULL_RETURN_RE.finditer(text, body_open, body_close + 1):
            issues.append(Issue(
                rel,
                get_line_no(text, empty_return_match.start()),
                'error',
                'EmptyReturnFromNonNullPointer',
                f'Function/lambda returning {return_match.group("kind")}<T> returns an empty value. Use the nullable n* return type or make absence impossible.',
            ))

    return issues


def iter_script_binding_function_matches(rel: str, text: str):
    yield from SCRIPT_API_FUNCTION_RE.finditer(text)

    if rel.startswith(PROJECT_KEY_PREFIX):
        yield from PROJECT_SCRIPT_IMPL_FUNCTION_RE.finditer(text)


def audit_script_receiver_inputs(rel: str, text: str) -> list[Issue]:
    issues: list[Issue] = []

    for function_match in iter_script_binding_function_matches(rel, text):
        receiver_names = [param_match.group('name') for param_match in SCRIPT_RECEIVER_RAW_PARAM_RE.finditer(function_match.group('params'))]

        if not receiver_names:
            continue

        body_open = function_match.end() - 1
        body_close = find_matching_brace(text, body_open)

        if body_close == -1:
            continue

        body_start_line = get_line_no(text, body_open) + 1
        body_text = text[body_open + 1:body_close]
        in_block_comment = False
        reported_names: set[str] = set()

        for line_offset, line in enumerate(body_text.splitlines()):
            code, in_block_comment = strip_comments_from_line(line, in_block_comment)

            for name in receiver_names:
                if name in reported_names:
                    continue
                if re.search(rf'\b{re.escape(name)}\s*->', code):
                    issues.append(Issue(
                        rel,
                        body_start_line + line_offset,
                        'error',
                        'ScriptReceiverRawDereference',
                        f'FO_SCRIPT_API receiver `{name}` is dereferenced directly. Bind it to ptr<T> or nptr<T> before normal engine work.',
                    ))
                    reported_names.add(name)

    return issues


def audit_script_receiver_raw_ignores(rel: str, text: str) -> list[Issue]:
    issues: list[Issue] = []

    for function_match in iter_script_binding_function_matches(rel, text):
        receiver_names = [param_match.group('name') for param_match in SCRIPT_RECEIVER_RAW_PARAM_RE.finditer(function_match.group('params'))]

        if not receiver_names:
            continue

        body_open = function_match.end() - 1
        body_close = find_matching_brace(text, body_open)

        if body_close == -1:
            continue

        body_start_line = get_line_no(text, body_open) + 1
        body_text = text[body_open + 1:body_close]
        in_block_comment = False
        reported_names: set[str] = set()

        for line_offset, line in enumerate(body_text.splitlines()):
            code, in_block_comment = strip_comments_from_line(line, in_block_comment)

            for name in receiver_names:
                if name in reported_names:
                    continue
                if re.search(rf'\bignore_unused\s*\([^;\n]*\b{re.escape(name)}\b', code):
                    issues.append(Issue(
                        rel,
                        body_start_line + line_offset,
                        'error',
                        'ScriptReceiverRawIgnore',
                        f'FO_SCRIPT_API receiver `{name}` is ignored while still raw. Bind it to ptr<T> first, even in no-op/disabled bodies.',
                    ))
                    reported_names.add(name)

    return issues


def audit_plain_script_raw_inputs(rel: str, text: str) -> list[Issue]:
    issues: list[Issue] = []
    receiver_names = {'self', 'server', 'client', 'mapper', 'engine'}

    for function_match in iter_script_binding_function_matches(rel, text):
        raw_param_names = [
            param_match.group('name')
            for param_match in SCRIPT_RAW_POINTER_PARAM_RE.finditer(function_match.group('params'))
            if param_match.group('name') not in receiver_names
        ]

        if not raw_param_names:
            continue

        body_open = function_match.end() - 1
        body_close = find_matching_brace(text, body_open)

        if body_close == -1:
            continue

        body_start_line = get_line_no(text, body_open) + 1
        body_text = text[body_open + 1:body_close]
        in_block_comment = False
        reported_names: set[str] = set()

        for line_offset, line in enumerate(body_text.splitlines()):
            code, in_block_comment = strip_comments_from_line(line, in_block_comment)

            for name in raw_param_names:
                if name in reported_names:
                    continue
                if re.search(rf'\b{re.escape(name)}\s*->', code):
                    issues.append(Issue(
                        rel,
                        body_start_line + line_offset,
                        'error',
                        'ScriptInputRawDereference',
                        f'FO_SCRIPT_API input `{name}` is dereferenced directly. Bind it to ptr<T> or nptr<T> before normal engine work.',
                    ))
                    reported_names.add(name)

    return issues


def audit_plain_script_raw_input_ignores(rel: str, text: str) -> list[Issue]:
    issues: list[Issue] = []
    receiver_names = {'self', 'server', 'client', 'mapper', 'engine'}

    for function_match in iter_script_binding_function_matches(rel, text):
        raw_param_names = [
            param_match.group('name')
            for param_match in SCRIPT_RAW_POINTER_PARAM_RE.finditer(function_match.group('params'))
            if param_match.group('name') not in receiver_names
        ]

        if not raw_param_names:
            continue

        body_open = function_match.end() - 1
        body_close = find_matching_brace(text, body_open)

        if body_close == -1:
            continue

        body_start_line = get_line_no(text, body_open) + 1
        body_text = text[body_open + 1:body_close]
        in_block_comment = False
        reported_names: set[str] = set()

        for line_offset, line in enumerate(body_text.splitlines()):
            code, in_block_comment = strip_comments_from_line(line, in_block_comment)

            for name in raw_param_names:
                if name in reported_names:
                    continue
                if re.search(rf'\bignore_unused\s*\([^;\n]*\b{re.escape(name)}\b', code):
                    issues.append(Issue(
                        rel,
                        body_start_line + line_offset,
                        'error',
                        'ScriptInputRawIgnore',
                        f'FO_SCRIPT_API input `{name}` is ignored while still raw. Bind it to ptr<T> or nptr<T> first, even in no-op/disabled bodies.',
                    ))
                    reported_names.add(name)

    return issues


CONTROL_EXIT_RE = re.compile(r'\b(?:return|continue|break|throw)\b')
CONTROL_HEAD_RE = re.compile(r'\b(?:if|while|for)\s*\(')
# Exit-on-fail assertion macros (FO_VERIFY_AND_CONTINUE reports-and-continues, so it is NOT here)
EXIT_ASSERT_MACROS = r'FO_(?:VERIFY_AND_THROW|STRONG_ASSERT|VERIFY_AND_RETURN(?:_VALUE)?)'
# Names re-bound by a structured binding / range-for variable, which shadow any stale outer nullable
STRUCTURED_BINDING_RE = re.compile(r'\bauto\s*&*\s*\[([^\]]*)\]')
RANGE_FOR_VAR_RE = re.compile(r'\bfor\s*\(\s*(?:const\s+)?(?:auto|[A-Za-z_][\w:<>]*)\s*[&*]*\s*([A-Za-z_]\w*)\s*:')


def nullable_local_truthiness_guarded_on_line(name: str, code: str, line_declared: frozenset = frozenset()) -> bool:
    # Positive non-null proof of `name` on this line. `line_declared` is the set of nullable
    # locals declared on this same line (so an `if (nptr<T> name = expr)` head counts as a guard)
    escaped = re.escape(name)
    # positive truthiness operand: `name && ...`, `name ? ...`, `name != nullptr`, `!!name && ...`
    if re.search(rf'(?<![\w!]){escaped}\s*(?:&&|\?)', code):
        return True
    if re.search(rf'(?<![\w!]){escaped}\s*!=\s*nullptr', code):
        return True
    if re.search(rf'!!\s*{escaped}\s*&&', code):
        return True
    # `name` as leading condition operand: `if (name` / `while (name`
    if re.search(rf'\b(?:if|while)\s*\(\s*{escaped}\s*(?:\)|&&)', code):
        return True
    # `name` as a trailing `&&` operand: `... && name )` / `&& name &&` / `&& name {`
    if re.search(rf'&&\s*{escaped}\s*(?:\)|&&|\{{)', code):
        return True
    # negative short-circuit that guards a same-line deref: `!name || name->...`
    if re.search(rf'(?<![\w])!\s*{escaped}\s*\|\|', code):
        return True
    # explicit for/if-init condition token: `; name ;` | `; name )` | `; name &&` — covers a for-loop
    # condition `for (T name = ..; name; ..)` and a C++17 if-init `if (T x = ..; name)`
    if re.search(rf';\s*{escaped}\s*(?:;|\)|&&)', code):
        return True
    # if/while init-decl idiom: `if (T name = expr)` — the declared value IS the condition, so it is
    # proven non-null in the guarded body, and it re-scopes/shadows any stale outer `name`. Restricted
    # to if/while: a range-for `for (nptr<T> name : container)` does NOT prove the element non-null,
    # and a `for (..; cond; ..)` is covered by the explicit-condition rule above
    if re.search(r'\b(?:if|while)\s*\(', code):
        if name in line_declared:
            return True
        if re.search(rf'[(,]\s*(?:const\s+)?(?:auto|[A-Za-z_][\w:<>]*)\s*[&*]*\s*{escaped}\s*=', code):
            return True
    return False


def nullable_local_negative_test_on_line(name: str, code: str) -> bool:
    # Negative test `!name` / `name == nullptr` inside an `if` — as the first operand, or as any
    # operand adjacent to `||` (`if (A || !name)` / `if (!name || B)`). An OR-chain early exit proves
    # `name` non-null for the remainder: not returning means every OR term is false, so `!name` is
    # false. `&& !name` is deliberately excluded — `if (A && !name) return;` does not prove non-null —
    # so the operand must be bounded by `)` or `||`, never `&&`
    escaped = re.escape(name)
    if not re.search(r'\bif\s*\(', code):
        return False
    if re.search(rf'\bif\s*\(\s*!\s*{escaped}\s*(?:\)|\|\|)', code):
        return True
    if re.search(rf'\bif\s*\(\s*{escaped}\s*==\s*nullptr\s*(?:\)|\|\|)', code):
        return True
    if re.search(rf'\|\|\s*!\s*{escaped}\s*(?:\)|\|\|)', code):
        return True
    if re.search(rf'\|\|\s*{escaped}\s*==\s*nullptr\s*(?:\)|\|\|)', code):
        return True
    return False


def nullable_local_assert_guarded_on_line(name: str, code: str) -> bool:
    # Exit-on-fail assertion of the local's truthiness — `FO_VERIFY_AND_THROW(name, ...)`,
    # `FO_STRONG_ASSERT(name, ...)`, `FO_VERIFY_AND_RETURN(name != nullptr, ...)`, etc. — proves
    # `name` non-null for the remainder of the scope (the macro exits on failure)
    escaped = re.escape(name)
    if re.search(rf'\b{EXIT_ASSERT_MACROS}\s*\(\s*{escaped}\s*(?:,|\))', code):
        return True
    if re.search(rf'\b{EXIT_ASSERT_MACROS}\s*\(\s*{escaped}\s*!=\s*nullptr', code):
        return True
    # `name.as_ptr()` / `name.take_not_null()` asserts non-null (always-on) — reaching the next line
    # proves `name` non-null
    if re.search(rf'(?<![\w.]){escaped}\s*\.\s*(?:as_ptr|take_not_null)\s*\(\s*\)', code):
        return True
    return False


def nullable_local_nonnull_assignment_on_line(name: str, code: str) -> bool:
    escaped = re.escape(name)
    assignment = re.search(rf'(?<![\w.]){escaped}\s*(?P<initializer>=|\{{)\s*(?P<rhs>[^;\n]+)', code)
    if assignment is None:
        return False

    rhs = assignment.group('rhs').strip()
    if assignment.group('initializer') == '{' and rhs.endswith('}'):
        rhs = rhs[:-1].rstrip()
    if re.match(r'^safe_alloc::make_(?:unique|raw|shared|refcounted)\b', rhs):
        return True
    if re.match(r'^make_ptr\s*\(', rhs):
        return True
    return '?' not in rhs and re.fullmatch(r'.*\.\s*(?:as_ptr|take_not_null)\s*\(\s*\)', rhs) is not None


def header_is_function_signature(header: str) -> bool:
    # `header` is the code between the previous top-level `; { }` boundary and a `{`; treat that `{`
    # as a function body opener when the (comment-stripped, whitespace-collapsed) header parses as a
    # function signature via detect_function_context
    collapsed = re.sub(r'\s+', ' ', header).strip()
    if not collapsed or collapsed.startswith('#'):
        return False
    if '(' not in collapsed or ')' not in collapsed:
        return False
    return detect_function_context(collapsed).startswith('function:')


CONSTRUCTOR_INIT_SIGNATURE_RE = re.compile(
    r'(?P<signature>[^;{}]*\)\s*(?:const\s*)?(?:noexcept(?:\s*\([^)]*\))?\s*)?)\s*:(?!:)'
)


def constructor_initializer_body_is_function(text: str, body_open: int) -> bool:
    # Constructor bodies follow a member-initializer list, so the nearest `{`/`}` boundaries belong
    # to member initializers rather than to the signature. Recognise the engine's body-on-own-line
    # style and look back to the last `signature) :` whose intervening initializer delimiters balance
    line_start = text.rfind('\n', 0, body_open) + 1
    if text[line_start:body_open].strip():
        return False

    window_start = max(0, body_open - 8192)
    window = text[window_start:body_open]

    for match in reversed(list(CONSTRUCTOR_INIT_SIGNATURE_RE.finditer(window))):
        signature = re.sub(r'\s+', ' ', match.group('signature')).strip()
        if not detect_function_context(signature).startswith('function:'):
            continue

        initializer_text = window[match.end():]
        if initializer_text.count('{') != initializer_text.count('}'):
            continue
        if initializer_text.count('(') != initializer_text.count(')'):
            continue
        if initializer_text.count('[') != initializer_text.count(']'):
            continue

        tail = initializer_text.rstrip()
        if tail and tail[-1] not in '})]':
            continue

        return True

    return False


def iter_generic_function_body_opens(text: str) -> list[int]:
    # Yield the text index of each top-level (non-nested) function body's opening `{`. Inner braces
    # (control blocks, lambdas, initializer lists) are skipped by jumping past the matching `}` of
    # each function. Multi-line signatures are collapsed by header_is_function_signature; constructor
    # member-init lists use the focused fallback above because their inner braces reset normal boundaries
    opens: list[int] = []
    index = 0
    length = len(text)
    boundary = 0
    state = 'code'

    while index < length:
        char = text[index]
        pair = text[index:index + 2]

        if state == 'code':
            if pair == '//':
                newline = text.find('\n', index)
                index = length if newline == -1 else newline
                continue
            if pair == '/*':
                block_end = text.find('*/', index + 2)
                index = length if block_end == -1 else block_end + 2
                continue
            if char == '"':
                state = 'string'
                index += 1
                continue
            if char == "'":
                state = 'char'
                index += 1
                continue
            if char == '{':
                if header_is_function_signature(text[boundary:index]) or constructor_initializer_body_is_function(text, index):
                    opens.append(index)
                    close = find_matching_brace(text, index)
                    if close == -1:
                        return opens
                    index = close + 1
                    boundary = index
                    continue
                boundary = index + 1
            elif char in ';}':
                boundary = index + 1
            index += 1
        elif state == 'string':
            if char == '\\':
                index += 2
                continue
            if char == '"':
                state = 'code'
            index += 1
        elif state == 'char':
            if char == '\\':
                index += 2
                continue
            if char == "'":
                state = 'code'
            index += 1

    return opens


def iter_nullable_deref_body_opens(rel: str, text: str) -> list[int]:
    # Function bodies to scan for unguarded nullable-local dereferences. In strict Engine and project
    # scopes this is every top-level function; elsewhere (build tooling, generated files) it stays
    # limited to script-binding bodies to preserve the original narrow coverage. Script-binding
    # bodies are always included (they can appear in any file) and deduped against the generic scan
    opens: set[int] = {function_match.end() - 1 for function_match in iter_script_binding_function_matches(rel, text)}

    if is_strict_wrapper_null_style_scope(rel):
        opens.update(iter_generic_function_body_opens(text))

    return sorted(opens)


def audit_nullable_local_dereferences(rel: str, text: str) -> list[Issue]:
    # Flag a nullable local (`nptr<T>` / `unique_nptr<T>` / `refcount_nptr<T>`) dereferenced with
    # `->` before any preceding null check. This is the static guarantee that replaces the runtime
    # assert `as_ptr()` used to add: the convention keeps the checked value as an `nptr<T>` and
    # dereferences it directly, so the analyzer must prove a guard precedes every deref. Recognised
    # guards: `if (name)` / positive short-circuit / `name != nullptr`, an `if (!name) <exit>` /
    # `verify(name)` early exit, an `if (nptr<T> name = expr)` if-init, and the `for (..; name; ..)`
    # loop condition. See Docs/en/contributing/coding-contracts/smart-pointers.md
    issues: list[Issue] = []

    for body_open in iter_nullable_deref_body_opens(rel, text):
        body_close = find_matching_brace(text, body_open)

        if body_close == -1:
            continue

        # body_text starts at the char after `{`, so its splitlines()[0] is the tail of the `{`
        # line (offset 0 == the `{` line). Anchor at the `{` line so `+ line_offset` lands on the
        # actual source line of a deref, not one line past it
        body_start_line = get_line_no(text, body_open)
        body_text = text[body_open + 1:body_close]
        in_block_comment = False
        nullable_names: set[str] = set()
        reported_names: set[str] = set()
        brace_depth = 0
        positive_guards: dict[str, list[int]] = {}  # name -> depths of open `if (name){` guard bodies
        guarded_depths: dict[str, list[int]] = {}  # name -> enclosing depths where a non-null proof is valid
        negative_blocks: list[list] = []  # open `if (!name){` blocks: [name, body_depth, saw_exit, made_nonnull]

        def add_guard(name: str, depth: int) -> None:
            guarded_depths.setdefault(name, []).append(depth)

        def has_guard(name: str) -> bool:
            return any(depth <= brace_depth for depth in guarded_depths.get(name, []))

        for line_offset, line in enumerate(body_text.splitlines()):
            code, in_block_comment = strip_comments_from_line(line, in_block_comment)

            for decl_match in LOCAL_NONNULL_DECL_RE.finditer(code):
                nullable_names.discard(decl_match.group('name'))

            # `auto X = <expr>.as_ptr();` / `.take_not_null()` re-declares X as a non-null ptr/owner,
            # shadowing any stale same-named nullable local left in the flat scan's name set
            for decl_match in re.finditer(r'\bauto\s*&?\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{)\s*[^;\n]*\.\s*(?:as_ptr|take_not_null)\s*\(\s*\)', code):
                nullable_names.discard(decl_match.group('name'))

            # A structured binding / range-for variable re-binds the name in a fresh scope, shadowing
            # any stale outer nullable of the same spelling (the flat scan does not pop block scopes)
            for binding_match in STRUCTURED_BINDING_RE.finditer(code):
                for bound_name in binding_match.group(1).split(','):
                    nullable_names.discard(bound_name.strip())
            for for_var_match in RANGE_FOR_VAR_RE.finditer(code):
                nullable_names.discard(for_var_match.group(1))

            line_declared = frozenset(decl_match.group('name') for decl_match in LOCAL_NULLABLE_DECL_RE.finditer(code))
            for name in line_declared:
                guarded_depths.pop(name, None)
                positive_guards.pop(name, None)
            nullable_names.update(line_declared)

            # The right-hand side reads the old value before assignment takes effect (`p = p->Next()`).
            # Remember assignments now, then invalidate the old proof after checking this line's derefs
            reassigned_names = {
                name for name in nullable_names
                if name not in line_declared and re.search(rf'(?<![\w.]){re.escape(name)}\s*=(?!=)', code)
            }
            asserted_names = {
                name for name in nullable_names
                if nullable_local_assert_guarded_on_line(name, code) and not CONTROL_HEAD_RE.search(code)
            }
            nonnull_assigned_names = {
                name for name in nullable_names
                if nullable_local_nonnull_assignment_on_line(name, code) and not CONTROL_HEAD_RE.search(code)
            }

            has_control_exit = bool(CONTROL_EXIT_RE.search(code))
            if has_control_exit:
                for negative_block in negative_blocks:
                    # Only an unconditional exit in the direct body proves that the entire negative
                    # branch cannot fall through. A nested/one-line `if (other) return` is not enough
                    if brace_depth == negative_block[1] and not CONTROL_HEAD_RE.search(code):
                        negative_block[2] = True

            for name in sorted(nullable_names):
                if name in reported_names:
                    continue
                if not re.search(rf'\b{re.escape(name)}\s*->', code):
                    continue
                if positive_guards.get(name) or has_guard(name) or name in asserted_names or name in nonnull_assigned_names or nullable_local_truthiness_guarded_on_line(name, code, line_declared):
                    continue
                issues.append(Issue(
                    rel,
                    body_start_line + line_offset,
                    'error',
                    'NullableLocalDereference',
                    f'Nullable local `{name}` is dereferenced without a preceding null check. '
                    f'Guard it (`if ({name})`, `{name} && ...`, `verify`/`FO_VERIFY_AND_THROW({name}, ...)`, '
                    f'or early-return on null) before `{name}->`.',
                ))
                reported_names.add(name)

            # Assignment invalidates a prior check only after its right-hand side has been evaluated.
            # Exit assertions and known-non-null assignments then establish the proof for later lines
            for name in reassigned_names:
                guarded_depths.pop(name, None)
                positive_guards.pop(name, None)
            for name in asserted_names:
                add_guard(name, brace_depth)
                for negative_block in negative_blocks:
                    if negative_block[0] == name and brace_depth == negative_block[1]:
                        negative_block[2] = True
            for name in nonnull_assigned_names:
                add_guard(name, brace_depth)
                for negative_block in negative_blocks:
                    if negative_block[0] == name and brace_depth == negative_block[1]:
                        negative_block[3] = True

            opens = code.count('{')
            closes = code.count('}')

            if code.rstrip().endswith('{') and opens > closes:
                # A block opened by `if (!name) {` is a negative-guard block even when the head also
                # contains a same-line short-circuit (`if (!name || ...)`), so classify negative first
                for name in nullable_names:
                    if nullable_local_negative_test_on_line(name, code):
                        negative_blocks.append([name, brace_depth + 1, False, False])
                    elif nullable_local_truthiness_guarded_on_line(name, code, line_declared):
                        positive_guards.setdefault(name, []).append(brace_depth + 1)
            else:
                for name in nullable_names:
                    if nullable_local_negative_test_on_line(name, code) and has_control_exit and len(CONTROL_HEAD_RE.findall(code)) == 1:
                        add_guard(name, brace_depth)

            brace_depth += opens - closes
            if brace_depth < 0:
                brace_depth = 0

            for name in list(positive_guards):
                positive_guards[name] = [depth for depth in positive_guards[name] if depth <= brace_depth]
                if not positive_guards[name]:
                    del positive_guards[name]

            for name in list(guarded_depths):
                guarded_depths[name] = [depth for depth in guarded_depths[name] if depth <= brace_depth]
                if not guarded_depths[name]:
                    del guarded_depths[name]

            while negative_blocks and negative_blocks[-1][1] > brace_depth:
                closed_name, _, saw_exit, made_nonnull = negative_blocks.pop()
                if saw_exit or made_nonnull:
                    add_guard(closed_name, brace_depth)

    return issues


def audit_nullable_if_init_dereferences(rel: str, text: str) -> list[Issue]:
    issues: list[Issue] = []

    # NullableIfInitDereference (body deref `if (nptr x = ...) { x->m }`) is intentionally NOT flagged:
    # the if-init condition is the nptr's own non-null check, so the guarded body deref is always safe
    # The condition-deref variant below stays — a deref in the *condition*
    # can run before/without the null check
    for match in NULLABLE_IF_INIT_CONDITION_DEREF_RE.finditer(text):
        issues.append(Issue(
            rel,
            get_line_no(text, match.start()),
            'error',
            'NullableIfInitConditionDereference',
            f'Nullable {match.group("kind")}<T> if-init local `{match.group("name")}` is dereferenced in the condition. Split the lookup, validate it, and bind a checked non-null local before use.',
        ))

    return issues


def audit_nonnull_param_nullable_operations(rel: str, text: str) -> list[Issue]:
    issues: list[Issue] = []
    lines = text.splitlines()
    line_index = 0

    while line_index < len(lines):
        signature = ''
        signature_end_index = line_index
        found_body = False

        for candidate_index in range(line_index, min(line_index + 12, len(lines))):
            code = lines[candidate_index].split('//', 1)[0]
            signature += code + '\n'
            signature_end_index = candidate_index

            if '{' in code:
                found_body = True
                break
            if ';' in code:
                break

        if not found_body or '(' not in signature or ')' not in signature:
            line_index += 1
            continue

        signature_before_body = signature.split('{', 1)[0]

        if '(' not in signature_before_body or ')' not in signature_before_body:
            line_index += 1
            continue

        signature_head = signature_before_body.split('(', 1)[0].strip()

        if CONTROL_SIGNATURE_RE.search(signature_head):
            line_index += 1
            continue

        # A window may open mid-expression (e.g. on the `)"` tail of a multi-line raw-string literal),
        # leaving the only '(' after the last ')'. Such a window is not a function signature
        params_split = signature_before_body.rsplit(')', 1)[0].split('(', 1)

        if len(params_split) < 2:
            line_index += 1
            continue

        params_text = params_split[1]
        params: dict[str, str] = {}

        for param in split_top_level_commas(params_text):
            param_match = NONNULL_PARAM_RE.match(param)

            if param_match is not None:
                params[param_match.group('name')] = param_match.group('kind')

        if not params:
            line_index += 1
            continue

        body_depth = 0
        body_lines: list[tuple[int, str]] = []
        body_end_index = signature_end_index

        for body_index in range(signature_end_index, len(lines)):
            code = lines[body_index].split('//', 1)[0]
            body_depth += code.count('{') - code.count('}')
            body_lines.append((body_index + 1, code))

            if body_depth <= 0:
                body_end_index = body_index
                break

        for name, kind in params.items():
            escaped_name = re.escape(name)

            for body_line_no, code in body_lines[1:]:
                nullable_operation = ''

                if re.search(rf'\b{escaped_name}\s*(?:==|!=)\s*nullptr|nullptr\s*(?:==|!=)\s*\b{escaped_name}\b', code):
                    nullable_operation = 'nullptr comparison'
                elif re.search(rf'\b(?:if|while)\s*\(\s*!\s*{escaped_name}\s*(?:\)|&&|\|\|)', code):
                    nullable_operation = 'negated bool check'
                elif re.search(rf'\b(?:if|while)\s*\(\s*{escaped_name}\s*(?:\)|&&|\|\|)', code):
                    nullable_operation = 'bool check'
                elif re.search(rf'\b{escaped_name}\s*\?\s*{escaped_name}\s*->', code):
                    nullable_operation = 'ternary dereference'
                elif re.search(rf'\b{escaped_name}\s*\?\s*{escaped_name}\.\s*get\s*\(\s*\)', code):
                    nullable_operation = 'ternary get'

                if nullable_operation:
                    issues.append(Issue(rel, body_line_no, 'error', 'NullableOperationOnNonNullParam', f'Parameter {kind}<T> `{name}` uses nullable operation: {nullable_operation}. Use an n* parameter or remove the nullable operation.'))

        line_index = body_end_index + 1

    return issues

def has_nullable_unique_del_spelling(code: str) -> bool:
    if OPTIONAL_UNIQUE_DEL_PTR_RE.search(code):
        return True
    for match in UNIQUE_DEL_PTR_START_RE.finditer(code):
        depth = 1
        for position in range(match.end(), len(code)):
            if code[position] == '<':
                depth += 1
            elif code[position] == '>':
                depth -= 1
                if depth == 0:
                    tail = code[position + 1:]
                    if EMPTY_UNIQUE_DEL_DECL_RE.match(tail):
                        return True
                    if re.search(r'\breturn\s*$', code[:match.start()]) and NULL_UNIQUE_DEL_TEMP_RE.match(tail):
                        return True
                    break
    return False


def audit_text(rel: str, text: str, options: SmartPointerOptions = DEFAULT_OPTIONS) -> list[Issue]:
    issues: list[Issue] = []
    context_stack: list[str] = []
    local_scopes: list[dict[str, str]] = []
    raw_local_scopes: list[set[str]] = []
    pending_context = ''
    lines = text.splitlines()

    issues.extend(audit_nonnull_empty_returns(rel, text))
    issues.extend(audit_script_receiver_inputs(rel, text))
    issues.extend(audit_script_receiver_raw_ignores(rel, text))
    issues.extend(audit_plain_script_raw_inputs(rel, text))
    issues.extend(audit_plain_script_raw_input_ignores(rel, text))
    issues.extend(audit_nullable_local_dereferences(rel, text))
    issues.extend(audit_nullable_if_init_dereferences(rel, text))
    issues.extend(audit_nonnull_param_nullable_operations(rel, text))

    for line_no, line in enumerate(lines, start=1):
        is_class_member_context = bool(context_stack and context_stack[-1] == 'class')
        visible_local_names: dict[str, str] = {}
        visible_raw_local_names: set[str] = set()

        for scope in local_scopes:
            visible_local_names.update(scope)
        for scope in raw_local_scopes:
            visible_raw_local_names.update(scope)

        code = line.split('//', 1)[0]

        if OLD_RAW_PTR_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'OldRawPtrName', 'Use ptr<T> or nptr<T> instead of raw_ptr<T> / nullable_raw_ptr<T>.'))

        if has_nullable_unique_del_spelling(code):
            issues.append(Issue(rel, line_no, 'error', 'NullableUniqueDelPtrSpelling', 'Use unique_del_nptr<T> for nullable custom-deleter owners and returns instead of optional<unique_del_ptr<T>>, default-empty unique_del_ptr<T>, or nullptr-initialized unique_del_ptr<T>.'))

        if OPTIONAL_SMART_POINTER_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'OptionalSmartPointerSpelling', 'Do not stack optional<T> around smart-pointer wrappers; use the matching nullable wrapper or a reviewed domain result type.'))

        if OPTIONAL_REFCOUNT_PTR_RE.search(code) and not is_allowed_optional_refcount_owner(rel, code):
            issues.append(Issue(rel, line_no, 'error', 'OptionalRefcountPtrSpelling', 'Use refcount_nptr<T> for ordinary nullable intrusive ownership; reserve optional<refcount_ptr<T>> for reviewed load-result contracts that carry a second state.'))

        if options.is_nullable_factory_adoption(code):
            issues.append(Issue(rel, line_no, 'error', 'DirectNullableFactoryUniqueDelPtr', 'Nullable external factory results must first use unique_del_nptr<T>, or be narrowed from nptr<T> to ptr<T> before constructing a non-null unique_del_ptr<T>.'))

        unique_del_from_nullable_local = RETURN_UNIQUE_DEL_PTR_FROM_NULLABLE_LOCAL_RE.search(code)

        if unique_del_from_nullable_local is not None:
            nullable_local_name = unique_del_from_nullable_local.group('name')

            if visible_local_names.get(nullable_local_name) in {'nptr', 'unique_nptr', 'unique_del_nptr'}:
                issues.append(Issue(rel, line_no, 'error', 'UniqueDelPtrFromNullableLocal', 'Nullable local results must be checked and implicitly converted into a typed ptr<T> local before constructing a non-null unique_del_ptr<T>, or the return contract should stay unique_del_nptr<T>.'))

        if rel not in DIRECT_STD_UNIQUE_PTR_ALLOWED_FILES and DIRECT_STD_UNIQUE_PTR_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'DirectStdUniquePtr', 'Use engine unique_ptr/unique_nptr aliases and safe_alloc factories instead of direct std::unique_ptr/std::make_unique.'))

        if rel not in DIRECT_STD_SHARED_PTR_ALLOWED_FILES and DIRECT_STD_SHARED_PTR_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'DirectStdSharedPtr', 'Use engine shared_ptr/weak_ptr aliases and safe_alloc::make_shared() instead of direct std::shared_ptr/std::weak_ptr/std::make_shared.'))

        if DIRECT_UNIQUE_OWNER_NEW_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'DirectUniqueOwnerNew', 'Use safe_alloc::make_unique() / make_raw() instead of constructing unique owners directly from new.'))

        if rel not in DIRECT_UNIQUE_OWNER_WRAPPER_GET_ALLOWED_FILES and DIRECT_UNIQUE_OWNER_WRAPPER_GET_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'DirectUniqueOwnerWrapperGet', 'Use adopt_unique_ptr() / make_unique_del_ptr() or a named ownership handoff instead of constructing unique owners directly from wrapper.get().'))

        if IGNORED_RELEASE_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'IgnoredRelease', 'Name the released owner or pass it to an explicit ownership-transfer boundary instead of discarding release() with (void).'))

        if RAW_LOCAL_WRAPPER_GET_ASSIGN_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'RawLocalWrapperGetAssignment', 'Keep wrapper unwraps at explicit ABI boundaries; do not store wrapper.get() in raw T* locals.'))

        if DIRECT_OWNER_RESET_WRAPPER_GET_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'DirectOwnerResetWrapperGet', 'Do not reset an owner from wrapper.get(); use a named ownership handoff/adoption helper instead.'))

        if DIRECT_DELETE_WRAPPER_GET_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'DirectDeleteWrapperGet', 'Adopt wrapper-owned raw storage into a named unique owner at cleanup boundaries instead of deleting wrapper.get() / wrapper.get_no_const() directly.'))

        if DIRECT_WRAPPER_GET_INDEX_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'DirectWrapperGetIndex', 'Narrow wrappers to named ptr/nptr locals or string/span views instead of indexing wrapper.get() directly.'))

        if rel.startswith('Source/Tests/') and DIRECT_WRAPPER_GET_ARITHMETIC_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'DirectWrapperGetArithmeticInTest', 'Use vector/span element helpers in tests instead of pointer arithmetic on wrapper.get().'))

        if rel not in DIRECT_WRAPPER_GET_COMPARISON_ALLOWED_FILES and DIRECT_WRAPPER_GET_COMPARISON_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'DirectWrapperGetComparison', 'Compare wrappers directly, or compare named ptr/nptr views, instead of unwrapping both sides through .get().'))

        if (rel in DIRECT_WRAPPER_RETURN_GET_FILES or rel in options.raw_return_guarded_keys) and DIRECT_SCRIPT_WRAPPER_RETURN_GET_RE.search(code) and not is_raw_return_boundary_context(context_stack) and not returns_raw_pointer(context_stack):
            issues.append(Issue(rel, line_no, 'error', 'DirectWrapperReturnGet', 'Route reviewed raw ABI/callback returns through a named Return* boundary instead of returning wrapper.get() directly.'))

        if WRAPPER_DYNAMIC_CAST_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'WrapperDynamicCast', 'Use wrapper dyn_cast() instead of unwrapping a smart pointer for dynamic_cast.'))

        if DIRECT_WRAPPER_RAW_GET_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'DirectWrapperRawGet', 'Keep wrapper narrowing named; avoid direct .as_ptr().get() / .as_nptr().get() chains.'))

        if rel.startswith('Source/Scripting/') and SCRIPT_VECTOR_RAW_HANDOFF_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'ScriptVectorRawHandoff', 'Use BorrowScriptVector*(), BorrowMutableScriptVector(), or ReleaseScriptOwnershipVector() for script-visible raw pointer vector handoffs.'))

        if rel == 'Source/Server/ClientDataValidation.cpp' and CLIENT_DATA_VALIDATION_INLINE_BYTE_EDGE_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'ClientDataValidationInlineByteEdge', 'Use local span-read helpers instead of inline data.data() + offset or string reinterpret_cast byte edges.'))

        if RAW_POINTER_RANGE_FOR_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'RawPointerRangeFor', 'Range-for over raw pointers is no longer allowed; use nptr<T> for nullable script/container input or ptr<T> for proven non-null wrapper containers.'))

        if 'CommandLineArgs' in text and DIRECT_COMMAND_LINE_ARG_RAW_USE_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'DirectCommandLineArgRawUse', 'Use CommandLineArgs::Get() and CommandLineArgs::IsOption() before reading CommandLineArgs elements.'))

        if is_strict_null_style_scope(rel) and NULLPTR_ASSERT_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'ExplicitNullptrAssert', 'Use wrapper/bool checks in assertions instead of explicit == nullptr / != nullptr spelling.'))

        if WRAPPER_DECL_NULLPTR_COMPARISON_RE.search(code) or WRAPPER_IF_INIT_NULLPTR_COMPARISON_RE.search(code) or WRAPPER_DYN_CAST_NULLPTR_COMPARISON_RE.search(code):
            issues.append(Issue(rel, line_no, 'error', 'WrapperNullptrComparison', 'Use wrapper bool conversion instead of comparing nptr/unique_nptr/unique_del_nptr/unique_arr_ptr/refcount_nptr/shared_ptr values with nullptr.'))

        match = DEFAULT_EMPTY_NONNULL_RE.search(line)
        if match is None and not is_class_member_context:
            match = PLAIN_DEFAULT_NONNULL_RE.search(line)
        if match is not None:
            nullable_kind = {
                'ptr': 'nptr',
                'unique_ptr': 'unique_nptr',
                'refcount_ptr': 'refcount_nptr',
            }[match.group('kind')]
            issues.append(Issue(rel, line_no, 'error', 'DefaultEmptyNonNullPointer', f'Use {nullable_kind}<T> for normal empty state.'))

        if rel not in INTERNAL_REFCOUNT_FACTORY_FILES:
            if has_direct_refcount_construction(line):
                issues.append(Issue(rel, line_no, 'error', 'DirectRefcountConstruction', 'Use hold_ref(), try_hold_ref(), from_addref(), or from_adopted_ref().'))
            if DIRECT_REFCOUNT_ADOPT_RE.search(line) or CTAD_REFCOUNT_ADOPT_RE.search(line):
                issues.append(Issue(rel, line_no, 'error', 'DirectRefcountAdopt', 'Use from_adopted_ref() for no-AddRef ownership transfer.'))

        for name, kind in visible_local_names.items():
            escaped_name = re.escape(name)

            nullable_operation = ''

            if re.search(rf'\b{escaped_name}\s*(?:==|!=)\s*nullptr|nullptr\s*(?:==|!=)\s*\b{escaped_name}\b', code):
                nullable_operation = 'nullptr comparison'
            elif re.search(rf'\b(?:if|while)\s*\(\s*!?\s*{escaped_name}\s*(?:\)|&&|\|\|)', code):
                nullable_operation = 'bool check'
            elif re.search(rf'\b{escaped_name}\s*\.\s*reset\s*\(\s*(?:nullptr)?\s*\)', code):
                nullable_operation = 'empty reset'
            elif re.search(rf'\b{escaped_name}\s*\.\s*get_pp\s*\(', code):
                nullable_operation = 'get_pp'
            elif kind == 'unique_ptr' and re.search(rf'\b{escaped_name}\s*\.\s*release\s*\(', code):
                nullable_operation = 'lvalue release'
            elif kind in {'ptr', 'unique_ptr', 'refcount_ptr'} and re.search(rf'\b{escaped_name}\s*=\s*nullptr\s*;', code):
                nullable_operation = 'nullptr assignment'

            if nullable_operation and kind in {'ptr', 'unique_ptr', 'refcount_ptr'}:
                issues.append(Issue(rel, line_no, 'error', 'NullableOperationOnNonNullPointer', f'Local {kind}<T> variable `{name}` uses nullable operation: {nullable_operation}. Use the nullable n* type or remove the operation.'))

            if nullable_operation == 'nullptr comparison' and kind in {'nptr', 'unique_nptr', 'unique_del_nptr', 'unique_arr_ptr', 'refcount_nptr', 'shared_ptr'} and (is_strict_wrapper_null_style_scope(rel) or 'FO_RUNTIME_ASSERT' in code):
                issues.append(Issue(rel, line_no, 'error', 'WrapperNullptrComparison', f'Local {kind}<T> variable `{name}` is compared with nullptr. Use wrapper bool conversion instead.'))

            if kind in {'nptr', 'unique_nptr', 'refcount_nptr'} and re.search(rf'\b{escaped_name}\s*\?\s*{escaped_name}\s*->', code):
                issues.append(Issue(rel, line_no, 'error', 'NullableTernaryDereference', f'Local {kind}<T> variable `{name}` is dereferenced directly in a ternary. Bind a checked non-null local before use.'))

            if kind in {'refcount_ptr', 'refcount_nptr'}:
                assign_match = re.search(rf'\b{escaped_name}\s*=\s*(?P<rhs>[^;]+);', code)

                if assign_match is not None:
                    rhs = assign_match.group('rhs').strip()
                    raw_assignment = classify_refcount_rhs(kind, rhs, visible_local_names, visible_raw_local_names)

                    if raw_assignment:
                        issues.append(Issue(rel, line_no, 'error', 'RawRefcountAssignment', f'Local {kind}<T> variable `{name}` receives raw pointer RHS: {raw_assignment}. Use hold_ref(), try_hold_ref(), from_addref(), or from_adopted_ref().'))

        if rel not in INTERNAL_REFCOUNT_FACTORY_FILES:
            for decl_init_match in REFCOUNT_DECL_INIT_RE.finditer(code):
                kind = decl_init_match.group('kind')
                rhs = decl_init_match.group('rhs').strip()
                if decl_init_match.group('initializer') == '{' and rhs.endswith('}'):
                    rhs = rhs[:-1].rstrip()
                raw_assignment = classify_refcount_rhs(kind, rhs, visible_local_names, visible_raw_local_names)

                if raw_assignment:
                    issues.append(Issue(rel, line_no, 'error', 'RawRefcountInitialization', f'Local {kind}<T> variable `{decl_init_match.group("name")}` is initialized from raw pointer RHS: {raw_assignment}. Use hold_ref(), try_hold_ref(), from_addref(), or from_adopted_ref().'))

        if not is_class_member_context and any(context.startswith('function:') for context in context_stack):
            for raw_decl_match in RAW_LOCAL_DECL_RE.finditer(code):
                raw_local_scopes[-1].add(raw_decl_match.group('name'))

            for decl_match in LOCAL_NONNULL_DECL_RE.finditer(code):
                after_decl = code[decl_match.end():]
                statement_end_candidates = [after_decl.find(marker) for marker in (';', '=', ',', '{') if after_decl.find(marker) != -1]
                first_statement_end = min(statement_end_candidates) if statement_end_candidates else -1
                first_paren = after_decl.find('(')

                if first_paren != -1 and (first_statement_end == -1 or first_paren < first_statement_end):
                    continue

                local_scopes[-1][decl_match.group('name')] = decl_match.group('kind')

            for decl_match in LOCAL_NULLABLE_DECL_RE.finditer(code):
                after_decl = code[decl_match.end():]
                statement_end_candidates = [after_decl.find(marker) for marker in (';', '=', ',', '{') if after_decl.find(marker) != -1]
                first_statement_end = min(statement_end_candidates) if statement_end_candidates else -1
                first_paren = after_decl.find('(')

                if first_paren != -1 and (first_statement_end == -1 or first_paren < first_statement_end):
                    continue

                local_scopes[-1][decl_match.group('name')] = decl_match.group('kind')

        if pending_context == '':
            if CLASS_START_RE.search(code):
                pending_context = 'class'
            else:
                pending_context = detect_function_context(code)

        for char in code:
            if char == '{':
                context_stack.append(pending_context or 'other')
                local_scopes.append({})
                raw_local_scopes.append(set())
                pending_context = ''
            elif char == '}':
                if context_stack:
                    context_stack.pop()
                if local_scopes:
                    local_scopes.pop()
                if raw_local_scopes:
                    raw_local_scopes.pop()

        if pending_context != '' and ';' in code:
            pending_context = ''

    return issues


def inventory_text(rel: str, text: str, options: SmartPointerOptions = DEFAULT_OPTIONS) -> tuple[list[InventoryRow], list[InventoryDetail]]:
    counters: Counter[str] = Counter()
    details: list[InventoryDetail] = []
    vocabulary_text = '\n'.join(line for line in text.splitlines() if not VOCABULARY_INVENTORY_SKIP_LINE_RE.search(line))

    for metric, pattern in VOCABULARY_PATTERNS.items():
        counters[metric] = len(pattern.findall(vocabulary_text))

    in_block_comment = False
    context_stack: list[str] = []
    pending_context = ''

    for line_no, line in enumerate(text.splitlines(), start=1):
        code, in_block_comment = strip_comments_from_line(line, in_block_comment)
        is_class_member_context = bool(context_stack and context_stack[-1] == 'class')
        detail_code = code.strip().replace('\t', '    ')

        if not VOCABULARY_INVENTORY_SKIP_LINE_RE.search(code):
            for nullable_unique_metric in ('unique_nptr', 'unique_del_nptr'):
                nullable_unique_count = len(VOCABULARY_PATTERNS[nullable_unique_metric].findall(code))

                for _ in range(nullable_unique_count):
                    details.append(InventoryDetail(rel, line_no, nullable_unique_metric, detail_code))

        if rel == PROPERTY_SERIALIZER_PATH and PROPERTY_SERIALIZER_RAW_CURSOR_RE.search(code):
            counters[PROPERTY_SERIALIZER_RAW_CURSOR_METRIC] += 1
            details.append(InventoryDetail(rel, line_no, PROPERTY_SERIALIZER_RAW_CURSOR_METRIC, detail_code))
        if rel == PROPERTY_SERIALIZER_PATH and PROPERTY_SERIALIZER_INLINE_BYTE_EDGE_RE.search(code):
            counters[PROPERTY_SERIALIZER_INLINE_BYTE_EDGE_METRIC] += 1
            details.append(InventoryDetail(rel, line_no, PROPERTY_SERIALIZER_INLINE_BYTE_EDGE_METRIC, detail_code))
        if rel == DATA_SERIALIZATION_PATH and DATA_SERIALIZATION_INLINE_READ_EDGE_RE.search(code):
            counters[DATA_SERIALIZATION_INLINE_READ_EDGE_METRIC] += 1
            details.append(InventoryDetail(rel, line_no, DATA_SERIALIZATION_INLINE_READ_EDGE_METRIC, detail_code))
        if rel == DATA_SERIALIZATION_PATH and DATA_SERIALIZATION_INLINE_WRITE_EDGE_RE.search(code):
            counters[DATA_SERIALIZATION_INLINE_WRITE_EDGE_METRIC] += 1
            details.append(InventoryDetail(rel, line_no, DATA_SERIALIZATION_INLINE_WRITE_EDGE_METRIC, detail_code))
        if rel != DATA_SERIALIZATION_PATH and DATA_WRITER_WRITE_PTR_CALL_RE.search(code):
            if is_test_scope(rel):
                if not DATA_WRITER_WRITE_PTR_ZERO_SIZE_CALL_RE.search(code):
                    counters[DATA_WRITER_WRITE_PTR_TEST_PAYLOAD_CALL_METRIC] += 1
                    details.append(InventoryDetail(rel, line_no, DATA_WRITER_WRITE_PTR_TEST_PAYLOAD_CALL_METRIC, detail_code))
            else:
                counters[DATA_WRITER_WRITE_PTR_PRODUCTION_CALL_METRIC] += 1
                details.append(InventoryDetail(rel, line_no, DATA_WRITER_WRITE_PTR_PRODUCTION_CALL_METRIC, detail_code))
        if rel != DATA_SERIALIZATION_PATH and not is_test_scope(rel) and DATA_READER_READ_PTR_CALL_RE.search(code):
            counters[DATA_READER_READ_PTR_PRODUCTION_CALL_METRIC] += 1
            details.append(InventoryDetail(rel, line_no, DATA_READER_READ_PTR_PRODUCTION_CALL_METRIC, detail_code))
        if rel != DATA_SERIALIZATION_TEST_PATH and is_test_scope(rel) and DATA_READER_READ_PTR_CALL_RE.search(code):
            counters[DATA_READER_READ_PTR_NON_API_TEST_CALL_METRIC] += 1
            details.append(InventoryDetail(rel, line_no, DATA_READER_READ_PTR_NON_API_TEST_CALL_METRIC, detail_code))
        if FILE_READER_COPY_DATA_CALL_RE.search(code) and not is_reviewed_file_reader_copy_data_call(rel, detail_code):
            counters[FILE_READER_COPY_DATA_PAYLOAD_CALL_METRIC] += 1
            details.append(InventoryDetail(rel, line_no, FILE_READER_COPY_DATA_PAYLOAD_CALL_METRIC, detail_code))
        if RAW_POINTER_MARKER_RE.search(code):
            if options.is_abi_line(code):
                counters['raw_pointer_abi_marker'] += 1
                details.append(InventoryDetail(rel, line_no, 'raw_pointer_abi_marker', detail_code))
            elif is_test_scope(rel):
                counters['raw_pointer_test_marker'] += 1
                details.append(InventoryDetail(rel, line_no, 'raw_pointer_test_marker', detail_code))
            elif RAW_POINTER_LOW_LEVEL_LINE_RE.search(code):
                counters['raw_pointer_low_level_marker'] += 1
                details.append(InventoryDetail(rel, line_no, 'raw_pointer_low_level_marker', detail_code))
            else:
                counters['raw_pointer_marker'] += 1
                details.append(InventoryDetail(rel, line_no, 'raw_pointer_marker', detail_code))
        if RAW_AUTO_POINTER_MARKER_RE.search(code):
            if options.is_abi_line(code):
                counters['raw_auto_pointer_abi_marker'] += 1
                details.append(InventoryDetail(rel, line_no, 'raw_auto_pointer_abi_marker', detail_code))
            elif is_test_scope(rel):
                counters['raw_auto_pointer_test_marker'] += 1
                details.append(InventoryDetail(rel, line_no, 'raw_auto_pointer_test_marker', detail_code))
            elif RAW_POINTER_LOW_LEVEL_LINE_RE.search(code):
                counters['raw_auto_pointer_low_level_marker'] += 1
                details.append(InventoryDetail(rel, line_no, 'raw_auto_pointer_low_level_marker', detail_code))
            else:
                counters['raw_auto_pointer_marker'] += 1
                details.append(InventoryDetail(rel, line_no, 'raw_auto_pointer_marker', detail_code))
        if RAW_POINTER_CONTAINER_MARKER_RE.search(code):
            if options.is_abi_line(code) or RAW_POINTER_CONTAINER_ABI_LINE_RE.search(code):
                counters['raw_pointer_container_abi_marker'] += 1
                details.append(InventoryDetail(rel, line_no, 'raw_pointer_container_abi_marker', detail_code))
            else:
                counters['raw_pointer_container_marker'] += 1
                details.append(InventoryDetail(rel, line_no, 'raw_pointer_container_marker', detail_code))
        if DEFAULT_EMPTY_NONNULL_RE.search(code):
            counters['default_empty_nonnull_marker'] += 1
            details.append(InventoryDetail(rel, line_no, 'default_empty_nonnull_marker', detail_code))
        if not is_class_member_context and PLAIN_DEFAULT_NONNULL_RE.search(code):
            counters['plain_default_nonnull_marker'] += 1
            details.append(InventoryDetail(rel, line_no, 'plain_default_nonnull_marker', detail_code))
        if is_class_member_context and REFERENCE_MEMBER_RE.search(code):
            counters['reference_member_marker'] += 1
            details.append(InventoryDetail(rel, line_no, 'reference_member_marker', detail_code))
        if rel not in INTERNAL_REFCOUNT_FACTORY_FILES and has_direct_refcount_construction(code):
            counters['direct_refcount_construction_marker'] += 1
            details.append(InventoryDetail(rel, line_no, 'direct_refcount_construction_marker', detail_code))

        wrapper_static_cast_count = len(WRAPPER_STATIC_CAST_RE.findall(code))
        wrapper_const_cast_count = len(WRAPPER_CONST_CAST_RE.findall(code))
        wrapper_reinterpret_cast_count = len(WRAPPER_REINTERPRET_CAST_RE.findall(code))
        counters['wrapper_static_cast_marker'] += wrapper_static_cast_count
        counters['wrapper_const_cast_marker'] += wrapper_const_cast_count
        counters['wrapper_reinterpret_cast_marker'] += wrapper_reinterpret_cast_count

        for _ in range(wrapper_static_cast_count):
            details.append(InventoryDetail(rel, line_no, 'wrapper_static_cast_marker', detail_code))
        for _ in range(wrapper_const_cast_count):
            details.append(InventoryDetail(rel, line_no, 'wrapper_const_cast_marker', detail_code))
        for _ in range(wrapper_reinterpret_cast_count):
            details.append(InventoryDetail(rel, line_no, 'wrapper_reinterpret_cast_marker', detail_code))

        if pending_context == '' and CLASS_START_RE.search(code):
            pending_context = 'class'

        for char in code:
            if char == '{':
                context_stack.append(pending_context or 'other')
                pending_context = ''
            elif char == '}':
                if context_stack:
                    context_stack.pop()

        if pending_context != '' and ';' in code:
            pending_context = ''

    return [InventoryRow(rel, metric, count) for metric, count in sorted(counters.items()) if count != 0], details



def report_unreviewed_reference_members(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric == 'reference_member_marker':
            issues.append(Issue(detail.path, detail.line, 'error', 'ReferenceMemberUnreviewed', f'Class reference member is not allowed; store required dependencies as values where possible or as reviewed ptr<T> members: {detail.code}'))

    return issues

def report_header_raw_pointer_abi_markers(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric in HEADER_RAW_ABI_METRICS and is_header_path(detail.path) and not RAW_POINTER_HEADER_ABI_LINE_RE.search(detail.code):
            issues.append(Issue(detail.path, detail.line, 'error', 'RawPointerHeaderAbi', f'Public/header raw pointer ABI marker must be moved behind a .cpp boundary, or be a named C ABI shape in RAW_POINTER_HEADER_ABI_LINE_RE: {detail.code}'))

    return issues


def report_raw_auto_pointer_markers(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric in RAW_AUTO_POINTER_METRICS:
            issues.append(Issue(detail.path, detail.line, 'error', 'RawAutoPointerMarker', f'Raw auto pointer declarations are no longer allowed; use ptr/nptr wrappers or spell the required ABI/native pointer type explicitly: {detail.code}'))

    return issues


def report_wrapper_raw_cast_markers(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric in WRAPPER_RAW_CAST_METRICS:
            issues.append(Issue(detail.path, detail.line, 'error', 'WrapperRawCastMarker', f'Wrapper-to-raw casts must stay behind named helpers or use existing wrapper/cast helpers instead of direct .get() casts: {detail.code}'))

    return issues


def report_property_serializer_raw_cursor_markers(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric == PROPERTY_SERIALIZER_RAW_CURSOR_METRIC:
            issues.append(Issue(detail.path, detail.line, 'error', 'PropertySerializerRawCursorMarker', f'PropertiesSerializer byte parsing must stay on span/cursor helpers instead of raw advancing uint8_t cursors: {detail.code}'))

    return issues


def report_property_serializer_inline_byte_edge_markers(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric == PROPERTY_SERIALIZER_INLINE_BYTE_EDGE_METRIC:
            issues.append(Issue(detail.path, detail.line, 'error', 'PropertySerializerInlineByteEdgeMarker', f'PropertiesSerializer byte/string handoffs must stay behind named helpers instead of inline .data() or reinterpret_cast byte edges: {detail.code}'))

    return issues


def report_data_serialization_inline_read_edge_markers(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric == DATA_SERIALIZATION_INLINE_READ_EDGE_METRIC:
            issues.append(Issue(detail.path, detail.line, 'error', 'DataSerializationInlineReadEdgeMarker', f'DataSerialization reader byte boundaries must stay behind read_bytes()/read_bytes_ptr() instead of inline _data_buf.data() + _read_pos or reinterpret_cast edges: {detail.code}'))

    return issues


def report_data_serialization_inline_write_edge_markers(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric == DATA_SERIALIZATION_INLINE_WRITE_EDGE_METRIC:
            issues.append(Issue(detail.path, detail.line, 'error', 'DataSerializationInlineWriteEdgeMarker', f'DataSerialization writer byte boundaries must stay behind append_bytes()/write_bytes_ptr() instead of inline _data_buf->data() memory::copy destinations: {detail.code}'))

    return issues


def report_data_writer_write_ptr_production_call_markers(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric == DATA_WRITER_WRITE_PTR_PRODUCTION_CALL_METRIC:
            issues.append(Issue(detail.path, detail.line, 'error', 'DataWriterWritePtrProductionCallMarker', f'Production data_writer payload writes should use write_bytes(), write_string_bytes(), write_object_array(), or a named raw helper instead of direct write_ptr(): {detail.code}'))

    return issues


def report_data_writer_write_ptr_test_payload_call_markers(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric == DATA_WRITER_WRITE_PTR_TEST_PAYLOAD_CALL_METRIC:
            issues.append(Issue(detail.path, detail.line, 'error', 'DataWriterWritePtrTestPayloadCallMarker', f'Test data_writer payload writes should use write_bytes(), write_string_bytes(), or write_object_array(); keep direct write_ptr() only for explicit zero-size raw escape coverage: {detail.code}'))

    return issues


def report_data_reader_read_ptr_production_call_markers(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric == DATA_READER_READ_PTR_PRODUCTION_CALL_METRIC:
            issues.append(Issue(detail.path, detail.line, 'error', 'DataReaderReadPtrProductionCallMarker', f'Production data_reader payload reads should use read_bytes(), read_string_bytes(), read_object_array(), or a named raw helper instead of direct read_ptr(): {detail.code}'))

    return issues


def report_data_reader_read_ptr_non_api_test_call_markers(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric == DATA_READER_READ_PTR_NON_API_TEST_CALL_METRIC:
            issues.append(Issue(detail.path, detail.line, 'error', 'DataReaderReadPtrNonApiTestCallMarker', f'Test data_reader payload reads should use read_bytes(), read_string_bytes(), or read_object_array(); keep direct read_ptr() only in Test_DataSerialization API coverage: {detail.code}'))

    return issues


def report_file_reader_copy_data_payload_call_markers(details: list[InventoryDetail]) -> list[Issue]:
    issues: list[Issue] = []

    for detail in details:
        if detail.metric == FILE_READER_COPY_DATA_PAYLOAD_CALL_METRIC:
            issues.append(Issue(detail.path, detail.line, 'error', 'FileReaderCopyDataPayloadCallMarker', f'FileReader payload reads should use ReadBytes(), ReadObject(), ReadObjectArray(), or a reviewed raw edge helper instead of direct CopyData(): {detail.code}'))

    return issues


def write_inventory(path: Path, rows: list[InventoryRow]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open('w', encoding='utf-8', newline='\n') as file:
        file.write('path\tmetric\tcount\n')
        for row in rows:
            file.write(f'{row.path}\t{row.metric}\t{row.count}\n')


def write_inventory_details(path: Path, details: list[InventoryDetail]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open('w', encoding='utf-8', newline='\n') as file:
        file.write('path\tline\tmetric\tcode\n')
        for detail in details:
            file.write(f'{detail.path}\t{detail.line}\t{detail.metric}\t{detail.code}\n')


def inventory_hotspots(rows: list[InventoryRow], limit: int) -> dict[str, list[dict[str, object]]]:
    if limit <= 0:
        return {}

    rows_by_metric: dict[str, list[InventoryRow]] = {}

    for row in rows:
        rows_by_metric.setdefault(row.metric, []).append(row)

    return {
        metric: [
            {
                'path': row.path,
                'count': row.count,
            }
            for row in sorted(metric_rows, key=lambda item: (-item.count, item.path))[:limit]
        ]
        for metric, metric_rows in sorted(rows_by_metric.items())
    }


def write_summary(path: Path, audited_files: int, issues: list[Finding], rows: list[InventoryRow]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    issue_counters = Counter(issue.severity for issue in issues)
    rule_counters = Counter(issue.rule for issue in issues)
    inventory_counters = Counter()

    for row in rows:
        inventory_counters[row.metric] += row.count

    summary = {
        'version': 3,
        'audited_files': audited_files,
        'issue_counts': {
            'error': issue_counters['error'],
            'warning': issue_counters['warning'],
            'info': issue_counters['info'],
        },
        'rule_counts': dict(sorted(rule_counters.items())),
        'inventory_counts': dict(sorted(inventory_counters.items())),
        'inventory_hotspots': inventory_hotspots(rows, DEFAULT_SUMMARY_HOTSPOT_LIMIT),
    }

    path.write_text(json.dumps(summary, indent=2, sort_keys=True) + '\n', encoding='utf-8', newline='\n')



INVENTORY_REPORTS = (
    report_raw_auto_pointer_markers,
    report_wrapper_raw_cast_markers,
    report_property_serializer_raw_cursor_markers,
    report_property_serializer_inline_byte_edge_markers,
    report_data_serialization_inline_read_edge_markers,
    report_data_serialization_inline_write_edge_markers,
    report_data_writer_write_ptr_production_call_markers,
    report_data_writer_write_ptr_test_payload_call_markers,
    report_data_reader_read_ptr_production_call_markers,
    report_data_reader_read_ptr_non_api_test_call_markers,
    report_file_reader_copy_data_payload_call_markers,
    report_unreviewed_reference_members,
    report_header_raw_pointer_abi_markers,
)


@dataclass
class SmartPointerResult:
    findings: list[Finding] = field(default_factory=list)
    rows: list[InventoryRow] = field(default_factory=list)
    details: list[InventoryDetail] = field(default_factory=list)


def options_from_config(context: AuditContext) -> SmartPointerOptions:
    section = context.config.get('smart_pointer', {})
    unknown = sorted(set(section) - {'raw_return_guarded_files', 'abi_line_patterns', 'nullable_factory_names'})

    if unknown:
        raise AuditUsageError(f'Unknown smart_pointer configuration keys: {", ".join(unknown)}')

    for key in ('raw_return_guarded_files', 'abi_line_patterns', 'nullable_factory_names'):
        value = section.get(key, [])

        if not isinstance(value, list) or not all(isinstance(item, str) and item for item in value):
            raise AuditUsageError(f'smart_pointer.{key} must be a list of non-empty strings')

    guarded = frozenset(context.policy_key(context.root / item) for item in section.get('raw_return_guarded_files', []))

    try:
        abi_patterns = section.get('abi_line_patterns', [])
        abi_line_re = re.compile('|'.join(f'(?:{pattern})' for pattern in abi_patterns)) if abi_patterns else None
        factory_names = section.get('nullable_factory_names', [])
        factory_re = build_nullable_factory_unique_del_ptr_re('|'.join(f'(?:{name})' for name in factory_names)) if factory_names else None
    except re.error as ex:
        raise AuditUsageError(f'Invalid smart_pointer regular expression: {ex}') from ex

    return SmartPointerOptions(guarded, abi_line_re, factory_re)


def audit_source(rel: str, display: str, text: str, options: SmartPointerOptions = DEFAULT_OPTIONS) -> SmartPointerResult:
    """Audit one file text; *rel* is its policy key and *display* the reported path."""

    issues = audit_text(rel, text, options)
    rows, details = inventory_text(rel, text, options)

    for report in INVENTORY_REPORTS:
        issues.extend(report(details))

    lines = text.splitlines()
    findings = [
        Finding(display, issue.line, issue.severity, issue.rule, issue.message, lines[issue.line - 1] if 0 < issue.line <= len(lines) else '')
        for issue in issues
    ]
    display_rows = [InventoryRow(display, row.metric, row.count) for row in rows]
    display_details = [InventoryDetail(display, detail.line, detail.metric, detail.code) for detail in details]
    return SmartPointerResult(findings, display_rows, display_details)


def audit_file(context: AuditContext, path: Path, options: SmartPointerOptions = DEFAULT_OPTIONS) -> SmartPointerResult:
    display = context.display_path(path)

    try:
        text = path.read_text(encoding='utf-8-sig', errors='replace')
    except OSError as ex:
        return SmartPointerResult([Finding(display, 1, 'error', 'ReadError', str(ex))])

    return audit_source(context.policy_key(path), display, text, options)


def collect(context: AuditContext, files: list[Path], options: SmartPointerOptions, jobs: int = 1) -> SmartPointerResult:
    """Audit *files* in order; with *jobs* > 1 files are audited in worker processes."""

    result = SmartPointerResult()

    if jobs > 1 and len(files) > 1:
        with ProcessPoolExecutor(max_workers=min(jobs, len(files))) as executor:
            file_results = list(executor.map(audit_file, repeat(context), files, repeat(options), chunksize=8))
    else:
        file_results = [audit_file(context, path, options) for path in files]

    for file_result in file_results:
        result.findings.extend(file_result.findings)
        result.rows.extend(file_result.rows)
        result.details.extend(file_result.details)

    return result
