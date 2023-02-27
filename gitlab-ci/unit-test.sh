#!/bin/bash

echo "preparation script"
pwd
. ./gitlab-ci/shared.sh

trap cleanup EXIT
trap cleanup SIGINT

if [ ! -f /etc/icwmpd/cwmp_notifications ]; then touch /etc/icwmpd/cwmp_notifications; fi

echo "Configure download server"
configure_download_firmware

rm /etc/supervisor/conf.d/*.conf
cp ./gitlab-ci/iopsys-supervisord-unit.conf /etc/supervisor/conf.d/

echo "Starting dependent services"
supervisorctl status all
supervisorctl update
supervisorctl restart all
supervisorctl stop icwmpd
ubus wait_for usp.raw
supervisorctl status all

echo "Clean cmocka"
make clean -C test/cmocka/

echo "icwmp datamodel install"
make -C test/cmocka libcwmpdm
mkdir -p /usr/lib/bbfdm
cp test/cmocka/libcwmpdm.so /usr/lib/bbfdm
supervisorctl stop uspd 
supervisorctl start uspd

echo "Running unit test"
make -C test/cmocka all
check_ret $?

echo "Stop dependent services"
supervisorctl stop all
supervisorctl status

#report part
#GitLab-CI output
gcovr -r . 2> /dev/null #throw away stderr
# Artefact

set -x
pwd && ls -la

gcovr -r . 2> /dev/null --xml -o ./unit-test-coverage.xml
date +%s > timestamp.log

echo "Unit test PASS"
