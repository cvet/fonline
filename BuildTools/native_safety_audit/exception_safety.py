"""Per-function exception-safety classification audit.

A committed baseline records the exception-safety level every native function
currently guarantees, with the reason that justifies it:

    NoThrow  - no exception propagates to the caller
    Strong   - may throw, but observable state is unchanged on throw
    Basic    - may throw after a mutation, but every invariant holds
    None     - a throw can escape leaving a broken invariant (defect candidate)

The level taxonomy and derivation rules are owned by
``Docs/en/contributing/coding-contracts/exception-safety.md`` (section 8).
This module owns the process: it enumerates function definitions in the Engine
``Source/**/*.cpp`` (and in the project scopes), hashes each body with comments
and zones stripped, and compares the inventory with the baselines so that a new
function without a classification, a changed body, a vanished function, a row
without a reason and an unclassified row all fail the audit. Engine rows live in
the Engine baseline; project rows live in the project baseline passed with
``--baseline``.
"""

from __future__ import annotations

import dataclasses
from pathlib import Path

from . import cpp_parser as parser
from .cpp_parser import FuncEntry
from .scope import ENGINE_ORIGIN, PROJECT_ORIGIN, AuditContext, ConfigError, SourceFile, engine_files, project_files, resolve_input


LEVELS = ('NoThrow', 'Strong', 'Basic', 'None', 'Unclassified')
STATUSES = ('annotated', 'verified', 'adjudicated', 'unclassified')
KINDS = ('marker', 'export', 'markerless', 'macro_family')
# The required reason is the last column, so a row never ends with an empty field
TSV_COLUMNS = ('file', 'kind', 'function', 'signature', 'body_hash', 'level', 'status', 'review_note', 'reason')

ENGINE_BASELINE = Path('BuildTools') / 'audit_baselines' / 'exception-safety.tsv'
CONTRACT = 'Docs/en/contributing/coding-contracts/exception-safety.md section 8'


@dataclasses.dataclass
class Unit:
    """One source population with its own baseline: the Engine or the project scopes."""

    origin: str
    files: list[SourceFile]
    baseline_path: Path
    display_prefix: str

    def display(self, file: str) -> str:
        return self.display_prefix + file


def is_skipped_source(unit_path: str) -> bool:
    """Tests and codegen template inputs are out of scope in every population."""

    return '/Tests/' in '/' + unit_path or unit_path.endswith('.template.cpp')


def create_units(context: AuditContext, project_baseline: Path | None) -> tuple[list[Unit], list[str]]:
    """Return the Engine unit, the optional project unit and stale-exclusion errors."""

    if context.scopes and project_baseline is None:
        raise ConfigError('exception-safety: a project --scope requires a project --baseline')

    if project_baseline is not None and not context.scopes:
        raise ConfigError('exception-safety: --baseline names a project baseline and requires at least one --scope')

    engine = Unit(ENGINE_ORIGIN, engine_files(context, (('Source', ('.cpp',)),), is_skipped_source),
                  context.engine_root / ENGINE_BASELINE, context.engine_display_prefix)
    units = [engine]
    errors: list[str] = []

    if context.scopes:
        files, errors = project_files(context, ('.cpp',), is_skipped_source)
        units.append(Unit(PROJECT_ORIGIN, files, resolve_input(context, project_baseline), ''))

    return units, errors


# ---------------------------------------------------------------------------
# Baseline I/O
# ---------------------------------------------------------------------------

def sanitize_field(value: str) -> str:
    return value.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ').strip()


