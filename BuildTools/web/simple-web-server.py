#!/usr/bin/python3

from __future__ import annotations

import argparse
import functools
import http.server
import os
import socketserver
import sys


class ReusableThreadingTCPServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


class NoCacheHttpRequestHandler(http.server.SimpleHTTPRequestHandler):
    timeout = 30

    def end_headers(self) -> None:
        self.send_header('Cache-Control', 'no-store, no-cache, must-revalidate, max-age=0')
        self.send_header('Pragma', 'no-cache')
        self.send_header('Expires', '0')
        super().end_headers()


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description='Simple HTTP server')
    parser.add_argument('--port', dest='port', type=int, default=7000, help='web server port')
    parser.add_argument('--fork', dest='fork', action='store_true', help='fork process')
    return parser


def maybe_fork(enable_fork: bool) -> None:
    fork = getattr(os, 'fork', None)
    if enable_fork and callable(fork):
        if fork():
            sys.exit(0)


def main() -> None:
    args = create_parser().parse_args()
    maybe_fork(args.fork)

    serve_dir = os.path.dirname(os.path.realpath(__file__))
    handler = functools.partial(NoCacheHttpRequestHandler, directory=serve_dir)
    with ReusableThreadingTCPServer(('', args.port), handler) as httpd:
        print('Serving', serve_dir, 'at port', args.port)
        httpd.serve_forever()


if __name__ == '__main__':
    main()
