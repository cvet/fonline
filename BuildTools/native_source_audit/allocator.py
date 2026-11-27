"""Allocator coverage audit.

Engine code allocates through the memory-system vocabulary: the container aliases from
``Source/Essentials/Containers.h`` (backed by ``safe_allocator``) and the ``safe_alloc::*_raw`` tier for C-ABI
boundaries.  Standard-library containers, allocating ``std::`` string helpers and bare C allocation calls
bypass the terminate-on-out-of-memory contract, so this audit reports them.  Reviewed sites are recorded in
a baseline; comments and string/character literals are blanked before matching.
"""

from __future__ import annotations

import re
from pathlib import Path

from native_source_audit.common import AuditContext, Finding, normalize_code


NAME = 'allocator'

# Containers.h defines the aliases in terms of the standard types and MemorySystem owns the primitives
ALLOCATOR_OWNER_FILES = frozenset({
    'Source/Essentials/Containers.h',
    'Source/Essentials/MemorySystem.h',
    'Source/Essentials/MemorySystem.cpp',
})

STD_CONTAINER_TYPES = (
    'basic_string',
    'deque',
    'istringstream',
    'list',
    'map',
    'multimap',
    'multiset',
    'ostringstream',
    'set',
    'string',
    'stringstream',
    'unordered_map',
    'unordered_multimap',
    'unordered_set',
    'vector',
    'wstring',
)

STD_CONTAINER_RE = re.compile(r'\bstd::(' + '|'.join(STD_CONTAINER_TYPES) + r')\b')

# std::format_to / std::vformat_to write into a caller-provided iterator and are the supported form
STD_ALLOCATING_CALL_RE = re.compile(r'\bstd::(to_string|to_wstring|format|vformat)\s*\(')

RAW_C_ALLOCATION_RE = re.compile(
    r'(?<![\w:.>])(malloc|calloc|realloc|free|strdup|_strdup|posix_memalign|aligned_alloc|_aligned_malloc|_aligned_realloc|_aligned_free)\s*\('
)

RULE_STD_CONTAINER = 'StdContainerType'
RULE_STD_ALLOCATING_CALL = 'StdAllocatingCall'
RULE_RAW_C_ALLOCATION = 'RawCAllocation'

RULES = (
    (RULE_STD_CONTAINER, STD_CONTAINER_RE),
    (RULE_STD_ALLOCATING_CALL, STD_ALLOCATING_CALL_RE),
    (RULE_RAW_C_ALLOCATION, RAW_C_ALLOCATION_RE),
)


def strip_comments_and_literals(text: str) -> list[str]:
    """Blank out comments and string/char literals so matches only come from real code.

    Removed spans become spaces rather than being deleted, so line and column positions stay intact.
    """

    out = list(text)
    length = len(text)
    i = 0
    state = 'code'

    while i < length:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < length else ''

        if state == 'code':
            if ch == '/' and nxt == '/':
                state = 'line_comment'
                out[i] = out[i + 1] = ' '
                i += 2
                continue
            if ch == '/' and nxt == '*':
                state = 'block_comment'
                out[i] = out[i + 1] = ' '
                i += 2
                continue
            if ch == '"':
                state = 'string'
                out[i] = ' '
                i += 1
                continue
            if ch == "'":
                state = 'char'
                out[i] = ' '
                i += 1
                continue
            i += 1
            continue

        if state == 'line_comment':
            if ch == '\n':
                state = 'code'
                i += 1
                continue
            out[i] = ' '
            i += 1
            continue

        if state == 'block_comment':
            if ch == '*' and nxt == '/':
                state = 'code'
                out[i] = out[i + 1] = ' '
                i += 2
                continue
            if ch != '\n':
                out[i] = ' '
            i += 1
            continue

        if ch == '\\':
            out[i] = ' '
            if i + 1 < length and text[i + 1] != '\n':
                out[i + 1] = ' '
                i += 2
                continue
            i += 1
            continue

        if (state == 'string' and ch == '"') or (state == 'char' and ch == "'"):
            state = 'code'
            out[i] = ' '
            i += 1
            continue

        if ch == '\n':
            # Unterminated literal; recover at the newline rather than swallowing the file
            state = 'code'
            i += 1
            continue

        out[i] = ' '
        i += 1

    return ''.join(out).splitlines()


def scan_text(display: str, text: str) -> list[Finding]:
    stripped = strip_comments_and_literals(text)
    raw_lines = text.splitlines()
    findings: list[Finding] = []

    for index, line in enumerate(stripped):
        code = raw_lines[index].strip() if index < len(raw_lines) else line.strip()

        for rule, pattern in RULES:
            if pattern.search(line):
                normalized = normalize_code(code)
                findings.append(Finding(display, index + 1, 'error', rule, f'Allocation bypasses the engine memory system: {normalized}', normalized))

    return findings


def scan_file(context: AuditContext, path: Path) -> list[Finding]:
    if context.policy_key(path) in ALLOCATOR_OWNER_FILES:
        return []

    display = context.display_path(path)

    try:
        text = path.read_text(encoding='utf-8-sig', errors='replace')
    except OSError as ex:
        return [Finding(display, 1, 'error', 'ReadError', str(ex))]

    return scan_text(display, text)


def collect(context: AuditContext, files: list[Path]) -> list[Finding]:
    findings: list[Finding] = []

    for path in files:
        findings.extend(scan_file(context, path))

    return findings
