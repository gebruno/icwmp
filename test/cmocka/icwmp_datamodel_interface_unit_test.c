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

#include "common.h"
#include "config.h"
#include "datamodel_interface.h"
#include "event.h"
#include "xml.h"
#include "rpc.h"
#include "session.h"
#include "log.h"
#include "uci_utils.h"

static LIST_HEAD(list_set_param_value);
static LIST_HEAD(faults_array);
static LIST_HEAD(parameters_list);

static int dm_iface_unit_tests_init(void **state)
{
	cwmp_main = (struct cwmp*)calloc(1, sizeof(struct cwmp));
	create_cwmp_session_structure();
	cwmp_session_init();
	get_global_config();
	return 0;
}

static int dm_iface_unit_tests_clean(void **state)
{
	icwmp_free_list_services();
	cwmp_free_all_list_param_fault(&faults_array);
	cwmp_free_all_dm_parameter_list(&list_set_param_value);
	cwmp_session_exit();
	clean_cwmp_session_structure();
	FREE(cwmp_main);
	return 0;
}

/*
 * datamodel interface tests
 */
static void dm_get_parameter_values_test(void **state)
{
	char *fault = NULL;
	/*
	 * Test of valid parameter path
	 */
	fault = cwmp_get_parameter_values("Device.DeviceInfo.UpTime", &parameters_list);
	assert_null(fault);
	struct cwmp_dm_parameter *param_value = NULL;
	list_for_each_entry (param_value, &parameters_list, list) {
		assert_non_null(param_value->name);
		assert_string_equal(param_value->name, "Device.DeviceInfo.UpTime");
		break;
	}
	cwmp_free_all_dm_parameter_list(&parameters_list);

	/*
	 * Test of non valid parameter path
	 */
	fault = cwmp_get_parameter_values("Device.Deviceno.UpTime", &parameters_list);
	assert_non_null(fault);
	assert_string_equal(fault, "9005");
	cwmp_free_all_dm_parameter_list(&parameters_list);

	/*
	 * Test of valid multi-instance_object_path
	 */
	fault = cwmp_get_parameter_values("Device.WiFi.SSID.", &parameters_list);
	assert_null(fault);
	cwmp_free_all_dm_parameter_list(&parameters_list);

	/*
	 * Test of valid not multi-instance_object_path
	 */
	fault = cwmp_get_parameter_values("Device.DeviceInfo.", &parameters_list);
	assert_null(fault);
	cwmp_free_all_dm_parameter_list(&parameters_list);

	/*
	 * Test of non valid object path
	 */
	fault = cwmp_get_parameter_values("Device.Deviceno.", &parameters_list);
	assert_non_null(fault);
	assert_string_equal(fault, "9005");
	cwmp_free_all_dm_parameter_list(&parameters_list);
}

