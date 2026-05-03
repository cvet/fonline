#!/usr/bin/python3

from __future__ import annotations

import argparse
import shutil
import socket
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


DEFAULT_LOGCAT_FILTERS = ['-s', 'SDL:V', 'FOnline:V', 'LF:V', '*:E']
MDNS_CONNECT_MARKERS = ('_adb-tls-connect._tcp', '_adb._tcp')
CONNECTED_DEVICE_STATES = {'device', 'authorizing'}


@dataclass(frozen=True)
class MdnsDevice:
	description: str
	endpoint: str


def log(*parts: object) -> None:
	print('[AndroidDevice]', *parts, flush=True)


def resolve_workspace_root(workspace_root: str | None) -> Path:
	if workspace_root:
		return Path(workspace_root).resolve()
	return (Path.cwd() / 'Workspace').resolve()


def resolve_adb_path(workspace_root: Path) -> str:
	adb_name = 'adb.exe' if os_name_is_windows() else 'adb'
	workspace_adb = workspace_root / 'android-sdk' / 'platform-tools' / adb_name
	if workspace_adb.is_file():
		return str(workspace_adb)

	adb_path = shutil.which('adb')
	if adb_path:
		return adb_path

	raise SystemExit('adb not found; prepare Android workspace or install adb on the host')


def os_name_is_windows() -> bool:
	return sys.platform == 'win32'


def run_capture(command: list[str], check: bool = True) -> str:
	result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf-8')
	output = result.stdout.strip()
	if check and result.returncode != 0:
		raise SystemExit(output or f'Command failed: {" ".join(command)}')
	return output


def run_capture_result(command: list[str]) -> tuple[int, str]:
	result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf-8')
	return result.returncode, result.stdout.strip()


def run_passthrough(command: list[str]) -> int:
	return subprocess.call(command)


def get_device_property(adb_path: str, endpoint: str, prop_name: str) -> str:
	return run_capture([adb_path, '-s', endpoint, 'shell', 'getprop', prop_name], check=False).strip()


def describe_device(adb_path: str, endpoint: str) -> str:
	manufacturer = get_device_property(adb_path, endpoint, 'ro.product.manufacturer')
	model = get_device_property(adb_path, endpoint, 'ro.product.model')
	parts = [part for part in [manufacturer, model] if part]
	if parts:
		return ' '.join(parts)
	return endpoint


def log_install_failed_user_restricted(adb_path: str, endpoint: str) -> None:
	device_name = describe_device(adb_path, endpoint)
	log(f'Install blocked by device policy on {device_name}')
	print('The APK push reached the device, but Android canceled the installation on the device side.', flush=True)
	print('What to do on the device:', flush=True)
	print('  1. Unlock the screen and keep the device awake.', flush=True)
	print('  2. Confirm any installation or security prompt shown by wireless debugging or Package Installer.', flush=True)
	print('  3. If this is Xiaomi/MIUI/HyperOS, enable Developer options -> Install via USB and USB debugging (Security settings).', flush=True)
	print('  4. If Android warns about wireless debugging or unknown app install, explicitly allow it and rerun the install task.', flush=True)
	print('  5. If the app was canceled once, rerun the same task after confirming the device-side prompt.', flush=True)


def state_file_path(workspace_root: Path) -> Path:
	return workspace_root / 'android-debug' / 'device-endpoint.txt'


def read_saved_endpoint(workspace_root: Path) -> str:
	state_file = state_file_path(workspace_root)
	if not state_file.is_file():
		return ''
	return state_file.read_text(encoding='utf-8').strip()


def write_saved_endpoint(workspace_root: Path, endpoint: str) -> None:
	state_file = state_file_path(workspace_root)
	state_file.parent.mkdir(parents=True, exist_ok=True)
	state_file.write_text(endpoint + '\n', encoding='utf-8')


def normalize_endpoint(value: str) -> str:
	endpoint = value.strip()
	if not endpoint:
		raise SystemExit('Android device endpoint is empty')
	if endpoint.count(':') == 0:
		return endpoint + ':5555'
	return endpoint


