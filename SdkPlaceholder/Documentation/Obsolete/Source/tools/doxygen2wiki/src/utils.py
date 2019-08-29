def getConvertedText(lines):
    l = [""]
    [x.getLines(l) for x in lines]
    return "".join(l).strip()

def getDirectDescendant(node, tagname):
    for n in node.getElementsByTagName(tagname):
        if n.parentNode is node:
            return n
    return None

def getDirectDescendants(node, tagname):
    return [n for n in node.getElementsByTagName(tagname) if n.parentNode is node]

def getNodeText(node):
    if node is None:
        return None
    elif node.nodeType == node.TEXT_NODE:
        return node.data
    else:
        return "".join(map(getNodeText, node.childNodes))