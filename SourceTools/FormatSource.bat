@echo off

call :IndentFile ..\Source\Common, *.h
call :IndentFile ..\Source\Common, *.cpp
call :IndentFile ..\Source\Common\AngelScriptExt, *.h
call :IndentFile ..\Source\Common\AngelScriptExt, *.cpp
call :IndentFile ..\Source\Server, *.h
call :IndentFile ..\Source\Server, *.cpp
call :IndentFile ..\Source\Client, *.h
call :IndentFile ..\Source\Client, *.cpp
call :IndentFile ..\Source\Editor, *.h
call :IndentFile ..\Source\Editor, *.cpp
call :IndentFile ..\Source\Applications, *.cpp

exit /B 0

:IndentFile
	for /F "tokens=*" %%G in ('dir /B .\%~1\%~2') do (
		echo Indenting file "%~1\%%G"
		"uncrustify.exe" -f "%~1\%%G" -c "uncrustify.cfg" -o indentoutput.tmp
		move /Y indentoutput.tmp "%~1\%%G"
	)
exit /B 0
