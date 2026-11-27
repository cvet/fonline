"""Lightweight C++ source masking and function-definition discovery.

Shared by the native safety audits. The parser does not evaluate the
preprocessor and does not build an AST: it blanks comments and literal
contents while preserving offsets, finds function bodies by brace shape and
by the ``FO_TRACE_ZONE`` / ``FO_SCRIPT_API`` conventions, and hashes bodies
with comments and zone statements removed.
"""

from __future__ import annotations

import dataclasses
import hashlib
import re

MARKER_RE = re.compile(r'^\s*FO_TRACE_ZONE(?:_NAMED)?\(\s*[A-Za-z]\w*\s*(?:,.*)?\);\s*$')
# A zone statement anywhere in a body is blanked before hashing: instrumentation does not change behavior
ZONE_STATEMENT_RE = re.compile(r'\bFO_TRACE_ZONE(?:_NAMED)?\s*\([^;]*\)\s*;')
EXPORT_RE = re.compile(r'^FO_SCRIPT_API\b')
DEFINE_RE = re.compile(r'^\s*#\s*define\s+(\w+)')
# A statement-ish line that is just an ALL_CAPS macro invocation (FO_BEGIN_NAMESPACE,
# FO_GCC_IGNORE_WARNINGS_PUSH(...), FO_GLOBAL_DATA(...);) - terminates signature walk-up
UPPER_MACRO_LINE_RE = re.compile(r'^[A-Z_][A-Z0-9_]*\s*(?:\(.*\))?\s*;?$')
# __declspec(...) before a name is an attribute, not the function parameter list
DECLSPEC_RE = re.compile(r'__declspec\s*\([^()]*\)')


@dataclasses.dataclass
class FuncEntry:
    file: str
    kind: str
    function: str
    signature: str
    body_hash: str
    level: str = 'Unclassified'
    status: str = 'unclassified'
    reason: str = ''
    review_note: str = ''
    line: int = 0  # runtime only, not persisted
    body_off: int = -1  # runtime only, not persisted

    def key(self) -> tuple[str, str, str]:
        return (self.file, self.function, self.signature)


# ---------------------------------------------------------------------------
# Source parsing
# ---------------------------------------------------------------------------

def build_masks(text: str) -> tuple[str, str]:
    """Return (code_mask, hash_mask).

    code_mask: comments and string/char literal contents blanked -> safe for
    brace/paren matching. hash_mask: only comments blanked -> used for body
    hashing so string literals stay semantic while comments never matter.
    Newlines are preserved in both masks.
    """
    n = len(text)
    code = list(text)
    hashm = list(text)
    i = 0
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ''
        if c == '/' and nxt == '/':
            j = i
            while j < n:
                if text[j] == '\n':
                    previous = j - 2 if j > 0 and text[j - 1] == '\r' else j - 1
                    if previous < i or text[previous] != '\\':
                        break
                else:
                    code[j] = ' '
                    hashm[j] = ' '
                j += 1
            i = j
        elif c == '/' and nxt == '*':
            code[i] = code[i + 1] = ' '
            hashm[i] = hashm[i + 1] = ' '
            j = i + 2
            while j < n:
                if text[j] == '*' and j + 1 < n and text[j + 1] == '/':
                    code[j] = code[j + 1] = ' '
                    hashm[j] = hashm[j + 1] = ' '
                    j += 2
                    break
                if text[j] != '\n':
                    code[j] = ' '
                    hashm[j] = ' '
                j += 1
            i = j
        elif c == '"':
            if i > 0 and text[i - 1] == 'R':
                j = i + 1
                delim = ''
                while j < n and text[j] != '(':
                    delim += text[j]
                    j += 1
                closer = ')' + delim + '"'
                end = text.find(closer, j)
                end = (end + len(closer)) if end != -1 else n
                for k in range(i + 1, min(end - 1, n)):
                    if text[k] != '\n':
                        code[k] = ' '
                i = end
            else:
                j = i + 1
                while j < n:
                    if text[j] == '\\':
                        j += 2
                        continue
                    if text[j] == '"' or text[j] == '\n':
                        j += 1
                        break
                    j += 1
                for k in range(i + 1, min(j - 1, n)):
                    if text[k] != '\n':
                        code[k] = ' '
                i = j
        elif c == "'":
            prev = text[i - 1] if i > 0 else ''
            if (prev.isalnum() or prev == '_') and nxt.isalnum():
                i += 1  # digit separator such as 1'000'000
                continue
            j = i + 1
            while j < n:
                if text[j] == '\\':
                    j += 2
                    continue
                if text[j] == "'" or text[j] == '\n':
                    j += 1
                    break
                j += 1
            for k in range(i + 1, min(j - 1, n)):
                if text[k] != '\n':
                    code[k] = ' '
            i = j
        else:
            i += 1
    return ''.join(code), ''.join(hashm)


