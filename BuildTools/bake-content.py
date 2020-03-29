#!/usr/bin/python

import os
import argparse

parser = argparse.ArgumentParser(description='FOnline scripts generation')
parser.add_argument('-baker', dest='baker', required=True, help='path to baker app')
parser.add_argument('-scripts', dest='scripts', required=True, help='path to scripts location')
parser.add_argument('-file', dest='file', action='append', required=True, help='path to content file')
parser.add_argument('-output', dest='output', help='output dir (current if not specified)')
args = parser.parse_args()

outputPath = (args.output if args.output else os.getcwd()).rstrip('\\/')
