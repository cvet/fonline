#!/usr/bin/python

import os
import argparse
import uuid

parser = argparse.ArgumentParser(description='FOnline script API generation')
parser.add_argument('-meta', dest='meta', required=True, help='path to script api metadata (ScriptApiMeta.txt)')
parser.add_argument('-config', dest='config', required=True, help='path to script config file (ScriptConfig.txt)')
parser.add_argument('-content', dest='content', required=True, help='path to content info file (Content.txt)')
parser.add_argument('-foroot', dest='foroot', help='path to fonline engine repository root (detected automatically if not specified)')
parser.add_argument('-output', dest='output', help='output dir (current if not specified)')
args = parser.parse_args()

metaPath = args.meta
configPath = args.config
contentPath = args.content
foRootPath = (args.foroot if args.foroot else os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))).rstrip('\\/')
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
    try:
        import mmh3 as mmh3
    except ImportError:
        import pymmh3 as mmh3
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

with open(metaPath, 'r') as f:
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

# Parse config
config = { 'Native': [], 'AngelScript': [], 'Mono': [] }

with open(configPath, 'r') as f:
    lines = f.readlines()

for line in lines:
    line = line.rstrip('\r\n')
    tokens = line.split()
    if not tokens:
        continue

    config[tokens[0]].append(tokens[1:])

# Parse content
content = { 'foitem': [], 'focr': [], 'fomap': [], 'foloc': [] }

with open(contentPath, 'r') as f:
    lines = f.readlines()

for line in lines:
    line = line.rstrip('\r\n')
    tokens = line.split()
    if not tokens:
        continue

    assert tokens[0] == 'Proto', tokens

    def getPidNames(file):
        result = [os.path.splitext(os.path.basename(file))[0]]
        with open(file) as f:
            fileLines = f.readlines()
        for fileLine in fileLines:
            if fileLine.startswith('$Name'):
                result.append(fileLine[fileLine.find('=') + 1:].strip(' \r\n'))
        return result

    for file in collectFiles(tokens[1], None):
        ext = os.path.splitext(os.path.basename(file))[1]
        content[ext[1:]].extend(getPidNames(file))

# Generate API
files = {}

def createFile(name):
    files[name] = []

def writeFile(name, line):
    files[name].append(line)

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

# C++ projects
if config['Native']:
    # Todo:
    # createFile('FOnline.h')
    pass

# AngelScript root file
if config['AngelScript']:
    # Generate content file
    createFile('Content.fos')
    def writeEnums(name, lst):
        writeFile('Content.fos', 'enum ' + name)
        writeFile('Content.fos', '{')
        for i in lst:
            writeFile('Content.fos', '    ' + i + ' = ' + getHash(i) + ',')
        writeFile('Content.fos', '}')
        writeFile('Content.fos', '')
    writeFile('Content.fos', '// FOS Common')
    writeFile('Content.fos', '')
    writeEnums('Item', content['foitem'])
    writeEnums('Critter', content['focr'])
    writeEnums('Map', content['fomap'])
    writeEnums('Location', content['foloc'])

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
        fname = target + 'RootModule.fos'

        def addInclude(file, comment):
            writeFile(fname, 'namespace ' + os.path.splitext(os.path.basename(file))[0] + ' {')
            writeFile(fname, '#include "' + file + ('" // ' + comment if comment else ''))
            writeFile(fname, '}')

        createFile(fname)
        addInclude(outputPath.replace('\\', '/') + '/Content.fos', 'Generated')
        for file in files:
            addInclude(file[1], 'Sort ' + str(file[0]) if file[0] else None)

    writeRootModule('Server', asServerFiles)
    writeRootModule('Client', asClientFiles)
    writeRootModule('Mapper', asMapperFiles)

