#!/usr/bin/python

import os
import argparse
import uuid
try:
    import mmh3 as mmh3
except ImportError:
    import pymmh3 as mmh3

parser = argparse.ArgumentParser(description='FOnline scripts generation')
parser.add_argument('-meta', dest='meta', required=True, help='path to script api metadata (ScriptApiMeta.txt)')
parser.add_argument('-markdown', dest='markdown', action='store_true', help='generate api in markdown format')
parser.add_argument('-native', dest='native', action='store_true', help='generate native api')
parser.add_argument('-angelscript', dest='angelscript', action='store_true', help='generate angelscript api')
parser.add_argument('-csharp', dest='csharp', action='store_true', help='generate csharp api')
parser.add_argument('-assource', dest='assource', action='append', default=[], help='angelscript file path')
parser.add_argument('-monoassembly', dest='monoassembly', action='append', default=[], help='assembly name')
parser.add_argument('-monocommonref', dest='monocommonref', action='append', default=[], help='mono assembly common reference')
parser.add_argument('-monoserverref', dest='monoserverref', action='append', default=[], help='mono assembly server reference')
parser.add_argument('-monoclientref', dest='monoclientref', action='append', default=[], help='mono assembly client reference')
parser.add_argument('-monomapperref', dest='monomapperref', action='append', default=[], help='mono assembly mapper reference')
parser.add_argument('-monocommonsource', dest='monocommonsource', action='append', default=[], help='csharp common file path')
parser.add_argument('-monoserversource', dest='monoserversource', action='append', default=[], help='csharp server file path')
parser.add_argument('-monoclientsource', dest='monoclientsource', action='append', default=[], help='csharp client file path')
parser.add_argument('-monomappersource', dest='monomappersource', action='append', default=[], help='csharp mapper file path')
parser.add_argument('-content', dest='content', action='append', default=[], help='content file path')
parser.add_argument('-output', dest='output', help='output dir (current if not specified)')
args = parser.parse_args()

outputPath = (args.output if args.output else os.getcwd()).rstrip('\\/')

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

def getGuid(name):
    return '{' + str(uuid.uuid3(uuid.NAMESPACE_OID, name)).upper() + '}'

def getHash(s):
    return str(mmh3.hash(s, seed=0))

# Parse meta information
enums = []
events = { 'common': [], 'server': [], 'client': [], 'mapper': [] }
methods = { 'globalcommon': [], 'globalserver': [], 'globalclient': [], 'globalmapper': [],
        'item': [], 'itemview': [], 'critter': [], 'critterview': [],
        'map': [], 'mapview': [], 'location': [], 'locationview': [] }
properties = { 'global': [], 'item': [], 'critter': [], 'map': [], 'location': [] }
settings = []
doc = None

with open(args.meta, 'r') as f:
    lines = f.readlines()

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
        return [i.split() for i in ' '.join(args).split(',')] if args else []

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
        events[tokens[1]].append((tokens[2], parseArgs(tokens[3:]), getDoc()))
    elif tokens[0] == 'method':
        methods[tokens[1]].append((tokens[2], tokens[3], parseArgs(tokens[4:]), getDoc()))
    elif tokens[0] == 'property':
        properties[tokens[1]].append((tokens[2], tokens[3], tokens[4], tokens[5], tokens[6:], getDoc()))
    elif tokens[0] == 'setting':
        settings.append((tokens[1], tokens[2], tokens[3:], getDoc()))

# Parse content
content = { 'foitem': [], 'focr': [], 'fomap': [], 'foloc': [] }

for file in args.content:
    def getPidNames(file):
        result = [os.path.splitext(os.path.basename(file))[0]]
        with open(file) as f:
            fileLines = f.readlines()
        for fileLine in fileLines:
            if fileLine.startswith('$Name'):
                result.append(fileLine[fileLine.find('=') + 1:].strip(' \r\n'))
        return result

    ext = os.path.splitext(os.path.basename(file))[1]
    content[ext[1:]].extend(getPidNames(file))

# Generate API
files = {}
lastFile = None

def createFile(name):
    global lastFile
    lastFile = []
    files[name] = lastFile

def writeFile(line):
    lastFile.append(line)

def flushFiles():
    if not os.path.isdir(outputPath):
        os.makedirs(outputPath)

    for name, lines in files.items():
        fpath = os.path.join(outputPath, name)
        if os.path.isfile(fpath):
            with open(fpath, 'r') as f:
                if ''.join([l.rstrip('\r\n') for l in f.readlines()]).rstrip() == ''.join(lines).rstrip():
                    continue
        with open(fpath, 'w') as f:
            f.write('\n'.join(lines) + '\n')

