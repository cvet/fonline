"""Exercise Mongo recovery reads through libmongoc against a loopback wire fixture.

This is a bounded fault fixture, not a replacement for a real MongoDB integration run.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import threading


class Int64(int):
    pass


def cstring(value: str) -> bytes:
    return value.encode('utf-8') + b'\0'


def bson_document(values: dict) -> bytes:
    result = bytearray()
    for key, value in values.items():
        if isinstance(value, bool):
            result += b'\x08' + cstring(key) + bytes([value])
        elif isinstance(value, float):
            result += b'\x01' + cstring(key) + struct.pack('<d', value)
        elif isinstance(value, Int64):
            result += b'\x12' + cstring(key) + struct.pack('<q', value)
        elif isinstance(value, int):
            result += b'\x10' + cstring(key) + struct.pack('<i', value)
        elif isinstance(value, str):
            text = value.encode('utf-8') + b'\0'
            result += b'\x02' + cstring(key) + struct.pack('<i', len(text)) + text
        elif isinstance(value, dict):
            result += b'\x03' + cstring(key) + bson_document(value)
        elif isinstance(value, list):
            result += b'\x04' + cstring(key) + bson_document({str(i): item for i, item in enumerate(value)})
        else:
            raise TypeError(f'Unsupported BSON fixture value: {type(value)!r}')
    return struct.pack('<i', len(result) + 5) + result + b'\0'


def receive_exact(connection: socket.socket, size: int) -> bytes | None:
    data = bytearray()
    while len(data) < size:
        part = connection.recv(size - len(data))
        if not part:
            return None
        data.extend(part)
    return bytes(data)


class MongoFaultServer:
    def __init__(self, mode: str):
        self.mode = mode
        self.commands: list[dict] = []
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._server = socket.socket()
        self._server.bind(('127.0.0.1', 0))
        self._server.listen()
        self._server.settimeout(0.2)
        self.port = self._server.getsockname()[1]
        self._thread = threading.Thread(target=self._accept, daemon=True)
        self._workers: list[threading.Thread] = []

    def start(self) -> None:
        self._thread.start()

    def close(self) -> None:
        self._stop.set()
        self._server.close()
        self._thread.join(timeout=2)
        if self._thread.is_alive():
            raise RuntimeError('Loopback fixture did not stop')
        for worker in self._workers:
            worker.join(timeout=2)
            if worker.is_alive():
                raise RuntimeError('Loopback connection handler did not stop')

    def _accept(self) -> None:
        while not self._stop.is_set():
            try:
                connection, _ = self._server.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            worker = threading.Thread(target=self._serve, args=(connection,), daemon=True)
            self._workers.append(worker)
            worker.start()

    def _serve(self, connection: socket.socket) -> None:
        with connection:
            connection.settimeout(10)
            try:
                while not self._stop.is_set():
                    header = receive_exact(connection, 16)
                    if not header:
                        return
                    length, request_id, _, opcode = struct.unpack('<iiii', header)
                    if length < 16 or length > 32_000_000:
                        raise ValueError(f'Invalid message length: {length}')
                    payload = receive_exact(connection, length - 16)
                    if payload is None:
                        return
                    if opcode == 2004:
                        offset = payload.index(b'\0', 4) + 1 + 8
                    elif opcode == 2013 and payload[4] == 0:
                        offset = 5
                    else:
                        raise ValueError(f'Unsupported Mongo message opcode: {opcode}')
                    end = payload.index(b'\0', offset + 5)
                    command = payload[offset + 5:end].decode('utf-8')
                    with self._lock:
                        self.commands.append({'command': command, 'opcode': opcode})
                    lower = command.lower()
                    if lower in ('ismaster', 'hello'):
                        reply = {'ok': 1.0, 'ismaster': True, 'isWritablePrimary': True,
                                 'helloOk': True, 'minWireVersion': 0, 'maxWireVersion': 17,
                                 'logicalSessionTimeoutMinutes': 30, 'maxBsonObjectSize': 16_777_216,
                                 'maxMessageSizeBytes': 48_000_000, 'maxWriteBatchSize': 100_000}
                    elif lower == 'listcollections':
                        reply = {'ok': 1.0, 'cursor': {'id': Int64(0), 'ns': 'recovery_fault_test.$cmd.listCollections',
                                                       'firstBatch': [{'name': 'test_collection'}]}}
                    elif lower == 'find' and self.mode == 'error':
                        reply = {'ok': 0.0, 'code': 123, 'errmsg': 'injected recovery find error'}
                    elif lower == 'find':
                        if self.mode in ('present', 'delete-error'):
                            batch = [{'_id': Int64(1001), 'value': Int64(1)}]
                        elif self.mode in ('unmatched', 'matched', 'unchanged'):
                            batch = [{'_id': Int64(1001), 'value': Int64(0)}]
                        else:
                            batch = []
                        reply = {'ok': 1.0, 'cursor': {'id': Int64(0), 'ns': 'recovery_fault_test.test_collection',
                                                       'firstBatch': batch}}
                    elif lower in ('insert', 'delete') and self.mode == lower + '-error':
                        reply = {'ok': 0.0, 'code': 123, 'errmsg': 'injected recovery write error'}
                    elif lower == 'update' and self.mode in ('unmatched', 'matched', 'unchanged'):
                        count = 0 if self.mode == 'unmatched' else 1
                        reply = {'ok': 1.0, 'n': count, 'nModified': 0 if self.mode == 'unchanged' else count}
                    elif lower in ('ping', 'endsessions'):
                        reply = {'ok': 1.0}
                    else:
                        raise ValueError(f'Unexpected Mongo command: {command}')
                    body = bson_document(reply)
                    if opcode == 2004:
                        response_body = struct.pack('<iqii', 0, 0, 0, 1) + body
                        response_opcode = 1
                    else:
                        response_body = struct.pack('<i', 0) + b'\0' + body
                        response_opcode = 2013
                    connection.sendall(struct.pack('<iiii', 16 + len(response_body), 1, request_id, response_opcode) + response_body)
            except (ConnectionError, TimeoutError, OSError):
                return
            except Exception as exc:
                with self._lock:
                    self.commands.append({'fixture_error': repr(exc)})


def run_case(test_exe: Path, project_dir: Path, output_dir: Path, mode: str) -> bool:
    cases = {'insert-error': 'MongoRecoveryRejectsWriteErrorWithoutClearingJournal',
             'delete-error': 'MongoRecoveryRejectsWriteErrorWithoutClearingJournal',
             'error': 'MongoRecoveryRejectsFindErrorWithoutClearingJournal',
             'missing': 'MongoRecoveryAcceptsMissingRecordForDelete',
             'present': 'MongoRecoveryAcceptsAlreadyAppliedUpdate',
             'unmatched': 'MongoRecoveryRejectsUnmatchedUpdateWithoutClearingJournal',
             'matched': 'MongoRecoveryAcceptsMatchedUpdate',
             'unchanged': 'MongoRecoveryAcceptsMatchedUpdate'}
    temp_dir = output_dir / f'temp-{mode}'
    temp_dir.mkdir(parents=True, exist_ok=True)
    server = MongoFaultServer(mode)
    server.start()
    env = os.environ.copy()
    env.update(FO_TEST_MONGO_FAULT_URI=f'mongodb://127.0.0.1:{server.port}/?directConnection=true&serverSelectionTimeoutMS=3000',
               TEMP=str(temp_dir), TMP=str(temp_dir))
    if mode in ('insert-error', 'delete-error'):
        env['FO_TEST_MONGO_WRITE_OP'] = mode.split('-')[0]
    process = None
    output = b''
    timed_out = False
    try:
        process = subprocess.Popen([str(test_exe), cases[mode]], cwd=project_dir, env=env,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        try:
            output, _ = process.communicate(timeout=30)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.kill()
            output, _ = process.communicate()
    finally:
        server.close()
    (output_dir / f'{mode}.log').write_bytes(output)
    (output_dir / f'{mode}-commands.json').write_text(json.dumps(server.commands, indent=2), encoding='utf-8')
    commands = [item.get('command', '').lower() for item in server.commands]
    fixture_errors = [item['fixture_error'] for item in server.commands if 'fixture_error' in item]
    passed = (not timed_out and process is not None and process.returncode == 0 and
              b'All tests passed' in output and 'find' in commands and not fixture_errors and
              (mode not in ('unmatched', 'matched', 'unchanged') or 'update' in commands) and
              (not mode.endswith('-error') or mode.split('-')[0] in commands))
    print(f'{mode}: {"PASS" if passed else "FAIL"}; exit={process.returncode if process else None}; commands={commands}')
    if fixture_errors:
        print(f'Fixture errors: {fixture_errors}')
    return passed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-exe', required=True, type=Path)
    parser.add_argument('--project-dir', required=True, type=Path)
    parser.add_argument('--output-dir', required=True, type=Path)
    args = parser.parse_args()
    test_exe = args.test_exe.resolve(strict=True)
    project_dir = args.project_dir.resolve(strict=True)
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    results = [run_case(test_exe, project_dir, output_dir, mode) for mode in ('error', 'missing', 'present', 'unmatched', 'matched', 'unchanged', 'insert-error', 'delete-error')]
    return 0 if all(results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