# C# projects
if config['Mono']:
    # Generate source
    createFile('Game.cs')
    writeFile('Game.cs', 'using System;')
    writeFile('Game.cs', 'using System.Collections.Generic;')
    writeFile('Game.cs', 'using System.Runtime.CompilerServices;')
    writeFile('Game.cs', 'using hash = System.UInt32;')
    writeFile('Game.cs', '')
    writeFile('Game.cs', 'namespace FOnline')
    writeFile('Game.cs', '{')
    writeFile('Game.cs', '    public static class Game')
    writeFile('Game.cs', '    {')
    def parseType(t):
        def mapType(t):
            typeMap = {'char': 'sbyte', 'uchar': 'byte', 'int64': 'long', 'uint64': 'ulong',
                    'ItemView': 'Item', 'CritterView': 'Critter', 'MapView': 'Map', 'LocationView': 'Location'}
            return typeMap[t] if t in typeMap else t
        tt = t.split('.')
        if tt[0] == 'dict':
            r = 'Dictionary<' + mapType(tt[1]) + ', ' + mapType(tt[2]) + '>'
        elif tt[0] == 'callback':
            r = 'Action'
        elif tt[0] == 'predicate':
            r = 'Func<' + mapType(tt[1]) + ', bool>'
        else:
            r = mapType(tt[0])
        if 'arr' in tt:
            r += '[]'
        if 'ref' in tt:
            r = 'ref ' + r
        return r
    def parseExtArgs(args):
        return ', '.join(['AppDomain domain', 'out Exception ex'] + [parseType(a[0]) + ' ' + a[1] for a in args])
    def parseArgs(args):
        return ', '.join([parseType(a[0]) + ' ' + a[1] for a in args])
    def parsePassArgs(args):
        return ', '.join(['AppDomain.CurrentDomain', 'out _ex'] +
                [('ref ' if 'ref' in a[0].split('.') else '') + a[1] for a in args])
    def writeGlobalFunc(tok):
        if tok[3]:
            for comm in tok[3][1:-1]:
                writeFile('Game.cs', '        /// ' + comm[3:])
        writeFile('Game.cs', '        public static ' + parseType(tok[1]) + ' ' + tok[0] + '(' + parseArgs(tok[2]) + ')')
        writeFile('Game.cs', '        {')
        writeFile('Game.cs', '            Exception _ex;')
        writeFile('Game.cs', '            ' + ('var _result = ' if tok[1] != 'void' else '') +
                '_' + tok[0] + '(' + parsePassArgs(tok[2]) + ');')
        writeFile('Game.cs', '            if (_ex != null)')
        writeFile('Game.cs', '                throw _ex;')
        if tok[1] != 'void':
            writeFile('Game.cs', '            return _result;')
        writeFile('Game.cs', '        }')
        writeFile('Game.cs', '')
    def writeGlobalExtFunc(tok):
        writeFile('Game.cs', '        [MethodImpl(MethodImplOptions.InternalCall)]')
        writeFile('Game.cs', '        extern static ' + parseType(tok[1]) + ' _' + tok[0] + '(' + parseExtArgs(tok[2]) + ');')
    for i in methods['globalcommon']:
        writeGlobalFunc(i)
    writeFile('Game.cs', '#if SERVER')
    for i in methods['globalserver']:
        writeGlobalFunc(i)
    writeFile('Game.cs', '#elif CLIENT')
    for i in methods['globalclient']:
        writeGlobalFunc(i)
    writeFile('Game.cs', '#elif MAPPER')
    for i in methods['globalmapper']:
        writeGlobalFunc(i)
    writeFile('Game.cs', '#endif')
    writeFile('Game.cs', '')
    for i in methods['globalcommon']:
        writeGlobalExtFunc(i)
    writeFile('Game.cs', '#if SERVER')
    for i in methods['globalserver']:
        writeGlobalExtFunc(i)
    writeFile('Game.cs', '#elif CLIENT')
    for i in methods['globalclient']:
        writeGlobalExtFunc(i)
    writeFile('Game.cs', '#elif MAPPER')
    for i in methods['globalmapper']:
        writeGlobalExtFunc(i)
    writeFile('Game.cs', '#endif')
    writeFile('Game.cs', '    }')
    writeFile('Game.cs', '}')

    # Todo:
    # Global properties
    # Item
    # Critter
    # Map
    # Location
    # Events
    # Settings
    # Enums

    # Generate content file
    createFile('Content.cs')
    writeFile('Content.cs', 'namespace FOnline')
    writeFile('Content.cs', '{')
    writeFile('Content.cs', '    public static class Content')
    writeFile('Content.cs', '    {')
    def writeEnums(name, lst):
        writeFile('Content.cs', '        public enum ' + name)
        writeFile('Content.cs', '        {')
        for i in lst:
            writeFile('Content.cs', '            ' + i + ' = ' + getHash(i) + ',')
        writeFile('Content.cs', '        }')
        writeFile('Content.cs', '')
    writeEnums('Item', content['foitem'])
    writeEnums('Critter', content['focr'])
    writeEnums('Map', content['fomap'])
    writeEnums('Location', content['foloc'])
    writeFile('Content.cs', '    }')
    writeFile('Content.cs', '}')

    # Generate .csproj files
    csprojects = []
    for cmd in config['Mono']:
        if cmd[0] != 'Assembly':
            continue

        for target in ['Server', 'Client', 'Mapper']:
            csprojName = cmd[1] + '.' + target + '.csproj'
            projGuid = getGuid(csprojName)
            csprojects.append((csprojName, projGuid))

            createFile(csprojName)
            writeFile(csprojName, '<?xml version="1.0" encoding="utf-8"?>')
            writeFile(csprojName, '<Project ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">')

            writeFile(csprojName, '  <PropertyGroup>')
            writeFile(csprojName, '    <Configuration Condition=" \'$(Configuration)\' == \'\' ">Debug</Configuration>')
            writeFile(csprojName, '    <Platform Condition=" \'$(Platform)\' == \'\' ">AnyCPU</Platform>')
            writeFile(csprojName, '    <ProjectGuid>' + projGuid + '</ProjectGuid>')
            writeFile(csprojName, '    <OutputType>Library</OutputType>')
            writeFile(csprojName, '    <RootNamespace>' + cmd[1] + '</RootNamespace>')
            writeFile(csprojName, '    <AssemblyName>' + cmd[1] + '.' + target + '</AssemblyName>')
            writeFile(csprojName, '    <TargetFrameworkVersion>v4.5</TargetFrameworkVersion>')
            writeFile(csprojName, '    <FileAlignment>512</FileAlignment>')
            writeFile(csprojName, '    <Deterministic>true</Deterministic>')
            writeFile(csprojName, '  </PropertyGroup>')

            writeFile(csprojName, '  <PropertyGroup Condition=" \'$(Configuration)|$(Platform)\' == \'Debug|AnyCPU\' ">')
            writeFile(csprojName, '    <DebugSymbols>true</DebugSymbols>')
            writeFile(csprojName, '    <DebugType>embedded</DebugType>')
            writeFile(csprojName, '    <Optimize>false</Optimize>')
            writeFile(csprojName, '    <OutputPath>bin\\Debug\\</OutputPath>')
            writeFile(csprojName, '    <DefineConstants>DEBUG;TRACE;' + target.upper() + '</DefineConstants>')
            writeFile(csprojName, '    <ErrorReport>prompt</ErrorReport>')
            writeFile(csprojName, '    <WarningLevel>4</WarningLevel>')
            writeFile(csprojName, '  </PropertyGroup>')

            writeFile(csprojName, '  <PropertyGroup Condition=" \'$(Configuration)|$(Platform)\' == \'Release|AnyCPU\' ">')
            writeFile(csprojName, '    <DebugType>embedded</DebugType>')
            writeFile(csprojName, '    <Optimize>true</Optimize>')
            writeFile(csprojName, '    <OutputPath>bin\\Release\\</OutputPath>')
            writeFile(csprojName, '    <DefineConstants>TRACE;' + target.upper() + '</DefineConstants>')
            writeFile(csprojName, '    <ErrorReport>prompt</ErrorReport>')
            writeFile(csprojName, '    <WarningLevel>4</WarningLevel>')
            writeFile(csprojName, '  </PropertyGroup>')

            writeFile(csprojName, '  <ItemGroup>')
            for src in config['Mono']:
                if src[0] != 'Source' or src[1] != cmd[1]:
                    continue
                if src[2] != 'Common' and src[2] != target:
                    continue
                def csGenerator():
                    return ['using FOnline;', 'using System;', '', 'namespace ' + src[1], '{', '}', '']
                for file in collectFiles(src[3], csGenerator):
                    writeFile(csprojName, '    <Compile Include="' + file.replace('/', '\\') + '" />')
            writeFile(csprojName, '    <Compile Include="Game.cs" />')
            writeFile(csprojName, '    <Compile Include="Content.cs" />')
            writeFile(csprojName, '  </ItemGroup>')

            writeFile(csprojName, '  <ItemGroup>')
            for src in config['Mono']:
                if src[0] != 'Reference' or src[1] != cmd[1]:
                    continue
                if src[2] != 'Common' and src[2] != target:
                    continue
                def isInternalReference(ref):
                    for r in config['Mono']:
                        if r[0] == 'Assembly' and r[1] == ref:
                            return True
                    return False
                if isInternalReference(src[3]):
                    writeFile(csprojName, '    <ProjectReference Include="' + src[3] + '.' + target + '.csproj">')
                    writeFile(csprojName, '      <Project>' + getGuid(src[3] + '.' + target + '.csproj') + '</Project>')
                    writeFile(csprojName, '      <Name>' + src[3] + '.' + target + '</Name>')
                    writeFile(csprojName, '    </ProjectReference>')
                else:
                    writeFile(csprojName, '    <Reference Include="' + src[3] + '" />')
            writeFile(csprojName, '  </ItemGroup>')

            writeFile(csprojName, '  <Import Project="$(MSBuildToolsPath)\\Microsoft.CSharp.targets" />')
            writeFile(csprojName, '</Project>')

    # Write solution file that contains all project files
    for cmd in config['Mono']:
        if cmd[0] == 'Assembly':
            slnName = cmd[1] + '.sln'
            break

    createFile(slnName)
    writeFile(slnName, '')
    writeFile(slnName, 'Microsoft Visual Studio Solution File, Format Version 12.00')
    writeFile(slnName, '# Visual Studio Version 16')
    writeFile(slnName, 'VisualStudioVersion = 16.0.29905.134')
    writeFile(slnName, 'MinimumVisualStudioVersion = 10.0.40219.1')

    for name, guid in csprojects:
        writeFile(slnName, 'Project("{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}") = "' +
                name[:-7] + '", "' + name + '", "' + guid + '"')
        writeFile(slnName, 'EndProject')

    writeFile(slnName, 'Global')
    writeFile(slnName, '    GlobalSection(SolutionConfigurationPlatforms) = preSolution')
    writeFile(slnName, '        Debug|Any CPU = Debug|Any CPU')
    writeFile(slnName, '        Release|Any CPU = Release|Any CPU')
    writeFile(slnName, '    EndGlobalSection')
    writeFile(slnName, '    GlobalSection(ProjectConfigurationPlatforms) = postSolution')

    for name, guid in csprojects:
        writeFile(slnName, '    ' + guid + '.Debug|Any CPU.ActiveCfg = Debug|Any CPU')
        writeFile(slnName, '    ' + guid + '.Debug|Any CPU.Build.0 = Debug|Any CPU')
        writeFile(slnName, '    ' + guid + '.Release|Any CPU.ActiveCfg = Release|Any CPU')
        writeFile(slnName, '    ' + guid + '.Release|Any CPU.Build.0 = Release|Any CPU')

    writeFile(slnName, '    EndGlobalSection')
    writeFile(slnName, '    GlobalSection(SolutionProperties) = preSolution')
    writeFile(slnName, '        HideSolutionNode = FALSE')
    writeFile(slnName, '    EndGlobalSection')
    writeFile(slnName, '    GlobalSection(ExtensibilityGlobals) = postSolution')
    writeFile(slnName, '        SolutionGuid = ' + getGuid(slnName))
    writeFile(slnName, '    EndGlobalSection')
    writeFile(slnName, 'EndGlobal')
    writeFile(slnName, '')

# Actual writing of generated files
flushFiles()
