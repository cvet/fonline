#!/usr/bin/python3

print('[CodeGen]', 'Start code generation')

import os
import sys
import argparse
import time
import uuid
import subprocess
import hashlib
from collections.abc import Mapping, Iterable

startTime = time.time()

parser = argparse.ArgumentParser(description='FOnline code generator', fromfile_prefix_chars='@')
parser.add_argument('-maincfg', dest='maincfg', required=True, help='main config file')
parser.add_argument('-buildhash', dest='buildhash', required=True, help='build hash')
parser.add_argument('-devname', dest='devname', required=True, help='dev game name')
parser.add_argument('-nicename', dest='nicename', required=True, help='nice game name')
parser.add_argument('-embedded', dest='embedded', required=True, help='embedded buffer capacity')
parser.add_argument('-meta', dest='meta', required=True, action='append', help='path to script api metadata (///@ tags)')
parser.add_argument('-commonheader', dest='commonheader', action='append', default=[], help='path to common header file')
parser.add_argument('-markdown', dest='markdown', action='store_true', default=None, help='generate api in markdown format')
parser.add_argument('-mdpath', dest='mdpath', default=None, help='path for markdown output')
parser.add_argument('-native', dest='native', action='store_true', default=None, help='generate native api')
parser.add_argument('-angelscript', dest='angelscript', action='store_true', default=None, help='generate angelscript api')
parser.add_argument('-csharp', dest='csharp', action='store_true', default=None, help='generate csharp api')
parser.add_argument('-monoassembly', dest='monoassembly', action='append', default=[], help='assembly name')
parser.add_argument('-monoserverref', dest='monoserverref', action='append', default=[], help='mono assembly server reference')
parser.add_argument('-monoclientref', dest='monoclientref', action='append', default=[], help='mono assembly client reference')
parser.add_argument('-monomapperref', dest='monomapperref', action='append', default=[], help='mono assembly mapper reference')
parser.add_argument('-monoserversource', dest='monoserversource', action='append', default=[], help='csharp server file path')
parser.add_argument('-monoclientsource', dest='monoclientsource', action='append', default=[], help='csharp client file path')
parser.add_argument('-monomappersource', dest='monomappersource', action='append', default=[], help='csharp mapper file path')
parser.add_argument('-genoutput', dest='genoutput', required=True, help='generated code output dir')
parser.add_argument('-verbose', dest='verbose', action='store_true', help='verbose mode')
args = parser.parse_args()

def getGuid(name):
    return '{' + str(uuid.uuid3(uuid.NAMESPACE_OID, name)).upper() + '}'

def getHash(input, seed=0):
    input = input.encode()

    def intTo4Bytes(i):
        return i & 0xffffffff

    m = 0x5bd1e995
    r = 24

    length = len(input)
    h = seed ^ length

    round = 0
    while length >= (round * 4) + 4:
        k = input[round * 4]
        k |= input[(round * 4) + 1] << 8
        k |= input[(round * 4) + 2] << 16
        k |= input[(round * 4) + 3] << 24
        k = intTo4Bytes(k)
        k = intTo4Bytes(k * m)
        k ^= k >> r
        k = intTo4Bytes(k * m)
        h = intTo4Bytes(h * m)
        h ^= k
        round += 1

    lengthDiff = length - (round * 4)

    if lengthDiff == 1:
        h ^= input[-1]
        h = intTo4Bytes(h * m)
    elif lengthDiff == 2:
        h ^= intTo4Bytes(input[-1] << 8)
        h ^= input[-2]
        h = intTo4Bytes(h * m)
    elif lengthDiff == 3:
        h ^= intTo4Bytes(input[-1] << 16)
        h ^= intTo4Bytes(input[-2] << 8)
        h ^= input[-3]
        h = intTo4Bytes(h * m)

    h ^= h >> 13
    h = intTo4Bytes(h * m)
    h ^= h >> 15

    assert h <= 0xffffffff
    if h & 0x80000000 == 0:
        return str(h)
    else:
        return str(-((h ^ 0xFFFFFFFF) + 1))

assert getHash('abcd') == '646393889'
assert getHash('abcde') == '1594468574'
assert getHash('abcdef') == '1271458169'
assert getHash('abcdefg') == '-106836237'

# Generated file list
genFileList = ['EmbeddedResources-Include.h',
        'Version-Include.h',
        'GenericCode-Common.cpp',
        'MetadataRegistration-Server.cpp',
        'MetadataRegistration-Client.cpp',
        'MetadataRegistration-Mapper.cpp',
        'MetadataRegistration-ServerStub.cpp',
        'MetadataRegistration-ClientStub.cpp',
        'MetadataRegistration-MapperStub.cpp']

# Parse meta information
codeGenTags = {
        'ExportEnum': [], # (group name, underlying type, [(key, value, [comment])], [flags], [comment])
        'ExportValueType': [], # (name, native type, [flags], [comment])
        'ExportProperty': [], # (entity, access, type, name, [flags], [comment])
        'ExportMethod': [], # (target, entity, name, ret, [(type, name)], [flags], [comment])
        'ExportEvent': [], # (target, entity, name, [(type, name)], [flags], [comment])
        'ExportRefType': [], # (target, name, [(type, name, [comment])], [(name, ret, [(type, name)], [comment])], [flags], [comment])
        'ExportEntity': [], # (name, serverClassName, clientClassName, [flags], [comment])
        'ExportSettings': [], #(group name, target, [(fixOrVar, keyType, keyName, [initValues], [comment])], [flags], [comment])
        'EngineHook': [], #(name, [flags], [comment])
        'MigrationRule': [], #([args], [comment])
        'CodeGen': [] } # (templateType, absPath, entry, line, padding, [flags], [comment])

def verbosePrint(*str):
    if args.verbose:
        print('[CodeGen]', *str)

errors = []

def showError(*messages):
    global errors
    errors.append(messages)
    print('[CodeGen]', str(messages[0]))
    for m in messages[1:]:
        print('[CodeGen]', '-', str(m))

def checkErrors():
    if errors:
        try:
            errorLines = []
            errorLines.append('')
            errorLines.append('#error Code generation failed')
            errorLines.append('')
            errorLines.append('//  Stub generated due to code generation error')
            errorLines.append('//')
            for messages in errors:
                errorLines.append('//  ' + messages[0])
                for m in messages:
                    errorLines.append('//  - ' + str(m))
            errorLines.append('//')
            
            def writeStub(fname):
                with open(args.genoutput.rstrip('/') + '/' + fname, 'w', encoding='utf-8') as f:
                    f.write('\n'.join(errorLines))
            
            for fname in genFileList:
                writeStub(fname)
            
        except Exception as ex:
            showError('Can\'t write stubs', ex)
        
        for e in errors[0]:
            if isinstance(e, BaseException):
                print('[CodeGen]', 'Most recent exception:')
                raise e
        
        sys.exit(1)

# Parse tags
tagsMetas = {}
for k in codeGenTags.keys():
    tagsMetas[k] = []

def parseMetaFile(absPath):
    global tagsMetas
    
    try:
        with open(absPath, 'r', encoding='utf-8-sig') as f:
            lines = f.readlines()
    except Exception as ex:
        showError('Can\'t read file', absPath, ex)
        return
        
    lastComment = []
    
    for lineIndex in range(len(lines)):
        line = lines[lineIndex]
        lineLen = len(line)
        
        if lineLen < 5:
            continue
        
        try:
            tagPos = -1
            for startPos in range(lineLen):
                if line[startPos] != ' ' and line[startPos] != '\t':
                    tagPos = startPos
                    break
            if tagPos == -1 or lineLen - tagPos <= 2 or line[tagPos] != '/' or line[tagPos + 1] != '/':
                lastComment = []
                continue
            
            if lineLen - tagPos >= 4 and line[tagPos + 2] == '/' and line[tagPos + 3] == '@':
                tagStr = line[tagPos + 4:].strip()
                
                commentPos = tagStr.find('//')
                if commentPos != -1:
                    lastComment = [tagStr[commentPos + 2:].strip()]
                    tagStr = tagStr[:commentPos].rstrip()
                
                comment = lastComment if lastComment else []
                
                tagSplit = tagStr.split(' ', 1)
                tagName = tagSplit[0]
                
                if tagName not in tagsMetas:
                    showError('Invalid tag ' + tagName, absPath + ' (' + str(lineIndex + 1) + ')', line.strip())
                    continue
                
                tagInfo = tagSplit[1] if len(tagSplit) > 1 else None
                
                tagContext = None
                if tagName.startswith('Export'):
                    if tagName == 'ExportEnum':
                        for i in range(lineIndex + 1, len(lines)):
                            if lines[i].lstrip().startswith('};'):
                                tagContext = [l.strip() for l in lines[lineIndex + 1 : i] if l.strip()]
                                break
                    elif tagName == 'ExportValueType':
                        tagContext = True
                    elif tagName == 'ExportProperty':
                        tagContext = lines[lineIndex + 1].strip()
                        for i in range(lineIndex, 0, -1):
                            if lines[i].startswith('class '):
                                propPos = lines[i].find('Properties')
                                assert propPos != -1
                                tagContext = lines[i][6:propPos] + ' ' + lines[lineIndex + 1].strip()
                                break
                    elif tagName == 'ExportMethod':
                        tagContext = lines[lineIndex + 1].strip()
                    elif tagName == 'ExportEvent':
                        for i in range(lineIndex, 0, -1):
                            if lines[i].startswith('class '):
                                className = lines[i][6:lines[i].find(' ', 6)]
                                tagContext = className + ' ' + lines[lineIndex + 1].strip()
                                break
                    elif tagName == 'ExportRefType':
                        for i in range(lineIndex + 1, len(lines)):
                            if lines[i].startswith('};'):
                                tagContext = [l.strip() for l in lines[lineIndex + 1 : i] if l.strip()]
                                break
                    elif tagName == 'ExportEntity':
                        tagContext = True
                    elif tagName == 'ExportSettings':
                        for i in range(lineIndex + 1, len(lines)):
                            if lines[i].startswith('SETTING_GROUP_END'):
                                tagContext = [l.strip() for l in lines[lineIndex + 1 : i] if l.strip()]
                                break
                    assert 'Invalid tag context', absPath
                elif tagName == 'EngineHook':
                    tagContext = lines[lineIndex + 1].strip()
                elif tagName == 'CodeGen':
                    tagContext = tagPos
                
                tagsMetas[tagName].append((absPath, lineIndex, tagInfo, tagContext, comment))
                lastComment = []
                
            elif lineLen - tagPos >= 3 and line[tagPos + 2] != '/':
                lastComment.append(line[tagPos + 2:].strip())
            else:
                lastComment = []
                
        except Exception as ex:
            showError('Invalid tag format', absPath + ' (' + str(lineIndex + 1) + ')', line.strip(), ex)

