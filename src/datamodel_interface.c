/*
 * datamodel_interface.c - API to call BBF datamodel functions (set, get, add, delete, setattributes, getattributes, getnames, ...)
 *
 * Copyright (C) 2021-2022, IOPSYS Software Solutions AB.
 *
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 *
 * See LICENSE file for license related information.
 *
 */

#include <libubox/blobmsg_json.h>

#include "datamodel_interface.h"
#include "ubus_utils.h"
#include "log.h"

unsigned int transaction_id = 0;

struct object_result {
	char **instance;
	char fault[5];
	bool status;
};

struct list_params_result {
	struct list_head *parameters_list;
	int error;
};

struct setm_values_res {
	bool status;
	struct list_head *faults_list;
};

struct transaction_info {
	bool status;
	bool restart_services;
};

/*
 * Common functions
 */
static struct blob_attr *get_parameters_array(struct blob_attr *msg)
{
	struct blob_attr *tb[1] = {0};
	const struct blobmsg_policy p[1] = {
			{ "parameters", BLOBMSG_TYPE_ARRAY }
	};

	if (msg == NULL) {
		CWMP_LOG(ERROR, "Ubus transaction callback msg is empty");
		return NULL;
	}

	blobmsg_parse(p, 1, tb, blobmsg_data(msg), blobmsg_len(msg));

	return tb[0];
}

int get_fault_value(struct blob_attr *msg)
{
	struct blob_attr *tb[1] = {0};
	const struct blobmsg_policy p[1] = {
			{ "fault", BLOBMSG_TYPE_INT32 }
	};

	if (msg == NULL) {
		CWMP_LOG(ERROR, "Ubus transaction callback msg is empty");
		return FAULT_CPE_INTERNAL_ERROR;
	}

	blobmsg_parse(p, 1, tb, blobmsg_data(msg), blobmsg_len(msg));

	return tb[0] ? blobmsg_get_u32(tb[0]) : FAULT_CPE_NO_FAULT;
}

static void prepare_optional_table(struct blob_buf *b)
{
	void *table = blobmsg_open_table(b, "optional");
	bb_add_string(b, "proto", "cwmp");
	bb_add_string(b, "format", "raw");
	blobmsg_add_u32(b, "instance_mode", cwmp_main->conf.instance_mode);
	blobmsg_add_u32(b, "transaction_id", transaction_id);
	blobmsg_close_table(b, table);
}

/*
 * Transaction Functions
 */
static void ubus_transaction_callback(struct ubus_request *req, int type __attribute__((unused)), struct blob_attr *msg)
{
	struct blob_attr *tb[3] = {0};
	const struct blobmsg_policy p[3] = {
			{ "status", BLOBMSG_TYPE_BOOL },
			{ "transaction_id", BLOBMSG_TYPE_INT32 },
			{ "updated_services", BLOBMSG_TYPE_ARRAY }
	};

	if (msg == NULL || req == NULL) {
		CWMP_LOG(ERROR, "Ubus transaction callback failed");
		return;
	}

	struct transaction_info *trans_info = (struct transaction_info *)req->priv;

	blobmsg_parse(p, 3, tb, blobmsg_data(msg), blobmsg_len(msg));

	if (!tb[0]) {
		trans_info->status = false;
		return;
	}

	trans_info->status = blobmsg_get_u8(tb[0]);
	if (trans_info->status == false)
		return;

	if (tb[1]) {
		transaction_id = blobmsg_get_u32(tb[1]);
	}

	if (trans_info->restart_services == false)
		return;

	if (tb[2]) {
		struct blob_attr *updated_services = tb[2];
		struct blob_attr *service = NULL;
		size_t rem;

		blobmsg_for_each_attr(service, updated_services, rem) {
			char *service_name = blobmsg_get_string(service);

			if (CWMP_STRLEN(service_name) == 0 || strcmp(service_name, "cwmp") == 0)
				continue;

			CWMP_LOG(DEBUG, "Detected service: %s will be restarted in the end session", service_name);
			icwmp_add_service(service_name);
		}
	}
}

