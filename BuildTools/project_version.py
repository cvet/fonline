"""Generate or check a machine-written, monotonic project VERSION file.

Generation leaves a VERSION file that already differs from HEAD (staged,
unstaged or untracked) untouched after validating it. Otherwise it reads the
file at the base ref, takes the highest value reachable from HEAD and the base
ref, and writes the scheme's next value after it. ``--check`` validates the
working-tree file and rejects a value below the reachable history.

The encoding is project policy and is selected with ``--scheme``: a built-in
scheme name, or ``<file.py or module>:<factory>`` for a project-owned scheme.
Both modes read Git only and need the complete local history.
"""

from __future__ import annotations

import argparse
import importlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
from typing import Any, Callable, Protocol


DEFAULT_FILE = 'VERSION'
DEFAULT_SCHEME = 'patch-per-commit'
CONFIG_KEYS = frozenset({'file', 'base_ref', 'scheme', 'limits', 'since_commit'})
_NUMBER = r'(?:0|[1-9][0-9]*)'


class VersionScheme(Protocol):
    def parse(self, text: str) -> tuple[int, ...]: ...

    def bump(self, value: tuple[int, ...]) -> tuple[int, ...]: ...

    def format(self, value: tuple[int, ...]) -> str: ...


class LimitedComponentsScheme:
    """Dotted non-negative integers; the last component advances and carries at its exclusive limit."""

    def __init__(self, components: tuple[str, ...], limits: dict[str, int]) -> None:
        unknown = sorted(set(limits) - set(components))
        if unknown:
            raise ValueError(f'Unknown limit component(s) {", ".join(unknown)}; this scheme has {", ".join(components)}')
        for name, limit in limits.items():
            if isinstance(limit, bool) or not isinstance(limit, int) or limit < 1:
                raise ValueError(f'Limit {name} must be a positive integer: {limit!r}')
        self._components = components
        self._limits = tuple(limits.get(name) for name in components)
        self._pattern = re.compile(r'\.'.join([_NUMBER] * len(components)))

    def parse(self, text: str) -> tuple[int, ...]:
        if self._pattern.fullmatch(text) is None:
            raise ValueError(f'expected {".".join(self._components).upper()} as non-negative integers without leading zeroes')
        value = tuple(int(part) for part in text.split('.'))
        for name, number, limit in zip(self._components, value, self._limits, strict=True):
            if limit is not None and number >= limit:
                raise ValueError(f'{name} {number} is outside the limit {limit}')
        return value

    def bump(self, value: tuple[int, ...]) -> tuple[int, ...]:
        numbers = list(value)
        for index in reversed(range(len(numbers))):
            numbers[index] += 1
            limit = self._limits[index]
            if limit is None or numbers[index] < limit:
                return tuple(numbers)
            if index == 0:
                raise ValueError(f'{self._components[0]} limit {limit} is exhausted after {self.format(value)}')
            numbers[index] = 0
        raise AssertionError('unreachable')

    def format(self, value: tuple[int, ...]) -> str:
        return '.'.join(str(number) for number in value)


def patch_per_commit_scheme(limits: dict[str, int]) -> VersionScheme:
    return LimitedComponentsScheme(('major', 'minor', 'patch'), limits)


def counter_scheme(limits: dict[str, int]) -> VersionScheme:
    return LimitedComponentsScheme(('value',), limits)


BUILTIN_SCHEMES: dict[str, Callable[[dict[str, int]], VersionScheme]] = {
    'patch-per-commit': patch_per_commit_scheme,
    'counter': counter_scheme,
}


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog='project_version.py',
        description='Generate or check a machine-written monotonic project VERSION file against Git history.')
    parser.add_argument('--root', type=Path, default='.',
                        help='directory inside the project work tree (default: current directory)')
    parser.add_argument('--config', type=Path,
                        help='JSON file with any of file, base_ref, scheme, limits, since_commit; options given on the command line win')
    parser.add_argument('--file', help='version file relative to --root (default: VERSION)')
    parser.add_argument('--base-ref', help='Git ref whose version the next value must exceed, for example origin/main; required to generate')
    parser.add_argument('--scheme', help='built-in patch-per-commit (default) or counter, or <file.py or module>:<factory> of a project scheme')
    parser.add_argument('--limit', action='append', default=[], metavar='COMPONENT=N',
                        help='exclusive upper bound of one scheme component, for example patch=10000; a full component carries into the previous one; repeatable')
    parser.add_argument('--since-commit',
                        help='ignore values committed before this commit, for example where the encoding changed; it must be an ancestor of every compared ref')
    parser.add_argument('--check', action='store_true', help='validate the working-tree file without writing it')
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = create_parser()
    args = parser.parse_args(argv)
    try:
        settings = resolve_settings(args)
        scheme = load_scheme(settings['scheme'], settings['limits'], args.root)
        project = ProjectVersion(args.root, settings['file'], scheme, settings['since_commit'])
        if args.check:
            message = project.check(settings['base_ref'])
        else:
            message = project.generate(settings['base_ref'])
    except (OSError, ValueError) as error:
        print(f'project_version: {error}', file=sys.stderr)
        return 1
    print(f'project_version: {message}')
    return 0


