from __future__ import annotations

import subprocess
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
FIXTURES = Path(__file__).parent / 'fixtures/linux_elf'


def test_inspect_real_readelf_version_requirement(tmp_path: Path, monkeypatch) -> None:
    import linux_elf

    binary = tmp_path / 'libparent.so.1'
    # ELF64 little-endian ET_DYN/EM_X86_64, from the captured Linux fixture.
    header = bytearray(64)
    header[:7] = b'\x7fELF\x02\x01\x01'
    header[16:20] = b'\x03\x00\x3e\x00'
    binary.write_bytes(header)
    captured = (FIXTURES / 'parent.readelf.txt').read_text(encoding='utf-8')
    calls = []

    def run(command, **kwargs):
        calls.append((command, kwargs))
        return SimpleNamespace(returncode=0, stdout=captured, stderr='')

    monkeypatch.setattr(subprocess, 'run', run)
    info = linux_elf.inspect_elf(binary)
    assert info.bits == 64
    assert info.machine == 62
    assert info.needed == ('libleaf.so.1',)
    assert info.soname == 'libparent.so.1'
    assert info.required_versions == {'libleaf.so.1': frozenset({'LEAF_1.0'})}
    assert not info.provided_versions
    assert info.interpreter is None
    assert len(calls) == 1
    assert calls[0][1]['env']['LC_ALL'] == 'C'


import os
import shutil
import struct

import linux_elf


def elf_header(*, elf_class: int = 2, elf_data: int = 1, elf_type: int = 3, machine: int = 62) -> bytes:
    """Minimal controlled ELF header stand-in for the captured Linux files."""
    header = bytearray(64)
    header[0:4] = b'\x7fELF'
    header[4] = elf_class
    header[5] = elf_data
    header[6] = 1
    endian = '>' if elf_data == 2 else '<'
    struct.pack_into(endian + 'HH', header, 16, elf_type, machine)
    return bytes(header)


def write_elf(path: Path, *, trailer: bytes = b'', **header_kwargs) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(elf_header(**header_kwargs) + trailer)
    return path


def captured(name: str) -> str:
    return (FIXTURES / f'{name}.readelf.txt').read_text(encoding='utf-8')


def leaf_output(version: str = 'LEAF_1.0') -> str:
    return captured('leaf').replace('LEAF_1.0', version)


def with_interpreter(output: str, interpreter: str = '/lib64/ld-linux-x86-64.so.2') -> str:
    return output.replace(
        'Program Headers:\n',
        '  INTERP0x000000 0x0000000000000000 0x0000000000000000 0x000000 0x000000 R   0x1\n'
        f'      [Requesting program interpreter: {interpreter}]\n',
        1,
    )


def stub_readelf(monkeypatch, outputs: dict[Path, str]) -> list:
    calls: list = []
    resolved_outputs = {path.resolve(): text for path, text in outputs.items()}

    def run(command, **kwargs):
        calls.append((command, kwargs))
        key = Path(command[-1]).resolve()
        if key not in resolved_outputs:
            raise AssertionError(f'unexpected readelf invocation for {key}')
        return SimpleNamespace(returncode=0, stdout=resolved_outputs[key], stderr='')

    monkeypatch.setattr(subprocess, 'run', run)
    return calls


@pytest.mark.parametrize('location', ['bin/main', 'lib/dlopen-only.so'])
@pytest.mark.parametrize('rpath', ['/outside/build/lib', 'relative', '$ORIGIN/../../../outside', ''])
def test_closure_rejects_external_or_cwd_rpath(tmp_path, monkeypatch, location, rpath):
    binary = write_elf(tmp_path / location)
    stub_readelf(monkeypatch, {binary: f' 0x000000000000000f (RPATH) Library rpath: [{rpath}]\n'})
    with pytest.raises(linux_elf.ElfError, match='RPATH'):
        linux_elf.validate_elf_closure(tmp_path)


def test_closure_accepts_private_origin_rpath_and_overridden_runpath(tmp_path, monkeypatch):
    first = write_elf(tmp_path / 'bin/main')
    second = write_elf(tmp_path / 'lib/plugin.so', trailer=b'different')
    stub_readelf(monkeypatch, {
        first: ' (RPATH) Library rpath: [$ORIGIN/../lib:${ORIGIN}]\n',
        second: ' (RPATH) Library rpath: [/old/build]\n (RUNPATH) Library runpath: [/old/build]\n',
    })
    assert len(linux_elf.validate_elf_closure(tmp_path)) == 2


