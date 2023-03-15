# Concepts and Workflow

In OpenWRT integration, `icwmpd` depends on `procd` based init script `/etc/init.d/icwmpd` to start it in boot-up. Once started, it reads the initial configuration from UCI and if configured connects to the ACS.

Provisioning of the ACS URL can be done in `icwmpd` with a firmware default uci value, or it can be done dynamically using DHCP Option 43 on the configured default_wan_interface.

ACS Session workflow could be checked with sniffer packets tool such as Wireshark or `tcpdump`.
In addition to that, `icwmpd` give provision to configure a log file in uci.
A snapshot of log description is listed below for demonstration(Content of the log can vary based on configuration):

```bash
24-12-2019, 10:21:18 [INFO]    STARTING ICWMP with PID :7762
24-12-2019, 10:21:18 [INFO]    Periodic event is enabled. Interval period = 180000s
24-12-2019, 10:21:18 [INFO]    Periodic time is Unknown
24-12-2019, 10:21:18 [INFO]    Connection Request server initiated with the port: 7547
24-12-2019, 10:21:18 [INFO]    Start session
24-12-2019, 10:21:18 [INFO]    ACS url: http://genieacs:7547
24-12-2019, 10:21:18 [INFO]    Preparing the Inform RPC message to send to the ACS
24-12-2019, 10:21:18 [INFO]    Send the Inform RPC message to the ACS
24-12-2019, 10:21:19 [INFO]    Get the InformResponse message from the ACS
24-12-2019, 10:21:19 [INFO]    Send empty message to the ACS
24-12-2019, 10:21:19 [INFO]    Receive HTTP 204 No Content
24-12-2019, 10:21:19 [INFO]    End session
24-12-2019, 10:21:19 [INFO]    Waiting the next session
```

Further, it provides different log level that can be configured in uci config `cwmp.cpe.log_severity` to get more verbose log to no logs.
