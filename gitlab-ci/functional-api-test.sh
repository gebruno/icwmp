#!/bin/bash

echo "preparation script"
pwd
. ./gitlab-ci/shared.sh
. ./test/script/common.sh

trap cleanup EXIT
trap cleanup SIGINT

echo "Add X_IOWRT_EU_Dropbear Object that is needed for functional test"
DROPBEAR_OBJECT='{"parent_dm": "Device.", "object": "X_IOWRT_EU_Dropbear"}'
jq --argjson newObj "$DROPBEAR_OBJECT" '.daemon.services += [$newObj]' "/etc/bbfdm/services/core.json" > /tmp/updated_core.json
mv /tmp/updated_core.json /etc/bbfdm/services/core.json

supervisorctl restart bbfdmd

echo "Compiling icmwp"
build_icwmp

echo "Configuring ACS"
configure_acs

mkdir -p /var/log
mkdir -p /var/state/icwmpd 

echo "Starting Services..."
cp ./gitlab-ci/icwmp-dm.conf /etc/supervisor/conf.d/
cp ./gitlab-ci/icwmp.conf /etc/supervisor/conf.d/
supervisorctl reread
supervisorctl update
sleep 20
supervisorctl status

echo "Checking cwmp status"
check_cwmp_status
supervisorctl status all

echo > ./functional-api-result.txt
echo > ./funl-test-debug.txt

echo "Running the api test cases"
ubus-api-validator -t 10 -f ./test/api/json/tr069.validation.json > ./api-test-result.txt
check_ret $?

sleep 5
echo "## Running script verification of functionalities ##"

test_num=0
for test in $(cat test/script/run_sequence.txt); do
	test=$(basename ${test})
	test_num=$(( test_num + 1 ))

	echo "#### Start $test ####" >> "$icwmp_master_log"
	./test/script/${test}
	if [ "$?" -eq 0 ]; then
		echo "ok ${test_num} - ${test}" >> ./functional-api-result.txt
		remove_icwmp_log
		echo "#### $test Done ####" >> "$icwmp_master_log"
		echo "#### $test Done ####"
	else
		echo "not ok ${test_num} - ${test}" >> ./functional-api-result.txt
		remove_icwmp_log
		echo "#### $test Ended with error ####" >> "$icwmp_master_log"
		echo "#### $test Ended with error ####"
	fi
	sleep 1
done

echo "Stop all services"
supervisorctl stop icwmpd

sleep 10
check_valgrind_xml

echo "Verify Custom notifications"
echo "#### Start custom_notifications ####" >> "$icwmp_master_log"
./test/script/verify_custom_notifications.sh
if [ "$?" -eq 0 ]; then
	echo "ok - verify_custom_notifications" >> ./functional-api-result.txt
	remove_icwmp_log
	echo "#### Done custom_notifications ####" >> "$icwmp_master_log"
else
	echo "not ok - verify_custom_notifications" >> ./functional-api-result.txt
	remove_icwmp_log
	echo "#### custom_notifications ended with error ####" >> "$icwmp_master_log"
fi

test_num=$(( test_num + 1 ))
echo "1..${test_num}" >> ./functional-api-result.txt

# Artefact
gcovr -r . 2> /dev/null --xml -o ./funl-test-coverage.xml
#GitLab-CI output
gcovr -r . 2> /dev/null

#report part
exec_cmd tap-junit --input ./functional-api-result.txt --output report

sleep 10
check_valgrind_xml

echo "Functional API test :: PASS"