def test_closure_rejects_ambiguous_dlopen_only_providers(tmp_path, monkeypatch):
    first = write_elf(tmp_path / 'a/plugin.so', trailer=b'one')
    second = write_elf(tmp_path / 'b/plugin.so', trailer=b'two')
    stub_readelf(monkeypatch, {first: '', second: ''})
    with pytest.raises(linux_elf.ElfError, match='ambiguous'):
        linux_elf.validate_elf_closure(tmp_path)


@pytest.mark.skipif(sys.platform != 'linux', reason='native ELF compiler fixture')
def test_actual_dlopen_elf_with_external_rpath_is_rejected(tmp_path):
    compiler = shutil.which('cc')
    if not compiler:
        pytest.skip('C compiler is required')
    source = tmp_path / 'plugin.c'
    source.write_text('int plugin(void) { return 42; }\n')
    subprocess.run([compiler, '-shared', '-fPIC', '-nostdlib', str(source),
                    '-Wl,--disable-new-dtags,-rpath,/outside/build', '-o', str(tmp_path / 'plugin.so')],
                   capture_output=True, check=True, timeout=30)
    with pytest.raises(linux_elf.ElfError, match='RPATH'):
        linux_elf.validate_elf_closure(tmp_path)


# ELF1: immutable metadata from the captured fixtures


def test_inspect_leaf_defines_provided_version(tmp_path: Path, monkeypatch) -> None:
    binary = write_elf(tmp_path / 'libleaf.so.1')
    stub_readelf(monkeypatch, {binary: captured('leaf')})

    info = linux_elf.inspect_elf(binary)

    assert info.path == binary
    assert info.bits == 64
    assert info.machine == 62
    assert info.needed == ()
    assert info.soname == 'libleaf.so.1'
    # The BASE entry names the SONAME itself and is not a provided version
    assert info.provided_versions == {'LEAF_1.0'}
    assert info.required_versions == {}
    assert info.interpreter is None


def test_inspect_associates_versions_with_the_declaring_file(tmp_path: Path, monkeypatch) -> None:
    binary = write_elf(tmp_path / 'libmulti.so.1')
    output = '\n'.join([
        'Version needs section \'.gnu.version_r\' contains 2 entries:',
        '  000000: Version: 1  File: liba.so.1  Cnt: 1',
        '  0x0010:   Name: A_1.0  Flags: none  Version: 2',
        '  000001: Version: 1  File: libb.so.2  Cnt: 1',
        '  0x0030:   Name: B_2.0  Flags: none  Version: 3',
        'Version symbols section \'.gnu.version\' contains 3 entries:',
        '  000:   0 (*local*)       2 (A_1.0)       3 (B_2.0)',
        ' 0x0000000000000001 (NEEDED)             Shared library: [liba.so.1]',
        ' 0x0000000000000001 (NEEDED)             Shared library: [libb.so.2]',
    ])
    stub_readelf(monkeypatch, {binary: output})

    info = linux_elf.inspect_elf(binary)

    assert info.needed == ('liba.so.1', 'libb.so.2')
    assert info.required_versions == {
        'liba.so.1': frozenset({'A_1.0'}),
        'libb.so.2': frozenset({'B_2.0'}),
    }
    # Version symbols mention A_1.0/B_2.0 but define nothing
    assert not info.provided_versions


# ELF2: header fields read from bytes, invalid metadata rejected before readelf


@pytest.mark.parametrize(
    'build, fragment',
    [
        (lambda: b'\x00' * 64, 'ELF magic'),
        (lambda: elf_header()[:10], 'truncated'),
        (lambda: elf_header(elf_class=0), 'class'),
        (lambda: elf_header(elf_data=0), 'encoding'),
        (lambda: elf_header(elf_type=1), 'type'),
        (lambda: elf_header(elf_type=0), 'type'),
    ],
)
def test_inspect_rejects_invalid_header(tmp_path: Path, monkeypatch, build, fragment) -> None:
    binary = tmp_path / 'probe.bin'
    binary.write_bytes(build())
    calls = stub_readelf(monkeypatch, {})

    with pytest.raises(ValueError, match=fragment):
        linux_elf.inspect_elf(binary)

    assert calls == []


