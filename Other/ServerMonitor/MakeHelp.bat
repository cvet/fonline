@echo off
REM -- First make map file from Microsoft Visual C++ generated resource.h
echo // MAKEHELP.BAT generated Help Map file.  Used by SERVERMONITOR.HPJ. >"hlp\ServerMonitor.hm"
echo. >>"hlp\ServerMonitor.hm"
echo // Commands (ID_* and IDM_*) >>"hlp\ServerMonitor.hm"
makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\ServerMonitor.hm"
echo. >>"hlp\ServerMonitor.hm"
echo // Prompts (IDP_*) >>"hlp\ServerMonitor.hm"
makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\ServerMonitor.hm"
echo. >>"hlp\ServerMonitor.hm"
echo // Resources (IDR_*) >>"hlp\ServerMonitor.hm"
makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\ServerMonitor.hm"
echo. >>"hlp\ServerMonitor.hm"
echo // Dialogs (IDD_*) >>"hlp\ServerMonitor.hm"
makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\ServerMonitor.hm"
echo. >>"hlp\ServerMonitor.hm"
echo // Frame Controls (IDW_*) >>"hlp\ServerMonitor.hm"
makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\ServerMonitor.hm"
REM -- Make help for Project SERVERMONITOR


echo Building Win32 Help files
start /wait hcw /C /E /M "hlp\ServerMonitor.hpj"
if errorlevel 1 goto :Error
if not exist "hlp\ServerMonitor.hlp" goto :Error
if not exist "hlp\ServerMonitor.cnt" goto :Error
echo.
if exist Debug\nul copy "hlp\ServerMonitor.hlp" Debug
if exist Debug\nul copy "hlp\ServerMonitor.cnt" Debug
if exist Release\nul copy "hlp\ServerMonitor.hlp" Release
if exist Release\nul copy "hlp\ServerMonitor.cnt" Release
echo.
goto :done

:Error
echo hlp\ServerMonitor.hpj(1) : error: Problem encountered creating help file

:done
echo.
