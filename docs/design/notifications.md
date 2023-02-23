# Notifications in icwmp client

Notifications in TR-069 standard are used by the CPE to notify the ACS about parameters value change.

Notifications in CWMP protocol are based on three things:

- SetParameterAttributes RPC method
- GetParameterAttributes RPC method
- '4 VALUE CHANGE' event

Parameters notifications in icwmp are stored in /etc/icwmpd/cwmp_notifications uci package the notifications section.
Seven possible uci list can be present under the notifications section:  disabled , passive, active, passive_lw, passive_passive_lw, active_lw, passive_active_lw

## SetParameterAttributes RPC method

cwmp_set_parameter_attributes is the C function that is executed in case the SetParameterAttributes is called by the ACS. Its activity diagram is the following:

 ```mermaid
flowchart TD
    A[Check valid parameter_name] --> B{fault?}
    B -- no --> C{parameter_name is forced notification}
    B -- yes  --> D[return 9005]
    D --> H[END]
    C -- no --> E[update_notifications_list => update_ret]
    C -- yes --> D1[return 9009]
    D1 --> H
    E --> F{update_ret == true}
    F -- yes --> G[Add parameter_name to suitable notifications uci list]
    F -- no --> H
```

Two input arguments are present for this function: the parameter_name and the notification value.

As a first step the SPA function check if the parameter path is valid, then it checks if it's a forced notifications parameter. After that it checks if the notifications lists need to be updated by the call of the function **update_notifications_list**, if yes the input parameter is added to the suitable notification uci list.

The following decribe the algorithm of the function **update_notifications_list**:

 ```mermaid
flowchart TD
    A[Iterate notifications list => param_iter notif_iter] --> B{param_name == param_iter <br> && <br> notification != notif_iter}
    B -- no --> C{param_iter is suboject of param_name}
    B -- yes --> D[Delete param_iter from the notifications list]
    C -- no --> E{End iterations?}
    C -- yes --> F[update_ret = false]
    F --> E
    E -- yes --> G[return  update_ret]
    E -- no --> A
```

Two input arguments are present: the parameter name and the notification. 

update_ret is the output value to indicate if notification value needs to be updated in the uci list. Its default value is true.

All parameters under all present notifcations list are iterated one by one. While iterating all sub-parameters of the input parameter that has notification different from the input are deleted from their list.

false value is affected to update_ret only if the parameter_name parameter is subobject of already existing object and with the same requested notification.

## GetParameterAttributes RPC method

cwmp_get_parameter_attributes is the C function  that is executed in case the GetParameterAttributes is called by the ACS. Its input argument is the requester parameter name: param_name. Its activity diagram is the following:


 ```mermaid
flowchart TD
	A[check valid parameter path of param_name] --> B{valid?}
	B -- yes --> C[get_parameter_family_notifications of param_name => ret_notif, children_notif]
	B -- no --> D[END]
	C --> E[GPV of param_name => list_params]
	E --> F[iterate of list_params => param_iter]
	F --> G{param_iter is forced notification <br> => force_notif}
	G -- no --> H{param_iter is among children notif <br> => child_notif}
	H -- yes --> I[Assign to param_iter child_notif as notification]
	G -- yes --> J[Assign to param_iter for_notif as notification]
	H -- no --> K[Assign to param_iter ret_notif as notification]
	I --> L{End iterations?}
	J --> L
	K --> L
	L -- no --> F
	L -- yes --> M[return the list of parameters with notifications]
```


As a first step the GPA function check if the parameter path is valid, after that it calls the function get_parameter_family_notifications that has the below algorithm. This function permits to get the list of all childs parameters notifications of parameter_name presents in uci notifications list. Its input argument is the requester parameter name: param_name.

 ```mermaid
flowchart TD
    A[iterate parameters_notifications list => param_iter notif_iter] --> B{param_name is subobject of param_iter <br> or param_name == param_iter}
    B -- no --> C{param_iter is subobject of param_name}
    B -- yes --> D[notif_ret = notif_iter]
    C -- yes --> E[Add param_iter,notif_ret to children_list]
    E --> F{End iterations?}
    C -- no --> F
    D --> F
    F -- yes --> G[return notif_ret and children_list]
    F -- no --> A
```

The function trigger an iteration of parameters under the requested parameter parameter_name, for each parameter it gets its notification and the parameter with its notification to the result list.

## VALUE CHANGE event

'4 VALUE CHANGE' event is a CWMP event that is used to notify the ACS that an important parameter value changed. Such events are sent in an Inform message to the ACS with the corresponding parameters and values. 

In icwmp parameter value change is detect by permenant check parameters (with enabled notifications) values according to their last values. Last parameters with enabled notification values are stored in JSON lines in the /etc/icwmpd/dm_enabled_notify file. 

The process to detect the parameters that has positive notifications values is done with the function **check_value_change**. Important parameters that are detected that their values changed are amended to the list **list_value_change**.

As soon as a parameter value si changed by the ACS or after check_value_change process is done, the /etc/icwmpd/dm_enabled_notify file needs to be updated according to last parameters values. This process is done with the function **cwmp_update_enabled_notify_file**.

Next sections describes those two features in notifications in icwmp:

- check_value_change
- cwmp_update_enabled_notify_file

## check value change function:

check_value_change is responsible to detect what parameters (with enabled notifications) values changed. This process algorithm is described in the following diagram

 ```mermaid
flowchart TD
	A[get all leaf parameters values list corresponding <br>to parameters with enabled notification <br> => list_notify_params] -->B
	B[parse dm_enabled_notify_file line by line]--param_json_obj-->C[get corresponding parameter/value from param_json_obj]
	C --> D[get actual parameter value from the list list_notify_params]
	D --> E[compare actual and last values]
	E --> F{Values are different and notification is enabled?}
	F -- yes --> G[add the parameter and its actual value to the list_value_change <br> and amend the notification value to notif_ret integer value]
	G --> H[next line in the dm_enabled_notify file]
	H --> I{End of file?}
	I -- yes --> J[return notif_ret value]
	I -- no => next line param_json_obj --> C
	F -- no --> H
```

The algorithm starts by get actual values of all leaf parameters (**list_notify_params**)that are under all parameters with enabled notifications whether fored notification parameters or parameters notifications set by the ACS with SetParameterAttributes.

After that the function starts parsing the /etc/icwmpd/dm_enabled_notify file, that contains old leaf parameters values, line by line. Each line is a JSON object similar to **{"parameter":"Device.WiFi.SSID.1.SSID","notification":2,"type":"xsd:string","value":"60"}**. This JSON object contains the parameter name, its notification and its recent value. In the next step, it gets the actual value of the parameter from the list **list_notify_params**. Then it compares two values : actual one and recent one. If it's changed so the notification value is amended (with OR operator) to the notif_ret integer (initialised with 0), and in addition the parameter and its actual value is amended to the lis_value_change list.

In the end of the algorithm notif_ret value is returned by the function.

Using the returned value, the decision will be token to add '4 VALUE CHANGE' to the list of events of the next session.

## cwmp_update_enabled_notify_file function:

Two cases are present to enable the file /etc/icwmpd/dm_enabled_notify content with the actual parameters values:

- After the Set value of parameters by the ACS
- After the call of check_value_change

The update of the file is done by removing the old one and creating the new one using actual parameters values.

Such process that's responsible to create dm_enabled_notify file with actual values is done by the call of the function **create_list_param_leaf_notify**.
 