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
parser.add_argument('-multiplayer', dest='multiplayer', action='store_true', help='generate multiplayer api')
parser.add_argument('-singleplayer', dest='singleplayer', action='store_true', help='generate singleplayer api')
parser.add_argument('-mapper', dest='mapper', action='store_true', help='generate mapper api')
parser.add_argument('-markdown', dest='markdown', action='store_true', help='generate api in markdown format')
parser.add_argument('-mdpath', dest='mdpath', default=None, help='path for markdown output')
parser.add_argument('-native', dest='native', action='store_true', help='generate native api')
parser.add_argument('-angelscript', dest='angelscript', action='store_true', help='generate angelscript api')
parser.add_argument('-csharp', dest='csharp', action='store_true', help='generate csharp api')
parser.add_argument('-assource', dest='assource', action='append', default=[], help='angelscript file path')
parser.add_argument('-monoassembly', dest='monoassembly', action='append', default=[], help='assembly name')
parser.add_argument('-monoserverref', dest='monoserverref', action='append', default=[], help='mono assembly server reference')
parser.add_argument('-monoclientref', dest='monoclientref', action='append', default=[], help='mono assembly client reference')
parser.add_argument('-monosingleref', dest='monosingleref', action='append', default=[], help='mono assembly singleplayer reference')
parser.add_argument('-monomapperref', dest='monomapperref', action='append', default=[], help='mono assembly mapper reference')
parser.add_argument('-monoserversource', dest='monoserversource', action='append', default=[], help='csharp server file path')
parser.add_argument('-monoclientsource', dest='monoclientsource', action='append', default=[], help='csharp client file path')
parser.add_argument('-monosinglesource', dest='monosinglesource', action='append', default=[], help='csharp sp file path')
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

# Parse meta information.
doc = None
class MetaInfo:
    def __init__(self, metaName):
        self.enums = []
        self.events = { 'common': [], 'server': [], 'client': [], 'mapper': [] }
        self.methods = { 'globalcommon': [], 'globalserver': [], 'globalclient': [], 'globalmapper': [],
                'item': [], 'itemview': [], 'critter': [], 'critterview': [],
                'map': [], 'mapview': [], 'location': [], 'locationview': [] }
        self.properties = { 'global': [], 'item': [], 'critter': [], 'map': [], 'location': [] }
        self.settings = []

        with open(metaName, 'r') as f:
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
                    self.enums.append((tokens[2], [], getDoc()))
                elif tokens[1] == 'entry':
                    addEntry(self.enums, tokens[2], (tokens[3], tokens[4], getDoc()))
            elif tokens[0] == 'event':
                self.events[tokens[1]].append((tokens[2], parseArgs(tokens[3:]), getDoc()))
            elif tokens[0] == 'method':
                self.methods[tokens[1]].append((tokens[2], tokens[3], parseArgs(tokens[4:]), getDoc()))
            elif tokens[0] == 'property':
                self.properties[tokens[1]].append((tokens[2], tokens[3], tokens[4], tokens[5], tokens[6:], getDoc()))
            elif tokens[0] == 'setting':
                self.settings.append((tokens[1], tokens[2], tokens[3:], getDoc()))

mainMeta = MetaInfo(args.meta)
mpMeta = MetaInfo(args.meta[:-4] + '-MP.txt')
spMeta = MetaInfo(args.meta[:-4] + '-SP.txt')
mpNativeMeta = MetaInfo(args.meta[:-4] + '-MP-Native.txt')
mpAngelScriptMeta = MetaInfo(args.meta[:-4] + '-MP-AngelScript.txt')
mpMonoMeta = MetaInfo(args.meta[:-4] + '-MP-Mono.txt')
spNativeMeta = MetaInfo(args.meta[:-4] + '-SP-Native.txt')
spAngelScriptMeta = MetaInfo(args.meta[:-4] + '-SP-AngelScript.txt')
spMonoMeta = MetaInfo(args.meta[:-4] + '-SP-Mono.txt')

# Parse content
content = { 'foitem': [], 'focr': [], 'fomap': [], 'foloc': [], 'fodlg': [], 'fomsg': [] }

