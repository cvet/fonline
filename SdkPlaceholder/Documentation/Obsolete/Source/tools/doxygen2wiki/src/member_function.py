from doxygen import doxygen
from utils import getConvertedText, getDirectDescendants, getDirectDescendant, getNodeText
from text_elements import convertLine

from templates.Function import Function

class DoxygenMemberFunction:
    def __init__(self, xml, parent_page):
        self.parent_page = parent_page
        self.refid = xml.attributes["id"].value
        self.name = getNodeText(xml.getElementsByTagName("name")[0])
        self.brief = convertLine(getDirectDescendant(xml, "briefdescription"), self)
        self.detailed = convertLine(getDirectDescendant(xml, "detaileddescription"), self)
        self.type = getNodeText(xml.getElementsByTagName("type")[0])

        # TODO: rewrite
        self.params = []
        paramsdict = {}
        for p in xml.getElementsByTagName("param"):
            type = getNodeText(p.getElementsByTagName("type")[0])
            declnameNode = getDirectDescendant(p, "declname")
            if declnameNode is not None:
                declname = getNodeText(declnameNode)
                paramsdict[declname] = type
            else:
                declname = ""
            self.params.append((type, declname))

        self.param_groups = [ParamGroup(node) for node in xml.getElementsByTagName("parameteritem")]
        # typing
        for param_group in self.param_groups:
            for param in param_group.params:
                 param.type = paramsdict[param.name]
        
        self.returns = ReturnGroup(getDirectDescendant(xml, "detaileddescription"))
        self.remarks = RemarkGroup(getDirectDescendant(xml, "detaileddescription"))
        self.see = SeeGroup(getDirectDescendant(xml, "detaileddescription"))
        
        # register function
        self.parent_page.addMember(self.refid, self.name)
        doxygen.addLink(self.refid, parent_page.getPageName() + "#" + self.parent_page.getMemberAnchor(self.refid))

    def __get_sig(self):
        return self.type + " " + self.name + "(" + ", ".join(["%s %s" % x for x in self.params]) + ")"
    sig = property(__get_sig)

    def __get_doc(self):
        return unicode(Function(searchList={"f": self,
                                            "detailed_description": getConvertedText(self.detailed),
                                            "brief_description": getConvertedText(self.brief),
                                            "anchor": self.parent_page.getMemberAnchor(self.refid),
                                            "returns": map(getConvertedText, self.returns.items),
                                            "remarks": map(getConvertedText, self.remarks.items),
                                            "see": map(getConvertedText, self.see.items)
                                            }))
    doc = property(__get_doc)

class ParamGroup:
    def __init__(self, xml):
        self.params = [Param(node) for node in xml.getElementsByTagName("parametername")]
        self._description = convertLine(getDirectDescendant(xml, "parameterdescription"), self)
        
    def __get_description(self):
        return getConvertedText(self._description)
    description = property(__get_description)
    
class Param:
    def __init__(self, xml):
        self.name = getNodeText(xml)
        self.direction = ""
        self.type = None # there is no type information, but it can be binded later
        try:
            self.direction = xml.attributes["direction"].value
        except KeyError:
            pass
        
class ReturnGroup:
    def __init__(self, xml):
        self.items = [convertLine(s, self) for s in xml.getElementsByTagName("simplesect") if kind(s) == "return"]
    
class RemarkGroup:
    def __init__(self, xml):
        self.items = []
        
        def extractRemarks(node):
            remarks = []
            remark = []
            for el in getDirectDescendants(node, "*"):
                if (el.tagName == "para"):
                    remark[-1:] = (convertLine(el, None))
                elif (el.tagName == "simplesectsep"): # remarks delimiter
                    remarks.append(remark)
                    remark = []
            else:
                if len(remark) > 0:
                    remarks.append(remark)
            return remarks

        for node in xml.getElementsByTagName("simplesect"):
            if kind(node) == "remark":
                self.items.extend(extractRemarks(node))
                
class SeeGroup:
    def __init__(self, xml):
        
        def extractLinks(node):
            return [convertLine(n, self) for n in getDirectDescendants(node, "para")]
        
        self.items = []
        for node in xml.getElementsByTagName("simplesect"):
            if kind(node) == "see":
                self.items.extend(extractLinks(node))     
    
def kind(node):
    return node.attributes["kind"].value;