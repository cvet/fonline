"""Migration guard: rule coverage for removed identities, retired-name reuse, and both script backends."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

import pytest


ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ENGINE_ROOT / 'BuildTools'))

import migration_guard as mg  # noqa: E402


MAIN_CONFIG = """Baking.ProtoFileExtensions = fopro fomap foitem focr

[ResourcePack]
Name = Metadata
InputDirs = Scripts
Bakers = Metadata

[ResourcePack]
Name = Protos
InputDirs = Items Critters Maps Modifiers
Bakers = Proto
"""

GUARD_CONFIG = {
    'main_config': 'Game.fomain',
    'manifest': 'Guard/identities.tsv',
    'skip_git_ignored': False,
    'file_identities': [{'type': 'Dialog', 'patterns': ['Critters/**/*.fodlg']}],
}


def manifest(live=(), retired=()) -> mg.Manifest:
    return mg.Manifest(set(live), set(retired))


def codes(violations) -> list[str]:
    return [violation.code for violation in violations]


def proto(name: str, owner: str = 'Item') -> mg.Identity:
    return ('proto', owner, name)


# ---------- pure violation logic ----------

def test_missing_rename_rule_flagged():
    violations = mg.compute_violations({proto('NewGun')}, manifest([proto('OldGun')]), set(), set(), {})
    assert ('MISSING_RULE', 'OldGun') in [(item.code, item.name) for item in violations]


def test_missing_remove_rule_flagged():
    assert codes(mg.compute_violations(set(), manifest([proto('Gone')]), set(), set(), {})) == ['MISSING_RULE']


def test_remove_rule_covers_removal_and_manifest_records_it():
    gone = proto('Gone')
    assert codes(mg.compute_violations(set(), manifest([gone]), {gone}, set(), {})) == ['STALE_MANIFEST']
    assert mg.retired_after(set(), manifest([gone]), {gone}) == {gone}
    assert mg.compute_violations(set(), manifest(retired=[gone]), {gone}, set(), {}) == []


def test_rename_rule_passes_once_the_manifest_follows():
    old, new = proto('OldGun'), proto('NewGun')
    renames = {old: 'NewGun'}
    assert codes(mg.compute_violations({new}, manifest([old]), {old}, set(), renames)) == ['STALE_MANIFEST', 'STALE_MANIFEST']
    assert mg.compute_violations({new}, manifest([new], [old]), {old}, set(), renames) == []


def test_dangling_rename_target_flagged():
    old = proto('OldGun')
    violations = mg.compute_violations(set(), manifest(retired=[old]), {old}, set(), {old: 'NewGun'})
    assert codes(violations) == ['DANGLING_TARGET']


def test_rename_chain_resolves_to_its_last_name():
    # EngineMetadata::CheckMigrationRule follows the chain, so A -> B -> C reaches the live C
    a, b, c = proto('A'), proto('B'), proto('C')
    assert mg.compute_violations({c}, manifest([c], [a, b]), {a, b}, set(), {a: 'B', b: 'C'}) == []
    assert codes(mg.compute_violations(set(), manifest(retired=[a, b]), {a, b}, set(), {a: 'B', b: 'C'})) == ['DANGLING_TARGET', 'DANGLING_TARGET']


def test_rule_is_type_scoped():
    violations = mg.compute_violations(set(), manifest([proto('X', 'Critter')]), {proto('X', 'Item')}, set(), {})
    assert [(item.code, item.owner) for item in violations if item.code == 'MISSING_RULE'] == [('MISSING_RULE', 'Critter')]


def test_new_identity_requires_manifest_update():
    violations = mg.compute_violations({proto('NewGun')}, manifest(), set(), set(), {})
    assert [(item.code, item.name) for item in violations] == [('STALE_MANIFEST', 'NewGun')]


def test_reused_retired_proto_id_flagged():
    carbine, rifle = proto('Carbine'), proto('Rifle')
    violations = mg.compute_violations({carbine, rifle}, manifest([carbine, rifle]), {carbine}, set(), {carbine: 'Rifle'})
    assert [(item.code, item.name) for item in violations] == [('REUSED_PROTO_ID', 'Carbine')]


def test_proto_transform_source():
    source = {proto('Carbine')}
    assert mg.compute_violations(source, manifest(source), set(), source, {}) == []
    assert codes(mg.compute_violations(set(), manifest(source), set(), source, {})) == ['STALE_MANIFEST']
    assert mg.compute_violations(set(), manifest(retired=source), set(), source, {}) == []
    assert codes(mg.compute_violations(source, manifest(), set(), source, {})) == ['REUSED_PROTO_ID']


def test_enum_value_rules():
    raider = ('enum', 'BodyType', 'Raider')
    assert codes(mg.compute_violations(set(), manifest([raider]), set(), set(), {})) == ['MISSING_RULE']
    assert mg.compute_violations(set(), manifest(retired=[raider]), {raider}, set(), {}) == []
    assert codes(mg.compute_violations({raider}, manifest([raider], [raider]), {raider}, set(), {})) == ['REUSED_ENUM_VALUE']


def test_property_name_rules():
    flag = ('property', 'Critter', 'Flag')
    assert codes(mg.compute_violations(set(), manifest([flag]), set(), set(), {})) == ['MISSING_RULE']
    assert codes(mg.compute_violations({flag}, manifest([flag], [flag]), set(), set(), {})) == ['REUSED_PROPERTY_NAME']
    # A rule that parks Flag elsewhere while Flag is declared again
    assert codes(mg.compute_violations({flag}, manifest([flag]), {flag}, set(), {})) == ['REUSED_PROPERTY_NAME']
    # Retirement is owner-scoped
    other = ('property', 'Item', 'Flag')
    assert mg.compute_violations({other}, manifest([other], [flag]), set(), set(), {}) == []


def test_renamed_property_old_name_is_retired():
    old, new = ('property', 'Critter', 'OldName'), ('property', 'Critter', 'NewName')
    assert mg.retired_after({new}, manifest([old]), {old}) == {old}


# ---------- fixture tree ----------

def write(root: Path, rel: str, content: str) -> None:
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding='utf-8')


def make_tree(root: Path, guard_config: dict | None = None) -> None:
    write(root, 'Game.fomain', MAIN_CONFIG)
    write(root, 'guard.json', json.dumps(guard_config or GUARD_CONFIG))
    write(root, 'Items/Guns.foitem', '[ProtoItem]\n$Name = Pistol\n\n[ProtoItem]\n$Name = Rifle\n')
    write(root, 'Critters/Npc.focr', '[ProtoCritter]\n')
    write(root, 'Critters/Dialogs/Talk.fodlg', 'greeting\n')
    write(root, 'Modifiers/Ability.fopro', '[ProtoModifier]\n')
    write(root, 'Modifiers/Badges.foinfo', '[MapCritterBadge]\nx = 1\n')
    write(root, 'Maps/Snowfield.fomap', '[ProtoMap]\nOutside = True\n[$Name/Item]\n$Proto = SnowPile\n')
    write(root, 'Maps/MultiZone.fomap', '[ProtoMap]\n$Name = ZoneA\n[$Name/Item]\n$Proto = X\n[ProtoMap]\n$Name = ZoneB\n')
    write(root, 'Disabled/Items/Old.foitem', '[ProtoItem]\n$Name = OldDisabled\n')
    write(root, 'Scripts/Migration.fos', '// no rules\n')


def guard(root: Path, *args: str) -> int:
    return mg.main(['--root', str(root), '--config', 'guard.json', *args])


def declarations(root: Path) -> mg.Declarations:
    args = mg.create_parser().parse_args(['--root', str(root), '--config', 'guard.json', '--check'])
    return mg.collect_declarations(mg.load_guard_config(args))


def names(found: mg.Declarations, kind: str) -> set[tuple[str, str]]:
    return {(owner, name) for item_kind, owner, name in found.current if item_kind == kind}


def test_collect_prototype_identities(tmp_path):
    root = tmp_path.resolve()
    make_tree(root)
    found = declarations(root)

    assert found.violations == []
    assert names(found, 'proto') == {
        ('Item', 'Pistol'), ('Item', 'Rifle'),          # $Name per section; a named file stem is no identity
        ('Critter', 'Npc'),                             # file stem without $Name
        ('Dialog', 'Talk'),                             # configured file identity
        ('Modifier', 'Ability'),
        ('Map', 'Snowfield'),                           # anonymous anchor takes the stem; nested section skipped
        ('Map', 'ZoneA'), ('Map', 'ZoneB'),             # named multi-map anchors
    }


def test_collection_errors_are_reported(tmp_path):
    root = tmp_path.resolve()
    make_tree(root)
    write(root, 'Items/Dup.foitem', '[ProtoItem]\n$Name = Pistol\n')
    found = declarations(root)
    assert [(item.code, item.location) for item in found.violations] == [('INVALID_CONTENT', 'Items/Guns.foitem:1')]
    assert 'DUPLICATE_PROTO' in found.violations[0].message


def test_git_ignored_files_are_skipped(tmp_path):
    if shutil.which('git') is None:
        pytest.skip('git is not installed')
    root = tmp_path.resolve()
    make_tree(root, {**GUARD_CONFIG, 'skip_git_ignored': True})
    write(root, '.gitignore', 'Maps/Generated/\n')
    write(root, 'Maps/Travel/Real.fomap', '[ProtoMap]\n')
    write(root, 'Maps/Generated/Compose.fomap', '[ProtoMap]\n')
    subprocess.run(['git', 'init', '-q'], cwd=root, check=True)

    found = names(declarations(root), 'proto')
    assert ('Map', 'Real') in found and ('Map', 'Compose') not in found


def test_git_required_outside_a_repository(tmp_path, capsys):
    if shutil.which('git') is None:
        pytest.skip('git is not installed')
    root = tmp_path.resolve()
    make_tree(root, {**GUARD_CONFIG, 'skip_git_ignored': True})
    probe = subprocess.run(['git', 'rev-parse', '--git-dir'], cwd=root, capture_output=True)
    if probe.returncode == 0:
        pytest.skip('the temporary directory is inside a git repository')
    assert guard(root, '--update-manifest') == 2
    assert 'skip_git_ignored' in capsys.readouterr().err


@pytest.mark.parametrize('extension', ['cs', 'fos'])
def test_property_declarations_and_ref_type_rules(tmp_path, extension):
    root = tmp_path.resolve()
    make_tree(root)
    write(root, f'Scripts/Quests.{extension}',
          '    ///@ Property Critter Server bool JimHostile Mutable Persistent\n'
          '    ///@ Property Item Common ident Locker.LockerId Mutable Persistent\n'
          '    ///@ Property Critter Server ident=>int32 BBMsgCount Mutable Persistent\n'
          '    ///@ Property Map Server int32=>uint8[] CompRiddleData Mutable Persistent\n'
          '    ///@ RefType Server RouteSnapshot\n'
          '    ///@ Property RouteSnapshot Server int32 Steps\n'
          '    ///@ MigrationRule Property Critter Rename OldHostile JimHostile\n'
          '    ///@ MigrationRule Property RouteSnapshotRefType Rename OldSteps Steps\n')
    found = declarations(root)

    assert found.violations == []
    assert names(found, 'property') == {('Critter', 'JimHostile'), ('Item', 'Locker.LockerId'), ('Critter', 'BBMsgCount'),
                                         ('Map', 'CompRiddleData'), ('RouteSnapshot', 'Steps')}
    retiring, _, _ = mg.rule_sets(found.rules, found.ref_types)
    assert retiring == {('property', 'Critter', 'OldHostile'), ('property', 'RouteSnapshot', 'OldSteps')}


def test_angelscript_migration_module_shapes(tmp_path):
    # The layout of a real AngelScript Migration.fos: namespace, role guard, comments and dotted component names
    root = tmp_path.resolve()
    make_tree(root)
    write(root, 'Scripts/Migration.fos', """// Migration rules for serialized data
