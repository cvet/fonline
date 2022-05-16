#!/usr/bin/python3

print('[CodeGen]', 'Start code generation')

import os
import sys
import argparse
import time
import uuid
import io
import zipfile
import struct

startTime = time.time()

parser = argparse.ArgumentParser(description='FOnline code generator', fromfile_prefix_chars='@')
parser.add_argument('-buildhash', dest='buildhash', required=True, help='build hash')
parser.add_argument('-devname', dest='devname', required=True, help='dev game name and version')
parser.add_argument('-gamename', dest='gamename', required=True, help='game name and version')
parser.add_argument('-meta', dest='meta', required=True, action='append', help='path to script api metadata (///@ tags)')
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
parser.add_argument('-resource', dest='resource', action='append', default=[], help='resource file path')
parser.add_argument('-config', dest='config', action='append', default=[], help='debugging config')
parser.add_argument('-genoutput', dest='genoutput', required=True, help='generated code output dir')
args = parser.parse_args()

def getGuid(name):
    return '{' + str(uuid.uuid3(uuid.NAMESPACE_OID, name)).upper() + '}'

# Parse meta information
codeGenTags = {
        'ExportEnum': [], # (group name, underlying type, [(key, value, [comment])], [flags], [comment])
        'ExportProperty': [], # (entity, access, type, name, [flags], [comment])
        'ExportMethod': [], # (target, entity, name, ret, [(type, name)], [flags], [comment])
        'ExportEvent': [], # (target, entity, name, [(type, name)], [flags], [comment])
        'ExportObject': [], # (target, name, [(type, name, [comment])], [flags], [comment])
        'ExportEntity': [], # (name, serverClassName, clientClassName, [flags], [comment])
        'ExportSettings': [], #(group name, target, [(fixOrVar, keyType, keyName, [initValues], [comment])], [flags], [comment])
        'Enum': [], # (group name, underlying type, [(key, value, [comment])], [flags], [comment])
        'Property': [], # (entity, access, type, name, [flags], [comment])
        'Event': [], # (target, entity, name, [(type, name)], [flags], [comment])
        'RemoteCall': [], # (target, subsystem, name, [(type, name)], [flags], [comment])
        'Setting': [], #(type, name, init value, [flags], [comment])
        'CodeGen': [] } # (templateType, absPath, entry, line, padding, [flags], [comment])
        
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
            
            writeStub('EmbeddedResources-Include.h')
            writeStub('Version-Include.h')
            writeStub('SettingsDefault-Include.h')
            writeStub('EntityProperties-Common.cpp')
            writeStub('DataRegistration-Server.cpp')
            writeStub('DataRegistration-Client.cpp')
            writeStub('DataRegistration-Mapper.cpp')
            writeStub('DataRegistration-Single.cpp')
            writeStub('DataRegistration-Baker.cpp')
            writeStub('DataRegistration-ServerCompiler.cpp')
            writeStub('DataRegistration-ClientCompiler.cpp')
            writeStub('DataRegistration-MapperCompiler.cpp')
            writeStub('DataRegistration-SingleCompiler.cpp')
            writeStub('AngelScriptScripting-Server.cpp')
            writeStub('AngelScriptScripting-Client.cpp')
            writeStub('AngelScriptScripting-Mapper.cpp')
            writeStub('AngelScriptScripting-Single.cpp')
            writeStub('AngelScriptScripting-ServerCompiler.cpp')
            writeStub('AngelScriptScripting-ClientCompiler.cpp')
            writeStub('AngelScriptScripting-MapperCompiler.cpp')
            writeStub('AngelScriptScripting-SingleCompiler.cpp')
            writeStub('MonoScripting-Server.cpp')
            writeStub('MonoScripting-Client.cpp')
            writeStub('MonoScripting-Mapper.cpp')
            writeStub('MonoScripting-Single.cpp')
            writeStub('NativeScripting-Server.cpp')
            writeStub('NativeScripting-Client.cpp')
            writeStub('NativeScripting-Mapper.cpp')
            writeStub('NativeScripting-Single.cpp')
            
        except Exception as ex:
            showError('Can\'t write stubs', ex)
        
        for e in errors[0]:
            if isinstance(e, BaseException):
                print('[CodeGen]', 'Most recent exception:')
                raise e
        
        sys.exit(1)

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
            if tagPos == -1 or lineLen - tagPos < 5 or line[tagPos] != '/' or line[tagPos + 1] != '/' or line[tagPos + 2] != '/':
                continue
            
            tagType = line[tagPos + 3]
            
            if tagType == '#':
                lastComment.append(line[tagPos + 4:].strip())
                
            elif tagType == '@':
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
                    elif tagName == 'ExportObject':
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
                
                elif tagName == 'CodeGen':
                    tagContext = tagPos
                
                tagsMetas[tagName].append((absPath, lineIndex, tagInfo, tagContext, comment))
                lastComment = []
                
        except Exception as ex:
            showError('Invalid tag format', absPath + ' (' + str(lineIndex + 1) + ')', line.strip(), ex)

metaFiles = []
for path in args.meta:
    absPath = os.path.abspath(path)
    if absPath not in metaFiles and 'GeneratedSource' not in absPath:
        metaFiles.append(absPath)
metaFiles.sort()

for absPath in metaFiles:
    parseMetaFile(absPath)

checkErrors()

userObjects = set()
scriptEnums = set()
engineEnums = set()
gameEntities = []
gameEntitiesInfo = {}
entityRelatives = set()
genericFuncdefs = set()

