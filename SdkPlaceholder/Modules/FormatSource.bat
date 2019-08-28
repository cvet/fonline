@echo off

FOR /F "tokens=*" %%G IN ('DIR /B /S .\*.fos') DO (
echo Indenting file "%%G"
"..\Binaries\SourceTools\uncrustify.exe" -f "%%G" -c "..\Binaries\SourceTools\uncrustify.cfg" -o indentoutput.tmp
move /Y indentoutput.tmp "%%G"
)
