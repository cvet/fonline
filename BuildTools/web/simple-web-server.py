#!/usr/bin/python3

import argparse
import os
import sys
import http.server
import socketserver

parser = argparse.ArgumentParser(description='FOnline packager')
parser.add_argument('--port', dest='port', type=int, default=7000, help='web server port')
parser.add_argument('--fork', dest='fork', action='store_true', help='fork process')
args = parser.parse_args()

if args.fork:
    if os.fork():
        sys.exit(0)

handler = http.server.SimpleHTTPRequestHandler

with socketserver.TCPServer(("", args.port), handler) as httpd:
    print("Serving at port", args.port)
    httpd.serve_forever()