def parseTags():
    validTypes = set()
    validTypes.update(['int8', 'uint8', 'int16', 'uint16', 'int', 'uint', 'int64', 'uint64',
            'float', 'double', 'hstring', 'string', 'bool', 'Entity', 'void'])
    
    def unifiedTypeToMetaType(t):
        if t.startswith('init-'):
            return 'init.' + unifiedTypeToMetaType(t[5:])
        if t.startswith('ObjInfo-'):
            return t.replace('-', '.')
        if t.startswith('ScriptFuncName-'):
            fd = [unifiedTypeToMetaType(a) for a in t[len('ScriptFuncName-'):].split('|')]
            genericFuncdefs.add('|'.join(fd))
            return 'ScriptFuncName.' + '|'.join(fd)
        if t.endswith('&'):
            return unifiedTypeToMetaType(t[:-1]) + '.ref'
        if '=>' in t:
            tt = t.split('=>')
            return 'dict.' + unifiedTypeToMetaType(tt[0]) + '.' + unifiedTypeToMetaType(tt[1])
        if t.endswith('[]'):
            return 'arr.' + unifiedTypeToMetaType(t[:-2])
        if t.startswith('predicate-'):
            return 'predicate.' + unifiedTypeToMetaType(t[10:])
        if t.startswith('callback-'):
            return 'callback.' + unifiedTypeToMetaType(t[9:])
        assert t in validTypes, 'Invalid type ' + t
        return t

    def engineTypeToUnifiedType(t):
        if t.startswith('InitFunc<'):
            r = engineTypeToUnifiedType(t[t.find('<') + 1:t.rfind('>')])
            return 'init-' + r
        elif t.startswith('ObjInfo<'):
            return 'ObjInfo-' + t[t.find('<') + 1:t.rfind('>')]
        elif t.startswith('ScriptFuncName<'):
            fargs = splitEngineArgs(t[t.find('<') + 1:t.rfind('>')])
            return 'ScriptFuncName-' + '|'.join([engineTypeToUnifiedType(a.strip()) for a in fargs])
        elif t.find('map<') != -1:
            tt = t[t.find('<') + 1:t.rfind('>')].split(',', 1)
            r = engineTypeToUnifiedType(tt[0].strip()) + '=>' + engineTypeToUnifiedType(tt[1].strip())
            if not t.startswith('const') and t.endswith('&'):
                r += '&'
            return r
        elif t.find('vector<') != -1:
            r = engineTypeToUnifiedType(t[t.find('<') + 1:t.rfind('>')]) + '[]'
            if not t.startswith('const') and t.endswith('&'):
                r += '&'
            return r
        elif t.find('std::function<') != -1:
            tt = t[t.find('<') + 1:t.rfind('>')].split('(', 1)
            assert tt[0] in ['void', 'bool']
            if tt[0] == 'void':
                return 'callback-' + engineTypeToUnifiedType(tt[1].rstrip(')'))
            else:
                return 'predicate-' + engineTypeToUnifiedType(tt[1].rstrip(')'))
        elif t in validTypes:
            return t
        elif t[-1] == '*':
            tt = t[:-1]
            if tt in userObjects or tt in entityRelatives:
                return tt
            for ename, einfo in gameEntitiesInfo.items():
                if tt == einfo['Server'] or tt == einfo['Client']:
                    return ename
            if tt in ['ServerEntity', 'ClientEntity', 'Entity']:
                return 'Entity'
            assert False, tt
        else:
            def mapType(t):
                typeMap = {'char': 'int8', 'uchar': 'uint8', 'short': 'int16', 'ushort': 'uint16',
                        'int': 'int', 'uint': 'uint', 'int64': 'int64', 'uint64': 'uint64',
                        'char&': 'int8&', 'uchar&': 'uint8&', 'short&': 'int16&', 'ushort&': 'uint16&',
                        'int&': 'int&', 'uint&': 'uint&', 'int64&': 'int64&', 'uint64&': 'uint64&',
                        'float': 'float', 'double': 'double', 'float&': 'float&', 'double&': 'double&',
                        'bool': 'bool', 'bool&': 'bool&', 'hstring': 'hstring', 'hstring&': 'hstring&', 'void': 'void',
                        'string&': 'string&', 'const string&': 'string', 'string_view': 'string', 'string': 'string'}
                assert t in typeMap, 'Invalid engine type ' + t
                return typeMap[t]
            return mapType(t)

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
        assert r.strip()
        result.append(r.strip())
        return result

    def parseTypeTags():
        global codeGenTags
        
        for tagMeta in tagsMetas['ExportEnum']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                exportFlags = [s for s in (tagInfo.split(' ') if tagInfo else []) if s]
                
                firstLine = tagContext[0]
                assert firstLine.startswith('enum class ')
                sep = firstLine.find(':')
                grname = firstLine[len('enum class '):sep if sep != -1 else len(firstLine)].strip()
                utype = engineTypeToMetaType('int' if sep == -1 else firstLine[sep + 1:].strip())
                
                keyValues = []
                assert tagContext[1].startswith('{')
                for l in tagContext[2:]:
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

        for tagMeta in tagsMetas['Enum']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                tok = [s for s in tagInfo.split(' ') if s]
                grname = tok[0]
                key = tok[1] if len(tok) > 1 else None
                value = tok[3] if len(tok) > 3 and tok[2] == '=' else None
                flags = tok[2 if len(tok) < 3 or tok[2] != '=' else 4:]
                
                def calcUnderlyingMetaType(kv):
                    if kv:
                        minValue = 0xFFFFFFFF
                        maxValue = -0xFFFFFFFF
                        for i in kv:
                            v = int(i[1], 0)
                            minValue = v if v < minValue else minValue
                            maxValue = v if v > maxValue else maxValue
                        if minValue < 0:
                            return 'int'
                        else:
                            assert maxValue <= 0xFFFFFFFF
                            if maxValue <= 0xFF:
                                return 'uint8'
                            if maxValue <= 0xFFFF:
                                return 'uint16'
                            if maxValue <= 0x7FFFFFFF:
                                return 'int'
                            if maxValue <= 0xFFFFFFFF:
                                return 'uint'
                        assert False, 'Can\'t deduce enum underlying type (' + minValue + ', ' + maxValue + ')'
                    else:
                        return 'uint8'
                
                for g in codeGenTags['Enum']:
                    if g[0] == grname:
                        g[2].append((key, value if value is not None else str(int(g[2][-1][1], 0) + 1), []))
                        g[1] = calcUnderlyingMetaType(g[2])
                        break
                else:
                    keyValues = [(key, value if value is not None else '0', [])]
                    codeGenTags['Enum'].append([grname, calcUnderlyingMetaType(keyValues), keyValues, flags, comment])
                
                    assert grname not in validTypes, 'Enum already in valid types'
                    validTypes.add(grname)
                    assert grname not in scriptEnums, 'Enum already added'
                    scriptEnums.add(grname)
                
            except Exception as ex:
                showError('Invalid tag Enum', absPath + ' (' + str(lineIndex + 1) + ')', tagInfo, ex)

        for tagMeta in tagsMetas['ExportObject']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                exportFlags = tagInfo.split(' ') if tagInfo else []
                assert exportFlags and exportFlags[0] in ['Server', 'Client', 'Mapper', 'Common'], 'Expected target in tag info'
                target = exportFlags[0]
                exportFlags = exportFlags[1:]
                
                firstLine = tagContext[0]
                firstLineTok = [t for t in firstLine.split(' ') if t]
                assert len(firstLineTok) >= 2, 'Expected 4 or more tokens in struct'
                assert firstLineTok[0] == 'struct', 'Expected struct type'
                
                objName = firstLineTok[1]
                assert objName not in validTypes
                
                assert tagContext[1].startswith('{')
                
                thirdLine = tagContext[2].strip()
                assert thirdLine.startswith('SCRIPTABLE_OBJECT('), 'Expected SCRIPTABLE_OBJECT as first line inside class definition'
                
                fields = []
                for l in tagContext[3:]:
                    l = l.lstrip()
                    sep = l.find(' ')
                    assert sep != -1
                    lTok = [t for t in l.split(' ') if t]
                    fields.append((engineTypeToMetaType(lTok[0]), lTok[1], []))
                
                codeGenTags['ExportObject'].append((target, objName, fields, exportFlags, comment))
                
                validTypes.add(objName)
                userObjects.add(objName)
                
            except Exception as ex:
                showError('Invalid tag ExportObject', absPath + ' (' + str(lineIndex + 1) + ')', tagContext[0] if tagContext else '(empty)', ex)
        
        for tagMeta in tagsMetas['ExportEntity']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta

            try:
                exportFlags = [i for i in tagInfo.split(' ') if i]
                
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
                        'HasProto': 'HasProto' in exportFlags, 'HasStatics': 'HasStatics' in exportFlags}
                
                assert name + 'Property' not in validTypes
                validTypes.add(name + 'Property')
                assert name + 'Property' not in scriptEnums, 'Enum already added'
                scriptEnums.add(name + 'Property')
                
                if not gameEntitiesInfo[name]['IsGlobal']:
                    assert 'Abstract' + name not in validTypes
                    validTypes.add('Abstract' + name)
                    assert 'Abstract' + name not in entityRelatives
                    entityRelatives.add('Abstract' + name)
                
                if gameEntitiesInfo[name]['HasProto']:
                    assert name + 'Proto' not in validTypes
                    validTypes.add(name + 'Proto')
                    assert name + 'Proto' not in entityRelatives
                    entityRelatives.add(name + 'Proto')
                
                if gameEntitiesInfo[name]['HasStatics']:
                    assert 'Static' + name not in validTypes
                    validTypes.add('Static' + name)
                    assert 'Static' + name not in entityRelatives
                    entityRelatives.add('Static' + name)
                
            except Exception as ex:
                showError('Invalid tag ExportEntity', absPath + ' (' + str(lineIndex + 1) + ')', ex)
                
    def parseOtherTags():
        global codeGenTags
        
        for tagMeta in tagsMetas['ExportProperty']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
                
            try:    
                exportFlags = tagInfo.split(' ') if tagInfo else []
                
                entity = tagContext[:tagContext.find(' ')]
                assert entity in gameEntities, entity
                toks = [t.strip() for t in tagContext[tagContext.find('(') + 1:tagContext.find(')')].split(',')]
                access = toks[0]
                assert access in ['PrivateCommon', 'PrivateClient', 'PrivateServer', 'Public', 'PublicModifiable',
                        'PublicFullModifiable', 'PublicStatic', 'Protected', 'ProtectedModifiable', 'VirtualPrivateCommon',
                        'VirtualPrivateClient', 'VirtualPrivateServer', 'VirtualPublic', 'VirtualProtected'], 'Invalid export property access ' + access
                ptype = engineTypeToMetaType(toks[1])
                name = toks[2]
                
                codeGenTags['ExportProperty'].append((entity, access, ptype, name, exportFlags, comment))
                
            except Exception as ex:
                showError('Invalid tag ExportProperty', absPath + ' (' + str(lineIndex + 1) + ')', tagContext, ex)
                
        for tagMeta in tagsMetas['ExportMethod']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                exportFlags = tagInfo.split(' ') if tagInfo else []
                
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
                assert entity in gameEntities, entity
                name = funcTok[2]
                ret = engineTypeToMetaType(tagContext[tagContext.rfind(' ', 0, retSpace - 1):retSpace].strip())
                
                resultArgs = []
                for arg in splitEngineArgs(funcArgs)[1 if entity != 'Global' else 1:]:
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
                exportFlags = tagInfo.split(' ') if tagInfo else []
                
                target = None
                entity = None
                className = tagContext[:tagContext.find(' ')].strip()
                for ename, einfo in gameEntitiesInfo.items():
                    if className == einfo['Server'] or className == einfo['Client']:
                        target, entity = ('Server' if className == einfo['Server'] else 'Client'), ename
                        break
                if className == 'FOMapper':
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

        for tagMeta in tagsMetas['Property']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
             
            try:
                tok = [s.strip() for s in tagInfo.split(' ') if s.strip()]
                entity = tok[0]
                assert entity in gameEntities, entity
                access = tok[1]
                assert access in ['PrivateCommon', 'PrivateClient', 'PrivateServer', 'Public', 'PublicModifiable',
                        'PublicFullModifiable', 'PublicStatic', 'Protected', 'ProtectedModifiable', 'VirtualPrivateCommon',
                        'VirtualPrivateClient', 'VirtualPrivateServer', 'VirtualPublic', 'VirtualProtected'], 'Invalid property access ' + access
                if tok[2] == 'const':
                    ptype = unifiedTypeToMetaType(tok[3])
                    name = tok[4]
                    flags = ['ReadOnly'] + tok[5:]
                else:
                    ptype = unifiedTypeToMetaType(tok[2])
                    name = tok[3]
                    flags = tok[4:]
                
                codeGenTags['Property'].append((entity, access, ptype, name, flags, comment))
                
            except Exception as ex:
                showError('Invalid tag Property', absPath + ' (' + str(lineIndex + 1) + ')', tagInfo, ex)
                
        for tagMeta in tagsMetas['Event']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                tok = [s.strip() for s in tagInfo.split(' ') if s.strip()]
                
                target = tok[0]
                assert target in ['Server', 'Client', 'Mapper', 'Common'], target
                entity = tok[1]
                eventName = tok[2] if '(' not in tok[2] else tok[2][:tok[2].find('(')]
                
                braceOpenPos = tagInfo.find('(')
                braceClosePos = tagInfo.rfind(')')
                eventArgsStr = tagInfo[braceOpenPos + 1:braceClosePos].strip()
                
                eventArgs = []
                if eventArgsStr:
                    for arg in eventArgsStr.split(','):
                        arg = arg.strip()
                        sep = arg.rfind(' ')
                        assert sep != -1, 'No argaument name in event'
                        argType = unifiedTypeToMetaType(arg[:sep].strip())
                        argName = arg[sep + 1:].strip()
                        eventArgs.append((argType, argName))
                
                flags = [s for s in tagInfo[braceClosePos + 1:].split(' ') if s]

                codeGenTags['Event'].append((target, entity, eventName, eventArgs, flags, comment))
                
            except Exception as ex:
                showError('Invalid tag Event', absPath + ' (' + str(lineIndex + 1) + ')', tagInfo, ex)
                
        for tagMeta in tagsMetas['RemoteCall']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                tok = [s.strip() for s in tagInfo.split(' ') if s.strip()]
                target = tok[0]
                assert target in ['Server', 'Client'], target
                funcName = tok[1] if '(' not in tok[1] else tok[1][:tok[1].find('(')]
                
                if absPath.endswith('.fos'):
                    subsystem = 'AngelScript'
                elif absPath.endswith('.cs'):
                    subsystem = 'Mono'
                else:
                    assert False, absPath
                
                braceOpenPos = tagInfo.find('(')
                braceClosePos = tagInfo.rfind(')')
                funcArgsStr = tagInfo[braceOpenPos + 1:braceClosePos].strip()
                
                funcArgs = []
                if funcArgsStr:
                    for arg in funcArgsStr.split(','):
                        arg = arg.strip()
                        sep = arg.rfind(' ')
                        assert sep != -1, 'Name missed in remote call parameter'
                        argType = unifiedTypeToMetaType(arg[:sep].strip())
                        argName = arg[sep + 1:].strip()
                        funcArgs.append((argType, argName))
                
                flags = [s for s in tagInfo[braceClosePos + 1:].split(' ') if s]

                codeGenTags['RemoteCall'].append((target, subsystem, funcName, funcArgs, flags, comment))
            
            except Exception as ex:
                showError('Invalid tag RemoteCall', absPath + ' (' + str(lineIndex + 1) + ')', tagInfo, ex)
        
        for tagMeta in tagsMetas['ExportSettings']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                exportFlags = tagInfo.split(' ') if tagInfo else []
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
        
        for tagMeta in tagsMetas['Setting']:
            absPath, lineIndex, tagInfo, tagContext, comment = tagMeta
            
            try:
                tok = [s.strip() for s in tagInfo.split(' ') if s.strip()]
                stype = unifiedTypeToMetaType(tok[0])
                name = tok[1]
                value = tok[3] if len(tok) > 3 and tok[2] == '=' else None
                flags = tok[2 if len(tok) < 3 or tok[2] != '=' else 4:]
                
                codeGenTags['Setting'].append((stype, name, value, flags, comment))
                
            except Exception as ex:
                showError('Invalid tag Setting', absPath + ' (' + str(lineIndex + 1) + ')', tagInfo, ex)
                
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
                elif fname == 'DataRegistration-Template.cpp':
                    templateType = 'DataRegistration'
                elif fname == 'EntityProperties-Template.cpp':
                    templateType = 'EntityProperties'
                else:
                    assert False, fname
                
                flags = tagInfo.split(' ')
                assert len(flags) >= 1, 'Invalid CodeGen entry'
                
                codeGenTags['CodeGen'].append((templateType, absPath, flags[0], lineIndex, tagContext, flags[1:], comment))
            
            except Exception as ex:
                showError('Invalid tag CodeGen', absPath + ' (' + str(lineIndex + 1) + ')', tagInfo, ex)
    
    def postprocessTags():
        # Generate entity properties enums
        for entity in gameEntities:
            keyValues = [('Invalid', '0xFFFF', [])]
            index = 0
            for propTag in codeGenTags['ExportProperty'] + codeGenTags['Property']:
                ent, _, _, name, _, _ = propTag
                if ent == entity:
                    keyValues.append((name, str(index), []))
                    index += 1
            codeGenTags['Enum'].append([entity + 'Property', 'uint16', keyValues, [], []])
        
        # Check uniquiness of enums
        for e in codeGenTags['ExportEnum'] + codeGenTags['Enum']:
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
    
    parseTypeTags()
    parseOtherTags()
    postprocessTags()

