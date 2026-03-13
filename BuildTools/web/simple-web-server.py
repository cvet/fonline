#!/usr/bin/python3

from __future__ import annotations

import argparse
import http.server
import os
import socketserver
import sys


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description='Simple HTTP server')
    parser.add_argument('--port', dest='port', type=int, default=7000, help='web server port')
    parser.add_argument('--fork', dest='fork', action='store_true', help='fork process')
    return parser


def maybe_fork(enable_fork: bool) -> None:
    if enable_fork and hasattr(os, 'fork'):
        if os.fork():
            sys.exit(0)


def main() -> None:
    args = create_parser().parse_args()
    maybe_fork(args.fork)

    handler = http.server.SimpleHTTPRequestHandler
    with socketserver.TCPServer(('', args.port), handler) as httpd:
        print('Serving at port', args.port)
        httpd.serve_forever()


if __name__ == '__main__':
    main()
