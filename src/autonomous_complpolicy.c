/*
 * autonomous_complpolicy.c - CWMP autonomous notification methods
 *
 * Copyright (C) 2021-2022, IOPSYS Software Solutions AB.
 *
 *	  Author Suvendhu Hansa <suvendhu.hansa@iopsys.eu>
 *
 * See LICENSE file for license related information.
 *
 */

#include "autonomous_complpolicy.h"
#include "cwmp_uci.h"
#include "cwmp_du_state.h"
#include "log.h"
#include <libubox/blobmsg_json.h>
#include "backupSession.h"
#include "common.h"
#include "session.h"
#include "event.h"

enum autonomous_notif_type {
	DU_STATE_CHANGE,
	__MAX_NOTIF_TYPE
};

typedef void (*autonomous_event_callback)(struct blob_attr *msg);

struct autonomous_event {
	char name[2048];
	autonomous_event_callback cb;
};

static bool filters_qualified(int type, void *data)
{
	char *enable = NULL, *op_filter = NULL, *res_filter = NULL;

	switch (type) {
	case DU_STATE_CHANGE:
		cwmp_uci_reinit();
		uci_get_state_value(UCI_AUTONOMOUS_DU_STATE_ENABLE, &enable);
		uci_get_state_value(UCI_AUTONOMOUS_DU_STATE_OPERATION, &op_filter);
		uci_get_state_value(UCI_AUTONOMOUS_DU_STATE_RESULT, &res_filter);

		auto_du_state_change_compl *p = (auto_du_state_change_compl *)data;
		if (p == NULL)
			return false;

		if (enable == NULL || op_filter == NULL)
			return false;

		if (uci_str_to_bool(enable) == false || strlen(op_filter) == 0)
			return false;

		if (strstr(op_filter, p->operation) == NULL)
			return false;

		if (res_filter && strlen(res_filter) != 0 && strcmp(res_filter, "Both") != 0) {
			if (strcmp(res_filter, "Failure") == 0 && strcmp(p->current_state, "Failed") != 0)
				return false;

			if (strcmp(res_filter, "Success") == 0 && strcmp(p->current_state, "Failed") == 0)
				return false;
		}
		/* For now falut_code filter is not supported since usp-data-model
		 * only provides 7002, 7004, 7223, 7225, 7226, 7227 & 7229 but
		 * cwmp-data-model supports more fault-codes with more specific cause,
		 * so need to implement a mapping between usp FaultString & cwmp error code */
		break;
	default:
		return false;
	}

	return true;
}