def split_endpoint(endpoint: str) -> tuple[str, int]:
	normalized_endpoint = normalize_endpoint(endpoint)
	host, separator, port_text = normalized_endpoint.rpartition(':')
	if not separator or not host or not port_text.isdigit():
		raise SystemExit(f'Invalid Android device endpoint: {endpoint}')
	return host, int(port_text)


def detect_host_ip_for_endpoint(endpoint: str) -> str:
	host, port = split_endpoint(endpoint)

	try:
		with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:
			udp_socket.connect((host, port))
			local_ip = udp_socket.getsockname()[0]
	except OSError as exc:
		raise SystemExit(f'Unable to determine host IP for Android device {endpoint}: {exc}') from exc

	if not local_ip or local_ip.startswith('127.'):
		raise SystemExit(
			'Unable to determine a routable host IP for the Android device; use --server-host to specify it explicitly',
		)

	return local_ip


def list_connected_devices(adb_path: str) -> list[tuple[str, str]]:
	output = run_capture([adb_path, 'devices'], check=False)
	devices: list[tuple[str, str]] = []
	for line in output.splitlines():
		line = line.strip()
		if not line or line.startswith('List of devices attached'):
			continue
		parts = line.split()
		if len(parts) < 2:
			continue
		devices.append((parts[0], parts[1]))
	return devices


def is_connected_device(adb_path: str, endpoint: str) -> bool:
	for serial, state in list_connected_devices(adb_path):
		if serial == endpoint and state in CONNECTED_DEVICE_STATES:
			return True
	return False


def parse_mdns_devices(output: str) -> list[MdnsDevice]:
	devices: list[MdnsDevice] = []
	seen: set[str] = set()

	for raw_line in output.splitlines():
		line = raw_line.strip()
		if not line or line.startswith('List of discovered mdns services'):
			continue
		if not any(marker in line for marker in MDNS_CONNECT_MARKERS):
			continue

		tokens = line.split()
		endpoint = ''
		endpoint_index = -1
		for index in range(len(tokens) - 1, -1, -1):
			token = tokens[index].rstrip(',;')
			if ':' not in token:
				continue
			host_part, _, port_part = token.rpartition(':')
			if host_part and port_part.isdigit():
				endpoint = token
				endpoint_index = index
				break

		if not endpoint:
			continue

		endpoint = normalize_endpoint(endpoint)
		if endpoint in seen:
			continue
		seen.add(endpoint)

		description = ' '.join(tokens[:endpoint_index]).strip() if endpoint_index > 0 else endpoint
		devices.append(MdnsDevice(description=description or endpoint, endpoint=endpoint))

	return devices


def discover_mdns_devices(adb_path: str) -> list[MdnsDevice]:
	output = run_capture([adb_path, 'mdns', 'services'], check=False)
	return parse_mdns_devices(output)


def try_connect(adb_path: str, endpoint: str) -> bool:
	output = run_capture([adb_path, 'connect', endpoint], check=False)
	if not output:
		return False
	output_lower = output.lower()
	if 'connected to' in output_lower or 'already connected to' in output_lower:
		log(output)
		return True
	log(output)
	return False


def choose_device_interactively(mdns_devices: list[MdnsDevice]) -> str:
	if not sys.stdin.isatty():
		raise SystemExit('No Android Wi-Fi device detected automatically and interactive input is unavailable')

	if mdns_devices:
		log('Discovered Android Wi-Fi devices:')
		for index, device in enumerate(mdns_devices, start=1):
			print(f'  {index}. {device.endpoint}  {device.description}', flush=True)
		print('Enter a device number or type IP[:port] manually.', flush=True)
	else:
		log('No Android Wi-Fi devices discovered via adb mdns; enter device IP[:port] manually')

	while True:
		answer = input('Android device> ').strip()
		if not answer:
			continue
		if answer.isdigit():
			index = int(answer)
			if 1 <= index <= len(mdns_devices):
				return mdns_devices[index - 1].endpoint
			print('Invalid selection', flush=True)
			continue
		return normalize_endpoint(answer)


