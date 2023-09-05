#!/bin/bash

echo "preparation script"
pwd
. ./gitlab-ci/shared.sh

trap cleanup EXIT
trap cleanup SIGINT

[ -f /etc/icwmpd/cwmp_notifications ] && echo "" >/etc/icwmpd/cwmp_notifications || touch /etc/icwmpd/cwmp_notifications

echo "Compiling icmwp"
build_icwmp

echo "Starting dependent services"
supervisorctl update
sleep 2
supervisorctl restart all
sleep 2
supervisorctl stop icwmpd
sleep 5
supervisorctl status all
ubus wait_for bbfdm

echo "Clean cmocka"
make clean -C test/cmocka/

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
gcovr -r . 2> /dev/null --xml -o ./unit-test-coverage.xml

echo "Unit test PASS"