# Markdown
if args.markdown:
    createFile('SCRIPT_API.md')
    writeFile('# FOnline Engine Script API')
    writeFile('')
    writeFile('> Document under development, do not rely on this API before the global refactoring complete.  ')
    writeFile('> Estimated finishing date is middle of this year.')
    writeFile('')
    writeFile('## Table of Content')
    writeFile('')
    writeFile('- [General information](#general-information)')
    writeFile('- [Common global methods](#common-global-methods)')
    writeFile('- [Server global methods](#server-global-methods)')
    writeFile('- [Client global methods](#client-global-methods)')
    writeFile('- [Mapper global methods](#mapper-global-methods)')
    writeFile('- [Global properties](#global-properties)')
    writeFile('- [Entities](#entities)')
    writeFile('  * [Item properties](#item-properties)')
    writeFile('  * [Item server methods](#item-server-methods)')
    writeFile('  * [Item client methods](#item-client-methods)')
    writeFile('  * [Critter properties](#critter-properties)')
    writeFile('  * [Critter server methods](#critter-server-methods)')
    writeFile('  * [Critter client methods](#critter-client-methods)')
    writeFile('  * [Map properties](#map-properties)')
    writeFile('  * [Map server methods](#map-server-methods)')
    writeFile('  * [Map client methods](#map-client-methods)')
    writeFile('  * [Location properties](#location-properties)')
    writeFile('  * [Location server methods](#location-server-methods)')
    writeFile('  * [Location client methods](#location-client-methods)')
    writeFile('- [Events](#events)')
    writeFile('  * [Server events](#server-events)')
    writeFile('  * [Client events](#client-events)')
    writeFile('  * [Mapper events](#mapper-events)')
    writeFile('- [Settings](#settings)')
    writeFile('- [Enums](#enums)')
    writeFile('  * [MessageBoxTextType](#messageboxtexttype)')
    writeFile('  * [MouseButton](#mousebutton)')
    writeFile('  * [KeyCode](#keycode)')
    writeFile('  * [CornerType](#cornertype)')
    writeFile('  * [MovingState](#movingstate)')
    writeFile('  * [CritterCondition](#crittercondition)')
    writeFile('  * [ItemOwnership](#itemownership)')
    writeFile('  * [Anim1](#anim1)')
    writeFile('  * [CursorType](#cursortype)')
    writeFile('- [Content](#content)')
    writeFile('  * [Item pids](#item-pids)')
    writeFile('  * [Critter pids](#critter-pids)')
    writeFile('  * [Map pids](#map-pids)')
    writeFile('  * [Location pids](#location-pids)')
    writeFile('')
    writeFile('## General infomation')
    writeFile('')
    writeFile('This document automatically generated from engine provided script API so any change in API will reflect to this document and all scripting layers (C++, C#, AngelScript).  ')
    writeFile('You can easily contribute to this API using provided by engine functionality.  ')
    writeFile('...write about FO_API* macro usage...')
    writeFile('')

    # Generate source
    def parseType(t):
        def mapType(t):
            typeMap = {'char': 'int8', 'uchar': 'uint8', 'short': 'int16', 'ushort': 'uint16',
                    'ItemView': 'Item', 'CritterView': 'Critter', 'MapView': 'Map', 'LocationView': 'Location'}
            return typeMap[t] if t in typeMap else t
        tt = t.split('.')
        if tt[0] == 'dict':
            r = mapType(tt[1]) + '->' + mapType(tt[2])
        elif tt[0] == 'callback':
            r = 'callback-' + mapType(tt[1])
        elif tt[0] == 'predicate':
            r = 'predicate-' + mapType(tt[1])
        else:
            r = mapType(tt[0])
        if 'arr' in tt:
            r += '[]'
        if 'ref' in tt:
            r = 'ref ' + r
        return r
    def parseArgs(args):
        return ', '.join([parseType(a[0]) + ' ' + a[1] for a in args])
    def writeDoc(doc):
        if doc:
            pass
    def writeMethod(tok, entity):
        writeFile('* ' + parseType(tok[1]) + ' ' + tok[0] + '(' + parseArgs(tok[2]) + ')')
        writeDoc(tok[3])
    def writeProp(tok, entity):
        def isPropGet(target, access):
            return True
        def isPropSet(target, access):
            return True
        def evalUsage(srv, cl, mapp):
            if not srv and not cl and not mapp:
                return 'none'
            if srv and cl and mapp:
                return 'all'
            if srv and cl:
                return 'server or client'
            if srv and mapp:
                return 'server or mapper'
            if cl and mapp:
                return 'client or mapper'
            assert False
        rw, name, access, ret, mods, comms = tok
        isServerGet, isServerSet = isPropGet('Server', access), (rw == 'rw' and isPropSet('Server', access))
        isClientGet, isClientSet = isPropGet('Client', access), (rw == 'rw' and isPropSet('Client', access))
        isMapperGet, isMapperSet = isPropGet('Mapper', access), (rw == 'rw' and isPropSet('Mapper', access))
        getUsage = evalUsage(isServerGet, isClientGet, isMapperGet)
        setUsage = evalUsage(isServerSet, isClientSet, isMapperSet)
        writeFile('* ' + parseType(ret) + ' ' + name + ' (get = ' + getUsage + ', set = ' + setUsage + ')')
        writeDoc(comms)

    # Global methods
    writeFile('## Common global methods')
    writeFile('')
    for i in methods['globalcommon']:
        writeMethod(i, None)
    writeFile('')
    writeFile('## Server global methods')
    writeFile('')
    for i in methods['globalserver']:
        writeMethod(i, None)
    writeFile('')
    writeFile('## Client global methods')
    writeFile('')
    for i in methods['globalclient']:
        writeMethod(i, None)
    writeFile('')
    writeFile('## Mapper global methods')
    writeFile('')
    for i in methods['globalmapper']:
        writeMethod(i, None)
    writeFile('')

    # Global properties
    writeFile('## Global properties')
    writeFile('')
    for i in properties['global']:
        writeProp(i, None)
    writeFile('')

    # Entities
    writeFile('## Entities')
    writeFile('')
    for entity in ['Item', 'Critter', 'Map', 'Location']:
        writeFile('### ' + entity + ' properties')
        writeFile('')
        for i in properties[entity.lower()]:
            writeProp(i, entity)
        writeFile('')
        writeFile('### ' + entity + ' server methods')
        writeFile('')
        for i in methods[entity.lower()]:
            writeMethod(i, entity)
        writeFile('')
        writeFile('### ' + entity + ' client methods')
        writeFile('')
        for i in methods[entity.lower() + 'view']:
            writeMethod(i, entity)
        writeFile('')

    # Events
    writeFile('## Events')
    writeFile('')
    for ename in ['server', 'client', 'mapper']:
        writeFile('### ' + ename[0].upper() + ename[1:] + ' events')
        writeFile('')
        for e in events[ename]:
            name, eargs, doc = e
            writeFile('* ' + name + '(' + parseArgs(eargs) + ')')
            writeDoc(doc)
        writeFile('')

    # Settings
    writeFile('## Settings')
    writeFile('')
    for i in settings:
        ret, name, init, doc = i
        writeFile('* ' + parseType(ret) + ' ' + name + ' = ' + init)
        writeDoc(doc)
        writeFile('')

    # Enums
    writeFile('## Enums')
    writeFile('')
    for i in enums:
        group, entries, doc = i
        writeFile('### ' + group)
        writeDoc(doc)
        writeFile('')
        for e in entries:
            name, val, edoc = e
            writeFile('* ' + name + ' = ' + val)
            writeDoc(edoc)
        writeFile('')

    # Content pids
    writeFile('## Content')
    writeFile('')
    def writeEnums(name, lst):
        writeFile('### ' + name + ' pids')
        writeFile('')
        for i in lst:
            writeFile('* ' +  i)
        writeFile('')
    writeEnums('Item', content['foitem'])
    writeEnums('Critter', content['focr'])
    writeEnums('Map', content['fomap'])
    writeEnums('Location', content['foloc'])

