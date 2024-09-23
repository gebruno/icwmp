/*
 * ubus_utils.c - ubus methods and utility functions
 *
 * Copyright (C) 2022, IOPSYS Software Solutions AB.
 *
 * Author: suvendhu.hansa@iopsys.eu
 *
 * See LICENSE file for license related information
 *
 */

#include "ubus_utils.h"
#include "log.h"
#include "sched_inform.h"
#include "event.h"
#include "session.h"
#include "cwmp_event.h"
#include "autonomous_complpolicy.h"
#include "heartbeat.h"

typedef int (*callback)(struct blob_buf *b);

static bool g_bbf_object_available = false;
static struct ubus_context *ubus_ctx = NULL;
struct uloop_timeout u_timeout;

static int icwmp_register_object(struct ubus_context *ctx);

struct command_cb {
	char *str;
	callback cb;
	char *help;
};

static const char *arr_session_status[] = {
	[SESSION_WAITING] = "waiting",
	[SESSION_RUNNING] = "running",
	[SESSION_FAILURE] = "failure",
	[SESSION_SUCCESS] = "success",
};


static void interface_update_handler(struct ubus_context *ctx __attribute__((unused)),
			      struct ubus_event_handler *ev __attribute__((unused)),
			      const char *type __attribute__((unused)), struct blob_attr *msg)
{
	if (!msg)
		return;

	const struct blobmsg_policy p[2] = {
		{ "interface", BLOBMSG_TYPE_STRING },
		{ "action", BLOBMSG_TYPE_STRING },
	};

	struct blob_attr *tb[2] = {NULL};
	blobmsg_parse(p, 2, tb, blob_data(msg), blob_len(msg));

	if (!tb[0] || !tb[1])
		return;

	const char *intf_name = blobmsg_get_string(tb[0]);
	const char *intf_up = blobmsg_get_string(tb[1]);

	if (CWMP_STRCMP(intf_up, "ifup") != 0 || CWMP_STRCMP(cwmp_main->conf.default_wan_iface, intf_name) != 0)
		return;

	/* If the last session was failure then schedule a session */
	if (cwmp_main->session->session_status.last_status == SESSION_FAILURE) {
		CWMP_LOG(INFO, "Schedule session for interface_update on %s, since last session was failure", intf_name);
		trigger_cwmp_session_timer();
	}
}

static int reload_cmd(struct blob_buf *b)
{
	CWMP_LOG(INFO, "triggered ubus reload");
	if (cwmp_main->session->session_status.last_status == SESSION_RUNNING) {
		cwmp_set_end_session(END_SESSION_RELOAD);
		blobmsg_add_u32(b, "status", 0);
		blobmsg_add_string(b, "info", "Session running, reload at the end of the session");
	} else {
		int error = cwmp_apply_acs_changes();
		if (error != CWMP_OK) {
			// Failed to load cwmp config
			CWMP_LOG(ERROR, "cwmp failed to reload the configuration");
			blobmsg_add_u32(b, "status", -1);
			blobmsg_add_string(b, "info", "icwmpd config reload failed");
		} else {
			blobmsg_add_u32(b, "status", 0);
			blobmsg_add_string(b, "info", "icwmpd config reloaded");

			if (cwmp_main->acs_changed) {
				CWMP_LOG(INFO, "%s: Schedule session with new ACS since URL changed", __func__);
				uloop_timeout_cancel(&session_timer);
				cwmp_main->retry_count_session = 0;
				uloop_timeout_cancel(&heartbeat_session_timer);
				cwmp_main->session->session_status.next_heartbeat = true;
				cwmp_main->session->session_status.is_heartbeat = false;
				trigger_cwmp_session_timer();
				cwmp_main->acs_changed = false;
			}
		}
	}

	return 0;
}

static struct command_cb cmd_cb[] ={
	{ "reload", reload_cmd, "Reload icwmpd with new configuration" }
};

