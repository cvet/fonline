from __future__ import annotations

import hashlib
import os
from pathlib import Path
import re
import select
import shutil
import signal
import subprocess
import sys
import time

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

pytestmark = pytest.mark.skipif(sys.platform != 'linux', reason='native Linux portable integration')


def test_reaped_primary_does_not_signal_a_reused_process_group(tmp_path):
    source = (Path(__file__).resolve().parents[1] / 'linux_apprun.c').read_text()
    start = source.index('static void forward_signal(')
    end = source.index('static int status_code(', start)
    owner = source[start:end]
    probe = tmp_path / 'signals.c'
    probe.write_text('''#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
static int calls;
static int recorded_kill(pid_t group, int number) { (void)group; (void)number; calls++; return -1; }
static void fail(const char* message) { (void)message; exit(5); }
#define kill recorded_kill
''' + owner + '\nint main(void) { forward_signal(SIGTERM, 0); return calls ? 1 : 0; }\n')
    binary = tmp_path / 'signals'
    subprocess.run(['cc', '-D_DEFAULT_SOURCE', '-Wall', '-Wextra', '-Werror', str(probe), '-o', str(binary)],
                   check=True, capture_output=True, timeout=30)
    assert subprocess.run([str(binary)], timeout=5).returncode == 0


@pytest.fixture(scope='module')
def portable_fixture(tmp_path_factory):
    tools_dir = os.environ.get('FO_PORTABLE_TEST_TOOLS')
    if not tools_dir:
        pytest.skip('set FO_PORTABLE_TEST_TOOLS to the pinned tool cache')
    import linux_portable

    root = tmp_path_factory.mktemp('portable fixture')
    source = root / 'source'
    source.mkdir()
    (source / 'payload.txt').write_text('portable-resource-ok\n')
    # Updater payload data must never be treated as an active native dependency.
    payload = source / 'Resources/PlatformBinaries/Other-arch/payload.so'
    payload.parent.mkdir(parents=True)
    payload.write_bytes(b'\x7fELF\x01\x01' + bytes(64))
    (root / 'leaf.c').write_text('int leaf(void) { return 42; }\n')
    (root / 'leaf.map').write_text('LEAF_1.0 { global: leaf; local: *; };\n')
    (root / 'parent.c').write_text('extern int leaf(void); int parent(void) { return leaf()+1; }\n')
    (root / 'main.c').write_text(r'''
        #define _GNU_SOURCE
        #include <dlfcn.h>
        #include <math.h>
        #include <signal.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include <unistd.h>
        static volatile sig_atomic_t stopping;
        static void stop(int sig) { stopping = sig; }
        static int resource(void) {
            char data[128];
            FILE* f = fopen("payload.txt", "r");
            if (!f || !fgets(data, sizeof(data), f)) return 8;
            fclose(f);
            printf("RESOURCE:%s", data);
            return 0;
        }
        int main(int argc, char** argv) {
            setvbuf(stdout, NULL, _IONBF, 0);
            for (int i = 1; i < argc; i++) {
                if (!strcmp(argv[i], "--stdin")) {
                    char input[64];
                    puts("STDIN-READY");
                    if (!fgets(input, sizeof(input), stdin)) return 11;
                    printf("STDIN:%s", input);
                    return 0;
                }
            }
            for (int i = 1; i < argc; i++) {
                printf("ARG:");
                for (const unsigned char* p = (const unsigned char*)argv[i]; *p; p++) printf("%02x", *p);
                puts("");
            }
            volatile float value = 4;
            printf("MATH:%.0f\n", (double)sqrtf(value));
            void* module = dlopen("libparent.so.1", RTLD_NOW);
            if (!module) { fprintf(stderr, "%s\n", dlerror()); return 4; }
            int (*parent)(void) = (int (*)(void))dlsym(module, "parent");
            if (!parent) return 5;
            printf("ANSWER:%d\n", parent());
            for (int i = 1; i < argc; i++) {
                if (!strcmp(argv[i], "--exit-seven")) return 7;
                if (!strcmp(argv[i], "--daemon")) {
                    pid_t child = fork();
                    if (child < 0) return 9;
                    if (child) return 0;
                    if (setsid() < 0) return 10;
                    signal(SIGTERM, stop);
                    printf("DAEMON:%ld\n", (long)getpid());
                    while (!stopping) pause();
                    return resource();
                }
            }
            return resource();
        }
    ''')
    commands = [
        ['cc', '-shared', '-fPIC', str(root / 'leaf.c'), '-Wl,-soname,libleaf.so.1',
         '-Wl,--version-script=' + str(root / 'leaf.map'), '-o', str(source / 'libleaf.so.1.0')],
        ['cc', '-shared', '-fPIC', str(root / 'parent.c'), '-L' + str(source),
         '-l:libleaf.so.1.0', '-Wl,-soname,libparent.so.1', '-Wl,-rpath,$ORIGIN',
         '-o', str(source / 'libparent.so.1')],
        ['cc', str(root / 'main.c'), '-fno-builtin-sqrtf', '-o', str(source / 'probe'), '-lm', '-ldl'],
    ]
    for command in commands:
        subprocess.run(command, check=True, capture_output=True, timeout=60)
    before = {p.relative_to(source): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in source.rglob('*') if p.is_file()}
    tool_paths = {key: Path(tools_dir) / filename for key, filename in [
        ('deploy', 'go-appimagetool'), ('appimagetool', 'appimagetool'), ('runtime', 'runtime-x86_64')]}
    original = root / 'original pack.AppDir'
    linux_portable.prepare_portable(source, original, 'probe', appimage=False, tools=tool_paths,
                                   extra_libraries=[], identity={'name': 'Portable probe', 'preserve_cwd': '0'})
    output = root / 'moved pack with spaces.AppDir'
    shutil.move(original, output)
    return root, source, output, before, tool_paths


