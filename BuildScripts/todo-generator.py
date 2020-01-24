#!/bin/python

"""
Todo list:
High priority:
- fix something now (SomeClass.cpp:333)
Regular:
- fix something (SomeClass.cpp:194)
In doubt:
- maybe fix something (SomeClass.cpp:222)
Undescripted:
- in file SomeClass.cpp: 22th
- in SomeClass2.h: 2th

SomeClass.cpp
// Todo: fix something now (!)
// Todo: fix something
// Todo: maybe fix something (?)

! - high priority
!! - ciritcal place, need fix as soon as possible
!!! - fix before commit
"""

import os

rootPath = '../'
todoPath =  'README.md'
sourcePath = 'Source;CMakeLists.txt'
sourceExt = 'cpp;h'
generateFile = False
generateSection = False
todoToFind = '// Todo:;# Todo:'
rowFormat = '- {TEXT} ({ARG0}/{PATH})'
headLine = '#### Todo list *(generated from source code)*'
criticalLine = '[red]Fix before commit:'
importantLine = 'Priority todo:'
regularLine = 'Todo:'
doubtLine = 'Doubt todo:'
tailLine = '  '
githubUrl = 'https://github.com/cvet/fonline/blob/master/'

def decideNewLine(fileLines):
    crCount = 0
    lfCount = 0
    crlfCount = 0
    for line in fileLines:
        if len(line) >= 2 and line[-1:] == '\n' and line[-2:-1] == '\r':
            crlfCount = crlfCount + 1
        elif line[-1:] == '\n':
            lfCount = lfCount + 1
        elif line[-1:] == '\r':
            crCount = crCount + 1

    if crlfCount == 0 and lfCount == 0 and crlfCount == 0:
        return os.linesep
    if crlfCount >= crCount and crlfCount >= lfCount:
        return '\r\n'
    if lfCount >= crlfCount and lfCount >= crCount:
        return '\n'
    if crCount >= crlfCount and crCount >= lfCount:
        return '\r'

    assert False

def readFileLines(filePath):
    with open(filePath, 'rb') as f:
        return f.readlines()

def findInList(lst, predicate):
    index = 0
    for l in lst:
        if predicate(l):
            return index
        index = index + 1
    return -1

def processFile(filePath, todoInfo):
    fileLines = readFileLines(filePath)
    lineIndex = 0
    for line in fileLines:
        lineIndex = lineIndex + 1
        for td in todoToFind.split(';'):
            tdIndex = line.find(td)
            if tdIndex != -1:
                todoEntry = line[tdIndex + len(td):].strip('\r\n \t')
                if len(todoEntry) == 0:
                    todoEntry = '(Unnamed todo)'
                entryIndex = findInList(todoInfo, lambda x: x[0] == todoEntry)
                if entryIndex == -1:
                    todoInfo.append((todoEntry, [(filePath, lineIndex)]))
                else:
                    todoInfo[entryIndex][1].append((filePath, lineIndex))

def processPath(path, todoInfo):
    if os.path.isfile(path):
        processFile(path, todoInfo)
    elif os.path.isdir(path):
        for p in os.listdir(path):
            processPath(os.path.join(path, p), todoInfo)

def generateTodoDesc(todoInfo):
    descList = []
    tdIndex = 0
    for td in todoInfo:
        tdIndex = tdIndex + 1
        print str(tdIndex) + ': ' + td[0] + ' (' + str(len(td[1])) + ')'
        desc = '* ' + td[0] + ' ('
        placeIndex = 0
        for place in td[1]:
            placeIndex = placeIndex + 1
            if placeIndex > 1:
                desc = desc + ', '
            placeUrl = githubUrl + place[0].replace('\\', '/') + '#L' + str(place[1])
            desc = desc + '[' + str(placeIndex) + '](' + placeUrl + ')'
        desc = desc + ')'
        descList.append(desc)
    if len(descList) == 0:
        descList.append('Nothing todo :)')
    return descList

def injectDesc(path, descList):
    todoLines = readFileLines(path)
    todoBegin = findInList(todoLines, lambda l: l.strip('\r\n') == headLine)
    todoEnd = findInList(todoLines, lambda l: l.strip('\r\n') == tailLine)

    if todoBegin == -1 and not generateSection:
        print 'Begin section not found'
        return

    if todoBegin != -1 and todoEnd != -1 and todoEnd > todoBegin:
        todoLines = todoLines[:todoBegin] + todoLines[todoEnd + 1:]

    nl = decideNewLine(path)
    nlDescList = [l + nl for l in descList]

    finalOutput = todoLines[:todoBegin]
    finalOutput.append(headLine + nl)
    finalOutput.append(nl)
    finalOutput.extend(nlDescList)
    finalOutput.append(tailLine + nl)
    finalOutput.extend(todoLines[todoBegin:])

    with open(path, 'wb') as f:
        f.writelines(finalOutput)

os.chdir(rootPath)

todoInfo = []
for srcPath in sourcePath.split(';'):
    processPath(srcPath, todoInfo)

descList = generateTodoDesc(todoInfo)

for tdPath in todoPath.split(';'):
    injectDesc(tdPath, descList)
