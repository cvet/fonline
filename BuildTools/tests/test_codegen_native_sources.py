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


def parse_native_tags(comment='Native value'):
    declarations = {
        'Enum': 'ProbeKind Value = 7',
        'Property': 'Game Common int32 ProbeValue',
        'Event': 'Common Game ProbeEvent(int32 value)',
        'Setting': 'Common int32 Probe.Value',
        'RemoteCall': 'Server ProbeCall(int32 value)',
        'RefType': 'Common ProbeRef',
        'Entity': 'Server ProbeEntity',
        'FixedType': 'Common ProbeFixed',
        'EntityHolder': 'Server Game ProbeEntity Probes',
    }
    for index, (name, declaration) in enumerate(declarations.items()):
        codegen.user_tag_metas[name] = [codegen.TagMetaRecord(
            'Common/Probe.cppm', index, declaration, None, [comment], is_user_origin=True)]
    codegen.game_entities = ['Game']
    codegen.parse_user_tags(codegen.create_valid_types())
    assert not codegen.errors


def test_native_tags_keep_source_tables_aligned():
    parse_native_tags()
    for name, tags in codegen.codegen_tags.items():
        sources = codegen.codegen_tag_sources[name]
        assert len(tags) == len(sources), name
        assert all(source is not None and source.abs_path == 'Common/Probe.cppm' for source in sources), name
    enum = codegen.codegen_tags['ExportEnum'][0]
    assert enum.user_origin
    assert enum.value_docs['Value'].source == codegen.SourceLocation('Common/Probe.cppm', 1)
    assert enum.value_docs['Value'].comment == ['Native value']


def test_native_documentation_does_not_change_runtime_compatibility():
    parse_native_tags()
    original = codegen.compatibility_hasher.hexdigest()
    codegen.reset_metadata_parse_state()
    parse_native_tags('Revised documentation')
    assert codegen.compatibility_hasher.hexdigest() == original


def test_reset_clears_native_metadata_between_parse_sessions():
    parse_native_tags()
    codegen.reset_metadata_parse_state()
    assert codegen.user_tag_metas == {}
    codegen.parse_user_tags(codegen.create_valid_types())
    assert not any(codegen.codegen_tags.values())

