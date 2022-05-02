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

typedef int (*callback)(struct blob_buf *b);

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

static int reload_cmd(struct blob_buf *b)
{
	CWMP_LOG(INFO, "triggered ubus reload");
	if (cwmp_main.session_status.last_status == SESSION_RUNNING) {
		cwmp_set_end_session(END_SESSION_RELOAD);
		blobmsg_add_u32(b, "status", 0);
		blobmsg_add_string(b, "info", "Session running, reload at the end of the session");
	} else {
		pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
		cwmp_apply_acs_changes();
		pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
		blobmsg_add_u32(b, "status", 0);
		blobmsg_add_string(b, "info", "icwmpd config reloaded");
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
		if (strcmp(cmd, cmd_cb[i].str) == 0) {
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
	struct blob_attr *tb[__COMMAND_MAX];
	struct blob_buf blob_command;

	memset(&blob_command, 0, sizeof(struct blob_buf));
	blob_buf_init(&blob_command, 0);

	blobmsg_parse(icwmp_cmd_policy, ARRAY_SIZE(icwmp_cmd_policy), tb, blob_data(msg), blob_len(msg));

	if (!tb[COMMAND_NAME]) {
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
		goto send_reply;
	}

	char *cmd = blobmsg_data(tb[COMMAND_NAME]);

	if (call_command_cb(cmd, &blob_command) != 0) {
		blob_buf_free(&blob_command);
		return -1;
	}

send_reply:
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

	for (i = 0; i < size && arr[i] == 0; i++); // find the first non zero element

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

	time_t next_time = get_nonzero_min_time(sched_time, cwmp_main.session_status.next_retry, cwmp_main.session_status.next_periodic);

	return next_time;
}

static void bb_add_icwmp_status(struct blob_buf *bb)
{
	void *tbl = blobmsg_open_table(bb, "cwmp");
	bb_add_string(bb, "status", "up");
	bb_add_string(bb, "start_time", get_time(cwmp_main.start_time));
	bb_add_string(bb, "acs_url", cwmp_main.conf.acsurl);
	blobmsg_close_table(bb, tbl);
}

static void bb_add_icwmp_last_session(struct blob_buf *bb)
{
	void *tbl = blobmsg_open_table(bb, "last_session");
	const char *status = cwmp_main.session_status.last_start_time ? arr_session_status[cwmp_main.session_status.last_status] : "N/A";
	bb_add_string(bb, "status", status);
	char *start_time = cwmp_main.session_status.last_start_time ? get_time(cwmp_main.session_status.last_start_time) : "N/A";
	bb_add_string(bb, "start_time", start_time);
	char *end_time = cwmp_main.session_status.last_end_time ? get_time(cwmp_main.session_status.last_end_time) : "N/A";
	bb_add_string(bb, "end_time", end_time);
	blobmsg_close_table(bb, tbl);
}

static void bb_add_icwmp_next_session(struct blob_buf *bb)
{
	void *tbl = blobmsg_open_table(bb, "next_session");
	bb_add_string(bb, "status", arr_session_status[SESSION_WAITING]);
	time_t ntime = get_next_session_time();
	char *start_time = ntime ? get_time(ntime) : "N/A";
	bb_add_string(bb, "start_time", start_time);
	bb_add_string(bb, "end_time", "N/A");
	blobmsg_close_table(bb, tbl);
}

static void bb_add_icwmp_statistics(struct blob_buf *bb)
{
	void *tbl = blobmsg_open_table(bb, "statistics");
	blobmsg_add_u32(bb, "success_sessions", cwmp_main.session_status.success_session);
	blobmsg_add_u32(bb, "failure_sessions", cwmp_main.session_status.failure_session);
	blobmsg_add_u32(bb, "total_sessions", cwmp_main.session_status.success_session + cwmp_main.session_status.failure_session);
	blobmsg_close_table(bb, tbl);

}

static int icwmp_status_handler(struct ubus_context *ctx, struct ubus_object *obj __attribute__((unused)), struct ubus_request_data *req, const char *method __attribute__((unused)), struct blob_attr *msg __attribute__((unused)))
{
	struct blob_buf bb;

	memset(&bb, 0, sizeof(struct blob_buf));
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

static void icwmp_inform_get_rpc_method(struct ubus_context *ctx, struct ubus_request_data *req)
{
	struct event_container *event_container;
	struct session *session;
	struct blob_buf bb;

	if (ctx == NULL)
		return;

	memset(&bb, 0, sizeof(struct blob_buf));
	blob_buf_init(&bb, 0);

	pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
	event_container = cwmp_add_event_container(&cwmp_main, EVENT_IDX_2PERIODIC, "");
	if (event_container == NULL) {
		pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
		return;
	}

	cwmp_save_event_container(event_container);
	session = list_entry(cwmp_main.head_event_container, struct session, head_event_container);
	if (cwmp_add_session_rpc_acs(session, RPC_ACS_GET_RPC_METHODS) == NULL) {
		pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
		return;
	}

	pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
	pthread_cond_signal(&(cwmp_main.threshold_session_send));
	blobmsg_add_u32(&bb, "status", 1);
	blobmsg_add_string(&bb, "info", "Session with GetRPCMethods will start");

	ubus_send_reply(ctx, req, bb.head);
	blob_buf_free(&bb);
}

static void icwmp_inform_event(struct ubus_context *ctx, struct ubus_request_data *req, char *event)
{
	struct blob_buf bb;

	if (ctx == NULL || event == NULL)
		return;

	memset(&bb, 0, sizeof(struct blob_buf));
	blob_buf_init(&bb, 0);

	int event_code = cwmp_get_int_event_code(event);
	pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
	cwmp_add_event_container(&cwmp_main, event_code, "");
	pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
	pthread_cond_signal(&(cwmp_main.threshold_session_send));
	if (cwmp_main.session_status.last_status == SESSION_RUNNING) {
		blobmsg_add_u32(&bb, "status", -1);
		blobmsg_add_string(&bb, "info", "Session already running, event will be sent at the end of the session");
	} else {
		blobmsg_add_u32(&bb, "status", 1);
		blobmsg_add_string(&bb, "info", "Session started");
	}

	ubus_send_reply(ctx, req, bb.head);
	blob_buf_free(&bb);
}

static int icwmp_inform_handler(struct ubus_context *ctx, struct ubus_object *obj __attribute__((unused)), struct ubus_request_data *req, const char *method __attribute__((unused)), struct blob_attr *msg)
{
	struct blob_attr *tb[__INFORM_MAX];
	bool is_get_rpc = false;
	char *event = "";

	blobmsg_parse(icwmp_inform_policy, ARRAY_SIZE(icwmp_inform_policy), tb, blob_data(msg), blob_len(msg));

	if (tb[INFORM_GET_RPC_METHODS]) {
		is_get_rpc = blobmsg_data(tb[INFORM_GET_RPC_METHODS]);
	}
	if (tb[INFORM_EVENT]) {
		event = blobmsg_data(tb[INFORM_EVENT]);
	}

	if (is_get_rpc) {
		icwmp_inform_get_rpc_method(ctx, req);
	} else {
		icwmp_inform_event(ctx, req, event);
	}

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

int icwmp_delete_object(struct ubus_context *ctx)
{
	return ubus_remove_object(ctx, &tr069_object);
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

int icwmp_ubus_invoke(const char *obj, const char *method, struct blob_attr *msg, icwmp_ubus_cb icwmp_callback, void *callback_arg)
{
	uint32_t id;
	int rc = 0;

	struct ubus_context *ubus_ctx = NULL;

	ubus_ctx = ubus_connect(NULL);
	if (ubus_ctx == NULL)
		return -1;

	if (!ubus_lookup_id(ubus_ctx, obj, &id))
		rc = ubus_invoke(ubus_ctx, id, method, msg, icwmp_callback, callback_arg, 20000);
	else
		rc = -1;

	if (ubus_ctx) {
		ubus_free(ubus_ctx);
		ubus_ctx = NULL;
	}

	return rc;
}
