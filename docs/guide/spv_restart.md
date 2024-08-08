# Handling of SPV and restart services

In case `icwmpd` receives SetParameterValues Request from the ACS, it will use the `set` ubus method of bbfdmd daemon(bbf ubus object), for all requested parameters.

Based on the response of set the icwmp will do the following:

- in case of fault icwmp aborts the set of all parameters and then sends Response to the ACS with FAULT 9003 including all parameters faults as defined in TR069 standard.
- In case of success icwmp commits the set of all parameters and immediately apply the changes by restarting only the services which are not present in critical services list and the services which belongs to critical services list are restarted at the end of CWMP session to apply the new changes.
- If all services whose configuration has been modified by SPV are non critical services then after restarting the services immediately icwmp sends a success response to the ACS including the status code 0 but if any of the modified service is a critical service then SPV response includes status code 1.

# How to define the services as critical service

From `icwmpd` point of view, a service considered as critical service, if reload of that particular service can cause wan/upstream reload which might cause premature termination of on-going communication with ACS, to prevent this `icwmp` apply these changes at the session end.

To notify the ACS about delay service restart/apply, it respond with `status=1` in `SetParameterValueResponse` if the current changes include any such service.

The default list of critical services defined in a JSON file [/etc/icwmpd/critical_services.json](https://dev.iopsys.eu/feed/iopsys/-/blob/devel/icwmp/files/etc/critical_services.json?ref_type=heads).

For user deployment scenarios, if user has more of these critical services then it can be updated in the `critical_services.json` file.
An example `critical_services.json` definition:

```json
{
	"services_list": [
			"firewall",
			"network",
			"dhcp",
			"stunc",
			"xmpp",
			"wireless",
			"time"
	]
}
```