for contentDir in args.content:
    def collectFiles(dir):
        # Todo: recursive search
        result = []
        for file in os.listdir(dir):
            fileParts = os.path.splitext(file)
            if len(fileParts) > 1 and fileParts[1][1:] in content:
                result.append(dir + '/' + file)
        return result

    for file in collectFiles(contentDir):
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
        def writeToDisk(fpath):
            if os.path.isfile(fpath):
                with open(fpath, 'r') as f:
                    if ''.join([l.rstrip('\r\n') for l in f.readlines()]).rstrip() == ''.join(lines).rstrip():
                        return
            with open(fpath, 'w') as f:
                f.write('\n'.join(lines) + '\n')

        writeToDisk(os.path.join(outputPath, name))
        if name[-3:] == '.md' and args.mdpath:
            writeToDisk(os.path.join(args.mdpath, name))

def genApiMarkdown(target):
    globalMeta = mpMeta if target == 'Multiplayer' else (spMeta if target == 'Singleplayer' else mainMeta)
    nativeMeta = mpNativeMeta if target == 'Multiplayer' else (spNativeMeta if target == 'Singleplayer' else mainMeta)
    angelScriptMeta = mpAngelScriptMeta if target == 'Multiplayer' else (spAngelScriptMeta if target == 'Singleplayer' else mainMeta)
    monoMeta = mpMonoMeta if target == 'Multiplayer' else (spMonoMeta if target == 'Singleplayer' else mainMeta)

    createFile(target.upper() + '_SCRIPT_API.md')
    writeFile('# FOnline Engine ' + target + ' Script API')
    writeFile('')
    writeFile('> Document under development, do not rely on this API before the global refactoring complete.  ')
    writeFile('> Estimated finishing date is middle of 2021.')
    writeFile('')
    writeFile('## Table of Content')
    writeFile('')
    writeFile('- [General information](#general-information)')
    if target == 'Multiplayer':
        writeFile('- [Common global methods](#common-global-methods)')
        writeFile('- [Server global methods](#server-global-methods)')
        writeFile('- [Client global methods](#client-global-methods)')
    elif target == 'Singleplayer':
        writeFile('- [Global methods](#global-methods)')
    elif target == 'Mapper':
        writeFile('- [Global methods](#global-methods)')
    writeFile('- [Global properties](#global-properties)')
    writeFile('- [Entities](#entities)')
    writeFile('  * [Item properties](#item-properties)')
    if target == 'Multiplayer':
        writeFile('  * [Item server methods](#item-server-methods)')
        writeFile('  * [Item client methods](#item-client-methods)')
    elif target == 'Singleplayer':
        writeFile('  * [Item methods](#item-methods)')
    writeFile('  * [Critter properties](#critter-properties)')
    if target == 'Multiplayer':
        writeFile('  * [Critter server methods](#critter-server-methods)')
        writeFile('  * [Critter client methods](#critter-client-methods)')
    elif target == 'Singleplayer':
        writeFile('  * [Critter methods](#critter-methods)')
    writeFile('  * [Map properties](#map-properties)')
    if target == 'Multiplayer':
        writeFile('  * [Map server methods](#map-server-methods)')
        writeFile('  * [Map client methods](#map-client-methods)')
    elif target == 'Singleplayer':
        writeFile('  * [Map methods](#map-methods)')
    writeFile('  * [Location properties](#location-properties)')
    if target == 'Multiplayer':
        writeFile('  * [Location server methods](#location-server-methods)')
        writeFile('  * [Location client methods](#location-client-methods)')
    elif target == 'Singleplayer':
        writeFile('  * [Location methods](#location-methods)')
    writeFile('- [Events](#events)')
    if target == 'Multiplayer':
        writeFile('  * [Server events](#server-events)')
        writeFile('  * [Client events](#client-events)')
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
    def writeMethod(tok, entity, usage):
        writeFile('* ' + parseType(tok[1]) + ' ' + tok[0] + '(' + parseArgs(tok[2]) + ')' + usage)
        writeDoc(tok[3])
    def writeProp(tok, entity):
        rw, name, access, ret, mods, comms = tok
        if not (target == 'Mapper' and 'Virtual' in access):
            if target == 'Multiplayer':
                writeFile('* ' + access + ' ' + ('const ' if rw == 'ro' else '') + parseType(ret) + ' ' + name)
            else:
                writeFile('* ' + ('const ' if rw == 'ro' else '')  + parseType(ret) + ' ' + name)
        writeDoc(comms)
    def getUsage(value, section, subsection):
        usage = []
        if value in nativeMeta.__dict__[section][subsection]:
            usage.append('Native')
        if value in angelScriptMeta.__dict__[section][subsection]:
            usage.append('AngelScript')
        if value in monoMeta.__dict__[section][subsection]:
            usage.append('Mono')
        assert usage, usage
        if len(usage) == 3:
            return ''
        return ' *(' + 'and '.join(usage) + ' only)*'

    # Global methods
    if target == 'Multiplayer':
        writeFile('## Common global methods')
        writeFile('')
        for i in globalMeta.methods['globalcommon']:
            writeMethod(i, None, getUsage(i, 'methods', 'globalcommon'))
        writeFile('')
        writeFile('## Server global methods')
        writeFile('')
        for i in globalMeta.methods['globalserver']:
            writeMethod(i, None, getUsage(i, 'methods', 'globalserver'))
        writeFile('')
        writeFile('## Client global methods')
        writeFile('')
        for i in globalMeta.methods['globalclient']:
            writeMethod(i, None, getUsage(i, 'methods', 'globalclient'))
        writeFile('')
    elif target == 'Singleplayer':
        writeFile('## Global methods')
        writeFile('')
        for i in globalMeta.methods['globalcommon']:
            writeMethod(i, None, getUsage(i, 'methods', 'globalcommon'))
        for i in globalMeta.methods['globalserver']:
            writeMethod(i, None, getUsage(i, 'methods', 'globalserver'))
        for i in globalMeta.methods['globalclient']:
            writeMethod(i, None, getUsage(i, 'methods', 'globalclient'))
        writeFile('')
    elif target == 'Mapper':
        writeFile('## Global methods')
        writeFile('')
        for i in globalMeta.methods['globalcommon']:
            writeMethod(i, None, getUsage(i, 'methods', 'globalcommon'))
        for i in globalMeta.methods['globalmapper']:
            writeMethod(i, None, getUsage(i, 'methods', 'globalmapper'))
        writeFile('')

    # Global properties
    writeFile('## Global properties')
    writeFile('')
    for i in globalMeta.properties['global']:
        writeProp(i, None)
    writeFile('')

    # Entities
    writeFile('## Entities')
    writeFile('')
    for entity in ['Item', 'Critter', 'Map', 'Location']:
        writeFile('### ' + entity + ' properties')
        writeFile('')
        for i in globalMeta.properties[entity.lower()]:
            writeProp(i, entity)
        writeFile('')
        if target == 'Multiplayer':
            writeFile('### ' + entity + ' server methods')
            writeFile('')
            for i in globalMeta.methods[entity.lower()]:
                writeMethod(i, entity, getUsage(i, 'methods', entity.lower()))
            writeFile('')
            writeFile('### ' + entity + ' client methods')
            writeFile('')
            for i in globalMeta.methods[entity.lower() + 'view']:
                writeMethod(i, entity, getUsage(i, 'methods', entity.lower() + 'view'))
            writeFile('')
        elif target == 'Singleplayer':
            writeFile('### ' + entity + ' methods')
            writeFile('')
            for i in globalMeta.methods[entity.lower()]:
                writeMethod(i, entity, getUsage(i, 'methods', entity.lower()))
            for i in globalMeta.methods[entity.lower() + 'view']:
                writeMethod(i, entity, getUsage(i, 'methods', entity.lower() + 'view'))
            writeFile('')

    # Events
    writeFile('## Events')
    writeFile('')
    if target == 'Multiplayer':
        writeFile('### Server events')
        writeFile('')
        for e in globalMeta.events['server']:
            name, eargs, doc = e
            writeFile('* ' + name + '(' + parseArgs(eargs) + ')')
            writeDoc(doc)
        writeFile('')
        writeFile('### Client events')
        writeFile('')
        for e in globalMeta.events['client']:
            name, eargs, doc = e
            writeFile('* ' + name + '(' + parseArgs(eargs) + ')')
            writeDoc(doc)
        writeFile('')
    elif target == 'Singleplayer':
        for e in globalMeta.events['server']:
            name, eargs, doc = e
            writeFile('* ' + name + '(' + parseArgs(eargs) + ')')
            writeDoc(doc)
        for e in globalMeta.events['client']:
            name, eargs, doc = e
            writeFile('* ' + name + '(' + parseArgs(eargs) + ')')
            writeDoc(doc)
        writeFile('')
    elif target == 'Mapper':
        for e in globalMeta.events['mapper']:
            name, eargs, doc = e
            writeFile('* ' + name + '(' + parseArgs(eargs) + ')')
            writeDoc(doc)
        writeFile('')

    # Settings
    writeFile('## Settings')
    writeFile('')
    for i in globalMeta.settings:
        ret, name, init, doc = i
        writeFile('* ' + parseType(ret) + ' ' + name)
        writeDoc(doc)
    writeFile('')

    # Enums
    writeFile('## Enums')
    writeFile('')
    for i in globalMeta.enums:
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