bool cwmp_transaction(const char *cmd, bool restart_services)
{
	struct blob_buf b = {0};
	struct transaction_info trans_info = {
			.status = false,
			.restart_services = restart_services
	};

	if (CWMP_STRLEN(cmd) == 0)
		return false;

	int start_cmp = strcmp(cmd, "start");
	int commit_cmp = strcmp(cmd, "commit");
	int abort_cmp = strcmp(cmd, "abort");

	if (start_cmp != 0 && commit_cmp != 0 && abort_cmp != 0)
		return false;

	if ((start_cmp == 0 && transaction_id != 0) ||
			((commit_cmp == 0 || abort_cmp == 0) && transaction_id == 0))
		return false;

	CWMP_LOG(INFO, "Transaction %s ...", cmd);

	memset(&b, 0, sizeof(struct blob_buf));

	blob_buf_init(&b, 0);
	bb_add_string(&b, "cmd", cmd);
	blobmsg_add_u8(&b, "restart_services", false);
	prepare_optional_table(&b);

	int e = icwmp_ubus_invoke(BBF_OBJECT_NAME, "transaction", b.head, ubus_transaction_callback, &trans_info);

	blob_buf_free(&b);

	if (commit_cmp == 0 || abort_cmp == 0)
		transaction_id = 0;

	if (e != 0) {
		CWMP_LOG(INFO, "Transaction %s failed: Ubus err code: %d", cmd, e);
		return false;
	}

	if (!trans_info.status) {
		CWMP_LOG(INFO, "Transaction %s failed: Status => false", cmd);
		return false;
	}

	return true;
}

/*
 * Get parameter value
 */
static void ubus_get_single_parameter_callback(struct ubus_request *req, int type __attribute__((unused)), struct blob_attr *msg)
{
	struct blob_attr *cur = NULL;
	int rem = 0;

	if (msg == NULL || req == NULL)
		return;

	struct cwmp_dm_parameter *result = (struct cwmp_dm_parameter *)req->priv;
	struct blob_attr *parameters = get_parameters_array(msg);

	if (parameters == NULL) {
		result->notification = get_fault_value(msg);
		return;
	}

	blobmsg_for_each_attr(cur, parameters, rem) {
		struct blob_attr *tb[3] = {0};
		const struct blobmsg_policy p[3] = {
				{ "parameter", BLOBMSG_TYPE_STRING },
				{ "value", BLOBMSG_TYPE_STRING },
				{ "type", BLOBMSG_TYPE_STRING }
		};

		blobmsg_parse(p, 3, tb, blobmsg_data(cur), blobmsg_len(cur));

		result->name = icwmp_strdup(tb[0] ? blobmsg_get_string(tb[0]) : "");
		result->value = icwmp_strdup(tb[1] ? blobmsg_get_string(tb[1]) : "");
		result->type = icwmp_strdup(tb[2] ? blobmsg_get_string(tb[2]) : "");

		break;
	}
}

bool cwmp_get_parameter_value(char *parameter_name, struct cwmp_dm_parameter *dm_parameter)
{
	struct blob_buf b = {0};
	int len = CWMP_STRLEN(parameter_name);

	if (len == 0 || parameter_name[len - 1] == '.')
		return false;

	memset(&b, 0, sizeof(struct blob_buf));
	blob_buf_init(&b, 0);

	bb_add_string(&b, "path", parameter_name);
	prepare_optional_table(&b);

	int e = icwmp_ubus_invoke(BBF_OBJECT_NAME, "get", b.head, ubus_get_single_parameter_callback, dm_parameter);

	blob_buf_free(&b);

	if (e < 0) {
		CWMP_LOG(INFO, "get ubus method failed: Ubus err code: %d", e);
		return false;
	}

	if (dm_parameter->notification) {
		CWMP_LOG(INFO, "Get parameter value of %s failed, fault_code: %d", parameter_name, dm_parameter->notification);
		return false;
	}

	return true;
}

/*
 * Get parameter Values/Names
 */