static int call_command_cb(char *cmd, struct blob_buf *b)
{
	int cmd_num, i;
	callback cb = NULL;

	if (cmd == NULL || b == NULL)
		return -1;

	cmd_num = sizeof(cmd_cb)/sizeof(struct command_cb);
	for (i = 0; i < cmd_num; i++) {
		if (CWMP_STRCMP(cmd, cmd_cb[i].str) == 0) {
			cb = cmd_cb[i].cb;
			break;
		}
	}

	if (cb == NULL) {
		char info[128] = {0};
		if (snprintf(info, sizeof(info), "\'%s\' is not supported. Check the supported commands.", cmd) == -1)
			return -1;

		blobmsg_add_u32(b, "status", -1);
		blobmsg_add_string(b, "info", info);
		return 0;
	}

	return cb(b);
}

enum command
{
	COMMAND_NAME,
	__COMMAND_MAX
};

static const struct blobmsg_policy icwmp_cmd_policy[] = {
	[COMMAND_NAME] = {.name = "command", .type = BLOBMSG_TYPE_STRING },
};

static int icwmp_command_handler(struct ubus_context *ctx, struct ubus_object *obj __attribute__((unused)), struct ubus_request_data *req, const char *method __attribute__((unused)), struct blob_attr *msg)
{
	if (ctx == NULL)
		return -1;
	struct blob_attr *tb[__COMMAND_MAX] = {0};
	struct blob_buf blob_command;

	CWMP_MEMSET(&blob_command, 0, sizeof(struct blob_buf));
	blob_buf_init(&blob_command, 0);

	int ret = blobmsg_parse(icwmp_cmd_policy, ARRAY_SIZE(icwmp_cmd_policy), tb, blob_data(msg), blob_len(msg));
	if (ret != 0) {
		int i;
		int cmd_num = sizeof(cmd_cb)/sizeof(struct command_cb);
		void *arr = blobmsg_open_array(&blob_command, "SupportedCommands");
		for (i = 0; i < cmd_num; i++) {
			void *tbl_in = blobmsg_open_table(&blob_command, "");
			bb_add_string(&blob_command, "command", cmd_cb[i].str);
			bb_add_string(&blob_command, "description", cmd_cb[i].help);
			blobmsg_close_table(&blob_command, tbl_in);
		}
		blobmsg_close_array(&blob_command, arr);
	} else {
		char *cmd = blobmsg_get_string(tb[COMMAND_NAME]);
		call_command_cb(cmd, &blob_command);
	}

	ubus_send_reply(ctx, req, blob_command.head);
	blob_buf_free(&blob_command);

	return 0;
}

static time_t get_nonzero_min_time(time_t time1, time_t time2, time_t time3)
{
	time_t arr[] = { time1, time2, time3 };
	time_t min = 0;
	int i;
	int size = sizeof(arr)/sizeof(time_t);

	for (i = 0; i < size && arr[i] == 0; i++) {} // find the first non zero element

	if (i == size) {
		return min; // array has no non-zero values
	}

	min = arr[i];
	for (; i < size; i++) {
		if (arr[i] != 0 && arr[i] < min)
			min = arr[i];
	}

	return min;
}

static time_t get_next_session_time()
{
	time_t sched_time = 0;
	if (list_schedule_inform.next != &(list_schedule_inform)) {
		struct schedule_inform *schedule_inform;
		schedule_inform = list_entry(list_schedule_inform.next, struct schedule_inform, list);
		sched_time = schedule_inform->scheduled_time;
	}

	time_t next_time = get_nonzero_min_time(sched_time, cwmp_main->session->session_status.next_retry, cwmp_main->session->session_status.next_periodic);

	return next_time;
}

static void bb_add_icwmp_status(struct blob_buf *bb)
{
	if (bb == NULL) {
		CWMP_LOG(ERROR, "icwmp status blob is null");
		return;
	}

	char start_time[26] = {0};
	get_time(cwmp_main->start_time, start_time, sizeof(start_time));

	void *tbl = blobmsg_open_table(bb, "cwmp");
	bb_add_string(bb, "status", "up");
	bb_add_string(bb, "start_time", start_time);
	bb_add_string(bb, "acs_url", cwmp_main->conf.acs_url);
	blobmsg_close_table(bb, tbl);
}

