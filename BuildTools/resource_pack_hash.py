"""Load the host-only FNV-1a accelerator used by resource archive packaging."""

from __future__ import annotations

import ctypes
import platform
import sys
from pathlib import Path
from typing import Sequence


FNV_OFFSET = 0xCBF29CE484222325


def library_name() -> str:
	return 'FOnlineResourcePackHash' + ('.dll' if sys.platform == 'win32' else '.dylib' if sys.platform == 'darwin' else '.so')


def discover_library(inputs: Sequence[str], explicit: str | None = None) -> str | None:
	if explicit is not None:
		path = Path(explicit).resolve()
		assert path.is_file(), 'Resource hash library not found: ' + str(path)
		return str(path)

	if sys.maxsize <= (1 << 32):
		return None

	architecture = platform.machine().lower()
	if architecture in ('amd64', 'x86_64'):
		architecture = 'win64' if sys.platform == 'win32' else 'x64'
	elif architecture in ('aarch64', 'arm64'):
		architecture = 'arm64'
	else:
		return None

	host = {'win32': 'Windows', 'linux': 'Linux', 'darwin': 'macOS'}.get(sys.platform)
	if host is None:
		return None

	for root in inputs:
		path = Path(root) / 'Binaries' / f'BuildTools-{host}-{architecture}' / library_name()
		if path.is_file():
			return str(path.resolve())

	return None


def _load_library(path: str) -> ctypes.CDLL:
	if sys.platform != 'win32':
		return ctypes.CDLL(path)

	kernel = ctypes.WinDLL('kernel32', use_last_error=True)
	kernel.GetThreadErrorMode.argtypes = []
	kernel.GetThreadErrorMode.restype = ctypes.c_uint32
	kernel.SetThreadErrorMode.argtypes = [ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)]
	kernel.SetThreadErrorMode.restype = ctypes.c_int
	previous_mode = ctypes.c_uint32()

	# Invalid DLLs must report loader errors without an interactive Windows dialog
	if not kernel.SetThreadErrorMode(kernel.GetThreadErrorMode() | 0x0001, ctypes.byref(previous_mode)):
		raise ctypes.WinError(ctypes.get_last_error())

	try:
		return ctypes.CDLL(path)
	finally:
		if not kernel.SetThreadErrorMode(previous_mode.value, None):
			raise ctypes.WinError(ctypes.get_last_error())


class ResourcePackHasher:
	def __init__(self, path: str) -> None:
		self.library = _load_library(path)
		self.function = self.library.FO_Fnv1a64
		self.function.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_uint64]
		self.function.restype = ctypes.c_uint64
		assert self.hash_bytes(b'') == FNV_OFFSET and self.hash_bytes(b'foobar') == 0x85944171F73967E8, 'Resource hash library has an invalid FNV-1a contract: ' + path
		assert self.hash_bytes(b'bar', self.hash_bytes(b'foo')) == 0x85944171F73967E8, 'Resource hash library does not preserve the streaming seed: ' + path

	def hash_bytes(self, data: bytes, seed: int = FNV_OFFSET) -> int:
		buffer = bytes(data)
		return self.function(buffer, len(buffer), seed) if buffer else seed
