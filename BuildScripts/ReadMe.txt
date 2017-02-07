gocd:fgt43trfcvq23

FO_BUILD_DEST=FOnlineBuild
FO_SOURCE=FOnlineSource
FO_FTP_DEST=109.167.147.160 (optional)
FO_FTP_USER=lf:qwerty (optional)
FO_COPY_DEST=D:\_COPY (optional)

Setup Windows:
Download and install gocd
Download and install cmake
Create user go/password (admin), assign to Go services.

Setup Linux:
curl https://download.gocd.io/GOCD-GPG-KEY.asc | sudo apt-key add -
sudo apt-get update
sudo adduser go sudo
echo "%sudo ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

Setup Mac:
Download and install gocd
Download and install cmake