def resolve_endpoint(adb_path: str, workspace_root: Path, explicit_endpoint: str | None) -> str:
	if explicit_endpoint:
		endpoint = normalize_endpoint(explicit_endpoint)
		if not is_connected_device(adb_path, endpoint) and not try_connect(adb_path, endpoint):
			raise SystemExit(f'Unable to connect to Android device {endpoint}')
		write_saved_endpoint(workspace_root, endpoint)
		return endpoint

	saved_endpoint = read_saved_endpoint(workspace_root)
	if saved_endpoint:
		saved_endpoint = normalize_endpoint(saved_endpoint)
		if is_connected_device(adb_path, saved_endpoint) or try_connect(adb_path, saved_endpoint):
			write_saved_endpoint(workspace_root, saved_endpoint)
			return saved_endpoint

	connected_network_devices = [
		serial for serial, state in list_connected_devices(adb_path)
		if ':' in serial and state in CONNECTED_DEVICE_STATES
	]
	if len(connected_network_devices) == 1:
		endpoint = connected_network_devices[0]
		write_saved_endpoint(workspace_root, endpoint)
		return endpoint

	mdns_devices = discover_mdns_devices(adb_path)
	endpoint = choose_device_interactively(mdns_devices)
	if not is_connected_device(adb_path, endpoint) and not try_connect(adb_path, endpoint):
		raise SystemExit(f'Unable to connect to Android device {endpoint}')
	write_saved_endpoint(workspace_root, endpoint)
	return endpoint


def command_discover(workspace_root: Path) -> int:
	adb_path = resolve_adb_path(workspace_root)
	mdns_devices = discover_mdns_devices(adb_path)
	if not mdns_devices:
		log('No Android Wi-Fi devices discovered via adb mdns')
		return 0

	log('Discovered Android Wi-Fi devices:')
	for device in mdns_devices:
		print(f'{device.endpoint}\t{device.description}', flush=True)
	return 0


def command_connect(workspace_root: Path, explicit_endpoint: str | None) -> int:
	adb_path = resolve_adb_path(workspace_root)
	endpoint = resolve_endpoint(adb_path, workspace_root, explicit_endpoint)
	log('Using Android device', endpoint)
	return 0


def command_install(workspace_root: Path, apk_path: str, explicit_endpoint: str | None) -> int:
	adb_path = resolve_adb_path(workspace_root)
	endpoint = resolve_endpoint(adb_path, workspace_root, explicit_endpoint)
	return_code, output = run_capture_result([adb_path, '-s', endpoint, 'install', '-r', apk_path])
	if output:
		print(output, flush=True)
	if return_code != 0 and 'INSTALL_FAILED_USER_RESTRICTED' in output:
		log_install_failed_user_restricted(adb_path, endpoint)
	return return_code


def command_launch(workspace_root: Path, activity: str, explicit_endpoint: str | None) -> int:
	adb_path = resolve_adb_path(workspace_root)
	endpoint = resolve_endpoint(adb_path, workspace_root, explicit_endpoint)
	return run_passthrough([adb_path, '-s', endpoint, 'shell', 'am', 'start', '-n', activity])


def command_launch_game(workspace_root: Path, activity: str, explicit_endpoint: str | None, explicit_server_host: str | None) -> int:
	adb_path = resolve_adb_path(workspace_root)
	endpoint = resolve_endpoint(adb_path, workspace_root, explicit_endpoint)
	server_host = explicit_server_host.strip() if explicit_server_host else detect_host_ip_for_endpoint(endpoint)
	if not server_host:
		raise SystemExit('Android server host is empty')
	package_name = activity.split('/', 1)[0]

	log('Launching game with server host', server_host)
	run_passthrough([adb_path, '-s', endpoint, 'shell', 'am', 'force-stop', package_name])
	return run_passthrough([
		adb_path,
		'-s',
		endpoint,
		'shell',
		'am',
		'start',
		'-n',
		activity,
		'--es',
		'server_host',
		server_host,
	])