def resolve_settings(args: argparse.Namespace) -> dict[str, Any]:
    settings: dict[str, Any] = {'file': DEFAULT_FILE, 'base_ref': None, 'scheme': DEFAULT_SCHEME, 'limits': {}, 'since_commit': None}
    if args.config is not None:
        settings.update(read_config(args.config))
    for key in ('file', 'base_ref', 'scheme', 'since_commit'):
        if getattr(args, key) is not None:
            settings[key] = getattr(args, key)
    if args.limit:
        settings['limits'] = dict(settings['limits'])
        settings['limits'].update(parse_limit(value) for value in args.limit)
    return settings


def read_config(path: Path) -> dict[str, Any]:
    try:
        config = json.loads(path.read_text(encoding='utf-8'))
    except json.JSONDecodeError as error:
        raise ValueError(f'Invalid JSON in {path}: {error}') from error
    if not isinstance(config, dict):
        raise ValueError(f'{path} must contain a JSON object')
    unknown = sorted(set(config) - CONFIG_KEYS)
    if unknown:
        raise ValueError(f'Unknown key(s) in {path}: {", ".join(unknown)}')
    for key in ('file', 'base_ref', 'scheme', 'since_commit'):
        if key in config and not isinstance(config[key], str):
            raise ValueError(f'{path}: {key} must be a string')
    if 'limits' in config and not isinstance(config['limits'], dict):
        raise ValueError(f'{path}: limits must be an object of component names to integers')
    return config


def parse_limit(text: str) -> tuple[str, int]:
    name, separator, value = text.partition('=')
    if not separator or not name or re.fullmatch(r'[1-9][0-9]*', value) is None:
        raise ValueError(f'--limit must be COMPONENT=N with a positive integer N: {text!r}')
    return name, int(value)


def load_scheme(spec: str, limits: dict[str, int], root: Path) -> VersionScheme:
    if spec in BUILTIN_SCHEMES:
        return BUILTIN_SCHEMES[spec](limits)

    source, separator, attribute = spec.rpartition(':')
    if not separator or not source or not attribute.isidentifier():
        raise ValueError(f'Unknown scheme {spec!r}; use {", ".join(sorted(BUILTIN_SCHEMES))} or <file.py or module>:<factory>')
    if source.endswith('.py'):
        path = Path(source)
        path = path if path.is_absolute() else root / path
        module_spec = importlib.util.spec_from_file_location(f'_project_version_scheme_{path.stem}', path)
        if module_spec is None or module_spec.loader is None or not path.is_file():
            raise ValueError(f'Scheme file not found: {path}')
        module = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(module)
    else:
        sys.path.insert(0, str(root.resolve()))
        try:
            module = importlib.import_module(source)
        except ImportError as error:
            raise ValueError(f'Cannot import scheme module {source!r}: {error}') from error
        finally:
            sys.path.pop(0)
    factory = getattr(module, attribute, None)
    if not callable(factory):
        raise ValueError(f'Scheme factory {attribute!r} is not a callable in {source}')
    scheme = factory(dict(limits))
    for method in ('parse', 'bump', 'format'):
        if not callable(getattr(scheme, method, None)):
            raise ValueError(f'Scheme from {spec} has no {method}() method')
    return scheme


