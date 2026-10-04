from __future__ import annotations

import datetime
import re
import subprocess
from pathlib import Path
from typing import NamedTuple


VERSION_RE = re.compile(
    r"(?P<year>[1-9][0-9]{3})\.(?P<major>[1-9][0-9]*)\.(?P<minor>0|[1-9][0-9]*)"
    r"(?:(?P<dev>-dev)|\.(?P<patch>0|[1-9][0-9]*)(?P<rc>-rc)?)"
)
RELEASE_RE = re.compile(r"^## ([^\s]+) - ([0-9]{4}-[0-9]{2}-[0-9]{2})$", re.MULTILINE)
CHANGELOG_PATHS = ("Docs/en/reference/changelog.md", "Docs/ru/reference/changelog.md")


class EngineVersion(NamedTuple):
    year: int
    major: int
    minor: int
    patch: int | None
    suffix: str


def parse_engine_version(version: str) -> EngineVersion:
    match = VERSION_RE.fullmatch(version)
    if not match:
        raise ValueError(
            f"Invalid Engine VERSION {version!r}; expected YEAR.MAJOR.MINOR-dev"
            " or YEAR.MAJOR.MINOR.PATCH with optional -rc (no leading zeroes)"
        )
    return EngineVersion(int(match['year']), int(match['major']), int(match['minor']),
                         int(match['patch']) if match['patch'] is not None else None,
                         '-dev' if match['dev'] else '-rc' if match['rc'] else '')


def read_engine_version(root: Path) -> str:
    path = root / "VERSION"
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exception:
        raise ValueError(f"Unable to read Engine VERSION: {path}: {exception}") from exception
    version = text.removesuffix("\n").removesuffix("\r")
    parse_engine_version(version)
    return version


def read_engine_revision(root: Path) -> str:
    # An archive nested in a game checkout must not inherit the game's Git identity
    if not (root / ".git").exists():
        return "unknown"
    try:
        revision = subprocess.check_output(
            ["git", "-C", str(root), "rev-parse", "HEAD"], text=True, stderr=subprocess.PIPE
        ).strip()
        dirty = subprocess.check_output(
            ["git", "-C", str(root), "status", "--porcelain", "--untracked-files=no"],
            text=True, stderr=subprocess.PIPE,
        ).strip()
    except (OSError, subprocess.CalledProcessError) as exception:
        raise ValueError(f"Unable to identify Engine Git revision: {root}: {exception}") from exception
    return revision + ("-dirty" if dirty else "")


def validate_changelog_texts(version: str, texts: list[str]) -> None:
    catalogs = []
    for relative_path, text, migration in zip(CHANGELOG_PATHS, texts, ('Migration', 'Миграция'), strict=True):
        if text.count("\n## Unreleased\n") != 1:
            raise ValueError(f"{relative_path} must contain exactly one Unreleased section")
        releases = []
        headings = list(RELEASE_RE.finditer(text))
        for candidate in re.findall(r'^## [0-9].*$', text, re.MULTILINE):
            if not RELEASE_RE.fullmatch(candidate):
                raise ValueError(f'Malformed dated version heading: {relative_path}: {candidate}')
        for heading in headings:
            release, date_text = heading.groups()
            parsed = parse_engine_version(release)
            date = datetime.date.fromisoformat(date_text)
            if parsed.suffix == '-dev' and parsed.year != date.year:
                raise ValueError(f"Development version year differs from its date: {release}: {date_text}")
            if release in [entry[0] for entry in releases]:
                raise ValueError(f"Duplicate changelog release: {relative_path}: {release}")
            end = re.search(r'^## ', text[heading.end():], re.MULTILINE)
            body = text[heading.end():heading.end() + end.start() if end else len(text)]
            migration_match = re.search(rf'^### {migration}\s*\n(.+?)(?=^### |\Z)', body,
                                        re.MULTILINE | re.DOTALL)
            if not migration_match or not migration_match[1].strip():
                raise ValueError(f"Missing explicit migration disposition: {relative_path}: {release}")
            releases.append((release, date_text))
        catalogs.append(releases)
    if catalogs[0] != catalogs[1]:
        raise ValueError("English and Russian changelog release versions/dates differ")
    if version not in [entry[0] for entry in catalogs[0]]:
        raise ValueError(f"Engine VERSION {version} has no dated bilingual changelog entry")


def validate_changelogs(root: Path) -> None:
    validate_changelog_texts(read_engine_version(root), [
        (root / path).read_text(encoding='utf-8') for path in CHANGELOG_PATHS
    ])


def validate_transition(previous: str, current: str, branch: str, date: datetime.date) -> None:
    new = parse_engine_version(current)
    if branch == 'master':
        if new.suffix != '-dev' or new.year != date.year:
            raise ValueError('master requires YEAR.MAJOR.MINOR-dev with the current UTC year')
        if previous == '2022.1.0.wip':
            if new != EngineVersion(date.year, 1, 1, None, '-dev'):
                raise ValueError('Initial policy adoption requires YEAR.1.1-dev')
            return
        old = parse_engine_version(previous)
        if old.suffix != '-dev':
            raise ValueError('master baseline must be a development version')
        expected = (new.year == old.year and new.major == old.major and new.minor == old.minor + 1)
        next_line = (new.year == old.year and new.major == old.major + 1 and new.minor == 0)
        next_year = (new.year > old.year and new.major == 1 and new.minor == 0)
        if not (expected or next_line or next_year):
            raise ValueError('master must increment minor by one, or start the next line/year at minor zero')
    elif branch.startswith('release/'):
        old = parse_engine_version(previous)
        if new.patch is None or branch != f'release/{new.year}.{new.major}':
            raise ValueError('release/YEAR.MAJOR requires YEAR.MAJOR.MINOR.PATCH[-rc]')
        if new[:3] != old[:3]:
            raise ValueError('Release patches must preserve the cut year, major and minor')
        if old.suffix == '-dev':
            valid = new.patch == 0 and new.suffix == '-rc'
        elif old.suffix == '-rc' and new.patch == old.patch and not new.suffix:
            valid = True
        else:
            valid = new.patch == old.patch + 1 and new.suffix == old.suffix
        if not valid:
            raise ValueError('Release changes increment patch by one; rc promotion only removes -rc')
    else:
        raise ValueError(f'Unsupported Engine update branch: {branch}')
