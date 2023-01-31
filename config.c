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

static char* get_value_from_uci_option(struct uci_option *tb) {
	if (tb == NULL)
		return NULL;

	if (tb->type == UCI_TYPE_STRING) {
		return tb->v.string;
	}

	return NULL;
}

static void config_get_cpe_elements(struct config *conf, struct uci_section *s)
{
	char *val = NULL;

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
	};

	struct uci_option *cpe_tb[__MAX_NUM_UCI_CPE_ATTRS] = {0};
	uci_parse_section(s, cpe_opts, __MAX_NUM_UCI_CPE_ATTRS, cpe_tb);

	global_string_param_write(&conf->ubus_socket, get_value_from_uci_option(cpe_tb[UCI_CPE_UBUS_SOCKET_PATH]));
	global_string_param_read(&conf->ubus_socket, &val);
	CWMP_LOG(DEBUG, "CWMP CONFIG - ubus socket: %s", val);
	FREE(val);

	log_set_log_file_name(get_value_from_uci_option(cpe_tb[UCI_CPE_LOG_FILE_NAME]));

	log_set_file_max_size(get_value_from_uci_option(cpe_tb[UCI_CPE_LOG_MAX_SIZE]));

	log_set_on_console(get_value_from_uci_option(cpe_tb[UCI_CPE_ENABLE_STDOUT_LOG]));

	log_set_on_file(get_value_from_uci_option(cpe_tb[UCI_CPE_ENABLE_FILE_LOG]));

	log_set_severity_idx(get_value_from_uci_option(cpe_tb[UCI_LOG_SEVERITY_PATH]));

	log_set_on_syslog(get_value_from_uci_option(cpe_tb[UCI_CPE_ENABLE_SYSLOG]));

	global_int_param_write(&conf->amd_version, DEFAULT_AMD_VERSION);
	char *version = get_value_from_uci_option(cpe_tb[UCI_CPE_AMD_VERSION]);
	if (version != NULL) {
		int a = atoi(version);
		if (a >= 1 && a <= 6) {
			global_int_param_write(&conf->amd_version, a);
		}
	}

	int amd_ver = global_int_param_read(&conf->amd_version);
	global_int_param_write(&conf->supported_amd_version, amd_ver);
	CWMP_LOG(DEBUG, "CWMP CONFIG - amendement version: %d", amd_ver);
	if (cpe_tb[UCI_CPE_DEFAULT_WAN_IFACE]) {
		char *default_wan_iface = get_value_from_uci_option(cpe_tb[UCI_CPE_DEFAULT_WAN_IFACE]);
		global_string_param_write(&conf->default_wan_iface, default_wan_iface ? default_wan_iface : "wan");
	} else {
		global_string_param_write(&conf->default_wan_iface, "wan");
	}

	global_string_param_read(&conf->default_wan_iface, &val);
	CWMP_LOG(DEBUG, "CWMP CONFIG - default wan interface: %s", val);
	FREE(val);
}

