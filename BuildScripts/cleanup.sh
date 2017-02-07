#!/bin/bash

sudo apt-get -y update
sudo apt-get -y install lftp

if [ -n "$FO_FTP_DEST" ]; then
	lftp -e "rm -rf Client; exit" -u $FO_FTP_USER $FO_FTP_DEST
	lftp -e "rm -rf Server; exit" -u $FO_FTP_USER $FO_FTP_DEST
	lftp -e "rm -rf Mapper; exit" -u $FO_FTP_USER $FO_FTP_DEST
	lftp -e "rm -rf ASCompiler; exit" -u $FO_FTP_USER $FO_FTP_DEST
fi
