# icwmpd command line

`icwmpd` command line options are described with `--help` option as below:

```bash
root@iopsys:~# icwmpd --help
Usage: icwmpd [OPTIONS]
 -b, --boot-event                                    (CWMP daemon) Start CWMP with BOOT event
 -g, --get-rpc-methods                               (CWMP daemon) Start CWMP with GetRPCMethods request to ACS
 -c, --cli                                           CWMP CLI
 -h, --help                                          Display this help text
 -v, --version                                       Display the version
```

## icwmpd CLI

The icwmpd CLI is a debug utility and can be invoked using -c (--cli) command line option.

Different options of this CLI are described with help command as below:

```bash
root@iopsys:~# icwmpd -c help
Valid commands:
        help                                    => show this help
        get [path-expr]                         => get parameter values
        get_names [path-expr] [next-level]      => get parameter names
        set [path-expr] [value]                 => set parameter value
        add [object]                            => add object
        del [object]                            => delete object
        get_notif [path-expr]                   => get parameter notifications
```
> Note: icwmpd CLI is a debug utility and hence it is advised to use for debug and development purpose only.
> icwmpd CLI utility is independent of icwmpd daemon.

icwmp CLI command success result is displayed in the terminal as following:

```bash
root@iopsys:~# icwmpd -c get Device.DeviceInfo.UpTime
Device.DeviceInfo.UpTime => 91472
root@iopsys:~# icwmpd -c set Device.WiFi.SSID.1.SSID wifi1_ssid
Set value is successfully done
Device.WiFi.SSID.1.SSID => wifi1_ssid
```
In the case of fault the result is displayed as following:

```bash
root@iopsys:~# icwmpd -c get Device.DeviceInfo.UpTme
Fault 9005: Invalid parameter name
root@iopsys:~# icwmpd -c set
Fault 9003: Invalid arguments
root@iopsys:~# icwmpd -c set Device.WiFi.SSID.1.SSID
Fault 9003: Invalid arguments
```