static void config_get_acs_elements(struct config *conf, struct uci_section *s)
{
	char *val = NULL;

	enum {
		UCI_ACS_IPV6_ENABLE,
		UCI_ACS_SSL_CAPATH,
		HTTP_DISABLE_100CONTINUE,
		UCI_ACS_INSECURE_ENABLE,
		__MAX_NUM_UCI_ACS_ATTRS,
	};

	const struct uci_parse_option acs_opts[] = {
		{ .name = "ipv6_enable", .type = UCI_TYPE_STRING },
		{ .name = "ssl_capath", .type = UCI_TYPE_STRING },
		{ .name = "http_disable_100continue", .type = UCI_TYPE_STRING },
		{ .name = "insecure_enable", .type = UCI_TYPE_STRING },
	};

	struct uci_option *acs_tb[__MAX_NUM_UCI_ACS_ATTRS];
	memset(acs_tb, 0, sizeof(acs_tb));
	uci_parse_section(s, acs_opts, __MAX_NUM_UCI_ACS_ATTRS, acs_tb);

	global_bool_param_write(&conf->ipv6_enable, uci_str_to_bool(get_value_from_uci_option(acs_tb[UCI_ACS_IPV6_ENABLE])));
	bool v6_enable = global_bool_param_read(&conf->ipv6_enable);
	CWMP_LOG(DEBUG, "CWMP CONFIG - ipv6 enable: %d", v6_enable);

	global_string_param_write(&conf->acs_ssl_capath, get_value_from_uci_option(acs_tb[UCI_ACS_SSL_CAPATH]));
	global_string_param_read(&conf->acs_ssl_capath, &val);
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs ssl cpath: %s", val);
	FREE(val);

	global_bool_param_write(&conf->http_disable_100continue, uci_str_to_bool(get_value_from_uci_option(acs_tb[HTTP_DISABLE_100CONTINUE])));
	bool http_100_enable = global_bool_param_read(&conf->http_disable_100continue);
	CWMP_LOG(DEBUG, "CWMP CONFIG - http disable 100continue: %d", http_100_enable);

	global_bool_param_write(&conf->insecure_enable, uci_str_to_bool(get_value_from_uci_option(acs_tb[UCI_ACS_INSECURE_ENABLE])));
	bool insecure_en = global_bool_param_read(&conf->insecure_enable);
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs insecure enable: %d", insecure_en);
}

