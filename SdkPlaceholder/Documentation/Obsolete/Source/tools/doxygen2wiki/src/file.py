from options import options
from member_function import DoxygenMemberFunction
from section import DoxygenSection
from utils import getNodeText, getDirectDescendants, getDirectDescendant

from templates.FilePage import FilePage

class DoxygenFile:
    def __init__(self, xml):

        def getSections(file, predicate=None):
            return [DoxygenSection(sd, self) for sd in filter(predicate, getDirectDescendants(xml, "sectiondef"))]

        def isUserDefined(section):
            return section.attributes["kind"].value == "user-defined"

        self.id = xml.attributes["id"].value
        self.name = getNodeText(xml.getElementsByTagName("compoundname")[0])
        self.brief = convertLine(getDirectDescendant(xml, "briefdescription"), self)
        self.detailed = convertLine(getDirectDescendant(xml, "detaileddescription"), self)

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
                 FilePage(searchList={"labels": options.labels,
                                      "prefix": options.prefix,
                                      "filename": self.name,
                                      "briefdescription": "".join(brief).strip(),
                                      "detaileddescription": "".join(detailed).strip(),
                                      "usersections": self.usersections,
                                      "generatedsections": self.generatedsections
                                      })
                 )]

from text_elements import convertLine, ProgramListing
