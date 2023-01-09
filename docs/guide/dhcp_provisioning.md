# Provisioning of CWMP Agent through DHCP Server
If CPE receives DHCP option 43 in DHCP offer from upstream network it exposes the same in its UBUS method (e.g. ifstatus wan).

```bash
root@iopsys-44d43771b000:~# ifstatus wan
{
	...
	...
	"data": {
		"vendorspecinf": "XXXXXXXXXXXXXXXXXXX"
	}
}
```
CWMP Agent uses DHCP option 43 for provisioning if CWMP and DHCP discovery is enabled in the device. ICWMPD package adds a [DHCP client hook script](https://dev.iopsys.eu/iopsys/icwmp/-/blob/devel/files/etc/udhcpc.user.d/udhcpc_icwmp_opt43.user), which parses DHCP option 43 value to extract informations like ACS URL, minimum interval for session retry, retry interval multiplier for session retry and provisioning code that specifies the primary service provider and other provisioning information, which may be used by the ACS to determine service provider-specific customization and provisioning parameters.
This script then writes the provisioning information in `/etc/config/cwmp`.

CWMP Agent reads the provisioning information from `/etc/config/cwmp` before starting a session.

> Note: If in any time ACS configures `Device.ManagementServer.URL` then DHCP discovery is disabled automatically.
