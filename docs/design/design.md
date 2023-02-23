# Design of icwmp tr-069 client


## General Description
icwmp is client side implementation of CWMP protocol. Its source code is completely conform with TR-069 standard. So it supports all required features described in the TR-069 standard.
So TR-069 features like cwmp session, events management, soap management, RPC methods management ... are supportd in the icwmp implementation.

As descibed in TR-069 standard, CWMP stack comprises several components that are unique to this protocol, and makes use of several standard protocols. Those components are followings:

<table>
<tr><td>

|<div>CWMP Stack </div> and icwmp corresponding source files| 
|--|
|__Application__			  cwmp.c session.c|
|__RPC Methods__			  rpc.c||
|__SOAP__					  xml.c|
|__HTTP__					  http.c digauth.c|
|__SSL/TLS__				  ssl_utils.c|

</td><td>

|Common Source files|
|--|
|<div>backup_session.c </div><div> cwmp_uci.c </div><div> ubus_utils.c </div><div> subprocess.c </div><div> common.c </div><div> config.c </div><div> datamodel_interface.c </div><div> diagnostic.c </div><div> download.c </div><div> upload.c </div> notifications.c|

</td></tr> </table>

- Application: the application uses CWMP protocol on the CPE. In the icwmp client, the main application is defined in cwmp.c source file. It's based on uloop libubox functionality. Multiple timers are running under the main uloop of icwmp like session timer, ubus timer, heartbeat timer, ...
- RPC methods: The specific RPC methods that are defined by CWMP protocol. In icwmp client RPC methods are defined under the source file rpc.c.
- SOAP: A standard XML-based syntax used here to encode remote procedure calls. The SOAP part is developed in xml.c file.
- HTTP: this part is responsible to send SOAP messages over HTTP. In icwmp it's based on libcurl library. Its corresponding C functions are developed in the file http.c.
- SSL/TLS: The standard Internet transport layer security protocol. icwmp can work with openssl or mbedtls or wolfssl depending on the SSL library selection. Its corresponding functions are defined in ssl_utils.c file.

## Main structure of icwmp app
 
 The main structure in icwmp is the cwmp structure. Just one instance of cwmp structure is needed for the executing icwmp app. 
 
 The following schema presents this sctructure with the most important attributes:
 
 ```mermaid
classDiagram
class cwmp {
	conf: struct config
	deviceid: struct deviceid
	session: struct session
	heart_session: bool
	diag_session: bool
	event_id: int
	cr_socket_desc: int
	pid_file: FILE
}
```
 - conf: its type is the config structure. config structure is responsible to define the UCI configuration of the application and it's loaded in the start of icwmp. Multiple UCI options are defined in this structure such as configurations related to the CPE like CR credentials, CR port, provisioning code ... and configurations related to the ACS connection like ACS access username/password, periodic_inform_enable, periodic_inform_interval ...
 - heart_session: is a boolean attribute. This attribute is used to check if the running session is a Heartbeat session.
 - diag_session: is a boolean attribute. This  attribute is used to check if the running session contains the event '8 DIAGNOSTICS COMPLETE'
 - event_id: is an integer attribute, and is used by the app to store the number of events in the running session.
 - cr_socket_des: is an integer attribute. This attribute contains the id of the socket used by the CR.
 - pide_file: is a File type attribute. This file used by the app in order to garantuate a single app instance running.
 - deviceid: its type is deviceid structure. This struct contains the device information important attributes values: OUI, Manufacturer, SerialNumber, ProductClass, SoftwareVersion.
 - session: its type is session structure. This structure contains attributes related to the running session. 
 
```mermaid
classDiagram
class session {
	head_rpc_cpe: list
	head_rpc_acs: list
	events: list
	session_status: struct session_status
	tree_in: mxml_node_t
	tree_out: mxml_node_t
	body_in: mxml_node_t
	fault_code: int
}
```

 session attributes are:
 
 - head_rpc_cpe: its type is struct list_head. It contains RPC methods list that needs to be executed by the CPE in the actual session.
 - head_rpc_acs: its type is struct list_head. It contains RPC methods list that the CPE requests to be executed in the ACS side in the actual session.
 - events: its type is struct list_head. It contains list of events that the CPE notifies the ACS in the Inform message of the actual session.
 - session_status: its type is struct session_status. it contains informations about the running session like start time, end time, if it's successful or failed session, hearbeat session validation ...
 
## icwmp application
 
 The following diagram shows how icwmp application manage CWMP sessions with uloop:
 
