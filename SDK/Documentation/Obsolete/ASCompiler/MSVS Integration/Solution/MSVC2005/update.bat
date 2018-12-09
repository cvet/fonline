@echo off
echo Note: 2008 solution has to be updated before this file is run.
echo You also need the sed utility.
more ..\Server.vcproj | sed s/Version=\"9.00\"/Version=\"8.00\"/ | sed s/RelativePath=\"..\\/RelativePath=\"..\\..\\/ | sed s/RelativePath=\".\\intellisense/RelativePath=\"..\\intellisense/ > Server.vcproj 
more ..\Client.vcproj | sed s/Version=\"9.00\"/Version=\"8.00\"/ | sed s/RelativePath=\"..\\/RelativePath=\"..\\..\\/ | sed s/RelativePath=\".\\intellisense/RelativePath=\"..\\intellisense/ > Client.vcproj 
more ..\Mapper.vcproj | sed s/Version=\"9.00\"/Version=\"8.00\"/ | sed s/RelativePath=\"..\\/RelativePath=\"..\\..\\/ | sed s/RelativePath=\".\\intellisense/RelativePath=\"..\\intellisense/ > Mapper.vcproj 
echo Finished. Open the solution and re-save it using MSVC2005.
pause