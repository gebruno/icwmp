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
    A[Iterate notifications list => param_iter notif_iter] --> B{param_name == param_iter && notification != notif_iter}
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
    F --> G{param_iter is forced notification => force_notif}
    G -- no --> H{param_iter is among children notif => child_notif}
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
    A[iterate parameters_notifications list => param_iter notif_iter] --> B{param_name is subobject of param_iter or param_name == param_iter}
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

**TODO**

## check_value_change

**TODO**

## update parameter notifications list

**TODO**