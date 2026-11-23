#!/usr/bin/env python3
"""Prepare private Linux runtimes and AppImages after ordinary binary packaging."""
from __future__ import annotations

import gzip
import hashlib
import json
import os
from pathlib import Path
import posixpath
import re
import shlex
import shutil
import ssl
import subprocess
import sys
import tempfile
from typing import Mapping, Sequence
import urllib.request

from linux_elf import ElfError, ElfInfo, inspect_elf, validate_elf_closure


BUILDTOOLS = Path(__file__).resolve().parent
ARCHITECTURES = {(64, 62): 'x64', (64, 183): 'arm64', (32, 40): 'arm', (32, 3): 'x86'}
APPIMAGE_ARCHITECTURES = {'x64': 'x86_64', 'arm64': 'aarch64', 'arm': 'armhf', 'x86': 'i686'}
DEPLOY_ENTRY = 'fonline-deploy-main'
MANIFEST = 'portable-manifest.json'


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def validate_ca_bundle(path: Path) -> None:
    content = path.read_bytes()
    if re.search(rb'-----BEGIN [A-Z ]*PRIVATE KEY-----', content):
        raise ValueError(f'CA bundle must not contain private key material: {path}')
    try:
        ssl.create_default_context(cafile=str(path))
    except ssl.SSLError as error:
        raise ValueError(f'Invalid CA certificate bundle: {path}') from error


def tool_catalog() -> dict:
    return json.loads((BUILDTOOLS / 'PortableTools.json').read_text(encoding='utf-8'))


def resolve_tools(cache: Path, architecture: str, *, appimage: bool) -> dict[str, Path]:
    """Download only explicitly pinned assets, keeping the cache keyed by their content."""
    result = {}
    catalog = tool_catalog()
    for role in (('deploy', 'appimagetool', 'runtime') if appimage else ('deploy',)):
        asset = catalog['tools'][role]['architectures'][architecture]
        destination = cache / asset['sha256'] / role
        if not destination.is_file():
            destination.parent.mkdir(parents=True, exist_ok=True)
            with urllib.request.urlopen(asset['url'], timeout=120) as response:
                content = response.read()
            if hashlib.sha256(content).hexdigest() != asset['sha256']:
                raise ValueError(f'Pinned portable tool digest mismatch: {role} ({asset["url"]})')
            with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as temporary:
                temporary.write(content)
                temporary_path = Path(temporary.name)
            temporary_path.replace(destination)
        verify_tool(destination, role, architecture)
        destination.chmod(0o755)
        result[role] = destination
    return result


def verify_tool(path: Path, role: str, architecture: str) -> None:
    expected = tool_catalog()['tools'][role]['architectures'][architecture]['sha256']
    if sha256(path) != expected:
        raise ValueError(f'Pinned portable tool digest mismatch: {role}: {path}')


def architecture_of(info: ElfInfo) -> str:
    try:
        return ARCHITECTURES[(info.bits, info.machine)]
    except KeyError:
        raise ValueError(f'No portable tool assets for ELF {info.bits}-bit machine {info.machine}: {info.path}') from None


def tool_environment(temporary: Path, architecture: str, version: str) -> dict[str, str]:
    environment = dict(os.environ)
    for key in ['LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT', 'GITHUB_TOKEN', 'GH_TOKEN',
                'GITHUB_REPOSITORY', 'GITHUB_UPLOAD_TOKEN', 'UPDATE_INFORMATION', 'SIGN']:
        environment.pop(key, None)
    environment.update(APPIMAGE_EXTRACT_AND_RUN='1', TMPDIR=str(temporary),
                       ARCH=APPIMAGE_ARCHITECTURES[architecture], VERSION=version)
    return environment


def run_tool(command: Sequence[str], *, cwd: Path, environment: Mapping[str, str],
             log_path: Path, timeout: int = 600) -> None:
    result = subprocess.run(command, cwd=cwd, env=environment, capture_output=True,
                            text=True, encoding='utf-8', errors='replace', timeout=timeout, umask=0o022)
    log_path.write_text(result.stdout + result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'Portable tool exited {result.returncode}: {command[0]}\n'
                           + (result.stdout + result.stderr)[-6000:])


def is_elf(path: Path) -> bool:
    if not path.is_file():
        return False
    with path.open('rb') as stream:
        return stream.read(4) == b'\x7fELF'