```mermaid
flowchart TD
	A[icwmp init] --> B[uloop init]
	A --> C[CR http server init]
	B --> D{wait uloop timeout}
	D -- ubus timeout --> E[receive tr069 method call]
	D -- periodic timeout --> F[Add '2 PERIODIC' event to the list of events]
	F --> G[Execute CWMP session]
	G --> H[trigger check value change timeout]
	H --> D
	E --> I{the method is Inform}
	I -- yes --> G
	I -- no --> J[Execute the ubus method]
	J --> D
	C --> K[Start CR server thread]
	K --> L[listen to coming HTTP requests in the CR port]
	L --> M{CR is received}
	M -- yes --> N[execute the ubus method: tr069 inform]
	N --> E
	M -- no --> L
```


 As described in the diagram, after initiating the icwmp application (config init, backup init, ...), the application trigger:
 
 1 - uloop initiation by initiating the following timeouts:
 
 - ubus methods timeout
 - autonomous policy timeouts
 - session timeout
 - heartbeat timeout
 - periodic session timeout
 
then the application start the uloop run by waiting uloop timeouts to come.

2 - In the same time a thread is created. It permits a permenant listening of the port , in the purpose to receive connection requests.
In case a CR is received successfully "tr069 inform" ubus call is triggered from this thread in the purpose to trigger a new session with '6 CONNECTION REQUEST' event.


## icwmp tr-069 session

[CWMP session in icwmp client](./session.md)

## Inform ACS method

The Inform ACS request is sent by the CPE to the ACS in the start of the CWMP session, in order to inform him about some informations like the device id, events, software version, the CR url ...

In icwmp the corresponding function responsible to create the Inform message is cwmp_rpc_acs_prepare_message_inform. It starts by getting the device ID attributes and the list of events that needs to be included in the inform message and then parameters that are amended in the ParameterList including default Inform Parameter and other parameters like parameters related to the Value Change.

## Events manipulation

As described in TR069 standard, events in CWMP protocol must be sent by the CPE in an Inform request in order to notify the ACS when something of interest has happened. 

CWMP events are stored in the **events** list_head attribute of the session structure attribute. Each element of this list is an instance of the structure event_container that is the following:

```mermaid
classDiagram
class event_container {
	code: integer
	next_session: boolean
	command_key: string
	head_dm_parameter: list
	id: integer
}
```

In icwmp when creating the inform message events are loaded from **events** list.

As soon as the event is reached, it's added to the list events. Different scenarios are presents to reach event in icwmp:

- Start app events: this scenario includes following events: "0 BOOTSTRAP" (if it's the first Inform to be sent to the specified ACS), "1 BOOT".
- Get the event from the backupSession in the start of the App: Just after starting the icwmp app in the init step, the app loads the backupsession events in events list from "cwmp_event" xml tag. Then add the to the events list. This case includes events that occurs before the restart of the icwmp or reboot/upgrade of the system. In this scenario events included are: "M Reboot", "M Download", "M ScheduleDownload", "7 TRANSFER COMPLETE".
- Events when running CWMP session. Events for this scenario are detected and added to the events while executing the CWMP session especially in RPC calls. Such as "3 SHEDULED" for ScheduleInform method, "7 TRANSFER COMPLETE" (in the call of Download, Upload), "8 DIAGNOSTIC COMPLETE" (after completing diagnostic execution triggered by the ACS), "11 DU STATE CHANGE COMPLETE" after completing CDU request execution triggered by the ACS), "M Download", "M Upload" ...
- Events are detected at specific time using uloop_timeout_set:"2 PERIODIC", "14 HEARTBEAT"
- External Events: "6 CONNECTION REQUEST" when receiving a connection request from the ACS in order to start a new session. CR thread is permenantly listening on the CR port in order to detect such event and then trigger a new session. "4 VALUE CHANGE" (check notifications part), "10 AUTONOMOUS Single TRANSFER COMPLETE", "12 AUTONOMOUS  DU STATE CHANGE COMPLETE"

## Notifications

[Notifications in icwmp client](./notifications.md)

## XML in icwmp

[XML mechanism in icwmp client](./xml.md)

## Backup Session management

Backup session feature in icwmp is used to store data that should be conserved even after reboot or sysupgrad of the system. Those data are followings:

- events
- download
- transfer complete
- schedule download
- schedule inform
- upload
- change du state
- autonomous change du state
- du state change complete

Those data are stored in xml format in the file  /etc/icwmpd/icwmpd_backup_session.xml. This data is loaded by the call of the function cwmp_load_saved_session in the start of the icwmp application.

Backup session feature uses XML functions (check XML part) to build/load XML nodes that should be stored in the backup session file. So for each kind of backup session datas there are rows in **xml_nodes_data** that their index are definde in the enumaration **xml_node_references**.
All backup session indexes are successive in the enumaration and starts with "BKP_".

For each kind of backup session data, there are insert functions that calls build_xml_node_data and load functions that calls load_xml_node_data.

## UCI and config mangement

**TODO**