static void bb_add_icwmp_last_session(struct blob_buf *bb)
{

	char start_time[26] = {0};
	char end_time[26] = {0};

	void *tbl = blobmsg_open_table(bb, "last_session");
	const char *status = cwmp_main->session->session_status.last_start_time ? arr_session_status[cwmp_main->session->session_status.last_status] : "N/A";

	if (cwmp_main->session->session_status.last_start_time) {
		get_time(cwmp_main->session->session_status.last_start_time, start_time, sizeof(start_time));
	} else {
		snprintf(start_time, sizeof(start_time), "N/A");
	}

	if (cwmp_main->session->session_status.last_end_time) {
		get_time(cwmp_main->session->session_status.last_end_time, end_time, sizeof(end_time));
	} else {
		snprintf(end_time, sizeof(end_time), "N/A");
	}

	bb_add_string(bb, "status", status);
	bb_add_string(bb, "start_time", start_time);
	bb_add_string(bb, "end_time", end_time);
	blobmsg_close_table(bb, tbl);
}

static void bb_add_icwmp_next_session(struct blob_buf *bb)
{
	char next_time[26] = {0};
	time_t ntime = get_next_session_time();

	if (ntime) {
		get_time(ntime, next_time, sizeof(next_time));
	} else {
		snprintf(next_time, sizeof(next_time), "N/A");
	}
	
	void *tbl = blobmsg_open_table(bb, "next_session");
	bb_add_string(bb, "status", arr_session_status[SESSION_WAITING]);
	bb_add_string(bb, "start_time", next_time);
	bb_add_string(bb, "end_time", "N/A");
	blobmsg_close_table(bb, tbl);
}

static void bb_add_icwmp_statistics(struct blob_buf *bb)
{
	void *tbl = blobmsg_open_table(bb, "statistics");
	blobmsg_add_u32(bb, "success_sessions", cwmp_main->session->session_status.success_session);
	blobmsg_add_u32(bb, "failure_sessions", cwmp_main->session->session_status.failure_session);
	blobmsg_add_u32(bb, "total_sessions", cwmp_main->session->session_status.success_session + cwmp_main->session->session_status.failure_session);
	blobmsg_close_table(bb, tbl);

}

static int icwmp_status_handler(struct ubus_context *ctx, struct ubus_object *obj __attribute__((unused)), struct ubus_request_data *req, const char *method __attribute__((unused)), struct blob_attr *msg __attribute__((unused)))
{
	struct blob_buf bb;

	CWMP_MEMSET(&bb, 0, sizeof(struct blob_buf));
	blob_buf_init(&bb, 0);

	bb_add_icwmp_status(&bb);
	bb_add_icwmp_last_session(&bb);
	bb_add_icwmp_next_session(&bb);
	bb_add_icwmp_statistics(&bb);

	ubus_send_reply(ctx, req, bb.head);
	blob_buf_free(&bb);

	return 0;
}

enum enum_inform
{
	INFORM_GET_RPC_METHODS,
	INFORM_EVENT,
	__INFORM_MAX
};

static const struct blobmsg_policy icwmp_inform_policy[] = {
	[INFORM_GET_RPC_METHODS] = {.name = "GetRPCMethods", .type = BLOBMSG_TYPE_BOOL },
	[INFORM_EVENT] = {.name = "event", .type = BLOBMSG_TYPE_STRING },
};

static int icwmp_inform_get_rpc_method(struct blob_buf *bb)
{
	if (cwmp_main->conf.acs_getrpc && cwmp_add_session_rpc_acs(RPC_ACS_GET_RPC_METHODS) == NULL)
		return -1;

	blobmsg_add_u32(bb, "status", 1);
	blobmsg_add_string(bb, "info", "Session with GetRPCMethods will start");

	return EVENT_IDX_2PERIODIC;
}

static int icwmp_inform_event(struct blob_buf *bb, const char *event)
{
	int event_code = cwmp_get_int_event_code(event);
	if (event_code != -1) {
		if (cwmp_main->session->session_status.last_status == SESSION_RUNNING) {
			blobmsg_add_u32(bb, "status", 1);
			blobmsg_add_string(bb, "info", "Session already running, event will be sent at the end of the session");
		} else {
			blobmsg_add_u32(bb, "status", 1);
			blobmsg_add_string(bb, "info", "Session started");
		}
	}
	return event_code;
}