parseTags()
checkErrors()
tagsMetas = {} # Cleanup memory

# Parse content
content = { 'foitem': [], 'focr': [], 'fomap': [], 'foloc': [], 'fodlg': [], 'fotxt': [] }

for contentDir in args.content:
    try:
        def collectFiles(dir):
            result = []
            for file in os.listdir(dir):
                fileParts = os.path.splitext(file)
                if len(fileParts) > 1 and fileParts[1][1:] in content:
                    result.append(dir + '/' + file)
            return result

        for file in collectFiles(contentDir):
            def getPidNames(file):
                result = [os.path.splitext(os.path.basename(file))[0]]
                with open(file, 'r', encoding='utf-8-sig') as f:
                    fileLines = f.readlines()
                for fileLine in fileLines:
                    if fileLine.startswith('$Name'):
                        result.append(fileLine[fileLine.find('=') + 1:].strip(' \r\n'))
                return result

            ext = os.path.splitext(os.path.basename(file))[1]
            content[ext[1:]].extend(getPidNames(file))
    
    except Exception as ex:
        showError('Can\'t process content dir ' + contentDir, ex)

checkErrors()

# Parse resources
resources = {}

for resourceEntry in args.resource:
    try:
        def collectFiles(dir, arcDir):
            result = []
            for entry in os.listdir(dir):
                entryPath = os.path.abspath(os.path.join(dir, entry))
                if os.path.isdir(entryPath):
                    result.extend(collectFiles(entryPath, arcDir + '/' + entry))
                elif os.path.isfile(entryPath):
                    result.append(((arcDir + '/' + entry).lstrip('/'), entryPath))
            return result        
        
        resType, resDir = resourceEntry.split(',', 1)
        resDir = os.path.abspath(resDir)
        
        if resType not in resources:
            resources[resType] = []
        
        if os.path.isdir(resDir):
            resources[resType].extend(collectFiles(resDir, ''))
        elif os.path.isfile(resDir):
            resources[resType].extend(resDir)
    
    except Exception as ex:
        showError('Can\'t process resources entry ' + resourceEntry, ex)

for key in resources.keys():
    resources[key].sort(key=lambda x: x[0])

checkErrors()

# Generate API
files = {}
lastFile = None

def createFile(name, output):
    assert output
    path = os.path.join(output, name)
    
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
    for path, lines in files.items():
        pathDir = os.path.dirname(path)
        if not os.path.isdir(pathDir):
            os.makedirs(pathDir)
        
        if os.path.isfile(path):
            with open(path, 'r', encoding='utf-8-sig') as f:
                if ''.join([l.rstrip('\r\n') for l in f.readlines()]).rstrip() == ''.join(lines).rstrip():
                    continue
        with open(path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines) + '\n')

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

def metaTypeToEngineType(t, target, passIn):
    tt = t.split('.')
    if tt[0] == 'dict':
        d2 = tt[2] if tt[2] != 'arr' else tt[2] + '.' + tt[3]
        r = 'map<' + metaTypeToEngineType(tt[1], target, False) + ', ' + metaTypeToEngineType(d2, target, False) + '>'
    elif tt[0] == 'arr':
        r = 'vector<' + metaTypeToEngineType(tt[1], target, False) + '>'
    elif tt[0] == 'callback':
        assert passIn
        r = 'std::function<void(' + metaTypeToEngineType(tt[1], target, False) + ')>'
    elif tt[0] == 'predicate':
        assert passIn
        r = 'std::function<bool(' + metaTypeToEngineType(tt[1], target, False) + ')>'
    elif tt[0] == 'init':
        assert passIn
        r = 'InitFunc<' + metaTypeToEngineType(tt[1], target, False) + '>'
    elif tt[0] == 'ObjInfo':
        return 'ObjInfo<' + tt[1] + '>'
    elif tt[0] == 'ScriptFuncName':
        return 'ScriptFuncName<' + ', '.join([metaTypeToEngineType(a, target, False) for a in '.'.join(tt[1:]).split('|')]) + '>'
    elif tt[0] == 'Entity':
        return 'ClientEntity*' if target != 'Server' else 'ServerEntity*'
    elif tt[0] in gameEntities:
        if target != 'Server':
            r = gameEntitiesInfo[tt[0]]['Client'] + '*'
            if tt[0] == 'Game' and target == 'Mapper':
                r = 'FOMapper*'
        else:
            r = gameEntitiesInfo[tt[0]]['Server'] + '*'
    elif tt[0] in scriptEnums:
        for e in codeGenTags['Enum']:
            if e[0] == tt[0]:
                return 'ScriptEnum_' + e[1]
        assert False, 'Enum not found ' + tt[0]
    elif tt[0] in userObjects or tt[0] in entityRelatives:
        r = tt[0] + '*'
    else:
        def mapType(mt):
            typeMap = {'int8': 'char', 'uint8': 'uchar', 'int16': 'short', 'uint16': 'ushort'}
            return typeMap[mt] if mt in typeMap else mt
        r = mapType(tt[0])
    if tt[-1] == 'ref':
        r += '&'
    elif passIn:
        if r == 'string':
            r = 'string_view'
        elif tt[0] in ['arr', 'dict', 'predicate', 'callback']:
            r = 'const ' + r + '&'
    return r

def genEntityProperties():
    globalLines = []

    globalLines.append('// Engine property indices')
    for entity in gameEntities:
        index = 0
        for propTag in codeGenTags['ExportProperty']:
            ent, _, _, name, _, _ = propTag
            if ent == entity:
                globalLines.append('ushort ' + entity + 'Properties::' + name + '_RegIndex = ' + str(index) + ';')
                index += 1
    globalLines.append('')

    createFile('EntityProperties-Common.cpp', args.genoutput)
    writeCodeGenTemplate('EntityProperties')
    insertCodeGenLines(globalLines, 'Body')

try:
    genEntityProperties()
except Exception as ex:
    showError('Code generation for data registration failed', ex)
checkErrors()
    
