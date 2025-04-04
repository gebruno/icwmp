/*
 * cwmp.c - icwmp Main file
 *
 * Copyright (C) 2021-2022, IOPSYS Software Solutions AB.
 *
 *	  Author Mohamed Kallel <mohamed.kallel@pivasoftware.com>
 *	  Author Ahmed Zribi <ahmed.zribi@pivasoftware.com>
 *	  Author Omar Kallel <omar.kallel@pivasoftware.com>
 *
 * See LICENSE file for license related information.
 *
 */

#include <math.h>
#include <stdlib.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/file.h>
#include <sys/socket.h>

#include "common.h"
#include "ssl_utils.h"
#include "xml.h"
#include "notifications.h"
#include "event.h"
#include "uci_utils.h"
#include "log.h"
#include "session.h"
#include "diagnostic.h"
#include "http.h"
#include "rpc.h"
#include "config.h"
#include "backupSession.h"
#include "ubus_utils.h"
#include "digauth.h"
#include "upload.h"
#include "download.h"
#include "sched_inform.h"
#include "datamodel_interface.h"
#include "cwmp_du_state.h"
#include "heartbeat.h"
#include "cwmp_http.h"

struct list_head intf_reset_list;
struct list_head du_uuid_list;
struct list_head force_inform_list;

static bool interface_reset_req(char *param_name, char *value)
{
	if (param_name == NULL || value == NULL)
		return false;

	char reg_exp[100] = {0};
	snprintf(reg_exp, sizeof(reg_exp), "^(%s|%s)[0-9]+\\.Reset$", DM_IP_INTERFACE_PATH, DM_PPP_INTERFACE_PATH);

	if (match_reg_exp(reg_exp, param_name) == false)
		return false;

	if (CWMP_STRCMP(value, "1") != 0 && CWMP_STRCMP(value, "true") != 0)
		return false;

	return true;
}

void set_interface_reset_request(char *param_name, char *value)
{
	char *inst_path = NULL;

	if (param_name == NULL || value == NULL)
		return;

	if (CWMP_OK != instantiate_param_name(param_name, &inst_path))
		return;

	if (!CWMP_STRLEN(inst_path))
		return;

	if (interface_reset_req(inst_path, value) == false) {
		FREE(inst_path);
		return;
	}

	// Store the interface path to handle after session end
	int len = 0;
	char *pos = strrchr(inst_path, '.');
	if (pos == NULL) {
		FREE(inst_path);
		return;
	}

	len = pos - inst_path + 2;
	if (len <= 0) {
		FREE(inst_path);
		return;
	}

	intf_reset_node *node = (intf_reset_node *)malloc(sizeof(intf_reset_node));
	if (node == NULL) {
		CWMP_LOG(ERROR, "Out of memory");
		FREE(inst_path);
		return;
	}

	CWMP_MEMSET(node, 0, sizeof(intf_reset_node));
	snprintf(node->path, len, "%s", inst_path);
	INIT_LIST_HEAD(&node->list);
	list_add_tail(&node->list, &intf_reset_list);
	FREE(inst_path);
}