# Meta files from build system
metaFiles = []

for path in args.meta:
    absPath = os.path.abspath(path)
    if absPath not in metaFiles and 'GeneratedSource' not in absPath:
        assert os.path.isfile(absPath), 'Invalid meta file path ' + path
        metaFiles.append(absPath)
metaFiles.sort()

for absPath in metaFiles:
    parseMetaFile(absPath)

checkErrors()

refTypes = set()
engineEnums = set()
customTypes = set()
customTypesNativeMap = {}
gameEntities = []
gameEntitiesInfo = {}
entityRelatives = set()
genericFuncdefs = set()
propertyComponents = set()

symTok = set('`~!@#$%^&*()+-=|\\/.,\';][]}{:><"')
def tokenize(text, anySymbols=[]):
    if text is None:
        return []
    text = text.strip()
    result = []
    curTok = ''
    def flushTok():
        if curTok.strip():
            result.append(curTok.strip())
        return ''
    for ch in text:
        if ch in symTok:
            if len(result) in anySymbols:
                curTok += ch
                continue
            curTok = flushTok()
            curTok = ch
            curTok = flushTok()
        elif ch in ' \t':
            curTok = flushTok()
        else:
            curTok += ch
    curTok = flushTok()
    return result

def parseTags():
    validTypes = set()
    validTypes.update(['int8', 'uint8', 'int16', 'uint16', 'int32', 'uint32', 'int64', 'uint64',
            'float32', 'float64', 'string', 'bool', 'Entity', 'void', 'hstring', 'any'])
    
    def unifiedTypeToMetaType(t):
        if t.startswith('callback('):
            assert t[-1] == ')'
            name = 'callback'
            fd = [unifiedTypeToMetaType(a) for a in t[t.find('(')+1:t.find(')')].split(',')]
            genericFuncdefs.add('|'.join(fd))
            return name + '.' + '|'.join(fd) + '|'
        if t.endswith('&'):
            return unifiedTypeToMetaType(t[:-1]) + '.ref'
        if '=>' in t:
            tt = t.split('=>')
            return 'dict.' + unifiedTypeToMetaType(tt[0]) + '.' + unifiedTypeToMetaType(tt[1])
        if t.endswith('[]'):
            return 'arr.' + unifiedTypeToMetaType(t[:-2])
        if t == 'SELF_ENTITY':
            return 'SELF_ENTITY'
        assert t in validTypes, 'Invalid type ' + t
        return t

    def engineTypeToUnifiedType(t):
        typeMap = {'int8': 'int8', 'uint8': 'uint8', 'int16': 'int16', 'uint16': 'uint16',
            'int32': 'int32', 'uint32': 'uint32', 'int64': 'int64', 'uint64': 'uint64',
            'float32': 'float32', 'float64': 'float64', 'bool': 'bool', 'void': 'void',
            'string_view': 'string', 'string': 'string',
            'hstring': 'hstring', 'any_t': 'any'}
        if t.startswith('ScriptFunc<'):
            fargs = splitEngineArgs(t[t.find('<') + 1:t.rfind('>')])
            r = 'callback(' + ','.join([engineTypeToUnifiedType(a.strip()) for a in fargs]) + ')'
            return r
        elif t.startswith('readonly_vector<'):
            r = engineTypeToUnifiedType(t[t.find('<') + 1:t.rfind('>')]) + '[]'
            assert not t.endswith('&')
            return r
        elif t.startswith('readonly_map<') :
            tt = t[t.find('<') + 1:t.rfind('>')].split(',', 1)
            r = engineTypeToUnifiedType(tt[0].strip()) + '=>' + engineTypeToUnifiedType(tt[1].strip())
            assert not t.endswith('&')
            return r
        elif t.startswith('vector<'):
            r = engineTypeToUnifiedType(t[t.find('<') + 1:t.rfind('>')]) + '[]'
            if t.endswith('&'):
                r += '&'
            return r
        elif t.startswith('map<'):
            tt = t[t.find('<') + 1:t.rfind('>')].split(',', 1)
            r = engineTypeToUnifiedType(tt[0].strip()) + '=>' + engineTypeToUnifiedType(tt[1].strip())
            if t.endswith('&'):
                r += '&'
            return r
        elif t[-1] in ['&', '*'] and t[:-1] in customTypesNativeMap:
            return customTypesNativeMap[t[:-1]] + t[-1]
        elif t in customTypesNativeMap:
            return customTypesNativeMap[t]
        elif t in typeMap:
            return typeMap[t]
        elif t[-1] == '&' and t[:-1] in typeMap:
            return typeMap[t[:-1]] + '&'
        elif t[-1] == '*' and t[:-1] in typeMap:
            return typeMap[t[:-1]] + '*'
        elif t in validTypes:
            return t
        elif t[-1] == '&' and t[:-1] in validTypes:
            return t
        elif t[-1] == '*' and t not in typeMap:
            tt = t[:-1]
            if tt in refTypes or tt in entityRelatives:
                return tt
            for ename, einfo in gameEntitiesInfo.items():
                if tt == einfo['Server'] or tt == einfo['Client']:
                    return ename
            if tt in ['ServerEntity', 'ClientEntity', 'Entity']:
                return 'Entity'
            if tt == 'ScriptSelfEntity':
                return 'SELF_ENTITY'
            assert False, tt
        else:
            assert False, 'Invalid engine type ' + t

    def engineTypeToMetaType(t):
        ut = engineTypeToUnifiedType(t)
        return unifiedTypeToMetaType(ut)

    def splitEngineArgs(fargs):
        result = []
        braces = 0
        r = ''
        for c in fargs:
            if c == ',' and braces == 0:
                result.append(r.strip())
                r = ''
            else:
                r += c
                if c == '<':
                    braces += 1
                elif c == '>':
                    braces -= 1
        if r.strip():
            result.append(r.strip())
        return result

    def parseTypeTags1():
        global codeGenTags
        
        for tagMeta in tagsMetas['ExportEnum']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                exportFlags = tokenize(tagInfo)
                
                firstLine = tagContext[0]
                assert firstLine.startswith('enum class ')
                sep = firstLine.find(':')
                grname = firstLine[len('enum class '):sep if sep != -1 else len(firstLine)].strip()
                utype = engineTypeToMetaType('int32' if sep == -1 else firstLine[sep + 1:].strip())
                
                keyValues = []
                assert tagContext[1].startswith('{')
                for l in tagContext[2:]:
                    comm = l.find('//')
                    if comm != -1:
                        l = l[:comm].rstrip()
                    sep = l.find('=')
                    if sep == -1:
                        keyValues.append((l.rstrip(','), str(int(keyValues[-1][1], 0) + 1) if len(keyValues) else '0', []))
                    else:
                        keyValues.append((l[:sep].rstrip(), l[sep + 1:].lstrip().rstrip(','), []))
                
                codeGenTags['ExportEnum'].append((grname, utype, keyValues, exportFlags, comment))
                assert grname not in validTypes, 'Enum already in valid types'
                validTypes.add(grname)
                assert grname not in engineEnums, 'Enum already in engine enums'
                engineEnums.add(grname)
                
            except Exception as ex:
                showError('Invalid tag ExportEnum', absPath + ' (' + str(lineIndex + 1) + ')', tagContext[0] if tagContext else '(empty)', ex)

    def parseTypeTags2():
        global codeGenTags
        
        for tagMeta in tagsMetas['ExportValueType']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                tok = tokenize(tagInfo)
                
                name = tok[0]
                ntype = tok[1]
                exportFlags = tok[2:]
                
                assert 'Layout' in exportFlags, 'No Layout specified in ExportValueType'
                assert exportFlags[exportFlags.index('Layout') + 1] == '=', 'Expected "=" after Layout tag'
                
                codeGenTags['ExportValueType'].append((name, ntype, exportFlags, comment))
                assert name not in validTypes, 'Type already in valid types'
                validTypes.add(name)
                assert name not in customTypes, 'Type already in custom types'
                customTypes.add(name)
                assert ntype not in customTypesNativeMap, 'Type already in custom types native map'
                customTypesNativeMap[ntype] = name
                
            except Exception as ex:
                showError('Invalid tag ExportValueType', absPath + ' (' + str(lineIndex + 1) + ')', tagContext[0] if tagContext else '(empty)', ex)

        for tagMeta in tagsMetas['ExportEntity']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta

            try:
                exportFlags = tokenize(tagInfo)
                
                name = exportFlags[0]
                serverClassName = exportFlags[1]
                clientClassName = exportFlags[2]
                exportFlags = exportFlags[3:]
                
                codeGenTags['ExportEntity'].append((name, serverClassName, clientClassName, exportFlags, comment))

                assert name not in validTypes           
                validTypes.add(name)
                assert name not in gameEntities
                gameEntities.append(name)
                gameEntitiesInfo[name] = {'Server': serverClassName, 'Client': clientClassName, 'IsGlobal': 'Global' in exportFlags,
                        'HasProtos': 'HasProtos' in exportFlags, 'HasStatics': 'HasStatics' in exportFlags,
                        'HasAbstract': 'HasAbstract' in exportFlags, 'HasTimeEvents': 'HasTimeEvents' in exportFlags,
                        'Exported': True, 'Comment': comment}
                
                assert name + 'Component' not in validTypes, name + 'Property component already in valid types'
                validTypes.add(name + 'Component')
                assert name + 'Property' not in validTypes, name + 'Property already in valid types'
                validTypes.add(name + 'Property')
                
                if gameEntitiesInfo[name]['HasAbstract']:
                    assert 'Abstract' + name not in validTypes
                    validTypes.add('Abstract' + name)
                    assert 'Abstract' + name not in entityRelatives
                    entityRelatives.add('Abstract' + name)
                
                if gameEntitiesInfo[name]['HasProtos']:
                    assert 'Proto' + name not in validTypes
                    validTypes.add('Proto' + name)
                    assert 'Proto' + name not in entityRelatives
                    entityRelatives.add('Proto' + name)
                
                if gameEntitiesInfo[name]['HasStatics']:
                    assert 'Static' + name not in validTypes
                    validTypes.add('Static' + name)
                    assert 'Static' + name not in entityRelatives
                    entityRelatives.add('Static' + name)
                
            except Exception as ex:
                showError('Invalid tag ExportEntity', absPath + ' (' + str(lineIndex + 1) + ')', ex)
                
    def parseTypeTags3():
        global codeGenTags
        
        for tagMeta in tagsMetas['ExportRefType']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                exportFlags = tokenize(tagInfo)
                assert exportFlags and exportFlags[0] in ['Server', 'Client', 'Mapper', 'Common'], 'Expected target in tag info'
                target = exportFlags[0]
                exportFlags = exportFlags[1:]
                
                firstLine = tagContext[0]
                firstLineTok = tokenize(firstLine)
                assert len(firstLineTok) >= 2, 'Expected 4 or more tokens in first line'
                assert firstLineTok[0] in ['class', 'struct'], 'Expected class/struct'
                
                refTypeName = firstLineTok[1]
                assert refTypeName not in validTypes
                
                assert tagContext[1].startswith('{')
                
                scriptableLines = False
                fields = []
                methods = []
                for line in tagContext[2:]:
                    line = line.lstrip()
                    if 'FO_SCRIPTABLE_OBJECT_BEGIN' in line:
                        scriptableLines = True
                    elif 'FO_SCRIPTABLE_OBJECT_END' in line:
                        scriptableLines = False
                    elif scriptableLines:
                        commPos = line.find('//')
                        if commPos != -1:
                            line = line[:commPos]
                        if not line:
                            continue
                        
                        lTok = tokenize(line)
                        assert len(lTok) >= 2
                        
                        if lTok[0] not in ['void', 'auto']:
                            if lTok[0] == 'vector':
                                fields.append((engineTypeToMetaType(lTok[0] + lTok[1] + lTok[2] + lTok[3]), lTok[4], []))
                            else:
                                fields.append((engineTypeToMetaType(lTok[0]), lTok[1], []))
                        else:
                            if lTok[0] == 'auto':
                                end = lTok[1].find(';')
                                offs =  lTok[end::-1].index('>') - 1
                                ret = ''.join(lTok[end-offs:end])
                            else:
                                ret = 'void'
                            
                            funcBegin = line.index('(')
                            funcEnd = line.index(')')
                            assert funcBegin != -1 and funcEnd != -1 and funcBegin < funcEnd, 'Invalid function definition'
                            funcArgs = line[funcBegin+1:funcEnd]
                            resultArgs = []
                            for arg in splitEngineArgs(funcArgs):
                                arg = arg.strip()
                                sep = arg.rfind(' ')
                                argType = engineTypeToMetaType(arg[:sep].rstrip())
                                argName = arg[sep + 1:]
                                resultArgs.append((argType, argName))
                            
                            methods.append((lTok[1], engineTypeToMetaType(ret), resultArgs, []))
                
                codeGenTags['ExportRefType'].append((target, refTypeName, fields, methods, exportFlags, comment))
                
                validTypes.add(refTypeName)
                refTypes.add(refTypeName)
                
            except Exception as ex:
                showError('Invalid tag ExportRefType', absPath + ' (' + str(lineIndex + 1) + ')', tagContext[0] if tagContext else '(empty)', ex)
    
    def parseTypeTags4():
        global codeGenTags
        
        for tagMeta in tagsMetas['ExportProperty']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:    
                exportFlags = tokenize(tagInfo)
                entity = tagContext[:tagContext.find(' ')]
                
                access = exportFlags[0]
                assert access in ['Common', 'Server', 'Client'], 'Invalid export property target ' + access
                exportFlags = exportFlags[1:]
                
                toks = [t.strip() for t in tagContext[tagContext.find('(') + 1:tagContext.find(')')].split(',')]
                assert len(toks) == 2
                ptype = engineTypeToMetaType(toks[0])
                assert 'uint64' not in ptype, 'Type uint64 is not supported by properties'
                name = toks[1]
                
                exportFlags.append('CoreProperty')
                
                if entity == 'Entity':
                    entities = gameEntities
                    exportFlags.append('SharedProperty')
                else:
                    entities = [entity]
                    assert entity in gameEntities, 'Invalid entity ' + entity
                
                for entity in entities:
                    codeGenTags['ExportProperty'].append((entity, access, ptype, name, exportFlags, comment))
                
            except Exception as ex:
                showError('Invalid tag ExportProperty', absPath + ' (' + str(lineIndex + 1) + ')', tagContext, ex)
                
        for tagMeta in tagsMetas['ExportMethod']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                exportFlags = tokenize(tagInfo)
                
                braceOpenPos = tagContext.find('(')
                braceClosePos = tagContext.rfind(')')
                retSpace = tagContext.rfind(' ', 0, braceOpenPos)
                funcName = tagContext[retSpace:braceOpenPos]
                funcArgs = tagContext[braceOpenPos + 1:braceClosePos]
                
                funcTok = funcName.split('_')
                assert len(funcTok) == 3, funcName
                
                target = funcTok[0].strip()
                assert target in ['Server', 'Client', 'Mapper', 'Common'], target
                entity = funcTok[1]
                assert entity in gameEntities + ['Entity'], entity
                name = funcTok[2]
                ret = engineTypeToMetaType(tagContext[tagContext.rfind(' ', 0, retSpace - 1):retSpace].strip())
                
                resultArgs = []
                for arg in splitEngineArgs(funcArgs)[1:]:
                    arg = arg.strip()
                    sep = arg.rfind(' ')
                    argType = engineTypeToMetaType(arg[:sep].rstrip())
                    argName = arg[sep + 1:]
                    resultArgs.append((argType, argName))
                
                codeGenTags['ExportMethod'].append((target, entity, name, ret, resultArgs, exportFlags, comment))
                
            except Exception as ex:
                showError('Invalid tag ExportMethod', absPath + ' (' + str(lineIndex + 1) + ')', tagContext, ex)
                
        for tagMeta in tagsMetas['ExportEvent']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                exportFlags = tokenize(tagInfo)
                
                target = None
                entity = None
                className = tagContext[:tagContext.find(' ')].strip()
                for ename, einfo in gameEntitiesInfo.items():
                    if className == einfo['Server'] or className == einfo['Client']:
                        target, entity = ('Server' if className == einfo['Server'] else 'Client'), ename
                        break
                if className == 'MapperEngine':
                    target, entity = 'Mapper', 'Game'
                assert target in ['Server', 'Client', 'Mapper'], target
                
                braceOpenPos = tagContext.find('(')
                braceClosePos = tagContext.rfind(')')
                firstCommaPos = tagContext.find(',', braceOpenPos)
                eventName = tagContext[braceOpenPos + 1:firstCommaPos if firstCommaPos != -1 else braceClosePos].strip()
                
                eventArgs = []
                if firstCommaPos != -1:
                    argsStr = tagContext[firstCommaPos + 1:braceClosePos].strip()
                    if len(argsStr):
                        for arg in splitEngineArgs(argsStr):
                            arg = arg.strip()
                            sep = arg.find('/')
                            argType = engineTypeToMetaType(arg[:sep - 1].rstrip())
                            argName = arg[sep + 2:-2]
                            eventArgs.append((argType, argName))
                
                codeGenTags['ExportEvent'].append((target, entity, eventName, eventArgs, exportFlags, comment))
            
            except Exception as ex:
                showError('Invalid tag ExportEvent', absPath + ' (' + str(lineIndex + 1) + ')', tagContext, ex)
        
        for tagMeta in tagsMetas['ExportSettings']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                exportFlags = tokenize(tagInfo)
                assert exportFlags and exportFlags[0] in ['Server', 'Client', 'Mapper', 'Common'], 'Expected target in tag info'
                target = exportFlags[0]
                exportFlags = exportFlags[1:]
                
                firstLine = tagContext[0]
                assert firstLine.startswith('SETTING_GROUP'), 'Invalid start macro'
                grName = firstLine[firstLine.find('(') + 1:firstLine.find(',')]
                assert grName.endswith('Settings'), 'Invalid group ending ' + grName
                grName = grName[:-len('Settings')]
                
                settings = []
                for l in tagContext[1:]:
                    settComment = [l[l.find('//') + 2:].strip()] if l.find('//') != -1 else []
                    settType = l[:l.find('(')]
                    assert settType in ['FIXED_SETTING', 'VARIABLE_SETTING'], 'Invalid setting type ' + settType
                    settArgs = [t.strip().strip('"') for t in l[l.find('(') + 1:l.find(')')].split(',')]
                    settings.append(('fix' if settType == 'FIXED_SETTING' else 'var', engineTypeToMetaType(settArgs[0]), settArgs[1], settArgs[2:], settComment))
                
                codeGenTags['ExportSettings'].append((grName, target, settings, exportFlags, comment))
                
            except Exception as ex:
                showError('Invalid tag ExportSettings', absPath + ' (' + str(lineIndex + 1) + ')', tagContext, ex)
        
        for tagMeta in tagsMetas['EngineHook']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                name = tokenize(tagContext)[2]
                assert name in ['InitServerEngine', 'InitClientEngine', 'ConfigSectionParseHook', 'ConfigEntryParseHook', 'SetupBakersHook'], 'Invalid engine hook ' + name
                
                codeGenTags['EngineHook'].append((name, [], comment))
            
            except Exception as ex:
                showError('Invalid tag EngineHook', absPath + ' (' + str(lineIndex + 1) + ')', tagInfo, ex)
        
        for tagMeta in tagsMetas['MigrationRule']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                ruleArgs = tokenize(tagInfo, anySymbols=[2, 3])
                assert len(ruleArgs) and ruleArgs[0] in ['Version', 'Property', 'Proto', 'Component', 'Remove'], 'Invalid migration rule'
                assert len(ruleArgs) == 4, 'Invalid migration rule args'
                assert not len([t for t in codeGenTags['MigrationRule'] if t[0][0:3] == ruleArgs[0:3]]), 'Migration rule already added'
                assert ruleArgs[2] != ruleArgs[3], 'Migration rule same last args'
                
                codeGenTags['MigrationRule'].append((ruleArgs, comment))
            
            except Exception as ex:
                showError('Invalid tag MigrationRule', absPath + ' (' + str(lineIndex + 1) + ')', tagInfo, ex)
                
        for tagMeta in tagsMetas['CodeGen']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                fname = os.path.basename(absPath)
                if fname == 'AngelScriptScripting-Template.cpp':
                    templateType = 'AngelScript'
                elif fname == 'MonoScripting-Template.cpp':
                    templateType = 'Mono'
                elif fname == 'NativeScripting-Template.cpp':
                    templateType = 'Native'
                elif fname == 'MetadataRegistration-Template.cpp':
                    templateType = 'MetadataRegistration'
                elif fname == 'GenericCode-Template.cpp':
                    templateType = 'GenericCode'
                else:
                    assert False, fname
                
                flags = tokenize(tagInfo)
                assert len(flags) >= 1, 'Invalid CodeGen entry'
                
                codeGenTags['CodeGen'].append((templateType, absPath, flags[0], lineIndex, tagContext, flags[1:], comment))
            
            except Exception as ex:
                showError('Invalid tag CodeGen', absPath + ' (' + str(lineIndex + 1) + ')', tagInfo, ex)
    
    def postprocessTags():
        # Generate missed enum values
        def findNextValue(kv):
            result = 0
            for _, value, _ in kv:
                if value is not None:
                    ivalue = int(value, 0)
                    if ivalue >= result:
                        result = ivalue + 1
            return str(result)
        
        for e in codeGenTags['ExportEnum']:
            gname, _, keyValues, _, _ = e
            for kv in keyValues:
                if kv[1] is None:
                    kv[1] = findNextValue(keyValues)
        
        # Generate entity properties enums
        for entity in gameEntities:
            keyValues = [['None', '0', []]]
            index = 1
            for propTag in codeGenTags['ExportProperty']:
                ent, _, _, name, _, _ = propTag
                if ent == entity:
                    name = name.replace('.', '_')
                    keyValues.append([name, str(index), []])
                    index += 1
            codeGenTags['ExportEnum'].append([entity + 'Property', 'uint16', keyValues, [], []])
        
        # Check for zero key entry in enums
        for e in codeGenTags['ExportEnum']:
            gname, _, keyValues, _, _ = e
            if not [1 for kv in keyValues if int(kv[1], 0) == 0]:
                showError('Zero entry not found in enum group ' + gname)
        
        # Check uniquiness of enums
        for e in codeGenTags['ExportEnum']:
            gname, _, keyValues, _, _ = e
            keys = set()
            values = set()
            for kv in keyValues:
                if kv[0] in keys:
                    showError('Detected collision in enum group ' + gname + ' key ' + kv[0])
                if int(kv[1], 0) in values:
                    showError('Detected collision in enum group ' + gname + ' value ' + kv[1] + (' (evaluated to ' + str(int(kv[1], 0)) + ')' if str(int(kv[1], 0)) != kv[1] else ''))
                keys.add(kv[0])
                values.add(int(kv[1], 0))
    
    parseTypeTags1()
    parseTypeTags2()
    parseTypeTags3()
    parseTypeTags4()
    postprocessTags()

