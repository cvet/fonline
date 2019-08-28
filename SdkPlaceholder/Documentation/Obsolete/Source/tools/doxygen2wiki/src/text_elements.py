import os.path

from doxygen import doxygen
from options import options
from utils import getNodeText

def convertLine(node, parent):

    if node is None:
        return []

    text = []
    child = node.firstChild
    while child is not None:
        if child.nodeType == child.TEXT_NODE:
            text.append(Text(child, parent))
        else:
            try:
                e = elements[child.tagName]
            except KeyError:
                print "Unrecognized element,", child.tagName
                t = Text(None, parent)
                t.text = getNodeText(child).strip()
                text.append(t)
            else:
                text.append(e(child, parent))

        child = child.nextSibling
    return text

class Ignore:
    def __init__(self, xml, parent):
        self.parent = parent

    def getLines(self, lines):
        pass

class PassThrough:
    def __init__(self, xml, parent):
        self.parent = parent
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        [x.getLines(lines) for x in self.text]

class Text:
    def __init__(self, xml, parent):
        self.parent = parent
        if xml is not None:
            self.text = xml.data.strip("\n")

    def getLines(self, lines):
        if len(lines[-1]) > 0 and lines[-1][-1] == "\n":
            lines.append(self.text.lstrip())
        else:
            lines[-1] = lines[-1] + self.text

class Highlight:
    def __init__(self, xml, parent):
        self.parent = parent
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        l = [""]
        [x.getLines(l) for x in self.text]
        text = "".join(l)
        if len(lines[-1]) > 0 and lines[-1][-1] == "\n":
            lines.append(text)
        else:
            lines[-1] = lines[-1] + text

class Bold:
    def __init__(self, xml, parent):
        self.parent = parent
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        l = [""]
        [x.getLines(l) for x in self.text]
        text = "'''" + "".join(l) + "'''"
        if len(lines[-1]) > 0 and lines[-1][-1] == "\n":
            lines.append(text)
        else:
            lines[-1] = lines[-1] + text

class Emphasis:
    def __init__(self, xml, parent):
        self.parent = parent
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        l = [""]
        [x.getLines(l) for x in self.text]
        text = "''" + "".join(l) + "''"
        if len(lines[-1]) > 0 and lines[-1][-1] == "\n":
            lines.append(text)
        else:
            lines[-1] = lines[-1] + text

class SuperScript:
    def __init__(self, xml, parent):
        self.parent = parent
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        l = [""]
        [x.getLines(l) for x in self.text]
        text = "<sup>" + "".join(l) + "</sup>"
        if len(lines[-1]) > 0 and lines[-1][-1] == "\n":
            lines.append(text)
        else:
            lines[-1] = lines[-1] + text

class SubScript:
    def __init__(self, xml, parent):
        self.parent = parent
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        l = [""]
        [x.getLines(l) for x in self.text]
        text = "<sub>" + "".join(l) + "</sub>"
        if len(lines[-1]) > 0 and lines[-1][-1] == "\n":
            lines.append(text)
        else:
            lines[-1] = lines[-1] + text

class Para:
    def __init__(self, xml, parent):
        self.parent = parent
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        if not lines[-1] == "\n":
            lines[-1] = lines[-1] + "\n"
        [x.getLines(lines) for x in self.text]
        if not lines[-1] == "\n":
            lines[-1] = lines[-1] + "\n"
        lines.append("\n")

class SimpleSect:
    def __init__(self, xml, parent):
        self.parent = parent
        self.kind = None

        kind = xml.attributes["kind"].value
        try:
            s = sections[kind]
        except KeyError:
            print "Unrecognized section,", kind
            self.kind = Para(xml, parent)
        else:
            self.kind = s(xml, parent)

    def getLines(self, lines):
        self.kind.getLines(lines)

class LineBreak:
    def __init__(self, xml, parent):
        self.parent = parent

    def getLines(self, lines):
        if len(lines[-1]) == 0:
            lines[-1] = "\n"
        elif not lines[-1].endswith("\n"):
            lines[-1] = lines[-1] + "\n"