static void ubus_get_parameter_callback(struct ubus_request *req, int type __attribute__((unused)), struct blob_attr *msg)
{
	struct blob_attr *cur = NULL;
	int rem = 0;
	const struct blobmsg_policy p[4] = {
			{ "parameter", BLOBMSG_TYPE_STRING },
			{ "value", BLOBMSG_TYPE_STRING },
			{ "type", BLOBMSG_TYPE_STRING },
			{ "writable", BLOBMSG_TYPE_STRING }
	};

	if (msg == NULL || req == NULL)
		return;

	struct list_params_result *result = (struct list_params_result *)req->priv;
	struct blob_attr *parameters = get_parameters_array(msg);

	if (parameters == NULL) {
		result->error = get_fault_value(msg);
		return;
	}

	blobmsg_for_each_attr(cur, parameters, rem) {
		struct blob_attr *tb[4] = {0};

		blobmsg_parse(p, 4, tb, blobmsg_data(cur), blobmsg_len(cur));

		if (!tb[0]) continue;

		char *param_name = blobmsg_get_string(tb[0]);
		char *param_value = tb[1] ? blobmsg_get_string(tb[1]) : "";
		char *param_type = tb[2] ? blobmsg_get_string(tb[2]) : "";
		char *param_per = tb[3] ? blobmsg_get_string(tb[3]) : "0";;
		bool writable = strcmp(param_per, "1") == 0 ? true : false;

		add_dm_parameter_to_list(result->parameters_list, param_name, param_value, param_type, 0, writable);
	}
}

char *cwmp_get_parameter_values(char *parameter_name, struct list_head *parameters_list)
{
	struct blob_buf b = {0};
	struct list_params_result get_result = {
			.parameters_list = parameters_list,
			.error = FAULT_CPE_NO_FAULT
	};
	unsigned int len = CWMP_STRLEN(parameter_name);

	if (len > 2 && parameter_name[len - 1] == '.' && parameter_name[len - 2] == '*')
		return "9005";

	char *param = len ? parameter_name : DM_ROOT_OBJ;

	memset(&b, 0, sizeof(struct blob_buf));
	blob_buf_init(&b, 0);

	bb_add_string(&b, "path", param);
	prepare_optional_table(&b);

	int e = icwmp_ubus_invoke(BBF_OBJECT_NAME, "get", b.head, ubus_get_parameter_callback, &get_result);
	blob_buf_free(&b);

	if (e < 0) {
		CWMP_LOG(WARNING, "Get failed (%s) Ubus err code: %d", param, e);
		return "9002";
	}

	if (get_result.error) {
		char buf[8] = {0};

		CWMP_LOG(WARNING, "Get parameter values (%s) failed: fault_code: %d", param, get_result.error);

		snprintf(buf, sizeof(buf), "%d", get_result.error);
		return icwmp_strdup(buf);
	}

	return NULL;
}

char *cwmp_get_parameter_names(char *parameter_name, bool next_level, struct list_head *parameters_list)
{
	struct blob_buf b = {0};
	struct list_params_result get_result = {
			.parameters_list = parameters_list,
			.error = FAULT_CPE_NO_FAULT
	};
	unsigned int len = CWMP_STRLEN(parameter_name);

	if (len > 2 && parameter_name[len - 1] == '.' && parameter_name[len - 2] == '*')
		return "9005";

	char *object = parameter_name ? parameter_name : "";

	memset(&b, 0, sizeof(struct blob_buf));
	blob_buf_init(&b, 0);

	bb_add_string(&b, "path", object);
	blobmsg_add_u8(&b, "first_level", next_level);
	prepare_optional_table(&b);

	int e = icwmp_ubus_invoke(BBF_OBJECT_NAME, "get_instances", b.head, ubus_get_parameter_callback, &get_result);
	blob_buf_free(&b);

	if (e < 0) {
		CWMP_LOG(INFO, "object_names ubus method failed: Ubus err code: %d", e);
		return "9002";
	}

	if (get_result.error) {
		char buf[8] = {0};

		CWMP_LOG(WARNING, "Get parameter Names (%s) failed: fault_code: %d", object, get_result.error);

		snprintf(buf, sizeof(buf), "%d", get_result.error);
		return icwmp_strdup(buf);
	}

	return NULL;
}