def load_baseline(path: Path) -> tuple[list[FuncEntry], list[str]]:
    """Read the baseline. Returns (entries, problems); a malformed row is a problem,
    never a silently accepted value - an unrecognized level/status/kind would otherwise
    pass the gate while making the row invisible to `report`."""
    entries: list[FuncEntry] = []
    problems: list[str] = []
    with path.open('r', encoding='utf-8', newline='') as handle:
        header = handle.readline().rstrip('\r\n').split('\t')
        if header != list(TSV_COLUMNS):
            problems.append(f'{path.name}:1: unexpected baseline header: {header} '
                            f'(expected {list(TSV_COLUMNS)})')
            return entries, problems
        for line_number, line in enumerate(handle, start=2):
            line = line.rstrip('\r\n')
            if not line:
                continue
            raw_parts = line.split('\t')
            if len(raw_parts) != len(TSV_COLUMNS):
                problems.append(f'{path.name}:{line_number}: expected {len(TSV_COLUMNS)} columns, got {len(raw_parts)}')
            parts = [sanitize_field(part) for part in raw_parts[:len(TSV_COLUMNS)]]
            parts += [''] * (len(TSV_COLUMNS) - len(parts))
            for column, value, allowed in (('kind', parts[1], KINDS),
                                           ('level', parts[5], LEVELS),
                                           ('status', parts[6], STATUSES)):
                if value not in allowed:
                    problems.append(f'{path.name}:{line_number}: invalid {column} {value!r} for '
                                    f'{parts[2] or "<unnamed row>"} (expected one of {", ".join(allowed)})')
            entries.append(FuncEntry(
                file=parts[0], kind=parts[1], function=parts[2], signature=parser.normalize_ws(parts[3]),
                body_hash=parts[4], level=parts[5], status=parts[6], review_note=parts[7],
                reason=parts[8]))
    return entries, problems


def save_baseline(path: Path, entries: list[FuncEntry]) -> None:
    ordered = sorted(entries, key=lambda e: (e.file, e.function, e.signature, e.body_hash))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', encoding='utf-8', newline='\n') as handle:
        handle.write('\t'.join(TSV_COLUMNS) + '\n')
        for e in ordered:
            row = '\t'.join(sanitize_field(v) for v in (
                e.file, e.kind, e.function, e.signature, e.body_hash,
                e.level, e.status, e.review_note, e.reason))
            handle.write(row + '\n')


def read_unit_baseline(unit: Unit, allow_missing: bool) -> tuple[list[FuncEntry], list[str]]:
    if not unit.baseline_path.is_file():
        if allow_missing:
            return [], []
        return [], [f'baseline not found: {unit.baseline_path}']
    return load_baseline(unit.baseline_path)


# ---------------------------------------------------------------------------
# Inventory
# ---------------------------------------------------------------------------

def build_inventory(unit: Unit, baseline: list[FuncEntry]) -> tuple[list[FuncEntry], list[str], set[tuple[str, str, str]]]:
    """Enumerate current functions: every zone-marked or FO_SCRIPT_API function, plus every other definition the
    baseline tracks, located by signature whether or not it carries a zone (a zone is instrumentation, so adding or
    dropping one never moves a function in or out of the inventory), and macro families by their
    #define name; returns (entries, location_errors, failed_keys)."""
    inventory: list[FuncEntry] = []
    errors: list[str] = []
    failed_keys: set[tuple[str, str, str]] = set()
    tracked_by_file: dict[str, dict[tuple[str, str, str], list[FuncEntry]]] = {}
    for entry in baseline:
        if entry.kind != 'export':
            tracked_by_file.setdefault(entry.file, {}).setdefault(entry.key(), []).append(entry)

    for source in unit.files:
        rel = source.unit_path
        text = source.path.read_text(encoding='utf-8', errors='replace')
        parsed = parser.parse_file(rel, text)
        inventory.extend(parsed.functions)
        discovered_keys = {f.key() for f in parsed.functions}
        claimed = {f.body_off for f in parsed.functions}

        for key, rows in tracked_by_file.get(rel, {}).items():
            kind = rows[0].kind
            if kind == 'macro_family' or key[2].rstrip().endswith(';'):
                locator = parser.locate_macro_family if kind == 'macro_family' else parser.locate_markerless
                for row in rows:
                    found = locator(parsed, row)
                    if found is None:
                        errors.append(f'{unit.display(rel)}: {kind} entry not found in source: {row.function} '
                                      f'(its signature changed or it was removed - re-derive and fix the baseline row)')
                        failed_keys.add(key)
                    else:
                        inventory.append(found)
                continue

            located = [dataclasses.replace(d, function=key[1]) for d in parsed.definitions
                       if d.signature == key[2] and d.body_off not in claimed]
            needed = len(rows) - sum(1 for f in parsed.functions if f.key() == key)
            if len(located) > needed:
                # An untracked twin may share the signature text (one-line class methods); the body the baseline
                # knows is taken first
                row_hashes = {row.body_hash for row in rows}
                located = sorted(located, key=lambda d: d.body_hash not in row_hashes)[:max(needed, 0)]
            if not located and key not in discovered_keys and kind == 'markerless':
                # A signature recorded across several lines may be collected differently by the definition scan
                found = parser.locate_markerless(parsed, rows[0])
                if found is not None:
                    located = [found]
            if not located and key not in discovered_keys and kind == 'markerless':
                errors.append(f'{unit.display(rel)}: markerless entry not found in source: {rows[0].function} '
                              f'(its signature changed or it was removed - re-derive and fix the baseline row)')
                failed_keys.add(key)
            inventory.extend(located)
    return inventory, errors, failed_keys


