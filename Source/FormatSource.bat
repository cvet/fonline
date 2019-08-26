@echo off

call :IndentFile Common, *.h
call :IndentFile Common, *.cpp
call :IndentFile Common\AngelScriptExt, *.h
call :IndentFile Common\AngelScriptExt, *.cpp
call :IndentFile Server, *.h
call :IndentFile Server, *.cpp
call :IndentFile Client, *.h
call :IndentFile Client, *.cpp
call :IndentFile Mapper, *.h
call :IndentFile Mapper, *.cpp
call :IndentFile Applications, *.cpp

exit /B 0

:IndentFile
	for /F "tokens=*" %%G in ('dir /B .\%~1\%~2') do (
		echo Indenting file "%~1\%%G"
		"SourceTools\uncrustify.exe" -f "%~1\%%G" -c "SourceTools\uncrustify.cfg" -o indentoutput.tmp
		move /Y indentoutput.tmp "%~1\%%G"
	)
exit /B 0
