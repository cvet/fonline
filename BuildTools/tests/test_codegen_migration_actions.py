from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import codegen


@pytest.fixture(autouse=True)
def clean_codegen_state():
    codegen.reset_metadata_parse_state()
    yield
    codegen.reset_metadata_parse_state()


def parse(*rules):
    codegen.tag_metas['MigrationRule'] = [
        codegen.TagMetaRecord('Migration.cs', index, rule, None, [])
        for index, rule in enumerate(rules)
    ]
    codegen.parse_migration_rule_tags()
    return [tag.args for tag in codegen.codegen_tags['MigrationRule']]


def test_explicit_proto_actions_and_qualified_function():
    assert parse('Proto Item Rename Old New', 'Proto Item Remove Removed',
                 'Proto Item Transform Conditional Game.Migrations.Rewrite') == [
        ['Proto', 'Item', 'Rename', 'Old', 'New'],
        ['Proto', 'Item', 'Remove', 'Removed'],
        ['Proto', 'Item', 'Transform', 'Conditional', 'Game.Migrations.Rewrite'],
    ]
    assert not codegen.errors


def test_property_remove_and_qualified_component():
    assert parse('Property Critter Remove Retired', 'Property Item Remove Component.Retired') == [
        ['Property', 'Critter', 'Remove', 'Retired'],
        ['Property', 'Item', 'Remove', 'Component.Retired'],
    ]
    assert not codegen.errors


def test_angelscript_qualified_transform_functions():
    assert parse('Property Critter Transform Identity Migrations::Rewrite',
                 'Proto Item Transform Old Migrations::Prototype') == [
        ['Property', 'Critter', 'Transform', 'Identity', 'Migrations::Rewrite'],
        ['Proto', 'Item', 'Transform', 'Old', 'Migrations::Prototype'],
    ]
    assert not codegen.errors


@pytest.mark.parametrize('rule', [
    'Proto Item Old New', 'Proto Item Old __remove__',
    'Proto Item Remove Old New', 'Proto Item Rename Old __remove__',
    'Proto Item Unknown Old New', 'Property Critter Remove',
    'Property Critter Remove Old New', 'Property Critter Rename Old __remove__',
])
def test_legacy_and_malformed_actions_rejected(rule):
    assert not parse(rule)
    assert codegen.errors


def test_proto_source_cannot_have_conflicting_actions():
    assert len(parse('Proto Item Rename Old New', 'Proto Item Transform Old Rewrite')) == 1
    assert codegen.errors


def test_property_destination_can_also_have_value_function():
    assert len(parse('Property Critter Rename Old Current',
                     'Property Critter Transform Current Rewrite')) == 2
    assert not codegen.errors


@pytest.mark.parametrize('first,second', [
    ('Property Critter Remove Old', 'Property Critter Rename Old New'),
    ('Property Critter Rename Old New', 'Property Critter Remove Old'),
    ('Property Critter Remove Old', 'Property Critter Transform Old Rewrite'),
    ('Property Critter Transform Old Rewrite', 'Property Critter Remove Old'),
])
def test_property_source_cannot_have_conflicting_actions(first, second):
    assert len(parse(first, second)) == 1
    assert codegen.errors
