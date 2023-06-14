/*
 * config.c - load/store icwmp application configuration
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

#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "config.h"
#include "log.h"
#include "reboot.h"
#include "ubus_utils.h"
#include "ssl_utils.h"
#include "datamodel_interface.h"
#include "heartbeat.h"
#include "cwmp_http.h"

pthread_mutex_t mutex_config_load = PTHREAD_MUTEX_INITIALIZER;

static char* get_value_from_uci_option(struct uci_option *tb) {
	if (tb == NULL)
		return NULL;

	if (tb->type == UCI_TYPE_STRING) {
		return tb->v.string;
	}

	return NULL;
}

static void config_get_cpe_elements(struct uci_section *s)
{
	enum {
		UCI_CPE_UBUS_SOCKET_PATH,
		UCI_CPE_LOG_FILE_NAME,
		UCI_CPE_LOG_MAX_SIZE,
		UCI_CPE_ENABLE_STDOUT_LOG,
		UCI_CPE_ENABLE_FILE_LOG,
		UCI_LOG_SEVERITY_PATH,
		UCI_CPE_ENABLE_SYSLOG,
		UCI_CPE_AMD_VERSION,
		UCI_CPE_DEFAULT_WAN_IFACE,
		UCI_CPE_CON_REQ_TIMEOUT,
		__MAX_NUM_UCI_CPE_ATTRS,
	};

	const struct uci_parse_option cpe_opts[] = {
		{ .name = "ubus_socket", .type = UCI_TYPE_STRING },
		{ .name = "log_file_name", .type = UCI_TYPE_STRING },
		{ .name = "log_max_size", .type = UCI_TYPE_STRING },
		{ .name = "log_to_console", .type = UCI_TYPE_STRING },
		{ .name = "log_to_file", .type = UCI_TYPE_STRING },
		{ .name = "log_severity", .type = UCI_TYPE_STRING },
		{ .name = "log_to_syslog", .type = UCI_TYPE_STRING },
		{ .name = "amd_version", .type = UCI_TYPE_STRING },
		{ .name = "default_wan_interface", .type = UCI_TYPE_STRING },
		{ .name = "cr_timeout", .type = UCI_TYPE_STRING }
	};

	struct uci_option *cpe_tb[__MAX_NUM_UCI_CPE_ATTRS] = {0};
	uci_parse_section(s, cpe_opts, __MAX_NUM_UCI_CPE_ATTRS, cpe_tb);

	cwmp_main->conf.ubus_socket = CWMP_STRDUP(get_value_from_uci_option(cpe_tb[UCI_CPE_UBUS_SOCKET_PATH]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - ubus socket: %s", cwmp_main->conf.ubus_socket ? cwmp_main->conf.ubus_socket : "");

	log_set_log_file_name(get_value_from_uci_option(cpe_tb[UCI_CPE_LOG_FILE_NAME]));

	log_set_file_max_size(get_value_from_uci_option(cpe_tb[UCI_CPE_LOG_MAX_SIZE]));

	log_set_on_console(get_value_from_uci_option(cpe_tb[UCI_CPE_ENABLE_STDOUT_LOG]));

	log_set_on_file(get_value_from_uci_option(cpe_tb[UCI_CPE_ENABLE_FILE_LOG]));

	log_set_severity_idx(get_value_from_uci_option(cpe_tb[UCI_LOG_SEVERITY_PATH]));

	log_set_on_syslog(get_value_from_uci_option(cpe_tb[UCI_CPE_ENABLE_SYSLOG]));

	cwmp_main->conf.amd_version = DEFAULT_AMD_VERSION;
	char *version = get_value_from_uci_option(cpe_tb[UCI_CPE_AMD_VERSION]);
	if (version != NULL) {
		int a = atoi(version);
		if (a >= 1 && a <= 6) {
			cwmp_main->conf.amd_version = a;
		}
	}
	cwmp_main->conf.supported_amd_version = cwmp_main->conf.amd_version;
	CWMP_LOG(DEBUG, "CWMP CONFIG - amendement version: %d", cwmp_main->conf.amd_version);

	if (cpe_tb[UCI_CPE_DEFAULT_WAN_IFACE])
		cwmp_main->conf.default_wan_iface = strdup(get_value_from_uci_option(cpe_tb[UCI_CPE_DEFAULT_WAN_IFACE]));
	else
		cwmp_main->conf.default_wan_iface = strdup("wan");
	CWMP_LOG(DEBUG, "CWMP CONFIG - default wan interface: %s", cwmp_main->conf.default_wan_iface);

	cwmp_main->conf.cr_timeout = DEFAULT_CR_TIMEOUT;
	char *tm_out = get_value_from_uci_option(cpe_tb[UCI_CPE_CON_REQ_TIMEOUT]);
	if (tm_out != NULL) {
		int a = strtod(tm_out, NULL);
		if (a > 0) {
			cwmp_main->conf.cr_timeout = a;
		}
	}
	CWMP_LOG(DEBUG, "CWMP CONFIG - connection req timeout: %d", cwmp_main->conf.cr_timeout);
}

static void config_get_acs_elements(struct uci_section *s)
{
	enum {
		UCI_ACS_SSL_CAPATH,
		HTTP_DISABLE_100CONTINUE,
		UCI_ACS_INSECURE_ENABLE,
		__MAX_NUM_UCI_ACS_ATTRS,
	};

	const struct uci_parse_option acs_opts[] = {
		{ .name = "ssl_capath", .type = UCI_TYPE_STRING },
		{ .name = "http_disable_100continue", .type = UCI_TYPE_STRING },
		{ .name = "insecure_enable", .type = UCI_TYPE_STRING },
	};

	struct uci_option *acs_tb[__MAX_NUM_UCI_ACS_ATTRS];
	memset(acs_tb, 0, sizeof(acs_tb));
	uci_parse_section(s, acs_opts, __MAX_NUM_UCI_ACS_ATTRS, acs_tb);

	cwmp_main->conf.acs_ssl_capath = CWMP_STRDUP(get_value_from_uci_option(acs_tb[UCI_ACS_SSL_CAPATH]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs ssl cpath: %s", cwmp_main->conf.acs_ssl_capath ? cwmp_main->conf.acs_ssl_capath : "");

	cwmp_main->conf.http_disable_100continue = uci_str_to_bool(get_value_from_uci_option(acs_tb[HTTP_DISABLE_100CONTINUE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - http disable 100continue: %d", cwmp_main->conf.http_disable_100continue);

	cwmp_main->conf.insecure_enable = uci_str_to_bool(get_value_from_uci_option(acs_tb[UCI_ACS_INSECURE_ENABLE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs insecure enable: %d", cwmp_main->conf.insecure_enable);
}

int get_preinit_config()
{
	struct uci_context *ctx;
	struct uci_package *pkg;
	struct uci_element *e;

	ctx = uci_alloc_context();
	if (!ctx)
		return CWMP_GEN_ERR;

	if (uci_load(ctx, "cwmp", &pkg)) {
		uci_free_context(ctx);
		return CWMP_GEN_ERR;
	}

	uci_foreach_element(&pkg->sections, e) {
		struct uci_section *s = uci_to_section(e);
		if (s== NULL || s->type == NULL)
			continue;
		if (strcmp(s->type, "acs") == 0) {
			config_get_acs_elements(s);
		} else if (strcmp(s->type, "cpe") == 0) {
			config_get_cpe_elements(s);
		}
	}

	uci_free_context(ctx);
	return CWMP_OK;
}

static char* get_alternate_option_value(bool discovery_enable, char *acs_val, char *dhcp_val)
{
	if ((discovery_enable == true || CWMP_STRLEN(acs_val) == 0) && (CWMP_STRLEN(dhcp_val) != 0)) {
		return dhcp_val;
	} else if (CWMP_STRLEN(acs_val) != 0) {
		return acs_val;
	}

	return NULL;
}

int get_global_config()
{
	int error;
	char *value = NULL, *value2 = NULL, *value3 = NULL;

	if ((error = uci_get_value(UCI_CPE_CWMP_ENABLE, &value)) == CWMP_OK) {
		if (value != NULL && uci_str_to_bool(value) == false) {
			FREE(value);
			CWMP_LOG(ERROR, "CWMP service is disabled");
			exit(0);
		}
	}
	FREE(value);

	bool discovery_enable = false;
	error = uci_get_value(UCI_DHCP_DISCOVERY_PATH, &value);
	if (error == CWMP_OK && value != NULL) {
		discovery_enable = uci_str_to_bool(value);
	}
	FREE(value);

	uci_get_value(UCI_ACS_URL_PATH, &value2);
	uci_get_value(UCI_DHCP_ACS_URL, &value3);

	FREE(cwmp_main->conf.acsurl);
	cwmp_main->conf.acsurl = CWMP_STRDUP(get_alternate_option_value(discovery_enable, value2, value3));

	FREE(value2);
	FREE(value3);

	if (cwmp_main->conf.acsurl == NULL) {
		return CWMP_GEN_ERR;
	}

	if ((error = uci_get_value(UCI_ACS_USERID_PATH, &value)) == CWMP_OK) {
		if (value != NULL) {
			FREE(cwmp_main->conf.acs_userid);
			cwmp_main->conf.acs_userid = strdup(value);
			FREE(value);
		}
	} else {
		CWMP_LOG(INFO, "Failed to get userid");
		return error;
	}

	if ((error = uci_get_value(UCI_ACS_PASSWD_PATH, &value)) == CWMP_OK) {
		if (value != NULL) {
			FREE(cwmp_main->conf.acs_passwd);
			cwmp_main->conf.acs_passwd = strdup(value);
			FREE(value);
		}
	} else {
		CWMP_LOG(INFO, "Failed to get acs password");
		return error;
	}

	if (uci_get_value(UCI_ACS_COMPRESSION, &value) == CWMP_OK) {
		cwmp_main->conf.compression = COMP_NONE;
		if (cwmp_main->conf.amd_version >= AMD_5 && value != NULL) {
			if (0 == strcasecmp(value, "gzip")) {
				cwmp_main->conf.compression = COMP_GZIP;
			} else if (0 == strcasecmp(value, "deflate")) {
				cwmp_main->conf.compression = COMP_DEFLATE;
			} else {
				cwmp_main->conf.compression = COMP_NONE;
			}
		}
		FREE(value);
	} else {
		cwmp_main->conf.compression = COMP_NONE;
	}

	cwmp_main->conf.retry_min_wait_interval = DEFAULT_RETRY_MINIMUM_WAIT_INTERVAL;
	uci_get_value(UCI_ACS_RETRY_MIN_WAIT_INTERVAL, &value2);
	uci_get_value(UCI_DHCP_ACS_RETRY_MIN_WAIT_INTERVAL, &value3);

	char *op_interval = get_alternate_option_value(discovery_enable, value2, value3);
	if (op_interval != NULL) {
		if (cwmp_main->conf.amd_version >= AMD_3) {
			int a = atoi(op_interval);
			if (a <= 65535 && a >= 1) {
				cwmp_main->conf.retry_min_wait_interval = a;
			}
		}
	}

	FREE(value2);
	FREE(value3);

	cwmp_main->conf.retry_interval_multiplier = DEFAULT_RETRY_INTERVAL_MULTIPLIER;
	uci_get_value(UCI_ACS_RETRY_INTERVAL_MULTIPLIER, &value2);
	uci_get_value(UCI_DHCP_ACS_RETRY_INTERVAL_MULTIPLIER, &value3);

	char *op_multi = get_alternate_option_value(discovery_enable, value2, value3);
	if (op_multi != NULL) {
		if (cwmp_main->conf.amd_version >= AMD_3) {
			int a = atoi(op_multi);
			if (a <= 65535 && a >= 1000) {
				cwmp_main->conf.retry_interval_multiplier = a;
			}
		}
	}

	FREE(value2);
	FREE(value3);

	if (uci_get_value(UCI_ACS_GETRPC, &value) == CWMP_OK) {
		cwmp_main->conf.acs_getrpc = uci_str_to_bool(value);
		FREE(value);
	} else {
		cwmp_main->conf.acs_getrpc = true;
	}

	FREE(cwmp_main->conf.cpe_userid);
	if (uci_get_value(UCI_CPE_USERID_PATH, &value) == CWMP_OK) {
		cwmp_main->conf.cpe_userid = strdup(value);
		FREE(value);
	} else {
		cwmp_main->conf.cpe_userid = strdup("");
	}

	FREE(cwmp_main->conf.cpe_passwd);
	if (uci_get_value(UCI_CPE_PASSWD_PATH, &value) == CWMP_OK) {
		cwmp_main->conf.cpe_passwd = strdup(value);
		FREE(value);
	} else {
		cwmp_main->conf.cpe_passwd = strdup("");
	}

	if (uci_get_value(UCI_CPE_PORT_PATH, &value) == CWMP_OK) {
		int a = 0;

		if (value != NULL) {
			a = atoi(value);
			FREE(value);
		}

		if (a == 0) {
			CWMP_LOG(DEBUG, "Set the connection request port to the default value: %d", DEFAULT_CONNECTION_REQUEST_PORT);
			cwmp_main->conf.connection_request_port = DEFAULT_CONNECTION_REQUEST_PORT;
		} else {
			cwmp_main->conf.connection_request_port = a;
		}
	} else {
		CWMP_LOG(DEBUG, "Failed to read cpe port, defaults to: %d", DEFAULT_CONNECTION_REQUEST_PORT);
		cwmp_main->conf.connection_request_port = DEFAULT_CONNECTION_REQUEST_PORT;
	}

	FREE(cwmp_main->conf.connection_request_path);
	if (uci_get_value(UCI_CPE_CRPATH_PATH, &value) == CWMP_OK) {
		if (value[0] == '/')
			cwmp_main->conf.connection_request_path = strdup(value);
		else {
			char cr_path[512];
			snprintf(cr_path, sizeof(cr_path), "/%s", value);
			cwmp_main->conf.connection_request_path = strdup(cr_path);
		}
		FREE(value);
	} else {
		cwmp_main->conf.connection_request_path = strdup("/");
	}

	if (uci_get_value(UCI_CPE_NOTIFY_PERIODIC_ENABLE, &value) == CWMP_OK) {
		cwmp_main->conf.periodic_notify_enable = uci_str_to_bool(value);
		FREE(value);
	} else {
		cwmp_main->conf.periodic_notify_enable = true;
	}

	if (uci_get_value(UCI_CPE_NOTIFY_PERIOD, &value) == CWMP_OK) {
		int a = 0;

		if (value != NULL) {
			a = atoi(value);
			FREE(value);
		}

		if (a == 0) {
			CWMP_LOG(DEBUG, "Set notify period to the default value: %d", DEFAULT_NOTIFY_PERIOD);
			cwmp_main->conf.periodic_notify_interval = DEFAULT_NOTIFY_PERIOD;
		} else {
			cwmp_main->conf.periodic_notify_interval = a;
		}
	} else {
		CWMP_LOG(DEBUG, "Failed to read notify period, default value: %d", DEFAULT_NOTIFY_PERIOD);
		cwmp_main->conf.periodic_notify_interval = DEFAULT_NOTIFY_PERIOD;
	}

	if (uci_get_value(UCI_PERIODIC_INFORM_TIME_PATH, &value) == CWMP_OK) {
		cwmp_main->conf.time = convert_datetime_to_timestamp(value);
		FREE(value);
	} else {
		cwmp_main->conf.time = 0;
	}

	char *entropy = generate_random_string(sizeof(unsigned int));
	if (entropy != NULL) {
		cwmp_main->conf.periodic_entropy = (unsigned int)strtoul(entropy, NULL, 16);
		free(entropy);
	}

	if (uci_get_value(UCI_PERIODIC_INFORM_INTERVAL_PATH, &value) == CWMP_OK) {
		int a = 0;

		if (value != NULL) {
			a = atoi(value);
			FREE(value);
		}

		if (a >= PERIOD_INFORM_MIN) {
			cwmp_main->conf.period = a;
		} else {
			CWMP_LOG(DEBUG, "Period interval of periodic inform should be > %ds. Set to default: %ds", PERIOD_INFORM_MIN, PERIOD_INFORM_DEFAULT);
			cwmp_main->conf.period = PERIOD_INFORM_DEFAULT;
		}
	} else {
		CWMP_LOG(DEBUG, "Failed to read period interval, default: %ds", PERIOD_INFORM_DEFAULT);
		cwmp_main->conf.period = PERIOD_INFORM_DEFAULT;
	}

	if (uci_get_value(UCI_PERIODIC_INFORM_ENABLE_PATH, &value) == CWMP_OK) {
		cwmp_main->conf.periodic_enable = uci_str_to_bool(value);
		FREE(value);
	} else {
		cwmp_main->conf.periodic_enable = true;
	}

	if (uci_get_value(UCI_CPE_INSTANCE_MODE, &value) == CWMP_OK) {
		if (0 == strcmp(value, "InstanceNumber")) {
			cwmp_main->conf.instance_mode = INSTANCE_MODE_NUMBER;
		} else {
			cwmp_main->conf.instance_mode = INSTANCE_MODE_ALIAS;
		}
		FREE(value);
	} else {
		cwmp_main->conf.instance_mode = DEFAULT_INSTANCE_MODE;
	}

	if (uci_get_value(UCI_CPE_SESSION_TIMEOUT, &value) == CWMP_OK) {
		cwmp_main->conf.session_timeout = DEFAULT_SESSION_TIMEOUT;
		if (value != NULL) {
			int a = atoi(value);
			if (a >= 1) {
				cwmp_main->conf.session_timeout = a;
			}
			FREE(value);
		}
	} else {
		cwmp_main->conf.session_timeout = DEFAULT_SESSION_TIMEOUT;
	}

	if (uci_get_value(LW_NOTIFICATION_ENABLE, &value) == CWMP_OK) {
		cwmp_main->conf.lw_notification_enable = uci_str_to_bool(value);
		FREE(value);
	} else {
		cwmp_main->conf.lw_notification_enable = false;
	}

	FREE(cwmp_main->conf.lw_notification_hostname);
	if (uci_get_value(LW_NOTIFICATION_HOSTNAME, &value) == CWMP_OK) {
		if (value != NULL) {
			cwmp_main->conf.lw_notification_hostname = strdup(value);
			FREE(value);
		} else {
			cwmp_main->conf.lw_notification_hostname = strdup(cwmp_main->conf.acsurl ? cwmp_main->conf.acsurl : "");
		}
	} else {
		cwmp_main->conf.lw_notification_hostname = strdup(cwmp_main->conf.acsurl ? cwmp_main->conf.acsurl : "");
	}

	if (uci_get_value(LW_NOTIFICATION_PORT, &value) == CWMP_OK) {
		if (value != NULL) {
			int a = atoi(value);
			cwmp_main->conf.lw_notification_port = a;
			FREE(value);
		} else {
			cwmp_main->conf.lw_notification_port = DEFAULT_LWN_PORT;
		}
	} else {
		cwmp_main->conf.lw_notification_port = DEFAULT_LWN_PORT;
	}

	if (uci_get_value(UCI_CPE_SCHEDULE_REBOOT, &value) == CWMP_OK) {
		if (value != NULL) {
			cwmp_main->conf.schedule_reboot = convert_datetime_to_timestamp(value);
			FREE(value);
		} else {
			cwmp_main->conf.schedule_reboot = 0;
		}
	} else {
		cwmp_main->conf.schedule_reboot = 0;
	}

	if (uci_get_value(UCI_CPE_DELAY_REBOOT, &value) == CWMP_OK) {
		int delay = -1;

		if (value != NULL) {
			delay = atoi(value);
			FREE(value);
		}

		cwmp_main->conf.delay_reboot = delay;
	} else {
		cwmp_main->conf.delay_reboot = -1;
	}

	if (uci_get_value(UCI_CPE_ACTIVE_NOTIF_THROTTLE, &value) == CWMP_OK) {
		int a = 0;

		if (value != NULL) {
			a = atoi(value);
			FREE(value);
		}

		cwmp_main->conf.active_notif_throttle = a;
	} else {
		cwmp_main->conf.active_notif_throttle = 0;
	}

	if (uci_get_value(UCI_CPE_MANAGEABLE_DEVICES_NOTIF_LIMIT, &value) == CWMP_OK) {
		int a = 0;

		if (value != NULL) {
			a = atoi(value);
			FREE(value);
		}

		cwmp_main->conf.md_notif_limit = a;
	} else {
		cwmp_main->conf.md_notif_limit = 0;
	}

	cwmp_main->conf.custom_notify_json = NULL;
	if (uci_get_value(UCI_CPE_JSON_CUSTOM_NOTIFY_FILE, &value) == CWMP_OK) {
		FREE(cwmp_main->conf.custom_notify_json);
		if (value != NULL) {
			cwmp_main->conf.custom_notify_json = strdup(value);
			FREE(value);
		}
	}

	if (uci_get_value(UCI_ACS_HEARTBEAT_ENABLE, &value) == CWMP_OK) {
		cwmp_main->conf.heart_beat_enable = uci_str_to_bool(value);
		FREE(value);
	} else {
		cwmp_main->conf.heart_beat_enable = false;
	}

	if (uci_get_value(UCI_ACS_HEARTBEAT_INTERVAL, &value) == CWMP_OK) {
		int a = 30;

		if (value != NULL) {
			a = atoi(value);
			FREE(value);
		}
		cwmp_main->conf.heartbeat_interval = a;
	} else {
		cwmp_main->conf.heartbeat_interval = 30;
	}

	if (uci_get_value(UCI_ACS_HEARTBEAT_TIME, &value) == CWMP_OK) {
		if (value != NULL) {
			cwmp_main->conf.heart_time = convert_datetime_to_timestamp(value);
			FREE(value);
		} else {
			cwmp_main->conf.heart_time = 0;
		}
	} else {
		cwmp_main->conf.heart_time = 0;
	}


	if (uci_get_value(UCI_AUTONOMOUS_TC_ENABLE, &value) == CWMP_OK) {
		cwmp_main->conf.auto_tc_enable = uci_str_to_bool(value);
		FREE(value);
	} else {
		cwmp_main->conf.auto_tc_enable = 0;
	}
	if (uci_get_value(UCI_AUTONOMOUS_TC_TRANSFERTYPE, &value) == CWMP_OK) {
		cwmp_main->conf.auto_tc_transfer_type = strdup(value);
		FREE(value);
	} else {
		cwmp_main->conf.auto_tc_transfer_type = NULL;
	}
	if (uci_get_value(UCI_AUTONOMOUS_TC_RESULTTYPE, &value) == CWMP_OK) {
		cwmp_main->conf.auto_tc_result_type = strdup(value);
		FREE(value);
	} else {
		cwmp_main->conf.auto_tc_result_type = NULL;
	}
	if (uci_get_value(UCI_AUTONOMOUS_TC_FILETYPE, &value) == CWMP_OK) {
		cwmp_main->conf.auto_tc_file_type = strdup(value);
		FREE(value);
	} else {
		cwmp_main->conf.auto_tc_file_type = NULL;
	}

	if (uci_get_value(UCI_AUTONOMOUS_CDU_ENABLE, &value) == CWMP_OK) {
		cwmp_main->conf.auto_cdu_enable = uci_str_to_bool(value);
		FREE(value);
	} else {
		cwmp_main->conf.auto_cdu_enable = 0;
	}
	if (uci_get_value(UCI_AUTONOMOUS_CDU_OPTYPE, &value) == CWMP_OK) {
		cwmp_main->conf.auto_cdu_oprt_type = strdup(value);
		FREE(value);
	} else {
		cwmp_main->conf.auto_cdu_oprt_type = NULL;
	}
	if (uci_get_value(UCI_AUTONOMOUS_CDU_RESULTYPE, &value) == CWMP_OK) {
		cwmp_main->conf.auto_cdu_result_type = strdup(value);
		FREE(value);
	} else {
		cwmp_main->conf.auto_cdu_result_type = NULL;
	}
	if (uci_get_value(UCI_AUTONOMOUS_CDU_FAULTCODE, &value) == CWMP_OK) {
		cwmp_main->conf.auto_cdu_fault_code = strdup(value);
		FREE(value);
	} else {
		cwmp_main->conf.auto_cdu_fault_code = NULL;
	}
	return CWMP_OK;
}

int global_conf_init()
{
	int error = CWMP_OK;

	pthread_mutex_lock(&mutex_config_load);

	if ((error = get_global_config(&(cwmp_main->conf)))) {
		cwmp_main->init_complete = false;
		goto end;
	}

	cwmp_main->init_complete = true;
	/* Launch reboot methods if needed */
	launch_reboot_methods();

