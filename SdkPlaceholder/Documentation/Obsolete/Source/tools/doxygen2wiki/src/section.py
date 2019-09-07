from member_function import DoxygenMemberFunction
from member_variable import DoxygenMemberVariable
from text_elements import convertLine
from utils import getNodeText, getDirectDescendants, getDirectDescendant, getConvertedText


class DoxygenSection:
    def __init__(self, xml, parent_page):
        
        def kind(node):
            return node.attributes["kind"].value

        self.parent_page = parent_page;
        self.kind = xml.attributes["kind"].value
        self.name = getNodeText(getDirectDescendant(xml, "header"))
        self._description = convertLine(getDirectDescendant(xml, "description"), self)
        
        self.functions = []
        self.variables = []
        
        for node in getDirectDescendants(xml, "memberdef"):
            if kind(node) == "function":
                self.functions.append(DoxygenMemberFunction(node, parent_page))
            elif kind(node) == "variable":
                self.variables.append(DoxygenMemberVariable(node, parent_page))
            else:
                pass
            
    def __get_description(self):
        return getConvertedText(self._description)
    description = property(__get_description)







