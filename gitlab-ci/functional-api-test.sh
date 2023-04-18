#!/bin/bash

echo "preparation script"
pwd
. ./gitlab-ci/shared.sh

trap cleanup EXIT
trap cleanup SIGINT

echo "Compiling icmwp"
build_icwmp

mkdir -p /var/state/icwmpd
echo "Starting dependent services"
supervisorctl update
supervisorctl restart all
sleep 10
supervisorctl status all
exec_cmd ubus wait_for bbfdm tr069

# wait until cwmp status is up
check_cwmp_status

echo "Running the api test cases"
ubus-api-validator -f ./test/api/json/tr069.validation.json > ./api-test-result.log
check_ret $?

sleep 5

echo "Stop all services"
supervisorctl stop icwmpd

# Artefact
gcovr -r . 2> /dev/null --xml -o ./api-test-coverage.xml
#GitLab-CI output
gcovr -r . 2> /dev/null

#report part
exec_cmd tap-junit --input ./api-test-result.log --output report

check_valgrind_xml

date +%s > timestamp.log

echo "Functional API test :: PASS"
