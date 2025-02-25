#!/bin/bash

source ./test/script/common.sh
source ./gitlab-ci/shared.sh

TEST_NAME="GET RPC Method"

echo "Running: $TEST_NAME"

remove_icwmp_log
curl $connection_request_path -X POST --data '{"name": "getParameterValues", "parameterNames": ["Device.X_IOWRT_EU_Dropbear.1.Alias"] }' >/dev/null 2>&1
check_ret $?
wait_for_session_end
check_session "GetParameterValues"
param_value=$(print_tag_value "cwmp:GetParameterValuesResponse" "Value xsi:type=\"xsd:string\"")
if [ "$param_value" != "cpe-1" ]; then
	echo "Error: Default value of 'Device.X_IOWRT_EU_Dropbear.1.Alias' is wrong, current_value($param_value) expected_value(cpe-1)" >> ./funl-test-debug.log
	exit 1
fi

echo "PASS: $TEST_NAME"
