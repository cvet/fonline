from options import options
from section import DoxygenSection
from utils import getNodeText, getDirectDescendants, getDirectDescendant
from wikipage import WikiPage

from templates.PageName import PageName 

class DoxygenGroup(WikiPage):
    def __init__(self, xml, template):

        def getSections(file, predicate=None):
            return [DoxygenSection(sd, self) for sd in filter(predicate, getDirectDescendants(xml, "sectiondef"))]

        def isUserDefined(section):
            return section.attributes["kind"].value == "user-defined"

        self.id = xml.attributes["id"].value
        self.parent = None
        self.template = template
        self.name = getNodeText(xml.getElementsByTagName("compoundname")[0])
        self.title = getNodeText(getDirectDescendant(xml, "title"))
        self.brief = convertLine(getDirectDescendant(xml, "briefdescription"), self)
        self.detailed = convertLine(getDirectDescendant(xml, "detaileddescription"), self)
        
        # call base class constructor
        WikiPage.__init__(self, self.getPageName())

        self.usersections = getSections(xml, isUserDefined)
        self.generatedsections = getSections(xml, lambda x: not isUserDefined(x))

        programlistings = xml.getElementsByTagName("programlisting");
        if len(programlistings) > 0:
            self.programlisting = ProgramListing(programlistings[0], self)

    def createFiles(self):
        brief = [""]
        for b in self.brief:
            b.getLines(brief)
        detailed = [""]
        for b in self.detailed:
            b.getLines(detailed)
        return [(
                 "wiki", options.prefix + "_" + self.id,
                 self.template(searchList={"summary": "Documentation for the %s file" % (self.name, ),
                                      "labels": options.labels,
                                      "prefix": options.prefix,
                                      "name": self.name,
                                      "title": self.title,
                                      "briefdescription": "".join(brief).strip(),
                                      "detaileddescription": "".join(detailed).strip(),
                                      "usersections": self.usersections,
                                      "generatedsections": self.generatedsections
                                      })
                 )]
        
    def getPageName(self):
        display_name = self.title if len(self.title) > 0 else self.name
        return PageName(searchList={"name": display_name}).respond()

from text_elements import convertLine, ProgramListing