# C++ projects
if args.native:
    createFile('FOnline.h')
    writeFile('// FOnline scripting native API')
    writeFile('')
    writeFile('#include <cstdint>')
    writeFile('#include <vector>')
    writeFile('#include <map>')
    writeFile('#include <function>')
    writeFile('')
    writeFile('using hash = uint32_t;')
    writeFile('using uint = uint32_t;')
    writeFile('')

    # Generate source
    def parseType(t):
        def mapType(t):
            typeMap = {'char': 'int8_t', 'uchar': 'uint8_t', 'short': 'int16_t', 'ushort': 'uint16_t',
                    'int64': 'int64_t', 'uint64': 'uint64_t', 'ItemView': 'ScriptItem*',
                    'CritterView': 'ScriptCritter*', 'MapView': 'ScriptMap*', 'LocationView': 'ScriptLocation*',
                    'Item': 'ScriptItem*', 'Critter': 'ScriptCritter*', 'Map': 'ScriptMap*', 'Location': 'ScriptLocation*'}
            return typeMap[t] if t in typeMap else t
        tt = t.split('.')
        if tt[0] == 'dict':
            r = 'std::map<' + mapType(tt[1]) + ', ' + mapType(tt[2]) + '>'
        elif tt[0] == 'callback':
            r = 'std::function<void(' + mapType(tt[1]) + ')>'
        elif tt[0] == 'predicate':
            r = 'std::function<bool(' + mapType(tt[1]) + ')>'
        else:
            r = mapType(tt[0])
        if 'arr' in tt:
            r = 'std::vector<' + r + '>'
        if 'ref' in tt:
            r += '&'
        return r
    def parseArgs(args):
        return ', '.join([parseType(a[0]) + ' ' + a[1] for a in args])
    def writeDoc(ident, doc):
        if doc:
            for d in doc:
                writeFile(''.center(ident) + d)
    def writeMethod(tok, entity):
        writeDoc(4, tok[3])
        writeFile('    ' + parseType(tok[1]) + ' ' + tok[0] + '(' + parseArgs(tok[2]) + ');')
        writeFile('')
    def writeProp(tok, entity):
        def isPropGet(target, access):
            return True
        def isPropSet(target, access):
            return True
        def evalUsage(srv, cl, mapp):
            if not srv and not cl and not mapp:
                return None
            if srv and cl and mapp:
                return ''
            if srv and cl:
                return '#if defined(SERVER) || defined(CLIENT)'
            if srv and mapp:
                return '#if defined(SERVER) || defined(MAPPER)'
            if cl and mapp:
                return '#if defined(CLIENT) || defined(MAPPER)'
            assert False
        rw, name, access, ret, mods, comms = tok
        isServerGet, isServerSet = isPropGet('Server', access), (rw == 'rw' and isPropSet('Server', access))
        isClientGet, isClientSet = isPropGet('Client', access), (rw == 'rw' and isPropSet('Client', access))
        isMapperGet, isMapperSet = isPropGet('Mapper', access), (rw == 'rw' and isPropSet('Mapper', access))
        getUsage = evalUsage(isServerGet, isClientGet, isMapperGet)
        setUsage = evalUsage(isServerSet, isClientSet, isMapperSet)
        if getUsage is not None:
            if getUsage:
                writeFile(getUsage)
            writeDoc(4, comms)
            writeFile('    ' + parseType(ret) + ' Get' + name + '();')
            if getUsage:
                writeFile('#endif')
            writeFile('')
        if setUsage is not None:
            if setUsage:
                writeFile(setUsage)
            writeDoc(4, comms)
            writeFile('    void Set' + name + '(' + parseType(ret) + ' value);')
            if setUsage:
                writeFile('#endif')
            writeFile('')

    # Global
    writeFile('class ScriptGame')
    writeFile('{')
    writeFile('public:')
    for i in properties['global']:
        writeProp(i, None)
    for i in methods['globalcommon']:
        writeMethod(i, None)
    writeFile('#if defined(SERVER)')
    for i in methods['globalserver']:
        writeMethod(i, None)
    writeFile('#elif defined(CLIENT)')
    for i in methods['globalclient']:
        writeMethod(i, None)
    writeFile('#elif defined(MAPPER)')
    for i in methods['globalmapper']:
        writeMethod(i, None)
    writeFile('#endif')
    writeFile('')
    writeFile('private:')
    writeFile('    void* _mainObjPtr;')
    writeFile('};')
    writeFile('')

    # Entities
    writeFile('class ScriptEntity')
    writeFile('{')
    writeFile('protected:')
    writeFile('    void* _mainObjPtr;')
    writeFile('    void* _thisPtr;')
    writeFile('};')
    writeFile('')

    for entity in ['Item', 'Critter', 'Map', 'Location']:
        writeFile('class Script' + entity + ' : public ScriptEntity')
        writeFile('{')
        writeFile('public:')
        for i in properties[entity.lower()]:
            writeProp(i, entity)
        writeFile('')
        writeFile('#if defined(SERVER)')
        for i in methods[entity.lower()]:
            writeMethod(i, entity)
        writeFile('#elif defined(CLIENT)')
        for i in methods[entity.lower() + 'view']:
            writeMethod(i, entity)
        writeFile('#endif')
        writeFile('};')
        writeFile('')

    # Events
    """
    writeFile('## Events')
    writeFile('')
    for ename in ['server', 'client', 'mapper']:
        writeFile('### ' + ename[0].upper() + ename[1:] + ' events')
        writeFile('')
        for e in events[ename]:
            name, eargs, doc = e
            writeFile('* ' + name + '(' + parseArgs(eargs) + ')')
            writeDoc(doc)
        writeFile('')

    # Settings
    writeFile('## Settings')
    writeFile('')
    for i in settings:
        ret, name, init, doc = i
        writeFile('* ' + parseType(ret) + ' ' + name + ' = ' + init)
        writeDoc(doc)
        writeFile('')

    # Enums
    writeFile('## Enums')
    writeFile('')
    for i in enums:
        group, entries, doc = i
        writeFile('### ' + group)
        writeDoc(doc)
        writeFile('')
        for e in entries:
            name, val, edoc = e
            writeFile('* ' + name + ' = ' + val)
            writeDoc(edoc)
        writeFile('')

    # Content pids
    writeFile('## Content')
    writeFile('')
    def writeEnums(name, lst):
        writeFile('### ' + name + ' pids')
        writeFile('')
        for i in lst:
            writeFile('* ' +  i)
        writeFile('')
    writeEnums('Item', content['foitem'])
    writeEnums('Critter', content['focr'])
    writeEnums('Map', content['fomap'])
    writeEnums('Location', content['foloc'])
    """

