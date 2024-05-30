#!/bin/bash

echo "install dependencies for unit-test script"
pwd

. ./gitlab-ci/shared.sh

# install required packages
apt update > /dev/null 2>&1
apt install -y jq uuid-dev libmxml-dev >/dev/null 2>&1

echo "Installing bbfdmd"
install_bbfdmd

#install WiFi Data Model as Plugin
echo "Installing wifidmd as plugin"
install_wifidmd_as_plugin