parseTags()
checkErrors()
tagsMetas = {} # Cleanup memory

# Generate compatability version
def hashRecursive(data, hasher=None, algorithm='sha256'):
    if hasher is None:
        hasher = hashlib.new(algorithm)
    if isinstance(data, Mapping):
        for key in sorted(data.keys()):
            hasher.update(str(key).encode('utf-8'))
            hashRecursive(data[key], hasher, algorithm)
    elif isinstance(data, Iterable) and not isinstance(data, str):
        for i, item in enumerate(data):
            hasher.update(str(i).encode('utf-8'))
            hashRecursive(item, hasher, algorithm)
    elif data is None:
        hasher.update(b'None')
    else:
        hasher.update(str(data).encode('utf-8'))
    return hasher
compatablityHasher = hashRecursive(codeGenTags)
compatablityVersion = compatablityHasher.hexdigest()[:16]

# Generate API
files = {}
lastFile = None
genFileNames = []

def createFile(name, output):
    assert output
    path = os.path.join(output, name)
    genFileNames.append(name)
    
    global lastFile
    lastFile = []
    files[path] = lastFile

def writeFile(line):
    lastFile.append(line)

def insertFileLines(lines, lineIndex):
    assert lineIndex <= len(lastFile), 'Invalid line index'
    lastFileCopy = lastFile[:]
    lastFileCopy = lastFileCopy[:lineIndex] + lines + lastFileCopy[lineIndex:]
    lastFile.clear()
    lastFile.extend(lastFileCopy)

