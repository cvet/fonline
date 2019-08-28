import os
from xml.dom.minidom import parseString

from options import options
from fix_xml import fixXML

from templates.GlobalsPage import GlobalsPage
from templates.ReservedFunctionsPage import ReservedFunctionsPage 

compound_elements = ["page", "class", "file", "group", "dir"]

known_groups = {
                "group___client_globals": GlobalsPage,
                "group___client_reserved": ReservedFunctionsPage,
                "group___server_globals": GlobalsPage,
                "group___server_reserved_critter": ReservedFunctionsPage,
                "group___server_reserved_item": ReservedFunctionsPage,
                "group___server_reserved_main": ReservedFunctionsPage,
                "group___server_reserved_map": ReservedFunctionsPage
                }

class Doxygen:
    def __init__(self):
        self.files = []
        self.staticfiles = []
        self.links = {}
        self.footer = {}
        if options.no_labels:
            self.labels = []
        elif options.labels == []:
            self.labels = ["Doxygen"]
        else:
            self.labels = options.labels

    def processFile(self, xml):
        global compound_elements
        global known_groups
        
        def id(node):
            return node.attributes["id"].value
        
        def refid(node):
            return node.attributes["refid"].value
        
        def kind(node):
            return node.attributes["kind"].value
        
        # index file
        if xml.documentElement.tagName == "doxygenindex":
            for node in xml.documentElement.getElementsByTagName("compound"):
                if kind(node) in compound_elements:
                    if options.verbose:
                        print "Processing", refid(node) + ".xml"
                    self.processFile(parseString(fixXML(open(options.docs + refid(node) + ".xml", "r").read())))
        # content
        elif xml.documentElement.tagName == "doxygen":
            compounds = xml.documentElement.getElementsByTagName("compounddef")
            for c in compounds:
                if kind(c) == "file":
                    # skip files
                    pass # self.files.append(DoxygenFile(c))
                elif kind(c) == "page":
                    # skip pages
                    pass # self.files.append(DoxygenPage(c))
                elif kind(c) == "class":
                    self.files.append(DoxygenClass(c));
                elif kind(c) == "group":
                    try:
                        template = known_groups[id(c)]
                    except KeyError:
                        template = None
                        print "Warning: unknown group '" + id(c) + "'"
                    if template is not None:
                        self.files.append(DoxygenGroup(c, template))
                elif kind(c) == "dir":
                    # skip dirs
                    pass
                else:
                    raise SystemError, "Unrecognised compound type. (%s)" % (c.attributes["kind"].value, )
        else:
            raise SystemError, "Unrecognised root file node. (%s)" % (xml.documentElement.tagName, )

    def addLink(self, refid, linkto):
        self.links[refid] = linkto
        
    def createFiles(self):
        files = []
        for f in self.files:
            files += f.createFiles()
        return files + self.staticfiles

    def getFooter(self):
        #footer = "---\n|| [%s Main Page]" % (options.prefix, )
        #if self.footer.has_key("file"):
        #    footer += " || [%s_files Files]" % (options.prefix, )
        #footer += " ||\n"
        #return footer
        return ""

    def copyFile(self, type, _from, _to):
        if options.output + _to not in [x[1] for x in self.staticfiles]:
            open(options.output + _to, "wb").write(open(_from, "rb").read())
            self.staticfiles.append(("static", _to, None))

doxygen = Doxygen()

from classfile import DoxygenClass
from file import DoxygenFile
from group import DoxygenGroup
from member_function import DoxygenMemberFunction
from page import DoxygenPage