class Sect1:
    def __init__(self, xml, parent):
        self.parent = parent
        while parent is not None and not hasattr(parent, "pagename"):
            parent = parent.parent
        if parent is not None:
            doxygen.addLink(xml.attributes["id"].value, parent.pagename)
        else:
            doxygen.addLink(xml.attributes["id"].value, None)
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        [x.getLines(lines) for x in self.text]

class Sect2:
    def __init__(self, xml, parent):
        self.parent = parent
        while parent is not None and not hasattr(parent, "pagename"):
            parent = parent.parent
        if parent is not None:
            doxygen.addLink(xml.attributes["id"].value, parent.pagename)
        else:
            doxygen.addLink(xml.attributes["id"].value, None)
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        [x.getLines(lines) for x in self.text]

class Verbatim:
    def __init__(self, xml, parent):
        self.parent = parent
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        if len(lines[-1]) == 0 or lines[-1][-1] != "\n":
            lines[-1] = lines[-1] + "\n"
        lines.append("<pre>\n")
        for x in self.text:
            assert isinstance(x, Text)
            lines.append(x.text)
        if len(lines[-1]) == 0 or lines[-1][-1] != "\n":
            lines[-1] = lines[-1] + "\n"
        lines.append("</pre>\n")

class ComputerOutput:
    def __init__(self, xml, parent):
        self.parent = parent
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        l = [""]
        [x.getLines(l) for x in self.text]
        text = "<code as short>" + "".join(l) + "</code>"
        if len(lines[-1]) > 0 and lines[-1][-1] == "\n":
            lines.append(text)
        else:
            lines[-1] = lines[-1] + text

class Heading:
    def __init__(self, xml, parent):
        self.parent = parent
        self.text = convertLine(xml, self)
        self.parentNode = xml.parentNode.tagName

    def getLines(self, lines):
        l = [""]
        [x.getLines(l) for x in self.text]
        # TODO: rewrite
        if self.parentNode not in [ "simplesect" ]:
            text = "= " + "".join(l) + " =\n"
        else:
            text = "<dl><dt>" + "".join(l) + "</dl>"
        lines.append(text)

class ULink:
    def __init__(self, xml, parent):
        self.parent = parent
        self.url = xml.attributes["url"].value
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        l = [""]
        [x.getLines(l) for x in self.text]
        text = "[" + self.url + " " + "".join(l) + "]"
        if len(lines[-1]) > 0 and lines[-1][-1] == "\n":
            lines.append(text)
        else:
            lines[-1] = lines[-1] + text

class Ref:
    def __init__(self, xml, parent):
        self.parent = parent
        self.ref = xml.attributes["refid"].value
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        try:
            ref = doxygen.links[self.ref]
        except KeyError:
            ref = None
            print "Warning: cannot find reference " + self.ref  
        l = [""]
        [x.getLines(l) for x in self.text]
        if ref and (not isinstance(self.parent, Highlight)) and (not isinstance(self.parent, ComputerOutput)):
            text = "[[" + ref + "|" + "".join(l) + "]]"
        else:
            text = "".join(l)
        if len(lines[-1]) > 0 and lines[-1][-1] == "\n":
            lines.append(text)
        else:
            lines[-1] = lines[-1] + text

class ItemizedList:
    def __init__(self, xml, parent):
        self.parent = parent
        self.listitems = []
        child = xml.firstChild
        while child is not None:
            if child.nodeType == child.TEXT_NODE:
                pass
            elif child.tagName == "listitem":
                self.listitems.append(convertLine(child, self))
            else:
                print "Unknown node type in itemized list, " + child.tagName

            child = child.nextSibling

    def getLines(self, lines):
        if not lines[-1] == "\n":
            lines[-1] = lines[-1] + "\n"
        text = []
        for item in self.listitems:
            l = [""]
            [i.getLines(l) for i in item]
            text.append("* " + "".join(l).strip() + "\n")
        lines.extend(text)