def sdl_dynamic_libraries(paths: Sequence[Path]) -> list[str]:
    """SDL's compiled-in dynamic backends are not represented by DT_NEEDED."""
    names = (
        'libX11.so.6', 'libXext.so.6', 'libXcursor.so.1', 'libXfixes.so.3', 'libXi.so.6',
        'libXrandr.so.2', 'libXss.so.1', 'libXtst.so.6', 'libasound.so.2', 'libjack.so.0',
        'libpulse.so.0', 'libpipewire-0.3.so.0', 'libsndio.so.7', 'libwayland-client.so.0',
        'libwayland-cursor.so.0', 'libwayland-egl.so.1', 'libxkbcommon.so.0', 'libdecor-0.so.0',
        'libdrm.so.2', 'libgbm.so.1', 'libudev.so.1', 'libusb-1.0.so.0',
    )
    found = set()
    for path in paths:
        content = b'\0' + path.read_bytes()
        found.update(name for name in names if b'\0' + name.encode('ascii') + b'\0' in content)
    return sorted(found)


def add_soname_aliases(root: Path) -> None:
    """Create real names for loader lookup; a SONAME alone is not a filesystem alias."""
    for path in sorted(root.rglob('*')):
        if not is_elf(path):
            continue
        info = inspect_elf(path)
        if info.soname:
            if Path(info.soname).name != info.soname or info.soname in ('.', '..'):
                raise ValueError(f'Non-portable SONAME in {path}: {info.soname}')
            alias = path.parent / info.soname
            if alias.exists():
                if sha256(alias) != sha256(path):
                    raise ValueError(f'Conflicting SONAME alias: {alias} and {path}')
            else:
                shutil.copy2(path, alias)


def materialize_external_links(root: Path) -> None:
    for current, directories, filenames in os.walk(root, followlinks=False):
        for name in directories + filenames:
            path = Path(current) / name
            if not path.is_symlink():
                continue
            target = path.resolve(strict=True)
            if target.is_relative_to(root.resolve()):
                continue
            path.unlink()
            if target.is_file():
                shutil.copy2(target, path)
            elif target.is_dir():
                shutil.copytree(target, path, symlinks=False)
            else:
                raise ValueError(f'Unsupported external link in deployed runtime: {path} -> {target}')


def normalize_loader_aliases(appdir: Path, interpreter: str) -> None:
    """Deploy can patch two copies of the same loader differently; keep one coherent image."""
    canonical = appdir / interpreter.lstrip('/')
    if not canonical.is_file():
        raise ElfError(f'Private ELF interpreter was not deployed: {interpreter}')

    def build_id(path: Path) -> str:
        result = subprocess.run(['readelf', '--notes', str(path)], check=True, capture_output=True,
                                text=True, env={**os.environ, 'LC_ALL': 'C'}, timeout=30)
        match = re.search(r'Build ID:\s+([0-9a-f]+)', result.stdout)
        if not match or result.stderr:
            raise ElfError(f'Cannot establish loader build identity: {path}')
        return match[1]

    identity = build_id(canonical)
    soname = inspect_elf(canonical).soname
    for path in sorted(appdir.rglob('ld-*.so*')):
        if path == canonical or not is_elf(path):
            continue
        if inspect_elf(path).soname != soname:
            continue
        if build_id(path) != identity:
            raise ElfError(f'Different loader builds in one private runtime: {canonical} and {path}')
        if sha256(path) != sha256(canonical):
            shutil.copyfile(canonical, path)


def restore_system_libraries(appdir: Path, protected: set[str], extras: Sequence[Path]) -> None:
    """Recover trusted host-library bytes after deploy rewrites aliases independently.

    The private loader's complete library path makes deployment RPATH rewriting unnecessary. A GNU
    build ID ties a deployed mirror path to its packaging-host input; application/update bytes remain
    owned by the ordinary packager. Unknown origins must still pass the independent validator.
    """
    def identity(binary: Path) -> str | None:
        result = subprocess.run(['readelf', '--notes', str(binary)], check=True, capture_output=True,
                                text=True, env={**os.environ, 'LC_ALL': 'C'}, timeout=30)
        match = re.search(r'Build ID:\s+([0-9a-f]+)', result.stdout)
        return match.group(1) if match and not result.stderr else None

    for path in sorted(appdir.rglob('*')):
        if not is_elf(path) or path.name in protected:
            continue
        original = Path('/') / path.relative_to(appdir)
        if not is_elf(original):
            continue
        build_id = identity(original)
        if build_id and identity(path) == build_id:
            path.write_bytes(original.read_bytes())
    for original in extras:
        for name in {original.name, inspect_elf(original).soname} - {None}:
            target = appdir / 'usr/bin' / name
            if target.is_file() and target.name not in protected:
                target.write_bytes(original.read_bytes())


