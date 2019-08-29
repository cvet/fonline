from doxygen import doxygen 
from options import options
from text_elements import convertLine
from section import DoxygenSection
from utils import getDirectDescendant, getDirectDescendants, getNodeText
from wikipage import WikiPage

from templates.ClassPage import ClassPage
from templates.ClassPageName import ClassPageName

class DoxygenClass(WikiPage):
    def __init__(self, xml):

        def getSections(file, predicate=None):
            return [DoxygenSection(sd, self) for sd in filter(predicate, getDirectDescendants(xml, "sectiondef"))]

        def isUserDefined(section):
            return section.attributes["kind"].value == "user-defined"
        
        self.id = xml.attributes["id"].value
        self.name = getNodeText(xml.getElementsByTagName("compoundname")[0])
        self.brief = convertLine(getDirectDescendant(xml, "briefdescription"), self)
        self.detailed = convertLine(getDirectDescendant(xml, "detaileddescription"), self)
        
        # call base class constructor
        WikiPage.__init__(self, self.getPageName())
        
        self.user_sections = getSections(xml, isUserDefined)
        self.generated_sections = getSections(xml, lambda x: not isUserDefined(x))
               
        # register class page
        doxygen.addLink(self.id, self.getPageName())

    def createFiles(self):
        brief = [""]
        for b in self.brief:
            b.getLines(brief)
        detailed = [""]
        for b in self.detailed:
            b.getLines(detailed)
        return [(
                 "wiki", options.prefix + "_" + self.id,
                 ClassPage(searchList={"labels": options.labels,
                                       "prefix": options.prefix,
                                       "name": self.name,
                                       "brief_description": "".join(brief).strip(),
                                       "detailed_description": "".join(detailed).strip(),
                                       "user_sections": self.user_sections,
                                       "generated_sections": self.generated_sections
                                       })
                 )]
    
    def getPageName(self):
        return ClassPageName(searchList={"className": self.name}).respond()