class OrderedList:
    def __init__(self, xml, parent):
        self.parent = parent
        self.listitems = []
        child = xml.firstChild
        while child is not None:
            if child.nodeType == child.TEXT_NODE:
                pass
            elif child.tagName == "listitem":
                self.listitems.append(convertLine(child, self))
            else:
                print "Unknown node type in itemized list, " + child.tagName

            child = child.nextSibling

    def getLines(self, lines):
        if not lines[-1] == "\n":
            lines[-1] = lines[-1] + "\n"
        text = []
        fake = False
        for item in self.listitems:
            l = [""]
            [i.getLines(l) for i in item]
            text.append(l)
            if len(l) > 1:
                fake = True

        if fake:
            for i in range(len(text)):
                lines.append("# " + "".join(text[i]).strip() + "\n")
        else:
            lines.extend(["# " + "".join(l).strip() + "\n" for l in text])

class TocList:
    def __init__(self, xml, parent):
        self.parent = parent
        self.items = convertLine(xml, self)

    def getLines(self, lines):
        [x.getLines(lines) for x in self.items]

class TocItem:
    def __init__(self, xml, parent):
        self.parent = parent
        self.ref = xml.attributes["id"].value
        self.text = convertLine(xml, self)

    def getLines(self, lines):
        try:
            ref = doxygen.links[self.ref]
        except KeyError:
            ref = None
        l = [""]
        [x.getLines(l) for x in self.text]
        if ref:
            text = "[" + ref + " " + "".join(l) + "] "
        else:
            text = "".join(l)
        if len(lines[-1]) > 0 and lines[-1][-1] == "\n":
            lines.append(text[1:])
        else:
            lines[-1] = lines[-1] + text

class ProgramListing:
    def __init__(self, xml, parent):
        self.parent = parent
        self.code = [convertLine(x, self) for x in xml.getElementsByTagName("codeline")]

    def getLines(self, lines):
        if len(lines[-1]) == 0 or lines[-1][-1] != "\n":
            lines[-1] = lines[-1] + "\n"
        lines.append("<code as>");
        lines.append("\n");
        for l in self.code:
            [x.getLines(lines) for x in l]
            if len(lines[-1]) == 0 or lines[-1][-1] != "\n":
                lines[-1] = lines[-1] + "\n"
        if len(lines[-1]) == 0 or lines[-1][-1] != "\n":
            lines[-1] = lines[-1] + "\n"
        lines.append("</code>");
        lines.append("\n")

class Space:
    def __init__(self, xml, parent):
        self.parent = parent

    def getLines(self, lines):
        lines[-1] = lines[-1] + " "

class VariableList:
    def __init__(self, xml, parent):
        self.parent = parent
        self.varitems = []
        self.listitems = []
        child = xml.firstChild
        while child is not None:
            if child.nodeType == child.TEXT_NODE:
                pass
            elif child.tagName == "listitem":
                self.listitems.append(convertLine(child, self))
            elif child.tagName == "varlistentry":
                self.varitems.append(convertLine(child, self))
            else:
                print "Unknown node type in itemized list, " + child.tagName

            child = child.nextSibling

    def getLines(self, lines):
        if not lines[-1] == "\n":
            lines[-1] = lines[-1] + "\n"
        text = []
        for vartext, item in zip(self.varitems, self.listitems):
            var = [""]
            [i.getLines(var) for i in vartext]
            l = [""]
            [i.getLines(l) for i in item]
            text.append("".join(var).strip() + "\n")
            text.append(" " + "".join(l).strip() + "\n")
        lines.extend(text)

class Image:
    def __init__(self, xml, parent):
        self.parent = parent
        if xml.attributes["type"].value == "html":
            image = self.findImage(xml.attributes["name"].value)
            if image:
                doxygen.copyFile("static", image, options.prefix + "_" + xml.attributes["name"].value)
            if options.project is None:
                print "Warning, to use images you *must* supply the project name using the -j option."
                self.url = None
            else:
                self.url = "http://%s.googlecode.com/svn/wiki/%s" % (options.project, options.prefix + "_" + xml.attributes["name"].value)
        else:
            self.url = None

    def findImage(self, name):
        if os.path.exists(options.output + name):
            return options.output + name
        if options.html and os.path.exists(options.html + name):
            return options.html + name
        for i in options.images:
            if os.path.exists(i + name):
                return i + name
        print "Couldn't find image %s." % (name, )
        print "Looked in..."
        for d in [options.output, options.html] + options.images:
            if d is not None:
                print d

    def getLines(self, lines):
        if self.url:
            lines[-1] = lines[-1] + "[" + self.url + "] "
        else:
            pass