def flushFiles():
    # Generate stubs missed files
    for fname in genFileList:
        if fname not in genFileNames:
            createFile(fname, args.genoutput)
            writeFile('// Empty file')
            verbosePrint('flushFiles', 'empty', fname)
    
    # Write if content changed
    for path, lines in files.items():
        pathDir = os.path.dirname(path)
        if not os.path.isdir(pathDir):
            os.makedirs(pathDir)
        
        if os.path.isfile(path):
            with open(path, 'r', encoding='utf-8-sig') as f:
                curLines = f.readlines()
            newLinesStr = ''.join([l.rstrip('\r\n') for l in curLines]).rstrip()
            curLinesStr = ''.join(lines).rstrip()
            if newLinesStr == curLinesStr:
                verbosePrint('flushFiles', 'skip', path)
                continue
            else:
                verbosePrint('flushFiles', 'diff found', path, len(newLinesStr), len(curLinesStr))
        else:
            verbosePrint('flushFiles', 'no file', path)
        with open(path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines).rstrip('\n') + '\n')
        verbosePrint('flushFiles', 'write', path)

# Code
curCodeGenTemplateType = None

def writeCodeGenTemplate(templateType):
    global curCodeGenTemplateType
    curCodeGenTemplateType = templateType
    
    templatePath = None
    for genTag in codeGenTags['CodeGen']:
        if curCodeGenTemplateType == genTag[0]:
            templatePath = genTag[1]
            break
    assert templatePath, 'Code gen template not found'
    
    with open(templatePath, 'r', encoding='utf-8-sig') as f:
        lines = f.readlines()
    insertFileLines([l.rstrip('\r\n') for l in lines], 0)

def insertCodeGenLines(lines, entryName):
    def findTag():
        for genTag in codeGenTags['CodeGen']:
            if curCodeGenTemplateType == genTag[0] and entryName == genTag[2]:
                return genTag[3] + 1, genTag[4]
        assert False, 'Code gen entry ' + entryName + ' in ' + curCodeGenTemplateType + ' not found'
    lineIndex, padding = findTag()
    
    if padding:
        lines = [''.center(padding) + l for l in lines]
    
    insertFileLines(lines, lineIndex)

def getEntityFromTarget(target):
    if target == 'Server':
        return 'ServerEntity*'
    if target in ['Client', 'Mapper']:
        return 'ClientEntity*'
    return 'Entity*'