int get_preinit_config(struct config *conf)
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
		if (s == NULL || s->type == NULL)
			continue;
		if (strcmp(s->type, "acs") == 0) {
			config_get_acs_elements(conf, s);
		} else if (strcmp(s->type, "cpe") == 0) {
			config_get_cpe_elements(conf, s);
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

int get_global_config(struct config *conf)
{
	int error;
	char *value = NULL, *value2 = NULL, *value3 = NULL, *temp = NULL;

	if ((error = uci_get_value(UCI_CPE_CWMP_ENABLE, &value)) == CWMP_OK) {
		if (value != NULL && uci_str_to_bool(value) == false) {
			FREE(value);
			CWMP_LOG(ERROR, "CWMP service is disabled");
			exit(0);
		}
	}
	FREE(value);

	global_string_param_read(&conf->default_wan_iface, &temp);
	error = get_connection_interface(temp);
	if (error != CWMP_OK) {
		CWMP_LOG(DEBUG, "Failed to get interface [%s] details", temp);
		FREE(temp);
		return error;
	}
	FREE(temp);

	bool discovery_enable = false;
	error = uci_get_value(UCI_DHCP_DISCOVERY_PATH, &value);
	if (error == CWMP_OK && value != NULL) {
		discovery_enable = uci_str_to_bool(value);
	}
	FREE(value);

	uci_get_value(UCI_ACS_URL_PATH, &value2);
	uci_get_value(UCI_DHCP_ACS_URL, &value3);

	global_string_param_free(&conf->acsurl);
	char *url = get_alternate_option_value(discovery_enable, value2, value3);
	global_string_param_write(&conf->acsurl, url);

	FREE(value2);
	FREE(value3);

	global_string_param_read(&conf->acsurl, &temp);
	if (CWMP_STRLEN(url) == 0) {
		FREE(temp);
		return CWMP_GEN_ERR;
	}
	FREE(temp);

	if ((error = uci_get_value(UCI_ACS_GETRPC, &value)) == CWMP_OK) {
		global_bool_param_write(&conf->acs_getrpc, uci_str_to_bool(value));
		FREE(value);
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_ACS_USERID_PATH, &value)) == CWMP_OK) {
		if (value != NULL) {
			global_string_param_free(&conf->acs_userid);
			global_string_param_write(&conf->acs_userid, value);
			FREE(value);
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_ACS_PASSWD_PATH, &value)) == CWMP_OK) {
		if (value != NULL) {
			global_string_param_free(&conf->acs_passwd);
			global_string_param_write(&conf->acs_passwd, value);
			FREE(value);
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_ACS_COMPRESSION, &value)) == CWMP_OK) {
		global_int_param_write(&conf->compression, COMP_NONE);
		if (global_int_param_read(&conf->amd_version) >= AMD_5 && value != NULL) {
			if (0 == strcasecmp(value, "gzip")) {
				global_int_param_write(&conf->compression, COMP_GZIP);
			} else if (0 == strcasecmp(value, "deflate")) {
				global_int_param_write(&conf->compression, COMP_DEFLATE);
			} else {
				global_int_param_write(&conf->compression, COMP_NONE);
			}
		}
		FREE(value);
	} else {
		global_int_param_write(&conf->compression, COMP_NONE);
	}

	global_int_param_write(&conf->retry_min_wait_interval, DEFAULT_RETRY_MINIMUM_WAIT_INTERVAL);
	uci_get_value(UCI_ACS_RETRY_MIN_WAIT_INTERVAL, &value2);
	uci_get_value(UCI_DHCP_ACS_RETRY_MIN_WAIT_INTERVAL, &value3);

	char *op_interval = get_alternate_option_value(discovery_enable, value2, value3);
	if (op_interval != NULL) {
		if (global_int_param_read(&conf->amd_version) >= AMD_3) {
			int a = atoi(op_interval);
			if (a <= 65535 && a >= 1) {
				global_int_param_write(&conf->retry_min_wait_interval, a);
			}
		}
	}

	FREE(value2);
	FREE(value3);

	global_int_param_write(&conf->retry_interval_multiplier, DEFAULT_RETRY_INTERVAL_MULTIPLIER);
	uci_get_value(UCI_ACS_RETRY_INTERVAL_MULTIPLIER, &value2);
	uci_get_value(UCI_DHCP_ACS_RETRY_INTERVAL_MULTIPLIER, &value3);

	char *op_multi = get_alternate_option_value(discovery_enable, value2, value3);
	if (op_multi != NULL) {
		if (global_int_param_read(&conf->amd_version) >= AMD_3) {
			int a = atoi(op_multi);
			if (a <= 65535 && a >= 1000) {
				global_int_param_write(&conf->retry_interval_multiplier, a);
			}
		}
	}

	FREE(value2);
	FREE(value3);

	if ((error = uci_get_value(UCI_CPE_USERID_PATH, &value)) == CWMP_OK) {
		global_string_param_free(&conf->cpe_userid);
		if (value != NULL) {
			global_string_param_write(&conf->cpe_userid, value);
			FREE(value);
		} else {
			global_string_param_write(&conf->cpe_userid, "");
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_CPE_PASSWD_PATH, &value)) == CWMP_OK) {
		global_string_param_free(&conf->cpe_passwd);
		if (value != NULL) {
			global_string_param_write(&conf->cpe_passwd, value);
			FREE(value);
		} else {
			global_string_param_write(&conf->cpe_passwd, "");
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_CPE_PORT_PATH, &value)) == CWMP_OK) {
		int a = 0;

		if (value != NULL) {
			a = atoi(value);
			FREE(value);
		}

		if (a == 0) {
			CWMP_LOG(INFO, "Set the connection request port to the default value: %d", DEFAULT_CONNECTION_REQUEST_PORT);
			global_int_param_write(&conf->connection_request_port, DEFAULT_CONNECTION_REQUEST_PORT);
		} else {
			global_int_param_write(&conf->connection_request_port, a);
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_CPE_CRPATH_PATH, &value)) == CWMP_OK) {
		global_string_param_free(&conf->connection_request_path);
		if (value == NULL)
			global_string_param_write(&conf->connection_request_path, "/");
		else {
			if (value[0] == '/')
				global_string_param_write(&conf->connection_request_path, value);
			else {
				char cr_path[512];
				snprintf(cr_path, sizeof(cr_path), "/%s", value);
				global_string_param_write(&conf->connection_request_path, cr_path);
			}
			FREE(value);
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_CPE_NOTIFY_PERIODIC_ENABLE, &value)) == CWMP_OK) {
		bool a = true;
		if (value != NULL) {
			a = uci_str_to_bool(value);
			FREE(value);
		}
		global_bool_param_write(&conf->periodic_notify_enable, a);
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_CPE_NOTIFY_PERIOD, &value)) == CWMP_OK) {
		int a = 0;

		if (value != NULL) {
			a = atoi(value);
			FREE(value);
		}

		if (a == 0) {
			CWMP_LOG(INFO, "Set notify period to the default value: %d", DEFAULT_NOTIFY_PERIOD);
			global_int_param_write(&conf->periodic_notify_interval, DEFAULT_NOTIFY_PERIOD);
		} else {
			global_int_param_write(&conf->periodic_notify_interval, a);
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_PERIODIC_INFORM_TIME_PATH, &value)) == CWMP_OK) {
		if (value != NULL) {
			global_time_param_write(&conf->time, convert_datetime_to_timestamp(value));
			FREE(value);
		} else {
			global_time_param_write(&conf->time, 0);
		}
	} else {
		return error;
	}

	char *entropy = generate_random_string(sizeof(unsigned int));
	if (entropy != NULL) {
		global_uint_param_write(&conf->periodic_entropy, (unsigned int)strtoul(entropy, NULL, 16));
		free(entropy);
	}

	if ((error = uci_get_value(UCI_PERIODIC_INFORM_INTERVAL_PATH, &value)) == CWMP_OK) {
		int a = 0;

		if (value != NULL) {
			a = atoi(value);
			FREE(value);
		}

		if (a >= PERIOD_INFORM_MIN) {
			global_int_param_write(&conf->period, a);
		} else {
			CWMP_LOG(ERROR, "Period interval of periodic inform should be > %ds. Set to default: %ds", PERIOD_INFORM_MIN, PERIOD_INFORM_DEFAULT);
			global_int_param_write(&conf->period, PERIOD_INFORM_DEFAULT);
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_PERIODIC_INFORM_ENABLE_PATH, &value)) == CWMP_OK) {
		global_bool_param_write(&conf->periodic_enable, uci_str_to_bool(value));
		FREE(value);
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_CPE_INSTANCE_MODE, &value)) == CWMP_OK) {
		if (value != NULL) {
			if (0 == strcmp(value, "InstanceNumber")) {
				global_uint_param_write(&conf->instance_mode, INSTANCE_MODE_NUMBER);
			} else {
				global_uint_param_write(&conf->instance_mode, INSTANCE_MODE_ALIAS);
			}
			FREE(value);
		} else {
			global_uint_param_write(&conf->instance_mode, DEFAULT_INSTANCE_MODE);
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_CPE_SESSION_TIMEOUT, &value)) == CWMP_OK) {
		global_uint_param_write(&conf->session_timeout, DEFAULT_SESSION_TIMEOUT);
		if (value != NULL) {
			int a = atoi(value);
			if (a >= 1) {
				global_uint_param_write(&conf->session_timeout, a);
			}
			FREE(value);
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(LW_NOTIFICATION_ENABLE, &value)) == CWMP_OK) {
		global_bool_param_write(&conf->lw_notification_enable, uci_str_to_bool(value));
		FREE(value);
	} else {
		return error;
	}

	if ((error = uci_get_value(LW_NOTIFICATION_HOSTNAME, &value)) == CWMP_OK) {
		global_string_param_free(&conf->lw_notification_hostname);
		if (value != NULL) {
			global_string_param_write(&conf->lw_notification_hostname, value);
			FREE(value);
		} else {
			global_string_param_read(&conf->acsurl, &temp);
			global_string_param_write(&conf->lw_notification_hostname, temp);
			FREE(temp);
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(LW_NOTIFICATION_PORT, &value)) == CWMP_OK) {
		if (value != NULL) {
			int a = atoi(value);
			global_int_param_write(&conf->lw_notification_port, a);
			FREE(value);
		} else {
			global_int_param_write(&conf->lw_notification_port, DEFAULT_LWN_PORT);
		}
	} else {
		return error;
	}

	if (uci_get_value(UCI_CPE_SCHEDULE_REBOOT, &value) == CWMP_OK) {
		if (value != NULL) {
			global_time_param_write(&conf->schedule_reboot, convert_datetime_to_timestamp(value));
			FREE(value);
		} else {
			global_time_param_write(&conf->schedule_reboot, 0);
		}
	} else {
		return error;
	}

	if (uci_get_value(UCI_CPE_DELAY_REBOOT, &value) == CWMP_OK) {
		int delay = -1;

		if (value != NULL) {
			delay = atoi(value);
			FREE(value);
		}

		global_int_param_write(&conf->delay_reboot, delay);
	} else {
		return error;
	}

	if (uci_get_value(UCI_CPE_FORCED_INFORM_JSON, &value) == CWMP_OK) {
		global_string_param_free(&conf->forced_inform_json_file);
		if (value != NULL) {
			global_string_param_write(&conf->forced_inform_json_file, value);
			FREE(value);
		} else {
			global_string_param_write(&conf->forced_inform_json_file, NULL);
		}
	}
	if (uci_get_value(UCI_CPE_BOOT_INFORM_JSON, &value) == CWMP_OK) {
		global_string_param_free(&conf->boot_inform_json_file);
		if (value != NULL) {
			global_string_param_write(&conf->boot_inform_json_file, value);
			FREE(value);
		} else {
			global_string_param_write(&conf->boot_inform_json_file, NULL);
		}
	}
	if (uci_get_value(UCI_CPE_JSON_CUSTOM_NOTIFY_FILE, &value) == CWMP_OK) {
		global_string_param_free(&conf->custom_notify_json);
		if (value != NULL) {
			global_string_param_write(&conf->custom_notify_json, value);
			FREE(value);
		} else {
			global_string_param_write(&conf->custom_notify_json, NULL);
		}
	}

	if ((error = uci_get_value(UCI_ACS_HEARTBEAT_ENABLE, &value)) == CWMP_OK) {
		global_bool_param_write(&conf->heart_beat_enable, uci_str_to_bool(value));
		FREE(value);
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_ACS_HEARTBEAT_INTERVAL, &value)) == CWMP_OK) {
		int a = 30;

		if (value != NULL) {
			a = atoi(value);
			FREE(value);
		}
		global_int_param_write(&conf->heartbeat_interval, a);
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_ACS_HEARTBEAT_TIME, &value)) == CWMP_OK) {
		if (value != NULL) {
			global_time_param_write(&conf->heart_time, convert_datetime_to_timestamp(value));
			FREE(value);
		} else {
			global_time_param_write(&conf->heart_time, 0);
		}
	} else {
		return error;
	}
	return CWMP_OK;
}