# ---------------------------------------------------------------------------
# Audit
# ---------------------------------------------------------------------------

def group_by_key(entries: list[FuncEntry]) -> dict[tuple[str, str, str], list[FuncEntry]]:
    grouped: dict[tuple[str, str, str], list[FuncEntry]] = {}
    for entry in entries:
        grouped.setdefault(entry.key(), []).append(entry)
    return grouped


def refresh_group_hashes(baseline_entries: list[FuncEntry], current_entries: list[FuncEntry]) -> tuple[int, str]:
    """Refresh changed hashes while preserving duplicate-definition classifications."""
    # Checked before the pairing walk: a count increase leaves stale_baseline empty and
    # would otherwise return a silent 'nothing to do' that no command can repair
    if len(current_entries) != len(baseline_entries):
        return 0, 'definition count changed; add or prune the affected baseline rows first'

    unmatched_current = sorted(entry.body_hash for entry in current_entries)
    stale_baseline: list[FuncEntry] = []

    for entry in baseline_entries:
        if entry.body_hash in unmatched_current:
            unmatched_current.remove(entry.body_hash)
        else:
            stale_baseline.append(entry)

    if not stale_baseline:
        return 0, ''

    classifications = {
        (entry.kind, entry.level, entry.status, entry.reason, entry.review_note)
        for entry in stale_baseline
    }
    if len(stale_baseline) > 1 and len(classifications) > 1:
        return 0, 'multiple changed definitions have different classifications; update their rows explicitly'

    for entry, body_hash in zip(stale_baseline, unmatched_current):
        entry.body_hash = body_hash
    return len(stale_baseline), ''


def migrate_signature_only_entries(baseline: list[FuncEntry], current: list[FuncEntry]) -> tuple[int, list[str]]:
    """Move classifications to changed signatures only when the function body is byte-for-byte equivalent.

    The file/function identity and body hash must match, and duplicate equal-body rows must have one identical
    classification. Anything ambiguous is left for explicit review.
    """
    baseline_keys = {entry.key() for entry in baseline}
    current_keys = {entry.key() for entry in current}
    stale_by_identity: dict[tuple[str, str], list[FuncEntry]] = {}
    new_by_identity: dict[tuple[str, str], list[FuncEntry]] = {}

    for entry in baseline:
        if entry.key() not in current_keys:
            stale_by_identity.setdefault((entry.file, entry.function), []).append(entry)
    for entry in current:
        if entry.key() not in baseline_keys:
            new_by_identity.setdefault((entry.file, entry.function), []).append(entry)

    migrated_old_ids: set[int] = set()
    replacements: list[FuncEntry] = []
    notes: list[str] = []

    for identity in sorted(stale_by_identity.keys() & new_by_identity.keys()):
        stale_entries = stale_by_identity[identity]
        new_entries = new_by_identity[identity]
        hashes = sorted({entry.body_hash for entry in stale_entries} & {entry.body_hash for entry in new_entries})

        for body_hash in hashes:
            stale_matches = [entry for entry in stale_entries if entry.body_hash == body_hash]
            new_matches = [entry for entry in new_entries if entry.body_hash == body_hash]

            if len(stale_matches) != len(new_matches):
                notes.append(f'{identity[0]}::{identity[1]}: equal-body signature row count differs '
                             f'({len(stale_matches)} baseline, {len(new_matches)} source)')
                continue

            classifications = {
                (entry.level, entry.status, entry.reason, entry.review_note)
                for entry in stale_matches
            }
            if len(classifications) != 1:
                notes.append(f'{identity[0]}::{identity[1]}: equal-body signature rows have different classifications')
                continue

            source = stale_matches[0]
            for entry in stale_matches:
                migrated_old_ids.add(id(entry))
            for entry in new_matches:
                replacements.append(dataclasses.replace(
                    entry,
                    level=source.level,
                    status=source.status,
                    reason=source.reason,
                    review_note=source.review_note,
                ))

    if migrated_old_ids:
        baseline[:] = [entry for entry in baseline if id(entry) not in migrated_old_ids]
        baseline.extend(replacements)

    return len(replacements), notes


