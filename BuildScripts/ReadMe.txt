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
echo "deb https://download.gocd.io/" | sudo tee /etc/apt/sources.list.d/gocd.list
curl https://download.gocd.io/GOCD-GPG-KEY.asc | sudo apt-key add -
sudo apt-get update
sudo apt-get install go-agent
sudo gedit /etc/default/go-agent GO_SERVER=...
sudo adduser go sudo
sudo -i
echo "%sudo ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
reboot

Setup Mac:
Download and install gocd
Download and install cmake
