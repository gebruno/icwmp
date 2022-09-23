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

pthread_mutex_t mutex_config_load = PTHREAD_MUTEX_INITIALIZER;

void get_dhcp_vend_info_cb(struct ubus_request *req, int type __attribute__((unused)), struct blob_attr *msg)
{
	if (req == NULL || msg == NULL)
		return;

	char **v_info = (char **)req->priv;
	if (v_info == NULL)
		return;

	struct blob_attr *param;
	size_t rem;

	enum {
		E_VENDOR_INFO,
		__E_MAX
	};

	const struct blobmsg_policy p[__E_MAX] = {
		{ "vendorspecinf", BLOBMSG_TYPE_STRING },
	};

	blobmsg_for_each_attr(param, msg, rem) {
		if (strcmp(blobmsg_name(param), "data") == 0) {
			struct blob_attr *tb[__E_MAX] = {NULL};
			if (blobmsg_parse(p, __E_MAX, tb, blobmsg_data(param), blobmsg_len(param)) != 0) {
				return;
			}

			if (tb[E_VENDOR_INFO]) {
				char *info = blobmsg_get_string(tb[E_VENDOR_INFO]);
				if (info == NULL)
					info = "";
				int len = strlen(info) + 1;
				*v_info = (char *)malloc(len);
				if (*v_info == NULL)
					return;

				memset(*v_info, 0, len);
				snprintf(*v_info, len, "%s", info);
			}

			break;
		}
	}

	return;
}

bool configure_dhcp_options(char *vendspecinf)
{
	if (vendspecinf == NULL) {
		CWMP_LOG(DEBUG, "No vendor specific info found");
		return false;
	}

	// extract url from vendor info
	int len = CWMP_STRLEN(vendspecinf) + 1;
	char vend_info[len];
	memset(vend_info, 0, len);
	snprintf(vend_info, len, "%s", vendspecinf);

	if (strncmp(vend_info, "http://", 7) == 0 || strncmp(vend_info, "https://", 8) == 0) {
		uci_set_value_by_path(UCI_DHCP_ACS_URL, vend_info, UCI_STANDARD_CONFIG);
		CWMP_LOG(DEBUG, "dhcp url: %s", vend_info);
		cwmp_commit_package("cwmp", UCI_STANDARD_CONFIG);
		return true;

	}

	bool update_uci = false;
	char *temp = strtok(vend_info, " ");
	while (temp) {
		if (strncmp(temp, "1=", 2) == 0) {
			char *pos = temp + 2;
			if (CWMP_STRLEN(pos)) {
				uci_set_value_by_path(UCI_DHCP_ACS_URL, pos, UCI_STANDARD_CONFIG);
				CWMP_LOG(DEBUG, "dhcp url: %s", pos);
				update_uci = true;
			}
		}

		if (strncmp(temp, "2=", 2) == 0) {
			char *pos = temp + 2;
			if (CWMP_STRLEN(pos)) {
				uci_set_value_by_path(UCI_DHCP_CPE_PROV_CODE, pos, UCI_STANDARD_CONFIG);
				update_uci = true;
			}
		}

		if (strncmp(temp, "3=", 2) == 0) {
			char *pos = temp + 2;
			if (CWMP_STRLEN(pos)) {
				uci_set_value_by_path(UCI_DHCP_ACS_RETRY_MIN_WAIT_INTERVAL, pos, UCI_STANDARD_CONFIG);
				update_uci = true;
			}
		}

		if (strncmp(temp, "4=", 2) == 0) {
			char *pos = temp + 2;
			if (CWMP_STRLEN(pos)) {
				uci_set_value_by_path(UCI_DHCP_ACS_RETRY_INTERVAL_MULTIPLIER, pos, UCI_STANDARD_CONFIG);
				update_uci = true;
			}
		}

		temp = strtok(NULL, " ");
	}

	if (update_uci)
		cwmp_commit_package("cwmp", UCI_STANDARD_CONFIG);

	cwmp_uci_reinit();
	return update_uci;
}