@pytest.mark.parametrize(
    'header_kwargs, bits, machine',
    [
        ({}, 64, 62),
        ({'elf_class': 1}, 32, 62),
        ({'elf_class': 1, 'elf_type': 2}, 32, 62),
        ({'elf_type': 2}, 64, 62),
        ({'machine': 183}, 64, 183),
        ({'elf_data': 2}, 64, 62),
    ],
)
def test_inspect_reads_header_fields(tmp_path: Path, monkeypatch, header_kwargs, bits, machine) -> None:
    binary = write_elf(tmp_path / 'probe.bin', **header_kwargs)
    stub_readelf(monkeypatch, {binary: captured('parent')})

    info = linux_elf.inspect_elf(binary)

    assert info.bits == bits
    assert info.machine == machine


# ELF3: closed bundle over the captured parent/leaf pair


def test_closure_passes_for_captured_parent_leaf(tmp_path: Path, monkeypatch) -> None:
    parent = write_elf(tmp_path / 'libparent.so.1')
    leaf = write_elf(tmp_path / 'libleaf.so.1')
    stub_readelf(monkeypatch, {parent: captured('parent'), leaf: captured('leaf')})

    infos = linux_elf.validate_elf_closure(tmp_path)

    assert [info.path for info in infos] == sorted([parent.resolve(), leaf.resolve()])
    parent_info = next(info for info in infos if info.path == parent.resolve())
    assert parent_info.required_versions == {'libleaf.so.1': frozenset({'LEAF_1.0'})}


def test_closure_scans_elf_regardless_of_filename(tmp_path: Path, monkeypatch) -> None:
    parent = write_elf(tmp_path / 'payload.dat')
    leaf = write_elf(tmp_path / 'libleaf.so.1')
    stub_readelf(monkeypatch, {parent: captured('parent'), leaf: captured('leaf')})

    infos = linux_elf.validate_elf_closure(tmp_path)

    assert {info.path for info in infos} == {parent.resolve(), leaf.resolve()}


def test_closure_fails_when_needed_provider_is_absent(tmp_path: Path, monkeypatch) -> None:
    parent = write_elf(tmp_path / 'libparent.so.1')
    stub_readelf(monkeypatch, {parent: captured('parent')})

    with pytest.raises(ValueError) as error:
        linux_elf.validate_elf_closure(tmp_path)

    assert 'libleaf.so.1' in str(error.value)
    assert str(parent.resolve()) in str(error.value)


def test_closure_requires_an_actual_needed_filename(tmp_path: Path, monkeypatch) -> None:
    parent = write_elf(tmp_path / 'libparent.so.1')
    leaf = write_elf(tmp_path / 'libleaf.so.1.0')
    stub_readelf(monkeypatch, {parent: captured('parent'), leaf: captured('leaf')})

    with pytest.raises(ValueError, match='libleaf.so.1'):
        linux_elf.validate_elf_closure(tmp_path)


def test_closure_rejects_provider_without_the_required_version(tmp_path: Path, monkeypatch) -> None:
    parent = write_elf(tmp_path / 'libparent.so.1')
    wrong_leaf = write_elf(tmp_path / 'libleaf.so.1')
    stub_readelf(monkeypatch, {parent: captured('parent'), wrong_leaf: leaf_output('LEAF_2.0')})

    with pytest.raises(ValueError) as error:
        linux_elf.validate_elf_closure(tmp_path)

    message = str(error.value)
    assert 'LEAF_1.0' in message
    assert 'LEAF_2.0' in message
    assert str(wrong_leaf.resolve()) in message


def test_closure_requires_bundled_program_interpreter(tmp_path: Path, monkeypatch) -> None:
    runner = write_elf(tmp_path / 'runner', elf_type=2)
    stub_readelf(monkeypatch, {runner: with_interpreter(leaf_output())})

    with pytest.raises(ValueError) as error:
        linux_elf.validate_elf_closure(tmp_path)

    assert 'interpreter' in str(error.value)
    assert '/lib64/ld-linux-x86-64.so.2' in str(error.value)


def test_closure_accepts_bundled_program_interpreter(tmp_path: Path, monkeypatch) -> None:
    runner = write_elf(tmp_path / 'runner', elf_type=2)
    loader = write_elf(tmp_path / 'lib64/ld-linux-x86-64.so.2')
    stub_readelf(monkeypatch, {runner: with_interpreter(leaf_output()), loader: leaf_output()})

    infos = linux_elf.validate_elf_closure(tmp_path)

    assert [info.path for info in infos] == sorted([runner.resolve(), loader.resolve()])


