#!/bin/sh

# newer uncrustify (tested v0.60, Fedora 19) produces different output,
# use provided windows version instead
uncrustify="wine SourceTools/uncrustify.exe"

for file in *.cpp *.h; do
	$uncrustify -c SourceTools/uncrustify.cfg --no-backup $file
done
