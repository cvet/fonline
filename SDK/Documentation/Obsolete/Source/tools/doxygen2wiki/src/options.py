from optparse import OptionParser

parser = OptionParser()
parser.add_option("-d", "--docs", dest="docs", default="../../output/ru/xml/",
                  help="Get documentation from this directory. (Default: ../../output/ru/xml/)")
parser.add_option("-o", "--output", dest="output", default="../../output/ru/wiki/",
                  help="Place the generated wiki pages in this directory. (Default: ../../output/ru/wiki/)")
parser.add_option("-p", "--prefix", dest="prefix", default="Doxygen",
                  help="Use this prefix when generating wiki pages. (Default: Doxygen)")
parser.add_option("-l", "--label", dest="labels", default=[], action="append",
                  help="Apply this label to all pages.")
parser.add_option("-n", "--no-labels", dest="no_labels", default=False, action="store_true",
                  help="Don't apply any labels to the pages. (Overrides any --label options)")
parser.add_option("-j", "--project", dest="project", default=None, action="store",
                  help="The name of the project the Wiki code will be commited to.")
parser.add_option("-t", "--html", dest="html", default=None, action="store",
                  help="The location of the Doxygen generated HTML files.")
parser.add_option("-i", "--image", dest="images", default=[], action="append",
                  help="The directory containing images used in the documentation.")
parser.add_option("-v", "--verbose", dest="verbose", default=False, action="store_true",
                  help="Make lots of noise.")

(options, args) = parser.parse_args()

if not options.docs.endswith("/"):
    options.docs = options.docs + "/"
if not options.output.endswith("/"):
    options.output = options.output + "/"
if options.html is None:
    options.html = options.docs + "../html/"
if options.html and not options.html.endswith("/"):
    options.html = options.html + "/"
for i in range(len(options.images)):
    if not options.images[i].endswith("/"):
        options.images[i] = options.images[i] + "/"