def arguments(output: str) -> list[str]:
    return [bytes.fromhex(line[4:]).decode() for line in output.splitlines() if line.startswith('ARG:')]


@pytest.mark.parametrize('suspend', [False, True])
def test_portable_interactive_terminal_input(portable_fixture, suspend):
    import pty
    root, _, app, _, _ = portable_fixture
    pid, master = pty.fork()
    if pid == 0:
        os.chdir(root)
        os.execl(str(app / 'AppRun'), 'AppRun', '--stdin')
    data = b''
    try:
        deadline = time.monotonic() + 8
        sent = False
        while time.monotonic() < deadline:
            if select.select([master], [], [], 0.1)[0]:
                try:
                    data += os.read(master, 4096)
                except OSError:
                    break
            if b'STDIN-READY' in data and not sent:
                if suspend:
                    os.write(master, b'\x1a')
                    stopped = False
                    limit = time.monotonic() + 3
                    while time.monotonic() < limit:
                        observed, status = os.waitpid(pid, os.WNOHANG | os.WUNTRACED)
                        if observed:
                            assert os.WIFSTOPPED(status), status
                            stopped = True
                            break
                        time.sleep(0.02)
                    assert stopped, 'Supervisor did not report the stopped foreground application'
                    os.kill(pid, signal.SIGCONT)
                os.write(master, b'portable-terminal-input\n')
                sent = True
            if b'STDIN:portable-terminal-input' in data:
                break
        assert b'STDIN:portable-terminal-input' in data, data
    finally:
        children = Path(f'/proc/{pid}/task/{pid}/children')
        for child in children.read_text().split() if children.exists() else []:
            try:
                os.kill(int(child), signal.SIGKILL)
            except ProcessLookupError:
                pass
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        os.waitpid(pid, 0)
        os.close(master)