namespace Migration
{

#if SERVER

// Idle animations are chosen at runtime now
///@ MigrationRule Property Critter Remove ActionAnimAlive
///@ MigrationRule Property Critter Rename ActionAnimKnockout KnockoutActionAnim
///@ MigrationRule Property Item Rename Grid_ToMap Grid.ToMap // flat field moved into the Grid component
///@ MigrationRule Proto Item Rename OldContainer NewContainer
///@ MigrationRule Enum QuestState Aborted Failed

#endif

}
""")
    write(root, 'Scripts/MigrationQuests.fos', """namespace MigrationQuests
{
    ///@ Property Critter Common int32 QuestStage Mutable Persistent PublicSync
    ///@ Property Item Common bool Grid Component
    ///@ Property Item Common hstring Grid.ToMap Mutable Persistent
    ///@ MigrationRule Property Critter Transform QuestStage MigrationQuests::RewriteQuestStage

    [[PropertyMigrator]]
    bool RewriteQuestStage(int&inout value, const DatabaseDocument&inout document)
    {
        return false;
    }
}
""")
    found = declarations(root)

    assert found.violations == []
    assert [str(rule) for rule in found.rules] == [
        'Property Critter Remove ActionAnimAlive',
        'Property Critter Rename ActionAnimKnockout KnockoutActionAnim',
        'Property Item Rename Grid_ToMap Grid.ToMap',
        'Proto Item Rename OldContainer NewContainer',
        'Enum QuestState Aborted Failed',
        'Property Critter Transform QuestStage MigrationQuests::RewriteQuestStage',
    ]
    assert {('Item', 'Grid'), ('Item', 'Grid.ToMap'), ('Critter', 'QuestStage')} <= names(found, 'property')
    retiring, migrating, renames = mg.rule_sets(found.rules, found.ref_types)
    assert ('property', 'Critter', 'QuestStage') not in retiring | migrating
    assert ('enum', 'QuestState', 'Aborted') in retiring
    assert renames == {('proto', 'Item', 'OldContainer'): 'NewContainer'}


def test_managed_and_angelscript_rules_are_equivalent(tmp_path):
    results = []
    for extension, body in (('cs', 'namespace MyGame;\n\nusing FOnline;\n\n{rules}\npublic static partial class GameMigrations\n{{\n}}\n'),
                            ('fos', 'namespace Migration\n{{\n{rules}}}\n')):
        root = (tmp_path / extension).resolve()
        make_tree(root)
        rules = ('///@ Enum BodyType Men\n'
                 '///@ MigrationRule Enum BodyType Raider Men\n'
                 '///@ MigrationRule Proto Modifier Rename "Local knowledge" Ability\n'
                 '///@ MigrationRule Proto Item Transform OldVariant GameMigrations.MigrateVariant\n')
        write(root, f'Scripts/Migration.{extension}', body.format(rules=rules))
        found = declarations(root)
        assert found.violations == []
        results.append((found.current, [str(rule) for rule in found.rules]))

    assert results[0] == results[1]


def test_malformed_rules_are_violations(tmp_path):
    root = tmp_path.resolve()
    make_tree(root)
    write(root, 'Scripts/Migration.fos',
          '///@ MigrationRule Proto Modifier Rename Local knowledge LocalKnowledge\n'
          '///@ MigrationRule Proto Modifier Rename "Local knowledge" "New Name"\n'
          '///@ MigrationRule Proto Item Rename A B\n'
          '///@ MigrationRule Proto Item Remove A\n'
          '///@ Property Critter Server int32\n')
    found = declarations(root)
    assert [(item.code, item.location) for item in found.violations] == [
        ('INVALID_RULE', 'Scripts/Migration.fos:1'),
        ('INVALID_RULE', 'Scripts/Migration.fos:2'),
        ('INVALID_TAG', 'Scripts/Migration.fos:5'),
        ('INVALID_RULE', 'Scripts/Migration.fos:4'),
    ]


@pytest.mark.parametrize('initializer', ['= "Game"', '{"Game"}'])
def test_native_sources(tmp_path, initializer):
    root = tmp_path.resolve()
    make_tree(root, {**GUARD_CONFIG, 'native_sources': ['Native/**/*.h'], 'exclude': ['Native/Tests/**']})
    write(root, 'Native/Common/EntityProperties.h',
          'class GameProperties : public EntityProperties\n{\n'
          f'    static constexpr string_view ENTITY_TYPE_NAME {initializer};\n'
          '    ///@ ExportProperty Common Persistent\n'
          '    FO_ENTITY_PROPERTY(synctime, SynchronizedTime);\n'
          '    ///@ MigrationRule Property Game Rename OldTime SynchronizedTime\n};\n')
    write(root, 'Native/Common/Enums.h',
          '///@ ExportEnum\nenum class CritterCondition : uint8_t\n{\n    Alive = 0,\n\n    // Lying on the ground\n'
          '    Knockout = 1, // trailing\n    Dead,\n};\nenum class NotExported\n{\n    Hidden,\n};\n'
          'auto marker {"///@ MigrationRule Proto Item Remove InsideStringLiteral"};\n')
    write(root, 'Native/Tests/Test_Enum.h', '///@ ExportEnum\nenum class TestOnly\n{\n    Fixture,\n};\n')
    found = declarations(root)

    assert names(found, 'property') == {('Game', 'SynchronizedTime')}
    assert names(found, 'enum') == {('CritterCondition', 'Alive'), ('CritterCondition', 'Knockout'), ('CritterCondition', 'Dead')}
    assert [str(rule) for rule in found.rules] == ['Property Game Rename OldTime SynchronizedTime']


# ---------- command line ----------

def output(capsys) -> str:
    return capsys.readouterr().out


def test_update_then_check_passes(tmp_path, capsys):
    root = tmp_path.resolve()
    make_tree(root)
    assert guard(root, '--update-manifest') == 0
    lines = (root / 'Guard/identities.tsv').read_text(encoding='utf-8').splitlines()
    assert lines[0] == mg.MANIFEST_HEADER and 'proto\tlive\tItem\tPistol' in lines
    assert guard(root, '--check') == 0
    assert 'OK:' in output(capsys)


def test_check_without_manifest_fails(tmp_path, capsys):
    root = tmp_path.resolve()
    make_tree(root)
    assert guard(root, '--check') == 1
    assert 'manifest' in output(capsys)


def test_removed_proto_without_rule_fails_and_cannot_be_laundered(tmp_path, capsys):
    root = tmp_path.resolve()
    make_tree(root)
    assert guard(root, '--update-manifest') == 0
    (root / 'Items/Guns.foitem').unlink()
    output(capsys)

    assert guard(root, '--check') == 1
    assert output(capsys).count('MISSING_RULE') == 2
    assert guard(root, '--update-manifest') == 1
    assert 'refusing' in output(capsys)


def test_move_out_of_the_packs_needs_a_rule(tmp_path, capsys):
    root = tmp_path.resolve()
    make_tree(root)
    assert guard(root, '--update-manifest') == 0
    (root / 'Critters/Dialogs/Talk.fodlg').unlink()
    write(root, 'Disabled/Dialogs/Talk.fodlg', 'greeting\n')
    (root / 'Modifiers/Ability.fopro').rename(root / 'Disabled/Ability.fopro')
    output(capsys)

    assert guard(root, '--check') == 1
    text = output(capsys)
    assert 'MISSING_RULE\tproto\tDialog\tTalk' in text and 'MISSING_RULE\tproto\tModifier\tAbility' in text


def test_removed_proto_with_remove_rule_passes_after_update(tmp_path, capsys):
    root = tmp_path.resolve()
    make_tree(root)
    assert guard(root, '--update-manifest') == 0
    (root / 'Modifiers/Ability.fopro').unlink()
    write(root, 'Scripts/Migration.fos', '///@ MigrationRule Proto Modifier Remove Ability\n')

    assert guard(root, '--check') == 1
    assert 'STALE_MANIFEST\tproto\tModifier\tAbility' in output(capsys)
    assert guard(root, '--update-manifest') == 0
    assert 'proto\tretired\tModifier\tAbility' in (root / 'Guard/identities.tsv').read_text(encoding='utf-8')
    assert guard(root, '--check') == 0


def test_added_proto_without_update_fails_then_update_passes(tmp_path, capsys):
    root = tmp_path.resolve()
    make_tree(root)
    assert guard(root, '--update-manifest') == 0
    write(root, 'Items/New.foitem', '[ProtoItem]\n$Name = NewGun\n')
    output(capsys)

    assert guard(root, '--check') == 1
    assert 'STALE_MANIFEST\tproto\tItem\tNewGun' in output(capsys)
    assert guard(root, '--update-manifest') == 0
    assert guard(root, '--check') == 0


def test_deleted_property_needs_a_rule_and_its_name_cannot_return(tmp_path, capsys):
    root = tmp_path.resolve()
    make_tree(root)
    write(root, 'Scripts/Quest.cs', '///@ Property Critter Server int32 QuestStep Mutable Persistent\n')
    assert guard(root, '--update-manifest') == 0

    write(root, 'Scripts/Quest.cs', '// retired\n')
    assert guard(root, '--check') == 1
    assert 'MISSING_RULE\tproperty\tCritter\tQuestStep' in output(capsys)

    write(root, 'Scripts/Migration.fos', '///@ MigrationRule Property Critter Remove QuestStep\n')
    assert guard(root, '--update-manifest') == 0
    assert guard(root, '--check') == 0

    # Declaring the name again, whatever its type, fails even after the tombstone rule is deleted
    write(root, 'Scripts/Migration.fos', '// rules\n')
    write(root, 'Scripts/Quest.cs', '///@ Property Critter Server bool QuestStep Mutable Persistent\n')
    output(capsys)
    assert guard(root, '--check') == 1
    assert 'REUSED_PROPERTY_NAME\tproperty\tCritter\tQuestStep' in output(capsys)
    assert guard(root, '--update-manifest') == 1


def test_retired_enum_value_cannot_return(tmp_path, capsys):
    root = tmp_path.resolve()
    make_tree(root)
    write(root, 'Scripts/Body.fos', '///@ Enum BodyType Men\n///@ Enum BodyType Raider\n')
    assert guard(root, '--update-manifest') == 0

    write(root, 'Scripts/Body.fos', '///@ Enum BodyType Men\n///@ MigrationRule Enum BodyType Raider Men\n')
    assert guard(root, '--update-manifest') == 0
    assert guard(root, '--check') == 0

    write(root, 'Scripts/Body.fos', '///@ Enum BodyType Men\n///@ Enum BodyType Raider\n///@ MigrationRule Enum BodyType Raider Men\n')
    output(capsys)
    assert guard(root, '--check') == 1
    assert 'REUSED_ENUM_VALUE' in output(capsys)
    assert guard(root, '--update-manifest') == 1


def test_property_remove_cannot_be_laundered_on_first_update(tmp_path, capsys):
    root = tmp_path.resolve()
    make_tree(root)
    write(root, 'Scripts/Migration.cs', '///@ MigrationRule Property Critter Remove Deleted\n'
                                        '///@ Property Critter Server bool Deleted Mutable Persistent\n')
    assert guard(root, '--update-manifest') == 1
    assert 'REUSED_PROPERTY_NAME' in output(capsys)
    assert not (root / 'Guard/identities.tsv').exists()


def test_last_ref_type_field_remove_keeps_the_declared_owner(tmp_path):
    root = tmp_path.resolve()
    make_tree(root)
    write(root, 'Scripts/Snapshot.cs', '///@ RefType Server Snapshot\n///@ Property Snapshot Server int32 LastField\n')
    assert guard(root, '--update-manifest') == 0

    write(root, 'Scripts/Snapshot.cs', '///@ MigrationRule Property SnapshotRefType Remove LastField\n///@ RefType Server Snapshot\n')
    assert guard(root, '--update-manifest') == 0
    recorded = mg.read_manifest(root / 'Guard/identities.tsv')
    assert ('property', 'Snapshot', 'LastField') in recorded.retired
    assert ('property', 'SnapshotRefType', 'LastField') not in recorded.retired
    assert guard(root, '--check') == 0


def test_renamed_proto_with_rule(tmp_path, capsys):
    root = tmp_path.resolve()
    make_tree(root)
    assert guard(root, '--update-manifest') == 0
    write(root, 'Items/Guns.foitem', '[ProtoItem]\n$Name = Pistol\n\n[ProtoItem]\n$Name = Carbine\n')
    write(root, 'Scripts/Migration.fos', '///@ MigrationRule Proto Item Rename Rifle Carbine\n')
    assert guard(root, '--update-manifest') == 0
    assert guard(root, '--check') == 0

    write(root, 'Scripts/Migration.fos', '///@ MigrationRule Proto Item Rename Rifle Musket\n')
    output(capsys)
    assert guard(root, '--check') == 1
    assert 'DANGLING_TARGET\tproto\tItem\tRifle' in output(capsys)


def test_sub_config_and_explicit_directories(tmp_path, capsys):
    root = tmp_path.resolve()
    make_tree(root)
    write(root, 'Game.fomain', MAIN_CONFIG + '\n[SubConfig]\nName = NoModifiers\nBaking.IgnoreInputDirs = Modifiers\n')
    assert guard(root, '--update-manifest', '--sub-config', 'NoModifiers') == 0
    assert 'Ability' not in (root / 'Guard/identities.tsv').read_text(encoding='utf-8')

    write(root, 'explicit.json', json.dumps({'manifest': 'Guard/explicit.tsv', 'skip_git_ignored': False,
                                             'content_dirs': ['Items'], 'script_dirs': ['Scripts'], 'proto_extensions': ['foitem']}))
    (root / 'Game.fomain').unlink()
    assert mg.main(['--root', str(root), '--config', 'explicit.json', '--update-manifest']) == 0
    assert mg.read_manifest(root / 'Guard/explicit.tsv').live == {proto('Pistol'), proto('Rifle')}


@pytest.mark.parametrize(('config', 'message'), [
    ('{"manifest": "m.tsv", "unknown": 1}', 'Unknown guard config keys'),
    ('{"main_config": "Game.fomain"}', 'No manifest path'),
    ('[1]', 'JSON object'),
    ('{"manifest": "m.tsv", "skip_git_ignored": "yes"}', 'true or false'),
    ('not json', 'Cannot read guard config'),
])
def test_config_errors_exit_2(tmp_path, capsys, config, message):
    root = tmp_path.resolve()
    make_tree(root)
    write(root, 'guard.json', config)
    assert guard(root, '--check') == 2
    assert message in capsys.readouterr().err


def test_corrupt_manifest_exits_2(tmp_path, capsys):
    root = tmp_path.resolve()
    make_tree(root)
    write(root, 'Guard/identities.tsv', 'proto\tlive\tItem\n')
    assert guard(root, '--check') == 2
    assert 'manifest line' in capsys.readouterr().err
