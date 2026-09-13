from __future__ import annotations

import hashlib
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import codegen


def parse_rule(monkeypatch: pytest.MonkeyPatch, text: str):
    record = SimpleNamespace(abs_path='Versioned.cs', line_index=0, tag_info=text, tag_context='', comment=[])
    monkeypatch.setattr(codegen, 'tag_metas', {'MigrationRule': [record]})
    monkeypatch.setattr(codegen, 'codegen_tags', {'MigrationRule': []})
    monkeypatch.setattr(codegen, 'compatibility_hasher', hashlib.sha256())
    errors = []
    monkeypatch.setattr(codegen, 'show_error', lambda *args: errors.append(args))
    codegen.parse_migration_rule_tags()
    return codegen.codegen_tags['MigrationRule'], errors


@pytest.mark.parametrize('rule', [
    'Property Actor Step LegacyStep',
    'Property Actor Quest.Step Quest.LegacyStep BeforeVersion Save.Version 3270',
])
def test_property_migration_version_registration(monkeypatch, rule):
    rules, errors = parse_rule(monkeypatch, rule)
    assert not errors
    assert len(rules) == 1
    helpers, registrations = [], []
    codegen.append_migration_rule_registration(helpers, registrations)
    generated = '\n'.join(helpers)
    if 'BeforeVersion' in rule:
        assert 'RegisterPropertyMigrationBeforeVersion("Actor", "Quest.Step", "Save.Version", "3270")' in generated
    else:
        assert 'RegisterPropertyMigrationBeforeVersion' not in generated


@pytest.mark.parametrize('rule', [
    'Property Actor Step LegacyStep BeforeVersion Version 0',
    'Property Actor Step LegacyStep BeforeVersion Version -1',
    'Property Actor Step LegacyStep BeforeVersion Version 1.5',
    'Property Actor Step LegacyStep BeforeVersion Version 9223372036854775808',
    'Property Actor Step LegacyStep BeforeVersion Version',
    'Proto Actor Step LegacyStep BeforeVersion Version 3270',
])
def test_invalid_property_migration_version_is_rejected(monkeypatch, rule):
    rules, errors = parse_rule(monkeypatch, rule)
    assert not rules
    assert len(errors) == 1
