#!/bin/bash

source ./test/script/common.sh
source ./gitlab-ci/shared.sh

TEST_NAME="ICWMP COMMAND LINE"

log "Running: $TEST_NAME"

log "GET METHOD: Correct Path"
res=$(./icwmpd -c get Device.DeviceInfo.VendorConfigFile.1.Alias 2>&1)
if [[ $res != *"cpe-1"* ]]; then
	log "Error: Get Method with correct path doesn't work correctly"
	exit 1
fi

log "GET METHOD: Wrong Path"
res=$(./icwmpd -c get Device.DeviceInfo.VendorConfigFile.1.Alia 2>&1)
if [[ $res != *"9005"* ]]; then
	log "Error: Get Method with wrong path doesn't work correctly"
	exit 1
fi

log "SET METHOD: Correct Path && Value"
res=$(./icwmpd -c set Device.DeviceInfo.ProvisioningCode 1234 2>&1)
if [[ $res != *"=> 1234"* ]]; then
	log "Error: Set Method with correct path && value doesn't work correctly"
	exit 1
fi

log "SET METHOD: Wrong Path && Correct Value"
res=$(./icwmpd -c set Device.DeviceInfo.WrongPath 1234 2>&1)
if [[ $res != *"9005"* ]]; then
	log "Error: Set Method with wrong path && correct value doesn't work correctly"
	exit 1
fi

log "SET METHOD: Correct Path && Wrong Value"
res=$(./icwmpd -c set Device.IP.Interface.1.Enable test 2>&1)
if [[ $res != *"9007"* ]]; then
	log "Error: Set Method with correct path && wrong value doesn't work correctly"
	exit 1
fi

log "GET NAME METHOD: Correct Path && level"
res=$(./icwmpd -c get_names Device.DeviceInfo.VendorConfigFile.1.Alias 0 2>&1)
if [[ $res != *"=> writable"* ]]; then
	log "Error: Get Name Method with correct path && level doesn't work correctly"
	exit 1
fi

log "GET NAME METHOD: Correct Path && Wrong level"
res=$(./icwmpd -c get_names Device.DeviceInfo.VendorConfigFile.1.Alias 1 2>&1)
if [[ $res != *"9003"* ]]; then
	log "Error: Get Name Method with correct path && wrong level doesn't work correctly"
	exit 1
fi

log "GET NAME METHOD: Wrong Path && Correct level"
res=$(./icwmpd -c get_names Device.DeviceInfo.VendorConfigFile.1.Ali 0 2>&1)
if [[ $res != *"9005"* ]]; then
	log "Error: Get Name Method with wrong path && correct level doesn't work correctly"
	exit 1
fi

log "GET NOTIFICATION METHOD: Correct Path"
res=$(./icwmpd -c get_notif Device.DeviceInfo.SoftwareVersion 2>&1)
if [[ $res != *"=> active"* ]]; then
	log "Error: Get Notification Method with correct path doesn't work correctly"
	exit 1
fi

log "GET NOTIFICATION METHOD: Wrong Path"
res=$(./icwmpd -c get_notif Device.DeviceInfo.VendorConfigFile.1.Ali 2>&1)
if [[ $res != *"9005"* ]]; then
	log "Error: Get Notification Method with wrong path doesn't work correctly"
	exit 1
fi

log "SET NOTIFICATION METHOD: Correct Path"
res=$(./icwmpd -c set_notif Device.DeviceInfo.VendorConfigFile.1.Alias 2 2>&1)
if [[ $res != *"=> 2"* ]]; then
	log "Error: Set Notification Method with correct path doesn't work correctly"
	exit 1
fi

log "SET NOTIFICATION METHOD: Wrong Path"
res=$(./icwmpd -c set_notif Device.DeviceInfo.VendorConfigFile.1.Ali 1 2>&1)
if [[ $res != *"9005"* ]]; then
	log "Error: Set Notification Method with wrong path doesn't work correctly"
	exit 1
fi

log "SET NOTIFICATION METHOD: Correst Path forced active notification"
res=$(./icwmpd -c set_notif Device.DeviceInfo.SoftwareVersion 1 2>&1)
if [[ $res != *"9009"* ]]; then
	log "Error: Set Notification Method with correct path forced active notification doesn't work correctly"
	exit 1
fi

log "ADD METHOD: Correct Path"
res=$(./icwmpd -c add Device.X_IOWRT_EU_Dropbear. 2>&1)
if [[ $res != *"Device.X_IOWRT_EU_Dropbear."* ]]; then
	log "Error: Add Method with correct path doesn't work correctly"
	exit 1
fi

del_path=$(echo "${res}" | cut -d' ' -f 2)

log "ADD METHOD: Wrong Path"
res=$(./icwmpd -c add Device.DeviceInfo.VendorConfigFil 2>&1)
if [[ $res != *"9005"* ]]; then
	log "Error: Add Method with wrong path doesn't work correctly"
	exit 1
fi

log "DELETE METHOD: Wrong Path"
res=$(./icwmpd -c del Device.DeviceInfo.VendorConfigFil 2>&1)
if [[ $res != *"9005"* ]]; then
	log "Error: Delete Method with wrong path doesn't work correctly"
	exit 1
fi

log "DELETE METHOD: Correct Path ${del_path} && one instance"
res=$(./icwmpd -c del "${del_path}" 2>&1)
if [[ $res != *"Deleted ${del_path}"* ]]; then
	log "Error: Delete Method with correct path && one instance doesn't work correctly"
	exit 1
fi

log "DELETE METHOD: Correct Path && all instance"
res=$(./icwmpd -c del Device.X_IOWRT_EU_Dropbear. 2>&1)
if [[ $res != *"9005"* ]]; then
	log "Error: Delete Method with correct path && all instances doesn't work correctly"
	exit 1
fi

echo "PASS: $TEST_NAME"
