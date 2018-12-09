@echo off

REM Set base directories

set doxygen=.\tools\doxygen\bin\win32\doxygen
set doxygen2wiki=.\tools\doxygen2wiki
set output=.\output

REM Choose doxygen configuration

set doxygenconfig=

if "%1" == "ru" set doxygenconfig=FOnlineDoc.cfg
if "%1" == "en" set doxygenconfig=FOnlineDoc.en.cfg

if "%doxygenconfig%" == "" (
  echo Error: unknown language [%1]
  goto EXIT
)

REM Create necessary directories

if not exist %output% mkdir %output%
if ERRORLEVEL 1 (
  echo Error: cannot create output directory [%output%]
  goto EXIT
)

REM Build

echo # Running Doxygen #

%doxygen% %doxygenconfig%

if "%2"=="-nowiki" goto EXIT

echo # Creating wiki pages #

if not exist %output%\%1\wiki mkdir %output%\%1\wiki
if ERRORLEVEL 1 (
  echo Error: cannot create output directory for wiki pages [%output%\%1\wiki]
  goto EXIT
)

%python% %doxygen2wiki%\cheetah.py compile --flat --nobackup -R --idir %doxygen2wiki%\templates\%1 --odir %doxygen2wiki%\src\templates\

if ERRORLEVEL 1 (
  echo Error: templates cannnot be created. Check Cheetah installation or Cheetah error messages.
  goto EXIT
)

%python% %doxygen2wiki%\doxygen2wiki.py -d %output%\%1\xml -o %output%\%1\wiki -v

:EXIT
