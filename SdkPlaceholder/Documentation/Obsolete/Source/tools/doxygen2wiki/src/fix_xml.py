import re
from utils import getNodeText, getDirectDescendants, getDirectDescendant
from xml.dom.minidom import parse

urlfix = re.compile("""<ulink url=".+?">.*?</ulink>""")

def repl(m):
    urlstart = m.string[m.start():m.end()].find('url="') + 5
    urlend = m.string[m.start():m.end()].find('"', urlstart + 1)
    url = m.string[m.start():m.end()][urlstart:urlend]

    newurl = []
    for i in range(len(url)):
        if url[i] == "&" and url[i:i+5] != "&amp;":
            newurl.append("&amp;")
        else:
            newurl.append(url[i])

    return m.string[m.start():m.start()+urlstart] + "".join(newurl) + m.string[m.start()+urlend:m.end()]

def fixXML(xml):
    text = urlfix.sub(repl, xml)
    return text