static int create_cwmp_temporary_files(void)
{
	if (!file_exists(VARSTATE_CONFIG"/icwmp")) {
		creat(VARSTATE_CONFIG"/icwmp", S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	}

	set_uci_path_value(VARSTATE_CONFIG, "icwmp.acs", "acs");
	set_uci_path_value(VARSTATE_CONFIG, "icwmp.cpe", "cpe");

	if (!file_exists(CWMP_NOTIFICATIONS_PACKAGE)) {
		creat(CWMP_NOTIFICATIONS_PACKAGE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	}

	set_uci_path_value("/etc/icwmpd", "cwmp_notifications.notifications", "notifications");

	if (!folder_exists("/var/run/icwmpd")) {
		if (mkdir("/var/run/icwmpd", S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
			CWMP_LOG(INFO, "Not able to create the folder /var/run/icwmpd");
			return CWMP_GEN_ERR;
		}
	}

	return CWMP_OK;
}

static void wait_for_time_sync(void)
{
	struct cwmp_dm_parameter cwmp_dm_param = {0};

	int loop_count = (cwmp_ctx.conf.clock_sync_timeout / 2);

	if (loop_count == 0) {
		CWMP_LOG(INFO, "Wait for time sync is disabled, cwmp.cpe.clock_sync_timeout=%d", cwmp_ctx.conf.clock_sync_timeout);
		return;
	}

	while (loop_count) {
		memset(&cwmp_dm_param, 0, sizeof(struct cwmp_dm_parameter));

		if (!cwmp_get_parameter_value("Device.Time.Status", &cwmp_dm_param)) {
			CWMP_LOG(ERROR, "Failed to get Device.Time.Status ...");
			return;
		}

		if (CWMP_STRCMP(cwmp_dm_param.value, "Disabled") == 0) {
			CWMP_LOG(INFO, "Time.Status is Disabled, no need to wait");
			return;
		}

		if (CWMP_STRCMP(cwmp_dm_param.value, "Synchronized") != 0) {
			loop_count--;
			CWMP_LOG(INFO, "Clock status [%s], checking again %d", cwmp_dm_param.value, loop_count);
			sleep(2);
			continue;
		}

		CWMP_LOG(INFO, "Clock is synchronized, ready to send inform");
		return;
	}

	CWMP_LOG(ERROR, "Clock not Synchronized in %d sec, last status %s", cwmp_ctx.conf.clock_sync_timeout, cwmp_dm_param.value);
	return;
}

static int cwmp_init(void)
{
	openlog("cwmp", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

	cwmp_ctx.curr_delay_reboot = -1;
	cwmp_ctx.curr_schedule_reboot = 0;

	get_preinit_config();

	CWMP_LOG(INFO, "STARTING ICWMP with PID :%d", getpid());

	/* Only One instance should run*/
	// cppcheck-suppress cert-MSC24-C
	cwmp_ctx.pid_file = fopen("/var/run/icwmpd.pid", "w+");
	fcntl(fileno(cwmp_ctx.pid_file), F_SETFD, fcntl(fileno(cwmp_ctx.pid_file), F_GETFD) | FD_CLOEXEC);
	int rc = flock(fileno(cwmp_ctx.pid_file), LOCK_EX | LOCK_NB);
	if (rc) {
		if (EWOULDBLOCK != errno) {
			const char *piderr = "PID file creation failed: Quit the daemon!";
			fprintf(stderr, "%s\n", piderr);
			CWMP_LOG(ERROR, "%s", piderr);
			exit(EXIT_FAILURE);
		} else
			exit(EXIT_SUCCESS);
	}

	if (cwmp_ctx.pid_file)
		fclose(cwmp_ctx.pid_file);

	CWMP_LOG(DEBUG, "Loading icwmpd configuration");
	cwmp_config_load();
	CWMP_LOG(DEBUG, "Successfully load icwmpd configuration");

	wait_for_time_sync();

	cwmp_ctx.prev_periodic_enable = cwmp_ctx.conf.periodic_enable;
	cwmp_ctx.prev_periodic_interval = cwmp_ctx.conf.period;
	cwmp_ctx.prev_periodic_time = cwmp_ctx.conf.time;
	cwmp_ctx.prev_heartbeat_enable = cwmp_ctx.conf.heart_beat_enable;
	cwmp_ctx.prev_heartbeat_interval = cwmp_ctx.conf.heartbeat_interval;
	cwmp_ctx.prev_heartbeat_time = cwmp_ctx.conf.heart_time;

	if (cwmp_stop == true)
		return CWMP_GEN_ERR;

	cwmp_get_deviceid();

	/* Load default force inform parameters */
	CWMP_MEMSET(&force_inform_list, 0, sizeof(struct list_head));
	INIT_LIST_HEAD(&force_inform_list);
	load_default_forced_inform();

	/* Load custom notify and force inform parameters */
	load_forced_inform_json();
	load_custom_notify_json();
	set_default_forced_active_parameters_notifications();
	init_list_param_notify();

	create_cwmp_session_structure();
	get_nonce_key();

	CWMP_MEMSET(&intf_reset_list, 0, sizeof(struct list_head));
	INIT_LIST_HEAD(&intf_reset_list);

	CWMP_MEMSET(&du_uuid_list, 0, sizeof(struct list_head));
	INIT_LIST_HEAD(&du_uuid_list);

	cwmp_ctx.start_time = time(NULL);

	return CWMP_OK;
}

static void cwmp_free()
{
	http_server_stop();
	FREE(nonce_key);
	clean_list_param_notify();
	bkp_tree_clean();
	icwmp_uloop_ubus_exit();
	icwmp_cleanmem();
	rpc_exit();
	clean_cwmp_session_structure();
	icwmp_free_critical_services();
	CWMP_LOG(INFO, "EXIT ICWMP");
	closelog();
}

void cwmp_exit()
{
	cwmp_stop = true;

	if (cwmp_ctx.session->session_status.last_status == SESSION_RUNNING)
		http_set_timeout();

	uloop_timeout_cancel(&retry_session_timer);
	uloop_timeout_cancel(&periodic_session_timer);
	uloop_timeout_cancel(&session_timer);
	uloop_timeout_cancel(&heartbeat_session_timer);
	clean_autonomous_complpolicy();
	clean_interface_update();
	clean_du_uuid_list();
	clean_force_inform_list();
	FREE(cwmp_ctx.ev);
	FREE(cwmp_ctx.intf_ev);
	uloop_end();
	shutdown(cwmp_ctx.cr_socket_desc, SHUT_RDWR);
	FREE(global_session_event);

	/* Free all memory allocation */
	cwmp_free();
}

int main(int argc, char **argv)
{
	int error = 0;

	CWMP_MEMSET(&cwmp_ctx, 0, sizeof(struct cwmp));

	error = icwmp_connect_ubus();
	if (error)
		return error;

	error = wait_for_bbf_object();
	if (error) {
		icwmp_free_ubus();
		return error;
	}

	if ((error = global_env_init(argc, argv, &(cwmp_ctx.env)))) {
		icwmp_free_ubus();
		return error;
	}

	if ((error = create_cwmp_temporary_files())) {
		icwmp_free_ubus();
		return error;
	}

	if ((error = cwmp_init())) {
		icwmp_free_ubus();
		return error;
	}

	if ((error = cwmp_init_backup_session(NULL, ALL))) {
		icwmp_free_ubus();
		return error;
	}

	if ((error = cwmp_root_cause_events())) {
		icwmp_free_ubus();
		return error;
	}

	icwmp_http_server_init();

	uloop_init();

	if ((error = icwmp_uloop_ubus_register())) {
		icwmp_free_ubus();
		return error;
	}

	if ((error = initiate_autonomous_complpolicy())) {
		icwmp_uloop_ubus_exit();
		return error;
	}

	if ((error = initiate_interface_update())) {
		icwmp_uloop_ubus_exit();
		return error;
	}

	trigger_cwmp_session_timer();

	intiate_heartbeat_procedures();

	initiate_cwmp_periodic_session_feature();

	http_server_start();

	apply_allowed_cr_ip_port();
	cwmp_ctx.conf.cr_ip_port_change = false;

	uloop_run();
	uloop_done();

	cwmp_exit();

	return CWMP_OK;
}