/*
 * Set multiple parameter values
 */

void ubus_setm_values_callback(struct ubus_request *req, int type __attribute__((unused)), struct blob_attr *msg)
{
	if (msg == NULL) {
		CWMP_LOG(ERROR, "dm_iface %s: msg is null", __FUNCTION__);
		return;
	}
	struct setm_values_res *set_result = (struct setm_values_res *)req->priv;
	const struct blobmsg_policy p[2] = { { "status", BLOBMSG_TYPE_BOOL } };
	struct blob_attr *tb[2] = { NULL, NULL };
	blobmsg_parse(p, 2, tb, blobmsg_data(msg), blobmsg_len(msg));
	if (tb[0]) {
		set_result->status = blobmsg_get_u8(tb[0]);
		if (set_result->status)
			return;
	}
	set_result->status = false;
	struct blob_attr *faults_params = get_parameters_array(msg);
	if (faults_params == NULL) {
		CWMP_LOG(ERROR, "dm_iface %s: faults_param is null", __FUNCTION__);
		return;
	}
	const struct blobmsg_policy pfault[3] = { { "path", BLOBMSG_TYPE_STRING }, { "fault", BLOBMSG_TYPE_INT32 }, { "status", BLOBMSG_TYPE_BOOL } };
	struct blob_attr *cur;
	int rem;
	blobmsg_for_each_attr(cur, faults_params, rem)
	{
		struct blob_attr *tbi[3] = { NULL, NULL, NULL };
		blobmsg_parse(pfault, 3, tbi, blobmsg_data(cur), blobmsg_len(cur));
		if (!tbi[0] || !tbi[1])
			continue;
		cwmp_add_list_fault_param(blobmsg_get_string(tbi[0]), blobmsg_get_u32(tbi[1]), set_result->faults_list);
	}
}

int cwmp_set_multiple_parameters_values(struct list_head *parameters_values_list, struct list_head *faults_list)
{
	int e;
	struct cwmp_dm_parameter *param_value = NULL;
	struct setm_values_res set_result = { .faults_list = faults_list };
	struct blob_buf b = { 0 };

	memset(&b, 0, sizeof(struct blob_buf));
	blob_buf_init(&b, 0);
	void *arr = blobmsg_open_array(&b, "pv_tuple");
	list_for_each_entry (param_value, parameters_values_list, list) {
		if (!param_value->name)
			break;
		void *tbl = blobmsg_open_table(&b, "");
		blobmsg_add_string(&b, "path", param_value->name);
		blobmsg_add_string(&b, "value", param_value->value);
		blobmsg_close_table(&b, tbl);
	}
	blobmsg_close_array(&b, arr);
	blobmsg_add_u32(&b, "transaction_id", transaction_id);
	bb_add_string(&b, "proto", "cwmp");
	blobmsg_add_u32(&b, "instance_mode", cwmp_main->conf.instance_mode);

	e = icwmp_ubus_invoke(BBF_OBJECT_NAME, "setm_values", b.head, ubus_setm_values_callback, &set_result);
	blob_buf_free(&b);

	if (e < 0) {
		CWMP_LOG(INFO, "setm_values ubus method failed: Ubus err code: %d", e);
		return FAULT_CPE_INTERNAL_ERROR;
	}
	if (set_result.status == false) {
		CWMP_LOG(INFO, "Set parameter_values failed");
		return FAULT_CPE_INVALID_ARGUMENTS;
	}

	return FAULT_CPE_NO_FAULT;
}

/*
 * Add Delete object
 */