class Formula:
    def __init__(self, xml, parent):
        self.parent = parent
        id = xml.attributes["id"].value
        image = self.findImage("form_" + id + ".png")
        if image:
            doxygen.copyFile("static", image, options.prefix + "_" + "form_" + id + ".png")
        if options.project is None:
            print "Warning, to use images you *must* supply the project name using the -j option."
            self.url = None
        else:
            self.url = "http://%s.googlecode.com/svn/wiki/%s" % (options.project, options.prefix + "_" + "form_" + id + ".png")

    def findImage(self, name):
        if os.path.exists(options.output + name):
            return options.output + name
        if options.html and os.path.exists(options.html + name):
            return options.html + name
        for i in options.images:
            if os.path.exists(i + name):
                return i + name
        print "Couldn't find image %s." % (name, )
        print "Looked in..."
        for d in [options.output, options.html] + options.images:
            if d is not None:
                print d

    def getLines(self, lines):
        if self.url:
            lines[-1] = lines[-1] + "[" + self.url + "] "
        else:
            pass

class Copy:
    def __init__(self, xml, parent):
        self.parent = parent

    def getLines(self, lines):
        lines[-1] = lines[-1] + u"\xa9"

class NonBreakableSpace:
    def __init__(self, xml, parent):
        self.parent = parent

    def getLines(self, lines):
        lines[-1] = lines[-1] + u"\xf0"

class NDash:
    def __init__(self, xml, parent):
        self.parent = parent

    def getLines(self, lines):
        lines[-1] = lines[-1] + u"\u2013"

class MDash:
    def __init__(self, xml, parent):
        self.parent = parent

    def getLines(self, lines):
        lines[-1] = lines[-1] + u"\u2014"

class Acute:
    def __init__(self, xml, parent):
        self.parent = parent
        l = xml.attributes["char"].value
        if l == "o":
            self.letter = u"\xf3"
        elif l == "O":
            self.letter = u"\xd3"
        elif l == "a":
            self.letter = u"\xe1"
        elif l == "A":
            self.letter = u"\xc1"
        else:
            print "Warning, %c is an unrecognized letter for Acute." % (l, )
            print "Please report this as a bug to the Doxygen2GWiki project."
            self.letter = ""

    def getLines(self, lines):
        lines[-1] = lines[-1] + self.letter

class Umlaut:
    def __init__(self, xml, parent):
        self.parent = parent
        l = xml.attributes["char"].value
        if l == "o":
            self.letter = u"\xf6"
        elif l == "O":
            self.letter = u"\xd5"
        elif l == "a":
            self.letter = u"\xe4"
        elif l == "A":
            self.letter = u"\xc2"
        else:
            print "Warning, %c is an unrecognized letter for Umlaut." % (l, )
            print "Please report this as a bug to the Doxygen2GWiki project."
            self.letter = ""

    def getLines(self, lines):
        lines[-1] = lines[-1] + self.letter

class Grave:
    def __init__(self, xml, parent):
        self.parent = parent
        l = xml.attributes["char"].value
        if l == "o":
            self.letter = u"\xf2"
        elif l == "O":
            self.letter = u"\xd2"
        elif l == "a":
            self.letter = u"\xe0"
        elif l == "A":
            self.letter = u"\xc0"
        else:
            print "Warning, %c is an unrecognized letter for Grave." % (l, )
            print "Please report this as a bug to the Doxygen2GWiki project."
            self.letter = ""

    def getLines(self, lines):
        lines[-1] = lines[-1] + self.letter