def find_system_library(name: str, *, bits: int, machine: int) -> Path:
    """Resolve explicit dlopen modules using host metadata rather than executing the game."""
    if Path(name).is_file():
        candidates = [Path(name).absolute()]
    else:
        ldconfig = shutil.which('ldconfig') or '/usr/sbin/ldconfig'
        result = subprocess.run([ldconfig, '-p'], check=True, capture_output=True, text=True,
                                env={**os.environ, 'LC_ALL': 'C'}, timeout=30)
        candidates = []
        for line in result.stdout.splitlines():
            match = re.match(r'\s*(\S+)\s+\([^)]*\)\s+=>\s+(.+)$', line)
            if match and match[1] == name:
                candidates.append(Path(match[2]).resolve())
    for candidate in candidates:
        info = inspect_elf(candidate)
        if (info.bits, info.machine) == (bits, machine):
            return candidate
    raise ValueError(f'Explicit Linux runtime library not found for {bits}-bit machine {machine}: {name}')


def write_desktop(appdir: Path, *, name: str, entry: str, version: str = '') -> None:
    if any(character in name + entry + version for character in '\n\r\x00'):
        raise ValueError('Desktop metadata must fit on a single line')
    content = f'[Desktop Entry]\nType=Application\nName={name}\nExec={entry}\nIcon=fonline\nCategories=Game;\n'
    if version:
        content += f'X-AppImage-Version={version}\n'
    desktop = appdir / 'usr/share/applications/fonline.desktop'
    desktop.parent.mkdir(parents=True, exist_ok=True)
    desktop.write_text(content, encoding='utf-8')
    (appdir / 'fonline.desktop').write_text(content, encoding='utf-8')
    (appdir / 'fonline.svg').write_text(
        '<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">'
        '<rect width="128" height="128" rx="20" fill="#263640"/>'
        '<path d="M30 29h34v12H43v21h18v12H43v25H30zm43 0h13v57h23v13H73z" fill="#e5bd78"/>'
        '</svg>\n', encoding='utf-8')
    icon = appdir / '.DirIcon'
    if icon.exists() or icon.is_symlink():
        icon.unlink()
    icon.symlink_to('fonline.svg')


def compile_launcher(appdir: Path, temporary: Path, expected: ElfInfo) -> None:
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    source = BUILDTOOLS / 'linux_apprun.c'
    object_file = temporary / 'AppRun.o'
    for arguments in [
        ['-std=c11', '-Os', '-Wall', '-Wextra', '-Werror', '-c', str(source), '-o', str(object_file)],
        ['-static', str(object_file), '-o', str(appdir / 'AppRun')],
    ]:
        result = subprocess.run([*compiler, *arguments], capture_output=True, text=True,
                                encoding='utf-8', errors='replace', timeout=120)
        if result.returncode:
            raise RuntimeError('Portable launcher compilation failed:\n' + result.stdout + result.stderr)
    info = inspect_elf(appdir / 'AppRun')
    if info.interpreter or info.needed or (info.bits, info.machine) != (expected.bits, expected.machine):
        raise ValueError('Portable launcher must have no dynamic dependencies and match the application architecture')
    (appdir / 'AppRun').chmod(0o755)
    relink = appdir / 'usr/share/fonline-launcher'
    relink.mkdir(parents=True)
    shutil.copy2(source, relink / source.name)
    shutil.copy2(BUILDTOOLS.parent / 'LICENSE', relink / 'LICENSE')
    with (relink / 'AppRun.o.gz').open('wb') as output:
        with gzip.GzipFile(fileobj=output, mode='wb', mtime=0) as compressed:
            compressed.write(object_file.read_bytes())
    (relink / 'RELINK.txt').write_text(
        'The launcher is linked with the packaging host C library. Its relocatable object and source are included.\n'
        'To relink using another compatible C library: gzip -dc AppRun.o.gz > AppRun.o; cc -static AppRun.o -o AppRun\n'
        'Keep AppRun next to AppRun.conf at the package root. Runtime dependency notices are in usr/share/doc.\n',
        encoding='utf-8')