def genDataRegistration(target, isASCompiler):
    globalLines = []
    registerLines = []
    restoreLines = []
    propertyMapLines = []

    # Enums
    registerLines.append('// Enums')
    for e in codeGenTags['ExportEnum']:
        gname, utype, keyValues, _, _ = e
        registerLines.append('AddEnumGroup("' + gname + '", typeid(' + metaTypeToEngineType(utype, target, False) + '),')
        registerLines.append('{')
        for kv in keyValues:
            registerLines.append('    {"' + kv[0] + '", ' + kv[1] + '},')
        registerLines.append('});')
        registerLines.append('')
    if target != 'Client' or isASCompiler:
        for e in codeGenTags['Enum']:
            gname, utype, keyValues, _, _ = e
            registerLines.append('AddEnumGroup("' + gname + '", typeid(' + metaTypeToEngineType(utype, target, False) + '),')
            registerLines.append('{')
            for kv in keyValues:
                registerLines.append('    {"' + kv[0] + '", ' + kv[1] + '},')
            registerLines.append('});')
            registerLines.append('')
    
    # Property registrators
    registerLines.append('// Properties')
    for entity in gameEntities:
        registerLines.append('registrators["' + entity + '"] = CreatePropertyRegistrator("' + entity + '");')
    registerLines.append('')
    
    # Properties
    def getEnumFlags(t, ename = 'Enum'):
        tt = t.split('.')
        if tt[0] == 'dict':
            return getEnumFlags(tt[1], 'KeyEnum') + getEnumFlags('.'.join(tt[2:]))
        if tt[0] == 'arr':
            return getEnumFlags(tt[1])
        if tt[0] in scriptEnums or tt[0] in engineEnums:
            return [ename, '=', tt[0]]
        return []
    for entity in gameEntities:
        registerLines.append('registrator = registrators["' + entity + '"];')
        for propTag in codeGenTags['ExportProperty']:
            ent, access, type, name, flags, _ = propTag
            if ent == entity:
                registerLines.append('RegisterProperty<' + metaTypeToEngineType(type, target, False) + '>(registrator, Property::AccessType::' +
                        access + ', "' + name + '", {' + ', '.join(['"' + f + '"' for f in flags + getEnumFlags(type) if f]) + '});')
        if target != 'Client' or isASCompiler:
            for propTag in codeGenTags['Property']:
                ent, access, type, name, flags, _ = propTag
                if ent == entity:
                    registerLines.append('RegisterProperty<' + metaTypeToEngineType(type, target, False) + '>(registrator, Property::AccessType::' +
                            access + ', "' + name + '", {' + ', '.join(['"' + f + '"' for f in flags + getEnumFlags(type) if f]) + '});')
        registerLines.append('')
    
    # Restore enums info
    if target == 'Server' and not isASCompiler:
        restoreLines.append('restoreInfo["Enums"] =')
        restoreLines.append('{')
        for e in codeGenTags['Enum']:
            gname, utype, keyValues, _, _ = e
            if keyValues:
                restoreLines.append('    "' + gname + ' ' + utype + '"')
                for kv in keyValues[:-1]:
                    restoreLines.append('    " ' + kv[0] + '=' + kv[1] + '"')
                restoreLines.append('    " ' + keyValues[-1][0] + '=' + keyValues[-1][1] + '",')
            else:
                restoreLines.append('    "' + gname + ' ' + utype + '",')
        restoreLines.append('};')
        restoreLines.append('')
    
    # Restore properties info
    if target == 'Server' and not isASCompiler:
        def replaceFakedEnum(t):
            tt = t.split('.')
            if tt[0] == 'dict':
                return 'dict.' + replaceFakedEnum(tt[1]) + replaceFakedEnum('.'.join(tt[2:]))
            if tt[0] == 'arr':
                return 'arr.' + replaceFakedEnum(tt[1])
            if tt[0] in scriptEnums or tt[0] in engineEnums:
                return metaTypeToEngineType(tt[0], target, False)
            return tt[0]
        dummyIndex = 0
        restoreLines.append('restoreInfo["Properties"] =')
        restoreLines.append('{')
        for e in codeGenTags['Property']:
            entity, access, type, name, flags, _ = e
            if access not in ['PrivateServer', 'VirtualPrivateServer']:
                allFlags = flags + getEnumFlags(type)
                restoreLines.append('    "' + entity + ' ' + access + ' ' + replaceFakedEnum(type) + ' ' + name + (' ' if allFlags else '') + ' '.join(allFlags) + '",')
            else:
                restoreLines.append('    "' + entity + ' ' + access + ' int __dummy' + str(dummyIndex) + '",')
                dummyIndex += 1
        restoreLines.append('};')
        restoreLines.append('')
    
    # Todo: make script events restorable
    # Restore events info
    #if target == 'Server' and not isASCompiler:
    #    restoreLines.append('restoreInfo["Events"] =')
    #    restoreLines.append('{')
    #    for entity in gameEntities:
    #        for evTag in codeGenTags['Event']:
    #            targ, ent, evName, evArgs, evFlags, _ = evTag
    #            if targ in ['Client', 'Common'] and ent == entity:
    #                restoreLines.append('    "' + entity + ' ' + evName + ' ' + ' '.join([a[0] + ' ' + (a[1] if a[1] else '_') for a in evArgs]) +
    #                        (' | ' if evFlags else '') + ' '.join(evFlags) + '",')
    #    restoreLines.append('};')
    #    restoreLines.append('')
    
    # Todo: make setting values restorable
    # Restore settings
    #if target == 'Server' and not isASCompiler:
    #    restoreLines.append('restoreInfo["Settings"] =')
    #    restoreLines.append('{')
    #    for settTag in codeGenTags['ExportSettings']:
    #        grName, targ, settings, flags, _ = settTag
    #        if targ in ['Common', 'Client']:
    #            for sett in settings:
    #                fixOrVar, keyType, keyName, initValues, _ = sett
    #                restoreLines.append('    "' + grName + ' ' + fixOrVar + ' ' + keyType + ' ' + keyName + (' ' if flags else '') + ' '.join(flags) + (' |' + ' '.join(initValues) if initValues else '') + '",')
    #    for settTag in codeGenTags['Setting']:
    #        type, name, initValue, flags, _ = settTag
    #        restoreLines.append('    "Script fix ' + type + ' ' + name + (' ' if flags else '') + ' '.join(flags) + (' |' + initValue if initValue is not None else '') + '",')
    #    restoreLines.append('};')
    #    restoreLines.append('')
        
    # Property map
    if target == 'Client' and not isASCompiler:
        def addPropMapEntry(e):
            propertyMapLines.append('{ "' + e + '", [](RESTORE_ARGS) { RegisterProperty<' + metaTypeToEngineType(e, target, False) + '>(RESTORE_ARGS_PASS); } },')
        fakedEnums = ['ScriptEnum_uint8', 'ScriptEnum_uint16', 'ScriptEnum_int', 'ScriptEnum_uint']
        for t in ['int8', 'int16', 'int', 'int64', 'uint8', 'uint16', 'uint', 'uint64', 'float', 'double', 'bool', 'string', 'hstring'] + fakedEnums:
            addPropMapEntry(t)
            addPropMapEntry('arr.' + t)
        for tKey in ['int8', 'int16', 'int', 'int64', 'uint8', 'uint16', 'uint', 'uint64', 'hstring'] + fakedEnums:
            for t in ['int8', 'int16', 'int', 'int64', 'uint8', 'uint16', 'uint', 'uint64', 'float', 'double', 'bool', 'hstring', 'string'] + fakedEnums:
                addPropMapEntry('dict.' + tKey + '.' + t)
                addPropMapEntry('dict.' + tKey + '.arr.' + t)
    
    createFile('DataRegistration-' + target + ('Compiler' if isASCompiler else '') + '.cpp', args.genoutput)
    writeCodeGenTemplate('DataRegistration')
    
    if not isASCompiler:
        if target == 'Server':
            insertCodeGenLines(restoreLines, 'WriteRestoreInfo')
            insertCodeGenLines(registerLines, 'ServerRegister')
            insertCodeGenLines(globalLines, 'Global')
        elif target == 'Client':
            insertCodeGenLines(registerLines, 'ClientRegister')
            insertCodeGenLines(propertyMapLines, 'PropertyMap')
            insertCodeGenLines(globalLines, 'Global')
        elif target == 'Mapper':
            insertCodeGenLines(registerLines, 'MapperRegister')
            insertCodeGenLines(globalLines, 'Global')
        elif target == 'Baker':
            insertCodeGenLines(registerLines, 'BakerRegister')
            insertCodeGenLines(globalLines, 'Global')
    else:
        insertCodeGenLines(registerLines, 'CompilerRegister')
        insertCodeGenLines(globalLines, 'Global')
    
    if target == 'Server':
        insertCodeGenLines(['#define SERVER_REGISTRATION 1', '#define CLIENT_REGISTRATION 0',
                '#define MAPPER_REGISTRATION 0', '#define BAKER_REGISTRATION 0', '#define COMPILER_MODE ' + ('1' if isASCompiler else '0')], 'Defines')
    elif target == 'Client':
        insertCodeGenLines(['#define SERVER_REGISTRATION 0', '#define CLIENT_REGISTRATION 1',
                '#define MAPPER_REGISTRATION 0', '#define BAKER_REGISTRATION 0', '#define COMPILER_MODE ' + ('1' if isASCompiler else '0')], 'Defines')
    elif target == 'Mapper':
        insertCodeGenLines(['#define SERVER_REGISTRATION 0', '#define CLIENT_REGISTRATION 0',
                '#define MAPPER_REGISTRATION 1', '#define BAKER_REGISTRATION 0', '#define COMPILER_MODE ' + ('1' if isASCompiler else '0')], 'Defines')
    elif target == 'Baker':
        assert not isASCompiler
        insertCodeGenLines(['#define SERVER_REGISTRATION 0', '#define CLIENT_REGISTRATION 0',
                '#define MAPPER_REGISTRATION 0', '#define BAKER_REGISTRATION 1', '#define COMPILER_MODE 0'], 'Defines')

try:
    genDataRegistration('Baker', False)
    
    if args.multiplayer:
        for target in ['Server', 'Client', 'Mapper']:
            genDataRegistration(target, False)
            if args.angelscript:
                genDataRegistration(target, True)
    
except Exception as ex:
    showError('Code generation for data registration failed', ex)
    
checkErrors()