def audit_unit(unit: Unit) -> tuple[int, list[str], list[str]]:
    """Return (checked function count, errors, warnings) for one unit."""
    baseline, baseline_problems = read_unit_baseline(unit, allow_missing=False)
    inventory, location_errors, failed_keys = build_inventory(unit, baseline)

    errors: list[str] = list(baseline_problems) + list(location_errors)
    warnings: list[str] = []

    current = group_by_key(inventory)
    base = group_by_key(baseline)

    for key, cur_list in sorted(current.items()):
        base_list = base.get(key, [])
        cur_hashes = sorted(e.body_hash for e in cur_list)
        base_hashes = sorted(e.body_hash for e in base_list)
        e = cur_list[0]
        location = f'{unit.display(e.file)}:{e.line}'
        if not base_list:
            errors.append(f'{location}: new function is not classified: {e.function} '
                          f"(run 'exception-safety update --add-new', derive its level per {CONTRACT}, "
                          f'and set its level and reason in the baseline)')
        elif len(cur_list) != len(base_list):
            # Must precede the hash check: differing counts always make the sorted hash
            # lists differ, so a count change would otherwise be misreported as a body
            # change, whose 'update --accept' remedy refresh_group_hashes rejects
            errors.append(f'{location}: definition count mismatch for {e.function} '
                          f'({len(cur_list)} in source, {len(base_list)} in baseline) '
                          f'(add or prune the affected baseline rows, then re-derive their level)')
        elif cur_hashes != base_hashes:
            levels = ','.join(sorted({b.level for b in base_list}))
            errors.append(f'{location}: body changed since classification (level {levels}): {e.function} '
                          f"(re-derive the level and reason, then 'exception-safety update --accept "
                          f'"{unit.display(e.file)}::{e.function}"\')')

    for key, base_list in sorted(base.items()):
        if key not in current and key not in failed_keys:
            b = base_list[0]
            errors.append(f"{unit.display(b.file)}: baseline entry vanished from source: {b.function} "
                          f"(run 'exception-safety update --prune-removed' if the removal is intentional)")

    for entry in sorted(baseline, key=lambda e: (e.file, e.function)):
        if entry.level == 'Unclassified':
            errors.append(f'{unit.display(entry.file)}: unclassified entry: {entry.function} (set its level in the baseline)')
        if not entry.reason:
            errors.append(f'{unit.display(entry.file)}: baseline row has no reason: {entry.function} '
                          f'(write why the body guarantees its level)')
        if entry.level == 'None' and entry.status != 'adjudicated':
            warnings.append(f'{unit.display(entry.file)}: ES: None pending skeptic adjudication: {entry.function} ({entry.reason})')
        if entry.review_note:
            warnings.append(f'{unit.display(entry.file)}: manual review advised: {entry.function} - {entry.review_note}')

    return len(inventory), errors, warnings