class ProjectVersion:
    def __init__(self, root: Path, file: str, scheme: VersionScheme, since_commit: str | None) -> None:
        self._top = Path(self._git(root, 'rev-parse', '--show-toplevel')).resolve()
        self._path = (root / file).resolve()
        try:
            self._relative = self._path.relative_to(self._top).as_posix()
        except ValueError as error:
            raise ValueError(f'Version file {self._path} is outside the work tree {self._top}') from error
        self._scheme = scheme
        self._since_commit = since_commit

    def check(self, base_ref: str | None) -> str:
        value = self._read_working_value()
        floor = self._history_floor(['HEAD', *([base_ref] if base_ref else [])])
        self._require_not_below(value, floor)
        return f'{self._relative} {self._scheme.format(value)} is valid; reachable floor {self._describe(floor)}'

    def generate(self, base_ref: str | None) -> str:
        if self._is_modified():
            value = self._read_working_value()
            self._require_not_below(value, self._history_floor(['HEAD']))
            return f'{self._relative} already modified: {self._scheme.format(value)}'

        if not base_ref:
            raise ValueError('Generation requires --base-ref (or base_ref in the config file)')
        base_text = self.git('show', f'{base_ref}:{self._relative}', strip=False)
        base_source = f'{base_ref}:{self._relative}'
        base_value = self._parse(normalize_text(base_text, base_source), base_source)
        floor = self._history_floor(['HEAD', base_ref])
        new_value = self._scheme.bump(max(floor, base_value) if floor is not None else base_value)
        new_text = self._scheme.format(new_value)
        self._parse(new_text, 'the scheme output')
        self._path.write_text(new_text + '\n', encoding='utf-8', newline='\n')
        return f'{self._relative}: {self._describe(floor)} -> {new_text} (base {base_ref}: {self._scheme.format(base_value)})'

    def git(self, *args: str, strip: bool = True) -> str:
        return self._git(self._top, *args, strip=strip)

    @staticmethod
    def _git(root: Path, *args: str, strip: bool = True) -> str:
        result = subprocess.run(['git', '--no-optional-locks', '-C', str(root), *args],
                                capture_output=True, text=True, encoding='utf-8')
        if result.returncode:
            raise ValueError(f'git {" ".join(args)} failed: {result.stderr.strip()}')
        return result.stdout.strip() if strip else result.stdout

    def _is_modified(self) -> bool:
        return bool(self.git('status', '--porcelain', '--untracked-files=all', '--', self._relative))

    def _read_working_value(self) -> tuple[int, ...]:
        try:
            text = self._path.read_bytes().decode('utf-8')
        except (OSError, UnicodeError) as error:
            raise ValueError(f'Cannot read {self._path}: {error}') from error
        return self._parse(normalize_text(text, self._relative), self._relative)

    def _parse(self, text: str, source: str) -> tuple[int, ...]:
        try:
            return tuple(self._scheme.parse(text))
        except ValueError as error:
            raise ValueError(f'{source} holds {text!r}, which is invalid: {error}') from error

    def _history_floor(self, refs: list[str]) -> tuple[int, ...] | None:
        if self.git('rev-parse', '--is-shallow-repository') != 'false':
            raise ValueError('The repository is shallow; fetch the complete history before comparing versions')
        for ref in refs:
            self.git('rev-parse', '--verify', f'{ref}^{{commit}}')

        values: list[tuple[int, ...]] = []
        log = ['log', '--full-history', '-m', '--format=', '--no-ext-diff', '--no-color', '--no-renames', '-p']
        if self._since_commit:
            since = self.git('rev-parse', '--verify', f'{self._since_commit}^{{commit}}')
            for ref in refs:
                self.git('merge-base', '--is-ancestor', since, ref)
            since_source = f'{since}:{self._relative}'
            values.append(self._parse(normalize_text(self.git('show', since_source, strip=False), since_source), since_source))
            # With a single bottom commit the plain option selects what --ancestry-path=<since> selects on newer Git
            log += ['--ancestry-path', *refs, f'^{since}']
        else:
            log += refs
        for line in self.git(*log, '--', self._relative, strip=False).splitlines():
            if line.startswith('+') and not line.startswith('+++ '):
                values.append(self._parse(line[1:], f'a committed {self._relative}'))
        return max(values) if values else None

    def _require_not_below(self, value: tuple[int, ...], floor: tuple[int, ...] | None) -> None:
        if floor is not None and value < floor:
            raise ValueError(f'{self._relative} {self._scheme.format(value)} is below the reachable version {self._scheme.format(floor)}')

    def _describe(self, floor: tuple[int, ...] | None) -> str:
        return self._scheme.format(floor) if floor is not None else 'none'


def normalize_text(text: str, source: str) -> str:
    # One value line; a single LF or CRLF line ending is accepted, nothing else around the value
    value = text.removesuffix('\n').removesuffix('\r') if text.endswith('\n') else text
    if not value or value != value.strip() or '\n' in value or '\r' in value or value.startswith(chr(0xFEFF)):
        raise ValueError(f'{source} must hold exactly one line with the version, got {text!r}')
    return value


if __name__ == '__main__':
    raise SystemExit(main())