static int icwmp_inform_handler(struct ubus_context *ctx, struct ubus_object *obj __attribute__((unused)), struct ubus_request_data *req, const char *method __attribute__((unused)), struct blob_attr *msg)
{
	struct blob_buf bb;
	CWMP_MEMSET(&bb, 0, sizeof(struct blob_buf));
	blob_buf_init(&bb, 0);

	struct blob_attr *tb[__INFORM_MAX] = {0};
	bool is_get_rpc = false;
	const char *event = "";
	int event_code;

	int ret = blobmsg_parse(icwmp_inform_policy, ARRAY_SIZE(icwmp_inform_policy), tb, blob_data(msg), blob_len(msg));

	if (ret == 0 && tb[INFORM_GET_RPC_METHODS] != NULL) {
		is_get_rpc = blobmsg_get_u8(tb[INFORM_GET_RPC_METHODS]);
	}

	if (ret == 0 && tb[INFORM_EVENT] != NULL) {
		event = blobmsg_get_string(tb[INFORM_EVENT]);
	}

	if (is_get_rpc) {
		event_code = icwmp_inform_get_rpc_method(&bb);
	} else {
		event_code = icwmp_inform_event(&bb, event);
	}
	if (event_code == -1) {
		CWMP_LOG(WARNING, "tr069 ubus: ubus inform method not able to get the event code");
		blobmsg_add_u32(&bb, "status", -1);
		blobmsg_add_string(&bb, "info", "not able to get the event code");
		goto end;
	}

	struct session_timer_event *ubus_inform_event = calloc(1, sizeof(struct session_timer_event));

	ubus_inform_event->session_timer_evt.cb = cwmp_schedule_session_with_event;
	ubus_inform_event->event = event_code;
	trigger_cwmp_session_timer_with_event(&ubus_inform_event->session_timer_evt);

end:
	ubus_send_reply(ctx, req, bb.head);
	blob_buf_free(&bb);
	return 0;
}

static const struct ubus_method icwmp_methods[] = {
	UBUS_METHOD("command", icwmp_command_handler, icwmp_cmd_policy),
	UBUS_METHOD_NOARG("status", icwmp_status_handler),
	UBUS_METHOD("inform", icwmp_inform_handler, icwmp_inform_policy),
};

static struct ubus_object_type tr069_object_type = UBUS_OBJECT_TYPE("icwmpd", icwmp_methods);

static struct ubus_object tr069_object = {
	.name = "tr069",
	.type = &tr069_object_type,
	.methods = icwmp_methods,
	.n_methods = ARRAY_SIZE(icwmp_methods),
};

int icwmp_register_object(struct ubus_context *ctx)
{
	return ubus_add_object(ctx, &tr069_object);
}

void bb_add_string(struct blob_buf *bb, const char *name, const char *value)
{
	if (bb == NULL)
		return;

	if (value)
		blobmsg_add_string(bb, name, value);
	else
		blobmsg_add_string(bb, name, "");
}

int icwmp_connect_ubus()
{
	ubus_ctx = ubus_connect(NULL);
	if (!ubus_ctx)
		return -1;

	return 0;
}

int icwmp_uloop_ubus_register()
{
	if (!ubus_ctx)
		return -1;

	ubus_add_uloop(ubus_ctx);

	if (icwmp_register_object(ubus_ctx))
		return -1;

	return 0;
}

void icwmp_uloop_ubus_exit()
{
	if (ubus_ctx) {
		ubus_remove_object(ubus_ctx, &tr069_object);
		ubus_free(ubus_ctx);
		ubus_ctx = NULL;
	}
}

int icwmp_ubus_invoke(const char *obj, const char *method, struct blob_attr *msg, icwmp_ubus_cb icwmp_callback, void *callback_arg)
{
	uint32_t id;
	int rc = 0;

	if (ubus_ctx == NULL) {
		CWMP_LOG(ERROR, "Failed to connect with ubus err: %d", errno);
		return -1;
	}

	if (!ubus_lookup_id(ubus_ctx, obj, &id))
		rc = ubus_invoke(ubus_ctx, id, method, msg, icwmp_callback, callback_arg, 60000);
	else
		rc = -1;

	return rc;
}