def write_manifest(appdir: Path, *, identity: Mapping[str, str], architecture: str,
                   source_hashes: Mapping[str, str], infos: Sequence[ElfInfo],
                   tools: Mapping[str, Path]) -> None:
    files = {}
    for path in sorted(appdir.rglob('*')):
        if path.is_file() and path != appdir / MANIFEST:
            files[path.relative_to(appdir).as_posix()] = {
                'sha256': sha256(path), 'size': path.stat().st_size,
                'mode': f'{path.stat().st_mode & 0o777:04o}',
                'symlink': path.readlink().as_posix() if path.is_symlink() else None,
            }
    manifest = {
        'schema_version': 1, 'identity': dict(identity), 'architecture': architecture,
        'entrypoint': 'AppRun', 'application_source_sha256': dict(source_hashes),
        'tools': {role: {**tool_catalog()['tools'][role]['architectures'][architecture],
                         'repository': tool_catalog()['tools'][role]['repository'],
                         'revision': tool_catalog()['tools'][role]['revision']} for role in tools},
        'runtime': [{'path': info.path.relative_to(appdir).as_posix(), 'bits': info.bits,
                     'machine': info.machine, 'needed': list(info.needed), 'soname': info.soname,
                     'rpath': list(info.rpath), 'has_runpath': info.has_runpath,
                     'required_versions': {name: sorted(versions) for name, versions in info.required_versions.items()},
                     'provided_versions': sorted(info.provided_versions)} for info in infos],
        'host_boundary': ['Linux kernel and CPU ISA', 'device/GPU drivers', 'network and machine configuration',
                          'external services such as MongoDB'],
        'mutable_paths': ['data/'], 'files': files,
    }
    (appdir / MANIFEST).write_text(json.dumps(manifest, sort_keys=True, indent=2) + '\n', encoding='utf-8')


