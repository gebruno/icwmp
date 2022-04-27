/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2013-2021 iopsys Software Solutions AB
 *	  Author Mohamed Kallel <mohamed.kallel@pivasoftware.com>
 *	  Author Ahmed Zribi <ahmed.zribi@pivasoftware.com>
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 *	Copyright (C) 2012 Luka Perkov <freecwmp@lukaperkov.net>
 */

#include <json-c/json.h>
#include <pthread.h>
#include <sys/socket.h>
#include <libubox/blobmsg_json.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/rtnetlink.h>

#include "ubus.h"
#include "session.h"
#include "log.h"
#include "sched_inform.h"
#include "cwmp_http.h"
#include "download.h"
#include "upload.h"
#include "cwmp_du_state.h"
#include "cwmp_uci.h"
#include "event.h"

static struct ubus_context *ctx = NULL;

static const char *arr_session_status[] = {
		[SESSION_WAITING] = "waiting",
		[SESSION_RUNNING] = "running",
		[SESSION_FAILURE] = "failure",
		[SESSION_SUCCESS] = "success",
};

enum command
{
	COMMAND_NAME,
	__COMMAND_MAX
};

static const struct blobmsg_policy command_policy[] = {
		[COMMAND_NAME] = {.name = "command", .type = BLOBMSG_TYPE_STRING },
};

static int cwmp_handle_command(struct ubus_context *ctx, struct ubus_object *obj __attribute__((unused)), struct ubus_request_data *req, const char *method __attribute__((unused)), struct blob_attr *msg)
{
	struct blob_attr *tb[__COMMAND_MAX];
	struct blob_buf blob_command;

	blobmsg_parse(command_policy, ARRAY_SIZE(command_policy), tb, blob_data(msg), blob_len(msg));

	if (!tb[COMMAND_NAME])
		return UBUS_STATUS_INVALID_ARGUMENT;

	memset(&blob_command, 0, sizeof(struct blob_buf));
	blob_buf_init(&blob_command, 0);

	char *cmd = blobmsg_data(tb[COMMAND_NAME]);
	char info[128] = {0};

	if (!strcmp("reload_end_session", cmd)) {
		CWMP_LOG(INFO, "triggered ubus reload_end_session");
		cwmp_set_end_session(END_SESSION_RELOAD);
		blobmsg_add_u32(&blob_command, "status", 0);
		if (snprintf(info, sizeof(info), "icwmpd config will reload at the end of the session") == -1)
			return -1;
	} else if (!strcmp("reload", cmd)) {
		CWMP_LOG(INFO, "triggered ubus reload");
		if (cwmp_main.session_status.last_status == SESSION_RUNNING) {
			cwmp_set_end_session(END_SESSION_RELOAD);
			blobmsg_add_u32(&blob_command, "status", 0);
			if (snprintf(info, sizeof(info), "Session running, reload at the end of the session") == -1)
				return -1;
		} else {
			pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
			cwmp_apply_acs_changes();
			pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
			blobmsg_add_u32(&blob_command, "status", 0);
			if (snprintf(info, sizeof(info), "icwmpd config reloaded") == -1)
				return -1;
		}
	} else if (!strcmp("reboot_end_session", cmd)) {
		CWMP_LOG(INFO, "triggered ubus reboot_end_session");
		cwmp_set_end_session(END_SESSION_REBOOT);
		blobmsg_add_u32(&blob_command, "status", 0);
		if (snprintf(info, sizeof(info), "icwmpd will reboot at the end of the session") == -1)
			return -1;
	} else if (!strcmp("action_end_session", cmd)) {
		CWMP_LOG(INFO, "triggered ubus action_end_session");
		cwmp_set_end_session(END_SESSION_EXTERNAL_ACTION);
		blobmsg_add_u32(&blob_command, "status", 0);
		if (snprintf(info, sizeof(info), "icwmpd will execute the scheduled action commands at the end of the session") == -1)
			return -1;
	} else if (!strcmp("exit", cmd)) {
		ubus_exit = true;
		thread_end = true;
		
		if (cwmp_main.session_status.last_status == SESSION_RUNNING)
			cwmp_http_set_timeout();

		pthread_cond_signal(&(cwmp_main.threshold_session_send));
		pthread_cond_signal(&(cwmp_main.threshold_periodic));
		pthread_cond_signal(&(cwmp_main.threshold_notify_periodic));
		pthread_cond_signal(&threshold_schedule_inform);
		pthread_cond_signal(&threshold_download);
		pthread_cond_signal(&threshold_change_du_state);
		pthread_cond_signal(&threshold_schedule_download);
		pthread_cond_signal(&threshold_apply_schedule_download);
		pthread_cond_signal(&threshold_upload);

		uloop_end();

		shutdown(cwmp_main.cr_socket_desc, SHUT_RDWR);

		if (!signal_exit)
			kill(getpid(), SIGTERM);

		if (snprintf(info, sizeof(info), "icwmpd daemon stopped") == -1)
			return -1;
	} else {
		blobmsg_add_u32(&blob_command, "status", -1);
		if (snprintf(info, sizeof(info), "%s command is not supported", cmd) == -1)
			return -1;
	}

	blobmsg_add_string(&blob_command, "info", info);
	ubus_send_reply(ctx, req, blob_command.head);
	blob_buf_free(&blob_command);

	return 0;
}