int initiate_autonomous_complpolicy(void)
{
	cwmp_main->ev = (struct ubus_event_handler *)malloc(sizeof(struct ubus_event_handler));
	if (cwmp_main->ev == NULL)
		return -1;

	CWMP_MEMSET(cwmp_main->ev, 0, sizeof(struct ubus_event_handler));
	cwmp_main->ev->cb = autonomous_notification_handler;

	int ret = ubus_register_event_handler(ubus_ctx, cwmp_main->ev, "bbfdm.event");
	if (ret) {
		return -1;
	}

	return 0;
}

void clean_autonomous_complpolicy(void)
{
	if (cwmp_main->ev == NULL)
		return;

	ubus_unregister_event_handler(ubus_ctx, cwmp_main->ev);
}

int initiate_interface_update(void)
{
	cwmp_main->intf_ev = (struct ubus_event_handler *)malloc(sizeof(struct ubus_event_handler));
	if (cwmp_main->intf_ev == NULL)
		return -1;

	CWMP_MEMSET(cwmp_main->intf_ev, 0, sizeof(struct ubus_event_handler));
	cwmp_main->intf_ev->cb = interface_update_handler;

	int ret = ubus_register_event_handler(ubus_ctx, cwmp_main->intf_ev, "network.interface");
	if (ret) {
		return -1;
	}

	return 0;
}

void clean_interface_update(void)
{
	if (cwmp_main->intf_ev == NULL)
		return;

	ubus_unregister_event_handler(ubus_ctx, cwmp_main->intf_ev);
}

static void lookup_event_cb(struct ubus_context *ctx __attribute__((unused)),
		struct ubus_event_handler *ev __attribute__((unused)),
		const char *type, struct blob_attr *msg)
{
	const struct blobmsg_policy policy = {
		"path", BLOBMSG_TYPE_STRING
	};
	struct blob_attr *attr;
	const char *path;

	if (CWMP_STRCMP(type, "ubus.object.add") != 0)
		return;

	blobmsg_parse(&policy, 1, &attr, blob_data(msg), blob_len(msg));
	if (!attr)
		return;

	path = blobmsg_data(attr);
	if (CWMP_STRCMP(path, BBFDM_OBJECT_NAME) == 0) {
		g_bbf_object_available = true;
		uloop_timeout_cancel(&u_timeout);
		uloop_end();
	}
}

static void lookup_timeout_cb(struct uloop_timeout *timeout __attribute__((unused)))
{
	uloop_end();
}

int wait_for_bbf_object()
{
#define BBF_WAIT_TIMEOUT 60

	int ret;
	uint32_t ubus_id;
	struct ubus_event_handler add_event;

	g_bbf_object_available = false;
	if (ubus_ctx == NULL) {
		CWMP_LOG(ERROR, "Can't create ubus context");
		return FAULT_CPE_INTERNAL_ERROR;
	}

	uloop_init();
	ubus_add_uloop(ubus_ctx);

	// register for add event
	CWMP_MEMSET(&add_event, 0, sizeof(struct ubus_event_handler));
	add_event.cb = lookup_event_cb;
	ubus_register_event_handler(ubus_ctx, &add_event, "ubus.object.add");

	// check if object already present
	ret = ubus_lookup_id(ubus_ctx, BBFDM_OBJECT_NAME, &ubus_id);
	if (ret == 0) {
		g_bbf_object_available = true;
		goto end;
	}

	// Set timeout to expire lookup
	CWMP_MEMSET(&u_timeout, 0, sizeof(struct uloop_timeout));
	u_timeout.cb = lookup_timeout_cb;
	uloop_timeout_set(&u_timeout, BBF_WAIT_TIMEOUT * 1000);

	uloop_run();
	uloop_done();

end:
	ubus_remove_object(ubus_ctx, &add_event.obj);

	if (g_bbf_object_available == false) {
		CWMP_LOG(ERROR, "%s object not found", BBFDM_OBJECT_NAME);
		return FAULT_CPE_INTERNAL_ERROR;
	}

	return 0;
}

void icwmp_free_ubus()
{
	if (ubus_ctx) {
		ubus_free(ubus_ctx);
		ubus_ctx = NULL;
	}
}
