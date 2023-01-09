# Notifications and Inform event

In this article we will discuss about how `icwmpd` manages inform messages and notifications towards ACS server.

## icwmpd forced inform parameters
As per the cwmp inform requirements, cwmp client has list of parameters defined internally. The list contains below parameters:

| Parameter name                                 |
| ---------------------------------------------- |
| Device.RootDataModelVersion                    |
| Device.DeviceInfo.HardwareVersion              |
| Device.DeviceInfo.SoftwareVersion              |
| Device.DeviceInfo.ProvisioningCode             |
| Device.ManagementServer.ParameterKey           |
| Device.ManagementServer.ConnectionRequestURL   |
| Device.ManagementServer.AliasBasedAddressing   |


In addition to the above defined forced inform parameters as specified in datamodel standard, TR-181 datamodel defines the multi instance object Device.ManagementServer.InformParameter.{i}. 
So new inform parameter can be added through the ACS by the call of the RPC method AddObject for the object Device.ManagementServer.InformParameter.{i}. and then set its parameters values.
icwmpd defines those new inform parameters in uci sections under the package /var/state/cwmp as below:

```bash
root@iopsys-44d43771aff0:~# cat /var/state/cwmp 

config inform_parameter
	option enable '1'
	option parameter_name "Device.DeviceInfo.UpTime"
	option events_list '1 BOOT,6 CONNECTION REQUEST'

config inform_parameter
	option enable '0'
```

## Notification management
`icwmpd` support below notification types, which can be configured from an ACS on the datamodel parameters
 - 0 = Notification off
 - 1 = Passive notification
 - 2 = Active notification
 - 3 = Passive lightweight notification
 - 4 = Passive notification with passive lightweight notification
 - 5 = Active lightweight notification
 - 6 = Passive notification with active lightweight notification

Along with this it does provide some debug utilities to get the notification from the device root shell as well
```bash
root@iopsys:~# icwmpd -c get_notif Device.Users.User.1.Username
Device.Users.User.1.Username => passive
```

To fulfill the requirement of forced active notification parameters, `icwmpd` internally maintains a list of forced active parameters specified in TR181. The list contains below parameters:

| Parameter name                        | Notification  |
| ------------------------------------- |---------------|
| Device.DeviceInfo.SoftwareVersion     |	2	|
| Device.DeviceInfo.ProvisioningCode    |	2	|

So, Creation of any other type of notification on the above parameters results in a cwmp fault 9009.


Along with this `icwmpd` support configuration of notification parameters using a JSON file. Users can include this json file in there firmware to override the existing notification parameters, or add new notifications from the firmware itself.


Below is the schema/format of the JSON file:

```bash
root@iopsys:~# cat /etc/icwmpd/inform.json
{
  "custom_notification": [
    {
      "parameter": "Device.Users.",
      "notify_type": "2"
    },
    {
      "parameter": "Device.WiFi.SSID.1.SSID",
      "notify_type": "1"
    }
  ]
}
```

> Note: In the Above example, parameter has to be defined with a valid datamodel parameter name and notify_type needs to be the notification type (number as present in the above table). Both the parameters are required.


After defining the JSON file with all the required parameters, this information required to update cwmp uci as below:
```bash
root@iopsys:~# uci set cwmp.cpe.custom_notify_json=/etc/icwmpd/inform.json
root@iopsys:~# uci commit cwmp
root@iopsys:~# /etc/init.d/icwmpd restart
```

>- ACS can manage the attributes of parameter added by custom_notification as it does for the other parameters
>- After firmware upgrade, for the 1st bootup, the custom_notify_json has higher priority, latter on ACS configured attributes get priority.
>- Addition of custom notification parameters is one time activity after upgrade, once done It can only be managed through ACS.
>- Parameters with wildcard not supported currently. So parameter like Device.WiFi.SSID.*. will be skipped

