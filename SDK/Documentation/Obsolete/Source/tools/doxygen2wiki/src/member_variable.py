from doxygen import doxygen
from text_elements import convertLine
from utils import getNodeText, getDirectDescendant, getConvertedText

from templates.Variable import Variable

class DoxygenMemberVariable:
    def __init__(self, xml, parent_page):
        self.parent_page = parent_page
        self.refid = xml.attributes["id"].value
        self.type = getNodeText(xml.getElementsByTagName("type")[0])
        self.name = getNodeText(xml.getElementsByTagName("name")[0])
        self.initializer = getNodeText(getDirectDescendant(xml, "initialize"))
        self.brief = convertLine(getDirectDescendant(xml, "briefdescription"), self)
        self.detailed = convertLine(getDirectDescendant(xml, "detaileddescription"), self)
        
        # register variable
        self.parent_page.addMember(self.refid, self.name)
        doxygen.addLink(self.refid, parent_page.getPageName() + "#" + self.parent_page.getMemberAnchor(self.refid))

    def __get_sig(self):
        return self.type + " " + self.name + self.initializer if len(self.initializer) > 0 else ""
    sig = property(__get_sig)

    def __get_doc(self):
        return unicode(Variable(searchList={"v": self,
                                            "brief_description": getConvertedText(self.brief),
                                            "detailed_description": getConvertedText(self.detailed),
                                            "anchor": self.parent_page.getMemberAnchor(self.refid)
                                            }))
    doc = property(__get_doc)