def genCode(lang, target, isASCompiler=False):
    createFile(lang + 'Scripting' + '-' + target + ('Compiler' if isASCompiler else '') + '.cpp', args.genoutput)
    
    # Make stub
    if (target == 'Single' and not args.singleplayer) or \
            ((target == 'Client' or target == 'Server') and not args.multiplayer) or \
            (lang == 'AngelScript' and not args.angelscript) or \
            (lang == 'Mono' and not args.csharp) or \
            (lang == 'Native' and not args.native):
        if isASCompiler:
            writeFile('struct ' + target + 'ScriptSystem { void InitAngelScriptScripting(const char*); };')
            writeFile('void ' + target + 'ScriptSystem::InitAngelScriptScripting(const char*) { }')
        else:
            writeFile('#include "' + target + 'Scripting.h"')
            writeFile('void ' + target + 'ScriptSystem::Init' + lang + 'Scripting() { }')
            
        return
    
    # Generate from template
    writeCodeGenTemplate(lang)
    
    # lang, absPath, entry, line, flags, comment = genTag    
    if lang == 'AngelScript':
        def metaTypeToASEngineType(t, ret = False):
            tt = t.split('.')
            if tt[0] == 'arr':
                return 'CScriptArray*'
            elif tt[0] == 'dict':
                return 'CScriptDict*'
            elif tt[0] in ['callback', 'predicate', 'init']:
                return 'asIScriptFunction*'
            elif tt[0] == 'Entity':
                return 'ClientEntity*' if target != 'Server' else 'ServerEntity*'
            elif tt[0] in gameEntities:
                if target != 'Server':
                    return gameEntitiesInfo[tt[0]]['Client'] + '*'
                else:
                    return gameEntitiesInfo[tt[0]]['Server'] + '*'
            elif tt[0] in userObjects or tt[0] in entityRelatives:
                return tt[0] + '*'
            elif tt[0] in scriptEnums:
                for e in codeGenTags['Enum']:
                    if e[0] == tt[0]:
                        return 'ScriptEnum_' + e[1]
                assert False, 'Enum not found ' + tt[0]
            elif tt[0] == 'ObjInfo':
                return '[[maybe_unused]] void* obj' + tt[1] + 'Ptr, int'
            elif tt[0] == 'ScriptFuncName':
                return 'asIScriptFunction*'
            else:
                def mapType(t):
                    typeMap = {'int8': 'char', 'uint8': 'uchar', 'int16': 'short', 'uint16': 'ushort'}
                    return typeMap[t] if t in typeMap else t
                r = mapType(tt[0])
            if tt[-1] == 'ref':
                r += '&'
            return r
            
        def metaTypeToASType(t, noHandle = False, isRet = False):
            def getHandle():
                return '' if noHandle else ('@' if isRet else '@+')
            tt = t.split('.')
            if tt[0] == 'dict':
                d2 = tt[2] if tt[2] != 'arr' else tt[2] + '.' + tt[3]
                return 'dict<' + metaTypeToASType(tt[1], True) + ', ' + metaTypeToASType(d2, True) + '>' + getHandle()
            elif tt[0] == 'arr':
                return metaTypeToASType(tt[1], True) + '[]' + getHandle()
            elif tt[0] == 'callback':
                return metaTypeToASType(tt[1], True) + 'Callback' + getHandle()
            elif tt[0] == 'predicate':
                return metaTypeToASType(tt[1], True) + 'Predicate' + getHandle()
            elif tt[0] == 'init':
                return metaTypeToASType(tt[1], True) + 'InitFunc' + getHandle()
            elif tt[0] == 'Entity':
                return 'Entity' + getHandle()
            elif tt[0] in gameEntities:
                return tt[0] + getHandle()
            elif tt[0] in engineEnums or tt[0] in scriptEnums:
                return tt[0]
            elif tt[0] in userObjects or tt[0] in entityRelatives:
                return tt[0] + getHandle()
            elif tt[0] == 'ObjInfo':
                return '?&in'
            elif tt[0] == 'ScriptFuncName':
                assert len(tt) > 1, 'Invalid generic function'
                return 'Generic_' + '.'.join(tt[1:]).replace('|', '_').replace('.', '_') + '_Func' + getHandle()
            else:
                return tt[0] if tt[-1] != 'ref' else tt[0] + '&'
    
        def marshalIn(t, v):
            tt = t.split('.')
            if tt[0] == 'dict':
                d2 = tt[2] if tt[2] != 'arr' else tt[2] + '.' + tt[3]
                return 'MarshalDict<' + metaTypeToEngineType(tt[1], target, False) + ', ' + metaTypeToEngineType(d2, target, False) + '>(GET_AS_ENGINE_FROM_SELF(), ' + v + ')'
            elif tt[0] == 'arr':
                return 'MarshalArray<' + metaTypeToEngineType(tt[1], target, False) + '>(GET_AS_ENGINE_FROM_SELF(), ' + v + ')'
            elif tt[0] == 'callback' or tt[0] == 'predicate':
                return 'nullptr' # metaTypeToEngineType(tt[1], target, False)
            elif tt[0] == 'init' or tt[0] == 'predicate':
                return 'hstring()' # metaTypeToEngineType(tt[1], target, False)
            elif tt[0] == 'ObjInfo':
                return 'GetASObjectInfo(' + v + 'Ptr, ' + v + ')'
            elif tt[0] == 'ScriptFuncName':
                return 'GetASFuncName(' + v + ')'
            return v
            
        def marshalBack(t, v):
            tt = t.split('.')
            if tt[0] == 'dict':
                d2 = tt[2] if tt[2] != 'arr' else tt[2] + '.' + tt[3]
                return 'MarshalBackScalarDict<' + metaTypeToEngineType(tt[1], target, False) + ', ' + metaTypeToEngineType(d2, target, False) + \
                        '>(GET_AS_ENGINE_FROM_SELF(), "dict<' + metaTypeToASType(tt[1], True) + ', ' + metaTypeToASType(d2, True) + '>", ' + v + ')'
            elif tt[0] == 'arr':
                if tt[1] in gameEntities or tt[1] in userObjects or tt[1] in entityRelatives:
                    return 'MarshalBackRefArray<' + metaTypeToEngineType(tt[1], target, False) + '>(GET_AS_ENGINE_FROM_SELF(), "' + metaTypeToASType(tt[1], True) + '[]", ' + v + ')'
                return 'MarshalBackScalarArray<' + metaTypeToEngineType(tt[1], target, False) + '>(GET_AS_ENGINE_FROM_SELF(), "' + metaTypeToASType(tt[1], True) + '[]", ' + v + ')'
            elif tt[0] == 'callback' or tt[0] == 'predicate':
                return 'nullptr; // Todo: !!!' # metaTypeToEngineType(tt[1], target, False)
            return v
        
        def nameMangling(params):
            if len(params):
                return ''.join([''.join([p2[0] + p2[-1] for p2 in p[0].split('.')]) for p in params])
            return '0'
        
        allowedTargets = ['Common', target] + (['Client' if target == 'Mapper' else ''])
        
        defineLines = []
        storageLines = []
        globalLines = []
        registerLines = []
        
        defineLines.append('#define SERVER_SCRIPTING ' + ('1' if target == 'Server' else '0'))
        defineLines.append('#define CLIENT_SCRIPTING ' + ('1' if target == 'Client' else '0'))
        defineLines.append('#define MAPPER_SCRIPTING ' + ('1' if target == 'Mapper' else '0'))
        defineLines.append('#define COMPILER_MODE ' + ('1' if isASCompiler else '0'))
        
        # Storage
        storageLines.append('ASGlobal Global;')
        
        # Scriptable objects and entity stubs
        if isASCompiler:
            globalLines.append('// Compiler entity stubs')
            for entity in gameEntities:
                engineEntityType = gameEntitiesInfo[entity]['Client' if target != 'Server' else 'Server']
                globalLines.append('struct ' + engineEntityType + ' : BaseEntity { };')
                if gameEntitiesInfo[entity]['HasStatics']:
                    globalLines.append('struct Static' + entity + ' : BaseEntity { };')
            globalLines.append('')
            
            globalLines.append('// Scriptable objects')
            for eo in codeGenTags['ExportObject']:
                targ, objName, fields, _, _ = eo
                if targ in allowedTargets:
                    globalLines.append('struct ' + objName)
                    globalLines.append('{')
                    globalLines.append('    void AddRef() { }')
                    globalLines.append('    void Release() { }')
                    globalLines.append('    int RefCounter;')
                    for f in fields:
                        globalLines.append('    ' + metaTypeToEngineType(f[0], target, False) + ' ' + f[1] + ';')
                    globalLines.append('};')
                    globalLines.append('')
            
        # Marshalling functions
        def writeMarshalingMethod(entity, targ, name, ret, params, isASGlobal):
            ident = '    ' if isASGlobal else ''
            engineEntityType = gameEntitiesInfo[entity]['Client' if target != 'Server' else 'Server']
            engineEntityTypeExtern = engineEntityType
            
            if entity == 'Game':
                if target == 'Mapper':
                    engineEntityType = 'FOMapper'
                if targ == 'Common':
                    engineEntityTypeExtern = 'FOEngineBase'
                elif targ == 'Mapper':
                    engineEntityTypeExtern = 'FOMapper'
            
            globalLines.append(ident + ('static ' if not isASGlobal else '') + metaTypeToASEngineType(ret, True) +
                    ' AS_' + targ + '_' + entity + '_' + name + '_' + nameMangling(params) + '(' +
                    (engineEntityType + '* self' + (', ' if params else '') if not isASGlobal else '') +
                    ', '.join([metaTypeToASEngineType(p[0]) + ' ' + p[1] for p in params]) +')')
            globalLines.append(ident + '{')
            
            if not isASCompiler:
                globalLines.append(ident + '    ENTITY_VERIFY(self);')
                for p in params:
                    if p[0] in gameEntities:
                        globalLines.append(ident + '    ENTITY_VERIFY(' + p[1] + ');')
                for p in params:
                    globalLines.append(ident + '    auto&& in_' + p[1] + ' = ' + marshalIn(p[0], p[1]) + ';')
                globalLines.append(ident + '    extern ' + metaTypeToEngineType(ret, target, False) + ' ' + targ + '_' + entity + '_' + name +
                        '(' + engineEntityTypeExtern + '*' + (', ' if params else '') + ', '.join([metaTypeToEngineType(p[0], target, True) for p in params]) + ');')
                globalLines.append(ident + '    ' + ('auto out_result = ' if ret != 'void' else '') + targ + '_' + entity + '_' + name +
                        '(self' + (', ' if params else '') + ', '.join(['in_' + p[1] for p in params]) + ');')
                for p in params:
                    pass # Marshall back
                if ret != 'void':
                    globalLines.append(ident + '    return ' + marshalBack(ret, 'out_result') + ';')
            else:
                # Stub for compiler
                globalLines.append(ident + '    UNUSED_VARIABLE(self);')
                for p in params:
                    globalLines.append(ident + '    UNUSED_VARIABLE(' + p[1] + ');')
                globalLines.append(ident + '    throw ScriptCompilerException("Stub");')
            
            globalLines.append(ident + '}')
            globalLines.append('')
            
        globalLines.append('// Marshalling entity methods')
        for entity in gameEntities:
            if not gameEntitiesInfo[entity]['IsGlobal']:
                for methodTag in codeGenTags['ExportMethod']:
                    targ, ent, name, ret, params, exportFlags, comment = methodTag
                    if targ in allowedTargets and ent == entity:
                        writeMarshalingMethod(entity, targ, name, ret, params, False)
        
        globalLines.append('// Marshalling global functions')
        globalLines.append('struct ASGlobal')
        globalLines.append('{')
        for entity in gameEntities:
            if gameEntitiesInfo[entity]['IsGlobal']:
                for methodTag in codeGenTags['ExportMethod']:
                    targ, ent, name, ret, params, exportFlags, comment = methodTag
                    if targ in allowedTargets and ent == entity:
                        writeMarshalingMethod(entity, targ, name, ret, params, True)
        globalLines.append('    FOEngine* self = nullptr;')
        globalLines.append('};')
        globalLines.append('')
        
        # Marshal events
        globalLines.append('// Marshalling events')
        for entity in gameEntities:
            for evTag in codeGenTags['ExportEvent'] + codeGenTags['Event']:
                targ, ent, evName, evArgs, evFlags, _ = evTag
                if targ in allowedTargets and ent == entity:
                    isGlobal = gameEntitiesInfo[entity]['IsGlobal']
                    isExported = evTag in codeGenTags['ExportEvent']
                    funcEntry = 'ASEvent_' + entity + '_' + evName
                    entityArg = metaTypeToEngineType(entity, target, True) + ' self'
                    if not isExported:
                        globalLines.append('static string ' + funcEntry + '_Name = "' + evName + '";')
                    globalLines.append('static void ' + funcEntry + '_Subscribe(' + entityArg + ', asIScriptFunction* func)')
                    globalLines.append('{')
                    if not isASCompiler:
                        globalLines.append('    ENTITY_VERIFY(self);')
                        globalLines.append('    auto event_data = Entity::EventCallbackData();')
                        globalLines.append('    event_data.SubscribtionPtr = (func->GetFuncType() == asFUNC_DELEGATE ? func->GetDelegateFunction() : func);')
                        globalLines.append('    event_data.Callback = [func, self](const initializer_list<void*>& args) {')
                        argIndex = 0
                        for p in evArgs:
                            argType = metaTypeToEngineType(p[0], target, False)
                            if argType[-1] == '&':
                                argType = argType[:-1]
                            globalLines.append('        auto&& arg_' + p[1] + ' = ' +
                                    marshalBack(p[0], '*reinterpret_cast<' + argType + '*>(const_cast<void*>(*(args.begin() + ' + str(argIndex) + ')))') + ';') 
                            argIndex += 1
                        globalLines.append('        auto* script_sys = GET_SCRIPT_SYS_FROM_AS_ENGINE(func->GetEngine());')
                        globalLines.append('        auto* ctx = script_sys->PrepareContext(func);')
                        if not isGlobal:
                            globalLines.append('        ctx->SetArgObject(0, self);')
                        setIndex = 0 if isGlobal else 1
                        for p in evArgs:
                            tt = p[0].split('.')
                            if tt[-1] == 'ref':
                                globalLines.append('        ctx->SetArgAddress(' + str(setIndex) + ', &arg_' + p[1] + ');')
                            elif tt[0] == 'dict' or tt[0] == 'arr' or p[0] == 'Entity' or p[0] in gameEntities or p[0] in userObjects:
                                globalLines.append('        ctx->SetArgObject(' + str(setIndex) + ', arg_' + p[1] + ');')
                            elif p[0] in entityRelatives:
                                globalLines.append('        ctx->SetArgObject(' + str(setIndex) + ', (void*)arg_' + p[1] + ');')
                            elif p[0] in ['string']:
                                globalLines.append('        ctx->SetArgObject(' + str(setIndex) + ', &arg_' + p[1] + ');')
                            elif p[0] in ['int8', 'uint8', 'int16', 'uint16', 'int', 'uint', 'int64', 'uint64', 'bool', 'float', 'double', 'hstring'] or p[0] in engineEnums or p[0] in scriptEnums:
                                globalLines.append('        memcpy(ctx->GetAddressOfArg(' + str(setIndex) + '), &arg_' + p[1] + ', sizeof(arg_' + p[1] + '));')
                            else:
                                globalLines.append('        static_assert(false, "Invalid configuration");')
                            setIndex += 1
                        globalLines.append('        auto event_result = true;')
                        globalLines.append('        if (script_sys->RunContext(ctx, func->GetReturnTypeId() == asTYPEID_VOID)) {')
                        globalLines.append('            event_result = (func->GetReturnTypeId() == asTYPEID_VOID || (func->GetReturnTypeId() == asTYPEID_BOOL && ctx->GetReturnByte() != 0));')
                        globalLines.append('            // Todo: marshal back before context returned')
                        globalLines.append('            UNUSED_VARIABLE(self);')
                        globalLines.append('            script_sys->ReturnContext(ctx);')
                        globalLines.append('        }')
                        globalLines.append('        return event_result;')
                        globalLines.append('    };')
                        if isExported:
                            globalLines.append('    self->On' + evName + '.Subscribe(std::move(event_data));')
                        else:
                            globalLines.append('    self->SubscribeEvent(' + funcEntry + '_Name, std::move(event_data));')
                    else:
                        globalLines.append('    UNUSED_VARIABLE(self);')
                        globalLines.append('    UNUSED_VARIABLE(func);')
                        globalLines.append('    throw ScriptCompilerException("Stub");')
                    globalLines.append('}')
                    globalLines.append('static void ' + funcEntry + '_Unsubscribe(' + entityArg + ', asIScriptFunction* func)')
                    globalLines.append('{')
                    if not isASCompiler:
                        globalLines.append('    ENTITY_VERIFY(self);')
                        if isExported:
                            globalLines.append('    self->On' + evName + '.Unsubscribe(func->GetFuncType() == asFUNC_DELEGATE ? func->GetDelegateFunction() : func);')
                        else:
                            globalLines.append('    self->UnsubscribeEvent(' + funcEntry + '_Name, func->GetFuncType() == asFUNC_DELEGATE ? func->GetDelegateFunction() : func);')
                    else:
                        globalLines.append('    UNUSED_VARIABLE(self);')
                        globalLines.append('    UNUSED_VARIABLE(func);')
                        globalLines.append('    throw ScriptCompilerException("Stub");')
                    globalLines.append('}')
                    globalLines.append('static void ' + funcEntry + '_UnsubscribeAll(' + entityArg + ')')
                    globalLines.append('{')
                    if not isASCompiler:
                        globalLines.append('    ENTITY_VERIFY(self);')
                        if isExported:
                            globalLines.append('    self->On' + evName + '.UnsubscribeAll();')
                        else:
                            globalLines.append('    self->UnsubscribeAllEvent(' + funcEntry + '_Name);')
                    else:
                        globalLines.append('    UNUSED_VARIABLE(self);')
                        globalLines.append('    throw ScriptCompilerException("Stub");')
                    globalLines.append('}')
                    if not isExported:
                        globalLines.append('static bool ' + funcEntry + '_Fire(' + entityArg + (', ' if evArgs else '') + ', '.join([metaTypeToASEngineType(p[0]) + ' ' + p[1] for p in evArgs]) + ')')
                        globalLines.append('{')
                        if not isASCompiler:
                            globalLines.append('    ENTITY_VERIFY(self);')
                            for p in evArgs:
                                if p[0] in gameEntities:
                                    globalLines.append('    ENTITY_VERIFY(' + p[1] + ');')
                            for p in evArgs:
                                globalLines.append('    auto&& in_' + p[1] + ' = ' + marshalIn(p[0], p[1]) + ';')
                            if isExported:
                                globalLines.append('    return self->On' + evName + '.Fire(' + ', '.join(['in_' + p[1] for p in evArgs]) + ');')
                            else:
                                globalLines.append('    return self->FireEvent(' + funcEntry + '_Name, {' + ', '.join(['&in_' + p[1] for p in evArgs]) + '});')
                        else:
                            globalLines.append('    UNUSED_VARIABLE(self);')
                            for p in evArgs:
                                globalLines.append('    UNUSED_VARIABLE(' + p[1] + ');')
                            globalLines.append('    throw ScriptCompilerException("Stub");')
                        globalLines.append('}')
                    globalLines.append('')
        
        # Marshal settings
        globalLines.append('// Marshalling settings')
        settEntity = metaTypeToEngineType('Game', target, False) + ' self'
        for settTag in codeGenTags['ExportSettings']:
            grName, targ, settings, flags, _ = settTag
            if targ in allowedTargets:
                for sett in settings:
                    fixOrVar, keyType, keyName, initValues, _ = sett
                    globalLines.append('static ' + metaTypeToASEngineType(keyType, True) + ' ASSetting_Get_' + keyName + '(' + settEntity + ')')
                    globalLines.append('{')
                    if not isASCompiler:
                        globalLines.append('    return ' + marshalBack(keyType, 'self->Settings.' + keyName) + ';')
                    else:
                        globalLines.append('    UNUSED_VARIABLE(self);')
                        globalLines.append('    throw ScriptCompilerException("Stub");')
                    globalLines.append('}')
                    if fixOrVar == 'var':
                        globalLines.append('static void ASSetting_Set_' + keyName + '(' + settEntity + ', ' + metaTypeToASEngineType(keyType, False) + ' value)')
                        globalLines.append('{')
                        if not isASCompiler:
                            globalLines.append('    self->Settings.' + keyName + ' = ' + marshalIn(keyType, 'value') + ';')
                        else:
                            globalLines.append('    UNUSED_VARIABLE(self);')
                            globalLines.append('    UNUSED_VARIABLE(value);')
                            globalLines.append('    throw ScriptCompilerException("Stub");')
                        globalLines.append('}')
                    globalLines.append('')
        for settTag in codeGenTags['Setting']:
            type, name, initValue, flags, _ = settTag
            globalLines.append('static ' + metaTypeToASEngineType(type, True) + ' ASSetting_Get_' + name + '(' + settEntity + ')')
            globalLines.append('{')
            if not isASCompiler:
                globalLines.append('    return ' + marshalBack(type, 'std::any_cast<' + metaTypeToEngineType(type, target, False) + '&>(GET_SCRIPT_SYS_FROM_SELF()->SettingsStorage["' + name + '"])') + ';')
            else:
                globalLines.append('    UNUSED_VARIABLE(self);')
                globalLines.append('    throw ScriptCompilerException("Stub");')
            globalLines.append('}')
            globalLines.append('static void ASSetting_Set_' + name + '(' + settEntity + ', ' + metaTypeToASEngineType(type, False) + ' value)')
            globalLines.append('{')
            if not isASCompiler:
                globalLines.append('    GET_SCRIPT_SYS_FROM_SELF()->SettingsStorage["' + name + '"] = ' + marshalIn(type, 'value') + ';')
            else:
                globalLines.append('    UNUSED_VARIABLE(self);')
                globalLines.append('    UNUSED_VARIABLE(value);')
                globalLines.append('    throw ScriptCompilerException("Stub");')
            globalLines.append('}')
            globalLines.append('')
        
        # Remote calls dispatchers
        if target in ['Server', 'Client']:
            globalLines.append('// Remote calls dispatchers')
            for rcTag in codeGenTags['RemoteCall']:
                targ, subsystem, rcName, rcArgs, rcFlags, _ = rcTag
                if targ == ('Client' if target == 'Server' else 'Server') and subsystem == 'AngelScript':
                    selfArg = 'Player* self' if target == 'Server' else 'FOClient* self'
                    globalLines.append('static void ASRemoteCall_' + rcName + '(' + selfArg + (', ' if rcArgs else '') + ', '.join([metaTypeToASEngineType(p[0]) + ' ' + p[1] for p in rcArgs]) + ')')
                    globalLines.append('{')
                    if not isASCompiler:
                        pass
                    else:
                        globalLines.append('    UNUSED_VARIABLE(self);')
                        for p in rcArgs:
                            globalLines.append('    UNUSED_VARIABLE(' + p[1] + ');')
                        globalLines.append('    throw ScriptCompilerException("Stub");')
                    globalLines.append('}')
                    globalLines.append('')
            globalLines.append('')
        
        # Register exported objects
        registerLines.append('// Exported objects')
        for eo in codeGenTags['ExportObject']:
            targ, objName, fields, _, _ = eo
            if targ in allowedTargets:
                registerLines.append('static_assert(std::is_standard_layout_v<' + objName + '>, "' + objName + ' is not standart layout type");')
                registerLines.append('AS_VERIFY(engine->RegisterObjectType("' + objName + '", sizeof(' + objName + '), asOBJ_REF));')
                registerLines.append('AS_VERIFY(engine->RegisterObjectBehaviour("' + objName + '", asBEHAVE_ADDREF, "void f()", SCRIPT_METHOD(' + objName + ', AddRef), SCRIPT_METHOD_CONV));')
                registerLines.append('AS_VERIFY(engine->RegisterObjectBehaviour("' + objName + '", asBEHAVE_RELEASE, "void f()", SCRIPT_METHOD(' + objName + ', Release), SCRIPT_METHOD_CONV));')
                registerLines.append('AS_VERIFY(engine->RegisterObjectBehaviour("' + objName + '", asBEHAVE_FACTORY, "' + objName + '@ f()", SCRIPT_FUNC((ScriptableObject_Factory<' + objName + '>)), SCRIPT_FUNC_CONV));')
                registerLines.append('AS_VERIFY(engine->RegisterObjectProperty("' + objName + '", "const int RefCounter", offsetof(' + objName + ', RefCounter)));')
                for f in fields:
                    registerLines.append('AS_VERIFY(engine->RegisterObjectProperty("' + objName + '", "' + metaTypeToASType(f[0], True) + ' ' + f[1] + '", offsetof(' + objName + ', ' + f[1] + ')));')
                registerLines.append('')
        
        # Register entities
        registerLines.append('// Register entities')
        for entity in gameEntities:
            engineEntityType = gameEntitiesInfo[entity]['Client' if target != 'Server' else 'Server']
            if gameEntitiesInfo[entity]['IsGlobal']:
                registerLines.append('REGISTER_GLOBAL_ENTITY("' + entity + '", ' + engineEntityType + ');')
            else:
                registerLines.append('REGISTER_ENTITY("' + entity + '", ' + engineEntityType + ');')
            if gameEntitiesInfo[entity]['HasProto']:
                assert not gameEntitiesInfo[entity]['IsGlobal']
                registerLines.append('REGISTER_ENTITY_PROTO("' + entity + '", ' + entity + 'Proto);')
            if gameEntitiesInfo[entity]['HasStatics']:
                assert not gameEntitiesInfo[entity]['IsGlobal']
                registerLines.append('REGISTER_ENTITY_STATICS("' + entity + '", ' + engineEntityType + ');')
        registerLines.append('')
        
        # Generic funcdefs
        registerLines.append('// Generic funcdefs')
        for fd in genericFuncdefs:
            fdParams = fd.split('|')
            registerLines.append('AS_VERIFY(engine->RegisterFuncdef("' + metaTypeToASType(fdParams[0]) + ' Generic_' + '.'.join(fdParams).replace('.', '_') +
                    '_Func(' + ', '.join([metaTypeToASType(p) for p in fdParams[1:]]) + ')"));')
        registerLines.append('')
        
        # Register entity methods and global functions
        registerLines.append('// Register methods')
        for entity in gameEntities:
            for methodTag in codeGenTags['ExportMethod']:
                targ, ent, name, ret, params, exportFlags, comment = methodTag
                if targ in allowedTargets and ent == entity:
                    if gameEntitiesInfo[entity]['IsGlobal']:
                        registerLines.append('AS_VERIFY(engine->RegisterGlobalFunction("' + metaTypeToASType(ret, isRet=True) + ' ' + name + '(' +
                                ', '.join([metaTypeToASType(p[0]) + ' ' + p[1] for p in params]) + ')", SCRIPT_METHOD(ASGlobal, AS_' + targ + '_' +
                                entity + '_' + name + '_' + nameMangling(params) + '), SCRIPT_FUNC_FUNCTOR_CONV, &storage->Global));')
                    else:
                        registerLines.append('AS_VERIFY(engine->RegisterObjectMethod("' + entity + '", "' + metaTypeToASType(ret, isRet=True) + ' ' + name + '(' +
                                ', '.join([metaTypeToASType(p[0]) + ' ' + p[1] for p in params]) + ')", SCRIPT_FUNC_THIS(AS_' + targ + '_' +
                                entity + '_' + name + '_' + nameMangling(params) + '), SCRIPT_FUNC_THIS_CONV));')
        registerLines.append('')
        
        # Register events
        registerLines.append('// Register events')
        for entity in gameEntities:
            for evTag in codeGenTags['ExportEvent'] + codeGenTags['Event']:
                targ, ent, evName, evArgs, evFlags, _ = evTag
                if targ in allowedTargets and ent == entity:
                    isExported = evTag in codeGenTags['ExportEvent']
                    funcEntry = 'ASEvent_' + entity + '_' + evName
                    asArgs = ', '.join([metaTypeToASType(p[0]) + ' ' + p[1] for p in evArgs])
                    asArgsEnt = metaTypeToASType(entity) + (', ' if evArgs else '') if not gameEntitiesInfo[entity]['IsGlobal'] else ''
                    if isExported:
                        registerLines.append('REGISTER_ENTITY_EXPORTED_EVENT("' + entity + '", "' + evName + '", "' + asArgsEnt + '", "' + asArgs + '", ' + funcEntry + ');')
                    else:
                        registerLines.append('REGISTER_ENTITY_SCRIPT_EVENT("' + entity + '", "' + evName + '", "' + asArgsEnt + '", "' + asArgs + '", ' + funcEntry + ');')
        registerLines.append('')
        
        # Register settings
        registerLines.append('// Register settings')
        settEntity = gameEntitiesInfo['Game']['Client' if target != 'Server' else 'Server'] + '* self'
        for settTag in codeGenTags['ExportSettings']:
            grName, targ, settings, flags, _ = settTag
            if targ in allowedTargets:
                for sett in settings:
                    fixOrVar, keyType, keyName, initValues, _ = sett
                    registerLines.append('REGISTER_GET_SETTING(' + keyName + ', "' + metaTypeToASType(keyType, isRet=True) + ' get_' + keyName + '() const");')
                    if fixOrVar == 'var':
                        registerLines.append('REGISTER_SET_SETTING(' + keyName + ', "void set_' + keyName + '(' + metaTypeToASType(keyType) + ')");')
        for settTag in codeGenTags['Setting']:
            type, name, initValue, flags, _ = settTag
            if not isASCompiler:
                registerLines.append('AngelScriptData->SettingsStorage["' + name + '"] = std::make_any<' + metaTypeToEngineType(type, target, False) + '>(' + (initValue if initValue is not None else '') + ');')
            registerLines.append('REGISTER_GET_SETTING(' + name + ', "' + metaTypeToASType(type, isRet=True) + ' get_' + name + '() const");')
            registerLines.append('REGISTER_SET_SETTING(' + name + ', "void set_' + name + '(' + metaTypeToASType(type) + ')");')
        registerLines.append('')
        
        # Register remote calls
        if target in ['Server', 'Client']:
            registerLines.append('// Register remote calls')
            for rcTag in codeGenTags['RemoteCall']:
                targ, subsystem, rcName, rcArgs, rcFlags, _ = rcTag
                if targ == ('Client' if target == 'Server' else 'Server') and subsystem == 'AngelScript':
                    registerLines.append('AS_VERIFY(engine->RegisterObjectMethod("RemoteCaller", "void ' + rcName +
                            '(' + ', '.join([metaTypeToASType(p[0]) + ' ' + p[1] for p in rcArgs]) + ')", SCRIPT_FUNC_THIS(ASRemoteCall_' + rcName + '), SCRIPT_FUNC_THIS_CONV));')
            registerLines.append('')
        
        # Modify file content (from bottom to top)
        insertCodeGenLines(registerLines, 'Register')
        insertCodeGenLines(storageLines, 'Storage')
        insertCodeGenLines(globalLines, 'Global')
        insertCodeGenLines(defineLines, 'Defines')

    elif lang == 'Mono':
        assert False, 'Mono generation not implemented'
    
    elif lang == 'Native':
        assert False, 'Native generation not implemented'

