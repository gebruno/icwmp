#!/bin/bash

echo "preparation script"
pwd

[ -d "/opt/dev/bbfdm" ] && cd /opt/dev/bbfdm && ./gitlab-ci/setup.sh && cp -f ./gitlab-ci/bbfdm_services.conf /etc/supervisor/conf.d/ && cd -

cp -rf ./test/files/* /

echo "set ACS url in cwmp uci"
url="http://acs:7547"
uci set cwmp.acs.url=$url
uci commit cwmp
echo "Current ACS URL=$url"

# copy schema for validation test
cp -r ./schemas/ubus/*.json /usr/share/rpcd/schemas/

echo "Configure Download Server"
mkdir -p /tmp/firmware/
dd if=/dev/zero of=/tmp/firmware/firmware_v1.0.bin bs=25MB count=1 >/dev/null 2>&1
echo "Valid" > /tmp/firmware/firmware_v1.0.bin

if=/dev/zero of=/tmp/firmware/invalid_firmware_v1.0.bin bs=25MB count=1 >/dev/null 2>&1
echo "Invalid" > /tmp/firmware/invalid_firmware_v1.0.bin

echo "Starting base services"
cp ./gitlab-ci/icwmp-base.conf /etc/supervisor/conf.d/
supervisorctl reread
supervisorctl update
sleep 5
supervisorctl status
