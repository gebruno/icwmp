#!/bin/bash

echo "preparation script"
pwd

[ -d "/opt/dev/bbfdm" ] && cd /opt/dev/bbfdm && ./gitlab-ci/setup.sh && cd -
rm -rf /etc/supervisor/conf.d/*.conf
cp ./gitlab-ci/iopsys-supervisord.conf /etc/supervisor/conf.d/

cp -rf ./test/files/* /

echo "set acs url in cwmp uci"
url="http://$(hostname -i):7547"
uci set cwmp.acs.url=$url
uci commit cwmp
echo "Current ACS URL=$url"

# copy schema for validation test
cp -r ./schemas/ubus/*.json /usr/share/rpcd/schemas/