static void send_du_state_change_notif(struct blob_attr *msg)
{
	if (!cwmp_main->conf.auto_cdu_enable) {
		CWMP_LOG(INFO, "Autonomous Change DU State is disabled");
		return;
	}
	(void)msg;
	CWMP_LOG(INFO, "Received DU STATE CHANGE EVENT");
	const struct blobmsg_policy p[2] = {
		{ "name", BLOBMSG_TYPE_STRING },
		{ "input", BLOBMSG_TYPE_TABLE }
	};

	const struct blobmsg_policy p1[9] = {
		{ "UUID", BLOBMSG_TYPE_STRING },
		{ "Version", BLOBMSG_TYPE_STRING },
		{ "CurrentState", BLOBMSG_TYPE_STRING },
		{ "Resolved", BLOBMSG_TYPE_INT8 },
		{ "StartTime", BLOBMSG_TYPE_STRING },
		{ "CompleteTime", BLOBMSG_TYPE_STRING },
		{ "OperationPerformed", BLOBMSG_TYPE_STRING },
		{ "Fault.FaultCode", BLOBMSG_TYPE_INT32 },
		{ "Fault.FaultString", BLOBMSG_TYPE_STRING }
	};

	struct blob_attr *tb[2] = {NULL};
	blobmsg_parse(p, 2, tb, blob_data(msg), blob_len(msg));

	if (tb[1]) {
		char *uuid = NULL, *oper = NULL;

		CWMP_LOG(INFO, "%s\n", blobmsg_format_json_indent(tb[1], true, -1));
		struct blob_attr *tb1[9] = {NULL};
		blobmsg_parse(p1, 9, tb1, blobmsg_data(tb[1]), blobmsg_len(tb[1]));

		if (tb1[0]) {
			uuid = blobmsg_get_string(tb1[0]);
		}

		if (tb1[6]) {
			oper = blobmsg_get_string(tb1[6]);
		}

		CWMP_LOG(INFO, "uuid: %s, oper: %s\n", uuid ? uuid : "", oper ? oper : "");
		if (uuid == NULL || oper == NULL)
			return;

		if (exists_in_uuid_list(uuid, oper)) {
			/* This DU operation was performed by cwmp */
			remove_node_from_uuid_list(uuid, oper);
		} else {
			/* This DU operation was performed from outside */
			auto_du_state_change_compl *data = calloc(1, sizeof(auto_du_state_change_compl));
			if (data == NULL)
				return;

			data->uuid = strdup(uuid);
			data->operation = strdup(oper);

			if (tb1[1]) {
				data->ver = strdup(blobmsg_get_string(tb1[1]));
			}

			if (tb1[2]) {
				data->current_state = strdup(blobmsg_get_string(tb1[2]));
			}

			if (tb1[3]) {
				data->resolved = blobmsg_get_u8(tb1[3]) ? 1 : 0;
			}

			if (tb1[4]) {
				data->start_time = strdup(blobmsg_get_string(tb1[4]));
			}

			if (tb1[5]) {
				data->complete_time = strdup(blobmsg_get_string(tb1[5]));
			}

			if (tb1[7]) {
				//data->fault_code = blobmsg_get_u32(tb1[7]);
				data->fault_code = 9001; // for now setting a generic code */
			}

			if (tb1[8]) {
				data->fault_string = strdup(blobmsg_get_string(tb1[8]));
			}

			// Check autonomous_du_state_change_complpolicy filters
			if (filters_qualified(DU_STATE_CHANGE, data) == false) {
				CWMP_LOG(INFO, "autonomous du state change filters not matched");
				FREE(data);
				return;
			}

			bkp_session_insert_autonomous_du_state_change(data);
			bkp_session_save();

			CWMP_LOG(INFO, "autonomous du state change event added");
			struct session_timer_event *ubus_inform_event = calloc(1, sizeof(struct session_timer_event));

			ubus_inform_event->extra_data = data;
			ubus_inform_event->session_timer_evt.cb = cwmp_schedule_session_with_event;
			ubus_inform_event->event = EVENT_IDX_12AUTONOMOUS_DU_STATE_CHANGE_COMPLETE;
			trigger_cwmp_session_timer_with_event(&ubus_inform_event->session_timer_evt);
		}
	}
}