def test_portable_relocates_and_preserves_source_and_updater_bytes(portable_fixture):
    root, source, moved, before, _ = portable_fixture
    supplied = ['one two', '', '--flag', '0', '"quoted"']
    result = subprocess.run([str(moved / 'AppRun'), *supplied], cwd=root,
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'ANSWER:43' in result.stdout and 'MATH:2' in result.stdout
    assert 'RESOURCE:portable-resource-ok' in result.stdout
    args = arguments(result.stdout)
    assert args[:len(supplied)] == supplied
    writable = args[args.index('--Common.UserWritablePath') + 1]
    assert Path(writable) == moved / 'data'
    assert {p.relative_to(source): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in source.rglob('*') if p.is_file()} == before
    for path in ['probe', 'libparent.so.1', 'libleaf.so.1.0']:
        assert hashlib.sha256((moved / 'usr/bin' / path).read_bytes()).hexdigest() == before[Path(path)]
    payload = Path('Resources/PlatformBinaries/Other-arch/payload.so')
    assert hashlib.sha256((moved / 'usr/share/fonline' / payload).read_bytes()).hexdigest() == before[payload]
    assert (moved / 'portable-manifest.json').is_file()


def test_portable_preserves_explicit_writable_override_and_exit_code(portable_fixture):
    root, _, app, _, _ = portable_fixture
    result = subprocess.run([str(app / 'AppRun'), '--Common.UserWritablePath', '0', '--exit-seven'],
                            cwd=root, capture_output=True, text=True, timeout=15)
    assert result.returncode == 7, result.stdout + result.stderr
    assert arguments(result.stdout) == ['--Common.UserWritablePath', '0', '--exit-seven']


def test_launcher_keeps_daemon_alive_and_forwards_sigterm(portable_fixture):
    root, _, app, _, _ = portable_fixture
    check_daemon(root, app / 'AppRun')


def check_daemon(root: Path, executable: Path, environment=None, *, runtime_signal_exit=False):
    process = subprocess.Popen([str(executable), '--daemon'], cwd=root, env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
    daemon = None
    started = None
    observed = []
    try:
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if not select.select([process.stdout], [], [], 0.5)[0]:
                continue
            observed.append(os.read(process.stdout.fileno(), 4096).decode())
            match = re.search(r'(?:^|\n)DAEMON:(\d+)\n', ''.join(observed))
            if match:
                daemon = int(match[1])
                started = Path(f'/proc/{daemon}/stat').read_text().rsplit(')', 1)[1].split()[19]
                break
        assert daemon is not None, ''.join(observed)
        assert process.poll() is None
        process.send_signal(signal.SIGTERM)
        output, error = process.communicate(timeout=15)
        expected_codes = (0, -signal.SIGTERM, 128 + signal.SIGTERM) if runtime_signal_exit else (0,)
        assert process.returncode in expected_codes, ''.join(observed) + output + error
        assert 'RESOURCE:portable-resource-ok' in output
        assert not Path(f'/proc/{daemon}').exists()
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        if daemon is not None and Path(f'/proc/{daemon}/stat').exists():
            current = Path(f'/proc/{daemon}/stat').read_text().rsplit(')', 1)[1].split()[19]
            if current == started:
                os.kill(daemon, signal.SIGKILL)


def test_appimage_rejects_local_writable_policy(portable_fixture):
    import linux_portable
    root, _, app, _, tools = portable_fixture
    with pytest.raises(ValueError, match='external'):
        linux_portable.make_appimage(app, root / 'invalid.AppImage', tools=tools, architecture='x64')
    assert not (root / 'invalid.AppImage').exists()


@pytest.fixture(scope='module')
def appimage_fixture(portable_fixture):
    import linux_portable
    root, source, _, _, tools = portable_fixture
    app = root / 'image input.AppDir'
    linux_portable.prepare_portable(source, app, 'probe', appimage=True, tools=tools,
                                   extra_libraries=[], identity={'name': 'Portable probe', 'preserve_cwd': '0'})
    image = root / 'portable probe.AppImage'
    linux_portable.make_appimage(app, image, tools=tools, architecture='x64')
    return root, app, image


def test_appimage_extract_and_run_uses_external_data(appimage_fixture):
    root, app, image = appimage_fixture
    temporary = root / 'image-tmp'
    temporary.mkdir(exist_ok=True)
    environment = {**os.environ, 'APPIMAGE_EXTRACT_AND_RUN': '1', 'TMPDIR': str(temporary)}
    result = subprocess.run([str(image), 'one two'], cwd=root, env=environment,
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    args = arguments(result.stdout)
    assert args == ['one two', '--Common.UserWritablePath', '*']
    assert 'RESOURCE:portable-resource-ok' in result.stdout and 'ANSWER:43' in result.stdout
    assert not (app / 'data').exists()
    assert image.with_suffix('.AppImage.sha256').is_file()


def test_image_creation_preserves_the_complete_validated_manifest(appimage_fixture):
    import json
    _, app, _ = appimage_fixture
    manifest = json.loads((app / 'portable-manifest.json').read_text())
    actual = {path.relative_to(app).as_posix() for path in app.rglob('*') if path.is_file()}
    assert actual == set(manifest['files']) | {'portable-manifest.json'}
    for name, metadata in manifest['files'].items():
        path = app / name
        assert hashlib.sha256(path.read_bytes()).hexdigest() == metadata['sha256'], name
        assert path.stat().st_mode & 0o777 == int(metadata['mode'], 8), name


def test_materialized_archive_aliases_preserve_the_logical_manifest(appimage_fixture):
    import json
    import linux_portable
    root, app, _ = appimage_fixture
    materialized = root / 'archive-materialized.AppDir'
    shutil.copytree(app, materialized, symlinks=False)
    assert not (materialized / '.DirIcon').is_symlink()
    linux_portable.validate_manifest_files(materialized, json.loads((materialized / 'portable-manifest.json').read_text()))


def test_portable_readonly_directory_uses_external_data(portable_fixture):
    import json
    root, _, app, _, _ = portable_fixture
    modes = json.loads((app / 'portable-manifest.json').read_text())['files']
    try:
        for path in app.rglob('*'):
            path.chmod(0o555 if path.is_dir() or path.stat().st_mode & 0o111 else 0o444)
        app.chmod(0o555)
        result = subprocess.run([str(app / 'AppRun')], cwd=root, capture_output=True, text=True, timeout=15)
        assert result.returncode == 0, result.stdout + result.stderr
        assert arguments(result.stdout) == ['--Common.UserWritablePath', '*']
    finally:
        app.chmod(0o755)
        for path in app.rglob('*'):
            path.chmod(0o755 if path.is_dir() else int(modes.get(path.relative_to(app).as_posix(), {}).get('mode', '0644'), 8))


@pytest.mark.parametrize('extract', [True, False])
def test_appimage_daemon_lifetime_and_sigterm(appimage_fixture, extract):
    root, _, image = appimage_fixture
    if not extract and not os.access('/dev/fuse', os.R_OK | os.W_OK):
        pytest.skip('FUSE device unavailable; extract-and-run is tested separately')
    temporary = root / ('extract-daemon-tmp' if extract else 'fuse-daemon-tmp')
    temporary.mkdir(exist_ok=True)
    environment = {**os.environ, 'TMPDIR': str(temporary)}
    environment.pop('APPIMAGE_EXTRACT_AND_RUN', None)
    if extract:
        environment['APPIMAGE_EXTRACT_AND_RUN'] = '1'
    check_daemon(root, image, environment, runtime_signal_exit=True)
