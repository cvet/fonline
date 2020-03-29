#!/usr/bin/python

import os
import argparse

parser = argparse.ArgumentParser(description='FOnline scripts generation')
parser.add_argument('-baker', dest='baker', required=True, help='path to baker app')
parser.add_argument('-res', dest='res', action='append', required=True, help='type and path to resource file')
parser.add_argument('-output', dest='output', help='output dir (current if not specified)')
args = parser.parse_args()

outputPath = (args.output if args.output else os.getcwd()).rstrip('\\/')