static inline time_t get_session_status_next_time()
{
	time_t ntime = 0;

	if (list_schedule_inform.next != &(list_schedule_inform)) {
		struct schedule_inform *schedule_inform;
		schedule_inform = list_entry(list_schedule_inform.next, struct schedule_inform, list);
		ntime = schedule_inform->scheduled_time;
	}

	if (!ntime || (cwmp_main.session_status.next_retry && ntime > cwmp_main.session_status.next_retry)) {
		ntime = cwmp_main.session_status.next_retry;
	}

	if (!ntime || (cwmp_main.session_status.next_periodic && ntime > cwmp_main.session_status.next_periodic)) {
		ntime = cwmp_main.session_status.next_periodic;
	}

	return ntime;
}

static int cwmp_handle_status(struct ubus_context *ctx, struct ubus_object *obj __attribute__((unused)), struct ubus_request_data *req, const char *method __attribute__((unused)), struct blob_attr *msg __attribute__((unused)))
{
	void *c;
	time_t ntime = 0;
	struct blob_buf blob_status;

	memset(&blob_status, 0, sizeof(struct blob_buf));
	blob_buf_init(&blob_status, 0);

	c = blobmsg_open_table(&blob_status, "cwmp");
	blobmsg_add_string(&blob_status, "status", "up");
	blobmsg_add_string(&blob_status, "start_time", get_time(cwmp_main.start_time));
	blobmsg_add_string(&blob_status, "acs_url", cwmp_main.conf.acsurl);
	blobmsg_close_table(&blob_status, c);

	c = blobmsg_open_table(&blob_status, "last_session");
	blobmsg_add_string(&blob_status, "status", cwmp_main.session_status.last_start_time ? arr_session_status[cwmp_main.session_status.last_status] : "N/A");
	blobmsg_add_string(&blob_status, "start_time", cwmp_main.session_status.last_start_time ? get_time(cwmp_main.session_status.last_start_time) : "N/A");
	blobmsg_add_string(&blob_status, "end_time", cwmp_main.session_status.last_end_time ? get_time(cwmp_main.session_status.last_end_time) : "N/A");
	blobmsg_close_table(&blob_status, c);

	c = blobmsg_open_table(&blob_status, "next_session");
	blobmsg_add_string(&blob_status, "status", arr_session_status[SESSION_WAITING]);
	ntime = get_session_status_next_time();
	blobmsg_add_string(&blob_status, "start_time", ntime ? get_time(ntime) : "N/A");
	blobmsg_add_string(&blob_status, "end_time", "N/A");
	blobmsg_close_table(&blob_status, c);

	c = blobmsg_open_table(&blob_status, "statistics");
	blobmsg_add_u32(&blob_status, "success_sessions", cwmp_main.session_status.success_session);
	blobmsg_add_u32(&blob_status, "failure_sessions", cwmp_main.session_status.failure_session);
	blobmsg_add_u32(&blob_status, "total_sessions", cwmp_main.session_status.success_session + cwmp_main.session_status.failure_session);
	blobmsg_close_table(&blob_status, c);

	ubus_send_reply(ctx, req, blob_status.head);
	blob_buf_free(&blob_status);

	return 0;
}

