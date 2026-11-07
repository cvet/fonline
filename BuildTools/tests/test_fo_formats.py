"""Engine-format readers checked against values derived from the Engine parsers and their tests.

Each case names the Engine source or the `Source/Tests` section that fixes the expected value, so a
disagreement points at either the reader or the Engine rather than at this file.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest


ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ENGINE_ROOT / 'BuildTools'))

import fo_formats as ff  # noqa: E402


DATA = Path(__file__).resolve().parent / 'data' / 'fo_formats'
PROJECT = DATA / 'Project'
EXPECTED = json.loads((DATA / 'expected.json').read_text(encoding='utf-8'))


def section(text: str, name: str = '', **options: bool) -> dict[str, str]:
    found = ff.parse_config_text(text, **options).first(name)
    assert found is not None
    return found.values


# ---------- configuration grammar: Source/Tests/Test_ConfigFile.cpp ----------

@pytest.mark.parametrize('suffix', ['', '\n'])
def test_terminal_continuation_with_or_without_final_newline(suffix):
    # PreservesTerminalContinuationWithOrWithoutFinalNewline
    assert section('Size = \\' + suffix) == {'Size': ''}
    assert section('Name = first \\' + suffix) == {'Name': 'first'}


def test_append_joins_with_one_space_and_starts_a_missing_key():
    # StoresViewsIntoOwnedInput, AppendsIntoMissingKeyWithoutLeadingSpace, IgnoresEmptyAppendedValueForExistingKey
    assert section('[ProtoItem]\nName = Base\nName += Extra\n', 'ProtoItem')['Name'] == 'Base Extra'
    assert section('[ProtoItem]\nName += Two\n', 'ProtoItem')['Name'] == 'Two'
    assert section('[ProtoItem]\nName = Base\nName +=    # ignored\n', 'ProtoItem')['Name'] == 'Base'


def test_repeated_key_replaces_the_earlier_value():
    # ConfigFile stores (*cur_section)[key] = value
    assert section('[S]\nKey = one\nKey = two\n', 'S') == {'Key': 'two'}


def test_crlf_lines_and_continuation():
    # ParsesCrLfLinesAndContinuation
    values = section('[Section]\r\nline = 1 \\\r\n2\r\nKey = Value\r\n', 'Section')
    assert values == {'line': '1 2', 'Key': 'Value'}


def test_form_feed_and_vertical_tab_are_config_whitespace():
    # TreatsFormFeedAndVerticalTabAsConfigWhitespace
    values = section('[Section]\n\fCount\v=\f42\v\n\vEnabled\f=\vtrue\f\n', 'Section')
    assert values == {'Count': '42', 'Enabled': 'true'}


@pytest.mark.parametrize(('line', 'expected'), [
    ('Text = keep\\#hash # strip this', 'keep\\#hash'),
    ('Text = "quoted # hash" # strip this', '"quoted # hash"'),
    ('Text = "quoted \\" # hash" # strip this', '"quoted \\" # hash"'),
    ('Text = a#b', 'a'),
])
def test_comment_boundaries(line, expected):
    # PreservesEscapedCommentCharacters, PreservesCommentCharactersInsideDoubleQuotes,
    # PreservesCommentCharactersAfterEscapedQuotesInsideDoubleQuotes
    assert section(f'[Section]\n{line}\nOther = value\n', 'Section') == {'Text': expected, 'Other': 'value'}


def test_quotes_survive_continuation_and_append():
    # PreservesCommentCharactersInsideDoubleQuotesAcrossContinuedLines, PreservesCommentCharactersInsideQuotedAppendedValues
    assert section('[S]\nText = "quoted # hash" \\\ncontinued # tail\n', 'S')['Text'] == '"quoted # hash" continued'
    assert section('[S]\nText = base\nText += "quoted # hash" # strip\n', 'S')['Text'] == 'base "quoted # hash"'


def test_brace_lines_and_malformed_entries_are_ignored():
    # SkipsBraceFormatLines, IgnoresMalformedSectionsAndEntries, IgnoresEntriesWithEmptyTrimmedKeys
    document = ff.parse_config_text('[]\nNoSeparator\n[ValidSection\n[Good]\n{100}{20}{Payload}\n   = Ignored\n\t+= IgnoredToo\nKey = Value\n')
    assert [item.name for item in document.sections] == ['', 'Good']
    assert document.first('Good').values == {'Key': 'Value'}


def test_header_without_name_keeps_following_keys_in_previous_section():
    # An ignored header does not open a section
    document = ff.parse_config_text('[A]\nx = 1\n[]\ny = 2\n[B\nz = 3\n')
    assert document.first('A').values == {'x': '1', 'y': '2', 'z': '3'}


def test_repeated_and_nested_sections_in_file_order():
    # ReturnsRepeatedSections, StoresNestedSectionNamesVerbatim, ExposesSectionsInFileOrder, SkipsNestedSectionsWhenRequested
    text = '[ProtoMap]\n$Name = MapOne\n[$Name/Item]\nKind = FromOne\n[ProtoMap]\n$Name = MapTwo\n[$Name/Critter]\nKind = Two\n'
    document = ff.parse_config_text(text)
    assert [item.name for item in document.sections] == ['', 'ProtoMap', '$Name/Item', 'ProtoMap', '$Name/Critter']
    assert [item.values['$Name'] for item in document.all('ProtoMap')] == ['MapOne', 'MapTwo']

    skipped = ff.parse_config_text(text, skip_nested_sections=True)
    assert [item.name for item in skipped.sections] == ['', 'ProtoMap', 'ProtoMap']
    assert skipped.sections[1].values == {'$Name': 'MapOne'}


def test_key_lines_cover_continuations_and_appends():
    document = ff.parse_config_text('[S]\nA = 1 \\\n2\nB = x\nB += y\n')
    assert document.first('S').key_lines == {'A': (2, 3), 'B': (4, 5)}


def test_byte_order_mark_is_not_stripped():
    # ConfigFile reads bytes: a BOM keeps the first header from starting with '['
    document = ff.parse_config_text('\ufeff[ProtoItem]\n$Name = X\n')
    assert [item.name for item in document.sections] == ['']
    assert document.main.values == {'$Name': 'X'}


def test_foconfig_reads_through_the_engine_grammar():
    import foconfig

    parser = foconfig.ConfigParser()
    parser.loadFromLines(['Key = "a # b" # c\n', 'List = one\n', 'List += two\n', '[Pack] trailing\n', 'Name = P\n'])
    assert parser.mainSection().content == {'Key': '"a # b"', 'List': 'one two'}
    assert parser.getSections('Pack')[0].getStr('Name') == 'P'


# ---------- setting directives: Source/Common/Settings.cpp ResolveSettingValue ----------

def test_env_and_file_directives(tmp_path):
    (tmp_path / 'VERSION').write_text('  4.5.6 \n', encoding='utf-8')
    warnings: list[str] = []

    assert ff.resolve_setting_value('S', 'v$ENV{GAME}-$FILE{VERSION}', tmp_path, environ={'GAME': 'x'}, warnings=warnings) == 'vx-4.5.6'
    assert warnings == []
    assert ff.resolve_setting_value('S', '$ENV{MISSING} $FILE{none.txt}', tmp_path, environ={}, warnings=warnings) == '$ENV{MISSING} $FILE{none.txt}'
    assert len(warnings) == 2
    assert ff.resolve_setting_value('S', 'cost $5 and $OTHER{x}', tmp_path, environ={}) == 'cost $5 and $OTHER{x}'


def test_target_directives_stay_verbatim_only_while_baking(tmp_path):
    assert ff.resolve_setting_value('S', '$TARGET_ENV{HOME}', tmp_path, environ={'HOME': '/h'}) == '$TARGET_ENV{HOME}'
    assert ff.resolve_setting_value('S', '$TARGET_ENV{HOME}', tmp_path, baking_mode=False, environ={'HOME': '/h'}) == '/h'


def test_unclosed_directive_is_rejected(tmp_path):
    with pytest.raises(ff.FormatError, match='Not closed'):
        ff.resolve_setting_value('S', '$ENV{OPEN', tmp_path, environ={})


# ---------- project configuration, resource packs and file walk ----------

def test_golden_project_config():
    config = ff.load_project_config(PROJECT / 'Golden.fomain')
    expected = EXPECTED['config']

    assert config.settings == expected['settings']
    assert {pack.name: {'input_dirs': [path.relative_to(PROJECT.resolve()).as_posix() for path in pack.input_dirs],
                        'bakers': pack.bakers, 'client_only': pack.client_only}
            for pack in config.resource_packs} == expected['packs']
    assert config.setting('Baking.AllowRepeatedProtoParents') == 'True'
    assert config.setting_list('Baking.ProtoFileExtensions') == ['fopro', 'fomap']


def test_golden_project_sub_config():
    config = ff.load_project_config(PROJECT / 'Golden.fomain', sub_config='Smoke')
    expected = EXPECTED['config_smoke']

    assert {key: config.settings[key] for key in expected['settings']} == expected['settings']
    protos = next(pack for pack in config.resource_packs if pack.name == 'Protos')
    assert [path.relative_to(PROJECT.resolve()).as_posix() for path in protos.input_dirs] == expected['protos_input_dirs']
    assert len(next(pack for pack in config.declared_resource_packs if pack.name == 'Protos').input_dirs) == 2


def test_config_rejections(tmp_path):
    cases = {
        '[ResourcePack]\nInputDirs = A\n': 'Resource pack name not specified',
        '[ResourcePack]\nName = P\nServerOnly = True\nClientOnly = True\n': 'common or server',
        '[SubConfig]\nParent = Missing\nName = X\n': 'Parent sub config not found',
        'Baking.IgnoreInputDirs = Nope\n[ResourcePack]\nName = P\nInputDirs = A\n': 'not an input directory',
    }
    for text, message in cases.items():
        path = tmp_path / 'Case.fomain'
        path.write_text(text, encoding='utf-8')
        with pytest.raises(ff.FormatError, match=message):
            ff.load_project_config(path)

    with pytest.raises(ff.FormatError, match='Sub config not found'):
        ff.load_project_config(PROJECT / 'Golden.fomain', sub_config='Absent')


@pytest.mark.parametrize(('path', 'pattern', 'expected'), [
    ('a.fopro', '*', True),
    ('d/a.fopro', '*', False),
    ('d/a.fopro', '**', True),
    ('a.fomap', '**/*.fomap', True),
    ('d/e/_x.fomap', '**/_*.fomap', True),
    ('d/x_.fomap', '**/_*.fomap', False),
    ('ab', 'a?', True),
    ('a/', 'a?', False),
    ('Scenery/House1/x.png', 'Scenery/House*/**', True),
])
def test_resource_glob(path, pattern, expected):
    # FileSystem.cpp MatchResourcePathGlob
    assert ff.match_resource_glob(path, pattern) is expected


def test_golden_prototype_files():
    config = ff.load_project_config(PROJECT / 'Golden.fomain')
    assert sorted(item.path for item in ff.project_prototype_files(config)) == EXPECTED['prototype_files']['files']


def test_first_input_dir_wins_and_missing_dir_fails(tmp_path):
    for root in ('A', 'B'):
        (tmp_path / root).mkdir()
        (tmp_path / root / 'same.fopro').write_text(root, encoding='utf-8')
    pack = ff.ResourcePack('P', tmp_path, [tmp_path / 'A', tmp_path / 'B'], [], [], [], ['Proto'])
    assert [item.file.parent.name for item in ff.pack_files(pack)] == ['A']

    missing = ff.ResourcePack('P', tmp_path, [tmp_path / 'Missing'], [], [], [], ['Proto'])
    with pytest.raises(ff.FormatError, match='Directory not found'):
        ff.pack_files(missing)

    archive = ff.ResourcePack('P', tmp_path, [], [tmp_path / 'data.zip'], [], [], ['Proto'])
    with pytest.raises(ff.FormatError, match='archive'):
        ff.pack_files(archive)
    assert ff.pack_files(archive, skip_archives=True) == []


# ---------- prototypes: Source/Tools/ProtoBaker.cpp, Source/Tests/Test_ProtoBaker.cpp ----------

def resolve_text(text: str, path: str = 'Items/Case.fopro', **options):
    prototypes = ff.collect_prototypes([(path, text)])
    resolved, issues = ff.resolve_prototypes(prototypes, **options)
    return resolved, prototypes.issues + issues


def test_golden_prototypes():
    config = ff.load_project_config(PROJECT / 'Golden.fomain')
    _, resolved, issues = ff.load_project_prototypes(config)
    expected = {key: value for key, value in EXPECTED['prototypes'].items() if key != 'derivation'}

    assert issues == []
    assert {f'{type_name} {name}': value.properties for (type_name, name), value in resolved.items()} == expected
    assert resolved[('Item', 'TwoParents')].property_sources == {'Weight': 'RightParent', 'Cost': 'TwoParents'}
    assert resolved[('Item', 'DiamondChild')].ancestors == ['DiamondBase', 'DiamondVariant', 'DiamondOther']


def test_golden_prototype_texts():
    config = ff.load_project_config(PROJECT / 'Golden.fomain')
    _, resolved, _ = ff.load_project_prototypes(config)
    languages = ff.parse_bake_languages(config.setting_list('Baking.BakeLanguages'))
    packs: dict[str, dict[str, list[ff.TextEntry]]] = {}
    unsupported = []

    for prototype in resolved.values():
        entries, issues = ff.prototype_text_entries(prototype, languages)
        unsupported += issues
        for language, items in entries.items():
            for entry in items:
                packs.setdefault(language, {}).setdefault(entry.collection, []).append(entry)

    normalized = ff.normalize_language_packs(languages, packs)
    for language in ('engl', 'russ'):
        actual = {f'{entry.collection} {entry.key1} {entry.key2}': entry.text
                  for entry in normalized[language]['Items'] if entry.key1 == 'ChildItem'}
        assert actual == EXPECTED['prototype_texts'][language]

    assert [issue.code for issue in unsupported] == ['UNSUPPORTED_LANGUAGE']


def test_repeated_parent_forbidden():
    # RejectsRepeatedProtoParentWhenNotAllowed, AcceptsUnrelatedParentsAndPlainChainsWhenRepetitionIsForbidden
    config = ff.load_project_config(PROJECT / 'Golden.fomain')
    sources = ff.read_pack_files(ff.project_metadata_sources(config))
    migrations = ff.MigrationRules.from_tags(tag for path, text in sources for tag in ff.iter_metadata_tags(text, path))
    prototypes = ff.collect_prototypes(ff.read_pack_files(ff.project_prototype_files(config)), migrations=migrations)
    resolved, issues = ff.resolve_prototypes(prototypes, allow_repeated_parents=False, migrations=migrations)

    assert [(issue.code, issue.message.split()[1]) for issue in issues] == [('REPEATED_PARENT', 'DiamondChild')]
    assert ('Item', 'DiamondChild') not in resolved
    assert resolved[('Item', 'DiamondVariant')].properties['LightDistance'] == '2'


@pytest.mark.parametrize('allow_repeated', [True, False])
@pytest.mark.parametrize('text', [
    '[ProtoItem]\n$Name = SelfCycle\n$Parent = SelfCycle\n',
    '[ProtoItem]\n$Name = CycleA\n$Parent = CycleB\n[ProtoItem]\n$Name = CycleB\n$Parent = CycleA\n',
    '[ProtoItem]\n$Name = LongA\n$Parent = LongC\n[ProtoItem]\n$Name = LongB\n$Parent = LongA\n[ProtoItem]\n$Name = LongC\n$Parent = LongB\n',
])
def test_parent_cycles_fail_under_every_setting(text, allow_repeated):
    # RejectsProtoParentCycleUnderEverySetting: every prototype on the cycle fails
    resolved, issues = resolve_text(text, allow_repeated_parents=allow_repeated)
    assert resolved == {}
    assert issues and {issue.code for issue in issues} == {'PARENT_CYCLE'}


def test_missing_and_cross_type_parents():
    # RejectsMissingDirectProtoParent, RejectsProtoParentFromAnotherType
    _, issues = resolve_text('[ProtoItem]\n$Name = Orphan\n$Parent = Missing\n')
    assert [issue.code for issue in issues] == ['MISSING_PARENT']

    _, issues = resolve_text('[ProtoItem]\n$Name = SharedBase\n[ProtoCritter]\n$Name = WrongChild\n$Parent = SharedBase\n')
    assert [issue.code for issue in issues] == ['MISSING_PARENT']

    _, issues = resolve_text('[ProtoItem]\n$Name = A\n$Parent = B\n[ProtoItem]\n$Name = B\n$Parent = Gone\n')
    assert [issue.code for issue in issues] == ['MISSING_PARENT', 'MISSING_PARENT']
    assert 'parent of B' in issues[0].message


def test_rightmost_parent_wins_and_child_replaces_whole_values():
    resolved, issues = resolve_text('[ProtoItem]\n$Name = A\nList = 1 2\nX = a\n[ProtoItem]\n$Name = B\nX = b\n'
                                    '[ProtoItem]\n$Name = C\n$Parent = A B\nList = 3\n')
    assert issues == []
    assert resolved[('Item', 'C')].properties == {'List': '3', 'X': 'b'}


def test_name_rules_and_duplicates():
    # RejectsWhitespaceInExplicitProtoName, RejectsWhitespaceInFileStemProtoName, RejectsDuplicateProtoIdsAcrossFiles,
    # RejectsCollidingAnonymousMapAnchors
    prototypes = ff.collect_prototypes([
        ('Items/Bad.fopro', '[ProtoItem]\n$Name = Bad Name\n'),
        ('Items/Bad Name.fopro', '[ProtoItem]\n'),
        ('Items/Slash.fopro', '[ProtoItem]\n$Name = A/B\n'),
        ('Items/One.fopro', '[ProtoItem]\n$Name = Duplicate\n'),
        ('Items/Two.fopro', '[ProtoItem]\n$Name = Duplicate\n'),
        ('Maps/Collide.fomap', '[ProtoMap]\nOutside = True\n[ProtoMap]\nOutside = False\n'),
    ])
    assert [issue.code for issue in prototypes.issues] == ['INVALID_PROTO_NAME', 'INVALID_PROTO_NAME', 'INVALID_PROTO_NAME',
                                                          'DUPLICATE_PROTO', 'DUPLICATE_PROTO']
    assert set(prototypes.prototypes) == {('Item', 'Duplicate'), ('Map', 'Collide')}


def test_section_typing():
    # RejectsInvalidProtoSectionNames; BakesDynamicFixedTypeProtos
    files = [('Protos/Mixed.fopro', '[Blueprint]\n$Name = Door\n[BrokenSection]\n$Name = Broken\n[Proto]\n$Name = Bare\n')]
    strict = ff.collect_prototypes(files, fixed_types={'Blueprint'})
    assert set(strict.prototypes) == {('Blueprint', 'Door')}
    assert [issue.code for issue in strict.issues] == ['INVALID_SECTION', 'INVALID_SECTION']

    lenient = ff.collect_prototypes(files)
    assert set(lenient.prototypes) == {('Blueprint', 'Door'), ('BrokenSection', 'Broken'), ('Proto', 'Bare')}


def test_migration_resolves_identity_before_duplicate_detection():
    rules = ff.MigrationRules.from_tags(ff.iter_metadata_tags('///@ MigrationRule Proto Item Rename Old New\n'))
    prototypes = ff.collect_prototypes([('a.fopro', '[ProtoItem]\n$Name = Old\n'), ('b.fopro', '[ProtoItem]\n$Name = New\n')], migrations=rules)
    assert [issue.code for issue in prototypes.issues] == ['DUPLICATE_PROTO']
    assert prototypes.prototypes[('Item', 'New')].authored_name == 'Old'


@pytest.mark.parametrize(('value', 'expected'), [
    ('Line\\nBreak', 'Line\nBreak'),
    ('a\\rb', 'ab'),
    ('say \\"hi\\"', 'say "hi"'),
    ('back\\\\slash', 'back\\slash'),
    ('tab\\tkept', 'tab\\tkept'),
    ('"protected text"', 'protected text'),
    ('', ''),
])
def test_decode_string(value, expected):
    # AnyData.cpp StringEscaping::DecodeString; Test_ProtoTextBaker decodes "Line\nBreak"
    assert ff.decode_string(value) == expected


@pytest.mark.parametrize(('value', 'message'), [
    ('"open', 'Unterminated'),
    ('"a"b"', 'Unexpected closing quote'),
    ('trailing\\', 'Invalid escape'),
])
def test_decode_string_rejections(value, message):
    with pytest.raises(ff.FormatError, match=message):
        ff.decode_string(value)


def test_prototype_text_key_shape():
    resolved, _ = resolve_text('[ProtoCritter]\n$Name = Npc\n$Text = Default\n$Text engl Desc Short = Brief\n$Text engl A B C = x\n')
    languages = ff.parse_bake_languages(['engl'])
    with pytest.raises(ff.FormatError, match='too many tokens'):
        ff.prototype_text_entries(resolved[('Critter', 'Npc')], languages)

    resolved, _ = resolve_text('[ProtoGizmo]\n$Name = G\n$Text = Default\n$Text engl Desc Short = Brief\n')
    entries, _ = ff.prototype_text_entries(resolved[('Gizmo', 'G')], languages)
    assert [(entry.collection, entry.key1, entry.key2, entry.key3, entry.text) for entry in entries['engl']] == [
        ('Protos', 'G', '', '', 'Default'), ('Protos', 'G', 'Desc', 'Short', 'Brief')]


# ---------- text packs: Source/Common/TextPack.cpp, Source/Tests/Test_TextPack.cpp ----------

def entries_of(pack: ff.TextPack) -> list[tuple[str, str, str]]:
    return [(entry.key1, entry.key2, entry.text) for entry in pack.entries]


def test_text_pack_keys_and_multiline_values():
    # LoadFromStringParsesKeysAndMultilineValues, LoadFromStringSupportsNamedKeysAndOffsets
    pack = ff.parse_text_pack('{100}{}{Hello}\n{200}{3}{World}\n{300}{}{Line1\nLine2}', 'Dialogs')
    assert entries_of(pack) == [('100', '', 'Hello'), ('200', '3', 'World'), ('300', '', 'Line1\nLine2')]
    assert pack.issues == []
    assert entries_of(ff.parse_text_pack('{QuestEntry}{}{Base}\n{QuestEntry}{Suffix}{Combined}', 'Quest')) == [
        ('QuestEntry', '', 'Base'), ('QuestEntry', 'Suffix', 'Combined')]


def test_malformed_text_lines_fail_but_keep_valid_entries():
    # MalformedLoadFromStringReportsFailureAfterKeepingValidEntries
    pack = ff.parse_text_pack('{10}{}{Valid}\n{20}{Broken\n{30}{}{StillValid}', 'Dialogs')
    assert entries_of(pack) == [('10', '', 'Valid'), ('30', '', 'StillValid')]
    assert [issue.line for issue in pack.issues] == [2]


def test_text_pack_corner_cases():
    # No comment grammar, no escape for '}', an unclosed first field is skipped, an empty first key fails,
    # an unclosed text swallows the rest of the file, and multiline text keeps carriage returns
    assert entries_of(ff.parse_text_pack('# {1}{}{in a comment}\n', 'D')) == [('1', '', 'in a comment')]
    assert entries_of(ff.parse_text_pack('{1}{}{a}b}\n', 'D')) == [('1', '', 'a')]

    skipped = ff.parse_text_pack('{1\n{2}{}{x}\n', 'D')
    assert entries_of(skipped) == [('2', '', 'x')] and skipped.issues == []

    empty_key = ff.parse_text_pack('{}{}{x}\n', 'D')
    assert empty_key.entries == [] and len(empty_key.issues) == 1

    swallowed = ff.parse_text_pack('{1}{}{open\n{2}{}{x}\n', 'D')
    assert entries_of(swallowed) == [('1', '', 'open\n{2')]

    unclosed = ff.parse_text_pack('{1}{}{open\nmore\n', 'D')
    assert unclosed.entries == [] and len(unclosed.issues) == 1

    assert entries_of(ff.parse_text_pack('{1}{}{a\r\nb}\r\n', 'D')) == [('1', '', 'a\r\nb')]


def test_text_pack_file_names():
    # TextBaker: <TextPack>.<Language>.fotxt
    assert ff.text_pack_name('Texts/Dialogs.engl.fotxt') == ('Dialogs', 'engl')
    for name in ('Dialogs.fotxt', 'A.b.c.fotxt'):
        with pytest.raises(ff.FormatError, match='pack name and language'):
            ff.text_pack_name(name)


def test_golden_text_packs():
    config = ff.load_project_config(PROJECT / 'Golden.fomain')
    languages = ff.parse_bake_languages(config.setting_list('Baking.BakeLanguages'))
    packs: dict[str, dict[str, list[ff.TextEntry]]] = {}

    for pack_file in ff.project_text_packs(config):
        pack = ff.read_text_pack(pack_file.file)
        assert pack.issues == []
        packs.setdefault(pack.language, {})[pack.collection] = pack.entries

    normalized = ff.normalize_language_packs(languages, packs)
    for language in ('engl', 'russ'):
        actual: dict[str, list[str]] = {}
        for entry in normalized[language]['Dialogs']:
            actual.setdefault(f'{entry.collection} {entry.key1} {entry.key2}', []).append(entry.text)
        assert actual == EXPECTED['text_packs'][language]


def test_bake_languages():
    # ParseBakeLanguagesNormalizesInlineFallbacks, ParseBakeLanguagesRejectsInvalidDeclarations
    languages = ff.parse_bake_languages(['russ', 'engl', 'ru18:russ', 'en18:engl'])
    assert languages.languages == ('russ', 'engl', 'ru18', 'en18')
    assert dict(languages.fallbacks) == {'ru18': 'russ', 'en18': 'engl'}

    for declarations in ([], ['russ', 'en18:'], ['russ', ':russ'], ['russ', 'en18:engl:extra'], ['russ', 'en18:engl'],
                         ['russ', 'engl:en18', 'en18'], ['russ', 'engl', 'en18:engl', 'en18:russ']):
        with pytest.raises(ff.FormatError):
            ff.parse_bake_languages(declarations)


def text(collection: str, key1: str, value: str) -> ff.TextEntry:
    return ff.TextEntry(collection, key1, text=value)


def texts_of(entries: list[ff.TextEntry]) -> dict[str, str]:
    return {entry.key1: entry.text for entry in entries}


def test_normalize_against_default_language():
    # FixPacksAddsMissingLanguagesAndNormalizesAgainstBase
    languages = ff.parse_bake_languages(['engl', 'russ', 'germ'])
    normalized = ff.normalize_language_packs(languages, {
        'engl': {'Dialogs': [text('Dialogs', '1', 'Hello'), text('Dialogs', '2', 'World')], 'Items': [text('Items', '10', 'Item')]},
        'russ': {'Dialogs': [text('Dialogs', '2', 'Mir'), text('Dialogs', '3', 'Extra')]},
        'span': {'Dialogs': [text('Dialogs', '1', 'Hola')]},
    })
    assert list(normalized) == ['engl', 'russ', 'germ']
    assert texts_of(normalized['russ']['Dialogs']) == {'1': 'Hello', '2': 'Mir'}
    assert texts_of(normalized['russ']['Items']) == {'10': 'Item'}
    assert texts_of(normalized['germ']['Dialogs']) == {'1': 'Hello', '2': 'World'}


def test_normalize_with_declared_fallbacks():
    # FixPacksInheritsMissingTextFromConfiguredLanguageParent, FixPacksBootstrapsDefaultLanguageWhenInputIsEmpty
    languages = ff.parse_bake_languages(['russ', 'engl', 'ru18:russ', 'en18:engl'])
    normalized = ff.normalize_language_packs(languages, {
        'russ': {'Dialogs': [text('Dialogs', '1', 'Базовая реплика'), text('Dialogs', '2', 'Базовый ответ')]},
        'engl': {'Dialogs': [text('Dialogs', '1', 'Base line'), text('Dialogs', '2', 'Base answer')]},
        'en18': {'Dialogs': [text('Dialogs', '2', 'Adult answer')]},
    })
    assert texts_of(normalized['en18']['Dialogs']) == {'1': 'Base line', '2': 'Adult answer'}
    assert texts_of(normalized['ru18']['Dialogs']) == {'1': 'Базовая реплика', '2': 'Базовый ответ'}

    empty = ff.normalize_language_packs(ff.parse_bake_languages(['engl', 'russ']), {})
    assert empty == {'engl': {}, 'russ': {}}


# ---------- metadata tags and migration rules: Source/Tools/MetadataBaker.cpp ----------

def test_metadata_tag_scanner():
    source = ('    ///@ Property Critter Server int32 QuestStep Mutable Persistent // trailing comment\n'
              'int x; ///@ Enum BodyType Men\n'
              '///@ MigrationRule Property Item Rename \\\n    OldName NewName\n'
              '///@ MigrationRule Proto Modifier Rename "Local knowledge" LocalKnowledge')
    tags = ff.iter_metadata_tags(source, 'Scripts/A.fos')
    assert [(tag.name, tag.tokens, tag.line) for tag in tags] == [
        ('Property', ('Critter', 'Server', 'int32', 'QuestStep', 'Mutable', 'Persistent'), 1),
        ('Enum', ('BodyType', 'Men'), 2),
        ('MigrationRule', ('Property', 'Item', 'Rename', 'OldName', 'NewName'), 3),
        ('MigrationRule', ('Proto', 'Modifier', 'Rename', 'Local knowledge', 'LocalKnowledge'), 5),
    ]

    with pytest.raises(ff.FormatError, match='empty tag'):
        ff.iter_metadata_tags('///@ // nothing\n')


@pytest.mark.parametrize(('line', 'expected'), [
    ('Property Item Rename Grid_ToMap Grid.ToMap', ('Property', 'Item', 'Rename', 'Grid_ToMap', 'Grid.ToMap')),
    ('Property Critter Remove ActionAnimAlive', ('Property', 'Critter', 'Remove', 'ActionAnimAlive', '')),
    ('Property Item Remove Component.Deleted', ('Property', 'Item', 'Remove', 'Component.Deleted', '')),
    ('Property Critter Transform QuestStage MigrationQuests::RewriteQuestStage',
     ('Property', 'Critter', 'Transform', 'QuestStage', 'MigrationQuests::RewriteQuestStage')),
    ('Property Critter Transform QuestStage GameMigrations.RewriteQuestStage',
     ('Property', 'Critter', 'Transform', 'QuestStage', 'GameMigrations.RewriteQuestStage')),
    ('Proto Item Transform OldVariant MyMigrations.Migrate', ('Proto', 'Item', 'Transform', 'OldVariant', 'MyMigrations.Migrate')),
    ('Enum QuestState Aborted Failed', ('Enum', 'QuestState', '', 'Aborted', 'Failed')),
    ('Version 0 0 87', ('Version', '0', '', '0', '87')),
])
def test_migration_rule_forms(line, expected):
    # Migration.fos and Migration.cs shapes; ParseMigrationRule merges dotted and qualified names
    rule = ff.parse_migration_rule(ff.iter_metadata_tags(f'///@ MigrationRule {line}\n')[0])
    assert (rule.kind, rule.owner, rule.action, rule.source, rule.replacement) == expected


@pytest.mark.parametrize(('line', 'message'), [
    ('Proto Item Rename Old', 'insufficient parameters'),
    ('Proto Item Remove Old New', 'too many rule arguments'),
    ('Proto Modifier Rename Local knowledge LocalKnowledge', 'too many rule arguments'),
    ('Proto Modifier Rename "Local knowledge" "New Name"', 'whitespace'),
    ('Proto Item Rename "Unclosed Old', 'malformed quoted'),
    ('Property Item Rename Old Ns::Func', 'too many rule arguments'),
    ('Property Critter Transform Value ::Func', 'insufficient parameters'),
    ('Proto Item Move Old New', 'unknown action'),
])
def test_migration_rule_rejections(line, message):
    with pytest.raises(ff.FormatError, match=message):
        ff.parse_migration_rule(ff.iter_metadata_tags(f'///@ MigrationRule {line}\n')[0])


def test_migration_rule_lookup_and_registration():
    # EngineMetadata::CheckMigrationRule follows chains; RegisterMigrationRule rejects repeats and cycles
    rules = ff.MigrationRules.from_tags(ff.iter_metadata_tags(
        '///@ MigrationRule Proto Item Rename A B\n///@ MigrationRule Proto Item Rename B C\n'
        '///@ MigrationRule Proto Item Remove Gone\n///@ MigrationRule Proto Critter Rename A Z\n'))
    assert rules.resolve('Proto', 'Item', 'A') == 'C'
    assert rules.resolve('Proto', 'Item', 'Gone') == ''
    assert rules.resolve('Proto', 'Item', 'C') is None
    assert rules.renamed('Proto', 'Critter', 'A') == 'Z'

    for text, message in (('Proto Item Rename A B\n///@ MigrationRule Proto Item Remove A', 'already added'),
                          ('Proto Item Rename A B\n///@ MigrationRule Proto Item Rename B A', 'cycle')):
        with pytest.raises(ff.FormatError, match=message):
            ff.MigrationRules.from_tags(ff.iter_metadata_tags(f'///@ MigrationRule {text}\n'))


# ---------- agreement with the Engine's generated format models and settings ----------

def test_defaults_match_engine_settings():
    settings_inc = (ENGINE_ROOT / 'Source' / 'Common' / 'Settings.inc').read_text(encoding='utf-8')
    for name, default in ff.ENGINE_SETTING_DEFAULTS.items():
        group, setting = name.split('.')
        line = next(line for line in settings_inc.splitlines() if line.startswith('SETTING(') and f', {group}, {setting}' in line)
        declared = line.split(f'{setting}', 1)[1].split(')', 1)[0].strip(', ').replace('"', '').replace(',', ' ')
        assert ff.split_list(declared.lower()) == ff.split_list(default.lower()), name


def test_defaults_match_generated_models():
    prototype_model = json.loads((ENGINE_ROOT / 'Docs' / 'generated' / 'prototype-format.json').read_text(encoding='utf-8'))
    text_model = json.loads((ENGINE_ROOT / 'Docs' / 'generated' / 'text-format.json').read_text(encoding='utf-8'))

    assert ff.split_list(ff.ENGINE_SETTING_DEFAULTS['Baking.ProtoFileExtensions']) == prototype_model['file_selection']['engine_defaults']
    assert prototype_model['file_selection']['nested_sections_skipped'] is True
    assert [*ff.PROTO_TEXT_PACKS.values(), ff.PROTO_TEXT_FALLBACK_PACK] == text_model['outputs']['prototype_packs']
    assert ff.split_list(ff.ENGINE_SETTING_DEFAULTS['Baking.BakeLanguages']) == text_model['setting_defaults']['Baking.BakeLanguages']
    assert {entry['name'] for entry in prototype_model['entity_types']} == set(ff.PROTO_TEXT_PACKS)


# ---------- command line ----------

def test_cli(capsys):
    assert ff.main(['protos', str(PROJECT / 'Golden.fomain'), '--name', 'Dagger']) == 0
    output = json.loads(capsys.readouterr().out)
    assert output['prototypes']['Item Dagger']['properties'] == {'Damage': '5'}

    assert ff.main(['config', str(PROJECT / 'Golden.fomain'), '--sub-config', 'Smoke', '--files']) == 0
    output = json.loads(capsys.readouterr().out)
    assert output['settings']['Custom.List'] == 'one two three'
    assert 'Items/Inherit.fopro' in next(pack for pack in output['resource_packs'] if pack['name'] == 'Protos')['files']

    assert ff.main(['text', str(PROJECT / 'Texts' / 'Dialogs.russ.fotxt')]) == 0
    assert json.loads(capsys.readouterr().out)['entries'][0]['text'] == 'Привет'

    assert ff.main(['config', str(PROJECT / 'Missing.fomain')]) == 2
    assert 'fo_formats:' in capsys.readouterr().err
