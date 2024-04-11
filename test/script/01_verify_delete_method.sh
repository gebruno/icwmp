#!/bin/bash

source ./test/script/common.sh
source ./gitlab-ci/shared.sh

TEST_NAME="DELETE RPC Method"

echo "Running: $TEST_NAME"

remove_icwmp_log
curl $connection_request_path -X POST --data '{"name": "deleteObject","objectName":"Device.X_IOPSYS_EU_Dropbear.2"}' >/dev/null 2>&1
check_ret $?
wait_for_session_end
check_session "DeleteObject"
status=$(print_tag_value "cwmp:DeleteObjectResponse" "Status")
if [ "$status" != "1" ]; then
	echo "Error: Delete Object Method doesn't work correctly, current_value($status) expected_value(1)" >> ./funl-test-debug.log
	exit 1
fi

remove_icwmp_log
curl $connection_request_path -X POST --data '{"name": "getParameterValues", "parameterNames": ["Device.X_IOPSYS_EU_Dropbear"] }' >/dev/null 2>&1
check_ret $?
wait_for_session_end
check_session "GetParameterValues"
if grep -q "Device.X_IOPSYS_EU_Dropbear.2" "$icwmp_log_file"; then
	echo "Error: 'Device.X_IOPSYS_EU_Dropbear.2' object is not really deleted" >> ./funl-test-debug.log
	exit 1
fi

echo "PASS: $TEST_NAME"
