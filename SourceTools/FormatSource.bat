@echo off

call :IndentFile ..\Source, *.h
call :IndentFile ..\Source, *.cpp

exit /B 0

:IndentFile
	for /F "tokens=*" %%G in ('dir /B /S .\%~1\%~2') do (
		echo Indenting file "%%G"
		"uncrustify.exe" -f "%%G" -c "uncrustify.cfg" -o indentoutput.tmp -q -l CPP
		move /Y indentoutput.tmp "%%G"
	)
exit /B 0