end:
	pthread_mutex_unlock(&mutex_config_load);

	return error;
}

void cwmp_config_load()
{
	int ret = CWMP_GEN_ERR;
	int error = CWMP_GEN_ERR;

	ret = global_conf_init();

	if (cwmp_stop == true)
		return;

	if (ret == CWMP_OK) {
		cwmp_main->net.ipv6_status = is_ipv6_enabled();
		error = icwmp_check_http_connection();
	}

	while (error != CWMP_OK && cwmp_stop != true) {
		if (ret != CWMP_OK) {
			CWMP_LOG(DEBUG, "Error reading uci ret = %d", ret);
		} else {
			CWMP_LOG(DEBUG, "Init: failed to check http connection");
		}

		sleep(UCI_OPTION_READ_INTERVAL);
		cwmp_uci_reinit();
		ret = global_conf_init();
		if (ret == CWMP_OK) {
			cwmp_main->net.ipv6_status = is_ipv6_enabled();
			error = icwmp_check_http_connection();
		}
	}
}

int cwmp_get_deviceid()
{
	struct cwmp_dm_parameter dm_param = {0};

	cwmp_get_parameter_value("Device.DeviceInfo.Manufacturer", &dm_param);
	cwmp_main->deviceid.manufacturer = strdup(dm_param.value ? dm_param.value : "");

	cwmp_get_parameter_value("Device.DeviceInfo.SerialNumber", &dm_param);
	cwmp_main->deviceid.serialnumber = strdup(dm_param.value ? dm_param.value : "");

	cwmp_get_parameter_value("Device.DeviceInfo.ProductClass", &dm_param);
	cwmp_main->deviceid.productclass = strdup(dm_param.value ? dm_param.value : "");

	cwmp_get_parameter_value("Device.DeviceInfo.ManufacturerOUI", &dm_param);
	cwmp_main->deviceid.oui = strdup(dm_param.value ? dm_param.value : "");

	cwmp_get_parameter_value("Device.DeviceInfo.SoftwareVersion", &dm_param);
	cwmp_main->deviceid.softwareversion = strdup(dm_param.value ? dm_param.value : "");

	return CWMP_OK;
}

int cwmp_config_reload()
{
	memset(&cwmp_main->env, 0, sizeof(struct env));
	int err = global_conf_init();
	if (err != CWMP_OK)
		return err;

	return CWMP_OK;
}
