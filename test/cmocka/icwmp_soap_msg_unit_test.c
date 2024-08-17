/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2021 iopsys Software Solutions AB
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <dirent.h>
#include <string.h>
#include <mxml.h>

#include "rpc.h"
#include "event.h"
#include "session.h"
#include "config.h"
#include "backupSession.h"
#include "log.h"
#include "download.h"
#include "xml.h"
#include "uci_utils.h"

#include "icwmp_soap_msg_unit_test.h"
#include "cwmp_event.h"

static int instance = 0;
struct list_head force_inform_list;

#define INVALID_PARAM_KEY "ParameterKeyParameterKeyParameter"
#define INVALID_USER "useruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruser1"
#define INVALID_PASS "passwordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpassword1"
/*
 * End test clean
 */

static void clean_name_space()
{
	FREE(ns.soap_env);
	FREE(ns.soap_enc);
	FREE(ns.xsd);
	FREE(ns.xsi);
	FREE(ns.cwmp);
}

static void unit_test_remove_all_session_events()
{
	while (cwmp_main->session->events.next != &(cwmp_main->session->events)) {
		struct event_container *event_container;
		event_container = list_entry(cwmp_main->session->events.next, struct event_container, list);
		free(event_container->command_key);
		cwmp_free_all_dm_parameter_list(&(event_container->head_dm_parameter));
		list_del(&(event_container->list));
		free(event_container);
	}
}

static void unit_test_end_test_destruction()
{
	unit_test_remove_all_session_events();
}

/*
 * CMOCKA functions
 */
static int soap_unit_tests_init(void **state)
{
	CWMP_MEMSET(&force_inform_list, 0, sizeof(struct list_head));
	INIT_LIST_HEAD(&force_inform_list);
	load_default_forced_inform();
	load_forced_inform_json();

	cwmp_main = (struct cwmp*)calloc(1, sizeof(struct cwmp));
	create_cwmp_session_structure();
	memcpy(&(cwmp_main->env), &cwmp_main, sizeof(struct env));
	cwmp_session_init();
	log_set_severity_idx("DEBUG");
	return 0;
}

static int soap_unit_tests_clean(void **state)
{
	clean_name_space();
	cwmp_session_exit();
	clean_force_inform_list();
	FREE(cwmp_main->session);
	FREE(cwmp_main);
	return 0;
}

/*
 * UNIT Tests
 */
static void get_config_test(void **state)
{
	int error = get_preinit_config();
	assert_int_equal(error, CWMP_OK);
	error = get_global_config();
}

static void get_deviceid_test(void **state)
{
	int error = cwmp_get_deviceid();
	assert_int_equal(error, CWMP_OK);
}

static void add_event_test(void **state)
{
	struct event_container *event_container;
	event_container = cwmp_add_event_container(EVENT_IDX_1BOOT, "");
	assert_non_null(event_container);
	assert_string_equal(EVENT_CONST[event_container->code].CODE, "1 BOOT");
}