# ELF4: architecture, symlinks, and ambiguous providers


def test_closure_rejects_architecture_mismatch(tmp_path: Path, monkeypatch) -> None:
    parent = write_elf(tmp_path / 'libparent.so.1')
    foreign = write_elf(tmp_path / 'libother.so', machine=183, trailer=b'other')
    stub_readelf(monkeypatch, {parent: captured('parent'), foreign: leaf_output()})

    with pytest.raises(ValueError) as error:
        linux_elf.validate_elf_closure(tmp_path)

    assert 'machine 183' in str(error.value)


def test_closure_rejects_symlink_escaping_the_root(tmp_path: Path) -> None:
    root = tmp_path / 'bundle'
    root.mkdir()
    outside = write_elf(tmp_path / 'outside.so')
    try:
        (root / 'liboutside.so').symlink_to(outside)
    except OSError:
        pytest.skip('symlink creation is unavailable')

    with pytest.raises(ValueError) as error:
        linux_elf.validate_elf_closure(root)

    assert 'escapes' in str(error.value)


def test_closure_rejects_broken_symlink(tmp_path: Path) -> None:
    root = tmp_path / 'bundle'
    root.mkdir()
    try:
        (root / 'libbroken.so').symlink_to(root / 'missing.so')
    except OSError:
        pytest.skip('symlink creation is unavailable')

    with pytest.raises(ValueError) as error:
        linux_elf.validate_elf_closure(root)

    assert 'broken' in str(error.value)


def test_closure_rejects_ambiguous_providers_with_different_content(tmp_path: Path, monkeypatch) -> None:
    parent = write_elf(tmp_path / 'libparent.so.1')
    # 'a' sorts first and holds the valid LEAF_1.0 provider, yet differing bytes must still fail
    first = write_elf(tmp_path / 'a' / 'libleaf.so.1', trailer=b'first')
    second = write_elf(tmp_path / 'b' / 'libleaf.so.1', trailer=b'second')
    stub_readelf(monkeypatch, {
        parent: captured('parent'),
        first: leaf_output(),
        second: leaf_output('LEAF_2.0'),
    })

    with pytest.raises(ValueError) as error:
        linux_elf.validate_elf_closure(tmp_path)

    message = str(error.value)
    assert 'libleaf.so.1' in message
    assert 'ambiguous' in message
    assert str(first.resolve()) in message
    assert str(second.resolve()) in message


def test_closure_accepts_identical_content_copies(tmp_path: Path, monkeypatch) -> None:
    parent = write_elf(tmp_path / 'libparent.so.1')
    first = write_elf(tmp_path / 'a' / 'libleaf.so.1', trailer=b'same')
    second = write_elf(tmp_path / 'b' / 'libleaf.so.1', trailer=b'same')
    stub_readelf(monkeypatch, {parent: captured('parent'), first: leaf_output(), second: leaf_output()})

    infos = linux_elf.validate_elf_closure(tmp_path)

    assert len(infos) == 3


def test_closure_accepts_symlink_alias_names(tmp_path: Path, monkeypatch) -> None:
    parent = write_elf(tmp_path / 'libparent.so.1')
    real_leaf = write_elf(tmp_path / 'leaf-real.so')
    stub_readelf(monkeypatch, {parent: captured('parent'), real_leaf: captured('leaf')})

    try:
        (tmp_path / 'libleaf.so.1').symlink_to(real_leaf)
    except OSError:
        pytest.skip('symlink creation is unavailable')

    infos = linux_elf.validate_elf_closure(tmp_path)

    # The alias resolves to the real file, so it is inspected once but stays a valid provider name
    assert len(infos) == 2


# ELF5: subprocess contract


def test_readelf_invocation_contract(tmp_path: Path, monkeypatch) -> None:
    binary = write_elf(tmp_path / 'libparent.so.1')
    calls = stub_readelf(monkeypatch, {binary: captured('parent')})

    linux_elf.inspect_elf(binary)

    assert len(calls) == 1
    command, kwargs = calls[0]
    assert command == ['readelf', '--wide', '--program-headers', '--dynamic', '--version-info', str(binary)]
    assert kwargs['capture_output'] is True
    assert kwargs['text'] is True
    assert kwargs['encoding'] == 'utf-8'
    assert kwargs['errors'] == 'replace'
    assert kwargs['timeout'] == 30
    assert kwargs['env']['LC_ALL'] == 'C'
    assert kwargs['env'] is not os.environ
    assert 'shell' not in kwargs


