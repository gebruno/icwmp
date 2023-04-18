#!/bin/bash

echo "preparation script"
pwd
. ./gitlab-ci/shared.sh
. ./test/script/common.sh

trap cleanup EXIT
trap cleanup SIGINT

echo "Compiling icmwp"
build_icwmp

echo "Starting dependent services"
supervisorctl update
sleep 2
supervisorctl restart all
sleep 2
supervisorctl stop icwmpd
sleep 2
supervisorctl status all

echo "Configuring genieacs"
configure_genieacs

mkdir -p /var/state/icwmpd 

echo "Starting icwmpd deamon"
supervisorctl start icwmpd
sleep 10

echo "Checking cwmp status"
check_cwmp_status

[ -f funl-test-result.log ] && rm -f funl-test-result.log

sleep 10
echo "## Running script verification of functionalities ##"
echo > ./funl-test-result.log
echo > ./funl-test-debug.log
test_num=0

for test in $(ls test/script/0*.sh); do
	test=$(basename ${test})
	test_num=$(( test_num + 1 ))

	echo "#### Start $test ####" >> "$icwmp_master_log"
	./test/script/${test}
	if [ "$?" -eq 0 ]; then
		echo "ok ${test_num} - ${test}" >> ./funl-test-result.log
		remove_icwmp_log
		echo "#### $test Done ####" >> "$icwmp_master_log"
		echo "#### $test Done ####"
	else
		echo "not ok ${test_num} - ${test}" >> ./funl-test-result.log
		remove_icwmp_log
		echo "#### $test Ended with error ####" >> "$icwmp_master_log"
		echo "#### $test Ended with error ####"
	fi
done

echo "Stop all services"
supervisorctl stop icwmpd

sleep 10
check_valgrind_xml

cp test/files/etc/config/users /etc/config/
cp test/files/etc/config/wireless /etc/config/

echo "Verify Custom notifications"
echo "#### Start custom_notifications ####" >> "$icwmp_master_log"
./test/script/verify_custom_notifications.sh
if [ "$?" -eq 0 ]; then
	echo "ok - verify_custom_notifications" >> ./funl-test-result.log
	remove_icwmp_log
	echo "#### Done custom_notifications ####" >> "$icwmp_master_log"
else
	echo "not ok - verify_custom_notifications" >> ./funl-test-result.log
	remove_icwmp_log
	echo "#### custom_notifications ended with error ####" >> "$icwmp_master_log"
fi

test_num=$(( test_num + 1 ))
echo "1..${test_num}" >> ./funl-test-result.log

# Artefact
gcovr -r . 2> /dev/null --xml -o ./funl-test-coverage.xml
#GitLab-CI output
gcovr -r . 2> /dev/null

#report part
exec_cmd tap-junit --input ./funl-test-result.log --output report

sleep 10
check_valgrind_xml

date +%s > timestamp.log

echo "Functional test :: PASS"
