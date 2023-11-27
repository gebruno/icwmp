#!/bin/bash

echo "install dependencies for unit-test script"
pwd

. ./gitlab-ci/shared.sh

# install required packages
apt update > /dev/null 2>&1
apt install -y jq uuid-dev >/dev/null 2>&1

cd /opt/dev/
git clone -b v3.3.1 https://github.com/michaelrsweet/mxml.git
cd mxml
./configure
make
make install

echo "Installing bbfdmd"
install_bbfdmd
