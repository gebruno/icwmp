#!/bin/bash

source ./test/script/common.sh
source ./gitlab-ci/shared.sh

TEST_NAME="ICWMP COMMAND LINE"

echo "Running: $TEST_NAME"

echo "GET METHOD: Correct Path" >> ./funl-test-debug.log
res=$(./icwmpd -c get Device.DeviceInfo.VendorLogFile.1.Alias 2>&1)
if [[ $res != *"cpe-1"* ]]; then
	echo "Error: Get Method with correct path doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "GET METHOD: Wrong Path" >> ./funl-test-debug.log
res=$(./icwmpd -c get Device.DeviceInfo.VendorLogFile.1.Alia 2>&1)
if [[ $res != *"9005"* ]]; then
	echo "Error: Get Method with wrong path doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "SET METHOD: Correct Path && Value" >> ./funl-test-debug.log
res=$(./icwmpd -c set Device.DeviceInfo.ProvisioningCode 1234 2>&1)
if [[ $res != *"=> 1234"* ]]; then
	echo "Error: Set Method with correct path && value doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "SET METHOD: Wrong Path && Correct Value" >> ./funl-test-debug.log
res=$(./icwmpd -c set Device.DeviceInfo.WrongPath 1234 2>&1)
if [[ $res != *"9005"* ]]; then
	echo "Error: Set Method with wrong path && correct value doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "SET METHOD: Correct Path && Wrong Value" >> ./funl-test-debug.log
res=$(./icwmpd -c set Device.IP.Interface.1.Enable test 2>&1)
if [[ $res != *"9007"* ]]; then
	echo "Error: Set Method with correct path && wrong value doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "GET NAME METHOD: Correct Path && level" >> ./funl-test-debug.log
res=$(./icwmpd -c get_names Device.DeviceInfo.VendorLogFile.1.Alias 0 2>&1)
if [[ $res != *"=> writable"* ]]; then
	echo "Error: Get Name Method with correct path && level doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "GET NAME METHOD: Correct Path && Wrong level" >> ./funl-test-debug.log
res=$(./icwmpd -c get_names Device.DeviceInfo.VendorLogFile.1.Alias 1 2>&1)
if [[ $res != *"9003"* ]]; then
	echo "Error: Get Name Method with correct path && wrong level doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "GET NAME METHOD: Wrong Path && Correct level" >> ./funl-test-debug.log
res=$(./icwmpd -c get_names Device.DeviceInfo.VendorLogFile.1.Ali 0 2>&1)
if [[ $res != *"9005"* ]]; then
	echo "Error: Get Name Method with wrong path && correct level doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "GET NOTIFICATION METHOD: Correct Path" >> ./funl-test-debug.log
res=$(./icwmpd -c get_notif Device.DeviceInfo.SoftwareVersion 2>&1)
if [[ $res != *"=> active"* ]]; then
	echo "Error: Get Notification Method with correct path doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "GET NOTIFICATION METHOD: Wrong Path" >> ./funl-test-debug.log
res=$(./icwmpd -c get_notif Device.DeviceInfo.VendorLogFile.1.Ali 2>&1)
if [[ $res != *"9005"* ]]; then
	echo "Error: Get Notification Method with wrong path doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "SET NOTIFICATION METHOD: Correct Path" >> ./funl-test-debug.log
res=$(./icwmpd -c set_notif Device.DeviceInfo.VendorLogFile.1.Alias 2 2>&1)
if [[ $res != *"=> 2"* ]]; then
	echo "Error: Set Notification Method with correct path doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "SET NOTIFICATION METHOD: Wrong Path" >> ./funl-test-debug.log
res=$(./icwmpd -c set_notif Device.DeviceInfo.VendorLogFile.1.Ali 1 2>&1)
if [[ $res != *"9005"* ]]; then
	echo "Error: Set Notification Method with wrong path doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "SET NOTIFICATION METHOD: Correst Path forced active notification" >> ./funl-test-debug.log
res=$(./icwmpd -c set_notif Device.DeviceInfo.SoftwareVersion 1 2>&1)
if [[ $res != *"9009"* ]]; then
	echo "Error: Set Notification Method with correct path forced active notification doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "ADD METHOD: Correct Path" >> ./funl-test-debug.log
res=$(./icwmpd -c add Device.DHCPv4.Relay.Forwarding. 2>&1)
if [[ $res != *"Device.DHCPv4.Relay.Forwarding."* ]]; then
	echo "Error: Add Method with correct path doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

del_path=$(echo "${res}" | cut -d' ' -f 2)

echo "ADD METHOD: Wrong Path" >> ./funl-test-debug.log
res=$(./icwmpd -c add Device.DeviceInfo.VendorLogFil 2>&1)
if [[ $res != *"9005"* ]]; then
	echo "Error: Add Method with wrong path doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "DELETE METHOD: Wrong Path" >> ./funl-test-debug.log
res=$(./icwmpd -c del Device.DeviceInfo.VendorLogFil 2>&1)
if [[ $res != *"9005"* ]]; then
	echo "Error: Delete Method with wrong path doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "DELETE METHOD: Correct Path ${del_path} && one instance" >> ./funl-test-debug.log
res=$(./icwmpd -c del "${del_path}" 2>&1)
if [[ $res != *"Deleted ${del_path}"* ]]; then
	echo "Error: Delete Method with correct path && one instance doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "DELETE METHOD: Correct Path && all instance" >> ./funl-test-debug.log
res=$(./icwmpd -c del Device.DHCPv4.Relay.Forwarding. 2>&1)
if [[ $res != *"Deleted Device.DHCPv4.Relay.Forwarding."* ]]; then
	echo "Error: Delete Method with correct path && all instances doesn't work correctly" >> ./funl-test-debug.log
	exit 1
fi

echo "PASS: $TEST_NAME"
