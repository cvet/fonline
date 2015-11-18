import re
with open('../../include/angelscript.h') as f:
    for l in f.readlines():
        if l.startswith('#define ANGELSCRIPT_VERSION_STRING'):
            print(re.search('[\d]+\.[\d]+\.[\d]+', l).group(0))