int global_conf_init(struct cwmp *cwmp)
{
	int error = CWMP_OK;

	if ((error = get_global_config(&(cwmp->conf)))) {
		cwmp->init_complete = false;
		goto end;
	}

	cwmp->init_complete = true;
	/* Launch reboot methods if needed */
	launch_reboot_methods(cwmp);

end:
	return error;
}

void cwmp_config_load(struct cwmp *cwmp)
{
	int ret;

	ret = global_conf_init(cwmp);
	while (ret != CWMP_OK && thread_end != true) {
		CWMP_LOG(DEBUG, "Error reading uci ret = %d", ret);
		sleep(UCI_OPTION_READ_INTERVAL);
		ret = global_conf_init(cwmp);
	}
}

int cwmp_get_deviceid(struct cwmp *cwmp)
{
	cwmp_get_leaf_value("Device.DeviceInfo.Manufacturer", &cwmp->deviceid.manufacturer);
	cwmp_get_leaf_value("Device.DeviceInfo.SerialNumber", &cwmp->deviceid.serialnumber);
	cwmp_get_leaf_value("Device.DeviceInfo.ProductClass", &cwmp->deviceid.productclass);
	cwmp_get_leaf_value("Device.DeviceInfo.ManufacturerOUI", &cwmp->deviceid.oui);
	cwmp_get_leaf_value("Device.DeviceInfo.SoftwareVersion", &cwmp->deviceid.softwareversion);
	return CWMP_OK;
}

int cwmp_config_reload(struct cwmp *cwmp)
{
	memset(&cwmp->env, 0, sizeof(struct env));
	int err = global_conf_init(cwmp);
	if (err != CWMP_OK)
		return err;

	return CWMP_OK;
}