enum enum_inform
{
	INFORM_GET_RPC_METHODS,
	INFORM_EVENT,
	__INFORM_MAX
};

static const struct blobmsg_policy inform_policy[] = {
		[INFORM_GET_RPC_METHODS] = {.name = "GetRPCMethods", .type = BLOBMSG_TYPE_BOOL },
		[INFORM_EVENT] = {.name = "event", .type = BLOBMSG_TYPE_STRING },
};

static int cwmp_handle_inform(struct ubus_context *ctx, struct ubus_object *obj __attribute__((unused)), struct ubus_request_data *req, const char *method __attribute__((unused)), struct blob_attr *msg)
{
	struct blob_attr *tb[__INFORM_MAX];
	bool grm = false;
	char *event = "";
	struct blob_buf blob_inform;

	memset(&blob_inform, 0, sizeof(struct blob_buf));
	blob_buf_init(&blob_inform, 0);

	blobmsg_parse(inform_policy, ARRAY_SIZE(inform_policy), tb, blob_data(msg), blob_len(msg));

	if (tb[INFORM_GET_RPC_METHODS]) {
		grm = blobmsg_data(tb[INFORM_GET_RPC_METHODS]);
	}
	if (tb[INFORM_EVENT]) {
		event = blobmsg_data(tb[INFORM_EVENT]);
	}
	if (grm) {
		struct event_container *event_container;
		struct session *session;

		pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
		event_container = cwmp_add_event_container(&cwmp_main, EVENT_IDX_2PERIODIC, "");
		if (event_container == NULL) {
			pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
			return 0;
		}
		cwmp_save_event_container(event_container);
		session = list_entry(cwmp_main.head_event_container, struct session, head_event_container);
		if (cwmp_add_session_rpc_acs(session, RPC_ACS_GET_RPC_METHODS) == NULL) {
			pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
			return 0;
		}
		pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
		pthread_cond_signal(&(cwmp_main.threshold_session_send));
		blobmsg_add_u32(&blob_inform, "status", 1);
		blobmsg_add_string(&blob_inform, "info", "Session with GetRPCMethods will start");
	} else {
		int event_code = cwmp_get_int_event_code(event);
		pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
		cwmp_add_event_container(&cwmp_main, event_code, "");
		pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
		pthread_cond_signal(&(cwmp_main.threshold_session_send));
		if (cwmp_main.session_status.last_status == SESSION_RUNNING) {
			blobmsg_add_u32(&blob_inform, "status", -1);
			blobmsg_add_string(&blob_inform, "info", "Session already running, event will be sent at the end of the session");
		} else {
			blobmsg_add_u32(&blob_inform, "status", 1);
			blobmsg_add_string(&blob_inform, "info", "Session started");
		}
	}
	ubus_send_reply(ctx, req, blob_inform.head);
	blob_buf_free(&blob_inform);

	return 0;
}

static const struct ubus_method freecwmp_methods[] = {
	UBUS_METHOD("command", cwmp_handle_command, command_policy),
	UBUS_METHOD_NOARG("status", cwmp_handle_status),
	UBUS_METHOD("inform", cwmp_handle_inform, inform_policy),
};

static struct ubus_object_type main_object_type = UBUS_OBJECT_TYPE("freecwmpd", freecwmp_methods);