def command_stop(workspace_root: Path, package_name: str, explicit_endpoint: str | None) -> int:
	adb_path = resolve_adb_path(workspace_root)
	endpoint = resolve_endpoint(adb_path, workspace_root, explicit_endpoint)
	return run_passthrough([adb_path, '-s', endpoint, 'shell', 'am', 'force-stop', package_name])


def command_logcat(workspace_root: Path, explicit_endpoint: str | None) -> int:
	adb_path = resolve_adb_path(workspace_root)
	endpoint = resolve_endpoint(adb_path, workspace_root, explicit_endpoint)
	return run_passthrough([adb_path, '-s', endpoint, 'logcat', *DEFAULT_LOGCAT_FILTERS])


def create_parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description='Android Wi-Fi device helper for BuildTools tasks')
	parser.add_argument('--workspace-root', dest='workspace_root', help='Workspace directory path containing android-sdk and android-debug')

	subparsers = parser.add_subparsers(dest='command', required=True)

	discover_parser = subparsers.add_parser('discover', help='List Android Wi-Fi devices discovered through adb mdns')
	discover_parser.set_defaults(no_args=True)

	connect_parser = subparsers.add_parser('connect', help='Connect to an Android Wi-Fi device and cache the endpoint')
	connect_parser.add_argument('--device', dest='device', help='Device IP[:port]; if omitted, auto-discovery and interactive selection are used')

	install_parser = subparsers.add_parser('install', help='Install an APK on the selected Android Wi-Fi device')
	install_parser.add_argument('--apk', required=True, help='APK path')
	install_parser.add_argument('--device', dest='device', help='Device IP[:port]; if omitted, cached endpoint or discovery is used')

	launch_parser = subparsers.add_parser('launch', help='Launch an Android activity on the selected device')
	launch_parser.add_argument('--activity', required=True, help='Fully qualified activity component, e.g. com.example.game/.FOnlineActivity')
	launch_parser.add_argument('--device', dest='device', help='Device IP[:port]; if omitted, cached endpoint or discovery is used')

	launch_game_parser = subparsers.add_parser('launch-game', help='Launch the Android game activity and pass RemoteSceneLaunch server host override')
	launch_game_parser.add_argument('--activity', required=True, help='Fully qualified activity component, e.g. com.example.game/.FOnlineActivity')
	launch_game_parser.add_argument('--device', dest='device', help='Device IP[:port]; if omitted, cached endpoint or discovery is used')
	launch_game_parser.add_argument('--server-host', dest='server_host', help='Host IP or name for ClientNetwork.ServerHost; if omitted, auto-detected from the route to the selected device')

	stop_parser = subparsers.add_parser('stop', help='Force-stop an Android package on the selected device')
	stop_parser.add_argument('--package', dest='package_name', required=True, help='Android package name')
	stop_parser.add_argument('--device', dest='device', help='Device IP[:port]; if omitted, cached endpoint or discovery is used')

	logcat_parser = subparsers.add_parser('logcat', help='Stream logcat from the selected device')
	logcat_parser.add_argument('--device', dest='device', help='Device IP[:port]; if omitted, cached endpoint or discovery is used')

	return parser


def main() -> None:
	parser = create_parser()
	args = parser.parse_args()
	workspace_root = resolve_workspace_root(args.workspace_root)

	if args.command == 'discover':
		raise SystemExit(command_discover(workspace_root))
	if args.command == 'connect':
		raise SystemExit(command_connect(workspace_root, args.device))
	if args.command == 'install':
		raise SystemExit(command_install(workspace_root, args.apk, args.device))
	if args.command == 'launch':
		raise SystemExit(command_launch(workspace_root, args.activity, args.device))
	if args.command == 'launch-game':
		raise SystemExit(command_launch_game(workspace_root, args.activity, args.device, args.server_host))
	if args.command == 'stop':
		raise SystemExit(command_stop(workspace_root, args.package_name, args.device))
	if args.command == 'logcat':
		raise SystemExit(command_logcat(workspace_root, args.device))

	raise SystemExit(f'Unsupported command: {args.command}')


if __name__ == '__main__':
	main()