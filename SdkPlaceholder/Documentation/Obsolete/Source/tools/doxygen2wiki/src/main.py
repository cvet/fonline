#!/usr/bin/python

import os
import sys
from xml.dom.minidom import parse

from doxygen import doxygen
from options import options

def getPreviousFiles():
    try:
        return [x.strip() for x in open("%s%s.status" % (options.output, options.prefix), "r").readlines()]
    except IOError:
        return None

def main():
    doxygen.processFile(parse(options.docs + "index.xml"))

    oldfiles = getPreviousFiles()
    files = []

    for type, file, code in doxygen.createFiles():
        if type == "wiki":
            file = file + ".wiki"
            files.append(file)

            if options.verbose:
                print "Generating page for " + file
            text = unicode(code)
            text += doxygen.getFooter()
            
            open(options.output + file, "w").write(text.encode("windows-1251"))
        else:
            files.append(file)