try:
    genCode('AngelScript', 'Server')
    genCode('AngelScript', 'Client')
    genCode('AngelScript', 'Mapper')
    genCode('AngelScript', 'Single')
    genCode('AngelScript', 'Server', True)
    genCode('AngelScript', 'Client', True)
    genCode('AngelScript', 'Mapper', True)
    genCode('AngelScript', 'Single', True)
    genCode('Mono', 'Server')
    genCode('Mono', 'Client')
    genCode('Mono', 'Mapper')
    genCode('Mono', 'Single')
    genCode('Native', 'Server')
    genCode('Native', 'Client')
    genCode('Native', 'Mapper')
    genCode('Native', 'Single')
    
except Exception as ex:
    showError('Code generation failed', ex)

checkErrors()

# AngelScript root file
def genAngelScriptRoot(target):
    # Generate content file
    createFile('Content.fos', args.genoutput)
    def writeEnums(name, lst):
        writeFile('namespace ' + name)
        writeFile('{')
        for i in lst:
            writeFile('    hstring ' + i + ' = hstring("' + i + '");')
        writeFile('}')
        writeFile('')
    writeFile('// FOS Common')
    writeFile('')
    writeEnums('Dialog', content['fodlg'])
    writeEnums('Item', content['foitem'])
    writeEnums('Critter', content['focr'])
    writeEnums('Map', content['fomap'])
    writeEnums('Location', content['foloc'])

    # Sort files
    asFiles = []

    for file in args.assource:
        file = os.path.abspath(file)
        
        with open(file, 'r', encoding='utf-8-sig') as f:
            line = f.readline()

        if len(line) >= 3 and ord(line[0]) == 0xEF and ord(line[1]) == 0xBB and ord(line[2]) == 0xBF:
            line = line[3:]

        assert line.startswith('// FOS'), 'Invalid .fos file header: ' + file + ' = ' + line
        fosParams = line[6:].split()
        sort = int(fosParams[fosParams.index('Sort') + 1]) if 'Sort' in fosParams else 0

        if target in fosParams or 'Common' in fosParams:
            asFiles.append((sort, file))

    asFiles.sort()

    # Write files
    def writeRootModule(files):
        def addInclude(file, comment):
            writeFile('namespace ' + os.path.splitext(os.path.basename(file))[0] + ' {')
            writeFile('#include "' + file.replace('\\', '/') + ('" // ' + comment if comment else '"'))
            writeFile('}')

        createFile(target + 'RootModule.fos', args.genoutput)
        addInclude(args.genoutput.replace('\\', '/').rstrip('/') + '/Content.fos', 'Generated')
        for file in files:
            addInclude(file[1], 'Sort ' + str(file[0]) if file[0] else None)

    writeRootModule(asFiles)

