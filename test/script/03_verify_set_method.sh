#!/bin/bash

source ./test/script/common.sh
source ./gitlab-ci/shared.sh

TEST_NAME="SET RPC Method"

echo "Running: $TEST_NAME"

remove_icwmp_log
curl $connection_request_path -X POST --data '{"name": "getParameterValues", "parameterNames": ["Device.X_IOPSYS_EU_Dropbear.1.PasswordAuth"] }' >/dev/null 2>&1
check_ret $?
wait_for_session_end
check_session "GetParameterValues"
param_value_before=$(print_tag_value "cwmp:GetParameterValuesResponse" "Value xsi:type=\"xsd:boolean\"")
if [ "$param_value_before" != "1" ]; then
	echo "Error: Default value of 'Device.X_IOPSYS_EU_Dropbear.1.PasswordAuth' is wrong, current_value($param_value_before) expected_value(1)" >> ./funl-test-debug.log
	exit 1
fi

remove_icwmp_log
curl $connection_request_path -X POST --data '{"name": "setParameterValues", "parameterValues": [["Device.X_IOPSYS_EU_Dropbear.1.PasswordAuth",false]]}' >/dev/null 2>&1
check_ret $?
wait_for_session_end
check_session "SetParameterValues"
get_status=$(print_tag_value "cwmp:SetParameterValuesResponse" "Status")
if [ "$get_status" != "0" ]; then
	echo "Error: Set Value doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

remove_icwmp_log
curl $connection_request_path -X POST --data '{"name": "getParameterValues", "parameterNames": ["Device.X_IOPSYS_EU_Dropbear.1.PasswordAuth"] }' >/dev/null 2>&1
check_ret $?
wait_for_session_end
check_session "GetParameterValues"
param_value_after=$(print_tag_value "cwmp:GetParameterValuesResponse" "Value xsi:type=\"xsd:boolean\"")
if [ "$param_value_after" != "0" ]; then
	echo "Error: the value of 'Device.X_IOPSYS_EU_Dropbear.1.PasswordAuth' is wrong, current_value($param_value_after) expected_value(0)" >> ./funl-test-debug.log
	exit 1
fi

curl $connection_request_path -X POST --data '{"name": "setParameterValues", "parameterValues": [["Device.X_IOPSYS_EU_Dropbear.1.PasswordAuth",true]]}' >/dev/null 2>&1
wait_for_session_end

echo "PASS: $TEST_NAME"
