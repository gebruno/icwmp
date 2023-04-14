#!/bin/bash

echo "install dependencies for unit-test script"
pwd

. ./gitlab-ci/shared.sh

# install required packages
apt update
apt install -y mongodb jq uuid-dev libmxml-dev

# install genieacs
exec_cmd npm install -g genieacs@1.2.9
ln -sf /root/.nvm/versions/node/v14.16.1/bin/genieacs-cwmp /usr/sbin/genieacs-cwmp
ln -sf /root/.nvm/versions/node/v14.16.1/bin/genieacs-fs /usr/sbin/genieacs-fs  
ln -sf /root/.nvm/versions/node/v14.16.1/bin/genieacs-ui /usr/sbin/genieacs-ui
ln -sf /root/.nvm/versions/node/v14.16.1/bin/genieacs-nbi /usr/sbin/genieacs-nbi
mkdir -p /data/db

echo "Installing bbfdmd"
install_bbfdmd

# install usermngr plugin
cd -
rm -rf /opt/dev/usermngr
exec_cmd git clone https://dev.iopsys.eu/iopsys/usermngr.git /opt/dev/usermngr

echo "Compiling libusermngr"
make clean -C /opt/dev/usermngr/src
make -C /opt/dev/usermngr/src

echo "Installing libusermngr"
cp -f /opt/dev/usermngr/src/libusermngr.so /usr/lib/bbfdm