if args.angelscript:
    try:
        if args.multiplayer:
            genAngelScriptRoot('Server')
            genAngelScriptRoot('Client')
        if args.singleplayer:
            assert False, 'Not implemented'
        if args.mapper:
            genAngelScriptRoot('Mapper')
            
    except Exception as ex:
        showError('Can\'t generate scripts', ex)

# Markdown
def genApiMarkdown(target):
    createFile(target.upper() + '_SCRIPT_API.md', args.mdpath)
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
    writeFile('  * [Player properties](#player-properties)')
    if target == 'Multiplayer':
        writeFile('  * [Player server methods](#player-server-methods)')
        writeFile('  * [Player client methods](#player-client-methods)')
    elif target == 'Singleplayer':
        writeFile('  * [Player methods](#player-methods)')
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
                    'PlayerView': 'Player', 'ItemView': 'Item', 'CritterView': 'Critter', 'MapView': 'Map', 'LocationView': 'Location'}
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
    for entity in ['Player', 'Item', 'Critter', 'Map', 'Location']:
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

if args.markdown and False:
    try:
        if args.multiplayer:
            genApiMarkdown('Multiplayer')
        if args.singleplayer:
            genApiMarkdown('Singleplayer')
        if args.mapper:
            genApiMarkdown('Mapper')
    
    except Exception as ex:
        showError('Can\'t generate markdown representation', ex)
    
    checkErrors()