int get_single_fault_from_blob_attr(struct blob_attr *msg)
{
	int fault_code = FAULT_CPE_NO_FAULT;
	if (msg == NULL) {
		CWMP_LOG(ERROR, "dm_iface %s: msg is null", __FUNCTION__);
		return FAULT_CPE_INTERNAL_ERROR;
	}
	fault_code = get_fault_value(msg);
	if (fault_code != FAULT_CPE_NO_FAULT)
		return fault_code;
	struct blob_attr *faults_array = get_parameters_array(msg);
	if (faults_array == NULL)
		return FAULT_CPE_NO_FAULT;
	struct blob_attr *cur;
	int rem;
	blobmsg_for_each_attr(cur, faults_array, rem)
	{
		fault_code = get_fault_value(cur);
		if (fault_code != FAULT_CPE_NO_FAULT)
			break;
	}
	return fault_code;
}

void ubus_objects_callback(struct ubus_request *req, int type __attribute__((unused)), struct blob_attr *msg)
{
	if (msg == NULL) {
		CWMP_LOG(ERROR, "dm_iface %s: msg is null", __FUNCTION__);
		return;
	}
	int fault_code = get_single_fault_from_blob_attr(msg);
	struct object_result *result = (struct object_result *)req->priv;
	if (result == NULL) {
		CWMP_LOG(ERROR, "dm_iface %s: result is null", __FUNCTION__);
		return;
	}
	if (fault_code != FAULT_CPE_NO_FAULT) {
		snprintf(result->fault, 5, "%d", fault_code);
		result->status = false;
		return;
	}
	struct blob_attr *parameters = get_parameters_array(msg);
	const struct blobmsg_policy p[2] = { { "status", BLOBMSG_TYPE_BOOL }, { "instance", BLOBMSG_TYPE_STRING } };
	struct blob_attr *cur;
	int rem;
	blobmsg_for_each_attr(cur, parameters, rem)
	{
		struct blob_attr *tb[2] = { NULL, NULL };
		blobmsg_parse(p, 2, tb, blobmsg_data(cur), blobmsg_len(cur));
		if (!tb[0])
			continue;
		result->status = blobmsg_get_u8(tb[0]);
		if (tb[1]) {
			char **instance = result->instance;
			*instance = strdup(blobmsg_get_string(tb[1]));
		}
		break;
	}
}

static void prepare_add_delete_blobmsg(struct blob_buf *b, char *object_name)
{
	if (b == NULL)
		return;

	char *object = CWMP_STRLEN(object_name) ? object_name : DM_ROOT_OBJ;
	bb_add_string(b, "path", object);
	blobmsg_add_u32(b, "transaction_id", transaction_id);
	bb_add_string(b, "proto", "cwmp");
	blobmsg_add_u32(b, "instance_mode", cwmp_main->conf.instance_mode);
}

char *cwmp_add_object(char *object_name, char **instance)
{
	int e;
	struct object_result add_result = { .instance = instance };
	struct blob_buf b = { 0 };

	memset(&b, 0, sizeof(struct blob_buf));
	blob_buf_init(&b, 0);
	prepare_add_delete_blobmsg(&b, object_name);

	e = icwmp_ubus_invoke(BBF_OBJECT_NAME, "add_object", b.head, ubus_objects_callback, &add_result);
	blob_buf_free(&b);

	if (e < 0) {
		CWMP_LOG(INFO, "add_object ubus method failed: Ubus err code: %d", e);
		return "9002";
	}
	if (add_result.status == false) {
		CWMP_LOG(INFO, "AddObject failed");
		return icwmp_strdup(add_result.fault);
	}
	return NULL;
}

char *cwmp_delete_object(char *object_name)
{
	int e;
	struct object_result add_result = { .instance = NULL };
	struct blob_buf b = { 0 };

	memset(&b, 0, sizeof(struct blob_buf));
	blob_buf_init(&b, 0);
	prepare_add_delete_blobmsg(&b, object_name);

	e = icwmp_ubus_invoke(BBF_OBJECT_NAME, "del_object", b.head, ubus_objects_callback, &add_result);
	blob_buf_free(&b);

	if (e < 0) {
		CWMP_LOG(INFO, "del_object ubus method failed: Ubus err code: %d", e);
		return "9002";
	}

	if (add_result.status == false) {
		CWMP_LOG(INFO, "DeleteObject failed");
		return icwmp_strdup(add_result.fault);
	}

	return NULL;
}