def test_readelf_nonzero_exit_is_rejected(tmp_path: Path, monkeypatch) -> None:
    binary = write_elf(tmp_path / 'libparent.so.1')

    def run(command, **kwargs):
        return SimpleNamespace(returncode=2, stdout='', stderr='readelf: error')

    monkeypatch.setattr(subprocess, 'run', run)

    with pytest.raises(ValueError) as error:
        linux_elf.inspect_elf(binary)

    assert 'exited with code 2' in str(error.value)


def test_readelf_stderr_is_rejected(tmp_path: Path, monkeypatch) -> None:
    binary = write_elf(tmp_path / 'libparent.so.1')

    def run(command, **kwargs):
        return SimpleNamespace(returncode=0, stdout=captured('parent'), stderr='readelf: Warning: junk')

    monkeypatch.setattr(subprocess, 'run', run)

    with pytest.raises(ValueError) as error:
        linux_elf.inspect_elf(binary)

    assert 'diagnostics' in str(error.value)


def test_readelf_timeout_is_rejected(tmp_path: Path, monkeypatch) -> None:
    binary = write_elf(tmp_path / 'libparent.so.1')

    def run(command, **kwargs):
        raise subprocess.TimeoutExpired(command, 30)

    monkeypatch.setattr(subprocess, 'run', run)

    with pytest.raises(ValueError) as error:
        linux_elf.inspect_elf(binary)

    assert 'timed out' in str(error.value)


def test_missing_readelf_tool_fails_with_os_error(tmp_path: Path, monkeypatch) -> None:
    binary = write_elf(tmp_path / 'libparent.so.1')

    def run(command, **kwargs):
        raise FileNotFoundError('readelf')

    monkeypatch.setattr(subprocess, 'run', run)

    with pytest.raises(FileNotFoundError):
        linux_elf.inspect_elf(binary)


# ELF6: determinism, explicit failures, and no side effects


def test_closure_returns_deterministic_sorted_result(tmp_path: Path, monkeypatch) -> None:
    paths = [
        write_elf(tmp_path / 'sub' / 'zulu.so', trailer=b'z'),
        write_elf(tmp_path / 'alpha.so', trailer=b'a'),
        write_elf(tmp_path / 'beta' / 'mike.so', trailer=b'm'),
    ]
    stub_readelf(monkeypatch, {path: leaf_output() for path in paths})

    first = linux_elf.validate_elf_closure(tmp_path)
    second = linux_elf.validate_elf_closure(tmp_path)

    assert [info.path for info in first] == sorted(path.resolve() for path in paths)
    assert [info.path for info in second] == [info.path for info in first]


def test_closure_fails_on_empty_and_non_elf_roots(tmp_path: Path) -> None:
    empty = tmp_path / 'empty'
    empty.mkdir()
    with pytest.raises(ValueError, match='No ELF files'):
        linux_elf.validate_elf_closure(empty)

    text_only = tmp_path / 'text'
    text_only.mkdir()
    (text_only / 'readme.txt').write_text('not an ELF', encoding='utf-8')
    with pytest.raises(ValueError, match='No ELF files'):
        linux_elf.validate_elf_closure(text_only)


def test_closure_fails_on_malformed_elf_file(tmp_path: Path, monkeypatch) -> None:
    broken = tmp_path / 'broken.so'
    broken.write_bytes(elf_header(elf_class=3))
    stub_readelf(monkeypatch, {})

    with pytest.raises(ValueError, match='class'):
        linux_elf.validate_elf_closure(tmp_path)


def test_closure_never_resolves_missing_providers_from_the_host(tmp_path: Path, monkeypatch) -> None:
    # libc.so.6 exists on a Linux host but is not in the bundle, so the closure must still fail
    parent = write_elf(tmp_path / 'libparent.so.1')
    stub_readelf(monkeypatch, {parent: captured('parent').replace('libleaf.so.1', 'libc.so.6')})

    with pytest.raises(ValueError) as error:
        linux_elf.validate_elf_closure(tmp_path)

    assert 'libc.so.6' in str(error.value)
    assert str(parent.resolve()) in str(error.value)