# AngelScript root file
if args.angelscript:
    # Generate content file
    createFile('Content.fos')
    def writeEnums(name, lst):
        writeFile('enum ' + name)
        writeFile('{')
        for i in lst:
            writeFile('    ' + i + ' = ' + getHash(i) + ',')
        writeFile('}')
        writeFile('')
    writeFile('// FOS Common')
    writeFile('')
    writeEnums('Item', content['foitem'])
    writeEnums('Critter', content['focr'])
    writeEnums('Map', content['fomap'])
    writeEnums('Location', content['foloc'])

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

    for file in args.assource:
        with open(file, 'r') as f:
            line = f.readline()

        if not line.startswith('// FOS'):
            print('Invalid .fos file header', file)
            continue

        targets = line[6:].split()
        sort = int(targets[targets.index('Sort') + 1]) if 'Sort' in targets else 0

        if 'Server' in targets or 'Common' in targets:
            addASSource(asServerFiles, file, sort)
        if 'Client' in targets or 'Common' in targets:
            addASSource(asClientFiles, file, sort)
        if 'Mapper' in targets or 'Common' in targets:
            addASSource(asMapperFiles, file, sort)

    # Write files
    def writeRootModule(target, files):
        def addInclude(file, comment):
            writeFile('namespace ' + os.path.splitext(os.path.basename(file))[0] + ' {')
            writeFile('#include "' + file + ('" // ' + comment if comment else '"'))
            writeFile('}')

        createFile(target + 'RootModule.fos')
        addInclude(outputPath.replace('\\', '/') + '/Content.fos', 'Generated')
        for file in files:
            addInclude(file[1], 'Sort ' + str(file[0]) if file[0] else None)

    writeRootModule('Server', asServerFiles)
    writeRootModule('Client', asClientFiles)
    writeRootModule('Mapper', asMapperFiles)