static struct ubus_object main_object = {
	.name = "tr069",
	.type = &main_object_type,
	.methods = freecwmp_methods,
	.n_methods = ARRAY_SIZE(freecwmp_methods),
};

static int check_interfaces(char *itf1, char *itf2)
{
	char itf1_buf[64] = {0};
	char itf2_buf[64] = {0};
	char *dot = NULL;

	if (itf1[0] == '\0' || itf2[0] == '\0')
		return -1;

	CWMP_STRNCPY(itf1_buf, itf1, sizeof(itf1_buf));
	CWMP_STRNCPY(itf2_buf, itf2, sizeof(itf2_buf));

	dot = strchr(itf1_buf, '.');
	if (dot)
		*dot = 0;

	dot = strchr(itf2_buf, '.');
	if (dot)
		*dot = 0;

	return strcmp(itf1_buf, itf2_buf);
}

static void cwmp_netlink_interface(struct nlmsghdr *nlh)
{
	struct ifaddrmsg *ifa = (struct ifaddrmsg *)NLMSG_DATA(nlh);
	struct rtattr *rth = IFA_RTA(ifa);
	int rtl = IFA_PAYLOAD(nlh);
	char if_name[IFNAMSIZ], if_addr[INET_ADDRSTRLEN];

	memset(&if_name, 0, sizeof(if_name));
	memset(&if_addr, 0, sizeof(if_addr));

	if (ifa->ifa_family == AF_INET) { //CASE IPv4
		while (rtl && RTA_OK(rth, rtl)) {
			if (rth->rta_type != IFA_LOCAL) {
				rth = RTA_NEXT(rth, rtl);
				continue;
			}

			uint32_t addr = htonl(*(uint32_t *)RTA_DATA(rth));
			if (htonl(13) == 13) {
				// running on big endian system
			} else {
				// running on little endian system
				addr = __builtin_bswap32(addr);
			}

			if_indextoname(ifa->ifa_index, if_name);
			if (check_interfaces(cwmp_main.conf.interface, if_name)) {
				rth = RTA_NEXT(rth, rtl);
				continue;
			}

			inet_ntop(AF_INET, &(addr), if_addr, INET_ADDRSTRLEN);

			FREE(cwmp_main.conf.ip);
			cwmp_main.conf.ip = strdup(if_addr);
			cwmp_uci_set_varstate_value("cwmp", "cpe", "ip", cwmp_main.conf.ip);
			cwmp_commit_package("cwmp", UCI_VARSTATE_CONFIG);
			connection_request_ip_value_change(&cwmp_main, IPv4);
			break;
		}
	} else { //CASE IPv6
		while (rtl && RTA_OK(rth, rtl)) {
			char pradd_v6[128];
			if (rth->rta_type != IFA_ADDRESS || ifa->ifa_scope == RT_SCOPE_LINK) {
				rth = RTA_NEXT(rth, rtl);
				continue;
			}

			if_indextoname(ifa->ifa_index, if_name);
			if (check_interfaces(cwmp_main.conf.interface, if_name)) {
				rth = RTA_NEXT(rth, rtl);
				continue;
			}

			inet_ntop(AF_INET6, RTA_DATA(rth), pradd_v6, sizeof(pradd_v6));

			FREE(cwmp_main.conf.ipv6);
			cwmp_main.conf.ipv6 = strdup(pradd_v6);
			cwmp_uci_set_varstate_value("cwmp", "cpe", "ipv6", cwmp_main.conf.ip);
			cwmp_commit_package("cwmp", UCI_VARSTATE_CONFIG);
			connection_request_ip_value_change(&cwmp_main, IPv6);
			break;
		}
	}
}