static void get_dhcp_vendor_info(char *intf)
{
	if (intf == NULL)
		return;

	char ubus_obj[100] = {0};
	snprintf(ubus_obj, sizeof(ubus_obj), "network.interface.%s", intf);

	struct blob_buf b;
	memset(&b, 0, sizeof(struct blob_buf));
	blob_buf_init(&b, 0);

	char *vendor_info = NULL;
	if (icwmp_ubus_invoke(ubus_obj, "status", b.head, get_dhcp_vend_info_cb, &vendor_info) == 0) {
		CWMP_LOG(DEBUG, "vendor info: %s", vendor_info);
		configure_dhcp_options(vendor_info);
	}

	FREE(vendor_info);

	blob_buf_free(&b);
}

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

	conf->ubus_socket = CWMP_STRDUP(get_value_from_uci_option(cpe_tb[UCI_CPE_UBUS_SOCKET_PATH]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - ubus socket: %s", conf->ubus_socket ? conf->ubus_socket : "");

	log_set_log_file_name(get_value_from_uci_option(cpe_tb[UCI_CPE_LOG_FILE_NAME]));

	log_set_file_max_size(get_value_from_uci_option(cpe_tb[UCI_CPE_LOG_MAX_SIZE]));

	log_set_on_console(get_value_from_uci_option(cpe_tb[UCI_CPE_ENABLE_STDOUT_LOG]));

	log_set_on_file(get_value_from_uci_option(cpe_tb[UCI_CPE_ENABLE_FILE_LOG]));

	log_set_severity_idx(get_value_from_uci_option(cpe_tb[UCI_LOG_SEVERITY_PATH]));

	log_set_on_syslog(get_value_from_uci_option(cpe_tb[UCI_CPE_ENABLE_SYSLOG]));

	conf->amd_version = DEFAULT_AMD_VERSION;
	char *version = get_value_from_uci_option(cpe_tb[UCI_CPE_AMD_VERSION]);
	if (version != NULL) {
		int a = atoi(version);
		if (a >= 1 && a <= 6) {
			conf->amd_version = a;
		}
	}
	conf->supported_amd_version = conf->amd_version;
	CWMP_LOG(DEBUG, "CWMP CONFIG - amendement version: %d", conf->amd_version);
	if (cpe_tb[UCI_CPE_DEFAULT_WAN_IFACE]) {
		char *default_wan_iface = get_value_from_uci_option(cpe_tb[UCI_CPE_DEFAULT_WAN_IFACE]);
		conf->default_wan_iface = strdup(default_wan_iface ? default_wan_iface : "wan");
	} else {
		conf->default_wan_iface = strdup("wan");
	}
	CWMP_LOG(DEBUG, "CWMP CONFIG - default wan interface: %s", conf->default_wan_iface);
}

static void config_get_acs_elements(struct config *conf, struct uci_section *s)
{
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

	conf->ipv6_enable = uci_str_to_bool(get_value_from_uci_option(acs_tb[UCI_ACS_IPV6_ENABLE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - ipv6 enable: %d", conf->ipv6_enable);

	conf->acs_ssl_capath = CWMP_STRDUP(get_value_from_uci_option(acs_tb[UCI_ACS_SSL_CAPATH]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs ssl cpath: %s", conf->acs_ssl_capath ? conf->acs_ssl_capath : "");

	conf->http_disable_100continue = uci_str_to_bool(get_value_from_uci_option(acs_tb[HTTP_DISABLE_100CONTINUE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - http disable 100continue: %d", conf->http_disable_100continue);

	conf->insecure_enable = uci_str_to_bool(get_value_from_uci_option(acs_tb[UCI_ACS_INSECURE_ENABLE]));
	CWMP_LOG(DEBUG, "CWMP CONFIG - acs insecure enable: %d", conf->insecure_enable);
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
	char *value = NULL, *value2 = NULL, *value3 = NULL;

	if ((error = uci_get_value(UCI_CPE_CWMP_ENABLE, &value)) == CWMP_OK) {
		if (value != NULL && uci_str_to_bool(value) == false) {
			FREE(value);
			CWMP_LOG(ERROR, "CWMP service is disabled");
			exit(0);
		}
	}
	FREE(value);

	error = get_connection_interface(conf->default_wan_iface);
	if (error != CWMP_OK) {
		CWMP_LOG(DEBUG, "Failed to get interface [%s] details", conf->default_wan_iface);
		return error;
	}

	bool discovery_enable = false;
	error = uci_get_value(UCI_DHCP_DISCOVERY_PATH, &value);

	// now read the vendor info from ifstatus before reading the DHCP_ACS_URL from uci
	if (error == CWMP_OK && value != NULL) {
		discovery_enable = uci_str_to_bool(value);
		if (discovery_enable == true && conf->default_wan_iface != NULL) {
			get_dhcp_vendor_info(conf->default_wan_iface);
		}
	}
	FREE(value);

	uci_get_value(UCI_ACS_URL_PATH, &value2);
	uci_get_value(UCI_DHCP_ACS_URL, &value3);

	FREE(conf->acsurl);
	conf->acsurl = CWMP_STRDUP(get_alternate_option_value(discovery_enable, value2, value3));

	FREE(value2);
	FREE(value3);

	if (conf->acsurl == NULL) {
		return CWMP_GEN_ERR;
	}

	if ((error = uci_get_value(UCI_ACS_USERID_PATH, &value)) == CWMP_OK) {
		if (value != NULL) {
			FREE(conf->acs_userid);
			conf->acs_userid = strdup(value);
			FREE(value);
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_ACS_PASSWD_PATH, &value)) == CWMP_OK) {
		if (value != NULL) {
			FREE(conf->acs_passwd);
			conf->acs_passwd = strdup(value);
			FREE(value);
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_ACS_COMPRESSION, &value)) == CWMP_OK) {
		conf->compression = COMP_NONE;
		if (conf->amd_version >= AMD_5 && value != NULL) {
			if (0 == strcasecmp(value, "gzip")) {
				conf->compression = COMP_GZIP;
			} else if (0 == strcasecmp(value, "deflate")) {
				conf->compression = COMP_DEFLATE;
			} else {
				conf->compression = COMP_NONE;
			}
		}
		FREE(value);
	} else {
		conf->compression = COMP_NONE;
	}

	conf->retry_min_wait_interval = DEFAULT_RETRY_MINIMUM_WAIT_INTERVAL;
	uci_get_value(UCI_ACS_RETRY_MIN_WAIT_INTERVAL, &value2);
	uci_get_value(UCI_DHCP_ACS_RETRY_MIN_WAIT_INTERVAL, &value3);

	char *op_interval = get_alternate_option_value(discovery_enable, value2, value3);
	if (op_interval != NULL) {
		if (conf->amd_version >= AMD_3) {
			int a = atoi(op_interval);
			if (a <= 65535 && a >= 1) {
				conf->retry_min_wait_interval = a;
			}
		}
	}

	FREE(value2);
	FREE(value3);

	conf->retry_interval_multiplier = DEFAULT_RETRY_INTERVAL_MULTIPLIER;
	uci_get_value(UCI_ACS_RETRY_INTERVAL_MULTIPLIER, &value2);
	uci_get_value(UCI_DHCP_ACS_RETRY_INTERVAL_MULTIPLIER, &value3);

	char *op_multi = get_alternate_option_value(discovery_enable, value2, value3);
	if (op_multi != NULL) {
		if (conf->amd_version >= AMD_3) {
			int a = atoi(op_multi);
			if (a <= 65535 && a >= 1000) {
				conf->retry_interval_multiplier = a;
			}
		}
	}

	FREE(value2);
	FREE(value3);

	if ((error = uci_get_value(UCI_CPE_USERID_PATH, &value)) == CWMP_OK) {
		FREE(conf->cpe_userid);
		if (value != NULL) {
			conf->cpe_userid = strdup(value);
			FREE(value);
		} else {
			conf->cpe_userid = strdup("");
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_CPE_PASSWD_PATH, &value)) == CWMP_OK) {
		FREE(conf->cpe_passwd);
		if (value != NULL) {
			conf->cpe_passwd = strdup(value);
			FREE(value);
		} else {
			conf->cpe_passwd = strdup("");
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
			conf->connection_request_port = DEFAULT_CONNECTION_REQUEST_PORT;
		} else {
			conf->connection_request_port = a;
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_CPE_CRPATH_PATH, &value)) == CWMP_OK) {
		FREE(conf->connection_request_path);
		if (value == NULL)
			conf->connection_request_path = strdup("/");
		else {
			if (value[0] == '/')
				conf->connection_request_path = strdup(value);
			else {
				char cr_path[512];
				snprintf(cr_path, sizeof(cr_path), "/%s", value);
				conf->connection_request_path = strdup(cr_path);
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
		conf->periodic_notify_enable = a;
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
			conf->periodic_notify_interval = DEFAULT_NOTIFY_PERIOD;
		} else {
			conf->periodic_notify_interval = a;
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_PERIODIC_INFORM_TIME_PATH, &value)) == CWMP_OK) {
		if (value != NULL) {
			conf->time = convert_datetime_to_timestamp(value);
			FREE(value);
		} else {
			conf->time = 0;
		}
	} else {
		return error;
	}

	char *entropy = generate_random_string(sizeof(unsigned int));
	if (entropy != NULL) {
		conf->periodic_entropy = (unsigned int)strtoul(entropy, NULL, 16);
		free(entropy);
	}

	if ((error = uci_get_value(UCI_PERIODIC_INFORM_INTERVAL_PATH, &value)) == CWMP_OK) {
		int a = 0;

		if (value != NULL) {
			a = atoi(value);
			FREE(value);
		}

		if (a >= PERIOD_INFORM_MIN) {
			conf->period = a;
		} else {
			CWMP_LOG(ERROR, "Period interval of periodic inform should be > %ds. Set to default: %ds", PERIOD_INFORM_MIN, PERIOD_INFORM_DEFAULT);
			conf->period = PERIOD_INFORM_DEFAULT;
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_PERIODIC_INFORM_ENABLE_PATH, &value)) == CWMP_OK) {
		conf->periodic_enable = uci_str_to_bool(value);
		FREE(value);
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_CPE_INSTANCE_MODE, &value)) == CWMP_OK) {
		if (value != NULL) {
			if (0 == strcmp(value, "InstanceNumber")) {
				conf->instance_mode = INSTANCE_MODE_NUMBER;
			} else {
				conf->instance_mode = INSTANCE_MODE_ALIAS;
			}
			FREE(value);
		} else {
			conf->instance_mode = DEFAULT_INSTANCE_MODE;
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_CPE_SESSION_TIMEOUT, &value)) == CWMP_OK) {
		conf->session_timeout = DEFAULT_SESSION_TIMEOUT;
		if (value != NULL) {
			int a = atoi(value);
			if (a >= 1) {
				conf->session_timeout = a;
			}
			FREE(value);
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(LW_NOTIFICATION_ENABLE, &value)) == CWMP_OK) {
		conf->lw_notification_enable = uci_str_to_bool(value);
		FREE(value);
	} else {
		return error;
	}

	if ((error = uci_get_value(LW_NOTIFICATION_HOSTNAME, &value)) == CWMP_OK) {
		FREE(conf->lw_notification_hostname);
		if (value != NULL) {
			conf->lw_notification_hostname = strdup(value);
			FREE(value);
		} else {
			conf->lw_notification_hostname = strdup(conf->acsurl ? conf->acsurl : "");
		}
	} else {
		return error;
	}

	if ((error = uci_get_value(LW_NOTIFICATION_PORT, &value)) == CWMP_OK) {
		if (value != NULL) {
			int a = atoi(value);
			conf->lw_notification_port = a;
			FREE(value);
		} else {
			conf->lw_notification_port = DEFAULT_LWN_PORT;
		}
	} else {
		return error;
	}

	if (uci_get_value(UCI_CPE_SCHEDULE_REBOOT, &value) == CWMP_OK) {
		if (value != NULL) {
			conf->schedule_reboot = convert_datetime_to_timestamp(value);
			FREE(value);
		} else {
			conf->schedule_reboot = 0;
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

		conf->delay_reboot = delay;
	} else {
		return error;
	}

	if (uci_get_value(UCI_CPE_FORCED_INFORM_JSON, &value) == CWMP_OK) {
		FREE(conf->forced_inform_json_file);
		if (value != NULL) {
			conf->forced_inform_json_file = strdup(value);
			FREE(value);
		} else {
			conf->forced_inform_json_file = NULL;
		}
	}
	if (uci_get_value(UCI_CPE_BOOT_INFORM_JSON, &value) == CWMP_OK) {
		FREE(conf->boot_inform_json_file);
		if (value != NULL) {
			conf->boot_inform_json_file = strdup(value);
			FREE(value);
		} else {
			conf->boot_inform_json_file = NULL;
		}
	}
	if (uci_get_value(UCI_CPE_JSON_CUSTOM_NOTIFY_FILE, &value) == CWMP_OK) {
		FREE(conf->custom_notify_json);
		if (value != NULL) {
			conf->custom_notify_json = strdup(value);
			FREE(value);
		} else {
			conf->custom_notify_json = NULL;
		}
	}

	if ((error = uci_get_value(UCI_ACS_HEARTBEAT_ENABLE, &value)) == CWMP_OK) {
		conf->heart_beat_enable = uci_str_to_bool(value);
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
		conf->heartbeat_interval = a;
	} else {
		return error;
	}

	if ((error = uci_get_value(UCI_ACS_HEARTBEAT_TIME, &value)) == CWMP_OK) {
		if (value != NULL) {
			conf->heart_time = convert_datetime_to_timestamp(value);
			FREE(value);
		} else {
			conf->heart_time = 0;
		}
	} else {
		return error;
	}
	return CWMP_OK;
}

int global_conf_init(struct cwmp *cwmp)
{
	int error = CWMP_OK;

	pthread_mutex_lock(&mutex_config_load);

	if ((error = get_global_config(&(cwmp->conf)))) {
		cwmp->init_complete = false;
		goto end;
	}

	cwmp->init_complete = true;
	/* Launch reboot methods if needed */
	launch_reboot_methods(cwmp);

end:
	pthread_mutex_unlock(&mutex_config_load);

	return error;
}

void cwmp_config_load(struct cwmp *cwmp)
{
	int ret;

	cwmp_uci_reinit();
	ret = global_conf_init(cwmp);
	while (ret != CWMP_OK && thread_end != true) {
		CWMP_LOG(DEBUG, "Error reading uci ret = %d", ret);
		sleep(UCI_OPTION_READ_INTERVAL);
		cwmp_uci_reinit();
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