static void soap_inform_message_test(void **state)
{
	mxml_node_t *env = NULL, *n = NULL, *device_id = NULL, *cwmp_inform = NULL;
	struct rpc *rpc_acs;

	rpc_acs = list_entry(&(cwmp_main->session->head_rpc_acs), struct rpc, list);
	cwmp_rpc_acs_prepare_message_inform(rpc_acs);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	cwmp_inform = mxmlFindElement(env, env, "cwmp:Inform", NULL, NULL, MXML_DESCEND);
	assert_non_null(cwmp_inform);
	device_id = mxmlFindElement(cwmp_inform, cwmp_inform, "DeviceId", NULL, NULL, MXML_DESCEND);
	assert_non_null(device_id);
	n = mxmlFindElement(device_id, device_id, "Manufacturer", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(device_id, device_id, "OUI", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(device_id, device_id, "ProductClass", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(device_id, device_id, "SerialNumber", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(cwmp_inform, cwmp_inform, "Event", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "EventStruct", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "EventCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "1 BOOT");
	n = mxmlFindElement(cwmp_inform, cwmp_inform, "ParameterList", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);

	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	unit_test_end_test_destruction();
}

static void prepare_session_for_rpc_method_call()
{
	mxml_node_t *b;
	char c[128];

	snprintf(c, sizeof(c), "%s:%s", ns.soap_env, "Body");
	b = mxmlFindElement(cwmp_main->session->tree_in, cwmp_main->session->tree_in, c, NULL, NULL, MXML_DESCEND);
	cwmp_main->session->body_in = b;
	xml_prepare_msg_out();
}

static void prepare_gpv_soap_request(char *parameters[], int len)
{
	mxml_node_t *params = NULL, *n = NULL;
	int i;

	cwmp_main->session->tree_in = mxmlLoadString(NULL, CWMP_GETPARAMETERVALUES_REQ, MXML_OPAQUE_CALLBACK);
	xml_recreate_namespace(cwmp_main->session->tree_in);
	params = mxmlFindElement(cwmp_main->session->tree_in, cwmp_main->session->tree_in, "ParameterNames", NULL, NULL, MXML_DESCEND);
	for (i = 0; i < len; i++) {
		n = mxmlNewElement(params, "string");
		n = mxmlNewOpaque(n, parameters[i]);
	}
}

static void soap_get_param_value_message_test(void **state)
{
	mxml_node_t *env = NULL, *n = NULL, *name = NULL, *value = NULL;

	struct rpc *rpc_cpe = build_sessin_rcp_cpe(RPC_CPE_GET_PARAMETER_VALUES);

	/*
	 * Valid parameter path
	 */
	char *valid_parameters[1] = { "Device.ManagementServer.PeriodicInformEnable" };
	prepare_gpv_soap_request(valid_parameters, 1);

	prepare_session_for_rpc_method_call();

	int ret = cwmp_handle_rpc_cpe_get_parameter_values(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:GetParameterValuesResponse", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "ParameterList", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "ParameterValueStruct", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);

	name = mxmlFindElement(n, n, "Name", NULL, NULL, MXML_DESCEND);
	assert_non_null(name);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(name)), "Device.ManagementServer.PeriodicInformEnable");
	value = mxmlFindElement(n, n, "Value", NULL, NULL, MXML_DESCEND);
	assert_non_null(value);

	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	/*
	* Wrong parameter path
	*/
	mxml_node_t *fault_code = NULL, *fault_string = NULL, *detail = NULL, *detail_code = NULL, *detail_string = NULL;

	char *invalid_parameter[1] = { "Device.ManagementServereriodicInformEnable" };
	prepare_gpv_soap_request(invalid_parameter, 1);

	prepare_session_for_rpc_method_call();

	ret = cwmp_handle_rpc_cpe_get_parameter_values(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "soap_env:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	fault_code = mxmlFindElement(n, n, "faultcode", NULL, NULL, MXML_DESCEND);
	assert_non_null(fault_code);
	fault_string = mxmlFindElement(n, n, "faultstring", NULL, NULL, MXML_DESCEND);
	assert_non_null(fault_string);
	detail = mxmlFindElement(n, n, "detail", NULL, NULL, MXML_DESCEND);
	assert_non_null(detail);
	detail = mxmlFindElement(detail, detail, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(detail);
	detail_code = mxmlFindElement(detail, detail, "FaultCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(detail_code);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(detail_code)), "9005");
	detail_string = mxmlFindElement(detail, detail, "FaultString", NULL, NULL, MXML_DESCEND);
	assert_non_null(detail_string);

	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);
	cwmp_main->session->head_rpc_acs.next = NULL;

	unit_test_end_test_destruction();
	clean_name_space();
}

static void prepare_addobj_soap_request(char *object, char *parameter_key)
{
	mxml_node_t *add_node = NULL, *n = NULL;

	cwmp_main->session->tree_in = mxmlLoadString(NULL, CWMP_ADDOBJECT_REQ, MXML_OPAQUE_CALLBACK);
	xml_recreate_namespace(cwmp_main->session->tree_in);
	add_node = mxmlFindElement(cwmp_main->session->tree_in, cwmp_main->session->tree_in, "cwmp:AddObject", NULL, NULL, MXML_DESCEND);
	n = mxmlFindElement(add_node, add_node, "ObjectName", NULL, NULL, MXML_DESCEND);
	n = mxmlNewOpaque(n, object);
	n = mxmlFindElement(add_node, add_node, "ParameterKey", NULL, NULL, MXML_DESCEND);
	n = mxmlNewOpaque(n, parameter_key);
}

static void soap_add_object_message_test(void **state)
{
	mxml_node_t *env = NULL, *n = NULL, *add_resp = NULL;

	struct rpc *rpc_cpe = build_sessin_rcp_cpe(RPC_CPE_ADD_OBJECT);
	/*
	 * Valid path & writable object
	 */
	prepare_addobj_soap_request("Device.WiFi.SSID.", "add_object_test");
	prepare_session_for_rpc_method_call();

	int ret = cwmp_handle_rpc_cpe_add_object(rpc_cpe);
	assert_int_equal(ret, 0);
	icwmp_restart_services(RELOAD_IMMIDIATE, true, false);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	add_resp = mxmlFindElement(n, n, "cwmp:AddObjectResponse", NULL, NULL, MXML_DESCEND);
	assert_non_null(add_resp);
	n = mxmlFindElement(add_resp, add_resp, "InstanceNumber", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	instance = (mxmlGetFirstChild(n) && mxmlGetOpaque(mxmlGetFirstChild(n))) ? atoi(mxmlGetOpaque(mxmlGetFirstChild(n))) : 1;
	n = mxmlFindElement(add_resp, add_resp, "Status", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "0");
	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	/*
	 * Wrong object path
	 */
	mxml_node_t *cwmp_fault = NULL;

	prepare_addobj_soap_request("Device.WiFi.SSI.", "add_object_test");
	prepare_session_for_rpc_method_call();

	ret = cwmp_handle_rpc_cpe_add_object(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "soap_env:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "detail", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	cwmp_fault = mxmlFindElement(n, n, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(cwmp_fault);
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "9005");
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultString", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	/*
	 * Not writable & Valid object path
	 */
	cwmp_fault = NULL;
	prepare_addobj_soap_request("Device.DeviceInfo.Processor.", "add_object_test");
	prepare_session_for_rpc_method_call();

	ret = cwmp_handle_rpc_cpe_add_object(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "detail", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	cwmp_fault = mxmlFindElement(n, n, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(cwmp_fault);
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "9005");
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultString", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	/*
	 * Invalid parameterkey
	 */
	cwmp_fault = NULL;
	prepare_addobj_soap_request("Device.WiFi.SSID.", INVALID_PARAM_KEY);
	prepare_session_for_rpc_method_call();

	ret = cwmp_handle_rpc_cpe_add_object(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "detail", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	cwmp_fault = mxmlFindElement(n, n, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(cwmp_fault);
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "9003");
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultString", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	cwmp_main->session->head_rpc_acs.next = NULL;
	unit_test_end_test_destruction();
	clean_name_space();
}

static void prepare_delobj_soap_request(char *object, char *parameter_key)
{
	mxml_node_t *del_node = NULL, *n = NULL;

	cwmp_main->session->tree_in = mxmlLoadString(NULL, CWMP_DELOBJECT_REQ, MXML_OPAQUE_CALLBACK);
	xml_recreate_namespace(cwmp_main->session->tree_in);
	del_node = mxmlFindElement(cwmp_main->session->tree_in, cwmp_main->session->tree_in, "cwmp:DeleteObject", NULL, NULL, MXML_DESCEND);
	n = mxmlFindElement(del_node, del_node, "ObjectName", NULL, NULL, MXML_DESCEND);
	n = mxmlNewOpaque(n, object);
	n = mxmlFindElement(del_node, del_node, "ParameterKey", NULL, NULL, MXML_DESCEND);
	n = mxmlNewOpaque(n, parameter_key);
}

static void soap_delete_object_message_test(void **state)
{
	mxml_node_t *env = NULL, *n = NULL, *add_resp = NULL;

	struct rpc *rpc_cpe = build_sessin_rcp_cpe(RPC_CPE_DELETE_OBJECT);

	/*
	 * Valid path & writable object
	 */
	char del_obj[32];
	snprintf(del_obj, sizeof(del_obj), "Device.WiFi.SSID.%d.", instance);
	prepare_delobj_soap_request(del_obj, "del_object_test");
	prepare_session_for_rpc_method_call();

	int ret = cwmp_handle_rpc_cpe_delete_object(rpc_cpe);
	assert_int_equal(ret, 0);
	icwmp_restart_services(RELOAD_IMMIDIATE, true, false);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	add_resp = mxmlFindElement(n, n, "cwmp:DeleteObjectResponse", NULL, NULL, MXML_DESCEND);
	assert_non_null(add_resp);
	n = mxmlFindElement(add_resp, add_resp, "Status", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "1");
	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	/*
	 * Wrong object path
	 */
	mxml_node_t *cwmp_fault = NULL;

	prepare_delobj_soap_request("Device.WiFi.SID.1", "del_object_test");
	prepare_session_for_rpc_method_call();

	ret = cwmp_handle_rpc_cpe_delete_object(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "soap_env:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "detail", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	cwmp_fault = mxmlFindElement(n, n, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(cwmp_fault);
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "9005");
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultString", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	/*
	 * Not writable & Valid object path
	 */
	cwmp_fault = NULL;
	prepare_delobj_soap_request("Device.DeviceInfo.Processor.2.", "del_object_test");
	prepare_session_for_rpc_method_call();

	ret = cwmp_handle_rpc_cpe_delete_object(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "detail", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	cwmp_fault = mxmlFindElement(n, n, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(cwmp_fault);
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "9005");
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultString", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	/*
	 * Invalid parameterkey
	 */
	cwmp_fault = NULL;
	prepare_delobj_soap_request("Device.WiFi.SSID.1.", INVALID_PARAM_KEY);
	prepare_session_for_rpc_method_call();

	ret = cwmp_handle_rpc_cpe_delete_object(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "detail", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	cwmp_fault = mxmlFindElement(n, n, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(cwmp_fault);
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "9003");
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultString", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	cwmp_main->session->head_rpc_acs.next = NULL;
	unit_test_end_test_destruction();
	clean_name_space();
}

static void prepare_gpa_soap_request(char *parameter)
{
	mxml_node_t *n = NULL;

	cwmp_main->session->tree_in = mxmlLoadString(NULL, CWMP_GETATTRIBUTES_REQ, MXML_OPAQUE_CALLBACK);
	xml_recreate_namespace(cwmp_main->session->tree_in);
	n = mxmlFindElement(cwmp_main->session->tree_in, cwmp_main->session->tree_in, "cwmp:GetParameterAttributes", NULL, NULL, MXML_DESCEND);
	n = mxmlFindElement(n, n, "ParameterNames", NULL, NULL, MXML_DESCEND);
	n = mxmlNewElement(n, "string");
	n = mxmlNewOpaque(n, parameter);
}

static void soap_get_parameter_attributes_message_test(void **state)
{
	mxml_node_t *env = NULL, *n = NULL, *param_attr = NULL;

	struct rpc *rpc_cpe = build_sessin_rcp_cpe(RPC_CPE_GET_PARAMETER_ATTRIBUTES);

	/*
	 * Valid path
	 */
	prepare_gpa_soap_request("Device.DeviceInfo.UpTime");
	prepare_session_for_rpc_method_call();

	int ret = cwmp_handle_rpc_cpe_get_parameter_attributes(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:GetParameterAttributesResponse", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "ParameterList", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	param_attr = mxmlFindElement(n, n, "ParameterAttributeStruct", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(param_attr, param_attr, "Name", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "Device.DeviceInfo.UpTime");
	n = mxmlFindElement(param_attr, param_attr, "Notification", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(param_attr, param_attr, "AccessList", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);

	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	/*
	 * Not Valid path
	 */
	mxml_node_t *fault_code = NULL, *fault_string = NULL, *detail = NULL, *detail_code = NULL, *detail_string = NULL;

	char *invalid_parameter[1] = { "Device.DeviceInfo.pTime" };
	prepare_gpv_soap_request(invalid_parameter, 1);

	prepare_session_for_rpc_method_call();

	ret = cwmp_handle_rpc_cpe_get_parameter_values(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "soap_env:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	fault_code = mxmlFindElement(n, n, "faultcode", NULL, NULL, MXML_DESCEND);
	assert_non_null(fault_code);
	fault_string = mxmlFindElement(n, n, "faultstring", NULL, NULL, MXML_DESCEND);
	assert_non_null(fault_string);
	detail = mxmlFindElement(n, n, "detail", NULL, NULL, MXML_DESCEND);
	assert_non_null(detail);
	detail = mxmlFindElement(detail, detail, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(detail);
	detail_code = mxmlFindElement(detail, detail, "FaultCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(detail_code);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(detail_code)), "9005");
	detail_string = mxmlFindElement(detail, detail, "FaultString", NULL, NULL, MXML_DESCEND);
	assert_non_null(detail_string);
	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	cwmp_main->session->head_rpc_acs.next = NULL;
	unit_test_end_test_destruction();
	clean_name_space();
}

static void prepare_spa_soap_request(char *parameter, char *notification, char *notification_change)
{
	mxml_node_t *n = NULL, *set_attr = NULL;

	cwmp_main->session->tree_in = mxmlLoadString(NULL, CWMP_SETATTRIBUTES_REQ, MXML_OPAQUE_CALLBACK);
	xml_recreate_namespace(cwmp_main->session->tree_in);
	set_attr = mxmlFindElement(cwmp_main->session->tree_in, cwmp_main->session->tree_in, "SetParameterAttributesStruct", NULL, NULL, MXML_DESCEND);
	n = mxmlFindElement(set_attr, set_attr, "Name", NULL, NULL, MXML_DESCEND);
	n = mxmlNewOpaque(n, parameter);
	n = mxmlFindElement(set_attr, set_attr, "Notification", NULL, NULL, MXML_DESCEND);
	n = mxmlNewOpaque(n, notification);
	n = mxmlFindElement(set_attr, set_attr, "NotificationChange", NULL, NULL, MXML_DESCEND);
	n = mxmlNewOpaque(n, notification_change);
}

static void soap_set_parameter_attributes_message_test(void **state)
{
	mxml_node_t *env = NULL, *n = NULL;

	struct rpc *rpc_cpe = build_sessin_rcp_cpe(RPC_CPE_SET_PARAMETER_ATTRIBUTES);

	/*
	 * Valid path
	 */
	prepare_spa_soap_request("Device.DeviceInfo.UpTime", "1", "true");
	prepare_session_for_rpc_method_call();

	int ret = cwmp_handle_rpc_cpe_set_parameter_attributes(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:SetParameterAttributesResponse", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_null(mxmlGetFirstChild(n));
	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	/*
	 * Not Valid path
	 */
	mxml_node_t *cwmp_fault = NULL;
	prepare_spa_soap_request("Device.DeviceInfo.pTime", "1", "true");
	prepare_session_for_rpc_method_call();

	ret = cwmp_handle_rpc_cpe_set_parameter_attributes(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "soap_env:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "detail", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	cwmp_fault = mxmlFindElement(n, n, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(cwmp_fault);
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "9005");
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultString", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);

	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	/*
	 * Not Valid Notification value
	 */
	prepare_spa_soap_request("Device.DeviceInfo.UpTime", "8", "true");
	prepare_session_for_rpc_method_call();

	ret = cwmp_handle_rpc_cpe_set_parameter_attributes(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "soap_env:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "detail", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	cwmp_fault = mxmlFindElement(n, n, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(cwmp_fault);
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "9003");
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultString", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);

	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	/*
	 * Invalid Notification String
	 */

	prepare_spa_soap_request("Device.DeviceInfo.UpTime", "balabala", "true");
	prepare_session_for_rpc_method_call();

	ret = cwmp_handle_rpc_cpe_set_parameter_attributes(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "soap_env:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "detail", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	cwmp_fault = mxmlFindElement(n, n, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(cwmp_fault);
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "9003");
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultString", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);

	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	/*
	 * Invalid NotificationChange
	 */

	prepare_spa_soap_request("Device.DeviceInfo.UpTime", "2", "5");
	prepare_session_for_rpc_method_call();

	ret = cwmp_handle_rpc_cpe_set_parameter_attributes(rpc_cpe);
	assert_int_equal(ret, 0);

	env = mxmlFindElement(cwmp_main->session->tree_out, cwmp_main->session->tree_out, "soap_env:Envelope", NULL, NULL, MXML_DESCEND);
	assert_non_null(env);
	n = mxmlFindElement(env, env, "soap_env:Header", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "cwmp:ID", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(env, env, "soap_env:Body", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "soap_env:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	n = mxmlFindElement(n, n, "detail", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	cwmp_fault = mxmlFindElement(n, n, "cwmp:Fault", NULL, NULL, MXML_DESCEND);
	assert_non_null(cwmp_fault);
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultCode", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);
	assert_string_equal(mxmlGetOpaque(mxmlGetFirstChild(n)), "9003");
	n = mxmlFindElement(cwmp_fault, cwmp_fault, "FaultString", NULL, NULL, MXML_DESCEND);
	assert_non_null(n);

	MXML_DELETE(cwmp_main->session->tree_in);
	MXML_DELETE(cwmp_main->session->tree_out);

	cwmp_main->session->head_rpc_acs.next = NULL;
	unit_test_end_test_destruction();
	clean_name_space();
}

int icwmp_soap_msg_test(void)
{
	const struct CMUnitTest tests[] = { //
		    cmocka_unit_test(get_config_test),
		    cmocka_unit_test(get_deviceid_test),
		    cmocka_unit_test(add_event_test),
		    cmocka_unit_test(soap_inform_message_test),
		    cmocka_unit_test(soap_get_param_value_message_test),
		    cmocka_unit_test(soap_add_object_message_test),
		    cmocka_unit_test(soap_delete_object_message_test),
		    cmocka_unit_test(soap_get_parameter_attributes_message_test),
		    cmocka_unit_test(soap_set_parameter_attributes_message_test),
	};

	return cmocka_run_group_tests(tests, soap_unit_tests_init, soap_unit_tests_clean);
}
