# CWMP Agent

`icwmpd` is a client implementation of [TR-069/CWMP](https://cwmp-data-models.broadband-forum.org/) protocol.

It is written in C programming language and depends on a number of libraries of OpenWrt for building and running.

## Good to Know

The `icwmpd` client is :

* Tested with several ACS such as **Axiros**, **AVSytem**, **GenieACS**, **OpenACS**, etc...
* Supports all required **TR069 RPCs**.
* Supports all DataModel of TR family such as **TR-181**, **TR-104**, **TR-143**, **TR-157**, etc...
* Supports all types of connection requests such as **HTTP**, **XMPP**, **STUN**.
* Supports integrated file transfer such as **HTTP**, **HTTPS**, **FTP**.

## Configuration File

The `icwmpd` UCI configuration is located in **'/etc/config/cwmp'**, and contains 3 sections: **'acs'**, **'cpe'** and **'lwn'**.

```bash
config acs 'acs'
	option userid 'iopsys'
	option dhcp_discovery 'enable'
	option compression 'Disabled'
	option retry_min_wait_interval '5'
	option retry_interval_multiplier '2000'

config cpe 'cpe'
	option default_wan_interface 'wan'
	option userid 'iopsys'
	option exec_download '0'
	
config lwn 'lwn'
	option enable '1'
	option hostname ''
	option port ''
```

> Note: `icwmpd` depends on usp.raw for all datamodel parameters, some `DeviceId` related parameters can be overwritten by writing them directly on `/etc/config/cwmp` file.

```bash
uci set cwmp.cpe.manufacturer="ABC"
uci set cwmp.cpe.manufacturer_oui="XXX"
uci set cwmp.cpe.product_class="TEST_CLASS"
uci set cwmp.cpe.serial_number="1234567890"
uci set cwmp.cpe.software_version="X.Y.Z"
uci set cwmp.cpe.model_name="MODELXXX"
uci set cwmp.cpe.description="This is a test device"
uci commit cwmp
```
> Complete UCI for `cwmp` configuration available in [link](./docs/api/uci/cwmp.md) or [raw schema](https://dev.iopsys.eu/iopsys/icwmp/-/blob/devel/schemas/uci/cwmp.json)

> icwmpd gets the datamodel from the DUT via ubus using uspd, and also it registers `tr069` ubus namespace to expose some debug and cwmp client rpc funtionalities, so it is required to start it after starting `ubusd` and `uspd`.

## Important topics
* [Concepts & Workflow](./docs/guide/workflow.md)
* [Provisioning via DHCP](./docs/guide/dhcp_provisioning.md)
* [HTTPS configuration guide](./docs/guide/https_config.md)
* [Handling of SPV and service restart](./docs/guide/spv_restart.md)
* [Inform and notification management](./docs/guide/notification.md)
* [Manageable devices and Gateway Information (TR069 - Annex F)](./docs/guide/annex_f.md)
* [Supported RPC Methods](./docs/guide/supported_rpc.md)
* [UBUS methods](./docs/guide/ubus_method.md)
* [Command Line Interface](./docs/guide/cli.md)

## Dependencies

To successfully build icwmp, the following libraries are needed:

| Dependency  | Link                                        | License        |
| ----------- | ------------------------------------------- | -------------- |
| libuci      | https://git.openwrt.org/project/uci.git     | LGPL 2.1       |
| libubox     | https://git.openwrt.org/project/libubox.git | BSD            |
| libubus     | https://git.openwrt.org/project/ubus.git    | LGPL 2.1       |
| libjson-c   | https://s3.amazonaws.com/json-c_releases    | MIT            |
| libwolfssl  | https://github.com/wolfSSL/wolfssl          | GPL-2.0        |
| libcurl     | https://dl.uxnr.de/mirror/curl              | MIT            |
| mxml        | https://github.com/michaelrsweet/mxml       | GPL-2.0       |

Runtime dependencies:

| Dependency  | Link                                        | License        |
| ----------- | ------------------------------------------- | -------------- |
| ubus        | https://git.openwrt.org/project/ubus.git    | LGPL 2.1       |
| bbf         | https://dev.iopsys.eu/iopsys/bbf.git        | LGPLv2.1       |
| uspd        | https://dev.iopsys.eu/iopsys/uspd.git       | GPL v2.0       |