def preprocessor_lines(lines: list[str]) -> set[int]:
    result: set[int] = set()
    continued = False
    for idx, line in enumerate(lines):
        if continued:
            result.add(idx)
            continued = line.rstrip().endswith('\\')
        elif line.lstrip().startswith('#'):
            result.add(idx)
            continued = line.rstrip().endswith('\\')
    return result


def line_offsets(text: str) -> list[int]:
    offsets = [0]
    for idx, ch in enumerate(text):
        if ch == '\n':
            offsets.append(idx + 1)
    return offsets


def offset_to_line(offsets: list[int], offset: int) -> int:
    lo, hi = 0, len(offsets) - 1
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if offsets[mid] <= offset:
            lo = mid
        else:
            hi = mid - 1
    return lo


def find_body_brace(code_mask: str, start: int) -> int:
    """Find the next '{' at paren depth 0 (skips default-argument braces like `= any_t {}`)."""
    depth = 0
    for i in range(start, len(code_mask)):
        c = code_mask[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
        elif c == '{' and depth == 0:
            return i
        elif c == ';' and depth == 0:
            return -1  # declaration, no body
    return -1


def match_brace(code_mask: str, open_off: int) -> int:
    """Return the offset of the '}' matching the '{' at open_off, or -1."""
    depth = 0
    for i in range(open_off, len(code_mask)):
        c = code_mask[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return i
    return -1


def normalize_ws(text: str) -> str:
    return ' '.join(text.split())


def hash_body(hash_mask: str, open_off: int, close_off: int) -> str:
    body = ZONE_STATEMENT_RE.sub(' ', hash_mask[open_off:close_off + 1])
    return hashlib.sha1(normalize_ws(body).encode('utf-8')).hexdigest()[:16]


def is_terminator_line(raw: str, code: str) -> bool:
    stripped_code = code.strip()
    stripped_raw = raw.strip()
    if stripped_raw == '':
        return True
    if stripped_code == '' and stripped_raw != '':
        return True  # pure comment line
    if stripped_code in ('{', '}', '};'):
        return True
    if stripped_code.endswith(';'):
        return True
    if UPPER_MACRO_LINE_RE.match(stripped_code):
        return True  # FO_BEGIN_NAMESPACE, FO_*_IGNORE_WARNINGS_PUSH(...), and friends
    if re.match(r'^(public|private|protected)\s*:$', stripped_code):
        return True  # access specifier before an inline class-body method
    return False


def collect_signature(lines: list[str], code_lines: list[str], hash_lines: list[str], pp: set[int], brace_line: int, brace_col: int,
                      transparent_pp: bool = False) -> tuple[list[int], str]:
    """Collect signature line indices for a body brace; returns (line_indices, text).

    Preprocessor lines stop the walk once something was collected; while the
    collection is still empty they are skipped, so a body brace that directly
    follows `#endif` (the dual `#if`-branch signature pattern in Applications/)
    still picks up the nearest branch's signature.
    """
    sig_indices: list[int] = []
    before_code = code_lines[brace_line][:brace_col]
    cur = brace_line - 1
    if before_code.strip():
        sig_indices.append(brace_line)
    while cur >= 0:
        if cur in pp:
            if sig_indices and not transparent_pp:
                break
            cur -= 1
            continue
        if is_terminator_line(lines[cur], code_lines[cur]):
            break
        sig_indices.insert(0, cur)
        cur -= 1
    parts: list[str] = []
    for idx in sig_indices:
        if idx == brace_line:
            parts.append(hash_lines[idx][:brace_col])
        else:
            parts.append(hash_lines[idx])
    return sig_indices, normalize_ws(' '.join(parts))


def extract_name(signature_code: str) -> str:
    signature_code = DECLSPEC_RE.sub('', signature_code)
    paren = signature_code.find('(')
    if paren == -1:
        return ''
    prefix = signature_code[:paren].rstrip()
    if 'operator' in prefix:
        pos = prefix.rindex('operator')
        start = pos
        while start > 0 and not prefix[start - 1].isspace():
            start -= 1
        name = prefix[start:]
        if prefix.endswith('operator'):
            name += '()'
        return name.lstrip('*&')
    # walk backwards, angle-bracket aware, to the start of the last token
    depth = 0
    end = len(prefix)
    start = end
    for i in range(end - 1, -1, -1):
        c = prefix[i]
        if c == '>':
            depth += 1
        elif c == '<':
            depth -= 1
        elif c.isspace() and depth == 0:
            start = i + 1
            break
        start = i
    return prefix[start:end].lstrip('*&')


@dataclasses.dataclass
class ParsedFile:
    text: str
    lines: list[str]
    code_mask: str
    hash_mask: str
    code_lines: list[str]
    hash_lines: list[str]
    pp: set[int]
    offsets: list[int]
    functions: list[FuncEntry]
    sig_start_to_func: dict[int, FuncEntry]
    body_open_line_to_func: dict[int, FuncEntry]
    definitions: list[FuncEntry]


CPP_KEYWORDS = {'if', 'for', 'while', 'switch', 'do', 'else', 'try', 'catch', 'return', 'sizeof', 'new', 'delete'}


def parse_file(rel_path: str, text: str) -> ParsedFile:
    lines = text.split('\n')
    code_mask, hash_mask = build_masks(text)
    code_lines = code_mask.split('\n')
    hash_lines = hash_mask.split('\n')
    pp = preprocessor_lines(lines)
    offsets = line_offsets(text)
    candidates: list[tuple[FuncEntry, int, int, int]] = []  # (entry, open_off, close_off, sig_start)
    claimed_braces: set[int] = set()

    def add_function(kind: str, brace_off: int) -> None:
        if brace_off in claimed_braces:
            return
        close_off = match_brace(code_mask, brace_off)
        if close_off == -1:
            return
        brace_line = offset_to_line(offsets, brace_off)
        brace_col = brace_off - offsets[brace_line]
        sig_indices, signature = collect_signature(lines, code_lines, hash_lines, pp, brace_line, brace_col)
        if not sig_indices or not signature:
            return
        sig_code_parts = []
        for idx in sig_indices:
            sig_code_parts.append(code_lines[idx][:brace_col] if idx == brace_line else code_lines[idx])
        sig_code = normalize_ws(' '.join(sig_code_parts))
        name = extract_name(sig_code)
        if not name or name.startswith('[') or name in CPP_KEYWORDS:
            return
        prefix = sig_code[:sig_code.find('(')] if '(' in sig_code else sig_code
        if '=' in prefix and 'operator' not in prefix:
            return  # assignment statement wrapping a lambda, not a definition
        entry = FuncEntry(
            file=rel_path, kind=kind, function=name, signature=signature,
            body_hash=hash_body(hash_mask, brace_off, close_off), line=sig_indices[0] + 1, body_off=brace_off)
        claimed_braces.add(brace_off)
        candidates.append((entry, brace_off, close_off, sig_indices[0]))

    for idx, line in enumerate(lines):
        if idx in pp:
            continue
        if MARKER_RE.match(line) and 'FO_TRACE_ZONE' in code_lines[idx]:
            brace_off = code_mask.rfind('{', 0, offsets[idx])
            if brace_off != -1:
                add_function('marker', brace_off)

    for idx, line in enumerate(lines):
        if idx in pp:
            continue
        if EXPORT_RE.match(line) and 'FO_SCRIPT_API' in code_lines[idx]:
            brace_off = find_body_brace(code_mask, offsets[idx])
            if brace_off != -1:
                add_function('export', brace_off)

    # Drop bodies nested inside another candidate's body: markers legitimately
    # appear inside thread-entry/job lambdas, and those are not functions we track
    spans = sorted((open_off, close_off) for _, open_off, close_off, _ in candidates)

    def is_nested(open_off: int, close_off: int) -> bool:
        for other_open, other_close in spans:
            if other_open < open_off and close_off < other_close:
                return True
        return False

    functions: list[FuncEntry] = []
    sig_start_to_func: dict[int, FuncEntry] = {}
    body_open_line_to_func: dict[int, FuncEntry] = {}
    for entry, open_off, close_off, sig_start in candidates:
        if is_nested(open_off, close_off):
            continue
        functions.append(entry)
        sig_start_to_func[sig_start] = entry
        body_open_line_to_func[offset_to_line(offsets, open_off)] = entry

    definitions = find_definitions(rel_path, lines, code_mask, hash_mask, code_lines, hash_lines, pp, offsets)
    return ParsedFile(text, lines, code_mask, hash_mask, code_lines, hash_lines, pp, offsets,
                      functions, sig_start_to_func, body_open_line_to_func, definitions)


# ---------------------------------------------------------------------------
# Markerless definition discovery
# ---------------------------------------------------------------------------

# Names before an opening parenthesis that never name a function
DEFINITION_KEYWORDS = CPP_KEYWORDS | {
    'constexpr', 'consteval', 'decltype', 'alignof', 'alignas', 'static_assert', 'noexcept',
    'defined', 'throw', 'typeid', 'co_await', 'co_return', 'co_yield', '__pragma', '_Pragma'}
TRAILER_WORD_RE = re.compile(r'(?:const|volatile|noexcept|override|final|mutable|try|throw)\b')
TRAILER_MACRO_RE = re.compile(r'[A-Z][A-Z0-9_]*\b')
INIT_MEMBER_RE = re.compile(r'[A-Za-z_][\w:]*')


def find_definitions(rel_path: str, lines: list[str], code_mask: str, hash_mask: str, code_lines: list[str],
                     hash_lines: list[str], pp: set[int], offsets: list[int]) -> list[FuncEntry]:
    """Return every function body outside other bodies, found by shape rather than by a zone marker.

    The scan enters namespace, class and initializer braces but skips a function body as a whole, so lambdas,
    local classes and control statements inside it never surface. Keys match the marker scan because the
    signature is collected the same way.
    """
    scope_chars = list(code_mask)
    for idx in pp:
        for off in range(offsets[idx], offsets[idx + 1] - 1 if idx + 1 < len(offsets) else len(scope_chars)):
            scope_chars[off] = ' '
    scope_mask = ''.join(scope_chars)

    definitions: list[FuncEntry] = []
    pos = 0
    paren_depth = 0
    while True:
        brace_off = scope_mask.find('{', pos)
        if brace_off == -1:
            break
        paren_depth += scope_mask.count('(', pos, brace_off) - scope_mask.count(')', pos, brace_off)
        pos = brace_off + 1
        if paren_depth > 0:
            continue  # a lambda or braced value inside an argument list
        entry = definition_at(rel_path, lines, code_mask, hash_mask, code_lines, hash_lines, pp, offsets, brace_off)
        if entry is not None:
            definitions.append(entry)
            close_off = match_brace(code_mask, brace_off)
            paren_depth += scope_mask.count('(', brace_off, close_off) - scope_mask.count(')', brace_off, close_off)
            pos = close_off + 1
    return definitions


def definition_at(rel_path: str, lines: list[str], code_mask: str, hash_mask: str, code_lines: list[str],
                  hash_lines: list[str], pp: set[int], offsets: list[int], brace_off: int) -> FuncEntry | None:
    brace_line = offset_to_line(offsets, brace_off)
    brace_col = brace_off - offsets[brace_line]
    before_code = code_lines[brace_line][:brace_col]
    if '(' in before_code:
        # A one-line definition carries its whole signature before the brace; walking up would join it with a
        # previous one-liner that ends with a brace rather than a terminator
        sig_indices = [brace_line]
        signature = normalize_ws(hash_lines[brace_line][:brace_col])
    else:
        sig_indices, signature = collect_signature(lines, code_lines, hash_lines, pp, brace_line, brace_col)
        entry = definition_from_signature(rel_path, code_mask, hash_mask, code_lines, offsets, brace_off, sig_indices, signature)
        if entry is not None or not sig_indices or sig_indices[0] - 1 not in pp:
            return entry
        # An initializer list split by #if continues above the directive
        sig_indices, signature = collect_signature(lines, code_lines, hash_lines, pp, brace_line, brace_col, transparent_pp=True)
    return definition_from_signature(rel_path, code_mask, hash_mask, code_lines, offsets, brace_off, sig_indices, signature)


def definition_from_signature(rel_path: str, code_mask: str, hash_mask: str, code_lines: list[str], offsets: list[int],
                              brace_off: int, sig_indices: list[int], signature: str) -> FuncEntry | None:
    if not sig_indices or not signature:
        return None
    brace_line = offset_to_line(offsets, brace_off)
    brace_col = brace_off - offsets[brace_line]
    sig_code = normalize_ws(' '.join(code_lines[idx][:brace_col] if idx == brace_line else code_lines[idx]
                                     for idx in sig_indices))
    # The __declspec(naked) parentheses are not a parameter list; the baseline key still carries the attribute
    sig_code = DECLSPEC_RE.sub('', sig_code).strip()
    name = extract_name(sig_code)
    if not name or name.startswith('[') or (']' in name and 'operator' not in name) or name in DEFINITION_KEYWORDS:
        return None
    prefix = sig_code[:sig_code.find('(')]
    if ('=' in prefix and 'operator' not in prefix) or any(ch in prefix for ch in '{};'):
        return None
    if re.fullmatch(r'[A-Z][A-Z0-9_]*', name) and prefix.strip() != name:
        return None  # an annotation macro on a member declaration with an initializer, like FO_TSA_GUARDED_BY(x) {}
    # The marker scan names a constrained template after its requires-clause; the key is kept and the clause skipped
    params = find_param_list(sig_code, sig_code.find('(') if name == 'requires' else -1)
    if params is None or classify_trailer(sig_code[params[1] + 1:]) != 'body':
        return None
    close_off = match_brace(code_mask, brace_off)
    if close_off == -1:
        return None
    return FuncEntry(file=rel_path, kind='markerless', function=name, signature=signature,
                     body_hash=hash_body(hash_mask, brace_off, close_off), line=sig_indices[0] + 1, body_off=brace_off)


def match_close(text: str, open_pos: int) -> int:
    """Return the index closing the bracket at open_pos (counting only brackets of the same kind), or -1."""
    opener = text[open_pos]
    closer = {'(': ')', '{': '}', '<': '>', '[': ']'}[opener]
    depth = 0
    for i in range(open_pos, len(text)):
        if text[i] == opener:
            depth += 1
        elif text[i] == closer:
            depth -= 1
            if depth == 0:
                return i
    return -1


def find_param_list(sig_code: str, skip_group: int = -1) -> tuple[int, int] | None:
    search_from = 0
    if skip_group != -1:
        skipped_close = match_close(sig_code, skip_group)
        if skipped_close == -1:
            return None
        search_from = skipped_close + 1
    open_pos = sig_code.find('(', search_from)
    if open_pos == -1:
        return None
    if sig_code[:open_pos].rstrip().endswith('operator'):
        # For operator() the first parentheses belong to the name
        name_close = match_close(sig_code, open_pos)
        open_pos = sig_code.find('(', name_close) if name_close != -1 else -1
        if open_pos == -1:
            return None
    close_pos = match_close(sig_code, open_pos)
    return (open_pos, close_pos) if close_pos != -1 else None


def skip_space(text: str, pos: int) -> int:
    while pos < len(text) and text[pos].isspace():
        pos += 1
    return pos


def classify_trailer(trailer: str) -> str:
    """Classify what the brace after the parameter list and this trailer opens: 'body', 'member' (initializer) or ''."""
    pos = 0
    while True:
        pos = skip_space(trailer, pos)
        if pos == len(trailer):
            return 'body'
        rest = trailer[pos:]
        if rest.startswith('->') or re.match(r'requires\b', rest):
            # A return type or constraint extends to the brace, so the brace cannot be inside it
            tail = rest[2:] if rest.startswith('->') else rest
            balanced = tail.count('(') == tail.count(')') and tail.count('<') == tail.count('>')
            return 'body' if balanced and not any(ch in tail for ch in ';{}') else ''
        if rest.startswith('&&'):
            pos += 2
            continue
        if rest.startswith('&'):
            pos += 1
            continue
        if rest.startswith(':') and not rest.startswith('::'):
            return classify_init_list(trailer, pos + 1)
        word = TRAILER_WORD_RE.match(trailer, pos) or TRAILER_MACRO_RE.match(trailer, pos)
        if not word:
            return ''
        pos = word.end()
        probe = skip_space(trailer, pos)
        takes_args = word.group(0) in ('noexcept', 'throw') or word.group(0).isupper()
        if takes_args and probe < len(trailer) and trailer[probe] == '(':
            close_pos = match_close(trailer, probe)
            if close_pos == -1:
                return ''
            pos = close_pos + 1


def classify_init_list(trailer: str, pos: int) -> str:
    while True:
        pos = skip_space(trailer, pos)
        member = INIT_MEMBER_RE.match(trailer, pos)
        if not member:
            return ''
        pos = skip_space(trailer, member.end())
        if pos < len(trailer) and trailer[pos] == '<':
            close_pos = match_close(trailer, pos)
            if close_pos == -1:
                return ''
            pos = skip_space(trailer, close_pos + 1)
        if pos == len(trailer):
            return 'member'
        if trailer[pos] not in '({':
            return ''
        close_pos = match_close(trailer, pos)
        if close_pos == -1:
            return ''
        pos = skip_space(trailer, close_pos + 1)
        if trailer.startswith('...', pos):
            pos = skip_space(trailer, pos + 3)
        if pos == len(trailer):
            return 'body'
        if trailer[pos] != ',':
            return ''
        pos += 1


def locate_markerless(parsed: ParsedFile, entry: FuncEntry) -> FuncEntry | None:
    """Locate a baseline markerless function by its normalized signature."""
    collapsed_parts: list[str] = []
    offset_map: list[int] = []
    prev_space = True
    for i, ch in enumerate(parsed.hash_mask):
        if ch.isspace():
            if not prev_space:
                collapsed_parts.append(' ')
                offset_map.append(i)
            prev_space = True
        else:
            collapsed_parts.append(ch)
            offset_map.append(i)
            prev_space = False
    collapsed = ''.join(collapsed_parts)
    pos = collapsed.find(entry.signature)
    while pos != -1:
        end_off = offset_map[min(pos + len(entry.signature) - 1, len(offset_map) - 1)]
        if entry.signature.rstrip().endswith(';'):
            # bodyless macro-invocation statement: presence is the contract
            digest = hashlib.sha1(entry.signature.encode('utf-8')).hexdigest()[:16]
            found = dataclasses.replace(entry, body_hash=digest)
            found.line = offset_to_line(parsed.offsets, end_off) + 1
            return found
        # skip forward declarations that share the definition's signature text
        brace_off = find_body_brace(parsed.code_mask, end_off + 1)
        if brace_off != -1:
            close_off = match_brace(parsed.code_mask, brace_off)
            if close_off != -1:
                found = dataclasses.replace(entry, body_hash=hash_body(parsed.hash_mask, brace_off, close_off))
                found.line = offset_to_line(parsed.offsets, end_off) + 1
                return found
        pos = collapsed.find(entry.signature, pos + 1)
    return None


def locate_macro_family(parsed: ParsedFile, entry: FuncEntry) -> FuncEntry | None:
    """Locate a baseline macro-family entry (#define NAME ...) and hash its block."""
    macro_name = entry.function
    for idx, line in enumerate(parsed.lines):
        m = DEFINE_RE.match(line)
        if m and m.group(1) == macro_name:
            block = [line]
            j = idx
            while parsed.lines[j].rstrip().endswith('\\') and j + 1 < len(parsed.lines):
                j += 1
                block.append(parsed.lines[j])
            digest = hashlib.sha1(normalize_ws(' '.join(block)).encode('utf-8')).hexdigest()[:16]
            found = dataclasses.replace(entry, body_hash=digest)
            found.line = idx + 1
            return found
    return None
