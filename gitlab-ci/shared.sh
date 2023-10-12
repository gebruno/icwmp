#!/bin/bash

function cleanup()
{
	echo ""
}

function check_ret()
{
	ret=$1
	if [ "$ret" -ne 0 ]; then
		echo "Validation of last command failed, ret(${ret})"
		exit $ret
	fi

}

function error_on_zero()
{
	ret=$1
	if [ "$ret" -eq 0 ]; then
		echo "Validation of last command failed, ret(${ret})"
		exit 1
	fi

}

function exec_cmd()
{
	echo "executing $@"
	$@ >/dev/null 2>&1

	if [ $? -ne 0 ]; then
		echo "Failed to execute $@"
		exit 1
	fi
}

function configure_acs()
{
	echo "Create a new ACS User"
	curl -X POST 'http://acs:3000/init' -H "Content-Type: application/json" --data '{"users": true, "presets": true, "filters": true, "device": true, "index": true, "overview": true}' >/dev/null 2>&1
	check_ret $?

	echo "Delete the default provision inform from ACS"
	curl -X DELETE 'http://acs:7557/provisions/inform' >/dev/null 2>&1
	check_ret $?

	echo "Add a new provision inform in ACS"
	curl -X PUT 'http://acs:7557/provisions/inform' --data-binary '@/tmp/connection_request_auth' >/dev/null 2>&1
	check_ret $?

	#echo "get the supported provisions"
	#curl -X GET 'http://acs:7557/provisions/'
	#check_ret $?

	#echo "Upload firmware image to ACS server"
	#exec_cmd dd if=/dev/zero of=/tmp/firmware_v1.0.bin bs=25MB count=1
	#echo "Valid" > /tmp/firmware_v1.0.bin
	#curl -X PUT 'http://localhost:7557/files/firmware_v1.0.bin' --data-binary '@/tmp/firmware_v1.0.bin' --header "fileType: 1 Firmware Upgrade Image" --header "oui: XXX" --header "productClass: FirstClass" --header "version: 000000001" >/dev/null 2>&1
	#check_ret $?
}

function check_cwmp_status()
{
	echo "icwmp status"
	iter=0
	state=0
	while [ $iter -lt 10 ]; do
		status=`ubus call tr069 status | jq -r ".cwmp.status"`
		if [ "${status}" == "up" ]; then
			state=1
			break
		fi

		iter=$(( $iter + 1))
		sleep 2
	done

	if [ $state -eq 0 ]; then
		echo "icwmpd is not started correctly, (the current status=${status})"
		exit 1
	fi
}

function clean_icwmp()
{
	if [ -d build ]; then
		rm -rf build
	fi

	exec_cmd make -C test/cmocka clean
	find -name '*.gcda' -exec rm {} -fv \;
	find -name '*.gcno' -exec rm {} -fv \;
	find -name '*.gcov' -exec rm {} -fv \;
	find -name '*.deps' -exec rm {} -rfv \;
	find -name '*.so' -exec rm {} -fv \;
	rm -f *.o *.log *.xml vgcore.* firmware_v1.0.bin
	rm -rf report
}

function build_icwmp()
{
	COV_CFLAGS='-g -O0 -fprofile-arcs -ftest-coverage'
	COV_LDFLAGS='--coverage'

	BINP="${PWD}"
	# clean icwmp
	clean_icwmp

	# compile icwmp
	mkdir -p build
	cd build
	cmake ../ -DCMAKE_C_FLAGS="$COV_CFLAGS " -DCMAKE_VERBOSE_MAKEFILE=ON -DCMAKE_EXE_LINKER_FLAGS="$COV_LDFLAGS" -DWITH_OPENSSL=ON -DCMAKE_INSTALL_PREFIX=/
	exec_cmd make

	echo "installing icwmpd binary and libcwmpdm.so library"
	exec_cmd cp icwmpd ../
	exec_cmd cp libcwmpdm.so ../
	exec_cmd make install
	[ -f "/usr/sbin/icwmpd" ] && rm /usr/sbin/icwmpd
	exec_cmd ln -s ${BINP}/icwmpd /usr/sbin/icwmpd
	cd ..
}

function install_bbfdmd()
{
	[ -d "/opt/dev/bbfdm" ] && rm -rf /opt/dev/bbfdm

	if [ -n "${BBFDM_BRANCH}" ]; then
		exec_cmd git clone -b ${BBFDM_BRANCH} https://dev.iopsys.eu/bbf/bbfdm.git /opt/dev/bbfdm
	else
		exec_cmd git clone -b release-7.2 https://dev.iopsys.eu/bbf/bbfdm.git /opt/dev/bbfdm
	fi

	cd /opt/dev/bbfdm
	exec_cmd ./gitlab-ci/install-dependencies.sh install
	exec_cmd ./gitlab-ci/setup.sh install
}

function check_valgrind_xml() {
	echo "Checking memory leaks..."
	cp /tmp/memory-report.xml memory-report.xml

	echo "checking UninitCondition"
	grep -q "<kind>UninitCondition</kind>" /tmp/memory-report.xml
	error_on_zero $?

	echo "checking Leak_PossiblyLost"
	grep -q "<kind>Leak_PossiblyLost</kind>" /tmp/memory-report.xml
	error_on_zero $?

	echo "checking Leak_DefinitelyLost"
	grep -q "<kind>Leak_DefinitelyLost</kind>" /tmp/memory-report.xml
	error_on_zero $?

	echo "checking Leak_StillReachable"
	grep -q "<kind>Leak_StillReachable</kind>" /tmp/memory-report.xml
	error_on_zero $?
}