static void send_transfer_complete_notif(struct blob_attr *msg)
{
	if (!cwmp_main->conf.auto_tc_enable) {
		CWMP_LOG(INFO, "Autonomous TransferComplete is disabled");
		return;
	}
	(void)msg;
	CWMP_LOG(INFO, "Received TRANSFER COMPLETE EVENT");
	const struct blobmsg_policy p[2] = {
		{ "name", BLOBMSG_TYPE_STRING },
		{ "input", BLOBMSG_TYPE_TABLE }
	};

	const struct blobmsg_policy p1[10] = {
		{ "AnnounceURL", BLOBMSG_TYPE_STRING },
		{ "TransferURL", BLOBMSG_TYPE_STRING },
		{ "TransferType", BLOBMSG_TYPE_STRING },
		{ "FileType", BLOBMSG_TYPE_STRING },
		{ "FileSize", BLOBMSG_TYPE_INT32 },
		{ "TargetFileName", BLOBMSG_TYPE_STRING },
		{ "StartTime", BLOBMSG_TYPE_STRING },
		{ "CompleteTime", BLOBMSG_TYPE_STRING },
		{ "Fault.FaultCode", BLOBMSG_TYPE_INT32 },
		{ "Fault.FaultString", BLOBMSG_TYPE_STRING }
	};

	struct blob_attr *tb[2] = {NULL, NULL};
	blobmsg_parse(p, 2, tb, blob_data(msg), blob_len(msg));

	if (tb[1]) {
		char *file_type = NULL;

		CWMP_LOG(INFO, "%s\n", blobmsg_format_json_indent(tb[1], true, -1));
		struct blob_attr *tb1[10] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
		blobmsg_parse(p1, 10, tb1, blobmsg_data(tb[1]), blobmsg_len(tb[1]));

		if (tb1[3]) {
			file_type = blobmsg_get_string(tb1[3]);
		}

		CWMP_LOG(INFO, "file_type: %s\n", file_type);
		if (file_type == NULL)
			return;

		auto_transfer_complete *data = calloc(1, sizeof(auto_transfer_complete));
		if (data == NULL)
			return;

		data->file_type = strdup(file_type);
		data->announce_url = strdup(tb1[0] ? blobmsg_get_string(tb1[0]) : "");
		data->transfer_url = strdup(tb1[1] ? blobmsg_get_string(tb1[1]) : "");
		data->is_download = (tb1[2] && strcmp(blobmsg_get_string(tb1[2]), "Download") == 0) ? true : false;
		data->file_size = tb1[4] ? blobmsg_get_u32(tb1[4]) : 0;
		data->target_file_name = strdup(tb1[5] ? blobmsg_get_string(tb1[5]) : "");

		if (tb1[6]) {
			data->start_time = strdup(blobmsg_get_string(tb1[6]));
		}

		if (tb1[7]) {
			data->complete_time = strdup(blobmsg_get_string(tb1[7]));
		}

		if (tb1[8]) {
			data->fault_code = tb1[8] ? blobmsg_get_u32(tb1[8]) : 0;
		}

		if (tb1[9]) {
			data->fault_string = strdup(blobmsg_get_string(tb1[8]));
		}

		bkp_session_insert_autonomous_transfer_complete(data);
		bkp_session_save();

		CWMP_LOG(INFO, "autonomous transfer complete event added");
		struct session_timer_event *ubus_inform_event = calloc(1, sizeof(struct session_timer_event));

		ubus_inform_event->extra_data = data;
		ubus_inform_event->session_timer_evt.cb = cwmp_schedule_session_with_event;
		ubus_inform_event->event = EVENT_IDX_10AUTONOMOUS_TRANSFER_COMPLETE;
		trigger_cwmp_session_timer_with_event(&ubus_inform_event->session_timer_evt);
	}
}

static struct autonomous_event event_info[] = {
	{ "Device.SoftwareModules.DUStateChange!", send_du_state_change_notif },
	{ "Device.LocalAgent.TransferComplete!", send_transfer_complete_notif }
};

static void send_autonomous_notification(char *ev_name, struct blob_attr *msg)
{
	int i;

	if (!ev_name)
		return;

	int count = sizeof(event_info)/sizeof(struct autonomous_event);
	for (i = 0; i < count; i++) {
		if (strcmp(event_info[i].name, ev_name) == 0) {
			autonomous_event_callback cb = event_info[i].cb;
			cb(msg);
			return;
		}
	}
}

void autonomous_notification_handler(struct ubus_context *ctx __attribute__((unused)),
				struct ubus_event_handler *ev __attribute__((unused)),
				const char *type __attribute__((unused)), struct blob_attr *msg)
{
	if (!msg)
		return;

	size_t len = (size_t)blobmsg_data_len(msg);
	struct blob_attr *attr;

	__blob_for_each_attr(attr, blobmsg_data(msg), len) {
		const char *attr_name = blobmsg_name(attr);
		if (attr_name != NULL && strcmp(attr_name, "name") == 0) {
			send_autonomous_notification(blobmsg_data(attr), msg);
			break;
		}
	}
}

int cwmp_rpc_acs_destroy_data_autonomous_du_state_change_complete(struct rpc *rpc)
{
	auto_du_state_change_compl *p = (auto_du_state_change_compl *)rpc->extra_data;
	if (p) {
		bkp_session_delete_autonomous_du_state_change(p);
		FREE(p->uuid);
		FREE(p->ver);
		FREE(p->current_state);
		FREE(p->start_time);
		FREE(p->complete_time);
		FREE(p->fault_string);
		FREE(p->operation);
		FREE(p);
	}

	return 0;
}

int cwmp_rpc_acs_destroy_data_autonomous_transfer_complete(struct rpc *rpc)
{
	auto_transfer_complete *p = (auto_transfer_complete *)rpc->extra_data;
	if (p) {
		bkp_session_delete_autonomous_transfer_complete(p);
		FREE(p->announce_url);
		FREE(p->transfer_url);
		FREE(p->file_type);
		FREE(p->start_time);
		FREE(p->complete_time);
		FREE(p->fault_string);
		FREE(p->target_file_name);
		FREE(p);
	}

	return 0;
}