def run_audit(units: list[Unit], scope_errors: list[str], fail_on: str) -> int:
    checked = 0
    errors: list[str] = list(scope_errors)
    warnings: list[str] = []

    for unit in units:
        unit_checked, unit_errors, unit_warnings = audit_unit(unit)
        checked += unit_checked
        errors += unit_errors
        warnings += unit_warnings

    for message in errors:
        print(f'error: {message}')
    for message in warnings:
        print(f'warning: {message}')
    print(f'exception-safety audit: {checked} functions checked, '
          f'{len(errors)} error(s), {len(warnings)} warning(s)')
    if errors:
        return 1
    if warnings and fail_on == 'warning':
        return 1
    return 0


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------

def area_of(rel_file: str) -> str:
    parts = rel_file.split('/')
    return parts[1] if len(parts) > 1 else parts[0]


def run_report(units: list[Unit], level: str | None, area: str | None,
               file_filter: str | None, function_filter: str | None) -> int:
    baseline: list[FuncEntry] = []
    areas_by_row: dict[int, str] = {}
    problems: list[str] = []
    for unit in units:
        rows, unit_problems = read_unit_baseline(unit, allow_missing=False)
        for row in rows:
            shown = dataclasses.replace(row, file=unit.display(row.file))
            areas_by_row[id(shown)] = area_of(row.file)
            baseline.append(shown)
        problems += unit_problems
    if problems:
        for message in problems:
            print(f'error: {message}')
        return 1
    if not baseline:
        print('error: the baselines contain no rows')
        return 1

    selected = [e for e in baseline
                if (not level or e.level == level)
                and (not area or areas_by_row[id(e)] == area)
                and (not file_filter or file_filter in e.file)
                and (not function_filter or function_filter in e.function)]

    filtered = bool(level or area or file_filter or function_filter)
    if filtered:
        print(f'{len(selected)} matching function(s):')
        for e in sorted(selected, key=lambda x: (x.file, x.function)):
            suffix = f' ({e.reason})' if e.reason else ''
            note = f' [review: {e.review_note}]' if e.review_note else ''
            print(f'  {e.level:<12} {e.status:<12} {e.file} :: {e.function}{suffix}{note}')
        return 0

    total = len(baseline)
    print(f'=== Exception-safety classification: {total} functions ===')
    by_level: dict[str, int] = {}
    for e in baseline:
        by_level[e.level] = by_level.get(e.level, 0) + 1
    for lvl in LEVELS:
        if by_level.get(lvl):
            print(f'  {lvl:<12} {by_level[lvl]:>5}  {by_level[lvl] / total:6.1%}')

    print('\n=== Per area ===')
    areas: dict[str, dict[str, int]] = {}
    for e in baseline:
        name = areas_by_row[id(e)]
        areas.setdefault(name, {}).setdefault(e.level, 0)
        areas[name][e.level] += 1
    shown_levels = [lvl for lvl in LEVELS if any(a.get(lvl) for a in areas.values())]
    print(f'{"AREA":<14}' + ''.join(f'{lvl:>13}' for lvl in shown_levels))
    for name in sorted(areas, key=lambda a: (-sum(areas[a].values()), a)):
        print(f'{name:<14}' + ''.join(f'{areas[name].get(lvl, 0):>13}' for lvl in shown_levels))

    by_status: dict[str, int] = {}
    for e in baseline:
        by_status[e.status] = by_status.get(e.status, 0) + 1
    print('\n=== Verification status ===')
    for status in STATUSES:
        if by_status.get(status):
            print(f'  {status:<14} {by_status[status]:>5}')

    nones = [e for e in baseline if e.level == 'None']
    print(f'\n=== ES: None ({len(nones)}) ===')
    for e in sorted(nones, key=lambda x: (x.file, x.function)):
        adjudicated = '' if e.status == 'adjudicated' else '  [pending adjudication]'
        print(f'  {e.file} :: {e.function}{adjudicated}')
        if e.reason:
            print(f'      {e.reason}')

    reviews = [e for e in baseline if e.review_note]
    if reviews:
        print(f'\n=== Manual-review queue ({len(reviews)}) ===')
        for e in sorted(reviews, key=lambda x: (x.file, x.function)):
            print(f'  {e.file} :: {e.function} ({e.level})')
            print(f'      {e.review_note}')
    return 0


