#!/bin/python

import os
import sys

rootPath = '../'
todoPath = ['README.md']
sourcePath = ['Source', 'CMakeLists.txt']
sourceExt = ['cpp', 'h']
generateSection = False
todoToFind = ['// Todo:', '# Todo:']
headLine = '#### Todo list *(generated from source code)*'
tailLine = '  '
priorityFiles = ['Common.h', 'Common.cpp']
capitalizeFirstLetter = False
uncapitalizeFirstLetter = False
printResult = True
dontWriteFile = False


def decideNewLine(fileLines):
    crCount = 0
    lfCount = 0
    crlfCount = 0
    for line in fileLines:
        if len(line) >= 2 and line[-1:] == '\n' and line[-2:-1] == '\r':
            crlfCount += 1
        elif line[-1:] == '\n':
            lfCount += 1
        elif line[-1:] == '\r':
            crCount += 1

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
        index += 1
    return -1


def processFile(filePath, todoInfo):
    fileLines = readFileLines(filePath)
    lineIndex = 0
    for line in fileLines:
        lineIndex += 1
        for td in todoToFind:
            tdIndex = line.find(td)
            if tdIndex != -1:
                todoEntry = line[tdIndex + len(td):].strip('\r\n \t')
                if len(todoEntry) == 0:
                    todoEntry = '(Unnamed todo)'

                entryIndex = findInList(
                    todoInfo, lambda x: x[0] == todoEntry and x[1] == filePath)
                if entryIndex == -1:
                    todoInfo.append([todoEntry, filePath, 1])
                else:
                    todoInfo[entryIndex][2] += 1


def getFilesAtPath(path):
    def fillRecursive(path, pathList):
        if os.path.isfile(path):
            pathList.append(path)
        elif os.path.isdir(path):
            for p in os.listdir(path):
                fillRecursive(os.path.join(path, p), pathList)

    def isNeedParse(path):
        ext = os.path.splitext(path)[1].lower()[1:]
        return ext in sourceExt

    filePathList = []
    fillRecursive(path, filePathList)
    filePathList = list(filter(isNeedParse, filePathList))
    filePathList.sort()

    priorityPathList = []
    for priorityFile in priorityFiles:
        for filePath in filePathList[:]:
            if priorityFile in filePath:
                priorityPathList.append(filePath)
                filePathList.remove(filePath)
                break

    return priorityPathList + filePathList


def processPath(path, todoInfo):
    filePathList = getFilesAtPath(path)
    for path in filePathList:
        processFile(path, todoInfo)


def generateTodoDesc(todoInfo):
    descList = []
    tdIndex = 0
    for td in todoInfo:
        tdIndex += 1

        entryName = td[0]
        if capitalizeFirstLetter:
            entryName = entryName[:1].upper() + entryName[1:]
        if uncapitalizeFirstLetter:
            entryName = entryName[:1].lower() + entryName[1:]

        filePath = td[1]
        fileName = os.path.splitext(os.path.basename(filePath))[0]
        desc = '* ' + fileName + ': ' + entryName

        if td[2] > 1:
            desc += ' (' + str(td[2]) + ')'

        descList.append(desc)

    if len(descList) == 0:
        descList.append('Nothing todo :)')
    return descList


def injectDesc(path, descList):
    todoList = []
    todoList.append(headLine)
    todoList.append('')
    todoList.extend(descList)
    todoList.append(tailLine)

    if printResult:
        for todo in todoList:
            print(todo)

    if not dontWriteFile:
        fileLines = readFileLines(path)

        todoBegin = findInList(
            fileLines, lambda l: l.strip('\r\n') == headLine)
        todoEnd = findInList(fileLines, lambda l: l.strip('\r\n') == tailLine)

        if todoBegin == -1 and not generateSection:
            print('Begin section not found')
            sys.exit(1)

        if todoBegin != -1 and todoEnd != -1 and todoEnd > todoBegin:
            fileLines = fileLines[:todoBegin] + fileLines[todoEnd + 1:]

        nl = decideNewLine(path)

        finalOutput = fileLines[:todoBegin]
        finalOutput.extend([todo + nl for todo in todoList])
        finalOutput.extend(fileLines[todoBegin:])

        with open(path, 'wb') as f:
            f.writelines(finalOutput)


os.chdir(rootPath)

todoInfo = []
for srcPath in sourcePath:
    processPath(srcPath, todoInfo)

descList = generateTodoDesc(todoInfo)

for tdPath in todoPath:
    injectDesc(tdPath, descList)