static void netlink_new_msg(struct uloop_fd *ufd, unsigned events __attribute__((unused)))
{
	struct nlmsghdr *nlh;
	char buffer[BUFSIZ];
	size_t msg_size;

	memset(&buffer, 0, sizeof(buffer));

	nlh = (struct nlmsghdr *)buffer;
	if ((int)(msg_size = recv(ufd->fd, nlh, BUFSIZ, 0)) == -1) {
		CWMP_LOG(ERROR, "error receiving netlink message");
		return;
	}

	while ((size_t)msg_size > sizeof(*nlh)) {
		int len = nlh->nlmsg_len;
		int req_len = len - sizeof(*nlh);

		if (req_len < 0 || (size_t)len > msg_size) {
			CWMP_LOG(ERROR, "error reading netlink message");
			return;
		}

		if (!NLMSG_OK(nlh, msg_size)) {
			CWMP_LOG(ERROR, "netlink message is not NLMSG_OK");
			return;
		}

		if (nlh->nlmsg_type == RTM_NEWADDR)
			cwmp_netlink_interface(nlh);

		msg_size -= NLMSG_ALIGN(len);
		nlh = (struct nlmsghdr *)((char *)nlh + NLMSG_ALIGN(len));
	}
}

static struct uloop_fd netlink_event[2] = {{.cb = netlink_new_msg }, {.cb = netlink_new_msg }};

