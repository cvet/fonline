cd doxygen
doxygen doxygen.cfg > doxy_log.txt
@echo off
@echo Creating doc shortcut...
MakeShortcut "..\html\index.html" "" "..\DocIndex.lnk" ""
@echo ##############################
@echo ### NOTE ON CHM GENERATION ###
@echo ##############################
@echo The *.chm doc file is only generated
@echo if the Microsoft HTML Help Workshop
@echo is installed in the folder:
@echo C:\Program Files\HTML Help Workshop\
@echo You can change this folder by modifying the
@echo directory of the value HHC_LOCATION
@echo in mkdoc/doxygen.cfg
@echo ##############################
@echo ##############################
@echo on