static void dm_set_multiple_parameter_values_test(void **state)
{
	int fault = 0;
	LargestIntegralType faults_values[15] = { 9005, 9007, 9008 };
	int fault_code = 0;
	char *fault_name = NULL;
	struct cwmp_param_fault *param_fault = NULL;

	/*
	 * Test of one valid parameter
	 */
	add_dm_parameter_to_list(&list_set_param_value, "Device.WiFi.SSID.1.Alias", "wifi_alias_1", NULL, 0, false);
	cwmp_transaction("start", false);
	fault = cwmp_set_multi_parameters_value(&list_set_param_value, &faults_array);
	assert_int_equal(fault, 0);
	cwmp_transaction("commit", true);
	cwmp_free_all_dm_parameter_list(&list_set_param_value);

	add_dm_parameter_to_list(&list_set_param_value, "Device.ManagementServer.Username", "iopsys_user", NULL, 0, false);
	cwmp_transaction("start", false);
	fault = cwmp_set_multi_parameters_value(&list_set_param_value, &faults_array);
	assert_int_equal(fault, 0);
	cwmp_transaction("commit", true);
	cwmp_free_all_dm_parameter_list(&list_set_param_value);
	fault = 0;

	/*
	 * Test of non valid parameter path
	 */
	add_dm_parameter_to_list(&list_set_param_value, "Device.WiFi.SSID.1.Alis", "wifi_alias_1", NULL, 0, false);
	cwmp_transaction("start", false);
	fault = cwmp_set_multi_parameters_value(&list_set_param_value, &faults_array);
	assert_non_null(fault);
	list_for_each_entry (param_fault, &faults_array, list) {
		fault_code = param_fault->fault_code;
		fault_name = param_fault->path_name;
		break;
	}
	assert_int_not_equal(fault, 0);
	assert_int_equal(fault_code, 9005);
	assert_non_null(fault_name);
	cwmp_transaction("abort", false);
	cwmp_free_all_dm_parameter_list(&list_set_param_value);
	cwmp_free_all_list_param_fault(&faults_array);
	fault_code = 0;
	fault_name = NULL;
	param_fault = NULL;
	fault = 0;

	/*
	 * Test of non writable, valid parameter path
	 */
	add_dm_parameter_to_list(&list_set_param_value, "Device.DeviceInfo.UpTime", "1234", NULL, 0, false);
	cwmp_transaction("start", false);
	fault = cwmp_set_multi_parameters_value(&list_set_param_value,&faults_array);
	assert_int_not_equal(fault, 0);
	list_for_each_entry (param_fault, &faults_array, list) {
		fault_code = param_fault->fault_code;
		fault_name = param_fault->path_name;
		break;
	}
	assert_int_not_equal(fault, 0);
	assert_int_equal(fault_code, 9008);
	assert_non_null(fault_name);
	cwmp_transaction("abort", false);
	cwmp_free_all_dm_parameter_list(&list_set_param_value);
	cwmp_free_all_list_param_fault(&faults_array);
	fault = 0;
	fault_code = 0;
	fault_name = NULL;
	param_fault = NULL;

	/*
	 * Test of writable, valid parameter path wrong value
	 */
	add_dm_parameter_to_list(&list_set_param_value, "Device.WiFi.SSID.1.Enable", "tre", NULL, 0, false);
	cwmp_transaction("start", false);
	fault = cwmp_set_multi_parameters_value(&list_set_param_value, &faults_array);
	assert_non_null(fault);
	list_for_each_entry (param_fault, &faults_array, list) {
		fault_code = param_fault->fault_code;
		fault_name = param_fault->path_name;
		break;
	}
	assert_int_not_equal(fault, 0);
	assert_int_equal(fault_code, 9007);
	assert_non_null(fault_name);
	cwmp_transaction("abort", false);
	cwmp_free_all_dm_parameter_list(&list_set_param_value);
	cwmp_free_all_list_param_fault(&faults_array);
	fault_code = 0;
	fault_name = NULL;
	param_fault = NULL;
	fault = 0;

	/*
	 * Test of list of valid parameters
	 */
	add_dm_parameter_to_list(&list_set_param_value, "Device.WiFi.SSID.1.Alias", "wifi_alias1_1", NULL, 0, false);
	add_dm_parameter_to_list(&list_set_param_value, "Device.WiFi.SSID.1.SSID", "wifi_ssid_2", NULL, 0, false);
	add_dm_parameter_to_list(&list_set_param_value, "Device.ManagementServer.Username", "iopsys_user_1", NULL, 0, false);
	cwmp_transaction("start", false);
	fault = cwmp_set_multi_parameters_value(&list_set_param_value, &faults_array);
	assert_int_equal(fault, 0);
	cwmp_transaction("commit", true);
	cwmp_free_all_list_param_fault(&faults_array);
	cwmp_free_all_dm_parameter_list(&list_set_param_value);

	/*
	 * Test of list wrong parameters values
	 */
	add_dm_parameter_to_list(&list_set_param_value, "Device.WiFi.SSID.1.SSID", "wifi_ssid_2", NULL, 0, false);
	add_dm_parameter_to_list(&list_set_param_value, "Device.WiFi.SSID.1.Enable", "tre", NULL, 0, false);
	add_dm_parameter_to_list(&list_set_param_value, "Device.WiFi.SSID.2.Alis", "wifi_2", NULL, 0, false);
	add_dm_parameter_to_list(&list_set_param_value, "Device.ATM.Link.1.Status", "Up", NULL, 0, false);
	cwmp_transaction("start", false);
	fault = cwmp_set_multi_parameters_value(&list_set_param_value, &faults_array);
	assert_int_not_equal(fault, 0);
	list_for_each_entry (param_fault, &faults_array, list) {
		assert_in_set(param_fault->fault_code, faults_values, 3);
	}
	cwmp_transaction("commit", true);
	cwmp_free_all_dm_parameter_list(&list_set_param_value);
}