def metaTypeToUnifiedType(t, selfEntity = 'SELF_ENTITY'):
    tt = t.split('.')
    if tt[0] == 'dict':
        d2 = tt[2] if tt[2] != 'arr' else tt[2] + '.' + tt[3]
        r = metaTypeToUnifiedType(tt[1], selfEntity) + '=>' + metaTypeToUnifiedType(d2, selfEntity)
    elif tt[0] == 'arr':
        r = '' + metaTypeToUnifiedType(tt[1], selfEntity) + '[]'
    elif tt[0] == 'callback':
        r = 'callback(' + ','.join([metaTypeToUnifiedType(a, selfEntity) for a in '.'.join(tt[1:]).split('|') if a]) + ')'
    elif tt[0] == 'Entity':
        r = tt[0]
    elif tt[0] in gameEntities:
        r = tt[0]
    elif tt[0] in refTypes or tt[0] in entityRelatives:
        r = tt[0]
    elif tt[0] == 'SELF_ENTITY':
        r = selfEntity
    else:
        r = tt[0]
    if tt[-1] == 'ref':
        r += '&'
    return r

def metaTypeToEngineType(t, target, passIn, refAsPtr=False, selfEntity=None, noRef=False):
    def resolveEntity(e):
        if target != 'Server':
            if e == 'Game' and target == 'Mapper':
                return 'MapperEngine*'
            else:
                return gameEntitiesInfo[e]['Client'] + '*'
        else:
            return gameEntitiesInfo[e]['Server'] + '*'
    tt = t.split('.')
    if tt[0] == 'string':
        r = 'string_view' if passIn and not tt[-1] == 'ref' else 'string'
    elif tt[0] == 'dict':
        d2 = tt[2] if tt[2] != 'arr' else tt[2] + '.' + tt[3]
        if passIn and not tt[-1] == 'ref':
            r = 'readonly_map<' + metaTypeToEngineType(tt[1], target, False, selfEntity=selfEntity) + ', ' + metaTypeToEngineType(d2, target, False, selfEntity=selfEntity) + '>'
        else:
            r = 'map<' + metaTypeToEngineType(tt[1], target, False, selfEntity=selfEntity) + ', ' + metaTypeToEngineType(d2, target, False, selfEntity=selfEntity) + '>'
    elif tt[0] == 'arr':
        if passIn and not tt[-1] == 'ref':
            r = 'readonly_vector<' + metaTypeToEngineType(tt[1], target, False, selfEntity=selfEntity) + '>'
        else:
            r = 'vector<' + metaTypeToEngineType(tt[1], target, False, selfEntity=selfEntity) + '>'
    elif tt[0] == 'callback':
        r = '<' + ', '.join([metaTypeToEngineType(a, target, False, refAsPtr=True, selfEntity=selfEntity) for a in '.'.join(tt[1:]).split('|') if a]) + '>'
        r = 'ScriptFunc' + r
    elif tt[0] == 'Entity':
        r = getEntityFromTarget(target)
    elif tt[0] == 'SELF_ENTITY':
        assert selfEntity
        r = resolveEntity(selfEntity) if selfEntity != 'Entity' else 'ScriptSelfEntity*'
    elif tt[0] in gameEntities:
        r = resolveEntity(tt[0])
    elif tt[0] in refTypes or tt[0] in entityRelatives:
        r = tt[0] + '*'
    elif tt[0] in customTypes:
        for e in codeGenTags['ExportValueType']:
            if e[0] == tt[0]:
                r = e[1]
                break
        assert r, 'Invalid native type ' + tt[0]
    else:
        def mapType(mt):
            typeMap = {'any': 'any_t'}
            return typeMap[mt] if mt in typeMap else mt
        r = mapType(tt[0])
    if tt[-1] == 'ref' and not noRef:
        r += '&' if not refAsPtr else '*'
    return r

def genGenericCode():
    globalLines = []
    
    # Engine hooks
    def isHookEnabled(hookName):
        for hookTag in codeGenTags['EngineHook']:
            name, flags, _ = hookTag
            if name == hookName:
                return True
        return False
    
    globalLines.append('// Engine hooks')
    if not isHookEnabled('InitServerEngine'):
        globalLines.append('class ServerEngine;')
        globalLines.append('void InitServerEngine(ServerEngine*) { /* Stub */ }')
    if not isHookEnabled('InitClientEngine'):
        globalLines.append('class ClientEngine;')
        globalLines.append('void InitClientEngine(ClientEngine*) { /* Stub */ }')
    if not isHookEnabled('ConfigSectionParseHook'):
        globalLines.append('void ConfigSectionParseHook(const string&, string&, map<string, string>&) { /* Stub */ }')
    if not isHookEnabled('ConfigEntryParseHook'):
        globalLines.append('void ConfigEntryParseHook(const string&, const string&, string&, string&) { /* Stub */ }')
    if not isHookEnabled('SetupBakersHook'):
        globalLines.append('class BaseBaker;')
        globalLines.append('struct BakingContext;')
        globalLines.append('void SetupBakersHook(span<const string>, vector<unique_ptr<BaseBaker>>&, shared_ptr<BakingContext>) { /* Stub */ }')
    globalLines.append('')
    
    # Engine properties
    globalLines.append('// Engine property indices')
    globalLines.append('EntityProperties::EntityProperties(Properties& props) noexcept : _propsRef(&props) { }')
    commonParsed = set()
    for entity in gameEntities:
        index = 1
        for propTag in codeGenTags['ExportProperty']:
            ent, _, _, name, flags, _ = propTag
            if ent == entity:
                if 'SharedProperty' not in flags:
                    globalLines.append('uint16 ' + entity + 'Properties::' + name + '_RegIndex = ' + str(index) + ';')
                elif name not in commonParsed:
                    globalLines.append('uint16 EntityProperties::' + name + '_RegIndex = ' + str(index) + ';')
                    commonParsed.add(name)
                index += 1
    globalLines.append('')
    
    # Settings list
    def writeSettings(target):
        globalLines.append('[[maybe_unused]] auto Get' + target + 'Settings() -> unordered_set<string>')
        globalLines.append('{')
        globalLines.append('    unordered_set<string> settings = {')
        for settTag in codeGenTags['ExportSettings']:
            grName, targ, settings, flags, _ = settTag
            if targ in [target, 'Common']:
                for sett in settings:
                    fixOrVar, keyType, keyName, initValues, _ = sett
                    globalLines.append('        "' + keyName + '",')
        globalLines.append('    };')
        globalLines.append('    return settings;')
        globalLines.append('}')
        globalLines.append('')
    writeSettings('Server')
    writeSettings('Client')
    
    createFile('GenericCode-Common.cpp', args.genoutput)
    writeCodeGenTemplate('GenericCode')
    
    insertCodeGenLines(globalLines, 'Body')

try:
    genGenericCode()
except Exception as ex:
    showError('Code generation for generic code failed', ex)
checkErrors()