class Circ:
    def __init__(self, xml, parent):
        self.parent = parent
        l = xml.attributes["char"].value
        if l == "a":
            self.letter = u"\xe2"
        elif l == "A":
            self.letter = u"\xc2"
        else:
            print "Warning, %c is an unrecognized letter for Circ." % (l, )
            print "Please report this as a bug to the Doxygen2GWiki project."
            self.letter = ""

    def getLines(self, lines):
        lines[-1] = lines[-1] + self.letter

class Tilde:
    def __init__(self, xml, parent):
        self.parent = parent
        l = xml.attributes["char"].value
        if l == "a":
            self.letter = u"\xe3"
        elif l == "A":
            self.letter = u"\xc3"
        else:
            print "Warning, %c is an unrecognized letter for Tilde." % (l, )
            print "Please report this as a bug to the Doxygen2GWiki project."
            self.letter = ""

    def getLines(self, lines):
        lines[-1] = lines[-1] + self.letter

class Szlig:
    def __init__(self, xml, parent):
        self.parent = parent

    def getLines(self, lines):
        lines[-1] = lines[-1] + u"\xdf"

class Cedil:
    def __init__(self, xml, parent):
        self.parent = parent
        l = xml.attributes["char"].value
        if l == "c":
            self.letter = u"\xe7"
        elif l == "C":
            self.letter = u"\xc7"
        else:
            print "Warning, %c is an unrecognized letter for Cedil." % (l, )
            print "Please report this as a bug to the Doxygen2GWiki project."
            self.letter = ""

    def getLines(self, lines):
        lines[-1] = lines[-1] + self.letter

class Ring:
    def __init__(self, xml, parent):
        self.parent = parent
        l = xml.attributes["char"].value
        if l == "a":
            self.letter = u"\xe5"
        elif l == "A":
            self.letter = u"\xc5"
        else:
            print "Warning, %c is an unrecognized letter for Ring." % (l, )
            print "Please report this as a bug to the Doxygen2GWiki project."
            self.letter = ""

    def getLines(self, lines):
        lines[-1] = lines[-1] + self.letter

class Context:
    def __init__(self):
        self.text = None


elements = {
    "highlight": Highlight,
    "bold": Bold,
    "emphasis": Emphasis,
    "para": Para,
    "sect1": Sect1,
    "sect2": Sect2,
    "anchor": Ignore,
    "htmlonly": Ignore,
    "latexonly": Ignore,
    "hruler": Ignore,
    "indexentry": Ignore,
    "verbatim": Verbatim,
    "preformatted": Verbatim,
    "computeroutput": ComputerOutput,
    "heading": Heading,
    "title": Heading,
    "ulink": ULink,
    "ref": Ref,
    "toclist": TocList,
    "tocitem": TocItem,
    "itemizedlist": ItemizedList,
    "orderedlist": OrderedList,
    "simplesect": SimpleSect,
    "linebreak": LineBreak,
    "programlisting": ProgramListing,
    "superscript": SuperScript,
    "subscript": SubScript,
    "sp": Space,
    "variablelist": VariableList,
    "varlistentry": Ignore,
    "term": PassThrough,
    "image": Image,
    "formula": Formula,
    "center": PassThrough,
    "copy": Copy,
    "umlaut": Umlaut,
    "acute": Acute,
    "grave": Grave,
    "nonbreakablespace": NonBreakableSpace,
    "ndash": NDash,
    "mdash": MDash,
    "circ": Circ,
    "tilde": Tilde,
    "szlig": Szlig,
    "cedil": Cedil,
    "ring": Ring,
    "parameterlist": Ignore
}

sections = {
    "see": Ignore,
    "return": Ignore,
    "author": Ignore,
    "authors": Ignore,
    "version": Ignore,
    "since": Ignore,
    "date": Ignore,
    "note": PassThrough,
    "warning": PassThrough,
    "pre": Ignore,
    "post": Ignore,
    "invariant": Ignore,
    "remark": Ignore,
    "attention": PassThrough,
    "par" : PassThrough,
    "rcs" : Para
}
