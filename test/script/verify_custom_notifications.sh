#!/bin/bash

source ./test/script/common.sh
source ./gitlab-ci/shared.sh

TEST_NAME="Custom Notifications"

echo "Running: $TEST_NAME"

echo "Install notification json files"
exec_cmd mkdir -p /etc/icwmpd
exec_cmd cp test/files/etc/icwmpd/custom_notification* /etc/icwmpd

#
# Test a valid custom notification json file
#
rm /var/log/icwmpd.log

exec_cmd uci set cwmp.cpe.custom_notify_json="/etc/icwmpd/custom_notification_valid.json"
uci commit cwmp

supervisorctl start icwmpd
sleep 5
check_cwmp_status

supervisorctl stop icwmpd

check_valgrind_xml

notif1=`uci -c /etc/icwmpd get cwmp_notifications.@notifications[0].active | grep "Device.X_IOWRT_EU_Dropbear."`
if [[ $notif1 != *"Device.X_IOWRT_EU_Dropbear."* ]]; then
	echo "FAIL: active notifications list doesn't contain Device.X_IOWRT_EU_Dropbear. parameter"
	exit 1
fi
notif2=`uci -c /etc/icwmpd get cwmp_notifications.@notifications[0].passive | grep "Device.WiFi.SSID.1.SSID"`
if [[ $notif2 != *"Device.WiFi.SSID.1.SSID"* ]]; then
	echo "FAIL: active notifications list doesn't contain Device.WiFi.SSID.1.SSID parameter"
	exit 1
fi

rm /etc/icwmpd/icwmpd_notify

echo "PASS test valid custom notification json file"

#
# Test custom notification invalid json file
#
rm /var/log/icwmpd.log
uci -c /etc/icwmpd delete cwmp_notifications.@notifications[0]
uci -c /etc/icwmpd commit cwmp_notifications

exec_cmd uci set cwmp.cpe.custom_notify_json="/etc/icwmpd/custom_notification_invalid_json.json"
uci commit cwmp

supervisorctl start icwmpd
sleep 2
check_cwmp_status

supervisorctl stop icwmpd

check_valgrind_xml

notif1=`uci -c /etc/icwmpd get cwmp_notifications.@notifications[0].active | grep "Device.X_IOWRT_EU_Dropbear."`
if [[ $notif1 == *"Device.X_IOWRT_EU_Dropbear."* ]]; then
	echo "FAIL: the json file is invalid, the active notifcation list shouldn't contain Device.X_IOWRT_EU_Dropbear. parameter"
	exit 1
fi
notif2=`uci -c /etc/icwmpd get cwmp_notifications.@notifications[0].passive | grep "Device.WiFi.SSID.1.SSID"`
if [[ $notif2 == *"Device.WiFi.SSID.1.SSID"* ]]; then
	echo "FAIL: the json file is invalid, the active notifcation list shouldn't contain Device.WiFi.SSID.1.SSID parameter"
	exit 1
fi

logfile=`cat /var/log/icwmpd.log`
if [[ $logfile != *"[WARNING] The file /etc/icwmpd/custom_notification_invalid_json.json is not a valid JSON file"* ]]; then
	echo "FAIL: Log file doesn't contain a WARNING that the file /etc/icwmpd/custom_notification_invalid_json.json is not valid."
	exit 1
fi 

echo "PASS test custom notification invalid json file"

rm /etc/icwmpd/icwmpd_notify

#
# Test custom notification json file containing forced active notification
#
rm /var/log/icwmpd.log
uci -c /etc/icwmpd delete cwmp_notifications.@notifications[0]
uci -c /etc/icwmpd commit cwmp_notifications

exec_cmd uci set cwmp.cpe.custom_notify_json="/etc/icwmpd/custom_notification_forced.json"
uci commit cwmp

supervisorctl start icwmpd
check_cwmp_status
sleep 2
supervisorctl stop icwmpd

check_valgrind_xml

notif1=`uci -c /etc/icwmpd get cwmp_notifications.@notifications[0].active | grep "Device.X_IOWRT_EU_Dropbear."`
if [[ $notif1 != *"Device.X_IOWRT_EU_Dropbear."* ]]; then
	echo "FAIL: active notifications list doesn't contain Device.X_IOWRT_EU_Dropbear. parameter"
	exit 1
fi
notif2=`uci -c /etc/icwmpd get cwmp_notifications.@notifications[0].passive | grep "Device.DeviceInfo.ProvisioningCode"`
if [[ $notif2 == *"Device.DeviceInfo.ProvisioningCode"* ]]; then
	echo "FAIL: passive notifications list contains Device.DeviceInfo.ProvisioningCode while it's a forced active notification paramter"
	exit 1
fi

logfile=`cat /var/log/icwmpd.log`
if [[ $logfile != *"[ERROR]   Invalid/forced parameter Device.DeviceInfo.ProvisioningCode, skipped 9009"* ]]; then
	echo "FAIL: Device.DeviceInfo.ProvisioningCode is forced notification parameter, can't be changed"
	exit 1
fi 

echo "PASS test custom notification json file containing forced active notification"

rm /etc/icwmpd/icwmpd_notify

#
# Test custom notification json file containing invalid parameter path
#
rm /var/log/icwmpd.log
uci -c /etc/icwmpd delete cwmp_notifications.@notifications[0]
uci -c /etc/icwmpd commit cwmp_notifications

exec_cmd uci set cwmp.cpe.custom_notify_json="/etc/icwmpd/custom_notification_invalid_parameter.json"
uci commit cwmp

supervisorctl start icwmpd
check_cwmp_status
sleep 2
supervisorctl stop icwmpd

check_valgrind_xml

notif1=`uci -c /etc/icwmpd get cwmp_notifications.@notifications[0].active | grep "Device.X_IOWRT_EU_Dropbear."`
if [[ $notif1 != *"Device.X_IOWRT_EU_Dropbear."* ]]; then
	echo "FAIL: active notifications list doesn't contain Device.X_IOWRT_EU_Dropbear. parameter"
	exit 1
fi
notif2=`uci -c /etc/icwmpd get cwmp_notifications.@notifications[0].passive | grep "Device.WiFi.SSID.1.SD"`
if [[ $notif2 == *"Device.WiFi.SSID.1.SD"* ]]; then
	echo "FAIL: passive notifications list contains Device.WiFi.SSID.1.SD while it's a wrong parameter path"
	exit 1
fi

logfile=`cat /var/log/icwmpd.log`
if [[ $logfile != *"[ERROR]   Invalid/forced parameter Device.WiFi.SSID.1.SD, skipped 9005"* ]]; then
	echo "FAIL: Log file should contain WARNING that Device.WiFi.SSID.1.SD is wrong parameter path."
	exit 1
fi 

echo "PASS test custom notification json file containing invalid parameter path"

rm /etc/icwmpd/icwmpd_notify

echo "PASS: $TEST_NAME"