def genApi(target):
    # C++ projects
    if args.native:
        createFile('FOnline.' + target + '.h')

        # Generate source
        def parseType(t):
            def mapType(t):
                typeMap = {'char': 'int8_t', 'uchar': 'uint8_t', 'short': 'int16_t', 'ushort': 'uint16_t', 'int64': 'int64_t', 'uint64': 'uint64_t',
                        'ItemView': 'ScriptItem*', 'ItemHexView': 'ScriptItem*', 'PlayerView': 'ScriptPlayer*', 'Player': 'ScriptPlayer*',
                        'CritterView': 'ScriptCritter*', 'CritterHexView': 'ScriptCritter*', 'MapView': 'ScriptMap*', 'LocationView': 'ScriptLocation*',
                        'Item': 'ScriptItem*', 'Critter': 'ScriptCritter*', 'Map': 'ScriptMap*', 'Location': 'ScriptLocation*',
                        'string': 'std::string', 'Entity': 'ScriptEntity*'}
                return typeMap[t] if t in typeMap else t
            tt = t.split('.')
            if tt[0] == 'dict':
                r = 'std::map<' + mapType(tt[1]) + ', ' + mapType(tt[2]) + '>'
            elif tt[0] == 'callback':
                r = 'std::function<void(' + mapType(tt[1]) + ')>'
            elif tt[0] == 'predicate':
                r = 'std::function<bool(' + mapType(tt[1]) + ')>'
            elif tt[0] in userObjects:
                return tt[0] + '*'
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
        writeFile('using hash = uint32_t;')
        writeFile('using uint = uint32_t;')
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

        for entity in ['Player', 'Item', 'Critter', 'Map', 'Location']:
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
        """

        # Settings
        """
        writeFile('## Settings')
        writeFile('')
        for i in nativeMeta.settings:
            ret, name, init, doc = i
            writeFile('* ' + parseType(ret) + ' ' + name + ' = ' + init)
            writeDoc(doc)
            writeFile('')
        """

        # Content pids
        """
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
                typeMap = {'char': 'sbyte', 'uchar': 'byte', 'int64': 'long', 'uint64': 'ulong',
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
                #writeFile('            ' + i + ' = ' + getHash(i) + ',')
                pass
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
"""
try:
    if args.multiplayer:
        genApi('Server')
        genApi('Client')
    if args.singleplayer:
        genApi('Single')
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
    #zipData = io.BytesIO()
    #with zipfile.ZipFile(zipData, 'w', compression=zipfile.ZIP_DEFLATED) as zip:
    #    for res in resources['Embedded']:
    #        zip.write('Backed_' + res[1], res[0])
    #createFile('EmbeddedResources-Include.h', args.genoutput)
    #writeFile('const unsigned char EMBEDDED_RESOURCES[] = {0x' + ', 0x'.join(zipData.getvalue().hex(' ').split(' ')) + '};')
    preserveBufSize = 1200000 # Todo: move preserveBufSize to build setup
    assert preserveBufSize > 100
    createFile('EmbeddedResources-Include.h', args.genoutput)
    writeFile('const unsigned char EMBEDDED_RESOURCES[' + str(preserveBufSize) + '] = {0x' + ', 0x'.join(struct.pack("I", preserveBufSize).hex(' ').split(' ')) + ', 0x00, ' + ('0x42, ' * 42) + '0x00};')

except Exception as ex:
    showError('Can\'t write embedded resources', ex)

checkErrors()

# Default settings
createFile('SettingsDefault-Include.h', args.genoutput)
writeFile('R"CONFIG(')
for cfg in args.config:
    k, v = cfg.split(',', 1)
    writeFile(k + ' = ' + v)
writeFile(')CONFIG"')

# Version info
createFile('Version-Include.h', args.genoutput)
writeFile('static constexpr auto FO_BUILD_HASH = "' + args.buildhash + '";')
writeFile('static constexpr auto FO_DEV_NAME = "' + args.devname + '";')
writeFile('static constexpr auto FO_GAME_VERSION = "' + args.gamename + '";')
writeFile('static constexpr auto FO_COMPATIBILITY_VERSION = 0x12345678;') # Todo: FO_COMPATIBILITY_VERSION

# Actual writing of generated files
try:
    flushFiles()
    
except Exception as ex:
    showError('Can\'t flush generated files', ex)

checkErrors()

elapsedTime = time.time() - startTime
print('[CodeGen]', 'Code generation complete in ' + '{:.2f}'.format(elapsedTime) + ' seconds')