# C# projects
if args.csharp:
    # Generate source
    def writeHeader(rootClass, usings=None):
        writeFile('using System;')
        writeFile('using System.Collections.Generic;')
        writeFile('using System.Runtime.CompilerServices;')
        writeFile('using hash = System.UInt32;')
        if usings:
            for u in usings:
                writeFile(u)
        writeFile('')
        writeFile('namespace FOnline')
        writeFile('{')
        writeFile('    ' + rootClass)
        writeFile('    {')
    def writeEndHeader():
        writeFile('    }')
        writeFile('}')
    def parseType(t):
        def mapType(t):
            typeMap = {'char': 'sbyte', 'uchar': 'byte', 'int64': 'long', 'uint64': 'ulong',
                    'ItemView': 'Item', 'CritterView': 'Critter', 'MapView': 'Map', 'LocationView': 'Location'}
            return typeMap[t] if t in typeMap else t
        tt = t.split('.')
        if tt[0] == 'dict':
            r = 'Dictionary<' + mapType(tt[1]) + ', ' + mapType(tt[2]) + '>'
        elif tt[0] == 'callback':
            r = 'Action<' + mapType(tt[1]) + '>'
        elif tt[0] == 'predicate':
            r = 'Func<' + mapType(tt[1]) + ', bool>'
        else:
            r = mapType(tt[0])
        if 'arr' in tt:
            r += '[]'
        if 'ref' in tt:
            r = 'ref ' + r
        return r
    def parseArgs(args):
        return ', '.join([parseType(a[0]) + ' ' + a[1] for a in args])
    def parseExtArgs(args, entity):
        r = ['AppDomain domain']
        if entity:
            r.append('IntPtr entityPtr')
        return ', '.join(r + ['out Exception ex'] + [parseType(a[0]) + ' ' + a[1] for a in args])
    def parsePassArgs(args, entity, addDomEx=True):
        r = []
        if addDomEx:
            r.append('AppDomain.CurrentDomain')
        if entity:
            r.append('_entityPtr')
        if addDomEx:
            r.append('out _ex')
        return ', '.join(r + [('ref ' if 'ref' in a[0].split('.') else '') + a[1] for a in args])
    def writeDoc(ident, doc):
        if doc:
            for comm in doc[1:-1]:
                writeFile(''.center(ident) + '/// ' + comm[3:])
    def writeMethod(tok, entity, extCalls):
        writeDoc(8, tok[3])
        writeFile('        public ' + ('static ' if entity is None else '') + parseType(tok[1]) + ' ' + tok[0] + '(' + parseArgs(tok[2]) + ')')
        writeFile('        {')
        writeFile('            Exception _ex;')
        writeFile('            ' + ('var _result = ' if tok[1] != 'void' else '') +
                '_' + tok[0] + '(' + parsePassArgs(tok[2], entity) + ');')
        writeFile('            if (_ex != null)')
        writeFile('                throw _ex;')
        if tok[1] != 'void':
            writeFile('            return _result;')
        writeFile('        }')
        writeFile('')
        extCalls.append('        [MethodImpl(MethodImplOptions.InternalCall)]')
        extCalls.append('        extern static ' + parseType(tok[1]) + ' _' + tok[0] + '(' + parseExtArgs(tok[2], entity) + ');')
    def writeProp(tok, entity, extCalls):
        def isPropGet(target, access):
            return True
        def isPropSet(target, access):
            return True
        def evalDefines(srv, cl, mapp):
            if not srv and not cl and not mapp:
                return None
            if srv and cl and mapp:
                return ''
            if srv and cl:
                return '#if SERVER || CLIENT'
            if srv and mapp:
                return '#if SERVER || MAPPER'
            if cl and mapp:
                return '#if CLIENT || MAPPER'
            assert False
        rw, name, access, ret, mods, comms = tok
        isServerGet, isServerSet = isPropGet('Server', access), (rw == 'rw' and isPropSet('Server', access))
        isClientGet, isClientSet = isPropGet('Client', access), (rw == 'rw' and isPropSet('Client', access))
        isMapperGet, isMapperSet = isPropGet('Mapper', access), (rw == 'rw' and isPropSet('Mapper', access))
        writeDoc(8, comms)
        writeFile('        public ' + ('static ' if entity is None else '') + parseType(ret) + ' ' + name)
        writeFile('        {')
        defines = evalDefines(isServerGet, isClientGet, isMapperGet)
        if defines is not None:
            if defines:
                writeFile(defines)
                extCalls.append(defines)
            writeFile('            get')
            writeFile('            {')
            writeFile('                Exception _ex;')
            if entity:
                writeFile('                var _result = _Get_' + name + '(AppDomain.CurrentDomain, _entityPtr, out _ex);')
            else:
                writeFile('                var _result = _Get_' + name + '(AppDomain.CurrentDomain, out _ex);')
            writeFile('                if (_ex != null)')
            writeFile('                    throw _ex;')
            writeFile('                return _result;')
            writeFile('            }')
            extCalls.append('        [MethodImpl(MethodImplOptions.InternalCall)]')
            if entity:
                extCalls.append('        extern static ' + parseType(ret) + ' _Get_' + name + '(AppDomain domain, IntPtr entityPtr, out Exception ex);')
            else:
                extCalls.append('        extern static ' + parseType(ret) + ' _Get_' + name + '(AppDomain domain, out Exception ex);')
            if defines:
                writeFile('#endif')
                extCalls.append('#endif')
        defines = evalDefines(isServerSet, isClientSet, isMapperSet)
        if defines is not None:
            if defines:
                writeFile(defines)
                ext.append(defines)
            writeFile('            set')
            writeFile('            {')
            writeFile('                Exception _ex;')
            if entity:
                writeFile('                _Set_' + name + '(AppDomain.CurrentDomain, _entityPtr, out _ex, value);')
            else:
                writeFile('                _Set_' + name + '(AppDomain.CurrentDomain, out _ex, value);')
            writeFile('                if (_ex != null)')
            writeFile('                    throw _ex;')
            writeFile('            }')
            extCalls.append('        [MethodImpl(MethodImplOptions.InternalCall)]')
            if entity:
                extCalls.append('        extern static void _Set_' + name + '(AppDomain domain, IntPtr entityPtr, out Exception ex, ' + parseType(ret) + ' value);')
            else:
                extCalls.append('        extern static void _Set_' + name + '(AppDomain domain, out Exception ex, ' + parseType(ret) + ' value);')
            if defines:
                writeFile('#endif')
                extCalls.append('#endif')
        writeFile('        }')
        writeFile('')

    # Global methods
    createFile('Game.cs')
    writeHeader('public static partial class Game')
    extCalls = []
    for i in methods['globalcommon']:
        writeMethod(i, None, extCalls)
    writeFile('#if SERVER')
    extCalls.append('#if SERVER')
    for i in methods['globalserver']:
        writeMethod(i, None, extCalls)
    writeFile('#elif CLIENT')
    extCalls.append('#elif CLIENT')
    for i in methods['globalclient']:
        writeMethod(i, None, extCalls)
    writeFile('#elif MAPPER')
    extCalls.append('#elif MAPPER')
    for i in methods['globalmapper']:
        writeMethod(i, None, extCalls)
    writeFile('#endif')
    extCalls.append('#endif')
    writeFile('')
    for i in extCalls:
        writeFile(i)
    writeEndHeader()

    # Global properties
    createFile('Globals.cs')
    writeHeader('public static partial class Game')
    extCalls = []
    for i in properties['global']:
        writeProp(i, None, extCalls)
    for i in extCalls:
        writeFile(i)
    writeEndHeader()

    # Entities
    for entity in ['Item', 'Critter', 'Map', 'Location']:
        createFile(entity + '.cs')
        writeHeader('public partial class ' + entity + ' : Entity')
        extCalls = []
        for i in properties[entity.lower()]:
            writeProp(i, entity, extCalls)
        writeFile('#if SERVER')
        extCalls.append('#if SERVER')
        for i in methods[entity.lower()]:
            writeMethod(i, entity, extCalls)
        writeFile('#elif CLIENT')
        extCalls.append('#elif CLIENT')
        for i in methods[entity.lower() + 'view']:
            writeMethod(i, entity, extCalls)
        writeFile('#endif')
        extCalls.append('#endif')
        for i in extCalls:
            writeFile(i)
        writeEndHeader()

    # Events
    createFile('Events.cs')
    writeHeader('public static partial class Game', ['using System.Linq;'])
    def writeEvent(tok):
        name, eargs, doc = tok
        writeDoc(8, doc)
        writeFile('        public static event On' + name + 'Delegate On' + name + ';')
        writeFile('        public delegate void On' + name + 'Delegate(' + parseArgs(eargs) + ');')
        writeFile('        public static event On' + name + 'RetDelegate On' + name + 'Ret;')
        writeFile('        public delegate bool On' + name + 'RetDelegate(' + parseArgs(eargs) + ');')
        writeFile('')
    def writeEventExt(tok):
        name, eargs, doc = tok
        pargs = parseArgs(eargs)
        writeFile('        static bool _' + name + '(' + (pargs + ', ' if pargs else '') + 'out Exception[] exs)')
        writeFile('        {')
        writeFile('            exs = null;')
        writeFile('            foreach (var eventDelegate in On' + name + '.GetInvocationList().Cast<On' + name + 'Delegate>())')
        writeFile('            {')
        writeFile('                try')
        writeFile('                {')
        writeFile('                    eventDelegate(' + parsePassArgs(eargs, None, False) + ');')
        writeFile('                }')
        writeFile('                catch (Exception ex)')
        writeFile('                {')
        writeFile('                    exs = exs == null ? new Exception[] { ex } : exs.Concat(new Exception[] { ex }).ToArray();')
        writeFile('                }')
        writeFile('            }')
        writeFile('            foreach (var eventDelegate in On' + name + 'Ret.GetInvocationList().Cast<On' + name + 'RetDelegate>())')
        writeFile('            {')
        writeFile('                try')
        writeFile('                {')
        writeFile('                    if (eventDelegate(' + parsePassArgs(eargs, None, False) + '))')
        writeFile('                        return true;')
        writeFile('                }')
        writeFile('                catch (Exception ex)')
        writeFile('                {')
        writeFile('                    exs = exs == null ? new Exception[] { ex } : exs.Concat(new Exception[] { ex }).ToArray();')
        writeFile('                }')
        writeFile('            }')
        writeFile('            return false;')
        writeFile('        }')
        writeFile('')
    for ename in ['server', 'client', 'mapper']:
        writeFile('#if ' + ename.upper())
        for e in events[ename]:
            writeEvent(e)
        writeFile('#endif')
    for ename in ['server', 'client', 'mapper']:
        writeFile('#if ' + ename.upper())
        for e in events[ename]:
            writeEventExt(e)
        writeFile('#endif')
    writeEndHeader()

    # Settings
    createFile('Settings.cs')
    writeHeader('public static partial class Game')
    writeFile('        public static class Settings')
    writeFile('        {')
    for i in settings:
        ret, name, init, doc = i
        writeDoc(12, doc)
        writeFile('            public static ' + parseType(ret) + ' ' + name)
        writeFile('            {')
        writeFile('                get')
        writeFile('                {')
        writeFile('                    Exception _ex;')
        writeFile('                    var _result = _Setting_' + name + '(AppDomain.CurrentDomain, out _ex);')
        writeFile('                    if (_ex != null)')
        writeFile('                        throw _ex;')
        writeFile('                    return _result;')
        writeFile('                }')
        writeFile('            }')
        writeFile('')
        extCalls.append('        [MethodImpl(MethodImplOptions.InternalCall)]')
        extCalls.append('        extern static ' + parseType(ret) + ' _Setting_' + name + '(AppDomain domain, out Exception ex);')
    writeFile('        }')
    writeEndHeader()

    # Enums
    createFile('Enums.cs')
    writeFile('namespace FOnline')
    writeFile('{')
    for i in enums:
        group, entries, doc = i
        writeDoc(4, doc)
        writeFile('    public enum ' + group)
        writeFile('    {')
        for e in entries:
            name, val, edoc = e
            writeDoc(8, edoc)
            writeFile('        ' + name + ' = ' + val + ',')
        writeFile('    }')
        writeFile('')
    writeFile('}')

    # Content pids
    createFile('Content.cs')
    writeFile('namespace FOnline')
    writeFile('{')
    writeFile('    public static class Content')
    writeFile('    {')
    def writeEnums(name, lst):
        writeFile('        public enum ' + name)
        writeFile('        {')
        for i in lst:
            writeFile('            ' + i + ' = ' + getHash(i) + ',')
        writeFile('        }')
        writeFile('')
    writeEnums('Item', content['foitem'])
    writeEnums('Critter', content['focr'])
    writeEnums('Map', content['fomap'])
    writeEnums('Location', content['foloc'])
    writeFile('    }')
    writeFile('}')

    # Generate .csproj files
    csprojects = []
    for assembly in args.monoassembly:
        for target in ['Server', 'Client', 'Mapper']:
            csprojName = assembly + '.' + target + '.csproj'
            projGuid = getGuid(csprojName)
            csprojects.append((csprojName, projGuid))

            createFile(csprojName)
            writeFile('<?xml version="1.0" encoding="utf-8"?>')
            writeFile('<Project ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">')

            writeFile('  <PropertyGroup>')
            writeFile('    <Configuration Condition=" \'$(Configuration)\' == \'\' ">Debug</Configuration>')
            writeFile('    <Platform Condition=" \'$(Platform)\' == \'\' ">AnyCPU</Platform>')
            writeFile('    <ProjectGuid>' + projGuid + '</ProjectGuid>')
            writeFile('    <OutputType>Library</OutputType>')
            writeFile('    <RootNamespace>' + assembly + '</RootNamespace>')
            writeFile('    <AssemblyName>' + assembly + '.' + target + '</AssemblyName>')
            writeFile('    <TargetFrameworkVersion>v4.5</TargetFrameworkVersion>')
            writeFile('    <FileAlignment>512</FileAlignment>')
            writeFile('    <Deterministic>true</Deterministic>')
            writeFile('  </PropertyGroup>')

            writeFile('  <PropertyGroup Condition=" \'$(Configuration)|$(Platform)\' == \'Debug|AnyCPU\' ">')
            writeFile('    <DebugSymbols>true</DebugSymbols>')
            writeFile('    <DebugType>embedded</DebugType>')
            writeFile('    <Optimize>false</Optimize>')
            writeFile('    <OutputPath>bin\\Debug\\</OutputPath>')
            writeFile('    <DefineConstants>DEBUG;TRACE;' + target.upper() + '</DefineConstants>')
            writeFile('    <ErrorReport>prompt</ErrorReport>')
            writeFile('    <WarningLevel>4</WarningLevel>')
            writeFile('  </PropertyGroup>')

            writeFile('  <PropertyGroup Condition=" \'$(Configuration)|$(Platform)\' == \'Release|AnyCPU\' ">')
            writeFile('    <DebugType>embedded</DebugType>')
            writeFile('    <Optimize>true</Optimize>')
            writeFile('    <OutputPath>bin\\Release\\</OutputPath>')
            writeFile('    <DefineConstants>TRACE;' + target.upper() + '</DefineConstants>')
            writeFile('    <ErrorReport>prompt</ErrorReport>')
            writeFile('    <WarningLevel>4</WarningLevel>')
            writeFile('  </PropertyGroup>')

            writeFile('  <ItemGroup>')
            for src in args.monocommonsource:
                if src.split(',')[0] == assembly:
                    writeFile('    <Compile Include="' + src.split(',')[1].replace('/', '\\') + '" />')
            if target == 'Server':
                for src in args.monoserversource:
                    if src.split(',')[0] == assembly:
                        writeFile('    <Compile Include="' + src.split(',')[1].replace('/', '\\') + '" />')
            if target == 'Client':
                for src in args.monoclientsource:
                    if src.split(',')[0] == assembly:
                        writeFile('    <Compile Include="' + src.split(',')[1].replace('/', '\\') + '" />')
            if target == 'Mapper':
                for src in args.monomappersource:
                    if src.split(',')[0] == assembly:
                        writeFile('    <Compile Include="' + src.split(',')[1].replace('/', '\\') + '" />')
            writeFile('    <Compile Include="Game.cs" />')
            writeFile('    <Compile Include="Globals.cs" />')
            writeFile('    <Compile Include="Item.cs" />')
            writeFile('    <Compile Include="Critter.cs" />')
            writeFile('    <Compile Include="Map.cs" />')
            writeFile('    <Compile Include="Location.cs" />')
            writeFile('    <Compile Include="Events.cs" />')
            writeFile('    <Compile Include="Settings.cs" />')
            writeFile('    <Compile Include="Enums.cs" />')
            writeFile('    <Compile Include="Content.cs" />')
            writeFile('  </ItemGroup>')

            writeFile('  <ItemGroup>')
            def addRef(ref):
                if ref in args.monoassembly:
                    writeFile('    <ProjectReference Include="' + ref + '.' + target + '.csproj">')
                    writeFile('      <Project>' + getGuid(ref + '.' + target + '.csproj') + '</Project>')
                    writeFile('      <Name>' + ref + '.' + target + '</Name>')
                    writeFile('    </ProjectReference>')
                else:
                    writeFile('    <Reference Include="' + ref + '">')
                    writeFile('      <Private>True</Private>')
                    writeFile('    </Reference>')
            for ref in args.monocommonref:
                if ref.split(',')[0] == assembly:
                    addRef(ref.split(',')[1])
            if target == 'Server':
                for ref in args.monoserverref:
                    if ref.split(',')[0] == assembly:
                        addRef(ref.split(',')[1])
            if target == 'Client':
                for ref in args.monoclientref:
                    if ref.split(',')[0] == assembly:
                        addRef(ref.split(',')[1])
            if target == 'Mapper':
                for ref in args.monomapperref:
                    if ref.split(',')[0] == assembly:
                        addRef(ref.split(',')[1])
            writeFile('  </ItemGroup>')

            writeFile('  <Import Project="$(MSBuildToolsPath)\\Microsoft.CSharp.targets" />')
            writeFile('</Project>')

    # Write solution file that contains all project files
    slnName = args.monoassembly[0] + '.sln'
    createFile(slnName)
    writeFile('')
    writeFile('Microsoft Visual Studio Solution File, Format Version 12.00')
    writeFile('# Visual Studio Version 16')
    writeFile('VisualStudioVersion = 16.0.29905.134')
    writeFile('MinimumVisualStudioVersion = 10.0.40219.1')

    for name, guid in csprojects:
        writeFile('Project("{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}") = "' +
                name[:-7] + '", "' + name + '", "' + guid + '"')
        writeFile('EndProject')

    writeFile('Global')
    writeFile('    GlobalSection(SolutionConfigurationPlatforms) = preSolution')
    writeFile('        Debug|Any CPU = Debug|Any CPU')
    writeFile('        Release|Any CPU = Release|Any CPU')
    writeFile('    EndGlobalSection')
    writeFile('    GlobalSection(ProjectConfigurationPlatforms) = postSolution')

    for name, guid in csprojects:
        writeFile('    ' + guid + '.Debug|Any CPU.ActiveCfg = Debug|Any CPU')
        writeFile('    ' + guid + '.Debug|Any CPU.Build.0 = Debug|Any CPU')
        writeFile('    ' + guid + '.Release|Any CPU.ActiveCfg = Release|Any CPU')
        writeFile('    ' + guid + '.Release|Any CPU.Build.0 = Release|Any CPU')

    writeFile('    EndGlobalSection')
    writeFile('    GlobalSection(SolutionProperties) = preSolution')
    writeFile('        HideSolutionNode = FALSE')
    writeFile('    EndGlobalSection')
    writeFile('    GlobalSection(ExtensibilityGlobals) = postSolution')
    writeFile('        SolutionGuid = ' + getGuid(slnName))
    writeFile('    EndGlobalSection')
    writeFile('EndGlobal')
    writeFile('')

# Actual writing of generated files
flushFiles()
