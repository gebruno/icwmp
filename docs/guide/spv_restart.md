# Handling of SPV and restart services

In case `icwmpd` receives SetParameterValues Request from the ACS, it will use the `setm_values` ubus method of usp daemon(usp.raw ubus object), for all requested parameters.

Based on the response of setm_values the icwmp will do the following:

- in case of fault icwmp aborts the set of all parameters and then sends Response to the ACS with FAULT 9003 including all parameters faults as defined in TR069 standard.
- in case of success icwmp commits the set of all parameters, without applying them so without restarting services, and then sends a success Response to the ACS including the status code 1.
- All required services(whose configuration has been modified by SPV) are restarted at the end of CWMP session in order to prevent any session interruption.

!!! note
    
    icwmp always returns 1 as status value in case of success of SPV because all services are restarted at the end of the session.