def test_closure_does_not_modify_bundle_or_fixture_files(tmp_path: Path, monkeypatch) -> None:
    parent = write_elf(tmp_path / 'libparent.so.1')
    leaf = write_elf(tmp_path / 'libleaf.so.1')
    stub_readelf(monkeypatch, {parent: captured('parent'), leaf: captured('leaf')})
    watched = [parent, leaf, FIXTURES / 'parent.readelf.txt', FIXTURES / 'leaf.readelf.txt']
    before = {path: path.read_bytes() for path in watched}

    linux_elf.validate_elf_closure(tmp_path)

    assert {path: path.read_bytes() for path in watched} == before


# Linux-only integration over the verified gcc leaf/parent commands


def build_native_leaf(build_dir: Path, version: str = 'LEAF_1.0') -> Path:
    build_dir.mkdir(parents=True, exist_ok=True)
    leaf_c = build_dir / 'leaf.c'
    leaf_map = build_dir / 'leaf.map'
    leaf_c.write_text('int leaf(void) { return 42; }\n', encoding='utf-8')
    leaf_map.write_text(f'{version} {{ global: leaf; local: *; }};\n', encoding='utf-8')
    leaf = build_dir / 'libleaf.so.1'
    subprocess.run(
        ['gcc', '-shared', '-fPIC', str(leaf_c), '-Wl,-soname,libleaf.so.1',
         '-Wl,--version-script=' + str(leaf_map), '-o', str(leaf)],
        check=True,
    )
    # The plain link name the parent build resolves through -lleaf
    (build_dir / 'libleaf.so').write_bytes(leaf.read_bytes())
    return leaf


def build_native_parent(build_dir: Path) -> Path:
    parent_c = build_dir / 'parent.c'
    parent_c.write_text('extern int leaf(void); int parent(void) { return leaf()+1; }\n', encoding='utf-8')
    parent = build_dir / 'libparent.so.1'
    subprocess.run(
        ['gcc', '-shared', '-fPIC', str(parent_c), '-L' + str(build_dir), '-lleaf',
         '-Wl,-soname,libparent.so.1', '-Wl,-rpath,$ORIGIN', '-o', str(parent)],
        check=True,
    )
    return parent


@pytest.mark.skipif(sys.platform != 'linux', reason='requires a Linux gcc toolchain')
def test_gcc_built_parent_leaf_closure(tmp_path: Path) -> None:
    if shutil.which('gcc') is None or shutil.which('readelf') is None:
        pytest.skip('gcc and readelf are required')

    build = tmp_path / 'build'
    bundle = tmp_path / 'bundle'
    bundle.mkdir()
    leaf = build_native_leaf(build)
    parent = build_native_parent(build)
    shutil.copy2(leaf, bundle / leaf.name)
    shutil.copy2(parent, bundle / parent.name)

    infos = linux_elf.validate_elf_closure(bundle)

    assert len(infos) == 2
    parent_info = next(info for info in infos if info.path.name == 'libparent.so.1')
    leaf_info = next(info for info in infos if info.path.name == 'libleaf.so.1')
    assert parent_info.needed == ('libleaf.so.1',)
    assert parent_info.required_versions == {'libleaf.so.1': frozenset({'LEAF_1.0'})}
    assert leaf_info.provided_versions == {'LEAF_1.0'}
    assert leaf_info.soname == 'libleaf.so.1'


@pytest.mark.skipif(sys.platform != 'linux', reason='requires a Linux gcc toolchain')
def test_gcc_built_wrong_leaf_version_rejected(tmp_path: Path) -> None:
    if shutil.which('gcc') is None or shutil.which('readelf') is None:
        pytest.skip('gcc and readelf are required')

    build = tmp_path / 'build'
    bundle = tmp_path / 'bundle'
    bundle.mkdir()
    build_native_leaf(build)
    parent = build_native_parent(build)
    # The wrong provider keeps the required SONAME but defines only LEAF_2.0
    wrong_leaf = build_native_leaf(build / 'wrong', version='LEAF_2.0')
    shutil.copy2(parent, bundle / parent.name)
    shutil.copy2(wrong_leaf, bundle / 'libleaf.so.1')

    with pytest.raises(ValueError) as error:
        linux_elf.validate_elf_closure(bundle)

    message = str(error.value)
    assert 'LEAF_1.0' in message
    assert 'LEAF_2.0' in message

