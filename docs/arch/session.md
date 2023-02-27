# CWMP session in icwmp client

In icwmp client, the CWMP session is executed under a uloop timeout handler that calls the C function responsible on the CWMP session execution: start_cwmp_session that has the following activity diagram:

 
```mermaid
flowchart TD
	A[init cwmp session] --> B[check notification value change]
	B --> C{value change?}
    C -- no --> D[send Inform Request to the ACS]
    C -- yes --> E[Add '4 VALUE CHANGE' and corresponding parameters to the Inform message]
    E --> D
    D --> F[Receive Inform Response from the ACS]
    F --> G[Parse rest of other ACS methods list]
    G --> H[Prepare next ACS method request]
    H --> I[Send Acs method request]
    I --> J[Receive ACS method response]
    J --> K{End of ACS methods list?}
    K -- no --> H
    K -- yes --> L[Send empty HTTP message to the ACS]
    L --> M[Receive HTTP Request from the ACS]
    M --> N{Is request body empty?}
    N -- yes --> O[Execute end session function]
    O --> P[exit session]
    N -- no --> Q[Execute CPE method]
    Q --> R[Send CPE method response to the ACS]
    R --> M
```


The CWMP session starts by initiating. The session initialisation contains essentially:

- The uci initialisation
- RPC ACS methods list initialisation

After that it checks if values of active/passive parameters are changed. Then it parses the RPC ACS methods list and starts with Inform method by sending its corresponding SOAP message in a HTTP Request to the ACS and then receives its corresponding response. 
Then it completes following the other ACS methods in the list and make send/receive to the ACS. 

In the next step the session sends an empty message to the ACS, therefore it starts receiving the RPC CPE methods requests from the ACS until it receives an empty message from the ACS. After that, after completing the execution of CPE methods Requests it execute the end session tasks (run_session_end_func), that includes all tasks that are requested in the session and need to be complete execution in the end session, like reloading config, restart services, download and apply files ...
In the last step the session completes by exiting. The session exit contain essentially:

- uci exit
- memory clean
- rpc methods list clean

## RPC ACS methods

RPC ACS methods are called just after initiating the session. It starts by the call of Inform method then GetRPCMethods. after that it complete other methods in the head_rpc_acs list.

In icwmp RPC ACS methods are defined in the array rpc_acs_methods of the structure rpc_acs_method. 


```mermaid
classDiagram
class rpc_acs_method {
	name: string
	prepare_message: function
	parse_message: function
}
```

- name: is a string attribute. It contains the name the RPC.
- prepare_message: it's a function attribute. This function is responsible to create the SOAP message that will be sent as the RPC method call to the ACS.
- parse_response: it's a function attribute. This function is responsible to to parse the SOAP message coming from the ACS

RPC ACS method is executed in a process respecting the tr-069 protocol layer as described in the following communication diagram:


```mermaid
graph RL
	A[RPC] --> |2 related data| B[SOAP]
	A --> |1 execute related routine and get data| A
	B --> |3 prepare rpc soap message| B
	B --> |4 SOAP RPC call| C[HTTP]
	C --> |5 HTTP request| D[ACS]
	D --> |6 HTTP response| C
	C --> |7 SOAP RPC response message| B
```

The RPC ACS call starts by preparing needed data after executing some features in RPC layer. Features that are executed differs from ACS method to an other:

- **Inform**

    Data present in the Inform call message are gotten form runtime variables and lists like events and also by execting internal get value (ubus usp.raw call) of some parameters for DeviceId and ParameterList.

- **TransferComplete**

    Transfer complete data can be gotten from backup session (in case of reboot or sysupgrade) or internal variables

- **AutonomousTransferComplete and AutonomousDUStateChangeComplete**

    Corresponding data are gotten from when receiving the ubus event usp.event.

After getting needed data, the corresponding RPC ACS prepare_message prepare the SOAP message that will be after that integrating as a body of a HTTP Request message. Then using curl library the HTTP layer part sends the HTTP Request to the ACS..
In the next step after the CPE receive the HTTP response from the ACS, it extracts the SOAP message and then needed data from the SOAP message.

## RPC CPE methods

In icwmp RPC CPE methods are defined in the array rpc_cpe_methods of the structure rpc_cpe_method.


```mermaid
classDiagram
class rpc_cpe_method {
	name: string
	prepare_message: function
	parse_message: function
}
```

- name: is a string attribute. It contains the name the RPC.
- handler: the corresponding the handler function responsible to extract data from the SOAP message and then execute corresponding features

RPC CPE method is executed in a process respecting the tr-069 protocol layer as described in the following communication diagram:


```mermaid
graph LR
    A[ACS] --> |1 HTTP Request| B[HTTP]
    B --> |2 SOAP RPC call| C[SOAP]
    C --> |3 extract RPC call| C
    C --> |4 extracted data| D[RPC]
    D --> |5 execute corresponding RPC features| D
    D --> |6 Result data| C
    C --> |7 Build SOAP response message| C
    C --> |8 SOAP RPC response| B
    B --> |9 HTTP response| A
```
 
The scenario starts by receiving the HTTP Request from the ACS. The HTTP body should be a SOAP RPC call message. The RPC method and its arguments are extracted from the SOAP message in the SOAP layer. Basing on this extracted data the corresponding function is executed in the RPC layer.

After that the SOAP component received returned and build the SOAP RPC response and it's encapsulated in a HTTP message and sent by curl as HTTP response to the ACS.

