#!/bin/bash -ex

#sudo apt-get -y update || true
#sudo apt-get -y install lftp

if [ -n "$FO_FTP_DEST" ]; then
	lftp -e "rm -r Client; exit" -u $FO_FTP_USER $FO_FTP_DEST || true
	lftp -e "rm -r Server; exit" -u $FO_FTP_USER $FO_FTP_DEST || true
	lftp -e "rm -r Mapper; exit" -u $FO_FTP_USER $FO_FTP_DEST || true
	lftp -e "rm -r ASCompiler; exit" -u $FO_FTP_USER $FO_FTP_DEST || true
fi