# ---------------------------------------------------------------------------
# Update
# ---------------------------------------------------------------------------

def update_unit(unit: Unit, add_new: bool, prune_removed: bool, migrate_signatures: bool,
                accept: set[str], accept_all_changed: bool) -> int:
    baseline, baseline_problems = read_unit_baseline(unit, allow_missing=unit.origin == PROJECT_ORIGIN)
    if baseline_problems:
        # Never rewrite a baseline that could not be parsed cleanly - saving would
        # silently normalize the malformed rows away
        for message in baseline_problems:
            print(f'error: {message}')
        return 1
    inventory, location_errors, _failed = build_inventory(unit, baseline)
    for message in location_errors:
        print(f'note: {message}')

    current = group_by_key(inventory)
    base = group_by_key(baseline)
    changed_files = not unit.baseline_path.is_file()

    if migrate_signatures:
        migrated, migration_notes = migrate_signature_only_entries(baseline, inventory)
        for message in migration_notes:
            print(f'note: cannot migrate signature for {message}')
        print(f'migrated {migrated} signature-only entr(ies)')
        changed_files = changed_files or migrated > 0
        base = group_by_key(baseline)

    if add_new:
        added = 0
        for key, cur_list in sorted(current.items()):
            if key not in base:
                for cur in cur_list:
                    baseline.append(dataclasses.replace(cur, level='Unclassified', status='unclassified', reason=''))
                    added += 1
        print(f'added {added} unclassified entr(ies)')
        changed_files = changed_files or added > 0

    if prune_removed:
        before = len(baseline)
        baseline = [e for e in baseline if e.key() in current]
        pruned = before - len(baseline)
        print(f'pruned {pruned} vanished entr(ies)')
        changed_files = changed_files or pruned > 0

    accepted = 0
    for key, base_list in base.items():
        cur_list = current.get(key, [])
        if not cur_list:
            continue
        if sorted(e.body_hash for e in base_list) == sorted(e.body_hash for e in cur_list):
            continue
        names = {f'{base_list[0].file}::{base_list[0].function}', f'{unit.display(base_list[0].file)}::{base_list[0].function}'}
        if accept_all_changed or names & accept:
            refreshed, problem = refresh_group_hashes(base_list, cur_list)
            if problem:
                print(f'note: cannot accept {unit.display(base_list[0].file)}::{base_list[0].function}: {problem}')
            accepted += refreshed
    if accept_all_changed or accept:
        print(f'accepted {accepted} refreshed body hash(es)')
        changed_files = changed_files or accepted > 0

    synced = sync_kinds(baseline, inventory)
    if synced:
        print(f'synced {synced} kind(s) with whether the function carries a zone')
        changed_files = True

    if changed_files:
        save_baseline(unit.baseline_path, baseline)
        print(f'baseline written: {unit.baseline_path}')
    else:
        print(f'baseline unchanged: {unit.baseline_path}')
    return 0


def run_update(units: list[Unit], scope_errors: list[str], add_new: bool, prune_removed: bool,
               migrate_signatures: bool, accept: list[str], accept_all_changed: bool) -> int:
    for message in scope_errors:
        print(f'error: {message}')
    if scope_errors:
        return 1
    result = 0
    for unit in units:
        result = max(result, update_unit(unit, add_new, prune_removed, migrate_signatures, set(accept), accept_all_changed))
    return result


def sync_kinds(baseline: list[FuncEntry], inventory: list[FuncEntry]) -> int:
    """Keep 'marker' / 'markerless' in step with whether the function carries a zone; rows pair by body hash."""
    found_kinds: dict[tuple[tuple[str, str, str], str], set[str]] = {}
    for entry in inventory:
        if entry.kind in ('marker', 'markerless'):
            found_kinds.setdefault((entry.key(), entry.body_hash), set()).add(entry.kind)
    changed = 0
    for row in baseline:
        kinds = found_kinds.get((row.key(), row.body_hash)) if row.kind in ('marker', 'markerless') else None
        if kinds and len(kinds) == 1 and row.kind not in kinds:
            row.kind = next(iter(kinds))
            changed += 1
    return changed