def prepare_portable(source: Path, destination: Path, entrypoint: str, *, appimage: bool,
                     tools: Mapping[str, Path], extra_libraries: Sequence[Path],
                     identity: Mapping[str, str], ca_bundle: Path | None = None) -> Path:
    if sys.platform != 'linux':
        raise RuntimeError('Portable Linux packaging runs in the matching Linux build/runtime environment')
    source = source.resolve(strict=True)
    destination = destination.absolute()
    if destination.exists():
        raise FileExistsError(f'Portable output already exists: {destination}')
    if Path(entrypoint).name != entrypoint or entrypoint in ('', '.', '..', DEPLOY_ENTRY):
        raise ValueError(f'Invalid portable entrypoint filename: {entrypoint}')
    if destination.is_relative_to(source):
        raise ValueError('Portable output cannot be placed inside its source tree')
    main = inspect_elf(source / entrypoint)
    if not main.interpreter:
        raise ValueError('Portable runtime preparation requires an ELF executable with an interpreter')
    architecture = architecture_of(main)
    if identity.get('architecture', architecture) != architecture:
        raise ElfError(f'Application architecture {architecture} differs from the requested package architecture')
    used_tools = {role: tools[role] for role in (('deploy', 'appimagetool', 'runtime') if appimage else ('deploy',))}
    for role, path in used_tools.items():
        verify_tool(path, role, architecture)
    certificates = ca_bundle.resolve(strict=True) if ca_bundle else next((path for path in [
        Path('/etc/ssl/certs/ca-certificates.crt'), Path('/etc/pki/tls/certs/ca-bundle.crt'),
        Path('/etc/ssl/cert.pem')] if path.is_file()), None)
    if certificates is None or not certificates.is_file():
        raise FileNotFoundError('A CA certificate bundle is required in the Linux packaging environment')
    validate_ca_bundle(certificates)
    source_hashes = {}
    for path in source.rglob('*'):
        if path.is_symlink() and not path.resolve(strict=True).is_relative_to(source):
            raise ValueError(f'Package input link escapes its source tree: {path}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=destination.name + '.', dir=destination.parent) as work_name:
        work = Path(work_name)
        appdir = work / 'AppDir'
        binaries = appdir / 'usr/bin'
        binaries.mkdir(parents=True)
        originals = {}
        for path in sorted(source.iterdir()):
            if is_elf(path):
                originals[path.name] = path.read_bytes()
                source_hashes[path.name] = sha256(path)
                shutil.copy2(path, binaries / (DEPLOY_ENTRY if path.name == entrypoint else path.name))
        for library in extra_libraries:
            target = binaries / library.name
            library = library.resolve(strict=True)
            if target.exists() and sha256(target) != sha256(library):
                raise ValueError(f'Conflicting explicit runtime library: {target}')
            shutil.copy2(library, target)
            if inspect_elf(library).soname == 'libasound.so.2':
                plugins = library.parent / 'alsa-lib'
                destination_plugins = binaries / 'alsa-lib'
                if plugins.is_dir():
                    shutil.copytree(plugins, destination_plugins, dirs_exist_ok=True, symlinks=False)
                else:
                    # The pinned deployer expects the optional plugin directory even on a host without plugins.
                    destination_plugins.mkdir(exist_ok=True)
        add_soname_aliases(binaries)
        write_desktop(appdir, name=identity.get('name', entrypoint), entry=DEPLOY_ENTRY, version=identity.get('version', 'dev'))
        tool_tmp = work / 'tool-tmp'
        tool_tmp.mkdir()
        environment = tool_environment(tool_tmp, architecture, identity.get('version', 'dev'))
        run_tool([str(tools['deploy'].resolve()), '-s', 'deploy',
                  str(appdir / 'usr/share/applications/fonline.desktop')],
                 cwd=work, environment=environment, log_path=work / 'deploy.log')
        materialize_external_links(appdir)
        alsa_data = Path('/usr/share/alsa')
        if (binaries / 'alsa-lib').is_dir() and alsa_data.is_dir():
            shutil.copytree(alsa_data, appdir / 'usr/share/alsa', dirs_exist_ok=True, symlinks=False)
        (binaries / DEPLOY_ENTRY).unlink()
        for name, content in originals.items():
            (binaries / name).write_bytes(content)
            (binaries / name).chmod(0o755 if name == entrypoint else 0o644)
        # Restore aliases of game-owned files too: their updater identity includes the ELF bytes
        for name, content in originals.items():
            info = inspect_elf(binaries / name)
            if info.soname and info.soname not in originals:
                (binaries / info.soname).write_bytes(content)
        protected = set(originals)
        protected.update(filter(None, (inspect_elf(binaries / name).soname for name in originals)))
        restore_system_libraries(appdir, protected, extra_libraries)
        normalize_loader_aliases(appdir, main.interpreter)
        add_soname_aliases(appdir)
        compile_launcher(appdir, work, main)
        infos = validate_elf_closure(appdir)
        library_dirs = sorted({info.path.parent.relative_to(appdir).as_posix() for info in infos
                               if info.path != appdir / 'AppRun'})
        libraries = ':'.join('$ORIGIN' + ('/' + relative if relative != '.' else '')
                             for directory in library_dirs
                             for relative in [posixpath.relpath(directory, 'usr/bin')])
        loader = appdir / main.interpreter.lstrip('/')
        if not loader.is_file():
            raise ElfError(f'Private ELF interpreter was not deployed: {main.interpreter}')
        loader.chmod(0o755)
        data = appdir / 'usr/share/fonline'
        data.mkdir(parents=True, exist_ok=True)
        for path in sorted(source.iterdir()):
            if path.name in originals:
                continue
            if path.is_dir():
                shutil.copytree(path, data / path.name, symlinks=False)
            elif path.is_file():
                shutil.copy2(path, data / path.name)
        ca = appdir / 'usr/share/fonline-certificates/ca-certificates.crt'
        ca.parent.mkdir()
        shutil.copy2(certificates, ca)
        gconv = next((directory for directory in library_dirs if directory.endswith('/gconv')), '')
        config = {
            'entry': 'usr/bin/' + entrypoint, 'loader': loader.relative_to(appdir).as_posix(),
            'libraries': libraries, 'data': 'usr/share/fonline',
            'policy': 'external' if appimage else 'local', 'preserve_cwd': identity.get('preserve_cwd', '0'),
            'gconv': gconv, 'ca': ca.relative_to(appdir).as_posix(),
            'ca_setting': identity.get('ca_setting', ''),
        }
        if any('\n' in value or '\r' in value or '\x00' in value for value in config.values()):
            raise ValueError('Launcher configuration values must fit on one line')
        (appdir / 'AppRun.conf').write_text(''.join(f'{key}={value}\n' for key, value in config.items()), encoding='utf-8')
        write_desktop(appdir, name=identity.get('name', entrypoint), entry='AppRun', version=identity.get('version', 'dev'))
        notices = appdir / 'usr/share/doc/fonline-portable'
        notices.mkdir(parents=True, exist_ok=True)
        packages = {'libc6', 'libc6-dev', 'libgcc-s1', 'ca-certificates'}
        dpkg_query = shutil.which('dpkg-query')
        if dpkg_query:
            host_paths = sorted({str(Path('/') / info.path.relative_to(appdir)) for info in infos
                                 if (Path('/') / info.path.relative_to(appdir)).is_file()})
            host_paths.extend(str(path) for path in extra_libraries)
            if host_paths:
                owners = subprocess.run([dpkg_query, '--search', *host_paths], capture_output=True,
                                        text=True, env={**os.environ, 'LC_ALL': 'C'}, timeout=30)
                packages.update(line.split(': ', 1)[0].split(':', 1)[0]
                                for line in owners.stdout.splitlines() if ': ' in line)
        for package in sorted(packages):
            notice = Path('/usr/share/doc') / package / 'copyright'
            if notice.is_file():
                shutil.copy2(notice, notices / (package + '.copyright'))
        if appimage:
            shutil.copy2(BUILDTOOLS / 'AppImageRuntime.LICENSE', notices / 'AppImageRuntime.LICENSE')
        for path in appdir.rglob('*'):
            if path.is_dir():
                path.chmod(0o755)
            elif path.is_file():
                path.chmod(0o755 if path.stat().st_mode & 0o111 else 0o644)
        manifest_identity = {**identity, 'format': 'AppImage' if appimage else 'Portable'}
        write_manifest(appdir, identity=manifest_identity, architecture=architecture, source_hashes=source_hashes,
                       infos=infos, tools=used_tools)
        appdir.rename(destination)
    return destination


def validate_manifest_files(appdir: Path, manifest: Mapping) -> None:
    expected = set(manifest['files']) | {MANIFEST}
    actual = {path.relative_to(appdir).as_posix() for path in appdir.rglob('*') if path.is_file()}
    if actual != expected:
        raise ValueError('AppDir changed after portable validation: file set differs')
    for name, metadata in manifest['files'].items():
        path = appdir / name
        # Archives may materialize internal aliases as regular files; logical bytes and modes remain authoritative.
        if (not path.resolve().is_relative_to(appdir) or not path.is_file()
                or sha256(path) != metadata['sha256']
                or path.stat().st_mode & 0o777 != int(metadata['mode'], 8)):
            raise ValueError(f'AppDir changed after portable validation: {name}')


def make_appimage(appdir: Path, destination: Path, *, tools: Mapping[str, Path], architecture: str) -> None:
    if sys.platform != 'linux':
        raise RuntimeError('AppImage creation runs in the matching Linux build/runtime environment')
    appdir, destination = appdir.resolve(strict=True), destination.absolute()
    config = dict(line.split('=', 1) for line in (appdir / 'AppRun.conf').read_text(encoding='utf-8').splitlines())
    if config.get('policy') != 'external':
        raise ValueError('AppImage requires the external writable-data policy')
    manifest = json.loads((appdir / MANIFEST).read_text(encoding='utf-8'))
    validate_manifest_files(appdir, manifest)
    for role in ['appimagetool', 'runtime']:
        verify_tool(tools[role], role, architecture)
    if destination.exists():
        raise FileExistsError(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=destination.name + '.', dir=destination.parent) as work_name:
        work = Path(work_name)
        output = work / 'output.AppImage'
        environment = tool_environment(work, architecture, manifest['identity'].get('version', 'dev'))
        run_tool([str(tools['appimagetool'].resolve()), '--runtime-file', str(tools['runtime'].resolve()),
                  str(appdir.resolve()), str(output)], cwd=work, environment=environment,
                 log_path=work / 'appimagetool.log')
        validate_manifest_files(appdir, manifest)
        if not output.is_file():
            raise RuntimeError('appimagetool did not create the requested AppImage')
        with output.open('rb') as stream:
            if stream.read(11)[8:11] != b'AI\x02':
                raise RuntimeError('appimagetool output is not a type-2 AppImage')
        checksum = work / 'output.sha256'
        checksum.write_text(sha256(output) + '  ' + destination.name + '\n', encoding='utf-8')
        output.chmod(0o755)
        output.rename(destination)
        checksum.rename(destination.with_suffix(destination.suffix + '.sha256'))