static void dm_add_object_test(void **state)
{
	struct object_result res = {0};
	bool status = false;

	/*
	 * Add valid path and writable object
	 */

	memset(&res, 0, sizeof(struct object_result));

	cwmp_transaction("start", false);
	status = cwmp_add_object("Device.WiFi.SSID.", &res);
	assert_non_null(res.instance);
	assert_true(status);
	cwmp_transaction("commit", false);
	FREE(res.instance);

	/*
	 * Add not valid path object
	 */

	memset(&res, 0, sizeof(struct object_result));

	cwmp_transaction("start", false);
	status = cwmp_add_object("Device.WiFi.SIDl.", &res);
	assert_false(status);
	assert_int_equal(res.fault_code, FAULT_9005);
	assert_null(res.instance);
	cwmp_transaction("commit", false);
	FREE(res.instance);

	/*
	 * Add valid path not writable object
	 */

	memset(&res, 0, sizeof(struct object_result));

	cwmp_transaction("start", false);
	status = cwmp_add_object("Device.DeviceInfo.Processor.", &res);
	assert_false(status);
	assert_int_equal(res.fault_code, FAULT_9005);
	assert_null(res.instance);
	cwmp_transaction("commit", false);
	FREE(res.instance);
}

static void dm_delete_object_test(void **state)
{
	struct object_result res = {0};
	bool status = false;

	/*
	 * Delete valid path and writable object
	 */

	memset(&res, 0, sizeof(struct object_result));

	cwmp_transaction("start", false);
	status = cwmp_delete_object("Device.WiFi.SSID.2.", &res);
	assert_true(status);
	cwmp_transaction("commit", true);

	/*
	 * Delete not valid path object
	 */

	memset(&res, 0, sizeof(struct object_result));

	cwmp_transaction("start", false);
	status = cwmp_delete_object("Device.WiFi.SIDl.3.", &res);
	assert_false(status);
	assert_int_equal(res.fault_code, FAULT_9005);
	cwmp_transaction("commit", true);

	/*
	 * Delte valid path not writable object
	 */

	memset(&res, 0, sizeof(struct object_result));

	cwmp_transaction("start", false);
	status = cwmp_delete_object("Device.DeviceInfo.Processor.2.", &res);
	assert_false(status);
	assert_int_equal(res.fault_code, FAULT_9005);
	cwmp_transaction("commit", true);
}

static void dm_get_parameter_names_test(void **state)
{
	char *fault = NULL;

	/*
	 * Valid multi-instance object path
	 */
	fault = cwmp_get_parameter_names("Device.WiFi.SSID.", true, &parameters_list);
	assert_null(fault);
	struct cwmp_dm_parameter *param_value = NULL;
	int nbre_objs = 0;
	list_for_each_entry (param_value, &parameters_list, list) {
		nbre_objs++;
	}
	assert_int_not_equal(nbre_objs, 0);
	cwmp_free_all_dm_parameter_list(&parameters_list);
	nbre_objs = 0;

	/*
	 * Valid not multi-instance object path
	 */
	fault = cwmp_get_parameter_names("Device.DeviceInfo.", true, &parameters_list);
	assert_null(fault);
	list_for_each_entry (param_value, &parameters_list, list) {
		nbre_objs++;
	}
	assert_int_not_equal(nbre_objs, 0);
	cwmp_free_all_dm_parameter_list(&parameters_list);
	nbre_objs = 0;

	/*
	 * Not valid object path
	 */
	fault = cwmp_get_parameter_names("Device.Devicenfo.", true, &parameters_list);
	assert_non_null(fault);
	assert_string_equal(fault, "9005");
}

int icwmp_datamodel_interface_test(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(dm_get_parameter_values_test), //
		cmocka_unit_test(dm_set_multiple_parameter_values_test),
		cmocka_unit_test(dm_add_object_test),
		cmocka_unit_test(dm_delete_object_test),
		cmocka_unit_test(dm_get_parameter_names_test),
	};

	return cmocka_run_group_tests(tests, dm_iface_unit_tests_init, dm_iface_unit_tests_clean);
}