def genMetadataRegistration(target, isStub):
    globalLines = []
    registerLines = []
    
    def entityAllowed(entity, entityTarget):
        if entityTarget == 'Server':
            return gameEntitiesInfo[entity]['Server'] is not None
        if entityTarget in ['Client', 'Mapper']:
            return gameEntitiesInfo[entity]['Client'] is not None
        assert False, entityTarget
    
    # Enums
    registerLines.append('// Enums')
    for e in codeGenTags['ExportEnum']:
        gname, utype, keyValues, _, _ = e
        registerLines.append('meta->RegisterEnumGroup("' + gname + '", "' + utype + '",')
        registerLines.append('{')
        for kv in keyValues:
            registerLines.append('    {"' + kv[0] + '", ' + kv[1] + '},')
        registerLines.append('});')
        registerLines.append('')
    
    # Value types
    registerLines.append('// Value types')
    for et in codeGenTags['ExportValueType']:
        name, ntype, flags, _ = et
        registerLines.append('meta->RegisterValueType("' + name + '");')
    registerLines.append('')
    
    for et in codeGenTags['ExportValueType']:
        name, ntype, flags, _ = et
        registerLines.append('meta->RegisterValueTypeLayout("' + name + '", { {')
        for layoutEntry in ''.join(flags[flags.index('Layout') + 2:]).split('+'):
            type, name = layoutEntry.split('-')
            registerLines.append('    {string_view("' + name + '"), string_view("' + type + '")},')
        registerLines.append('} });')
        registerLines.append('')
    
    # Ref types
    allowedTargets = ['Common', target] + (['Client' if target == 'Mapper' else ''])
    registerLines.append('// Ref types')
    for eo in codeGenTags['ExportRefType']:
        targ, refTypeName, fields, methods, flags, _ = eo
        if targ in allowedTargets:
            hasFactory = 'HasFactory' in flags
            registerLines.append('meta->RegisterRefType("' + refTypeName + '");')
    registerLines.append('')
    
    for eo in codeGenTags['ExportRefType']:
        targ, refTypeName, fields, methods, flags, _ = eo
        if targ in allowedTargets:
            hasFactory = 'HasFactory' in flags
            registerLines.append('meta->RegisterRefTypeMethods("' + refTypeName + '", {')
            def writeRefCall(name, call):
                registerLines.append('    MethodDesc{ .Name = "' + name + '", ' +
                        '.Call = [](FuncCallData& call) {' + (' ignore_unused(call); } },' if isStub else ''))
                if not isStub:
                        registerLines.append('        struct Wrapped { ' + call + ' };')
                        registerLines.append('        NativeDataCaller::NativeCall<&Wrapped::Call>(call);')
                        registerLines.append('    } },')
            writeRefCall('__AddRef', 'static void Call(' + refTypeName + '* self) { self->AddRef(); }')
            writeRefCall('__Release', 'static void Call(' + refTypeName + '* self) { self->Release(); }')
            if hasFactory:
                registerLines.append('    MethodDesc{ .Name = "__Factory", ' +
                        '.Ret = resolve_type("' + refTypeName + '"), .Call = [](FuncCallData& call) {' +
                        (' ignore_unused(call); } },' if isStub else ''))
                if not isStub:
                    registerLines.append('        FO_STACK_TRACE_ENTRY_NAMED("' + refTypeName + '::__Factory");')
                    registerLines.append('        struct Wrapped { ' + 'static auto Call() -> ' + refTypeName + '* ' +
                            '{ return SafeAlloc::MakeRefCounted<' + refTypeName + '>().release_ownership(); }' + ' };')
                    registerLines.append('        NativeDataCaller::NativeCall<&Wrapped::Call>(call);')
                    registerLines.append('    } },')
            def writeMethod(methodName, ret, params, getter=False, setter=False):
                registerLines.append('    MethodDesc{ .Name = "' + methodName + '", ' +
                        '.Args = {' + ', '.join('{"' + p[1] + '", resolve_type("' + metaTypeToUnifiedType(p[0]) + '")}' for p in params) + '}, ' +
                        '.Ret = ' + ('resolve_type("' + metaTypeToUnifiedType(ret) + '")' if ret != 'void' else '{' + '}') + ', ' +
                        '.Call = [](FuncCallData& call) {' + (' ignore_unused(call); }' if isStub else ''))
                if not isStub:
                    isProp = getter or setter
                    registerLines.append('        FO_STACK_TRACE_ENTRY_NAMED("' + refTypeName + '::' + methodName +
                            (' (Getter)' if getter else '') + (' (Setter)' if setter else '') + '");')
                    registerLines.append('        struct Wrapped { static ' +
                            ('auto' if ret != 'void' else 'void') + ' Call(' + refTypeName + '* self' + (', ' if params else '') +
                            ', '.join([metaTypeToEngineType(p[0], targ, True) + ' ' + p[1] for p in params]) + ') ' +
                            ('-> ' + metaTypeToEngineType(ret, targ, False) if ret != 'void' else '') +
                            ' { ' + ('return ' if ret != 'void' else '') + 'self->' + methodName +
                            ('(' if not isProp else ' = ' if setter else '') + ', '.join([p[1] for p in params]) + (')' if not isProp else '') + '; } };')
                    registerLines.append('        NativeDataCaller::NativeCall<&Wrapped::Call>(call);')
                    registerLines.append('    }' + (', .Getter = true' if getter else '') + (', .Setter = true' if setter else '') + ' },')
                else:
                    registerLines[-1] += (', .Getter = true' if getter else '') + (', .Setter = true' if setter else '') + ' },'
            for f in fields:
                writeMethod(f[1], f[0], [], getter=True)
                writeMethod(f[1], 'void', [[f[0], 'value']], setter=True)
            for m in methods:
                writeMethod(m[0], m[1], m[2])
            registerLines.append('});')
            registerLines.append('')
    
    # Entity types
    registerLines.append('// Entity types')
    registerLines.append('unordered_map<string, PropertyRegistrator*> registrators;')
    registerLines.append('PropertyRegistrator* registrator = nullptr;')
    registerLines.append('')
    for entity in gameEntities:
        if not entityAllowed(entity, target):
            continue
        registerLines.append('registrators["' + entity + '"] = meta->RegisterEntityType("' + entity + '", ' +
                ('true' if gameEntitiesInfo[entity]['Exported'] else 'false') + ', ' +
                ('true' if gameEntitiesInfo[entity]['IsGlobal'] else 'false') + ', ' +
                ('true' if gameEntitiesInfo[entity]['HasProtos'] else 'false') + ', ' +
                ('true' if gameEntitiesInfo[entity]['HasStatics'] else 'false') + ', ' +
                ('true' if gameEntitiesInfo[entity]['HasAbstract'] else 'false') + ');')
    registerLines.append('')
    
    # Properties
    registerLines.append('// Properties')
    def getRegisterFlags(t, name, access, baseFlags):
        return [access, metaTypeToUnifiedType(t), name] + baseFlags
    for entity in gameEntities:
        if not entityAllowed(entity, target):
            continue
        registerLines.append('registrator = registrators["' + entity + '"];')
        for propTag in codeGenTags['ExportProperty']:
            ent, access, type, name, flags, _ = propTag
            if ent == entity:
                registerLines.append('registrator->RegisterProperty({' + ', '.join(['"' + f + '"' for f in getRegisterFlags(type, name, access, flags)]) + '});')
        registerLines.append('')
    
    # Methods
    registerLines.append('// Methods')
    registerLines.append('vector<MethodDesc> methods;')
    registerLines.append('methods.reserve(256);')
    allowedTargets = ['Common', target] + (['Client' if target == 'Mapper' else ''])
    for entity in gameEntities:
        zoneNames = []
        def calcZoneName(name):
            count = zoneNames.count(name)
            zoneNames.append(name)
            return name if count == 0 else name + '_' + str(count + 1)
        for methodTag in codeGenTags['ExportMethod']:
            targ, ent, name, ret, params, exportFlags, comment = methodTag
            if targ in allowedTargets and (ent == entity or ent == 'Entity'):
                if 'TimeEventRelated' in exportFlags and not gameEntitiesInfo[entity]['HasTimeEvents']:
                    continue    
                isGeneric = ent == 'Entity'
                engineEntityType = gameEntitiesInfo[entity]['Client' if target != 'Server' else 'Server']
                engineEntityTypeExtern = getEntityFromTarget(target) if isGeneric else engineEntityType + '*'
                engineEntityTypeExtern2 = 'Entity' if isGeneric else entity
                if entity == 'Game':
                    if target == 'Mapper':
                        engineEntityType = 'MapperEngine*'
                    if targ == 'Common':
                        engineEntityTypeExtern = 'BaseEngine*'
                    elif targ == 'Mapper':
                        engineEntityTypeExtern = 'MapperEngine*'
                retType = metaTypeToEngineType(ret, targ, False, selfEntity='Entity')
                funcName = targ + '_' + engineEntityTypeExtern2 + '_' + name
                if not isStub:
                    globalLines.append('extern ' + retType + ' ' + funcName + '(' + engineEntityTypeExtern + (', ' if params else '') +
                            ', '.join([metaTypeToEngineType(p[0], targ, True, selfEntity='Entity') for p in params]) + ');')
                registerLines.append('methods.emplace_back(MethodDesc{ .Name = "' + name + '", ' +
                        '.Args = {' + ', '.join('{"' + p[1] + '", resolve_type("' + metaTypeToUnifiedType(p[0], selfEntity=entity) + '")}' for p in params) + '}, ' +
                        '.Ret = ' + ('resolve_type("' + metaTypeToUnifiedType(ret, selfEntity=entity) + '")' if ret != 'void' else '{' + '}') + ', ' +
                        '.Call = [](FuncCallData& call) {')
                if not isStub:
                    registerLines.append('    FO_STACK_TRACE_ENTRY_NAMED("' + calcZoneName(funcName) + '");')
                    registerLines.append('    NativeDataCaller::NativeCall<static_cast<' + retType + '(*)(' + engineEntityTypeExtern + (', ' if params else '') +
                            ', '.join([metaTypeToEngineType(p[0], targ, True, selfEntity='Entity') for p in params]) + ')>(&' + funcName + ')>(call);')
                else:
                    registerLines.append('    ignore_unused(call);')
                registerLines.append('}' +
                        (', .Getter = true' if 'Getter' in exportFlags else '') +
                        (', .Setter = true' if 'Setter' in exportFlags else '') +
                        (', .PassOwnership = true' if 'PassOwnership' in exportFlags else '') + 
                        ' });')
        registerLines.append('meta->RegisterEntityMethods("' + entity + '", std::move(methods));')
        registerLines.append('methods.clear();')
        registerLines.append('')
    registerLines.append('')
    
    # Events
    registerLines.append('// Events')
    for entity in gameEntities:
        for evTag in codeGenTags['ExportEvent']:
            targ, ent, evName, evArgs, evFlags, _ = evTag
            if targ in allowedTargets and ent == entity:
                evArgs = ', '.join('{"' + p[1] + '", resolve_type("' + metaTypeToUnifiedType(p[0], selfEntity=entity) + '")}' for p in evArgs)
                registerLines.append('meta->RegisterEntityEvent("' + entity + '", EntityEventDesc { .Name = "' + evName + '", ' +
                        '.Args = {' + evArgs + '}, .Exported = true, .Deferred = false });')
    registerLines.append('')
    
    # Migration rules
    if target in ['Server', 'Mapper'] and not isStub:
        registerLines.append('// Migration rules')
        registerLines.append('const auto to_hstring = [&](string_view str) -> hstring { return meta->Hashes.ToHashedString(str); };')
        registerLines.append('')
        registerLines.append('meta->RegisterMigrationRules({')
        for arg0 in sorted(set(ruleTag[0][0] for ruleTag in codeGenTags['MigrationRule'])):
            registerLines.append('    {')
            registerLines.append('        to_hstring("' + arg0 + '"), {')
            for arg1 in sorted(set(ruleTag[0][1] for ruleTag in codeGenTags['MigrationRule'] if ruleTag[0][0] == arg0)):
                registerLines.append('            {')
                registerLines.append('                to_hstring("' + arg1 + '"), {')
                for ruleTag in codeGenTags['MigrationRule']:
                    if ruleTag[0][0] == arg0 and ruleTag[0][1] == arg1:
                        registerLines.append('                    {to_hstring("' + ruleTag[0][2] + '"), to_hstring("' + ruleTag[0][3] + '")},')
                registerLines.append('                },')
                registerLines.append('            },')
            registerLines.append('        },')
            registerLines.append('    },')
        registerLines.append('});')
        registerLines.append('')
    
    includeLines = []
    for commonheader in args.commonheader:
        includeLines.append('#include "' + commonheader + '"')
    
    createFile('MetadataRegistration-' + target + ('Stub' if isStub else '') + '.cpp', args.genoutput)
    writeCodeGenTemplate('MetadataRegistration')
    insertCodeGenLines(registerLines, 'Register')
    insertCodeGenLines(globalLines, 'Global')
    insertCodeGenLines(includeLines, 'Includes')
    
    if target == 'Server':
        insertCodeGenLines(['#define SERVER_REGISTRATION 1', '#define CLIENT_REGISTRATION 0',
                '#define MAPPER_REGISTRATION 0', '#define STUB_MODE ' + ('1' if isStub else '0')], 'Defines')
    elif target == 'Client':
        insertCodeGenLines(['#define SERVER_REGISTRATION 0', '#define CLIENT_REGISTRATION 1',
                '#define MAPPER_REGISTRATION 0', '#define STUB_MODE ' + ('1' if isStub else '0')], 'Defines')
    elif target == 'Mapper':
        insertCodeGenLines(['#define SERVER_REGISTRATION 0', '#define CLIENT_REGISTRATION 0',
                '#define MAPPER_REGISTRATION 1', '#define STUB_MODE ' + ('1' if isStub else '0')], 'Defines')

