#!/usr/bin/python

import os
import argparse

parser = argparse.ArgumentParser(description='FOnline script API generation')
parser.add_argument('-meta', dest='meta', required=True, help='path to api metadata (ScriptApiMeta.txt)')
parser.add_argument('-config', dest='config', required=True, help='path to config file (ScriptConfig.txt)')
parser.add_argument('-foroot', dest='foroot', help='path to fonline engine repository root (detected automatically if not specified)')
parser.add_argument('-output', dest='output', help='output dir (current if not specified)')
args = parser.parse_args()

metaPath = args.meta
configPath = args.config
foRootPath = args.foroot if args.foroot else os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
outputPath = args.output if args.output else os.getcwd()

# Parse meta information
with open(metaPath, 'r') as f:
    lines = f.readlines()

enums = []
events = [('common', []), ('server', []), ('client', []), ('mapper', [])]
methods = [('global-common', []), ('global-server', []), ('global-client', []), ('global-mapper', []),
        ('item', []), ('item-view', []), ('critter', []), ('critter-view', []),
        ('map', []), ('map-view', []), ('location', []), ('location-view', [])]
properties = [('global', []), ('item', []), ('critter', []), ('map', []), ('location', [])]
settings = []
doc = None

for line in lines:
    def getDoc():
        global doc
        ret = doc
        doc = None
        return ret

    def findInList(lst, predicate):
        index = 0
        for l in lst:
            if predicate(l):
                return index
            index += 1
        return -1

    def addEntry(lst, name, entry):
        index = findInList(lst, lambda x: x[0] == name)
        assert index != -1, name
        lst[index][1].append(entry)

    def parseArgs(args):
        return [i.split() for i in ' '.join(args).split(',')]

    line = line.rstrip('\r\n')
    tokens = line.split()
    if not tokens:
        continue

    if tokens[0].startswith('/**'):
        doc = [tokens[0]]
    elif line.startswith(' *'):
        doc.append(line)
    elif tokens[0] == 'enum':
        if tokens[1] == 'group':
            enums.append((tokens[2], [], getDoc()))
        elif tokens[1] == 'entry':
            addEntry(enums, tokens[2], (tokens[3], tokens[4], getDoc()))
    elif tokens[0] == 'event':
        addEntry(events, tokens[1], (tokens[2], parseArgs(tokens[3:]), getDoc()))
    elif tokens[0] == 'method':
        addEntry(methods, tokens[1], (tokens[2], tokens[3], parseArgs(tokens[4:]), getDoc()))
    elif tokens[0] == 'property':
        addEntry(properties, tokens[1], (tokens[2], tokens[3], tokens[4], tokens[5], tokens[6:], getDoc()))
    elif tokens[0] == 'setting':
        settings.append((tokens[1], tokens[2], tokens[3:], getDoc()))

# Parse config
with open(configPath, 'r') as f:
    lines = f.readlines()

config = { 'Native': [], 'AngelScript': [], 'Mono': [] }

for line in lines:
    line = line.rstrip('\r\n')
    tokens = line.split()
    if not tokens:
        continue

    config[tokens[0]].append(tokens[1:])

# Generate API
files = {}
def createFile(name):
    global files
    files[name] = []

def writeFile(name, ident, line):
    files[name].append(''.center(ident) + line)

def flushFiles():
    if not os.path.isdir(outputPath):
        os.makedirs(outputPath)

    for name, lines in files.items():
        fpath = os.path.join(outputPath, name)
        if os.path.isfile(fpath):
            with open(fpath, 'r') as f:
                if [l.rstrip('\r\n') for l in f.readlines()] == lines:
                    continue
        with open(fpath, 'w') as f:
            f.write('\n'.join(lines))

def collectFiles(pathPattern, generator):
    # Todo: recursive search
    result = []
    if '*' in pathPattern:
        dir = os.path.dirname(pathPattern)
        basename = os.path.basename(pathPattern)
        name = os.path.splitext(basename)[0]
        ext = os.path.splitext(basename)[1] if len(os.path.splitext(basename)) > 1 else None
        for file in os.listdir(dir):
            n, e = os.path.splitext(file)
            if ('*' in name or n == name) and ('*' in ext or e == ext):
                result.append(dir + '/' + file)
    else:
        if not os.path.isfile(pathPattern):
            lines = generator()
            with open(pathPattern, 'w') as f:
                f.write('\n'.join(lines))
        result.append(pathPattern)
    return result

# C++ projects
# Todo:

# AngelScript root file
if config['AngelScript']:
    # Collect files
    asFiles = []
    for entry in config['AngelScript']:
        assert entry[0] == 'Source', entry
        def fosGenerator():
            return ['// FOS Common', '']
        for file in collectFiles(entry[1], fosGenerator):
            asFiles.append(file)

    # Sort files
    asServerFiles = []
    asClientFiles = []
    asMapperFiles = []

    def addASSource(files, file, sort):
        for i in range(len(files)):
            if sort < files[i][0]:
                files.insert(i, (sort, file))
                return
        files.append((sort, file))

    for file in asFiles:
        with open(file, 'r') as f:
            line = f.readline()

        if not line.startswith('// FOS'):
            print('Invalid .fos file header', file)
            continue

        types = line[6:].split()
        sort = int(types[types.index('Sort') + 1]) if 'Sort' in types else 0

        if 'Server' in types or 'Common' in types:
            addASSource(asServerFiles, file, sort)
        if 'Client' in types or 'Common' in types:
            addASSource(asClientFiles, file, sort)
        if 'Mapper' in types or 'Common' in types:
            addASSource(asMapperFiles, file, sort)

    # Write files
    def writeRootModule(type, files):
        fname = type + '-root-module.fos'
        createFile(fname)
        for file in files:
            writeFile(fname, 0, 'namespace ' + os.path.splitext(os.path.basename(file[1]))[0] + ' {')
            writeFile(fname, 0, '#include "' + file[1] + '" // Sort ' + str(file[0]) )
            writeFile(fname, 0, '}')

    writeRootModule('server', asServerFiles)
    writeRootModule('client', asClientFiles)
    writeRootModule('mapper', asMapperFiles)

# C# projects
# Todo:
"""
createFile('FOnline.cs')
# generate .csproj files from .bake files
# createFile('FOnline.csproj')

# Native API
createFile('FOnline.h')
# generate .cpp files from .bake
"""

# Actual writing of generated files
flushFiles()