static int netlink_init(bool is_ipv6)
{
	struct {
		struct nlmsghdr hdr;
		struct ifaddrmsg msg;
	} req;
	struct sockaddr_nl addr;
	int sock[2];

	memset(&addr, 0, sizeof(addr));
	memset(&req, 0, sizeof(req));

	if ((sock[0] = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1) {
		CWMP_LOG(ERROR, "couldn't open NETLINK_ROUTE socket");
		return -1;
	}

	addr.nl_family = AF_NETLINK;
	addr.nl_groups = is_ipv6 ? RTMGRP_IPV6_IFADDR : RTMGRP_IPV4_IFADDR;

	if ((bind(sock[0], (struct sockaddr *)&addr, sizeof(addr))) == -1) {
		CWMP_LOG(ERROR, "couldn't bind netlink socket");
		return -1;
	}

	netlink_event[is_ipv6 ? 1 : 0].fd = sock[0];
	uloop_fd_add(&netlink_event[is_ipv6 ? 1 : 0], ULOOP_READ | ULOOP_EDGE_TRIGGER);

	if ((sock[1] = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) == -1) {
		CWMP_LOG(ERROR, "couldn't open NETLINK_ROUTE socket");
		return -1;
	}

	req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
	req.hdr.nlmsg_type = RTM_GETADDR;
	req.msg.ifa_family = is_ipv6 ? AF_INET6 : AF_INET;

	if ((send(sock[1], &req, req.hdr.nlmsg_len, 0)) == -1) {
		CWMP_LOG(ERROR, "couldn't send netlink socket");
		return -1;
	}

	struct uloop_fd dummy_event = {.fd = sock[1] };
	netlink_new_msg(&dummy_event, 0);

	return 0;
}

int cwmp_ubus_init(struct cwmp *cwmp)
{
	uloop_init();

	if (netlink_init(false)) {
		CWMP_LOG(ERROR, "netlink initialization failed");
	}

	if (cwmp->conf.ipv6_enable) {
		if (netlink_init(true)) {
			CWMP_LOG(ERROR, "netlink initialization failed");
		}
	}

	ctx = ubus_connect(cwmp->conf.ubus_socket);
	if (!ctx)
		return -1;

	ubus_add_uloop(ctx);

	if (ubus_add_object(ctx, &main_object))
		return -1;

	uloop_run();
	uloop_done();
	return 0;
}

void cwmp_ubus_exit(void)
{
	if (ctx) {
		ubus_remove_object(ctx, &main_object);
		ubus_free(ctx);
	}
}

int cwmp_ubus_call(const char *obj, const char *method, const struct cwmp_ubus_arg u_args[], int u_args_size, void (*ubus_callback)(struct ubus_request *req, int type, struct blob_attr *msg), void *callback_arg)
{
	uint32_t id;
	int i = 0;
	int rc = 0;
	struct blob_buf b = { 0 };

	struct ubus_context *ubus_ctx = NULL;

	ubus_ctx = ubus_connect(NULL);
	if (ubus_ctx == NULL)
		return -1;

	memset(&b, 0, sizeof(struct blob_buf));
	blob_buf_init(&b, 0);
	for (i = 0; i < u_args_size; i++) {
		if (u_args[i].type == UBUS_String)
			blobmsg_add_string(&b, u_args[i].key, u_args[i].val.str_val);
		else if (u_args[i].type == UBUS_Integer) {
			blobmsg_add_u32(&b, u_args[i].key, u_args[i].val.int_val);
		} else if (u_args[i].type == UBUS_Array_Obj || u_args[i].type == UBUS_Array_Str) {
			void *a;
			int j;
			a = blobmsg_open_array(&b, u_args[i].key);
			if (u_args[i].type == UBUS_Array_Obj) {
				void *t;
				t = blobmsg_open_table(&b, "");
				for (j = 0; j < ARRAY_MAX; j++) {
					if (u_args[i].val.array_value[j].param_value.key == NULL || strlen(u_args[i].val.array_value[j].param_value.key) == 0)
						break;
					blobmsg_add_string(&b, u_args[i].val.array_value[j].param_value.key, u_args[i].val.array_value[j].param_value.value);
				}
				blobmsg_close_table(&b, t);
			}
			if (u_args[i].type == UBUS_Array_Str) {
				for (j = 0; j < ARRAY_MAX; j++) {
					if (u_args[i].val.array_value[j].str_value == NULL || strlen(u_args[i].val.array_value[j].str_value) == 0)
						break;
					blobmsg_add_string(&b, NULL, u_args[i].val.array_value[j].str_value);
				}
			}
			blobmsg_close_array(&b, a);
		} else if (u_args[i].type == UBUS_List_Param_Set) {
			struct cwmp_dm_parameter *param_value;
			void *a;
			a = blobmsg_open_array(&b, u_args[i].key);
			list_for_each_entry (param_value, u_args[i].val.param_value_list, list) {
				void *t;
				if (!param_value->name)
					break;
				t = blobmsg_open_table(&b, "");
				blobmsg_add_string(&b, "path", param_value->name);
				blobmsg_add_string(&b, "value", param_value->value);
				blobmsg_close_table(&b, t);
			}
			blobmsg_close_array(&b, a);
		} else if (u_args[i].type == UBUS_List_Param_Get) {
			struct cwmp_dm_parameter *param_value;
			void *a;
			a = blobmsg_open_array(&b, u_args[i].key);
			list_for_each_entry (param_value, u_args[i].val.param_value_list, list) {
				if (!param_value->name)
					break;
				blobmsg_add_string(&b, NULL, param_value->name);
			}
			blobmsg_close_array(&b, a);
		} else if (u_args[i].type == UBUS_Obj_Obj) {
			struct cwmp_dm_parameter *param_value;
			json_object *input_json_obj = json_object_new_object();
			list_for_each_entry (param_value, u_args[i].val.param_value_list, list) {
				if (!param_value->name)
					break;
				json_object_object_add(input_json_obj, param_value->name, json_object_new_string(param_value->value));
			}
			blobmsg_add_json_element(&b, u_args[i].key, input_json_obj);
		} else if (u_args[i].type == UBUS_Bool)
			blobmsg_add_u8(&b, u_args[i].key, u_args[i].val.bool_val);
	}
	if (!ubus_lookup_id(ubus_ctx, obj, &id))
		rc = ubus_invoke(ubus_ctx, id, method, b.head, ubus_callback, callback_arg, 20000);
	else
		rc = -1;
	blob_buf_free(&b);
	if (ubus_ctx) {
		ubus_free(ubus_ctx);
		ubus_ctx = NULL;
	}
	return rc;
}
