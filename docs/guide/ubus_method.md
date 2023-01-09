# UBUS methods

`icwmpd` provides some RPCs support over ubus and some debug utilities those can be accessed using `tr069` ubus object. So, it must be launched on startup after `ubusd`.

> Note: For more info on the `tr069` ubus schema see [link](../api/ubus/tr069.md) or [raw schema](https://dev.iopsys.eu/iopsys/icwmp/-/blob/devel/schemas/ubus/tr069.json)

### tr069 ubus examples
Please note, the output shown in below examples are just for demonstration purpose, the actual output shall vary as per the cwmp configuration and state.

```bash
root@iopsys:~# ubus -v list tr069
'tr069' @aadff65c
        "command":{"command":"String"}
        "status":{}
        "inform":{"GetRPCMethods":"Boolean","event":"String"}
root@iopsys:~#
```

Each object registered with the `'tr069'` namespace has a specific functionality.

- To get the status of cwmp client, use the `status` ubus method:

```bash
root@iopsys:~# ubus call tr069 status
{
        "cwmp": {
                "status": "up",
                "start_time": "2021-07-29T09:29:02+02:00",
                "acs_url": "http://genieacs:7547"
        },
        "last_session": {
                "status": "success",
                "start_time": "2021-07-29T09:29:59+02:00",
                "end_time": "2021-07-29T09:30:00+02:00"
        },
        "next_session": {
                "status": "waiting",
                "start_time": "2021-07-29T09:59:59+02:00",
                "end_time": "N/A"
        },
        "statistics": {
                "success_sessions": 2,
                "failure_sessions": 0,
                "total_sessions": 2
        }
}
root@iopsys:~#
```

- To trigger a new session to ACS with the event `'6 CONNECTION REQUEST'` or `'8 DIAGNOSTICS COMPLETE'`, etc.., use the `inform` ubus method with the appropriate `event` argument:

```bash
root@iopsys:~# ubus call tr069 inform '{"event":"6 CONNECTION REQUEST"}'
{
	"status": 1,
	"info": "Session started"
}
root@iopsys:~#
root@iopsys:~# ubus call tr069 inform '{"event":"8 DIAGNOSTICS COMPLETE"}'
{
	"status": 1,
	"info": "Session started"
}
root@iopsys:~#
root@iopsys:~# ubus call tr069 inform '{"GetRPCMethods":"1"}'
{
	"status": 1,
	"info": "Session started"
}
root@iopsys:~#
```

- To reload the icwmpd config, use the `command` ubus method with `reload` argument:

```bash
root@iopsys:~# ubus call tr069 command '{"command":"reload"}'
{
	"status": 1,
	"info": "icwmpd config reloaded"
}
root@iopsys:~#
```

