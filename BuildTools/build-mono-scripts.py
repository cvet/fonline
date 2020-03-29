#!/usr/bin/python

import os
import argparse

parser = argparse.ArgumentParser(description='FOnline scripts generation')
parser.add_argument('-meta', dest='meta', required=True, help='path to script api metadata (ScriptApiMeta.txt)')
parser.add_argument('-content', dest='content', required=True, help='path to content info file (Content.txt)')
parser.add_argument('-foroot', dest='foroot', help='path to fonline engine repository root (detected automatically if not specified)')
parser.add_argument('-output', dest='output', help='output dir (current if not specified)')
args = parser.parse_args()

metaPath = args.meta
contentPath = args.content
foRootPath = (args.foroot if args.foroot else os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))).rstrip('\\/')
outputPath = (args.output if args.output else os.getcwd()).rstrip('\\/')
