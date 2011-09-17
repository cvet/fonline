@echo off

FOR /F "tokens=*" %%G IN ('DIR /B .\*.h') DO (
echo Indenting file "%%G"
"SourceTools\uncrustify.exe" -f "%%G" -c "SourceTools\uncrustify.cfg" -o indentoutput.tmp
move /Y indentoutput.tmp "%%G"
)

FOR /F "tokens=*" %%G IN ('DIR /B .\*.cpp') DO (
echo Indenting file "%%G"
"SourceTools\uncrustify.exe" -f "%%G" -c "SourceTools\uncrustify.cfg" -o indentoutput.tmp
move /Y indentoutput.tmp "%%G"
)