try:
    genMetadataRegistration('Mapper', False)
    if args.angelscript:
        genMetadataRegistration('Mapper', True)
    
    genMetadataRegistration('Server', False)
    genMetadataRegistration('Client', False)
    if args.angelscript:
        genMetadataRegistration('Server', True)
        genMetadataRegistration('Client', True)
    
except Exception as ex:
    showError('Code generation for data registration failed', ex)
    
checkErrors()

# Markdown
def genApiMarkdown():
    createFile('SCRIPT_API.md', args.mdpath)
    writeFile('# ' + args.nicename + ' Script API')
    writeFile('')
    writeFile('## Table of Content')
    writeFile('')
    writeFile('GNEREATE AUTOMATICALLY')
    writeFile('')
    
    # Generate source
    def writeComm(comm, ident):
        if not comm:
            comm = ['...']
        writeFile('')
        index = 0
        for c in comm:
            if c.startswith('param'):
                pass
            elif c.startswith('return'):
                pass
            else:
                pass
            writeFile(''.center(ident * 2 + 2) + '' + c + '' + ('  ' if index < len(comm) - 1 else ''))
            index += 1
        writeFile('')
    
    writeFile('## Settings')
    writeFile('')
    writeFile('### General')
    writeFile('')
    for settTag in codeGenTags['ExportSettings']:
        grName, targ, settings, flags, comm = settTag
        writeFile('### ' + grName + (' ' + ', '.join(flags) if flags else ''))
        writeComm(comm, 0)
        for sett in settings:
            fixOrVar, keyType, keyName, initValues, comm2 = sett
            writeFile('* `' + ('const ' if fixOrVar == 'fix' else '')  + metaTypeToUnifiedType(keyType) + ' ' + keyName + ' = ' + ', '.join(initValues) + '`')
            writeComm(comm2, 0)
        writeFile('')
    
    for entity in gameEntities:
        entityInfo = gameEntitiesInfo[entity]
        writeFile('## ' + entity + ' entity')
        writeComm(entityInfo['Comment'], 0)
        writeFile('* `Target: ' + ('Server/Client' if entityInfo['Client'] else 'Server only') + '`')
        writeFile('* `Built-in: ' + ('Yes' if entityInfo['Exported'] else 'No') + '`')
        writeFile('* `Singleton: ' + ('Yes' if entityInfo['IsGlobal'] else 'No') + '`')
        writeFile('* `Has proto: ' + ('Yes' if entityInfo['HasProtos'] else 'No') + '`')
        writeFile('* `Has statics: ' + ('Yes' if entityInfo['HasStatics'] else 'No') + '`')
        writeFile('* `Has abstract: ' + ('Yes' if entityInfo['HasAbstract'] else 'No') + '`')
        writeFile('')
        writeFile('### ' + entity + ' properties')
        writeFile('')
        for propTag in codeGenTags['ExportProperty']:
            ent, access, type, name, flags, comm = propTag
            if ent == entity:
                writeFile('* `' + access + ' ' + metaTypeToUnifiedType(type) + ' ' + name + (' ' + ' '.join(flags) if flags else '') + '`')
                writeComm(comm, 0)
        writeFile('### ' + entity + ' server events')
        writeFile('')
        for evTag in codeGenTags['ExportEvent']:
            targ, ent, evName, evArgs, evFlags, comm = evTag
            if ent == entity and targ == 'Server':
                writeFile('* `' + evName + '(' + ', '.join([metaTypeToUnifiedType(a[0]) + ' ' + a[1] for a in evArgs]) + ')' + (' ' + ' '.join(evFlags) if evFlags else '') + '`')
                writeComm(comm, 0)
        writeFile('### ' + entity + ' client events')
        writeFile('')
        for evTag in codeGenTags['ExportEvent']:
            targ, ent, evName, evArgs, evFlags, comm = evTag
            if ent == entity and targ == 'Client':
                writeFile('* `' + evName + '(' + ', '.join([metaTypeToUnifiedType(a[0]) + ' ' + a[1] for a in evArgs]) + ')' + (' ' + ' '.join(evFlags) if evFlags else '') + '`')
                writeComm(comm, 0)
        writeFile('### ' + entity + ' common methods')
        writeFile('')
        for methodTag in codeGenTags['ExportMethod']:
            targ, ent, name, ret, params, exportFlags, comm = methodTag
            if ent == entity and targ == 'Common':
                writeFile('* `' + metaTypeToUnifiedType(ret) + ' ' + name + '(' + ', '.join([metaTypeToUnifiedType(a[0]) + ' ' + a[1] for a in params]) + ')' + (' ' + ' '.join(exportFlags) if exportFlags else '') + '`')
                writeComm(comm, 0)
        writeFile('### ' + entity + ' server methods')
        writeFile('')
        for methodTag in codeGenTags['ExportMethod']:
            targ, ent, name, ret, params, exportFlags, comm = methodTag
            if ent == entity and targ == 'Server':
                writeFile('* `' + metaTypeToUnifiedType(ret) + ' ' + name + '(' + ', '.join([metaTypeToUnifiedType(a[0]) + ' ' + a[1] for a in params]) + ')' + (' ' + ' '.join(exportFlags) if exportFlags else '') + '`')
                writeComm(comm, 0)
        writeFile('### ' + entity + ' client methods')
        writeFile('')
        for methodTag in codeGenTags['ExportMethod']:
            targ, ent, name, ret, params, exportFlags, comm = methodTag
            if ent == entity and targ == 'Client':
                writeFile('* `' + metaTypeToUnifiedType(ret) + ' ' + name + '(' + ', '.join([metaTypeToUnifiedType(a[0]) + ' ' + a[1] for a in params]) + ')' + (' ' + ' '.join(exportFlags) if exportFlags else '') + '`')
                writeComm(comm, 0)
        if entity == 'Game':
            writeFile('### ' + entity + ' mapper events')
            writeFile('')
            for evTag in codeGenTags['ExportEvent']:
                targ, ent, evName, evArgs, evFlags, comm = evTag
                if ent == entity and targ == 'Mapper':
                    writeFile('* `' + evName + '(' + ', '.join([metaTypeToUnifiedType(a[0]) + ' ' + a[1] for a in evArgs]) + ')' + (' ' + ' '.join(evFlags) if evFlags else '') + '`')
                    writeComm(comm, 0)
            writeFile('### ' + entity + ' mapper methods')
            writeFile('')
            for methodTag in codeGenTags['ExportMethod']:
                targ, ent, name, ret, params, exportFlags, comm = methodTag
                if ent == entity and targ == 'Mapper':
                    writeFile('* `' + metaTypeToUnifiedType(ret) + ' ' + name + '(' + ', '.join([metaTypeToUnifiedType(a[0]) + ' ' + a[1] for a in params]) + ')' + (' ' + ' '.join(exportFlags) if exportFlags else '') + '`')
                    writeComm(comm, 0)
    
    writeFile('## Types')
    writeFile('')
    for eoTag in codeGenTags['ExportRefType']:
        targ, refTypeName, fields, methods, flags, comm = eoTag
        writeFile('### ' + refTypeName + ' reference object')
        writeComm(comm, 0)
        for f in fields:
            writeFile('* `' + metaTypeToUnifiedType(f[0]) + ' ' + f[1] + '`')
            writeComm(f[2], 0)
        for m in methods:
            writeFile('* `' + metaTypeToUnifiedType(m[1]) + ' ' + m[0] + '()' + '`')
            writeComm(m[2], 0)
    for etTag in codeGenTags['ExportValueType']:
        name, ntype, flags, comm = etTag
        writeFile('### ' + name + ' value object')
        writeComm(comm, 0)
        if flags:
            writeFile('* `Flags: ' + ', '.join(flags) + '`')
        writeFile('')
    
    writeFile('## Enums')
    writeFile('')
    for eTag in codeGenTags['ExportEnum']:
        gname, utype, keyValues, flags, comm = eTag
        writeFile('* `' + gname + (' ' + ' '.join(flags) if flags else '') + '`')
        writeComm(comm, 0)
        for kv in keyValues:
            writeFile('  - `' + kv[0] + ' = ' + kv[1] + '`')
            if kv[2]:
                writeComm(kv[2], 1)
            else:
                writeFile('')
    
    # Cleanup empty sections and generate Table of content
    global lastFile
    filteredLines = []
    tableOfContent = []
    i = 0
    while i < len(lastFile):
        if i < len(lastFile) - 2 and lastFile[i].startswith('### ') and lastFile[i + 2].startswith('#'):
            i += 2
            continue
        if lastFile[i].startswith('## '):
            tableOfContent.append('* [' + lastFile[i][3:] + '](#' + lastFile[i][3:].lower().replace(' ', '-') + ')')
        elif lastFile[i].startswith('### '):
            tableOfContent.append('  - [' + lastFile[i][4:] + '](#' + lastFile[i][4:].lower().replace(' ', '-') + ')')
        filteredLines.append(lastFile[i])
        i += 1
    lastFile.clear()
    lastFile.extend(filteredLines[:4] + tableOfContent + filteredLines[5:])