if args.markdown:
    if args.multiplayer:
        genApiMarkdown('Multiplayer')
    if args.singleplayer:
        genApiMarkdown('Singleplayer')
    if args.mapper:
        genApiMarkdown('Mapper')

def genApi(target):
    nativeMeta = mpNativeMeta if target in ['Server', 'Client'] else (spNativeMeta if target == 'Single' else mainMeta)
    angelScriptMeta = mpAngelScriptMeta if target in ['Server', 'Client'] else (spAngelScriptMeta if target == 'Single' else mainMeta)
    monoMeta = mpMonoMeta if target in ['Server', 'Client'] else (spMonoMeta if target == 'Single' else mainMeta)

    # C++ projects
    if args.native:
        createFile('FOnline.' + target + '.h')
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
            rw, name, access, ret, mods, comms = tok
            if not (target == 'Mapper' and 'Virtual' in access) and \
                    not (target == 'Server' and 'PrivateClient' in access) and \
                    not (target == 'Client' and 'PrivateServer' in access):
                isGet, isSet = True, rw == 'rw'
                if isGet:
                    writeDoc(4, comms)
                    writeFile('    ' + parseType(ret) + ' Get' + name + '();')
                    writeFile('')
                if isSet:
                    writeDoc(4, comms)
                    writeFile('    void Set' + name + '(' + parseType(ret) + ' value);')
                    writeFile('')

        # Global
        writeFile('class ScriptGame')
        writeFile('{')
        writeFile('public:')
        for i in nativeMeta.properties['global']:
            writeProp(i, None)
        if target == 'Server':
            for i in nativeMeta.methods['globalcommon']:
                writeMethod(i, None)
            for i in nativeMeta.methods['globalserver']:
                writeMethod(i, None)
        elif target == 'Client':
            for i in nativeMeta.methods['globalcommon']:
                writeMethod(i, None)
            for i in nativeMeta.methods['globalclient']:
                writeMethod(i, None)
        elif target == 'Single':
            for i in nativeMeta.methods['globalcommon']:
                writeMethod(i, None)
            for i in nativeMeta.methods['globalserver']:
                writeMethod(i, None)
            for i in nativeMeta.methods['globalclient']:
                writeMethod(i, None)
        elif target == 'Mapper':
            for i in nativeMeta.methods['globalmapper']:
                writeMethod(i, None)
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
            for i in nativeMeta.properties[entity.lower()]:
                writeProp(i, entity)
            writeFile('')
            if target == 'Server' or target == 'Single':
                for i in nativeMeta.methods[entity.lower()]:
                    writeMethod(i, entity)
            if target == 'Client' or target == 'Single':
                for i in nativeMeta.methods[entity.lower() + 'view']:
                    writeMethod(i, entity)
            writeFile('};')
            writeFile('')

        # Events
        """
        writeFile('## Events')
        writeFile('')
        for ename in ['server', 'client', 'mapper']:
            writeFile('### ' + ename[0].upper() + ename[1:] + ' events')
            writeFile('')
            for e in nativeMeta.events[ename]:
                name, eargs, doc = e
                writeFile('* ' + name + '(' + parseArgs(eargs) + ')')
                writeDoc(doc)
            writeFile('')

        # Settings
        writeFile('## Settings')
        writeFile('')
        for i in nativeMeta.settings:
            ret, name, init, doc = i
            writeFile('* ' + parseType(ret) + ' ' + name + ' = ' + init)
            writeDoc(doc)
            writeFile('')

        # Enums
        writeFile('## Enums')
        writeFile('')
        for i in nativeMeta.enums:
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
        asFiles = []

        def addASSource(files, file, sort):
            for i in range(len(files)):
                if sort < files[i][0]:
                    files.insert(i, (sort, file))
                    return
            files.append((sort, file))

        for file in args.assource:
            with open(file, 'r') as f:
                line = f.readline()

            if len(line) >= 3 and ord(line[0]) == 0xEF and ord(line[1]) == 0xBB and ord(line[2]) == 0xBF:
                line = line[3:]

            assert line.startswith('// FOS'), 'Invalid .fos file header: ' + file + ' = ' + line
            fosParams = line[6:].split()
            sort = int(fosParams[fosParams.index('Sort') + 1]) if 'Sort' in fosParams else 0

            if target in fosParams or 'Common' in fosParams:
                addASSource(asFiles, file, sort)

        # Write files
        def writeRootModule(files):
            def addInclude(file, comment):
                writeFile('namespace ' + os.path.splitext(os.path.basename(file))[0] + ' {')
                writeFile('#include "' + file + ('" // ' + comment if comment else '"'))
                writeFile('}')

            createFile(target + 'RootModule.fos')
            addInclude(outputPath.replace('\\', '/') + '/Content.fos', 'Generated')
            for file in files:
                addInclude(file[1], 'Sort ' + str(file[0]) if file[0] else None)

        writeRootModule(asFiles)

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
            rw, name, access, ret, mods, comms = tok
            if not (target == 'Mapper' and 'Virtual' in access) and \
                    not (target == 'Server' and 'PrivateClient' in access) and \
                    not (target == 'Client' and 'PrivateServer' in access):
                isGet, isSet = True, rw == 'rw'
                writeDoc(8, comms)
                writeFile('        public ' + ('static ' if entity is None else '') + parseType(ret) + ' ' + name)
                writeFile('        {')
                if isGet:
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
                if isSet:
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
                writeFile('        }')
                writeFile('')

        # Global methods
        createFile(target + 'Game.cs')
        writeHeader('public static partial class Game')
        extCalls = []
        if target == 'Server':
            for i in monoMeta.methods['globalcommon']:
                writeMethod(i, None, extCalls)
            for i in monoMeta.methods['globalserver']:
                writeMethod(i, None, extCalls)
        elif target == 'Client':
            for i in monoMeta.methods['globalcommon']:
                writeMethod(i, None, extCalls)
            for i in monoMeta.methods['globalclient']:
                writeMethod(i, None, extCalls)
        elif target == 'Single':
            for i in monoMeta.methods['globalcommon']:
                writeMethod(i, None, extCalls)
            for i in monoMeta.methods['globalserver']:
                writeMethod(i, None, extCalls)
            for i in monoMeta.methods['globalclient']:
                writeMethod(i, None, extCalls)
        elif target == 'Mapper':
            for i in monoMeta.methods['globalcommon']:
                writeMethod(i, None, extCalls)
            for i in monoMeta.methods['globalmapper']:
                writeMethod(i, None, extCalls)
        writeFile('')
        for i in extCalls:
            writeFile(i)
        writeEndHeader()

        # Global properties
        createFile(target + 'Globals.cs')
        writeHeader('public static partial class Game')
        extCalls = []
        for i in monoMeta.properties['global']:
            writeProp(i, None, extCalls)
        for i in extCalls:
            writeFile(i)
        writeEndHeader()

        # Entities
        for entity in ['Item', 'Critter', 'Map', 'Location']:
            createFile(target + entity + '.cs')
            writeHeader('public partial class ' + entity + ' : Entity')
            extCalls = []
            if target == 'Server':
                for i in monoMeta.properties[entity.lower()]:
                    writeProp(i, entity, extCalls)
                for i in monoMeta.methods[entity.lower()]:
                    writeMethod(i, entity, extCalls)
            elif target == 'Client':
                for i in monoMeta.properties[entity.lower()]:
                    writeProp(i, entity, extCalls)
                for i in monoMeta.methods[entity.lower() + 'view']:
                    writeMethod(i, entity, extCalls)
            elif target == 'Single':
                for i in monoMeta.properties[entity.lower()]:
                    writeProp(i, entity, extCalls)
                for i in monoMeta.methods[entity.lower()]:
                    writeMethod(i, entity, extCalls)
                for i in monoMeta.methods[entity.lower() + 'view']:
                    writeMethod(i, entity, extCalls)
            elif target == 'Mapper':
                for i in monoMeta.properties[entity.lower()]:
                    writeProp(i, entity, extCalls)
            for i in extCalls:
                writeFile(i)
            writeEndHeader()

        # Events
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
        createFile(target + 'Events.cs')
        writeHeader('public static partial class Game', ['using System.Linq;'])
        if target == 'Server':
            for e in monoMeta.events['server']:
                writeEvent(e)
            for e in monoMeta.events['server']:
                writeEventExt(e)
        elif target == 'Client':
            for e in monoMeta.events['client']:
                writeEvent(e)
            for e in monoMeta.events['client']:
                writeEventExt(e)
        elif target == 'Single':
            for ename in ['server', 'client']:
                for e in monoMeta.events[ename]:
                    writeEvent(e)
            for ename in ['server', 'client']:
                for e in monoMeta.events[ename]:
                    writeEventExt(e)
        elif target == 'Mapper':
            for e in monoMeta.events['mapper']:
                writeEvent(e)
            for e in monoMeta.events['mapper']:
                writeEventExt(e)
        writeEndHeader()

        # Settings
        createFile(target + 'Settings.cs')
        writeHeader('public static partial class Game')
        writeFile('        public static class Settings')
        writeFile('        {')
        for i in monoMeta.settings:
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
        createFile(target + 'Enums.cs')
        writeFile('namespace FOnline')
        writeFile('{')
        for i in monoMeta.enums:
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
        createFile(target + 'Content.cs')
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
        for assembly in args.monoassembly:
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
            if target == 'Server':
                for src in args.monoserversource:
                    if src.split(',')[0] == assembly:
                        writeFile('    <Compile Include="' + src.split(',')[1].replace('/', '\\') + '" />')
            elif target == 'Client':
                for src in args.monoclientsource:
                    if src.split(',')[0] == assembly:
                        writeFile('    <Compile Include="' + src.split(',')[1].replace('/', '\\') + '" />')
            elif target == 'Single':
                for src in args.monosinglesource:
                    if src.split(',')[0] == assembly:
                        writeFile('    <Compile Include="' + src.split(',')[1].replace('/', '\\') + '" />')
            elif target == 'Mapper':
                for src in args.monomappersource:
                    if src.split(',')[0] == assembly:
                        writeFile('    <Compile Include="' + src.split(',')[1].replace('/', '\\') + '" />')
            writeFile('    <Compile Include="' + target + 'Game.cs" />')
            writeFile('    <Compile Include="' + target + 'Globals.cs" />')
            writeFile('    <Compile Include="' + target + 'Item.cs" />')
            writeFile('    <Compile Include="' + target + 'Critter.cs" />')
            writeFile('    <Compile Include="' + target + 'Map.cs" />')
            writeFile('    <Compile Include="' + target + 'Location.cs" />')
            writeFile('    <Compile Include="' + target + 'Events.cs" />')
            writeFile('    <Compile Include="' + target + 'Settings.cs" />')
            writeFile('    <Compile Include="' + target + 'Enums.cs" />')
            writeFile('    <Compile Include="' + target + 'Content.cs" />')
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
            if target == 'Server':
                for ref in args.monoserverref:
                    if ref.split(',')[0] == assembly:
                        addRef(ref.split(',')[1])
            elif target == 'Client':
                for ref in args.monoclientref:
                    if ref.split(',')[0] == assembly:
                        addRef(ref.split(',')[1])
            elif target == 'Single':
                for ref in args.monoclientref:
                    if ref.split(',')[0] == assembly:
                        addRef(ref.split(',')[1])
            elif target == 'Mapper':
                for ref in args.monomapperref:
                    if ref.split(',')[0] == assembly:
                        addRef(ref.split(',')[1])
            writeFile('  </ItemGroup>')

            writeFile('  <Import Project="$(MSBuildToolsPath)\\Microsoft.CSharp.targets" />')
            writeFile('</Project>')

csprojects = []

if args.multiplayer:
    genApi('Server')
    genApi('Client')
if args.singleplayer:
    genApi('Single')
if args.mapper:
    genApi('Mapper')

# Write solution file
if csprojects:
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