if args.markdown:
    try:
        genApiMarkdown()
    
    except Exception as ex:
        showError('Can\'t generate markdown representation', ex)
    
    checkErrors()

"""
def genApi(target):
    # C++ projects
    if args.native:
        createFile('FOnline.' + target + '.h')

        # Generate source
        def parseType(t):
            def mapType(t):
                typeMap = {'int8': 'int8', 'uint8': 'uint8', 'int16': 'int16', 'uint16': 'uint16', 'int64': 'int64', 'uint64': 'uint64',
                        'ItemView': 'ScriptItem*', 'ItemHexView': 'ScriptItem*', 'PlayerView': 'ScriptPlayer*', 'Player': 'ScriptPlayer*',
                        'CritterView': 'ScriptCritter*', 'CritterHexView': 'ScriptCritter*', 'MapView': 'ScriptMap*', 'LocationView': 'ScriptLocation*',
                        'Item': 'ScriptItem*', 'Critter': 'ScriptCritter*', 'Map': 'ScriptMap*', 'Location': 'ScriptLocation*',
                        'string': 'string', 'Entity': 'ScriptEntity*'}
                return typeMap[t] if t in typeMap else t
            tt = t.split('.')
            if tt[0] == 'dict':
                r = 'map<' + mapType(tt[1]) + ', ' + mapType(tt[2]) + '>'
            elif tt[0] in refTypes:
                return tt[0] + '*'
            else:
                r = mapType(tt[0])
            if 'arr' in tt:
                r = 'vector<' + r + '>'
            if 'ref' in tt:
                r += '&'
            return r
        def parseArgs(args):
            return ', '.join([parseType(a[0]) + ' ' + a[1] for a in args])
        def writeDoc(ident, doc):
            if doc:
                writeFile(''.center(ident) + '/**')
                for d in doc:
                    writeFile(''.center(ident) + ' * ' + d)
                writeFile(''.center(ident) + ' */')
            else:
                writeFile(''.center(ident) + '/**')
                writeFile(''.center(ident) + ' * ...')
                writeFile(''.center(ident) + ' */')
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

        # Header
        writeFile('// FOnline scripting native API')
        writeFile('')
        writeFile('#include <cstdint>')
        writeFile('#include <vector>')
        writeFile('#include <map>')
        writeFile('#include <functional>')
        writeFile('')
        
        # Namespace begin
        writeFile('namespace FOnline')
        writeFile('{')
        writeFile('')
        
        # Usings
        writeFile('')
        
        # Forward declarations
        writeFile('class ScriptEntity;')
        for entity in ['Player', 'Item', 'Critter', 'Map', 'Location']:
            writeFile('class Script' + entity + ';')
        writeFile('class MapSprite;')
        writeFile('')
        
        # Enums
        for i in nativeMeta.enums:
            group, entries, doc = i
            writeDoc(0, doc)
            writeFile('enum class ' + group)
            writeFile('{')
            for e in entries:
                name, val, edoc = e
                writeDoc(4, edoc)
                writeFile('    ' + name + ' = ' + val + ',')
                if e != entries[len(entries) - 1]:
                    writeFile('')
            writeFile('};')
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

        for entity in ['Player', 'Item', 'Critter', 'Map', 'Location']:
            writeFile('class Script' + entity + ' : public ScriptEntity')
            writeFile('{')
            writeFile('public:')
            for i in nativeMeta.properties[entity.lower()]:
                writeProp(i, entity)
            writeFile('')
            if target == 'Server':
                for i in nativeMeta.methods[entity.lower()]:
                    writeMethod(i, entity)
            if target == 'Client':
                for i in nativeMeta.methods[entity.lower() + 'view']:
                    writeMethod(i, entity)
            writeFile('};')
            writeFile('')

        # Events
        " ""
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
        " ""

        # Settings
        " ""
        writeFile('## Settings')
        writeFile('')
        for i in nativeMeta.settings:
            ret, name, init, doc = i
            writeFile('* ' + parseType(ret) + ' ' + name + ' = ' + init)
            writeDoc(doc)
            writeFile('')
        " ""

        # Namespace end
        writeFile('} // namespace FOnline')

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
                typeMap = {'int8': 'sbyte', 'uint8': 'byte', 'int16': 'short', 'uint16': 'ushort', 'int32': 'int', 'uint32': 'uint', 'int64': 'long', 'uint64': 'ulong',
                        'PlayerView': 'Player', 'ItemView': 'Item', 'CritterView': 'Critter', 'MapView': 'Map', 'LocationView': 'Location'}
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
        for entity in ['Player', 'Item', 'Critter', 'Map', 'Location']:
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
            writeFile('        public static event ' + name + 'Delegate ' + name + ';')
            writeFile('        public delegate void ' + name + 'Delegate(' + parseArgs(eargs) + ');')
            writeFile('        public static event ' + name + 'RetDelegate ' + name + 'Ret;')
            writeFile('        public delegate bool ' + name + 'RetDelegate(' + parseArgs(eargs) + ');')
            writeFile('')
        def writeEventExt(tok):
            name, eargs, doc = tok
            pargs = parseArgs(eargs)
            writeFile('        static bool _' + name + '(' + (pargs + ', ' if pargs else '') + 'out Exception[] exs)')
            writeFile('        {')
            writeFile('            exs = null;')
            writeFile('            foreach (var eventDelegate in ' + name + '.GetInvocationList().Cast<' + name + 'Delegate>())')
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
            writeFile('            foreach (var eventDelegate in ' + name + 'Ret.GetInvocationList().Cast<' + name + 'RetDelegate>())')
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
            elif target == 'Mapper':
                for src in args.monomappersource:
                    if src.split(',')[0] == assembly:
                        writeFile('    <Compile Include="' + src.split(',')[1].replace('/', '\\') + '" />')
            writeFile('    <Compile Include="' + target + 'Game.cs" />')
            writeFile('    <Compile Include="' + target + 'Globals.cs" />')
            writeFile('    <Compile Include="' + target + 'Player.cs" />')
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
            elif target == 'Mapper':
                for ref in args.monomapperref:
                    if ref.split(',')[0] == assembly:
                        addRef(ref.split(',')[1])
            writeFile('  </ItemGroup>')

            writeFile('  <Import Project="$(MSBuildToolsPath)\\Microsoft.CSharp.targets" />')
            writeFile('</Project>')
"""

csprojects = []
"""
try:
    genApi('Server')
    genApi('Client')
    if args.mapper:
        genApi('Mapper')
        
except Exception as ex:
    showError('Can\'t generate scripts', ex)
"""

# Write solution file
if csprojects:
    try:
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
    
    except Exception as ex:
        showError('Can\'t write solution files', ex)

checkErrors()

# Embedded resources
try:
    capacity = int(args.embedded)
    assert capacity >= 10000, 'Embedded capacity must be greather/equal to 10000'
    assert capacity % 10000 == 0, 'Embedded capacity must иу ьгдешзду ин 10000'
    createFile('EmbeddedResources-Include.h', args.genoutput)
    writeFile('alignas(uint32_t) volatile const uint8_t EMBEDDED_RESOURCES[' + str(capacity) + '] = {' + ','.join([str((i + 42) % 200) for i in range(capacity)]) + '};')
    
except Exception as ex:
    showError('Can\'t write embedded resources', ex)

checkErrors()

# Version info
createFile('Version-Include.h', args.genoutput)
writeFile('static constexpr string_view_nt FO_BUILD_HASH = "' + args.buildhash + '";')
writeFile('static constexpr string_view_nt FO_DEV_NAME = "' + args.devname + '";')
writeFile('static constexpr string_view_nt FO_NICE_NAME = "' + args.nicename + '";')
writeFile('static constexpr string_view_nt FO_COMPATIBILITY_VERSION = "' + compatablityVersion + '";')

try:
    branch = subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"], text=True).strip()
    writeFile('static constexpr string_view_nt FO_GIT_BRANCH = "' + branch + '";')
except:
    writeFile('static constexpr string_view_nt FO_GIT_BRANCH = "";')

# Actual writing of generated files
try:
    flushFiles()
    
except Exception as ex:
    showError('Can\'t flush generated files', ex)

checkErrors()

elapsedTime = time.time() - startTime
print('[CodeGen]', 'Code generation complete in ' + '{:.2f}'.format(elapsedTime) + ' seconds')
